/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

trans_corresp.h -- Declarations related to matching entities across
                   translation units.

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_CORRESP_H
#define TRANS_CORRESP_H 1

extern a_type_ptr canonical_type_entry_of(a_type_ptr type);

extern a_template_ptr canonical_template_entry_of(a_template_ptr templ);

extern void establish_trans_unit_correspondences_for_scope(a_scope_ptr  scope);

extern void verify_trans_unit_correspondences_for_scope(a_scope_ptr  scope);

extern void record_instantiation(a_symbol_ptr                      inst,
                                 a_template_symbol_supplement_ptr  tssp);

#endif /* ifndef TRANS_CORRESP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
