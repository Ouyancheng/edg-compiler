/* Edison Design Group, 1995. */
/*
stdexcept.h -- Include file for exception handling (see 19.1.1)
*/
#ifndef _STDEXCEPT_H
#define _STDEXCEPT_H

#if 0
namespace std {
#endif /* 0 */
  class exception {
  public:
    exception() throw();
    exception& operator=(const exception&) throw();
    virtual ~exception() throw();
    virtual const char* what() const throw();
  };
#if 0
}  /* namespace std */
#endif /* 0 */

#endif /* _STDEXCEPT_H */

