/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Linux.
*/

#define __ANSIC__ 1
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 39
#define C_GEN_BE_GENERATES_ANSI_C 1
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define LONG_LONG_ALLOWED 1
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE 1
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */

#define BUILTIN_VA_LIST_OVERRIDE_TYPE "__gnuc_va_list"

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

