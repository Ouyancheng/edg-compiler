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

C++ functions to support exception handling.

*/

#include <stdlib.h>
#include "basics.h"
#include "config.h"

#if EXCEPTION_HANDLING
#include "eh.h"

void terminate()
/*
The default terminate routine.
*/
{
  abort();
}  /* terminate */


EXTERN_C void __call_terminate(void)
/*
Used by the EH runtime when terminate needs to be called.  Ensures
that terminate does not return.
*/
{
  __default_terminate_routine();
  abort();
}  /* __call_terminate */


a_void_function_ptr set_terminate(a_void_function_ptr new_func)
/*
Set the terminate routine pointer to the value passed by the caller
and return the old value.
*/
{
  a_void_function_ptr	old_func = __default_terminate_routine;
  __default_terminate_routine = new_func;
  return old_func;
}  /* set_terminate */


void unexpected()
/*
The default unexpected routine.  This routine calls terminate.
*/
{
  __default_terminate_routine();
}  /* unexpected */



EXTERN_C void __call_unexpected(void)
/*
Used by the EH runtime when unexpected needs to be called.  Ensures
that unexpected does not return.
*/
{
  __default_unexpected_routine();
  abort();
}  /* __call_unexpected */


a_void_function_ptr set_unexpected(a_void_function_ptr new_func)
/*
Set the unexpected routine pointer to the value passed by the caller
and return the old value.
*/
{
  a_void_function_ptr	old_func = __default_unexpected_routine;
  __default_unexpected_routine = new_func;
  return old_func;
}  /* set_unexpected */

#endif /* EXCEPTION_HANDLING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
