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

#include <stddef.h>
#include <stdlib.h>
#include "basics.h"
#include "runtime.h"
#include "new.h"


#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE

void operator delete[](void*, void*)
/*
Placement operator delete -- does nothing.
*/
{
}  /* operator delete[](void*, void*) */

#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
