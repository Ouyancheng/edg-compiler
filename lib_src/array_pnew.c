/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

C++ operator new[](size_t, void*) (placement array new).

*/


#include <stddef.h>
#include <stdlib.h>
#include "basics.h"
#include "config.h"

#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE

void *operator new[](size_t, void* ptr)
/*
Placement array operator new.
*/
{
  return ptr;
}  /* operator new[](size_t, void*) */

#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
