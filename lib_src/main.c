/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2014 Edison Design Group Inc.                   [_]          *
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
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1992-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
