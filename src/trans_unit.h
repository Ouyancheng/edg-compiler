/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

trans_unit.h -- Declarations related to translation unit management.

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_UNIT_H
#define TRANS_UNIT_H 1


/*
Structure used to record information about a translation unit.

The front end processes more than one translation unit at once when doing
processing for exported templates.

Note that when simply compiling multiple source files, there is not more
than one translation unit being used.  Instead, the front end is reinitialized
and the subsequent files are processed one at a time, each as a primary
translation unit.
*/
typedef struct a_translation_unit *a_translation_unit_ptr;
typedef struct a_translation_unit {
  a_void_ptr	variables_block;
			/* Pointer to the block of memory used to store
			   variables that are saved and restored when
			   switching between translation units. */
} a_translation_unit;


extern void trans_unit_early_init(void);

#endif /* ifndef TRANS_UNIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
