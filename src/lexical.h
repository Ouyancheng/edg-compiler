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
  tok_lbracket          /* [ */,    tok_rbracket           /* ] */,
  tok_lparen            /* ( */,    tok_rparen             /* ) */,
  tok_period            /* . */,    tok_arrow              /* -> */,
  tok_plus_plus         /* ++ */,   tok_minus_minus        /* -- */,
  tok_ampersand         /* & */,    tok_star               /* * */,
  tok_plus              /* + */,    tok_minus              /* - */,
  tok_compl             /* ~ */,    tok_not                /* ! */,
  tok_divide            /* / */,    tok_remainder          /* % */,
  tok_shift_left        /* << */,   tok_shift_right        /* >> */,
  tok_lt                /* < */,    tok_gt                 /* > */,
  tok_le                /* <= */,   tok_ge                 /* >= */,
  tok_eq                /* == */,   tok_ne                 /* != */,
  tok_excl_or           /* ^ */,    tok_or                 /* | */,
  tok_and_and           /* && */,   tok_or_or              /* || */,
  tok_quest_mark        /* ? */,    tok_colon              /* : */,
  tok_assign            /* = */,    tok_times_assign       /* *= */,
  tok_divide_assign     /* /= */,   tok_remainder_assign   /* %= */,
  tok_plus_assign       /* += */,   tok_minus_assign       /* -= */,
  tok_shift_left_assign /* <<= */,  tok_shift_right_assign /* >>= */,
  tok_and_assign        /* &= */,   tok_excl_or_assign     /* ^= */,
  tok_or_assign         /* |= */,   tok_comma              /* , */,
  tok_sharp             /* # */,    tok_paste              /* ## */,
  /* Punctuators (standard, 3.1.6) that are not also operators: */
  tok_lbrace            /* { */,    tok_rbrace             /* } */,
  tok_semicolon         /* ; */,    tok_ellipsis           /* ... */,
  /* Keywords (standard, 3.1.1): */
  tok_auto,                         tok_break,
  tok_case,                         tok_char,
  tok_const,                        tok_continue,
  tok_default,                      tok_do,
  tok_double,                       tok_else,
  tok_enum,                         tok_extern,
  tok_float,                        tok_for,
  tok_goto,                         tok_if,
  tok_int,                          tok_long,
  tok_register,                     tok_return,
  tok_short,                        tok_signed,
  tok_sizeof,                       tok_static,
  tok_struct,                       tok_switch,
  tok_typedef,                      tok_union,
  tok_unsigned,                     tok_void,
  tok_volatile,                     tok_while,
  /* Extensions (__ALIGNOF__ is similar to sizeof; __INTADDR__ is used
     to scan an integer address expression for offsetof): */
  tok_alignof,                      tok_intaddr,
#if RESTRICT_ALLOWED
  tok_restrict,
#endif /* RESTRICT_ALLOWED */
  /* C++ tokens not in C (ARM, 2.4): */
  tok_colon_colon       /* :: */,   tok_period_star        /* .* */,
  tok_arrow_star        /* ->* */,
  tok_asm,                          tok_catch,
  tok_class,                        tok_delete,
  tok_friend,                       tok_inline,
  tok_new,                          tok_operator,
  tok_private,                      tok_protected,
  tok_public,                       tok_template,
  tok_this,                         tok_throw,
  tok_try,                          tok_virtual,
  /* Recognized in cfront compatibility mode only. */
  tok_overload,
  /* Token used to indicate keywords that are not yet implemented. */
  tok_unimplemented,
  /* Error token. */
  tok_error,
  /* Place-holder for last position in enumeration. */
  tok_last
} a_token_kind;
/* More compact form: */
typedef a_byte a_byte_token_kind;

/*
Table of names corresponding to token kinds.
*/
EXTERN char	*token_names[(int)tok_last+1]
#if VAR_INITIALIZERS
= {"identifier", "float constant", "int constant", "char constant",
   "string literal", "end of source", "newline", "header name",
   "pp number", "digit sequence", "cpp quote", "ptr to member", 
   "[", "]", "(", ")", ".", "->", "++", "--", "&", "*", "+", "-",
   "~", "!", "/", "%", "<<", ">>", "<", ">", "<=", ">=", "==",
   "!=", "^", "|", "&&", "||", "?", ":", "=", "*=", "/=", "%=",
   "+=", "-=", "<<=", ">>=", "&=", "^=", "|=", ",", "#", "##", "{", "}",
   ";", "...", "auto", "break", "case", "char", "const",
   "continue", "default", "do", "double", "else", "enum", "extern",
   "float", "for", "goto", "if", "int", "long", "register",
   "return", "short", "signed", "sizeof", "static", "struct",
   "switch", "typedef", "union", "unsigned", "void", "volatile",
   "while", "__ALIGNOF__", "__INTADDR__",
#if RESTRICT_ALLOWED
   "restrict",
#endif /* RESTRICT_ALLOWED */
   "::", ".*", "->*", "asm", "catch", "class", "delete", "friend",
   "inline", "new", "operator", "private", "protected", "public",
   "template", "this", "throw", "try", "virtual",
   "overload", "unimplemented", "error",
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
#define GID_CLASS_MUST_BE_PROTOTYPE_INSTANTIATION \
				      0x80
			/* If the identifier is a qualified name the
			   class component must refer to the prototype
			   instantiation. */
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
  ilm_class,		/* Find only class names. */
  ilm_tag,		/* Find only tag names. */
  ilm_tentative_type,	/* Uses IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME
			   to do the lookup. */
  ilm_ctor_initializer_name,
			/* Used to look up identifiers in the initializer
			   list of a constructor declaration. */
  ilm_last
} an_identifier_lookup_mode;


typedef struct a_token_cache *a_token_cache_ptr;
typedef struct a_token_cache {
  /* Data structure used to hold a token cache, i.e., some number of
     tokens that are being saved for later rescanning. */
  struct a_cached_token
		*first_token,
		*last_token;
			/* First and last tokens on the list, or both NULL
			   if the list is empty. */
  a_byte_boolean
		is_reusable;
			/* TRUE if this cache will be reused (e.g.,
			   for a template cache.  This should be TRUE if
			   there is any possibility that the cache may
			   be resued. */
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
   (an_opname_kind)onk_none,          /* tok_alignof */
   (an_opname_kind)onk_none,          /* tok_intaddr */
#if RESTRICT_ALLOWED
   (an_opname_kind)onk_none,          /* tok_restrict */
#endif /* RESTRICT_ALLOWED */
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
   (an_opname_kind)onk_none,          /* tok_overload */
   (an_opname_kind)onk_none,          /* tok_unimplemented */
   (an_opname_kind)onk_none,          /* tok_error */
   (an_opname_kind)onk_last           /* tok_last */
  }
#endif /* VAR_INITIALIZERS */
;
/*
Array giving the token name for each opname kind.
*/
EXTERN char	*opname_names[(int)onk_last];


/*
Structure used to record information about files that have been included.
See the comment preceding find_include_history in lexical.c.
*/
typedef struct an_include_file_history *an_include_file_history_ptr;
typedef struct an_include_file_history {
  an_include_file_history_ptr
                next;
			/* Pointer to the next entry in a linked list of
			   history entries. */
  char          *full_name;
			/* Pointer to the full path name of the include
			   file. */
  sizeof_t	name_length;
			/* Length of full_name. */
  unsigned int	suppress_subsequent_include:1;
			/* TRUE if this file is potentially one that can
			   have subsequence includes suppressed. */
  unsigned int	pragma_once:1;
			/* TRUE if this file contained a "#pragma once"
			   directive. */
  unsigned int	ifdef_guard:1;
			/* TRUE if this file was guarded by a #ifdef. */
  unsigned int	ifndef_guard:1;
			/* TRUE if this file was guarded by a #ifndef. */
  char          *controlling_macro_name;
			/* The name of the macro used to guard the include
			   file against multiple inclusions. */
} an_include_file_history;


/*
The order of these states is important - see near return of get_token().
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


extern a_boolean suppress_subsequent_include_of_file
				(char                         *full_name,
				 an_include_file_history_ptr *ifhp_ptr);

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
  unsigned int
		is_include_file:1;
			/* TRUE if this file was added to the input stack
			   as the result of a #include directive.  FALSE
			   for all other cases including implicitly included
			   source files. */
  unsigned int
	        nested_inclusion:1;
			/* TRUE if this is a nested inclusion of a file
			   already on the input stack. */
  unsigned int	saved_any_tokens_fetched:1;
			/* Used to save and restore the value of the global
			   variable any_tokens_fetched_from_curr_input_file. */
#if CHECKING
      unsigned int
		dummy:2;
			/* Extra field that can be initialized to prevent
			   spurious reference to uninitialized data warnings
			   from CodeCenter. */
#endif /* CHECKING */
  a_byte        ifg_state;
			/* Include file guard state information used to
                           determine whether subsequent inclusions of this
                           file may be suppressed. */
  an_include_file_history_ptr
               include_history;
                        /* Pointer to the structure that preserves information
                           used for include file guard processing. */
} an_input_stack_entry;

/* See lexical.c for the definitions of input_stack, depth_input_stack,
   and seq_number_last_read. */
EXTERN an_input_stack_entry_ptr
		curr_ise;
			/* Pointer to input_stack[depth_input_stack].  NULL
			   if depth_input_stack == -1. */


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
			   ended by both a newline and a null.  Space is
			   dynamically allocated, and its upper bound is given
			   by after_end_of_curr_source_line.
			   See lexical_init for the initial allocation.
			   When after_end_of_all_source is TRUE, this array
			   contains just a null (no newline, nothing else). */
#define CURR_SOURCE_LINE_INITIAL_ALLOCATION 3000
#define CURR_SOURCE_LINE_INCREMENTAL_ALLOCATION 5000
			/* Initial and incremental allocation sizes for
			   curr_source_line.  The initial allocation should be
			   such that almost all cases can be accepted (so that
			   the realloc is hardly ever needed) -- that means
			   big enough for all the lines of a large macro
			   definition. */
EXTERN char	*after_end_of_curr_source_line /* = NULL */;
			/* Address past the last element of curr_source_line,
			   as an aid to checking for overflow, etc.  A variable
			   because curr_source_line line can reallocated larger
			   if needed. */
EXTERN a_seq_number
		curr_seq_number;
			/* The sequence number of the first physical line
			   in curr_source_line.  Once the end of file on
			   the primary source file has been passed, this
			   indicates a sequence number one past the highest
			   sequence number actually read, as an indication
			   of a sort of end-of-file line. */

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
			   null at the end of the line when the line has
			   been completely scanned. */

/*
Marker characters used within curr_source_line and macro_buffer.
These must not conflict with any characters that can be read from input.
*/
#ifdef lint
/* Define the characters in a way that will avoid lint warnings about
   nonportable comparisons.  The code will not actually work this way,
   however; this is only for lint checking. */
#define END_OF_TOKEN_MARKER '`'
#define ATTENTION_MARKER    '@'
#else /* !defined(lint) */
/* 0x81 is a good choice because it works okay with ISO 8859 (Latin-1, ...)
   and EUC.  However, for full internationalization there should really
   be no special characters. */
#if CHAR_MIN < 0
/* Host has signed characters. */
#define UNUSED_CHAR_POS (-127)  /* ffffff81 in integer form */
#else /* CHAR_MIN < 0 */
/* Host has unsigned characters. */
#define UNUSED_CHAR_POS 0x81
#endif /* CHAR_MIN < 0 */
#define END_OF_TOKEN_MARKER ((char)UNUSED_CHAR_POS)
#if !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS
#define ATTENTION_MARKER    ((char)(UNUSED_CHAR_POS+1))
#else /* READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
/* When reading source in binary mode under MS-DOS, we know that control-Z
   indicates end-of-file, so use that character as the attention marker
   since it cannot otherwise make it into source lines. */
#define ATTENTION_MARKER    CONTROL_Z
#endif /* !READ_SOURCE_IN_BINARY_MODE_FOR_MSDOS */
#endif /* ifdef lint */
			/* END_OF_TOKEN_MARKER marks the ends of tokens
			   in macro definitions and macro expansions.
			   It does not appear in curr_source_line, only
			   in macro_buffer.  ATTENTION_MARKER marks the
			   first character of a sequence of characters
			   that is deleted or replaced (as a cue to look
			   at the source_line_modif_list).  It can appear
			   both in curr_source_line and macro_buffer. */

/*
Modifications made to the current source line.  orig_line_modif holds
information needed to restore curr_source_line to its original form
when read; source_line_modif is the information needed to transform
curr_source_line into its fully preprocessed form (with macros expanded,
etc.).
*/

typedef enum /*an_orig_line_modif_kind*/ {
  olm_trigraph,
  olm_line_splice
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
    /* When kind == olm_trigraph: */
    char	trigraph_orig_char;
			/* The original third character of the trigraph,
			   for example "=" in "??=". */
    /* When kind == olm_line_splice: */
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
     the current contents of curr_source_line to turn them into the fully
     preprocessed version of the line. */
  a_source_line_modif_ptr
		next;
			/* A pointer to the next entry on the
			   source_line_modif_list, or NULL if there is
			   no next entry.  Also used to link entries on the 
			   avail_source_line_modifs list. */
  char		*line_loc;
			/* The location in curr_source_line or macro_buffer
			   of the modification.  Points to an ATTENTION_MARKER
			   character that replaced the first character of
			   the text to be deleted.  NULL to indicate that
			   this entry is be inserted preceding the first
			   character of the source line (there can be only
			   one of those). */
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
  unsigned int	is_isolated_text:1;
			/* TRUE if this modification is a temporary one
			   that inserts the text so that it can be
			   macro-expanded.  Such an entry is not truly
			   part of the logical source line. */
  unsigned int	is_for_comment:1;
			/* TRUE if this modification is due to a comment
			   in the source (as opposed to a macro expansion). */
  unsigned int	parent_modif_determined:1;
			/* TRUE if a value has been determined for
			   parent_modif. */
  char		orig_char;
			/* The character that was in the source line at
			   position line_loc (provided so that the original
			   line can be reconstructed; not needed otherwise).
			   Meaningless when line_loc == NULL. */
  char		inserted_chars[2];
			/* Place for an insert string of up to two characters
			   including the null.  Used for the blank that
			   replaces comments. */
  char		*inserted_text;
			/* Pointer to text to be inserted, in macro_buffer.
			   The text is terminated by a null.  Points to a
			   zero-length string if this is a deletion only. */
  char		*end_inserted_text;
			/* Pointer to the null at the end of inserted_text. */
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
  a_source_line_modif_ptr
		assoc_copy_modif;
			/* Used when making copies of modification entries
			   that apply to the expanded versions of macro
			   arguments.  From the master modification entry 
			   for a macro argument, this points to the copy
			   entry generated in the current expansion of that
			   argument.  NULL when not used. */
  a_source_position
		source_position;
			/* When source_position.seq != 0, this indicates
			   the source position associated with this
			   modification.  This is particularly useful when
			   the modification is for a multi-line macro call. */
  char		*text_from_primary_source_line;
			/* If non-NULL, text from the location pointed to
			   to the final null character is from the primary
			   source line, placed in this modification so that
			   it can be token-pasted with the end of a macro
			   expansion in pcc mode. */
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
save_curr_token_state and restore_curr_token_state.
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
EXTERN int	kind_of_white_space_skipped;
			/* Kind of white space skipped by the most recent
			   call to skip_white_space (not necessarily
			   correct when get_token is called).  Used in
			   scanning macro definitions, where white space
			   can be significant.  0 = none, others as bits
			   as follows: */
#define WHITE_SPACE_COMMENTS 0x01
#define WHITE_SPACE_OTHER    0x02
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
		any_initial_get_token_tests_needed;
			/* TRUE if a condition exists that requires some
			   special processing when get_token is called.
			   This is set when tokens are being rescanned
			   from a cache or when there are pragmas that
			   are associated with the current token. */


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
		a_token_set_array[(int)tok_last+1];
			/* Generic array-of-unsigned-char both for global
			   variable stop_token_array and for local arrays
			   used in token caching. */
typedef a_token_set_array
		a_stop_token_array;

EXTERN a_stop_token_array
		stop_token_array;
			/* The current set of tokens that will terminate
			   a flush on syntactic error.  A given token is
			   in the set if stop_token_array[token] != 0; */

/*
Other general variables:
*/
EXTERN a_boolean
		is_id_char[CHAR_MAX-CHAR_MIN+1];
			/* For each character, whether or not it can be a
			   character after the first in an identifier.
			   Also used in scanning pp-numbers. */

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
  teik_pragma		/* Extra information for a pragma. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_token_extra_info_kind;
typedef struct a_cached_token *a_cached_token_ptr;
typedef struct a_cached_token {
  /* Information on a single token, saved for later rescanning of the
     token. */
  a_cached_token_ptr
		next;	/* Next cached token on the list, NULL if none. */
  a_source_position
		source_position;
			/* Source position of the token. */
  a_byte_token_kind
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
/* Save the current token in a token cache. */
extern void cache_curr_token(a_token_cache *cache);
/* Save a token stream in a token cache. */
extern void cache_token_stream(a_token_cache      *cache,
                               a_token_set_array  stop_tokens);
/* Put some cached tokens on the get_token rescan list. */
extern void rescan_cached_tokens(a_token_cache *cache);
/* Push a reusable cache on to the reusable cache stack. */
extern void rescan_reusable_cache(a_token_cache *cache);
/* Rescan a copy of a token cache. */
extern void rescan_copy_of_cache(a_token_cache *cache);

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
a_boolean read_logical_source_line(a_boolean do_pop_on_end_of_file);
/* Skip white space. */
extern void skip_white_space(void);
/* Get next token. */
extern a_token_kind get_token(void);
/* Scan a literal constant. */
extern a_token_kind scan_literal_constant(a_token_kind kind);
/* Generate a line-identifying directive in preprocessing output. */
extern void gen_pp_line_info(char kind,
		             int  increment);
/* Generate textual preprocessing output for the current line. */
extern void gen_pp_output_for_curr_line(void);
/* Generate a line information record in the raw listing file. */
extern void gen_rlisting_line_info(char kind);
/* Generate raw listing output for the macro-expanded form of the current
   line. */
extern void gen_expanded_raw_listing_output_for_curr_line(
                                                   a_boolean do_inserted_text);
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
/* Back up one token. */
extern void unget_token(void);
/* Get a C++ destructor name, like "~A". */
extern a_boolean f_get_destructor_name(void);
#define get_destructor_name()			                      \
  ((curr_token == tok_compl) ? f_get_destructor_name() : FALSE)
/* get_opname is in lexical.c. */


extern a_symbol_ptr coalesce_template_class_reference
			(a_symbol_ptr		   template_symbol,
			 an_identifier_options_set options,
			 a_boolean		   *err);

extern void begin_rescan_of_pragma_tokens(struct a_pending_pragma *ppp,
					  a_stop_token_array      stop_tokens);

extern void wrapup_rescan_of_pragma_tokens(a_boolean          error_in_pragma,
                                           a_stop_token_array stop_tokens);

extern a_boolean f_is_generalized_identifier_start
                     (an_identifier_options_set options);
extern a_boolean coalesce_and_lookup_qualified_name
                     (an_identifier_options_set        options,
		      an_identifier_lookup_mode	       ilm,
                      a_boolean			       *err);
extern a_symbol_ptr coalesce_and_lookup_generalized_identifier
                        (an_identifier_options_set        options,
                         an_identifier_lookup_mode        ilm,
                         a_boolean                        *err);

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
    f_is_generalized_identifier_start(options))				\
  /* } */								\


/* Return TRUE if the current token is the start of a C++ qualified
   name (including a simple identifier). */
#define is_qualified_name_start()                                        \
  (is_generalized_identifier_start(GID_NO_OPTIONS))

/* Same thing for use in switch statements, in the form
     case QUALIFIED_NAME_START_CASE:
   Note that one must check for "::new" and "::delete" separately.
*/
#define QUALIFIED_NAME_START_CASE tok_identifier:	\
                             case tok_colon_colon

#if RESTRICT_ALLOWED
#define is_type_qualifier_token(tok)                                   \
  ((tok) == tok_const || (tok) == tok_volatile || (tok) == tok_restrict)
#else /* !RESTRICT_ALLOWED */
#define is_type_qualifier_token(tok)                                   \
  ((tok) == tok_const || (tok) == tok_volatile)
#endif /* RESTRICT_ALLOWED */

/* Push a file onto the input stack. */
extern void open_file_and_push_input_stack
                                (char                       *file_name,
                                 a_directory_name_entry_ptr search_path,
				 a_boolean		    is_include_file,
				 a_boolean                  is_system_include);
extern FILE *open_file_for_input(char                       *file_name,
                                 a_directory_name_entry_ptr search_path,
                                 a_boolean                  replace_suffix,
                                 char                       **full_file_name,
                                 char                       **display_name);
extern void push_input_stack (FILE      		  *new_input_file,
                              char      		  *name_as_written,
                              char      		  *display_name,
                              char     			  *full_file_name,
			      a_boolean                   is_include_file,
			      a_boolean                   is_system_include,
			      an_include_file_history_ptr ifhp);

extern void pop_input_stack(void);

/* Set the error position to the current token position. */
#define set_err_pos_to_curr_token()                                   \
{ copy_source_position(pos_curr_token, error_position);}

/* Define primitive operations on a_token_set_array. */
#define clear_token_set_array(array) memzero((char *)(array), sizeof(array))
#define incr_token_set_array_element(array, tok) (array)[(int)(tok)]++
#define decr_token_set_array_element(array, tok) (array)[(int)(tok)]--

/* Clear the set of syntax error flush stop tokens. */
#define clear_stop_tokens() clear_token_set_array(stop_token_array)
/* Add a token to the set of syntax error flush stop tokens. */
#define add_stop_token(stop_token)                                    \
  incr_token_set_array_element(stop_token_array, stop_token)
/* Remove a token from the set of syntax error flush stop tokens. */
#define remove_stop_token(stop_token)                                 \
  decr_token_set_array_element(stop_token_array, stop_token)
/* Copy one stop token array to another. */
#define copy_stop_tokens(from, to) \
  memcpy((char *)(to), (char *)(from), sizeof(stop_token_array));
/* Flush to the token that matches an opening token (e.g., parenthesis). */
extern void flush_until_matching_token(void);
/* Flush tokens on error, to a token in the stop token set. */
extern void flush_tokens(void);
/* Initialize the lexical routines. */
extern void lexical_reset(void);
extern void lexical_one_time_init(void);
extern void lexical_init(void);
/* Flush until the tok_end_of_source terminating a token cache is found. */
#define flush_past_token_cache_terminator()			\
  {								\
    while (curr_token != tok_end_of_source) (void)get_token();	\
    /* Advance past the end-of-source token. */			\
    (void)get_token();						\
  }

#if DEBUG
/* Show space used in the lexical routines, for debugging purposes. */
extern unsigned long show_lexical_space_used(void);
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

#endif /* ifndef LEXICAL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
