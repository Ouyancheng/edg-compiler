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
extern a_boolean is_decl_start(a_boolean  expr_context,
                               a_boolean  real_declarator_allowed);

extern a_boolean f_check_for_overload_anachronism(void);

/*
Return TRUE if the current token is "overload" and the declaration following
it is just an identifier (or a comma-list of identifiers).
*/
#define check_for_overload_anachronism()                               \
  (curr_token == tok_overload && f_check_for_overload_anachronism())

extern a_boolean f_is_decl_not_expr(a_boolean  abstract_declarator_allowed,
                                    a_boolean  real_declarator_allowed,
                                    a_boolean  single_type_required);

/*
Macro called in various contexts to distinguish expressions from declarations. 
In C this is straightforward -- is_decl_start() provides all the information
needed.  But added complexity of disambiguation in C++ requires calling a
routine to do lookahead, etc.
*/
#define is_decl_not_expr(abstract_decl_allowed, real_decl_allowed, single_type_required) \
  ((C_dialect == C_dialect_cplusplus) ?                               \
    (is_decl_start(/*expr_context=*/TRUE, real_decl_allowed) ?        \
      f_is_decl_not_expr(abstract_decl_allowed, real_decl_allowed,    \
                         single_type_required) :                      \
      curr_token == tok_overload) :                                   \
    is_decl_start(/*expr_context=*/TRUE, real_decl_allowed))

/*
Macro that is TRUE if the current token is the start of a declarator
(3.5.4 -- real, not abstract).
*/
#define is_declarator_start()                                         \
  (curr_token == tok_identifier || curr_token == tok_star ||          \
   curr_token == tok_lparen ||                                        \
   (C_dialect == C_dialect_cplusplus &&                               \
    (curr_token == tok_ampersand || curr_token == tok_operator)))

extern void type_name(a_type_ptr *type_ptr);

extern void new_type_name(a_boolean         is_parenthesized,
                          a_type_ptr        *type_ptr);

extern a_boolean scan_conversion_operator(a_source_position  *pos,
				          a_type_ptr         class_type);

extern a_type_ptr type_keyword(void);

extern a_boolean check_function_return_type(a_type_ptr         return_type,
                                            a_source_position  *err_pos,
                                            a_boolean          is_expr_use);

extern void adjust_parameter_type(a_type_ptr *type_ptr);

extern void check_operator_function_params(a_type_ptr        rout_type,
                                           a_type_ptr        class_type,
                                           a_symbol_locator  *locator);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean is_definition);

extern void local_declaration(void);

extern void translation_unit(void);

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
extern void scan_implicitly_included_template_definition_file(void);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

extern a_boolean reconcile_external_symbol_types(
                            a_symbol_ptr          ext_sym,
                            a_source_position_ptr position,
                            a_type_ptr            type_ptr,
                            a_boolean             suppress_incompatible_error);

extern a_symbol_ptr scan_tag_name(a_symbol_kind     tag_kind,
                                  a_symbol_locator  *locator,
                                  a_boolean         check_for_vacuous_decl,
                                  a_boolean         is_ref_within_new_expr,
                                  a_scope_depth     *effective_decl_level,
                                  a_boolean         *tag_resolution);

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
                                  a_boolean       at_file_scope,
                                  a_boolean       add_to_list);

extern void fixup_parameters(a_variable_ptr    param_list,
                             a_param_type_ptr  param_type_list);

extern a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                       a_symbol_locator *locator,
                                       a_boolean        at_file_scope,
                                       a_boolean        suppress_redecl_error);

extern void decl_typedef(a_symbol_locator             *locator,
                         a_type_ptr                   type_ptr,
                         a_symbol_ptr                 *symbol_ptr,
                         a_source_sequence_entry_ptr  declarator_ssep);

extern void record_lint_argsused_and_varargs_state(a_symbol_ptr  rout_sym);

extern void record_arg_pragma(a_pending_pragma_ptr  ppp,
                              a_symbol_ptr          sym,
                              a_statement_ptr       sp);

extern void inline_function_definition(a_routine_ptr     routine_ptr,
                                       a_func_info_block *func_info);

extern void reconcile_routine_types(a_routine_ptr  routine_ptr,
                                    a_type_ptr     type_ptr,
                                    a_boolean      preserve_rout_type,
                                    a_boolean      preserve_type_ptr);

extern void add_exception_specification(a_func_info_block_ptr  func_info,
                                        a_routine_ptr          rp);


extern void check_exception_specification(a_func_info_block_ptr  func_info,
                                          a_routine_ptr          rp);


#if GENERATE_SOURCE_SEQUENCE_LISTS
extern void set_src_seq_secondary_decl_type(char        *il_entry_ptr,
                                            a_type_ptr  type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void decl_var_or_routine(a_symbol_locator             *locator,
                                a_storage_class              storage_class,
                                a_type_ptr                   type_ptr,
                                a_func_info_block_ptr        func_info,
                                a_source_sequence_entry_ptr  declarator_ssep,
                                a_symbol_reference_kind      srk_flags,
                                a_symbol_ptr                 *symbol_ptr,
                                an_id_linkage_kind           *linkage_ptr,
                                a_type_ptr                   *old_type,
                                a_symbol_ptr                 *ext_sym);

extern void decl_function_template(a_symbol_locator    *locator,
                                   a_type_ptr          type_ptr,
                                   a_func_info_block   *func_info,
                                   a_symbol_ptr        *symbol_ptr,
                                   a_storage_class     storage_class);

extern void handler_declaration(a_statement_ptr     sp,
                                a_source_position*  catch_pos);

extern an_asm_entry_ptr asm_declaration(a_boolean  asm_decl_allowed,
                                        a_boolean  is_asm_statement);

/* Bit vector used to pass flags into declarator and into and out of
   declaration routines.  Each bit represents a flag. */
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

extern a_boolean is_constructor_decl(a_type_ptr  class_type);

extern a_boolean decl_specifiers(a_decl_flag_set      input_flags,
				 a_decl_flag_set      *output_flags,
				 a_storage_class      *storage_class,
				 a_type_ptr           *type_ptr);

/* Constants defining bits in the input bit vector used in calls to
   scan_function_body. */
#define SFB_NO_FLAGS 0x0
#define SFB_IMPLICITLY_DECLARED_RETURN_TYPE 0x1
			/* If this bit is set the return type was not
			   explicitly declared (and is "int" by default). */
#define SFB_NO_CLASS_REACTIVATION 0x2
			/* If this bit is set the scope for the parent
			   class of a member function has already been
			   reactivated. */
#define SFB_NEW_STRUCT_STMT_STACK_REQUIRED 0x4
			/* If this bit is set the function definition may be
			   within a statement context -- e.g., an inline
			   member function of a local class or an inline
			   template function being instantiated "on demand".
			   In such cases the structured statement stack should
			   be reinitialized, and then restored once the
			   function definition is complete. */
#define SFB_OLD_STYLE_PARAM_DECL 0x8
			/* If this bit is set the declaration defining the
			   function contains old-style parameter declarations.
			   This may be true even in a case like this:
			     void f(int,int);
			     void f(i,j) int i; int j { ... }
			   where the type associated with the routine entry is
			   marked as prototyped but the defining declaration
			   is old-style. */
#define SFB_IS_INSTANTIATION 0x10
			/* If this bit is set the definition is being generated
			   by the compiler based on a template. */

extern void scan_function_body(a_routine_ptr      rout_ptr,
                               a_func_info_block  *func_info,
                               a_decl_flag_set    flags);

#endif /* DECLS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
