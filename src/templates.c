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


static a_boolean instantiate_template_class(a_type_ptr  type)
/*
*/
{
  a_symbol_ptr                      template_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         success = FALSE;
  a_token_cache                     *p_token_cache;

  db_enter(3, "instantiate_template_class");
  type = skip_typerefs(type);
  template_sym = (symbol_supplement_for_class(type))->class_template;
  if (template_sym == NULL) {
    /* Not a class based on a class template.  Return FALSE. */
  } else {
    /* There is a class template from which to generate this class. */
    tssp = template_sym->variant.template.extra_info;
    p_token_cache = &tssp->template_body;
    if (p_token_cache->first_token == NULL) {
      /* The template itself has not yet been instantiated.  The caller will
         issue an incomplete-type error.  Return FALSE. */
    } else {
      /* We proceed with the instantiation. */
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "instantiating: ");
        db_type(skip_typerefs(type));
        db_symbol(template_sym, "\nbased on: ", 2);
      }  /* if */
#endif /* DEBUG */
      rescan_cached_tokens(p_token_cache);
      (void)push_scope(sck_template_instantiation, tssp->declaration_scope,
                       type, (a_routine_ptr)NULL);
      if (curr_token == tok_struct) {
        type->kind = (a_type_kind)tk_struct;
      }  /* if */
      /* Bypass "class", "struct" or "union". */
      (void)get_token();
      /* Bypass the identifier that follows it. */
#if CHECKING
      if (curr_token != tok_identifier) {
        internal_error("instantiate_template_class: missing identifier");
      }  /* if */
#endif /* if CHECKING */
      (void)get_token();
      /* Scan the base specifiers list, if any, and the body of the the
         class. */
      (void)scan_class_definition(type, DEPTH_OF_FILE_SCOPE,
                                  /*is_local_class=*/FALSE);
      pop_scope();
      /* In the normal case the current token should be end_of_source, which
         was inserted to mark the end of the cached token stream. If necessary,
         keep flushing until end-of-source is found. */
      while (curr_token != tok_end_of_source) (void)get_token();
      /* Advance past the end-of-source token. */
      (void)get_token();
      /* Return TRUE. */
      success = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
  return success;
}  /* instantiate_template_class */


a_boolean try_template_class_instantiation(a_type_ptr  type)
/*
*/
{
  a_boolean  success = FALSE;

  if (C_dialect == C_dialect_cplusplus) {
    if (is_class_struct_union_type(type)) {
#if CHECKING
      if (skip_typerefs(type)->size != 0) {
        /* We assume the check for size (is_incomplete_type or is_object_type)
           has already been done. */
        internal_error("try_template_class_instantiation: nonzero size");
      }  /* if */
#endif /* CHECKING */
      success = instantiate_template_class(type);
    }  /* if */
  }  /* if */
  return success;
}  /* try_template_class_instantiation */


static a_boolean equiv_class_template_arg_lists(a_template_arg_ptr  list1,
                                                a_template_arg_ptr  list2)
/*
Return TRUE if the two linked lists of template arguments for a given
template class are equivalent -- that is, if corresponding type arguments
refer to the same type and corresponding constant arguments refer to the
same constant.  This routine should not be call for function template
argument lists.
*/
{
  a_boolean           equiv;
  a_template_arg_ptr  arg1 = list1, arg2 = list2;

  db_enter(4, "equiv_class_template_arg_lists");
  /* Assume they are equivalent, until we find evidence to the contrary. */
  equiv = TRUE;
  /* Loop through both lists in step, comparing arguments. */
  do {
#if CHECKING
    /* There is no way to produce a NULL template argument list, so the real
       code doesn't need to check for that.  Moreover, for a given class,
       argument lists should always be exactly the same length and have
       the same sequence of type and constant arguments. */
    if (arg1 == NULL || arg2 == NULL || arg1->is_type != arg2->is_type) {
      internal_error("equiv_template_arg_lists: arg inconsistency");
    }  /* if */
#endif /* CHECKING */
    if (arg1->is_type) {
      /* Both are type arguments.  If they are not identical, this is a
         mismatch. */
      if (!identical_types(arg1->variant.type, arg2->variant.type)) {
        equiv = FALSE;
        break;
      }  /* if */
    } else {
      /* Both are constant arguments.  If they are not identical, this is a
         mismatch. */
      if (!eq_constants(arg1->variant.constant, arg2->variant.constant)) {
        equiv = FALSE;
        break;
      }  /* if */
    }  /* if */
    /* Advance to the next arguments in step. */
    arg1 = arg1->next;
    arg2 = arg2->next;
  } while (arg1 != NULL);

  db_exit();
  return equiv;
}  /* equiv_class_template_arg_lists */


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
  a_type_kind                       type_kind;
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
    if (equiv_class_template_arg_lists(old_list, new_list)) {
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
    if (sym->kind == (a_symbol_kind)sk_union_tag) {
      type_kind = (a_type_kind)tk_union;
    } else {
      /* Classes and structs are functionally equivalent.  If the type needs
         to be changed to tk_struct, that will be done during the full
         instantiation. */
      type_kind = (a_type_kind)tk_class;
    }  /* if */
    sym->variant.class_struct_union.type = class_type = alloc_type(type_kind);
    /* Record the argument list in the type.  It should be available in the
       IL at least for name generation and possibly for debuggers, too. */
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    /* All template instantiations have C++ external linkage, and the type
       is entered in the file scope. */
    class_type->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
    add_to_types_list(class_type, DEPTH_OF_FILE_SCOPE,
                      /*in_old_style_param_decl_list=*/FALSE);
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


static a_boolean class_template_declaration(a_symbol_ptr   *p_sym_ptr,
                                            a_token_cache  *p_token_cache)
/*
If this turns out to be a class template declaration, scan it and return
TRUE, setting *p_sym_ptr to the class template symbol and, if this is a
defining declaration, setting *p_token_cache to the token cache for the
entire declaration (from "class" through the closing right brace).  If it
is not a class declaration, return FALSE.
*/
{
  a_boolean         is_class_template_decl = FALSE;
  a_symbol_locator  locator;
  a_symbol_ptr      sym = NULL;

  db_enter(3, "class_template_declaration");
  if (curr_token == tok_class || curr_token == tok_struct ||
      curr_token == tok_union) {
    /* This appears to be a class template declaration -- though it could
       be a function template declaration with a return type using one of
       these keywords.  We'll proceed on the assumption that it is indeed
       a class template until we see evidence to the contrary. */
    is_class_template_decl = TRUE;
    /* Bypass "class", "struct", or "union".  The token has to be cached
       because instantiation is done by class_specifier, which expects it. */
    cache_curr_token(p_token_cache);
    (void)get_token();
    /* Next should be the class name. */
    if (!is_qualified_name_start()) {  /* Identifier or "::". */
      /* Not an identifier.  Cache a dummy identifier token and proceed. */
      error(ec_exp_identifier);
      curr_token = tok_identifier;
      set_to_error_locator(locator_for_curr_id);
      cache_curr_token(p_token_cache);
      locator = locator_for_curr_id;
    } else {
      /* Look up the identifier.  If it's a qualified name there will be an
         error down the line. */
      sym = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
      /* Cache the identifier and advance past it so we can discriminate
         between a class template and a function template. */
      cache_curr_token(p_token_cache);
      locator = locator_for_curr_id;
      (void)get_token();
      if (is_declarator_start()) {
        /* Since the current token appears to be the start of a declarator
           this looks like a function template declaration after all.  Return
           to the caller, but first rewind to the start of the return type
           declaration. */
        is_class_template_decl = FALSE;
#if 0
        reset_token_cache_persistence(p_token_cache);
#endif /* if 0 */
        rescan_cached_tokens(p_token_cache);
        sym = NULL;
        clear_token_cache(p_token_cache);
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
    /* If get_normal_id_or_qualified_name returned something, we may have a
       name conflict or a redefinition. */
    if (sym != NULL) {
      if (sym->kind == (a_symbol_kind)sk_class_template) {
        if (sym->defined &&
            (curr_token == tok_colon || curr_token == tok_lbrace)) {
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
    /* If this is a class template definition, continue caching all the tokens
       that comprise it. */
    if (curr_token == tok_colon || curr_token == tok_lbrace) {
      sym->defined = TRUE;
      /* Now scan the remaining tokens. */
      add_stop_token(tok_semicolon);
      if (curr_token == tok_colon) {
        /* Scan the tokens in the base class declarations, stopping when
           the "{" is reached. */
        add_stop_token(tok_lbrace);
        cache_token_stream(p_token_cache);
        remove_stop_token(tok_lbrace);
      }  /* if */
      remove_stop_token(tok_semicolon);
      /* Scan the class body.  If the body is missing the error will be
         found during prototype instantiation. */
      if (curr_token == tok_lbrace) {
        /* Swallow the "{" and then cache everything through to the "}". */
        cache_curr_token(p_token_cache);
        (void)get_token();
        add_stop_token(tok_rbrace);
        cache_token_stream(p_token_cache);
        remove_stop_token(tok_rbrace);
        /* Now cache the "}" (unless we didn't find one). */
        if (curr_token == tok_rbrace) {
          cache_curr_token(p_token_cache);
          /* Add an end-of-source token after the right brace. */
          curr_token = tok_end_of_source;
          cache_curr_token(p_token_cache);
          (void)get_token();
#if CHECKING
        } else if (curr_token != tok_end_of_source) {
          internal_error("class_template_declaration: expected end-of-source");
#endif /* CHECKING */
        }  /* if */
      }  /* if */
    } else {
      /* This is not a class template declaration, so we have no need to
         cache the tokens. */
#if 0
      free_token_cache(p_token_cache);
#else
      clear_token_cache(p_token_cache);
#endif /* if 0 */
    }  /* if */
    /* Note that the semicolon is not cached. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  }  /* if */
done:;
  db_exit();
  *p_sym_ptr = sym;
  return is_class_template_decl;
}  /* class_template_declaration */


static void function_template_declaration(a_symbol_ptr   *sym,
                                          a_token_cache  *p_token_cache)
/*
*/
{
  a_storage_class       storage_class;
  a_type_ptr            type;
  a_symbol_locator      locator;
  a_decl_flag_set       do_flags, dso_flags;
  a_func_info_block     func_info;
  a_type_ptr            bottom_derived_type = NULL;
  an_expr_node_ptr      dim_expr_ptr;

  db_enter(3, "function_template_declaration");

  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  (void)decl_specifiers((DSI_IS_TEMPLATE_DECLARATION |
                         DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_STORAGE_CLASS_SPECIFIER_ALLOWED),
                         &dso_flags, &storage_class, &type);
  declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags, type, (a_type_ptr)NULL,
             &locator, &type, &bottom_derived_type, &func_info,
             &dim_expr_ptr);
  *sym = enter_symbol((a_symbol_kind)sk_function_template, &locator,
                     DEPTH_OF_FILE_SCOPE, /*suppress_redecl_error=*/FALSE);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  if (curr_token == tok_lbrace) {
    (*sym)->defined = TRUE;
    clear_token_cache(p_token_cache);
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
      curr_token = tok_end_of_source;
      cache_curr_token(p_token_cache);
      /* Advance to the next token. */
      (void)get_token();
    }  /* if */
  } else {
    /* No body to cache.  Check for final semicolon. */
    (void)required_token(tok_semicolon, ec_exp_semicolon);
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
      sym = enter_symbol((a_symbol_kind)sk_constant, &locator_for_curr_id,
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


void template_declaration(void)
/*
*/
{
  a_template_param_ptr              template_param, template_param_list = NULL;
  a_symbol_ptr                      sym;
  a_token_cache                     token_cache;
  a_template_symbol_supplement_ptr  tssp;

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
  push_scope((a_scope_kind)sck_template_declaration, NO_SCOPE_NUMBER,
             (a_type_ptr)NULL, (a_routine_ptr)NULL);
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
  clear_token_cache(&token_cache);
  if (!class_template_declaration(&sym, &token_cache)) {
    function_template_declaration(&sym, &token_cache);
  }  /* if */
  tssp = sym->variant.template.extra_info;
  tssp->parameters = template_param_list;
  tssp->template_body = token_cache;
  tssp->declaration_scope = scope_stack[decl_scope_level].number;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "template symbol: ", 2);
  }  /* if */
#endif /* DEBUG */
  pop_scope();
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
