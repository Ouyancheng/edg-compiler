/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
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

extern a_boolean check_string_constant_initializer_full(
                                                   a_type_ptr      *dst_type,
                                                   a_constant_ptr  string_con,
                                                   a_boolean       *excess);

#define check_string_constant_initializer(dst_type, string_con)              \
  (check_string_constant_initializer_full(dst_type, string_con,              \
                                          (a_boolean*)NULL))

extern void repeat_nonconstant_init(a_dynamic_init_ptr  ctor_dip,
                                    a_type_ptr          array_type,
                                    a_type_ptr          elem_type,
                                    a_dynamic_init_ptr  new_dip,
                                    a_targ_size_t       count);

extern void initializer(a_decl_parse_state  *state,
                        a_source_position   *source_pos,
                        an_id_linkage_kind  linkage,
                        a_boolean           paren_flag,
                        a_boolean           *incomplete_type_error_reported,
                        a_decl_pos_block    *decl_pos_block);

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

/* Describes how the length of each C++/CLI array dimension should be processed
   while scanning an array-init. */
typedef struct a_cli_array_init_scan_info
{
  an_expr_node_ptr
		cli_array_dimension_lengths;
			/* A list of C++/CLI array dimension lengths associated
			   with the array-init being scanned. */
  a_boolean
		populate_cli_array_lengths;
			/* TRUE if cli_array_dimension_lengths should be
			   populated with the greatest length of each dimension
			   while scanning a C++/CLI array-init.  FALSE if
			   cli_array_dimension_lengths should be used as bound
			   checks for each dimension length.  This member is
			   only relevant if cli_array_dimension_lengths is
			   non-NULL. */
}  a_cli_array_init_scan_info;

typedef a_cli_array_init_scan_info *a_cli_array_init_scan_info_ptr;

#if MICROSOFT_EXTENSIONS_ALLOWED

extern a_boolean scan_cli_array_init(
                    a_decl_parse_state_ptr      dps,
                    a_type_ptr                  *handle_to_cli_array_type,
                    a_variable_ptr              vp,
                    a_boolean                   static_lifetime,
                    a_source_position           *err_pos,
                    a_dynamic_init_ptr          *init_dip,
                    a_decl_pos_block_ptr        decl_pos_block,
                    an_expr_node_ptr            *cli_array_dimension_lengths);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef DECL_INITS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
