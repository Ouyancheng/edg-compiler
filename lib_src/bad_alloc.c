/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2012 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Member functions of the bad_cast class.

*/

#include "basics.h"
#include "runtime.h"

/* The definition of the exception classes can be disabled. */
#if USE_EDG_EXCEPTION_CLASSES

/*
If the runtime should be defined in the std namespace, open
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

#if EXCEPTION_HANDLING

/*
The bad_alloc class is only supplied when the runtime supports exception
handling.
*/

bad_alloc::bad_alloc() THROW_NOTHING()
/*
Constructor for bad_alloc.
*/
{
}  /* bad_alloc::bad_alloc */


bad_alloc::bad_alloc(const bad_alloc& rhs) THROW_NOTHING() : exception(rhs)
/*
Copy constructor for bad_alloc.  Currently does nothing.
*/
{
}  /* bad_alloc::bad_alloc */


bad_alloc& bad_alloc::operator=(const bad_alloc& rhs) THROW_NOTHING()
/*
Assignment operator for bad_alloc.  Currently does nothing.
*/
{
  /* Call the base class assignment operator. */
  exception::operator=(rhs);
  return *this;
}  /* bad_alloc::operator= */


bad_alloc::~bad_alloc() THROW_NOTHING()
/*
Destructor for bad_alloc.
*/
{
}  /* bad_alloc::~bad_alloc */


const char* bad_alloc::what() const THROW_NOTHING()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* bad_alloc::~bad_alloc */

#endif /* EXCEPTION_HANDLING */


/*
If the runtime should be defined in the std namespace, close
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

#endif /* USE_EDG_EXCEPTION_CLASSES */


/******************************************************************************
*                                                             \  ___  /       *
* Edison Design Group C++ Runtime                               /   \         *
*                                                            - | \^/ | -      *
* Copyright 1992-2012 Edison Design Group, Inc.                 \   /         *
* All rights reserved.  Consult your license                  /  | |  \       *
* regarding permissions and restrictions.                        [_]          *
*                                                                             *
******************************************************************************/
