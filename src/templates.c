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
#include "decls.h"
#include "error.h"
#include "il.h"
#include "lexical.h"
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

  db_enter(3, "instantiate_template_class");
  if (is_array_type(tp)) tp = underlying_array_element_type(tp);
  if (tp != NULL && is_class_struct_union_type(tp)) {
    tp = skip_typerefs(tp);
    template_sym = (symbol_supplement_for_class(tp))->class_template;
    if (template_sym == NULL) {
      /* Not a class based on a class template. */
    } else {
      /* There is a class template from which to generate this class. */
      tssp = template_sym->variant.template.extra_info;
      p_token_cache = &tssp->body_token_cache;
      if (p_token_cache->first_token == NULL) {
        /* The template itself has not yet been defined.  The caller will
           issue an incomplete-type error. */
      } else if (instantiation_in_progress(tp)) {
        /* The template is currently being instantiated. */
      } else {
        /* We proceed with the instantiation. */
#if DEBUG
        if (debug_level >= 3) {
          fprintf(f_debug, "instantiating: ");
          db_type(tp);
          db_symbol(template_sym, "\nbased on: ", 2);
        }  /* if */
#endif /* DEBUG */
        rescan_reusable_cache(p_token_cache);
#if CHECKING
        if (curr_token != tok_lbrace && curr_token != tok_colon) {
          internal_error("instantiate_template_class: bad 1st token in cache");
        }  /* if */
#endif /* CHECKING */
        (void)push_scope(sck_template_instantiation, tssp->declaration_scope,
                         tp, (a_routine_ptr)NULL,
                         (a_function_instantiation_entry_ptr)NULL);
        /* Scan the base specifiers list, if any, and the body of the class. */
        (void)scan_class_definition(tp, DEPTH_OF_FILE_SCOPE,
                                    /*is_local_class=*/FALSE);
        pop_scope();
        /* In the normal case the current token should be end_of_source,
           which was inserted to mark the end of the cached token stream.
           If necessary, keep flushing until end-of-source is found. */
        while (curr_token != tok_end_of_source) (void)get_token();
        /* Advance past the end-of-source token. */
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* instantiate_template_class */


void instantiate_template_function(a_routine_ptr   rout,
                                   a_token_cache   *p_token_cache,
                                   a_scope_number  scope_number)
/*
*/
{
  db_enter(3, "instantiate_template_function");
#if 0
  rescan_reusable_cache(p_token_cache);
  void(push_scope(sck_template_instantiation, scope_number,
                  (a_type_ptr)NULL, rout,
                  (a_function_instantiation_entry_ptr)NULL);
  template_function_definition(...);
  pop_scope();
  /* In the normal case the current token should be end_of_source, which was
     inserted to mark the end of the cached token stream. If necessary, keep
     flushing until end-of-source is found. */
  while (curr_token != tok_end_of_source) (void)get_token();
  /* Advance past the end-of-source token. */
  (void)get_token();
#endif /* if 0 */
  db_exit();
}  /* instantiate_template_function */


static a_boolean equiv_template_arg_lists(a_template_arg_ptr  list1,
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
                                 a_template_arg_ptr  new_list,
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
    if (equiv_template_arg_lists(old_list, new_list,
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
    /* Record the argument list in the type.  It should be available in the
       IL at least for name generation and possibly for debuggers, too.  Note,
       however, that the type itself is not added to the scope types list
       until a full instantiation takes place -- or, if there is none, in
       pop_scope, as with ordinary classes. */
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    /* All template instantiations have C++ external linkage, but mark it as
       internally linked for now.  The name linkage will be fixed up later,
       along with nontemplate classes.  This assures uniform processing of
       members. */
    class_type->source_corresp.name_linkage =
                                        (a_name_linkage_kind)nlk_internal;
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(class_template_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  db_exit();
  return sym;
}  /* find_template_class */


a_symbol_ptr find_template_function(a_symbol_ptr        function_template_sym,
                                    a_template_arg_ptr  new_list,
                                    a_source_position   *source_pos)
/*
*/
{
  a_template_symbol_supplement_ptr    tssp;
  a_function_instantiation_entry_ptr  fiep, prev_fiep;
  a_template_arg_ptr                  old_list;

  db_enter(3, "find_template_function");
  /* Make a pass over the entries representing instantiations of the function
     template. */
  tssp = function_template_sym->variant.template.extra_info;
  fiep = tssp->variant.function.instantiations;
  prev_fiep = NULL;
  for (; fiep != NULL; fiep = fiep->next) {
    /* Old list is the template argument list from a template function that
       has already been created.  See if the list passed in matches it. */
    old_list = fiep->arg_list;
    if (equiv_template_arg_lists(old_list, new_list,
                                 /*is_func_template=*/TRUE)) {
      if (prev_fiep != NULL) {
        prev_fiep->next = fiep->next;
        fiep->next = tssp->variant.function.instantiations;
        tssp->variant.function.instantiations = fiep;
      }  /* if */
#if DEBUG
      if (debug_level >= 3) db_symbol(fiep->routine_sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
    prev_fiep = fiep;
  }  /* for */
  if (fiep == NULL) {
#if 0
    /* NYI */
#endif /* if 0 */
  }  /* if */
  db_exit();
  return fiep->routine_sym;
}  /* find_template_function */


static a_boolean class_template_declaration(a_symbol_ptr  *p_sym_ptr,
                                            a_boolean     *tag_resolution)
/*
If this turns out to be a class template declaration, scan it and return
TRUE, setting *p_sym_ptr to the class template symbol and, if this is a
defining declaration, setting *p_token_cache to the token cache for the
entire declaration (from "class" through the closing right brace).  If it
is not a class declaration, return FALSE.  If a class template had been
declared previously but not defined, and this is a defining declaration,
return *tag_resolution TRUE.
*/
{
  a_boolean                         is_class_template_decl = FALSE;
  a_symbol_locator                  locator;
  a_symbol_ptr                      sym = NULL;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     local_token_cache;
  a_type_kind                       type_kind;

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
      sym = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
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
    /* If get_normal_id_or_qualified_name returned something, we may have a
       name conflict or a redefinition. */
    if (sym != NULL) {
      if (sym->kind == (a_symbol_kind)sk_class_template) {
        if (!sym->defined) {
          *tag_resolution = TRUE;
        } else if (curr_token == tok_colon || curr_token == tok_lbrace) {
          /* Attempting to redefine a class template. */
          pos_sy_error(ec_already_defined, &locator.source_position, sym);
        }  /* if */
      } else {
        /* Force the call to enter symbol, which will report the name clash. */
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      /* Enter the symbol at file scope. */
      sym = enter_symbol((a_symbol_kind)sk_class_template, &locator,
                         DEPTH_OF_FILE_SCOPE, /*suppress_redecl_error=*/FALSE);
    }  /* if */
    /* Save the type kind (corresponding to the class/struct/union token)
       in the class template symbol's supplement -- it will be needed when
       type entries for instantiations are created. */
    sym->variant.template.extra_info->variant.class.type_kind = type_kind;
    /* If this is a class template definition, continue caching all the tokens
       that comprise it. */
    if (curr_token == tok_colon || curr_token == tok_lbrace) {
      sym->defined = TRUE;
      tssp = sym->variant.template.extra_info;
      /* Now scan the remaining tokens. */
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
      /* This is not a class template declaration, so we have no need to
         cache the tokens. */
    }  /* if */
    /* Note that the semicolon is not cached. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
done:;
  db_exit();
  *p_sym_ptr = sym;
  return is_class_template_decl;
}  /* class_template_declaration */


static void function_template_declaration(a_symbol_ptr  *sym,
                                          a_type_ptr    *p_rout_type)
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
  a_token_cache                     decl_token_cache;
  a_template_symbol_supplement_ptr  tssp;

  db_enter(3, "function_template_declaration");

  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  clear_token_cache(&decl_token_cache);
  cache_token_stream(&decl_token_cache);
  /* Add an end-of-source token to the end of the token cache to assure that
     we don't scan past the end of the cache in the actual scan. */
  terminate_token_cache(&decl_token_cache);
  rescan_reusable_cache(&decl_token_cache);
  (void)decl_specifiers((DSI_IS_TEMPLATE_DECLARATION |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_STORAGE_CLASS_SPECIFIER_ALLOWED),
                         &dso_flags, &storage_class, &type);
  declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags, type, (a_type_ptr)NULL,
             &locator, &type, &bottom_derived_type, &func_info,
             &dim_expr_ptr);
  *p_rout_type = type;
  *sym = enter_symbol((a_symbol_kind)sk_function_template, &locator,
                     DEPTH_OF_FILE_SCOPE, /*suppress_redecl_error=*/FALSE);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  tssp = (*sym)->variant.template.extra_info;
  tssp->variant.function.decl_token_cache = decl_token_cache;
  if (curr_token == tok_end_of_source) {
    /* Advance past the end-of-source token. */
    (void)get_token();
    if (curr_token == tok_lbrace) {
      (*sym)->defined = TRUE;
      /* Cache the "{" and advance past it. */
      cache_curr_token(&tssp->body_token_cache);
      (void)get_token();
      /* Cache all tokens up to the "}" (or end-of-source). */
      add_stop_token(tok_rbrace);
      cache_token_stream(&tssp->body_token_cache);
      remove_stop_token(tok_rbrace);
      /* Cache the "}" and append an end-of-source token. */
      if (curr_token == tok_rbrace) {
        cache_curr_token(&tssp->body_token_cache);
        /* Advance to the next token. */
        (void)get_token();
      }  /* if */
      /* Add an end-of-source token to the end of the token cache to assure
         that we don't scan past the end of the cache in the actual scan. */
      terminate_token_cache(&tssp->body_token_cache);
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

  db_enter(3, "scan_template_param_list");
  /* Loop through the comma-separated list of template parameter
     declarations. */
  do {
    add_stop_token(tok_comma);
    copy_source_position(pos_curr_token, param_pos);
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
  if (tparam_type == tp) {
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
        /* Check both the return type and all the parameter types. */
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
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         tag_resolution = FALSE;
  a_type_ptr                        rout_type;

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
  if (class_template_declaration(&sym, &tag_resolution)) {
    /* The declaration was successfully scanned as a class template
       declaration. */
  } else {
    /* It must be a function template declaration. */
    function_template_declaration(&sym, &rout_type);
    /* Go back through the template params and be sure there are only type
       args.  The other kind is allowed only for class templates. */
    for (tpp = template_param_list; tpp != NULL; tpp = tpp->next) {
      a_symbol_ptr  param_sym = tpp->param_symbol;
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
  tssp = sym->variant.template.extra_info;
  tssp->parameters = template_param_list;
  tssp->declaration_scope = scope_stack[decl_scope_level].number;
  pop_scope();
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
    db_symbol(sym, "template symbol: ", 2);
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
