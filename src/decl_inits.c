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


static a_boolean has_constructor(a_type_ptr  tp)
{
  a_boolean      found = FALSE;
  a_routine_ptr  rp;

  if (C_dialect == C_dialect_cplusplus && is_class_struct_union_type(tp)) {
    skip_typerefs(tp);
    for (rp = tp->variant.class_struct_union.extra_info->assoc_scope->routines;
         rp != NULL;
         rp = rp->next) {
      if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
        found = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return found;
}  /* has_constructor */


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
  a_boolean           is_constant;
  a_dynamic_init_ptr  dip;

  db_enter(4, "get_initializer");
  err = FALSE;
  local_type = skip_typerefs(*type);
  if (is_aggregate_or_union_type(local_type) ||
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
    check_for_opening_brace(&brace_flag);
    if (C_dialect == C_dialect_cplusplus) {
      scan_initializer_expression(
              /*convert_array_to_pointer=*/TRUE,
              &is_constant, &expression, &constant, &err);
    } else {
      scan_constant_initializer_expression(/*convert_array_to_pointer=*/TRUE,
                                           &constant, &err);
      is_constant = TRUE;
    }  /* if */
    if (!err) {
      if (is_constant) {
        /* Check the type of the initial value against the type of the object
           being initialized. */
        check_constant_initializer(&constant, &local_type, &err);
        if (!err) {
          /* Allocate the constant that is the value of the initializer. */
          init_con = alloc_unshared_constant(&constant);
        }  /* if */
      } else {
        /* Non-constant.  Check the type by assignment rules and cast the
           node if necessary. */
        node_prepare_assignment(&expression, local_type,
                                ec_bad_initializer_type, &err);
        if (!err) {
          init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
          init_con->variant.dynamic_init = dip =
                       alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
          dip->variant.expression = expression;
          if (*di_list == NULL) {
            *di_list = dip;
          } else {
            (*end_of_di_list)->next = dip;
          }  /* if */
          *end_of_di_list = dip;
        }  /* if */
      }  /* if */
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
  a_scope_stack_entry_ptr ssep;

  /* Build the dynamic initialization entry. */
  new_dip = alloc_dynamic_init(dip->kind);
  switch (dip->kind) {
    case dik_constant:
      new_dip->variant.constant = dip->variant.constant;
      break;
    case dik_expression:
      new_dip->variant.expression = dip->variant.expression;
      break;
    case dik_constructor:
      new_dip->variant.constructor.routine = dip->variant.constructor.routine;
      new_dip->variant.constructor.args = dip->variant.constructor.args;
      new_dip->variant.constructor.corresp_destructor =
                                  dip->variant.constructor.corresp_destructor;
      break;
    case dik_aggregate:
      new_dip->variant.aggregate.aggr_const =
                                  dip->variant.aggregate.aggr_const;
      new_dip->variant.aggregate.dynamic_init =
                                  dip->variant.aggregate.dynamic_init;
      break;
#if CHECKING
    default:
      internal_error("gen_dynamic_initialization: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* Attach the dynamic initialization entry to the scope list. */
  ssep = &scope_stack[decl_scope_level];
  if (ssep->il_scope->dynamic_inits == NULL) {
    ssep->il_scope->dynamic_inits = new_dip;
  } else {
    ssep->last_dynamic_init->next = new_dip;
  }  /* if */
  ssep->last_dynamic_init = new_dip;
  /* Make the variable point at the dynamic initialization. */
  vp->init_kind = (an_init_kind)initk_dynamic;
  vp->initializer.dynamic = new_dip;
  /* Set the referenced flag for the variable, because there is a
     reference now -- the dynamic initialization. */
  vp->source_corresp.referenced = TRUE;
  if (ssep->kind == (a_scope_kind)sck_file) {
    /* A dynamic file-scope initialization (possible only in C++) has
       no associated stmk_init statement. */
    new_dip->variable = vp;
  } else {
    /* Build the initialization statement. */
    init_stmt = add_statement((a_statement_kind)stmk_init);
    init_stmt->seq_number = vp->source_corresp.decl_position.seq;
    init_stmt->variant.dynamic_init = new_dip;
  }  /* if */
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
  a_boolean             err = FALSE;
  a_boolean             put_init_in_variable;
  a_variable_ptr        vp = NULL;
  a_type_ptr            vp_type = NULL;
  a_boolean             brace_flag;
  a_boolean             is_constant;
  an_expr_node_ptr      expression;
  a_constant            constant;
  a_constant_ptr        cp;
  an_init_kind          init_kind;
  a_dynamic_init        local_di, *di_list, *end_of_di_list;
  a_boolean             dynamic_init_required;
  a_boolean             initialization_is_dynamic;
  a_routine_ptr         rp;
  an_expr_node_ptr      arg_list;

  db_enter(3, "initializer");

  if (is_parameter) {
    /* Parameter declarations cannot contain an initializer. */
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
  if (has_constructor(vp_type)) {
    if (curr_token == tok_lbrace) {
      error(ec_exp_primary_expr);
      flush_tokens();
    } else {
#if CHECKING
    internal_error("initializer: constructors not yet implemented");
#endif /* CHECKING */
#if 0
      scan_constructor_args(vp_type, paren_flag, &arg_list);
      rp = select_constructor(vp_type, arg_list);
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constructor);
      local_di.variant.constructor.routine = rp;
      local_di.variant.constructor.args = arg_list;
      local_di.variant.constructor.corresp_destructor =
                                           select_destructor(vp_type);
      initialization_is_dynamic = TRUE;
#endif /* if 0 */
    }  /* if */
  } else if (is_aggregate_or_union_type(vp_type)) {
    di_list = end_of_di_list = NULL;
    cp = get_initializer(&vp_type, &di_list, &end_of_di_list,
                         /*top_level=*/TRUE);
    if (cp->kind == (a_constant_repr_kind)ck_aggregate) {
      clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_aggregate);
      local_di.variant.aggregate.aggr_const = cp;
      local_di.variant.aggregate.dynamic_init = di_list;
      initialization_is_dynamic = (di_list != NULL);
      if (put_init_in_variable) {
        /* Copy the type back into the variable.  It might have been changed
           if vp is an incomplete array. */
        if (vp != NULL && vp_type != vp->type) {
          put_type_back_into_variable(vp, symbol_ptr, source_pos, linkage,
                                      vp_type);
        }  /* if */
      }  /* if */
    } else {
#if CHECKING
      if (cp->kind == (a_constant_repr_kind)ck_error) {
        internal_error("initializer: unexpected constant kind");
      }  /* if */
#endif /* CHECKING */
      err = TRUE;
    }  /* if */
  } else {
    check_for_opening_brace(&brace_flag);
    if (C_dialect == C_dialect_cplusplus ||
        (vp != NULL && has_static_storage_duration(vp->storage_class))) {
      /* Scan a potentially non-constant initializer expression.  The result
         of the scan is a constant if the expression is constant, and an
         expression node if not. */
      scan_initializer_expression(
              /*convert_array_to_pointer=*/!is_char_array_type(vp_type),
              &is_constant, &expression, &constant, &err);
    } else {
      scan_constant_initializer_expression(
              /*convert_array_to_pointer=*/!is_char_array_type(vp_type),
              &constant, &err);
      is_constant = TRUE;
    }  /* if */
    if (!err) {
      /* See if the scanned expression was constant or not. */
      if (is_constant) {
        /* Constant.  Check the constant type to see if it is legal,
           change the constant type if necessary. */
        check_constant_initializer(&constant, &vp_type, &err);
        if (!err) {
          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_constant);
          local_di.variant.constant = cp = alloc_unshared_constant(&constant);
        }  /* if */
      } else {
        /* Non-constant.  Check the type by assignment rules and cast the
           node if necessary. */
        node_prepare_assignment(&expression, vp_type, ec_bad_initializer_type,
                                &err);
        if (!err) {
          clear_dynamic_init(&local_di, (a_dynamic_init_kind)dik_expression);
          local_di.variant.expression = expression;
          initialization_is_dynamic = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* If an extra opening brace was ignored earlier, ignore the matching
       closing brace now. */
    check_for_matching_closing_brace(brace_flag);
  }  /* if */
  if (!err && put_init_in_variable) {
    if (C_dialect == C_dialect_cplusplus) {
      dynamic_init_required = (decl_scope_level != DEPTH_OF_FILE_SCOPE);
    } else {
      dynamic_init_required = !has_static_storage_duration(vp->storage_class);
    }  /* if */
    if (initialization_is_dynamic || dynamic_init_required) {
      if (dynamic_init_required) {
        /* Issue a warning for a dynamic initialization in an unreachable
           block. */
        if (!curr_code_reachable()) {
          warning(ec_initialization_not_reachable);
        }  /* if */
      }  /* if */
      /* Generate a dynamic initialization entry (based on local_di) and
         attach it to the variable, and generate an stmk_init statement if
         appropriate. */
      gen_dynamic_initialization(vp, &local_di);
    } else {
      vp->init_kind = (an_init_kind)initk_static;
      vp->initializer.constant = cp;
    }  /* if */
  }  /* if */
  db_exit();
}  /* initializer */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
