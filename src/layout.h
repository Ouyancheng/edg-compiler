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
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

extern void scan_bit_field_size(a_boolean         *unnamed_bit_field,
                                a_type_ptr        *p_base_type,
                                long              *p_bit_field_size,
                                a_boolean         *p_is_signed,
                                a_symbol_locator  *locator);

extern a_boolean do_alignment(a_targ_size_t               *byte_offset,
                              an_unnormalized_bit_offset  *bit_offset,
                              a_targ_alignment            alignment);

extern a_boolean set_field_size_and_offset(
                                    a_field_ptr                 field,
                                    a_targ_size_t               *p_byte_offset,
                                    an_unnormalized_bit_offset  *p_bit_offset,
                                    a_targ_alignment            *p_alignment);

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
