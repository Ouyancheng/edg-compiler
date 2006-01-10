/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2006 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

lower_c99.h -- Declarations related to lower_c99.c.

*/

/* Avoid including these declarations more than once. */
#ifndef LOWER_C99_H
#define LOWER_C99_H 1
#if DO_IL_LOWERING

#include "il.h"

extern void lower_runtime_sizeof(an_expr_node_ptr expr);

extern void lower_vla_dimension_expression(a_vla_dimension_ptr  vdp);

#if LOWER_VARIABLE_LENGTH_ARRAYS

extern void record_vla_component_types_for_lowering(a_type_ptr  tp);

extern void prepare_to_lower_variably_modified_typedef(a_type_ptr  type);

extern an_expr_node_ptr lower_vla_dimensions(a_type_ptr  tp);

extern void lower_vla_types(void);

extern void lower_vla_decl(a_statement_ptr  stmt);

extern void lower_set_vla_size(a_statement_ptr  stmt);

extern void lower_vla_pointer_integer_arithmetic(an_expr_node_ptr  expr);

extern void lower_vla_pointer_difference(an_expr_node_ptr  expr);

extern void lower_vla_cast(an_expr_node_ptr  expr);

extern void lower_vla_address(an_expr_node_ptr  expr);

extern void lower_vla_dealloc(an_expr_node_ptr  expr);
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */

extern void create_dimension_variable(a_statement_ptr  stmt);

extern void create_element_count_variable_for_vla(a_statement_ptr  stmt);

#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_COMPLEX

extern void lower_c99_nonreal_float_types(void);

extern void lower_c99_complex_constant(a_constant_ptr  constant);

extern void lower_c99_complex_cast(an_expr_node_ptr  expr);

void lower_c99_xnegate(an_expr_node_ptr  expr);

void lower_c99_xadd(an_expr_node_ptr  expr);

void lower_c99_xsubtract(an_expr_node_ptr  expr);

void lower_c99_xmultiply(an_expr_node_ptr  expr);

void lower_c99_xdivide(an_expr_node_ptr  expr);

void lower_c99_xeq(an_expr_node_ptr  expr);

void lower_c99_xne(an_expr_node_ptr  expr);

#if GNU_EXTENSIONS_ALLOWED

void lower_xconj(an_expr_node_ptr  expr);

void lower_complex_projection(an_expr_node_ptr  expr);

#endif /* GNU_EXTENSIONS_ALLOWED */
#endif /* LOWER_COMPLEX */
#if DO_C99_IL_LOWERING

#if FIXED_POINT_ALLOWED
#define or_fixed_point_lowering_needed() || fixed_point_enabled
#else /* !FIXED_POINT_ALLOWED */
#define or_fixed_point_lowering_needed() /* Nothing */
#endif /* FIXED_POINT_ALLOWED */

#define c99_il_lowering_needed()                                             \
  ((c99_mode || gcc_mode || compound_literals_allowed || vla_enabled ||      \
    designators_allowed                                                      \
    or_fixed_point_lowering_needed()) &&                                     \
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

#endif /* DO_C99_IL_LOWERING */

extern void lower_c99_one_time_init(void);

extern void lower_c99_trans_unit_init(void);

extern void lower_c99_init(void);

#endif /* DO_IL_LOWERING */
#endif /* #ifndef LOWER_C99_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2006 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
