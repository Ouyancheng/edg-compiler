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

/*
Value used to indicate "no region number" for exception handling regions.
It's all one bits, truncated to fit in a TARG_REGION_NUMBER_INT_KIND integer.
*/
EXTERN a_cleanup_region_number
		null_eh_region_number;

/*
Current position in the cleanup region table.  This is the point at which
cleanup should start if an exception were thrown at the current position
in the program.  __eh_curr_region gets set to this value.  (This value
is maintained, and code is generated to make __eh_curr_region track it.)
*/
EXTERN a_cleanup_region_number
		curr_cleanup_region_number;


extern void define_scope_class_typeinfo_vars(a_scope_ptr scope);

extern void type_is_used_in_exception(a_type_ptr type);

extern void lower_throw(an_expr_node_ptr expr);

extern void init_object_addr_table_entry(
                                       an_init_pos_descr_ptr ipdp,
                                       a_handle_number       entry_number,
                                       an_insert_location    *insert_location);

extern a_handle_number object_addr_table_index(void);

extern a_variable_ptr make_caught_object_address_var(void);

extern a_cleanup_region_number cleanup_region_number(a_dynamic_init_ptr dip);

extern void set_eh_curr_region(a_cleanup_region_number region_number,
                               an_insert_location      *insert_location);

extern a_constant_ptr make_region_table_entry(
                              an_init_pos_descr_ptr   ipdp,
                              a_routine_ptr           routine,
                              a_boolean               is_delete,
                              a_variable_ptr          conditional_flag_var,
                              a_handle_number         conditional_flag_handle,
                              a_cleanup_region_number next_region_number,
                              a_cleanup_region_number *region_number,
                              an_insert_location      *insert_location);

extern void make_dyn_init_region_table_entry(
                                          a_dynamic_init_ptr dip,
                                          a_dynamic_init_ptr next_dip,
                                          an_insert_location *insert_location);

extern void clone_region_table_entry_list(a_dynamic_init_ptr dip,
                                          a_dynamic_init_ptr stop_before);

extern void add_eh_function_prologue(a_scope_ptr scope);

extern void begin_catch_clause(a_handler_ptr handler);

extern void cleanup_on_exit_from_try_block(
                                          a_context_ptr      context_ptr,
                                          an_insert_location *insert_location);

extern void cleanup_on_exit_from_catch(an_insert_location *insert_location);

extern void lower_try_block(a_statement_ptr statement);

extern void eh_function_lower_init(void);

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
