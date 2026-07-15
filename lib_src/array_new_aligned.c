/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2026 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*

C++ operator new[]();

*/


#include "basics.h"
#include "runtime.h"

#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__

void *operator new[](size_t size, STD_NAMESPACE::align_val_t align) NEW_THROWS
/*
Default aligned array operator new.  Just call the normal aligned operator new.
*/
{
  return operator new(size, align);
}  /* operator new[] */

#endif /* ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__ */
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2026 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
