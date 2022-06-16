/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.h -- Interface to IL interpreter for constexpr functions

*/
/* Avoid including these declarations more than once: */
#ifndef INTERPRET_H
#define INTERPRET_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_subobject_path_ptr* last_subobject_path_link(a_constant_ptr  con);

a_subobject_path_ptr get_trailing_subobject_path_entry(
                                               a_constant_ptr  con,
                                               a_boolean       is_offset,
                                               a_boolean       is_base_class);

a_boolean interpret_clang_enable_if_opnd(
                                       an_expr_node_ptr              expr,
                                       Dyn_array<a_constant*> const  &params,
                                       a_source_position             *pos,
                                       a_boolean                     *p_value);

a_boolean is_core_constant_expr(an_expr_node_ptr  expr,
                                a_diag_list_ptr   diag_list);

a_boolean interpret_expr(an_expr_node_ptr  expr,
                         a_boolean         is_constant_evaluated,
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list);

a_routine_ptr get_constexpr_callee(an_expr_node_ptr  call_expr,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_boolean         is_constant_evaluated,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_dynamic_init_full(
                                  a_dynamic_init_ptr  dip,
                                  a_source_position   *pos,
                                  a_type_ptr          result_type,
                                  a_boolean           is_constant_evaluated,
                                  a_constant_ptr      result_con,
                                  a_diag_list_ptr     diag_list,
                                  a_boolean           allow_reinterpret_cast);

#define interpret_dynamic_init(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list)                        \
  (interpret_dynamic_init_full(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list, FALSE))

a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_boolean           is_constant_evaluated,
                                   a_source_position   *pos,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list);

enum a_constexpr_intrinsic {
  cit_error,
  cit_std_is_constant_evaluated,
  cit_std_allocator_allocate,
  cit_std_allocator_deallocate,
  cit_std_construct_at,
  cit_std_destroy_at,
  cit_std_report_constexpr_value,
  cit_last
};

void register_constexpr_intrinsic(a_constexpr_intrinsic  tag,
                                  a_routine_ptr          rp);

#if DEBUG
uintptr_t db_hash_ptr(void  *ptr);

a_host_large_integer db_int_val(a_byte  *val_bytes);

void db_complete_object(a_byte  *addr);

void db_call_stack(void  *ips);

a_byte* db_stack_storage(void  *ptr,
                         void  *ips);

void db_data_map(void  *map_ptr);

void db_live_set(void  *interpreter_state);

unsigned long db_show_interpret_fe_space_used(unsigned long  grand_total);

#if TRACK_INTERPRETER_ALLOCATIONS
unsigned long db_object_alloc_num(a_byte  *ptr);
#endif /* TRACK_INTERPRETER_ALLOCATIONS */

#endif /* DEBUG */

void clean_up_interpreter(void);

void interpret_trans_unit_init(void);

void interpret_init(void);

void interpret_one_time_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef INTERPRET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
