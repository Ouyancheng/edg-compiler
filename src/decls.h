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

/*
Macro that returns TRUE if the current token is an identifier that is
a template-id.
*/
#define identifier_is_template_id()                                  \
  (is_generalized_identifier_start(GID_NO_OPTIONS) ?        \
         locator_for_curr_id.is_template_id : FALSE)

/*
Macro that is TRUE if the current token is the start of a type
specifier (except for the typedef and friend cases).  (3.5.2)
*/
#define is_type_specifier()                                           \
  (curr_token == tok_void     || curr_token == tok_char     ||        \
   curr_token == tok_short    || curr_token == tok_int      ||        \
   curr_token == tok_long     || curr_token == tok_float    ||        \
   curr_token == tok_double   || curr_token == tok_signed   ||        \
   curr_token == tok_unsigned || curr_token == tok_struct   ||        \
   curr_token == tok_union    || curr_token == tok_enum     ||        \
   curr_token == tok_class)

/*
Macro that is TRUE if the current token is the start of a type qualifier
(3.5.3).
*/
#define is_type_qualifier() is_type_qualifier_token(curr_token)

/*
Macro that is TRUE if the current token is the start of a
Microsoft calling convention.
*/
#if MICROSOFT_KEYWORDS_ALLOWED
#define is_microsoft_calling_convention()				\
  (microsoft_mode &&							\
   (curr_token == tok_cdecl ||						\
    curr_token == tok_fastcall ||					\
    curr_token == tok_stdcall))
#else /* MICROSOFT_KEYWORDS_ALLOWED */
/* When microsoft keywords are not allowed simply return FALSE. */
#define is_microsoft_calling_convention() (FALSE)
#endif /* MICROSOFT_KEYWORDS_ALLOWED */


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

extern a_boolean simplify_curr_class_qualified_name(void);

extern void type_name(a_type_ptr *type_ptr);

extern void new_type_name(a_boolean         is_parenthesized,
                          a_type_ptr        *type_ptr);

extern a_boolean scan_conversion_operator(a_source_position  *pos,
				          a_type_ptr         class_type);

extern a_type_ptr type_keyword(void);

extern void adjust_parameter_type(a_type_ptr *type_ptr,
                                  a_boolean  restrict_qualified);

extern void check_operator_arrow_return_type(a_routine_ptr      rout_ptr,
                                             a_boolean          is_expr_use,
                                             a_source_position  *error_pos);

extern void check_operator_function_params(a_type_ptr        rout_type,
                                           a_type_ptr        class_type,
                                           a_symbol_locator  *locator);

extern void check_and_adjust_parameter_type
                                    (a_type_ptr         *type_ptr,
                                     a_source_position  *error_pos,
                                     a_boolean          restrict_qualified);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean is_definition);

extern void declaration(a_boolean      function_definition_allowed,
                        a_boolean      extern_implied,
                        a_boolean      is_old_style_param_decl,
                        a_boolean      is_top_level_declaration,
                        a_param_id_ptr param_id_list);

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

extern a_source_sequence_entry_ptr init_param_source_sequence_sublist(void);

extern void terminate_param_source_sequence_sublist(
                                     a_func_info_block_ptr        func_info,
                                     a_source_sequence_entry_ptr  prev);

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
typedef unsigned long a_decl_flag_set;

#if MICROSOFT_KEYWORDS_ALLOWED
extern void update_microsoft_variable_info(a_variable_ptr  var,
                                           a_decl_flag_set flags);

extern void update_microsoft_routine_info(a_routine_ptr   routine,
                                          a_decl_flag_set flags);
#else /* !MICROSOFT_KEYWORDS_ALLOWED */
/* Supply macros that do nothing when Microsoft keywords are not
   recognized. */
#define update_microsoft_variable_info(x,y) /* nothing */
#define update_microsoft_routine_info(x,y) /* nothing */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */

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
