/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Functions that implement the "exception" class (19.1.1).

*/

#include "basics.h"
#include "runtime.h"

#if EXCEPTION_HANDLING

#include "exception.h"

/*
If the runtime should be defined in the std namespace, open
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */


exception::exception() THROW_NOTHING()
/*
Constructor for exception.
*/
{
}  /* exception::exception */


exception& exception::operator=(const exception&) THROW_NOTHING()
/*
Assignment operator for exception.  Currently does nothing.
*/
{
  return *this;
}  /* exception::operator= */


exception::~exception() THROW_NOTHING()
/*
Destructor for exception.
*/
{
}  /* exception::~exception */


const char* exception::what() const THROW_NOTHING()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* exception::~exception */


/*
If the runtime should be defined in the std namespace, close
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */


#endif /* EXCEPTION_HANDLING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
