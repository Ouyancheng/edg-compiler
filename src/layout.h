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

/* Data structure to track some information about the layout of a class
   as it is being constructed. */
typedef struct a_layout_block *a_layout_block_ptr;
typedef struct a_layout_block {
  a_type_ptr	class_type;
			/* Pointer to the class type whose layout is being
			   defined. */
  a_targ_size_t	byte_offset;
			/* Byte offset (relative to the start of the class
			   object) for the *next* field to be entered. */
  an_unnormalized_bit_offset
		bit_offset;
			/* Bit offset relative to byte_offset for the *next*
			   bit field to be entered. */
  a_targ_alignment
		alignment;
			/* The alignment for the class object as a whole,
			   never less than the alignment required for any
			   field or subobject of that class. */
  a_byte_boolean
		any_overflow;
			/* Set to TRUE when the layout exceeds the maximum
			   size allowed for a class object. */
} a_layout_block;

extern void clear_layout_block(a_layout_block_ptr  lob,
                               a_type_ptr          class_type);

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

extern void set_offsets_for_nonvirtual_base_classes(a_layout_block_ptr  lob);

extern void finish_laying_out_class(a_layout_block_ptr  lob);

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
