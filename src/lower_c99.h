/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

lower_c99.h -- Declarations related to lower_c99.c.

*/

/* Avoid including these declarations more than once. */
#ifndef LOWER_C99_H
#define LOWER_C99_H 1
#include "il.h"

#if DO_C99_IL_LOWERING

#define c99_il_lowering_needed()                                             \
  (c99_mode && !suppress_il_lowering && total_errors == 0)

extern void lower_c99_constant(a_constant_ptr constant);

extern void lower_c99_expr(an_expr_node_ptr expr);

extern void lower_c99_full_expr(an_expr_node_ptr expr);

extern void lower_c99_il_memory_region(a_scope_ptr scope);

extern void lower_c99_one_time_init(void);

extern void lower_c99_init(void);

#endif /* DO_C99_IL_LOWERING */

#endif /* #ifndef LOWER_C99_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

