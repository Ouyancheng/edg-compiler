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

C++ operator new(size_t, void*);

*/

#include "basics.h"
#include "runtime.h"

#if DEFINE_PLACEMENT_NEW_ROUTINES

void *operator new(size_t, void *ptr) THROW_NOTHING()
/*
Return the value of ptr as the address of the new object.
*/
{
  return ptr;
}  /* operator new (size_t, void*) */

#endif /* DEFINE_PLACEMENT_NEW_ROUTINES */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
