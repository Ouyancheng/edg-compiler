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
#ifndef DECLARATOR_H
#define DECLARATOR_H 1

#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */

/*
Macro that is TRUE if the current token is the start of a declarator
(3.5.4 -- real, not abstract).
*/
#define is_declarator_start()                                        \
  (curr_token == tok_identifier ?                                    \
     (C_mode() || !identifier_is_template_id()) :                    \
     (curr_token == tok_star || curr_token == tok_lparen ||          \
      (C_dialect == C_dialect_cplusplus &&                           \
       (curr_token == tok_ampersand || curr_token == tok_operator))))


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
#define DI_IS_TEMPLATE_DECLARATION 0x100
			/* If this bit is set declarator is called for a
			   declaration of a template function or a template
			   static data member. */
#define DI_IS_TYPEDEF_DECLARATION 0x200
			/* If this bit is set a storage class of "typedef" has
			   been encountered. */
#define DI_OPERATOR_NAME_ALLOWED 0x400
			/* If this bit is set an operator name (e.g.,
			   "operator+" or "operator int") is allowed as the
			   declarator identifier. */
#define DI_IS_FRIEND_DECL 0x800
			/* If this bit is set the declarator is part of a
			   friend declaration. */
#define DI_LAST DI_IS_FRIEND_DECL
			/* Last bit in the bit vector that is in use. */
/* Constants defining bits in the output bit vector used in calls to
   declarator. */
#define DO_NO_OUTPUT_FLAGS 0x0
#define DO_PARENTHESIZED_INITIALIZER 0x1
			/* If this bit was set the declarator appears to be
			   followed by a parenthesized initializer. */
#define DO_REAL_DECLARATOR_SCANNED 0x2
			/* If this bit was set a name was scanned, indicating
			   a real, not abstract, declarator. */
#define DO_CFRONT_MEMBER_FUNCTION_TYPEDEF 0x4
			/* If this bit was set a nonstandard member-function
			   typedef declaration was seen; these are recognized
			   in cfront-compatibility mode only. */
#define DO_CLASS_SCOPE_DEACTIVATION_REQUIRED 0x8
			/* If this bit was set a class scope was reactivated
			   to handle a class member declaration.  This flag,
			   used only when declarator is called recursively,
			   lets the caller know that the scope needs to be
			   popped. */
#define DO_LAST DO_CLASS_SCOPE_DEACTIVATION_REQUIRED
			/* Last bit in the bit vector that is in use. */

extern void declarator(a_decl_flag_set   input_flags,
                       a_decl_flag_set   *output_flags,
		       a_type_ptr        specifiers_type,
                       a_type_ptr        member_parent_type,
                       a_symbol_locator  *locator,
                       a_type_ptr        *p_complete_type,
                       a_type_ptr        *p_bottom_derived_type,
                       a_source_sequence_entry_ptr
                                         *declarator_ssep,
                       a_func_info_block *func_info);

extern a_type_ptr pointer_declarator(a_type_ptr  specifiers_type,
			             a_type_ptr  *bottom_pointer_derived_type,
                                     a_boolean   reference_allowed);

extern void array_declarator(a_type_ptr *new_type_ptr,
                             a_boolean  nonconstant_dimension_allowed);

extern void add_to_derived_type_list(a_type_ptr new_type_ptr,
                                     a_type_ptr *derived_type,
                                     a_type_ptr *bottom_derived_type);

#endif /* DECLARATOR_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
