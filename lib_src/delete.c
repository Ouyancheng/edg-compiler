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

delete.C -- C++ operator delete();

*/

#include <stdlib.h>


extern void operator delete(void *ptr)
/*
Free the memory pointed to by ptr.
*/
{
  if (ptr != NULL) {
    free(ptr);
  }  /* if */
}  /* operator delete */


