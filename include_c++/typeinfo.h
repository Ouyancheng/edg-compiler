/* Edison Design Group, 1995. */
/*
typeinfo.h -- Include file for type information (18.5.1)
*/
#ifndef _TYPEINFO_H
#define _TYPEINFO_H

#include <stdexcept.h>

/*
If bool is not supported, use a typedef for bool.
*/
#ifdef _BOOL
typedef bool _bool;
#else /* ifndef _BOOL */
typedef int _bool;
#endif /* ifdef _BOOL */

#if 0
namespace std {
#endif /* 0 */
  class type_info {
  public:
    virtual ~type_info();
    _bool operator==(const type_info&) const;
    _bool operator!=(const type_info&) const;
    _bool before(const type_info&) const;
    const char* name() const;
  private:
    type_info(const type_info&);
    type_info& operator=(const type_info&);
    void* _typeinfo;
  };

  class bad_cast : public exception {
  public:
    bad_cast() throw();
    bad_cast(const bad_cast&) throw();
    bad_cast& operator=(const bad_cast&) throw();
    virtual ~bad_cast() throw();
    virtual const char* what() const throw();
  };

  class bad_typeid : public exception {
  public:
    bad_typeid() throw();
    bad_typeid(const bad_typeid&) throw();
    bad_typeid& operator=(const bad_typeid&) throw();
    virtual ~bad_typeid() throw();
    virtual const char* what() const throw();
  };

#if 0
}  /* namespace std */
#endif /* 0 */

#endif /* _TYPEINFO_H */
