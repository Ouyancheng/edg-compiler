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
    src_seq.h
    symbol_tbl.h
    types.h
    targ_def.h
    target.h
    version.h

*/

/* Basic configuration declarations.  This header file pulls in basics.h,
   defines.h, lang_feat.h, host_envir.h, and targ_def.h. */
#ifndef BASIC_HDRS_H
#include "basic_hdrs.h"
#endif /* ifndef BASIC_HDRS_H */

/* Errors.  error.h also pulls in err_codes.h. */
#include "error.h"

/* Front end version number. */
#include "version.h"

/* Memory management data structures. */
#include "mem_tables.h"

/* IL declarations.  Note that il.h pulls in il_def.h. */
#include "il.h"

#if !STANDALONE_UTILITY_PROGRAM
/* IL allocation declarations. */
#include "il_alloc.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Target configuration variables. */
#include "target.h"

/* Additional declarations relating to memory management. */
#include "mem_manage.h"

/* Manipulation of target integer constants. */
#include "const_ints.h"

/* Manipulation of internal floating point quantities. */
#include "float_pt.h"

/* Production of a string-form representation of IL entities. */
#include "il_to_str.h"

#if !STANDALONE_UTILITY_PROGRAM
/* Symbol table declarations.  symbol_tbl.h also pulls in lexical.h. */
#include "symbol_tbl.h"

/* Identifier lookup routines. */
#include "lookup.h"

/* Variables set on the basis of command line options. */
#include "cmd_line.h"

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Type system support. */
#include "types.h"

/* Translation unit data structures. */
#include "trans_unit.h"

#if GENERATE_SOURCE_SEQUENCE_LISTS
/* Source sequence list management */
#include "src_seq.h"
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

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
