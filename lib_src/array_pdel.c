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

C++ operator delete[](size_t, void*);

*/

#include "basics.h"
#include "runtime.h"


#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE && ABI_CHANGES_FOR_PLACEMENT_DELETE

void operator delete[](void*, void*) THROW_NOTHING()
/*
Placement operator delete -- does nothing.
*/
{
}  /* operator delete[](void*, void*) */

#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE &&
          ABI_CHANGES_FOR_PLACEMENT_DELETE*/

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
