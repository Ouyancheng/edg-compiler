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
#include "decls.h"
#include "error.h"
#include "il.h"
#include "lexical.h"
#include "symbol_tbl.h"
#include "types.h"


static a_boolean equiv_template_arg_lists(a_template_arg_ptr  list1,
                                          a_template_arg_ptr  list2)
{
  a_boolean           equiv = TRUE;
  a_template_arg_ptr  arg1, arg2;

#if CHECKING
  if (list1 == NULL || list2 == NULL) {
    internal_error("equiv_template_arg_lists: NULL list");
  }  /* if */
#endif /* CHECKING */
  arg1 = list1;
  arg2 = list2;
  do {
    if (arg1->is_type != arg2->is_type) {
      equiv = FALSE;
      break;
    }  /* if */
    if (arg1->is_type) {
      if (!identical_types(arg1->variant.type, arg2->variant.type)) {
        equiv = FALSE;
        break;
      }  /* if */
    } else {
      if (!eq_constants(arg1->variant.constant, arg2->variant.constant)) {
        equiv = FALSE;
        break;
      }  /* if */
    }  /* if */
    arg1 = arg1->next;
    arg2 = arg2->next;
    if ((arg1 == NULL) != (arg2 == NULL)) {
      equiv = FALSE;
      break;
    }  /* if */
  } while (arg1 != NULL);
  return equiv;
}  /* equiv_template_arg_lists */


a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                 a_template_arg_ptr  new_list,
                                 a_source_position   *source_pos)
/*
*/
{
  a_symbol_ptr        sym;
  a_template_arg_ptr  old_list;
  a_type_kind         type_kind;
  a_type_ptr          class_type;

  db_enter(3, "find_template_class");
  sym = class_template_sym->variant.template.extra_info->
                                        variant.class.instantiations;
  for (; sym != NULL; sym = sym->next) {
    old_list = sym->variant.type->
                     variant.class_struct_union.extra_info->template_arg_list;
    if (equiv_template_arg_lists(old_list, new_list)) {
      /* We've found it. */
#if DEBUG
      if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
  }  /* for */
  if (sym == NULL) {
    sym = make_template_class_symbol(class_template_sym, source_pos);
    if (sym->kind == (a_symbol_kind)sk_union_tag) {
      type_kind = (a_type_kind)tk_union;
    } else {
      type_kind = (a_type_kind)tk_class;
    }  /* if */
    sym->variant.class_struct_union.type = class_type = alloc_type(type_kind);
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    class_type->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
    add_to_types_list(class_type, DEPTH_OF_FILE_SCOPE,
                      /*in_old_style_param_decl_list=*/FALSE);

#if DEBUG
    if (debug_level >= 3) db_symbol(sym, "created: ", 2);
#endif /* DEBUG */
  }  /* if */
  db_exit();
  return sym;
}  /* find_template_class */


static a_symbol_ptr class_template_declaration(void)
/*
*/
{
  a_symbol_ptr      sym;

  db_enter(3, "class_template_declaration");
  /* Bypass "class", "struct", or "union". */
  (void)get_token();
  sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_class_template) {
      if (!sym->defined) {
        /* Reuse the symbol from a previous declaration. */
      } else {
        sym_error(ec_already_defined, sym);
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    sym = enter_symbol((a_symbol_kind)sk_class_template, &locator_for_curr_id,
                       DEPTH_OF_FILE_SCOPE, /*suppress_redecl_error=*/FALSE);
  }  /* if */
  /* Bypass the identifier. */
  if (curr_token == tok_colon || curr_token == tok_lbrace) {
    sym->defined = TRUE;
  }  /* if */    
  db_exit();
  return sym;
}  /* class_template_declaration */


static a_template_param_ptr scan_template_param_list(void)
/*
*/
{
  a_decl_flag_set      do_flags, dso_flags;
  a_symbol_ptr         sym;
  a_type_ptr           param_type_ptr;
  a_source_position    param_type_pos;
  a_storage_class      param_storage_class;
  a_symbol_locator     param_locator;
  a_type_ptr           bottom_derived_type;
  an_expr_node_ptr     dim_expr_ptr;
  a_template_param_ptr template_param;
  a_template_param_ptr template_param_list = NULL;
  a_template_param_ptr end_of_template_param_list = NULL;

  db_enter(3, "scan_template_param_list");
  do {
    add_stop_token(tok_comma);
    copy_source_position(pos_curr_token, param_type_pos);
    if (curr_token == tok_class && next_token() == tok_identifier) {
      /* A type-argument. Note that there is a possible ambiguity here:
         template <class T> vs. template <class T X>, where in the second
         case T is already declared.  One could argue that the second is an
         "arg-declaration" rather than a "type-argument", but the working
         paper (14.1 para 2) appears to resolve the ambiguity in favor of
         always interpreting <class T ... as a type-argument. */
      /* Bypass "class". */
      (void)get_token();
      sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.type = error_type();
      /* Bypass the identifier. */
      (void)get_token();
    } else {
      /* Not a type-argument, so treat it as an arg-declaration.  If this
         template declaration happens to be of a function rather than a class,
         arg-declarations are not allowed.  That will be detected later. */
      /* Scan the declaration specifiers. */
      (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                             DSI_IS_TEMPLATE_PARAMETER),
                             &dso_flags, &param_storage_class,
                             &param_type_ptr);
      if (dso_flags & DSO_DEFINES_SOMETHING) {
        pos_error(ec_type_definition_not_allowed, &param_type_pos);
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
      /* Enter a symbool and bind an error constant to it temporarily.  At the
         point of instantiation an actual constant will be substituted. */
      sym = enter_symbol((a_symbol_kind)sk_constant, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.constant = fs_constant((a_constant_repr_kind)ck_error);
      sym->variant.constant->type = param_type_ptr;
      set_source_corresp(&sym->variant.constant->source_corresp, sym);
    }  /* if */
    /* Allocate a template parameter and add it to the end of the list. */
    template_param = alloc_template_param();
    template_param->param_symbol = sym;
    if (template_param_list == NULL) {
      template_param_list = template_param;
    } else {
      end_of_template_param_list->next = template_param;
    }  /* if */
    end_of_template_param_list = template_param;
    remove_stop_token(tok_comma);
  } while (loop_token(tok_comma));
  db_exit();
  return template_param_list;
}  /* scan_template_param_list */


void template_declaration(void)
/*
*/
{
  a_template_param_ptr  template_param, template_param_list = NULL;
  a_decl_flag_set       do_flags, dso_flags;
  a_type_ptr            type;
  a_storage_class       storage_class;
  a_func_info_block     func_info;
  a_type_ptr            bottom_derived_type = NULL;
  an_expr_node_ptr      dim_expr_ptr;
  a_symbol_ptr          sym;
  a_symbol_locator      locator;
  a_boolean             semicolon_expected;
  a_token_cache         *p_token_cache;

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
  (void)required_token(tok_gt, ec_exp_gt);
#if 0
  /* Cache all the tokens that remain in the template declaration. */
#endif /* if 0 */
  if (is_decl_start(/*expr_context=*/FALSE,
                    /*real_declarator_allowed=*/TRUE)) {
    (void)decl_specifiers((DSI_IS_TEMPLATE_DECLARATION |
                           DSI_TYPE_SPECIFIER_ALLOWED |
                           DSI_STORAGE_CLASS_SPECIFIER_ALLOWED),
                           &dso_flags, &storage_class, &type);
  }  /* if */
  if (dso_flags & DSO_CLASS_TEMPLATE) {
    sym = class_template_declaration();
  } else {
    /* This must be a function template declaration. */
    declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags, type, (a_type_ptr)NULL,
               &locator, &type, &bottom_derived_type, &func_info,
               &dim_expr_ptr);
    sym = enter_symbol((a_symbol_kind)sk_function_template, &locator,
                       DEPTH_OF_FILE_SCOPE, /*suppress_redecl_error=*/FALSE);
  }  /* if */
  sym->variant.template.extra_info->parameters = template_param_list;

  p_token_cache = &sym->variant.template.extra_info->template_body;
  clear_token_cache(p_token_cache);
  cache_token_stream(p_token_cache);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  cache_curr_token(p_token_cache);
  semicolon_expected = TRUE;
  if (curr_token == tok_lbrace) {
    (void)get_token();
    add_stop_token(tok_rbrace);
    cache_token_stream(p_token_cache);
    remove_stop_token(tok_rbrace);
    if (curr_token == tok_rbrace) {
      cache_curr_token(p_token_cache);
      (void)get_token();
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        semicolon_expected = FALSE;
        if (curr_token != tok_semicolon) {
          error(ec_exp_semicolon);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (semicolon_expected) {
    if (curr_token == tok_semicolon) {
      cache_curr_token(p_token_cache);
      (void)get_token();
    } else {
      error(ec_exp_semicolon);
    }  /* if */
  }  /* if */

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
