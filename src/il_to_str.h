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
typedef void an_output_token_mode_control_function(a_boolean full_tokens);
typedef an_output_token_mode_control_function
                                    *an_output_token_mode_control_function_ptr;
typedef void an_output_name_function(char *entry, an_il_entry_kind kind);
typedef an_output_name_function *an_output_name_function_ptr;
typedef void an_output_default_arg_function(a_param_type_ptr param);
typedef an_output_default_arg_function *an_output_default_arg_function_ptr;
typedef struct an_il_to_str_output_control_block
                                        *an_il_to_str_output_control_block_ptr;
/* If you add a field here, add it also to
   clear_il_to_str_output_control_block. */
typedef struct an_il_to_str_output_control_block {
  an_output_str_function_ptr
	output_str;	/* Function to output a null-terminated string. */
  an_output_token_mode_control_function_ptr
	token_mode_control;
			/* Calling this function with TRUE indicates that
			   succeeding output_str calls will be outputting
			   one or more complete tokens (that's the initial
			   state).  Calling with FALSE indicates that the
			   strings being output may be partial tokens.
			   NULL if there's no way or need to control the
			   token mode on output. */
  an_output_name_function_ptr
	output_name;
			/* Function to output the name of an entity.  NULL
			   if a default routine should be used. */
  an_output_default_arg_function_ptr
	output_default_arg;
			/* Function to output a default argument of a
			   function given a_param_type.  NULL if default
			   arguments should not be put out. */
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
#if DEBUG
  a_byte_boolean
	debug_output;	/* TRUE if the generated string is part of debug
			   output. */
#endif /* DEBUG */
} an_il_to_str_output_control_block;

extern void clear_il_to_str_output_control_block(
                                   an_il_to_str_output_control_block_ptr octl);

extern void form_template_args(a_template_arg_ptr                    tap,
                               an_il_to_str_output_control_block_ptr octl);

extern void form_name(char                                  *entry,
                      an_il_entry_kind                      kind,
                      an_il_to_str_output_control_block_ptr octl);

extern char *int_kind_name(an_integer_kind kind);

extern char *float_kind_name(a_float_kind kind);

extern void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_boolean                             add_const,
                    an_il_to_str_output_control_block_ptr octl);

extern void form_function_declarator(
                              a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl);

extern void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    an_il_to_str_output_control_block_ptr octl);

extern void form_type(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl);

extern void form_constant(a_constant_ptr                        constant,
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
