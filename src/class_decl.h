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

/* The pointer to a_delayed_scan_fixup is declared here even though the struct
   itself is defined in class_decl.c.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_delayed_scan_fixup *a_delayed_scan_fixup_ptr;

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

extern void prescan_default_arg_expr(a_param_type_ptr  ptp);

extern a_boolean simplify_curr_class_qualified_name(void);

extern a_boolean do_alignment(a_targ_size_t    *byte_offset,
                              int              *bit_offset,
                              a_targ_alignment alignment);

extern a_boolean set_field_size_and_offset(a_field_ptr      field,
                                           a_targ_size_t    *p_byte_offset,
                                           int              *p_bit_offset,
                                           a_targ_alignment *p_alignment);

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

#if ASSIGNMENT_TO_THIS_ALLOWED
extern void set_class_assoc_operator_new_routine(a_type_ptr class_type);
extern void set_class_assoc_operator_delete_routine(a_type_ptr class_type);
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */

extern void define_special_member_function(a_routine_ptr      rout_ptr,
                                           a_type_ptr         class_type,
                                           a_source_position  *pos);

extern void reference_to_implicitly_invoked_function(a_symbol_ptr       sym,
                                                     a_source_position  *pos);

extern a_symbol_ptr member_function_redecl_sym(a_symbol_ptr  sym,
                                               a_type_ptr    type);

extern a_boolean check_for_dominance(a_symbol_ptr          sym1,
                                     a_symbol_ptr          sym2,
                                     a_derivation_step_ptr path_to_sym2);

extern a_derivation_step_ptr make_derivation_step(
                                            a_base_class       *base_class,
                                            a_derivation_step  *existing_step);

extern void free_derivation_step(a_derivation_step_ptr  step);

extern a_boolean equivalent_paths(a_derivation_step_ptr  path1,
                                  a_derivation_step_ptr  path2);

extern void check_class_linkage(void);

extern void class_decl_init(void);

#if DEBUG
extern void db_base_class(a_base_class_ptr  bcp,
                          a_boolean         show_offset);

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
