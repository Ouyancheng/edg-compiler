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

templates.c -- Support for C++ templates.

*/

#include "basics.h"
#include "templates.h"
#include "cmd_line.h"
#include "decl_inits.h"
#include "decls.h"
#include "error.h"
#include "il.h"
#include "lexical.h"
#include "statements.h"
#include "symbol_tbl.h"
#include "types.h"


static a_boolean instantiation_in_progress(a_type_ptr tp)
/*
Return TRUE if a class/struct/union scope for tp, which represents a template
class, is currently on the scope stack.  If it is, that means an instantiation
for tp is currently in progress.
*/
{
  a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
  a_boolean               found = FALSE;

  /* Loop through the scope stack. */
  for (; ssep != &scope_stack[0]; ssep--) {
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        ssep->il_scope->variant.assoc_type == tp) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* instantiation_in_progress */


void instantiate_template_class(a_type_ptr  tp)
/*
This routine should be called from check_for_uninstantiated_template_class,
which determines that tp is an incomplete type.  If it also turns out to
be a template type, this routine attempts to instantiate it; it might not be
able to if the template itself has not yet been defined.
*/
{
  a_symbol_ptr                      template_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     *p_token_cache;
  a_class_symbol_supplement_ptr     cssp;

  db_enter(3, "instantiate_template_class");
  if (is_array_type(tp)) tp = underlying_array_element_type(tp);
  if (tp != NULL && is_class_struct_union_type(tp)) {
    tp = skip_typerefs(tp);
    cssp = symbol_supplement_for_class(tp);
    template_sym = cssp->class_template;
    if (template_sym == NULL) {
      /* Not a class based on a class template. */
    } else if (cssp->is_nonreal_class) {
      /* Don't try to instantiate a template class without real template
         arguments. */
#if CHECKING
    } else if (cssp->is_specific_template_def) {
      internal_error("instantiate_template_class: is specific template def");
#endif /* CHECKING */
    } else {
      /* There is a class template from which to generate this class and its
         a real instantiation. */
      tssp = template_sym->variant.template.extra_info;
      p_token_cache = &tssp->body_token_cache;
      if (p_token_cache->first_token == NULL) {
        /* The template itself has not yet been defined.  The caller will
           issue an incomplete-type error. */
      } else if (instantiation_in_progress(tp)) {
        /* This particular template class (not just some other one based on
           the same template) is currently being instantiated. */
      } else if (tssp->pending_instantiations >= MAX_PENDING_INSTANTIATIONS) {
        /* This class instantiation occurs within the context of other
           instantiations of the same class template.  When the number of
           such instantantiations-in-progress exceeds a configuration
           constant value, we assume this to be runaway recursion -- for
           for instance (to give a rather unlikely example):
              template <class T, int I> class X {
                X<T,I+1> x;
              };
         */                
        type_error(ec_runaway_recursive_instantiation, tp);
        /* Give tp a size of 1 so it won't be treated as incomplete in
           subsequent processing. */
        tp->size = 1;
      } else {
        /* We proceed with the instantiation. */
        /* Increment the count of instantiations-in-progress for the current
           class template.  It will be decremented when the instantiation is
           complete. */
        ++(tssp->pending_instantiations);
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "instantiating: ");
          db_type(tp);
          db_symbol(template_sym, "\nbased on: ", 2);
        }  /* if */
#endif /* DEBUG */
        (void)push_scope((a_scope_kind)sck_template_instantiation,
                         tssp->declaration_scope, tp, (a_routine_ptr)NULL,
                         (a_function_instantiation_entry_ptr)NULL);
        rescan_reusable_cache(p_token_cache);
#if CHECKING
        if (curr_token != tok_lbrace && curr_token != tok_colon) {
          internal_error("instantiate_template_class: bad 1st token in cache");
        }  /* if */
#endif /* CHECKING */
        /* Scan the base specifiers list, if any, and the body of the class. */
        (void)scan_class_definition(tp, DEPTH_OF_FILE_SCOPE,
                                    /*is_local_class=*/FALSE,
                                    /*is_prototype_instantiation=*/FALSE);
        pop_scope();
        /* In the normal case the current token should be end_of_source,
           which was inserted to mark the end of the cached token stream.
           If necessary, keep flushing until end-of-source is found. */
        while (curr_token != tok_end_of_source) (void)get_token();
        /* Advance past the end-of-source token. */
        (void)get_token();
        /* Decrement the count of instantiations-in-progress for the current
           class template. */
        --(tssp->pending_instantiations);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* instantiate_template_class */


void instantiate_class_template(a_symbol_ptr  template_sym,
                                a_type_ptr    prototype_type)
/*
*/
{
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     *p_token_cache;

  db_enter(3, "instantiate_class_template");
  tssp = template_sym->variant.template.extra_info;
  p_token_cache = &tssp->body_token_cache;
#if CHECKING
  if (p_token_cache->first_token == NULL) {
    /* The template itself has not yet been defined. */
    internal_error("instantiate_class_template: bad cache");
  } else if (instantiation_in_progress(prototype_type)) {
    /* The template is currently being instantiated. */
    internal_error("instantiate_class_template: already being instantiated");
  };
#endif /* CHECKING */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(template_sym, "prototype instantiation of: ", 2);
  }  /* if */
#endif /* DEBUG */
  (void)push_scope((a_scope_kind)sck_template_instantiation,
                   tssp->declaration_scope, prototype_type,
                   (a_routine_ptr)NULL,
                   (a_function_instantiation_entry_ptr)NULL);
  rescan_reusable_cache(p_token_cache);
#if CHECKING
  if (curr_token != tok_lbrace && curr_token != tok_colon) {
    internal_error("instantiate_class_template: bad 1st token in cache");
  }  /* if */
#endif /* CHECKING */
  /* Scan the base specifiers list, if any, and the body of the class. */
  (void)scan_class_definition(prototype_type, DEPTH_OF_FILE_SCOPE,
                              /*is_local_class=*/FALSE,
                              /*is_prototype_instantiation=*/TRUE);
  pop_scope();
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token stream.
     If necessary, keep flushing until end-of-source is found. */
  while (curr_token != tok_end_of_source) (void)get_token();
  /* Advance past the end-of-source token. */
  (void)get_token();
  db_exit();
}  /* instantiate_class_template */


void instantiate_template_function(a_function_instantiation_entry_ptr  fiep)
/*
*/
{
  a_symbol_ptr                      rout_sym;
  a_routine_ptr                     rout_ptr;
  a_scope_ptr                       scope;
  a_type_ptr                        rout_type;
  a_routine_type_supplement_ptr     rtsp;
  a_template_symbol_supplement_ptr  tssp;
  a_param_id_ptr                    pip;
  a_param_type_ptr                  ptp;

  db_enter(3, "instantiate_template_function");
  rout_sym = fiep->routine_sym;
  rout_ptr = rout_sym->variant.routine.ptr;
#if CHECKING
  if (rout_ptr->assoc_scope != NULL_region_number) {
#if 0
    internal_error("instantiate_template_function: already has a body");
#else
    goto done;
#endif /* if 0 */
  }  /* if */
#endif /* CHECKING */
#if 0
#else
  /* TEMPORARY -- all template functions are put out as static for now -- to
     avoid problems with their being declared in multiple files. */
  rout_ptr->storage_class = (a_storage_class)sc_static;
  rout_ptr->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
#endif /* if 0 */
  rout_type = rout_ptr->type;
  rtsp = rout_type->variant.routine.extra_info;
  tssp = fiep->template_sym->variant.template.extra_info;
  rout_ptr->is_inline = tssp->variant.function.routine->is_inline;
  (void)push_scope((a_scope_kind)sck_template_instantiation,
                  tssp->declaration_scope, (a_type_ptr)NULL, rout_ptr, fiep);
  rescan_reusable_cache(&tssp->body_token_cache);

#if 0

  /* Call set_routine_calling_method_flag and set_arg_transfer_method_flag??
     This has already been done -- should it be done again? */

  /* Check return type -- is it complete?  Is it an uninstantiated class? */

#endif /* if 0 */

  if (rout_sym->class_of_which_a_member != NULL) {
    push_class_reactivation_scope(rout_sym->class_of_which_a_member);
  }  /* if */
  /* Push the name scope for the routine body. */
  scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, rout_ptr,
                     (a_function_instantiation_entry_ptr)NULL);
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  rout_ptr->assoc_scope = curr_il_region_number;
  rtsp->assoc_routine = rout_ptr;

#if 0
  if (tssp->func_info.prototype_scope_symbols != NULL) {
    reactivate_prototype_scope_symbols(
                    tssp->variant.function.func_info.prototype_scope_symbols);
  }  /* if */
#endif /* if 0 */

  /* For a member function create the implicit "this" param variable and
     set a pointer to it in the scope entry. */
  if (rtsp->implicit_this_param_type != NULL) {
    /* Routine is a nonstatic member function. */
    scope->variant.routine.this_param_variable =
                make_param_variable(rtsp->implicit_this_param_type,
                                    (a_storage_class)sc_auto);
  }  /* if */
  /* If appropriate, set the return value pointer variable in the scope
     entry.  This is a pointer to an implicit parameter specifying the
     storage provided by the caller into which to copy a class object that
     is returned by value. */
  make_return_value_pointer_variable(rout_type, scope);

  pip = tssp->variant.function.func_info.param_id_list;
  ptp = rtsp->param_type_list;
#if CHECKING
  if ((pip == NULL) != (ptp == NULL)) {
    internal_error("inline_function_definition: pip and ptp out of sync");
  }  /* if */
#endif /* CHECKING */
  for (; pip != NULL; pip = pip->next, ptp = ptp->next) {
    /* Declare each parameter identifier to have the associated type
       from the parameter type list. */
    decl_parameter(pip, ptp, /*template_instantiation=*/TRUE);
#if CHECKING
    if ((pip->next == NULL) != (ptp->next == NULL)) {
      internal_error(
              "instantiate_template_function: param_id and ptp out of sync");
    }  /* if */
#endif /* CHECKING */
  }  /* for */

  /* Set the assoc_param_type field in each of the parameter variables. */
  fixup_parameters(scope->variant.routine.parameters, rtsp->param_type_list);

  /* Special processing for constructors and destructors. */
  switch (rout_ptr->special_kind) {
    case sfk_constructor:
      /* If the current token is a ":", explicit initialization for the
         constructor follows, but even without an explicit initializer, any
         implicit initializers should be recorded. */
      scope->variant.routine.constructor_inits =
                                      ctor_initializer(rout_ptr,
                                                       /*user_defined=*/TRUE);
#if ASSIGNMENT_TO_THIS_ALLOWED
      /* Determine and remember the operator new() routine for the class. */
      set_class_assoc_operator_new_routine(
                             rout_ptr->source_corresp.class_of_which_a_member);
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
      break;
    case sfk_destructor:
      /* Record the destructors that are to be called implicitly when this
         destructor is executed. */
      scope->variant.routine.constructor_inits = dtor_initializer(rout_ptr);
      /* Determine and remember the operator delete() routine for the class. */
      set_class_assoc_operator_delete_routine(
                             rout_ptr->source_corresp.class_of_which_a_member);
      break;
    default:;
      /* No action. */
  }  /* switch */

#if 0
  /* A call to new_struct_stmt_stack should not be required. */
#endif /* if 0 */

  scope->assoc_block = compound_statement(/*at_function_level=*/TRUE,
                                          /*explicit_return_type=*/TRUE);

  /* Pop the function scope. */
  pop_scope();
  if (rout_sym->class_of_which_a_member != NULL) {
    pop_class_reactivation_scope();
  }  /* if */

#if 0
  /* The lint "argsused" and "varargs" flags are only applicable until
     the end of a function declaration. */
  lint_argsused_flag = FALSE;
  lint_varargs_count = NOT_LINT_VARARGS;
#endif /* if 0 */

  /* Check for the closing "}", not done in compound_statement.  Note that
     required_token is not called; if compound_statement returned on
     anything other than a right brace, it's because we should start parsing
     on this token. */
  if (curr_token != tok_rbrace) {
    pos_error(ec_exp_rbrace, &pos_curr_token);
  } else {
    (void)get_token();
  }  /* if */

  /* Pop the template instantiation scope. */
  pop_scope();
  /* In the normal case the current token should be end_of_source, which was
     inserted to mark the end of the cached token stream. If necessary, keep
     flushing until end-of-source is found. */
  while (curr_token != tok_end_of_source) (void)get_token();
  /* Advance past the end-of-source token. */
  (void)get_token();

#if 0
#else
  done:;
#endif /* if 0 */
  db_exit();
}  /* instantiate_template_function */


a_boolean equiv_template_arg_lists(a_template_arg_ptr  list1,
                                   a_template_arg_ptr  list2,
                                   a_boolean           is_func_template)
/*
Return TRUE if the two linked lists of template arguments for a given template
class or template function are equivalent -- that is, if corresponding type
arguments refer to the same type and corresponding constant arguments refer to
the same constant.  If is_func_template is TRUE, the lists will contain
only type arguments.
*/
{
  a_boolean           equiv;
  a_template_arg_ptr  arg1 = list1, arg2 = list2;

  db_enter(4, "equiv_template_arg_lists");
#if CHECKING
  /* There is no way to produce a NULL template argument list, so the real
     code doesn't need to check for that. */
  if (arg1 == NULL || arg2 == NULL) {
    internal_error("equiv_template_arg_lists: NULL arg list");
  }  /* if */
#endif /* CHECKING */
  /* Assume they are equivalent, until we find evidence to the contrary. */
  equiv = TRUE;
  /* Loop through both lists in step, comparing arguments. */
  do {
#if CHECKING
    if (is_func_template) {
      /* Nontype arguments are not allowed in function template arg lists. */
      if (!arg1->is_type || !arg2->is_type) {
        internal_error("equiv_template_arg_lists: nontype arg");
      }  /* if */
    } else {
    /* For a given class, argument lists should always have the same sequence
       of type and constant arguments. */
      if (arg1->is_type != arg2->is_type) {
        internal_error("equiv_template_arg_lists: arg inconsistency");
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    if (!is_func_template && !arg1->is_type) {
      /* Both are constant arguments.  If they are not identical, this is a
         mismatch. */
      if (!eq_constants(arg1->variant.constant, arg2->variant.constant)) {
        equiv = FALSE;
        break;
      }  /* if */
    } else {
      /* Both are type arguments.  If they are not identical, this is a
         mismatch. */
      if (!identical_types(arg1->variant.type, arg2->variant.type)) {
        equiv = FALSE;
        break;
      }  /* if */
    }  /* if */
    /* Advance to the next arguments in step. */
    arg1 = arg1->next;
    arg2 = arg2->next;
#if CHECKING
    /* For a given function argument lists should always be exactly the same
       length. */
    if ((arg1 == NULL) != (arg2 == NULL)) {
      internal_error("equiv_template_arg_lists: unequal arg list lengths");
    }  /* if */
#endif /* CHECKING */
  } while (arg1 != NULL);

  db_exit();
  return equiv;
}  /* equiv_template_arg_lists */


a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                 a_template_arg_ptr  *new_list,
                                 a_source_position   *source_pos)
/*
Given a symbol for a class template and a template argument list (that is,
a list of actual arguments), look for an existing class that is the
corresponding instantiation of the template.  If none is found, create
such an instantiation (i.e., allocate the type entry and create the
symbol, adding the latter to the instantiation list for the template).
Return the symbol that is found or newly created.

Note that this function does not fully instantiate a class template;
rather, when it creates a class type entry, it is for an incomplete type.
The full instantiation is done later, when it is clearly needed.  This
allows this kind of code to be handled correctly:

  template <class T> class X;  // class template X is not yet defined.
  X<int> *pxi;                 // declares a pointer to an instantiation of X
                               //   which is incomplete at this point.

The class template X may or may not be defined subsequently, and even if it
is the full instantiation of X<int> may not be needed.  This is consistent
with the handling of pointers to incomplete non-template classes:

  class Y;                     // Y is not yet defined.
  Y *py;                       // pointer to incomplete class is okay.

Note, moreover, that even if class template X were defined there would be
no need to actually instantiate X<int> in the example above.
*/
{
  a_symbol_ptr                      sym, prev_sym;
  a_template_arg_ptr                old_list;
  a_type_ptr                        class_type;
  a_template_symbol_supplement_ptr  tssp;
  a_template_arg_ptr                tap;

  db_enter(3, "find_template_class");
  /* Make a pass over the symbols representing instantiations of the class
     template. */
  tssp = class_template_sym->variant.template.extra_info ;
  sym = tssp->variant.class.instantiations;
  prev_sym = NULL;
  for (; sym != NULL; sym = sym->next) {
    /* Old list is the template argument list from a template class that has
       already been created.  See if the list passed in matches it. */
    old_list = sym->variant.type->
                     variant.class_struct_union.extra_info->template_arg_list;
    if (equiv_template_arg_lists(old_list, *new_list,
                                 /*is_func_template=*/FALSE)) {
      /* We've found a match.  Remove the found symbol from its current
         position in the instantiation list and add it to the front. */
      if (prev_sym != NULL) {
        prev_sym->next = sym->next;
        sym->next = tssp->variant.class.instantiations;
        tssp->variant.class.instantiations = sym;
      }
#if DEBUG
      if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
    prev_sym = sym;
  }  /* for */
  if (sym == NULL) {
    /* No match was found on the list, so do a partial instantiation of the
       template class based on the template arguments.  First create a symbol
       (but do not enter it into the symbol table, since class templates
       are always looked up through the template. */
    sym = make_template_class_symbol(class_template_sym, source_pos);
    /* Add the new symbol to the head of the instantiation list. */
    sym->next = tssp->variant.class.instantiations;
    tssp->variant.class.instantiations = sym;
    /* Now create a new type entry. */
    class_type = alloc_type(tssp->variant.class.type_kind);
    sym->variant.class_struct_union.type = class_type;
    /* If this is a "real instantiation" leave the type incomplete; it will
       become compilete when it is instantiated.  However, if it is based on
       template parameters and is therefore a "nonreal" instantiation, give it
       a size and alignment to permit it to pass through subsequent processing
       without causing spurious errors. */
    for (tap = *new_list; tap != NULL; tap = tap->next) {
      if (tap->is_type) {
        if (tap->variant.type->kind == (a_type_kind)tk_template_param) {
          sym->variant.class_struct_union.extra_info->is_nonreal_class = TRUE;
          break;
        }  /* if */
      } else {
        /* Constant case. */
#if 0
/* Oops -- do we have any way to look at a constant entry and recognize it to
   be a template parameter.  Bug here:  if there is no type param among the
   template parameters, the flag will remain set incorrectly. */
#endif /* if 0 */
      }  /* if */
    }  /* for */
    /* Record the argument list in the type.  It should be available in the
       IL at least for name generation and possibly for debuggers, too.  Note,
       however, that the type itself is not added to the scope types list
       until a full instantiation takes place -- or, if there is none, in
       pop_scope, as with ordinary classes. */
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = *new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    /* All template instantiations have C++ external linkage, but mark it as
       internally linked for now.  The name linkage will be fixed up later,
       along with nontemplate classes.  This assures uniform processing of
       members. */
    class_type->source_corresp.name_linkage =
                                        (a_name_linkage_kind)nlk_internal;
    if (sym->variant.class_struct_union.extra_info->is_nonreal_class) {
      class_type->size = 1;
      class_type->alignment = 1;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(class_template_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* We are reusing a class type that already exists, so *new_list will not
       be used.  Return the entries to the available list for reuse. */
    free_template_arg_list(*new_list);
    *new_list = NULL;
  }  /* if */
  db_exit();
  return sym;
}  /* find_template_class */


static a_type_ptr copy_type_with_substitution(
                                        a_type_ptr          type,
                                        a_template_arg_ptr  templ_arg_list,
                                        a_source_position   *source_pos)
/*
If "type", a pointer to a type entry, is a template-parameter type, return
the corresponding real type, based on the template argument list.  If "type"
contains a template-parameter type, return a copy with the substitution made.
If it involves no template-parameter type, simply return "type".
*/
{
  a_type_ptr                     new_type, tp, tp2;
  int                            i, reusable_param_types;
  a_template_arg_ptr             tap;
  a_type_ptr                     new_return_type;
  a_type_ptr                     first_new_type_for_param_types_list;
  a_param_type_ptr               ptp, new_ptp, prev_ptp;
  a_class_symbol_supplement_ptr  cssp;

  db_enter(5, "copy_type_with_substitution");
#if DEBUG
  if (debug_level >= 5) {
    fputs("in:  ", f_debug);
    db_type(type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  switch (type->kind) {
    case tk_template_param:
      /* If this template parameter type entry corresponds to the nth
         parameter, the real type to substitute for it is given in the nth
         template argument.  Find the template argument that matches this
         template parameter and return it to the caller. */
      tap = templ_arg_list;
      for (i = type->variant.list_position; i > 1; --i) {
        tap = tap->next;
      }  /* for */
#if CHECKING
      if (tap->variant.type == NULL) {
        internal_error("copy_type_with_substitution: NULL ptr in templ arg");
      }  /* if */
#endif /* CHECKING */
      new_type = tap->variant.type;
      break;
    case tk_pointer:
      /* Make a pointer type based on a copy (or reuse, if copying is not
         required) of the type pointed to. */
      tp = type->variant.pointer.type;
      tp = copy_type_with_substitution(tp, templ_arg_list, source_pos);
      if (type->variant.pointer.is_reference) {
        new_type = make_reference_type(tp);
      } else {
        new_type = make_pointer_type(tp);
      }  /* if */
      break;
    case tk_typeref:
      /* Make an identically qualified type of a copy (or reuse) of the type
         that underlies the typeref. */
      tp = copy_type_with_substitution(skip_typerefs(type), templ_arg_list,
                                       source_pos);
      new_type = make_identically_qualified_type(tp, type);
      break;
    case tk_ptr_to_member:
      /* Make a pointer to member type.  The current pointer to member type
         points to two types, so the new type is based on copies (or reuses)
         of each. */
      tp = copy_type_with_substitution(type->variant.ptr_to_member.type,
                                       templ_arg_list, source_pos);
      tp2 = copy_type_with_substitution(
                          type->variant.ptr_to_member.class_of_which_a_member,
                          templ_arg_list, source_pos);
      new_type = ptr_to_member_type(tp, tp2);
      break;
    case tk_routine:
      /* We can reuse "type" as long as we can reuse the return type and all
         its param types.  Otherwise we will need to allocate a new type entry.
         Go through "type" until we find that a new type was returned from
         copy_type_with_substitution. */
      reusable_param_types = 0;
      first_new_type_for_param_types_list = NULL;
      new_return_type = copy_type_with_substitution(
                                        type->variant.routine.return_type,
                                        templ_arg_list, source_pos);
      if (new_return_type != type->variant.routine.return_type) {
        /* A substitution was made on the return type, so a new routine type
           will be required. */
        goto make_new_type;
      }  /* if */
      /* Now examine each of the parameters. */
      for (ptp = type->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        tp = copy_type_with_substitution(ptp->type, templ_arg_list,
                                         source_pos);
        if (tp != ptp->type) {
          /* A substitution was made, so a new routine type will be required.
             Remember tp so we can avoid calling copy_type_with_substituion
             again for this param type entry. */
          first_new_type_for_param_types_list = tp;
          goto make_new_type;
        }  /* if */
        /* Keep track of the number of param type entries for which reuse of
           the existing type is okay. */
        ++reusable_param_types;
      }  /* for */
      /* Falling through to here means that no substitutions are required
         for this type.  Therefore it can simply be reused. */
      new_type = type;
      break;
make_new_type:
      /* Make a routine type based on "type".  Checking for reusable types has 
         already been done for the return type and possibly for some of the
         parameter types. */
      new_type = alloc_type((a_type_kind)tk_routine);
      /* Fill in the return type.  It has already been determined. */
      new_type->variant.routine.return_type = new_return_type;
      /* Clone the routine type supplement, except for the pointers. */
      *(new_type->variant.routine.extra_info) =
                                       *(type->variant.routine.extra_info);
      new_type->variant.routine.extra_info->assoc_routine = NULL;
      /* Make copies of the entries on type's param types list, making the
         appropriate substitutions for template parameter type entries. */
      prev_ptp = NULL;
      for (ptp = type->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        if (reusable_param_types > 0) {
          /* We have already called copy_type_with_substitution for this
             parameter and we know we can reuse the existing type. */
          tp = ptp->type;
          --reusable_param_types;
        } else if (first_new_type_for_param_types_list != NULL) {
          /* We have already called copy_type_with_substitution for this
             parameter and the type returned contained a substitution; we can
             use that type. */
          tp = first_new_type_for_param_types_list;
          first_new_type_for_param_types_list = NULL;
        } else {
          /* copy_type_with_substitution has not been called yet. */
          tp = copy_type_with_substitution(ptp->type, templ_arg_list,
                                           source_pos);
        }  /* if */
        /* Allocate the param type entry and copy default arg info. */
        new_ptp = alloc_param_type(tp);
        if (ptp->has_default_arg) {
          new_ptp->has_default_arg = TRUE;
          new_ptp->default_arg_expr = copy_expr_tree(ptp->default_arg_expr,
                                                     /*clone_temps=*/TRUE);
        }  /* if */
        /* Add the new param type entry to the param types list. */
        if (prev_ptp == NULL) {
          new_type->variant.routine.extra_info->param_type_list = new_ptp;
        } else {
          prev_ptp->next = new_ptp;
        }  /* if */
        prev_ptp = new_ptp;
      }  /* if */
      set_routine_calling_method_flag(new_type);
      /* A brand new type has been created -- add it to the file scope types
         list. */
      add_to_types_list(new_type, DEPTH_OF_FILE_SCOPE,
                          /*in_old_style_param_decl_list=*/FALSE);
      break;
    case tk_array:
      /* Make an array type based on "type", making substitutions as
         required in the element type.  Note that if the element type doesn't
         require substitution, we don't create a new type entry. */
      tp = copy_type_with_substitution(type->variant.array.element_type,
                                       templ_arg_list, source_pos);
      if (tp == type->variant.array.element_type) {
        /* Reuse the current type. */
        new_type = type;
      } else {
        /* Create a new array type. */
        tp2 = alloc_type((a_type_kind)tk_array);
        *tp2 = *type;
        tp2->variant.array.element_type = tp;
        new_type = tp2;
        add_to_types_list(new_type, DEPTH_OF_FILE_SCOPE,
                          /*in_old_style_param_decl_list=*/FALSE);
      }  /* if */
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      cssp = symbol_supplement_for_class(type);
      if (!cssp->is_nonreal_class) {
        /* Reuse the current type. */
        new_type = type;
#if CHECKING
      } else if (cssp->class_template == NULL) {
        internal_error(
                "copy_type_with_substitution: nonreal class with no template");
#endif /* CHECKING */
      } else {
        /* The class is a template. The copy will be an instantiation of it.
           Build a new template arg list and call find_template_class. */
        a_template_arg_ptr  new_list, new_tap, prev_new_tap;
        a_symbol_ptr        sym;

        tap = type->variant.class_struct_union.extra_info->template_arg_list;
        prev_new_tap = new_list = NULL;
        for (; tap != NULL; tap = tap->next) {
          new_tap = alloc_template_arg(tap->is_type);
          if (tap->is_type) {
            new_tap->variant.type =
                     copy_type_with_substitution(tap->variant.type,
                                                 templ_arg_list, source_pos);
          } else {
#if CHECKING
            if (tap->variant.constant->kind ==
#if 0
                                  (a_constant_repr_kind)ck_template_param
#else
                                  (a_constant_repr_kind)ck_error
#endif /* if 0 */
                                                                ) {
              internal_error("copy_type_with_subst: bad const in templ arg");
            }  /* if */
#endif /* CHECKING */
            new_tap->variant.constant = tap->variant.constant;
          }  /* if */
          if (new_list == NULL) {
            new_list = new_tap;
          } else {
            prev_new_tap->next = new_tap;
          }  /* if */
          prev_new_tap = new_tap;
        }  /* for */
        sym = find_template_class(cssp->class_template, &new_list, source_pos);
        new_type = sym->variant.class_struct_union.type;
      }  /* if */
      break;
    default:;
      /* No modification required. */
      new_type = type;
  }  /* switch */
#if DEBUG
  if (debug_level >= 5) {
    fputs("out: ", f_debug);
    db_type(new_type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return new_type;
}  /* copy_type_with_substitution */


a_boolean matches_template_type(a_type_ptr         type,
                                a_type_ptr         templ_type,
                                a_template_arg_ptr *templ_arg_list)
/*
Compare type and templ_type.  The latter is from a parameter list of a
function template (function params, not template params).  If the types are
identical, return TRUE.  If they are identical but for a template parameter,
return TRUE if the type is consistent with other uses of that template
parameter, as represented in the template argument list.  Otherwise, return
FALSE.  When for the nth template parameter, the nth template arg has not
yet been created, extend the template argument list to include n entries.
*/
{
  a_boolean                      match = FALSE;
  a_type_ptr                     tp, ttp;
  a_param_type_ptr               ptp, tptp;
  int                            i;
  a_template_arg_ptr             tap, prev_tap;
  a_class_symbol_supplement_ptr  templ_cssp;

  db_enter(5, "matches_template_type");
  if (templ_type->kind == (a_type_kind)tk_template_param) {
    /* A real type "matches" a template parameter type if it is identical to
       the real type, if any, that was previously associated with that
       template type. */
    /* For the nth template parameter find the nth template argument.  If
       the nth template argument hasn't been created yet, create it along
       with all missing template args that should precede it in the linked
       list. */
    prev_tap = NULL;
    for (i = templ_type->variant.list_position; i > 0; --i) {
      if (prev_tap == NULL) {
        /* This must be the first time through the loop. */
        tap = *templ_arg_list;
      } else {
        /* Not the first iteration. */
        tap = prev_tap->next;
      }  /* if */
      /* If the template arg doesn't exist yet, create it and add it to the
         list.  Note that some of the template args on the list will have
         NULL type pointers. */
      if (tap == NULL) {
        tap = alloc_template_arg(/*is_arg_type=*/TRUE);
        if (prev_tap == NULL) {
          /* First iteration -- the start the list. */
          *templ_arg_list = tap;
        } else {
          /* Add to the end of the list. */
          prev_tap->next = tap;
        }  /* if */
      }  /* if */
      /* Remember the current entry so that next time though (if there is a
         next time) we can find its successor or, if necessary, append a
         new entry to it. */
      prev_tap = tap;
    }  /* for */
    /* Now we have the nth template argument, which should correspond to
       the nth template parameter, whose type is templ_type. */
    if (tap->variant.type == NULL) {
      /* No type has been bound to this template argument yet, so just use
         "type".  This counts as a match. */
      tap->variant.type = type;
      match = TRUE;
    } else {
      /* A type was already bound to this template argument.  We have a match
         if and only if the new type is the same as the one already there. */
      if (identical_types(type, tap->variant.type)) {
        /* Okay. */
        match = TRUE;
      } else {
        /* Not a match.  Return FALSE. */
      }  /* if */
    }  /* if */
  } else {
    /* The type from the template is not a template parameter type.  Before
       checking further, remove typedefs -- but keep the type qualifiers
       in place. */
    type = skip_typedefs(type);
    templ_type = skip_typedefs(templ_type);
    if (templ_type->kind != type->kind) {
      /* No match. */
    } else {
      switch (type->kind) {
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Non-identical class types match if one represents a template
             and the other is an instantiation of that template.  For
             instance:
                template <class T> class A {  ...  };
                template <class TT> void f(A<TT>) {  ...  };
                f(A<int>);
             We reach this code when examining the argument in the call to f.
             "type" would refer to A<int> and "templ_type" would refer to
             A<TT>.  First we determine that A<int> and A<TT> refer to the same
             class template, and that the latter is a nonreal instantiation.
             Then we call matches_template_type on the template arg types. */
          templ_cssp = symbol_supplement_for_class(templ_type);
          if (templ_cssp->class_template != NULL &&
              symbol_supplement_for_class(type)->class_template ==
                                             templ_cssp->class_template &&
              templ_cssp->is_nonreal_class) {
            /* The two classes refer to the same template, but templ_type
               is a nonreal instantiation -- i.e., one based on template
               parameter types instead of real types. */
            a_template_arg_ptr  tap, templ_tap;
            tap = type->variant.class_struct_union.extra_info->
                                                           template_arg_list;
            templ_tap = templ_type->variant.class_struct_union.
                                               extra_info->template_arg_list;
            do {
              if (tap->is_type) {
                match = matches_template_type(tap->variant.type,
                                              templ_tap->variant.type,
                                              templ_arg_list);
              } else if (templ_tap->variant.constant->kind ==
#if 0
                                  (a_constant_repr_kind)ck_template_param
#else
                                  (a_constant_repr_kind)ck_error
#endif /* if 0 */
                                                                ) {
                match = TRUE;
              } else {
                match = eq_constants(tap->variant.constant,
                                     templ_tap->variant.constant);
              }  /* if */
              tap = tap->next;
              templ_tap = templ_tap->next;
            } while (match && tap != NULL);
          }  /* if */
          break;
        case tk_typeref:
          if (!type_qualifiers_match(type, templ_type)) {
            /* Not a match. */
          } else {
            /* Qualifiers match.  See if the underlying types do, too. */
            tp = type->variant.typeref.type;
            ttp = templ_type->variant.typeref.type;
            match = matches_template_type(tp, ttp, templ_arg_list);
          }  /* if */
          break;
        case tk_array:
          /* Array types match if their element types match and the number of
             elements is the same. */
          if (type->variant.array.number_of_elements !=
                        templ_type->variant.array.number_of_elements) {
            /* Not a match. */
          } else {
            tp = type->variant.array.element_type;
            ttp = templ_type->variant.array.element_type;
            match = matches_template_type(tp, ttp, templ_arg_list);
          }  /* if */
          break;
        case tk_pointer:
          /* Pointer matches pointer and reference matches reference, but they
             can't be mixed. */
          if (type->variant.pointer.is_reference !=
                         templ_type->variant.pointer.is_reference) {
            /* Not a match. */
          } else {
            tp = type->variant.pointer.type;
            ttp = templ_type->variant.pointer.type;
            match = matches_template_type(tp, ttp, templ_arg_list);
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* For ptr-to-member types, there needs to be a match on both the
             member types and the class-of-which-a-member. */
          tp = type->variant.ptr_to_member.type;
          ttp = templ_type->variant.ptr_to_member.type;
          if (matches_template_type(tp, ttp, templ_arg_list)) {
            tp = type->variant.ptr_to_member.class_of_which_a_member;
            ttp = templ_type->variant.ptr_to_member.class_of_which_a_member;
            match = (matches_template_type(tp, ttp, templ_arg_list));
          }  /* if */
          break;
        case tk_routine:
          /* For routine types there has to be a match both on the return types
             and on all the parameter types.  In addition, the has-ellipsis
             flags should be set the same. */
          tp = type->variant.routine.return_type;
          ttp = templ_type->variant.routine.return_type;
          if (matches_template_type(tp, ttp, templ_arg_list) &&
              (type->variant.routine.extra_info->has_ellipsis ==
                  templ_type->variant.routine.extra_info->has_ellipsis)) {
            /* Return type and ellipsis are okay.  Check the param types. */
            ptp = type->variant.routine.extra_info->param_type_list;
            tptp = templ_type->variant.routine.extra_info->param_type_list;
            for (;;) {
              tp = ptp->type;
              ttp = tptp->type;
              if (!matches_template_type(tp, ttp, templ_arg_list)) {
                /* The first param type for which there is a mismatch causes
                   a mismatch for the entire type.  No need to keep looping. */
                break;
              }  /* if */
              ptp = ptp->next;
              tptp = tptp->next;
              if (ptp == NULL || tptp == NULL) {
                /* One or both of the param type lists is exhausted.  It's a
                   match only if they're both done. */
                match = (ptp == tptp);
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          break;
        default:
          /* They are simple types -- these are leaf nodes in a type tree.
             Check for identity. */
          match = identical_types(templ_type, type);
      }  /* switch */
    }  /* if */
  }  /* if */
  db_exit();
  return match;
}  /* matches_template_type */


#if CHECKING
static void check_function_template_arg_list(
                                    a_template_arg_ptr  templ_arg_list,
                                    a_symbol_ptr        templ_sym)
/*
Do some simple consistency checking on a function template argument list.
*/
{
  a_template_param_ptr  tpp;
  a_template_arg_ptr    tap;

  tpp = templ_sym->variant.template.extra_info->parameters;
  for (tap = templ_arg_list; tap != NULL; tap = tap->next) {
    if (!tap->is_type) {
      internal_error("check_template_arg_list: not a type arg");
    } else if (tap->variant.type == NULL) {
      internal_error("check_template_arg_list: missing type ptr");
    }  /* if */
    if (tpp == NULL) {
      internal_error("check_template_arg_list: too many template args");
    }  /* if */
    tpp = tpp->next;
  }  /* for */
  if (tpp != NULL) {
    internal_error("check_template_arg_list: too few template args");
  }  /* if */
}  /* check_template_arg_list */
#endif /* CHECKING */


a_symbol_ptr make_template_function(a_symbol_ptr        templ_sym,
                                    a_type_ptr          rout_type,
                                    a_template_arg_ptr  templ_arg_list,
                                    a_source_position   *source_pos)
/*
Allocate the symbol and routine entry for a template function, based on
the function template (represented by templ_sym) and the function type
(rout_type), and allocate and enter the associated function instantiation
entry, where the template arg list for the instantiation (templ_arg_list)
is also recorded.  If rout_type is NULL, create a routine type based on
the template argument list and the template parameter list (reached through
templ_sym).
*/
{
  a_symbol_ptr                        sym;
  a_template_symbol_supplement_ptr    tssp;
  a_memory_region_number              region_to_switch_back_to;
  a_function_instantiation_entry_ptr  fiep;
  a_routine_ptr                       templ_rout, rp;

  db_enter(4, "make_template_function");
#if CHECKING
  check_function_template_arg_list(templ_arg_list, templ_sym);
#endif /* CHECKING */
  /* Allocate the template function symbol.  Note that it is not entered
     into the symbol table -- it will appear on a function instantiation
     list under the function template symbol and, optionally, in the overload
     list if it is also explicitly declared by the user. */
  sym = make_template_function_symbol(templ_sym, source_pos);
  tssp = templ_sym->variant.template.extra_info;
  templ_rout = tssp->variant.function.routine;
  /* All IL routines must be at the file scope level, so switch to that
     memory region if necessary to allocate the routine entry. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  sym->variant.routine.ptr = rp = alloc_routine();
  if (rout_type == NULL) {
    /* If the routine type does not already exist, create one, based on the
       function template's parameter list (the function parameters, that is,
       not the template parameters) along with the template argument list. */
    rout_type = copy_type_with_substitution(templ_rout->type, templ_arg_list,
                                            source_pos);
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  /* Give the routine entry the type passed in, and set other fields in
     accord with the settings in the template. */
  rp->type = rout_type;
  rp->storage_class = templ_rout->storage_class;
  rp->special_kind = templ_rout->special_kind;
  rp->opname_kind = templ_rout->opname_kind;
  rp->is_inline = templ_rout->is_inline;
  set_source_corresp(&rp->source_corresp, sym);
  rp->source_corresp.name_linkage = templ_rout->source_corresp.name_linkage;
  /* Add it to the file scope routines list. */
  add_to_routines_list(rp, /*at_file_scope=*/TRUE);
  /* Create the associated function instantiation entry and link it
     onto the front of the instantiation list for the template. */
  fiep = alloc_function_instantiation_entry();
  fiep->template_sym = templ_sym;
  fiep->arg_list = templ_arg_list;
  fiep->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = fiep;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  fiep->routine_sym = sym;
  sym->variant.routine.instance_ptr = fiep;

  db_exit();
  return sym;
}  /* make_template_function */


a_boolean is_match_for_function_template(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
                                         a_template_arg_ptr *templ_arg_list,
                                         a_symbol_ptr       *instance_sym)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If such a template
function exists, return its symbol.  Otherwise, try to generate a template
arg list to serve as the basis for creating one.  If either a symbol can
be found or a template arg list can be created, return TRUE; otherwise,
return FALSE.
*/
{
  a_boolean                           match = FALSE;
  a_symbol_ptr                        sym = NULL;
  a_type_ptr                          rout_type, templ_rout_type;
  a_template_symbol_supplement_ptr    tssp;
  a_function_instantiation_entry_ptr  fiep;
  a_param_type_ptr                    ptp, other_ptp;

  db_enter(3, "is_match_for_function_template");
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("is_match_for_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  *templ_arg_list = NULL;
  *instance_sym = NULL;
  /* sym is the symbol for a template function to be returned.  Returning NULL
     means no template function could be found or created. */
  tssp = templ_sym->variant.template.extra_info;
  templ_rout_type = tssp->variant.function.routine->type;
  /* First be sure the number of parameters in the template function is
     equal to the number in param_type_list. */
  ptp = curr_type->variant.routine.extra_info->param_type_list;
  other_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (other_ptp == NULL) {
      /* Too many params to match this template. */
      goto done;
    }  /* if */
    other_ptp = other_ptp->next;
  }  /* if */
  if (other_ptp != NULL) {
    /* Too many args in function template (and therefore in each of its
       instantiations) to justify looking any further. */
    goto done;
  }  /* if */
  /* Make a pass over the entries representing instantiations of the function
     template to see if any of them match the current type signature. */
  for (fiep = tssp->variant.function.instantiations;
       fiep != NULL;
       fiep = fiep->next) {
    if (fiep->specific_decl) {
      /* A function that matches this template but has a user declaration.
         If it is the function we seek it will already have been found
         directly -- ignore such cases in this search. */
      goto get_next_sym;
    }  /* if */
    sym = fiep->routine_sym;
    rout_type = sym->variant.routine.ptr->type;
    /* Return type must match exactly. */
    if (!identical_types(curr_type->variant.routine.return_type,
                         rout_type->variant.routine.return_type)) {
      /* No match.  Advance to the next template function symbol. */
      goto get_next_sym;
    }  /* if */
    /* Each parameter type must match exactly. */
    ptp = curr_type->variant.routine.extra_info->param_type_list;
    other_ptp = rout_type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      if (!identical_types(ptp->type, other_ptp->type)) {
        /* No match.  Advance to the next template function symbol. */
        goto get_next_sym;
      }  /* if */
      other_ptp = other_ptp->next;      
    }  /* for */
    /* Falling through to here means curr_type exactly matches the function
       type for sym.  Skip over the remaining processing and return sym to
       the caller. */
    match = TRUE;
    *instance_sym = sym;
    goto done;
get_next_sym:;
    /* No match so far.  Continue looping through the function instantiation
       entries. */
  }  /* for */
  /* Falling through to here means the type signature passed in does not
     match any existing template function based on the function template in
     question, but that it is not disqualified on other grounds.  Try to match
     the type signature to the template's type signature.  If successful, a
     template arg list is returned; otherwise, NULL is returned. */
  if (!matches_template_type(curr_type->variant.routine.return_type,
                             templ_rout_type->variant.routine.return_type,
                             templ_arg_list)) {
    goto done;
  } else {
    /* The routine type for curr_type can be accommodated to the template
       return type.  Now check each of the parameters. */
    ptp = curr_type->variant.routine.extra_info->param_type_list;
    other_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
    for (; other_ptp != NULL; other_ptp = other_ptp->next) {
      if (!matches_template_type(ptp->type, other_ptp->type,
                                 templ_arg_list)) {
        goto done;
      }  /* if */
      ptp = ptp->next;
    }  /* for */
    match = TRUE;
  }  /* if */
done:
  if (!match) {
    if (*templ_arg_list != NULL) {
      free_template_arg_list(*templ_arg_list);
      *templ_arg_list = NULL;
    }  /* if */
#if CHECKING
  } else if ((*instance_sym == NULL) == (*templ_arg_list == NULL)) {
    internal_error(
              "is_match_for_function_template: bad sym or templ arg list");
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return match;
}  /* is_match_for_function_template */


a_symbol_ptr matching_template_function(a_symbol_ptr        templ_sym,
                                        a_type_ptr          curr_type,
                                        a_source_position   *source_pos)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If no such template
function exists, try to create one.  If the search/creation is successful
return a pointer to the symbol; otherwise, return NULL.
*/
{
  a_symbol_ptr          sym;
  a_template_arg_ptr    templ_arg_list;

  db_enter(3, "matching_template_function");
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("matching_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  curr_type = skip_typerefs(curr_type);
  if (is_match_for_function_template(templ_sym, curr_type,
                                     &templ_arg_list, &sym)) {
    if (sym != NULL) {
      /* A match has been found -- just return a pointer to it. */
    } else {
      /* Use the template arg list to create a new symbol. */
      sym = make_template_function(templ_sym, curr_type, templ_arg_list,
                                   source_pos);
    }  /* if */
  }  /* if */
  db_exit();
  return sym;
}  /* matching_template_function */


void record_predeclared_template_function(a_symbol_ptr  templ_sym,
                                          a_symbol_ptr  rout_sym)
/*
*/
{
  a_symbol_ptr                       sym;
  a_template_symbol_supplement_ptr   tssp;
  a_function_instantiation_entry_ptr fiep;
  a_type_ptr                         tp;
  a_template_arg_ptr                 templ_arg_list;

  db_enter(3, "record_predeclared_template_function");
  if (rout_sym->variant.routine.instance_ptr != NULL) {
    /* Symbol is already marked as an instantiation. */
  } else {
    tp = skip_typerefs(rout_sym->variant.routine.ptr->type);
    if (is_match_for_function_template(templ_sym, tp, &templ_arg_list, &sym)) {
      /* A match has been found. */
#if CHECKING
#if 0
      /* This situation might come up in an error case.  We'll figure out what
         to do about it if it ever happens. */
#endif /* if 0 */
      if (sym != NULL) {
        internal_error("record_predeclared_template_function: sym found");
      }  /* if */
#endif /* CHECKING */
      /* Create the associated function instantiation entry and link it
         onto the front of the instantiation list for the template. */
      fiep = alloc_function_instantiation_entry();
      fiep->template_sym = templ_sym;
      fiep->arg_list = templ_arg_list;
      /* Mark this function as a "specialization". */
      fiep->specific_decl = TRUE;
      if (rout_sym->defined) {
        /* User-defined, so no instantiation is required. */
        fiep->specific_def = TRUE;
      }  /* if */
      tssp = templ_sym->variant.template.extra_info;
      fiep->next = tssp->variant.function.instantiations;
      tssp->variant.function.instantiations = fiep;
      /* Make the function instantiation entry and its associated symbol
         point at each other. */
      fiep->routine_sym = rout_sym;
      rout_sym->variant.routine.instance_ptr = fiep;
    }  /* if */
  }  /* if */
  db_exit();
}  /* record_predeclared_template_function */


void find_member_function_template(a_symbol_ptr  rout_sym,
                                   a_symbol_ptr  corresp_prototype_tag_sym)
/*
rout_sym is a member function of a template class.  corresp_prototype_tag_sym
is a symbol representing the corresponding prototype instantiation.  (For
instance, if rout_sym is a member of A<int>, the corresponding prototype
instantiation is A<T>; if rout_sym is a member of A<int>::B, the corresponding
prototype instantiation is A<T>::B.)  Find the function template symbol that
is a member of the corresponding prototype instantiation and that corresponds
to rout_sym (if the name is overloaded, use the source position to decide),
and create a function instantiation entry to bind the two symbols together.
*/
{
  a_symbol_ptr                       sym;
  a_template_symbol_supplement_ptr   tssp;
  a_function_instantiation_entry_ptr fiep;
  a_type_ptr                         tp;
  a_scope_number                     corresp_prototype_decl_scope;

  db_enter(3, "find_member_function_template");
  /* Find a function symbol on the inactive list that is in the scope of the
     prototype instantiation.  It should either be a function template or
     overloaded function symbol. */
  if (is_constructor_symbol(rout_sym)) {
    sym = corresp_prototype_tag_sym->
                         variant.class_struct_union.extra_info->constructor;
  } else {
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    tp = type_symbol_type(corresp_prototype_tag_sym);
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = rout_sym->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope &&
          (sym->kind == (a_symbol_kind)sk_function_template ||
           sym->kind == (a_symbol_kind)sk_overloaded_function)) {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (sym != NULL)
#endif /* CHECKING */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* An overloaded function was found.  Go through the symbols on its list
       and find the function template symbol that corresponds to rout_sym.
       The easiest way is just to compare source positions. */
    for (sym = sym->variant.overloaded_function.symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->kind == (a_symbol_kind)sk_function_template &&
          sym->decl_position.seq == rout_sym->decl_position.seq &&
          sym->decl_position.column == rout_sym->decl_position.column) {
        /* sym is the template function symbol for rout_sym. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_function_template ||
      sym->decl_position.seq != rout_sym->decl_position.seq ||
      sym->decl_position.column != rout_sym->decl_position.column) {
    internal_error("find_member_function_template: no corresponding template");
  }  /* if */
#endif /* CHECKING */
  /* sym is the template symbol for which member function rout_sym is an
     instantiation.  Create the function instantiation entry and set the
     pointers to bind them together. */
  fiep = alloc_function_instantiation_entry();
  fiep->template_sym = sym;
  /* Get the template arg list for the class and use it.  Note that if
     this is a nested class we have to climb the parent chain to find the
     template class in which the template arg list is recorded. */
  tp = rout_sym->class_of_which_a_member;
  while (tp->source_corresp.class_of_which_a_member != NULL) {
    tp = tp->source_corresp.class_of_which_a_member;
  }  /* if */
  fiep->arg_list =
             tp->variant.class_struct_union.extra_info->template_arg_list;
  tssp = sym->variant.template.extra_info;
  /* Link the new entry to the star of the instantiation list of the
     function template. */
  fiep->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = fiep;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  fiep->routine_sym = rout_sym;
  rout_sym->variant.routine.instance_ptr = fiep;
  db_exit();
}  /* find_memeber_function_template */


a_symbol_ptr find_template_function(a_symbol_ptr        templ_sym,
                                    a_template_arg_ptr  *new_list,
                                    a_source_position   *source_pos)
/*
templ_sym is a pointer to a symbol representing a function template and
*new_list is a pointer to a linked list of template arg entries.  If
*new_list is equivalent to the template arg list of a previous instantiation,
return the symbol representing the latter and put the entries on *new_list
back onto the available list.  Otherwise, create a new function instantiation
entry, symbol, routine entry, etc., and return the new symbol; in that
case *new_list is not disposed of but rather used in the resulting data
structure.
*/
{
  a_symbol_ptr                        sym;
  a_template_symbol_supplement_ptr    tssp;
  a_function_instantiation_entry_ptr  fiep, prev_fiep;

  db_enter(3, "find_template_function");
  /* Make a pass over the entries representing instantiations of the function
     template. */
  tssp = templ_sym->variant.template.extra_info ;
  fiep = tssp->variant.function.instantiations;
  prev_fiep = NULL;
  for (; fiep != NULL; fiep = fiep->next) {
    if (equiv_template_arg_lists(fiep->arg_list, *new_list,
                                 /*is_func_template=*/TRUE)) {
      /* We've found a match.  Remove the found function instantiation entry
         from its current position in the instantiation list and add it to
         the front. */
      if (prev_fiep != NULL) {
        prev_fiep->next = fiep->next;
        fiep->next = tssp->variant.function.instantiations;
        tssp->variant.function.instantiations = fiep;
      }
      sym = fiep->routine_sym;
#if DEBUG
      if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
    prev_fiep = fiep;
  }  /* for */
  if (fiep == NULL) {
    /* No match was found, so create a new template function.  That means
       create a symbol entry, a routine entry, a routine type entry, and a
       function instantiation entry, and linking all these appropriately.
       Note that the symbol will not be added to the symbol table, since it
       is accessed through the list of function instantiation entries. */
    sym = make_template_function(templ_sym, (a_type_ptr)NULL, *new_list,
                                 source_pos);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(templ_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* We are reusing a template function that already exists, so *new_list
       will not be used.  Return it to the available list for reuse. */
    free_template_arg_list(*new_list);
    *new_list = NULL;
  }  /* if */
  db_exit();
  return sym;
}  /* find_template_function */



static a_boolean class_template_declaration(a_template_param_ptr templ_params,
                                            a_symbol_ptr         *p_sym_ptr,
                                            a_boolean            *resolution,
                                            a_type_ptr           *new_type)
/*
If this turns out to be a class template declaration, scan it and return
TRUE, setting *p_sym_ptr to the class template symbol.  If it is not a class
declaration, return FALSE.  If a class template had been declared previously
but not defined, and this is a defining declaration, return *resolution
TRUE.  In addition, if this is a defining declaration, cache all the tokens
that make up the declaration and do a prototype instantiation.
*/
{
  a_boolean                         is_class_template_decl = FALSE;
  a_boolean                         suppress_redecl_error = FALSE;
  a_boolean                         is_definition, is_redecl;
  a_symbol_locator                  locator;
  a_symbol_ptr                      sym = NULL, prototype_sym, param_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     local_token_cache;
  a_type_kind                       type_kind;
  a_type_ptr                        prototype_type = NULL;
  a_template_arg_ptr                tap, *append_addr;
  a_template_param_ptr              tpp;
  a_boolean			    err;

  db_enter(3, "class_template_declaration");
  if (curr_token == tok_class || curr_token == tok_struct ||
      curr_token == tok_union) {
    switch (curr_token) {
      case tok_class:  type_kind = (a_type_kind)tk_class;  break;
      case tok_struct: type_kind = (a_type_kind)tk_struct; break;
      case tok_union:  type_kind = (a_type_kind)tk_union;  break;
    }  /* switch */
    /* This appears to be a class template declaration -- though it could
       be a function template declaration with a return type using one of
       these keywords.  We'll proceed on the assumption that it is indeed
       a class template until we see evidence to the contrary. */
    is_class_template_decl = TRUE;
    /* Bypass "class", "struct", or "union".  It has to be cached in case it
       has to be rescanned as part of a function template declaration. */
    clear_token_cache(&local_token_cache);
    cache_curr_token(&local_token_cache);
    (void)get_token();
    /* Next should be the class name. */
    if (!is_qualified_name_start()) {  /* Identifier or "::". */
      /* Not an identifier. */
      error(ec_exp_identifier);
      set_to_error_locator(locator);
    } else {
      /* Look up the identifier.  If it's a qualified name there will be an
         error down the line. */
      sym = coalesce_and_lookup_generalized_identifier
                               (GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
      /* Cache the identifier and advance past it so we can discriminate
         between a class template and a function template. */
      cache_curr_token(&local_token_cache);
      locator = locator_for_curr_id;
      (void)get_token();
      if (is_declarator_start()) {
        /* Since the current token appears to be the start of a declarator
           this looks like a function template declaration after all.  Return
           to the caller, but first rewind to the start of the return type
           declaration. */
        is_class_template_decl = FALSE;
        rescan_cached_tokens(&local_token_cache);
        sym = NULL;
        goto done;
      }  /* if */
      /* Now check for a qualified name.  If it is, set the locator to an
         error locator -- we don't have to worry about the locator that's
         already in the cache because this template will never be
         instantiated. */
      if (locator.is_qualified_name) {
        error(ec_qualified_name_not_allowed);
        set_to_error_locator(locator);
        sym = NULL;
      }  /* if */
    }  /* if */
    /* We needed local_token_cache only in case this was not a class
       template declaration.  But now we can assume it is. */
    discard_token_cache(&local_token_cache);
    is_definition = (curr_token == tok_colon || curr_token == tok_lbrace);
    /* If get_normal_id_or_qualified_name returned something, we may have a
       name conflict or a redefinition. */
    if (sym != NULL) {
      if (sym->kind == (a_symbol_kind)sk_class_template) {
        is_redecl = TRUE;
        tssp = sym->variant.template.extra_info;
        if (!sym->defined) {
          *resolution = TRUE;
        } else if (is_definition) {
          /* Attempting to redefine a class template. */
          pos_sy_error(ec_already_defined, &locator.source_position, sym);
          suppress_redecl_error = TRUE;
          sym = NULL;
        } else if ((type_kind == (a_type_kind)tk_union) !=
                   (tssp->variant.class.type_kind == (a_type_kind)tk_union)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator.source_position, sym);
          suppress_redecl_error = TRUE;
          sym = NULL;
        }  /* if */
      } else {
        /* Force the call to enter symbol, which will report the name clash. */
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* Enter the symbol at file scope. */
      sym = enter_symbol((a_symbol_kind)sk_class_template, &locator,
                         DEPTH_OF_FILE_SCOPE, suppress_redecl_error);
      tssp = sym->variant.template.extra_info;
      is_redecl = FALSE;
    }  /* if */
    if (is_definition || !is_redecl) {
      /* Either this is the first declaration of the template class or a
         defining redeclaration. */

      /* Save the type kind (corresponding to the class/struct/union token)
         in the class template symbol's supplement -- it will be needed when
         type entries for instantiations are created. */
      tssp->variant.class.type_kind = type_kind;

      tssp->parameters = templ_params;
      tssp->declaration_scope = scope_stack[decl_scope_level].number;
    }  /* if */
    if (is_definition) {
      sym->defined = TRUE;
      prototype_sym = make_template_class_symbol(sym, &sym->decl_position);
      /* Add the new symbol to the head of the instantiation list. */
      prototype_sym->next = tssp->variant.class.instantiations;
      tssp->variant.class.instantiations = prototype_sym;
      /* Now create a new type entry. */
      prototype_type = alloc_type(tssp->variant.class.type_kind);
      prototype_sym->variant.class_struct_union.type = prototype_type;
      set_source_corresp(&(prototype_type->source_corresp), prototype_sym);
      prototype_type->source_corresp.name_linkage =
                                           (a_name_linkage_kind)nlk_internal;
      append_addr = &prototype_type->
                     variant.class_struct_union.extra_info->template_arg_list;
      for (tpp = templ_params; tpp != NULL; tpp = tpp->next) {
        param_sym = tpp->param_symbol;
        if (param_sym->kind == (a_symbol_kind)sk_type) {
          tap = alloc_template_arg(/*is_arg_type=*/TRUE);
          tap->variant.type = param_sym->variant.type;
        } else {
          tap = alloc_template_arg(/*is_arg_type=*/FALSE);
          tap->variant.constant = param_sym->variant.constant;
        }  /* if */
        *append_addr = tap;
        append_addr = &tap->next;
      }  /* for */
      /* This is a class template definition, so scan all the tokens that
         comprise it and cache them away. */
      add_stop_token(tok_semicolon);
      if (curr_token == tok_colon) {
        /* Scan the tokens in the base class declarations, stopping when
           the "{" is reached. */
        add_stop_token(tok_lbrace);
        cache_token_stream(&tssp->body_token_cache);
        remove_stop_token(tok_lbrace);
      }  /* if */
      remove_stop_token(tok_semicolon);
      /* Scan the class body.  If the body is missing the error will be
         found during prototype instantiation. */
      if (curr_token == tok_lbrace) {
        /* Swallow the "{" and then cache everything through to the "}". */
        cache_curr_token(&tssp->body_token_cache);
        (void)get_token();
        add_stop_token(tok_rbrace);
        cache_token_stream(&tssp->body_token_cache);
        remove_stop_token(tok_rbrace);
        /* Now cache the "}" (unless we didn't find one). */
        if (curr_token == tok_rbrace) {
          cache_curr_token(&tssp->body_token_cache);
          (void)get_token();
        }  /* if */
      }  /* if */
      /* Add an end-of-source token to the end of the token cache to assure
         that we don't scan past the end of the cache in the actual scan. */
      terminate_token_cache(&tssp->body_token_cache);
    } else {
      /* This is not a class template definition, so we have no need to
         cache the tokens. */
    }  /* if */
    /* Note that the semicolon is not cached. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
done:;
  db_exit();
  *p_sym_ptr = sym;
  *new_type = prototype_type;
  return is_class_template_decl;
}  /* class_template_declaration */


static a_boolean function_template_declaration(a_symbol_ptr  *sym)
/*
*/
{
  a_storage_class                   storage_class;
  a_type_ptr                        type;
  a_symbol_locator                  locator;
  a_decl_flag_set                   do_flags, dso_flags;
  a_func_info_block                 func_info;
  a_type_ptr                        bottom_derived_type = NULL;
  an_expr_node_ptr                  dim_expr_ptr;
  a_token_cache                     local_token_cache, *p_token_cache;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         err = FALSE;
  a_source_position                 decl_start_pos;

  db_enter(3, "function_template_declaration");

  decl_start_pos = pos_curr_token;
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_colon);
  clear_token_cache(&local_token_cache);
  cache_token_stream(&local_token_cache);
  /* Add an end-of-source token to the end of the token cache to assure that
     we don't scan past the end of the cache in the actual scan. */
  terminate_token_cache(&local_token_cache);
  rescan_reusable_cache(&local_token_cache);
  (void)decl_specifiers((DSI_IS_TEMPLATE_DECLARATION |
                         DSI_INLINE_ALLOWED |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                         DSI_STORAGE_CLASS_SPECIFIER_ALLOWED),
                         &dso_flags, &storage_class, &type);
  declarator(DI_REAL_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED,
             &do_flags, type, (a_type_ptr)NULL, &locator, &type,
             &bottom_derived_type, &func_info, &dim_expr_ptr);
  if (is_error_locator(locator) || !is_function_type(type)) {
    if (!is_error_locator(locator)) {
      pos_error(ec_bad_template_declaration, &decl_start_pos);
    }  /* if */
    err = TRUE;
    discard_token_cache(&local_token_cache);
    clear_token_cache(&local_token_cache);
    p_token_cache = &local_token_cache;
  } else {
    decl_function_template(&locator, type, sym, storage_class,
                           (dso_flags & DSO_INLINE) != 0);
    if (is_error_locator(locator)) err = TRUE;
    tssp = (*sym)->variant.template.extra_info;
    tssp->variant.function.decl_token_cache = local_token_cache;
    tssp->variant.function.func_info = func_info;
    p_token_cache = &tssp->body_token_cache;
  }  /* if */
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  remove_stop_token(tok_colon);
  if (curr_token == tok_end_of_source) {
    /* Advance past the end-of-source token. */
    (void)get_token();
    if (!err && curr_token == tok_colon && is_constructor_symbol(*sym)) {
      add_stop_token(tok_lbrace);
      add_stop_token(tok_semicolon);
      cache_token_stream(p_token_cache);
      remove_stop_token(tok_lbrace);
      remove_stop_token(tok_semicolon);
    }  /* if */      
    if (curr_token == tok_lbrace) {
      if (!err) (*sym)->defined = TRUE;
      /* Cache the "{" and advance past it. */
      cache_curr_token(p_token_cache);
      (void)get_token();
      /* Cache all tokens up to the "}" (or end-of-source). */
      add_stop_token(tok_rbrace);
      cache_token_stream(p_token_cache);
      remove_stop_token(tok_rbrace);
      /* Cache the "}" and append an end-of-source token. */
      if (curr_token == tok_rbrace) {
        cache_curr_token(p_token_cache);
        /* Advance to the next token. */
        (void)get_token();
      }  /* if */
      if (!err) {
        /* Add an end-of-source token to the end of the token cache to assure
           that we don't scan past the end of the cache in the actual scan. */
        terminate_token_cache(p_token_cache);
      } else {
        discard_token_cache(p_token_cache);
      }  /* if */
    } else {
      /* No body to cache.  Check for final semicolon. */
      (void)required_token(tok_semicolon, ec_exp_semicolon);
    }  /* if */
  } else {
    (void)required_token(tok_semicolon, ec_exp_semicolon);
    while (curr_token != tok_end_of_source) (void)get_token();
    /* Advance past the end-of-source token. */
    (void)get_token();
  }  /* if */
  db_exit();
  return !err;
}  /* function_template_declaration */


static a_template_param_ptr scan_template_param_list(void)
/*
Scan a comma-separated list of template parameters.  The opening "<" will
already have been scanned, and an empty list will have already been
checked for.  The current token, consequently, is the first token of the
first parameter.  Return a pointer to the linked list that is created
to represent the template parameters.
*/
{
  a_symbol_ptr         sym;
  a_source_position    param_pos;
  a_template_param_ptr template_param;
  a_template_param_ptr template_param_list = NULL;
  a_template_param_ptr end_of_template_param_list = NULL;
  a_type_ptr           template_param_type;
  int                  template_param_list_pos = 0;

  db_enter(3, "scan_template_param_list");
  /* Loop through the comma-separated list of template parameter
     declarations. */
  do {
    add_stop_token(tok_comma);
    copy_source_position(pos_curr_token, param_pos);
    ++template_param_list_pos;
    /* Determine whether this is a "type-argument" (a parameter that
       represents a type) or a "arg-declaration" (a parameter that represents
       a constant). */
    if (curr_token == tok_class && next_token() == tok_identifier) {
      /* A type-argument. Note that there is a possible ambiguity here:
         template <class T> vs. template <class T X>, where in the second
         case T is already declared.  One could argue that the second is an
         "arg-declaration" rather than a "type-argument", but the working
         paper (14.1 para 2) appears to resolve the ambiguity in favor of
         always interpreting <class T ... as a type-argument.  Moreover, a
         class object cannot be a constant. */
      /* Bypass "class". */
      (void)get_token();
      /* Enter a type symbol in the symbol table.  It is made (for now) to
         point to an error type, to make everything work smoothly during
         preliminary scanning of the body of the class. */
      sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      /* Allocate a template-param type.  This type is for front-end use
         only and will not appear in the IL passed on to the back end.  It
         is therefore not added to any scope types list. */
      template_param_type = alloc_type((a_type_kind)tk_template_param);
      template_param_type->variant.list_position = template_param_list_pos;
      set_type_size(template_param_type);
      set_source_corresp(&template_param_type->source_corresp, sym);
      /* The type symbol for the template parameter points for now to the
         template-param type -- "for now", since it will be replaced with
         an actual type during instantiation of the class or function. */
      sym->variant.type = template_param_type;
      /* Bypass the identifier. */
      (void)get_token();
    } else {
      /* Not a type-argument, so treat it as an arg-declaration.  If this
         template declaration happens to be of a function rather than a class,
         arg-declarations are not allowed.  That will be detected later. */
      a_decl_flag_set      do_flags, dso_flags;
      a_type_ptr           param_type_ptr;
      a_storage_class      param_storage_class;
      a_symbol_locator     param_locator;
      a_type_ptr           bottom_derived_type;
      an_expr_node_ptr     dim_expr_ptr;

      /* Scan the declaration specifiers. */
      (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                             DSI_IS_TEMPLATE_PARAMETER),
                             &dso_flags, &param_storage_class,
                             &param_type_ptr);
      if (dso_flags & DSO_DEFINES_SOMETHING) {
        pos_error(ec_type_definition_not_allowed, &param_pos);
        param_type_ptr = error_type();
      }  /* if */
      if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
        /* Missing type specifier. */
        warning(ec_missing_type_specifier);
      }  /* if */
      /* Scan the declarator. */
      declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags,
                 param_type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
                 &param_locator, &param_type_ptr, &bottom_derived_type,
                 (a_func_info_block_ptr)NULL, &dim_expr_ptr);
#if 0
/* Is a call to adjust_parameter_type required??? */
      /* Adjust the type if necessary (for example, "array of x"
         becomes "pointer to x"). */
      adjust_parameter_type(&param_type_ptr);
#endif /* if 0 */
#if 0
      /* Check here for types for which constants cannot be created?  E.g.,
         the program would not be able to declare a constant class object or
         a constant array.  Likewise, should reference types be permitted?
         Should a constant with an error type be created for such cases? */
#endif /* if 0 */
      /* Enter a symbol and bind an error constant to it temporarily.  At the
         point of instantiation an actual constant will be substituted. */
      sym = enter_symbol((a_symbol_kind)sk_constant, &param_locator,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.constant = fs_constant((a_constant_repr_kind)ck_error);
      sym->variant.constant->type = param_type_ptr;
      set_source_corresp(&sym->variant.constant->source_corresp, sym);
      template_param_type = NULL;
    }  /* if */
    /* Allocate a template parameter and add it to the end of the list. */
    template_param = alloc_template_param();
    template_param->param_symbol = sym;
    /* The param_type field will be NULL for constant parameters and point to
       a tk_template_param type for type parameters. */
    template_param->param_type = template_param_type;
    /* Add the template param to the end of the list. */
    if (template_param_list == NULL) {
      template_param_list = template_param;
    } else {
      end_of_template_param_list->next = template_param;
    }  /* if */
    end_of_template_param_list = template_param;
    remove_stop_token(tok_comma);
    /* Keep looping on a comma. */
  } while (loop_token(tok_comma));
  db_exit();
  return template_param_list;
}  /* scan_template_param_list */


/* Forward declaration because of recursive invocation. */
static a_boolean template_param_appears_in_param_list(a_type_ptr  tparam_type,
                                                      a_type_ptr  rout_type);

static a_boolean template_param_appears_in_type_tree(a_type_ptr  tparam_type,
                                                     a_type_ptr  tp)
/*
tparam_type is a tk_template_parameter type entry used in a template
declaration.  Search the type tree represented by tp and return TRUE if
tparam_type appears in the tree.  This routine traverses the type tree
by means of recursive calls.
*/
{
  a_boolean           found;
  a_template_arg_ptr  tap;

  tp = skip_typerefs(tp);
  if (identical_types(tp, tparam_type)) {
    found = TRUE;
  } else {
    switch (tp->kind) {
      case tk_pointer:
        /* Check the type pointed to. */
        found = template_param_appears_in_type_tree(tparam_type,
                                                    type_pointed_to(tp));
        break;
      case tk_array:
        /* Check the type of an element of the array. */
        found = template_param_appears_in_type_tree(
                              tparam_type, underlying_array_element_type(tp));
        break;
      case tk_routine:
        /* Check both the return type and all the parameter types.  Note that
           cfront 3.0 does not consider a template parameter that appears in
           return type of a pointer-to-function as a "use" in forming the
           signature of the function template; we believe this is a cfront
           bug. */
        found = (template_param_appears_in_type_tree(
                              tparam_type, tp->variant.routine.return_type) ||
                 template_param_appears_in_param_list(tparam_type, tp));
        break;
      case tk_ptr_to_member:
        /* Check both the member type and the class type. */
        found = (template_param_appears_in_type_tree(tparam_type,
                                                     pm_member_type(tp)) ||
                 template_param_appears_in_type_tree(tparam_type,
                                                     pm_class_type(tp)));
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* If the class is an instantiation of a template, check the types
           on which the instantiation is based. */
        tap = tp->variant.class_struct_union.extra_info->template_arg_list;
        found = FALSE;
        for (; tap != NULL; tap = tap->next) {
          if (tap->is_type) {
            if (template_param_appears_in_type_tree(tparam_type,
                                                    tap->variant.type)) {
              found = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
        break;
      case tk_error:
        /* We assume, with no justification other than to avoid apparently
           spurious diagnostics, that the error (of which the error type is
           a representation) involved the very type we are looking at. */
        found = TRUE;
        break;
      default:
        /* We have reached a leaf in the type tree without finding the
           template parameter. */
        found = FALSE;
    }  /* switch */
  }  /* if */
  return found;
}  /* template_param_appears_in_type_tree */


static a_boolean template_param_appears_in_param_list(a_type_ptr  tparam_type,
                                                      a_type_ptr  rout_type)
/*
tparam_type is a tk_template_parameter type entry used in a template
declaration, and rout_type is a routine type.  Search each of the routine's
parameter types to see if tparam_type appears in it.
*/
{
  a_boolean         found = FALSE;
  a_param_type_ptr  ptp;

  ptp = rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (template_param_appears_in_type_tree(tparam_type, ptp->type)) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* template_param_appears_in_param_list */


void template_declaration(void)
/*
Scan a C++ template declaration.  Syntax:

  template-declaration:

    template < template-argument-list > declaration

  template-argument:

    type-argument
    argument-declaration

  type-argument:

    class identifier

Template declarations will declare either a class template or a function
template; in the latter case the template argument list may include only
type-arguments. During the scan of the template declaration a special scope
entry is pushed on the scope stack.
*/
{
  a_template_param_ptr              tpp, template_param_list = NULL;
  a_symbol_ptr                      sym, param_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         tag_resolution = FALSE;
  a_type_ptr                        rout_type, prototype_type = NULL;

  db_enter(3, "template_declaration");
#if CHECKING
  if (curr_token != tok_template) {
    internal_error("template_declaration: expected tok_template");
  }  /* if */
#endif /* CHECKING */
  if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
    /* template declarations may appear at file scope only (ARM 14.1). */
    error(ec_nonglobal_template_declaration);
  }  /* if */
  (void)push_scope((a_scope_kind)sck_template_declaration, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL,
                   (a_function_instantiation_entry_ptr)NULL);
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  /* Bypass "template".  The next token should be "<". */
  (void)get_token();
  if (required_token(tok_lt, ec_exp_lt)) {
    add_stop_token(tok_gt);
    template_param_list = scan_template_param_list();
    if (template_param_list == NULL) {
      error(ec_missing_template_param);
    }  /* if */
    remove_stop_token(tok_gt);
  }  /* if */
  /* Check for an bypass the ">". */
  (void)required_token(tok_gt, ec_exp_gt);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  if (class_template_declaration(template_param_list, &sym, &tag_resolution,
                                 &prototype_type)) {
    /* The declaration was successfully scanned as a class template
       declaration. */
  } else if (function_template_declaration(&sym)) {
    if (sym->class_of_which_a_member != NULL) {
      /* Out-of-line definition of a member function of a class template.
         Don't impose requirements on the use of template parameters in the
         parameters. */
    } else {
      /* Go back through the template params and be sure there are only type
         args.  The other kind is allowed only for class templates. */
      tssp = sym->variant.template.extra_info;
      tssp->parameters = template_param_list;
      tssp->declaration_scope = scope_stack[decl_scope_level].number;
      rout_type = tssp->variant.function.routine->type;
      for (tpp = template_param_list; tpp != NULL; tpp = tpp->next) {
        param_sym = tpp->param_symbol;
        if (param_sym->kind != (a_symbol_kind)sk_type) {
          pos_error(ec_not_a_type_arg, &param_sym->decl_position);
        } else if (!param_sym->referenced ||
                   (template_param_appears_in_type_tree(
                                    param_sym->variant.type,
                                    rout_type->variant.routine.return_type) &&
                    !template_param_appears_in_param_list(
                                    param_sym->variant.type, rout_type))) {
          pos_sy2_error(ec_not_used_in_template_function_params,
                       &param_sym->decl_position, param_sym, sym);
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Error. */
  }  /* if */
  /* Note that the template declaration scope must be popped before doing the
     prototype instantiation. */
  pop_scope();
  if (prototype_type != NULL) {
#if CHECKING
    if (sym == NULL || sym->kind != (a_symbol_kind)sk_class_template ||
        !sym->defined || (tssp = sym->variant.template.extra_info) == NULL ||
        tssp->variant.class.instantiations == NULL ||
        tssp->variant.class.instantiations->
                         variant.class_struct_union.type != prototype_type) {
      internal_error("template_declaration: sym & prototype_type out of sync");
    }  /* if */
#endif /* CHECKING */
    /* Do a "prototype instantiation" of the class template -- i.e., parse
       the declarative information looking for gross syntax errors. */
    instantiate_class_template(sym, prototype_type);
  }  /* if */
  if (tag_resolution) {
    /* This is the resolution of a previously incomplete template declaration;
       if there are array types to be resolved, look to see if any of them are
       arrays whose element type is an instantiation of this template.  (This
       is by analogy with normal classes, for which an array of incomplete
       class objects is allowed, pending completion.) */
    check_fixup_list_for_array_types();
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (sym != NULL) db_symbol(sym, "template symbol: ", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* template_declaration */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
