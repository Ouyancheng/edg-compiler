/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2002 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

C++ functions to for one-time initialization guard variables.

*/

#include "basics.h"
#include "runtime.h"

#ifdef __EDG_IA64_ABI

typedef unsigned long long a_guard;
			/* A guard variable.  This definition must
			   match the type selected in
			   lower_init.c:add_first_time_test. */

typedef a_guard *a_guard_ptr;
			/* A pointer to a guard variable. */

/*
None of these functions are presently used by the front end, but
they are required by the ABI.
*/

EXTERN_C int ABI_NAMESPACE::__cxa_guard_acquire(a_guard_ptr guard)
/*
If the guard variable indicates that the guarded variable has already
been initialized, return 0.  Otherwise, mark the guard variable as
initialized and return 1.
*/
{
  char *first_byte = (char *)guard;
  int  initialize = FALSE;

  if (*first_byte == 0) {
    *first_byte = 1;
    initialize = TRUE;
  }  /* if */
  return initialize;
}  /* __cxa_guard_acquire */


EXTERN_C void ABI_NAMESPACE::__cxa_guard_release(a_guard_ptr guard)
/*
Called when the initialization of the guarded object is complete.
*/
{
  /* In a multi-threaded implementation, this function would release a
     lock, but in a single-threaded implementation there is nothing to
     do. */
}  /* __cxa_guard_release */


EXTERN_C void ABI_NAMESPACE::__cxa_guard_abort(a_guard_ptr guard)
/*
The initialization of the guarded object has been aborted due to an
exception being thrown.  Reset the guard so that the initialization 
will be tried again.
*/
{
  *(char *)guard = 0;
}  /* __cxa_guard_abort */

#endif /* ifdef __EDG_IA64_ABI */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2002 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
