/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

fixed_pt.h -- Declarations for fixed_pt.c (having to do with manipulation of
              internal fixed-point quantities).

*/

/* Avoid including these declarations more than once: */
#ifndef FIXED_PT_H
#define FIXED_PT_H 1

#define cmp_fixed_point_constants(cp1, cp2)  \
  cmp_integer_constants((cp1), (cp2))

extern void fxp_init_value(a_fixed_point_value  *value);

extern void fxp_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                                      char                      *str,
                                      a_fixed_point_value       *value,
                                      a_boolean                 *err);

extern void fxp_hex_string_to_fixed_point(a_fixed_point_type_descr  *fxp_descr,
                                          char                      *str,
                                          a_fixed_point_value       *value,
                                          a_boolean                 *err,
                                          a_boolean                 *inexact);

extern char* fxp_to_string(a_fixed_point_type_descr  *fxp_descr,
                           a_fixed_point_value       *value);

extern unsigned int fxp_hash(a_fixed_point_value *value);

#endif /* ifndef FIXED_PT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

