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

Run-time type identification -- user callable functions.

*/

#include "basics.h"
#include "runtime.h"

#if ABI_CHANGES_FOR_RTTI

#include "rtti.h"
#include <typeinfo>

/*
If the runtime should be defined in the std namespace, open
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */


__bool type_info::operator==(const type_info& rhs) const
/*
Return TRUE if two type_info structures refer to the same type.
*/
{
  a_type_info_impl_ptr  tiip1;
  a_type_info_impl_ptr  tiip2;

  tiip1 = (a_type_info_impl_ptr)this;
  tiip2 = (a_type_info_impl_ptr)&rhs;
  return matching_type_info(tiip1, tiip2);
}  /* type_info::operator== */


__bool type_info::operator!=(const type_info& rhs) const
/*
Return TRUE if two type_info structures do not refer to the same type.
*/
{
  a_type_info_impl_ptr  tiip1;
  a_type_info_impl_ptr  tiip2;

  tiip1 = (a_type_info_impl_ptr)this;
  tiip2 = (a_type_info_impl_ptr)&rhs;
  return !matching_type_info(tiip1, tiip2);
}  /* type_info::operator!= */


__bool type_info::before(const type_info& rhs) const
/*
Return TRUE if the type_info for *this precedes the type_info for rhs using
some implementation dependent collating sequence.

This is implemented by comparing the pointers used to represent the type
information.  If the type_info_impl has a unique_id, the address of the
unique_id is used for collation.  If it does not have a unique_id (i.e.,
we know there is only a single type_info_impl structure for the type) then
the address of the single type_info_impl is used for collation.
*/
{
  a_type_info_impl_ptr  tiip1;
  a_type_info_impl_ptr  tiip2;
  void*                 ptr1;
  void*                 ptr2;

  tiip1 = (a_type_info_impl_ptr)this;
  tiip2 = (a_type_info_impl_ptr)&rhs;
  ptr1 = tiip1->unique_id != NULL ? (void*)tiip1->unique_id : (void*)tiip1;
  ptr2 = tiip2->unique_id != NULL ? (void*)tiip2->unique_id : (void*)tiip2;
  return ptr1 < ptr2;
}  /* type_info::before */


const char * type_info::name() const
/*
Returns a pointer to the name string for this type.
*/
{
  a_type_info_impl_ptr  tiip1;

  tiip1 = (a_type_info_impl_ptr)this;
  return tiip1->name;
}  /* type_info::name */


type_info::~type_info()
/*
Destructor for type_info.  This should never actually be called.
*/
{
}  /* type_info::~type_info */


/* The definition of the exception classes can be disabled. */
#if USE_EDG_EXCEPTION_CLASSES

#if EXCEPTION_HANDLING

/*
The bad_cast class is only supplied when the runtime supports exception
handling.
*/

bad_cast::bad_cast() THROW_NOTHING()
/*
Constructor for bad_cast.
*/
{
}  /* bad_cast::bad_cast */


bad_cast::bad_cast(const bad_cast& rhs) THROW_NOTHING() : exception(rhs)
/*
Copy constructor for bad_cast.  Currently does nothing.
*/
{
}  /* bad_cast::bad_cast */


bad_cast& bad_cast::operator=(const bad_cast& rhs) THROW_NOTHING()
/*
Assignment operator for bad_cast.  Currently does nothing.
*/
{
  /* Call the base class assignment operator. */
  exception::operator=(rhs);
  return *this;
}  /* bad_cast::operator= */


bad_cast::~bad_cast() THROW_NOTHING()
/*
Destructor for bad_cast.
*/
{
}  /* bad_cast::~bad_cast */


const char* bad_cast::what() const THROW_NOTHING()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* bad_cast::~bad_cast */


bad_typeid::bad_typeid() THROW_NOTHING()
/*
Constructor for bad_typeid.
*/
{
}  /* bad_typeid::bad_typeid */


bad_typeid::bad_typeid(const bad_typeid& rhs) THROW_NOTHING() : exception(rhs)
/*
Copy constructor for bad_typeid.  Currently does nothing.
*/
{
}  /* bad_typeid::bad_typeid */


bad_typeid& bad_typeid::operator=(const bad_typeid& rhs) THROW_NOTHING()
/*
Assignment operator for bad_typeid.  Currently does nothing.
*/
{
  /* Call the base class assignment operator. */
  exception::operator=(rhs);
  return *this;
}  /* bad_typeid::operator= */


bad_typeid::~bad_typeid() THROW_NOTHING()
/*
Destructor for bad_typeid.
*/
{
}  /* bad_typeid::~bad_typeid */


const char* bad_typeid::what() const THROW_NOTHING()
/*
Return a string providing information about the exception.  Currently,
no additional information is available.
*/
{
  return "";
}  /* bad_typeid::~bad_typeid */

#endif /* EXCEPTION_HANDLING */

#endif /* USE_EDG_EXCEPTION_CLASSES */

/*
If the runtime should be defined in the std namespace, close
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

#endif /* ABI_CHANGES_FOR_RTTI */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
