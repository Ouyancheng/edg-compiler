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


/* Return the symbol if the current token is a type name identifier. */
a_symbol_ptr curr_type_symbol(a_boolean is_new_type_name,
                              a_boolean in_prescan);

/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define curr_id_is_type_name()						\
  (curr_type_symbol(/*is_new_type_name=*/FALSE, /*in_prescan=*/FALSE) != NULL)

/* Test whether or not the current token is the start of a type. */
extern a_boolean is_type_start(a_boolean is_expr_context);

/* Test whether or not the current token is the start of a declaration. */
extern a_boolean is_decl_start(a_boolean  expr_context,
                               a_boolean  real_declarator_allowed);

extern a_boolean check_member_function_typedef(a_type_ptr         tp,
                                               a_source_position  *pos);

extern a_boolean f_check_for_overload_anachronism(void);

/*
Return TRUE if the current token is "overload" and the declaration following
it is just an identifier (or a comma-list of identifiers).
*/
#define check_for_overload_anachronism()                               \
  (curr_token == tok_overload && f_check_for_overload_anachronism())

/*
Macro that returns TRUE if the current token is an identifier that is
a template-id.
*/
#define identifier_is_template_id()                                  \
  (is_generalized_identifier_start(GID_NO_OPTIONS) ?        \
         locator_for_curr_id.is_template_id : FALSE)

/*
Macro to be used in conjunction with is_type_keyword to check for
Microsoft extensions.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_microsoft_type_keyword(tok)                             \
  || (microsoft_mode &&                                               \
      ((tok) == tok_int8  || (tok) == tok_int16 ||                    \
       (tok) == tok_int32 || (tok) == tok_int64))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_microsoft_type_keyword(tok)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to be used in conjunction with is_type_keyword to check for C99
extensions.
*/
#if C99_IL_EXTENSIONS_SUPPORTED
#define or_is_c99_type_keyword(tok)                                       \
  || (c99_mode &&                                                         \
      ((tok) == tok_c99_bool  ||                                          \
       (tok) == tok_c99_complex || (tok) == tok_c99_imaginary))
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define or_is_c99_type_keyword(tok)                                       \
  || (c99_mode && (tok) == tok_c99_bool)
#endif /* C99_IL_EXTENSIONS_SUPPORTED */


/*
Macro that is TRUE if the indicated token is a type keyword, e.g., int.
If you change this, see also type_keyword.
*/
#define is_type_keyword(tok)                                          \
  ((tok) == tok_void     || (tok) == tok_char     ||                  \
   (tok) == tok_short    || (tok) == tok_int      ||                  \
   (tok) == tok_long     || (tok) == tok_float    ||                  \
   (tok) == tok_double   || (tok) == tok_signed   ||                  \
   (tok) == tok_unsigned || (tok) == tok_wchar_t  ||                  \
   (tok) == tok_bool                                                  \
   or_is_c99_type_keyword(tok)                                        \
   or_is_microsoft_type_keyword(tok))

/*
Macro that is TRUE if the current token is the start of a type
specifier (except for the typedef and friend cases).  (3.5.2)
*/
#define is_type_specifier()                                           \
 (is_type_keyword(curr_token) ||                                      \
  curr_token == tok_struct   || curr_token == tok_union    ||         \
  curr_token == tok_enum     || curr_token == tok_class    ||         \
  curr_token == tok_typename)

/*
Macro that is TRUE if the current token is the start of a type qualifier
(3.5.3).
*/
#define is_type_qualifier() is_type_qualifier_token(curr_token)


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


/*
A collection of source positions passed around during declaration processing.
*/
typedef struct a_decl_pos_block *a_decl_pos_block_ptr;
typedef struct a_decl_pos_block {
  a_source_position
		decl_pos;
			/* Source position of the identifier. */
  a_source_position
		storage_class_pos;
			/* Source position of storage-class, if any. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		identifier_range;
			/* Start and end positions of coalesced identifier. */
  a_source_range
		specifiers_range;
			/* Start and end positions of decl-specifiers. */
  a_source_range
		declarator_range;
			/* Start and end positions of declarator. */
  a_source_range
		var_init_range;
			/* Start and end positions of initializer. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
} a_decl_pos_block;

#if EXTRA_SOURCE_POSITIONS_IN_IL

extern a_decl_position_supplement_ptr make_decl_pos_supplement(
                                        a_boolean             at_file_scope,
                                        a_decl_pos_block_ptr  decl_pos_block);

extern void update_decl_pos_info(a_source_correspondence  *scp,
                                 a_decl_pos_block_ptr     decl_pos_block);

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

extern void clear_decl_pos_block(a_decl_pos_block_ptr  decl_pos_block);

extern a_boolean simplify_curr_class_qualified_name(void);

extern void report_missing_type_specifier(
                                     a_source_position  *err_pos,
                                     a_boolean          is_function,
                                     a_boolean          is_function_def,
                                     a_boolean          is_main_function,
                                     a_boolean          any_decl_specifiers);

/*
Report use of "implicit int" in a declaration.  This macro is not called for
function declarations (call f_report_missing_type_specifiers directly) or
when all decl-specifiers are missing.
*/
#define report_implicit_int(pos)                                      \
  report_missing_type_specifier(pos, /*is_function=*/FALSE,           \
                                /*is_function_def=*/FALSE,            \
                                /*is_main_function=*/FALSE,           \
                                /*any_decl_specifiers=*/TRUE)


extern void type_name_full(a_type_ptr  *type_ptr,
                           a_boolean   *explicit_cv_qualifiers);

/*
Scan a type-name (see 3.5.5) and set *type_ptr to the scanned type.  This is
a short-hand for the common case where we don't care about explicit
cv-qualifiers.
*/
#define type_name(type_ptr)           \
  type_name_full(type_ptr, (a_boolean*)NULL)

extern void new_type_name(a_boolean         is_parenthesized,
                          a_type_ptr        *type_ptr);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr simple_type_specifier_sequence(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
a_boolean scan_conversion_operator(
			a_source_position		*id_pos,
                        a_boolean			is_class_member,
                        a_parent_class_or_namespace_ptr	parent);

extern a_type_ptr type_keyword(void);

extern void adjust_parameter_type(a_type_ptr           *type_ptr,
                                  a_type_qualifier_set array_qualifiers);

extern a_boolean is_single_param_operator_new_or_delete(
                                                   a_symbol_locator *locator,
                                                   a_type_ptr       type);

extern void check_operator_function_params(a_type_ptr        rout_type,
                                           a_type_ptr        class_type,
                                           a_symbol_locator  *locator);

extern void check_and_adjust_parameter_type
                                    (a_type_ptr           *type_ptr,
                                     a_source_position    *error_pos,
                                     a_type_qualifier_set array_qualifiers);

void check_old_specialization_allowed(a_symbol_ptr       sym,
                                      a_source_position  *pos);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean is_definition);

extern a_boolean scan_name_linkage_string(a_name_linkage_kind *kind);

extern void check_main_function(a_func_info_block_ptr  func_info,
                                a_type_ptr             type,
                                a_storage_class        *declared_storage_class,
                                a_boolean              *is_inline,
                                a_source_position_ptr  pos);

extern void declaration(a_boolean       function_definition_allowed,
                        a_boolean       is_old_style_param_decl,
                        a_boolean       is_top_level_declaration,
                        a_param_id_ptr  param_id_list,
                        a_source_range  *linkage_spec_range_ptr);

extern void local_declaration(void);

EXTERN a_boolean
		next_token_is_top_level_decl_start;
			/* Flag toggled in translation_unit when advancing
			   past a token that marks the end of a "top-level"
			   declaration (i.e., a ";" or "}").  When this flag
			   is TRUE, the state of the compiler is in effect
			   between declarations -- or else just before the
			   first declaration or just after the last. */

extern void translation_unit(void);

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
extern void scan_implicitly_included_template_definition_file(void);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

extern a_boolean reconcile_external_symbol_types(
                            a_symbol_ptr          ext_sym,
                            a_source_position_ptr position,
                            a_type_ptr            type_ptr,
                            a_boolean             suppress_incompatible_error);

extern a_routine_ptr make_routine(a_type_ptr      type_ptr,
                                  a_storage_class storage_class,
                                  a_scope_depth   scope_depth);

extern a_using_decl_ptr make_using_decl(a_symbol_ptr      sym,
                                        a_source_position *pos);

extern a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                       a_symbol_locator *locator,
                                       a_scope_depth    scope_level,
                                       a_boolean        suppress_redecl_error);

extern void decl_typedef(a_symbol_locator             *locator,
                         a_type_ptr                   type_ptr,
                         a_type_ptr                   class_type,
                         a_symbol_ptr                 *symbol_ptr,
                         a_source_sequence_entry_ptr  declarator_ssep,
                         a_decl_pos_block_ptr         decl_pos_block);

extern void record_lint_argsused_and_varargs_state(a_symbol_ptr  rout_sym);

extern void record_arg_pragma(a_pending_pragma_ptr  ppp,
                              a_symbol_ptr          sym,
                              a_statement_ptr       sp);

extern void reconcile_routine_types(a_routine_ptr  routine_ptr,
                                    a_type_ptr     type_ptr,
                                    a_boolean      preserve_rout_type,
                                    a_boolean      preserve_type_ptr);

extern void check_exception_specification(a_type_ptr         new_rout_type,
                                          a_symbol_ptr       prev_decl,
                                          a_source_position  *throw_pos,
                                          a_boolean          is_redecl);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern void set_routine_declared_type(a_routine_ptr  routine_ptr,
                                      a_type_ptr     declared_type);

extern a_boolean update_src_seq_secondary_decl(
                                        char                 *il_entry_ptr,
                                        a_type_ptr           declared_type,
                                        an_sssd_flag_set     flags,
                                        a_decl_pos_block_ptr decl_pos_block);

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void decl_routine(a_symbol_locator             *locator,
                         a_storage_class              storage_class,
                         a_type_ptr                   type_ptr,
                         a_func_info_block_ptr        func_info,
                         a_source_sequence_entry_ptr  declarator_ssep,
                         a_symbol_reference_kind      srk_flags,
                         a_decl_modifiers_block_ptr   decl_modifiers,
                         a_symbol_ptr                 *symbol_ptr,
                         an_id_linkage_kind           *linkage_ptr,
                         a_type_ptr                   *old_type,
                         a_symbol_ptr                 *ext_sym,
                         a_decl_pos_block_ptr         decl_pos_block);

void decl_variable(a_symbol_locator             *locator,
                   a_storage_class              storage_class,
                   a_type_ptr                   type_ptr,
                   a_source_sequence_entry_ptr  declarator_ssep,
                   a_symbol_reference_kind      srk_flags,
                   a_decl_modifiers_block_ptr   decl_modifiers,
                   a_symbol_ptr                 *symbol_ptr,
                   an_id_linkage_kind           *linkage_ptr,
                   a_type_ptr                   *old_type,
                   a_symbol_ptr                 *ext_sym,
                   a_decl_pos_block_ptr         decl_pos_block);

extern
void decl_function_template(a_symbol_locator            *locator,
                            a_type_ptr                  type_ptr,
                            a_func_info_block           *func_info,
                            a_symbol_ptr                *symbol_ptr,
                            a_storage_class             storage_class,
                            a_decl_modifiers_block_ptr  decl_modifiers,
                            a_template_decl_info_ptr    templ_decl_info,
                            a_scope_depth               orig_decl_level,
                            a_boolean                   is_specialization);

extern void handler_declaration(a_statement_ptr     sp,
                                a_source_position*  catch_pos);

extern an_asm_entry_ptr asm_declaration(a_boolean  asm_decl_allowed,
                                        a_boolean  is_asm_statement);

extern a_variable_ptr condition_declaration(void);

extern void make_using_directive(a_namespace_ptr    nsp,
				 a_scope_depth	    depth,
                                 a_source_position  *pos,
		   	         a_boolean	    compiler_generated);

/* Bit vector used to pass flags into declarator and into and out of
   declaration routines.  Each bit represents a flag.  (Note that several of
   the bit sets described with this type have more than 16 flags; "unsigned
   long" is used to assure that there is no overflow problem in environments
   where "unsigned int" would be too small.) */
typedef unsigned long a_decl_flag_set;

#if DECL_MODIFIERS_IN_USE
extern
void update_routine_decl_modifiers(a_routine_ptr               routine,
                                   a_decl_modifiers_block_ptr  new_modifiers,
                                   a_source_position           *position,
                                   a_boolean                   is_redecl,
                                   a_boolean                   is_definition,
                                   a_boolean                   is_inline);

extern
void update_variable_decl_modifiers(a_variable_ptr              variable,
                                    a_decl_modifiers_block_ptr  new_modifiers,
                                    a_source_position           *position,
                                    a_boolean                   is_redecl);
#else /* !DECL_MODIFIERS_IN_USE */
/* Define these as macros that expand to nothing. */
#define update_routine_decl_modifiers(a,b,c,d,e,f) /* nothing */
#define update_variable_decl_modifiers(a,b,c,d) /* nothing */
#endif /* !DECL_MODIFIERS_IN_USE */

extern void check_default_args_for_param_type(a_param_type_ptr  ptp,
                                              a_source_position *pos);

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
