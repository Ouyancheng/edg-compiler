/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Declarations relating to main.c -- program startup and termination.

*/

#ifndef MAIN_H
#define MAIN_H 1

#ifndef RUNTIME_H
#include "runtime.h"
#endif /* ifndef RUNTIME_H */

#include <stddef.h>

	
/*
The __linkl structure for each source module that has file scope initialization
or termination routines.
*/
struct __linkl {
  struct __linkl
		*next;		/* Pointer to the next struct __linkl in
				   the linked list. */
  void		(*ctor)();	/* Pointer to the initialization function,
				   NULL, if none. */
  void		(*dtor)();	/* Pointer to the termination function,
				   NULL, if none. */
};


/*
The AT&T patch utility will link all the struct __linkl *__link defined
in separate compilations onto a linked list pointed to by __head.
*/
EXTERN struct __linkl	*__head 
				/* Pointer to the head of the linked list
				   of initialization and termination
				   structures. */
#if VAR_INITIALIZERS
                        = NULL
#endif /* VAR_INITIALIZERS */
                              ;


/*
The AT&T munch utility creates arrays of pointers to static constructor
and destructor pointers.
*/
typedef void (*a_void_function_ptr)();
extern a_void_function_ptr _ctors[];
extern a_void_function_ptr _dtors[];


/*
Data structure used to build a list of static object destructions
to be performed at the end of execution.  Entries are added to the
list each time a static object is created.  New objects are added
to the front of the list.
*/
typedef struct a_needed_destruction *a_needed_destruction_ptr;
typedef struct a_needed_destruction {
  a_needed_destruction_ptr
		next;
			/* Pointer to the next entry in the list. */
  void		*object;
			/* Pointer to the object to be destroyed if this
			   is a "simple" destruction, or a NULL pointer
		 	   if this is a "complex" destruction.  A simple
			   destruction is one that can be done with
			   a single call to the destructor passing an
			   object pointer and a destruction flag. */
  union {
    /* When object != NULL */
    a_destructor_ptr
		simple_destruction;
			/* For a simple destruction, this is a pointer to
			   the destructor to be called. */
    /* When object == NULL */
    a_void_function_ptr    
		complex_destruction;
			/* For a complex destruction, this is a pointer
                           to a function that when called, will call the
                           necessary destructors. */
  } variant;
} a_needed_destruction;

#endif /* MAIN_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
