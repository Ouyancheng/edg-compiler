/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

pragma.h -- Declarations related to the #pragma directives

*/

/* Avoid including these declarations more than once. */
#ifndef PRAGMA_H
#define PRAGMA_H 1

#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

extern a_pending_pragma_ptr extract_specific_pragmas(a_pragma_kind    kind,
                                                     a_symbol_ptr     sym,
                                                     a_statement_ptr  sp);

extern void process_pragmas_bound_to_curr_decl_or_stmt(a_symbol_ptr     sym,
                                                       a_statement_ptr  sp);
#endif /* ifndef PRAGMA_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1994 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
