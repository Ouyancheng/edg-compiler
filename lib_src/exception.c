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

bad_exception::bad_exception() throw()
/*
Constructor for bad_exception.
*/
{
}  /* bad_exception::bad_exception */


bad_exception::bad_exception(const bad_exception& rhs) throw() : exception(rhs)
/*
Copy constructor for bad_exception.  Currently does nothing.
*/
{
}  /* bad_exception::bad_exception */


bad_exception& bad_exception::operator=(const bad_exception& rhs) throw()
/*
Assignment operator for bad_exception.  Currently does nothing.
*/
{
  /* Call the base class assignment operator. */
  exception::operator=(rhs);
  return *this;
}  /* bad_exception::operator= */


bad_exception::~bad_exception() throw()
/*
Destructor for bad_exception.
*/
{
}  /* bad_exception::~bad_exception */


const char* bad_exception::what() const throw()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* bad_exception::~bad_exception */


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
