/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
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

extern int asm_operands_spec(an_asm_operand_ptr *p_operands);

extern int asm_clobbers_spec(a_named_register **p_clobbers);

extern void validate_operands_and_clobbers(an_asm_operand_ptr operands,
                                           int num_operands,
                                           a_named_register *clobbers,
                                           int num_clobbers);

extern void extasm_one_time_init(void);

#endif /* GNU_EXTENSIONS_ALLOWED */

#endif /* EXTASM_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

