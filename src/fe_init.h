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

fe_init.h -- Declarations relating to fe_init.c (having to do with
             global initialization of the front end).

*/

/* Avoid including these declarations more than once: */
#ifndef FE_INIT_H
#define FE_INIT_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/*
Date/time of compilation, in ctime format ("Sun Sep 16 01:03:52 1973\n"):
*/
EXTERN char	curr_date_time[26];

/* Initialize front end: */
extern void fe_one_time_init(void);
extern void fe_init(void);

extern a_symbol_ptr enter_predef_macro(char      *repl_text,
			               char      *macro_name,
				       a_boolean cannot_be_redefined);


#endif /* ifndef FE_INIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
