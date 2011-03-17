/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

basic_hdrs.h -- Inclusion of low-level universal header files.

*/

/* Avoid including these declarations more than once. */
#ifndef BASIC_HDRS_H
#define BASIC_HDRS_H 1

/* Basic declarations.  basics.h also pulls in defines.h, which provides
   default configuration parameters for this version. */
#include "basics.h"

/* Language configuration. */
#include "lang_feat.h"

/* Host environment configuration. */
#include "host_envir.h"

/* Target configuration. */
#include "targ_def.h"

#ifdef _lint
/* Directives to control lint diagnostics. */
#include "lint.h"
#endif /* ifdef _lint */

#endif /* ifndef BASIC_HDRS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
