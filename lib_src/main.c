/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

_main routine -- called by main to handle calling of static constructors
and destructors. 

*/

#include <stddef.h>
#include "basics.h"
#include "main.h"
#include "static_init.h"



extern "C" {
	void _main (void);
}

void _main ()
/*
Perform any static initializations that are needed and perform any
setup needed to ensure that static destruction is done at the
end of execution.  The actual operations are separated into
other functions to allow the replacement of this "_main" with
another version.  This shouldn't be necessary but it is done by
version 3.0 of the NIH libraries.
*/
{
  __call_ctors();
}  /* _main */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
