/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

C++ functions to support exception handling.

*/

#include <stdlib.h>
#include "basics.h"
#include "config.h"
#include "runtime.h"

#if EXCEPTION_HANDLING
#include "eh.h"

/*
If the runtime should be defined in the std namespace, open
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
namespace std {
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */


void terminate()
/*
The default terminate routine.
*/
{
  if (__default_terminate_routine != NULL) __default_terminate_routine();
  abort();
}  /* terminate */


a_void_function_ptr set_terminate(a_void_function_ptr new_func)
/*
Set the terminate routine pointer to the value passed by the caller
and return the old value.
*/
{
  a_void_function_ptr	old_func = __default_terminate_routine;
  __default_terminate_routine = new_func;
  return old_func;
}  /* set_terminate */


void unexpected()
/*
The default unexpected routine.  This routine calls terminate.
*/
{
  if (__default_unexpected_routine != NULL) __default_unexpected_routine();
  terminate();
}  /* unexpected */


a_void_function_ptr set_unexpected(a_void_function_ptr new_func)
/*
Set the unexpected routine pointer to the value passed by the caller
and return the old value.
*/
{
  a_void_function_ptr	old_func = __default_unexpected_routine;
  __default_unexpected_routine = new_func;
  return old_func;
}  /* set_unexpected */


_bool uncaught_exception()
/*
Return TRUE if an exception is in the process of being thrown.
*/
{
  an_eh_stack_entry_ptr	ehsep = __curr_eh_stack_entry;
  _bool			result = FALSE;

  for (; ehsep != NULL; ehsep = ehsep->next) {
    if (ehsep->kind == ehsek_throw_processing_marker) {
      result = TRUE;
    }  /* if */
  }  /* for */
  return result;
}  /* uncaught_exception */

/*
If the runtime should be defined in the std namespace, close
the std namespace.
*/
#ifdef __EDG_RUNTIME_USES_NAMESPACES
}  /* namespace std */
#endif /* ifdef __EDG_RUNTIME_USES_NAMESPACES */


EXTERN_C void __call_unexpected(void)
/*
Used by the EH runtime when unexpected needs to be called.  Ensures
that unexpected does not return.
*/
{
  STD_NAMESPACE::unexpected();
  abort();
}  /* __call_unexpected */


EXTERN_C void __call_terminate(void)
/*
Used by the EH runtime when terminate needs to be called.  Ensures
that terminate does not return.
*/
{
  STD_NAMESPACE::terminate();
  abort();
}  /* __call_terminate */


#endif /* EXCEPTION_HANDLING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1993 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
