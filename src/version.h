/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
version.h -- Front end version number.
*/

/* Avoid including these declarations more than once. */
#ifndef VERSION_H
#define VERSION_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Definition of the version number of this version.  It is made a separate
file to make updates easy.
*/
#define VERSION_NUMBER "6.7"  /* January 27, 2025. */

/*
Version number used to set a predefined macro that expands to the
front end version.  This must be a numeric value.
*/
#define VERSION_NUMBER_FOR_MACRO 607

/*
The date and time that this version was built.  These variables will
be defined when fe_init.c is compiled.
*/
#ifndef __DATE__
#define __DATE__ "[date unknown]"
#endif /* ifndef __DATE__ */

#ifndef __TIME__
#define __TIME__ "[time unknown]"
#endif /* ifndef __TIME__ */

#if EDG_WIN32
#pragma warning( push )
#pragma warning(disable : 5048 )
#endif /* EDG_WIN32 */

EXTERN a_const_char
		*build_date
#if VAR_INITIALIZERS
			    = __DATE__
#endif /* VAR_INITIALIZERS */
                                      ;

EXTERN a_const_char
		*build_time
#if VAR_INITIALIZERS
			    = __TIME__
#endif /* VAR_INITIALIZERS */
                                      ;

#if EDG_WIN32
#pragma warning( pop )
#endif /* EDG_WIN32 */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef VERSION_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
