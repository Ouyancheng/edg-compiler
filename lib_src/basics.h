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

Declarations for all runtime routines.

*/

#ifndef BASICS_H
#define BASICS_H 1

#include <stdio.h>
#include <limits.h>

/*
EXTERN is defined usually as "extern"; in the translation unit that
actually defines storage for external variables, it is defined as an
empty string.  EXTERN is used on the declarations of external variables
in .h files.  This scheme makes it easy to define them in only one
place while using the same source in all places.  Likewise, 
VAR_INITIALIZERS is defined to cause inclusion of initializers for those
variables.
*/
#ifndef EXTERN
#define EXTERN extern
#endif /* ifndef EXTERN */
#ifndef VAR_INITIALIZERS
#define VAR_INITIALIZERS 0
#endif /* ifndef VAR_INITIALIZERS */

/* Macro used to provide an initial value for variables declared EXTERN. */
#if VAR_INITIALIZERS
#define initial_value(value) = value
#else /* VAR_INITIALIZERS */
#define initial_value(value) /* nothing */
#endif /* VAR_INITIALIZERS */

#ifndef DEBUG
/* Include debugging code. */
#define DEBUG 1
#endif /* ifndef DEBUG */
#if DEBUG
EXTERN int	__debug_level /* = 0 */;
			/* Debug level.  0 means no debug output, 1 - 5
                            means increasing amounts. */
EXTERN FILE	*__f_debug initial_value(stderr);
			/* Debug output file. */
#endif /* DEBUG */

#ifndef CHECKING
/* Include consistency-checking code. */
#define CHECKING 1
#endif /* ifndef CHECKING */

/*
EXTERN_C is used to declare an external function with C linkage.  When
compiling with a C compiler this is just set to ``extern'', but when
compiling with a C++ compiler it is set to ``extern "C"''.
*/
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else /* !defined(__cplusplus) */
#define EXTERN_C extern
#endif /* __cplusplus */

/*
Definition of a generic byte.  Always "unsigned char".
*/
typedef unsigned char a_byte;

/* Simple boolean type: */
typedef int	a_boolean;
typedef a_byte	a_byte_boolean;
#define FALSE 0
#define TRUE 1

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

#endif /* BASICS_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
