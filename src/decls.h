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
  a_symbol_ptr	prototype_scope_symbols;
			/* List of symbols in the prototype scope, linked
			   on the next_in_scope field.  NULL if none.
			   Usually NULL.  Only named types (structs/unions/
			   enums) declared within the prototype scope
			   appear on this list. */
  a_param_id_ptr
		param_id_list;
			/* List of entries giving parameter names, NULL if
			   there were none.  Used for both old-style and
			   new-style parameter names. */
  a_scope_number
		scope_number;
			/* The scope number used for the function prototype
			   scope for the parameters, to be reused for the
			   function scope if a body is found. */
  a_byte_boolean
		any_prototype_names_omitted;
			/* TRUE if the parameter list is a prototype list,
			   and it includes at least one parameter with
			   just a type and no name. */
} a_func_info_block;


EXTERN a_param_id_ptr
		avail_param_ids;
			/* List of parameter id entries freed and available
			   for reuse. */

EXTERN a_boolean
		in_old_style_param_decl_list;
			/* When we are inside the declaration list for
			   old-style function parameters, this flag is TRUE. */

typedef struct an_extern_linkage *an_extern_linkage_ptr;
typedef struct an_extern_linkage {
  a_name_linkage_kind
		kind;
			/* The kind of external linkage ("C++" or "C"). */
  a_byte_boolean
		is_explicit;
			/* TRUE if the external linkage requirement is
			   explicitly specified in the source; FALSE for the
			   default set for the translation unit as a whole. */
} an_extern_linkage;

EXTERN an_extern_linkage
		def_external_linkage;
			/* The default external linkage kind (e.g., "C++" or
			   "C" name linkage) for a variable or function at a
			   given point.  For instance, the setting may
			   change from the translation unit default when
			   we are inside the declaration list for a C++
			   linkage specification. */

/* Test whether or not the current token is the start of a type. */
extern a_boolean is_type_start(void);

/* Test whether or not the current token is the start of a declaration. */
extern a_boolean is_decl_start(void);

extern a_boolean is_overload_specifier(void);

extern a_boolean check_for_overload_anachronism(void);

extern a_boolean f_is_decl_not_expr(a_boolean  abstract_declarator_allowed);

/*
Macro called in various contexts to distinguish expressions from declarations. 
In C this is straightforward -- is_decl_start() provides all the information
needed.  But added complexity of disambiguation in C++ requires calling a
routine to do lookahead, etc.
*/
#define is_decl_not_expr(abstract_declarator_allowed)                 \
  ((C_dialect == C_dialect_cplusplus) ?                               \
         (is_decl_start() ?                                           \
                   f_is_decl_not_expr(abstract_declarator_allowed) :  \
                   is_overload_specifier()) :                         \
         is_decl_start())

extern void type_name(a_type_ptr *type_ptr);

extern void new_type_name(a_boolean         is_parenthesized,
                          a_type_ptr        *type_ptr,
                          an_expr_node_ptr  *dimension_expr);

extern a_boolean scan_conversion_operator(a_source_position  *pos);

extern a_type_ptr type_keyword(void);

extern void check_operator_function_params(a_routine_ptr      rout,
                                           a_source_position  *pos);

extern void clear_func_info(a_func_info_block *func_info);

extern void scan_default_arg_expr(a_param_type_ptr ptp);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean is_definition);

extern void local_declaration(void);

extern void translation_unit(void);

extern void free_param_id_list(a_param_id_ptr *pidlist);

extern a_boolean reconcile_external_symbol_types(
                            a_symbol_ptr          ext_sym,
                            a_source_position_ptr position,
                            a_type_ptr            type_ptr,
                            a_boolean             suppress_incompatible_error);

extern a_symbol_ptr curr_tag_symbol(a_symbol_kind tag_kind);

extern a_symbol_ptr curr_scope_tag_symbol(a_symbol_kind tag_kind);

extern a_variable_ptr make_variable(a_type_ptr      type_ptr,
                                    a_storage_class storage_class,
                                    a_boolean       at_file_scope);

extern a_variable_ptr make_param_variable(a_type_ptr       type,
                                          a_storage_class  storage_class);

extern a_variable_ptr make_parameter(a_type_ptr       type,
                                     a_storage_class  storage_class,
                                     a_symbol_ptr     sym);

extern a_routine_ptr make_routine(a_type_ptr      type_ptr,
                                  a_storage_class storage_class,
                                  a_boolean       at_file_scope);

extern a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                       a_symbol_locator *locator,
                                       a_boolean        at_file_scope,
                                       a_boolean        suppress_redecl_error);

extern void decl_typedef(a_symbol_locator   *locator,
                         a_type_ptr         type_ptr,
                         a_symbol_ptr       *symbol_ptr);

extern void inline_function_definition(a_routine_ptr     routine_ptr,
                                       a_func_info_block *func_info);

extern void decl_var_or_routine(a_symbol_locator    *locator,
                                a_storage_class     storage_class,
                                a_type_ptr          type_ptr,
                                a_boolean           is_implicit_function,
                                a_boolean           is_function_def_with_body,
                                a_boolean           inline_specified,
                                a_symbol_ptr        *symbol_ptr,
                                an_id_linkage_kind  *linkage_ptr,
                                a_type_ptr          *old_type,
                                a_symbol_ptr        *ext_sym);

/* Bit vector used to pass flags into declarator and into and out of
   decl_specifiers.  Each bit represents a flag. */
typedef int a_decl_flag_set;
/* Constants defining bits in the input bit vector used in calls to
   declarator. */
#define DI_NO_INPUT_FLAGS 0x0
#define DI_REAL_DECLARATOR_ALLOWED 0x1
			/* If this bit is set the entity may be scanned as an
			   declarator (rather than an abstract declarator). */
#define DI_ABSTRACT_DECLARATOR_ALLOWED 0x2
			/* If this bit is set the entity may be scanned as an
			   abstract declarator. */
#define DI_QUALIFIED_NAME_ALLOWED 0x4
			/* If this bit is set it a qualified name is not
			   in the declarator (e.g., for a formal parameter or
			   a typedef declaration). */
#define DI_PARENTHESIZED_INITIALIZER_ALLOWED 0x8
			/* If this bit is set a declarator may be followed
			   by an initializer using the "(expr-list)"
			   notation (ARM 8.4). */
#define DI_DESTRUCTOR_SPECIFIERS 0x10
			/* If this bit is set decl_specifiers has seen a "~"
			   and determined that specifiers preceding it, if any,
			   are consistent with a destructor declaration.  */
#define DI_NONSTATIC_MEMBER 0x20
			/* If this bit is set the declarator is for a class
			   member declared within a class definition without
			   a "static" type specifier.  If the name turns out
			   to be a function name, it will thus be that of
			   a nonstatic member function.  This is of importance
			   to function_declarator in creating the implicit
			   this param type entry for such functions. */
#define DI_IS_CONSTRUCTOR 0x40
			/* If this bit is set decl_specifiers has determined
			   that the declaration is that of a constructor. */
#define DI_DIMENSION_EXPRESSION_ALLOWED 0x80
			/* If this bit is set the first dimension of an array
			   declarator may be a nonconstant expression. */
/* Constants defining bits in the output bit vector used in calls to
   declarator. */
#define DO_NO_OUTPUT_FLAGS 0x0
#define DO_PARENTHESIZED_INITIALIZER 0x1
			/* If this bit was set the declarator appears to be
			   followed by a parenthesized initializer. */
#define DO_REAL_DECLARATOR_SCANNED 0x2
			/* If this bit was set a name was scanned, indicating
			   a real, not abstract, declarator. */
/* Constants defining bits in the input bit vector used in calls to
   decl_specifiers. */
#define DSI_NO_INPUT_FLAGS 0x0
#define DSI_STORAGE_CLASS_SPECIFIER_ALLOWED 0x1
			/* If this bit is set the declaration specifiers may
			   include a storage class keyword. */
#define DSI_TYPE_SPECIFIER_ALLOWED 0x2
			/* If this bit is set the declaration specifiers may
			   include a type specifier. */
#define DSI_IS_MEMBER_DECLARATION 0x4
			/* If this bit is set the declaration is that of a
			   class member. */
#define DSI_IS_PARAMETER 0x8
			/* If this bit is set the declaration specifiers are
			   part of the declaration of a parameter. */
#define DSI_EMPTY_DECL_SPECIFIERS_ALLOWED 0x10
			/* If this bit is set suppress an error when an
			   non-type-name identifier is found before the first
			   specifier.  Simply set the default type and return
			   a flag signaling that there are no specifiers. */
#define DSI_SUPPRESS_MISSING_TYPE_SPEC_WARNING 0x20
                        /* If this bit is set do not issue a warning on a
                           missing type specifier. */
#define DSI_INLINE_ALLOWED 0x40
			/* If this bit is set allow an inline specifier. */
#define DSI_IS_NEW_TYPE_NAME 0x80
			/* If this bit is set the declaration specifiers are
			   part of the type declaration associated with a
			   "new" operator. */
/* Constants defining bits in the output bit vector returned from
   decl_specifiers. */
#define DSO_NO_OUTPUT_FLAGS 0x0
#define DSO_HAS_EXPLICIT_TYPE_SPECIFIER 0x1
			/* If this bit is set the declaration specifiers
			   were found to have at least one type specifier. */
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
#define DSO_DEFINES_SOMETHING 0x80
			/* If this bit is set the declaration specifiers
			   actually define something (a class, struct, union,
			   or enumeration). */
#define DSO_JUST_VOID 0x100
			/* If this bit is set the keyword "void" was found,
			   and nothing else. */
#define DSO_DANGLING_TYPE_SPECIFIER 0x200
			/* If this bit is set a malformed type specification
			   was detected, probably caused by a missing
			   semicolon following an class, struct, union, or
			   enum declaration.  Error reporting is left to the
			   caller in such cases. */
#define DSO_NO_DECL_SPECIFIERS 0x400
			/* If this bit is set then no declaration specifiers
			   were found before the first non-type-name
			   identifier was encountered. */
#define DSO_ELABORATED_TYPE_SPECIFIER 0x800
                        /* If this bit is set the declaration specifiers
                           consist of (1) a keyword class, struct, union, or
                           enum and (2) an identifier (and optionally (3) the
                           keyword friend). */
#define DSO_CONSTRUCTOR 0x1000
			/* If this bit is set the declaration is for a
			   constructor, in which case the type returned from
			   decl_specifiers is tk_void. */
#define DSO_DESTRUCTOR 0x2000
			/* If this bit is set the declaration appears to be
                           that of a destructor (a "~" was seen, and the
                           specifiers, if any, are consistent with those
			   allowed on a destructor declaration), and so a type
                           of tk_void was returned. */

extern void declarator(a_decl_flag_set   input_flags,
                       a_decl_flag_set   *output_flags,
		       a_type_ptr        specifiers_type,
                       a_type_ptr        member_parent_type,
                       a_symbol_locator  *locator,
                       a_type_ptr        *p_complete_type,
                       a_type_ptr        *p_bottom_derived_type,
                       a_func_info_block *func_info,
                       an_expr_node_ptr  *dim_expr_ptr);

extern a_boolean decl_specifiers(a_decl_flag_set      input_flags,
				 a_decl_flag_set      *output_flags,
				 a_storage_class      *storage_class,
				 a_type_ptr           *type_ptr);

#endif /* DECLS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
