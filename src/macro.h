/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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
EXTERN a_symbol_ptr
	       	base_file_macro_symbol;
			/* Pointer to the symbol entry for the special
			   GNU macro __BASE_FILE__. */


/* Find a macro symbol on a list of symbols. */
extern a_symbol_ptr find_defined_macro(a_symbol_header_ptr sym_hdr);

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
extern void scan_assert_predicate_reference(a_boolean *rescan);

extern a_symbol_ptr enter_predef_macro(char      *repl_text,
			               char      *macro_name,
                                       a_boolean cannot_be_redefined,
                                       a_boolean ref_suppresses_pch_file);

extern void fixup_predefined_macros(char  curr_date_time[26]);

extern void init_predefined_macros(char  curr_date_time[26]);

#if DEBUG
/* Show and return the amount of space used by macro entries. */
extern unsigned long show_macro_space_used(void);
#endif /* DEBUG */

extern void macro_one_time_init(void);

extern void macro_trans_unit_init(void);

extern void macro_init(void);

/* When variadic macros are enabled, the identifier __VA_ARGS__ can only
   appear in the replacement lists of variadic macros.  The following check
   appears in a few places, including lexical analysis of identifiers. */
#define check_use_of_VA_ARGS(len, buf)                                \
  if (variadic_macros_allowed &&                                      \
      len == sizeof("__VA_ARGS__")-1 &&                               \
      strncmp(buf, "__VA_ARGS__", sizeof("__VA_ARGS__")-1) == 0) {    \
    error(ec_VA_ARGS_not_allowed);                                    \
  }  /* if */

#endif /* MACRO_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
