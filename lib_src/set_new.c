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

set_new_handler routine to allow the user to affect the behavior of the
default operator new() when memory cannot be allocated.

*/

#include "basics.h"
#include "runtime.h"
#include "new.h"

#ifndef NULL
#define NULL 0
#endif /* ifndef NULL */

extern "C" STD_NAMESPACE::__new_handler _new_handler = NULL;

/*
If the runtime should be defined in the std namespace, open
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

__new_handler set_new_handler(__new_handler handler)
/*
Set _new_handler to the new function pointer provided and return the
previous value of _new_handler.
*/
{
  __new_handler rr = _new_handler;
  _new_handler = handler;
  return rr;
}  /* set_new_handler */

/*
If the runtime should be defined in the std namespace, close
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
