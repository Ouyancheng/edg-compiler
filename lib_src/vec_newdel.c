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
#include "vec_newdel.h"
#include "main.h"
#include "config.h"
#include "eh.h"

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


EXTERN_C void _array_pointer_not_from_vec_new();
                               /* Function called when an invalid pointer that
                                  was not allocated by vec_new is passed
                                  to one of the vector handling routines. */


EXTERN_C void	*__nw__FUi(size_t);	/* Mangled name for simple
					   operator new(). */
EXTERN_C void	__dl__FPv(void *);	/* Mangled name for operator delete. */

/*
Increment a void* pointer by a given value.
*/
#define increment_ptr(ptr, incr) (ptr = ((void*)((char*)ptr + incr)))

#if EXCEPTION_HANDLING
static void add_vec_new_or_delete_eh_stack_entry
				(an_eh_stack_entry_ptr	ehsep,
			         a_boolean		is_vec_new)
/*
Link an entry onto the EH stack that describes the vec_new or vec_delete
operation that is in process.
*/
{
  ehsep->next = __curr_eh_stack_entry;
  __curr_eh_stack_entry = ehsep;
  ehsep->kind = ehsek_vec_new_or_delete;
  ehsep->variant.vec_new_del.array_ptr              = NULL;
  ehsep->variant.vec_new_del.number_of_elements     = 0;
  ehsep->variant.vec_new_del.element_size           = 0;
  ehsep->variant.vec_new_del.elements_processed     = 0;
  ehsep->variant.vec_new_del.is_vec_new             = is_vec_new;
  ehsep->variant.vec_new_del.free_memory_on_cleanup = FALSE;
  ehsep->variant.vec_new_del.destructor		    = NULL;
}  /* add_vec_new_or_delete_eh_stack_entry */
#endif /* EXCEPTION_HANDLING */


/*ARGSUSED*/ /* <-- "dtor" is only used when EXCEPTION_HANDLING is TRUE. */
EXTERN_C void *__vec_new_eh(void                         *array_ptr,
                            int                          number_of_elements,
                            size_t                       element_size,
                            a_constructor_ptr	 	 ctor,
                            a_destructor_ptr	         dtor)

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

dtor is a pointer to the destructor for objects of the element type.
This is used by the exception handling mechanism for object cleanup
if an exception is thrown while the array is being constructed.
If there is no destructor then dtor is NULL and no cleanup is done.

This routine uses a linked list to record the number of elements in the
array.  Consequently, the performance degrades if a large number of arrays
are allocated.  An algorithm that performs better with large numbers of
elements should be used in a production runtime system.
*/
{
  register vec_info_ptr info_ptr;
  size_t   array_size;
  int      i;
  void     *arr_ptr;

#if EXCEPTION_HANDLING
  an_eh_stack_entry	ehse;
  if (dtor != NULL) {
    add_vec_new_or_delete_eh_stack_entry(&ehse, /*is_vec_new=*/TRUE);
    ehse.variant.vec_new_del.free_memory_on_cleanup = array_ptr == NULL;
    ehse.variant.vec_new_del.number_of_elements     = number_of_elements;
    ehse.variant.vec_new_del.element_size           = element_size;
    ehse.variant.vec_new_del.destructor		    = dtor;
  }  /* if */
#endif /* EXCEPTION_HANDLING */
  if (array_ptr == NULL) {
    /* Allocate the needed memory and construct the "hidden" array
       information. */
    if (_free_vec_info != NULL) {
      /* Reuse a previously allocated structure. */
      info_ptr = _free_vec_info;
      _free_vec_info = info_ptr->next;
    } else {
      /* Allocate an array information structure from free memory. */
      info_ptr = (vec_info_ptr)malloc(sizeof(vec_info));
      if (info_ptr == NULL) {
        array_ptr = NULL;
        goto error_exit;
      }  /* if */
    }  /* if */
    array_size = number_of_elements * element_size;
    array_ptr = (void *)__nw__FUi(array_size);
    if (array_ptr == NULL) {
      goto error_exit;
    }  /* if */
    info_ptr->next       = _head_vec_info;
    info_ptr->array_ptr  = array_ptr;
    info_ptr->array_size = array_size;
    _head_vec_info  = info_ptr;
  }  /* if */
#if EXCEPTION_HANDLING
  if (dtor != NULL) {
    ehse.variant.vec_new_del.array_ptr = array_ptr;
  }  /* if */
#endif /* EXCEPTION_HANDLING */
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
         i++, increment_ptr(arr_ptr, element_size)) {
#if CFRONT_COMPATIBILITY_MODE
      a_cfront_constructor_ptr	cfront_ctor;
      cfront_ctor = (a_cfront_constructor_ptr)ctor;
      (*cfront_ctor)(arr_ptr, (void *)0, (void *)0, (void *)0, (void *)0,
                     (void *)0, (void *)0, (void *)0, (void *)0);
#else /* CFRONT_COMPATIBILITY_MODE */
      (*ctor)(arr_ptr); 
#endif /* CFRONT_COMPATIBILITY_MODE */
#if EXCEPTION_HANDLING
    if (dtor != NULL) {
      /* Update the counter of the number of elements processed in the
         EH stack entry. */
      ehse.variant.vec_new_del.elements_processed++;
    }  /* if */
#endif /* EXCEPTION_HANDLING */
   }  /* for */
  }  /* if */
#if EXCEPTION_HANDLING
    if (dtor != NULL) {
      /* Unlink the vec_new EH stack entry. */
      __curr_eh_stack_entry = __curr_eh_stack_entry->next;
    }  /* if */
#endif /* EXCEPTION_HANDLING */
error_exit:
  /* Return the pointer to the array. */
  return array_ptr;
}  /* __vec_new_eh */


EXTERN_C void *__vec_new(void                         *array_ptr,
                         int                          number_of_elements,
                         size_t                       element_size,
                         a_constructor_ptr            ctor)
/*
This is an entry point used for compatibility with code generated
before EH was supported.  This simply calls the general version of
vec_new_eh that includes a destructor pointer.
*/
{
  return (__vec_new_eh(array_ptr, number_of_elements, element_size, ctor,
                       (a_destructor_ptr)NULL));
}  /* __vec_new */

#if EXCEPTION_HANDLING
EXTERN_C void __cleanup_vec_new_or_delete(an_eh_stack_entry_ptr ehsep)
/*
Called by the exception handling cleanup routine to do the cleanup
processing for a vec_new or vec_delete operation that was interrupted by
an exception.
*/
{
  /* Call the destructor, if specified, on each element in the array, in
     reverse order. */
  a_destructor_ptr	dtor = ehsep->variant.vec_new_del.destructor;
  a_sizeof_t		number_of_elements;
  a_sizeof_t		element_size;
  void*                 arr_ptr;
  void*			array_ptr;
  a_sizeof_t		i;
  a_sizeof_t		first_element;

  array_ptr = (void *)ehsep->variant.vec_new_del.array_ptr;
  element_size = ehsep->variant.vec_new_del.element_size;
  if (ehsep->variant.vec_new_del.is_vec_new) {
    /* Cleaning up a vec_new.  Destroy the fully constructed elements of
       the array in reverse order. */
    number_of_elements = ehsep->variant.vec_new_del.elements_processed;
    first_element = number_of_elements - 1;
  } else {
    first_element = ehsep->variant.vec_new_del.number_of_elements -
                    ehsep->variant.vec_new_del.elements_processed - 1;
    number_of_elements = first_element + 1;
  }  /* if */
  for (i = 0,
       arr_ptr = (void *)(((char *)array_ptr) + first_element * element_size);
       i < number_of_elements;
       i++, increment_ptr(arr_ptr, -element_size)) {
    /* Call the destructor with 0x2 - whole object = TRUE
                                0x1 - delete object = FALSE. */
    (*dtor)(arr_ptr, 0x2 /*whole object = TRUE, delete = FALSE*/);
  }  /* for */
  if (ehsep->variant.vec_new_del.free_memory_on_cleanup) {
    /* Call the delete routine to free the memory. */
    __dl__FPv(ehsep->variant.vec_new_del.array_ptr);
  }  /* if */
}  /* __cleanup_vec_new_or_delete */
#endif /* EXCEPTION_HANDLING */


/*ARGSUSED*/ /* <-- "unused" is unused. */
EXTERN_C void __vec_delete(void                *array_ptr,
                           int                 number_of_elements,
                           size_t              element_size,
                           a_destructor_ptr    dtor,
                           int                 delete_flag,
                           int                 unused)
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
  void                  *arr_ptr;

  /* If the address of the array is NULL, do nothing. */
  if (array_ptr != NULL ) {
#if EXCEPTION_HANDLING
    an_eh_stack_entry	ehse;
    add_vec_new_or_delete_eh_stack_entry(&ehse, /*is_vec_new=*/FALSE);
    ehse.variant.vec_new_del.free_memory_on_cleanup = delete_flag;
    ehse.variant.vec_new_del.array_ptr              = array_ptr;
    ehse.variant.vec_new_del.number_of_elements     = number_of_elements;
    ehse.variant.vec_new_del.element_size           = element_size;
    ehse.variant.vec_new_del.destructor		    = dtor;
#endif /* EXCEPTION_HANDLING */
    /* Determine the number of elements in the array, if unknown. */
    if (number_of_elements == -1) {
      /* Determine the number of elements from the memory allocation size. */
      /* Find the "hidden" information  for this array. */
      for (prev_ptr = NULL, info_ptr = _head_vec_info;
           (info_ptr != NULL) && (info_ptr->array_ptr != array_ptr);
           info_ptr = info_ptr->next) {
        prev_ptr = info_ptr;
      }  /* for */
      if (info_ptr == NULL) {
        /* This array was not allocated by vec_new, so we do not know the
           size.  Call a function that will abort.  The name should
           be sufficient to identify the nature of the problem to the user. */

        _array_pointer_not_from_vec_new();
      }  /* if */
      if (delete_flag) {
        /* Unhook this array information from the linked list and add to the
           front of the free list. */
        if (prev_ptr == NULL) {
          /* This structure is on the beginning of the linked list. */
          _head_vec_info = info_ptr->next;
        } else {
          prev_ptr->next = info_ptr->next;
        }  /* if */
      }  /* if */
      number_of_elements = info_ptr->array_size / element_size;
    }  /* if */
#if EXCEPTION_HANDLING
    ehse.variant.vec_new_del.number_of_elements     = number_of_elements;
#endif /* EXCEPTION_HANDLING */

    /* Call the destructor, if specified, on each element in the array, in
       reverse order. */
    if (dtor != NULL) {
      for (i = 0,
           arr_ptr = (void*)((char*)array_ptr +
                             (number_of_elements - 1) * element_size);
           i < number_of_elements;
           i++, increment_ptr(arr_ptr, -element_size)) {
#if EXCEPTION_HANDLING
        /* Update the counter of the number of elements processed in the
           EH stack entry.  This is incremented before the destructor is
           called so that, should an exception occur, we won't try
           destroying this element again. */
        ehse.variant.vec_new_del.elements_processed++;
#endif /* EXCEPTION_HANDLING */
        /* Call the destructor with 0x2 - whole object = TRUE
                                    0x1 - delete object = FALSE. */
        (*dtor)(arr_ptr, 0x2 /*whole object = TRUE, delete = FALSE*/);
      }  /* for */
    }  /* if */
#if EXCEPTION_HANDLING
    /* Unlink the vec_new EH stack entry.  This is unlinked before the memory
       for the array is freed.  If an exception occurs during the free
       it should just be handled by the normal mechanism. */
    __curr_eh_stack_entry = __curr_eh_stack_entry->next;
#endif /* EXCEPTION_HANDLING */

    /* Delete the array, if requested. */
    if (delete_flag) {
      __dl__FPv(array_ptr);
      if (info_ptr != NULL) {
        /* Add the vec_info record to the free list. */
        info_ptr->next = _free_vec_info;
        _free_vec_info = info_ptr;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* __vec_delete */


EXTERN_C void _array_pointer_not_from_vec_new()
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
