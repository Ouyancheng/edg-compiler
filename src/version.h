/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
version.h -- Front end version number.
*/

#ifndef VERSION_H
#define VERSION_H 1

/*
Definition of the version number of this version.  It is made a separate
file to make updates easy.
*/
#define VERSION_NUMBER "2.27"  /* December 22, 1994. */

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
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
