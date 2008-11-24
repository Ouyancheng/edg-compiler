/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lexical.h -- Declarations relating to lexical.c (having to do with source
             input and lexical scanning).

*/

/* Avoid including these declarations more than once: */
#ifndef LEXICAL_H
#define LEXICAL_H 1

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct an_orig_line_modif  *an_orig_line_modif_ptr;
typedef struct a_source_line_modif *a_source_line_modif_ptr;

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */

/* There are more #includes later in this file. */

/*
Token kinds.  See 3.1, 3.1.1, 3.1.5, 3.1.6 in standard.
If this enumeration is changed, be sure to change token_names and
opname_kind_for_token below.
*/
typedef enum /*a_token_kind*/ {
  /* Complex tokens: */
  tok_identifier,
  tok_float_constant,
  tok_fixed_point_constant,
  tok_int_constant,
  tok_char_constant,
  tok_string_literal,
  tok_end_of_source,
  tok_newline,
  tok_header_name,
  tok_pp_number,
  tok_digit_sequence,
  tok_cpp_quote,
  tok_ptr_to_member,	/* C++ only */
  /* Operators (standard, 3.1.5; sizeof appears with keywords): */
  tok_lbracket              /* [ */,
  tok_rbracket              /* ] */,
  tok_lparen                /* ( */,
  tok_rparen                /* ) */,
  tok_period                /* . */,
  tok_arrow                 /* -> */,
  tok_plus_plus             /* ++ */,
  tok_minus_minus           /* -- */,
  tok_ampersand             /* & */,
  tok_star                  /* * */,
  tok_plus                  /* + */,
  tok_minus                 /* - */,
  tok_compl                 /* ~ */,
  tok_not                   /* ! */,
  tok_divide                /* / */,
  tok_remainder             /* % */,
  tok_shift_left            /* << */,
  tok_shift_right           /* >> */,
  tok_lt                    /* < */,
  tok_gt                    /* > */,
  tok_le                    /* <= */,
  tok_ge                    /* >= */,
  tok_eq                    /* == */,
  tok_ne                    /* != */,
  tok_excl_or               /* ^ */,
  tok_or                    /* | */,
  tok_and_and               /* && */,
  tok_or_or                 /* || */,
  tok_quest_mark            /* ? */,
  tok_colon                 /* : */,
  tok_assign                /* = */,
  tok_times_assign          /* *= */,
  tok_divide_assign         /* /= */,
  tok_remainder_assign      /* %= */,
  tok_plus_assign           /* += */,
  tok_minus_assign          /* -= */,
  tok_shift_left_assign     /* <<= */,
  tok_shift_right_assign    /* >>= */,
  tok_and_assign            /* &= */,
  tok_excl_or_assign        /* ^= */,
  tok_or_assign             /* |= */,
  tok_comma                 /* , */,
  tok_sharp                 /* # */,
  tok_paste                 /* ## */,
  /* The min and max operators are only recognized in GNU C++ mode. */
  tok_gnu_min               /* <? */,
  tok_gnu_max               /* >? */,
  /* Punctuators (standard, 3.1.6) that are not also operators: */
  tok_lbrace                /* { */,
  tok_rbrace                /* } */,
  tok_semicolon             /* ; */,
  tok_ellipsis              /* ... */,
  /* Keywords (standard, 3.1.1): */
  tok_auto,
  tok_break,
  tok_case,
  tok_char,
  tok_const,
  tok_continue,
  tok_default,
  tok_do,
  tok_double,
  tok_else,
  tok_enum,
  tok_extern,
  tok_float,
  tok_for,
  tok_goto,
  tok_if,
  tok_int,
  tok_long,
  tok_register,
  tok_return,
  tok_short,
  tok_signed,
  tok_sizeof,
  tok_static,
  tok_struct,
  tok_switch,
  tok_typedef,
  tok_union,
  tok_unsigned,
  tok_void,
  tok_volatile,
  tok_while,
  /* Specific to C99 mode. */
  tok_generic,
  tok_genericfx,
  /* Extensions (__ALIGNOF__ is similar to sizeof; __INTADDR__ is used
     to scan an integer address expression for offsetof): */
  tok_alignof,
  tok_intaddr,
  /* Used when <stdarg.h> is treated as a builtin. */
  tok_va_start, tok_va_arg, tok_va_end, tok_va_copy,
  tok_builtin_offsetof,
  tok_restrict,
  tok_gnu_restrict,
  /* C99 types: _Bool, _Complex and _Imaginary. */
  tok_c99_bool,
  tok_c99_complex,
  tok_c99_imaginary,
  /* Token for __I__, for the C99 imaginary number "i" (i*i == -1). */
  tok_imaginary_unit,
  /* Token for __NAN__, for a Not-a-Number constant (C99 and other modes). */
  tok_nan,
  /* Token for __INFINITY__, for an Infinity constant (C99 and other modes). */
  tok_infinity,
  /* Tokens for fixed-point type support ("_Fract", "_Accum", and "_Sat"). */
  tok_fract,
  tok_accum,
  tok_sat,
#if MICROSOFT_EXTENSIONS_ALLOWED
  tok_abstract,
  tok_override,
  tok_sealed,
  tok_cdecl,
  tok_declspec,
  tok_fastcall,
  tok_stdcall,
  tok_thiscall,
  tok_microsoft_inline,
  tok_forceinline,
  tok_unaligned,
  tok_microsoft_try,
  tok_finally,
  tok_leave,
  tok_except,
  tok_int8,
  tok_int16,
  tok_int32,
  tok_int64,
  tok_based,
  tok_uuidof,
  tok_assume,
  tok_charize,
  tok_if_exists,
  tok_if_not_exists,
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
  tok_end_of_if_exists,		/* Generated token used by front end. */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
  tok_super,
  tok_noop,
  tok_interface,
  tok_microsoft_ptr32,
  tok_microsoft_ptr64,
  tok_microsoft_sptr,
  tok_microsoft_uptr,
  tok_microsoft_w64,
  tok_microsoft_lprefix,
  tok_microsoft_identifier,
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  tok_microsoft_asm,
  /* Special constants for various versions of the name of the current
     function, e.g., __func__ from C99, __FUNCTION__ from GNU and Microsoft. */
  tok_func_name,		/* __func__ */
  tok_function_name,		/* __FUNCTION__ */
  tok_pretty_function_name,	/* __PRETTY_FUNCTION__ */
  tok_decorated_function_name,	/* Microsoft __FUNCDNAME__ */
#if NEAR_AND_FAR_ALLOWED
  tok_near,
  tok_far,
#endif /* NEAR_AND_FAR_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  tok_attribute,
  tok_va_start_single_operand,
  tok_builtin_types_compatible,
  tok_gnu_real,
  tok_gnu_imag,
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* C++ tokens not in C (ARM, 2.4): */
  tok_colon_colon       /* :: */,
  tok_period_star       /* .* */,
  tok_arrow_star        /* ->* */,
  tok_asm,
  tok_catch,
  tok_class,
  tok_delete,
  tok_friend,
  tok_inline,
  tok_new,
  tok_operator,
  tok_private,
  tok_protected,
  tok_public,
  tok_template,
  tok_this,
  tok_throw,
  tok_try,
  tok_virtual,
  /* C++ tokens not in the ARM: */
  tok_wchar_t,
  tok_const_cast,
  tok_dynamic_cast,
  tok_explicit,
  tok_export,
  tok_mutable,
  tok_namespace,
  tok_reinterpret_cast,
  tok_static_cast,
  tok_typeid,
  tok_using,
  tok_bool,
  tok_false,
  tok_true,
  tok_typename,
  tok_static_assert,
  tok_decltype,
  /* Recognized in GNU C and C++ modes only. */
  tok_typeof,
  tok_extension,
  tok_null,
  /* Recognized in cfront compatibility mode only. */
  tok_overload,
#if SUN_EXTENSIONS_ALLOWED
  /* Recognized in Sun C++ mode only. */
  tok_global_link_scope,
  tok_symbolic_link_scope,
  tok_hidden_link_scope,
#endif /* SUN_EXTENSIONS_ALLOWED */
  tok_thread,
#if UPC_EXTENSIONS_ALLOWED
  /* Recognized in UPC mode only. */
  tok_upc_strict,
  tok_upc_relaxed,
  tok_upc_shared,
  tok_upc_forall,
  tok_upc_barrier,
  tok_upc_notify,
  tok_upc_wait,
  tok_upc_fence,
  tok_upc_threads,
  tok_upc_mythread,
  tok_upc_blocksizeof,
  tok_upc_localsizeof,
  tok_upc_elemsizeof,
#endif /* UPC_EXTENSIONS_ALLOWED */
  tok_has_assign,
  tok_has_copy,
  tok_has_nothrow_assign,
  tok_has_nothrow_constructor,
  tok_has_nothrow_copy,
  tok_has_trivial_assign,
  tok_has_trivial_constructor,
  tok_has_trivial_copy,
  tok_has_trivial_destructor,
  tok_has_user_destructor,
  tok_has_virtual_destructor,
  tok_is_abstract,
  tok_is_base_of,
  tok_is_class,
  tok_is_convertible_to,
  tok_is_empty,
  tok_is_enum,
  tok_is_pod,
  tok_is_polymorphic,
  tok_is_union,
  /* Token used to indicate keywords that are not yet implemented. */
  tok_unimplemented,
  /* Error token. */
  tok_error,
  /* Placeholder for a removed default argument. */
  tok_removed_default_arg,
  /* Place-holder for last position in enumeration. */
  tok_last
} a_token_kind;

/*
Define the type to be used as a more compact representation of a_token_kind.
*/
#ifndef TYPE_FOR_A_SMALL_TOKEN_KIND
#define TYPE_FOR_A_SMALL_TOKEN_KIND a_byte
#endif /* ifndef TYPE_FOR_A_SMALL_TOKEN_KIND */

typedef TYPE_FOR_A_SMALL_TOKEN_KIND a_small_token_kind;

/*
Table of names corresponding to token kinds.
*/
EXTERN char	*token_names[(int)tok_last+1]
#if VAR_INITIALIZERS
= {"identifier", "float constant", "fixed-point constant", "int constant",
   "char constant", "string literal", "end of source", "newline",
   "header name", "pp number", "digit sequence", "cpp quote", "ptr to member", 
   "[", "]", "(", ")", ".", "->", "++", "--", "&", "*", "+", "-",
   "~", "!", "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==",
   "!=", "^", "|", "&&", "||", "?", ":", "=", "*=", "/=", "%=",
   "+=", "-=", "<<=", ">>=", "&=", "^=", "|=", ",", "#", "##", "<?", ">?",
   "{", "}", ";", "...", "auto", "break", "case", "char", "const",
   "continue", "default", "do", "double", "else", "enum", "extern",
   "float", "for", "goto", "if", "int", "long", "register",
   "return", "short", "signed", "sizeof", "static", "struct",
   "switch", "typedef", "union", "unsigned", "void", "volatile",
   "while", "__generic", "__genericfx", "__ALIGNOF__", "__INTADDR__",
   "va_start", "va_arg", "va_end", "va_copy",
   "__builtin_offsetof",
   "restrict", "__restrict",
   "_Bool", "_Complex", "_Imaginary", "__I__", "__NAN__", "__INFINITY__",
   "_Fract", "_Accum", "_Sat",
#if MICROSOFT_EXTENSIONS_ALLOWED
   "abstract", "override", "sealed",
   "__cdecl", "__declspec", "__fastcall", "__stdcall", "__thiscall",
   "__inline", "__forceinline",
   "__unaligned", "__try", "__finally", "__leave", "__except",
   "__int8", "__int16", "__int32", "__int64", "__based",
   "__uuidof", "__assume", "#@", "__if_exists", "__if_not_exists",
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
   "end of __if_exists",
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
   "__super",
   "__noop", "__interface",
   "__ptr32", "__ptr64", "__sptr", "__uptr", "__w64",
   "__LPREFIX", "__identifier",
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   "__asm",
   "__func__",
   "__FUNCTION__",
   "__PRETTY_FUNCTION__",
   "__FUNCDNAME__",
#if NEAR_AND_FAR_ALLOWED
    "__near", "__far",
#endif /* NEAR_AND_FAR_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
   "__attribute__", "__builtin_varargs_start", "__builtin_types_compatible_p",
   "__real", "__imag",
#endif /* GNU_EXTENSIONS_ALLOWED */
   "::", ".*", "->*", "asm", "catch", "class", "delete", "friend",
   "inline", "new", "operator", "private", "protected", "public",
   "template", "this", "throw", "try", "virtual", "wchar_t",
   "const_cast", "dynamic_cast", "explicit", "export", "mutable", "namespace",
   "reinterpret_cast", "static_cast", "typeid", "using",
   "bool", "false", "true", "typename", "static_assert", "decltype",
   "__typeof__", "__extension__", "__null",
   "overload",
#if SUN_EXTENSIONS_ALLOWED
   "__global", "__symbolic", "__hidden",
#endif /* SUN_EXTENSIONS_ALLOWED */
   "__thread",
#if UPC_EXTENSIONS_ALLOWED
   "strict", "relaxed", "shared", "upc_forall", "upc_barrier", "upc_notify",
   "upc_wait", "upc_fence", "THREADS", "MYTHREAD", "upc_blocksizeof",
   "upc_localsizeof", "upc_elemsizeof",
#endif /* UPC_EXTENSIONS_ALLOWED */
   "__has_assign",
   "__has_copy",
   "__has_nothrow_assign",
   "__has_nothrow_constructor",
   "__has_nothrow_copy",
   "__has_trivial_assign",
   "__has_trivial_constructor",
   "__has_trivial_copy",
   "__has_trivial_destructor",
   "__has_user_destructor",
   "__has_virtual_destructor",
   "__is_abstract",
   "__is_base_of",
   "__is_class",
   "__is_convertible_to",
   "__is_empty",
   "__is_enum",
   "__is_pod",
   "__is_polymorphic",
   "__is_union",
   "unimplemented", "error", "removed default arg",
   "last" /* used to check that initialization is right. */
  }
#endif /* VAR_INITIALIZERS */
;

/*
A sequence number is assigned to each token fetched from the input.
This is the type used to represent the sequence number.
*/
typedef unsigned long a_token_sequence_number;

EXTERN a_token_sequence_number
		curr_token_sequence_number;
			/* The sequence number associated with the
			   current token.  A token retains its sequence
			   number even when saved and restored from a
			   token cache. */

EXTERN a_token_sequence_number
		last_token_sequence_number_used;
			/* The counter used to assign token sequence
			   numbers. */

#define NO_TOKEN_SEQUENCE_NUMBER (0)
			/* The value used to indicate that no sequence number
			   is present. */

#define MAX_TOKEN_SEQUENCE_NUMBER (~(a_token_sequence_number)(0))
			/* The largest possible token sequence number. */


/* These includes are placed here so that a_token_kind will be defined
   for general use before including these files. */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


/* These declarations are placed here so that they will be defined before
   symbol_tbl.h is included. */


/* Flags used to specify how identifiers are to be scanned by the
   generalized identifier routines.   is_generalized_identifier_start
   recognizes only the GID_TEMPLATE_ARGS_OPTIONAL and
   GID_DTOR_RECOGNIZED flags. */
typedef int an_identifier_options_set;
#define GID_NO_OPTIONS                0x00
#define GID_TEMPLATE_ARGS_OPTIONAL    0x01
			/* Normally if a class template name is seen it must
			   be followed by a template argument list, otherwise
			   an error is issued.  This flag allows the
			   template argument list to be omitted. */
#define GID_DTOR_RECOGNIZED           0x02
			/* Enables recognition of destructor names
			   ("~" followed by an identifier).  The default
			   is to not recognize "~" as the start of an
			   identifier (no error is issued).  Destructor
			   names are always recognized following qualifiers. */
#define GID_DISALLOW_QUALIFIED_NAME   0x04
			/* Causes an error to be issued if the identifier
			   is a qualified name (either A::B or ::i). */
#define GID_DISALLOW_GLOBAL_QUALIFIER 0x08
			/* Causes an error to be issued if the identifier
			   is a global qualifier (::A::B or ::i). */
#define GID_DISALLOW_OPERATOR_NAME    0x10
			/* Causes an error to be issued if the identifier
			   is an operator name of the form "operator =" or
			   "operator int". */
#define GID_VACUOUS_DTOR_RECOGNIZED   0x20
			/* Enables recognition of destructor calls, as part
			   of a qualified name, for non-class types and class
			   types that have no destructors (e.g., A::~A or
			   int::~int). */
#define GID_DTOR_MUST_BE_NONCLASS     0x40
			/* Enables recognition of destructor calls for
			   non-class types and class types that have no
			   destructors that are not part of a qualified name
			   (e.g., ~A or ~int). */
#define GID_IS_TEMPLATE_DECLARATION   0x80
			/* If the identifier is a qualified name the
			   class component must refer to the prototype
			   instantiation, unless GID_IS_TEMPLATE_SPECIALIZATION
			   is also set. */
#define GID_IS_NEW_TYPE_NAME	      0x100
			/* Specifies that the name being scanned is the type
			   name in a new expression.  This causes the check
			   for an unexpected template argument list to be
			   suppressed because a new type name may be followed
			   by a less than sign. */
#define GID_IS_FIELD_SELECTION_OPERAND \
				      0x200
			/* Specifies that the name being scanned is the
			   operand following a "." or "->" operator. */
#define GID_USE_PROTOTYPE_NOT_NONREAL 0x400
			/* Specifies that a template reference such as
			   A<T> should be considered to refer to the
			   prototype instantiation, not the nonreal
			   instantiation of the same name. */
#define GID_IS_TYPENAME	0x800
			/* Specifies that the name being looked up follows the
			   typename keyword.  This affects the way that
			   a qualified name is handled. */
#define GID_CLASS_TEMPLATE_REQUIRED 0x1000
			/* Specifies that a class template name that is not
			   followed by an argument list must be returned as
			   the class template and should not be converted to
			   the current instantiation of the template, if such
			   an instantiation is currently in scope. */
#define GID_IS_TEMPLATE_SPECIALIZATION 0x2000
			/* If the identifier is a qualified name the
			   identifier named must be a member template. */
#define GID_IS_EXPR_CONTEXT 0x4000
			/* TRUE if the name is being coalesced in a context
			   that is known to be an expression context.  This
			   causes a "<" to be treated as a less than sign
			   and not the start of a template argument list
			   when it follows a nonreal class member. */
#define GID_IS_CLASS_TEMPLATE_DECL 0x8000
			/* TRUE if the name being coalesced is the class
			   template name in a class template declaration. */
#define GID_IS_TEMPLATE_PRESCAN 0x10000
			/* TRUE if the name is being coalesced during the
      			   prescan of a template declaration.  This suppresses
			   certain diagnostics. */
#define GID_FOLLOWS_TEMPLATE 0x20000
			/* TRUE if the name being coalesced follows the
      			   "template" keyword.  This is used is constructs
			   like "p->template f<x>(1)". */
#define GID_IMPLICIT_TYPE_CONTEXT 0x40000
			/* TRUE if the name is known to be a type based on
			   context. */
#define GID_IN_IF_EXISTS 0x80000
			/* TRUE when scanning the identifier of a Microsoft
			   __if_exists or __if_not_exists directive. */
#define GID_IS_FRIEND_DECL 0x100000
			/* TRUE when scanning the declarator of a friend
			   function declaration. */
#define GID_ERROR_FLAGS (GID_DISALLOW_QUALIFIED_NAME |		\
			 GID_DISALLOW_GLOBAL_QUALIFIER |	\
			 GID_DISALLOW_OPERATOR_NAME)
			/* Contains all the flags used by the routine
			   check_for_generalized_identifier_errors.
			   Is used to mask the error flags so that errors
			   are only reported once. */

/* Lookup modes supported by coalesce_and_lookup_generalized_identifier.
   If this list is changed the corresponding array of ID lookup options
   in lexical.c must also be changed.  */
typedef enum /* an_identifier_lookup_mode */ {
  ilm_normal,		/* Find any symbol. */
  ilm_tag,		/* Find only tag names. */
  ilm_tentative_type,	/* Uses IDL_TENTATIVE_TYPE_LOOKUP to do the lookup. */
  ilm_ctor_initializer_name,
			/* Used to look up identifiers in the initializer
			   list of a constructor declaration. */
  ilm_qualified_ctor_initializer_name,
			/* Used to look up qualified identifiers in the
                           initializer list of a constructor declaration. */
  ilm_namespace,	/* Find only namespace names. */
  ilm_typename,		/* Uses IDL_TYPENAME_LOOKUP to do the lookup. */
  ilm_class,		/* Find only class names. */
  ilm_template_linkage,	/* Uses IDL_LINKAGE_LOOKUP to do the lookup, create
			   nonreal members as templates. */
  ilm_using_declaration,
			/* Uses IDL_USING_DECLARATION to do the lookup. */
  ilm_using_typename,	/* Uses both IDL_USING_DECLARATION and
			   IDL_TYPENAME_LOOKUP to do the lookup. */
  ilm_expr,		/* Uses IDL_IS_EXPR_CONTEXT. */
  ilm_last
} an_identifier_lookup_mode;


typedef struct a_token_cache *a_token_cache_ptr;
typedef struct a_cached_token *a_cached_token_ptr;
typedef struct a_token_cache {
  /* Data structure used to hold a token cache, i.e., some number of
     tokens that are being saved for later rescanning. */
  a_cached_token_ptr
		first_token,
		last_token;
			/* First and last tokens on the list, or both NULL
			   if the list is empty. */
  a_byte_boolean
		is_reusable;
			/* TRUE if this cache will be reused (e.g.,
			   for a template cache).  This should be TRUE if
			   there is any possibility that the cache may
			   be reused. */
#if DEBUG
  unsigned long	token_count;
			/* The number of tokens in this cache.  Used for
			   tracking memory usage. */
  unsigned long	pragma_count;
			/* The number of pragma entries pointed to by tokens
			   in this cache.  Used for tracking memory usage. */
#endif /* DEBUG */
} a_token_cache;


/* These includes are placed here so that a_token_cache will be defined
   for general use before including these files. */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Array of opname kinds indexed by token kind. */
EXTERN an_opname_kind opname_kind_for_token[(int)tok_last+1]
#if VAR_INITIALIZERS
= {(an_opname_kind)onk_none,          /* tok_identifier */
   (an_opname_kind)onk_none,          /* tok_float_constant */
   (an_opname_kind)onk_none,          /* tok_fixed_point_constant */
   (an_opname_kind)onk_none,          /* tok_int_constant */
   (an_opname_kind)onk_none,          /* tok_char_constant */
   (an_opname_kind)onk_none,          /* tok_string_literal */
   (an_opname_kind)onk_none,          /* tok_end_of_source */
   (an_opname_kind)onk_none,          /* tok_newline */
   (an_opname_kind)onk_none,          /* tok_header_name */
   (an_opname_kind)onk_none,          /* tok_pp_number */
   (an_opname_kind)onk_none,          /* tok_digit_sequence */
   (an_opname_kind)onk_none,          /* tok_cpp_quote */
   (an_opname_kind)onk_none,          /* tok_ptr_to_member */
   (an_opname_kind)onk_subscript,     /* operator[] starts with tok_lbrace */
   (an_opname_kind)onk_none,          /* tok_rbrace */
   (an_opname_kind)onk_function_call, /* operator() starts with tok_lparen */
   (an_opname_kind)onk_none,          /* tok_rparen */
   (an_opname_kind)onk_none,          /* tok_period */
   (an_opname_kind)onk_arrow,
   (an_opname_kind)onk_plus_plus,
   (an_opname_kind)onk_minus_minus,
   (an_opname_kind)onk_ampersand,
   (an_opname_kind)onk_star,
   (an_opname_kind)onk_plus,
   (an_opname_kind)onk_minus,
   (an_opname_kind)onk_compl,
   (an_opname_kind)onk_not,
   (an_opname_kind)onk_divide,
   (an_opname_kind)onk_remainder,
   (an_opname_kind)onk_shift_left,
   (an_opname_kind)onk_shift_right,
   (an_opname_kind)onk_lt,
   (an_opname_kind)onk_gt,
   (an_opname_kind)onk_le,
   (an_opname_kind)onk_ge,
   (an_opname_kind)onk_eq,
   (an_opname_kind)onk_ne,
   (an_opname_kind)onk_excl_or,
   (an_opname_kind)onk_or,
   (an_opname_kind)onk_and_and,
   (an_opname_kind)onk_or_or,
   (an_opname_kind)onk_question,      /* Used only in front end. */
   (an_opname_kind)onk_none,          /* tok_colon */
   (an_opname_kind)onk_assign,
   (an_opname_kind)onk_times_assign,
   (an_opname_kind)onk_divide_assign,
   (an_opname_kind)onk_remainder_assign,
   (an_opname_kind)onk_plus_assign,
   (an_opname_kind)onk_minus_assign,
   (an_opname_kind)onk_shift_left_assign,
   (an_opname_kind)onk_shift_right_assign,
   (an_opname_kind)onk_and_assign,
   (an_opname_kind)onk_excl_or_assign,
   (an_opname_kind)onk_or_assign,
   (an_opname_kind)onk_comma,
   (an_opname_kind)onk_none,          /* tok_sharp */
   (an_opname_kind)onk_none,          /* tok_paste */
   (an_opname_kind)onk_gnu_min,       /* tok_gnu_min */
   (an_opname_kind)onk_gnu_max,       /* tok_gnu_max */
   (an_opname_kind)onk_none,          /* tok_lbrace */
   (an_opname_kind)onk_none,          /* tok_rbrace */
   (an_opname_kind)onk_none,          /* tok_semicolon */
   (an_opname_kind)onk_none,          /* tok_ellipsis */
   (an_opname_kind)onk_none,          /* tok_auto */
   (an_opname_kind)onk_none,          /* tok_break */
   (an_opname_kind)onk_none,          /* tok_case */
   (an_opname_kind)onk_none,          /* tok_char */
   (an_opname_kind)onk_none,          /* tok_const */
   (an_opname_kind)onk_none,          /* tok_continue */
   (an_opname_kind)onk_none,          /* tok_default */
   (an_opname_kind)onk_none,          /* tok_do */
   (an_opname_kind)onk_none,          /* tok_double */
   (an_opname_kind)onk_none,          /* tok_else */
   (an_opname_kind)onk_none,          /* tok_enum */
   (an_opname_kind)onk_none,          /* tok_extern */
   (an_opname_kind)onk_none,          /* tok_float */
   (an_opname_kind)onk_none,          /* tok_for */
   (an_opname_kind)onk_none,          /* tok_goto */
   (an_opname_kind)onk_none,          /* tok_if */
   (an_opname_kind)onk_none,          /* tok_int */
   (an_opname_kind)onk_none,          /* tok_long */
   (an_opname_kind)onk_none,          /* tok_register */
   (an_opname_kind)onk_none,          /* tok_return */
   (an_opname_kind)onk_none,          /* tok_short */
   (an_opname_kind)onk_none,          /* tok_signed */
   (an_opname_kind)onk_none,          /* tok_sizeof */
   (an_opname_kind)onk_none,          /* tok_static */
   (an_opname_kind)onk_none,          /* tok_struct */
   (an_opname_kind)onk_none,          /* tok_switch */
   (an_opname_kind)onk_none,          /* tok_typedef */
   (an_opname_kind)onk_none,          /* tok_union */
   (an_opname_kind)onk_none,          /* tok_unsigned */
   (an_opname_kind)onk_none,          /* tok_void */
   (an_opname_kind)onk_none,          /* tok_volatile */
   (an_opname_kind)onk_none,          /* tok_while */
   (an_opname_kind)onk_none,          /* tok_generic */
   (an_opname_kind)onk_none,          /* tok_genericfx */
   (an_opname_kind)onk_none,          /* tok_alignof */
   (an_opname_kind)onk_none,          /* tok_intaddr */
   (an_opname_kind)onk_none,          /* tok_va_start */
   (an_opname_kind)onk_none,          /* tok_va_arg */
   (an_opname_kind)onk_none,          /* tok_va_end */
   (an_opname_kind)onk_none,          /* tok_va_copy */
   (an_opname_kind)onk_none,          /* tok_builtin_offsetof */
   (an_opname_kind)onk_none,          /* tok_restrict */
   (an_opname_kind)onk_none,          /* tok_gnu_restrict */
   (an_opname_kind)onk_none,          /* tok_c99_bool */
   (an_opname_kind)onk_none,          /* tok_c99_complex */
   (an_opname_kind)onk_none,          /* tok_c99_imaginary */
   (an_opname_kind)onk_none,          /* tok_imaginary_unit */
   (an_opname_kind)onk_none,          /* tok_nan */
   (an_opname_kind)onk_none,          /* tok_infinity */
   (an_opname_kind)onk_none,          /* tok_fract */
   (an_opname_kind)onk_none,          /* tok_accum */
   (an_opname_kind)onk_none,          /* tok_sat */
#if MICROSOFT_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_abstract */
   (an_opname_kind)onk_none,          /* tok_override */
   (an_opname_kind)onk_none,          /* tok_sealed */
   (an_opname_kind)onk_none,          /* tok_cdecl */
   (an_opname_kind)onk_none,          /* tok_declspec */
   (an_opname_kind)onk_none,          /* tok_fastcall */
   (an_opname_kind)onk_none,          /* tok_stdcall */
   (an_opname_kind)onk_none,          /* tok_thiscall */
   (an_opname_kind)onk_none,          /* tok_microsoft_inline */
   (an_opname_kind)onk_none,          /* tok_forceinline */
   (an_opname_kind)onk_none,          /* tok_unaligned */
   (an_opname_kind)onk_none,          /* tok_microsoft_try */
   (an_opname_kind)onk_none,          /* tok_finally */
   (an_opname_kind)onk_none,          /* tok_leave */
   (an_opname_kind)onk_none,          /* tok_except */
   (an_opname_kind)onk_none,          /* tok_int8 */
   (an_opname_kind)onk_none,          /* tok_int16 */
   (an_opname_kind)onk_none,          /* tok_int32 */
   (an_opname_kind)onk_none,          /* tok_int64 */
   (an_opname_kind)onk_none,          /* tok_based */
   (an_opname_kind)onk_none,          /* tok_uuidof */
   (an_opname_kind)onk_none,          /* tok_assume */
   (an_opname_kind)onk_none,          /* tok_charize */
   (an_opname_kind)onk_none,          /* tok_if_exists */
   (an_opname_kind)onk_none,          /* tok_if_not_exists */
#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
   (an_opname_kind)onk_none,          /* tok_end_of_if_exists */
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
   (an_opname_kind)onk_none,          /* tok_super */
   (an_opname_kind)onk_none,          /* tok_noop */
   (an_opname_kind)onk_none,          /* tok_interface */
   (an_opname_kind)onk_none,          /* tok_microsoft_ptr32 */
   (an_opname_kind)onk_none,          /* tok_microsoft_ptr64 */
   (an_opname_kind)onk_none,          /* tok_microsoft_sptr */
   (an_opname_kind)onk_none,          /* tok_microsoft_uptr */
   (an_opname_kind)onk_none,          /* tok_microsoft_w64 */
   (an_opname_kind)onk_none,          /* tok_microsoft_lprefix */
   (an_opname_kind)onk_none,          /* tok_microsoft_identifier */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_microsoft_asm */
   (an_opname_kind)onk_none,          /* tok_func_name */
   (an_opname_kind)onk_none,          /* tok_function_name */
   (an_opname_kind)onk_none,          /* tok_pretty_function_name */
   (an_opname_kind)onk_none,          /* tok_decorated_function_name */
#if NEAR_AND_FAR_ALLOWED
   (an_opname_kind)onk_none,          /* tok_near */
   (an_opname_kind)onk_none,          /* tok_far */
#endif /* NEAR_AND_FAR_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_attribute */
   (an_opname_kind)onk_none,          /* tok_va_start_single_operand */
   (an_opname_kind)onk_none,          /* tok_builtin_types_compatible */
   (an_opname_kind)onk_none,          /* tok_gnu_real */
   (an_opname_kind)onk_none,          /* tok_gnu_imag */
#endif /* GNU_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_colon_colon */
   (an_opname_kind)onk_none,          /* tok_period_star */
   (an_opname_kind)onk_arrow_star,
   (an_opname_kind)onk_none,          /* tok_asm */
   (an_opname_kind)onk_none,          /* tok_catch */
   (an_opname_kind)onk_none,          /* tok_class */
   (an_opname_kind)onk_delete,
   (an_opname_kind)onk_none,          /* tok_friend */
   (an_opname_kind)onk_none,          /* tok_inline */
   (an_opname_kind)onk_new,
   (an_opname_kind)onk_none,          /* tok_operator */
   (an_opname_kind)onk_none,          /* tok_private */
   (an_opname_kind)onk_none,          /* tok_protected */
   (an_opname_kind)onk_none,          /* tok_public */
   (an_opname_kind)onk_none,          /* tok_template */
   (an_opname_kind)onk_none,          /* tok_this */
   (an_opname_kind)onk_none,          /* tok_throw */
   (an_opname_kind)onk_none,          /* tok_try */
   (an_opname_kind)onk_none,          /* tok_virtual */
   (an_opname_kind)onk_none,          /* tok_wchar_t */
   (an_opname_kind)onk_none,          /* tok_const_cast */
   (an_opname_kind)onk_none,          /* tok_dynamic_cast */
   (an_opname_kind)onk_none,          /* tok_explicit */
   (an_opname_kind)onk_none,          /* tok_export */
   (an_opname_kind)onk_none,          /* tok_mutable */
   (an_opname_kind)onk_none,          /* tok_namespace */
   (an_opname_kind)onk_none,          /* tok_reinterpret_cast */
   (an_opname_kind)onk_none,          /* tok_static_cast */
   (an_opname_kind)onk_none,          /* tok_typeid */
   (an_opname_kind)onk_none,          /* tok_using */
   (an_opname_kind)onk_none,          /* tok_bool */
   (an_opname_kind)onk_none,          /* tok_false */
   (an_opname_kind)onk_none,          /* tok_true */
   (an_opname_kind)onk_none,          /* tok_typename */
   (an_opname_kind)onk_none,          /* tok_static_assert */
   (an_opname_kind)onk_none,          /* tok_decltype */
   (an_opname_kind)onk_none,          /* tok_typeof */
   (an_opname_kind)onk_none,          /* tok_extension */
   (an_opname_kind)onk_none,          /* tok_null */
   (an_opname_kind)onk_none,          /* tok_overload */
#if SUN_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_global_link_scope */
   (an_opname_kind)onk_none,          /* tok_symbolic_link_scope */
   (an_opname_kind)onk_none,          /* tok_hidden_link_scope */
#endif /* SUN_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_thread */
#if UPC_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_upc_strict */
   (an_opname_kind)onk_none,          /* tok_upc_relaxed */
   (an_opname_kind)onk_none,          /* tok_upc_shared */
   (an_opname_kind)onk_none,          /* tok_upc_forall */
   (an_opname_kind)onk_none,          /* tok_upc_barrier */
   (an_opname_kind)onk_none,          /* tok_upc_notify */
   (an_opname_kind)onk_none,          /* tok_upc_wait */
   (an_opname_kind)onk_none,          /* tok_upc_fence */
   (an_opname_kind)onk_none,          /* tok_upc_threads */
   (an_opname_kind)onk_none,          /* tok_upc_mythread */
   (an_opname_kind)onk_none,          /* tok_upc_blocksizeof */
   (an_opname_kind)onk_none,          /* tok_upc_localsizeof */
   (an_opname_kind)onk_none,          /* tok_upc_elemsizeof */
#endif /* UPC_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_has_assign */
   (an_opname_kind)onk_none,          /* tok_has_copy */
   (an_opname_kind)onk_none,          /* tok_has_nothrow_assign */
   (an_opname_kind)onk_none,          /* tok_has_nothrow_constructor */
   (an_opname_kind)onk_none,          /* tok_has_nothrow_copy */
   (an_opname_kind)onk_none,          /* tok_has_trivial_assign */
   (an_opname_kind)onk_none,          /* tok_has_trivial_constructor */
   (an_opname_kind)onk_none,          /* tok_has_trivial_copy */
   (an_opname_kind)onk_none,          /* tok_has_trivial_destructor */
   (an_opname_kind)onk_none,          /* tok_has_user_destructor */
   (an_opname_kind)onk_none,          /* tok_has_virtual_destructor */
   (an_opname_kind)onk_none,          /* tok_is_abstract */
   (an_opname_kind)onk_none,          /* tok_is_base_of */
   (an_opname_kind)onk_none,          /* tok_is_class */
   (an_opname_kind)onk_none,          /* tok_is_convertible_to */
   (an_opname_kind)onk_none,          /* tok_is_empty */
   (an_opname_kind)onk_none,          /* tok_is_enum */
   (an_opname_kind)onk_none,          /* tok_is_pod */
   (an_opname_kind)onk_none,          /* tok_is_polymorphic */
   (an_opname_kind)onk_none,          /* tok_is_union */
   (an_opname_kind)onk_none,          /* tok_unimplemented */
   (an_opname_kind)onk_none,          /* tok_error */
   (an_opname_kind)onk_none,          /* tok_removed_default_arg */
   (an_opname_kind)onk_last           /* tok_last */
  }
#endif /* VAR_INITIALIZERS */
;
/*
Array giving the token name for each opname kind.
*/
EXTERN char	*opname_names[(int)onk_last];


/*
Structure used to build a list of files to be pre-included.
*/
typedef struct a_preinclude_file *a_preinclude_file_ptr;
typedef struct a_preinclude_file {
  a_preinclude_file_ptr
		next;	/* Pointer to the next entry in a linked list of
			   preinclude files. */
  char		*file_name;
			/* Name of the file to be preincluded. */
} a_preinclude_file;

/*
Structure used to record information about files that have been included.
See the comment preceding find_include_history in lexical.c.
*/
typedef struct an_include_file_history *an_include_file_history_ptr;
typedef struct an_include_file_history {
  char          *full_name;
			/* Pointer to the full path name of the include
			   file. */
  a_bit_field	suppress_subsequent_include:1;
			/* TRUE if this file is potentially one that can
			   have subsequent includes suppressed. */
  a_bit_field	pragma_once:1;
			/* TRUE if this file contained a "#pragma once"
			   directive. */
  a_bit_field	ifdef_guard:1;
			/* TRUE if this file was guarded by a #ifdef. */
  a_bit_field	ifndef_guard:1;
			/* TRUE if this file was guarded by a #ifndef. */
  char          *controlling_macro_name;
			/* The name of the macro used to guard the include
			   file against multiple inclusions. */
} an_include_file_history;


/*
The order of these states is important.
*/
#define IFG_STATE_START		0
			/* The state when a file is first opened and before
			   any tokens have been scanned. */
#define IFG_STATE_ACCEPT	1
			/* We have seen the closing #endif of a top level
			   #ifdef or #ifndef.  There must be no additional
			   tokens in this file. */
#define IFG_STATE_FAIL		2
			/* This file is not a candidate for suppression of
			   subsequent includes. */
#define IFG_STATE_INTERMED	3
			/* We are in the process of scanning the tokens
		 	   inside a top level #ifdef or #ifndef. */
#define IFG_STATE_ONCE		4
			/* A #pragma once has been encountered in the file. */


extern a_boolean suppress_subsequent_include_of_file(
				 char                         *full_name,
				 an_include_file_history_ptr *ifhp_ptr,
				 a_boolean		     create);

extern a_boolean find_include_history(char                        *full_name,
	    		              an_include_file_history_ptr *ifhp_ptr,
			              a_boolean		          create);

extern a_byte get_ifg_state(void);
extern void set_ifg_state(a_byte new_state);

EXTERN a_boolean
		any_tokens_fetched_from_curr_input_file;
			/* TRUE if any "real" tokens have been fetched from
			   the current input file.  This is used to
			   determine whether the current file is a candidate
			   to have subsequent inclusions suppressed.  This
			   variable is cleared at various times during the
			   scanning of the include file so great care must
			   be used before attempting to use this value for
			   some more general purpose. */


/*
Variables pertaining to the input stack (for include files and the
primary source file) and the current input file (the top entry on the
stack).
*/
typedef struct an_input_stack_entry *an_input_stack_entry_ptr;
typedef struct an_input_stack_entry {
  FILE		*file;
			/* The file at this level.  NULL if the file has
			   been closed and must be reopened (see position). */
  char		*file_name;
			/* The form of the file name to be used in error
			   messages and the like, null-terminated.
			   May have been modified by a #line directive. */
  char		*full_name;
			/* The form of the file name to be used in opening the
			   file (may have directory).  Null-terminated.
			   Can be the same as file_name. */
  char		*dir_name;
			/* The directory name of full_name.  Can be "". */
  a_directory_name_entry_ptr
		dir_entry;
			/* The directory name entry on the search path that
			   was used to find this file, or NULL if the
			   search path was not used (e.g., for an absolute
			   path name). */
  a_line_number	line_number;
			/* Physical line number of the line most recently
			   read from this file.  May have been modified by
			   a #line directive. */
  long		position;
			/* If the file was closed to preserve file blocks
			   (i.e., file == NULL), this is the position for
			   an fseek to re-establish the proper position. */
  a_source_file_ptr
		assoc_il_file,
		assoc_actual_il_file;
			/* The associated source file description in the
			   intermediate language.  The two pointers usually
			   point to the same entry, but when a #line directive
			   is processed assoc_il_file points to a new entry
			   for the #line information and assoc_actual_il_file
			   stays as it was (pointing to the entry for the
			   file actually being read). */
  long		base_pp_if_stack_depth;
			/* The value of pp_if_stack_depth (see preproc.c)
			   at entry to this file.  Needed because ANSI C
			   requires that each #if be closed in the same
			   file in which it began. */
  a_line_number actual_line;
			/* The physical line number of the line currently
			   being read or last read from this file.  This
			   field will not be modified by a #line directive. */
  a_line_number next_index_point;
			/* The next physical line number whose file position
			   should be recorded in file position index table
			   maintained for diagnostic generation. */
  a_bit_field	is_include_file:1;
			/* TRUE if this file was added to the input stack
			   as the result of a #include directive or a
			   --preinclude command-line directive.  FALSE
			   for all other cases including implicitly included
			   source files. */
  a_bit_field	from_system_include_dir:1;
			/* TRUE if this source file was found in an include
			   directory marked as a "system" include directory.
			   Warnings are suppressed when processing system
			   include directories. */
  a_bit_field	nested_inclusion:1;
			/* TRUE if this is a nested inclusion of a file
			   already on the input stack. */
  a_bit_field	saved_any_tokens_fetched:1;
			/* Used to save and restore the value of the global
			   variable any_tokens_fetched_from_curr_input_file. */
  a_bit_field	is_preinclude:1;
			/* TRUE if this is a preincluded file. */
  a_bit_field	do_not_advance_past_end_of_file:1;
			/* TRUE if when we reach the end of this file we
			   should stay there and not advance beyond it. */
  bitfield_to_avoid_codecenter_warnings()
  a_byte        ifg_state;
			/* Include file guard state information used to
                           determine whether subsequent inclusions of this
                           file may be suppressed. */
  an_include_file_history_ptr
		include_history;
                        /* Pointer to the structure that preserves information
                           used for include file guard processing. */
  a_unicode_source_kind
		unicode_source_kind;
			/* Indication of the kind of Unicode encoding (e.g.,
			   UTF-8, UTF-16) of this source file.  usk_none
			   except in versions with UNICODE_SOURCE_SUPPORTED
			   set to TRUE. */
} an_input_stack_entry;

/* See lexical.c for the definitions of input_stack, depth_input_stack,
   and seq_number_last_read. */
EXTERN an_input_stack_entry_ptr
		curr_ise;
			/* Pointer to input_stack[depth_input_stack].  NULL
			   if depth_input_stack == -1. */

#if UNICODE_SOURCE_SUPPORTED
EXTERN a_unicode_source_kind
		curr_file_unicode_source_kind;
			/* If not usk_none, indicates the kind of Unicode
			   source characters being read in the current source
			   file. */
#endif /* UNICODE_SOURCE_SUPPORTED */


#if FULLY_RESOLVED_MACRO_POSITIONS
/*
Data structures allowing offsets in a given buffer -- a macro definition,
argument, or expansion -- to be translated into the source positions from
which they originally came.
*/
typedef struct a_macro_text_map_entry *a_macro_text_map_entry_ptr;
typedef struct a_macro_text_map_entry {
  /* One of an array of entries that enables a given position in a macro
     definition string, a macro argument, or a macro expansion (source line
     modification) to be mapped into the corresponding source location.  Each
     entry has an offset from the beginning of the text buffer of the macro
     definition, argument, or expansion, and the source location to which it
     corresponds. */
  sizeof_t	start_of_region;
  a_simple_source_position
		corresponding_source_pos;
#if MACRO_INVOCATION_TREE_IN_IL
  a_macro_invocation_record_index
		macro_context;
			/* Identifies the macro invocation to which this
			   entry belongs.  (Logically, this information is
			   part of the source line modification with which
			   this entry is associated.  However, in pcc and
			   Microsoft modes, all the source line modifications
			   for a given top-level macro invocation are
			   coalesced into a single source line modification,
			   so having the macro context here allows the parent
			   macro chain to be preserved in spite of the loss of
			   the source line modifications.) */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
} a_macro_text_map_entry;

typedef struct a_macro_text_map *a_macro_text_map_ptr;
typedef struct a_macro_text_map {
  /* A data structure that enables each offset in a macro definition string,
     a macro argument, or a macro expansion (source line modification) to be
     mapped to the corresponding source location.  This is done via a sorted
     extensible array of a_macro_text_map_entry objects; each entry specifies
     the corresponding source position for that offset in the text buffer of
     the macro definition, argument, or expansion and, by extension, for all
     offsets up to that of the next entry. */
  sizeof_t	max_entries;
			/* The number of a_macro_text_map_entry objects that
			   can be stored in the current allocation of the
			   entries array. */
  sizeof_t	num_entries;
			/* The number of a_macro_text_map_entry objects that
			   are currently in the entries array. */
  a_boolean	resizable;
			/* If TRUE, the entries array is allocated using
			   alloc_resizable_buffer and can be extended to
			   accommodate more entries.  Otherwise, the size is
			   fixed, and either entries is a pointer to a
			   subrange of the entries in another text map or the
			   array is allocated in front-end memory and thus
			   will be saved in precompiled headers. */
  a_macro_text_map_entry_ptr
		entries;
			/* Pointer to an array of a_macro_text_map_entry
			   objects.  These are kept in order of increasing
			   offset into the buffer with which this map is
			   associated. */
} a_macro_text_map;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Data structures that enable tracking of the location of a concatenation in
a macro expansion when check_concatenations is TRUE.  This information is
used to detect cases in which a concatenation did not result in a valid
token, as required by the language standards: if a new token begins at the
location occupied by the right-hand operand of ##, the concatenation did
not form a valid token.  (Cases in which a putative token begins in the
left-hand operand, continues across the concatenation point, and becomes
invalid at a later point are not detected as concatenation failures.)

A source line modification that incorporates text created by concatenation
has a singly-linked list of concatenation records in the order of their
offsets within the inserted text.  As tokenization proceeds through the
inserted text, concatenation records are removed from the list and placed
on a list of available records for potential reuse.
*/
typedef struct a_concatenation_record *a_concatenation_record_ptr;
typedef struct a_concatenation_record {
  a_concatenation_record_ptr
		next;	/* Pointer to the next entry in the list of records
			   for a source line modification or the list of
			   available records; NULL if this is the last
			   record on the list. */
  char		*line_loc;
			/* Pointer to the first character in a source line
			   modification's inserted text that comes from the
			   right-hand operand of the ## operator. */
  a_symbol_ptr	macro_sym;
			/* The macro in whose expansion the concatenation
			   occurs. */
} a_concatenation_record;

EXTERN a_concatenation_record_ptr
		avail_concatenation_records;
			/* List of entries that have been allocated and
			   freed and are available for reuse. */

/*
Variables pertaining to the current logical source line.  A "logical"
source line is what results after trigraph characters (see standard,
2.2.1.1) have been replaced, and lines ending in newline-backslash
have been spliced with the lines immediately following (see standard,
2.1.1.2).

Note that the source line and positions within it are supposed to be
hidden within the lexical routines.  Outside of these routines, all
positions are in a_source_position form (sequence number, column).
One exception is the pointers start_of_curr_token and end_of_curr_token,
which point to characters in curr_source_line.

On reading (by read_logical_source_line), curr_source_line contains the
characters of the logical source line, and orig_line_modif_list
contains information on the trigraphs and line splices sufficient to
map locations in curr_source_line back to source positions and to
exactly reconstruct the original lines.  These transformations are
done in curr_source_line because they have significance at the intra-token
level.

If any preprocessor transformations are done (comments deleted, macros
expanded), source_line_modif_list will be non-NULL and will contain
information about the transformations done.  The line in curr_source_line
is only logically modified by this information.  Physically, it is only
changed in that the first character of text being deleted is changed to 
ATTENTION_MARKER as a cue to indicate that the source_line_modif_list
should be consulted.  The text that is the expansion of a macro appears
in macro_buffer, and is pointed to by the source_line_modif entry.
These transformations have significance at the inter-token level; they
cannot occur in the middle of tokens.

A fundamental principle of input line scanning is that it must be possible
to represent the current source position completely in terms of a
single pointer (curr_char_loc), and it must be possible to set that
pointer to an arbitrary location in the source input and scan forward
using that pointer only.  Therefore, if there is some point in the
sequence of characters following curr_char_loc where input must be
diverted elsewhere, there must be a marker character at that point to
force examination of related data structures.
*/
EXTERN char	*curr_source_line;
			/* Characters of the current logical source line,
			   ended by LE_NEWLINE and LE_END_OF_LINE lexical
			   escape sequences.  Space is dynamically allocated,
			   and its upper bound is given by
			   after_end_of_curr_source_line.  See lexical_init
			   for the initial allocation.  When
			   after_end_of_all_source is TRUE, this array
			   contains just the LE_END_OF_LINE lexical escape
			   sequence (no newline, nothing else). */
#define CURR_SOURCE_LINE_INITIAL_ALLOCATION 3000
			/* Initial allocation size for curr_source_line.  The
			   initial allocation should be such that almost all
			   cases can be accepted (so that the realloc is
			   hardly ever needed) -- that means big enough for
			   all the lines of a large macro definition.
			   Subsequent reallocations will double the amount
			   previously allocated. */
EXTERN char	*after_end_of_curr_source_line;
			/* Address past the last element of curr_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because curr_source_line line can reallocated larger
			   if needed. */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
EXTERN char	**logical_char_info;
			/* A dynamically allocated array that is used to
			   convert a pointer into curr_source_line from a
			   raw byte offset to a logical column number.  Each
			   multibyte character in the source line represents
			   one logical character.  When a non-initial character
			   of a token is encountered, the pointer to that
			   non-initial character is stored in the next
			   available entry in this array (the first such
			   pointer is in element zero, the second in element
			   one, etc.).  The array is allocated with the same
			   number of elements as curr_source_line to guarantee
			   that it is not possible to overflow the array.
			   To convert a pointer into curr_source_line into
			   a logical column number, you need to find the entry
			   in this array that contains the largest pointer that
			   is <= the pointer you are looking for.  The array
			   index+1 is the adjustment needed to convert to
			   a logical column number. */
#endif /* MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
EXTERN int	logical_char_info_entries_used;
			/* The number of entries in the logical_char_info
			   array that are in use for the current source
			   line (zero if no entries are in use).  Only
			   referenced when MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
			   is TRUE. */
EXTERN a_seq_number
		curr_seq_number;
			/* The sequence number of the first physical line
			   in curr_source_line.  Once the end of file on
			   the primary source file has been passed, this
			   indicates a sequence number one past the highest
			   sequence number actually read, as an indication
			   of a sort of end-of-file line. */
EXTERN a_boolean
		at_end_of_source_file;
			/* If TRUE, there is no current logical source line;
			   the last attempt to read one ran into an end of
			   file instead.  This may, however, be only the end
			   of an include file, and not of the entire source
			   sequence (see pop_input_stack).  The current
			   position is at the end of the current input file,
			   but still within it -- the stack has not yet been
			   popped.  curr_source_line still contains the line
			   most recently read. */

EXTERN a_seq_number
		seq_number_last_read;
			/* The sequence number of the physical line last read
			   from curr_input_stream.  This is often not the
			   same as the line number in the file.  Once the
			   end of file on the primary source file has been
			   passed, this indicates a sequence number one past
			   the highest sequence number actually read, as an
			   indication of a sort of end-of-file line. */
/*
Variable giving the current character position in the current
logical source line.  Usually, this points within curr_source_line,
but when macros are being scanned, it can point elsewhere (in macro_buffer
or in an argument string).
*/
EXTERN char	*curr_char_loc;
			/* Pointer to the current character within
			   curr_source_line.  More precisely, this is the
			   character about to be processed.  Points at the
			   LE_END_OF_LINE lexical escape sequence at the end
			   of the line when the line has been completely
			   scanned. */

/*
Escape characters used within curr_source_line and macro_buffer.
*/
/*
ATTENTION_MARKER is a one-character escape indicating the point at
which a source line modification occurs.  The source_line_modif_list
must be consulted for details of the modification.

ATTENTION_MARKER must be something that will not otherwise occur in the
source line, including in multibyte characters.  The newline character
is used.  The real newline at the end of input lines is replaced by a
two-character escape to free up a character that can be used as
a single-character escape.
*/
#define ATTENTION_MARKER '\n'
/*
Two-character escapes.  The first character is always LE_ESCAPE (a zero,
which is guaranteed not to occur otherwise in source lines).  Lint thinks
that a test for LE_ESCAPE followed by a check of the next character is
looking beyond the end of a null-terminated string.
*/
#define LE_ESCAPE 0 /*lint --e(448)*/
#define LE_ESCAPE_LEN 2	/* Length of escape sequence. */
/*
Second character of two-character escape is one of the following.
Note that zero is not used so that one can find LE_ESCAPE characters
without worrying that they are the second character of a two-character
escape.
*/
#define LE_END_OF_LINE 1
			/* End of curr_source_line. */
#define LE_NEWLINE 2
			/* Newline character (replaced with an escape so
			   that the newline character itself can be used
			   as ATTENTION_MARKER). */
#define LE_END_OF_INSERTION 3
			/* End of an insertion, e.g., the replacement text
			   inserted for a macro expansion. */
#define LE_END_OF_TOKEN 4
			/* End of a token.  Used as a token divider in
			   macro definitions and macro expansions, to
			   guarantee that the text will be tokenized the
			   same way as on the original macro definition. */
#define LE_INERT_MACRO 5
			/* Precedes an identifier that is the name of
			   a macro that appears within its own expansion,
			   and should therefore not be expanded.  Not used
			   in pcc preprocessing mode. */
#define LE_NULL 6
			/* In modes that allow a null (zero) character in
			   an input line (e.g., gcc mode), indicates such
			   a character. */
#define LE_COMMA_FROM_ARGUMENT 7
			/* In Microsoft mode, indicates that the following
			   comma token originally appeared at the top level
			   (i.e., not within parentheses) in the expanded
			   text of a macro argument.  (Used to prevent such
			   commas from delimiting macro arguments when the
			   expansion is rescanned.) */

/*
Modifications made to the current source line.  orig_line_modif holds
information needed to restore curr_source_line to its original form
when read; source_line_modif is the information needed to transform
curr_source_line into its fully preprocessed form (with macros expanded,
etc.).
*/

typedef enum /*an_orig_line_modif_kind*/ {
  olm_trigraph,
  olm_line_splice,
  olm_multiline_string_splice,
  olm_null		/* Null (zero) character in source line. */
} an_orig_line_modif_kind;

typedef struct an_orig_line_modif {
  /* A modification (trigraph or line splice) that has been made to the
     original source lines to turn them into the current contents of
     curr_source_line. */
  an_orig_line_modif_ptr
		next;
			/* A pointer to the next entry on the
			   orig_line_modif_list, or NULL if there is no
			   next entry.  Also used to link entries on
			   the avail_orig_line_modifs list. */
  char		*line_loc;
			/* The location in curr_source_line of the
			   modification.  Points to the character that
			   resulted from the trigraph, or the character
			   following the line splice. */
  an_orig_line_modif_kind
		kind;
			/* Kind of modification: trigraph or line splice. */
  union {
    /* When kind == olm_null, no variant fields. */
    /* When kind == olm_trigraph: */
    char	trigraph_orig_char;
			/* The original third character of the trigraph,
			   for example "=" in "? ? =" (extra space added
			   so that won't actually be a trigraph). */
    /* When kind == olm_line_splice or olm_multiline_string_splice: */
    a_seq_number
		line_splice_seq_number;
			/* The source sequence number of the line following
			   the line splice. */
  } variant;
} an_orig_line_modif;

EXTERN an_orig_line_modif_ptr
		orig_line_modif_list,
		end_orig_line_modif_list;
			/* List of modifications to the original source lines
			   to produce the current logical source line.  Entries
			   are in order by position in curr_source_line.
			   end_orig_line_modif_list points to the last entry
			   on the list.  Both pointers are NULL if there are
			   no changes (a very common case). */
EXTERN an_orig_line_modif_ptr
		avail_orig_line_modifs;
			/* List of entries that have been allocated and freed
			   and are available for reuse. */

typedef struct a_source_line_modif {
  /* A modification (text deletion or replacement) that must be made to
     the current contents of curr_source_line or macro_buffer to turn them
     into the fully preprocessed version of the line. */
  a_source_line_modif_ptr
		next;
			/* A pointer to the next entry on the
			   source_line_modif_list, or NULL if there is
			   no next entry.  Also used to link entries on the 
			   avail_source_line_modifs list. */
  a_source_line_modif_ptr
		next_in_hash_table;
			/* A pointer to the next entry in the same bucket
			   of the hash table of source line modifications. */
  char		*line_loc;
			/* The location in curr_source_line or macro_buffer
			   of the modification.  Points to an ATTENTION_MARKER
			   character that replaced the first character of
			   the text to be deleted.  NULL to indicate that
			   this entry is be inserted preceding the first
			   character of the source line (there can be only
			   one of those).  Also NULL for entries saved
			   because they contain macro argument text, when
			   the text is from a previous source line. */
  a_source_line_modif_ptr
		parent_modif;
			/* If line_loc points into the text inserted by
			   another modification, this points to that parent
			   modification entry.  Otherwise (i.e., if line_loc
			   points into the primary source line), NULL.
			   Meaningful only if parent_modif_determined is
			   TRUE. */
  sizeof_t	num_chars_to_delete;
			/* The number of characters to be logically deleted
			   (beginning at line_loc).  Greater than zero
			   (except that when line_loc == NULL, this is
			   zero). */
  a_bit_field	is_isolated_text:1;
			/* TRUE if this modification's text isn't connected
			   to the surrounding context.  When the end of the
			   insertion is reached, end-of-source is returned.
			   This is used during macro expansion of macro
			   arguments to prevent scanning more tokens from
			   the remainder of the source file, and also for
			   buffers that are temporary. */
  a_bit_field	is_for_comment:1;
			/* TRUE if this modification is due to a comment
			   in the source (as opposed to a macro expansion). */
  a_bit_field	parent_modif_determined:1;
			/* TRUE if a value has been determined for
			   parent_modif. */
  a_bit_field	being_rescanned_for_token_pasting:1;
			/* TRUE if this modification is for a macro expansion
			   that is currently being rescanned in order to
			   do old-style token pasting. */
  a_bit_field	locked:1;
			/* TRUE if this modification should not be moved (for
			   the purpose of optimizing searches).  Used when
			   this entry is used as a marker to delimit a prefix
			   part of the list of source line modifications. */
  a_bit_field	contains_saved_macro_argument_text:1;
			/* TRUE if this modification contains some text saved
			   for a macro argument, and therefore should not be
			   freed automatically when the next source line is
			   read. */
  char		orig_char;
			/* The character that was in the source line at
			   position line_loc (provided so that the original
			   line can be reconstructed; not needed otherwise).
			   Meaningless when line_loc == NULL. */
  char		inserted_chars[3];
			/* Place for an insert string of up to three characters
			   including the end-of-insertion lexical escape.
			   Used for the blank that replaces comments. */
  char		*inserted_text;
			/* Pointer to text to be inserted, in macro_buffer.
			   The text is terminated by an LE_END_OF_INSERTION
			   lexical escape.  Points to a zero-length string
			   if this is a deletion only. */
  char		*end_inserted_text;
			/* Pointer to the LE_END_OF_INSERTION lexical escape
			   at the end of inserted_text. */
  a_macro_def_ptr
		assoc_macro;
			/* The macro that generated this expansion.  This
			   is important in that the macro name is protected
			   from expansion within its own expansion.  If
			   this is NULL, the expansion did not come from
			   a macro (it might have come from deletion of
			   a comment or insertion of the text of a macro
			   argument so macro expansion can be done on it). */
  unsigned long	sequence_id;
			/* An integer indicating the sequence in which 
			   modifications were added to the source line
			   (higher values were added later).  Generated
			   from the value of sequence_id_for_source_line_modifs
			   when this entry was created. */
  a_source_position
		source_position;
			/* When source_position.seq != 0, this indicates
			   the source position associated with this
			   modification.  This is particularly useful when
			   the modification is for a multi-line macro call. */
  char		*text_from_primary_source_line;
			/* If non-NULL, text from the location pointed to
			   to the end-of-insertion marker is from the primary
			   source line, placed in this modification so that
			   it can be token-pasted with the end of a macro
			   expansion in pcc mode. */
#if FULLY_RESOLVED_MACRO_POSITIONS
  a_macro_text_map
		text_map;
			/* A map of offsets within inserted_text to the
			   original positions (macro definition and arguments)
			   from which they came.  (This field is unused for
			   modifications not resulting from macro expansions.)
			   Note that this text map is not "standalone" -- the
			   num_entries and entries fields denote a subset of
			   the macro_text_map defined in macro.c, so this
			   array is not extensible. */
  int		num_active_position_trackers;
			/* The number of a_text_map_position_trackers that are
			   currently referring to this source line modif (see
			   macro.c).  If the inserted text is compacted when
			   the macro_buffer is reallocated, any position
			   trackers referring to this source line modification
			   may need to be updated accordingly. */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if MACRO_INVOCATION_TREE_IN_IL
  a_macro_invocation_record_index
		invocation_record;
			/* The invocation record for the macro invocation
			   that resulted in this modification, or
			   NO_PARENT_MACRO_INVOCATION if this modification is
			   not the result of a macro expansion. */
  a_macro_invocation_record_index
		invocation_depth;
			/* The depth in the invocation stack of this
			   invocation (0 for modifications that are not the
			   result of a macro expansion). */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
  a_concatenation_record_ptr
		concatenations;
			/* Head of a singly-linked list of concatenation
			   records that apply to this modification's
			   inserted text; NULL if there are none (including
			   the case where check_concatenations is
			   FALSE). */
} a_source_line_modif;

EXTERN a_source_line_modif_ptr
		source_line_modif_list;
			/* List of modifications to the current logical source
			   line to produce the fully preprocessed line.
			   Modifications are in no particular order, so
			   searching for something usually requires a linear
			   search from the start of the list.  However,
			   entries are moved to the front of the list when
			   they are referenced, so the searches are short.
			   NULL if there are no changes (a very common
			   case). */
EXTERN a_source_line_modif_ptr
		line_start_source_line_modif;
			/* Ordinarily NULL.  When non-NULL, points to
			   a source modification entry with line_loc == NULL,
			   which is an entry to be inserted before the first
			   character of curr_source_line (there is at most
			   one of these, and that only in rare cases).
			   The entry is also on the source_line_modif_list;
			   this pointer exists only to allow rapid
			   determination of whether or not there is such
			   an entry.  Used for re-insertion of macro name
			   identifiers for function-like macros when the
			   identifier is not followed by a "(". */
EXTERN a_source_line_modif_ptr
		avail_source_line_modifs;
			/* List of entries that have been allocated and freed
			   and are available for reuse. */
EXTERN unsigned long
		sequence_id_for_source_line_modifs;
			/* Used to generate sequence_id values as 
			   modifications are added; incremented each
			   time used.  This is useful so that all entries
			   generated after a certain "time" can be
			   easily found. */

/*
This flag is ordinarily NULL.  It is set non-NULL when a section of a line
is to be deleted because of preprocessing.  Specifically, the part of the
line from delete_source_from_loc to just before curr_char_loc is to
be deleted.  This flag is useful when scanning may cause reading of a new
logical source line.  Having the flag ensures that the deletion will be
done as the newline is processed, after which delete_source_from_loc
will be set to the start of the new line.
*/
EXTERN char	*delete_source_from_loc;

/*
Variables pertaining to the current token:
(These are the main interface between the lexical routines and the
rest of the compiler.)
See also the related variables in symbol_tbl.h.
If new variables are added here, be sure to put them also into
cache_curr_token, get_token_from_cached_token_rescan_list, and
get_token_from_reusable_cache_stack.
*/
EXTERN a_token_kind
		curr_token;
			/* The current token of input.  More precisely,
			   the one about to be processed. */
EXTERN char	*start_of_curr_token,
		*end_of_curr_token;
			/* The characters of the current token, in
			   curr_source_line.  Only valid outside the
			   lexical routines when raw pp tokens are being
			   fetched. */
EXTERN sizeof_t	len_of_curr_token;
			/* The length of the current token.  Only valid 
			   outside the lexical routines when raw pp tokens
			   are being fetched. */
EXTERN a_source_position
		pos_curr_token;
			/* The start position of the current token. */

#if EXTRA_SOURCE_POSITIONS_IN_IL
EXTERN a_source_position
		end_pos_curr_token;
			/* The end position of the current token -- that is,
			   the source position of the last character of the
			   token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

EXTERN int	kind_of_white_space_skipped;
			/* Kind of white space skipped by the most recent
			   call to skip_white_space (not necessarily
			   correct when get_token is called).  Used in
			   scanning macro definitions, where white space
			   can be significant.  0 = none, others as bits
			   as follows: */
#define WHITE_SPACE_COMMENTS 0x01
#define WHITE_SPACE_OTHER    0x02

EXTERN a_boolean
		comma_is_from_argument;
			/* Set to TRUE when an LE_COMMA_FROM_ARGUMENT is
			   seen by skip_white_space.  It is the responsibility
			   of the caller of skip_white_space to set it to
			   FALSE beforehand. */
EXTERN a_source_line_modif_ptr
		last_source_line_modif_exited_while_skipping_white_space;
			/* Set by skip_white_space whenever a source line
			   modification is exited. */
EXTERN a_constant
		const_for_curr_token;
			/* If the current token is a literal constant,
			   this is its value. */
EXTERN an_error_code
		err_code_for_error_token;
			/* If the current token is tok_error, this is the
			   error code that applies to the error.  This is
			   useful if the token was fetched with fetch_pp_tokens
			   TRUE, since no diagnostic was put out in that
			   case. */
EXTERN a_boolean
		curr_token_is_inert_macro;
			/* TRUE if the current token is an identifier that
			   is the name of a macro that was found within its
			   own expansion and therefore should not be expanded
			   again.  Not used in pcc preprocessing mode. */

EXTERN a_boolean
		any_initial_get_token_tests_needed;
			/* TRUE if a condition exists that requires some
			   special processing when get_token is called.
			   This is set when tokens are being rescanned
			   from a cache or when there are pragmas that
			   are associated with the current token. */

EXTERN a_boolean
		treat_newline_as_token;
			/* TRUE if ends-of-lines should be returned as
			   tok_newline instead of skipped as white space. */

EXTERN char	*curr_token_asm_string;
			/* When curr_token == tok_microsoft_asm, this points
			   to the associated asm string. */

EXTERN a_constant_ptr
		name_linkage_constants;
			/* A pointer to an array of constants representing
			   the name linkages that the front end accepts. */

#if ASM_SUPPORT_NEEDED

EXTERN a_boolean
		in_asm_function_body;
			/* TRUE if processing takes place during the scan of
			   an asm function body. */

EXTERN sizeof_t pos_in_asm_func_body_buffer;
			/* The number of characters that have been added to
			   asm_func_body_buffer thus far in processing. */

EXTERN char *asm_func_body_buffer;
			/* Pointer to a dynamically allocated buffer used to
			   construct the string representation of an asm
			   function or Microsoft asm block. */

EXTERN sizeof_t size_asm_func_body_buffer;
			/* The size of the asm buffer. */

EXTERN char *prev_asm_stop_char;
			/* The last character copied by the previous call to
			   copy_from_source_to_asm_func_buffer.  (Must be
			   EXTERN so it can be relocated if macro_buffer is
			   reallocated.) */


#if ASM_FUNCTION_ALLOWED
extern void copy_from_source_to_asm_func_buffer(char *stop_char,
                                                char *after_comment_stop_char);

extern void reset_asm_buffer(void);
#endif /* ASM_FUNCTION_ALLOWED */
#endif /* ASM_SUPPORT_NEEDED */

#if EXTRA_SOURCE_POSITIONS_IN_IL
EXTERN a_source_position
		curr_construct_end_position;
			/* Global variable used to return an end position
			   from the scanning of some construct.  Should be
			   retrieved and saved quickly, since it is set
			   by many scanning routines. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */


/*
The stop token array: If a syntactic error occurs, flush_tokens
will be called.  It will throw away tokens until it finds one for which
stop_token_array[curr_token] != 0 (i.e., a non-zero entry means the
corresponding token is in the set of stop tokens).  The entries are
actually counts; each time something is added to the stop set, an
entry is incremented, and each time something is removed, the entry is
decremented.  The entries are unsigned so that overflow need not be
checked for; in boundary cases, like an error in an expression with
256 levels of parentheses, one might flush a little further than usual,
but this is not a meaningful problem.
*/
typedef unsigned char
		a_token_set_array_element;
typedef a_token_set_array_element
		a_token_set_array[(int)tok_last+1];
			/* Generic array-of-unsigned-char both for global
			   variable stop_token_array and for local arrays
			   used in token caching. */

/*
A stack of stop token arrays is maintained.  The top of the stack is
pointed to by curr_stop_token_stack_entry.  A new stop token stack
entry is pushed when a new lexical context is entered (for example,
when a template instantiation is done) and popped when that context
is no longer needed and the previous context must be restored.
*/
typedef struct a_stop_token_stack_entry *a_stop_token_stack_entry_ptr;
typedef struct a_stop_token_stack_entry {
  a_stop_token_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack. */
  a_token_set_array
		stop_tokens;
			/* The set of tokens that will terminate a flush on
			   syntactic error.  A given token is in the set if
			   stop_token_array[token] != 0; */
} a_stop_token_stack_entry;

EXTERN a_stop_token_stack_entry_ptr
		curr_stop_token_stack_entry;
			/* Pointer to the current stop token stack entry. */

/*
Other general variables:
*/
EXTERN a_boolean
		is_id_char[CHAR_MAX-CHAR_MIN+1];
			/* For each character, whether or not it can be a
			   character after the first in an identifier.
			   Also used in scanning pp-numbers.  Note that
			   this covers only the identifier characters that
			   can be expressed in a single character; with
			   encodings like UTF-8, there may be other
			   identifier characters that take more than one
			   character.  If you need the full set of identifier
			   characters, see is_identifier_char. */
#if UNICODE_SOURCE_SUPPORTED
EXTERN a_boolean
		is_id_char_no_mbc[UCHAR_MAX+1];
			/* For each character, whether or not it can be a
			   character after the first in an identifier.
			   This is used to check characters after
			   multibyte characters have been converted to a
			   single character value.  For example, in UTF-8
			   the accented European characters of Latin-1
			   can be checked in this table once they have been
			   converted to a single-byte Unicode code point. */
#endif /* UNICODE_SOURCE_SUPPORTED */

/*
Structure used to record information about a pp token in a token cache.
*/
typedef struct a_pp_token_descr *a_pp_token_descr_ptr;
typedef struct a_pp_token_descr {
  char		*token_start;
			/* Pointer to the first character of the token. */
  char		*token_end;
			/* Pointer to the last character of the token.
			   The last character will be followed by a null
			   terminator. */
} a_pp_token_descr;


/*
Structure used to record information about a template body that has been
extracted from the enclosing cache.  This is used when creating template
strings so that the nested template body can be put out as part of the
template string for the enclosing template.
*/
typedef struct an_extracted_template_descr {
  a_symbol_ptr	symbol;
			/* The symbol associated with the extracted body. */
  a_byte_boolean
		semicolon_inserted;
			/* TRUE if the token with which this body is associated
			   is a semicolon that was inserted after the body
			   was removed. */
  a_cached_token_ptr
		next_in_token_string;
			/* This field is used only for extracted body entries
			   associated with friend functions whose bodies are
			   not actually removed from the token cache, but
			   should be skipped when creating a token string.
			   It is also used for the opposite purpose for
			   default arguments.  Default arguments are skipped
			   for normal processing, but are included when
			   generating template strings. */
} an_extracted_template_descr;


/*
Data structure used to save information about a token so that the token
can be cached and then rescanned.  Note that this is never done with
pp-tokens.  See cache_curr_token et al.
*/
enum a_token_extra_info_kind_tag {
  /* Kind of additional information saved in a cached token entry. */
  teik_none,		/* No extra information, i.e., normal token. */
  teik_identifier,	/* Extra information for an identifier. */
  teik_constant,	/* Extra information for a literal constant. */
  teik_pragma,		/* Extra information for a pragma. */
  teik_pp_token,        /* Extra information for a pp token. */
  teik_extracted_body,  /* Extra information for an extracted template body. */
  teik_asm_string,	/* Extra information for a Microsoft asm block. */
  teik_insert_string    /* Extra information for an inserted token string. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_token_extra_info_kind;
typedef struct a_cached_token {
  /* Information on a single token, saved for later rescanning of the
     token. */
  a_cached_token_ptr
		next;	/* Next cached token on the list, NULL if none. */
  a_source_position
		source_position;
			/* Source position of the token. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		end_source_position;
			/* The end position of the current token -- that is,
			   the source position of the last character of the
			   token. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_small_token_kind
		token;
			/* The token kind (e.g., tok_identifier).  Not valid
			   when extra_info_kind == teik_pragma. */
  a_token_extra_info_kind
		extra_info_kind;
			/* Indication of the type of extra information about
			   the token provided below. */
  a_token_sequence_number
		token_sequence_number;
			/* The sequence number associated with this token. */
  union {
    /* When extra_info_kind == teik_normal, no variant fields. */
    /* When extra_info_kind == teik_identifier: */
    a_symbol_locator
		locator;
			/* Symbol locator for the identifier. */
    /* When extra_info_kind == teik_constant: */
    a_constant_ptr
		constant;
			/* Pointer to a constant entry (in front end storage)
			   giving the value for the literal constant. */
    /* When extra_info_kind == teik_pragma: */
    struct a_pending_pragma
		*pragmas;
			/* A list of pragmas associated with the next token
			   in the cache. */
    /* When extra_info_kind == teik_pp_token: */
    a_pp_token_descr
		pp_token_descr;
			/* When a pp token is cached a copy of the string
			   that represents the token is saved as part of
			   the cache. */
    /* When extra_info_kind == teik_extracted_body: */
    an_extracted_template_descr
		extracted_template;
			/* When a template body is removed from a token
			   cache, the semicolon after the member declaration
			   is annotated with an extract template descriptor. */
    /* When extra_info_kind == teik_asm_string: */
    char	*asm_string;
			/* The string representing a Microsoft asm block. */
  } variant;
} a_cached_token;

/*
The token cache stack is used to manipulate persistent token caches
used for template instantiations.
*/
typedef struct a_reusable_cache_entry *a_reusable_cache_entry_ptr;
typedef struct a_reusable_cache_entry {
  a_reusable_cache_entry_ptr
                next;
                        /* Pointer to the next entry on the stack. */
  a_cached_token_ptr
                previous_token_rescan_list;
                        /* Contains a pointer to the token rescan list at
                           the time the new persistent cache was pushed
                           onto the stack. */
  a_cached_token_ptr
                next_cached_token;
                        /* Points to the next token in the persistent cache
                           to be rescanned. */
} a_reusable_cache_entry;

		

/* Initialize a token cache. */
extern void clear_token_cache(a_token_cache *cache,
			      a_boolean     reusable);
/* Discard the contents of a token cache. */
extern void discard_token_cache(a_token_cache *cache);
/* Save an end-of-source token in the token cache. */
extern void terminate_token_cache(a_token_cache *cache);
extern void remove_cache_terminator(a_token_cache *cache);
/* Create a token cache entry for a given token kind. */
extern
a_cached_token_ptr build_cached_token(a_token_kind	      kind,
                                      a_token_sequence_number sequence_number,
                                      a_source_position	      *position);
/* Save the current token in a token cache. */
extern void cache_curr_token(a_token_cache *cache);
/* Save a token stream in a token cache. */
extern void cache_token_stream(a_token_cache      *cache,
                               a_token_set_array  stop_tokens);
extern
void cache_token_stream_coalesce_identifiers(a_token_cache_ptr  cache,
                                             a_token_set_array  stop_tokens,
                                             a_token_cache_ptr	src_cache);

extern
void remove_token_from_cache(a_cached_token_ptr	ctp,
			     a_cached_token_ptr	*prev_ptr,
			     a_token_cache_ptr	cache);

extern a_token_kind get_token_to_be_cached(void);
extern
void cache_rest_of_declaration(a_token_cache_ptr	cache,
                               a_boolean		stop_on_colon,
                               a_boolean		stop_on_lbrace);
/* Put some cached tokens on the get_token rescan list. */
extern void rescan_cached_tokens(a_token_cache *cache);
/* Push a reusable cache on to the reusable cache stack. */
extern void rescan_reusable_cache(a_token_cache *cache);
/* Rescan a copy of a token cache. */
extern void rescan_copy_of_cache(a_token_cache *cache);

extern void free_tokens_from_reusable_cache(a_cached_token_ptr	ctp,
					    a_token_cache	*cache);

extern
void split_token_cache(a_token_cache	       *cache1,
                       a_token_cache	       *cache2,
                       a_token_sequence_number split_location,
                       a_boolean	       include_prev_token,
                       a_boolean	       okay_if_not_found);

/* Move a list of tokens from one cache to another. */
extern
void move_cached_tokens(a_cached_token_ptr	first_token,
			a_token_cache		*from_cache,
                        a_token_cache		*to_cache);


extern a_boolean same_string_ignoring_underscores(char  *s1, 
                                                  char  *s2);

/*
Variables to flag whether extra token separators should be emitted in
preprocessed (or raw listing) output to ensure that the output is correctly
tokenizable.  The second variable only applies to the next line of output
(after which it is reset to the value of the first variable).
*/
EXTERN a_boolean
		no_token_separators_in_pp_output;

EXTERN a_boolean
		no_token_separators_in_this_line_of_pp_output;

EXTERN a_preinclude_file_ptr
		next_preinclude_file;
			/* The next preinclude file to be processed in either
			   the normal or macro-only preinclude file list,
			   depending on the setting of
			   processing_macro_preincludes. */

EXTERN a_boolean
		processing_macro_preincludes;
			 /* TRUE when processing the list of macro-only
			    preincludes. */

EXTERN a_boolean
		next_token_is_top_level_decl_start;
			/* Flag toggled in translation_unit when advancing
			   past a token that marks the end of a "top-level"
			   declaration (i.e., a ";" or "}").  When this flag
			   is TRUE, the state of the compiler is in effect
			   between declarations -- or else just before the
			   first declaration or just after the last. */

/*
Data structure used in deciding where to put extra blanks to separate
adjacent tokens in textual preprocessing output.
*/
EXTERN a_byte	pp_lexical_category[CHAR_MAX-CHAR_MIN+1];
			/* For each character, the lexical category to
			   be used in preprocessing output.  These categories
			   are used to decide when extra token-separating
			   blanks must be inserted between tokens resulting
			   from macro expansion. */
#define PLC_SINGLETON 1
			/* A character in the singleton category always
			   stands alone as a token, and thus no extra blank
			   is ever required next to it for token separation. */
#define PLC_ID_OR_NUMBER 2
			/* Characters appearing in identifiers or pp-numbers
			   (see is_id_char). */
#define PLC_OTHER 3
			/* All other characters. */


/* Read next logical source line. */
a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file,
                                   a_boolean extend_current_line);
/* Check character as identifier character. */
extern a_boolean is_identifier_char(char      *ptr,
                                    int       *len,
                                    a_boolean is_identifier_start);
/* Check character as nonstandard. */
extern a_boolean is_nonstandard_character(char ch);
/* Skip white space. */
extern void skip_white_space(void);
/* Concatenate adjacent string literals in the current string constant. */
extern void concat_adjacent_string_literals(a_boolean function_name_case);
/* Get next token. */
extern a_token_kind get_token(void);
/* Generate a line-identifying directive in preprocessing output. */
extern void gen_pp_line_info(char      kind,
		             a_boolean next_line);
/* Generate textual preprocessing output for the current line. */
extern void gen_pp_output_for_curr_line(void);
/* Generate a line information record in the raw listing file. */
extern void gen_rlisting_line_info(char kind);
/* Generate raw listing output for the macro-expanded form of the current
   line. */
extern void gen_expanded_raw_listing_output_for_curr_line(
                                                   a_boolean do_inserted_text);
extern void finish_raw_listing_file(void);
extern void add_source_line_modif_to_hash_table(a_source_line_modif_ptr slmp);
extern void rem_source_line_modif_from_hash_table(
                                                a_source_line_modif_ptr slmp);
#if FULLY_RESOLVED_MACRO_POSITIONS
extern int compare_macro_text_map_entry_with_offset(
                                                   a_const_void_ptr offset_ptr,
                                                   a_const_void_ptr entry_ptr);
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
extern void add_concatenation_record(a_concatenation_record_ptr *headp,
                                     a_concatenation_record_ptr *tailp,
                                     char                       *line_loc,
                                     a_symbol_ptr               macro_sym);
/* Add an entry recording a logical modification to the source line. */
extern a_source_line_modif_ptr add_source_line_modif(
                          char                      *line_loc,
                          sizeof_t                  num_chars_to_delete,
                          char                      *inserted_text,
                          char                      *end_inserted_text);
/* Free a source line modification entry. */
extern void free_source_line_modif(a_source_line_modif_ptr *slmp);
/* Remove a source line modification entry from the source_line_modif_list. */
extern void rem_source_line_modif(a_source_line_modif_ptr slmp);
/* Find the source line modification entry associated with a given source
   location. */
extern a_source_line_modif_ptr assoc_source_line_modif(char *loc_in_line);
/* Find the parent modification of a given source line modification. */
#define parent_source_line_modif(slmp)                                \
  ((slmp)->parent_modif_determined ? (slmp)->parent_modif :           \
                                     f_parent_source_line_modif(slmp))
extern a_source_line_modif_ptr f_parent_source_line_modif(
                                                 a_source_line_modif_ptr slmp);
/* Set the parent pointer of ins_slmp to point to slmp. */
#define set_parent_modif(ins_slmp, slmp)                              \
{ (ins_slmp)->parent_modif = (slmp);                                  \
  (ins_slmp)->parent_modif_determined = TRUE;                         \
}  /* set_parent_modif */
/* Find the source line modification that affects the attention marker
   at a given source location. */
extern a_source_line_modif_ptr nested_source_line_modif(char *loc_in_line);
/* Convert a character location in the source line to a source sequence
   number and column. */
extern void conv_line_loc_to_source_pos(char              *loc_in_line,
                                        a_source_position *position_var);
/* Check for a specific token. */
extern a_boolean required_token(a_token_kind  token,
				an_error_code error_code);
extern a_boolean required_token_no_advance(a_token_kind  token,
                                           an_error_code error_code);
/* Check for a specific token, at the bottom of a loop for a repeated
   syntactic construct. */
extern a_boolean loop_token(a_token_kind token);
/* Look ahead at the token following the current one. */
extern a_token_kind next_token_with_seq_number(a_token_sequence_number *seq);
/* Macro that calls next_token_with_seq_number and provides a NULL argument. */
#define next_token()							\
  (next_token_with_seq_number((a_token_sequence_number*)NULL))

/* Look ahead at the next two tokens. */
extern
a_token_kind next_two_tokens(a_token_kind	first_token_must_be,
                             a_token_kind	*token_2);

/* Back up one token. */
extern void unget_token(void);

extern a_symbol_ptr coalesce_template_class_reference
			(a_symbol_ptr		   template_symbol,
			 an_identifier_options_set options,
			 a_boolean		   *err);

extern void begin_rescan_of_pragma_tokens(struct a_pending_pragma *ppp);

extern void wrapup_rescan_of_pragma_tokens(a_boolean          error_in_pragma);

extern a_boolean f_is_generalized_identifier_start
                     (an_identifier_options_set options,
                      a_type_ptr                field_sel_type);
extern a_boolean coalesce_and_lookup_qualified_name
                     (an_identifier_options_set        options,
		      an_identifier_lookup_mode	       ilm,
                      a_boolean			       *err);
extern a_symbol_ptr coalesce_and_lookup_generalized_identifier
                        (an_identifier_options_set        options,
                         an_identifier_lookup_mode        ilm,
                         a_boolean                        *err);

extern unsigned long scan_universal_character(
					char		**start_pos,
					a_boolean	is_identifier,
				        a_boolean	is_identifier_start,
					a_boolean	issue_diagnostics);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean check_context_sensitive_keyword(a_token_kind  tok_kind,
                                                 char          *tok_str);

extern a_boolean accum_quoted_string(unsigned long     *num_chars,
                                     a_boolean         is_header_name,
                                     a_character_kind  character_kind,
                                     char              quoting_char);

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
extern void setlocale_pragma(a_pending_pragma_ptr	ppp);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */

/* Macro that tests whether f_is_generalized_identifier_start needs
   to be called.  We don't need to call it if we have an identifier that
   has already been coalesced.  There are other cases that could be
   eliminated such as tok_ptr_to_member (which returns FALSE) and current
   tokens that are not things that could start an identifier.  These have
   smaller payoffs so are not currently included. */
#define is_generalized_identifier_start(options)			\
  /* if */ ((curr_token == tok_identifier &&				\
            locator_for_curr_id.has_been_coalesced) /* { */ ?		\
    TRUE								\
  /* } else { */ :							\
    f_is_generalized_identifier_start(options, (a_type_ptr)NULL))	\
  /* } */								\


/* Return TRUE if the current token is the start of a C++ qualified
   name (including a simple identifier).  This version of the macro is
   used in expression contexts in which a "<" should be considered a
   less than sign and not the start of a template argument list. */
#define is_expr_qualified_name_start()					  \
 (is_generalized_identifier_start(GID_IS_EXPR_CONTEXT))

/* Return TRUE if the current token is the start of a C++ qualified
   name (including a simple identifier).  This version of the macro is
   used in declaration contexts in which a "<" should be considered to
   be the start of a template argument list. */
#define is_decl_qualified_name_start()					  \
 (is_generalized_identifier_start(GID_NO_OPTIONS))

/*
Return TRUE if the indicated token is a type qualifier.  This is
straightforward for const, volatile, and restrict but __unaligned is
valid only with certain configurations.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
/*lint -save -e773*/
#define or_is_unaligned_token(tok) || (tok) == tok_unaligned
/*lint -restore*/
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_unaligned_token(tok)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/*
Unified Parallel C adds several new type qualifiers.
*/
#if UPC_EXTENSIONS_ALLOWED
/*lint -save -e773*/
#define or_is_upc_qual_token(tok)                                             \
  || (tok) == tok_upc_shared                                                  \
  || (tok) == tok_upc_strict || (tok) == tok_upc_relaxed
#else /* !UPC_EXTENSIONS_ALLOWED */
#define or_is_upc_qual_token(tok) /* Nothing */
#endif /* UPC_EXTENSIONS_ALLOWED */

/*
Return TRUE if the current token is the standard "restrict" token or the
equivalent GNU/Microsoft "__restrict" token.  (Distinct token kinds are used
because uses of the nonstandard spelling are warned about in some modes.)
*/
#define is_restrict_token(tok)                                                \
  ((tok) == tok_restrict || (tok) == tok_gnu_restrict)

#define is_type_qualifier_token(tok)                                          \
  ((tok) == tok_const || (tok) == tok_volatile || is_restrict_token((tok))    \
   or_is_unaligned_token(tok)                                                 \
   or_is_upc_qual_token(tok))


extern void push_next_preinclude_file(void);

/* Push a file onto the input stack. */
extern void open_file_and_push_input_stack(char      *file_name,
                                           a_boolean use_search_path,
                                           a_boolean is_include_file,
                                           a_boolean is_system_include,
                                           a_boolean is_preinclude,
					   a_boolean preinclude_macros,
                                           a_boolean is_implicit_include,
                                           a_boolean is_include_next,
					   a_boolean continue_on_open_failure);

extern a_boolean open_file_for_input(
		char				*file_name,
		a_boolean			use_search_path,
		a_boolean			is_include_file,
		a_boolean			is_system_include,
		a_boolean			is_include_next,
		a_boolean			is_implicit_include,
		a_boolean			continue_on_open_failure,
		char				**full_file_name,
		char				**display_name,
		FILE				**new_input_file,
		a_boolean			*suppress_include,
		a_unicode_source_kind		*unicode_source_kind,
		a_directory_name_entry_ptr	*dir_entry);
extern void push_input_stack(
			FILE			    *new_input_file,
                        char			    *name_as_written,
                        char			    *display_name,
                        char     		    *full_file_name,
			a_boolean                   is_include_file,
			a_boolean                   is_system_include,
                        a_boolean                   is_preinclude,
			a_boolean		    preinclude_macros,
                        a_boolean                   is_implicit_include,
                        a_unicode_source_kind       unicode_source_kind,
                        a_directory_name_entry_ptr  dir_entry,
			an_include_file_history_ptr ifhp);

extern void pop_input_stack(void);

extern void ensure_min_curr_source_line_length(sizeof_t  min_len);

extern a_boolean processing_primary_source_file(void);

extern void check_for_generation_of_pch_on_return_to_primary_file(void);

extern a_boolean cache_function_body(
				a_token_cache		*p_token_cache,
				a_boolean		is_constructor,
				a_boolean		*missing_end,
				a_token_sequence_number	*first_tsn,
				a_token_sequence_number	*last_tsn,
				a_source_position	*start_pos,
				a_source_position	*end_pos);

/* Set the error position to the current token position. */
#define set_err_pos_to_curr_token()                                   \
{ copy_source_position(pos_curr_token, error_position);}

/* Define primitive operations on a_token_set_array. */
#define clear_token_set_array(array) memzero((char *)(array), sizeof(array))
#define incr_token_set_array_element(array, tok) (array)[(int)(tok)]++
#define decr_token_set_array_element(array, tok) (array)[(int)(tok)]--

/* Clear the set of syntax error flush stop tokens. */
#define clear_stop_tokens() \
  clear_token_set_array(curr_stop_token_stack_entry->stop_tokens)
/* Add a token to the set of syntax error flush stop tokens. */
#define add_stop_token(stop_token)                                    \
  incr_token_set_array_element(curr_stop_token_stack_entry->stop_tokens, \
			       stop_token)
/* Remove a token from the set of syntax error flush stop tokens. */
#define remove_stop_token(stop_token)                                 \
  decr_token_set_array_element(curr_stop_token_stack_entry->stop_tokens, \
			       stop_token)
/* Flush to the token that matches an opening token (e.g., parenthesis). */
extern void flush_until_matching_token(void);
/* Flush tokens on error, to a token in the stop token set. */
extern void flush_tokens_with_stop_tokens_and_warning_flag(
				a_token_set_array	stop_tokens,
				a_boolean		suppress_warning);
extern void flush_to_closing_paren(void);
extern void flush_tokens(void);
extern void flush_tokens_without_warning(void);
extern void flush_to_end_of_arg_list(void);

/*
Flush to the newline at the end of the current preprocessing directive.
End-of-source is also checked for because it can come up in some error
cases.
*/
#define flush_to_newline()                                            \
{ while (curr_token != tok_newline &&                                 \
         curr_token != tok_end_of_source) (void)get_token();}


extern void push_stop_token_stack(void);
extern void pop_stop_token_stack(void);
extern a_template_ptr scan_template_template_argument(
				a_template_ptr		param_template,
				a_source_position	*err_pos);

extern void insert_string_into_token_stream(char	*string,
					    a_boolean	insert_after);

#if CHECKING
void check_all_stop_token_entries_are_reset(a_token_set_array stop_tokens);
#endif /* CHECKING */

/* Initialize the name linkage constants. */
void init_name_linkage_constants(void);

/* Initialize the lexical routines. */
extern void lexical_reset(void);
extern void lexical_one_time_init(void);
extern void lexical_trans_unit_init(void);
extern void lexical_init(void);
extern void lexical_trans_unit_wrapup(void);
#if MAKE_FRONT_END_CALLABLE
extern void lexical_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

/* Flush until the tok_end_of_source terminating a token cache is found. */
#define flush_past_token_cache_terminator()			\
  {								\
    while (curr_token != tok_end_of_source) (void)get_token();	\
    /* Advance past the end-of-source token. */			\
    (void)get_token();						\
  }

extern
void add_token_cache_segment_to_string(a_token_cache_ptr	cache,
				       a_token_sequence_number	start_tsn,
				       a_token_sequence_number	end_tsn);

extern void add_token_cache_to_string(a_token_cache_ptr	cache);

extern void init_token_string(a_source_position *pos,
                              a_boolean         keep_spacing);

extern char *make_copy_of_token_string(void);

extern a_preinclude_file_ptr alloc_preinclude_file(void);

#if RECORD_FORM_OF_NAME_REFERENCE
extern a_name_reference_ptr make_name_reference(
					a_symbol_locator	*locator,
					a_source_correspondence	*scp);
extern void make_name_reference_from_locator(
				      a_symbol_locator		*locator,
				      a_name_reference_ptr	nrp);
extern a_name_reference_ptr find_allocated_name_reference(
				a_source_correspondence		*scp,
				a_name_reference_ptr		entry_to_copy);
extern void db_name_qualifier(a_name_qualifier_ptr	nqp);
extern void db_name_reference(a_name_reference_ptr	nrp);

/*
Convenience macro to avoid calling make_name_reference for entities that are
known not to have a qualified name.  This includes all entities in C mode,
and all entities declared in functions.
*/
#define qualifiable_name_reference(loc, scp)                                  \
  ((C_mode() || !in_file_scope(scp)) ? (a_name_reference_ptr)NULL             \
                                     : make_name_reference((loc), (scp)))
#endif /* RECORD_FORM_OF_NAME_REFERENCE */

#if DEBUG
/* Show space used in the lexical routines, for debugging purposes. */
extern unsigned long show_lexical_space_used(void);

extern void db_token_cache(a_token_cache *cache,
                           char		 *cache_name);

extern void db_stop_tokens(void);
#endif /* DEBUG */

/* Test whether or not a given character location falls within
   curr_source_line. */
/* Watch out -- the argument is evaluated more than once. */
#define within_curr_source_line(line_loc) \
  ptr_in_range((line_loc), curr_source_line, after_end_of_curr_source_line)

/*
loc_in_line points to an attention marker indicating the start of
an insertion.  Update loc_in_line to point to the first character in the
insertion.  If the insertion is really a deletion, update loc_in_line
to after the deleted character.  slmp is set to point to the source
line modification for the insertion.
If you change this, also change walk_into_insertion.
*/
#define go_into_insertion(slmp, loc_in_line)                          \
{ (slmp) = nested_source_line_modif(loc_in_line);                     \
  if ((slmp)->inserted_text != (slmp)->end_inserted_text) {           \
    /* Text replacement; continue with the text in the expansion. */  \
    (loc_in_line) = (slmp)->inserted_text;                            \
  } else {                                                            \
    /* Text deletion; skip over the deleted text. */                  \
    (loc_in_line) += (slmp)->num_chars_to_delete;                     \
  }  /* if */                                                         \
}  /* go_into_insertion */

/*
Similar to go_into_insertion, but for cases where one maintains a pointer
to the modification entry for the current position, as when one is
walking the entire line modification tree.  slmp on entry points to the
current modification entry, and it is updated on exit.  loc_in_line points
to an attention marker indicating the start of an insertion.  loc_in_line
is updated to point to the first character in the insertion.  If the
insertion is really a deletion, loc_in_line is updated to after the
deleted character.  ins_slmp is set to point to the source line
modification for the insertion.  The parent pointer in the insertion
entry is set, so exiting from the insertion will be quick.
If you change this, also change go_into_insertion.
*/
#define walk_into_insertion(slmp, ins_slmp, loc_in_line)              \
{ (ins_slmp) = nested_source_line_modif(loc_in_line);                 \
  set_parent_modif(ins_slmp, slmp);                                   \
  if ((ins_slmp)->inserted_text != (ins_slmp)->end_inserted_text) {   \
    /* Text replacement; continue with the text in the expansion. */  \
    (loc_in_line) = (ins_slmp)->inserted_text;                        \
    (slmp) = (ins_slmp);                                              \
  } else {                                                            \
    /* Text deletion; skip over the deleted text. */                  \
    (loc_in_line) += (ins_slmp)->num_chars_to_delete;                 \
  }  /* if */                                                         \
}  /* walk_into_insertion */

/*
slmp points to a source modification.  Set loc_in_line to the character
location that follows the characters replaced by that modification.
*/
#define leave_insertion(slmp, loc_in_line)                            \
{ (loc_in_line) = loc_of_insert(slmp) + (slmp)->num_chars_to_delete;  \
}  /* leave_insertion */

/*
slmp points to a source modification.  Set loc_in_line to the character
location that follows the characters replaced by that modification.  Update
slmp to point to the modification associated with loc_in_line (i.e.,
the parent of the original slmp).
*/
#define walk_out_of_insertion(slmp, loc_in_line)                      \
{ leave_insertion(slmp, loc_in_line)                                  \
  slmp = parent_source_line_modif(slmp);                              \
}  /* walk_out_of_insertion */

/*
Determine the insert location for a source line modification.  This is
tricky for an entry that is inserted in front of the first character
of curr_source_line.
*/
#define loc_of_insert(slmp)                                           \
  ((slmp)->line_loc != NULL ? (slmp)->line_loc : curr_source_line)

/*
Set loc_in_line to point to the first character of the current source
line.  This is tricky when there is an insert at the start of the line.
Also set slmp for use in scanning the line with the "walk_.." macros.
*/
#define set_up_for_walk_of_source_line(loc_in_line, slmp)             \
{ if (line_start_source_line_modif != NULL) {                         \
    slmp = line_start_source_line_modif;                              \
    loc_in_line = slmp->inserted_text;                                \
  } else {                                                            \
    slmp = NULL;                                                      \
    loc_in_line = curr_source_line;                                   \
  }  /* if */                                                         \
}  /* set_up_for_walk_of_source_line */

/*
Convert a character hex digit to the associated hex digit value.
*/
#define hexvalue(ch) ((ch) - (isdigit((unsigned char)ch) ? '0' : \
                             (islower((unsigned char)ch) ? 'a'-0xa : 'A'-0xA)))

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
/*
Macro used in locations where a source sequence entry can be created for
an __if_exists directive.
*/
#define check_for_if_exists_pragmas()					\
  if (curr_token_pragmas != NULL) f_check_for_if_exists_pragmas()
/*
Macro used to determine whether a source sequence entry should be created
for a given __if_exists directive.
*/
#define generate_microsoft_if_exists_entries()				\
  (create_microsoft_if_exists_entries &&				\
   prototype_instantiations_in_il &&					\
   depth_scope_stack != NO_SCOPE_DEPTH &&				\
   scope_stack[depth_scope_stack].create_ms_if_exists_entries)
#else /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */
#define check_for_if_exists_pragmas() /* nothing */
#endif /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern void if_exists_pragma(a_pending_pragma_ptr	ppp);
extern void f_check_for_if_exists_pragmas(void);
extern void check_for_unclosed_if_exists_blocks(void);
#endif /* !GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#endif /* ifndef LEXICAL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
