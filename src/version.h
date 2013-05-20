/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
version.h -- Front end version number.
*/

/* Avoid including these declarations more than once. */
#ifndef VERSION_H
#define VERSION_H 1

/*
Definition of the version number of this version.  It is made a separate
file to make updates easy.
*/
#define VERSION_NUMBER "4.7"  /* May 20, 2013. */

/*
Version number used to set a predefined macro that expands to the
front end version.  This must be a numeric value.
*/
#define VERSION_NUMBER_FOR_MACRO 407

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

EXTERN char	*build_date
#if VAR_INITIALIZERS
			    = __DATE__
#endif /* VAR_INITIALIZERS */
                                      ;

EXTERN char	*build_time
#if VAR_INITIALIZERS
			    = __TIME__
#endif /* VAR_INITIALIZERS */
                                      ;


#endif /* ifndef VERSION_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
