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

C++ runtime routine __pure_virtual_called() -- called when the user
calls a pure virtual function.  This function simply aborts the program.

*/

#include "basics.h"
#include "runtime.h"

extern "C" {
	void __pure_virtual_called(void);
}


void __pure_virtual_called(void)
/*
Notify the user that a call to a pure virtual function has been made and
abort the program.
*/
{
  abort();
}  /* __pure_virtual */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
