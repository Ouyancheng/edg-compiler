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

_pure_virt.C -- C++ runtime routine __pure_virtual_called() to inform the
                user that a pure virtual function has been called and to
                abort the program.

*/

#include <stdlib.h>
#include <stdio.h>


extern "C" {
	void __pure_virtual_called(void);
}


void __pure_virtual_called(void)
/*
Notify the user that a call to a pure virtual function has been made and
abort the program.
*/
{
  (void)fprintf(stderr, "Call of pure virtual function: abort\n");
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
