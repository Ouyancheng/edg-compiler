/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

Declarations for EDG equivalent of the AT&T munch utility.

*/


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
Type code output by "nm" for externally visible function definitions.
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
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
