/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2026 Edison Design Group Inc.                   [_]          *
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

Exit processing.

*/


/*
Do not include any files that will result in an extern "C" version
of exit being declared.
*/

#include "basics.h"
/* Note that runtime.h is not included. */
#include "edg_exit.h"


extern "C++" NORETURN void exit(int val)
/*
This routine just provides a means of transferring control to our own
version of exit which will do some processing and then call the system
exit routine.  This is needed because one file cannot refer to both
the C and C++ versions of void exit(int).
*/
{
  __edg_exit(val);
}


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2026 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
