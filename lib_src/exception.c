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

Functions that implement the "bad_exception" class (18.6.2.1).

*/

#include "basics.h"
#include "config.h"
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


bad_exception::bad_exception() THROW_NOTHING()
/*
Constructor for bad_exception.
*/
{
}  /* bad_exception::bad_exception */


bad_exception::bad_exception(const bad_exception& rhs)
THROW_NOTHING() : exception(rhs)
/*
Copy constructor for bad_exception.  Currently does nothing.
*/
{
}  /* bad_exception::bad_exception */


bad_exception& bad_exception::operator=(const bad_exception& rhs)
THROW_NOTHING()
/*
Assignment operator for bad_exception.  Currently does nothing.
*/
{
  /* Call the base class assignment operator. */
  exception::operator=(rhs);
  return *this;
}  /* bad_exception::operator= */


bad_exception::~bad_exception() THROW_NOTHING()
/*
Destructor for bad_exception.
*/
{
}  /* bad_exception::~bad_exception */


const char* bad_exception::what() const THROW_NOTHING()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* bad_exception::what */


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
