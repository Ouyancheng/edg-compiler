/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
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
typedef struct a_token_cache *a_token_cache_ptr;
typedef struct a_cached_token *a_cached_token_ptr;

typedef struct a_symbol_header a_symbol_header_dummy_typedef;

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef ERROR_H
#include "error.h"
#endif /* ifndef ERROR_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


/* There are more #includes later in this file. */

/*
A sequence number is assigned to each token fetched from the input.
This is the type used to represent the sequence number.
*/
typedef uint32_t a_token_sequence_number;

EXTERN a_token_sequence_number
		curr_token_sequence_number;
			/* The sequence number associated with the
			   current token.  A token retains its sequence
			   number even when saved and restored from a
			   token cache.  For a coalesced token this is the
			   token sequence number of the initial token. */

EXTERN a_token_sequence_number
		last_token_sequence_number_of_token;
			/* The sequence number associated with the end of
			   the current token.  This is normally the same as
			   curr_token_sequence_number, but for coalesced
			   identifiers refers to a later token. */

EXTERN a_token_sequence_number
		last_token_sequence_number_used;
			/* The counter used to assign token sequence
			   numbers. */

#define NO_TOKEN_SEQUENCE_NUMBER (0)
			/* The value used to indicate that no sequence number
			   is present. */

/*
A type used to represent a token from a reusable cache.
*/
typedef a_cached_token_ptr a_cached_token_handle;

EXTERN a_cached_token_handle
		curr_cached_token_handle;
			/* If the current token originated from a reusable
			   token cache, this is a handle that can be used
			   to access that token from the reusable cache.
			   It will be NO_CACHED_TOKEN_HANDLE if the token did
			   not originate from a reusable cache. */

#define NO_CACHED_TOKEN_HANDLE ((a_cached_token_handle)NULL)

/* These declarations are placed here so that they will be defined before
   symbol_tbl.h is included. */


/* Flags used to specify how identifiers are to be scanned by the
   generalized identifier routines.  is_generalized_identifier_start
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
#define GID_IS_TAG_NAME    0x200000
			/* TRUE when scanning a tag name. */
#define GID_SIMPLIFY_CURR_CLASS_QUALIFIED_NAME 0x400000
			/* TRUE if a qualified name of the form Q::X (with no
			   leading "::") should be treated as just X if Q
			   denotes the class currently being defined.
			   Microsoft compilers in particular appear to behave
			   this way. */
#define GID_IS_UNKNOWN_TEMPLATE_ARG    0x800000
			/* TRUE when scanning an identifier at the beginning
			   of a template argument in an unknown template
			   argument list context. */
#define GID_IS_BASE_CLASS 0x1000000
			/* TRUE when scanning a base class specifier. */
#define GID_ERROR_FLAGS (GID_DISALLOW_QUALIFIED_NAME |		\
			 GID_DISALLOW_GLOBAL_QUALIFIER |	\
			 GID_DISALLOW_OPERATOR_NAME)
			/* Contains all the flags used by the routine
			   check_for_generalized_identifier_errors.
			   Is used to mask the error flags so that errors
			   are only reported once. */
#define GID_CHECK_TAG_NAME_FLAGS (GID_TEMPLATE_ARGS_OPTIONAL |   \
                                  GID_IMPLICIT_TYPE_CONTEXT |    \
                                  GID_IS_TAG_NAME)
			/* Contains the flags used to check for a tag name. */

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
  ilm_declarator,	/* Uses IDL_IS_DECLARATOR. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  ilm_static_declarator,
			/* Uses both IDL_IS_DECLARATOR and
			   IDL_IS_STATIC_DECL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  ilm_template_friend,	/* Uses IDL_FRIEND_LOOKUP to do the lookup, create
			   nonreal members as templates. */
  ilm_template_tag,	/* Uses find tag names and find members of the
			   prototype instantiation, not nonreal members. */
  ilm_last
} an_identifier_lookup_mode;


typedef struct a_token_cache {
  /* Data structure used to hold a token cache, i.e., some number of
     tokens that are being saved for later rescanning. */
  a_token_cache_ptr
		next;
			/* Pointer to the next entry on the available list
			   of freed entries. */
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
= {
   (an_opname_kind)onk_none,          /* tok_error */
   (an_opname_kind)onk_none,          /* tok_identifier */
   (an_opname_kind)onk_none,          /* tok_float_constant */
   (an_opname_kind)onk_none,          /* tok_fixed_point_constant */
   (an_opname_kind)onk_none,          /* tok_int_constant */
   (an_opname_kind)onk_none,          /* tok_char_constant */
   (an_opname_kind)onk_none,          /* tok_string_literal */
   (an_opname_kind)onk_none,          /* tok_ud_literal */
   (an_opname_kind)onk_none,          /* tok_end_of_source */
   (an_opname_kind)onk_none,          /* tok_newline */
   (an_opname_kind)onk_none,          /* tok_header_name */
   (an_opname_kind)onk_none,          /* tok_pp_number */
   (an_opname_kind)onk_none,          /* tok_digit_sequence */
   (an_opname_kind)onk_none,          /* tok_cpp_quote */
   (an_opname_kind)onk_none,          /* tok_ptr_to_member */
   (an_opname_kind)onk_none,          /* tok_removed_default_arg */
   (an_opname_kind)onk_none,          /* tok_removed_template_body */
#if MICROSOFT_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_cli_typeid */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_decltype_construct */
   (an_opname_kind)onk_none,          /* tok_unimplemented */
   (an_opname_kind)onk_subscript,     /* operator[] starts with tok_lbracket */
   (an_opname_kind)onk_none,          /* tok_rbracket */
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
   (an_opname_kind)onk_none,          /* tok_c99_generic */
   (an_opname_kind)onk_none,          /* tok_c99_genericfx */
   (an_opname_kind)onk_none,          /* tok_ext_alignof */
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
   (an_opname_kind)onk_none,          /* tok_char16_t */
   (an_opname_kind)onk_none,          /* tok_char32_t */
   (an_opname_kind)onk_none,          /* tok_fract */
   (an_opname_kind)onk_none,          /* tok_accum */
   (an_opname_kind)onk_none,          /* tok_sat */
   (an_opname_kind)onk_none,          /* tok_declspec */
#if MICROSOFT_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_abstract */
   (an_opname_kind)onk_none,          /* tok_sealed */
   (an_opname_kind)onk_none,          /* tok_cdecl */
   (an_opname_kind)onk_none,          /* tok_fastcall */
   (an_opname_kind)onk_none,          /* tok_stdcall */
   (an_opname_kind)onk_none,          /* tok_thiscall */
   (an_opname_kind)onk_none,          /* tok_vectorcall */
   (an_opname_kind)onk_none,          /* tok_clrcall */
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
   (an_opname_kind)onk_none,          /* tok_uuid */
   (an_opname_kind)onk_none,          /* tok_in */
   (an_opname_kind)onk_none,          /* tok_gcnew */
   (an_opname_kind)onk_none,          /* tok_safe_cast */
   (an_opname_kind)onk_none,          /* tok_implements */
   (an_opname_kind)onk_none,          /* tok_unresolved_type */
   (an_opname_kind)onk_none,          /* tok_for_each */
   (an_opname_kind)onk_none,          /* tok_ref_class */
   (an_opname_kind)onk_none,          /* tok_ref_struct */
   (an_opname_kind)onk_none,          /* tok_value_class */
   (an_opname_kind)onk_none,          /* tok_value_struct */
   (an_opname_kind)onk_none,          /* tok_enum_class */
   (an_opname_kind)onk_none,          /* tok_enum_struct */
   (an_opname_kind)onk_none,          /* tok_interface_class */
   (an_opname_kind)onk_none,          /* tok_interface_struct */
   (an_opname_kind)onk_none,          /* tok_ref_new */
   (an_opname_kind)onk_none,          /* tok_partial_ref_class */
   (an_opname_kind)onk_none,          /* tok_partial_ref_struct */
   (an_opname_kind)onk_none,          /* tok_prefix_ref */
   (an_opname_kind)onk_none,          /* tok_prefix_value */
   (an_opname_kind)onk_none,          /* tok_prefix_interface */
   (an_opname_kind)onk_none,          /* tok_prefix_for */
   (an_opname_kind)onk_none,          /* tok_prefix_enum */
   (an_opname_kind)onk_none,          /* tok_prefix_partial */
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
   (an_opname_kind)onk_none,          /* tok_attribute */
#if GNU_EXTENSIONS_ALLOWED
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
   (an_opname_kind)onk_none,          /* tok_thread_local */
   (an_opname_kind)onk_none,          /* tok_c11_thread_local */
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
   (an_opname_kind)onk_none,          /* tok_is_trivial */
   (an_opname_kind)onk_none,          /* tok_is_standard_layout */
   (an_opname_kind)onk_none,          /* tok_is_trivially_copyable */
   (an_opname_kind)onk_none,          /* tok_is_literal_type */
   (an_opname_kind)onk_none,          /* tok_has_trivial_move_constructor */
   (an_opname_kind)onk_none,          /* tok_has_trivial_move_assign */
   (an_opname_kind)onk_none,          /* tok_has_nothrow_move_assign */
   (an_opname_kind)onk_none,          /* tok_is_constructible */
   (an_opname_kind)onk_none,          /* tok_is_nothrow_constructible */
   (an_opname_kind)onk_none,          /* tok_is_trivially_constructible */
   (an_opname_kind)onk_none,          /* tok_is_destructible */
   (an_opname_kind)onk_none,          /* tok_is_nothrow_destructible */
   (an_opname_kind)onk_none,          /* tok_is_trivially_destructible */
   (an_opname_kind)onk_none,          /* tok_is_nothrow_assignable */
   (an_opname_kind)onk_none,          /* tok_is_trivially_assignable */
   (an_opname_kind)onk_none,          /* tok_is_valid_winrt_type */
   (an_opname_kind)onk_none,          /* tok_underlying_type */
#if MICROSOFT_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_has_finalizer */
   (an_opname_kind)onk_none,          /* tok_is_delegate */
   (an_opname_kind)onk_none,          /* tok_is_interface_class */
   (an_opname_kind)onk_none,          /* tok_is_ref_array */
   (an_opname_kind)onk_none,          /* tok_is_ref_class */
   (an_opname_kind)onk_none,          /* tok_is_sealed */
   (an_opname_kind)onk_none,          /* tok_is_simple_value_class */
   (an_opname_kind)onk_none,          /* tok_is_value_class */
   (an_opname_kind)onk_none,          /* tok_is_win_class */
   (an_opname_kind)onk_none,          /* tok_is_win_interface */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED*/
   (an_opname_kind)onk_none,          /* tok_nullptr */
#if MICROSOFT_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_native_nullptr */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_internal_alias_decl */
#if INT128_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_int128 */
#endif /* INT128_EXTENSIONS_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_override */
   (an_opname_kind)onk_none,          /* tok_final */
   (an_opname_kind)onk_none,          /* tok_is_final */
   (an_opname_kind)onk_none,          /* tok_noexcept */
   (an_opname_kind)onk_none,          /* tok_constexpr */
   (an_opname_kind)onk_none,          /* tok_alignof */
   (an_opname_kind)onk_none,          /* tok_alignas */
#if GNU_EXTENSIONS_ALLOWED
   (an_opname_kind)onk_none,          /* tok_bases */
   (an_opname_kind)onk_none,          /* tok_direct_bases */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_VECTOR_TYPES_ALLOWED
   (an_opname_kind)onk_none,           /* tok_builtin_shuffle */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
   (an_opname_kind)onk_none,          /* tok_noreturn */
   (an_opname_kind)onk_none,          /* tok_builtin_complex */
   (an_opname_kind)onk_none,          /* tok_c11_generic */
   (an_opname_kind)onk_last           /* tok_last */
  }
#endif /* VAR_INITIALIZERS */
;
/*
Array giving the token name for each opname kind.
*/
EXTERN a_const_char
		*opname_names[(int)onk_last];


/*
Structure used to build a list of files to be pre-included.
*/
typedef struct a_preinclude_file *a_preinclude_file_ptr;
typedef struct a_preinclude_file {
  a_preinclude_file_ptr
		next;	/* Pointer to the next entry in a linked list of
			   preinclude files. */
  a_const_char	*file_name;
			/* Name of the file to be preincluded. */
} a_preinclude_file;

/*
Structure used to record information about files that have been included.
See the comment preceding find_include_history in lexical.c.
*/
typedef struct an_include_file_history *an_include_file_history_ptr;
typedef struct an_include_file_history {
  a_const_char  *full_name;
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
  a_bit_field	use_canonical_name:1;
			/* This is used to pass a flag into the
			   compare_include_file_history routine to indicate
			   whether the file name comparison should use the
			   canonical form of the file name. */
  a_const_char  *controlling_macro_name;
			/* The name of the macro used to guard the include
			   file against multiple inclusions. */
#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
  a_unique_file_id
		unique_id;
			/* Host-dependent information that uniquely
			   identifies a file.  This is used when two different
			   names can refer to the same file (i.e., as a
			   result of symbolic or hard links to the file or
			   directories containing the file). */
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */
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
				 a_const_char                 *full_name,
				 an_include_file_history_ptr *ifhp_ptr,
				 a_boolean		     create,
			         a_boolean		     use_canonical);

extern
a_boolean find_include_history(a_const_char                *full_name,
			       an_include_file_history_ptr *ifhp_ptr,
			       a_boolean		   create,
			       a_boolean		   use_canonical);

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
  a_const_char	*file_name;
			/* The form of the file name to be used in error
			   messages and the like, null-terminated.
			   May have been modified by a #line directive. */
  a_const_char	*full_name;
			/* The form of the file name to be used in opening the
			   file (may have directory).  Null-terminated.
			   Can be the same as file_name. */
  a_const_char	*dir_name;
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
#if ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR
  a_bit_field	prev_line_terminator_was_carriage_return:1;
			/* TRUE if the most recent line read from this file
			   ended with a carriage return.  This allows
			   treating a carriage return followed by a newline
			   as a single line terminator instead of the
			   newline being treated as the end of an empty
			   line. */
#endif /* ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR */
  a_bit_field	cloned_for_line_directive:1;
			/* TRUE if this entry was created to track #line
			   directives by cloning the previous top of the
			   input stack. */
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
#if RECORD_MACRO_INVOCATIONS
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
#endif /* RECORD_MACRO_INVOCATIONS */
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
location occupied by the right-hand operand of ##, or if the combined token
ends before the right-hand token previously did, the concatenation did not
form a valid token.

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
EXTERN a_const_char
		*curr_source_line;
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
EXTERN a_const_char
		*after_end_of_curr_source_line;
			/* Address past the last element of curr_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because curr_source_line line can reallocated larger
			   if needed. */
#if MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
EXTERN a_const_char
		**logical_char_info;
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
EXTERN sizeof_t	end_of_line_escape_offset;
			/* The offset of the LE_END_OF_LINE escape that
			   ends curr_source_line.  This is set by
			   skip_white_space and is used only in the rare
			   case when a line that logically begins with a
			   line_start_source_line_modif is the last line of
			   a file.  As noted in the preceding comment for
			   at_end_of_source_file, the line contents at that
			   point are still the last line before the EOF was
			   encountered, so this variable allows the
			   line_start_source_line_modif to be treated as if
			   it occurred just before the terminating
			   LE_END_OF_LINE instead of the beginning of the
			   line to avoid processing the line's contents
			   twice. */

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
EXTERN a_const_char
		*curr_char_loc;
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
#define LE_TEMPORARILY_INERT_MACRO 8
			/* Like LE_INERT_MACRO, except that it is removed
			   by expand_top_level_pcc_macro.  It is used to
			   support Microsoft-mode macros in which the left
			   parenthesis of a macro invocation appears in a
			   macro expansion in a macro argument and the
			   corresponding right parenthesis appears in the
			   text following the outermost macro invocation
			   (which violates the Standard requirement that
			   macro arguments are expanded in isolation).  A
			   macro with a "missing" right parenthesis is
			   marked as temporarily inert so it will only be
			   expanded after the expansion of the top-level
			   macro invocation is complete. */
#define LE_END_OF_TOP_LEVEL_EXPANSION 9
			/* Indicates the end of the expansion of a macro
			   whose invocation occurs on a source line, i.e.,
			   not in the expansion of another macro.  Used
			   only when FULLY_RESOLVED_MACRO_POSITIONS is
			   FALSE as part of an optimization to speed up
			   calculation of source positions. */

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
  a_const_char	*line_loc;
			/* The location in curr_source_line of the
			   modification.  Points to the character that
			   resulted from the trigraph, or the character
			   following the line splice. */
  an_orig_line_modif_kind
		kind;
			/* Kind of modification: trigraph or line splice. */
  a_byte_boolean
		in_raw_string_literal;
			/* TRUE if this modification occurs within a raw
			   string literal, FALSE otherwise. */
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
  a_const_char	*line_loc;
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
  sizeof_t	num_deleted_chars;
			/* The number of characters in inserted_text that
			   have been replaced by a nested source line
			   modification (less one for the ATTENTION_MARKER)
			   and thus would be reclaimed if the macro buffer
			   were compacted. */
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
  a_bit_field	is_whitespace_kwd:1;
			/* TRUE if this modification represents the canonical
			   form of a whitespace keyword or is a line-start
			   modification that reinserts the first token of
			   a failed multi-line whitespace keyword, i.e., a
			   token that might have begun a whitespace keyword
			   but the token that followed it (on a succeeding
			   line) did not complete the keyword. */
  char		orig_char;
			/* The character that was in the source line at
			   position line_loc (provided so that the original
			   line can be reconstructed; not needed otherwise).
			   Meaningless when line_loc == NULL. */
  char		inserted_chars[3];
			/* Place for an insert string of up to three characters
			   including the end-of-insertion lexical escape.
			   Used for the blank that replaces comments. */
  a_const_char	*inserted_text;
			/* Pointer to text to be inserted, in macro_buffer.
			   The text is terminated by an LE_END_OF_INSERTION
			   lexical escape.  Points to a zero-length string
			   if this is a deletion only. */
  a_const_char	*end_inserted_text;
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
  a_const_char	*text_from_primary_source_line;
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
#if RECORD_MACRO_INVOCATIONS
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
#endif /* RECORD_MACRO_INVOCATIONS */
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
EXTERN a_const_char
		*delete_source_from_loc;

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
EXTERN a_const_char
		*start_of_curr_token,
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
EXTERN a_symbol_ptr
		ud_lit_op_sym_for_curr_token;
			/* If the current token is a user-defined literal
			   (tok_ud_literal), this is the symbol for its
			   literal operator or literal operator template,
			   if known.  May be NULL or designate an
			   overloaded function in error cases or if the
			   user-defined literal is a string literal
			   appearing in the declaration of a literal
			   operator or literal operator template;
			   otherwise, it designates the specific function
			   or template to be used to produce the value of
			   this literal. */
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
		curr_token_is_temporarily_inert_macro;
			/* TRUE when curr_token_is_inert_macro is TRUE and
			   the lexical escape indicating inertness is
			   LE_TEMPORARILY_INERT_MACRO instead of
			   LE_INERT_MACRO. */

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

EXTERN a_boolean
		id_contains_ucn_or_multibyte_char;
			/* When curr_token is tok_identifier, TRUE if the
			   the identifier was written using a
			   universal-character-name or a multibyte
			   character, FALSE otherwise.  This is only set
			   upon the initial scan of an identifier from
			   source text, not, e.g., when the identifier
			   comes from a token cache, so it should only be
			   checked in contexts such as preprocessor
			   directives where it is known that source text is
			   being scanned. */

EXTERN a_boolean
		number_contains_digit_separator;
			/* Set to TRUE by scan_number if the current token
			   has embedded digit separators (apostrophes) and
			   FALSE otherwise.  Valid only until the next call
			   to get_token. */

#if ASM_SUPPORT_NEEDED

EXTERN a_boolean
		in_asm_function_body;
			/* TRUE if processing takes place during the scan of
			   an asm function body. */

EXTERN sizeof_t	pos_in_asm_func_body_buffer;
			/* The number of characters that have been added to
			   asm_func_body_buffer thus far in processing. */

EXTERN char	*asm_func_body_buffer;
			/* Pointer to a dynamically allocated buffer used to
			   construct the string representation of an asm
			   function or Microsoft asm block. */

EXTERN sizeof_t	size_asm_func_body_buffer;
			/* The size of the asm buffer. */

EXTERN a_const_char
		*prev_asm_stop_char;
			/* The last character copied by the previous call to
			   copy_from_source_to_asm_func_buffer.  (Must be
			   EXTERN so it can be relocated if macro_buffer is
			   reallocated.) */


#if ASM_FUNCTION_ALLOWED
extern void copy_from_source_to_asm_func_buffer(
                                        a_const_char *stop_char,
                                        a_const_char *after_comment_stop_char);

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

#if !FULLY_RESOLVED_MACRO_POSITIONS
EXTERN a_source_position
		pos_of_macro_invocation;
			/* Global variable that contains the source
			   position of the outermost macro invocation
			   within whose expansion the current source
			   position occurs, if any, and
			   null_source_position otherwise.  Used to speed
			   up conversion of line locations to source
			   positions.  Not used with
			   FULLY_RESOLVED_MACRO_POSITIONS because original
			   positions must be calculated for each
			   position. */
#endif /* !FULLY_RESOLVED_MACRO_POSITIONS */

/*
The stop token array: If a syntactic error occurs, flush_tokens
will be called.  It will throw away tokens until it finds one for which
curr_stop_token_stack_entry->stop_tokens[curr_token] != 0 (i.e., a non-zero
entry means the corresponding token is in the set of stop tokens).
The entries are actually counts; each time something is added to the stop
set, an entry is incremented, and each time something is removed, the entry
is decremented.  The entries are unsigned so that overflow need not be
checked for; in boundary cases, like an error in an expression with
256 levels of parentheses, one might flush a little further than usual,
but this is not a meaningful problem.
*/
typedef unsigned char
		a_token_set_array_element;
typedef a_token_set_array_element
		a_token_set_array[(int)tok_last+1];
			/* Generic array-of-unsigned-char both for the global
			   stop token array and for local arrays used in
			   token caching. */

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
			   this one is popped off of the stack). */
  a_token_set_array
		stop_tokens;
			/* The set of tokens that will terminate a flush on
			   syntactic error.  A given token is in the set if
			   stop_tokens[token] != 0; */
} a_stop_token_stack_entry;

EXTERN a_stop_token_stack_entry_ptr
		curr_stop_token_stack_entry;
			/* Pointer to the current stop token stack entry. */

/*
A stack of lexical state arrays is maintained.  The top of the stack is
pointed to by curr_lexical_state_stack_entry.  A new lexical state stack
entry is pushed when a new lexical context is entered (for example,
when a template instantiation is done) and popped when that context
is no longer needed and the previous context must be restored.
*/
typedef struct a_lexical_state_stack_entry *a_lexical_state_stack_entry_ptr;
typedef struct a_lexical_state_stack_entry {
  a_lexical_state_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack). */
  int		cache_tokens;
			/* Non-zero if tokens fetched by get_token should also
			   be cached.  This is incremented by each caller that
			   requests token caching. */
  a_token_sequence_number
		last_tsn_in_cache;
			/* The token sequence number of the last token added
			   to the cache, or NO_TOKEN_SEQUENCE_NUMBER if the
			   cache is empty. */
  a_source_position
		error_position;
			/* The saved value of error_position when the state
			   stack was pushed. */
  a_token_cache	cache;
			/* The cache used to save tokens when cache_tokens is
			   TRUE. */
  a_byte_boolean
		caching_tokens;
			/* The saved value of caching_tokens when the state
			   stack was pushed. */
} a_lexical_state_stack_entry;

EXTERN a_lexical_state_stack_entry_ptr
		curr_lexical_state_stack_entry;
			/* Pointer to the current lexical state stack entry. */

/*
Return a pointer to the token cache associated with the current lexical
state stack entry.
*/
#define curr_lexical_state_cache() \
  (&curr_lexical_state_stack_entry->cache)

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
EXTERN a_boolean
		char_ends_id[CHAR_MAX-CHAR_MIN+1];
			/* A table to quickly identify characters that end
			   an identifier (to avoid a relatively expensive
			   call to f_is_identifier_char. */
EXTERN a_boolean
		is_raw_string_delimiter_char[CHAR_MAX-CHAR_MIN+1];
			/* For each character, whether or not it can appear
			   in the d-char-sequence of a raw string
			   literal. */

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
  teik_constant,	/* Extra information for a literal constant or
			   user-defined literal. */
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
  a_token_sequence_number
		ending_token_sequence_number;
			/* The ending sequence number of this token (different
			   from token_sequence_number for coalesced
			   identifiers). */
  a_cached_token_handle
		token_handle;
			/* For tokens from reusable caches, this identifies the
			   token entry from the reusable cache.  This is used
			   for variadic templates to allow an arbitrary
			   set of tokens from a cache to be rescanned.  If
			   a token from a reusable cache is later placed in
			   a non-reusable one, this still refers to the
			   entry in the reusable cache. */
  a_symbol_ptr	ud_lit_op_sym;
			/* For user-defined literal tokens (tok_ud_literal),
			   the literal operator or literal operator template
			   selected to produce the value of the literal, if
			   any; otherwise, NULL. */
  a_const_char	*ud_suffix;
			/* For user-defined literal tokens (tok_ud_literal),
			   the identifier portion of the literal operator
			   or literal operator template name (this is needed
			   when a user-defined literal is used to declare
			   the first literal operator or literal operator
			   template with that name and thus there is no
			   existing symbol for ud_lit_op_sym).  NULL for
			   tokens other than tok_ud_literal. */
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
			   giving the value for the literal constant.  For
			   tok_ud_literal tokens, this is the value to be
			   passed as the first argument to the literal
			   operator designated by ud_lit_op_sym or the
			   ck_string containing the characters of the token
			   spelling with which the literal operator template
			   is to be instantiated. */
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
  a_token_cache_ptr
		token_cache;
			/* Points to the token cache from which the token
			   list was obtained. */
  a_token_cache
		copy_of_token_cache;
			/* A copy of the token cache from which the token
			   list was obtained.  This is used when
			   discard_cache_when_done is set below.  The
			   token_cache pointer is used to recognize that
			   the cache associated with this entry is being
			   discarded, but a copy of that entry must be made
			   to be able to discard it later because token_cache
			   could point to an an entry that no longer exists
			   at the point at which the cache is actually
			   discarded. */
  uint32_t
		variadic_rescans_in_progress;
			/* If this cache is being used for a variadic
			   template rescan, this is the number of variadic
			   rescans in use.  This prevents the cache from
			   being popped during error recovery. */
  a_byte_boolean
		skip_terminator;
			/* TRUE if the tok_end_of_source terminator on the
			   cache should be bypassed instead of being
			   returned. */
  a_byte_boolean
		discard_cache_when_done;
			/* TRUE if token_cache should be freed when the
			   rescan is complete.  This is set if an attempt
			   is made to discard the cache while it is still
			   being scanned. */
} a_reusable_cache_entry;

		
/*
Bit vector used to pass flags into the cache token stream routines.
*/
typedef unsigned int a_cts_flag_set;
#define CTS_NO_OPTIONS		  0x0
#define CTS_COALESCE_IDS	  0x1
			/* TRUE if identifiers should be coalesced during
			   the caching process. */
#define CTS_STOP_ON_STATEMENT_END 0x2
			/* TRUE if the caching should stop if a semicolon
			   or mismatched right brace is encountered.  This
			   is used to avoid excessive caching in programs
			   with certain kinds of syntax errors. */


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

extern void adjust_token_handles(a_token_cache *cache);

extern
void cache_token_stream_coalesce_identifiers(a_token_cache_ptr  cache,
                                             a_token_set_array  stop_tokens);

extern void cache_token_stream_full(a_token_cache_ptr  cache,
                                    a_token_set_array  stop_tokens,
                                    a_cts_flag_set     options);

extern void cache_std_attribute(a_token_cache	*cache,
                                a_boolean	add_tokens_to_cache);

extern a_boolean cache_token_stream_until_matching_token(
				a_token_cache		*cache,
				a_cts_flag_set		options);

extern
void remove_token_from_cache(a_cached_token_ptr	ctp,
			     a_cached_token_ptr	*prev_ptr,
			     a_token_cache_ptr	cache);

extern a_token_kind get_token_to_be_cached(void);
/* Put some cached tokens on the get_token rescan list. */
extern void rescan_cached_tokens(a_token_cache *cache);
extern void f_rescan_cached_tokens(a_token_cache *cache,
                                   a_boolean	  discard_curr_token);
/* Push a reusable cache on to the reusable cache stack. */
extern void rescan_reusable_cache(a_token_cache *cache);
extern void rescan_reusable_cache_full(a_token_cache	*cache,
				       a_boolean	skip_terminator);
/* Rescan a copy of a token cache. */
extern void rescan_copy_of_cache(a_token_cache *cache);

extern void free_tokens_from_reusable_cache(a_cached_token_ptr	ctp,
					    a_token_cache	*cache);

extern void begin_caching_fetched_tokens(a_boolean	include_curr_token);

extern void end_caching_fetched_tokens(void);

extern
void copy_tokens_from_cache(a_token_cache_ptr	       src_cache,
                            a_token_sequence_number    first_tsn,
                            a_token_sequence_number    last_tsn,
			    a_boolean                  include_last_token,
                            a_token_cache_ptr	       dest_cache);

extern
void split_token_cache(a_token_cache	       *cache1,
                       a_token_cache	       *cache2,
                       a_token_sequence_number split_location,
                       a_boolean	       include_prev_token,
                       a_boolean	       okay_if_not_found,
                       a_boolean               update_cache_being_scanned);

/* Move a list of tokens from one cache to another. */
extern
void move_cached_tokens(a_cached_token_ptr	first_token,
			a_token_cache		*from_cache,
                        a_token_cache		*to_cache);


extern void update_reusable_cache_rescan_location(
				a_pack_expansion_stack_entry_ptr	pesep);

extern void increment_variadic_rescans_for_reusable_cache(void);

extern void decrement_variadic_rescans_for_reusable_cache(void);

extern a_boolean same_string_ignoring_underscores(a_const_char *s1, 
                                                  a_const_char *s2);

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

EXTERN a_boolean
		caching_tokens;
			/* TRUE when caching a token stream to be scanned
			   later. */

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
extern a_boolean f_is_identifier_char(a_const_char *ptr,
                                      int          *len,
                                      a_boolean    is_identifier_start);

#define is_identifier_char(ptr, len, is_start)                               \
  (!char_ends_id[*(ptr)-CHAR_MIN] && f_is_identifier_char(ptr, len, is_start))

#if CPPCLI_ENABLING_POSSIBLE && EDG_WIN32
extern an_error_code is_valid_UCN_identifier_char(
                                           unsigned long uchar,
                                           a_boolean     is_identifier_start);
#endif /* CPPCLI_ENABLING_POSSIBLE && EDG_WIN32 */
/* Check character as nonstandard. */
extern a_boolean is_nonstandard_character(char ch);
/* Skip white space. */
extern void skip_white_space(void);
/* Concatenate adjacent string literals in the current string constant. */
extern a_token_kind concat_adjacent_string_literals(
                                                 a_boolean function_name_case);
/* Get next token. */
extern a_token_kind get_token(void);
/* Return whether a token is a keyword token. */
extern a_boolean is_keyword_token(a_token_kind	token);
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
extern a_token_kind get_token_with_colon_separation(
                                              a_boolean *seen_tok_colon_colon);
#if FULLY_RESOLVED_MACRO_POSITIONS
#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */
extern int compare_macro_text_map_entry_with_offset(
                                                   a_const_void_ptr offset_ptr,
                                                   a_const_void_ptr entry_ptr);
#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
extern void add_concatenation_record(a_concatenation_record_ptr *headp,
                                     a_concatenation_record_ptr *tailp,
                                     char                       *line_loc,
                                     a_symbol_ptr               macro_sym);
/* Add an entry recording a logical modification to the source line. */
extern a_source_line_modif_ptr add_source_line_modif(
                          a_const_char              *line_loc,
                          sizeof_t                  num_chars_to_delete,
                          a_const_char              *inserted_text,
                          a_const_char              *end_inserted_text);
/* Free a source line modification entry. */
extern void free_source_line_modif(a_source_line_modif_ptr *slmp);
/* Remove a source line modification entry from the source_line_modif_list. */
extern void rem_source_line_modif(a_source_line_modif_ptr slmp);
/* Find the source line modification entry associated with a given source
   location. */
extern a_source_line_modif_ptr assoc_source_line_modif_full(
                                                 a_const_char *loc_in_line,
                                                 a_boolean    failure_allowed);
/* Interface for assoc_source_line_modif_full that assumes a matching
   source line modification will be found. */
#define assoc_source_line_modif(loc_in_line)                          \
  assoc_source_line_modif_full((loc_in_line), /*failure_allowed=*/FALSE)
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
extern a_source_line_modif_ptr nested_source_line_modif(
                                                    a_const_char *loc_in_line);
/* Convert a character location in the source line to a source sequence
   number and column. */
extern void conv_line_loc_to_source_pos(a_const_char      *loc_in_line,
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
extern a_token_kind next_token_full(a_token_sequence_number *seq,
                                    struct a_symbol_header  **sym_hdr);
/* Macro that calls next_token_full with default arguments. */
#define next_token()							\
  (next_token_full((a_token_sequence_number*)NULL,			\
                   (struct a_symbol_header **)NULL))
#define next_token_with_seq_number(seq) 				\
  (next_token_full((seq), (struct a_symbol_header **)NULL))

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
					a_const_char	**start_pos,
					a_boolean	is_identifier,
				        a_boolean	is_identifier_start,
					a_boolean	issue_diagnostics);

#if ABI_COMPATIBILITY_VERSION >= 302
extern char *make_canonical_identifier(a_const_char *identifier,
                                       sizeof_t     *length);
#else /* !(ABI_COMPATIBILITY_VERSION >= 302) */
/*
No translation of identifiers was done for older ABIs.  Simply
return the original identifier pointer.
*/
#define make_canonical_identifier(identifier, length) (identifier)
#endif /* ABI_COMPATIBILITY_VERSION >= 302 */

/*
Macro that returns TRUE when a given symbol header is for a given identifier
string.
*/
#define symbol_header_is_for_identifier_string(sym_hdr, tok_str)             \
  ((sym_hdr)->identifier[0] == (tok_str)[0] &&                               \
   strcmp((sym_hdr)->identifier, (tok_str)) == 0)

extern a_boolean curr_token_is_identifier_string(a_const_char *tok_str);

extern a_boolean check_context_sensitive_keyword(a_token_kind  tok_kind,
                                                 a_const_char  *tok_str);

/* Flags describing the encoding prefix, if any, of a string or character
   literal and which kind of literal is being processed.  The flags are
   defined as follows:

     kReee
     ||---
     || |
     || +---- The encoding prefix (U, u, u8, L, or none)
     |+------ Whether the literal is raw or not (string literals only)
     +------- The kind of literal (string or character) */

#define SCLK_NOT_A_LITERAL      -1
			/* The characters are not a literal prefix. */
#define SCLK_ORDINARY_LITERAL   0x01
			/* No encoding prefix */
#define SCLK_UTF8_LITERAL       0x02
			/* u8"..." */
#define SCLK_CHAR16_T_LITERAL   0x03
			/* u"..." or u'x' */
#define SCLK_CHAR32_T_LITERAL   0x04
			/* U"..." or U'x' */
#define SCLK_WIDE_LITERAL       0x05
			/* L"..." or L'x' */
#define SCLK_RAW_STRING_LITERAL 0x08
			/* R"...", u8R"...", uR"xxx", or UR"..." */
#define SCLK_STRING_LITERAL     0x10
			/* TRUE for string literal, FALSE for char literal */

/* A convenient name for a narrow non-raw string literal: */
#define SCLK_ORDINARY_STRING_LITERAL \
                                  (SCLK_ORDINARY_LITERAL | SCLK_STRING_LITERAL)

/* Extract the encoding prefix from a_string_or_literal_kind. */
#define literal_encoding_prefix(k) ((k) & 0x7)

/* Return the offset of the first character following the prefix (if any)
   and quote character of a string or character literal described by the
   specified a_string_or_char_literal_kind. */
#define offset_to_start_of_literal_value(k)                                  \
  ((int)(((k) & SCLK_RAW_STRING_LITERAL) != 0) /* 1 for "R" */               \
   + ((literal_encoding_prefix(k) > SCLK_UTF8_LITERAL) ? 1 /* 1 for u/U/L */ \
      : (literal_encoding_prefix(k) == SCLK_UTF8_LITERAL) ? 2 /* 2 for u8 */ \
      : 0) /* 0 for no prefix */                                             \
   + 1 /* 1 for quoting character */)

extern a_string_or_char_literal_kind scan_encoding_prefix(a_const_char *loc);

extern a_boolean accum_quoted_string(
                  unsigned long                 *num_chars,
                  a_boolean                     is_header_name,
                  a_string_or_char_literal_kind literal_kind,
                  char                          quoting_char,
                  a_const_char                  *start_of_raw_string_delimiter,
                  int                           raw_string_delimiter_len);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_valid_GUID_string(a_const_char  *str,
                                      a_targ_size_t length);

#if NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
extern void setlocale_pragma(a_pending_pragma_ptr	ppp);
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */

/* Macro that tests whether f_is_generalized_identifier_start needs
   to be called.  We don't need to call it in C mode, or if we have an
   identifier that has already been coalesced.  There are other cases that
   could be eliminated such as tok_ptr_to_member (which returns FALSE) and
   current tokens that are not things that could start an identifier.  These
   have smaller payoffs so are not currently included. */
#define is_generalized_identifier_start_full(options, type)		\
  /* if */ ((C_dialect != C_dialect_cplusplus) /* { */ ?		\
    curr_token == tok_identifier					\
  /* } else { */ :							\
    /* if */ (curr_token == tok_identifier &&				\
              locator_for_curr_id.has_been_coalesced) /* { */ ?		\
      TRUE								\
    /* } else { */ :							\
      f_is_generalized_identifier_start(options, (type)))		\
  /* } */								\

/*
Interface to is_generalized_identifier_start_full that provides a default
NULL type pointer.
*/
#define is_generalized_identifier_start(options)			\
  (is_generalized_identifier_start_full((options), (a_type_ptr)NULL))

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
extern void open_file_and_push_input_stack(
                                         a_const_char *file_name,
                                         a_boolean    use_search_path,
                                         a_boolean    is_include_file,
                                         a_boolean    is_system_include,
                                         a_boolean    is_preinclude,
                                         a_boolean    preinclude_macros,
                                         a_boolean    is_implicit_include,
                                         a_boolean    is_include_next,
                                         a_boolean    continue_on_open_failure,
                                         a_boolean    *include_was_suppressed);

extern a_boolean open_file_for_input(
		a_const_char			*file_name,
		a_boolean			use_search_path,
		a_boolean			is_include_file,
		a_boolean			is_system_include,
		a_boolean			is_include_next,
		a_boolean			is_implicit_include,
		a_boolean			is_preinclude,
		a_boolean			continue_on_open_failure,
		a_const_char			**full_file_name,
		a_const_char			**display_name,
		FILE				**new_input_file,
		a_boolean			*suppress_include,
		a_unicode_source_kind		*unicode_source_kind,
		a_directory_name_entry_ptr	*dir_entry);

extern a_boolean header_can_be_found(a_const_char *filename,
                                     a_boolean    is_system_include,
                                     a_boolean    is_include_next);

extern void push_input_stack(
			FILE			    *new_input_file,
                        a_const_char		    *name_as_written,
                        a_const_char		    *display_name,
                        a_const_char		    *full_file_name,
			a_boolean                   is_include_file,
			a_boolean                   is_system_include,
                        a_boolean                   is_preinclude,
			a_boolean		    preinclude_macros,
                        a_boolean                   is_implicit_include,
                        a_unicode_source_kind       unicode_source_kind,
                        a_directory_name_entry_ptr  dir_entry,
			an_include_file_history_ptr ifhp);

extern void push_cloned_input_stack_entry(void);

extern void pop_input_stack(void);

extern void pop_cloned_input_stack_entry(void);

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
extern void flush_until_matching_token_full(a_boolean	limit_flush);
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
extern void push_lexical_state_stack(void);
extern void pop_lexical_state_stack(void);
extern a_template_ptr scan_template_template_argument(
				a_template_ptr		param_template,
				a_source_position	*err_pos,
				a_boolean		is_default);

extern void insert_string_into_token_stream(
                                        a_const_char      *string,
                                        a_boolean         insert_after,
                                        a_boolean         p_expand_macros,
                                        a_source_position position_for_tokens);

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
                              a_boolean         keep_spacing,
                              a_boolean         suppress_identifier_wrapping);

extern char *make_copy_of_token_string(void);

extern a_const_char *il_string_for_curr_token(void);

extern a_preinclude_file_ptr alloc_preinclude_file(void);

#if GET_DEFINITION_OF_CLASS_NEEDED
extern void get_definition_of_class(a_type_ptr	class_type);
#endif /* GET_DEFINITION_OF_CLASS_NEEDED */

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

#if DEBUG
/* Show space used in the lexical routines, for debugging purposes. */
extern unsigned long show_lexical_space_used(void);

extern void db_token_cache(a_token_cache *cache,
                           a_const_char	 *cache_name);

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
of curr_source_line, as indicated by line_loc == NULL.  Normally, the
location in that case will be the first character of curr_source_line.
However, when this occurs at the end of a file (at_end_of_source_file ==
TRUE), the previous contents of the line will not yet have been replaced
but have already been scanned.  In this case, we treat the insertion as
having appeared just before the terminating LE_END_OF_LINE to avoid
processing the now-defunct line a second time.
*/
#define loc_of_insert(slmp)                                                \
  ((slmp)->line_loc != NULL ? (slmp)->line_loc :                           \
   at_end_of_source_file ? curr_source_line + end_of_line_escape_offset    \
                         : curr_source_line)

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

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void init_whitespace_keywords(void);

extern char *generate_top_level_metadata_code(an_assembly_index index);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern a_token_cache_ptr alloc_token_cache(void);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern void free_token_cache(a_token_cache_ptr tcp);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_hash_value hash_include_file_history(a_void_ptr	key);
extern a_boolean compare_include_file_history(a_void_ptr	entry,
                                              a_void_ptr	key);
extern a_hash_value hash_include_search_result(a_void_ptr	key);
extern a_boolean compare_include_search_result(a_void_ptr	entry,
                                               a_void_ptr	key);

#if UNIQUE_FILE_IDENTIFIER_AVAILABLE
extern a_hash_value hash_unique_file_id_for_table(a_void_ptr	key);

extern a_boolean compare_unique_file_id(a_void_ptr	entry,
                                        a_void_ptr	key);
#endif /* UNIQUE_FILE_IDENTIFIER_AVAILABLE */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_partial_class_body_ptr cache_partial_class_body(
                                                       a_type_ptr class_type);

extern void replace_curr_token(a_token_kind  new_token);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */



#endif /* ifndef LEXICAL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
