/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

trans_copy.h -- Declarations related to trans_copy.c (copying of IL
                from secondary translation units to the primary
                translation unit).

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_COPY_H
#define TRANS_COPY_H 1

extern void copy_secondary_trans_unit_IL_to_primary(void);

extern
void mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed(void);

#if ENSURE_LOWERED_TYPE_LIST_ORDERING
extern void fix_type_list_ordering_problems(void);
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */

#endif /* ifndef TRANS_COPY_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
