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

extern char* f_canonical_il_entry_of(char *il_entry);

#define canonical_il_entry_of(ptr)                                     \
  (f_canonical_il_entry_of((char*)(ptr)))


/*
The following canonical_*_entry_of routines return the canonical entry
associated with the given entity.  If it has not yet been looked up, that
canonical entry will be established as part of the call.

The same_*_entities macros determine whether the two given entities are in fact
the same, even though they might have been declared in different translation
units (resulting in distinct IL entries).
*/

extern a_namespace_ptr canonical_namespace_entry_of(a_namespace_ptr  nsp);

#define same_namespace_entities(ptr1, ptr2)                               \
  ((ptr1) == (ptr2) ||                                                    \
   ((ptr1) != NULL && (ptr2) != NULL &&                                   \
    (il_entry_prefix_of(ptr1).secondary_trans_unit ||                     \
     il_entry_prefix_of(ptr2).secondary_trans_unit) &&                    \
    canonical_namespace_entry_of(ptr1) == canonical_namespace_entry_of(ptr2)))

extern a_field_ptr canonical_field_entry_of(a_field_ptr  field);

#define same_field_entities(ptr1, ptr2)                                   \
  ((ptr1) == (ptr2) ||                                                    \
   ((ptr1) != NULL && (ptr2) != NULL &&                                   \
    (il_entry_prefix_of(ptr1).secondary_trans_unit ||                     \
     il_entry_prefix_of(ptr2).secondary_trans_unit) &&                    \
    canonical_field_entry_of(ptr1) == canonical_field_entry_of(ptr2)))

extern a_routine_ptr canonical_routine_entry_of(a_routine_ptr  routine);

#define same_routine_entities(ptr1, ptr2)                                 \
  ((ptr1) == (ptr2) ||                                                    \
   ((ptr1) != NULL && (ptr2) != NULL &&                                   \
    (il_entry_prefix_of(ptr1).secondary_trans_unit ||                     \
     il_entry_prefix_of(ptr2).secondary_trans_unit) &&                    \
    canonical_routine_entry_of(ptr1) == canonical_routine_entry_of(ptr2)))

extern a_variable_ptr canonical_variable_entry_of(a_variable_ptr  var);

#define same_variable_entities(ptr1, ptr2)                                \
  ((ptr1) == (ptr2) ||                                                    \
   ((ptr1) != NULL && (ptr2) != NULL &&                                   \
    (il_entry_prefix_of(ptr1).secondary_trans_unit ||                     \
     il_entry_prefix_of(ptr2).secondary_trans_unit) &&                    \
    canonical_variable_entry_of(ptr1) == canonical_variable_entry_of(ptr2)))

extern a_type_ptr canonical_type_entry_of(a_type_ptr type);

#define same_type_entities(ptr1, ptr2)                                    \
  ((ptr1) == (ptr2) ||                                                    \
   ((ptr1) != NULL && (ptr2) != NULL &&                                   \
    (il_entry_prefix_of(ptr1).secondary_trans_unit ||                     \
     il_entry_prefix_of(ptr2).secondary_trans_unit) &&                    \
    canonical_type_entry_of(ptr1) == canonical_type_entry_of(ptr2)))

extern a_template_ptr canonical_template_entry_of(a_template_ptr templ);

extern a_symbol_ptr find_corresponding_symbol_in_trans_unit(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup);

extern a_symbol_ptr find_corresponding_class_instance_in_trans_unit(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup);

extern void set_trans_unit_correspondences(void);

extern void record_instantiation(a_symbol_ptr                      inst,
                                 a_template_symbol_supplement_ptr  tssp);

extern void establish_trans_unit_correspondences_for_class(a_type_ptr  type);

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
