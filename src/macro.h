/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

macro.h -- Declarations relating to macro.c (having to do with macro
           definition and expansion routines).

*/

/* Avoid including these declarations more than once: */
#ifndef MACRO_H
#define MACRO_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

EXTERN unsigned long
		macro_depth;
			/* Current number of levels of nesting of macro
			   invocations.  Zero if no macro calls are being
			   processed currently. */

EXTERN a_symbol_ptr
	       	line_macro_symbol,
		file_macro_symbol,
		defined_macro_symbol;
			/* Pointers to the symbol entries for the special
			   macros "__LINE__", "__FILE__", and "defined". */

extern a_macro_def_ptr alloc_macro_def(void);

/* Find a macro symbol on a list of symbols. */
extern a_symbol_ptr find_defined_macro(a_symbol_ptr assoc_symbol);

/* Adjust addresses in the curr_source_line structure after something
   has been realloc'd. */
extern void adjust_curr_source_line_structure_after_realloc(char *old_ptr,
                                                     char *old_after_end_ptr,
                                                     char *new_ptr);

/* Expand a macro invocation. */
extern a_token_kind macro_invocation(a_symbol_ptr  macro_symbol,
                                     a_boolean     *rescan);

/* Make a_constant with a given integer value, for a preprocessing 
   expression. */
extern a_token_kind make_pp_int_constant(long value);

/* Process a #define directive. */
extern void proc_define(void);

/* Process an #assert directive. */
extern void proc_assert(void);

/* Process an #unassert directive. */
extern void proc_unassert(void);

/* Enter a predefined #assert predicate. */
extern void enter_assert_predicate(char *value,
                                   char *name);

/* Scan a reference to an #assert predicate */
extern a_boolean scan_assert_predicate_reference(void);

extern a_symbol_ptr enter_predef_macro(char      *repl_text,
			               char      *macro_name,
				       a_boolean cannot_be_redefined);

extern void init_predefined_macros(char  curr_date_time[26]);

#if DEBUG
/* Show and return the amount of space used by macro entries. */
extern unsigned long show_macro_space_used(void);
#endif /* DEBUG */

extern void macro_one_time_init(void);

extern void macro_init(void);

#endif /* MACRO_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
