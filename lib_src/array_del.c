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

C++ operator delete();

*/

#include <stdlib.h>


void operator delete[](void *ptr)
/*
Default array operator delete.  Just call the normal operator delete.
*/
{
  operator delete(ptr);
}  /* operator delete[] */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
