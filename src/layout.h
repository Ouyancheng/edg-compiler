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

layout.h -- Declarations related to layout.c (having to do with laying out
            class objects)

*/

/* Avoid including these declarations more than once: */
#ifndef LAYOUT_H
#define LAYOUT_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

typedef unsigned long an_unnormalized_bit_offset;

#if USER_CONTROL_OF_STRUCT_PACKING
extern a_boolean check_pack_alignment_value(long              value,
                                            a_targ_alignment  *alignment);

extern void pack_pragma(a_pending_pragma_ptr ppp);

extern void set_max_member_alignment_for_class(a_type_ptr  class_type);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

extern void do_class_layout(a_type_ptr  class_type);

extern void layout_one_time_init(void);

extern void layout_init(void);

#endif /* LAYOUT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
