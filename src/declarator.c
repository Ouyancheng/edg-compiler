/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
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
#include "statements.h"

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


a_boolean is_cfront_member_function_typedef(a_type_ptr   type_ptr,
                                            a_type_ptr   *rout_type,
                                            a_type_ptr   *class_type,
                                            a_symbol_ptr *sym)
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

Return TRUE if this is a member function typedef.  Also return a pointer to
the function type and the class type if this is the case -- and a pointer to
the type symbol for the typedef, for use in diagnostics.
*/
{
  a_type_ptr  tp;
  a_boolean   is_member_function_typedef = FALSE;

  *class_type = NULL;
  *rout_type = NULL;
  *sym = NULL;
  if (type_ptr->kind == (a_type_kind)tk_typeref &&
      is_function_type(type_ptr)) {
    *rout_type = skip_typerefs(type_ptr);
    tp = (*rout_type)->variant.routine.extra_info->this_class;
    if (tp != NULL) {
      is_member_function_typedef = TRUE;
      *class_type = tp;
      *sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
    }  /* if */
  }  /* if */
  return is_member_function_typedef;
}  /* is_cfront_member_function_typedef */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_type_qualifier_set collect_type_qualifiers(
                                       a_decl_pos_block_ptr  decl_pos_block)
/*
Call decl_specifiers to scan one or more declarator qualifiers, and return
a bit vector describing what was found.  At least one qualifier must be
present (i.e., the caller must have already checked that the current
token is a qualifier).
*/
{
  a_decl_flag_set         dsi_flags, dso_flags;
  a_storage_class         dummy_storage_class;
  a_type_ptr              dummy_type_ptr;
  a_decl_modifiers_block  dummy_decl_modifiers;
  a_type_qualifier_set    qualifiers;
  a_decl_pos_block        local_decl_pos_block;

  clear_decl_pos_block(&local_decl_pos_block);
  dsi_flags = DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS;
  if (microsoft_mode) { dsi_flags |= DSI_INLINE_ALLOWED; }
  (void)decl_specifiers(dsi_flags, &dso_flags,
                        &dummy_storage_class, &dummy_type_ptr,
                        &qualifiers, &dummy_decl_modifiers,
                        &local_decl_pos_block);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    check_assertion(local_decl_pos_block.specifiers_range.end.seq != 0);
    decl_pos_block->declarator_range.end =
                       local_decl_pos_block.specifiers_range.end;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return qualifiers;
}  /* collect_type_qualifiers */


a_boolean restrict_qualifier_is_allowed(a_type_ptr         type,
                                        a_source_position  *error_pos)
/*
Return TRUE if type may be qualified by the "restrict" qualifier.  It may be
applied to pointer and reference types (but not pointer-to-function-type),
pointer-to-member types (but not pointers to member functions), and (in
parameter declarations only) array types.  If a restrict qualifier is not
allowed, issue a diagnostic and return FALSE.
*/
{
  a_type_ptr     tp;
  an_error_code  error_code = ec_no_error;
  
  if (!is_error_type(type)) {
    if (is_ptr_or_ref_type(type)) {
      /* Pointer types and references may be restrict qualified unless they
         point to function types. */
      tp = type_pointed_to(type);
      if (tp != NULL && is_function_type(tp)) {
        error_code = ec_restrict_pointer_to_function;
      }  /* if */
    } else if (is_ptr_to_member_type(type)) {
      /* Pointer-to-member types may be restrict qualified unless they point
         to function types. */
      tp = pm_member_type(type);
      if (tp != NULL && is_function_type(tp)) {
        error_code = ec_restrict_pointer_to_function;
      }  /* if */
    } else {
      /* Anything else is disallowed. */
      error_code = ec_restrict_not_allowed;
    }  /* if */
    if (error_code != ec_no_error) {
      pos_error(error_code, error_pos);
    }  /* if */
  }  /* if */
  return (error_code == ec_no_error);
}  /* restrict_qualifier_is_allowed */


static void check_for_restrict_qualifier_on_derived_type(
                                              a_type_ptr  new_type_ptr,
                                              a_type_ptr  *derived_type,
                                              a_type_ptr  *bottom_derived_type)
/*
*derived_type is the top of a derived type that is being constructed, and
*bottom_derived_type is the bottom, which is about to be updated to link to
new_type_ptr.  Check for the presence of improperly applied restrict
qualifier: if *bottom_derived_type is some kind of pointer or reference type
and is about to be updated to point to a function type, then the restrict
qualifier, if there is one, is invalid.  Issue a diagnostic and rewrite the
derived type to remove the restrict qualifier.
*/
{
  a_type_ptr            tp, prev_tp, new_tp;
  a_type_qualifier_set  qualifiers;

  if (is_function_type(new_type_ptr)) {
    check_assertion(is_ptr_or_ref_type(*bottom_derived_type) ||
                    is_ptr_to_member_type(*bottom_derived_type));
    /* We are about to form a derived type that is pointer-to-function-type,
       reference-to-function-type, or ptr-to-member-function.  Such pointer
       types, unlike other pointer types, may not be restrict qualified.
       Go through the derived type list looking for restrict qualifier that
       applies to the pointer type. */
    for (tp = *derived_type, prev_tp = NULL;
         tp != *bottom_derived_type;
         prev_tp = tp, tp = underlying_type_of_derived_type(tp)) {
      if (tp->kind == (a_type_kind)tk_typeref) {
        /* Check for qualifiers on a typeref. */
        qualifiers = get_top_level_type_qualifiers(tp);
        tp = skip_typerefs(tp);
        if (tp == *bottom_derived_type) {
          if (qualifiers & TQ_RESTRICT) {
            /* A restrict qualifier was found and it applies to the pointer
               type that is going to be set to point to the function type.
               Issue a diagnostic and remove the restrict qualifier. */
            error(ec_restrict_pointer_to_function);
            if (qualifiers == TQ_RESTRICT) {
              new_tp = *bottom_derived_type;
            } else {
              new_tp = make_qualified_type(*bottom_derived_type,
                                           (qualifiers & ~TQ_RESTRICT));
              *bottom_derived_type = skip_typerefs(new_tp);
            }  /* if */
            if (prev_tp == NULL) {
              *derived_type = new_tp;
            } else {
              switch (prev_tp->kind) {
                case tk_pointer:  /* Includes C++ reference too. */
                  prev_tp->variant.pointer.type = new_tp;
                  break;
                case tk_ptr_to_member:
                  prev_tp->variant.ptr_to_member.type = new_tp;
                  break;
                case tk_array:
                  prev_tp->variant.array.element_type = new_tp;
                  break;
                case tk_routine:
                  prev_tp->variant.routine.return_type = new_tp;
                  break;
#if CHECKING
                default:
                  internal_error("check_for_restrict...: bad type kind");
#endif /* CHECKING */
              }  /* switch */
            }  /* if */
            *bottom_derived_type = skip_typerefs(new_tp);
          }  /* if */
          break;
        }  /* if */
      }  /* if */
      /* Continue to the next type in the derived type sequence. */
    }  /* for */
  }  /* if */  
}  /* check_for_restrict_qualifier_on_derived_type */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_type_ptr form_declared_type(a_type_ptr             type_ptr,
                              a_func_info_block_ptr  func_info)
/*
If type_ptr is a function type, return a copy of type_ptr that incorporates
the parameter types as actually declared in the source program; that is,
preserve the parameter type as it was before any adjustment was done (e.g.,
array-to-pointer decay).
*/
{
  a_type_ptr        declared_type;
  a_param_id_ptr    param_id;
  a_param_type_ptr  ptp, param_type_list;
  a_boolean         fixup_needed;

  db_enter(4, "form_declared_type");
  if (type_ptr->kind == (a_type_kind)tk_typeref) {
    /* Leave the declared type the same as the routine type. */
    declared_type = type_ptr;
  } else {
    /* Make a copy of the type.  Note that default arg expressions, if any,
       will be copied later. */
    declared_type =
               copy_routine_type_with_param_types(type_ptr,
                                                  /*copy_default_args=*/FALSE);
    fixup_needed = FALSE;
    param_id = func_info->param_id_list;
    param_type_list = skip_typerefs(declared_type)->
                            variant.routine.extra_info->param_type_list;
    if (param_id != NULL && param_type_list != NULL) {
      /* There is no need to create a new routine type entry if none of the
         parameter types underwent adjustment. */
      ptp = param_type_list;
      for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
        check_assertion(param_id->declared_type != NULL);
        if (!identical_types(ptp->type, param_id->declared_type)) {
          /* Some adjustment must have been done. */
          fixup_needed = TRUE;
          break;
        }  /* if */
        check_assertion((param_id->next == NULL) == (ptp->next == NULL));
      }  /* for */
      if (fixup_needed) {
        /* It's necessary to create a new type. */
        ptp = param_type_list;
        param_id = func_info->param_id_list;
        for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
          a_type_ptr  tp = param_id->declared_type;

          check_assertion(tp != NULL);
          if (is_error_type(ptp->type) || is_error_type(tp)) {
            /* Do nothing. */
          } else {
            if (!C_mode() && is_or_contains_template_param(tp)) {
              if (is_function_type(tp) && !is_function_type(ptp->type)) {
                /* Undo the change of a function type to pointer-to-function
                   type. */
                ptp->type = type_pointed_to(ptp->type);
                check_assertion(is_function_type(ptp->type));
              } else if (is_array_type(tp) && !is_array_type(ptp->type)) {
                /* Undo array-to-pointer decay. */
                a_type_ptr new_type = alloc_type((a_type_kind)tk_array);
                new_type->variant.array.element_type =
                                             type_pointed_to(ptp->type);
                ptp->type = new_type;
              } else if (is_qualified_type(tp)) {
                ptp->type = make_identically_qualified_type(ptp->type, tp);
              }  /* if */
            } else {
              ptp->type = tp;
            }  /* if */
            ptp->qualifiers = TQ_NONE;
          }  /* if */
          check_assertion((param_id->next == NULL) == (ptp->next == NULL));
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fputs("declared type: ", f_debug);
    db_type(declared_type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return declared_type;
}  /* form_declared_type */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void add_to_derived_type_list(a_type_ptr new_type_ptr,
                              a_type_ptr *derived_type,
                              a_type_ptr *bottom_derived_type,
                              a_boolean  microsoft_property)
/*
Add the type entry pointed to by new_type_ptr to the list of derived-type
entries pointed to by *derived_type (and whose end is pointed to by
*bottom_derived_type).  Aside from the purely mechanical issues of
linking the entries, this routine also checks to see if the resulting
type is legal.  When microsoft_property is TRUE, some of these checks are
omitted (because Microsoft compilers do little checking on the types of
property fields).
*/
{
  a_type_ptr              temp_type, prev_temp_type, tp;
  a_boolean               err = FALSE;
  a_type_kind             tkind;
  a_boolean               array_of_incomp_class_or_enum = FALSE;
  a_boolean               is_member_function_typedef = FALSE;
  a_type_ptr              mft_class_type, mft_rout_type;
  a_symbol_ptr            mft_sym;


  db_enter(3, "add_to_derived_type_list");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "At start of add_to_derived_type_list:\n");
    fprintf(f_debug, "  new_type_ptr = ");
    if (new_type_ptr != NULL) db_type(new_type_ptr);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  derived_type = ");
    if (*derived_type != NULL) db_type(*derived_type);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  *bottom_derived_type = ");
    if (*bottom_derived_type != NULL) db_type(*bottom_derived_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Note that while derived types are being built up, the derived-type
     entries are connected to one another from the top down, which
     means that the bottom-most derived-type entry temporarily points
     to nothing.  Each derived type is checked as the type below it
     is attached.  This must be done carefully, because the type
     being attached may look incomplete (its size may be zero). */
  if (*bottom_derived_type == NULL) {
    /* This is the first entry on the list.  No checking can be done yet. */
    *derived_type = new_type_ptr;
    /* We'll find the real bottom of the type below. */
    *bottom_derived_type = new_type_ptr;
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
      if (any_cfront_mode()) {
        /* Check for a "member function typedef" type -- it can only be used
           to form pointer-to-member types. */
        if (is_cfront_member_function_typedef(new_type_ptr, &mft_rout_type,
                                              &mft_class_type, &mft_sym)) {
          /* The validity of this use in the current context is checked
             later. */
          is_member_function_typedef = TRUE;
        }  /* if */
      }  /* if */
      if (tkind == (a_type_kind)tk_array) {
        /* Array.  See if the element type is proper.  3.1.2.5: the 
           elements must have an object type.  If the element type is
           a partial array or pointer type (see comment above), let it
           by as long as it looks okay otherwise.  Note that
           is_object_type will return FALSE for a type with a size
           of zero, which is the case for the partial array and pointer
           types. */
        temp_type = skip_typerefs(new_type_ptr);
        if (is_object_type(temp_type)) {
          /* Usually okay. */
          if (flexible_array_members_allowed) {
            /* A struct or union containing a member that is a zero-length
               array cannot be an array element type. */
            if (is_class_struct_union_type(temp_type) &&
                temp_type->variant.class_struct_union.
                                contains_flexible_array_member) {
              error(ec_flexible_array_member_not_allowed);
              err = TRUE;
            }  /* if */
          }  /* if */
        } else if (is_pointer_type(temp_type)) {
          /* Okay. */
        } else if (temp_type->kind == (a_type_kind)tk_array &&
                   (has_unknown_specified_bound(temp_type) ||
                    temp_type->
                           variant.array.variant.number_of_elements != 0)) {
          /* Okay. */
          tp = underlying_array_element_type(temp_type);
          if (tp != NULL) {
            tp = skip_typerefs(tp);
            if (is_incomplete_type(tp) &&
                (is_immediate_class_type(tp) || is_immediate_enum_type(tp))) {
              /* This is an array of array ... of incomplete class or enum
                 type.  A diagnostic may be issued (see below), but this is
                 done only when the class is the immediate element type. */
              array_of_incomp_class_or_enum = TRUE;
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
          /* In C++ mode and as an extension in C mode, allow an array of
             incomplete class type.  Obviously, the element type has to be
             completed before the array is actually used. (The array type
             will be added to a list of types to be fixed up when the
             class/struct/union declaration is completed.) */
          a_boolean  complete_type_required = (C_mode() && strict_ansi_mode);
          if (complete_type_required) {
            complete_class_type_is_needed(temp_type);
          }  /* if */
          if (is_incomplete_type(temp_type)) {
            array_of_incomp_class_or_enum = TRUE;
            if (complete_type_required) {
              diagnostic(strict_ansi_error_severity,
                         ec_array_of_incomplete_type);
              if (strict_ansi_error_severity == es_error) err = TRUE;
            }  /* if */
          }  /* if */
        } else if (is_immediate_enum_type(temp_type)) {
          if (is_incomplete_type(temp_type)) {
            /* In C++ mode and as an extension in C mode, allow an array of
               incomplete enum type.  Obviously, the element type has to be
               completed before the array is actually used. (The enum type
               will be added to a list of types to be fixed up when the
               class/struct/union declaration is completed.) */
            array_of_incomp_class_or_enum = TRUE;
            if (C_mode() && strict_ansi_mode) {
              diagnostic(strict_ansi_error_severity,
                         ec_array_of_incomplete_type);
              if (strict_ansi_error_severity == es_error) err = TRUE;
            }  /* if */
          }  /* if */
        } else {
          /* Element type is not okay.  Select a specific error message. */
          if (is_member_function_typedef) {
            /* A cfront member function typedef type can only be used in
               forming a pointer-to-member type. */
            sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
            err = TRUE;
          } else if (is_function_type(temp_type)) {
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
          } else if (!microsoft_property) {
            error(ec_bad_array_element_type);
            err = TRUE;
          }  /* if */
        }  /* if */
        if (err) new_type_ptr = error_type();
        (*bottom_derived_type)->variant.array.element_type = new_type_ptr;
      } else if (is_pointer_type(*bottom_derived_type)) {
        /* Pointer type. */
        if (is_member_function_typedef) {
          /* The code contains "T*" where "T" names a member function typedef.
             It points to a routine type in which the implicit this-param
             type pointer identifies the parent class, say "S".  Then "T*" is
             equivalent to a pointer-to-member declaration, say int S::*(),
             where the return type and argument types are read from the
             routine type pointed to by "T".  What we need is not to add
             something to *bottom_derived_type (as in other cases) but rather
             to change it from a "pointer-to-?" type to a "ptr-to-member"
             type pointing the class and routine type. */
          tp = ptr_to_member_type(mft_rout_type, mft_class_type);
          copy_type(tp, *bottom_derived_type);
          /* Change new_type_ptr and tkind to make it seem as if this were
             an ordinary ptr-to-member declaration. */
          new_type_ptr = mft_rout_type;
          tkind = (a_type_kind)tk_ptr_to_member;
        } else {
          if (is_reference_type(skip_typerefs(new_type_ptr))) {
            /* Pointer to reference is illegal. */
            error(ec_pointer_to_reference);
            new_type_ptr = error_type();
          }  /* if */
          check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                       derived_type,
                                                       bottom_derived_type);
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
        } else if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        }  /* if */
	if (err) new_type_ptr = error_type();
        check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                     derived_type,
                                                     bottom_derived_type);
        (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
      } else if (is_ptr_to_member_type(*bottom_derived_type)) {
        /* Pointer-to-member type. */
        if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        } else if (!check_pm_member_type(new_type_ptr)) {
          err = TRUE;
        } else if (is_function_type(new_type_ptr)) {
          /* This is a pointer-to-member-function type.  Be sure the
             implicit this parameter is set.  If not, create it based on
             the class type. */
          a_type_ptr  class_type = (*bottom_derived_type)->variant.
                                      ptr_to_member.class_of_which_a_member;
          new_type_ptr = check_ptr_to_member_function_type(new_type_ptr,
                                                           class_type);
        }  /* if */
        if (err) new_type_ptr = error_type();
        check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                     derived_type,
                                                     bottom_derived_type);
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
        if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        } else if (is_function_type(new_type_ptr)) {
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
        if (is_qualified_type(new_type_ptr)) {
          /* Qualifier on return type. */
          if (!C_mode() &&
              (is_class_struct_union_type(new_type_ptr) ||
               is_template_param_type(new_type_ptr))) {
            /* In C++ mode class rvalues can have type qualifiers, so allow
               a function returning a qualified class type or a qualified
               template param type (the latter because a function template
               could end up being instantiated with a class type). */
          } else if (get_type_qualifiers(new_type_ptr) == TQ_RESTRICT) {
            /* Exactly one type qualifier -- "restrict".  No warning. */
          } else if (is_reference_type(new_type_ptr)) {
            /* A diagnostic will already have been issued. */
          } else {
            /* Type qualifiers on a function return type are meaningless.
               Note, however, that it is left as part of the type. */
            an_error_severity  severity = es_warning;

            if (C_mode()) {
              if (is_void_type(skip_typerefs(new_type_ptr)) &&
                  get_type_qualifiers(new_type_ptr) == TQ_VOLATILE) {
                /* Issue just a remark for "volatile void" -- gcc uses that to
                   indicate a function (like exit()) that does not return. */
                severity = es_remark;
              }  /* if */
            } else {
              if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
                  !scope_stack[decl_scope_level].in_prototype_instantiation) {
                /* Inside a template instantiation it is sometimes the case
                   that the type qualifier is "useless" for some instantiations
                   but not in general -- e.g.,
                     template <class T> struct A {
                       const T f();
                     };
                     struct X { };
                     A<int> aint;     // A<int>::f returns const int (useless)
                     A<X> ax;         // A<X>::f returns const X (okay)
                   Reduce the severity to a remark to eliminate annoying
                   warnings the user can't do anything about. */
                /* Note that this solution fails to warn on cases that are
                   *always* useless, too.  If A<T>::f returned "T * const" a
                   warning would always be appropriate, whatever T was replaced
                   by in the instantiation.  But the representation of types
                   based on template arguments will have to be improved to
                   make this distinction. */
                severity = es_remark;
              }  /* if */
            }  /* if */
            diagnostic(severity, ec_useless_type_qualifier_on_return_type);
          }  /* if */
        }  /* if */
        if (err) new_type_ptr = error_type();
        check_assertion((*bottom_derived_type)->kind ==
                                                    (a_type_kind)tk_routine);
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
          !microsoft_property &&
          (tkind == (a_type_kind)tk_pointer ||
           tkind == (a_type_kind)tk_ptr_to_member ||
           array_of_incomp_class_or_enum ||
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
  /* Make sure that the new bottom derived type is really the bottom
     and not a node above some other kind of type. */
  *bottom_derived_type = find_bottom_of_type(*bottom_derived_type);

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "At end of add_to_derived_type_list:\n");
    fprintf(f_debug, "  new_type_ptr = ");
    if (new_type_ptr != NULL) db_type(new_type_ptr);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  derived_type = ");
    if (*derived_type != NULL) db_type(*derived_type);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  *bottom_derived_type = ");
    if (*bottom_derived_type != NULL) db_type(*bottom_derived_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* add_to_derived_type_list */


static an_exception_specification_ptr scan_exception_specification(
                                  a_func_info_block  *func_info,
                                  a_boolean          exception_spec_allowed,
                                  a_boolean          is_top_level_declarator)
/*
Scan a throw specification, which may be empty or take either of two forms:

  throw ( type-name [, type-name]... )
  throw ()

A throw specification with a list of names means "these types will be
thrown".  A throw specification with an empty list ("throw ()") means "no
exception will be thrown".  An empty throw specification means "any
exception may be thrown".

Return a (possibly NULL) pointer to the appropriate kind of throw
specification entry.

Diagnostics are issued on redundant types on a list, but if this is a
redeclaration of a routine, reconciliation with the previous throw
specification is handled later (see check_exception_specification).
*/
{
  an_exception_specification_ptr       esp = NULL;
  an_exception_specification_type_ptr  estp, other_estp, end_of_list = NULL;
  a_source_position                    type_pos;
  a_boolean                            ignoring_exception_spec = FALSE;

  db_enter(4, "scan_exception_specification");
  if (exceptions_enabled || curr_token == tok_throw) {
    /* Update the source position for the "throw".  Even if there is no
       "throw" this is where it would appear in the source.  If exception
       support is not enabled but a "throw" appears, we may want to issue
       a diagnostic, so save the source position for that case, too. */
    func_info->throw_position = pos_curr_token;
  }  /* if */
  if (curr_token != tok_throw) {
    /* No explicit throw specification, meaning anything may be thrown. */
    goto done;
  }  /* if */
  if (!exceptions_enabled || !exception_spec_allowed ||
      ignore_exception_specifications) {
    /* If exception-handling support is not enabled, or if this is a context
       in which an exception specification is not allowed, or if (e.g., in
       Microsoft-compatibility mode) exception specifications are recognized
       but ignored, set a flag to control the diagnostics that are put out. */
    ignoring_exception_spec = TRUE;
  }  /* if */
  if (!ignoring_exception_spec) {
    /* Exceptions are outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_exceptions_in_embedded_cplusplus);
    esp = alloc_exception_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
    esp->source_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else if (!exception_spec_allowed) {
    /* This is a declaration on which an exception specification is not
       allowed. */
    pos_diagnostic((!exceptions_enabled || ignore_exception_specifications) ?
                       es_warning : es_discretionary_error,
                   ec_exception_specification_not_allowed, &pos_curr_token);
  } else if (ignore_exception_specifications) {
    /* Issue a warning (e.g., in Microsoft mode) -- exception specifications
       are parsed and discarded. */
    pos_remark(ec_exception_specification_ignored, &pos_curr_token);
  }  /* if */
  /* Bypass "throw". */
  (void)get_token();
  /* Start a new stop token state. */
  push_stop_token_stack();
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_rparen);
  /* Next token should be a left paren. */
  if (curr_token == tok_lparen) {
    (void)get_token();
    if (curr_token == tok_rparen) {
      /* Case is "throw ()" -- which means "no exception will be thrown by
         this routine." */
      goto finish_list;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && curr_token == tok_ellipsis) {
      /* Microsoft compilers treat function with "C" linkage as having an
         implicit "throw()" specification.  For those functions with "C"
         linkage that can throw any exception, an explicit "throw(...)" must
         be specified. */
      /* Bypass the ellipsis. */
      (void)get_token();
      if (esp == NULL) {
        esp = alloc_exception_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
        esp->source_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
      esp->throw_any = TRUE;
      goto finish_list;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  } else {
    /* Syntax error -- left paren is missing.  We don't actually call
       syntax_error or required_token for this, however, since writing
       "throw int" instead of "throw (int)" might be a common mistake. */
    error(ec_exp_lparen);
  }  /* if */
  /* Loop through the types. */
  do {
    add_stop_token(tok_comma);
    /* Allocate the throw spec type entry. */
    estp = alloc_exception_specification_type();
#if EXTRA_SOURCE_POSITIONS_IN_IL
    estp->source_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    type_pos = pos_curr_token;
    if (!is_decl_start(/*expr_context=*/FALSE,
                       /*real_declarator_allowed=*/FALSE) ||
        !is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED)) {
      /* Error. */
      pos_error(ec_exp_type_specifier, &type_pos);
      /* Flush tokens to the comma or right paren. */
      flush_tokens();
      estp->type = error_type();
    } else {
      type_name(&estp->type);
      if (exceptions_enabled && !is_error_type(estp->type)) {
        /* Check the type to be sure it's not an incomplete type or a pointer
           to an incomplete type. */
        a_type_ptr     tp = estp->type;
        an_error_code  error_code = ec_no_error;

        /* Issue a diagnostic if an incomplete type is indicated in the
           exception specification.  According to the standard, this is always
           an error, but it really only makes a difference on a function
           definition.  We don't know at this point whether a top-level
           declarator belongs to a function definition or not, so we defer
           issuing the diagnostic in that case. */
        /* Force instantiation of template class. */
        complete_type_is_needed(tp);
        if (is_incomplete_type(tp)) {
          /* Incomplete type (including possibly void type). */
          error_code = ec_incomplete_type_not_allowed;
        } else if (is_ptr_or_ref_type(tp)) {
          tp = type_pointed_to(tp);
          if (is_void_type(tp)) {
            /* Pointer to cv-qualified void is okay. */
          } else {
            /* Force instantiation of template class. */
            complete_type_is_needed(tp);
            if (is_incomplete_type(tp)) {
              error_code = ec_ptr_or_ref_to_incomplete_type;
            }  /* if */
          }  /* if */
        }  /* if */
        if (!ignoring_exception_spec && error_code != ec_no_error) {
          /* Defer a diagnostic if this is a top-level declarator and the
             type is something other than "void"; in strict mode or if the
             type is "void", issue a diagnostic.  Otherwise, suppress the
             diagnostic -- that is, silently allow a non-top-level declaration
             that throws an incomplete type (or pointer thereto) */
          if (is_top_level_declarator && !is_void_type(tp)) {
            defer_exception_spec_error(func_info, error_code, &type_pos);
          } else if (strict_ansi_mode) {
            pos_diagnostic(strict_ansi_discretionary_severity, error_code,
                           &type_pos);
          } else if (is_void_type(tp)) {
            pos_error(error_code, &type_pos);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (esp != NULL) {
      /* Add estp to the list. */
      if (end_of_list == NULL) {
        esp->exception_specification_type_list = estp;
      } else {
        if (!is_error_type(estp->type)) {
          /* Examine other entries already on the list to see if the current
             one is redundant. */
          other_estp = esp->exception_specification_type_list;
          for (; other_estp != NULL; other_estp = other_estp->next) {
            if (!other_estp->redundant &&
                identical_types(estp->type, other_estp->type)) {
              pos_remark(ec_redundant_exception_specification_type, &type_pos);
              estp->redundant = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        /* Add it to the end of the list. */
        end_of_list->next = estp;
      }  /* if */
      end_of_list = estp;
      if (!estp->redundant && !is_error_type(estp->type)) {
        /* Mark the type as having been used in an exception.  (Also, if it
           "contains" any classes, they are marked as requiring external
           linkage.) */
        set_used_in_exception_or_rtti_flag(estp->type);
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
finish_list:;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (esp != NULL) {
    esp->source_range.end = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* List should be terminated by a right paren. */
  remove_stop_token(tok_rparen);
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  /* Restore the stop token state. */
  pop_stop_token_stack();
done:;
  db_exit();
  return esp;
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


static a_boolean current_scope_is_class(a_type_ptr type)
/*
Determine whether the current scope is the class scope of the given class
type.  For templates, use the class template scope.
*/
{
  a_boolean                result, instance;
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  instance = (ssep->kind == (a_scope_kind)sck_template_instantiation);
  if (ssep->kind == (a_scope_kind)sck_template_declaration || instance) {
    --ssep;
  }  /* if */
  /* If an instantiation scope (for a function declaration) sits on top of
     a class reactivation scope, we are presumably rescanning a function
     member declared inside a class. */
  if ((ssep->kind == (a_scope_kind)sck_class_struct_union ||
       (instance && ssep->kind == (a_scope_kind)sck_class_reactivation)) &&
      ssep->assoc_type == type) {
    result = TRUE;
  } else {
    result = FALSE;
  }  /* if */
  return result;
} /* current_scope_is_class */


static void function_declarator(a_type_ptr        *new_type_ptr,
                                a_func_info_block *func_info,
                                a_symbol_locator  *locator,
                                a_type_ptr        member_function_parent_type,
                                a_boolean         is_nonstatic_member_function,
                                a_boolean         is_constructor,
                                a_boolean         is_destructor,
                                a_boolean         disallow_default_args,
                                a_boolean         disallow_exception_spec,
                                a_boolean         is_typedef_decl,
				a_boolean	  is_friend_decl,
                                a_decl_pos_block  *decl_pos_block)
/*
Scan a function declarator (3.5.4.3), or an array declarator in an
abstract declarator (3.5.5).  Allocate and return in *new_type_ptr an
appropriate function type.  The initial opening parenthesis has
already been checked and passed over (which is unusual; that's
necessary because of the syntactic strangeness of abstract
declarators).  If func_info is NULL, then the function declarator is
not a top type or this is an abstract declarator (and therefore
certain forms are disallowed); otherwise, extra information about the
function declarator is returned in *func_info.  For member functions,
member_function_parent_type is a pointer to the class (or struct or
union) type of which it is a member; otherwise it is NULL.  When it is
non-NULL, is_nonstatic_member_function will distinguish static from
nonstatic member functions when the current scope is that of a class
definition.  is_constructor or is_destructor is TRUE if previous
processing had determined that this is a constructor or destructor
declaration, respectively.  If disallow_default_args is TRUE issue an
error if a default argument expression is encountered.  is_friend_decl
is TRUE if this is the function declarator in a friend function
declaration.
*/
{
  a_param_type_ptr        ptp;
  a_storage_class         param_storage_class;
  a_type_ptr              param_type_ptr, declared_type, tp;
  a_decl_flag_set         dso_flags;
  a_boolean               qualifier_err = FALSE;
  a_decl_modifiers_block  decl_modifiers;
  a_param_type_ptr        last_param_type;
  a_param_id_ptr          last_param_id;
  a_source_sequence_entry_ptr
                          param_ssep;
  a_symbol_locator        param_locator;
  a_boolean               done;
  a_boolean               any_params;
  a_source_position       start_pos, param_type_pos;
  a_routine_type_supplement_ptr
                          extra_info;
  a_boolean               dangling_type_specifier = FALSE;
  a_boolean               defines_something;
  a_boolean               default_arg_allowed_on_curr_param = FALSE;
  a_boolean               may_be_copy_constructor = FALSE;
  a_boolean               bad_first_param_for_copy_constructor = FALSE;
  a_source_position       pos_of_first_param_type;
  a_func_info_block       local_func_info_block;
  a_token_cache		  decl_token_cache;
  a_boolean               is_top_level_declarator = TRUE;

  db_enter(3, "function_declarator");
  copy_source_position(pos_curr_token, start_pos);
  set_err_pos_to_curr_token();
  add_stop_token(tok_rparen);
  /* Initialize a token cache that might be needed for default argument
     processing. */
  clear_token_cache(&decl_token_cache, /*reusable=*/TRUE);
  /* If the caller passed in a func_info pointer, this is the declarator of
     a "top-level" function declaration.  Use the storage passed in by the
     caller.  But if func_info is NULL, use a local func info block.  This
     is mainly useful for managing param_id entries properly. */
  if (func_info == NULL) {
    clear_func_info(&local_func_info_block);
    func_info = &local_func_info_block;
    is_top_level_declarator = FALSE;
  }  /* if */
  last_param_id = NULL;
  *new_type_ptr = alloc_type((a_type_kind)tk_routine);
  extra_info = (*new_type_ptr)->variant.routine.extra_info;
  if (is_constructor) extra_info->assoc_routine_is_ctor = TRUE;
  if (is_destructor) extra_info->assoc_routine_is_dtor = TRUE;
  extra_info->param_type_list = NULL;
  /* Copy the current name linkage into the routine type.  It will not
     necessarily correspond to the name linkage of the routine (if any) with
     which this type is associated. */
  extra_info->routine_name_linkage =
                          scope_stack[depth_scope_stack].default_name_linkage;
  if (!is_name_linkage_kind_for_rout_type(extra_info->routine_name_linkage)) {
    /* Custom name linkage kinds may presumably not affect routine types
       (i.e., calling conventions). */
    check_assertion(!C_mode() &&
                    extra_info->routine_name_linkage >
                                      (a_name_linkage_kind)nlk_last_standard);
    extra_info->routine_name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
  }  /* if */
  if (scope_stack[depth_scope_stack].name_linkage_is_explicit) {
    extra_info->routine_name_linkage_is_explicit = TRUE;
  }  /* if */
#if CHECKING
  if (extra_info->routine_name_linkage == (a_name_linkage_kind)nlk_none ||
      extra_info->routine_name_linkage == (a_name_linkage_kind)nlk_internal) {
    unexpected_condition_str2("function_declarator:",
                              "bad default name linkage kind");
  }  /* if */
#endif /* CHECKING */
  if (curr_token == tok_rparen) {
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ f() is equivalent to f(void).  Leave param_type_list empty. */
      extra_info->prototyped = TRUE;
    } else {
      /* In C, f() is an old-style empty parameter list. */
      extra_info->prototyped = FALSE;
    }  /* if */
    any_params = FALSE;
  } else if (curr_token == tok_ellipsis &&
             (!C_mode() || allow_ellipsis_only_param_in_C_mode)) {
    /* The first thing in the parameter list is an ellipsis. */
    if (is_destructor) {
      /* Destructors are allowed no arguments. */
      error(ec_too_many_params_for_destructor);
    } else {
      /* In C++ f(...) is legal, though it is not recommended since it is not
         portable (ARM 8.3).  In C it's an extension that is supported when
         allow_ellipsis_only_param_in_C_mode is TRUE. */
      extra_info->has_ellipsis = TRUE;
      /* An ellipsis only occurs in prototyped param lists. */
      extra_info->prototyped = TRUE;
#if ASM_FUNCTION_ALLOWED
      if (func_info->is_asm_function) {
        pos_error(ec_bad_asm_func_ellipsis, &pos_curr_token);
      } else {
#endif /* ASM_FUNCTION_ALLOWED */
        if (C_mode() && strict_ansi_mode) {
          /* Issue a diagnostic on use of a nonstandard feature. */
          pos_diagnostic(strict_ansi_error_severity,
                         ec_nonstd_ellipsis_only_param, &pos_curr_token);
        }  /* if */
#if ASM_FUNCTION_ALLOWED
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
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
    if (any_params && !disallow_default_args) {
      /* In C++ mode a default argument may be declared with the parameter
         unless the function is a user-defined overloaded operator (except
         operator()(), for which a default argument is allowed) or a
         user-defined conversion.  Note that locator may be NULL (e.g., with
         abstract declarators). */
      /* operator new(), new[](), delete(), and delete[]() can also take
         default arguments in the second and successive arguments -- this
         is implied by ARM 13.4, which excludes those operators from the
         restrictions that are listed for overloaded operators in general.
         We don't set the flag till after the first parameter has been seen,
         however; see below.  The test for template-ids is used to disallow
         default arguments on friend declarations that refer to an explicit
         template instance through the use of an explicit template argument
         list. */
      if (locator != NULL && !locator->is_conversion_name &&
          !locator->is_template_id &&
          (!locator->is_operator_name ||
           locator->variant.opname == (an_opname_kind)onk_function_call)) {
        default_arg_allowed_on_curr_param = TRUE;
      }  /* if */
    }  /* if */
    /* Push a function prototype scope for the parameters. */
    (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                     *new_type_ptr, (a_routine_ptr)NULL);
    /* Remember the scope number for later use if and when a body appears. */
    func_info->scope_number = scope_stack[depth_scope_stack].number;
    if (any_params) {
      last_param_type = NULL;
      do {
        a_type_qualifier_set qualifiers = TQ_NONE;
        a_decl_pos_block     local_decl_pos_block;
        a_decl_flag_set      dsi_flags = DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                                         DSI_TYPE_SPECIFIER_ALLOWED |
                                         DSI_IS_PARAMETER |
                                         DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
        if (gcc_mode && curr_token == tok_extension) {
          /* Ignore the GNU C __extension__ annotation. */
          (void)get_token();
          dsi_flags |= DSI_MARKED_AS_GNU_EXTENSION;
        }  /* if */
        add_stop_token(tok_comma);
        copy_source_position(pos_curr_token, param_type_pos);
        clear_decl_pos_block(&local_decl_pos_block);
        /* Scan a parameter-declaration. */
        (void)decl_specifiers(dsi_flags, &dso_flags, &param_storage_class,
                              &param_type_ptr, &qualifiers, 
                              &decl_modifiers, &local_decl_pos_block);
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
        } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
          /* No type specifier (aside from const or volatile) appeared among
             the decl_specifiers.  Issue a diagnostic. */
          report_implicit_int(&pos_curr_token);
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
          a_decl_flag_set  do_flags, di_flags;

          di_flags = DI_IS_PARAMETER_DECL |
                     DI_REAL_DECLARATOR_ALLOWED |
                     DI_ABSTRACT_DECLARATOR_ALLOWED;
          if (is_typedef_decl) {
            /* At the top level this is a typedef declaration. */
            di_flags |= DI_IS_TYPEDEF_DECLARATION;
          }  /* if */
          if (vla_enabled) {
            /* Permit a variable length array declaration. */
            di_flags |= DI_VLA_ALLOWED | DI_VLA_ASTERISK_ALLOWED;
          }  /* if */
          declarator(di_flags, &do_flags, param_type_ptr,
                     /*member_parent_type=*/(a_type_ptr)NULL,
                     &param_locator, &param_type_ptr, &param_ssep,
                     (a_func_info_block_ptr)NULL, &local_decl_pos_block);
        } else {
          /* No declarator. */
          set_to_error_locator(param_locator);
        }  /* if */
        /* Save a pointer to the type as it was declared (i.e., before the
           array-to-pointer adjustment, if any). */
        declared_type = param_type_ptr;
        /* Check that the type is legal, and do required adjustments. */
        check_and_adjust_parameter_type(&param_type_ptr, &param_type_pos);
        /* Standardize the storage class: unspecified becomes auto. */
        if (param_storage_class == (a_storage_class)sc_unspecified) {
          param_storage_class = (a_storage_class)sc_auto;
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Make adjustments on the param source sequence entry before it is
           bound to the param_id entry. */
        if (!is_top_level_declarator || is_template_dependent_context()) {
          /* If a parameter id was specified in a non-top-level function
             declarator, a source sequence entry created for it is useless.
             In certain configurations source sequence entries are put out
             during prototype instantiation of class templates -- but since
             the function body won't be scanned at this time, the source
             sequence entry for the param id should be eliminated in that
             case, too. */
          if (param_ssep != NULL) {
            remove_from_src_seq_list(param_ssep);
            param_ssep = NULL;
          }  /* if */
        } else if (param_ssep == NULL) {
          /* Declarator was not called or param_ssep was not created for some
             some other reason.  Still, if this turns out to be a function
             definition, it will be needed (in C++ unnamed parameters are
             allowed). */
#if DEBUG
          if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
            if (!is_error_locator(param_locator)) {
              fprintf(f_debug,
                     "function_declarator: empty ss entry for param \"%s\":\n",
                      param_locator.symbol_header->identifier);
            }  /* if */
          }  /* if */
#endif /* DEBUG */
          param_ssep = add_empty_source_sequence_entry();
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (is_error_locator(param_locator)) {
          /* There was no declarator. */
          func_info->any_prototype_names_omitted = TRUE;
        }  /* if */
        /* Add an entry to record the parameter name and other information
           associated with the parameter declaration.  These go on to the
           the param-id list. */
        add_to_param_id_list(&param_locator, param_type_ptr,
                             &param_type_pos, param_storage_class,
                             func_info, param_ssep, &last_param_id);
        last_param_id->declared_type = declared_type;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        last_param_id->specifiers_range =
                            local_decl_pos_block.specifiers_range;
	last_param_id->declarator_range =
                            local_decl_pos_block.declarator_range;
	last_param_id->identifier_range =
                            local_decl_pos_block.identifier_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        if (remove_qualifiers_from_param_types) {
          /* Strip off top-level type qualifiers.  They are not part of the
             type signature of a C++ function -- see 8.3.5 para 3.  However,
             because they do belong to the type of the parameter variable,
             they were not removed before add_to_param_id_list was called. */
          /* Note: whether to remove top-level qualifiers is sensitive to the
             ABI version because qualifiers are reflected in mangled names. */
          check_assertion(!C_mode());
          param_type_ptr = make_unqualified_type(param_type_ptr);
        }  /* if */
        /* Create a param-type entry and add it to the list of param-types
           associated with the routine type. */
        ptp = make_param_type(param_type_ptr, &param_type_pos);
        ptp->declared_type = declared_type;
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
        if (!is_error_locator(param_locator)) {
          ptp->name = param_locator.symbol_header->identifier;
        }  /* if */
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        {
        /* Update source range information in the param-type entry. */
        a_decl_position_supplement_ptr  dpsp;

        dpsp = alloc_decl_position_supplement(in_file_scope(ptp));
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
        dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
        dpsp->variant.declarator_range = local_decl_pos_block.declarator_range;
        ptp->decl_pos_info = dpsp;
        }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        if (remove_qualifiers_from_param_types) {
          /* Record the top-level type qualifiers that were declared for this
             parameter and then removed. */
          check_assertion(!C_mode());
          ptp->qualifiers = get_type_qualifiers(last_param_id->type);
        }  /* if */
        if (last_param_type == NULL) {
          extra_info->param_type_list = ptp;
        } else {
          last_param_type->next = ptp;
        }  /* if */
        last_param_type = ptp;
        if (curr_token == tok_assign && C_dialect == C_dialect_cplusplus) {
          /* Argument expressions are not allowed in overloaded operator
             declarations.  Issue an error, but go ahead and scan the
             expression. */
          a_scope_kind		parent_scope_kind;
          a_scope_stack_entry_ptr
				parent_ssep;
          a_boolean		is_member_or_friend_function;
          a_boolean		cache_default_arg;
          a_boolean		ignore_default_arg_expr;
          a_boolean		invalid_default_arg = FALSE;
          a_param_type_ptr	ptp_for_scan;
          a_scope_depth		def_arg_scope_depth = NO_SCOPE_DEPTH;

          if (!default_arg_allowed_on_curr_param) {
            pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);

          } else if (strict_ansi_mode && !is_top_level_declarator) {
            /* In strict mode default arguments are only allowed on top-level
               function declarations (i.e., not on typedef declarations,
               pointer-to-function or pointer-to-member-function declarations,
               param type declarations, etc.). */
            pos_diagnostic(strict_ansi_discretionary_severity,
                           ec_nonstd_default_arg, &pos_curr_token);
          }  /* if */
          /* Advance past the equal sign. */
          (void)get_token();
          /* Check the scope immediately containing the current scope, which
             is a function prototype scope.  We may have to cache the
             default argument tokens and rescan them later. */
          cache_default_arg = FALSE;
          is_member_or_friend_function = FALSE;
          ignore_default_arg_expr = !default_arg_allowed_on_curr_param;
          parent_ssep = &scope_stack[depth_scope_stack-1];
          parent_scope_kind = parent_ssep->kind;
          if (default_arg_allowed_on_curr_param) {
            if (parent_scope_kind == (a_scope_kind)sck_class_struct_union) {
              /* A member function of a class (normal or template) inside
                 a class declaration. */
              cache_default_arg = TRUE;
              is_member_or_friend_function = TRUE;
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_declaration) {
              /* A function template declaration.  Note that all default
                 arguments are cached. */
              cache_default_arg = TRUE;
              def_arg_scope_depth = scope_depth_of(parent_ssep);
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_instantiation) {
              /* A template instantiation -- the function declarator tokens are
                 being rescanned.  All the default arguments are scanned from
                 caches during a later fixup, so ignore the expression now. */
              cache_default_arg = TRUE;
              ignore_default_arg_expr = TRUE;
              def_arg_scope_depth = scope_depth_of(parent_ssep);
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_class_reactivation ||
                       parent_scope_kind ==
                                   (a_scope_kind)sck_namespace_reactivation) {
              /* First skip surrounding reactivation scopes.  While doing so
                 skip any template instantiation scopes pushed as Microsoft
                 mode specialization scopes.  These should be considered
                 to be part of the associated class reactivation. */
              int  template_scope = depth_scope_stack-1;
              a_scope_stack_entry_ptr	ssep = &scope_stack[template_scope];
              while (ssep->kind == (a_scope_kind)sck_class_reactivation ||
                     ssep->kind == (a_scope_kind)sck_namespace_reactivation) {
                if (ssep->kind == (a_scope_kind)sck_class_reactivation &&
                    ssep->microsoft_specialization_scope_pushed) ssep--;
                ssep--;
              }  /* while */
              if (ssep->kind == (a_scope_kind)sck_template_declaration) {
                /* A member function declaration of a template class outside
                   of the class declaration.  This is not allowed, except
                   in Microsoft mode. */
                if (microsoft_mode) {
                  /* This is a template case, so the default should be
                     cached. */
                  cache_default_arg = TRUE;
                  def_arg_scope_depth = scope_depth_of(ssep);
                } else {
                  pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);
                  default_arg_allowed_on_curr_param = FALSE;
                  ignore_default_arg_expr = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          if (curr_token == tok_comma || curr_token == tok_rparen ||
              curr_token == tok_semicolon || curr_token == tok_rbrace || 
              curr_token == tok_lbrace) {
            /* There was an "=" sign, but the following token is not one
               that can begin a default argument. */
            invalid_default_arg = TRUE;
          }  /* if */
          /* A NULL param type pointer is used as a signal to the caching
             and scanning routines that the default argument should be
             ignored. */
          ptp_for_scan = ignore_default_arg_expr ? NULL : ptp;
          if (cache_default_arg) {
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
            if (invalid_default_arg && is_member_or_friend_function &&
                scope_stack[depth_scope_stack].in_prototype_instantiation) {
              /* During a prototype instantiation default arguments are
                 cached, but not rescanned.  Issue the syntax error here. */
              pos_error(ec_exp_primary_expr, &pos_curr_token);
            }  /* if */
            if (is_member_or_friend_function) {
              /* Scan the default arguments for a member or friend
                 function. */
              prescan_member_function_default_arg_expr(ptp_for_scan,
						       is_friend_decl,
                                                       &decl_token_cache);
            } else {
              /* Scan the default arguments for a function template. */
              prescan_function_template_default_arg_expr(ptp_for_scan,
                                                         def_arg_scope_depth);
            }  /* if */
          } else {
            /* Not a case in which the default argument should be
               cached -- or else a syntax error.  Go ahead and
               scan the expression and convert it to the required type. */
            scan_default_arg_expr(ptp_for_scan);
          }  /* if */
          if (default_arg_allowed_on_curr_param) {
            ptp->has_default_arg = TRUE;
            func_info->any_default_args = TRUE;
          }  /* if */
        }  /* if */
        if (!disallow_default_args && !default_arg_allowed_on_curr_param) {
          if (last_param_type == extra_info->param_type_list) {
            /* The first parameter on the list has just been processed. */
            if (locator != NULL && locator->is_operator_name &&
                (is_new_operator(locator->variant.opname) ||
                 is_delete_operator(locator->variant.opname))) {
              /* Default argument expressions are permitted on the second and
                 subsequent parameters of an operator new and delete
                 declarations. */
              default_arg_allowed_on_curr_param = TRUE;
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
          extra_info->has_ellipsis = TRUE;
#if ASM_FUNCTION_ALLOWED
          if (func_info->is_asm_function) {
            pos_error(ec_bad_asm_func_ellipsis, &pos_curr_token);
          }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
          (void)get_token();
          done = TRUE;
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
            tp = skip_typerefs(param_type_ptr);
            if (identical_types(member_function_parent_type, tp)) {
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
              if (is_reference_type(param_type_ptr)) {
                tp = type_pointed_to(param_type_ptr);
                tp = skip_typerefs(tp);
                if (identical_types(member_function_parent_type, tp)) {
                  /* Depending on whether the next parameter has a default
                     argument, this may be a copy constructor. */
                  may_be_copy_constructor = TRUE;
                }  /* if */
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
              tp = skip_typerefs(ptp->type);
              if (identical_types(member_function_parent_type, tp)) {
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
    if (is_top_level_declarator) {
      /* Note that a pointer to the current entry of scope_stack is not saved
         from earlier in this routine because scope_stack might have been
         reallocated in the interim. */
      func_info->prototype_scope_symbols =
             assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Transfer the source sequence list in the function prototype scope
         over to the func_info block. */
      func_info->prototype_scope_ss_list =
                         scope_stack[depth_scope_stack].source_sequence_list;
      scope_stack[depth_scope_stack].source_sequence_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    /* Process pragmas associated with the closing paren before the current
       scope is popped. */
    process_curr_token_pragmas();
    /* Before popping the scope, move the vla_fixup_list from the scope_stack
       to func_info. */
    func_info->vla_fixup_list = scope_stack[depth_scope_stack].vla_fixup_list;
    scope_stack[depth_scope_stack].vla_fixup_list = NULL;
    /* Pop the function prototype scope. */
    pop_scope();
  } else if (any_params) {
    /* Old-style list of identifiers. */
    if (!is_top_level_declarator) {
      /* This type of parameter list is not valid in abstract declarators
         and non-top-level function declarators. */
      if (microsoft_mode && C_mode()) {
        /* No diagnostic in Microsoft C mode. */
      } else {
        error(ec_param_id_list_needs_function_def);
      }  /* if */
    } else if (C_dialect == C_dialect_cplusplus) {
      /* This type of parameter list is an anachronism in C++. */
      diagnostic(anachronism_error_severity, ec_old_style_parameter_list);
    }  /* if */
    do {
      add_stop_token(tok_comma);
      /* Scan the list of identifiers. */
      if (curr_token != tok_identifier) {
        if (curr_token == tok_ellipsis && next_token() == tok_rparen) {
          /* In Microsoft C an ellipsis is permitted (and ignored) on an
             old-style param list. */
          diagnostic(microsoft_mode ? es_warning : es_error,
                     ec_ellipsis_not_allowed);
          /* Advance past the ellipsis. */
          (void)get_token();
        } else {
          /* Error, expected identifier. */
          (void)required_token(tok_identifier, ec_exp_identifier);
        }  /* if */
      } else {
        /* See if the identifier is also a typedef name.  Such a name is
           not allowed (3.7.1, constraints).  In pcc mode, however, this
           is allowed. */
        if (C_dialect != C_dialect_pcc && !microsoft_bugs &&
            curr_id_is_type_name()) {
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
        /* Update the param-id entry just created with the source position
           of the identifier. */
        last_param_id->old_style_id_pos = locator_for_curr_id.source_position;
        /* Advance past the identifier. */
        (void)get_token();
      }  /* if */
      remove_stop_token(tok_comma);
      /* Keep looping on a comma, stop otherwise. */
    } while (loop_token(tok_comma));
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.end = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for closing right parenthesis.  We temporarily clear the stop
     token array values for tok_comma and tok_assign, in order to flush past
     either to the right paren. */
  { a_token_set_array_element t1;
    a_token_set_array_element t2;
    t1 = curr_stop_token_stack_entry->stop_tokens[(int)tok_comma];
    t2 = curr_stop_token_stack_entry->stop_tokens[(int)tok_assign];
    curr_stop_token_stack_entry->stop_tokens[(int)tok_comma] = 0;
    curr_stop_token_stack_entry->stop_tokens[(int)tok_assign] = 0;
    (void)required_token(tok_rparen, ec_exp_rparen);
    curr_stop_token_stack_entry->stop_tokens[(int)tok_comma] = t1;
    curr_stop_token_stack_entry->stop_tokens[(int)tok_assign] = t2;
  }
  remove_stop_token(tok_rparen);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* If this function type was declared with an ellipsis, its calling
       convention is required to be __cdecl.  If this isn't already the
       default for the compilation, set it in the type.  (Ordinarily, the
       setting in the type reflects an explicit specification of the calling
       convention.) */
    if (extra_info->has_ellipsis &&
        default_calling_convention != (a_calling_convention)cc_cdecl) {
      extra_info->calling_convention = (a_calling_convention)cc_cdecl;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (C_dialect == C_dialect_cplusplus) {
    a_type_ptr            this_class = NULL;
    a_type_qualifier_set  qualifiers = TQ_NONE;

    /* Create a pointer to the implicit "this" parameter.  This can be done
       for nonstatic function declarations within a class definition or
       for member function declarations outside a class definition when
       a function qualifier is present.  If there is a function qualifier,
       it is applied to the type pointed to by the this param type. */
    if ((is_type_qualifier() or_is_near_or_far() ||
         (microsoft_mode && curr_token == tok_inline)) &&
        extra_info->prototyped) {
      /* In C++ the type of certain member functions may be qualified.  Scan
         for a const or volatile qualifier. */
      a_source_position  qualifier_pos;

      copy_source_position(pos_curr_token, qualifier_pos);
      qualifiers = collect_type_qualifiers(decl_pos_block);
      /* When a member function is declared with the restrict qualifier, the
         qualifier attaches to the this pointer, not to *this (as with const
         and volatile). */
      /* If this is not a member function or it is but it is a static member
         function declared within a class definition, a qualifier on the
         function is illegal (ARM 8.2.5).  However, qualifiers on a pointer
         to member function are permitted.  Also, in microsoft mode the
         keyword "inline" is always accepted as a qualifier (a warning that
         it is ignored will have been issued earlier). */
      if (microsoft_mode && qualifiers == TQ_NONE) {
        /* No diagnostic and no need to adjust the type of this or *this. */
      } else if (locator != NULL && locator->is_operator_name &&
                 (is_new_operator(locator->variant.opname) ||
                  is_delete_operator(locator->variant.opname))) {
        /* Operator new and delete can never be qualified. */
        qualifier_err = TRUE;
      } else if (member_function_parent_type == NULL && !is_typedef_decl) {
        /* Cv-qualifier is allowed on a member function only. */
        qualifier_err = TRUE;
      } else if (!is_nonstatic_member_function && !is_typedef_decl &&
                 current_scope_is_class(member_function_parent_type)) {
        /* This must be the declaration of a static member function inside
           its class definition.  "const" and "volatile" are not allowed,
           but with Cfront it's sometimes okay (depending on the return type!)
           so just put out a warning in cfront mode. */
        if (any_cfront_mode() && member_function_parent_type != NULL &&
            !is_nonstatic_member_function) {
          pos_warning(ec_function_qualifier_not_allowed, &qualifier_pos);
        } else {
          qualifier_err = TRUE;
        }  /* if */
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
        this_class = member_function_parent_type;
        qualifiers = TQ_NONE;
      } else {
        this_class = member_function_parent_type;
      }  /* if */
      if (qualifier_err && 
          scope_stack[depth_scope_stack].kind !=
                                   (a_scope_kind)sck_template_instantiation) {
        /* The qualifier was not allowed here, but if we're parsing an
           instantiation, the error was already emitted when parsing the
           template declaration. */
        pos_error(ec_function_qualifier_not_allowed, &qualifier_pos);
      }  /* if */
    }  /* if */
    if (is_nonstatic_member_function &&
        qualifiers == TQ_NONE && !qualifier_err) {
      /* This is a nonstatic member function declared within the definition
         of the class indicated, but without significant qualifiers. */
      this_class = member_function_parent_type;
    }  /* if */
    if (this_class != NULL &&
        this_class->kind == (a_type_kind)tk_template_param) {
      /* Ensure that "this_class" points to a class type. */
      this_class = proxy_class_for_template_param(this_class);
    }  /* if */
    /* The implicit "this" param type will be either "pointer to class-type"
       or, if there was a const qualifier on the function, "pointer to const
       class-type".  However, it is possible to have a cv-qualified function
       type in a typedef declaration.  So the qualifiers and the class type
       are encoded separately.  E.g. in
          typedef void CF() const;
       this_class == NULL but qualifiers != TQ_NONE. */
    extra_info->this_class = this_class;
    extra_info->qualifiers = qualifier_err ? TQ_NONE : qualifiers;
#if 0
    /* Should a diagnostic be issued if a throw specification appears other
       than on a top-level declaration? */
    if (curr_token == tok_throw && !is_top_level_declarator) {
      /* Error?  Warning? */
    }  /* if */
#endif /* if 0 */
    extra_info->exception_specification =
                       scan_exception_specification(func_info,
                                                    !disallow_exception_spec,
                                                    is_top_level_declarator);

  }  /* if */
  if (!is_top_level_declarator) {
    done_with_func_info(local_func_info_block);
  }  /* if */
  copy_source_position(start_pos, error_position);
  if (decl_token_cache.first_token != NULL) {
    /* If a declaration token cache was built while processing member function
       default arguments, free it now. */
    discard_token_cache(&decl_token_cache);
  }  /* if */
  db_exit();
}  /* function_declarator */


void array_declarator(a_type_ptr            *new_type_ptr,
                      a_boolean             nonconstant_dimension_allowed,
                      a_boolean             vla_allowed,
                      a_boolean             vla_asterisk_allowed,
                      a_boolean             top_level_field_decl,
                      a_boolean             top_level_param_decl,
                      a_decl_pos_block_ptr  decl_pos_block)
/*
Scan an array declarator (ISO C 6.5.4.2), or an array declarator in an
abstract declarator (ISO C 6.5.5).  Allocate and return in *new_type_ptr an
appropriate array type.  The initial opening bracket is the current token.
In C++ the dimension may sometimes be a nonconstant expression (e.g., with a
new type name); that case is indicated by nonconstant_dimension_allowed.  In
C (when vla_enabled is TRUE), the dimension may be a nonconstant expression
when vla_allowed is TRUE; and when vla_asterisk_allowed is TRUE, a VLA of
unknown size can be indicated with the "[*]" syntax in a function prototype.
top_level_field_decl is TRUE to indicate that this is the declaration of
a nonstatic data member of a class.  top_level_param_decl is TRUE to
indicate that this is a a top-level declarator in a function parameter
declaration.
*/
{
  a_targ_size_t           num_of_elements;
  a_constant              constant;
  a_boolean               is_constant_bound = FALSE;
  a_boolean               err = FALSE;
  a_boolean               has_vla_asterisk = FALSE;
  a_boolean               template_dependent_bound = FALSE;
  a_source_position       start_pos, size_pos;
  an_expr_node_ptr        dim_expr = NULL;
  a_boolean               static_seen = FALSE;
  a_type_qualifier_set    qualifiers = TQ_NONE;

  db_enter(3, "array_declarator");
  copy_source_position(pos_curr_token, start_pos);
  /* Pass over the initial left bracket. */
  (void)get_token();
  add_stop_token(tok_rbracket);
  if (c99_mode && top_level_param_decl && curr_token == tok_static) {
    /* In C99, "static" in an array declarator in a parameter declaration
       indicates that the actual argument must have at least as many elements
       as the declared size of the array. */
    static_seen = TRUE;
    (void)get_token();
  }  /* if */
  /* In some modes, "restrict" is allowed inside the brackets:
       int x[restrict 5]
     or
       int y[restrict]
     This is allowed only for formal parameter declarations, and
     indicates that the pointer type to which the array type decays
     is restrict-qualified (e.g., "restrict pointer to int" in
     the first example above.  In C99, cv-qualifiers are also allowed
     inside the brackets. */
  if (is_type_qualifier_token(curr_token)) {
    a_source_position     qualifier_pos;

    qualifier_pos = pos_curr_token;
    qualifiers = collect_type_qualifiers(decl_pos_block);
    if (top_level_param_decl) {
      /* This is a top-level declaration of a function parameter type. */
      /* Only C99 mode allows cv-qualifiers.  restrict is allowed in
         any mode where the keyword is enabled. */
      if (!c99_mode && ((qualifiers & TQ_RESTRICT) != qualifiers)) {
        pos_error(ec_type_qualifier_not_allowed, &qualifier_pos);
        qualifiers &= TQ_RESTRICT;
      }  /* if */
    } else {
      /* This is not a top-level declarator for a parameter, so "restrict"
         and cv-qualifiers are not allowed.  Issue an error. */
      pos_error((qualifiers == TQ_RESTRICT) ?
                   ec_restrict_not_allowed : ec_type_qualifier_not_allowed,
                &qualifier_pos);
      qualifiers = TQ_NONE;
    }  /* if */
    if (c99_mode && top_level_param_decl && curr_token == tok_static &&
        !static_seen) {
      /* In C99, "static" can appear after cv-qualifiers as well. */
      static_seen = TRUE;
      (void)get_token();
    }  /* if */
  }  /* if */
  size_pos = pos_curr_token;
  if (curr_token == tok_rbracket && !static_seen) {
    /* Empty brackets, indicating an incomplete array type. */
    num_of_elements = 0;
  } else if (vla_enabled && curr_token == tok_star &&
             next_token() == tok_rbracket) {
    /* [*] syntax for a VLA in a prototype. */
    if (vla_asterisk_allowed && !static_seen) {
      has_vla_asterisk = TRUE;
    } else {
      error(ec_vla_with_unspecified_bound_not_allowed);
      err = TRUE;
    }  /* if */
    /* Pass over the asterisk. */
    (void)get_token();
  } else {
    /* Scan the array size. */
    if (nonconstant_dimension_allowed || vla_allowed) {
      scan_nonconstant_dimension_expression(vla_allowed, &is_constant_bound,
                                            &dim_expr, &constant);
      check_assertion(is_constant_bound == (dim_expr == NULL));
    } else {
      scan_fs_integral_constant_expression(&constant);
      is_constant_bound = TRUE;
    }  /* if */
    if (dim_expr == NULL) {
      switch (constant.kind) {
        case ck_integer:
          /* Array size must be greater than zero. */
          if (sign_of_integer_constant(&constant) > 0) {
            num_of_elements =
                        unsigned_value_of_integer_constant(&constant, &err);
            if (err) error(ec_array_size_too_large);
          } else if (((microsoft_mode && top_level_field_decl) || gcc_mode) &&
                     sign_of_integer_constant(&constant) == 0) {
            /* In Microsoft C mode a field may be a zero-sized array type if
               it is the last field of the struct.  Thus
                 struct S { int a,b,c[0]; }
               is allowed, and "c[0]" has the same semantics as "c[]".  Also
               allowed in Microsoft C++ mode, as long as the class is an
               "aggregate".  Note: last-field restriction and the aggregate
               restriction in C++ are enforced in scan_class_definition.
               GNU C also allows zero-sized array types: they have the same
               semantics as the "[]" notation for the last field of a
               struct, but different semantics in other contexts.  Our
               emulation currently just treats it identically to "[]". */
            num_of_elements = 0;
          } else {
            error(ec_array_size_must_be_positive);
            err = TRUE;
          }  /* if */
          break;
        case ck_template_param:
          /* Template-dependent bound.  Handled below. */
          template_dependent_bound = TRUE;
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
    (*new_type_ptr)->variant.array.is_static = static_seen;
    (*new_type_ptr)->variant.array.qualifiers = qualifiers;
    /* Store the array size. */
    if (has_vla_asterisk) {
      /* [*] case (C only).  Since the size of the VLA is not specified,
         there is no need to allocate a_vla_dimension. */
      (*new_type_ptr)->variant.array.is_vla = TRUE;
      (*new_type_ptr)->variant.array.is_variable_size_array = TRUE;
    } else if (dim_expr != NULL) {
      /* Expression case. */
      (*new_type_ptr)->variant.array.is_variable_size_array = TRUE;
      if (vla_allowed) {
        /* VLA case (C only). */
        (*new_type_ptr)->variant.array.is_vla = TRUE;
        /* A VLA dimension entry will be created to record the array
           dimension expression. */
        if (scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
          /* For a VLA in a function parameter declaration, generation of the
             stmk_set_vla_size statement is delayed until it is determined
             that the parameter is part of a function definition, not a
             declaration.  */
          add_vla_fixup_entry(*new_type_ptr, dim_expr, (a_symbol_ptr)NULL,
                              &size_pos);
        } else {
          /* Create a VLA dimension entry to record the expression. */
          a_vla_dimension_ptr  vdp;

          vdp = make_vla_dimension(*new_type_ptr, dim_expr,
                                   /*in_prototype_scope=*/FALSE,
                                   &size_pos);
          if (in_expression_context()) {
            /* Don't put out an stmk_set_vla_size statement if this is an
               expression context (e.g., a sizeof or cast). */
          } else {
            /* Generate an stmk_set_vla_size statement for the VLA to indicate
               when (at runtime) the VLA dimension expression is to be
               evaluated to fix the size of the array. */
            set_vla_size_statement(vdp, &start_pos);
          }  /* if */
        }  /* if */
      } else {
        (*new_type_ptr)->variant.array.variant.element_count_expr = dim_expr;
      }  /* if */
    } else {
      /* Not an expression or VLA bound, so either [] or a constant bound. */
      a_constant_ptr         il_constant = NULL;
      a_memory_region_number region_to_switch_back_to;

      /* Make sure any constants are allocated in the file scope memory
         region, because they will be pointed to by the array type, which
         is in the file scope memory region. */
      switch_to_file_scope_region(&region_to_switch_back_to);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (is_constant_bound) {
        /* Save the constant for the bound, which has an attached
           expression. */
        il_constant = alloc_shareable_constant(&constant);
        (*new_type_ptr)->variant.array.bound_constant = il_constant;
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      if (template_dependent_bound) {
        /* Template-dependent bound (constant but not a known value). */
        if (il_constant == NULL) {
          il_constant = alloc_shareable_constant(&constant);
        }  /* if */
        (*new_type_ptr)->variant.array.variant.element_count_constant =
                                                                  il_constant;
        (*new_type_ptr)->variant.array.is_template_dependent_size_array = TRUE;
      } else {
        /* Normal constant bound. */
        (*new_type_ptr)->variant.array.variant.number_of_elements =
                                                              num_of_elements;
      }  /* if */
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
    /* The size of the array (in bytes) is updated in 
       add_to_derived_type_list. */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for closing right bracket. */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* array_declarator */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void scan_microsoft_calling_convention(a_calling_convention *call_conv)
/*
Scan a list of Microsoft calling conventions (__cdecl, __fastcall, __stdcall).
Actually, only one calling convention may be specified, but the
same specifier may appear more than once.  It can be assumed that
is_microsoft_calling_convention is TRUE on entry.  *call_conv on entry
has any calling convention previously scanned, or is cc_default if there
was no previous calling convention.  On exit it is set to the calling
convention scanned on this call.
*/
{
  set_err_pos_to_curr_token();
  do {
    a_calling_convention new_call_conv;
    switch (curr_token) {
      case tok_cdecl:
        new_call_conv = (a_calling_convention)cc_cdecl;
        break;
      case tok_fastcall:
        new_call_conv = (a_calling_convention)cc_fastcall;
        break;
      case tok_stdcall:
        new_call_conv = (a_calling_convention)cc_stdcall;
        break;
      default: unexpected_condition();
    }  /* switch */
    if (*call_conv != (a_calling_convention)cc_default) {
      /* A calling convention was specified. */
      if (*call_conv != new_call_conv) {
        /* The new calling convention does not agree with the old one. */
        error(ec_conflicting_calling_conventions);
      } else {
        /* The new and old calling conventions are the same. */
        warning(ec_dupl_calling_convention);
      }  /* if */
    }  /* if */
    *call_conv = new_call_conv;
    (void)get_token();
  } while (is_microsoft_calling_convention());
}  /* scan_microsoft_calling_convention */


static
void update_calling_convention(a_type_ptr	     *type,
			       a_call_conv_descr_ptr p_calling_convention,
                               a_source_position    *decl_pos)
/*
Determine whether the "type" specifies a type for which a calling
convention may be specified.  If so, update the calling convention
information.  Otherwise, determine whether the calling convention
information should be ignored or if an error should be issued.
*/
{
  a_calling_convention           calling_convention;
  a_boolean                      discard = FALSE;
  a_routine_type_supplement_ptr  rtsp;

  calling_convention = p_calling_convention->call_conv;
  if (*type == NULL) {
    /* Null type -- the calling convention will be discarded. */
    discard = TRUE;
  } else if (calling_convention != (a_calling_convention)cc_default) {
    if (!is_function_type(*type)) {
      /* A calling convention on a non-function type is ignored. */
      pos_remark(ec_calling_convention_ignored_for_type, decl_pos);
    } else {
      /* A calling convention applied to a function type. */
      if (is_qualified_type(*type)) {
        /* The type involves qualifiers on top of a routine type.  This is an
           unusual situation that can only occur when a type qualifier is
           applied to a typedef that points to a routine type.  If a calling
           convention were allowed to be declared on top of that, it would
           cause the underlying routine type to be modified; but that would
           affect the meaning of the typedef that points to it.  Another
           approach would be to copy the routine type, add the calling
           convention to the copy, and then reapply the qualifier directly to
           the copy, but this has implementation difficulties: among other
           things, without a typedef in the resulting type tree the type can't
           be represented outside the IL (e.g., in diagnostics).  Here's an
           example of what is disallowed:
             typedef void F(int);
             typedef const F CF;
             extern CF __stdcall f;     // __stdcall is not allowed here
           This should be a very rarely encountered limitation, since type
           qualifiers are uncommon on routine types to begin with. */
        /* An error is issued, since to just to ignore the declaration (even
           with a warning) could give the user a false impression. */
        pos_error(ec_calling_convention_not_allowed, decl_pos);
      } else {
        a_type_ptr  tp = *type;
        a_boolean   any_typedefs = FALSE;

        /* Skip past any typerefs.  See if any of them are typedefs. */
        while (tp->kind == (a_type_kind)tk_typeref) {
          any_typedefs |= (int)(typeref_is_typedef(tp));
          tp = tp->variant.typeref.type;
        }  /* while */
        check_assertion(tp->kind == (a_type_kind)tk_routine);
        rtsp = tp->variant.routine.extra_info;
        if (rtsp->has_ellipsis) {
          /* Calling convention for functions with variable argument lists
             is always __cdecl.  Whether or not __cdecl was explicitly
             specified, add it to the type. */
          rtsp->calling_convention = (a_calling_convention)cc_cdecl;
          if (calling_convention != (a_calling_convention)cc_cdecl) {
            /* Issue a diagnostic to indicate that whatever was explicitly
               specified is being ignored. */
            discard = TRUE;
          }  /* if */
        } else if (rtsp->calling_convention != calling_convention) {
          /* The underlying routine type needs to be updated. */
          if (any_typedefs) {
            /* Copy the routine type, since it's about to be modified and we
               don't want to change the meaning of the typedef.  But that
               means *type has to be adjusted. */
            tp = copy_routine_type_with_param_types(tp,
                                                   /*copy_default_args=*/TRUE);
            *type = tp;
            rtsp = tp->variant.routine.extra_info;
          }  /* if */
          rtsp->calling_convention = calling_convention;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (discard) {
    /* Issue a remark indicating that the calling convention has no
       effect. */
    pos_remark(ec_calling_convention_ignored, &p_calling_convention->position);
  }  /* if */
  /* Whether or not we were able to apply the calling convention,
     reset it so that the caller does not attempt to reuse it later. */
  clear_call_conv_descr(p_calling_convention);
}  /* update_calling_convention */


static a_variable_ptr scan_based_modifier(void)
/*
Scan the Microsoft __based modifier.  The syntax is

	__based(identifier)

The identifier must name a variable with pointer type.  Return a
pointer to the variable.  If the identifier is undefined, or
is not a variable with pointer type, return NULL.
*/
{
  a_variable_ptr	var = NULL;

  check_assertion(curr_token == tok_based);
  /* Bypass the __based token. */
  (void)get_token();
  if (required_token(tok_lparen, ec_exp_lparen)) {
    add_stop_token(tok_rparen);
    if (!is_generalized_identifier_start(GID_NO_OPTIONS)) {
      syntax_error(ec_exp_identifier);
      /* Flush tokens to the right paren. */
      flush_tokens();
    } else {
      /* Call an expression routine to scan the identifier.  The routine
         will return NULL if an error occurred while scanning the variable. */
      var = based_variable();
    }  /* if */
    remove_stop_token(tok_rparen);
    /* Bypass the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  return var;
}  /* scan_based_modifier */


static void issue_invalid_based_error(a_source_position *pos)
/*
Issue an error that a __based modifier is not allowed in the
indicated position.
*/
{
  pos_error(ec_based_not_allowed_here, pos);
}  /* issue_invalid_based_error */

/*
Macro that tests whether a based symbol is present and, if so, issues
an error and resets the symbol.  This macro expands to nothing when
Microsoft extensions are not allowed.
*/
#define based_not_allowed_here(var, pos)			      \
  { if ((var) != NULL) issue_invalid_based_error(&pos); var = NULL; }

#else  /* !MICROSOFT_EXTENSIONS_ALLOWED */

/*
Expands to nothing when Microsoft extensions are not being used.
*/
#define based_not_allowed_here(sym, pos)  /* Nothing */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* var is used only for Microsoft extensions. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
static a_type_ptr make_possibly_based_pointer_type(a_type_ptr     tp,
                                                   a_variable_ptr *var)
/*
Given a type "tp", make a pointer to that type.  In Microsoft mode, if
var is not NULL, create a based pointer using "var" as the base.
Clear the pointer stored in "var" if it is used.
*/
{
  a_type_ptr new_tp;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (*var != NULL) {
    check_assertion(microsoft_mode);
    new_tp = make_based_pointer_type(tp, *var);
    *var = NULL;
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    new_tp = make_pointer_type(tp);
  }  /* if */

  return new_tp;
}  /* make_possibly_based_pointer_type */


#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- because when MICROSOFT_EXTENSIONS_ALLOWED is FALSE,
                    call_conv, based_var, and based_pos are not used. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void collect_pointer_declarator_extended_qualifiers(
                                          a_type_qualifier_set *qualifiers,
                                          a_source_position    *qual_pos,
                                          a_call_conv_descr    *call_conv,
                                          a_variable_ptr       *based_var,
                                          a_source_position    *based_pos,
                                          a_decl_pos_block_ptr decl_pos_block)
/*
Collect a set of pointer declarator qualifiers provided as an extension
(e.g., for Microsoft compatibility).  Aside from the standard const/volatile,
support for near and far may be enabled (e.g., in Microsoft 16-bit mode),
and Microsoft mode also allows other modifiers, notably __based and calling
conventions like __cdecl.  Scan all of those, and return information about
what was scanned in *qualifiers, *call_conv, and *based_var.  If qualifiers
are scanned, *qual_pos is set to their starting position.  If a __based
qualifier is scanned, *based_pos is set to its source position.  It's
permissible for the input to contain no qualifiers. If Microsoft extended
decl specifiers, introduced by __declspec, are encountered, they are
scanned and thrown away with a warning.
*/
{
  a_type_qualifier_set new_qualifiers, duplicates;

  *qualifiers = TQ_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  clear_call_conv_descr(call_conv);
  *based_var = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  for (;;) {
    if (is_type_qualifier() or_is_near_or_far()) {
      /* Normal qualifiers like const, and declarator-only qualifiers like
         near. */
      *qual_pos = pos_curr_token;
      new_qualifiers = collect_type_qualifiers(decl_pos_block);
      duplicates = (new_qualifiers & *qualifiers);
#if NEAR_AND_FAR_ALLOWED
      if (near_and_far_enabled()) {
        if ((new_qualifiers & TQ_NEAR) && (*qualifiers & TQ_FAR )) {
          /* Incompatible near and far specifications. */
          error(ec_mem_attrib_incompatible);
          new_qualifiers &= ~TQ_NEAR;
          duplicates &= ~TQ_NEAR;
        }  /* if */
        if ((new_qualifiers & TQ_FAR ) && (*qualifiers & TQ_NEAR)) {
          /* Incompatible near and far specifications. */
          error(ec_mem_attrib_incompatible);
          new_qualifiers &= ~TQ_FAR;
          duplicates &= ~TQ_FAR;
        }  /* if */
        /* Check for repetition of "near" or "far".  The Microsoft compiler
           gives only a warning for these cases, so we do too. */
        if (duplicates & (TQ_NEAR | TQ_FAR)) {
          warning(ec_dupl_mem_attrib);
          duplicates &= ~(TQ_NEAR | TQ_FAR);
        }  /* if */
      }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
      /* Check for duplicates other than "near" and "far". */
      if (duplicates != TQ_NONE) {
        /* The Microsoft compiler gives only a warning for duplicates, so
           we do too. */
        warning(ec_dupl_type_qualifier);
      }  /* if */
      *qualifiers |= new_qualifiers;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode) {
      if (is_microsoft_calling_convention()) {
        /* Calling conventions like __cdecl. */
        call_conv->position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->declarator_range.end = end_pos_curr_token;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        scan_microsoft_calling_convention(&call_conv->call_conv);
      } else if (curr_token == tok_based) {
        /* __based. */
        if (*based_var != NULL) {
          /* __based appears more than once. */
          error(ec_dupl_type_qualifier);
        }  /* if */
        *based_pos = pos_curr_token;
        *based_var = scan_based_modifier();
      } else if (curr_token == tok_declspec) {
        /* Scan the decl-modifiers.  The Microsoft compiler appears to accept
           and ignore __declspec declarations that appear during declarator
           processing -- there is no evidence that the decl-modifiers are ever
           actually applied to the function or variable being declared. */
        scan_and_discard_extended_decl_modifiers();
      } else if (curr_token == tok_mutable) {
        /* The Microsoft compiler appears to accept and ignore "mutable"
           during declarator processing.  Issue a warning and continue. */
        warning(ec_mutable_not_allowed);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->declarator_range.end = end_pos_curr_token;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        (void)get_token();
      } else {
        /* Something else; exit the loop. */
        break;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      break;
    }  /* if */
  }  /* for */
}  /* collect_pointer_declarator_extended_qualifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if NEAR_AND_FAR_ALLOWED

static void check_for_addition_of_incompatible_qualifiers(
                                              a_type_ptr           type,
                                              a_type_qualifier_set *qualifiers,
                                              a_source_position    *pos)
/*
The memory attribute qualifiers in the set *qualifiers are about to be added
to the indicated type.  If there is some conflict between the new qualifiers
and the existing ones (explicit and implied), issue an error (at position
*pos) and remove the incompatible qualifiers from *qualifiers.
*/
{
  a_type_qualifier_set new_qualifiers = *qualifiers;

  if (new_qualifiers & (TQ_NEAR | TQ_FAR)) {
    /* Adding a memory attribute.  See if there is a conflicting one
       already. */
    a_type_qualifier_set old_qualifiers = get_original_type_qualifiers(type);
    if (((old_qualifiers & TQ_NEAR) && (new_qualifiers & TQ_FAR)) ||
        ((new_qualifiers & TQ_NEAR) && (old_qualifiers & TQ_FAR))) {
      /* Incompatible memory attributes. */
      pos_error(ec_mem_attrib_incompatible, pos);
      new_qualifiers &= ~(TQ_NEAR | TQ_FAR);
      *qualifiers = new_qualifiers;
    }  /* if */
  }  /* if */
}  /* check_for_addition_of_incompatible_qualifiers */

#endif /* NEAR_AND_FAR_ALLOWED */

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* <-- left_calling_convention and unbound_calling_convention
                    are only used when Microsoft extensions are allowed.
                    Moreover, left_qualifiers and unbound_qualifiers are used
                    only if either Microsoft extensions or near/far are
                    allowed. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
a_type_ptr pointer_declarator(
                      a_type_ptr            specifiers_type,
                      a_boolean   	    reference_allowed,
                      a_call_conv_descr_ptr left_calling_convention,
                      a_call_conv_descr_ptr unbound_calling_convention,
                      a_type_qualifier_set  *left_qualifiers,
                      a_type_qualifier_set  *unbound_qualifiers,
                      a_decl_pos_block_ptr  decl_pos_block)
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

Note that this routine actually scans a sequence of pointer declarators.
The pointer type modifiers are placed on top of the type passed in as
specifiers_type, and a pointer to the complete type is returned.
specifiers_type is NULL for a nested declarator (one enclosed in
parentheses); in that case the pointer type modifiers are built up
but nothing is attached to the bottom-most modifier.

In Microsoft mode, the Microsoft __cdecl, __stdcall, and __fastcall are
recognized as calling conventions.  The handling of calling conventions
is intended to match the behavior of the Microsoft 32-bit C/C++ compiler.
Calling conventions are allowed on function types and pointer to
function types.  They are permitted on object declarations, but have
no meaning.  They are not allowed on pointers to objects or on
references.

left_calling_convention and unbound_calling_convention are pointers
to calling conventions.  The values of these calling conventions
are returned by this routine.  If the pointer declarator looks like

	__cdecl * __cdecl * __cdecl

the first calling convention is returned in *left_calling_convention
(but only if specifiers_type is NULL; otherwise, it is applied directly
to the specifiers type); the middle one is discarded (by applying it
to the pointer type); and the last one (not followed by a pointer
operator) is returned in *unbound_calling_convention.  If
unbound_calling_convention is NULL, an unbound calling convention
is just thrown away.

Also in Microsoft mode, type qualifiers can appear at the beginning of
the declarator, e.g.,

  int i, const j, const *k;  // Declares j as "const int", k as "int *"

This routine will see them only at the beginning of a declarator that
is not immediately next to its specifiers list, as above, because otherwise
the qualifiers are processed as part of the specifiers.

Type qualifiers get processing similar to that for calling conventions.
If a pointer declarator looks like

	far * far * far

*left_qualifiers is used to return the first qualifier (but only if
specifiers_type is NULL; otherwise, the qualifier is applied directly
to the specifiers type); the middle qualifier is handled internally
by applying it to the pointer type; and the last (unbound) qualifier
is returned in *unbound_qualifiers.  If unbound_qualifiers is NULL,
unbound qualifiers are just thrown away.

Microsoft extended decl modifiers are also scanned, but they are ignored
(which is what the Microsoft compiler itself appears to do).
*/
{
  a_type_ptr     		complete_type = specifiers_type;
  a_boolean      		err = FALSE;
  a_type_qualifier_set		qualifiers;
  a_type_ptr     		class_type;
  a_type_ptr     		rout_type;
  a_variable_ptr		based_var = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  a_type_qualifier_set		pending_qualifiers = TQ_NONE;
  a_source_position		pending_qualifiers_pos;
  a_call_conv_descr		ccd;
  a_source_position		based_pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

  db_enter(3, "pointer_declarator");
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (microsoft_mode or_near_and_far_enabled()) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Clear parameters used to return left/unbound qualifiers. */
    if (left_calling_convention != NULL) {
      clear_call_conv_descr(left_calling_convention);
    }  /* if */
    if (unbound_calling_convention != NULL) {
      clear_call_conv_descr(unbound_calling_convention);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (left_qualifiers != NULL) *left_qualifiers = TQ_NONE;
    if (unbound_qualifiers != NULL) *unbound_qualifiers = TQ_NONE;
    /* Scan qualifiers that precede the first pointer or reference, e.g.,
         int far *p;
    */
    collect_pointer_declarator_extended_qualifiers(&pending_qualifiers,
                                                   &pending_qualifiers_pos,
                                                   &ccd,
                                                   &based_var,
                                                   &based_pos,
                                                   decl_pos_block);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  /* Loop while there are pointer declarators. */
  for (;;) {
    /* See if there is a pointer declarator. */
    a_boolean another_pointer_declarator = FALSE;
    a_boolean ptr_to_member_case = FALSE;
    if ((curr_token == tok_star ||
         (reference_allowed && curr_token == tok_ampersand))) {
      /* A pointer "*" or reference "&". */
      another_pointer_declarator = TRUE;
    } else if (C_dialect == C_dialect_cplusplus &&
               is_ptr_to_member_declarator_start()) {
      /* A pointer-to-member "Name::*". */
      another_pointer_declarator = TRUE;
      ptr_to_member_case = TRUE;
    }  /* if */
    /* Exit the loop if there is not another pointer declarator. */
    if (!another_pointer_declarator) break;
    set_err_pos_to_curr_token();
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (pending_qualifiers != TQ_NONE) {
      /* Apply pending qualifiers to the complete type being built up, now
         that we know those are not unbound qualifiers. */
#if NEAR_AND_FAR_ALLOWED
      if ((pending_qualifiers & ~(TQ_NEAR | TQ_FAR)) != TQ_NONE) {
        /* Drop qualifiers like const/volatile because Microsoft drops
           them:
             int p, const *q;
           q has type "int *", not "const int *".  The only qualifiers
           like this that can be dropped are from the qualifiers collected
           above before the first iteration of the loop. */
        pos_warning(ec_type_qualifier_ignored, &pending_qualifiers_pos);
        pending_qualifiers &= (TQ_NEAR | TQ_FAR);
      }  /* if */
      if (pending_qualifiers != TQ_NONE) {
        /* Some qualifiers like near were specified.  Apply them to
           the complete type or (at the beginning of a nested declarator)
           return them to the caller.  Note that qualifiers like const
           do not get here (they're handled at the end of the loop). */
        if (complete_type != NULL) {
          check_for_addition_of_incompatible_qualifiers(complete_type,
                                                        &pending_qualifiers,
                                                      &pending_qualifiers_pos);
          complete_type = make_qualified_type(complete_type,
                                              pending_qualifiers);
        } else {
          /* Return left-most qualifiers to the caller. */
          *left_qualifiers = pending_qualifiers;
        }  /* if */
        pending_qualifiers = TQ_NONE;
      }  /* if */
#else /* !NEAR_AND_FAR_ALLOWED */
      pos_warning(ec_type_qualifier_ignored, &pending_qualifiers_pos);
      pending_qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (ccd.call_conv != (a_calling_convention)cc_default) {
        /* A calling convention was specified.  Apply it to the complete
           type or (at the beginning of a nested declaration) return it to
           the caller. */
        if (complete_type != NULL) {
          update_calling_convention(&complete_type, &ccd, &ccd.position);
        } else {
          /* Return left-most calling convention to the caller. */
          *left_calling_convention = ccd;
          clear_call_conv_descr(&ccd);
        }  /* if */
      }  /* if */
      /* __based is handled when building the pointer type. */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    if (!ptr_to_member_case) {
      /* Pointer ("*") or reference ("&") case. */
      /* Add a pointer type to the top of the existing type.  Note that this
         works out right.  For example, if one has

           int * const * volatile i;

         the proper type for i is "volatile pointer to const pointer to int".
         In the loop, the type will be built up from "int" to
         "const pointer to int" to "volatile pointer to const pointer to int"
         on successive iterations. */
      if (complete_type != NULL) {
        /* Normal case -- the specifiers type is given, and the pointer or
           reference type can be attached directly to it.  (Or, this is a
           pointer to a pointer type or a reference to a pointer type). */
        a_type_ptr    temp_type;
        a_symbol_ptr  sym;
        a_boolean     is_member_function_typedef = FALSE;

        temp_type = skip_typerefs(complete_type);
        if (temp_type != complete_type) {
          if (any_cfront_mode()) {
            /* Check for a special form of member function typedef that is
               an extension in cfront mode. */
            is_member_function_typedef =
                  is_cfront_member_function_typedef(complete_type, &rout_type,
                                                    &class_type, &sym);
          } else if (typeref_is_typedef(complete_type) &&
                     is_function_type(temp_type)) {
            a_routine_type_supplement_ptr  rtsp =
                                        temp_type->variant.routine.extra_info;
            if (rtsp->this_class == NULL && rtsp->qualifiers != TQ_NONE) {
              /* Catch the following:
                    typedef void f() const;  typedef F *PF;
                 Qualified function types are only allowed to declare members,
                 pointer-to-members and synonym typedefs. */
              error(ec_ptr_or_ref_to_qualified_function_type);
            }  /* if */
          }  /* if */
        }  /* if */
        if (curr_token == tok_star) {
          /* "*" for pointer. */
          if (is_member_function_typedef) {
            /* This is a proper use of a cfront member function typedef type
               -- to form a pointer-to-member type.  Do the transformation. */
            complete_type = ptr_to_member_type(rout_type, class_type);
          } else {
            if (is_reference_type(temp_type)) {
              /* Type "pointer to reference to anything" is illegal. */
              error(ec_pointer_to_reference);
              err = TRUE;
            }  /* if */
            /* Make the pointer type. */
            complete_type = make_possibly_based_pointer_type
                                   (err ? error_type() : complete_type,
                                    &based_var);
          }  /* if */
        } else {
          /* "&" for reference. */
          /* Make sure this was not preceded by __based. */
          based_not_allowed_here(based_var, based_pos);
          if (is_reference_type(temp_type)) {
            /* Type "reference to reference" is illegal. */
            error(ec_reference_to_reference);
            err = TRUE;
          } else if (is_void_type(temp_type)) {
            /* Type "reference to void" is illegal. */
            error(ec_reference_to_void);
            err = TRUE;
          } else if (is_member_function_typedef) {
            /* A cfront member function typedef type can only be used in
               forming a pointer-to-member type. */
            sym_error(ec_bad_use_of_member_function_typedef, sym);
            err = TRUE;
          }  /* if */
          /* Make the reference type. */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (based_var != NULL) {
          /* If the pointer operator was preceded by a __based
             modifier, update the pointer type with the variable
             used in the __based modifier. */
          complete_type->variant.pointer.base_variable = based_var;
          based_var = NULL;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    } else {
      /* Pointer-to-member declarator. */
      /* Issue an error if this was preceded by __based. */
      based_not_allowed_here(based_var, based_pos);
      /* Upon return from is_ptr_to_member_declarator_start the current
         token is tok_ptr_to_member. */
      class_type = qualifier_class_type(locator_for_curr_id);
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
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      decl_pos_block->declarator_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Advance past the "*", "&", or "Name::*". */
    (void)get_token();
    /* Scan any qualifiers following the pointer declarator, e.g.,
         int * const x;
    */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (microsoft_mode or_near_and_far_enabled()) {
      /* Microsoft mode allows several kinds of qualifiers. */
      collect_pointer_declarator_extended_qualifiers(&qualifiers,
                                                     &pending_qualifiers_pos,
                                                     &ccd,
                                                     &based_var,
                                                     &based_pos,
                                                     decl_pos_block);
      /* Break the qualifiers into those like const that are handled
         immediately and those like near that stay pending into the next
         iteration of the loop. */
#if NEAR_AND_FAR_ALLOWED
      pending_qualifiers = (qualifiers & (TQ_NEAR | TQ_FAR));
      qualifiers -= pending_qualifiers;
#endif /* NEAR_AND_FAR_ALLOWED */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    /* Do not add code here. */
    { /* Just look for type qualifiers. */
      qualifiers = TQ_NONE;
      if (is_type_qualifier()) {
        qualifiers = collect_type_qualifiers(decl_pos_block);
      }  /* if */
    }
    if (qualifiers != TQ_NONE) {
      /* Some qualifiers were specified. */
      /* Check for invalid use of the restrict qualifier. */
      a_type_qualifier_set restrict_bit = (qualifiers & TQ_RESTRICT);
      if (restrict_bit) {
        /* Remove the restrict bit from the set to allow easier testing
           of qualifiers on references below (restrict is allowed). */
        qualifiers &= ~TQ_RESTRICT;
        if (!restrict_qualifier_is_allowed(complete_type, &error_position)) {
          /* Restrict is not allowed here.  The diagnostic has been issued
             already.  Turn off the restrict bit. */
          restrict_bit = 0;
        }  /* if */
      }  /* if */
      /* Check for using qualifiers on a reference type.  The restrict
         bit has been removed if it was set, which is good because it is okay
         to put restrict on a reference. */
      if (qualifiers != TQ_NONE && is_reference_type(complete_type)) {
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_qualified_reference_type);
        qualifiers = TQ_NONE;
      }  /* if */
      /* Restore the restrict bit if it was on. */
      qualifiers |= restrict_bit;
      /* Add the qualifiers to the complete type being built up. */
      complete_type = make_qualified_type(complete_type, qualifiers);
    }  /* if */
    /* Keep looping as long as there are pointer declarators. */
  }  /* for */
#if DEBUG
  if (debug_level >= 4) {
    if (complete_type != specifiers_type) {
      fputs("pointer/reference type: ", f_debug);
      db_type(complete_type);
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  /* Return unbound qualifiers to the caller. */
  if (pending_qualifiers != TQ_NONE) {
    if (microsoft_mode && microsoft_version < 1000) {
      /* Case like
           int i, const j;
         The type qualifiers are ignored in later versions of the Microsoft
         compiler, but were applied in MSVC++ 2.0. */
#if NEAR_AND_FAR_ALLOWED
    } else if ((pending_qualifiers & ~(TQ_NEAR | TQ_FAR)) != TQ_NONE) {
      /* A qualifier other than near/far.  It's ignored -- strip it out of
         the bit vector and issue a warning. */
      pos_warning(ec_type_qualifier_ignored, &pending_qualifiers_pos);
      pending_qualifiers &= (TQ_NEAR | TQ_FAR);
#else /* !NEAR_AND_FAR_ALLOWED */
    } else {
      pos_warning(ec_type_qualifier_ignored, &pending_qualifiers_pos);
      pending_qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
    }  /* if */
    if (pending_qualifiers != TQ_NONE) {
      /* If there are still qualifiers (because of MSVC++ 2.0 compatibility
         and/or because near/far is present), return them to the caller if
         appropriate, or else issue a warning that they're being ignored. */
      if (unbound_qualifiers != NULL) {
        *unbound_qualifiers = pending_qualifiers;
      } else {
        /* Can't be returned to the caller, so put out a warning. */
        pos_warning(near_and_far_enabled() ?
                      ec_mem_attrib_ignored : ec_type_qualifier_ignored,
                    &pending_qualifiers_pos);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (ccd.call_conv != (a_calling_convention)cc_default) {
      /* Calling convention like __cdecl. */
      if (unbound_calling_convention != NULL) {
        *unbound_calling_convention = ccd;
      } else {
        /* Discard an unbound calling convention. */
        pos_warning(ec_calling_convention_ignored, &ccd.position);
      }  /* if */
    }  /* if */
    if (based_var != NULL) {
      /* A __based modifier was present that was not followed by a
         pointer operator.  Issue a warning that the modifier is
         being discarded. */
      pos_warning(ec_based_not_followed_by_star, &based_pos);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_exit();
  return complete_type;
}  /* pointer_declarator */


static a_boolean is_microsoft_static_operator(an_opname_kind opname)
/*
Microsoft compilers allowed most operators to be declared as to be declared
static.  This function returns FALSE if and only if the operator kind opname
is an exception to that rule, or if microsoft bugs mode is disabled.
*/
{
  return microsoft_bugs && opname != (an_opname_kind)onk_assign &&
                           opname != (an_opname_kind)onk_function_call &&
                           opname != (an_opname_kind)onk_subscript &&
                           opname != (an_opname_kind)onk_arrow;
}  /* is_microsoft_static_operator */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void scan_real_declarator_id(
                          a_decl_flag_set   input_flags,
                          a_decl_flag_set   *output_flags,
                          a_symbol_locator  *locator,
                          a_boolean         *is_constructor,
                          a_boolean         *is_destructor,
                          a_boolean         *parenthesized_initializer_allowed,
                          a_type_ptr        *p_member_parent_type,
                          a_decl_pos_block  *decl_pos_block)
/*
This routine is called by declarator for real declarators; it scans the name
that is specified.  The current token is the beginning of the name (usually
but not always an identifier).  input_flags is the set of flags passed in to
declarator, and *output_flags is the set of flags that will be returned to
declarator's caller.  *locator is returned with the locator for the name,
*p_member_parent_type is the class type when this is a qualified name,
*is_constructor or *is_destructor is returned TRUE when the name is a
constructor or destructor name, and *parenthesized_initializer_allowed is set
to FALSE if the entity being declared is not initializable.
*/
{
  a_source_position         declarator_pos;
  a_boolean                 err;
  an_identifier_options_set options;
  a_symbol_ptr              sym;
  a_namespace_ptr           nsp;
  a_boolean		    is_in_class_specialization = FALSE;
  a_boolean		    is_specialization_or_instantiation;
  a_boolean		    explicit_template_args_allowed = FALSE;

  db_enter(3, "scan_real_declarator_id");
  declarator_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->identifier_range.start = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Process the identifier.  This is done if we are at the beginning of a
     qualified name.  A special test is done to exclude a destructor name
     that is not part of a qualified name -- this case is handled separately
     below.  A destructor that is not part of a qualified name (according to
     the locator) can appear to be an identifier under some circumstances.
     For example, inside the definition of class A, the destructor "A::~A"
     will be coalesced by is_generalized_identifier_start.  The qualifier
     will then be discarded by simplify_curr_class_qualified_name resulting
     in an unqualified destructor that has already been coalesced. */
  is_specialization_or_instantiation =
    (input_flags & (DI_IS_SPECIALIZATION | DI_IS_EXPLICIT_INSTANTIATION)) != 0;
  options = GID_DTOR_RECOGNIZED;
  if (!(input_flags & DI_QUALIFIED_NAME_ALLOWED)) {
    options |= GID_DISALLOW_QUALIFIED_NAME | GID_DISALLOW_GLOBAL_QUALIFIER;
  }  /* if */
  if (input_flags & DI_IS_TEMPLATE_DECLARATION) {
    options |= GID_IS_TEMPLATE_DECLARATION;
    if (input_flags & DI_IS_SPECIALIZATION) {
      options |= GID_IS_TEMPLATE_SPECIALIZATION;
    }  /* if */
  }  /* if */
  if (is_specialization_or_instantiation ||
      ((input_flags & DI_IS_FRIEND_DECL) &&
       !(options & GID_IS_TEMPLATE_DECLARATION))) {
    explicit_template_args_allowed = TRUE;
  } else if (microsoft_mode &&
             depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    /* In Microsoft mode a function declarator can take explicit template
       argument syntax -- the declaration is taken to be a specialization.
       For example,
         template <class T> void f(T) { ... }
         void f<int>(int) { ... }
       is allowed in Microsoft mode -- the second line is equivalent to
         template <> void f<int>(int) { ... }
    */
    explicit_template_args_allowed = TRUE;
  }  /* if */
  if (*p_member_parent_type != NULL && (input_flags & DI_IS_SPECIALIZATION)) {
    /* When a member parent type is provided and the specialization flag is
       set, this must be a Microsoft mode in-class specialization. */
    is_in_class_specialization = TRUE;
  }  /* if */
  if (is_generalized_identifier_start(GID_DTOR_RECOGNIZED) &&
      (!locator_for_curr_id.is_destructor_name ||
       locator_for_curr_id.is_qualified_name)) {
    if (any_cfront_mode()) {
      /* Provide support for an exploitable cfront bug. */
      if (locator_for_curr_id.is_qualified_name &&
          qualifier_class_type(locator_for_curr_id) != NULL &&
          input_flags & DI_IS_TYPEDEF_DECLARATION) {
        /* We have a typedef declaration involving what appears to be a
           qualified name, but cfront interprets it as a kind of member
           routine type, e.g.,
               typedef void A::t(int);
                               ^---------We're here now.
           The type "t" is construed as a routine type taking an int argument
           and returning void and having an implicit this-param type of
           const-ptr-to-A.  Note that this syntax and interpretation are not
           supported in the ARM.  We allow it under cfront compatibility mode
           only. */
        check_assertion(locator_for_curr_id.specific_symbol == NULL);
        /* Force function_declarator to add an implicit-this-param pointer
           to the routine type. */
        *p_member_parent_type = qualifier_class_type(locator_for_curr_id);
        *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
        /* Clear the is_qualified_name flag in the locator, but keep the
           qualifer_class_type around, in case this is a recursive declarator
           call and the function_declarator is called at another level. */
        locator_for_curr_id.is_qualified_name = FALSE;
        /* Note that the diagnostic on this nonstandard construct is issued
           by the caller. */
      }  /* if */
    }  /* if */
    if (locator_for_curr_id.is_qualified_name) {
      if (locator_for_curr_id.is_class_member) {
        if (!(input_flags & DI_IS_FRIEND_DECL) || microsoft_bugs) {
          /* If this declaration appears in the immediate context of a class
             definition and the current token is an identifier representing
             the name of the current class, see if this is a qualified name
             and if so change it into a simple name (e.g., A::x becomes x,
             its equivalent in A's scope).  This needs to be done after the
             check for the cfront member typedef processing that is done
             above.  Ordinarily, this does not apply to the declarator of a
             friend declaration (i.e., "friend void A::f();" is an error in
             class A if f had not been declared yet), but Microsoft compilers
             perform the transformation anyway (thereby creating ::f instead
             of A::f(!!)). */
          if (simplify_curr_class_qualified_name() /* side effects */ &&
              (input_flags & DI_IS_FRIEND_DECL)) {
            /* We're emulating a Microsoft bug by ignoring the qualification
               on a friend function (possibly injecting it in the surrounding
               namespace scope).  This is likely unintended. */
            warning(ec_friend_qualification_ignored);
          }  /* if */
        }  /* if */
      } else {
        /* This must be a namespace-qualified name.  This is used when a
           namespace member is redeclared (defined) outside its namespace.
             namespace N { void f(); }
             void N::f() { ... }
           Strictly speaking, when the qualifier is used on a declarator that
           appears within that namespace, it is an error -- though usually a
           benign error:
             namespace N { void N::f(); }       // error
           (It is not benign, however, when the qualifier appears on the
           declarator of a template declaration, because the entire declarator
           is cached and rescanned during instantiations, at which point it is
           possible that the qualifier's meaning will have changed.) */
        a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
        a_boolean                is_template_decl = FALSE;

        if (ssep->kind == (a_scope_kind)sck_template_declaration) {
          /* This is a template declaration. */
          is_template_decl = TRUE;
          ssep--;
        }  /* if */
        if ((ssep->kind == (a_scope_kind)sck_namespace ||
             ssep->kind == (a_scope_kind)sck_namespace_extension) &&
            ssep->il_scope->variant.assoc_namespace ==
                        qualifier_namespace_ptr(locator_for_curr_id)) {
          /* The declarator name is qualified by the current namespace. */
          pos_diagnostic(is_template_decl ? es_error : es_discretionary_error,
                         ec_qualifier_in_namespace_member_decl,
                         &pos_curr_token);
          /* Reset the fields in the locator to make it appear as if the
             qualifier were not present. */
          clear_qualifier_from_locator(&locator_for_curr_id);
          if (is_template_decl) {
            /* This is not a benign error in a template declaration, so
               make this an error locator.  Otherwise, there are name-binding
               bugs in this sort of case:
                 namespace N {
                   template <class T> void N::f(T);
                   class N { ... }
                   void f(long);         // Problems with this specialization
                 }
            */
            set_to_named_error_locator(locator_for_curr_id);
          }  /* if */
        } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* Error has already been issued on the template declaration.
             Just skip over it here on the instantiation. */
          clear_qualifier_from_locator(&locator_for_curr_id);
        }  /* if */
      }  /* if */
    }  /* if */
    /* The declarator may be a qualified name or a normal name. */
    if (coalesce_and_lookup_qualified_name(options, ilm_normal, &err)) {
      /* See if the name is a qualified name, like "A::x" or "::j". */
      if (locator_for_curr_id.is_qualified_name) {
        *p_member_parent_type = qualifier_class_type(locator_for_curr_id);
        if (*p_member_parent_type != NULL) {
          a_boolean  reactivate_scope = FALSE;

          sym = locator_for_curr_id.specific_symbol;
          /* See if the name is the name of a member function. */
          if (sym->kind == (a_symbol_kind)sk_member_function ||
              sym->kind == (a_symbol_kind)sk_overloaded_function ||
              sym->kind == (a_symbol_kind)sk_function_template) {
            /* It is a member function.  Its parameters should be scanned
               with the original class reactivated. */
            reactivate_scope = TRUE;
            *parenthesized_initializer_allowed = FALSE;
            if (is_constructor_symbol(sym)) {
              *is_constructor = TRUE;
            } else if (is_destructor_symbol(sym)) {
              *is_destructor = TRUE;
            }  /* if */
          } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
            /* The dimensions of static data members (if any) are scanned
               with the original class reactivated. */
            reactivate_scope = TRUE;
          }  /* if */
          if (reactivate_scope) {
            /* Reactivate the scope of the parent class.  It will be
               deactivated once the entire declarator has been scanned. */
            push_class_reactivation_scope(*p_member_parent_type,
                                          /*extend_namespace=*/FALSE);
            *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
            if (any_deferred_access_checks()) {
              /* Discard any access errors that occurred while scanning
                 the name of the thing being defined. */
              discard_declarator_access_errors();
              /* Recheck any access errors that occurred while scanning
                 the specifiers or the beginning of the declarator
                 now that we know the class of the thing being declared. */
              perform_deferred_access_checks();
            }  /* if */
          }  /* if */
        } else {
          /* This must be a namespace-qualified name. */
          nsp = qualifier_namespace_ptr(locator_for_curr_id);
          if (nsp != NULL) {
            /* Push the namespace extension scope.  It will be popped when
               scanning the declarator has been completed. */
            push_namespace_reactivation_scope(nsp);
            *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* The declarator id is not qualified. */
      if (!C_mode() && scope_stack[depth_scope_stack].kind
                                    == (a_scope_kind)sck_class_struct_union) {
        /* Check if we have a constructor. Trying to find it out while
           scanning the specifiers might have failed because the scanning had
           to stop at an opening parenthesis. However, we might have
           "struct S { (S)(); }". Note that destructors aren't a problem
           because of the distinctive leading tilde. */
        if (!err && (input_flags & DI_NO_TYPE_SPECIFIERS) != 0 &&
            is_constructor_decl(scope_stack[depth_scope_stack].assoc_type)) {
          *is_constructor = TRUE;
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      decl_pos_block->identifier_range.end = end_pos_curr_token;
      decl_pos_block->declarator_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
  } else {
    if (!(input_flags & DI_IS_FRIEND_DECL)) {
      /* The call to simplify_curr_class_qualified_name is placed here so
         that it will be done at this point for all cases other than the
         normal identifier case handled above. */
      (void)simplify_curr_class_qualified_name();
    }  /* if */
    if (curr_token == tok_identifier &&
        locator_for_curr_id.is_destructor_name) {
      /* A destructor name, like "~A".  It must have the same name as
         the class currently being defined, it must be followed by a
         left paren, and the specifiers must include no type. */
      *is_destructor = TRUE;
      *parenthesized_initializer_allowed = FALSE;
      if (is_error_locator(locator_for_curr_id)) {
        /* There is some error in the destructor name. */
        set_to_error_locator(*locator);
      } else if ((input_flags & DI_IS_FRIEND_DECL) &&
                 !locator_for_curr_id.is_qualified_name) {
        error(ec_destructor_name_must_be_qualified);
        set_to_error_locator(*locator);
      } else if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
        /* "typedef ~X();" is not acceptable. */
        error(ec_bad_destructor_decl);
        set_to_error_locator(*locator);
      } else {
        a_scope_stack_entry_ptr ssep = &scope_stack[decl_scope_level];

        if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
          /* Not inside a class; destructor is not allowed. */
          error(ec_bad_destructor_decl);
          set_to_error_locator(*locator);
        } else {
          sym = (a_symbol_ptr)ssep->assoc_type->source_corresp.assoc_info;
          if (!destructor_name_matches_class_name(sym)) {
            /* The name on the destructor is not the name of the class. */
            error(ec_bad_destructor_decl);
            set_to_error_locator(*locator);
          } else {
            *locator = locator_for_curr_id;
            *p_member_parent_type = ssep->il_scope->variant.assoc_type;
          }  /* if */
        }  /* if */
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->identifier_range.end = end_pos_curr_token;
        decl_pos_block->declarator_range.end = end_pos_curr_token;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Advance past the destructor. */
      (void)get_token();
    } else {
      add_stop_token(tok_lparen);
      add_stop_token(tok_lbracket);
      set_to_error_locator(*locator);
      copy_source_position(pos_curr_token, locator->source_position);
      syntax_error(ec_exp_identifier);
      remove_stop_token(tok_lparen);
      remove_stop_token(tok_lbracket);
      *parenthesized_initializer_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->kind == (a_symbol_kind)sk_namespace) {
    /* A namespace name cannot be a declarator. */
    pos_error(ec_namespace_name_not_allowed, &declarator_pos);
    set_to_error_locator(*locator);
  }  /* if */
  if (!explicit_template_args_allowed && locator->is_template_id &&
      !is_error_locator(*locator)) {
    /* An explicit template argument list is only permitted on explicit
       specializations, explicit instantiations, and friend declarations.
       Other declarations that appear to include an explicit argument list,
       such as a destructor declaration of the form ~A<T>(), will have
       already been transformed to a form where they are no longer considered
       to be template-ids. */
    pos_error(ec_explicit_template_args_not_allowed, &declarator_pos);
    set_to_error_locator(*locator);
  }  /* if */
  if (locator->is_operator_name) {
    /* Enforce some restrictions on the declarations of overloaded
       operator functions. */
    *parenthesized_initializer_allowed = FALSE;
    if (*p_member_parent_type != NULL) {
      if (locator->specific_symbol != NULL) {
        /* This must be a redeclaration. */
      } else if (is_in_class_specialization) {
        /* A specialization declared within the class.  Suppress the following
           test for this case.  It is a kind of redeclaration. */
      } else if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
        /* "typedef int operator+" is now allowed. */
        pos_error(ec_operator_name_not_allowed,
                  &locator->source_position);
        set_to_error_locator(*locator);
      } else if (!(input_flags & DI_NONSTATIC_MEMBER) &&
                 !is_new_operator(locator->variant.opname) &&
                 !is_delete_operator(locator->variant.opname) &&
                 !is_microsoft_static_operator(locator->variant.opname)) {
        /* Most operators cannot be declared to be static members (except in
           Microsoft mode, but except for new and delete those static member
           operators can only be invoked with qualified notation. */
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
    /* A conversion function must be a nonstatic member function.  Allow
       a Microsoft in-class specialization as this should be treated
       as a redeclaration. */
    if (*p_member_parent_type == NULL ||
        (locator->specific_symbol == NULL &&
         !(input_flags & DI_NONSTATIC_MEMBER) &&
         !is_in_class_specialization)) {
      pos_error(ec_bad_conversion_function_decl,
                &locator->source_position);
      set_to_error_locator(*locator);
      /* Avoid error recovery problems later. */
      locator->is_conversion_name = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
}  /* scan_real_declarator_id */


#if !MICROSOFT_EXTENSIONS_ALLOWED || !NEAR_AND_FAR_ALLOWED
/*ARGSUSED*/  /* <-- because p_left_call_conv et al. are used only in
                     Microsoft mode, and p_left_qualifiers is used only when
                     near and far are supported. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void r_declarator(
		  a_decl_flag_set             input_flags,
                  a_decl_flag_set             *output_flags,
                  a_type_ptr                  specifiers_type,
                  a_type_ptr                  member_parent_type,
                  a_symbol_locator            *locator,
                  a_type_ptr                  *p_complete_type,
                  a_type_ptr                  *p_bottom_derived_type,
                  a_boolean                   *is_constructor,
                  a_boolean                   *is_destructor,
                  a_call_conv_descr_ptr       p_left_call_conv,
                  a_call_conv_descr_ptr       p_unbound_call_conv,
                  a_type_qualifier_set        *p_left_qualifiers,
                  a_type_qualifier_set        *p_unbound_qualifiers,
                  a_source_sequence_entry_ptr *declarator_ssep,
                  a_func_info_block           *func_info,
                  a_decl_pos_block_ptr        decl_pos_block)
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
list.  If source sequence entries are enabled, *declarator_ssep is set
to point to a source sequence entry for the declaration.  If the top
type in the declarator derived type list is a function, *func_info is
filled with extra information about the parameter list, for use if a
function body follows.  For declarators that may turn out to be member
functions, member_parent_type is a pointer to the class (or struct or
union) type of which it is a member; otherwise it is NULL.

The routine "declarator" is called at the top level, and it calls
this routine to do the actual work.  This routine can call itself
recursively to handle nested declarators.

If left-side or unbound qualifiers are detected, they are returned
in *p_left_call_conv, *p_unbound_call_conv, *left_qualifiers, and
*unbound_qualifiers.  See pointer_declarator for more information
on those.

The syntax is:

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
  a_type_ptr            complete_type;
  a_type_ptr            derived_type;
  a_type_ptr            bottom_derived_type;
  a_type_ptr            new_type_ptr;
  a_source_position     declarator_pos;
  a_boolean             real_declarator_allowed;
  a_boolean             abstract_declarator_allowed;
  a_boolean             is_name_start;
  a_boolean             is_nonstatic_member_function = FALSE;
  a_boolean             nonconstant_dimension_allowed;
  a_boolean             vla_allowed;
  a_boolean             vla_asterisk_allowed;
  a_boolean             parenthesized_initializer_allowed;
  a_call_conv_descr     left_call_conv, inner_left_call_conv;
  a_call_conv_descr     unbound_call_conv;
  a_type_qualifier_set  left_qualifiers, inner_left_qualifiers;
  a_type_qualifier_set  unbound_qualifiers;
  a_boolean             disallow_default_args, disallow_exception_spec;
  a_func_info_block     *local_func_info;

  db_enter(3, "r_declarator");
  set_err_pos_to_curr_token();
  /* Set declarator_pos to the start of the declarator (which may not be the
     position of the declarator-id).  It will be changed later if required. */
  copy_source_position(pos_curr_token, declarator_pos);
  *output_flags = DO_NO_OUTPUT_FLAGS;
  real_declarator_allowed = input_flags & DI_REAL_DECLARATOR_ALLOWED;
  abstract_declarator_allowed = input_flags & DI_ABSTRACT_DECLARATOR_ALLOWED;
  parenthesized_initializer_allowed =
                       (input_flags & DI_PARENTHESIZED_INITIALIZER_ALLOWED);
  nonconstant_dimension_allowed =
                            (input_flags & DI_DIMENSION_EXPRESSION_ALLOWED);
  vla_allowed = (input_flags & DI_VLA_ALLOWED) != 0;
  vla_asterisk_allowed = (input_flags & DI_VLA_ASTERISK_ALLOWED) != 0;
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
  /* Scan a list of pointer, reference and pointer-to-member declarators. */
  complete_type = pointer_declarator(specifiers_type,
                                     /*reference_allowed=*/
                                       C_dialect == C_dialect_cplusplus,
                                     &left_call_conv, &unbound_call_conv,
                                     &left_qualifiers, &unbound_qualifiers,
                                     decl_pos_block);
  derived_type = NULL;
  bottom_derived_type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) clear_call_conv_descr(&inner_left_call_conv);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) inner_left_qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
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
          curr_token == tok_ellipsis) {
        /* Function declarator rather than a nested declarator. */
        goto function_lparen;
      }  /* if */
    }  /* if */
    /* This parenthesis begins a nested declarator. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (unbound_call_conv.call_conv != (a_calling_convention)cc_default) {
        /* Constructs such as
             int __cdecl (*fp)();
           are not permitted. */
        pos_error(ec_calling_convention_may_not_precede_nested_declarator,
                  &declarator_pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (microsoft_mode or_near_and_far_enabled()) {
      if (unbound_qualifiers != TQ_NONE) {
        /* Constructs such as
             int far (*p);
           are not permitted. */
        pos_error(ec_mem_attrib_may_not_precede_nested_declarator,
                  &declarator_pos);
      }  /* if */
    }  /* if */
#endif  /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    add_stop_token(tok_rparen);
    /* Get the nested declarator, removing the flag allowing parenthesized
       initializers from the input_flags bit vector.  (The other flags are
       passed on in the recursive call.) */
    r_declarator((input_flags & ~DI_PARENTHESIZED_INITIALIZER_ALLOWED),
                 &local_do_flags, /*specifiers_type=*/(a_type_ptr)NULL,
                 member_parent_type, locator,
                 &derived_type, &bottom_derived_type,
                 is_constructor, is_destructor,
                 &inner_left_call_conv, &unbound_call_conv,
                 &inner_left_qualifiers, &unbound_qualifiers,
                 declarator_ssep, func_info, decl_pos_block);
    if (local_do_flags & DO_REAL_DECLARATOR_SCANNED) {
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      /* Copy the position of the declarator-id into declarator_pos. */
      declarator_pos = error_position;
    } else {
      parenthesized_initializer_allowed = FALSE;
    }  /* if */
    if (local_do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
      *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
      /* Force function_declarator to add an implicit-this-param pointer
         to the routine type. */
      member_parent_type = qualifier_class_type(*locator);
      check_assertion(member_parent_type != NULL);
    } else if (member_parent_type == NULL && locator != NULL) {
      /* In certain error cases (involving parenthesized declarators) the
         declaration can be marked as a constructor or destructor but
         member_parent_type will be NULL.  In other cases of parenthesized
         declarators member_parent_type must be updated from the locator. */
      if (is_error_locator(*locator) && (*is_constructor || *is_destructor)) {
        *is_constructor = *is_destructor = FALSE;
      } else {
        member_parent_type = qualifier_class_type(*locator);
      }  /* if */
    }  /* if */
    if (local_do_flags & DO_SCOPE_DEACTIVATION_REQUIRED) {
      /* A class scope was reactivated to scan a static data member or a
         member function.  It will have to be deactivated when the scanning
         of the top-level declarator is complete. */
      *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
    }  /* if */
    /* A nonconstant dimension, if allowed at all, is allowed only on the
       topmost type (an interpretation of the language specification in ARM
       5.3.3).  Set the flag to FALSE for subsequent processing. */
    nonconstant_dimension_allowed = FALSE;
    /* Check for and get the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    /* Not a nested declarator. */
    /* An identifier is expected next, but is omitted in the 
       abstract declarator. */
    is_name_start = (is_decl_qualified_name_start() ||
                     curr_token == tok_operator || curr_token == tok_compl);
    if (!real_declarator_allowed ||
        (abstract_declarator_allowed && !is_name_start)) {
      /* Identifier is omitted in an abstract declarator.  Be sure it is not a
         tk_unknown type. */
      check_assertion(specifiers_type == NULL ||
                      !is_unknown_type(specifiers_type) ||
                      (complete_type != NULL &&
                       is_or_contains_error_type(complete_type)));
      parenthesized_initializer_allowed = FALSE;
    } else {
      /* Real (non-abstract) declarator.  Reset declarator_pos to correspond
         to the position of the declarator-id. */
      declarator_pos = pos_curr_token;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        fprintf(f_debug, "declarator: empty ss entry for \"%s\":\n",
                curr_token == tok_identifier ?
                  locator_for_curr_id.symbol_header->identifier :
                  token_names[(int)curr_token]);
      }  /* if */
#endif /* DEBUG */
      *declarator_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      /* Process the name declared here. */
      scan_real_declarator_id(input_flags, output_flags, locator,
                              is_constructor, is_destructor,
                              &parenthesized_initializer_allowed,
                              &member_parent_type, decl_pos_block);
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
     a derived type list, and the new entries are added to its bottom.
  */
  while (curr_token == tok_lparen || curr_token == tok_lbracket) {
    if (curr_token == tok_lparen) {
      /* Appears to be a function declarator.  But be sure it's not the
         start of a parenthesized initializer (C++ only). */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_source_position  lparen_pos;
      lparen_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Advance past the left parenthesis. */
      (void)get_token();
      if (parenthesized_initializer_allowed &&
          curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* The context and other information we have about the declarator do
           not preclude a parenthesized initializer, nor does the token that
           follows the left paren.  If the construct inside the parentheses
           could be interpreted as a declaration, then do so.  Otherwise,
           treat this as a parenthesized initializer. */
        if (!is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                              DFS_REAL_DECLARATOR_ALLOWED)) {
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
            if (decl_pos_block != NULL) {
              decl_pos_block->var_init_range.start = lparen_pos;
            }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            /* Function_declarator should not be called, so exit the loop. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
function_lparen:
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      local_func_info = func_info;
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
          local_func_info = NULL;
          *is_constructor = *is_destructor = FALSE;
        } else if (*output_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
          check_assertion(func_info == NULL);
          is_nonstatic_member_function = TRUE;
          *is_constructor = *is_destructor = FALSE;
        } else if (func_info == NULL) {
          *is_constructor = *is_destructor = FALSE;
          is_nonstatic_member_function = FALSE;
          member_parent_type = NULL;
        } else if (*is_constructor || *is_destructor) {
          is_nonstatic_member_function = TRUE;
        } else {
          if (input_flags & DI_NONSTATIC_MEMBER) {
            if (locator->is_operator_name &&
                (is_new_operator(locator->variant.opname) ||
                 is_delete_operator(locator->variant.opname))) {
              /* operator new and operator delete are always static, even
                 if "static" was not specified in the declaration. */
            } else {
              is_nonstatic_member_function = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Normal C case.  If the derived type is nonnull this is not the
           top-most type, so we don't want to fetch the extra function info. */
        if (derived_type != NULL) local_func_info = NULL;
      }  /* if */
      /* Pass in a flag to indicate whether default arguments are allowed at
         all.  They should be disallowed on top-level function declarations
         for explicit template instantiations and template specializations --
         for instance:
           template <class T> void f(T) { ... }
           template<> void f(int=0);
           template void f(char=0);
         In addition, default arguments are disallowed in template parameter
         declarations. */
      disallow_default_args = C_mode() ||
                              (input_flags & DI_IS_TEMPLATE_PARAM_DECL) ||
                              (local_func_info != NULL &&
                               (input_flags & (DI_IS_SPECIALIZATION |
                                               DI_IS_EXPLICIT_INSTANTIATION)));
      /* Pass in a flag to indicate whether exception specifications are
         allowed.  They are allowed on a top-level function declaration and
         on a top-level pointer-to-function-type declaration that does not
         appear in a typedef declaration.  (Note: pointer-to-member-functions
         declarations are not mentioned in WP 15.4 [except.spec] as allowing
         exception specifications.) */
      disallow_exception_spec = TRUE;
      if (!C_mode() && !(input_flags & DI_IS_TYPEDEF_DECLARATION)) {
        if (derived_type == NULL || is_function_type(derived_type)) {
          /* Top level function declaration, or return type of function
             type. */
          disallow_exception_spec = FALSE;
        } else if (is_ptr_or_ref_type(derived_type)) {
          /* If derived_type is a pointer or reference type that currently
             points to NULL, this can be assumed to be a top-level pointer
             or reference declaration, and an exception specification is
             permitted:
               void (*pf)() throw();    // Okay
               void (**ppf)() throw();  // Error
          */
          disallow_exception_spec = (type_pointed_to(derived_type) != NULL);
        } else if (is_ptr_to_member_type(derived_type)) {
          /* Similarly if derived_type is a pointer-to-member type whose
             member pointer is NULL:
               void (A::*pmf)() throw ();  // Okay
               void (A::**ppmf)() throw(); // Error
               void (* A::*pm)() throw();  // Error
          */
          disallow_exception_spec = (pm_member_type(derived_type) != NULL);
        }  /* if */
      }  /* if */
      function_declarator(&new_type_ptr, local_func_info, locator,
                          member_parent_type, is_nonstatic_member_function,
                          *is_constructor, *is_destructor,
                          disallow_default_args, disallow_exception_spec,
                          (input_flags & DI_IS_TYPEDEF_DECLARATION) != 0,
                          (input_flags & DI_IS_FRIEND_DECL) != 0,
                          decl_pos_block);
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
      a_boolean  top_level_field_decl, top_level_param_decl;

      /* This is a top-level declarator if derived_type is NULL; it's a field
         declaration only if the nonstatic member flag is set.  (Note: it
         will be set for fields in C mode as well as in C++ mode.) */
      top_level_field_decl = (input_flags & DI_NONSTATIC_MEMBER) &&
                             derived_type == NULL;
      /* See whether this is a top-level declarator in a parameter
         declaration. */
      top_level_param_decl = (input_flags & DI_IS_PARAMETER_DECL) &&
                             derived_type == NULL;
      array_declarator(&new_type_ptr, nonconstant_dimension_allowed,
                       vla_allowed, vla_asterisk_allowed,
                       top_level_field_decl, top_level_param_decl,
                       decl_pos_block);
      if (nonconstant_dimension_allowed) {
        /* In C++ a array declarator that appears in an operator new()
           expression may have a nonconstant expression in the first
           dimension (ARM 5.3.3).  Subsequent dimensions must be constants. */
        nonconstant_dimension_allowed = FALSE;
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      /* Apply left-side qualifiers that were hanging:
           int (__cdecl *f)();
                             ^ We're here now.
                ^ inner_left_call_conv indicates this.
         The __cdecl calling convention was returned from the nested
         declarator scan and goes on top of the function type. */
      if (inner_left_call_conv.call_conv != (a_calling_convention)cc_default) {
        update_calling_convention(&new_type_ptr, &inner_left_call_conv,
                                  locator != NULL ? &locator->source_position
                                                  : &declarator_pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
    if (near_and_far_enabled()) {
      if (inner_left_qualifiers != TQ_NONE) {
        if (new_type_ptr->kind == (a_type_kind)tk_array) {
          /* A case like
               int (__near *p)[5];
             The qualifiers cannot be applied to the array now because they
             go on the element type and the element type is not attached yet.
             Leave them in inner_left_qualifiers for the next time around the
             loop. */
        } else {
          /* Function case.  The qualifiers can be added now. */
          new_type_ptr = make_qualified_type(new_type_ptr,
                                             inner_left_qualifiers);
          inner_left_qualifiers = TQ_NONE;
        }  /* if */
      }  /* if */
    }  /* if */
#endif  /* NEAR_AND_FAR_ALLOWED */
    /* Check that we do not create a typedef for a pointer or reference to a
       qualified function type. */
    if (new_type_ptr->kind == (a_type_kind)tk_routine &&
        derived_type != NULL && is_ptr_or_ref_type(derived_type)) {
      a_routine_type_supplement_ptr  rtsp =
                                     new_type_ptr->variant.routine.extra_info;
      if (rtsp->this_class == NULL && rtsp->qualifiers != TQ_NONE) {
        /* Catch the following:
              typedef void (*PF)() const;
           Qualified function types are only allowed to declare members,
           pointer-to-members and synonym typedefs. */
        pos_error(ec_ptr_or_ref_to_qualified_function_type,
                  &locator->source_position);
      }  /* if */
    }  /* if */
    /* Add the new type to the bottom of the existing derived type list.
       Note that this involves error checking. */
    add_to_derived_type_list(new_type_ptr, &derived_type, &bottom_derived_type,
                             (input_flags & DI_IS_MICROSOFT_PROPERTY) != 0);
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
    (skip_typerefs(specifiers_type))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Use the position of the identifier as the position of this declarator
     for error purposes.  If this was an abstract declarator, declarator_pos
     has been set to the beginning of the declarator.  The error position
     is set a bit early here so that any errors below from combining the
     two lists will have the right position. */
  copy_source_position(declarator_pos, error_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Apply left-side qualifiers that were hanging, for the case where
       there were no function or array declarators:
         typedef void F(int);
         F (__cdecl *f);
                       ^ We're here now.
            ^ inner_left_call_conv indicates this.
       The __cdecl calling convention was returned from the nested
       declarator scan and goes on top of the complete type.  If there
       is no complete type, move these hanging left-side qualifiers to
       the variables that will apply them to the specifiers type. */
    if (inner_left_call_conv.call_conv != (a_calling_convention)cc_default) {
      if (complete_type != NULL) {
        update_calling_convention(&complete_type, &inner_left_call_conv,
                                  &declarator_pos);
      } else {
        check_assertion(left_call_conv.call_conv ==
                        (a_calling_convention)cc_default);
        left_call_conv = inner_left_call_conv;
      }  /* if */
    }  /* if */
    /* Return the left calling convention to the caller if it can't be
       handled at this level (i.e., in a nested declarator). */
    if (specifiers_type == NULL) *p_left_call_conv = left_call_conv;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    if (inner_left_qualifiers != TQ_NONE) {
      if (complete_type != NULL) {
        check_for_addition_of_incompatible_qualifiers(complete_type,
                                                      &inner_left_qualifiers,
                                                      &declarator_pos);
        complete_type = make_qualified_type(complete_type,
                                            inner_left_qualifiers);
      } else {
        check_assertion(left_qualifiers == TQ_NONE);
        left_qualifiers = inner_left_qualifiers;
      }  /* if */
    }  /* if */
    /* Return the left qualifiers to the caller if they can't be handled
       at this level by applying them to the specifiers type (i.e., in a
       nested declarator). */
    if (specifiers_type == NULL) *p_left_qualifiers = left_qualifiers;
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  if (specifiers_type != NULL) {
    /* This is a top-level call to declarator.  Do some checks for special
       member functions and set complete_type appropriately, so that it can
       be added as return type to the associated routine type. */
    if (!(input_flags & DI_OPERATOR_NAME_ALLOWED) && locator != NULL &&
        (locator->is_operator_name || locator->is_conversion_name)) {
      pos_error(ec_operator_name_not_allowed, &locator->source_position);
      set_to_error_locator(*locator);
      complete_type = error_type();
    } else if (locator != NULL && locator->is_conversion_name) {
      /* Do error checking on the conversion function declaration. */
      if (is_error_locator(*locator)) {
        complete_type = error_type();
      } else {
        if (!is_unknown_type(specifiers_type) &&
            !(input_flags & DI_NO_TYPE_SPECIFIERS)) {
          pos_error(ec_return_type_on_conversion_function, &declarator_pos);
        }  /* if */
        complete_type = locator->variant.conversion_result_type;
      }  /* if */
    } else if (*is_constructor) {
      /* Return type should be "unknown" at this point, unless the declarator
         was parenthesized in which case decl_specifiers will have though we
         are in an "implicit int" case.  Change it to the constructed type
         (a front end convention that deviates from what is explicitly in the
         source for a constructor declaration. */
      if (!is_unknown_type(specifiers_type) &&
          !(input_flags & DI_NO_TYPE_SPECIFIERS)) {
        pos_error(ec_return_type_on_constructor, &declarator_pos);
      }  /* if */
      complete_type = make_reference_type(member_parent_type);
    } else if (*is_destructor) {
      /* Make the destructor return "void". */
      if (is_error_locator(*locator)) {
        complete_type = error_type();
      } else {
        if (!(input_flags & DI_NO_TYPE_SPECIFIERS)) {
          pos_error(ec_return_type_on_destructor, &declarator_pos);
        } else if (derived_type == NULL || !is_function_type(derived_type)) {
          pos_error(ec_bad_destructor_decl, &declarator_pos);
        }  /* if */
        complete_type = void_type();
      }  /* if */
    }  /* if */
  }  /* if */
  /* Combine the derived type list with the earlier complete type
     (pointer derived type list plus specifiers_list), making
     the full type.  Note that this involves error checking. */
  if (derived_type != NULL && complete_type != NULL) {
    add_to_derived_type_list(complete_type,
                             &derived_type, &bottom_derived_type,
                             (input_flags & DI_IS_MICROSOFT_PROPERTY) != 0);
    complete_type = derived_type;
  } else {
    if (derived_type != NULL) complete_type = derived_type;
    /* Find the bottom of the type to be returned. */
    if (complete_type != NULL && !is_error_type(complete_type)) {
      bottom_derived_type = find_bottom_of_type(complete_type);
    } else {
      bottom_derived_type = NULL;
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (unbound_call_conv.call_conv != (a_calling_convention)cc_default) {
      /* If there is an unbound calling convention, attempt to apply it to
         the complete type (if one exists).  If none exists, return the unbound
         type to the caller. */
      if (complete_type != NULL) {
        update_calling_convention(&complete_type, &unbound_call_conv,
                                  &declarator_pos);
      } else {
        *p_unbound_call_conv = unbound_call_conv;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (microsoft_mode or_near_and_far_enabled()) {
    if (unbound_qualifiers != TQ_NONE) {
      /* If there are unbound type qualifiers, apply them to the complete
         type (if it exists).  If it does not exist, return the unbound
         type qualifiers to the caller. */
      if (complete_type != NULL) {
#if NEAR_AND_FAR_ALLOWED
        check_for_addition_of_incompatible_qualifiers(complete_type,
                                                      &unbound_qualifiers,
                                                      &declarator_pos);
#endif  /* NEAR_AND_FAR_ALLOWED */
        complete_type = make_qualified_type(complete_type, unbound_qualifiers);
      } else {
        *p_unbound_qualifiers = unbound_qualifiers;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  if (specifiers_type != NULL) {
    /* This is a top-level call to declarator. */
    if (!is_function_type(complete_type)) {
      if (locator != NULL &&
          (locator->is_operator_name || locator->is_conversion_name)) {
        /* A declaration of an operator must have a function type. */
        pos_error(ec_function_type_required, &locator->source_position);
        set_to_error_locator(*locator);
        complete_type = bottom_derived_type = error_type();
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    } else if (func_info != NULL) {
      /* Set the declared type in the func_info block.  Note that further
         fixup may be required for member functions, since default argument
         expressions will not have been scanned yet. */
      func_info->declared_type = form_declared_type(complete_type, func_info);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  }  /* if */
  if (*output_flags & DO_SCOPE_DEACTIVATION_REQUIRED) {
    /* A class scope was reactivated when a qualified name was seen. */
    if (specifiers_type != NULL) {
      /* This is a top-level call to declarator, so the class scope can now
         be deactivated. */
      if (scope_stack[depth_scope_stack].kind ==
                          (a_scope_kind)sck_class_reactivation) {
        pop_class_reactivation_scope();
      } else {
        /* Must be a namespace reactivation. */
        pop_namespace_reactivation_scope();
      }  /* if */
      /* Clear the flag, just to be neat. */
      *output_flags &= ~(a_decl_flag_set)DO_SCOPE_DEACTIVATION_REQUIRED;
    } else {
      /* Just pass the information up to the caller. */
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
}  /* r_declarator */


void declarator(a_decl_flag_set             input_flags,
                a_decl_flag_set             *output_flags,
                a_type_ptr                  specifiers_type,
                a_type_ptr                  member_parent_type,
                a_symbol_locator            *locator,
                a_type_ptr                  *p_complete_type,
                a_source_sequence_entry_ptr *declarator_ssep,
                a_func_info_block           *func_info,
                a_decl_pos_block_ptr        decl_pos_block)
/*
Scan a declarator.  This is an interface routine for r_declarator, provided
so that parameters needed only on recursive calls for nested declarators
need not be supplied on other calls.  See r_declarator for the meaning of
the parameters.
*/
{
  a_type_ptr  bottom_derived_type = NULL;
  a_boolean   is_constructor = FALSE, is_destructor = FALSE;

  is_constructor = (input_flags & DI_IS_CONSTRUCTOR) != 0;
  /* If DI_IS_CONSTRUCTOR is set, the parent class should be provided. */
  check_assertion_str(!is_constructor || member_parent_type != NULL ||
                      (input_flags & DI_IS_FRIEND_DECL),
                      "declarator: parent class is NULL for ctor");
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.start = pos_curr_token;
    decl_pos_block->declarator_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  r_declarator(input_flags, output_flags, specifiers_type,
               member_parent_type, locator, p_complete_type,
               &bottom_derived_type, &is_constructor, &is_destructor,
               (a_call_conv_descr_ptr)NULL, (a_call_conv_descr_ptr)NULL,
               (a_type_qualifier_set *)NULL, (a_type_qualifier_set *)NULL,
               declarator_ssep, func_info, decl_pos_block);
  if (is_constructor) {
    *output_flags |= DO_IS_CONSTRUCTOR;
  }  /* if */
  if (is_destructor) {
    *output_flags |= DO_IS_DESTRUCTOR;
  }  /* if */
}  /* declarator */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1996 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
