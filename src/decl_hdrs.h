/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decl_hdrs.h -- Inclusion of header files used by files involved in declaration
               processing.

*/

#include "class_decl.h"
#include "decl_inits.h"
#include "decl_spec.h"
#include "declarator.h"
#include "decls.h"
#include "def_arg.h"
#include "func_def.h"
#include "lexical.h"
#include "pch.h"
#include "pragma.h"
#include "preproc.h"
#include "symbol_ref.h"
#include "symbol_tbl.h"
#include "templates.h"
#include "types.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
