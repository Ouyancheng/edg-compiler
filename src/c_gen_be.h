/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

c_gen_be.h - Declarations related to c_gen_be.c (C-generating back end
             for testing)

*/

/* Avoid including these declarations more than once: */
#ifndef C_GEN_BE_H
#define C_GEN_BE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#if BACK_END_IS_C_GEN_BE

EXTERN char	*module_list_for_union_init;
			/* The operand of the command-line "-i" option,
			   a comma-separated list of modules to be linked
			   with this one, and for which union initialization
			   routines should be called. */

#if !STANDALONE_UTILITY_PROGRAM
extern void back_end(void);
#endif /* !STANDALONE_UTILITY_PROGRAM */

#endif /* BACK_END_IS_C_GEN_BE */

#endif /* ifndef C_GEN_BE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
