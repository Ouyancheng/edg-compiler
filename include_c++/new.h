/* Edison Design Group, 1992. */
/*
new.h -- Include file for C++ default operator new (see ARM 12.5).
*/

#ifndef __NEW_H
#define __NEW_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif
#ifndef _EXCEPTION_H
#include <exception.h>
#endif /* _EXCEPTION_H */

#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

  class bad_alloc : public exception {
  public:
    bad_alloc() throw();
    bad_alloc(const bad_alloc&) throw();
    bad_alloc& operator=(const bad_alloc&) throw();
    virtual ~bad_alloc() throw();
    virtual const char* what() const throw();
  };

  typedef void (*__new_handler)();
  __new_handler set_new_handler(__new_handler);
  struct nothrow {};

#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */

#ifdef __EDG_IMPLICIT_USING_STD
/* Implicitly include a using directive for the STD namespace when this
   preprocessing flag is TRUE. */
using namespace std;
#endif /* ifdef __EDG_IMPLICIT_USING_STD */

#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

/*
Normal operator new routine.
*/
void *operator new(size_t) /* throw(__EDG_STD_NAMESPACE::bad_alloc) */;

/*
Nothrow version of operator new.
*/
void *operator new(size_t, const __EDG_STD_NAMESPACE::nothrow&) /* throw()*/;

/*
Placement new.  This was not in the ARM, but it is now standard in
[lib.new.delete.placement].
*/
void *operator new(size_t, void*);

/*
Placement delete.
*/
#if 0
void operator delete(void*, void*);
#endif

#ifdef __ARRAY_OPERATORS
/*
Placement array new.
*/
void *operator new[](size_t, void*) /* throw()*/;

/*
Placement array delete.
*/
#if 0
void operator delete[](void*, void*);
#endif

/*
Nothrow version of array new.
*/
void *operator new[](size_t, const __EDG_STD_NAMESPACE::nothrow&) /* throw()*/;
#endif /* __ARRAY_OPERATORS */

#endif
