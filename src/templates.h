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

templates.h -- Declarations relating to templates.c (template support)

*/

/* Avoid including these declarations more than once: */
#ifndef TEMPLATES_H
#define TEMPLATES_H 1

#include "il.h"
#include "symbol_tbl.h"

extern void template_declaration(void);

extern a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                        a_template_arg_ptr  template_arg_list,
                                        a_source_position   *source_pos);

extern a_boolean matches_template_type(a_type_ptr          type,
                                       a_type_ptr          templ_type,
                                       a_template_arg_ptr  *templ_arg_list);

extern a_symbol_ptr make_template_function(a_symbol_ptr        templ_sym,
                                           a_type_ptr          rout_type,
                                           a_template_arg_ptr  templ_arg_list,
                                           a_source_position   *source_pos);

extern a_symbol_ptr find_template_function
                                  (a_symbol_ptr        function_template_sym,
                                   a_type_ptr          curr_type,
                                   a_source_position   *source_pos);

extern void instantiate_template_class(a_type_ptr  type);

extern void instantiate_template_function(a_routine_ptr   rout,
                                          a_token_cache   *token_cache,
                                          a_scope_number  scope_number);

/* Macro to call instantiate_template_class if tp is plausibly a class
   in need of instantiation or an array whose underlying element type is such
   a class.  Most of the checking is left to the function. */
#define check_for_uninstantiated_template_class(tp)                    \
{ if (C_dialect == C_dialect_cplusplus &&                              \
      is_incomplete_type(tp)) instantiate_template_class(tp); }

#endif /* TEMPLATES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
