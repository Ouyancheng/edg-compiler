/* Edison Design Group, 1995. */
/*
stdexcept.h -- Include file for exception handling (see 19.1.1)
*/
#ifndef _STDEXCEPT_H
#define _STDEXCEPT_H

#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */
  class exception {
  public:
    exception() throw();
    exception& operator=(const exception&) throw();
    virtual ~exception() throw();
    virtual const char* what() const throw();
  };
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

#endif /* _STDEXCEPT_H */

