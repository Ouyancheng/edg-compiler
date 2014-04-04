/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2014 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Declarations for runtime routines for lowered VLA operations.

*/

#ifndef VLA_ALLOC_H
#define VLA_ALLOC_H

#include "runtime.h"

EXTERN_C void __vla_alloc(void       *ptr,
                          ptrdiff_t  n_bytes);

EXTERN_C void __vla_dealloc(void  *ptr);

#endif /* ifndef VLA_ALLOC_H */

/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2014 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
