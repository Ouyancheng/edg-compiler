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

#include "basics.h"
#include "eh.h"

/* Declare the abort routine. */
EXTERN_C void abort();


void terminate()
/*
The default terminate routine.
*/
{
  abort();
}  /* terminate */


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
The default unexpected routine.
*/
{
  abort();
}  /* unexpected */


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

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
