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

fe_init.h -- Declarations relating to fe_init.c (having to do with
             global initialization of the front end).

*/

/* Avoid including these declarations more than once: */
#ifndef FE_INIT_H
#define FE_INIT_H 1

#if STANDALONE_UTILITY_PROGRAM
extern void standalone_utility_init(void);
#else /* !STANDALONE_UTILITY_PROGRAM */
extern void fe_early_init(void);
extern void fe_one_time_init(void);
extern void fe_init_part_1(void);
extern void fe_init_for_pch_prefix_scan(void);
extern void fe_init_part_2(void);
extern void fe_translation_unit_init(void);
#endif /* STANDALONE_UTILITY_PROGRAM */

extern void initialize_opname_names(void);

#endif /* ifndef FE_INIT_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
