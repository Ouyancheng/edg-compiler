/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_hdrs.h -- Inclusion of header files used by files involved in IL
                lowering.

*/

/* Avoid including these declarations more than once. */
#ifndef LOWER_HDRS_H
#define LOWER_HDRS_H 1

#include "folding.h"
#include "lower_eh.h"
#include "lower_il.h"
#include "lower_init.h"
#include "lower_name.h"
#if MINIMAL_INLINING
#include "inline.h"
#endif /* MINIMAL_INLINING */
#include "lower_c99.h"
#include "pch.h"
				   
#endif /* ifndef LOWER_HDRS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
