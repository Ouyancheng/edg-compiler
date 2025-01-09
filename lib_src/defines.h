/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*

defines.h -- Defines configuration parameters for a given version of the
             runtime library.

*/

#define _XOPEN_VERSION 0
#define _POSIX_C_SOURCE 0

#ifdef __sun
/* The Solaris version uses a version of gcc that has IA-64 support. */
#define SYSTEM_RUNTIME_HAS_IA64_SUPPORT TRUE
/* The following is needed to get the declaration for memalign. */
#define __EXTENSIONS__
#endif /* ifdef __sun */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
