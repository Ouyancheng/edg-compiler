/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             runtime library.

*/

/*
Note: This is the EDG internal version.  The version shipped as part of
the release should contain no defines.
*/

#define _XOPEN_VERSION 0
#define _POSIX_C_SOURCE 0

#ifdef __sparc
/* The Sparc Solaris version uses a version of gcc that has IA-64 support. */
#define SYSTEM_RUNTIME_HAS_IA64_SUPPORT TRUE
#endif /* ifdef __sparc */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
