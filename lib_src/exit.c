/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2014 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
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


extern "C++" void exit(int val)
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
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2014 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
