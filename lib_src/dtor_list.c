/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

dtor_list.c -- destruction list processing.

*/

#include "basics.h"
#include "runtime.h"
#include "dtor_list.h"

/*
The list of static objects that require destruction.  An entry is
added to the front of this list each time a new destructible static
object is created.
*/
static a_needed_destruction_ptr
		needed_destruction_head /* = NULL*/;


void __process_needed_destructions(void)
/*
Go through the needed destructions list and perform the required
destructions.
*/
{
  a_needed_destruction_ptr	ndp;
  while (needed_destruction_head != NULL) {
    void	*object_ptr;
    /* Note that the value of needed_destruction_head may change
       during the execution of the destructor.  Consequently, the
       current entry is removed from the list before the destructor
       routine is called. */
    ndp = needed_destruction_head;
    needed_destruction_head = needed_destruction_head->next;
    object_ptr = ndp->object;
    /* Choose between a simple and complex destruction based on whether
       or not the object pointer is NULL. */
    if (object_ptr != NULL) {
      a_destructor_ptr	dp;
      /* Destroy the object by calling a destructor.  The flag value of 2
         indicates the object should be destroyed, but operator delete
         should not be called. */
      dp = (a_destructor_ptr)ndp->destruction_routine;
      (dp)(object_ptr, 2);
    } else {
      /* Destroy the object by calling a special function that will do the
         destruction of this specific object. */
      (ndp->destruction_routine)();
    }  /* if */
  }  /* while */
}  /* __process_needed_destructions */


EXTERN_C void __record_needed_destruction(a_needed_destruction_ptr ndp)
/*
Called when a static object has been constructed to register a
destruction that must be done at program termination.  ndp points to
a needed destruction entry that is to be added to the front of the
list of needed destructions.
*/
{
  ndp->next = needed_destruction_head;
  needed_destruction_head = ndp;
}  /* __record_needed_destruction */


#if CFRONT_COMPATIBILITY_MODE
EXTERN_C void __std__needed_destruction_list(void)
/*
This routine is provided for use with a cfront runtime library, including
cfront startup and termination code.  This routine will be called by
the cfront static destruction routines and will ensure that any needed
destructions get done.  The sequence of the destructions will not
be standard conforming, but there is no way to get standard conforming
behavior when using the cfront termination routines.

This routine will only be used when munch is being used.  When patch
is being used, the link structure defined below will result in a call to
__process_needed_destructions.
*/
{
  __process_needed_destructions();
}  /* __std__needed_destruction_list */


/*
Define a link structure that will be used when patch is being used.
*/
struct a_link {
  a_link	*next;
  a_void_function_ptr
		ctor;
  a_void_function_ptr
		dtor;
};

static a_link __link = {(a_link*)NULL,
                        (a_void_function_ptr)NULL,
                        (a_void_function_ptr)__process_needed_destructions};

static void dummy(a_link *ptr)
/*
Suppress unused warning on __link.
*/
{
  dummy(&__link);
}  /* dummy */
#endif /* CFRONT_COMPATIBILITY_MODE */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
