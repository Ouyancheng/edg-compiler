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



typedef void (*a_ptr_to_func_returning_void) (char *);

typedef void (*a_ptr_to_destructor) (char *, int);

extern "C" {
extern  void	*__nw__FUi(size_t);	/* Mangled name for simple
					   operator new(). */
        void	__dl__FPv(void *);	/* Mangled name for operator delete. */
	char	*_vec_new(char *, int, size_t, a_ptr_to_func_returning_void);
	void 	_vec_delete(char *, int, size_t, a_ptr_to_destructor,
                            int, int);
}


/*
For arrays, _vec_new() and _vec_delete() will maintain a linked list of 
"hidden" information on each array allocated and subsequently deleted.
This information will be used by _vec_ctor() and _vec_dtor() to determine
the size of undimensioned arrays.
*/

/*
Hidden structure of information for each array "allocated" by _vec_ new().
*/
typedef struct vec_info *vec_info_ptr;
				/* Pointer to a vector information struct. */
typedef struct vec_info {
  vec_info_ptr next;		/* Pointer to the next structure in a linked
				   list. */
  void 	       *array_ptr;	/* Pointer to array. */
  size_t       array_size;	/* Size of memory in the array. */
} vec_info;


vec_info_ptr _head_vec_info = NULL;
				/* Pointer to the beginning of the linked list
				   of array information. */
vec_info_ptr _free_vec_info = NULL;
				/* Pointer to a list of free array information
				   structures. */


char *_vec_new(char                         *array_ptr,
               int                          number_of_elements,
               size_t                       element_size,
               a_ptr_to_func_returning_void ctor)

/*
If array_ptr is NULL, allocate the space for the array of class objects
and construct the "behind the scenes" information to keep track of array
sizes for subsequent _vec_delete() calls.  The number_of_elements and the
element_size are used to calculate the amount of memory needed.  The new
information is added to the front of the linked list pointed to by
_head_vec_info.  If specified, a call of the constructor (may be NULL) for each
element in the array is done.  Again, the element_size is used to determine
the address of each object in the array.
*/
{
  register vec_info_ptr info_ptr;
  int      array_size;
  int      i;
  char     *arr_ptr;

  if (array_ptr == NULL) {
    /* Allocate the needed memory and construct the "hidden" array
       information. */
    array_ptr = (char *)__nw__FUi((size_t)(array_size = number_of_elements *
                                           element_size));
    if (_free_vec_info != NULL) {
      /* Reuse a previously allocated structure. */
      info_ptr = _free_vec_info;
      _free_vec_info = info_ptr->next;
    } else {
      /* Allocate an array information structure from free memory. */
      info_ptr = (vec_info_ptr)__nw__FUi(sizeof(vec_info));
    }  /* if */
    info_ptr->next       = _head_vec_info;
    info_ptr->array_ptr  = array_ptr;
    info_ptr->array_size = array_size;
    _head_vec_info  = info_ptr;
  }  /* if */
  /* Call the constructor, if any, for each member of the array.  Note,
     there may be zero elements. */
  if (ctor != NULL) {
    for (i = 0, arr_ptr = array_ptr;
         i < number_of_elements;
         i++, arr_ptr += element_size) {
      (*ctor)(arr_ptr);
    }  /* for */
  }  /* if */
  /* Return the pointer to the array. */
  return array_ptr;
}  /* _vec_new */


void _vec_delete(char                *array_ptr,
                 int                 number_of_elements,
                 size_t              element_size,
                 a_ptr_to_destructor dtor,
                 int                 delete_flag,
                 int                 /*unused_arg*/)
/*
If an array of objects is provided (array_ptr not NULL), call the
destructor for each element of the array, in reverse order.  Note, do
not allow the destructor to delete the element.  If the number_of_elements
is a -1, the array was allocated by _vec_new(), and the size of the array
is determined by the size originally allocated and maintained in the "hidden"
array information structure for this array.  If specified (by delete_flag = 1),
delete the array and return the "hidden" array information to its available
list.
*/
{
  vec_info_ptr          prev_ptr;
  register vec_info_ptr info_ptr = NULL;
  int                   i;
  char                  *arr_ptr;

  /* If the address of the array is NULL, do nothing. */
  if (array_ptr != NULL ) {
    /* Determine the number of elements in the array, if unknown. */
    if (number_of_elements == -1) {
      /* Determine the number of elements from the memory allocation size. */
      /* Find the "hidden" information  for this array. */
      for (prev_ptr = NULL, info_ptr = _head_vec_info;
           (info_ptr != NULL) && (info_ptr->array_ptr != array_ptr);
           prev_ptr = info_ptr, info_ptr = info_ptr->next) {
      }  /* for */
      if (info_ptr == NULL) {
        /* This array was not allocated with new(); there will be no array
           size to be used to determine the number of elements in the
           array. */
        (void)fprintf(stderr,
     "EDG runtime - _vec_delete(): unknown dimension array not from new().\n");
        abort();
      }  /* if */
      number_of_elements = info_ptr->array_size / element_size;
    }  /* if */

    /* Call the destructor, if specified, on each element in the array, in
       reverse order. */
    if (dtor != NULL) {
      for (i = 0, arr_ptr = array_ptr +
                                 (number_of_elements - 1) * element_size;
           i < number_of_elements;
           i++, arr_ptr -= element_size) {
        /* Call the destructor with 0x2 - whole object = TRUE
                                    0x1 - delete object = FALSE. */
        (*dtor)(arr_ptr, 0x2 /*whole object = TRUE, delete = FALSE*/);
      }  /* for */
    }  /* if (*/

    /* Delete the array, if requested. */
    if (delete_flag == 1) {
      __dl__FPv(array_ptr);
      if (info_ptr != (vec_info_ptr)NULL) {
        /* Unhook this array information from the linked list and add to the
           front of the free list. */
        if (prev_ptr == (vec_info_ptr)NULL) {
          /* This structure is on the beginning of the linked list. */
          _head_vec_info = info_ptr->next;
        } else {
          prev_ptr->next = info_ptr->next;
        }  /* if */
        info_ptr->next = _free_vec_info;
        _free_vec_info = info_ptr;
      }  /* if */
    }  /* if */
  }  /* if */
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
