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

C++ runtime routine __deleted_virtual_called/__cxa_deleted_virtual() -- called
when the user calls a deleted virtual function.  This function simply aborts
the program.

*/

#include "basics.h"
#include "runtime.h"

#ifdef __EDG_IA64_ABI
#define DELETED_VIRTUAL_FUNCTION_NAME ABI_NAMESPACE::__cxa_deleted_virtual
#else /* !defined(__EDG_IA64_ABI) */
#define DELETED_VIRTUAL_FUNCTION_NAME __deleted_virtual_called
#endif /* !defined(__EDG_IA64_ABI) */


EXTERN_C void DELETED_VIRTUAL_FUNCTION_NAME(void)
/*
Notify the user that a call to a deleted virtual function has been made and
abort the program.
*/
{
  __abort_execution(ec_deleted_virtual_called);
}  /* DELETED_VIRTUAL_FUNCTION_NAME */


/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2011 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
