/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/* 
decl_inits.h -- Declarations related to decl_inits.c (having to do with
                initializers in declarations).

*/

/* Avoid including these declarations more than once: */
#ifndef DECL_INITS_H
#define DECL_INITS_H 1

#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

extern a_boolean check_string_constant_initializer(a_type_ptr      *var_type,
                                                   a_constant_ptr  string_con);

extern void repeat_nonconstant_init(a_dynamic_init_ptr  ctor_dip,
                                    a_type_ptr          array_type,
                                    a_type_ptr          elem_type,
                                    a_dynamic_init_ptr  new_dip,
                                    a_targ_size_t       count);

extern a_boolean dynamic_init_has_side_effects(
                                        a_dynamic_init_ptr dip,
                                        a_boolean          *suppress_warning);

extern void initializer(a_symbol_ptr       symbol_ptr,
                        a_source_position  *source_pos,
                        an_id_linkage_kind linkage,
                        a_boolean          paren_flag,
                        a_boolean          is_parameter,
                        a_boolean          *incomplete_type_error_reported,
                        a_decl_pos_block   *decl_pos_block);

extern a_boolean def_initializer(a_symbol_ptr       sym,
                                 a_source_position  *err_pos);

extern a_constructor_init_ptr ctor_initializer(a_routine_ptr  ctor_rout,
                                               a_boolean      user_defined);

extern a_constructor_init_ptr dtor_initializer(a_routine_ptr  dtor_rout);

extern void check_for_missing_initializer(a_symbol_ptr       sym,
                                          a_type_ptr         type);

extern void scan_compound_literal_initializer(a_type_ptr         *type,
                                              a_boolean          is_static,
                                              a_dynamic_init_ptr *dip);

#endif /* ifndef DECL_INITS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
