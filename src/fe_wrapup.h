/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

fe_wrapup.h - declarations related to end of front end processing.

*/

/* Avoid including these declarations more than once: */
#ifndef FE_WRAPUP_H
#define FE_WRAPUP_H 1

extern void translation_unit_wrapup(void);

extern void fe_wrapup(void);

extern void fe_wrapup_part_2(void);

#if MAKE_FRONT_END_CALLABLE
extern void fe_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

#endif /* ifndef FE_WRAPUP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

