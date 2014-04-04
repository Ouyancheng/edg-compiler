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

munch_ctors.c -- Provides the definition of the _ctors variable that is
                 defined by edg_munch (when edg_munch is used).  This is
                 used to avoid undefined symbols when patch is being used.

*/

#include "basics.h" 
#include "runtime.h" 

typedef void (*PFV)();
PFV _ctors[] = {0};

/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2014 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
