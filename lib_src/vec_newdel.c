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

_vec_newdel.C -- C++ runtime routines for to provide vector new() and 
                 delete() functionality.  The memory allocated for an
                 array is preceded by a header which provides space for
                 the array size.  The array size is later used by
                 _vec_dtor() to call destructors for undimensioned array
                 deletes.

*/

#include <stdlib.h>
#include "_newdel.h"


typedef void (*ptr_to_delete_func) (void *);

extern "C" {
	size_t	_vec_new_size(size_t);
	void	*_vec_new_init(char *, size_t);
	void 	_vec_delete(char *, ptr_to_delete_func);
}


size_t _vec_new_size(size_t array_size)
/*
Increment to requested array size by the length of the memory header that
precedes that user memory.
*/
{
  return array_size + sizeof(new_header);
}  /* _vec_new_size */


void *_vec_new_init(char     *new_ptr,
                    size_t   array_size)

/*
Initialize the memory header preceding the user array memory area with the
array_size.  Return a pointer to the beginning of the user array.
*/
{
  if (new_ptr == NULL) {
    /* Do nothing with a NULL pointer. */
    return (void *)NULL;
  }  /* if */

  ((new_header *)new_ptr)->requested_size = array_size;
#if DEBUG
  ((new_header *)new_ptr)->magic_number = MAGIC_NUMBER;
#endif /*DEBUG */
  return (void *)(new_ptr + sizeof(new_header));
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


