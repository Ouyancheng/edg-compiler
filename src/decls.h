/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

decls.h -- Declarations related to decls.c (having to do with scanning
           of declarations).

*/

/* Avoid including these declarations more than once: */
#ifndef DECLS_H
#define DECLS_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* SYMBOL_TBL_H */


/*
Kinds of linkage, meaning whether or not an identifier declared in
a certain way is linked to (the same as) some other liked-named identifier
declared elsewhere.  See 3.1.2.2.
*/
typedef enum /*an_id_linkage_kind*/ {
  idl_none,		/* Identifier is different from others of the same
			   name. */
  idl_internal,		/* Identifier is the same as others in the same
			   compilation with this name. */
  idl_external		/* Identifier is the same as others in the same
			   compilation and in other compilations for
			   the same program */
} an_id_linkage_kind;
/*
Data structure used to pass information about function declarations back
from the scanning of the function declarator.
*/
typedef struct a_param_id *a_param_id_ptr;
typedef struct a_param_id {
  /* Entry giving the name of one parameter in a function declarator.
     The type of the parameter does not appear here; it is in an
     entry of type a_param_type attached to the type entry for the
     function.  The present structure is used both for old-style
     identifier lists and for the names of parameters in prototypes. */
  a_param_id_ptr
		next;
			/* Next parameter id on the list, or NULL if this
			   is the last parameter id. */
  a_symbol_locator
		locator;
			/* Locator that gives information on entering this
			   symbol into the symbol table, including (indirectly)
			   the identifier's name. */
  a_byte_boolean
		declaration_processed;
			/* Set to TRUE when the declaration for an old-style
			   identifier is processed.  Used to check for
			   undeclared parameters that get "int" type by
			   default.  Not used for prototyped parameters. */
  a_symbol_ptr	symbol;
			/* When declaration_processed is TRUE, this points
			   to the symbol for an old-style parameter.  The
			   a_variable entry is not yet attached to it. */
  a_type_ptr	type;
			/* For a new- or old-style function parameter, this
			   is its type.  This is usually the same as the
			   information in the function type parameter list,
			   but is kept here also so we can be sure of
			   associating the proper identifier and type
			   in error cases. */
  a_storage_class
		storage_class;
			/* For a new- or old-style style function parameter,
			   this is the storage class to be associated with
			   it when it is declared. */
} a_param_id;

typedef struct a_func_info_block *a_func_info_block_ptr;
typedef struct a_func_info_block {
  /* Information about the parameter list in a function declarator. */
  a_scope_number
		scope_number;
			/* The scope number used for the function prototype
			   scope for the parameters, to be reused for the
			   function scope if a body is found. */
  a_param_id_ptr
		param_id_list;
			/* List of entries giving parameter names, NULL if
			   there were none.  Used for both old-style and
			   new-style parameter names. */
  a_byte_boolean
		any_prototype_names_omitted;
			/* TRUE if the parameter list is a prototype list,
			   and it includes at least one parameter with
			   just a type and no name. */
} a_func_info_block;

/*
Flag that is meaningful while within a routine.  It indicates whether the
current routine's type was explicitly specified, rather than defaulted
to "int".
*/
EXTERN a_boolean
		curr_rout_type_explicitly_specified;


EXTERN a_param_id_ptr
		avail_param_ids;
			/* List of parameter id entries freed and available
			   for reuse. */

EXTERN a_boolean
		in_old_style_param_decl_list;
			/* When we are inside the declaration list for
			   old-style function parameters, this flag is TRUE. */

/* Test whether or not the current token is the start of a type. */
extern a_boolean is_type_start(void);

/* Test whether or not the current token is the start of a declaration. */
extern a_boolean is_decl_start(void);

extern void type_name(a_type_ptr *type_ptr);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean is_definition);

extern void opt_declaration_list(void);

extern void translation_unit(void);

extern void free_param_id_list(a_param_id_ptr *pidlist);

extern a_boolean reconcile_external_symbol_types(
                            a_symbol_ptr          ext_sym,
                            a_source_position_ptr position,
                            a_type_ptr            type_ptr,
                            a_boolean             suppress_incompatible_error);

#if ASM_FUNCTION_ALLOWED
/* Routine is only needed externally when asm functions are allowed. */
extern void decl_var_or_routine(a_symbol_locator   *locator,
                                a_storage_class    storage_class,
                                a_type_ptr         type_ptr,
                                a_boolean          is_implicit_function,
                                a_boolean          is_function_def_with_body,
                                a_symbol_ptr       *symbol_ptr,
                                an_id_linkage_kind *linkage_ptr,
                                a_type_ptr         *old_type,
                                a_symbol_ptr       *ext_sym);
#endif /* ASM_FUNCTION_ALLOWED */

/* Bit vector used to pass flags into and out of declaration_specifiers.
   Each bit represent a flag. */
typedef int a_decl_flag_set;
/* Constants defining bits in the input bit vector, used in calls to
   declaration_specifiers. */
#define DSI_NO_INPUT_FLAGS 0x0
#define DSI_STORAGE_CLASS_SPECIFIER_ALLOWED 0x1
			/* If this bit is set the declaration specifier may
			   contain a storage class keyword. */
#define DSI_TYPE_SPECIFIER_ALLOWED 0x2
			/* If this bit is set the declaration specifier may
			   contain a type specifier. */
#define DSI_VIRTUAL_OR_FRIEND_ALLOWED 0x4
			/* If this bit is set the declaration specifier may
			   contain the keyword "virtual" or "friend". */
#define DSI_IS_PARAMETER 0x8
			/* If this bit is set the declaration specifier is
			   part of the declaration of a parameter. */
#define DSI_EMPTY_SPECIFIER_ALLOWED 0x10
			/* If this bit is set the declaration specifier may
			   be "empty"; otherwise, at least one specifier
			   is required. */
/* Constants defining bits in the output bit vector, returned from
   declaration_specifiers. */
#define DSO_NO_OUTPUT_FLAGS 0x0
#define DSO_HAS_EXPLICIT_TYPE_SPECIFIER 0x1
			/* If this bit is set the declaration specifier
			   was found to have at least one type specifier. */
#define DSO_CONST_QUALIFIED 0x2
			/* If this bit is set the keyword "const" was found
			   in the qualifiers list. */
#define DSO_VOLATILE_QUALIFIED 0x4
			/* If this bit is set the keyword "volatile" was found
			   in the qualifiers list. */
#define DSO_INLINE 0x8
			/* If this bit is set the function specifier "inline"
			   was found. */
#define DSO_VIRTUAL 0x10
			/* If this bit is set the function specifier "volatile"
			   was found. */
#define DSO_FRIEND 0x20
			/* If this bit is set the declaration specifier
			   "friend" was found. */
#define DSO_DECLARES_SOMETHING 0x40
			/* If this bit is set the declaration specifiers
			   actually declare something (a tag or enumeration
			   members). */
#define DSO_JUST_VOID 0x80
			/* If this bit is set the keyword "void" was found,
			   and nothing else. */

extern a_boolean declaration_specifiers(a_decl_flag_set	input_flags,
					a_decl_flag_set	*output_flags,
					a_storage_class *storage_class,
					a_type_ptr      *type_ptr);

#endif /* DECLS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
