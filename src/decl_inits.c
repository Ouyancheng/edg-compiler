/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
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
#include "exprutil.h"
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


static void check_constant_initializer (a_constant *constant,
                                        a_type_ptr *type,
                                        a_boolean  *err)
/*
Check that the given constant is acceptable as an initial value of an object
of the given type.  Change the constant's type, or remove the final null
from a string literal, if necessary.  If type is an incomplete array of
char type, and *constant is a string, change the number of elements in the
incomplete array type by copying and modifying the type.  *err is returned
TRUE if there was an error of some kind.
*/
{
  a_type_ptr    array_type;
  a_targ_size_t string_length;
  a_targ_size_t array_length;

  *err = FALSE;
  if (is_char_array_type(*type)) {
    /* The object to be initialized is an array (possibly incomplete) of
       char -- i.e., a string. */
    if (constant->kind != (a_constant_repr_kind)ck_string) {
      /* The constant is not a string. */
      error(ec_bad_initializer_type);
      *err = TRUE;
    } else {
      /* The constant is a string. */
      string_length = constant->variant.string.length;
      array_type = skip_typerefs(*type);
      if (is_incomplete_type(array_type)) {
        /* The array type is incomplete, and therefore the array size
           is set from the string length. */
        set_initialized_array_size(type, string_length);
      } else {
        /* The object being initialized is an array that has a definite
           size.  See if the string will fit in the array. */
        array_length = array_type->variant.array.number_of_elements;
        if (string_length > array_length) {
          /* The string is longer than the array.  Check to see if the
             string will fit if we drop the final null.  See 3.5.7. */
          if (string_length-1 == array_length) {
            /* Decrement the string length, and change its type,
               thus "dropping" the final null.  Note that this depends on
               the string not being shared. */
            constant->variant.string.length = --string_length;
            constant->type = string_type(string_length);
          } else {
            /* The initializer string is too long for the array being
               initialized. */
            error(ec_bad_initializer_type);
            *err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* All types other than string.  Use the assignment compatibility
       rules.  Change the type of the constant if necessary (this is
       effectively a cast to the type of the object being initialized). */
    constant_prepare_assignment(constant, *type, ec_bad_initializer_type, err);
  }  /* if */
}  /* check_constant_initializer */


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


static void scan_initializer_of_simple_object(
                                       a_boolean       nonconst_allowed,
                                       a_boolean       convert_array_to_ptr,
                                       a_type_ptr      *type,
                                       a_dynamic_init  *dip,
                                       a_boolean       *err)
{
  an_expr_node_ptr    expression;
  a_boolean           is_constant;
  a_constant          constant;

  if (nonconst_allowed) {
    /* Scan a potentially non-constant initializer expression.  The result
       of the scan is a constant if the expression is constant, and an
       expression node if not. */
    scan_initializer_expression(convert_array_to_ptr, &is_constant,
                                &expression, &constant, err);
  } else {
    /* Non-constant is not allowed. */
    scan_constant_initializer_expression(convert_array_to_ptr, &constant, err);
    is_constant = TRUE;
  }  /* if */
  if (!*err) {
    /* See if the scanned expression was constant or not. */
    if (is_constant) {
      /* Constant.  Check the constant type to see if it is legal,
         change the constant type if necessary. */
      check_constant_initializer(&constant, type, err);
      if (!*err) {
        /* Set the dynamic init entry to represent constant initialization.
           (A local dynamic init entry is used only for convenience --
           dynamic initialization is not presumed.) */
        clear_dynamic_init(dip, (a_dynamic_init_kind)dik_constant);
        dip->variant.constant = alloc_unshared_constant(&constant);
      }  /* if */
    } else {
      /* Non-constant.  Check the type by assignment rules and cast the
         node if necessary. */
      node_prepare_assignment(&expression, *type, ec_bad_initializer_type,
                              err);
      if (!*err) {
        /* Set the dynamic init entry to represent non-constant assignment
           initialization. */
        clear_dynamic_init(dip, (a_dynamic_init_kind)dik_expression);
        dip->variant.expression = expression;
      }  /* if */
    }  /* if */
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
  a_constant_ptr      con_list, end_con_list;
  a_constant_ptr      member_con;
  a_constant          constant;
  a_targ_size_t       curr_array_element;
  a_field_ptr         curr_field;
  a_boolean           done, no_more_members;
  a_type_kind         kind;
  a_boolean           array_too_long_error_given = FALSE;
  a_boolean           took_extra_comma;
  an_expr_node_ptr    expression;
  a_dynamic_init_ptr  dip;
  a_routine_ptr       rp;
  a_source_position   expr_pos;
  a_class_symbol_supplement_ptr
                      cssp;

  db_enter(4, "get_initializer");
  err = FALSE;
  local_type = skip_typerefs(*type);
  if (is_class_struct_union_type(local_type)) {
    cssp = symbol_supplement_for_class(local_type);
  } else {
    cssp = NULL;
  }  /* if */
  if (cssp != NULL && cssp->constructor != NULL) {
#if CHECKING
    if (top_level) {
      internal_error("get_initializer: constructor encountered at top level");
#if 0
    } else if (cssp->copy_constructor == NULL) {
      internal_error("get_initializer: missing copy constructor");
#endif /* if 0 */
    }  /* if */
#endif /* CHECKING */
    /* This is an array element that can only be initialized by a
       constructor.  Treat the expression as an argment for the constructor
       call. */
    copy_source_position(pos_curr_token, expr_pos);
    expression = scan_argument_expression();
    /* Look for a constructor to convert the right hand side to the
       required class type. */
    if (!select_constructor(cssp->constructor, &rp, &expression, &expr_pos)) {
      /* No such constructor was found.  Abort the initialization. */
      err = TRUE;
#if 0
    } else if (rp != cssp->copy_constructor->variant.routine) {
      /* Something other than the copy constructor was returned, so be sure
         the copy constructor is accessible. */
      if (!have_access_to_symbol(cssp->copy_constructor)) {
        /* It is an error if the copy constructor is inaccessible, even
           though it is being optimized away. */
        pos_error(ec_inaccessible_copy_constructor, &expr_pos);
        err = TRUE;
      }  /* if */
#endif /* if 0 */
    }  /* if */
    if (!err) {
      init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
      init_con->variant.dynamic_init = dip =
                  alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
      dip->variant.constructor.routine = rp;
      dip->variant.constructor.args = expression;
      if (*di_list == NULL) {
        *di_list = dip;
      } else {
        (*end_of_di_list)->next = dip;
      }  /* if */
      *end_of_di_list = dip;
      /* Mark the constructor as referenced. */
      rp->source_corresp.referenced = TRUE;
    }  /* if */
  } else if (is_aggregate_or_union_type(local_type) ||
             (is_error_type(local_type) && curr_token == tok_lbrace)) {
    /* Initialization of an array (complete or incomplete), struct, or
       union.  The result will be an aggregate constant except when an
       array of char is initialized by an string.  The initial
       values can either appear inside a brace-enclosed list, or at
       the current level. */
    check_for_opening_brace(&brace_flag);
    if (is_char_array_type(local_type) && curr_token == tok_string_literal) {
      /* The object being initialized has type array of char, and is
         being initialized with a string.  Handle this case specially. */
      scan_constant_initializer_expression(/*convert_array_to_pointer=*/FALSE,
                                           &constant, &err);
      if (!err) {
        /* Check the type of the string against the type of the object
           being initialized, adjust one or the other if necessary. */
        check_constant_initializer(&constant, &local_type, &err);
        if (!err) {
          /* Allocate the string constant. */
          init_con = alloc_unshared_constant(&constant);
          /* Pass the type back to the caller; the array size is now
             known if it was incomplete. */
          *type = local_type;
        }  /* if */
      }  /* if */
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
        if (kind != (a_type_kind)tk_class &&
            kind != (a_type_kind)tk_struct &&
            kind != (a_type_kind)tk_union) {
          internal_error("get_initializer: not array or class/struct/union");
        }  /* if */
#endif /* CHECKING */
        curr_field = local_type->variant.class_struct_union.field_list;
        done = (curr_field == NULL);
      }  /* if */
      con_list = end_con_list = NULL;
      took_extra_comma = FALSE;
      /* Loop, scanning initializers. */
      while (!done) {
        /* Determine the type of the member being initialized. */
        if (kind == (a_type_kind)tk_array || kind == (a_type_kind)tk_error) {
          /* member_type was set outside the loop. */
        } else if (kind == (a_type_kind)tk_class ||
                   kind == (a_type_kind)tk_struct ||
                   kind == (a_type_kind)tk_union) {
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
          end_con_list->next = member_con;
        }  /* if */
        end_con_list = member_con;
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
                curr_array_element) no_more_members = TRUE;
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
        /* Note that curr_array_element indicates the NEXT array element
           to be initialized, and is therefore one larger than the one
           last initialized.  Thus, it is the array size. */
        set_initialized_array_size(&local_type, curr_array_element);
        *type = local_type;
      }  /* if */
      /* Allocate the aggregate constant that is the value for the
         initializer. */
      init_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      init_con->variant.aggregate.first_constant = con_list;
      init_con->variant.aggregate.last_constant  = end_con_list;
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

    check_for_opening_brace(&brace_flag);
    scan_initializer_of_simple_object(/*nonconst_allowed=*/
                                            (C_dialect == C_dialect_cplusplus),
                                      /*convert_array_to_pointer=*/TRUE,
                                      &local_type, &local_di, &err);
    if (!err) {
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
    }  /* if */
    /* If there was an initial opening brace, check for and skip the
       closing brace now. */
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


static void gen_dynamic_initialization(a_variable_ptr      vp,
                                       a_dynamic_init_ptr  dip)
/*
Generate a dynamic initialization of the variable vp.  If
kind == dik_constant, constant points to the initial value constant;
if kind == dik_expression, expression points to the initial value expression.
Except for a dynamic initialization at file scope (possible only in C++),
also create an stmk_init statement at the current point in the code.
*/
{
  a_dynamic_init_ptr      new_dip;
  a_statement_ptr         init_stmt;
  an_expr_node_ptr        node;

  db_enter(4, "gen_dynamic_initiailization");
  /* Build the dynamic initialization entry. */
  new_dip = alloc_dynamic_init(dip->kind);
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_constant:
      new_dip->variant.constant = dip->variant.constant;
      break;
    case dik_expression:
      new_dip->variant.expression = dip->variant.expression;
      break;
    case dik_constructor:
      new_dip->variant.constructor.routine = dip->variant.constructor.routine;
      new_dip->variant.constructor.args = dip->variant.constructor.args;
      break;
    case dik_nonconstant_aggregate:
      new_dip->variant.aggregate.aggr_const =
                                  dip->variant.aggregate.aggr_const;
      new_dip->variant.aggregate.dynamic_init_list =
                                  dip->variant.aggregate.dynamic_init_list;
      break;
#if CHECKING
    default:
      internal_error("gen_dynamic_initialization: bad kind");
#endif /* CHECKING */
  }  /* switch */
  new_dip->destructor = dip->destructor;
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
    node = alloc_expr_node((an_expr_node_kind)enk_variable_address);
    node->type = vp->type;
    node->variant.variable = vp;
    init_stmt->expr = node;
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
  a_symbol_ptr     ext_sym;
  a_symbol_locator locator, ext_locator;

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
    ext_sym = find_external_symbol(&locator,
                                   /*is_static=*/(linkage==idl_internal),
                                   &ext_locator);
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
                 a_boolean          paren_flag,
                 a_boolean          is_parameter)
/*
Scan an initializer (3.5.7) for the symbol pointed to by symbol_ptr
(with linkage as given by linkage; a parameter if is_parameter is TRUE).
The source position of the symbol (which may differ from the decl_position
in symbol_ptr if this is a second declaration) is given by *source_pos.
The syntax is:

3.5.7  initializer:
		assignment-expression
		{ initializer-list }
		{ initializer-list , }

       initializer-list:
		initializer-list
		initializer-list , initializer

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
  a_routine_ptr                  rp;

  db_enter(3, "initializer");

  if (is_parameter) {
    /* Parameter declarations cannot contain an initializer.  (Declarations
       for which is_parameter is TRUE are old-style C parameter declarations.
       A C++ default argument, which looks a bit like a parameter with an
       initializer -- e.g., void f(int i = 1) -- are handled elsewhere.) */
    error(ec_initializer_in_param);
    err = TRUE;
  } else if (symbol_ptr->kind != (a_symbol_kind)sk_variable &&
             symbol_ptr->kind != (a_symbol_kind)sk_static_data_member) {
    /* Not a variable (for example, might be a typedef). */
    pos_error(ec_cannot_initialize, source_pos);
    err = TRUE;
  } else {
    vp = symbol_ptr->variant.variable;
    vp_type = vp->type;
    if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
        symbol_ptr->decl_scope != FILE_SCOPE_NUMBER && 
        linkage != idl_none) {
      /* Block scope variable with internal or external linkage --
         not allowed to be initialized.  (3.5.7 Constraints) */
      pos_error(ec_cannot_initialize, source_pos);
      err = TRUE;
    } else if (vp->init_kind != (an_init_kind)initk_none) {
      /* Variable already initialized (presumably, it is being declared
         again, and we have the variable from the earlier declaration). */
      pos_error(ec_already_initialized, source_pos);
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
      } else if (vp_type->kind == (a_type_kind)tk_reference) {
        /* Reference type -- okay. */
      } else {
        /* An object of this type cannot be initialized. */
        pos_error(ec_cannot_initialize, source_pos);
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
    (void)push_scope((a_scope_kind)sck_class_reactivation,
                     symbol_ptr->decl_scope,
                     symbol_ptr->class_of_which_a_member, (a_routine_ptr)NULL);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(vp_type)) {
    cssp = symbol_supplement_for_class(vp_type);
  }  /* if */
  if (cssp != NULL && paren_flag) {
    /* This is an initialization of the form S x (arg [, ...]), where S is a
       class type name.  Depending on the arguments present, a constructor,
       possibly the copy constructor, will be selected and returned.  The
       scan function returns FALSE if it finds no constructor for which the
       arguments match. */
    if (!scan_ctor_arguments(cssp->constructor, &rp, &arg_list)) {
      err = TRUE;
    } else {
      /* Set the dynamic init entry to represent constructor initialization. */
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
      local_di.variant.constructor.routine = rp;
      local_di.variant.constructor.args = arg_list;
      /* Mark the constructor referenced. */
      rp->source_corresp.referenced = TRUE;
    }  /* if */
    initialization_is_dynamic = TRUE;
  } else if (cssp != NULL && cssp->constructor != NULL &&
             curr_token == tok_lbrace) {
    /* This is an attempt to do C-style aggregate initialization on a class
       object for which a constructor exists.  In such cases the constructor
       must be used. */
    syntax_error(ec_brace_initialization_not_allowed);
    err = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             is_class_struct_union_type(vp_type) && curr_token != tok_lbrace) {
    /* Special C++ case:  a class aggregate may be initialized with an object
       of its class or a class derived from it.  E.g., if S is the name of a
       struct and x is an S, then S y = x is permitted.  In addition, x may
       be any expression of a type for which there is a type conversion to S.
       Thus S y = 1 is a legal initialization if S(int) exists to perform the
       conversion. */
    /* Scan the expression on the right hand side of the equal sign. */
    expression = scan_argument_expression();
    if (cssp->constructor == NULL) {
      /* The case of C-style structs.  No constructor exists, but simple
         struct assignment can be performed.  Check the type by assignment
         rules. */
      node_prepare_assignment(&expression, vp_type,
                              ec_bad_initializer_type, &err);
      if (!err) {
        /* Set the dynamic init entry to represent non-constant assignment
           initialization. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_expression);
        local_di.variant.expression = expression;
      }  /* if */
    } else {
      /* Initializing a class object that has a constructor in a statement
         that looks like an assignment (see discussion in ARM 12.6.1).
         It is as though the expression on the right hand side is constructed
         into a temporary and then a copy constructor is called to actually
         do the initialization -- e.g., complex x = 1 is to be treated as
         complex x = complex(1).  The policy is that the copy constructor
         (which is guaranteed to exist) must be accessible for the statement
         to be legal, but, as an optimization, it need not actually be used in
         the operation.  Accordingly, the following constructs directly into
         the object being initialized -- e.g., complex x = 1 is treated as
         complex x(1). */
#if CHECKING
#if 0
      if (cssp->copy_constructor == NULL) {
        internal_error("initializer: missing copy constructor");
      }  /* if */
#endif /* if 0 */
#endif /* CHECKING */
      /* Look for a constructor to convert the right hand side to the
         required class type. */
      if (!select_constructor(cssp->constructor, &rp,
                              &expression, source_pos)) {
        /* No such constructor was found.  Abort the initialization. */
        err = TRUE;
#if 0
      } else if (rp != cssp->copy_constructor->variant.routine) {
        /* Something other than the copy constructor was returned, so be sure
           the copy constructor is accessible. */
        if (!have_access_to_symbol(cssp->copy_constructor)) {
          /* It is an error if the copy constructor is inaccessible, even
             though it is being optimized away. */
          pos_error(ec_inaccessible_copy_constructor, source_pos);
          err = TRUE;
        }  /* if */
#endif /* if 0 */
      }  /* if */
      if (!err) {
        /* Set the dynamic init entry to represent constructor
           initialization. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
        local_di.variant.constructor.routine = rp;
        local_di.variant.constructor.args = expression;
        /* Mark the constructor as referenced. */
        rp->source_corresp.referenced = TRUE;
      }  /* if */
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
    if (paren_flag) {
      add_stop_token(tok_rparen);
    } else {
      check_for_opening_brace(&brace_flag);
    }  /* if */
    scan_initializer_of_simple_object(
             /*nonconst_allowed=*/(C_dialect == C_dialect_cplusplus ||
               (vp != NULL && has_static_storage_duration(vp->storage_class))),
             /*convert_array_to_pointer=*/!is_char_array_type(vp_type),
             &vp_type, &local_di, &err);
    if (local_di.kind == (a_dynamic_init_kind)dik_expression) {
      initialization_is_dynamic = TRUE;
    }  /* if */
    /* Check for matching delimiter if lparen or lbrace appeared in front of
       the initializer. */
    if (paren_flag) {
      remove_stop_token(tok_rparen);
      (void)required_token(tok_rparen, ec_exp_rparen);
    } else {
      /* If an extra opening brace was ignored earlier, ignore the matching
         closing brace now. */
      check_for_matching_closing_brace(brace_flag);
    }  /* if */
  }  /* if */
  if (symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) {
    /* The initializer of a static data member was scanned with the original
       class reactivated.  Restore the scope to what it was before. */
    pop_scope();
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
    if (cssp != NULL && cssp->destructor != NULL) {
      local_di.destructor = rp = cssp->destructor->variant.routine;
      /* Mark the destructor referenced. */
      rp->source_corresp.referenced = TRUE;
      initialization_is_dynamic = TRUE;
    }  /* if */
    if (initialization_is_dynamic || dynamic_init_required) {
      if (dynamic_init_required && !err) {
        /* Issue a warning for a dynamic initialization in an unreachable
           block. */
        if (!curr_code_reachable()) {
          pos_warning(ec_initialization_not_reachable, source_pos);
        }  /* if */
      }  /* if */
      /* Generate a dynamic initialization entry (based on local_di) and
         attach it to the variable, and generate an stmk_init statement. */
      gen_dynamic_initialization(vp, &local_di);
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
{
  a_boolean                      def_init_performed = FALSE;
  a_variable_ptr                 var;
  a_type_ptr                     var_type, tp;
  a_class_symbol_supplement_ptr  cssp;
  a_routine_ptr                  rp;
  a_dynamic_init                 local_di, *ctor_dip;

  db_enter(3, "def_initializer");
  if (C_dialect == C_dialect_cplusplus &&
      (sym->kind == (a_symbol_kind)sk_variable ||
      sym->kind == (a_symbol_kind)sk_static_data_member)) {
    var = sym->variant.variable;
    tp = var_type = skip_typerefs(var->type);
    while (is_array_type(tp)) {
      tp = skip_typerefs(tp->variant.array.element_type);
    }  /* while */
    if (is_class_struct_union_type(tp)) {
      cssp = symbol_supplement_for_class(tp);
      if (cssp->constructor != NULL) {
        if (is_incomplete_type(var_type)) {
#if 0
          /* Don't quite know what to do on this yet.  What's done in
             end_of_scope_symbol_check is nontrivial.  And I'm not sure it
             works for multi-dimensional array. */
          pos_warning(ec_default_size_for_incomplete_array, err_pos);
#else
          internal_error(
                      "def_initializer: incomplete types not yet supported");
#endif /* if 0 */
        }  /* if */
        if (cssp->default_constructor == NULL) {
          pos_st_error(ec_no_default_constructor, err_pos,
                       tp->source_corresp.name);
        } else {
          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
          local_di.variant.constructor.routine = rp =
                                   cssp->default_constructor->variant.routine;
          local_di.variant.constructor.args = NULL;
          /* Mark the constructor referenced. */
          rp->source_corresp.referenced = TRUE;
          if (cssp->destructor != NULL) {
            local_di.destructor = rp = cssp->destructor->variant.routine;
            /* Mark the destructor referenced. */
            rp->source_corresp.referenced = TRUE;
          }  /* if */
          if (var_type != tp) {
            ctor_dip =
                    alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
            *ctor_dip = local_di;
            repeat_constructor_init(ctor_dip, &local_di,
                                    var_type->size == 0 ? 1 :
                                               var_type->size / tp->size);


          }  /* if */
          gen_dynamic_initialization(var, &local_di);
          def_init_performed = TRUE;
#if DEBUG
          if (debug_level >= 3) {
            db_variable(var);
            fputs(",\n", f_debug);
            db_initializer(var, 2);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      } else if (cssp->destructor != NULL) {
        /* Default initialization of an object that has a destructor.  We
           generate a dik_none dynamic initialization entry for this object,
           even though it is not actually initialized, so that the existence
           of the destructor can be duly recorded. */
        clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_none);
        local_di.destructor = rp = cssp->destructor->variant.routine;
        /* Mark the destructor referenced. */
        rp->source_corresp.referenced = TRUE;
        gen_dynamic_initialization(var, &local_di);
        /* Don't set def_init_performed.  A dik_none dynamic initialization
           doesn't count as initialization. */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return def_init_performed;
}  /* def_initializer */


a_constructor_init_ptr ctor_initializer(a_routine_ptr  ctor_rout)
{
  a_boolean                     err;
  a_type_ptr                    class_type, init_type, tp, array_type;
  a_symbol_ptr                  sym, class_sym, member_or_base_sym;
  a_constructor_init_ptr        cip, new_cip, prev_cip;
  a_constructor_init_ptr        cip_list, end_of_cip_list;
  a_constructor_init_ptr        virtual_list, end_of_virtual_list;
  a_constructor_init_ptr        direct_list, end_of_direct_list;
  a_base_class_ptr              bcp;
  a_routine_ptr                 rp;
  a_class_symbol_supplement_ptr cssp;
  a_dynamic_init_ptr            dip, ctor_dip;

  db_enter(3, "ctor_initializer");
  class_type = ((a_symbol_ptr)ctor_rout->source_corresp.assoc_info)->
                                                   class_of_which_a_member;
#if CHECKING
  if (class_type == NULL) internal_error("ctor_initializer: NULL class type");
#endif /* if CHECKING */
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
    if (bcp->is_virtual || bcp->direct) {
      cssp = symbol_supplement_for_class(bcp->type);
      /* If the virtual base class or direct base class has a constructor or
         a destructor, a dynamic init entry will be required.  Create the
         constructor init entry now; the dynamic init will be added later. */
      if (cssp->constructor != NULL || cssp->destructor != NULL) {
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
    }  /* if */
  }  /* for */
  /* Move on to the third list -- the list of nonstatic data members requiring
    initialization. */
  cip_list = end_of_cip_list = NULL;
  /* Loop through the symbol list for the class, not the field list, since
     the symbol list is always in declaration order, but the field list is in
     allocation order.  These need not be the same. */
  class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  for (sym = class_sym->variant.class_struct_union.extra_info->symbols;
       sym != NULL;
       sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* sym represents a field.  Determine whether constructor initialization
         is required. */
      tp = sym->variant.field->type;
      while (is_array_type(tp)) {
        tp = skip_typerefs(tp->variant.array.element_type);
      }  /* while */
      if (is_class_struct_union_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
        if (cssp->constructor != NULL || cssp->destructor != NULL) {
          cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
          cip->variant.field = sym->variant.field;
          if (cip_list == NULL) {
            cip_list = cip;
          } else {
            end_of_cip_list->next = cip;
          }  /* if */
          end_of_cip_list = cip;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (curr_token == tok_colon) {
    /* Bypass the colon. */
    (void)get_token();
    add_stop_token(tok_lbrace);
    do {
      err = FALSE;
      add_stop_token(tok_comma);
      if (!is_qualified_name_start()) {
        syntax_error(ec_exp_identifier);
      } else {
        bcp = NULL;
        dip = NULL;
        /* Scan the base class name or member name. */
        member_or_base_sym = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
        if (member_or_base_sym == NULL) {
          str_error(ec_not_a_field_or_base_class,
                    class_type->source_corresp.name);
          err = TRUE;
          init_type = error_type();
        } else if (member_or_base_sym->kind == (a_symbol_kind)sk_field) {
          /* Okay. */
          init_type = member_or_base_sym->variant.field->type;
          if (is_array_type(init_type) && !is_char_array_type(init_type)) {
            error(ec_cannot_initialize);
            init_type = error_type();
          } else {
            for (new_cip = cip_list;
                 new_cip != NULL;
                 new_cip = new_cip->next) {
              if (new_cip->variant.field ==
                  member_or_base_sym->variant.field) {
                 break;
              }  /* if */
            }  /* if */
            if (new_cip != NULL) {
              if (new_cip->initializer != NULL) {
                error(ec_already_initialized);
                err = TRUE;
              }  /* if */
            } else {
              new_cip = alloc_ctor_init((a_constructor_init_kind)cik_field);
              new_cip->variant.field = member_or_base_sym->variant.field;
              if (cip_list == NULL) {
                /* Easy case:  start a new list. */
                cip_list = end_of_cip_list = new_cip;
              } else {
                prev_cip = NULL;
                cip = cip_list;
                for (sym = class_sym->
                             variant.class_struct_union.extra_info->symbols;
                     sym != NULL;
                     sym = sym->next_in_scope) {
                  if (sym->kind == (a_symbol_kind)sk_field) {
                    if (sym == member_or_base_sym) {
                      /* Found the field.  Insert new_cip into cip_list
                         immediately following prev_cip.  If prev_cip is NULL
                         this will be at the head of the list. */
                      if (prev_cip == NULL) {
                        new_cip->next = cip_list;
                        cip_list = new_cip;
                      } else {
                        new_cip->next = prev_cip->next;
                        prev_cip->next = new_cip;
                      }  /* if */
                      break;
                    } else if (sym->variant.field == cip->variant.field) {
                      /* We didn't find the field we're trying to insert, but
                         we did find the next item on the list. */
                      if (cip == end_of_cip_list) {
                        /* Since this is the end of the list, we know the
                           new field must appear after the current entry.
                           Cut short the search. */
                        end_of_cip_list->next = new_cip;
                        end_of_cip_list = new_cip;
                        break;
                      }  /* if */
                      /* Advance through the cip list, saving the current
                         entry as a possible insertion point. */
                      prev_cip = cip;
                      cip = cip->next;
                    }  /* if */
                  }  /* if */
                }  /* for */
              }  /* if */
              /* At this point new_cip should point to the field's constructor
                 init entry to which the initializer should be attached.  It
                 has been located in or inserted into the list of such entries
                 at a spot corresponding to its declaration order. */
            }  /* if */
          }  /* if */
        } else if (is_class_symbol(sym)) {
          a_boolean  indirect_nonvirtual_base_class_found = FALSE;
          init_type = type_symbol_type(sym);
          bcp = class_type->variant.class_struct_union.extra_info->
                                                             base_classes;
          for (; bcp != NULL; bcp = bcp->next) {
            if (bcp->type == init_type) {
              if (bcp->direct || bcp->is_virtual) {
                break;
              } else {
                indirect_nonvirtual_base_class_found = TRUE;
              }  /* if */
            }  /* if */
          }  /* for */
          if (bcp == NULL) {
            /* No match found. */
            if (indirect_nonvirtual_base_class_found) {
              error(ec_indirect_nonvirtual_base_class_not_allowed);
            } else {
              error(ec_not_a_field_or_base_class);
            }  /* if */
            init_type = error_type();
          } else {
            new_cip = (bcp->is_virtual) ? virtual_list : direct_list;
            for (; new_cip != NULL; new_cip = new_cip->next) {
              if (new_cip->variant.base_class == bcp) break;
            }  /* for */
            if (new_cip == NULL) {
              str_error(ec_no_constructor, init_type->source_corresp.name);
              init_type = error_type();
            }  /* if */
          }  /* if */
        }  /* if */
        /* Advance past the identifier. */
        (void)get_token();
        if (required_token(tok_lparen, ec_exp_lparen)) {
          if (is_error_type(init_type)) {
            /* Flush tokens? Scan? */
          } else if (is_scalar_type(init_type)) {
            add_stop_token(tok_rparen);
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
            scan_initializer_of_simple_object(/*nonconst_allowed=*/TRUE,
                                              /*convert_array_to_ptr=*/TRUE,
                                              &init_type, dip, &err);
            new_cip->initializer = dip;
            remove_stop_token(tok_rparen);
            (void)required_token(tok_rparen, ec_exp_rparen);
          } else if (is_class_struct_union_type(init_type)) {
            cssp = symbol_supplement_for_class(init_type);
            if (cssp->constructor == NULL) {
              str_error(ec_no_constructor, init_type->source_corresp.name);
              err = TRUE;
            } else {
              /* This is treated like an initialization of the form
                 S x (arg [, ...]), where S is a class type name.  Depending
                 on the arguments present, a constructor will be selected and
                 returned.  The scan function returns FALSE if it finds no
                 constructor for which the arguments match. */
              an_expr_node_ptr  arg_list;
              if (!scan_ctor_arguments(cssp->constructor, &rp, &arg_list)) {
                err = TRUE;
              } else {
                /* Set the dynamic init entry to represent constructor
                   initialization. */
                dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
                dip->variant.constructor.routine = rp;
                dip->variant.constructor.args = arg_list;
                /* Mark the constructor referenced. */
                rp->source_corresp.referenced = TRUE;
                if (cssp->destructor != NULL) {
                  dip->destructor = rp = cssp->destructor->variant.routine;
                  rp->source_corresp.referenced = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            if (err) {
              a_constant_ptr  cp;
              dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
              cp = alloc_constant((a_constant_repr_kind)ck_error);
              set_error_constant(cp);
              dip->variant.constant = cp;
            }  /* if */
            new_cip->initializer = dip;
          }  /* if */
        }  /* if */
      }  /* if */
      remove_stop_token(tok_comma);
    } while (loop_token(tok_comma));
    remove_stop_token(tok_lbrace);
  }  /* if */
  /* Merge the three lists into one. */
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
  for (cip = cip_list; cip != NULL; cip = cip->next) {
    if (cip->initializer == NULL) {
      array_type = NULL;
      if (cip->kind == (a_constructor_init_kind)cik_field) {
        tp = cip->variant.field->type;
        if (is_array_type(tp)) {
          array_type = tp;
          do {
            tp = skip_typerefs(tp->variant.array.element_type);
          } while(is_array_type(tp));
        }  /* while */
      } else {
        tp = cip->variant.base_class->type;
      }  /* if */
      if (is_class_struct_union_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
        dip = NULL;
        if (cssp->default_constructor == NULL) {
          if (cssp->destructor == NULL) {
            str_error(ec_no_default_constructor,
                      cip->variant.base_class->type->source_corresp.name);
          } else {
            dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          }  /* if */
        } else {
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.routine = rp =
                                 cssp->default_constructor->variant.routine;
          rp->source_corresp.referenced = TRUE;
          dip->variant.constructor.args = NULL;
        }  /* if */
        if (cssp->destructor != NULL) {
          dip->destructor = rp = cssp->destructor->variant.routine;
          /* Mark the destructor referenced. */
          rp->source_corresp.referenced = TRUE;
        }  /* if */
        if (dip != NULL && array_type != NULL) {
          ctor_dip = dip;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          repeat_constructor_init(ctor_dip, dip,
                                    array_type->size == 0 ? 1 :
                                               array_type->size / tp->size);
        }  /* if */
        cip->initializer = dip;
      }  /* if */
    }  /* if */
  }  /* if */
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
      db_symbol(sym, "  for: ", 4);
      fputs("  initializer = ", f_debug);
      if (cip->initializer == NULL) {
        fputs("<null>", f_debug);
      } else {
        db_dynamic_initializer(cip->initializer, 6);
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return cip_list;
}  /* ctor_initializer */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
