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

disambig.h -- Declarations related to disambig.c (having to with
              disambiguation of C++ declarations and expressions).

*/

/* Avoid including these declarations more than once: */
#ifndef DISAMBIG_H
#define DISAMBIG_H 1

/*
Macro called in various contexts to distinguish expressions from declarations. 
In C this is straightforward -- is_decl_start() provides all the information
needed.  But the added complexity of disambiguation in C++ requires calling a
routine to do lookahead, etc.
*/
#define is_decl_not_expr(abstract_decl_allowed, real_decl_allowed, single_type_required) \
  ((C_dialect == C_dialect_cplusplus) ?                               \
    (is_decl_start(/*expr_context=*/TRUE, real_decl_allowed) ?        \
      f_is_decl_not_expr(abstract_decl_allowed, real_decl_allowed,    \
                         single_type_required) :                      \
      curr_token == tok_overload) :                                   \
    is_decl_start(/*expr_context=*/TRUE, real_decl_allowed))

extern a_boolean f_is_decl_not_expr(a_boolean  abstract_declarator_allowed,
                                    a_boolean  real_declarator_allowed,
                                    a_boolean  single_type_required);

#endif /* DISAMBIG_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
