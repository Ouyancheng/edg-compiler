/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

_handler.C -- set_new_handler routine to allow the user to affect the
             behavior of the default operator new() when memory cannot
             be allocated.

*/

#define NULL 0

typedef void (*pointer_to_function_returning_void) ();

extern pointer_to_function_returning_void _new_handler = NULL;


extern pointer_to_function_returning_void set_new_handler(
             pointer_to_function_returning_void handler)
/*
Set _new_handler to the new function pointer provided and return the
previous value of _new_handler.
*/
{
  pointer_to_function_returning_void rr = _new_handler;
  _new_handler = handler;
  return rr;
}  /* set_new_handler */
