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
#endif /* ifndef sun */
#endif /* ifndef USE_ATEXIT */


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
