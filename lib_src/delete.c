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

_delete.C -- C++ operator delete();

*/

#include <stdlib.h>
#include <stdio.h>
#include "_newdel.h"


extern void operator delete(void *ptr)
/*
Free the memory pointed to by ptr.
*/
{
  if (ptr != NULL) {
    new_header *head_ptr;

    /* Adjust the pointer to the beginning of the header. */
    head_ptr = (new_header *)((char *)ptr - sizeof(new_header));
#if DEBUG
    if (head_ptr->magic_number != MAGIC_NUMBER) {
      (void)fprintf(stderr,
                    "EDG runtime - delete(): delete of unallocated memory.\n");
      abort();
    }  /* if */
    head_ptr->magic_number = 0;
#endif /* DEBUG */
    free((char *)head_ptr);
  }  /* if */
}  /* operator delete */


