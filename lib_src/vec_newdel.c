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

C++ runtime routines to provide vector new() and delete() functionality.

*/

#include <stdlib.h>
#include "basics.h"
#include "main.h"
#include "config.h"

/*
For arrays, _vec_new() and _vec_delete() will maintain a linked list of 
"hidden" information on each array allocated and subsequently deleted.
This information will be used by _vec_ctor() and _vec_dtor() to determine
the size of undimensioned arrays.
*/

/*
Hidden structure of information for each array "allocated" by new().
*/
typedef struct vec_info *vec_info_ptr;
				/* Pointer to a vector information struct. */
typedef struct vec_info {
  vec_info_ptr next;		/* Pointer to the next structure in a linked
				   list. */
  void 	       *array_ptr;	/* Pointer to array. */
  size_t       array_size;	/* Size of memory in the array. */
} vec_info;


static vec_info_ptr _head_vec_info = NULL;
				/* Pointer to the beginning of the linked list
				   of array information. */

static vec_info_ptr _free_vec_info = NULL;
				/* Pointer to a list of free array information
				   structures. */


extern "C" void _array_pointer_not_from_vec_new();
                               /* Function called when an invalid pointer that
                                  was not allocated by vec_new is passed
                                  to one of the vector handling routines. */


#if CFRONT_COMPATIBILITY_MODE
typedef void (*a_ptr_to_a_constructor)(char*, void* b1, void* b2, void*b3,
                                       void* b4, void* b5, void* b6, void* b7,
                                       void* b8);
#else /* CFRONT_COMPATIBILITY_MODE */
typedef void (*a_ptr_to_a_constructor)(char*);
#endif /* CFRONT_COMPATIBILITY_MODE */

typedef void (*a_ptr_to_destructor) (char *, int);

extern "C" {
        void	*__nw__FUi(size_t);	/* Mangled name for simple
					   operator new(). */
        void	__dl__FPv(void *);	/* Mangled name for operator delete. */

	char	*__vec_new(char *, int, size_t, a_ptr_to_a_constructor);
	void 	__vec_delete(char *, int, size_t, a_ptr_to_destructor,
                            int, int);
}

char *__vec_new(char                         *array_ptr,
                int                          number_of_elements,
                size_t                       element_size,
                a_ptr_to_a_constructor       ctor)

/*
Allocate storage for an array, then call a constructor for each
element of the array.  If array_ptr is NULL, allocate the space for an
array of class objects (with number_of_elements elements each of size
element_size).  Also remember the size of the array in a
behind-the-scenes data structure so that it can be recalled at the
time of the corresponding vec_delete call.  If array_ptr is non-NULL,
it points to an already-allocated array.  If ctor is non-NULL, it
points to a constructor function to be called for each element of the
array (whether the array is allocated here or pre-allocated).  Return
the address of the array.

This routine uses a linked list to record the number of elements in the
array.  Consequently, the performance degrades if a large number of arrays
are allocated.  An algorithm that performs better with large numbers of
elements should be used in a production runtime system.
*/
{
  register vec_info_ptr info_ptr;
  size_t   array_size;
  int      i;
  char     *arr_ptr;

  if (array_ptr == NULL) {
    /* Allocate the needed memory and construct the "hidden" array
       information. */
    array_size = number_of_elements * element_size;
    array_ptr = (char *)__nw__FUi(array_size);
    if (_free_vec_info != NULL) {
      /* Reuse a previously allocated structure. */
      info_ptr = _free_vec_info;
      _free_vec_info = info_ptr->next;
    } else {
      /* Allocate an array information structure from free memory. */
      info_ptr = (vec_info_ptr)malloc(sizeof(vec_info));
    }  /* if */
    info_ptr->next       = _head_vec_info;
    info_ptr->array_ptr  = array_ptr;
    info_ptr->array_size = array_size;
    _head_vec_info  = info_ptr;
  }  /* if */
  /* Call the constructor, if any, for each member of the array.  Note that
     there may be zero elements.  Cfront tacks on what appears to be eight
     additional NULL values to be used as the addresses of the first
     eight virtual base classes.  The EDG compiler generates a special wrapper
     for use by vec_new and doesn't need the additional arguments.  The
     additional arguments here allow cfront-generated vec_new calls to
     be used with this vec_new. */
  if (ctor != NULL) {
    for (i = 0, arr_ptr = array_ptr;
         i < number_of_elements;
         i++, arr_ptr += element_size) {
#if CFRONT_COMPATIBILITY_MODE
      (*ctor)(arr_ptr, (void *)0, (void *)0, (void *)0, (void *)0, (void *)0,
              (void *)0, (void *)0, (void *)0);
#else /* CFRONT_COMPATIBILITY_MODE */
      (*ctor)(arr_ptr); 
#endif /* CFRONT_COMPATIBILITY_MODE */
   }  /* for */
  }  /* if */
  /* Return the pointer to the array. */
  return array_ptr;
}  /* __vec_new */


void __vec_delete(char                *array_ptr,
                 int                 number_of_elements,
                 size_t              element_size,
                 a_ptr_to_destructor dtor,
                 int                 delete_flag,
                 int                 /*unused_arg*/)
/*
Call a destructor for each element of an array, then delete the storage
for the array.  array_ptr points to the array, which has number_of_elements
elements each of size element_size.  If number_of_elements is -1, use the
size stored by vec_new at the time of allocation of this array.
If array_ptr is NULL, this routine does nothing and returns.
If dtor is non-NULL, it points to a destructor function to be called
for each element of the array.  If delete_flag is TRUE, the storage
for the array is deallocated after the destruction; number_of_elements
must be -1 for that case.
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
        /* This array was not allocated by vec_new, so we do not know the
           size.  Call a function that will abort.  The name should
           be sufficient to identify the nature of the problem to the user. */

        _array_pointer_not_from_vec_new();
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
    if (delete_flag) {
      __dl__FPv(array_ptr);
      if (info_ptr != (vec_info_ptr)NULL) {
        /* Unhook this array information from the linked list and add to the
           front of the free list. */
        if (prev_ptr == NULL) {
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
}  /* __vec_delete */


void _array_pointer_not_from_vec_new()
/*
This routine is used when a pointer that was not created by vec_new is
passed to one other vector handling routines that needs to get the size
from the information created by vec_new.  This routine simply aborts.
The name is intended to describe the nature of the problem to the user
*/
{
	abort();
}

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
