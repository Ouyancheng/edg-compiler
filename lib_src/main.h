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
struct __linkl	*__head         /* Pointer to the head of the linked list
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
typedef void (*func_ptr)();
extern func_ptr _ctors[];
extern func_ptr _dtors[];

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
