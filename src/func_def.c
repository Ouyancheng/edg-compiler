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

func_def.c -- Processing for function definitions (both user supplied and
              compiler generated).

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
#include "exprutil.h"
#include "lower_il.h"
#include "statements.h"


a_boolean check_function_return_type(a_type_ptr         rout_type,
                                     a_source_position  *err_pos,
                                     a_boolean          is_expr_use)
/*
Given a routine type, check that the return type is valid, issuing an
error if not, and also set the routine calling method flag if appropriate.
is_expr_use is TRUE if the function is being called or its address is
being taken.
*/
{
  a_type_ptr                     return_type;
  an_error_code                  error_code;
  a_boolean                      err = FALSE;
  a_routine_type_supplement_ptr  rtsp;

  rout_type = skip_typerefs(rout_type);
  /* Any type qualifiers on the return type are dropped because rvalues
     do not have qualified types. */
  return_type = skip_typerefs(rout_type->variant.routine.return_type);
  /* If return_type is an uninstantiated template class, force its
     instantiation. */
  check_for_uninstantiated_template_class(return_type);
  /* 3.7.1, constraints: The return type of a function shall be void
     or an object type other than array.  See also the constraints of
     3.5.4.3 on function declarators, enforced previously by
     add_to_derived_type_list.  In addition, a reference type (including a
     reference to an array or function) may also be returned (ARM 8.2.5). */
  if (is_void_type(return_type)) {
    /* Okay. */
  } else if (is_error_type(return_type)) {
    /* No diagnostic this time. */
  } else {
    if (is_expr_use) {
      /* The type check is simpler on function calls, because function and
         array types have already been filtered out. */
      check_assertion(!is_array_type(return_type) &&
                      !is_function_type(return_type));
      if (is_incomplete_type(return_type)) {
        /* Note that err is set (for the return value) even if no diagnostic
           is actually issued. */
        err = TRUE;
        rtsp = rout_type->variant.routine.extra_info;
        if (rtsp->suppress_diagnostic_on_incomplete_return_type) {
          /* A diagnostic has already been issued on calling (or taking the
             address of) this routine.  No need to do it again. */
          error_code = ec_no_error;
        } else {
          error_code = ec_incomplete_function_return_type;
          rtsp->suppress_diagnostic_on_incomplete_return_type = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Declaration case. */
      if ((is_object_type(return_type) && !is_array_type(return_type)) ||
          is_reference_type(return_type)) {
        /* err = FALSE; */
      } else {
        err = TRUE;
        if (is_class_struct_union_type(return_type) &&
               is_incomplete_type(return_type)) {
          error_code = ec_incomplete_function_return_type;
        } else {
          error_code = ec_bad_function_return_type;
        }  /* if */
      }  /* if */
    }  /* if */
    if (err && error_code != ec_no_error) {
      pos_error(error_code, err_pos);
    }  /* if */
  }  /* if */
  return !err;
}  /* check_function_return_type */


static void fixup_parameters(a_variable_ptr    param_list,
                             a_param_type_ptr  param_type_list)
/*
Set each variable in a linked list of parameters to point to the corresponding
param type entry.
*/
{
  a_variable_ptr    vp = param_list;
  a_param_type_ptr  ptp = param_type_list;

  if (param_list != NULL) {
    for (; vp != NULL; vp = vp->next, ptp = ptp->next) {
      /* Be sure there are not too few param type entries. */
      check_assertion(ptp != NULL);
      vp->assoc_param_type = ptp;
    }  /* for */
    /* Be sure there are not too many param type entries. */
    check_assertion(ptp == NULL);
  }  /* if */
}  /* fixup_parameters */


static void decl_parameter(a_param_id_ptr    param_id,
                           a_param_type_ptr  ptp,
                           a_boolean         function_instantiation)
/*
Enter the declaration of an identifier for a parameter.  The param_id
points to an sk_parameter symbol, which under ordinary circumstances, is
turned into an sk_variable symbol; but if function_instantiation is TRUE,
a new symbol is created and entered in the symbol table.
*/
{
  a_symbol_ptr      sym;
  a_variable_ptr    vp;
  a_type_ptr        tp;
  a_symbol_locator  locator;

  db_enter(3, "decl_parameter");
  /* Choose the type to use, the one in the param-type entry or the one in
     the param-id entry.  Usually, they will be the same.  However, for
     template functions being instantiated the type pointed to by the param-id
     may include a template parameter, so use the type in param-type entry,
     which will be the result of the template arg substitution.  Otherwise,
     use the param-id type, since it will be the one actually used in the
     function definition, whereas the type in the param-type entry may be a
     composite type, as in the following example:
       void f(int a[3]);
       void f(a) int a[]; { ... }
     In the second declaration the param-id type is int[], but the composite
     type produced for the routine's interface is int[3]. */
  tp = function_instantiation ? ptp->type : param_id->type;
  check_for_uninstantiated_template_class(tp);
  if (is_incomplete_type(tp)) {
    /* Incomplete type is not allowed. */
    pos_error(ec_incomplete_type_not_allowed, &param_id->type_pos);
    tp = ptp->type = error_type();
  }  /* if */
  /* Create the parameter variable. */
  vp = make_param_variable(tp, param_id->storage_class);
  add_to_parameters_list(vp);
  sym = param_id->symbol;
  if (sym == NULL) {
    /* This param_id entry represents an unnamed parameter (which is legal
       in function definitions in C++). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CHECKING
    if (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
        depth_template_declaration_scope == NO_SCOPE_DEPTH) {
      check_assertion(param_id->source_sequence_entry != NULL);
    }  /* if */
#endif /* CHECKING */
    update_source_sequence_list((char *)vp, (an_il_entry_kind)iek_variable,
                                param_id->source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
    make_locator_for_symbol(sym, &locator);
    if (function_instantiation) {
      sym = enter_local_symbol((a_symbol_kind)sk_variable, &locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/FALSE);
    } else {
      set_symbol_kind(sym, (a_symbol_kind)sk_variable);
    }  /* if */
    sym->variant.variable.ptr = vp;
    set_source_corresp(&(vp->source_corresp), sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                              &sym->decl_position,
                              param_id->source_sequence_entry);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_defined(sym, &sym->decl_position);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_variable_value_set(sym);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "Changed from parameter symbol: ", 4);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  db_exit();
}  /* decl_parameter */


void scan_function_body(a_routine_ptr     rout_ptr,
                        a_func_info_block *func_info,
                        a_decl_flag_set   flags)
/*
Scan the function body of the routine pointed to rout_ptr.  *func_info
contains information accumulated during the declaration.  The flags control
specific requirements of the scan, since this routine is called not only
for normal function definitions but also (in C++ mode only, of course) for the
delayed scan of cached tokens of member functions defined in a class definition
and for the instantiation of template functions.
*/
{
  a_type_ptr                     class_type, rout_type;
  a_routine_type_supplement_ptr  rtsp;
  a_scope_number                 scope_number;
  a_param_id_ptr                 param_id;
  a_scope_ptr                    scope_ptr;
  a_struct_stmt_stack_state      saved_sss_state;
  a_boolean                      is_instantiation;
  a_param_type_ptr               ptp;

  db_enter(3, "scan_function_body");
  class_type = rout_ptr->source_corresp.class_of_which_a_member;
  /* Type on a function should never be a typedef. */
  check_assertion(rout_ptr->type->kind == (a_type_kind)tk_routine);
  rout_type = skip_typerefs(rout_ptr->type);
  /* Issue an error if this is an invalid return type. */
  (void)check_function_return_type(rout_type,
                                   &rout_ptr->source_corresp.decl_position,
                                   /*is_expr_use=*/FALSE);
  /* In certain very obscure cases, the routine type associated with
     rout_ptr may be replaced by an equivalent type entry.  Refetch the type,
     just in case. */
  rout_type = skip_typerefs(rout_ptr->type);
  rtsp = rout_type->variant.routine.extra_info;
  if (class_type != NULL) {
    /* Member function -- either an inline or "out-of-line" definition. */
    if (flags & SFB_NO_CLASS_REACTIVATION) {
      /* Inline.  Class has already been reactivated. */
    } else {
      /* Push a class symbol reactivation scope, to make class member names
         visible for processing the function definition. */
      push_class_reactivation_scope(class_type);
    }  /* if */
  }  /* if */
  is_instantiation = (flags & SFB_IS_INSTANTIATION) != 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!func_info->function_type_from_typedef &&
      func_info->prototype_scope_ss_entry_start != NULL) {
    a_source_sequence_entry_ptr  starting_ssep, ending_ssep;
    a_src_seq_sublist_ptr        sublist = NULL;

    starting_ssep = func_info->prototype_scope_ss_entry_start;
    if (starting_ssep != NULL) {
      ending_ssep = func_info->prototype_scope_ss_entry_end;
      check_assertion(in_file_scope(starting_ssep));            
      if (depth_innermost_ss_list_scope != DEPTH_OF_FILE_SCOPE) {
        sublist = sublist_header_of(starting_ssep);
      }  /* if */
      if (starting_ssep->prev == NULL) {
        /* The head of the list. */
        check_assertion(sublist != NULL);
        sublist->source_sequence_list = ending_ssep->next;
      } else {
        starting_ssep->prev->next = ending_ssep->next;
      }  /* if */
      if (ending_ssep->next == NULL) {
        /* Tail of the list. */
        if (sublist != NULL) {
          sublist->last_source_sequence_entry = starting_ssep->prev;
        } else {
          scope_stack[DEPTH_OF_FILE_SCOPE].last_source_sequence_entry =
                                                        starting_ssep->prev;
        }  /* if */
      } else {
        ending_ssep->next->prev = starting_ssep->prev;
      }  /* if */
      if (sublist != NULL && sublist->source_sequence_list == NULL) {
        remove_sublist_header_and_parent(sublist,
                                         find_sublist_parent(sublist));
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  scope_number = (is_instantiation) ?
                        NO_SCOPE_NUMBER : func_info->scope_number;
  /* Push the name scope for the routine body. */
  scope_ptr = push_scope((a_scope_kind)sck_function, scope_number,
                         (a_type_ptr)NULL, rout_ptr, (a_symbol_ptr)NULL,
                         (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  rout_ptr->assoc_scope = curr_il_region_number;
  rtsp->assoc_routine = rout_ptr;
  /* If return value optimization may be possible (i.e., if the routine
     returns a class value via a copy constructor) set the flag to TRUE.
     (It is also required that all the return statements return a single local
     variable -- if that turns out not to be the case, the flag will be
     cleared again.) */
  if (rtsp->value_returned_by_cctor) {
    scope_stack[depth_scope_stack].return_value_optimization_possible = TRUE;
  }  /* if */
  if (class_type != NULL && rtsp->implicit_this_param_type != NULL) {
    a_variable_ptr  vp = make_param_variable(rtsp->implicit_this_param_type,
                                             (a_storage_class)sc_auto);
    vp->is_this_parameter = TRUE;
    scope_ptr->variant.routine.this_param_variable = vp;
  }  /* if */
  if (func_info->function_type_from_typedef) {
    /* An error was already issued on this.  Now, since no parameters were
       specified, skip the processing for parameter names. */
    check_assertion(func_info->prototype_scope_symbols == NULL);
    check_assertion(func_info->param_id_list == NULL);
  } else {
    /* Correctly declared function type. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->prototype_scope_ss_entry_start != NULL) {
      /* Step through the segment of file-scope source sequence entries
         generated when the parameter list of the function was scanned.
         Do necessary fixups for parameter entries, and build function-scope
         proxies where necessary. */
      a_scope_stack_entry_ptr  stack_ptr;
      a_source_sequence_entry_ptr  ssep, next_ssep;

      stack_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE];
      ssep = func_info->prototype_scope_ss_entry_start;
      for (; ssep != NULL; ssep = next_ssep) {
        if (ssep == func_info->prototype_scope_ss_entry_end) {
          next_ssep = NULL;
        } else {
          next_ssep = ssep->next;
        }  /* if */
        ssep->prev = ssep->next = NULL;
        switch (ss_entry_kind(ssep)) {
          case iek_none:
            /* An empty entry should be for a parameter (the entry could
               not be filled in when the parameter identifier appeared, because
               the variable entry does not get built at that time).  Find
               the corresponding parameter (the lists are not necessarily in
               the same order). */
            param_id = func_info->param_id_list;
            for (; param_id != NULL; param_id = param_id->next) {
              if (param_id->source_sequence_entry == ssep) break;
            }  /* for */
            check_assertion(param_id != NULL);
            /* Take the entry off the file-scope list and add one (also
               empty so far) to the function-scope list. */
            ssep->next = stack_ptr->source_sequence_avail_list;
            stack_ptr->source_sequence_avail_list = ssep;
            param_id->source_sequence_entry =
                                          add_empty_source_sequence_entry();
            break;
          default:
            /* For types (as well as other miscellany, such as fields in a
               C struct definition), add the entries to a sublist of the
               function scope list. */
            add_to_source_sequence_list(ssep);
            break;
            /* No action. */
        }  /* switch */
      }  /* for */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (rtsp->prototyped && func_info->any_prototype_names_omitted) {
      /* New-style (function prototype) for which at least one of the param
         names was omitted in the prototype.  In C this is not valid on a
         function definition; in C++ it's okay (see ARM 8.2.5, 8.3). */
      if (C_dialect != C_dialect_cplusplus) {
        error(ec_all_proto_params_must_be_named);
      }  /* if */
    }  /* if */
    param_id = func_info->param_id_list;
    ptp = rtsp->param_type_list;
    /* Be sure param-id and param-type lists are in sync. */
    check_assertion((param_id == NULL) == (ptp == NULL));
    for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
      /* Declare each parameter identifier to have the associated type
         from the parameter type list. */
      decl_parameter(param_id, ptp, is_instantiation);
      /* Be sure param-id and param-type lists are in sync. */
      check_assertion((param_id->next == NULL) == (ptp->next == NULL));
    }  /* for */
    if (!is_instantiation) {
      /* Parameter symbols that were created in the prototype scope (and then
         removed in pop_scope) have to be reentered in the function scope;
         they will be transformed into variable symbols.  Also, in C mode,
         types that were defined in the prototype scope are reactivated now
         so that they will be available in the current scope. */
      if (func_info->prototype_scope_symbols != NULL) {
        reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
      }  /* if */
    }  /* if */
    /* Set the assoc_param_type field in each of the parameter variables. */
    fixup_parameters(scope_ptr->variant.routine.parameters,
                     rtsp->param_type_list);
  }  /* if */
#if CHECKING
  if (total_errors == 0) {
    /* Except where there are invalid declarations, the flags in the types
       should be consistent with the special function kinds. */
    check_assertion((a_boolean)rtsp->assoc_routine_is_ctor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_constructor));
    check_assertion((a_boolean)rtsp->assoc_routine_is_dtor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor));
  }  /* if */
#endif /* CHECKING */
  /* Enter the constructor initializers.  If the current token is a ":",
     explicit initialization for the constructor follows, but even without
     an explicit initializer, any implicit initializers should be recorded. */
  if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
    scope_ptr->variant.routine.constructor_inits =
                                      ctor_initializer(rout_ptr,
                                                       /*user_defined=*/TRUE);
  } else if (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor) {
    scope_ptr->variant.routine.constructor_inits =
                                      dtor_initializer(rout_ptr);
  }  /* if */
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Save structured statement stack state before calling compound_statement
       (so that it can be restored upon return) and create a new structured
       statement stack.  This is required for function definitions in classes
       defined within a function definition.  An indefinite nesting depth is
       supported */
    new_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  /* Scan the compound statement defining the function.  The closing "}"
     is not swallowed by compound_statement, so that the pop_scope call
     can be done to get any errors out right on the "}". */
  scope_ptr->assoc_block =
        compound_statement(/*at_function_level=*/TRUE,
                           (flags & SFB_IMPLICITLY_DECLARED_RETURN_TYPE) == 0,
                           /*is_catch_clause=*/FALSE);
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Restore the original structured statement stack. */
    restore_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  /* Pop the function scope. */
  pop_scope();
  if (class_type != NULL && !(flags & SFB_NO_CLASS_REACTIVATION)) {
    /* Pop the class symbol reactivation scope. */
    pop_class_reactivation_scope();
  }  /* if */  
  /* Check for the closing "}", not done in compound_statement.  Note that
     required_token is not called; if compound_statement returned on
     anything other than a right brace, it's because we should start parsing
     on this token. */
  if (curr_token != tok_rbrace) {
    pos_error(ec_exp_rbrace, &pos_curr_token);
  }  /* if */
  db_exit();
}  /* scan_function_body */


static void define_member_function(a_symbol_locator   *locator,
				   a_type_ptr         type_ptr,
                                   a_func_info_block  *func_info,
				   a_symbol_ptr       *symbol_ptr,
                                   an_id_linkage_kind *linkage_ptr,
				   a_type_ptr	      *old_type,
				   a_symbol_ptr	      *ext_sym)
/*
This routine is called in the case of a member function definition.  Its
function is similar to that of decl_var_or_routine, which is called for
the definitions of ordinary functions.  After doing some error checking,
it calls reconcile_routine_types to merge the current type with the type
on a prior declaration.
*/
{
  a_symbol_ptr	 sym;
  a_type_ptr     class_type;
  a_routine_ptr  rp;

  db_enter(3, "define_member_function");
  class_type = locator->specific_symbol->class_of_which_a_member;
  sym = locator->specific_symbol;
  if (!is_member_function_symbol(sym)) {
    /* We must have nonfunction class member.  This is an error, so set sym
       to NULL to force the creation of a fake member function symbol. */
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    }  /* if */
    sym = NULL;
  } else {
    /* Look for a member function symbol of this type in the symbol table.
       It is an error if it is  not already there. */
    sym = member_function_redecl_sym(sym, type_ptr);
    if (sym == NULL) {
      /* No member function with a matching type was found.  Issue an error. */
      pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_overloaded_function_incompatible_type :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
    } else if (sym->variant.routine.ptr->compiler_generated) {
      /* Attempting to give a definition for a function that was implicitly
         declared. */
      pos_error(ec_definition_of_implicitly_declared_function,
                &locator->source_position);
      /* Unless a definition has already been generated, reset some flags
         so that that this routine will be treated as user-declared from
         now on. */
      if (!sym->defined) {
        sym->variant.routine.ptr->compiler_generated = FALSE;
        sym->variant.routine.ptr->is_inline = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL || sym->defined) {
    /* Error case. */
    a_routine_ptr        other_rp = NULL;
    a_symbol_header_ptr  hdr = locator->symbol_header;

    if (sym != NULL) {
      /* Type was okay, but this member function has a body. */
      pos_sy_error(ec_function_redefinition, &locator->source_position, sym);
      other_rp = sym->variant.routine.ptr;
      type_ptr->variant.routine.extra_info->implicit_this_param_type =
          other_rp->type->variant.routine.extra_info->implicit_this_param_type;
    } else {
      /* In the error case assume the member function is nonstatic and give
         it an implicit this parameter type.  This will prevent an error from
         being issued on a direct reference to a nonstatic data member in the
         function body. */
      type_ptr->variant.routine.extra_info->implicit_this_param_type =
                                               make_pointer_type(class_type);
    }  /* if */
    /* An error has been detected.  Make a "fake" symbol and routine entry so
       that the routine definition can proceed. */
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             DEPTH_OF_FILE_SCOPE,
                             /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    sym->class_of_which_a_member = class_type;
    rp = make_routine(type_ptr, (a_storage_class)sc_static,
                      /*at_file_scope=*/TRUE, /*add_to_list=*/TRUE);
    sym->variant.routine.ptr = rp;
    set_source_corresp(&(rp->source_corresp), sym);
    rp->source_corresp.class_of_which_a_member = class_type;
    if (other_rp != NULL) {
      rp->special_kind = other_rp->special_kind;
      rp->opname_kind = other_rp->opname_kind;
    }  /* if */
    *old_type = type_ptr;
  } else {
    /* A member function symbol with a compatible type was found. */
    *old_type = routine_symbol_type(sym);
    /* The types may be compatible but not identical.  Create (in type_ptr)
       a composite type.  First copy the implicit this param type pointer
       into type_ptr:  it is always wrong for nonstatic member functions. */
    rp = sym->variant.routine.ptr;
    type_ptr->variant.routine.extra_info->implicit_this_param_type =
           (*old_type)->variant.routine.extra_info->implicit_this_param_type;
    reconcile_routine_types(sym->variant.routine.ptr, type_ptr,
                            /*preserve_rout_type=*/FALSE,
                            /*preserve_type_ptr=*/TRUE);
    if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
      /* If the routine is a default constructor or a copy constructor, it may
         be that this has not yet been recorded in the symbol.  (This becomes
         possible if there are default arguments in the definition.) */
      a_class_symbol_supplement_ptr  cssp;
      cssp = symbol_supplement_for_class(class_type);
      if (!cssp->has_default_constructor && is_default_constructor(rp)) {
        /* This is a default constructor, so set the flag. */
        cssp->has_default_constructor = TRUE;
      }  /* if */
      /* There are three flags associated with copy constructors. */
      if (!cssp->has_copy_constructor_for_const_object ||
          cssp->construction_by_bitwise_copy_allowed) {
        a_type_qualifier_set  qualifiers;
        if (is_copy_constructor(rp, class_type, &qualifiers)) {
          /* This is a copy constructor.  Note that the presence of a user-
             defined copy constructor means that construction by bitwise
             copying is not done. */
          cssp->has_copy_constructor = TRUE;
          cssp->has_copy_constructor_for_const_object =
                                            ((qualifiers & TQ_CONST) != 0);
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Do compatibility checking on the throw specification and bind the
       throw specification to the routine entry.  Note that the checking must
       be done before the routine's decl position is modified, to assure that
       the "original declaration line number" is displayed accurately. */
    check_exception_specification(func_info, rp);
    /* If this is an member function of an instantiation of a class
       template, set the specific_def flag in the instance entry. */
    if (sym->variant.routine.instance_ptr != NULL) {
      sym->variant.routine.instance_ptr->specific_def = TRUE;
      sym->variant.routine.ptr->specific_def = TRUE;
      sym->variant.routine.instance_ptr->instantiation_required = FALSE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                              &locator->source_position,
                              func_info->declarator_ssep);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_defined(sym, &locator->source_position);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    copy_source_position(locator->source_position,
                         rp->source_corresp.decl_position);
  }  /* if */
  if (func_info->is_inline) {
    if (!sym->variant.routine.ptr->is_inline &&
        sym->variant.routine.ptr->called) {
      /* Unless it was originally declared "inline" a member function that
         has been called may not have the "inline" attribute here. */
      pos_sy_error(ec_called_function_redeclared_inline,
                   &locator->source_position, sym);
    }  /* if */
    sym->variant.routine.ptr->is_inline = TRUE;
  }  /* if */
  if (any_deferred_access_checks()) {
    /* Now that we know which function has been declared, recheck any
       access errors that occurred while scanning the declaration. */
    check_assertion(rp != NULL);
    perform_deferred_access_checks_for_function(rp);
  }  /* if */
  /* If a lint-style "argsused" or "varargs" comment appeared, record that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments.  Note that this is done before calling
     process_curr_construct_pragmas; otherwise the pragmas we're interested
     in would have been disposed of. */
  record_lint_argsused_and_varargs_state(sym);
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  *symbol_ptr = sym;
  *ext_sym = NULL;
  *linkage_ptr = idl_external;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_member_function */


void function_definition(a_symbol_locator   *locator,
                         a_type_ptr         rout_type,
                         a_func_info_block  *func_info,
                         a_storage_class    storage_class,
                         a_boolean          has_explicit_type_specifier)
/*
Scan a function definition.  The declarator has already been scanned; the
old-style parameter declarations and the compound statement for the body
are still to come.  *locator is the locator to be used to enter the
function symbol; rout_type is the type for the function (which, in C++,
can be qualified -- hence the use of local variable unqualified_rout_type
where appropriate in this routine); *func_info contains information about
parameters, as well as field function_type_from_typedef (when it is FALSE,
the function type came from the declarator; when it is TRUE an error is
reported); storage_class is the storage class from the specifiers list; and
has_explicit_type_specifier is TRUE if the type of the function was explicitly
specified (rather than defaulted to "int").
*/
{
  a_symbol_ptr                   symbol_ptr, ext_sym;
  a_routine_ptr                  routine_ptr;
  a_param_id_ptr                 param_id;
  an_id_linkage_kind             linkage;
  a_type_ptr                     old_type, unqualified_rout_type;
  a_routine_type_supplement_ptr  extra_info;
  a_boolean                      prototyped;
  a_boolean	                 is_member_function_def = FALSE;
  a_param_type_ptr               ptp;
  a_decl_flag_set                flags;
  a_source_sequence_entry_ptr    declarator_ssep;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr    ss_entry_start_prev;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "function_definition");
  /* The top type (function) must have come from a declarator, not from a
     typedef (see constraints section of 3.7.1, and associated footnote). */
  if (func_info->function_type_from_typedef) {
    error(ec_function_type_must_come_from_declarator);
    /* Build a copy of the routine type that can be used below, to avoid
       further error recovery problems, and because we need a non-shared
       routine type entry that we can modify. */
    /* The type was probably from a typedef, so skip past that. */
    rout_type = skip_typerefs(rout_type);
    check_assertion(rout_type->kind == (a_type_kind)tk_routine);
    unqualified_rout_type = alloc_type((a_type_kind)tk_routine);
    copy_routine_type_with_param_types(rout_type, unqualified_rout_type);
    rout_type = unqualified_rout_type;
  } else {
    unqualified_rout_type = make_unqualified_type(rout_type);
    check_assertion(unqualified_rout_type->kind == (a_type_kind)tk_routine);
  }  /* if */
  extra_info = unqualified_rout_type->variant.routine.extra_info;
  prototyped = extra_info->prototyped;
  /* Force the storage class of a function with a body to unspecified
     (meaning external) or either static or inline (meaning internal);
     i.e., change extern to unspecified. */
  if (storage_class == (a_storage_class)sc_extern) {
    storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* Create the symbol entry and routine entry for the routine. */
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->class_of_which_a_member != NULL) {
    /* This is the definition of a member function. */
    check_assertion(prototyped);
    is_member_function_def = TRUE;
    define_member_function(locator, rout_type, func_info, &symbol_ptr,
                           &linkage, &old_type, &ext_sym);
  } else {
    if (!prototyped) {
      /* Old-style id list.  Before calling decl_var_or_routine scan the
         parameter declarations.  It is important for the routine type to
         include all the parameter information in order to do overloading
         involving both prototyped and old-style functions. */
      a_param_type_ptr   old_style_param_types = NULL;
      a_param_type_ptr   end_old_style_param_types = NULL;

      /* Push the name scope for the parameter declarations. */
      (void)push_scope((a_scope_kind)sck_func_prototype,
                       func_info->scope_number, rout_type,
                       (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                       (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
      /* Remember the scope number for later use when the body is scanned. */
      func_info->scope_number = scope_stack[depth_scope_stack].number;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      ss_entry_start_prev = init_param_source_sequence_sublist();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      while (curr_token == tok_identifier ||
             is_decl_start(/*expr_context=*/FALSE,
                           /*real_declarator_allowed=*/TRUE)) {
        /* This declaration is checked to make sure the identifier is on the
           param_id_list. */
        declaration(/*function_definition_allowed=*/FALSE, 
                    /*extern_implied=*/FALSE,
                    /*is_old_style_param_decl=*/TRUE,
                    /*is_top_level_declaration=*/FALSE, 
                    func_info->param_id_list);
      }  /* while */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      terminate_param_source_sequence_sublist(func_info, ss_entry_start_prev);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Scan the list of identifiers, assigning types to any that remain
         undeclared, and create the param type entries. */
      for (param_id = func_info->param_id_list;
           param_id != NULL;
           param_id = param_id->next) {
        if (param_id->type == NULL) {
          /* Enter any undeclared parameters with a type of int. */
          param_id->type = integer_type((an_integer_kind)ik_int);
          param_id->storage_class = (a_storage_class)sc_auto;
          param_id->implicitly_declared = TRUE;
          copy_source_position(param_id->symbol->decl_position,
                               param_id->type_pos);
          /* Symbols for explicitly declared parameters will already have been
             entered into the symbol table; so the same for parameters that
             are implicitly declared. */
          reenter_symbol(param_id->symbol, decl_scope_level,
                         /*suppress_error=*/FALSE);
        }  /* if */
        /* The param_type entry must be allocated in the file-scope
           region. */
        ptp = make_param_type(param_id->type, &param_id->type_pos);
        /* Now build the list of parameter types that is attached to the 
           routine type (needed for checking type compatibility -- see
           types_are_compatible). */
        if (old_style_param_types == NULL) {
          old_style_param_types = ptp;
        } else {
          end_old_style_param_types->next = ptp;
        }  /* if */
        end_old_style_param_types = ptp;
      }  /* for */
      /* Set the type to the new type information from the old-style
         parameters just scanned. */
      extra_info->param_type_list = old_style_param_types;
      extra_info->prototyped = FALSE;
      extra_info->old_style_params_scanned = TRUE;
      /* Parameter symbols are not actually entered in the function
         prototype scope, but other symbols (in consequence of an error or
         a type declaration) may be.  Record them so that they can be
         transferred to the function scope later. */
      func_info->prototype_scope_symbols =
                                      scope_stack[depth_scope_stack].symbols;
      /* Process pragmas associated with the opening brace before the current
         scope is popped.  This means, for old-style param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the function prototype scope; it's different for prototyped
         param lists. */
      process_curr_token_pragmas();
      /* Pop the function prototype scope. */
      pop_scope();
    } else {
      /* Prototyped. */
      /* Process pragmas associated with the opening brace before pushing
         the function scope.  This means, for prototyped param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the file; it's different for old-style param lists. */
      process_curr_token_pragmas();
    }  /* if */
    /* Create the symbol entry and routine entry for the routine. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    declarator_ssep = func_info->declarator_ssep;
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    declarator_ssep = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    decl_var_or_routine(locator, storage_class, rout_type, func_info,
                        declarator_ssep, (SRK_DECLARATION | SRK_DEFINITION),
                        &symbol_ptr, &linkage,
                        &old_type, &ext_sym);
  }  /* if */
  routine_ptr = symbol_ptr->variant.routine.ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* The following check cannot be done, since the type may end up being
     copied. */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
  check_assertion(make_unqualified_type(routine_ptr->type) ==
                                                      unqualified_rout_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!is_member_function_def &&
      storage_class == (a_storage_class)sc_unspecified &&
      routine_ptr->source_corresp.name != NULL &&
      strcmp(routine_ptr->source_corresp.name, "main") == 0) {
    /* This is "main", so remember the location of its routine entry. */
    il_header.main_routine = routine_ptr;
  }  /* if */
  /* Scan the function body. */
  flags = SFB_NO_FLAGS;
  if (!has_explicit_type_specifier) {
    flags |= SFB_IMPLICITLY_DECLARED_RETURN_TYPE;
  }  /* if */
  if (!prototyped) {
     flags |= SFB_OLD_STYLE_PARAM_DECL;
  }  /* if */
  scan_function_body(routine_ptr, func_info, flags);
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  /* Save the symbol associated with the most recent constructor or
     destructor for which a definition was supplied outside of the
     class definition.  Clear this value when any other member function
     is processed.  This is used to emulate a cfront name lookup bug.
     See check_for_cfront_name_lookup_bug in symbol_tbl.c for more
     information. */
  if (is_member_function_def) {
    if (cfront_2_1_mode) {
      if (routine_ptr->special_kind ==
			 (a_special_function_kind)sfk_constructor ||
          routine_ptr->special_kind ==
			 (a_special_function_kind)sfk_destructor) {
        last_ctor_or_dtor_sym = symbol_ptr;
      } else {
        last_ctor_or_dtor_sym = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

  db_exit();
  return;
}  /* function_definition */


static void make_default_constructor_body(a_scope_ptr  scope)
/*
Create the body for a default constructor or a default copy constructor.  It
will return a pointer to the constructed object.
*/
{
  a_routine_ptr                  rp;
  a_statement_ptr                sp;
  a_routine_type_supplement_ptr  rtsp;
  a_variable_ptr                 vp;
  a_param_type_ptr               ptp;

  db_enter(4, "make_default_constructor_body");
  rp = scope->variant.routine.ptr;
  /* Create the parameter variable -- needed for copy constructors only. */
  rtsp = (skip_typerefs(rp->type))->variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  if (ptp != NULL) {
    vp = make_parameter(ptp->type, (a_storage_class)sc_auto,
                        (a_symbol_ptr)NULL);
    vp->assoc_param_type = ptp;
  }  /* if */    
  /* Create entries describing constructions to be done in the wrapper code. */
  scope->variant.routine.constructor_inits =
                                  ctor_initializer(rp, /*user_defined=*/FALSE);
  /* Create a statement block that is empty except for the return statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements = sp =
          alloc_statement((a_statement_kind)stmk_return);
  sp->expr = this_param_value_expr();
  db_exit();
}  /* make_default_constructor_body */


static void make_default_destructor_body(a_scope_ptr  scope)
/*
Create the body for a default destructor.  It will return no value.
*/
{
  a_routine_ptr rp;

  db_enter(4, "make_default_destructor_body");
  rp = scope->variant.routine.ptr;
  /* Create entries describing destructions to be done in the wrapper code. */
  scope->variant.routine.constructor_inits = dtor_initializer(rp);
  /* Create a statement block that is empty except for the return
     statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements =
          alloc_statement((a_statement_kind)stmk_return);
  db_exit();
}  /* make_default_destructor_body */


static a_boolean is_virtual_base_class_of(a_type_ptr  base_class_type,
                                          a_type_ptr  derived_type)
/*
Return TRUE if base_class_type is a virtual base class of derived_type.
*/
{
  a_base_class_ptr  bcp;

  /* Loop through the base classes. */
  for (bcp = base_classes_of(derived_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->type == base_class_type) {
      /* Found it if it's virtual. */
      if (!bcp->is_virtual) bcp = NULL;
      break;
    }  /* if */
  }  /* for */
  /* Return TRUE if we found it. */
  return (bcp != NULL);
}  /* is_virtual_base_class_of */


static a_boolean virtual_base_class_is_indirect(a_base_class_ptr  vbcp,
                                                a_type_ptr        class_type)
/*
vbcp points to a virtual direct base class of the current class (class_type).
Return TRUE if it is also an indirect base class of the current class -- i.e.,
if at least one direct base class of the current class is virtually derived
from the same class as the one with which vbcp is associated.
*/
{
  a_base_class_ptr  bcp;
  a_boolean         is_indirect = FALSE;

  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (is_virtual_base_class_of(vbcp->type, bcp->type)) {
      is_indirect = TRUE;
      break;
    }  /* if */
  }  /* for */
  return is_indirect;
}  /* virtual_base_class_is_indirect */


static a_routine_ptr select_assignment_operator(
                                    a_type_ptr            class_type,
                                    a_type_qualifier_set  required_qualifiers,
                                    a_source_position     *err_pos,
                                    a_boolean             *pass_by_value)
/*
Return a pointer to the routine entry for the current class's default
assignment operator.
*/
{
  a_symbol_ptr    sym, opass_sym = NULL;
  a_boolean       is_overloaded_function;
  a_boolean       ambiguous = FALSE;
  a_boolean       opass_sym_matches_exactly = FALSE;
  a_routine_ptr   opass_routine;

  db_enter(4, "select_assignment_operator");
  sym = symbol_supplement_for_class(class_type)->assignment_operator;
  /* If sym is an overloaded function symbol we need to go through the whole
     list. */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_overloaded_function = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  } else {
    is_overloaded_function = FALSE;
  }  /* if */
  /* Find an assignment operator whose argument is ref-class (pass by
     reference) or class (pass_by_value). */
  for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
    a_boolean             sym_matches_exactly;
    a_boolean             is_ref_arg;
    a_type_qualifier_set  qualifiers = TQ_NONE;

    if (is_assignment_operator_for_copy(sym, &is_ref_arg, &qualifiers)) {
      /* Found an assignment operator that can copy the current class. */
      if (!is_ref_arg) {
        /* Not a reference type, so qualifiers are ignored. */
        sym_matches_exactly = TRUE;
      } else {
        /* Reference type. */
        if ((required_qualifiers & qualifiers) != required_qualifiers) {
          /* No match -- keep looking. */
          continue;
        } else if (required_qualifiers == qualifiers) {
          /* It's an exact match. */
          sym_matches_exactly = TRUE;
        } else {
          /* It's not quite an exact match. */
          sym_matches_exactly = FALSE;
        }  /* if */
      }  /* if */
      if (opass_sym != NULL) {
        /* We have a match on this symbol, but we've already had one before
           as well.  If one but not the other is an exact match, take the
           one that matches.  Otherwise it's an ambiguity.  */
        ambiguous = (sym_matches_exactly == opass_sym_matches_exactly);
        if (!sym_matches_exactly) continue;
      }  /* if */
      opass_sym = sym;
      opass_sym_matches_exactly = sym_matches_exactly;
      *pass_by_value = !is_ref_arg;
    }  /* if */
  }  /* for */
  opass_routine = NULL;
  if (opass_sym == NULL) {
    /* No applicable assignment operator function. */
    if (required_qualifiers == TQ_CONST) {
      /* The common case:  missing const assignment operator function. */
      pos_ty_error(ec_missing_const_assignment_operator, err_pos, class_type);
    } else {
      /* Unusual case: volatile or const-volatile expected. */
      pos_ty_error(ec_no_suitable_assignment_operator, err_pos, class_type);
    }  /* if */
  } else {
    if (ambiguous) {
      /* More than one applicable assignment operator function. */
      pos_ty_error(ec_ambiguous_assignment_operator, err_pos, class_type);
    } else {
      /* Exactly one assignment operator function is best. */
      /* Check that the function is accessible and mark it referenced. */
      reference_to_implicitly_invoked_function(
                                  opass_sym, err_pos, (a_type_ptr)NULL,
                                  /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                                  /*suppress_access_check=*/FALSE);
    }  /* if */
    opass_routine = opass_sym->variant.routine.ptr;
  }  /* if */
  db_exit();
  return opass_routine;
}  /* select_assignment_operator */


static a_statement_ptr make_assignment_call(an_expr_node_ptr  source_expr,
                                            an_expr_node_ptr  dest_expr,
                                            a_routine_ptr     rp,
                                            a_boolean         pass_by_value,
                                            a_source_position *err_pos)
/*
Return a statement pointer that represents a call to an assignment operator.
source_expr points to the node that is the source of the assignment; it may
require additional modification.  dest_expr points to the destination node.
rp is the pointer to the routine entry for the assignment operator.
pass_by_value is TRUE if the source_expr is passed by value, FALSE if it is
passed by reference.  *err_pos is the source position for diagnostics.
*/
{
  a_param_type_ptr  ptp;
  a_type_ptr        tp;
  a_statement_ptr   sp;

  /* Get the first parameter of the assignment operator, which represents the
     source type. */
  ptp = skip_typerefs(rp->type)->variant.routine.extra_info->param_type_list;
  if (pass_by_value) {
    source_expr = add_indirection_to_node(source_expr);
    /* Make sure a copy constructor call is added if one is needed. */
    source_expr = prep_rvalue_arg_expr(source_expr, ptp, err_pos);
  } else {
    /* If the assignment operator takes an argument that is a base class
       instead of the current class we need a cast. */
    tp = ptp->type;
    if (is_reference_type(tp)) {
      /* Change the reference type to a pointer type. */
      tp = make_pointer_type(type_pointed_to(tp));
    }  /* if */
    cast_node(&source_expr, tp, /*is_implicit_cast=*/TRUE, err_pos);
  }  /* if */
  sp = make_call_assignment_statement(rp, dest_expr, source_expr, err_pos);
  return sp;
}  /* make_assignment_call */


static void make_default_assignment_body(a_scope_ptr        scope,
                                         a_source_position  *err_pos)
/*
Create the body for a default assignment operator.  Typically it will
entail a series of member-wise and base-class-wise assignment operations:
based on the properties of the subobject, it will either call an assignment
operator routine or do bitwise assignment.
*/
{
  a_type_ptr                     class_type, tp, array_type;
  a_routine_type_supplement_ptr  rtsp;
  a_statement_ptr                sp;
  a_statement                    head_of_statement_list;
  a_variable_ptr                 source_var;
  an_expr_node_ptr               source_expr, dest_expr;
  a_base_class_ptr               bcp;
  a_field_ptr                    fp;
  a_routine_ptr                  rp;
  a_symbol_ptr                   sym;
  a_boolean                      pass_by_value;
  a_type_qualifier_set           qualifiers;
  a_param_type_ptr               ptp;
  a_boolean                      bitwise_assign;

  db_enter(4, "make_default_assignment_body");
  /* The source variable of the copy is the first parameter on the parameters
     list for the routine.  There must be exactly one parameter for an
     assignment function. */
  rtsp = (skip_typerefs(scope->variant.routine.ptr->type))->
                                                  variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  source_var = make_parameter(ptp->type, (a_storage_class)sc_auto,
                              (a_symbol_ptr)NULL);
  source_var->assoc_param_type = ptp;
  class_type =
          type_pointed_to(scope->variant.routine.this_param_variable->type);
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* See if a bitwise copy is all that is called for. */
  if (symbol_supplement_for_class(class_type)->
                   assignment_by_bitwise_copy_allowed) {
    /* Yes.  (Then why are we defining a routine?  Probably because the
       address of the default assignment operator was taken, forcing the
       actual creation of the routine.) */
    /* Get the source and destination expressions to use as operands for an
       assignment statement. */
    source_expr = var_rvalue_expr(source_var);
    dest_expr = this_param_value_expr();
    sp = sp->next = make_assignment_statement(dest_expr, source_expr);
  } else {
    /* Memberwise copy is required.  That is, first do the appropriate
       operation on each direct base class (direct assignment or calling
       the base class's assignment function), and then do the appropriate
       copy of each member. */
    if (is_const_qualified_type(type_pointed_to(source_var->type))) {
      qualifiers = TQ_CONST;
    } else {
      qualifiers = TQ_NONE;
    }  /* if */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        /* We are only interested in direct base classes. */
        if (bcp->is_virtual &&
            virtual_base_class_is_indirect(bcp, class_type)) {
          /* If it is also an indirect base class, it will be handled by
             the assignment function of some other base class. */
          continue;
        }  /* if */
        /* The destination is always the implicit "this" parameter cast to
           the appropriate base class. */
        dest_expr = base_class_selection_expr(this_param_value_expr(), bcp);
        /* The source is the first parameter cast to the same base class. */
        source_expr = base_class_selection_expr(var_rvalue_expr(source_var),
                                                bcp);
        if (symbol_supplement_for_class(bcp->type)->
                         assignment_by_bitwise_copy_allowed) {
          /* A bitwise copy may be performed. */
          /* Dereference the pointer-to-base-class. */
          source_expr = add_indirection_to_node(source_expr);
          /* Create the assignment statement.  The appropriate operator
             will be selected by the function. */
          sp = sp->next = make_assignment_statement(dest_expr, source_expr);
        } else {
          /* A bitwise copy may not be done.  Find the default assignment
             operator and put out a call to it. */
          rp = select_assignment_operator(bcp->type, qualifiers,
                                          &bcp->decl_position, &pass_by_value);
          if (rp == NULL) {
            /* Error has already been issued in the subroutine. */
            continue;
          }  /* if */
          sp = sp->next = make_assignment_call(source_expr, dest_expr, rp,
                                               pass_by_value, err_pos);
        }  /* if */
      }  /* if */
      /* Advance to the next base class. */
    }  /* for */
    /* Now go through all the fields, copying them one at a time.  Use the
       symbol list rather than the field list to be sure we adhere to
       declaration order and to be sure only user defined fields are
       copied. */
    sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                           variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        /* A field. */
        fp = sym->variant.field.ptr;
        tp = skip_typerefs(fp->type);
        if (is_const_qualified_type(tp) || is_reference_type(tp)) {
          /* The error has already been issued for const and ref members.
             Don't bother trying to do the copy. */
          check_assertion(total_errors > 0);
          continue;
        }  /* if */
        /* If this is an array, we need the element type. */
        if (is_array_type(tp)) {
          array_type = tp;
          tp = skip_typerefs(underlying_array_element_type(tp));
        } else {
          array_type = NULL;
        }  /* if */
        /* The destination is the appropriate field (lvalue) of the "this"
           parameter. */
        dest_expr = field_lvalue_selection_expr(this_param_value_expr(), fp);
        /* The source will be the appropriate field of the first argument,
           but we don't know yet whether it's an lvalue or an rvalue. */
        source_expr = var_rvalue_expr(source_var);
        if (is_class_struct_union_type(tp)) {
          /* It's a class type, so we may have to call an assignment operator
             function. */
          if (symbol_supplement_for_class(tp)->
                           assignment_by_bitwise_copy_allowed) {
            /* A bitwise copy may be performed. */
            bitwise_assign = TRUE;
          } else {
            a_statement_ptr call_stmt;
            /* A bitwise copy may not be done.  Find the default assignment
               operator and put out a call to it. */
            bitwise_assign = FALSE;
            rp = select_assignment_operator(tp, qualifiers,
                                            &fp->source_corresp.decl_position,
                                            &pass_by_value);
            if (rp == NULL) {
              /* Error has already been issued in the subroutine. */
              continue;
            }  /* if */
            source_expr = field_lvalue_selection_expr(source_expr, fp);
            if (array_type != NULL) {
              /* Copying an array of classes.  Generate a loop around the
                 call of the assignment routine, like
                   tmp = 0;
                   do {
                     assignfunc(&dest[tmp], &src[tmp]);
                   } while (++tmp < num_elements);
              */
              a_variable_ptr   temp_var;
              an_expr_node_ptr temp_node, temp_incr_node, compare_node;
              a_type_ptr       size_t_type;
              a_targ_size_t    num_elems;

              size_t_type = integer_type(targ_size_t_int_kind);
              temp_var = alloc_temporary_variable(size_t_type);
              /* Make "tmp = 0;" */
              temp_node = var_lvalue_expr(temp_var);
              sp = sp->next =
                make_assignment_statement(temp_node,
                                          node_for_integer_constant(
                                                    0L, targ_size_t_int_kind));
              /* Make "++tmp < num_elements". */
              temp_node = var_lvalue_expr(temp_var);
              temp_incr_node = make_operator_node(
                                          (an_expr_operator_kind)eok_ipre_incr,
                                          size_t_type, temp_node);
              num_elems = skip_typerefs(array_type)->size / tp->size;
              temp_incr_node->next = 
                               node_for_integer_constant((long)num_elems,
                                                         targ_size_t_int_kind);
              compare_node =
                      make_operator_node((an_expr_operator_kind)eok_ilt,
                                         integer_type((an_integer_kind)ik_int),
                                         temp_incr_node);
              /* Make the do-while statement. */
              sp = sp->next = alloc_statement(
                                        (a_statement_kind)stmk_end_test_while);
              sp->expr = compare_node;
              /* Convert the source and destination expressions from
                 pointer-to-array to pointer-to-array-element. */
              cast_node(&source_expr, make_pointer_type(tp),
                        /*is_implicit_cast=*/TRUE, err_pos);
              cast_node(&dest_expr, make_pointer_type(tp),
                        /*is_implicit_cast=*/TRUE, err_pos);
              /* Add the subscript to the source_expr. */
              source_expr->next = var_rvalue_expr(temp_var);
              source_expr =
                      make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         source_expr->type, source_expr);
              /* Add the subscript to the dest_expr. */
              dest_expr->next = var_rvalue_expr(temp_var);
              dest_expr =
                      make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         dest_expr->type, dest_expr);
            }  /* if */
            call_stmt = make_assignment_call(source_expr, dest_expr, rp,
                                             pass_by_value, err_pos);
            if (array_type != NULL) {
              /* Array case; the call goes under the do-while. */
              sp->variant.loop_statement = call_stmt;
            } else {
              /* Non-array case; the call goes at the end of the statement
                 sequence. */
              sp = sp->next = call_stmt;
            }  /* if */
          }  /* if */
        } else {
          /* Not a class type.  Just do a bitwise copy. */
          bitwise_assign = TRUE;
        }  /* if */
        if (bitwise_assign) {
          /* Do a bitwise assignment. */
          if (array_type != NULL) {
            /* Array type.  Do a special assignment (source operand is an
               address). */
            source_expr = field_lvalue_selection_expr(source_expr, fp);
            sp = sp->next =
                       make_array_assignment_statement(dest_expr, source_expr);
        
          } else {
            /* Not an array.  The appropriate IL operator will be selected
               by make_assignment_statement. */
            source_expr = field_rvalue_selection_expr(source_expr, fp);
            sp = sp->next = make_assignment_statement(dest_expr, source_expr);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Make the return statement.  A pointer to the variable assigned to is
     the return value. */
  sp = sp->next = alloc_statement((a_statement_kind)stmk_return);
  sp->expr = this_param_value_expr();
  /* We now have a list of one or more statements hanging off the local
     variable head_of_statement_list.  The start of the list is pointed to
     by the next field.  Create a block statement and attach the list to
     it. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements = head_of_statement_list.next;
  db_exit();
  return;
}  /* make_default_assignment_body */


static void check_default_assignment_operator(a_type_ptr         class_type,
                                              a_source_position  *err_pos)
/*
Issue an error if a compiler-generated assignment operator is not allowed
because the class has a const or ref member (ARM 12.8).  The case of a
member or a base class with a nonpublic operator=() is handled elsewhere.
*/
{
  a_boolean     err, is_ref, is_const;
  a_symbol_ptr  sym;
  a_type_ptr    tp;

  db_enter(4, "check_default_assignment_operator");
  if (class_type->variant.class_struct_union.any_const_member ||
      symbol_supplement_for_class(class_type)->any_ref_member) {
    /* An error is issued only if a immediate member of the class is const or
       ref.  Those in base classes or embedded within members are diagnosed
       elsewhere. */
    err = FALSE;
    /* Go through all the fields, using the symbol list rather than the field
       list to be sure only user defined fields are checked and to be sure
       anonymous union fields are picked up. */
    sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                           variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        tp = sym->variant.field.ptr->type;
        is_ref = is_const = FALSE;
        if (is_reference_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             ref type. */
          is_ref = TRUE;
        } else if (is_const_qualified_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             const type. */
          is_const = TRUE;
        }  /* if */
        if (is_ref || is_const) {
          if (!err) {
            /* Multi-line diagnostic has not been started yet. */
            pos_start_error(ec_bad_default_assignment, err_pos);
          }  /* if */
          sym_add_diag_info(is_ref ? ec_reference_member : ec_const_member,
                            sym);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* for */
    if (err) end_error();
  }  /* if */
  db_exit();
}  /* check_default_assignment_operator */


static void define_special_member_function(a_routine_ptr  rout_ptr)
/*
Define a compiler generated routine for a member function (constructor or
destructor).  This entails creating a new memory region, a scope, and an
empty statement block.
*/
{
  a_scope_ptr                    scope;
  a_type_ptr                     class_type;
  a_routine_type_supplement_ptr  rtsp;
  a_source_position              *err_pos;
  an_object_lifetime_ptr         saved_curr_object_lifetime;

  db_enter(4, "define_special_member_function");
  class_type = rout_ptr->source_corresp.class_of_which_a_member;
  if (symbol_supplement_for_class(class_type)->is_nonreal_class) {
    /* Don't bother generating the definition for a member of an unreal
       instantiation of a template class. */
  } else {
    /* Save the current object lifetime stack. */
    saved_curr_object_lifetime = curr_object_lifetime;
    curr_object_lifetime = scope_stack[DEPTH_OF_FILE_SCOPE].il_scope->lifetime;
    /* Push a class symbol reactivation scope, to make class member names
       visible for processing the function definition. */
    push_class_reactivation_scope(class_type);
    /* Push the scope for the new function itself. */
    scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                       (a_type_ptr)NULL, rout_ptr, (a_symbol_ptr)NULL,
                       (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
    /* Associate the scope to the routine entry and the routine entry to its
       type entry. */
    rout_ptr->assoc_scope = curr_il_region_number;
    rtsp = rout_ptr->type->variant.routine.extra_info;
    rtsp->assoc_routine = rout_ptr;
    if (rtsp->implicit_this_param_type != NULL) {
      a_variable_ptr  vp = make_param_variable(rtsp->implicit_this_param_type,
                                               (a_storage_class)sc_auto);
      vp->is_this_parameter = TRUE;
      scope->variant.routine.this_param_variable = vp;
    }  /* if */
    /* Enter the constructor and destructor initializers, to record possible
       implicit initializers. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      make_default_constructor_body(scope);
    } else if (rout_ptr->special_kind ==
                                  (a_special_function_kind)sfk_destructor) {
      make_default_destructor_body(scope);
    } else {
      /* Assignment operator case. */
      check_assertion(rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_operator &&
                      rout_ptr->opname_kind == (an_opname_kind)onk_assign);
      err_pos = &class_type->source_corresp.decl_position;
      check_default_assignment_operator(class_type, err_pos);
      make_default_assignment_body(scope, err_pos);
    }  /* if */
    /* End of statement block is unreachable because of the return
       statement. */
    scope->assoc_block->
                   variant.block.extra_info->end_of_block_reachable = FALSE;
    /* Terminate the function scope. */
    pop_scope();
    /* Terminate the class reactivation scope. */
    pop_class_reactivation_scope();
    /* Restore the current object lifetime stack. */
    curr_object_lifetime = saved_curr_object_lifetime;
    /* Mark the symbol for this routine "defined". */
    ((a_symbol_ptr)rout_ptr->source_corresp.assoc_info)->defined = TRUE;
  }  /* if */
  db_exit();
}  /* define_special_member_function */


void force_definition_of_compiler_generated_routine(a_routine_ptr  rp)
/*
If rp points to a compiler-generated routine that is being referenced and
whose definition has not yet been generated, force the definition now.
*/
{
  a_special_function_kind  skind = rp->special_kind;

  if (rp->compiler_generated && rp->assoc_scope == NULL_region_number) {
    /* Only force a definition for constructors, destructors, and
       operator= functions.  In particular, do not try to define operator new
       and delete functions. */
    if (skind == (a_special_function_kind)sfk_constructor ||
        skind == (a_special_function_kind)sfk_destructor  ||
        (skind == (a_special_function_kind)sfk_operator &&
         rp->opname_kind == (an_opname_kind)onk_assign)) {
      define_special_member_function(rp);
    }  /* if */
  }  /* if */
}  /* force_definition_of_compiler_generated_routine */


void generate_required_virtual_destructor_bodies(a_type_ptr  types_list)
/*
Go through the classes on types_list and generate bodies for virtual
destructors, as required.
*/
{
  a_type_ptr                     tp;
  a_routine_ptr                  rp;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;

  db_enter(3, "generate_required_virtual_destructor_bodies");
  for (tp = types_list; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp->assoc_scope == NULL) {
        /* Class has no definition. */
      } else if (tp->source_corresp.assoc_info == NULL) {
        /* Class has no tag symbol.  This serves to eliminate types
           generated by IL lowering. */
      } else {
        cssp = symbol_supplement_for_class(tp);
        if (cssp->destructor != NULL) {
          rp = cssp->destructor->variant.routine.ptr;
          if (rp->is_virtual && rp->compiler_generated &&
              rp->assoc_scope == NULL_region_number) {
            /* The destructor for the current class is virtual and was
               generated automatically but does not yet have a body. */
            if (virtual_dtor_should_be_generated_for_class(tp)) {
              /* But the body for it should be generated, e.g., because the
                 virtual function table in which its address will appear is
                 being generated. */
              define_special_member_function(rp);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Do the same check for nested classes, if any. */
        generate_required_virtual_destructor_bodies(ctsp->assoc_scope->types);
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* generate_required_virtual_destructor_bodies */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
