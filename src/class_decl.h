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

class_decl.h -- Declarations related to class_decl.c (having to do with
	        scanning declarations of C++ classes).

*/

/* Avoid including these declarations more than once: */
#ifndef CLASS_DECL_H
#define CLASS_DECL_H 1

#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */


extern void prescan_member_function_default_arg_expr(a_param_type_ptr  ptp);

extern a_boolean scan_class_definition(
                                   a_type_ptr     class_type,
                                   a_scope_depth  effective_decl_level,
                                   a_boolean      is_local_class);

extern void check_anonymous_union_symbols(a_symbol_ptr  assoc_object_sym,
                                          a_type_ptr    class_type,
                                          a_boolean     is_nonstd);

#if NEW_CAN_BE_FOLDED_INTO_CTOR
extern void set_class_assoc_operator_new_routine(a_type_ptr class_type);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

#if DELETE_CAN_BE_FOLDED_INTO_DTOR
extern void set_class_assoc_operator_delete_routine(a_type_ptr class_type);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

extern a_boolean is_assignment_operator_for_copy(
                                              a_symbol_ptr  sym,
                                              a_boolean     *is_ref_arg,
                                              a_boolean     *accepts_const,
                                              a_boolean     *accepts_volatile);

extern a_symbol_ptr member_function_redecl_sym(a_symbol_ptr  sym,
                                               a_type_ptr    type);

extern a_derivation_step_ptr make_derivation_step(
                                            a_base_class       *base_class,
                                            a_derivation_step  *existing_step);

extern void free_derivation_step(a_derivation_step_ptr  step);

extern a_boolean congruent_paths(a_derivation_step_ptr  dsp1,
                                 a_derivation_step_ptr  dsp2);

extern void check_class_linkage(void);

extern void class_decl_init(void);

#if DEBUG
extern unsigned long db_show_routine_fixups_used(unsigned long grand_total);

extern void db_path(a_derivation_step_ptr dsp,
                    a_boolean             show_offset);

extern void db_abbreviated_base_class(a_base_class_ptr  bcp);

extern void db_base_class(a_base_class_ptr  bcp,
                          a_boolean         show_offset);

extern void db_base_class_list(a_type_ptr tp);

extern void db_all_virtual_function_override_lists(a_type_ptr  class_type);
#endif /* DEBUG */

#endif /* CLASS_DECL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
