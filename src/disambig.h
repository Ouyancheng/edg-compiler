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
