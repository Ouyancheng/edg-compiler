/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

declarator.c -- Scanning of declarators.

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
#include "disambig.h"


static a_boolean check_pm_member_type(a_type_ptr  member_type)
/*
member_type is to be used in a pointer-to-member type.  Check its validity
and return TRUE if it's okay; otherwise issue a diagnostic and return FALSE.
*/
{
  a_boolean  err = FALSE;

  if (is_void_type(member_type) || is_reference_type(member_type)) {
    type_error(ec_bad_member_type_in_ptr_to_member, member_type);
    err = TRUE;
  }  /* if */
  return !err;
}  /* check_pm_member_type */


static a_boolean is_cfront_member_function_typedef(a_type_ptr  type_ptr,
                                                   a_type_ptr  *rout_type,
                                                   a_type_ptr  *class_type)
/*
We are checking for a type entry produced by a typedef declaration like
this:

        typedef void A::T(int);  // Nonstandard typedef

(meaning "T" names a routine type for a member function of A that takes an
int argument and returning void.  Its tie to class A is indicated by having
an implicit this-param type of const-ptr-to-A).  Cfront treats "T*" as though
it had been a ptr-to-member declaration -- e.g.,

        T* pm = &A::f(int);      // Nonstd ptr-to-member decl

and

        void (A::*pm)(int) = &A::f(int);

have the very same meaning for cfront.  Although this is not part of the
language defined in the ARM, it is supported for cfront compatibility.
*/
{
  a_type_ptr  tp;

  *class_type = NULL;
  if (any_cfront_mode() && is_function_type(type_ptr)) {
    *rout_type = skip_typerefs(type_ptr);
    if (*rout_type != type_ptr) {
      tp = (*rout_type)->variant.routine.extra_info->implicit_this_param_type;
      if (tp != NULL) *class_type = type_pointed_to(tp);
    }  /* if */
  }  /* if */
  return (*class_type != NULL);
}  /* is_cfront_member_function_typedef */


void add_to_derived_type_list(a_type_ptr new_type_ptr,
                              a_type_ptr *derived_type,
                              a_type_ptr *bottom_derived_type)
/*
Add the type entry pointed to by new_type_ptr to the list of derived-type
entries pointed to by *derived_type (and whose end is pointed to by
*bottom_derived_type).  Aside from the purely mechanical issues of
linking the entries, this routine also checks to see if the resulting
type is legal.
*/
{
  a_type_ptr              temp_type, prev_temp_type, tp;
  a_boolean               err = FALSE;
  a_type_kind             tkind;
  a_boolean               array_of_incomp_struct_or_union = FALSE;

  db_enter(3, "add_to_derived_type_list");
  /* Note that while derived types are being built up, the derived-type
     entries are connected to one another from the top down, which
     means that the bottom-most derived-type entry temporarily points
     to nothing.  Each derived type is checked as the type below it
     is attached.  This must be done carefully, because the type
     being attached may look incomplete (its size may be zero). */
  if (*bottom_derived_type == NULL) {
    /* This is the first entry on the list.  No checking can be done yet. */
    *derived_type = new_type_ptr;
    /* The type may be a qualified type in C++ (e.g., the type of a function
       declarator that is const qualified). */
    *bottom_derived_type = make_unqualified_type(new_type_ptr);
  } else {
    /* The derived-type list is non-empty, so we need to check to see if
       the bottom derived type can legally be connected to the new type
       (e.g., if it's an array, can it have elements of the indicated
       type, and if it's a function, can it have a result of the indicated
       type). */
    tkind = (*bottom_derived_type)->kind;
    if (tkind == (a_type_kind)tk_error) {
      /* The bottom derived type is an error, and nothing can be attached
         to it.  Therefore, the new type is thrown away. */
    } else {
      if (tkind == (a_type_kind)tk_array) {
        /* Array.  See if the element type is proper.  3.1.2.5: the 
           elements must have an object type.  If the element type is
           a partial array or pointer type (see comment above), let it
           by as long as it looks okay otherwise.  Note that
           is_object_type will return FALSE for a type with a size
           of zero, which is the case for the partial array and pointer
           types. */
        temp_type = skip_typerefs(new_type_ptr);
        if (is_object_type(temp_type) || is_pointer_type(temp_type)) {
          /* Okay. */
        } else if (temp_type->kind == (a_type_kind)tk_array &&
                   (temp_type->variant.array.is_variable_size_array ||
                    temp_type->
                           variant.array.variant.number_of_elements != 0)) {
          /* Okay. */
          tp = underlying_array_element_type(temp_type);
          if (tp != NULL) {
            tp = skip_typerefs(tp);
            if (is_immediate_class_type(tp) && is_incomplete_type(tp)) {
              /* This is an array of array ... of incomplete class type.  A
                 diagnostic may be issued in C mode, but only when the class is
                 the immediate element type (see below). */
              array_of_incomp_struct_or_union = TRUE;
            }  /* if */
          }  /* if */
        } else if (is_ptr_to_member_type(temp_type) &&
                   pm_member_type(temp_type) == NULL) {
          /* This is an incomplete ptr-to-member type, presumably a
             pointer to member function.  Okay. */
        } else if (is_template_param_type(temp_type)) {
          /* This is a declaration in the midst of a template declaration.
             Okay. */
        } else if (is_immediate_class_type(temp_type)) {
          check_for_uninstantiated_template_class(temp_type);
          if (is_incomplete_type(temp_type)) {
            /* As an extension in C mode, allow an array of incomplete struct
               or or union type.  In C++ this is apparently not an extension,
               since the ARM imposes no restriction.  Obviously, the element
               type has to be completed before the array is actually used.
               Add the array type to a list of array types to be fixed up when
               the class/struct/union declaration is completed. */
            array_of_incomp_struct_or_union = TRUE;
            if (strict_ansi_mode && C_dialect != C_dialect_cplusplus) {
              diagnostic(strict_ansi_error_severity,
                         ec_bad_array_element_type);
            }  /* if */
          }  /* if */
        } else {
          /* Element type is not okay.  Select a specific error message. */
          if (is_function_type(temp_type)) {
            error(ec_array_of_function);
            err = TRUE;
          } else if (is_void_type(temp_type)) {
            error(ec_array_of_void);
            err = TRUE;
          } else if (is_reference_type(temp_type)) {
	    error(ec_array_of_reference);
	    err = TRUE;
          } else if (temp_type->kind == (a_type_kind)tk_error) {
            /* Error already put out. */
            err = TRUE;
          } else {
            error(ec_bad_array_element_type);
            err = TRUE;
          }  /* if */
        }  /* if */
        if (err) new_type_ptr = error_type();
        (*bottom_derived_type)->variant.array.element_type = new_type_ptr;
      } else if (is_pointer_type(*bottom_derived_type)) {
        /* Pointer type. */
        a_type_ptr  class_type, rout_type;

        if (is_cfront_member_function_typedef(new_type_ptr, &rout_type,
                                              &class_type)) {
          /* The code contains "T*" where "T" names a member function typedef.
             It points to a routine type in which the implicit this-param
             type pointer identifies the parent class, say "S".  Then "T*" is
             equivalent to a pointer-to-member declaration, say int S::*(),
             where the return type and argument types are read from the
             routine type pointed to by "T".  What we need is not to add
             something to *bottom_derived_type (as in other cases) but rather
             to change it from a "pointer-to-???" type to a "ptr-to-member"
             type pointing the class and routine type. */
          tp = ptr_to_member_type(rout_type, class_type);
          copy_type(tp, *bottom_derived_type);
          /* Change new_type_ptr and tkind to make it seem as if this were
             an ordinary ptr-to-member declaration. */
          new_type_ptr = rout_type;
          tkind = (a_type_kind)tk_ptr_to_member;
        } else {
          if (is_reference_type(skip_typerefs(new_type_ptr))) {
            /* Pointer to reference is illegal. */
            error(ec_pointer_to_reference);
            new_type_ptr = error_type();
          }  /* if */
          (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
        }  /* if */
      } else if (is_reference_type(*bottom_derived_type)) {
        /* Reference type. */
        temp_type = skip_typerefs(new_type_ptr);
	if (is_reference_type(temp_type)) {
	  /* Reference to reference is illegal. */
          error(ec_reference_to_reference);
	  err = TRUE;
	} else if (is_void_type(temp_type)) {
	  /* Reference to void is illegal. */
          error(ec_reference_to_void);
	  err = TRUE;
        }  /* if */
	if (err) new_type_ptr = error_type();
        (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
      } else if (is_ptr_to_member_type(*bottom_derived_type)) {
        /* Pointer-to-member type. */
        if (!check_pm_member_type(new_type_ptr)) {
          new_type_ptr = error_type();
          err = TRUE;
        }  /* if */
        (*bottom_derived_type)->variant.ptr_to_member.type = new_type_ptr;
      } else {
        /* Function type. */
        check_assertion(tkind == (a_type_kind)tk_routine);
        /* 3.5.4.3, constraints: A function declarator shall not specify
           a return type that is a function type or an array type.
           Footnote to 3.5.2.3 also says it is legal to have an incomplete
           struct or union type, as long as it is complete before the 
           function is called or defined.  These are the constraints on
           a declarator; there are additional constraints (3.7.1) on function
           definitions -- see function_definition. */
        if (is_function_type(new_type_ptr)) {
          error(ec_function_returning_function);
          err = TRUE;
        } else if (is_array_type(new_type_ptr)) {
          error(ec_function_returning_array);
          err = TRUE;
        } else if (C_dialect == C_dialect_pcc) {
          /* In pcc mode, promote float functions to double functions.
             Any type qualifiers or typedef information on the new type
             are discarded. */
          promote_float_to_double(new_type_ptr);
        }  /* if */
        if (is_qualified_type(new_type_ptr) &&
            !is_reference_type(new_type_ptr)) {
          /* Type qualifiers on a function return type are meaningless. */
          /* Issue just a remark for "volatile void" -- gcc uses that to
             indicate a function (like exit()) that does not return.  Also
             just issue a remark for "const void". */
          if (is_void_type(skip_typerefs(new_type_ptr))) {
            remark(ec_useless_type_qualifiers);
          } else {
            warning(ec_useless_type_qualifiers);
          }  /* if */
        }  /* if */
        if (err) new_type_ptr = error_type();
        (*bottom_derived_type)->variant.routine.return_type = new_type_ptr;
        /* Check whether the routine needs special support for returning a
           class object by value. */
        set_routine_calling_method_flag(*bottom_derived_type, &error_position);
      }  /* if */
      temp_type = *bottom_derived_type;
      *bottom_derived_type = new_type_ptr;
      /* If the former bottom entry (temp_type) has no size, and the
         new bottom entry (new_type_ptr) has a size, loop from the bottom
         up computing sizes of types on the list.  This is necessary to
         finish off array and pointer type entries which had dependent
         types with unknown sizes up until now.  This happens because
         the string of array/pointer/function types has to be built up
         in a strange order, and sometimes a list of derived types has
         no type at the bottom (just a NULL pointer waiting to be filled
         in). */
      /* Note that the size of a pointer pointing to an incomplete type
         can be determined, so do that even if the new type is incomplete. */
      if (temp_type->size == 0 &&
          tkind != (a_type_kind)tk_routine /* For speed. */ &&
          (tkind == (a_type_kind)tk_pointer ||
           tkind == (a_type_kind)tk_ptr_to_member ||
           array_of_incomp_struct_or_union ||
           is_object_type(new_type_ptr) || is_function_type(new_type_ptr) ||
           is_error_type(new_type_ptr))) {
        while (tkind == (a_type_kind)tk_array ||
               tkind == (a_type_kind)tk_pointer ||
               tkind == (a_type_kind)tk_typeref ||
               tkind == (a_type_kind)tk_ptr_to_member) {
          /* Determine the type size.  For the "array of incomplete struct or
             union" case, this will put the type entry on a list for later
             fixup. */
          set_type_size(temp_type);
          /* set_type_size may have returned an error type. */
          if (is_error_type(temp_type)) *bottom_derived_type = temp_type;
          /* Find the derived type above this one, and see if it needs to
             have its size computed.  If so, continue looping. */
          if (temp_type == *derived_type) {
            /* We have reached the top of the derived type list; stop. */
            break;
          } else {
            /* Find the derived type entry above this one by searching down
               from the top of the list. */
            prev_temp_type = *derived_type;
            for (;;) {
              tp = underlying_type_of_derived_type(prev_temp_type);
              check_assertion_str(tp != NULL,
                                 "add_to_derived_type_list: bad type in list");
              if (tp == temp_type) break;
              prev_temp_type = tp;
            }  /* for */
            /* Found the previous type entry.  Keep looping. */
            temp_type = prev_temp_type;
            tkind = temp_type->kind;
          }  /* if */
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
}  /* add_to_derived_type_list */


static void scan_exception_specification(a_func_info_block_ptr  func_info)
/*
Scan a throw specification, which may be empty or take either of two forms:

  throw ( type-name [, type-name]... )
  throw ()

A throw specification with a list of names means "these types will be
thrown".  A throw specification the an empty list ("throw ()") means "no
exception with be thrown".  An empty throw specification means "any
exception may be thrown".

Update the func_info block with a pointer to the appropriate kind of throw
specification entry.

Diagnostics are issued on redundant types on a list, but if this is a
redeclaration of a routine, reconciliation with the previously throw
specification is handled later (see check_exception_specification).
*/
{
  an_exception_specification_ptr       esp;
  an_exception_specification_type_ptr  estp, other_estp, end_of_list = NULL;
  a_source_position                    type_pos;
  a_stop_token_array                   save_stop_token_array;

  db_enter(4, "scan_exception_specification");
  /* Update the source position for the "throw".  Even if there is no
     "throw" this is where it would appear in the source. */
  if (exceptions_enabled) func_info->throw_position = pos_curr_token;
  if (curr_token != tok_throw) {
    /* No explicit throw specification, meaning anything may be thrown. */
    goto done;
  }  /* if */
  if (!exceptions_enabled) {
    /* Exceptions are suppressed for this compilation. */
    pos_error(ec_no_exception_support, &pos_curr_token);
  } else {
    esp = alloc_exception_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
    esp->throw_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    func_info->exception_specification = esp;
  }  /* if */
  /* Bypass "throw". */
  (void)get_token();
  /* Next token should be a left paren. */
  if (curr_token == tok_lparen) {
    (void)get_token();
    if (curr_token == tok_rparen) {
      /* Case is "throw ()" -- which means "no exception will be thrown by
         this routine. */
      /* Bypass the right paren. */
      (void)get_token();
      goto done;
    }  /* if */
  } else {
    /* Syntax error -- left paren is missing.  We don't actually call
       syntax_error or required_token for this, however, since writing
       "throw int" instead of "throw (int)" might be a common mistake. */
    if (exceptions_enabled) error(ec_exp_lparen);
  }  /* if */
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_rparen);
  /* Loop through the types. */
  do {
    add_stop_token(tok_comma);
    /* Allocate the throw spec type entry. */
    estp = alloc_exception_specification_type();
    type_pos = pos_curr_token;
    if (!is_decl_start(/*expr_context=*/FALSE,
                       /*real_declarator_allowed=*/FALSE) ||
        !is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                          /*real_declarator_allowed=*/FALSE,
                          /*single_type_required=*/FALSE)) {
      /* Error. */
      if (exceptions_enabled) pos_error(ec_exp_type_specifier, &type_pos);
      /* Flush tokens to the comma or right paren. */
      flush_tokens();
      estp->type = error_type();
    } else {
      type_name(&estp->type);
    }  /* if */
    if (exceptions_enabled) {
      /* Add esp to the list. */
      if (end_of_list == NULL) {
        esp->exception_specification_type_list = estp;
      } else {
        /* Examine other entries already on the list to see if the current one
           is redundant. */
        other_estp = esp->exception_specification_type_list;
        for (; other_estp != NULL; other_estp = other_estp->next) {
          if (!other_estp->redundant &&
              identical_types(estp->type, other_estp->type)) {
            pos_remark(ec_redundant_exception_specification_type, &type_pos);
            estp->redundant = TRUE;
            break;
          }  /* if */
        }  /* for */
        end_of_list->next = estp;
      }  /* if */
      end_of_list = estp;
      if (!estp->redundant && !is_error_type(estp->type)) {
        /* Mark the type as having been used in an exception.  (Also, if it
           "contains" any classes, they are marked as requiring external
           linkage.) */
        set_used_in_exception_flag(estp->type);
      }  /* if */
    }  /* if */
    remove_stop_token(tok_comma);
    /* If the next token is not a comma, it should be a right paren -- but
       check for a few other tokens that (in error cases) should also force
       the loop to terminate. */
    if (curr_token == tok_rparen || curr_token == tok_end_of_source ||
        curr_token == tok_semicolon || curr_token == tok_lbrace) {
      break;
    }  /* if */
  } while (loop_token(tok_comma));
  /* List should be terminated by a right paren. */
  remove_stop_token(tok_rparen);
  if (curr_token == tok_rparen) {
    (void)get_token();
  } else {
    if (exceptions_enabled) (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  /* Restore the stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
done:;
  db_exit();
}  /* scan_exception_specification */


static a_boolean is_prototyped_parameter_list_start(void)
/*
Return TRUE if the current token is the start of a prototyped parameter list,
FALSE if it is the start of an old-style identifier list.  The current token
is the first token after the opening parenthesis.  Note that the case of
an empty parameter list, as in "int f();", is handled by the caller and
need not be addressed here.
*/
{
  a_boolean    prototyped;
  a_token_kind next_tok;

  /* The ambiguous cases start with an identifier. */
  if (curr_token == tok_identifier) {
    if (curr_id_is_type_name()) {
      /* The identifier is a typedef symbol. */
      if (C_dialect != C_dialect_pcc) {
        /* In ANSI and C++ mode, this must be a prototyped parameter list. */
        prototyped = TRUE;
      } else {
        /* In pcc mode, it's an old-style identifier list if the
           identifier is followed by "," or ")", and a prototyped parameter
           list otherwise.  pcc would, of course, consider anything to
           be an old-style identifier list. */
        next_tok = next_token();
        if (next_tok == tok_comma || next_tok == tok_rparen) {
          /* A case like 
               int f(t )     or
               int f(t , u)
                       ^---- must be old-style.
          */
          prototyped = FALSE;
        } else {
          /* A case like
               int f(t x)
                       ^---- must be prototyped.
          */
          prototyped = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* The identifier is not a typedef.  This suggests the start of an
         old-style identifier list. */
      if (C_dialect == C_dialect_pcc) {
        /* In pcc mode, this must be an old-style identifier list. */
        prototyped = FALSE;
      } else {
        /* To improve error recovery in ANSI mode for
             int f(tt x);
           where tt was supposed to be a typedef identifier but was not
           declared (maybe tt is misspelled), assume a prototyped parameter
           list if the next token is not a "," or ")". */
        next_tok = next_token();
        if (next_tok == tok_comma || next_tok == tok_rparen) {
          /* A case like 
               int f(tt )     or
               int f(tt , u )
                        ^---- must be old-style.
          */
          prototyped = FALSE;
        } else {
          /* A case like
               int f(tt x)
                        ^---- must be prototyped.
          */
          prototyped = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_decl_start(/*expr_context=*/FALSE,
                           /*real_declarator_allowed=*/TRUE)) {
    /* The parameter list starts with something that looks like the start of
       a declaration (but not a typedef identifier): for example, "int".
       This must be a prototyped parameter list. */
    prototyped = TRUE;
  } else {
    /* The parameter list starts with something else, meaning there's an
       error.  In ANSI mode, assume a prototyped parameter list; in pcc
       mode, assume an old-style identifier list.  The error will be
       issued below. */
    prototyped = (C_dialect != C_dialect_pcc);
  }  /* if */
  return prototyped;
}  /* is_prototyped_parameter_list_start */


static void function_declarator(a_type_ptr        *new_type_ptr,
                                a_func_info_block *func_info,
                                a_symbol_locator  *locator,
                                a_type_ptr        member_function_parent_type,
                                a_boolean         is_nonstatic_member_function,
                                a_boolean         is_constructor,
                                a_boolean         is_destructor)
/*
Scan a function declarator (3.5.4.3), or an array declarator in an
abstract declarator (3.5.5).  Allocate and return in *new_type_ptr an
appropriate function type.  The initial opening parenthesis has already
been checked and passed over (which is unusual; that's necessary
because of the syntactic strangeness of abstract declarators).  If func_info
is NULL, then the function declarator is not a top type or this is an
abstract declarator (and therefore certain forms are disallowed);
otherwise, extra information about the function declarator is returned
in *func_info.  For member functions, member_function_parent_type is a
pointer to the class (or struct or union) type of which it is a member;
otherwise it is NULL.  When it is non-NULL, is_nonstatic_member_function
will distinguish static from nonstatic member functions when the current
scope is that of a class definition.
*/
{
  a_param_type_ptr        ptp;
  a_storage_class         param_storage_class;
  a_type_ptr              param_type_ptr;
  a_decl_flag_set         dso_flags;
  a_param_type_ptr        last_param_type;
  a_param_id_ptr          last_param_id;
  a_source_sequence_entry_ptr
                          param_ssep;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
                          ss_entry_start_prev;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_symbol_locator        param_locator;
  a_type_ptr              bottom_derived_type;
  a_boolean               done;
  a_boolean               any_params;
  a_source_position       start_pos, param_type_pos;
  a_routine_type_supplement_ptr
                          extra_info;
  a_boolean               dangling_type_specifier = FALSE;
  a_boolean               defines_something;
  a_boolean               default_arg_expr_allowed = FALSE;
  a_boolean               may_be_copy_constructor = FALSE;
  a_boolean               bad_first_param_for_copy_constructor = FALSE;
  a_source_position       pos_of_first_param_type;
  a_func_info_block       local_func_info_block;

  db_enter(3, "function_declarator");
  copy_source_position(pos_curr_token, start_pos);
  set_err_pos_to_curr_token();
  add_stop_token(tok_rparen);
  /* If the caller passed in a func_info pointer, this is the declarator of
     a "top-level" function declaration.  Use the storage passed in by the
     caller.  But if func_info is NULL, use a local func info block.  This
     is mainly useful for managing param_id entries properly. */
  clear_func_info(&local_func_info_block);
  if (func_info == NULL) func_info = &local_func_info_block;
  last_param_id = NULL;
  *new_type_ptr = alloc_type((a_type_kind)tk_routine);
  extra_info = (*new_type_ptr)->variant.routine.extra_info;
  if (is_constructor) extra_info->assoc_routine_is_ctor = TRUE;
  if (is_destructor) extra_info->assoc_routine_is_dtor = TRUE;
  extra_info->param_type_list = NULL;
  if (curr_token == tok_rparen) {
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ f() is equivalent to f(void).  Leave param_type_list empty. */
      extra_info->prototyped = TRUE;
    } else {
      /* In C, f() is an old-style empty parameter list. */
      extra_info->prototyped = FALSE;
    }  /* if */
    any_params = FALSE;
  } else if (curr_token == tok_ellipsis && C_dialect == C_dialect_cplusplus) {
    if (is_destructor) {
      /* Destructors are allowed no arguments. */
      error(ec_too_many_params_for_destructor);
    } else {
      /* In C++ f(...) is legal, though it is not recommended since is not
         portable (ARM 8.3). */
      extra_info->prototyped = TRUE;
      extra_info->has_ellipsis = TRUE;
    }  /* if */
    /* Advance past the ellipsis. */
    (void)get_token();
    any_params = FALSE;
  } else {
    /* Determine whether this is an old-style list of identifiers or
       a prototyped parameter list. */
    if (!C_mode() &&
        (!allow_anachronisms || member_function_parent_type != NULL)) {
      /* If this is a C++ member function, it must be prototyped.  If
         anachronism support is not the default or was not explicitly
         requested, always parse the declaration as a prototyped param list
         -- this will produce better error messages in certain cases (even
         though it will produce poor error recovery if it actually *is* an
         old-style list). */
      extra_info->prototyped = TRUE;
    } else {
      /* Not a member function -- examine the first token. */
      extra_info->prototyped = is_prototyped_parameter_list_start();
    }  /* if */
    any_params = TRUE;
  }  /* if */
  if (extra_info->prototyped) {
    /* ANSI function prototype, as in

         int f(int a, char *b)
               or
         int f(int, char *)

    */
    if (any_params && C_dialect == C_dialect_cplusplus) {
      a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
      while (ssep->kind == (a_scope_kind)sck_class_reactivation) {
        --ssep;
      }  /* if */
      if (ssep->kind == (a_scope_kind)sck_pragma) {
        /* Disallow default arguments in function declarations within a
           pragma. */
      } else {
        /* In C++ mode a default argument may be declared with the parameter
           unless the function is a user-defined overloaded operator (except
           operator()(), as an extension) or a user-defined conversion.  Note
           that locator may be NULL (e.g., with abstract declarators). */
        /* operator new() can also take default arguments in the second and
           successive arguments -- this is implied by ARM 13.4, which
           excludes operator new() from the restrictions that are listed for
           overloaded operators in general.  We don't set the flag till after
           the first parameter has been seen, however; see below. */
        if (locator != NULL && !locator->is_conversion_name &&
            (!locator->is_operator_name ||
             locator->variant.opname == (an_opname_kind)onk_function_call)) {
          default_arg_expr_allowed = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Push a function prototype scope for the parameters. */
    (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                     *new_type_ptr, (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
    /* Remember the scope number for later use if and when a body appears. */
    func_info->scope_number = scope_stack[depth_scope_stack].number;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info != &local_func_info_block) {
      ss_entry_start_prev = init_param_source_sequence_sublist();
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (any_params) {
      last_param_type = NULL;
      do {
        add_stop_token(tok_comma);
        copy_source_position(pos_curr_token, param_type_pos);
        /* Scan a parameter-declaration. */
        (void)decl_specifiers((DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                               DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_IS_PARAMETER |
                               DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER),
                              &dso_flags, &param_storage_class,
                              &param_type_ptr);
        dangling_type_specifier = dso_flags & DSO_DANGLING_TYPE_SPECIFIER;
        defines_something = dso_flags & DSO_DEFINES_SOMETHING;
        if (last_param_type == NULL && curr_token == tok_rparen) {
          if (dso_flags & DSO_JUST_VOID) {
            /* The first and only parameter-declaration is just "void", which
               has a special meaning (no parameters).  (3.5.4.3)  */
            remove_stop_token(tok_comma);
            break;
          } else if (is_void_type(param_type_ptr) &&
                     param_type_ptr->kind == (a_type_kind)tk_typeref &&
                     !is_qualified_type(param_type_ptr) &&
                     param_storage_class == (a_storage_class)sc_unspecified) {
            /* A type name is bound to void type -- this construct is treated
               as a nonstandard way of signifying an empty param list. */
            if (strict_ansi_mode) {
              pos_warning(ec_nonstd_void_param_list, &param_type_pos);
            }  /* if */
            remove_stop_token(tok_comma);
            break;
          }  /* if */
        }  /* if */
        if (is_destructor && last_param_type == NULL) {
          /* Destructors are allowed no arguments.  Issue an error on the
             first param. */
          error(ec_too_many_params_for_destructor);
        }  /* if */
        if (defines_something && C_dialect == C_dialect_cplusplus) {
          pos_error(ec_type_definition_not_allowed, &param_type_pos);
          param_type_ptr = error_type();
        } else {
          /* Mark the type as referenced.  This is important for a
             parameter declaration like "struct s {int a;} p;" --
             the structure is referenced (because it's the type of "p")
             even through the struct is not referenced by name.  This is
             usually redundant, since the type is also marked as
             referenced in declarator.  But if declarator is not called,
             we still consider this use of the type as a reference, since
             it is incorporated into the definition of the function. */
          (skip_typerefs(param_type_ptr))->source_corresp.referenced = TRUE;
        }  /* if */
        /* Scan an optional declarator or abstract declarator.  Don't bother
           looking for a declarator when decl_specifiers has found a badly
           formed type specifier.  If an error is to be put out, that's done
           later. */
        param_ssep = NULL;
        if (!dangling_type_specifier &&
            is_abstract_or_real_declarator_start()) {
          a_decl_flag_set  do_flags;

          if (curr_token == tok_identifier &&
              !(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
            /* Missing type specifier. */
            warning(ec_missing_type_specifier);
          }  /* if */
          declarator(DI_REAL_DECLARATOR_ALLOWED |
                       DI_ABSTRACT_DECLARATOR_ALLOWED, &do_flags,
                     param_type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
                     &param_locator, &param_type_ptr, &bottom_derived_type,
                     &param_ssep, (a_func_info_block_ptr)NULL);
        } else {
          /* No declarator. */
          set_to_error_locator(param_locator);
        }  /* if */
        /* Check that the type is legal, and do required adjustments. */
        check_and_adjust_parameter_type(&param_type_ptr, &param_type_pos);
        /* Standardize the storage class: unspecified becomes auto. */
        if (param_storage_class == (a_storage_class)sc_unspecified) {
          param_storage_class = (a_storage_class)sc_auto;
        }  /* if */
        /* Put the parameter type on the type list attached to the function
           type, and the name (if present) on the id list. */
        ptp = make_param_type(param_type_ptr, &param_type_pos);
        if (last_param_type == NULL) {
          extra_info->param_type_list = ptp;
        } else {
          last_param_type->next = ptp;
        }  /* if */
        last_param_type = ptp;
        /* A parameter name is present. */
        if (is_error_locator(param_locator)) {
          func_info->any_prototype_names_omitted = TRUE;
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Make adjustments on the param source sequence entry before it is
           bound to the param_id entry. */
        if (func_info == &local_func_info_block) {
          /* If a parameter id was specified in a non-top-level function
             declarator, a source sequence entry created for it is useless. */
          if (param_ssep != NULL) {
            a_src_seq_sublist_ptr  dummy = NULL;
            remove_from_source_sequence_list(param_ssep, &dummy);
            param_ssep = NULL;
          }  /* if */
        } else if (param_ssep == NULL) {
          /* Declarator was not called or param_ssep was not created for some
             some other reason.  Still, if this turns out to be a function
             definition, it will be needed (in C++ unnamed parameters are
             allowed). */
          param_ssep = add_empty_source_sequence_entry();
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        add_to_param_id_list(&param_locator, param_type_ptr,
                             &param_type_pos, param_storage_class,
                             func_info, param_ssep, &last_param_id);
        if (curr_token == tok_assign && C_dialect == C_dialect_cplusplus) {
          /* Argument expressions are not allowed in overloaded operator
             declarations.  Issue an error, but go ahead and scan the
             expression. */
          a_scope_kind  parent_scope_kind;
          a_boolean     is_member_function;
          a_boolean     cache_default_arg;
          a_boolean     ignore_default_arg_expr;

          if (!default_arg_expr_allowed) {
            pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);
          } else if (locator->is_operator_name &&
                     locator->variant.opname ==
                                         (an_opname_kind)onk_function_call) {
            /* According to the ARM a default argument is not allowed for
               any overloaded operators, but operator()() is an exception
               in common use. Accept this silently in cfront compatibility
               mode. Otherwise produce at least a warning and possibly an
               error in strict ANSI mode.  */
            if (!any_cfront_mode()) {
              an_error_severity    severity;
              severity = strict_ansi_mode ? strict_ansi_error_severity :
                                            es_warning;
              pos_diagnostic(severity, ec_nonstd_default_arg,
                             &pos_curr_token);
            }  /* if */
          }  /* if */
          /* Advance past the equal sign. */
          (void)get_token();
          /* Check the scope immediately containing the current scope, which
             is a function prototype scope.  We may have to cache the
             default argument tokens and rescan them later. */
          cache_default_arg = FALSE;
          is_member_function = FALSE;
          ignore_default_arg_expr = !default_arg_expr_allowed;
          parent_scope_kind = scope_stack[depth_scope_stack-1].kind;
          if (default_arg_expr_allowed) {
            if (parent_scope_kind == (a_scope_kind)sck_class_struct_union) {
              /* A member function of a class (normal or template) inside
                 a class declaration. */
              cache_default_arg = TRUE;
              is_member_function = TRUE;
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_declaration) {
              /* A function template declaration.  Note that all default
                 arguments are cached. */
              cache_default_arg = TRUE;
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_instantiation) {
              /* A template instantiation -- the function declarator tokens are
                 being rescanned.  All the default arguments are scanned from
                 caches during a later fixup, so ignore the expression now. */
              ignore_default_arg_expr = TRUE;
            } else if (parent_scope_kind ==
                                  (a_scope_kind)sck_class_reactivation &&
                       scope_stack[depth_scope_stack-2].kind ==
                                   (a_scope_kind)sck_template_declaration) {
              /* An member function declaration of a template class
                 outside of the class declaration.  This is not supported. */
              pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);
              default_arg_expr_allowed = FALSE;
              ignore_default_arg_expr = TRUE;
            }  /* if */
          }  /* if */
          if (cache_default_arg &&
              curr_token != tok_comma && curr_token != tok_rparen &&
              curr_token != tok_semicolon && curr_token != tok_rbrace && 
              curr_token != tok_lbrace) {
            /* The default argument should be cached because it is either
               in a member function declaration inside a class or in
               a function template declaration.  The defaults arguments
               for member function are cached at this point and only
               scanned once the entire class has been defined.
               This is because forward references may legally appear
               in the default argument expression (C++ draft standard,
               section 8.2.6, para 3).  Function template whose arguments
               involve template parameters are cached here and scanned
               when an instance of the function template is created. */
            if (is_member_function) {
              /* Scan the default arguments for a member function. */
              prescan_member_function_default_arg_expr(ptp);
            } else {
              /* Scan the default arguments for a function template. */
              prescan_function_template_default_arg_expr(ptp);
            }  /* if */
          } else {
            /* Not a case in which the default argument should be
               cached -- or else a syntax error.  Go ahead and
               scan the expression and convert it to the required type. */
            scan_default_arg_expr(ignore_default_arg_expr ?
                                    (a_param_type_ptr)NULL : ptp);
          }  /* if */
          if (default_arg_expr_allowed) {
            ptp->has_default_arg = TRUE;
            func_info->any_default_args = TRUE;
          }  /* if */
        }  /* if */
        if (C_dialect == C_dialect_cplusplus && !default_arg_expr_allowed) {
          if (last_param_type == extra_info->param_type_list) {
            /* The first parameter on the list has just been processed. */
            if (locator != NULL && locator->is_operator_name &&
                locator->variant.opname == (an_opname_kind)onk_new) {
              /* Default argument expressions are permitted on the second and
                 subsequent parameters of an operator new declaration. */
              default_arg_expr_allowed = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Keep scanning parameter-declarations if there is a comma.
           However, also check for an ellipsis ("...") following the comma,
           which ends the prototype list in a different way. */
        /* Note that a comma preceding the ellipsis is optional in C++. */
        if (dangling_type_specifier ||
            C_mode() ? curr_token == tok_ellipsis :
                       (is_error_locator(param_locator) &&
                        identifier_is_template_id())) {
          /* A dangling type specifier is detected by decl_specifiers
             when a comma is omitted between the end of a type specifier
             and the start of the next.  This is pretty unlikely, but the
             mechanism was added for class declarations, where it is more
             useful. */
          /* Another unlikely case is the identifier-but-not-declarator-id
             case -- which occurs when a template-id appears where a
             declarator was expected. */
          pos_error(ec_exp_comma, &pos_curr_token);
          done = FALSE;
        } else {
          done = !loop_token(tok_comma);
        }  /* if */
        if (curr_token == tok_ellipsis) {
          /* The parameter list ends with an ellipsis.  Set the ellipsis
             flag on the parameter type list, and exit the loop. */
          (void)get_token();
          done = TRUE;
          extra_info->has_ellipsis = TRUE;
        }  /* if */
        if (is_constructor) {
          /* In case this is an ill-formed copy constructor, we need to do
             some additional error checking.  We're looking for cases like
               A::A(A);                // case 1
               A::A(A, T=x);           // case 2
               A::A(A&, A=y);          // case 3
               A::A(A&, T=x, A=y);     // case 4
             It's not actually possible to know whether a constructor is a
             (legal or illegal) copy constructor without looking past the
             first parameter.  That's part of what makes this check a little
             complicated.  (See ARM 12.1.) */
          if (extra_info->param_type_list->next == NULL) {
            /* This is the first item on the list. */
            if (identical_types(member_function_parent_type,
                                skip_typerefs(param_type_ptr))) {
              /* Type of the first parameter is identical to the type of the
                 parent class. */
              if (done) {
                /* This is like case 1 above. */
                pos_ty_error(ec_bad_constructor_param, &param_type_pos,
                             member_function_parent_type);
                ptp->type = error_type();
              } else {
                /* Depending on whether the next parameter has a default
                   argument (see case 2 above), this may be an (illegal)
                   copy constructor. */
                may_be_copy_constructor = TRUE;
                /* Record information to assure that an error will be issued
                   if this does turn out to be an error case. */
                bad_first_param_for_copy_constructor = TRUE;
                pos_of_first_param_type = param_type_pos;
              }  /* if */
            } else if (!done) {
              /* We're looking at the first parameter.  See if this may be a
                 copy constructor.  This will help find cases 3 and 4. */
              if (is_reference_type(param_type_ptr) &&
                  identical_types(member_function_parent_type,
                                  skip_typerefs(type_pointed_to(
                                                          param_type_ptr)))) {
                /* Depending on whether the next parameter has a default
                   argument, this may be a copy constructor. */
                may_be_copy_constructor = TRUE;
              }  /* if */
            }  /* if */
          } else if (may_be_copy_constructor) {
            /* We are beyond the first parameter on the list of what may
               be a copy constructor. */
            if (ptp == extra_info->param_type_list->next) {
              /* This is the second parameter in the list. */
              if (!ptp->has_default_arg) {
                /* The second parameter lacks a default argument so this must
                   not be a copy constructor. */
                may_be_copy_constructor = FALSE;
              } else if (bad_first_param_for_copy_constructor) {
                /* The second parameter does have a default argument and the
                   first argument is not a ref.  This is like case 2.  Issue
                   the error using the source position of the first param
                   type. */
                pos_ty_error(ec_bad_constructor_param,
                             &pos_of_first_param_type,
                             member_function_parent_type);
                extra_info->param_type_list->type = error_type();
                may_be_copy_constructor = FALSE;
              }  /* if */
            }  /* if */
            if (may_be_copy_constructor) {
              if (identical_types(member_function_parent_type,
                                  skip_typerefs(ptp->type))) {
                /* Type of this parameter is identical to the type of the
                   parent class (see cases 3 and 4 above).  Since this is a
                   copy constructor, an error is in order. */
                pos_ty_error(ec_bad_constructor_param, &param_type_pos,
                             member_function_parent_type);
                ptp->type = error_type();
                may_be_copy_constructor = FALSE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        remove_stop_token(tok_comma);
      } while (!done);
    }  /* if */
    /* Save the list of symbols for the prototype scope (usually NULL, but
       can have symbols for named types declared within the prototype). */
    if (func_info != &local_func_info_block) {
      /* Note that a pointer to the current entry of scope_stack is not saved
         from earlier in this routine because scope_stack might have been
         reallocated in the interim. */
      func_info->prototype_scope_symbols =
                                        scope_stack[depth_scope_stack].symbols;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Record the start and end of the prototype scope. */
      terminate_param_source_sequence_sublist(func_info,
                                              ss_entry_start_prev);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    /* Process pragmas associated with the closing paren before the current
       scope is popped. */
    process_curr_token_pragmas();
    /* Pop the function prototype scope. */
    pop_scope();
  } else if (any_params) {
    /* Old-style list of identifiers. */
    if (func_info == &local_func_info_block) {
      /* This type of parameter list is not valid in abstract declarators
         and non-top-level function declarators. */
      error(ec_param_id_list_needs_function_def);
    } else if (C_dialect == C_dialect_cplusplus) {
      /* This type of parameter list is an anachronism in C++. */
      diagnostic(anachronism_error_severity, ec_old_style_parameter_list);
    }  /* if */
    do {
      add_stop_token(tok_comma);
      /* Scan the list of identifiers. */
      if (curr_token != tok_identifier) {
        /* Error, expected identifier. */
        (void)required_token(tok_identifier, ec_exp_identifier);
      } else {
        /* See if the identifier is also a typedef name.  Such a name is
           not allowed (3.7.1, constraints).  In pcc mode, however, this
           is allowed. */
        if (C_dialect != C_dialect_pcc && curr_id_is_type_name()) {
          error(C_mode() ? ec_typedef_cannot_be_param_name :
                           ec_type_cannot_be_param_name);
          /* Enter the parameter anyway, for best error recovery. */
        }  /* if */
        /* Add the identifier to the parameter id list. */
        add_to_param_id_list(&locator_for_curr_id, (a_type_ptr)NULL,
                             (a_source_position*)NULL,
                             (a_storage_class)sc_unspecified, func_info,
                             (a_source_sequence_entry_ptr)NULL,
                             &last_param_id);
        /* Advance past the identifier. */
        (void)get_token();
      }  /* if */
      remove_stop_token(tok_comma);
      /* Keep looping on a comma, stop otherwise. */
    } while (loop_token(tok_comma));
  }  /* if */
  /* Check for closing right parenthesis.  We temporarily clear the stop
     token array values for tok_comma and tok_assign, in order to flush past
     either to the right paren. */
  { int t1 = (int)stop_token_array[(int)tok_comma],
        t2 = (int)stop_token_array[(int)tok_assign];
    stop_token_array[(int)tok_comma] = 0;
    stop_token_array[(int)tok_assign] = 0;
    (void)required_token(tok_rparen, ec_exp_rparen);
    stop_token_array[(int)tok_comma] = t1;
    stop_token_array[(int)tok_assign] = t2;
  }
  remove_stop_token(tok_rparen);
  if (C_dialect == C_dialect_cplusplus) {
    a_type_ptr            this_param_type = NULL;
    a_type_qualification  qualifier;

    /* Create a pointer to the implicit this parameter.  This can be done
       for nonstatic function declarations within a class definition or
       for member function declarations outside a class definition when
       a function qualifier is present.  If there is a function qualifier,
       it is applied to the type pointed to by the this param type. */
    if (is_type_qualifier() && extra_info->prototyped) {
      /* In C++ the type of certain member functions may be qualified.  Scan
         for a const or volatile qualifier. */
      a_storage_class    dummy_storage_class;
      a_type_ptr         dummy_type_ptr;
      a_source_position  qualifier_pos;
      a_boolean          qualifier_err = FALSE;

      copy_source_position(pos_curr_token, qualifier_pos);
      (void)decl_specifiers(DSI_COLLECT_TYPE_QUALIFIERS, &dso_flags,
                            &dummy_storage_class, &dummy_type_ptr);
      /* If this is not a member function or it is but it is a static member
         function declared within a class definition, a qualifier on the
         function is illegal (ARM 8.2.5)..  However, qualifiers on a pointer
         to member function are permitted. */
      if (locator != NULL && locator->is_operator_name &&
          (locator->variant.opname == (an_opname_kind)onk_new ||
           locator->variant.opname == (an_opname_kind)onk_delete)) {
        /* Operator new and delete can never be qualified. */
        qualifier_err = TRUE;
      } else if (member_function_parent_type == NULL ||
                 (!is_nonstatic_member_function &&
                  scope_stack[decl_scope_level].kind ==
                                 (a_scope_kind)sck_class_struct_union)) {
        /* It is illegal to specify "const" or "volatile" on any function
           other than a nonstatic member function (ARM 8.2.5).  We just
           issue a warning since it is harmless. */
        qualifier_err = TRUE;
      } else if (is_constructor || is_destructor) {
        /* A qualifier appearing on a constructor or destructor is not
           allowed (ARM 9.3.1). */
        if (cfront_2_1_mode) {
          /* Cfront 2.1 issues no diagnostic for a qualifier on a constructor
             or destructor. */
          pos_warning(ec_function_qualifier_not_allowed, &qualifier_pos);
        } else {
          qualifier_err = TRUE;
        }  /* if */
        this_param_type = member_function_parent_type;
      } else {
        qualifier = TQ_NONE;
        if (dso_flags & DSO_CONST_QUALIFIED) qualifier |= TQ_CONST;
        if (dso_flags & DSO_VOLATILE_QUALIFIED) qualifier |= TQ_VOLATILE;
        this_param_type = make_qualified_type(member_function_parent_type,
                                              qualifier);
      }  /* if */
      if (qualifier_err) {
        pos_error(ec_function_qualifier_not_allowed, &qualifier_pos);
      }  /* if */
    } else if (is_nonstatic_member_function) {
      /* This is a nonstatic member function declared within the definition
         of the class indicated. */
      this_param_type = member_function_parent_type;
    }  /* if */
    if (this_param_type != NULL) {
      /* The implicit this param type will be either "const pointer to
         class-type" or, if there was a const qualifier on the function,
         "const pointer to const class-type". */
      this_param_type = make_pointer_type(this_param_type);
      qualifier = TQ_CONST;
      this_param_type = make_qualified_type(this_param_type, qualifier);
      extra_info->implicit_this_param_type = this_param_type;
    }  /* if */
#if 0
    /* Should a diagnostic be issued if a throw specification appears other
       than on a top-level declaration? */
    if (curr_token == tok_throw && func_info == &local_func_info_block) {
      /* Error?  Warning? */
    }  /* if */
#endif /* if 0 */
    scan_exception_specification(func_info);
  }  /* if */
  done_with_func_info(local_func_info_block);
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* function_declarator */


void array_declarator(a_type_ptr *new_type_ptr,
                      a_boolean  nonconstant_dimension_allowed)
/*
Scan an array declarator (3.5.4.2), or an array declarator in an
abstract declarator (3.5.5).  Allocate and return in *new_type_ptr an
appropriate array type.  The initial opening bracket is the current
token.  In C++ the dimension may sometimes be a nonconstant
expression (e.g., with a new type name); that case is indicated by
nonconstant_dimension_allowed.
*/
{
  a_targ_size_t           num_of_elements;
  a_constant              constant;
  a_boolean               err = FALSE;
  a_source_position       start_pos;
  an_expr_node_ptr        dim_expr = NULL;
  a_memory_region_number  region_to_switch_back_to;

  db_enter(3, "array_declarator");
  copy_source_position(pos_curr_token, start_pos);
  /* Pass over the initial left bracket. */
  (void)get_token();
  add_stop_token(tok_rbracket);
  if (curr_token == tok_rbracket) {
    /* Empty brackets, indicating an incomplete array type. */
    num_of_elements = 0;
  } else {
    /* Scan the array size. */
    if (nonconstant_dimension_allowed) {
      a_boolean  is_constant;

      scan_new_array_dimension_expression(&is_constant, &dim_expr, &constant);
      check_assertion(is_constant == (dim_expr == NULL));
    } else {
      scan_integral_constant_expression(&constant);
    }  /* if */
    if (dim_expr == NULL) {
      switch (constant.kind) {
        case ck_integer:
          /* Array size must be greater than zero. */
          if (sign_of_integer_constant(&constant) <= 0) {
            error(ec_array_size_must_be_positive);
            err = TRUE;
          } else {
            num_of_elements =
                        unsigned_value_of_integer_constant(&constant, &err);
            if (err) error(ec_array_size_too_large);
          }  /* if */
          break;
        case ck_template_param:
          switch_to_file_scope_region(&region_to_switch_back_to);
          dim_expr = alloc_expr_node((an_expr_node_kind)enk_constant);
          dim_expr->variant.constant =
                   alloc_constant((a_constant_repr_kind)ck_template_param);
          copy_constant(&constant, dim_expr->variant.constant);
          switch_back_to_original_region(region_to_switch_back_to);
          break;
        case ck_error:
          err = TRUE;
          break;
#if CHECKING
        default:
          internal_error("array declarator: bad constant kind");
#endif /* if CHECKING */
      }  /* switch */
    }  /* if */
  }  /* if */
  if (err) {
    *new_type_ptr = error_type();
  } else {
    *new_type_ptr = alloc_type((a_type_kind)tk_array);
    /* Store the array size. */
    if (dim_expr != NULL) {
      /* Expression case. */
      (*new_type_ptr)->variant.array.is_variable_size_array = TRUE;
      (*new_type_ptr)->variant.array.variant.element_count_expr = dim_expr;
    } else {
      (*new_type_ptr)->variant.array.variant.number_of_elements =
                                                           num_of_elements;
    }  /* if */
    /* The size of the array (in bytes) is updated in 
       add_to_derived_type_list. */
  }  /* if */
  /* Check for closing right bracket. */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* array_declarator */


a_type_ptr pointer_declarator(a_type_ptr  specifiers_type,
                              a_type_ptr  *bottom_pointer_derived_type,
                              a_boolean   reference_allowed)
/*
Scan the pointer component of a declarator.  Syntax for C++ (ARM 8.0):

8.0	ptr-operator:
		* cv-qualifier-list
			           opt
		& cv-qualifier-list
				   opt
		complete-class-name :: * cv-qualifier-list
							  opt

where a cv-qualifier-list consists of "const" or "volatile" or both.  Only
the first form is accepted in C.  Note also that even in C++ the second form
is not allowed in a new-declarator (ARM 5.3.3), so the reference_allowed
parameter controls the restrictions imposed by the context.
*/
{
  a_type_ptr     complete_type = specifiers_type;
  a_boolean      err;
  a_type_ptr     class_type, rout_type;

  db_enter(3, "pointer_declarator");
  for (;;) {
    /* Add a pointer type to the top of the existing type.  Note that this
       works out right.  For example, if one has

       int * const * volatile i;

       the proper type for i is "volatile pointer to const pointer to int".
       In the loop, the type will be built up from "int" to
       "const pointer to int" to "volatile pointer to const pointer to int"
       on successive iterations. */
    err = FALSE;
    if (curr_token == tok_star ||
        (reference_allowed && curr_token == tok_ampersand)) {
      set_err_pos_to_curr_token();
      if (complete_type != NULL) {
        /* Normal case -- the specifiers type is given, and the pointer or
           reference type can be attached directly to it.  (Or, this is a
           pointer to a pointer type or a reference to a pointer type). */
        a_type_ptr  temp_type = skip_typerefs(complete_type);

        if (curr_token == tok_star) {
          if (is_cfront_member_function_typedef(complete_type, &rout_type,
                                                &class_type)) {
            complete_type = ptr_to_member_type(rout_type, class_type);
          } else {
            if (is_reference_type(temp_type)) {
              /* Type "pointer to reference to anything" is illegal. */
              error(ec_pointer_to_reference);
              err = TRUE;
            }  /* if */
            complete_type = make_pointer_type(err ? error_type() :
                                                    complete_type);
          }  /* if */
        } else {
          if (is_reference_type(temp_type)) {
            /* Type "reference to reference" is illegal. */
            error(ec_reference_to_reference);
            err = TRUE;
          } else if (is_void_type(temp_type)) {
            /* Type "reference to void" is illegal. */
            error(ec_reference_to_void);
            err = TRUE;
          }  /* if */
          complete_type = err ? error_type() :
                                make_reference_type(complete_type);
        }  /* if */
      } else {
        /* The specifiers type is not known, so the bottom-most pointer type
           modifier is built but not attached to anything.  It will be
           connected later.  Note that the size is not set (set_type_size is
           not called) at this point; that's done in add_to_derived_type_list,
           once the type pointed to is known, in case pointers to different
           types have different sizes. */
        a_type_ptr new_type_ptr = alloc_type((a_type_kind)tk_pointer);
        new_type_ptr->variant.pointer.type = complete_type;
        if (curr_token == tok_ampersand) {
          new_type_ptr->variant.pointer.is_reference = TRUE;
        }  /* if */
        complete_type = new_type_ptr;
      }  /* if */
    /* Check for C++ a pointer-to-member declarator. */
    } else if (C_dialect == C_dialect_cplusplus &&
               is_ptr_to_member_declarator_start()) {
      /* Qualified name followed by "*". */
      /* Upon return from is_ptr_to_member_declarator_start the current
         token is tok_ptr_to_member. */
      class_type = locator_for_curr_id.qualifier_class_type;
      if (class_type == NULL) {
        /* It looks like a pointer-to-member declarator, but there was some
           error in the class qualifier (e.g., nonclassname::*).  We don't
           want a pointer-to-member type pointing at anything but a
           valid class type, so make it an error type instead. */
        complete_type = error_type();
        err = TRUE;
      } else {
        /* A valid pointer-to-member declarator. */
        if (complete_type != NULL && !check_pm_member_type(complete_type)) {
          complete_type = error_type();
        }  /* if */
        complete_type = ptr_to_member_type(complete_type, class_type);
      }  /* if */
    } else {
      /* Not a pointer, reference, or pointer-to-member declarator. */
      break;
    }  /* if */
    if (err || *bottom_pointer_derived_type == NULL) {
      *bottom_pointer_derived_type = complete_type;
    }  /* if */
    /* Take a type qualifier list (const, volatile, or both) if one appears. */
    (void)get_token();
    if (is_type_qualifier()) {
      a_decl_flag_set       dso_flags;
      a_storage_class       dummy_storage_class;
      a_type_ptr            dummy_type_ptr;
      a_type_qualification  qualifier = TQ_NONE;

      set_err_pos_to_curr_token();
      (void)decl_specifiers(DSI_COLLECT_TYPE_QUALIFIERS, &dso_flags,
                            &dummy_storage_class, &dummy_type_ptr);
      if (is_reference_type(complete_type)) {
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_qualified_reference_type);
      } else {
        if (dso_flags & DSO_CONST_QUALIFIED) qualifier |= TQ_CONST;
        if (dso_flags & DSO_VOLATILE_QUALIFIED) qualifier |= TQ_VOLATILE;
        complete_type = make_qualified_type(complete_type, qualifier);
      }  /* if */
    }  /* if */
  }  /* while */

#if DEBUG
  if (debug_level >= 4) {
    if (complete_type != specifiers_type) {
      fputs("pointer/reference type: ", f_debug);
      db_type(complete_type);
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return complete_type;
}  /* pointer_declarator */


void declarator(a_decl_flag_set   input_flags,
                a_decl_flag_set   *output_flags,
                a_type_ptr        specifiers_type,
                a_type_ptr        member_parent_type,
                a_symbol_locator  *locator,
                a_type_ptr        *p_complete_type,
                a_type_ptr        *p_bottom_derived_type,
                a_source_sequence_entry_ptr
                                  *declarator_ssep,
                a_func_info_block *func_info)
/*
Scan a declarator (3.5.4) or an abstract declarator (3.5.5), depending
on the values of real_declarator_allowed and abstract_declarator_allowed
(real, abstract, or either can be allowed).  specifiers_type points
to the type scanned in a preceding specifiers list, or is NULL when this
routine calls itself to scan a nested declarator.  Return in *locator
the symbol table locator and source position for the identifier in the
declarator (if only an abstract declarator is allowed, locator is not
used; if both real and abstract declarators are allowed, and an abstract
declarator is scanned, *locator is set to a null declarator).  Return
the final type (declarator derived types, if any, combined with the
specifiers type) in *p_complete_type.  If specifiers_type was NULL on
entry, *p_complete_type points to just the declarator derived type list
(with nothing attached to the bottom), or is NULL if there is no derived
type list.  *p_bottom_derived_type is set to point to the bottom type in
the declarator derived type list, or NULL if there is no derived type
list.  If the top type in the declarator derived type list is a
function, *func_info is filled with extra information about the
parameter list, for use if a function body follows.  For declarators
that may turn out to be member functions, member_parent_type is
a pointer to the class (or struct or union) type of which it is a member;
otherwise it is NULL.  The syntax is:

3.5.4  declarator:
		pointer    direct-declarator
		       opt

       direct-declarator:
		identifier
		( declarator )
		direct-declarator [ constant-expression    ]
						       opt
                direct-declarator ( parameter-type-list )
		direct-declarator ( identifier-list    )
						   opt
       pointer:
		* type-qualifier-list
				     opt
		* type-qualifier-list    pointer
				     opt

3.5.5  abstract-declarator:
		pointer
		pointer    direct-abstract-declarator
                       opt

       direct-abstract-declarator:
		( abstract-declarator )
		direct-abstract-declarator    [ constant-expression    ]
					  opt                      opt
		direct-abstract-declarator    ( parameter-type-list    )
					  opt                      opt

*/
{
  a_type_ptr      complete_type;
  a_type_ptr      derived_type;
  a_type_ptr      bottom_derived_type;
  a_type_ptr      new_type_ptr;
  a_type_ptr      bottom_pointer_derived_type;
  a_source_position
                  declarator_pos;
  a_boolean       real_declarator_allowed;
  a_boolean       abstract_declarator_allowed;
  a_boolean       is_name_start;
  a_boolean       is_constructor = FALSE, is_destructor = FALSE;
  a_boolean       is_nonstatic_member_function = FALSE;
  a_boolean       cfront_member_function_typedef = FALSE;
  a_boolean       nonconstant_dimension_allowed;
  a_boolean       parenthesized_initializer_allowed;
  a_boolean       is_friend_decl = FALSE;
  a_boolean       class_scope_deactivation_required = FALSE;
#if CHECKING
  a_boolean       any_syntax_error = FALSE;
#endif /* if CHECKING */

  db_enter(3, "declarator");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, declarator_pos);
  *output_flags = DO_NO_OUTPUT_FLAGS;
  real_declarator_allowed = input_flags & DI_REAL_DECLARATOR_ALLOWED;
  abstract_declarator_allowed = input_flags & DI_ABSTRACT_DECLARATOR_ALLOWED;
  is_constructor = (input_flags & DI_IS_CONSTRUCTOR) != 0;
  parenthesized_initializer_allowed =
                       (input_flags & DI_PARENTHESIZED_INITIALIZER_ALLOWED);
  nonconstant_dimension_allowed =
                            (input_flags & DI_DIMENSION_EXPRESSION_ALLOWED);
  is_friend_decl = (input_flags & DI_IS_FRIEND_DECL);
  if (!real_declarator_allowed) {
    func_info = NULL;
    locator = NULL;
  } else if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
    /* Avoid confusing a typedef declaration of a function type with a
       function declaration. */
    func_info = NULL;
  }  /* if */
  /* Set the locator to indicate there is no identifier. */
  if (locator != NULL) set_to_error_locator(*locator);
  /* Look for any initial "*" list indicating pointer types. */
  bottom_pointer_derived_type = NULL;
  complete_type = pointer_declarator(specifiers_type,
                                     &bottom_pointer_derived_type,
                                     /*reference_allowed=*/
                                       C_dialect == C_dialect_cplusplus);
  derived_type = NULL;
  bottom_derived_type = NULL;
  /* The next thing is an identifier, or a parenthesis that begins a
     nested declarator.  For the abstract declarator case, the
     identifier is omitted. */
  set_err_pos_to_curr_token();
  if (curr_token == tok_lparen) {
    /* Left parenthesis indicating nested declarator.  For the abstract
       declarator case, this might be a parenthesis indicating a function.
       We can differentiate the two cases because in the case of a nested
       declarator the next token must be a "*", "(", or "[", whereas in the
       function case it is ")", "...", or a declaration specifier. */
    a_decl_flag_set  local_do_flags;

    (void)get_token();
    if (abstract_declarator_allowed) {
      if (curr_token == tok_rparen ||
          is_decl_start(/*expr_context=*/FALSE,
                        /*real_declarator_allowed=*/TRUE) ||
          (C_dialect == C_dialect_cplusplus && curr_token == tok_ellipsis)) {
        /* Function declarator rather than a nested declarator. */
        goto function_lparen;
      }  /* if */
    }  /* if */
    add_stop_token(tok_rparen);
    /* Get the nested declarator, removing the flag allowing parenthesized
       initializers from the input_flags bit vector.  (The other flags are
       passed on in the recursive call.) */
    declarator(~(~input_flags | DI_PARENTHESIZED_INITIALIZER_ALLOWED),
               &local_do_flags, /*specifiers_type=*/(a_type_ptr)NULL,
               member_parent_type, locator, &derived_type,
               &bottom_derived_type, declarator_ssep, func_info);
    if (local_do_flags & DO_REAL_DECLARATOR_SCANNED) {
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
    } else {
      parenthesized_initializer_allowed = FALSE;
    }  /* if */
    if (local_do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
      *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
      cfront_member_function_typedef = TRUE;
      /* Force function_declarator to add an implicit-this-param pointer
         to the routine type. */
      check_assertion(locator->qualifier_class_type != NULL);
      member_parent_type = locator->qualifier_class_type;
    }  /* if */
    if (local_do_flags & DO_CLASS_SCOPE_DEACTIVATION_REQUIRED) {
      /* A class scope was reactivated to scan a static data member or a
         member function.  It will have to be deactivated when the scanning
         of the top-level declarator is complete. */
      class_scope_deactivation_required = TRUE;
    }  /* if */
    /* A nonconstant dimension, if allowed at all, is allowed only on the
       topmost type (an interpretation of the language specification in ARM
       5.3.3).  Set the flag to FALSE for subsequent processing. */
    nonconstant_dimension_allowed = FALSE;
    /* Check for and get the closing parenthesis. */
#if CHECKING
    if (curr_token != tok_rparen) any_syntax_error = TRUE;
#endif /* if CHECKING */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    /* An identifier is expected next, but is omitted in the 
       abstract declarator. */
    is_name_start = (is_qualified_name_start() || curr_token == tok_operator ||
                     curr_token == tok_compl);
    if (!real_declarator_allowed ||
        (abstract_declarator_allowed && !is_name_start)) {
      /* Identifier is omitted in an abstract declarator.  Be sure it is not a
         tk_unknown type. */
      check_assertion(specifiers_type == NULL ||
                      !is_unknown_type(specifiers_type));
      parenthesized_initializer_allowed = FALSE;
    } else {
      /* Real (non-abstract) declarator. */
      declarator_pos = pos_curr_token;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "adding empty ss entry for declarator \"%s\":\n",
                locator_for_curr_id.symbol_header->identifier);
      }  /* if */
#endif /* DEBUG */
      *declarator_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      /* Process the identifier.  This is done if we are at the beginning of
         a qualified name.  A special test is done to exclude a destructor
         name that is not part of a qualified name -- this case is handled
         separately below.  A destructor that is not part of a qualified name
         (according to the locator) can appear to be an identifier under
         some circumstances.  For example, inside the definition of
         class A, the destructor "A::~A" will be coalesced by
         is_generalized_identifier_start.  The qualifier will then be
         discarded by simplify_curr_class_qualified_name resulting in an
         unqualified destructor that has already been coalesced. */
      if (curr_token == tok_identifier &&
          (!locator_for_curr_id.is_destructor_name ||
           locator_for_curr_id.is_qualified_name)) {
        a_boolean        	  err;
        an_identifier_options_set options;
        /* Access checking is suppressed for declarators.  When the
           declarator contains a class qualifier it is defining something
           already declared in the class definition.  This should not be
           considered an access violation. */
        options = GID_DISALLOW_GLOBAL_QUALIFIER | GID_SUPPRESS_ACCESS_ERRORS;
        if (!(input_flags & DI_QUALIFIED_NAME_ALLOWED)) {
          options |= GID_DISALLOW_QUALIFIED_NAME;
        }  /* if */
        if (input_flags & DI_IS_TEMPLATE_DECLARATION) {
          options |= GID_CLASS_MUST_BE_PROTOTYPE_INSTANTIATION;
        }  /* if */
        if (any_cfront_mode()) {
          /* Provide support for an exploitable cfront bug. */
          if (locator_for_curr_id.is_qualified_name &&
              locator_for_curr_id.qualifier_class_type != NULL &&
              input_flags & DI_IS_TYPEDEF_DECLARATION) {
            /* We have a typedef declaration involving what appears to be a
               qualified name, but cfront interprets it as a kind of member
               routine type, e.g.,
                   typedef void A::t(int);
                                   ^---------We're here now.
               The type "t" is construed as a routine type taking an int
               argument and returning void and having an implicit this-param
               type of const-ptr-to-A.  Note that this syntax and
               interpretation are not supported in the ARM.  We allow it
               under cfront compatibility mode only. */
            check_assertion(locator_for_curr_id.specific_symbol == NULL);
            /* Force function_declarator to add an implicit-this-param pointer
               to the routine type. */
            member_parent_type = locator_for_curr_id.qualifier_class_type;
            *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
            cfront_member_function_typedef = TRUE;
            /* Clear the is_qualified_name flag in the locator, but keep the
               qualifer_class_type around, in case this is a recursive
               declarator call and the function_declarator is called at
               another level. */
            locator_for_curr_id.is_qualified_name = FALSE;
            /* Note that the diagnostic on this nonstandard construct is
               issued by the caller. */
          }  /* if */
        }  /* if */
        if (C_dialect == C_dialect_cplusplus && !is_friend_decl) {
          /* If this declaration appears in the immediate context of a class
             definition and the current token is an identifier representing
             the name of the current class, see if this is a qualified name
             and if so change it into a simple name (e.g., A::x becomes x,
             its equivalent in A's scope).
             This needs to be done after the check for the cfront member
	     typedef processing that is done above. */
          (void)simplify_curr_class_qualified_name();
        }  /* if */
        /* The declarator may be a qualified name or a normal name. */
        if (coalesce_and_lookup_qualified_name(options, ilm_normal, &err)) {
          /* See if the name is a qualified name, like "A::x" or "::j". */
          if (locator_for_curr_id.is_qualified_name) {
            member_parent_type = locator_for_curr_id.qualifier_class_type;
            if (member_parent_type != NULL) {
              a_symbol_ptr  sym = locator_for_curr_id.specific_symbol;
              a_boolean     reactivate_scope = FALSE;

              /* See if the name is the name of a member function. */
              if (sym->kind == (a_symbol_kind)sk_member_function ||
                  sym->kind == (a_symbol_kind)sk_overloaded_function ||
                  sym->kind == (a_symbol_kind)sk_function_template) {
                /* It is a member function.  Its parameters should be scanned
                   with the original class reactivated. */
                reactivate_scope = TRUE;
                parenthesized_initializer_allowed = FALSE;
                if (is_constructor_symbol(sym)) {
                  is_constructor = TRUE;
                  if (!is_unknown_type(complete_type)) {
                    error(ec_return_type_not_allowed);
                    complete_type = unknown_type();
                  }  /* if */
                } else if (is_destructor_symbol(sym)) {
                  is_destructor = TRUE;
                  if (!is_unknown_type(complete_type)) {
                    error(ec_return_type_not_allowed);
                    complete_type = unknown_type();
                  }  /* if */
                }  /* if */
              } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
                /* The dimensions of static data members (if any) are scanned
                   with the original class reactivated. */
                reactivate_scope = TRUE;
              }  /* if */
              if (reactivate_scope) {
                /* Reactivate the scope of the parent class.  It will be
                   deactivated once the entire declarator has been scanned. */
                push_class_reactivation_scope(member_parent_type);
                class_scope_deactivation_required = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        if (err) {
          /* An error occurred while scanning the identifier -- use an error
             locator. */
          set_to_named_error_locator(locator_for_curr_id);
        }  /* if */
        /* Save information on the identifier to be declared. */
        *locator = locator_for_curr_id;
        (void)get_token();
      } else {
        /* The call to simplify_curr_class_qualified_name is placed here so
           that it will be done at this point for all cases other than the
           normal identifier case handled above. */
        if (!is_friend_decl) (void)simplify_curr_class_qualified_name();
        if ((curr_token == tok_identifier &&
             locator_for_curr_id.is_destructor_name) ||
            get_destructor_name()) {
          /* A destructor name, like "~A".  It must have the same name as
             the class currently being defined, it must be followed by a
             left paren, and the specifiers must include no type. */
          if (is_error_locator(locator_for_curr_id)) {
            /* There is some error in the destructor name. */
          } else {
            a_scope_stack_entry_ptr ssep = &scope_stack[decl_scope_level];
            a_symbol_ptr            class_sym;

            if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
              /* Not inside a class; destructor is not allowed. */
              error(ec_bad_destructor_decl);
            } else {
              class_sym = (a_symbol_ptr)ssep->assoc_type->
                                                     source_corresp.assoc_info;
              if (!destructor_name_matches_class_name(class_sym)) {
                /* The name on the destructor is not the name of the class. */
                error(ec_bad_destructor_decl);
              } else {
                if (!is_unknown_type(complete_type)) {
                  error(ec_return_type_not_allowed);
                  complete_type = unknown_type();
                } else if (!(input_flags & DI_DESTRUCTOR_SPECIFIERS)) {
                  /* The specifiers, including possibly the type specifier,
                     are not consistent with a destructor declaration (e.g., a
                     destructor cannot be specified "static" or "void"). */
                  error(ec_bad_destructor_decl);
                } else {
                  /* Valid destructor declaration. */
                  member_parent_type = ssep->il_scope->variant.assoc_type;
                }  /* if */
                is_destructor = TRUE;
                parenthesized_initializer_allowed = FALSE;
                *locator = locator_for_curr_id;
              }  /* if */
            }  /* if */
          }  /* if */
          /* Advance past the destructor. */
          (void)get_token();
          if (!is_destructor) {
            /* Invalid destructor name. */
            set_to_error_locator(*locator);
            /* Avoid spurious errors later. */
            if (is_unknown_type(complete_type)) complete_type = void_type();
          } else if (curr_token != tok_lparen) {
            /* A valid destructor name is not followed by a left
               parenthesis. */
            error(ec_exp_lparen);
            if (curr_token != tok_rparen) {
              error(ec_exp_rparen);
            } else {
              (void)get_token();
            }  /* if */
            is_destructor = FALSE;
            complete_type = error_type();
            set_to_error_locator(*locator);
          }  /* if */
          parenthesized_initializer_allowed = FALSE;
        } else {
          add_stop_token(tok_lparen);
          add_stop_token(tok_lbracket);
          copy_source_position(pos_curr_token, locator->source_position);
          syntax_error(ec_exp_identifier);
          remove_stop_token(tok_lparen);
          remove_stop_token(tok_lbracket);
          parenthesized_initializer_allowed = FALSE;
        }  /* if */
      }  /* if */
      if (!(input_flags & DI_OPERATOR_NAME_ALLOWED)) {
        if (locator->is_operator_name || locator->is_conversion_name) {
          pos_error(ec_operator_name_not_allowed, &locator->source_position);
          set_to_error_locator(*locator);
          complete_type = error_type();
        }  /* if */
      }  /* if */
      if (locator->is_operator_name) {
        /* Enforce some restrictions on the declarations of overloaded
           operator functions. */
        if (member_parent_type != NULL) {
          if (locator->specific_symbol != NULL) {
            /* This must be a redeclaration. */
          } else if (!(input_flags & DI_NONSTATIC_MEMBER) &&
                     locator->variant.opname != (an_opname_kind)onk_new &&
                     locator->variant.opname != (an_opname_kind)onk_delete) {
            pos_error(ec_static_member_operator_not_allowed,
                      &locator->source_position);
            set_to_error_locator(*locator);
          }  /* if */
        } else {
          char *s = NULL;
          switch (locator->variant.opname) {
            case onk_assign:         s = "=";       break;
            case onk_function_call:  s = "()";      break;
            case onk_subscript:      s = "[]";      break;
            case onk_arrow:          s = "->";      break;
            default:;  /* No error. */
          }  /* switch */
          if (s != NULL) {
            if (locator->variant.opname == (an_opname_kind)onk_assign &&
                cfront_2_1_mode) {
              pos_st_warning(ec_nonmember_operator_not_allowed,
                             &locator->source_position, s);
            } else {
              pos_st_error(ec_nonmember_operator_not_allowed,
                           &locator->source_position, s);
              set_to_error_locator(*locator);
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (locator->is_conversion_name) {
        if (!is_unknown_type(complete_type)) {
          pos_error(ec_return_type_on_conversion_function, &declarator_pos);
        }  /* if */
        complete_type = locator->variant.conversion_result_type;
        /* A conversion function must be a nonstatic member function. */
        if (member_parent_type == NULL ||
            (locator->specific_symbol == NULL &&
             !(input_flags & DI_NONSTATIC_MEMBER))) {
          pos_error(ec_bad_conversion_function_decl,
                    &locator->source_position);
          set_to_error_locator(*locator);
          /* Avoid error recovery problems later. */
          locator->is_conversion_name = TRUE;
        }  /* if */
      } else if (is_constructor) {
        /* Return type should be "unknown" at this point.  Change it to
           the constructed type (a front end convention that deviates from
           what is explicitly in the source for a constructor declaration. */
        check_assertion(is_unknown_type(complete_type));
        complete_type = make_reference_type(member_parent_type);
      } else if (is_destructor) {
        /* Return type should be "unknown" at this point.  Change it to void
           (again, a front end convention). */
        check_assertion(is_unknown_type(complete_type));
        complete_type = void_type();
      }  /* if */
    }  /* if */
  }  /* if */
  /* The declarator can end at this point, or an array or function
     specification (or a series of them) can follow.  The additional
     specifications, if they appear, are parsed in their order of 
     appearance.  However, the resulting type has to be built from
     the top down, which makes the logic a bit convoluted.  For example,

     int a[3][4];

     is a 3-element array, each of whose elements is a four-element
     int array.  The type "array[3] of" is built on the first iteration;
     on the second iteration the type "array[4] of" is built, and
     the new type is added to the end of the existing list, giving
     "array[3] of array[4] of".  Then the loop ends, and the array
     type is attached to the original type "int" (from complete_type).

     If a nested declarator was scanned above, there may already be
     a derived type list, and the new entries are added to its end.
  */
  while (curr_token == tok_lparen || curr_token == tok_lbracket) {
    if (curr_token == tok_lparen) {
      /* Appears to be a function declarator.  But be sure it's not the
         start of a parenthesized initializer (C++ only). */
      /* Advance past the left parenthesis. */
      (void)get_token();
      if (parenthesized_initializer_allowed &&
          curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* The context and other information we have about the declarator do
           not preclude a parenthesized initializer, nor does the token that
           follows the left paren.  Be sure the declarator type (which has not
           yet been assembled) is one for which a parenthesized initializer is
           legal and see if the token(s) following the left paren are not
           declarations. */
        a_type_ptr  tp;

        tp = derived_type != NULL ? derived_type : complete_type;
        check_assertion(tp != NULL);
        if ((is_arithmetic_type(tp) || is_ptr_or_ref_type(tp) ||
             is_class_struct_union_type(tp) || is_ptr_to_member_type(tp)) &&
            !is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                              /*real_declarator_allowed=*/TRUE,
                              /*single_type_required=*/FALSE)) {
          a_boolean  is_function_decl = FALSE;
          /* This appears to be a parenthesized initializer.  However, it
             might also be a function definition with an old-style parameter
             list.  If, starting with the current token, we have a comma-
             separated list of identifiers followed by a right paren followed
             by a left brace or the start of a declaration, then this can
             only be a function definition.  Otherwise assume it to be a
             parenthesized initializer. */
          if (curr_token == tok_identifier) {
            a_token_cache       cache;

            clear_token_cache(&cache, /*reusable=*/FALSE);
            cache_curr_token(&cache);
            /* Advance past all comma-identifier pairs till what should be
               the closing paren. */
            while (get_token() == tok_comma) {
              cache_curr_token(&cache);
              if (get_token() == tok_identifier) {
                cache_curr_token(&cache);
              } else {
                break;
              }  /* if */
            }  /* while */
            /* If there is a closing paren, see if the next token is the
               start of a declaration or else a left brace introducing the
               function body. */
            if (curr_token == tok_rparen) {
              cache_curr_token(&cache);
              if (get_token() == tok_lbrace ||
                  is_decl_start(/*expr_context=*/FALSE,
                                /*real_declarator_allowed=*/TRUE)) {
                /* This looks exactly like a function declaration with an
                   old style parameter list. */
                is_function_decl = TRUE;
              }  /* if */
            }  /* if */
            rescan_cached_tokens(&cache);
          }  /* if */
          if (!is_function_decl) {
            *output_flags |= DO_PARENTHESIZED_INITIALIZER;
            /* Function_declarator should not be called, so exit the loop. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
function_lparen:
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      if (C_dialect == C_dialect_cplusplus) {
        if (derived_type != NULL) {
          /* If the function is pointed to by a pointer-to-member type, we need
             to pass the class-of-which-a-member to function_declarator. */
          a_type_ptr tp = bottom_derived_type;
          if (tp != NULL && is_ptr_to_member_type(tp)) {
            /* Declaration of a pointer to member function. */
            member_parent_type = pm_class_type(tp);
            is_nonstatic_member_function = TRUE;
          } else {
            member_parent_type = NULL;
          }  /* if */
          func_info = NULL;
          is_constructor = is_destructor = FALSE;
        } else if (cfront_member_function_typedef) {
          check_assertion(func_info == NULL);
          is_nonstatic_member_function = TRUE;
          is_constructor = is_destructor = FALSE;
        } else if (func_info == NULL) {
          is_constructor = is_destructor = FALSE;
          is_nonstatic_member_function = FALSE;
          member_parent_type = NULL;
        } else if (is_constructor || is_destructor) {
          is_nonstatic_member_function = TRUE;
        } else {
          if (input_flags & DI_NONSTATIC_MEMBER) {
            if (locator->is_operator_name &&
                (locator->variant.opname == (an_opname_kind)onk_new ||
                 locator->variant.opname == (an_opname_kind)onk_delete)) {
              /* operator new and operator delete are always nonstatic, even
                 if "static" was not specified in the declaration. */
            } else {
              is_nonstatic_member_function = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Normal C case.  If the derived type is nonnull this is not the
           top-most type, so we don't want to fetch the extra function info. */
        if (derived_type != NULL) func_info = NULL;
      }  /* if */
      function_declarator(&new_type_ptr, func_info, locator,
                          member_parent_type, is_nonstatic_member_function,
                          is_constructor, is_destructor);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (func_info != NULL) {
        /* Record the source sequence entry in func_info even if there was
           an error in the declarator (i.e., even if the locator is an error
           locator).  This could mean an empty source sequence entry is in
           the list when there's an error, but that should be okay. */
        func_info->declarator_ssep = *declarator_ssep;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Left bracket, indicating array declarator. */
      array_declarator(&new_type_ptr, nonconstant_dimension_allowed);
      if (nonconstant_dimension_allowed) {
        /* In C++ a array declarator that appears in an operator new()
           expression may have a nonconstant expression in the first
           dimension (ARM 5.3.3).  Subsequent dimension must be constants. */
        nonconstant_dimension_allowed = FALSE;
      }  /* if */
    }  /* if */
    /* Add the new type to the bottom of the existing derived type list.
       Note that this involves error checking. */
    add_to_derived_type_list(new_type_ptr,
                             &derived_type, &bottom_derived_type);
  }  /* while */
  /* Set the referenced flag on the specifiers type if this is the top-level
     scan of the declarator (i.e., if specifiers_type is non-NULL) -- but
     do this only if a real declarator was scanned.  This enables us to
     properly handle tags whose definitions appear in the declaration of
     another entity -- e.g., "struct S {int a;} x", where there is no reference
     to S by tag name (and so the symbol's referenced flag is not set) but
     there IS a use of the IL entity (and therefore the reference flag in the
     type entry is set).  (Compare this to "struct S {int a;}; struct S x",
     where the reference to (use of) the type appears with the reference
     to the tag name.)  As written, this sets the referenced flag for all
     types, not just tags, which is harmless. */
  if (specifiers_type != NULL) {
    /* Use m_is_error_type instead of is_error_type for efficiency. */
    if (!m_is_error_type(complete_type)) {
#if CHECKING
      check_assertion((*output_flags & DO_REAL_DECLARATOR_SCANNED) ||
                      is_ptr_or_ref_type(complete_type) ||
                      derived_type != NULL || any_syntax_error ||
                      is_ptr_to_member_type(complete_type));
#endif /* if CHECKING */
      (skip_typerefs(specifiers_type))->source_corresp.referenced = TRUE;
    }  /* if */
  }  /* if */
  /* Use the position of the identifier as the position of this declarator
     for error purposes.  If this was an abstract declarator, declarator_pos
     has been set to the beginning of the declarator.  The error position
     is set a bit early here so that any errors below from combining the
     two lists will have the right position. */
  copy_source_position(declarator_pos, error_position);
  /* Combine the derived type list with the earlier complete type
     (pointer derived type list plus specifiers_list), making
     the full type.  Note that this involves error checking. */
  if (derived_type != NULL) {
    /* Use m_is_error_type instead of is_error_type for efficiency. */
    if (m_is_error_type(derived_type)) {
      bottom_derived_type = error_type();
    } else if (complete_type != NULL) {
      if (is_immediate_error_type(bottom_derived_type)) {
        /* The bottom derived type is an error, so we cannot attach the
           complete type to the bottom.  Also clear the pointer to the
           bottom-most pointer type, since it's in the complete_type
           and is being thrown away. */
        bottom_pointer_derived_type = NULL;
      } else {
        /* Normal case -- combine derived_type and complete_type. */
        add_to_derived_type_list(complete_type,
                                 &derived_type, &bottom_derived_type);
        if (is_immediate_error_type(bottom_derived_type)) {
          /* There must have been an error -- e.g., array-of-invalid-type. */
          bottom_pointer_derived_type = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
    complete_type = derived_type;
  }  /* if */
  /* If there were pointer types scanned at the beginning of this routine,
     the bottom-most derived type is the bottom-most pointer type. */
  if (bottom_pointer_derived_type != NULL && !is_error_type(complete_type)) {
    bottom_derived_type = bottom_pointer_derived_type;
  }  /* if */
  if (specifiers_type != NULL) {
    /* This is a top-level call to declarator. */
    if (locator != NULL &&
        (locator->is_operator_name || locator->is_conversion_name) &&
        !is_function_type(complete_type) && !is_error_type(complete_type)) {
      /* A declaration of an operator must have a function type. */
      pos_error(ec_function_type_required, &locator->source_position);
      set_to_error_locator(*locator);
      complete_type = bottom_derived_type = error_type();
    }  /* if */
  }  /* if */
  if (class_scope_deactivation_required) {
    /* A class scope was reactivated when a qualified name was seen. */
    if (specifiers_type != NULL) {
      /* This is a top-level call to declarator, so the class scope can now
         be deactivated. */
      pop_class_reactivation_scope();
    } else {
      /* Pass the information up to the caller. */
      *output_flags |= DO_CLASS_SCOPE_DEACTIVATION_REQUIRED;
    }  /* if */
  }  /* if */
  *p_complete_type = complete_type;
  *p_bottom_derived_type = bottom_derived_type;
#if DEBUG
  if (debug_level >= 3) {
    fputs("complete_type: ", f_debug);
    if (complete_type == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_type(complete_type);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* declarator */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
