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

decl_spec.h -- Declarations related to decl_spec.c (having to with
               scanning of declaration specifiers).

*/

/* Avoid including these declarations more than once: */
#ifndef DECL_SPEC_H
#define DECL_SPEC_H 1

#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */

extern a_boolean is_constructor_decl(a_type_ptr  class_type);

extern a_boolean decl_specifiers(a_decl_flag_set      input_flags,
				 a_decl_flag_set      *output_flags,
				 a_storage_class      *storage_class,
				 a_type_ptr           *type_ptr);

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
#define DSI_VACUOUS_TAG_DECL_ALLOWED 0x100
			/* If this bit is set a class, struct, union, or enum
			   declaration with no associated definition may be
			   interpreted as introducing a new tag name, not
			   referring to an existing one from outer scope. */
#define DSI_IS_TEMPLATE_PARAMETER 0x200
			/* If this bit is set decl_specifiers is called for
			   a template parameter declaration. */
#define DSI_IS_TEMPLATE_DECLARATION 0x400
			/* If this bit is set decl_specifiers is called for
			   a template class or template function
                           declaration. */
#define DSI_COLLECT_TYPE_QUALIFIERS 0x800
			/* If this bit is set decl_specifiers is called to
			   scan a list of type qualifiers -- e.g., in the
			   context of a pointer declarator.  What a token
			   other than a type qualifier is seen, return
			   immediately, without issuing any diagnostics. */
#define DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER 0x1000
			/* If this bit is set decl_specifiers will do special
			   checking for a "dangling type specifier" -- an
			   identifier that what may belong to a type specifier
			   of a subsequent declaration because a ";" is
			   missing. */
#define DSI_IS_OLD_STYLE_PARAM_DECL 0x2000
			/* If this bit is set decl_specifiers is being called
			   for an old-style parameter declaration.  Some error
			   checking is affected. */
#define DSI_LAST DSI_IS_OLD_STYLE_PARAM_DECL
			/* Last bit in the bit vector that is in use. */
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
#define DSO_CLASS_TEMPLATE 0x4000
			/* If this bit is set the declaration appears to be
			   that of a class template. */
#define DSO_LAST DSO_CLASS_TEMPLATE
			/* Last bit in the bit vector that is in use. */

#endif /* DECL_SPEC_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
