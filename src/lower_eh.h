/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_eh.h -- Declarations related to lower_eh.c (having to do with IL
              lowering for exception handling constructs).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_EH_H
#define LOWER_EH_H 1

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef LOWER_IL_H
#include "lower_il.h"
#endif /* ifndef LOWER_IL_H */


extern void define_scope_class_typeinfo_vars(a_scope_ptr scope);

extern void type_is_used_in_exception(a_type_ptr type);

extern void lower_throw(an_expr_node_ptr expr);

extern void init_object_addr_table_entry(
                                       an_init_pos_descr_ptr ipdp,
                                       a_targ_size_t         entry_number,
                                       an_insert_location    *insert_location);

extern a_variable_ptr make_caught_object_address_var(void);

extern a_cleanup_region_number cleanup_region_number(a_cleanup_action_ptr cap);

extern void assign_region_number_to_eh_curr_region(
                                     a_cleanup_region_number region_number,
                                     an_insert_location      *insert_location);

extern void set_eh_curr_region(a_context_ptr      context,
                               an_insert_location *insert_location);

extern void set_region_on_prev_destructor_wrapper_cleanup(
                                        a_cleanup_action_ptr cap,
                                        an_insert_location   *insert_location);

extern void make_region_table_entry(a_cleanup_action_ptr cap,
                                    an_insert_location   *insert_location);

extern void remove_from_exception_cleanup_list(a_cleanup_action_ptr cap);

extern void add_eh_function_prologue(a_scope_ptr scope);

extern void begin_catch_clause(a_handler_ptr handler);

extern void cleanup_on_exit_from_try_block(
                                        a_cleanup_action_ptr cap,
                                        an_insert_location   *insert_location);

extern void cleanup_on_exit_from_catch(an_insert_location *insert_location);

extern void lower_try_block(a_statement_ptr statement);

extern void eh_function_lower_init(a_boolean file_scope_term_routine);

extern void eh_lower_one_time_init(void);

extern void eh_lower_init(void);

#endif /* DO_IL_LOWERING */
#endif /* ifndef LOWER_EH_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
