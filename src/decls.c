/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decls.c -- Scanning of declarations.

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
#include "statements.h"
#include "trans_corresp.h"
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */

/*
Macro to test whether the current token is a Microsoft storage class
specifier.  Includes an "||" at the beginning.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_microsoft_storage_class() ||			      \
  (curr_token == tok_declspec ||				      \
   curr_token == tok_microsoft_inline ||			      \
   curr_token == tok_forceinline)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_microsoft_storage_class() /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


/*
Macro that is TRUE if the current token is the start of a storage class
specifier (3.5.1).
*/
#define is_storage_class()                                            \
  (curr_token == tok_typedef  || curr_token == tok_extern   ||        \
   curr_token == tok_static   || curr_token == tok_auto     ||        \
   curr_token == tok_register || curr_token == tok_mutable            \
   or_is_microsoft_storage_class())

/*
Macro that is TRUE if the current token is the start of a function
specifier.
*/
#define is_function_specifier()                                      \
  (curr_token == tok_inline   || curr_token == tok_virtual ||        \
   curr_token == tok_explicit)

#if GNU_EXTENSIONS_ALLOWED

static char *scan_asm_name(void)
/*
Scan a construct of the form
    asm ( "string" )
and return the contents of the string literal.  This is a GNU C extension
that provides the name to be used for an entity in generated assembler
code.  If the construct is not present, or if it is present but there is 
an error, return NULL.
*/
{
  char *result = NULL;

  db_enter(3, "scan_asm_name");
  if (curr_token == tok_asm) {
    /* Bypass "asm" and the leading paren. */
    (void)get_token();
    if (required_token(tok_lparen, ec_exp_lparen)) {
      add_stop_token(tok_rparen);
      /* The next token must be a string constant.  If it isn't,
         flush and then consume any right paren to avoid double
         errors. */
      if (curr_token != tok_string_literal) {
        syntax_error(ec_exp_string_literal);
        if (curr_token == tok_rparen) {
          (void)get_token();
        }  /* if */
      } else {
        /* If there was an error in parsing the string, we do not need
           to issue another error here. */
        if (!is_error_constant(&const_for_curr_token)) {
          /* GCC accepts string literals with embedded null characters (like
             "ab\0c") but ignores everything after the "\0".  So, storing the
             asm argument as a character pointer, without a length, gives
             compatibility with GCC. */
          result = const_for_curr_token.variant.string.value;
        }  /* if */
        /* Consume the string constant. */
        (void)get_token();
        /* There should now be a right parenthesis. */
        (void)required_token(tok_rparen, ec_exp_rparen);
      }  /* if */
      remove_stop_token(tok_rparen);
    }  /* if */
  }  /* if */
  db_exit();
  return result;
}  /* scan_asm_name */

#endif /* GNU_EXTENSIONS_ALLOWED */


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
     current identifier is a type name, don't complain if the name is that
     of a template but there are no template args (since it may actually
     be a different use of the name). */
  options = GID_NO_OPTIONS;
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
        /* Clear the specific_symbol pointer in the locator, to avoid
           biasing subsequent lookup of this identifier. */
        clear_specific_symbol(locator_for_curr_id);
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
#define is_type_name(options) (is_generalized_identifier_start(options) &&\
                               curr_id_is_type_name())


a_boolean is_type_start(a_boolean is_expr_context)
/*
Return TRUE if the current token looks like the start of a type.  A type
starts with a type-specifier (including a typedef name) or a type-qualifier.
is_expr_context is TRUE if this is called from a context in which an
expression is permitted.
*/
{
  a_boolean    is_start = FALSE;

  if (is_type_specifier() || is_type_qualifier() ||
      is_function_specifier() || curr_token == tok_friend) {
    is_start = TRUE;
  } else if (is_type_name(is_expr_context ? GID_IS_EXPR_CONTEXT
                                          : GID_NO_OPTIONS)) {
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
  } else if (curr_token == tok_template || curr_token == tok_export) {
    /* Probably an error. */
    is_start = TRUE;
  } else if (is_type_start(expr_context)) {
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


void clear_decl_pos_block(a_decl_pos_block_ptr  decl_pos_block)
/*
Initialize the fields of the specified decl-pos block.
*/
{
  decl_pos_block->decl_pos = null_source_position;
  decl_pos_block->storage_class_pos = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_pos_block->identifier_range = null_source_range;
  decl_pos_block->specifiers_range = null_source_range;
  decl_pos_block->declarator_range = null_source_range;
  decl_pos_block->var_init_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* clear_decl_pos_block */

#if EXTRA_SOURCE_POSITIONS_IN_IL

a_decl_position_supplement_ptr make_decl_pos_supplement(
                                        a_boolean             at_file_scope,
                                        a_decl_pos_block_ptr  decl_pos_block)
/*
If the specified decl-pos block pointer is non-NULL, allocate a
decl-position-supplement entry, set its values, and return it.  at_file_scope
is TRUE when the entry should be allocated in the file scope memory region.
*/
{
  a_decl_position_supplement_ptr  dpsp;

  if (decl_pos_block != NULL) {
    dpsp = alloc_decl_position_supplement(at_file_scope);
    dpsp->identifier_range = decl_pos_block->identifier_range;
    dpsp->specifiers_range = decl_pos_block->specifiers_range;
    dpsp->variant.declarator_range = decl_pos_block->declarator_range;
  } else {
    dpsp = NULL;
  }  /* if */
  return dpsp;
}  /* make_decl_pos_supplement */


void update_decl_pos_info(a_source_correspondence  *scp,
                          a_decl_pos_block_ptr     decl_pos_block)
/*
If the specified decl-pos block pointer is non-NULL, set the values in
the decl-position-supplement entry pointed to from the specified source
correspondence entry.
*/
{
  a_decl_position_supplement_ptr  dpsp;

  if (decl_pos_block != NULL) {
    dpsp = scp->decl_pos_info;
    if (dpsp != NULL) {
      dpsp->identifier_range = decl_pos_block->identifier_range;
      dpsp->specifiers_range = decl_pos_block->specifiers_range;
      dpsp->variant.declarator_range = decl_pos_block->declarator_range;
    }  /* if */
  }  /* if */
}  /* update_decl_pos_info */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

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
      /* The check for the final semicolon is done by the caller. */
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


a_boolean check_member_function_typedef(a_type_ptr         tp,
                                        a_source_position  *pos)
/*
If tp is a "member function typedef" (cfront compatibility mode only) issue
an error diagnostic and return TRUE.
*/
{
  a_boolean     is_member_function_typedef = FALSE;
  a_type_ptr    rout_type, class_type;
  a_symbol_ptr  sym;

  if (is_cfront_member_function_typedef(tp, &rout_type, &class_type, &sym)) {
    pos_sy_error(ec_bad_use_of_member_function_typedef, pos, sym);
    is_member_function_typedef = TRUE;
  }  /* if */
  return is_member_function_typedef;
}  /* check_member_function_typedef */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- attributes is not used in this case. */
#endif /* !GNU_EXTENSIONS_ALLOWED */
void adjust_parameter_type(a_type_ptr       *type_ptr,
                           an_attribute_ptr attributes)
/*
*type_ptr points to the type of a parameter.  Modify the type if
necessary.  See 3.7.1:  A declaration of a parameter as "array of
type" shall be adjusted to "pointer to type", and the declaration of
a parameter as "function returning type" shall be adjusted to
"pointer to function returning type", as in 3.2.2.1.  attributes
points to a list of GNU C attributes, if applicable.
*/
{
  db_enter(4, "adjust_parameter_type");
  /* Note that incomplete types are allowed.  One can't call a function
     that has an incomplete-type parameter, but one can complete it later. */
  if (is_array_type(*type_ptr)) {
    /* Array, adjust to pointer to element type. */
    a_type_qualifier_set  qualifiers =
                           skip_typerefs(*type_ptr)->variant.array.qualifiers;
    *type_ptr = make_pointer_type(array_element_type(*type_ptr));
    /* A parameter type that is restrict-qualified-array-of-T decays into
       restrict-qualified-ptr-to-T.  (Same with const and volatile in C99.) */
    if (qualifiers != TQ_NONE) {
      *type_ptr = make_qualified_type(*type_ptr, qualifiers);
    }  /* if */
  } else if (is_function_type(*type_ptr)) {
    /* Function, adjust to pointer to function. */
    *type_ptr = make_pointer_type(*type_ptr);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  *type_ptr = apply_attributes_to_variable_type(attributes, *type_ptr);
#endif /* GNU_EXTENSIONS_ALLOWED */
  db_exit();
}  /* adjust_parameter_type */


static void check_type_qualifiers(a_type_ptr         *type_ptr,
                                  a_source_position  *error_pos)
/*
An parameter, variable, or function is about to be declared with the given
type.  Check to see if any type qualifiers that are specified are meaningful.
*/
{
  db_enter(4, "check_type_qualifiers");
  if (get_type_qualifiers(*type_ptr)
#if NEAR_AND_FAR_ALLOWED
                                     & ~(TQ_NEAR|TQ_FAR)
#endif /* NEAR_AND_FAR_ALLOWED */
                                                        ) {
    /* The type has type qualifiers. */
    if ((is_function_type(*type_ptr) && C_dialect != C_dialect_cplusplus) ||
        is_void_type(*type_ptr)) {
      /* Type qualifiers on void types are useless.  On function types they
         are undefined (3.5.3).  This can happen with something like
           typedef int F();
           const F g;
         -- we mark them as useless. */
      pos_warning(ec_useless_type_qualifiers, error_pos);
      /* The useless qualifiers could be removed by the statement
      *type_ptr = make_unqualified_type(*type_ptr);
	 but they are kept in case the back end assigns any meaning to them. */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_type_qualifiers */


void check_and_adjust_parameter_type(a_type_ptr           *type_ptr,
                                     a_source_position    *error_pos,
                                     an_attribute_ptr     attributes)
/*
This routine is called for all function parameter declarations.  It does
error checking and type adjustments as required.  attributes points to a
list of GNU C attributes, if applicable.
*/
{
  if (any_cfront_mode() &&
      check_member_function_typedef(*type_ptr, error_pos)) {
    /* The type is a cfront-style member function typedef -- it is an error
       to use it anywhere but in a pointer-to-member declaration. */
    *type_ptr = error_type();
  } else {
    /* Verify that the parameter type is not a qualified function type. */
    if ((*type_ptr)->kind == (a_type_kind)tk_typeref &&
        typeref_is_typedef(*type_ptr) &&
        is_function_type(*type_ptr) &&
        skip_typerefs(*type_ptr)->variant.routine.extra_info->qualifiers
                                                                 != TQ_NONE) {
      pos_error(ec_bad_qualified_function_type_parameter, error_pos);
    }  /* if */
    /* Adjust the type if necessary (for example, "array of x" becomes
       "pointer to x"). */
    adjust_parameter_type(type_ptr, attributes);
    /* Disallow "void" as a parameter type. */
    if (is_void_type(*type_ptr)) {
      pos_error(ec_void_param_not_allowed, error_pos);
      *type_ptr = error_type();
    } else {
      /* See if any type qualifiers were specified, and if they are
         okay. */
      check_type_qualifiers(type_ptr, error_pos);
      if (!C_mode()) {
        /* In C++ disallow a parameter type that includes a pointer or
           reference to an array of unspecified size (WP 8.3.5 para 3).
           (This restriction is relaxed in cfront and Microsoft compatibility
           modes; it can also be relaxed in default mode -- see
           DEFAULT_PTR_TO_UNKNOWN_BOUND_ARRAY_ALLOWED_IN_PARAM_TYPE.) */
        if (!ptr_to_unknown_bound_array_allowed_in_param_type) {
          a_boolean  is_ref = FALSE;
#if 0
          /* WP 8.3.5 para 3 uses "includes" -- does this cover use in a
             template argument?  We currently assume "yes", but if the answer
             turns out to be "no", the flags passed to traverse_type_tree by
             is_or_contains_ptr_or_ref_to_unknown_bound_array should be
             changed. */
#endif /* if 0 */
          if (is_or_contains_ptr_or_ref_to_unknown_bound_array(*type_ptr,
                                                               &is_ref)) {
            pos_error(is_ref ? ec_param_type_ref_array_of_unknown_bound :
                               ec_param_type_ptr_to_array_of_unknown_bound,
                      error_pos);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_and_adjust_parameter_type */


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
  a_boolean                      any_class_or_enum_type_params = FALSE;
  a_boolean                      any_template_param_type_params = FALSE;
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
#if CHECKING
    if (is_new_operator(opname) || is_delete_operator(opname)) {
      /* Operator new/delete cannot be a nonstatic member function. */
      check_assertion_str2(!is_nonstatic_member_function,
                           "check_operator_function_params:",
                           "new or delete is nonstatic member function");
    }  /* if */
#endif /* CHECKING */
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
      if (is_class_struct_union_type(tp) ||
          (operator_overloading_on_enums_enabled && is_enum_type(tp))) {
        any_class_or_enum_type_params = TRUE;
      } else if (is_template_param_type(tp)) {
        any_template_param_type_params = TRUE;
      }  /* if */
    }  /* if */
    if (is_new_operator(opname) ||
        is_delete_operator(opname) ||
        opname == (an_opname_kind)onk_function_call) {
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
      } else if (opname != (an_opname_kind)onk_function_call) {
        ptp = rout_type->variant.routine.extra_info->param_type_list;
        tp = ptp->type;
        if (!is_error_type(tp)) {
          if (is_new_operator(opname)) {
            /* operator new or operator new[]. */
            if (!is_integral_type(tp) ||
                skip_typerefs(tp)->variant.integer.int_kind !=
                                                      targ_size_t_int_kind) {
              error_code = ec_bad_arg_type_for_operator_new;
              ptp->type = error_type();
            }  /* if */
          } else {
            /* operator delete or operator delete[]. */
            if (!is_void_star_type(tp)) {
              /* Error. */
              an_error_severity  severity;
              if (cfront_2_1_mode && is_pointer_type(tp) &&
                  is_void_type(type_pointed_to(tp))) {
                /* In cfront 2.1 "const void *" is allowed.   Issue a warning
                   and ignore the qualifier on the type. */
                severity = es_warning;
                ptp->type = make_pointer_type(void_type());
              } else {
                /* Error case. */
                severity = es_error;
                ptp->type = error_type();
                err = TRUE;
              }  /* if */
              pos_diagnostic(severity,
                             ec_bad_first_arg_type_for_operator_delete,
                             &locator->source_position);
            }  /* if */
            /* Note: In the ARM and until late 1995 in the Working Paper,
               global delete was allowed exactly one parameter and class
               member delete was allowed exactly two (and the second had to
               be size_t).  This restriction is no longer imposed, given the
               rules on matching delete to new in WP 5.3.4 para 18-19. */
            /* Actually using placement delete only occurs when exception
               handling is enabled, and only with newer ABIs.  If EH support
               is disabled or an old ABI is used, issue a diagnostic if this
               turns out to be a placement delete declaration. */
            if (!err
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
                     && !exceptions_enabled
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
                                           ) {
              ptp = ptp->next;
              if (ptp != NULL) {
                /* There is a second argument.  Except for the case in which
                   a class member operator delete has a second parameter type
                   of size_t, issue a diagnostic. */
                tp = skip_typerefs(ptp->type);
                if (!is_error_type(tp)) {
                  if (class_type != NULL && is_integral_type(tp) &&
                      tp->variant.integer.int_kind == targ_size_t_int_kind) {
                    /* No warning for X::operator delete(void *, size_t). */
                  } else {
                    pos_diagnostic(exceptions_enabled ? es_warning : es_remark,
                                   ec_useless_placement_delete,
                                   &locator->source_position);
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
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
      tp = skip_typerefs(ptp->type);
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        if (!is_integral_type(tp) ||
            tp->variant.integer.int_kind != (an_integer_kind)ik_int) {
          pos_st_error(ec_bad_extra_arg_for_postfix_operator,
                       &locator->source_position,
                       (char *)(opname == (an_opname_kind)onk_plus_plus
                                                               ? "++" : "--"));
          ptp->type = error_type();
          err = TRUE;
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
    if (is_new_operator(opname) || is_delete_operator(opname)) {
      /* Check return type. */
      tp = rout_type->variant.routine.return_type;
      if (!is_error_type(tp)) {
        if (is_new_operator(opname)) {
          /* operator new or operator new[]: return type must be "void *". */
          if (!is_void_star_type(tp) || is_qualified_type(tp)) {
            pos_error(ec_bad_return_type_for_op_new,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        } else {
          /* operator delete or operator delete[]: return type must be
             "void". */
          if (!is_void_type(tp) || is_qualified_type(tp)) {
            pos_error(ec_bad_return_type_for_op_delete,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* If operator function is not a nonstatic member and does not have
         operands of class or enum type (or reference to class or enum type),
         issue an error.  This restriction does not apply to new and delete,
         however. */
      if (!is_nonstatic_member_function && !any_class_or_enum_type_params &&
          !any_template_param_type_params) {
        pos_error(operator_overloading_on_enums_enabled ?
                        ec_no_params_with_class_or_enum_type :
                        ec_no_params_with_class_type,
                  &locator->source_position);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) set_to_error_locator(*locator);
  db_exit();
}  /* check_operator_function_params */


static void report_bad_new_or_delete(a_symbol_locator  *locator,
                                     a_storage_class   storage_class,
                                     a_boolean         *bad_scope)
/*
Issue a diagnostic when attempting to declare an operator new or delete
function that is a namespace member or that has internal linkage (i.e.,
storage_class == sc_static).  If the former is true, set *bad_scope to TRUE.
If a true error is issued mark *locator as an error locator.
*/
{
  an_error_code      error_code = ec_no_error;
  an_error_severity  severity;

  if (locator->is_operator_name && !locator->is_class_member &&
      (is_new_operator(locator->variant.opname) ||
       is_delete_operator(locator->variant.opname))) {
    /* A new or delete operator that is not a class member. */
    if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
        (!locator->is_qualified_name ||
         !locator->is_file_scope_qualified_name)) {
      /* This operator declaration either appears inside a namespace or else
         has the effect of injecting a declaration into a namespace. */
      severity = microsoft_mode ? es_warning : es_error;
      error_code = is_new_operator(locator->variant.opname)?
                                        ec_allocation_operator_in_namespace :
                                        ec_deallocation_operator_in_namespace;
      *bad_scope = TRUE;
    } else if (storage_class == (a_storage_class)sc_static) {
      severity = strict_ansi_mode ? strict_ansi_error_severity : es_warning;
      error_code = ec_no_internal_linkage_for_new_or_delete;
    }  /* if */
    if (error_code != ec_no_error) {
      diagnostic(severity, error_code);
      if (severity == es_error) {  /*lint !e774*/
        /* Set the is_error flag in the locator. */
        set_to_named_error_locator(*locator);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* report_bad_new_or_delete */


static a_boolean compare_exception_specification_type_list(
                              an_exception_specification_ptr  spec_1,
                              an_exception_specification_ptr  spec_2,
                              a_source_position_ptr           throw_pos,
                              an_error_code                   diff_msg,
                              an_error_code                   intro_msg,
                              a_symbol_ptr                    prev_sym,
                              a_boolean                       difference_seen)
/*
Helper function to report differences between two exception specifications
spec_1 and spec_2.  Normally this routine is called twice with the arguments
for spec_1 and spec_2 exchanged and diff_msg set to an error message that
reports missing or extraneous types.  The position of the keyword "throw" that
introduced the latest declaration is throw_pos.  The earlier declaration
resulted in symbol prev_sym.
If difference_seen is FALSE and a difference is seen in this comparison, an
introductory message is issued, and difference_seen is set to TRUE.
The routine returns difference_seen.
*/
{
  an_exception_specification_type_ptr  etype_1, etype_2;

  etype_1 = spec_1->exception_specification_type_list;
  for (; etype_1 != NULL; etype_1 = etype_1->next) {
    if (etype_1->redundant) {
      /* Don't bother looking for a match on redundant types.  It will already
         have been done. */
    } else {
      a_boolean  match = FALSE;

      etype_2 = spec_2->exception_specification_type_list;
      for (; etype_2 != NULL; etype_2 = etype_2->next) {
        if (!etype_2->redundant && etype_2->type != NULL &&
            identical_types(etype_1->type, etype_2->type)) {
          /* An entry of the same type was found on the list of spec_2. */
          match = TRUE;
          break;
        }  /* if */
      }  /* for */
      if (!match) {
        /* No match was found, so the list of spec_2 does not have a type that
           is on the list of spec_1. */
        if (!difference_seen) {
          /* The diagnostics will be combined with a header message followed
             by additional messages identifying the specific discrepancy.
             This is the first diagnostic, so put out the header message
             first. */
          pos_stsy_start_error(intro_msg, throw_pos, ":", prev_sym);
          difference_seen = TRUE;
        }  /* if */
        ty_add_diag_info(diff_msg, etype_1->type);
      }  /* if */
    }  /* if */
  }  /* for */
  return difference_seen;
}  /* compare_exception_specification_type_list */


void check_exception_specification(a_type_ptr         new_rout_type,
                                   a_symbol_ptr       prev_decl,
                                   a_source_position  *throw_pos,
                                   a_boolean          is_redecl)
/*
Check that the throw specification on the current declaration, if any, is
consistent with that of the previous declaration.
*/
{
  a_boolean                       any_difference_seen;
  an_exception_specification_ptr  new_tsp, old_tsp;
  an_error_code                   error_code;
  a_routine_ptr                   rp = NULL;
  a_type_ptr                      prev_type;

  db_enter(4, "check_exception_specification");
  /* Retrieve the routine type of the previous declaration: */
  switch (prev_decl->kind) {
    case sk_routine:
    case sk_member_function:
      rp = prev_decl->variant.routine.ptr;
      prev_type = rp->type;
      break;
    case sk_extern_routine:
      rp = prev_decl->variant.extern_symbol_descr->variant.routine.ptr;
      prev_type = rp->type;
      break;
    case sk_function_template:
      rp = prev_decl->variant.template_info->variant.function.routine;
      prev_type = rp->type;
      break;
    case sk_variable:
      prev_type = prev_decl->variant.variable.ptr->type;
      break;
    case sk_static_data_member:
      prev_type = prev_decl->variant.static_data_member.variable->type;
      break;
    case sk_extern_variable:
      prev_type =
               prev_decl->variant.extern_symbol_descr->variant.variable->type;
      break;
    default:
      unexpected_condition_str(
                            "check_exception_specification: bad symbol kind");
  }  /* switch */
  if (is_or_contains_error_type(prev_type) ||
      is_or_contains_error_type(new_rout_type)) {
    /* Something went wrong earlier on; do not attempt to issue more
       diagnostics. */
    goto done;
  }  /* if */
  if (rp == NULL) {
    /* Not a routine type, but a pointer-to, reference-to or pointer-to-member
       function. */
    if (is_ptr_to_member_type(prev_type) &&
        is_ptr_to_member_type(new_rout_type)) {
      prev_type = pm_member_type(skip_typerefs(prev_type));
      new_rout_type = pm_member_type(skip_typerefs(new_rout_type));
    } else if (is_ptr_or_ref_type(prev_type) &&
               is_ptr_or_ref_type(new_rout_type)) {
      prev_type = type_pointed_to(skip_typerefs(prev_type));
      new_rout_type = type_pointed_to(skip_typerefs(new_rout_type));
    }  /* if */
  }  /* if */
  if (!(is_function_type(prev_type) && is_function_type(new_rout_type))) {
    /* There is something more fundamentally wrong that an exception
       specification mismatch (likely the same name is used for two very
       different kinds of entities).  Skip this processing. */
    goto done;
  }  /* if */
  if (exceptions_enabled && prev_type->kind != (a_type_kind)tk_typeref) {
    old_tsp = skip_typerefs(prev_type)->
                         variant.routine.extra_info->exception_specification;
    new_tsp = skip_typerefs(new_rout_type)->
                    variant.routine.extra_info->exception_specification;
    /* Set error_code for issuing diagnostics. */
    if (is_redecl) {
      /* This a function redeclaration -- the exception specifications have to
         match. */
      error_code = ec_incompatible_exception_specification;
    } else {
      /* Not a redeclaration -- probably a template specialization.
         In diagnostics refer to template rather than a previous declaration
         of this instance. */
      error_code = ec_bad_exception_specification_for_specialization;
      /* Distinguish between (member) function specializations and static
         data member specializations: */
      if (rp != NULL && prev_decl->variant.routine.instance_ptr != NULL) {
        /* In diagnostics refer to template rather than a previous declaration
           of this instance. */
        prev_decl = prev_decl->variant.routine.instance_ptr->template_sym;
      } else if (rp == NULL &&
                 prev_decl->variant.static_data_member.instance_ptr != NULL) {
        prev_decl =
             prev_decl->variant.static_data_member.instance_ptr->template_sym;
      }  /* if */
    }  /* if */
    if (rp != NULL && rp->compiler_generated) {
      /* Ignore any differences between exception specifications on a
         compiler generated routine (e.g., predeclared operator new or delete)
         and the current declaration. */
    } else if (old_tsp == NULL) {
      /* Previous specification asserted that any exception may be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_tsp != NULL) {
        /* Previously the exception specification was absent; now one is
           provided.  Issue an error. */
        pos_stsy_error(error_code, throw_pos, "", prev_decl);
      }  /* if */
    } else if (new_tsp == NULL) {
      /* Issue a diagnostic on the omission of a throw specification on the
         current declaration (it must have been present on the previous
         one). */
      an_error_severity  severity = es_error;
      /* Unless we are in strict mode, issue a warning instead of an error
         if this is a redeclaration of what may be a library new or delete
         routine: the relaxation is to ease the upgrading of old code. */
      if (is_redecl && rp != NULL && !rp->source_corresp.is_class_member &&
          (is_new_operator(rp->opname_or_builtin.opname_kind) ||
           is_delete_operator(rp->opname_or_builtin.opname_kind))) {
        /* Set the severity, depending on the strict mode setting. */
        severity = strict_ansi_mode ? strict_ansi_error_severity : es_warning;
      }  /* if */
      pos_sy_diagnostic(severity,
                        is_redecl?
                          ec_omitted_exception_specification :
                          ec_omitted_exception_specification_on_specialization,
                        throw_pos, prev_decl);
    } else if (old_tsp->exception_specification_type_list == NULL) {
      /* Previous specification asserted that no exceptions will be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_tsp->exception_specification_type_list != NULL) {
        pos_stsy_start_error(error_code, throw_pos, ":", prev_decl);
        add_diag_info(ec_previous_exception_specification_was_empty);
        end_error();
      }  /* if */
    } else {
      /* Previous specification was a list of the types that will be thrown.
         Check for a mismatch between the previous list and the current one. */
      any_difference_seen = FALSE;
      /* Check extraneous types: */
      any_difference_seen = compare_exception_specification_type_list(
                              new_tsp, old_tsp, throw_pos,
                              ec_omitted_in_previous_exception_specification,
                              error_code, prev_decl, any_difference_seen);
      /* Check missing types: */
      any_difference_seen = compare_exception_specification_type_list(
                              old_tsp, new_tsp, throw_pos,
                              ec_included_in_previous_exception_specification,
                              error_code, prev_decl, any_difference_seen);
      if (any_difference_seen) end_error();
    }  /* if */
  }  /* if */
done:
  db_exit();
}  /* check_exception_specification */


a_routine_ptr make_routine(a_type_ptr      type_ptr,
                           a_storage_class storage_class,
                           a_scope_depth   scope_depth)
/*
Allocate an entry for a routine with function type type_ptr and storage class
storage_class, and return a pointer to it.  The entry is allocated at the
file scope.  type_ptr must be in the file scope.  Unless scope_depth is
NO_SCOPE_DEPTH, add the new routine entry to the routines list of the
specified scope.
*/
{
  a_routine_ptr          rp;
  a_memory_region_number region_to_switch_back_to;

  /* Always allocate routines at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  rp->type = type_ptr;
  rp->storage_class = storage_class;
  if (scope_depth != NO_SCOPE_DEPTH) add_to_routines_list(rp, scope_depth);
  switch_back_to_original_region(region_to_switch_back_to);
  return rp;
}  /* make_routine */


static void make_anonymous_union_variable(a_type_ptr      anon_union_type,
                                          a_storage_class storage_class)
/*
Create a variable to represent an anonymous union.  Issue an error if its
storage class is invalid.  Also promote the fields of the union to the
current scope.
*/
{
  a_variable_ptr vp;
  a_boolean      at_file_or_namespace_scope;
  a_symbol_ptr   assoc_object_sym;
  a_scope_depth  scope_depth;

  at_file_or_namespace_scope =
                     (depth_scope_stack == depth_innermost_namespace_scope);
  /* Check the storage class.  At file scope, only static is allowed. */
  if (at_file_or_namespace_scope) {
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
    /* Not at file scope or namespace scope. */
    switch (storage_class) {
      case sc_extern:
        /* Error, then default to automatic. */
        error(ec_anon_union_storage_class);
        /*FALLTHROUGH*/
      case sc_unspecified:
        /* Default to automatic. */
        storage_class = (a_storage_class)sc_auto;
        /*FALLTHROUGH*/
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
  scope_depth = at_file_or_namespace_scope ?
                     depth_innermost_namespace_scope : decl_scope_level; 
  vp = make_variable(anon_union_type, storage_class, scope_depth);
  vp->is_anonymous_parent_object = TRUE;
  /* Promote the fields of the anonymous union to the current scope, and do
     some error checking on the anonymous union's members. */
  assoc_object_sym = make_anonymous_parent_object_symbol(
                                         (a_symbol_kind)sk_variable,
                                         &pos_curr_token,
                                         scope_stack[decl_scope_level].number);
  assoc_object_sym->variant.variable.ptr = vp;
  if (at_file_or_namespace_scope) {
    set_namespace_membership(assoc_object_sym, &vp->source_corresp,
                             (a_namespace_ptr)NULL);
  } else {
    vp->source_corresp.is_local_to_function = TRUE;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Mark the type declaration as autonomous. */
  anon_union_type->autonomous_primary_tag_decl = TRUE;
  /* Also put out a source sequence entry for the variable (even though the
     variable declaration doesn't actually appear). */
  vp->declared_type = anon_union_type;
  add_to_source_sequence_list((char *)vp, (an_il_entry_kind)iek_variable);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Promote symbols for anonymous unions members to the enclosing scope.
     Error checking is also done. */
  check_anonymous_union_symbols(assoc_object_sym, (a_type_ptr)NULL,
                                /*is_nonstd=*/FALSE);
}  /* make_anonymous_union_variable */


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
      /* In C-mode a type declared in a parameter declaration is local to
         the function.  Issue a warning on type declarations, since they will
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
  sym = enter_symbol(kind, locator, scope_level, suppress_redecl_error);
  db_exit();
  return(sym);
}  /* enter_local_symbol */


a_boolean is_single_param_operator_new_or_delete(a_symbol_locator *locator,
                                                 a_type_ptr       type)
/*
Return TRUE if the locator is for an operator new or delete and the type
indicates that it is the default version (i.e., if it has exactly one
parameter, which elsewhere is confirmed to have type size_t (new) or void*
(delete).
*/
{
  a_boolean         match = FALSE;
  a_param_type_ptr  ptp;

  if (locator->is_operator_name &&
      (is_new_operator(locator->variant.opname) ||
       is_delete_operator(locator->variant.opname))) {
    check_assertion(is_function_type(type));
    ptp = (skip_typerefs(type))->variant.routine.extra_info->param_type_list;
    if (ptp != NULL && ptp->next == NULL) {
      match = TRUE;
    }  /* if */
  }  /* if */
  return match;
}  /* is_single_param_operator_new_or_delete */


typedef struct an_id_linkage_block {
  a_symbol_locator
		*locator;
			/* Locator associated with the current variable,
			   routine, or function template declaration. */
  a_symbol_ptr	linked_symbol;
			/* A symbol apparently representing a prior
			   declaration of the entity currently being
			   declared. */
  a_symbol_ptr	homonym_symbol;
			/* If linked_symbol is NULL, another declaration
			   with the same name and belonging to the same
			   scope; otherwise, if linked_symbol is a newly
			   created symbol representing a function template
			   instance that is a guiding declaration,
			   homomyn_symbol may point to the symbol for the
			   template, with which the guiding-declaration symbol
			   will be overloaded; otherwise, homonym_symbol is
			   always NULL when linked_symbol in non-NULL.  (C++
			   only, and used only with function declarations.) */
  a_symbol_ptr  prior_decl_in_enclosing_scope;
			/* If the current declaration is a block-extern or
			   friend declaration, a prior declaration in an
			   enclosing scope (used to determine linkage);
			   otherwise NULL. */
  a_symbol_ptr	overload_symbol;
			/* A symbol representing the overload set to which
			   the current declaration already belongs or will
			   belong.  Specifically, if linked_symbol or
			   homonym_symbol is non-NULL and is an overload-set
			   member, it is the associated sk_overloaded_function
			   symbol.  If homonym_symbol is itself an overload
			   symbol, a pointer to the same symbol.  It is
			   always NULL when both linked_symbol and
			   homonym_symbol are NULL.  (C++ only, and used only
			   with function declarations.) */
  a_scope_depth
		effective_decl_level;
			/* The effective scope depth of the declaration.  It
			   is usually the same as decl_scope_level, except
			   in the case of friend declarations and certain
			   template declarations. */
  a_storage_class
		storage_class;
			/* On entry to id_linkage, the storage class
			   explicitly specified in the declaration.  May be
			   replaced in id_linkage by a setting corresponding
			   to the linkage. */
  a_func_info_block
		*func_info;
			/* Pointer to a func_info block for the declaration.
			   NULL for variable declarations. */
  a_type_ptr	type;
			/* The type with which the entity was declared. */
  a_byte_boolean
		is_friend_decl;
			/* TRUE when the declaration is a friend
			   declaration. */
  a_byte_boolean
		is_function_template;
			/* TRUE when the declaration is a function template
			   declaration. */
  a_byte_boolean
		is_new_template_instance;
			/* TRUE when the declaration matches a function
			   template instance that is newly created by the
			   declaration. */
  a_byte_boolean
		is_definition;
			/* TRUE when the declaration is a definition. */
  a_byte_boolean
		is_block_extern_decl;
			/* TRUE when the declaration is a block extern
			   declaration (a declaration within a function of
			   an entity with linkage outside the function). */
  a_byte_boolean
		is_local_class_friend_decl;
			/* TRUE when is_friend_decl is TRUE and the class in
			   which the declaration appears is a local class. */
  a_byte_boolean
		within_unnamed_namespace;
			/* TRUE when the declaration appears within an
			   unnamed namespace. */
  a_byte_boolean
		extern_C_name_linkage_specified;
			/* TRUE when the declaration appears within the
			   context of an extern "C" linkage specification. */
  a_byte_boolean
		direct_linkage_specifier;
			/* If TRUE, a linkage specification appeared directly
			   on the declaration (as opposed to the declaration
			   just being in a linkage specification block). */
  a_byte_boolean
		namespace_reactivated;
			/* TRUE if a namespace reactivation scope was
			   pushed during linkage processing; it must be
			   popped by the caller. */
  a_template_param_ptr
		templ_param_list;
			/* When is_function_template is TRUE, a pointer to
			   the associated template parameter list. */
  an_id_linkage_kind
		linkage;
			/* The linkage (none, internal, external) computed
			   for the current declaration. */
  a_name_linkage_kind
		name_linkage;
			/* The name linkage for the current declaration. */
  a_byte_boolean
		name_linkage_is_explicit;
			/* TRUE if the name linkage for the current
			   declaration was explicitly specified. */
} an_id_linkage_block;


static void clear_id_linkage_block(an_id_linkage_block *idlbp)
/*
*/
{
  idlbp->locator = NULL;
  idlbp->homonym_symbol = NULL;
  idlbp->prior_decl_in_enclosing_scope = NULL;
  idlbp->linked_symbol = NULL;
  idlbp->overload_symbol = NULL;
  idlbp->effective_decl_level = NO_SCOPE_DEPTH;
  idlbp->storage_class = (a_storage_class)sc_unspecified;
  idlbp->func_info = NULL;
  idlbp->type = NULL;
  idlbp->is_friend_decl = FALSE;
  idlbp->is_function_template = FALSE;
  idlbp->is_new_template_instance = FALSE;
  idlbp->is_definition = FALSE;
  idlbp->is_block_extern_decl = FALSE;
  idlbp->is_local_class_friend_decl = FALSE;
  idlbp->within_unnamed_namespace = FALSE;
  idlbp->extern_C_name_linkage_specified = FALSE;
  idlbp->direct_linkage_specifier = FALSE;
  idlbp->namespace_reactivated = FALSE;
  idlbp->templ_param_list = NULL;
  idlbp->linkage = idl_none;
  idlbp->name_linkage = (a_name_linkage_kind)nlk_none;
  idlbp->name_linkage_is_explicit = FALSE;
}  /* clear_id_linkage_block */


static void compute_effective_decl_level(an_id_linkage_block  *idlbp,
                                         a_scope_depth        depth)
/*
A name is being declared, and the "effective declaration level" identifies
the scope into which the newly declared symbol will be entered.  Usually
this is exactly the same as the current declaration scope, as indicated by
decl_scope_level.  This routine checks for some special cases and returns
the appropriate scope depth.  (The normal case is that the effective
declaration level is the same as decl_scope_level, which in turn is usually
the same as depth_scope_stack).
*/
{
  db_enter(4, "compute_effective_decl_level");
  if (C_mode()) {
    if (idlbp->is_block_extern_decl) {
      if (C_dialect == C_dialect_pcc &&
          idlbp->storage_class == (a_storage_class)sc_extern) {
        /* In pcc mode, functions and extern variables are always effectively
           declared at the file scope level. */
        depth = DEPTH_OF_FILE_SCOPE;
      } else if (idlbp->func_info != NULL &&
                 idlbp->storage_class == (a_storage_class)sc_static) {
        /* In C mode, as extension, "static" is accepted on function
           declarations at function scope.  Such declarations are promoted
           to file scope. */
        depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
    }  /* if */
  } else if (idlbp->is_friend_decl) {
    /* Make the appropriate adjustment for a friend declaration.  The
       The "effective declaration level" of a friend function declaration is 
       usually the innermost non-class scope.  Starting from the specified
       scope depth, find the depth of the containing non-class scope. */
    a_scope_kind  kind = scope_stack[depth].kind;
    while (kind == (a_scope_kind)sck_class_struct_union ||
           kind == (a_scope_kind)sck_class_reactivation) {
      depth--;
      kind = scope_stack[depth].kind;
      if (kind == (a_scope_kind)sck_template_instantiation) {
        if (scope_stack[depth].in_prototype_instantiation) {
          /* During prototype instantiation, use the instantiation scope
             as the effective declaration scope. */
        } else {
          depth = depth_innermost_namespace_scope;
        }  /* if */
        break;
      } else if (kind == (a_scope_kind)sck_template_declaration) {
        /* Must be an error of some sort. */
        depth = depth_innermost_namespace_scope;
        break;
      }  /* if */
    }  /* while */
  }  /* if */
  idlbp->effective_decl_level = depth;
  db_exit();
}  /* compute_effective_decl_level */


static a_boolean is_local_class_friend_decl(an_id_linkage_block  *idlbp)
/*
Return TRUE if and only if the declaration being processed (with the given
id-linkage block) is a friend declaration in a local class.
*/
{
  a_boolean  result = FALSE;
  a_scope_depth decl_level = idlbp->effective_decl_level;

  if (idlbp->is_friend_decl &&
      decl_level != depth_innermost_namespace_scope &&
      (scope_stack[decl_level].kind == (a_scope_kind)sck_block ||
       scope_stack[decl_level].kind == (a_scope_kind)sck_function)) {
      result = TRUE;
  }  /* if */
  return result;
}  /* is_local_class_friend_decl */


static void set_linkage_environment(an_id_linkage_block  *idlbp,
                                    a_scope_depth        orig_decl_level)
/*
Set values in the indicated id-linkage block to reflect the environment of
the current declaration.  orig_decl_level is usually the current scope,
except in the case of function-template declarations (for which a template
declaration scope will have been pushed).
*/
{
  if (scope_stack[orig_decl_level].kind ==
                           (a_scope_kind)sck_class_struct_union) {
    /* This must be a friend declaration. */
    check_assertion(!C_mode());
    idlbp->is_friend_decl = TRUE;
  }  /* if */
  /* The effective declaration level is usually the current scope -- but not
     always (e.g., friend declarations). */
  compute_effective_decl_level(idlbp, orig_decl_level);
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    /* A declaration inside a function. */
    if (idlbp->func_info != NULL ||
        idlbp->storage_class == (a_storage_class)sc_extern) {
      /* It's a function declaration or a variable declaration on which
         "extern" appeared explicitly. */
      idlbp->is_block_extern_decl = TRUE;
    }  /* if */
  }  /* if */
  if (!C_mode()) {
    a_scope_depth  depth;

    /* Record whether this is a friend declaration inside a local class. */
    if (idlbp->is_friend_decl) {
      idlbp->is_local_class_friend_decl = is_local_class_friend_decl(idlbp);
    }  /* if */
    /* Record whether this declaration appears within the scope of an
       unnamed namespace. */
    if (idlbp->is_block_extern_decl ||
        idlbp->is_local_class_friend_decl) {
      depth = depth_innermost_namespace_scope;
    } else {
      depth = idlbp->effective_decl_level;
    }  /* if */
    if (scope_stack[depth].within_unnamed_namespace) {
      idlbp->within_unnamed_namespace = TRUE;
    }  /* if */
    /* Record whether this declaration appears within the context of an
       extern "C" linkage specification. */
    if (scope_stack[decl_scope_level].default_name_linkage ==
                                       (a_name_linkage_kind)nlk_external) {
      idlbp->extern_C_name_linkage_specified = TRUE;
    }  /* if */
  }  /* if */
}  /* set_linkage_environment */


static void compute_name_linkage(an_id_linkage_block  *idlbp)
/*
Based on the current state of the indicated id-linkage block, set the
fields describing the name-linkage for the current declaration.  This
routine can be called more than once for a given declaration (e.g., when
an entity initially declared with external linkage is redeclared to have
internal linkage).
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_symbol_ptr             prior_decl;
  a_source_correspondence  *scp;

  idlbp->name_linkage_is_explicit = FALSE;
  if (idlbp->linkage == idl_external ||
      /* In Sun mode a name linkage specifier also affects functions with
         internal name linkage: */
      (sun_mode && idlbp->linkage == idl_internal &&
       ssep->name_linkage_is_explicit)) {
    if (C_mode()) {
      /* External entity in C mode. */ 
      idlbp->name_linkage = (a_name_linkage_kind)nlk_external;
    } else if (idlbp->func_info != NULL &&
               idlbp->func_info->is_main_function) {
      /* In C++ "main" gets C++ linkage. */
      idlbp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    } else if (idlbp->is_function_template) {
      /* Function templates with external linkage get C++ linkage. */
      idlbp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    } else if (ssep->name_linkage_is_explicit) {
      /* Use the explicitly specified name-linkage even when there is a
         prior declaration with different linkage. */
      idlbp->name_linkage = ssep->default_name_linkage;
      idlbp->name_linkage_is_explicit = TRUE;
    } else {
      /* No explicit linkage specification.  Use the prior declaration (if
         there is one), or else the default. */
      prior_decl = idlbp->linked_symbol;
      if (prior_decl == NULL) {
        /* A block extern declaration for which there is a visible prior
           declaration. */
        prior_decl = idlbp->prior_decl_in_enclosing_scope;
      }  /* if */
      if (prior_decl != NULL) {
        reduce_projection_symbol_to_fundamental_symbol(prior_decl);
        if (is_type_symbol(prior_decl) ||
            prior_decl->kind == (a_symbol_kind)sk_field ||
            prior_decl->kind == (a_symbol_kind)sk_constant ||
            prior_decl->kind == (a_symbol_kind)sk_undefined) {
          /* The prior declaration may have been a typedef in an enclosing
             scope; there is no relationship wrt. name linkage.  In error
             situations, we may also pick up fields, constants or undefined
             identifiers: ignore them. */
          prior_decl = NULL;
        }  /* if */
      }  /* if */
      if (prior_decl != NULL &&
          (idlbp->func_info == NULL) ==
                     (prior_decl->kind == (a_symbol_kind)sk_variable)) {
       /* Use the linkage specifier from the prior declaration. */
        scp = source_corresp_entry_for_symbol(prior_decl);
        check_assertion(scp != NULL);
        idlbp->name_linkage = scp->name_linkage;
      } else {
        /* Use the default. */
        idlbp->name_linkage = ssep->default_name_linkage;
      }  /* if */
    }  /* if */
    if (idlbp->type->kind == (a_type_kind)tk_routine &&
        is_name_linkage_kind_for_rout_type(idlbp->name_linkage)) {
      /* In case there's a change, reset the routine-name-linkage (i.e.,
         calling convention) in the routine type.  Note this is not done
         when the type is based on a typedef.  Also, certain custom
         name linkage kinds may not apply to routine types. */
      idlbp->type->variant.routine.extra_info->
                            routine_name_linkage = idlbp->name_linkage;
    }  /* if */
  } else if (idlbp->linkage == idl_internal) {
    /* Internal linkage is easy -- ignore the default linkage specifier and
       the linkage of prior declarations. */
    idlbp->name_linkage = (a_name_linkage_kind)nlk_internal;
  }   /* if */
}  /* compute_name_linkage */


static void find_linked_symbol(an_id_linkage_block *idlbp)
/*
Find and return a symbol representing the potential prior declaration of the
variable or routine named by the specified symbol locator.  This deals with
two kinds of cases:
  -- redeclaration in the same scope:
        void f();
        void f() { }        // the other "f" is returned as linked symbol
        static int i;
        extern int i;       // the other "i" is returned as linked symbol
  -- friend declaration (C++ only):
        void g();
        class A {
          friend g();       // the other "g" is returned as linked symbol
        };

It also returns two other sorts of symbols.  (1) *prior_decl is the same as
the linked symbol in the cases listed above, but a symbol may also be returned
as *prior_decl even when the linked symbol is NULL, e.g.,
  -- block extern declaration
        int j;
        void f() {
          extern int j;     // linked_symbol is NULL but the other "j" is
        }                   //   returned as *prior_decl
(Note, however, that the file-scope symbol is returned as *prior_decl only
when it is "visible", e.g.,
        static int k;
        void f() {
          int k;            // ::k is no longer visible
          { extern int k; } // both linked_symbol and *prior_decl are NULL
        }
This behavior affects how id_linkage determines the linkage of block-extern
declared k; the latter gets external linkage, which results in a linkage
conflict that is reported in strict mode.)

And (2), in C++ only, when type is a routine type, *overload_symbol is
returned when a name match was found with another function declaration,
whether or not a type match was also found.  (In C++ mode the types must
match for either linked_symbol or *prior_decl to be set for a routine.)  For
instance,
  -- redeclaration in the same scope:
       void f(int);
       void f(int,int);     // both linked_symbol and *prior_decl are NULL,
                            //   but f(int) is returned as *overload_symbol
  -- friend declaration
       void f(int);
       class A {
         friend f(int,int);  // both linked_symbol and *prior_decl are NULL,
       };                    //   but f(int) is returned as *overload_symbol
  -- block extern
       void f(int);
       void g() {
         extern f(int,int);  // Both linked_symbol and *prior_decl are NULL,
       }                     //   but f(int) is returned as *overload_symbol
The logic described here is also extended to take function templates
into account, since they also participate in overload sets.

Besides the type, required for function matching in C++, and the locator, the
input parameters include effective_decl_level, the scope at which the entity
is to be entered into the symbol table; is_main, TRUE when the current
declaration is global "main"; is_friend_decl, TRUE when the declaration is a
friend declaration within a class; and is_function_template, TRUE when the
declaration is a function template declaration.  templ_param_list is the
template parameter list for the function template.  This function is only
called by id_linkage.
*/
{
  a_boolean     decls_at_same_scope;
  a_boolean     is_list;
  a_symbol_ptr  other_decl, other_decl_saved;
  a_symbol_kind kind = (a_symbol_kind)sk_last;
  a_boolean     function_template_seen = FALSE;
  a_boolean     is_function = is_function_type(idlbp->type);
  a_boolean     is_namespace_member_def = FALSE;
  a_symbol_locator  *locator = idlbp->locator;
  a_boolean     is_guiding_decl = FALSE;

  db_enter(3, "find_linked_symbol");
  if (locator->specific_symbol != NULL &&
      (qualifier_namespace_ptr(*locator) != NULL ||
       locator->is_file_scope_qualified_name ||
       locator->is_template_id)) {
    /* The declarator is a namespace-qualified or a global-scope-qualified
       name.  If it's not a friend declaration, it is supposed to be a
       definition. */
    if (!idlbp->is_friend_decl) is_namespace_member_def = TRUE;
    other_decl = locator->specific_symbol;
    /* If the name, looked up as a member of the specified scope, turns out
       to be a namespace projection, it must have been pulled into the
       scope via a using declaration or a using directive.  In either case it
       is not a valid declarator -- ignore it. */
    if (other_decl->kind == (a_symbol_kind)sk_namespace_projection) {
      other_decl = NULL;
    }  /* if */
  } else {
    if (idlbp->is_block_extern_decl || idlbp->is_local_class_friend_decl) {
      /* This is either a block-extern declaration of a function or variable
         or (what amounts to the same thing) a friend declaration within a
         local class.  Find the visible declaration of the same name. */
      (void)normal_id_lookup(locator, IDL_LINKAGE_LOOKUP);
      other_decl = locator->specific_symbol;
    } else {
      /* Not a context in which lookup in enclosing scopes is meaningful.
         Just check for a prior declaration in the current scope. */
      check_assertion(idlbp->effective_decl_level ==
                                            depth_innermost_namespace_scope ||
                      idlbp->is_friend_decl);
      if (depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
        /* Do the lookup in the file scope. */
        (void)file_scope_id_lookup(il_header.primary_scope,
                                   locator, IDL_LINKAGE_LOOKUP);
        other_decl = locator->specific_symbol;
      } else {
        /* Do the lookup in the innermost namespace scope. */
        a_namespace_ptr  nsp;
        nsp = scope_stack[depth_innermost_namespace_scope].il_scope->
                                                       variant.assoc_namespace;
        (void)namespace_qualified_id_lookup(locator, nsp, IDL_LINKAGE_LOOKUP);
        other_decl = locator->specific_symbol;
      }  /* if */
    }  /* if */
    /* Clear out the specific symbol pointer of the locator.  It was set by
       the lookup routine, but it may not be valid. */
    clear_specific_symbol(*locator);
  }  /* if */
  /* We are only interested in variable and function declarations.  If
     something else was found, we're not interested. */
  if (other_decl != NULL) {
    kind = other_decl->kind;

    decls_at_same_scope = (other_decl->decl_scope ==
                            scope_stack[idlbp->effective_decl_level].number);
    if ((microsoft_mode || sun_mode) &&
        kind == (a_symbol_kind)sk_namespace_projection) {
      /* In Microsoft mode, a member of another namespace can be redeclared
         with an unqualified declarator if that member was made visible via
         a using-declaration.  E.g.,
           namespace N { void f(); }  using N::f; void f() {} // Fine: N::f
         We emulate this only if the entity has C name linkage.  */
      a_symbol_ptr  fund_other_decl = fundamental_symbol_of(other_decl);
      if (source_corresp_entry_for_symbol(fund_other_decl)->name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
        kind = fund_other_decl->kind;
      }  /* if */
    }  /* if */
    if (kind == (a_symbol_kind)sk_variable ||
        kind == (a_symbol_kind)sk_routine ||
        kind == (a_symbol_kind)sk_function_template ||
        kind == (a_symbol_kind)sk_overloaded_function) {
      /* Okay to use other_decl. */
    } else {
      if (!C_mode() && kind == (a_symbol_kind)sk_namespace_projection) {
        a_symbol_kind  fund_kind = fundamental_symbol_of(other_decl)->kind;
        if (fund_kind == (a_symbol_kind)sk_routine ||
            fund_kind == (a_symbol_kind)sk_function_template) {
          /* In a case like:
               namespace N { void f(int); }
               using N::f;
               void f();
             we'll need to form an overload set with N::f and ::f. */
          if (decls_at_same_scope) {
            idlbp->homonym_symbol = other_decl;
          }  /* if */
        } else if (fund_kind == (a_symbol_kind)sk_variable &&
                   depth_innermost_function_scope == NO_SCOPE_DEPTH) {
          /* This is a variable declaration at file/namespace scope.  We need
             to deal with a case like this:
                 namespace N { extern "C" int i; }
                 using N::i;
                 extern "C" int i;
             [namespace.udecl] says a declaration can coexist with a
             using-declaration as long as they refer to the same entity.
             We know the other declaration was in fact a using-declaration,
             so check whether both declarations refer to extern "C"
             variables of the same type. */
          a_variable_ptr  vp = fundamental_symbol_of(other_decl)->
                                                  variant.variable.ptr;
          if (vp->source_corresp.name_linkage ==
                                    (a_name_linkage_kind)nlk_external &&
              scope_stack[depth_scope_stack].default_name_linkage ==
                                    (a_name_linkage_kind)nlk_external &&
              identical_types(vp->type, idlbp->type)) {
            /* Remove other_decl from the symbol table.  It will be replaced
               by the current variable declaration. */
            remove_symbol(other_decl);
          }  /* if */
        }  /* if */
      }  /* if */
      if ((idlbp->is_block_extern_decl ||
           idlbp->is_local_class_friend_decl) &&
          other_decl->decl_scope != scope_stack[depth_scope_stack].number) {
        /* This is a friend or block-extern declaration and a declaration
           that cannot possibly match it was found in an enclosing scope.
           Remember it for a subsequent error message. */
        idlbp->prior_decl_in_enclosing_scope = other_decl;
      }  /* if */
      other_decl = NULL;
    }  /* if */
  }  /* if */
  if (other_decl != NULL) {
    /* A match was found in searching the symbol table.  However, in
       C++ we have to allow for function overloading.  If what we found
       was an sk_overloaded_function symbol, we need to look for a type
       match amongst the instances of the name.  Even if it was an
       sk_routine symbol, we may want to overload the two functions. */
    if (C_dialect == C_dialect_cplusplus && is_function &&
        kind != (a_symbol_kind)sk_variable) {
      /* C++ function -- type compatibility check is required. */
      if (decls_at_same_scope) {
        /* *overload_symbol is set for cases in which the current symbol
           may be added to an overload list.  Note that overloading across
           scopes is not allowed.  Also, *overload_symbol may end up being
           cleared later. */
        idlbp->homonym_symbol = other_decl;
      }  /* if */
      if (other_decl->kind == (a_symbol_kind)sk_overloaded_function) {
        if (decls_at_same_scope) idlbp->overload_symbol = other_decl;
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
        a_symbol_ptr	fund_other_decl;
        fund_other_decl = fundamental_symbol_of(other_decl);
        if (other_decl->kind == (a_symbol_kind)sk_namespace_projection &&
            !locator->is_template_id &&
            !((sun_mode || microsoft_mode) &&
              source_corresp_entry_for_symbol(fund_other_decl)->name_linkage ==
                                         (a_name_linkage_kind)nlk_external)) {
          /* Ignore namespace projection symbols that may have gotten into
             this overload set by a using declaration -- e.g.,
               namespace N { void f(int); }
               using N::f;
               void f();
               int f(int);           // Does *not* match N::f(int)
             (except in Sun and Microsoft modes.)
          */
        } else if (fund_other_decl->kind ==
                                         (a_symbol_kind)sk_function_template) {
          a_template_symbol_supplement_ptr  tssp;
          tssp = fund_other_decl->variant.template_info;
          if (idlbp->is_function_template) {
            a_template_param_ptr	other_templ_param_list;
            tp = tssp->variant.function.routine->type;
            other_templ_param_list =
                       tssp->variant.function.decl_cache.decl_info->parameters;
            if (equiv_template_param_lists(other_templ_param_list,
                                           idlbp->templ_param_list,
                                           /*issue_errors=*/FALSE,
                                           (a_source_position*)NULL) &&
                routine_types_are_compatible(tp, idlbp->type, TCF_NO_FLAGS)) {
              /* The other_decl template function matches the current
                 declaration. */
              idlbp->linked_symbol = fund_other_decl;
              idlbp->homonym_symbol = NULL;
              goto done;
            }  /* if */
          } else {
            /* There may be a match involving an instance of this function
               template, but we delay searching its list of instantiations
               until all normally declared functions have been seen. */
            function_template_seen = TRUE;
          }  /* if */
        } else {
          /* The listed declaration is not a template or a using-decl. */
          if (idlbp->is_function_template || locator->is_template_id) {
            /* If we are matching a template or a template instance, we cannot
               establish the match based on routine types.  That case will be
               covered by going back to the original template below. */
          } else {
            /* Compare the routine type of the current declaration with that
               of the previous declaration.  other_decl may be a namespace
               projection symbol, so fundamental_symbol_of is called. */
            check_assertion(
                  fund_other_decl->kind == (a_symbol_kind)sk_routine ||
                  fund_other_decl->kind == (a_symbol_kind)sk_member_function);
            tp = fund_other_decl->variant.routine.ptr->type;
            if (routine_types_are_compatible(tp, idlbp->type, TCF_NO_FLAGS)) {
              /* Other_decl matches the current declaration.  Null out
                 *overload_symbol in case it was set. */
              other_decl = fund_other_decl;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      if (other_decl == NULL && function_template_seen &&
          (guiding_decls_allowed ||
           locator->is_template_id ||
           locator->is_qualified_name)) {
        /* We didn't find a match, but there was at least one function
           template.  See if it either provides a match with an existing
           instance of the template or if a new instance can be created
           based on the current type.  This is done if guiding declarations
           are recognized or if the declaration is known to refer to a
           previously declared template.  The latter is the case when the
           name was specified as a qualified name or includes an explicit
           template argument list. */
        a_symbol_ptr                   sym, match = NULL;
        a_partial_order_candidate_ptr  candidates_list = NULL;
        a_symbol_ptr                   fund_other_decl;

        for (other_decl = other_decl_saved;
             other_decl != NULL;
             other_decl = is_list ? other_decl->next : NULL) {
          if (locator->is_template_id || locator->is_qualified_name) {
            fund_other_decl = fundamental_symbol_of(other_decl);
          } else {
            /* Just a possible guiding-declaration, so ignore projections of
               function templates -- they are pulled into the current scope
               by using-declarations, and so declarations in the current
               scope can't serve as guiding declarations for them. */
            fund_other_decl = other_decl;
          }  /* if */
          if (fund_other_decl->kind == (a_symbol_kind)sk_function_template) {
            /* Look for a match on the list of instantiations. */
            if (has_matching_template_function(fund_other_decl, idlbp->type,
                                               locator->template_arg_list,
                                              /*is_decl_context=*/TRUE)) {
              /* This template can generate an instance of the appropriate
                 type.  Add the matching template to a list of matching
                 candidates. */
              add_to_partial_order_candidates_list(&candidates_list,
                                                   fund_other_decl,
                                                   (a_template_arg_ptr)NULL);
            }  /* if */
          }  /* if */
        }  /* for */
        if (candidates_list != NULL) {
          /* If any of the templates matched, select the best one using
             the partial ordering rules.  If a best match cannot be selected,
             an arbitrary member of the unordered set of templates will be
             returned and the ambiguous flag will be set. */
          a_boolean		ambiguous;
          a_template_arg_ptr	templ_arg_list;
          a_symbol_ptr		best_sym;
          a_boolean             is_new_template_instance;
          select_best_partial_order_candidate(
                           candidates_list, (a_symbol_ptr)NULL, &best_sym,
                           &templ_arg_list, &ambiguous);
          /* Generate a partial instantiation of the matching instance. */
          sym = matching_template_function(best_sym, idlbp->type,
                                           locator->template_arg_list,
					   (a_boolean)locator->is_template_id,
                                           /*is_decl_context=*/TRUE,
                                           &is_new_template_instance);
          other_decl = best_sym;
          match = sym;
          idlbp->is_new_template_instance = is_new_template_instance;
          if (ambiguous) {
            /* This declaration cannot be a guiding declaration for more
               than one template function.  Issue an ambiguity error. */
            pos_syty_error(ec_ambiguous_guiding_decl,
                           &locator->source_position, sym, idlbp->type);
          }  /* if */
        }  /* if */
        if (match != NULL) {
          idlbp->linked_symbol = other_decl = match;
          is_guiding_decl = guiding_decls_allowed;
        }  /* if */
      }  /* if */
    }  /* if */
    if (decls_at_same_scope || idlbp->is_friend_decl) {
      /* The symbol was located in the current scope. If there there was an
         exact type match of C++ functions, and in general otherwise, this
         is a redeclaration, and if other_decl has linkage we can return in
         *linked_symbol a pointer to the function or variable it represents. */
      if (other_decl != NULL) {
        if (idlbp->effective_decl_level == depth_innermost_namespace_scope) {
          /* Redeclaration at file or namespace scope. */
          idlbp->linked_symbol = other_decl;
        } else if (is_function_symbol(other_decl)) {
          /* Functions always have linkage. */
          idlbp->linked_symbol = other_decl;
        } else if (!other_decl->variant.variable.ptr->
                           source_corresp.is_local_to_function) {
          /* Variables at file scope always have linkage.  Automatic,
             register, and static variables in local scopes do not. */
          idlbp->linked_symbol = other_decl;
        }  /* if */
      }  /* if */
    } else if (is_namespace_member_def) {
      idlbp->linked_symbol = other_decl;
    }  /* if */
    if (other_decl != NULL &&
        (idlbp->is_block_extern_decl || idlbp->is_local_class_friend_decl) &&
        other_decl->decl_scope != scope_stack[depth_scope_stack].number) {
      idlbp->prior_decl_in_enclosing_scope = other_decl;
    }  /* if */
    if (idlbp->linked_symbol != NULL) {
      if (idlbp->homonym_symbol == NULL) {
        /* Okay. */
      } else if (is_guiding_decl && idlbp->overload_symbol == NULL) {
        /* Special case -- return both linked_symbol and homonym_symbol, to
           form an overload set down the line. */
        check_assertion(idlbp->homonym_symbol != NULL &&
                        idlbp->homonym_symbol->kind ==
                                  (a_symbol_kind)sk_function_template);
      } else {
        idlbp->homonym_symbol = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
done:;
#if DEBUG
  if (debug_level >= 3) {
    if (idlbp->linked_symbol != NULL) {
      db_symbol(idlbp->linked_symbol, "linked symbol: ", 2);
    }  /* if */
    if (idlbp->homonym_symbol != NULL) {
      db_symbol(idlbp->homonym_symbol, "homonym symbol: ", 2);
    }  /* if */
    if (idlbp->overload_symbol != NULL) {
      db_symbol(idlbp->overload_symbol, "overload symbol: ", 2);
    }  /* if */
    if (idlbp->prior_decl_in_enclosing_scope != NULL) {
      db_symbol(idlbp->prior_decl_in_enclosing_scope,
                "prior decl in enclosing scope: ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* find_linked_symbol */


static a_boolean has_linkage_within_innermost_namespace_scope(a_symbol *sym)
/*
Given the symbol *sym representing a declaration found by find_linked_symbol,
this function returns TRUE if that prior symbol's linkage should be applied
to a newly processed symbol of the same name and type.  Called from id_linkage
only.
*/
{
  a_boolean result = FALSE;

  if (sym->decl_scope ==
                    scope_stack[depth_innermost_namespace_scope].number) {
    /* The symbol was found in namespace scope, so it should have linkage. */
    result = TRUE;
  } else if (sym->decl_scope >
                    scope_stack[depth_innermost_namespace_scope].number &&
             !(sym->kind == (a_symbol_kind)sk_variable &&
               sym->variant.variable.ptr
                                    ->source_corresp.is_local_to_function)) {
    /* The symbol was found inside the innermost namespace scope, and it is
       not a local static variable.  So it should have linkage. */
    result = TRUE;
  }  /* if */
  return result;
}  /* has_linkage_within_innermost_namespace_scope */


static void id_linkage(an_id_linkage_block  *idlbp)
/*
Determine the linkage (internal, external, or none) of the current variable,
routine, or function template declaration.  Find previous declarations with
the same name that may affect the linkage.  Return the information in the
specified id-linkage block.
*/
{
  a_boolean        is_object, is_function;
  a_symbol_ptr     prior_decl;
  a_storage_class  local_storage_class = idlbp->storage_class;
  a_boolean        is_template_instance = FALSE;
  a_boolean        is_const_variable = FALSE;

  db_enter(3, "id_linkage");
  check_assertion(local_storage_class != (a_storage_class)sc_typedef);
  /* Except in Microsoft mode, member functions cannot be redeclared (without
     being defined) outside their parent class: */
  check_assertion(idlbp->locator->specific_symbol == NULL ||
                  !idlbp->locator->specific_symbol->is_class_member ||
                  microsoft_mode);
  is_function = idlbp->func_info != NULL;
  is_object = !is_function;
  if (is_error_locator(*idlbp->locator)) {
    /* Symbol is compiler-generated as a result of an error, so there are
       no other declarations of the same symbol. */
  } else if (is_object && depth_innermost_function_scope != NO_SCOPE_DEPTH &&
             local_storage_class != (a_storage_class)sc_extern) {
    /* Local variable declaration. */
  } else if (scope_stack[decl_scope_level].kind ==
                                     (a_scope_kind)sck_func_prototype) {
    /* Function parameters have no linkage. */
  } else {
    /* Set default linkage, based on the environment of the current
       declaration but without taking into account the prior declaration. */
    if (idlbp->effective_decl_level != depth_innermost_namespace_scope &&
        !idlbp->is_block_extern_decl &&
        !idlbp->is_local_class_friend_decl) {
      /* A declaration of a local variable. */
      idlbp->linkage = idl_none;
    } else if (idlbp->storage_class == (a_storage_class)sc_static) {
      /* A declaration a non-local entity with "static" storage class. */
      idlbp->linkage = idl_internal;
    } else if (!C_mode() && is_function && !extern_inline_allowed &&
               idlbp->func_info->is_inline) {
      /* A C++ inline function.  Note that in C99 mode an inline function
         has internal linkage only if declared "static". */
      idlbp->linkage = idl_internal;
    } else if (!C_mode() && is_object &&
               is_const_qualified_type(idlbp->type) &&
               decl_scope_level == depth_innermost_namespace_scope &&
               idlbp->storage_class == (a_storage_class)sc_unspecified &&
               !(idlbp->extern_C_name_linkage_specified &&
                 idlbp->direct_linkage_specifier)) {
      /* In C++ all const qualified objects at file or namespace scope with
         no explicit storage class are internally linked (unless previously
         declared to be extern -- see below). */
      idlbp->linkage = idl_internal;
      is_const_variable = TRUE;
    } else {
      idlbp->linkage = idl_external;
    }  /* if */
    /* Find a previously declared entity with the same name and to which
       this declaration is linked. */
    find_linked_symbol(idlbp);
    prior_decl = idlbp->linked_symbol;
    if (prior_decl == NULL) {
      /* Special case for block extern declarations. */
      prior_decl = idlbp->prior_decl_in_enclosing_scope;
      if (prior_decl != NULL &&
          prior_decl->kind != (a_symbol_kind)sk_variable &&
          prior_decl->kind != (a_symbol_kind)sk_routine &&
          prior_decl->kind != (a_symbol_kind)sk_function_template &&
          prior_decl->kind != (a_symbol_kind)sk_overloaded_function) {
        /* This is a friend or block-extern declaration and a declaration
           that cannot possibly match it was found in an enclosing scope.
           It should not be considered a candidate for linked-symbol. */
        prior_decl = NULL;
      }  /* if */
    }  /* if */
#if ASM_FUNCTION_ALLOWED
    if (local_storage_class == (a_storage_class)sc_asm) {
      /* An asm function has internal linkage. */
      idlbp->linkage = idl_internal;
      prior_decl = NULL;
    }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
    if (prior_decl != NULL) {
      if (is_object == (prior_decl->kind == (a_symbol_kind)sk_variable)) {
        /* The current declaration matches the prior declaration, so the latter
           can be used to determine the linkage of the former. */
      } else {
        /* Ignore the previous declaration. */
        prior_decl = NULL;
      }  /* if */
    }  /* if */
    if (prior_decl != NULL) {
      if ((prior_decl->kind == (a_symbol_kind)sk_routine ||
           prior_decl->kind == (a_symbol_kind)sk_member_function) &&
          prior_decl->variant.routine.instance_ptr != NULL) {
        is_template_instance = TRUE;
      }  /* if */
      if (is_template_instance) {
        if (!idlbp->is_definition &&
            !routine_has_been_defined(prior_decl->variant.routine.ptr)) {
          /* A specific declaration of a function template instance for which
             a specific definition has not been seen.  The storage class of
             this declaration must agree with the storage class of the
             template. */
          a_storage_class  templ_storage_class;
          a_boolean        templ_is_inline;

          templ_storage_class = prior_decl->variant.routine.ptr->storage_class;
          templ_is_inline = prior_decl->variant.routine.ptr->is_inline;
          if (((templ_storage_class != (a_storage_class)sc_static) &&
               (local_storage_class == (a_storage_class)sc_static))) {
            /* The template was not static but the new declaration is.  Issue
               a warning, since the explicitly specified storage class will
               be ignored. */
            pos_sy_warning(ec_template_and_instance_linkage_conflict,
                           &idlbp->locator->source_position, prior_decl);
          } else if (idlbp->func_info->is_inline && !templ_is_inline) {
            /* The specific declaration is inline but the template is not.
               Issue a diagnostic because the inline specifier here will be
               disregarded. */
            pos_sy_warning(ec_incompatible_inline_specifier_on_specific_decl,
                           &idlbp->locator->source_position, prior_decl);
          }  /* if */
          if (!microsoft_mode) {
            idlbp->func_info->is_inline = templ_is_inline;
            local_storage_class = templ_storage_class;
          }  /* if */
        }  /* if */
        idlbp->storage_class = local_storage_class;
        if (local_storage_class == (a_storage_class)sc_static) {
          idlbp->linkage = idl_internal;
        } else if (!extern_inline_allowed && idlbp->func_info->is_inline) {
          idlbp->linkage = idl_internal;
        } else {
          idlbp->linkage = idl_external;
        }  /* if */
      } else if (is_const_variable &&
                 prior_decl->variant.variable.ptr->storage_class !=
                                              (a_storage_class)sc_static) {
        /* Prior declaration of this variable had external linkage, so that
           is retained. */
        idlbp->linkage = idl_external;
      } else {
        if (local_storage_class == (a_storage_class)sc_extern ||
            (is_function &&
             local_storage_class == (a_storage_class)sc_unspecified)) {
          /* An object or function with extern storage class, or a function
             with no storage class, has the same linkage as any visible
             declaration of this identifier within the enclosing namespace
             scope. */
          if (has_linkage_within_innermost_namespace_scope(prior_decl)) {
            /* There is a prior declaration in or inside the innermost
               namespace scope that is visible from here. */
            switch (prior_decl->kind) {
              case sk_routine:
                local_storage_class = prior_decl->variant.routine.ptr->
                                                               storage_class;
                break;
              case sk_variable:
                local_storage_class = prior_decl->variant.variable.ptr->
                                                                storage_class;
                break;
              case sk_function_template:
                local_storage_class = prior_decl->variant.template_info->
                                      variant.function.routine->storage_class;
                break;
#if CHECKING
              case sk_overloaded_function:
              default:
                internal_error("id_linkage: bad kind for prior_decl");
#endif /* CHECKING */
            }  /* switch */
            if (local_storage_class == (a_storage_class)sc_static) {
              /* An entity initially declared "static" is now being declared
                 without "static".  But it still has internal linkage. */
              idlbp->linkage = idl_internal;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Linkage for %s is ",
                     is_error_locator(*idlbp->locator) ?
                        "<error>" : idlbp->locator->symbol_header->identifier);
    switch (idlbp->linkage) {
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
  if (idlbp->linkage == idl_none) {
    /* If there's no linkage, be sure the linked_symbol is NULL. */
    idlbp->linked_symbol = NULL;
#if ASM_FUNCTION_ALLOWED
  } else if (idlbp->storage_class == (a_storage_class)sc_asm) {
    idlbp->name_linkage = (a_name_linkage_kind)nlk_internal;
#endif /* ASM_FUNCTION_ALLOWED */
  } else {
    /* Set the storage class to fit the linkage. */
    if (idlbp->linkage == idl_internal) {
      idlbp->storage_class = (a_storage_class)sc_static;
    } else if (idlbp->linkage == idl_external) {
      if (idlbp->is_definition) {
        idlbp->storage_class = (a_storage_class)sc_unspecified;
      } else {
        idlbp->storage_class = (a_storage_class)sc_extern;
      }  /* if */
    }  /* if */
    /* Based on the linkage that has been determined, figure out what the
       "name linkage" should be. */
    compute_name_linkage(idlbp);
  }  /* if */
  db_exit();
}  /* id_linkage */


static a_boolean incompatible_types_are_SVR4_compatible(a_type_ptr  tp1,
                                                        a_type_ptr  tp2)
/*
tp1 and tp2 are types that have already been determined to be incompatible.
However, we are in SVR4-C mode and the compatibility rules are relaxed in some
cases.  Return TRUE if the two types are compatible by these relaxed rules.
*/
{
  a_boolean   compat = FALSE;
  a_type_ptr  ret1;
  a_type_ptr  ret2;

  check_assertion(SVR4_C_mode);
  if (is_function_type(tp1)) {
    /* Two function types.  In SVR4 mode if they are incompatible solely
       because of their return types, and if the return types are "close
       enough", then consider the routine types themselves to be compatible.
       Further, if the return types are "close enough" and one type has a
       prototyped and the other a nonprototyped parameter list, also consider
       the types compatible. */
    tp1 = skip_typerefs(tp1);
    ret1 = tp1->variant.routine.return_type;
    check_assertion(is_function_type(tp2));
    tp2 = skip_typerefs(tp2);
    ret2 = tp2->variant.routine.return_type;
    if (types_are_compatible(ret1, ret2) ||
        (is_integral_or_enum_type(ret1) &&
         interchangeable_types(ret1, ret2))) {
      /* Either the return types are compatible or else they are incompatible
         but both are integral and they are interchangeable (i.e., they have
         the same size and alignment).  See whether the two routine types
         are otherwise compatible. */
      if (tp1->variant.routine.extra_info->prototyped !=
            tp2->variant.routine.extra_info->prototyped) {
        /* One of the routine types is prototyped and the other is not.
           Ignore incompatibilities (if there are any) in SVR4 mode. */
        compat = TRUE;
      } else {
        tp1->variant.routine.return_type = ret2;
        compat = types_are_compatible(tp1, tp2);
        /* Restore the original return type. */
        tp1->variant.routine.return_type = ret1;
      }  /* if */
    }  /* if */
  } else {
    check_assertion(!is_function_type(tp2));
    if (is_array_type(tp1)) {
      /* Array object types are "compatible" if they have the same element
         type. */
      tp1 = array_element_type(tp1);
      check_assertion(is_array_type(tp2));
      tp2 = array_element_type(tp2);
      compat = identical_types(tp1, tp2);
    } else {
      /* Non-array object types are "compatible" if they are
         interchangeable. */
      compat = interchangeable_types(tp1, tp2);
    }  /* if */
  }  /* if */
  return compat;
}  /* incompatible_types_are_SVR4_compatible */


static void recover_from_irreconcilable_external_symbol_types(
                                           a_type_ptr             latest_type,
                                           an_extern_symbol_descr *esdp,
                                           a_boolean              *okay)
/*
Decide the type of an IL entity and the mode with which to proceed when the
latest declaration of that entity conflicts with the previous (possibly
implicit) declaration.  latest_type is the type implied by the latest
declaration and esdp points to the relevant part of the IL entity (associated
with an sk_extern_routine or sk_extern_variable).  esdp->type is updated with
the type with which to proceed, and *okay is set to FALSE if the subsequent
processing should proceed in error mode.
*/
{
  if (!is_function_type(latest_type)) {
    /* A variable: imbue an error type for recovery. */
    esdp->type = error_type();
    *okay = FALSE;
  } else if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
    /* A routine, and the latest declaration is in file-scope: assume this
       latest declaration has the type intended by the programmer and
       proceed in error mode. */
    esdp->type = latest_type;
    *okay = FALSE;
  } else {
    /* The latest declaration is in block scope. Proceed in non-error mode
       (although a diagnostic is still emitted for this conflict): this will
       cause the type of this declaration to prevail in this scope, and that
       of the previous declaration to be restored when this scope ends. */
  }
}  /* recover_from_irreconcilable_external_symbol_types */


static void find_file_scope_decl(a_symbol_ptr  ext_sym,
                                 a_boolean     is_routine,
                                 a_boolean     *non_file_scope_decl_found,
                                 a_boolean     *file_scope_decl_found)
/*
Given an external symbol ext_sym, determine whether there's an intervening
declaration that hides an original at file scope by looping through the
symbol list (only valid in C mode).
*/
{
  a_symbol_ptr                sym = NULL;
  an_extern_symbol_descr_ptr  esdp;

  check_assertion(C_mode());
  /* The symbol header for the external symbol may be truncated and/or
     case insensitive.  We therefore need to determine the header of the
     symbol associated with an actual declaration. */
  esdp = ext_sym->variant.extern_symbol_descr;
  if (is_routine) {
    sym = (a_symbol_ptr)esdp->variant.routine.ptr->source_corresp.assoc_info;
  } else {
    sym = (a_symbol_ptr)esdp->variant.variable->source_corresp.assoc_info;
  }  /* if */
  if (sym != NULL) {
    sym = sym->header->symbol;
  }  /* if */
  for (; sym != NULL; sym = sym->next) {
    if (name_space_for_symbol_kind[(int)sym->kind] == nsk_other) {
      /* Found a symbol of the right sort. */
      if (sym->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
        if (is_tag_symbol(sym)) {
          /* Ignore a tag symbol and look for a routine or variable
             at file scope. */
        } else {
          /* Special handling of symbols encountered at file scope. */
          if ((is_routine &&
               sym->kind == (a_symbol_kind)sk_routine) ||
              (!is_routine &&
               sym->kind == (a_symbol_kind)sk_variable)) {
            *file_scope_decl_found = TRUE;
          }  /* if */
          break;
        }  /* if */
      } else if (*non_file_scope_decl_found) {
        /* We've already located an intervening declaration. */
      } else if (is_routine) {
        /* Current declaration is a routine. */
        if (sym->kind != (a_symbol_kind)sk_routine) {
          /* An intervening declaration of something other than a
             function.  This redeclaration is hidden from the original
             declaration, so issue a warning instead of an error. */
          *non_file_scope_decl_found = TRUE;
        }  /* if */
      } else {
        /* Current declaration is a variable. */
        if (sym->kind == (a_symbol_kind)sk_variable) {
          if (sym->variant.variable.ptr == NULL) {
            /* This is a symbol not yet bound to an IL entry, and so
               it is the one just now being created.  Skip past it. */
          } else if (sym->defined &&
                     sym->decl_scope !=
                            scope_stack[DEPTH_OF_FILE_SCOPE].number) {
            /* This is an intervening declaration of a local variable
               of a parameter. (We check the defined flag to rule out
               an intervening block extern declaration.) */
            *non_file_scope_decl_found = TRUE;
          }  /* if */
        } else {
          /* An intervening declaration of something other than a
             variable.  This redeclaration is hidden from the original
             declaration, so issue a warning instead of an error. */
          *non_file_scope_decl_found = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* find_file_scope_decl */


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
  a_boolean                  is_routine;
  an_error_severity          severity;
  a_symbol_ptr               sym;
  a_boolean                  compat;
  a_boolean                  incompatible_linkage_spec = FALSE;

  db_enter(4, "reconcile_external_symbol_types");
  esdp = ext_sym->variant.extern_symbol_descr;
  old_type = esdp->type;
  /* If the old and new types are the same, no checking or processing is
     required. */
  if (old_type != type_ptr) {
    /* Use a special comparison for routine types, to ignore calling
       convention differences.  In C mode, overloading is not possible, so
       allow error type mismatches on routine types. */
    is_routine = ext_sym->kind == (a_symbol_kind)sk_extern_routine;
    if (is_routine) {
      if (C_mode()) {
        compat = types_are_compatible(old_type, type_ptr);
      } else {
        compat = types_are_strictly_compatible(old_type, type_ptr);
        if (!compat &&
            routine_types_are_compatible(old_type, type_ptr, TCF_NO_FLAGS)) {
          incompatible_linkage_spec = TRUE;
        }  /* if */
      }  /* if */
    } else {
      compat = types_are_redecl_compatible(old_type, type_ptr);
    }  /* if */
    if (compat) {
      /* The old and new types are compatible.  Form the composite of
         those types, and save that as the type of the external symbol. */
      esdp->type = composite_type(old_type, type_ptr);
    } else {
      /* The old and new types are incompatible.  Issue a warning instead of
         an error for certain cases in SVR4 C compatibility mode. */
      if (SVR4_C_mode) {
        if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
          /* Functions and variables are treated differently.  For example:
               void f() { extern unsigned g(); extern int x; }
               int g();      // Warning in SVR4 C mode.
               int x;        // Error in SVR4 C mode.
          */
          if (is_routine &&
              incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
            severity = es_warning;
            /* Record the most recent type as the external symbol's type. */
            esdp->type = type_ptr;
            goto issue_diagnostic;
          }  /* if */
        } else if (is_array_type(type_ptr)) {
          /* Array types are compatible when the element types are the same
             no matter what the visibility constraints are. */
          if (incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
            severity = es_warning;
            goto issue_diagnostic;
          }  /* if */
        } else {
          /* The SVR4 algorithm is such that an error is issued if the
             incompatibility of a block extern declaration is with a visible
             declaration, but only a warning is given otherwise.  For example:
               extern int f();
               extern int ff();
               void g() { extern float f(); }                    // Error
               void gg() { int ff = 0; { extern float ff(); } }  // Warning
             Determine whether there's an intervening declaration that hides
             an original at file scope by looping through the symbol list.
             Since this only happens in C mode it is pretty straightforward. */
          a_boolean     non_file_scope_decl_found = FALSE;
          a_boolean     file_scope_decl_found = FALSE;
          find_file_scope_decl(ext_sym, is_routine, &non_file_scope_decl_found,
                               &file_scope_decl_found);
          if (non_file_scope_decl_found || !file_scope_decl_found) {
            /* Either there was no other declaration in scope (e.g., when the
               external symbol records another block extern declaration) or
               else there was an intervening declaration.  Issue a warning. */
            severity = es_warning;
            if (incompatible_types_are_SVR4_compatible(old_type, type_ptr)) {
              /* If this is a variable, record the most recent type as the
                 external symbol's type. */
              if (!is_routine) esdp->type = type_ptr;
            } else {
              okay = FALSE;
              if (file_scope_decl_found) {
                /* Retain the IL entry on the external symbol. */
              } else {
                /* Force "abandonment" of the IL entry associated with the
                   current external symbol. */
                if (is_routine) {
                  esdp->variant.routine.ptr->superseded_external = TRUE;
                  esdp->variant.routine.ptr = NULL;
                } else {
                  check_assertion_str2(
                             (esdp->variant.variable->storage_class ==
                                             (a_storage_class)sc_extern) &&
                             (esdp->variant.variable->init_kind ==
                                             (an_init_kind)initk_none),
                             "reconcile_external_symbol_types:",
                             "can't set superseded_external");
                  esdp->variant.variable->superseded_external = TRUE;
                  esdp->variant.variable = NULL;
                }  /* if */
                esdp->type = type_ptr;
              }  /* if */
            }  /* if */
            goto issue_diagnostic;
          }  /* if */
        }  /* if */
      } else if (microsoft_mode && is_routine) {
        if (C_mode()) {
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            sym = NULL;
          } else {
            for (sym = ext_sym->header->symbol; sym != NULL; sym = sym->next) {
              if (sym->kind == (a_symbol_kind)sk_routine &&
                  sym->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          if (sym != NULL) {
            /* The current declaration is a block extern declaration and
               there has been a file-scope declaration of a function with the
               same name.  Don't reset the external symbol. */
          } else {
            /* Force "abandonment" of the IL entry associated with the external
               routine. */
            esdp->variant.routine.ptr->superseded_external = TRUE;
            esdp->variant.routine.ptr = NULL;
          }  /* if */
          okay = FALSE;
        } else if (!incompatible_linkage_spec) {
          /* In Microsoft C++ mode extern "C" routine declarations are
             allowed to have incompatible types when they appear in
             different namespaces. */
          severity = es_warning;
          goto issue_diagnostic;
        }  /* if */
      }  /* if */
      severity = es_error;
      /* Decide how to proceed and which type to select for recovery: */
      recover_from_irreconcilable_external_symbol_types(type_ptr, esdp, &okay);
issue_diagnostic:
      /* The old and new types are incompatible.  Error. */
      if (!suppress_incompatible_error) {
        pos_sy_diagnostic(severity,
                          incompatible_linkage_spec ?
                            ec_incompatible_linkage_specifier :
                            ec_decl_incompatible_with_previous_use,
                          position, ext_sym);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* reconcile_external_symbol_types */


static a_symbol_ptr create_external_symbol_for_linked_entity(
                            a_symbol_locator       *locator,
                            a_type_ptr             type_ptr,
                            a_name_linkage_kind    name_linkage,
                            a_func_info_block_ptr  func_info,
                            a_boolean              redeclaration,
                            a_boolean              suppress_incompatible_error,
                            a_boolean              suppress_ext_sym_lookup,
                            a_variable_ptr         *variable_ptr,
                            a_routine_ptr          *routine_ptr)
/*
Find or create an external symbol entry for a variable or routine being
declared.  *locator gives the symbol locator for the identifier;
is_function is TRUE for a function, FALSE for a variable; type_ptr
gives the variable or routine type; name_linkage indicates the linkage
(internal, external, C++ external).  Aside from creating the
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
  a_boolean                  is_implicit_declaration;
  a_boolean                  is_function;

  db_enter(4, "create_external_symbol_for_linked_entity");
  if (func_info != NULL) {
    check_assertion(is_function_type(type_ptr));
    is_function = TRUE;
    is_implicit_declaration = func_info->is_implicit_declaration;
    ext_sym_kind = (a_symbol_kind)sk_extern_routine;
  } else {
    is_function = FALSE;
    ext_sym_kind = (a_symbol_kind)sk_extern_variable;
  }  /* if */
  if (is_error_locator(*locator)) err = TRUE;
  if (suppress_ext_sym_lookup || err) {
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
        old_name = esdp->variant.routine.ptr->source_corresp.name;
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
        /* Both are variables, or both are routines.  Compare the old and
           new types; they must be compatible. */
        err = !reconcile_external_symbol_types(ext_sym,
                                               &locator->source_position,
                                               type_ptr,
                                               suppress_incompatible_error);
        if (ext_sym_kind == (a_symbol_kind)sk_extern_routine) {
          /* If this declaration is not the result of an implicit
             declaration, clear the is_implicit_declaration flag in the
             external symbol entry. */
          if (!is_implicit_declaration) {
            esdp->variant.routine.is_implicit_declaration = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_function && !err) {
    if (!C_mode() && name_linkage == (a_name_linkage_kind)nlk_external &&
        !func_info->is_main_function) {
      /* This is an extern "C" function declaration in C++.  Be sure no other
         extern "C" function has been declared in this translation unit --
         only one is permitted with a given name, ignoring namespaces.
         (Microsoft compilers ignore this, so in Microsoft bugs mode we
         weaken this to a warning.) */
      a_symbol_ptr   sym = locator->symbol_header->other_symbols;
      a_routine_ptr  rp;

      for (; sym != NULL; sym = sym->next) {
        if (sym->kind == (a_symbol_kind)sk_extern_routine) {
          /* Ignore symbols not associated with the current file scope.  These
             could be extern entities associated with other translation
             units. */
          if (sym->decl_scope != file_scope_number) continue;
          rp = sym->variant.extern_symbol_descr->variant.routine.ptr;
          if (rp->source_corresp.name_linkage ==
                                     (a_name_linkage_kind)nlk_external &&
              !routine_types_are_compatible(
                                          type_ptr, rp->type, TCF_NO_FLAGS)) {
            /* Illegal overloading involving two extern "C" functions with
               the same name.  Microsoft compilers let this through if the
               two declarations are in different namespaces. */
            err = !(microsoft_bugs &&
                    depth_scope_stack == depth_innermost_namespace_scope &&
                    sym->parent.namespace_ptr !=
                              scope_stack[depth_scope_stack].assoc_namespace);
            pos_sy_diagnostic(err ? es_error : es_warning,
                              ec_overloaded_function_linkage,
                              &locator->source_position, sym);
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (ext_sym == NULL) {
    /* There is no (compatible) external symbol entry for the identifier.
       Create one. */
    ext_sym = enter_extern_symbol(ext_sym_kind, &ext_locator);
    esdp = ext_sym->variant.extern_symbol_descr;
    esdp->type = type_ptr;
    if (ext_sym_kind == (a_symbol_kind)sk_extern_routine) {
      esdp->variant.routine.is_implicit_declaration = is_implicit_declaration;
    }  /* if */
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
        *routine_ptr =
                    ext_sym->variant.extern_symbol_descr->variant.routine.ptr;
        if (*routine_ptr != NULL && !C_mode()) {
          a_symbol_ptr  rout_sym = (a_symbol_ptr)(*routine_ptr)->
                                                    source_corresp.assoc_info;
          if (func_info->is_definition &&
              routine_has_been_defined(*routine_ptr)) {
            /* This error can come up when the same extern "C" function is
               defined in two different namespaces -- e.g.,
                 namespace N { extern "C" void f() { } }
                 namespace M { extern "C" void f() { } }
            */
            pos_sy_error(ec_already_defined, &locator->source_position,
                         rout_sym);
            *routine_ptr = NULL;
          } else {
            /* Do compatibility checking on the throw specification. */
            check_exception_specification(type_ptr, rout_sym,
                                          &func_info->throw_position,
                                          /*is_redecl=*/TRUE);
          }  /* if */
        }  /* if */
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
           Another example involves local typedefs:
             int *p;
             void f() {
               typedef int *IP;
               extern IP p;
             }  // "p" cannot have type "IP" in file scope; restore "int*"
        */
        /* The fixup is only needed if the pre-existing definition is in
           a scope that surrounds the current one.  That's hard to determine,
           since it's hard to know the scope associated with the previous
           definition (the associated symbol gives only one scope, and
           not necessarily the outermost).  A simple way out is to build
           the fixup whenever the current scope is not the file scope.
           That builds more fixups than needed, but it always works. */
        if (C_dialect == C_dialect_pcc &&
            !identical_types(type_ptr, preexisting_type)) {
          /* In pcc mode all symbols with linkage are entered at the file
             scope, so a fixup is never needed.  However, that leaves the
             possibility that the external entity is typed with a function
             scope typedef (see example above).  To reduce the occurrence
             of such unaesthetic situations, a fixup is applied if the
             types are otherwise identical. */
        } else if (decl_scope_level != depth_innermost_namespace_scope) {
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
  db_exit();
  return ext_sym;
}  /* create_external_symbol_for_linked_entity */

#if DECL_MODIFIERS_IN_USE
#if !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* is_redecl and is_definition are only used in Microsoft
                  mode. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
void update_routine_decl_modifiers(a_routine_ptr               routine,
                                   a_decl_modifiers_block_ptr  new_modifiers,
                                   a_source_position           *position,
                                   a_boolean                   is_redecl,
                                   a_boolean                   is_definition,
                                   a_boolean                   is_inline)
/*
Update the decl_modifiers field of the routine entry to reflect the
modifiers specified in new_modifiers.  If this is a redeclaration or
definition of a previously defined routine, make sure that the new
modifiers are consistent with the previous declaration specified
by routine.  position is used as the error position for any
diagnostics.
*/
{
  a_boolean        any_invalid_redecl, invalid_modifier, invalid_redecl;
  int              bit_number;
  a_decl_modifier  modifier_value;
  a_boolean        implicit_dllexport = FALSE;

  /* Loop through the bits in the new_modifiers bit vector and process the
     modifiers associated with the bits that are set. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (routine->is_inline) is_inline = TRUE;
  implicit_dllexport = is_definition && is_redecl && !is_inline &&
                       routine->decl_modifiers & (DM_DLLIMPORT | DM_DLLEXPORT);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (new_modifiers->flags != DM_NONE || implicit_dllexport) {
    any_invalid_redecl = FALSE;
    for (bit_number = 0; bit_number < (int)dmt_last; ++bit_number) {
      modifier_value = (1 << bit_number);
      if ((new_modifiers->flags & modifier_value) != 0
#if MICROSOFT_EXTENSIONS_ALLOWED
          || (implicit_dllexport && bit_number == (int)dmt_dllexport)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                     ) {
        /* This bit is set -- or, if this is a non-inline function definition,
           pretend the dllexport bit is set. */
        invalid_modifier = FALSE;
        invalid_redecl = FALSE;
        switch (bit_number) {
#if MICROSOFT_EXTENSIONS_ALLOWED
          case dmt_dllimport:
            if (is_definition && !is_inline) {
              /* This is an error.  A function with dllimport specified on its
                 definition has to be inline. */
              invalid_modifier = TRUE;
              break;
            }  /* if */
            /*FALLTHROUGH*/
          case dmt_dllexport:
            if (is_redecl) {
              if (!(routine->decl_modifiers & (DM_DLLIMPORT | DM_DLLEXPORT))) {
                /* Any previous declaration should have been declared
                   with either dllimport or dllexport.  Issue a warning. */
                invalid_redecl = TRUE;
              } else if (!((DM_DLLIMPORT | DM_DLLEXPORT) &
                           routine->decl_modifiers &
                           new_modifiers->flags)) {
                /* The current declaration is inconsistent with a previous
                   declaration.  Issue a warning and clear the previous
                   dllimport/dllexport state. */
                invalid_redecl = TRUE;
                routine->decl_modifiers &= ~(DM_DLLIMPORT | DM_DLLEXPORT);
              }  /* if */
            }  /* if */
            break;
          case dmt_naked:
            if (!is_definition) {
              invalid_modifier = TRUE;
              new_modifiers->flags &= (~modifier_value);
            }  /* if */
            break;
          case dmt_microsoft_inline:
          case dmt_forceinline:
          case dmt_nothrow:
          case dmt_noreturn:
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          default:
            invalid_modifier = TRUE;
            new_modifiers->flags &= (~modifier_value);
            break;
        }  /* switch */
        if (invalid_modifier) {
          pos_st_diagnostic(es_discretionary_error,
                            ec_decl_modifiers_invalid_for_this_decl,
                            position, decl_modifier_names[bit_number]);
        }  /* if */
        any_invalid_redecl |= invalid_redecl;
      }  /* if */
    }  /* for */
    if (any_invalid_redecl) {
      pos_warning(ec_decl_modifiers_incompatible_with_previous_decl, position);
    }  /* if */
    /* Update the routine entry with any valid modifiers that were found. */
    routine->decl_modifiers |= new_modifiers->flags;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (new_modifiers->allocate_segname != NULL) {
    /* Only allowed for variables with static storage duration. */
    pos_error(ec_declspec_allocate_not_allowed, position);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* update_routine_decl_modifiers */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* is_redecl is only used in Microsoft mode. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
void update_variable_decl_modifiers(a_variable_ptr              variable,
                                    a_decl_modifiers_block_ptr  new_modifiers,
                                    a_source_position           *position,
                                    a_boolean                   is_redecl)
/*
Update the decl_modifiers field of the variable entry to reflect the
modifiers specified in new_modifiers.  If this is a redeclaration or a
definition of a previously declared variable, make sure that the new
modifiers are consistent with the previous declaration specified
by variable.  position is used as the error position for any
diagnostics.  is_redecl is TRUE if this is a redeclaration.
*/
{
  a_boolean	   any_invalid_redecl = FALSE;
  a_boolean        invalid_modifier, invalid_redecl;
  int		   bit_number;
  a_decl_modifier  modifier_value;

  /* Loop through the bits of the new_modifiers bit vector and process
     the modifiers associated with the bits that are set. */
  if (new_modifiers->flags != DM_NONE) {
    for (bit_number = 0; bit_number < (int)dmt_last; ++bit_number) {
      modifier_value = (1 << bit_number);
      if ((new_modifiers->flags & modifier_value) != 0) {
        /* This bit is set. */
        invalid_modifier = FALSE;
        invalid_redecl = FALSE;
        switch (bit_number) {
#if MICROSOFT_EXTENSIONS_ALLOWED
          case dmt_dllimport:
          case dmt_dllexport:
            /* Any previous declaration must have been declared
               with either dllimport or dllexport. */
            if (is_redecl &&
                !(variable->decl_modifiers & (DM_DLLIMPORT | DM_DLLEXPORT))) {
              invalid_redecl = TRUE;
            }  /* if */
            break;
          case dmt_thread:
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          default:
            invalid_modifier = TRUE;
            break;
        }  /* switch */
        /* If this modifier is invalid, reset the bit in the new modifiers. */
        if (invalid_modifier || invalid_redecl) {
          new_modifiers->flags &= (~modifier_value);
        }  /* if */
        if (invalid_modifier) {
          pos_st_diagnostic(es_discretionary_error,
                            ec_decl_modifiers_invalid_for_this_decl,
                            position, decl_modifier_names[bit_number]);
        }  /* if */
        any_invalid_redecl |= invalid_redecl;
      }  /* if */
    }  /* for */
    /* Update the variable entry with any valid modifiers that were found. */
    variable->decl_modifiers |= new_modifiers->flags;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (new_modifiers->allocate_segname != NULL) {
    /* __declspec(allocate(...)) has been specified. */
    if (!has_static_storage_duration(variable->storage_class)) {
      /* Only allowed for variables with static storage duration. */
      pos_error(ec_declspec_allocate_not_allowed, position);
    } else if (variable->allocate_segname != NULL) {
      if (strcmp(variable->allocate_segname,
                 new_modifiers->allocate_segname) == 0) {
        /* Redeclaration of same segment name. */
      } else {
        /* Error. */
        any_invalid_redecl = TRUE;
      }  /* if */
    } else {
      /* Copy the declared data segment name to the variable. */
      variable->allocate_segname = new_modifiers->allocate_segname;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (any_invalid_redecl) {
    pos_diagnostic(es_discretionary_error,
                   ec_decl_modifiers_incompatible_with_previous_decl,
                   position);
  }  /* if */
}  /* update_variable_decl_modifiers */

#endif /* DECL_MODIFIERS_IN_USE */

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


void check_default_args_for_param_type(a_param_type_ptr  ptp,
                                       a_source_position *pos)
/*
Given a param type pointer, make sure that any parameter with a default
argument value is followed only by other parameters with defaults.
Issue an error if a default argument that is followed by a parameter
without a default is found.
*/
{
  /* Loop through the single list. */
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg && ptp->next != NULL &&
        !ptp->next->has_default_arg) {
      /* Current parameter has a default argument and its successor does
         not.  Report the error and break out of the loop. */
      pos_error(ec_default_arg_not_at_end, pos);
      break;
    }  /* if */
  }  /* for */
}  /* check_default_args_for_param_type */


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
  check_default_args_for_param_type(ptp, &error_position);
}  /* check_default_args */


static void check_for_any_default_args(a_type_ptr type)
/*
Check whether the routine type pointer specified by type contains any
default arguments.  Issue an error if any are found.
*/
{
  a_param_type_ptr  ptp;

  /* Look for a param type entry with a default argument. */
  ptp = skip_typerefs(type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg) {
      pos_diagnostic(es_discretionary_error, ec_default_arg_expr_not_allowed,
                     &error_position);
      break;
    }  /* if */
  }  /* for */
}  /* check_for_any_default_args */


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


void check_old_specialization_allowed(a_symbol_ptr       sym,
                                      a_source_position  *pos)
/*
Issue a discretionary error if old-style template specializations are not
allowed.  sym is the instance symbol, pos the error position.
*/
{
  an_error_code  code;

  if (!old_specializations_allowed) {
    if (strict_ansi_mode) {
      /* Old-style template specialization is nonstandard. */
      code = ec_nonstd_old_specialization;
    } else {
      /* Old-style template specialization is not allowed. */
      code = ec_old_specialization_not_allowed;
    }  /* if */
    pos_sy_diagnostic(es_discretionary_error, code, pos, sym);
  }  /* if */
}  /* check_old_specialization_allowed */


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
  a_routine_type_supplement_ptr
                    rtsp, comp_rtsp;
  a_boolean         preserve_qualifiers_from_rout_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_calling_convention
                    orig_calling_convention =
                         skip_typerefs(rout_type)->variant.routine.extra_info->
                                                            calling_convention;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "reconcile_routine_types");
  if (rout_type != type_ptr) {
    /* We only try to reconcile routine types that have already been
       determined to be compatible. */
    check_assertion_str(routine_types_are_compatible(type_ptr, rout_type,
                                      TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING),
                        "reconcile_routine_types: types are not compatible");
    /* We cannot be required to preserve the types from both sources. */
    check_assertion_str(!preserve_rout_type || !preserve_type_ptr,
                        "reconcile_routine_types: can't preserve both types");
    if (C_dialect == C_dialect_cplusplus) {
      /* If there are default arguments associated with the parameters, check
         them at this time.  They will be merged in composite_type. */
      check_default_arg_compatibility(type_ptr, rout_type);
    }  /* if */
    /* The type of the routine should be the composite of the two types. */
    if (!preserve_rout_type && !preserve_type_ptr) {
      /* Simple case -- no required result type location.  Normally, we prefer
         the original declaration, but if that was compiler-generated, we
         prefer the later one (presumably declared in the source). */
      if (routine_ptr->compiler_generated) {
        routine_ptr->type = composite_type(type_ptr, rout_type);
      } else {
        routine_ptr->type = composite_type(rout_type, type_ptr);
      }  /* if */
    } else {
      /* Some requirement on where the result ends up.  Favor the type we'd
         like by passing it first to composite_type. */
      if (preserve_rout_type) {
        /* rout_type must be preserved. */
        comp_type = composite_type(rout_type, type_ptr);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (comp_type != rout_type &&
            routine_ptr->declared_type == rout_type) {
          /* The declared type, which was saved when the routine was defined,
             points to a type entry that is going to be modified, so change
             it to point to a copy. */
          routine_ptr->declared_type =
                copy_routine_type_with_param_types(rout_type,
                                                   /*copy_default_args=*/TRUE);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      } else {
        /* type_ptr must be preserved. */
        comp_type = composite_type(type_ptr, rout_type);
        routine_ptr->type = rout_type = type_ptr;
      }  /* if */
      /* If rout_type is not what was returned, copy the composite
         type on top of the existing rout_type. */
      if (comp_type != rout_type) {
        comp_type = skip_typerefs(comp_type);
        comp_rtsp = comp_type->variant.routine.extra_info;
        rout_type = skip_typerefs(rout_type);
        rtsp = rout_type->variant.routine.extra_info;
        /* Transfer the composite type to rout_type, which is usually
           unshared.  We want to preserve fields like assoc_routine and
           arg_pragma in rout_type, so we can't just do a copy_type. */
        rout_type->variant.routine.return_type =
                            comp_type->variant.routine.return_type;
        rtsp->prototyped = comp_rtsp->prototyped;
        preserve_qualifiers_from_rout_type = FALSE;
        if (rtsp->param_type_list == NULL) {
          /* The entire list may just be transferred over. */
          rtsp->param_type_list = comp_rtsp->param_type_list;
        } else if (rtsp->param_type_list == comp_rtsp->param_type_list) {
          /* This is a fairly rare case in C mode: the routine was declared
             twice without prototype, but the latter declaration is a
             definition.  In that case, composite_routine_type will have
             shared the param_type_list between the composed types.
             Nothing needs to be done. */
          check_assertion_str2(C_mode(), "reconcile_routine_types:",
                                         "shared param types unexpected");

        } else {
          /* Copy the param type entries from the composite type onto the
             param type entries for the routine type.  This is done in case
             new param type entries were created.  The original ones must be
             preserved, however, since they may be pointed to by the parameter
             variables with which they are associated. */
          rout_type_ptp = rtsp->param_type_list;
          comp_type_ptp = comp_rtsp->param_type_list;
          if (remove_qualifiers_from_param_types) {
            /* Usually, the top-level param-type qualifiers recorded for the
               function (either as currently declared or as previously
               declared when the function was defined) should be preserved.
               Since composite_type cannot be presumed to have gotten it
               right, the qualifiers will have to be copied into the new
               type by hand. */
            if (!preserve_rout_type ||
                routine_ptr->assoc_scope != NULL_region_number) {
              preserve_qualifiers_from_rout_type = TRUE;
            }  /* if */
          }  /* if */
          for (; rout_type_ptp != NULL; rout_type_ptp = next_rout_type_ptp,
                                        comp_type_ptp = comp_type_ptp->next) {
            a_type_qualifier_set  saved_qualifiers = rout_type_ptp->qualifiers;
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
            char *saved_name = rout_type_ptp->name;
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
#if EXTRA_SOURCE_POSITIONS_IN_IL
            a_decl_position_supplement_ptr saved_decl_pos_info =
                                                rout_type_ptp->decl_pos_info;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            check_assertion_str2(rout_type_ptp != comp_type_ptp,
                                 "reconcile_routine_types:",
                                 "param type appears on two lists");
            /* Save the original next pointer and restore it after the copy. */
            next_rout_type_ptp = rout_type_ptp->next;
            /* First copy the param type entry, and then update the fields
               that need to be restored. */
            *rout_type_ptp = *comp_type_ptp;
            /* Restore the next pointer. */
            rout_type_ptp->next = next_rout_type_ptp;
            if (preserve_qualifiers_from_rout_type) {
              /* Restore the qualifiers as originally declared. */
              rout_type_ptp->qualifiers = saved_qualifiers;
            }  /* if */
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
            /* Restore the name that is associated with the routine type. */
            rout_type_ptp->name = saved_name;
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
#if EXTRA_SOURCE_POSITIONS_IN_IL
            /* Restore the source-range information that was recorded for
               the routine type. */
            rout_type_ptp->decl_pos_info = saved_decl_pos_info;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          }  /* for */
        }  /* if */
        if (!C_mode()) {
          if (exceptions_enabled) {
            /* Preserve the exception specification -- the pointer will have
               been copied into comp_type by composite_type.  (Note that we
               just copy the pointer, so that two routine types may end up
               pointing to the same exception specification entry.  This
               should be okay.) */
            rtsp->exception_specification = comp_rtsp->exception_specification;
          }  /* if */
          rtsp->routine_name_linkage = comp_rtsp->routine_name_linkage;
          rtsp->routine_name_linkage_is_explicit =
                           comp_rtsp->routine_name_linkage_is_explicit;
        }  /* if */          
        /* has_ellipsis need not be copied -- it will be the same in all of
           the types, since the original two types are compatible. */
        /* Likewise, the this_class pointers should be identical -- this will
           have been verified in types_are_compatible. */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The Microsoft Visual C++ compiler always uses the calling convention
       from the declaration of a member function, even if the calling
       convention on the definition is different. */
    if (microsoft_mode &&
        routine_ptr->source_corresp.is_class_member) {
      skip_typerefs(routine_ptr->type)->variant.routine.extra_info->
                                  calling_convention = orig_calling_convention;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  db_exit();
}  /* reconcile_routine_types */


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


static a_boolean routine_name_linkages_are_compatible(a_type_ptr  rout_type,
                                                      a_type_ptr  type_ptr)
/*
rout_type is a pointer to the type of a previous declaration of a given
routine, and type_ptr is a pointer to the current type.  Return TRUE if
the routine-name-linkages of the two declarations are compatible.
*/
{
  a_boolean                      compat = TRUE;
  a_routine_type_supplement_ptr  rtsp;

  type_ptr = skip_typerefs(type_ptr);
  rtsp = type_ptr->variant.routine.extra_info;
  if (rtsp->routine_name_linkage_is_explicit) {
    rout_type = skip_typerefs(rout_type);
    compat = routine_linkages_are_compatible(
                  rtsp->routine_name_linkage,
                  rout_type->variant.routine.extra_info->routine_name_linkage,
                  /*is_impl_conv=*/FALSE);
  }  /* if */
  return compat;
}  /* routine_name_linkages_are_compatible */


static void check_constituent_types_have_linkage(a_symbol_ptr      sym,
                                                 a_source_position *error_pos)
/*
If a variable or routine is to have linkage, its type (and any types that
that type is made up of) should have linkage as well. This excludes local
types and certain typedef types.  sym is a pointer to a routine or variable
symbol whose type is to be verified.  error_pos determines where any error
should be reported.
*/
{
  a_boolean  is_function = (sym->kind == (a_symbol_kind)sk_routine);
  a_type_ptr type = is_function? sym->variant.routine.ptr->type :
                                 sym->variant.variable.ptr->type;
  if (is_or_contains_local_type(type)) {
    /* A block extern declaration that involves a local type.  Issue an
       error (except in cfront or Microsoft compatibility mode). */
    pos_diagnostic((any_cfront_mode() || microsoft_mode) ? es_warning :
                                                           es_error,
                   is_function ? ec_local_type_in_function :
                                 ec_local_type_in_nonlocal_var,
                   error_pos);
  } else if (contains_type_with_no_name_linkage(type)) {
    /* Catch the use of typedefs that do not have linkage.
       E.g., typedef enum { e1 } *pE; void f(pE); */
    pos_diagnostic(strict_ansi_mode? es_error: es_warning,
                   is_function ? ec_type_with_no_linkage_in_function :
                                 ec_type_with_no_linkage_in_var_with_linkage,
                   error_pos);
  }  /* if */
}  /* check_constituent_types_have_linkage */


static void set_name_linkage(an_id_linkage_block     *idlbp,
                             a_symbol_ptr            sym,
                             a_source_correspondence *scp,
                             a_symbol_ptr            ext_sym,
                             a_source_position       *error_pos)
/*
Called from decl_variable and decl_routine, this function sets the name
linkage of the IL entry.  The indicated id linkage block indicates the
linkage (internal, external, none) that has been assigned.  sym is the symbol
for the variable or routine whose name linkage is to be set, and scp points
to the source correspondence of the associated IL entry.  ext_sym is the
associated sk_external_variable or sk_external_routine symbol, if any.
*error_pos is the source position of the identifier.
*/
{
  a_boolean                is_function =
                                   (sym->kind == (a_symbol_kind)sk_routine);
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_boolean                err;

  if (idlbp->linkage != idl_none) {
    if (scp->name_linkage == (a_name_linkage_kind)nlk_none) {
      scp->name_linkage = idlbp->name_linkage;
      sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
      if (ext_sym != NULL) {
        ext_sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
      }  /* if */
    } else {
      err = FALSE;
      if (scp->name_linkage == idlbp->name_linkage) {
        /* The linkage kinds (C or C++) are the same; however, the ARM states,
           "A function declaration without a linkage specification may not
           precede the first linkage specification for that function." */
        if (idlbp->name_linkage_is_explicit) {
          err = (scp->name_linkage !=
                          (a_name_linkage_kind)nlk_cplusplus_external &&
                 !sym->explicit_linkage_specifier &&
                 !(ext_sym != NULL && ext_sym->explicit_linkage_specifier));
          /* Mark the symbols as having an explicit linkage specifier to
             keep this error from occurring again later. */
          sym->explicit_linkage_specifier = TRUE;
          if (ext_sym != NULL) ext_sym->explicit_linkage_specifier = TRUE;
        }  /* if */
      } else {
        /* Linkage is not the same, but it's no error as long as the current
           specification is implicit. */
        err = ssep->name_linkage_is_explicit;
        /* Reset the name linkage in certain cases: when the current
           linkage was explicitly specified whereas the previous one was not,
           or when one of the declarations specified internal linkage and the
           other didn't (in which case the later declaration is favored). */
        if ((idlbp->name_linkage_is_explicit &&
             !sym->explicit_linkage_specifier) ||
            scp->name_linkage == (a_name_linkage_kind)nlk_internal ||
            idlbp->name_linkage == (a_name_linkage_kind)nlk_internal) {
          scp->name_linkage = idlbp->name_linkage;
          sym->explicit_linkage_specifier = idlbp->name_linkage_is_explicit;
          if (ext_sym != NULL && idlbp->name_linkage_is_explicit) {
            ext_sym->explicit_linkage_specifier = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (err) {
        /* Neither functions nor variables are supposed to have inconsistent
           linkage specifications, but it's more of a problem for functions.
           Issue an error for functions, a warning for variables. */
        pos_sy_diagnostic(is_function ? (an_error_severity)es_error :
                                          (strict_ansi_mode ?
                                              strict_ansi_error_severity :
                                              (an_error_severity)es_warning),
                          ec_incompatible_linkage_specifier, error_pos,
                          ext_sym == NULL ? idlbp->linked_symbol : ext_sym);
      }  /* if */
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* A variable or routine with linkage should not be declared in terms of
         types with no linkage. */
      check_constituent_types_have_linkage(sym, error_pos);
    }  /* if */
  }  /* if */
}  /* set_name_linkage */


static void add_namespace_parent_pointer(a_symbol_ptr             sym,
                                         a_source_correspondence  *scp)
/*
If appropriate, set the namespace pointer in the symbol (unless this is a
block-extern declaration) and in the IL entry (unless this is an extern "C"
declaration).
*/
{
  a_namespace_ptr  ns_ptr;

  ns_ptr = scope_stack[depth_innermost_namespace_scope].il_scope->
                                                variant.assoc_namespace;
  check_assertion(ns_ptr != NULL);
  if (scope_stack[depth_scope_stack].default_name_linkage ==
                                        (a_name_linkage_kind)nlk_external) {
    /* This is an extern "C" context. */
    if (scp->assoc_info != (char *)sym) {
      /* This entity was originally declared in another scope and then
         redeclared in the current namespace -- e.g.,
           extern "C" void f();
           namespace N {
             extern "C" void f();    // same entity
           }
         Don't reset the namespace parent pointer (which may or may not be
         non-NULL). */
      scp = NULL;
    }  /* if */
  }  /* if */
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    /* This is a block-extern declaration, so the symbol is not set. */
    sym = NULL;
  }  /* if */
  set_namespace_membership(sym, scp, ns_ptr);
}  /* add_namespace_parent_pointer */


static void qualified_name_redecl_sym(an_id_linkage_block  *idlbp)
/*
This routine is called from decl_variable and decl_routine for either
of two cases: (1) when is_friend_decl is FALSE, a namespace-qualified
identifier is being redeclared outside of the namespace of which it is
a member -- this will be a definition in a well-formed program; and
(2) when is_friend_decl is TRUE, a namespace- or file-scope-qualified
name is being declared in a friend declaration.  *locator will point
to a specific symbol, as well as to a namespace parent in most cases.
effective_decl_level will have computed by the caller.  type_ptr is
the declared type, used for looking up symbols in an overload set.
is_definition is usually TRUE unless is_friend_decl is TRUE.
templ_param_list points to the template parameter list when the entity
declared is a function template.  When the symbol is a member of an
overloaded function set, *overload_symbol is returned with a pointer
to the sk_overloaded_function symbol.  *linkage is returned with a
value reflecting the linkage of the original symbol.
*namespace_reactivated is returned TRUE when the caller needs to pop
the namespace scope, which for friend declarations is a
namespace-reactivation scope and for other declarations is a
namespace-extension scope.
*/
{
  a_symbol_ptr     linked_symbol;
  a_boolean        err = FALSE;
  a_storage_class  storage_class;
  a_symbol_locator *locator = idlbp->locator;
  a_namespace_ptr  nsp = qualifier_namespace_ptr(*locator);
  a_scope_depth    orig_effective_decl_level;

  db_enter(3, "qualified_name_redecl_sym");
  if (!idlbp->is_definition && !idlbp->is_friend_decl && !microsoft_mode) {
    /* Improper use of a qualified name in a declarator (WP 8.3).  This
       is permitted in Microsoft mode. */
    sym_error(ec_bad_scope_for_redeclaration, locator->specific_symbol);
    err = TRUE;
  } else if (idlbp->is_definition && !locator->is_class_member &&
             !namespace_is_enclosed_by_scope(locator->specific_symbol,
                                             &scope_stack[idlbp->
                                                     effective_decl_level])) {
    /* This declaration appears within a namespace scope in which the name
       cannot be defined -- it is a member (directly or indirectly) of a
       namespace that is not enclosed by the current namespace scope
       (see WP 7.3.1.4). */
    sym_error(ec_bad_scope_for_definition, locator->specific_symbol);
    err = TRUE;
  } else {
    /* This is a valid location for such a declaration. */
    if (scope_stack[decl_scope_level].kind ==
                                 (a_scope_kind)sck_template_declaration &&
        is_function_type(idlbp->type)) {
      idlbp->is_function_template = TRUE;
    }  /* if */
    if (nsp != NULL) {
      if (idlbp->is_friend_decl) {
        /* Push a namespace-reactivation scope scope for friend declarations
           (the scope is "read-only" -- no injections allowed), and a
           namespace-extension scope otherwise. */
        push_namespace_reactivation_scope(nsp);
      } else {
        /* This is a definition of a namespace member appearing
           in a scope other than that of the namespace to which it belongs, so
           extend the original namespace scope. */
        push_namespace_extension_scope(nsp);
        orig_effective_decl_level = idlbp->effective_decl_level;
        idlbp->effective_decl_level = depth_scope_stack;
      }  /* if */
      idlbp->namespace_reactivated = TRUE;
    } else {
      /* Must be a file scope qualified name or a template-id. */
      check_assertion(locator->is_file_scope_qualified_name ||
                      locator->is_template_id);
    }  /* if */
    /* Look up the name. */
    find_linked_symbol(idlbp);
    linked_symbol = idlbp->linked_symbol;
    storage_class = idlbp->storage_class;
    if (linked_symbol != NULL &&
        linked_symbol->kind != (a_symbol_kind)sk_overloaded_function) {
      /* A linked symbol was found -- set the storage class and linkage
         appropriately. */
      switch (linked_symbol->kind) {
        case sk_routine:
          storage_class = linked_symbol->variant.routine.ptr->storage_class;
          break;
        case sk_function_template:
          storage_class = linked_symbol->variant.template_info->
                                      variant.function.routine->storage_class;
          break;
        case sk_variable:
          storage_class = linked_symbol->variant.variable.ptr->storage_class;
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      if (storage_class == (a_storage_class)sc_static) {
        idlbp->linkage = idl_internal;
        idlbp->storage_class = (a_storage_class)sc_static;
      } else {
        idlbp->linkage = idl_external;
        if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
            idlbp->is_definition) {
          idlbp->storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      if (!idlbp->is_friend_decl && !microsoft_mode &&
          linked_symbol->kind == (a_symbol_kind)sk_routine &&
          linked_symbol->variant.routine.instance_ptr != NULL) {
        /* This is an out-of-scope definition of a namespace template
           function.  Be sure there is a prior declaration -- i.e., that
           this was not just instantiated by a reference or as a result of
           this very declaration (i.e., in the call to find_linked_symbol):
             namespace N {
               template <class T> void f(T);
               void f(int);
             }
             void N::f(int) { ... }          // Okay
             void N::f(double) { ... }       // Error
        */
        if (guiding_decls_allowed) {
          if (!linked_symbol->variant.routine.instance_ptr->is_guiding_decl) {
            pos_sy_error(ec_no_prior_declaration, &locator->source_position,
                         linked_symbol);
            err = TRUE;
          }  /* if */
        } else {
          /* When guiding declarations are not allowed, an out-of-scope
             definition is an old-style specialization.  Check whether
             such specializations are permitted. */
          check_old_specialization_allowed(linked_symbol,
                                           &locator->source_position);
        }  /* if */
      }  /* if */
    } else {
      /* The lookup failed.  Issue the right error. */
      a_symbol_ptr  sym = locator->specific_symbol;
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* The qualified name referred to an overloaded function.  We want
           to issue the appropriate error after disregarding namespace
           projection symbols, which indicate names pulled into the scope
           with a using declaration or a using directive. */
        a_symbol_ptr  overload_sym = sym, temp;
        sym = NULL;
        for (temp = overload_sym->variant.overloaded_function.symbols;
             temp != NULL;
             temp = temp->next) {
          if (temp->kind == (a_symbol_kind)sk_routine ||
              temp->kind == (a_symbol_kind)sk_function_template) {
            if (sym == NULL) {
              /* This is the first symbol in the overload set that is not
                 a projection symbol. */
              sym = temp;
            } else {
              /* This is the second -- it's okay to issue a diagnostic for
                 an overloaded function. */
              sym = overload_sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
      if (sym == NULL || sym->kind == (a_symbol_kind)sk_namespace_projection) {
        /* There's no entry directly declared in the specified scope. */
        if (nsp != NULL) {
          /* Namespace scope. */
          pos_stsy_error(ec_not_an_actual_member, &locator->source_position,
                         locator->symbol_header->identifier,
                         (a_symbol_ptr)nsp->source_corresp.assoc_info);
        } else {
          /* File scope. */
          pos_st_error(ec_name_not_found_in_file_scope,
                       &locator->source_position,
                       locator->symbol_header->identifier);
        }  /* if */
      } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* Overloaded function case. */
        pos_sy_error(ec_no_match_for_type_of_overloaded_function,
                     &locator->source_position, sym);
      } else {
        /* Everything else. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
      }  /* if */
      err = TRUE;
    }  /* if */
    if (err && nsp != NULL) {
      if (idlbp->is_friend_decl) {
        pop_namespace_reactivation_scope();
      } else {
        pop_namespace_extension_scope();
        idlbp->effective_decl_level = orig_effective_decl_level;
      }  /* if */
      idlbp->namespace_reactivated = FALSE;
    }  /* if */
  }  /* if */
  if (err) {
    /* Set safe values when an error has been reported. */
    set_to_named_error_locator(*idlbp->locator);
    idlbp->linked_symbol = NULL;
    idlbp->linkage = idl_none;
    idlbp->overload_symbol = NULL;
    idlbp->homonym_symbol = NULL;
  } else if (idlbp->linkage != idl_none) {
    /* Based on the linkage that has been determined, figure out what the
       "name linkage" should be. */
    compute_name_linkage(idlbp);
  }  /* if */
  db_exit();
}  /* qualified_name_redecl_sym */


#if !DECL_MODIFIERS_IN_USE || !GNU_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* decl_modifiers, attributes, and/or asm_name are not 
                  used in some configurations. */
#endif /* !DECL_MODIFIERS_IN_USE || !GNU_EXTENSIONS_ALLOWED */
void decl_variable(a_symbol_locator             *locator,
                   a_storage_class              storage_class,
                   a_type_ptr                   type_ptr,
                   a_source_sequence_entry_ptr  declarator_ssep,
                   a_symbol_reference_kind      srk_flags,
                   a_decl_modifiers_block_ptr   decl_modifiers,
                   an_attribute_ptr             attributes,
                   char                         *asm_name,
                   a_symbol_ptr                 *symbol_ptr,
                   an_id_linkage_kind           *linkage_ptr,
                   a_type_ptr                   *old_type,
                   a_symbol_ptr                 *ext_sym,
                   a_decl_pos_block_ptr         decl_pos_block)
/*
Enter the declaration of an identifier for a variable.  *locator gives the
symbol locator (and thus its name and its declaration position).  type_ptr,
storage_class, decl_modifiers, attributes, and asm_name give the type, storage
class, declaration modifier flags, attributes, and assembly symbol name.
Create and enter a symbol entry, and return a pointer to it in *symbol_ptr.
Also allocate any associated IL construct, and attach it to the symbol.  If
the identifier has linkage and there is an existing symbol or IL entry, it
will be re-used.  Return in *linkage_ptr the linkage of the identifier.
Return in *old_type any previously-known type for this identifier from a
linked identifier in the same scope, or NULL if there was no previously-known
type.  If the identifier has linkage, return in *ext_sym a pointer to the
external symbol entry; otherwise, set *ext_sym to NULL.  declarator_ssep
(non-NULL only if source sequence entries are being generated) is a pointer to
the empty source sequence entry already created for the declarator and added
to the appropriate list; its kind and entity pointer are updated.  srk_flags
contain specific information about the kind of declaration (whether it's a
definition, a tentative definition (C only), and so forth); this information
is passed on for use in generating cross-reference output describing this
declaration.
*/
{
  a_symbol_ptr             sym = NULL;
  a_boolean                alloc_at_file_scope;
  a_symbol_ptr             linked_symbol;
  a_boolean                redecl_error_already_issued = FALSE;
  a_boolean                linked_redecl_error = FALSE;
  a_boolean                redeclaration = FALSE;
  a_variable_ptr           variable_ptr = NULL;
  an_id_linkage_kind       linkage;
  a_source_correspondence  *source_corresp_ptr;
  a_scope_depth            effective_decl_level;
  a_boolean                suppress_ext_sym_lookup = FALSE;
  a_boolean                is_variable_def = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr               declared_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_id_linkage_block      idlb;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean                linked_to_previous_variable = FALSE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GNU_EXTENSIONS_ALLOWED
  a_boolean                is_register;
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(3, "decl_variable");
  *old_type = NULL;
#if GNU_EXTENSIONS_ALLOWED
  is_register = storage_class == (a_storage_class)sc_register;
  /* A global variable declared with "register" is effectively "static". */
  if (is_register && decl_scope_level == depth_innermost_namespace_scope) {
    storage_class = (a_storage_class)sc_static;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  check_assertion(storage_class != (a_storage_class)sc_typedef);
  if (srk_flags & SRK_DEFINITION) is_variable_def = TRUE;
  if (locator->is_template_id && !is_error_locator(*locator)) {
    /* An explicit template argument list is not allowed.  It could have
       sneaked past prior checking (during declarator processing) in Microsoft
       compatibility mode. */
    pos_error(ec_explicit_template_args_not_allowed,
              &locator->source_position);
    set_to_error_locator(*locator);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* Allow the attributes specified to modify the type with which the
     variable was declared. */
  type_ptr = apply_attributes_to_variable_type(attributes, type_ptr);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  clear_id_linkage_block(&idlb);
  idlb.locator = locator;
  idlb.type = type_ptr;
  idlb.is_definition = is_variable_def;
  idlb.storage_class = storage_class;
  idlb.direct_linkage_specifier = decl_modifiers->direct_linkage_specifier;
  set_linkage_environment(&idlb, decl_scope_level);
  if (!C_mode() && locator->specific_symbol != NULL &&
      (qualifier_namespace_ptr(*locator) != NULL ||
       locator->is_file_scope_qualified_name)) {
    /* This identifier is a namespace-qualified name that was previously
       declared.  Be sure this is a valid scope in which to define it
       (7.3.1.4). */
    qualified_name_redecl_sym(&idlb);
  } else {
    /* Determine the linkage of this symbol. */
    id_linkage(&idlb);
  }  /* if */
  locator = idlb.locator;
  storage_class = idlb.storage_class;
  linked_symbol = idlb.linked_symbol;
  effective_decl_level = idlb.effective_decl_level;
  linkage = idlb.linkage;
  /* alloc_at_file_scope will be TRUE if the IL variable entry must be
     allocated in the file scope memory region.  This is always true
     true for variables with linkage. */
  alloc_at_file_scope = (linkage != idl_none);
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    redeclaration = TRUE;
  }  /* if */
  if (redeclaration) {
    if (linked_symbol->kind == (a_symbol_kind)sk_variable) {
      /* If necessary, check that throw-specifications match. */
      if (!C_mode() &&
          ((is_ptr_or_ref_type(type_ptr) &&
            is_function_type(type_pointed_to(type_ptr))) ||
           (is_ptr_to_member_type(type_ptr) &&
            is_function_type(pm_member_type(type_ptr))))) {
        check_exception_specification(type_ptr, linked_symbol,
                                      &locator->source_position,
                                      /*is_redecl=*/TRUE);
      }  /* if */
      if (C_mode() || (microsoft_mode && (srk_flags & SRK_TENTATIVE_DEF))) {
        if (linked_symbol->defined &&
            linked_symbol->variant.variable.ptr->init_kind !=
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
        set_to_named_error_locator(*locator);
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
#if GNU_EXTENSIONS_ALLOWED
        if (is_variable_def && variable_ptr->aliased_variable != NULL) {
          /* If the variable was already defined, it cannot be assigned
             an alias now.  (See apply_attributes_to_variable for
             additional compatibility notes regarding the way in which
             GCC handles this situation.) */
          pos_sy_error(ec_cannot_be_alias_and_defn,
                       &locator->source_position, sym);
          /* Pretend the variable was not an alias so that the IL
             remains internally consistent. */
          variable_ptr->aliased_variable = NULL;
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        *old_type = variable_ptr->type;
        if (!types_are_redecl_compatible(type_ptr, *old_type)) {
          an_error_severity  severity = es_none;

          if (gcc_mode) {
            a_type_ptr  orig_type = skip_typerefs(*old_type);
            a_type_ptr  redecl_type = skip_typerefs(type_ptr);
            if (types_are_redecl_compatible(redecl_type, orig_type)) {
              /* GNU C accepts (with a warning) redeclarations of variables
                 that only differ in cv-qualification. */
              severity = es_warning;
              type_ptr = make_qualified_type(redecl_type,
                                             get_type_qualifiers(type_ptr) |
                                             get_type_qualifiers(*old_type));
              *old_type = make_qualified_type(orig_type,
                                              get_type_qualifiers(type_ptr) |
                                              get_type_qualifiers(*old_type));
            }  /* if */
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (C_mode() && microsoft_mode &&
              is_integral_type(type_ptr) && is_integral_type(*old_type) &&
              type_ptr->size == (*old_type)->size &&
              type_ptr->alignment == (*old_type)->alignment) {
            /* Just issue a warning in Microsoft C mode.  MSVC uses the first
               declaration, so adjust type_ptr. */
            severity = es_warning;
            type_ptr = *old_type;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          if (severity == es_none) {
            severity = es_error;      
            redecl_error_already_issued = TRUE;
            linked_redecl_error = TRUE;
          }  /* if */
          pos_sy_diagnostic(severity, ec_not_compatible_with_previous_decl,
                            &locator->source_position, linked_symbol);
        }  /* if */
        if (!linked_redecl_error) {
          /* The type of the variable should be the composite of the two
             types. */
          variable_ptr->type = type_ptr = composite_type(type_ptr, *old_type);
        }  /* if */
      }  /* if */
    } else {
      /* The linked symbol is a routine, while the new one is a variable. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  }  /* if */
  if (linked_redecl_error) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration.  Force a new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = NULL;
    variable_ptr = NULL;
    *old_type = NULL;
    redeclaration = FALSE;
  }  /* if */
  if (sym == NULL) {
    /* There is no (compatible) symbol, so enter one now. */
    sym = enter_symbol((a_symbol_kind)sk_variable, locator,
                       effective_decl_level,
                       redecl_error_already_issued);
#if RECORD_HIDDEN_NAMES_IN_IL
    /* Block extern declarations have associated hidden name entries; so we
       must make sure there is an IL scope to attach those entries to. */
      if (scope_stack[effective_decl_level].kind == (a_scope_kind)sck_block &&
          scope_stack[effective_decl_level].il_scope == NULL &&
          linkage != idl_none) {
        (void)ensure_il_scope_exists(&scope_stack[effective_decl_level]);
      }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  }  /* if */
  *ext_sym = NULL;
  if (linkage != idl_none && !redeclaration) {
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
       points to one, get a pointer to it and use it.
       Note that in Microsoft compilers, an extern "C" declaration (or a
       declaration with external linkage in C mode) in one scope does not link
       up with an extern "C" declaration of the same name in another scope
       (though the linker will catch redefinitions of such names). */
    a_routine_ptr  dummy_rp;
    suppress_ext_sym_lookup = suppress_ext_sym_lookup ||
                              (microsoft_bugs &&
                               idlb.name_linkage ==
                                           (a_name_linkage_kind)nlk_external);
    *ext_sym = 
        create_external_symbol_for_linked_entity(locator, type_ptr,
                                                 idlb.name_linkage,
                                                 (a_func_info_block_ptr)NULL,
                                                 redeclaration,
                                                 redecl_error_already_issued,
                                                 suppress_ext_sym_lookup,
                                                 &variable_ptr, &dummy_rp);
  }  /* if */
  /* The entity being declared is a variable. */
  if (variable_ptr == NULL) {
    /* There is no IL entry, so create one now.  If the variable has
       internal or external linkage, it is entered at the file scope. */
    a_scope_depth  scope_depth;
    if (!alloc_at_file_scope) {
      scope_depth = decl_scope_level;
    } else if (depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE ||
               (scope_stack[depth_scope_stack].default_name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
                storage_class != (a_storage_class)sc_static)) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    } else {
      scope_depth = depth_innermost_namespace_scope;
    }  /* if */
    variable_ptr = make_variable(type_ptr, storage_class, scope_depth);
    source_corresp_ptr = &variable_ptr->source_corresp;
    if (*ext_sym != NULL &&
        (*ext_sym)->variant.extern_symbol_descr->variant.variable != NULL) {
      /* A new variable entry has been created, yet the external symbol
         already refers to a different variable.  This can occur when there
         is an error, but it can also occur in SVR4 C mode -- for example:
           unsigned int i;
           void f(int i) { { extern int i; } }
         where the second declaration of i has an incompatible type, yet
         no error is issued. */
      if (!linked_redecl_error &&
          !is_error_type((*ext_sym)->variant.extern_symbol_descr->type)) {
        check_assertion_str2(SVR4_C_mode && !is_variable_def &&
                             (storage_class == (a_storage_class)sc_extern),
                             "decl_variable:",
                             "can't set superseded_external");
        variable_ptr->superseded_external = TRUE;
      }  /* if */
    }  /* if */
  } else {
    /* There is an existing IL entry that we are reusing. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    linked_to_previous_variable = TRUE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for internal linkage on the old but not the new, or
       vice-versa. */
    check_for_linkage_conflict(&variable_ptr->storage_class,
                               &linkage, &storage_class,
                               &locator->source_position,
                               /*suppress_diagnostic=*/linked_redecl_error);
    if (linkage != idlb.linkage) {
      /* The linkage has been changed, so change the "name linkage", too. */
      idlb.linkage = linkage;
      compute_name_linkage(&idlb);
    }  /* if */
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
    /* Similarly, it should have its "used" flag set.  This is only needed
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
      if (sym->defined && (srk_flags & SRK_TENTATIVE_DEF)) {
        /* In C ignore a tentative definition (i.e., one for which no
           initializer is present) if the variable has already been defined
           in a previous tentative definition.  This may also come up in
           Microsoft mode for arrays of unknown dimension. */
      } else {
        /* This is a definition of a variable that was not previously
           defined, so unlink the variable entry and relink it at the end
           of the variables list, so that variables appear in the order in
           which they are defined. */
        /* This is only possible for file-scope variables, never for local
           variables, since it is only by means of a prior extern declaration
           or (in C mode only) a prior tentative definition that we can be
           defining a variable that has already been declared. */
        a_scope_depth  depth = depth_innermost_namespace_scope;

        if (variable_ptr->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
          depth = DEPTH_OF_FILE_SCOPE;
        }  /* if */
        check_assertion(in_file_scope(variable_ptr));
        remove_from_variables_list(variable_ptr, depth);
        add_to_variables_list(variable_ptr, depth);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Link the symbol to the IL variable entry. */
  sym->variant.variable.ptr = variable_ptr;
  if (*ext_sym != NULL &&
      (*ext_sym)->variant.extern_symbol_descr->variant.variable == NULL) {
    /* Link the external symbol to the IL variable entry. */
    (*ext_sym)->variant.extern_symbol_descr->variant.variable = variable_ptr;
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the variable or routine, so
       update it with the current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
  } else if (!redeclaration) {
    /* Record a reference to the outer-scope symbol of the same name,
       but do not set the IL entity referenced flag. */
    record_symbol_reference(SRK_REFERENCE,
                            (a_symbol_ptr)source_corresp_ptr->assoc_info,
                            &locator->source_position,
                            /*update_il_entry=*/FALSE);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* Apply the attributes to the variable declaration. */
  apply_attributes_to_variable(attributes, variable_ptr);
  if (asm_name && is_register) {
    /* If the variable has been declared with the register keyword, then
       the assembly name indicates a particular register. */
    a_named_register anr = name_to_register(asm_name);
    if (anr != (a_named_register)anr_invalid) {
      variable_ptr->asm_name_or_reg.reg = anr;
      variable_ptr->asm_name_is_valid = FALSE;
    }  /* if */
  } else {
    /* Otherwise, the assembly name is just a name.  */
    variable_ptr->asm_name_or_reg.name = asm_name;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (linkage != idl_none) {
    /* In case this is a block extern declaration, clear the
       is_local_to_function flag -- it will have been set based on scope
       alone in set_source_corresp. */
    source_corresp_ptr->is_local_to_function = FALSE;
  }  /* if */
  if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
      alloc_at_file_scope && !redeclaration) {
    add_namespace_parent_pointer(sym, source_corresp_ptr);
  }  /* if */
  /* Copy the decl-modifiers into the variable entry. */
  update_variable_decl_modifiers(variable_ptr, decl_modifiers,
                                 &locator->source_position, redeclaration);
  /* The name linkage has already been determined.  Apply it to the current
     declaration, and report inconsistencies, if appropriate. */
  set_name_linkage(&idlb, sym, source_corresp_ptr, *ext_sym,
                   &locator->source_position);
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the declarator_ssep
     entry. */
  record_symbol_declaration(srk_flags, sym, &locator->source_position,
                            declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (is_variable_def || (!redeclaration && !linked_to_previous_variable)) {
    /* The position corresponds to that of the first declaration, or to that
       of the definition if a definition is seen. */
    update_decl_pos_info(&variable_ptr->source_corresp, decl_pos_block);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Restore the scope stack.  This is done prior to updating secondary
     source sequence entries because otherwise we might not find such an
     entry for the case of a nondefining namespace-qualified variable
     declaration (valid in Microsoft mode only). */
  if (idlb.namespace_reactivated) pop_namespace_extension_scope();
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Do fixup on the source sequence entry that was just created to
     represent the current declaration.  Note that declaration_ssep is not
     used, since it may have been replaced (e.g., when a file scope entity
     is declared in a local scope and a sublist is generated). */
  if (!is_variable_def || (srk_flags & SRK_TENTATIVE_DEF)) {
    an_sssd_flag_set              flags = SSSD_NO_FLAGS;
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    (void)update_src_seq_secondary_decl((char *)variable_ptr, declared_type,
                                        flags, decl_pos_block);
  } else {
    /* The defining declaration of the variable.  Record the type. */
    if (variable_ptr->declared_type == NULL) {
      variable_ptr->declared_type = declared_type;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      variable_ptr->source_corresp.marked_as_gnu_extension = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (c99_mode) {
    if (is_variable_def &&
        depth_innermost_function_scope != NO_SCOPE_DEPTH &&
        variable_ptr->storage_class == (a_storage_class)sc_static &&
        !is_const_qualified_type(variable_ptr->type)) {
      /* The definition of a local static variable that is modifiable --
         in C99 it is an error for such a variable to be defined within the
         body of an inline function with external linkage. */
      a_routine_ptr      rp;
      an_error_severity  severity;

      rp = scope_stack[depth_innermost_function_scope].assoc_routine;
      check_assertion(rp != NULL);
      if (rp->is_inline &&
          rp->storage_class == (a_storage_class)sc_unspecified) {
        severity = strict_ansi_mode ? strict_ansi_discretionary_severity :
                                      es_discretionary_error;
        pos_diagnostic(severity, ec_static_variable_in_inline_function,
                       &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  if (vla_enabled) {
    if (is_variably_modified_type(type_ptr)) {
      /* Since the type may have various run-time dependencies, put out a
         statement indicating where in the executable stream this declaration
         appears.  */
      if (depth_stmt_stack < 0) {
        /* Some (unlikely) error situations can cause us to get here outside a
           function scope: for such cases we cannot actually add a statement
           (there is no active statement stack). */
        check_assertion(total_errors > 0);
      } else {
        a_statement_ptr vla_stmt;

        variable_ptr->has_variably_modified_type = TRUE;
        vla_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_vla_decl,
                                             &locator->source_position);
        vla_stmt->variant.vla.is_typedef_decl = FALSE;
        vla_stmt->variant.vla.variant.variable = variable_ptr;
        if (is_vla_type(type_ptr)) {
          if (!is_variable_def) {
            /* Must be an error. */
            check_assertion(total_errors > 0);
          } else {
            /* Memory for this variable will also have to be allocated.  Mark
               the variable as a variable length array that requires allocation
               as well as deallocation upon exit from the current scope. */
            variable_ptr->is_vla = TRUE;
            /* Update the control-flow list used in statement processing to
               diagnose illegal branches. */
            update_init_statement_control_flow(vla_stmt);
          }  /* if */
        }  /* if */
      }  /* if */
    } /* if */
  }  /* if */
  if (is_variable_def && is_volatile_qualified_type(type_ptr)) {
    /* A variable with a volatile type is considered to be used and modified
       from "elsewhere".  (We use "is_variable_def" to exclude cases like
       "extern volatile int x", for which the flags shouldn't be set unless
       there is an explicit use in this translation unit.)  Note that this
       must be done after set_source_corresp because the latter clears the
       IL referenced flag. */
    source_corresp_ptr->referenced = TRUE;
    sym->referenced = TRUE;
    sym->variant.variable.used = TRUE;
    sym->variant.variable.value_has_been_set = TRUE;
  }  /* if */
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration.  Note that this has to be *after* the
     scope stack is restored, since processing depends on the pending_pragmas
     pointer in the scope stack entry. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  /* Return symbol and linkage pointers. */
  *symbol_ptr = sym;
  *linkage_ptr = linkage;

#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_variable */


#if GENERATE_SOURCE_SEQUENCE_LISTS

void set_routine_declared_type(a_routine_ptr  routine_ptr,
                               a_type_ptr     declared_type)
/*
Set the declared_type field in the indicated routine entry, using its own
type entry if appropriate, otherwise using the indicated declared_type.
*/
{
  a_type_ptr                     rout_type = routine_ptr->type;
  a_boolean                      use_routine_type;
  a_routine_type_supplement_ptr  rtsp1, rtsp2;
  a_param_type_ptr               ptp1, ptp2;

  if (routine_ptr->declared_type != NULL) {
    check_assertion_str(routine_ptr->is_template_function ||
                        routine_ptr->is_prototype_instantiation ||
                        (total_errors !=  0),
                       "set_routine_declared_type: declared type already set");
    declared_type = routine_ptr->declared_type;
    routine_ptr->declared_type = NULL;
  }  /* if */
  /* Make the declared type consistent with the routine type. */
  rtsp1 = skip_typerefs(rout_type)->variant.routine.extra_info;
  rtsp2 = skip_typerefs(declared_type)->variant.routine.extra_info;
  if (rtsp1->this_class != rtsp2->this_class ||
      rtsp1->qualifiers != rtsp2->qualifiers ||
      rtsp1->routine_name_linkage != rtsp2->routine_name_linkage) {
    if (declared_type->kind == (a_type_kind)tk_typeref) {
      check_assertion(!is_qualified_type(declared_type));
      declared_type =
         copy_routine_type_with_param_types(declared_type,
                                            /*copy_default_args=*/TRUE);
      rtsp2 = declared_type->variant.routine.extra_info;
    }  /* if */
    rtsp2->this_class = rtsp1->this_class;
    rtsp2->qualifiers = rtsp1->qualifiers;
    rtsp2->routine_name_linkage = rtsp1->routine_name_linkage;
  }  /* if */
  if (!identical_types(declared_type, rout_type)) {
    /* The types are not identical, so the routine's type cannot also be
       used as the declared type. */
    use_routine_type = FALSE;
  } else if ((rtsp1->exception_specification == NULL) !=
             (rtsp2->exception_specification == NULL)) {
    /* Exception specification mismatch (usually involves predeclared
       functions like new and delete). */
    use_routine_type = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL || RECORD_NAME_IN_PARAM_TYPE_ENTRY
  } else if (rtsp2->param_type_list != NULL) {
    /* The name and position information of parameters on this declaration is
       likely different from that of previous declarations.  Hence, force the
       use of the type just parsed to correctly record that information. */
    use_routine_type = FALSE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL || RECORD_NAME_IN_PARAM_TYPE_ENTRY */
  } else {
    /* Loop through the param-type entries to see if there are any default
       arguments -- if so, just use the copy of the routine type instead of
       the routine type itself. */
    use_routine_type = TRUE;
    for (ptp1 = rtsp1->param_type_list, ptp2 = rtsp2->param_type_list;
         ptp1 != NULL;
         ptp1 = ptp1->next, ptp2 = ptp2->next) {
      if (ptp1->has_default_arg || ptp2->has_default_arg) {
        use_routine_type = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* Set the declared_type pointer in the routine entry. */
  if (use_routine_type) {
    routine_ptr->declared_type = rout_type;
  } else {
    /* There was some difference, so use a separate declared type. */
    routine_ptr->declared_type = declared_type;
  }  /* if */
}  /* set_routine_declared_type */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
a_boolean update_src_seq_secondary_decl(char                  *il_entry_ptr,
                                        a_type_ptr            declared_type,
                                        an_sssd_flag_set      flags,
                                        a_decl_pos_block_ptr  decl_pos_block)
/*
Call set_src_seq_secondary_decl_fields to set the declared_type field and
various flags in the secondary-decl source sequence entry associated with
il_entry_ptr.  declared_type may be NULL.  flags is a bit vector whose
non-zero bits correspond to bit fields in the secondary source sequence
entry that need to be set.  If decl_pos_block is non-NULL, also update the
decl_pos_info supplement of the secondary-decl entry.
*/
{
  a_src_seq_secondary_decl_ptr  sssdp;

  if (source_sequence_entries_disallowed) {
    /* We are in a context in which source sequence entries are not being
       created.  No further action is required. */
    sssdp = NULL;
  } else {
    sssdp = set_src_seq_secondary_decl_fields(il_entry_ptr, declared_type,
                                              flags);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (sssdp != NULL && decl_pos_block != NULL) {
      /* Update source range information in the secondary-decl entry. */
      sssdp->decl_pos_info = make_decl_pos_supplement(in_file_scope(sssdp),
                                                      decl_pos_block);
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  return (sssdp != NULL);
}  /* update_src_seq_secondary_decl */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if !DECL_MODIFIERS_IN_USE || !GNU_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* decl_modifiers and/or attributes are not used in
                  some configurations. */
#endif /* !DECL_MODIFIERS_IN_USE || !GNU_EXTENSIONS_ALLOWED */
void decl_routine(a_symbol_locator             *locator,
                  a_storage_class              storage_class,
                  a_type_ptr                   type_ptr,
                  a_func_info_block_ptr        func_info,
                  a_source_sequence_entry_ptr  declarator_ssep,
                  a_symbol_reference_kind      srk_flags,
                  a_decl_modifiers_block_ptr   decl_modifiers,
                  an_attribute_ptr             attributes,
                  char                         *asm_name,
                  a_symbol_ptr                 *symbol_ptr,
                  an_id_linkage_kind           *linkage_ptr,
                  a_type_ptr                   *old_type,
                  a_symbol_ptr                 *ext_sym,
                  a_decl_pos_block_ptr         decl_pos_block)
/*
Enter the declaration of an identifier for a nonmember routine.  *locator
gives the symbol locator (and thus its name and its declaration position).
storage_class, type_ptr, decl_modifiers, attributes, and asm_name give the
storage class, type, declaration modifier flags, attributes, and assembly
symbol name.  If func_info->implicit_declaration is TRUE, this declaration is
for an implicit function declaration, and *symbol_ptr already contains a
pointer to the symbol entry, which is already in the symbol table; if
func_info->is_definition is TRUE, the identifier being defined is part of a
function definition (meaning there is a body in the definition), in which case
it is guaranteed that type_ptr points to an unshared type entry, and that type
entry will be preserved as the routine type.  Create and enter a symbol entry,
and return a pointer to it in *symbol_ptr.  Also allocate any associated IL
construct, and attach it to the symbol.  If the identifier has linkage and
there is an existing symbol or IL entry, it will be re-used.  Return in
*linkage_ptr the linkage of the identifier.  Return in *old_type any
previously-known type for this identifier from a linked identifier in the same
scope, or NULL if there was no previously-known type.  If the identifier has
linkage, return in *ext_sym a pointer to the external symbol entry; otherwise,
set *ext_sym to NULL.  declarator_ssep (non-NULL only if source sequence
entries are being generated) is a pointer to the empty source sequence entry
already created for the declarator and added to the appropriate list; its kind
and entity pointer are updated.  srk_flags contain specific information about
the kind of declaration (whether it's a definition, an implicit declaration (C
only), a friend declaration (C++ only), and so forth); this information is
passed on for use in generating cross-reference output describing this
declaration.
*/
{
  a_symbol_ptr             sym = NULL;
  a_symbol_ptr             linked_symbol, homonym_symbol = NULL;
  a_symbol_ptr             overload_symbol = NULL;
  a_boolean                redecl_error_already_issued = FALSE;
  a_boolean                linked_redecl_error = FALSE;
  a_boolean                old_decl_has_body = FALSE;
  a_boolean                redeclaration = FALSE;
  a_routine_ptr            routine_ptr = NULL;
  an_id_linkage_kind       linkage;
  a_source_correspondence  *source_corresp_ptr;
  a_scope_depth            effective_decl_level;
  a_boolean                template_function_specific_decl = FALSE;
  a_boolean		   explicit_template_reference = FALSE;
  a_boolean                suppress_ext_sym_lookup = FALSE;
  a_boolean                is_function_def = FALSE;
  a_boolean                changed_to_inline = FALSE;
  a_boolean                is_friend_decl = (srk_flags & SRK_FRIEND) != 0;
  a_boolean                invalid_scope_for_new_or_delete = FALSE;
  a_boolean                set_invisible = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean                first_decl = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
  an_id_linkage_block      idlb;
  a_boolean                suppress_inline_body = FALSE;

  db_enter(3, "decl_routine");
  *old_type = NULL;
  check_assertion_str(func_info != NULL, "decl_routine: NULL func_info");
  check_assertion_str(storage_class != (a_storage_class)sc_typedef,
                      "decl_routine: bad storage class");
  check_assertion_str(srk_flags & SRK_DECLARATION,
                      "decl_routine: missing SRK_DECLARATION");
  if (func_info->is_definition) {
    is_function_def = TRUE;
    check_assertion_str(srk_flags & SRK_DEFINITION,
                        "decl_routine: missing SRK_DEFINITION");
  }  /* if */
  if (!C_mode()) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* When the declared_type was created (in declarator), the default args
       were ignored.  If appropriate, copy them from type_ptr to the
       declared_type now (i.e., before composite_type is called). */
    if (!is_function_def && source_sequence_entries_disallowed) {
      /* The declared_type is not used. */
    } else if (func_info->declared_type == type_ptr) {
      /* No fixup required.  (This can happen when type_ptr is a typedef.) */
    } else if (func_info->declared_type != NULL &&
               skip_typerefs(func_info->declared_type)->
                                variant.routine.extra_info->prototyped) {
      copy_routine_type_default_args(type_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CHECKING
    if (!C_mode() && func_info->is_inline && !extern_inline_allowed) {
      check_assertion_str(storage_class == (a_storage_class)sc_unspecified ||
                          storage_class == (a_storage_class)sc_static,
                          "decl_routine: bad storage class for inline");
    }  /* if */
#endif /* CHECKING */
    /* Verify that we are not declaring a const or volatile function through
       a typedef (other cases are caught while parsing). */
    if (type_ptr->kind == (a_type_kind)tk_typeref &&
        typeref_is_typedef(type_ptr) &&
        skip_typerefs(type_ptr)->variant.routine.extra_info->qualifiers
                                                                 != TQ_NONE) {
      /* Strip the qualifier from the routine type to avoid problems
         later on. */
      a_type_ptr  type_for_recovery = alloc_type((a_type_kind)tk_routine);
      copy_type(skip_typerefs(type_ptr), type_for_recovery);
      type_for_recovery->variant.routine.extra_info->qualifiers = TQ_NONE;
      type_ptr = type_for_recovery;
      pos_error(ec_bad_qualified_function_type, &locator->source_position);
    }  /* if */
    /* If this is an overloaded operator, check for errors in the
       argument list. */
    check_operator_function_params(type_ptr, /*class_type=*/(a_type_ptr)NULL,
                                   locator);
    report_bad_new_or_delete(locator, storage_class,
                             &invalid_scope_for_new_or_delete);
  } else if (c99_mode) {
    /* In C99 mode, if a function is declared "inline" every time it is
       declared in a given translation unit and is never declared with an
       explicitly specified storage class, then its definition is regarded
       as an "inline definition" instead of an "external definition" (see
       6.9, 6.7.4).  An inline function with an "inline definition", even
       though it has external linkage, is not visible outside the current
       translation unit. */
    if (func_info->is_inline &&
        storage_class == (a_storage_class)sc_unspecified) {
      /* "inline" was present in the declaration, but no storage class was
         specified. */
      suppress_inline_body = TRUE;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (gcc_mode && 
             storage_class == (a_storage_class)sc_extern &&
             func_info->is_inline && func_info->is_definition) {
    /* In GCC mode, if a function definition uses both the "extern"
       and "inline" keywords then no definition of the function
       should be emitted, even though it has external linkage.  This
       treatment is analogous to the C99 "inline definition" concept. */
    suppress_inline_body = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  clear_id_linkage_block(&idlb);
  idlb.locator = locator;
  idlb.storage_class = storage_class;
  idlb.type = type_ptr;
  idlb.func_info = func_info;
  idlb.is_definition = is_function_def;
  set_linkage_environment(&idlb, decl_scope_level);
  check_assertion(idlb.is_friend_decl == ((srk_flags & SRK_FRIEND) != 0) ||
                  is_or_contains_error_type(type_ptr));
  idlb.is_friend_decl = ((srk_flags & SRK_FRIEND) != 0);
  is_friend_decl = idlb.is_friend_decl;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (locator->is_template_id && locator->specific_symbol == NULL) {
      /* If this is a template-id for which the symbol has not yet been
         found, look it up now. */
      (void)normal_id_lookup(locator, IDL_NO_OPTIONS);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (func_info->is_implicit_declaration) {
    check_assertion_str(srk_flags & SRK_IMPLICIT,
                        "decl_routine: missing SRK_IMPLICIT");
    if (C_dialect != C_dialect_cplusplus) {
      /* For an implicit function, the identifier would not be in the process
         of being declared implicitly as a function if there were any visible
         declaration of it, and therefore it must have external linkage. */
      idlb.linkage = linkage = idl_external;
      compute_name_linkage(&idlb);
    } else {
      /* In C++ this is an error case.  Don't give this dummy routine any
         linkage. */
      linkage = idl_none;
    }  /* if */
    linked_symbol = NULL;
    sym = *symbol_ptr;
  } else {
    if (!C_mode() && locator->specific_symbol != NULL &&
        (qualifier_namespace_ptr(*locator) != NULL ||
         locator->is_file_scope_qualified_name ||
         locator->is_template_id)) {
      /* This identifier is a namespace-qualified name that was previously
         declared, or else a file-scope qualified name (friend declarations
         only).  Do the appropriate checking, including overload resolution.
         Furthermore, for definitions of namespace-qualified names, be sure
         this is a valid scope for the definition (7.3.1.4). */
      /* Look up the name. */
      qualified_name_redecl_sym(&idlb);
    } else {
      /* Determine the linkage of this symbol. */
      id_linkage(&idlb);
    }  /* if */
    linkage = idlb.linkage;
    linked_symbol = idlb.linked_symbol;
    homonym_symbol = idlb.homonym_symbol;
    overload_symbol = idlb.overload_symbol;
    effective_decl_level = idlb.effective_decl_level;
    storage_class = idlb.storage_class;
  }  /* if */
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
        linked_symbol->variant.routine.instance_ptr != NULL) {
      if (locator->is_template_id ||
          (locator->is_qualified_name && is_friend_decl)) {
        /* This is either a friend declaration that nominates an instance of a
           previously declared template, or it is an old-style specialization
           with explicit template arguments. */
        explicit_template_reference = TRUE;
      } else if (!locator->is_qualified_name) {
        /* This is not actually a redeclaration -- linked_symbol refers to a
           function template instantiation. */
        check_assertion(guiding_decls_allowed || microsoft_mode);
        template_function_specific_decl = TRUE;
      }  /* if */
    }  /* if */
    if (!explicit_template_reference && !template_function_specific_decl) {
      /* The new declaration must be compatible with the old. */
      redeclaration = TRUE;
    }  /* if */
#if CHECKING
    if (linked_symbol->overload_set_member && !is_friend_decl) {
      /* Normally we should have a record of which overload set the symbol
         belongs to.  The exception occurs when redeclaring an entity that
         is visible only through a using-declaration in Sun or Microsoft 
         mode. */
      check_assertion((redeclaration && (microsoft_mode || sun_mode) &&
                       scope_stack[decl_scope_level].number !=
                                                 linked_symbol->decl_scope) ||
                      (overload_symbol != NULL &&
                       overload_symbol->kind ==
                                      (a_symbol_kind)sk_overloaded_function));
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  if (redeclaration) {
    if (linked_symbol->kind == (a_symbol_kind)sk_routine) {
      /* Linked symbol and new symbol are both routines.  The new declaration
         must be compatible with the old. */
      sym = linked_symbol;
      routine_ptr = linked_symbol->variant.routine.ptr;
      check_assertion_str(routine_ptr != NULL,
                          "decl_routine: linked symbol routine is missing");
      if (routine_has_been_defined(routine_ptr)
#if ASM_FUNCTION_ALLOWED
          || routine_ptr->storage_class == (a_storage_class)sc_asm
#endif /* ASM_FUNCTION_ALLOWED */
                                                        ) {
        /* Previous declaration was a definition.  (We check assoc_scope
           rather than the defined flag in the routine, because in pcc mode
           it is possible to have a nested redeclaration -- e.g.,
             int f() { int f(); ... };
           -- but the flag isn't set till the definition is complete.) */
        old_decl_has_body = TRUE;
      } else if (sym->defined) {
        /* In C++ the defined flag in the symbol may have been set without
           the body having been scanned and bound to the routine yet (e.g.,
           inline friend function or a dllimport function). */
#if MICROSOFT_EXTENSIONS_ALLOWED
        check_assertion_str((routine_ptr->decl_modifiers & DM_DLLIMPORT) ||
                            routine_ptr->defined_in_friend_decl ||
                            scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union,
                            "decl_routine: defined flag is set wrong");
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
        check_assertion_str(routine_ptr->defined_in_friend_decl ||
                            scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union,
                            "decl_routine: defined flag is set wrong");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
        set_to_named_error_locator(*locator);
      } else {
        /* Check that the routine types are compatible. */
        a_boolean      routines_compat = TRUE;
        an_error_code  error_code = ec_not_compatible_with_previous_decl;

        /* Friend functions that name an existing declaration should not
           introduce default arguments. */
        if (!friend_injection_enabled && is_friend_decl &&
            func_info->any_default_args) {
          pos_error(ec_friend_cannot_add_default_arguments,
                    &locator->source_position);
        }  /* if */
        /* For routines that can be overloaded, id_linkage has already
           checked that the routine types are compatible.  "main" cannot
           be overloaded, so it was not checked. */
        if ((C_mode() || func_info->is_main_function) &&
            !types_are_compatible(routine_ptr->type, type_ptr)) {
          /* Error -- redeclaration requires type compatibility. */
          routines_compat = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode &&
                   !calling_conventions_are_compatible(routine_ptr->type,
                                                       type_ptr)) {
          /* Error -- calling conventions are not compatible. */
          routines_compat = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (!C_mode() && !func_info->is_main_function &&
                   !routine_name_linkages_are_compatible(routine_ptr->type,
                                                         type_ptr)) {
          /* Error -- routine-name-linkages are not compatible. */
          routines_compat = FALSE;
          error_code = ec_incompatible_linkage_specifier;
        }  /* if */
        if (!routines_compat) {
          /* The old and new declarations are incompatible.  There is special
             handling for SVR4 and Microsoft C compatibility modes. */
          a_type_ptr  old_return_type = return_type_of(routine_ptr->type);
          a_type_ptr  new_return_type = return_type_of(type_ptr);
          if (SVR4_C_mode &&
              incompatible_types_are_SVR4_compatible(type_ptr,
                                                     routine_ptr->type)) {
            /* The routine types are incompatible, but in SVR4 mode this is
               not an error as long as the incompatibility is only in the
               return type. */
            pos_sy_warning(ec_not_compatible_with_previous_decl,
                           &locator->source_position, linked_symbol);
            /* There is compatibility but for the return type.  If this is the
               definition, reset the type of the routine entry to use the new
               type. */
            *old_type = routine_ptr->type;
            if (is_function_def) routine_ptr->type = type_ptr;
          } else if (microsoft_mode && C_mode() &&
                     identical_types(old_return_type, new_return_type)) {
            /* In Microsoft C mode "anything goes" as far as function
               redeclarations are concerned, provided the return types
               are identical. */
            pos_sy_warning(ec_not_compatible_with_previous_decl,
                           &locator->source_position, linked_symbol);
            *old_type = routine_ptr->type;
            if (is_function_def || !old_decl_has_body) {
              routine_ptr->type = type_ptr;
            }  /* if */
          } else {
            /* Issue an error on incompatible declarations. */
            pos_sy_error(error_code, &locator->source_position, linked_symbol);
            if (routine_ptr->storage_class == (a_storage_class)sc_static) {
              /* Reuse the routine entry to avoid error recovery problems
                 connected with constraints placed on static functions. */
              *old_type = routine_ptr->type;
              if (is_function_def) routine_ptr->type = type_ptr;
            } else {
              /* Force creation of a new symbol and a new routine entry. */
              linked_redecl_error = TRUE;
            }  /* if */
            /* Set a flag to suppress reuse of the existing external-routine
               symbol. */
            suppress_ext_sym_lookup = TRUE;
          }  /* if */
          redecl_error_already_issued = TRUE;
        } else {
          /* The declarations are compatible.  Form the composite type. */
          *old_type = routine_ptr->type;
          if (C_dialect == C_dialect_cplusplus) {
            if (strict_ansi_mode && idlb.is_local_class_friend_decl) {
              /* A local class friend declaration must refer to a function
                 declared within the immediately enclosing non-class scope. */
              a_symbol_ptr  prior_decl = idlb.prior_decl_in_enclosing_scope;

              if (prior_decl != NULL &&
                  prior_decl->decl_scope !=
                           scope_stack[effective_decl_level].number) {
                /* There was a prior declaration, but it wasn't in the
                   innermost enclosing non-class scope. */
                pos_diagnostic(strict_ansi_discretionary_severity,
                               ec_local_class_friend_requires_prior_decl,
                               &locator->source_position);
              }  /* if */
            }  /* if */
            /* Do compatibility checking on the throw specification. */
            check_exception_specification(type_ptr, linked_symbol,
                                          &func_info->throw_position,
                                          /*is_redecl=*/TRUE);
          }  /* if */
          reconcile_routine_types(routine_ptr, type_ptr,
                                  /*preserve_rout_type=*/old_decl_has_body,
                                  /*preserve_type_ptr=*/is_function_def);
        }  /* if */
#if GNU_EXTENSIONS_ALLOWED
        if (routine_ptr->aliased_routine != NULL) {
          pos_sy_error(ec_cannot_be_alias_and_defn, 
                       &locator->source_position, linked_symbol);
          /* Pretend the routine was not an alias so that the IL
             remains internally consistent. */
          routine_ptr->aliased_routine = NULL;
        }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      }  /* if */
    } else {
      /* The linked symbol must be a variable. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  } else {
    /* Not a redeclaration. */
    a_symbol_ptr  symbol_for_overloading = NULL;

    if (C_dialect == C_dialect_cplusplus) {
      /* Be sure the default arguments, if any, are at the end of the
         parameters list. */
      check_default_args(type_ptr);
      if (is_friend_decl && !friend_injection_enabled) {
        set_invisible = TRUE;
      }  /* if */
      symbol_for_overloading = overload_symbol == NULL ? homonym_symbol :
                                                         overload_symbol;
      if (homonym_symbol != NULL &&
          homonym_symbol->kind != (a_symbol_kind)sk_function_template) {
        /* homonym_symbol is a previously declared routine symbol with the
           same name but a different type signature from that of the current
           declaration.  We may have an instance of function overloading. */
        an_error_code  error_code;

        if (!overload_distinguishable(homonym_symbol, type_ptr,
                                      (a_template_param_ptr)NULL,
                                      &error_code)) {
          /* The previous declaration and the current one are not "overload
             distinguishable" for a reason given by the error code returned. */
          pos_error(error_code, &locator->source_position);
          redecl_error_already_issued = TRUE;
          /* We can't add a symbol to the overload list, so change to locator
             to an error locator to prevent hiding the overload symbol when
             the new symbol is entered. */
          set_to_error_locator(*locator);
          /* Don't treat this as a template function specific declaration
             even if it was previously thought to be.  Do treat it as a
             redeclaration error. */
          template_function_specific_decl = FALSE;
          linked_redecl_error = TRUE;
          goto skip_overloading;
        }  /* if */
      }  /* if */
      if (is_friend_decl) {
        a_symbol_ptr  prior_decl = idlb.prior_decl_in_enclosing_scope;

        if (prior_decl != NULL) {
          if (!is_function_symbol(fundamental_symbol_of(prior_decl))) {
            /* Issue an error for a case like this:
                 int x;
                 struct S { friend x(); }    // Incompatible decl
            */
            pos_sy_error(ec_decl_incompatible_with_previous_use,
                         &locator->source_position, prior_decl);
            redecl_error_already_issued = TRUE;
          }  /* if */
        } else {
          if (strict_ansi_mode && idlb.is_local_class_friend_decl) {
            /* A local class friend declaration requires a prior declaration
               in the scope that encloses the class definition. */
            pos_diagnostic(strict_ansi_discretionary_severity,
                           ec_local_class_friend_requires_prior_decl,
                           &locator->source_position);
          }  /* if */
        }  /* if */
      } else if (!C_mode() && idlb.is_block_extern_decl &&
                 idlb.prior_decl_in_enclosing_scope != NULL) {
        /* Be sure to transfer the name linkage ("C"/"C++") to a block extern
           declaration of a function if a prior declaration was visible. */
        a_symbol_ptr  prior_decl =
                    fundamental_symbol_of(idlb.prior_decl_in_enclosing_scope);
        if (is_function_symbol(prior_decl)) {
          a_type_ptr  prior_type = routine_symbol_type(prior_decl);
          if (routine_types_are_compatible(prior_type, type_ptr,
                                           TCF_NO_FLAGS) &&
              type_ptr->kind != (a_type_kind)tk_typeref) {
            type_ptr->variant.routine.extra_info->routine_name_linkage =
                 prior_type->variant.routine.extra_info->routine_name_linkage;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (template_function_specific_decl && !inside_local_class &&
        depth_innermost_function_scope == NO_SCOPE_DEPTH) {
      /* This is an explicit declaration of a template function.  Note that
         we are only interested in file- and namespace-scope declarations --
         declarations at local scope are handled separately. */
      sym = linked_symbol;
      routine_ptr = sym->variant.routine.ptr;
      if (routine_has_been_defined(routine_ptr)) {
        old_decl_has_body = TRUE;
      } else if (sym->defined) {
        /* In C++ the defined flag may have been set without the body having
           been scanned and bound to the routine yet (e.g., inline friend
           function). */
        check_assertion_str(scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union,
                            "decl_routine: defined flag is set wrong");
        old_decl_has_body = TRUE;
      }  /* if */
      if (is_function_def || (microsoft_mode && !is_friend_decl)) {
        /* This is normally a definition (an old-style specialization). */
        /* In Microsoft mode it need not be a definition.  Consider:
             template <class T> void f(T t) { ... }
             void f(int);
           Function f(int) is marked as a specialization, and it is expected
           that the definition will be provided elsewhere (rather than
           generated from the template) -- i.e., the second line appears to
           have the same meaning as if it were written:
             template<> void f(int);
        */
        check_old_specialization_allowed(sym, &locator->source_position);
        if (!old_decl_has_body) {
          /* Okay. */
          /* Update the linkage information in the routine to reflect
             this declaration instead of the information inherited from
             the template. */
          routine_ptr->storage_class = storage_class;
          if (func_info->is_inline && !routine_ptr->is_inline) {
            changed_to_inline = TRUE;
          }  /* if */
          routine_ptr->is_inline = func_info->is_inline;
          routine_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_static) ?
                                (a_name_linkage_kind)nlk_internal :
                                (a_name_linkage_kind)nlk_cplusplus_external;
          routine_ptr->is_specialized = TRUE;
          routine_ptr->specialized_with_old_syntax = TRUE;
        } else if (is_function_def) {
          /* There is already a definition.  This is some sort of error. */
          if (!sym->variant.routine.ptr->is_specialized &&
              routine_ptr->is_inline && routine_ptr->called) {
            /* An inline function template has been declared, an instance
               of it has been referenced and therefore instantiated on
               the fly, and now a specializing declaration appears.
               Issue an error (you can't reference an inline template
               function that is specialized before the specialization is
               declared) and treat it as a redeclaration error. */
            pos_error(ec_specialization_of_called_inline_template_function,
                      &locator->source_position);
          } else {
            /* Already defined, presumably by a specialization. */
            pos_sy_error(ec_already_defined, &locator->source_position, sym);
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
        if (!sym->variant.routine.instance_ptr->is_guiding_decl &&
            symbol_for_overloading != NULL) {
          a_boolean	use_namespace;

          check_assertion_str(sym->parent.namespace_ptr ==
                                symbol_for_overloading->parent.namespace_ptr,
                             "decl_routine: namespace mismatch");
          /*  Its symbol is already on the template's function instantiation
              list, but it needs to be added to the overload list as well,
              to assure that it will be found by the ordinary overload
              resolution algorithm. */
          use_namespace = sym->parent.namespace_ptr != NULL;
          overload_symbol = 
                    add_symbol_to_overload_list(sym, symbol_for_overloading,
                                                use_namespace,
                                                sym->parent.namespace_ptr);
          sym->variant.routine.instance_ptr->is_guiding_decl = TRUE;
        }  /* if */
        *old_type = routine_ptr->type;
        if (C_dialect == C_dialect_cplusplus) {
          /* Do compatibility checking on the throw specification. */
          check_exception_specification(type_ptr, linked_symbol,
                                        &func_info->throw_position,
                                        /*is_redecl=*/TRUE);
        }  /* if */
        /* A guiding declaration should never impose the declared type --
           always use the type derived from the template declaration, even
           if the guiding declaration is simultaneously an old-style
           specialization. */
        reconcile_routine_types(routine_ptr, type_ptr,
                                /*preserve_rout_type=*/TRUE,
                                /*preserve_type_ptr=*/FALSE);
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
      first_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
    } else if (explicit_template_reference) {
      /* A reference to a template instance in a friend declaration or an
         old-style specialization.  Such a declaration cannot be a definition
         unless we're in Microsoft mode. */
      sym = linked_symbol;
      routine_ptr = sym->variant.routine.ptr;
      if (is_function_def && !microsoft_mode) {
        pos_sy_error(ec_old_specialization_not_allowed,
                     &locator->source_position, sym);
        /* Set a flag to suppress reuse of the existing external-routine
           symbol and of the routine already in use.  Also, to suppress a
           possible declared-but-not-used message, set the referenced flag
           in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        mark_symbol_to_suppress_warnings(linked_symbol);
        set_to_named_error_locator(*locator);
      } else if (func_info->is_inline &&
                 (!is_friend_decl || !microsoft_mode)) {
        /* A declaration that is an explicit reference of a template cannot
           include the inline specifier. */
        pos_diagnostic(strict_ansi_discretionary_severity,
                       ec_inline_not_allowed, &locator->source_position);
      }  /* if */
      if (!locator->is_template_id) {
        /* If the declarator was not specified using an explicit template
           argument list, check for the presence of default arguments, which
           are not permitted in template instance declarations. */
        check_for_any_default_args(type_ptr);
      }  /* if */
      /* Do compatibility checking on the throw specification. */
      check_exception_specification(type_ptr, linked_symbol,
                                    &func_info->throw_position,
                                    /*is_redecl=*/TRUE);
      if (!is_friend_decl && !is_error_locator(*locator)) {
        /* This is an old-style specialization (using explicit template
           arguments). */
        /* Update the linkage information in the routine to reflect
           this declaration instead of the information inherited from
           the template. */
        routine_ptr->storage_class = storage_class;
        if (func_info->is_inline && !routine_ptr->is_inline) {
          changed_to_inline = TRUE;
        }  /* if */
        routine_ptr->is_inline = func_info->is_inline;
        routine_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_static) ?
                                (a_name_linkage_kind)nlk_internal :
                                (a_name_linkage_kind)nlk_cplusplus_external;
        routine_ptr->is_specialized = TRUE;
        routine_ptr->specialized_with_old_syntax = TRUE;
      }  /* if */
    } else if (symbol_for_overloading != NULL) {
      /* Overloaded function.  Create the new symbol, which will be on the
         list of functions connected to an sk_overloaded symbol. */
      a_boolean  overload_set_is_invisible = FALSE;

      if (set_invisible && symbol_for_overloading->is_invisible) {
        overload_set_is_invisible = TRUE;
      }  /* if */
      sym = enter_overloaded_symbol((a_symbol_kind)sk_routine, locator,
                                    /*is_constructor=*/FALSE,
                                    symbol_for_overloading, &overload_symbol);
      if (set_invisible) {
        sym->is_invisible = TRUE;
        if (overload_set_is_invisible) {
          overload_symbol->is_invisible = TRUE;
        }  /* if */
      } else if (!is_friend_decl) {
        overload_symbol->is_invisible = FALSE;
      }  /* if */
    }  /* if */
skip_overloading:;
  }  /* if */
  if (linked_redecl_error) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration.  Force a new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = NULL;
    routine_ptr = NULL;
    *old_type = NULL;
    redeclaration = FALSE;
  }  /* if */
  if (sym == NULL) {
    /* There is no (compatible) symbol, so enter one now. */
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             effective_decl_level,
                             redecl_error_already_issued);
#if RECORD_HIDDEN_NAMES_IN_IL
    /* Block extern declarations have associated hidden name entries; so we
       must make sure there is an IL scope to attach those entries to. */
      if (scope_stack[effective_decl_level].kind == (a_scope_kind)sck_block &&
          scope_stack[effective_decl_level].il_scope == NULL &&
          linkage != idl_none) {
        (void)ensure_il_scope_exists(&scope_stack[effective_decl_level]);
      }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    if (microsoft_mode && invalid_scope_for_new_or_delete) {
      /* An operator new or delete function was declared in a namespace scope.
         The Microsoft C++ compiler permits this (i.e., no error is issued),
         yet it proceeds to ignore the declaration in processing new and
         delete expressions.  We emulate this behavior by not adding the
         symbol to the symbol table. */
      remove_symbol(sym);
    }  /* if */
    /* Mark friend functions for which this is the initial declaration. */
    if (set_invisible) sym->is_invisible = TRUE;
  } else {
    /* Note: if this is an implicit declaration of a function, the symbol has
       already been entered and marked as declared. */
    /* If appropriate, clear the is_invisible flag in the linked symbol and
       in the symbol representing its overload set. */
    if (sym->is_invisible && !is_friend_decl) {
      sym->is_invisible = FALSE;
      if (sym->overload_set_member) {
        check_assertion(overload_symbol != NULL);
        overload_symbol->is_invisible = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  *ext_sym = NULL;
  if (linkage != idl_none && !redeclaration &&
      (!scope_stack[depth_scope_stack].in_prototype_instantiation ||
       prototype_instantiations_in_il)) {
    /* Create an external symbol for the present linkable declaration.
       Ordinarily, this may involve some lookup to find a declaration in a
       previous scope to which the present one is linked.  However, in
       Microsoft compilers, an extern "C" declaration (or a declaration with
       external linkage in C mode) in one scope does not link up with an
       extern "C" declaration of the same name in another scope (though the
       linker will catch redefinitions of such names). */
    a_variable_ptr  dummy_vp;
    suppress_ext_sym_lookup = suppress_ext_sym_lookup ||
                              (microsoft_bugs &&
                               idlb.name_linkage ==
                                           (a_name_linkage_kind)nlk_external);
    *ext_sym = 
        create_external_symbol_for_linked_entity(locator, type_ptr,
                                                 idlb.name_linkage,
                                                 func_info, redeclaration,
                                                 redecl_error_already_issued,
                                                 suppress_ext_sym_lookup,
                                                 &dummy_vp, &routine_ptr);
  }  /* if */
  if (template_function_specific_decl && sym != linked_symbol) {
    /* This is a declaration of a template function at the local scope.
       A function instantiation entry with an associated symbol and routine
       entry already exist.  Be sure this local symbol is properly bound
       to the file-scope entities to which it corresponds. */
    if (routine_ptr != NULL &&
        routine_ptr != linked_symbol->variant.routine.ptr) {
      /* It must be that this routine was mentioned in a block-extern
         declaration before the function template declaration was seen. */
      check_assertion_str2(*ext_sym != NULL &&
                           (*ext_sym)->variant.extern_symbol_descr->
                                        variant.routine.ptr == routine_ptr &&
                           (a_symbol_ptr)routine_ptr->
                                   source_corresp.assoc_info != linked_symbol,
                          "decl_routine: unexpected conditions for routine",
                          "entry mismatch");
      /* Replace the routine pointed to from the extern-routine symbol with
         the new one. */
      (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr = NULL;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
    if (idlb.is_new_template_instance) {
      /* This declaration triggered the creation of a new template instance. */
      first_decl = TRUE;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
    sym->variant.routine.instance_ptr =
                                linked_symbol->variant.routine.instance_ptr;
    routine_ptr = linked_symbol->variant.routine.ptr;
    sym->variant.routine.ptr = routine_ptr;
    *old_type = routine_ptr->type;
    reconcile_routine_types(routine_ptr, type_ptr,
                            /*preserve_rout_type=*/TRUE,
                            /*preserve_type_ptr=*/FALSE);
    /* Do compatibility checking for the throw specification. */
    check_exception_specification(type_ptr, linked_symbol,
                                  &func_info->throw_position,
                                  /*is_redecl=*/TRUE);
  } else if (routine_ptr == NULL) {
    /* There is no IL entry, so create one now, and add it to the routines
       list of the innermost namespace scope (or, if this is an extern "C"
       context, add it to routines list of the file scope).  If we're in a
       prototype instantiation scope, do not add it to the routines list
       unless prototype instantiations are stored in the IL. */
    a_scope_depth  scope_depth = depth_innermost_namespace_scope;

    if (scope_stack[depth_scope_stack].in_prototype_instantiation &&
        !prototype_instantiations_in_il) {
      scope_depth = NO_SCOPE_DEPTH;
    } else if (linkage == idl_external &&
               scope_stack[depth_scope_stack].default_name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
      scope_depth = DEPTH_OF_FILE_SCOPE;
    }  /* if */
    routine_ptr = make_routine(type_ptr, storage_class, scope_depth);
    if (C_dialect == C_dialect_cplusplus) {
      if (locator->is_operator_name) {
        routine_ptr->special_kind = (a_special_function_kind)sfk_operator;
        routine_ptr->opname_or_builtin.opname_kind = locator->variant.opname;
      }  /* if */
    }  /* if */
    if (!linked_redecl_error && *ext_sym != NULL &&
        (*ext_sym)->variant.extern_symbol_descr->
                                            variant.routine.ptr != NULL) {
      /* A new routine entry has been created, yet the external symbol
         already refers to a different routine.  This can occur when there
         is an error, but it can also occur in SVR4 C mode -- for example:
           extern int ff();
           void f(int ff) { { extern float ff(); } }
         where the second declaration of ff has an incompatible type, yet
         no error is issued. */
      routine_ptr->superseded_external = TRUE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
    first_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
  } else {
    /* There is an existing IL entry that we are reusing. */
    /* Check for internal linkage on the old but not the new, or
       vice-versa. */
    a_boolean suppress_diagnostic = linked_redecl_error;

    if (routine_ptr->compiler_generated) {
      /* This is an entry for an intrinsic function or operator (e.g., the
         compiler generated ::operator new or ::operator delete).  It was
         created during initialization, but is overridden by the present
         declaration. */
      check_assertion_str2(routine_ptr->source_corresp.decl_position.seq == 0,
                           "decl_routine: compiler-generated function was",
                           "already assigned a position");
      routine_ptr->compiler_generated = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL
      /* Since the flag is cleared here, we're guaranteed that this is the
         first time we see the declaration in this translation unit. */
      first_decl = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS || EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Don't diagnose linkage mismatches either. */
      suppress_diagnostic = TRUE;
      /* Record the new source position, both in the symbol and in the
         routine entry. */
      sym->decl_position = locator->source_position;
      routine_ptr->source_corresp.decl_position = sym->decl_position;
      /* Record the type of the latest declaration (which may have a different
         exception specification). */
      routine_ptr->type = type_ptr;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (routine_ptr->source_corresp.decl_pos_info == NULL &&
          decl_pos_block != NULL) {
        /* Update source range information now that we have seen an actual
           declaration. */
        routine_ptr->source_corresp.decl_pos_info =
                         make_decl_pos_supplement(in_file_scope(routine_ptr),
                                                  decl_pos_block);
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if CHECKING
      if (routine_ptr->special_kind ==
                             (a_special_function_kind)sfk_operator) {
        check_assertion_str
             (is_new_operator(routine_ptr->opname_or_builtin.opname_kind) ||
	      is_delete_operator(routine_ptr->opname_or_builtin.opname_kind),
	      "decl_routine: bad opname kind");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
#if ASM_FUNCTION_ALLOWED
    if (storage_class == (a_storage_class)sc_asm ||
        routine_ptr->storage_class == (a_storage_class)sc_asm) {
      /* asm functions have internal linkage but do not conflict
         with previous declarations that are either extern or static. */
        routine_ptr->storage_class = storage_class = (a_storage_class)sc_asm;
    } else
#endif /* ASM_FUNCTION_ALLOWED */
    /* Do not add code here. */
    {
      check_for_linkage_conflict(&routine_ptr->storage_class, &linkage,
                                 &storage_class, &locator->source_position,
                                 suppress_diagnostic);
      if (linkage != idlb.linkage) {
        /* The linkage has been changed, so change the "name linkage", too. */
        idlb.linkage = linkage;
        compute_name_linkage(&idlb);
      }  /* if */
    }  /* if */
    if (is_function_def) {
      a_boolean      saved_referenced_flag;
      a_scope_depth  scope_depth = depth_innermost_namespace_scope;

      /* If this is a definition, unlink the routine entry and relink it
         at the end of the routines list, so that routines appear in the
         order that their bodies appear. */
      if (routine_ptr->source_corresp.name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
        scope_depth = DEPTH_OF_FILE_SCOPE;
      }  /* if */
      remove_from_routines_list(routine_ptr, scope_depth);
      add_to_routines_list(routine_ptr, scope_depth);
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
      if (func_info->is_inline && !routine_ptr->is_inline) {
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (func_info->is_inline) routine_ptr->is_inline = TRUE;
  if (c99_mode) {
    /* In C99 mode the suppress_inline_body flag is set only if that is
       justified by every declaration of a given inline function. */
    if (redeclaration) {
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
      if ((!suppress_inline_body || !routine_ptr->suppress_inline_body) &&
          routine_ptr->storage_class == (a_storage_class)sc_unspecified) {
        /* The definition should not be discarded if it was preceded or
           followed by an extern declaration.  (If the current definition
           has an inline specifier and routine->suppress_inline_body is FALSE,
           the previous declaration did not have an "inline" specifier.
           If the previous declaration was an inline definition and the
           current declaration has no inline specifier, then
           suppress_inline_body will be FALSE.) */
        mark_as_needed((char *)routine_ptr, (an_il_entry_kind)iek_routine);
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
      routine_ptr->suppress_inline_body &= suppress_inline_body;
    } else {
      routine_ptr->suppress_inline_body = suppress_inline_body;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (gcc_mode && suppress_inline_body) {
    /* In GNU mode only the keywords present at the point of
       definition matter. */
    routine_ptr->suppress_inline_body = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  source_corresp_ptr = &routine_ptr->source_corresp;
  update_routine_decl_modifiers(routine_ptr, decl_modifiers,
                                &locator->source_position, redeclaration,
                                is_function_def,
                                (a_boolean)func_info->is_inline);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && func_info->is_main_function) {
    /* main should use __cdecl calling convention.  If that's not the default
       for the compilation, set it now. */
    if (type_ptr->kind != (a_type_kind)tk_routine) {
      /* Skip this processing if the function was declared with a typedef. */
    } else {
      a_routine_type_supplement_ptr  rtsp;
      rtsp = type_ptr->variant.routine.extra_info;
      if (rtsp->calling_convention != (a_calling_convention)cc_default ||
          default_calling_convention != (a_calling_convention)cc_cdecl) {
        if (rtsp->calling_convention == (a_calling_convention)cc_cdecl) {
          /* __cdecl was already explicitly specified in this declaration. */
        } else {
          rtsp->calling_convention = (a_calling_convention)cc_cdecl;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (microsoft_mode) {
    /* In Microsoft mode we must track whether a routine was only declared
       through friend declarations.  Such declarations do not declare
       specializations of templates.
       (See record_predeclared_template_function) */
    if (is_friend_decl) {
      if (!redeclaration) {
        routine_ptr->declared_only_as_friend = TRUE;
      }  /* if */
    } else {
      routine_ptr->declared_only_as_friend = FALSE;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Link the symbol to the IL routine entry. */
  sym->variant.routine.ptr = routine_ptr;
  if (*ext_sym != NULL &&
      (*ext_sym)->variant.extern_symbol_descr->variant.routine.ptr == NULL) {
    /* Link the external symbol to the IL routine entry. */
    (*ext_sym)->variant.extern_symbol_descr->
                                variant.routine.ptr = routine_ptr;
  }  /* if */
  if (any_deferred_access_checks()) {
    /* Now that we know which function has been declared, recheck any
       access errors that occurred while scanning the declaration. */
    perform_deferred_access_checks_for_function(routine_ptr);
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the routine, so update it with the
       current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
  } else {
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
        (!redeclaration && !template_function_specific_decl)) {
      /* Record a reference to the outer-scope symbol of the same name,
         but do not set the IL entity referenced flag. */
      record_symbol_reference(SRK_REFERENCE,
                              (a_symbol_ptr)source_corresp_ptr->assoc_info,
                              &locator->source_position,
                              /*update_il_entry=*/FALSE);
    }  /* if */
    if (!C_mode() &&
        source_corresp_ptr->name_linkage ==
                              (a_name_linkage_kind)nlk_external) {
      /* An extern "C" declaration. */
      a_symbol_ptr  other_sym = (a_symbol_ptr)(source_corresp_ptr->assoc_info);

      if (depth_innermost_function_scope == NO_SCOPE_DEPTH &&
          !is_function_def && !redeclaration &&
          !template_function_specific_decl) {
        /* If the original declaration was a block extern declaration, reset
           the assoc_info pointer to refer to the current declaration -- which
           should be the first non-block-extern declaration of the entity. */
        if (other_sym->parent.namespace_ptr != NULL ||
            other_sym->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
          /* The symbol specified by the assoc_info pointer does not belong
             to a function scope. */
        } else {
          a_boolean  saved_referenced_flag = source_corresp_ptr->referenced;

          set_source_corresp(source_corresp_ptr, sym);
          source_corresp_ptr->referenced = saved_referenced_flag;
          source_corresp_ptr->parent.namespace_ptr = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE &&
      !redeclaration && !template_function_specific_decl &&
      !explicit_template_reference) {
    /* Set the namespace parent in the symbol and IL entry. */
    add_namespace_parent_pointer(sym, source_corresp_ptr);
  }  /* if */
  if (changed_to_inline) {
    if (routine_ptr->called) {
      pos_sy_remark(ec_called_function_redeclared_inline,
                    &locator->source_position, sym);
    }  /* if */
  }  /* if */
  if (linkage != idl_none) {
    /* In case this is a block extern declaration, clear the
       is_local_to_function flag -- it will have been set based on scope
       alone in set_source_corresp. */
    source_corresp_ptr->is_local_to_function = FALSE;
  }  /* if */
  if (func_info->is_main_function) {
    /* This is "main", so remember the location of its routine entry. */
    if (il_header.main_routine == NULL) {
      il_header.main_routine = routine_ptr;
    } else {
      /* Unless there's an error there cannot be two "main" functions. */
      check_assertion_str(il_header.main_routine == routine_ptr ||
                            total_errors > 0,
                          "decl_routine: main redeclared");
    }  /* if */
  }  /* if */
  /* The name linkage has already been determined.  Apply it to the current
     declaration, and report inconsistencies, if appropriate. */
  set_name_linkage(&idlb, sym, source_corresp_ptr, *ext_sym,
                   &locator->source_position);
#if BACK_END_IS_CP_GEN_BE
  if (!C_mode()) {
    /* Set the "name linkage environment" for this routine.  This is used by
       the C++-generating back end in cases like the following:
         extern "C" {
           static void f() { extern void g(); }
         }
       where the extern "C" block must be regenerated so that g() has C
       linkage.  */
    routine_ptr->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (overload_symbol != NULL &&
      sym == overload_symbol->variant.overloaded_function.symbols) {
    /* sym has been newly added to an overload set that may include symbols
       introduced by using-declarations.  Check whether any of the latter
       have the same type as the current function -- report the error and
       remove the offending projection symbol(s) (to avoid overload ambiguity
       errors later on). */
    check_for_conflicts_with_using_decls(overload_symbol,
                                         &locator->source_position);
  }  /* if */
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the declarator_ssep
     entry. */
  record_symbol_declaration(srk_flags, sym, &locator->source_position,
                            declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (is_function_def || first_decl) {
    update_decl_pos_info(&routine_ptr->source_corresp, decl_pos_block);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (is_function_def && is_friend_decl) {
    /* Mark this function as defined in a friend declaration. */
    routine_ptr->defined_in_friend_decl = TRUE;
  }  /* if */
  /* Restore the scope stack. */
  if (idlb.namespace_reactivated)  {
    if (is_friend_decl) {
      pop_namespace_reactivation_scope();
    } else {
      pop_namespace_extension_scope();
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* Apply the attributes to the routine. */
  apply_attributes_to_routine(attributes, routine_ptr);
  /* Record the assembly name. */
  routine_ptr->asm_name = asm_name;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Do fixup on the source sequence entry that was just created to
     represent the current declaration.  Note that declaration_ssep is not
     used, since it may have been replaced (e.g., when a file scope entity
     is declared in a local scope and a sublist is generated). */
  if (is_function_def) {
    /* The defining declaration of the function.  Set a pointer to the
       declared type. */
    set_routine_declared_type(routine_ptr, func_info->declared_type);
    if (is_friend_decl) {
      /* If there were any default arguments, they still need to be scanned.
         Enable the default-arg fixup processing to find the declared type. */
      func_info->declared_type = routine_ptr->declared_type;
    }  /* if */
    if (qualifier_namespace_ptr(*locator) != NULL) {
      check_assertion(!is_friend_decl || locator->is_error);
      routine_ptr->defined_outside_of_parent = TRUE;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      routine_ptr->source_corresp.marked_as_gnu_extension = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  if (!is_function_def
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
      || func_info->is_movable_member_or_friend_def
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
                                                   ) {
    /* Set the type in the secondary declaration entry. */
    /* Note that the is_movable_member_or_friend_friend_def flag is set for
       non-member friend function definitions where the source-sequence
       entry that is put out within the class definition is a secondary-decl;
       the primary source sequence entry is put out after the class definition
       is complete. */
    an_sssd_flag_set              flags = SSSD_NO_FLAGS;
    a_type_ptr                    declared_type;

#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
    if (func_info->is_movable_member_or_friend_def) {
      /* Remove default arguments, if any, from the type associated with
         the secondary source-sequence entry; they will appear on the
         source-sequence entry for the definition instead.  (If they were
         repeated the C++-generating back end would put out invalid code.) */
      declared_type =
             routine_type_without_default_args(routine_ptr->declared_type);
    } else
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
    /* Do not insert code here. */
    {
      /* Normal case. */
      declared_type = func_info->declared_type;
    }  /* if */
    if (is_friend_decl) flags |= SSSD_FRIEND_DECL;
    if (func_info->is_implicit_declaration) flags |= SSSD_IMPLICIT_DECL;
    if (first_decl) flags |= SSSD_FIRST_DECLARATION;
#if GNU_EXTENSIONS_ALLOWED
    if (decl_modifiers->marked_as_gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    (void)update_src_seq_secondary_decl((char *)routine_ptr, declared_type,
                                        flags, decl_pos_block);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (is_function_def) {
    /* If a lint-style "argsused" or "varargs" comment appeared, record that in
       the function type.  That will suppress any warnings about unused
       parameters or variable arguments.  Note that this is done before calling
       process_curr_construct_pragmas; otherwise the pragmas we're interested
       in would have been disposed of. */
    record_lint_argsused_and_varargs_state(sym);
    if (!C_mode() && !exceptions_enabled && !func_info->is_inline &&
        func_info->throw_position.seq != 0) {
      /* Issue a diagnostic on attempting to define a noninline function with
         an exception specification when exception support is not enabled.
         (No diagnostic is issued on nondefinition -- the exception
         specification is just ignored.) */
      pos_error(ec_no_exception_support, &func_info->throw_position);
    }  /* if */
    if (c99_mode) {
      /* In C99 mode, save the current settings of the predefined pragmas. */
      routine_ptr->fp_contract = curr_fp_contract_state;
      routine_ptr->fenv_access = curr_fenv_access_state;
      routine_ptr->cx_limited_range = curr_cx_limited_range_state;
    }  /* if */
  }  /* if */
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration.  Note that this has to be *after* the
     scope stack is restored, since processing depends on the pending_pragmas
     pointer in the scope stack entry. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  /* Return symbol and linkage pointers. */
  *symbol_ptr = sym;
  *linkage_ptr = linkage;

#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_routine */


#if !DECL_MODIFIERS_IN_USE
/* ARGSUSED */ /* decl_modifiers is not used in some configurations. */
#endif /* !DECL_MODIFIERS_IN_USE */
void decl_function_template(a_symbol_locator            *locator,
                            a_type_ptr                  type_ptr,
                            a_func_info_block           *func_info,
                            a_symbol_ptr                *symbol_ptr,
                            a_storage_class             storage_class,
                            a_decl_modifiers_block_ptr  decl_modifiers,
                            a_template_decl_info_ptr    templ_decl_info,
                            a_scope_depth               orig_decl_level,
                            a_boolean                   is_specialization)
/*
Roughly speaking, this routine does for function templates what
decl_routine does for ordinary functions.  Look up and reuse or else
create a function template symbol; for new symbols also create a routine
entry (though one that is not added to the IL).  *locator represents the
current identifier, type_ptr is the function type, storage_class is the
storage class, if any, specified in the declaration, and is_inline is TRUE
if "inline" was specified in the declaration.  The function template may
be part of an overload set, it may have been previously declared (but not
defined), and it may be an out-of-line definition of a member function of a
class template.  orig_decl_level is the nearest enclosing scope that
is not a template declaration scope.  is_specialization is TRUE if this
is a template specialization declaration.
*/
{
  a_symbol_ptr                      sym = NULL;
  a_symbol_ptr                      overload_symbol = NULL;
  a_symbol_ptr                      homonym_symbol = NULL;
  a_symbol_ptr                      rout_sym, ext_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_routine_ptr                     rout_ptr, rp;
  a_memory_region_number            region_to_switch_back_to;
  a_boolean                         changed_to_inline = FALSE;
  a_boolean                         set_invisible = FALSE;
  a_boolean			    in_prototype_instantiation;
#if DECL_MODIFIERS_IN_USE
  a_boolean			    redeclaration = FALSE;
#endif /* DECL_MODIFIERS_IN_USE */
  an_id_linkage_block      idlb;
  a_boolean  microsoft_out_of_class_redecl;
  a_boolean  proxy_member_friend = FALSE;

  db_enter(3, "decl_function_template");
  check_assertion(scope_stack[depth_scope_stack].kind ==
                                (a_scope_kind)sck_template_declaration);
  if (func_info->is_inline && !extern_inline_allowed) {
    storage_class = (a_storage_class)sc_static;
  } else if (storage_class == (a_storage_class)sc_unspecified) {
    /* Default. */
    storage_class = (a_storage_class)sc_extern;
  }  /* if */
  clear_id_linkage_block(&idlb);
  idlb.templ_param_list = templ_decl_info->parameters;
  idlb.storage_class = storage_class;
  idlb.func_info = func_info;
  idlb.is_function_template = TRUE;
  idlb.type = type_ptr;
  idlb.locator = locator;
  set_linkage_environment(&idlb, orig_decl_level);
  in_prototype_instantiation =
            scope_stack[idlb.effective_decl_level].in_prototype_instantiation;
  if (idlb.is_friend_decl && !friend_injection_enabled) {
    set_invisible = TRUE;
  }  /* if */
  if (locator->is_qualified_name && locator->is_class_member &&
      locator->specific_symbol != NULL) {
    a_type_ptr		parent_class;
    a_symbol_ptr	parent_class_sym;
    /* Member function template. */
    sym = locator->specific_symbol;
    parent_class = sym->parent.class_type;
    parent_class_sym = (a_symbol_ptr)parent_class->source_corresp.assoc_info;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (is_nontype_template_param_symbol(sym)) {
      /* A name like "A<T>::x" that is a nontype member of a proxy class.
         This should only happen in friend templates. */
      check_assertion(locator->is_class_member);
      if (!idlb.is_friend_decl) {
        pos_stsy_error(ec_not_a_member, &locator->source_position,
                       sym->header->identifier, parent_class_sym);
        set_to_error_locator(*locator);
      } else if (curr_token != tok_semicolon) {
        pos_sy_error(ec_bad_scope_for_definition,
                     &locator->source_position, sym);
        set_to_error_locator(*locator);
      }  /* if */
      sym = NULL;
      proxy_member_friend = TRUE;
    } else if (sym->kind != (a_symbol_kind)sk_member_function &&
               sym->kind != (a_symbol_kind)sk_function_template &&
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
      sym = member_function_redecl_sym(sym, type_ptr, idlb.templ_param_list);
      if (sym != NULL) {
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* This is the symbol for a member template function.  Use
             this symbol. */
        } else if (is_prototype_instantiation_symbol(parent_class_sym)) {
          /* This is a member function symbol of a prototype instantiation.
             Get the associated function template. */
          sym = get_member_function_template_symbol(sym);
        } else {
          /* Not a template or a member of a prototype instantiation --
             this must be an error. */
          sym = NULL;
        }  /* if */
      }  /* if */
      if (sym == NULL) {
        /* No member function with a matching type was found.  Issue an
           error. */
        pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_no_match_for_type_of_overloaded_function :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
        set_to_error_locator(*locator);
      } else {
        /* Merge type information from the two declarations. */
        a_type_ptr	prev_type;
        tssp = template_supplement_for_symbol(sym);
        prev_type = tssp->variant.function.routine->type;
        adjust_member_routine_type(type_ptr, prev_type);
        reconcile_routine_types(tssp->variant.function.routine, type_ptr,
                                /*preserve_rout_type=*/TRUE,
                                /*preserve_type_ptr=*/FALSE);
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
    } else if (is_single_param_operator_new_or_delete(locator, type_ptr)) {
      /* Overloading should not be allowed on the single-argument version of
         operator new(size_t) or operator delete(void *).  Though it is not
         expressly prohibited, it can be inferred from the fact that new and
         delete have an invariant first argument.  At least one C++ test
         suite expects an error. */
      pos_error(is_new_operator(locator->variant.opname) ?
                    ec_template_operator_new : ec_template_operator_delete,
                &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace || curr_token == tok_try ||
      (curr_token == tok_colon && sym != NULL && is_constructor_symbol(sym))) {
    /* This is a defining declaration of the function template. */
    func_info->is_definition = TRUE;
    idlb.is_definition = TRUE;
    if (func_info->function_type_from_typedef) {
      /* Just as it is an error when a normal function is defined for the
         function type to come from a typedef, so too is that an error when
         a function template is being defined. */
      error(ec_function_type_must_come_from_declarator);
      /* Copy the type entry, since the typedef type may not be shared. */
      type_ptr = copy_routine_type_with_param_types(skip_typerefs(type_ptr),
                                                   /*copy_default_args=*/TRUE);
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    /* Must be a member template. */
    if (sym->is_class_member && idlb.is_friend_decl &&
        func_info->is_definition) {
      /* A class member function cannot be defined in a friend declaration. */
      pos_sy_error(ec_bad_scope_for_definition,
                   &locator->source_position, locator->specific_symbol);
    } else if (idlb.is_friend_decl &&
               (!func_info->is_definition || in_prototype_instantiation)) {
      /* Don't check the scope if this is a friend declaration unless it
         is a definition during a real instantiation. */
    } else if (!namespace_is_enclosed_by_scope(sym,
                                               &scope_stack[idlb.
                                                    effective_decl_level])) {
      /* This member template is being defined in a scope that does not
         enclose the scope in which the parent class was defined. */
      if (is_specialization) {
        sym_error(ec_bad_scope_for_specialization, sym);
      } else if (func_info->is_definition) {
        sym_error(ec_bad_scope_for_definition, sym);
      } else {
        sym_error(ec_bad_scope_for_redeclaration, sym);
      }  /* if */
    }  /* if */
  } else if (locator->specific_symbol != NULL &&
             (qualifier_namespace_ptr(*locator) != NULL ||
              locator->is_file_scope_qualified_name)) {
    /* This identifier is a namespace-qualified name that was previously
       declared, or else a file-scope qualified name (friend declarations
       only).  Do the appropriate checking, including overload resolution.
       Furthermore, for definitions of namespace-qualified names, be sure
       this is a valid scope for the definition (7.3.1.4). */
    /* Look up the name. */
    if (in_prototype_instantiation) {
      /* Don't try to find a matching qualified name for a friend declaration
         in a prototype instantiation. */
    } else {
      qualified_name_redecl_sym(&idlb);
      sym = idlb.linked_symbol;
      if (sym != NULL && sym->kind != (a_symbol_kind)sk_function_template) {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
        set_to_error_locator(*locator);
        sym = NULL;
      }  /* if */
      homonym_symbol = idlb.homonym_symbol;
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    tssp = template_supplement_for_symbol(sym);
    rout_ptr = tssp->variant.function.routine;
  } else {
    if (scope_stack[idlb.effective_decl_level].in_prototype_instantiation) {
      /* Suppress lookup of friend template declarations during prototype
         instantiation. */
    } else {
      /* id_linkage will set sym to point to an existing symbol when we have
         a redeclaration of a function template. */
      id_linkage(&idlb);
      sym = idlb.linked_symbol;
      homonym_symbol = idlb.homonym_symbol;
      overload_symbol = idlb.overload_symbol;
      if (sym != NULL && sym->kind != (a_symbol_kind)sk_function_template) {
        /* Invalid redeclaration. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
        sym = NULL;
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* Not a redeclaration. */
      a_scope_stack_entry_ptr  ssep = &scope_stack[idlb.effective_decl_level];
      an_error_code            error_code;
      a_boolean                invalid_scope_for_new_or_delete = FALSE;

      if (!is_error_locator(*locator)) {
        /* If this is an overloaded operator, check for errors in the
           argument list.  Note that this check is not done for redeclarations,
           on the assumption that once will have been enough. */
        check_assertion(!locator->is_class_member || proxy_member_friend);
        check_operator_function_params(type_ptr, (a_type_ptr)NULL, locator);
        /* If it's a new or delete operator, be sure the scope is not a
           namespace scope. */
        report_bad_new_or_delete(locator, storage_class,
                                 &invalid_scope_for_new_or_delete);
      }  /* if */
      check_default_args(type_ptr);
      if (homonym_symbol != NULL &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    idlb.templ_param_list, &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        set_to_error_locator(*locator);
        /* Avoid overloading. */
        homonym_symbol = NULL;
      }  /* if */
      if (proxy_member_friend ||
          (in_prototype_instantiation && locator->is_qualified_name)) {
        /* A member template of a (dependent) proxy class was named as a
           friend or a qualified friend declaration in a prototype
           instantiation.  Create a dummy symbol for it (it will not be
           linked into the symbol table) and configure it with the appropriate
           parent. */
        sym = alloc_symbol((a_symbol_kind)sk_function_template,
                           locator->symbol_header, &locator->source_position);
        if (locator->is_class_member) {
          set_class_membership(sym, (a_source_correspondence_ptr)NULL,
                               locator->parent.class_type);
        } else if (locator->parent.namespace_ptr != NULL) {
          set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                   locator->parent.namespace_ptr);
        }  /* if */
        sym->is_error = locator->is_error;
      } else if (homonym_symbol != NULL) {
        /* Another function with the same name has been declared already.  It
           may or may not be a function template.  In any case, create a new
           symbol and add it to an overload list. */
        a_boolean  overload_set_is_invisible = FALSE;

        check_assertion(!microsoft_mode || !invalid_scope_for_new_or_delete);
        if (set_invisible && homonym_symbol->is_invisible) {
          overload_set_is_invisible = TRUE;
        }  /* if */
        sym = enter_overloaded_symbol((a_symbol_kind)sk_function_template,
                                      locator, /*is_constructor=*/FALSE,
                                      homonym_symbol, &overload_symbol);
        if (set_invisible) {
          sym->is_invisible = TRUE;
          if (overload_set_is_invisible) {
            overload_symbol->is_invisible = TRUE;
          }  /* if */
        } else if (!idlb.is_friend_decl) {
          overload_symbol->is_invisible = FALSE;
        }  /* if */
      } else {
        /* No overloading.  Simply create a new symbol. */
        sym = enter_local_symbol((a_symbol_kind)sk_function_template, locator,
                                 idlb.effective_decl_level,
                                 /*suppress_redecl_error=*/FALSE);
        if (microsoft_mode && invalid_scope_for_new_or_delete) {
          /* The Microsoft C++ compiler permits declaring a new or delete
             function template in a namespace scope, but it doesn't actually
             find the template when processing new and delete expressions.
             We emulate this behavior by removing the symbol from the symbol
             table. */
          remove_symbol(sym);
        }  /* if */
        /* Mark friend functions for which this is the initial declaration. */
        if (set_invisible) sym->is_invisible = TRUE;
      }  /* if */
      tssp = template_supplement_for_symbol(sym);
      if (tssp->variant.function.decl_cache.decl_info == NULL) {
        /* If this is the initial declaration of this template, set the
           template cache information to point to the template declaration
           information that was passed in.  This must be done now so that
           things like the template parameter list will be available to
           other routines that are called below.  This includes the
           diagnostic routines that make use of the template parameter
           list in diagnostic output. */
        set_template_cache_info(&tssp->variant.function.decl_cache,
                                (a_token_cache_ptr)NULL,
                                templ_decl_info);
      }  /* if */
      rout_ptr = NULL;
      /* Set namespace membership on this template function. */
      if (idlb.is_friend_decl && ssep->in_prototype_instantiation) {
        ssep = &scope_stack[depth_innermost_namespace_scope];
      }  /* if */
      if (ssep->kind == (a_scope_kind)sck_namespace ||
          ssep->kind == (a_scope_kind)sck_namespace_extension) {
        set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                 ssep->il_scope->variant.assoc_namespace);
      }  /* if */
    } else {
      a_param_type_ptr  ptp;

      check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
      tssp = template_supplement_for_symbol(sym);
      /* Discard any previously created substituted type entries.  These
         may no longer be valid as a result of the redeclaration. */
      free_list_of_substituted_type_list_entries(
                                     tssp->variant.function.substituted_types);
      tssp->variant.function.substituted_types = NULL;
      rout_ptr = tssp->variant.function.routine;
      /* Declaring a default argument on a function template redeclaration is
         nonstandard.  Issue at least a warning, and always an error if the
         template has already been instantiated. */
      for (ptp = type_ptr->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        if (ptp->has_default_arg) {
          an_error_severity  severity;
          an_error_code      error_code;
          if (tssp->variant.function.instantiations != NULL) {
            severity = es_error;
            error_code = ec_default_arg_on_function_template_not_allowed;
          } else {
            if (strict_ansi_mode) {
              severity = strict_ansi_error_severity;
            } else {
              severity = es_warning;
            }  /* if */
            error_code = ec_nonstd_default_arg_on_function_template_redecl;
          }  /* if */
          pos_diagnostic(severity, error_code, &locator->source_position);
          break;
        }  /* if */
      }  /* for */
      /* Merge type information from the two declarations. */
      reconcile_routine_types(rout_ptr, type_ptr,
                              /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE);
      /* If appropriate, clear the is_invisible flag in the symbol and
         in the symbol representing its overload set. */
      if (sym->is_invisible && !idlb.is_friend_decl) {
        sym->is_invisible = FALSE;
        if (sym->overload_set_member) {
          check_assertion(overload_symbol != NULL);
          overload_symbol->is_invisible = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* A routine entry is created for the function template, but it is not
     entered in the IL.  It is a convenient place to keep track of prototype
     information: type, storage class, etc.  These values may be reused
     when the template is instantiated.  This routine entry will not, of
     course, have a body associated with it. */
  if (rout_ptr == NULL) {
    a_symbol_ptr	prototype_sym;
    switch_to_file_scope_region(&region_to_switch_back_to);
    tssp->variant.function.routine = rout_ptr = alloc_routine();
    switch_back_to_original_region(region_to_switch_back_to);
    rout_ptr->type = type_ptr;
    rout_ptr->storage_class = storage_class;
    rout_ptr->is_inline = func_info->is_inline;
    if (locator->is_operator_name) {
      rout_ptr->special_kind = (a_special_function_kind)sfk_operator;
      rout_ptr->opname_or_builtin.opname_kind = locator->variant.opname;
    }  /* if */
    check_assertion(is_error_locator(*locator) ||
                    !locator->is_conversion_name);
    /* Allocate the symbol for the prototype instantiation of the
       function template. */
    prototype_sym = make_function_template_prototype_symbol(
                                         sym, rout_ptr, idlb.templ_param_list);
    set_source_corresp(&rout_ptr->source_corresp, prototype_sym);
    set_membership_in_source_corresp(&(rout_ptr->source_corresp),
				     prototype_sym);
    rout_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_extern) ?
                                (a_name_linkage_kind)nlk_cplusplus_external :
                                (a_name_linkage_kind)nlk_internal;
    /* Call a routine that manages the correspondence of entities between
       translation units to notify it of the new instance. */
    record_instantiation(prototype_sym, tssp);
    if (prototype_instantiations_in_il && !locator->is_error) {
      /* Normally, we let add_to_routines_list determine which scope to add
         the routine to, but for proxy members nominated in friends, that
         would yield a nonexisting scope; instead we just put those on the
         file scope list. */
      add_to_routines_list(rout_ptr,
                           proxy_member_friend ? DEPTH_OF_FILE_SCOPE :
                                                 NO_SCOPE_DEPTH);
    }  /* if */
  } else {
    if (func_info->is_inline) {
      if (!rout_ptr->is_inline) {
        rout_ptr->is_inline = TRUE;
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
    /* Be sure the current throw specification is consistent with the one
       on the previous declaration. */
    check_exception_specification(type_ptr, sym, &func_info->throw_position,
                                  /*is_redecl=*/TRUE);
#if DECL_MODIFIERS_IN_USE
    redeclaration = TRUE;
#endif /* DECL_MODIFIERS_IN_USE */
  }  /* if */
  microsoft_out_of_class_redecl = microsoft_mode && sym->is_class_member &&
                                                    !func_info->is_definition;
  if (!is_error_locator(*locator)) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Prevent the generation of a source sequence entry for the a_template
       entry: we already did so elsewhere. */
    a_boolean saved_sses_disallowed = source_sequence_entries_disallowed;
    source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (func_info->is_definition) {
      if (sym->defined) {
        pos_sy_error(ec_already_defined, &locator->source_position, sym);
      } /* if */
      mark_defined(sym, &locator->source_position);
    } else if (!microsoft_out_of_class_redecl) {
      mark_declared(sym, &locator->source_position);
      if (!microsoft_mode &&
          sym->is_class_member && !idlb.is_friend_decl && !is_specialization) {
        /* A non-defining declaration of a member function is only allowed
           in Microsoft mode. */
        pos_sy_error(ec_member_function_redecl_outside_class,
                     &locator->source_position, sym);
      } /* if */
    } /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Restore the previous state wrt. the generation of source sequence
       entries. */
    source_sequence_entries_disallowed = saved_sses_disallowed;
    if (func_info->is_definition) {
      set_routine_declared_type(rout_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } /* if */
  update_routine_decl_modifiers(rout_ptr, decl_modifiers,
                                &locator->source_position, redeclaration,
                                (a_boolean)func_info->is_definition,
                                (a_boolean)func_info->is_inline);
  if (overload_symbol != NULL && guiding_decls_allowed) {
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
    for (rout_sym = overload_symbol->variant.overloaded_function.symbols;
         rout_sym != NULL;
         rout_sym = rout_sym->next) {
      if (rout_sym->kind == (a_symbol_kind)sk_routine) {
        /* Determine whether rout_sym is a specialization of the function
           template represented by sym. */
        record_predeclared_template_function(sym, rout_sym,
                                             idlb.templ_param_list);
      }  /* if */
    }  /* for */
  }  /* if */
  if (changed_to_inline) {
    /* An existing template function has been redeclared and this time it's
       inline.  Be sure that "inline" and storage class are propagated
       through the instances. */
    a_template_instance_ptr  tip = tssp->variant.function.instantiations;
    for (; tip != NULL; tip = tip->next) {
      rp = tip->instance_sym->variant.routine.ptr;
      if (rp->is_specialized) {
        /* An explicit specialization is only inline if so declared. */
      } else {
        if (!extern_inline_allowed &&
            rp->storage_class != (a_storage_class)sc_static) {
          /* Issue a warning on linkage inconsistency only on nonmember
             function templates. */
          if (!sym->is_class_member) {
            sym_warning(ec_template_and_instance_linkage_conflict,
                        tip->instance_sym);
          }  /* if */
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
        }  /* if */
        /* Issue a diagnostic is the function has already been called. */
        if (!rp->is_inline && rp->called) {
          sym_remark(ec_called_function_redeclared_inline,
                     tip->instance_sym);
        }  /* if */
        rp->is_inline = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!sym->is_class_member && guiding_decls_allowed) {
    /* The overload list for the current scope has been searched for previous
       declarations that now appear to be instances of the template, but we
       also need to check for block-extern declarations that fall into the
       same category.  Issue an error if any such routine was actually used
       in a call. */
    for (ext_sym = sym->header->other_symbols;
         ext_sym != NULL;
         ext_sym = ext_sym->next) {
      /* The decl_scope test is used to exclude symbols that are not for
         the current translation unit. */
      if (ext_sym->kind == (a_symbol_kind)sk_extern_routine &&
          ext_sym->parent.namespace_ptr == sym->parent.namespace_ptr &&
          ext_sym->decl_scope == file_scope_number) {
        /* A routine belonging to the same namespace.  Don't check on the
           the type before determining that there is no instance pointer
           (i.e., it didn't appear in the search of the overload set) and
           it was referenced. */
        rp = ext_sym->variant.extern_symbol_descr->variant.routine.ptr;
        rout_sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
        if (rp->source_corresp.referenced &&
            rout_sym->variant.routine.instance_ptr == NULL) {
          /* This must be a block-extern declaration.  Check whether it is
             an instance of the function templates represented by sym. */
          a_type_ptr          tp = skip_typerefs(rp->type);
          a_template_arg_ptr  templ_arg_list;
          a_symbol_ptr        dummy;

          if (is_match_for_function_template(sym, tp, &templ_arg_list, &dummy,
                                             idlb.templ_param_list,
                                             (a_template_arg_ptr)NULL,
                                             /*is_decl_context=*/TRUE)) {
            sym_error(ec_template_instance_already_used,
                      (a_symbol_ptr)rp->source_corresp.assoc_info);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (arg_dependent_lookup_enabled && sym->is_invisible &&
      idlb.is_friend_decl) {
    a_scope_stack_entry_ptr ssep = &scope_stack[orig_decl_level];

    check_assertion(!sym->is_class_member || sym->is_error);
    check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
    add_friend_function_to_lookup_list_for_class(sym, ssep->il_scope->
                                                        variant.assoc_type);
  }  /* if */
  /* Restore the scope stack. */
  if (idlb.namespace_reactivated)  {
    if (idlb.is_friend_decl) {
      pop_namespace_reactivation_scope();
    } else {
      pop_namespace_extension_scope();
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


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void define_static_data_member(a_symbol_locator   *locator,
                                      a_storage_class    storage_class,
                                      a_type_ptr         type_ptr,
                                      a_boolean          has_initializer,
                                      a_source_sequence_entry_ptr  ssep,
                                      a_symbol_ptr       *symbol_ptr,
                                      an_id_linkage_kind *linkage_ptr,
                                      a_decl_pos_block   *decl_pos_block)
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
  a_variable_ptr           var;
  a_boolean                err = FALSE;
  a_symbol_ptr             sym;
  a_symbol_reference_kind  srk_flags;
  a_boolean                incompatible_ptr_to_member_class_types = FALSE;

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
  if (microsoft_mode && sym->kind == (a_symbol_kind)sk_projection) {
    /* In Microsoft compatibility mode it's permitted to define a static
       data member by referring to it as an inherited member.  However, only
       allow this if the reference is unambiguous. */
    if (!sym->ambiguous) sym = fundamental_symbol_of(sym);
  }  /* if */
  if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    var = sym->variant.static_data_member.variable;
    if (sym->defined) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
      err = TRUE;
    } else if (!namespace_is_enclosed_by_scope(sym,
                                           &scope_stack[depth_scope_stack])) {
      /* This static data member is being defined in a scope that does not
         enclose the scope in which the parent class was defined. */
      sym_error(ec_bad_scope_for_definition, sym);
      err = TRUE;
    } else if (!types_are_redecl_compatible(type_ptr, var->type)) {
      /* Types are not compatible. */
      if (microsoft_bugs &&
          f_types_are_compatible(type_ptr, var->type,
                                 TCF_REDECLARATION |
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |
                                 TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE)) {
        /* The incompatibility amounts to some difference in the class type
           specified in a pointer to member type that is part of the type of
           the static data member.  This is allowed in Microsoft-bugs mode. */
        pos_sy_warning(ec_not_compatible_with_previous_decl,
                       &locator->source_position, sym);
        incompatible_ptr_to_member_class_types = TRUE;
      } else {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
        err = TRUE;
      }  /* if */
    } else if ((is_ptr_or_ref_type(type_ptr) &&
                is_function_type(type_pointed_to(type_ptr))) ||
               (is_ptr_to_member_type(type_ptr) &&
                is_function_type(pm_member_type(type_ptr)))) {
      /* Check for mismatches in exception specifications. */
      check_exception_specification(type_ptr, sym, &locator->source_position,
                                    /*is_redecl=*/TRUE);
    }  /* if */
    if (!err) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Since this is the defining declaration of the static data member,
         record the type.  Note that this has to be done before composite
         type is called -- in case there's some modification. */
      check_assertion(var->declared_type == NULL);
      var->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      if (incompatible_ptr_to_member_class_types) {
        /* Microsoft bug -- leave the static data member type (or for
           incomplete arrays the underlying array element type) as it was
           originally declared. */
        if (is_array_type(var->type)) {
          a_type_ptr     array_type, new_type;

          check_assertion(is_array_type(type_ptr));
          array_type = skip_typerefs(var->type);
          if (array_type->size == 0) {
            /* The static data member was originally declared as an array
               of unknown size.  Make a copy of the original type, using
               the size from the current type.  (We have to do it this way
               instead of calling composite_type because the two types are
               not actually compatible.) */
            new_type = alloc_type((a_type_kind)tk_array);
            copy_type(array_type, new_type);
            new_type->variant.array.variant.number_of_elements =
                       skip_typerefs(type_ptr)->
                              variant.array.variant.number_of_elements;
            set_type_size(new_type);
            /* Update the variable entry to point to the new type. */
            var->type = new_type;
          }  /* if */            
        }  /* if */
      } else {
        /* The type of the variable should be the composite of the two
           types. */
        var->type = composite_type(type_ptr, var->type);
      }  /* if */
      /* Ordinarily a static data member will have been given a storage class
         of sc_extern; promote it to sc_unspecified, now that the definition
         has been seen.  (In cfront mode the storage class is promoted from
         sc_static at the end of the translation unit.) */
      if (var->storage_class == (a_storage_class)sc_extern) {
        var->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      /* Set the IL referenced flag since, as an externally visible variable,
         it could be referenced from another translation unit. */
      var->source_corresp.referenced = TRUE;
      /* If this is a member of an instantiation of a class
         template, set the supress_instantiation field of the variable. */
      if (sym->variant.static_data_member.instance_ptr != NULL) {
        check_old_specialization_allowed(sym, &locator->source_position);
        var->is_specialized = TRUE;
        var->specialized_with_old_syntax = TRUE;
      }  /* if */
      srk_flags = SRK_DECLARATION | SRK_DEFINITION;
      /* Even without an explicit initializer this is an initializing
         declaration it is the static data member is nontrivially
         constructible -- i.e., if it is a class object (or array of class)
         and the class has a nontrivial default constructor (which must be a
         user-declared default constructor if the static data member's type
         is const qualified -- WP 7.1.5.1 [dcl.type.cv]). */
      if (has_initializer ||
          is_const_qualified_type(var->type) ?
            type_has_user_declared_default_constructor(var->type) :
            type_has_nontrivial_default_constructor(var->type)) {
        srk_flags |= SRK_INITIALIZATION;
      }  /* if */
      record_symbol_declaration(srk_flags, sym, &locator->source_position,
                                ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      update_decl_pos_info(&var->source_corresp, decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
    a_type_ptr           tp = sym->parent.class_type;
    a_symbol_header_ptr  hdr = locator->symbol_header;
    a_variable_ptr       vp;

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
    vp = make_variable(error_type(), (a_storage_class)sc_static,
                       depth_innermost_namespace_scope);
    sym->variant.static_data_member.variable = vp;
    /* Make the error symbol a class member -- it is expected of
       sk_static_data_member symbols downstream. */
    set_class_membership(sym, &vp->source_corresp, tp);
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


static void remove_any_inherited_type_synonym(a_symbol_locator  *locator)
/*
Microsoft compilers accept code like:
  struct B { typedef int I; };
  struct D: B {
    typedef I J;      // Uses B::I
    typedef double I; // Introduces D::I
  };
To emulate this, we must remove projections of a type synonymous with the
type being declared.
*/
{
  if (curr_scope_id_lookup(locator, IDL_PROJ_SYMBOL_ALLOWED) != NULL) {
    a_symbol_ptr  sym = locator->specific_symbol;

    if (sym->kind == (a_symbol_kind)sk_projection &&
        !sym->variant.projection.is_using_decl) {
      remove_symbol(sym);
    }  /* if */
    clear_specific_symbol(*locator);
  }  /* if */
}  /* remove_any_inherited_type_synonym */


static void set_name_linkage_for_enumerators(a_type_ptr  tp)
/*
The given type should be an enumeration type.  This routine ensures its
associated enumerator constants are assigned the same name linkage as the
type itself.
*/
{
  a_constant_ptr  enumerator = tp->variant.integer.enum_info.constant_list;

  for (; enumerator != NULL; enumerator = enumerator->next) {
    enumerator->source_corresp.name_linkage = tp->source_corresp.name_linkage;
  }  /* for */
}  /* set_name_linkage_for_enumerators */


static void set_linkage_for_class_members(a_type_ptr  tp)
/*
The given type should be a class type.  If it acquired linkage through a
typedef, we must make sure to propagate that to its members.
*/
{
  a_scope_ptr          scope;
  a_routine_ptr        routine;
  a_variable_ptr       var;
  a_type_ptr           type;
  a_name_linkage_kind  name_linkage = tp->source_corresp.name_linkage;

  check_assertion(!C_mode() && is_immediate_class_type(tp));
  scope = tp->variant.class_struct_union.extra_info->assoc_scope;
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    routine->source_corresp.name_linkage = name_linkage;
    if (name_linkage == (a_name_linkage_kind)nlk_cplusplus_external ||
        name_linkage == (a_name_linkage_kind)nlk_external) {
      routine->storage_class = (a_storage_class)
                                  (routine->assoc_scope != NULL_region_number ?
                                                   sc_unspecified : sc_extern);
    }  /* if */
  }  /* for */
  for (var = scope->variables; var != NULL; var = var->next) {
    var->source_corresp.name_linkage = name_linkage;
    if (name_linkage == (a_name_linkage_kind)nlk_cplusplus_external ||
        name_linkage == (a_name_linkage_kind)nlk_external) {
      var->storage_class = (a_storage_class)sc_extern;
    }  /* if */
  }  /* for */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      set_name_linkage_for_type(type);
      set_linkage_for_class_members(type);
    } else if (is_immediate_enum_type(type)) {
      set_name_linkage_for_type(type);
      set_name_linkage_for_enumerators(type);
    }  /* if */
  }  /* for */
}  /* set_linkage_for_class_members */


#if !EXTRA_SOURCE_POSITIONS_IN_IL || !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL.  attributes
                is not used unless GNU extensions are supported. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL || !GNU_EXTENSIONS_ALLOWED */
void decl_typedef(a_symbol_locator             *locator,
                  a_type_ptr                   type_ptr,
                  a_type_ptr                   class_type,
                  an_attribute_ptr             attributes,
                  a_symbol_ptr                 *symbol_ptr,
                  a_source_sequence_entry_ptr  declarator_ssep,
                  a_decl_pos_block_ptr         decl_pos_block)
/*
Enter the declaration of an identifier for a typedef.  *locator gives the
symbol locator (and thus its name and its declaration position).  type_ptr
gives the type.  If this is a member typedef, class_type identifies the
class of which it is a member.  Create and enter a symbol entry, and
return a pointer to it in *symbol_ptr.
*/
{
  a_type_ptr               tp;
  a_symbol_ptr             sym = NULL;
  a_boolean                suppress_redecl_error = FALSE;
  a_boolean                saved_referenced_flag;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
  a_namespace_ptr          nsp;

  db_enter(3, "decl_typedef");
  sym = curr_scope_id_lookup(locator, IDL_PROJ_SYMBOL_ALLOWED);
  if (microsoft_mode && sym == NULL &&
      ssep->kind == (a_scope_kind)sck_class_struct_union) {
    remove_any_inherited_type_synonym(locator);
  }  /* if */
  if (sym != NULL) {
    /* This name already exists in the current scope.  C++ allows a
       redefinition of the typedef with the same type, and we allow that
       also in C.  See if this is a redefinition. */
    if (sym->kind == (a_symbol_kind)sk_type ||
        (C_dialect == C_dialect_cplusplus && is_type_symbol(sym))) {
      /* sym is a type name symbol from the current scope.  Issue an error
         if this is an illegal redefinition of the name; otherwise, reuse
         the existing symbol. */
      a_boolean  types_are_identical;
      tp = type_symbol_type(sym);
      types_are_identical = identical_types(tp, type_ptr);
      if ((types_are_identical
#if NEAR_AND_FAR_ALLOWED
           /* When near/far qualifiers appear, they have to match what was
              explicitly specified. */
           && (!near_and_far_enabled() ||
               (get_original_type_qualifiers(tp) ==
                   get_original_type_qualifiers(type_ptr)))
#endif /* NEAR_AND_FAR_ALLOWED */
                                                           ) ||
          is_error_type(tp) ||
          (microsoft_bugs && C_mode() && is_integral_type(type_ptr) &&
           interchangeable_types(tp, type_ptr))) {
        /* The current declaration simply redefines the name to the same
           type, which is permitted in C++ (ARM 7.1.3) and warned about for
           ordinary C.  In Microsoft C mode we also accept a redeclaration
           to an integral type that is "similar" to the original. */
        /* If this a member type check to be sure the access isn't being
           changed. */
        if (!C_mode() && class_type != NULL && !is_error_type(tp)) {
          check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
          if (tp->source_corresp.access != ssep->current_access) {
            /* Access for previous declaration does not correspond to access
               for current declaration. */
            pos_sy_diagnostic(strict_ansi_mode ?
                                strict_ansi_discretionary_severity :
                                es_warning,
                              ec_cannot_change_access,
                              &locator->source_position, sym);
            /* Stay with the access specified on the original declaration. */
          }  /* if */
        }  /* if */
        /* In C++ we may still need an sk_type symbol, since tags and typedefs
           do not occupy the same name space. */
        if (locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_namespace_projection) {
          /* A typedef declares the same name as a using declaration, and both
             also correspond to the same type.  For example:
               namespace N { typedef int I; }
               using N::I;
               typedef int I;
             Inhibit the redeclaration error.
          */
          sym = NULL;
          suppress_redecl_error = TRUE;
          clear_specific_symbol(*locator);
        } else if (sym->kind == (a_symbol_kind)sk_type) {
          a_symbol_reference_kind  ref_kind = SRK_DECLARATION;

#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode && sym == predeclared_size_t_symbol &&
              !sym->defined) {
            /* This is a redeclaration of the predeclared symbol for size_t.
               We know it's the first explicit declaration because the defined
               flag is not set. */
            ref_kind |= SRK_DEFINITION;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          if (C_mode()) {
            /* Allowing a benign redeclaration is an extension in C, so issue
               a warning. */
            pos_diagnostic(strict_ansi_mode ?
                             strict_ansi_error_severity : es_warning,
                           types_are_identical ?
                             ec_duplicate_typedef : ec_similar_typedef,
                           &locator->source_position);
          }  /* if */
          record_symbol_declaration(ref_kind, sym, &locator->source_position,
                                    declarator_ssep);
          if (!(ref_kind & SRK_DEFINITION)) {  /*lint !e774*/
#if GENERATE_SOURCE_SEQUENCE_LISTS
            (void)update_src_seq_secondary_decl((char *)sym->variant.type.ptr,
                                                type_ptr, SSSD_NO_FLAGS,
                                                decl_pos_block);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          } else {
#if EXTRA_SOURCE_POSITIONS_IN_IL
            /* The first explicit declaration of size_t in Microsoft mode
               (see above).  Set the extended position information. */
            update_decl_pos_info(&sym->variant.type.ptr->source_corresp,
                                 decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          }  /* if */
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
  } else if (C_dialect == C_dialect_cplusplus && !is_error_locator(*locator)) {
    /* No symbol by this name.  See if this is a tagless type for which the
       typedef name will serve as the "name for linkage purposes" (WP 7.1.3
       [dcl.typedef]).  If so, set the name pointer in the type entry to
       point to the same name as the current typedef name. */
    a_boolean  is_class_or_enum = is_immediate_class_type(type_ptr) ||
                                  is_immediate_enum_type(type_ptr);
    tp = NULL;
    if (is_class_or_enum) {
      if (type_ptr->source_corresp.name == NULL) {
        /* A class/struct/union or enum type with no name. */
        tp = type_ptr;
      }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
    } else {
      /* Normally, inferring a linkage name from a typedef name is allowed
         only for unqualified class/struct/union and enum types.  However, in
         cfront's name mangling scheme it is also done when there is a type
         qualifier on top of the tagless class or enum:
           typedef struct { ... } A;         // linkage name "A" (all modes)
           typedef const struct { ... } B;   // linkage name "B" (cfront mode)
           typedef enum { ... } C;           // linkage name "C" (all modes)
           typedef const enum { ... } D;     // linkage name "D" (cfront mode)
      */
      if (is_class_struct_union_type(type_ptr) || is_enum_type(type_ptr)) {
        if (skip_typedefs(type_ptr) == type_ptr &&
            skip_typerefs(type_ptr)->source_corresp.name == NULL) {
          /* A possibly qualified class or enum type with no name.  Get at the
             underlying type. */
          tp = skip_typerefs(type_ptr);
        }  /* if */
      }  /* if */
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
    }  /* if */
    if (tp != NULL) {
      if (any_cfront_mode()) {
        /* An unnamed tag symbol was created for the class and can be reused
           now that we have a name to assign to it.  We need to unlink it
           from the symbol table, give it the name, and relink it into the
           symbol table. */
        sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
        check_assertion(sym != NULL && is_unnamed_tag_symbol(sym));
        relink_unnamed_tag_symbol(sym, locator);
        /* Call set_source_corresp, but preserve the current IL referenced
           setting, which set_source_corresp will clear. */
        saved_referenced_flag = tp->source_corresp.referenced;
        set_source_corresp(&(tp->source_corresp), sym);
        tp->source_corresp.referenced = saved_referenced_flag;
        suppress_redecl_error = TRUE;
#if RECORD_HIDDEN_NAMES_IN_IL
        /* Set the flags directly, since record_symbol_declaration is not
           called. */
        sym->header->any_tag_decl = TRUE;
        if (sym->decl_scope == file_scope_number ||
            (!sym->is_class_member && sym->parent.namespace_ptr != NULL)) {
          sym->header->any_decl_in_file_or_namespace_scope = TRUE;
        }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
        /* Note that we do not look for conflicts between the class's new
           name and the names of its members.  This is an area where the
           wording of the ARM (7.1.3) has been clarified and/or amended by
           the X3J16 working paper, and so the restrictions specified in
           ARM 9.2 do not apply. */
      } else {
        /* The typedef name is the name of the class or enum "for linkage
           purposes".  That means the typedef name should be recorded in the
           source correspondence field for the type.  However, we won't
           reenter the symbol into the symbol table; this keeps the typedef
           name from being used in an elaborated type specifier (7.1.3 para 5,
           9.1 para 5). */
        /* We also assign a name to class and enum types when one of their
           cv-qualified forms has been typedefed: that name is used to
           mangle functions that make use of these types. */
        /* tp cannot point to a typeref at this point since it was either a
           direct class or enum type, or it was obtained after a skip_typerefs
           operation. */
        if (is_class_or_enum ||
            (!has_name(tp) && (is_immediate_class_type(tp) ||
                               is_immediate_enum_type(tp)))) {
          /* Note that in non-cfront mode this is done only for types that
             actually do have linkage. */
          tp->source_corresp.name = locator->symbol_header->identifier;
        }  /* if */
      }  /* if */
      /* Recompute the name linkage. */
      if (is_class_or_enum) {
        set_name_linkage_for_type(tp);
        if (is_immediate_class_type(tp)) {
          set_linkage_for_class_members(tp);
        } else {
          set_name_linkage_for_enumerators(tp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (attributes != NULL) {
    /* If there are attributes, make a copy of the underlying type,
       and apply the attributes to the copy. */
    type_ptr = apply_attributes_to_typedef(attributes, type_ptr);
  }  /* if */ 
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Create a new type entry and add it to the types list for the current
     scope. */
  tp = alloc_type((a_type_kind)tk_typeref);
  tp->variant.typeref.type = type_ptr;
  /* Create a new symbol for this type and bind it to the new type. */
  sym = enter_typedef_symbol(tp, locator, decl_scope_level,
                             suppress_redecl_error);
  set_source_corresp(&(tp->source_corresp), sym);
  nsp = NULL;
  if (!C_mode()) {
    if (class_type != NULL) {
      set_class_membership(sym, &tp->source_corresp, class_type);
      tp->source_corresp.access = ssep->current_access;
    } else {
      set_namespace_membership(sym, &tp->source_corresp,
                               (a_namespace_ptr)NULL);
      nsp = tp->source_corresp.parent.namespace_ptr;
    }  /* if */
  }  /* if */
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                            &locator->source_position, declarator_ssep);
#if BACK_END_IS_CP_GEN_BE
  /* Set the "name linkage environment" for this type.  This is used by the
     C++-generating back end to decide when to emit extern "C". */
  tp->variant.typeref.surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&tp->source_corresp, decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  add_to_types_list(tp, decl_scope_level);
  /* Issue a diagnostic if size_t is declared in a way inconsistent with
     the target configuration. */
  if (!is_error_type(type_ptr) &&
      (decl_scope_level == DEPTH_OF_FILE_SCOPE ||
       (nsp != NULL &&
        nsp == symbol_for_namespace_std->variant.namespace_info.ptr)) &&
      strcmp(sym->header->identifier, "size_t") == 0) {
    /* "size_t" declared at file scope or in namespace "std". */
    if (!is_integral_type(type_ptr) ||
        skip_typerefs(type_ptr)->variant.integer.int_kind !=
                                                targ_size_t_int_kind ||
        is_qualified_type(type_ptr)) {
      pos_ty_warning(ec_unexpected_type_for_size_t, &locator->source_position,
                     integer_type(targ_size_t_int_kind));
    }  /* if */
  }  /* if */
  if (vla_enabled && innermost_function_scope != NULL) {
    /* A typedef declaration inside a function. */
    if (is_variably_modified_type(type_ptr)) {
      a_statement_ptr  sp;
      
      sp = add_statement_at_stmt_pos((a_statement_kind)stmk_vla_decl,
                                     &locator->source_position);
      sp->variant.vla.is_typedef_decl = TRUE;
      sp->variant.vla.variant.typedef_type = tp;
      tp->variant.typeref.has_variably_modified_type = TRUE;
    }  /* if */
  }  /* if */
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
  a_decl_modifiers_block decl_modifiers;

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
    if (symbol_ptr->decl_scope != file_scope_number) {
      /* Take the symbol out of the symbol table. */
      remove_symbol(symbol_ptr);
      /* Put the symbol back into the symbol table at the file scope level. */
      reenter_symbol(symbol_ptr, DEPTH_OF_FILE_SCOPE, /*suppress_error=*/TRUE);
    }  /* if */
  }  /* if */
  /* All IL routines and their types must be at the file scope level, so switch
     to that memory region if necessary. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Generate the function type.  In C mode indicate it has an old-style
     no-information parameter list and a return type of "int".  See 3.3.2.2,
     semantics.  In C++ this must be an error, so give it a return type of
     tk_error and call it prototyped. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.extra_info->param_type_list = NULL;
  if (C_mode()) {
    rout_type->variant.routine.return_type =
                                       integer_type((an_integer_kind)ik_int);
    rout_type->variant.routine.extra_info->prototyped = FALSE;
  } else {
    /* Making the return type an error type prevents cascading errors. */
    rout_type->variant.routine.return_type = error_type();
    rout_type->variant.routine.extra_info->prototyped = TRUE;
    /* Setting has_ellipsis to TRUE means no diagnostics will be issued for
       having too many arguments. */
    rout_type->variant.routine.extra_info->has_ellipsis = TRUE;
  }  /* if */
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  func_info.declared_type = form_declared_type(rout_type, &func_info);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (exceptions_enabled) func_info.throw_position = locator.source_position;
  clear_decl_modifiers_block(&decl_modifiers);
  decl_routine(&locator, (a_storage_class)sc_extern, rout_type, &func_info,
               (a_source_sequence_entry_ptr)NULL,
               (SRK_DECLARATION | SRK_IMPLICIT), &decl_modifiers, 
               (an_attribute_ptr)NULL, (char *)NULL, &symbol_ptr, &linkage,
               &old_type, &ext_sym, (a_decl_pos_block_ptr)NULL);
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


a_label_ptr scan_label(a_boolean  is_definition,
                       a_boolean  is_declaration)
/*
Scan a label as part of a statement label, a goto statement, or a GNU C local
label declaration.  Return a pointer to the IL label.  The current token
should be a label identifier.  is_definition is TRUE if the label is being
scanned as part of a label definition.  is_declaration is TRUE if the label
is being scanned as part of a GNU C local label declaration.
*/
{
  a_symbol_ptr  label_sym;
  a_label_ptr   label;
  a_boolean     err = FALSE;

  a_source_position start_pos;

  db_enter(3, "scan_label");

  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    (void)required_token(tok_identifier, ec_exp_identifier);
    set_to_error_locator(locator_for_curr_id);
    label_sym = NULL;
    err = TRUE;
  } else {
    /* See if the label identifier is already in the symbol table. */
    a_scope_number  scope_number;
    scope_number = gcc_mode ? NO_SCOPE_NUMBER
                            : scope_stack[depth_innermost_function_scope].
                                                             il_scope->number;
    label_sym = find_label_symbol(locator_for_curr_id.symbol_header,
                                  scope_number);
    if (is_declaration && label_sym != NULL) {
      if (label_sym->decl_scope ==
                             scope_stack[decl_scope_level].il_scope->number) {
        /* A duplicate declaration. */
        sym_error(ec_already_defined, label_sym);
        err = TRUE;
      } else {
        /* The new declaration is going to hide the old one. */
        label_sym = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  if (label_sym == NULL) {
    /* Enter the label identifier into the symbol table.  This is normally
       done at the function level even if we are inside some blocks.  The
       exception is a GNU C local label declaration.  Use a locator with an
       undefined source position; the decl_position will be handled
       explicitly shortly. */
    a_scope_depth  depth = is_declaration ? decl_scope_level
                                          : depth_innermost_function_scope;
    locator_for_curr_id.source_position = null_source_position;
    label_sym = enter_symbol((a_symbol_kind)sk_label, &locator_for_curr_id,
                             depth, /*suppress_error=*/TRUE);
    /* Allocate the IL label and attach it to the symbol. */
    label_sym->variant.label.ptr = label = alloc_label();
#if GNU_EXTENSIONS_ALLOWED
    label->locally_declared = is_declaration;
#endif /* GNU_EXTENSIONS_ALLOWED */
    add_to_labels_list(label);
    set_source_corresp(&label->source_corresp, label_sym);
    /* The exec_stmt field stays NULL to indicate that the declaration
       has not been (fully?) processed yet. */
  }  /* if */
  if (!err) {
    /* Record the right kind of reference to the label symbol. */
    if (is_definition) {
      /* Note that we want mark_defined is called even if the symbol
         was previously entered.  Labels are strange in that a reference
         can come up before a declaration. */
      mark_defined(label_sym, &pos_curr_token);
    } else if (is_declaration) {
      mark_declared(label_sym, &pos_curr_token);
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
constructor for class A is declared A::A() rather than A().  This is
nonstandard, but it is allowed by cfront.
*/
{
  a_boolean                is_member_id = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  db_enter(3, "simplify_curr_class_qualified_name");

  if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
      is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL) &&
      locator_for_curr_id.is_qualified_name) {
    if (qualifier_class_type(locator_for_curr_id) == ssep->assoc_type &&
        locator_for_curr_id.is_global_qualified_name == FALSE) {
      is_member_id = TRUE;
      /* Reset the fields in the locator to make it appear as if the
         qualifier was not present. */
      clear_qualifier_from_locator(&locator_for_curr_id);
      if (any_cfront_mode() || microsoft_bugs) {
        /* No diagnostic, to be consistent with cfront's and Microsoft's
           behavior.  In the Microsoft case, we may also end up here for
           friend declarations ("struct S { friend void S::f(); };") and
           dropping the qualifier may result in the injection of the name
           in namespace scope. */
      } else {
        /* Accepting qualified member names is an extension -- issue a
           diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_qualifier_in_member_declaration);
      }  /* if */ 
    }  /* if */
  }  /* if */
  db_exit();
  return is_member_id;
}  /* simplify_curr_class_qualified_name */


void report_missing_type_specifier(a_source_position  *err_pos,
                                   a_boolean          is_function,
                                   a_boolean          is_function_def,
                                   a_boolean          is_main_function,
                                   a_boolean          any_decl_specifiers)
/*
No type was explicitly specified for the current declaration.  Issue the
appropriate diagnostic at the source position given by *err_pos.  is_function
is TRUE if this is a function declaration; is_function_def is TRUE if it is
a function declaration that is also a definition; is_main_function is TRUE
if it is a declaration of global scope "main".  any_decl_specifiers is
TRUE if at least one decl-specifier was seen (e.g., a storage class or
cv-qualifier).
*/
{
  an_error_code      error_code = ec_no_error;
  an_error_severity  severity;
  a_boolean          implicit_int_allowed =
                             !(C_dialect == C_dialect_cplusplus || c99_mode);

  if (is_function) {
    /* It must be a function declaration or else there is at least some type
       specifier (even if the type itself is implicit). */
    if (C_dialect == C_dialect_pcc) {
      /* No diagnostic is issued. */
    } else if (is_main_function) {
      /* Special handling for global function "main" -- including a separate
         error code, in case discretionary-error control for "main" should
         be independent of that for other functions. */
      if (implicit_int_allowed) {
        /* No diagnostic. */
      } else {
        /* C++ or C99 mode, in which the standard requires an explicit
           return type on "main". */
        error_code = ec_implicit_int_on_main;
        severity = strict_ansi_mode ?
                          strict_ansi_discretionary_severity : es_remark;
      }  /* if */
    } else {
      /* In general, issue a message about the implicit int return type,
         which deserves at least a remark in C mode and is now an error in
         strict C++ and strict C99 modes. */
      if (implicit_int_allowed) {
        /* Function declaration in C mode. */
        if (!any_decl_specifiers && !is_function_def) {
          /* Something like "f();". */
          error_code = ec_missing_decl_specifiers;
          severity = strict_ansi_mode ?
                         strict_ansi_discretionary_severity : es_warning;
        } else {
          /* Something like "static f();" or "f() { ... }".  The message
             indicates that "int" is implicit. */
          error_code = ec_missing_type_specifier;
          severity = es_remark;
        }  /* if */
      } else {
        /* Function declaration in C++ or C99 mode.  Unlike in C mode, issue
           the same message whether or not any_decl_specifiers is TRUE and
           whether this is a definition or merely a declaration.  That is,
           all the following are treated the same way:
             a();
             b(){}
             extern c();
             extern d(){}
        */
        error_code = ec_nonstd_implicit_int;
        if (any_cfront_mode()) {
          severity = es_remark;
        } else if (strict_ansi_mode) {
          severity = strict_ansi_discretionary_severity;
        } else {
          /* Default mode. */
          severity = es_warning;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (!any_decl_specifiers) {
    /* This is a non-function declaration for which the decl-specifiers are
       missing altogether.  Issue a diagnostic in all modes. */
    error_code = ec_missing_decl_specifiers;
    if (C_mode() && (microsoft_mode || C_dialect == C_dialect_pcc)) {
      severity = es_warning;
    } else {
      severity = es_discretionary_error;
    }  /* if */
  } else {
    /* Non-function declaration with at least some decl-specifiers -- e.g.,
       "const i;" or "typedef const CI;".  Use a different message and
       severity in C++ and C99 than in ordinary C, since it's a standards
       violation in C++ and C99. */
    if (implicit_int_allowed) {
      error_code = ec_missing_type_specifier;
      severity = es_warning;
    } else {
      error_code = ec_nonstd_implicit_int;
      severity = strict_ansi_mode ?
                   strict_ansi_discretionary_severity : es_warning;
    }  /* if */
  }  /* if */
  /* Unless the error is suppressed (e.g., in pcc mode), put out the
     diagnostic. */
  if (error_code != ec_no_error) {
    pos_diagnostic(severity, error_code, err_pos);
  }  /* if */
}  /* report_missing_type_specifier */


void type_name_full(a_type_ptr  *type_ptr,
                    a_boolean   *explicit_cv_qualifiers)
/*
Scan a type-name (see 3.5.5) and set *type_ptr to the type.
If explicit_cv_qualifiers is non-NULL, set *explicit_cv_qualifiers to TRUE if
explicit cv-qualifiers were scanned.
The syntax is:

3.5.5  type-name:
		specifier-qualifier-list abstract-declarator
							    opt

In C++ mode an error is issued if a type definition appears in a type-name
(for class/struct/union and enum types).
*/
{
  a_storage_class              storage_class;
  a_decl_flag_set              dso_flags, do_flags, di_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifiers_block       decl_modifiers;
  a_source_position            start_pos;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;

  db_enter(3, "type_name_full");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
			&storage_class, type_ptr, &qualifiers,
			(an_attribute_ptr *)NULL, &decl_modifiers,
			(a_decl_pos_block_ptr)NULL);
  if (C_dialect == C_dialect_cplusplus &&
      (dso_flags & DSO_DEFINES_SOMETHING)) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    report_implicit_int(&start_pos);
  }  /* if */
  if (explicit_cv_qualifiers != NULL) {
    *explicit_cv_qualifiers = (qualifiers != TQ_NONE);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_abstract_declarator_start()) {
    di_flags = DI_ABSTRACT_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED;
    if (vla_enabled && depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      /* Note that int[*] is not allowed, but int(*)[*] is okay.  Therefore
         we turn on DI_VLA_ASTERISK_ALLOWED and check for the error case
         once the scan has been completed. */
      di_flags |= DI_VLA_ALLOWED | DI_VLA_ASTERISK_ALLOWED;
    }  /* if */
    declarator(di_flags, &do_flags, *type_ptr,
               /*member_parent_type=*/(a_type_ptr)NULL,
               (a_symbol_locator *)NULL, type_ptr,
               &declarator_ssep, (a_func_info_block_ptr)NULL,
               (a_decl_pos_block_ptr)NULL, (an_attribute_ptr *)NULL);
    if (explicit_cv_qualifiers != NULL) {
      /* Explicit qualifiers might have been introduced in the declarator: */
      *explicit_cv_qualifiers = is_top_level_qualified_type(*type_ptr);
    }  /* if */
    if (di_flags & DI_VLA_ALLOWED) {
      /* VLA checking was done. */
      if (is_array_type(*type_ptr) &&
          is_or_contains_vla_type_with_unspecified_bound(*type_ptr)) {
        /* This is an array in which the variable bound is unspecified in
           one of its dimensions. */
        pos_error(ec_vla_with_unspecified_bound_not_allowed, &start_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  if ((any_cfront_mode() &&
       check_member_function_typedef(*type_ptr, &start_pos)) ||
      is_unknown_type(*type_ptr)) {
    /* If the type is of the unknown kind, presumably an error occurred and
       hence an error type should be returned.  If the type is a cfront-style
       member function typedef -- it is an error to use it anywhere but in a
       pointer-to-member declaration. */
    *type_ptr = error_type();
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* type_name_full */


void new_type_name(a_boolean         is_parenthesized,
                   a_type_ptr        *type_ptr)
/*
Scan a C++ new-type-name or a parenthesized type-name that may appear in a
"new" expression (ARM 5.3.3), and return a pointer to the type in *type_ptr.
The syntax is:

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

This syntax allows only a restricted form of types, but you can specify
an arbitrary type by enclosing a type-name in parentheses:

   type-name:
              type-specifier-list abstract-declarator
                                                     opt

If is_parenthesized is TRUE, the caller has already trapped the left
parenthesis for such a construct.  The parenthesis is also checked for
within this routine if is_parenthesized comes in FALSE.
*/
{
  a_type_ptr                  complete_type, new_type_ptr;
  a_type_ptr                  derived_type, bottom_derived_type;
  a_decl_flag_set             dso_flags, do_flags;
  a_type_qualifier_set        qualifiers;
  a_decl_modifiers_block      decl_modifiers;
  a_source_position           start_pos;
  a_storage_class             storage_class;
  a_source_sequence_entry_ptr declarator_ssep = NULL;
  a_decl_pos_block            decl_pos_block;

  db_enter(3, "new_type_name");
  /* Check for the parenthesized form. */
  if (!is_parenthesized && curr_token == tok_lparen) {
    is_parenthesized = TRUE;
    (void)get_token();
  }  /* if */
  if (is_parenthesized) add_stop_token(tok_rparen);
  set_err_pos_to_curr_token();
  clear_decl_pos_block(&decl_pos_block);
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED | DSI_IS_NEW_TYPE_NAME,
                        &dso_flags, &storage_class, type_ptr, &qualifiers,
			(an_attribute_ptr *)NULL, &decl_modifiers,
			&decl_pos_block);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    report_implicit_int(&error_position);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_parenthesized) {
    /* In the parenthesized form, the full declarator syntax is allowed. */
    if (is_abstract_declarator_start()) {
      declarator(DI_ABSTRACT_DECLARATOR_ALLOWED |
                    DI_QUALIFIED_NAME_ALLOWED |
                    DI_DIMENSION_EXPRESSION_ALLOWED,
                 &do_flags, *type_ptr,
                 /*member_parent_type=*/(a_type_ptr)NULL,
                 (a_symbol_locator *)NULL, type_ptr,
                 &declarator_ssep, (a_func_info_block_ptr)NULL,
                 &decl_pos_block, (an_attribute_ptr *)NULL);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    /* In the non-parenthesized form, a limited declarator syntax is
       allowed. */
    /* Scan pointer declarators. */
    complete_type = pointer_declarator(*type_ptr,
                                       /*reference_allowed=*/FALSE,
				       (a_call_conv_descr_ptr)NULL,
				       (a_call_conv_descr_ptr)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       &decl_pos_block,
                                       (an_attribute_ptr *)NULL);
    derived_type = NULL;
    bottom_derived_type = NULL;
    add_stop_token(tok_lbracket);
    if (curr_token == tok_lbracket) {
      /* Scan array declarators.  The first one allows an expression
         as the size; the others require a constant size. */
      array_declarator(&new_type_ptr, /*nonconstant_allowed=*/TRUE,
                       /*vla_is_allowed=*/FALSE,
                       /*vla_asterisk_allowed=*/FALSE,
                       /*top_level_field_decl=*/FALSE,
                       /*top_level_param_decl=*/FALSE,
                       &decl_pos_block);
      add_to_derived_type_list(new_type_ptr,
                               &derived_type, &bottom_derived_type,
                               /*microsoft_property=*/FALSE);
      while (curr_token == tok_lbracket) {
        array_declarator(&new_type_ptr, /*nonconstant_allowed=*/FALSE,
                         /*vla_is_allowed=*/FALSE,
                         /*vla_asterisk_allowed=*/FALSE,
                         /*top_level_field_decl=*/FALSE,
                         /*top_level_param_decl=*/FALSE,
                         &decl_pos_block);
        /* Add the new type to the bottom of the existing derived type list.
           Note that this involves error checking. */
        add_to_derived_type_list(new_type_ptr,
                                 &derived_type, &bottom_derived_type,
                                 /*microsoft_property=*/FALSE);
      }  /* while */
      if (derived_type != NULL) {
        if (complete_type != NULL) {
          if (!is_error_type(bottom_derived_type)) {
            /* Combine derived_type and complete_type. */
            add_to_derived_type_list(complete_type,
                                     &derived_type, &bottom_derived_type,
                                     /*microsoft_property=*/FALSE);
          }  /* if */
        }  /* if */
        complete_type = derived_type;
      }  /* if */
    }  /* if */
    remove_stop_token(tok_lbracket);
    *type_ptr = complete_type;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block.declarator_range.end.seq != 0) {
    curr_construct_end_position = decl_pos_block.declarator_range.end;
  } else {
    curr_construct_end_position = decl_pos_block.specifiers_range.end;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (any_cfront_mode() &&
      check_member_function_typedef(*type_ptr, &start_pos)) {
    /* The type is a cfront-style member function typedef -- it is an error
       to use it anywhere but in a pointer-to-member declaration. */
    *type_ptr = error_type();
  }  /* if */
  db_exit();
}  /* new_type_name */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_type_ptr simple_type_specifier_sequence(void)
/*
Scan a sequence of simple-type-specifiers and return a pointer to the
resulting type.  This is called in Microsoft mode for function-style casts
where the type involves more than one token -- e.g., "unsigned int(x)".
*/
{
  a_decl_flag_set             dso_flags;
  a_storage_class             storage_class;
  a_type_ptr                  type_ptr;
  a_type_qualifier_set        qualifiers;
  a_decl_modifiers_block      decl_modifiers;
  a_source_position           pos;
  a_decl_pos_block            decl_pos_block;

  check_assertion(microsoft_mode);
  pos = pos_curr_token;
  clear_decl_pos_block(&decl_pos_block);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
                        &storage_class, &type_ptr, &qualifiers,
			(an_attribute_ptr *)NULL, &decl_modifiers,
			&decl_pos_block);
  /* Set error_position to the start of the type-specifier sequence. */
  error_position = pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = decl_pos_block.specifiers_range.end;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return type_ptr;
}  /* simple_type_specifier_sequence */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean scan_conversion_operator(
			a_source_position		*id_pos,
                        a_boolean			is_class_member,
                        a_parent_class_or_namespace_ptr	parent)
/*
The token "operator" has been seen and passed; we are now on the token
immediately following it.  If it marks the start of a type name we have
an identifier for a conversion operator -- scan the type name, update the
locator, and return TRUE.  If it doesn't, return FALSE.
If the class or namespace pointed to by parent is not NULL then push a
class or namespace reactivation scope before scanning type name in a type
conversion operator.  is_class_member is TRUE if the parent points
to a class, it is FALSE if parent points to a namespace or if there
is no parent.
*/
{
  a_storage_class         storage_class;
  a_decl_flag_set         dso_flags;
  a_type_qualifier_set    qualifiers;
  a_decl_modifiers_block  decl_modifiers;
  a_type_ptr              specifiers_type, complete_type;
  a_source_position       type_pos;
  a_boolean               is_conversion_operator;
  a_boolean               class_reactivated = FALSE;
  a_boolean               namespace_reactivated = FALSE;
  a_decl_pos_block        decl_pos_block;

  db_enter(3, "scan_conversion_operator");
  /* Push a class or namespace reactivation scope if the class or namespace
     pointed to by parent is not NULL.  This is used when scanning conversion
     operators such as "A::operator B" where B needs to be looked up within
     A.  This is not needed for overloaded operator routines, but we
     don't know what kind of operator we are scanning until we call
     is_type_start, and the class needs to be reactivated before
     is_type_start is called. */
  if (is_class_member) {
    if (parent->class_type != NULL &&
        !is_incomplete_type(parent->class_type)) {
      a_symbol_ptr	sym;
      sym = (a_symbol_ptr)parent->class_type->source_corresp.assoc_info;
      /* In valid usage, the class type will always be either a complete
         real class type or a prototype instantiation.  In other cases,
         suppress the reactivation because incomplete and nonreal classes
         cannot be reactivated.  An error will be issued elsewhere for these
         cases. */
      if (sym != NULL && 
          (is_real_class_symbol(sym) ||
           is_prototype_instantiation_symbol(sym))) {
        push_class_reactivation_scope(parent->class_type,
                                      /*extend_namespace=*/FALSE);
        class_reactivated = TRUE;
      }  /* if */
    }  /* if */
  } else if (parent != NULL && parent->namespace_ptr != NULL) {
    push_namespace_reactivation_scope(parent->namespace_ptr);
    namespace_reactivated = TRUE;
  }  /* if */
  if (is_type_start(/*is_expr_context=*/FALSE)) {
    /* It is the start of a type name. */
    is_conversion_operator = TRUE;
    set_err_pos_to_curr_token();
    copy_source_position(pos_curr_token, type_pos);
    clear_decl_pos_block(&decl_pos_block);
    (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
                          &storage_class, &specifiers_type, &qualifiers,
			  (an_attribute_ptr *)NULL, &decl_modifiers,
			  &decl_pos_block);
    if (dso_flags & DSO_DEFINES_SOMETHING) {
      /* Definition of a class, struct, union, or enum type is not allowed. */
      pos_error(ec_type_definition_not_allowed, &type_pos);
    } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      /* Missing type specifier. */
      report_implicit_int(&error_position);
    }  /* if */
    complete_type = pointer_declarator(specifiers_type,
                                       /*reference_allowed=*/TRUE,
                                       (a_call_conv_descr_ptr)NULL,
                                       (a_call_conv_descr_ptr)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       (a_type_qualifier_set *)NULL,
                                       &decl_pos_block,
                                       (an_attribute_ptr *)NULL);
    if (any_cfront_mode() &&
        check_member_function_typedef(complete_type, &type_pos)) {
      /* The type is a cfront-style member function typedef -- it is an error
         to use it anywhere but in a pointer-to-member declaration. */
      complete_type = error_type();
    }  /* if */
    unget_token();
    curr_token = tok_identifier;
    pos_curr_token = error_position = *id_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* After backing up one token, the end position needs to be reset too. */
    if (decl_pos_block.declarator_range.end.seq != 0) {
      end_pos_curr_token = decl_pos_block.declarator_range.end;
    } else {
      end_pos_curr_token = decl_pos_block.specifiers_range.end;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    make_type_conversion_locator(complete_type, &locator_for_curr_id, id_pos);
  } else {
    is_conversion_operator = FALSE;
  }  /* if */
  /* Pop the class or namespace reactivation scope if one was
     pushed earlier. */
  if (class_reactivated) {
    pop_class_reactivation_scope();
  } else if (namespace_reactivated) {
    pop_namespace_reactivation_scope();
  }  /* if */
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

  check_assertion(!C_mode());
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
    case tok_wchar_t:
      type = wchar_t_type();
      break;
    case tok_bool:
      type = bool_type();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The following tokens may or may not be defined, but for each that is,
       the corresponding integer kind will be set to something besides
       ik_none. */
    case tok_int8:
      type = integer_type(targ_int8_int_kind);
      break;
    case tok_int16:
      type = integer_type(targ_int16_int_kind);
      break;
    case tok_int32:
      type = integer_type(targ_int32_int_kind);
      break;
    case tok_int64:
      type = integer_type(targ_int64_int_kind);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  
  rtsp = routine_symbol_type(rout_sym)->variant.routine.extra_info;
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
    routine_symbol_type(sym)->variant.routine.extra_info->arg_pragma =
                                                        ppp->descr_ptr->kind;
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


a_boolean scan_name_linkage_string(a_name_linkage_kind *kind)
/*
Scan the string portion of a linkage specification (extern "C", extern "C++",
etc.).  The current token is the string.  Look it up in the set of strings
that may appear in a linkage specification, and if the lookup is successful
return TRUE and set *kind to the corresponding name-linkage kind.
*/
{
  a_boolean            err = FALSE;
  a_name_linkage_kind  local_kind;

  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  if (is_error_constant(&const_for_curr_token)) {
    /* There must have been an error in scanning the string literal (e.g.,
       no closing '"'. */
    err = TRUE;
  } else {
    /* Look for the predefined string ("C++", "C", ...) which the current
       token matches. */
    for (local_kind = (a_name_linkage_kind)nlk_cplusplus_external;
         (int)local_kind < (int)nlk_last;
         local_kind = (a_name_linkage_kind)(local_kind + 1)) {
      if (eq_constants(&const_for_curr_token,
                       &name_linkage_constants[(int)local_kind])) {
        /* Found a matching linkage kind string. */
        break;
      }  /* if */
    }  /* for */
    if (local_kind != (a_name_linkage_kind)nlk_last) {
      /* Return the name-linkage kind to the caller. */
      *kind = local_kind;
    } else {
      /* Bad linkage kind. */
      error(ec_bad_linkage_specifier);
      err = TRUE;
    }  /* if */
  }  /* if */
  /* Return success status to the caller. */
  return !err;
}  /* scan_name_linkage_string */


static void linkage_specification(a_boolean      function_definition_allowed,
                                  a_boolean      is_old_style_param_decl,
                                  a_boolean      is_top_level_declaration,
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
in a local variable, the new one is established by updating a global
variable, the declaration(s) are processed, and then the original linkage
specifier is restored.
*/
{
  a_name_linkage_kind  kind;
  a_boolean            err = FALSE;
  a_source_range       linkage_spec_range;

  db_enter(3, "linkage_specification");
  if (decl_scope_level != depth_innermost_namespace_scope) {
    error(ec_linkage_specifier_not_allowed);
    err = TRUE;
  }  /* if */
  linkage_spec_range.start = pos_curr_token;
  /* Advance to the string literal. */
  (void)get_token();
  check_assertion(curr_token == tok_string_literal);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  linkage_spec_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  /* Record the new default linkage in the scope stack. */
  if (!scan_name_linkage_string(&kind) || err) {
    /* If there's an error on this linkage-specification declaration, leave
       the default name linkage kind unchanged.  But to simplify processing,
       the push and pop will still be done. */
    kind = scope_stack[depth_scope_stack].default_name_linkage;
  }  /* if */
  /* Save the current default linkage and set the new one. */
  push_name_linkage(kind);
  /* Advance past the string token. */
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
      declaration(function_definition_allowed, is_old_style_param_decl,
                  /*is_top_level_declaration=*/FALSE,
                  /*marked_as_gnu_extension=*/FALSE, param_id_list,
                  (a_source_range *)NULL);
    }  /* while */
    /* Restore the default linkage to the value it had before the declaration
       (or declaration list) was processed.  Note that this must be done
       before advancing past the closing brace -- there is a dependency in
       precompiled header processing on the state maintained in the scope
       stack entry. */
    pop_name_linkage();
    /* Check for the final right brace of the linkage specification block,
       but don't advance past it -- that is handled in translation_unit. */
    remove_stop_token(tok_rbrace);
    if (curr_token != tok_rbrace) {
      pos_error(ec_exp_rbrace, &pos_curr_token);
    } else {
      /* Advance past right brace.  If the current declaration is a top-level
         declaration, set a global flag to enable checking for a header
         stop. */
      if (is_top_level_declaration) next_token_is_top_level_decl_start = TRUE;
      (void)get_token();
      next_token_is_top_level_decl_start = FALSE;
    }  /* if */
  } else {
    if (curr_token == tok_end_of_source) {
      /* Missing declaration. */
      error(ec_exp_declaration);
      pop_name_linkage();
    } else {
      /* Just one declaration is governed by this linkage specifier.  If no
         storage class is specified it is as though "extern" were specified --
         this is an interpretation of the sentence in ARM 7.4 asserting, "An
         object defined withing an `extern "C" {...}' construct is still
         defined and not just declared," and of the example following it,
         where without the braces the variable is not defined. */
      declaration(function_definition_allowed, is_old_style_param_decl,
                  is_top_level_declaration, /*marked_as_gnu_extension=*/FALSE,
                  param_id_list, &linkage_spec_range);
      /* pop_name_linkage will already have been called in declaration
         (before advancing past the end of the declaration, because there
         is a dependency in precompiled header processing on the state
         maintained in the scope stack entry). */
    }  /* if */
  }  /* if */

  db_exit();
}  /* linkage_specification */


static a_boolean is_invalid_catch_type(a_type_ptr type, a_source_position *pos)
/*
Check whether the given type is a valid catch type: incomplete types, pointers
and references to incomplete types and abstract class types are not valid.
pos is used to mark the location that carries any diagnostic.
*/
{
  a_boolean result = FALSE;
  if (is_incomplete_type(type)) {
    pos_error(ec_incomplete_type_not_allowed, pos);
    result = TRUE;
  } else if (is_ptr_or_ref_type(type)) {
    type = type_pointed_to(type);
    /* Force instantiation of template class. */
    complete_type_is_needed(type);
    if (is_incomplete_type(type) && !is_void_type(type)) {
      pos_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                     ec_ptr_or_ref_to_incomplete_type, pos);
      /* We might have only issued a warning.  Hence proceed with normal
         processing (i.e., result remains FALSE).  The code generators can
         handle it. */
    }  /* if */
  } else if (is_abstract_class_type(type)) {
    report_abstract_class_error(ec_abstract_class_catch_type, type, pos);
    result = TRUE;
  }  /* if */
  return result;
}  /* is_invalid_catch_type */


void handler_declaration(a_statement_ptr     try_block_stmt,
                         a_source_position*  catch_pos)
/*
Process a handler declaration:

  "catch" "(" exception-declaration ")" compound-statement

try_block_stmt is a pointer to the try-block statement to which the catch
clause is to be attached.  catch_pos is the source position of "catch".
*/
{
  a_handler_ptr                handler, prev_handler;
  a_type_ptr                   type_ptr = NULL;
  a_storage_class              storage_class;
  a_decl_flag_set              dso_flags, do_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifiers_block       decl_modifiers;
  a_symbol_ptr                 sym;
  a_symbol_locator             locator;
  a_source_position            decl_pos;
  a_routine_ptr                cctor, dtor;
  a_param_type_ptr             ptp;
  a_dynamic_init_ptr           dip;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;

  db_enter(3, "handler_declaration");
  /* Push the scope for the handler before processing the exception
     declaration to assure that the scope of the handler's parameter is the
     same as that of the handler's compound statement block. */
  (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL);
  /* Allocate the handler. */
  handler = alloc_handler();
  /* Set the assoc_handler field of the IL scope entry. */
  set_block_scope_handler(handler);
  set_stmt_source_position(handler->catch_position, *catch_pos);
  if (required_token(tok_lparen, ec_exp_lparen)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (struct_stmt_stack != NULL) {
      struct_stmt_stack[depth_stmt_stack].in_handler_parameter_declaration
                                                                       = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
        a_decl_pos_block  decl_pos_block;

        clear_decl_pos_block(&decl_pos_block);
        (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_EMPTY_DECL_SPECIFIERS_ALLOWED),
                              &dso_flags, &storage_class, &type_ptr,
                              &qualifiers, (an_attribute_ptr *)NULL,
			      &decl_modifiers, &decl_pos_block);
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
          report_implicit_int(&pos_curr_token);
        }  /* if */
        sym = NULL;
        if (is_abstract_or_real_declarator_start()) {
          declarator(DI_REAL_DECLARATOR_ALLOWED |
                       DI_ABSTRACT_DECLARATOR_ALLOWED,
                     &do_flags, type_ptr,
                     /*member_parent_type=*/(a_type_ptr)NULL,
                     &locator, &type_ptr, &declarator_ssep,
                     (a_func_info_block_ptr)NULL, &decl_pos_block,
                     (an_attribute_ptr *)NULL);
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
          complete_type_is_needed(type_ptr);
          /* Adjust the type if necessary (for example, "array of x"
             becomes "pointer to x"). */
          adjust_parameter_type(&type_ptr, (an_attribute_ptr)NULL);
          if (is_invalid_catch_type(type_ptr, &decl_pos)) {
            /* An appropriate error message will have been issued by
               invalid_catch_type. */
            type_ptr = error_type();
          } else {
            /* Mark the type as having been used in an exception.  (Also,
               if it "contains" any classes, they are marked as requiring
               external linkage.) */
            set_used_in_exception_or_rtti_flag(type_ptr);
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Record additional source-range information in the variable entry
           for the handler parameter. */
        if (sym != NULL) {
          update_decl_pos_info(&handler->parameter->source_corresp,
                               &decl_pos_block);
        } else {
          /* Since set_source_corresp is not called for unnamed entities,
             create the associated decl-pos supplement directly. */
          handler->parameter->source_corresp.decl_pos_info =
                          make_decl_pos_supplement(/*at_file_scope=*/FALSE,
                                                   &decl_pos_block);
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Set the is_local_to_function flag after returning from
           set_source_corresp. */
        handler->parameter->source_corresp.is_local_to_function = TRUE;
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
          /* Both the copy constructor and destructor must be accessible in
             the context of the handler (15.3 [except.handle] para 17).
             (However, the Microsoft compiler doesn't enforce accessibility of
             copy constructors; see
                                reference_to_implicitly_invoked_function). */
          cctor = select_copy_constructor(type_ptr,
                                          (a_type_qualifier_set)TQ_NONE,
                                          &pos, type_ptr, &bitwise_copy,
                                          /*evaluated=*/TRUE);
          check_assertion((cctor == NULL) == bitwise_copy);
          dtor = select_destructor(type_ptr, type_ptr, &pos,
                                   /*honor_virtual=*/FALSE,
                                   /*evaluated=*/TRUE);
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
          dip->variant.constructor.args =
           copy_default_arg_expr_list(cctor, ptp,
                                      /*inside_conditional_expression=*/FALSE,
                                      /*potentially_evaluated=*/TRUE);
          /* Only at runtime is the source known. */
          dip->variant.constructor.
                             is_copy_constructor_with_implied_source = TRUE;
        } else {
          /* A bitwise copy is all that is required. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        }  /* if */
        dip->variable = handler->parameter;
        dip->destructor = dtor;
        record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                           /*block_lifetime=*/TRUE);
        handler->dynamic_init = dip;
      }  /* if */
    }  /* if */
    prev_handler = try_block_stmt->variant.try_block->handlers;
    if (prev_handler == NULL) {
      /* This is the first handler declared for this try block. */
      try_block_stmt->variant.try_block->handlers = handler;
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
          /* Default handler has already been declared.  If it's the last on
             the list, issue an error; if not, an error will already have
             been issued, and further checking is suppressed. */
          if (prev_handler->next == NULL) {
            /* Anything following a default handler is masked by it, but we
               only issue an error on the first handler that follows. */
            pos_error(ec_masked_by_default_handler, &decl_pos);
          }  /* if */
        } else if (handler->parameter == NULL) {
          /* Current handler is a default handler -- it can only be masked by
             another default handler. */
        } else if (prev_handler->parameter->type == error_type()) {
          /* No need to check for masking in this case. */
        } else if (type_masks_handler_param_type(prev_handler->parameter->type,
                                                 type_ptr)) {
          /* The type of prev_handler assures that handler will never be
             called, because it masks current handler's type.  See ARM 15.4. */
          pos_ty_warning(ec_masked_by_handler, &decl_pos,
                         prev_handler->parameter->type);
          masked = TRUE;
        }  /* if */
        if (prev_handler->next == NULL) {
          /* End of the list.  Append the new handler. */
          prev_handler->next = handler;
          break;
        } else {
          /* Keep looping. */
          prev_handler = prev_handler->next;
        }  /* if */
      }  /* for */
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (struct_stmt_stack != NULL) {
      struct_stmt_stack[depth_stmt_stack].in_handler_parameter_declaration
                                                                      = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Parse the body of the handler. */
  handler->statement = compound_statement(/*at_function_level=*/FALSE,
                                          /*explicit_return_type=*/FALSE,
                                          /*is_catch_clause=*/TRUE,
                                          /*is_statement_expr=*/FALSE);
  /* pop_scope is called from compound_statement processing. */
  db_exit();
}  /* handler_declaration */


#if !GENERATE_SOURCE_SEQUENCE_LISTS && !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */ /* is_asm_statement is not referenced.*/
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS && !MICROSOFT_EXTENSIONS_ALLOWED */
an_asm_entry_ptr asm_declaration(a_boolean  asm_decl_allowed,
                                 a_boolean  is_asm_statement)
/*
Scan an asm declaration, create an entry to represent it in the IL, and
return a pointer to the asm entry.  asm_decl_allowed is FALSE if an error
should be issued.  is_asm_statement is TRUE if this asm declaration appears
inside a function (and will therefore be associated an stmk_asm statement
entry), FALSE otherwise.  An asm declaration is specified as follows in the
ARM:

  asm ( string-literal ) ;

It can appear at file scope, function scope, and block scope.  In C mode,
where declarations and executable statements may not be mingled, an asm
"declaration" at function or block scope is always treated as executable.

In Microsoft mode support is provided for additional syntax:

  __asm { asm-instruction-list } ;opt
  __asm asm-instruction ;opt

where an asm-instruction-list is a semi-colon-delimited list of asm
instructions (unquoted).

Microsoft asm blocks are converted into a string when the __asm token
is encountered.  The string is pointed to by curr_token_asm_string and
is saved and restored as needed by the token caching mechanism.

In GNU mode support is provided for additional syntax:

  asm volatile    ( string-literal : operand-spec )
              opt

This may appear only at function or block scope.  The operand-spec tells
the compiler how to map C/C++ variables into and out of the assembly
instruction's operands.
*/
{
  a_constant                asm_string;
  an_asm_entry_ptr          ap = NULL;
  a_source_position         asm_pos;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean                 is_volatile = FALSE;
  an_asm_operand_ptr        operands = NULL;
  a_named_register_list_ptr clobbers = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(3, "asm_declaration");
  check_assertion(curr_token == tok_asm || curr_token == tok_microsoft_asm);
  if (!asm_decl_allowed) {
    /* An asm declaration is not allowed in the current scope. */
    error(ec_asm_decl_not_allowed);
    discard_curr_construct_pragmas();
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an asm
       declaration. */
    cannot_bind_to_curr_construct();
  }  /* if */
  copy_source_position(pos_curr_token, asm_pos);
  if (curr_token == tok_microsoft_asm) {
    clear_constant(&asm_string, (a_constant_repr_kind)ck_string);
    /* The asm string is already allocated in IL memory. */
    asm_string.variant.string.value = curr_token_asm_string;
    asm_string.variant.string.length =
                            (a_targ_size_t)(strlen(curr_token_asm_string)) + 1;
    asm_string.type = string_type(asm_string.variant.string.length);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Bypass the Microsoft asm token. */
    (void)get_token();
  } else {
    /* Skip past the "asm". */
    (void)get_token();
#if GNU_EXTENSIONS_ALLOWED
    /* Skip a potential "volatile". */
    if (gcc_mode && curr_token == tok_volatile) {
      is_volatile = TRUE;
      (void)get_token();
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
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
#if GNU_EXTENSIONS_ALLOWED
    /* Check for operands spec. */
    if (gcc_mode && is_asm_statement) {
      if (curr_token == tok_colon || curr_token == tok_colon_colon) {
        operands = asm_operands_spec();
        clobbers = asm_clobbers_spec();
        validate_operands_and_clobbers(operands, clobbers);
      }  /* if */
      /* An asm() with no outputs is automatically volatile. */
      if (operands == NULL ||
          !(operands->modifiers & (an_asm_operand_modifier)aom_output)) {
        is_volatile = TRUE;
      }  /* if */
    } else {
      /* This is not a situation where operand specs are accepted (e.g., not
         GNU C mode).  Mark the entry as "volatile" to indicate the fact that
         we do not know the effect on operands. */
      is_volatile = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Check for and skip the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for and skip the semicolon. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
  /* Update the IL. */
  if (asm_decl_allowed) {
    /* Allocate and set the asm-entry. */
    ap = alloc_asm_entry();
    ap->asm_string = alloc_unshared_constant(&asm_string);
    copy_source_position(asm_pos, ap->source_corresp.decl_position);
#if GNU_EXTENSIONS_ALLOWED
    ap->is_volatile = is_volatile;
    ap->operands = operands;
    ap->clobbers = clobbers;
#endif /* GNU_EXTENSIONS_ALLOWED */
    if (!is_asm_statement) {
      /* Add the asm entry to the list for the current scope.  This is only
         done for asm declarations that do not appear in an executable context
         and so have no statement entry to point at them. */
      add_to_asm_entries_list(ap);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* There's no name or symbol for the asm declaration, so call
         update_source_sequence_list directly. */
      add_to_source_sequence_list((char *)ap, (an_il_entry_kind)iek_asm_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  }  /* if */

  db_exit();
  return ap;
}  /* asm_declaration */


a_variable_ptr condition_declaration(void)
/*
Scan a condition declaration.  Syntax:

  type-specifier-seq declarator = assignment-expression

Return a pointer to the variable that is declared.
*/
{
  a_storage_class              storage_class;
  a_type_ptr                   type_ptr = NULL;
  a_decl_flag_set              dsi_flags, dso_flags, do_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifiers_block       decl_modifiers;
  a_symbol_ptr                 sym;
  a_variable_ptr               vp;
  a_symbol_locator             locator;
  a_source_position            decl_pos;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;
  a_boolean                    incomplete_type_error_reported;
  a_boolean                    missing_declarator = FALSE;
  a_symbol_reference_kind      srk_flags;
  a_decl_pos_block             decl_pos_block;

  db_enter(3, "condition_declaration");
  decl_pos = pos_curr_token;
  /* Scan the declaration specifiers.  "typedef" is not allowed and may
     not introduce a new class or enumeration. */
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
              DSI_IS_CONDITION_DECL;
  clear_decl_pos_block(&decl_pos_block);
  (void)decl_specifiers(dsi_flags, &dso_flags, &storage_class, &type_ptr,
                        &qualifiers, (an_attribute_ptr *)NULL,
			&decl_modifiers, &decl_pos_block);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &decl_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Implicit int. */
    report_implicit_int(&pos_curr_token);
  }  /* if */
  if (storage_class == (a_storage_class)sc_unspecified) {
    storage_class = (a_storage_class)sc_auto;
  }  /* if */
  if (is_declarator_start()) {
    /* Scan the declarator, which is not allowed to specify a function or an
       array. */
    declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags, type_ptr,
               /*member_parent_type=*/(a_type_ptr)NULL, &locator, &type_ptr,
               &declarator_ssep, (a_func_info_block_ptr)NULL, &decl_pos_block,
               (an_attribute_ptr *)NULL);
  } else {
    /* No declarator.  Issue a single diagnostic on this malformed
       condition declaration. */
    missing_declarator = TRUE;
    set_to_error_locator(locator);
    error_position = pos_curr_token;
  }  /* if */
  complete_type_is_needed(type_ptr);
  if (is_incomplete_type(type_ptr)) {
    /* Incomplete type is not allowed. */
    pos_error(ec_incomplete_type_not_allowed, &decl_pos);
    type_ptr = error_type();
  } else if (is_function_type(type_ptr)) {
    /* Function type is disallowed. */
    pos_error(ec_function_type_not_allowed, &decl_pos);
    type_ptr = error_type();
  } else if (is_array_type(type_ptr)) {
    /* Array type is disallowed. */
    pos_error(ec_array_type_not_allowed, &decl_pos);
    type_ptr = error_type();
  }  /* if */
  /* Enter the symbol in the current scope, which should be an sck_condition
     scope. */
  sym = enter_symbol((a_symbol_kind)sk_variable, &locator, decl_scope_level,
                     /*suppress_redecl_error=*/FALSE);
  /* Allocate the variable and bind the symbol to it. */
  vp = make_variable(type_ptr, storage_class, decl_scope_level);
  sym->variant.variable.ptr = vp;
  set_source_corresp(&vp->source_corresp, sym);
  /* Copy the decl-modifiers into the variable entry. */
  update_variable_decl_modifiers(vp, &decl_modifiers,
                                 &locator.source_position,
                                 /*is_redecl=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  sym->variant.variable.ptr->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  srk_flags = SRK_DECLARATION | SRK_DEFINITION;
  if (!missing_declarator && curr_token == tok_assign) {
    srk_flags |= SRK_INITIALIZATION;
  }  /* if */
  record_symbol_declaration(srk_flags, sym, &sym->decl_position,
                            declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&sym->variant.variable.ptr->source_corresp,
                       &decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (missing_declarator) {
    /* Now issue the error for the missing declarator. */
    syntax_error(ec_exp_declarator_in_condition_decl);
  } else {
    /* The syntax for condition (see WP [stmt.select]) explicitly requires the
       "= expr" syntax for initialization (that is, parenthesized initializers
       are disallowed, as is implicit initialization of objects with default
       constructors). */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_pos_block.var_init_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)required_token(tok_assign, ec_exp_assign);
    if (curr_token == tok_lbrace) {
      /* The syntax does not permit initialization with a brace enclosed
         initializer list. */
      error_position = pos_curr_token;
      syntax_error(ec_exp_primary_expr);
    } else {
      initializer(sym, &locator.source_position, (an_id_linkage_kind)idl_none,
                  /*parenthesized_initializer=*/FALSE, /*is_parameter=*/FALSE,
                  &incomplete_type_error_reported, &decl_pos_block);
    }  /* if */
    /* Reset the error position to the source position of the declarator. */
    error_position = locator.source_position;
  }  /* if */
  /* Both in the error and normal case consider the variable set.  Don't
     do this earlier so we can catch "if (int x = x);". */
  mark_variable_value_set(sym);
  db_exit();
  /* Return a pointer to the variable. */
  return vp;
}  /* condition_declaration */


void make_using_directive(a_namespace_ptr    nsp,
			  a_scope_depth	     depth,
                          a_source_position  *pos,
			  a_boolean	     compiler_generated)
/*
Create a using-decl entry for a using-directive that specifies the indicated
namespace, add it to the list of using-decl entries for the scope specified
by depth, and "activate" it to assure that inactive-list symbols belonging
to the namespace will be found during name lookup.

compiler_generated is TRUE for implicit using-directives created for
unnamed namespaces, and for certain using-directives created to emulate
a Microsoft bug.
*/
{
  a_using_decl_ptr  udp;

  /* Create the using-directive entry. */
  udp = alloc_using_decl();
  udp->position = *pos;
  udp->entity.kind = (a_byte_il_entry_kind)iek_namespace;
  udp->entity.ptr = (char *)nsp;
  udp->is_using_directive = TRUE;
  udp->compiler_generated = compiler_generated;
  udp->decl_sequence_number = ++decl_seq_counter;
  add_to_using_decls_list(udp, depth);
  /* Activate it. */
  add_active_using_directive(udp, depth);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!compiler_generated) {
    /* Not a compiler-generated using directive for an unnamed namespace. */
    add_to_source_sequence_list((char *)udp, (an_il_entry_kind)iek_using_decl);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* make_using_directive */


static void namespace_declaration(a_token_kind  *final_token)
/*
Scan a namespace declaration, which may be an original namespace definition,
an extension namespace definition, an unnamed namespace definition, or a
namespace alias definition.  The syntax is:

  original-namespace-definition:
    namespace identifier { namespace-body }

  extension-namespace-definition:
    namespace original-namespace-name { namespace-body }

  unnamed-namespace:
    namespace { namespace-body }

  namespace-alias-definition:
    namespace identifier = qualified-namespace-specifier;

*final_token is set to tok_semicolon if this is a namespace alias definition
and to tok_brace otherwise; the final token is swallowed by the caller.
*/
{
  a_source_position           namespace_pos;
  a_namespace_ptr             nsp;
  a_symbol_ptr                ns_sym = NULL, sym;
  a_symbol_locator            locator;
  a_boolean                   is_unnamed_namespace = FALSE;
  a_boolean                   is_namespace_alias = FALSE;
  a_scope_pointers_block_ptr  pointers_block;
  a_boolean                   err = FALSE;
  a_symbol_reference_kind     srk_flags = SRK_DECLARATION;
  a_boolean                   bad_scope_for_namespace_def = FALSE;
  a_source_sequence_entry_ptr namespace_ssep = NULL;
  a_boolean		      namespace_scope_pushed = FALSE;

  db_enter(3, "namespace_declaration");
  /* Save the source position of the declaration. */
  namespace_pos = pos_curr_token;
  /* A namespace declaration is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_namespaces_in_embedded_cplusplus);
  /* Bypass "namespace". */
  (void)get_token();
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
    fputs("namespace_declaration: adding empty ss entry\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  namespace_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (curr_token == tok_lbrace) {
    /* This must be an unnamed namespace definition. */
    is_unnamed_namespace = TRUE;
  } else if (is_generalized_identifier_start(GID_NO_OPTIONS)) {
    /* Save the identifier's locator before bypassing it. */
    locator = locator_for_curr_id;
    /* Issue an error if this is not a simple identifier name. */
    if (locator.is_qualified_name) {
      error(ec_qualified_name_not_allowed);
      set_to_error_locator(locator);
      err = TRUE;
    } else if (locator.is_operator_name || locator.is_conversion_name) {
      error(ec_operator_name_not_allowed);
      set_to_error_locator(locator);
      err = TRUE;
    }  /* if */
    if (get_token() == tok_assign) {
      /* This must be a namespace alias definition. */
      is_namespace_alias = TRUE;
    }  /* if */
  } else {
    add_stop_token(tok_semicolon);
    add_stop_token(tok_lbrace);
    (void)required_token(tok_identifier, ec_exp_identifier);
    set_to_error_locator(locator);
    remove_stop_token(tok_semicolon);
    remove_stop_token(tok_lbrace);
  }  /* if */
  if (depth_scope_stack != depth_innermost_namespace_scope) {
    /* The current scope is not the file scope or a namespace scope. */
    if (!is_namespace_alias) {
      /* This is a namespace definition of some sort, which can only occur
         in a file or namespace scope. */
      pos_error(ec_namespace_def_not_allowed, &namespace_pos);
      set_to_error_locator(locator);
      err = TRUE;
      bad_scope_for_namespace_def = TRUE;
    } else {
      /* This is a namespace alias definition, which can also occur in
         function and block scopes. */
      if (scope_stack[depth_scope_stack].kind != (a_scope_kind)sck_function &&
          scope_stack[depth_scope_stack].kind != (a_scope_kind)sck_block) {
        pos_error(ec_namespace_alias_def_not_allowed, &namespace_pos);
        set_to_error_locator(locator);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_unnamed_namespace) {
    /* No identifier -- this is an unnamed namespace definition. */
    if (!err) {
      /* See if this is its first definition in the current scope or if this
         is an extension. */
      pointers_block =
                  assoc_pointers_block_of(&scope_stack[depth_scope_stack]);
      ns_sym = pointers_block->unnamed_namespace_sym;
      if (ns_sym == NULL) {
        /* This is the first definition.  Create the symbol. */
        ns_sym = make_unnamed_namespace_symbol(&pos_curr_token);
        pointers_block->unnamed_namespace_sym = ns_sym;
      } else {
        /* A definition for the unnamed namespace has already appeared.  This
           definition will extend it, so reuse the symbol that was found. */
      }  /* if */
      make_locator_for_symbol(ns_sym, &locator);
    }  /* if */
  } else {
    /* A named namespace definition or a namespace alias.  Look up the
       identifier (which should be the current token) and see if it is
       already a namespace name in the current scope. */
    if (!err) {
      ns_sym = curr_scope_id_lookup(&locator, IDL_NO_OPTIONS);
      if (ns_sym != NULL) {
        /* A name was found in the current scope. */
        a_boolean  ns_sym_was_alias =
                       ns_sym->kind == (a_symbol_kind)sk_namespace &&
                       ns_sym->variant.namespace_info.ptr->is_namespace_alias; 

        if (microsoft_mode && !is_namespace_alias && ns_sym_was_alias) {
          /* In Microsoft mode, a namespace alias name can be used to define
             a namespace extension for the aliased namespace. */
          ns_sym = (a_symbol_ptr)
             skip_namespace_aliases(ns_sym->variant.namespace_info.ptr)->
                                                    source_corresp.assoc_info;
        } else if (ns_sym->kind != (a_symbol_kind)sk_namespace ||
                   is_namespace_alias != ns_sym_was_alias) {
          /* The namespace name should not conflict with the declaration of
             another entity.  Furthermore, an alias should not be redeclared
             as a namespace name, nor should a plain namespace name be
             redeclared as an alias. */
          pos_st_error(ec_id_already_declared, &locator.source_position,
                       locator.symbol_header->identifier);
          ns_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* The closing right brace or semicolon will be swallowed by the caller. */
  *final_token = is_namespace_alias ? tok_semicolon : tok_rbrace;
  if (is_namespace_alias) {
    /* Bypass the "=". */
    (void)get_token();
    add_stop_token(tok_semicolon);
    if (!is_decl_qualified_name_start()) {
      /* A namespace alias definition requires a (possibly qualified)
         namespace or class name to the right of the "=". */
      discard_curr_construct_pragmas();
      syntax_error(ec_exp_identifier);
    } else {
      /* Look up the namespace specifier. */
      sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
                                                       ilm_namespace, &err);
      if (!err) {
        if (sym != NULL &&
            locator_for_curr_id.specific_symbol->kind ==
                                  (a_symbol_kind)sk_namespace_projection &&
            locator_for_curr_id.specific_symbol->ambiguous) {
          /* The name for which an alias is being declared is ambiguous. */
          sym_error(ec_ambiguous_name, locator_for_curr_id.specific_symbol);
        } else if (sym == NULL || sym->kind != (a_symbol_kind)sk_namespace) {
          /* Either nothing was found or what was found was not a namespace. */
          error(ec_missing_namespace_name);
        } else {
          if (ns_sym != NULL &&
              ns_sym->variant.namespace_info.ptr != NULL) {
            if (skip_namespace_aliases(ns_sym->variant.namespace_info.ptr) ==
                     skip_namespace_aliases(sym->variant.namespace_info.ptr)) {
              /* Redefining the alias to the same thing. */
              record_symbol_declaration(srk_flags, ns_sym,
                                        &locator.source_position,
                                        namespace_ssep);
            } else {
              pos_sy_error(ec_already_defined, &locator.source_position,
                           ns_sym);
            }  /* if */
          } else {
            if (ns_sym == NULL) {
              /* Create a namespace symbol to represent the alias.  Its
                 creation was delayed till all the error cases had been
                 dispensed with, to avoid creating a symbol with no namespace
                 to bind to. */
              ns_sym = enter_symbol((a_symbol_kind)sk_namespace, &locator,
                                    depth_scope_stack,
                                    /*suppress_redecl_error=*/TRUE);
            }  /* if */
            /* Now create a namespace entry.  It will point to the namespace
               entry that was just looked up. */
            nsp = alloc_namespace(/*is_alias=*/TRUE);
            nsp->variant.assoc_namespace = sym->variant.namespace_info.ptr;
            set_source_corresp(&nsp->source_corresp, ns_sym);
            set_namespace_membership(ns_sym, &nsp->source_corresp,
                                     (a_namespace_ptr)NULL);
            nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
            ns_sym->variant.namespace_info.ptr = nsp;
            add_to_namespaces_list(nsp);
            record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION,
                                      ns_sym, &locator.source_position,
                                      namespace_ssep);
          }  /* if */
          mark_referenced(sym, &pos_curr_token);
        }  /* if */
      }  /* if */
      if (!err && ns_sym != NULL) {
        /* Do processing required for any pragmas bound to the current
           declaration. */
        process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      } else {
        discard_curr_construct_pragmas();
      }  /* if */
      /* Bypass the identifier. */
      (void)get_token();
    }  /* if */
    remove_stop_token(tok_semicolon);
    if (required_token_no_advance(tok_semicolon, ec_exp_semicolon)) {
      /* Closing semicolon was found. */
      cannot_bind_to_curr_construct();
    } else {
      discard_curr_construct_pragmas();
    }  /* if */
  } else if (bad_scope_for_namespace_def) {
    /* Attempting to define a namespace within something other than the
       file scope or a namespace scope.  Ignore all the declarations between
       the braces. */
    discard_curr_construct_pragmas();
    if (curr_token == tok_lbrace) {
      /* Ignore the namespace definition. */
      flush_until_matching_token();
      /* The closing right brace will be swallowed by the caller. */
      *final_token = tok_rbrace;
    }  /* if */
  } else {
    /* Namespace definition. */
    if (ns_sym == NULL) {
      if (locator.symbol_header == symbol_for_namespace_std->header &&
          depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
          !locator.is_error) {
        /* This is the initial explicit declaration of namespace "std".
           Reuse the predeclared symbol. */
        ns_sym = symbol_for_namespace_std;
        enter_symbol_for_namespace_std(&locator);
        srk_flags |= SRK_DEFINITION;
      } else {
        /* Create a namespace symbol. */
        ns_sym = enter_symbol((a_symbol_kind)sk_namespace, &locator,
                              depth_scope_stack,
                              /*suppress_redecl_error=*/TRUE);
      }  /* if */
    }  /* if */
    if (ns_sym->variant.namespace_info.ptr == NULL) {
      /* Original definition -- allocate the namespace entry. */
      nsp = alloc_namespace(/*is_alias=*/FALSE);
      set_source_corresp(&nsp->source_corresp, ns_sym);
      if (is_unnamed_namespace) nsp->source_corresp.name = NULL;
      set_namespace_membership(ns_sym, &nsp->source_corresp,
                               (a_namespace_ptr)NULL);
      nsp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
      ns_sym->variant.namespace_info.ptr = nsp;
      /* Set a flag indicating that this namespace is itself an unnamed
         namespace or is enclosed by an unnamed namespace. */
      if (is_unnamed_namespace ||
          (ns_sym->parent.namespace_ptr != NULL &&
           symbol_supplement_for_namespace(ns_sym->parent.namespace_ptr)->
                                                within_unnamed_namespace)) {
        ns_sym->variant.namespace_info.extra_info->
                                           within_unnamed_namespace = TRUE;
      }  /* if */
      add_to_namespaces_list(nsp);
      /* Do processing required for any pragmas bound to the current
         declaration. */
      process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      if (!ignore_std_namespace ||
          ns_sym != symbol_for_namespace_std) {
        /* Push a scope for the scanning the namespace body.  This is not done
           when using the g++ compatibility feature that makes "std" a
           synonym for the global namespace. */
        (void)push_namespace_scope((a_scope_kind)sck_namespace, nsp);
        nsp->variant.assoc_scope->variant.assoc_namespace = nsp;
        namespace_scope_pushed = TRUE;
      }  /* if */
      if (is_unnamed_namespace) {
        /* The model for the initial definition of an unnamed namespace
             namespace { ... }
           is this:
             namespace UNIQUE { }
             using namespace UNIQUE;
             namespace UNIQUE { ... }
           This enables this sort of code to work:
             namespace {
               int i;
               int j = ::i;       // lookup rules find UNIQUE::i
             }
           The model is implemented by immediately popping the
           original definition of the unnamed namespace, inserting the
           implicit using directive, and then reopening the namespace as
           as an extension. */
        pop_scope();
        /* Do an implicit "using" directive of the unnamed namespace. */
        make_using_directive(nsp, depth_scope_stack, &pos_curr_token,
                             /*compiler_generated=*/TRUE);
        (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                   nsp);
        scope_stack[depth_scope_stack].
                              explicitly_declared_namespace_extension = TRUE;
      }  /* if */
      srk_flags |= SRK_DEFINITION;
    } else {
      /* Do processing required for any pragmas bound to the current
         declaration. */
      process_curr_construct_pragmas(ns_sym, (a_statement_ptr)NULL);
      /* An extension of the original definition of this namespace -- push
         a scope for scanning the namespace body. */
      nsp = ns_sym->variant.namespace_info.ptr;
      if (!ignore_std_namespace ||
          ns_sym != symbol_for_namespace_std) {
        /* Push a scope for the scanning the namespace body.  This is not done
           when using the g++ compatibility feature that makes "std" a
           synonym for the global namespace. */
        (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                   skip_namespace_aliases(nsp));
        scope_stack[depth_scope_stack].
                               explicitly_declared_namespace_extension = TRUE;
        namespace_scope_pushed = TRUE;
      }  /* if */
    }  /* if */
    record_symbol_declaration(srk_flags, ns_sym, &locator.source_position,
                              namespace_ssep);
    if (!required_token(tok_lbrace, ec_exp_lbrace)) {
      discard_curr_construct_pragmas();
    } else {
      /* Scan the namespace body. */
      add_stop_token(tok_rbrace);
      while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
        declaration(/*function_definition_allowed=*/TRUE,
                    /*is_old_style_param_decl=*/FALSE,
                    /*is_top_level_declaration=*/FALSE,
                    /*marked_as_gnu_extension=*/FALSE,
                    (a_param_id_ptr)NULL, (a_source_range *)NULL);
      }  /* while */
      remove_stop_token(tok_rbrace);
      /* Process pragmas associated with the closing brace before the current
         scope is popped and before add_end_of_construct_source_sequence_entry
         is called. */
      process_curr_token_pragmas();
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the namespace
       definition. */
    add_end_of_construct_source_sequence_entry(
                                        (char *)nsp,
                                        (a_byte_il_entry_kind)iek_namespace);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (required_token_no_advance(tok_rbrace, ec_exp_rbrace)) {
      /* Closing right brace was found. */
      cannot_bind_to_curr_construct();
    } else {
      discard_curr_construct_pragmas();
    }  /* if */
    if (namespace_scope_pushed) {
      /* Pop the namespace or namespace-extension scope. */
      pop_namespace_scope();
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* If (because of an error) an empty source-sequence entry was left in the
     list, remove it now. */
  if (namespace_ssep != NULL &&
      namespace_ssep->entity.kind == (a_byte_il_entry_kind)iek_none) {
    remove_from_src_seq_list(namespace_ssep);
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  db_exit();
}  /* namespace_declaration */


static void using_directive(void)
/*
Scan a using directive.  Its syntax is:

  using namespace namespace-name

A using-directive entry is created and activated for the current scope.
*/
{
  a_source_position              decl_start_pos;
  a_symbol_ptr                   sym;
  a_boolean                      err = FALSE;

  db_enter(3, "using_directive");
  decl_start_pos = pos_curr_token;
  /* A using-directive is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_namespaces_in_embedded_cplusplus);
  /* Bypass "using" and "namespace". */
  (void)get_token();
  (void)get_token();
  add_stop_token(tok_semicolon);
  if (!is_decl_qualified_name_start()) {
    syntax_error(ec_exp_identifier);
    /* Ignore pragma declarations. */
    discard_curr_construct_pragmas();
  } else {
    /* Scan the namespace name. */
    sym = coalesce_and_lookup_generalized_identifier(GID_NO_OPTIONS,
                                                     ilm_namespace, &err);
    if (err) {
      /* A diagnostic has already been issued. */
    } else if (sym == NULL || sym->kind != (a_symbol_kind)sk_namespace) {
      error(ec_missing_namespace_name);
      err = TRUE;
    } else if (locator_for_curr_id.specific_symbol->kind ==
                     (a_symbol_kind)sk_namespace_projection &&
               locator_for_curr_id.specific_symbol->ambiguous) {
      /* Note: the lookup returns the projection symbol, if there is one,
         in the locator, so that's what needed to be tested for ambiguity. */
      sym_error(ec_ambiguous_name, locator_for_curr_id.specific_symbol);
      err = TRUE;
    }  /* if */
    if (err) {
      /* Ignore pragma declarations. */
      discard_curr_construct_pragmas();
    } else {
      /* Pragmas cannot bind to a using declaration. */
      cannot_bind_to_curr_construct();
      mark_referenced(sym, &locator_for_curr_id.source_position);
      /* Allocate a using-directive entry specifying this namespace and
         activate it. */
      make_using_directive(sym->variant.namespace_info.ptr, depth_scope_stack,
                           &decl_start_pos, /*compiler_generated=*/FALSE);
    }  /* if */
    (void)get_token();
  }  /* if */
  remove_stop_token(tok_semicolon);
  /* Check for final semicolon in the caller. */
  db_exit();
}  /* using_directive */


a_using_decl_ptr make_using_decl(a_symbol_ptr      sym,
                                 a_source_position *pos)
/*
Allocate a using-decl entry, set its fields based on sym, add it to the list
for the current scope, and return a pointer to it.  This routine is used for
class member using-declarations and nonmember using-declarations; similar
processing is done for using-directives by make_using_directive.
*/
{
  a_using_decl_ptr  udp;
  an_il_entry_kind  kind;
  char              *entity;

  /* Determine the IL entity and the entity-kind, based on the symbol. */
  entity = il_entry_for_symbol(sym, &kind);
  check_assertion_str(kind != (an_il_entry_kind)iek_none,
                      "make_using_decl: no IL entry for symbol");
  /* Allocate a using-decl entry, and make it point to the IL entity. */
  udp = alloc_using_decl();
  udp->entity.kind = (a_byte_il_entry_kind)kind;
  udp->entity.ptr = entity;
  udp->position = *pos;
  /* Attach it the list for the current scope. */
  add_to_using_decls_list(udp, depth_scope_stack);

  return udp;
}  /* make_using_decl */


static void create_nonmember_using_declaration(
                                       a_symbol_ptr     sym,
                                       a_symbol_ptr     *overload_sym_ptr,
                                       a_symbol_ptr     other_decl,
                                       a_namespace_ptr  nsp,
                                       a_using_decl_ptr *prev_udp,
                                       a_boolean        is_list,
                                       a_boolean        suppress_redecl_error)
/*
Create a projection for symbol "sym" from namespace "nsp".  If this is part
of an overload set being imported, "is_list" will be TRUE and
"*overload_sym_ptr" will point to the overload symbol.  Even when it is not
part of an overload set, we may need to create such a set because existing
declarations in the scope are being overloaded.  "*prev_udp" is the previous
using-declaration structure for the using-declaration construct that is
currently being processed (NULL if none).
*/
{
  a_symbol_locator   locator;
  a_symbol_ptr       new_sym;
  a_symbol_ptr       overload_sym = *overload_sym_ptr;
  a_symbol_ptr       fund_sym = fundamental_symbol_of(sym);
  a_source_position  decl_pos;

  locator = locator_for_curr_id;
  clear_specific_symbol(locator);
  decl_pos = locator_for_curr_id.source_position;
  if (overload_sym == NULL) {
    /* If we bring in a type that was declared previously, suppress a
       redeclaration error.  This is similar to the case
       "typedef struct S {} S;". */
    if (!suppress_redecl_error && other_decl != NULL &&
        is_type_symbol(other_decl) && is_type_symbol(fund_sym)) {
      a_type_ptr  type1 = type_symbol_type(other_decl),
                  type2 = type_symbol_type(fund_sym);
      suppress_redecl_error = identical_types(type1, type2);
    }  /* if */
    /* No overloading. */
    new_sym = enter_namespace_projection_symbol(fund_sym, &locator,
                                                depth_scope_stack,
                                                suppress_redecl_error);
    /* If is_list is TRUE, there will be overloading on the next
       iteration of this loop. */
    if (is_list) { *overload_sym_ptr = new_sym; }
  } else if (already_in_lookup_set(overload_sym, sym)) {
    /* Don't try to add a symbol that is already pointed to by
       overload_sym. */
    goto done;
  } else if (conflicts_with_previous_function_decl(
                                         fund_sym, overload_sym, &decl_pos)) {
    /* A function introduced by a using declaration cannot have the
       same type as a function already declared in the scope
       (WP 7.3.3 [namespace.udecl] paragraph 12).  The diagnostic
       will have been issued by the subroutine; don't create a
       projection symbol. */
    /* In Microsoft mode, two distinct IL entries may have been created for
       declarations of an extern "C" function in different namespaces.
       Microsoft compilers allow one of these to be brought into the scope
       of the other one with a using-declaration (though an attempt to call
       the function will result in an overload ambiguity). */
    goto done;
  } else {
    /* Add a new symbol to the overload set. */
    new_sym = make_namespace_projection_symbol(fund_sym,
                                               &locator.source_position,
                                               depth_scope_stack);
    overload_sym = add_symbol_to_overload_list(new_sym, overload_sym,
                                               /*use_namespace=*/FALSE,
                                               (a_namespace_ptr)NULL);
    if (overload_sym != *overload_sym_ptr) {
      *overload_sym_ptr = overload_sym;
      set_namespace_membership(overload_sym, (a_source_correspondence *)NULL,
                               (a_namespace_ptr)NULL);
    }  /* if */
  }  /* if */
  set_namespace_membership(new_sym, (a_source_correspondence *)NULL,
                           (a_namespace_ptr)NULL);
  if (fund_sym->kind == (a_symbol_kind)sk_undefined) {
    /* Undefined symbols have no IL entries, so don't create an
      IL entry for this using-declaration. */
  } else {
    /* Create a using-decl entry to represent this declaration in
       the IL. */
    a_using_decl_ptr  udp = make_using_decl(fund_sym, &decl_pos);
    /* Record the class that was actually specified in the qualified
       name in the source. */
    udp->qualifier.namespace_ptr = nsp;
    /* Update cross-reference and source-sequence info, if
       required. */
    record_using_decl(fund_sym, &decl_pos, udp, *prev_udp);
    *prev_udp = udp;
  }  /* if */
done:;
}  /* create_nonmember_using_declaration */


static void import_any_hidden_tags(a_symbol_ptr      other_decl,
                                   a_namespace_ptr   nsp,
                                   a_using_decl_ptr  *prev_udp,
                                   a_boolean         *redecl_error)
/*
A name that is imported by a using-declaration can refer to both tag names and
non-tag names.  In those cases, ordinary namespace-qualified lookup will only
find the non-tag, and this routine is used to also import the tag.  nsp is the
namespace from which to import the hidden tag.  *prev_udp is set to the using-
declaration structure that is created (if any).  *redecl_error is TRUE if and
only if a redeclaration error is issued.
*/
{
  /* Check if we missed a tag symbol; it should be imported too. */
  a_symbol_ptr      null_sym_ptr = NULL, tag_sym;
  a_symbol_locator  locator;

  *redecl_error = FALSE;
  locator = locator_for_curr_id;
  clear_specific_symbol(locator);
  /* Look for a tag symbol in the namespace referenced by the
     using-declaration. */
  if (nsp == NULL) {
    tag_sym = file_scope_id_lookup(il_header.primary_scope,
                                   &locator,
                                   IDL_MUST_BE_TAG |
                                   IDL_DIRECT_NAMESPACE_MEMBERS_ONLY);
  } else {
    tag_sym = namespace_qualified_id_lookup(
                          &locator, nsp,
                          IDL_MUST_BE_TAG | IDL_DIRECT_NAMESPACE_MEMBERS_ONLY);
  }  /* if */
  if (tag_sym != NULL) {
    a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
    clear_specific_symbol(locator);
    if (!is_class_template_symbol(tag_sym) &&
        !(other_decl != NULL && is_file_or_namespace_scope(ssep) &&
          symbols_are_lookup_equivalent(fundamental_symbol_of(tag_sym),
                                        fundamental_symbol_of(other_decl)))) {
      /* We found a tag that was masked by another declaration (sym),
         and importing it is not just a redeclaration. */
      create_nonmember_using_declaration(tag_sym, &null_sym_ptr,
                                         other_decl, nsp, prev_udp,
                                         /*is_list=*/FALSE,
                                         /*suppress_redecl_error=*/FALSE);
      *redecl_error = (curr_scope_id_lookup(
                          &locator, IDL_MUST_BE_TAG | IDL_PROJ_SYMBOL_ALLOWED)
                         != NULL);
    }  /* if */
  }  /* if */
}  /* import_any_hidden_tags */


static void nonmember_using_declaration(void)
/*
Scan a using_declaration in a nonclass scope.  Its syntax is:

  using qualified-name ;

A sk_namespace_projection is created and added to the symbol table for the
current scope.
*/
{
  a_symbol_ptr             sym, fund_sym, overload_sym, other_decl,
                             fund_other_decl;
  a_boolean                err = FALSE;
  a_symbol_locator         locator;
  a_boolean                is_list = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(3, "nonmember_using_declaration");
  /* A using declaration is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_using_decl_in_embedded_cplusplus);
  /* Bypass "using". */
  (void)get_token();
  add_stop_token(tok_semicolon);
  if (!is_decl_qualified_name_start()) {
    syntax_error(ec_exp_identifier);
    /* Ignore pragma declarations. */
    discard_curr_construct_pragmas();
  } else {
    sym = coalesce_and_lookup_generalized_identifier(
                GID_NO_OPTIONS | GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
    if (err) {
      /* Diagnostic has already been issued. */
    } else if (sym == NULL) {
      str_error(ec_undefined_identifier,
                locator_for_curr_id.symbol_header->identifier);
      err = TRUE;
    } else if (!locator_for_curr_id.is_qualified_name &&
               !nonstandard_using_decl_allowed) {
      /* An unqualified name is not allowed here.  This is optionally
         permitted because the Sun 5.0 compiler accepts an unqualified
         name in a using-declaration. */
      error(ec_namespace_qualified_name_required);
      err = TRUE;
    } else if (locator_for_curr_id.is_class_member) {
      /* A class-qualified name is not allowed here. */
      error(ec_class_qualified_name_not_allowed);
      err = TRUE;
    } else if (locator_for_curr_id.is_template_id) {
      /* A template-id (that is, template-name<template-args>) is not allowed
         here. */
      error(ec_template_id_not_allowed);
      err = TRUE;
    } else if (sym->kind == (a_symbol_kind)sk_namespace) {
      pos_error(ec_namespace_name_not_allowed,
                &locator_for_curr_id.source_position);
      err = TRUE;
    }  /* if */
    if (err) {
      /* Ignore pragma declarations. */
      discard_curr_construct_pragmas();
    } else {
      a_namespace_ptr  nsp;
      /* Pragmas cannot bind to a using declaration. */
      cannot_bind_to_curr_construct();
      if ((nsp = qualifier_namespace_ptr(locator_for_curr_id)) != NULL &&
          ssep->il_scope != NULL &&
          ssep->il_scope->kind == (a_scope_kind)sck_namespace &&
          ssep->il_scope->variant.assoc_namespace ==
                                               skip_namespace_aliases(nsp)) {
        /* Attempting a using-declaration with a namespace qualifier that is
           the same as the current namespace:
             namespace N { int i; using N::i; }
           Issue a warning and ignore the using-declaration. */
        warning(ec_useless_using_declaration);
      } else if (depth_scope_stack == DEPTH_OF_FILE_SCOPE && nsp == NULL) {
        /* Attempting a using declaration at file scope with name already
           declared in the file scope -- e.g.,
             int i; using ::i;
           Issue a warning and ignore the using-declaration. */
        check_assertion(locator_for_curr_id.is_global_qualified_name ||
                        nonstandard_using_decl_allowed);
        warning(ec_useless_using_declaration);
      } else {
        check_assertion(qualifier_namespace_ptr(locator_for_curr_id) != NULL ||
                        locator_for_curr_id.is_global_qualified_name ||
                        nonstandard_using_decl_allowed);
        locator = locator_for_curr_id;
        clear_specific_symbol(locator);
        /* Look for a declaration of the same name in the current scope. */
        (void)curr_scope_id_lookup(&locator, IDL_PROJ_SYMBOL_ALLOWED);
        other_decl = locator.specific_symbol;
        fund_other_decl = (other_decl == NULL) ?
                                     NULL : fundamental_symbol_of(other_decl);
        overload_sym = NULL;
        if (is_function_symbol(sym) ||
            sym->kind == (a_symbol_kind)sk_function_template) {
          /* The specified name represents a function or function template (or
             overload set thereof) so we need to create or add to an overload
             set in the current scope, too. */
          if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
            /* Using an overload set. */
            is_list = TRUE;
            sym = sym->variant.overloaded_function.symbols;
          }  /* if */
          if (other_decl != NULL) {
            if (is_function_symbol(fund_other_decl) ||
                fund_other_decl->kind == (a_symbol_kind)sk_function_template) {
              /* Overloading is okay. */
              overload_sym = other_decl;
            } else {
              /* There is no function symbol in the current scope with which
                 the new symbol should be overloaded. */
            }  /* if */
          }  /* if */
        }  /* if */
        fund_sym = fundamental_symbol_of(sym);
        if (other_decl != NULL && overload_sym == NULL &&
            other_decl->decl_position.seq != 0 &&
            is_file_or_namespace_scope(ssep) &&
            symbols_are_lookup_equivalent(fund_sym, fund_other_decl)) {
          /* This is a duplicate using declaration of something other than a
             function or function template.  7.3.3 [namespace.udecl] para 7
             says duplicates are allowed in file or namespace scope, so ignore
             the declaration.  (However, do not ignore the using declaration
             if it duplicates a built-in declaration, i.e. seq == 0). */
        } else {
          a_using_decl_ptr  prev_udp = NULL;
          a_boolean         suppress_redecl_error = FALSE;
          /* Create the new sk_namespace_projection symbol(s). */
          if (!is_tag_symbol(fund_sym)) {
            /* Check if we missed a tag symbol; it should be imported too. */
            import_any_hidden_tags(other_decl, nsp, &prev_udp,
                                     &suppress_redecl_error);
          }  /* if */
          /* If we're importing a typedef that redeclares an existing type
             to the same name, inhibit the declaration error. */
          if (fund_other_decl != NULL &&
              fund_sym->kind == (a_symbol_kind)sk_type) {
            a_symbol_ptr  prev_tag_sym = NULL;
            if (is_tag_symbol(fund_other_decl)) {
              /* If the previous declaration was a tag name, and the new
                 declaration is also a tag name, we should have caught the
                 duplicate earlier. */
              check_assertion(!is_tag_symbol(fund_sym));
              prev_tag_sym = fund_other_decl;
            } else {
              /* Look up a tag in the current scope: */
              clear_specific_symbol(locator);
              prev_tag_sym = curr_scope_id_lookup(
                                   &locator,
                                   IDL_MUST_BE_TAG | IDL_PROJ_SYMBOL_ALLOWED);
            }  /* if */
            if (prev_tag_sym != NULL) {
              /* There was a previous tag.  If the newly imported type is
                 identical to the tagged type, suppress the redeclaration
                 error. */
              a_type_ptr  tp1 = type_symbol_type(prev_tag_sym);
              a_type_ptr  tp2 = type_symbol_type(fund_sym);
              if (identical_types(tp1, tp2)) { suppress_redecl_error = TRUE; }
            }  /* if */
          }  /* if */
          for (; sym != NULL; sym = is_list ? sym->next : NULL) {
            create_nonmember_using_declaration(sym, &overload_sym, other_decl,
                                               nsp, &prev_udp, is_list,
                                               suppress_redecl_error);
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Bypass the identifier. */
    (void)get_token();
  }  /* if */
  remove_stop_token(tok_semicolon);
  /* Check for final semicolon in the caller. */
  db_exit();
}  /* nonmember_using_declaration */


static a_boolean check_for_missing_declarator(
                                    a_decl_flag_set   dso_flags,
                                    a_type_ptr        type_ptr,
                                    a_storage_class   storage_class,
                                    a_boolean         is_old_style_param_decl,
                                    a_boolean         is_linkage_spec_decl,
                                    a_source_position *decl_start_pos,
                                    a_boolean         decl_spec_err)
/*
The decl-specifiers have been scanned.  Check for the case in which a
declarator is missing, in which case return TRUE after issuing appropriate
diagnostics.  (It is not always an error -- for example, an "autonomous"
class definition like "struct S { int i; };".)

Three of the parameters relay information returned from decl_specifiers:
dso_flags is a vector of flags, type_ptr is the type that was specified, and
storage_class is the storage class specified.  is_old_style_param_decl is
TRUE if a parameter declaration from a non-prototyped parameter list is
being scanned.  is_linkage_spec_decl is TRUE if the declaration is part of a
linkage-specification declaration.  decl_start_pos indicates the source
position of the first token of the current declaration.  decl_spec_err is
TRUE if an error was reported while the decl-specifiers were scanned.
*/
{
  a_boolean          declarator_omitted = FALSE;
  a_boolean          declares_something;
  a_boolean          defines_something;
  a_boolean          inline_specified;
  an_error_severity  severity;

  declares_something = ((dso_flags & DSO_DECLARES_SOMETHING) != 0);
  if (curr_token == tok_semicolon) {
    defines_something = ((dso_flags & DSO_DEFINES_SOMETHING) != 0);
    inline_specified = ((dso_flags & DSO_INLINE) != 0);

    declarator_omitted = TRUE;
    if (decl_spec_err) {
      /* Don't issue further errors on this declaration. */
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
      { a_type_ptr  tp = skip_typerefs(type_ptr);
        if (defines_something) {
          tp->autonomous_primary_tag_decl = TRUE;
        } else {
          (void)set_src_seq_secondary_decl_fields((char *)tp, (a_type_ptr)NULL,
                                                  SSSD_AUTONOMOUS_TAG_DECL);
        }  /* if */
      }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (!declares_something && C_dialect == C_dialect_cplusplus &&
               defines_something && type_ptr->kind == (a_type_kind)tk_union &&
               storage_class != (a_storage_class)sc_typedef) {
      /* Special C++ case:  the declaration of an anonymous union.   Do the
         required error checking and special processing, including creation
         of a variable which will represent the anonymous union and with
         which its fields will be aliased. */
      check_assertion(is_unnamed_tag_symbol(
                        (a_symbol_ptr)(type_ptr->source_corresp.assoc_info)));
      make_anonymous_union_variable(type_ptr, storage_class);
      /* The anonymous union variable is marked as referenced, as are all
         unnamed entities.  So its type is also marked referenced. */
      type_ptr->source_corresp.referenced = TRUE;
    } else if (is_linkage_spec_decl && is_enum_type(type_ptr)) {
      /* This is a declaration like
                      extern "C" enum E { e1, e2, e3 };
         which is not allowed (inference from ARM 7.4). */
      pos_error(ec_enum_not_allowed, decl_start_pos);
    } else {
      if (storage_class == (a_storage_class)sc_typedef) {
        /* Typedef declaration with no declarator. */
        severity = es_warning;
        if (declares_something ||
            (C_mode() && defines_something && is_enum_type(type_ptr))) {
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (defines_something) {
          skip_typerefs(type_ptr)->autonomous_primary_tag_decl = TRUE;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if ASM_FUNCTION_ALLOWED
      } else if (storage_class == (a_storage_class)sc_asm) {
        pos_error(ec_bad_asm_function_def, &pos_curr_token);
#endif /* ASM_FUNCTION_ALLOWED */
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
          severity = strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning;
          diagnostic(severity, ec_useless_decl);
        }  /* if */
        /* A storage class can only be specified for an object or a function
           (ARM 7.1.1). */
        if (storage_class != (a_storage_class)sc_unspecified) {
          severity = (C_mode() || any_cfront_mode() || microsoft_mode) ?
                       es_warning : es_discretionary_error;
          diagnostic(severity, ec_storage_class_requires_function_or_variable);
        }  /* if */
        /* ARM 7.1.6 implies that the absence of an object in this declaration
           makes it ill-formed.  Is the implication strong enough to justify
           an error here? */
        if (is_qualified_type(type_ptr)) {
          severity = (C_dialect == C_dialect_cplusplus && strict_ansi_mode) ?
                       strict_ansi_error_severity : es_warning;
          pos_diagnostic(severity, ec_useless_type_qualifiers,
                         decl_start_pos);
        }  /* if */
        /* Inline can only be specified for a function (ARM 7.1.2). */
        if (inline_specified) {
          pos_error(ec_inline_and_nonfunction, decl_start_pos);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (defines_something || declares_something) {
          /* This is a class/struct/union or enum declaration. */
          a_type_ptr  tp = skip_typerefs(type_ptr);
          if (defines_something) {
            tp->autonomous_primary_tag_decl = TRUE;
          } else {
            (void)set_src_seq_secondary_decl_fields((char *)tp,
                                                    (a_type_ptr)NULL,
                                                    SSSD_AUTONOMOUS_TAG_DECL);
          }  /* if */
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
    cannot_bind_to_curr_construct();
  } else if ((dso_flags & DSO_DANGLING_TYPE_SPECIFIER) ||
             (!C_mode() && identifier_is_template_id() && !microsoft_mode)) {
    /* The "dangling type specifier" case -- a class, struct, union, or
       enum definition was followed by a type specifier keyword.  This is
       treated as a missing-semicolon error, since the type specifier can
       be taken as introducing a new declaration.  (A similar case is the
       identifier-but-not-declarator-id case -- which occurs when a
       template-id appears where a declarator was expected. */
    declarator_omitted = TRUE;
    if (decl_spec_err) {
      /* Don't issue further errors on this declaration. */
    } else if (declares_something) {
      if (is_old_style_param_decl) {
        /* An old style param declaration that introduces a named struct or
           enum type but has no declarator for the parameter. */
        pos_error(ec_decl_should_be_of_param, decl_start_pos);
      } else {
        /* Maybe something "struct A { ... } int i;", where the declaration
           is okay and the problem is that a semicolon is missing. */
      }  /* if */
    } else {
      /* A declaration that introduces an unnamed struct or enum type but has
         no declarator.  May or may not be in an old-style param list. */
      pos_error(ec_exp_identifier, &pos_curr_token);
    }  /* if */
    /* Issue the missing-semicolon error. */
    pos_error(ec_exp_semicolon, &pos_curr_token);
    discard_curr_construct_pragmas();
  }  /* if */
  return declarator_omitted;
}  /* check_for_missing_declarator */

static void report_member_function_redeclaration(
                                        a_symbol_locator       *locator,
                                        a_type_ptr             type,
                                        a_source_position_ptr  declarator_pos)
/*
A declaration of a class member function that is not also a definition
can not appear outside the class.  Upon entering this routine we already know
we encountered such a declaration: issue the appropriate diagnostic (except
for some situations in Microsoft mode).
*/
{
  a_symbol_ptr  sym = locator->specific_symbol;

  if (is_member_function_symbol(locator->specific_symbol)) {
    /* If this is a member function, but one with a type that doesn't
       match a previously declared member, see if it matches an
       instance of a member template.  If it does, assume that it is
       an attempt to declare a specialization with the incorrect
       old-style specialization syntax. */
    a_boolean     is_member_redecl;
    a_boolean     is_template_instance;

    is_member_redecl = member_function_redecl_sym(
                               sym, type, (a_template_param_ptr)NULL) != NULL;
    is_template_instance = has_matching_template_instance(
                                       sym, type, locator->template_arg_list);
    if (!is_member_redecl && is_template_instance) {
      pos_sy_error(ec_old_specialization_not_allowed,
                   &locator->source_position, sym);
      set_to_error_locator(*locator);
    } else {
      pos_sy_error(ec_member_function_redecl_outside_class,
                   declarator_pos, sym);
      set_to_error_locator(*locator);
    }  /* if */
  } else {
    pos_sy_error(ec_not_compatible_with_previous_decl, declarator_pos, sym);
    set_to_error_locator(*locator);
  }  /* if */
}  /* report_member_function_redeclaration */


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


void check_main_function(a_func_info_block_ptr  func_info,
                         a_type_ptr             type,
                         a_storage_class        *declared_storage_class,
                         a_boolean              *is_inline,
                         a_source_position_ptr  pos)
/*
Check some C++ constraints on the main() function declared with type "type"
and storage class "*declared_storage_class".  If "*is_inline" is TRUE, the
function was declared "inline" (or was defined as an in-class friend).
Additional information is provided through the "func_info" parameter, while
the "pos" parameter determines which positions should be reported in any
diagnostics.
*/
{
  /* Not a class or namespace member named "main". */
  a_routine_type_supplement_ptr  rtsp;
  a_type_ptr                     return_type;

  /* Perform some error checking that is specific to C++ and C99. */
  return_type = skip_typerefs(type)->variant.routine.return_type;
  if (c99_mode || !C_mode()) {
    a_type_ptr  int_type = integer_type((an_integer_kind)ik_int);
    if (!identical_types(return_type, int_type)) {
      /* main must return "int" (3.6.1). */
      pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_discretionary_severity : es_warning,
                     ec_bad_return_type_on_main, pos);
    }  /* if */
    /* "inline" isn't allowed in C++ and C99 modes. */
    if (*is_inline) {
      pos_error(ec_inline_main, pos);
      *is_inline = FALSE;
    }  /* if */
  }  /* if */
  if (!C_mode()) {
    rtsp = skip_typerefs(type)->variant.routine.extra_info;
    if (rtsp->routine_name_linkage_is_explicit) {
      pos_warning(ec_linkage_specifier_not_allowed, pos);
      rtsp->routine_name_linkage_is_explicit = FALSE;
    }  /* if */
    rtsp->routine_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    if (rtsp->exception_specification != NULL) {
      /* main() cannot have a throw specification, since there's no
         call stack to unwind from main. */
      pos_warning(ec_exception_specification_not_allowed,
                  &func_info->throw_position);
      rtsp->exception_specification = NULL;
    }  /* if */
    /* "static" is not allowed (ARM 3.4). */
    if (*declared_storage_class == (a_storage_class)sc_static) {
      pos_error(ec_static_not_allowed, pos);
      *declared_storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
  }  /* if */
}  /* check_main_function */


void declaration(a_boolean       function_definition_allowed,
                 a_boolean       is_old_style_param_decl,
                 a_boolean       is_top_level_declaration,
                 a_boolean       marked_as_gnu_extension,
                 a_param_id_ptr  param_id_list,
                 a_source_range  *linkage_spec_range_ptr)
/*
Scan a declaration (standard, 3.5).  If function_definition_allowed is TRUE,
alternatively scan a function-definition (3.7.1).  With that flag TRUE, this
routine also corresponds to an external-declaration (3.7).  param_id_list
is non-NULL if this declaration is for an old-style function parameter; in 
that case, the identifier declared must be on the list.
is_top_level_declaration is TRUE when a declaration appears at file scope
and is not part of any other declarative structure; it is used for
precompiled-header processing.  linkage_spec_range_ptr is non-NULL when this
declaration includes an explicit linkage specification, but is otherwise
NULL, even when it is part of a block of declarations governed by a linkage
specification (i.e., it is non-NULL for `extern "C" void f()' and NULL for
`extern "C" { void f() }'); when it is non-NULL, it indicates the source
range of the linkage specifier.

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
  a_boolean                    local_is_old_style_param_decl;
  a_storage_class              declared_storage_class, local_storage_class;
  a_type_ptr                   type_ptr, old_type;
  a_type_ptr	               local_type_ptr;
  a_boolean                    has_explicit_type_specifier;
  a_boolean	               defines_something;
  a_decl_flag_set              dso_flags, do_flags;
  a_type_qualifier_set         qualifiers;
  a_decl_modifiers_block       decl_modifiers, local_decl_modifiers;
  a_decl_flag_set              dsi_flags, di_flags;
  a_symbol_ptr                 symbol_ptr = NULL, ext_sym;
  a_boolean	               decl_specifiers_omitted = FALSE;
  a_boolean                    is_function, is_main_function;
  a_boolean                    is_static_data_member;
  a_symbol_locator             locator;
  a_param_id_ptr               param_id;
  a_func_info_block            func_info;
  a_boolean                    top_declarator_type_is_function;
  an_id_linkage_kind           linkage;
  a_boolean                    has_initializer;
  a_boolean                    has_parenthesized_initializer;
  a_boolean                    err = FALSE;
  a_boolean                    inline_specified;
  a_source_position            decl_start_pos, declarator_pos;
  a_source_position            declarator_start_pos;
  a_boolean                    need_semicolon_remove_stop_token = FALSE;
  a_boolean                    need_comma_remove_stop_token     = FALSE;
  a_boolean                    need_assign_remove_stop_token    = FALSE;
  a_boolean                    need_lbrace_remove_stop_token    = FALSE;
  a_boolean                    is_variable_def, incomplete_type_error_reported;
  a_boolean                    is_tentative_definition;
  a_variable_ptr               var_ptr;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                    first_declarator = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  an_attribute_ptr             prefix_attributes = NULL;
  char                         *asm_name = NULL;
#if GNU_EXTENSIONS_ALLOWED
  an_attribute_ptr             *last_prefix_attribute;
  an_attribute_ptr             attributes = NULL;
  a_source_position            asm_start_pos;
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_boolean                    access_checks_deferred = FALSE;
  a_token_kind                 final_token = tok_semicolon;
  a_boolean                    is_linkage_spec_decl = FALSE;
  a_boolean                    restore_name_linkage = FALSE;
  a_decl_pos_block             decl_pos_block;
  a_boolean                    microsoft_out_of_class_redecl = FALSE;

  db_enter(3, "declaration");

  if (gcc_mode && !marked_as_gnu_extension && curr_token == tok_extension) {
    /* Ignore the GNU C __extension__ annotation. */
    (void)get_token();
    marked_as_gnu_extension = TRUE;
  }  /* if */
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, decl_start_pos);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (decl_scope_level == depth_innermost_namespace_scope) {
    /* For each declaration at namespace scope, reset the source-sequence
       insert point for instantiations to NULL -- it will be set to point
       to the first source sequence entry that
       add_source_sequence_entry_to_list sees, which should be the first
       entry associated with the current declaration. */
    reset_ss_list_instantiation_insert_point();
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (linkage_spec_range_ptr != NULL) {
    /* The caller has already scanned the linkage specifier. */
    is_linkage_spec_decl = TRUE;
    restore_name_linkage = TRUE;
  }  /* if */
  if (is_linkage_spec_decl) {
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
  if (function_definition_allowed) {
    /* This is a file scope or namespace scope declaration.  Indicate
       that access checking should be deferred until the declarator has
       been scanned. */
    begin_deferral_of_access_checks();
    access_checks_deferred = TRUE;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    if (curr_token == tok_extern && next_token() == tok_string_literal) {
      /* This looks like a C++ linkage specification, which is "extern"
         followed by a string literal (e.g., "C++" or "C"). */
      linkage_specification(function_definition_allowed,
                            is_old_style_param_decl,
                            is_top_level_declaration, param_id_list);
      goto return_point;
    } else if (curr_token == tok_template ||
               curr_token == tok_export ||
               (microsoft_mode && curr_token == tok_extern &&
                next_token() == tok_template)) {
      /* Do the processing required for a template declaration.  If this is
         a top level declaration, the subroutine should not advance past the
         final token of the declaration. */
      a_template_decl_options_set td_flags = TDO_NO_OPTIONS;

      if (curr_token == tok_extern) {
        /* In Microsoft mode "extern template ..." is permitted. */
        (void)get_token();
        td_flags = TDO_EXTERN;
      }  /* if */
      template_directive_or_declaration(&final_token, td_flags);
      /* The terminating token will be either a semicolon or a right
         brace.  The latter has already been checked for, but the former
         has not. */
      if (final_token == tok_semicolon) {
        (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
      }  /* if */
      /* Swallow the current token if it is the same as final_token, then
         return. */
      goto advance_past_final_token;
    } else if (curr_token == tok_namespace) {
      /* Process a namespace definition or a namespace alias declaration. */
      namespace_declaration(&final_token);
      /* Swallow the current token if it is the same as final_token, then
         return. */
      goto advance_past_final_token;
    } else if (curr_token == tok_using) {
      /* A using-directive (which has the form "using namespace N;") or a
         using-declaration ("using N::x;" or "using ::x;"); */
      if (next_token() == tok_namespace) {
        using_directive();
      } else {
        nonmember_using_declaration();
      }  /* if */
      cannot_bind_to_curr_construct();
      goto check_for_semicolon;
    } else if (check_for_overload_anachronism()) {
      /* We check for and discard declarations of the form "overload f;" --
         issue diagnostics on pragmas that are trying to bind to an overload
         declaration. */
      cannot_bind_to_curr_construct();
      goto check_for_semicolon;
    }  /* if */
  }  /* if */
  add_stop_token(tok_semicolon);
  need_semicolon_remove_stop_token = TRUE;
  /* Set the flags for calling decl_specifiers. */
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED;
  if (curr_token == tok_asm || curr_token == tok_microsoft_asm) {
#if ASM_FUNCTION_ALLOWED
    if (curr_token == tok_microsoft_asm ||
        !function_definition_allowed || next_token() == tok_lparen) {
#endif /* ASM_FUNCTION_ALLOWED */
      /* Scan the asm declaration. */
      (void)asm_declaration(/*asm_decl_allowed=*/!is_old_style_param_decl,
                            /*is_asm_statement=*/FALSE);
      goto return_point;
#if ASM_FUNCTION_ALLOWED
    }  /* if */
    /* Not "asm (...)", so assume we have an asm function declaration --
       something like "asm void f(void) { ... }".  Note: we leave
       DSI_STORAGE_CLASS_SPECIFIER_ALLOWED unset when asm is the first
       specifier. */
#endif /* ASM_FUNCTION_ALLOWED */
  } else {
    dsi_flags |= DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
    dsi_flags |= DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
    /* Within a non-block linkage specification no storage class (except
       typedef?) is allowed (inferred from ARM 7.4). */
    if (is_linkage_spec_decl) {
      dsi_flags |= DSI_IS_LINKAGE_SPEC_DECL;
    }  /* if */
  }  /* if */
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
      dsi_flags |= DSI_INLINE_ALLOWED;
#if ASM_FUNCTION_ALLOWED
      /* "asm" is allowed only on function definitions at file scope. */
      dsi_flags |= DSI_ASM_ALLOWED;
#endif /* ASM_FUNCTION_ALLOWED */
    } else if (microsoft_bugs) {
      dsi_flags |= DSI_INLINE_ALLOWED;
    }  /* if */
  }  /* if */
  /* Scan the initial declaration specifiers (including storage class,
     type specifiers, and type qualifiers).  For a function definition,
     the specifiers can be omitted entirely. */
  if (!is_decl_start(/*expr_context=*/FALSE,
                     /*real_declarator_allowed=*/TRUE)) {
    if (function_definition_allowed && is_declarator_start()) {
      /* At file or namespace scope, a declarator with no decl-specifiers,
         apparently.  In C mode this could be a legal function definition.
         In C++ mode a diagnostic will be issued (usually just a warning). */
#if ASM_FUNCTION_ALLOWED
    } else if (curr_token == tok_asm) {
      /* The start of an asm function declaration.  ("asm" is not checked
         for by is_decl_start since it starts a declaration only in a
         restricted context.) */
#endif /* ASM_FUNCTION_ALLOWED */
    } else {
      /* Look for some cases that are obviously not the start of a declaration,
         and give a more specific "Expected a declaration" message. */
      if (curr_token == tok_semicolon) {
        if (is_linkage_spec_decl) {
          /* Something like: ``extern "C";'' -- Issue an error. */
          diagnostic(es_discretionary_error, ec_exp_declaration);
        } else if (strict_ansi_mode) {
          /* An empty declaration is ignored (as an extension in ANSI mode). */
          diagnostic(strict_ansi_discretionary_severity, ec_extra_semicolon);
        } else {
          remark(ec_extra_semicolon);
        }  /* if */
        cannot_bind_to_curr_construct();
      } else if (curr_token == tok_lbrace) {
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
        discard_curr_construct_pragmas();
      }  /* if */
      /* Give up on scanning a declaration (assume we're at the end of one). */
      goto advance_past_final_token;
    }  /* if */
  }  /* if */
continue_with_declaration:
  /* Initialize source position information associated with this
     declaration. */
  clear_decl_pos_block(&decl_pos_block);
  if (marked_as_gnu_extension) {
    dsi_flags |= DSI_MARKED_AS_GNU_EXTENSION;
  }  /* if */
  /* Scan the specifiers. */
  err = decl_specifiers(dsi_flags, &dso_flags, &declared_storage_class,
                        &type_ptr, &qualifiers, &prefix_attributes,
                        &decl_modifiers, &decl_pos_block);
#if GNU_EXTENSIONS_ALLOWED
  /* Find the last prefix_attribute. */
  last_prefix_attribute = &prefix_attributes;
  while (*last_prefix_attribute) {
    last_prefix_attribute = &(*last_prefix_attribute)->next;
  }  /* while */
#endif /* GNU_EXTENSIONS_ALLOWED */
  has_explicit_type_specifier =
                      ((dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) != 0);
  if (dso_flags & DSO_LINKAGE_SPEC_DECL) {
    /* A linkage-specifier will be found among the decl_specifiers only in
       Microsoft mode -- e.g., for a case like this:
         extern "C" __declspec(dllexport) void f();
       Moreover, it is allowed only if this is not already a linkage-specifier
       declaration -- i.e., an error should have been issued on
         extern "C" __declspec(dllexport) extern "C" void f();
    */
    check_assertion(microsoft_mode && !is_linkage_spec_decl);
    /* Set a flag to treat this as a normal linkage-specification
       declaration. */
    is_linkage_spec_decl = TRUE;
    /* push_name_linkage was called in decl_specifiers, and the corresponding
       pop must be done before exiting this routine. */
    restore_name_linkage = TRUE;
  } else if (is_linkage_spec_decl) {
    /* Record the fact that this declaration has a linkage specifier attached
       directly to it (as opposed to just being inside a linkage block). */
    decl_modifiers.direct_linkage_specifier = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Adjust the specifiers-range to reflect the fact that there is a
       linkage specification (which was scanned not by decl_specifiers but
       by the caller). */
    decl_pos_block.specifiers_range.start = linkage_spec_range_ptr->start;
    if (dso_flags & DSO_NO_DECL_SPECIFIERS) {
      decl_pos_block.specifiers_range.end = linkage_spec_range_ptr->end;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (dso_flags & DSO_NO_DECL_SPECIFIERS) {
    if (is_linkage_spec_decl) {
      /* This is something like ``extern "C" f();'' -- treat the linkage
         specifier like a decl-specifier, for the purposes of diagnostics. */
    } else {
      decl_specifiers_omitted = TRUE;
    }  /* if */
  }  /* if */
  /* The declaration can end at this point (";" is next). */
  if (!decl_specifiers_omitted &&
      check_for_missing_declarator(dso_flags, type_ptr, declared_storage_class,
                                   is_old_style_param_decl,
                                   is_linkage_spec_decl,
                                   &decl_start_pos, err)) {
    if (curr_token != tok_semicolon) {
      /* This must be a "dangling type specifier", and an error has already
         been issued on the missing semicolon.  required_token is not called
         because it would flush what is assumed to be the next declaration. */
      goto advance_past_final_token;
    }  /* if */
  } else if (curr_token == tok_void && C_dialect == C_dialect_pcc && 
             declared_storage_class == (a_storage_class)sc_typedef &&
             next_token() == tok_semicolon) {
    /* "typedef <something> void;" in pcc mode.  Usually "typedef int void;".
       Shows up in old pre-void-keyword code.  Ignored in pcc mode. */
    set_err_pos_to_curr_token();
    warning(ec_decl_of_void_ignored);
    cannot_bind_to_curr_construct();
    /* Advance past "void" to the semicolon. */
    (void)get_token();
    goto advance_past_final_token;
  } else {
    a_boolean is_constructor_or_destructor;

    inline_specified = ((dso_flags & DSO_INLINE) != 0);
    defines_something = ((dso_flags & DSO_DEFINES_SOMETHING) != 0);
    /* Set the various flags for declarator processing. */
    di_flags = DI_REAL_DECLARATOR_ALLOWED;
    if (C_dialect == C_dialect_cplusplus) {
      di_flags |= DI_PARENTHESIZED_INITIALIZER_ALLOWED;
      di_flags |= DI_OPERATOR_NAME_ALLOWED;
      if (declared_storage_class != (a_storage_class)sc_typedef &&
          (decl_scope_level == depth_innermost_namespace_scope ||
           (microsoft_mode &&
            depth_innermost_namespace_scope != NO_SCOPE_DEPTH))) {
        di_flags |= DI_QUALIFIED_NAME_ALLOWED;
      }  /* if */
    }  /* if */
    if (declared_storage_class == (a_storage_class)sc_typedef) {
      di_flags |= DI_IS_TYPEDEF_DECLARATION;
    }  /* if */
    if (is_old_style_param_decl) {
      di_flags |= DI_IS_PARAMETER_DECL;
      /* A variable length array declaration is permitted in an old-style
         parameter declaration. */
      if (vla_enabled) di_flags |= DI_VLA_ALLOWED;
    } else if (vla_enabled) {
      if (!function_definition_allowed &&
          declared_storage_class != (a_storage_class)sc_asm) {
        /* Not at file scope, so a VLA may appear on some declarations. */
        di_flags |= DI_VLA_ALLOWED;
      }  /* if */
    }  /* if */
    if (!has_explicit_type_specifier && qualifiers == TQ_NONE) {
      di_flags |= DI_NO_TYPE_SPECIFIERS;
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
      clear_func_info(&func_info);
#if ASM_FUNCTION_ALLOWED
      if (declared_storage_class == (a_storage_class)sc_asm) {
        func_info.is_asm_function = TRUE;
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!first_declarator &&
          depth_scope_stack == depth_innermost_namespace_scope) {
        /* This is a declaration at file scope, and not the first declarator
           in the declarator list.  As for the start of the declaration,
           set the source-sequence insert point for instantiations to NULL. */
        reset_ss_list_instantiation_insert_point();
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
      /* Look for optional attributes, which are added to the prefix
         attributes.  Note that the draft GCC manual for version 3.1
         says that in the future, these attributes may apply only to
         the next declarator, but that they presently apply to all
         declarators. */
      if (gcc_mode) {
        /* Scan the attributes. */
        attributes = scan_attributes();
        /* Add these to the prefix_attributes. */
        *last_prefix_attribute = attributes;
        /* And compute what's now the end of the prefix attributes. */
        last_prefix_attribute = last_attribute_link(last_prefix_attribute);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Save the source position of the first token of the declarator. */
      declarator_start_pos = pos_curr_token;
      declarator(di_flags, &do_flags, type_ptr, 
                 /*member_parent_type=*/(a_type_ptr)NULL, &locator,
                 &local_type_ptr, &declarator_ssep, &func_info,
                 &decl_pos_block, 
#if GNU_EXTENSIONS_ALLOWED
                 last_prefix_attribute
#else /* !GNU_EXTENSIONS_ALLOWED */
                 (an_attribute_ptr *)NULL
#endif /* !GNU_EXTENSIONS_ALLOWED */
                 );
#if GNU_EXTENSIONS_ALLOWED
      last_prefix_attribute = last_attribute_link(last_prefix_attribute);
      asm_name = NULL;
      if (gcc_mode) {
        /* Look for an asm() symbol name tag.  It is ignored on
           typedefs (with a warning). */
        asm_start_pos = pos_curr_token;
        asm_name = scan_asm_name();
        if (asm_name != NULL &&
            declared_storage_class == (a_storage_class)sc_typedef) {
          pos_warning(ec_asm_name_in_typedef, &asm_start_pos);
          asm_name = NULL;
        }  /* if */
        /* Look for optional (postfix) attributes. */
        attributes = scan_attributes();
        /* Combine the prefix and postfix attributes. */
        *last_prefix_attribute = attributes;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* If a parenthesized constructor declarator is scanned, di_flags would
         not have DI_IS_CONSTRUCTOR set, but do_flags would have
         DO_IS_CONSTRUCTOR turned on. Similarly for destructors. Update the
         local state with that information: */
      is_constructor_or_destructor =
                    ((do_flags & (DO_IS_CONSTRUCTOR | DO_IS_DESTRUCTOR)) != 0);
      /* declarator will have set error_position to the position of the
         declarator-id if this is a real declarator and the first token of
         the whole declarator if it is an abstract declarator. */
      declarator_pos = error_position;
      is_function = (declared_storage_class != (a_storage_class)sc_typedef &&
                     is_function_type(local_type_ptr));
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (is_old_style_param_decl &&
          func_info.prototype_scope_ss_list != NULL) {
        /* Move the source sequence list that had been entered into the
           function prototype scope to the current scope. */
        a_source_sequence_entry_ptr  head, tail;

        /* Identify the head and tail of the list that is pointed to from
           func info block. */
        head = func_info.prototype_scope_ss_list;
        for (tail = head;; tail = tail->next) {
          if (tail->next == NULL) break;
        }  /* for */
#if DEBUG
        if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
          fputs("declaration: moving ss list from func info to curr scope\n",
                f_debug);
        }  /* if */
#endif /* DEBUG */
        /* Append the list to the list for the current scope. */
        insert_src_seq_list(head, tail, depth_scope_stack,
                            (a_source_sequence_entry_ptr)NULL);
        /* Just to be neat. */
        func_info.prototype_scope_ss_list = NULL;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      is_main_function = FALSE;
      if (is_function && !is_error_locator(locator) &&
          locator.symbol_header->identifier != NULL &&
          (strcmp(locator.symbol_header->identifier, "main") == 0)) {
        /* Recognizing a declaration of function "main" is more than checking
           the identifier. */
        if (C_dialect == C_dialect_cplusplus) {
          if (locator.is_qualified_name ?
                !locator.is_file_scope_qualified_name :
                depth_innermost_namespace_scope != DEPTH_OF_FILE_SCOPE) {
            /* A declaration that's a qualified name (except ::main), or one
               that's unqualified but in a namespace scope, can't refer to
               global main. */
          } else {
            check_assertion(locator.specific_symbol == NULL ||
                            (!locator.specific_symbol->is_class_member &&
                             locator.specific_symbol->
                                          parent.namespace_ptr == NULL));
            func_info.is_main_function = is_main_function = TRUE;
          }  /* if */
        } else {
          /* C mode. */
          if (declared_storage_class == (a_storage_class)sc_unspecified ||
              declared_storage_class == (a_storage_class)sc_extern) {
            /* Not a static function named "main".  This is not an option
               in C++ (ARM 3.4). */
            func_info.is_main_function = is_main_function = TRUE;
          }  /* if */
        }  /* if */
        if (is_main_function) {
          check_main_function(&func_info, local_type_ptr,
                              &declared_storage_class, &inline_specified,
                              &locator.source_position);
        }  /* if */
      } else if (declared_storage_class == (a_storage_class)sc_typedef &&
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
      has_parenthesized_initializer =
                          ((do_flags & DO_PARENTHESIZED_INITIALIZER) != 0);
      if (is_function && any_cfront_mode()) {
        /* Check for the declaration with a "member function typedef" type --
           it is only  supposed to be used for pointer-to-member declarations
           (only in cfront compatibility mode). */
        if (check_member_function_typedef(local_type_ptr, &decl_start_pos)) {
          is_function = FALSE;
          local_type_ptr = type_ptr = error_type();
        }  /* if */
      }  /* if */
      /* top_declarator_type_is_function is TRUE if the fact that this is a
         function is derived from the declarator and not from a typedef.  It
         is sufficient that the result type is a function type and a
         declarator was scanned (but watch out for type qualifiers). */
      top_declarator_type_is_function = (is_function &&
				         skip_typerefs(local_type_ptr) !=
                                                      skip_typerefs(type_ptr));
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
      local_storage_class = declared_storage_class;
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
            /* Set the declared_type field in the param_id entry before the
               type is adjusted (e.g., decays from array to pointer). */
            param_id->declared_type = local_type_ptr;
#if GNU_EXTENSIONS_ALLOWED
            /* We need a copy of the attribute list so that we can
               apply the attributes when we create the variable
               corresponding to this parameter. */
            param_id->attributes = copy_attribute_list(prefix_attributes);
#endif /* GNU_EXTENSIONS_ALLOWED */
          }  /* if */
          /* Check that the type is legal, and do required adjustments. */
          check_and_adjust_parameter_type(&local_type_ptr, &decl_start_pos,
                                          prefix_attributes);
          is_function = top_declarator_type_is_function = FALSE;
          /* For pcc compatibility, promote float parameters to double. */
          if (C_dialect == C_dialect_pcc) {
            promote_float_to_double(local_type_ptr);
          }  /* if */
        }  /* if */
      } else if (local_storage_class != (a_storage_class)sc_typedef) {
        /* See if any type qualifiers were specified, and if they are okay. */
        check_type_qualifiers(&local_type_ptr, &declarator_pos);
      }  /* if */
      if (need_lbrace_remove_stop_token) {
        remove_stop_token(tok_lbrace);
        need_lbrace_remove_stop_token = FALSE;
      }  /* if */
#if ASM_FUNCTION_ALLOWED
      if (declared_storage_class == (a_storage_class)sc_asm) {
        if (!is_function) {
          /* Not a function definition. */
          pos_error(ec_bad_asm_function_def, &declarator_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
          set_to_named_error_locator(locator);
        } else {
          func_info.is_asm_function = TRUE;
          /* Issue a diagnostic about using a nonstandard feature. */
          if (strict_ansi_mode) {
            pos_diagnostic(strict_ansi_error_severity, ec_nonstd_asm_function,
                           &decl_start_pos);
          }  /* if */
        }  /* if */
      } else
#endif /* ASM_FUNCTION_ALLOWED */
      if (is_function && local_storage_class != (a_storage_class)sc_typedef) {
        if (local_storage_class != (a_storage_class)sc_unspecified &&
            local_storage_class != (a_storage_class)sc_extern &&
            local_storage_class != (a_storage_class)sc_static) {
          /* The storage class of a function must be extern or static. */
          pos_error(ec_bad_function_storage_class,
                    &decl_pos_block.storage_class_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (locator.specific_symbol != NULL &&
            locator.specific_symbol->is_class_member) {
          /* This is the definition of a static member function.  No storage
             class specifier (not even "static") is permitted. */
          if (local_storage_class != (a_storage_class)sc_unspecified) {
            an_error_severity  severity = es_error;
            if (!extern_inline_allowed && inline_specified &&
                local_storage_class == (a_storage_class)sc_static) {
              /* Just give a warning on this.  The storage class designation
                 is taken to be redundant, since all "inline" member functions
                 (both static and nonstatic, in the sense applied to member
                 functions) are "static" (in the sense of having internal
                 linkage). */
              severity = es_warning;
            }  /* if */
            pos_diagnostic(severity, ec_storage_class_not_allowed,
                           &decl_pos_block.storage_class_pos);
          }  /* if */
          /* Set the storage class to sc_unspecified for now.  It will be
             checked and reset if necessary in define_member_function. */
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      /* Check for restrictions on use of the "inline" specifier. */
      if (inline_specified) {
        if (!is_function) {
          /* Not a function declaration. */
          pos_error(ec_inline_and_nonfunction, &decl_start_pos);
        } else {
          func_info.is_inline = TRUE;
        }  /* if */
      }  /* if */
      /* Indicate whether the function type is based on a typedef. */
      func_info.function_type_from_typedef = !top_declarator_type_is_function;
      /* If the thing declared is a function, and if the token following looks
         like it could be part of a function-definition, go scan that.
         A very special case are Microsoft out-of-class member redeclarations
         (that are not definitions); they are handled by the code for out-of-
         class definitions. */
      microsoft_out_of_class_redecl = microsoft_mode &&
                                                  locator.is_class_member &&
                                                  curr_token == tok_semicolon;
      if ((function_definition_allowed || microsoft_out_of_class_redecl) &&
          is_function) {
        if (local_storage_class != (a_storage_class)sc_typedef &&
            (curr_token != tok_semicolon || microsoft_out_of_class_redecl) &&
            curr_token != tok_comma &&
            curr_token != tok_assign &&
#if GNU_EXTENSIONS_ALLOWED
            /* Attributes and asm names are only allowed on function
               declarations, not on function definitions. */
            curr_token != tok_attribute &&
            curr_token != tok_asm &&
#endif /* GNU_EXTENSIONS_ALLOWED */
            curr_token != tok_end_of_source) {
          a_boolean  is_function_try_block = curr_token == tok_try;
          if (!has_explicit_type_specifier) {
            /* Function with no explicitly specified return type.  Issue a
               remark (except in pcc mode and except for C++ constructors,
               destructors, and conversion operators). */
            if (C_dialect != C_dialect_pcc && !is_constructor_or_destructor &&
                !locator.is_conversion_name &&
                !(locator.is_error && looks_like_ctor_or_dtor(&locator))) {
              report_missing_type_specifier(&declarator_start_pos,
                                            /*is_function=*/TRUE,
                                            /*is_function_def=*/TRUE,
                                            is_main_function,
                                            !decl_specifiers_omitted);
            }  /* if */
          }  /* if */
          remove_all_local_stop_tokens();  /*lint !e774*/
          func_info.is_definition = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
          func_info.declarator_ssep = declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if USER_CONTROL_OF_STRUCT_PACKING
          /* Recored the current setting of the maximum alignment for local
             class members (an adjustment may be required for packing). */
          func_info.max_member_alignment =
                             current_max_alignment_for_class_members();
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
          if (!C_mode()) {
            /* Issue diagnostic on an incomplete-type in an exception
               specification.  (It wasn't done when the exception
               specification was scanned because definitions and declarations
               are treated differently. */
            report_exception_spec_errors(&func_info);
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
          /* GCC does not allow "void f() __attribute((...)) {}".  It
             does, however, allow "void __attribute((...)) f() {}". */
          if (attributes != NULL) {
            pos_error(ec_attributes_in_rout_defn, &locator.source_position); 
          }  /* if */
          /* GNU C doesn't allow "void f() asm("bar") {}". */
          if (asm_name != NULL) {
            pos_error(ec_asm_name_in_rout_defn, &locator.source_position);
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
          /* Do processing required for a function definition, including
             scanning the function body.  Note that the closing '}' will not
             been consumed -- that will be done by the caller. */
          (void)function_definition(&locator, local_type_ptr,
                                    &func_info, local_storage_class,
                                    has_explicit_type_specifier,
                                    &decl_modifiers, prefix_attributes,
                                    &decl_pos_block);
          done_with_func_info(func_info);
          if (is_function_try_block) {
            /* Checking for the closing brace will already have been done. */
            goto return_point;
          }  /* if */
          check_assertion(curr_token == tok_rbrace ||
                          curr_token == tok_end_of_source ||
                          microsoft_out_of_class_redecl ||
                          total_errors != 0);
          /* Right brace is expected, except for the Microsoft extension that
             allows a nondefining out-of-class member declaration. */
          final_token = microsoft_out_of_class_redecl ? tok_semicolon :
                                                        tok_rbrace;
          goto advance_past_final_token;
#if ASM_FUNCTION_ALLOWED
        } else if (declared_storage_class == (a_storage_class)sc_asm) {
          /* Not a function definition. */
          pos_error(ec_bad_asm_function_def, &pos_curr_token);
          local_storage_class = (a_storage_class)sc_unspecified;
          set_to_named_error_locator(locator);
#endif /* ASM_FUNCTION_ALLOWED */
        }  /* if */
      }  /* if */
      /* Not a function definition, must be a declaration. */
      /* After a declaration has been scanned, it is no longer possible
         that the next thing is a function definition. */
      function_definition_allowed = FALSE;
      is_static_data_member = FALSE;
      if (locator.specific_symbol != NULL &&
          locator.specific_symbol->is_class_member) {
        if (is_function) {
          /* A qualified name that identifies a function is allowed only when
             the function body is present. */
          report_member_function_redeclaration(&locator, local_type_ptr,
                                               &declarator_pos);
        } else {
          /* Assume that qualified names that are not functions refer to static
             data members. */
          is_static_data_member = TRUE;
        }  /* if */
      }  /* if */
      if (is_function && !C_mode()) {
        /* Issue diagnostic on an incomplete-type in an exception
           specification.  (It wasn't done when the exception specification
           was scanned because definitions and declarations are treated
           differently. */
        report_exception_spec_errors(&func_info);
      }  /* if */
      /* Issue diagnostics on missing type specifiers, etc. */
      if (!has_explicit_type_specifier && !is_constructor_or_destructor &&
#if GNU_EXTENSIONS_ALLOWED
          /* "typedef foo = 3;" is a GNU extension, not a use of
             implicit int. */
          !(gcc_mode && curr_token == tok_assign && 
            local_storage_class == (a_storage_class)sc_typedef) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
          !locator.is_conversion_name) {
        report_missing_type_specifier(&declarator_start_pos, is_function,
                                      /*is_function_def=*/FALSE,
                                      is_main_function,
                                      !decl_specifiers_omitted);
      }  /* if */
      if (top_declarator_type_is_function) {
        if (func_info.param_id_list != NULL) {
          /* If the function has a non-empty old-style identifier list of
             parameters, a body should have been present. */
          if (!skip_typerefs(local_type_ptr)->
                                  variant.routine.extra_info->prototyped) {
            if (microsoft_mode && C_mode()) {
              /* No diagnostic in Microsoft C mode. */
            } else {
              error(ec_param_id_list_needs_function_def);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Update xref info on param ids.  Do this even if there are no
           parameters because some source sequence entries might have been
           created (e.g., for pragmas inside the empty parameter list). */
        record_param_id_list_declarations(&func_info);
      }  /* if */
      /* Do some checking of storage classes, but not for typedefs. */
      if (local_storage_class != (a_storage_class)sc_typedef) {
        /* auto and register may not appear in a file-scope level
           declaration (3.7, constraints). */
        if (decl_scope_level == depth_innermost_namespace_scope &&
            (local_storage_class == (a_storage_class)sc_auto ||
             (
#if GNU_EXTENSIONS_ALLOWED
              /* The register keyword is allowed if there is an
                 explicit register name for a variable. */
              (asm_name == NULL || is_function || is_static_data_member) &&
#endif /* GNU_EXTENSIONS_ALLOWED */
              local_storage_class == (a_storage_class)sc_register))) {
          pos_error(ec_bad_file_scope_storage_class,
                    &decl_pos_block.storage_class_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if ((decl_modifiers.flags & DM_DLLIMPORT) &&
            local_storage_class == (a_storage_class)sc_unspecified) {
          /* __declspec(dllimport) implies extern. */
          local_storage_class = (a_storage_class)sc_extern;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
              an_error_severity  severity;
              if (C_dialect == C_dialect_cplusplus) {
                /* id_linkage doesn't expect block level statics in
                   C++ mode. */
                severity = es_error;
                local_storage_class = (a_storage_class)sc_extern;
              } else {  /* a C dialect */
                /* This is an extension to ANSI C so produce a diagnostic
                   in strict ANSI C mode. */
                severity = strict_ansi_mode ?
                              strict_ansi_error_severity : es_none;
              }  /* if */
              if (severity != es_none) {
                pos_diagnostic(severity,
                               ec_block_scope_function_must_be_extern,
                               &decl_pos_block.storage_class_pos);
              }  /* if */
            }  /* if */
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
              if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
                  local_is_old_style_param_decl) {
                /* We are inside a function body or this is an old-style
                   parameter declaration, so an unspecified storage class
                   means auto. */
                local_storage_class = (a_storage_class)sc_auto;
              } else if (is_linkage_spec_decl) {
                /* This must be part of an linkage specification declaration.
                   An "extern" storage class is implied (ARM 7.4, comment on
                   p. 118). */
                local_storage_class = (a_storage_class)sc_extern;
              }  /* if */
            }  /* if */
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
        decl_pos_block.var_init_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
      local_decl_modifiers = decl_modifiers;
      is_variable_def = FALSE;
      is_tentative_definition = FALSE;
      if (local_is_old_style_param_decl) {
        symbol_ptr = param_id->symbol;
        copy_source_position(locator.source_position,
                             symbol_ptr->decl_position);
        param_id->type = local_type_ptr;
        copy_source_position(decl_start_pos, param_id->type_pos);
        param_id->storage_class = local_storage_class;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        /* Update extra source position information in the param-id entry so
           that it can be transferred to the variable entry later. */
        param_id->specifiers_range = decl_pos_block.specifiers_range;
        param_id->declarator_range = decl_pos_block.declarator_range;
        param_id->identifier_range = decl_pos_block.identifier_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Note that the creation of the parameter variable, etc., is done
           in decl_parameter, called when the function body is scanned. */
      } else if (local_storage_class == (a_storage_class)sc_typedef) {
        /* A typedef declaration. */
        decl_typedef(&locator, local_type_ptr, (a_type_ptr)NULL,
                     prefix_attributes, &symbol_ptr, declarator_ssep,
                     &decl_pos_block);
      } else if (is_static_data_member) {
        /* A static data member definition. */
        define_static_data_member(&locator, local_storage_class,
                                  local_type_ptr, has_initializer,
                                  declarator_ssep, &symbol_ptr, &linkage,
                                  &decl_pos_block);
        var_ptr = symbol_ptr->variant.static_data_member.variable;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        /* All static data member declarations that that pass though this
           code are definitions. */
        is_variable_def = TRUE;
#if DECL_MODIFIERS_IN_USE
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode) {
          /* "selectany" is allowed only on variables that have static
              initialization and external linkage. */
          if (has_initializer && (local_decl_modifiers.flags & DM_SELECTANY)) {
            /* Postpone the checking until the initializer is scanned. */
            local_decl_modifiers.flags &= ~DM_SELECTANY;
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Copy the decl-modifiers into the variable entry. */
        update_variable_decl_modifiers(var_ptr, &local_decl_modifiers,
                                       &locator.source_position,
                                       /*is_redecl=*/TRUE);
#endif /* DECL_MODIFIERS_IN_USE */
      } else if (is_function) {
        /* A function declaration with no body. */
        if (vla_enabled) {
          if (func_info.vla_fixup_list != NULL) {
            /* Throw away VLA info created for the function prototype. */
            free_vla_fixup_list(func_info.vla_fixup_list);
            func_info.vla_fixup_list = NULL;
          }  /* if */
          /* A function declaration at function or block scope. */
          if (is_variably_modified_type(local_type_ptr)) {
            pos_error(ec_variably_modified_type_not_allowed,
                      &locator.source_position);
          }  /* if */
        }  /* if */          
        decl_routine(&locator, local_storage_class, local_type_ptr,
                     &func_info, declarator_ssep, SRK_DECLARATION,
                     &local_decl_modifiers, prefix_attributes, asm_name,
                     &symbol_ptr, &linkage, &old_type, &ext_sym,
                     &decl_pos_block);
      } else {
        /* A variable declaration. */
        a_symbol_reference_kind  srk_flags = SRK_DECLARATION;

        if (vla_enabled && !function_definition_allowed) {
          /* Local declaration. */
          if (local_storage_class == (a_storage_class)sc_extern ||
              local_storage_class == (a_storage_class)sc_static) {
            if (is_vla_type(local_type_ptr)) {
              /* An object with static storage duration cannot be a VLA. */
              pos_error(ec_vla_is_not_auto, &locator.source_position);
            } else if (local_storage_class == (a_storage_class)sc_extern &&
                       is_variably_modified_type(local_type_ptr)) {
              /* An entity with linkage cannot have a variably modified
                 type. */
              pos_error(ec_variably_modified_type_not_allowed,
                        &locator.source_position);
            }  /* if */
          }  /* if */
        }  /* if */
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
          /* A variable declaration involving an initializer is usually
             considered to be a definition.  An exception is when the
             initialization is ill-formed -- e.g., when it appears on a
             block-extern variable declaration. */
          if (decl_scope_level == depth_innermost_namespace_scope ||
              local_storage_class != (a_storage_class)sc_extern) {
            is_variable_def = TRUE;
          }  /* if */
          srk_flags |= SRK_INITIALIZATION;
        } else if (C_dialect == C_dialect_cplusplus) {
          /* Variable declaration in C++ mode with no explicit initializer. */
          if (microsoft_mode &&
              local_storage_class == (a_storage_class)sc_unspecified &&
              is_incomplete_type(local_type_ptr) &&
              is_array_type(local_type_ptr) &&
              !is_const_qualified_type(local_type_ptr)) {
            /* In Microsoft C++ mode, a non-const variable at file scope
               that is a zero-length array is treated like a C-mode tentative
               definition. */
            is_tentative_definition = TRUE;
            srk_flags |= SRK_TENTATIVE_DEF;
          } else if (local_storage_class != (a_storage_class)sc_extern) {
            /* In C++ all other variable declarations are definitions, except
               those with a storage class of extern. */
            is_variable_def = TRUE;
            /* Even without an explicit initializer this is an initializing
               declaration if the variable is nontrivially constructible
               -- i.e., if it is a class object (or array of class) and the
               class has a nontrivial default constructor (which must be a
               user-declared default constructor if the variable's type is
               const qualified -- WP 7.1.5.1 [dcl.type.cv]). */
            if (is_const_qualified_type(local_type_ptr) ?
                  type_has_user_declared_default_constructor(local_type_ptr) :
                  type_has_nontrivial_default_constructor(local_type_ptr)) {
              srk_flags |= SRK_INITIALIZATION;
            }  /* if */
          }  /* if */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode) {
          /* "selectany" is allowed only on variables that have static
              initialization and external linkage. */
          if (has_initializer &&
              (local_decl_modifiers.flags & DM_SELECTANY) &&
              depth_innermost_function_scope == NO_SCOPE_DEPTH &&
              (local_storage_class == (a_storage_class)sc_unspecified ||
               local_storage_class == (a_storage_class)sc_extern)) {
            /* Postpone the checking until the initializer is scanned. */
            local_decl_modifiers.flags &= ~DM_SELECTANY;
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        decl_variable(&locator, local_storage_class, local_type_ptr,
                      declarator_ssep, srk_flags, &local_decl_modifiers,
                      prefix_attributes, asm_name, &symbol_ptr, &linkage,
                      &old_type, &ext_sym, &decl_pos_block);
        var_ptr = symbol_ptr->variant.variable.ptr;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        local_storage_class = (a_storage_class)var_ptr->storage_class;
        if (is_variable_def) {
          /* The "declared_storage_class" field is updated only for variable
             definitions. */
          var_ptr->declared_storage_class = declared_storage_class;
        }  /* if */
        if (is_old_style_param_decl) {
          /* Error case (described above).  Mark the symbol referenced, to
             suppress subsequent "declared and not referenced" warnings. */
          mark_symbol_to_suppress_warnings(symbol_ptr);
        }  /* if */
      }  /* if */
      if (is_variable_def || is_tentative_definition) {
        /* In C++ mode, check whether a template class type needs to be
           instantiated.  If appropriate, record that a complete type is
           required in this context (both C and C++). */
        complete_type_is_needed(local_type_ptr);
      }  /* if */
      incomplete_type_error_reported = FALSE;
      if (!C_mode() && var_ptr != NULL) {
        if (is_abstract_class_type(local_type_ptr)) {
          /* Abstract class objects are prohibited (ARM 10.3). */
          report_abstract_class_error(ec_abstract_class_object_not_allowed,
                                      local_type_ptr,
                                      &locator.source_position);
        }  /* if */
      }  /* if */
      /* Set the error position to the start of the initializer (that is, to
         the "=" if there is one) or to where the initializer should be in
         case there ought to be one. */
      set_err_pos_to_curr_token();
      if (has_initializer) {
        /* If the variable had already been declared previously, old_type
           would be set. */
        a_boolean  decl_invisible_to_initializer =
                        (microsoft_bugs && has_parenthesized_initializer &&
                         old_type == NULL);
        /* Advance past the "=". */
        if (curr_token == tok_assign) (void)get_token();
        /* Now scan the initializer. */
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
            !is_old_style_param_decl) {
          /* Set the storage class of a file-scope initialized variable to
             unspecified (meaning external) or static (meaning internal).
             See 3.7.2. */
          if (decl_scope_level == depth_innermost_namespace_scope) {
            if (var_ptr->storage_class == (a_storage_class)sc_extern) {
              var_ptr->storage_class = (a_storage_class)sc_unspecified;
            }  /* if */
          }  /* if */
        }  /* if */
        if (decl_invisible_to_initializer && !symbol_ptr->is_error) {
          /* For parenthesized initializers in Microsoft bugs mode, the
             declared variable is not visible until after the initializer
             has been parsed.  To emulate this, we temporarily mark the
             symbol as invisible. */
          symbol_ptr->is_invisible = TRUE;
        }  /* if */
        /* If the symbol is a parameter, the subroutine will generate the
           error.  This is done rather than flagging the error here because
           the subroutine can scan over the initializer expression neatly. */
#if GNU_EXTENSIONS_ALLOWED
        if (gcc_mode && local_storage_class == (a_storage_class)sc_typedef) {
          typedef_initializer(symbol_ptr);
#if EXTRA_SOURCE_POSITIONS_IN_IL
          decl_pos_block.var_init_range.end = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        } else
#endif /* GNU_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          initializer(symbol_ptr, &locator.source_position, linkage,
                      has_parenthesized_initializer, is_old_style_param_decl,
                      &incomplete_type_error_reported, &decl_pos_block);
        }  /* if */
        if (decl_invisible_to_initializer && !symbol_ptr->is_error) {
          /* Mark the symbol as visible now that the initializer is
             complete. */
          symbol_ptr->is_invisible = FALSE;
        }  /* if */
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
            !is_old_style_param_decl) {
          /* All initialized variables are considered defined.  This flag
             may have already been set based on storage class and scope
             level.  Be sure to check this after the initializer is scanned,
             so that "int x = x;" can be caught. */
          mark_variable_value_set(symbol_ptr);
        }  /* if */
        /* Fetch the type of the symbol again, since it might have been
           changed if it was an incomplete array and was initialized. */
        if (var_ptr != NULL) local_type_ptr = var_ptr->type;
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode &&
            (decl_modifiers.flags &
             ~(local_decl_modifiers.flags) & DM_SELECTANY)) {
          /* Checking for the "selectany" decl-modifier was deferred. */
          if (var_ptr->init_kind == (an_init_kind)initk_static) {
            /* Flag the variable. */
            var_ptr->decl_modifiers |= DM_SELECTANY;
          } else if (var_ptr->init_kind == (an_init_kind)initk_dynamic) {
            /* The "selectany" decl-modifier cannot appear with a dynamic
               initialization. */
            pos_st_diagnostic(es_discretionary_error,
                              ec_decl_modifiers_invalid_for_this_decl,
                              &locator.source_position,
                              decl_modifier_names[(int)dmt_selectany]);
          } else {
            /* Error in initializer. */
          }  /* if */        
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
            /* Unless this variable has non-static storage duration and
               is default-initialized by a trivial default constructor (which
               is a no-op), mark it as having a value. */
            if (has_static_storage_duration(var_ptr->storage_class)) {
              /* Objects with static storage duration are zero-initialized,
                 so they always have some value. */
              mark_variable_value_set(symbol_ptr);
            } else {
              a_type_ptr  tp = skip_typerefs(var_ptr->type);
              if (is_array_type(tp)) {
                tp = underlying_array_element_type(tp);
                tp = skip_typerefs(tp);
              }  /* if */
              if (is_immediate_class_type(tp) &&
                  symbol_supplement_for_class(tp)->
                                trivial_default_constructor != NULL) {
                /* Must have been initialized by a trivial default constructor.
                   Since such constructors would do nothing even if they were
                   actually called, don't regard them as setting the value of
                   the variable. */
              } else {
                mark_variable_value_set(symbol_ptr);
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable ||
                   symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
          /* No default initialization, so do some additional checking. */
          check_for_missing_initializer(symbol_ptr, local_type_ptr);
          if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
              (!var_ptr->source_corresp.is_local_to_function ||
               var_ptr->storage_class == (a_storage_class)sc_static)) {
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
        /* Or else:  This is a tentative definition, which should be treated
           as though it were a definition. */
        mark_variable_value_set(symbol_ptr);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
#if DEBUG
      if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
        if (is_variable_def && is_static_data_member) {
          fprintf(f_debug, "decl-pos info for static data member def\n");
          db_decl_pos_info(symbol_ptr);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      copy_source_position(locator.source_position, error_position);
      if (var_ptr != NULL && !is_error_locator(locator) &&
          is_incomplete_type(local_type_ptr)) {
        /* Issue an error on a variable for which this is the defining
           declaration but whose type is incomplete.  Also, in C mode, issue
           an error on a static variable with incomplete type (6.7.2 para 3)
           or an externally linked variable with a tentative definition but an
           uncompletable type (a case like "void i;" at file scope).  And in
           C++ mode, since no object may be of void type, issue the error for
           cases like "extern void i;" even though it is not a defining
           declaration. */
        if (is_variable_def ||
            (!C_mode() && is_void_type(local_type_ptr)) ||
            (is_tentative_definition && is_void_type(local_type_ptr))) {
          if (!incomplete_type_error_reported) {
            pos_error(ec_incomplete_type_not_allowed,
                      &locator.source_position);
          }  /* if */
          var_ptr->type = error_type();
        } else if (strict_ansi_mode && is_tentative_definition && 
                   local_storage_class == (a_storage_class)sc_static) {
          /* The C standard prohibits tentative declarations with incomplete
             type and internal linkage in 6.7.2 para 3, but a reading of
             6.1.2.5 may lead to the conclusion that the prohibition does not
             exist: issue a discretionary error instead of a "hard" error. */
          if (!incomplete_type_error_reported) {
            pos_diagnostic(strict_ansi_discretionary_severity,
                           ec_incomplete_type_not_allowed,
                           &locator.source_position);
          }  /* if */
        }  /* if */
      }  /* if */
      done_with_func_info(func_info);
      remove_stop_token(tok_comma);
      need_comma_remove_stop_token = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      first_declarator = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
      /* We are done with the postfix attributes. */
      *last_prefix_attribute = NULL;
      free_attribute_list(attributes);
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Keep scanning the list of declarators. */
    } while (loop_token(tok_comma));
  }  /* if */
  if (microsoft_bugs) {
    /* In Microsoft bugs mode, the typedef is processed before member function
       bodies etc. are rescanned.  This makes e.g. the following legal:
          typedef struct {
            enum { e };
            void f() { S::e; PS p; }
          } S, *PS;
    */
    process_deferred_class_fixups_and_instantiations();
  }  /* if */
check_for_semicolon:
  /* Check for a final semicolon. */
  if (required_token_no_advance(tok_semicolon, ec_exp_semicolon)) {
advance_past_final_token:
    if (access_checks_deferred) {
      /* We are processing a declaration for which access checks were
         deferred.  Normally, any deferred checks will have already been
         performed.  In error cases, they may not have been.  If any
         remain, do them now. */
      end_deferral_of_access_checks();
      access_checks_deferred = FALSE;
    }  /* if */
    if (is_linkage_spec_decl) {
      pop_name_linkage();
      restore_name_linkage = FALSE;
    }  /* if */
    if (curr_token == final_token) {
      /* Advance past the final token of the declaration (which should be a
         ';' or '}').  However, if the current declaration is a top-level
         declaration, set a global flag to enable checking for a header
         stop. */
      if (is_top_level_declaration) next_token_is_top_level_decl_start = TRUE;
      (void)get_token();
      next_token_is_top_level_decl_start = FALSE;
    }  /* if */
  }  /* if */
return_point:
  if (access_checks_deferred) {
    /* We are processing a declaration for which access checks were
       deferred.  Normally, any deferred checks will have already been
       performed.  In error cases, they may not have been.  If any
       remain, do them now. */
    end_deferral_of_access_checks();
  }  /* if */
  if (is_linkage_spec_decl) {
    /* Unless restore_name_linkage is TRUE, pop_name_linkage will already
       have been called. */
    if (restore_name_linkage) pop_name_linkage();
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  free_attribute_list(prefix_attributes);
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do necessary remove_stop_tokens.  Even when there is no error, this
     does the remove_stop_token for tok_semicolon. */
  remove_all_local_stop_tokens();
  db_exit();
  return;
}  /* declaration */


void local_declaration(a_boolean  marked_as_gnu_extension)
/*
Scan a block-level declaration.
*/
{
  declaration(/*function_definition_allowed=*/FALSE,
              /*is_old_style_param_decl=*/FALSE,
              /*is_top_level_declaration=*/FALSE,
              marked_as_gnu_extension,
              (a_param_id_ptr)NULL, (a_source_range *)NULL);
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
  /* If the preinclude_macros option was used, scan the files that provide
     macro definitions. */
  if (is_macro_preinclude) process_macro_preinclude();
  /* Set the global flag to enable the check for a header stop. */
  next_token_is_top_level_decl_start = TRUE;
  (void)get_token();
  next_token_is_top_level_decl_start = FALSE;
  if (next_event_resumes_compilation) {
     /* We are done skipping the file prefix when making use of a PCH (i.e.,
        to skip over the part of the file that is being replaced by
        information from the PCH).  We've encountered the first token of
        the normal compilation.  Do any fixup required. */
    pch_fixup_part_2();
  }  /* if */
  if (curr_token == tok_end_of_source) {
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
    while (curr_token != tok_end_of_source) {
      /* A C99 predefined pragma in the file scope must appear between
         top-level declarations. */
      if (c99_mode) check_for_stdc_pragmas();
      declaration(/*function_definition_allowed=*/TRUE,
                  /*is_old_style_param_decl=*/FALSE,
                  /*is_top_level_declaration=*/TRUE,
                  /*marked_as_gnu_extension=*/FALSE,
                  (a_param_id_ptr)NULL, (a_source_range *)NULL);
    } /* while */
  }  /* if */
  check_assertion_str2(!header_stop_position_pending, "translation_unit:",
                       "header stop position not found");
  /* Do any end-of-translation unit pragma processing that may be required. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* First reset the point for instantiations to NULL. */
  reset_ss_list_instantiation_insert_point();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* A C99 predefined pragma in the file scope must appear between
     top-level declarations. */
  if (c99_mode) check_for_stdc_pragmas();
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
                /*is_old_style_param_decl=*/FALSE,
                /*is_top_level_declaration=*/FALSE,
                /*marked_as_gnu_extension=*/FALSE,
                (a_param_id_ptr)NULL, (a_source_range *)NULL);
  }  /* if */
}  /* scan_implicitly_included_template_definition_file */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1998 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
