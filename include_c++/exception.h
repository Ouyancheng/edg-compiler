/* Edison Design Group, 1994. */
/*
exception.h -- Include file for exception handling (see 18.6)
*/
#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include <stdexcept.h>

#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

  class bad_exception : public exception {
  public:
    bad_exception() throw();
    bad_exception(const bad_exception&) throw();
    bad_exception& operator=(const bad_exception&) throw();
    virtual ~bad_exception() throw();
    virtual const char* what() const throw();
  };

  typedef void (*terminate_handler)();
  extern terminate_handler set_terminate(terminate_handler);

  typedef void (*unexpected_handler)();
  extern unexpected_handler set_unexpected(unexpected_handler);

  /* unexpected and terminate are in the WP definition of exception.h.
     It is not clear why. */
  void terminate();
  void unexpected();

#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace */

#ifdef __EDG_IMPLICIT_USING_STD
/* Implicitly include a using directive for the STD namespace when this
   preprocessing flag is TRUE. */
using namespace std;
#endif /* ifdef __EDG_IMPLICIT_USING_STD */

#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

#endif /* _EXCEPTION_H */

