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
                                   a_boolean      is_local_class,
                                   a_boolean      is_prototype_instantiation);

extern a_boolean class_specifier(a_boolean  vacuous_decl_allowed,
                                 a_boolean  is_friend_decl,
                                 a_boolean  is_ref_within_new_expr,
                                 a_type_ptr *type_ptr,
                                 a_boolean  *declares_something,
                                 a_boolean  *defines_something);

extern void check_anonymous_union_symbols(a_type_ptr     class_type,
                                          a_field_ptr    assoc_field_object,
                                          a_variable_ptr assoc_var_object);

extern a_boolean is_copy_constructor(a_routine_ptr  ctor_rout,
                                     a_type_ptr     class_of_which_a_member,
                                     a_boolean      *const_object_okay,
                                     a_boolean      *volatile_object_okay);

extern a_boolean is_default_constructor(a_routine_ptr  ctor_rout);

#if NEW_CAN_BE_FOLDED_INTO_CTOR
extern void set_class_assoc_operator_new_routine(a_type_ptr class_type);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

#if DELETE_CAN_BE_FOLDED_INTO_DTOR
extern void set_class_assoc_operator_delete_routine(a_type_ptr class_type);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

extern void reference_to_implicitly_invoked_function
                                    (a_symbol_ptr       sym,
                                     a_source_position  *pos,
                                     a_type_ptr         class_of_object,
                                     a_boolean          honor_virtual,
                                     a_boolean          evaluated,
                                     a_boolean          suppress_access_check);

extern void force_definition_of_compiler_generated_routine(a_routine_ptr rp);

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
