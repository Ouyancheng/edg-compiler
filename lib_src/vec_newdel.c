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

_vec_newdel.C -- C++ runtime routines for vector new() and delete() which
                 precedes the adds a header in front of the allocation to
                 track the array size.  The array size is later used by
                 _vec_dtor to call destructors for undimensioned array
                 deletes.

*/

#include <stdlib.h>
#include "_newdel.h"


typedef void *(*ptr_to_new_func) (size_t);
typedef void (*ptr_to_delete_func) (void *);

extern "C" {
	void *_vec_new(size_t, ptr_to_new_func);
	void _vec_delete(char *, ptr_to_delete_func);
}


void *_vec_new(size_t             array_size,
               ptr_to_new_func    new_func)
/*
Using the specified operator new(), allocate the requested storage with a
header to track the size of the array.
*/
{
  char *ptr;
  ptr = (char *)(*new_func)(array_size + sizeof(new_header));
  ((new_header *)ptr)->requested_size = array_size;
#if DEBUG
  ((new_header *)ptr)->magic_number = MAGIC_NUMBER;
#endif /*DEBUG */
  ptr += sizeof(new_header);
  return (void *)ptr;
}  /* _vec_new */


void _vec_delete(void               *array_ptr,
                 ptr_to_delete_func delete_func)
/*
After adjusting the array_ptr for the length of the header preceding array,
call the specified operator delete().
*/
{
  new_header *head_ptr = (new_header *)array_ptr;

  if (array_ptr != NULL) {
    /* Adjust the pointer to the beginning of the header. */
    head_ptr = (new_header *)((char *)array_ptr - sizeof(new_header));
#if DEBUG
    if (head_ptr->magic_number != MAGIC_NUMBER) {
      (void)fprintf(stderr, "EDG runtime - _vec_delete(): ");
      (void)fprintf(stderr, "delete of array not allocated by _vec_new().\n");
      abort();
    }  /* if */
    head_ptr->magic_number = 0;
#endif /* DEBUG */
  }  /* if */

  /* Call the specified delete function. */
  (*delete_func)((void *)head_ptr);
}  /* _vec_delete */


