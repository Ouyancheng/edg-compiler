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

static_init.c -- called by _main to handle calling of static constructors
                 and destructors. 

*/

#include <stddef.h>
#include <stdlib.h>
#include "basics.h"
#include "static_init.h"
#include "main.h"
#include "config.h"


/*
Indicates whether the executable is set up to use the "patch" method
of static initialization.  If this is FALSE then it is assumed that
the "munch" method is being used.
*/
static int use_patch_info = TRUE;


void __call_dtors()
/*
Call functions to perform static destruction of objects.  This routine
uses two different methods of calling the destructors based on whether
the executable has been processed using the "patch" or "munch" system.
*/
{
  struct __linkl	*link_ptr;
  static a_boolean	dtors_done = FALSE;

  if (!dtors_done) {
    dtors_done = TRUE;
    if (use_patch_info) {
      /* Walk through the linked list of constructor/destructor function
         pointers, which are now in reverse order, and call each termination
         routine, if defined.  The global variable __head points to the
         linked list. */
      for (link_ptr = __head; link_ptr != NULL; link_ptr = link_ptr->next) {
        if (link_ptr->dtor != NULL) {
          (*(link_ptr->dtor))();
        }  /* if */
      }  /* for */
    } else {
      /* Using munch information.  Call the destructors in the _dtors
         array in reverse order. */
      int   pos = 0;
      while (_dtors[pos]) pos++;
      while (pos--) (_dtors[pos])();
    }  /* if */
  }  /* if */
}  /* __call_dtors */


#if !defined(sun) && USE_ATEXIT == 0
/* Declare low-level routine or system call that exits a process. */
extern "C" void _exit(int status);


void exit(int status)
/*
This is used on systems that don't support a protocol such as the 
ANSI C "atexit" or the SunOS "on_exit" functions.  This version of exit
should be used instead of the normal OS exit routine.  Which means that
the library containing this routine needs to be linked in ahead of
the standard C library (such as libc.a).  Using this version of exit
means that some processing done by special versions of exit (such as the
profiling version) will not be done.
*/
{
  __call_dtors();
  _exit(status);
}  /* exit */
#endif /* !defined(sun) && USE_ATEXIT == 0 */


#if defined(sun) && USE_ATEXIT == 0
/* Used to register a function to be called by exit to do wrapup
   processing. */
extern "C" void on_exit(void (*)(), char *);
#endif /* defined(sun) && USE_ATEXIT == 0 */


void __call_ctors()
/*
Call functions to perform static construction of objects.  This routine
first determines whether the executable as been processed using the
"patch" or "munch" utility and then uses the appropriate method to
call the static initializer functions.
*/
{
  struct __linkl	*link_ptr;
  struct __linkl	*reverse_ptr;
  struct __linkl	*next_ptr;

  /* Figure out whether we should use "patch" initialization or "munch"
     initialization.  "patch" uses a tool to link together a list of
     pointers to static constructors and destructors.  Munch generates
     an array of pointers to constructors and destructors.  The array
     is linked into the executable. */
  
  use_patch_info = (__head != NULL);
  if (use_patch_info) {
    /* Walk through the linked list of constructor/destructor function
       pointers and call each initialization (constructor) function.
       The list is pointed to by the global variable __head.  As each __linkl
       structure is visited, reverse the order of the linked list. */
    for (link_ptr = __head, reverse_ptr = NULL;
         link_ptr != NULL;
         link_ptr = next_ptr ) {

      /* Save the pointer to the next item on the list. */
      next_ptr = link_ptr->next;

      /* If the initialization routine pointer is non-NULL, call the 
         initialization function. */
      if (link_ptr->ctor != NULL) {
        (*(link_ptr->ctor))();
      }  /* if */

      /* Reverse the order of the linked list.  The first will now point 
         nowhere (NULL).  The second will point to the first, etc.. */
      link_ptr->next = reverse_ptr;
      reverse_ptr = link_ptr;
    }  /* for */

    /* Point to the new head of the list. */
    __head = reverse_ptr;
  } else {
    /* Using munch information.  Call the constructors in the _ctors
       array. */
    int   pos = 0;
    while (_ctors[pos]) (*_ctors[pos++])();
  }  /* if */

  /* Establish that the termination routines should be called when exit()
     is called or when main() returns normally. */
#if USE_ATEXIT
  atexit(__call_dtors);
#elif defined(sun)
  on_exit(__call_dtors, (char *)NULL);
#endif /* USE_ATEXIT */
}  /* __call_ctors */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
