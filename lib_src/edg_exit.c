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

#include "basics.h"
#include "config.h"

/* The version of exit to be called must be the system exit routine not the
   interface routine in the runtime that has C++ linkage. */
extern "C" void exit(int);

#if EXCEPTION_HANDLING
/* Routine in throw.c that does exit processing for exception handling.
   Declared here to prevent pulling in all of eh.h that would redeclare
   exit. */
extern "C" __eh_exit_processing(void);
#endif /* EXCEPTION_HANDLING */

void edg_exit(int val)
/*
Do any wrapup processing required by the runtime including any
exception handling processing that must be done.  Then call the
system exit routine to complete the exit processing.
*/
{
#if EXCEPTION_HANDLING
  __eh_exit_processing();
#endif /* EXCEPTION_HANDLING */
  exit(val);
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
