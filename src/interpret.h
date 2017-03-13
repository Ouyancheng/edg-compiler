/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.h -- Interface to IL interpreter for constexpr functions

*/
/* Avoid including these declarations more than once: */
#ifndef INTERPRET_H
#define INTERPRET_H 1

a_boolean is_core_constant_expr(an_expr_node_ptr  expr,
                                a_diag_list_ptr   diag_list);

a_boolean interpret_expr(an_expr_node_ptr  expr,
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list);

a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_dynamic_init(a_dynamic_init_ptr  dip,
                                 a_source_position   *pos,
                                 a_type_ptr          result_type,
                                 a_constant_ptr      result_con,
                                 a_diag_list_ptr     diag_list);

a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list);

#if DEBUG
uintptr_t db_hash_ptr(void  *ptr);

a_host_large_integer db_int_val(a_byte  *val_bytes);

void db_object(a_byte      *addr,
               a_type_ptr  tp);

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

#endif /* ifndef INTERPRET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
