/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Declarations for EDG equivalent of the AT&T munch utility.

*/



/*
The following flag indicates whether all external names have an additional
underscore at the beginning.
*/
#define UNDERSCORE_PREFIX TRUE

/*
Names of the arrays to be created containing pointers to the static
constructor and destructor functions to be called.
*/
#define CTORS_ARRAY_NAME "_ctors"
#define DTORS_ARRAY_NAME "_dtors"

/*
Typedef name to be used for the pointer arrays, and return type of the
function.
*/
#define FUNCTION_PTR_TYPEDEF_NAME "func_ptr"
#define FUNCTION_RETURN_TYPE "void"

/*
Type code output by "nm" for externally visable function definitions.
*/
#define EXTERN_TYPE 'T'

/*
Strings that are used to identify the static constructors and
destructors.
*/
#define CTOR_PREFIX "__sti__"
#define DTOR_PREFIX "__std__"


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
