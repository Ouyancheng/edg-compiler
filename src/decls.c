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

decls.c -- Scanning of declarations.

*/

#include "basics.h"
#include "decls.h"
#include "def_arg.h"
#include "class_decl.h"
#include "il.h"
#include "symbol_tbl.h"
#include "statements.h"
#include "lexical.h"
#include "error.h"
#include "cmd_line.h"
#include "types.h"
#include "lang_feat.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "expr.h"
#include "exprutil.h"
#include "target.h"
#include "decl_inits.h"
#include "preproc.h"
#include "const_ints.h"
#include "folding.h"
#include "templates.h"
#include "types.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */


/* Declarations required because of mutual recursion. */
static void declaration(a_boolean      function_definition_allowed,
                        a_boolean      extern_implied,
                        a_boolean      is_old_style_param_decl,
                        a_param_id_ptr param_id_list);


/*
Macro that is TRUE if the current token is the start of a storage class
specifier (3.5.1).
*/
#define is_storage_class()                                            \
  (curr_token == tok_typedef  || curr_token == tok_extern   ||        \
   curr_token == tok_static   || curr_token == tok_auto     ||        \
   curr_token == tok_register)


/*
Macro that is TRUE if the current token is the start of a type
specifier (except for the typedef and friend cases).  (3.5.2)
*/
#define is_type_specifier()                                           \
  (curr_token == tok_void     || curr_token == tok_char     ||        \
   curr_token == tok_short    || curr_token == tok_int      ||        \
   curr_token == tok_long     || curr_token == tok_float    ||        \
   curr_token == tok_double   || curr_token == tok_signed   ||        \
   curr_token == tok_unsigned || curr_token == tok_struct   ||        \
   curr_token == tok_union    || curr_token == tok_enum     ||        \
   curr_token == tok_class)


/*
Macro that is TRUE if the current token is the start of a function
specifier.
*/
#define is_function_specifier()                                      \
  (curr_token == tok_inline   || curr_token == tok_virtual)

/*
Macro that is TRUE if the current token is the start of a type qualifier
(3.5.3).
*/
#define is_type_qualifier()                                           \
  (curr_token == tok_const    || curr_token == tok_volatile )


/*
Macro that is TRUE if the current identifier token is the start of a
pointer-to-member declarator (class-name :: *).  is_qualified_name_start
calls is_generalized_identifier_start, which sets curr_token to
tok_ptr_to_member and returns FALSE if a pointer to member is found.
*/
#define is_ptr_to_member_declarator_start()				\
  (!is_qualified_name_start() && curr_token == tok_ptr_to_member)


/*
Macro that is TRUE if the current token is the start of an abstract
declarator (3.5.5).
*/
#define is_abstract_declarator_start()                                \
  (curr_token == tok_star || curr_token == tok_lbracket ||            \
   curr_token == tok_lparen ||                                        \
   (C_dialect == C_dialect_cplusplus &&                               \
    (is_ptr_to_member_declarator_start() ||                           \
     curr_token == tok_ampersand)))

/*
Macro that is TRUE if the current token is the start of either an
abstract or real declarator.
*/
#define is_abstract_or_real_declarator_start()                        \
  (is_declarator_start() || curr_token == tok_lbracket ||             \
   (C_dialect == C_dialect_cplusplus &&                               \
    is_ptr_to_member_declarator_start()))

static a_boolean is_name_of_curr_class(void)
/*
Return TRUE if the current scope is a class definition and the name of the
class is the same as the name that locator_for_curr_id represents.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
  a_symbol_ptr             class_sym;
  a_boolean                match;

  if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
    match = FALSE;
  } else {
    class_sym = (a_symbol_ptr)ssep->assoc_type->source_corresp.assoc_info;
    match = (locator_for_curr_id.symbol_header == class_sym->header &&
             (!locator_for_curr_id.is_qualified_name ||
              locator_for_curr_id.qualifier_class_type == ssep->assoc_type));
  }  /* if */
  return match;
}  /* is_name_of_curr_class */


static a_symbol_ptr curr_type_symbol(a_boolean is_new_type_name)
/*
The current token is an identifier or, in C++, the "::" at the start of a
global qualified name.  If it is the name of a type (a typedef name or,
in C++, the name of a class, struct, union, or enum), return a pointer to
the symbol.  Otherwise, return NULL.  Ambiguity and access control checking
is not done.
*/
{
  a_symbol_ptr			assoc_symbol;
  a_boolean   			err;
  an_identifier_options_set	options;

  assoc_symbol = NULL;
  if (locator_for_curr_id.is_operator_name ||
      locator_for_curr_id.is_conversion_name) {
    /* Cannot be a type name. */
  } else {
    /* Set the options.  Since this call is a "probe" to determine if the
       current identifier is a type name, don't issue access errors yet, and
       don't complain if the name is that of a template but there are no
       template args (since it may actually be a different use of the name). */
    options = GID_DEFER_ACCESS_ERRORS;
    if (is_new_type_name) options |= GID_IS_NEW_TYPE_NAME;
    if (is_generalized_identifier_start(options)) {
      /* Look up the current token identifier, which may be a qualified name.
         Since curr_type_symbol is often called as part of a test of the
         presence of a type name identifier, it is inappropriate to cause a
         projection symbol to be created in the current scope if in fact it
         projects something other than a type name.  It's easier to suppress
         the creation of such gratuitous projections here than to try to ignore
         them in symbol entry later.  Defer any access errors that may occur
         because we may actually be scanning something that is not a type
         (e.g., a declarator). */
      assoc_symbol = coalesce_and_lookup_generalized_identifier
                         (GID_DTOR_RECOGNIZED | options,
                          ilm_tentative_type, &err);
      if (assoc_symbol != NULL && !is_type_symbol(assoc_symbol)) {
        /* Symbol was found, but it is not a type name symbol.  Return NULL. */
        assoc_symbol = NULL;
      }  /* if */
      /* If a type symbol was found, issue any access errors that may have
         occurred. */
      if (assoc_symbol) {
        issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
      }  /* if */
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_type_symbol */


/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define curr_id_is_type_name()						\
  (curr_type_symbol(/*is_new_type_name=*/FALSE) != NULL)

/*
Macro that is TRUE if the current token is an identifier that represents
the name of a type (a typedef name or, in C++, the name of a class, struct,
union, or enum).  Also works if the current is the "::" at the start of
a global qualified name.
*/
#define is_type_name() (is_qualified_name_start() && curr_id_is_type_name())


a_boolean is_type_start(void)
/*
Return TRUE if the current token looks like the start of a type.  A type
starts with a type-specifier (including a typedef name) or a type-qualifier.
*/
{
  a_boolean    is_start = FALSE;

  if (is_type_specifier() || is_type_qualifier() ||
      is_function_specifier() || curr_token == tok_friend) {
    is_start = TRUE;
  } else if (is_type_name()) {
    /* Identifier that is a type name (a typedef name or, in C++,
       the name of a class, struct, or union). */
    is_start = TRUE;
  }  /* if */
  return(is_start);
}  /* is_type_start */


a_boolean is_decl_start(a_boolean  expr_context,
                        a_boolean  real_declarator_allowed)
/*
Return TRUE if the current token looks like the start of a declaration,
i.e., it is the start of a type-specifier, a type-qualifier, or a
storage-class-specifier.  Note that this does not cover the start of
function-definitions, since they can start with the declarator.  If
expr_context is TRUE this test is done in a context in which an expression
is allowed.  If real_declarator_allowed is FALSE the error recovery
optimization is suppressed.
*/
{
  a_boolean     is_start = FALSE;
  a_token_kind  next_tok;

  if (is_storage_class()) {
    /* A storage-class-specifier. */
    is_start = TRUE;
  } else if (curr_token == tok_template) {
    /* Probably an error. */
    is_start = TRUE;
  } else if (is_type_start()) {
    /* Is start of type. */
    is_start = TRUE;
  } else if (curr_token == tok_identifier &&
             !is_error_locator(locator_for_curr_id)) {
    /* A special check to produce better error recovery in certain cases.
       If the lexical sequence suggests that this is a declaration even
       though the current identifier is not defined (and therefore not
       recognized as a type name), call it a declaration anyway. */
    if (!real_declarator_allowed) {
      /* With a sizeof or cast operation, real_declarator_allowed will come
         in as FALSE.  There's no point in looking ahead in such cases:
         "sizeof(x y)" isn't syntactically possible, so if x is not a
         type name, we'll assume it's an object name. */
    } else if (symbol_list_from_locator(locator_for_curr_id) == NULL) {
      /* If the current token is an identifier we'll proceed with the error
         recovery optimization only if we can be quite sure the name can't
         have another meaning.  The first indication of that is that there
         are no active symbols with this name in scope.  However, if the
         scope stack has a class reactivation entry on it or a class that
         itself has base classes, a deactivated symbol (one from the inactive
         list) may be visible.  Note that that this is an issue only if we
         are in a context that accepts an expression and there are inactive
         symbols associated with this name. */
      if (expr_context &&
          inactive_symbol_list_from_locator(locator_for_curr_id) != NULL &&
          scope_stack[depth_scope_stack].inactive_symbols_may_be_visible) {
        /* The scope stack contains either a class with base classes or a
           reactivated class.  In either case there may be a member among the
           inactive symbols, so we will suppress the optimization. */
      } else {
        /* An undefined identifier.  Check the next token -- we may have a
           token pattern that can be nothing but a declaration. */
        next_tok = next_token();
        if (next_tok == tok_identifier || next_tok == tok_operator) {
          /* Pattern "x y" or "x operator..." -- looks like a declaration in
             'most any context. */
          is_start = TRUE;
        } else if (!expr_context &&
                   (next_tok == tok_star || next_tok == tok_ampersand)) {
          /* Pattern "x *..." or x &..." -- looks like a declaration as long as
             the context rules out expressions. */
          is_start = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return(is_start);
}  /* is_decl_start */


/* Forward declaration because of indirect recursion involving
   prescan_declarator and prescan_declaration. */
static void prescan_declaration(a_token_cache  *token_cache_ptr,
                                a_boolean      abstract_declarator_allowed,
                                a_boolean      real_declarator_allowed,
                                a_boolean      *may_be_decl,
                                a_boolean      *may_be_expr);

static void prescan_declarator(a_token_cache  *token_cache_ptr,
                               a_boolean      abstract_declarator_allowed,
                               a_boolean      real_declarator_allowed,
                               a_boolean      *may_be_decl,
                               a_boolean      *may_be_expr)
/*
We are in the midst of scanning what is assumed to be a declaration -- at
the point where, if it is a declaration, the declarator should begin.  Both
*may_be_decl and *may_be_expr will be TRUE upon entry, signifying that the
ambiguity is not yet resolved.  We scan the tokens looking for evidence
that it is in fact a declarator, and can be nothing else, (in which case
*may_be_expr is set to FALSE) or cannot possibly be a declarator (in which
it is *may_be_decl that is set to FALSE).  Whichever way the ambiguity is
resolved, scanning is stopped immediately.  If the ambiguity is not resolved
we keep scanning till the end of the declarator and return leaving both
*may_be_decl and *may_be_expr TRUE.
*/
{
  a_boolean  pointer_operator_seen = FALSE;

  db_enter(4, "prescan_declarator");
  /* Look for one or more instances of a sequence of tokens corresponding
     to ptr-operator.  Syntax:
         * cv-qualifier-list
         & cv-qualifier-list
         complete-class-name :: * cv-qualifier-list
     Note that neither pointer declarators nor qualifiers are allowed in
     in expressions, so their presence means this is a declaration. */
  for (;;) {
    if (curr_token == tok_star || curr_token == tok_ampersand) {
      pointer_operator_seen = TRUE;
      /* Cache and bypass the "*" or "&". */
      cache_curr_token(token_cache_ptr);
      (void)get_token();
      if (curr_token == tok_const || curr_token == tok_volatile ||
          curr_token == tok_rparen) {
        /* Qualifier rules out expression.  So does an asterisk followed
           by a right paren. */
        *may_be_expr = FALSE;
        goto done;
      }  /* if */
      /* Keep looping. */
    } else if (is_ptr_to_member_declarator_start()) {
      /* Pointer to member declarator rules out expression. */
      *may_be_expr = FALSE;
      goto done;
    } else {
      /* No more ptr-operators. */
      break;
    }  /* for */
  }  /* for */
  /* In a declaration a "(" here would signal either a nested declarator or
     the start of a function type's parameter declaration list. */
  if (curr_token == tok_lparen) {
    /* Cache and bypass the "(". */
    cache_curr_token(token_cache_ptr);
    (void)get_token();
    if (abstract_declarator_allowed) {
      /* In an abstract declarator a ")" or type specifier following the "("
         would indicate the presence of a param list.  But an ellipsis can
         appear only in a declaration. */
      if (curr_token == tok_rparen ||
          is_decl_start(/*expr_context=*/FALSE,
                        /*real_declarator_allowed=*/TRUE)) {
        goto function_lparen;
      } else if (curr_token == tok_ellipsis) {
        *may_be_expr = FALSE;
        goto done;
      }  /* if */
    }  /* if */
    /* Determine whether the current token could be the start of a nested
       declarator. */
    prescan_declarator(token_cache_ptr, abstract_declarator_allowed,
                       real_declarator_allowed, may_be_decl, may_be_expr);
    /* Bail out if there was any resolution of the ambiguity. */
    if (!*may_be_expr || !*may_be_decl) goto done;
    if (curr_token == tok_rparen) {
      /* Cache and bypass the ")". */
      cache_curr_token(token_cache_ptr);
      (void)get_token();
    } else {
      /* An unexpected token following what was thought to be a parenthesized
         declarator disqualifies this as a declarator.  For instance,
         int((a)) is a declaration but int((a+b)) is a cast. */
      may_be_decl = FALSE;
      goto done;
    }  /* if */
  } else if (curr_token == tok_lbracket) {
    /* May be an array declarator. */
  } else {
    /* Not a nested declarator.  May be a real declarator. */
    if (is_qualified_name_start() || curr_token == tok_operator) {
      /* Appears to be a real declarator.  But if a real declarator is not
         allowed in the current context, it's probably an expression. */
      if (!real_declarator_allowed) {
        *may_be_decl = FALSE;
        goto done;
      } else if (abstract_declarator_allowed && cfront_compatibility_mode &&
                 curr_token == tok_identifier && !pointer_operator_seen) {
        /* Cfront bug.  In a context in which a parameter declaration
           must be distinguished from an argument expression, cfront seems
           always to treat "type-name ( identifier ... )" as an expression,
           contrary to our reading of the ARM.  For example:
             class A { A(int); };
             A a(int(x));
           Cfront takes "int(x)" to be an argument to the constructor and
           treats "a" as a variable, but the ARM requires "int(x)" to be a
           declaration and therefore "a" must be a function.  (Note that it
           is a param-decl-vs-arg-expr context if both real and abstract
           declarators are allowed.) */
        *may_be_decl = FALSE;
        goto done;
      } else if (curr_token == tok_identifier &&
                 locator_for_curr_id.is_global_qualified_name &&
                 next_token() == tok_rparen) {
        /* Looks like a cast expression -- int(::x).  Note that global
           qualifiers are not allowed (by the syntax) to be used in
           declarators.  Other qualifiers are allowed by the syntax but
           are not valid in most declarations.  Only global qualifiers,
           however, are sufficient to conclude that this cannot be a
           declaration. */
        *may_be_decl = FALSE;
        goto done;
      }  /* if */
      if (curr_token == tok_identifier) {
        /* Cache and bypass the identifier. */
        cache_curr_token(token_cache_ptr);
        (void)get_token();
      } else if (curr_token == tok_operator) {
        /* Cache and bypass "operator". */
        cache_curr_token(token_cache_ptr);
        (void)get_token();
        /* Cache and bypass the one- or two-token sequence that specifies
           the overloaded operator. */
        if (curr_token == tok_lparen || curr_token == tok_lbracket) {
          cache_curr_token(token_cache_ptr);
          (void)get_token();
        }  /* if */
        cache_curr_token(token_cache_ptr);
        (void)get_token();
        /* A left paren is required next, whether this is a declaration or a
           call.  If it's not there, mark this as not a declaration, even
           though it's not a legal expression, either. */
        if (curr_token != tok_lparen) {
          *may_be_decl = FALSE;
          goto done;
        }  /* if */
      }  /* if */
    } else if (curr_token == tok_compl) {
      /* A "~" is always taken to specify an expression, since the syntax
         for declaring a destructor is very restrictive. */
      *may_be_decl = FALSE;
      goto done;
    } else if (curr_token == tok_rparen) {
      if (!abstract_declarator_allowed) *may_be_decl = FALSE;
      goto done;
    } else {
      /* The current token doesn't fit into the pattern for a declaration
         (e.g., it's a constant or an operator), so it's likely to be an
         expression. */
      *may_be_decl = FALSE;
      goto done;
    }  /* if */
  }  /* if */
  for (;;) {
    /* Look for "[...]" and "(...)". */
    if (curr_token == tok_lbracket) {
      /* Cache and bypass the "[" and all tokens following it up to but not
         including the matching "]".  Note that what's contained within the
         brackets can't help resolve the ambiguity, so we can ignore it. */
      a_stop_token_array  save_stop_token_array;

      /* Save the current stop token state, and reinitialize it. */
      copy_stop_tokens(stop_token_array, save_stop_token_array);
      clear_stop_tokens();
      add_stop_token(tok_rbracket);
      cache_token_stream(token_cache_ptr);
      remove_stop_token(tok_rbracket);
      /* Restore the stop token state. */
      copy_stop_tokens(save_stop_token_array, stop_token_array);
      /* Bypass and cache the "]". */
      if (curr_token == tok_rbracket) {
        cache_curr_token(token_cache_ptr);
        (void)get_token();
      }  /* if */
    } else if (curr_token == tok_lparen) {
      /* Cache and bypass the "(". */
      cache_curr_token(token_cache_ptr);
      (void)get_token();
function_lparen:
      /* Scan each parameter declaration to be sure it is a declaration.  If
         one turns out not to be, we take it to be an expression, which means
         we have a function call rather than a function declaration. */
      while (curr_token != tok_rparen) {
        if (!is_decl_start(/*expr_context=*/FALSE,
                           /*real_declarator_allowed=*/TRUE)) {
          /* Not a declaration, since old-style parameter lists are not
             allowed. */
          *may_be_decl = FALSE;
          goto done;
        } else if (next_token() == tok_lparen) {
          /* See if there is any ambiguity in the parameter declaration.
             This could be a declaration or it could be a cast or a
             constructor call. */
          prescan_declaration(token_cache_ptr,
                              /*abstract_declarator_allowed=*/TRUE,
                              /*real_declarator_allowed=*/TRUE,
                              may_be_decl, may_be_expr);
          /* If the ambiguity is resolved, we can bail out. */
          if (!*may_be_decl || !*may_be_expr) goto done;
        } else {
          /* Scan through the tokens comprising the parameter declaration. */
          for (;;) {
            cache_curr_token(token_cache_ptr);
            (void)get_token();
            if (curr_token == tok_rparen || curr_token == tok_comma ||
                curr_token == tok_end_of_source) {
              break;
            } else if (is_declarator_start()) {
              prescan_declarator(token_cache_ptr,
                                 /*abstract_declarator_allowed=*/TRUE,
                                 /*real_declarator_allowed=*/TRUE,
                                 may_be_decl, may_be_expr);
              /* If the ambiguity is resolved, we can bail out. */
              if (!*may_be_decl || !*may_be_expr) goto done;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        /* Cache and bypass the "," if there is one. */
        if (curr_token == tok_comma) {
          cache_curr_token(token_cache_ptr);
          (void)get_token();
        } else if (curr_token != tok_rparen) {
          /* No comma, so since we are not at the closing parenthesis we may
             take this to be an expression. */
          *may_be_decl = FALSE;
          goto done;
        }  /* if */
      }  /* while */
      /* We have completed the parameter declarations (if any), so we should
         be at the closing paren. */
      if (curr_token == tok_rparen) {
        /* Cache and bypass the ")". */
        cache_curr_token(token_cache_ptr);
        (void)get_token();
        /* Look for "const" or "volatile" qualifier, which appear only on
           declarations. */
        if (curr_token == tok_const || curr_token == tok_volatile) {
          *may_be_expr = FALSE;
          goto done;
        }  /* while */
      }  /* if */
    } else {
      /* Not a "(" or a "[", so stop here and let the caller figure it out. */
      break;
    }  /* if */
  }  /* for */
done:;
  db_exit();
}  /* prescan_declarator */


static void prescan_declaration(a_token_cache  *token_cache_ptr,
                                a_boolean      abstract_declarator_allowed,
                                a_boolean      real_declarator_allowed,
                                a_boolean      *may_be_decl,
                                a_boolean      *may_be_expr)
/*
Scan a sequence of tokens and cache them for rescanning later.  The purpose
of this prescan is to help determine whether this is a declaration or an
expression.  The caller guarantees that is_decl_start is TRUE for the current
token.

Assuming that we are in the midst of a declaration, we scan ahead to find
evidence to the contrary.  If a token sequence appears that unequivocally
confirms this to be a declaration, set *may_be_expr to FALSE;  if it is
*not* a declaration, *may_be_decl is FALSE and *may_be_expr is TRUE.  It
can be that both are TRUE.
*/
{
  db_enter(3, "prescan_declaration");
  if (next_token() == tok_lparen &&
      ((curr_token == tok_identifier && curr_id_is_type_name()) ||
        curr_token == tok_void || curr_token == tok_char ||
        curr_token == tok_short || curr_token == tok_int ||
        curr_token == tok_long || curr_token == tok_float ||
        curr_token == tok_double || curr_token == tok_signed ||
        curr_token == tok_unsigned)) {
    /* Disambiguation is required.  This could be a cast expression or a
       constructor call. */
    *may_be_expr = TRUE;
    /* Cache the current token.  Then advance past the left paren and cache
       it, too. */
    cache_curr_token(token_cache_ptr);
    (void)get_token();
    cache_curr_token(token_cache_ptr);
    /* Advance to the first token within the parentheses. */
    (void)get_token();
    if (curr_token == tok_rparen) {
      *may_be_decl = FALSE;
    } else {
      prescan_declarator(token_cache_ptr, abstract_declarator_allowed,
                         real_declarator_allowed, may_be_decl, may_be_expr);
      if (real_declarator_allowed && abstract_declarator_allowed) {
        /* This may be an arg list and it may be a param list.  If the
           ambiguity was not resolved by looking at the first item (arg or
           param) in the list, look at the next item.  Note that the recursive
           invocation of prescan_declaration has the effect of bumping down
           the list till it ends or until the ambiguity is resolved. */
        if (*may_be_decl && *may_be_expr) {
          /* Ambiguity is still unresolved. */
          if (curr_token == tok_rparen && next_token() == tok_comma) {
            /* Advance past the right paren. */
            cache_curr_token(token_cache_ptr);
            (void)get_token();
            /* Advance past the comma. */
            cache_curr_token(token_cache_ptr);
            (void)get_token();
            if (!is_decl_start(/*expr_context=*/FALSE,
                               /*real_declarator_allowed=*/TRUE)) {
              *may_be_decl = FALSE;
            } else {
              prescan_declaration(token_cache_ptr, abstract_declarator_allowed,
                                  real_declarator_allowed, may_be_decl,
                                  may_be_expr);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (*may_be_decl && *may_be_expr) {
        /* If this is not a rparen, can't we assume that we have an expression
           here.  For instance, int(*p + 1). */
        if (curr_token != tok_rparen && curr_token != tok_end_of_source) {
          *may_be_decl = FALSE;
        } else {
          /* Cache and bypass the ")". */
          cache_curr_token(token_cache_ptr);
          (void)get_token();
          switch (curr_token) {
            case tok_assign:
            case tok_lparen:
            case tok_const:
            case tok_volatile:
            case tok_lbracket:
            case tok_comma:
            case tok_semicolon:
              /* It's a declaration. */
              break;
            case tok_period:
            case tok_arrow:
            case tok_plus_plus:
            case tok_minus_minus:
            case tok_ampersand:
            case tok_star:
            case tok_plus:
            case tok_minus:
            case tok_divide:
            case tok_remainder:
            case tok_shift_left:
            case tok_shift_right:
            case tok_lt:
            case tok_gt:
            case tok_le:
            case tok_ge:
            case tok_eq:
            case tok_ne:
            case tok_excl_or:
            case tok_or:
            case tok_and_and:
            case tok_or_or:
            case tok_quest_mark:
            case tok_times_assign:
            case tok_divide_assign:
            case tok_remainder_assign:
            case tok_plus_assign:
            case tok_minus_assign:
            case tok_shift_left_assign:
            case tok_shift_right_assign:
            case tok_and_assign:
            case tok_excl_or_assign:
            case tok_or_assign:
            case tok_period_star:
            case tok_arrow_star:
              /* It's an expression. */
              *may_be_decl = FALSE;
              break;
            default:
              /* What's not obviously a declaration or an expression is
                 probably a syntax error.  Let the error be reported in
                 declaration processing. */
              *may_be_expr = FALSE;
          }  /* switch */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    *may_be_expr = FALSE;
  }  /* if */
  db_exit();
}  /* prescan_declaration */


a_boolean f_is_decl_not_expr(a_boolean  abstract_declarator_allowed,
                             a_boolean  real_declarator_allowed)
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

The ARM discusses disambiguation in sections 6.8 and 8.1.1.  In general, if a
sequence of tokens looks like a declaration, then it is a declaration, even
if it could also be an expression.  The technique used here involves assuming
a declaration and looking ahead as many tokens as necessary to confirm or
disprove the assumption or, in the case of a more persistent ambiguity, to
decide on the basis of tokens following the "declaration".  Tokens are cached
so that they can be rescanned by the caller.

The caller provides some information about the context, specifically whether,
if it is a declaration, an abstract or real declarator is expected -- or
either.
*/
{
  a_token_cache       token_cache;
  a_boolean           may_be_decl = TRUE;
  a_boolean           may_be_expr = TRUE;

  db_enter(3, "f_is_decl_not_expr");
  /* Initialize the token cache. */
  clear_token_cache(&token_cache, /*reusable=*/FALSE);
  /* Scan forward as far as required to determine whether this is a
     declaration.  Each token that is encountered is cached away, so that
     that they can be restored for the actual scan. */
  prescan_declaration(&token_cache, abstract_declarator_allowed,
                      real_declarator_allowed, &may_be_decl, &may_be_expr);
  /* Restore the tokens. */
  rescan_cached_tokens(&token_cache);
  db_exit();
  return may_be_decl;
}  /* f_is_decl_not_expr */


a_boolean f_check_for_overload_anachronism(void)
/*
Issue a diagnostic, bypass the current token, which is "overload", and check
the tokens that follow.  If a declaration is of the format "overload f;" (or
"overload f, g, h;") just check for syntax errors and discard the entire
declaration; in such cases return TRUE.  Otherwise, return FALSE --
declaration processing will continue as though "overload" had not been seen.
(This function is only called from the macro check_for_overload_anachronism.)
*/
{
  a_boolean     discard_declaration = FALSE;
  a_token_kind  next_tok;

  db_enter(3, "f_check_for_overload_anachronism");
  check_assertion(curr_token == tok_overload);
  /* Issue an anachronism diagnostic indicating that "overload" is
     no longer allowed.  This can be either an error or a warning. */
  diagnostic(anachronism_error_severity, ec_overload_anachronism);
  /* Bypass "overload" */
  (void)get_token();
  if (curr_token == tok_identifier) {
    next_tok = next_token();
    if (next_tok == tok_semicolon || next_tok == tok_comma) {
      /* We have a single function name or a comma separated list of
         function names.  (We do not support a mixed list of function
         names and function declarations.) Throw away the identifier
         and advance to the ";" or ",". */
      (void)get_token();
      if (curr_token == tok_comma) {
        /* It is a list of names.  Loop through them just to flag syntax
           errors. */
        add_stop_token(tok_semicolon);
        /* Advance past the comma */
        (void)get_token();
        do {
          (void)required_token(tok_identifier, ec_exp_identifier);
        } while (loop_token(tok_comma));
        remove_stop_token(tok_semicolon);
      }  /* if */
      /* Check for final semicolon. */
      (void)required_token(tok_semicolon, ec_exp_semicolon);
      /* Tell the caller to do no more processing. */
      discard_declaration = TRUE;
    } else {
      /* Treat this as a function declaration.  Having bypassed the overload
         keyword we return to the caller. */
    }  /* if */
  }  /* if */
  db_exit();
  return discard_declaration;
}  /* f_check_for_overload_anachronism */


/*
If type_ptr is float, change it to double.  Used in pcc mode to promote
function parameter and return types.
*/
#define promote_float_to_double(type_ptr)                             \
{ if (is_floating_type(type_ptr) &&                                   \
      skip_typerefs(type_ptr)->variant.float_kind ==                  \
                                            (a_float_kind)fk_float) { \
    type_ptr = float_type((a_float_kind)fk_double);                   \
  }  /* if */                                                         \
}  /* promote_float_to_double */


static a_boolean is_cfront_member_function_typedef(a_type_ptr  type_ptr,
                                                   a_type_ptr  *rout_type,
                                                   a_type_ptr  *class_type)
/*
We are checking for a type entry produced by a typedef declaration like
this:

        typedef void A::T(int);  // Nonstandard typedef

(meaning "T" names a routine type for a member function of A that takes an
int argument and returning void.  It's tie to class A is indicated by having
an implicit this-param type of const-ptr-to-A).  Cfront treats "T*" as though
it had been a ptr-to-member declaration -- e.g.,

        T* pm = &A::f(int);      // Nonstd ptr-to-member decl

and

        void (A::*pm)(int) = &A::f(int);

have the very same meaning for cfront.  Although this is not part of the
language defined in the ARM, it is support for cfront compatibility.
*/
{
  a_type_ptr  tp;

  *class_type = NULL;
  if (cfront_compatibility_mode && is_function_type(type_ptr)) {
    *rout_type = skip_typerefs(type_ptr);
    if (*rout_type != type_ptr) {
      tp = (*rout_type)->variant.routine.extra_info->implicit_this_param_type;
      if (tp != NULL) *class_type = type_pointed_to(tp);
    }  /* if */
  }  /* if */
  return (*class_type != NULL);
}  /* is_cfront_member_function_typedef */


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


static void add_to_derived_type_list(a_type_ptr new_type_ptr,
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
        } else if (C_dialect == C_dialect_cplusplus &&
                   is_illegal_abstract_class_type(new_type_ptr)) {
          /* Function return type may not be an abstract class (ARM 10.3). */
          error(ec_function_returning_abstract_class);
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
        set_routine_calling_method_flag(*bottom_derived_type);
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
              tkind = prev_temp_type->kind;
              switch (tkind) {
                case tk_array:
                  tp = prev_temp_type->variant.array.element_type;
                  break;
                case tk_pointer:
                  tp = prev_temp_type->variant.pointer.type;
                  break;
                case tk_routine:
                  tp = prev_temp_type->variant.routine.return_type;
                  break;
                case tk_typeref:
                  tp = prev_temp_type->variant.typeref.type;
                  break;
                case tk_ptr_to_member:
                  tp = pm_member_type(prev_temp_type);
                  break;
#if CHECKING
                default:
                  internal_error("add_to_derived_type_list: bad type in list");
#endif /* CHECKING */
              }  /* switch */
              if (tp == temp_type) break;
              prev_temp_type = tp;
            }  /* for */
            /* Found the previous type entry.  Keep looping. */
            temp_type = prev_temp_type;
          }  /* if */
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
}  /* add_to_derived_type_list */


void adjust_parameter_type(a_type_ptr *type_ptr)
/*
*type_ptr points to the type of a parameter.  Modify the type if
necessary.  See 3.7.1:  A declaration of a parameter as "array of
type" shall be adjusted to "pointer to type", and the declaration of
a parameter as "function returning type" shall be adjusted to
"pointer to function returning type", as in 3.2.2.1.
*/
{
  db_enter(4, "adjust_parameter_type");
  /* Note that incomplete types are allowed.  One can't call a function
     that has an incomplete-type parameter, but one can complete it later. */
  if (is_array_type(*type_ptr)) {
    /* Array, adjust to pointer to element type. */
    *type_ptr = make_pointer_type(array_element_type(*type_ptr));
  } else if (is_function_type(*type_ptr)) {
    /* Function, adjust to pointer to function. */
    *type_ptr = make_pointer_type(*type_ptr);
  }  /* if */
  db_exit();
}  /* adjust_parameter_type */


static void check_type_qualifiers(a_type_ptr *type_ptr)
/*
A parameter or variable is about to be declared with the given type.
Check to see if any type qualifiers that are specified are meaningful.
*/
{
  db_enter(4, "check_type_qualifiers");
  if (is_qualified_type(*type_ptr)) {
    /* The type has type qualifiers. */
    if ((is_function_type(*type_ptr) && C_dialect != C_dialect_cplusplus) ||
        is_void_type(*type_ptr)) {
      /* Type qualifiers on void types are useless.  On function types they
         are undefined (3.5.3; this can happen with something like
           typedef int F();
           const F g;
         ); we mark them as useless.
      */
      warning(ec_useless_type_qualifiers);
      /* The useless qualifiers could be removed by the statement
      *type_ptr = make_unqualified_type(*type_ptr);
	 but they are kept in case the back end assigns any meaning to them. */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_type_qualifiers */


void check_operator_function_params(a_type_ptr        rout_type,
                                    a_type_ptr        class_type,
                                    a_symbol_locator  *locator)
/*
Check the argument list on the declaration of a user-defined conversion
or overloaded operator function.  For conversion functions, no arguments
are allowed.  For operators there are different requirements for different
operator kinds.  Issue a diagnostic if an error is found.
If this routine is modified to use additional fields from the locator
make_template_function needs to be updated to make sure that the
new fields are set properly.
*/
{
  an_opname_kind                 opname;
  int                            param_count;
  a_param_type_ptr               ptp;
  a_boolean                      any_class_type_params = FALSE;
  a_type_ptr                     tp;
  a_boolean                      is_nonstatic_member_function;
  an_error_code                  error_code = ec_no_error;
  a_boolean                      err = FALSE;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "check_operator_function_params");
  rout_type = skip_typerefs(rout_type);
  rtsp = rout_type->variant.routine.extra_info;
  if (is_error_locator(*locator)) {
    /* Nothing to do. */
  } else if (locator->is_conversion_name) {
    check_assertion(class_type != NULL);
    /* Check the target type of the conversion -- which is the return type
       of rout_type. */
    if (!cfront_compatibility_mode &&
        is_void_type(rout_type->variant.routine.return_type)) {
      /* Conversion operators specifying conversion to void type are not
         allowed (Boston X3J16). */
      pos_ty2_error(ec_conversion_to_type_not_allowed,
                    &locator->source_position, class_type,
                    rout_type->variant.routine.return_type);
      err = TRUE;
    }  /* if */
    /* Any parameter is too many for a conversion function. */
    if (rtsp->param_type_list != NULL || rtsp->has_ellipsis) {
      pos_error(ec_too_many_args_for_conversion, &locator->source_position);
      err = TRUE;
    }  /* if */
  } else if (locator->is_operator_name) {
    /* It's an operator. */
    opname = locator->variant.opname;
    check_assertion(opname != (an_opname_kind)onk_none);
    is_nonstatic_member_function =
                routine_type_is_nonstatic_member_function(rout_type);
    /* Operator new/delete cannot be a nonstatic member function. */
    check_assertion(!(is_nonstatic_member_function &&
                      (opname == (an_opname_kind)onk_new ||
                       opname == (an_opname_kind)onk_delete)));
    /* Make a pass over the param types list to count the number of
       arguments to see if there are any parameters that are of class type
       or reference-to-class type.  Note that param_count is initialized to
       0 except in the case of nonstatic member functions, for which it is
       initialized to 1. This is because the implicit "this" parameter is
       counted in the latter case. */
    param_count = is_nonstatic_member_function ? 1 : 0;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      param_count++;
      tp = ptp->type;
      if (is_reference_type(tp)) tp = type_pointed_to(tp);
      if (is_class_struct_union_type(tp)) any_class_type_params = TRUE;
    }  /* if */
    if (opname == (an_opname_kind)onk_function_call ||
        opname == (an_opname_kind)onk_new) {
      /* Function call and new must have one or more arguments. */
      if (param_count == 0) {
        if (rtsp->has_ellipsis) {
          /* operator()(...) and operator new(...) are errors, but we do,
             with some trepidation, allow operator()(T, ...) and
             operator new(size_t, ...). */
          error_code = ec_ellipsis_on_operator_function;
        } else {
          error_code = ec_too_few_args_for_operator;
        }  /* if */
      } else if (opname == (an_opname_kind)onk_new) {
        ptp = rout_type->variant.routine.extra_info->param_type_list;
        tp = ptp->type;
        if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
          if (!is_integral_type(tp) ||
              skip_typerefs(tp)->variant.integer.int_kind !=
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND) {
            error_code = ec_bad_arg_type_for_operator_new;
            ptp->type = error_type();
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (rtsp->has_ellipsis) {
      /* All overloaded operators (except function call and new, handled
         above) require a specific number of arguments, so ellipsis is not
         allowed. */
      error_code = ec_ellipsis_on_operator_function;
    } else if (opname == (an_opname_kind)onk_compl ||
        opname == (an_opname_kind)onk_not ||
        opname == (an_opname_kind)onk_arrow) {
      /* Unary operator must have exactly one argument. */
      if (param_count > 1) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 1) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    } else if (param_count == 1 &&
               (opname == (an_opname_kind)onk_plus ||
                opname == (an_opname_kind)onk_minus ||
                opname == (an_opname_kind)onk_star ||
                opname == (an_opname_kind)onk_ampersand ||
                opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
       /* These operators can be either unary or binary.  It is legal for
          them to have exactly one argument. */
    } else if (param_count == 2 &&
               (opname == (an_opname_kind)onk_plus_plus ||
                opname == (an_opname_kind)onk_minus_minus)) {
      /* Extra argument on postfix operator must be of type "int"
         (ARM 13.4.7). */
      ptp = rout_type->variant.routine.extra_info->param_type_list;
      if (!is_nonstatic_member_function) ptp = ptp->next;
      tp = ptp->type;
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        if (!is_integral_type(tp) ||
            skip_typerefs(tp)->variant.integer.int_kind !=
                                                  (an_integer_kind)ik_int) {
          pos_st_error(ec_bad_extra_arg_for_postfix_operator,
                       &locator->source_position,
                       opname == (an_opname_kind)onk_plus_plus ? "++" : "--");
          ptp->type = error_type();
          err = TRUE;
        }  /* if */
      }  /* if */
    } else if (opname == (an_opname_kind)onk_delete) {
      ptp = rout_type->variant.routine.extra_info->param_type_list;
      if (param_count == 0) {
        error_code = ec_too_few_args_for_operator;
      } else {
        tp = ptp->type;
        if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
          /* Check for "void *" -- notice that is_void_type is not called
             on the type-pointed-to: that is to catch "const void *". */
          if (!is_pointer_type(tp) ||
              skip_typedefs(type_pointed_to(tp))->kind !=
                                                   (a_type_kind)tk_void) {
            if (cfront_compatibility_mode && is_pointer_type(tp) &&
                is_void_type(type_pointed_to(tp))) {
              /* Cfront 2.1 allows "const void *" parameter.  Issue a
                 warning and change the type to "void *". */
              pos_warning(ec_bad_first_arg_type_for_operator_delete,
                          &locator->source_position);
              ptp->type = make_pointer_type(void_type());
            } else {
              pos_error(ec_bad_first_arg_type_for_operator_delete,
                        &locator->source_position);
              ptp->type = error_type();
              err = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        ptp = ptp->next;
        if (ptp != NULL) {
          /* There is a second argument.  This is permitted for class operator
             delete() but not for global operator delete() (ARM 12.5). */
          if (class_type == NULL) {
            error_code = ec_too_many_args_for_operator;
          } else {
            /* The second argument must be of type size_t (ARM 12.5). */
            tp = ptp->type;
            if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
              if (!is_integral_type(tp) ||
                  skip_typerefs(tp)->variant.integer.int_kind !=
                                    (an_integer_kind)TARG_SIZE_T_INT_KIND) {
                pos_error(ec_bad_second_arg_type_for_operator_delete,
                          &locator->source_position);
                ptp->type = error_type();
                err = TRUE;
              }  /* if */
            }  /* if */
            /* More than two arguments are not allowed. */
            if (ptp->next != NULL) error_code = ec_too_many_args_for_operator;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Binary operator must have exactly two arguments. */
      if (param_count > 2) {
        error_code = ec_too_many_args_for_operator;
      } else if (param_count < 2) {
        error_code = ec_too_few_args_for_operator;
      }  /* if */
    }  /* if */
    if (error_code != ec_no_error) {
      pos_error(error_code, &locator->source_position);
      err = TRUE;
    }  /* if */
    /* Check return type. */
    if (opname == (an_opname_kind)onk_arrow) {
      /* For operator->() do a special check on the return type.  It must
         be something that can be used as a pointer -- either a pointer
         to a class or an object of or reference to a class for which
         operator->() is defined (ARM 13.4.6). */
      tp = rout_type->variant.routine.return_type;
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        a_boolean  local_err;
        if (is_pointer_type(tp)) {
          tp = type_pointed_to(tp);
          local_err = !is_class_struct_union_type(tp) &&
                      !is_or_contains_template_param(tp);
        } else {
          if (is_reference_type(tp)) tp = type_pointed_to(tp);
          if (is_or_contains_template_param(tp)) {
            local_err = FALSE;
          } else {
            local_err = (!is_class_struct_union_type(tp) ||
                         tp == class_type ||
                         opname_member_function_symbol(opname, tp) == NULL);
          }  /* if */
        }  /* if */
        if (local_err) {
          pos_error(ec_bad_return_type_for_op_arrow,
                    &locator->source_position);
          rout_type->variant.routine.return_type = error_type();
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (opname == (an_opname_kind)onk_new ||
        opname == (an_opname_kind)onk_delete) {
      tp = rout_type->variant.routine.return_type;
      if (!is_error_type(tp) && !is_or_contains_template_param(tp)) {
        if (opname == (an_opname_kind)onk_new) {
          if (!is_pointer_type(tp) || !is_void_type(type_pointed_to(tp))) {
            pos_error(ec_bad_return_type_for_op_new,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        } else {
          if (!is_void_type(tp)) {
            pos_error(ec_bad_return_type_for_op_delete,
                      &locator->source_position);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* If operator function is not a nonstatic member and does not have
         operands of class type or reference-to-class type, issue an error.
         This restriction does not apply to new and delete, however. */
      if (!is_nonstatic_member_function && !any_class_type_params) {
        pos_error(ec_no_args_with_class_type, &locator->source_position);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) set_to_error_locator(*locator);
  db_exit();
}  /* check_operator_function_params */


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


void add_throw_specification(a_func_info_block_ptr  func_info,
                             a_routine_ptr          rp)
/*
*/
{
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "add_throw_specification");
  if (exceptions_enabled) {
    if (func_info->throw_specification != NULL &&
        func_info->is_main_function) {
      /* main() cannot have a throw specification, since there's no call stack
         to unwind from main. */
      pos_warning(ec_throw_specification_not_allowed,
                  &func_info->throw_position);
    }  /* if */
    check_assertion(rp->type->kind == (a_type_kind)tk_routine);
    rtsp = rp->type->variant.routine.extra_info;
    check_assertion(rtsp->throw_specification == NULL);
    rtsp->throw_specification = func_info->throw_specification;
  }  /* if */
  db_exit();
}  /* if */


void check_throw_specification(a_func_info_block_ptr  func_info,
                               a_routine_ptr          rp)
/*
Check that the throw specification on the current declaration, if any, is
consistent with that of the previous declaration.
*/
{
  a_boolean                      match, any_difference_seen;
  a_throw_specification_ptr      new_throw_spec, old_throw_spec;
  a_throw_spec_type_ptr          new_tst_list, old_tst_list;
  a_throw_spec_type_ptr          tstp, other_tstp;
  a_symbol_ptr                   rout_sym;

  db_enter(4, "check_throw_specification");
  if (exceptions_enabled) {
    rout_sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
    old_throw_spec = rp->type->variant.routine.extra_info->throw_specification;
    new_throw_spec = func_info->throw_specification;
    if (new_throw_spec != NULL && func_info->is_main_function) {
      /* main() cannot have a throw specification, since there's no call stack
         to unwind from main. */
      pos_warning(ec_throw_specification_not_allowed,
                  &func_info->throw_position);
    }  /* if */
    if (old_throw_spec == NULL) {
      /* Previous specification asserted that any exception may be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_throw_spec != NULL) {
        pos_stsy_error(ec_incompatible_throw_specification,
                       &func_info->throw_position, "", rout_sym);
      }  /* if */
    } else if (new_throw_spec == NULL) {
      /* Issue an error on the omission of a throw specification on the current
         declaration (it must have been present on the previous one). */
      pos_sy_error(ec_omitted_throw_specification, &func_info->throw_position,
                   rout_sym);
    } else if (old_throw_spec->throw_spec_type_list == NULL) {
      /* Previous specification asserted that no exceptions will be thrown.
         It is compatible only with an identical specification on the current
         declaration. */
      if (new_throw_spec->throw_spec_type_list != NULL) {
        pos_stsy_start_error(ec_incompatible_throw_specification,
                             &func_info->throw_position, ":", rout_sym);
        add_diag_info(ec_previously_empty_throw_list);
        end_error();
      }  /* if */
    } else {
      /* Previous specification was a list of the types that will be thrown.
         Check for a mismatch between the previous list and the current one. */
      old_tst_list = old_throw_spec->throw_spec_type_list;
      new_tst_list = new_throw_spec->throw_spec_type_list;
      any_difference_seen = FALSE;
      /* First loop through the current list (if any) and issue a diagnostic
         on any type present in the current list but absent from the
         previous one. */
      for (tstp = new_tst_list; tstp != NULL; tstp = tstp->next) {
        if (tstp->redundant) {
          /* Don't bother looking for a match on redundant types.  It will
             already have been done. */
        } else {
          match = FALSE;
          other_tstp = old_tst_list;
          for (; other_tstp != NULL; other_tstp = other_tstp->next) {
            if (!other_tstp->redundant && other_tstp->type != NULL &&
                identical_types(tstp->type, other_tstp->type)) {
              /* An entry of the same type was found on the previous list. */
              match = TRUE;
              break;
            }  /* if */
          }  /* for */
          if (!match) {
            /* No match was found, so the previous list does not have a
               type that is on the current list. */
            if (!any_difference_seen) {
              /* The diagnostics will be combined with a header message
                 followed by additional messages identifying the specific
                 discrepancy.  This is the first diagnostic, so put out
                 the header message first. */
              pos_stsy_start_error(ec_incompatible_throw_specification,
                                   &func_info->throw_position, ":",
                                   rout_sym);
              any_difference_seen = TRUE;
            }  /* if */
            ty_add_diag_info(ec_previously_omitted_throw_type,
                             tstp->type);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Next loop through the previous list and issue a diagnostic on any
         type present in the previous list but absent from the current one
         (if any). */
      other_tstp = old_tst_list;
      for (; other_tstp != NULL; other_tstp = other_tstp->next) {
        if (other_tstp->redundant) {
          /* Don't bother looking for a match on redundant types.  It will
             already have been done. */
        } else {
          match = FALSE;
          for (tstp = new_tst_list; tstp != NULL; tstp = tstp->next) {
            if (!tstp->redundant &&
                other_tstp->type != NULL && tstp->type != NULL &&
                identical_types(tstp->type, other_tstp->type)) {
              match = TRUE;
              break;
            }  /* if */
          }  /* for */
          if (!match) {
            if (!any_difference_seen) {
              /* This is the first diagnostic, so put out the header
                 message first. */
              pos_stsy_start_error(ec_incompatible_throw_specification,
                                   &func_info->throw_position, ":",
                                   rout_sym);
               any_difference_seen = TRUE;
            }  /* if */
            ty_add_diag_info(ec_previously_included_throw_type,
                             other_tstp->type);
          }  /* if */
        }  /* if */
      }  /* for */
      if (any_difference_seen) end_error();
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_throw_specification */


static void scan_throw_specification(a_func_info_block_ptr  func_info)
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
specification is handled later (see check_throw_specification).
*/
{
  a_throw_specification_ptr  tsp;
  a_throw_spec_type_ptr      tstp, other_tstp, end_of_list = NULL;
  a_source_position          type_pos;
  a_stop_token_array         save_stop_token_array;

  db_enter(4, "scan_throw_specification");
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
    tsp = alloc_throw_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
    tsp->throw_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    func_info->throw_specification = tsp;
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
    tstp = alloc_throw_spec_type();
    type_pos = pos_curr_token;
    if (!is_decl_start(/*expr_context=*/FALSE,
                       /*real_declarator_allowed=*/FALSE) ||
        !is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                          /*real_declarator_allowed=*/FALSE)) {
      /* Error. */
      if (exceptions_enabled) pos_error(ec_exp_type_specifier, &type_pos);
      /* Flush tokens to the comma or right paren. */
      flush_tokens();
      tstp->type = error_type();
    } else {
      type_name(&tstp->type);
    }  /* if */
    if (exceptions_enabled) {
      /* Add tsp to the list. */
      if (end_of_list == NULL) {
        tsp->throw_spec_type_list = tstp;
      } else {
        /* Examine other entries already on the list to see if the current one
           is redundant. */
        other_tstp = tsp->throw_spec_type_list;
        for (; other_tstp != NULL; other_tstp = other_tstp->next) {
          if (!other_tstp->redundant &&
              identical_types(tstp->type, other_tstp->type)) {
            pos_remark(ec_redundant_throw_type, &type_pos);
            tstp->redundant = TRUE;
            break;
          }  /* if */
        }  /* for */
        end_of_list->next = tstp;
      }  /* if */
      end_of_list = tstp;
      if (!tstp->redundant && !is_error_type(tstp->type)) {
        /* Mark the type as having been used in an exception.  (Also, if it
           "contains" any classes, they are marked as requiring external
           linkage.) */
        set_used_in_exception_flag(tstp->type);
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
}  /* scan_throw_specification */


#if GENERATE_SOURCE_SEQUENCE_LISTS
#define init_param_source_sequence_sublist()                           \
  scope_stack[depth_innermost_file_scope_region_ss_list_scope].        \
                                           last_source_sequence_entry

static void terminate_param_source_sequence_sublist(
                                     a_func_info_block_ptr        func_info,
                                     a_source_sequence_entry_ptr  prev)
/*
*/
{
  if (prev != NULL) {
    func_info->prototype_scope_ss_entry_start = prev->next;
  } else {
    func_info->prototype_scope_ss_entry_start =
       scope_stack[depth_innermost_file_scope_region_ss_list_scope].
                                         il_scope->source_sequence_list;
  }  /* if */
  func_info->prototype_scope_ss_entry_end =
       scope_stack[depth_innermost_file_scope_region_ss_list_scope].
                                            last_source_sequence_entry;
#if DEBUG
  if (debug_level >= 4) {
    fputs("function prototype source sequence list (in file scope):\n",
          f_debug);
    if (func_info->prototype_scope_ss_entry_start == NULL) {
      fputs("  <empty list>\n", f_debug);
    } else {
      a_source_sequence_entry_ptr  tmp_prev, tmp_next;
      tmp_prev = func_info->prototype_scope_ss_entry_start->prev;
      func_info->prototype_scope_ss_entry_start->prev = NULL;
      tmp_next = func_info->prototype_scope_ss_entry_end->next;
      func_info->prototype_scope_ss_entry_end->next = NULL;
      db_source_sequence_list(func_info->prototype_scope_ss_entry_start);
      func_info->prototype_scope_ss_entry_start->prev = tmp_prev;
      func_info->prototype_scope_ss_entry_end->next = tmp_next;
    }  /* if */
  }  /* if */
#endif /* DEBUG */
}
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

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
  if (func_info == NULL) func_info = &local_func_info_block;
  clear_func_info(func_info);
#if 0
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (locator != NULL && !is_error_locator(*locator) &&
      func_info != &local_func_info_block) {
    func_info->declarator_ssep = add_source_sequence_entry_for_routine();
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* if 0 */
  last_param_id = NULL;
  *new_type_ptr = alloc_type((a_type_kind)tk_routine);
  extra_info = (*new_type_ptr)->variant.routine.extra_info;
  /* If a pragma indicating special argument checking appeared (e.g.,
     for printf args), remember that in the function type. */
  extra_info->arg_pragma = arg_pragma;
  if (is_constructor) extra_info->assoc_routine_is_ctor = TRUE;
  if (is_destructor) extra_info->assoc_routine_is_dtor = TRUE;
  arg_pragma = (an_arg_pragma_kind)apk_none;
  extra_info->param_type_list = NULL;
  if (curr_token == tok_rparen) {
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ f() is equivalent to f(void).  Leave param_type_list empty. */
      extra_info->prototyped = TRUE;
    } else {
      /* In C, f() is an old-style empty parameter list. */
      extra_info->prototyped = FALSE;
    }  /* if */
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
  } else {
    /* Determine whether this is an old-style list of identifiers or
       a prototyped parameter list. */
    if (member_function_parent_type != NULL) {
      /* This is a C++ member function so it must be prototyped. */
      extra_info->prototyped = TRUE;
    } else {
      /* Not a member function -- examine the first token. */
      extra_info->prototyped = is_prototyped_parameter_list_start();
    }  /* if */
    if (extra_info->prototyped) {
      /* ANSI function prototype, as in

         int f(int a, char *b)
               or
         int f(int, char *)

      */
      if (C_dialect == C_dialect_cplusplus) {
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
      last_param_type = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      ss_entry_start_prev = init_param_source_sequence_sublist();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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
          if (!is_error_type(param_type_ptr) && defines_something &&
              !(dso_flags & DSO_DECLARES_SOMETHING)) {
            /* Issue a warning on cases like this:
                 void f(struct { int i; });
               It will come up in C mode only, since the definition is already
               outlawed in C++. */
            diagnostic(strict_ansi_mode ?
                         strict_ansi_error_severity : es_warning,
                       ec_useless_decl);
          }  /* if */
          set_to_error_locator(param_locator);
        }  /* if */
        /* Adjust the type if necessary (for example, "array of x"
           becomes "pointer to x"). */
        adjust_parameter_type(&param_type_ptr);
        if (C_dialect == C_dialect_cplusplus &&
                   is_illegal_abstract_class_type(param_type_ptr)) {
          /* Abstract class may not be used as an arg type (ARM 10.3). */
          pos_error(ec_abstract_class_object_not_allowed,
                    is_error_locator(param_locator) ?
                           &param_type_pos : &param_locator.source_position);
        } else if (is_void_type(param_type_ptr) &&
                   C_dialect == C_dialect_cplusplus) {
          pos_error(ec_void_param_not_allowed, &param_type_pos);
          param_type_ptr = error_type();
        }  /* if */
        /* See if any type qualifiers were specified, and if they are
           okay. */
        check_type_qualifiers(&param_type_ptr);
        /* Standardize the storage class: unspecified becomes auto. */
        if (param_storage_class == (a_storage_class)sc_unspecified) {
          param_storage_class = (a_storage_class)sc_auto;
        }  /* if */
        /* Put the parameter type on the type list attached to the function
           type, and the name (if present) on the id list. */
        ptp = alloc_param_type(param_type_ptr);
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
            if (!cfront_compatibility_mode) {
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
          ptp->has_default_arg = default_arg_expr_allowed;
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
            (C_dialect != C_dialect_cplusplus &&
             curr_token == tok_ellipsis)) {
          /* A dangling type specifier is detected by decl_specifiers
             when a comma is omitted between the end of a type specifier
             and the start of the next.  This is pretty unlikely, but the
             mechanism was added for class declarations, where it is more
             useful. */
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
      terminate_param_source_sequence_sublist(func_info, ss_entry_start_prev);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Save the list of symbols for the prototype scope (usually NULL, but
         can have symbols for named types declared within the prototype). */
      if (func_info != &local_func_info_block) {
        /* Note that a pointer to the current entry of scope_stack is not saved
           from earlier in this routine because scope_stack might have been
           reallocated in the interim. */
        func_info->prototype_scope_symbols =
                                        scope_stack[depth_scope_stack].symbols;
      }  /* if */
      /* Pop the function prototype scope. */
      pop_scope();
    } else {
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
  }  /* if */
  if (func_info == &local_func_info_block) {
    if (local_func_info_block.param_id_list != NULL) {
      /* Free the list of parameter identifiers -- they're not needed
         if there's no definition. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_param_id_ptr  pip = local_func_info_block.param_id_list;
      for (; pip != NULL; pip = pip->next) {
        if (pip->source_sequence_entry != NULL) {
          remove_from_source_sequence_list(&pip->source_sequence_entry,
                                           (a_type_ptr)NULL);
        }  /* if */
      }  /* for */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      free_param_id_list(&(local_func_info_block.param_id_list));
    }  /* if */
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
    a_type_ptr  this_param_type = NULL;

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
        if (cfront_compatibility_mode) {
          /* Cfront 2.1 issues no diagnostic for a qualifier on a constructor
             or destructor. */
          pos_warning(ec_function_qualifier_not_allowed, &qualifier_pos);
        } else {
          qualifier_err = TRUE;
        }  /* if */
        this_param_type = member_function_parent_type;
      } else {
        this_param_type =
                      make_qualified_type(member_function_parent_type,
		                          dso_flags & DSO_CONST_QUALIFIED,
		                          dso_flags & DSO_VOLATILE_QUALIFIED);
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
      this_param_type = make_qualified_type(this_param_type, /*is_const=*/TRUE,
                                            /*is_volatile=*/FALSE);
      extra_info->implicit_this_param_type = this_param_type;
    }  /* if */
#if 0
    /* Should a diagnostic be issued if a throw specification appears other
       than on a top-level declaration? */
    if (curr_token == tok_throw && func_info == &local_func_info_block) {
      /* Error?  Warning? */
    }  /* if */
#endif /* if 0 */
    scan_throw_specification(func_info);
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* function_declarator */


static void array_declarator(a_type_ptr *new_type_ptr,
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


a_routine_ptr make_routine(a_type_ptr      type_ptr,
                           a_storage_class storage_class,
                           a_boolean       at_file_scope,
                           a_boolean       add_to_list)
/*
Allocate an entry for a routine with function type type_ptr and storage class
storage_class, and return a pointer to it.  The entry is allocated at the
file scope.  type_ptr must be in the file scope.  If add_to_list is TRUE,
add the new routine entry to the routines list.
*/
{
  a_routine_ptr          rp;
  a_memory_region_number region_to_switch_back_to;

  /* Always allocate routines at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  rp->type = type_ptr;
  rp->storage_class = storage_class;
  if (add_to_list) add_to_routines_list(rp, at_file_scope);
  switch_back_to_original_region(region_to_switch_back_to);
  return rp;
}  /* make_routine */


a_variable_ptr make_variable(a_type_ptr      type_ptr,
                             a_storage_class storage_class,
                             a_boolean       at_file_scope)
/*
Allocate an entry for a variable with type type_ptr and storage class
storage_class, and return a pointer to it.  Add the variable to the
file scope if at_file_scope is TRUE (in that case, type_ptr must be
in the file scope).
*/
{
  a_variable_ptr          vp;

  /* Allocate the variable at the file scope if requested.  Variables
     receiving static storage, even if they are not to go on the file scope
     variables list, should also be allocated in the file scope memory
     region. */
  vp = alloc_variable(storage_class);
  vp->type = type_ptr;
  add_to_variables_list(vp, at_file_scope);
  return vp;
}  /* make_variable */


static void make_anonymous_union_variable(a_type_ptr      anon_union_type,
                                          a_storage_class storage_class)
/*
Create a variable to represent an anonymous union.  Issue an error if its
storage class is invalid.  Also promote the fields of the union to the
current scope.
*/
{
  a_variable_ptr vp;
  a_boolean      at_file_scope = (decl_scope_level == DEPTH_OF_FILE_SCOPE);

  /* Check the storage class.  At file scope, only static is allowed. */
  if (at_file_scope) {
    switch (storage_class) {
      case sc_static:
        /* Okay. */
        break;
      case sc_extern:
      case sc_unspecified:
        /* Disallowed (ARM 9.5). */
        error(ec_anon_union_storage_class);
        storage_class = (a_storage_class)sc_static;
        break;
      default:
        /* Invalid for any variable at file scope. */
        error(ec_bad_file_scope_storage_class);
        storage_class = (a_storage_class)sc_static;
    }  /* switch */
  } else {
    /* Not at file scope. */
    switch (storage_class) {
      case sc_extern:
        /* Error, then default to automatic. */
        error(ec_anon_union_storage_class);
      case sc_unspecified:
        /* Default to automatic. */
        storage_class = (a_storage_class)sc_auto;
      case sc_static:
      case sc_auto:
      case sc_register:
        /* Okay. */
        break;
#if CHECKING
      default:
        internal_error("make_anonymous_union_variable: bad storage class");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* Allocate a variable to represent the anonymous union. */
  vp = make_variable(anon_union_type, storage_class, at_file_scope);
  /* Promote the fields of the anonymous union to the current scope, and do
     some error checking on the anonymous union's members. */
  check_anonymous_union_symbols((a_type_ptr)NULL, (a_field_ptr)NULL, vp);
}  /* make_anonymous_union_variable */


a_variable_ptr make_param_variable(a_type_ptr       type_ptr,
                                   a_storage_class  storage_class)
/*
Allocate a variable entry with type type_ptr.  If type_ptr is NULL (as it
will be in trying to creating an implicit this parameter for static member
functions) simply return NULL.
*/
{
  a_variable_ptr vp;

  check_assertion(type_ptr != NULL);
  vp = alloc_variable(storage_class);
  vp->type = type_ptr;
  vp->is_parameter = TRUE;
  return(vp);
}  /* make_param_variable */


a_variable_ptr make_parameter(a_type_ptr       type,
                              a_storage_class  storage_class,
                              a_symbol_ptr     sym)
/*
Allocate a parameter variable with the specified type and storage class
and return a pointer to it.  The parameter is linked to/from its associated
symbol sym.
*/
{
  a_variable_ptr vp;

  vp = make_param_variable(type, storage_class);
  /* sym will be NULL when the parameter is unnamed. */
  if (sym != NULL) {
    sym->variant.variable.ptr = vp;
    set_source_corresp(&(vp->source_corresp), sym);
    mark_variable_value_set(sym);
  }  /* if */
  add_to_parameters_list(vp);
  return(vp);
}  /* make_parameter */


void fixup_parameters(a_variable_ptr    param_list,
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


a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                a_symbol_locator *locator,
                                a_scope_depth    scope_level,
                                a_boolean        suppress_redecl_error)
/*

Enter a symbol declarative scope specified by scope_level.  kind indicates
the kind of symbol (e.g., a variable), and *locator is a locator for the
identifier.  Enter the symbol at the file scope if is_file_scope is TRUE.
If suppress_redecl_error is TRUE, suppress any error on a duplicate
declaration of this symbol.

*/
{
  a_symbol_ptr  sym;

  db_enter(4, "enter_local_symbol");
  if (scope_stack[scope_level].kind == (a_scope_kind)sck_func_prototype) {
    if (kind == (a_symbol_kind)sk_variable) {
      /* A variable declared in a function prototype scope is the result of
         an error in an old-style param list. */
    } else if (C_dialect == C_dialect_cplusplus) {
      /* We can't get here in C++ in a legal program.  If there was some
         sort of error, just go ahead and enter the symbol in the current
         scope. */
    } else {
      /* Any other declaration expected in a prototype scope is that of a
         type or an enumeration constant. */
      check_assertion(kind == (a_symbol_kind)sk_class_or_struct_tag ||
                      kind == (a_symbol_kind)sk_union_tag ||
                      kind == (a_symbol_kind)sk_enum_tag ||
                      kind == (a_symbol_kind)sk_type ||
                      kind == (a_symbol_kind)sk_constant);
      if (C_dialect == C_dialect_pcc) {
        /* In pcc a type declared in a parameter declaration belongs to the
           file scope.  For example:
               void f(a) struct s { int i; }; s a; { ... }
               struct s *aa;
           "s" refers to the same struct in both declarations. */
        scope_level = DEPTH_OF_FILE_SCOPE;
      } else {
        /* In C-mode a type declared in a parameter declaration is local to
           function.  Issue a warning on type declarations, since they will
           not be visible outside the function declaration.  For example:
               inf f(struct s a;);
               struct s {int b;};
           The first "struct s" is a different type than the second, which is
           probably not what was wanted. */
        if (kind != (a_symbol_kind)sk_constant &&
            !is_error_locator(*locator)) {
          pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  sym = enter_symbol(kind, locator, scope_level, suppress_redecl_error);
  db_exit();
  return(sym);
}  /* enter_local_symbol */


static a_boolean is_default_operator_new(a_symbol_locator *locator,
                                         a_type_ptr       type)
/*
Return TRUE if the locator is for an operator new() and the type indicates
that it is the default operator new() (i.e., if it has exactly one parameter,
which elsewhere is confirmed to have type size_t).
*/
{
  a_boolean         match = FALSE;
  a_param_type_ptr  ptp;

  if (locator->is_operator_name &&
      locator->variant.opname == (an_opname_kind)onk_new) {
    check_assertion(is_function_type(type));
    ptp = (skip_typerefs(type))->variant.routine.extra_info->param_type_list;
    if (ptp != NULL && ptp->next == NULL) {
      match = TRUE;
    }  /* if */
  }  /* if */
  return match;
}  /* is_default_operator_new */


static an_id_linkage_kind id_linkage(a_symbol_locator *locator,
                                     a_storage_class  *storage_class,
                                     a_type_ptr       type,
                                     a_boolean        is_main_function,
                                     a_symbol_ptr     *linked_symbol,
                                     a_symbol_ptr     *overload_symbol,
                                     a_scope_depth    *effective_decl_level)
/*
An identifier (specified by *locator) of type "type" and storage class
"storage class" is about to be declared at the current scope level.
Determine its linkage (see 3.1.2.2), and return it.  The linkage
determines how this declaration interacts with other (possibly tentative)
declarations of the same identifier.  Return in *linked_symbol a pointer
to any linked symbol (an identifier with the same name in the same scope,
if the new identifier has linkage).  This routine should not be called
for function parameters (they have no linkage); it will correctly handle
typedefs.  In C++, when the type is a function type and a previously
declared function in the same scope has a different type, we have a
candidate for function overloading; the *linked_symbol in this case will
be NULL, but we return in *overload_symbol a pointer to the symbol that
will be involved in overloading.
*/
{
  an_id_linkage_kind linkage;
  a_boolean          is_object, is_function, file_scope, decls_at_same_scope;
  a_boolean          is_list, is_friend_decl = FALSE;
  a_symbol_ptr       other_decl, sym, other_decl_saved;
  a_boolean          is_default_global_operator_new = FALSE;
  a_storage_class    local_storage_class = *storage_class;
  a_boolean          is_function_template_decl = FALSE;
  a_boolean          function_template_seen = FALSE;

  db_enter(3, "id_linkage");
  *linked_symbol = NULL;
  *overload_symbol = NULL;
  if (local_storage_class == (a_storage_class)sc_typedef) {
    /* A typedef is not an object or function, and has no linkage. */
    linkage = idl_none;
  } else if ((sym = locator->specific_symbol) != NULL &&
             sym->class_of_which_a_member != NULL) {
    /* Static data member. */
    check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
    *linked_symbol = sym;
    linkage = idl_external;
  } else {
    /* Is this type an object or function, and is it going to be declared
       at file scope? */
    is_function = is_function_type(type);
    is_object = !is_function;
    /* In pcc mode, functions and extern variables are always effectively
       declared at the file scope level.  In addition, because we accept
       static function declarations inside functions as an extension, we
       must promote static function declarations to file scope. */
    if ((C_dialect == C_dialect_pcc &&
         (is_function || local_storage_class == (a_storage_class)sc_extern)) ||
        (C_dialect != C_dialect_cplusplus &&
         (is_function && local_storage_class == (a_storage_class)sc_static))) {
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
    } else if (scope_stack[decl_scope_level].kind ==
                                     (a_scope_kind)sck_template_declaration) {
      is_function_template_decl = TRUE;
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
    } else if (C_dialect == C_dialect_cplusplus &&
               is_default_operator_new(locator, type)) {
      /* Default global operator new must always be entered at file scope. */
      *effective_decl_level = DEPTH_OF_FILE_SCOPE;
      is_default_global_operator_new = TRUE;
    } else {
      *effective_decl_level = decl_scope_level;
      while (scope_stack[*effective_decl_level].kind ==
                                       (a_scope_kind)sck_class_struct_union) {
        /* This must be a friend function declaration.  Enter the name at the
           level of the scope containing the class.  We allow for nested
           classes by popping out till we find a non-class scope. */
        (*effective_decl_level)--;
        is_friend_decl = TRUE;
        if (scope_stack[*effective_decl_level].kind ==
                                 (a_scope_kind)sck_template_instantiation) {
          *effective_decl_level = DEPTH_OF_FILE_SCOPE;
          break;
        }  /* if */
      }  /* while */
    }  /* if */
    file_scope = (*effective_decl_level == DEPTH_OF_FILE_SCOPE);
    if (is_error_locator(*locator)) {
      /* Symbol is compiler-generated as a result of an error, so there are
         no other declarations of the same symbol. */
      other_decl = NULL;
    } else {
      other_decl = locator->symbol_header->symbol;
      /* Look for any visible declaration of the identifier. */
      for (; other_decl != NULL; other_decl = other_decl->next) {
        if (name_space_for_symbol_kind[(int)other_decl->kind] == nsk_other) {
          /* Found one.  If it's not a variable or routine (say, if it's
             a typedef), pretend that there is no visible declaration. */
          if (other_decl->class_of_which_a_member != NULL) {
            /* This is a member of a class scope.  Ignore it and keep looking
               till a match in an enclosing scope is found. */
          } else if (is_default_global_operator_new &&
                     other_decl->decl_scope != FILE_SCOPE_NUMBER) {
            /* This is a non-default global operator new that was not
               declared at file scope.  Skip over it and look for a file
               scope symbol. */
          } else {
            if (other_decl->kind != (a_symbol_kind)sk_variable &&
                other_decl->kind != (a_symbol_kind)sk_routine &&
                other_decl->kind != (a_symbol_kind)sk_function_template &&
                other_decl->kind != (a_symbol_kind)sk_overloaded_function) {
              other_decl = NULL;
            }  /* if */
            break;
          }  /* if */
        }  /* if */
      }  /* for */
      if (other_decl != NULL) {
        /* A match was found in searching the symbol table.  However, in
           C++ we have to allow for function overloading.  If what we found
           was an sk_overloaded_function symbol, we need to look for a type
           match amongst the instances of the name.  Even if it was an
           sk_routine symbol, we may want to overload the two functions. */
        decls_at_same_scope = (other_decl->decl_scope ==
                                    scope_stack[*effective_decl_level].number);
        if (C_dialect == C_dialect_cplusplus && is_function &&
            other_decl->kind != (a_symbol_kind)sk_variable &&
            !is_main_function) {
          /* C++ function -- type compatibility check is required. */
          if (decls_at_same_scope) {
            /* *overload_symbol is set for cases in which the current symbol
               may be added to an overload list.  Note that overloading across
               scopes is not allowed.  Also, *overload_symbol may end up being
               cleared latter. */
            *overload_symbol = other_decl;
          }  /* if */
          if (other_decl->kind == (a_symbol_kind)sk_overloaded_function) {
            other_decl = other_decl->variant.overloaded_function.symbols;
            is_list = TRUE;
          } else {
            is_list = FALSE;
          }  /* if */
          other_decl_saved = other_decl;
          /* Go through the list of functions and look for type compatibility.
             If types_are_compatible returns TRUE, this is a redeclaration.
             If no type match is found, this is a candidate for overloading. */
          for (; other_decl != NULL;
                 other_decl = is_list ? other_decl->next : NULL) {
            a_type_ptr  tp;

            if (other_decl->kind == (a_symbol_kind)sk_function_template) {
              a_template_symbol_supplement_ptr  tssp;
              tssp = other_decl->variant.template_info;
              if (is_function_template_decl) {
                tp = tssp->variant.function.routine->type;
                if (types_are_compatible(tp, type)) {
                  *linked_symbol = other_decl;
                  *overload_symbol = NULL;
                  goto determine_linkage;
                }
              } else {
                /* There may a match involving an instance of this function
                   template, but we delay searching its list of instantiations
                   until all normally declared functions have been seen. */
                function_template_seen = TRUE;
              }  /* if */
            } else {
              if (is_function_template_decl) {
                /* No match. */
              } else {
                tp = routine_symbol_type(other_decl);
                if (types_are_compatible(tp, type)) {
                  /* Other_decl matches the current declaration.  Null out
                     *overload_symbol in case it was set. */
                  *overload_symbol = NULL;
                  break;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* for */
          if (other_decl == NULL && function_template_seen) {
            /* We didn't find a match, but there was at least one function
               template.  See if it either provides a match with an
               existing instance of the template or if a new instance can
               be created based on the current type. */
            for (other_decl = other_decl_saved;
                 other_decl != NULL;
                 other_decl = is_list ? other_decl->next : NULL) {
              if (other_decl->kind == (a_symbol_kind)sk_function_template) {
                /* Look for a match on the list of instantiations. */
                a_symbol_ptr sym;
                sym = matching_template_function(other_decl, type,
                                                 &locator->source_position);
                if (sym != NULL) {
                  /* Found a match. */
                  *linked_symbol = other_decl = sym;
                  if (sym->variant.routine.instance_ptr->specific_decl) {
                    *overload_symbol = NULL;
                  }  /* if */
                  goto determine_linkage;
                }  /* if */
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
        if (decls_at_same_scope || is_friend_decl) {
          /* The function symbol was located in the current scope. If there
             there was an exact type match of C++ functions, and in general
             otherwise, this is a redeclaration, and if other_decl has
             linkage we can return in *linked_symbol a pointer to the function
             or variable it represents. */
          if (other_decl != NULL) {
            if (is_function_symbol(other_decl)) {
              /* Functions always have linkage. */
              *linked_symbol = other_decl;
            } else if (other_decl->decl_scope == FILE_SCOPE_NUMBER ||
                       other_decl->variant.variable.ptr->storage_class ==
                                           (a_storage_class)sc_extern ||
                       other_decl->variant.variable.ptr->storage_class ==
                                           (a_storage_class)sc_unspecified) {
              /* Variables at file scope always have linkage.  Automatic,
                 register, and static variables in local scopes do not. */
              *linked_symbol = other_decl;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
determine_linkage:
    /* Determine the linkage. */
    if (!file_scope && local_storage_class != (a_storage_class)sc_extern) {
      /* A non-file-scope object without extern storage class has no
         linkage.  In C++ a non-file-scope function may be declared --
         a friend function defined inline within a local class; it too
         is given no linkage. */
      check_assertion(!is_function ||
                      (is_friend_decl &&
                       local_storage_class == (a_storage_class)sc_static));
      linkage = idl_none;
    } else if (file_scope &&
               local_storage_class == (a_storage_class)sc_static) {
      /* An object or function at file scope with static storage class
         has internal linkage. */
      linkage = idl_internal;
      /* File scope objects with internal linkage must have static storage
         class.  Sometimes an adjustment must be made on the storage class
         passed in, e.g.,
           static int i; extern int i;
         or
           static void f(); void f() { }
         The variable and function acquire static storage class from the prior
         declarations. */
      *storage_class = (a_storage_class)sc_static;
    } else if (local_storage_class == (a_storage_class)sc_extern ||
               (is_function &&
                local_storage_class == (a_storage_class)sc_unspecified)) {
      /* An object or function with extern storage class, or a function
         with no storage class, has the same linkage as any visible
         declaration of this identifier with file scope.  If there is
         no visible declaration, the identifier has external linkage. */
      if (other_decl != NULL && other_decl->decl_scope == FILE_SCOPE_NUMBER) {
        /* There is a declaration with file scope that is visible from
           here.  Set the flags to describe this identifier, and go
           retry the determination of the linkage. */
        file_scope = TRUE;
        switch (other_decl->kind) {
          case sk_routine:
            local_storage_class = other_decl->variant.routine.ptr->
                                                                storage_class;
            is_function = TRUE;
            break;
          case sk_variable:
            local_storage_class = other_decl->variant.variable.ptr->
                                                                storage_class;
            is_function = FALSE;
            break;
          case sk_function_template:
            local_storage_class = other_decl->variant.template_info->
                                      variant.function.routine->storage_class;
            is_function = TRUE;
            break;
#if CHECKING
          default:
            internal_error("id_linkage: bad kind for other_decl");
#endif /* CHECKING */
        }  /* switch */
        is_object = !is_function;
        /* If we check again for visible identifiers, there can be no
           other visible identifier with the same name. */
        other_decl = NULL;
        goto determine_linkage;
      }  /* if */
      /* No visible declaration found, so the linkage is external. */
      linkage = idl_external;
    } else if (is_object && file_scope &&
               local_storage_class == (a_storage_class)sc_unspecified) {
      /* An object at file scope with no storage class has external linkage. */
      linkage = idl_external;
#if ASM_FUNCTION_ALLOWED
    } else if (local_storage_class == (a_storage_class)sc_asm) {
      /* An asm function has internal linkage. */
      linkage = idl_internal;
#endif /* ASM_FUNCTION_ALLOWED */
#if CHECKING
    } else {
      /* There should not be any other cases. */
      internal_error("id_linkage: could not determine identifier linkage");
#endif /* CHECKING */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Linkage for %s is ",
                     is_error_locator(*locator) ?
                        "<error>" : locator->symbol_header->identifier);
    switch (linkage) {
      case idl_none:     fputs("none",     f_debug); break;
      case idl_internal: fputs("internal", f_debug); break;
      case idl_external: fputs("external", f_debug); break;
#if CHECKING
      default:      internal_error("id_linkage: bad id linkage determination");
#endif /* CHECKING */
    }  /* switch */
    putc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  if (linkage == idl_none) *linked_symbol = NULL;

  db_exit();
  return(linkage);
}  /* id_linkage */


a_boolean reconcile_external_symbol_types(
                             a_symbol_ptr          ext_sym,
                             a_source_position_ptr position,
                             a_type_ptr            type_ptr,
                             a_boolean             suppress_incompatible_error)
/*
Change the type of the external symbol ext_sym to type_ptr.  External
symbol entries are constructed for all variables and routines with
linkage (external or internal) as a place to keep the pointer to the
unique IL entry (needed because the normal symbol entries will not
necessarily stay in scope for the entire compilation).  *position
gives the source position to be used in case of error.
If suppress_incompatible_error is TRUE, do not issue an error about
a type incompatibility (presumably because the caller has already
issued a similar error).  Return FALSE if there is some error.
*/
{
  an_extern_symbol_descr_ptr esdp;
  a_type_ptr                 old_type;
  a_boolean                  okay = TRUE;

  esdp = ext_sym->variant.extern_symbol_descr;
  old_type = esdp->type;
  /* If the old and new types are the same, no checking or processing is
     required. */
  if (old_type != type_ptr) {
    if (!types_are_compatible(old_type, type_ptr)) {
      /* The old and new types are incompatible.  Error. */
      if (!suppress_incompatible_error) {
        pos_sy_error(ec_decl_incompatible_with_previous_use,
                     position, ext_sym);
      }  /* if */
      okay = FALSE;
      /* Record an error type as the external symbol's type, to avoid
         future errors. */
      esdp->type = error_type();
    } else {
      /* The old and new types are compatible.  Form the composite of
         those types, and save that as the type of the external symbol. */
      esdp->type = composite_type(old_type, type_ptr);
    }  /* if */
  }  /* if */
  return okay;
}  /* reconcile_external_symbol_types */


static a_symbol_ptr create_external_symbol_for_linked_entity(
                              a_symbol_locator     *locator,
                              a_boolean            is_function,
                              a_type_ptr           type_ptr,
                              a_name_linkage_kind  name_linkage,
                              a_boolean            redeclaration,
                              a_boolean            suppress_incompatible_error,
                              a_boolean            suppress_ext_sym_lookup,
                              a_variable_ptr       *variable_ptr,
                              a_routine_ptr        *routine_ptr)
/*
Find or create an external symbol entry for a variable or routine being
declared.  *locator gives the symbol locator for the identifier;
is_function is TRUE for a function, FALSE for a variable; type_ptr
gives the variable or routine type; linkage indicates the kind of
linkage the entity has (internal or external).  Aside from creating the
entry, this routine checks that the new declaration is compatible with
any previous linked declaration of the same name.  redeclaration is
TRUE if the present declaration is a redeclaration within the same scope.
suppress_incompatible_error is TRUE to suppress incompatibility errors
detected in this routine, presumably because the caller has already
issued a similar error.  If *variable_ptr and *routine_ptr are both NULL,
meaning no IL entity has been found for the entity, the appropriate one
of the two will be set to point to the IL entity attached to the external
symbol, if there is one.  Note that the pointer from the external symbol
entry to the IL variable or routine is not filled in if the entry is
created; the caller must set it.
*/
{
  a_symbol_ptr               ext_sym;
  an_extern_symbol_descr_ptr esdp;
  char                       *old_name, *new_name;
  a_symbol_kind              ext_sym_kind;
  a_symbol_locator           ext_locator;
  a_boolean                  err = FALSE;
  a_boolean                  use_existing_il_entry = FALSE;
  a_type_ptr                 preexisting_type;

  ext_sym_kind = is_function ? (a_symbol_kind)sk_extern_routine :
                               (a_symbol_kind)sk_extern_variable;
  if (suppress_ext_sym_lookup) {
    /* Ignore the presence of an external symbol with which the current
       symbol is compatible. */
    ext_sym = NULL;
    ext_locator = *locator;
    ext_locator.specific_symbol = NULL;
  } else {
    /* Look up the external name of the identifier (i.e., the name after
       any truncation, etc.). */
    ext_sym = find_external_symbol(locator, name_linkage,
                                   is_function ? type_ptr : NULL,
                                   &ext_locator);
  }  /* if */
  if (ext_sym != NULL) {
    /* There is an existing external symbol for the name. */
    esdp = ext_sym->variant.extern_symbol_descr;
    if (!redeclaration) {
      /* Check if the old entity has a different name than the new entity,
         which would indicate an error (two different names ended up mapping
         to the same external name). */
      if (ext_sym->kind == (a_symbol_kind)sk_extern_variable) {
        old_name = esdp->variant.variable->source_corresp.name;
      } else {
        old_name = esdp->variant.routine->source_corresp.name;
      }  /* if */
      check_assertion(old_name != NULL);
      new_name = locator->symbol_header->identifier;
      if (old_name != new_name && strcmp(old_name, new_name) != 0) {
        /* Two different names ended up mapping to the same external name.
           In the error, reference the old name in its original form. */
        pos_st_error(ec_external_name_clash, &locator->source_position,
                     old_name);
        err = TRUE;
        /* Force creation of a new external symbol. */
        ext_sym = NULL;
      }  /* if */
    }  /* if */
    if (!err) {
      if (ext_sym_kind != ext_sym->kind) {
        /* The old entity is a variable and the new one is a routine, or
           vice-versa; error. */
        if (!suppress_incompatible_error) {
          pos_sy_error(ec_decl_incompatible_with_previous_use,
                       &locator->source_position, ext_sym);
        }  /* if */
        err = TRUE;
        /* Force creation of a new external symbol. */
        ext_sym = NULL;
      } else {
        /* Both are variables, or both are routines.  Compare the old and new
           types; they must be compatible. */
        err = !reconcile_external_symbol_types(ext_sym,
                                               &locator->source_position,
                                               type_ptr,
                                               suppress_incompatible_error);
      }  /* if */
    }  /* if */
  }  /* if */
  if (ext_sym == NULL) {
    /* There is no (compatible) external symbol entry for the identifier.
       Create one. */
    ext_sym = enter_symbol(ext_sym_kind, &ext_locator, DEPTH_OF_FILE_SCOPE,
                           /*suppress_error=*/TRUE);
    esdp = ext_sym->variant.extern_symbol_descr;
    esdp->type = type_ptr;
    /* The pointer to the variable or routine IL entry is filled in later,
       by the caller of this routine. */
  }  /* if */
  if (!err) {
    /* If we do not already have an IL entry, and the external symbol entry
       points to one, reuse it. */
    /* Note that since err == FALSE we know that the external symbol and
       the new entity are both variables or both routines. */
    if (!is_function) {
      /* The entity being declared is a variable. */
      if (*variable_ptr == NULL) {
        *variable_ptr = ext_sym->variant.extern_symbol_descr->variant.variable;
        if (*variable_ptr != NULL) {
          /* There is a variable entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*variable_ptr)->type;
          (*variable_ptr)->type = type_ptr;
        }  /* if */
      }  /* if */
    } else {
      /* The entity being declared is a routine. */
      if (*routine_ptr == NULL) {
        *routine_ptr = ext_sym->variant.extern_symbol_descr->variant.routine;
        if (*routine_ptr != NULL) {
          /* There is a routine entry we can reuse. */
          use_existing_il_entry = TRUE;
          preexisting_type = (*routine_ptr)->type;
          (*routine_ptr)->type = type_ptr;
        }  /* if */
      }  /* if */
    }  /* if */
    if (use_existing_il_entry) {
      /* An existing IL entry can be reused. */
      /* See if the entry's type has been changed.  Note that we're checking
         for pointer equality here, so an equivalent but distinct type
         will fail to match.  One of the issues that deals with is routine
         types with associated routine pointers -- the associated routine
         pointer needs to be preserved. */
      if (type_ptr != preexisting_type) {
        /* The type has been changed.  See if the pre-existing type will
           have to be restored at the end of the current scope.  If so,
           create a fixup entry that will be processed by pop_scope.  The
           type must be restored in a case like
             int a[];
             main () {
               extern int a[5];
               ... Type of "a" is now "int [5]".
             }
             ... Type of "a" must be restored to "int []" at the end of "main".
        */
        /* The fixup is only needed if the pre-existing definition is in
           a scope that surrounds the current one.  That's hard to determine,
           since it's hard to know the scope associated with the previous
           definition (the associated symbol gives only one scope, and
           not necessarily the outermost).  A simple way out is to build
           the fixup whenever the current scope is not the file scope.
           That builds more fixups than needed, but it always works. */
        if (C_dialect == C_dialect_pcc) {
          /* In pcc mode all symbols with linkage are entered at the
             file scope, so a fixup is never needed. */
        } else if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
          /* Create a fixup entry, which will be processed at the end
             of the current scope (see pop_scope).  Note that the
             allocation routine places the new entry on the fixup list
             for the current scope. */
          an_extern_type_fixup_ptr etfp;
          etfp = alloc_etype_fixup();
          etfp->type = preexisting_type;
          etfp->is_routine = is_function;
          if (!is_function) {
            etfp->variant.variable = *variable_ptr;
          } else {
            etfp->variant.routine = *routine_ptr;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return ext_sym;
}  /* create_external_symbol_for_linked_entity */


static void check_for_linkage_conflict(a_storage_class    *old_storage_class,
                                       an_id_linkage_kind *linkage,
                                       a_storage_class    *storage_class,
                                       a_source_position  *position,
                                       a_boolean          suppress_diagnostic)
/*
A variable or routine is being declared again.  The existing storage class
of the entity is *old_storage_class.  The linkage and storage class of the
new declaration are given by *linkage and *storage_class.  Issue a diagnostic
(at the indicated position) if the old and new linkages conflict, and update
*linkage, *storage_class, and *old_storage_class appropriately.
*/
{
  if ((*linkage == idl_internal) !=
                          (*old_storage_class == (a_storage_class)sc_static)) {
    /* External versus internal linkage conflict. */
    if (!suppress_diagnostic) {
      /* There is a conflict between a prior declaration and the current one.
         This is clearly an error in C++ (ARM 7.1.1, 7.1.2), but because of
         prevailing practice we only issue a remark.  The same is done in
         C mode, partly because it is common practice in pcc. */
      pos_diagnostic((strict_ansi_mode ?
                           strict_ansi_error_severity : es_remark),
                     ec_linkage_conflict, position);
    }  /* if */
    /* If either declaration has unspecified storage class (i.e., it's an
       external definition), that takes precedence, and the entity should
       have unspecified storage class.  Otherwise, because of the test above,
       one or the other of the declarations will have static storage class,
       and the entity should have static storage class.  extern storage
       class just defers to the other storage class, i.e., it's considered a
       reference to something else that is not necessarily external.  That
       means, for example, that "extern int f();" followed by
       "static int f();" yields a static routine (that's how pcc does it,
       and it's undefined according to the standard). */
    if (*old_storage_class == (a_storage_class)sc_unspecified ||
        *storage_class == (a_storage_class)sc_unspecified) {
      *storage_class = (a_storage_class)sc_unspecified;
      *linkage = idl_external;
    } else {
      *storage_class = (a_storage_class)sc_static;
      *linkage = idl_internal;
    }  /* if */
    *old_storage_class = *storage_class;
  }  /* if */
}  /* check_for_linkage_conflict */


static void check_default_args(a_type_ptr  type)
/*
Given a routine type based on a current declaration, where there is no
prior declaration with which to merge it, look for the case in which a
parameter with a default argument is followed in the parameter list by
one without a default argument, and report the error.
*/
{
  a_param_type_ptr  ptp;

  /* Loop through the single list. */
  ptp = skip_typerefs(type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->has_default_arg && ptp->next != NULL &&
        !ptp->next->has_default_arg) {
      /* Current parameter has a default argument and its successor does
         not.  Report the error and break out of the loop. */
      error(ec_default_arg_not_at_end);
      break;
    }  /* if */
  }  /* for */
}  /* if */


static void check_default_arg_compatibility(a_type_ptr  orig_type,
                                            a_type_ptr  new_type)
/*
Given an existing routine type (orig_type) and the type based on a new
declaration (new_type), compare the default argument expressions on a
parameter-by-parameter basis and report any errors (see ARM 8.2.6).  The
merging of the default arguments occurs in composite_type.
*/
{
  a_boolean         not_at_end_of_list_error = FALSE;
  a_boolean         redecl_error = FALSE;
  a_boolean         default_arg_required = FALSE;
  a_param_type_ptr  ptp1, ptp2;

  /* Loop through the two lists in tandem.  We may assume that they are
     of equal length. */
  ptp1 = skip_typerefs(orig_type)->variant.routine.extra_info->param_type_list;
  ptp2 = skip_typerefs(new_type)->variant.routine.extra_info->param_type_list;
  for (; ptp1 != NULL; ptp1 = ptp1->next, ptp2 = ptp2->next) {
    if (ptp1->has_default_arg) {
      /* The parameter on the original type has a default arg. */
      if (ptp2->has_default_arg) {
        /* So does the parameter on the new type.  This is illegal. */
        redecl_error = TRUE;
      }  /* if */
      default_arg_required = TRUE;
    } else if (ptp2->has_default_arg) {
      default_arg_required = TRUE;
    } else if (default_arg_required) {
      not_at_end_of_list_error = TRUE;
    }  /* if */
  }  /* for */
  if (redecl_error) {
    error(ec_default_arg_already_defined);
  }  /* if */
  if (not_at_end_of_list_error) {
    error(ec_default_arg_not_at_end);
  }  /* if */
}  /* check_default_arg_compatibility */


void reconcile_routine_types(a_routine_ptr  routine_ptr,
                             a_type_ptr     type_ptr,
                             a_boolean      preserve_rout_type,
                             a_boolean      preserve_type_ptr)
/*
The routine routine_ptr has been given both the type it already has (i.e.,
routine_ptr->type) and the other type given by type_ptr; it may be assumed
that the two types are compatible.  Check the consistency of the default
argument specifications, if any, of the two types and set the routine type
to the composite of the two types.  If preserve_rout_type is TRUE,
routine_ptr->type must remain the same pointer; that type entry is
guaranteed to be unshared and is modified if necessary.  If
preserve_type_ptr is TRUE, routine_ptr->type must end up equal to type_ptr,
and type_ptr is guaranteed to be unshared and is modified if necessary.
Those flags are used when the associated type is part of a function
definition, and therefore already contains definition information like
assoc_routine which should not be overridden.  Obviously, both flags may
not be TRUE.
*/
{
  a_type_ptr        rout_type = routine_ptr->type;
  a_type_ptr        comp_type;
  a_param_type_ptr  rout_type_ptp, comp_type_ptp, next_rout_type_ptp;

  db_enter(4, "reconcile_routine_types");
  if (rout_type != type_ptr) {
    /* We only try to reconcile routine types that have already been
       determined to be compatible. */
    check_assertion(types_are_compatible(type_ptr, rout_type));
    /* We cannot be required to preserve the types from both sources. */
    check_assertion(!preserve_rout_type || !preserve_type_ptr);
    if (C_dialect == C_dialect_cplusplus) {
      /* If there are default arguments associated with the parameters, check
         them at this time.  They will be merged in composite types. */
      check_default_arg_compatibility(type_ptr, rout_type);
    }  /* if */
    /* The type of the routine should be the composite of the two types. */
    if (!preserve_rout_type && !preserve_type_ptr) {
      /* Simple case -- no required result type location. */
      routine_ptr->type = composite_type(type_ptr, rout_type);
    } else {
      /* Some requirement on where the result ends up.  Favor the type we'd
         like by passing it first to composite_type. */
      if (preserve_rout_type) {
        /* rout_type must be preserved. */
        comp_type = composite_type(rout_type, type_ptr);
      } else {
        /* type_ptr must be preserved. */
        comp_type = composite_type(type_ptr, rout_type);
        routine_ptr->type = rout_type = type_ptr;
      }  /* if */
      /* If rout_type is not what was returned, copy the composite
         type on top of the existing rout_type (it's guaranteed to be
         unshared). */
      if (comp_type != rout_type) {
        comp_type = skip_typerefs(comp_type);
        /* Transfer the composite type to rout_type, which is unshared.
           We want to preserve fields like assoc_routine and arg_pragma in
           rout_type, so we can't just do a copy_type. */
        rout_type->variant.routine.return_type =
                            comp_type->variant.routine.return_type;
        rout_type->variant.routine.extra_info->prototyped =
                            comp_type->variant.routine.extra_info->prototyped;
        if (rout_type->variant.routine.extra_info->param_type_list == NULL) {
          /* The entire list may just be transferred over. */
          rout_type->variant.routine.extra_info->param_type_list =
                    comp_type->variant.routine.extra_info->param_type_list;
        } else {
          /* Copy the param type entries from the composite type onto the
             param type entries for the routine type.  This is done in case
             new param type entries were created.  The original ones must be
             preserved, however, since they may be pointed to by the parameter
             variables with which they are associated. */
          rout_type_ptp =
                     rout_type->variant.routine.extra_info->param_type_list;
          comp_type_ptp =
                     comp_type->variant.routine.extra_info->param_type_list;
          for (; rout_type_ptp != NULL; rout_type_ptp = next_rout_type_ptp,
                                        comp_type_ptp = comp_type_ptp->next) {
            if (rout_type_ptp == comp_type_ptp) {
              /* Whenever the corresponding param type entries on the two
                 lists are the same entry, all subsequent ones will also be
                 the same, so we can bail out at that point. */
              break;
            }  /* if */
            /* Save the original next pointer and restore it after the copy. */
            next_rout_type_ptp = rout_type_ptp->next;
            *rout_type_ptp = *comp_type_ptp;
            rout_type_ptp->next = next_rout_type_ptp;
          }  /* for */
        }  /* if */
        /* has_ellipsis need not be copied -- it will be the same in all of
           the types, since the original two types are compatible. */
        /* Likewise, the implicit_this_param_type pointers should be identical
           -- this will have been verified in types_are_compatible. */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* reconcile_routine_types */


void decl_var_or_routine(a_symbol_locator      *locator,
                         a_storage_class       storage_class,
                         a_type_ptr            type_ptr,
                         a_func_info_block_ptr func_info,
                         a_source_sequence_entry_ptr
                                               declarator_ssep,
                         a_boolean             is_variable_def,
                         a_symbol_ptr          *symbol_ptr,
                         an_id_linkage_kind    *linkage_ptr,
                         a_type_ptr            *old_type,
                         a_symbol_ptr          *ext_sym)
/*
Enter the declaration of an identifier for a variable or routine.
*locator gives the symbol locator (and thus its name and its declaration
position).  storage_class and type_ptr give the storage class and type.
func_info will be non-NULL if and only if this is a function declaration.
If it is non-NULL then: if func_info->implicit_declaration is TRUE, this
declaration is for an implicit function declaration, and *symbol_ptr
already contains a pointer to the symbol entry, which is already in the
symbol table; if func_info->is_definition is TRUE, the identifier being
defined is part of a function definition (meaning there is a body in the
definition), in which case it is guaranteed that type_ptr points to an
unshared type entry, and that type entry will be preserved as the routine
type.  Create and enter a symbol entry, and return a pointer to it in
*symbol_ptr.  Also allocate any associated IL construct, and attach it to
the symbol.  If the identifier has linkage and there is an existing symbol
or IL entry, it will be re-used.  Return in *linkage_ptr the linkage of
the identifier.  Return in *old_type any previously-known type for this
identifier from a linked identifier in the same scope, or NULL if there
was no previously-known type.  If the identifier has linkage, return in
*ext_sym a pointer to the external symbol entry; otherwise, set *ext_sym
to NULL.  declarator_ssep (non-NULL only if source sequence entries are
being generated) is a pointer to the empty source sequence entry already
created for the declarator and added to the appropriate list; its kind
and entity pointer are updated.
*/
{
  a_symbol_ptr      sym = NULL;
  a_boolean         is_function;
  a_boolean         at_file_scope;
  a_symbol_ptr      linked_symbol, homonym_symbol, overload_symbol = NULL;
  a_boolean         redecl_error_already_issued = FALSE;
  a_boolean         linked_redecl_error = FALSE;
  a_boolean         old_decl_has_body = FALSE;
  a_boolean         redeclaration = FALSE;
  a_variable_ptr    variable_ptr = NULL;
  a_routine_ptr     routine_ptr = NULL;
  an_id_linkage_kind
                    linkage;
  a_source_correspondence
                    *source_corresp_ptr;
  a_scope_depth     effective_decl_level = decl_scope_level;
  a_boolean         template_function_specific_decl = FALSE;
  a_boolean         suppress_ext_sym_lookup = FALSE;
  a_boolean         is_main_function = FALSE;
  a_boolean         is_function_def = FALSE;
  a_boolean         changed_to_inline = FALSE;

  db_enter(3, "decl_var_or_routine");
  *old_type = NULL;
  is_function = is_function_type(type_ptr);
  check_assertion(is_function == (func_info != NULL));
  check_assertion(storage_class != (a_storage_class)sc_typedef);
  if (is_function) {
    if (func_info->is_main_function) is_main_function = TRUE;
    if (func_info->is_definition) is_function_def = TRUE;
    if (C_dialect == C_dialect_cplusplus) {
      if (func_info->is_inline) {
        check_assertion(storage_class == (a_storage_class)sc_unspecified ||
                        storage_class == (a_storage_class)sc_static);
        storage_class = (a_storage_class)sc_static;
      }  /* if */
      /* If this is an overloaded operator, check for errors in the
         argument list. */
      check_operator_function_params(type_ptr, /*class_type=*/(a_type_ptr)NULL,
                                     locator);
    }  /* if */
  }  /* if */
  if (is_function && func_info->is_implicit_declaration) {
    if (C_dialect != C_dialect_cplusplus) {
      /* For an implicit function, the identifier would not be in the process
         of being declared implicitly as a function if there were any visible
         declaration of it, and therefore it must have external linkage. */
      linkage = idl_external;
    } else {
      /* In C++ this is an error case.  Don't give this dummy routine any
         linkage. */
      linkage = idl_none;
    }  /* if */
    linked_symbol = NULL;
    homonym_symbol = NULL;
    sym = *symbol_ptr;
  } else {
    /* Determine the linkage of this symbol. */
    linkage = id_linkage(locator, &storage_class, type_ptr, is_main_function,
                         &linked_symbol, &homonym_symbol,
                         &effective_decl_level);
  }  /* if */
  /* at_file_scope will be TRUE if the IL variable or routine must be
     allocated in the file scope memory region.  This is always true
     of routines, and also true of variables with linkage. */
  at_file_scope = (is_function || linkage != idl_none);
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked. */
    if (is_function && linked_symbol->kind == (a_symbol_kind)sk_routine &&
        linked_symbol->variant.routine.instance_ptr != NULL &&
        !linked_symbol->variant.routine.instance_ptr->specific_decl) {
      /* This is not actually a redeclaration -- linked_symbol refers to a
         function template instantiation. */
      template_function_specific_decl = TRUE;
    } else {
      /* The new declaration must be compatible with the old. */
      redeclaration = TRUE;
    }  /* if */
  }  /* if */
  if (redeclaration) {
    if (linked_symbol->kind == (a_symbol_kind)sk_variable && !is_function) {
      if (C_dialect == C_dialect_cplusplus && linked_symbol->defined &&
          is_variable_def) {
        /* Variable has already been defined.  Issue an error here and
           suppress an error when the symbol is entered. */
        pos_sy_error(ec_already_defined, &locator->source_position,
                     linked_symbol);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
        /* Set a flag to suppress reuse of the existing external-variable
           symbol and of the variable already in use.  This is to avoid
           redundant errors in case both this and the previous definition
           involved initialization.  Also, to suppress a declared-but-not-used
           message, set the referenced flag in the linked symbol. */
        suppress_ext_sym_lookup = TRUE;
        linked_symbol->referenced = TRUE;
      } else {
        /* Linked symbol and new symbol are both variables.  See if they
           are compatible. */
        sym = linked_symbol;
        variable_ptr = linked_symbol->variant.variable.ptr;
        check_assertion(variable_ptr != NULL);
        *old_type = variable_ptr->type;
        if (!types_are_compatible(type_ptr, *old_type)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator->source_position, linked_symbol);
          redecl_error_already_issued = TRUE;
          linked_redecl_error = TRUE;
        } else {
          /* The type of the variable should be the composite of the two
             types. */
          variable_ptr->type = type_ptr = composite_type(type_ptr, *old_type);
        }  /* if */
      }  /* if */
    } else if (linked_symbol->kind == (a_symbol_kind)sk_routine &&
               is_function) {
      /* Linked symbol and new symbol are both routines.  The new declaration
         must be compatible with the old. */
      sym = linked_symbol;
      routine_ptr = linked_symbol->variant.routine.ptr;
      check_assertion(routine_ptr != NULL);
      if (routine_ptr->assoc_scope != NULL_region_number
#if ASM_FUNCTION_ALLOWED
          || routine_ptr->storage_class == (a_storage_class)sc_asm
#endif /* ASM_FUNCTION_ALLOWED */
                                                        ) {
        old_decl_has_body = TRUE;
      } else if (sym->defined) {
        /* In C++ the defined flag may have been set without the body having
           been scanned and bound to the routine yet (e.g., inline friend
           function). */
        check_assertion(scope_stack[decl_scope_level].kind ==
                                        (a_scope_kind)sck_class_struct_union);
        old_decl_has_body = TRUE;
      }  /* if */
      if (is_function_def && old_decl_has_body) {
        /* Previous routine already has a body, and new one does (or will)
           too. */
        pos_sy_error(ec_already_defined, &locator->source_position, sym);
        redecl_error_already_issued = TRUE;
        linked_redecl_error = TRUE;
      } else {
        /* If this is not C++ mode (for which this check has already been
           done in id_linkage), be sure that the old and new types are
           compatible.  Then form the composite type.  Note that there is a
           second test for compatibility after old-style parameter
           declarations are scanned, if this declaration has a body (see
           function_definition). */
        if ((C_dialect != C_dialect_cplusplus || is_main_function) &&
            !types_are_compatible(routine_ptr->type, type_ptr)) {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator->source_position, linked_symbol);
          redecl_error_already_issued = TRUE;
          if (!old_decl_has_body) {
            routine_ptr->type = type_ptr;
          } else {
            type_ptr = routine_ptr->type;
          }  /* if */
          linked_redecl_error = TRUE;
        } else {
          *old_type = routine_ptr->type;
          reconcile_routine_types(routine_ptr, type_ptr,
                                  /*preserve_rout_type=*/old_decl_has_body,
                                  /*preserve_type_ptr=*/is_function_def);
        }  /* if */
      }  /* if */
    } else {
      /* The linked symbol is a variable, while the new one is a routine,
         or vice-versa; error. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, linked_symbol);
      redecl_error_already_issued = TRUE;
      linked_redecl_error = TRUE;
    }  /* if */
  } else {
    /* Not a redeclaration. */
    if (is_function && C_dialect == C_dialect_cplusplus) {
      /* Be sure the default arguments, if any, are at the end of the
         parameters list. */
      check_default_args(type_ptr);
    }  /* if */
    if (homonym_symbol != NULL) {
      /* homonym_symbol is a previously declared routine symbol with the
         same name but a different type signature from that of the current
         declaration.  We may have an instance of function overloading. */
      an_error_code  error_code;

      if (homonym_symbol->kind != (a_symbol_kind)sk_overloaded_function &&
          homonym_symbol->kind != (a_symbol_kind)sk_function_template) {
        a_routine_ptr  rp = homonym_symbol->variant.routine.ptr;
        if (rp->special_kind == (a_special_function_kind)sfk_operator &&
            rp->opname_kind == (an_opname_kind)onk_delete) {
          /* Overloading is not allowed for operator delete() (ARM 12.5). */
          pos_error(ec_delete_already_declared, &locator->source_position);
          redecl_error_already_issued = TRUE;
          goto skip_overloading;
        }  /* if */
      }  /* if */
      if (homonym_symbol->kind != (a_symbol_kind)sk_function_template &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    /*new_is_template=*/FALSE, &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        redecl_error_already_issued = TRUE;
        /* We can't add a symbol to the overload list, so change to locator
           to an error locator to prevent hiding the overload symbol when the
           new symbol is entered. */
        set_to_error_locator(*locator);
        /* Don't treat this as a template function specific declaration even
           if it was previously thought to be.  Do treat it as a redeclaration
           error. */
        template_function_specific_decl = FALSE;
        linked_redecl_error = TRUE;
      } else if (template_function_specific_decl) {
        /* This is an explicit declaration of a template function.  Its symbol
           is already on the template's function instantiation list, but it
           needs to be added to the overload list as well, to assure that it
           will be found by the ordinary overload resolution algorithm. */
        sym = linked_symbol;
        overload_symbol = add_symbol_to_overload_list(sym, homonym_symbol);
        sym->variant.routine.instance_ptr->specific_decl = TRUE;
        if (is_function_def) {
          sym->variant.routine.instance_ptr->specific_def = TRUE;
          sym->variant.routine.ptr->specific_def = TRUE;
          sym->variant.routine.instance_ptr->instantiation_required = FALSE;
        }  /* if */
        routine_ptr = sym->variant.routine.ptr;
        old_decl_has_body = (routine_ptr->assoc_scope != NULL_region_number);
        *old_type = routine_ptr->type;
        reconcile_routine_types(routine_ptr, type_ptr,
                                /*preserve_rout_type=*/old_decl_has_body,
                                /*preserve_type_ptr=*/is_function_def);
      } else {
        /* Overloaded function.  Create the new symbol, which will be on the
           list of functions connected to an sk_overloaded symbol. */
        sym = enter_overloaded_symbol((a_symbol_kind)sk_routine, locator,
                                      homonym_symbol, &overload_symbol);
      }  /* if */
skip_overloading:;
    }  /* if */
  }  /* if */
  if (linked_redecl_error) {
    /* There is a linked symbol, but it is not compatible with the new
       declaration.  Force a new symbol and a new IL entry. */
    sym = NULL;
    linked_symbol = NULL;
    variable_ptr = NULL;
    routine_ptr = NULL;
    *old_type = NULL;
    redeclaration = FALSE;
  }  /* if */
  if (sym == NULL) {
    /* There is no (compatible) symbol, so enter one now. */
    sym = enter_local_symbol
                  ((a_symbol_kind)(is_function ? sk_routine : sk_variable),
                   locator, effective_decl_level, redecl_error_already_issued);
  } else if (is_function && func_info->is_implicit_declaration) {
    /* This is an implicit declaration of a function.  The symbol has
       already been entered and marked as declared. */
  } else {
    if (C_dialect == C_dialect_cplusplus && is_function &&
        routine_ptr != NULL) {
      /* Do compatibility checking on the throw specification and, if this
         is a definition, bind the throw specification to the routine entry.
         Note that if it is a definition the checking must be done before
         the routine's decl position is modified, to assure that the
         "original declaration line number" is displayed accurately. */
      check_throw_specification(func_info, routine_ptr);
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    if (!is_function && decl_scope_level == DEPTH_OF_FILE_SCOPE &&
        storage_class == (a_storage_class)sc_unspecified &&
        type_or_element_type_is_const_qualified(type_ptr)) {
      /* In C++ all const qualified objects at file scope with no explicit
         storage class are internally linked unless previously declared to
         be extern (ARM 7.1.1).  The storage class has been left "unspecified"
         because till now we didn't know whether this was a redeclaration. */
      if (variable_ptr == NULL ||
          variable_ptr->storage_class != (a_storage_class)sc_extern) {
        storage_class = (a_storage_class)sc_static;
        linkage = idl_internal;
      }  /* if */
    }  /* if */
  }  /* if */
  *ext_sym = NULL;
  if (linkage != idl_none) {
    /* The symbol has external or internal linkage.  Find or create an
       external symbol entry for the identifier name, to check that the
       current declaration is compatible with any previous and future
       declarations of the same name.  Note that while other declarations
       are required to be compatible with the present one (because all
       declarations of a name with linkage refer to the same object or
       function), no composite type is formed; the type of the IL entity
       is only what is known in the current scope.  The external symbol
       keeps track of the full composite type behind the scenes.
       If we do not already have an IL entry, and the external symbol entry
       points to one, get a pointer to it and use it. */
    /* Determine the name linkage that should be used in looking up an
       existing external symbol entry. */
    a_name_linkage_kind  name_linkage;

    if (linkage == idl_internal) {
      name_linkage = (a_name_linkage_kind)nlk_internal;
    } else if (C_dialect != C_dialect_cplusplus || is_main_function) {
      name_linkage = (a_name_linkage_kind)nlk_external;
    } else {
      name_linkage = def_external_linkage.kind;
    }  /* if */
    *ext_sym = create_external_symbol_for_linked_entity(
                                                 locator, is_function,
                                                 type_ptr, name_linkage,
                                                 redeclaration,
                                                 linked_redecl_error,
                                                 suppress_ext_sym_lookup,
                                                 &variable_ptr, &routine_ptr);
  }  /* if */
  if (!is_function) {
    /* The entity being declared is a variable. */
    if (variable_ptr == NULL) {
      /* There is no IL entry, so create one now.  If the variable has
         internal or external linkage, it is entered at the file scope. */
      variable_ptr = make_variable(type_ptr, storage_class, at_file_scope);
      source_corresp_ptr = &variable_ptr->source_corresp;
    } else {
      /* There is an existing IL entry that we are reusing. */
      /* Check for internal linkage on the old but not the new, or
         vice-versa. */
      check_for_linkage_conflict(&variable_ptr->storage_class,
                                 &linkage, &storage_class,
                                 &locator->source_position,
                                 /*suppress_diagnostic=*/linked_redecl_error);
      /* Modify the storage class if necessary (an unspecified storage 
         class on the new declaration indicates a tentative definition --
         see 3.7.2).  Do not force anything but sc_unspecified on the
         preexisting variable entry -- we don't want to change the storage
         class in a case like this:  int i; extern int i; . */
      if (storage_class == (a_storage_class)sc_unspecified) {
        variable_ptr->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      /* If the IL entry was previously referenced, the symbol should be
         marked as referenced too.  We may have a case like this:
           void f() { extern int i; i = 0; }
           int i;
         The IL entity associated with i is referenced in the function scope
         but the symbol at file scope is created later -- it should have its
         "referenced" flag set to prevent unwanted "defined but not referenced"
         warnings from being put out. */
      if (storage_class != (a_storage_class)sc_extern &&
          variable_ptr->source_corresp.referenced) {
        sym->referenced = TRUE;
      }  /* if */
      /* Similarly, it should have it's "used" flag set.  This is only needed
         for file-scope static variables, in cases like this:
           int f() { extern int i; return i; }
           static int i = 0;
         to avoid "set-but-never-used" diagnostics. */
      source_corresp_ptr = &variable_ptr->source_corresp;
      if (((a_symbol_ptr)source_corresp_ptr->assoc_info)->
                                                    variant.variable.used) {
        sym->variant.variable.used = TRUE;
      }  /* if */
    }  /* if */
    /* Link the symbol to the IL variable entry. */
    sym->variant.variable.ptr = variable_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL variable entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.variable = variable_ptr;
    }  /* if */
  } else {
    /* The entity being declared is a routine. */
    if (template_function_specific_decl && sym != linked_symbol) {
      /* This is a declaration of a template function at the local scope.
         A function instantiation entry with an associated symbol and routine
         entry already exist.  Be sure this local symbol is properly bound
         to the file-scope entities to which it corresponds. */
      check_assertion(linked_symbol != NULL &&
                      effective_decl_level != DEPTH_OF_FILE_SCOPE &&
                      (routine_ptr == NULL ||
                       routine_ptr == linked_symbol->variant.routine.ptr));
      sym->variant.routine.instance_ptr =
                                  linked_symbol->variant.routine.instance_ptr;
      routine_ptr = linked_symbol->variant.routine.ptr;
      sym->variant.routine.ptr = routine_ptr;
      *old_type = routine_ptr->type;
      reconcile_routine_types(routine_ptr, type_ptr,
                              /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE);
      /* Do compatibility checking for the throw specification. */
      check_throw_specification(func_info, routine_ptr);
    } else if (routine_ptr == NULL) {
      /* There is no IL entry, so create one now, and add it to the routine
         list of the file scope. */
      routine_ptr = make_routine(type_ptr, storage_class,
                                 /*at_file_scope=*/TRUE, /*add_to_list=*/TRUE);
      if (C_dialect == C_dialect_cplusplus) {
        /* Bind the throw specification to the routine entry. */
        add_throw_specification(func_info, routine_ptr);
        if (locator->is_operator_name) {
          routine_ptr->special_kind = (a_special_function_kind)sfk_operator;
          routine_ptr->opname_kind = locator->variant.opname;
        }  /* if */
      }  /* if */
    } else {
      /* There is an existing IL entry that we are reusing. */
      /* Check for internal linkage on the old but not the new, or
         vice-versa. */
#if ASM_FUNCTION_ALLOWED
      if (storage_class == (a_storage_class)sc_asm ||
          routine_ptr->storage_class == (a_storage_class)sc_asm) {
        /* asm functions have internal linkage but do not conflict
           with previous declarations that are either extern or static. */
          routine_ptr->storage_class = storage_class = (a_storage_class)sc_asm;
      } else {
#endif /* ASM_FUNCTION_ALLOWED */
        check_for_linkage_conflict(&routine_ptr->storage_class, &linkage,
                                   &storage_class, &locator->source_position,
                                  /*suppress_diagnostic=*/linked_redecl_error);
#if ASM_FUNCTION_ALLOWED
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      if (is_function_def) {
        a_boolean saved_referenced_flag;
        /* If this is a definition, unlink the routine entry and relink it
           at the end of the routines list, so that routines appear in the
           order that their bodies appear. */
        remove_from_routines_list(routine_ptr);
        add_to_routines_list(routine_ptr, /*at_file_scope=*/TRUE);
        /* Put in the storage class for the definition (static or 
           unspecified). */
        routine_ptr->storage_class = storage_class;
        /* If the IL entry was previously referenced, the symbol should
           be considered to have been referenced as well. */
        saved_referenced_flag = routine_ptr->source_corresp.referenced;
        if (saved_referenced_flag) sym->referenced = TRUE;
        /* Reset the source correspondence to the definition symbol. */
        set_source_corresp(&routine_ptr->source_corresp, sym);
        /* Keep an indication of any references so far (the referenced
           flag is reset by the set_source_corresp call). */
        routine_ptr->source_corresp.referenced = saved_referenced_flag;
        if (routine_ptr->compiler_generated) {
          /* This is an entry for a compiler generated ::operator new or
             ::operator delete.  It was created during initialization, but
             is overridden by the present declaration. */
          check_assertion(routine_ptr->special_kind ==
                                (a_special_function_kind)sfk_operator &&
                          (routine_ptr->opname_kind ==
                                                 (an_opname_kind)onk_new ||
                           routine_ptr->opname_kind ==
                                                 (an_opname_kind)onk_delete));
          routine_ptr->compiler_generated = FALSE;
        }  /* if */
      }  /* if */
      changed_to_inline = (func_info->is_inline && !routine_ptr->is_inline);
    }  /* if */
    if (func_info->is_inline) routine_ptr->is_inline = TRUE;
    source_corresp_ptr = &routine_ptr->source_corresp;
    /* Link the symbol to the IL routine entry. */
    sym->variant.routine.ptr = routine_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL routine entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.routine = routine_ptr;
    }  /* if */
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    /* There is no symbol pointed to from the variable or routine, so
       update it with the current symbol. */
    set_source_corresp(source_corresp_ptr, sym);
  } else if (!redeclaration && !template_function_specific_decl) {
    /* Record a reference to the outer-scope symbol of the same name,
       but do not set the IL entity referenced flag. */
    reference_to_symbol(SRK_REFERENCE,
                        (a_symbol_ptr)source_corresp_ptr->assoc_info,
                        &locator->source_position, /*update_il_entry=*/FALSE);
  }  /* if */
  if (changed_to_inline) {
    if (routine_ptr->called) {
      pos_sy_error(ec_called_function_redeclared_inline,
                   &locator->source_position, sym);
    }  /* if */
  }  /* if */
  if (linkage == idl_external) {
    /* Indicate in the IL entry that the name is externally visible by
       assigning the external linkage kind that is the default for the current
       context. */
    if (C_dialect != C_dialect_cplusplus || is_main_function) {
      /* Note that "main" is always given "C" linkage. */
      source_corresp_ptr->name_linkage = (a_name_linkage_kind)nlk_external;
      sym->explicit_linkage_specifier = FALSE;
    } else if (source_corresp_ptr->name_linkage ==
                                         (a_name_linkage_kind)nlk_none) {
      /* No prior declaration, so there's no conflict. */
      source_corresp_ptr->name_linkage = def_external_linkage.kind;
      sym->explicit_linkage_specifier = 
                              (*ext_sym)->explicit_linkage_specifier = 
                                         def_external_linkage.is_explicit;
      if (overload_symbol != NULL &&
          def_external_linkage.kind == (a_name_linkage_kind)nlk_external) {
        /* "At most one of a set of overloaded functions . . . can have
           C linkage" (ARM 7.4).  Search for conflicts. */
        a_symbol_ptr  sp;
        for (sp = overload_symbol->variant.overloaded_function.symbols;
             sp != NULL;
             sp = sp->next) {
          if (sp != sym &&
              sp->variant.routine.ptr->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external) {
            pos_sy_error(ec_overloaded_function_linkage,
                         &locator->source_position, overload_symbol);
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* Multiple specifications of external linkage must be the same
         (ARM 7.4).  But it's a little trickier than that.  We will not
         override the previous specification, but we need to be sure the
         current one is consistent with it. */
      a_boolean  err = FALSE;
      if (source_corresp_ptr->name_linkage == def_external_linkage.kind) {
        /* The linkage kinds (C or C++) are the same; however, the ARM states,
           "A function declaration without a linkage specification may not
           precede the first linkage specification for that function." */
        if (def_external_linkage.is_explicit) {
          err = (!sym->explicit_linkage_specifier &&
                 !(*ext_sym)->explicit_linkage_specifier);
          /* Mark the symbols as having an explicit linkage specifier to
             keep this error from occurring again later. */
          sym->explicit_linkage_specifier = 
                              (*ext_sym)->explicit_linkage_specifier = TRUE;
        }  /* if */
      } else {
        /* Linkage is not the same, but it's no error as long as the current
           specification is implicit. */
        err = def_external_linkage.is_explicit;
      }  /* if */
      if (err) {
        /* The ARM specifies that inconsistencies are errors for functions but
           not for variables.  Just issue a remark or, in strict ansi mode,
           a warning in the latter case. */
        pos_sy_diagnostic(is_function ?
                            (an_error_severity)es_error :
                            (strict_ansi_mode ?
                               (an_error_severity)es_warning :
                               (an_error_severity)es_remark),
                          ec_incompatible_linkage_specifier,
                          &locator->source_position, *ext_sym);
      }  /* if */
    }  /* if */
  } else if (linkage == idl_internal) {
    /* Internal linkage. */
    source_corresp_ptr->name_linkage = (a_name_linkage_kind)nlk_internal;
  } else {
    /* No linkage -- e.g., an automatic variable. */
    check_assertion(source_corresp_ptr->name_linkage ==
                                               (a_name_linkage_kind)nlk_none);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* A variable or routine with linkage should not be declared in terms of
       a local type. */
    if (source_corresp_ptr->name_linkage != (a_name_linkage_kind)nlk_none) {
      if (is_or_contains_local_type(type_ptr)) {
        pos_warning(is_function ? ec_local_type_in_function :
                                  ec_local_type_in_nonlocal_var,
                    &locator->source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If cross-reference information is being issued, update the output.  If
     source sequence entries are being generated, update the declarator_ssep
     entry. */
  if (is_variable_def || is_function_def) {
    /* Also set the the defined flag in the symbol and update the source
       position in the IL entity. */
    f_mark_defined(sym, &locator->source_position, declarator_ssep);
  } else {
    f_mark_declared(sym, &locator->source_position, declarator_ssep);
  }  /* if */
  if (!is_function && is_volatile_qualified_type(type_ptr)) {
    /* A variable with a volatile type is considered to be used and modified
       from "elsewhere".  Note that this must be done after set_source_corresp
       because the latter clears the IL referenced flag. */
    source_corresp_ptr->referenced = TRUE;
    sym->referenced = TRUE;
    sym->variant.variable.used = TRUE;
    sym->variant.variable.value_has_been_set = TRUE;
  }  /* if */
  /* Return symbol and linkage pointers. */
  *symbol_ptr = sym;
  *linkage_ptr = linkage;

#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_var_or_routine */


void decl_function_template(a_symbol_locator    *locator,
                            a_type_ptr          type_ptr,
                            a_func_info_block   *func_info,
                            a_symbol_ptr        *symbol_ptr,
                            a_storage_class     storage_class)
/*
Roughly speaking, this routine does for function templates what
decl_var_or_routine does for ordinary functions.  Lookup and reuse or else
create a function template symbol; for new symbols also create a routine
entry (though one that is not added to the IL).  *locator represents the
current identifier, type_ptr is the function type, storage_class is the
storage class, if any, specified in the declaration, and is_inline is TRUE
if "inline" was specified in the declaration.  The function template may
be part of an overload set, it may have been previously declared (but not
defined), and it may be an out-of-line definition of a member function of a
class template.
*/
{
  a_scope_depth                     effective_decl_level;
  a_symbol_ptr                      sym = NULL;
  a_symbol_ptr                      overload_symbol = NULL, homonym_symbol;
  a_template_symbol_supplement_ptr  tssp;
  a_routine_ptr                     rout_ptr;
  a_memory_region_number            region_to_switch_back_to;
  a_boolean                         changed_to_inline = FALSE;

  db_enter(3, "decl_function_template");
  if (func_info->is_inline) {
    storage_class = (a_storage_class)sc_static;
  } else if (storage_class == (a_storage_class)sc_unspecified) {
    /* Default. */
    storage_class = (a_storage_class)sc_extern;
  }  /* if */
  effective_decl_level = DEPTH_OF_FILE_SCOPE;
  if (locator->is_qualified_name && locator->specific_symbol != NULL) {
    /* Member function template. */
    sym = locator->specific_symbol;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (sym->kind != (a_symbol_kind)sk_member_function &&
               sym->kind != (a_symbol_kind)sk_overloaded_function) {
      /* We must have nonfunction class member.  This is an error, so set sym
         to NULL to force the creation of a fake member function symbol. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
      sym = NULL;
      set_to_error_locator(*locator);
    } else {
      /* Look for a member function symbol of this type in the symbol table.
         It is an error if it is  not already there. */
      sym = member_function_redecl_sym(sym, type_ptr);
      if (sym == NULL) {
        /* No member function with a matching type was found.  Issue an
           error. */
        pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_overloaded_function_incompatible_type :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
        set_to_error_locator(*locator);
      } else {
        /* This is a member function symbol of a prototype instantiation.
           Get the associated function template. */
        sym = get_member_function_template_symbol(sym);
      }  /* if */
    }  /* if */
  } else if (!is_error_locator(*locator)) {
    a_symbol_header_ptr  hdr = locator->symbol_header;
    if (hdr->identifier != NULL &&
        (strcmp(hdr->identifier, "main") == 0)) {
      /* A function template named "main" is not allowed.  (This prohibition
         is not explicit in the ARM, but it makes sense, since a function
         named "main" cannot be called (ARM 3.4). */
      pos_error(ec_function_template_named_main, &locator->source_position);
      set_to_error_locator(*locator);
    } else if (locator->is_operator_name) {
      if (locator->variant.opname == (an_opname_kind)onk_delete) {
        /* A template definition of operator delete is not allowed.  This
           is inferred from the ARM prohibition against overloading
           operator delete. */
        pos_error(ec_template_operator_delete, &locator->source_position);
        set_to_error_locator(*locator);
      } else if (is_default_operator_new(locator, type_ptr)) {
        /* Overloading should not be allowed on the single-argument
           version of operator new(size_t), though it is not expressly
           prohibited.  At least one C++ test suite expects an error. */
        pos_error(ec_template_operator_new, &locator->source_position);
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* id_linkage will set sym to point to an existing symbol when we have
       a redeclaration of a function template. */
    (void)id_linkage(locator, &storage_class, type_ptr,
                     /*is_main_function=*/FALSE, &sym, &homonym_symbol,
                     &effective_decl_level);
    if (sym == NULL) {
      /* Not a redeclaration. */
      an_error_code error_code;

      check_default_args(type_ptr);
      if (homonym_symbol != NULL &&
          !overload_distinguishable(homonym_symbol, type_ptr,
                                    /*new_is_template=*/TRUE, &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        set_to_error_locator(*locator);
        /* Avoid overloading. */
        homonym_symbol = NULL;
      }  /* if */
      if (homonym_symbol != NULL) {
        /* Another function with the same name has been declared already.  It
           may or may not be a function template.  In any case, create a new
           symbol and add it to an overload list. */
        sym = enter_overloaded_symbol((a_symbol_kind)sk_function_template,
                                      locator, homonym_symbol,
                                      &overload_symbol);
      } else {
        /* No overloading.  Simply create a new symbol. */
        sym = enter_local_symbol((a_symbol_kind)sk_function_template, locator,
                                 DEPTH_OF_FILE_SCOPE,
                                 /*suppress_redecl_error=*/FALSE);
      }  /* if */
    } else {
      check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
      /* Merge type information from the two declarations. */
      reconcile_routine_types(sym->variant.template_info->
						variant.function.routine,
			       type_ptr,
                              /*preserve_rout_type=*/TRUE,
                              /*preserve_type_ptr=*/FALSE);
    }  /* if */
  }  /* if */
  tssp = template_supplement_for_symbol(sym);
  rout_ptr = tssp->variant.function.routine;
  /* A routine entry is created for the function template, but it is not
     entered in the IL.  It is a convenient place to keep track of prototype
     information: type, storage class, etc.  These values may be reused
     when the template is instantiated.  This routine entry will not, of
     course, have a body associated with it. */
  if (rout_ptr == NULL) {
    switch_to_file_scope_region(&region_to_switch_back_to);
    tssp->variant.function.routine = rout_ptr = alloc_routine();
    switch_back_to_original_region(region_to_switch_back_to);
    rout_ptr->type = type_ptr;
    rout_ptr->storage_class = storage_class;
    rout_ptr->is_inline = func_info->is_inline;
    if (locator->is_operator_name) {
      rout_ptr->special_kind = (a_special_function_kind)sfk_operator;
      rout_ptr->opname_kind = locator->variant.opname;
    }  /* if */
    check_assertion(!locator->is_conversion_name);
    set_source_corresp(&rout_ptr->source_corresp, sym);
    rout_ptr->source_corresp.name_linkage =
                          (storage_class == (a_storage_class)sc_extern) ?
                                (a_name_linkage_kind)nlk_cplusplus_external :
                                (a_name_linkage_kind)nlk_internal;
    /* Bind the throw specification to the routine entry's type. */
    add_throw_specification(func_info, rout_ptr);
  } else {
    if (func_info->is_inline) {
      if (!rout_ptr->is_inline) {
        rout_ptr->is_inline = TRUE;
        changed_to_inline = TRUE;
      }  /* if */
    }  /* if */
    /* Be sure the current throw specification is consistent with the one
       on the previous declaration. */
    check_throw_specification(func_info, rout_ptr);
  }  /* if */
  if (overload_symbol != NULL) {
    /* A new symbol was added to an overload list which may have included
       functions that were specific declarations of the current template.
       For instance,
         void f(int i) {  ... }
         template <class T> void f(T t) { ... }
       Here the first declaration of f turns out to be a specific declaration
       of the template named f, even though the template is declared after the
       instance.  We need to go back over the overload list and associate a
       function instantiation entry with each routine that can in retrospect
       be recognized as a specific declaration of the function template. */
    a_symbol_ptr  rout_sym;
    for (rout_sym = overload_symbol->variant.overloaded_function.symbols;
         rout_sym != NULL;
         rout_sym = rout_sym->next) {
      if (rout_sym->kind == (a_symbol_kind)sk_routine) {
        /* Determine whether rout_sym is a specialization of the function
           template represented by sym. */
        record_predeclared_template_function(sym, rout_sym);
      }  /* if */
    }  /* for */
  } else if (changed_to_inline) {
    /* An existing template function has been redeclared and this time it's
       inline.  Be sure that "inline" and storage class are propagated
       through the instances. */
    a_template_instance_ptr  tip = tssp->variant.function.instantiations;
    for (; tip != NULL; tip = tip->next) {
      a_routine_ptr  rp = tip->instance_sym->variant.routine.ptr;
      if (tip->specific_def) {
#if 0
        /* Must the inline setting of a specific definition of a function
           template be consistent with that of the template? */
#endif /* if 0 */
      } else {
        if (rp->storage_class != (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict,
                      tip->instance_sym);
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
        }  /* if */
        /* Issue a diagnostic is the function has already been called. */
        if (!rp->is_inline && rp->called) {
          sym_error(ec_called_function_redeclared_inline,
                    tip->instance_sym);
        }  /* if */
        rp->is_inline = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Return the function template symbol. */
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_function_template */


static void define_static_data_member(a_symbol_locator   *locator,
                                      a_storage_class	 storage_class,
				      a_type_ptr	 type_ptr,
                                      a_source_sequence_entry_ptr  ssep,
				      a_symbol_ptr       *symbol_ptr,
                                      an_id_linkage_kind *linkage_ptr)
/*
Enter the definition of a static data member.  *locator gives the symbol
locator (and thus its name and its declaration position).  storage_class and
type_ptr give the storage class and type of the current definition.
Note that static data members must already have been declared within the
class (or struct or union) of which they are members.  The type must be
compatible with the original declaration, and there must be no explicit
storage class on the current definition.  The storage class of defined
static members should be changed to sc_unspecified.  Return a pointer to
the symbol and its linkage (which is always "none").
*/
{
  a_variable_ptr	var;
  a_boolean		err = FALSE;
  a_symbol_ptr		sym;

  db_enter(3, "define_static_data_member");
  /* This routine is called after a qualified name has been seen, but be sure
     the object is a static data member.  (In invalid programs it could also
     be the name of a nonstatic data member or a member function.) */
  sym = locator->specific_symbol;
  /* A storage class of sc_unspecified means "no storage class explicitly
     specified" -- anything else is an error. */
  if (storage_class != (a_storage_class)sc_unspecified) {
    pos_error(ec_storage_class_not_allowed, &locator->source_position);
  }  /* if */
  if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    var = sym->variant.static_data_member.variable;
    check_assertion(var->storage_class == (a_storage_class)sc_static);
    if (sym->defined) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
      err = TRUE;
    } else if (!types_are_compatible(type_ptr, var->type)) {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
      err = TRUE;
    } else {
      /* The type of the variable should be the composite of the two types. */
      var->type = composite_type(type_ptr, var->type);
      /* Set the IL referenced flag since, as an externally visible variable,
         it could be referenced from another translation unit. */
      var->source_corresp.referenced = TRUE;
      /* If this is a member of an instantiation of a class
         template, set the specific_def flag in the instance entry. */
      if (sym->variant.static_data_member.instance_ptr != NULL) {
        sym->variant.static_data_member.instance_ptr->specific_def = TRUE;
        sym->variant.static_data_member.variable->specific_def = TRUE;
      }  /* if */
      f_mark_defined(sym, &locator->source_position, ssep);
    }  /* if */
  } else {
    /* Not a static data member (but a member of some sort, since it is a
       qualified name).  Issue the appropriate error. */
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* Nonstatic data members (fields) cannot be defined. */
      pos_error(ec_nonstatic_member_def_not_allowed,
                &locator->source_position);
    } else if (is_member_function_symbol(sym)) {
      /* A member function -- this is treated as a type incompatibility. */
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else if (sym->kind != (a_symbol_kind)sk_undefined &&
               !is_error_locator(*locator)) {
      pos_sy_error(ec_already_defined, &locator->source_position, sym);
    }  /* if */
    err = TRUE;
  }  /* if */
  if (err) {
    /* An error occurred which prevents using the object specified as
       target of any initialization that may follow.  Create a dummy
       variable with an error type (to suppress semantic errors on the
       initialization, if any). */
    a_type_ptr           tp = sym->class_of_which_a_member;
    a_symbol_header_ptr  hdr = locator->symbol_header;

    /* Record the symbol declaration, using the original symbol, even
       though there was an error.  This will make it show up on a cross
       reference listing. */
    f_mark_declared(sym, &locator->source_position, ssep);
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_symbol((a_symbol_kind)sk_static_data_member,
                       locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    sym->variant.static_data_member.variable =
               make_variable(error_type(), (a_storage_class)sc_static,
                             /*at_file_scope=*/TRUE);
    /* Make the error symbol have a class_of_which_a_member field, since
       it is expected on sk_static_data_member fields downstream. */
    sym->class_of_which_a_member = tp;
  }  /* if */
  *linkage_ptr = idl_none;
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_static_data_member */


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
        a_boolean  const_okay, volatile_okay;
        if (is_copy_constructor(rp, class_type, &const_okay,
                                &volatile_okay)) {
          /* This is a copy constructor.  Note that the presence of a user-
             defined copy constructor means that construction by bitwise
             copying is not done. */
          cssp->has_copy_constructor = TRUE;
          cssp->has_copy_constructor_for_const_object = const_okay;
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Do compatibility checking on the throw specification and bind the
       throw specification to the routine entry.  Note that the checking must
       be done before the routine's decl position is modified, to assure that
       the "original declaration line number" is displayed accurately. */
    check_throw_specification(func_info, rp);
    copy_source_position(locator->source_position,
                         rp->source_corresp.decl_position);
    /* If this is an member function of an instantiation of a class
       template, set the specific_def flag in the instance entry. */
    if (sym->variant.routine.instance_ptr != NULL) {
      sym->variant.routine.instance_ptr->specific_def = TRUE;
      sym->variant.routine.ptr->specific_def = TRUE;
      sym->variant.routine.instance_ptr->instantiation_required = FALSE;
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  f_mark_defined(sym, &locator->source_position, func_info->declarator_ssep);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
  mark_defined(sym, &locator->source_position);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
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


void decl_typedef(a_symbol_locator             *locator,
                  a_type_ptr                   type_ptr,
                  a_symbol_ptr                 *symbol_ptr,
                  a_source_sequence_entry_ptr  declarator_ssep)
/*
Enter the declaration of an identifier for a typedef.  *locator gives
the symbol locator (and thus its name and its declaration position).
type_ptr gives the type.  Create and enter a symbol entry, and return
a pointer to it in *symbol_ptr.
*/
{
  a_type_ptr    tp;
  a_symbol_ptr  sym = NULL;
  a_boolean     suppress_redecl_error = FALSE;
  a_boolean     saved_referenced_flag;

  db_enter(3, "decl_typedef");
  if ((sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS)) != NULL) {
    if (sym->kind == (a_symbol_kind)sk_type ||
        (C_dialect == C_dialect_cplusplus && is_type_symbol(sym))) {
      /* Sym is a type name symbol from the current scope.  Issue an error
         if this is an illegal redefinition of the name; otherwise, reuse
         the existing symbol. */
      if (sym->kind == (a_symbol_kind)sk_type) {
        /* A typedef name -- strip off the typerefs until the base type
           is reached or a qualifier is found. */
        tp = skip_typedefs(sym->variant.type);
      } else if (sym->kind == (a_symbol_kind)sk_enum_tag) {
        /* C++ only. */
        tp = sym->variant.type;
      } else {
        /* C++ only -- sk_class_or_struct_tag or sk_union_tag: */
        tp = sym->variant.class_struct_union.type;
      }  /* if */
      if (identical_types(tp, type_ptr) || is_error_type(tp)) {
        /* The current declaration simply redefines the name to the same
           type, which is permitted in C++ (ARM 7.1.3) and warned about for
           ordinary C.  However, in C++ we may still need an sk_type symbol,
           since tags and typedefs do not occupy the same name space. */
        if (sym->kind == (a_symbol_kind)sk_type) {
          if (C_dialect != C_dialect_cplusplus) {
            /* Allowing a benign redeclaration is an extension in C, so issue
               a warning. */
            pos_diagnostic(strict_ansi_error_severity,
                           ec_duplicate_typedef, &locator->source_position);
          }  /* if */
          mark_declared(sym, &locator->source_position);
          goto return_point;
        } else {
          /* C++ only.  Must be a tag symbol. */
          suppress_redecl_error = TRUE;
        }  /* if */
      } else {
        /* This typedef statement redefines a type name to a different type.
           -- diagnostic is issued in enter_symbol processing. */
      }  /* if */
    } else {
      /* Either the symbol was not declared in the current scope, in which
         case a redefinition here is legal, or else it is not a type name
         symbol, in which case the error message will be issued by
         enter_symbol. */
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus) {
    /* No symbol by this name.  See if this is a tagless class, struct, or
       union type.  If so, the present name will serve as the tag (ARM 7.1.3).
       Note that we do NOT want to do a skip_typerefs on the type; only if
       *type_ptr itself lacks an associated tag symbol with a name do we want
       to create a new symbol. */
    if (!is_error_type(type_ptr) && !is_error_locator(*locator)) {
      sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      if (sym != NULL && is_unnamed_class_symbol(sym)) {
        /* An unnamed tag symbol was created for the class and can be reused
           now that we have a name to assign to it.  We need to unlink it from
           the symbol table, give it the name, and relink it into the symbol
           table. */
        relink_unnamed_class_symbol(sym, locator);
        /* Call set_source_corresp, but preserve the current IL referenced
           setting, which set_source_corresp will clear. */
        saved_referenced_flag = type_ptr->source_corresp.referenced;
        set_source_corresp(&(type_ptr->source_corresp), sym);
        type_ptr->source_corresp.referenced = saved_referenced_flag;
        suppress_redecl_error = TRUE;
        /* Note that we do not look for conflicts between the class's new
           name and the names of its members.  This is an area where the
           wording of the ARM (7.1.3) has been clarified and/or amended by
           the X3J16 working paper, and so the restrictions specified in
           ARM 9.2 do not apply. */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Call enter symbol to create a new symbol for this type.  It will
     also issue an error if the name is already declared in the current
     scope. */
  sym = enter_local_symbol((a_symbol_kind)sk_type, locator,
                           decl_scope_level, suppress_redecl_error);
  /* Create a new type entry and add it to the types list for the current
     scope. */
  sym->variant.type = tp = alloc_type((a_type_kind)tk_typeref);
  tp->variant.typeref.type = type_ptr;
  set_source_corresp(&(tp->source_corresp), sym);
  f_mark_defined(sym, &locator->source_position, declarator_ssep);
  add_to_types_list(tp, decl_scope_level);

return_point:
  /* Return the type name symbol to the caller. */
  *symbol_ptr = sym;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(*symbol_ptr, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_typedef */


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
    f_mark_defined(sym, &sym->decl_position, param_id->source_sequence_entry);
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


void decl_default_function(a_symbol_ptr symbol_ptr)
/*
Declare the given symbol as a default function.  This routine is called
when a previously-unknown identifier appears in an expression followed by
a left parenthesis, indicating that it is an undeclared function.  The
symbol has already been entered as an undefined symbol.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             rout_type, old_type;
  a_symbol_ptr           ext_sym;
  a_symbol_locator       locator;
  a_memory_region_number region_to_switch_back_to;
  a_func_info_block      func_info;

  db_enter(4, "decl_default_function");
  /* Change the symbol kind to routine.  Note that the symbol has already
     been entered.  Fortunately, "undefined" and "routine" are in the
     same name space, so that proper checking for a duplicate definition
     will have already been done (there's a check in symbol_tbl_init
     that the name spaces are the same). */
  set_symbol_kind(symbol_ptr, (a_symbol_kind)sk_routine);
  /* In pcc mode, all routines are entered at file scope level.  Remove
     and re-enter the symbol (if necessary) so it will be there. */
  if (C_dialect == C_dialect_pcc) {
    if (symbol_ptr->decl_scope != FILE_SCOPE_NUMBER) {
      /* Take the symbol out of the symbol table. */
      remove_symbol(symbol_ptr);
      /* Put the symbol back into the symbol table at the file scope level. */
      reenter_symbol(symbol_ptr, DEPTH_OF_FILE_SCOPE, /*suppress_error=*/TRUE);
    }  /* if */
  }  /* if */
  /* All IL routines and their types must be at the file scope level, so switch
     to that memory region if necessary. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Generate the function type, with an old-style no-information parameter
     list, and in C a return type of "int".  See 3.3.2.2, semantics. In C++
     this must be an error, so give it a return type of tk_error. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.extra_info->param_type_list = NULL;
  rout_type->variant.routine.extra_info->prototyped = FALSE;
  if (C_dialect != C_dialect_cplusplus) {
    rout_type->variant.routine.return_type =
                                       integer_type((an_integer_kind)ik_int);
  } else {
    /* Making the return type an error type prevents cascading errors. */
    rout_type->variant.routine.return_type = error_type();
  }  /* if */
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
  if (exceptions_enabled) func_info.throw_position = locator.source_position;
  decl_var_or_routine(&locator, (a_storage_class)sc_extern, rout_type,
                      &func_info, (a_source_sequence_entry_ptr)NULL,
                      /*is_variable_def=*/FALSE, &symbol_ptr,
                      &linkage, &old_type, &ext_sym);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  symbol_ptr->variant.routine.ptr->source_corresp.referenced = TRUE;
  switch_back_to_original_region(region_to_switch_back_to);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(symbol_ptr, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_default_function */


a_label_ptr scan_label(a_boolean is_definition)
/*
Scan a label as part of a statement label or goto statement.  Return a
pointer to the IL label.  The current token should be a label identifier.
is_definition is TRUE if the label is being scanned as part of a label.
*/
{
  a_symbol_ptr      label_sym;
  a_label_ptr       label;

  a_source_position start_pos;

  db_enter(3, "scan_label");

  copy_source_position(pos_curr_token, start_pos);
  if (curr_token != tok_identifier) {
    (void)required_token(tok_identifier, ec_exp_identifier);
    set_to_error_locator(locator_for_curr_id);
    label_sym = NULL;
  } else {
    /* See if the label identifier is already in the symbol table. */
    label_sym = symbol_list_from_locator(locator_for_curr_id);
    get_symbol_of_kind((a_symbol_kind)sk_label, label_sym);
    /* If the label is not from the current function, pretend it was
       not found.  This comes up in functions within local classes:
         void f() {
           label1:;
           class A {
             void g() { goto label1; }
           };
         }
    */
    if (label_sym != NULL &&
        label_sym->decl_scope !=
            scope_stack[depth_innermost_function_scope].il_scope->number) {
      /* This label is from an outer routine.  Pretend it's not found. */
      label_sym = NULL;
    }  /* if */  
  }  /* if */
  if (label_sym == NULL) {
    /* Enter the label identifier into the symbol table.  This is done
       at the function level even if we are inside some blocks.  Use
       a locator with an undefined source position; the decl_position will
       be handled explicitly shortly. */
    locator_for_curr_id.source_position.seq = 0;
    locator_for_curr_id.source_position.column = SP_COL_UNKNOWN;
    label_sym = enter_symbol((a_symbol_kind)sk_label, &locator_for_curr_id,
                             depth_innermost_function_scope,
                             /*suppress_error=*/TRUE);
    /* Allocate the IL label and attach it to the symbol. */
    label_sym->variant.label.ptr = label = alloc_label();
    add_to_labels_list(label);
    set_source_corresp(&label->source_corresp, label_sym);
    /* The exec_stmt field stays NULL to indicate that the declaration
       has not been (fully?) processed yet. */
  }  /* if */
  if (!is_error_locator(locator_for_curr_id)) {
    /* Record the right kind of reference to the label symbol. */
    if (is_definition) {
      /* Note that we want mark_defined is called even if the symbol
         was previously entered.  Labels are strange in that a reference
         can come up before a declaration. */
      mark_defined(label_sym, &pos_curr_token);
    } else {
      mark_referenced(label_sym, &pos_curr_token);
      /* Set the decl_position in case no declaration shows up, so we
         have the location of the use. */
      if (label_sym->decl_position.seq == 0 &&
          label_sym->decl_position.column == SP_COL_UNKNOWN) {
        copy_source_position(pos_curr_token, label_sym->decl_position);
      }  /* if */
    }  /* if */
    /* Advance past the identifier. */
    (void)get_token();
  }  /* if */

  copy_source_position(start_pos, error_position);
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(label_sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(label_sym->variant.label.ptr);
}  /* scan_label */


static a_type_ptr pointer_declarator(a_type_ptr  specifiers_type,
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
      a_decl_flag_set    dso_flags;
      a_storage_class    dummy_storage_class;
      a_type_ptr         dummy_type_ptr;

      set_err_pos_to_curr_token();
      (void)decl_specifiers(DSI_COLLECT_TYPE_QUALIFIERS, &dso_flags,
                            &dummy_storage_class, &dummy_type_ptr);
      if (is_reference_type(complete_type)) {
        warning(ec_qualified_reference_type);
      }  /* if */
      complete_type = make_qualified_type(complete_type,
                                          dso_flags & DSO_CONST_QUALIFIED,
                                          dso_flags & DSO_VOLATILE_QUALIFIED);
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


static a_boolean simplify_curr_class_qualified_name(void)
/*

If the current token is the start of a qualified name in which the class
name component is the name of a class currently being defined, advance past
the class name and the "::" so that the current token is a non-qualified
name.  Return TRUE if such a modification is done and FALSE otherwise.
This routine is called in C++ only.

This functionality is provided to deal with declarations of class members
where a qualified name is used instead of a simple name, e.g., when a
constructor for class A is declared A::A() rather than A().  The ARM does
not specifically allow this syntax, but it is supported by cfront.
*/
{
  a_boolean                is_member_id = FALSE;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  db_enter(3, "simplify_curr_class_qualified_name");

  if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
      is_generalized_identifier_start(GID_TEMPLATE_ARGS_OPTIONAL) &&
      locator_for_curr_id.is_qualified_name) {
    if (locator_for_curr_id.qualifier_class_type == ssep->assoc_type &&
        locator_for_curr_id.is_global_qualified_name == FALSE) {
      is_member_id = TRUE;
      /* Issue any access errors encountered while scanning the
         qualifier -- even though there shouldn't be any for this
         case. */
      issue_qualifier_access_errors(&locator_for_curr_id.access_errors);
      /* Reset the fields in the locator to make it appear as if the
         qualifier was not present. */
      locator_for_curr_id.is_qualified_name = FALSE;
      locator_for_curr_id.is_file_scope_qualified_name = FALSE;
      locator_for_curr_id.is_global_qualified_name = FALSE;
      locator_for_curr_id.qualifier_class_type = NULL;
      /* Accepting qualified member names is an extension so issue a
         diagnostic in strict ANSI mode. */
      if (strict_ansi_mode) {
        diagnostic(strict_ansi_error_severity,
                   ec_qualifier_in_member_declaration);
      }  /* if */ 
    }  /* if */
  }  /* if */
  db_exit();
  return is_member_id;
}  /* simplify_curr_class_qualified_name */


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
  a_boolean       is_member_def = FALSE;
  a_boolean       real_declarator_allowed;
  a_boolean       abstract_declarator_allowed;
  a_boolean       is_name_start;
  a_boolean       is_constructor = FALSE, is_destructor = FALSE;
  a_boolean       is_nonstatic_member_function = FALSE;
  a_boolean       is_ptr_to_member_typedef = FALSE;
  a_boolean       nonconstant_dimension_allowed;
  a_boolean       parenthesized_initializer_allowed;
  a_boolean       is_friend_decl = FALSE;

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
  if (func_info != NULL) clear_func_info(func_info);
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
    /* A nonconstant dimension, if allowed at all, is allowed only on the
       topmost type (an interpretation of the language specification in ARM
       5.3.3).  Set the flag to FALSE for subsequent processing. */
    nonconstant_dimension_allowed = FALSE;
    /* Check for and get the closing parenthesis. */
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
      *declarator_ssep =
           add_empty_source_sequence_entry(
                   /*alloc_in_fs=*/TRUE,
                   /*proxy_allowed=*/scope_stack[depth_scope_stack].kind !=
                                             (a_scope_kind)sck_func_prototype);
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
        if (cfront_compatibility_mode) {
          /* Provide support for an exploitable cfront bug. */
          if (locator_for_curr_id.is_qualified_name &&
              locator_for_curr_id.qualifier_class_type != NULL &&
              input_flags & DI_IS_TYPEDEF_DECLARATION &&
              next_token() == tok_lparen) {
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
            is_ptr_to_member_typedef = TRUE;
            /* Toss out the qualifier. */
            locator_for_curr_id.qualifier_class_type = NULL;
            locator_for_curr_id.is_qualified_name = FALSE;
            /* Issue a warning. */
            pos_warning(ec_ptr_to_member_typedef,
                        &locator_for_curr_id.source_position);
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
              a_symbol_ptr sym = locator_for_curr_id.specific_symbol;
              /* See if the name is the name of a member function. */
              if (sym->kind == (a_symbol_kind)sk_member_function ||
                  sym->kind == (a_symbol_kind)sk_overloaded_function ||
                  sym->kind == (a_symbol_kind)sk_function_template) {
                /* It is a member function.  Save information about the class
                   needed to reopen the class scope if a function declarator
                   is scanned. */
                is_member_def = TRUE;
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
                is_member_def = TRUE;
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
                cfront_compatibility_mode && allow_anachronisms) {
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
                              /*real_declarator_allowed=*/TRUE)) {
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
      if (is_member_def) {
        /* The parameters of member functions are scanned with the original
           class reactivated. */
        push_class_reactivation_scope(member_parent_type);
      }  /* if */
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
        } else if (is_ptr_to_member_typedef) {
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
      if (*declarator_ssep != NULL) {
        if (func_info != NULL) {
          if (locator != NULL && !is_error_locator(*locator)) {
            func_info->declarator_ssep = *declarator_ssep;
          } else {
            remove_from_source_sequence_list(declarator_ssep,
                                             (a_type_ptr)NULL);
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      if (is_member_def) {
        pop_class_reactivation_scope();
      }  /* if */
    } else {
      if (is_member_def) {
        /* The dimensions of static data members are scanned with the original
           class reactivated. */
        push_class_reactivation_scope(member_parent_type);
      }  /* if */
      /* Left bracket, indicating array declarator. */
      array_declarator(&new_type_ptr, nonconstant_dimension_allowed);
      if (nonconstant_dimension_allowed) {
        /* In C++ a array declarator that appears in an operator new()
           expression may have a nonconstant expression in the first
           dimension (ARM 5.3.3).  Subsequent dimension must be constants. */
        nonconstant_dimension_allowed = FALSE;
      }  /* if */
      if (is_member_def) {
        pop_class_reactivation_scope();
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
      check_assertion((*output_flags & DO_REAL_DECLARATOR_SCANNED) ||
                      is_ptr_or_ref_type(complete_type) ||
                      derived_type != NULL ||
                      is_ptr_to_member_type(complete_type));
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


a_symbol_ptr scan_tag_name(a_symbol_kind     tag_kind,
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
        locator_for_curr_id.specific_symbol = NULL;
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
#if 0
    if (C_dialect == C_dialect_cplusplus &&
        scope_stack[decl_scope_level].kind ==
                                       (a_scope_kind)sck_func_prototype) {
    } else {
#endif /* if 0 */
      /* Look for a tag symbol in the current scope.  If the tag kind does
         not match the tag being processed, issue an error. */
      tag_sym = curr_scope_id_lookup(&locator_for_curr_id, IDL_MUST_BE_TAG);
      if (tag_sym != NULL && tag_sym->kind != tag_kind) {
        pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                       &locator_for_curr_id.source_position,
                       name_of_symbol_kind(tag_kind), tag_sym);
        tag_sym = NULL;
        tag_err = TRUE;
      }  /* if */
#if 0
    }  /* if */
#endif /* if 0 */
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
        /* The tag has already appeared in the current scope. */
        if (is_incomplete_type(type_symbol_type(tag_sym))) {
          /* Resolution of a previous incomplete declaration. */
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
    } else if (curr_token == tok_semicolon) {
      /* A useless redeclaration of an enum tag. */
      mark_declared(tag_sym, &locator.source_position);
    } else {
      mark_referenced(tag_sym, &locator.source_position);
    }  /* if */
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
                  f_skip_typerefs(constant.type)->size <= TARG_SIZEOF_INT) {
                /* In non-strict mode, allow unsigned constants that can be
                   coerced into an int. */
                type_change_constant(&constant,
                                     integer_type((an_integer_kind)ik_int),
                                     /*is_implicit_cast=*/TRUE,
                                     /*constant_context=*/TRUE,
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
    /* Determine the representation type for the enumeration.  In pcc mode,
       and when enum_types_can_be_smaller_than_int is FALSE, it's always
       "int", and that's already set.  Otherwise, pick the first of "char",
       "signed char", "unsigned char", "short", "unsigned short", and
       "int" into which the enumeration values will fit.  Note that
       it is pointless to try "unsigned int", because all enumeration
       values must fall in the "int" range. */	
    if (C_dialect != C_dialect_pcc && enum_types_can_be_smaller_than_int) {
      if (!min_max_set ||
          in_range_for_integer_kind(&min_value, &max_value,
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
#if TARG_SIZEOF_SHORT < TARG_SIZEOF_INT
      } else if (in_range_for_integer_kind(&min_value, &max_value,
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
#endif /* TARG_SIZEOF_SHORT < TARG_SIZEOF_INT */
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


/*
Local macro for decl_specifiers: if curr_token_type_symbol has not
been determined, determine it now.
*/
#define determine_curr_token_type_symbol(is_new_type_name)            \
{ if (!determined_curr_token_type_symbol) {                           \
    curr_token_type_symbol = curr_type_symbol(is_new_type_name);      \
    determined_curr_token_type_symbol = TRUE;                         \
  }  /* if */                                                         \
}  /* determine_curr_token_type_symbol */


a_boolean decl_specifiers(a_decl_flag_set       input_flags,
                          a_decl_flag_set       *output_flags,
                          a_storage_class       *storage_class,
                          a_type_ptr            *type_ptr)
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

The DSI_IS_PARAMETER bit of input_flags is set if these specifiers are
part of the declaration of a parameter, and, for C++, the
DSI_VIRTUAL_OR_FRIEND_ALLOWED bit is set when a declaration appears within
a class declaration, to permit recognition of "virtual" and "friend"
keywords.

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
  int          num_specifiers;
  a_symbol_ptr curr_token_type_symbol;
  a_boolean    determined_curr_token_type_symbol = FALSE;
  a_boolean    err = FALSE;
  a_boolean    bad_combination_of_type_specifiers = FALSE;
  a_source_position
               start_pos;
  a_type_kind  kind;
  an_integer_kind
	       ikind;
  a_float_kind fkind;
  a_type_ptr   temp_type;
  a_type_ptr   base_type;
  a_boolean    explicitly_signed;

  a_boolean    is_const_qualified    = FALSE;
  a_boolean    is_volatile_qualified = FALSE;
  a_boolean    is_parameter = (input_flags & DSI_IS_PARAMETER);
  a_boolean    is_member_decl = (input_flags & DSI_IS_MEMBER_DECLARATION);
  a_boolean    vacuous_decl_allowed;
  a_boolean    declares_something = FALSE;
  a_boolean    defines_something = FALSE;
  a_boolean    void_first_specifier;
  a_boolean    type_specifier_allowed;
  a_boolean    dangling_type_specifier = FALSE;
  a_boolean    is_elaborated_type_specifier = FALSE;
  a_boolean    is_friend_decl = FALSE;
  a_boolean    is_inline = FALSE;
  an_error_severity
               es;

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
          if (curr_token == tok_typedef) {
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
        } else {
          if (C_dialect != C_dialect_pcc && !err) {
            if (num_specifiers > ((*output_flags & DSO_FRIEND) ? 1 : 0) +
                                 (is_inline ? 1 : 0)) {
              /* Issue a warning if the storage class is not the first
                 specifier (except for "inline" or "friend"). */
              warning(ec_storage_class_not_first);
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
      case tok_const:
        /* const type qualifier (3.5.3). */
        if (is_const_qualified) {
          /* const may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
#if 0
/* The ARM requires the following check, but Stroustrup has stated that that
   is probably an error.  We expect this to be changed. */
#endif /* if 0 */
          if (input_flags & DSI_IS_NEW_TYPE_NAME && strict_ansi_mode) {
            /* volatile may not appear in a new-type-name -- ARM 5.3.3. */
            warning(ec_const_volatile_not_allowed);
          }  /* if */
          is_const_qualified = TRUE;
          /* Set the output_flags bit, for the case where only type qualifiers
	     are acceptable, and therefore there is no type entry in which to
	     return the const qualifier. */
	  *output_flags |= DSO_CONST_QUALIFIED;
        }  /* if */
        break;
      case tok_volatile:
        /* volatile type qualifier (3.5.3). */
        if (is_volatile_qualified) {
          /* volatile may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
#if 0
/* The ARM requires the following check, but Stroustrup has stated that that
   is probably an error.  We expect this to be changed. */
#endif /* if 0 */
          if (input_flags & DSI_IS_NEW_TYPE_NAME && strict_ansi_mode) {
            /* volatile may not appear in a new-type-name -- ARM 5.3.3. */
            warning(ec_const_volatile_not_allowed);
          }  /* if */
          is_volatile_qualified = TRUE;
          /* Set the output_flags bit, for the case where only type qualifiers
	     are acceptable, and therefore there is no type entry in which to
	     return the const qualifier. */
	  *output_flags |= DSO_VOLATILE_QUALIFIED;
        }  /* if */
        break;
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
                unget_token();
                curr_token = tok_friend;
              } else if (cfront_compatibility_mode && tag_sym == NULL) {
                /* This friend declaration introduces a new type -- which is
                   okay in cfront compatibility mode.  Still, issue a remark
                   on use of a nonstandard feature. */
                vacuous_decl_allowed = FALSE;
                pos_st_remark(ec_nonstd_friend_decl, &ident_pos, "class");
                goto process_class_specifier;
              } else {
                a_type_ptr  tp = NULL;

                if (tag_sym != NULL && is_class_symbol(tag_sym)) {
                  tp = type_symbol_type(tag_sym);
                  if (tp->kind == (a_type_kind)tk_typeref) {
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
                     (by default a remark, but potentially a more severe
                     diagnostic in strict ANSI mode) to report the use of a
                     nonstandard feature. */
                  an_error_severity  severity;
                  char               *class_key_string;

                  if (strict_ansi_mode) {
                    /* Strict ANSI diagnostic in strict ANSI mode. */
                    severity = strict_ansi_error_severity;
                  } else {
                    /* Default case -- remark. */
                    severity = es_remark;
                  }  /* if */
                  *type_ptr = tp;
                  switch ((*type_ptr)->kind) {
                    case tk_class:   class_key_string = "class";   break;
                    case tk_struct:  class_key_string = "struct";  break;
                    case tk_union:   class_key_string = "union";   break;
#if CHECKING
                    default: internal_error("decl_specifiers: bad type kind");
#endif /* CHECKING */
                  }  /* switch */
                  pos_st_diagnostic(severity, ec_nonstd_friend_decl,
                                    &ident_pos, class_key_string);
                }  /* if */
                basic_type = bt_struct_union;
                is_elaborated_type_specifier = TRUE;
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
          bad_combination_of_type_specifiers = TRUE;
          error(ec_bad_combination_of_type_specifiers);
        } else {
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
          bad_combination_of_type_specifiers = TRUE;
          error(ec_bad_combination_of_type_specifiers);
        } else {
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
            if (num_specifiers > 0) vacuous_decl_allowed = FALSE;
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
                                 (is_inline ? 1 : 0)) &&
              is_name_of_curr_class()) {
            /* The name is the same as that of a class being defined.  This
               is treated as a constructor declaration if the next two
               tokens are a left paren and declaration start token.  Use
               token caching in the look-ahead, since the tokens will have
               to be rescanned no matter what. */
            a_token_cache    cache;
            a_boolean        is_constructor = FALSE;

            /* Change "A::A" into "A" if we are processing inside the
               definition of class "A".  This is necessary for
               determine_curr_type_symbol to handle this case
               correctly. */
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
              a_type_ptr         tp = scope_stack[decl_scope_level].assoc_type;
              a_symbol_ptr       tag_sym =
                                   (a_symbol_ptr)tp->source_corresp.assoc_info;
              a_source_position  pos;

              basic_type = bt_no_type;
              *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
              /* Turn the current locator from a "specific symbol" locator
                 into a constructor locator. */
              determine_curr_token_type_symbol(/*is_new_type_name=*/FALSE);
              if (curr_token_type_symbol != tag_sym) {
                if (locator_for_curr_id.specific_symbol->
                                         class_of_which_a_member == tp) {
                  if (locator_for_curr_id.specific_symbol->kind !=
                                            (a_symbol_kind)sk_projection) {
                    /* This can only mean that another member has been declared
                       with the class name.  Issue an error. */
                    str_error(ec_id_already_declared,
                              locator_for_curr_id.symbol_header->identifier);
                  }  /* if */
                } else if (curr_token_type_symbol != NULL) {
                  check_assertion(
                          is_template_class_symbol(curr_token_type_symbol));
                  pos_sy2_error(ec_bad_constructor_name,
                                &locator_for_curr_id.source_position,
                                curr_token_type_symbol, tag_sym);
                }  /* if */
                locator_for_curr_id.specific_symbol = tag_sym;
              }  /* if */
              pos = locator_for_curr_id.source_position;
              change_class_locator_into_constructor_locator(
                                                   &locator_for_curr_id, &pos);
              /* Note that with a branch to exit_loop the get_token call
                 is bypassed.  This means curr_token will still represent
                 the constructor name (= class name) upon return to the
                 caller. */
              goto exit_loop;
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
        determine_curr_token_type_symbol(input_flags & DSI_IS_NEW_TYPE_NAME);
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
          mark_referenced(curr_token_type_symbol, &pos_curr_token);
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
        if (num_specifiers == 0 &&
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
        {
        a_boolean     local_err = FALSE;

        if (next_token() == tok_lt) {
          /* This appears to be a template declaration inside another
             declaration.  Go ahead and scan the template declaration. */
          if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
            /* Error will be issued in template_declaration. */
            local_err = TRUE;
          } else {
            error(ec_template_not_allowed);
            local_err = TRUE;
          }  /* if */
          (void)template_declaration(&defines_something);
          defines_something = TRUE;
        } else {
          local_err = TRUE;
          error(ec_template_not_allowed);
          (void)get_token();
        }  /* if */
        if (local_err) {
          err = TRUE;
          if (basic_type == bt_typedef) {
            *type_ptr = error_type();
          } else {
            basic_type = bt_error;
          }  /* if */
        }  /* if */
        }
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
          syntax_error(ec_exp_type_specifier);
          err = TRUE;
          basic_type = bt_error;
        }  /* if */
        goto exit_loop;
    }  /* switch */
    (void)get_token();
no_get_token:
    num_specifiers++;
    determined_curr_token_type_symbol = FALSE;
    /* Check for special conditions that will cause this loop to terminate. */
    if (input_flags & DSI_COLLECT_TYPE_QUALIFIERS) {
      /* We are only interested in scanning type qualifiers (e.g., in a
         pointer declarator). */
      if (!is_type_qualifier()) goto exit_loop;
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
           class B; typedef class {...} B...    <== Error detected elsewhere
           class C; typedef class C {...} C...  <== Legal
         Note that this logic works for both C++ and standard C. */
      if (curr_token == tok_identifier &&
          *storage_class != (a_storage_class)sc_typedef) {
        determine_curr_token_type_symbol(/*is_new_type_name=*/FALSE);
      }  /* if */
      if (is_type_specifier() ||
          (C_dialect == C_dialect_cplusplus &&
           *storage_class != (a_storage_class)sc_typedef &&
           determined_curr_token_type_symbol &&
           curr_token_type_symbol != NULL &&
           curr_token_type_symbol->decl_scope ==
                                     scope_stack[decl_scope_level].number)) {
        /* The current token is either a type keyword or a type name; treat
           it as the start of a new declaration.  Missing punctuation and
           the error will be handled by the caller. */
        dangling_type_specifier = TRUE;
        goto exit_loop;
      } else if (curr_token == tok_identifier &&
                 locator_for_curr_id.is_qualified_name &&
                 locator_for_curr_id.specific_symbol != NULL &&
                 (is_constructor_symbol(locator_for_curr_id.specific_symbol) ||
                  is_destructor_symbol(locator_for_curr_id.specific_symbol))) {
        /* In this case we find a class or enum definition followed by what
           can only be a constructor or destructor declaration, e.g.,
             class A { A(); ...} A::A()...
           This is not caught with the others because the type specifier is
           omitted.  The missing semicolon will be handled by the caller. */
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
    if (!err && !is_const_qualified &&
        !is_volatile_qualified && !defines_something && 
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
    if (is_const_qualified || is_volatile_qualified) {
      if (C_dialect == C_dialect_cplusplus &&
          (*type_ptr)->kind == (a_type_kind)tk_typeref) {
        /* In C++ adding a qualifier to a typedef name that is already
           identically qualified is okay, so don't even bother checking for
           an error.  Note that make_qualified_type will not actually add
           superfluous qualifiers. */
      } else {
        /* According to 3.5.3: "If the specification of an array type
           includes any type qualifiers, the element type is so-qualified,
           not the array type.", and this is interpreted recursively
           for arrays of arrays.  The type qualifiers therefore apply
           to the ultimate element type.  This can only happen with typedefs,
           as in "typedef int A[2][3]; const A a;", which makes "a" an
           array of array of const int. */
        base_type = *type_ptr;
        if (is_array_type(base_type)) {
          base_type = underlying_array_element_type(base_type);
        }  /* while */
        if ((is_const_qualified && is_const_qualified_type(base_type)) ||
            (is_volatile_qualified && is_volatile_qualified_type(base_type))) {
          /* Duplication of type qualifier (probably because of a typedef
             that is already qualified). */
          error(ec_dupl_type_qualifier);
          err = TRUE;
        }  /* if */
      }  /* if */
      /* Add the qualifiers if necessary.  make_qualified_type understands the
         strange array case too. */
      *type_ptr = make_qualified_type(*type_ptr,
                                      is_const_qualified,
                                      is_volatile_qualified);
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


void type_name(a_type_ptr *type_ptr)
/*
Scan a type-name (see 3.5.5) and return a pointer to the type.  The syntax is:

3.5.5  type-name:
		specifier-qualifier-list abstract-declarator
							    opt
*/
{
  a_storage_class              storage_class;
  a_decl_flag_set              dso_flags, do_flags;
  a_type_ptr                   bottom_derived_type;
  a_source_position            start_pos;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;

  db_enter(3, "type_name");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
			&storage_class, type_ptr);
  if (C_dialect == C_dialect_cplusplus &&
      (dso_flags & DSO_DEFINES_SOMETHING)) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_abstract_declarator_start()) {
    declarator(DI_ABSTRACT_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED,
               &do_flags, *type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
	       (a_symbol_locator *)NULL,
               type_ptr, &bottom_derived_type, &declarator_ssep,
               (a_func_info_block_ptr)NULL);
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* type_name */


void new_type_name(a_boolean         is_parenthesized,
                   a_type_ptr        *type_ptr)
/*
Scan a C++ new-type-name or a parenthesized type-name that may appear in a
"new" expression (ARM 5.3.3), and return a pointer to the type.  The
syntax is:
   new-type-name:
              type-specifier-list new-declarator
                                                opt
   new-declarator:
              * cv-qualifier-list    new-declarator
                                 opt               opt
              class-name :: * cv-qualifier-list    new-declarator
                                               opt               opt
              new-declarator    [ expression ]
                            opt

   type-name:
              type-specifier-list abstract-declarator
                                                     opt
*/
{
  a_type_ptr            complete_type, new_type_ptr;
  a_type_ptr            derived_type, bottom_derived_type = NULL;
  a_decl_flag_set       dso_flags, do_flags;
  a_source_position     start_pos;
  a_storage_class       storage_class;
  a_source_sequence_entry_ptr
                        declarator_ssep = NULL;

  db_enter(3, "new_type_name");
  if (!is_parenthesized && curr_token == tok_lparen) {
    is_parenthesized = TRUE;
    (void)get_token();
  }  /* if */
  if (is_parenthesized) add_stop_token(tok_rparen);
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED | DSI_IS_NEW_TYPE_NAME,
                        &dso_flags, &storage_class, type_ptr);
  if (C_dialect == C_dialect_cplusplus && dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  if (*type_ptr != NULL) {
    (skip_typerefs(*type_ptr))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  bottom_derived_type = NULL;
  if (is_parenthesized) {
    if (is_abstract_declarator_start()) {
      declarator(DI_ABSTRACT_DECLARATOR_ALLOWED |
                    DI_QUALIFIED_NAME_ALLOWED |
                    DI_DIMENSION_EXPRESSION_ALLOWED,
                 &do_flags, *type_ptr,
                 /*member_parent_type=*/(a_type_ptr)NULL,
                 (a_symbol_locator *)NULL, type_ptr,
                 &bottom_derived_type, &declarator_ssep,
                 (a_func_info_block_ptr)NULL);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    complete_type = pointer_declarator(*type_ptr, &bottom_derived_type,
                                       /*reference_allowed=*/FALSE);

    derived_type = NULL;
    bottom_derived_type = NULL;
    add_stop_token(tok_lbracket);
    if (curr_token == tok_lbracket) {
      array_declarator(&new_type_ptr, /*nonconstant_allowed=*/TRUE);
      add_to_derived_type_list(new_type_ptr,
                               &derived_type, &bottom_derived_type);
      while (curr_token == tok_lbracket) {
        array_declarator(&new_type_ptr, /*nonconstant_allowed=*/FALSE);
        /* Add the new type to the bottom of the existing derived type list.
           Note that this involves error checking. */
        add_to_derived_type_list(new_type_ptr,
                                 &derived_type, &bottom_derived_type);
      }  /* while */
      if (derived_type != NULL) {
        if (complete_type != NULL) {
          if (!is_error_type(bottom_derived_type)) {
            /* Combine derived_type and complete_type. */
            add_to_derived_type_list(complete_type,
                                     &derived_type, &bottom_derived_type);
          }  /* if */
        }  /* if */
        complete_type = derived_type;
      }  /* if */
    }  /* if */
    remove_stop_token(tok_lbracket);
    *type_ptr = complete_type;
  }  /* if */
  db_exit();
}  /* new_type_name */


a_boolean scan_conversion_operator(a_source_position  *id_pos,
				   a_type_ptr	      class_type)
/*
The token "operator" has been seen and passed; we are now on the token
immediately following it.  If it marks the start of a type name we have
an identifier for a conversion operator -- scan the type name, update the
locator, and return TRUE.  If it doesn't, return FALSE.
If class_type is not NULL then push a class reactivation scope before
scanning type name in a type conversion operator.
*/
{
  a_storage_class           storage_class;
  a_decl_flag_set           dso_flags;
  a_type_ptr                specifiers_type, complete_type;
  a_type_ptr                bottom_derived_type = NULL;
  a_source_position         type_pos;
  a_boolean                 is_conversion_operator;

  db_enter(3, "scan_conversion_operator");
  /* Push a class reactivation scope if class_type is not NULL.  This is
     used when scanning conversion operators such as "A::operator B" where
     B needs to be looked up within A.  This is not needed for overloaded
     operator routines, but we don't know what kind of operator we are
     scanning until we call is_type_start, and the class needs to
     be reactivated before is_type_start is called. */
  if (class_type != NULL) push_class_reactivation_scope(class_type);
  if (is_type_start()) {
    /* It is the start of a type name. */
    is_conversion_operator = TRUE;
    set_err_pos_to_curr_token();
    copy_source_position(pos_curr_token, type_pos);
    (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
                          &storage_class, &specifiers_type);
    if (C_dialect == C_dialect_cplusplus &&
        (dso_flags & DSO_DEFINES_SOMETHING)) {
      /* Definition of a class, struct, union, or enum type is not allowed. */
      pos_error(ec_type_definition_not_allowed, &type_pos);
    } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
      /* Missing type specifier. */
      warning(ec_missing_type_specifier);
    }  /* if */
    complete_type = pointer_declarator(specifiers_type, &bottom_derived_type,
                                       /*reference_allowed=*/TRUE);
    unget_token();
    curr_token = tok_identifier;
    pos_curr_token = error_position = *id_pos;
    make_type_conversion_locator(complete_type, &locator_for_curr_id, id_pos);
  } else {
    is_conversion_operator = FALSE;
  }  /* if */
  /* Pop the class reactivation scope if one was pushed earlier. */
  if (class_type != NULL) pop_class_reactivation_scope();
  db_exit();
  return is_conversion_operator;
}  /* scan_conversion_operator */


a_type_ptr type_keyword(void)
/*
If the current token is a type keyword (e.g., int, long); return the type
indicated by the keyword, otherwise, return NULL.  This is used in scanning
a simple-type-name for C++ functional-notation casts.  The current token is
not advanced.  Note that this routine does not deal with identifiers that
are defined as types, only keywords; it also does not accept multi-token
types, e.g., "unsigned int".  See ARM 7.1.6 and 5.2.3.
*/
{
  a_type_ptr type;

  switch (curr_token) {
    case tok_char:
      type = integer_type((an_integer_kind)ik_char);
      break;
    case tok_short:
      type = integer_type((an_integer_kind)ik_short);
      break;
    case tok_int:
    case tok_signed:
      type = integer_type((an_integer_kind)ik_int);
      break;
    case tok_long:
      type = integer_type((an_integer_kind)ik_long);
      break;
    case tok_unsigned:
      type = integer_type((an_integer_kind)ik_unsigned_int);
      break;
    case tok_float:
      type = float_type((a_float_kind)fk_float);
      break;
    case tok_double:
      type = float_type((a_float_kind)fk_double);
      break;
    case tok_void:
      type = void_type();
      break;
    default:
      type = NULL;
      break;
  }  /* switch */
  return type;
}  /* type_keyword */


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
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr    ssep, next_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

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
    if (func_info->param_id_list != NULL) {
      /* Step through the segment of file-scope source sequence entries
         generated when the parameter list of the function was scanned.
         Do necessary fixups for parameter entries, and build function-scope
         proxies where necessary. */
      ssep = func_info->prototype_scope_ss_entry_start;
      for (; ssep != NULL; ssep = next_ssep) {
        if (ssep == func_info->prototype_scope_ss_entry_end) {
          next_ssep = NULL;
        } else {
          next_ssep = ssep->next;
        }  /* if */
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
#if CHECKING
            if (param_id == NULL) {
              internal_error("scan_function_body: no param-id for ss entry");
            }  /* if */
#endif /* CHECKING */
            /* Take the entry off the file-scope list and add one (also
               empty so far) to the function-scope list. */
            remove_from_source_sequence_list(
                              &ssep, func_info->class_in_which_defined_inline);
            param_id->source_sequence_entry =
                  add_empty_source_sequence_entry(/*alloc_in_fs=*/FALSE,
                                                  /*proxy_allowed=*/FALSE);
            break;
          case iek_type:
            /* For types, make proxy entries on the function scope list. */
            { a_type_ptr tp = ss_entry_ptr(ssep, a_type_ptr);
              if (is_immediate_class_type(tp) || is_immediate_enum_type(tp)) {
                make_proxy_ptr_source_sequence_entry(ssep);
              }  /* if */
            }
            break;
          default:;
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
      /* Free the list of parameter ids, now that it is no longer needed. */
      free_param_id_list(&(func_info->param_id_list));
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
  /* The lint "argsused" and "varargs" flags are only applicable until
     the end of a function declaration. */
  lint_argsused_flag = FALSE;
  lint_varargs_count = NOT_LINT_VARARGS;
  /* Check for the closing "}", not done in compound_statement.  Note that
     required_token is not called; if compound_statement returned on
     anything other than a right brace, it's because we should start parsing
     on this token. */
  if (curr_token != tok_rbrace) {
    pos_error(ec_exp_rbrace, &pos_curr_token);
  } else {
    (void)get_token();
  }  /* if */
  db_exit();
}  /* scan_function_body */


static void function_definition(a_symbol_locator   *locator,
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
#if CHECKING
      if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
        internal_error("function_definition: bad region number");
      }  /* if */
#endif /* CHECKING */
#if 0
#else /* 0 */
      /* Remember the scope number for later use when the body is scanned. */
      func_info->scope_number = scope_stack[depth_scope_stack].number;
#endif /* if 0 */
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
        ptp = alloc_param_type(param_id->type);
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
      /* Pop the function prototype scope. */
      pop_scope();
    }  /* if */
    /* Create the symbol entry and routine entry for the routine. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    declarator_ssep = func_info->declarator_ssep;
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    declarator_ssep = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    decl_var_or_routine(locator, storage_class, rout_type, func_info,
                        declarator_ssep, /*is_variable_def=*/FALSE,
                        &symbol_ptr, &linkage, &old_type, &ext_sym);
  }  /* if */
  routine_ptr = symbol_ptr->variant.routine.ptr;
  check_assertion(make_unqualified_type(routine_ptr->type) ==
                                                      unqualified_rout_type);
  if (!is_member_function_def &&
      storage_class == (a_storage_class)sc_unspecified &&
      routine_ptr->source_corresp.name != NULL &&
      strcmp(routine_ptr->source_corresp.name, "main") == 0) {
    /* This is "main", so remember the location of its routine entry. */
    il_header.main_routine = routine_ptr;
  }  /* if */
  /* If a lint-style "argsused" or "varargs" comment appeared, remember that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments. */
  extra_info->lint_argsused_flag = lint_argsused_flag;
  extra_info->lint_varargs_count = lint_varargs_count;
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
    if (cfront_compatibility_mode) {
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


void inline_function_definition(a_routine_ptr     rout_ptr,
                                a_func_info_block *func_info)
/*
Called in rescanning the inline definition of a friend or member function
that appears within a class definition, this routine duplicates relevant
processing of function definition.
*/
{
  db_enter(3, "inline_function_definition");
  /* Scan the function body. */
  scan_function_body(rout_ptr, func_info,
                     (SFB_NO_CLASS_REACTIVATION |
                      SFB_NEW_STRUCT_STMT_STACK_REQUIRED));
  db_exit();
  return;
}  /* inline_function_definition */

#if C_ANACHRONISMS_ALLOWED

static a_boolean is_initializer_start(void)
/*
Return TRUE if the current token appears to be the start of an initializer.
This is used in pcc mode to decide whether or not an old-style initializer
is present when a "=" is not there.
*/
{
  a_boolean    is_init_start = FALSE;
  a_symbol_ptr assoc_symbol;

  if (curr_token == tok_semicolon ||
      curr_token == tok_comma     ||
      curr_token == tok_rbrace    ||
      is_decl_start(/*expr_context=*/FALSE,
                    /*real_declarator_allowed=*/TRUE)) {
    /* No initializer present. */
  } else if (curr_token == tok_identifier) {
    /* Identifier -- only consider as start of an initializer if defined
       as something that might be part of an expression. */
    assoc_symbol = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    if (assoc_symbol != NULL) {
      if (assoc_symbol->kind == (a_symbol_kind)sk_constant ||
          assoc_symbol->kind == (a_symbol_kind)sk_routine  ||
          assoc_symbol->kind == (a_symbol_kind)sk_variable) {
        /* This identifier is defined as an enumeration constant, a routine,
           or a variable, so it might be an initializer.  Note that a variable
           or routine is allowed in that it might get implicitly converted
           to a pointer to that entity. */
        is_init_start = TRUE;
      }  /* if */
    }  /* if */
  } else {
    /* Other tokens (for example, "("); assume this is an initializer. */
    is_init_start = TRUE;
  }  /* if */
  return is_init_start;
}  /* is_initializer_start */

#endif /* C_ANACHRONISMS_ALLOWED */

static void linkage_specification(a_boolean      function_definition_allowed,
                                  a_boolean      is_old_style_param_decl,
                                  a_param_id_ptr param_id_list)
/*
The caller has determined that we are at the start of a C++ linkage
specification -- that is, the current token is "extern" and it is followed
by a string literal.  The syntax (from ARM 7.4) is:

  linkage-specification:
      extern string-literal { declaration-list    }
                                              opt
      extern string-literal declaration

Since linkage specifications nest, the current linkage specifier is saved
in in a local variable, the new one is established by updating a global
variable, the declaration(s) are processed, and then the original linkage
specifier is restored.
*/
{
  an_extern_linkage  saved_linkage;
  char               *str;
  a_boolean          err = FALSE;

  db_enter(3, "linkage_specification");
  if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
    error(ec_linkage_specifier_not_allowed);
    err = TRUE;
  }  /* if */
  /* Advance to the string literal. */
  (void)get_token();
  str = const_for_curr_token.variant.string.value;
  /* ARM 7.4 specifies that the strings "C" and "C++" must be supported,
     but that implementations are permitted to add others, such as "Ada"
     or "FORTRAN".  If changes are made here to support other strings, be
     sure to update the name linkage kind enumeration. */
  /* Save the current default linkage. */
  saved_linkage = def_external_linkage;
  if (strcmp(str, "C") == 0) {
    if (!err) {
      def_external_linkage.kind = (a_name_linkage_kind)nlk_external;
      def_external_linkage.is_explicit = TRUE;
    }  /* if */
  } else if (strcmp(str, "C++") == 0) {
    if (!err) {
      def_external_linkage.kind =
			     (a_name_linkage_kind)nlk_cplusplus_external;
      def_external_linkage.is_explicit = TRUE;
    }  /* if */
  } else {
    /* Leave def_external_linkage unmodified. */
    error(ec_bad_linkage_specifier);
  }  /* if */
  (void)get_token();
  /* If a brace enclosed declaration list follows, call declaration
     repeatedly.  If no brace follows, call declaration just once to pick
     up the rest of the current declaration. */
  if (curr_token == tok_lbrace) {
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Go through the declarations. */
    while (curr_token != tok_rbrace && curr_token != tok_end_of_source) {
      declaration(function_definition_allowed, /*extern_implied=*/FALSE,
                  is_old_style_param_decl, param_id_list);
    }  /* while */
    remove_stop_token(tok_rbrace);
    (void)required_token(tok_rbrace, ec_exp_rbrace);
  } else {
    /* Just one declaration is governed by this linkage specifier.  If no
       storage class is specified it is as though "extern" were specified --
       this is an interpretation of the sentence in ARM 7.4 asserting, "An
       object defined withing an `extern "C" {...}' construct is still defined
       and not just declared," and of the example following it, where without
       the braces the variable is not defined. */
    declaration(function_definition_allowed, /*extern_implied=*/TRUE,
                is_old_style_param_decl, param_id_list);
  }  /* if */
  /* Restore the default linkage to the value it had before the declaration
     (or declaration list) was processed. */
  def_external_linkage = saved_linkage;

  db_exit();
}  /* linkage_specification */


void handler_declaration(a_statement_ptr     try_block_stmt,
                         a_source_position*  catch_pos)
/*
Process a handler declaration:

  "catch" "(" exception-declaration ")" compound-statement

try_block_stmt is a pointer to the try-block statement to which the catch
clause is to be attached.  catch_pos is the source position of "catch".
*/
{
  a_handler_ptr      handler, prev_handler;
  a_type_ptr         type_ptr = NULL, bottom_derived_type;
  a_storage_class    storage_class;
  a_decl_flag_set    dso_flags, do_flags;
  a_symbol_ptr       sym;
  a_symbol_locator   locator;
  a_source_position  decl_pos;
  a_routine_ptr      cctor, dtor;
  a_param_type_ptr   ptp;
  a_dynamic_init_ptr dip;
  a_source_sequence_entry_ptr
                     declarator_ssep = NULL;

  db_enter(3, "handler_declaration");
  /* Push the scope for the handler before processing the exception
     declaration to assure that the scope of the handler's parameter is the
     same as that of the handler's compound statement block. */
  (void)push_scope((a_scope_kind)sck_block, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL,
                   (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                   (a_template_arg_ptr)NULL);
  /* Allocate the handler. */
  handler = alloc_handler();
  /* Set the assoc_handler field of the IL scope entry. */
  set_block_scope_handler(handler);
  set_stmt_source_position(handler->catch_position, *catch_pos);
  if (required_token(tok_lparen, ec_exp_lparen)) {
    decl_pos = pos_curr_token;
    if (curr_token == tok_ellipsis) {
      /* NULL parameter. */
      (void)get_token();
    } else {
      if (curr_token != tok_identifier &&
          !is_decl_start(/*expr_context=*/FALSE,
                         /*real_declarator_allowed=*/TRUE)) {
        add_stop_token(tok_rparen);
        syntax_error(ec_missing_exception_declaration);
        type_ptr = error_type();
        set_to_error_locator(locator);
        remove_stop_token(tok_rparen);
      } else {
        (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_EMPTY_DECL_SPECIFIERS_ALLOWED),
                              &dso_flags, &storage_class, &type_ptr);
        if (dso_flags & DSO_DEFINES_SOMETHING) {
          /* Definition of a class, struct, union, or enum type is not
             allowed. */
          pos_error(ec_type_definition_not_allowed, &decl_pos);
        } else if (dso_flags & DSO_NO_DECL_SPECIFIERS) {
          /* Missing type specifier. */
          pos_error(ec_missing_exception_declaration, &decl_pos);
          type_ptr = error_type();
        } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
          /* Implicit int. */
          pos_warning(ec_missing_type_specifier, &decl_pos);
        }  /* if */
        sym = NULL;
        if (is_abstract_or_real_declarator_start()) {
          declarator(DI_REAL_DECLARATOR_ALLOWED |
                       DI_ABSTRACT_DECLARATOR_ALLOWED,
                     &do_flags, type_ptr,
                     /*member_parent_type=*/(a_type_ptr)NULL, &locator,
                     &type_ptr, &bottom_derived_type, &declarator_ssep,
                     (a_func_info_block_ptr)NULL);
          if (do_flags & DO_REAL_DECLARATOR_SCANNED) {
            sym = enter_symbol((a_symbol_kind)sk_variable, &locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/FALSE);
          }  /* if */
        }  /* if */
        if (!exceptions_enabled) {
          /* Don't bother with the semantic checks on the handler type.  Set
             type to error type to avoid inappropriate errors downstream. */
          type_ptr = error_type();
        } else if (!is_error_type(type_ptr)) {
          /* Force instantiation of template class. */
          check_for_uninstantiated_template_class(type_ptr);
          /* Adjust the type if necessary (for example, "array of x"
             becomes "pointer to x"). */
          adjust_parameter_type(&type_ptr);
          if (is_incomplete_type(type_ptr)) {
            /* Incomplete type is not allowed. */
            pos_error(ec_incomplete_type_not_allowed, &decl_pos);
            type_ptr = error_type();
          } else {
            /* Mark the type as having been used in an exception.  (Also,
               if it "contains" any classes, they are marked as requiring
               external linkage.) */
            set_used_in_exception_flag(type_ptr);
            if (is_or_contains_local_type(type_ptr)) {
              /* Exception types, if they have linkage at all, must have
                 external linkage; however, it is possible to write a useful
                 program in which a local type is thrown and caught -- e.g.,
                   void f() {
                     class A { ... };
                     try { ... throw A ... }
                     catch (A) { ... }
                   }
                 We still issue a diagnostic, since there are other cases
                 (not necessarily detectable by the compiler) in which local
                 types would be problematic. */
              pos_remark(ec_local_type_used_in_exception, &decl_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Create a variable for the handler parameter, even if there's no
           explicit name. */
        handler->parameter = make_handler_parameter(type_ptr);
        /* Update the symbol, if there is one. */
        if (sym != NULL) {
          sym->variant.variable.ptr = handler->parameter;
          set_source_corresp(&(handler->parameter->source_corresp), sym);
          mark_defined(sym, &locator.source_position);
          mark_variable_value_set(sym);
        }  /* if */
        /* A handler parameter is initialized by the run-time when the
           handler is invoked.  Create the dynamic init entry to represent
           the initialization. */
        if (is_class_struct_union_type(type_ptr)) {
          /* Classes may require the use of a copy constructor. */
          a_boolean          bitwise_copy;
          a_source_position  pos;

          if (sym != NULL) {
            pos = sym->decl_position;
          } else {
            pos = pos_curr_token;
          }  /* if */
          cctor = select_copy_constructor(type_ptr,
                                          /*const_object_required=*/FALSE,
                                          /*volatile_object_okay=*/FALSE,
                                          &pos, type_ptr, &bitwise_copy,
                                          /*evaluated=*/TRUE,
                                          /*suppress_access_check=*/TRUE);
          check_assertion((cctor == NULL) == bitwise_copy); 
          dtor = select_destructor(type_ptr, type_ptr, &pos,
                                   /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                                   /*suppress_access_check=*/TRUE);
        } else {
          /* Non classes require only bitwise copying. */
          cctor = dtor = NULL;
        }  /* if */
        if (cctor != NULL) {
          /* A copy constructor was located.  Create a dynamic-init entry
             to point to it. */
          ptp = (skip_typerefs(cctor->type))->
                                   variant.routine.extra_info->param_type_list;

          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
          dip->variant.constructor.ptr = cctor;
          /* We need to copy the default arg expressions of the second and
             subsequent parameters (if any) of the copy constructor.  The
             first param is ignored even if it is declared to have a default
             arg. */
          ptp = ptp->next;
          dip->variant.constructor.args = copy_default_arg_expr_list(ptp);
          /* Only at runtime is the source known. */
          dip->variant.constructor.
                             is_copy_constructor_with_implied_source = TRUE;
        } else {
          /* A bitwise copy is all that is required. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_bitwise_copy);
        }  /* if */
        dip->variable = handler->parameter;
        dip->destructor = dtor;
        handler->dynamic_init = dip;
      }  /* if */
    }  /* if */
    prev_handler = try_block_stmt->variant.try_block.handlers;
    if (prev_handler == NULL) {
      /* This is the first handler declared for this try block. */
      try_block_stmt->variant.try_block.handlers = handler;
    } else {
      a_boolean  masked = FALSE;
      /* Make a pass over the previously declared handlers in this try block
         to do error checking and locate the end of the list, where the new
         handler will be added. */
      for (;;) {
        if (!exceptions_enabled) {
          /* Don't bother with semantic checks. */
        } else if (masked) {
          /* One "masking" diagnostic has already been issued -- there's no
             point in putting out another. */
        } else if (type_ptr == error_type()) {
          /* No need to check for masking in this case. */
        } else if (prev_handler->parameter == NULL) {
          /* Anything following a default handler is masked by it. */
          pos_error(ec_masked_by_default_handler, &decl_pos);
          masked = TRUE;
        } else if (handler->parameter == NULL) {
          /* Current handler is a default handler -- it can only be masked by
             another default handler. */
        } else if (prev_handler->parameter->type == error_type()) {
          /* No need to check for masking in this case. */
        } else if (type_masks_handler_param_type(prev_handler->parameter->type,
                                                 type_ptr)) {
          /* The type of prev_handler assures that handler will never be
             called, because it masks current handler's type.  See ARM 15.4. */
          pos_ty_error(ec_masked_by_handler, &decl_pos,
                       prev_handler->parameter->type);
          masked = TRUE;
        }  /* if */
        if (prev_handler->next == NULL) break;
        prev_handler = prev_handler->next;
      }  /* for */
      prev_handler->next = handler;
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  /* Parse the body of the handler. */
  handler->statement = compound_statement(/*at_function_level=*/FALSE,
                                          /*explicit_return_type=*/FALSE,
                                          /*is_catch_clause=*/TRUE);
  /* pop_scope is called from compound_statement processing. */
  db_exit();
}  /* handler_declaration */


an_asm_entry_ptr asm_declaration(a_boolean  asm_decl_allowed)
/*
Scan an asm declaration, create an entry to represent it in the IL, and
return a pointer to the asm entry.  An asm declaration is specified as
follows in the ARM:

  asm ( string-literal ) ;

We infer that it can appear as a declaration at file scope, function scope,
and block scope.  It can also appear as a block of executable code, so
in C mode, where declarations and executable statements may not be mingled,
an asm "declaration" is actually treated as an executable statement.
*/
{
  a_constant        asm_string;
  an_asm_entry_ptr  ap = NULL;
  a_source_position asm_pos;

  db_enter(3, "asm_declaration");
  check_assertion(curr_token == tok_asm);
  if (!asm_decl_allowed) {
    /* An asm declaration is not allowed in the current scope. */
    error(ec_asm_not_allowed);
  }  /* if */
  copy_source_position(pos_curr_token, asm_pos);
  /* Skip past the "asm". */
  (void)get_token();
  /* Check for and skip the opening parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the enclosed string. */
  if (curr_token != tok_string_literal) {
    syntax_error(ec_exp_asm_string);
    set_error_constant(&asm_string);
  } else {
    copy_constant(&const_for_curr_token, &asm_string);
    (void)get_token();
  }  /* if */
  /* Check for and skip the closing parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  /* Check for and skip the semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  /* Update the IL. */
  if (asm_decl_allowed) {
    ap = alloc_asm_entry();
    ap->asm_string = alloc_unshared_constant(&asm_string);
    copy_source_position(asm_pos, ap->source_corresp.decl_position);
    /* Add the asm entry to the list for the current scope. */
    add_to_asm_entries_list(ap);
  }  /* if */

  db_exit();
  return ap;
}  /* asm_declaration */


/*
Local macro for the routine "declaration".  Does any remove_stop_token
calls that have not yet been done.  Useful in ensuring that all the stop
tokens get removed, especially when the internal flow is complicated by
error cases.
*/
#define remove_all_local_stop_tokens()                                \
{ if (need_semicolon_remove_stop_token) {                             \
    remove_stop_token(tok_semicolon);                                 \
    need_semicolon_remove_stop_token = FALSE;                         \
  }  /* if */                                                         \
  if (need_comma_remove_stop_token) {                                 \
    remove_stop_token(tok_comma);                                     \
    need_comma_remove_stop_token = FALSE;                             \
  }  /* if */                                                         \
  if (need_assign_remove_stop_token) {                                \
    remove_stop_token(tok_assign);                                    \
    need_assign_remove_stop_token = FALSE;                            \
  }  /* if */                                                         \
  if (need_lbrace_remove_stop_token) {                                \
    remove_stop_token(tok_lbrace);                                    \
    need_lbrace_remove_stop_token = FALSE;                            \
  }  /* if */                                                         \
}  /* remove_all_local_stop_tokens */


static void declaration(a_boolean      function_definition_allowed,
                        a_boolean      extern_implied,
                        a_boolean      is_old_style_param_decl,
                        a_param_id_ptr param_id_list)
/*
Scan a declaration (standard, 3.5).  If function_definition_allowed is TRUE,
alternatively scan a function-definition (3.7.1).  With that flag TRUE, this
routine also corresponds to an external-declaration (3.7).  param_id_list
is non-NULL if this declaration is for an old-style function parameter; in 
that case, the identifier declared must be on the list.

Syntax:

3.7    external-declaration:
		function-definition
		declaration
3.7.1  function-definition:
		declaration-specifiers    declarator declaration-list
                                      opt                            opt
		    compound-statement
3.5    declaration:
		declaration-specifiers init-declarator-list    ;
                                                           opt
3.5    init-declarator-list:
		init-declarator
		init-declarator-list , init-declarator
3.5    init-declarator:
		declarator
		declarator = initializer

This routine is used in scanning file-scope declarations (of types,
variables, and functions), old-style parameter declarations, and declarations
of local variables (and types, etc.) of functions and in blocks.
*/
{
  a_boolean         local_is_old_style_param_decl;
  a_storage_class   storage_class, local_storage_class;
  a_type_ptr        type_ptr, old_type;
  a_type_ptr	    local_type_ptr;
  a_boolean         has_explicit_type_specifier;
  a_boolean	    declares_something;
  a_boolean	    defines_something;
  a_decl_flag_set   dso_flags, do_flags;
  a_decl_flag_set   dsi_flags, di_flags;
  a_symbol_ptr      symbol_ptr, ext_sym;
  a_boolean	    decl_specifiers_omitted = FALSE;
  a_boolean         is_function, is_main_function;
  a_boolean         is_constructor_or_destructor;
  a_boolean         is_static_data_member;
  a_symbol_locator  locator;
  a_param_id_ptr    param_id;
  a_type_ptr        bottom_derived_type;
  a_func_info_block func_info;
  a_boolean         top_declarator_type_is_function;
  an_id_linkage_kind
                    linkage;
  a_boolean         has_initializer;
  a_boolean         has_parenthesized_initializer;
  a_boolean         err = FALSE;
  a_boolean         decl_start;
  a_boolean         dangling_type_specifier = FALSE;
  a_boolean         inline_specified;
  a_source_position decl_start_pos, declarator_pos;
  a_token_kind      next_tok;
  a_boolean         need_semicolon_remove_stop_token = FALSE;
  a_boolean         need_comma_remove_stop_token     = FALSE;
  a_boolean         need_assign_remove_stop_token    = FALSE;
  a_boolean         need_lbrace_remove_stop_token    = FALSE;
  a_boolean         is_variable_def, incomplete_type_error_reported;
  a_boolean         is_tentative_definition;
  a_variable_ptr    var_ptr;
  a_source_sequence_entry_ptr
                    declarator_ssep = NULL;
#if ASM_FUNCTION_ALLOWED
  a_boolean         is_asm_function = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */

  db_enter(3, "declaration");

  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, decl_start_pos);
  if (C_dialect == C_dialect_cplusplus) {
    if (curr_token == tok_extern && next_token() == tok_string_literal) {
      /* This looks like a C++ linkage specification, which is "extern"
         followed by a string literal (e.g., "C++" or "C"). */
      linkage_specification(function_definition_allowed,
                            is_old_style_param_decl, param_id_list);
      goto return_point;
    } else if (curr_token == tok_template) {
      /* Do the processing required for a template declaration.  */
      symbol_ptr = template_declaration(&defines_something);
      if (symbol_ptr != NULL && defines_something &&
          (symbol_ptr->kind == (a_symbol_kind)sk_function_template ||
           symbol_ptr->kind == (a_symbol_kind)sk_member_function)) {
        /* No trailing semicolon expected for a function template. */
      } else {
        /* This should be a class template -- check for final semicolon. */
        (void)required_token(tok_semicolon, ec_exp_semicolon);
      }  /* if */
      goto return_point;
    }  /* if */
  }  /* if */
  add_stop_token(tok_semicolon);
  need_semicolon_remove_stop_token = TRUE;
  if (curr_token == tok_asm) {
#if ASM_FUNCTION_ALLOWED
    if (function_definition_allowed && next_token() != tok_lparen) {
      is_asm_function = TRUE;
      /* Skip over the "asm". */
      (void)get_token();
    } else {
#endif /* ASM_FUNCTION_ALLOWED */
      /* Scan the asm declaration. */
      (void)asm_declaration(/*asm_decl_allowed=*/!is_old_style_param_decl);
      goto return_point;
#if ASM_FUNCTION_ALLOWED
    }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  }  /* if */

  if (C_dialect == C_dialect_cplusplus) {
    /* Check for and discard declarations of the form "overload f;". */
    if (check_for_overload_anachronism()) goto return_point;
  }  /* if */
  /* Set the flags for calling decl_specifiers. */
  decl_start = is_decl_start(/*expr_context=*/FALSE,
                             /*real_declarator_allowed=*/TRUE);
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED;
  /* Within a non-block linkage specification no storage class is allowed
     (inferred from ARM 7.4). */
  if (!extern_implied) dsi_flags |= DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
  if (is_old_style_param_decl) {
    dsi_flags |= DSI_IS_PARAMETER;
  } else {
    dsi_flags |= DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
    if (function_definition_allowed) {
      dsi_flags |= DSI_EMPTY_DECL_SPECIFIERS_ALLOWED;
      /* "inline" is allowed only on function declarations at file scope. */
      if (!extern_implied) dsi_flags |= DSI_INLINE_ALLOWED;
    } else {
      /* A "vacuous declaration" of a class, struct, or union only makes sense
         when we are not at file scope. */
      dsi_flags |= DSI_VACUOUS_TAG_DECL_ALLOWED;
    }  /* if */
  }  /* if */
  /* Scan the initial declaration specifiers (including storage class,
     type specifiers, and type qualifiers).  For a function definition,
     the specifiers can be omitted entirely. */
  if (!decl_start) {
    if (function_definition_allowed && is_declarator_start()) {
      /* Function definition with omitted specifiers. */
    } else {
      /* Look for some cases that are obviously not the start of a declaration,
         and give a more specific "Expected a declaration" message. */
      if (curr_token == tok_semicolon) {
        /* An empty declaration is ignored (as an extension in ANSI mode). */
        if (strict_ansi_mode) {
          diagnostic(strict_ansi_error_severity, ec_extra_semicolon);
        } else {
          remark(ec_extra_semicolon);
        }  /* if */
      } else {
        if (curr_token == tok_lbrace) {
          /* Special error recovery on encountering an open brace: it
             may be the start of a routine. */
          error(ec_exp_declaration);
          flush_until_matching_token();
          if (curr_token == tok_rbrace) (void)get_token();
          if (is_decl_start(/*expr_context=*/FALSE,
                            /*real_declarator_allowed=*/TRUE)) {
            goto continue_with_declaration;
          }  /* if */
        } else {
          syntax_error(ec_exp_declaration);
        }  /* if */
      }  /* if */
      /* Give up on scanning a declaration (assume we're at the end of one). */
      if (curr_token == tok_semicolon) (void)get_token();
      goto return_point;
    }  /* if */
  }  /* if */
continue_with_declaration:
  /* Scan the specifiers. */
  err = decl_specifiers(dsi_flags, &dso_flags, &storage_class, &type_ptr);
  has_explicit_type_specifier = dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
  declares_something = dso_flags & DSO_DECLARES_SOMETHING;
  defines_something = dso_flags & DSO_DEFINES_SOMETHING;
  dangling_type_specifier = dso_flags & DSO_DANGLING_TYPE_SPECIFIER;
  decl_specifiers_omitted = dso_flags & DSO_NO_DECL_SPECIFIERS;
  is_constructor_or_destructor =
                            dso_flags & (DSO_CONSTRUCTOR | DSO_DESTRUCTOR);
  inline_specified = dso_flags & DSO_INLINE;
  /* The declaration can end at this point (";" is next). */
  if (curr_token == tok_semicolon && !decl_specifiers_omitted) {
    if (err) {
      /* There was a previous error, so do not check further. */
    } else if (is_old_style_param_decl && C_dialect != C_dialect_pcc &&
               (declares_something || defines_something)) {
      /* ANSI C does not allow freestanding declarations (as of structs)
         within an old-style parameter list.  pcc, on the other hand,
         will allow something like
            int f(a)
            struct s {int b;};
            struct s a;
            { ... }
      */
      diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                 ec_decl_should_be_of_param);
    } else if (!declares_something && C_dialect == C_dialect_cplusplus &&
               defines_something && type_ptr->kind == (a_type_kind)tk_union &&
               storage_class != (a_storage_class)sc_typedef) {
      /* Special C++ case:  the declaration of an anonymous union.   Do the
         required error checking and special processing, including creation
         of a variable which will represent the anonymous union and with
         which its fields will be aliased. */
      check_assertion(is_unnamed_class_symbol(
                        (a_symbol_ptr)(type_ptr->source_corresp.assoc_info)));
      make_anonymous_union_variable(type_ptr, storage_class);
      /* The anonymous union variable is marked as referenced, as are all
         unnamed entities.  So its type is also marked referenced. */
      type_ptr->source_corresp.referenced = TRUE;
    } else if (extern_implied && is_enum_type(type_ptr)) {
      /* This is a declaration like
                      extern "C" enum E { e1, e2, e3 };
         which is not allowed (inference from ARM 7.4). */
      pos_error(ec_enum_not_allowed, &decl_start_pos);
    } else {
      if (storage_class == (a_storage_class)sc_typedef) {
        /* Typedef declaration with no declarator. */
        an_error_severity  severity = es_warning;

        if (declares_something ||
            (defines_something && is_enum_type(type_ptr))) {
          /* No error on a case like "typedef struct S { int i; };" or
             "typedef enum { red, green, blue };" -- see first constraint,
             Section 3.5 of the ANSI C standard.  However, a warning should
             be issued, since the "typedef" is superfluous. */
        } else {
          /* A case like "typedef int;" or "typedef struct { int i; };" --
             gets a warning by default but may get an error in strict ANSI
             mode. */
          if (strict_ansi_mode) severity = strict_ansi_error_severity;
        }  /* if */
        set_err_pos_to_curr_token();
        diagnostic(severity, ec_missing_typedef_name);
      } else if (!declares_something) {
        if (defines_something &&
            (storage_class != (a_storage_class)sc_unspecified ||
             is_qualified_type(type_ptr))) {
          /* If defines_something is TRUE and declares something is FALSE we
             have a class, struct, union, or enum declaration without a tag
             name and also without the name of an object but with a storage
             class or qualifier -- e.g., "extern struct { int i; };".  Tell
             the user an object name is missing. */
          set_err_pos_to_curr_token();
          diagnostic(C_dialect == C_dialect_cplusplus ? es_error : es_warning,
                     ec_missing_object_name);
        } else {
          /* The specifiers should have declared something or this declaration
             is pointless.  An example would be
                int ;
             An example of a useful declaration with a null declarator is
                struct x {int a;};
             since it declares something (namely x). */
          /* ANSI probably thinks of this as an error, but that seems a bit
             extreme, especially since pcc allows it.  Normally we issue a
             warning, unless the -A option is selected. */
          diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_useless_decl);
        }  /* if */
      } else {
        /* Since declares_something is TRUE, this must be a class, struct,
           union, or enum declaration.  A storage class or qualifier is not
           allowed, nor is "inline". */
        if (storage_class != (a_storage_class)sc_unspecified) {
          diagnostic(C_dialect == C_dialect_cplusplus && strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_storage_class_not_allowed);
        }  /* if */
        if (is_qualified_type(type_ptr)) {
          diagnostic(C_dialect == C_dialect_cplusplus ? es_error : es_warning,
                     ec_const_volatile_not_allowed);
        }  /* if */
        if (inline_specified) {
          error(ec_inline_and_nonfunction);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (dangling_type_specifier && (curr_token != tok_identifier ||
             ((next_tok = next_token()) != tok_semicolon &&
              next_tok != tok_comma && next_tok != tok_assign &&
              next_tok != tok_lbracket && next_tok != tok_lparen))) {
    /* A class, struct, union, or enum declaration was followed by a
       a type specifier keyword or else by an identifier that is a type name
       and that is not followed by a comma, semicolon, or equal sign.  In
       other words, issue a missing-semicolon error on the following:
           class A;
           class B {...} A ...
       where A is probably the start of a new declaration.  However, don't
       put out that error in this case:
           class A;
           class B {...} A;
       where the semicolon following A in the second line makes it clear that
       A was intended to be a declarator. */
    set_err_pos_to_curr_token();
    if (!declares_something) error(ec_exp_identifier);
    error(ec_exp_semicolon);
    goto return_point;
  } else if (curr_token == tok_void && C_dialect == C_dialect_pcc && 
             storage_class == (a_storage_class)sc_typedef &&
             next_token() == tok_semicolon) {
    /* "typedef <something> void;" in pcc mode.  Usually "typedef int void;".
       Shows up in old pre-void-keyword code.  Ignored in pcc mode. */
    set_err_pos_to_curr_token();
    warning(ec_decl_of_void_ignored);
    (void)get_token();
  } else {
    /* Set the various flags for declarator processing. */
    di_flags = DI_REAL_DECLARATOR_ALLOWED;
    if (C_dialect == C_dialect_cplusplus) {
      di_flags |= DI_PARENTHESIZED_INITIALIZER_ALLOWED;
      di_flags |= DI_OPERATOR_NAME_ALLOWED;
      if (storage_class != (a_storage_class)sc_typedef &&
          decl_scope_level == DEPTH_OF_FILE_SCOPE) {
        di_flags |= DI_QUALIFIED_NAME_ALLOWED;
      }  /* if */
    }  /* if */
    if (storage_class == (a_storage_class)sc_typedef) {
      di_flags |= DI_IS_TYPEDEF_DECLARATION;
    }  /* if */
    /* Scan the declarator list. */
    do {
      add_stop_token(tok_comma);
      need_comma_remove_stop_token = TRUE;
      add_stop_token(tok_assign);
      need_assign_remove_stop_token = TRUE;
      if (function_definition_allowed) {
        add_stop_token(tok_lbrace);
        need_lbrace_remove_stop_token = TRUE;
      }  /* if */
      if (curr_token == tok_identifier &&
          (locator_for_curr_id.is_operator_name ||
           locator_for_curr_id.is_conversion_name)) {
        copy_source_position(locator_for_curr_id.source_position,
                             declarator_pos);
      } else {
        copy_source_position(pos_curr_token, declarator_pos);
      }  /* if */
      declarator(di_flags, &do_flags, type_ptr, 
                 /*member_parent_type=*/(a_type_ptr)NULL, &locator,
                 &local_type_ptr, &bottom_derived_type, &declarator_ssep,
                 &func_info);
      is_function = (storage_class != (a_storage_class)sc_typedef &&
                     is_function_type(local_type_ptr));
      is_main_function = FALSE;
      if (is_function && !is_error_locator(locator) &&
          locator.symbol_header->identifier != NULL &&
          (strcmp(locator.symbol_header->identifier, "main") == 0)) {
        /* Recognizing a declaration of function "main" is more than checking
           the identifier. */
        if (C_dialect == C_dialect_cplusplus) {
          if (locator.specific_symbol == NULL ||
              locator.specific_symbol->class_of_which_a_member == NULL) {
            /* Not a member function named "main". */
            func_info.is_main_function = is_main_function = TRUE;
            /* Perform some error checking that is specific to C++. */
            if (def_external_linkage.is_explicit) {
              pos_warning(ec_linkage_specifier_not_allowed, &declarator_pos);
            }  /* if */
            /* "inline" and "static" are not allowed (ARM 3.4). */
            if (storage_class == (a_storage_class)sc_static) {
              pos_error(ec_static_not_allowed, &declarator_pos);
              storage_class =(a_storage_class)sc_unspecified;
            }  /* if */
            if (inline_specified) {
              pos_error(ec_inline_main, &declarator_pos);
              inline_specified = FALSE;
            }  /* if */
          }  /* if */
        } else {
          if (storage_class == (a_storage_class)sc_unspecified ||
              storage_class == (a_storage_class)sc_extern) {
            /* Not a static function named "main".  This is not an option
               in C++ (ARM 3.4). */
            func_info.is_main_function = is_main_function = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      has_parenthesized_initializer = do_flags & DO_PARENTHESIZED_INITIALIZER;
      /* top_declarator_type_is_function is TRUE if the fact that this is a
         function is derived from the declarator and not from a typedef.  It
         is sufficient that the result type is a function type and a
         declarator was scanned. */
      top_declarator_type_is_function = (is_function &&
				         local_type_ptr != type_ptr);
      if (is_function && !top_declarator_type_is_function &&
          cfront_compatibility_mode) {
        a_type_ptr                     tp = skip_typerefs(local_type_ptr);
        a_routine_type_supplement_ptr  rtsp = tp->variant.routine.extra_info;

        /* Check for the declaration of a function with a typedef type that
           is supposed to be used only for pointer-to-member declarations
           (and only in cfront compatibility mode). */
        if (rtsp->implicit_this_param_type != NULL) {
          /* It must be one of these special member function typedefs.  Issue
             an error, since this appears to be a function declaration,
             not a pointer to member declaration. */
          pos_sy_error(ec_bad_use_of_ptr_to_member_typedef, &decl_start_pos,
                       (a_symbol_ptr)(make_unqualified_type(local_type_ptr)->
                                                   source_corresp.assoc_info));
          /* Replace the type with one that does not have an implicit
             this param. */
          local_type_ptr = alloc_type((a_type_kind)tk_routine);
          copy_routine_type_with_param_types(tp, local_type_ptr);
          local_type_ptr->variant.routine.extra_info->
                                       implicit_this_param_type = NULL;
        }  /* if */
      }  /* if */
      if (C_dialect == C_dialect_cplusplus && defines_something) {
        /* The ARM (8.2.5) explicitly prohibits defining a type in a
           function return type.  This is taken to apply to pointer-to-function
           type declarations as well to the function declarations. */
        a_boolean  is_function_type_decl = is_function;
        if (!is_function_type_decl) {
          a_type_ptr  tp = local_type_ptr;
          for (;;) {
            if (is_ptr_or_ref_type(tp)) {
              /* Get type pointed to and continue. */
              tp = type_pointed_to(tp);
            } else if (is_ptr_to_member_type(tp)) {
              /* Get member type and continue. */
              tp = pm_member_type(tp);
            } else {
              /* No function type can be involved.  Stop looping. */
              break;
            }  /* if */
          }  /* for */
          is_function_type_decl = is_function_type(tp);
        }  /* if */
        if (is_function_type_decl) {
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &decl_start_pos);
        }  /* if */
      }  /* if */
      local_storage_class = storage_class;
      local_is_old_style_param_decl = is_old_style_param_decl;
      /* If this is a parameter (old-style), make sure it appears on
         the param_id_list.  Also adjust the type if necessary
         (for example, "array of x" becomes "pointer to x"). */
      if (local_is_old_style_param_decl) {
        if (local_storage_class == (a_storage_class)sc_typedef) {
          if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
            pos_error(ec_decl_should_be_of_param, &decl_start_pos);
            set_to_error_locator(locator);
          } else {
            pos_warning(ec_decl_should_be_of_param, &decl_start_pos);
          }  /* if */
          local_is_old_style_param_decl = FALSE;
        } else {
          param_id = param_id_on_list(&locator, param_id_list);
          if (param_id == NULL) {
            /* The identifier was not found on the list. */
            error(ec_decl_should_be_of_param);
            /* Enter the declared object as a variable rather than as a
               parameter.  */
            local_is_old_style_param_decl = FALSE;
          } else if (param_id->type != NULL) {
            /* Parameter has already been declared. */
            str_error(ec_id_already_declared,
                      locator.symbol_header->identifier);
          } else {
            /* When the parameter name was listed (but not yet actually
               declared) the sk_parameter symbol was created but not entered
               in the symbol table.  Now that it is explicitly declared, add
               it to the function prototype scope; it will later be moved
               to the function scope. */
            reenter_symbol(param_id->symbol, decl_scope_level,
                           /*suppress_error=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
            param_id->source_sequence_entry = declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* if */
          adjust_parameter_type(&local_type_ptr);
          is_function = top_declarator_type_is_function = FALSE;
          /* For pcc compatibility, promote float parameters to double. */
          if (C_dialect == C_dialect_pcc) {
            promote_float_to_double(local_type_ptr);
          }  /* if */
        }  /* if */
      }  /* if */
      /* See if any type qualifiers were specified, and if they are okay. */
      check_type_qualifiers(&local_type_ptr);
      if (need_lbrace_remove_stop_token) {
        remove_stop_token(tok_lbrace);
        need_lbrace_remove_stop_token = FALSE;
      }  /* if */
#if ASM_FUNCTION_ALLOWED
      if (is_asm_function) {
        /* This is an asm function.  Go process it. */
        remove_all_local_stop_tokens();
        asm_function_definition(&locator, local_type_ptr, 
                                top_declarator_type_is_function,
                                &func_info, local_storage_class);
        goto return_point;
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      if (is_function && local_storage_class != (a_storage_class)sc_typedef) {
        if (local_storage_class != (a_storage_class)sc_unspecified &&
            local_storage_class != (a_storage_class)sc_extern &&
            local_storage_class != (a_storage_class)sc_static) {
          /* The storage class of a function must be extern or static. */
          pos_error(ec_bad_function_storage_class, &declarator_pos);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (locator.specific_symbol != NULL &&
            locator.specific_symbol->class_of_which_a_member != NULL) {
          /* This is the definition of a static member function.  No storage
             class specifier (not even "static") is permitted. */
          if (local_storage_class != (a_storage_class)sc_unspecified) {
            if (local_storage_class == (a_storage_class)sc_static &&
                inline_specified) {
              /* Just give a warning on this.  The storage class designation
                 is taken to be redundant, since all "inline" member functions
                 (both static and nonstatic, in the sense applied to member
                 functions) are "static" (in the sense of having internal
                 linkage). */
              pos_warning(ec_storage_class_not_allowed, &decl_start_pos);
            } else {
              pos_error(ec_storage_class_not_allowed, &decl_start_pos);
            }  /* if */
          }  /* if */
          /* Set the storage class to sc_static. */
          local_storage_class = (a_storage_class)sc_static;
        }  /* if */
      }  /* if */
      /* Check for restrictions on use of the "inline" specifier. */
      if (inline_specified) {
        if (!is_function) {
          /* Not a function declaration. */
          pos_error(ec_inline_and_nonfunction, &declarator_pos);
        } else {
          /* Set the storage class to sc_static. */
          local_storage_class = (a_storage_class)sc_static;
          func_info.is_inline = TRUE;
        }  /* if */
      }  /* if */
      /* If the thing declared is a function, and if the token following looks
         like it could be part of a function-definition, go scan that. */
      if (function_definition_allowed && is_function &&
          local_storage_class != (a_storage_class)sc_typedef &&
          curr_token != tok_semicolon && curr_token != tok_comma &&
          curr_token != tok_assign && curr_token != tok_end_of_source) {
        if (!has_explicit_type_specifier && !is_main_function) {
          /* Function with no explicitly specified return type.  Issue a
             remark (except in pcc mode and except for C++ constructors,
             destructors, and conversion operators). */
          if (C_dialect != C_dialect_pcc) {
            if (C_dialect != C_dialect_cplusplus ||
                (!is_constructor_or_destructor &&
                 !locator.is_conversion_name)) {
              pos_remark(ec_missing_type_specifier, &declarator_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        remove_all_local_stop_tokens();
        func_info.is_definition = TRUE;
        func_info.function_type_from_typedef =
                                    !top_declarator_type_is_function;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        func_info.declarator_ssep = declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        function_definition(&locator, local_type_ptr, &func_info,
                            local_storage_class, has_explicit_type_specifier);
        goto return_point;
      }  /* if */
      /* Not a function definition, must be a declaration. */
      /* After a declaration has been scanned, it is no longer possible
         that the next thing is a function definition. */
      function_definition_allowed = FALSE;
      is_static_data_member = FALSE;
      if (locator.specific_symbol != NULL &&
          locator.specific_symbol->class_of_which_a_member != NULL) {
        if (is_function) {
          /* A qualified name that identifies a function is allowed only when
             the function body is present. */
          if (is_member_function_symbol(locator.specific_symbol)) {
            pos_error(ec_member_function_redecl_outside_class,
                      &declarator_pos);
          } else {
            pos_sy_error(ec_not_compatible_with_previous_decl,
                         &declarator_pos, locator.specific_symbol);
          }  /* if */
          set_to_error_locator(locator);
        } else {
          /* Assume that qualified names that are not functions refer to static
             data members. */
          is_static_data_member = TRUE;
        }  /* if */
      }  /* if */
      /* Issue diagnostics on missing type specifiers, etc. */
      if (!is_main_function) {
        if (decl_specifiers_omitted &&
            (C_dialect != C_dialect_cplusplus || !is_function)) {
          /* In ANSI C declaration specifiers can only be entirely omitted in
             a function definition.  This is possibly an undefined typedef
             name at the start of a declaration, so enter an error symbol
             instead of the name given.  In pcc mode the declaration is taken
             as a declaration of an int variable.  (In C++ the decl specifiers
             may be omitted on function declarations and definitions.) */
          if (C_dialect == C_dialect_pcc) {
            pos_warning(ec_missing_decl_specifiers, &declarator_pos);
          } else {
            pos_error(ec_missing_decl_specifiers, &declarator_pos);
          }  /* if */
        } else if (!has_explicit_type_specifier) {
          if (is_function) {
            /* Function with no explicitly specified return type.  Issue a
               remark (except in pcc mode and except for C++ constructors,
               destructors, and conversion operators). */
            if (C_dialect != C_dialect_pcc) {
              if (C_dialect != C_dialect_cplusplus ||
                  (!is_constructor_or_destructor &&
                   !locator.is_conversion_name)) {
                pos_remark(ec_missing_type_specifier, &declarator_pos);
              }  /* if */
            }  /* if */
          } else {
            /* For implicitly typed nonfunction declarations (variables,
               typedefs, etc.) issue a warning in all modes. */
            pos_warning(ec_missing_type_specifier, &declarator_pos);
          }  /* if */
        }  /* if */
      }  /* if */
      if (top_declarator_type_is_function) {
        a_param_id_ptr  pid = func_info.param_id_list;

        if (pid != NULL) {
          /* If the function has a non-empty old-style identifier list of
             parameters, a body should have been present. */
          if (!local_type_ptr->variant.routine.extra_info->prototyped) {
            error(ec_param_id_list_needs_function_def);
          }  /* if */
          /* After updating xref information on each symbol, free the list
             of parameter identifiers -- they're not needed if there's no
             definition. */
          for (; pid != NULL; pid = pid->next) {
            if (pid->symbol != NULL) {
              mark_declared(pid->symbol, &pid->symbol->decl_position);
            }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
            if (pid->source_sequence_entry != NULL) {
              remove_from_source_sequence_list(&pid->source_sequence_entry,
                                               (a_type_ptr)NULL);
            }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* for */
          free_param_id_list(&(func_info.param_id_list));
        }  /* if */
      }  /* if */
      /* Do some checking of storage classes, but not for typedefs. */
      if (local_storage_class != (a_storage_class)sc_typedef) {
        /* auto and register may not appear in a file-scope level
           declaration (3.7, constraints). */
        if (decl_scope_level == DEPTH_OF_FILE_SCOPE &&
            (local_storage_class == (a_storage_class)sc_auto ||
             local_storage_class == (a_storage_class)sc_register)) {
          error(ec_bad_file_scope_storage_class);
          local_storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (is_function) {
          /* A function with block scope (i.e., within an sck_function or
             sck_block scope) can only have an explicit storage class of
             extern (3.5.1). */
          if ((scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_function ||
               scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_block) &&
              local_storage_class != (a_storage_class)sc_unspecified &&
              local_storage_class != (a_storage_class)sc_extern) {
            /* Allow "static" in all C modes except strict ANSI. The function
               will be entered at the file scope as static.  This is an
               extension to ANSI C.  Do not allow at all in C++ mode. */
            if (local_storage_class == (a_storage_class)sc_static) {
              if (C_dialect == C_dialect_cplusplus) {
                error(ec_block_scope_function_must_be_extern);
                /* id_linkage doesn't expect block level statics in
                   C++ mode. */
                local_storage_class = (a_storage_class)sc_extern;
              } else {  /* a C dialect */
                /* This is an extension to ANSI C so produce a diagnostic
                   in strict ANSI C mode. */
                if (strict_ansi_mode) {
                  diagnostic(strict_ansi_error_severity,
                             ec_block_scope_function_must_be_extern);
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          /* Make a function with no body (yet?) have a storage class of
             extern (for external linkage) or static (for internal linkage). */
          if (local_storage_class != (a_storage_class)sc_static) {
            local_storage_class = (a_storage_class)sc_extern;
          }  /* if */
        } else if (is_static_data_member) {
	  /* A static data member (or, illegally, a qualified name referring
	     to another kind of member).  Leave the storage class set to
	     sc_unspecified even if we are not at file scope. */
        } else {
          /* Not a function, not a typedef, therefore a variable or 
             parameter.  If the storage class is unspecified, and
             we are not at file scope, use a storage class of auto. */
          if (local_storage_class == (a_storage_class)sc_unspecified) {
            if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
              /* We are not at file scope, so an unspecified storage class
                 means auto. */
              local_storage_class = (a_storage_class)sc_auto;
            } else if (extern_implied) {
              /* This must be part of an linkage specification declaration.
                 An "extern" storage class is implied (ARM 7.4, comment on
                 p. 118). */
              local_storage_class = (a_storage_class)sc_extern;
            }  /* if */
          }  /* if */
        }  /* if */
        if (!is_function) {
          if (C_dialect == C_dialect_cplusplus) {
            if (is_illegal_abstract_class_type(local_type_ptr)) {
              /* Abstract class objects are prohibited (ARM 10.3). */
              error(ec_abstract_class_object_not_allowed);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* Enter the symbol with the proper type. */
      linkage = idl_none;
      var_ptr = NULL;
      /* Look for optional initializer. */
      remove_stop_token(tok_assign);
      need_assign_remove_stop_token = FALSE;
      if (has_parenthesized_initializer) {
        has_initializer = TRUE;
      } else if (curr_token == tok_assign) {
        has_initializer = TRUE;
#if C_ANACHRONISMS_ALLOWED
      } else if (C_dialect == C_dialect_pcc && is_initializer_start()) {
        /* In pcc mode, the "=" may be omitted (K&R first edition, Appendix A,
           section 17 (Anachronisms)). */
        has_initializer = TRUE;
        warning(ec_old_fashioned_initializer);
#endif /* C_ANACHRONISMS_ALLOWED */
      } else {
        has_initializer = FALSE;
      }  /* if */
      is_variable_def = FALSE;
      is_tentative_definition = FALSE;
      if (local_is_old_style_param_decl) {
        symbol_ptr = param_id->symbol;
        copy_source_position(locator.source_position,
                             symbol_ptr->decl_position);
        param_id->type = local_type_ptr;
        copy_source_position(decl_start_pos, param_id->type_pos);
        param_id->storage_class = local_storage_class;
        /* Note that the creation of the parameter variable, etc., is done
           in decl_parameter, called when the function body is scanned. */
      } else if (local_storage_class == (a_storage_class)sc_typedef) {
        /* A typedef declaration. */
        decl_typedef(&locator, local_type_ptr, &symbol_ptr, declarator_ssep);
      } else if (is_static_data_member) {
        /* A static data member definition. */
        define_static_data_member(&locator, local_storage_class,
                                  local_type_ptr, declarator_ssep,
                                  &symbol_ptr, &linkage);
        var_ptr = symbol_ptr->variant.static_data_member.variable;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        /* All static data member declarations that that pass though this
           code are definitions. */
        is_variable_def = TRUE;
      } else if (is_function) {
        /* A function declaration with no body. */
        decl_var_or_routine(&locator, local_storage_class, local_type_ptr,
                            &func_info, declarator_ssep,
                            /*is_variable_def=*/FALSE, &symbol_ptr,
                            &linkage, &old_type, &ext_sym);
      } else {
        /* A variable declaration. */
        /* Set a flag marking this as a defining declaration, if that's
           appropriate. */
        if (is_old_style_param_decl) {
          /* This flag is TRUE when local_is_old_style_param_decl is FALSE
             in the error case where a name appears in an old-style param
             declaration but for which no corresponding param-id was created.
               void f(i,j) int i, j, k; { }      // Error on "k"
             Treat this as a definition. */
          is_variable_def = TRUE;
        } else if (has_initializer) {
          /* A variable declaration involving an initializer is always
             considered to be a definition. */
          is_variable_def = TRUE;
        } else if (C_dialect == C_dialect_cplusplus) {
          /* In C++ all other variable declarations are definitions, except
             those with a storage class of extern. */
          is_variable_def =
                       (local_storage_class != (a_storage_class)sc_extern);
        } else {
          /* C mode. */
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            if (local_storage_class == (a_storage_class)sc_unspecified ||
                local_storage_class == (a_storage_class)sc_static) {
              /* In C a file scope variable declaration with no storage class
                 or static storage class is called a tentative definition. */
              is_tentative_definition = TRUE;
            }  /* if */
          } else {
            /* In C all local variable declarations are definitions. */
            if (local_storage_class != (a_storage_class)sc_extern) {
              is_variable_def = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        decl_var_or_routine(&locator, local_storage_class, local_type_ptr,
                            (a_func_info_block *)NULL, declarator_ssep,
                            is_variable_def || is_tentative_definition,
                            &symbol_ptr, &linkage, &old_type, &ext_sym);
        var_ptr = symbol_ptr->variant.variable.ptr;
        /* Fetch the type of the symbol again, since it might have been
           changed when reconciled with the original declaration. */
        local_type_ptr = var_ptr->type;
        if (is_old_style_param_decl) {
          /* Error case (described above).  Mark the symbol referenced, to
             suppress subsequent "declared and not referenced" warnings. */
          symbol_ptr->referenced = TRUE;
        }  /* if */
      }  /* if */
      if (is_variable_def && C_dialect == C_dialect_cplusplus) {
        /* At the point at which an object of incomplete template class is
           defined, its class needs to be instantiated.  When its type is
           ref-template-class, the instantiation is also required.  Note that
           in this respect a reference does not behave like a pointer -- in
           the latter case, the instantiation is not required until the pointer
           is dereferenced. */
        a_type_ptr  tp = local_type_ptr;
        if (is_reference_type(tp)) tp = type_pointed_to(tp);
        check_for_uninstantiated_template_class(tp);
      }  /* if */
      incomplete_type_error_reported = FALSE;
      /* Set the error position to the start of the initializer (that is, to
         the "=" if there is one) or to where the initializer should be in
         case there ought to be one. */
      set_err_pos_to_curr_token();
      if (has_initializer) {
        /* Advance past the "=". */
        if (curr_token == tok_assign) (void)get_token();
        /* Now scan the initializer. */
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
            !is_old_style_param_decl) {
          /* Set the storage class of a file-scope initialized variable to
             unspecified (meaning external) or static (meaning internal).
             See 3.7.2. */
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE) {
            if (var_ptr->storage_class == (a_storage_class)sc_extern) {
              var_ptr->storage_class = (a_storage_class)sc_unspecified;
            }  /* if */
          }  /* if */
          /* All initialized variables are considered defined.  This flag
             may have already been set based on storage class and scope
             level. */
          mark_variable_value_set(symbol_ptr);
        }  /* if */
        /* If the symbol is a parameter, the subroutine will generate the
           error.  This is done rather than flagging the error here because
           the subroutine can scan over the initializer expression neatly. */
        initializer(symbol_ptr, &locator.source_position, linkage,
                    has_parenthesized_initializer, is_old_style_param_decl,
                    &incomplete_type_error_reported);
        /* Fetch the type of the symbol again, since it might have been
           changed if it was an incomplete array and was initialized. */
        if (var_ptr != NULL) local_type_ptr = var_ptr->type;
      } else if (is_old_style_param_decl) {
        /* Don't worry about missing initializer. */
      } else if (is_variable_def && !is_error_locator(locator) &&
                 var_ptr->init_kind == (an_init_kind)initk_none) {
        /* Uninitialized variable or static data member is being defined, but
           no explicit initializer was provided.  Do default initialization
           if appropriate (e.g., if a default constructor exists). */
        if (def_initializer(symbol_ptr, &locator.source_position)) {
          /* Default initialization was successful. */
          if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
            mark_variable_value_set(symbol_ptr);
          }  /* if */
        } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
          /* No default initialization, so do some additional checking. */
          check_for_missing_initializer(symbol_ptr, local_type_ptr);
          if (!var_ptr->source_corresp.is_local_to_function ||
              var_ptr->storage_class == (a_storage_class)sc_static) {
            mark_variable_value_set(symbol_ptr);
          }  /* if */
        }  /* if */
      } else if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
                 (local_storage_class == (a_storage_class)sc_extern ||
                  is_tentative_definition)) {
        /* Either:  This is not a definition of a variable but rather an extern
           declaration.  Such a variable may be assumed to be initialized
           at the point of definition, so flag it as "set" (even if it is not
           actually set at the current declaration). */
        /* Or else:  This is a tentative definition (C mode only), which should
           be treated as though it were a definition. */
        mark_variable_value_set(symbol_ptr);
      }  /* if */
      copy_source_position(locator.source_position, error_position);
      if (is_incomplete_type(local_type_ptr)) {
        /* Issue an error on a variable for which this is the defining
           declaration but whose type is incomplete.  Also, in C mode, issue
           an error on a static variable with incomplete type (6.7.2 para 3)
           or a externally linked variable with a tentative definition but an
           uncompletable type (a case like "void i;" at file scope). */
        if (is_variable_def ||
            (is_tentative_definition &&
             (local_storage_class == (a_storage_class)sc_static ||
              is_void_type(local_type_ptr)))) {
          if (!incomplete_type_error_reported) {
            pos_error(ec_incomplete_type_not_allowed,
                      &locator.source_position);
          }  /* if */
          var_ptr->type = error_type();
        }  /* if */
      }  /* if */
      remove_stop_token(tok_comma);
      need_comma_remove_stop_token = FALSE;
      /* Keep scanning the list of declarators. */
    } while (loop_token(tok_comma));
  }  /* if */
  /* Check for final semicolon. */
  (void)required_token(tok_semicolon, ec_exp_semicolon);

return_point:
  /* Do necessary remove_stop_tokens.  Even when there is no error, this
     does the remove_stop_token for tok_semicolon. */
  remove_all_local_stop_tokens();
  db_exit();
  return;
}  /* declaration */


void local_declaration(void)
/*
Scan a block-level declaration.
*/
{
  declaration(/*function_definition_allowed=*/FALSE,
              /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
              (a_param_id_ptr)NULL);
}  /* local_declaration */


void translation_unit(void)
/*
Scan a translation-unit (3.7).  This is the topmost syntactic entity in
a compilation.  The syntax is

3.7    translation-unit:
		external-declaration
		translation-unit external-declaration

*/
{
  if (get_token() == tok_end_of_source) {
    /* A translation unit cannot be empty.  Note that this can happen not
       only for an empty file, but also for a file containing only
       preprocessing directives.  pcc allows an empty source file.
       In ANSI mode, it's allowed as an extension. */
    if (strict_ansi_mode) {
      diagnostic(strict_ansi_error_severity, ec_empty_translation_unit);
    }  /* if */
  } else {
    do {
      declaration(/*function_definition_allowed=*/TRUE,
                  /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
                  (a_param_id_ptr)NULL);
    } while (curr_token != tok_end_of_source);
  }  /* if */
}  /* translation_unit */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
void scan_implicitly_included_template_definition_file(void)
/*
Scan an implicitly included template definition file.  It is just like
scanning a translation-unit, except there's no diagnostic on the empty file.
*/
{
  (void)get_token();
  while (curr_token != tok_end_of_source) {
    declaration(/*function_definition_allowed=*/TRUE,
                /*extern_implied=*/FALSE, /*is_old_style_param_decl=*/FALSE,
                (a_param_id_ptr)NULL);
  }  /* if */
}  /* scan_implicitly_included_template_definition_file */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
