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

Placement version of C++ operator new[]();

*/


#include <stddef.h>
#include <stdlib.h>
#include "basics.h"
#include "runtime.h"
#include "new.h"

#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE

void *operator new[](size_t size, const STD_NAMESPACE::nothrow& nothrow_arg)
/*
Nothrow version of array operator new.  Just call the normal nothrow
operator new.
*/
{
  return operator new(size, nothrow_arg);
}  /* operator new[] */

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
