/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
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
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && ... */
#if LOWER_DESIGNATED_INITIALIZERS
#include "lower_init.h"
#endif /* LOWER_DESIGNATED_INITIALIZERS */
#endif /* DO_IL_LOWERING */


#define array_element_count(array_type, elem_type)                      \
  ((array_type)->size == 0 ? 1 : (array_type)->size / (elem_type)->size)


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
                                 a_boolean                   static_lifetime)
/*
Initialize an entry of type an_aggregrate_init_info.
*/
{
  init_info->static_lifetime = static_lifetime;
  init_info->any_uninitialized_member = FALSE;
  init_info->any_uninitialized_const_or_ref_member = FALSE;
  init_info->comma_seen = FALSE;
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
		pending_init_con;
			/* Pointer to a constant entry produced when
			   scanning for a whole-class-object initializer for
			   an aggregate class.  The class as a whole cannot
			   be initialized, so the initializer is saved to
			   initialize a member. */
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
  unsigned long
		pending_init_levels;
			/* When pending_init_con is non-NULL, the number of
			   levels down at which to find the member to be
			   initialized (since the first direct member of an
			   aggregate class can itself be an aggregate class
			   or an array). */
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
  init_context->pending_init_con = NULL;
  init_context->repeat = NULL;
  init_context->pending_init_levels = 0;
  if (prev_init_context != NULL && !is_error_type(type)) {
    /* See if there is a pending constant (parsed while testing for the
       "whole-object initialization" case) that needs to be moved to the
       next context down (provided that is not an error context). */
    a_constant_ptr  init_con = prev_init_context->pending_init_con;
    unsigned long   levels_down = prev_init_context->pending_init_levels;

    check_assertion((levels_down > 0) == (init_con != NULL));
    if (init_con != NULL) {
      /* Propagate the pending constant to the next context, and clear it
         from the current context, decrementing the level indicator. */
      init_context->pending_init_con = init_con;
      init_context->pending_init_levels = levels_down - 1;
      prev_init_context->pending_init_con = NULL;
      prev_init_context->pending_init_levels = 0;
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
  set_type_size(array_type);
  *type = array_type;
}  /* set_initialized_array_size */


a_boolean check_string_constant_initializer(a_type_ptr      *var_type,
                                            a_constant_ptr  string_con)
/*
var_type is an array of char or wchar_t.  Return TRUE if and only if it can
be initialized with the given string literal.  If necessary, the string
literal may be truncated and the type may be modified (e.g., to set the
length of the string).
*/
{
  /* The object being initialized has type array of char or wchar_t, and
     is being initialized with a string.  Handle this case specially. */
  a_type_ptr     array_type;
  a_targ_size_t  string_length, num_elems;
  a_targ_size_t  array_length;
  a_boolean      is_wide_string = !is_char_array_type(string_con->type);
  a_boolean      err = FALSE;

  /* The object to be initialized is an array (possibly incomplete) of
     char or wchar_t -- i.e., a string or wide string. */
  check_assertion(is_char_array_type(*var_type) ||
                  is_wchar_t_array_type(*var_type));
  /* The constant and the array should have the same underlying character
     element type -- e.g., it's a mismatch if one is a wide string
     and the other a normal string. */
  err = (is_char_array_type(*var_type) != !is_wide_string);
  if (!err) {
    /* The constant is a string with characters that are compatible with
       the array element type.  (Note that an array of characters of any
       signedness can be initialized with a string literal: ANSI C 3.5.7.) */
    num_elems = string_length = string_con->variant.string.length;
    if (is_wide_string) {
      /* Adjust the wide string number of elements. */
      num_elems /= targ_sizeof_wchar_t;
    }  /* if */
    array_type = skip_typerefs(*var_type);
    if (is_incomplete_type(array_type)) {
      /* The array type is incomplete, and therefore the array size
         is set from the string length. */
      set_initialized_array_size(&array_type, num_elems);
      *var_type = array_type;
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
        if (num_elems-1 == array_length &&
            C_dialect != C_dialect_cplusplus) {
          /* Decrement the string length, and change its type,
             thus "dropping" the final null.  Note that this depends on
             the string not being shared. */
          num_elems--;
          if (!is_wide_string) {
            string_length--;
            string_con->type = string_type(num_elems);
          } else {
            string_length -= targ_sizeof_wchar_t;
            string_con->type = wide_string_type(num_elems);
          }  /* if */
          string_con->variant.string.length = string_length;
        } else {
          /* The initializer string is too long for the array being
             initialized. */
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return !err;
}  /* check_string_constant_initializer */


static a_boolean process_string_constant_initializer(
                                 a_type_ptr                     *type_ptr,
                                 a_constant_ptr                 *init_con,
                                 an_aggregate_init_context_ptr  init_context)
/*
If the variable type (given in *type_ptr) is a character array and the
initializer is a string literal, return TRUE and update *init_con to indicate
the initialization that is specified. The given string constant must be
acceptable as an initial value for the string type *type.  Change the
constant's type, or remove the final null from a string literal, if necessary.
If *type_ptr is an incomplete type, change it to reflect the actual size of
the string literal. (Note the extra level of indirection that allows that.)
If there is an error, issue an error and return an error constant.
*/
{
  a_boolean      is_string_init = FALSE;
  a_boolean      paren_flag = FALSE;
  a_boolean      using_pending_init_con = FALSE;
  a_constant_ptr cp;

  if (is_string_type(*type_ptr)) {
    if (init_context != NULL && init_context->pending_init_con != NULL) {
      /* The initializer has already been scanned. */
      if (init_context->pending_init_levels == 0) {
        /* It applies to the current level.  Check whether it's a string
           literal. */
        cp = init_context->pending_init_con;
        if (cp->kind == (a_constant_repr_kind)ck_string) {
          is_string_init = TRUE;
          using_pending_init_con = TRUE;
          /* Clear the pointer in the context block so it won't be reused
             later. */
          init_context->pending_init_con = NULL;
        }  /* if */
      }  /* if */
    } else if (curr_token == tok_string_literal) {
      is_string_init = TRUE;
    } else if (curr_token == tok_lparen) {
      if ((any_cfront_mode() || C_dialect == C_dialect_pcc ||
           microsoft_mode) &&
          next_token() == tok_string_literal) {
        /* This is a special case that's accepted in K&R mode, cfront mode,
           and Microsoft mode:
             char a[] = ("hello");
           (Note: we only recognize this sort of case when there is a single
           set of parentheses surrounding the string -- both pcc and cfront
           do allow multiple parens.) */
        is_string_init = TRUE;
        paren_flag = TRUE;
        /* Bypass the left paren. */
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_string_init) {
    /* The object being initialized has type array of char or wchar_t, and
       is being initialized with a string.  Handle this case specially. */
    a_boolean      err = FALSE;

    /* The object to be initialized is an array (possibly incomplete) of
       char or wchar_t -- i.e., a string or wide string. */
    if (!using_pending_init_con) {
      /* The constant wasn't prescanned. */
      cp = &const_for_curr_token;
      if (cp->kind != (a_constant_repr_kind)ck_string) {
        /* The constant is not a string. */
        err = TRUE;
      }  /* if */
    }  /* if */
    err = !check_string_constant_initializer(type_ptr, cp);
    if (err) {
      /* There was an error of some kind. */
      if (!is_error_type(cp->type)) {
        pos_ty2_error(ec_bad_initializer_type, &error_position,
                      cp->type, *type_ptr);
      }  /* if */
      *init_con = alloc_error_constant();
    } else {
      if (!using_pending_init_con) {
        /* Allocate the string constant. */
        *init_con = alloc_unshared_constant(cp);
      } else {
        /* The prescanned constant was already allocated. */
        *init_con = cp;
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITI0NS_IN_IL */
    if (!using_pending_init_con) {
      /* Bypass the string and the right paren, if appropriate. */
      (void)get_token();
    }  /* if */
    if (paren_flag) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (curr_token == tok_rparen) {
        curr_construct_end_position = pos_curr_token;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITI0NS_IN_IL */
      (void)required_token(tok_rparen, ec_exp_rparen);
    }  /* if */
  }  /* if */
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
Examine each field in the linked list headed by fp, returning TRUE if any
is of class-struct-union type with a constructor (or array thereof).
init_info is a pointer to a block of information tracking this initialization.
*/
{
  a_field_ptr                    fp = init_context->field;
  a_type_ptr                     tp;
  a_boolean                      ctor_found = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  /* Make a pass over the remaining fields. */
  for (; fp != NULL; fp = fp->next) {
    tp = fp->type;
    if (is_reference_type(tp)) {
      /* Field is a reference -- an error should be put out. */
      init_info->any_uninitialized_const_or_ref_member = TRUE;
    } else if (C_mode() && is_const_qualified_type(tp)) {
      /* Const field in C mode -- a warning will be issued. */
      init_info->any_uninitialized_const_or_ref_member = TRUE;
      break;
    } else {
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      tp = skip_typerefs(tp);
      if (is_immediate_class_type(tp)) {
        /* Field is a class type (or an array of class-type elements). */
        cssp = symbol_supplement_for_class(tp);
        if (C_mode() && tp->variant.class_struct_union.any_const_member) {
          /* In C mode, the field's type is a struct with a const field. */
          init_info->any_uninitialized_const_or_ref_member = TRUE;
          break;
        } else if (cssp->constructor != NULL ||
                   (exceptions_enabled && cssp->destructor != NULL)) {
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
    /* There are one or more uninitialized elements. */
    element_type = f_skip_typerefs(array_element_type(array_type));
    if (is_array_type(element_type)) {
      a_type_ptr  tp = element_type;
      element_type = f_skip_typerefs(underlying_array_element_type(tp));
      number_of_uninitialized_elements *=
                                    array_element_count(tp, element_type);
    }  /* if */
    if (is_class_struct_union_type(element_type)) {
      /* It is an array of class objects. */
      cssp = symbol_supplement_for_class(element_type);
    } else {
      cssp = NULL;
    }  /* if */
    if (!any_constructible_fields_remaining(init_context, init_info) &&
        (cssp == NULL ||
         (cssp->constructor == NULL &&
          (!exceptions_enabled || cssp->destructor == NULL)))) {
      if (cssp != NULL && cssp->constructor == NULL) {
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
      if (cssp == NULL || cssp->constructor == NULL) {
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
        init_done = TRUE;
      } else {
        /* Get the default constructor.  Note that it is an error if it
           is missing. */
        ctor_rp = select_default_constructor(element_type, &pos_curr_token,
                                             element_type, /*evaluated=*/TRUE);
        if (ctor_rp == NULL) {
          /* Error of some sort. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          init_done = TRUE;
        } else  {
          /* If there's a constructor routine create a dik_constructor
             dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = ctor_rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          copy_ctor_default_args_to_dynamic_init(dip);
          /* If the default constructor is generated and some component of the
             class requires zeroing, initialization is not really done because
             the value-initialization rules require that the zeroing occurs. */
          init_done = !(ctor_rp->compiler_generated &&
                        element_type
                        ->variant.class_struct_union.has_zero_init_component);
        }  /* if */
      }  /* if */
      if (cssp != NULL) {
        if (exceptions_enabled && cssp->destructor != NULL) {
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
    for (; init_context->field != NULL;
           init_context->field = init_context->field->next) {
      tp = skip_typerefs(init_context->field->type);
      if (is_array_type(tp)) {
        array_type = tp;
        tp = f_skip_typerefs(underlying_array_element_type(tp));
      } else {
        array_type = NULL;
      }  /* if */
      if (is_immediate_class_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
      } else {
        cssp = NULL;
      }  /* if */
      if (cssp == NULL || cssp->constructor == NULL) {
        /* Zero-initialize the field and then continue looping.  There is
           a field later in the list for which the default constructor has to
           be called, but we can't leave this field uninitialized. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
      } else {
        /* Default initialization is required -- this must be the field found
           by the call to any_constructible_fields_remaining. */
        found_constructible_field = TRUE;
        /* Get the default constructor.  Note that it is an error if it
           is missing. */
        ctor_rp = select_default_constructor(tp, &pos_curr_token, tp,
                                             /*evaluated=*/TRUE);
        if (ctor_rp == NULL) {
          /* Error of some sort. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else  {
          /* If there's a constructor routine create a dik_constructor
             dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = ctor_rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          copy_ctor_default_args_to_dynamic_init(dip);
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
        if (exceptions_enabled && cssp->destructor != NULL) {
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
    }  /* for */
    init_done = TRUE;
    init_context->any_dynamic_initialization = TRUE;
  }  /* if */
  db_exit();
  return init_done && !incomplete_value_init;
}  /* init_remaining_fields */


static a_constant_ptr scan_initializer_of_simple_object(
                                    a_boolean           nonconst_allowed,
                                    a_boolean           static_lifetime,
                                    a_boolean           force_object_lifetime,
                                    a_boolean           is_copy_initialization,
                                    a_type_ptr          type,
                                    a_dynamic_init_ptr  *dip_ptr)
/*
Scan a initializer for a non-aggregate object (i.e., not an array and not
a class/struct/union object).  If nonconst_allowed is TRUE (always the case
in C++, sometimes otherwise) a nonconstant expression is allowed; if not,
a constant is required.  If static_lifetime is TRUE, the underlying entity
has static storage duration.  force_object_lifetime is TRUE only in C++ mode
and only when this function is called in scanning a entry in a ctor
initializer list; it is passed on to scan_initializer_expression to force
creation of an object lifetime for expression temporaries even if
long_lifetime_temps is TRUE.  If is_copy_initialization is TRUE, this
is copy-initialization ("="-form); otherwise, it's direct-initialization
("()"-form).  type is the data type of the object being initialized.
dip_ptr is a pointer to a dynamic init pointer; if the latter is NULL,
a dynamic init entry may be allocated and returned, but if *dip_ptr is
non-NULL, build the initialization information into the object it
points to.  A (possibly NULL) constant pointer is returned; iff
*dip_ptr is updated, NULL is returned.  Thus, if nonconst_allowed is
TRUE, return a pointer to a constant entry.  Otherwise, if the
initializer is a constant value then return a pointer to a constant
only if *dip_ptr is NULL.  If the initializer is nonconstant or
*dip_ptr is non-NULL, return a NULL constant pointer and build
*dip_ptr to represent the initialization.
*/
{
  an_expr_node_ptr expression;
  a_boolean        is_constant;
  a_constant       constant, *cp = NULL;

  if (process_string_constant_initializer(
                                   &type, &cp,
                                   (an_aggregate_init_context_ptr)NULL)) {
    /* The object being initialized has type array of char or wchar_t, and
       is being initialized with a string. */
    is_constant = TRUE;
  } else if (nonconst_allowed) {
    /* Scan a potentially non-constant initializer expression.  The result
       of the scan is a constant if the expression is constant, and an
       expression node if not. */
    scan_initializer_expression(type, static_lifetime, force_object_lifetime,
                                is_copy_initialization,
                                &is_constant, &expression, &constant);
  } else {
    /* Non-constant is not allowed. */
    scan_constant_initializer_expression(type, &constant);
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


static a_boolean designator_coming(a_boolean *array_designator)
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
NULL if the return value is not needed.
*/
{
  a_boolean result = FALSE, local_array_designator = FALSE;

  if (designators_allowed) {
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
This function returns TRUE if such a case applied and, if so, sets
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
   } b = { 4, a, a}; // b.a1.i == 4, b.a2.i == 0, b.z == 20 after this
The bulk of this work is done in scan_aggregate_initializer_expression.
If a value was scanned but whole object initialization did not apply, the
resulting constant is placed on context->pending_init_con for use further on.
In C99 mode, the processing is similar to that in C++.
*/
{
  a_boolean                      is_whole_object_init; /* result */
  a_boolean                      top_level = (context->prev_context == NULL);
  a_boolean                      err = FALSE, is_constant = FALSE;
  a_boolean                      string_literal = FALSE;
  a_constant                     constant;
  unsigned long                  levels_down;
  a_class_symbol_supplement_ptr  cssp;
  a_dynamic_init_ptr             dip;

  if ((!C_mode() || c99_mode || gcc_mode) &&
      (is_class_struct_union_type(context->type) ||
       (gcc_mode && is_array_type(context->type))) &&
      (curr_token != tok_lbrace || context->pending_init_con != NULL) &&
      !top_level && !designator_coming((a_boolean *)NULL)) {
    /* If this is an aggregate, whole object initialization is possible but
       not required.  Indeed, if the initializing expression can initialize
       the first initializable member of an aggregate, then that should be
       done instead of whole aggregate initialization.
       scan_aggregate_initializer_expression will determine this. */

    is_whole_object_init = TRUE;
    if (!is_array_type(context->type)) {
      cssp = symbol_supplement_for_class(context->type);
      check_assertion_str(c99_mode || gcc_mode ||
                          cssp->has_copy_constructor ||
                          cssp->construction_by_bitwise_copy_allowed ||
                          skip_typerefs(context->type)->
                                  variant.class_struct_union.is_nonreal_class,
                        "process_whole_object_init: missing copy constructor");
    }  /* if */
    if (context->pending_init_con != NULL) {
      /* The initializer has already been scanned. */
      levels_down = context->pending_init_levels;
      if (levels_down == 0) {
        /* This is the level at which the initializer is to be applied. */
        *init_constant = context->pending_init_con;
        if ((*init_constant)->kind == (a_constant_repr_kind)ck_dynamic_init) {
          dip = (*init_constant)->variant.dynamic_init;
        } else {
          is_constant = TRUE;
        }  /* if */
      }  /* if */
    } else if (!scan_aggregate_initializer_expression(
                              context->type, init_info->static_lifetime,
                              &levels_down, &is_constant, &dip, &constant)) {
      /* No appropriate initializer was found. */
      err = TRUE;
    } else {
      if (is_constant) {
        /* A constant initializer was found. */
        *init_constant = alloc_unshared_constant(&constant);
        if (constant.kind == (a_constant_repr_kind)ck_string) {
          /* The initializer is a string literal: this is a special case
             that should be handled by process_string_constant_initializer.
             Set string_literal to TRUE to indicate that this is not a
             whole object initializer and that the constant should be
             remembered for later processing. */
          check_assertion(gcc_mode);
          string_literal = TRUE;
        }  /* if */
      } else {
        /* A dynamic initialization. */
        check_assertion(dip != NULL);
        *init_constant = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
        (*init_constant)->variant.dynamic_init = dip;
      }  /* if */
    }  /* if */
    if (!err) {
      if (levels_down == 0 && !string_literal) {
        /* The initialization applies at the current level. */
        (*init_constant)->type = rvalue_type(context->type);
        if (!is_constant) {
          context->any_dynamic_initialization = TRUE;
          if (exceptions_enabled) {
            if (cssp->destructor != NULL) {
              /* If appropriate, add a destructor pointer to the dynamic
                 init entry. This is for the case in which an exception is
                 thrown by the constructor before the entire array has been
                 initialized. */
              a_routine_ptr  dtor_rp = cssp->destructor->variant.routine.ptr;
              add_dtor_for_partially_constructed_aggregate(dtor_rp, dip);
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Whole object initialization will not be done at this level after
           all.  (This will only occur for aggregate classes.) */
        is_whole_object_init = FALSE;
        if (*init_constant != NULL) {
          /* The initialization applies one or more levels down.  Remember
             what was "prescanned". */
          context->pending_init_con = *init_constant;
          context->pending_init_levels = levels_down;
          *init_constant = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    is_whole_object_init = FALSE;
  }  /* if */
  return is_whole_object_init;
}  /* process_whole_object_init */


static void handle_missing_brace(a_type_ptr *dest_type)
/*
In ANSI C and C++, the top-level initializer for a class, struct, union, or
array must be surrounded by braces (except for whole object initialization of
classes, handled elsewhere).  However, pcc allows it -- e.g., "int a[2] = 1;"
is equivalent to "int a[2] = { 1 };".  We allow pcc behavior as an extension
in C mode, but it's an error in C++ mode. The type of the object being
initialized is *dest_type; if an error occurs this will be set to an error
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
    /* If we're issuing an error, avoid error recovery problems by
       setting dest_type to an error type. */
    if (severity == es_error) {
      *dest_type = error_type();
    }
  }  /* if */
}  /* handle_missing_brace */


static void start_aggregate_init_scan_loop(
                          an_aggregate_init_context  *context,
                          a_type_ptr                 *member_type,
                          a_boolean                  *any_more_members,
                          a_boolean                  *is_incomplete_array)
/*
Initialize the state for the loop that will scan an aggregate initializer
list.  The type of the aggregate or subaggregate whose initializer is about
to be scanned is dest_type.  State information about this initializer scanning
is maintained in *context and its "field" member will be made to point to
the first initializable field (if any) if dest_type is a class type.
*member_type will be set to the type of the next member to be initialized (or
an error type if dest_type is an error_type).  If there are any members to
initialize, *any_more_members will be set to TRUE.  If context->type is an
array type of unknown size, *is_incomplete_array will be set to TRUE.
*/
{
  a_type_ptr  type = skip_typerefs(context->type);
  a_type_kind kind = type->kind;

  *any_more_members = TRUE;  /* Assume. */
  *is_incomplete_array = FALSE;
  if (kind == (a_type_kind)tk_error ||
      kind == (a_type_kind)tk_template_param) {
    /* Unknown member type (due to error or template parameterization). */
    *member_type = error_type();
  } else if (kind == (a_type_kind)tk_array) {
    /* Array.  Start with first element. */
    *is_incomplete_array = is_incomplete_type(type);
    *member_type = type->variant.array.element_type;
    /* Note that arrays of incomplete struct/union types (an extension)
       do not make it to here (they're caught as an error at the top
       level in the routine "initializer" and replaced by an error type),
       so we don't have to check for them here. */
  } else {
    a_field_ptr field;
    /* Class/struct/union.  Start with first field. */
    check_assertion(is_immediate_class_type(type));
    field = type->variant.class_struct_union.field_list;
    /* Skip past an unnamed field. */
    field = next_initializable_field(field);
    *any_more_members = (field != NULL);
    context->field = field;
  }  /* if */
}  /* start_aggregate_init_scan_loop */


static a_boolean any_initializers(an_aggregate_init_context_ptr context,
                                  a_boolean                     brace_flag,
                                  a_boolean                     any_members,
                                  a_boolean                     *nothing_taken)
/*
Returns whether any initializers are available for the initialization of the
aggregate tracked by init_context.  If an introductory brace was scanned,
brace_flag should be TRUE.  If the aggregate has any initializable members,
any_members should be TRUE.  This function detects the case where an empty
class is being initialized and sets *nothing_taken accordingly to indicate
if no initializer was consumed.
*/
{
  /* Check for cases that involve initializing nothing, i.e., the
     zero-trip-loop cases: */
  a_boolean result = TRUE;  /* Assume. */

  if (brace_flag) {
    /* The list for the aggregate at this level is enclosed in { }. */
    if (curr_token == tok_rbrace && context->pending_init_con == NULL) {
      /* Empty initializer list --  "{ }".  An error in C, okay in C++. */
      if (C_mode()) error(ec_exp_primary_expr);
      result = FALSE;
    }  /* if */
  } else {
    /* The list for the aggregate is not enclosed in braces. */
    a_boolean top_level = (context->prev_context == NULL);
    if (!any_members && !top_level && !C_mode()) {
      /* This is an initialization of an aggregate with no members,
         i.e., an empty class, and there are no braces for this
         level of the aggregate.  Take nothing to satisfy this
         initialization. */
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
  a_constant constant;
  a_boolean  okay = TRUE;

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
            error(ec_subscript_out_of_range);
            okay = FALSE;
          } else {
            *subscript = value;
          }  /* if */
        }  /* if */
      } else {
        /* Negative subscript. */
        error(ec_subscript_out_of_range);
        okay = FALSE;
      }  /* if */
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


static a_designation_state check_for_end_of_designation(
                                                  a_boolean allow_colon,
                                                  a_boolean assign_optional)
/*
After a designator has been scanned, call this function to check if the
designation is completed (returns ds_complete_designation) or more
designators are to come (returns ds_partial_designation).  If extended field
designators of the form 'x:' are allowed, allow_colon should be set to true
(and the colon indicates a complete designation has been seen).  Similarly,
extended array element designators make the '=' optional and assign_optional
should be TRUE in that case.  The termination token is consumed if it is
present.
*/
{
  a_designation_state result;

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
    }  /* if */
    result = ds_complete_designation;
  } else {
    result = ds_partial_designation;
  }  /* if */
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
    if (!is_error_type(context->type)) {
      error(ec_invalid_designator_kind);
      context->type = error_type();
    }  /* if */
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
#endif /* EXTRA_SOURCE_POSITI0NS_IN_IL */
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
    *curr_array_element = start_el;
  } else {
    /* Some error. */
    context->type = error_type();
  }  /* if */
  /* See whether the designator list ends here. */
  init_info->designation_state = check_for_end_of_designation(
                             /*allow_colon=*/FALSE,
                             /*assign_optional=*/extended_designators_allowed);
}  /* get_array_designator */


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
  if (!is_class_struct_union_type(context->type)) {
    /* An attempt to use a field designator in a non-struct/union context. */
    if (!is_error_type(context->type)) {
      error(ec_invalid_designator_kind);
      context->type = error_type();
    }  /* if */
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
    /* Extended form: identifier followed by period. */
    extended_form = TRUE;
  }  /* if */
  if (have_id) {
    /* Look up the field associated with the identifier: */
    if (!okay) {
      /* Can't look up the identifier; we don't know where we are. */
    } else {
      a_symbol_ptr member_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                                          context->type,
                                                          IDL_NO_OPTIONS);
      if (member_sym == NULL) {
        /* The name was not found. */
        pos_stsy_error(ec_not_a_field, &error_position,
                       locator_for_curr_id.symbol_header->identifier,
                       (a_symbol_ptr)skip_typerefs(context->type)
                                                  ->source_corresp.assoc_info);
        okay = FALSE;
      } else {
        check_assertion_str(member_sym->kind == (a_symbol_kind)sk_field,
                            "get_field_designator: non-field member");
        designated_field = member_sym->variant.field.ptr;
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITI0NS_IN_IL */
    /* Move past the identifier */
    (void)get_token();
  }  /* if */
  error_position = start_pos;
  if (okay) {
    /* Build the ck_designator constant and add it to the list. */
    a_constant_ptr designator =
                         alloc_constant((a_constant_repr_kind)ck_designator);

    designator->variant.designator.field = designated_field;
    append_initializer_constant(context, designator);
    *field = designated_field;
  } else {
    /* Some error. */
    context->type = error_type();
  }  /* if */
  /* See whether the designator list ends here. */
  init_info->designation_state =
       check_for_end_of_designation(/*allow_colon=*/extended_form,
                                    /*assign_optional=*/FALSE);
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
      designator_coming(&array_designator)) {
    /* Process an array designator ( [x] = ) or a field designator
       ( .f = ). */
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
                                      an_aggregate_init_info_ptr    init_info,
                                      an_aggregate_init_context_ptr context)
/*
Scans a "single value" as a simple non-class type item for an aggregate
initializer.  Unfortunately, this scanning might have already occurred while
trying to determine if the expression could initialize a class type member
(see process_whole_object_init): in that case, the constant is pending in the
context structure.  init_info describes the state of the complete initializer
and context describes the state of the initialization of the current
subaggregate.  The function returns a pointer to an IL a_constant entity.
*/
{
  a_constant_ptr      constant; /* Result of this function */
  a_boolean           nonconst_allowed, brace_flag;
  a_boolean           microsoft_enum_case = FALSE;
  a_dynamic_init_ptr dip = 0;

  check_for_opening_brace(&brace_flag);
  if (context->pending_init_con != NULL) {
    /* The initializer has been prescanned when checking for whole-object
       initialization.  Use the pending initializer rather than doing
       another scan. */
    check_assertion(context->pending_init_levels == 0);
    constant = context->pending_init_con;
    constant->type = context->type;
    if (constant->kind == (a_constant_repr_kind)ck_dynamic_init) {
      dip = constant->variant.dynamic_init;
      check_assertion(dip != NULL);
    } else {
      constant->type = rvalue_type(constant->type);
    }  /* if */
    context->pending_init_con = NULL;
  } else {
    a_type_ptr  required_type = context->type;
    if (!C_mode()) {
      nonconst_allowed = TRUE;
    } else if (c99_mode || microsoft_mode || gcc_mode) {
      /* A C99 permits a nonconstant initializer in the aggregate
         initialization of an automatic variable.  This is also accepted
         by GNU and Microsoft compilers. */
      nonconst_allowed = !init_info->static_lifetime;
    } else {
      nonconst_allowed = FALSE;
    }  /* if */
    if (microsoft_mode && !C_mode() &&
        context->prev_context != NULL &&
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
                                     nonconst_allowed,
                                     (a_boolean)init_info->static_lifetime,
                                     /*force_object_lifetime=*/FALSE,
                                     /*is_copy_initialization=*/TRUE,
                                     required_type, &dip);
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
      type_change_constant(constant, context->type, /*is_implicit_cast=*/TRUE,
                           /*constant_context=*/FALSE,
                           /*evaluated_context=*/TRUE,
                           /*fold_constant_addr_exprs=*/FALSE,
                           /*is_reinterpret_cast=*/FALSE,
                           /*maintain_expression=*/TRUE,
                           &did_not_fold, &pos_curr_token);
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
  a_boolean                      is_incomplete_array;
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

  db_enter(4, "get_initializer");
  *nothing_taken = FALSE;
  *any_dynamic_init = FALSE;
  initialize_init_context(&context, prev_init_context, *type);
  if (process_whole_object_init(init_info, &context, &init_con)) {
    /* process_whole_object_init might have found that the "whole object
       initialization" case did not apply, in which case "FALSE" was returned
       and we fall through.  Otherwise, all the required work was done. */
  } else if (is_aggregate_or_union_type(context.type) ||
             ((is_error_type(context.type) ||
               is_template_param_type(context.type)) &&
              ((curr_token == tok_lbrace &&
                context.pending_init_con == NULL) ||
               (init_info->designation_state != ds_complete_designation &&
                designator_coming((a_boolean *)NULL))))) {
    /* Initialization of an array (complete or incomplete), struct, or
       union.  The result will be an aggregate constant except when an
       array of char is initialized by a string.  The initial
       values can either appear inside a brace-enclosed list, or at
       the current level. */
    if (curr_token == tok_lbrace && context.pending_init_con == NULL) {
      /* Make sure it's truly an aggregate and not some non-aggregate class: */
      if (is_class_struct_union_type(context.type) &&
          !symbol_supplement_for_class(context.type)->is_class_aggregate) {
        if (!skip_typerefs(context.type)
                              ->variant.class_struct_union.is_nonreal_class) {
          /* For a nonreal class, we cannot relate the initializers to the
             inner type structure of that class.  An error type ensures that
             we just collect the expressions, but no diagnostic should be
             issued. */
          pos_ty_error(ec_brace_initialization_not_allowed, &pos_curr_token,
                       context.type);
        }  /* if */
        context.type = error_type();
      }  /* if */
      check_for_opening_brace(&brace_flag);
      /* Since we saw a left brace, we start afresh with designations: */
      init_info->designation_state = ds_no_designation;
    } else {
      brace_flag = FALSE;
    }  /* if */
    if (process_string_constant_initializer(type, &init_con, &context)) {
      /* The object being initialized has type array of char or wchar_t, and
         is being initialized with a string. */
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
      /* In ANSI C and C++, the top-level initializer for a struct, union, or
         array must be surrounded by braces.  e.g., "int a[1] = 1;" is
         not allowed.  However, pcc will allow initialization with
         a single value and we allow it as an extension. */
      if (top_level && !brace_flag) {
        /* If a hard error is decided, context.type will become an error
           type. */
        handle_missing_brace(&context.type);
      }  /* if */
      /* Get information on the first member of the aggregate to be
         initialized (if any). */
      kind = skip_typerefs(context.type)->kind;
      start_aggregate_init_scan_loop(&context, &member_type,
                                     &any_more_members, &is_incomplete_array);
      curr_field = context.field;
      any_more_initializers = any_initializers(&context,
                                               brace_flag,
                                               any_more_members,
                                               nothing_taken);
      took_extra_comma = FALSE;
      /* Loop, scanning initializers and building an aggregate constant. */
      while (any_more_initializers) {
        add_stop_token(tok_comma);
        /* See whether a designator is next. */
        if (get_designator(init_info, &context, &curr_array_element,
                           &curr_field)) {
          /* A designator was present and has been processed. */
        } else {
          /* No designator. */
          if (!any_more_members) {
            /* There are more undesignated initializers, but we've run out of
               members into which to put them. */
            error(ec_too_many_initializer_values);
            context.type = error_type();
            any_more_members = TRUE;
          }  /* if */
        }  /* if */
        /* Determine the type of the member being initialized. */
        if (is_error_type(context.type)) {
          /* Some error was detected.  We don't know where we are or
             what we're initializing. */
          member_type = error_type();
          kind = (a_type_kind)tk_error;
        } else if (is_template_param_type(context.type)) {
          /* The destination type is a template parameter and therefore
             essentially unknown.  Treat it as if it had members of its
             own type. */
          member_type = context.type;
        } else if (kind == (a_type_kind)tk_array) {
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
          if (is_incomplete_type(member_type)) {
            /* Members of unions or aggregates cannot be incomplete. */
            if (is_array_type(member_type) && curr_field->next == NULL) {
              /* ... except that in several modes it's okay to declare a field
                 of incomplete array type when it's the last field in the
                 struct (but only when the struct is the top-level object
                 type).  (See also: check_field_type.)  Only in Microsoft
                 mode can such a field be initialized. */
              if (!top_level || !microsoft_mode) {
                error(ec_cannot_initialize_flexible_array_member);
              }  /* if */
            } else {
              unexpected_condition_str(
                                  "get_initializer: can't init 0-size member");
            }  /* if */
          }  /* if */
        }  /* if */
        /* Get the initializer for this one member. */
        set_err_pos_to_curr_token();
        member_con = get_initializer(&member_type, init_info, &context,
                                     &local_nothing_taken,
                                     &local_any_dynamic_init);
        /* If exceptions are enabled and the type of the member being
           initialized is a class with a destructor, it may be appropriate to
           record the destructor in case an exception is thrown before the
           top-level object is fully constructed. */
        if (exceptions_enabled &&
            member_con->kind != (a_constant_repr_kind)ck_dynamic_init &&
            is_class_struct_union_type(member_type)) {
          cssp = symbol_supplement_for_class(member_type);
          if (cssp->destructor != NULL) {
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
            /* Extended designators of the for '[i ... j]' cannot be applied
               to initializers with a dynamic component. */
            error(ec_no_range_designator_with_dynamic_init);
          } else {
            context.repeat->variant.init_repeat.constant = member_con;
            member_con = context.repeat;
            context.repeat = NULL;
          }  /* if */
        }  /* if */
        /* Add the constant entry to the list of constants. */
        append_initializer_constant(&context, member_con);
        /* If a designation was active, it is now consumed: */
        init_info->designation_state = ds_no_designation;
        remove_stop_token(tok_comma);
        check_assertion(!(local_nothing_taken && is_incomplete_array));
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
            if (!is_incomplete_array) {
              /* Note that we may get here with any_more_members == FALSE and
                 a designator can turn it into TRUE again. */
              a_type_ptr array_type = skip_typerefs(context.type);
              any_more_members =
                (array_type->variant.array.is_template_dependent_size_array ||
                 array_type->variant.array.variant.number_of_elements
                                                        > curr_array_element);
            } else {
              /* Keep track of the maximum subscript seen: */
              if (curr_array_element > array_size) {
                array_size = curr_array_element;
              }  /* if */
            }  /* if */
          }  /* if */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (microsoft_mode && top_level) {
            /* In Microsoft C mode, the check for a field of incomplete array
               type is not made -- such initializations are allowed for a
               top-level struct member. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
          } else if (designator_coming((a_boolean *)NULL)) {
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
        if (is_incomplete_array) {
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
    if (curr_token == tok_lbrace && !top_level &&
        context.pending_init_con == NULL) {
      diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                 ec_nonstd_braces);
    }  /* if */
    init_con = get_single_value_for_aggregate_initializer(init_info, &context);
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
  a_boolean              no_token_consumed, any_dynamic_init;
  a_boolean		 err = FALSE;

  check_assertion(C_mode() && (curr_token == tok_lbrace));
  initialize_init_info(&info, is_static);
  compound_constant = get_initializer(type, &info,
                                      (an_aggregate_init_context_ptr)NULL,
                                      &no_token_consumed,
                                      &any_dynamic_init);
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
    if (is_aggregate_or_union_type(*type)) {
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
  if (!err && info.any_uninitialized_member) {
    (*dip)->is_partially_initialized_compound_literal = TRUE;
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
static a_boolean scan_initializer_list(a_type_ptr            *type,
                                       a_variable_ptr        vp,
                                       a_boolean             static_lifetime,
                                       a_constant_ptr        *init_con,
                                       a_dynamic_init_ptr    *init_dip,
                                       a_source_position     *err_pos,
                                       a_decl_pos_block_ptr  decl_pos_block)
/*
Scan an initializer list for an aggregate initialization.  Usually it is a
brace-enclosed list of initializers, but the case of initializing an
array-of-char with a string is also handled here.  *type points to the type
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
  initialize_init_info(&init_info, static_lifetime);
  *init_con = get_initializer(type, &init_info,
                              (an_aggregate_init_context_ptr)NULL,
                              &nothing_taken, &any_dynamic_init);
  if ((*init_con)->kind == (a_constant_repr_kind)ck_error) {
    err = TRUE;
  } else {
    a_type_ptr  tp = *type;
    if (is_array_type(*type)) tp = underlying_array_element_type(tp);
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp) &&
        symbol_supplement_for_class(tp)->destructor != NULL) {
      dtor_rp = select_destructor(tp, tp, err_pos, /*honor_virtual=*/FALSE,
                                  /*evaluated=*/TRUE);
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
      if (init_info.any_uninitialized_member) {
        vp->is_partially_initialized = TRUE;
      }  /* if */
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


static a_boolean init_con_has_side_effects(a_constant_ptr con,
                                           a_boolean      *suppress_warning)
/*
Return TRUE if the indicated constant (part of an initialization)
has side effects.  Return *suppress_warning TRUE if a warning about
the expression doing nothing should be suppressed.
*/
{
  a_boolean      has_side_effects = FALSE, suppress = FALSE;
  a_constant_ptr subcon;

  if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    /* For aggregates, visit the enclosed constants. */
    for (subcon = con->variant.aggregate.first_constant;
         subcon != NULL && !has_side_effects;
         subcon = subcon->next) {
      a_boolean local_suppress;
      has_side_effects = init_con_has_side_effects(subcon, &local_suppress);
      suppress |= local_suppress;
    }  /* for */
  } else if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
    /* For dynamic init entries, check the dynamic initialization. */
    has_side_effects = dynamic_init_has_side_effects(con->variant.dynamic_init,
                                                     &suppress);
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* For a repeat, look at the repeated constant. */
    has_side_effects =
                   init_con_has_side_effects(con->variant.init_repeat.constant,
                                             &suppress);
  } else if (con->kind == (a_constant_repr_kind)ck_error) {
    /* An error constant could have been anything, including something
       with side effects. */
    has_side_effects = TRUE;
    suppress = TRUE;
  }  /* if */
  *suppress_warning = suppress;
  return has_side_effects;
}  /* init_con_has_side_effects */


a_boolean dynamic_init_has_side_effects(a_dynamic_init_ptr dip,
                                        a_boolean          *suppress_warning)
/*
Return TRUE if the indicated dynamic initialization has side effects,
i.e., it does something other than just return a value for the initialization.
Return *suppress_warning TRUE if a warning about the expression doing nothing
should be suppressed.
*/
{
  a_boolean has_side_effects = FALSE, suppress = FALSE;

  if (dip->destructor != NULL) {
    /* A destructor call causes side effects. */
    has_side_effects = TRUE;
  } else {
    switch (dip->kind) {
      case dik_none:
      case dik_zero:
        /* No side effects. */
        break;
      case dik_constant:
        if (dip->variant.constant->kind == (a_constant_repr_kind)ck_error) {
          /* An error constant could have been anything, including something
             with side effects. */
          has_side_effects = TRUE;
          suppress = TRUE;
        }  /* if */
        break;
      case dik_expression:
      case dik_call_returning_class_via_cctor:
        /* An expression might have side effects.  See if it does. */
        has_side_effects = node_has_side_effects(dip->variant.expression,
                                                 &suppress);
        break;
      case dik_constructor:
        /* A constructor call causes side effects. */
        has_side_effects = TRUE;
        break;
      case dik_nonconstant_aggregate:
        /* A non-constant aggregate must be examined recursively. */
        has_side_effects = init_con_has_side_effects(dip->variant.constant,
                                                     &suppress);
        break;
#if CHECKING
      case dik_bitwise_copy:
      default:
        internal_error("dynamic_init_has_side_effects: bad dyn init kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  *suppress_warning = suppress;
  return has_side_effects;
}  /* dynamic_init_has_side_effects */


static void gen_dynamic_initialization(
                                     a_variable_ptr     vp,
                                     a_dynamic_init_ptr dip,
                                     a_local_static_variable_init_ptr
                                                        *local_static_var_init,
                                     a_source_position  *source_pos,
                                     a_statement_ptr    *p_init_stmt)
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
created, or NULL it there is none.
*/
{
  a_statement_ptr          init_stmt;
  a_boolean                static_lifetime = FALSE;
  a_boolean                at_file_scope;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  db_enter(4, "gen_dynamic_initialization");
  *local_static_var_init = NULL;
  if (p_init_stmt != NULL) *p_init_stmt = NULL;
  at_file_scope = (depth_innermost_function_scope == NO_SCOPE_DEPTH);
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
       This must be done after record_end_of_lifetime_destruction is called. */
    init_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_init,
                                          &vp->source_corresp.decl_position);
    if (p_init_stmt != NULL) *p_init_stmt = init_stmt;
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
                                        /*suppress_incompatible_error=*/FALSE);
    }  /* if */
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


static a_constant_ptr simple_initializer(a_boolean          static_lifetime,
                                         a_type_ptr         vp_type,
                                         a_dynamic_init_ptr *init_dip,
                                         a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a simple nonaggregate, nonparenthesized initializer.  static_lifetime is
TRUE is the entity being initialized has static storage duration; vp_type is
the type of that entity.
*/
{
  a_constant_ptr constant; /* result of this function */
  a_boolean      brace_flag, nonconstant_allowed;

  check_for_opening_brace(&brace_flag);
  nonconstant_allowed = (!C_mode() || !static_lifetime);
  /* Scan the initializer.  Either a constant pointer is returned or else
     a dynamic init entry representing an expression. */
  constant =
          scan_initializer_of_simple_object(nonconstant_allowed,
                                            static_lifetime,
                                            /*force_object_lifetime=*/FALSE,
                                            /*is_copy_initialization=*/TRUE,
                                            vp_type, init_dip);
  if (microsoft_bugs) {
    /* The microsoft compiler accepts things like "int x = { f(), { 3 } }"
       and has the last value replace previous ones (though side-effects
       take place), unless they're both constants and x is not automatic. */
    while (curr_token == tok_comma && next_token() == tok_lbrace) {
      /* Eat the comma, parse the next constant (recursive) and combine
         initializer expressions: */
      a_constant_ptr     next_constant;
      a_dynamic_init_ptr next_dip = NULL;
      (void)get_token();
      next_constant = simple_initializer(static_lifetime, vp_type, &next_dip,
                                         decl_pos_block);
      if (static_lifetime && constant != NULL && next_constant != NULL) {
        /* Approximately emulate the Microsoft behavior that if only true
           constants are involved, the first value is kept for variables
           with static lifetime.  The emulation is not perfect when more
           nesting is involved as in "int x = { f(), { 1, { 2 }}};". */
      } else {
        combine_initializers(constant, *init_dip, next_constant, next_dip);
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


void initializer(a_symbol_ptr          symbol_ptr,
                 a_source_position     *source_pos,
                 an_id_linkage_kind    linkage,
                 a_boolean             parenthesized_initializer,
                 a_boolean             is_parameter,
                 a_boolean             *incomplete_type_error_reported,
                 a_decl_pos_block_ptr  decl_pos_block)
/*
Scan an initializer (3.5.7) for the symbol pointed to by symbol_ptr
(with linkage as given by linkage; a parameter if is_parameter is TRUE).
The source position of the symbol (which may differ from the decl_position
in symbol_ptr if this is a second declaration) is given by *source_pos.
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
  a_variable_ptr                    vp = NULL;
  a_type_ptr                        vp_type = NULL;
  a_boolean                         var_err, init_err;
  a_boolean                         static_lifetime;
  a_constant_ptr                    init_con = NULL;
  a_dynamic_init_ptr                init_dip = NULL;
  a_class_symbol_supplement_ptr     cssp = NULL;
  a_boolean                         nonconstant_allowed;
  a_memory_region_number            region_to_switch_back_to;
  an_object_lifetime_ptr            local_static_lifetime = NULL;
  a_local_static_variable_init_ptr  local_static_var_init = NULL;

  db_enter(3, "initializer");
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
         again, and we have the variable from the earlier declaration). */
      pos_sy_error(ec_already_initialized, source_pos, symbol_ptr);
      var_err = TRUE;
    } else {
      /* Only object types (except for VLAs) and incomplete arrays are
         allowed to be initialized. */
      if (is_object_type(vp_type)) {
        /* Object type -- okay. */
      } else if (is_array_type(vp_type) &&
                 !is_incomplete_type(array_element_type(vp_type))) {
        /* Array type.  The is_incomplete_type test disallows arrays of
           incomplete struct/unions (which in C are possible as an
           extension). */
      } else if (is_reference_type(vp_type)) {
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
    if (is_incomplete_type(symbol_ptr->parent.class_type)) {
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
      push_class_reactivation_scope(symbol_ptr->parent.class_type,
                                    /*extend_namespace=*/TRUE);
    }  /* if */
  } else {
    if (symbol_ptr->parent.namespace_ptr != NULL) {
      push_namespace_reactivation_scope(symbol_ptr->parent.namespace_ptr);
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
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(vp_type)) {
    cssp = symbol_supplement_for_class(vp_type);
    if (curr_token == tok_lbrace &&
        !(cssp->is_class_aggregate ||
          skip_typerefs(vp_type)->
                               variant.class_struct_union.is_nonreal_class)) {
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
  /* Now process the initializer.  There are three cases:  parenthesized
     initializer (C++ only), brace-enclosed initializer list, and simple
     initializer.  These are handled in turn. */
  if (parenthesized_initializer) {
    /* Either this is an initialization of the form S x (arg [, ...]), where
       S is a class type name or an initialization of a scalar like int i(0).
       This form of initialization is allowed in C++ mode only.  Note that
       the opening parenthesis has already been scanned in the caller. */
    a_boolean  dependent_class_type = could_be_dependent_class_type(vp_type);
    if ((cssp != NULL && cssp->constructor != NULL) || dependent_class_type) {
      /* It's a class type and there's a constructor or we're dealing with a
         dependent type that could be such a class. */
      /* Depending on the arguments present, a constructor, possibly the copy
         constructor, will be selected and returned. */
      a_source_position  pos;

      /* Use the source position of the first argument as the call position. */
      pos = pos_curr_token;
      if (dependent_class_type) {
        scan_dependent_type_parenthesized_initializer(
                                  /*force_object_lifetime=*/FALSE, &init_dip);
      } else {
        scan_class_parenthesized_initializer(vp_type, vp_type,
                                             /*force_object_lifetime=*/FALSE,
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
            scan_initializer_of_simple_object(nonconstant_allowed,
                                              static_lifetime,
                                              /*force_object_lifetime=*/FALSE,
                                              /*is_copy_initialization=*/
                                                                        FALSE,
                                              vp_type, &init_dip);
      /* The closing right paren will not have been consumed, as it is
         the arg list for a constructor call is scanned, so bypass it
         explicitly. */
      remove_stop_token(tok_rparen);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (curr_token == tok_rparen && decl_pos_block != NULL) {
        decl_pos_block->var_init_range.end = pos_curr_token;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      check_closing_paren_after_expr_list();
    }  /* if */
  } else if (is_aggregate_or_union_type(vp_type) ||
             (curr_token == tok_lbrace &&
              (is_error_type(vp_type) || is_template_param_type(vp_type)))) {
    /* Either a brace enclosed list of initializers or other aggregate
       initialization. */
    if (curr_token != tok_lbrace && is_class_struct_union_type(vp_type) &&
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
      if (scan_class_initializer_expression(vp_type, &init_dip)) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->var_init_range.end = curr_construct_end_position;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      } else {
        /* No appropriate constructor was found.  Abort the initialization. */
        init_err = TRUE;
      }  /* if */
    } else if (gcc_mode && curr_token != tok_lbrace && static_lifetime) {
      /* In GNU C mode, a compound literal is treated as a constant-expression
         that can initialize a variable with a static lifetime.  We may also
         arrive here when the initializer is a (possibly parenthesized) string
         literal. */
      a_constant  constant;
      scan_constant_initializer_expression(vp_type, &constant);
      init_con = alloc_unshared_constant(&constant);
      if (!var_err && vp != NULL && is_incomplete_type(vp->type)) {
        /* An array of unspecified size is initialized with a constant that
           has a known number of elements: adjust the variable type. */
        a_type_ptr  array_type = skip_typerefs(vp->type);
        check_assertion(is_array_type(array_type) &&
                        is_array_type(constant.type));
        set_initialized_array_size(&array_type,
                                   constant.type->variant.array.
                                                  variant.number_of_elements);
        vp->type = array_type;
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
      if (scan_initializer_list(&vp_type, vp, static_lifetime, &init_con,
                                &init_dip, source_pos, decl_pos_block)) {
        /* The scan was successful. */
        if (!var_err) {
          /* Copy the type back into the variable.  It might have been changed
             if vp is an incomplete array. */
          if (vp != NULL && vp_type != vp->type) {
            put_type_back_into_variable(vp, symbol_ptr, source_pos, linkage,
                                        vp_type);
          }  /* if */
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
    init_con = simple_initializer(static_lifetime, vp_type, &init_dip,
                                  decl_pos_block);
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
        dtor = select_destructor(vp_type, vp_type, source_pos,
                                 /*honor_virtual=*/FALSE, /*evaluated=*/TRUE);
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
                                 source_pos, &init_stmt);
#if DO_IL_LOWERING
#if LOWER_DESIGNATED_INITIALIZERS
      if (designators_allowed &&
          (init_dip->kind == (a_dynamic_init_kind)dik_constant ||
           init_dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate)) {
        /* Rewrite designated initializers into standard C89. */
        lower_designated_initializers(init_dip->variant.constant);
      }  /* if */
#endif /* LOWER_DESIGNATED_INITIALIZERS */
#endif /* DO_IL_LOWERING */
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
      check_assertion(!in_file_scope(init_con));
      if (init_con->kind == (a_constant_repr_kind)ck_aggregate
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
          || init_con->expr != NULL
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
                                   ) {
        /* Aggregate-constant initialization.  Since the aggregate constant
           is in the local memory region, the variable can't have a pointer
           to it.  Instead, create a local-static-variable-init entry to point
           to the initializer -- it is added to a list associated with the
           current function or block scope.  A similar problem exists when
           we record the expression forming the constant: the expression is
           allocated in the local memory region and should not have a pointer
           to it from file scope memory. */
        local_static_var_init =
              make_local_static_variable_init(vp, (a_scope_ptr)NULL,
                                              (an_init_kind)initk_static,
                                              init_con,
                                              (a_dynamic_init_ptr)NULL);
#if DO_IL_LOWERING
#if LOWER_DESIGNATED_INITIALIZERS
        if (designators_allowed) {
          /* Rewrite designated initializers into standard C89. */
          lower_designated_initializers(init_con);
        }  /* if */
#endif /* LOWER_DESIGNATED_INITIALIZERS */
#endif /* DO_IL_LOWERING */
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
#if DO_IL_LOWERING
#if LOWER_DESIGNATED_INITIALIZERS
      if (designators_allowed) {
        /* Rewrite designated initializers into standard C. */
        lower_designated_initializers(init_con);
      }  /* if */
#endif /* LOWER_DESIGNATED_INITIALIZERS */
#endif /* DO_IL_LOWERING */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      vp->initializer_range = decl_pos_block->var_init_range;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (symbol_ptr->is_class_member) {
    /* The initializer of a static data member was scanned with the original
       class reactivated (if we're parsing a prototype instantiation, this was
       done elsewhere).  Restore the scope to what it was before.  (We may
       also end up here with an sk_variable.) */
    /* Note that this call has to be after the select_destructor call in the
       preceding section of code. */
    if (is_incomplete_type(symbol_ptr->parent.class_type)) {
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
    if (symbol_ptr->parent.namespace_ptr != NULL) {
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
type.  Such initialization is required whenever the class has a constructor;
the default constructor (if one exists) is called.
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
    if (cssp != NULL && !cssp->is_POD &&
        var->storage_class != (a_storage_class)sc_extern &&
        !is_incomplete_type(var_type)) {
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        /* Perform the default initialization of a static data member with
           its parent class reactivated. */
        push_class_reactivation_scope(sym->parent.class_type,
                                      /*extend_namespace=*/TRUE);
      } else {
        if (exceptions_enabled && static_lifetime &&
            depth_innermost_function_scope != NO_SCOPE_DEPTH) {
          /* This is the initialization of a local static variable.  Push
             a block lifetime around the entire initialization. */
          push_object_lifetime((an_il_entry_kind)iek_none, (char *)NULL,
                               (an_object_lifetime_kind)olk_block);
          local_static_lifetime = curr_object_lifetime;
        }  /* if */
        if (sym->parent.namespace_ptr != NULL) {
          push_namespace_reactivation_scope(sym->parent.namespace_ptr);
        }  /* if */
      }  /* if */
      /* Find a default constructor. */
      if (cssp->constructor != NULL) {
        /* There are user-declared constructor(s) and/or implicitly-declared
           nontrivial constructors.  Look for a default constructor. */
        ctor = select_default_constructor(tp, err_pos, tp,
                                          /*evaluated=*/TRUE);
        if (is_const && ctor != NULL && ctor->compiler_generated) {
          /* A default constructor was found, but it isn't a user-declared
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
        /* The class has no non-trivial constructors. */
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
          if (reference_to_trivial_default_constructor(tp, err_pos)) {
            def_init_performed = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      dtor = select_destructor(tp, tp, err_pos,
                               /*honor_virtual=*/FALSE, /*evaluated=*/TRUE);
      if (ctor == NULL && dtor == NULL && !is_nonreal_class) {
        /* No constructor for default initialization; no destructor either. */
      } else {
        if (ctor != NULL) {
          /* Normal case -- there's a constructor to do the initialization. */
          init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          init_dip->variant.constructor.ptr = ctor;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          copy_ctor_default_args_to_dynamic_init(init_dip);
          if (var_type != tp) {
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
             of the destructor can be duly recorded. */
          init_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        }  /* if */
        /* A constructor (or at least a destructor) was found and a dynamic
           init entry (local_di) was set to represent the initialization. */
        init_dip->destructor = dtor;
        /* Allocate a dynamic init entry (a copy of local_di) and attach it
           to the variable. */
        gen_dynamic_initialization(var, init_dip, &local_static_var_init,
                                   err_pos, (a_statement_ptr *)NULL);
#if DEBUG
        if (debug_level >= 3 || db_flag_is_set("dump_init")) {
          db_variable(var);
          fputs(",\n", f_debug);
          db_initializer(var, 2);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        pop_class_reactivation_scope();
      } else {
        /* If an object lifetime was pushed to surround the initialization of
           a local static variable, pop it now. */
        if (local_static_lifetime != NULL) {
          pop_object_lifetime_for_local_static_init(local_static_lifetime,
                                                    local_static_var_init,
                                                    /*err=*/FALSE);
        }  /* if */
        if (sym->parent.namespace_ptr != NULL) {
          pop_namespace_reactivation_scope();
        }  /* if */
      }  /* if */
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
  a_type_ptr class1 = field1->source_corresp.parent.class_type;

  /* Work up from each field looking at the parent classes.  Find the
     innermost class/struct/union that the fields have in common. */
  for (;;) {
    a_type_ptr class2 = field2->source_corresp.parent.class_type;
    for (;;) {
      if (class2 == class1) {
        /* We've found the innermost class/struct/union that the
           fields have in common.  If it is a union, they conflict. */
        are_disjoint_members = (class1->kind == (a_type_kind)tk_union);
        goto end_of_routine;
      }  /* if */
      if (!class2->source_corresp.is_class_member) break;
      class2 = class2->source_corresp.parent.class_type;
    }  /* for */
    check_assertion(class1->source_corresp.is_class_member);
    class1 = class1->source_corresp.parent.class_type;
  }  /* for */
end_of_routine:
  return are_disjoint_members;
}  /* are_disjoint_members_of_union */


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
  class_type = ctor_rout->source_corresp.parent.class_type;
  check_assertion(class_type != NULL);
  ctsp = class_type->variant.class_struct_union.extra_info;
  is_generated_cctor = !user_defined &&
                       is_copy_constructor(ctor_rout, class_type,
                                           &required_qualifiers,
                                           /*is_declarative_context=*/TRUE);
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
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && (field->get_property_name != NULL ||
                             field->put_property_name != NULL)) {
        /* Property fields are not really data members and should not be
           initialized. */
        continue;
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not add code here. */
      if (is_generated_cctor) {
        /* All fields are explicitly listed for a generated copy constructor,
           since even if there is no constructor at least a bitwise copy is
           required. */
      } else {
        /* This is not a copy constructor.  See if this is a field that
           requires an initializer. */
        tp = field->type;
        if (is_reference_type(tp) || is_const_qualified_type(tp)) {
          /* Ref-type fields and const and array-of-const fields require an
             initializer. */
        } else {
          if (is_array_type(tp)) tp = underlying_array_element_type(tp);
          tp = skip_typerefs(tp);
          if (is_class_struct_union_type(tp)) {
            cssp = symbol_supplement_for_class(tp);
            if (cssp->constructor != NULL) {
              /* If the mem-initializer is omitted for this field, a
                 default constructor will have to be called. */
            } else if (cssp->trivial_default_constructor != NULL) {
              /* If the mem-initializer is omitted for this field, the
                 definition of the trivial default constructor will be
                 generated, though only in case there are diagnostics. */
            } else if (exceptions_enabled && cssp->destructor != NULL) {
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
    /* User-specified initializers are present.  Bypass the colon. */
    (void)get_token();
    add_stop_token(tok_lbrace);
    /* Loop through the comma-separated list of initializers. */
    do {
      a_boolean          template_param_init = FALSE;
      a_boolean          dependent_class_init = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_source_position  init_start_pos;

      init_start_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      new_cip = NULL;
      array_type = NULL;
      add_stop_token(tok_comma);
      /* Unless this is an old style base class initializer, a base class
         name or a member name is expected. */
      if (curr_token != tok_lparen &&
          !is_decl_qualified_name_start()) {
        /* Either an identifier or "::" is expected here. */
        syntax_error(ec_exp_identifier);
      } else {
        bcp = NULL;
        dip = NULL;
        if (curr_token == tok_lparen) {
          /* Old-style base class initializer.  It is assumed to apply the
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
              /* A mem-initializer for a field: */
              dependent_class_init = could_be_dependent_class_type(
                                 member_or_base_sym->variant.field.ptr->type);
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
            } else if (is_class_symbol(member_or_base_sym) &&
                       member_or_base_sym->parent.class_type == class_type) {
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
        record_symbol_reference(SRK_REFERENCE | SRK_INITIALIZATION,
                                member_or_base_sym, &error_position,
                                /*update_il_entry=*/FALSE);
        if (member_or_base_sym->kind == (a_symbol_kind)sk_field &&
            member_or_base_sym->parent.class_type == class_type) {
          /* This is a field of the current class and may be mentioned in the
             constructor's initializer list.  But it's an error to refer to
             it by a qualified name. */
          a_field_ptr field = member_or_base_sym->variant.field.ptr;
          if (locator_for_curr_id.is_qualified_name) {
            pos_error(ec_qualified_name_not_allowed,
                      &locator_for_curr_id.source_position);
          }
#if MICROSOFT_EXTENSIONS_ALLOWED
          else if (microsoft_mode && (field->get_property_name != NULL ||
                                      field->put_property_name != NULL)) {
            /* Property fields cannot be mentioned in a constructor
               initializer list. */
            pos_error(ec_property_name_not_allowed,
                      &locator_for_curr_id.source_position);
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          init_type = field->type;
          if (is_array_type(init_type) && !is_string_type(init_type)) {
            /* Arrays can be default-initialized if the expression-list is
               omitted. */
            array_type = init_type;
            init_type = f_skip_typerefs(
                             underlying_array_element_type(init_type));
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
                   assume new_cip->kind is cik_field. */
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
              if (bcp->type == init_type) {
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
                 - A reference to a class type that might be a virtual base of
                   a dependent base.
                 For these case, we make up a nonvirtual base class node. */
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
            /* new_cip was initially marked as compiler-generated. Reset the
               flag now that it's appeared explicitly in the ctor-initializer
               list. */
            new_cip->compiler_generated = FALSE;
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, bcp->type);
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
                                        /*force_object_lifetime=*/TRUE, &dip);
            } else {
              a_type_ptr        object_class_type;
  
              /* If it is a base class, the object being constructed is the
                 whole class (and the base class is a subobject thereof).
                 If it is a field, the object being constructed is field
                 itself.  Set the object class type accordingly. */
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
                                           /*force_object_lifetime=*/TRUE,
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
              detach_from_object_lifetime_tree(init_expr_lifetime_of(dip));
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
                                         init_type, &error_position)) {
            /* We fake a call to the trivial default constructor for the
               class.  No call is actually made, but the constructor
               definition is triggered (in case there are side-effects).
               Note that this is a so-called "value-initialization" case
               and hence the object must be zeroed. */
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
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
              if (is_reference_type(init_type)) {
                /* Error.  A reference type may not be default-initialized
                   (8.5 [dcl.init], which says it's a no-op, and 8.5.3
                   [dcl.init.ref], which says that the initializer must
                   be an object. */
                a_constant_ptr  cp;

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
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_zero);
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
              add_stop_token(tok_rparen);
              if (array_type != NULL) {
                /* Arrays can only be default- or value-initialized -- i.e.,
                   the expression-list must be omitted. */
                sym_error(ec_array_member_initialization, member_or_base_sym);
                /* Set the initializer field to record that an initialization
                   was attempted. */
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
                flush_to_end_of_arg_list();
              } else {
                /* Allocate a new dynamic init entry, setting the kind to
                   dik_none for now.  It will be adjusted after the scan. */
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
                (void)scan_initializer_of_simple_object(
                                                /*nonconst_allowed=*/TRUE,
                                                /*static_lifetime=*/FALSE,
                                                /*force_object_lifetime=*/TRUE,
                                                /*is_copy_initialization=*/
                                                                         FALSE,
                                                init_type, &dip);
                /* If the initializer produced an object lifetime for the full
                   expression, remove it temporarily from the object lifetime
                   tree and restore it in the correct position later. */
                detach_from_object_lifetime_tree(init_expr_lifetime_of(dip));
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
                  if (curr_token == tok_comma) flush_to_end_of_arg_list();
                }  /* if */
              }  /* if */
              remove_stop_token(tok_rparen);
              if (curr_token == tok_rparen) (void)get_token();
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
          if (dip->kind == (a_dynamic_init_kind)dik_expression) {
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
         all that's required for explicit initializations. */
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
          rp = select_copy_constructor(tp,
                                       required_qualifiers | object_qualifiers,
                                       &err_pos, object_class_type,
                                       &bitwise_copy, /*evaluated=*/TRUE);
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
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = rp;
          /* No expression node is created to represent the subobject.  The
             back end will compute the subobject's address based on the
             base class or field just as it will compute the address of the
             implicit "this" parameter, which is the address of the subobject
             to be initialized by the copy. */
          dip->variant.constructor.
                            is_copy_constructor_with_implied_source = TRUE;
          /* We need to copy the default arg expressions of the second and
             subsequent parameters (if any) of the copy constructor.  The
             first param is ignored even if it is declared to have a default
             arg. */
          copy_ctor_default_args_to_dynamic_init(dip);
        }  /* if */
      } else {
        /* No copy constructor is required.  If any constructor exists, the
           default constructor should be called. */
        if (cip->kind == (a_constructor_init_kind)cik_field &&
            (is_reference_type(tp) || is_const_qualified)) {
          /* Ref-type field or const-qualified field but no initializer. */
          if (is_union_type(class_type)) {
            /* We don't issue diagnostics on initializing union members,
               partly because it's not well defined what should happen when
               const and non-const members are mixed, */
          } else if (is_const_qualified && cssp != NULL &&
                     cssp->constructor != NULL) {
            /* A const qualified field may be initialized without an explicit
               initializer it is of class type and there is a default
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
            if (is_reference_type(tp)) any_ref_member_on_uninit_list = TRUE;
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
            (void)reference_to_trivial_default_constructor(tp, &err_pos);
          }  /* if */
        }  /* if */
        if (cssp == NULL ||
            (cssp->constructor == NULL &&
             (!exceptions_enabled || cssp->destructor == NULL))) {
          /* This constructor initializer entry is not really needed.  It may
             be the result of an empty initializer on a field or it may be
             associated with a base class without a constructor.  Unlink it
             from the list. */
          if (prev_cip == NULL) {
            cip_list = cip->next;
          } else {
            prev_cip->next = cip->next;
          }  /* if */
          continue;
        }  /* if */
        if (cssp->constructor == NULL) {
          rp = NULL;
        } else {
          rp = select_default_constructor(tp, &err_pos, object_class_type,
                                          /*evaluated=*/TRUE);
        }  /* if */
        if (rp == NULL) {
          /* Error in trying to find a default constructor. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A default constructor does exist.  Generate the dynamic init
             entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          copy_ctor_default_args_to_dynamic_init(dip);
        }  /* if */
      }  /* if */
      /* Attach the new dynamic init entry to the constructor initializer. */
      dip->is_constructor_init = TRUE;
      cip->initializer = dip;
    }  /* if */
    /* Do processing for both implicitly and explicitly initialized members
       when exception handling is enabled. */
    if (exceptions_enabled) {
      if (cssp != NULL && cssp->destructor != NULL) {
        /* Since an exception could be thrown after this subobject is
           constructed but before construction of the entire object is
           complete, record the destructor in the dynamic-init entry. */
        if (dip->destructor != NULL) {
          /* This must be an entry for an explicit initialization, for which
             the destructor will already have been filled in. */
        } else {
          /* Implicit initialization -- the destructor has not yet been
             looked up. */
          dip->destructor = select_destructor(tp, object_class_type, &err_pos,
                                              /*honor_virtual=*/FALSE,
                                              /*evaluated=*/TRUE);
        }  /* if */
        /* Record the need for a destruction in the context of the current
           lifetime.   Note: when the field is an array, it is the dynamic
           init entry for the array element that is being handled at this
           time; the array as a whole is dealt with below. */
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
         involved, a discretionary error is issued. */
      if (!any_ref_member_on_uninit_list) severity = es_discretionary_error;
      pos_sy_start_diagnostic(severity, ec_missing_initializer_on_fields,
                              &pos_curr_token, (a_symbol_ptr)ctor_rout->
                                                   source_corresp.assoc_info);
    }  /* if */
    for (cip = uninit_list; cip != NULL; cip = cip->next) {
      a_symbol_ptr field_sym = (a_symbol_ptr)cip->variant.field->
                                                   source_corresp.assoc_info;
      if (is_reference_type(cip->variant.field->type)) {
        sym_add_diag_info(ec_reference_member, field_sym);
      } else {
        /* Must be a const member. */
        sym_add_diag_info(ec_const_member, field_sym);
      }  /* if */
    }  /* for */
    end_error();
  }  /* if */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  if (ctor_rout->is_trivial_default_constructor) {
    /* IL will not be put out for a trivial default constructor anyway, so
       there's no need to deal with default operator new. */
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
        set_class_assoc_operator_delete_routine(class_type,
                                                (a_routine_ptr)NULL);
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
  class_type = dtor_rout->source_corresp.parent.class_type;
  check_assertion(class_type != NULL);
  ctsp = class_type->variant.class_struct_union.extra_info;
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
        rp = select_destructor(bcp->type, class_type, &source_pos,
                               /*honor_virtual=*/FALSE, /*evaluated=*/TRUE);
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
  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  for (sym = class_sym->variant.class_struct_union.extra_info->symbols;
       sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* sym represents a field.  Determine whether a destructor exists. */
      tp = sym->variant.field.ptr->type;
      /* For arrays get the element type, allowing for multidimensional
         arrays. */
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      tp = skip_typerefs(tp);
      if (is_immediate_class_type(tp)) {
        rp = select_destructor(tp, tp, &source_pos,
                               /*honor_virtual=*/FALSE, /*evaluated=*/TRUE);
        if (rp != NULL) {
          /* Create the constructor init entry for a field. */
          cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
          cip->variant.field = sym->variant.field.ptr;
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
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
  { a_routine_ptr delete_routine;
    /* Determine and remember the default operator delete() routine for the
       class.  This is done here because we are working out the "wrapper"
       code that will be required, and the "delete" routine will be called from
       the wrapper. */
    set_class_assoc_operator_delete_routine(class_type, dtor_rout);
    delete_routine = ctsp->assoc_operator_delete_routine;
    if (delete_routine != NULL) {
      mark_routine_referenced(delete_routine);
      delete_routine->called = TRUE;
    }  /* if */
  }
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("dump_init")) {
    db_symbol((a_symbol_ptr)dtor_rout->source_corresp.assoc_info,
              "destructor: ", 2);
    for (cip = cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = (a_symbol_ptr)cip->variant.field->source_corresp.assoc_info;
      } else {
        sym = (a_symbol_ptr)cip->variant.base_class->type->
                                                source_corresp.assoc_info;
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
  if (is_reference_type(type)) {
    /* Note that a reference type object cannot be produced by new. */
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
              if (is_class_struct_union_type(type)) {
                /* MSVC++ does not require an initializer for a const class
                   variable with no default constructor. */
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
            if (is_class_struct_union_type(type) && !is_incomplete_array &&
                !any_cfront_mode() && !microsoft_mode) {
               /* Even if the class has an implicitly declared default
                  constructor, a user-declared default constructor must be
                  present (WP 7.1.5.1 [dcl.cv]). */
              check_assertion(
                      !type_has_user_declared_default_constructor(type));
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
          check_assertion(!type_has_user_declared_default_constructor(type));
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
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
