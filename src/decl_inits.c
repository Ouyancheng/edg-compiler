/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decl_inits.c -- Scanning of initializers in declarations.

*/

#include "basics.h"
#include "decl_inits.h"
#include "decls.h"
#include "il.h"
#include "symbol_tbl.h"
#include "error.h"
#include "types.h"
#include "expr.h"
#include "lexical.h"
#include "statements.h"
#include "cmd_line.h"


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
  a_type_ptr array_type;

  array_type = alloc_type((a_type_kind)tk_array);
  copy_type(*type, array_type);
  array_type->variant.array.number_of_elements = size;
  set_type_size(array_type);
  *type = array_type;
}  /* set_initialized_array_size */


static void check_string_constant_initializer(a_constant *constant,
                                              a_type_ptr *type,
                                              a_boolean  *err)
/*
Check that the given string constant is acceptable as an initial value
for the string type *type.  Change the constant's type, or remove the
final null from a string literal, if necessary.  If *type is an incomplete
type, change it to reflect the actual size of the string literal.
(Note the extra level of indirection that allows that.)  Issue an error
and return *err TRUE if there is an error of some kind.
*/
{
  a_type_ptr    array_type;
  a_targ_size_t string_length, num_elems;
  a_targ_size_t array_length;
  a_boolean     is_wide_string = FALSE;

  *err = FALSE;
  if (is_string_type(*type)) {
    /* The object to be initialized is an array (possibly incomplete) of
       char or wchar_t -- i.e., a string or wide string. */
    is_wide_string = !is_char_array_type(*type);
    if (constant->kind != (a_constant_repr_kind)ck_string) {
      /* The constant is not a string. */
      *err = TRUE;
    } else if (char_int_kind_from_string_type(*type) !=
               char_int_kind_from_string_type(constant->type)) {
      /* The constant and the array do not have the same underlying character
         element type; it must be that one is a wide string and the other
         a normal string.  Note that there is no mismatch in that case if
         wchar_t is char. */
      *err = TRUE;
    } else {
      /* The constant is a string. */
      num_elems = string_length = constant->variant.string.length;
      if (is_wide_string) {
        /* Adjust the wide string number of elements. */
        num_elems /= TARG_SIZEOF_WCHAR_T;
      }  /* if */
      array_type = skip_typerefs(*type);
      if (is_incomplete_type(array_type)) {
        /* The array type is incomplete, and therefore the array size
           is set from the string length. */
        set_initialized_array_size(type, num_elems);
      } else {
        /* The object being initialized is an array that has a definite
           size.  See if the string will fit in the array. */
        array_length = array_type->variant.array.number_of_elements;
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
              constant->type = string_type(num_elems);
            } else {
              string_length -= TARG_SIZEOF_WCHAR_T;
              constant->type = wide_string_type(num_elems);
            }  /* if */
            constant->variant.string.length = string_length;
          } else {
            /* The initializer string is too long for the array being
               initialized. */
            *err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (*err) {
      /* There was an error of some kind. */
      error(ec_bad_initializer_type);
      set_error_constant(constant);  
    }  /* if */
  }  /* if */
}  /* check_string_constant_initializer */


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
  a_stop_token_array save_stop_token_array;

  /* Note:  db_enter/db_exit CANNOT be called, because this routine
     exits with the stop tokens set different than on entry. */
  if (flag) {
    if (curr_token == tok_rbrace) {
      /* The brace is there.  Skip over it. */
      (void)get_token();
    } else {
      /* Error, the brace is not there.  Change the stop tokens set to
         just skip to a right brace (flush_tokens has some other "hard"
         tokens wired in), then record an error and flush tokens. */
      copy_stop_tokens(stop_token_array, save_stop_token_array);
      clear_stop_tokens();
      (void)required_token(tok_rbrace, ec_exp_rbrace);
      /* Restore the stop token set as at entry. */
      copy_stop_tokens(save_stop_token_array, stop_token_array);
    }  /* if */
    remove_stop_token(tok_rbrace);
  }  /* if */
}  /* check_for_matching_closing_brace */


static void init_remaining_array_elements(a_type_ptr          array_type,
                                          a_targ_size_t       curr_element,
                                          a_constant_ptr      *con_list,
                                          a_constant_ptr      *end_of_con_list,
                                          a_dynamic_init_ptr  *di_list,
                                          a_dynamic_init_ptr  *end_of_di_list)
/*
This routine is called from get_initializer when an array whose
elements require constructor initialization (and/or destruction by
calling a destructor) has been only partially initialized.  The
remaining elements of the array receive initialization by the default
constructor.  array_type is a pointer to the type entry for the array
object.  curr_element identifies the next element to be initialized.
*con_list and *di_list are list of constant entries and dynamic init
entries (respectively) that represent the initialization of the array;
*end_of_con_list and *end_of_di_list point to the terminal entries on
the two list.
*/
{
  a_type_ptr                     element_type;
  a_targ_size_t                  number_of_uninitialized_elements;
  a_constant_ptr                 cp, repeat_con;
  a_routine_ptr                  ctor_rp;
  a_class_symbol_supplement_ptr  cssp;
  a_param_type_ptr               ptp;
  a_dynamic_init_ptr             dip;

  db_enter(4, "init_remaining_array_elements");

  number_of_uninitialized_elements =
                  array_type->variant.array.number_of_elements - curr_element;
  if (number_of_uninitialized_elements > 0) {
    /* There are one or more uninitialized elements. */
    element_type = array_element_type(array_type);
    if (is_class_struct_union_type(element_type)) {
      /* It is an array of class objects. */
      cssp = symbol_supplement_for_class(element_type);
      if (cssp->constructor != NULL || cssp->destructor != NULL) {
        /* Initialization is required. */
        if (cssp->constructor != NULL) {
          /* Get the default constructor.  Note that it is an error if it
             is missing. */
          ctor_rp = select_default_constructor(element_type, &pos_curr_token);
        }  /* if */
        if (ctor_rp != NULL) {
          /* If there's a constructor routine create a dik_constructor
             dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.routine = ctor_rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          ptp = (skip_typerefs(ctor_rp->type))->
                                   variant.routine.extra_info->param_type_list;
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
        } else {
          /* If there's no constructor routine we use a dik_none dynamic
             init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        }  /* if */
        /* Register the destructor if there's one there. */
        dip->destructor = select_destructor(element_type);
        /* Now create the constant entry that will point to the new dynamic
           init entry. */
        cp = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
        cp->variant.dynamic_init = dip;
        if (number_of_uninitialized_elements > 1) {
          /* When there is more than one unitialized element remaining in the
             array, we put out an init_repeat constant on top of the
             dynamic init constant. */
          repeat_con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
          repeat_con->variant.init_repeat.count = 
                                             number_of_uninitialized_elements;
          repeat_con->variant.init_repeat.constant = cp;
          cp = repeat_con;
        }  /* if */
        /* Add the constant entry to the list of constants. */
        if (*con_list == NULL) {
          *con_list = cp;
        } else {
          (*end_of_con_list)->next = cp;
        }  /* if */
        *end_of_con_list = cp;
        /* Add the dynamic init entry to its list. */
        if (*di_list == NULL) {
          *di_list = dip;
        } else {
          (*end_of_di_list)->next = dip;
        }  /* if */
        *end_of_di_list = dip;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* init_remaining_array_elements */


static void scan_initializer_of_simple_object(a_boolean       nonconst_allowed,
                                              a_type_ptr      type,
                                              a_dynamic_init  *dip)
/*
Scan a initializer for a non-aggregate object (i.e., not an array and not
a class/struct/union object).  If nonconst_allowed is TRUE (always the case
in C++, sometimes otherwise) a nonconstant expression is allowed; if not,
a constant is required.  type is the data type of the object being
initialized.  *dip is the dynamic init entry to be updated, even in the case
of constant initializers.
*/
{
  an_expr_node_ptr expression;
  a_boolean        is_constant;
  a_constant       constant;

  if (nonconst_allowed) {
    /* Scan a potentially non-constant initializer expression.  The result
       of the scan is a constant if the expression is constant, and an
       expression node if not. */
    scan_initializer_expression(type, &is_constant, &expression, &constant);
  } else {
    /* Non-constant is not allowed. */
    scan_constant_initializer_expression(type, &constant);
    is_constant = TRUE;
  }  /* if */
  /* See if the scanned expression was constant or not. */
  if (is_constant) {
    /* Constant. */
    /* Set the dynamic init entry to represent constant initialization.
       (A local dynamic init entry is used only for convenience --
       dynamic initialization is not presumed.) */
    clear_dynamic_init(dip, (a_dynamic_init_kind)dik_constant);
    dip->variant.constant = alloc_unshared_constant(&constant);
  } else {
    /* Non-constant. */
    /* Set the dynamic init entry to represent non-constant assignment
       initialization. */
    clear_dynamic_init(dip, (a_dynamic_init_kind)dik_expression);
    dip->variant.expression = expression;
  }  /* if */
}  /* scan_initializer_of_simple_object */


static a_constant_ptr get_initializer(a_type_ptr          *type,
                                      a_dynamic_init_ptr  *di_list,
                                      a_dynamic_init_ptr  *end_of_di_list,
                                      a_boolean           top_level)
/*
Scan a constant initializer or initializer list, and return a pointer to
the constant for it (an aggregate constant if an initializer list is
scanned).  *type indicates the type of the object being initialized.
It will be updated if the object is an incomplete array whose size is
now known because it is initialized.  If there is some error in the
initializer, an error constant is returned.  top_level is TRUE if this
is a top-level initializer (braces are required surrounding initializers
for unions and aggregates at that level).
*/
{
  a_constant_ptr      init_con = NULL;
  a_boolean           err = FALSE;
  a_boolean           is_incomplete_array;
  a_type_ptr          local_type, member_type;
  a_boolean           brace_flag;
  a_constant_ptr      con_list, end_of_con_list;
  a_constant_ptr      member_con;
  a_targ_size_t       curr_array_element;
  a_field_ptr         curr_field;
  a_boolean           done, no_more_members;
  a_type_kind         kind;
  a_boolean           array_too_long_error_given = FALSE;
  a_boolean           took_extra_comma;
  an_expr_node_ptr    expression;
  a_dynamic_init_ptr  dip;
  a_routine_ptr       conversion_routine;
  a_boolean           class_bitwise_copy;

  db_enter(4, "get_initializer");
  err = FALSE;
  local_type = skip_typerefs(*type);
  check_for_opening_brace(&brace_flag);
  if (!brace_flag && C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(local_type)) {
#if CHECKING
    a_class_symbol_supplement_ptr cssp =
                                    symbol_supplement_for_class(local_type);
    if (top_level) {
      internal_error("get_initializer: class encountered at top level");
    } else if (!cssp->has_copy_constructor &&
               !cssp->construction_by_bitwise_copy_allowed) {
      internal_error("get_initializer: missing copy constructor");
    }  /* if */
#endif /* CHECKING */
    /* This is an array element that can only be initialized by a
       constructor.  Treat the expression as an argument for the constructor
       call. */
    expression = scan_class_initializer_expression(local_type,
                                                   &conversion_routine,
                                                   &class_bitwise_copy);
    if (!class_bitwise_copy && conversion_routine == NULL) {
      /* No constructor was found.  Abort the initialization. */
      err = TRUE;
    } else {
      init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      if (conversion_routine != NULL) {
        /* An appropriate constructor (copy or other) was found.  Build
           a dynamic init entry to call it. */
        init_con->variant.dynamic_init = dip =
                      alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
        dip->variant.constructor.routine = conversion_routine;
        dip->variant.constructor.args = expression;
      } else {
        /* Generate code for a bitwise copy. */
        init_con->variant.dynamic_init = dip =
                      alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
        dip->variant.expression = expression;
      }  /* if */
      dip->destructor = select_destructor(local_type);
      if (*di_list == NULL) {
        *di_list = dip;
      } else {
        (*end_of_di_list)->next = dip;
      }  /* if */
      *end_of_di_list = dip;
    }  /* if */
  } else if (is_aggregate_or_union_type(local_type) ||
             (is_error_type(local_type) && brace_flag)) {
    /* Initialization of an array (complete or incomplete), struct, or
       union.  The result will be an aggregate constant except when an
       array of char is initialized by an string.  The initial
       values can either appear inside a brace-enclosed list, or at
       the current level. */
#if 0
    check_for_opening_brace(&brace_flag);
#endif /* if 0 */
    if (curr_token == tok_string_literal && is_string_type(local_type)) {
      /* The object being initialized has type array of char or wchar_t, and
         is being initialized with a string.  Handle this case specially. */
      check_string_constant_initializer(&const_for_curr_token, &local_type,
                                        &err);
      if (!err) {
        /* Allocate the string constant. */
        init_con = alloc_unshared_constant(&const_for_curr_token);
        /* Pass the type back to the caller; the array size is now
           known if it was incomplete. */
        *type = local_type;
      }  /* if */
      (void)get_token();
    } else {
      /* Normal case, not array of char.  Could be an array, a struct,
         or a union, or an error type.  Note that local_type has already
         been stripped of typerefs above. */
      done = FALSE;
      is_incomplete_array = FALSE;
      /* In ANSI C, the top-level initializer for a struct, union, or
         array must be surrounded by braces.  e.g., "int a[1] = 1;" is
         not allowed.  However, pcc will allow initialization with
         a single value and we allow it as an extension. */
      if (top_level && !brace_flag) {
        if (strict_ansi_mode) warning(ec_exp_lbrace);
      }  /* if */
      kind = local_type->kind;
      if (kind == (a_type_kind)tk_error) {
        member_type = local_type;
      } else if (kind == (a_type_kind)tk_array) {
        curr_array_element = 0;
        is_incomplete_array = is_incomplete_type(local_type);
        member_type = local_type->variant.array.element_type;
        /* Note that arrays of incomplete struct/union types (an extension)
           do not make it to here (they're caught as an error at the top
           level in the routine "initializer" and replaced by an error type),
           so we don't have to check for them here. */
      } else {
#if CHECKING
        if (!is_immediate_class_type(local_type)) {
          internal_error("get_initializer: not array or class/struct/union");
        }  /* if */
#endif /* CHECKING */
        curr_field = local_type->variant.class_struct_union.field_list;
        done = (curr_field == NULL);
      }  /* if */
      con_list = end_of_con_list = NULL;
      took_extra_comma = FALSE;
      /* Loop, scanning initializers. */
      while (!done) {
        /* Determine the type of the member being initialized. */
        if (kind == (a_type_kind)tk_array || kind == (a_type_kind)tk_error) {
          /* member_type was set outside the loop. */
        } else if (is_immediate_class_type(local_type)) {
          /* Get the type of the current field. */
          member_type = curr_field->type;
#if CHECKING
          /* Members of unions or aggregates cannot be incomplete. */
          if (is_incomplete_type(member_type)) {
            internal_error(
                      "get_initializer: member of aggregate has incomp type");
          }  /* if */
#endif /* CHECKING */
        }  /* if */
        add_stop_token(tok_comma);
        /* Get the initializer for this one member. */
        member_con = get_initializer(&member_type, di_list, end_of_di_list,
                                     /*top_level=*/FALSE);
        remove_stop_token(tok_comma);
        /* Add the constant to the list. */
        if (con_list == NULL) {
          con_list = member_con;
        } else {
          end_of_con_list->next = member_con;
        }  /* if */
        end_of_con_list = member_con;
        /* Advance to the next member. */
        no_more_members = FALSE;
        if (kind == (a_type_kind)tk_error) {
          /* No processing for this case. */
        } else if (kind == (a_type_kind)tk_array) {
          if (curr_array_element == TARG_SIZE_T_MAX) {
            /* Array too long; presumably, this is an incomplete array
               being initialized with a ridiculous number of initial
               values. */
            if (!array_too_long_error_given) {
              error(ec_array_size_too_large);
              array_too_long_error_given = TRUE;
            }  /* if */
          } else {
            /* Advance to next array element. */
            curr_array_element++;
            /* Exit the loop if there are no elements remaining. */
            if (!is_incomplete_array &&
                local_type->variant.array.number_of_elements <=
                                                         curr_array_element) {
              no_more_members = TRUE;
            }  /* if */
          }  /* if */
        } else if (kind == (a_type_kind)tk_class ||
                   kind == (a_type_kind)tk_struct) {
          /* Advance to the next field of the class or struct. */
          curr_field = curr_field->next;
          /* Exit the loop if there are no fields remaining. */
          if (curr_field == NULL) {
            no_more_members = TRUE;
          } else if (curr_field->next == NULL &&
                     is_incomplete_type(curr_field->type)) {
            /* Also exit on an incomplete array at the final field of a
               struct (allowed as an extension, but not allowed to be
               initialized).  This would come up in a case like
                 struct {int i; int j[];} = {0, 0};  <-- Error on 2nd 0.
            */
            no_more_members = TRUE;
          }  /* if */
        } else {
#if CHECKING
          if (kind != (a_type_kind)tk_union) {
            internal_error(
                     "get_initializer: in loop, not array/struct/union");
          }  /* if */
#endif /* CHECKING */
          /* Only the first field in a union is initialized, so having done
             that field, we are done with the union. */
          no_more_members = TRUE;
        }  /* if */
        /* If there are no more members and this is not a brace-enclosed list,
           exit the loop now, without taking a comma or brace following.
           Likewise if this is a top-level list that is not brace-enclosed
           (an error except in pcc mode), end the loop now, having taken only
           one value. */
        if (!brace_flag && (no_more_members || top_level)) break;
        /* Skip a comma separating initializers.  This might be an extra
           comma at the end of the list. */
        done = !loop_token(tok_comma);
        /* Always end the loop upon encountering a right brace.  This might
           be the right brace matching the opening brace for this list
           (in the case that there was one), or a brace closing some
           higher-level list, which nevertheless serves to end this
           lower-level list.  In either case, however, if a comma was
           just taken, it is "extra" and no extra comma should be allowed
           outside the loop. */
        if (curr_token == tok_rbrace) {
          took_extra_comma = !done;
          done = TRUE;
        }  /* if */
        if (!done && no_more_members) {
          /* There are more initializers, but we've run out of members
             into which to put them. */
          error(ec_too_many_initializer_values);
          /* Read the rest of the constants as part of an error type. */
          kind = (a_type_kind)tk_error;
          member_type = error_type();
        }  /* if */
      }  /* while */
      /* The entire list of values for the entity being initialized has
         now been read.  We stopped either because we exhausted the
         initial values or because we ran out of members to initialize. */
      /* Set the size of an incomplete array from the number of elements
         in its initial value.  Note that arrays of char initialized
         to strings are not handled here. */
      if (is_incomplete_array) {
        /* Note that curr_array_element indicates the *next* array element
           to be initialized, and is therefore one larger than the one
           last initialized.  Thus, it is the array size. */
        set_initialized_array_size(&local_type, curr_array_element);
        *type = local_type;
      } else if (C_dialect == C_dialect_cplusplus && brace_flag &&
                 kind == (a_type_kind)tk_array) {
        /* When the number of initializers is fewer than the number of
           array elements to be initialized, and when the element type is
           such that a constructor is required to initialize the elements,
           we are required to provide default initialization by calling
           the default constructor. */
        init_remaining_array_elements(local_type, curr_array_element,
                                      &con_list, &end_of_con_list, di_list,
                                      end_of_di_list);
      }  /* if */
      /* Allocate the aggregate constant that is the value for the
         initializer. */
      init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      init_con->variant.aggregate.first_constant = con_list;
      init_con->variant.aggregate.last_constant  = end_of_con_list;
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
    /* If there was an initial opening brace, check for and skip the
       closing brace now. */
    check_for_matching_closing_brace(brace_flag);
  } else {
    /* Non-aggregate/union case -- initializer is a single (possibly
       brace-enclosed) value. */
    a_dynamic_init  local_di;

#if 0
    check_for_opening_brace(&brace_flag);
#endif /* if 0 */
    scan_initializer_of_simple_object(/*nonconst_allowed=*/
                                            (C_dialect == C_dialect_cplusplus),
                                      local_type, &local_di);
    switch (local_di.kind) {
      case dik_constant:
        init_con = local_di.variant.constant;
        break;
      case dik_expression:
        init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
        init_con->variant.dynamic_init = dip =
                       alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
        dip->variant.expression = local_di.variant.expression;
        if (*di_list == NULL) {
          *di_list = dip;
        } else {
          (*end_of_di_list)->next = dip;
        }  /* if */
        *end_of_di_list = dip;
        break;
#if CHECKING
      default:
        internal_error("get_initializer: bad dynamic init kind");
#endif /* CHECKING */
    }  /* switch */
    /* If there was an initial opening brace, check for and skip the
       closing brace now.  Check also for an extra comma (required in C++
       per ARM 8.4, offered in C along with the extension that permits
       brace-enclosed initializers on non-aggregate variables in the first
       place). */
    if (brace_flag && curr_token == tok_comma) (void)get_token();
    check_for_matching_closing_brace(brace_flag);
  }  /* if */
  /* If the return value constant was not allocated (because of an error),
     allocate an error constant to return. */
  if (init_con == NULL) {
    init_con = alloc_constant((a_constant_repr_kind)ck_error);
    set_error_constant(init_con);
  }  /* if */
  db_exit();
  return(init_con);
}  /* get_initializer */


/* Declaration needed because of mutual recursion: */
static a_boolean dynamic_init_has_side_effects(a_dynamic_init_ptr dip);


static a_boolean init_con_has_side_effects(a_constant_ptr con)
/*
Return TRUE if the indicated constant (part of an initialization)
has side effects.
*/
{
  a_boolean      has_side_effects = FALSE;
  a_constant_ptr subcon;

  if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    /* For aggregates, visit the enclosed constants. */
    for (subcon = con->variant.aggregate.first_constant;
         subcon != NULL;
         subcon = subcon->next) {
      if (init_con_has_side_effects(subcon)) {
        has_side_effects = TRUE;
        break;
      }  /* if */
    }  /* for */
  } else if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
    /* For dynamic init entries, check the dynamic initialization. */
    has_side_effects =
                      dynamic_init_has_side_effects(con->variant.dynamic_init);
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* For a repeat, look at the repeated constant. */
    has_side_effects =
                  init_con_has_side_effects(con->variant.init_repeat.constant);
  }  /* if */
  return has_side_effects;
}  /* init_con_has_side_effects */


static a_boolean dynamic_init_has_side_effects(a_dynamic_init_ptr dip)
/*
Return TRUE if the indicated dynamic initialization has side effects,
i.e., it does something other than just return a value for the initialization.
*/
{
  a_boolean has_side_effects = FALSE;

  if (dip->destructor != NULL) {
    /* A destructor call causes side effects. */
    has_side_effects = TRUE;
  } else {
    switch (dip->kind) {
      case dik_none:
      case dik_constant:
        /* No side effects. */
        break;
      case dik_expression:
        /* An expression might have side effects.  See if it does. */
        has_side_effects = node_has_side_effects(dip->variant.expression);
        break;
      case dik_constructor:
        /* A constructor call causes side effects. */
        has_side_effects = TRUE;
        break;
      case dik_nonconstant_aggregate:
        /* A non-constant aggregate must be examined recursively. */
        has_side_effects =
                  init_con_has_side_effects(dip->variant.aggregate.aggr_const);
        break;
#if CHECKING
      case dik_member_copy:
      case dik_base_class_copy:
      default:
        internal_error("dynamic_init_has_side_effects: bad dyn init kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return has_side_effects;
}  /* dynamic_init_has_side_effects */


static void gen_dynamic_initialization(a_variable_ptr      vp,
                                       a_dynamic_init_ptr  dip,
                                       a_source_position   *source_pos)
/*
Generate a dynamic initialization of the variable vp based on the
dynamic init entry pointed to by dip.  Except for a dynamic
initialization at file scope (possible only in C++), also create an
stmk_init statement at the current point in the code.  *source_pos is
the source position for an error (dynamic initialization is in
unreachable code).
*/
{
  a_dynamic_init_ptr      new_dip;
  a_statement_ptr         init_stmt;
  a_symbol_ptr            assoc_sym;

  db_enter(4, "gen_dynamic_initialization");
  if (depth_stmt_stack >= 0) {
    /* We are in executable code (i.e., inside a function or block rather
       than at file scope). */
    if (dip->kind != (a_dynamic_init_kind)dik_none) {
      /* The initialization is not just a destruction. */
      /* Issue a warning for a dynamic initialization in an unreachable
         block. */
      if (!curr_code_reachable()) {
        pos_warning(ec_initialization_not_reachable, source_pos);
      }  /* if */
    }  /* if */
    /* If this dynamic init appears after some executable code
       in its block, set a flag to that effect in the dynamic
       init entry (it identifies the initialization as a C++ case). */
    if (struct_stmt_stack[depth_stmt_stack].any_exec_statement_seen) {
      dip->follows_an_exec_statement = TRUE;
    }  /* if */
  }  /* if */
  /* Build the dynamic initialization entry. */
  new_dip = alloc_dynamic_init(dip->kind);
  *new_dip = *dip;
  /* Attach the dynamic initialization entry to the scope list. */
  add_to_dynamic_inits_list(new_dip);
  /* Make the variable point at the dynamic initialization. */
  vp->init_kind = (an_init_kind)initk_dynamic;
  vp->initializer.dynamic = new_dip;
  new_dip->variable = vp;
  if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
    /* A dynamic file-scope initialization (possible only in C++) has
       no associated stmk_init statement. */
  } else {
    /* Build the initialization statement. */
    init_stmt = add_statement((a_statement_kind)stmk_init);
    init_stmt->seq_number = vp->source_corresp.decl_position.seq;
    init_stmt->variant.dynamic_init = new_dip;
  }  /* if */
  /* Mark all dynamically initialized variables as referenced.  (They are
     "referenced" in the sense that a variable assigned to, even if never
     used, is referenced.)  It is especially important not to leave the
     referenced flag unset when the initialization (e.g., by constructor)
     may have side effects. */
  vp->source_corresp.referenced = TRUE;
  /* Also set the referenced flag in the associated symbol if the
     initialization has side effects.  That suppresses a warning that the
     symbol is declared but never referenced. */
  assoc_sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
  if (assoc_sym != NULL) {
    if (dynamic_init_has_side_effects(dip)) {
      assoc_sym->referenced = TRUE;
    }  /* if */
  }  /* if */
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
  if (linkage != idl_none) {
    /* The type of a variable with linkage has been adjusted because it
       is an incomplete array that has been initialized.  Check that the
       new type is compatible with other declarations of the variable.
       This is necessary for cases like
         main () {extern char a[5];}
         char a[] = "abc";  <-- Error; int [3] is incompatible with int [5].
    */
    make_locator_for_symbol(symbol_ptr, &locator);
    name_linkage = symbol_ptr->variant.variable->source_corresp.name_linkage;
    ext_sym = find_external_symbol(&locator, name_linkage,
                                   (a_type_ptr)NULL, &ext_locator);
#if CHECKING
    if (ext_sym == NULL) {
      internal_error("put_type_back_into_variable: ext_sym not found");
    }  /* if */
#endif /* CHECKING */
    (void)reconcile_external_symbol_types(ext_sym, source_pos, vp_type,
                                        /*suppress_incompatible_error=*/FALSE);
  }  /* if */
  /* Put the updated type into the variable. */
  vp->type = vp_type;
  db_exit();
}  /* put_type_back_into_variable */


void initializer(a_symbol_ptr       symbol_ptr,
                 a_source_position  *source_pos,
                 an_id_linkage_kind linkage,
                 a_boolean          parenthesized_initializer,
                 a_boolean          is_parameter)
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
*/
{
  a_boolean                      err = FALSE;
  a_boolean                      put_init_in_variable;
  a_variable_ptr                 vp = NULL;
  a_type_ptr                     vp_type = NULL;
  a_boolean                      brace_flag = FALSE;
  an_expr_node_ptr               expression;
  a_constant                     constant;
  a_dynamic_init                 local_di;
  a_boolean                      dynamic_init_required;
  a_boolean                      initialization_is_dynamic;
  an_expr_node_ptr               arg_list;
  a_class_symbol_supplement_ptr  cssp = NULL;
  a_routine_ptr                  conversion_routine;
  a_memory_region_number         region_to_switch_back_to = NULL_region_number;
  a_boolean                      class_bitwise_copy;

  db_enter(3, "initializer");

  if (is_parameter) {
    /* Parameter declarations cannot contain an initializer.  (Declarations
       for which is_parameter is TRUE are old-style C parameter declarations.
       C++ default arguments, which look a bit like a parameter with an
       initializer -- e.g., void f(int i = 1) -- are handled elsewhere.) */
    pos_error(ec_initializer_in_param, source_pos);
    err = TRUE;
  } else if (symbol_ptr->kind != (a_symbol_kind)sk_variable &&
             symbol_ptr->kind != (a_symbol_kind)sk_static_data_member) {
    /* Not a variable (for example, might be a typedef). */
    pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
    err = TRUE;
  } else {
    vp = symbol_ptr->variant.variable;
    vp_type = vp->type;
    if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
        symbol_ptr->decl_scope != FILE_SCOPE_NUMBER && 
        linkage != idl_none) {
      /* Block scope variable with internal or external linkage --
         not allowed to be initialized.  (3.5.7 Constraints) */
      pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
      err = TRUE;
    } else if (vp->init_kind != (an_init_kind)initk_none) {
      /* Variable already initialized (presumably, it is being declared
         again, and we have the variable from the earlier declaration). */
      pos_sy_error(ec_already_initialized, source_pos, symbol_ptr);
      err = TRUE;
    } else {
      /* Only object types and incomplete arrays are allowed to be
         initialized. */
      if (is_object_type(vp_type)) {
        /* Object type -- okay. */
      } else if (is_array_type(vp_type) &&
                 !is_incomplete_type(array_element_type(vp_type))) {
        /* Array type.  The is_incomplete_type test disallows arrays of
           incomplete struct/unions (which are an extension). */
      } else if (is_reference_type(vp_type)) {
        /* Reference type -- okay. */
      } else {
        /* An object of this type cannot be initialized. */
        pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
        err = TRUE;
        /* Use an error type to avoid additional errors. */
        vp_type = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Note that in the error cases just detected, we go ahead and scan the
     initializer, but then discard the value. */
  put_init_in_variable = !err;
  if (vp_type == NULL) vp_type = error_type();
  initialization_is_dynamic = FALSE;
  if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
    /* The initializer of a static data member is scanned with the original
       class reactivated. */
    push_class_reactivation_scope(symbol_ptr->class_of_which_a_member);
#if CHECKING
    /* Though static data members may be given storage class of extern or
       unspecified, that fixup should not have taken place yet. */
    if (vp != NULL && vp->storage_class != (a_storage_class)sc_static) {
      internal_error("initializer: bad storage class for static data member");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  if (vp != NULL && vp->storage_class == (a_storage_class)sc_static) {
    /* Variables with static storage class, even when declared at function
       scope, will have been allocated in the file scope memory region.  Be
       sure the initializers are also at file scope. */
    switch_to_file_scope_region(&region_to_switch_back_to);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(vp_type)) {
    cssp = symbol_supplement_for_class(vp_type);
  }  /* if */
  if (cssp != NULL && parenthesized_initializer) {
    /* This is an initialization of the form S x (arg [, ...]), where S is a
       class type name. */
    if (cssp->constructor != NULL) {
      /* Depending on the arguments present, a constructor, possibly the copy
         constructor, will be selected and returned. */
      scan_ctor_arguments(cssp->constructor, &arg_list, &conversion_routine,
                          source_pos);
      if (conversion_routine == NULL) {
        err = TRUE;
      } else {
        /* Set the dynamic init entry to represent constructor
           initialization. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
        local_di.variant.constructor.routine = conversion_routine;
        local_di.variant.constructor.args = arg_list;
      }  /* if */
    } else {
      /* C-style class with no constructors, so initialization by bitwise
         copy is allowed. */
      scan_initializer_of_simple_object(/*nonconst_allowed=*/TRUE,
                                        vp_type, &local_di);
      /* The closing right paren will not be consumed, as it is when scanning
         the arg list for a constructor call, so bypass it explicitly. */
      check_closing_paren_after_expr_list();
    }  /* if */
    initialization_is_dynamic = TRUE;
  } else if (cssp != NULL && !cssp->is_class_aggregate &&
             curr_token == tok_lbrace) {
    /* This is an attempt to do C-style aggregate initialization on a class
       object for which there is a constructor, nonpublic members, base
       classes, or virtual functions.  In such cases a constructor must be
       used. */
    syntax_error(ec_brace_initialization_not_allowed);
    err = TRUE;
  } else if (is_class_struct_union_type(vp_type) && curr_token != tok_lbrace &&
             (C_dialect == C_dialect_cplusplus ||
                (vp != NULL &&
                 !has_static_storage_duration(vp->storage_class)))) {
    /* Special C++ case:  a class aggregate may be initialized with an object
       of its class or a class derived from it.  E.g., if S is the name of a
       struct and x is an S, then S y = x is permitted.  In addition, x may
       be any expression of a type for which there is a type conversion to S.
       Thus S y = 1 is a legal initialization if S(int) exists to perform the
       conversion. */
    /* In ordinary C a struct or union variable may be initialized by an
       object of the same type as long as dynamic initialization is otherwise
       allowed. */
    expression = scan_class_initializer_expression(vp_type,
                                                   &conversion_routine,
                                                   &class_bitwise_copy);
    if (class_bitwise_copy) {
      /* The case of C-style structs.  No constructor exists, but simple
         struct assignment can be performed.  Set the dynamic init entry to
         represent non-constant assignment initialization. */
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_expression);
      local_di.variant.expression = expression;
    } else if (conversion_routine != NULL) {
      /* Initializing a class object that has a constructor in a statement
         that looks like an assignment (see discussion in ARM 12.6.1).
         It is as though the expression on the right hand side is constructed
         into a temporary and then a copy constructor is called to actually
         do the initialization -- e.g., complex x = 1 is to be treated as
         complex x = complex(1).  Set the dynamic init entry to represent
         constructor initialization. */
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
      local_di.variant.constructor.routine = conversion_routine;
      local_di.variant.constructor.args = expression;
    } else {
      /* No appropriate constructor was found.  Abort the initialization. */
      err = TRUE;
    }  /* if */
    initialization_is_dynamic = TRUE;
  } else if (is_aggregate_or_union_type(vp_type)) {
    /* Ordinary C-style aggregate initialization, usually with a brace-
       enclosed list of values.  Except in C++ such lists may include
       non-constants. */
    a_constant_ptr       cp;
    a_dynamic_init_ptr   di_list = NULL, end_of_di_list = NULL;

    /* Scan the initializer list. */
    cp = get_initializer(&vp_type, &di_list, &end_of_di_list,
                         /*top_level=*/TRUE);
    switch (cp->kind) {
      case ck_aggregate:
        /* Scan was successful and the value list was recorded as a list of
           constant entries hanging off a ck_aggregate constant. */
        if (di_list != NULL) {
          /* Set the dynamic init entry to represent aggregate
             initialization. */
          clear_dynamic_init(&local_di,
                             (a_dynamic_init_kind)dik_nonconstant_aggregate);
          local_di.variant.aggregate.aggr_const = cp;
          local_di.variant.aggregate.dynamic_init_list = di_list;
          initialization_is_dynamic = TRUE;
          break;
        }  /* if */
        /* Fall through for the case in which all the aggregate initializers
           are constants. */
      case ck_string:    
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constant);
        local_di.variant.constant = cp;
        break;
      case ck_error:
        err = TRUE;
        break;
#if CHECKING
      default:
        internal_error("initializer: unexpected constant kind");
#endif /* CHECKING */
    }  /* switch */
    if (!err && put_init_in_variable) {
      /* Copy the type back into the variable.  It might have been changed
         if vp is an incomplete array. */
      if (vp != NULL && vp_type != vp->type) {
        put_type_back_into_variable(vp, symbol_ptr, source_pos, linkage,
                                    vp_type);
      }  /* if */
    }  /* if */
  } else {
    /* A non-aggregate object is being initialized.  Braces or parens are
       permitted (but not both, of course).  A constant or non-constant
       expression may be permitted as the initializer. */
    if (parenthesized_initializer) {
      add_stop_token(tok_rparen);
    } else {
      check_for_opening_brace(&brace_flag);
    }  /* if */
    scan_initializer_of_simple_object(
           /*nonconst_allowed=*/(C_dialect == C_dialect_cplusplus ||
              (vp != NULL && !has_static_storage_duration(vp->storage_class))),
           vp_type, &local_di);
    if (local_di.kind == (a_dynamic_init_kind)dik_expression) {
      initialization_is_dynamic = TRUE;
    }  /* if */
    /* Check for matching delimiter if lparen or lbrace appeared in front of
       the initializer. */
    if (parenthesized_initializer) {
      remove_stop_token(tok_rparen);
      check_closing_paren_after_expr_list();
    } else {
      /* If an extra opening brace was ignored earlier, ignore the matching
         closing brace now.  Check also for an extra comma (required in C++
         per ARM 8.4, offered in C along with the extension that permits
         brace-enclosed initializers on non-aggregate variables in the first
         place). */
      if (brace_flag && curr_token == tok_comma) (void)get_token();
      check_for_matching_closing_brace(brace_flag);
    }  /* if */
  }  /* if */
  if (region_to_switch_back_to != NULL_region_number) {
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
    /* The initializer of a static data member was scanned with the original
       class reactivated.  Restore the scope to what it was before. */
    pop_class_reactivation_scope();
  }  /* if */
  if (put_init_in_variable) {
    /* There was no error that precludes initialization, so update the
       variable entry with the initializer. */
    if (err) {
      /* There was an error in the initializer.  Put an error constant
         into the initializer field of the variable, if only to be sure
         another initialization will be prevented. */
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constant);
      set_error_constant(&constant);
      local_di.variant.constant = alloc_unshared_constant(&constant);
    }  /* if */
    /* Sometimes the need for dynamic initialization can be inferred from the
       initializer itself, but all initialization of non-static variables must
       be handled at run time.  Set a flag to that effect. */
    dynamic_init_required = !has_static_storage_duration(vp->storage_class);
    /* Check for the existence of a destructor independently of checks for a
       constructor.  This is to catch the unusual case in which a user has
       defined a destructor but the object can be initialized without a
       constructor. */
    if (cssp != NULL) {
      a_routine_ptr  rp = select_destructor(vp_type);
      if (rp != NULL) {
        local_di.destructor = rp;
        initialization_is_dynamic = TRUE;
      }  /* if */
    }  /* if */
    if (initialization_is_dynamic || dynamic_init_required) {
      /* Generate a dynamic initialization entry (based on local_di) and
         attach it to the variable, and generate an stmk_init statement. */
      gen_dynamic_initialization(vp, &local_di, source_pos);
    } else {
      /* Neither the variable nor the initializer require initialization to be
         dynamic. */
      vp->init_kind = (an_init_kind)initk_static;
      vp->initializer.constant = local_di.variant.constant;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      db_variable(vp);
      fputs(",\n", f_debug);
      db_initializer(vp, 2);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  db_exit();
}  /* initializer */


void repeat_constructor_init(a_dynamic_init_ptr  ctor_dip,
                             a_dynamic_init_ptr  new_dip,
                             int                 count)
/*
Define a dynamic init entry for a nonconstant aggregate, which will always be
for an array whose elements are to be initialized by a series of constructor
calls.  The dynamic entry to be defined (new_dip) has already been allocated;
the dynamic init entry that represents the constructor is ctor_dip.  count is
the number of elements in the array to be initialized.
*/
{
  a_constant_ptr           aggr_con, repeat_con, dynamic_init_con;

  clear_dynamic_init(new_dip, (a_dynamic_init_kind)dik_nonconstant_aggregate);
  /* Create a ck_aggregate constant. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  new_dip->variant.aggregate.aggr_const = aggr_con;
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
  /* Set the new dynamic init entry also to point the dik_constructor entry. */
  new_dip->variant.aggregate.dynamic_init_list = ctor_dip;
}  /* repeat_constructor_init */


a_boolean def_initializer(a_symbol_ptr       sym,
                          a_source_position  *err_pos)
/*
Perform default initialization for variables and static data members of class
type.  Such initialization is required whenever the class has a constructor;
the default constructor (if one exists) is called.
*/
{
  a_boolean                      def_init_performed = FALSE;
  a_variable_ptr                 var;
  a_type_ptr                     var_type, tp;
  a_class_symbol_supplement_ptr  cssp;
  a_dynamic_init                 local_di;
  a_routine_ptr                  rp;
  int                            count;

  db_enter(3, "def_initializer");
  /* Default initialization is done only in C++ and only for variables and
     static data members. */
  if (C_dialect == C_dialect_cplusplus &&
      (sym->kind == (a_symbol_kind)sk_variable ||
      sym->kind == (a_symbol_kind)sk_static_data_member)) {
    var = sym->variant.variable;
    tp = var_type = skip_typerefs(var->type);
    if (is_array_type(tp)) {
      tp = skip_typerefs(underlying_array_element_type(tp));
    }  /* if */
    /* Default initialization is done only for objects that are defined in
       the current translation unit (i.e., storage class other than "extern")
       and that require constructor initialization. */
    if (is_class_struct_union_type(tp) &&
        var->storage_class != (a_storage_class)sc_extern) {
      cssp = symbol_supplement_for_class(tp);
      if (cssp->constructor != NULL) {
        if (is_incomplete_type(var_type)) {
#if 0
          /* Don't quite know what to do on this yet.  What's done in
             end_of_scope_symbol_check is nontrivial.  And I'm not sure it
             works for multi-dimensional array. */
          pos_warning(ec_default_size_for_incomplete_array, err_pos);
#else
#if CHECKING
          internal_error(
                      "def_initializer: incomplete types not yet supported");
#endif /* CHECKING */
#endif /* if 0 */
        }  /* if */
        if ((rp = select_default_constructor(tp, err_pos)) != NULL) {
          a_param_type_ptr  ptp = (skip_typerefs(rp->type))->
                                   variant.routine.extra_info->param_type_list;
          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
          local_di.variant.constructor.routine = rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          local_di.variant.constructor.args = copy_default_arg_expr_list(ptp);
          local_di.destructor = select_destructor(tp);
          if (var_type != tp) {
            /* The variable for which initialization is done is an array, so
               we need to generate the repeat construct so that the constructor
               (and destructor) can be called once for each element. */
            a_dynamic_init  *ctor_dip;

            /* Copy the dynamic init entry. */
            ctor_dip =
                    alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
            *ctor_dip = local_di;
            /* Compute the repeat count. */
            if (var_type->size == 0) {
              count = 1;
            } else {
              count = (int)(var_type->size / tp->size);
            }  /* if */
            repeat_constructor_init(ctor_dip, &local_di, count);
          }  /* if */
          /* Build the repeat construct. */
          gen_dynamic_initialization(var, &local_di, err_pos);
          def_init_performed = TRUE;
#if DEBUG
          if (debug_level >= 3) {
            db_variable(var);
            fputs(",\n", f_debug);
            db_initializer(var, 2);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      } else if ((rp = select_destructor(tp)) != NULL) {
        /* Default initialization of an object that has a destructor.  We
           generate a dik_none dynamic initialization entry for this object,
           even though it is not actually initialized, so that the existence
           of the destructor can be duly recorded. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_none);
        local_di.destructor = rp;
        if (var_type != tp) {
          /* The object has an array type. */
          a_dynamic_init  *dtor_dip;

          /* Copy the dynamic init entry. */
          dtor_dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          *dtor_dip = local_di;
          /* Compute the repeat count. */
          if (var_type->size == 0) {
            count = 1;
          } else {
            count = (int)(var_type->size / tp->size);
          }  /* if */
          /* Build the repeat construct. */
          repeat_constructor_init(dtor_dip, &local_di, count);
        }  /* if */
        gen_dynamic_initialization(var, &local_di, err_pos);
        /* Don't set def_init_performed.  A dik_none dynamic initialization
           doesn't count as initialization. */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return def_init_performed;
}  /* def_initializer */


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
  a_boolean                     err, is_generated_cctor;
  a_boolean                     const_object_okay, volatile_object_okay;
  a_type_ptr                    class_type, init_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym, member_or_base_sym;
  a_constructor_init_ptr        cip, new_cip, prev_cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list, end_of_virtual_list;
  a_constructor_init_ptr        direct_list, end_of_direct_list;
  a_base_class_ptr              bcp;
  a_class_symbol_supplement_ptr cssp;
  a_routine_ptr                 conversion_routine, rp;
  a_dynamic_init_ptr            dip, ctor_dip;
  int                           direct_base_class_count = 0;
  a_source_position             lparen_pos;

  db_enter(3, "ctor_initializer");
  class_type = ((a_symbol_ptr)ctor_rout->source_corresp.assoc_info)->
                                                   class_of_which_a_member;
#if CHECKING
  if (class_type == NULL) internal_error("ctor_initializer: NULL class type");
#endif /* if CHECKING */
  is_generated_cctor = !user_defined &&
                       is_copy_constructor(ctor_rout, class_type,
                                           &const_object_okay,
                                           &volatile_object_okay);
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
  for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->direct) ++direct_base_class_count;
    if (bcp->is_virtual || bcp->direct) {
      cssp = symbol_supplement_for_class(bcp->type);
      /* If the virtual base class or direct base class has a constructor, a
         dynamic init entry will be required; otherwise it is optional.
         Create the constructor init entry now; the dynamic init will be added
         later. */
      cip = alloc_ctor_init(bcp->is_virtual ?
                            (a_constructor_init_kind)cik_virtual_base_class :
                            (a_constructor_init_kind)cik_direct_base_class);
      cip->variant.base_class = bcp;
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
      if (is_generated_cctor) {
        /* All fields are explicitly listed for a generated copy constructor,
           since even if there is no constructor at least a bitwise copy is
           required. */
      } else {
        /* This is not a copy constructor.  See if this is a field that
           requires an initializer. */
        tp = sym->variant.field.ptr->type;
        if (is_reference_type(tp)) {
          /* Ref-type fields require an initializer. */
        } else {
          if (is_array_type(tp)) {
            tp = skip_typerefs(underlying_array_element_type(tp));
          }  /* if */
          if (is_const_qualified_type(tp)) {
            /* Const and array-of-const fields require an initializer. */
          } else if (is_class_struct_union_type(tp) &&
                     symbol_supplement_for_class(tp)->constructor != NULL) {
            /* A constructor initializer is required. */
          } else {
            /* No initializer is needed. */
            continue;
          }  /* if */
        }  /* if */
      }  /* if */
      /* A constructor init entry is required for this field. */
      cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
      cip->variant.field = sym->variant.field.ptr;
      if (cip_list == NULL) {
        cip_list = cip;
      } else {
        end_of_cip_list->next = cip;
      }  /* if */
      end_of_cip_list = cip;
    }  /* if */
  }  /* for */
  /* Three lists that have been created thus far were made to cover the
     default initialization required because base classes and fields need
     it.  It remains to scan the user specified initializers, if any, and
     to integrate them into the lists. */
  if (user_defined && curr_token == tok_colon) {
    /* User-specified initializers are present.  Bypass the colon. */
    (void)get_token();
    add_stop_token(tok_lbrace);
    /* Loop through the comma-separated list of initializers. */
    do {
      err = FALSE;
      new_cip = NULL;
      add_stop_token(tok_comma);
      /* Unless this is an old style base class initializer, a base class
         name or a member name is expected. */
      if (curr_token != tok_lparen && !is_qualified_name_start()) {
        /* Either an identifier or "::" is expected here. */
        syntax_error(ec_exp_identifier);
      } else {
        bcp = NULL;
        dip = NULL;
        if (curr_token == tok_lparen) {
          /* Old-style base class initializer.  It is assumed to apply the
             the direct base class (further assuming that there is exactly
             one direct base class). */
          if (direct_base_class_count != 1) {
            /* Either no base classes or more than one. */
            error(ec_missing_base_class_or_member_name);
            init_type = error_type();
          } else {
            /* The base class is probably on the direct_list, but if it was
               declared virtual it is on the virtual list. */
            new_cip = (direct_list != NULL) ? direct_list : virtual_list;
            bcp = new_cip->variant.base_class;
#if CHECKING
            if (!bcp->direct) {
              internal_error("ctor_initializer: not a direct base class");
            }  /* if */
#endif /* CHECKING */
            init_type = bcp->type;
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, init_type);
              err = TRUE;
            } else {
              type_warning(ec_base_class_init_anachronism, init_type);
            }  /* if */
          }  /* if */
          /* Back up so that the left paren will be rescanned. */
          unget_token();
          goto scan_paren;
        }  /* if */
        /* Scan the base class name or member name. */
        member_or_base_sym = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
        if (member_or_base_sym == NULL) {
          /* No such name or qualified name in the symbol table. */
          type_error(ec_not_a_field_or_base_class, class_type);
          init_type = error_type();
        } else if (member_or_base_sym->kind == (a_symbol_kind)sk_field &&
                   member_or_base_sym->class_of_which_a_member == class_type) {
          /* This is a field of the current class and may be mentioned in the
             constructor's initializer list.  But it's an error to refer to
             it by a qualified name. */
          if (locator_for_curr_id.is_qualified_name) {
            pos_error(ec_qualified_name_not_allowed,
                      &locator_for_curr_id.source_position);
          }  /* if */
          init_type = member_or_base_sym->variant.field.ptr->type;
          /* The syntax does not provide for the initialization of arrays --
             except character strings. */
          if (is_array_type(init_type) && !is_string_type(init_type)) {
            sym_error(ec_cannot_initialize, member_or_base_sym);
            init_type = error_type();
            goto scan_paren;
          }  /* if */
          /* Check the list for a constructor init entry that refers to this
             member.  If it's there we may have a reinitialization error. */
          for (new_cip = cip_list; new_cip != NULL; new_cip = new_cip->next) {
            if (new_cip->variant.field ==
                                   member_or_base_sym->variant.field.ptr) {
              if (new_cip->initializer != NULL) {
                sym_error(ec_member_already_initialized, member_or_base_sym);
                err = TRUE;
                goto scan_paren;
              }  /* if */
              break;
            }  /* if */
          }  /* for */
          if (new_cip == NULL) {
            /* No constructor init entry exists for this field.  Allocate one
               and add it to the list. */
            new_cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
            new_cip->variant.field = member_or_base_sym->variant.field.ptr;
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
        } else if (is_class_symbol(member_or_base_sym)) {
          /* It is a base class of the current class for which initialization
             is to be done. */
          a_boolean  indirect_nonvirtual_base_class_found = FALSE;
          if (locator_for_curr_id.is_semivisible_nested_class) {
            /* The symbol in the locator is a nested class that is not
               visible according to the ARM lookup rules but is returned
               in support of the nested class anachronism (ARM 18.3.5).
               Issue a warning. */
            sym_warning(ec_nested_class_anachronism,
                        locator_for_curr_id.specific_symbol);
          }  /* if */
          init_type = type_symbol_type(member_or_base_sym);
           /* The symbol's type entry could be a "tag typeref".  If so, get
              the type entry at file scope that it points to. */
          if (init_type->kind == (a_type_kind)tk_typeref &&
              init_type->variant.typeref.is_function_scope_tag) {
            init_type = init_type->variant.typeref.type;
          }  /* if */
          /* Locate it in the base classes list for the current class.  Note
             that only direct and virtual base classes can be specified. */
          bcp = class_type->
                    variant.class_struct_union.extra_info->base_classes;
          for (; bcp != NULL; bcp = bcp->next) {
            if (bcp->type == init_type) {
              if (bcp->direct || bcp->is_virtual) {
                break;
              } else {
                /* A base class of the required type was found, but it is
                   neither direct nor virtual.  Unless another is found with
                   the same name, this will be an error. */
                indirect_nonvirtual_base_class_found = TRUE;
              }  /* if */
            }  /* if */
          }  /* for */
          if (bcp == NULL) {
            /* No match found. */
            if (indirect_nonvirtual_base_class_found) {
              /* Actually, a match was found, but it was not a direct or
                 virtual base class. */
              error(ec_indirect_nonvirtual_base_class_not_allowed);
            } else {
              /* Not a base class of the class for which a constructor is
                 being defined. */
              type_error(ec_not_a_field_or_base_class, class_type);
            }  /* if */
            init_type = error_type();
          } else {
            /* The base class was found.  Now look on the appropriate list of
               constructor initializers. */
            new_cip = (bcp->is_virtual) ? virtual_list : direct_list;
            for (; new_cip != NULL; new_cip = new_cip->next) {
              if (new_cip->variant.base_class == bcp) break;
            }  /* for */
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, bcp->type);
              err = TRUE;
            }  /* if */
          }  /* if */
        } else {
          /* Not a base class, not a field.  Issue an error. */
          type_error(ec_not_a_field_or_base_class, class_type);
          init_type = error_type();
        }  /* if */
scan_paren:
        /* Advance past the identifier. */
        (void)get_token();
        copy_source_position(pos_curr_token, lparen_pos);
        if (required_token(tok_lparen, ec_exp_lparen)) {
          if (is_class_struct_union_type(init_type)) {
            /* This is either a base class or a field of class type.  In
               either case, it will be initialized by a constructor call if
               a constructor exists.  Otherwise, it will be initialized
               like any scalar. */
            an_expr_node_ptr  arg_list;
            cssp = symbol_supplement_for_class(init_type);
            if (cssp->constructor == NULL) {
              /* There is no constructor. */
              goto scan_arg_for_scan_initialization;
            } else {
              /* This is treated like an initialization of the form
                 S x (arg [, ...]), where S is a class type name.  Depending
                 on the arguments present, a constructor will be selected and
                 returned.  The scan function returns FALSE if it finds no
                 constructor for which the arguments match. */
              scan_ctor_arguments(cssp->constructor, &arg_list,
                                  &conversion_routine, &lparen_pos);
              if (conversion_routine == NULL) err = TRUE;
            }   /* if */
            if (!err) {
              /* Set the dynamic init entry to represent constructor
                 initialization. */
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
              dip->variant.constructor.routine = conversion_routine;
              dip->variant.constructor.args = arg_list;
            } else {
              /* Create a fake initializer to represent the error. */
              a_constant_ptr  cp;
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
              cp = alloc_constant((a_constant_repr_kind)ck_error);
              set_error_constant(cp);
              dip->variant.constant = cp;
            }  /* if */
            new_cip->initializer = dip;
          } else {
scan_arg_for_scan_initialization:
            add_stop_token(tok_rparen);
            /* Allocate a new dynamic init entry, setting the kind to
               dik_none for now.  It will be adjusted after the scan. */
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
            if (curr_token == tok_rparen) {
              /* No expression.  Leave the dynamic init entry as is. */
            } else {
              scan_initializer_of_simple_object(/*nonconst_allowed=*/TRUE,
                                                init_type, dip);
            }  /* if */
            if (new_cip != NULL) new_cip->initializer = dip;
            remove_stop_token(tok_rparen);
            (void)required_token(tok_rparen, ec_exp_rparen);
          }  /* if */
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
  /* Make a pass over the new list, adding default constructors where
     appropriate. */
  prev_cip = NULL;
  for (cip = cip_list; cip != NULL; cip = cip->next) {
    if (cip->initializer == NULL) {
      a_boolean  is_const_qualified = FALSE;
      /* No initializer was explicitly specified. */
      array_type = NULL;
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        /* Get the field type.  For arrays, we want the element type. */
        tp = cip->variant.field->type;
        if (is_array_type(tp)) {
          array_type = skip_typerefs(tp);
          tp = underlying_array_element_type(tp);
        }  /* if */
        if (is_const_qualified_type(tp)) is_const_qualified = TRUE;
        tp = skip_typerefs(tp);
      } else {
        /* Get the type of the base class. */
        tp = cip->variant.base_class->type;
      }  /* if */
      cssp = is_class_struct_union_type(tp) ? symbol_supplement_for_class(tp) :
                                              NULL;
      if (is_generated_cctor) {
        /* The constructor for the object as a whole is a generated copy
           constructor.  Any subobject constructors must also be copy
           constructors, and fields and base classes that have no constructor
           must be accounted for, too. */
        a_boolean  bitwise_copy = FALSE;
        if (cssp == NULL) {
          bitwise_copy = TRUE;
        } else {
          /* The flag const_object_okay describes whether the top-level
             constructor can accept a const object for copying; if it can,
             then all constructors called to copy subobjects *must* accept a
             const object for copying (a conclusion based in part on ARM 12.8).
             By extension, the same applies to the volatile qualifier.  Thus
             the parameter name on the other end of this call stipulates a
             requirement on the search for a copy constructor.  If construction
             by bitwise copy is allowed for this class, class_bitwise_copy will
             be returned TRUE. */
          rp = select_copy_constructor(tp,
                                       const_object_okay, volatile_object_okay,
                                       &error_position, &bitwise_copy);
        }  /* if */
        if (bitwise_copy) {
          /* Construction by bitwise copy is allowed. */
          if (cip->kind == (a_constructor_init_kind)cik_field) {
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_member_copy);
          } else {
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_base_class_copy);
          }  /* if */
        } else if (rp == NULL) {
          /* The copy constructor was invalid in some way or other. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A valid copy constructor does exist.  Generate the dynamic init
             entry. */
          a_param_type_ptr  ptp = (skip_typerefs(rp->type))->
                                   variant.routine.extra_info->param_type_list;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.routine = rp;
          /* No expression node is created to represent the subobject.  The
             back end will compute the subobject's address based on the
             base class or field just as it will compute the address of the
             implicit "this" parameter, which is the address of the subobject
             to be initialized by the copy. */
          /* We need to copy the default arg expressions of the second and
             subsequent parameters (if any) of the copy constructor.  The
             first param is ignored even if it is declared to have a default
             arg. */
          ptp = ptp->next;
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
          dip->variant.constructor.is_copy_constructor_for_subobject = TRUE;
        }  /* if */
      } else {
        /* No copy constructor is required.  If any constructor exists, the
           default constructor should be called. */
        if (cip->kind == (a_constructor_init_kind)cik_field &&
            (is_reference_type(tp) || is_const_qualified)) {
          /* Ref-type field or const-qualified field but no initializer. */
          a_symbol_ptr field_sym = (a_symbol_ptr)cip->variant.field->
                                                   source_corresp.assoc_info;
          if (ctor_rout->compiler_generated) {
            pos_sy_warning(ec_cannot_initialize_field, &error_position,
                           field_sym);
          } else {
            pos_sy_warning(ec_missing_initializer_on_field, &error_position,
                           field_sym);
          }  /* if */
          if (prev_cip == NULL) {
            cip_list = cip->next;
          } else {
            prev_cip->next = cip->next;
          }  /* if */
          continue;
        }  /* if */
#if CHECKING
        if (!is_class_struct_union_type(tp)) {
          internal_error("ctor_initializer: unexpected type on noncopy ctor");
        }  /* if */
#endif /* CHECKING */
        if (cssp == NULL || cssp->constructor == NULL) {
          /* This constructor initializer entry is not really needed.  It is
             associated with a base class without a constructor.  Unlink it
             from the list. */
          if (prev_cip == NULL) {
            cip_list = cip->next;
          } else {
            prev_cip->next = cip->next;
          }  /* if */
          continue;
        }  /* if */
        rp = select_default_constructor(tp, &error_position);
        if (rp == NULL) {
          /* Error in trying to find a default constructor. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        } else {
          /* A default constructor does exist.  Generate the dynamic init
             entry. */
          a_param_type_ptr  ptp = (skip_typerefs(rp->type))->
                                   variant.routine.extra_info->param_type_list;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.routine = rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
        }  /* if */
      }  /* if */
      if (array_type != NULL &&
          dip->kind == (a_dynamic_init_kind)dik_constructor) {
        int  count;
        /* We have an array of objects with constructors.  Create a dynamic
           init entry to handle the aggregate. */
        ctor_dip = dip;
        dip =
           alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
        /* Build the looping constant entry. */
        if (array_type->size == 0) {
          count = 1;
        } else {
          count = (int)(array_type->size / tp->size);
        }  /* if */
        repeat_constructor_init(ctor_dip, dip, count);
      }  /* if */
      /* Attach the new dynamic init entry to the constructor initializer. */
      cip->initializer = dip;
    }  /* if */
    prev_cip = cip;
  }  /* for */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol((a_symbol_ptr)ctor_rout->source_corresp.assoc_info,
              "constructor: ", 2);
    for (cip = cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = (a_symbol_ptr)cip->variant.field->source_corresp.assoc_info;
      } else {
        sym = (a_symbol_ptr)cip->variant.base_class->type->
                                                source_corresp.assoc_info;
      }  /* if */
      fprintf(f_debug, "    initializer for %s %s: %s",
                       (cip->kind == (a_constructor_init_kind)cik_field) ?
                          "field" : "base class",
                       sym->header->identifier,
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
  a_type_ptr                    class_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym;
  a_constructor_init_ptr        cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list;
  a_routine_ptr                 rp;
  a_base_class_ptr              bcp;
  a_dynamic_init_ptr            dip;

  db_enter(3, "dtor_initializer");
  class_type = ((a_symbol_ptr)dtor_rout->source_corresp.assoc_info)->
                                                   class_of_which_a_member;
#if CHECKING
  if (class_type == NULL) internal_error("dtor_initializer: NULL class type");
#endif /* if CHECKING */
  /* The order of destructor calls is exactly the reverse of the order of
     constructor calls.  In other words, destructors for virtual base classes
     are last, preceded by destructors for nonvirtual direct base classes,
     with destructors for members coming first (ARM 12.4).  Thus we follow the
     logic in ctor_initializer, except that the lists are built backwards and
     merged backwards.   First construct the lists for virtual base classes
     and nonvirtual direct base classes. */
  virtual_list = NULL;
  cip_list = end_of_cip_list = NULL;
  for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual || bcp->direct) {
      /* If the virtual base class or direct base class has a destructor, a
         dynamic init entry will be required.  Create the constructor init
         entry now; the dynamic init will be added later. */
      if ((rp = select_destructor(bcp->type)) != NULL) {
        cip = alloc_ctor_init(bcp->is_virtual ?
                              (a_constructor_init_kind)cik_virtual_base_class :
                              (a_constructor_init_kind)cik_direct_base_class);
        cip->variant.base_class = bcp;
        /* Create a dynamic init entry. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
        dip->destructor = rp;
        /* Attach the new dynamic init entry to the constructor initializer. */
        cip->initializer = dip;
        /* Add the constructor init to the end of the appropriate list. */
        if (bcp->is_virtual) {
          /* Add to the start of the virtual list. */
          cip->next = virtual_list;
          virtual_list = cip;
        } else {
          /* Add to the start of the direct list.  If this is the first
             entry, keep track of it, since it will be the tail of the list
             to which the virtual list will be attached later. */
          if (end_of_cip_list == NULL) end_of_cip_list = cip;
          cip->next = cip_list;
          cip_list = cip;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (cip_list != NULL) {
    /* Attach the virtual list, if any, to the end of the direct list. */
    end_of_cip_list->next = virtual_list;
  } else {
    /* No direct list.  Just use the virtual list. */
    cip_list = virtual_list;
  }  /* if */
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
      array_type = NULL;
      tp = skip_typerefs(sym->variant.field.ptr->type);
      /* For arrays get the element type, allowing for multidimensional
         arrays.  Keep track of the array type for later. */
      if (is_array_type(tp)) {
        array_type = tp;
        tp = skip_typerefs(underlying_array_element_type(tp));
      }  /* if */
      if (is_class_struct_union_type(tp)) {
        if ((rp = select_destructor(tp)) != NULL) {
          /* Create the constructor init entry for a field. */
          cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
          cip->variant.field = sym->variant.field.ptr;
          /* Create a dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = rp;
          if (array_type != NULL) {
            int  count;
            /* We have an array of objects with destructors.  Create a dynamic
               init entry to handle the aggregate. */
            a_dynamic_init_ptr  dtor_dip = dip;
            dip = alloc_dynamic_init(
                             (a_dynamic_init_kind)dik_nonconstant_aggregate);
            /* Build the looping constant entry. */
            if (array_type->size == 0) {
              count = 1;
            } else {
              count = (int)(array_type->size / tp->size);
            }  /* if */
            repeat_constructor_init(dtor_dip, dip, count);
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
#if DEBUG
  if (debug_level >= 3) {
    db_symbol((a_symbol_ptr)dtor_rout->source_corresp.assoc_info,
              "destructor: ", 2);
    for (cip = cip_list; cip != NULL; cip = cip->next) {
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        sym = (a_symbol_ptr)cip->variant.field->source_corresp.assoc_info;
      } else {
        sym = (a_symbol_ptr)cip->variant.base_class->type->
                                                source_corresp.assoc_info;
      }  /* if */
      fprintf(f_debug, "    destructor for %s %s: %s",
                       (cip->kind == (a_constructor_init_kind)cik_field) ?
                          "field" : "base class",
                       sym->header->identifier,
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
