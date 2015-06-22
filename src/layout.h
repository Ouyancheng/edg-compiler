/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
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

typedef a_host_large_unsigned an_unnormalized_bit_offset;

#if USER_CONTROL_OF_STRUCT_PACKING
EXTERN a_targ_alignment
		curr_max_member_alignment;
			/* Current pack alignment, as specified by the most
			   recent #pragma pack directive.  If it is zero, use
			   the default pack alignment, as specified on the
			   command line. */

typedef struct a_pack_alignment_stack_entry *a_pack_alignment_stack_entry_ptr;

/* An entry in which to save current pack-alignment state during the
   instantiation of a class or function template.  The state is restored
   when the instantiation is completed. */
typedef struct a_pack_alignment_state *a_pack_alignment_state_ptr;
typedef struct a_pack_alignment_state {
  a_targ_alignment
		saved_max_member_alignment;
			/* The value of curr_max_member_alignment (defined
			   in layout.c) when the instantiation begins. */
  a_pack_alignment_stack_entry_ptr
		saved_pack_alignment_stack;
			/* A pointer to the top of the pack-alignment stack
			   (see pack_alignment_stack, defined in layout.c)
			   when the instantiation begins. */
} a_pack_alignment_state;

extern void reset_pack_alignment_state(a_targ_alignment            alignment,
                                       a_pack_alignment_state_ptr  state);

extern void restore_pack_alignment_state(a_pack_alignment_state_ptr state);

extern
a_boolean check_pack_alignment_value(a_host_large_integer value,
                                     a_targ_alignment      *alignment);

extern void pack_pragma(a_pending_pragma_ptr ppp);

extern a_targ_alignment current_max_alignment_for_class_members(void);

extern a_targ_alignment current_pack_pragma_value(void);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

extern a_targ_alignment alignment_of_field_full(a_field_ptr  field,
                                                a_boolean    for_alignof);

extern a_boolean is_empty_class_type(a_type_ptr type);

extern void do_class_layout(a_type_ptr  class_type);

extern void layout_one_time_init(void);

extern void layout_trans_unit_init(void);

extern void layout_init(void);

#endif /* LAYOUT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
