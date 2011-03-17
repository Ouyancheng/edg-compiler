/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

extasm.h -- Declarations related to extasm.c (having to do with 
            extended asm() statements, a GNU C extension).

*/

/* Avoid including these declarations more than once: */
#ifndef EXTASM_H
#define EXTASM_H 1

#if GNU_EXTENSIONS_ALLOWED

extern a_named_register name_to_register(char  *name);

extern an_asm_operand_ptr asm_operands_spec(void);

extern a_named_register_list_ptr asm_clobbers_spec(void);

extern void validate_operands_and_clobbers(an_asm_entry_ptr  asm_entry);

extern void extasm_one_time_init(void);

#endif /* GNU_EXTENSIONS_ALLOWED */

#endif /* EXTASM_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

