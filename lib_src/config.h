/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Configuration parameters for the runtime.

*/

#ifndef CONFIG_H
#define CONFIG_H 1

/*
Flag used to retain ABI (Application Binary Interface, i.e., runtime layout
and calling sequence) compatibility with older versions.  The value is the
version number of the EDG C++ front end, e.g., 227 for version 2.27, for
which compatibility should be maintained.  ABI changes made after that
version will be suppressed.  Of course, that may suppress certain language
features that cannot be implemented without the corresponding ABI changes.
The default -- a large value -- has the effect of requesting the latest
version of the ABI.

Beginning with version 2.29, the front end defines a preprocessing symbol
called __EDG_ABI_COMPATIBILITY_VERSION that defines the ABI level
begin used by the front end.  This value is used, if it is defined.
*/
#ifndef ABI_COMPATIBILITY_VERSION
#ifdef __EDG_ABI_COMPATIBILITY_VERSION
#define ABI_COMPATIBILITY_VERSION __EDG_ABI_COMPATIBILITY_VERSION
#else /* ifndef __EDG_ABI_COMPATIBILITY_VERSION */
#define ABI_COMPATIBILITY_VERSION 99999 /* Use latest version. */
#endif /* ifdef __EDG_ABI_COMPATIBILITY_VERSION */
#endif /* ifndef ABI_COMPATIBILITY_VERSION */

/*
This flag indicates that the runtime system may be called by cfront-generated
code, and consequently that it must behave as expected by the cfront code.
For example, cfront passes eight NULL pointers to constructors called
from vec_new.  The NULL pointers initialize parameters that point to
virtual base classes.  We only do this in cfront compatibility mode.
*/
#define CFRONT_COMPATIBILITY_MODE TRUE


/*
The runtime uses one of several different mechanisms to invoke static
destructors upon completion of the program.  On Suns, on_exit is used.
For ANSI C environments, atexit is used.  Otherwise a version of
exit is supplied in our runtime library to replace the one normally
found in the C library (e.g., libc.a).  Using our exit may affect
usage that requires an alternate version of the exit function, such
as generating profiling information.

USE_ATEXIT indicates that the atexit function should be used.
*/
#ifndef USE_ATEXIT
#ifndef sun
#define USE_ATEXIT TRUE
#else /* ifdef sun */
#define USE_ATEXIT FALSE
#endif /* ifndef sun */
#endif /* ifndef USE_ATEXIT */

/*
Should the components of the runtime system that implement
exception handling be included.
*/
#ifndef EXCEPTION_HANDLING
#define EXCEPTION_HANDLING TRUE
#endif /* ifndef EXCEPTION_HANDLING */


#if EXCEPTION_HANDLING
/*
The EH runtime allocates a static block of memory to be used for purposes
of tracking pending exceptions, making a copy of the thrown object, etc.
Additional space is allocated if needed.  This parameter specifies the
size of the initial block of memory allocated and the minimum size of
any additional blocks that are required.
*/
#ifndef EH_MEMORY_ALLOCATION_INCREMENT
#define EH_MEMORY_ALLOCATION_INCREMENT 8192
#endif /* ifndef EH_MEMORY_ALLOCATION_INCREMENT */

/*
The strictest alignment required of any data type.
*/
#ifndef MOST_STRICT_ALIGNMENT
#define MOST_STRICT_ALIGNMENT 8
#endif /* ifndef MOST_STRICT_ALIGNMENT */

/*
A type that, when used, will be aligned with the strictest alignment
requirements.
*/
#ifndef TYPE_WITH_MOST_STRICT_ALIGNMENT
#define TYPE_WITH_MOST_STRICT_ALIGNMENT double
#endif /* ifndef TYPE_WITH_MOST_STRICT_ALIGNMENT */


/*
The mangled name of the typeinfo record for a void * type.
*/
#ifndef MANGLED_NAME_OF_PTR_TO_VOID
#define MANGLED_NAME_OF_PTR_TO_VOID __T_v
#endif /* ifndef MANGLED_NAME_OF_PTR_TO_VOID */
#endif /* EXCEPTION_HANDLING */

/*
Should the components of the runtime system that implement run-time
type identification be included.  Note that enabling RTTI alters
the structure of the a_type_info_impl that is shared by RTTI and
exception handling, consequently RTTI cannot be enabled when
preserving ABI compatibility with versions up to 2.28.
*/
#ifndef RTTI
#if ABI_COMPATIBILITY_VERSION <= 228
#define RTTI FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228 */
#define RTTI TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 */
#endif /* ifndef RTTI */
#if RTTI && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- RTTI TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* RTTI && (ABI_COMPATIBILITY_VERSION <= 228) */

/*
Should the EH runtime include the throw interface that uses the "public"
and "ambiguous" information in the type_info structure to determine
accessibility be provided.  This interface is only available in versions
later than 2.28.  The newer interface implemented the revised language
rules that specify that when an object is thrown, it can only be caught
by public, unambiguous base classes.
*/
#ifndef EH_ABI_VERSION_2
#if ABI_COMPATIBILITY_VERSION <= 228
#define EH_ABI_VERSION_2 FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228 */
#define EH_ABI_VERSION_2 TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 */
#endif /* ifndef EH_ABI_VERSION_2 */
#if EH_ABI_VERSION_2 && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- EH_ABI_VERSION_2 TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* EH_ABI_VERSION_2 && (ABI_COMPATIBILITY_VERSION <= 228) */

#endif /* CONFIG_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++  Runtime                           - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1992 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
