/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2004 Edison Design Group Inc.                   [_]          *
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

#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
#define or_vla_lowering_needed() || vla_enabled
#else /* !(VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS) */
#define or_vla_lowering_needed() /* Nothing */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */

#if FIXED_POINT_ALLOWED
#define or_fixed_point_lowering_needed() || fixed_point_enabled
#else /* !FIXED_POINT_ALLOWED */
#define or_fixed_point_lowering_needed() /* Nothing */
#endif /* FIXED_POINT_ALLOWED */

#define c99_il_lowering_needed()                                             \
  ((c99_mode || gcc_mode || compound_literals_allowed                        \
    or_fixed_point_lowering_needed()                                         \
    or_vla_lowering_needed()) &&                                             \
   !suppress_il_lowering && total_errors == 0)

#if LOWER_FIXED_POINT
extern a_type_ptr lowered_integer_type_for_fixed_point_type(
                                                           a_type_ptr fx_type);
#endif /* LOWER_FIXED_POINT */

extern void lower_c99_cast(an_expr_node_ptr expr);

extern void lower_c99_constant(a_constant_ptr constant);

extern void lower_c99_operator(an_expr_node_ptr expr);

extern void lower_c99_expr(an_expr_node_ptr expr,
                           a_boolean        used_as_lvalue);

extern void lower_c99_full_expr(an_expr_node_ptr expr);

void post_lower_c99_bool_cast(an_expr_node_ptr expr);

extern void lower_c99_il_memory_region(a_memory_region_number region_number);

extern void lower_c99_one_time_init(void);

extern void lower_c99_trans_unit_init(void);

extern void lower_c99_init(void);

#endif /* DO_C99_IL_LOWERING */

#endif /* #ifndef LOWER_C99_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2004 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
