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

_new.C -- C++ operator new();

*/

#include <stddef.h>
#include <stdlib.h>
#include "_newdel.h"

typedef void (*pointer_to_function_returning_void) ();

extern pointer_to_function_returning_void _new_handler;


extern void *operator new(size_t size)
/*
Allocate the specified memory size from free store.  If the amount of
memory allocated or the size requested is 0 bytes, call *_new_handler()
if defined (non-NULL pointer).
*/
{
  char *ptr;
  size_t new_size;

  if (size == 0) {
    /* Call *_New_handler() if defined. */
      if (_new_handler != NULL)
        (*_new_handler) ();
      else
        return (void *)NULL;
  } else {
    new_size = size + sizeof(new_header);
    while ((ptr = malloc(new_size)) == NULL) {
      if (_new_handler != NULL)
        (*_new_handler) ();
      else
        return (void *)NULL;
    }  /* while */
  }  /* if */
  ((new_header *)ptr)->requested_size = size;
#if DEBUG
  ((new_header *)ptr)->magic_number = MAGIC_NUMBER;
#endif /*DEBUG */
  ptr += sizeof(new_header);
  return (void *)ptr;
}  /* operator new */

