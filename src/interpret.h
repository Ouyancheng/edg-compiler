/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.h -- Interface to IL interpreter for constexpr functions

*/
/* Avoid including these declarations more than once: */
#ifndef INTERPRET_H
#define INTERPRET_H 1

a_boolean interpret_constexpr_call(an_expr_node_ptr      call_expr,
                                   a_constant_ptr        result_con);

#if DEBUG
uintptr_t db_hash_ptr(void  *ptr);

void db_call_stack(void  *ips);
#endif /* DEBUG */
#endif /* ifndef INTERPRET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
