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
#include "class_decl.h"
#include "il.h"
#include "symbol_tbl.h"
#include "statements.h"
#include "lexical.h"
#include "error.h"
#include "cmd_line.h"
#include "types.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "expr.h"
#include "exprutil.h"
#include "target.h"
#include "decl_inits.h"
#include "preproc.h"
#include "types.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */


/* Declarations required because of mutual recursion. */
static void declaration(a_boolean      function_definition_allowed,
                        a_boolean      extern_implied,
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
Macro that is TRUE if the current token is the start of a declarator
(3.5.4 -- real, not abstract).
*/
#define is_declarator_start()                                         \
  (curr_token == tok_identifier || curr_token == tok_star ||          \
   curr_token == tok_lparen ||                                        \
   (C_dialect == C_dialect_cplusplus &&                               \
    (curr_token == tok_ampersand || curr_token == tok_operator)))


static a_boolean is_ptr_to_member_declarator_start(void)
/*
Return TRUE if the current identifier token is the start of a pointer-to-
member declarator (class-name :: *).
*/
{
  a_boolean      is_start = FALSE;
  a_token_cache  token_cache;
  a_type_ptr     class_type;
  a_boolean      is_file_scope_qualifier, has_global_qualifier, err;

  if (get_class_qualifier(&token_cache, &class_type, &is_file_scope_qualifier,
                          &has_global_qualifier, &err)) {
    /* A class qualifier is present.  Note that file scope qualifiers are not
       permitted.  This is a pointer-to-member declarator if the current token
       is a "*". */
    if (curr_token == tok_star) is_start = TRUE;
    /* Back up to the start of the class qualifier.  It will be rescanned
       by the caller. */
    rescan_cached_tokens(&token_cache);
  }  /* if */
  return is_start;
}  /* is_ptr_to_member_declarator_start */


/*
Macro that is TRUE if the current token is the start of an abstract
declarator (3.5.5).
*/
#define is_abstract_declarator_start()                                \
  (curr_token == tok_star || curr_token == tok_lbracket ||            \
   curr_token == tok_lparen ||                                        \
   (C_dialect == C_dialect_cplusplus &&                               \
    ((is_qualified_name_start() &&                                    \
      is_ptr_to_member_declarator_start()) ||                         \
     curr_token == tok_ampersand)))


static a_boolean has_name_of_curr_class(a_symbol_locator  *loc)
/*
Return TRUE if the current scope is a class definition and the name of the
class is the same as the name that loc represents.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
  a_symbol_ptr             class_sym;
  a_boolean                match;

  if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
    match = FALSE;
  } else {
    class_sym = (a_symbol_ptr)ssep->assoc_type->source_corresp.assoc_info;
    match = (loc->symbol_header == class_sym->header);
  }  /* if */
  return match;
}  /* has_name_of_curr_class */


static a_symbol_ptr curr_type_symbol(void)
/*
The current token is an identifier or, in C++, the "::" at the start of a
global qualified name.  If it is the name of a type (a typedef name or,
in C++, the name of a class, struct, union, or enum), return a pointer to
the symbol.  Otherwise, return NULL.  Ambiguity and access control checking
is not done.
*/
{
  a_symbol_ptr assoc_symbol;

  if (curr_token == tok_colon_colon && is_global_new_or_delete()) {
    /* "::new" and "::delete" are not type names. */
    assoc_symbol = NULL;
  } else if (is_ptr_to_member_declarator_start()) {
    /* "class-name::*" is a pointer-to-member declarator, not a type name. */
    assoc_symbol = NULL;
  } else {
    /* Look up the current token identifier, which may be a qualified name.
       Since curr_type_symbol is often called as part of a test of the
       presence of a type name identifier, it is inappropriate to cause a
       projection symbol to be created in the current scope if in fact it
       projects something other than a type name.  It's easier to suppress
       the creation of such gratuitous projections here than to try to ignore
       them in symbol entry later. */
    assoc_symbol = get_normal_id_or_qualified_name(
                                 IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME);
    if (assoc_symbol != NULL && !is_type_symbol(assoc_symbol)) {
      /* Symbol was found, but it is not a type name symbol.  Return NULL. */
      assoc_symbol = NULL;
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_type_symbol */


/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define curr_id_is_type_name() (curr_type_symbol() != NULL)

/*
Macro that is TRUE if the current token is an identifier that represents
the name of a type (a typedef name or, in C++, the name of a class, struct,
union, or enum).  Also works if the current is the "::" at the start of
a global qualified name.
*/
#define is_type_name() (is_qualified_name_start() && curr_id_is_type_name())


a_symbol_ptr curr_tag_symbol(a_symbol_kind tag_kind)
/*
The current token is an identifier.  If it is a tag of the indicated kind,
do ambiguity and access control checking and return a pointer to the tag
symbol.  Otherwise, return NULL.
*/
{
  a_symbol_ptr assoc_symbol;

  /* Look up the current token.  Note that a qualified name is not allowed. */ 
  assoc_symbol = normal_id_lookup(&locator_for_curr_id, IDL_MUST_BE_TAG);
  if (assoc_symbol != NULL) {
    if (assoc_symbol->kind != tag_kind) {
      /* A tag, but the wrong kind of tag (e.g., struct when union is
         required). */
      assoc_symbol = NULL;
    } else {
      if (locator_for_curr_id.is_semivisible_nested_class) {
        /* The symbol in the locator is a nested class that is not visible
           according to the ARM lookup rules but is returned in support of the
           nested class anachronism (ARM 18.3.5).  Issue a warning. */
        sym_warning(ec_nested_class_anachronism,
                    locator_for_curr_id.specific_symbol);
      }  /* if */
      /* Do ambiguity and access control checking on the member. */
      check_ambiguity_and_verify_access(&locator_for_curr_id);
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_tag_symbol */


a_symbol_ptr curr_scope_tag_symbol(a_symbol_kind kind)
/*
The current token is an identifier.  If it represents a tag of the indicated
kind from the current scope, return a pointer to the corresponding symbol.
Otherwise, return NULL.
*/
{
  a_symbol_ptr    sym = symbol_list_from_locator(locator_for_curr_id);
  a_scope_number  scope_number;

  /* Look for a symbol in the current scope for which the kind matches that
     of the scope level specified by the caller. */
  scope_number = scope_stack[decl_scope_level].number;
  for (; sym != NULL; sym = sym->next) {
    if (sym->decl_scope == scope_number && sym->kind == kind) {
      /* Found it. */
      break;
    }  /* if */
  }  /* for */
  return sym;
}  /* curr_scope_tag_symbol */


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


a_boolean is_overload_specifier(void)
/*
Return TRUE if the current token is an overload specifier (C++ anachronism).
Note that "overload" is not a keyword and will not be recognized as a
specifier if the name has been declared.  Called only in C++.
*/
{
  char         *id_name;
  a_boolean    is_overload = FALSE;

  if (curr_token == tok_identifier && !is_error_locator(locator_for_curr_id)) {
    id_name = locator_for_curr_id.symbol_header->identifier;
    if (*id_name == 'o' && strcmp(id_name, "overload") == 0) {
      /* Identifier is "overload" -- check for definition. */
      if (normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS) == NULL) {
        /* The name is not in the symbol table.  Treat is as a keyword. */
        is_overload = TRUE;
      } else if (locator_for_curr_id.is_semivisible_nested_class) {
        /* There is a nested class named "overload" that is visible by
           the nested class anachronism (ARM 18.3.5).  Ignore it. */
        is_overload = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return (is_overload);
}  /* is_overload_specifier */


a_boolean is_decl_start(void)
/*
Return TRUE if the current token looks like the start of a declaration,
i.e., it is the start of a type-specifier, a type-qualifier, or
a storage-class-specifier.  Note that this does not cover the start of
function-definitions, since they can start with the declarator.
*/
{
  a_boolean is_start = FALSE;

  if (is_storage_class()) {
    /* A storage-class-specifier. */
    is_start = TRUE;
  } else if (is_type_start()) {
    /* Is start of type. */
    is_start = TRUE;
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
      /* Cache and bypass the "*" or "&". */
      cache_curr_token(token_cache_ptr);
      (void)get_token();
      if (curr_token == tok_const || curr_token == tok_volatile) {
        /* Qualifier rules out expression. */
        *may_be_expr = FALSE;
        goto done;
      }  /* if */
      /* Keep looping. */
    } else if (is_qualified_name_start() &&
               is_ptr_to_member_declarator_start()) {
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
      if (curr_token == tok_rparen || is_decl_start()) {
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
  } else {
    /* Not a nested declarator.  May be a real declarator. */
    if (is_qualified_name_start() || curr_token == tok_operator) {
      /* Appears to be a real declarator.  But if a real declarator is not
         allowed in the current context, it's probably an expression. */
      if (!real_declarator_allowed) {
        *may_be_decl = FALSE;
        goto done;
      }  /* if */
      /* Scan a qualified name without checking for legal class names.  We
         just want a token sequence that looks right. */
      if (is_qualified_name_start()) {
        /* Cache and bypass an initial "::", if any. */
        if (curr_token == tok_colon_colon) {
          cache_curr_token(token_cache_ptr);
          (void)get_token();
        }  /* if */
        /* Cache and bypass one or more token pairs in which an identifier
           is followed by "::". */
        while (curr_token == tok_identifier &&
               next_token() == tok_colon_colon) {
          cache_curr_token(token_cache_ptr);
          (void)get_token();
          cache_curr_token(token_cache_ptr);
          (void)get_token();
        }  /* while */
      }  /* if */
      /* Now the class or global qualifier, if any, is stripped off. */
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
      add_stop_token(tok_rbracket);
      cache_token_stream(token_cache_ptr);
      remove_stop_token(tok_rbracket);
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
        if (!is_decl_start()) {
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
            default:;
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

The ARM discusses disambiguation in section 6.8.  In general, if a sequence
of tokens looks like a declaration, then it is a declaration, even if it
could also be an expression.  The technique used here involves assuming
a declaration and looking ahead as many tokens as necessary to confirm or
disprove the assumption or, in the case of a more persistent ambiguity, to
decide on the basis of tokens following the "declaration".  Tokens are
cached so that they can be rescanned by the caller.

The caller provides some information about the context, specifically whether,
if it is a declaration, an abstract or real declarator is expected -- or
either.
*/
{
  a_token_cache       token_cache;
  a_stop_token_array  save_stop_token_array;
  a_boolean           may_be_decl = TRUE;
  a_boolean           may_be_expr = TRUE;

  db_enter(3, "f_is_decl_not_expr");
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_rparen);
  /* Initialize the token cache. */
  clear_token_cache(&token_cache);
  /* Scan forward as far as required to determine whether this is a
     declaration.  Each token that is encountered is cached away, so that
     that they can be restored for the actual scan. */
  prescan_declaration(&token_cache, abstract_declarator_allowed,
                      real_declarator_allowed, &may_be_decl, &may_be_expr);
  /* Restore the tokens. */
  rescan_cached_tokens(&token_cache);
  /* Restore the stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  db_exit();
  return may_be_decl;
}  /* f_is_decl_not_expr */


a_boolean check_for_overload_anachronism(void)
/*
Check for the presence of the pseudo-keyword "overload" at the start of
a declaration.  If it is found, pass over it and examine the tokens following.
If a declaration is of the format "overload f;" (or "overload f, g, h;")
just check for syntax errors and discard the entire declaration; in such
cases return TRUE.  Otherwise, return FALSE -- declaration processing will
continue as though "overload" had not been seen.
*/
{
  a_boolean     discard_declaration = FALSE;
  a_token_kind  next_tok;

  if (is_overload_specifier()) {
    /* Issue a diagnostic indicating that "overload" is ignored. */
    warning(ec_overload_ignored);
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
           "keyword" we return to the caller. */
      }  /* if */
    }  /* if */
  }  /* if */
  return discard_declaration;
}  /* check_for_overload_anachronism */


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
  a_type_ptr              temp_type, prev_temp_type, pt;
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
        if (is_object_type(temp_type) || is_pointer_type(temp_type) ||
            (temp_type->kind == (a_type_kind)tk_array &&
             temp_type->variant.array.number_of_elements != 0)) {
          /* Okay. */
        } else if (is_ptr_to_member_type(temp_type) &&
                   pm_member_type(temp_type) == NULL) {
          /* This is an incomplete ptr-to-member type, presumably a
             pointer to member function.  Okay. */
        } else if (is_class_struct_union_type(temp_type)) {
          /* As an extension, allow arrays of incomplete struct or union
             types.  Obviously, these have to be completed before they
             are actually used.  Add the array type to a list of array
             types to be fixed up when struct or union declarations are
             completed. */
          array_of_incomp_struct_or_union = TRUE;
          if (strict_ansi_mode) warning(ec_bad_array_element_type);
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
	if (is_reference_type(skip_typerefs(new_type_ptr))) {
	  /* Pointer to reference is illegal. */
          error(ec_pointer_to_reference);
	  new_type_ptr = error_type();
	}  /* if */
        (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
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
#if 0
        /* Error checking is needed. */
#endif /* if 0 */
        (*bottom_derived_type)->variant.ptr_to_member.type = new_type_ptr;
      } else {
        /* Function type. */
#if CHECKING
        if (tkind != (a_type_kind)tk_routine) {
          internal_error(
            "add_to_derived_type_list: not array/function/pointer/reference");
        }  /* if */
#endif /* CHECKING */
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
             indicate a function (like exit()) that does not return. */
          if (is_void_type(skip_typerefs(new_type_ptr)) &&
              is_volatile_qualified_type(new_type_ptr)) {
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
                  pt = prev_temp_type->variant.array.element_type;
                  break;
                case tk_pointer:
                  pt = prev_temp_type->variant.pointer.type;
                  break;
                case tk_routine:
                  pt = prev_temp_type->variant.routine.return_type;
                  break;
                case tk_typeref:
                  pt = prev_temp_type->variant.typeref.type;
                  break;
                case tk_ptr_to_member:
                  pt = pm_member_type(prev_temp_type);
                  break;
#if CHECKING
                default:
                  internal_error("add_to_derived_type_list: bad type in list");
#endif /* CHECKING */
              }  /* switch */
              if (pt == temp_type) break;
              prev_temp_type = pt;
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


static void adjust_parameter_type(a_type_ptr *type_ptr)
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


static a_param_id_ptr alloc_param_id(void)
/*
Allocate a parameter id block, set its fields to default values, and
return a pointer to it.  The locator field of the entry is set to
locator_for_curr_id.
*/
{
  register a_param_id_ptr pip;

  db_enter(5, "alloc_param_id");
  if (avail_param_ids != NULL) {
    /* Reuse a previously-freed entry. */
    pip = avail_param_ids;
    avail_param_ids = avail_param_ids->next;
  } else {
    /* Allocate a new entry. */
    pip = (a_param_id_ptr)alloc_fe(sizeof(a_param_id));
  }  /* if */
  /* Set the entry's fields to default values. */
  pip->next = NULL;
  pip->symbol = NULL;
  pip->type = NULL;
  pip->storage_class = (a_storage_class)sc_unspecified;
  db_exit();
  return(pip);
}  /* alloc_param_id */


static void free_param_id(a_param_id_ptr *ppip)
/*
Free the parameter id block pointed to by *ppip, set *ppip to NULL.
*/
{
  db_enter(5, "free_param_id");
  (*ppip)->next = avail_param_ids;
  avail_param_ids = *ppip;
  *ppip = NULL;
  db_exit();
}  /* free_param_id */


void free_param_id_list(a_param_id_ptr *pidlist)
/*
Free the list of parameter id blocks pointed to by *pidlist, and set
*pidlist to NULL.
*/
{
  a_param_id_ptr pip;

  db_enter(5, "free_param_id_list");
  while (*pidlist != NULL) {
    pip = *pidlist;
    *pidlist = pip->next;
    free_param_id(&pip);
  }  /* while */
  db_exit();
}  /* free_param_id_list */


static a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                       a_param_id_ptr    param_id_list)
/*
Search the parameter id list given by param_id_list to see if the identifier
given by *locator is on it.  If so, return a pointer to the entry; if not,
return NULL.
*/
{
  register a_param_id_ptr param_id = param_id_list;

  while (param_id != NULL) {
    if (param_id->symbol != NULL &&
        param_id->symbol->header == locator->symbol_header) {
      /* Found a match. */
      break;
    }  /* if */
    /* Keep searching the param id list for this name. */
    param_id = param_id->next;
  }  /* while */
  return(param_id);
}  /* param_id_on_list */


static void add_to_param_id_list(a_symbol_locator      *locator,
                                 a_type_ptr            type_ptr,
                                 a_storage_class       storage_class,
                                 a_func_info_block_ptr func_info,
                                 a_param_id_ptr        *last_param_id)
/*
Add the indicated identifier to the parameter id list pointed to by
func_info.  Do nothing if func_info == NULL.  If there is a parameter
id list, *last_param_id points to the last entry on it.  type_ptr and
storage_class are the type and storage class for the parameter.
*/
{
  a_param_id_ptr  new_param_id;
  a_symbol_ptr    sym;
  a_boolean       unnamed_param = FALSE;
  a_boolean       is_prototype_param_decl = (type_ptr != NULL);

  if (func_info != NULL) {
    /* See if this identifier name already appears on the list.  If so, issue
       an error.  Create a param_id entry if this is a prototype parameter
       list, but not otherwise. */
    if (!is_error_locator(*locator)) {
      if (param_id_on_list(locator, func_info->param_id_list) != NULL) {
        error(ec_dupl_param_name);
        set_to_error_locator(*locator);
      } /* if */
    } else if (is_prototype_param_decl) {
      /* Assume that if an error locator is passed in and this is a prototype
         parameter declaration that we have an unnamed parameter.  We'll need
         a param_id entry to keep track of the type. */
      unnamed_param = TRUE;
    } /* if */
    /* Create a param_id entry and enter it onto the param_id list.  Skip
       this if we have an old-style param id list in which a duplicate was
       encountered. */
    if (is_prototype_param_decl || !is_error_locator(*locator)) {
      new_param_id = alloc_param_id();
      /* Save the type and storage class for the later declaration. */
      new_param_id->type = type_ptr;
      new_param_id->storage_class = storage_class;
      /* Create a parameter symbol.  It is used during parameter processing
         only.  The corresponding symbol in the function scope itself is a
         variable symbol for which the variable's is_parameter flag is set to
         TRUE. */
      if (unnamed_param) {
        /* Create no symbol for an unnamed parameter. */
        sym = NULL;
      } else if (type_ptr != NULL) {
        /* Prototyped parameter list.  The symbol is entered in the the
           function prototype scope.  It will later be copied to the function
           scope when it is changed to sk_variable. */
        sym = enter_symbol((a_symbol_kind)sk_parameter, locator,
                           depth_scope_stack, /*suppress_redecl_error=*/FALSE);
      } else {
        /* Must be an old-style parameter declaration.  The type and storage
           class will be supplied later.  We won't actually enter this symbol
           until the function scope is pushed. */
        sym = make_parameter_symbol(locator);
      }  /* if */
      new_param_id->symbol = sym;
      /* Put this entry on the end of the list of param ids. */
      if (func_info->param_id_list == NULL) {
        func_info->param_id_list = new_param_id;
      } else {
        (*last_param_id)->next = new_param_id;
      }  /* if */
      (*last_param_id) = new_param_id;
    }  /* if */
  }  /* if */
}  /* add_to_param_id_list */


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


void check_operator_function_params(a_routine_ptr      rout,
                                    a_source_position  *pos)
/*
Check the argument list on the declaration of a user-defined conversion
or overloaded operator function.  For conversion functions, no arguments
are allowed.  For operators there are different requirements for different
operator kinds.  Issue a diagnostic if an error is found.
*/
{
  int               param_count;
  a_param_type_ptr  ptp;
  a_boolean         any_class_type_params = FALSE;
  an_opname_kind    opname;
  a_type_ptr        tp;
  a_boolean         is_nonstatic_member_function;
  an_error_code     error_code = ec_no_error;

  db_enter(4, "check_operator_function_params");
  if (rout->special_kind == (a_special_function_kind)sfk_conversion) {
    /* Any parameter is too many for a conversion function. */
    if (rout->type->variant.routine.extra_info->param_type_list != NULL) {
      pos_error(ec_too_many_args_for_conversion, pos);
    }  /* if */
  } else if (rout->special_kind == (a_special_function_kind)sfk_operator) {
    /* It's an operator.  Get the specific kind. */
    opname = rout->opname_kind;
    is_nonstatic_member_function =
                routine_type_is_nonstatic_member_function(rout->type);
#if CHECKING
    if (is_nonstatic_member_function &&
        (opname == (an_opname_kind)onk_new ||
         opname == (an_opname_kind)onk_delete)) {
      internal_error(
               "check_operator_function_params: new or delete is nonstatic");
    }  /* if */
#endif /* CHECKING */
    /* Make a pass over the param types list to count the number of
       arguments to see if there are any parameters that are of class type
       or reference-to-class type.  Note that param_count is initialized to
       0 except in the case of nonstatic member functions, for which it is
       initialized to 1. This is because the implicit "this" parameter is
       counted in the latter case. */
    param_count = is_nonstatic_member_function ? 1 : 0;
    ptp = rout->type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      param_count++;
      tp = ptp->type;
      if (is_reference_type(tp)) tp = type_pointed_to(tp);
      if (is_class_struct_union_type(tp)) any_class_type_params = TRUE;
    }  /* if */
    if (opname == (an_opname_kind)onk_compl ||
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
    } else if (opname == (an_opname_kind)onk_function_call ||
               opname == (an_opname_kind)onk_new) {
      /* Function call and new must have one or more arguments. */
      if (param_count == 0) {
	error_code = ec_too_few_args_for_operator;
      } else if (opname == (an_opname_kind)onk_new) {
        ptp = rout->type->variant.routine.extra_info->param_type_list;
        tp = ptp->type;
        if (!is_error_type(tp)) {
          if (!is_integral_type(tp) ||
              skip_typerefs(tp)->variant.integer.int_kind !=
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND) {
            error_code = ec_bad_arg_type_for_operator_new;
            ptp->type = error_type();
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (opname == (an_opname_kind)onk_delete) {
      ptp = rout->type->variant.routine.extra_info->param_type_list;
      if (param_count == 0) {
	error_code = ec_too_few_args_for_operator;
      } else {
        tp = ptp->type;
        if (!is_error_type(tp)) {
          if (!is_pointer_type(tp) || !is_void_type(type_pointed_to(tp))) {
            pos_error(ec_bad_first_arg_type_for_operator_delete, pos);
            ptp->type = error_type();
          }  /* if */
        }  /* if */
        ptp = ptp->next;
        if (ptp != NULL) {
          /* There is a second argument.  This is permitted for class operator
             delete() but not for global operator delete() (ARM 12.5). */
          if (rout->source_corresp.class_of_which_a_member == NULL) {
            error_code = ec_too_many_args_for_operator;
          } else {
            /* The second argument must be of type size_t (ARM 12.5). */
            tp = ptp->type;
            if (!is_error_type(tp)) {
              if (!is_integral_type(tp) ||
                  skip_typerefs(tp)->variant.integer.int_kind !=
                                    (an_integer_kind)TARG_SIZE_T_INT_KIND) {
                pos_error(ec_bad_second_arg_type_for_operator_delete, pos);
                ptp->type = error_type();
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
    if (error_code != ec_no_error) pos_error(error_code, pos);
    /* Check return type. */
    if (opname == (an_opname_kind)onk_arrow) {
      /* For operator->() do a special check on the return type.  It must
         be something that can be used as a pointer -- either a pointer
         to a class or an object of or reference to a class for which
         operator->() is defined (ARM 13.4.6). */
      tp = rout->type->variant.routine.return_type;
      if (!is_error_type(tp)) {
        a_boolean  err;
        if (is_pointer_type(tp)) {
          err = !is_class_struct_union_type(type_pointed_to(tp));
        } else {
          if (is_reference_type(tp)) tp = type_pointed_to(tp);
          err = (!is_class_struct_union_type(tp) ||
                 tp == rout->source_corresp.class_of_which_a_member ||
                 opname_member_function_symbol(opname, tp) == NULL);
        }  /* if */
        if (err) {
          pos_error(ec_bad_return_type_for_op_arrow, pos);
          rout->type->variant.routine.return_type = error_type();
        }  /* if */
      }  /* if */
    }  /* if */
    if (opname == (an_opname_kind)onk_new ||
        opname == (an_opname_kind)onk_delete) {
      tp = rout->type->variant.routine.return_type;
      if (!is_error_type(tp)) {
        if (opname == (an_opname_kind)onk_new) {
          if (!is_pointer_type(tp) || !is_void_type(type_pointed_to(tp))) {
            pos_error(ec_bad_return_type_for_op_new, pos);
          }  /* if */
        } else {
          if (!is_void_type(tp)) {
            pos_error(ec_bad_return_type_for_op_delete, pos);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* If operator function is not a nonstatic member and does not have
         operands of class type or reference-to-class type, issue an error.
         This restriction does not apply to new and delete, however. */
      if (!is_nonstatic_member_function && !any_class_type_params) {
        pos_error(ec_no_args_with_class_type, pos);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_operator_function_params */


void clear_func_info(a_func_info_block *func_info)
/*
Clear the fields of a function information block to default values.
*/
{
  func_info->prototype_scope_symbols     = NULL;
  func_info->param_id_list               = NULL;
  func_info->scope_number                = NO_SCOPE_NUMBER;
  func_info->any_prototype_names_omitted = FALSE;
}  /* clear_func_info */


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
  } else if (is_decl_start()) {
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
  a_symbol_locator        param_locator;
  a_type_ptr              bottom_derived_type;
  a_boolean               done;
  a_source_position       start_pos, param_type_pos;
  a_routine_type_supplement_ptr
                          extra_info;
  a_boolean               dangling_type_specifier = FALSE;
  a_boolean               defines_something;
  a_boolean               default_arg_expr_allowed = FALSE;
  an_expr_node_ptr        dim_expr_ptr;

  db_enter(3, "function_declarator");
  copy_source_position(pos_curr_token, start_pos);
  set_err_pos_to_curr_token();
  add_stop_token(tok_rparen);
  if (func_info != NULL) clear_func_info(func_info);
  last_param_id = NULL;
  *new_type_ptr = alloc_type((a_type_kind)tk_routine);
  extra_info = (*new_type_ptr)->variant.routine.extra_info;
  extra_info->constructor_or_destructor = (is_constructor || is_destructor);
  /* If a pragma indicating special argument checking appeared (e.g.,
     for printf args), remember that in the function type. */
  extra_info->arg_pragma = arg_pragma;
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
      /* Push a function prototype scope for the parameters. */
      (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                       *new_type_ptr, (a_routine_ptr)NULL);
      /* Remember the scope number for later use if and when a body appears. */
      if (func_info != NULL) {
        func_info->scope_number = scope_stack[depth_scope_stack].number;
      }  /* if */
      last_param_type = NULL;
      if (C_dialect == C_dialect_cplusplus) {
        /* In C++ mode a default argument may be declared with the parameter
           unless the function is a user-defined overloaded operator or
           conversion.  Note that locator may be NULL (e.g., with abstract
           declarators). */
        if (locator != NULL &&
            !locator->is_operator_name && !locator->is_conversion_name) {
          default_arg_expr_allowed = TRUE;
        }  /* if */
      }  /* if */
      do {
        add_stop_token(tok_comma);
        copy_source_position(pos_curr_token, param_type_pos);
        /* Scan a parameter-declaration. */
        (void)decl_specifiers((DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                               DSI_TYPE_SPECIFIER_ALLOWED |
                               DSI_IS_PARAMETER),
			      &dso_flags, &param_storage_class,
                              &param_type_ptr);
        dangling_type_specifier = dso_flags & DSO_DANGLING_TYPE_SPECIFIER;
        defines_something = dso_flags & DSO_DEFINES_SOMETHING;
        if ((dso_flags & DSO_JUST_VOID) &&
            last_param_type == NULL &&
	    curr_token == tok_rparen) {
          /* The first and only parameter-declaration is just "void", which
             has a special meaning (no parameters).  (3.5.4.3) */
          /* param_type_list is already NULL. */
          done = TRUE;
        } else {
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
	     looking for a declarator when there's a comma or right paren or
	     when decl_specifiers has found a badly formed type specifier or
             when the next token is an ellipsis.  If an error is to be put out,
             that's done later. */
          if (curr_token != tok_comma && curr_token != tok_rparen &&
	      !dangling_type_specifier && curr_token != tok_ellipsis) {
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
                       (a_func_info_block_ptr)NULL, &dim_expr_ptr);
          } else {
            /* No declarator. */
            set_to_error_locator(param_locator);
          }  /* if */
          /* Adjust the type if necessary (for example, "array of x"
             becomes "pointer to x"). */
          adjust_parameter_type(&param_type_ptr);
          if (is_constructor &&
              identical_types(member_function_parent_type,
                              skip_typerefs(param_type_ptr))) {
            /* X::X(X) is not allowed -- ARM 12.1. */
            pos_error(ec_bad_constructor_param, &param_type_pos);
            /* We have to set the locator to an error locator to avoid both
               overloading problems and an infinite loop downstream.  This
               assures that the routine can never be used as a constructor. */
            set_to_error_locator(*locator);
          } else if (C_dialect == C_dialect_cplusplus &&
                     is_illegal_abstract_class_type(param_type_ptr)) {
            /* Abstract class may not be used as an arg type (ARM 10.3). */
            pos_error(ec_abstract_class_object_not_allowed,
                      is_error_locator(param_locator) ?
                             &param_type_pos : &param_locator.source_position);
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
          if (func_info != NULL) {
            /* A parameter name is present.  Save it only if there is a
               func_info entry in which to save it.  If there isn't, the
               name is only significant for commenting purposes anyway.
               Note that null-locator names are saved for all unnamed
               parameters.  This is done because named and unnamed parameters
               can be mixed in one list.  In C, such a list is really only
               allowed when there is no body defining the function, and in
               that case the names are not significant.  However, if the user
               makes a mistake, having as complete a list as possible
               minimizes the error recovery problems. */
            if (is_error_locator(param_locator)) {
              func_info->any_prototype_names_omitted = TRUE;
            }  /* if */
            add_to_param_id_list(&param_locator, param_type_ptr,
                                 param_storage_class,
                                 func_info, &last_param_id);
          }  /* if */
          if (curr_token == tok_assign && C_dialect == C_dialect_cplusplus) {
            /* Argument expressions are not allowed in overloaded operator
               declarations.  Issue an error, but go ahead and scan the
               expression. */
            if (!default_arg_expr_allowed) {
              pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);
            }  /* if */
            /* Advance past the equal sign. */
            (void)get_token();
            /* Check the scope immediately containing the current scope, which
               is a function prototype scope.  We may have to cache the
               default argument tokens and rescan them later. */
            if (default_arg_expr_allowed &&
                scope_stack[depth_scope_stack-1].kind ==
                                   (a_scope_kind)sck_class_struct_union &&
                curr_token != tok_comma && curr_token != tok_rparen &&
                curr_token != tok_semicolon && curr_token != tok_rbrace && 
                curr_token != tok_lbrace) {
              /* This function declaration appears within a class definition.
                 The tokens for the default argument expression are cached at
                 this point and only scanned once the entire class has been
                 defined.  This is because forward references may legally
                 appear in the default argument expression (C++ draft standard,
                 section 8.2.6, para 3). */
              prescan_default_arg_expr(ptp);
            } else {
              /* Not a class scope -- or else a syntax error.  Go ahead and
                 scan the expression and convert it to the required type. */
              scan_default_arg_expr(default_arg_expr_allowed ?
                                      ptp : (a_param_type_ptr)NULL);
            }  /* if */
            ptp->has_default_arg = default_arg_expr_allowed;
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
        }  /* if */
        remove_stop_token(tok_comma);
      } while (!done);
      /* Save the list of symbols for the prototype scope (usually NULL, but
         can have symbols for named types declared within the prototype). */
      if (func_info != NULL) {
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
      if (func_info == NULL) {
        /* This type of parameter list is not valid in abstract declarators
           and non-top-level function declarators. */
        error(ec_param_id_list_needs_function_def);
      } else if (C_dialect == C_dialect_cplusplus) {
        /* This type of parameter list is an anachronism in C++. */
        warning(ec_old_style_parameter_list);
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
            error(ec_typedef_cannot_be_param_name);
            /* Enter the parameter anyway, for best error recovery. */
          }  /* if */
          /* Add the identifier to the parameter id list. */
          add_to_param_id_list(&locator_for_curr_id, (a_type_ptr)NULL,
                               (a_storage_class)sc_unspecified,
                               func_info, &last_param_id);
          /* Advance past the identifier. */
          (void)get_token();
        }  /* if */
        remove_stop_token(tok_comma);
        /* Keep looping on a comma, stop otherwise. */
      } while (loop_token(tok_comma));
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
    /* Create a pointer to the implicit this parameter.  This can be done
       for nonstatic function declarations within a class definition or
       for member function declarations outside a class definition when
       a function qualifier is present.  If there is a function qualifier,
       it is applied to the type pointed to by the this param type. */
    a_type_ptr  this_param_type = NULL;

    if (is_type_qualifier()) {
      /* In C++ the type of certain member functions may be qualified.  Scan
         for a const or volatile qualifier. */
      a_storage_class    dummy_storage_class;
      a_type_ptr         dummy_type_ptr;
      a_source_position  qualifier_pos;

      copy_source_position(pos_curr_token, qualifier_pos);
      (void)decl_specifiers(DSI_NO_INPUT_FLAGS, &dso_flags,
                            &dummy_storage_class, &dummy_type_ptr);
      /* If this is not a member function or it is but it is a static member
         function declared within a class definition, a qualifier on the
         function is illegal (ARM 8.2.5)..  However, qualifiers on a pointer to member function
         are permitted. */
      if (member_function_parent_type == NULL ||
          (!is_nonstatic_member_function &&
           scope_stack[decl_scope_level].kind ==
                                 (a_scope_kind)sck_class_struct_union)) {
        /* It is illegal to specify "const" or "volatile" on any function
           other than a nonstatic member function (ARM 8.2.5).  We just
           issue a warning since it is harmless. */
        pos_warning(ec_function_qualifier_not_allowed, &qualifier_pos);
      } else if (is_constructor || is_destructor) {
        /* A qualifier appearing on a constructor or destructor is not
           allowed (ARM 9.3.1). */
        pos_error(ec_function_qualifier_not_allowed, &qualifier_pos);
        this_param_type = member_function_parent_type;
      } else {
        this_param_type =
                      make_qualified_type(member_function_parent_type,
		                          dso_flags & DSO_CONST_QUALIFIED,
		                          dso_flags & DSO_VOLATILE_QUALIFIED);
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
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* function_declarator */


static void array_declarator(a_type_ptr *new_type_ptr)
/*
Scan an array declarator (3.5.4.2), or an array declarator in an
abstract declarator (3.5.5).  Allocate and return in *new_type_ptr an
appropriate array type.  The initial opening bracket is the current
token.
*/
{
  long              num_of_elements;
  a_constant        constant;
  a_boolean         err = FALSE;
  a_source_position start_pos;

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
    scan_integral_constant_expression(&constant);
    if (is_error_constant(&constant)) {
      err = TRUE;
    } else {
#if CHECKING
      if (constant.kind != (a_constant_repr_kind)ck_integer) {
        internal_error("array_declarator: array size not int");
      }  /* if */
#endif /* CHECKING */
      num_of_elements = constant.variant.integer_value;
      if (num_of_elements <= 0) {
        error(ec_array_size_must_be_positive);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    *new_type_ptr = error_type();
  } else {
    *new_type_ptr = alloc_type((a_type_kind)tk_array);
    /* Store the array size. */
    (*new_type_ptr)->variant.array.number_of_elements = num_of_elements;
    /* The size of the array (in bytes) is updated in 
       add_to_derived_type_list. */
  }  /* if */
  /* Check for closing right bracket. */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* array_declarator */


static void nonconstant_array_declarator(a_type_ptr       *new_type_ptr,
                                         an_expr_node_ptr *dim_expr)
/*
Scan an array declarator for an operator new type-name.  The logic is
much the same as that of array_declarator, except that if a nonconstant
expression is found, a pointer to it is returned in *dim_expr and the
size of the array created is zero.  When there is a constant, *dim_expr
is set to NULL and the constant value is used for the size.
*/
{
  long              num_of_elements;
  a_constant        constant;
  a_boolean         err = FALSE, is_constant;
  a_source_position start_pos;

  db_enter(3, "nonconstant_array_declarator");
  *dim_expr = NULL;
  copy_source_position(pos_curr_token, start_pos);
  /* Pass over the initial left bracket. */
  (void)get_token();
  add_stop_token(tok_rbracket);
  if (curr_token == tok_rbracket) {
    /* Empty brackets, indicating an incomplete array type. */
    num_of_elements = 0;
  } else {
    /* Scan the array dimension. */
    scan_new_array_dimension_expression(&is_constant, dim_expr, &constant);
    if (is_constant) {
      if (is_error_constant(&constant)) {
        err = TRUE;
      } else {
#if CHECKING
        if (constant.kind != (a_constant_repr_kind)ck_integer) {
          internal_error("nonconstant_array_declarator: array size not int");
        }  /* if */
#endif /* CHECKING */
        num_of_elements = constant.variant.integer_value;
        if (num_of_elements <= 0) {
          error(ec_array_size_must_be_positive);
          err = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* An expression was returned.  Create an array whose element count
         is zero; actual element count is in dim_expr and will be supplied
         at run time. */
      num_of_elements = 0;
    }  /* if */
  }  /* if */
  if (err) {
    *new_type_ptr = error_type();
  } else {
    *new_type_ptr = alloc_type((a_type_kind)tk_array);
    /* Store the array size. */
    (*new_type_ptr)->variant.array.number_of_elements = num_of_elements;
    /* The size of the array (in bytes) is updated in 
       add_to_derived_type_list. */
  }  /* if */
  /* Check for closing right bracket. */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* nonconstant_array_declarator */


a_routine_ptr make_routine(a_type_ptr      type_ptr,
                           a_storage_class storage_class,
                           a_boolean       at_file_scope)
/*
Allocate an entry for a routine with return type type_ptr
and storage class storage_class, and return a pointer to it.
The entry is allocated at the file scope.  type_ptr must be in
the file scope.
*/
{
  a_routine_ptr          rp;
  a_memory_region_number region_to_switch_back_to;

  /* Always allocate routines at the file scope. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  rp = alloc_routine();
  rp->type = type_ptr;
  rp->storage_class = storage_class;
  add_to_routines_list(rp, at_file_scope);
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

  if (type_ptr == NULL) {
    vp = NULL;
  } else {
    vp = alloc_variable(storage_class);
    vp->type = type_ptr;
    vp->is_parameter = TRUE;
  }  /* if */
  return(vp);
}  /* make_param_variable */


a_variable_ptr make_parameter(a_type_ptr       type,
                              a_storage_class  storage_class,
                              a_symbol_ptr     sym)
/*
Allocate a parameter variable with the specified type and storage class
and return a pointer to it. The parameter is linked to/from its associated
symbol sym.
*/
{
  a_variable_ptr vp;

  vp = make_param_variable(type, storage_class);
  /* sym will be NULL when the parameter is unnamed. */
  if (sym != NULL) {
    sym->variant.variable = vp;
    set_source_corresp(&(vp->source_corresp), sym);
  }  /* if */
  add_to_parameters_list(vp);
  return(vp);
}  /* make_parameter */


static void fixup_parameters(a_variable_ptr    param_list,
                             a_param_type_ptr  param_type_list)
/*
Set each variable in a linked list of parameters to point to the corresponding
param type entry.
*/
{
  a_variable_ptr    vp = param_list;
  a_param_type_ptr  ptp = param_type_list;

  for (; vp != NULL; vp = vp->next, ptp = ptp->next) {
#if CHECKING
    if (ptp == NULL) {
      internal_error("fixup_parameters: too few param type entries");
    }  /* if */
#endif /* CHECKING */
    vp->assoc_param_type = ptp;
  }  /* for */
#if CHECKING
  if (ptp != NULL) {
    internal_error("fixup_parameters: too many param type entries");
  }  /* if */
#endif /* CHECKING */
}  /* fixup_parameters */


static void make_return_value_pointer_variable(a_type_ptr  rout_type,
                                               a_scope_ptr scope_ptr)
/*
If required, allocate a variable with type pointer-to-function-return-type
that will point to the storage provided by the caller for returning a
class object by value, and store the variable in the function's scope
entry.  The variable is created only when a flag in the routine type
supplement indicates that it is required.  The variable is flagged as a
parameter, but it is added to no list.
*/
{
  a_type_ptr      return_type;

  if (rout_type->variant.routine.extra_info->
                             caller_provides_place_to_put_return_value) {
    /* The implicit parameter for returning a class object by value is
       required.  Get the return type, stripped of any qualifiers. */
    return_type = rout_type->variant.routine.return_type;
    return_type = skip_typerefs(return_type);
#if CHECKING
    /* A class type is expected. */
    if (!is_class_struct_union_type(return_type)) {
      internal_error("make_return_value_pointer_variable: not a class type");
    }  /* if */
#endif /* CHECKING */
    /* Create the variable (its type is pointer to the class type) and
       record it in the routine's IL scope. */
    scope_ptr->variant.routine.return_value_pointer_variable =
                          make_param_variable(make_pointer_type(return_type),
                                              (a_storage_class)sc_auto);
  }  /* if */
}  /* make_return_value_pointer_variable */


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
  if (C_dialect != C_dialect_cplusplus) {
    /* Give a warning for anything declared within a prototype scope.
       For example,

         inf f(struct s a;);
         struct s {int b;};

       The first "struct s" is a different type than the second, which is
       probably not what was wanted.  (This warning is not issued for C++
       because an error is issued elsewhere, where appropriate.) */
    if (scope_stack[scope_level].kind == (a_scope_kind)sck_func_prototype) {
      pos_warning(ec_decl_in_prototype_scope, &locator->source_position);
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
that it is the default operator new().
*/
{
  a_boolean         match = FALSE;

  if (locator->is_operator_name &&
      locator->variant.opname == (an_opname_kind)onk_new) {
#if CHECKING
    if (!is_function_type(type)) {
      internal_error("is_default_operator_new: bad type");
    } else if (type->variant.routine.extra_info->param_type_list == NULL) {
      internal_error("is_default_operator_new: bad param type list");
    }  /* if */
#endif /* CHECKING */
    if (type->variant.routine.extra_info->param_type_list->next == NULL) {
      match = TRUE;
    }  /* if */
  }  /* if */
  return match;
}  /* is_default_operator_new */


static an_id_linkage_kind id_linkage(a_symbol_locator *locator,
                                     a_storage_class  storage_class,
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
  a_symbol_ptr       other_decl, sym;
  a_boolean          is_default_global_operator_new = FALSE;

  *linked_symbol = NULL;
  *overload_symbol = NULL;
  if (storage_class == (a_storage_class)sc_typedef) {
    /* A typedef is not an object or function, and has no linkage. */
    linkage = idl_none;
  } else if ((sym = locator->specific_symbol) != NULL &&
             sym->class_of_which_a_member != NULL) {
#if CHECKING
    if (sym->kind != (a_symbol_kind)sk_static_data_member) {
      internal_error("id_linkage: bad symbol kind for class member");
    }  /* if */
#endif /* CHECKING */
    *linked_symbol = sym;
    linkage = idl_external;
  } else {
    /* Is this type an object or function, and is it going to be declared
       at file scope? */
    is_function = is_function_type(type);
    is_object = !is_function;
    /* In pcc mode, functions and extern variables are always effectively
       declared at the file scope level. */
    if (C_dialect == C_dialect_pcc &&
        (is_function || storage_class == (a_storage_class)sc_extern)) {
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
          if (decls_at_same_scope) *overload_symbol = other_decl;
          if (other_decl->kind == (a_symbol_kind)sk_overloaded_function) {
            other_decl = other_decl->variant.overloaded_function.symbols;
            is_list = TRUE;
          } else {
            is_list = FALSE;
          }  /* if */
          /* Go through the list of functions and look for type compatibility.
             If types_are_compatible returns TRUE, this is a redeclaration.
             If no type match is found, this is a candidate for overloading. */
          for (; other_decl != NULL;
                 other_decl = is_list ? other_decl->next : NULL) {
            if (types_are_compatible(routine_symbol_type(other_decl), type)) {
              /* Null out *overload_symbol in case it was set. */
              *overload_symbol = NULL;
              break;
            }  /* if */
          }  /* for */
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
                       other_decl->variant.variable->storage_class ==
                                           (a_storage_class)sc_extern ||
                       other_decl->variant.variable->storage_class ==
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
    if (!file_scope && storage_class != (a_storage_class)sc_extern) {
      /* A non-file-scope object without extern storage class has no
         linkage.  In C++ a non-file-scope function may be declared --
         a friend function defined inline within a local class; it too
         is given no linkage. */
      linkage = idl_none;
#if CHECKING
      if (is_function &&
          (!is_friend_decl || storage_class != (a_storage_class)sc_static)) {
        internal_error("id_linkage: expected friend and static storage class");
      }  /* if */
#endif /* CHECKING */
    } else if (file_scope &&
               storage_class == (a_storage_class)sc_static) {
      /* An object or function at file scope with static storage class
         has internal linkage. */
      linkage = idl_internal;
    } else if (storage_class == (a_storage_class)sc_extern ||
               (is_function &&
                storage_class == (a_storage_class)sc_unspecified)) {
      /* An object or function with extern storage class, or a function
         with no storage class, has the same linkage as any visible
         declaration of this identifier with file scope.  If there is
         no visible declaration, the identifier has external linkage. */
      if (other_decl != NULL && other_decl->decl_scope == FILE_SCOPE_NUMBER) {
        /* There is a declaration with file scope that is visible from
           here.  Set the flags to describe this identifier, and go
           retry the determination of the linkage. */
        is_function = (other_decl->kind == (a_symbol_kind)sk_routine);
        is_object = !is_function;
        file_scope = TRUE;
        if (is_function) {
          storage_class = other_decl->variant.routine->storage_class;
        } else {
          storage_class = other_decl->variant.variable->storage_class;
        }  /* if */
        /* If we check again for visible identifiers, there can be no
           other visible identifier with the same name. */
        other_decl = NULL;
        goto determine_linkage;
      }  /* if */
      /* No visible declaration found, so the linkage is external. */
      linkage = idl_external;
    } else if (is_object && file_scope &&
               storage_class == (a_storage_class)sc_unspecified) {
      /* An object at file scope with no storage class has external linkage. */
      linkage = idl_external;
#if ASM_FUNCTION_ALLOWED
    } else if (storage_class == (a_storage_class)sc_asm) {
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
        pos_error(ec_decl_incompatible_with_previous_use, position);
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
  /* Look up the external name of the identifier (i.e., the name after
     any truncation, etc.). */
  ext_sym = find_external_symbol(locator, name_linkage,
                                 is_function ? type_ptr : NULL,
                                 &ext_locator);
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
#if CHECKING
      if (old_name == NULL) {
        internal_error("create_external_symbol_for_linked_entity: NULL name");
      }  /* if */
#endif /* CHECKING */
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
          pos_error(ec_decl_incompatible_with_previous_use,
                    &locator->source_position);
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
         prevailing practice we only issue a warning.  The same is done in
         C mode, partly because it is common practice in pcc. */
      pos_warning(ec_linkage_conflict, position);
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
    if (ptp->default_arg_expr != NULL && ptp->next != NULL &&
        ptp->next->default_arg_expr == NULL) {
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
    if (ptp1->default_arg_expr != NULL) {
      /* The parameter on the original type has a default arg. */
      if (ptp2->default_arg_expr != NULL) {
        /* So does the parameter on the new type.  This is illegal. */
        redecl_error = TRUE;
      }  /* if */
      default_arg_required = TRUE;
    } else if (ptp2->default_arg_expr != NULL) {
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

#if CHECKING
  if (!types_are_compatible(type_ptr, rout_type)) {
    internal_error("reconcile_routine_types: types not compatible");
  } else if (preserve_rout_type && preserve_type_ptr) {
    internal_error("reconcile_routine_types: both preserve flags TRUE");
  }  /* if */
#endif /* CHECKING */
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
        /* Copy the param type entries from the composite type onto the param
           type entries for the routine type.  This is done in case new param
           type entries were created.  The original ones must be preserved,
           however, since they may be pointed to by the parameter variables
           with which they are associated. */
        rout_type_ptp = rout_type->variant.routine.extra_info->param_type_list;
        comp_type_ptp = comp_type->variant.routine.extra_info->param_type_list;
        for (; rout_type_ptp != NULL; rout_type_ptp = next_rout_type_ptp,
                                      comp_type_ptp = comp_type_ptp->next) {
          if (rout_type_ptp == comp_type_ptp) {
            /* Whenever the corresponding param type entries on the two lists
               are the same entry, all subsequent ones will also be the same,
               so we can bail out at that point. */
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
}  /* reconcile_routine_types */


void decl_var_or_routine(a_symbol_locator      *locator,
                         a_storage_class       storage_class,
                         a_type_ptr            type_ptr,
                         a_boolean             is_implicit_function,
                         a_boolean             is_function_def_with_body,
                         a_boolean             inline_specified,
                         a_boolean             is_main_function,
                         a_symbol_ptr          *symbol_ptr,
                         an_id_linkage_kind    *linkage_ptr,
                         a_type_ptr            *old_type,
                         a_symbol_ptr          *ext_sym)
/*
Enter the declaration of an identifier for a variable or routine.
*locator gives the symbol locator (and thus its name and its
declaration position).  storage_class and type_ptr give the storage
class and type.  If is_implicit_function is TRUE, this declaration is
for an implicit function declaration, and *symbol_ptr already contains
a pointer to the symbol entry, which is already in the symbol table.
is_function_def_with_body is TRUE if the identifier being defined is
part of a function definition (meaning there is a body in the definition).
In that case, it is guaranteed that type_ptr points to an unshared
type entry, and that type entry will be preserved as the routine type.
Create and enter a symbol entry, and return a pointer to it in
*symbol_ptr.  Also allocate any associated IL construct, and attach it
to the symbol.  If the identifier has linkage and there is an existing
symbol or IL entry, it will be re-used.  Return in *linkage_ptr the
linkage of the identifier.  Return in *old_type any previously-known
type for this identifier from a linked identifier in the same scope,
or NULL if there was no previously-known type.  If the identifier has
linkage, return in *ext_sym a pointer to the external symbol entry;
otherwise, set *ext_sym to NULL.
*/
{
  a_symbol_ptr      sym = NULL;
  a_boolean         is_function;
  a_boolean         at_file_scope;
  a_symbol_ptr      linked_symbol, homonym_symbol, overload_symbol = NULL;
  a_boolean         redecl_error_already_issued = FALSE;
  a_boolean         linked_redecl_error = FALSE;
  a_boolean         old_decl_has_body;
  a_boolean         redeclaration = FALSE;
  a_variable_ptr    variable_ptr = NULL;
  a_routine_ptr     routine_ptr = NULL;
  an_id_linkage_kind
                    linkage;
  a_source_correspondence
                    *source_corresp_ptr;
  a_scope_depth     effective_decl_level;

  db_enter(3, "decl_var_or_routine");
#if CHECKING
  if (storage_class == (a_storage_class)sc_typedef) {
    internal_error("decl_var_or_routine: called with typedef");
  }  /* if */
#endif /* CHECKING */
  *old_type = NULL;
  is_function = is_function_type(type_ptr);
  if (inline_specified) {
#if CHECKING
    if (storage_class != (a_storage_class)sc_unspecified &&
        storage_class != (a_storage_class)sc_static) {
      internal_error("decl_var_or_routine: bad storage class with inline");
    }  /* if */
#endif /* CHECKING */
    storage_class = (a_storage_class)sc_static;
  }  /* if */
  if (is_implicit_function) {
    /* For an implicit function, the identifier would not be in the process
       of being declared implicitly as a function if there were any visible
       declaration of it, and therefore it must have external linkage. */
    linkage = idl_external;
    linked_symbol = NULL;
    homonym_symbol = NULL;
    sym = *symbol_ptr;
  } else {
    /* Determine the linkage of this symbol. */
    linkage = id_linkage(locator, storage_class, type_ptr, is_main_function,
                         &linked_symbol, &homonym_symbol,
                         &effective_decl_level);
  }  /* if */
  /* at_file_scope will be TRUE if the IL variable or routine must be
     allocated in the file scope memory region.  This is always true
     of routines, and also true of variables with linkage. */
  at_file_scope = (is_function || linkage != idl_none);
  if (linkage != idl_none && linked_symbol != NULL) {
    /* There is a previous identifier of this name in the same scope,
       to which this declaration is linked.  The new declaration must be
       compatible with the old. */
    redeclaration = TRUE;
    if (linked_symbol->kind == (a_symbol_kind)sk_variable && !is_function) {
      if (C_dialect == C_dialect_cplusplus && linked_symbol->defined &&
          storage_class != (a_storage_class)sc_extern) {
        /* Variable has already been defined.  Force an error on symbol lookup
           by leaving sym NULL. */
        linked_redecl_error = TRUE;
      } else {
        /* Linked symbol and new symbol are both variables.  See if they
           are compatible. */
        sym = linked_symbol;
        variable_ptr = linked_symbol->variant.variable;
#if CHECKING
        if (variable_ptr == NULL) {
          internal_error(
              "decl_var_or_routine: linked variable symbol has NULL variable");
        }  /* if */
#endif /* CHECKING */
        *old_type = variable_ptr->type;
        if (!types_are_compatible(type_ptr, *old_type)) {
          pos_error(ec_not_compatible_with_previous_decl,
                    &locator->source_position);
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
      routine_ptr = linked_symbol->variant.routine;
#if CHECKING
      if (routine_ptr == NULL) {
        internal_error(
                "decl_var_or_routine: linked routine symbol has NULL routine");
      }  /* if */
#endif /* CHECKING */
      old_decl_has_body = (routine_ptr->assoc_scope != NULL_region_number
#if ASM_FUNCTION_ALLOWED
                     || routine_ptr->storage_class == (a_storage_class)sc_asm
#endif /* ASM_FUNCTION_ALLOWED */
                          );
      if (is_function_def_with_body && old_decl_has_body) {
        /* Previous routine already has a body, and new one does (or will)
           too.  Let processing fall into call to alloc_local_symbol, which
           will give a redefinition error. */
        redecl_error_already_issued = FALSE;
        linked_redecl_error = TRUE;
      } else {
        /* Check that the old and new types are compatible, and form the
           composite type.  Note that there is a second test for compatibility
           after old-style parameter declarations are scanned, if this
           declaration has a body (see function_definition). */
        if (!types_are_compatible(routine_ptr->type, type_ptr)) {
          pos_error(ec_not_compatible_with_previous_decl,
                    &locator->source_position);
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
                                  /*preserve_type_ptr=*/
                                                   is_function_def_with_body);
        }  /* if */
      }  /* if */
    } else {
      /* The linked symbol is a variable, while the new one is a routine,
         or vice-versa; error. */
      pos_error(ec_not_compatible_with_previous_decl,
                &locator->source_position);
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
      a_routine_ptr  rp = homonym_symbol->variant.routine;
      if (rp->special_kind == (a_special_function_kind)sfk_operator &&
          rp->opname_kind == (an_opname_kind)onk_delete) {
        /* Overloading is not allowed for operator delete() (ARM 12.5). */
        pos_error(ec_delete_already_declared, &locator->source_position);
        redecl_error_already_issued = TRUE;
      } else if (!overload_distinguishable(homonym_symbol, type_ptr,
                                           &error_code)) {
        /* The previous declaration and the current one are not "overload
           distinguishable" for a reason given by the error code returned. */
        pos_error(error_code, &locator->source_position);
        redecl_error_already_issued = TRUE;
      } else {
        /* Overloaded function.  Create the new symbol, which will be on the
           list of functions connected to an sk_overloaded symbol. */
        sym = enter_overloaded_symbol((a_symbol_kind)sk_routine, locator,
                                      homonym_symbol, &overload_symbol);
      }  /* if */
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
    sym = enter_local_symbol(is_function ? (a_symbol_kind)sk_routine :
                                           (a_symbol_kind)sk_variable,
                             locator, effective_decl_level,
                             redecl_error_already_issued);
  } else if (is_implicit_function) {
    /* This is an implicit declaration of a function.  The symbol has
       already been entered and marked as declared. */
  } else {
    /* There is a linked symbol that is compatible with the new declaration.
       Record the re-declaration for cross-reference purposes. */
    mark_declared(sym, &locator->source_position,
                  /*save_as_decl_position=*/is_function_def_with_body);
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
                                                 &variable_ptr, &routine_ptr);
  }  /* if */
  if (!is_function) {
    /* The entity being declared is a variable. */
    /* Set the defined flag in the symbol.  The rules are slightly different
       in C and C++, since the latter does not allow tentative definitions.
       In C++ the declaration of any variable without an "extern"
       specification is a definition; in C a storage class of unspecified
       means it is just a declaration (unless there's an initializer). */
    if (storage_class != (a_storage_class)sc_extern) {
      if (C_dialect == C_dialect_cplusplus ||
          storage_class != (a_storage_class)sc_unspecified ||
          decl_scope_level != DEPTH_OF_FILE_SCOPE) {
        sym->defined = TRUE;
      }  /* if */
    }  /* if */
    if (variable_ptr == NULL) {
      /* There is no IL entry, so create one now.  If the variable has
         internal or external linkage, it is entered at the file scope. */
      variable_ptr = make_variable(type_ptr, storage_class, at_file_scope);
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
         see 3.7.2). */
      if (storage_class == (a_storage_class)sc_unspecified) {
        variable_ptr->storage_class = (a_storage_class)sc_unspecified;
      }  /* if */
      /* If the IL entry was previously referenced, and this is a definition
         of the variable, the symbol should be considered to have been
         referenced as well. */
      if (storage_class != (a_storage_class)sc_extern &&
          variable_ptr->source_corresp.referenced) {
         sym->referenced = TRUE;
      }  /* if */
    }  /* if */
    source_corresp_ptr = &variable_ptr->source_corresp;
    /* Link the symbol to the IL variable entry. */
    sym->variant.variable = variable_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL variable entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.variable = variable_ptr;
    }  /* if */
  } else {
    /* The entity being declared is a routine. */
    if (routine_ptr == NULL) {
      /* There is no IL entry, so create one now, and add it to the routine
         list of the file scope. */
      routine_ptr = make_routine(type_ptr, storage_class,
                                 /*at_file_scope=*/TRUE);
      if (C_dialect == C_dialect_cplusplus) {
        if (locator->is_operator_name) {
          routine_ptr->special_kind = (a_special_function_kind)sfk_operator;
          routine_ptr->opname_kind = locator->variant.opname;
        } else if (locator->is_conversion_name) {
          routine_ptr->special_kind = (a_special_function_kind)sfk_conversion;
        }  /* if */
        /* If this is a user-defined conversion or an overloaded operator,
           check for errors in the argument list. */
        check_operator_function_params(routine_ptr, &locator->source_position);
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
      if (is_function_def_with_body) {
        a_boolean saved_referenced_flag;
        /* If this is a definition, unlink the routine entry and relink it
           at the end of the routines list, so that routines appear in the
           order that their bodies appear. */
        remove_from_routines_list(routine_ptr);
        add_to_routines_list(routine_ptr, /*at_file_scope=*/TRUE);
        if (routine_ptr->is_inline) {
          /* Leave the storage class static. */
        } else {
          /* Put in the storage class for the definition (static or 
             unspecified). */
          routine_ptr->storage_class = storage_class;
        }  /* if */
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
#if CHECKING
          if (routine_ptr->special_kind !=
                              (a_special_function_kind)sfk_operator ||
              (routine_ptr->opname_kind != (an_opname_kind)onk_new &&
               routine_ptr->opname_kind != (an_opname_kind)onk_delete)) {
            internal_error(
                       "decl_var_or_routine: compiler_generated unexpected");
          }  /* if */
#endif /* CHECKING */
          /* This is an entry for a compiler generated ::operator new or
             ::operator delete.  It was created during initialization, but
             is overridden by the present declaration. */
          routine_ptr->compiler_generated = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (inline_specified) routine_ptr->is_inline = TRUE;
    source_corresp_ptr = &routine_ptr->source_corresp;
    /* Link the symbol to the IL routine entry. */
    sym->variant.routine = routine_ptr;
    if (*ext_sym != NULL) {
      /* Link the external symbol to the IL routine entry. */
      (*ext_sym)->variant.extern_symbol_descr->variant.routine = routine_ptr;
    }  /* if */
  }  /* if */
  /* Set the source correspondence, but leave it pointing at an outer-scope
     symbol if there is one. */
  if (source_corresp_ptr->assoc_info == NULL) {
    set_source_corresp(source_corresp_ptr, sym);
  } else if (!redeclaration) {
    /* Record a reference to the outer-scope symbol of the same name,
       but do not set the IL entity referenced flag. */
    mark_symbol_referenced(srk_reference, 
                           (a_symbol_ptr)source_corresp_ptr->assoc_info,
                           &locator->source_position);
  }  /* if */
  if (!is_function && is_volatile_qualified_type(type_ptr)) {
    /* A variable with a volatile type is considered to be referenced
       from "elsewhere".  Note that this must be done after set_source_corresp
       because the latter clears the IL referenced flag. */
    source_corresp_ptr->referenced = TRUE;
    sym->referenced = TRUE;
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
              sp->variant.routine->source_corresp.name_linkage ==
                                         (a_name_linkage_kind)nlk_external) {
            pos_st_error(ec_overloaded_function_linkage,
                         &locator->source_position, source_corresp_ptr->name);
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
           not for variables.  Just issue a warning in the latter case. */
        if (is_function) {
          pos_error(ec_incompatible_linkage_specifier,
                    &locator->source_position);
        } else {
          pos_warning(ec_incompatible_linkage_specifier,
                      &locator->source_position);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (linkage == idl_internal) {
    /* Internal linkage. */
    source_corresp_ptr->name_linkage = (a_name_linkage_kind)nlk_internal;
  } else {
    /* No linkage -- e.g., an automatic variable. */
#if CHECKING
    if (source_corresp_ptr->name_linkage != (a_name_linkage_kind)nlk_none) {
      internal_error("decl_var_or_routine:  linkage/nolinkage conflict");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  *symbol_ptr = sym;
  *linkage_ptr = linkage;

#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_var_or_routine */


static void define_static_data_member(a_symbol_locator   *locator,
                                      a_storage_class	 storage_class,
				      a_type_ptr	 type_ptr,
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
    var = sym->variant.variable;
#if CHECKING
    if (var->storage_class != (a_storage_class)sc_static) {
      internal_error("define_static_data_member:  not sc_static");
    }  /* if */
#endif /* CHECKING */
    if (sym->defined) {
      pos_error(ec_redefinition_not_allowed, &locator->source_position);
      err = TRUE;
    } else if (!types_are_compatible(type_ptr, var->type)) {
      pos_error(ec_not_compatible_with_previous_decl,
                &locator->source_position);
      err = TRUE;
    } else {
      /* The type of the variable should be the composite of the two types. */
      var->type = composite_type(type_ptr, var->type);
      /* Mark the static data member defined.  It can only be defined once. */
      sym->defined = TRUE;
      /* Set the IL referenced flag since, as an externally visible variable,
         it could be referenced from another translation unit. */
      var->source_corresp.referenced = TRUE;
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
      pos_error(ec_not_compatible_with_previous_decl,
                &locator->source_position);
    } else if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else if (sym->kind != (a_symbol_kind)sk_undefined &&
               !is_error_locator(*locator)) {
      pos_error(ec_redefinition_not_allowed, &locator->source_position);
    }  /* if */
    err = TRUE;
  }  /* if */
  /* Record the symbol referenced, using the original symbol, even if there
     was an error.  This will make it show up on a cross reference listing. */
  mark_symbol_referenced(srk_reference, sym, &locator->source_position);
  if (err) {
    /* An error occurred which prevents using the object specified as
       target of any initialization that may follow.  Create a dummy
       variable with an error type (to suppress semantic errors on the
       initialization, if any). */
    a_type_ptr  tp = sym->class_of_which_a_member;
    set_to_error_locator(*locator);
    sym = enter_symbol((a_symbol_kind)sk_static_data_member,
                       locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/TRUE);
    sym->variant.variable = make_variable(error_type(),
					  (a_storage_class)sc_static,
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
                                   a_boolean          inline_specified,
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
  if (!is_member_function_symbol(locator->specific_symbol)) {
    /* We must have nonfunction class member.  This is an error, so set sym
       to NULL to force the creation of a fake member function symbol. */
    pos_error(ec_not_compatible_with_previous_decl, &locator->source_position);
    sym = NULL;
  } else {
    /* Look for a member function symbol of this type in the symbol table.
       It is an error if it is  not already there. */
    sym = member_function_redecl_sym(locator->specific_symbol, type_ptr);
    if (sym == NULL) {
      /* No member function with a matching type was found.  Issue an error. */
      if (locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function) {
        /* Special message for overloaded function. */
        pos_st_error(ec_overloaded_function_incompatible_type,
                     &locator->source_position,
                     locator->symbol_header->identifier);
      } else {
        pos_error(ec_not_compatible_with_previous_decl,
                  &locator->source_position);
      }  /* if */
    } else if (sym->variant.routine->compiler_generated) {
      /* Attempting to give a definition for a function that was implicitly
         declared. */
      pos_error(ec_definition_of_implicitly_declared_function,
                &locator->source_position);
      /* Unless a definition has already been generated, reset some flags
         so that that this routine will be treated as user-declared from
         now on. */
      if (!sym->defined) {
        sym->variant.routine->compiler_generated = FALSE;
        sym->variant.routine->is_inline = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (sym == NULL || sym->defined) {
    /* Error case. */
    a_routine_ptr  other_rp = NULL;

    if (sym != NULL) {
      /* Type was okay, but this member function has a body. */
      pos_error(ec_function_redefinition, &locator->source_position);
      other_rp = sym->variant.routine;
      type_ptr->variant.routine.extra_info->implicit_this_param_type =
          other_rp->type->variant.routine.extra_info->implicit_this_param_type;
    }  /* if */
    /* An error has been detected.  Make a "fake" symbol and routine entry so
       that the routine definition can proceed. */
    set_to_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             DEPTH_OF_FILE_SCOPE,
                             /*suppress_redecl_error=*/TRUE);
    sym->class_of_which_a_member = class_type;
    rp = make_routine(type_ptr, (a_storage_class)sc_static,
                      /*at_file_scope=*/TRUE);
    sym->variant.routine = rp;
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
    rp = sym->variant.routine;
    type_ptr->variant.routine.extra_info->implicit_this_param_type =
               rp->type->variant.routine.extra_info->implicit_this_param_type;
    reconcile_routine_types(sym->variant.routine, type_ptr,
                            /*preserve_rout_type=*/FALSE,
                            /*preserve_type_ptr=*/TRUE);
    /* If the routine is a default constructor or a copy constructor, it may
       be that this has not yet been recorded in the symbol. */
    if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
      a_class_symbol_supplement_ptr  cssp;
      cssp = symbol_supplement_for_class(class_type);
      if (!cssp->has_default_constructor && is_default_constructor(rp)) {
        cssp->has_default_constructor = TRUE;
      }  /* if */
      if (!cssp->has_copy_constructor_for_const_object) {
        a_boolean  const_okay, volatile_okay;
        if (is_copy_constructor(rp, class_type, &const_okay,
                                &volatile_okay)) {
          cssp->has_copy_constructor = TRUE;
          cssp->has_copy_constructor_for_const_object = const_okay;
        }  /* if */
      }  /* if */
    }  /* if */
    mark_declared(sym, &locator->source_position,
                  /*save_as_decl_position=*/TRUE);
    copy_source_position(locator->source_position,
                         rp->source_corresp.decl_position);
  }  /* if */
  if (inline_specified) sym->variant.routine->is_inline = TRUE;
  sym->defined = TRUE;
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


void decl_typedef(a_symbol_locator   *locator,
                  a_type_ptr         type_ptr,
                  a_symbol_ptr       *symbol_ptr)
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
  if ((sym = normal_id_lookup(locator, IDL_NO_OPTIONS)) != NULL) {
    if (sym->decl_scope == scope_stack[decl_scope_level].number &&
        (sym->kind == (a_symbol_kind)sk_type ||
         (C_dialect == C_dialect_cplusplus && is_type_symbol(sym)))) {
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
            pos_warning(ec_duplicate_typedef, &locator->source_position);
          }  /* if */
          mark_declared(sym, &locator->source_position,
                        /*save_as_decl_position=*/FALSE);
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
         case a redefinition here is legal, or else it is not type name
         symbol, in which case the error message will be issued by
         enter_symbol. */
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus) {
    /* No symbol by this name.  See if this is a tagless class, struct,
       union, or enum type.  If so, the present name will serve as the
       tag (ARM 7.1.3).  Note that we do NOT want to do a skip_typerefs on
       the type; only if *type_ptr itself lacks an associated tag symbol
       with a name do we want to create a new symbol. */
    if (!is_error_type(type_ptr) && !is_error_locator(*locator)) {
      sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      if (sym == NULL) {
        if (type_ptr->kind == (a_type_kind)tk_integer &&
            type_ptr->variant.integer.enum_type) {
          /* This is a tagless enum -- e.g., typedef enum { ... } E; */
          sym = enter_local_symbol((a_symbol_kind)sk_enum_tag, locator,
                                   decl_scope_level,
                                   /*suppress_redecl_error=*/FALSE);
          sym->variant.type = type_ptr;
          set_source_corresp(&(type_ptr->source_corresp), sym);
          suppress_redecl_error = TRUE;
#if CHECKING
        } else if (is_class_struct_union_type(type_ptr)) {
          /* A tagless class -- e.g., typedef class { ... } C -- should always
             have a tag symbol. */
          internal_error("decl_typedef: expected unnamed tag sym for class");
#endif /* CHECKING */
        }  /* if */
      } else if (is_unnamed_class_symbol(sym)) {
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
  add_to_types_list(tp, decl_scope_level, in_old_style_param_decl_list);

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


static void decl_parameter(a_param_id_ptr     param_id)
/*
Enter the declaration of an identifier for a parameter.  *locator gives
the symbol locator (and thus its name and its declaration position).
Under ordinary circumstances, create and enter a symbol entry, and return
a pointer to it in *symbol_ptr.
*/
{
  a_symbol_ptr   sym;
  a_variable_ptr vp;

  db_enter(3, "decl_parameter");
  vp = make_param_variable(param_id->type, param_id->storage_class);
  add_to_parameters_list(vp);
  sym = param_id->symbol;
  if (sym == NULL) {
    /* This param_id entry represents an unnamed parameter (which is legal
       in function definitions in C++). */
  } else {
    remove_symbol(sym);
    set_symbol_kind(sym, (a_symbol_kind)sk_variable);
    sym->variant.variable = vp;
    set_source_corresp(&(vp->source_corresp), sym);
    reenter_symbol(sym, decl_scope_level, /*suppress_error=*/TRUE);
    sym->defined = TRUE;
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
     list, and a return type of "int".  See 3.3.2.2, semantics. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.return_type =
                                         integer_type((an_integer_kind)ik_int);
  rout_type->variant.routine.extra_info->param_type_list = NULL;
  rout_type->variant.routine.extra_info->prototyped = FALSE;
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Declare the function identifier. */
  decl_var_or_routine(&locator, (a_storage_class)sc_extern, rout_type,
                      /*is_implicit_function=*/TRUE,
                      /*is_function_def_with_body=*/FALSE,
                      /*inline_specified=*/FALSE,
                      /*is_main_function=*/FALSE,
                      &symbol_ptr, &linkage, &old_type, &ext_sym);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  symbol_ptr->variant.routine->source_corresp.referenced = TRUE;
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
    label_sym->variant.label = label = alloc_label();
    add_to_labels_list(label);
    set_source_corresp(&label->source_corresp, label_sym);
    /* The exec_stmt field stays NULL to indicate that the declaration
       has not been (fully?) processed yet. */
  }  /* if */
  if (!is_error_locator(locator_for_curr_id)) {
    /* Record the right kind of reference to the label symbol. */
    if (is_definition) {
      /* Note that we want mark_declared called even if the symbol
         was previously entered.  Labels are strange in that a reference
         can come up before a declaration. */
      mark_declared(label_sym, &pos_curr_token,
                    /*save_as_decl_position=*/TRUE);
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
  return(label_sym->variant.label);
}  /* scan_label */


static a_type_ptr pointer_declarator(a_type_ptr  specifiers_type,
			             a_type_ptr  *bottom_pointer_derived_type)
/*
Scan the pointer component of a declarator.  Syntax for C++ (ARM 8.0):

8.0	ptr-operator:
		* cv-qualifier-list
			           opt
		& cv-qualifier-list
				   opt
		complete-class-name :: * cv-qualifier-list
							  opt

where a cv-qualifier-list consists of const or volatile or both.
Only the first form is accepted in C.
*/
{
  a_type_ptr     complete_type = specifiers_type;
  a_boolean      err;
  a_type_ptr     class_type;
  a_boolean      is_file_scope_qualifier, has_global_qualifier;
  a_token_cache  token_cache;


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
        (C_dialect == C_dialect_cplusplus && curr_token == tok_ampersand)) {
      set_err_pos_to_curr_token();
      if (complete_type != NULL) {
        /* Normal case -- the specifiers type is given, and the pointer or
           reference type can be attached directly to it.  (Or, this is a
           pointer to a pointer type or a reference to a pointer type). */
        a_type_ptr  temp_type = skip_typerefs(complete_type);
        if (curr_token == tok_star) {
          if (is_reference_type(temp_type)) {
            /* Type "pointer to reference to anything" is illegal. */
            error(ec_pointer_to_reference);
            err = TRUE;
          }  /* if */
          complete_type = make_pointer_type(err ? error_type() :
                                                  complete_type);
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
          complete_type = make_reference_type(err ? error_type() :
                                                    complete_type);
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
               is_qualified_name_start() &&
               get_class_qualifier(&token_cache, &class_type,
                                   &is_file_scope_qualifier,
                                   &has_global_qualifier, &err)) {
      /* A class qualifier is present.  This is a pointer-to-member
         declarator if the current token is a "*". */
      if (curr_token == tok_star && !is_file_scope_qualifier) {
        /* It is a pointer-to-member declarator.  Construct the type entry. */
        complete_type = ptr_to_member_type(complete_type,
                                           class_type == NULL ? error_type() :
                                                                class_type);
      } else {
        /* The class qualifier is not followed by a "*", so back up to the
           start of the class qualifier and exit the loop. */
        rescan_cached_tokens(&token_cache);
        break;
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

      (void)decl_specifiers(DSI_NO_INPUT_FLAGS, &dso_flags,
                            &dummy_storage_class, &dummy_type_ptr);
      /* Note -- the check for dangling_type_specifier is not relevant here. */
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


void declarator(a_decl_flag_set   input_flags,
                a_decl_flag_set   *output_flags,
                a_type_ptr        specifiers_type,
                a_type_ptr        member_parent_type,
                a_symbol_locator  *locator,
                a_type_ptr        *p_complete_type,
                a_type_ptr        *p_bottom_derived_type,
                a_func_info_block *func_info,
                an_expr_node_ptr  *dim_expr_ptr)
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
  a_boolean       is_member_function_def = FALSE;
  a_boolean       real_declarator_allowed;
  a_boolean       abstract_declarator_allowed;
  a_boolean       is_name_start;
  a_symbol_header_ptr
                  class_symbol_header;
  a_boolean       is_constructor = FALSE, is_destructor = FALSE;
  a_boolean       is_nonstatic_member_function = FALSE;
  a_boolean       nonconstant_dimension_allowed;
  a_boolean       parenthesized_initializer_allowed;

  db_enter(3, "declarator");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, declarator_pos);
  *output_flags = DO_NO_OUTPUT_FLAGS;
  real_declarator_allowed = input_flags & DI_REAL_DECLARATOR_ALLOWED;
  abstract_declarator_allowed = input_flags & DI_ABSTRACT_DECLARATOR_ALLOWED;
  is_constructor = (input_flags & DI_IS_CONSTRUCTOR) != 0;
  parenthesized_initializer_allowed =
                      (C_dialect == C_dialect_cplusplus && !is_constructor &&
                       (input_flags & DI_PARENTHESIZED_INITIALIZER_ALLOWED));
  nonconstant_dimension_allowed =
                            (input_flags & DI_DIMENSION_EXPRESSION_ALLOWED);
  if (!real_declarator_allowed) {
    func_info = NULL;
    locator = NULL;
  }  /* if */
  if (func_info != NULL) clear_func_info(func_info);
  /* Set the locator to indicate there is no identifier. */
  if (locator != NULL) set_to_error_locator(*locator);
  *dim_expr_ptr = NULL;
  /* Look for any initial "*" list indicating pointer types. */
  bottom_pointer_derived_type = NULL;
  complete_type = pointer_declarator(specifiers_type,
                                     &bottom_pointer_derived_type);
  derived_type = NULL;
  bottom_derived_type = NULL;
  add_stop_token(tok_lbracket);
  add_stop_token(tok_lparen);
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
      if (curr_token == tok_rparen || is_decl_start() ||
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
               &bottom_derived_type, func_info, dim_expr_ptr);
    if (local_do_flags & DO_REAL_DECLARATOR_SCANNED) {
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
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
      /* Identifier is omitted in an abstract declarator. */
    } else {
      /* Real (non-abstract) declarator. */
      declarator_pos = pos_curr_token;
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      if (C_dialect == C_dialect_cplusplus) {
        /* If this declaration appears in the immediate context of a class
           definition and the current token is an identifier representing
           the name of the current class, see if this is a qualified name
           and if so change it into a simple name (e.g., A::x becomes x,
           its equivalent in A's scope). */
        (void)simplify_curr_class_qualified_name();
      }  /* if */
      if (is_qualified_name_start()) {  /* Identifier or "::". */
        /* The declarator may be a qualified name or a normal name. */
        an_id_lookup_options_set lookup_options = IDL_NO_OPTIONS;
        /* The unary "::" is not allowed in declarators. */
        if (curr_token == tok_colon_colon ||
            curr_token == tok_identifier &&
                                locator_for_curr_id.is_global_qualified_name) {
          error(ec_unary_colon_colon_in_declarator);
          /* Suppress a second error on the name not being found in the
             indicated class. */
          lookup_options |= IDL_SUPPRESS_QUALIFIED_NAME_NOT_FOUND_ERROR;
        }  /* if */
        /* See if the name is a qualified name, like "A::x" or "::j". */
        if (get_qualified_name(lookup_options)) {
          if (input_flags & DI_QUALIFIED_NAME_ALLOWED) {
            a_symbol_ptr sym = locator_for_curr_id.specific_symbol;
            /* See if the name is the name of a member function. */
            if (is_member_function_symbol(sym)) {
              /* It is a member function.  Save information about the class
                 needed to reopen the class scope if a function declarator
                 is scanned. */
              is_member_function_def = TRUE;
              parenthesized_initializer_allowed = FALSE;
              member_parent_type = sym->class_of_which_a_member;
            }  /* if */
          } else {
            /* This is a declaration in which a qualified name is not
               allowed. */
            pos_error(ec_qualified_name_not_allowed, &declarator_pos);
            set_to_error_locator(locator_for_curr_id);
          }  /* if */
        }  /* if */
        /* Save information on the identifier to be declared. */
        *locator = locator_for_curr_id;
        (void)get_token();
      } else if (get_destructor_name(&class_symbol_header)) {
        /* A destructor name, like "~A".  It must have the same name as
           the class currently being defined, it must be followed by a
           left paren, and the specifiers must include no type. */
        if (class_symbol_header == NULL) {
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
            if (class_symbol_header != class_sym->header) {
              /* The name on the destructor is not the name of the class. */
              error(ec_bad_destructor_decl);
            } else if (!is_void_type(specifiers_type) ||
                       !(input_flags & DI_DESTRUCTOR_SPECIFIERS)) {
              /* The specifiers, including possibly the type specifier,
                 are not consistent with a destructor declaration (e.g., a
                 destructor cannot be specified "static" or "void"). */
              error(ec_bad_destructor_decl);
            } else {
              /* Valid destructor declaration. */
              member_parent_type = ssep->il_scope->variant.assoc_type;
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
        } else if (curr_token != tok_lparen) {
          /* A valid destructor name is not followed by a left parenthesis. */
          error(ec_exp_lparen);
        }  /* if */
        parenthesized_initializer_allowed = FALSE;
      } else if (get_opname()) {
        /* The name is an operator name like "operator+". */
        *locator = locator_for_curr_id;
        (void)get_token();
        parenthesized_initializer_allowed = FALSE;
      } else {
        copy_source_position(pos_curr_token, locator->source_position);
        syntax_error(ec_exp_identifier);
        parenthesized_initializer_allowed = FALSE;
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
            pos_st_error(ec_nonmember_operator_not_allowed,
                         &locator->source_position, s);
            set_to_error_locator(*locator);
          }  /* if */
        }  /* if */
        if (required_token(tok_lparen, ec_exp_lparen)) goto function_lparen;
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
      if (parenthesized_initializer_allowed) {
        if (curr_token != tok_rparen && curr_token != tok_ellipsis &&
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

            clear_token_cache(&cache);
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
              if (get_token() == tok_lbrace || is_decl_start()) {
                /* This looks exactly like a function declaration with an
                   old style parameter list. */
                is_function_decl = TRUE;
              }  /* if */
            }  /* if */
            rescan_cached_tokens(&cache);
          }  /* if */
          if (!is_function_decl) {
#if CHECKING
            a_type_ptr  tp = skip_typerefs(complete_type);
            if (!is_scalar_type(tp) && !is_ptr_or_ref_type(tp)) {
              /* Should have been checked when the input flag was defined. */
              if (!is_class_struct_union_type(tp)) {
                internal_error("declarator: unexpected type for paren init");
              }  /* if */
            }  /* if */
#endif /* CHECKING */
            *output_flags |= DO_PARENTHESIZED_INITIALIZER;
            /* Function_declarator should not be called, so exit the loop. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
function_lparen:
      if (is_member_function_def) {
        /* The parameters of member functions are scanned with the original
           class reactivated. */
        push_class_reactivation_scope(member_parent_type);
      }  /* if */
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      if (C_dialect == C_dialect_cplusplus) {
        if (func_info == NULL || derived_type != NULL) {
          /* If the function is pointed to by a pointer-to-member type, we need
             to pass the class-of-which-a-member to function_declarator. */
          a_type_ptr tp = derived_type;
          member_parent_type = NULL;
          if (tp != NULL) {
            if (!is_array_type(tp) ||
                (tp = underlying_array_element_type(tp)) != NULL) {
              tp = skip_typerefs(tp);
              if (is_ptr_to_member_type(tp)) {
                /* Declaration of a pointer to member function. */
                member_parent_type = pm_class_type(tp);
                is_nonstatic_member_function = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          func_info = NULL;
          is_constructor = is_destructor = FALSE;
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
      }  /* if */
      function_declarator(&new_type_ptr, func_info, locator,
                          member_parent_type, is_nonstatic_member_function,
                          is_constructor, is_destructor);
      if (is_member_function_def) {
        pop_class_reactivation_scope();
      }  /* if */
    } else {
      /* Left bracket, indicating array declarator. */
      if (nonconstant_dimension_allowed) {
        /* In C++ a array declarator that appears in an operator new()
           expression may have a nonconstant expression in the first
           dimension (ARM 5.3.3).  Subsequent dimension must be constants. */
        nonconstant_array_declarator(&new_type_ptr, dim_expr_ptr);
        nonconstant_dimension_allowed = FALSE;
      } else {
        /* The normal case. */
        array_declarator(&new_type_ptr);
      }  /* if */
    }  /* if */
    /* Add the new type to the bottom of the existing derived type list.
       Note that this involves error checking. */
    add_to_derived_type_list(new_type_ptr,
                             &derived_type, &bottom_derived_type);
    parenthesized_initializer_allowed = FALSE;
  }  /* while */
  remove_stop_token(tok_lbracket);
  remove_stop_token(tok_lparen);
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
  if (specifiers_type != NULL &&
      (*output_flags & DO_REAL_DECLARATOR_SCANNED)) {
    (skip_typerefs(specifiers_type))->source_corresp.referenced = TRUE;
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
    if (complete_type != NULL) {
      if (bottom_derived_type->kind == (a_type_kind)tk_error) {
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
  if (bottom_pointer_derived_type != NULL) {
    bottom_derived_type = bottom_pointer_derived_type;
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
*/
{
  a_symbol_ptr      tag_sym;
  a_token_kind      next_tok;

  db_enter(3, "scan_tag_name");
  *tag_resolution = FALSE;
  /* Find a declaration of this tag in the current scope.  (We only look
     in the current scope for now, but we may have to do a complete lookup
     later.) */
  if (curr_token == tok_colon_colon || next_token() == tok_colon_colon) {
    /* This looks like a qualified name, which is not allowed here. */
    error(ec_qualified_name_not_allowed);
    (void)get_qualified_name(IDL_NO_OPTIONS);
    set_to_error_locator(*locator);
    tag_sym = NULL;
  } else {
    tag_sym = curr_scope_tag_symbol(tag_kind);
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
         is not its definition, so it is either a reference or a "vacuous
         declaration" (e.g. "struct S;" or "enum E;").  (The effect of a
         vacuous declaration (unless we are in pcc mode) is to establish the
         name in the current scope, even if the tag name exists in a containing
         scope or is inherited from a base class.)  For enum declarations it
         is an extension in strict ANSI mode. */
      if (next_tok == tok_semicolon && check_for_vacuous_decl &&
          C_dialect != C_dialect_pcc) {
        /* This is indeed a vacuous declaration.  Leave tag_sym set to NULL
           to force the creation of a new symbol in the current scope. */
        if (tag_kind == (a_symbol_kind)sk_enum_tag && strict_ansi_mode) {
          pos_warning(ec_nonstd_forward_def_enum, &locator->source_position);
        }  /* if */
      } else {
        /* This may be a reference to an existing tag from a containing
           scope or a base class.  This can be ascertained by doing a full
           lookup of the tag name (before it was done just for the current
           scope). */
        tag_sym = curr_tag_symbol(tag_kind);
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
            while (scope_stack[*effective_decl_level].kind ==
                                   (a_scope_kind)sck_class_struct_union ||
                   scope_stack[*effective_decl_level].kind ==
                                   (a_scope_kind)sck_func_prototype) {
              (*effective_decl_level)--;
            }  /* while */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
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
  long                     curr_value;
  a_constant_ptr           enum_con;
  a_constant_ptr           end_of_enum_con_list;
  long                     max_value = TARG_INT_MIN;
  long                     min_value = TARG_INT_MAX;
  a_boolean                done;
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
#if CHECKING
  if (curr_token != tok_enum) internal_error("enum_specifier: expected enum");
#endif /* CHECKING */
  /* If there is an identifier next, it is a tag.  It can be the declaration
     of a new tag or a reference to an existing tag. */
  (void)get_token();
  tag_id_present = is_qualified_name_start();
  if (tag_id_present) {
    a_boolean  tag_resolution;
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    *declares_something = TRUE;
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
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
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
    }  /* if */
    /* When an enumeration is defined within a class definition, its access
       should be set based on the access recorded in the current scope stack
       entry and its parent class should be recorded. */
    enum_type->source_corresp.access = access;
    enum_type->source_corresp.class_of_which_a_member =
                                                class_of_which_a_member;
  } else {
    /* Using an existing type.  Fetch the enumerated type pointer from it. */
    enum_type = tag_sym->variant.type;
    /* Record cross-reference information. */
    if (curr_token == tok_lbrace) {
      mark_declared(tag_sym, &locator.source_position,
                    /*save_as_decl_position=*/TRUE);
    } else {
      mark_referenced(tag_sym, &locator.source_position);
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* Scan the enumeration itself.  Since the enumeration type entry is
       allocated in the file scope memory region, all its components should
       also be.  Switch to the file scope memory region here at the start of
       the definition and switch back when we reach the right brace. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    *defines_something = TRUE;
    if (tag_sym != NULL) tag_sym->defined = TRUE;
    (void)get_token();
    if (C_dialect == C_dialect_cplusplus && curr_token == tok_rbrace) {
      /* An enumerator constant list is optional in C++.  Give the enumeration
         an integer kind of char. */
      enum_type->variant.integer.int_kind = plain_char_int_kind;
      /*  Advance past the right brace. */
      (void)get_token();
    } else {
      add_stop_token(tok_rbrace);
      curr_value = -1;  /* Incremented before first use. */
      end_of_enum_con_list = NULL;
      if (C_dialect == C_dialect_cplusplus) {
        /* In C++ (see ARM 7.2) the type of an enumerator is the same as that
           of its enumeration -- i.e., enum_type and enum_con_type are the
           same.  enum_type may have to be adjusted later to match the range
           of the enumeration constant values. */
        enum_con_type = enum_type;
      } else {
        /* The type of the constants is always "int", regardless of
           the type of the enumerated type (see 3.5.2.2).  However, it is
           tagged with enum_constant_list pointing to the constant list, so
           that enum compatibility warnings can be generated later. */
        enum_con_type = alloc_type((a_type_kind)tk_integer);
        enum_con_type->variant.integer.int_kind = (an_integer_kind)ik_int;
        /* enum_con_type->variant.integer.enum_constant_list is set below when
           the first constant is put on the list. */
        set_type_size(enum_con_type);
      }  /* if */
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
        /* See if "= constant-expression" follows. */
        if (curr_token == tok_assign) {
          (void)get_token();
          /* Scan the constant expression. */
          scan_integral_constant_expression(&constant);
          if (is_error_constant(&constant)) {
            curr_value = 0;
          } else {
#if CHECKING
            if (constant.kind != (a_constant_repr_kind)ck_integer) {
              internal_error("enum_specifier: enum value not int");
            }  /* if */
#endif /* CHECKING */
            curr_value = constant.variant.integer_value;
            /* Check the value to see if it is out of range.  (3.5.2.2,
               constraints) */
            if (curr_value > TARG_INT_MAX ||
                curr_value < TARG_INT_MIN) {
              error(ec_enum_value_out_of_int_range);
              curr_value = 0;
            }  /* if */
          }  /* if */
        } else {
          /* No explicit value, so use a value one larger than the previous
             value. */
          /* Check the value to see if it is out of range.  (3.5.2.2,
             constraints) */
          if (curr_value == TARG_INT_MAX) {
            error(ec_enum_value_out_of_int_range);
            curr_value = 0;
          } else {
            curr_value++;
          }  /* if */
        }  /* if */
        /* Enter the enumeration constant identifier. */
        enum_sym = enter_local_symbol((a_symbol_kind)sk_constant, &locator,
                                      decl_scope_level,
                                      /*suppress_redecl_error=*/FALSE);
        enum_sym->defined = TRUE;
        *declares_something = TRUE;
        /* Track the highest and lowest values in the enumeration.  These are
           used to determine the appropriate representation type. */
        if (curr_value < min_value) min_value = curr_value;
        if (curr_value > max_value) max_value = curr_value;
        /* Assign the value to the enumeration constant. */
        enum_con = alloc_constant((a_constant_repr_kind)ck_integer);
        set_source_corresp(&(enum_con->source_corresp), enum_sym);
        enum_sym->variant.constant = enum_con;
        enum_con->type = enum_con_type;
        enum_con->variant.integer_value = curr_value;
        /* Specify membership and access. */
        enum_con->source_corresp.class_of_which_a_member =
                enum_sym->class_of_which_a_member = class_of_which_a_member;
        enum_con->source_corresp.access = access;
        /* Add the enumeration constant to the list under the enumerated
           type. */
        if (end_of_enum_con_list == NULL) {
          enum_type->variant.integer.enum_constant_list = enum_con;
          if (C_dialect != C_dialect_cplusplus) {
            enum_con_type->variant.integer.enum_constant_list = enum_con;
          }  /* if */
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
              a warning (the gcc compiler source includes
              cases like this, and that source is part of the SPEC benchmark
              suite). */
          done = TRUE;
          if (C_dialect != C_dialect_pcc) {
            if (strict_ansi_mode) {
              pos_warning(ec_nonstd_extra_comma, &pos_comma);
            } else {
              pos_remark(ec_nonstd_extra_comma, &pos_comma);
            }  /* if */
          }  /* if */
        }  /* if */
        remove_stop_token(tok_comma);
      } while (!done);
      remove_stop_token(tok_rbrace);
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
        if (min_value >= targ_min_char && max_value <= targ_max_char) {
          /* "Plain" char. */
          enum_type->variant.integer.int_kind = plain_char_int_kind;
        } else if (min_value >= TARG_SCHAR_MIN &&
                   max_value <= TARG_SCHAR_MAX) {
          /* Signed char. */
          enum_type->variant.integer.int_kind =
                                          (an_integer_kind)ik_signed_char;
        } else if (min_value >= 0 && max_value <= TARG_UCHAR_MAX) {
          /* Unsigned char. */
          enum_type->variant.integer.int_kind =
                                          (an_integer_kind)ik_unsigned_char;
        } else if (min_value >= TARG_SHRT_MIN && max_value <= TARG_SHRT_MAX) {
          /* Short. */
          enum_type->variant.integer.int_kind = (an_integer_kind)ik_short;
        } else if (min_value >= 0 && max_value <= TARG_USHRT_MAX) {
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
    }  /* if */
    /* Set the type size (based on the integral type it is mapped onto). */
    set_type_size(enum_type);
    /* Add the type to the types list for the current scope.  This is done
       after the closing brace, if any, to get the IL types list in the
       right order.  Note that incomplete enums are not added
       to the type list, because the actual definition has not yet
       appeared.  See pop_scope; they get added at the end of the scope.
       Tags that were declared in a prototype scope were added to the
       type list at the end of the prototype scope, so do not add them
       again. */
    if (!prototype_tag_resolution) {
      add_to_types_list(enum_type, effective_decl_level,
                        in_old_style_param_decl_list);
    }  /* if */
    /* Switch back from the file scope memory region to whatever region
       was current upon entry. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */

  *type_ptr = enum_type;
  db_exit();
}  /* enum_specifier */


/*
Local macro for decl_specifiers: if curr_token_type_symbol has not
been determined, determine it now.
*/
#define determine_curr_token_type_symbol()                            \
{ if (!determined_curr_token_type_symbol) {                           \
    curr_token_type_symbol = curr_type_symbol();                      \
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
was encountered (C++ only).

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

  enum {bt_none, bt_void, bt_char, bt_int,
        bt_float, bt_double, bt_typedef,
        bt_struct_union, bt_enum, bt_no_type}  basic_type = bt_none;
  enum {sign_none, sign_signed, sign_unsigned} sign       = sign_none;
  enum {size_none, size_short, size_long}      size       = size_none;

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
        } else {
          if (num_specifiers != 0 && C_dialect != C_dialect_pcc) {
            /* Storage class specifier other than first is an obsolescent
               feature (see 3.9.3). */
            if (*output_flags & DSO_FRIEND) {
              /* An error is issued when a storage class appears on a friend
                 declaration, so suppress the warning. */
            } else {
              warning(ec_storage_class_not_first);
            }  /* if */
          }  /* if */
          if (is_parameter && curr_token != tok_register &&
              (C_dialect != C_dialect_cplusplus || curr_token != tok_auto)) {
            /* For parameters, the only allowed storage class specifiers are
	       "register" and (in C++ only) "auto". */
            error(ec_bad_param_storage_class);
            err = TRUE;
          } else if (is_inline && curr_token != tok_static) {
            error(ec_bad_storage_class_with_inline);
            err = TRUE;
          } else if (is_member_decl &&
                     curr_token != tok_static && curr_token != tok_typedef) {
            error(ec_bad_member_storage_class);
            err = TRUE;
          } else {
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
            }  /* switch */
          }  /* if */
        }  /* if */
        break;
      case tok_const:
        /* const type qualifier (3.5.3). */
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          /* const may not appear in a new-type-name. */
          error(ec_const_volatile_not_allowed);
          err = TRUE;
        } else if (is_const_qualified) {
          /* const may not appear more than once. */
          error(ec_dupl_type_qualifier);
          err = TRUE;
        } else {
          is_const_qualified = TRUE;
          /* Set the output_flags bit, for the case where only type qualifiers
	     are acceptable, and therefore there is no type entry in which to
	     return the const qualifier. */
	  *output_flags |= DSO_CONST_QUALIFIED;
        }  /* if */
        break;
      case tok_volatile:
        /* volatile type qualifier (3.5.3). */
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          /* volatile may not appear in a new-type-name. */
          error(ec_const_volatile_not_allowed);
          err = TRUE;
        } else if (is_volatile_qualified) {
          /* volatile may not appear more than once. */
          error(ec_dupl_type_qualifier);
          err = TRUE;
        } else {
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
          if (get_token() == tok_identifier && next_token() == tok_semicolon) {
            /* Special case -- a friend declaration of the form "friend T;"
               which is taken to mean the same as "friend class T;" by cfront
               (even if T has not yet been defined).  Although there is no
               support for this syntax in the ARM, we accept it since it is
               widely used in older C++ code. */
            remark(ec_bad_friend_decl);
            vacuous_decl_allowed = FALSE;
            goto process_class_specifier;
          } else {
            /* Not the special case -- restore the current token and continue
               processing. */
            unget_token();
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
      case QUALIFIED_NAME_START_CASE:  /* Identifier or "::". */
        /* Identifier. */
        if (C_dialect == C_dialect_cplusplus) {
          if (is_overload_specifier()) {
            /* Special case -- the "overload" pseudo keyword.  We ignore it and
               advance to the next token. */
            warning(ec_overload_ignored);
            break;
          } else {
            /* Check for a constructor declaration.  The following conditions
               must be satisfied:  (1) we are inside a class definition;
               (2) the current token is the name of the class being defined
               (note that typedef names are not allowed); (3) the declaration
               has no other specifiers besides "inline" (e.g., "const" is
               not allowed); (4) the next token is a left parenthesis; (5)
               the token following the left paren is a right paren or the
               start of a formal parameter declaration. */
            if (is_member_decl &&
                num_specifiers == (is_inline ? 1 : 0) &&
                has_name_of_curr_class(&locator_for_curr_id)) {
              /* The name is the same as that of a class being defined.  This
                 is treated as a constructor declaration if the next two
                 tokens are a left paren and declaration start token.  Use
                 token caching in the look-ahead, since the tokens will have
                 to be rescanned no matter what. */
              a_token_cache    cache;
              a_boolean        is_constructor = FALSE;

              clear_token_cache(&cache);
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
                    is_decl_start()) {
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
                a_type_ptr    tp = scope_stack[decl_scope_level].assoc_type;
                a_symbol_ptr  tag_sym =
                                   (a_symbol_ptr)tp->source_corresp.assoc_info;
                basic_type = bt_no_type;
                *type_ptr = make_reference_type(tp);
                *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
                /* Turn the current locator from a "specific symbol" locator
                   into a constructor locator. */
                determine_curr_token_type_symbol();
                if (curr_token_type_symbol != tag_sym) {
                  /* This can only mean that another member has been declared
                     with the class name.  Issue an error. */
                  if (locator_for_curr_id.specific_symbol->
                               class_of_which_a_member == tp) {
                    error(ec_id_already_declared);
                  }  /* if */
                  locator_for_curr_id.specific_symbol = tag_sym;
                }  /* if */
                change_class_locator_into_constructor_locator(
                                                     &locator_for_curr_id);
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
        determine_curr_token_type_symbol();
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
          /* The identifier is a type name and should be treated as a
             type specifier. */
          if (locator_for_curr_id.is_semivisible_nested_class) {
            /* The symbol in the locator is a nested class that is not
               visible according to the ARM lookup rules but is returned
               in support of the nested class anachronism (ARM 18.3.5).
               Issue a warning. */
            sym_warning(ec_nested_class_anachronism,
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
        if (locator_for_curr_id.is_operator_name ||
            locator_for_curr_id.is_conversion_name) {
          /* This identifier represents something like "A::operator+" or
             "A::operator int"." */
          goto operator_or_conversion_name;
        }  /* if */
        if (num_specifiers == 0 &&
            !(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
          /* If this is the first specifier, and this identifier is undefined,
             assume that we are dealing with a name that was supposed to be
             declared as a typedef.  Note that we do not get here on
             declarations, so this bit of error recovery tweaking applies
             only to things like prototyped parameter declarations and
             members of structs/unions. */
          if ((symbol_list_from_locator(locator_for_curr_id)) == NULL) {
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
        } else if (num_specifiers ==
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
              *type_ptr = make_reference_type(sym->class_of_which_a_member);
            } else if (is_destructor_symbol(sym)) {
              *output_flags |= DSO_DESTRUCTOR;
              basic_type = bt_no_type;
            }  /* if */
          }  /* if */
          goto exit_loop;
        }  /* if */
        /* For non-typedef identifiers, branch to the default case. */
        goto something_unexpected;
      case tok_operator:
        (void)get_opname();
operator_or_conversion_name:
        if (locator_for_curr_id.is_conversion_name) {
          if (basic_type != bt_none || sign != sign_none ||
              size != size_none) {
            error(ec_type_specifier_not_allowed);
            err = TRUE;
          }  /* if */
          basic_type = bt_typedef;
          *type_ptr = locator_for_curr_id.variant.conversion_result_type;
        } else if (locator_for_curr_id.is_operator_name && is_member_decl) {
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
      case tok_compl:
        if (is_member_decl) {
          if (num_specifiers == 0) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          /* The only specifiers that are allowed with a destructor are
             virtual (ARM 12.4) and inline.  Additional checking is done
             in declarator. */
          if (num_specifiers == 0 ||
              (num_specifiers == 1 &&
               (*output_flags & (DSO_VIRTUAL | DSO_INLINE))) ||
              (num_specifiers == 2 &&
               (*output_flags & (DSO_VIRTUAL & DSO_INLINE)))) {
            *output_flags |= DSO_DESTRUCTOR;
            basic_type = bt_no_type;
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
        }  /* if */
        goto exit_loop;
    }  /* switch */
    (void)get_token();
    if (C_dialect == C_dialect_cplusplus) {
      /* If this declaration appears in the immediate context of a class
         definition and the current token is an identifier representing the
         name of the current class, see if this is a qualified name and if
         so change it into a simple name (e.g., A::x becomes x, its equivalent
         in A's scope). */
      (void)simplify_curr_class_qualified_name();
    }  /* if */
no_get_token:
    num_specifiers++;
    determined_curr_token_type_symbol = FALSE;
    /* Check for special conditions that will cause this loop to terminate. */
    if (defines_something) {
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
      if (is_qualified_name_start()) determine_curr_token_type_symbol();
      if (is_type_specifier() ||
          (determined_curr_token_type_symbol &&
              curr_token_type_symbol != NULL &&
              *storage_class != (a_storage_class)sc_typedef)) {
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
       destructors) void. */
    if (basic_type == bt_none) {
      basic_type = bt_int;
    } else if (basic_type == bt_no_type) {
      if (*output_flags & DSO_DESTRUCTOR) basic_type = bt_void;
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
        pos_warning(ec_bad_combination_of_type_specifiers, &start_pos);
      }  /* if */
    } else if (basic_type == bt_double && sign == sign_none &&
               size == size_long) {
      /* float. */
      kind = (a_type_kind)tk_float;
      fkind = (a_float_kind)fk_long_double;
    } else if ((basic_type == bt_struct_union || basic_type == bt_enum) &&
               sign == sign_none && size == size_none) {
      /* struct, union, or enum. */
    } else if (basic_type == bt_typedef && sign == sign_none &&
               size == size_none) {
      /* typedef. */
    } else if (basic_type == bt_no_type) {
      /* Constructor return type.  *type_ptr is already set. */
#if CHECKING
      if (!(*output_flags & DSO_CONSTRUCTOR)) {
        internal_error("decl_specifiers: expected a constructor");
      } else if (*type_ptr == NULL || !is_reference_type(*type_ptr) ||
                 !is_class_struct_union_type(type_pointed_to(*type_ptr))) {
        internal_error("decl_specifiers: bad type pointer for constructor");
      }  /* if */
#endif /* CHECKING */
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
	if (ikind == (an_integer_kind)ik_int && explicitly_signed) {
	  /* For an explicitly "signed" int, use a different type entry.
	     Plain "int" and "signed int" have to be kept separate because
	     they may mean different things as bit-field types. */
	  *type_ptr = signed_int_type();
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
  a_storage_class           storage_class;
  a_decl_flag_set           dso_flags, do_flags;
  a_type_ptr                bottom_derived_type;
  a_source_position         start_pos;
  an_expr_node_ptr          dim_expr_ptr;

  db_enter(3, "type_name");
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED, &dso_flags,
			&storage_class, type_ptr);
  if (C_dialect == C_dialect_cplusplus && dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  /* Note -- the check for dangling_type_specifier is not relevant here. */
  if (is_abstract_declarator_start()) {
    declarator(DI_ABSTRACT_DECLARATOR_ALLOWED | DI_QUALIFIED_NAME_ALLOWED,
               &do_flags, *type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
	       (a_symbol_locator *)NULL,
               type_ptr, &bottom_derived_type, (a_func_info_block_ptr)NULL,
               &dim_expr_ptr);
  }  /* if */
  copy_source_position(start_pos, error_position);
  db_exit();
}  /* type_name */


void new_type_name(a_boolean         is_parenthesized,
                   a_type_ptr        *type_ptr,
                   an_expr_node_ptr  *dimension_expr)
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

  db_enter(3, "new_type_name");
  if (!is_parenthesized && curr_token == tok_lparen) {
    is_parenthesized = TRUE;
    (void)get_token();
  }  /* if */
  if (is_parenthesized) add_stop_token(tok_rparen);
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
  *dimension_expr = NULL;
  (void)decl_specifiers(DSI_TYPE_SPECIFIER_ALLOWED | DSI_IS_NEW_TYPE_NAME,
                        &dso_flags, &storage_class, type_ptr);
  if (C_dialect == C_dialect_cplusplus && dso_flags & DSO_DEFINES_SOMETHING) {
    /* Definition of a class, struct, union, or enum type is not allowed. */
    pos_error(ec_type_definition_not_allowed, &start_pos);
  } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
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
                 &bottom_derived_type, (a_func_info_block_ptr)NULL,
                 dimension_expr);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  } else {
    complete_type = pointer_declarator(*type_ptr, &bottom_derived_type);
    derived_type = NULL;
    bottom_derived_type = NULL;
    add_stop_token(tok_lbracket);
    if (curr_token == tok_lbracket) {
      nonconstant_array_declarator(&new_type_ptr, dimension_expr);
      add_to_derived_type_list(new_type_ptr,
                               &derived_type, &bottom_derived_type);
      while (curr_token == tok_lbracket) {
        array_declarator(&new_type_ptr);
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


a_boolean scan_conversion_operator(a_source_position  *id_pos)
/*
The token "operator" has been seen and passed; we are now on the token
immediately following it.  If it marks the start of a type name we have
an identifier for a conversion operator -- scan the type name, update the
locator, and return TRUE.  If it doesn't, return FALSE.
*/
{
  a_storage_class           storage_class;
  a_decl_flag_set           dso_flags;
  a_type_ptr                specifiers_type, complete_type;
  a_type_ptr                bottom_derived_type = NULL;
  a_source_position         type_pos;
  a_boolean                 is_conversion_operator;

  db_enter(3, "scan_conversion_operator");
  if (C_dialect == C_dialect_cplusplus && is_type_start()) {
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
    complete_type = pointer_declarator(specifiers_type, &bottom_derived_type);
    unget_token();
    curr_token = tok_identifier;
    make_type_conversion_locator(complete_type, &locator_for_curr_id, id_pos);
  } else {
    is_conversion_operator = FALSE;
  }  /* if */
  db_exit();
  return is_conversion_operator;
}  /* scan_conversion_operator */


a_type_ptr type_keyword(void)
/*
The current token is a type keyword (e.g., int, long); return the type
indicated by the keyword.  This is used in scanning a simple-type-name
for C++ functional-notation casts.  The current token is not advanced.
Note that this routine does not deal with identifiers that are defined
as types, only keywords; it also does not accept multi-token types,
e.g., "unsigned int".  See ARM 7.1.6 and 5.2.3.
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
#if CHECKING
    default:
      internal_error("type_keyword: unexpected token");
#endif /* CHECKING */
  }  /* switch */
  return type;
}  /* type_keyword */


static void reactivate_prototype_scope_symbols(
                                          a_symbol_ptr prototype_scope_symbols)
/*
Some symbols for types were created in a function prototype scope.
Reactivate those symbols (which were removed at the end of the function
prototype scope) now that we are in the body of the function.
*/
{
  a_symbol_ptr curr_symbol, next_symbol;

  /* Re-enter each symbol in the symbol table. */
  for (curr_symbol = prototype_scope_symbols;
       curr_symbol != NULL;
       curr_symbol = next_symbol) {
    next_symbol = curr_symbol->next_in_scope;
    reenter_symbol(curr_symbol, depth_scope_stack, /*suppress_error=*/TRUE);
    curr_symbol->reentered_from_prototype_scope = TRUE;
  }  /* for */
}  /* reactivate_prototype_scope_symbols */


static void function_definition(
                          a_symbol_locator   *locator,
                          a_type_ptr         rout_type,
                          a_boolean          top_declarator_type_is_function,
                          a_func_info_block  *func_info,
                          a_storage_class    storage_class,
                          a_boolean          inline_specified,
                          a_boolean          has_explicit_type_specifier,
                          a_boolean          is_main_function)
/*
Scan a function definition.  The declarator has already been scanned; the
old-style parameter declarations and the compound statement for the body
are still to come.  *locator is the locator to be used to enter the
function symbol; rout_type is the type for the function (which, in C++,
can be qualified -- hence the use of local variable unqualified_rout_type
where appropriate in this routine); top_declarator_type_is_function is
TRUE if the top type of rout_type is a function, and the function came
from a declarator rather than a typedef (the FALSE case is flagged as an
error by this routine); *func_info contains information about parameters;
storage_class is the storage class from the specifiers list; and
has_explicit_type_specifier is TRUE if the type of the function was
explicitly specified (rather than defaulted to "int").
*/
{
  a_symbol_ptr       symbol_ptr, ext_sym;
  a_routine_ptr      routine_ptr;
  a_scope_ptr        scope_ptr;
  a_param_id_ptr     param_id;
  a_param_type_ptr   old_style_param_types, end_old_style_param_types;
  an_id_linkage_kind linkage;
  a_type_ptr         return_type, old_type, unqualified_rout_type;
  a_routine_type_supplement_ptr
                     extra_info;
  a_memory_region_number
                     function_memory_region;
  a_boolean          prototyped;
  a_boolean          linked_redecl_error;
  a_param_type_ptr   comp_param_type_list;
  a_boolean          comp_prototyped;
  a_boolean	     is_member_function_def = FALSE;
  a_param_type_ptr   ptp;

  db_enter(3, "function_definition");
  /* The top type (function) must have come from a declarator, not from a
     typedef (see constraints section of 3.7.1, and associated footnote). */
  if (!top_declarator_type_is_function) {
    error(ec_function_type_must_come_from_declarator);
    /* Build a copy of the routine type that can be used below, to avoid
      further error recovery problems, and because we need a non-shared
       routine type entry that we can modify. */
    /* The type was probably from a typedef, so skip past that. */
    rout_type = skip_typerefs(rout_type);
#if CHECKING
    if (rout_type->kind != (a_type_kind)tk_routine) {
      internal_error("function_definition: rout_type is not a routine");
    }  /* if */
#endif /* CHECKING */
    unqualified_rout_type = alloc_type((a_type_kind)tk_routine);
    copy_type(rout_type, unqualified_rout_type);
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    if (ptp != NULL) {
      /* Copy the param type list so that the new unshared routine type will
         also have an unshared param type list. */
      a_param_type_ptr  new_ptp = alloc_param_type(ptp->type);
      *new_ptp = *ptp;
      /* Attach the first new param type entry to the routine type
         supplement. */
      unqualified_rout_type->
                   variant.routine.extra_info->param_type_list = new_ptp;
      /* If there are addition param type entries, add them to the end of the
         list (new_ptp will always be the current end-of-list entry). */
      ptp = ptp->next;
      for (; ptp != NULL; ptp = ptp->next, new_ptp = new_ptp->next) {
        new_ptp->next = alloc_param_type(ptp->type);
        *(new_ptp->next) = *ptp;
      }  /* for */
    }  /* if */
    rout_type = unqualified_rout_type;
  } else {
    unqualified_rout_type = make_unqualified_type(rout_type);
#if CHECKING
    if (unqualified_rout_type->kind != (a_type_kind)tk_routine) {
      internal_error(
           "function_definition: unqualified_rout_type is not a routine");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  extra_info = unqualified_rout_type->variant.routine.extra_info;
  prototyped = extra_info->prototyped;
  /* Force the storage class of a function with a body to unspecified
     (meaning external) or either static or inline (meaning internal);
     i.e., change extern to unspecified. */
  if (storage_class == (a_storage_class)sc_extern) {
    storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* 3.7.1, constraints: The return type of a function shall be void
     or an object type other than array.  See also the constraints of
     3.5.4.3 on function declarators, enforced previously by
     add_to_derived_type_list.  In C++ a reference type (including a
     reference to an array or function) may also be returned (ARM 8.2.5). */
  return_type = unqualified_rout_type->variant.routine.return_type;
  if (is_void_type(return_type) ||
      (is_object_type(return_type) && !is_array_type(return_type)) ||
      is_reference_type(return_type)) {
    /* Okay. */
  } else if (!is_error_type(return_type)) {
    /* Bad return type. */
    error(ec_bad_function_return_type);
  }  /* if */
  /* Create the symbol entry and routine entry for the routine. */
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->class_of_which_a_member != NULL) {
    /* This is the definition of a member function. */
#if CHECKING
    if (!prototyped) {
      internal_error("function_definition: member function not prototyped");
    }  /* if */
#endif /* if */
    is_member_function_def = TRUE;
    define_member_function(locator, rout_type, inline_specified,
                           &symbol_ptr, &linkage, &old_type, &ext_sym);
  } else {
    decl_var_or_routine(locator, storage_class, rout_type,
                        /*is_implicit_function=*/FALSE,
                        /*is_function_def_with_body=*/TRUE, inline_specified,
                        is_main_function, &symbol_ptr, &linkage,
                        &old_type, &ext_sym);
  }  /* if */
  symbol_ptr->defined = TRUE;
  routine_ptr = symbol_ptr->variant.routine;
#if CHECKING
  if (make_unqualified_type(routine_ptr->type) != unqualified_rout_type) {
    internal_error("function_definition: routine type not preserved");
  }  /* if */
#endif /* CHECKING */
  if (is_member_function_def) {
    /* Push a class symbol reactivation scope, to make class member names
       visible for processing the function definition. */
    push_class_reactivation_scope(symbol_ptr->class_of_which_a_member);
  }  /* if */  
  /* Push the name scope for the routine body. */
  scope_ptr = push_scope((a_scope_kind)sck_function, func_info->scope_number,
                         (a_type_ptr)NULL, routine_ptr);
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  routine_ptr->assoc_scope = function_memory_region = curr_il_region_number;
  extra_info->assoc_routine = routine_ptr;
  if (is_member_function_def) {
    a_type_ptr	rtp = skip_typerefs(old_type);
    scope_ptr->variant.routine.this_param_variable =
	make_param_variable(rtp->variant.routine.extra_info->
						   implicit_this_param_type,
                            (a_storage_class)sc_auto);
  } else if (storage_class == (a_storage_class)sc_unspecified &&
             routine_ptr->source_corresp.name != NULL &&
             strcmp(routine_ptr->source_corresp.name, "main") == 0) {
    /* This is "main", so remember the location of its routine entry. */
    il_header.main_routine = routine_ptr;
  }  /* if */
  /* If appropriate, set the return value pointer variable in the scope
     entry.  This is a pointer to an implicit parameter specifying the
     storage provided by the caller into which to copy a class object that
     is returned by value. */
  make_return_value_pointer_variable(rout_type, scope_ptr);
  if (top_declarator_type_is_function) {
    /* Parameter symbols that were created in the prototype scope (and then
       removed in pop_scope) have to be reentered in the function scope; they
       will be transformed in to variable symbols.  Also, in C mode, types
       that were defined in the prototype scope need to reactivated now so
       that they will be available in the current scope. */
    if (func_info->prototype_scope_symbols != NULL) {
      reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
    }  /* if */
  }  /* if */
  /* If a lint-style "argsused" or "varargs" comment appeared, remember that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments. */
  extra_info->lint_argsused_flag = lint_argsused_flag;
  extra_info->lint_varargs_count = lint_varargs_count;
  /* If the parameters are old-style, process a set of declarations.
     If they are new-style, declare the identifiers that appeared in
     the function prototype. */
  if (!prototyped) {
    /* Old-style id list. */
    old_style_param_types = end_old_style_param_types = NULL;
    /* Scan an optional list of declarations of parameters. */
    if (func_info->param_id_list == NULL) {
      /* No parameters to declare. */
    } else {
      /* When the id list was originally scanned, sk_parameter symbols were
         created but not actually entered into the symbol table, since there
         was no scope in which to enter them.  Now that the function scope
         has been created, enter the param names. */
      for (param_id = func_info->param_id_list;
           param_id != NULL;
           param_id = param_id->next) {
#if CHECKING
        if (param_id->symbol == NULL) {
          internal_error(
                      "function_definition: NULL old-style param_id symbol");
        }  /* if */
#endif /* CHECKING */
        reenter_symbol(param_id->symbol, decl_scope_level,
                       /*suppress_error=*/FALSE);
      }  /* for */
      in_old_style_param_decl_list = TRUE;
      /* Switch memory regions so that any types in the parameter declarations
         will be allocated in the memory region in which the function appears.
         This is necessary so that the parameter types are available for
         type-compatibility checking (see types_are_compatible).  Note that
         the variable entries for the parameters themselves are not allocated
         here, but rather a little later in this routine (q.v.), so that
         they will properly be in the sub-scope. */
      switch_il_region(FILE_SCOPE_REGION_NUMBER);
      while (is_decl_start() || curr_token == tok_identifier) {
        /* This declaration is checked to make sure the identifier is on the
           param_id_list. */
        declaration(/*function_definition_allowed=*/FALSE, 
                    /*extern_implied=*/FALSE, func_info->param_id_list);
      }  /* while */
      /* Switch back to the memory region for the current routine body. */
      switch_il_region(function_memory_region);
      in_old_style_param_decl_list = FALSE;
      /* Scan the list of identifiers, assign types to any that remain
         undeclared, and create the variable entries. */
      for (param_id = func_info->param_id_list;
           param_id != NULL;
           param_id = param_id->next) {
        if (param_id->type == NULL) {
          /* Enter any undeclared parameters with a type of int. */
          param_id->type = integer_type((an_integer_kind)ik_int);
          param_id->storage_class = (a_storage_class)sc_auto;
        }  /* if */
        /* The param_type entry must be allocated in the file-scope region. */
        ptp = alloc_param_type(param_id->type);
        /* Add the parameter variable to the list of parameters for this
           routine.  This is done in this way so that the parameters
           will be in the order they appear in the original identifier
           list rather than the order in which they appear in the
           declarations.  Note that the variable entry is allocated
           in the current (function) scope, not at the file scope. */
        decl_parameter(param_id);
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
    }  /* if */
    /* Save the composite type determined by decl_var_or_routine, if any.
       It's restored below if the old and new types are compatible and
       the old type is prototyped. */
    comp_param_type_list = extra_info->param_type_list;
    comp_prototyped      = extra_info->prototyped;
    /* Set the type to the new type information from the old-style
       parameters just scanned. */
    extra_info->param_type_list = old_style_param_types;
    extra_info->prototyped = FALSE;
    /* If there was a linked routine (a previous declaration), check
       type compatibility again now that the parameter types are known.
       No composite is formed here (see 3.1.2.6). */
    linked_redecl_error = FALSE;
    if (old_type != NULL) {
      if (types_are_compatible(rout_type, old_type)) {
        /* The type of the previous declaration is compatible with the type
           here.  If the old type was prototyped, restore the composite type
           determined by decl_var_or_routine, which is also prototyped.
           That composite type is still the right one, because old-style
           parameter information does not enter into a composite type. */
        if (comp_prototyped) {
          extra_info->param_type_list = comp_param_type_list;
          extra_info->prototyped      = comp_prototyped;
        }  /* if */
      } else {
        /* Type of previous declaration is incompatible with the type here.
           Note that there is a first test for compatibility in
           decl_var_or_routine.  If that one fails, old_type will be NULL,
           so we won't do this part of the test and won't give two errors. */
        pos_error(ec_not_compatible_with_previous_decl, 
                  &locator->source_position);
        linked_redecl_error = TRUE;
      }  /* if */
    }  /* if */
    /* Check that the type now that the parameters are known is compatible
       with the external symbol type.  There must be an external symbol because
       all routines have linkage. */
#if CHECKING
    if (ext_sym == NULL) {
      internal_error("function_definition: ext_sym is NULL");
    }  /* if */
#endif /* CHECKING */
    (void)reconcile_external_symbol_types(ext_sym, &locator->source_position,
                                          rout_type, linked_redecl_error);
  } else {
    /* New-style (function prototype). */
    a_param_type_ptr  ptp = extra_info->param_type_list;
    if (func_info->any_prototype_names_omitted) {
      /* At least one of the parameter names was omitted in the prototype.
         This is not valid when there is a function definition (except in C++:
         ARM 8.2.5, 8.3). */
      if (C_dialect != C_dialect_cplusplus) {
        error(ec_all_proto_params_must_be_named);
      }  /* if */
    }  /* if */
    param_id = func_info->param_id_list;
#if CHECKING
    if ((param_id == NULL) != (ptp == NULL)) {
      internal_error("function_definition: param_id and ptp out of sync");
    }  /* if */
#endif /* CHECKING */
    for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
      /* Declare each parameter identifier to have the associated type
         from the parameter type list. */
      param_id->type = ptp->type;
      decl_parameter(param_id);
#if CHECKING
      if ((param_id->next == NULL) != (ptp->next == NULL)) {
        internal_error("function_definition: param_id and ptp out of sync");
      }  /* if */
#endif /* CHECKING */
    }  /* while */
  }  /* if */
  /* Free the list of parameter ids, now that it is no longer needed. */
  free_param_id_list(&(func_info->param_id_list));
  /* Set the assoc_param_type field in each of the parameter variables. */
  fixup_parameters(scope_ptr->variant.routine.parameters,
                   extra_info->param_type_list);
  /* Enter the constructor initializers.  If the current token is a ":",
     explicit initialization for the constructor follows, but even without
     an explicit initializer, any implicit initializers should be recorded. */
  if (routine_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
    scope_ptr->variant.routine.constructor_inits =
                                      ctor_initializer(routine_ptr,
                                                       /*user_defined=*/TRUE);
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* Determine and remember the operator new() routine for the class. */
    set_class_assoc_operator_new_routine(
                          routine_ptr->source_corresp.class_of_which_a_member);
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  } else if (routine_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor) {
    scope_ptr->variant.routine.constructor_inits =
                                      dtor_initializer(routine_ptr);
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* Determine and remember the operator delete() routine for the class. */
    set_class_assoc_operator_delete_routine(
                          routine_ptr->source_corresp.class_of_which_a_member);
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
  }  /* if */
  /* Scan the compound statement defining the function.  The closing "}"
     is not swallowed by compound_statement, so that the pop_scope call
     can be done to get any errors out right on the "}". */
  scope_ptr->assoc_block = compound_statement(/*at_function_level=*/TRUE,
                                              has_explicit_type_specifier);
  /* Pop the function scope. */
  pop_scope();
  if (is_member_function_def) {
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
  a_type_ptr          rout_type, return_type;
  a_symbol_locator    locator;
  a_symbol_ptr        symbol_ptr;
  a_routine_type_supplement_ptr
                      extra_info;
  a_scope_ptr         scope;
  a_param_type_ptr    ptp;
  a_param_id_ptr      param_id;
  int                 saved_container_pos, saved_depth_stmt_stack;
  int                 saved_code_reachable;

  db_enter(3, "inline_function_definition");
  rout_type = skip_typerefs(rout_ptr->type);
  extra_info = rout_type->variant.routine.extra_info;
  /* Check whether the routine needs special support for returning a class
     object by value.   This flag is set in declarator (i.e., as soon as the
     routine type is seen) and usually that is sufficient.  However, with
     inlined friend functions a class that is referenced as a return type may
     not have been completely defined. */
  set_routine_calling_method_flag(rout_type);
  /* Similarly, check for value parameters that must be passed using a copy
     constructor. */
  for (ptp = extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
    set_arg_transfer_method_flag(ptp);
  }  /* for */
  /* 3.7.1, constraints: The return type of a function shall be void
     or an object type other than array.  See also the constraints of
     3.5.4.3 on function declarators, enforced previously by
     add_to_derived_type_list.  In addition, a reference type (including a
     reference to an array or function) may also be returned (ARM 8.2.5). */
  return_type = rout_type->variant.routine.return_type;
  if (is_void_type(return_type) ||
      (is_object_type(return_type) && !is_array_type(return_type)) ||
      is_reference_type(return_type)) {
    /* Okay. */
  } else {
    /* Bad return type. */
    if (!is_error_type(return_type)) error(ec_bad_function_return_type);
  }  /* if */
  symbol_ptr = (a_symbol_ptr)rout_ptr->source_corresp.assoc_info;
  make_locator_for_symbol(symbol_ptr, &locator);
  /* Push the name scope for the routine body. */
  scope = push_scope((a_scope_kind)sck_function, func_info->scope_number,
                     (a_type_ptr)NULL, rout_ptr);
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  rout_ptr->assoc_scope = curr_il_region_number;
  extra_info->assoc_routine = rout_ptr;
  rout_ptr->is_inline = TRUE;
  /* Parameter symbols that were created in the prototype scope (and then
     removed in pop_scope) have to be reentered in the function scope; they
     will be transformed in to variable symbols. */
  if (func_info->prototype_scope_symbols != NULL) {
    reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
  }  /* if */
  /* For a member function create the implicit "this" param variable and
     set a pointer to it in the scope entry. */
  if (extra_info->implicit_this_param_type != NULL) {
    /* Routine is a nonstatic member function. */
    scope->variant.routine.this_param_variable =
                make_param_variable(extra_info->implicit_this_param_type,
                                    (a_storage_class)sc_auto);
  }  /* if */
  /* If appropriate, set the return value pointer variable in the scope
     entry.  This is a pointer to an implicit parameter specifying the
     storage provided by the caller into which to copy a class object that
     is returned by value. */
  make_return_value_pointer_variable(rout_type, scope);
  /* If a lint-style "argsused" or "varargs" comment appeared, remember that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments. */
  extra_info->lint_argsused_flag = lint_argsused_flag;
  extra_info->lint_varargs_count = lint_varargs_count;

  param_id = func_info->param_id_list;
  ptp = extra_info->param_type_list;
#if CHECKING
  if ((param_id == NULL) != (ptp == NULL)) {
    internal_error("inline_function_definition: param_id and ptp out of sync");
  }  /* if */
#endif /* CHECKING */
  for (; param_id != NULL;
         param_id = param_id->next, ptp = ptp->next) {
    /* Declare each parameter identifier to have the associated type
       from the parameter type list. */
    param_id->type = ptp->type;
    decl_parameter(param_id);
#if CHECKING
    if ((param_id->next == NULL) != (ptp->next == NULL)) {
      internal_error(
                   "inline_function_definition: param_id and ptp out of sync");
    }  /* if */
#endif /* CHECKING */
  }  /* for */
  /* Free the list of parameter ids, now that it is no longer needed. */
  free_param_id_list(&(func_info->param_id_list));
  /* Set the assoc_param_type field in each of the parameter variables. */
  fixup_parameters(scope->variant.routine.parameters,
                   extra_info->param_type_list);
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
#if ASSIGNMENT_TO_THIS_ALLOWED
      /* Determine and remember the operator delete() routine for the class. */
      set_class_assoc_operator_delete_routine(
                             rout_ptr->source_corresp.class_of_which_a_member);
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
      break;
    default:;
      /* No action. */
  }  /* switch */
  /* Save structured statement stack state before calling compound_statement
     (so that it can be restored upon return) and create a new structured
     statement stack.  This is required for function definitions in classes
     defined within a function definition.  An indefinite nesting depth is
     supported */
  new_struct_stmt_stack(&saved_container_pos, &saved_depth_stmt_stack,
                        &saved_code_reachable);
  /* Scan the compound statement defining the function.  The closing "}"
     is not swallowed by compound_statement, so that the pop_scope call
     can be done to get any errors out right on the "}". */
  scope->assoc_block = compound_statement(/*at_function_level=*/TRUE,
                                          /*explicit_return_type=*/TRUE);
  /* Restore the original structured statement stack. */
  restore_struct_stmt_stack(saved_container_pos, saved_depth_stmt_stack,
                            saved_code_reachable);
  /* Pop the function scope. */
  pop_scope();
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
  return;
}  /* inline_function_definition */


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
      is_decl_start()) {
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


static void linkage_specification(a_boolean      function_definition_allowed,
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
                  param_id_list);
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
                param_id_list);
  }  /* if */
  /* Restore the default linkage to the value it had before the declaration
     (or declaration list) was processed. */
  def_external_linkage = saved_linkage;

  db_exit();
}  /* linkage_specification */


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
  a_boolean         is_parameter, local_is_parameter;
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
  an_expr_node_ptr  dim_expr_ptr;
  a_memory_region_number
                    region_to_switch_back_to;
#if ASM_FUNCTION_ALLOWED
  a_boolean         is_asm_function = FALSE;
#endif /* ASM_FUNCTION_ALLOWED */

  db_enter(3, "declaration");

  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, decl_start_pos);
  if (C_dialect == C_dialect_cplusplus &&
      curr_token == tok_extern && next_token() == tok_string_literal) {
    /* This looks like a C++ linkage specification, which is "extern"
       followed by a string literal (e.g., "C++" or "C"). */
    linkage_specification(function_definition_allowed, param_id_list);
    goto return_point;
  }  /* if */
  add_stop_token(tok_semicolon);
  need_semicolon_remove_stop_token = TRUE;
  is_parameter = (param_id_list != NULL);
#if ASM_FUNCTION_ALLOWED
  /* Check for "asm", which indicates the start of an asm function.
     "asm" is not a keyword.  It's recognized here as an identifier
     that is not defined as anything meaningful in the current context. */
  if (function_definition_allowed && is_asm_function_start()) {
    is_asm_function = TRUE;
    /* Skip over the "asm". */
    (void)get_token();
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  if (C_dialect == C_dialect_cplusplus) {
    /* Check for and discard declarations of the form "overload f;". */
    if (check_for_overload_anachronism()) goto return_point;
  }  /* if */
  /* Set the flags for calling decl_specifiers. */
  decl_start = is_decl_start();
  dsi_flags = DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
              DSI_TYPE_SPECIFIER_ALLOWED;
  if (is_parameter) {
    dsi_flags |= DSI_IS_PARAMETER;
  } else if (function_definition_allowed) {
    dsi_flags |= DSI_EMPTY_DECL_SPECIFIERS_ALLOWED;
    /* "inline" is allowed only on function declarations at file scope. */
    dsi_flags |= DSI_INLINE_ALLOWED;
  } else {
    /* A "vacuous declaration" of a class, struct, or union only makes sense
       when we are not at file scope. */
    dsi_flags |= DSI_VACUOUS_TAG_DECL_ALLOWED;
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
#if 0
        warning(ec_extra_semicolon);
#else
        if (strict_ansi_mode) {
          warning(ec_extra_semicolon);
        } else {
          remark(ec_extra_semicolon);
        }  /* if */
#endif /* if 0 */
      } else {
        if (curr_token == tok_lbrace) {
          /* Special error recovery on encountering an open brace: it
             may be the start of a routine. */
          error(ec_exp_declaration);
          flush_until_matching_token();
          if (curr_token == tok_rbrace) (void)get_token();
          if (is_decl_start()) goto continue_with_declaration;
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
    } else if (is_parameter && C_dialect == C_dialect_ANSI) {
      /* ANSI C does not allow freestanding declarations (as of structs)
         within an old-style parameter list.  pcc, on the other hand,
         will allow something like
            int f(a)
            struct s {int b;};
            struct s a;
            { ... }
      */
      error(ec_decl_should_be_of_param);
    } else if (!declares_something && C_dialect == C_dialect_cplusplus &&
               defines_something && type_ptr->kind == (a_type_kind)tk_union &&
               storage_class != (a_storage_class)sc_typedef) {
      /* Special C++ case:  the declaration of an anonymous union.   Do the
         required error checking and special processing, including creation
         of a variable which will represent the anonymous union and with
         which its fields will be aliased. */
#if CHECKING
      if (!is_unnamed_class_symbol(
                       (a_symbol_ptr)(type_ptr->source_corresp.assoc_info))) {
        internal_error("declaration: nameless symbol expected");	
      }  /* if */
#endif /* CHECKING */
      make_anonymous_union_variable(type_ptr, storage_class);
      /* The anonymous union variable is marked as referenced, as are all
         unnamed entities.  So its type is also marked referenced. */
      type_ptr->source_corresp.referenced = TRUE;
    } else {
      if (storage_class == (a_storage_class)sc_typedef) {
        /* A case like "typedef int;" or "typedef struct { int i; };" */
        set_err_pos_to_curr_token();
        warning(ec_missing_typedef_name);
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
          if (C_dialect == C_dialect_cplusplus) {
            error(ec_missing_object_name);
          } else {
            warning(ec_missing_object_name);
          }  /* if */
        } else {
          /* The specifiers should have declared something or this declaration
             is pointless.  An example would be
                int ;
             An example of a useful declaration with a null declarator is
                struct x {int a;};
             since it declares something (namely x). */
          /* ANSI probably thinks of this as an error, but that seems a bit
             extreme, so we make it a warning.  pcc allows this, so the most
             we can issue in that case is a warning. */
          warning(ec_useless_decl);
        }  /* if */
      } else {
        /* Since declares_something is TRUE, this must be a class, struct,
           union, or enum declaration.  A storage class or qualifier is not
           allowed, nor is "inline". */
        if (storage_class != (a_storage_class)sc_unspecified) {
          if (C_dialect == C_dialect_cplusplus) {
            error(ec_storage_class_not_allowed);
          } else {
            warning(ec_storage_class_not_allowed);
          }  /* if */
        }  /* if */
        if (is_qualified_type(type_ptr)) {
          if (C_dialect == C_dialect_cplusplus) {
            error(ec_const_volatile_not_allowed);
          } else {
            warning(ec_const_volatile_not_allowed);
          }  /* if */
        }  /* if */
        if (inline_specified) {
          error(ec_inline_and_nonfunction);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (dangling_type_specifier && (curr_token != tok_identifier ||
             ((next_tok = next_token()) != tok_semicolon &&
              next_tok != tok_comma))) {
    /* A class, struct, union, or enum declaration was followed by a
       a type specifier keyword or else by an identifier that is a type name
       and that is not followed by a comma or semicolon.  In other words,
       issue a missing-semicolon error on the following:
           class A;
           class B {...} A ...
       where A is probably the start of a new declaration.  However, don't
       put out that error in this case:
           class A;
           class B {...} A;
       where the semicolon following A in the second line makes it clear that
       A was intended to be a declarator. */
    set_err_pos_to_curr_token();
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
      if (is_scalar_type(type_ptr) ||
          is_class_struct_union_type(type_ptr)) {
        di_flags |= DI_PARENTHESIZED_INITIALIZER_ALLOWED;
      }  /* if */
      if (storage_class != (a_storage_class)sc_typedef &&
          decl_scope_level == DEPTH_OF_FILE_SCOPE) {
        di_flags |= DI_QUALIFIED_NAME_ALLOWED;
      }  /* if */
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
                 &local_type_ptr, &bottom_derived_type, &func_info,
                 &dim_expr_ptr);
      is_function = is_function_type(local_type_ptr);
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
            is_main_function = TRUE;
            /* Perform some error checking that is specific to C++. */
            if (def_external_linkage.is_explicit) {
              pos_warning(ec_linkage_specifier_not_allowed, &declarator_pos);
            }  /* if */
            /* "inline" and "static" are not allowed (ARM 3.4). */
            if (storage_class == (a_storage_class)sc_static) {
              pos_error(ec_static_main, &declarator_pos);
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
            is_main_function = TRUE;
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
      if (is_function && C_dialect == C_dialect_cplusplus) {
        /* The ARM (8.2.5) explicitly prohibits defining a type in a
           function return type. */
        if (defines_something) {
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &decl_start_pos);
        }  /* if */
      }  /* if */
      local_storage_class = storage_class;
      local_is_parameter = is_parameter;
      /* If this is a parameter (old-style), make sure it appears on
         the param_id_list.  Also adjust the type if necessary
         (for example, "array of x" becomes "pointer to x"). */
      if (local_is_parameter) {
        param_id = param_id_on_list(&locator, param_id_list);
        if (param_id == NULL) {
          /* The identifier was not found on the list. */
          error(ec_decl_should_be_of_param);
          /* Enter the declared object as a variable rather than as a
             parameter.  */
          local_is_parameter = FALSE;
        } else if (param_id->type != NULL) {
          /* Parameter has already been declared. */
          error(ec_id_already_declared);
        }  /* if */
        adjust_parameter_type(&local_type_ptr);
        is_function = top_declarator_type_is_function = FALSE;
        /* For pcc compatibility, promote float parameters to double. */
        if (C_dialect == C_dialect_pcc) {
          promote_float_to_double(local_type_ptr);
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
        }  /* if */
      }  /* if */
      /* If the thing declared is a function, and if the token following looks
         like it could be part of a function-definition, go scan that. */
      if (function_definition_allowed && is_function &&
          local_storage_class != (a_storage_class)sc_typedef &&
          curr_token != tok_semicolon && curr_token != tok_comma &&
          curr_token != tok_assign && curr_token != tok_end_of_source) {
        if (!has_explicit_type_specifier && !is_main_function) {
          /* Function with no explicitly specified return type. */
          if (C_dialect == C_dialect_cplusplus) {
            /* Issue a warning in C++, unless this is a constructor or
               destructor definition. */
            if (!is_constructor_or_destructor && !locator.is_conversion_name) {
              pos_warning(ec_missing_type_specifier, &declarator_pos);
            }  /* if */
          } else if (C_dialect != C_dialect_pcc) {
            /* Ordinary C -- issue a remark. */
            pos_remark(ec_missing_type_specifier, &declarator_pos);
          }  /* if */
        }  /* if */
        remove_all_local_stop_tokens();
        function_definition(&locator, local_type_ptr, 
                            top_declarator_type_is_function, &func_info,
                            local_storage_class, inline_specified,
                            has_explicit_type_specifier, is_main_function);
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
          pos_error(ec_member_function_redeclaration, &declarator_pos);
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
            (C_dialect != C_dialect_cplusplus ||
             (!is_static_data_member && !is_function))) {
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
            set_to_error_locator(locator);
          }  /* if */
        } else if (!has_explicit_type_specifier) {
          if (is_function) {
            /* For implicitly typed function declarations, issue a warning in
               C++, a remark in ordinary C, and no diagnostic in pcc mode. */
            if (C_dialect == C_dialect_cplusplus) {
              /* Issue a warning in C++, unless this is a constructor or
                 destructor definition. */
              if (!is_constructor_or_destructor) {
                pos_warning(ec_missing_type_specifier, &declarator_pos);
              }  /* if */
            } else if (C_dialect != C_dialect_pcc) {
              /* Ordinary C -- issue a remark. */
              pos_remark(ec_missing_type_specifier, &declarator_pos);
            }  /* if */
          } else {
            /* For implicitly typed nonfunction declarations (variables,
               typedefs, etc.) issue a warning in all modes. */
            pos_warning(ec_missing_type_specifier, &declarator_pos);
          }  /* if */
        }  /* if */
      }  /* if */
      if (top_declarator_type_is_function) {
        /* If the function has a non-empty old-style identifier list of
           parameters, a body should have been present. */
        if (!local_type_ptr->variant.routine.extra_info->prototyped &&
            func_info.param_id_list != NULL) {
          error(ec_param_id_list_needs_function_def);
          /* Free the list of parameter identifiers. */
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
            /* In pcc mode, allow "static"; the function will be entered
               at the file scope as static. */
            if (C_dialect == C_dialect_pcc &&
                local_storage_class == (a_storage_class)sc_static) {
              /* Okay. */
            } else {
              error(ec_block_scope_function_must_be_extern);
              local_storage_class = (a_storage_class)sc_extern;
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
        if (!is_function && C_dialect == C_dialect_cplusplus) {
          /* Abstract class objects are prohibited (ARM 10.3). */
          if (is_illegal_abstract_class_type(local_type_ptr)) {
            error(ec_abstract_class_object_not_allowed);
          }  /* if */
        }  /* if */
      }  /* if */
      /* Enter the symbol with the proper type. */
      linkage = idl_none;
      if (local_is_parameter) {
        symbol_ptr = param_id->symbol;
        copy_source_position(locator.source_position,
                             symbol_ptr->decl_position);
        param_id->type = local_type_ptr;
        param_id->storage_class = local_storage_class;
      } else if (local_storage_class == (a_storage_class)sc_typedef) {
        decl_typedef(&locator, local_type_ptr, &symbol_ptr);
      } else if (is_static_data_member) {
        define_static_data_member(&locator, local_storage_class,
				  local_type_ptr, &symbol_ptr, &linkage);
      } else {
        if (is_parameter) {
          /* We are in an old-style param declaration but a name was found
             that was not on the param id list.  Switch (back) to the function
             scope memory region so that the variable will be treated like
             an ordinary automatic variable. */
          switch_to_function_scope_region(&region_to_switch_back_to);
        }  /* if */
        decl_var_or_routine(&locator, local_storage_class, local_type_ptr,
                            /*is_implicit_function=*/FALSE,
                            /*is_function_def_with_body=*/FALSE,
                            inline_specified, is_main_function, &symbol_ptr,
                            &linkage, &old_type, &ext_sym);
        if (is_parameter) {
          switch_back_to_original_region(region_to_switch_back_to);
        }  /* if */
        /* Fetch the storage class again, which might have been changed if
           this is a file scope redeclaration of an extern const variable. */
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable) {
          local_storage_class = symbol_ptr->variant.variable->storage_class;
        }  /* if */
      }  /* if */
      /* Look for optional initializer. */
      remove_stop_token(tok_assign);
      need_assign_remove_stop_token = FALSE;
      has_initializer = FALSE;
      set_err_pos_to_curr_token();
      if (has_parenthesized_initializer) {
        has_initializer = TRUE;
      } else if (curr_token == tok_assign) {
        (void)get_token();
        has_initializer = TRUE;
      } else if (C_dialect == C_dialect_pcc && is_initializer_start()) {
        /* In pcc mode, the "=" may be omitted (K&R first edition, Appendix A,
           section 17 (Anachronisms)). */
        has_initializer = TRUE;
        warning(ec_old_fashioned_initializer);
      }  /* if */
      if (has_initializer) {
        /* Initializer is present.  Scan it. */
        /* If the symbol is a parameter, the subroutine will generate the
           error.  This is done rather than flagging the error here because
           the subroutine can scan over the initializer expression neatly. */
        initializer(symbol_ptr, &locator.source_position, linkage,
                    has_parenthesized_initializer, is_parameter);
        if (symbol_ptr->kind == (a_symbol_kind)sk_variable && !is_parameter) {
          /* Fetch the type of the symbol again, since it might have been
             changed if it was an incomplete array and was initialized. */
          local_type_ptr = symbol_ptr->variant.variable->type;
          /* Set the storage class of a file-scope initialized variable to
             unspecified (meaning external) or static (meaning internal).
             See 3.7.2. */
          if (decl_scope_level == DEPTH_OF_FILE_SCOPE &&
              local_storage_class != (a_storage_class)sc_static) {
            symbol_ptr->variant.variable->storage_class =
                                              (a_storage_class)sc_unspecified;
          }  /* if */
          /* All initialized variables are considered defined.  This flag
             may have already been set based on storage class and scope
             level. */
          symbol_ptr->defined = TRUE;
        }  /* if */
      } else if ((symbol_ptr->kind == (a_symbol_kind)sk_variable ||
                  symbol_ptr->kind == (a_symbol_kind)sk_static_data_member) &&
                 !is_parameter && !is_error_locator(locator)) {
        a_variable_ptr  vp = symbol_ptr->variant.variable;
        if (vp->init_kind != (an_init_kind)initk_none) {
          /* Already initialized -- this must be a redeclaration. */
        } else {
          /* Determine whether a default initializer should be generated for
             this symbol, and if so do it.  The function returns TRUE if
             default initialization was performed (or if it was attempted but
             an error was reported). */
          if (!def_initializer(symbol_ptr, &locator.source_position) &&
              symbol_ptr->kind == (a_symbol_kind)sk_variable) {
            /* No default initialization, so do some additional checking. */
            if (is_reference_type(local_type_ptr) &&
                vp->storage_class != (a_storage_class)sc_extern) {
              /* Non-extern reference variables must be initialized
                 (ARM 8.4.3). */
              error(ec_missing_initializer_on_reference);
            } else if (
                     type_or_element_type_is_const_qualified(local_type_ptr)) {
              /* Uninitialized const variable.  In C++ this is permitted only
                 for externally linked variables.  In ordinary C we issue a
                 warning for local variables (both static and automatic) here,
                 but the warning for static file scope variables is given
                 later. */
              a_name_linkage_kind  name_linkage =
                                           (a_name_linkage_kind)vp->
                                                 source_corresp.name_linkage;
              if (C_dialect == C_dialect_cplusplus) {
                if (name_linkage == (a_name_linkage_kind)nlk_none ||
                    (name_linkage == (a_name_linkage_kind)nlk_internal &&
                     decl_scope_level == DEPTH_OF_FILE_SCOPE)) {
                  /* In C++ const qualified variables that are internally
                     linked must be initialized (ARM 7.1.6). */
                  error(ec_missing_initializer_on_const);
                }  /* if */
              } else {
                /* Ordinary C -- a warning, and only on local variables. */
                if (name_linkage == (a_name_linkage_kind)nlk_none) {
                  warning(ec_missing_initializer_on_const);
                }  /* if */
              }  /* if */
            } else if (vp->storage_class != (a_storage_class)sc_extern) {
              /* Check for an uninitialed variable that has members that
                 ought to be initialized.  Issue a warning in such cases. */
              a_type_ptr  tp = local_type_ptr;
              if (is_array_type(tp)) tp = underlying_array_element_type(tp);
              if (is_class_struct_union_type(tp)) {
                /* The variable is a class-struct-union type or an array
                   whose element type is a class-struct-union type.  Issue
                   a warning if there is a const qualified field or a field
                   of reference type.  Note that this check is not explicitly
                   mandated by the ARM (though it is implied in 12.6.2:  "The
                   argument list . . . is the only way to initialize nonstatic
                   const and reference members").  Cfront issues an error on
                   class declarations that contain nonstatic const or reference
                   members and no constructor, but this seems to introduce an
                   unnecessary incompatibility with C. */
                tp = skip_typerefs(tp);
                if (tp->variant.class_struct_union.any_const_member) {
                  pos_warning(ec_uninitialized_const_member,
                              &declarator_pos);
                }  /* if */
                if (C_dialect == C_dialect_cplusplus &&
                    symbol_supplement_for_class(tp)->any_ref_member) {
                  pos_warning(ec_uninitialized_ref_member, &declarator_pos);
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      copy_source_position(locator.source_position, error_position);
      /* If a variable has no linkage, the type must be complete here.
         A case like "void i;" at file scope is also an error, since it
         can never be completed. */
      if (symbol_ptr->kind == (a_symbol_kind)sk_variable &&
          !is_parameter && is_incomplete_type(local_type_ptr) &&
          (linkage == idl_none ||
           (local_storage_class == (a_storage_class)sc_unspecified &&
            is_void_type(local_type_ptr)))) {
        error(ec_incomplete_type_not_allowed);
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
              /*extern_implied=*/FALSE, (a_param_id_ptr)NULL);
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
    if (strict_ansi_mode) warning(ec_empty_translation_unit);
  } else {
    do {
      declaration(/*function_definition_allowed=*/TRUE,
                  /*extern_implied=*/FALSE, (a_param_id_ptr)NULL);
    } while (curr_token != tok_end_of_source);
  }  /* if */
}  /* translation_unit */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
