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

disambig.c -- Disambiguation of C++ declarations and expressions.

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

/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define prescan_curr_id_is_type_name()					\
  (curr_type_symbol(/*is_new_type_name=*/FALSE, /*in_prescan=*/TRUE) != NULL)

/*
Macros to test bits in a disambiguation flag set.
*/
#define real_declarator_allowed(flags)					\
  (((flags) & DFS_REAL_DECLARATOR_ALLOWED) != 0)

#define abstract_declarator_allowed(flags)				\
  (((flags) & DFS_ABSTRACT_DECLARATOR_ALLOWED) != 0)

#define single_type_required(flags)					\
  (((flags) & DFS_SINGLE_TYPE_REQUIRED) != 0)

#define is_condition(flags)						\
  (((flags) & DFS_IS_CONDITION) != 0)

#define condition_is_for_stmt(flags)					\
  (((flags) & DFS_CONDITION_IS_FOR_STMT) != 0)

#define is_cast(flags)							\
  (((flags) & DFS_IS_CAST) != 0)

#define is_template_decl(flags)					\
  (((flags) & DFS_IS_TEMPLATE_DECL) != 0)


/*
Macro that returns GID_USE_PROTOTYPE_NOT_NONREAL if is_template_decl is TRUE.
*/
#define gid_flags_for_template(flags)					\
  (is_template_decl(flags) ? GID_USE_PROTOTYPE_NOT_NONREAL : GID_NO_OPTIONS)



static void cache_tokens_until(a_token_cache	*token_cache_ptr,
			       a_token_kind	stop_token)
/*
Wrapper for cache_token_stream that saves and restores the stop tokens
array.  Cache tokens until the specified token is found.
*/
{
  a_token_set_array  stop_token_array;

  clear_token_set_array(stop_token_array);
  incr_token_set_array_element(stop_token_array, stop_token);
  cache_token_stream(token_cache_ptr, stop_token_array);
}  /* cache_tokens_until */


static void prescan_initializer(a_token_cache	*token_cache_ptr)
/*
Cache the tokens that comprise an initializer of the form
"= initializer-clause".
*/
{
  a_token_set_array  stop_token_array;

  clear_token_set_array(stop_token_array);
  incr_token_set_array_element(stop_token_array, tok_comma);
  incr_token_set_array_element(stop_token_array, tok_semicolon);
  incr_token_set_array_element(stop_token_array, tok_rparen);
  cache_token_stream(token_cache_ptr, stop_token_array);
}  /* prescan_initializer */


static void get_token_and_coalesce_if_identifier(a_disambig_flag_set flags)
/*
Get a token and, if it is a tok_identifier, call
is_generalized_identifier_start to coalesce it in case it is the
beginning of something like a qualified name.  This is used by the
disambiguation routines to ensure that any identifiers that are
scanned are coalesced prior to analysis.
*/
{
  (void)get_token();
  (void)is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                        gid_flags_for_template(flags));
}  /* get_token_and_coalesce_if_identifier */


#if MICROSOFT_EXTENSIONS_ALLOWED
static
void prescan_microsoft_extended_decl_modifiers
                                         (a_token_cache       *token_cache_ptr,
                                          a_disambig_flag_set flags)
/*
Prescan the Microsoft __declspec specifier:

	__declspec ( extended-decl-modifier-seq )

When this routine is called, the current token must be the __declspec
keyword.
*/
{
  check_assertion_str2(curr_token == tok_declspec,
                       "prescan_microsoft_extended_decl_modifiers:",
                       "curr_token not tok_declspec");
  /* Bypass the __declspec token. */
  cache_curr_token(token_cache_ptr);
  (void)get_token();
  if (curr_token == tok_lparen) {
    cache_curr_token(token_cache_ptr);
    get_token_and_coalesce_if_identifier(flags);
    while (curr_token == tok_identifier) {
      cache_curr_token(token_cache_ptr);
      get_token_and_coalesce_if_identifier(flags);
    }  /* while */
    if (curr_token == tok_rparen) {
      cache_curr_token(token_cache_ptr);
      get_token_and_coalesce_if_identifier(flags);
    }  /* if */
  }  /* if */
}  /* prescan_microsoft_extended_decl_modifiers */


static void prescan_based_modifier(a_token_cache       *token_cache_ptr,
                                   a_disambig_flag_set flags)
/*
Prescan the Microsoft __based modifier:

	__based ( identifier )

When this routine is called, the current token must be the __based
keyword.
*/
{
  check_assertion_str2(curr_token == tok_based,
                       "prescan_microsoft_extended_decl_modifiers:",
                       "curr_token not tok_based");
  /* Bypass the __based token. */
  cache_curr_token(token_cache_ptr);
  (void)get_token();
  if (curr_token == tok_lparen) {
    cache_curr_token(token_cache_ptr);
    /* Get the next token, which should be an identifier. */
    get_token_and_coalesce_if_identifier(flags);
    if (curr_token == tok_identifier) {
      /* Get the next token, which should be a right paren. */
      cache_curr_token(token_cache_ptr);
      get_token_and_coalesce_if_identifier(flags);
      if (curr_token == tok_rparen) {
        /* Bypass the right paren. */
        cache_curr_token(token_cache_ptr);
        get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* prescan_based_modifier */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static a_boolean is_ctor_or_dtor(void)
/*
Return TRUE if the current locator is for a constructor or destructor
definition.  The locator must refer to a qualified name.  
*/
{
  a_boolean	result = FALSE;
  a_boolean	err;

#if 0
  /* This will need to be updated to support member templates. */
#endif /* 0 */
  if (locator_for_curr_id.is_qualified_name) {
    if (locator_for_curr_id.is_destructor_name) {
      /* This is a destructor name. */
      result = TRUE;
    } else {
      /* To see if this is a constructor, look up the identifier using
         a tentative type lookup (so that no error will be issued if the
         name is not found).  The look at the resulting symbol. */
      a_symbol_ptr	sym;
      (void)coalesce_and_lookup_qualified_name(GID_NO_OPTIONS,
                                               ilm_tentative_type,
                                               &err);
      sym = locator_for_curr_id.specific_symbol;
      if (sym != NULL && is_constructor_symbol(sym)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_ctor_or_dtor */


static void prescan_decl_specifiers(a_token_cache       *token_cache_ptr,
                                    a_disambig_flag_set flags,
                                    a_boolean           *may_be_decl)
/*
Scan and cache the tokens that comprise a list of decl_specifiers.
*/
{
  a_boolean	is_decl_specifier_token = TRUE;
  a_boolean	any_decl_specifiers = FALSE;
  a_boolean	type_specifier_seen = FALSE;
  a_boolean	is_ctor_or_dtor_name = FALSE;
  a_symbol_ptr	sym;
  for (;;) {
    switch (curr_token) {
      /* Storage class specifiers. */
      case tok_auto:
      case tok_register:
      case tok_static:
      case tok_extern:
      case tok_mutable:
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* The Microsoft __inline keyword is treated as a storage class. */
      case tok_microsoft_inline:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Function specifiers. */
      case tok_inline:
      case tok_virtual:
      /* Friend and typedef. */
      case tok_friend:
      case tok_typedef:
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_declspec:
        prescan_microsoft_extended_decl_modifiers(token_cache_ptr, flags);
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Type specifier - identifier that may be a simple type name.
         If we haven't yet seen a type specifier, then this identifier,
         if it is a type, is the type specifier.  Otherwise, this is
         probably the start of the declarator, so we stop scanning
         decl-specifiers. */
      case tok_identifier:
        sym = locator_for_curr_id.specific_symbol;
        /* If we are scanning a template declaration, see if this is a
           constructor or destructor declaration.  This is not done in other
           cases because the other disambiguation contexts are not in a
           namespace scope, and constructor and destructor declarations are
           not permitted. */
        is_ctor_or_dtor_name = is_template_decl(flags) && is_ctor_or_dtor();
        if (!type_specifier_seen &&
            !is_ctor_or_dtor_name &&
            (prescan_curr_id_is_type_name() ||
             (sym != NULL && sym->kind == (a_symbol_kind)sk_class_template))) {
          /* A class template will probably result in a "missing template
             argument list" error later.  Consider it as a type name for now
             though. */
          type_specifier_seen = TRUE;
        } else {
          is_decl_specifier_token = FALSE;
        }  /* if */
        if (sym != NULL && !is_template_class_symbol(sym)) {
          /* Clear the locator field so that the lookup done by
             curr_id_is_type_name will not be used when the statement
             is actually parsed later. */
          clear_specific_symbol(locator_for_curr_id);
        }  /* if */
        break;
      /* Type specifier - other simple type name tokens. */
      case tok_char:
      case tok_short:
      case tok_int:
      case tok_long:
      case tok_signed:
      case tok_unsigned:
      case tok_float:
      case tok_double:
      case tok_void:
      case tok_bool:
      case tok_wchar_t:
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Microsoft type specifiers. */
      case tok_int32:
      case tok_int64:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        type_specifier_seen = TRUE;
        break;
      /* Type qualifier. */
      case tok_const:
      case tok_volatile:
#if RESTRICT_ALLOWED
      case tok_restrict:
#endif /* RESTRICT_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Microsoft type qualifiers. */
      case tok_unaligned:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        break;
      case tok_class:
      case tok_struct:
      case tok_union:
      case tok_enum:
        /* This could be an elaborated type specifier or the start of
           a enum or class specifier.  The prescanning routines can't
           handle enum and class specifiers, but there should be no need
           for them to do so because types can't be defined in parameter
           lists, and other contexts that are involved in disambiguation.
           We assume this is an elaborated type specifier */
        cache_curr_token(token_cache_ptr);
        (void)get_token_and_coalesce_if_identifier(flags);
        if (type_specifier_seen) {
          /* We've already seen a type specifier, this is probably an
             error. */
          is_decl_specifier_token = FALSE;
        } else {
          /* The token after the class/struct/union/enum keyword must be
             an identifier. */
          if (curr_token != tok_identifier) {
            is_decl_specifier_token = FALSE;
          } else {
            type_specifier_seen = TRUE;
          }  /* if */
        }  /* if */
        break;
      case tok_ellipsis:
        /* An ellipsis is not really a decl-specifier, but we allow it
           here because the prescanning routines are sometimes used to
           scan what may be a function parameter which could look like
           "int ...". */
        break;
      default:
        is_decl_specifier_token = FALSE;
        break;
    }  /* switch */
    /* If this token is not part of a decl-specifier then exit the loop. */
    if (!is_decl_specifier_token) break;
    any_decl_specifiers = TRUE;
    /* Cache this token and get the next one. */
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
  }  /* for */
  if (!any_decl_specifiers && !is_ctor_or_dtor_name) {
    /* A declaration must have at least one decl-specifier, or this must be
       a constructor or destructor.  This requirement is waived for namespace
       scope declarations. */
    *may_be_decl = is_template_decl(flags);
  }  /* if */
  return;
}  /* prescan_decl_specifiers */


/* Forward declaration. */
static void prescan_declaration(a_token_cache       *token_cache_ptr,
                                a_disambig_flag_set flags,
			        a_boolean           is_top_level,
                                a_boolean           *may_be_decl,
                                a_type_ptr          *decl_class_type);


static void prescan_function_declarator
                              (a_token_cache        *token_cache_ptr,
			       a_disambig_flag_set  flags,
                               a_boolean            *may_be_decl)
/*
Scan and cache the tokens that comprise a function declarator.  This routine
is used by the disambiguation routines.  If a construct that cannot be
part of a function declarator is found, may_be_decl is set to FALSE.
*/
{
  /* Scan the function argument list. */
  while (curr_token != tok_rparen) {
    if (curr_token == tok_ellipsis) {
      /* Advance past the ellipsis. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
    } else {
      /* A parameter declaration.  Scan the declaration. */
      prescan_declaration(token_cache_ptr,
			  (DFS_ABSTRACT_DECLARATOR_ALLOWED |
                           DFS_REAL_DECLARATOR_ALLOWED),
                          /*is_top_level=*/FALSE,
                          may_be_decl, (a_type_ptr*)NULL);
      if (!*may_be_decl) goto done;
    }  /* if */
    if (curr_token == tok_comma) {
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
    } else if (curr_token != tok_rparen && curr_token != tok_ellipsis) {
      /* After scanning a parameter declaration we should be at a comma,
         the closing right parenthesis, or an ellipsis that follows an
         argument without an intervening comma.  If not, we conclude that this
         isn't really a declaration. */
     *may_be_decl = FALSE;
     goto done;
    }  /* if */
  }  /* while */
  /* Cache and bypass the right parenthesis. */
  cache_curr_token(token_cache_ptr);
  (void)get_token_and_coalesce_if_identifier(flags);
  /* Skip past any cv-qualifiers associated with this function declarator. */
  while (is_type_qualifier_token(curr_token)) {
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
  }  /* while */
  /* Cache the tokens associated with the optional throw specification.
     Note that we don't try to disambiguate a throw expression from a
     throw declaration. */
  if (curr_token == tok_throw) {
    /* Advance past the throw keyword. */
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
    if (curr_token != tok_lparen) {
      /* A throw specification must follow the throw keyword in a function
         declarator. */
      *may_be_decl = FALSE;
      goto done;
    }  /* if */
    /* Advance past the left parenthesis. */
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
    cache_tokens_until(token_cache_ptr, tok_rparen);
    if (curr_token == tok_rparen) {
      /* Cache the right parenthesis. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
    }  /* if */
  }  /* if */
done:
  return;
}  /* prescan_function_declarator */


static void prescan_declarator(a_token_cache       *token_cache_ptr,
			       a_disambig_flag_set flags,
			       a_boolean           paren_initializer_allowed,
			       a_boolean           is_top_level,
                               a_boolean           *may_be_decl,
                               a_type_ptr          *decl_class_type)
/*
Scan and cache the tokens that comprise a declarator.  This routine
is used by the disambiguation routines.  If a construct that cannot be
part of a declarator is found, may_be_decl is set to FALSE.
*/
{
  a_boolean       paren_initializer_seen = FALSE;
  a_boolean       pointer_operator_seen = FALSE;

  /* Look for one or more instances of a sequence of tokens corresponding
     to ptr-operator.  Syntax:
         * cv-qualifier-list
         & cv-qualifier-list
         complete-class-name :: * cv-qualifier-list
         microsoft-qualifier-list
     Note that neither pointer declarators nor qualifiers are allowed in
     in expressions, so their presence means this is a declaration. */
  for (;;) {
    if (curr_token == tok_star || curr_token == tok_ampersand) {
      /* Cache and bypass the "*" or "&". */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
      pointer_operator_seen = TRUE;
    } else if (is_type_qualifier_token(curr_token)) {
      /* Cache and bypass any cv-qualifiers. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
    } else if (curr_token == tok_ptr_to_member) {
      /* Cache and bypass any pointer to member operators. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (is_microsoft_declarator_keyword()) {
      /* Keywords allowed in declarators in Microsoft mode, e.g., __cdecl. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
    } else if (curr_token == tok_based) {
      /* Microsoft __based modifier. */
      prescan_based_modifier(token_cache_ptr, flags);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* No more ptr-operators. */
      break;
    }  /* if */
  }  /* for */
  if (curr_token == tok_lparen) {
    /* Left parenthesis indicating nested declarator.  For the abstract
       declarator case, this might be a parenthesis indicating a function.
       We can differentiate the two cases because in the case of a nested
       declarator the next token must be a "*", "(", or "[", whereas in the
       function case it is ")", "...", or a declaration specifier. */
    a_boolean	treat_as_expr = FALSE;
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
    if (any_cfront_mode() && is_top_level) {
      /* Cfront handles declarations like
             int a(int());
         as the declaration of an object with an initializer of int()
         (which evaluates to zero), where it really should be a
         function taking parameter of type "function () returning int".
         If this is a top level declaration (e.g., not part of a
         parameter list) don't consider typename() to be a declaration
         in cfront mode. 
 
         Also, in a context in which a parameter declaration
         must be distinguished from an argument expression, cfront seems
         always to treat "type-name ( identifier )" as an expression,
         contrary to our reading of the ARM.  For example:
           class A { A(int); };
           A a(int(x));
         Cfront takes "int(x)" to be an argument to the constructor and
         treats "a" as a variable, but the ARM requires "int(x)" to be a
         declaration and therefore "a" must be a function.  (Note that it
         is a param-decl-vs-arg-expr context if both real and abstract
         declarators are allowed.)

         And finally, cfront makes the same kind of mistake when evaluating
         constructs like this:
           class A { A(int); };
           A(x);
         cfront treats this as a constructor call instead of a declaration
         of an object named x. */
      if (curr_token == tok_identifier && next_token() == tok_rparen) {
        /* Construct like "A(x);". */
        treat_as_expr = TRUE;
      } else if (curr_token == tok_rparen) {
        /* Construct like "A a(int());". */
        treat_as_expr = TRUE;
      } else if (abstract_declarator_allowed(flags) &&
                 !pointer_operator_seen) {
        if (is_type_specifier() || curr_token == tok_identifier) {
          /* Construct like A a(int(x));". */
          treat_as_expr = TRUE;
        }  /* if */
      }  /* if */
      if (treat_as_expr) {
        *may_be_decl = FALSE;
        goto done;
      }  /* if */
    }  /* if */
    if (abstract_declarator_allowed(flags)) {
      if (curr_token == tok_rparen ||
          is_decl_start(/*expr_context=*/FALSE,
                        /*real_declarator_allowed=*/TRUE) ||
                        curr_token == tok_ellipsis) {
        /* Function declarator rather than a nested declarator. */
        goto function_lparen;
      }  /* if */
    }  /* if */
    /* Get the nested declarator. */
    prescan_declarator(token_cache_ptr, flags,
                       /*paren_initializer_allowed=*/FALSE,
                       /*is_top_level=*/FALSE, may_be_decl, decl_class_type);
    if (!*may_be_decl) goto done;
    /* The nested declarator must be followed by a ")". */
    if (curr_token != tok_rparen) {
      *may_be_decl = FALSE;
      goto done;
    }  /* if */
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
  } else {
    /* An identifier is expected next, but is omitted in the 
       abstract declarator.  All tokens that could start an identifier will
       have already been coalesced into a tok_identifier.  Destructor
       declarators are not allowed in this context, so tildes don't need
       to be handled. */
    a_boolean	is_name_start;
    /* Declarator names cannot contain global qualifiers (e.g., ::i). */
    is_name_start = curr_token == tok_identifier &&
                    !locator_for_curr_id.is_global_qualified_name;
    if (!real_declarator_allowed(flags) ||
        (abstract_declarator_allowed(flags) && !is_name_start)) {
      /* Identifier is omitted in an abstract declarator. */
    } else if (!is_name_start) {
      /* Real (non-abstract) declarator.  An identifier must be found here. */
      *may_be_decl = FALSE;
      goto done;
    } else {
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
      if (decl_class_type != NULL) {
        /* Return a pointer to the class of which a member (if any) of the
           declarator. */
        *decl_class_type = qualifier_class_type(locator_for_curr_id);
        /* Clear the may_be_decl flag to suppress further scanning. */
        *may_be_decl = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* if */
  /* The declarator can end at this point, or an array or function
     specification (or a series of them) can follow.  The additional
     specifications, if they appear, are parsed in their order of 
     appearance. */
  while (curr_token == tok_lparen || curr_token == tok_lbracket) {
    if (curr_token == tok_lparen) {
      /* Appears to be a function declarator.  But be sure it's not the
         start of a parenthesized initializer. */
      /* Advance past the left parenthesis. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
      if (curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* See if the think inside the parenthesis looks like an
           initializer. */
        if (paren_initializer_allowed &&
            !is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                              DFS_REAL_DECLARATOR_ALLOWED)) {
          /* Function_declarator should not be called, scan the tokens that
             comprise the parenthesized initializer and exit the loop. */
          paren_initializer_seen = TRUE;
          cache_tokens_until(token_cache_ptr, tok_rparen);
          if (curr_token == tok_rparen) {
            /* Cache the right parenthesis. */
            cache_curr_token(token_cache_ptr);
            (void)get_token_and_coalesce_if_identifier(flags);
          }  /* if */
          break;
        }  /* if */
      }  /* if */
function_lparen:
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      prescan_function_declarator(token_cache_ptr, flags, may_be_decl);
    } else {
      /* Left bracket, indicating array declarator. */
      check_assertion(curr_token == tok_lbracket);
      /* Advance past the left bracket. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
      cache_tokens_until(token_cache_ptr, tok_rbracket);
      /* Bypass and cache the "]". */
      if (curr_token == tok_rbracket) {
        cache_curr_token(token_cache_ptr);
        (void)get_token_and_coalesce_if_identifier(flags);
      }  /* if */
    }  /* if */
  }  /* while */
  /* Look for an initialization that begins with an assignment operator. */
  if (!paren_initializer_seen && curr_token == tok_assign) {
    /* An initializer that begins with an equals sign.  Cache the
       tokens that comprise the initializer and leave curr_token
       as the token following the initializer (usually a comma or
       a semicolon). */
    prescan_initializer(token_cache_ptr);
  } else {
    /* A condition is required to have an "=" style initialization.
       If the initialization is missing, don't consider this to be
       a condition.  This causes things like "int(i)" to be treated as
       expressions, not conditions. */
    if (is_top_level && is_condition(flags)) *may_be_decl = FALSE;
  }  /* if */
done:;
}  /* prescan_declarator */


static void prescan_declaration(a_token_cache       *token_cache_ptr,
                                a_disambig_flag_set flags,
			        a_boolean           is_top_level,
                                a_boolean           *may_be_decl,
                                a_type_ptr          *decl_class_type)
/*
Scan a sequence of tokens and cache them for rescanning later.  The purpose
of this prescan is to help determine whether this is a declaration or an
expression.  The caller guarantees that is_decl_start is TRUE for the current
token.

Assuming that we are in the midst of a declaration, we scan ahead to find
evidence to the contrary. 
*/
{
  a_boolean	is_first_declarator = TRUE;
  db_enter(3, "prescan_declaration");
  /* Coalesce the identifier if this is a tok_identifier. */
  (void)is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL |
                                        gid_flags_for_template(flags));
  for (;;) {
    /* Scan the decl specifiers. */
    prescan_decl_specifiers(token_cache_ptr, flags, may_be_decl);
    if (!*may_be_decl) goto done;
    for (;;) {
      /* Parenthesized initializers are only allowed in contexts that
         in which only real declarators are allowed, but not in
         conditions. */
      a_boolean	paren_initializer_allowed;
      paren_initializer_allowed = !abstract_declarator_allowed(flags) &&
                                  !is_condition(flags);
      prescan_declarator(token_cache_ptr, flags,
                         paren_initializer_allowed,
			 is_top_level && is_first_declarator, may_be_decl,
                         decl_class_type);
      if (!*may_be_decl) goto done;
      /* If we are not processing real declarators, or if we are processing
         a condition, don't look for additional declarators. */
      if (abstract_declarator_allowed(flags) ||
          is_condition(flags) ||
          curr_token != tok_comma) break;
      /* Advance past the comma then scan the next declarator. */
      cache_curr_token(token_cache_ptr);
      (void)get_token_and_coalesce_if_identifier(flags);
      is_first_declarator = FALSE;
    }  /* for */
    /* If multiple types are not allowed, then break out of the loop. */
    if ((!real_declarator_allowed(flags) &&
         single_type_required(flags)) || curr_token != tok_comma) break;
    /* Advance past the comma then scan the next declaration. */
    cache_curr_token(token_cache_ptr);
    (void)get_token_and_coalesce_if_identifier(flags);
  }  /* for */
done:
  db_exit();
}  /* prescan_declaration */


a_boolean f_is_decl_not_expr(a_disambig_flag_set flags)
/*
This routine is called via the macro is_decl_not_expr (in C++ only) to
distinguish statements and expressions from declarations -- for example:
  (1) a statement vs. a declaration, e.g.,
         typedef int I;
         I(i);                // declaration (= I i);
         I(i)++;              // cast i to I, then increment
  (2) in an operator new expression, a parenthesized type vs. a placement
      expression, e.g.,
         new (int(1.5)) A     // placement
         new (int(*  ))       // type
  (3) a parenthesized initializer vs. a parameter declaration, e.g.,
         A a(int(1));         // initialize a by calling A::A() with arg 1
         A a(int(i));         // function a takes int arg, returns A
  (4) a statement vs. a declaration in a condition context
         if (A(i) = 1)        // A (probably invalid) condition declaration
         if (A(i) == 1)       // An expression

The ARM discusses disambiguation in sections 6.8 and 8.1.1.  In general, if a
sequence of tokens looks like a declaration, then it is a declaration, even
if it could also be an expression.  The technique used here involves assuming
a declaration and looking ahead as many tokens as necessary to confirm or
disprove the assumption or, in the case of a more persistent ambiguity, to
decide on the basis of tokens following the "declaration".  Tokens are cached
so that they can be rescanned by the caller.

"flags" provides some information about the context, specifically whether,
if it is a declaration, an abstract or real declarator is expected -- or
either.  The DFS_SINGLE_TYPE_REQUIRED flag is used to indicate that the
construct scanned must be a single type and not a comma separated list.
DFS_IS_CONDITION indicates that a condition statement is being scanned.
DFS_CONDITION_IS_FOR_STMT indicates that the condition is in a for
statement.  DFS_IS_CAST indicates that a cast like "(int)x" is being
scanned, in which case the context following the cast is inspected
to distinguish between cases like (A()), which is an expression and
(A())+1 which is a cast.

These flags are combined in the following ways to handle the various
disambiguation contexts:

Context				Abstract	Real		Single
				allowed		allowed		decl. required
----------------------  	--------	-------		--------------
For-init statements		false		true		disregarded
declaration vs. expr stmt.	false		true		disregarded
sizeof, alignof, new, cast	true		false		true
throw specification		true		false		true
parenthesized initializer vs.	true		true		disregarded
	parameter list


In other words, when a real declarator is allowed, but an abstract declarator
is not, the context is a normal declaration that may contain multiple
declarators.

When both real and abstract declarators are allowed, the context is a
parameter list in which multiple declarations (i.e., not declarators)
may be separated by commas.

When abstract declarators are allowed, but real declarators are not, the
context is one type (when single_type_required is TRUE) or multiple
types separated by commas (when single_type_required is FALSE).

*/
{
  a_token_cache       token_cache;
  a_boolean           may_be_decl = TRUE;
  a_boolean	      prev_do_not_clear_specific_symbol;

  db_enter(3, "f_is_decl_not_expr");
  /* The ambiguous cases all begin a type name followed by a left
     parenthesis.   Check for this case first to quickly discard most
     cases. */
  if (next_token() == tok_lparen && is_type_start()) {
    /* Initialize the token cache. */
    clear_token_cache(&token_cache, /*reusable=*/FALSE);
    if (curr_token == tok_identifier) {
      /* The prescanning process clears the specific symbol field of the
         locator to prevent the prescanning process from biasing
         subsequent lookups.  If the locator is already set, however, then
         the prescanning process should not cause it to be cleared. */
      prev_do_not_clear_specific_symbol =
                            locator_for_curr_id.do_not_clear_specific_symbol;
      locator_for_curr_id.do_not_clear_specific_symbol = TRUE;
    }  /* if */
    /* Scan forward as far as required to determine whether this is a
       declaration.  Each token that is encountered is cached away, so that
       that they can be restored for the actual scan. */
    prescan_declaration(&token_cache, flags, /*is_top_level=*/TRUE,
                        &may_be_decl, (a_type_ptr*)NULL);
    if (!may_be_decl) goto done;
    /* We should now be at either a comma separating two declarators or at
       the semicolon at the end of the declaration.  If not, assume that this
       is really an expression. */
    if (real_declarator_allowed(flags)) {
      if (abstract_declarator_allowed(flags)) {
        /* We are scanning a parameter list, we should be at the closing
           right parenthesis. */
        if (curr_token != tok_rparen) may_be_decl = FALSE;
      } else {
        /* We are scanning a real declaration, we should be at the end of
           the statement now. */
        if (is_condition(flags) && !condition_is_for_stmt(flags)) {
          /* Condition statements (except in for statements) must end
             with a right parenthesis. */
          if (curr_token != tok_rparen) may_be_decl = FALSE;
        } else {
          /* All other declarations must end in a semicolon. */
          if (curr_token != tok_semicolon) may_be_decl = FALSE;
        }  /* if */
      }  /* if */
    } else {
      /* Otherwise, if we are scanning one or more types.  We should be
         at the right parenthesis. */
      if (curr_token != tok_rparen) may_be_decl = FALSE;
      if (may_be_decl && is_cast(flags)) {
        /* If we are in a cast context, look at what follows the right
           parenthesis to see if it is something that could follow a cast.
           This is to prevent (A()) from being interpreted as an invalid
           cast. */
        cache_curr_token(&token_cache);
        (void)get_token();
        /* If the thing after the right parenthesis is not the start of
           an expression, then this is not a cast -- so indicate that this
           is not a declaration. */
        if (!is_expr_start_token(curr_token)) may_be_decl = FALSE;
      }  /* if */
    }  /* if */
done:
    /* Restore the tokens. */
    rescan_cached_tokens(&token_cache);
    if (curr_token == tok_identifier) {
      /* Restore the saved value of the do_not_clear_specific_symbol
         flag. */
      locator_for_curr_id.do_not_clear_specific_symbol =
                                            prev_do_not_clear_specific_symbol;
    }  /* if */
  }  /* if */
  db_exit();
  return may_be_decl;
}  /* f_is_decl_not_expr */


a_type_ptr prescan_and_find_declarator(a_token_cache *decl_token_cache_ptr)
/*
Scan the declaration that follows "template <...>" and find the
declarator.  Record the class of which a member of the declarator.
The caller provides a token cache containing the tokens to be scanned.
Consequently, the cache built by the prescan routines is not needed
and is discarded.  After the scan is done, any tokens remaining in the
cache passed by the caller are flushed.
*/
{
  a_token_cache       token_cache;
  a_boolean           may_be_decl = TRUE;
  a_type_ptr	      decl_class_type = NULL;

  /* Initialize the token cache. */
  clear_token_cache(&token_cache, /*reusable=*/FALSE);
  rescan_reusable_cache(decl_token_cache_ptr);
  prescan_declaration(&token_cache,
                      DFS_REAL_DECLARATOR_ALLOWED | DFS_IS_TEMPLATE_DECL,
                     /*is_top_level=*/TRUE,
                      &may_be_decl, &decl_class_type);
  /* Flush and remaining tokens from the reusable cache. */
  while (curr_token != tok_end_of_source) (void)get_token();
  /* Skip past the tok_end_of_source. */
  (void)get_token();
  /* Discard the cached token.  They are not needed because we were already
     scanning from a cache. */
  discard_token_cache(&token_cache);
  return decl_class_type;
}  /* prescan_and_find_declarator */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
