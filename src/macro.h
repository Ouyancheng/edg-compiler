/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

macro.h -- Declarations relating to macro.c (having to do with macro
           definition and expansion routines).

*/

/* Avoid including these declarations more than once: */
#ifndef MACRO_H
#define MACRO_H 1

EXTERN a_symbol_ptr
	       	line_macro_symbol,
		file_macro_symbol,
		alignof_macro_symbol,
		intaddr_macro_symbol,
		defined_macro_symbol;
			/* Pointers to the symbol entries for the special
			   macros "__LINE__", "__FILE__", "__ALIGNOF__",
			   "__INTADDR__", and "defined". */

extern a_macro_def_ptr alloc_macro_def(void);

/* Find a macro symbol on a list of symbols. */
extern a_symbol_ptr find_defined_macro(a_symbol_ptr assoc_symbol);

/* Expand a macro invocation. */
extern a_token_kind macro_invocation(a_symbol_ptr  macro_symbol,
                                     a_boolean     *rescan);

/* Make a_constant with a given integer value, for a preprocessing 
   expression. */
extern a_token_kind make_pp_int_constant(long value);

/* Process a #define directive. */
extern void proc_define(void);

#if DEBUG
/* Show and return the amount of space used by macro entries. */
extern unsigned long show_macro_space_used(void);
#endif /* DEBUG */

extern void macro_proc_init(void);

#endif /* MACRO_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
