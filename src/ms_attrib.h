/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

ms_attrib.h -- Declarations related to ms_attrib.c (Microsoft attribute
               processing).

*/

/* Avoid including these declarations more than once: */
#ifndef MS_ATTRIB_H
#define MS_ATTRIB_H 1

#if MICROSOFT_EXTENSIONS_ALLOWED

extern an_ms_attribute_ptr scan_microsoft_attributes(void);

extern void ms_attrib_one_time_init(void);

extern void ms_attrib_init(void);

#if DEBUG
unsigned long db_show_ms_attrib_space_used(unsigned long grand_total);
#endif /* DEBUG */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef MS_ATTRIB_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
