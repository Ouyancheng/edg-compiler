/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Exit processing.

*/


/*
Do not include any files that will result in an extern "C" version
of exit being declared.
*/

#include "edg_exit.h"


extern "C++" void exit(int val)
/*
This routine just provides a means of transfering control to our own
version of exit which will do some processing and then call the system
exit routine.  This is needed because one file cannot refer to both
the C and C++ versions of void exit(int).
*/
{
  edg_exit(val);
}


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
