/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

cp_gen_be.h - Declarations related to cp_gen_be.c (C++/C-generating back end).

*/

/* Avoid including these declarations more than once: */
#ifndef CP_GEN_BE_H
#define CP_GEN_BE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#if BACK_END_IS_CP_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
extern void back_end(void);
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
extern void cp_gen_be_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

extern a_boolean expr_has_comma_operation(an_expr_node_ptr expr);

extern an_expr_node_ptr skip_implicit_steps(an_expr_node_ptr node);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_CP_GEN_BE */

#endif /* ifndef CP_GEN_BE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
