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
#include "target.h"
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

  check_assertion(!(*type)->variant.array.is_variable_size_array);
  array_type = alloc_type((a_type_kind)tk_array);
  copy_type(*type, array_type);
  array_type->variant.array.variant.number_of_elements = size;
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
    } else if (is_wide_string ?
                 (char_int_kind_from_string_type(*type) ==
                              char_int_kind_from_string_type(constant->type)) :
                 is_char_array_type(constant->type)) {
      /* The constant is a string with characters that are compatible with
         the array element type.  (Note that an array of characters of any
         signedness can be initialized with a string literal: ANSI C 3.5.7.) */
      num_elems = string_length = constant->variant.string.length;
      if (is_wide_string) {
        /* Adjust the wide string number of elements. */
        num_elems /= targ_sizeof_wchar_t;
      }  /* if */
      array_type = skip_typerefs(*type);
      if (is_incomplete_type(array_type)) {
        /* The array type is incomplete, and therefore the array size
           is set from the string length. */
        set_initialized_array_size(type, num_elems);
      } else {
        /* The object being initialized is an array that has a definite
           size.  See if the string will fit in the array. */
        check_assertion(!array_type->variant.array.is_variable_size_array);
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
              constant->type = string_type(num_elems);
            } else {
              string_length -= targ_sizeof_wchar_t;
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
    } else {
      /* The constant and the array do not have the same underlying character
         element type; it must be that one is a wide string and the other
         a normal string.  Note that there is no mismatch in that case if
         wchar_t is char. */
      *err = TRUE;
    }  /* if */
    if (*err) {
      /* There was an error of some kind. */
      if (!is_error_type(constant->type)) {
        pos_ty2_error(ec_bad_initializer_type, &error_position,
                      constant->type, *type);
      }  /* if */
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


static a_boolean init_remaining_array_elements(
                                          a_type_ptr          array_type,
                                          a_targ_size_t       curr_element,
                                          a_constant_ptr      *con_list,
                                          a_constant_ptr      *end_of_con_list,
                                          a_dynamic_init_ptr  *di_list,
                                          a_dynamic_init_ptr  *end_of_di_list,
                                          a_boolean           *incomplete_init)
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
the two lists.  *incomplete_init is set to TRUE if a reference or const
member remains uninitialized.  TRUE is returned if the remaining array
elements are indeed initialized.  This routine is called in C++ mode only.
*/
{
  a_type_ptr                     element_type;
  a_targ_size_t                  number_of_uninitialized_elements;
  a_constant_ptr                 cp, repeat_con;
  a_routine_ptr                  ctor_rp;
  a_class_symbol_supplement_ptr  cssp;
  a_param_type_ptr               ptp;
  a_dynamic_init_ptr             dip;
  a_boolean                      init_done = FALSE;

  db_enter(4, "init_remaining_array_elements");

  check_assertion(!array_type->variant.array.is_variable_size_array);
  number_of_uninitialized_elements =
          array_type->variant.array.variant.number_of_elements - curr_element;
  if (number_of_uninitialized_elements > 0) {
    /* There are one or more uninitialized elements. */
    element_type = array_element_type(array_type);
    if (is_class_struct_union_type(element_type)) {
      /* It is an array of class objects. */
      cssp = symbol_supplement_for_class(element_type);
      if (cssp->constructor == NULL &&
          (element_type->variant.class_struct_union.any_const_member ||
           cssp->any_ref_member)) {
        /* An array element of class type with no constructor but with
           const or ref member will end up uninitialized. */
        *incomplete_init = TRUE;
      }  /* if */
      if (cssp->constructor != NULL || cssp->destructor != NULL) {
        /* Initialization is required. */
        if (cssp->constructor != NULL) {
          /* Get the default constructor.  Note that it is an error if it
             is missing. */
          ctor_rp = select_default_constructor(element_type, &pos_curr_token,
					       element_type,
                                               /*evaluated=*/TRUE);
        }  /* if */
        if (ctor_rp != NULL) {
          /* If there's a constructor routine create a dik_constructor
             dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = ctor_rp;
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
        dip->destructor = select_destructor(element_type, element_type,
                                            &pos_curr_token,
                                            /*honor_virtual=*/FALSE,
                                            /*evaluated=*/TRUE,
                                            /*suppress_access_check=*/FALSE);
        /* Now create the constant entry that will point to the new dynamic
           init entry. */
        cp = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
        cp->variant.dynamic_init = dip;
        cp->type = element_type;
        if (number_of_uninitialized_elements > 1) {
          /* When there is more than one uninitialized element remaining in the
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
        init_done = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return init_done;
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
  

static a_constant_ptr get_initializer(
                    a_type_ptr          *type,
                    a_dynamic_init_ptr  *di_list,
                    a_dynamic_init_ptr  *end_of_di_list,
                    a_boolean           top_level,
                    a_boolean           *any_member_uninitialized,
                    a_boolean           *any_const_or_ref_member_uninitialized,
                    a_boolean           *nothing_taken)
/*
Scan a constant initializer or initializer list, and return a pointer to
the constant for it (an aggregate constant if an initializer list is
scanned).  *type indicates the type of the object being initialized.  It
will be updated if the object is an incomplete array whose size is now
known because it is initialized.  If there is some error in the
initializer, an error constant is returned.  top_level is TRUE if this is
a top-level initializer (braces are required surrounding initializers for
unions and aggregates at that level).  *nothing_taken is returned TRUE if
no source tokens were taken because the entity being initialized is an
empty class. *incomplete_init is returned TRUE when at least one const or
ref field of a class object (or an array of same) remains uninitialized.
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
  a_boolean           any_more_initializers, any_more_members;
  a_boolean           local_nothing_taken;
  a_type_kind         kind;
  a_boolean           array_too_long_error_given = FALSE;
  a_boolean           took_extra_comma;
  a_dynamic_init_ptr  dip;
  a_boolean           whole_object_initialization = FALSE;

  db_enter(4, "get_initializer");
  err = FALSE;
  *nothing_taken = FALSE;
  local_type = skip_typerefs(*type);
  /* There is special handling to initialize a field or array element that
     is itself a class object.  If it is a C-style struct (an aggregate
     class, which has no constructors -- see ARM 8.4.1) we assume the
     initial values are to be applied on a member by member basis.  This
     produces slightly anomalous behavior:
        struct S { int a, b; };        // C-style struct (aggregate)
        struct T { int a, b; T(); };   // nonaggregate due to T::T()
        S s1 = { 1, 2 };               // okay (ARM 8.4.1)
        S s2 = s1;                     // okay (ARM 8.4.1) even without copy
                                       //   constructor S::S(const S&)
        S sa1[] = { 1, 2, 1, 2 };      // equivalent to {{1,2},{1,2}}
        S sa2[] = { s1, s2 };          // error!
        T t1 = { 1, 2 };               // error -- must use T::T()
        T t2 = t1;                     // okay -- uses T::T(const T&)
        T ta[] = { t1, t2 };           // okay -- see ARM 12.6.1
     The point to be noted is that if the initialization of sa1 is permitted
     (which is required for C compatibility) the code to initialize sa2 must
     be disallowed, despite what one might expect by looking at s2, t2, and
     ta.  (If we wanted to support the initialization of sa2 and disallow that
     of sa1, the check for is_class_aggregate in the following conditional
     would have to be removed.) */
  if (C_dialect == C_dialect_cplusplus) {
    if (is_class_struct_union_type(local_type)) {
      /* If this is not a C-style struct (i.e., if it is not one for which
         C-style aggregate initialization is allowed) or if it has no
         members, the object is initialized as a whole. */
      if (!(symbol_supplement_for_class(local_type)->is_class_aggregate)) {
        if (curr_token == tok_lbrace) {
          pos_ty_error(ec_brace_initialization_not_allowed, &pos_curr_token,
                       local_type);
          local_type = error_type();
        } else {
          whole_object_initialization = TRUE;
        }  /* if */
      } else if (local_type->variant.class_struct_union.field_list == NULL) {
        /* An empty class.*/
        if (curr_token == tok_lbrace) {
          /* An empty class can be initialized with "{}". */
        } else {
          whole_object_initialization = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  check_for_opening_brace(&brace_flag);
  if (whole_object_initialization) {
#if CHECKING
    if (top_level) {
      internal_error("get_initializer: class encountered at top level");
    } else {
      a_class_symbol_supplement_ptr cssp;
      cssp = symbol_supplement_for_class(local_type);
      if (!cssp->has_copy_constructor &&
          !cssp->construction_by_bitwise_copy_allowed) {
        internal_error("get_initializer: missing copy constructor");
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    /* This is an array element that can only be initialized by a
       constructor.  Treat the expression as an argument for the constructor
       call. */
    if (!scan_class_initializer_expression(local_type, &dip)) {
      /* No constructor was found.  Abort the initialization. */
      err = TRUE;
    } else {
      init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      init_con->type = local_type;
      init_con->variant.dynamic_init = dip;
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
       array of char is initialized by a string.  The initial
       values can either appear inside a brace-enclosed list, or at
       the current level. */
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
      /* In ANSI C and C++, the top-level initializer for a struct, union, or
         array must be surrounded by braces.  e.g., "int a[1] = 1;" is
         not allowed.  However, pcc will allow initialization with
         a single value and we allow it as an extension. */
      if (top_level && !brace_flag) {
        /* In ANSI C and C++, the top-level initializer for a class, struct,
           union, or array must be surrounded by braces (except for whole
           object initialization of classes, handled elsewhere).  However,
           pcc allows it -- e.g., "int a[2] = 1;" is equivalent to
           "int a[2] = { 1 };".  We allow pcc behavior as an extension in
           C mode, but it's an error in C++ mode. */
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
             setting local_type to an error type. */
          if (severity == es_error) local_type = error_type();
        }  /* if */
      }  /* if */
      /* Get information on the first member of the aggregate to be
         initialized. */
      any_more_members = TRUE;  /* Assume. */
      is_incomplete_array = FALSE;
      kind = local_type->kind;
      if (kind == (a_type_kind)tk_error) {
        /* Error type. */
        member_type = error_type();
      } else if (kind == (a_type_kind)tk_array) {
        /* Array.  Start with first element. */
        curr_array_element = 0;
        is_incomplete_array = is_incomplete_type(local_type);
        member_type = local_type->variant.array.element_type;
        /* Note that arrays of incomplete struct/union types (an extension)
           do not make it to here (they're caught as an error at the top
           level in the routine "initializer" and replaced by an error type),
           so we don't have to check for them here. */
      } else {
        /* Class/struct/union.  Start with first field. */
        check_assertion(is_immediate_class_type(local_type));
        curr_field = local_type->variant.class_struct_union.field_list;
        /* Skip past an unnamed field. */
        curr_field = next_initializable_field(curr_field);
        any_more_members = (curr_field != NULL);
      }  /* if */
      /* Check for cases that involve initializing nothing, i.e., the
         zero-trip-loop cases. */
      any_more_initializers = TRUE;  /* Assume. */
      if (brace_flag) {
        /* The list for the aggregate at this level is enclosed in { }. */
        if (curr_token == tok_rbrace) {
          /* Empty initializer list --  "{ }".  An error in C, okay in C++. */
          if (C_mode()) error(ec_exp_primary_expr);
          any_more_initializers = FALSE;
        }  /* if */
      } else {
        /* The list for the aggregate is not enclosed in braces. */
        if (!any_more_members && !top_level) {
          /* This is an initialization of an aggregate with no members,
             i.e., an empty class, and there are no braces for this
             level of the aggregate.  Take nothing to satisfy this
             initialization. */
          *nothing_taken = TRUE;
          any_more_initializers = FALSE;
        }  /* if */
      }  /* if */
      con_list = end_of_con_list = NULL;
      took_extra_comma = FALSE;
      /* Loop, scanning initializers and building an aggregate constant. */
      while (any_more_initializers) {
        /* Determine the type of the member being initialized. */
        if (!any_more_members) {
          /* There are more initializers, but we've run out of members
             into which to put them. */
          error(ec_too_many_initializer_values);
          /* Switch to an error type to take this and all following
             initializers without error. */
          member_type = error_type();
          kind = (a_type_kind)tk_error;
          any_more_members = TRUE;
        }  /* if */
        if (kind == (a_type_kind)tk_error) {
          /* No processing for this case. */
        } else if (kind == (a_type_kind)tk_array) {
          /* member_type was set outside the loop. */
#if DEBUG
          if (debug_level == 4 && kind == (a_type_kind)tk_array) {
            fprintf(f_debug, "getting initializer for element %d, type = ",
                    (int)curr_array_element);
            db_abbreviated_type(member_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
        } else {
          /* Class type: get the type of the current field. */
          member_type = curr_field->type;
#if DEBUG
          if (debug_level == 4) {
            fputs("getting initializer for field \"", f_debug);
            db_name(&curr_field->source_corresp);
            fputs("\", type = ", f_debug);
            db_abbreviated_type(member_type);
            fputc('\n', f_debug);
          }  /* if */
#endif /* DEBUG */
          /* Members of unions or aggregates cannot be incomplete. */
          check_assertion(!is_incomplete_type(member_type));
        }  /* if */
        add_stop_token(tok_comma);
        /* Get the initializer for this one member. */
        member_con = get_initializer(&member_type, di_list, end_of_di_list,
                                     /*top_level=*/FALSE,
                                     any_member_uninitialized,
                                     any_const_or_ref_member_uninitialized,
                                     &local_nothing_taken);
        remove_stop_token(tok_comma);
        check_assertion(!(local_nothing_taken && is_incomplete_array));
        /* Add the constant to the list. */
        if (con_list == NULL) {
          con_list = member_con;
        } else {
          end_of_con_list->next = member_con;
        }  /* if */
        end_of_con_list = member_con;
        /* Advance to the next member of the aggregate.  Set
           any_more_members FALSE if there are no more members. */
        if (kind == (a_type_kind)tk_error) {
          /* Error case; do not advance. */
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
            curr_array_element++;
            check_assertion(!local_type->variant.array.is_variable_size_array);
            if (!is_incomplete_array &&
                local_type->variant.array.variant.number_of_elements <=
                                                       curr_array_element) {
              /* No more elements in the array. */
              any_more_members = FALSE;
            }  /* if */
          }  /* if */
        } else if (kind == (a_type_kind)tk_class ||
                   kind == (a_type_kind)tk_struct) {
          /* Advance to the next named field of the class or struct. */
          curr_field = next_initializable_field(curr_field->next);
          /* Check for no fields remaining. */
          if (curr_field == NULL) {
            any_more_members = FALSE;
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
             comma at the end of the list. */
          any_more_initializers = loop_token(tok_comma);
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
          }  /* if */
        }  /* if */
        /* Keep looping while there are more initializers. */
      }  /* while */
      /* There are no more initializers in the source (at least, none
         that should be considered part of the current aggregate). */
      if (any_more_members) {
        /* Set *any_const_or_ref_member_uninitialized if (1) there are more
           fields or array elements and (2) those fields or array elements
           are const or ref or have const or ref components. */
        if (kind == (a_type_kind)tk_error) {
          /* No action required. */
          any_more_members = FALSE;
        } else if (kind == (a_type_kind)tk_array) {
          /* We have been initializing the elements of an array, but we
             ran out of initializers before reaching the end of the array.
             However, in the aggregate case all remaining elements are
             initialized to zero by default (ARM 8.4.1), so an array of const
             elements does get properly initialized in this case; the
             nonaggregate case (that is, an array of nonaggregate classes) is
             handled by init_remaining_array_elements. */
        } else if (kind == (a_type_kind)tk_union) {
          /* No action required. */
        } else {
          check_assertion(kind == (a_type_kind)tk_struct ||
                          kind == (a_type_kind)tk_class);
          /* We have been initializing the fields of a class object, but we
             ran out of initializers before reaching the last field.  See if
             any of the remaining fields are const or ref types. */
          if (local_type->variant.class_struct_union.any_const_member ||
              (C_dialect == C_dialect_cplusplus &&
               symbol_supplement_for_class(local_type)->any_ref_member)) {
            /* We know there is at least one const or ref member.  Check the
               remaining fields (the ones that have not yet been matched up
               with an initial value in the initializer list) for one that
               needs to be initialized. */
            a_field_ptr  fp = curr_field;
            a_type_ptr   tp;

            /* Make a pass over the remaining fields.  Break out of the loop
             if a const or ref type is encountered. */
            for (; fp != NULL; fp = fp->next) {
              tp = fp->type;
              if (is_const_qualified_type(tp) || is_reference_type(tp)) {
                /* Const qualified type or reference type. */
                *any_const_or_ref_member_uninitialized = TRUE;
                break;
              } else if (is_class_struct_union_type(tp)) {
                /* Field is a class type (or an array of class-type
                   elements). */
                tp = skip_typerefs(tp);
                if (tp->variant.class_struct_union.any_const_member ||
                    (C_dialect == C_dialect_cplusplus &&
                     symbol_supplement_for_class(tp)->any_ref_member)) {
                  /* At least one sub-field of the field is a const or
                     ref. */
                  *any_const_or_ref_member_uninitialized = TRUE;
                  break;
                }  /* if */
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
      }  /* if */
      /* The entire list of values for the entity being initialized has
         now been read.  We stopped either because we exhausted the
         initial values or because we ran out of members to initialize. */
      if (top_level && !brace_flag && is_error_type(local_type)) {
        /* An error has been issued on the missing {...} list.  Although
           the initializer was scanned anyway, don't create an aggregate
           constant to represent the initialization -- just an error constant
           will do (the default upon exiting the routine. */
      } else {
        /* Set the size of an incomplete array from the number of elements
           in its initial value.  Note that arrays of char initialized
           to strings are not handled here. */
        if (is_incomplete_array) {
          /* Note that curr_array_element indicates the *next* array element
             to be initialized, and is therefore one larger than the one
             last initialized.  Thus, it is the array size. */
          set_initialized_array_size(&local_type, curr_array_element);
          *type = local_type;
          any_more_members = FALSE;
        } else if (C_dialect == C_dialect_cplusplus && brace_flag &&
                   kind == (a_type_kind)tk_array) {
          /* When the number of initializers is fewer than the number of
             array elements to be initialized, and when the element type is
             such that a constructor is required to initialize the elements,
             we are required to provide default initialization by calling
             the default constructor. */
          if (init_remaining_array_elements(
                                      local_type, curr_array_element,
                                      &con_list, &end_of_con_list, di_list,
                                      end_of_di_list,
                                      any_const_or_ref_member_uninitialized)) {
            any_more_members = FALSE;
          }  /* if */
        }  /* if */
        /* Allocate the aggregate constant that is the value for the
           initializer. */
        init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
        init_con->variant.aggregate.first_constant = con_list;
        init_con->variant.aggregate.last_constant  = end_of_con_list;
        if (any_more_members) *any_member_uninitialized = TRUE;
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
    /* If there was an initial opening brace, check for and skip the
       closing brace now. */
    check_for_matching_closing_brace(brace_flag);
  } else {
    /* Non-aggregate/union case -- initializer is a single (possibly
       brace-enclosed) value. */
    a_dynamic_init  local_di;

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
        init_con->type = local_type;
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
      case dik_constant:
        /* No side effects. */
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
  a_memory_region_number  region_to_switch_back_to = NULL_region_number;

  db_enter(4, "gen_dynamic_initialization");
  if (depth_stmt_stack >= 0) {
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
    if (struct_stmt_stack[depth_stmt_stack].any_exec_statement_seen) {
      dip->follows_an_exec_statement = TRUE;
    }  /* if */
  }  /* if */
  /* Build the dynamic initialization entry. */
  if (decl_scope_level != DEPTH_OF_FILE_SCOPE &&
      vp->storage_class == (a_storage_class)sc_static) {
    /* Initializers for local static variables must appear in the file scope
       memory region. */
    switch_to_file_scope_region(&region_to_switch_back_to);
  }  /* if */
  new_dip = alloc_dynamic_init(dip->kind);
  if (region_to_switch_back_to != NULL_region_number) {
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  *new_dip = *dip;
  /* Make the variable point at the dynamic initialization. */
  vp->init_kind = (an_init_kind)initk_dynamic;
  vp->initializer.dynamic = new_dip;
  new_dip->variable = vp;
  if (scope_stack[depth_scope_stack].kind == (a_scope_kind)sck_function ||
      scope_stack[depth_scope_stack].kind == (a_scope_kind)sck_block) {
    /* Must be the initialization of a local static variable.  Build the
       initialization statement and add it to the statement block. */
    check_assertion(vp->source_corresp.class_of_which_a_member == NULL);
    init_stmt = add_statement_at_stmt_pos((a_statement_kind)stmk_init,
                                          &vp->source_corresp.decl_position);
    init_stmt->variant.dynamic_init = new_dip;
  } else {
    /* A dynamic file-scope initialization (possible only in C++) has
       no associated stmk_init statement, so attach the dynamic initialization
       entry to the scope list. */
    check_assertion(decl_scope_level == DEPTH_OF_FILE_SCOPE ||
                    vp->source_corresp.class_of_which_a_member != NULL);
    add_to_dynamic_inits_list(new_dip);
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


void initializer(a_symbol_ptr       symbol_ptr,
                 a_source_position  *source_pos,
                 an_id_linkage_kind linkage,
                 a_boolean          parenthesized_initializer,
                 a_boolean          is_parameter,
                 a_boolean          *incomplete_type_error_reported)
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

*incomplete_type_error_reported is set to TRUE if the caller should suppress
issuing an error on an incomplete type.
*/
{
  a_boolean                      err = FALSE;
  a_boolean                      put_init_in_variable;
  a_variable_ptr                 vp = NULL;
  a_type_ptr                     vp_type = NULL;
  a_boolean                      brace_flag = FALSE;
  a_constant                     constant;
  a_dynamic_init                 local_di;
  a_boolean                      dynamic_init_required;
  a_boolean                      initialization_is_dynamic;
  an_expr_node_ptr               arg_list;
  a_class_symbol_supplement_ptr  cssp = NULL;
  a_routine_ptr                  conversion_routine;
  a_memory_region_number         region_to_switch_back_to = NULL_region_number;

  db_enter(3, "initializer");
  if (is_parameter) {
    /* Parameter declarations cannot contain an initializer.  (Declarations
       for which is_parameter is TRUE are old-style C parameter declarations.
       C++ default arguments, which look a bit like a parameter with an
       initializer -- e.g., void f(int i = 1) -- are handled elsewhere.) */
    pos_error(ec_initializer_in_param, source_pos);
    err = TRUE;
  } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
    vp = symbol_ptr->variant.variable.ptr;
  } else if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
    vp = symbol_ptr->variant.static_data_member.variable;
  } else {
    /* Not a variable (for example, might be a typedef). */
    pos_sy_error(ec_cannot_initialize, source_pos, symbol_ptr);
    err = TRUE;
  }  /* if */
  if (!err) {
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
    /* Though static data members may be given storage class of extern or
       unspecified, that fixup should not have taken place yet. */
    check_assertion(vp == NULL ||
                    vp->storage_class == (a_storage_class)sc_static);
    /* The initializer of a static data member is scanned with the original
       class reactivated. */
    push_class_reactivation_scope(symbol_ptr->class_of_which_a_member);
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
      a_source_position  pos;

      /* Use the source position of the first argument as the call position. */
      pos = pos_curr_token;
      scan_ctor_arguments(cssp->constructor, &arg_list, &conversion_routine,
                          &pos, vp_type);
      if (conversion_routine == NULL) {
        err = TRUE;
      } else {
        /* Set the dynamic init entry to represent constructor
           initialization. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
        local_di.variant.constructor.ptr = conversion_routine;
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
    /* We can't call syntax_error because the type is being displayed. */
    type_error(ec_brace_initialization_not_allowed, vp_type);
    /* Flush tokens until something in the stop token set turns up. */
    flush_tokens();
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
    a_dynamic_init_ptr  dip;

    if (scan_class_initializer_expression(vp_type, &dip)) {
      local_di = *dip;
#if 0
      /* Note that a dynamic-init entry was allocated in the subroutine,
         but it is not used here.  We just copy it into local_di, on the
         basis of which another dynamic-init entry will be allocated in
         gen_dynamic_initialization.  Using a free-list to eliminate this
         memory leakage is a possibility, but we have to take into account
         that not all the dynamic init entries will have been created in
         the file scope memory region. */
#endif /* if 0 */
    } else {
      /* No appropriate constructor was found.  Abort the initialization. */
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_none);
      err = TRUE;
    }  /* if */
    initialization_is_dynamic = TRUE;
  } else if (is_aggregate_or_union_type(vp_type) ||
             (is_error_type(vp_type) && curr_token == tok_lbrace)) {
    /* Ordinary C-style aggregate initialization, usually with a brace-
       enclosed list of values.  Except that in C++ such lists may include
       non-constants. */
    a_constant_ptr       cp;
    a_dynamic_init_ptr   di_list = NULL, end_of_di_list = NULL;
    a_boolean            any_member_uninitialized = FALSE;
    a_boolean            any_const_or_ref_member_uninitialized = FALSE;
    a_boolean            nothing_taken;

    /* Scan the initializer list. */
#if DEBUG
    if (debug_level == 4) {
      fputs("scanning initializer list for variable \"", f_debug);
      db_name(&vp->source_corresp);
      fputs("\", type = ", f_debug);
      db_abbreviated_type(vp_type);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    cp = get_initializer(&vp_type, &di_list, &end_of_di_list,
                         /*top_level=*/TRUE, &any_member_uninitialized,
                         &any_const_or_ref_member_uninitialized,
                         &nothing_taken);
    if (cp->kind == (a_constant_repr_kind)ck_error) {
      err = TRUE;
      if (is_incomplete_type(vp_type) && is_array_type(vp_type)) {
        /* Initialization of an incomplete array failed and some appropriate
           error has been reported.  Suppress further errors on this failed
           initialization. */
        *incomplete_type_error_reported = TRUE;
      }  /* if */
    } else {
      /* Check the constant kind. */
      check_assertion(cp->kind == (a_constant_repr_kind)ck_aggregate ||
                      (di_list == NULL &&
                       cp->kind == (a_constant_repr_kind)ck_string));
      if (di_list != NULL) initialization_is_dynamic = TRUE;
      clear_dynamic_init(&local_di,
                         (a_dynamic_init_kind)(initialization_is_dynamic ?
                                               dik_nonconstant_aggregate :
                                               dik_constant));
      local_di.variant.constant = cp;
      if (any_const_or_ref_member_uninitialized) {
        /* A const or ref field was not initialized. */
        if (is_union_type(vp_type)) {
          /* No diagnostic for unions. */
        } else {
          /* Issue an error. */
          an_error_code		code;
          an_error_severity	severity;
          if (C_dialect == C_dialect_cplusplus) {
            code = ec_var_with_uninitialized_member;
            severity = es_error;
          } else {
            code = ec_var_with_uninitialized_field;
            severity = es_warning;
          }  /* if */
          pos_sy_diagnostic(severity, code, source_pos, symbol_ptr);
        }  /* if */
      }  /* if */
      if (any_member_uninitialized) vp->is_partially_initialized = TRUE;
    }  /* if */
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
    if (local_di.destructor == NULL) {
      /* Check for the existence of a destructor independently of checks for a
         constructor.  This is to catch the unusual case in which a user has
         defined a destructor but the object can be initialized without a
         constructor. */
      if (cssp != NULL) {
        a_routine_ptr rp = select_destructor(vp_type, vp_type, source_pos,
                                             /*honor_virtual=*/FALSE,
                                             /*evaluated=*/TRUE,
                                             /*suppress_access_check=*/FALSE);
        if (rp != NULL) {
          local_di.destructor = rp;
          initialization_is_dynamic = TRUE;
        }  /* if */
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


void repeat_nonconstant_init(a_dynamic_init_ptr  ctor_dip,
                             a_type_ptr          elem_type,
                             a_dynamic_init_ptr  new_dip,
                             a_targ_size_t       count)
/*
Define a dynamic init entry for a nonconstant aggregate, which will always be
for an array whose elements (of type elem_type) are to be initialized by a
series of constructor calls.  The dynamic entry to be defined (new_dip) has
already been allocated; the dynamic init entry that represents the constructor
call is ctor_dip.  count is the number of elements in the array to be
initialized.
*/
{
  a_constant_ptr           aggr_con, repeat_con, dynamic_init_con;

  /* The IL structure is
       dynamic init (ck_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init (ck_constructor)
  */
  /* Create a ck_aggregate constant. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
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
  a_boolean                      def_init_performed = FALSE;
  a_variable_ptr                 var = NULL;
  a_type_ptr                     var_type, tp;
  a_class_symbol_supplement_ptr  cssp;
  a_dynamic_init                 local_di;
  a_routine_ptr                  ctor = NULL, dtor = NULL;
  a_targ_size_t                  count;
  a_memory_region_number         region_to_switch_back_to = NULL_region_number;

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
    tp = var_type = skip_typerefs(var->type);
    if (is_array_type(tp)) {
      tp = skip_typerefs(underlying_array_element_type(tp));
    }  /* if */
    /* Default initialization is done only for objects that are defined in
       the current translation unit (i.e., storage class other than "extern")
       and that require constructor initialization. */
    if (is_class_struct_union_type(tp) &&
        var->storage_class != (a_storage_class)sc_extern &&
        !is_incomplete_type(var_type)) {
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        /* Perform the default initialization of a static data member with
           its parent class reactivated. */
        push_class_reactivation_scope(sym->class_of_which_a_member);
      }  /* if */
      cssp = symbol_supplement_for_class(tp);
      if (cssp->constructor != NULL) {
        ctor = select_default_constructor(tp, err_pos, tp, /*evaluated=*/TRUE);
        /* Even if ctor is NULL (as a result of failing to find a default
           constructor) we still set def_init_performed as though default
           initialization were done even though it wasn't -- this will
           prevent a redundant diagnostic from being issued. */
        def_init_performed = TRUE;
      }  /* if */
      dtor = select_destructor(tp, tp, err_pos,
                               /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                               /*suppress_access_check=*/FALSE);
      if (ctor == NULL && dtor == NULL) {
        /* No constructor for default initialization; no destructor either. */
      } else {
        if (ctor != NULL) {
          /* Normal case -- there's a constructor to do the initialization. */
          a_param_type_ptr  ptp = (skip_typerefs(ctor->type))->
                                   variant.routine.extra_info->param_type_list;

          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
          local_di.variant.constructor.ptr = ctor;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          local_di.variant.constructor.args = copy_default_arg_expr_list(ptp);
        } else {
          /* Default initialization of an object that has a destructor.  We
             generate a dik_none dynamic initialization entry for this object,
             even though it is not actually initialized, so that the existence
             of the destructor can be duly recorded. */
          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_none);
        }  /* if */
        local_di.destructor = dtor;
        /* A constructor (or at least a destructor) was found and a dynamic
           init entry (local_di) was set to represent the initialization. */
        if (var_type != tp) {
          /* The object has an array type.  We need to build an aggregate
             initialization on top of the other dynamic init entry. */
          a_dynamic_init  *dip;

          if (decl_scope_level != DEPTH_OF_FILE_SCOPE &&
              var->storage_class == (a_storage_class)sc_static) {
            /* Initializers for local static variables must appear in
               the file scope memory region. */
            switch_to_file_scope_region(&region_to_switch_back_to);
          }  /* if */
          /* Copy the dynamic init entry. */
          dip = alloc_dynamic_init(local_di.kind);
          *dip = local_di;
          /* Now that the local_di has been copied, reinitialize it. */
          clear_dynamic_init(&local_di,
                               (a_dynamic_init_kind)dik_nonconstant_aggregate);
          /* Compute the repeat count. */
          count = var_type->size / tp->size;
          /* Build the repeat construct. */
          repeat_nonconstant_init(dip, tp, &local_di, count);
          if (region_to_switch_back_to != NULL_region_number) {
            switch_back_to_original_region(region_to_switch_back_to);
          }  /* if */
        }  /* if */
        /* Allocate a dynamic init entry (a copy of local_di) and attach it
           to the variable. */
        gen_dynamic_initialization(var, &local_di, err_pos);
#if DEBUG
        if (debug_level >= 3) {
          db_variable(var);
          fputs(",\n", f_debug);
          db_initializer(var, 2);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_static_data_member) {
        pop_class_reactivation_scope();
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
  a_constructor_init_ptr        cip, new_cip, prev_cip, next_cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list, end_of_virtual_list;
  a_constructor_init_ptr        direct_list, end_of_direct_list;
  a_base_class_ptr              bcp;
  a_class_type_supplement_ptr   ctsp;
  a_class_symbol_supplement_ptr cssp;
  a_routine_ptr                 conversion_routine, rp;
  a_dynamic_init_ptr            dip, ctor_dip;
  int                           direct_base_class_count = 0;
  a_source_position             lparen_pos;
  a_constructor_init_ptr        uninit_list = NULL, end_of_uninit_list = NULL;

  db_enter(3, "ctor_initializer");
  class_type = ((a_symbol_ptr)ctor_rout->source_corresp.assoc_info)->
                                                   class_of_which_a_member;
  check_assertion(class_type != NULL);
  ctsp = class_type->variant.class_struct_union.extra_info;
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
      if (is_generated_cctor) {
        /* All fields are explicitly listed for a generated copy constructor,
           since even if there is no constructor at least a bitwise copy is
           required. */
      } else {
        /* This is not a copy constructor.  See if this is a field that
           requires an initializer. */
        tp = sym->variant.field.ptr->type;
        if (is_reference_type(tp) || is_const_qualified_type(tp)) {
          /* Ref-type fields and const and array-of-const fields require an
             initializer. */
        } else {
          if (is_array_type(tp)) {
            tp = skip_typerefs(underlying_array_element_type(tp));
          }  /* if */
          if (is_class_struct_union_type(tp) &&
              ((cssp = symbol_supplement_for_class(tp))->constructor != NULL ||
               (exceptions_enabled && cssp->destructor != NULL))) {
            /* When the type of the field has a constructor, a constructor
               initializer is required.  Otherwise, if it has a destructor
               and exception handling is enabled, we put out a constructor
               initializer entry anyway, just to record the destructor. */
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
            new_cip->compiler_generated = FALSE;
            bcp = new_cip->variant.base_class;
            check_assertion(bcp->direct);
            init_type = bcp->type;
            if (new_cip->initializer != NULL) {
              type_error(ec_base_class_already_initialized, init_type);
              err = TRUE;
            } else {
              type_diagnostic(anachronism_error_severity,
                              ec_base_class_init_anachronism, init_type);
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
          member_or_base_sym = coalesce_and_lookup_generalized_identifier
                                   (GID_NO_OPTIONS, ilm_ctor_initializer_name,
                                    &gid_err);
          err |= gid_err;
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
        mark_referenced(member_or_base_sym, &error_position);
        if (member_or_base_sym->kind == (a_symbol_kind)sk_field &&
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
        } else if (is_class_symbol(member_or_base_sym)) {
          /* It is a base class of the current class for which initialization
             is to be done. */
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
          if (is_qualified_type(init_type)) {
            bcp = NULL;
          } else {
            a_base_class_ptr  found_bcp = NULL;
            init_type = skip_typerefs(init_type);
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
              err = TRUE;
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
          if (is_class_struct_union_type(init_type)) {
            cssp = symbol_supplement_for_class(init_type);
          } else {
            cssp = NULL;
          }  /* if */
          if (curr_token == tok_rparen &&
              (cssp == NULL || cssp->constructor == NULL)) {
            if (cssp != NULL) {
              /* "class-name()" can only be a default constructor call,
                 but only issue a warning since it's not clearly outlawed. */
              pos_ty_warning(ec_no_constructor, &lparen_pos, init_type);
            } else {
              /* "field-name()" is, technically, permitted by the syntax, so
                 we can only issue a warning. */
              pos_warning(ec_exp_primary_expr, &pos_curr_token);
            }  /* if */
            /* Bypass the right paren. */
            (void)get_token();
            if (new_cip != NULL) {
              /* Set the initializer field to record that an initialization
                 (such as it is) has been attempted. */
              new_cip->initializer = 
                            alloc_dynamic_init((a_dynamic_init_kind)dik_none);
            }  /* if */
          } else if (cssp != NULL && cssp->constructor != NULL) {
            /* This is either a base class or a field of class type.  In
               either case, it will be initialized by a constructor call if
               a constructor exists.  Otherwise, it will be initialized
               like any scalar. */
            an_expr_node_ptr  arg_list;
            a_type_ptr        object_class_type;

            /* If it is a base class, the object being constructed is the
               whole class (and the base class is an incomplete subobject
               thereof).  If it is a field, the object being constructed is
               field itself.  Set the object class type accordingly. */
            if (new_cip->kind == (a_constructor_init_kind)cik_field) {
              object_class_type = init_type;
            } else {
              object_class_type = class_type;
            }  /* if */
            /* This is treated like an initialization of the form
               S x (arg [, ...]), where S is a class type name.  Depending
               on the arguments present, a constructor will be selected and
               returned.  The scan function returns FALSE if it finds no
               constructor for which the arguments match. */
            scan_ctor_arguments(cssp->constructor, &arg_list,
                                &conversion_routine, &lparen_pos,
                                object_class_type);
            if (conversion_routine == NULL) err = TRUE;
            if (!err) {
              /* Set the dynamic init entry to represent constructor
                 initialization. */
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
              dip->variant.constructor.ptr = conversion_routine;
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
            /* A field whose initialization does not involve a constructor. */
            add_stop_token(tok_rparen);
            /* Allocate a new dynamic init entry, setting the kind to
               dik_none for now.  It will be adjusted after the scan. */
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
            scan_initializer_of_simple_object(/*nonconst_allowed=*/TRUE,
                                              init_type, dip);
            if (new_cip != NULL) new_cip->initializer = dip;
            remove_stop_token(tok_rparen);
            if (!required_token(tok_rparen, ec_exp_rparen)) {
              /* Special code to avoid poor error recovery in cases where
                 a comma-list appears between the parens in what is taken
                 to be the initializer of a simple object -- e.g.,
                     A::A(int i, int j) : x(i,j) { }
                 If there is no constructor for x then it is interpreted as
                 a simple object, only "i" is scanned, and an error is issued
                 on the expected ")".  After that we want to bypass the rest
                 of the comma-list before resuming scanning. */
              if (curr_token == tok_comma) {
                a_stop_token_array  save_stop_token_array;
                /* Save the current stop token state, and reinitialize it. */
                copy_stop_tokens(stop_token_array, save_stop_token_array);
                stop_token_array[(int)tok_comma] = 0;
                /* Flush the tokens till a stop-token is reached. */
                flush_tokens();
                /* Restore the original stop token state. */
                copy_stop_tokens(save_stop_token_array, stop_token_array);
              }  /* if */
            }  /* if */
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
  for (cip = cip_list; cip != NULL; cip = next_cip) {
    a_boolean          is_const_qualified;
    a_source_position  err_pos;
    /* object_class_type is the type of the object being created.
       For base classes it will be different than the type associated
       with the constructor being called.  For fields it will be
       the same as the field type.  This is needed to check protected
       member access. */
    a_type_ptr         object_class_type;

    next_cip = cip->next;
    if (cip->initializer == NULL ||
        cip->initializer->kind == (a_dynamic_init_kind)dik_none ||
        exceptions_enabled) {
      /* Either no initializer was explicitly specified or a destructor, if
         any, has to be entered for exception handling support. */
      array_type = NULL;
      object_class_type = NULL;
      cssp = NULL;
      is_const_qualified = FALSE;
      if (user_defined) err_pos = pos_curr_token;
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        /* Get the field type.  For arrays, we want the element type. */
        tp = cip->variant.field->type;
        if (is_const_qualified_type(tp)) is_const_qualified = TRUE;
        tp = skip_typerefs(tp);
        if (is_array_type(tp)) {
          array_type = tp;
          tp = skip_typerefs(underlying_array_element_type(tp));
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
      if (cip->initializer != NULL &&
          cip->initializer->kind != (a_dynamic_init_kind)dik_none) {
        /* This must be a special exception handling case -- the initializer
           has already been processed, and the destructor, if any, has to
           recorded. */
        check_assertion(array_type == NULL);
        dip = cip->initializer;
      } else if (is_generated_cctor) {
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
                                       &err_pos, object_class_type,
                                       &bitwise_copy, /*evaluated=*/TRUE,
                                       /*suppress_access_check=*/FALSE);
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
          a_param_type_ptr  ptp = (skip_typerefs(rp->type))->
                                   variant.routine.extra_info->param_type_list;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = rp;
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
          dip->variant.constructor.
                            is_copy_constructor_with_implied_source = TRUE;
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
            continue;
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
          a_param_type_ptr  ptp = (skip_typerefs(rp->type))->
                                   variant.routine.extra_info->param_type_list;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = rp;
          /* A user defined default constructor may have default args that
             should be incorporated into the constructor call. */
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
        }  /* if */
      }  /* if */
      if (exceptions_enabled && cssp != NULL && cssp->destructor != NULL) {
        /* If exception handling is enabled, record the destructor in the
           constructor initializer.  This is required if an exception
           occurs in the middle of constructing an object of this type --
           the information is used to register which destructors need to be
           called for a partially constructed object. */
        dip->destructor = select_destructor(tp, object_class_type, &err_pos,
                                            /*honor_virtual=*/FALSE,
                                            /*evaluated=*/TRUE,
                                            /*suppress_access_check=*/FALSE);
      }  /* if */
      if (array_type != NULL &&
          dip->kind == (a_dynamic_init_kind)dik_constructor) {
        a_targ_size_t count;
        /* We have an array of objects with constructors.  Create a dynamic
           init entry to handle the aggregate. */
        ctor_dip = dip;
        dip =
           alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
        /* Build the looping constant entry. */
        if (array_type->size == 0) {
          count = 1;
        } else {
          count = array_type->size / tp->size;
        }  /* if */
        repeat_nonconstant_init(ctor_dip, tp, dip, count);
      }  /* if */
      /* Attach the new dynamic init entry to the constructor initializer. */
      cip->initializer = dip;
    }  /* if */
    prev_cip = cip;
  }  /* for */
  if (uninit_list != NULL) {
    /* Issue an error for uninitialized const and ref members. */
    if (ctor_rout->compiler_generated) {
      pos_ty_start_error(ec_cannot_initialize_fields, &pos_curr_token,
                         class_type);
    } else {
      pos_sy_start_error(ec_missing_initializer_on_fields, &pos_curr_token,
                         (a_symbol_ptr)ctor_rout->source_corresp.assoc_info);
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
  { a_routine_ptr new_routine;
    /* Determine and remember the default operator new() routine for the
       class.  This is done here because we are working out the "wrapper"
       code that will be required, and the "new" routine will be called from
       the wrapper. */
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
        mark_routine_referenced(delete_routine);
      }  /* if */
    }  /* if */
  }
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
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
  a_type_ptr                    class_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym;
  a_constructor_init_ptr        cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list;
  a_routine_ptr                 rp;
  a_base_class_ptr              bcp;
  a_dynamic_init_ptr            dip;
  a_class_type_supplement_ptr   ctsp;
  a_source_position             source_pos;

  db_enter(3, "dtor_initializer");
  source_pos = dtor_rout->source_corresp.decl_position;
  class_type = ((a_symbol_ptr)dtor_rout->source_corresp.assoc_info)->
                                                   class_of_which_a_member;
  check_assertion(class_type != NULL);
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* The order of destructor calls is exactly the reverse of the order of
     constructor calls.  In other words, destructors for virtual base classes
     are last, preceded by destructors for nonvirtual direct base classes,
     with destructors for members coming first (ARM 12.4).  Thus we follow the
     logic in ctor_initializer, except that the lists are built backwards and
     merged backwards.   First construct the lists for virtual base classes
     and nonvirtual direct base classes. */
  virtual_list = NULL;
  cip_list = end_of_cip_list = NULL;
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->is_virtual || bcp->direct) {
      /* If the virtual base class or direct base class has a destructor, a
         dynamic init entry will be required.  Create the constructor init
         entry now; the dynamic init will be added later. */
      rp = select_destructor(bcp->type, class_type, &source_pos,
                             /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                             /*suppress_access_check=*/FALSE);
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
        rp = select_destructor(tp, tp, &source_pos,
                               /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                               /*suppress_access_check=*/FALSE);
        if (rp != NULL) {
          /* Create the constructor init entry for a field. */
          cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
          cip->variant.field = sym->variant.field.ptr;
          cip->compiler_generated = TRUE;
          /* Create a dynamic init entry. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = rp;
          if (array_type != NULL) {
            a_targ_size_t count;
            /* We have an array of objects with destructors.  Create a dynamic
               init entry to handle the aggregate. */
            a_dynamic_init_ptr  dtor_dip = dip;
            dip = alloc_dynamic_init(
                             (a_dynamic_init_kind)dik_nonconstant_aggregate);
            /* Build the looping constant entry. */
            if (array_type->size == 0) {
              count = 1;
            } else {
              count = array_type->size / tp->size;
            }  /* if */
            repeat_nonconstant_init(dtor_dip, tp, dip, count);
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
    set_class_assoc_operator_delete_routine(class_type);
    delete_routine = ctsp->assoc_operator_delete_routine;
    if (delete_routine != NULL) {
      mark_routine_referenced(delete_routine);
      delete_routine->called = TRUE;
    }  /* if */
  }
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
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
This routine is called when no explicit or default initialization has
occurred.  It determines whether an initializer should have been provided
and issues a diagnostic if appropriate.  It is used both for variable
declarations (when sym represents the variable) and for unnamed objects that
are created by a new expression (in which case sym is NULL).  In both cases
"type" points to the type of the object.
*/
{
  a_variable_ptr       vp;
  a_name_linkage_kind  name_linkage;
  a_boolean            init_required;
  a_base_class_ptr     bcp;

  db_enter(4, "check_for_missing_initializer");
  if (sym != NULL) {
    /* This must be a variable declaration. */
    check_assertion(sym->kind == (a_symbol_kind)sk_variable);
    vp = sym->variant.variable.ptr;
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
    if (is_array_type(type)) type = underlying_array_element_type(type);
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(type) &&
        !symbol_supplement_for_class(type)->any_nonstatic_data_members) {
      /* Uninitialized const object that is an "empty" class (i.e., one with
         no nonstatic data members).  No error is issued. */
      /* Note that the ARM can be read as requiring initialization of
         const objects even when they are empty.  Other C++ compilers don't
         enforce such a restriction, however. */
    } else if (vp != NULL) {
      /* Uninitialized const variable.  In C++ this is permitted only for
         externally linked variables.  In ordinary C we issue a warning for
         local variables (both static and automatic) here, but the warning
         for static file scope variables is given later. */
       name_linkage = (a_name_linkage_kind)vp->source_corresp.name_linkage;
       if (C_dialect == C_dialect_cplusplus) {
         if (name_linkage == (a_name_linkage_kind)nlk_none ||
             (name_linkage == (a_name_linkage_kind)nlk_internal &&
              decl_scope_level == DEPTH_OF_FILE_SCOPE)) {
           /* In C++ const qualified variables that are internally linked
              must be initialized (ARM 7.1.6). */
           sym_error(ec_missing_initializer_on_const, sym);
         }  /* if */
       } else {
         /* Ordinary C -- a warning, and only on local variables. */
         if (name_linkage == (a_name_linkage_kind)nlk_none) {
           sym_warning(ec_missing_initializer_on_const, sym);
        }  /* if */
      }  /* if */
    } else {
      /* Uninitialized const new-object.  Issue a warning.  (One can infer
         from the ARM that an error is required, but it's not explicit.
         Until the language definition is improved, we'll let it by.) */
      warning(ec_missing_initializer_on_unnamed_const);
    }  /* if */
  } else {
    if (is_array_type(type)) type = underlying_array_element_type(type);
    type = skip_typerefs(type);
    if (is_union_type(type)) {
      /* No diagnostic on unions with const/ref members. */
    } else if (is_class_struct_union_type(type) &&
               (vp == NULL ||
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
          an_error_code		code;
          an_error_severity	severity;
          if (C_dialect == C_dialect_cplusplus) {
            code = ec_var_with_uninitialized_member;
            severity = es_error;
          } else {
            code = ec_var_with_uninitialized_field;
            severity = es_warning;
          }  /* if */
          pos_sy_diagnostic(severity, code, &sym->decl_position, sym);
        } else {
          /* New object -- there's no name to display.  Again, just issue
             a warning (until the language definition is clearer about this
             kind of case). */
          warning(ec_unnamed_object_with_uninitialized_field);
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
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
