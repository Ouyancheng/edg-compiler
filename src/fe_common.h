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

This header file should be included first in all translation units.
Moreover, the header files mentioned here should *only* be included here, to
avoid problems resulting from interdependencies.

These header files involve declarations upon which other declarations depend
and/or which are used throughout the compiler rather than in a single phase
of processing.  This is a list of the files that are directly or indirectly
incorporated:

    basics.h
    cmd_line.h
    const_ints.h
    debug.h
    defines.h
    err_codes.h
    error.h
    float_pt.h
    host_envir.h
    il.h
    il_def.h
    il_to_str.h
    lang_feat.h
    mem_manage.h
    mem_tables.h
    targ_def.h
    target.h
    version.h

*/

/* Basic declarations.  basics.h also pulls in defines.h, which provides
   default configuration parameters for this version. */
#include "basics.h"

/* Language configuration. */
#include "lang_feat.h"

/* Host environment configuration.  Note that host_envir.h includes error.h
   because of references to an_error_severity; error.h in turn pulls in
   err_codes.h. */
#include "host_envir.h"

/* Target configuration. */
#include "targ_def.h"

/* Front end version number. */
#include "version.h"

/* Memory management data structures. */
#include "mem_tables.h"

/* IL declarations.  Note that il.h pulls in il_def.h. */
#include "il.h"

/* The next group of header files depend on il_def.h. */
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
