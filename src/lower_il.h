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

lower_il.h -- Declarations related to lower_il.c (having to do with
              lowering C++ intermediate language to C intermediate language).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_IL_H
#define LOWER_IL_H 1

#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */


/*
This switch controls whether or not types and static variables that are local
to function and block scopes are moved onto the file scope lists.  Such
entities are allocated in the file scope memory region, but they are
normally linked on the local scope types or variables list.  That accurately
reflects the source form, which is desirable for generating symbolic
debug information.  That form probably works fine when the IL is being
fed into a true back end, but will not work when the IL is being turned
into C output (as with the C-generating back end), because the local
types and variables will not be visible from member functions of local
classes and in the file-scope termination routine when it deals with
calling destructors for local static variables.  When the switch here
is TRUE, the local types and variables will be (selectively) promoted
to the actual file scope.
*/
#define PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE BACK_END_IS_C_GEN_BE


#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		allocated_name_string_length;
#endif /* DEBUG */

extern void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta);

extern void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset);

extern a_boolean virtual_dtor_should_be_generated_for_class(
                                                        a_type_ptr class_type);

extern void lower_il_memory_region(a_memory_region_number region_number);

#if DEBUG
extern unsigned long show_lowering_space_used(void);
#endif /* DEBUG */

extern void il_lower_init(void);

#endif /* ifndef LOWER_IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
