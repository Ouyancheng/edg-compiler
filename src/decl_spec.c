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

decl_spec.c -- Scanning of declaration specifiers.

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
#include "folding.h"


static a_boolean tag_currently_being_defined(a_type_ptr tag_type)
/*
Returns TRUE if the type pointed to by tag_type is in the process
of being defined.  This is determined by examining any
class/struct/union scopes on the scope stack.  This is only used in
C mode.
*/
{
  a_scope_depth	depth;
  a_boolean	result = FALSE;

  for (depth = depth_scope_stack ;depth != DEPTH_OF_FILE_SCOPE; depth--) {
    if (scope_stack[depth].kind == (a_scope_kind)sck_class_struct_union) {
      if (scope_stack[depth].assoc_type == tag_type) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* tag_currently_being_defined */


static void check_template_param_tag_kind(a_symbol_ptr	   tag_sym,
					  a_symbol_kind	   tag_kind,
					  a_boolean        *tag_err)
/*
This routine is called when a template parameter has been used in an
elaborated type specifier.  If this is the first time the parameter has
been used in such a context, we record the tag kind in the
template_param_type_descr.  If it is not the first time, we make sure
that this usage is consistent with the previous usage.
*/
{
  a_type_ptr				tp = tag_sym->variant.type;
  a_type_kind				prev_tag_type_kind;
  a_type_kind				new_tag_type_kind;
  a_template_param_type_descr_ptr	tptdp;
  check_assertion(tp->kind == (a_type_kind)tk_template_param);
  if (tp->variant.template_param.descr == NULL) {
    /* The tk_template_param type does not yet have a description
       entry, allocate one. */
    tp->variant.template_param.descr = alloc_template_param_type_descr();
  }  /* if */
  /* If this template parameter has already been used in an elaborated
     type specifier, make sure that the current tag kind is consistent
     with the previous use. */
  tptdp = tp->variant.template_param.descr;
  prev_tag_type_kind = tptdp->tag_kind;
  new_tag_type_kind = type_kind_for_tag_kind(tag_kind);
  if (prev_tag_type_kind == (a_type_kind)tk_unknown) {
    /* The template parameter does not yet have a tag kind.  Assign
       it the current tag kind. */
    tptdp->tag_kind = new_tag_type_kind;
  } else if (prev_tag_type_kind != new_tag_type_kind) {
    /* Error -- the new tag kind does not match the previous use. */
    pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                   &locator_for_curr_id.source_position,
                   name_of_symbol_kind(tag_kind), tag_sym);
    *tag_err = TRUE;
    tag_sym = NULL;
  }  /* if */
}  /* check_template_param_tag_kind */


static a_symbol_ptr scan_tag_name(a_symbol_kind     tag_kind,
                                  a_symbol_locator  *locator,
                                  a_boolean         check_for_vacuous_decl,
                                  a_boolean         is_ref_within_new_expr,
                                  a_scope_depth     *effective_decl_level,
                                  a_boolean         *tag_resolution)
/*
Scan a tag identifier for a class, struct, union, or enum declaration.
If a tag symbol already exists for the identifier, return a pointer to
that symbol; otherwise return NULL.  If there is no identifier or if there
is an error, return NULL.

This routine may look more complicated than is necessary -- it isn't.
This routine can either be matching up a definition with a previous
declaration or may be entering a definition in a new scope.  The lookups
have to be done very carefully to create new entries only when required
and to find existing entries only when appropriate.  Exercise great
caution when modifying this routine.
*/
{
  a_symbol_ptr      tag_sym = NULL;
  a_token_kind      next_tok;
  a_boolean	    err = FALSE;
  a_boolean	    tag_err = FALSE;

  db_enter(3, "scan_tag_name");
  *tag_resolution = FALSE;
  /* Check for the presence of a qualified name.  If we have a qualified
     name, do the lookup in a manner that will only find tag names. */
  if (coalesce_and_lookup_qualified_name(GID_TEMPLATE_ARGS_OPTIONAL,
                                         ilm_tag, &err)) {
    if (err) {
      /* An error occurred while scanning or looking up the qualified
         name. */
      tag_err = TRUE;
    } else {
      tag_sym = locator_for_curr_id.specific_symbol;
      if (tag_sym != NULL && tag_sym->kind != tag_kind) {
        /* A qualified name is being used with a different tag kind than
           that of its declaration.  Issue an error. */
        if (tag_sym->kind == (a_symbol_kind)sk_type &&
            tag_sym->variant.type->kind == (a_type_kind)tk_template_param) {
          /* This is a template parameter during a prototype instantiation.
             Don't issue an error.  This will be checked during real
	     instantiations. */
        } else {
          pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                         &locator_for_curr_id.source_position,
                         name_of_symbol_kind(tag_kind), tag_sym);
          tag_sym = NULL;
          tag_err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus &&
             curr_token == tok_identifier &&
             decl_scope_level == DEPTH_OF_FILE_SCOPE &&
             tag_kind != (a_symbol_kind)sk_enum_tag) {
    /* Check for an identifier that is a class template name.  A class
       template name at file scope must have an argument list.  A use
       of a class template name in another scope is actually a
       declaration of a new class that has nothing to do with the template. */
    a_symbol_ptr  templ_sym;
    /* Look up what may be a class template symbol.  If the name is
       the start of a qualified name (e.g., A::B) or has a template
       argument list (e.g., A<T>) it will have been coalesced by the
       call to coalesce_and_lookup_qualified_name.  If it just a simple
       identifier (e.g., "A") we need look it up and coalesce it here. */
    templ_sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    /* There are two situations that need to be handled: this could be the
       first time we are scanning this template reference -- in which case
       we need to scan the arguments (using coalesce_template_class_reference).
       Alternately, the arguments may have already been coalesced.  If the
       symbol is a class template then we need to scan the arguments.  If
       the symbol is a template class symbol then the arguments have already
       been scanned and we should simply use the symbol returned by
       normal_id_lookup. */
    if (templ_sym != NULL) {
      if (templ_sym->kind == (a_symbol_kind)sk_class_template) {
        tag_sym = coalesce_template_class_reference(templ_sym, GID_NO_OPTIONS,
                                                    &err);
        /* If an error occurred while scanning the template arguments, set
           tag_sym to NULL.  The caller is not prepared for it to point to
           an error symbol. */
        if (err) {
          tag_sym = NULL;
          tag_err = TRUE;
        }  /* if */
      } else if (is_template_class_symbol(templ_sym)) {
        /* Use the symbol pointer from the locator (returned by
           normal_id_lookup earlier).  The template class reference has
           already been coalesced. */
        tag_sym = templ_sym;
      } else {
        /* Don't prejudice subsequent lookups. */
        clear_specific_symbol(locator_for_curr_id);
      }  /* if */
    }  /* if */
  }  /* if */
  if (tag_sym != NULL) {
    /* Tag symbol is a qualified name or a template class reference. */
    /* Return a copy of the locator to the caller. */
    *locator = locator_for_curr_id;
  } else if (tag_err) {
    /* An error occurred while handling a qualified name or a template
       reference earlier. */
  } else {
    /* Look for a tag symbol in the current scope.  If the tag kind does
       not match the tag being processed, issue an error. */
    tag_sym = curr_scope_id_lookup(&locator_for_curr_id, IDL_MUST_BE_TAG);
    if (tag_sym != NULL && tag_sym->kind != tag_kind) {
      pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                     &locator_for_curr_id.source_position,
                     name_of_symbol_kind(tag_kind), tag_sym);
      tag_sym = NULL;
      tag_err = TRUE;
      goto done;
    }  /* if */
    /* Save the symbol locator for this identifier before doing the
       get_token. */
    *locator = locator_for_curr_id;
    next_tok = next_token();
    if (next_tok == tok_lbrace ||
        (next_tok == tok_colon && C_dialect == C_dialect_cplusplus &&
         tag_kind != (a_symbol_kind)sk_enum_tag && !is_ref_within_new_expr)) {
      /* The token following the tag marks the start of a class or enum
         definition. Determine whether it is the resolution of a previous
         incomplete declaration. */
      /* Note that we had to check the is_ref_within_new_expr flag because
         a colon has a different meaning in an expression context than
         in a declaration context (namely, it may belong to a ?: operator). */
      if (tag_sym != NULL) {
        a_type_ptr	tag_type = type_symbol_type(tag_sym);
        /* The tag has already appeared in the current scope. */
        if (is_incomplete_type(tag_type) &&
            (!C_mode() || !tag_currently_being_defined(tag_type))) {
          /* Resolution of a previous incomplete declaration.  In C mode, make
             sure that an incomplete type is not in the process of being
             defined. */
          *tag_resolution = TRUE;
        } else {
          /* Redeclaration of a tag that has already been defined.  Set
             tag_sym to NULL and let enter_symbol issue an error. */
          tag_sym = NULL;
        }  /* if */
      }  /* if */
    } else if (tag_sym == NULL) {
      /* This is the first appearance of the tag in the current scope.  This
         is not its definition, so it is either a reference to an existing
         tag or a declaration of a new (incomplete) tag. */
      /* Check for a cfront bug (violation of ARM 7.1.3, which says a typedef
         name may not appear in an elaborated type specifier) which allows
         a typedef name as long as it refers to a class/struct/union type. */
      if (any_cfront_mode() && tag_kind != (a_symbol_kind)sk_enum_tag) {
        /* Look up the name again in the current scope, but this time don't
           restrict the search to tag names. */
        a_symbol_ptr  sym;

        check_assertion(locator->specific_symbol == NULL);
        sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS);
        if (sym != NULL) {
          /* Found a symbol of the same name that was declared in the current
             scope. */
          if (sym->kind == (a_symbol_kind)sk_type) {
            /* Name is already declared in the current scope as a typedef. */
            a_type_ptr  tp = skip_typerefs(sym->variant.type);
            if (is_immediate_class_type(tp) &&
                ((tag_kind == (a_symbol_kind)sk_union_tag) ==
                 (tp->kind == (a_type_kind)tk_union))) {
              /* This is the special case.  Return an sk_type symbol instead
                 of the normally expected sk_class_or_struct_tag. */
              tag_sym = sym;
              goto done;
            }  /* if */
          }  /* if */
          /* Reset the specific_symbol pointer to avoid prejudicing any
             subsequent lookup. */
          clear_specific_symbol(*locator);
        }  /* if */
      }  /* if */
      /* Check for a "vacuous declaration" (e.g. "struct S;" or "enum E;").
         The effect of a vacuous declaration (unless we are in pcc mode) is
         to establish the name in the current scope, even if the tag name
         exists in a containing scope or is inherited from a base class. */
      if (next_tok == tok_semicolon && check_for_vacuous_decl &&
          C_dialect != C_dialect_pcc) {
        /* This is indeed a vacuous declaration.  Leave tag_sym set to NULL
           to force the creation of a new symbol in the current scope. */
      } else {
        /* This may be a reference to an existing tag from a containing
           scope or a base class.  This can be ascertained by doing a full
           lookup of the tag name (before, it was done just for the current
           scope). */
        tag_sym = curr_tag_symbol(locator, tag_kind);
        if (tag_sym == NULL) {
          /* We will need to enter an incomplete tag that may be resolved
             later.  Just leave tag_sym NULL.  In C it will be entered at
             the scope level indicated by decl_scope_level.  In C++ we need
             to pop out to the innermost non-class/non-prototype scope.
             (For example, to introduce class name B in a parameter
             declaration of a member function within the definition of class
             A does not introduce the name of nested class A::B; rather, B
             is entered in the same scope as A.) */
          if (C_dialect == C_dialect_cplusplus) {
            /* Pop out to the containing scope -- file scope, function scope,
               or block scope.  *effective_decl_level will already have been
               initialized to decl_scope_level. */
            a_boolean     done = FALSE;
            a_symbol_ptr  instance_sym;

            do {
              switch (scope_stack[*effective_decl_level].kind) {
                case sck_template_instantiation:
                  /* We hit a template instantiation scope.  If the
                     instantiation scope is for a real instantiation then
                     set effective_decl_level to file scope.  If it is a
                     prototype or nonreal instantiation then leave
                     effective_decl_level pointing at the instantiation
                     scope.  The problem is that a class declared in a
                     prototype instantiation may not be a real type, but we
                     don't know yet.  We want to avoid contaminating the name
                     space, etc., so it gets declared in the instantiation
                     scope. */
                  instance_sym =
                             scope_stack[*effective_decl_level].instance_sym;
                  if (instance_sym == NULL ||
                      is_real_class_symbol(instance_sym)) {
                    *effective_decl_level = DEPTH_OF_FILE_SCOPE;
                  }  /* if */
                case sck_file:
                case sck_function:
                case sck_block:
                  done = TRUE;
                  break;
                default:
                  (*effective_decl_level)--;
              }  /* if */
            } while (!done);
          }  /* if */
        } else if (!C_mode() && tag_sym->kind == (a_symbol_kind)sk_type) {
          /* A tag symbol was found from an enclosing scope.  If this is
             a template parameter symbol, make sure the tag kind is
             consistent with any previous declarations. */
          check_template_param_tag_kind(tag_sym, tag_kind, &tag_err);
        }  /* if */
      }  /* if */
      if (tag_sym == NULL && tag_kind == (a_symbol_kind)sk_enum_tag) {
        /* Since tag_sym was not found, this is either a vacuous declaration
           or a reference to an incomplete (because not yet declared) type.
           In either case this is non-standard for enums.  It is allowed as
           an extension by analogy with classes. */
        if (strict_ansi_mode) {
          /* Incomplete enum declarations are nonstandard in C and C++. */
          pos_diagnostic(strict_ansi_error_severity,
                         ec_nonstd_forward_def_enum,
                         &locator->source_position);
        }  /* if */
      }  
    }  /* if */
  }  /* if */
done:
  if (tag_err) {
    /* If an error occurred while scanning the tag, make the locator that
       is returned to the caller an error locator. */
    set_to_error_locator(*locator);
  }  /* if */
  /* Now that we have completed the lookup on the tag identifier we can
     advance past it. */
  (void)get_token();
  db_exit();
  return tag_sym;
}  /* scan_tag_name */


static a_boolean class_specifier(a_boolean  vacuous_decl_allowed,
                                 a_boolean  is_friend_decl,
                                 a_boolean  is_ref_within_new_expr,
                                 a_type_ptr *type_ptr,
                                 a_boolean  *declares_something,
                                 a_boolean  *defines_something)
/*
Scan a class-specifier (3.5.2.1), which declares a struct or
union type.  The syntax is

9
        class-specifier:
                class-head { member-list    }
                                        opt

        class-head:
                class-key identifier    base-spec
                                    opt          opt
                class-key class-name base-spec
                                              opt

        class-key
                class
                struct
                union

9.2
        member-list
                member-declaration member-list
                                              opt
                access-specifier : member-list

        member-declaration:
                decl-specifiers    member-declarator-list    ;
                               opt                       opt
                function-definition ;
                                     opt
                qualified-name ;

        member-declarator-list:
                member-declarator
                member-declarator-list , member-declarator

        member-declarator
                declarator pure-specifier
                                         opt
                identifier    : constant-expression
                          opt

        pure-specifier
                = 0

The type is returned in *type_ptr. *declares_something is set to indicate
whether or not this specifier declares something, and *defines_something
to indicate whether the class/struct/union is actually defined.
*/
{
  a_symbol_kind           tag_kind;
  a_type_kind             type_kind;
  a_symbol_locator        locator;
  a_symbol_ptr            tag_sym, error_tag_sym = NULL;
  a_boolean               tag_id_present;
  a_type_ptr              class_type;
  a_boolean               is_local_class = FALSE;
  a_boolean               is_template_class_instantiation = FALSE;
  a_boolean               tag_resolution = FALSE;
  a_boolean               err = FALSE;
  a_scope_depth           effective_decl_level = decl_scope_level;
  a_boolean               is_class_definition;
  a_source_position       decl_start_pos;
  a_scope_stack_entry_ptr ssep;
  a_source_position       tag_position;
  a_symbol_reference_kind srk_flags;

  db_enter(3, "class_specifier");
  *declares_something = FALSE;
  *defines_something = FALSE;
  decl_start_pos = pos_curr_token;
  /* Determine whether this is a template class instantiation or a local
     class (one being declared within a function scope). */
  ssep = &scope_stack[depth_scope_stack];
  if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    is_template_class_instantiation = TRUE;
    class_type = scope_stack[depth_scope_stack].assoc_type;
    tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
    if (curr_token == tok_struct) {
      class_type->kind = (a_type_kind)tk_struct;
    }  /* if */
    type_kind = class_type->kind;
    (void)get_token();
    (void)get_token();
    goto skip_tag_scan;
  } else if (depth_innermost_function_scope != NO_SCOPE_NUMBER ||
             inside_local_class) {
    /* This declaration appears within a function or block scope, or else it
       is a nested class declaration within a local class.  In either case,
       it is a local class. */
    is_local_class = TRUE;
  }  /* if */
  if (!is_qualified_name_start()) {
    /* Skip over "class", "struct", or "union", remembering which appears. */
    check_assertion(curr_token == tok_class || curr_token == tok_struct ||
                    curr_token == tok_union);
    if (curr_token == tok_union) {
      tag_kind = (a_symbol_kind)sk_union_tag;
      type_kind = (a_type_kind)tk_union;
    } else {
      tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
      type_kind = (a_type_kind)(curr_token == tok_struct ?
                                                    tk_struct : tk_class);
    }  /* if */
    /* If there is an identifier next, it is a tag.  It can be the declaration
       of a new tag or a reference to an existing tag.  Although it is an
       error, also be on the lookout for a qualified name. */
    (void)get_token();
    tag_id_present = is_qualified_name_start();
  } else {
    /* class_specifier is called with is_friend_decl TRUE only when the name
       has not yet been declared; this happens in cfront compatibility mode
       only.  Default kind is "class" when a class is introduced by a friend
       declaration.   (In fact, there is a slight incompatibility here, since
       in cfront 2.1 this can also be turned into a union declaration.) */
    check_assertion(is_friend_decl);
    tag_id_present = TRUE;
    tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
    type_kind = (a_type_kind)tk_class;
  }  /* if */
  if (tag_id_present) {
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    tag_position = pos_curr_token;
    *declares_something = TRUE;
    check_assertion(!vacuous_decl_allowed || !is_friend_decl);
    tag_sym = scan_tag_name(tag_kind, &locator, vacuous_decl_allowed,
                            is_ref_within_new_expr, &effective_decl_level,
                            &tag_resolution);
    if (tag_sym != NULL) {
      /* Check for tag mismatch.  This can only happen when an instance of a
         class template is being referenced in an elaborated type specifier. */
      if (tag_sym->kind == (a_symbol_kind)sk_type) {
#if CHECKING
        if (tag_sym->variant.type->kind == (a_type_kind)tk_template_param) {
          /* Template param used in with a class-key -- for instance:
               template <class T> class A {
                 class T x;
               };
             During prototype instantiation we have to assume that T can be a
             valid class name.  Therefore "class T x" is treated as synonymous
             with "T x".  In addition, "friend class T" is also supported. */
        } else if (any_cfront_mode()) {
          /* Cfront bug that allows this:
               typedef class A B;
               class B;
               class B *pa;
             The current declaration must not be a definition and the
             typedef name must refer to a class type. */
          check_assertion(is_class_struct_union_type(tag_sym->variant.type));
        } else {
          internal_error("class_specifier: invalid sk_type tag_sym");
        }  /* if */
#endif /* CHECKING */
      } else if (tag_sym->kind != tag_kind) {
        check_assertion(is_template_class_symbol(tag_sym));
        /* Error -- tag-kind mismatch in a specialization. */
        pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos,
                     tag_sym->variant.class_struct_union.extra_info->
                                                            class_template);
        set_to_named_error_locator(locator);
        error_tag_sym = tag_sym;
        tag_sym = NULL;
      }  /* if */
    }  /* if */
    if (is_error_locator(locator)) err = TRUE;
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    /* Don't leave tag_position undefined. */
    tag_position = decl_start_pos;
    set_to_error_locator(locator);
    if (is_ref_within_new_expr) {
      /* We are within a new expression and no class name is given following
         the keyword -- e.g., "class A *pa = new class;" -- report the missing
         identifier as a syntax error. */
      syntax_error(ec_exp_identifier);
      err = TRUE;
    } else if (curr_token == tok_lbrace ||
               (C_dialect == C_dialect_cplusplus && curr_token == tok_colon)) {
      /* This is a tagless class definition. */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      add_stop_token(tok_lbrace);
      if (C_dialect == C_dialect_cplusplus) add_stop_token(tok_colon);
      syntax_error(ec_exp_definition_of_tag);
      err = TRUE;
      if (C_dialect == C_dialect_cplusplus) remove_stop_token(tok_colon);
      remove_stop_token(tok_lbrace);
    }  /* if */
  }  /* if */
skip_tag_scan:
  /* If the next token is a "{" or, in C++, a ":" (introducing a list of
     base classes) we should expect to scan a class definition.  The exception
     to this is when an elaborated class name (e.g., "struct S" instead of
     simply "S") appears within the context of a new expression.  The
     issue is the colon: since a colon could be part of the expression
     context (e.g., "struct S *ps = flag ? new struct S : 0;") it should
     not be interpreted as introducing a base classes list. */
  is_class_definition = curr_token == tok_lbrace ||
                        (C_dialect == C_dialect_cplusplus &&
                         curr_token == tok_colon && !is_ref_within_new_expr);
  if (is_class_definition && is_friend_decl) {
    /* This is an error.  Defer the diagnostic until we have a tag_sym
       to use for the fill-in.  If tag_sym is already non-NULL, we'll create
       another one. */
    set_to_named_error_locator(locator);
    tag_sym = NULL;
  }  /* if */
  if (tag_sym != NULL && C_dialect == C_dialect_cplusplus) {
    if (tag_sym->kind == (a_symbol_kind)sk_type) {
      if (is_class_definition) {
        /* Attempting to redefine a template parameter name.  Let enter_symbol
           issue an error. */
        tag_sym = NULL;
      }  /* if */
    } else {
      a_class_symbol_supplement_ptr  cssp;

      cssp = tag_sym->variant.class_struct_union.extra_info;
      if (cssp->class_template != NULL) {
        if (is_class_definition && tag_sym->defined) {
          /* This template class has already been instantiated. */
          pos_sy_error(ec_already_defined, &tag_position, tag_sym);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
          set_to_named_error_locator(locator);
          err = TRUE;
        } else if (is_class_definition ||
                   (curr_token == tok_semicolon && !is_friend_decl)) {
          /* We have a specific declaration of a template class. */
          if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
            /* Specific definitions of template classes may only occur at
               file scope. */
            pos_error(ec_specific_def_must_be_global, &tag_position);
            error_tag_sym = tag_sym;
            tag_sym = NULL;
            set_to_named_error_locator(locator);
            err = TRUE;
          } else {
            cssp->is_specific_template_def = TRUE;
          }  /* if */
        }  /* if */
      } else if (is_class_definition) {
        if (tag_sym->class_of_which_a_member != NULL &&
            (ssep->kind != (a_scope_kind)sck_class_struct_union ||
             tag_sym->class_of_which_a_member != ssep->assoc_type)) {
          /* A definition of a nested class that appears in the scope other
             than that of its parent class. */
          pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
          tag_sym = NULL;
          set_to_error_locator(locator);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new class, struct, or union type.  All such types are
       allocated in the file scope memory region, though local types will be
       added to the function scope's types list. */
    class_type = alloc_type(type_kind);
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      /* A type is actually declared in a function prototype scope only in
         C mode.  In C++ the type is injected into a containing scope. */
      check_assertion(err || C_dialect != C_dialect_cplusplus ||
                      is_class_definition);
      class_type->declared_in_function_prototype = TRUE;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && error_tag_sym != NULL) {
      class_type->variant.class_struct_union.extra_info->template_arg_list =
             error_tag_sym->variant.class_struct_union.type->
                      variant.class_struct_union.extra_info->template_arg_list;
    }  /* if */
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "struct {int a; int b;}"). */
    if (tag_id_present) {
      tag_sym = enter_local_symbol(tag_kind, &locator, effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      set_source_corresp(&(class_type->source_corresp), tag_sym);
    } else {
      /* Tagless class, struct, or union.  Create a symbol to represent it;
         though not entered in the symbol table, it is needed to carry
         around some information about classes that is of interest to the
         front end only. */
      tag_sym = make_unnamed_class_symbol(tag_kind, &pos_curr_token);
      /* Although the symbol header has a name of sorts, it should not appear
         in the type, so NULL it out after the call to set_source_corresp. */
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      class_type->source_corresp.name = NULL;
      class_type->variant.class_struct_union.originally_unnamed = TRUE;
    }  /* if */
    tag_sym->variant.class_struct_union.type = class_type;
    if (is_class_definition && is_friend_decl) {
      /* Issuing the diagnostic was deferred till now. */
      pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
      err = TRUE;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* In C classes have no linkage.  In C++ most classes have either
         internal linkage or, for classes declared at file scope and with
         other characteristics (see ARM 3.3), C++ external linkage; local
         classes and classes nested within local classes have no linkage.
         For now give nonlocal classes internal linkage; it may be changed
         later (see check_class_linkage).  Note that even nameless classes
         may be marked as having linkage; this is useful for dealing with
         member functions.) */
      if (!is_local_class) {
        /* Nonlocal class. */
        class_type->source_corresp.name_linkage =
                                         (a_name_linkage_kind)nlk_internal;
      }  /* if */
      /* If this is the declaration of a nested class, set the parent class
         pointer in the tag symbol. */
      if (scope_stack[decl_scope_level].kind ==
                                (a_scope_kind)sck_class_struct_union) {
        /* A new class name is being declared within a class scope. */
        if (is_class_definition ||
            (vacuous_decl_allowed && curr_token == tok_semicolon)) {
          /* Either a definition or a vacuous declaration -- the latter
             introduces a name into the current scope. */
          class_type->source_corresp.class_of_which_a_member =
            tag_sym->class_of_which_a_member =
                              scope_stack[decl_scope_level].assoc_type;
        }  /* if */
      }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
      /* If the current declaration coexists with another declaration in the
         current scope that effectively hides it, record that information in
         the IL. */
      if (!tag_sym->is_error && !is_unnamed_class_symbol(tag_sym)) {
        if (tag_sym->header->symbol != tag_sym &&
            tag_sym->header->symbol->decl_scope == tag_sym->decl_scope) {
          record_defeatable_name_hiding(tag_sym,
                                        /*tag_hidden_by_nontag=*/TRUE,
                                        (a_scope_ptr)NULL);
        }  /* if */
      }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    }  /* if */
    srk_flags = SRK_DECLARATION;
    if (is_class_definition) srk_flags |= SRK_DEFINITION;
    if (is_friend_decl) srk_flags |= SRK_FRIEND;
    record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                              (a_source_sequence_entry_ptr)NULL);
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    if (tag_sym->variant.type->kind == (a_type_kind)tk_template_param) {
      /* Use of template parameter name as a proxy tag name during a
         prototype instantiation. */
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  } else {
    /* Using an existing type.  Fetch the type pointer from it. */
    class_type = tag_sym->variant.class_struct_union.type;
    /* Record cross-reference information. */
    if (is_class_definition ||
        (curr_token == tok_semicolon &&
         (vacuous_decl_allowed || is_friend_decl))) {
      srk_flags = SRK_DECLARATION;
      if (is_friend_decl) srk_flags |= SRK_FRIEND;
      if (is_class_definition) {
        if (!is_template_class_instantiation) {
          srk_flags |= SRK_DEFINITION;
        }  /* if */
        /* Allow for alternating between class and struct, but stay with the
           one associated with the definition.  The difference only affects
           default member access. */
        class_type->kind = type_kind;
      } else {
        /* A declaration of the form "class A;", when A has already been
           declared, is treated as a redeclaration (not a reference). */
      }  /* if */
      record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                                (a_source_sequence_entry_ptr)NULL);
    } else {
      /* Not a definition, not a vacuous declaration, so presumably a
         reference. */
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  if (!(*declares_something) && !is_class_definition) {
    /* A pragma will not bind to a class reference in an elaborated type
       specifier. */
  } else {
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
  }  /* if */
  if (is_class_definition) {
    if (scan_class_definition(class_type, effective_decl_level,
                              is_local_class)) {
      *defines_something = TRUE;
    } else {
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    *type_ptr = error_type();
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    *type_ptr = tag_sym->variant.type;
  } else {
    *type_ptr = class_type;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(tag_sym, "tag_sym: ", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return !err;
}  /* class_specifier */


static void enum_specifier(a_boolean  vacuous_decl_allowed,
                           a_type_ptr *type_ptr,
                           a_boolean  *declares_something,
                           a_boolean  *defines_something)
/*
Scan an enumeration specifier (3.5.2.2).  The syntax is

3.5.2.2
       enum-specifier:
		enum identifier    { enumerator-list }
                               opt
		enum identifier

       enumerator-list:
		enumerator
		enumerator-list , enumerator

       enumerator:
		enumeration-constant
		enumeration-constant = constant-expression

An enumeration-constant is an identifier.

The type is returned in *type_ptr.  *declares_something is set to indicate
whether or not this specifier declares something, and *defines_something
to indicate whether an enumeration is actually defined.
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             tag_sym;
  a_boolean                tag_id_present;
  a_type_ptr               enum_type;
  a_type_ptr               enum_con_type;
  a_symbol_ptr             enum_sym;
  a_constant               constant;
  a_boolean                err, did_not_fold;
  a_constant_ptr           enum_con;
  a_constant_ptr           end_of_enum_con_list;
  a_constant               max_value, min_value;
  a_boolean                done, min_max_set;
  a_source_position        pos_comma;
  a_boolean                prototype_tag_resolution = FALSE;
  a_memory_region_number   region_to_switch_back_to;
  a_scope_stack_entry_ptr  ssep;
  a_type_ptr               class_of_which_a_member;
  an_access_specifier      access;
  a_scope_depth            effective_decl_level = decl_scope_level;

  db_enter(3, "enum_specifier");

  *declares_something = FALSE;
  *defines_something = FALSE;
  ssep = &scope_stack[decl_scope_level];
  if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
    class_of_which_a_member = scope_stack[decl_scope_level].assoc_type;
    access = scope_stack[decl_scope_level].current_access;
  } else {
    class_of_which_a_member = NULL;
    access = (an_access_specifier)as_public;
  }  /* if */
  /* Skip over "enum". */
  check_assertion(curr_token == tok_enum);
  (void)get_token();
  /* If there is an identifier next, it is a tag.  It can be the declaration
     of a new tag or a reference to an existing tag. */
  tag_id_present = is_qualified_name_start();
  if (tag_id_present) {
    a_boolean          tag_resolution;
    a_source_position  tag_position;
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    *declares_something = TRUE;
    tag_position = pos_curr_token;
    tag_sym = scan_tag_name((a_symbol_kind)sk_enum_tag, &locator,
                            vacuous_decl_allowed,
                            /*is_ref_within_new_expr=*/FALSE,
                            &effective_decl_level, &tag_resolution);
    if (tag_resolution) {                            
      /* Resolution of a previous incomplete declaration. */
      if (C_dialect != C_dialect_cplusplus) {
        /* If the tag was declared in a prototype scope and is now being
           resolved within the function, as in
             int f(enum f p) {enum f{a, b};  ... }
           we must switch into the file scope for the duration of the
           definition.  (In C++ a tag declared in a prototype scope
           refers to a file scope type, so this check is not relevant.) */
        if (ssep->kind == (a_scope_kind)sck_function) {
          prototype_tag_resolution = TRUE;
        }  /* if */
      }  /* if */
      if (effective_decl_level != decl_scope_level) {
        class_of_which_a_member = NULL;
        access = (an_access_specifier)as_public;
      }  /* if */
    } else if (tag_sym != NULL && curr_token == tok_lbrace) {
      /* This is a definition of an enumeration that has previously been
         declared. */
      if (tag_sym->class_of_which_a_member != NULL &&
          tag_sym->class_of_which_a_member != class_of_which_a_member) {
        /* This is an attempt to define a member enum outside the class of
           which it is a member. */
        pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
        tag_sym = NULL;
        set_to_error_locator(locator);
      }  /* if */
    }  /* if */
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    set_to_error_locator(locator);
    if (curr_token == tok_lbrace) {
      /* This is a tagless class definition. */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      add_stop_token(tok_lbrace);
      syntax_error(ec_exp_definition_of_tag);
      remove_stop_token(tok_lbrace);
      /* This statement might have declared something, but since we're
         scanning past the relevant tokens we'll never know.  Set the flag
         to TRUE anyway, to avoid other errors down the line. */
      *declares_something = TRUE;
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new enumerated type.  All enumeration type entries are
       allocated in the file scope memory region. */
    enum_type = alloc_type((a_type_kind)tk_integer);
    /* set_type_size is called later, once the final type is known. */
    /* Set a default representation of "int", which may be adjusted later. */
    enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    enum_type->variant.integer.enum_type = TRUE;
    enum_type->variant.integer.enum_info.constant_list = NULL;
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      enum_type->declared_in_function_prototype = TRUE;
    }  /* if */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "enum {a, b, c}"). */
    if (tag_id_present) {
      tag_sym = enter_local_symbol((a_symbol_kind)sk_enum_tag, &locator,
                                   effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      *declares_something = TRUE;
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      tag_sym->class_of_which_a_member = class_of_which_a_member;
      tag_sym->variant.type = enum_type;
      if (curr_token == tok_lbrace) {
        mark_defined(tag_sym, &locator.source_position);
      } else {
        mark_declared(tag_sym, &locator.source_position);
      }  /* if */
    } else if (curr_token == tok_lbrace) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* An unnamed enum type.  mark_defined can't be called to put out a
         source sequence entry for it, but we need one anyway, so call
         the subroutine directly. */
      update_source_sequence_list((char *)enum_type,
                                  (an_il_entry_kind)iek_type,
                                  (a_source_sequence_entry_ptr)NULL);
#endif  /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* set_source_corresp and mark_defined are not called, so copy in
         the decl position manually. */
      enum_type->source_corresp.decl_position = locator.source_position;
    }  /* if */
    /* When an enumeration is defined within a class definition, its access
       should be set based on the access recorded in the current scope stack
       entry and its parent class should be recorded. */
    enum_type->source_corresp.access = access;
    enum_type->source_corresp.class_of_which_a_member =
                                                class_of_which_a_member;
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
  } else {
    /* Using an existing type.  Fetch the enumerated type pointer from it. */
    enum_type = tag_sym->variant.type;
    /* Record cross-reference information. */
    if (curr_token == tok_lbrace) {
      mark_defined(tag_sym, &locator.source_position);
    } else if (curr_token == tok_semicolon && !strict_ansi_mode) {
      /* A useless redeclaration of an enum tag. */
      mark_declared(tag_sym, &locator.source_position);
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  if (tag_sym != NULL) {
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an unnamed
       enum. */
    cannot_bind_to_curr_construct();
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* Scan the enumeration itself.  Since the enumeration type entry is
       allocated in the file scope memory region, all its components should
       also be.  Switch to the file scope memory region here at the start of
       the definition and switch back when we reach the right brace. */
    *defines_something = TRUE;
    (void)get_token();
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ (see ARM 7.2) the type of an enumerator is the same as that
         of its enumeration -- i.e., enum_type and enum_con_type are the
         same.  enum_type may have to be adjusted later to match the range
         of the enumeration constant values. */
      enum_con_type = enum_type;
    } else {
      /* The type of the constants is always "int", regardless of
         the type of the enumerated type (see 3.5.2.2).  However, it is
         tagged with the enumerated type, so that enum compatibility checking
         can be done later. */
      enum_con_type = alloc_type((a_type_kind)tk_integer);
      enum_con_type->variant.integer.int_kind = (an_integer_kind)ik_int;
      enum_con_type->variant.integer.enum_type = FALSE;
      enum_con_type->variant.integer.enum_info.affiliated_type = enum_type;
      set_type_size(enum_con_type);
    }  /* if */
    min_max_set = FALSE;
    if (C_dialect == C_dialect_cplusplus && curr_token == tok_rbrace) {
      /* An enumerator constant list is optional in C++. */
    } else {
      add_stop_token(tok_rbrace);
      end_of_enum_con_list = NULL;
      /* Scan the list of enumerated constants. */
      do {
        add_stop_token(tok_comma);
        add_stop_token(tok_assign);
        if (curr_token != tok_identifier) {
          (void)required_token(tok_identifier, ec_exp_identifier);
          set_to_error_locator(locator);
        } else {
          locator = locator_for_curr_id;
          /* Advance past the identifier. */
          (void)get_token();
          /* Set the error position to the identifier position. */
          copy_source_position(locator.source_position, error_position);
        }  /* if */
        /* Note that the enumerator symbol is entered at little later, after
           the constant expression (if any) has been scanned.  (C standard,
           3.1.2.1 and 3.5.2.2) */
        remove_stop_token(tok_assign);
        err = FALSE;
        /* See if "= constant-expression" follows. */
        if (curr_token == tok_assign) {
          (void)get_token();
          /* Scan the constant expression. */
          scan_integral_constant_expression(&constant);
          if (is_error_constant(&constant)) {
            err = TRUE;
          } else if (constant.kind ==
                                (a_constant_repr_kind)ck_template_param) {
            /* We are doing a prototype instantiation and we have a case like
               this:
                 template <int N> class A { enum e { e1 = N }; };
               This is perfectly legal, but for convenience we represent the
               value as though this had been an error.  It will have no
               effect on other processing. */
            err = TRUE;
          } else {
            check_assertion(constant.kind == (a_constant_repr_kind)ck_integer);
            /* Check the value to see if it is out of range.  (3.5.2.2,
               constraints) */
            if (!in_range_for_integer_kind(&constant, &constant,
                                           (an_integer_kind)ik_int)) {
              a_boolean		conversion_allowed = TRUE;
              if (strict_ansi_mode) {
                conversion_allowed = strict_ansi_error_severity != es_error;
              }  /* if */
              if (conversion_allowed &&
                  f_skip_typerefs(constant.type)->size <= targ_sizeof_int) {
                /* In non-strict mode, allow unsigned constants that can be
                   coerced into an int. */
                type_change_constant(&constant,
                                     integer_type((an_integer_kind)ik_int),
                                     /*is_implicit_cast=*/TRUE,
                                     /*constant_context=*/TRUE,
                                     /*evaluated_context=*/TRUE,
                                     /*fold_constant_addr_exprs=*/TRUE,
                                     &did_not_fold,
                                     &error_position);
              }  /* if */
              if (strict_ansi_mode) {
                diagnostic(strict_ansi_error_severity,
                           ec_enum_value_out_of_int_range);
              }  /* if */
            }  /* if */
          }  /* if */
        } else {
          /* No explicit value. */
          if (end_of_enum_con_list == NULL) {
            /* This is the first enumerator.  Start with zero. */
            set_integer_constant(&constant, 0L, (an_integer_kind)ik_int);
          } else if (is_error_constant(&constant)) {
            /* There was a previous error. */
            err = TRUE;
          } else {
            /* Use a value one larger than the previous value. */
            /* Check the value to see if it is out of range.  (3.5.2.2,
               constraints) */
            if (is_max_value_for_integer_kind(&constant, 
                                              (an_integer_kind)ik_int)) {
              error(ec_enum_value_out_of_int_range);
              err = TRUE;
            } else {
              incr_integer_value(&constant.variant.integer_value);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Enter the enumeration constant identifier. */
        enum_sym = enter_local_symbol((a_symbol_kind)sk_constant, &locator,
                                      decl_scope_level,
                                      /*suppress_redecl_error=*/FALSE);
        *declares_something = TRUE;
        /* Track the highest and lowest values in the enumeration.  These are
           used to determine the appropriate representation type. */
        if (err) {
          /* There was some kind of error in the value for the enumerator. */
          set_error_constant(&constant);
        } else if (!min_max_set) {
          max_value = constant;
          min_value = constant;
          min_max_set = TRUE;
        } else if (cmp_integer_constants(&constant, &max_value) > 0) {
          max_value = constant;
        } else if (cmp_integer_constants(&constant, &min_value) < 0) {
          min_value = constant;
        }  /* if */
        /* Assign the value to the enumeration constant. */
        switch_to_file_scope_region(&region_to_switch_back_to);
        enum_con = alloc_constant((a_constant_repr_kind)ck_integer);
        /* Switch back from the file scope memory region to whatever region
           was current upon entry. */
        switch_back_to_original_region(region_to_switch_back_to);
        copy_constant(&constant, enum_con);
        set_source_corresp(&(enum_con->source_corresp), enum_sym);
        enum_sym->variant.constant = enum_con;
        enum_con->type = enum_con_type;
        /* Specify membership and access. */
        enum_con->source_corresp.class_of_which_a_member =
                enum_sym->class_of_which_a_member = class_of_which_a_member;
        enum_con->source_corresp.access = access;
        mark_defined(enum_sym, &locator.source_position);
        /* Add the enumeration constant to the list under the enumerated
           type. */
        if (end_of_enum_con_list == NULL) {
          enum_type->variant.integer.enum_info.constant_list = enum_con;
        } else {
          end_of_enum_con_list->next = enum_con;
        }  /* if */
        end_of_enum_con_list = enum_con;
        /* Keep looping while there are more enumeration constant
           identifiers. */
        copy_source_position(pos_curr_token, pos_comma);
        done = !loop_token(tok_comma);
        if (!done && curr_token == tok_rbrace) {
           /* Special trick: pcc allows an extra comma at the end of the 
              list.  In ANSI mode, we allow it as an extension, with
              a strict ANSI diagnostic (the gcc compiler source includes
              cases like this, and that source is part of the SPEC benchmark
              suite). */
          done = TRUE;
          if (C_dialect != C_dialect_pcc) {
            an_error_severity    severity;
            severity = strict_ansi_mode ? strict_ansi_error_severity :
                                          es_remark;
            pos_diagnostic(severity, ec_nonstd_extra_comma, &pos_comma);
          }  /* if */
        }  /* if */
        remove_stop_token(tok_comma);
      } while (!done);
      remove_stop_token(tok_rbrace);
    }  /* if */
    /* Check for and pass over the closing "}". */
    (void)required_token(tok_rbrace, ec_exp_rbrace);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the enum definition. */
    add_end_of_construct_source_sequence_entry((char *)enum_type,
                                               (a_byte_il_entry_kind)iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Determine the representation type for the enumeration.  In pcc mode,
       and when targ_enum_types_can_be_smaller_than_int is FALSE, it's always
       "int", and that's already set.  Otherwise, pick the first of "char",
       "signed char", "unsigned char", "short", "unsigned short", and
       "int" into which the enumeration values will fit.  Note that
       it is pointless to try "unsigned int", because all enumeration
       values must fall in the "int" range. */	
    if (C_dialect != C_dialect_pcc &&
        targ_enum_types_can_be_smaller_than_int) {
      if (!min_max_set || in_range_for_integer_kind(&min_value, &max_value,
                                                    plain_char_int_kind)) {
        /* "Plain" char. */
        enum_type->variant.integer.int_kind = plain_char_int_kind;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_signed_char)) {
        /* Signed char. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_signed_char;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                          (an_integer_kind)ik_unsigned_char)) {
        /* Unsigned char. */
        enum_type->variant.integer.int_kind =
                                             (an_integer_kind)ik_unsigned_char;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_short)) {
        /* Short. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_short;
      } else if ((targ_sizeof_short < targ_sizeof_int) &&
                  in_range_for_integer_kind(&min_value, &max_value,
                                         (an_integer_kind)ik_unsigned_short)) {
        /* Unsigned short.  Note that we can only get here if
           sizeof(short) < sizeof(int) on the target, for otherwise the
           previous test (for "short") is testing the same range as "int"
           (into which all enumeration values must fall), so "short" would
           have been selected.  This is important, as we would not want
           to pick "unsigned short" if the integral promotions would
           promote it to "unsigned int" rather than "int". */
        enum_type->variant.integer.int_kind =
                                            (an_integer_kind)ik_unsigned_short;
      } else {
        /* Representation type should be int, which is already set. */
      }  /* if */
    }  /* if */
    /* Set the type size (based on the integral type it is mapped onto). */
    set_type_size(enum_type);
    /* Add the type to the types list for the current scope.  This is done
       after the closing brace, if any, to get the IL types list in the
       right order.  Note that incomplete enums are not added
       to the type list, because the actual definition has not yet
       appeared.  See pop_scope; they get added at the end of the scope. */
    if (prototype_tag_resolution) {
      /* Tags that were declared in a prototype scope were added to the type
         list at the end of the prototype scope, so do not add them again. */
    } else if (scope_stack[effective_decl_level].kind ==
                                   (a_scope_kind)sck_template_declaration) {
      /* This is an error case -- an enum definition within a template
         parameter declaration.  Don't try to enter the type in the IL. */
    } else {
      add_to_types_list(enum_type, effective_decl_level);
    }  /* if */
  }  /* if */

  *type_ptr = enum_type;
  db_exit();
}  /* enum_specifier */


a_boolean is_constructor_decl(a_type_ptr    class_type)
/*
class_type is a pointer to the class that is currently being defined.  Return
TRUE and modify locator_for_curr_id appropriate if the current declaration
is a that of a constructor.
*/
{
  a_boolean      is_constructor = FALSE;
  a_symbol_ptr   tag_sym;
  a_token_cache  cache;
  a_symbol_ptr   curr_token_type_symbol;

  tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  if (locator_for_curr_id.symbol_header == tag_sym->header &&
      (!locator_for_curr_id.is_qualified_name ||
       locator_for_curr_id.qualifier_class_type == class_type)) {
    /* The name is the same as that of a class being defined.  This is treated
       as a constructor declaration if the next two tokens are a left paren
       and declaration start token.  Use token caching in the look-ahead,
       since the tokens will have to be rescanned no matter what. */
    /* Change "A::A" into "A" if we are processing inside the definition of
       class "A".  This is necessary for curr_token_type_symbol to handle
       this case correctly. */
    (void)simplify_curr_class_qualified_name();
    clear_token_cache(&cache, /*reusable=*/FALSE);
    /* Put the current token in the cache. */
    cache_curr_token(&cache);
    /* Advance to what may be the left paren. */
    if (get_token() == tok_lparen) {
      /* Cache the left parenthesis. */
      cache_curr_token(&cache);
      /* Advance past it.  If the next token is a right paren or
         the start of a parameter declaration, this must be a
         constructor. */
      (void)get_token();
      if (curr_token == tok_rparen || curr_token == tok_ellipsis ||
          is_decl_start(/*expr_context=*/FALSE,
                        /*real_declarator_allowed=*/TRUE)) {
        /* Constructor. */
        is_constructor = TRUE;
      }  /* if */
    }  /* if */
    /* Note that rescan_cached_tokens caches the current token as
       well as resetting the current token state to what it was
       before token caching was started.  So the current token
       should again be the name of the class being defined. */
    rescan_cached_tokens(&cache);
    if (is_constructor) {
      a_source_position  pos;

      /* Turn the current locator from a "specific symbol" locator
         into a constructor locator. */
      curr_token_type_symbol = curr_type_symbol(/*is_new_type_name=*/FALSE,
                                                /*in_prescan=*/FALSE);
      if (curr_token_type_symbol != tag_sym) {
        /* The symbol one gets by looking up the class name is not the same as
           the class symbol.  This might be okay, but it has to be checked
           carefully. */
        if (class_type ==
               locator_for_curr_id.specific_symbol->class_of_which_a_member) {
          if (locator_for_curr_id.specific_symbol->kind !=
                                    (a_symbol_kind)sk_projection) {
            /* This can only mean that another member has been
               declared with the class name.  Issue an error. */
            str_error(ec_id_already_declared,
                        locator_for_curr_id.symbol_header->identifier);
          }  /* if */
        } else if (curr_token_type_symbol != NULL) {
          if (curr_token_type_symbol->kind == (a_symbol_kind)sk_type &&
              f_skip_typerefs(curr_token_type_symbol->variant.type) ==
                                                                  class_type) {
            /* There is a typedef for the class type with the same name as
               the class.  It was found instead of the class on the lookup.
               That's okay. */
          } else {
            pos_sy2_error(ec_bad_constructor_name,
                          &locator_for_curr_id.source_position,
                          curr_token_type_symbol, tag_sym);
          }  /* if */
        }  /* if */
        /* Use the class symbol instead of whatever the lookup returned. */
        locator_for_curr_id.specific_symbol = tag_sym;
      }  /* if */
      pos = locator_for_curr_id.source_position;
      change_class_locator_into_constructor_locator(
                                         &locator_for_curr_id, &pos);
    }  /* if */
  }  /* if */
  return is_constructor;
}  /* is_constructor_decl */


#if MICROSOFT_KEYWORDS_ALLOWED
static void scan_microsoft_extended_decl_modifiers(a_decl_flag_set *flags,
						   a_boolean       *err)
/*
Scan the Microsoft __declspec specifier, which has the form

	__declspec ( extended-decl-modifier-seq)

	extended-decl-modifier-seq:
		extended-decl_modifier
		                      opt
		extended-decl-modifier-seq extended-decl-modifier

	extended-decl_modifier:
		thread
		naked
		dllimport
		dllexport

Update "flags" to reflect the modifiers that were found.  If an error
occurs (e.g., an invalid modifier), set err to TRUE.  err is unchanged if
there are no errors.

When this routine is called, the current token must be the __declspec
keyword.
*/
{
  check_assertion_str2(curr_token == tok_declspec,
                       "scan_microsoft_extended_decl_modifiers:",
                       "curr_token not tok_declspec");
  *flags = DSO_NO_OUTPUT_FLAGS;
  /* Bypass the __declspec token. */
  (void)get_token();
  if (required_token(tok_lparen, ec_exp_lparen)) {
    add_stop_token(tok_rparen);
    if (curr_token != tok_identifier) {
      syntax_error(ec_exp_identifier);
    } else {
      while (curr_token == tok_identifier) {
        char	*modifier;
        modifier = locator_for_curr_id.symbol_header->identifier;
        if (strcmp(modifier, "dllexport") == 0) {
          *flags |= DSO_DLLEXPORT;
        } else if (strcmp(modifier, "dllimport") == 0) {
          *flags |= DSO_DLLIMPORT;
        } else if (strcmp(modifier, "thread") == 0) {
          *flags |= DSO_THREAD;
        } else if (strcmp(modifier, "naked") == 0) {
          *flags |= DSO_NAKED;
        } else {
          str_error(ec_bad_declspec_modifier, modifier);
          *err = TRUE;
        }  /* if */
        (void)get_token();
      }  /* while */
    }  /* if */
    remove_stop_token(tok_rparen);
  }  /* if */
}  /* scan_microsoft_extended_decl_modifiers */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */


a_boolean decl_specifiers(a_decl_flag_set       input_flags,
                          a_decl_flag_set       *output_flags,
                          a_storage_class       *storage_class,
                          a_type_ptr            *type_ptr,
                          a_type_qualifier_set  *qualifiers)
/*
Scan a list of declaration specifiers.  Specifically, scan a
declaration-specifiers (3.5), a specifier_qualifier_list (3.5.2.1), or
a type_qualifier_list (3.5.4).  These are all made up of storage class
specifiers (3.5.1; allowed only if the input_flags bit
DSI_STORAGE_CLASS_SPECIFIER_ALLOWED is set), type specifiers (3.5.2;
allowed only if DSI_TYPE_SPECIFIER_ALLOWED is set), and type
qualifiers (3.5.3; always allowed).  The list must always include at
least one specifier.  The ANSI C syntax is as follows:

3.5    declaration-specifiers:
		storage-class-specifier declaration-specifiers
                                                              opt
		type-specifier declaration-specifiers
                                                     opt
		type-qualifier declaration-specifiers
                                                     opt
3.5.2.1
       specifier-qualifier-list:
		type-specifier specifier-qualifier-list
                                                       opt
		type-qualifier specifier-qualifier-list
						       opt
3.5.4  type-qualifier-list:
		type-qualifier
		type-qualifier-list type-qualifier
3.5.1  storage-class-specifier:
		typedef
		extern
		static
		auto
		register
3.5.2  type-specifier:
		void
		char
		short
		int
		long
		float
		double
		signed
		unsigned
		struct-or-union-specifier
		enum-specifier
		typedef-name
3.5.3  type-qualifier:
		const
		volatile

When Microsoft keywords are recognized, additional the syntax is
amended as follows (see comments below regarding recognition of the
modified syntax):

        storage-class-specifier:
		__declspec ( extended-decl-modifier-seq )
		__inline

	type-qualifier:
		__stdcall
		__fastcall
		__stdcall

	extended-decl-modifier-seq:
		extended-decl_modifier
		                      opt
		extended-decl-modifier-seq extended-decl-modifier

	extended-decl_modifier:
		thread
		naked
		dllimport
		dllexport

The DSI_IS_PARAMETER bit of input_flags is set if these specifiers are
part of the declaration of a parameter, and, for C++, the
DSI_VIRTUAL_OR_FRIEND_ALLOWED bit is set when a declaration appears within
a class declaration, to permit recognition of "virtual" and "friend"
keywords.

The syntax for the Microsoft extensions does not exactly match the
syntax described in the Microsoft documentation.  It does, however,
match the observed behavior of the Microsoft compiler.  The
additional type qualifiers are only recognized when the
DSI_MICROSOFT_QUALIFIERS_ALLOWED bit of input_flags is set.  The
additional storage class specifiers are recognized anywhere that
storage classes are normally allowed.

Returns *storage_class set to the storage class scanned (or
sc_unspecified if none was scanned), *type_ptr pointing to the type
scanned (including qualifiers, if any), and any of various flags in
*output_flags: DSO_HAS_EXPLICIT_TYPE_SPECIFIER is set if there was at
least one type specifier; DSO_DECLARES_SOMETHING is set if the
specifiers declare something (a tag or enumeration members);
DSO_JUST_VOID is set if the specifiers are simply the one keyword
"void"; and DSO_CONST_QUALIFIED and DSO_VOLATILE_QUALIFIED are set
if the associated qualifiers appear directly in the qualifiers list
(these flags are useful when this routine is called to scan only type
qualifiers, since in that case no type is built).  For C++ specifically,
DSO_VIRTUAL, DSO_INLINE, and DSO_FRIEND are set to report that a
"virtual", "inline", or "friend" keyword was scanned.  It also returns
a name linkage specifier to signal when, for instance, ``extern "C"''
was encountered (C++ only).  If DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER is
true, then DSO_DANGLING_TYPE_SPECIFIER may be set for cases that are
recognized as an omitted semi-colon or comma after a class or enum
definition (e.g., "typedef int T; struct A { ... } T x;").

Returns TRUE if there is an error in the specifiers.
*/
{
  int                        num_specifiers;
  a_symbol_ptr               curr_token_type_symbol;
  a_boolean                  err = FALSE;
  a_boolean                  bad_combination_of_type_specifiers = FALSE;
  a_source_position          start_pos;
  a_source_position          non_restrict_qualifier_pos;
  a_type_kind                kind;
  an_integer_kind            ikind;
  a_float_kind               fkind;
  a_type_ptr                 temp_type;
  a_boolean                  explicitly_signed;
  a_boolean                  is_parameter = (input_flags & DSI_IS_PARAMETER);
  a_boolean                  is_member_decl =
                                    (input_flags & DSI_IS_MEMBER_DECLARATION);
  a_boolean                  vacuous_decl_allowed;
  a_boolean                  declares_something = FALSE;
  a_boolean                  defines_something = FALSE;
  a_boolean                  void_first_specifier;
  a_boolean                  type_specifier_allowed;
  a_boolean                  dangling_type_specifier = FALSE;
  a_boolean                  is_elaborated_type_specifier = FALSE;
  a_boolean                  is_friend_decl = FALSE;
  a_boolean                  is_inline = FALSE;
  an_error_severity          es;
  an_identifier_options_set  options;

  enum {bt_none, bt_void, bt_char, bt_int,
        bt_float, bt_double, bt_typedef,
        bt_struct_union, bt_enum, bt_no_type, bt_error} basic_type = bt_none;
  enum {sign_none, sign_signed, sign_unsigned} sign       = sign_none;
  enum {size_none, size_short, size_long
#if LONG_LONG_ALLOWED
        , size_long_long
#endif /* LONG_LONG_ALLOWED */
                                        }      size       = size_none;

  db_enter(3, "decl_specifiers");
  explicitly_signed = FALSE;
  *output_flags = DSO_NO_OUTPUT_FLAGS;
  *storage_class = (a_storage_class)sc_unspecified;
  *type_ptr = NULL;
  *qualifiers = TQ_NONE;
  void_first_specifier = (curr_token == tok_void);
  type_specifier_allowed = (input_flags & DSI_TYPE_SPECIFIER_ALLOWED);
  vacuous_decl_allowed = (input_flags & DSI_VACUOUS_TAG_DECL_ALLOWED) != 0;
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  num_specifiers = 0;
  /* Loop for each declaration specifier. */
  for (;;) {
    switch (curr_token) {
      case tok_extern:
        if (C_dialect == C_dialect_cplusplus &&
            next_token() == tok_string_literal) {
          /* This is a C++ linkage specification, which is recognized and
             ignored in this context -- except for the error that's put out. */
          error(ec_linkage_specifier_not_allowed);
	  err = TRUE;
          /* Consume the string literal.  We don't bother validating it since
             an error has already been issued. */
          (void)get_token();
          break;
        }  /* if */
        /* Otherwise drop through for normal storage class processing. */
      case tok_typedef:
      case tok_static:
      case tok_auto:
      case tok_register:
        /* A storage class specifier (3.5.1). */
        if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED)) {
          error(ec_storage_class_not_allowed);
          err = TRUE;
        } else if (*storage_class != (a_storage_class)sc_unspecified) {
          /* More than  one storage class may not be specified. */
          error(ec_mult_storage_classes);
          err = TRUE;
        } else if (is_parameter && curr_token != tok_register &&
                   (C_dialect != C_dialect_cplusplus ||
                    curr_token != tok_auto)) {
          /* For parameters, the only allowed storage class specifiers are
             "register" and (in C++ only) "auto". */
          if (curr_token == tok_typedef &&
              (input_flags & DSI_IS_OLD_STYLE_PARAM_DECL)) {
            /* Error will be handled by caller. */
            *storage_class = (a_storage_class)sc_typedef;
          } else {
            error(ec_bad_param_storage_class);
            err = TRUE;
          }  /* if */
        } else if (is_inline && curr_token != tok_static) {
          error(ec_bad_storage_class_with_inline);
          err = TRUE;
        } else if (is_member_decl &&
                   curr_token != tok_static && curr_token != tok_typedef) {
          error(ec_bad_member_storage_class);
          err = TRUE;
        } else if (input_flags & DSI_IS_TEMPLATE_DECLARATION &&
                   curr_token != tok_extern && curr_token != tok_static) {
          error(ec_bad_storage_class_on_template_decl);
          err = TRUE;
        } else if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
                   (curr_token == tok_auto || curr_token == tok_register)) {
          error(ec_bad_file_scope_storage_class);
          err = TRUE;
        } else {
          if (C_dialect != C_dialect_pcc && !err) {
            if (num_specifiers > ((*output_flags & DSO_FRIEND) ? 1 : 0) +
                                 (is_inline ? 1 : 0)) {
              /* Issue a diagnostic if the storage class is not the first
                 specifier (except for "inline" or "friend"). */
              diagnostic(strict_ansi_mode ? es_warning : es_remark,
                         ec_storage_class_not_first);
            }  /* if */
          }  /* if */
          switch (curr_token) {
            case tok_typedef:
              *storage_class = (a_storage_class)sc_typedef;  break;
            case tok_extern:
              *storage_class = (a_storage_class)sc_extern;   break;
            case tok_static:
              *storage_class = (a_storage_class)sc_static;   break;
            case tok_auto:
              *storage_class = (a_storage_class)sc_auto;     break;
            case tok_register:
              *storage_class = (a_storage_class)sc_register; break;
#if CHECKING
            default:
              internal_error("decl_specifiers: bad storage class");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
#if MICROSOFT_KEYWORDS_ALLOWED
      case tok_microsoft_inline:
      case tok_declspec:
        {
          a_decl_flag_set	new_output_flags = DSO_NO_OUTPUT_FLAGS;
          a_source_position	specifier_start_pos = pos_curr_token;
          /* A Microsoft storage class modifier.  If this is a __declspec,
             scan the list of declaration modifiers. */
          switch (curr_token) {
            case tok_declspec:
              scan_microsoft_extended_decl_modifiers(&new_output_flags, &err);
              break;
            case tok_microsoft_inline:
	      new_output_flags = DSO_MICROSOFT_INLINE;
              break;
          }  /* switch */
          if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED)) {
            pos_error(ec_storage_class_not_allowed, &specifier_start_pos);
            err = TRUE;
          } else {
            /* There were no errors, update the output flags to reflect
               this specifier. */
            *output_flags |= new_output_flags;
            if (is_parameter) {
              /* For parameters, warn if a storage class modifier is used. */
              pos_warning(ec_bad_param_storage_class, &specifier_start_pos);
            }  /* if */
          }  /* if */
        }
        break;
        case tok_cdecl:
        case tok_fastcall:
        case tok_stdcall:
          /* Microsoft calling convention specifiers.  These are treated
             like type qualifiers. */
          {
            a_type_qualifier_set	new_qualifier = 0;
            if (!(input_flags & DSI_MICROSOFT_QUALIFIERS_ALLOWED)) {
              error(ec_calling_convention_not_allowed);
            } else {
              switch (curr_token) {
                case tok_cdecl:    new_qualifier = TQ_CDECL;    break;
                case tok_fastcall: new_qualifier = TQ_FASTCALL; break;
                case tok_stdcall:  new_qualifier = TQ_STDCALL;  break;
                default: unexpected_condition(); break;
              }  /* switch */
              if ((*qualifiers & TQ_CALLING_CONVENTION_QUALIFIERS) != 0 &&
                  (new_qualifier & *qualifiers) == 0) {
                /* The qualifier bit set already contains a calling
                   convention.  The same convention may be specified more
		   than once, but conflicting ones cannot be specified. */
                error(ec_conflicting_calling_conventions);
              } else {
                *qualifiers |= new_qualifier;
                non_restrict_qualifier_pos = pos_curr_token;
              }  /* if */
            }  /* if */
          }
          break;
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
      case tok_const:
        /* const type qualifier (3.5.3). */
        if (*qualifiers & TQ_CONST) {
          /* const may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_CONST;
        }  /* if */
        break;
      case tok_volatile:
        /* volatile type qualifier (3.5.3). */
        if (*qualifiers & TQ_VOLATILE) {
          /* volatile may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_VOLATILE;
        }  /* if */
        break;
#if RESTRICT_ALLOWED
      case tok_restrict:
        /* volatile type qualifier. */
        if (*qualifiers & TQ_RESTRICT) {
          /* Issue a diagnostic if restrict appears more than once. */
          es = (C_dialect == C_dialect_cplusplus) ? es_warning : es_error;
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          *qualifiers |= TQ_RESTRICT;
        }  /* if */
        break;
#endif /* RESTRICT_ALLOWED */
      case tok_friend:
	/* "friend" specifier is allowed only in a C++ class declaration.
	   This also excludes its appearing in a function parameter
	   specification. */
	if (is_parameter) {
	  /* "friend" may not appear in a function parameter specification. */
	  error(ec_bad_param_specifier);
	  err = TRUE;
	} else if (!is_member_decl) {
	  /* In fact, it may only appear in a C++ class (or struct or union)
	     declaration. */
	  error(ec_bad_specifier_outside_class_decl);
	  err = TRUE;
	} else if (*output_flags & DSO_FRIEND) {
	  /* Only one "friend" specifier at at time. */
	  error(ec_dupl_decl_specifier);
	  err = TRUE;
	} else {
	  *output_flags |= DSO_FRIEND;
          is_friend_decl = TRUE;
          if (num_specifiers == 0) {
            /* Check for a special case -- a friend declaration of the form
               "friend T;" which is taken to mean the same as "friend class T;"
               by cfront (even if T has not yet been defined).  Although there
               is no support for this syntax in the ARM, we accept it (except
               in strict ANSI mode) since it is widely used in older C++
               code.  */
            /* Advance to the token following "friend". */
            (void)get_token();
            if (!is_qualified_name_start()) {
              /* Can't be the start of an identifier -- back up to continue
                 processing. */
              unget_token();
              curr_token = tok_friend;
            } else {
              /* Advance over the identifier, which may actually be a
                 qualified name or even a template class. */
              a_boolean          lookup_err;
              a_symbol_ptr       tag_sym;
              a_source_position  ident_pos;

              ident_pos = pos_curr_token;
              tag_sym = coalesce_and_lookup_generalized_identifier(
                                      GID_NO_OPTIONS, ilm_normal, &lookup_err);
              /* Even if the lookup was successful, if the next token is not
                 a ";" this is not of the form "friend T;". */
              if (next_token() != tok_semicolon) {
                /* No semicolon -- back up. */
                clear_specific_symbol(locator_for_curr_id);
                unget_token();
                curr_token = tok_friend;
              } else if (any_cfront_mode() && tag_sym == NULL) {
                /* This friend declaration introduces a new type -- which is
                   okay in cfront compatibility mode.  Still, issue a remark
                   on use of a nonstandard feature. */
                vacuous_decl_allowed = FALSE;
                pos_st_remark(ec_nonstd_friend_decl, &ident_pos, "class");
                goto process_class_specifier;
              } else {
                a_type_ptr  tp = NULL;
                a_boolean   is_typedef = FALSE;

                if (tag_sym != NULL && is_class_symbol(tag_sym)) {
                  tp = type_symbol_type(tag_sym);
                  if (tp->kind == (a_type_kind)tk_typeref) {
                    is_typedef = TRUE;
                    if (is_qualified_type(tp)) {
                      /* This should be an error:
                           typedef const struct A TA;
                           class B { friend TA; };
                      */
                      tp = NULL;
                    } else {
                      tp = skip_typerefs(tp);
                    }  /* if */
                  }  /* if */
                }  /* if */
                if (tp == NULL) {
                  /* Lookup failed to find an unqualified class symbol.  Issue
                     an error. */
                  error(ec_bad_friend_decl);
                  err = TRUE;
                  *type_ptr = error_type();
                } else {
                  /* This declaration is of the form "friend T;" and T is
                     a previously declared class name.  Issue a diagnostic
                     for using a nonstandard feature. */
                  char               *class_key_string;

                  switch (tp->kind) {
                    case tk_class:   class_key_string = "class";   break;
                    case tk_struct:  class_key_string = "struct";  break;
                    case tk_union:   class_key_string = "union";   break;
#if CHECKING
                    default: internal_error("decl_specifiers: bad type kind");
#endif /* CHECKING */
                  }  /* switch */
                  /* Strict ANSI diagnostic in strict ANSI mode, remark
                     otherwise. */
                  pos_st_diagnostic(strict_ansi_mode ?
                                      strict_ansi_error_severity : es_remark,
                                    ec_nonstd_friend_decl, &ident_pos,
                                    class_key_string);
                  /* Note that scan_class_specifier is not called for this
                     case.  Therefore, mark the symbol declared. */
                  record_symbol_declaration(SRK_DECLARATION | SRK_FRIEND,
                                            tag_sym, &ident_pos,
                                            (a_source_sequence_entry_ptr)NULL);
                  if (!is_typedef) declares_something = TRUE;
                  *type_ptr = tp;
                  basic_type = bt_struct_union;
                  is_elaborated_type_specifier = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
	}  /* if */
	break;
      case tok_virtual:
	if (is_parameter) {
	  /* "virtual" may not appear in a function parameter specification. */
	  error(ec_bad_param_specifier);
	  err = TRUE;
	} else if (!is_member_decl) {
	  /* In fact, it may only appear in a C++ class (or struct or union)
	     declaration. */
	  error(ec_bad_specifier_outside_class_decl);
	  err = TRUE;
	} else if (*output_flags & DSO_VIRTUAL) {
	  /* Only one "virtual" specifier at at time. */
	  error(ec_dupl_decl_specifier);
	  err = TRUE;
	} else {
	  *output_flags |= DSO_VIRTUAL;
	}  /* if */
	break;
      case tok_inline:
	if (is_parameter) {
	  /* "inline" may not appear in a function parameter specification. */
	  error(ec_bad_param_specifier);
	  err = TRUE;
	} else if (!(input_flags & DSI_INLINE_ALLOWED) ||
                   (*storage_class != (a_storage_class)sc_unspecified &&
                    *storage_class != (a_storage_class)sc_static)) {
          /* "inline" allowed on certain function declarations only. */
          error(ec_inline_not_allowed);
          err = TRUE;
	} else if (is_inline) {
	  /* Only one "inline" specifier at at time. */
	  error(ec_dupl_decl_specifier);
	  err = TRUE;
	} else {
          is_inline = TRUE;
	  *output_flags |= DSO_INLINE;
	}  /* if */
	break;
      case tok_void:
        if (C_dialect == C_dialect_pcc) {
          /* To allow "typedef <something> void;" to be ignored in pcc mode,
             stop scanning on "void" when a basic type has already been scanned
             in a typedef. */
          if (basic_type != bt_none &&
              *storage_class == (a_storage_class)sc_typedef) goto exit_loop;
        }  /* if */
        /* Fall-through to next case. */
      case tok_char:
      case tok_int:
      case tok_float:
      case tok_double:
        /* A type specifier (3.5.2) that indicates a basic type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (basic_type != bt_none) {
          /* Basic type has already been specified in some way. */
          bad_combination_of_type_specifiers = TRUE;
          error(ec_bad_combination_of_type_specifiers);
        } else {
          switch (curr_token) {
            case tok_void:     basic_type = bt_void;   break;
            case tok_char:     basic_type = bt_char;   break;
            case tok_int:      basic_type = bt_int;    break;
            case tok_float:    basic_type = bt_float;  break;
            case tok_double:   basic_type = bt_double; break;
#if CHECKING
            default:
              internal_error("decl_specifiers: bad type specifier");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
      case tok_short:
      case tok_long:
        /* A type specifier (3.5.2) that modifies the length of a basic
           type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
#if LONG_LONG_ALLOWED
        } else if (size == size_long && curr_token == tok_long) {
          /* long long.  This is an extension. */
          size = size_long_long;
          if (strict_ansi_mode) {
            diagnostic(strict_ansi_error_severity, ec_nonstd_long_long);
          }  /* if */
#endif /* LONG_LONG_ALLOWED */
        } else if (size != size_none) {
          /* Size has already been specified in some way. */
          if ((size == size_short) == (curr_token == tok_short)) {
            /* "short short" or "long long". */
            if (any_cfront_mode() && curr_token == tok_short) {
              /* Cfront allows the redundancy.  It issues a warning on
                 "long long", so we do too. */
            } else {
              /* Since the redundancy is harmless, just issue a warning
                 (except in -A mode). */
              diagnostic((strict_ansi_mode ?
                           strict_ansi_error_severity : es_warning),
                         ec_dupl_decl_specifier);
            }  /* if */
          } else {
            /* Mixing size specifications. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
          }  /* if */
        } else {
          /* First specification of size. */
          if (curr_token == tok_short) {
            size = size_short;
          } else {
            size = size_long;
          }  /* if */
        }  /* if */
        break;
      case tok_signed:
      case tok_unsigned:
        /* A type specifier (3.5.2) that modifies the signedness of a
           basic type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (sign != sign_none) {
          /* Sign has already been specified in some way. */
          if ((sign == sign_signed) == (curr_token == tok_signed)) {
            /* Either "signed signed" or "unsigned unsigned". */
            if (any_cfront_mode()) {
              /* Cfront allows the redundancy. */
            } else {
              /* Since the redundancy is harmless, just issue a warning
                 (except in -A mode). */
              diagnostic((strict_ansi_mode ?
                           strict_ansi_error_severity : es_warning),
                         ec_dupl_decl_specifier);
            }  /* if */
          } else {
            /* Mixing signs. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
          }  /* if */
        } else {
          /* First specification of sign. */
          if (curr_token == tok_signed) {
            sign = sign_signed;
            explicitly_signed = TRUE;
          } else {
            sign = sign_unsigned;
          }  /* if */
        }  /* if */
        break;
      case tok_class:
      case tok_struct:
      case tok_union:
process_class_specifier:
        /* A struct or union specifier (3.5.2.1). */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            if (num_specifiers > 0) vacuous_decl_allowed = FALSE;
            if (!class_specifier(vacuous_decl_allowed, is_friend_decl,
                                 (input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                 type_ptr, &declares_something,
                                 &defines_something)) {
              err = TRUE;
            }  /* if */
            basic_type = bt_struct_union;
            is_elaborated_type_specifier = TRUE;
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
            /* Scan the specifier anyway, but throw it away. */
            (void)class_specifier(/*vacuous_decl_allowed=*/FALSE,
                                  /*is_friend_decl=*/FALSE,
                                  (input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                  &dummy_type, &dummy_flag, &dummy_flag);
          }  /* if */
          goto no_get_token;
        }  /* if */
        break;
      case tok_enum:
        /* An enumeration specifier (3.5.2.2). */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            if (num_specifiers > 0 || strict_ansi_mode) {
              vacuous_decl_allowed = FALSE;
            }  /* if */
            enum_specifier(vacuous_decl_allowed, type_ptr,
                           &declares_something, &defines_something);
            basic_type = bt_enum;
            is_elaborated_type_specifier = TRUE;
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
            /* Scan the specifier anyway, but throw it away. */
            enum_specifier(/*vacuous_decl_allowed=*/FALSE,
                           &dummy_type, &dummy_flag, &dummy_flag);
          }  /* if */
          goto no_get_token;
        }  /* if */
        break;
      case tok_overload:
        /* Special case -- the "overload" keyword (which shows up in
           cfront compatibility mode only).  Ignore it and advance to the
           next token. */
        diagnostic(anachronism_error_severity, ec_overload_anachronism);
        break;
      case QUALIFIED_NAME_START_CASE:  /* Identifier or "::". */
        /* Identifier. */
        /* In case the identifier has not yet been coalesced, do it now. */
        options = GID_NO_OPTIONS;
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          options |= GID_IS_NEW_TYPE_NAME;
        }  /* if */
        if (!is_generalized_identifier_start(options)) {
          /* This could result from "::" followed by something strange. */
          goto something_unexpected;
        }  /* if */
        if (C_dialect == C_dialect_cplusplus) {
          /* Check for a constructor declaration.  The following conditions
             must be satisfied:  (1) we are inside a class definition;
             (2) the current token is the name of the class being defined
             (note that typedef names are not allowed); (3) the declaration
             has no other specifiers besides "inline" (which is legal) and
             "virtual" or "static" (which are not); (4) the next token is a
             left parenthesis; (5) the token following the left paren is a
             right paren or the start of a formal parameter declaration. */
          if (is_member_decl &&
              num_specifiers == (((*output_flags & DSO_VIRTUAL) ? 1 : 0) +
                                 ((*storage_class ==
                                     (a_storage_class)sc_static) ? 1 : 0) +
                                 (is_inline ? 1 : 0))) {
            if (scope_stack[decl_scope_level].kind !=
                                     (a_scope_kind)sck_class_struct_union) {
              /* Error case of some sort. */
            } else {
              a_type_ptr class_type = scope_stack[decl_scope_level].assoc_type;
              check_assertion(class_type != NULL &&
                              is_class_struct_union_type(class_type)); 
              if (is_constructor_decl(class_type)) {
                basic_type = bt_no_type;
                *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
                /* Note that with a branch to exit_loop the get_token call
                   is bypassed.  This means curr_token will still represent
                   the constructor name (= class name) upon return to the
                   caller. */
                goto exit_loop;
              }  /* if */              
            }  /* if */              
          }  /* if */
        }  /* if */
        /* The appearance of an identifier may mean that the specifiers
           are complete (the identifier is a declarator) or it may be another
           specifier.  First we look for conditions that will cause us to
           exit the loop -- generally because the identifier is clearly not
           a specifier. */
        /* To be more specific: in ANSI C, an identifier that appears to be
           a typedef name is not recognized as such if the specifiers list
           already includes a basic type or sign.  This is so that the
           following (from the standard, 3.5.6) will work:
               typedef signed int t;
               main () {
                 long t;    <-- This declares a new identifier t.
               }
           K&R (first edition, Appendix A, section 11.1) also includes the
           following:
               typedef float distance;
               {
                 auto int distance;
               }
           but pcc (at least on a Sun 3) doesn't accept this.  It always
           seems to scan a typedef name as a typedef name.  To conform to
           K&R, we consider an identifier to be a typedef when there is
           just a sign or size (since these are "adjectives" to pcc), but
           not when there is a type specifier. */
        if (basic_type != bt_none) {
          /* There's already a basic type, so the identifier should be
             processed as a declarator.  If it happens to be a type name,
             it is better to have a invalid-redeclaration error later than
             a bad-combination-of-types error here.  In addition, the
             following is permitted in C++:
                 struct S {...};
                 int S;
             since tag names are not in the same name space with other
             objects. */
          goto exit_loop;
        }  /* if */
        if (sign != sign_none || size != size_none) {
          /* There is an indication of sign and/or size (but no indication
             of a basic type).  In ANSI C and C++, assume we're dealing with
             a declarator.  In pcc mode, adjectival modification of a typedef
             is allowed in certain circumstances, so keep going till we know
             if the identifier is a typedef. */
          if (C_dialect != C_dialect_pcc) goto exit_loop;
        }  /* if */
        /* Look up the identifier as a type symbol, if it has not already been
           looked up. */
        curr_token_type_symbol = curr_type_symbol(input_flags &
                                                        DSI_IS_NEW_TYPE_NAME,
                                                  /*in_prescan=*/FALSE);
        if (curr_token_type_symbol != NULL) {
          if (sign != sign_none || size != size_none) {
            /* We are in pcc mode, in which adjectival modification of a
               typedef is allowed -- but with restrictions.  For integral
               types, any size/sign is allowed. For floating types, only
               "long" is allowed. */
            a_type_ptr  tp = type_symbol_type(curr_token_type_symbol);
            if (is_integral_type(tp) ||
                (is_floating_type(tp) &&
                 sign == sign_none && size == size_long)) {
              /* Adjectives okay. */
            } else {
              goto exit_loop;
            }  /* if */
          }  /* if */
#if 0
/* It is unlikely that the following special case (namely the declaration of
   function A with implicit return type) should be supported.  Cfront does,
   but it may be a bug, so (for now, at least) we suppress support even in
   cfront compatibility mode. */
          /* Special case.  Consider the following:
               struct A { };
               A (i);       // declares var i of type A (C++ only)
               A (int i);   // declares func A taking int param, returning int
             The third declaration is a valid function declaration in C and
             is allowed in C++ as well.  We need to look ahead to disambiguate
             this case. */
          if (!locator_for_curr_id.is_qualified_name &&
              basic_type == bt_none && next_token() == tok_lparen) {
            a_type_ptr  tp = type_symbol_type(curr_token_type_symbol);
            if (is_immediate_class_type(tp) && !is_template_class_type(tp)) {
              a_token_cache  cache;
              a_boolean      is_decl;

              clear_token_cache(&cache, /*reusable=*/FALSE);
              /* Put the current token in the cache. */
              cache_curr_token(&cache);
              /* Advance to the left paren, cache it, and move past it. */
              (void)get_token();
              cache_curr_token(&cache);
              (void)get_token();
              is_decl = is_decl_start(/*expr_context=*/FALSE,
                                      /*real_declarator_allowed=*/TRUE);
              rescan_cached_tokens(&cache);
              if (is_decl) goto exit_loop;
            }  /* if */
          }  /* if */
#endif /* if 0 */
          /* The identifier is a type name and should be treated as a
             type specifier. */
          if (locator_for_curr_id.is_semivisible_nested_type) {
            /* The symbol in the locator is a nested class that is not
               visible according to the ARM lookup rules but is returned
               in support of the nested class anachronism (ARM 18.3.5).
               Issue an anachronism diagnostic. */
            sym_diagnostic(anachronism_error_severity, 
                           ec_nested_class_anachronism,
                           locator_for_curr_id.specific_symbol);
          }  /* if */
          /* Do ambiguity and access control checking. */
          check_ambiguity_and_verify_access(&locator_for_curr_id);
          /* If the symbol is a projection symbol, get the fundamental
             symbol. */
          reduce_projection_symbol_to_fundamental_symbol(
                                                      curr_token_type_symbol);
          mark_referenced(curr_token_type_symbol,
                          &locator_for_curr_id.source_position);
          if (!type_specifier_allowed) {
            error(ec_type_specifier_not_allowed);
            err = TRUE;
          } else {
            /* Save the type. */
            basic_type = bt_typedef;
            *type_ptr = type_symbol_type(curr_token_type_symbol);
          }  /* if */
          break;
        }  /* if */
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          /* This is a an identifier in a "new" expression so it was
             probably intended to be a type name.  Issue an error and
             pretend that's what it is. */
          error(ec_exp_type_specifier);
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          break;
        }  /* if */
        if (locator_for_curr_id.is_operator_name ||
            locator_for_curr_id.is_conversion_name) {
          /* This identifier represents something like "A::operator+" or
             "A::operator int"." */
          goto operator_or_conversion_name;
        }  /* if */
        if (locator_for_curr_id.is_destructor_name && !is_friend_decl &&
	    (simplify_curr_class_qualified_name() ||
	     !locator_for_curr_id.is_qualified_name)) {
          /* This identifier represents something like "A::~A".  This
             case is handled one way if we are currently processing the
             definition of class A and another way if we are not.  If we
	     are processing the definition of class A, "A::~A" will have
	     already been coalesced and the qualifier information
             will have been discarded by simplify_curr_class_qualified_name.
             This test identifies this case and transfers control to the code
             that would have been executed if the program simply said "~A"
             instead of "A::~A".  If we are not processing the definition of
             class A, we simply fall through this test. */
          goto destructor_name;
        }  /* if */
        if (is_friend_decl) {
          if (curr_token_type_symbol == NULL) {
            /* This is a declaration of the form "... friend X ... ",
               where X is already known to be neither the name of a class
               in a friend class declaration nor the name of a type for a
               function return type.  That means it is probably the name of
               a function with an implicit return type.  Clear the specific
               symbol pointer in the locator to deal with this sort of case:
                 class A {
                   int x;         // Declare A::x
                   friend x();    // Cause injection of ::x at file scope
                 };
            */
            clear_specific_symbol(locator_for_curr_id);
          }  /* if */
          goto exit_loop;
        }  /* if */
        if (is_error_locator(locator_for_curr_id) &&
            locator_for_curr_id.is_template_id) {
          /* An error was detected in scanning a class template id.  Since
             a template id can only be a type, treat it as an error type. */
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          break;
        } else if (num_specifiers == 0 &&
                   !(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
          /* If this is the first specifier, and this identifier is undefined,
             assume that we are dealing with a name that was supposed to be
             declared as a typedef.  Note that we do not get here on
             declarations, so this bit of error recovery tweaking applies
             only to things like prototyped parameter declarations and
             members of structs/unions. */
          if (!is_error_locator(locator_for_curr_id) &&
              (symbol_list_from_locator(locator_for_curr_id)) == NULL) {
            /* The identifier is undefined.  Assume it's an undefined
               typedef name. */
            str_error(ec_undefined_identifier,
                      locator_for_curr_id.symbol_header->identifier);
            err = TRUE;
            basic_type = bt_typedef;
            *type_ptr = error_type();
            reference_to_invalid_name(&locator_for_curr_id);
            break;
          }  /* if */
        } else if (num_specifiers == 0 &&
                   is_error_locator(locator_for_curr_id) &&
                   locator_for_curr_id.is_global_qualified_name) {
          /* An error was detected in scanning a qualified name that started
             with "::".  Since declarators may not start with "::" we assume
             this to be like an unidentified type name. */
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          break;
        } else {
          /* Two special cases. */
          if (type_specifier_allowed && basic_type == bt_none &&
              sign == sign_none && size == size_none &&
              (locator_for_curr_id.specific_symbol == NULL ||
               locator_for_curr_id.specific_symbol->kind ==
                                   (a_symbol_kind)sk_undefined)) {
            /* No type name has been seen and the current token may be
               undeclared identifier or an invalid qualified name.  If the
               next token is the start of a declarator, we may plausibly
               have something like "extern x y" or "static x *z", where x
               can be interpreted as a type name. */
            a_token_cache  cache;
            a_boolean      is_declarator;

            clear_token_cache(&cache, /*reusable=*/FALSE);
            /* Put the current token in the cache. */
            cache_curr_token(&cache);
            /* Advance to next token. */
            (void)get_token();
            /* Check next token for start of a declarator.  "(" may be part
               of a function declaration, so it doesn't count. */
            is_declarator = is_declarator_start() && curr_token != tok_lparen;
            /* Restore the token state. */
            rescan_cached_tokens(&cache);
            if (is_declarator) {
              /* Assume that the undefined identifier that is apparently
                 followed by a declarator was intended to be a type name. */
              err = TRUE;
              if (locator_for_curr_id.specific_symbol == NULL) {
                /* This is not an invalid qualified name, but it may be that
                   it is a valid identifier, just not a type name.  Treat
                   undefined-names and defined-but-not-a-type-names with
                   different diagnostics. */
                if (normal_id_lookup(&locator_for_curr_id,
                                     IDL_NO_OPTIONS) == NULL) {
                  str_error(ec_undefined_identifier,
                            locator_for_curr_id.symbol_header->identifier);
                } else {
                  error(ec_exp_type_specifier);
                }  /* if */
              }  /* if */
              basic_type = bt_typedef;
              *type_ptr = error_type();
              reference_to_invalid_name(&locator_for_curr_id);
              break;
            }  /* if */
          }  /* if */
          if (num_specifiers ==
                     ((is_friend_decl ? 1 : 0) + (is_inline ? 1 : 0))) {
            a_symbol_ptr  sym = locator_for_curr_id.specific_symbol;

            /* A function declaration without declaration specifiers is
               permitted. */
            if (num_specifiers == 0) {
              *output_flags |= DSO_NO_DECL_SPECIFIERS;
            }  /* if */
            /* Set the type appropriately if this the name of a constructor
               member function. */
            if (sym != NULL) {
              if (is_constructor_symbol(sym)) {
                *output_flags |= DSO_CONSTRUCTOR;
                basic_type = bt_no_type;
              } else if (is_destructor_symbol(sym)) {
                *output_flags |= DSO_DESTRUCTOR;
                basic_type = bt_no_type;
              }  /* if */
            }  /* if */
            goto exit_loop;
          }  /* if */
        }  /* if */
        /* For non-typedef identifiers, branch to the default case. */
        goto something_unexpected;
      case tok_operator:
        /* Coalesce the operator name. */
        (void)is_generalized_identifier_start(GID_NO_OPTIONS);
operator_or_conversion_name:
        if (locator_for_curr_id.is_conversion_name) {
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* A type has already been specified, but the error is issued
               later in order to unify error processing for both of the
               following (only the first of which is detected here):
                 class A {
                   char operator char();       // Error detectable here
                   char* operator char*();     // Error not detectable here
                 };
               Errors for both are issued in declarator. */
          }  /* if */
        } else if (locator_for_curr_id.is_operator_name &&
                   is_member_decl && !is_friend_decl) {
          an_opname_kind  opname = locator_for_curr_id.variant.opname;
          if (opname == (an_opname_kind)onk_new ||
              opname == (an_opname_kind)onk_delete) {
            /* We are inside a class definition, so an operator new or
               operator delete function is automatically treated as a
               static member function, even if "static" is not explicitly
               specified. */
            if (*storage_class == (a_storage_class)sc_unspecified) {
              *storage_class = (a_storage_class)sc_static;
            }  /* if */
          }  /* if */
        }  /* if */
        goto exit_loop;
      case tok_template:
        /* "template" cannot appear in decl-specifiers. */
        set_to_error_locator(locator_for_curr_id);
        locator_for_curr_id.source_position = pos_curr_token;
        pos_error(ec_template_not_allowed, &pos_curr_token);
        if (next_token() == tok_lt) {
          flush_tokens();
        } else {
          (void)get_token();
        }  /* if */
        err = TRUE;
        if (basic_type == bt_typedef) {
          *type_ptr = error_type();
        } else {
          basic_type = bt_error;
        }  /* if */
        goto no_get_token;
      case tok_compl:
destructor_name:
        if (is_member_decl) {
          if (num_specifiers == 0) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          *output_flags |= DSO_DESTRUCTOR;
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* It is an error to specify the type on a destructor, but it
               will be reported later. */
          }  /* if */
          goto exit_loop;
        }  /* if */
        /* If destructors aren't expected, fall through into the default
           case. */
      default:
        /* Something unexpected.  After the first time, we can just exit
           the loop (we've taken all we're supposed to).  The first time,
           this is an error. */
something_unexpected:
        if (num_specifiers == 0) {
          if (!(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
            syntax_error(ec_exp_type_specifier);
            err = TRUE;
            basic_type = bt_error;
          } else {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
        }  /* if */
        goto exit_loop;
    }  /* switch */
    (void)get_token();
no_get_token:
    num_specifiers++;
    /* Check for special conditions that will cause this loop to terminate. */
    if (input_flags & DSI_COLLECT_TYPE_QUALIFIERS) {
      /* We are only interested in scanning type qualifiers (e.g., in a
         pointer declarator). */
      if (!is_type_qualifier() || is_microsoft_type_qualifier()) {
        goto exit_loop;
      }  /* if */
    } else if (defines_something &&
               input_flags & DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER) {
      /* The basic type is a class, struct, union, or enum that actually
         defines a type.  We are especially interested in cases like this:
           class A {...}          <== Note the missing semicolon.
           class B {...};
         where we'd rather report a missing semicolon than a conflict of
         types.  This is referred to as a "dangling type specifier".  Look
         for a type-specifier keyword or (if this is not a typedef declaration)
         a type name.  E.g.,
           class A {...} int...                 <== Dangling type specifier
         Note that this logic works for both C++ and standard C. */
      if (is_type_specifier()) {
        /* The current token is a type keyword; treat it as the start
           of a new declaration.  The error on missing punctuation will be
           handled by the caller. */
        dangling_type_specifier = TRUE;
        goto exit_loop;
      }  /* if */
    }  /* if */
  }  /* for */

exit_loop:
  if (void_first_specifier && num_specifiers == 1) {
    /* Set the output_flags bit to indicate that the sequence of specifiers
       had just one specifier, and it was "void". */
    *output_flags |= DSO_JUST_VOID;
  } else if (is_elaborated_type_specifier) {
    if (!err && *qualifiers == TQ_NONE && !defines_something && 
        !(*output_flags & DSO_VIRTUAL) && !(*output_flags & DSO_INLINE) &&
        *storage_class == (a_storage_class)sc_unspecified) {
      *output_flags |= DSO_ELABORATED_TYPE_SPECIFIER;
    }  /* if */
  }  /* if */

  /* Return the position of the first specifier as the position of the
     overall list of specifiers for error purposes. */
  copy_source_position(start_pos, error_position);

  if (type_specifier_allowed) {
    if ((basic_type != bt_none && basic_type != bt_no_type) ||
        sign != sign_none || size != size_none) {
      /* Set the flag indicating an explicit type specifier.  Adjectival
         type modifiers (size, sign) are okay, since this is for detecting
         implicit void function return types in pre-ANSI C. */
      *output_flags |= DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
      /* Note that is certain cases a diagnostic is issued to warn the
         user about a missing type specifier.  This is handled by the
         caller. */
    }  /* if */
    if (dangling_type_specifier) {
      /* Set the bit to mark a malformed type specification, typically
         caused by a missing semicolon following an class, struct, union,
	 or enum declaration.  Error reporting is left to the caller in
         such cases. */
      *output_flags |= DSO_DANGLING_TYPE_SPECIFIER;
    }  /* if */
    /* Combine the type specifiers into a type. */
    /* If there was no basic type, use int or (for constructors and
       destructors) tk_unknown. */
    if (basic_type == bt_none) {
      basic_type = bt_int;
    } else if (basic_type == bt_no_type) {
      *type_ptr = unknown_type();
    }  /* if */
    if (C_dialect == C_dialect_pcc && basic_type == bt_typedef &&
        (sign != sign_none || size != size_none)) {
      /* pcc allows use of unsigned, long, and short as adjectives modifying
         a typedef type.  Turn the typedef into a matching basic type,
         for the cases for which it makes sense.  For the others, an error
         will be detected below. */
      temp_type = skip_typerefs(*type_ptr);
      if (temp_type->kind == (a_type_kind)tk_integer) {
        if (temp_type->variant.integer.enum_type) {
          /* Don't allow adjectives on enum integers. */
        } else {
          /* Adjectives (size and sign) are only allowed where they
             fill in empty holes -- unspecified attributes -- in the
             following table:

                                     sign      size      base type
               ik_signed_char                  -fixed-   char
               ik_unsigned_char      see note  -fixed-   char
               ik_short                        short     int
               ik_unsigned_short     unsigned  short     int
               ik_int                                    int
               ik_unsigned_int       unsigned            int
               ik_long                         long      int
               ik_unsigned_long      unsigned  long      int
               ik_long_long                    long long int
               ik_unsigned_long_long unsigned  long long int

             In pcc mode, the "signed" keyword does not exist, so something
             that is signed really has unspecified sign.  Note that ik_char
             is not used in pcc mode, so ik_unsigned_char has to be viewed
             as not specifying a sign if it is plain_char_int_kind.
             ik_signed_char always implies an unspecified sign.  A size may
             not be specified for those ("short char" and "long char" don't
             make sense). */
          ikind = temp_type->variant.integer.int_kind;
          switch (ikind) {
            case ik_unsigned_char:
              if (plain_char_int_kind != ikind && sign != sign_none) break;
              /* Fall into signed char case. */
            case ik_signed_char:
              if (size != size_none) break;
              basic_type = bt_char;
              break;
            case ik_short:
              if (size != size_none) break;
              basic_type = bt_int;
              size = size_short;
              break;
            case ik_unsigned_short:
              /* No holes to fill in. */
              break;
            case ik_unsigned_int:
              if (sign != sign_none) break;
              sign = sign_unsigned;
              /* Fall into signed int case. */
            case ik_int:
              basic_type = bt_int;
              break;
            case ik_long:
              if (size != size_none) break;
              basic_type = bt_int;
              size = size_long;
              break;
            case ik_unsigned_long:
              /* No holes to fill in. */
              break;
#if LONG_LONG_ALLOWED
            case ik_long_long:
              if (size != size_none) break;
              basic_type = bt_int;
              size = size_long_long;
              break;
            case ik_unsigned_long_long:
              /* No holes to fill in. */
              break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
            default:
              internal_error("decl_specifiers: bad typedef int kind");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
      } else if (temp_type->kind == (a_type_kind)tk_float) {
        fkind = temp_type->variant.float_kind;
        if (fkind == (a_float_kind)fk_float) {
          basic_type = bt_float;
        } else if (fkind == (a_float_kind)fk_double) {
          basic_type = bt_double;
        }  /* if */
      }  /* if */
      if (basic_type != bt_typedef) *type_ptr = NULL;
    }  /* if */
    /* Now check for the various legal combinations of specifiers.
       See 3.5.2 for list. */
    if (bad_combination_of_type_specifiers) {
      /* Don't look at basic_type etc. if there was an error. */
    } if (basic_type == bt_void && sign == sign_none && size == size_none) {
      /* void. */
      kind = (a_type_kind)tk_void;
    } else if (basic_type == bt_char && size == size_none) {
      kind = (a_type_kind)tk_integer;
      if (sign == sign_none) {
        /* "plain" char. */
        ikind = plain_char_int_kind;
      } else if (sign == sign_signed) {
        /* signed char. */
        ikind = (an_integer_kind)ik_signed_char;
      } else {
        /* unsigned char. */
        ikind = (an_integer_kind)ik_unsigned_char;
      }  /* if */
    } else if (basic_type == bt_int && size == size_short) {
      kind = (a_type_kind)tk_integer;
      if (sign != sign_unsigned) {
        /* short, signed short, short int, signed short int. */
        ikind = (an_integer_kind)ik_short;
      } else {
        /* unsigned short, unsigned short int. */
        ikind = (an_integer_kind)ik_unsigned_short;
      }  /* if */
    } else if (basic_type == bt_int && size == size_none) {
      kind = (a_type_kind)tk_integer;
      if (sign != sign_unsigned) {
        /* int, signed, signed int, or no type specifiers. */
        ikind = (an_integer_kind)ik_int;
      } else {
        /* unsigned, unsigned int. */
        ikind = (an_integer_kind)ik_unsigned_int;
      }  /* if */
    } else if (basic_type == bt_int && size == size_long) {
      kind = (a_type_kind)tk_integer;
      if (sign != sign_unsigned) {
        /* long, signed long, long int, signed long int. */
        ikind = (an_integer_kind)ik_long;
      } else {
        /* unsigned long, unsigned long int. */
        ikind = (an_integer_kind)ik_unsigned_long;
      }  /* if */
#if LONG_LONG_ALLOWED
    } else if (basic_type == bt_int && size == size_long_long) {
      kind = (a_type_kind)tk_integer;
      if (sign != sign_unsigned) {
        /* long long, signed long long, long long int, signed long long int. */
        ikind = (an_integer_kind)ik_long_long;
      } else {
        /* unsigned long long, unsigned long long int. */
        ikind = (an_integer_kind)ik_unsigned_long_long;
      }  /* if */
#endif /* LONG_LONG_ALLOWED */
    } else if (basic_type == bt_float && sign == sign_none &&
               size == size_none) {
      /* float. */
      kind = (a_type_kind)tk_float;
      fkind = (a_float_kind)fk_float;
    } else if (basic_type == bt_double && sign == sign_none &&
               size == size_none) {
      /* double. */
      kind = (a_type_kind)tk_float;
      fkind = (a_float_kind)fk_double;
    } else if (basic_type == bt_float && sign == sign_none &&
               size == size_long) {
      /* long float, which is double in pcc.  Allowed as an extension
         in ANSI mode. */
      kind = (a_type_kind)tk_float;
      fkind = (a_float_kind)fk_double;
      if (strict_ansi_mode) {
        pos_diagnostic(strict_ansi_error_severity,
                       ec_bad_combination_of_type_specifiers, &start_pos);
      }  /* if */
    } else if (basic_type == bt_double && sign == sign_none &&
               size == size_long) {
      /* long double. */
      kind = (a_type_kind)tk_float;
      fkind = (a_float_kind)fk_long_double;
    } else if ((basic_type == bt_struct_union || basic_type == bt_enum) &&
               sign == sign_none && size == size_none) {
      /* struct, union, or enum. */
    } else if (basic_type == bt_typedef && sign == sign_none &&
               size == size_none) {
      /* typedef. */
    } else if (basic_type == bt_no_type) {
      /* No specifiers type declared (constructor, destructor, or conversion
         operator). */
    } else if (basic_type == bt_error) {
      /* Error, already diagnosed. */
      kind = (a_type_kind)tk_error;
      err = TRUE;
      *type_ptr = NULL;
    } else {
      /* Error, not an acceptable combination. */
      bad_combination_of_type_specifiers = TRUE;
      pos_error(ec_bad_combination_of_type_specifiers, &start_pos);
    }  /* if */
    if (bad_combination_of_type_specifiers) {
      /* Bad combination of type specifiers.  Use an error type. */
      err = TRUE;
      *type_ptr = NULL;
      kind = (a_type_kind)tk_error;
    }  /* if */
    /* Find a type entry if one is wanted.  It should be possible to use
       a standard one, since non-standard cases should already have been
       allocated. */
    if (*type_ptr == NULL) {
      if (kind == (a_type_kind)tk_void) {
        *type_ptr = void_type();
      } else if (kind == (a_type_kind)tk_error) {
        *type_ptr = error_type();
      } else if (kind == (a_type_kind)tk_integer) {
	if (explicitly_signed && ikind != (an_integer_kind)ik_signed_char) {
          /* For an explicitly "signed" int, use a different type entry.
             Plain "int" and "signed int" have to be kept separate because
             they may mean different things as bit-field types.  The same
             applies to explicitly signed short, long, and long long. */
	  *type_ptr = signed_integer_type((an_integer_kind)ikind);
	} else {
          *type_ptr = integer_type((an_integer_kind)ikind);
	}  /* if */
      } else if (kind == (a_type_kind)tk_float) {
        *type_ptr = float_type((a_float_kind)fkind);
#if CHECKING
      } else {
        internal_error(
               "decl_specifiers: need to allocate a non-standard type");
#endif /* CHECKING */
      }  /* if */
    }  /* if */
    /* Add any type qualifiers (const or volatile) to the type. */
    if (*qualifiers != TQ_NONE) {
      if ((*type_ptr)->kind == (a_type_kind)tk_typeref) {
        if (C_dialect == C_dialect_cplusplus) {
          /* In C++ adding a qualifier to a typedef name that is already
             identically qualified is okay, so don't even bother checking for
             an error.  Note that make_qualified_type will not actually add
             superfluous qualifiers. */
          /* However, adding a qualifier to a typedef for a reference type
             is not allowed.  More precisely, the qualifier is ignored.
             Issue a diagnostic. */
          if (is_reference_type(*type_ptr)) {
#if RESTRICT_ALLOWED
            /* "restrict" may be applied to reference types, but the other
                qualifiers may not. */
            if ((*qualifiers & ~TQ_RESTRICT) != TQ_NONE) {
              *qualifiers &= TQ_RESTRICT;
              pos_warning(ec_useless_type_qualifiers,
                          &non_restrict_qualifier_pos);
            }  /* if */
#else /* !RESTRICT_ALLOWED */
            *qualifiers = TQ_NONE;
            pos_warning(ec_useless_type_qualifiers,
                        &non_restrict_qualifier_pos);
#endif /* RESTRICT_ALLOWED */
          }  /* if */        
        } else {
          /* In C we check for duplicate qualifiers on a declaration, even
             if, in the case of an array type, one is a top-level qualifier
             and the other qualifies an element type.  That's why top_level
             is set to FALSE here -- that's normally not the case in C mode. */
          /* According to 3.5.3: "If the specification of an array type
             includes any type qualifiers, the element type is so-qualified,
             not the array type.", and this is interpreted recursively
             for arrays of arrays.  The type qualifiers therefore apply
             to the ultimate element type.  This can only happen with typedefs,
             as in "typedef int A[2][3]; const A a;", which makes "a" an
             array of array of const int. */
          if ((*qualifiers &
               f_get_type_qualifiers(*type_ptr, /*top_level=*/FALSE)) != 0) {
            /* Duplication of type qualifier (probably because of a typedef
               that is already qualified). */
            error(ec_dupl_type_qualifier);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
#if RESTRICT_ALLOWED
      /* The restrict qualifier may only be applied to pointer and reference
         types (but not pointer-to-function-type), pointer-to-member types,
         and (in parameter declarations only) array types. */
      if ((*qualifiers & TQ_RESTRICT) &&
          !restrict_qualifier_is_allowed(*type_ptr, &start_pos)) {
        /* Diagnostic has already been issued.  Just remove TQ_RESTRICT
           from the qualifier set. */
        *qualifiers &= ~TQ_RESTRICT;
        err = TRUE;
      }  /* if */
#endif /* RESTRICT_ALLOWED */
      if (*qualifiers != TQ_NONE) {
        /* Add the qualifiers if necessary.  make_qualified_type understands
           the strange array case too. */
        *type_ptr = make_qualified_type(*type_ptr, *qualifiers);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If there was an error, assume something was declared.  Who knows what the
     correct code should have done. */
  if (err || declares_something) *output_flags |= DSO_DECLARES_SOMETHING;
  if (defines_something) *output_flags |= DSO_DEFINES_SOMETHING;
#if DEBUG
  if (debug_level >= 3) {
    fputs("type_ptr: ", f_debug);
    if (*type_ptr == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_type(*type_ptr);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(err);
}  /* decl_specifiers */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
