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

extern void instantiate_template_class(a_type_ptr  type);

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
