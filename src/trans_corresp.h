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

trans_corresp.h -- Declarations related to matching entities across
                   translation units.

*/

/* Avoid including these declarations more than once: */
#ifndef TRANS_CORRESP_H
#define TRANS_CORRESP_H 1


EXTERN a_boolean
		correspondence_checking_underway;
			/* TRUE if the correspondence checking code is
			   currently executing. */
EXTERN a_boolean
		correspondence_checking_done;
			/* TRUE if the correspondence checking code has been
			   completed for the current translation unit. */


extern a_type_ptr canonical_type_entry_of(a_type_ptr type);

extern a_template_ptr canonical_template_entry_of(a_template_ptr templ);

extern void set_trans_unit_correspondences(void);

extern void record_instantiation(a_symbol_ptr                      inst,
                                 a_template_symbol_supplement_ptr  tssp);

extern void corresp_one_time_init(void);

extern void corresp_trans_unit_init(void);

extern void corresp_init(void);

#endif /* ifndef TRANS_CORRESP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
