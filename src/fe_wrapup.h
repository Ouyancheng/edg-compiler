/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
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

extern a_boolean routine_should_be_externalized_for_exported_templates(
                                                           a_routine_ptr rout);

#endif /* ifndef FE_WRAPUP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

