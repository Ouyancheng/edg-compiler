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


/* TRUE if curr_token is the indicated token, but not if there's anything
   in the cache that should be taken first. */
#define curr_token_if_nothing_cached_is(tok, dps) \
  (curr_token == (tok) && \
   !anything_cached(&dps->prescanned_initializer_cache))


/*
While scanning a designation in an aggregate initializer, we may be in one
of three states: (1) no designators have just been scanned, (2) a designator
was just scanned, but another one is expected right after it, or (3) a
designator that completes a designation was just scanned.
*/
typedef enum a_designation_state {
  ds_no_designation,
  ds_partial_designation,
  ds_complete_designation
} a_designation_state;


/*
Data structure containing information to be passed among get_initializer
and its subroutines.  There is one such entry for each top-level (i.e.,
non-recursive) call to get_initializer.
*/
typedef struct an_aggregate_init_info *an_aggregate_init_info_ptr;
typedef struct an_aggregate_init_info {
  a_decl_parse_state
		*dps;
			/* Points to the state information for the declaration,
			   sometimes fabricated (e.g., for compound
			   literals). */
  a_boolean	static_lifetime;
			/* TRUE when the variable being initialized has
			   static lifetime. */
  a_boolean	any_uninitialized_member;
			/* Set to TRUE if any member of the aggregate remains
			   uninitialized. */
  a_boolean	any_uninitialized_const_or_ref_member;
			/* Set to TRUE if any member of the aggregate that
			   is uninitialized has const or reference type. */
  a_boolean	comma_seen;
			/* Set to TRUE when a comma was skipped while looking
			   ahead to find that there are no more initializers
			   for the current aggregate. */
  a_boolean	compound_literal;
			/* Set to TRUE when get_initializer is called to parse
			   a compound literal. */
  a_boolean	has_flexible_array_initializer;
			/* Set to TRUE when get_initializer encounters values
			   that initialize a flexible array member. */
  a_boolean     uses_designated_initializers;
                        /* Set to TRUE if any member of the aggregate was
                           initialized by a designated initializer. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		init_end_position;
			/* Source position of the end of the initializer. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_designation_state
                designation_state;
                        /* Have we just collected a partial or complete
                           designation? */
} an_aggregate_init_info;


static void initialize_init_info(an_aggregate_init_info_ptr  init_info,
                                 a_boolean                   static_lifetime,
                                 a_decl_parse_state          *dps)
/*
Initialize an entry of type an_aggregrate_init_info.
*/
{
  check_assertion(dps != NULL);
  init_info->dps = dps;
  init_info->static_lifetime = static_lifetime;
  init_info->any_uninitialized_member = FALSE;
  init_info->any_uninitialized_const_or_ref_member = FALSE;
  init_info->comma_seen = FALSE;
  init_info->compound_literal = FALSE;
  init_info->has_flexible_array_initializer = FALSE;
  init_info->uses_designated_initializers = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  init_info->init_end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  init_info->designation_state = ds_no_designation;
}  /* initialize_init_info */


/*
Data structure to represent the context of the current get_initializer
processing.  A new entry is created each time get_initializer is called,
and when they are linked together, they create a context stack.
*/
typedef struct an_aggregate_init_context *an_aggregate_init_context_ptr;
typedef struct an_aggregate_init_context {
  an_aggregate_init_context_ptr
		prev_context;
			/* Pointer to the previous init-context (i.e., when
			   get_initializer is called recursively); NULL when
			   it is a top-level call. */
  a_type_ptr	type;
			/* The type of the aggregate or subaggregate
			   associated with this context structure.  An error
			   type if we've lost our position because of
			   errors. */
  a_field_ptr	field;
			/* The field currently being initialized.  NULL if
			   the current context is not struct or if all fields
			   have been initialized. */
  a_constant_ptr
		constant_list;
			/* Pointer to the head of the list of constant entries
			   representing the initializations at the current
			   level; NULL when there are no initializations in
			   the current context. */
  a_constant_ptr
		end_of_constant_list;
			/* Pointer to the end of the list that constant_list
			   heads. */
  a_constant_ptr
		repeat;
			/* NULL when the next initializer was not preceded by a
			   designator of the form '[' <expr> '...' <expr> ']'.
			   Otherwise, a ck_init_repeat constant describing how
			   many times the initializer should be repeated. */
  a_byte_boolean
		any_dynamic_initialization;
			/* Flag that is TRUE if the current aggregate member
			   requires dynamic initialization.  This information
			   percolates back up when returning from recursive
			   calls to get_initializer. */
  a_symbol_ptr	anonymous_union_field_sym;
			/* In a case where a designated initializer names
			   a field of an anonymous union or (nonstandard)
			   anonymous struct, this points to the symbol for
			   the field; NULL otherwise.  This is non-NULL while
			   processing the implicit designator levels needed
			   to get down to the field, at which point it is
			   cleared to NULL. */
} an_aggregate_init_context;


static void initialize_init_context(
                             an_aggregate_init_context_ptr  init_context,
                             an_aggregate_init_context_ptr  prev_init_context,
                             a_type_ptr                     type)
/*
Initialize an entry of type an_aggregrate_init_context.
*/
{
  init_context->prev_context = prev_init_context;
  init_context->type = type;
  init_context->field = NULL;
  init_context->constant_list = NULL;
  init_context->end_of_constant_list = NULL;
  init_context->any_dynamic_initialization = FALSE;
  init_context->repeat = NULL;
  init_context->anonymous_union_field_sym = NULL;
  if (prev_init_context != NULL && !is_error_type(type)) {
    /* Transfer the anonymous_union_field_sym value down if set. */
    if (prev_init_context->anonymous_union_field_sym != NULL) {
      init_context->anonymous_union_field_sym =
                                  prev_init_context->anonymous_union_field_sym;
      prev_init_context->anonymous_union_field_sym = NULL;
    }  /* if */
  }  /* if */
}  /* initialize_init_context */


static void set_initialized_array_size(a_type_ptr    *type,
                                       a_targ_size_t size)
/*
Change *type to point to a new array type that is the same as the current
*type except that its number of elements is changed to "size".  This is
used to set the length of an incomplete array when its size becomes known
because it is initialized.  The original type may be shared, and therefore
a copy is made and modified.
*/
{
  a_type_ptr array_type, incomplete_type = skip_typerefs(*type);

  check_assertion(!has_unknown_specified_bound(incomplete_type));
  array_type = alloc_type((a_type_kind)tk_array);
  copy_type(incomplete_type, array_type);
  array_type->variant.array.variant.number_of_elements = size;
  if (gnu_mode && size == 0) {
    /* In GNU C and C++ mode, an empty pair of braces can
       be a valid initializer for a zero-length array. */
    array_type->variant.array.bound_is_zero = TRUE;
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
    a_type_ptr  var_elem_type;
    switch (string_con->character_kind) {
      case chk_char:
        err = !is_char_array_type(*dst_type);
        break;
      case chk_wchar_t:
        err = !is_wchar_t_array_type(*dst_type);
        break;
      case chk_char16_t:
        var_elem_type  = array_element_type(*dst_type);
        err = skip_typerefs(var_elem_type)->variant.integer.int_kind !=
                                                       targ_char16_t_int_kind;
        break;
      case chk_char32_t:
        var_elem_type  = array_element_type(*dst_type);
        err = skip_typerefs(var_elem_type)->variant.integer.int_kind !=
                                                       targ_char32_t_int_kind;
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
      set_initialized_array_size(&array_type, num_elems);
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


static a_boolean tentative_aggregate_init(
                           a_decl_parse_state          *dps,
                           an_aggregate_init_info_ptr  init_info,
                           an_aggregate_init_context   *context,
                           a_boolean                   allow_whole_string_init,
                           a_constant_ptr              *init_constant);


static a_boolean process_string_constant_initializer(
                                 a_type_ptr                     *type_ptr,
                                 a_constant_ptr                 *init_con,
                                 a_decl_parse_state             *dps,
                                 an_aggregate_init_info_ptr     init_info,
                                 an_aggregate_init_context_ptr  init_context)
/*
If the variable type (given in *type_ptr) is a character array and the
initializer is a string literal, return TRUE and update *init_con to indicate
the initialization that is specified.  The given string constant must be
acceptable as an initial value for the string type *type.  Change the
constant's type, or remove the final null from a string literal, if necessary.
If *type_ptr is an incomplete type, change it to reflect the actual size of
the string literal. (Note the extra level of indirection that allows that.)
If there is an error, issue an error and return an error constant.
During prototype instantiations, *type_ptr may also be an array whose
element type is template dependent.  If this is part of an aggregate
initialization, init_info and init_context are pointers to blocks of
information tracking this initialization; otherwise, these pointers
are NULL.  When they are NULL, dps provides the decl parse state
block, which otherwise would be found in init_info->dps.  However,
dps can also be NULL in some cases.  When this initialization is
part of an aggregate, if the initializer expression initializes the
first character of the string instead of the whole string, the
initializer is put into the cache and FALSE is returned, letting the
caller handle it.
*/
{
  a_boolean          is_string_init = FALSE;
  a_boolean          is_parenthesized = FALSE;
  a_source_position  lparen_pos;
  a_constant_ptr     con_from_expr_scan;
  a_constant_ptr     cp;
  a_boolean          err = FALSE;
  a_boolean          using_prescanned_constant = FALSE;

  if (init_info != NULL) dps = init_info->dps;
  if (is_string_type(*type_ptr) ||
      (is_array_type(*type_ptr) &&
       is_template_param_type(array_element_type(*type_ptr)))) {
    /* The entity has or might have a string type, so check for a string
       initializer. */
    if (dps != NULL &&
        (anything_cached(&dps->prescanned_initializer_cache) ||
         (is_parenthesized = (curr_token == tok_lparen)) != FALSE)) {
      /* An expression starting with a parenthesis is next.  It might be a
         parenthesized string literal, a string literal cast to "char*"
         (treated like a string literal in Microsoft mode), or something else
         altogether.  Or, we've previously cached an expression, which we
         pick up out of the cache here. */
      if (is_parenthesized) lparen_pos = pos_curr_token;
      /* Scan the expression and see whether it matches the string type. */
      if (!tentative_aggregate_init(dps, init_info, init_context,
                                    /*allow_whole_string_init=*/TRUE,
                                    &con_from_expr_scan)) {
        /* The initializer expression initializes the first character of
           the string, so leave it in the cache and return FALSE.
           However, if we're not in an aggregate initializer go on to
           check the constant value against the string type below (and
           get an error), since the expression can't initialize a member
           of the string in that case. */
        if (init_info != NULL) {
          is_string_init = FALSE;
          goto done;
        }  /* if */
      }  /* if */
      /* The expression initializes the string itself, so go on and use
         the constant below. */
      cp = con_from_expr_scan;
      using_prescanned_constant = TRUE;
      if (cp->kind == (a_constant_repr_kind)ck_string) {
        is_string_init = TRUE;
      }  /* if */
    } else {
      /* Look for a string literal token that initializes the string. */
      if (curr_token == tok_string_literal) {
        is_string_init = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (curr_token == tok_microsoft_lprefix &&
                 set_curr_token_to_microsoft_lprefix_operator_string()) {
        is_string_init = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (token_is_function_name_string_literal(curr_token)) {
        /* In some modes, keywords like __FUNCTION__ are treated as
           string literals. */
        set_curr_token_to_function_name_string(/*do_concat=*/TRUE);
        is_string_init = TRUE;
      }  /* if */
      /* If the current token is a string literal (or equivalent), check
         whether it is "standalone" or followed by an operator (a comma is not
         an operator in this context).  The latter case is not a string
         initializer case. */
      if (is_string_init && !token_ends_initializer(next_token())) {
        is_string_init = FALSE;
      }  /* if */
      if (is_string_init) {
        /* Do concatenations like "abc" __FUNCTION__. */
        (void)do_expression_level_string_literal_concatenation();
        cp = &const_for_curr_token;
        if (cp->kind != (a_constant_repr_kind)ck_string) {
          /* The constant is not a string. */
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_string_init) {
    /* The object being initialized has type array of character, and
       is being initialized with a string. */
    a_type_ptr  orig_const_type;
    /* check_string_constant_initializer may trim the string (hence modifying
       its type).  Save the original type in case it is needed in a
       diagnostic. */
    orig_const_type = cp->type;
    if (!err) {
      err = !check_string_constant_initializer(type_ptr, cp);
    }  /* if */
    if (err) {
      /* There was an error of some kind. */
      if (!is_error_type(cp->type)) {
        pos_ty2_error(ec_bad_initializer_type, &error_position,
                      orig_const_type, *type_ptr);
      }  /* if */
      *init_con = alloc_error_constant();
    } else {
      a_type_ptr  array_type = skip_typerefs(*type_ptr);
      if (!using_prescanned_constant) {
        /* Allocate the string constant. */
        *init_con = alloc_unshared_constant(cp);
      } else {
        /* The prescanned constant was already allocated. */
        *init_con = cp;
      }  /* if */
      if (init_info != NULL && !is_incomplete_type(array_type) &&
          !has_unknown_specified_bound(array_type)) {
        /* Aggregate initialization of an array: Record if any elements remain
           uninitialized. */
        if (array_type->variant.array.variant.number_of_elements
                                                 > cp->variant.string.length) {
          init_info->any_uninitialized_member = TRUE;
        }  /* if */
      }  /* if */
      if (is_parenthesized && strict_ansi_mode) {
        /* Parenthesizing a string initializer is nonstandard, but most
           compilers appear to silently accept such constructs. */
        pos_diagnostic(strict_ansi_discretionary_severity,
                       ec_nonstandard_parenthesized_string_initializer,
                       &lparen_pos);
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (!using_prescanned_constant) {
      /* Bypass the string and the right paren, if appropriate. */
      (void)get_token();
    }  /* if */
  }  /* if */
done:
  return is_string_init;
}  /* process_string_constant_initializer */


static void check_for_opening_brace(a_boolean *flag)
/*
In some cases, an extra pair of braces can surround an initializer.  This
routine checks for and ignores an extra left brace.  It sets *flag to a value
that should be passed to check_for_matching_closing_brace so the latter
can ignore the closing brace if necessary.
*/
{
  /* Note:  db_enter/db_exit CANNOT be called, because this routine
     exits with the stop tokens set different than on entry. */
  *flag = FALSE;
  if (curr_token == tok_lbrace) {
    *flag = TRUE;
    (void)get_token();
    add_stop_token(tok_rbrace);
  }  /* if */
}  /* check_for_opening_brace */


static void check_for_matching_closing_brace(a_boolean flag)
/*
Companion to check_for_opening_brace.  flag should be the flag returned by
that routine.  This routine ignores a closing brace if that is appropriate.
*/
{
  /* Note:  db_enter/db_exit CANNOT be called, because this routine
     exits with the stop tokens set different than on entry. */
  if (flag) {
    if (curr_token == tok_rbrace) {
      /* The brace is there.  Skip over it. */
      (void)get_token();
    } else {
      /* Error, the brace is not there.  Change the stop tokens set to
         just skip to a right brace (flush_tokens has some other "hard"
         tokens wired in), then record an error and flush tokens.  Always
         stop at a semicolon, however, if one is already in the stop-token
         array. */
      a_boolean  stop_at_semicolon = curr_stop_token_stack_entry->
                                         stop_tokens[(int)tok_semicolon] != 0;
      push_stop_token_stack();
      if (stop_at_semicolon) add_stop_token(tok_semicolon);
      (void)required_token(tok_rbrace, ec_exp_rbrace);
      if (stop_at_semicolon) remove_stop_token(tok_semicolon);
      pop_stop_token_stack();
    }  /* if */
    remove_stop_token(tok_rbrace);
  }  /* if */
}  /* check_for_matching_closing_brace */


static void copy_ctor_default_args_to_dynamic_init(a_dynamic_init_ptr  dip)
/*
dip points to a dik_constructor dynamic init entry.  Make a copy of the
default arguments from the routine type of the constructor, updating
the dynamic init entry.
*/
{
  a_routine_ptr           rp;
  a_param_type_ptr        ptp;
  an_object_lifetime_ptr  expr_temp_lifetime;

  rp = dip->variant.constructor.ptr;
  ptp = skip_typerefs(rp->type)->variant.routine.extra_info->param_type_list;
  if (dip->variant.constructor.is_copy_constructor_with_implied_source) {
    check_assertion(ptp != NULL);
    ptp = ptp->next;
  }  /* if */
  if (ptp != NULL) {
    if (!long_lifetime_temps) {
      /* Push an object lifetime, in case the expression requires
         generating a temporary. */
      push_object_lifetime((an_il_entry_kind)iek_none, (char *)NULL,
                           (an_object_lifetime_kind)olk_expr_temporary);
      expr_temp_lifetime = curr_object_lifetime;
    }  /* if */
    /* If there is a default argument value, or several, use them. */
    /* Copy the default-arg list. */
    dip->variant.constructor.args =
      copy_default_arg_expr_list(rp, ptp,
                                 /*inside_conditional_expression=*/FALSE,
                                 /*potentially_evaluated=*/TRUE);
    if (!long_lifetime_temps) {
      /* Pop the object lifetime for the temp, binding the lifetime and
         dynamic init entry if appropriate. */
      if (!is_useless_object_lifetime(expr_temp_lifetime)) {
        bind_object_lifetime(expr_temp_lifetime,
                             (an_il_entry_kind)iek_dynamic_init,
                             (char *)dip);
      }  /* if */
      (void)pop_object_lifetime();
    }  /* if */
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


static void add_dtor_for_partially_constructed_aggregate(
                                                 a_routine_ptr       dtor_rp,
                                                 a_dynamic_init_ptr  dip)
/*
This routine should really be called, "add destructor to dynamic init for
member of partially constructed aggregate".  dtor_rp is the destructor
routine.  dip is a dynamic-init entry created for the initialization of a
field or array element.
*/
{
  if (dip->destructor == NULL && dtor_rp != NULL) {
    dip->destructor = dtor_rp;
    dip->destruction_is_for_partially_constructed_aggregate = TRUE;
    /* Since the destructor has been added to a dynamic init entry that
       will not be "on top" when gen_dynamic_initialization is called,
       record the destruction, if needed, with the appropriate
       object-lifetime entry.  Note -- static_lifetime is FALSE because
       (for function-local static variables) even though the underlying
       entity has static lifetime, the lifetime of the destruction is as
       though it were automatic. */
    record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                       /*block_lifetime=*/FALSE);
  }  /* if */
}  /* add_dtor_for_partially_constructed_aggregate */


static a_boolean any_constructible_fields_remaining(
                                  an_aggregate_init_context_ptr  init_context,
                                  an_aggregate_init_info_ptr     init_info)
/*
Examine each field in the linked list headed by init_context->field, returning
TRUE if any is of class-struct-union type with a constructor (or array
thereof).  init_info is a pointer to a block of information tracking this
initialization.
*/
{
  a_field_ptr                    fp = init_context->field;
  a_type_ptr                     tp;
  a_boolean                      ctor_found = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  /* Make a pass over the remaining fields. */
  for (; fp != NULL; fp = fp->next) {
    tp = fp->type;
    if (is_any_reference_type(tp)) {
      /* Field is a reference -- an error should be put out. */
      init_info->any_uninitialized_const_or_ref_member = TRUE;
    } else if (C_mode() && is_const_qualified_type(tp)) {
      /* Const field in C mode -- a warning will be issued. */
      init_info->any_uninitialized_const_or_ref_member = TRUE;
      break;
    } else {
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      if (is_real_class_type(tp)) {
        /* Field is a class type (or an array of class-type elements). */
        tp = skip_typerefs(tp);
        cssp = symbol_supplement_for_class(tp);
        if (C_mode() && tp->variant.class_struct_union.any_const_member) {
          /* In C mode, the field's type is a struct with a const field. */
          init_info->any_uninitialized_const_or_ref_member = TRUE;
          break;
        } else if (!has_trivial_default_constructor(cssp) ||
                   (exceptions_enabled && has_nontrivial_destructor(cssp))) {
          ctor_found = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return ctor_found;
}  /* any_constructible_fields_remaining */


static a_boolean init_remaining_array_elements(
                                 a_type_ptr                     array_type,
                                 a_targ_size_t                  curr_element,
                                 an_aggregate_init_info_ptr     init_info,
                                 an_aggregate_init_context_ptr  init_context)
/*
This routine is called from get_initializer to deal with the case where an
array whose elements require constructor initialization (and/or destruction
by calling a destructor) has been only partially initialized.  The remaining
elements of the array receive initialization by the default constructor.
array_type is a pointer to the type entry for the array object.  curr_element
identifies the next element to be initialized.  *con_list is a list of
constant entries that represents the initialization of the array;
*end_of_con_list points to the terminal entry on the list.  init_info is
a pointer to a block of information tracking this initialization.  TRUE
is returned if the remaining array elements are indeed completely initialized
(including any zeroing required by the value-initialization rules).  This
routine is called in C++ mode only.
*/
{
  a_type_ptr                     element_type;
  a_targ_size_t                  number_of_uninitialized_elements;
  a_constant_ptr                 cp, repeat_con;
  a_routine_ptr                  ctor_rp;
  a_class_symbol_supplement_ptr  cssp;
  a_dynamic_init_ptr             dip;
  a_boolean                      init_done = FALSE;

  db_enter(4, "init_remaining_array_elements");

  array_type = skip_typerefs(array_type);
  if (array_type->variant.array.is_template_dependent_size_array) {
    /* The array size may be template dependent. */
    check_assertion(is_template_dependent_context());
    number_of_uninitialized_elements = 0;
  } else {
    check_assertion(!has_unknown_specified_bound(array_type));
    number_of_uninitialized_elements =
          array_type->variant.array.variant.number_of_elements - curr_element;
  }  /* if */
  if (number_of_uninitialized_elements > 0) {
    a_boolean  trivial_ctor = TRUE;
    /* There are one or more uninitialized elements. */
    element_type = f_skip_typerefs(array_element_type(array_type));
    if (is_array_type(element_type)) {
      a_type_ptr  tp = element_type;
      element_type = f_skip_typerefs(underlying_array_element_type(tp));
      number_of_uninitialized_elements *=
                                    array_element_count(tp, element_type);
    }  /* if */
    if (is_real_class_type(element_type)) {
      /* It is an array of class objects. */
      cssp = symbol_supplement_for_class(element_type);
      trivial_ctor = has_trivial_default_constructor(cssp);
    } else {
      cssp = NULL;
    }  /* if */
    if (!any_constructible_fields_remaining(init_context, init_info) &&
        (cssp == NULL ||
         (trivial_ctor &&
          (!exceptions_enabled || cssp->has_trivial_destructor)))) {
      if (cssp != NULL && trivial_ctor) {
        /* An array element of class type with no constructor but with a ref
           member will end up uninitialized; set the flag in C mode for a
           const member -- a warning will be issued. */
        if (C_mode() ?
              element_type->variant.class_struct_union.any_const_member :
              cssp->any_ref_member) {
          init_info->any_uninitialized_const_or_ref_member = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Initialization is required. */
      if (cssp == NULL) {
        /* Non-class (or nonreal class) element type. */
        ctor_rp = NULL;
      } else {
        /* Get the default constructor.  Note that it is an error if it
           is missing. */
        ctor_rp = select_default_constructor(element_type, &pos_curr_token,
                                             element_type,
                                             (a_boolean *)NULL);
      }  /* if */
      if (ctor_rp == NULL) {
        /* Trivial default constructor, non-class type, or error. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
        init_done = TRUE;
      } else  {
        /* For a non-trivial constructor, create a dik_constructor
           dynamic init entry. */
        dip = alloc_ctor_dynamic_init(ctor_rp, /*implied_source=*/FALSE);
        /* If the default constructor is generated and some component of the
           class requires zeroing, initialization is not really done because
           the value-initialization rules require that the zeroing occurs. */
        init_done = !(ctor_rp->compiler_generated &&
                      element_type
                        ->variant.class_struct_union.has_zero_init_component);
      }  /* if */
      if (cssp != NULL) {
        if (exceptions_enabled && has_nontrivial_destructor(cssp)) {
          /* If appropriate, add a destructor pointer to the dynamic init
             entry.  This is for the case in which an exception is thrown by
             the constructor before the entire array has been initialized. */
          a_routine_ptr  dtor_rp = cssp->destructor->variant.routine.ptr;
          add_dtor_for_partially_constructed_aggregate(dtor_rp, dip);
        }  /* if */
      }  /* if */
      /* Now create the constant entry that will point to the new dynamic
         init entry. */
      cp = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      cp->variant.dynamic_init = dip;
      cp->type = element_type;
      if (number_of_uninitialized_elements > 1) {
        /* When there is more than one uninitialized element remaining
           in the array, we put out an init_repeat constant on top of
           the dynamic init constant. */
        repeat_con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
        repeat_con->variant.init_repeat.count = 
                                             number_of_uninitialized_elements;
        repeat_con->variant.init_repeat.constant = cp;
        cp = repeat_con;
      }  /* if */
      /* Add the constant entry to the list of constants. */
      if (init_context->constant_list == NULL) {
        init_context->constant_list = cp;
      } else {
        init_context->end_of_constant_list->next = cp;
      }  /* if */
      init_context->end_of_constant_list = cp;
      init_context->any_dynamic_initialization = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
  return init_done;
}  /* init_remaining_array_elements */


static a_boolean init_remaining_fields(
                                 an_aggregate_init_info_ptr     init_info,
                                 an_aggregate_init_context_ptr  init_context)
/*
This routine is called from get_initializer to deal with the case where a
class object is only partially initialized.  It checks whether any of the
uninitialized fields is itself of class (or array of class) type and if so
does the appropriate value initialization (i.e., looks for and calls the
default constructor).  *curr_field is the first of the uninitialized fields.
*con_list is a list of constant entries that represents the initialization of
the array; *end_of_con_list points to the terminal entry on the list.
init_info is a pointer to a block of information tracking this initialization.
TRUE is returned if all of the remaining fields are indeed completely
initialized (including the zeroing required by value-initialization rules).
This routine is called in C++ mode only.
*/
{
  a_type_ptr                     tp, array_type;
  a_constant_ptr                 cp, repeat_con;
  a_dynamic_init_ptr             dip;
  a_class_symbol_supplement_ptr  cssp;
  a_routine_ptr                  ctor_rp;
  a_boolean                      init_done = FALSE,
                                 incomplete_value_init = FALSE;
  a_boolean                      found_constructible_field = FALSE;

  db_enter(4, "init_remaining_fields");
  /* Before looping through the fields, do a preliminary pass to determine
     whether there is anything requiring default initialization. */
  if (any_constructible_fields_remaining(init_context, init_info)) {
    /* Yes, so make another pass over the remaining fields. */
    while (init_context->field != NULL) {
      tp = skip_typerefs(init_context->field->type);
      if (is_array_type(tp)) {
        array_type = tp;
        tp = f_skip_typerefs(underlying_array_element_type(tp));
      } else {
        array_type = NULL;
      }  /* if */
      if (is_immediate_class_type(tp) &&
          !is_template_param_or_nonreal_class_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
      } else {
        cssp = NULL;
      }  /* if */
      if (cssp == NULL) {
        /* Non-class (or nonreal class) field. */
        /* Zero-initialize the field and then continue looping.  There is
           a field later in the list for which the default constructor has to
           be called, but we can't leave this field uninitialized. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
      } else {
        /* Get the default constructor.  Note that it is an error if it
           is missing. */
        ctor_rp = select_default_constructor(tp, &pos_curr_token, tp,
                                             (a_boolean *)NULL);
        if (ctor_rp == NULL) {
          /* Trivial default constructor, or error of some sort. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
        } else  {
          /* Default initialization is required -- this must be the field found
             by the call to any_constructible_fields_remaining. */
          found_constructible_field = TRUE;
          dip = alloc_ctor_dynamic_init(ctor_rp, /*implied_source=*/FALSE);
          /* If the default constructor for the field was not user-written,
             a part of the field might need to be zeroed according to the
             rules for value-initialization. */
          if (!incomplete_value_init && ctor_rp->compiler_generated &&
              tp->variant.class_struct_union.has_zero_init_component) {
            incomplete_value_init = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (cssp != NULL) {
        if (exceptions_enabled && has_nontrivial_destructor(cssp)) {
          /* If appropriate, add a destructor pointer to the dynamic init
             entry.  This is for the case in which an exception is thrown by
             the constructor before the entire array has been initialized. */
          a_routine_ptr  dtor_rp = cssp->destructor->variant.routine.ptr;
          add_dtor_for_partially_constructed_aggregate(dtor_rp, dip);
          found_constructible_field = TRUE;
        }  /* if */
      }  /* if */
      /* Create the constant entry that will point to the new dynamic
         init entry. */
      cp = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      cp->variant.dynamic_init = dip;
      cp->type = tp;
      /* If the field is an array, each of its elements has to be
         constructed. */
      if (array_type != NULL) {
        /* Build the looping constant entry. */
        repeat_con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
        repeat_con->variant.init_repeat.count = 
                                      array_element_count(array_type, tp);
        repeat_con->variant.init_repeat.constant = cp;
        /* Put a ck_aggregate constant on top of the ck_init_repeat.  The
           resulting IL will look like this: ck_aggregate (representing
           the array as a whole) on top of ck_init_repeat on top of
           ck_dynamic_init on top of a dynamic-init entry (either dik_none,
           dik_zero, or dik_constructor). */
        cp = alloc_constant((a_constant_repr_kind)ck_aggregate);
        cp->type = array_type;
        cp->variant.aggregate.first_constant = repeat_con;
        cp->variant.aggregate.last_constant = repeat_con;
      }  /* if */
      /* Add the constant entry to the list of constants. */
      if (init_context->constant_list == NULL) {
        init_context->constant_list = cp;
      } else {
        init_context->end_of_constant_list->next = cp;
      }  /* if */
      init_context->end_of_constant_list = cp;
      /* Continue looping only if there are other constructible fields that
         remain uninitialized. */
      if (found_constructible_field) {
        if (any_constructible_fields_remaining(init_context, init_info)) {
          /* There is another field requiring default initialization, so
             keep looping. */
          found_constructible_field = FALSE;
        } else {
          /* No additional processing is required. */
          break;
        }  /* if */
      }  /* if */
      init_context->field =
                          next_initializable_field(init_context->field->next);
    }  /* while */
    init_done = TRUE;
    init_context->any_dynamic_initialization = TRUE;
  }  /* if */
  db_exit();
  return init_done && !incomplete_value_init;
}  /* init_remaining_fields */


static a_constant_ptr scan_initializer_of_simple_object(
                        a_decl_parse_state            *dps,
                        an_aggregate_init_info_ptr    init_info,
                        an_aggregate_init_context_ptr init_context,
                        a_boolean                     nonconst_allowed,
                        a_boolean                     static_lifetime,
                        a_boolean                     force_object_lifetime,
                        a_boolean                     suppress_object_lifetime,
                        a_boolean                     is_copy_initialization,
                        a_type_ptr                    *p_type,
                        a_dynamic_init_ptr            *dip_ptr)
/*
Scan a nonaggregate initializer (i.e., not a brace-enclosed expression
list).  *dps describes the declaration directly associated with this
initializer (if any; dps is NULL for ctor-initializers, for example).
If the initializer is an element in an aggregate initialization,
init_info and init_context give context information on the aggregate
initialization; otherwise, they are NULL.  If nonconst_allowed is TRUE
(always the case in C++, sometimes otherwise) a nonconstant expression
is allowed; if not, a constant is required.  If static_lifetime is
TRUE, the underlying entity has static storage duration.
force_object_lifetime is TRUE only in C++ mode and only when this
function is called in scanning an entry in a ctor initializer list; it
is passed on to scan_initializer_expression to force creation of an
object lifetime for expression temporaries even if long_lifetime_temps
is TRUE.  Conversely, suppress_object_lifetime is TRUE when no object
lifetime entry should be generated (used when parsing compound
literals in C++ mode).  If is_copy_initialization is TRUE, this is
copy-initialization ("="-form); otherwise, it's direct-initialization
("()"-form).  *p_type is the data type of the object being
initialized.  It may be updated if it is an incomplete string type and
the initializer is a string constant.  dip_ptr is a pointer to a
dynamic init pointer; if the latter is NULL, a dynamic init entry may
be allocated and returned, but if *dip_ptr is non-NULL, build the
initialization information into the object it points to.  A (possibly
NULL) constant pointer is returned; iff *dip_ptr is updated, NULL is
returned.  Thus, if nonconst_allowed is FALSE, return a pointer to a
constant entry.  Otherwise, if the initializer is a constant value
then return a pointer to a constant only if *dip_ptr is NULL.  If the
initializer is nonconstant or *dip_ptr is non-NULL, return a NULL
constant pointer and build *dip_ptr to represent the initialization.
*/
{
  an_expr_node_ptr expression;
  a_boolean        is_constant;
  a_constant       constant, *cp = NULL;

  if (process_string_constant_initializer(p_type, &cp, dps,
                                          init_info, init_context)) {
    /* The object being initialized has type pointer to (narrow or wide)
       characters, and is being initialized with a string. */
    is_constant = TRUE;
  } else if (nonconst_allowed) {
    /* Scan a potentially non-constant initializer expression.  The result
       of the scan is a constant if the expression is constant, and an
       expression node if not. */
    scan_initializer_expression(
                         *p_type, dps, static_lifetime, force_object_lifetime,
                         suppress_object_lifetime, is_copy_initialization,
                         &is_constant, &expression, &constant);
  } else {
    /* Non-constant is not allowed. */
    scan_constant_initializer_expression(*p_type, dps, &constant);
    is_constant = TRUE;
  }  /* if */
  /* See if the scanned expression was constant or not. */
  if (is_constant) {
    /* Constant. */
    if (cp == NULL) cp = alloc_unshared_constant(&constant);
    if (*dip_ptr == NULL) {
      /* If the caller has not preallocated a dynamic init entry, it signals
         that a constant should be returned. */
    } else {         
      /* Even though this is a constant, the initialization is dynamic. */
      clear_dynamic_init(*dip_ptr, (a_dynamic_init_kind)dik_constant);
      (*dip_ptr)->variant.constant = cp;
      /* Since the constant is being returned in the dynamic init entry,
         avoid confusion and set cp to NULL. */
      cp = NULL;
    }  /* if */
  } else {
    /* Non-constant. */
    /* Set the dynamic init entry to represent non-constant assignment
       initialization. */
    if (*dip_ptr == NULL) {
      /* A new one needs to be allocated. */
      *dip_ptr = alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
    } else {
      /* Use the one whose address is pointed to by dip_ptr. */
      clear_dynamic_init(*dip_ptr, (a_dynamic_init_kind)dik_expression);
    }  /* if */
    (*dip_ptr)->variant.expression = expression;
  }  /* if */
  return cp;
}  /* scan_initializer_of_simple_object */


static a_boolean designator_coming(
                                  an_aggregate_init_info_ptr init_info,
                                  a_boolean                  *array_designator)
/*
A C99 language feature allows aggregate initializers to be preceded by
a "designation" that indicates which field or element is initialized. The
syntax looks like:
   X x = { .a = 1, .b.c = 2, { [3] = { 0 }, [4][0] = { 4 } }, .d[2].d = 0 };
Each '.<identifier>' and '[<const-expr>]' is called a "designator".
Furthermore, some compilers also allow the following syntax for field
designators:
   X x = { a: 1 };
This function returns TRUE if a designator is the next thing in the stream of
tokens (assuming designators are enabled).  If the designator is an array
designator, *array_designator is returned TRUE.  array_designator can be
NULL if the return value is not needed.  If init_info is non-NULL, it gives
the aggregate initialization state, which is used to see if there are
any cached expressions, in which case a designator is not "next" because
the cached expression comes first.
*/
{
  a_boolean result = FALSE, local_array_designator = FALSE;

  if (designators_allowed &&
      (init_info == NULL ||
       !anything_cached(&init_info->dps->prescanned_initializer_cache))) {
    if (curr_token == tok_period) {
      result = TRUE;
    } else if (curr_token == tok_lbracket) {
      result = TRUE;
      local_array_designator = TRUE;
    } else if (extended_designators_allowed) {
      /* Designators of the form "name :" need lookahead to
         recognize the colon: */
      if (curr_token == tok_identifier && next_token() == tok_colon) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (array_designator != NULL) *array_designator = local_array_designator;
  return result;
}  /* designator_coming */ 


static a_boolean designator_is_next(an_aggregate_init_info_ptr init_info,
                                    an_aggregate_init_context  *context)
/*
Return TRUE if the next constant to create is a ck_designator.  This may be
a designator appearing explicitly in the source, or part of an implicit field
designator chain resulting from a designator construct referring to an
anonymous union member.
*/
{
  return designator_coming(init_info, (a_boolean *)NULL) ||
         context->anonymous_union_field_sym != NULL;
}  /* designator_is_next */


static a_boolean tentative_aggregate_init(
                           a_decl_parse_state          *dps,
                           an_aggregate_init_info_ptr  init_info,
                           an_aggregate_init_context   *context,
                           a_boolean                   allow_whole_string_init,
                           a_constant_ptr              *init_constant)
/*
We are parsing an aggregate initializer, with the state of processing
described by init_info and context.  The next construct is expected to
be an expression that may (or may not, hence "tentative") initialize a
complete aggregate (array or class) subobject.  If the expression
initializes the whole aggregate (for the meaning of that, see
process_whole_object_init), the initializer is returned through
*init_constant, and TRUE is returned.  In all other cases, the
initializer expression is cached for scanning at some other level, and
FALSE is returned.  If the cache already contains something on entry,
its first expression is consumed instead of scanning another one from
source.  If allow_whole_string_init is FALSE, the initialization of an
array by a string literal is not considered an aggregate
initialization by this routine, and FALSE is returned for that case.
In some cases involving initializing an array of characters with
a string, init_info and context are NULL, and dps provides the
decl parse state, which otherwise is gotten from init_info->dps.
*/
{
  a_boolean                      is_whole_object_init = TRUE;
  a_boolean                      is_constant = FALSE;
  a_boolean                      whole_string_init = FALSE;
  a_constant                     constant;
  a_class_symbol_supplement_ptr  cssp = NULL;
  a_dynamic_init_ptr             dip;

  if (init_info != NULL) dps = init_info->dps;
  check_assertion(dps != NULL);
  if (is_class_struct_union_type(context->type)) {
    cssp = symbol_supplement_for_class(context->type);
    check_assertion_str(c99_mode || gcc_mode ||
                        cssp->has_copy_constructor ||
                        cssp->construction_by_bitwise_copy_allowed ||
                        skip_typerefs(context->type)->
                                variant.class_struct_union.is_nonreal_class,
                        "tentative_aggregate_init: missing copy constructor");
  }  /* if */
  /* See whether we can initialize the entire aggregate with the next
     expression. */
  if (!scan_aggregate_initializer_expression(
                                  context->type,
                                  ((init_info != NULL) ?
                                       init_info->static_lifetime : FALSE),
                                  ((init_info != NULL) ?
                                       init_info->compound_literal : FALSE),
                                  dps,
                                  (allow_whole_string_init ?
                                       NULL :
                                       &whole_string_init),
                                  &is_constant, &dip, &constant)) {
    /* The initializer expression doesn't match the aggregate or any of
       its first members.  An error has been issued already. */
  } else if (dps->prescanned_initializer_levels_down > 0) {
    /* The expression next up does not apply at this level.  It is supposed
       to initialize the first member of the aggregate (or its first member,
       etc.).  Leave it in the cache and pick it up at another level. */
    is_whole_object_init = FALSE;
  } else if (whole_string_init) {
    /* The caller asked us not to treat the initialization of a whole
       string as a whole-object initialization, so leave the initializing
       string in the cache to be picked up elsewhere. */
    is_whole_object_init = FALSE;
  } else {
    /* Whole-object initialization applies at the current level. */
    if (is_constant) {
      /* A constant initializer. */
      *init_constant = alloc_unshared_constant(&constant);
      if (init_info != NULL &&
          constant.uses_designated_initializers) {
        /* Propagate the use of designated initializers upwards. */
        init_info->uses_designated_initializers = TRUE;
      }  /* if */
    } else {
      /* A dynamic initialization. */
      check_assertion(dip != NULL);
      *init_constant = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      (*init_constant)->variant.dynamic_init = dip;
      if (init_info != NULL &&
          dip->kind == (a_dynamic_init_kind)dik_constant &&
          dip->variant.constant->uses_designated_initializers) {
        /* Propagate the use of designated initializers upwards. */
        init_info->uses_designated_initializers = TRUE;
      }  /* if */
    }  /* if */
    if (context != NULL) {
      (*init_constant)->type = rvalue_type(context->type);
      if (!is_constant) {
        /* We should only get here for class types (as opposed to array types).
           If we emulate GNU C++ whole-object initialization using nonconstant
           compound literals, some array cases may get here too, and the
           following should be revised. */
        check_assertion(cssp != NULL);
        context->any_dynamic_initialization = TRUE;
        if (exceptions_enabled) {
          if (has_nontrivial_destructor(cssp)) {
            /* If appropriate, add a destructor pointer to the dynamic
               init entry. This is for the case in which an exception is
               thrown by the constructor before the entire array has been
               initialized. */
            a_routine_ptr  dtor_rp = cssp->destructor->variant.routine.ptr;
            add_dtor_for_partially_constructed_aggregate(dtor_rp, dip);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_whole_object_init;
}  /* tentative_aggregate_init */


static a_boolean process_whole_object_init(
                                   an_aggregate_init_info_ptr  init_info,
                                   an_aggregate_init_context   *context,
                                   a_constant_ptr              *init_constant)
/*
In C, a single value of an aggregate initializer cannot have an aggregate class
type.  However, in C++ this "whole object initialization" may be valid:
   struct S { int a, b; };        // aggregate class
   S s1 = { 1, 2 };               // okay
   S s2 = s1;                     // okay
   S sa1[] = { 1, 2, 1, 2 };      // equivalent to {{1,2},{1,2}}
   S sa2[] = { s1, s2 };          // okay
   S sa3[] = { s1, 1, 2 };        // okay
This function returns TRUE if such a case applies and, if so, sets
*init_constant to point to the IL structure for the scanned initializer.
Information about the state of the processing of the complete initializer and
the current subaggregate initializer is kept in *init_info and *context
respectively (and updated as needed by this function).
This is the only case in aggregate initialization where a value of the
initializer must be scanned and analyzed before we can determine which element
of the destination type it maps onto.  If this scanned value can be converted
to the aggregate class type, whole object initialization applies and
implicit conversions to a type that allows it to initialize a member of the
aggregate are not considered.  For example:
   struct A {
     int i;
     operator int() { return i + 20; }
   } a;
   struct B {
       A a1, a2;
       int z;
   } b = { 4, a, a }; // b.a1.i == 4, b.a2.i == 0, b.z == 20 after this
If a value was scanned but whole object initialization did not apply,
the initializer expression is placed in the
init_info->dps->prescanned_initializer_cache for use later on, and
FALSE is returned.

In C99 and GNU C modes, the processing is similar to that in C++.
*/
{
  a_boolean is_whole_object_init;
  a_boolean top_level = (context->prev_context == NULL);

  check_assertion(init_info != NULL && init_info->dps != NULL);
  if ((!C_mode() || c99_mode || gcc_mode) &&
      (is_class_struct_union_type(context->type) ||
       (gnu_mode && is_array_type(context->type) &&
        !skip_typerefs(context->type)->variant.array.bound_is_zero)) &&
      !curr_token_if_nothing_cached_is(tok_lbrace, init_info->dps) &&
      !top_level && !designator_is_next(init_info, context)) {
    is_whole_object_init = tentative_aggregate_init(
                                             (a_decl_parse_state *)NULL,
                                             init_info, context,
                                             /*allow_whole_string_init=*/FALSE,
                                             init_constant);
  } else {
    is_whole_object_init = FALSE;
  }  /* if */
  return is_whole_object_init;
}  /* process_whole_object_init */


static void handle_missing_brace(an_aggregate_init_info_ptr init_info,
                                 an_aggregate_init_context  *context)
/*
In standard C and C++, the top-level initializer for a class, struct, union, or
array must be surrounded by braces (except for whole object initialization of
classes, handled elsewhere).  However, pcc allows it -- e.g., "int a[2] = 1;"
is equivalent to "int a[2] = { 1 };".  We allow pcc behavior as an extension
in C mode, but it's an error in C++ mode. The type of the object being
initialized is context->type; if an error occurs this will be set to an error
type.
*/
{
  if (C_dialect != C_dialect_pcc) {
    an_error_severity  severity;
    if (C_dialect == C_dialect_cplusplus) {
      severity = es_error;
    } else if (strict_ansi_mode) {
      severity = strict_ansi_error_severity;
    } else {
      /* Issue a warning in non-ANSI C mode. */
      severity = es_warning;
    }  /* if */
    diagnostic(severity, ec_missing_initializer_list);
    /* If we're issuing an error, avoid error recovery problems by setting
       the aggregate type to an error type.  We don't do this if the
       initializer has been scanned already since that would make it look
       like there weren't any initializers at all. */
    if (severity == es_error &&
        !(init_info != NULL && init_info->dps != NULL &&
          anything_cached(&init_info->dps->prescanned_initializer_cache))) {
      context->type = error_type();
    }  /* if */
  }  /* if */
}  /* handle_missing_brace */


static void start_aggregate_init_scan_loop(
                          an_aggregate_init_context  *context,
                          a_type_kind                *kind,
                          a_type_ptr                 *member_type,
                          a_boolean                  *any_more_members,
                          a_boolean                  *is_flexible_array)
/*
Initialize the state for the loop that will scan an aggregate initializer list.
State information about this initializer scanning is maintained in *context.
The type of the aggregate or subaggregate whose initializer is about to be
scanned is context->type.  context->field member will be made to point to the
first initializable field (if any) if context->type is a class type.  *kind
will be set to the kind of entity being initialized, which is the type kind for
context->type, except that nonreal class types produce tk_template_param.
*member_type will be set to the type of the next member to be initialized (or
an error type if context->type is an error_type).  If there are any members to
initialize, *any_more_members will be set to TRUE.  If context->type is an
array type of zero size ("[]" or "[0]"), *is_flexible_array will be set to
TRUE.
*/
{
  a_type_ptr  type = skip_typerefs(context->type);

  if (is_template_param_or_nonreal_class_type(context->type)) {
    /* Treat both template parameters and nonreal types as "types whose
       inner structure is unknown". */
    *kind = (a_type_kind)tk_template_param;
  } else {
    *kind = skip_typerefs(context->type)->kind;
  }  /* if */
  *any_more_members = TRUE;  /* Assume. */
  *is_flexible_array = FALSE;
  switch (*kind) {
    case tk_error:
      /* Unknown member type (due to error). */
      *member_type = error_type();
      break;
    case tk_template_param:
      /* Unknown member type (due to template parameterization). */
      *member_type = type_of_unknown_templ_param_nontype;
      break;
    case tk_array:
      /* Array.  Start with first element. */
      if (type->size == 0) {
        if (type->incomplete) {
          /* A flexible array member (declared with a[]). */
          *is_flexible_array = TRUE;
        } else {
          /* A zero-length array (a GNU extension).  This is different from a
             flexible array, in that no initializers are allowed for it.
             (Note, this could include a type like "int[3][0]".) */
          check_assertion(scope_stack_top().in_prototype_instantiation ||
                          num_array_elements(type) == 0);
          *any_more_members = FALSE;
        }  /* if */
      }  /* if */
      *member_type = type->variant.array.element_type;
      if (is_array_type(*member_type) &&
          skip_typerefs(*member_type)->variant.array.bound_is_zero) {
        /* Some modes allow zero-length arrays.  If the member type contains
           such an array, do not attempt to initialize it.  E.g.:
              int a[][0] = { 0 };  // Excess initializer.
        */
        *any_more_members = FALSE;
      }  /* if */
      /* Note that arrays of incomplete struct/union types (an extension)
         do not make it to here (they're caught as an error at the top
         level in the routine "initializer" and replaced by an error type),
         so we don't have to check for them here. */
      break;
    case tk_struct:
    case tk_class:
    case tk_union:
      /* Class/struct/union.  Start with first field. */
      context->field = type->variant.class_struct_union.field_list;
      /* Skip past an unnamed field. */
      context->field = next_initializable_field(context->field);
      *any_more_members = (context->field != NULL);
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      /* GNU vector: Similar to an array.  Start with first element. */
      check_assertion(gnu_mode);
      *member_type = type->variant.vector.element_type;
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    default:
      unexpected_condition();
  }  /* switch */
}  /* start_aggregate_init_scan_loop */


static a_boolean any_initializers(
                                  an_aggregate_init_info_ptr    init_info,
                                  an_aggregate_init_context_ptr context,
                                  a_boolean                     brace_flag,
                                  a_boolean                     any_members,
                                  a_boolean                     *nothing_taken)
/*
Returns whether any initializers are available for the initialization
of the aggregate tracked by init_info and context.  If an introductory
brace was scanned, brace_flag is TRUE.  If the aggregate has any
initializable members, any_members is TRUE.  This function detects the
case where an empty class is being initialized and sets *nothing_taken
accordingly to indicate if no initializer was consumed.
*/
{
  a_boolean result = TRUE;

  if (anything_cached(&init_info->dps->prescanned_initializer_cache)) {
    /* There's at least one pre-scanned expression, so there are more
       initializers. */
    result = TRUE;
  } else if (brace_flag) {
    /* The list for the aggregate at this level is enclosed in { }. */
    if (curr_token == tok_rbrace) {
      /* Empty initializer list --  "{ }".  An error in C, okay in C++ and
         in GNU C mode. */
      if (C_mode() && !gcc_mode) error(ec_exp_primary_expr);
      result = FALSE;
    }  /* if */
  } else {
    /* The list for the aggregate is not enclosed in braces. */
    a_boolean top_level = (context->prev_context == NULL);
    if (!any_members && !top_level && 
        (!C_mode() || 
         (gnu_mode && is_array_type(context->type) &&
          skip_typerefs(context->type)->variant.array.bound_is_zero))) {
      /* This is an initialization of an aggregate with no members,
         i.e., an empty class or GNU zero-sized array, and there are no
         braces for this level of the aggregate.  Take nothing to
         satisfy this initialization. */
      *nothing_taken = TRUE;
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* any_initializers */


static a_boolean scan_array_element_subscript(a_type_ptr    dest_type,
                                              a_targ_size_t *subscript)
/*
This function scans an integral constant expression and checks that it can
be a valid subscript for the given array type.  dest_type is an array type,
or an error type if we don't know where we are.  The function returns FALSE
if an error occurs; otherwise TRUE is returned and *subscript is set to the
scanned value.
*/
{
  a_constant         constant;
  a_boolean          okay = TRUE;
  a_source_position  pos;

  pos = pos_curr_token;
  scan_integral_constant_expression(&constant);
  switch (constant.kind) {
    case ck_integer:
      if (sign_of_integer_constant(&constant) >= 0) {
        if (is_error_type(dest_type)) {
          /* Some previous error. */
          okay = FALSE;
        } else {
          a_boolean     overflow;
          a_targ_size_t value = unsigned_value_of_integer_constant(&constant,
                                                                   &overflow);
          /* Check that the subscript value is not too large. */
          check_assertion(is_array_type(dest_type));
          dest_type = skip_typerefs(dest_type);
          if (overflow ||
              (!is_incomplete_type(dest_type) &&
               value >= dest_type->variant.array.variant.number_of_elements)) {
            pos_error(ec_subscript_out_of_range, &pos);
            okay = FALSE;
          } else {
            *subscript = value;
          }  /* if */
        }  /* if */
      } else {
        /* Negative subscript. */
        pos_error(ec_subscript_out_of_range, &pos);
        okay = FALSE;
      }  /* if */
      break;
    case ck_template_param:
      pos_error(ec_template_dependent_designator, &pos);
      okay = FALSE;
      break;
    case ck_error:
      okay = FALSE;
      break;
    default:
      unexpected_condition_str(
                            "scan_array_element_subscript: bad constant kind");
  }  /* switch */
  return okay;
}  /* scan_array_element_subscript */


#if !GNU_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* designator_pos isn't used in some configurations. */
#endif /* !GNU_EXTENSIONS_ALLOWED */
static a_designation_state check_for_end_of_designation(
                                           a_boolean          allow_colon,
                                           a_boolean          assign_optional,
                                           a_source_position  *designator_pos)
/*
After a designator has been scanned, call this function to check if the
designation is completed (returns ds_complete_designation) or more
designators are to come (returns ds_partial_designation).  If extended field
designators of the form 'x:' are allowed, allow_colon should be set to true
(and the colon indicates a complete designation has been seen).  Similarly,
extended array element designators make the '=' optional and assign_optional
should be TRUE in that case.  The termination token is consumed if it is
present.  *designator_pos is the position of the designator that was just
scanned.
*/
{
  a_designation_state  result;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean            extended_syntax = FALSE;
#endif /* GNU_EXTENSIONS_ALLOWED */

  set_err_pos_to_curr_token();
  if (curr_token == tok_assign) {
    /* The next token is "=", so the designation is complete. */
    (void)get_token();
    result = ds_complete_designation;
  } else if (extended_designators_allowed && curr_token == tok_colon) {
    /* The next token is ":", so the designation is complete. */
    if (!allow_colon) {
      /* Something like ".f:" -- you can't mix the old-style and new-style
         designators. */
      error(ec_no_ordinary_and_extended_designators);
#if GNU_EXTENSIONS_ALLOWED
    } else {
      extended_syntax = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    (void)get_token();
    result = ds_complete_designation;
  } else if (curr_token != tok_period && curr_token != tok_lbracket) {
    /* A designator should be followed by '=' or another designator.
       When extended field designators are allowed, the '=' is optional on
       array element designators; otherwise, we have an error and we'll
       assume the token was forgotten. */
    if (!assign_optional) {
      error(ec_exp_assign);
#if GNU_EXTENSIONS_ALLOWED
    } else {
      extended_syntax = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    result = ds_complete_designation;
  } else {
    result = ds_partial_designation;
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    if (extended_syntax) {
      report_gnu_extension_if_needed(designator_pos,
                                     ec_extended_designator_is_gnu_extension);
    } else if (!c99_mode) {
      report_gnu_extension_if_needed(designator_pos,
                                     ec_designator_is_nonstandard);
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  return result;
}  /* check_for_end_of_designation */


static void append_initializer_constant(
                         an_aggregate_init_context_ptr context,
                         a_constant_ptr                constant)
/*
Add the IL entry constant to end of the list of constants tracked by context.
*/
{
  if (context->constant_list == NULL) {
    context->constant_list = constant;
  } else {
    context->end_of_constant_list->next = constant;
  }  /* if */
  context->end_of_constant_list = constant;
}  /* append_initializer_constant */


static void handle_invalid_designator_context(
                                          an_aggregate_init_context  *context)
/*
The upcoming tokens are a designator and the caller has established that it is
invalid in the given initialization context.  For example, we may have
encountered a field designator in an array context.  Issue an error if
appropriate, and set the context type to an error type.
*/
{
  if (!is_error_type(context->type)) {
    if (is_template_param_or_nonreal_class_type(context->type)) {
      /* A designator into a template dependent (i.e., unknown) type.
         We do not currently accept such cases. */
      error(ec_designator_for_template_dependent_type);
    } else {
      error(ec_invalid_designator_kind);
    }  /* if */
    context->type = error_type();
  }  /* if */
}  /* handle_invalid_designator_context */


static void get_array_designator(
                              an_aggregate_init_info_ptr init_info,
                              an_aggregate_init_context  *context,
                              a_targ_size_t              *curr_array_element)
/*
We're at the beginning of an array designator while scanning the initializer
for an entity whose type is context->type (an array type in non-error
cases).  Record the designator in the active list of constants, and
adjust *curr_array_element to reflect it.  The parameter context is a
pointer to a structure that keeps track of the initializer for the current
subaggregate (see get_initializer), while init_info tracks the whole
initializer.  An array designator is a C99 feature and looks like
   int a[20][40] = { [1] = { 7, 8, 9 }, [3][4] = 11 };
When extended_designators_allowed is TRUE, we also accept:
   int b[20][40] = { [1] { 7, 8, 9 }, [3 ... 7][4] = 11 };
This routine scans a single '[' <expr> ']' (or '[' <expr> '...' <expr> ']'
designator.  Designations consisting of multiple designators are handled by
the recursion in get_initializer.
*/
{
  a_boolean         okay = TRUE;
  a_targ_size_t     start_el, last_el;
  a_source_position start_pos;

  start_pos = pos_curr_token;
  if (!is_array_type(context->type)) {
    /* An attempt to use an array designator in a non-array context. */
    handle_invalid_designator_context(context);
    okay = FALSE;
  }  /* if */
  check_assertion(curr_token == tok_lbracket);
  /* Move past the left bracket and set a recovery point at the right
     bracket: */
  (void)get_token();
  add_stop_token(tok_rbracket);
  add_stop_token(tok_ellipsis);
  /* Parse the expression and evaluate it if possible: */
  okay &= scan_array_element_subscript(context->type, &start_el);
  if (extended_designators_allowed && curr_token == tok_ellipsis) {
    /* We're in a designator of the form "[ 5 ... 7 ]": scan the "..." and
       following integral constant expression. */
    (void)get_token();
    okay &= scan_array_element_subscript(context->type, &last_el);
    if (okay && last_el < start_el) {
      error(ec_no_negative_designator_range);
      okay = FALSE;
    }  /* if */
  } else {
    /* Normal form, no ending subscript. */
    if (okay) last_el = start_el;
  }  /* if */
  remove_stop_token(tok_ellipsis);
  error_position = start_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Move past the closing right bracket: */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  if (okay) {
     /* Build the ck_designator constant and add it to the list. */
    a_constant_ptr designator =
                         alloc_constant((a_constant_repr_kind)ck_designator);

    designator->variant.designator.array_element = start_el;
    append_initializer_constant(context, designator);
    if (last_el > start_el) {
      /* The initialization constant should be repeated at least twice.
         Make a ck_init_repeat constant and attach it to the current
         context. */
      context->repeat = alloc_constant((a_constant_repr_kind)ck_init_repeat);
      context->repeat->variant.init_repeat.count = last_el-start_el+1;
    }  /* if */
    *curr_array_element = last_el;
  } else {
    /* Some error. */
    context->type = error_type();
  }  /* if */
  /* See whether the designator list ends here. */
  init_info->designation_state = check_for_end_of_designation(
                             /*allow_colon=*/FALSE,
                             /*assign_optional=*/extended_designators_allowed,
                             &start_pos);
}  /* get_array_designator */


static void add_field_designator(an_aggregate_init_context   *context,
                                 a_field_ptr                 designated_field)
/*
Add the ck_designator constant for a field designator to the current
context.  designated_field identifies the field named by the designator.
*/
{
  a_constant_ptr designator =
                           alloc_constant((a_constant_repr_kind)ck_designator);

  designator->variant.designator.field = designated_field;
  append_initializer_constant(context, designator);
}  /* add_field_designator */


static void add_field_designator_for_anonymous_union(
                                            an_aggregate_init_context *context,
                                            a_field_ptr               *field)
/*
We are in the middle of processing designators for an anonymous union
or (nonstandard) anonymous struct member.  context->anonymous_union_field_sym
indicates the field named.  context->type indicates where we are now.
Add one level of ck_designator constant and return *field set to the
field designated at this level.  For levels before the last, the designator
is an implied one and the field is a generated one for an anonymous
parent object.  At the last level, the field is the one named;
context->anonymous_union_field_sym is reset to NULL at that point.
*/
{
  a_symbol_ptr member_sym = context->anonymous_union_field_sym;
  a_field_ptr  member_field;
  a_type_ptr   context_type = skip_typerefs(context->type);

  /* Work up through parent types looking for the level that comes next
     at the current context->type. */
  for (;;) {
    a_type_ptr  parent_class;
    check_assertion(member_sym != NULL &&
                    member_sym->kind == (a_symbol_kind)sk_field);
    member_field = member_sym->variant.field.ptr;
    parent_class = parent_class_of(member_field);
    if (same_entities(parent_class, context_type)) {
      break;
    }  /* if */
    member_sym = member_sym->variant.field.anonymous_parent_object;
  }  /* for */
  *field = member_field;
  /* Add the ck_designator constant for this next level. */
  add_field_designator(context, member_field);
  if (member_sym == context->anonymous_union_field_sym) {
    /* We made it to the designated field. */
    context->anonymous_union_field_sym = NULL;
  } /* if */
}  /* add_field_designator_for_anonymous_union */


static void get_field_designator(an_aggregate_init_info_ptr  init_info,
                                 an_aggregate_init_context   *context,
                                 a_field_ptr                 *field)
/*
We're at the beginning of a field designator while scanning the initializer
for an entity whose type is context->type (a struct/union type in non-error
cases).  Record the designator in the active list of constants, and
adjust *curr_field to reflect it.  The parameter context is a pointer
to a structure that keeps track of the initializer for the current
subaggregate (see get_initializer), while init_info tracks the whole
initializer.  A field designator is a C99 feature and looks like
   typedef struct X { int a, b, c; } X;
   struct Y { X p, q, r; } y = { .p = { 12, 13, 14 }, .q.b = 42 };
Some compilers also accept the following extended form:
   X x = { b: 71 }; // Only one extended designator per designation
This routine scans a single field designator.  Designations consisting of
multiple designators are handled by the recursion in get_initializer.
*/
{
  a_boolean         okay = TRUE, have_id = TRUE;
  a_field_ptr       designated_field;
  a_source_position start_pos;
  a_boolean         extended_form = FALSE;

  start_pos = pos_curr_token;
  *field = NULL;
  if (!is_real_class_type(context->type)) {
    /* An attempt to use a field designator in a non-struct/union context. */
    handle_invalid_designator_context(context);
    okay = FALSE;
  }  /* if */
  if (curr_token == tok_period) {
    /* Move past the period: */
    (void)get_token();
    /* Normally, we should find an identifier next: */
    if (curr_token != tok_identifier) {
      syntax_error(ec_exp_identifier);
      okay = FALSE;
      have_id = FALSE;
      if (curr_token == tok_identifier) (void)get_token();
    }  /* if */
  } else {
    check_assertion(extended_designators_allowed &&
                    curr_token == tok_identifier);
    /* Extended form: identifier followed by colon. */
    extended_form = TRUE;
  }  /* if */
  if (have_id) {
    /* Look up the field associated with the identifier: */
    if (!okay) {
      /* Can't look up the identifier; we don't know where we are. */
    } else {
      a_type_ptr    type_to_look_in = skip_typerefs(context->type);
      a_symbol_ptr  member_sym;
      if (!C_mode()) {
        /* If we're in an anonymous union (the field case), look for the field
           in the enclosing class scope. */
        a_class_type_supplement_ptr  ctsp = class_type_supp(type_to_look_in);
        while (ctsp->anonymous_union_kind ==
                                          (an_anonymous_union_kind)auk_field) {
          type_to_look_in = parent_class_of(type_to_look_in);
          ctsp = class_type_supp(type_to_look_in);
        }  /* while */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
      } else if (type_to_look_in
                 ->variant.class_struct_union.is_nonstd_anonymous_union_type) {
        /* Nonstandard anonymous-union-like constructs are possible in some
           C modes, but no class type supplement is available in those cases.
           Instead, we can use the context chain to recover the type in which
           the fields were promoted. */
        an_aggregate_init_context  *assoc_context = context;
        do {
          assoc_context = assoc_context->prev_context;
          type_to_look_in = skip_typerefs(assoc_context->type);
          check_assertion(is_immediate_class_type(type_to_look_in));
        } while (type_to_look_in
                  ->variant.class_struct_union.is_nonstd_anonymous_union_type);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
      }  /* if */
      member_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                             type_to_look_in, IDL_NO_OPTIONS);
      if (member_sym == NULL) {
        /* The name was not found. */
        pos_stsy_error(ec_not_a_field, &error_position,
                       locator_for_curr_id.symbol_header->identifier,
                       symbol_for(skip_typerefs(context->type)));
        okay = FALSE;
      } else if (member_sym->kind != (a_symbol_kind)sk_field) {
        /* The name was found, but it's not a field. */
        pos_st_error(ec_not_a_field_name, &error_position,
                     locator_for_curr_id.symbol_header->identifier);
        okay = FALSE;
        check_assertion(!C_mode());
      } else {
        designated_field = member_sym->variant.field.ptr;
        if (member_sym->variant.field.anonymous_parent_object != NULL) {
          /* This field is a member of an anonymous union or (nonstandard)
             anonymous struct.  We will have to put out one or more
             implicit designators to step down through the anonymous
             parent objects. */
          context->anonymous_union_field_sym = member_sym;
        }  /* if */
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Move past the identifier */
    (void)get_token();
  }  /* if */
  error_position = start_pos;
  if (okay) {
    if (context->anonymous_union_field_sym != NULL) {
      /* Special processing for the anonymous union case. */
      add_field_designator_for_anonymous_union(context, field);
    } else {
      /* Build the ck_designator constant and add it to the list. */
      add_field_designator(context, designated_field);
      *field = designated_field;
    }  /* if */
  } else {
    /* Some error. */
    context->type = error_type();
  }  /* if */
  /* See whether the designator list ends here. */
  init_info->designation_state =
       check_for_end_of_designation(
                             /*allow_colon=*/extended_form,
                             /*assign_optional=*/extended_designators_allowed,
                             &start_pos);
}  /* get_field_designator */


static a_boolean get_designator(
                               an_aggregate_init_info_ptr  init_info,
                               an_aggregate_init_context   *context,
                               a_targ_size_t               *curr_array_element,
                               a_field_ptr                 *curr_field)
/*
We're at a point where a designator might appear in an aggregate initializer.
If a designator appears, scan it, record a ck_designator constant in
the active list of constants for it, adjust *curr_array_element or
*curr_field to reflect it, and return TRUE.  Otherwise, return FALSE.
The parameter context is a pointer to a structure that keeps track of
the initializer for the current subaggregate (see get_initializer), while
init_info tracks the whole initializer.
*/
{
  a_boolean designator_present = FALSE;
  a_boolean array_designator;

  /* See whether there is a designator next. */
  /* If we've already seen the '=' that completed a designation,
     don't scan for another designator.  If there is one (e.g.,
     '[2] = [3] = ...') it will result in a syntax error. */
  if (init_info->designation_state != ds_complete_designation &&
      designator_coming(init_info, &array_designator)) {
    /* Process an array designator ( [x] = ) or a field designator
       ( .f = ). */
    if (designators_allowed && !C_mode() &&
        init_info->designation_state ==
                               (a_designation_state)ds_no_designation) {
      /* Allowing designators in non-POD types would raise subtle questions
         about order of initialization and destruction.  For now, at least,
         we disallow such constructs.  (The error is only issued on the first
         designator if there is a sequence of consecutive designators.) */
      a_type_ptr  elem_type = skip_typerefs(context->type);
      if (is_array_type(elem_type)) {
        elem_type = underlying_array_element_type(elem_type);
        elem_type = skip_typerefs(elem_type);
      }  /* if */
      if (is_immediate_class_type(elem_type) &&
          !elem_type->variant.class_struct_union.is_nonreal_class &&
          !symbol_supplement_for_class(elem_type)->is_POD) {
        pos_error(ec_designator_for_non_POD, &pos_curr_token);
      }  /* if */
    }  /* if */
    designator_present = TRUE;
    add_stop_token(tok_assign);
    if (array_designator) {
      get_array_designator(init_info, context, curr_array_element);
    } else {
      get_field_designator(init_info, context, curr_field);
    }  /* if */
    remove_stop_token(tok_assign);
  }  /* if */
  return designator_present;
}  /* get_designator */


static a_constant_ptr get_single_value_for_aggregate_initializer(
                              an_aggregate_init_info_ptr     init_info,
                              an_aggregate_init_context_ptr  context,
                              a_boolean                      extra_braces)
/*
Scans a "single value" as a simple non-class type item for an
aggregate initializer.  init_info describes the state of the complete
initializer and context describes the state of the initialization of
the current subaggregate.  extra_braces is TRUE if this is a recursive
call to deal with extra levels of braces: Some diagnostics issued in
the outermost call are not repeated during the recursive call.  The
function returns a pointer to an IL a_constant entity.  The
initializer expression may already be prescanned in
init_info->dps->prescanned_initializer_cache.
*/
{
  a_constant_ptr      constant; /* Result of this function */
  a_boolean           top_level = context->prev_context == NULL;
  a_boolean           nonconst_allowed, brace_flag, another_brace_next;
  a_boolean           extra_braces_okay = extra_braces;
  a_boolean           microsoft_enum_case = FALSE;
  a_dynamic_init_ptr  dip = NULL;
  a_source_position   pos_first_token;

  check_assertion(init_info != NULL && init_info->dps != NULL);
  if (anything_cached(&init_info->dps->prescanned_initializer_cache)) {
    /* There's a prescanned expression, so don't look at tokens. */
    brace_flag = another_brace_next = FALSE;
  } else {
    pos_first_token = pos_curr_token;
    check_for_opening_brace(&brace_flag);
    another_brace_next = (curr_token == tok_lbrace);
  }  /* if */
  if (brace_flag && !top_level && !extra_braces) {
    if (another_brace_next) {
      /* Two or more consecutive left braces: The second one will usually be
         diagnosed with an error later on; so diagnosing the first is
         superfluous in such cases. */
      extra_braces_okay =
                (gcc_mode ||
                 (microsoft_mode && (!C_mode() || microsoft_version < 1310)));
      if (extra_braces_okay) {
        /* Some Microsoft and GNU modes allow extraneous braces: We handle the
           extra braces through recursion below, but issue a warning on the
           first one. */
        pos_warning(ec_nonstd_braces, &pos_first_token);
      }  /* if */
    } else {
      /* A single level of braces is standard in C, but not in C++. */
      if (!C_mode()) {
        pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity
                                        : es_warning,
                       ec_nonstd_braces,
                       &pos_first_token);
      }  /* if */
    }  /* if */
  }  /* if */
  if (another_brace_next && extra_braces_okay) {
    /* In some modes, an arbitrary number of extraneous braces are accepted.
       Each level of braces can also contain a trailing comma.  For example:
         struct S s = { { { 1, }, }, };
       We handle these cases via recursion.  A warning will already have been
       issued for the outermost braces in the original (nonrecursive) call.
       (Source code rarely takes advantage of this bug, so the cost of
       recursion should be acceptable.) */
    constant = get_single_value_for_aggregate_initializer(
                                   init_info, context, /*extra_braces=*/TRUE);
    goto process_closing_brace;
  }  /* if */
  if (is_error_type(context->type) &&
      curr_token_if_nothing_cached_is(tok_rbrace, init_info->dps)) {
    /* Prevent cascading errors by returning an error constant here. */
    constant = alloc_constant((a_constant_repr_kind)ck_error);
    set_error_constant(constant);
  } else {
    a_type_ptr  required_type = context->type;
    if (init_info->designation_state == ds_partial_designation) {
      /* A designator has been started but not completed.  Yet an additional
         designator cannot appear at this level.  For example:
           int a[10] = { [1][2] = 3 };
                            ^-- An "=" is required here.
      */
      error(ec_exp_assign);
    }  /* if */
    if (!C_mode()) {
      nonconst_allowed = TRUE;
    } else if (allow_nonconstant_auto_aggr_init_in_c_mode) {
      /* C99 permits a nonconstant initializer in the aggregate initialization
         of an automatic variable.  This is also accepted by GNU and Microsoft
         compilers. */
      nonconst_allowed = !init_info->static_lifetime;
    } else {
      nonconst_allowed = FALSE;
    }  /* if */
    if (microsoft_mode && !C_mode() && !top_level &&
        context->prev_context->field != NULL &&
        context->prev_context->field->is_bit_field &&
        is_enum_type(context->type)) {
      /* Microsoft compilers allow bit fields of enumeration types to be
         initialized by integer values. */
      required_type = integer_type(skip_typerefs(context->type)
                                                  ->variant.integer.int_kind);
      microsoft_enum_case = TRUE;
    }  /* if */
    constant = scan_initializer_of_simple_object(
                                     init_info->dps,
                                     init_info, context,
                                     nonconst_allowed,
                                     (a_boolean)init_info->static_lifetime,
                                     /*force_object_lifetime=*/FALSE,
                                     (a_boolean)init_info->compound_literal,
                                     /*is_copy_initialization=*/TRUE,
                                     &required_type, &dip);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    init_info->init_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (constant == NULL) {
      /* Returning NULL means a nonconstant expression was scanned, and so
         a dynamic init entry was allocated and returned.  Create a dynamic
         init constant to point to it. */
      constant = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      constant->variant.dynamic_init = dip;
      constant->type = context->type;
      if (microsoft_enum_case) {
        /* The initialization of an enum bit field in Microsoft mode.
           Add a cast to the expression under the dynamic initialization
           to adjust the expression from integral to the desired
           enum destination type. */
        check_assertion(dip->kind == (a_dynamic_init_kind)dik_expression);
        dip->variant.expression = add_cast(dip->variant.expression,
                                           context->type);
      }  /* if */
    } else if (microsoft_enum_case) {
      /* The initialization of an enum bit field in Microsoft mode.  We
         scanned as if an integer bit field was being initialized, but the
         destination type is an enumeration. */
      a_boolean  did_not_fold = FALSE;
      check_assertion(microsoft_mode &&
                      context->prev_context != NULL &&
                      context->prev_context->field != NULL &&
                      context->prev_context->field->is_bit_field);
      type_change_constant_full(constant, context->type,
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
                                &error_position);
    }  /* if */
  }  /* if */
  if (dip != NULL) {
    /* Dynamic initialization. */
    context->any_dynamic_initialization = TRUE;
    /* Since the destructor may have been added to a dynamic init entry
       that will not be "on top" when gen_dynamic_initialization is called,
       record the destruction, if needed, with the appropriate
       object-lifetime entry. */
    record_end_of_lifetime_destruction(dip,
                                       (a_boolean)init_info->static_lifetime,
                                       /*block_lifetime=*/TRUE);
  }  /* if */
  /* If there was an initial opening brace, check for and skip the
     closing brace now.  Check also for an extra comma (required in C++
     per ARM 8.4, offered in C along with the extension that permits
     brace-enclosed initializers on non-aggregate variables in the first
     place). */
process_closing_brace:
  if (brace_flag && curr_token == tok_comma) (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (brace_flag && curr_token == tok_rbrace) {
    init_info->init_end_position = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  check_for_matching_closing_brace(brace_flag);
  return constant;
}  /* get_single_value_for_aggregate_initializer */


static a_constant_ptr get_initializer(
                              a_type_ptr                    *type,
                              an_aggregate_init_info_ptr    init_info,
                              an_aggregate_init_context_ptr prev_init_context,
                              a_boolean                     *nothing_taken,
                              a_boolean                     *any_dynamic_init)
/*
Scan a constant initializer or initializer list, and return a pointer to the
constant for it (an aggregate constant if an initializer list is scanned).
*type indicates the type of the object being initialized.  It will be
updated if the object is an incomplete array whose size is now known because
it is initialized.  If there is some error in the initializer, an error
constant is returned.  init_info is a pointer to a block of information
tracking this initialization.  top_level is TRUE if this is a top-level
initializer (braces are required surrounding initializers for unions and
aggregates at that level).  *nothing_taken is returned TRUE if no source
tokens were taken because the entity being initialized is an empty
class.  *any_dynamic_init is returned TRUE if the entity being initialized
required dynamic initialization -- i.e., if the constant entry returned by
this function points to a tree that includes a dynamic-init entry.
*/
{
  a_constant_ptr                 init_con = NULL;
  a_boolean                      is_flexible_array;
  a_type_ptr                     member_type;
  a_boolean                      brace_flag;
  a_constant_ptr                 member_con;
  a_boolean                      any_more_initializers, any_more_members;
  a_boolean                      local_nothing_taken, local_any_dynamic_init;
  a_type_kind                    kind;
  a_boolean                      array_too_long_error_given = FALSE;
  a_boolean                      took_extra_comma;
  a_dynamic_init_ptr             dip;
  a_class_symbol_supplement_ptr  cssp;
  an_aggregate_init_context      context;
  a_boolean                      top_level = (prev_init_context == NULL);
  a_source_position              initializer_pos;
  a_decl_parse_state             *dps;
  a_boolean                      saved_initializer_is_expr_list;

  db_enter(4, "get_initializer");
  check_assertion(init_info != NULL);
  dps = init_info->dps;
  check_assertion(dps != NULL);
  *nothing_taken = FALSE;
  *any_dynamic_init = FALSE;
  initialize_init_context(&context, prev_init_context, *type);
  /* See if we're initializing an aggregate and the next initializer
     expression initializes the whole aggregate. */
  if (process_whole_object_init(init_info, &context, &init_con)) {
    /* The next expression initializes the whole aggregate, or there
       was an error and a diagnostic has been issued.  In either case,
       init_con has been set to the right initializer value. */
  } else if (is_aggregate_or_union_type(context.type) ||
#if GNU_VECTOR_TYPES_ALLOWED
             (gnu_mode && is_vector_type(context.type) &&
              curr_token_if_nothing_cached_is(tok_lbrace, dps)) ||
#endif /* GNU_VECTOR_TYPES_ALLOWED */
             ((is_template_param_or_nonreal_class_type(context.type) ||
               is_error_type(context.type)) &&
              (curr_token_if_nothing_cached_is(tok_lbrace, dps) ||
               (init_info->designation_state != ds_complete_designation &&
                designator_is_next(init_info, &context))))) {
    /* Initialization of a class/struct/union, array (complete or incomplete),
       or GNU vector ("aggregate types" are required here).  The result will
       be an aggregate constant except when an array of char is initialized by
       a string.  The initial values can either appear inside a brace-enclosed
       list, or at the current level (except for vectors, where the braces
       cannot be omitted if element values are specified). */
    if (curr_token_if_nothing_cached_is(tok_lbrace, dps)) {
      /* Make sure it's truly an aggregate and not some non-aggregate class: */
      /* For a nonreal class, we cannot relate the initializers to the inner
         type structure of that class, but that is handled correctly in what
         follows. */
      if (is_real_class_type(context.type) &&
          !symbol_supplement_for_class(context.type)->is_class_aggregate) {
        if (gpp_mode && is_prototype_instantiation_context()) {
          /* The g++ compiler treats all types in prototype instantiations as
             "unknown". */
          context.type = type_of_unknown_templ_param_nontype;
        } else {
          pos_ty_error(ec_brace_initialization_not_allowed, &pos_curr_token,
                       context.type);
          context.type = error_type();
        }  /* if */
      }  /* if */
      check_for_opening_brace(&brace_flag);
      /* Since we saw a left brace, we start afresh with designations: */
      init_info->designation_state = ds_no_designation;
    } else {
      brace_flag = FALSE;
    }  /* if */
    /* Record that we're now processing a comma-separated expression list,
       which enables variadic template pack expansions. */
    saved_initializer_is_expr_list = dps->initializer_is_expr_list;
    dps->initializer_is_expr_list = TRUE;
    if (!(brace_flag && is_template_dependent_type(*type)) &&
        process_string_constant_initializer(type, &init_con,
                                            (a_decl_parse_state *)NULL,
                                            init_info, &context)) {
      /* The object being initialized has type array of characters, and
         is being initialized with a string.  In prototype instantiations,
         we must beware of something like
           T s[] = { "a", "b" };  // "T" is a template parameter.
         which is valid, but cannot be handled here.  We therefore also do
         not handle "T s[] = { "a" };" here, even though it would be
         appropriate to do so if we could distinguish the two cases. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      init_info->init_end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Allow an extra comma after the string-constant initializer and before
         the expected right brace. */
      if (brace_flag && curr_token == tok_comma) (void)get_token();
    } else {
      /* Normal case, not array of char.  Could be an array, a struct,
         or a union, or an error type. */
      a_targ_size_t curr_array_element = 0, array_size = 0;
      a_field_ptr   curr_field;
      a_boolean     discard_initializers = FALSE, is_gnu_vector = FALSE;
      /* In ANSI C and C++, the top-level initializer for a struct, union, or
         array/vector must be surrounded by braces.  e.g., "int a[1] = 1;" is
         not allowed.  However, pcc will allow initialization with a single
         value and we allow it as an extension. */
      if (top_level && !brace_flag) {
        /* If a hard error is decided, context.type will become an error
           type. */
        handle_missing_brace(init_info, &context);
      }  /* if */
      if (dps->prescanned_initializer_levels_down > 0) {
        /* We prescanned an expression that applies at a lower level.  Count
           our way down to the right level.  Note that we do this after we
           do the string check above, which is still at the higher level. */
        dps->prescanned_initializer_levels_down--;
      }  /* if */
      /* Get information on the first member of the aggregate to be
         initialized (if any). */
      start_aggregate_init_scan_loop(&context, &kind, &member_type,
                                     &any_more_members, &is_flexible_array);
#if GNU_VECTOR_TYPES_ALLOWED
      is_gnu_vector = (kind == (a_type_kind)tk_vector);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      curr_field = context.field;
      any_more_initializers = any_initializers(init_info, &context,
                                               brace_flag,
                                               any_more_members,
                                               nothing_taken);
      took_extra_comma = FALSE;
      /* Loop, scanning initializers and building an aggregate constant. */
      while (any_more_initializers) {
        add_stop_token(tok_comma);
        /* See whether a designator is next (except in initializers for GNU
           vectors). */
        if (context.anonymous_union_field_sym != NULL) {
          /* Special processing for the anonymous union case. */
          add_field_designator_for_anonymous_union(&context, &curr_field);
        } else if (!is_gnu_vector &&
                   get_designator(init_info, &context, &curr_array_element,
                                  &curr_field)) {
          /* A designator was present and has been processed. */
          init_info->uses_designated_initializers = TRUE;
          /* If initializers were being discarded because we ran out of
             array elements, we can now start recording the initializers
             again (GNU C mode). */
          if (discard_initializers && !is_error_type(context.type)) {
            discard_initializers = FALSE;
            kind = skip_typerefs(context.type)->kind;
            /* member_type had been set to an error type.  For struct
               initializers, it is reset in every iteration of the loop,
               but for arrays we need to restore it here. */
            if (kind == (a_type_kind)tk_array) {
              member_type =
                      skip_typerefs(context.type)->variant.array.element_type;
            }  /* if */
          }  /* if */
        } else {
          /* No designator. */
          if (!any_more_members) {
            /* There are more undesignated initializers, but we've run out of
               members into which to put them. */
            if (gcc_mode) {
              /* In GNU C mode, excess initializers are ignored (with
                 a warning). */
              if (!discard_initializers) {
                warning(ec_excess_initializers_ignored);
              }  /* if */
              discard_initializers = TRUE;
            } else {
              error(ec_too_many_initializer_values);
              context.type = error_type();
              any_more_members = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Determine the type of the member being initialized. */
        if (is_error_type(context.type) ||
            (gnu_mode && !any_more_members && any_more_initializers &&
             discard_initializers)) {
          /* Either some error was detected, or we are in GNU mode (where
             excess initializers are ignored with a warning).  We don't know
             where we are or what we're initializing. */
          member_type = error_type();
          kind = (a_type_kind)tk_error;
        } else if (kind == (a_type_kind)tk_template_param) {
          /* The destination type is unknown (i.e., template dependent).
             The member type is therefore also unknown. */
          member_type = type_of_unknown_templ_param_nontype;
        } else if (kind == (a_type_kind)tk_array || is_gnu_vector) {
          /* member_type was set outside the loop. */
#if DEBUG
          if (debug_level == 4) {
            fprintf(f_debug, "Getting initializer for element %lu, type = ",
                             (unsigned long)curr_array_element);
            db_abbreviated_type(member_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
        } else {
          /* struct or union type: get the type of the current field. */
          check_assertion(curr_field != NULL);
          member_type = curr_field->type;
#if DEBUG
          if (debug_level == 4) {
            fputs("Getting initializer for field \"", f_debug);
            db_name(&curr_field->source_corresp);
            fputs("\", type = ", f_debug);
            db_abbreviated_type(member_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
          if (skip_typerefs(member_type)->size == 0) {
            /* Members of unions or aggregates cannot have size zero (which
               either indicates an incomplete type or a zero-length array in
               some modes)... */
            if (is_array_type(member_type) &&
                (curr_field->next == NULL || kind == (a_type_kind)tk_union)) {
              /* ... except that in several modes it's okay to declare a field
                 of zero-sized array type when it's the last field in the
                 struct (or, in Microsoft mode, any field in a union).  (See
                 also: check_field_type.)  Only in Microsoft and GNU C modes
                 can such a field be initialized (and in GNU C mode, the
                 initialization is only allowed when the target has static
                 storage duration). */
              if (microsoft_mode || (gcc_mode && init_info->static_lifetime)) {
                a_type_ptr  element_type =
                                   underlying_array_element_type(member_type);
                if (!C_mode() && is_class_struct_union_type(element_type)) {
                  element_type = skip_typerefs(element_type);
                  cssp = symbol_supplement_for_class(element_type);
                  if (has_nontrivial_destructor(cssp)) {
                    /* Microsoft C++ allows the aggregate initialization of
                       flexible array members only if they do not have
                       nontrivial destructors. */
                    error(ec_cannot_initialize_destructible_flexible_array);
                  }  /* if */
                } else if (gcc_mode && !top_level &&
                           !(curr_token_if_nothing_cached_is(tok_lbrace, dps)&&
                             next_token() == tok_rbrace)) {
                  /* GNU C does not allow flexible array member initializers
                     that are not at the top level, except if the initializer
                     is empty.  For example:
                       struct F { int n; int a[]; };
                       struct T { struct F f; };
                       T x1 = { { 1, {} } };  // Okay: non-top-level but empty.
                       T x2 = { { 1, { 2 } } };  // Error.
                  */
                  error(ec_cannot_initialize_indirect_flexible_array);
                }  /* if */
                init_info->has_flexible_array_initializer = TRUE;
                any_more_members = FALSE;
                /* Record the current position in case a diagnostic must be
                   issued below. */
                initializer_pos = pos_curr_token;
              } else if (is_incomplete_type(member_type)) {
                error(gcc_mode ? ec_cannot_init_auto_flexible_array_member
                               : ec_cannot_initialize_flexible_array_member);
              }  /* if */
            } else {
              /* The only other zero-sized type that should be allowed here
                 is a (complete) zero-sized GNU C class. */
              check_assertion_str(gnu_mode && !is_incomplete_type(member_type),
                                  "get_initializer: can't init 0-size member");
            }  /* if */
          }  /* if */
        }  /* if */
        /* Get the initializer for this one member. */
        set_err_pos_to_curr_token();
        member_con = get_initializer(&member_type, init_info, &context,
                                     &local_nothing_taken,
                                     &local_any_dynamic_init);
        if (!is_flexible_array && init_info->has_flexible_array_initializer) {
          /* We just scanned an aggregate initializer for a flexible array
             member. */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
          member_con->flexible_array_initializer = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
          if (gcc_mode && local_any_dynamic_init) {
            /* In GNU C mode, a flexible array initializer can not include a
               dynamic component. */
            pos_error(ec_nonconstant_flexible_array_member_init,
                      &initializer_pos);
          }  /* if */
        }  /* if */
        /* If exceptions are enabled and the type of the member being
           initialized is a class with a destructor, it may be appropriate to
           record the destructor in case an exception is thrown before the
           top-level object is fully constructed. */
        if (exceptions_enabled &&
            member_con->kind != (a_constant_repr_kind)ck_dynamic_init &&
            is_class_struct_union_type(member_type)) {
          cssp = symbol_supplement_for_class(member_type);
          if (has_nontrivial_destructor(cssp)) {
            a_routine_ptr  dtor_rp = cssp->destructor->variant.routine.ptr;
            if (local_any_dynamic_init) {
              /* Not a ck_dynamic_init, yet there was dynamic initialization:
                 this must be an aggregate constant with a ck_dynamic_init
                 in its tree somewhere.  Put a dik_nonconstant_aggregate on
                 top of it (instead of a dik_constant). */
              check_assertion(member_con->kind ==
                                      (a_constant_repr_kind)ck_aggregate);
              dip = alloc_dynamic_init(
                         (a_dynamic_init_kind)dik_nonconstant_aggregate);
            } else {
              /* Normal case: dik_constant is okay to use. */
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
            }  /* if */
            dip->variant.constant = member_con;
            add_dtor_for_partially_constructed_aggregate(dtor_rp, dip);
            /* Now put a ck_dynamic_init constant on top of the new dynamic
               init entry, to which the destructor has been attached. */
            member_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
            member_con->type = member_type;
            member_con->variant.dynamic_init = dip;
            context.any_dynamic_initialization = TRUE;
          }  /* if */
        }  /* if */
        /* If this initializer was preceded by a array-range designator, put
           the pending ck_init_repeat constant on top of member_con: */
        if (context.repeat != NULL) {
          if (local_any_dynamic_init) {
            /* Extended designators of the form '[i ... j]' cannot be applied
               to initializers with a dynamic component. */
            error(ec_no_range_designator_with_dynamic_init);
          } else {
            /* If the constant we're going to repeat contains an aggregate
               (without braces) designated initializer and there is more than
               a single initializer in that aggregate, set the
               multidimensional_aggr_tail_not_repeated flag so the layout for
               this initializer will be handled correctly in lowering.  
               For example: int X[3][3] = { [0 ... 2][0] = 4, 5, 6 }; */
            a_constant_ptr const_ptr = member_con;
            while (const_ptr != NULL) {
              if (const_ptr->kind == (a_constant_repr_kind)ck_aggregate) {
                if (const_ptr->explicit_braces_on_aggregate) {
                  break;
                } else if (const_ptr->variant.aggregate.first_constant != 
                                                                        NULL &&
                           const_ptr->variant.aggregate.first_constant->kind
                                      == (a_constant_repr_kind)ck_designator) {
                  a_constant_ptr desig_con = 
                                 const_ptr->variant.aggregate.first_constant;
                  check_assertion(desig_con != NULL && 
                                  desig_con->next != NULL);
                  if (desig_con->next->next != NULL) {
                    context.repeat->variant.init_repeat.
                                multidimensional_aggr_tail_not_repeated = TRUE;
                    break;
                  }  /* if */
                }  /* if */
                const_ptr = const_ptr->variant.aggregate.first_constant;
              } else if (const_ptr->kind == 
                                        (a_constant_repr_kind)ck_init_repeat) {
                const_ptr = const_ptr->variant.init_repeat.constant;
              } else if (const_ptr->kind == 
                                         (a_constant_repr_kind)ck_designator) {
                const_ptr = const_ptr->next;
              } else {
                break;
              }  /* if */
            }  /* while */
            context.repeat->variant.init_repeat.constant = member_con;
            member_con = context.repeat;
            context.repeat = NULL;
          }  /* if */
        }  /* if */
        if (!discard_initializers) {
          /* Add the constant entry to the list of constants. */
          append_initializer_constant(&context, member_con);
        }  /* if */
        /* If a designation was active, it is now consumed: */
        init_info->designation_state = ds_no_designation;
        remove_stop_token(tok_comma);
        check_assertion(!(local_nothing_taken && is_flexible_array));
        /* Advance to the next member of the aggregate.  Set
           any_more_members FALSE if there are no more members. */
        if (kind == (a_type_kind)tk_error ||
            kind == (a_type_kind)tk_template_param) {
          /* Unknown destination type: there is nothing to "advance". */
        } else if (kind == (a_type_kind)tk_array) {
          /* Array; see if there are any elements remaining. */
          if (curr_array_element == targ_size_t_max) {
            /* Array too long; presumably, this is an incomplete array
               being initialized with a ridiculous number of initial
               values.  Note that it is okay to do the check and increment
               before knowing whether or not there is an initializer for
               this element because after the last initializer of an incomplete
               array the curr_array_element will indicate the size, which
               is also subject to the same range check. */
            if (!array_too_long_error_given) {
              error(ec_array_size_too_large);
              array_too_long_error_given = TRUE;
            }  /* if */
          } else {
            /* Advance to next array element. */
            ++curr_array_element;
            check_assertion(!skip_typerefs(context.type)->variant.array.
                                                      is_variable_size_array);
            if (!is_flexible_array) {
              /* Note that we may get here with any_more_members == FALSE and
                 a designator can turn it into TRUE again. */
              a_type_ptr array_type = skip_typerefs(context.type);
              /* In general, more "members" are available if the array length
                 is larger than the current element index.  In the case of
                 templates, the array length may not be known, or the
                 underlying type could be a type that allows for additional
                 initializers: In those cases any_more_members is also TRUE. */
              any_more_members =
                (array_type->variant.array.is_template_dependent_size_array ||
                 array_type->variant.array.variant.number_of_elements
                                                       > curr_array_element ||
                 is_template_param_type(
                                  underlying_array_element_type(array_type)));
            } else {
              /* Keep track of the maximum subscript seen: */
              if (curr_array_element > array_size) {
                array_size = curr_array_element;
              }  /* if */
            }  /* if */
          }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
        } else if (is_gnu_vector) {
          /* Advance to next vector element. */
          a_type_ptr     vec_type = skip_typerefs(context.type);
          ++curr_array_element;
          if (vector_type_is_template_dependent(vec_type)) {
            /* The number of elements is unknown due to template dependencies:
               Assume there are more elements. */
            any_more_members = TRUE;
          } else if (skip_typerefs(member_type)->size == 0) {
            expect_error();
          } else {
            a_targ_size_t  n_vector_elems =
                              vec_type->size/skip_typerefs(member_type)->size;
            any_more_members = n_vector_elems > curr_array_element;
          }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        } else if (kind == (a_type_kind)tk_class ||
                   kind == (a_type_kind)tk_struct) {
          /* Advance to the next named field of the class or struct. */
          curr_field = next_initializable_field(curr_field->next);
          context.field = curr_field;
          /* Note that any_more_members might be FALSE and then turn TRUE
             again because a designator was encountered. */
          any_more_members = (curr_field != NULL);
          /* Check for no fields remaining. */
          if (curr_field == NULL) {
            /* Nothing to do. */
          } else if (microsoft_mode || gcc_mode) {
            /* In GNU C and Microsoft modes, the check for a field of
               incomplete array type is not made -- such initializations are
               allowed (but only for top-level fields in GNU C mode). */
          } else if (curr_field->next == NULL &&
                     is_incomplete_type(curr_field->type)) {
            /* Also exit on an incomplete array as the final field of a
               struct (allowed as an extension, but not allowed to be
               initialized).  This would come up in a case like
                 struct {int i; int j[];} = {0, 0};  <-- Error on 2nd 0.
            */
            any_more_members = FALSE;
          }  /* if */
        } else {
          /* Only the first field in a union is initialized, so having done
             that field, we are done with the union. */
          check_assertion(kind == (a_type_kind)tk_union);
          any_more_members = FALSE;
        }  /* if */
        /* See if there are any more initializer expressions in the source
           input that should be taken as part of the current aggregate. */
        if (local_nothing_taken) {
          /* The initializer was not scanned, so we don't want to look for
             a comma.  any_more_initializers remains TRUE. */
        } else if (anything_cached(&dps->prescanned_initializer_cache)) {
          /* There is at least one expression cached, so go around again. */
          any_more_initializers = TRUE;
        } else if (!brace_flag && top_level) {
          /* If this is a top-level list that is not brace-enclosed
             (an error or extension, except in pcc mode) we must stop now,
             having taken only one value.  Do not check for a comma,
             because if present it would be part of the declaration
             syntax rather than an initializer list separator:
               int a[2] = 1, b;
                           ^not an initializer list separator
          */
          any_more_initializers = FALSE;
        } else if (!brace_flag && !any_more_members) {
          /* There are no more members and this is not a brace-enclosed
             list, so take no more initializers. */
          any_more_initializers = FALSE;
        } else {
          /* Skip a comma separating initializers.  This might be an extra
             comma at the end of the list.  The comma might already have been
             skipped while looking ahead for a designator: in that case,
             init_info->comma_seen should be set. */
          any_more_initializers = init_info->comma_seen ||
                                  loop_token(tok_comma);
          init_info->comma_seen = FALSE;
          /* Always end the loop upon encountering a right brace.  This might
             be the right brace matching the opening brace for this list
             (in the case that there was one), or a brace closing some
             higher-level list, which nevertheless serves to end this
             lower-level list.  In either case, however, if a comma was
             just taken, it is "extra" and no extra comma should be allowed
             outside the loop. */
          if (curr_token == tok_rbrace) {
            took_extra_comma = any_more_initializers;
            any_more_initializers = FALSE;
          } else if (designator_is_next(init_info, &context)) {
            /* A designator ends a non-brace-enclosed list of initializers,
               but if it is brace-enclosed then an upcoming designator means
               more initializers are following. */
            if (!any_more_initializers) {
              error(ec_exp_comma);
            } else if (!brace_flag) {
              /* The comma really indicated that there are more initializers at
                 a previous level.  Record it has been seen. */
              init_info->comma_seen = TRUE;
            }  /* if */
            any_more_initializers = brace_flag;
          }  /* if */
        }  /* if */
        /* Keep looping while there are more initializers. */
      }  /* while */
      /* There are no more initializers in the source (at least, none
         that should be considered part of the current aggregate). */
      if (kind == (a_type_kind)tk_error) any_more_members = FALSE;
      /* The entire list of values for the entity being initialized has
         now been read.  We stopped either because we exhausted the
         initial values or because we ran out of members to initialize. */
      if (top_level && !brace_flag && is_error_type(context.type)) {
        /* An error has been issued on the missing {...} list.  Although
           the initializer was scanned anyway, don't create an aggregate
           constant to represent the initialization -- just an error constant
           will do (the default upon exiting the routine). */
      } else {
        /* Set the size of an incomplete array from the number of elements
           in its initial value.  Note that arrays of char initialized
           to strings are not handled here. */
        if (is_flexible_array) {
          /* Note that the size is not necessarily curr_array_element since
             intermediate designations may have implied a larger size. */
          if (!is_error_type(context.type)) {
            set_initialized_array_size(&context.type, array_size);
          }  /* if */
          *type = context.type;
          any_more_members = FALSE;
        } else if (C_dialect == C_dialect_cplusplus) {
          if (curr_token == tok_rbrace && kind == (a_type_kind)tk_array) {
            /* When the number of initializers is fewer than the number of
               array elements to be initialized, and when the element type is
               such that a constructor is required to initialize the elements,
               we are required to provide default initialization by calling
               the default constructor. */
            if (init_remaining_array_elements(context.type, curr_array_element,
                                              init_info, &context)) {
              any_more_members = FALSE;
            }  /* if */
          } else if (kind == (a_type_kind)tk_struct ||
                     kind == (a_type_kind)tk_class) {
            if (init_remaining_fields(init_info, &context)) {
              curr_field = context.field;
              if (curr_field == NULL) any_more_members = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Allocate the aggregate constant that is the value for the
           initializer. */
        init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
        init_con->type = context.type;
        init_con->variant.aggregate.first_constant = context.constant_list;
        init_con->variant.aggregate.last_constant =
                                          context.end_of_constant_list;
        if (any_more_members) init_info->any_uninitialized_member = TRUE;
        if (brace_flag) {
          /* Remember the explicit braces.  This affects the meaning of
             some designated initializers. */
          init_con->explicit_braces_on_aggregate = TRUE;
          /* Allow an extra comma before the "}" in a brace-enclosed list.
             Do not allow it if an extra comma was taken already in 
             the loop. */
          if (curr_token == tok_comma && !took_extra_comma) {
            (void)get_token();
            took_extra_comma = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */  
    dps->initializer_is_expr_list = saved_initializer_is_expr_list;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (brace_flag && curr_token == tok_rbrace) {
      init_info->init_end_position = pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* If there was an initial opening brace, check for and skip the
       closing brace now. */
    check_for_matching_closing_brace(brace_flag);
  } else {
    /* Non-aggregate/union case -- initializer is a single (possibly
       brace-enclosed) value. */
    init_con = get_single_value_for_aggregate_initializer(
                                 init_info, &context, /*extra_braces=*/FALSE);
  }  /* if */
  if (prev_init_context != NULL) {
    /* Let the flag recording whether there were any dynamic initializations
       seen in this call to get_initializer percolate up to the next level. */
    if (context.any_dynamic_initialization) {
      prev_init_context->any_dynamic_initialization = TRUE;
    }  /* if */
  }  /* if */
  /* If the return value constant was not allocated (because of an error),
     allocate an error constant to return. */
  if (init_con == NULL) {
    init_con = alloc_constant((a_constant_repr_kind)ck_error);
    set_error_constant(init_con);
  } else {
    /* Return to the caller information about whether init_con involves a
       dynamic-init entry. */
    *any_dynamic_init = context.any_dynamic_initialization;
    if (init_info->uses_designated_initializers) {
      /* This initializer uses designated initializers. */
      init_con->uses_designated_initializers = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
  return(init_con);
}  /* get_initializer */


void scan_compound_literal_initializer(a_type_ptr         *type,
                                       a_boolean          is_static,
                                       a_dynamic_init_ptr *dip)
/*
Scan the brace-enclosed part of a C99 compound literal.  Such literals are of
the form (type){initializer} or (type){initializer,}.  The type provided in
parentheses is passed to this function through parameter *type; if this type
is incomplete, the complete type should be deduced from the initializer and
*type will be updated with that complete type.  is_static indicates whether
the literal appears outside a function body (in which case it has static
storage duration) or inside a function body (in which case it is automatic and
hence is_static is passed as FALSE).  A dynamic init entry is created by this
function and a pointer to it is returned through dip.  The caller is
responsible for ensuring that the current token is a brace, and the
function get_initializer does all the hard work.
*/
{
  a_constant_ptr         compound_constant;
  an_aggregate_init_info info;
  a_decl_parse_state     dps;
  a_boolean              no_token_consumed, any_dynamic_init;
  a_boolean		 err = FALSE;

  check_assertion((C_mode() || gpp_mode) && (curr_token == tok_lbrace));
  init_decl_parse_state(&dps);
  initialize_init_info(&info, is_static, &dps);
  info.compound_literal = TRUE;
  compound_constant = get_initializer(type, &info,
                                      (an_aggregate_init_context_ptr)NULL,
                                      &no_token_consumed,
                                      &any_dynamic_init);
  compound_constant->explicit_cast_applied = TRUE;
  if (is_error_type(*type)) {
    /* The literal has an invalid type.  Don't build a dynamic init entry. */
    err = TRUE;
    *dip = NULL;
  } else if (!any_dynamic_init) {
    /* A truly constant value (scalar or aggregate). */
    *dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
    (*dip)->variant.constant = compound_constant;
  } else {
    /* There is a dynamic component to this literal, so create a dynamic init
       entry of kind dik_expression (for nonaggregates) or
       dik_nonconstant_aggregate depending on the type of the literal. */
    if (is_aggregate_or_union_type(*type) ||
#if GNU_VECTOR_TYPES_ALLOWED
        is_vector_type(*type) ||
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        /* A g++-mode compound literal like (T){42, f()} with T a template
           parameter can result in a ck_aggregate constant with a
           tk_template_param type. */
        (is_template_param_type(*type) &&
         compound_constant->kind == (a_constant_repr_kind)ck_aggregate)) {
      check_assertion(compound_constant->kind ==
                                          (a_constant_repr_kind)ck_aggregate);
      *dip =
           alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
      (*dip)->variant.constant = compound_constant;
    } else {
      /* get_initializer (through get_single_value_for_aggregate_initializer
         and its helpers) created a constant on top of a dynamic init entry.
         Extract it back out of the constant. */
      check_assertion(compound_constant->kind ==
                                       (a_constant_repr_kind)ck_dynamic_init);
      *dip = compound_constant->variant.dynamic_init;
    }  /* if */
  }  /* if */
  if (!err) {
    (*dip)->is_compound_literal = TRUE;
    if (info.any_uninitialized_member ||
        info.uses_designated_initializers) {
      (*dip)->is_partially_initialized_compound_literal = TRUE;
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Record the position of the closing brace. */
  curr_construct_end_position = info.init_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* scan_compound_literal_initializer */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_boolean scan_initializer_list(a_decl_parse_state    *dps,
                                       a_type_ptr            *type,
                                       a_variable_ptr        vp,
                                       a_boolean             static_lifetime,
                                       a_constant_ptr        *init_con,
                                       a_dynamic_init_ptr    *init_dip,
                                       a_source_position     *err_pos,
                                       a_decl_pos_block_ptr  decl_pos_block)
/*
Scan an initializer list for an aggregate initialization.  Usually it is a
brace-enclosed list of initializers, but the case of initializing an
array-of-char with a string is also handled here.  *dps describes general
properties of the declaration (and its initializer).  *type points to the type
of the variable being initialized, and vp (which may be NULL in error cases)
points to the variable.  (*type is passed independently because it may be
modified as part of initializer processing, but the variable should not
necessarily be updated.)  static_lifetime is TRUE for global and local static
variables.  Either *init_con or *init_dip (but not both) will be updated,
depending on whether this is an instance of dynamic initialization.  *err_pos
indicates the source position for diagnostics.  The function returns TRUE
unless there were errors in the scan (other than those reporting the
detection of uninitialized fields).
*/
{
  an_aggregate_init_info  init_info;
  a_boolean               nothing_taken, any_dynamic_init;
  a_boolean               err = FALSE;
  a_routine_ptr           dtor_rp = NULL;

  db_enter(3, "scan_initializer_list");
  /* Scan the initializer list. */
#if DEBUG
  if (debug_level == 4) {
    fputs("scanning initializer list for variable \"", f_debug);
    if (vp == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_name(&vp->source_corresp);
    }  /* if */
    fputs("\", type = ", f_debug);
    db_abbreviated_type(*type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  initialize_init_info(&init_info, static_lifetime, dps);
  *init_con = get_initializer(type, &init_info,
                              (an_aggregate_init_context_ptr)NULL,
                              &nothing_taken, &any_dynamic_init);
  if ((*init_con)->kind == (a_constant_repr_kind)ck_error) {
    err = TRUE;
  } else {
    a_type_ptr  tp = *type;
    if (is_array_type(*type)) tp = underlying_array_element_type(tp);
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp)) {
      dtor_rp = select_destructor(tp, tp, err_pos);
      if (dtor_rp != NULL) any_dynamic_init = TRUE;
    }  /* if */
    if (any_dynamic_init) {
      check_assertion((*init_con)->kind == (a_constant_repr_kind)ck_aggregate);
      *init_dip = alloc_dynamic_init(
                         (a_dynamic_init_kind)dik_nonconstant_aggregate);
      (*init_dip)->variant.constant = *init_con;
      (*init_dip)->destructor = dtor_rp;
      *init_con = NULL;
#if CHECKING
    } else {
      check_assertion((*init_con)->kind == (a_constant_repr_kind)ck_string ||
                      (*init_con)->kind == (a_constant_repr_kind)ck_aggregate);
#endif /* CHECKING */
    }  /* if */
    if (vp != NULL) {
      if (init_info.any_uninitialized_const_or_ref_member) {
        /* A const or ref field was not initialized. */
        if (is_union_type(*type)) {
          /* No diagnostic for unions. */
        } else {
          a_symbol_ptr  sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
          if (C_mode()) {
            pos_sy_warning(ec_var_with_uninitialized_field, err_pos, sym);
          } else {
            pos_sy_error(ec_var_with_uninitialized_member, err_pos, sym);
          }  /* if */
        }  /* if */
      }  /* if */
      /* If the initializer doesn't initialize every member of an
         aggregate, make note that the variable is only partially initialized.
         In cases where designated initializers have been used, we don't know
         if the aggregate is partially initialized or not so to be safe
         assume it is.  During lowering if it is discovered that the
         aggregate is indeed fully initialized, this field will be
         set appropriately. */
      if ((init_info.any_uninitialized_member ||
           init_info.uses_designated_initializers)) {
        vp->is_partially_initialized = TRUE;
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
      /* Record whether an initializer for a flexible array member was seen. */
      vp->has_flexible_array_initializer =
                                     init_info.has_flexible_array_initializer;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->var_init_range.end = init_info.init_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
  return !err;
}  /* scan_initializer_list */


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
       position.  Otherwise, use the position of the variable declaration. */
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
      stmt_end_pos = &vp->source_corresp.decl_pos_info->identifier_range.end;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
    init_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_init,
                                          stmt_pos);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    set_stmt_source_position(init_stmt->end_position, *stmt_end_pos);
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

  db_enter(5, "put_type_back_into_variable");
  check_assertion(is_array_type(vp->type) && is_incomplete_type(vp->type));
  /* See if the variable has linkage. */
  if (symbol_ptr->kind == (a_symbol_kind)sk_variable && linkage != idl_none) {
    /* The type of a variable with linkage has been adjusted because it
       is an incomplete array that has been initialized.  Check that the
       new type is compatible with other declarations of the variable.
       This is necessary for cases like
         main () {extern char a[5];}
         char a[] = "abc";  <-- Error; int [3] is incompatible with int [5].
    */
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
  if (is_incomplete_type(vp_type)) {
    pos_error(ec_bad_initializer_for_array_with_unspecified_bound, source_pos);
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
  if (err || local_static_var_init == NULL ||
      local_static_var_init->init_kind != (an_init_kind)initk_dynamic ||
      !dynamic_init_has_side_effects(local_static_var_init->
                                               initializer.dynamic,
                                     &suppress_warning)) {
    /* No object lifetime is needed if this is not a dynamic initialization,
       or if it is represented as a dynamic initialization but the initializer
       has no side effects (e.g., is a constant). */
    mark_object_lifetime_as_useless(local_static_lifetime);
  } else {
    bind_object_lifetime(local_static_lifetime,
                         (an_il_entry_kind)
                             iek_local_static_variable_init,
                         (char *)local_static_var_init);
  }  /* if */
  (void)pop_object_lifetime();
}  /* pop_object_lifetime_for_local_static_init */


static a_constant_ptr simple_initializer(a_decl_parse_state    *dps,
                                         a_boolean             static_lifetime,
                                         a_type_ptr            vp_type,
                                         a_dynamic_init_ptr    *init_dip,
                                         a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a simple nonaggregate, nonparenthesized initializer for a declaration
described by *dps.  static_lifetime is TRUE is the entity being initialized
has static storage duration; vp_type is the type of that entity.
*/
{
  a_constant_ptr constant; /* result of this function */
  a_boolean      brace_flag = FALSE, nonconstant_allowed;

  check_assertion(dps != NULL);
  if (!anything_cached(&dps->prescanned_initializer_cache)) {
    check_for_opening_brace(&brace_flag);
  }  /* if */
  nonconstant_allowed = (!C_mode() || !static_lifetime);
  /* Scan the initializer.  Either a constant pointer is returned or else
     a dynamic init entry representing an expression. */
  constant =
          scan_initializer_of_simple_object(dps,
                                            (an_aggregate_init_info *)NULL,
                                            (an_aggregate_init_context *)NULL,
                                            nonconstant_allowed,
                                            static_lifetime,
                                            /*force_object_lifetime=*/FALSE,
                                            /*suppress_object_lifetime=*/FALSE,
                                            /*is_copy_initialization=*/TRUE,
                                            &vp_type, init_dip);
  if (microsoft_bugs && microsoft_version < 1310) {
    /* Earlier microsoft compilers accept things like "int x = { f(), { 3 } }".
       The last value replaces previous ones (though side-effects take place),
       unless they're both constants and x is not automatic. */
    while (curr_token_if_nothing_cached_is(tok_comma, dps) &&
           next_token() == tok_lbrace) {
      /* Eat the comma, parse the next constant (recursive) and combine
         initializer expressions: */
      a_constant_ptr     next_constant;
      a_dynamic_init_ptr next_dip = NULL;
      (void)get_token();
      next_constant = simple_initializer(dps, static_lifetime, vp_type,
                                         &next_dip, decl_pos_block);
      if (static_lifetime && constant != NULL && next_constant != NULL) {
        /* Approximately emulate the Microsoft behavior that if only true
           constants are involved, the first value is kept for variables
           with static lifetime.  The emulation is not perfect when more
           nesting is involved as in "int x = { f(), { 1, { 2 }}};". */
      } else {
        combine_initializers(constant, *init_dip, next_constant, next_dip);
        check_assertion(next_dip != NULL || next_constant != NULL);
        /* If the combined initializer is non-constant, keep using the
           dynamic-initializer representation. */
        if (next_dip != NULL) {
          *init_dip = next_dip;
          constant = NULL;
        } else if (next_constant->kind ==
                                       (a_constant_repr_kind)ck_dynamic_init) {
          *init_dip = next_constant->variant.dynamic_init;
          constant = NULL;
        } else {
          *init_dip = NULL;
          constant = next_constant;
        }  /* if */
      }  /* if */
    }  /* while */
  }  /* if */
  /* If an extra opening brace was ignored earlier, ignore the matching
     closing brace now.  Check also for an extra comma (required in C++
     per ARM 8.4, offered in C along with the extension that permits
     brace-enclosed initializers on non-aggregate variables in the first
     place). */
  if (brace_flag && curr_token == tok_comma) (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    if (brace_flag) {
      decl_pos_block->var_init_range.end = pos_curr_token;
    } else {
      decl_pos_block->var_init_range.end = curr_construct_end_position;
    }  /* if */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  check_for_matching_closing_brace(brace_flag);
  return constant;
}  /* simple_initializer */


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
  a_boolean                         is_parameter =
                                                 dps->is_old_style_param_decl;
  a_constant_ptr                    init_con = NULL;
  a_dynamic_init_ptr                init_dip = NULL;
  a_class_symbol_supplement_ptr     cssp = NULL;
  a_boolean                         nonconstant_allowed;
  a_memory_region_number            region_to_switch_back_to;
  an_object_lifetime_ptr            local_static_lifetime = NULL;
  a_local_static_variable_init_ptr  local_static_var_init = NULL;
  a_token_kind                      first_token;
  a_source_position                 pos_first_token;

  db_enter(3, "initializer");
  dps->has_initializer = TRUE;
  /* There are a number of tests to determine whether the variable can take
     an initializer.  If it cannot, set var_err; it will be checked later
     to decide whether to update the variable with information about the
     initialization. */
  var_err = FALSE;
  if (is_parameter) {
    /* Parameter declarations cannot contain an initializer.  (Declarations
       for which is_parameter is TRUE are old-style C parameter declarations.
       C++ default arguments, which look a bit like a parameter with an
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
      if (sun_mode && vp->is_member_constant &&
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
          pos_error(ec_incomplete_type_not_allowed, source_pos);
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
             template<> int A<double>::x = 37;   */
      check_assertion(symbol_ptr->is_error ||
                      !is_file_or_namespace_scope(
                                            &scope_stack[depth_scope_stack]));
    } else if (!is_template_dependent_context()) {
      push_class_reactivation_scope(sym_parent_class(symbol_ptr),
                                    /*extend_namespace=*/TRUE);
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
  /* If the initialization is invalid in some way, init_err will be set to
     TRUE.  It will be used to assure that the initialization bound to the
     variable will be an error constant (or a dynamic initializer pointing
     to an error constant. */
  init_err = FALSE;
  if (!C_mode() && is_real_class_type(vp_type)) {
    cssp = symbol_supplement_for_class(vp_type);
    if (curr_token == tok_lbrace && !cssp->is_class_aggregate) {
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
  if (dps->auto_type_specifier_seen && !dps->has_trailing_return_type &&
      !is_error_type(vp_type)) {
    /* An initializer for a variable declared with the "auto" type specifier.*/
    if (first_token == tok_lbrace) {
      error(ec_auto_brace_initialization_not_allowed);
      vp->type = vp_type = error_type();
    } else {
      prescan_initializer_for_auto_type_deduction(dps,
                                                  parenthesized_initializer);
      vp_type = dps->type;
    }  /* if */
    if (is_error_type(vp_type)) {
      cssp = NULL;
    }  /* if */
  }  /* if */
  dps->type = vp_type;
  /* Now process the initializer.  There are three cases:  parenthesized
     initializer (C++ only), brace-enclosed initializer list, and simple
     initializer.  These are handled in turn. */
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
        scan_dependent_type_parenthesized_initializer(dps, &init_dip);
      } else {
        scan_class_parenthesized_initializer(vp_type, vp_type, dps,
                                             &pos, /*fill_in_dtor=*/TRUE,
                                             &init_dip);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* If no dynamic init entry was created, there must have been an
         error. */
      if (init_dip == NULL) init_err = TRUE;
    } else {
      /* An entity with no constructor.  (If it's a C-style struct with no
         constructor, initialization with bitwise copy is allowed -- e.g.,
         S x, y(x) -- but typically it's an object of non-class type.) */
      add_stop_token(tok_rparen);
      /* Scan the initializer.  Either a constant pointer is returned or else
         a dynamic init entry representing an expression. */
      nonconstant_allowed = (!C_mode() || !static_lifetime);
      init_con =
          scan_initializer_of_simple_object(dps,
                                            (an_aggregate_init_info *)NULL,
                                            (an_aggregate_init_context *)NULL,
                                            nonconstant_allowed,
                                            static_lifetime,
                                            /*force_object_lifetime=*/FALSE,
                                            /*suppress_object_lifetime=*/FALSE,
                                            /*is_copy_initialization=*/FALSE,
                                            &vp_type, &init_dip);
      if (init_con != NULL && vp != NULL &&
          init_con->kind == (a_constant_repr_kind)ck_string &&
          is_incomplete_type(vp->type) && is_string_type(vp->type)) {
        /* Handle something like:
             char s[]("xx");
           Update the variable type to reflect the string size. */
        check_assertion(!is_incomplete_type(vp_type));
        put_type_back_into_variable(vp, symbol_ptr, source_pos, linkage,
                                    vp_type);
      }  /* if */
      /* The closing right paren will not have been consumed, as it is
         the arg list for a constructor call is scanned, so bypass it
         explicitly. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (first_token == tok_rparen && decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = pos_first_token;
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
  } else if (is_aggregate_or_union_type(vp_type) ||
             (first_token == tok_lbrace &&
              (is_error_type(vp_type) ||
#if GNU_VECTOR_TYPES_ALLOWED
               (gnu_mode && is_vector_type(vp_type)) ||
#endif /* GNU_VECTOR_TYPES_ALLOWED */
               is_template_param_type(vp_type)))) {
    /* Either a brace enclosed list of initializers or other aggregate
       initialization. */
    if (first_token != tok_lbrace && is_class_struct_union_type(vp_type) &&
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
      if (scan_class_initializer_expression(dps, &init_dip)) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->var_init_range.end = curr_construct_end_position;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      } else {
        /* No appropriate constructor was found.  Abort the initialization. */
        init_err = TRUE;
      }  /* if */
    } else if (gnu_mode && first_token != tok_lbrace && static_lifetime) {
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
          set_initialized_array_size(&array_type, num_elems);
          vp->type = array_type;
        } else if (is_array_type(array_type) && 
                   !has_unknown_specified_bound(array_type) &&
                   init_con->kind == (a_constant_repr_kind)ck_string) {
          /* Flag the variable as partially initialized if the string
             contains fewer elements than the array. */
          vp->is_partially_initialized = 
                         array_type->variant.array.variant.number_of_elements >
                                               init_con->variant.string.length;
        }  /* if */
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    } else {
      /* Ordinary C-style aggregate initialization, usually with a brace-
         enclosed list of values.  Except that in C++ such lists may include
         non-constants. */
      if (scan_initializer_list(dps, &vp_type, vp, static_lifetime, &init_con,
                                &init_dip, source_pos, decl_pos_block)) {
        /* The scan was successful. */
        if (!var_err && vp != NULL &&
            is_incomplete_type(vp->type) && is_array_type(vp->type)) {
          /* An array variable with unspecified bound is dimensioned
             according to its initializer: Copy the type back into the
             variable. */
          put_type_back_into_variable(vp, symbol_ptr, source_pos, linkage,
                                      vp_type);
        }  /* if */
      } else {
        /* Errors were encountered (and reported) during the scan. */
        init_err =  TRUE;
        if (is_incomplete_type(vp_type) && is_array_type(vp_type)) {
          /* Initialization of an incomplete array failed and some appropriate
             error has been reported.  Suppress further errors on this failed
             initialization. */
          *incomplete_type_error_reported = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* A non-aggregate object is being initialized.  Braces are permitted
       but not required.  A constant or non-constant expression may be
       permitted as the initializer. */
    init_con = simple_initializer(dps, static_lifetime, vp_type, &init_dip,
                                  decl_pos_block);
  }  /* if */
  /* Verify that any prescanned operand was consumed. */
  check_assertion(!anything_cached(&dps->prescanned_initializer_cache) ||
                  is_error_type(dps->type));
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      vp->initializer_range = decl_pos_block->var_init_range;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (symbol_ptr->is_class_member) {
#if NEED_NAME_MANGLING
    if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
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
       done elsewhere).  Restore the scope to what it was before.  (We may
       also end up here with an sk_variable.) */
    /* Note that this call has to be after the select_destructor call in the
       preceding section of code. */
    if (is_incomplete_type(sym_parent_class(symbol_ptr))) {
      check_assertion(symbol_ptr->is_error);
    } else if (!is_template_dependent_context()) {
      pop_class_reactivation_scope();
    }  /* if */
  } else {
    /* If an object lifetime was pushed to surround the initialization of
       a local static variable, pop it now. */
    if (local_static_lifetime != NULL) {
        pop_object_lifetime_for_local_static_init(local_static_lifetime,
                                                  local_static_var_init,
                                                  init_err);
    }  /* if */
    if (sym_is_namespace_member(symbol_ptr)) {
      pop_namespace_reactivation_scope();
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_init")) {
    if (!var_err) {
      db_variable(vp);
      fputs(",\n", f_debug);
      db_initializer(vp, 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* initializer */


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
  a_constant_ptr           aggr_con, repeat_con, dynamic_init_con;

  /* The IL structure is
       new dynamic init new_dip (dik_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init ctor_dip (dik_constructor)
  */
  /* Create a ck_aggregate constant. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->type = array_type;
  new_dip->variant.constant = aggr_con;
  /* Set it to point to a newly created ck_init_repeat constant. */
  aggr_con->variant.aggregate.first_constant =
    aggr_con->variant.aggregate.last_constant =
    repeat_con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
  /* Set the ck_init_repeat constant fields, including a pointer to a new
     ck_dynamic_init constant. */
  repeat_con->variant.init_repeat.count = count;
  repeat_con->variant.init_repeat.constant = dynamic_init_con =
    alloc_constant((a_constant_repr_kind)ck_dynamic_init);
  /* Set the ck_dynamic_init_constant to point to the dynamic init entry
     representing the constructor call. */
  dynamic_init_con->variant.dynamic_init = ctor_dip;
  dynamic_init_con->type = elem_type;
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
  a_dynamic_init_ptr                init_dip, orig_init_dip;
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
    /* Default initialization is done only for non-POD class objects that
       are defined in the current translation unit (i.e., storage class
       other than "extern"). */
    /* We don't test just is_POD because we want to catch cases where there
       is a user-declared defaulted constructor or destructor that's not
       accessible. */
    if (cssp != NULL &&
        (!cssp->is_POD ||
         cssp->constructor != NULL || cssp->destructor != NULL) &&
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
           nontrivial constructors.  Look for a default constructor. */
        a_boolean err;
        ctor = select_default_constructor(tp, err_pos, tp, &err);
        if (err) {
          /* Some error, already diagnosed, e.g., no default constructor. */
        } else if (is_const &&
                   (ctor == NULL || ctor->compiler_generated)) {
          /* A default constructor was found, but it isn't a user-provided
             constructor, which is required for a const-qualified variable. */
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
          if (reference_to_trivial_default_constructor(tp, err_pos,
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
              add_dtor_for_partially_constructed_aggregate(dtor,
                                                           orig_init_dip);
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
        /* Allocate a dynamic init entry (a copy of local_di) and attach it
           to the variable. */
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
  }  /* if */
  db_exit();
  return def_init_performed;
}  /* def_initializer */


static void add_as_child_of_curr_object_lifetime(an_object_lifetime_ptr  olp)
/*
Restore *olp to the object lifetime tree -- it was detached earlier, but is
added back, but possibly in a different position on the child_lifetime list
of curr_object_lifetime (which is assumed to be its former parent).
*/
{
  db_enter(4, "add_as_child_of_curr_object_lifetime");
  if (olp != NULL) {
    a_dynamic_init_ptr outer_dip = olp->parent_destruction_sublist;
    if (outer_dip != NULL &&
        outer_dip->overlaps_temps_in_inner_lifetime &&
        outer_dip->lifetime_of_overlapping_temps == olp) {
      /* There was an associated destruction for a temporary whose lifetime
         was promoted (to bind it to a reference).  The destruction was
         removed by detach_object_lifetime_for_dynamic_init.  Put it back
         now at the right place in the list. */
      record_end_of_lifetime_destruction(outer_dip, /*static_lifetime=*/FALSE,
                                         /*block_lifetime=*/TRUE);
    }  /* if */
    check_assertion_str2(olp->parent_lifetime == NULL,
                         "add_as_child_of_curr_object_lifetime:",
                         "non-NULL parent_lifetime");
    olp->next = curr_object_lifetime->child_lifetime;
    curr_object_lifetime->child_lifetime = olp;
    olp->parent_lifetime = curr_object_lifetime;
    olp->parent_destruction_sublist = curr_object_lifetime->destructions;
#if DEBUG
    if (debug_level >= 4) {
      fputs("after restoration:\n", f_debug);
      db_object_lifetime(olp);
      db_object_lifetime(curr_object_lifetime);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  db_exit();
}  /* add_as_child_of_curr_object_lifetime */



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
  a_dynamic_init_ptr     outer_dip;

  if (olp != NULL) {
    detach_from_object_lifetime_tree(olp);
    outer_dip = olp->parent_destruction_sublist;
    if (outer_dip != NULL &&
        outer_dip->overlaps_temps_in_inner_lifetime &&
        outer_dip->lifetime_of_overlapping_temps == olp) {
      remove_from_destruction_list(outer_dip);
    }  /* if */
  }  /* if */
}  /* detach_object_lifetime_for_dynamic_init */


static a_boolean are_disjoint_members_of_union(a_field_ptr field1,
                                               a_field_ptr field2)
/*
Return TRUE if the two indicated fields (ultimately, members of the
same class) are members of the same union, or members of members of
the same union.  Anonymous unions and structs are considered in the
determination.
*/
{
  a_boolean  are_disjoint_members = FALSE;
  a_type_ptr class1 = parent_class_of(field1);

  /* Work up from each field looking at the parent classes.  Find the
     innermost class/struct/union that the fields have in common. */
  for (;;) {
    a_type_ptr class2 = parent_class_of(field2);
    for (;;) {
      if (same_entities(class2, class1)) {
        /* We've found the innermost class/struct/union that the
           fields have in common.  If it is a union, they conflict. */
        are_disjoint_members = (class1->kind == (a_type_kind)tk_union);
        goto end_of_routine;
      }  /* if */
      if (!class2->source_corresp.is_class_member) break;
      class2 = parent_class_of(class2);
    }  /* for */
    check_assertion(class1->source_corresp.is_class_member);
    class1 = parent_class_of(class1);
  }  /* for */
end_of_routine:
  return are_disjoint_members;
}  /* are_disjoint_members_of_union */


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


static void check_out_of_order_init(a_constructor_init_ptr  new_cip,
                                    a_constructor_init_ptr  *p_prev_cip,
                                    a_boolean               *diag_issued)
/*
new_cip represents a new constructor initializer and *p_prev_cip represents
the previous initializer for the same constructor definition (or NULL, if
there was no previous initializer).  If *diag_issued is FALSE, issue a remark
if the new initializer will occur before the previous initializer, and set
*diag_issued to TRUE.  In all cases, set *p_prev_cip to new_cip.
*/
{
  if (!*diag_issued) {
    a_boolean  is_out_of_order = FALSE;
    if (*p_prev_cip == NULL) {
      /* There was no previous initializer, so there cannot be an out-of-order
         item yet. */
    } else if (new_cip->kind < (*p_prev_cip)->kind) {
      /* The previous item was of a kind that is initialized after the current
         item. */
      is_out_of_order = TRUE;
    } else if (new_cip->kind == (*p_prev_cip)->kind) {
      /* The previous item was of the same kind as the current item, so they
         should both be on the same list.  See if the new item comes after it
         on the initialization-order list (which would mean the order of the
         two initializers matches the order in which the corresponding
         initializations are done). */
      a_constructor_init_ptr  cip = *p_prev_cip;
      for (; cip != NULL; cip = cip->next) {
        if (cip == new_cip) break;
      }  /* if */
      /* If new_cip was not found after *p_prev_cip, presumably it came before
         *p_prev_cip, and an out-of-order diagnostic should be issued. */
      is_out_of_order = (cip == NULL);
    }  /* if */
    if (is_out_of_order) {
      pos_sy2_diagnostic(es_remark, ec_out_of_order_ctor_init, &error_position,
                         ctor_init_symbol(new_cip),
                         ctor_init_symbol(*p_prev_cip));
      *diag_issued = TRUE;
    }  /* if */
  }  /* if */
  *p_prev_cip = new_cip;
}  /* check_out_of_order_init */


a_constructor_init_ptr ctor_initializer(a_routine_ptr  ctor_rout,
                                        a_boolean      user_defined)
/*
Process the explicit and implicit constructor initializations for constructor
routine ctor_rout.  If user_defined is TRUE, the context is that of an
explicit definition in the source code; otherwise, this routine is called
as part of the implicit definition of a compiler generated constructor.

When user_defined is TRUE, the explicit initializations are scanned from the
source, based on the following syntax:

    ctor-initializer
              ":" mem-initializer-list
    mem-initializer
              complete-class-name "(" expression-list    ")"
                                                     opt
              identifier "(" expression-list    ")"
                                            opt

complete-class-name identifies a base class from which the class to which
the constructor belongs is derived, in which case the initializer list entry
means "invoke the constructor X::X with the (possibly null) actual arguments
given by expression-list".  identifier represents a nonstatic data member of
the current class, and expression-list contains the value(s) with which it
is to be initialized.

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
  a_boolean                     is_generated_cctor;
  a_type_qualifier_set          required_qualifiers, object_qualifiers;
  a_type_ptr                    class_type, init_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym, member_or_base_sym;
  a_constructor_init_ptr        cip, new_cip, prev_cip, next_cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list, end_of_virtual_list;
  a_constructor_init_ptr        direct_list, end_of_direct_list;
  a_base_class_ptr              bcp;
  a_class_type_supplement_ptr   ctsp;
  a_class_symbol_supplement_ptr cssp;
  a_routine_ptr                 rp;
  a_dynamic_init_ptr            dip, ctor_dip;
  int                           direct_base_class_count = 0;
  a_source_position             lparen_pos;
  a_constructor_init_ptr        uninit_list = NULL, end_of_uninit_list = NULL;
  a_boolean                     any_ref_member_on_uninit_list = FALSE;

  db_enter(3, "ctor_initializer");
  class_type = parent_class_of(ctor_rout);
  check_assertion(class_type != NULL);
  ctsp = class_type->variant.class_struct_union.extra_info;
  is_generated_cctor = !user_defined &&
                       is_copy_constructor(ctor_rout, class_type,
                                           &required_qualifiers,
                                           rvalue_ctor_is_copy_ctor,
                                           /*is_declarative_context=*/TRUE);
  /* Move constructors are currently not generated. */
  check_assertion(!is_generated_cctor || !copy_ctor_is_move_ctor(ctor_rout));
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
  virtual_list = end_of_virtual_list = NULL;
  direct_list = end_of_direct_list = NULL;
  /* Scan the list of base classes, which may include some that are
     ineligible for initialization. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) ++direct_base_class_count;
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
        if (virtual_list == NULL) {
          /* Start a new list. */
          virtual_list = cip;
        } else {
          /* Add to end of list. */
          end_of_virtual_list->next = cip;
        }  /* if */
        end_of_virtual_list = cip;
      } else {
        if (direct_list == NULL) {
          /* Start a new list. */
          direct_list = cip;
        } else {
          /* Add to end of list. */
          end_of_direct_list->next = cip;
        }  /* if */
        end_of_direct_list = cip;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Move on to the third list -- the list of nonstatic data members requiring
     initialization. */
  cip_list = end_of_cip_list = NULL;
  /* Loop through the symbol list for the class, not the field list, since
     the symbol list contains only user-defined fields whereas the field
     list may also include compiler-generated field entries. */
  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  for (sym = class_sym->variant.class_struct_union.extra_info->symbols;
       sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* sym represents a field.  Determine whether constructor initialization
         is required. */
      a_field_ptr field = sym->variant.field.ptr;
      if (microsoft_mode && field_is_nontrivial_property(field)) {
        /* Nontrivial property fields are not really data members and should
           not be initialized. */
        continue;
      } else if (is_generated_cctor) {
        /* All fields are explicitly listed for a generated copy constructor,
           since even if there is no constructor at least a bitwise copy is
           required. */
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
      if (cip_list == NULL) {
        cip_list = cip;
      } else {
        end_of_cip_list->next = cip;
      }  /* if */
      end_of_cip_list = cip;
    }  /* if */
  }  /* for */
  /* Three lists that have been created thus far were made to cover the
     default (or value) initialization required because base classes and
     fields need it.  It remains to scan the user specified initializers,
     if any, and to integrate them into the lists. */
  if (user_defined && curr_token == tok_colon) {
    /* User-specified initializers are present. */
    a_boolean               out_of_order_diag_issued = FALSE;
    a_constructor_init_ptr  prev_init = NULL;
    /* Bypass the colon. */
    (void)get_token();
    add_stop_token(tok_lbrace);
    /* Loop through the comma-separated list of initializers. */
    do {
      a_boolean          template_param_init = FALSE;
      a_boolean          dependent_class_init = FALSE;
      a_boolean          flexible_array_member = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_source_position  init_start_pos;

      init_start_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      new_cip = NULL;
      array_type = NULL;
      add_stop_token(tok_comma);
      /* Unless this is an old style base class initializer, a base class
         name or a member name is expected. */
      if (curr_token != tok_lparen && !is_decl_qualified_name_start()) {
        /* Either an identifier or "::" is expected here. */
        syntax_error(ec_exp_identifier);
      } else {
        bcp = NULL;
        dip = NULL;
        if (curr_token == tok_lparen) {
          /* Old-style base class initializer.  It is assumed to apply to the
             the direct base class (further assuming that there is exactly
             one direct base class). */
          if (!allow_anachronisms || direct_base_class_count != 1) {
            /* Either no base classes or more than one. */
            error(ec_missing_base_class_or_member_name);
            init_type = error_type();
          } else {
            /* The base class is probably on the direct_list, but if it was
               declared virtual it is on the virtual list. */
            new_cip = (direct_list != NULL) ? direct_list : virtual_list;
            new_cip->compiler_generated = FALSE;
            bcp = new_cip->variant.base_class;
            check_assertion(bcp->direct);
            init_type = bcp->type;
            type_diagnostic(anachronism_error_severity,
                            ec_base_class_init_anachronism, init_type);
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, init_type);
            } else {
              check_out_of_order_init(new_cip, &prev_init,
                                      &out_of_order_diag_issued);
            }  /* if */
          }  /* if */
          /* Back up so that the left paren will be rescanned. */
          unget_token();
          goto scan_paren;
        }  /* if */
        /* Scan the base class name or member name.  The lookup mode
	   ilm_ctor_initializer_name skips the current function scope
	   to ensure that a constructor parameter with the same name
	   as a member or base class is not visible. */
        {
          a_boolean gid_err;
          an_identifier_lookup_mode	ilm;
          an_identifier_options_set	gid_options = GID_NO_OPTIONS;
          /* A qualified name must name a class. */
          if (locator_for_curr_id.is_qualified_name) {
            gid_options |= GID_IMPLICIT_TYPE_CONTEXT;
            ilm = ilm_qualified_ctor_initializer_name;
          } else {
            ilm = ilm_ctor_initializer_name;
          }  /* if */
          member_or_base_sym = coalesce_and_lookup_generalized_identifier
                                   (gid_options, ilm, &gid_err);
          if (member_or_base_sym != NULL) {
            /* Check if a template-dependent entity is being initialized: */
            if (member_or_base_sym->kind == (a_symbol_kind)sk_field) {
              if (!member_or_base_sym->is_class_member) {
                /* This can happen in error cases with anonymous unions:
                     static union { int i; double j; };
                     struct S { S(): i(j) {} };
                   Avoid having to deal with non-member fields during error
                   recovery by dropping the result of the lookup.  */
                member_or_base_sym = NULL;
              } else {
                /* A mem-initializer for a field: */
                dependent_class_init = could_be_dependent_class_type(
                                 member_or_base_sym->variant.field.ptr->type);
              }  /* if */
            } else if (is_type_symbol(member_or_base_sym)) {
              /* This is presumably a mem-initializer for a base. */
              a_type_ptr  type = type_symbol_type(member_or_base_sym);
              type = skip_typerefs(type);
              dependent_class_init = could_be_dependent_class_type(type);
              template_param_init =
                               (type->kind == (a_type_kind)tk_template_param);
            }  /* if */
          }  /* if */
          if ((!class_name_injection_enabled || microsoft_mode) &&
              !is_error_locator(locator_for_curr_id) &&
              !locator_for_curr_id.is_qualified_name) {
            /* If no symbol was returned from the lookup, or if the symbol
               returned was not a base class or member of the current class,
               see if the name (if it was unqualified) matches the name of a
               base class. This can be necessary in cases like this:
                 namespace N {
                   class A { A(int); ... };
                 }
                 class B : public N::A {
                   B() : A(0) { }
                 };
               The check that follows does not quite emulate the results of
               a lookup that supports class name injection (e.g., it doesn't
               deal properly with hiding within the inheritance hierarchy),
               but the differences will be manifested as slightly different
               diagnostics, and then only in rather obscure cases.

               This check is done in Microsoft mode even though class name
               injection is enabled because, in Microsoft mode, the injected
               name is ignored for most lookups.
            */
            a_boolean     check_base_classes;

            if (member_or_base_sym == NULL) {
              check_base_classes = TRUE;
            } else if (is_class_symbol(member_or_base_sym) &&
                       find_base_class_of(class_type,
                                          type_symbol_type(
                                                 member_or_base_sym))) {
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
              for (bcp = base_classes_of(class_type);
                   bcp != NULL;
                   bcp = bcp->next) {
                if (bcp->direct || bcp->is_virtual ||
                    member_or_base_sym == NULL) {
                  a_symbol_ptr  tmp_sym = (a_symbol_ptr)bcp->type->
                                                 source_corresp.assoc_info;
                  if (locator_for_curr_id.symbol_header == tmp_sym->header) {
                    member_or_base_sym = tmp_sym;
                    break;
                  }  /* if */
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* if */
        }
        if (member_or_base_sym == NULL ||
            member_or_base_sym->kind == (a_symbol_kind)sk_undefined) {
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
        if (member_or_base_sym->kind == (a_symbol_kind)sk_field &&
            same_entities(sym_parent_class(member_or_base_sym), class_type)) {
          /* This is a field of the current class and may be mentioned in the
             constructor's initializer list.  But it's an error to refer to
             it by a qualified name. */
          a_field_ptr field = member_or_base_sym->variant.field.ptr;
          if (locator_for_curr_id.is_qualified_name) {
            pos_error(ec_qualified_name_not_allowed,
                      &locator_for_curr_id.source_position);
          } else if (microsoft_mode && field_is_nontrivial_property(field)) {
            /* Property fields cannot be mentioned in a constructor
               initializer list. */
            pos_error(ec_property_name_not_allowed,
                      &locator_for_curr_id.source_position);
          }  /* if */
          init_type = field->type;
          if (is_array_type(init_type)) {
            flexible_array_member = is_incomplete_type(init_type);
            if (!is_string_type(init_type)) {
              /* Arrays can be default-initialized if the expression-list is
                 omitted. */
              array_type = init_type;
              init_type = f_skip_typerefs(
                                    underlying_array_element_type(init_type));
            }  /* if */
          }  /* if */
          /* Only one member of a union or an anonymous union subobject is
             allowed to appear in the ctor-initializer list. */
          if (is_union_type(class_type) ||
              member_or_base_sym->
                         variant.field.anonymous_parent_object != NULL) {
            /* Check through fields for which initializers have already been
               specified. */
            for (cip = cip_list; cip != NULL; cip = cip->next) {
              if (cip->initializer != NULL) {
                /* Note: at this point cip_list includes only fields, so we can
                   assume cip->kind is cik_field. */
                if (cip->variant.field == field) {
                  /* Error on duplicate initialization will be issued below. */
                } else if (!microsoft_mode &&
                           are_disjoint_members_of_union(cip->variant.field,
                                                         field)) {
                  /* The union (or the anonymous union subobject) has already
                     been initialized. */
                  error(ec_union_already_initialized);
                }  /* if */
              }  /* if */
            }  /* for */
          }  /* if */
          /* Check the list for a constructor init entry that refers to this
             member.  If it's there we may have a reinitialization error. */
          for (new_cip = cip_list; new_cip != NULL; new_cip = new_cip->next) {
            /* Note: at this point cip_list includes only fields, so we can
               assume new_cip->kind is cik_field. */
            if (new_cip->variant.field ==
                                   member_or_base_sym->variant.field.ptr) {
              if (new_cip->initializer != NULL) {
                sym_error(ec_member_already_initialized, member_or_base_sym);
                goto scan_paren;
              }  /* if */
              break;
            }  /* if */
          }  /* for */
          if (new_cip != NULL) {
            /* Already on the list and presumably marked as compiler-generated.
               Reset the flag, now that it's appeared explicitly in the ctor-
               initializer list. */
            new_cip->compiler_generated = FALSE;
          } else {
            /* No constructor init entry exists for this field.  Allocate one
               and add it to the list. */
            new_cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
            new_cip->variant.field = member_or_base_sym->variant.field.ptr;
            new_cip->compiler_generated = FALSE;
            if (cip_list == NULL) {
              /* Easy case:  start a new list. */
              cip_list = end_of_cip_list = new_cip;
            } else {
              /* The order in which fields appear on the constructor init list
                 must correspond exactly to the order in which they were
                 declared.  This order is preserved in the symbol list for the
                 class, so advance through the symbol list and through whatever
                 is already on the constructor init list together. */
              prev_cip = NULL;
              cip = cip_list;
              sym = class_sym->variant.class_struct_union.extra_info->symbols;
              for (; sym != NULL; sym = sym->next_in_scope) {
                if (sym->kind == (a_symbol_kind)sk_field) {
                  /* Found a nonstatic data member. */
                  if (sym == member_or_base_sym) {
                    /* Found the field.  Insert new_cip into cip_list
                       immediately following prev_cip.  If prev_cip is NULL
                       this will be at the head of the list. */
                    if (prev_cip == NULL) {
                      /* Insert at head of list. */
                      new_cip->next = cip_list;
                      cip_list = new_cip;
                    } else {
                      /* Insert into the list. */
                      new_cip->next = prev_cip->next;
                      prev_cip->next = new_cip;
                    }  /* if */
                    break;
                  } else if (sym->variant.field.ptr == cip->variant.field) {
                    /* We didn't find the field we're trying to insert, but
                       we did find the next item on the list. */
                    if (cip == end_of_cip_list) {
                      /* Since this is the end of the list, we know the new
                         field must appear after the current entry.  Cut short
                         the search. */
                      end_of_cip_list->next = new_cip;
                      end_of_cip_list = new_cip;
                      break;
                    }  /* if */
                    /* Advance through the cip list, saving the current entry
                       as a possible insertion point. */
                    prev_cip = cip;
                    cip = cip->next;
                  }  /* if */
                }  /* if */
              }  /* for */
            }  /* if */
          }  /* if */
          /* At this point new_cip should point to the field's constructor init
             entry to which the initializer should be attached.  It has been
             located in or inserted into the list of such entries at a spot
             corresponding to its declaration order. */
          check_out_of_order_init(new_cip, &prev_init,
                                  &out_of_order_diag_issued);
        } else if (is_class_symbol(member_or_base_sym) ||
                   template_param_init) {
          /* It is a base class of the current class for which initialization
             is to be done.  (In a prototype instantiation, this could look
             like the initialization of a template parameter.) */
          a_boolean  indirect_nonvirtual_base_class_found = FALSE;
          if (locator_for_curr_id.is_semivisible_nested_type) {
            /* The symbol in the locator is a nested class that is not
               visible according to the ARM lookup rules but is returned
               in support of the nested class anachronism (ARM 18.3.5).
               Issue an anachronism diagnostic. */
            sym_diagnostic(anachronism_error_severity,
                           ec_nested_class_anachronism,
                           locator_for_curr_id.specific_symbol);
          }  /* if */
          init_type = type_symbol_type(member_or_base_sym);
          init_type = skip_typerefs(init_type);
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
            bcp = ctsp->base_classes;
            for (; bcp != NULL; bcp = bcp->next) {
              if (same_entities(bcp->type, init_type)) {
                if (bcp->direct || bcp->is_virtual) {
                  if (found_bcp == NULL) {
                    found_bcp = bcp;
                  } else {
                    /* This condition occurs when there is a direct nonvirtual
                       base class with the same name as an indirect virtual
                       base class. */
                    pos_ty_error(ec_ambiguous_base_class, &error_position,
                                 bcp->type);
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
            if ((!member_or_base_sym->is_template_param &&
                 template_param_init) ||
                (class_type->variant.class_struct_union.is_nonreal_class &&
                 symbol_supplement_for_class(class_type)->
                                                  any_nonreal_base_classes)) {
              /* There are some cases where we cannot match up a base:
                 - A dependent reference to a base, but not a template
                   parameter itself (presumably, a dependent qualified name).
                 - A reference to a class type that might be a dependent base
                   or a virtual base class thereof in some instantiation.
                 For these cases, we make up a nonvirtual base class node.
                 We also call complete_class_type_is_needed for that
                 presumed base class so that it will be instantiated if
                 necessary: base classes must be complete, and we will also
                 need to know if it has a constructor. */
              complete_class_type_is_needed(init_type);
              new_cip = alloc_ctor_init(
                              (a_constructor_init_kind)cik_direct_base_class);
              new_cip->variant.base_class = alloc_base_class();
              new_cip->variant.base_class->type = init_type;
              if (direct_list == NULL) {
                /* Start a new list. */
                direct_list = new_cip;
              } else {
                /* Add to end of list. */
                end_of_direct_list->next = new_cip;
              }  /* if */
              end_of_direct_list = new_cip;
            } else {
              /* No match found. */
              if (indirect_nonvirtual_base_class_found) {
                /* Actually, a match was found, but it was not a direct or
                   virtual base class. */
                error(ec_indirect_nonvirtual_base_class_not_allowed);
              } else {
                /* Not a base class of the class for which a constructor is
                   being defined. */
                pos_stty_error(ec_not_a_field_or_base_class, &error_position,
                               member_or_base_sym->header->identifier,
                               class_type);
              }  /* if */
              init_type = error_type();
            }  /* if */
          } else {
            /* The base class was found.  Now look on the appropriate list of
               constructor initializers. */
            new_cip = (bcp->is_virtual) ? virtual_list : direct_list;
            for (; new_cip != NULL; new_cip = new_cip->next) {
              if (new_cip->variant.base_class == bcp) break;
            }  /* for */
            check_assertion(new_cip != NULL);
            /* new_cip was initially marked as compiler-generated. Reset the
               flag now that it's appeared explicitly in the ctor-initializer
               list. */
            new_cip->compiler_generated = FALSE;
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, bcp->type);
            } else {
              check_out_of_order_init(new_cip, &prev_init,
                                      &out_of_order_diag_issued);
            }  /* if */
          }  /* if */
        } else {
          /* Not a base class, not a field.  Issue an error. */
          pos_stty_error(ec_not_a_field_or_base_class, &error_position,
                         member_or_base_sym->header->identifier, class_type);
          init_type = error_type();
        }  /* if */
scan_paren:
        /* Advance past the identifier. */
        (void)get_token();
        copy_source_position(pos_curr_token, lparen_pos);
        if (required_token(tok_lparen, ec_exp_lparen)) {
          if (is_class_struct_union_type(init_type) &&
              (array_type == NULL || curr_token == tok_rparen)) {
            /* The type of the base or member is class or array-of-class --
               the latter only if the expression-list is empty. */
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
            if (dependent_class_init) {
              scan_dependent_type_parenthesized_initializer(
                                             (a_decl_parse_state*)NULL, &dip);
            } else {
              a_type_ptr  object_class_type;
              /* If it is a base class, the object being constructed is the
                 whole class (and the base class is a subobject thereof).
                 If it is a field, the object being constructed is field
                 itself.  Set the object class type accordingly. */
              check_assertion(new_cip != NULL);
              if (new_cip->kind == (a_constructor_init_kind)cik_field) {
                object_class_type = init_type;
              } else {
                object_class_type = class_type;
              }  /* if */
              /* This is treated like an initialization of the form
                 S x (arg [, ...]), where S is a class type name.  Depending
                 on the arguments present, a constructor will be selected and
                 returned.  The scan function returns dip set to NULL if it
                 finds no constructor for which the arguments match. */
              scan_class_parenthesized_initializer(
                                           init_type, object_class_type,
                                           (a_decl_parse_state*)NULL,
                                           &lparen_pos,
                                           /*fill_in_dtor=*/exceptions_enabled,
                                           &dip);
            }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
            if (new_cip != NULL) {
              new_cip->ctor_init_range.start = init_start_pos;
              new_cip->ctor_init_range.end = curr_construct_end_position;
            }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            if (dip == NULL) {
              /* Create a fake initializer to represent the error. */
              a_constant_ptr  cp;
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
              cp = alloc_constant((a_constant_repr_kind)ck_error);
              set_error_constant(cp);
              dip->variant.constant = cp;
            } else {
              /* If the initializer produced an object lifetime for the full
                 expression, remove it temporarily from the object lifetime
                 tree and restore it in the correct position later. */
              detach_object_lifetime_for_dynamic_init(dip);
              /* If this is the initialization of an array, the dynamic init
                 entry at this point represents the initialization of an
                 element of the array, not of the array as a whole.  The
                 remaining processing is done later, along with members of
                 array type that are not explicitly specified in the
                 mem-initializer list. */
#if CHECKING
              if (array_type != NULL) {
                check_assertion(dip->kind ==
                                  (a_dynamic_init_kind)dik_constructor);
              }  /* if */
#endif /* CHECKING */
            }  /* if */
          } else if (curr_token == tok_rparen && cssp != NULL &&
                     reference_to_trivial_default_constructor(
                                         init_type, &error_position,
                                         /*check_access=*/TRUE,
                                         (a_boolean *)NULL)) {
            /* We fake a call to the trivial default constructor for the
               class.  No call is actually made, but the constructor
               definition is triggered (in case there are side-effects).
               Note that this is a so-called "value-initialization" case
               and hence the object must be zeroed. */
            a_dynamic_init_kind init_kind = (a_dynamic_init_kind)dik_zero;
            if (!value_initialization_enabled ||
                (gpp_mode &&
                 emulate_gnu_value_initialization_bugs)) {
              init_kind = (a_dynamic_init_kind)dik_none;
            }  /* if */
            dip = alloc_dynamic_init(init_kind);
#if EXTRA_SOURCE_POSITIONS_IN_IL
            if (new_cip != NULL) {
              new_cip->ctor_init_range.start = init_start_pos;
              new_cip->ctor_init_range.end = pos_curr_token;
            }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            /* Bypass the right paren. */
            (void)get_token();
          } else {
            /* A field whose initialization does not involve a constructor. */
            if (curr_token == tok_rparen) {
              if (is_any_reference_type(init_type)) {
                /* Error.  A reference type may not be default-initialized. */
                a_constant_ptr  cp;
#if MICROSOFT_EXTENSIONS_ALLOWED
                /* Fields cannot be tracking references. */
                check_assertion(!cppcli_enabled ||
                                !is_tracking_reference_type(tp));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                error(ec_default_init_of_reference);
                /* Create a fake initializer to represent the error. */
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
                cp = alloc_constant((a_constant_repr_kind)ck_error);
                set_error_constant(cp);
                dip->variant.constant = cp;
              } else {
                /* Using "()" with the mem-initializer means, perform value
                   initialization.  Note that the class and array-of-class
                   cases have already been dealt with, so value initialization
                   is tantamount to zero-initialization (8.5 [dcl.init]). */
                a_dynamic_init_kind init_kind = (a_dynamic_init_kind)dik_zero;
                if ((microsoft_bugs && microsoft_version < 1310 &&
                     emulate_msvc_value_initialization_bugs) ||
                    (gpp_mode && emulate_gnu_value_initialization_bugs &&
                     new_cip != NULL &&
                     new_cip->kind != (a_constructor_init_kind)cik_field) ||
                    flexible_array_member) {
                  /* MSVC++ up to version 7.0 never initializes the entity in
                     cases like this.  g++ up to 3.4 at least does not
                     initialize base classes.  The flexible array member case
                     cannot be initialized since the array has no known number
                     of elements. */
                  init_kind = (a_dynamic_init_kind)dik_none;
                }  /* if */
                dip = alloc_dynamic_init(init_kind);
              }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
              if (new_cip != NULL) {
                new_cip->ctor_init_range.start = init_start_pos;
                new_cip->ctor_init_range.end = pos_curr_token;
              }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
              /* Bypass the right paren. */
              (void)get_token();
            } else {
              /* Not default-initialization. */
              add_stop_token(tok_rparen);
              if (array_type != NULL) {
                /* Arrays can only be default- or value-initialized -- i.e.,
                   the expression-list must be omitted.  GNU C++, however, is
                   more permissive and allows initialization with an expression
                   of the same array type if the elements of the array have a
                   nontrivial copy constructor. */
                dip = scan_array_mem_initializer(new_cip);
              } else {
                /* Allocate a new dynamic init entry, setting the kind to
                   dik_none for now.  It will be adjusted after the scan. */
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
                (void)scan_initializer_of_simple_object(
                                            (a_decl_parse_state*)NULL,
                                            (an_aggregate_init_info *)NULL,
                                            (an_aggregate_init_context *)NULL,
                                            /*nonconst_allowed=*/TRUE,
                                            /*static_lifetime=*/FALSE,
                                            /*force_object_lifetime=*/TRUE,
                                            /*suppress_object_lifetime=*/FALSE,
                                            /*is_copy_initialization=*/FALSE,
                                            &init_type, &dip);
                /* If the initializer produced an object lifetime for the full
                   expression, remove it temporarily from the object lifetime
                   tree and restore it in the correct position later. */
                detach_object_lifetime_for_dynamic_init(dip);
              }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
              if (new_cip != NULL && curr_token == tok_rparen) {
                new_cip->ctor_init_range.start = init_start_pos;
                new_cip->ctor_init_range.end = pos_curr_token;
              }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
              if (!required_token(tok_rparen, ec_exp_rparen)) {
                /* Special code to avoid poor error recovery in cases where
                   a comma-list appears between the parens in what is taken
                   to be the initializer of a simple object -- e.g.,
                       A::A(int i, int j) : x(i,j) { }
                   If there is no constructor for x then it is interpreted
                   as a simple object, only "i" is scanned, and an error is
                   issued on the expected ")".  After that we want to bypass
                   the rest of the comma-list before resuming scanning. */
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
          if (new_cip != NULL) new_cip->initializer = dip;
        }  /* if */
      }  /* if */
      remove_stop_token(tok_comma);
    } while (loop_token(tok_comma));
    remove_stop_token(tok_lbrace);
  }  /* if */
  /* Merge the three lists into one:  virtual base classes followed by
     nonvirtual direct base classes followed by nonstatic data members
     (ARM 12.6.2). */
  if (direct_list != NULL) {
    end_of_direct_list->next = cip_list;
    cip_list = direct_list;
  }  /* if */
  if (virtual_list != NULL) {
    end_of_virtual_list->next = cip_list;
    cip_list = virtual_list;
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
           (a) special handling for generated copy constructor; or
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
  for (cip = cip_list; cip != NULL; cip = next_cip) {
    a_boolean          is_const_qualified;
    a_source_position  err_pos;
    /* object_class_type is the type of the object being created.
       For base classes it will be different than the type associated
       with the constructor being called.  For fields it will be
       the same as the field type.  This is needed to check protected
       member access. */
    a_type_ptr            object_class_type;

    next_cip = cip->next;
    dip = cip->initializer;
    /* If this was an explicit specialization, check whether an object
       lifetime needs to be restored to the IL. */
    if (dip != NULL && dip->kind != (a_dynamic_init_kind)dik_none) {
      /* If the initializer had produced an object lifetime for the full
         expression, it was temporarily removed from the object lifetime tree;
         Now that we are reconsidering the initializers in the canonical order
         (not the order in the source), restore the object lifetime. */
      an_object_lifetime_ptr  olp = init_expr_lifetime_of(cip->initializer);

      if (olp != NULL) {
        if (!long_lifetime_temps) {
          /* Add the lifetime back in as a child of the current object
             lifetime.  This assures that the order of the child-lifetime
             list will reflect the actual order of construction. */
          add_as_child_of_curr_object_lifetime(olp);
        } else {
          /* Promote destructions associated with expression temps to the
             function scope lifetime. */
          if (olp->destructions != NULL) {
            move_destruction_to_curr_object_lifetime(olp->destructions);
            olp->destructions = NULL;
          }  /* if */
          dip = cip->initializer;
          if (dip->kind == (a_dynamic_init_kind)dik_expression &&
              dip->variant.expression->kind ==
                                      (an_expr_node_kind)enk_object_lifetime &&
              dip->variant.expression->variant.object_lifetime.ptr == olp) {
            /* Link around the enk_object_lifetime expression -- it's not
               needed any longer. */
            dip->variant.expression =
                    dip->variant.expression->variant.object_lifetime.expr;
          }  /* if */
          /* Unbind the object lifetime and return it to an available list. */
          unbind_object_lifetime(olp);
          free_object_lifetime(olp);
        }  /* if */
      }  /* if */
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
      if (is_generated_cctor) {
        /* The constructor for the object as a whole is a generated copy
           constructor.  Any subobject constructors must also be copy
           constructors, and fields and base classes that have no constructor
           must be accounted for, too. */
        a_boolean  bitwise_copy = FALSE;
        if (cssp == NULL) {
          bitwise_copy = TRUE;
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
          rp = select_copy_constructor(tp, eff_qualifiers,
                                       /*source_is_rvalue=*/FALSE,
                                       &err_pos, object_class_type,
                                       &bitwise_copy,
                                       /*allow_suppressed_ctor=*/FALSE);
        }  /* if */
        if (bitwise_copy) {
          /* Construction by bitwise copy is allowed. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        } else if (rp == NULL) {
          /* The copy constructor was invalid in some way or other. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A valid copy constructor does exist.  Generate the dynamic init
             entry. */
          dip = alloc_ctor_dynamic_init(rp, /*implied_source=*/TRUE);
        }  /* if */
      } else {
        /* No copy constructor is required.  If any constructor exists, the
           default constructor should be called. */
        if (cip->kind == (a_constructor_init_kind)cik_field &&
            (is_any_reference_type(tp) || is_const_qualified)) {
          /* Ref-type field or const-qualified field but no initializer. */
          if (is_union_type(class_type)) {
            /* We don't issue diagnostics on initializing union members,
               partly because it's not well defined what should happen when
               const and non-const members are mixed, */
          } else if (is_const_qualified && 
                     ((cssp != NULL &&
                       cssp->has_user_provided_default_constructor) ||
                      is_template_dependent_type(tp))) {
            /* A const qualified field may be initialized without an explicit
               initializer if it is of class type and there is a default
               constructor for the class. */
          } else {
             /* There may be more than one uninitialized const or ref field,
                so we wait to collect them all before issuing the error. */
            /* Remove cip from the list. */
            if (prev_cip == NULL) {
              cip_list = cip->next;
            } else {
              prev_cip->next = cip->next;
            }  /* if */
            cip->next = NULL;
            /* Add it to a list that identifies fields that need to be
               initialized but have no initializer. */
            if (uninit_list == NULL) {
              uninit_list = cip;
            } else {
              end_of_uninit_list->next = cip;
            }  /* if */
            end_of_uninit_list = cip;
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
                a_symbol_ptr field_sym = (a_symbol_ptr)cip->variant.field->
                                                   source_corresp.assoc_info;
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
            (void)reference_to_trivial_default_constructor(tp, &err_pos,
                                                         /*check_access=*/TRUE,
                                                         (a_boolean *)NULL);
          }  /* if */
        }  /* if */
        /* Consider dropping the ctor-initializer entry if it isn't needed. */
        if (cssp == NULL ||
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
              cip_list = cip->next;
            } else {
              prev_cip->next = cip->next;
            }  /* if */
          }  /* if */
          continue;
        }  /* if */
        rp = select_default_constructor(tp, &err_pos, object_class_type,
                                        (a_boolean *)NULL);
        if (rp == NULL) {
          /* No constructor to call. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A default constructor does exist.  Generate the dynamic init
             entry. */
          dip = alloc_ctor_dynamic_init(rp, /*implied_source=*/FALSE);
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
    if (array_type != NULL &&
        dip->kind == (a_dynamic_init_kind)dik_constructor) {
      /* We have an array whose elements are constructible.  dip is the
         dynamic init entry for the element.  Create a dynamic init entry to
         represent the initialization of the array as a whole. */
      check_assertion(dip->is_constructor_init);
      ctor_dip = dip;
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
      /* Build the looping constant entry. */
      repeat_nonconstant_init(ctor_dip, array_type, tp, dip,
                              array_element_count(array_type, tp));
      dip->is_constructor_init = TRUE;
      if (ctor_dip->destructor != NULL) {
        /* A destructor is recorded in the array element dynamic-init entry.
           This is in case an exception is thrown in the midst of constructing
           the array, so that the already-constructed elements can be
           properly destroyed. */
        check_assertion(exceptions_enabled);
        ctor_dip->destruction_is_for_partially_constructed_aggregate = TRUE;
        /* The dynamic init for the array as a whole should also indicate
           destruction. */
        dip->destructor = ctor_dip->destructor;
        record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                           /*block_lifetime=*/TRUE);
      }  /* if */
      /* Overwrite the dynamic-init pointer in the current ctor-init entry. */
      cip->initializer = dip;
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
      if (!any_ref_member_on_uninit_list) {
        severity = (gpp_mode && gnu_version < 30400) ? es_warning
                                                     : es_discretionary_error;
      }  /* if */
      pos_sy_start_diagnostic(severity, ec_missing_initializer_on_fields,
                              &pos_curr_token, (a_symbol_ptr)ctor_rout->
                                                   source_corresp.assoc_info);
    }  /* if */
    for (cip = uninit_list; cip != NULL; cip = cip->next) {
      a_symbol_ptr field_sym = symbol_for(cip->variant.field);
      if (is_any_reference_type(cip->variant.field->type)) {
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
    for (cip = cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = (a_symbol_ptr)cip->variant.field->source_corresp.assoc_info;
      } else {
        sym = (a_symbol_ptr)cip->variant.base_class->type->
                                                source_corresp.assoc_info;
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
  db_exit();
  return cip_list;
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

  db_enter(3, "dtor_initializer");
  source_pos = dtor_rout->source_corresp.decl_position;
  class_type = parent_class_of(dtor_rout);
  check_assertion(class_type != NULL);
  ctsp = class_type_supp(class_type);
  /* The order of destructor calls is exactly the reverse of the order of
     constructor calls.  In other words, destructors for virtual base classes
     are last, preceded by destructors for nonvirtual direct base classes,
     with destructors for members coming first (ARM 12.4).  Thus we follow the
     logic in ctor_initializer, except that the lists are built backwards and
     merged backwards.   First construct the lists for virtual base classes
     and nonvirtual direct base classes. */
  /* First loop through the base classes looking for virtual base classes. */
  is_virtual_pass = TRUE;
  cip_list = NULL;
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
      if (microsoft_mode && field_is_nontrivial_property(field)) {
        /* Property fields are not really data members and should not be
           destroyed. */
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


void check_for_missing_initializer(a_symbol_ptr       sym,
                                   a_type_ptr         type)
/*
This routine is called when no explicit, value, or default initialization has
occurred.  It determines whether an initializer should have been provided
and issues a diagnostic if appropriate.  It is used both for variable
declarations (when sym represents the variable) and for unnamed objects that
are created by a new expression (in which case sym is NULL).  In both cases
"type" points to the type of the object.
*/
{
  a_variable_ptr       vp;
  a_boolean            init_required;
  a_base_class_ptr     bcp;
  a_boolean            is_empty_POD_class = FALSE;
  an_error_severity    severity;
  a_boolean            is_incomplete_array = FALSE;

  db_enter(4, "check_for_missing_initializer");
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
              } else if (vp->declared_storage_class ==
                                                 (a_storage_class)sc_static) {
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
}  /* check_for_missing_initializer */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
