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

il_to_str.h -- Declarations related to il_to_str.c (produce an external
               string-form representation for various IL entries).

*/

/* Avoid including these declarations more than once. */
#ifndef IL_TO_STR_H
#define IL_TO_STR_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */


/*
Block describing how to do output from within the il_to_str routines.
*/
typedef void an_output_str_function(char *str);
typedef an_output_str_function *an_output_str_function_ptr;
typedef void an_output_name_function(char *entry, an_il_entry_kind kind);
typedef an_output_name_function *an_output_name_function_ptr;
typedef void an_output_class_qualifier_function(a_type_ptr type);
typedef an_output_class_qualifier_function
                                       *an_output_class_qualifier_function_ptr;
typedef void an_output_func_declarator_function(a_type_ptr type);
typedef an_output_func_declarator_function
                                       *an_output_func_declarator_function_ptr;
typedef void an_output_expression_function(an_expr_node_ptr expr);
typedef an_output_expression_function *an_output_expression_function_ptr;
typedef void an_output_temp_name_function(char *entry);
typedef an_output_temp_name_function *an_output_temp_name_function_ptr;
typedef struct an_il_to_str_output_control_block
                                        *an_il_to_str_output_control_block_ptr;
/* If you add a field here, add it also to
   clear_il_to_str_output_control_block. */
typedef struct an_il_to_str_output_control_block {
  an_output_str_function_ptr
	output_str;	/* Function to output a null-terminated string, where
			   the string consists of one of more complete
			   tokens. */
  an_output_str_function_ptr
	output_partial_token_str;
			/* Function to output a null-terminated string, where
			   the string may be only part of a token.  If NULL,
			   the output_str routine is used (implying that
			   for the kind of output being done token boundaries
			   don't matter). */
  an_output_name_function_ptr
	output_name;
			/* Function to output the name of an entity.  NULL
			   if a default routine should be used. */
  an_output_name_function_ptr
	output_template_name;
			/* Function to output the name of a template.  NULL
			   if a default routine should be used. */
  an_output_class_qualifier_function_ptr
	output_class_qualifier;
			/* Function to output a class qualifier, e.g.,
			   "A::B::".  NULL if a default routine should be
			   used. */
  an_output_temp_name_function_ptr
	output_temp_name;
			/* Function to output a compiler-generated name based
			   on the address passed in.  Used by the C-generating
			   back end.  NULL if not needed. */
  an_output_func_declarator_function_ptr
	output_func_declarator;
			/* Function to output a function declarator from
			   a function type.  NULL if a default routine
			   should be used. */
  an_output_expression_function_ptr
	output_expression;
			/* Function to output expressions in various contexts
			   (e.g., VLA declarators).  NULL if a default routine
			   should be used (it puts out non-compilable code). */
  a_byte_boolean
	gen_compilable_code;
			/* TRUE if the generated string is intended to be
			   compiled later; FALSE means it's intended to
			   be read by humans.  One reason for this flag:
			   compilable code has to avoid bugs of the target
			   compilers even if it makes the output less
			   readable. */
  a_byte_boolean
	gen_pcc_code;	/* TRUE if the generated string should be old-style
			   pcc/K&R C.  Used by the C-generating back end. */
  a_byte_boolean
	suppress_local_typedefs;
			/* Suppress function-local typedefs in type output,
			   i.e., skip over them and don't show them in the
			   output. */
  a_byte_boolean
	suppress_not_yet_defined_typedefs;
			/* Suppress typedefs that have not yet been defined,
			   according to the typedef_definition_has_been_put_out
			   flag.  Used by the C++-generating back end for
			   references in template arguments of instantiations
			   that have been promoted out of their enclosing
			   nested context. */
  a_byte_boolean
	render_c99_bool;
			/* TRUE if the C99 boolean type should be rendered as
			   "_Bool".  Otherwise, the underlying type is used. */
  a_byte_boolean
	c_generating_back_end;
			/* TRUE if the output is being done for the
			   C-generating back end. */
#if DEBUG
  a_byte_boolean
	debug_output;	/* TRUE if the generated string is part of debug
			   output. */
#endif /* DEBUG */
  a_byte_boolean
	force_qualified_name;
			/* Set to TRUE by the il_to_str routines when calling
			   the output_name routine, to indicate that the
			   output must be a qualified name even if the output
			   routine thinks it could go out as an unqualified
			   name because of the context (used, e.g., for
			   pointers-to-members). */
  a_byte_boolean
	gen_vla_array_as_asterisk_bound_array;
			/* TRUE if the bounds for variable-length arrays should
			   be put out as "[*]".  This is needed to suppress
			   VLAs in function declarations. */
  a_byte_boolean
	gen_raw_tab_in_literals;
			/* TRUE if a tab character should be emitted as an
			   actual tab character rather than as '\t'. */
} an_il_to_str_output_control_block;

/*
Options for form_type_first_part/form_type_second_part, in bit set form.
*/
typedef int a_form_type_options_set;
#define FTO_NO_OPTIONS 0
#define FTO_SUPPRESS_CONST 0x1
			/* Suppress top-level "const" on the type. */
#define FTO_SUPPRESS_SPECIFIERS 0x2
			/* Suppress the type specifiers of the type (put out
			   only the declarator). */


extern void clear_il_to_str_output_control_block(
                                   an_il_to_str_output_control_block_ptr octl);

extern void form_a_template_arg(a_template_arg_ptr                    tap,
                                an_il_to_str_output_control_block_ptr octl);

extern void form_template_args(a_template_arg_ptr                    tap,
                               an_il_to_str_output_control_block_ptr octl);

extern void form_class_or_namespace_qualifier(
                         a_boolean                             is_class_member,
                         a_parent_class_or_namespace           parent,
                         an_il_to_str_output_control_block_ptr octl);

extern void form_name(a_source_correspondence               *scp,
                      an_il_entry_kind                      kind,
                      an_il_to_str_output_control_block_ptr octl);

extern
void form_unqualified_name(a_source_correspondence               *scp,
                           an_il_entry_kind                      entry_kind,
                           an_il_to_str_output_control_block_ptr octl);

extern char *int_kind_name_full(an_integer_kind kind,
                                a_boolean       for_generated_code);

extern char *int_kind_name(an_integer_kind kind);

extern char *int_type_name(a_type_ptr type);

extern char *float_kind_name(a_float_kind kind);

#ifdef CFE
extern void form_type_qualifier(
                     a_type_qualifier_set                  qualifiers,
                     a_boolean                             need_trailing_space,
                     an_il_to_str_output_control_block_ptr octl);
#endif /* ifdef CFE */

extern void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_type_qualifier_set                  added_qualifiers,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

/* Macro used to call form_type_first_part for the simple cases. */
#define form_type_first_part_simple(type, lhs_decl, tr_space, octl)   \
  form_type_first_part(type, lhs_decl, tr_space, TQ_NONE, FTO_NO_OPTIONS, octl)

extern void form_function_declarator(
                              a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl);

extern void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

/* Macro used to call form_type_second_part for the simple cases. */
#define form_type_second_part_simple(type, lhs_decl, octl)            \
  form_type_second_part(type, lhs_decl, FTO_NO_OPTIONS, octl)

extern void form_type(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_integer_constant(
                           a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl);

extern int form_char(char                                  ch,
                     an_il_to_str_output_control_block_ptr octl);

extern void form_pm_constant(
                      a_constant_ptr                        constant,
                      a_boolean                             minimal_casts,
                      a_boolean                             need_parens,
                      an_il_to_str_output_control_block_ptr octl);

#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
/* Return TRUE if the given constant has associated information about
   the expression it came from and should be put out in expression
   form.  Don't be fooled by enumeration constants, which also have a
   non-NULL expression pointer if they were given an explicit value in
   their definitions, but shouldn't be put out in expression form. */
#define constant_should_be_put_out_as_expr(constant) \
  ((constant)->expr != NULL && !is_enum_constant(constant))
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */

extern void form_uuidof_reference(
                           a_type_ptr                            uuid_type,
                           an_il_to_str_output_control_block_ptr octl);

extern void form_unknown_function_constant(
                             a_constant_ptr                        constant,
                             an_il_to_str_output_control_block_ptr octl);

extern void form_constant(a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);

extern void form_lvalue_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);


#if BACK_END_IS_CP_GEN_BE

extern void remap_template_param(a_template_param_coordinate_ptr  coord,
                                 a_source_correspondence_ptr      scp);

extern void unmap_template_param(a_template_param_coordinate_ptr  coord);

#endif /* BACK_END_IS_CP_GEN_BE */

#endif /* ifndef IL_TO_STR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
