/* Edison Design Group, 1994. */
/*
exception.h -- Include file for exception handling (see 17.3.2)
*/
#ifndef _EXCEPTION_H
#define _EXCEPTION_H

typedef void (*_PFV)();
extern _PFV set_terminate(_PFV);
extern _PFV set_unexpected(_PFV);

#endif /* _EXCEPTION_H */

