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

C++ runtime routine to execute a copy constructor for each element of an array.

*/

#include <stdlib.h>


typedef void (*a_ptr_to_func_returning_void) (void *, void *);

extern "C" {
	void _vec_cctor(void *, size_t, size_t, a_ptr_to_func_returning_void,
                        void *);
}


void _vec_cctor(void                         *array_ptr,
                size_t                       number_of_elements,
                size_t                       element_size,
                a_ptr_to_func_returning_void ctor,
                void                         *src_array_ptr)
/*
Walk through the array, calling the specified copy constructor for each
array element.  The corresponding element of the array pointed to by
src_array_ptr is the source operand for the copy constructor.  Because
this runtime routine will only be called for constructor initialization
of member arrays, the number_of_elements can never be zero.
*/
{
  int  i;
  char *arr_ptr;
  char *src_arr;

  if (ctor != NULL) {
    for (i = 0, arr_ptr = (char *)array_ptr, src_arr = (char *)src_array_ptr;
         i < number_of_elements;
         i++, arr_ptr += element_size, src_arr += element_size) {
      (*ctor)((void *)arr_ptr, (void *)src_arr);
    }  /* for */
  }  /* if */
}  /* _vec_ctor */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
