/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2011 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
/*

_main routine -- called by main to handle calling of static constructors
and destructors. 

*/

#include "basics.h"
#include "runtime.h"
#pragma hdrstop
#include "main.h"
#include "static_init.h"


EXTERN_C void _main ()
/*
Perform any static initializations that are needed and perform any
setup needed to ensure that static destruction is done at the
end of execution.  The actual operations are separated into
other functions to allow the replacement of this "_main" with
another version.  This shouldn't be necessary but it is done by
version 3.0 of the NIH libraries.
*/
{
  static a_boolean	main_called = FALSE;
  /* If main is called more than once, only invoke the static constructors
     the first time.  Doing otherwise can result in an infinite loop during
     static destruction because entries on the needed destruction list are
     improperly linked. */
  if (!main_called) {
    main_called = TRUE;
    __call_ctors();
  }  /* if */
}  /* _main */


/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2011 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
