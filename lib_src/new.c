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

new.C -- C++ operator new();

*/

#include <stddef.h>
#include <stdlib.h>

typedef void (*pointer_to_function_returning_void) ();

extern pointer_to_function_returning_void _new_handler;


extern void *operator new(size_t size)
/*
Allocate the specified memory size from free store.  If the amount of
memory allocated or the size requested is 0 bytes, call *_new_handler()
if defined (non-NULL pointer).
*/
{
  void *ptr;

  while ((ptr = malloc(size)) == NULL) {
    if (_new_handler != NULL)
      (*_new_handler) ();
    else
      return (void *)NULL;
  }  /* while */
  return ptr;
}  /* operator new */


