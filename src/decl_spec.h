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

typedef struct an_extended_decl_info_block {
  a_type_qualifier_set
		qualifiers;
  a_decl_modifiers_block
		decl_modifiers;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_inheritance_kind
		inheritance_kind;
  a_source_position
		inheritance_kind_pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} an_extended_decl_info_block;

#if MICROSOFT_EXTENSIONS_ALLOWED
#define clear_extended_decl_info_block(block)                           \
  { (block).qualifiers = TQ_NONE;                                       \
    clear_decl_modifiers_block(&((block).decl_modifiers));              \
    (block).inheritance_kind = (an_inheritance_kind)ihk_none;           \
    (block).inheritance_kind_pos = null_source_position;                \
  }
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define clear_extended_decl_info_block(block)                           \
  { (block).qualifiers = TQ_NONE;					\
    clear_decl_modifiers_block(&((block).decl_modifiers));              \
  }
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
extern void scan_extended_decl_modifiers(
                            a_boolean                    is_class_decl,
                            a_boolean                    is_member_decl,
                            an_extended_decl_info_block  *extended_decl_info,
                            a_boolean                    *err);

extern void scan_and_discard_extended_decl_modifiers(void);

extern void update_extended_decl_info_for_class(
                            a_type_ptr                   class_type,
                            an_extended_decl_info_block  *extended_decl_info,
                            a_source_position            *err_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void check_inheritance_kind(a_type_ptr           class_type,
                                   an_inheritance_kind  inheritance_kind,
                                   a_source_position    *err_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void typename_specifier(a_type_ptr            *type_ptr,
                               a_boolean             within_using_decl,
                               a_decl_pos_block_ptr  decl_pos_block);

extern a_boolean is_constructor_decl(a_type_ptr  class_type);

extern a_boolean decl_specifiers(a_decl_flag_set             input_flags,
                                 a_decl_flag_set             *output_flags,
                                 a_storage_class             *storage_class,
                                 a_type_ptr                  *type_ptr,
                                 a_type_qualifier_set        *qualifiers,
                                 an_attribute_ptr            *attributes,
                                 a_decl_modifiers_block_ptr  decl_modifiers,
                                 a_decl_pos_block_ptr        decl_pos_block,
                                 a_upc_block_size            *upc_block_size);

extern void set_name_linkage_for_type(a_type_ptr  tp);

extern void decl_spec_one_time_init(void);

/* Constants defining bits in the input bit vector used in calls to
   decl_specifiers. */
#define DSI_NO_INPUT_FLAGS ((a_decl_flag_set)0x0)
#define DSI_STORAGE_CLASS_SPECIFIER_ALLOWED ((a_decl_flag_set)0x1)
			/* If this bit is set the declaration specifiers may
			   include a storage class keyword. */
#define DSI_TYPE_SPECIFIER_ALLOWED ((a_decl_flag_set)0x2)
			/* If this bit is set the declaration specifiers may
			   include a type specifier. */
#define DSI_IS_MEMBER_DECLARATION ((a_decl_flag_set)0x4)
			/* If this bit is set the declaration is that of a
			   class member, inside the class. */
#define DSI_IS_PARAMETER ((a_decl_flag_set)0x8)
			/* If this bit is set the declaration specifiers are
			   part of the declaration of a parameter. */
#define DSI_EMPTY_DECL_SPECIFIERS_ALLOWED ((a_decl_flag_set)0x10)
			/* If this bit is set suppress an error when an
			   non-type-name identifier is found before the first
			   specifier.  Simply set the default type and return
			   a flag signaling that there are no specifiers. */
#define DSI_INLINE_ALLOWED ((a_decl_flag_set)0x20)
			/* If this bit is set allow an inline specifier. */
#define DSI_IS_NEW_TYPE_NAME ((a_decl_flag_set)0x40)
			/* If this bit is set the declaration specifiers are
			   part of the type declaration associated with a
			   "new" operator. */
#define DSI_VACUOUS_TAG_DECL_ALLOWED ((a_decl_flag_set)0x80)
			/* If this bit is set a class, struct, union, or enum
			   declaration with no associated definition may be
			   interpreted as introducing a new tag name, not
			   referring to an existing one from outer scope. */
#define DSI_IS_TEMPLATE_PARAMETER ((a_decl_flag_set)0x100)
			/* If this bit is set decl_specifiers is called for
			   a template parameter declaration. */
#define DSI_IS_TEMPLATE_DECLARATION ((a_decl_flag_set)0x200)
			/* If this bit is set decl_specifiers is called for
			   a template class or template function
                           declaration. */
#define DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS ((a_decl_flag_set)0x400)
			/* If this bit is set decl_specifiers is called to
			   scan a list of type qualifiers in the
			   context of a pointer declarator.  When a token
			   other than a type qualifier is seen, return
			   immediately, without issuing any diagnostics. */
#define DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER ((a_decl_flag_set)0x800)
			/* If this bit is set decl_specifiers will do special
			   checking for a "dangling type specifier" -- an
			   identifier that may belong to a type specifier
			   of a subsequent declaration because a ";" is
			   missing. */
#define DSI_IS_OLD_STYLE_PARAM_DECL ((a_decl_flag_set)0x1000)
			/* If this bit is set decl_specifiers is being called
			   for an old-style parameter declaration.  Some error
			   checking is affected. */
#define DSI_ASM_ALLOWED ((a_decl_flag_set)0x2000)
			/* If this bit is set "asm" is recognized as a decl-
			   specifier.  Used only when ASM_FUNCTION_ALLOWED is
			   TRUE. */
#define DSI_IS_LINKAGE_SPEC_DECL ((a_decl_flag_set)0x4000)
			/* If this bit is set the declaration belongs to
			   a non-brace-enclosed linkage specification. */
#define DSI_IS_CONDITION_DECL ((a_decl_flag_set)0x8000)
			/* If this bit is set the declaration is that of a
			   C++ condition in an if, switch, for, or while
			   statement. */
#define DSI_IS_EXPLICIT_INSTANTIATION ((a_decl_flag_set)0x10000)
			/* If this bit is set the declaration is that of a
			   C++ explicit template instantiation directive. */
#define DSI_IS_SPECIALIZATION ((a_decl_flag_set)0x20000)
			/* If this bit is set the declaration is that of a
			   C++ template specialization. */
#define DSI_MARKED_AS_GNU_EXTENSION ((a_decl_flag_set)0x40000)
			/* If this bit is set the declaration was preceded by
			   the GNU keyword __extension__. */
#define DSI_LAST DSI_MARKED_AS_GNU_EXTENSION
			/* Last bit in the bit vector that is in use. */
			/*lint -esym(755,DSI_LAST)*/

/* Constants defining bits in the output bit vector returned from
   decl_specifiers. */
#define DSO_NO_OUTPUT_FLAGS ((a_decl_flag_set)0x0)
#define DSO_HAS_EXPLICIT_TYPE_SPECIFIER	\
				((a_decl_flag_set)0x1)
			/* If this bit is set the declaration specifiers
			   were found to have at least one type specifier. */
#define DSO_INLINE 		((a_decl_flag_set)0x2)
			/* If this bit is set the function specifier "inline"
			   was found. */
#define DSO_VIRTUAL 		((a_decl_flag_set)0x4)
			/* If this bit is set the function specifier "virtual"
			   was found. */
#define DSO_FRIEND		 ((a_decl_flag_set)0x8)
			/* If this bit is set the declaration specifier
			   "friend" was found. */
#define DSO_DECLARES_SOMETHING	((a_decl_flag_set)0x10)
			/* If this bit is set the declaration specifiers
			   actually declare something (a tag or enumeration
			   members). */
#define DSO_DEFINES_SOMETHING	((a_decl_flag_set)0x20)
			/* If this bit is set the declaration specifiers
			   actually define something (a class, struct, union,
			   or enumeration). */
#define DSO_JUST_VOID 		((a_decl_flag_set)0x40)
			/* If this bit is set the keyword "void" was found,
			   and nothing else. */
#define DSO_DANGLING_TYPE_SPECIFIER	\
				((a_decl_flag_set)0x80)
			/* If this bit is set a malformed type specification
			   was detected, probably caused by a missing
			   semicolon following an class, struct, union, or
			   enum declaration.  Error reporting is left to the
			   caller in such cases. */
#define DSO_NO_DECL_SPECIFIERS	 ((a_decl_flag_set)0x100)
			/* If this bit is set then no declaration specifiers
			   were found before the first non-type-name
			   identifier was encountered. */
#define DSO_ELABORATED_TYPE_SPECIFIER	\
				((a_decl_flag_set)0x200)
                        /* If this bit is set the declaration specifiers
                           consist of (1) a keyword class, struct, union, or
                           enum and (2) an identifier (and optionally (3) the
                           keyword friend). */
#define DSO_CONSTRUCTOR 	((a_decl_flag_set)0x400)
			/* If this bit is set the declaration is for a
			   constructor, in which case the type returned from
			   decl_specifiers is tk_void. */
#define DSO_DESTRUCTOR 		((a_decl_flag_set)0x800)
			/* If this bit is set the declaration appears to be
                           that of a destructor (a "~" was seen, and the
                           specifiers, if any, are consistent with those
			   allowed on a destructor declaration), and so a type
                           of tk_void was returned. */
#define DSO_MUTABLE		((a_decl_flag_set)0x1000)
			/* If this bit is set the storage class "mutable" was
			   found. */
#define DSO_EXPLICIT		((a_decl_flag_set)0x2000)
			/* If this bit is set the specifier "explicit" was
			   found. */
#define DSO_LINKAGE_SPEC_DECL   ((a_decl_flag_set)0x4000)
                        /* If this bit is set the decl-specifiers included a
                           linkage specifier; this is only accepted in
                           Microsoft mode (and only under restricted
                           circumstances). */
#define DSO_TYPENAME		((a_decl_flag_set)0x8000)
			/* This bit is set if and only if the keyword typename
			   introduced an elaborated type specifier (i.e.,
			   DSO_ELABORATED_TYPE_SPECIFIER must also be set). */
#define DSO_LAST DSO_TYPENAME
			/* Last bit in the bit vector that is in use. */
			/*lint -esym(755,DSO_LAST)*/
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
