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
#include "config.h"
#include "runtime.h"

#if EXCEPTION_HANDLING

#include "exception.h"

exception::exception() throw()
/*
Constructor for exception.
*/
{
}  /* exception::exception */


exception& exception::operator=(const exception&) throw()
/*
Assignment operator for exception.  Currently does nothing.
*/
{
  return *this;
}  /* exception::operator= */


exception::~exception() throw()
/*
Destructor for exception.
*/
{
}  /* exception::~exception */


const char* exception::what() const throw()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* exception::~exception */


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
