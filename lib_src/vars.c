/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*

Performs initialization of global variables used by the runtime.

*/

#define EXTERN /* empty */
#define VAR_INITIALIZERS 1

#include "basics.h"
#include "runtime.h"
#include "main.h"
#include "vec_newdel.h"
#include "eh.h"



/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
