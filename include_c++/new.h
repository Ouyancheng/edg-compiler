/* Edison Design Group, 1992. */
/*
new.h -- Include file for C++ default operator new (see ARM 12.5).
*/

#ifndef __NEW_H
#define __NEW_H

#ifndef __STDDEF_H
#include <stddef.h>
#endif

#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

  typedef void (*__new_handler)();
  __new_handler set_new_handler(__new_handler);

#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */

/* The following function should not be here according to the strict
   language definition, but many compilers provide it, and users count
   on it to do a simple placement new. */
void *operator new(size_t, void*);

#endif
