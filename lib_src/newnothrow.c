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

C++ operator new() that does not throw an exception.

*/

#include <stddef.h>
#include <stdlib.h>
#include "basics.h"
#include "runtime.h"
#include "new.h"

extern "C" STD_NAMESPACE::__new_handler _new_handler;

/* Note that operator new is not in the std namespace. */

static a_boolean call_new_handler()
/*
Call the new handler routine.  If exception handling is supported, wrap
the call in a try block to see if the new handler throws a bad_alloc
exception.
*/
{
  a_boolean	done = FALSE;
#if EXCEPTION_HANDLING
  try {
    (*_new_handler) ();
  }
  catch (STD_NAMESPACE::bad_alloc) {
    done = TRUE;
  }
#else /* !EXCEPTION_HANDLING */
  (*_new_handler) ();
#endif /* EXCEPTION_HANDLING */
  return done;
}  /* call_new_handler */


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
      /* Call the new handler.  The call_new_handler will return TRUE if
         a NULL pointer should be returned to the caller. */
      if (call_new_handler()) return (void*)NULL;
    } else {
      /* There is no new handler -- Just return a NULL. */
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
