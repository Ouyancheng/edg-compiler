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
typedef void an_output_func_declarator_function(a_type_ptr type);
typedef an_output_func_declarator_function
                                       *an_output_func_declarator_function_ptr;
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
	c_generating_back_end;
			/* TRUE if the output is being done for the
			   C-generating back end. */
#if DEBUG
  a_byte_boolean
	debug_output;	/* TRUE if the generated string is part of debug
			   output. */
#endif /* DEBUG */
} an_il_to_str_output_control_block;

/*
Options for form_type_first_part/form_type_second_part, in bit set form.
*/
typedef int a_form_type_options_set;
#define FT_NO_OPTIONS 0
#define FT_ADD_CONST 0x1
			/* Add an extra "const" over the type. */
#define FT_SUPPRESS_CONST 0x2
			/* Suppress top-level "const" on the type. */
#if MICROSOFT_KEYWORDS_ALLOWED
#define FT_SUPPRESS_MICROSOFT_QUALIFIERS 0x4
			/* Suppress the Microsoft qualifiers like __cdecl. */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */


extern void clear_il_to_str_output_control_block(
                                   an_il_to_str_output_control_block_ptr octl);

extern void form_template_args(a_template_arg_ptr                    tap,
                               an_il_to_str_output_control_block_ptr octl);

extern void form_class_qualifier(
                          a_type_ptr                            class_type,
                          an_il_to_str_output_control_block_ptr octl);

extern void form_name(a_source_correspondence               *scp,
                      an_il_entry_kind                      kind,
                      an_il_to_str_output_control_block_ptr octl);

extern char *int_kind_name(an_integer_kind kind);

extern char *float_kind_name(a_float_kind kind);

#ifdef CFE
extern void form_type_qualifier(
                     a_type_ptr                            type,
                     a_boolean                             suppress_const,
                     a_boolean                             need_trailing_space,
                     an_il_to_str_output_control_block_ptr octl);

#endif /* ifdef CFE */

extern void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

extern void form_function_declarator(
                              a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl);

extern void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl);

extern void form_type(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_integer_constant(
                           a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl);

extern void form_char(char                                  ch,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_pm_constant(
                      a_constant_ptr                        constant,
                      a_boolean                             minimal_casts,
                      a_boolean                             need_parens,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_constant(a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);

extern void form_reference_init_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl);

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
