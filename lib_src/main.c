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

#include "basics.h"
#include "runtime.h"
#pragma hdrstop
#include "main.h"
#include "static_init.h"


EXTERN_C void __main_called_more_than_once()
/*
This routine is called when _main is called more than once.  It
simply calls abort.  The name is intended to describe the nature
of the problem to the user.
*/
{
  __abort_execution(ec_main_called_more_than_once);
}


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
  /* Make sure that the _main routine is not called more than once. 
     Doing so can result in an infinite loop during static destruction
     because entries on the needed destruction list are improperly linked. */
  if (main_called) __main_called_more_than_once();
  main_called = TRUE;
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
