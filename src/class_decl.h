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

class_decl.h -- Declarations related to class_decl.c (having to do with
	        scanning declarations of C++ classes).

*/

/* Avoid including these declarations more than once: */
#ifndef CLASS_DECL_H
#define CLASS_DECL_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* The pointer to a_delayed_scan_fixup is declared here even though the struct
   itself is defined in class_decl.c.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_delayed_scan_fixup *a_delayed_scan_fixup_ptr;

extern a_boolean do_alignment(a_targ_size_t    *byte_offset,
                             int              *bit_offset,
                             a_targ_alignment alignment);

extern a_boolean set_field_size_and_offset(a_field_ptr      field,
                                           a_targ_size_t    *p_byte_offset,
                                           int              *p_bit_offset,
                                           a_targ_alignment *p_alignment);

extern a_boolean class_specifier(a_boolean  first_specifier,
                                 a_type_ptr *type_ptr,
                                 a_boolean  *declares_something,
                                 a_boolean  *defines_something);

extern an_access_specifier compute_access(an_access_specifier access,
                                          an_access_specifier class_access);

extern a_boolean check_for_dominance(a_symbol_ptr          sym1,
                                     a_symbol_ptr          sym2,
                                     a_derivation_step_ptr path_to_sym2);

extern a_derivation_step_ptr make_derivation_step(
                                            a_base_class       *base_class,
                                            a_derivation_step  *existing_step);

extern void free_derivation_step(a_derivation_step_ptr  step);

extern a_boolean equivalent_paths(a_derivation_step_ptr  path1,
                                  a_derivation_step_ptr  path2);

#endif /* CLASS_DECL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
