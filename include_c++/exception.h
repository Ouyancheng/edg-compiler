/* Edison Design Group, 1994. */
/*
exception.h -- Include file for exception handling (see 18.6)
*/
#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include <stdexcept.h>

#if __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* __EDG_RUNTIME_USES_NAMESPACES */

  class bad_exception : public exception {
  public:
    bad_exception() throw();
    bad_exception(const bad_exception&) throw();
    bad_exception& operator=(const bad_exception&) throw();
    virtual ~bad_exception() throw();
    virtual const char* what() const throw();
  };

  typedef void (*_PFV)();
  extern _PFV set_terminate(_PFV);
  extern _PFV set_unexpected(_PFV);

  /* unexpected and terminate are in the WP definition of exception.h.
     It is not clear why. */
  void terminate();
  void unexpected();

#if __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace */
#endif /* __EDG_RUNTIME_USES_NAMESPACES */

#endif /* _EXCEPTION_H */

