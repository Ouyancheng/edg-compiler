/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

fe_util.h -- General utility components (mostly templates) from the front end.

*/

/* Set up prerequisites from checking.h that are unavailable. */

/* check_assertion must produce a void result. */
#ifndef check_assertion
#define check_assertion(test) ((void)0)
#endif  /* check_assertion */

#ifndef check_assertion_str
#define check_assertion_str(test, string) /* Nothing */
#endif  /* check_assertion_str */

#ifndef unexpected_condition
#define unexpected_condition() /* Nothing */
#endif /* unexpected_condition */

#ifndef unexpected_condition_str
#define unexpected_condition_str(string) /* Nothing */
#endif /* unexpected_condition_str */

#ifndef check_assertion_str2
#define check_assertion_str2(test, string1, string2) /* Nothing */
#endif /* check_assertion_str2 */

#ifndef unexpected_condition_str2
#define unexpected_condition_str2(string1, string2) /* Nothing */
#endif /* unexpected_condition_str2 */

/* Set up alternative allocation functions for the general allocator so that
   it can be used in utility programs. */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

inline char *alloc_general(sizeof_t size)
/*
Interface to alloc_general_record_allocation that causes a memory allocation
entry to be created.
*/
{
  return (char*)malloc(size);
}  /* alloc_general */


inline void free_general(a_void_ptr          ptr,
                         ARG_UNUSED sizeof_t size)
/*
Free a block of memory to general storage.
*/
{
  if (ptr == NULL) {
    /* If this assertion fails a null block of memory was given with a non-zero
       size.  Thus, either the caller got the pointer to the block of memory or
       the size wrong. */
    check_assertion(size == 0);
  } else {
    free((char*)ptr);
  }  /* if */
}  /* free_general */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/* Include the actual util.h file. */
#include "util.h"

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
