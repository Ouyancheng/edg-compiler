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
#include "static_init.h"

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
  /* __call_dtors is called here and is also registered as an atexit
     function that will be called by the system exit routine.  This means
     that it will be called twice when the EDG C++ linkage version of
     exit is being used.  This is intended.  There is no ill effect of
     calling __call_dtors more than once. */
  __call_dtors();
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
