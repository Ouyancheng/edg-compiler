/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Miscellaneous declarations for all runtime routines.

*/

#ifndef RUNTIME_H
#define RUNTIME_H 1
#include "config.h"

typedef void (*a_destructor_ptr)(void*, int);
			/* Type used to store a pointer a destructor. */

typedef void (*a_delete_ptr)(void*);
			/* Type used to store a pointer to an operator delete
			   routine. */

#if CFRONT_COMPATIBILITY_MODE
typedef void (*a_cfront_constructor_ptr)(void*, void* b1, void* b2, void*b3,
                                         void* b4, void* b5, void* b6,
					 void* b7, void* b8);
			/* Type of a constructor called from vec_new in
			   cfront mode. */
#endif /* CFRONT_COMPATIBILITY_MODE */

typedef void (*a_constructor_ptr)(void*);
			/* Type of a default constructor called from
			   vec_new. */


#endif /* RUNTIME_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
