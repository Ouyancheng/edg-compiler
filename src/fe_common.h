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

fe_common.h -- Inclusion of header files common to all EDG front end files.

These header files involve declarations upon which other declarations depend
and/or which are used throughout the compiler rather than in a single phase
of processing.  This is a list of the files that are directly or indirectly
incorporated:

    basics.h
    cmd_line.h
    const_ints.h
    debug.h
    def_arg.h
    defines.h
    err_codes.h
    error.h
    float_pt.h
    host_envir.h
    il.h
    il_def.h
    il_to_str.h
    lang_feat.h
    lexical.h
    mem_manage.h
    mem_tables.h
    pragma.h
    symbol_tbl.h
    types.h
    targ_def.h
    target.h
    version.h

*/

/* Basic configuration declarations.  This header file pulls in basics.h,
   defines.h, lang_feat.h, host_envir.h, and targ_def.h. */
#include "basic_hdrs.h"

/* Errors.  error.h also pulls in err_codes.h. */
#include "error.h"

/* Front end version number. */
#include "version.h"

/* Memory management data structures. */
#include "mem_tables.h"

/* IL declarations.  Note that il.h pulls in il_def.h. */
#include "il.h"

/* Target configuration variables. */
#include "target.h"

/* Variables set on the basis of command line options. */
#include "cmd_line.h"

/* Additional declarations relating to memory management. */
#include "mem_manage.h"

/* Manipulation of target integer constants. */
#include "const_ints.h"

/* Manipulation of internal floating point quantitities. */
#include "float_pt.h"

/* Production of a string-form representation of IL entities. */
#include "il_to_str.h"

/* Symbol table declarations.  symbol_tbl.h also pulls in lexical.h. */
#include "symbol_tbl.h"

/* Type system support. */
#include "types.h"

#if DEBUG
/* Debug declarations. */
#include "debug.h"
#endif /* DEBUG */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
