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

_vec_newdel.C -- C++ runtime routines to provide vector new() and 
                 delete() functionality.  A hidden structure is used
                 to track array size for later use by _vec_ctor() and 
                 _vec_dtor() processing of an undimensioned array .

*/

#include <stdlib.h>
#include "_newdel.h"


typedef void (*ptr_to_delete_func) (void *);

extern "C" {
	void	*_vec_new(void *, size_t);
	void 	_vec_delete(char *, ptr_to_delete_func);
}


vec_info_ptr _head_vec_info = NULL;
				/* Pointer to the beginning of the linked list
				   of array information. */
vec_info_ptr _free_vec_info = NULL;
				/* Pointer to a list of free array information
				   structures. */


void *_vec_new(void     *array_ptr,
               size_t   array_size)

/*
Construct the "behind the scenes" information to keep track of array
sizes for constructor or destructor calls.  The new information is added
to the front of the linked list pointed to by _head_vec_info.
*/
{
  register vec_info_ptr info_ptr;

  if (array_ptr != NULL) {
    if (_free_vec_info != (vec_info_ptr)NULL) {
      /* Reuse a previously allocated structure. */
      info_ptr = _free_vec_info;
      _free_vec_info = info_ptr->next;
    } else {
      /* Allocate an array information structure from free memory. */
      info_ptr = (vec_info_ptr)malloc((size_t)sizeof(vec_info));
    }  /* if */
    info_ptr->next       = _head_vec_info;
    info_ptr->array_ptr  = array_ptr;
    info_ptr->array_size = array_size;
    _head_vec_info  = info_ptr;
  }  /* if */
  return array_ptr;
}  /* _vec_new */


void _vec_delete(char               *array_ptr,
                 ptr_to_delete_func delete_func)
/*
After locating and freeing the array information block, call the
specified operator delete().
*/
{
  vec_info_ptr prev_ptr;
  register vec_info_ptr info_ptr;

  /* Locate the "hidden" information structure. */
  for (prev_ptr = (vec_info_ptr)NULL, info_ptr = _head_vec_info;
       (info_ptr != (vec_info_ptr)NULL) && (info_ptr->array_ptr != array_ptr);
       prev_ptr = info_ptr, info_ptr = info_ptr->next) {
  }  /* for */
  if (info_ptr != (vec_info_ptr)NULL) {
    /* Unhook this array information from the linked list and add to the front
       of the free list. */
    if (prev_ptr == (vec_info_ptr)NULL) {
      /* This structure is on the beginning of the linked list. */
      _head_vec_info = info_ptr->next;
    } else {
      prev_ptr->next = info_ptr->next;
    }  /* if */
    info_ptr->next = _free_vec_info;
    _free_vec_info = info_ptr;
  }  /* if */

  /* Call the specified delete function. */
  (*delete_func)((void *)array_ptr);
}  /* _vec_delete */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
