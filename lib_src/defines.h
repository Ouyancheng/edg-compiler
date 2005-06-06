/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2005 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
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

#ifdef sun
/* The Solaris version uses a version of gcc that has IA-64 support. */
#define SYSTEM_RUNTIME_HAS_IA64_SUPPORT TRUE
#endif /* ifdef sun */

/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2005 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
