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

C++ operator new();

*/

#include <stddef.h>
#include <stdlib.h>

typedef void (*pointer_to_function_returning_void) ();

extern pointer_to_function_returning_void _new_handler;


extern void *operator new(size_t size)
/*
Allocate the specified memory size from free store.  If the allocation fails,
call *_new_handler() if defined (non-NULL pointer), and try the allocation
again.  The new_handler is permitted to
	- cause more memory to be available,
	- throw an exception, or
	- call exit or abort.

If the size passed by the caller is zero, it is incremented to one
because the behavior of malloc is unspecified when size is zero.
In C++, a call of operator new(0) must return a value distinct from other
calls of operator new.
*/
{
  void *ptr;

  if (size == 0) size = 1;
  while ((ptr = (void *)malloc(size)) == NULL) {
    if (_new_handler != NULL) {
      (*_new_handler) ();
    } else {
      return (void *)NULL;
    }  /* if */
  }  /* while */
  return ptr;
}  /* operator new */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
