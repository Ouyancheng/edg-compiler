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
#include <stdio.h>

#ifndef DEBUG
/* Include debugging code. */
#define DEBUG 0
#endif /* ifndef DEBUG */
#if DEBUG
EXTERN int	__debug_level /* = 0 */;
			/* Debug level.  0 means no debug output, 1 - 5
                            means increasing amounts. */
#endif /* DEBUG */
#if DEBUG || CHECKING
EXTERN FILE	*__f_debug initial_value(stderr);
			/* Debug output file. */
#endif /* DEBUG || CHECKING */

/*
Error routines.
*/
#if CHECKING
/* Macro to test an assertion and generate an internal error if
   the condition is not TRUE.  The macro expands to nothing when checking
   code is not being used. */
/* stdlib.h is included is to define abort(). */
#include <stdlib.h>
#define assert(test) \
  {									\
    if (!(test)) {							\
      (void)fprintf(__f_debug, "Assertion failed in file \"%s\", line %d\n", \
                    __FILE__, __LINE__);				\
      abort();								\
    }  /* if */								\
  }

#define check_assertion(test)						\
  assert(test)
#define unexpected_condition()						\
  assert(FALSE)
#else /* !CHECKING */
#define check_assertion(test) /* Nothing */
#define unexpected_condition()    /* Nothing */
#endif /* CHECKING */

/*
a_size_of_t is used as a synonym for size_t by the runtime.
*/
#if 0
This should probably use some other kind of test.
#endif /* 0 */
#if __cplusplus
typedef size_t a_sizeof_t;
#else /* __cplusplus */
typedef unsigned int a_sizeof_t;
#endif /* __cplusplus */

typedef void (*a_destructor_ptr)(void*, int);
			/* Type used to store a pointer a destructor. */

typedef void (*a_delete_ptr)(void*);
			/* Type used to store a pointer to an operator delete
			   routine. */

typedef void (*a_two_operand_delete_ptr)(void *, a_sizeof_t);
			/* Type used to store a pointer to a two operand
			   operator delete routine. */

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
