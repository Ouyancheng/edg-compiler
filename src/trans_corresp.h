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


/* Return TRUE if the indicated entry has a correspondence set (it
   has a correspondence pointer, the pointer is set, and it doesn't
   point to itself). */
#define has_correspondence(ptr)                                        \
  (trans_unit_corresp_of_unknown_entry(ptr) != NULL &&                 \
   canonical_il_entry_of(ptr) != (char*)(ptr))


/*
Routine to record builtin type correspondences.
*/
extern void record_builtin_type(a_type_ptr  type);

#if C99_IL_EXTENSIONS_SUPPORTED
/*
Routines to retrieve certain canonical builtin types.
*/
extern a_type_ptr canonical_bool_type(void);

extern a_type_ptr canonical_complex_type(a_float_kind  kind);

extern a_type_ptr canonical_imaginary_type(a_float_kind  kind);

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

/*
Return TRUE if we need to compare the canonical entries in order to determine
if two pointers refer to the same entity.  This test is never needed in
standalone utility programs.
*/
#if !STANDALONE_UTILITY_PROGRAM
#define canonical_test_needed(ptr1, ptr2)				\
  (secondary_translation_unit_seen() &&					\
    (ptr1) != NULL && (ptr2) != NULL)
#else /* STANDALONE_UTILITY_PROGRAM */
#define canonical_test_needed(ptr1, ptr2) (FALSE)
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
The following canonical_*_entry_of routines return the canonical entry
associated with the given entity.  If it has not yet been looked up, that
canonical entry will be established as part of the call.

The same_*_entities macros determine whether the two given entities are in fact
the same, even though they might have been declared in different translation
units (resulting in distinct IL entries).
*/

extern a_namespace_ptr canonical_namespace_entry_of(a_namespace_ptr  nsp);

#define corresponding_namespaces(ptr1, ptr2)                               \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_namespace_entry_of(ptr1) == canonical_namespace_entry_of(ptr2)))

extern a_field_ptr canonical_field_entry_of(a_field_ptr  field);

#define corresponding_fields(ptr1, ptr2)                                   \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_field_entry_of(ptr1) == canonical_field_entry_of(ptr2)))

extern a_routine_ptr canonical_routine_entry_of(a_routine_ptr  routine);

#define corresponding_routines(ptr1, ptr2)                                 \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_routine_entry_of(ptr1) == canonical_routine_entry_of(ptr2)))

extern a_variable_ptr canonical_variable_entry_of(a_variable_ptr  var);

#define corresponding_variables(ptr1, ptr2)                                \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_variable_entry_of(ptr1) == canonical_variable_entry_of(ptr2)))

extern a_type_ptr canonical_type_entry_of(a_type_ptr type);

#define corresponding_types(ptr1, ptr2)                                    \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_type_entry_of(ptr1) == canonical_type_entry_of(ptr2)))

extern a_template_ptr canonical_template_entry_of(a_template_ptr templ);

#define corresponding_templates(ptr1, ptr2)                                \
  ((ptr1) == (ptr2) ||                                                     \
   (canonical_test_needed(ptr1, ptr2) &&                                   \
    canonical_template_entry_of(ptr1) == canonical_template_entry_of(ptr2)))


/*
Macro that returns the trans_unit_corresp for an IL entry that has a source
correspondence.
*/
#define trans_unit_corresp_of(ptr)					\
  ((ptr)->source_corresp.trans_unit_corresp)

/*
Macro like trans_unit_corresp_of, but that can operate on a char* pointer
or a direct source correspondence pointer.
*/
#define trans_unit_corresp_of_unknown_entry(ptr)			\
  (((a_source_correspondence*)(ptr))->trans_unit_corresp)

/*
Macro that returns the canonical IL entry pointer for an IL entry that
has a source correspondence.  If the entry has no correspondence pointer,
a NULL pointer is returned.
*/
#define canonical_il_entry_of(ptr)				            \
  (trans_unit_corresp_of_unknown_entry(ptr) != NULL		            \
                    ? trans_unit_corresp_of_unknown_entry(ptr)->canonical   \
                    : (char*)ptr)

/*
Compare two translation unit correspondence pointers.  They match if they
are equal and non-NULL.
*/
#define same_trans_unit_corresps(ptr1, ptr2)				\
  ((ptr1) == (ptr2) && (ptr1) != NULL)

/*
Return TRUE if two IL entries (that have source correspondence entries)
refer to the same IL entity.  If the pointers differ, check the
translation unit correspondence pointers.  Unlike the "corresponding_*"
macros above, same_entities assumes that any correspondences between
the objects pointed to by ptr1 and ptr2 have already been set (if in
doubt whether that assumption is valid, it is always same to use one
of the "corresponding_*" macros).
*/
#define same_entities(ptr1, ptr2)					\
  ((ptr1) == (ptr2) ||							\
   ((ptr1) != NULL && (ptr2) != NULL &&					\
    same_trans_unit_corresps(trans_unit_corresp_of(ptr1),		\
                             trans_unit_corresp_of(ptr2))))

/*
This is an interface to the function version of same_entities.  This is
a macro too so that the source correspondence pointer can be used as
the function argument so that any IL entity with a source correspondence
can be used as an argument.  A cast is used instead of &(ptrn)->source_corresp
because the pointers are allowed to be NULL.
*/
#define f_same_entities(ptr1, ptr2)					\
  (ff_same_entities((a_source_correspondence *)(ptr1),			\
                    (a_source_correspondence *)(ptr2)))


extern a_boolean ff_same_entities(a_source_correspondence	*ptr1,
				  a_source_correspondence	*ptr2);
/*
Return TRUE if two base classes refer to the same IL entry.  If the
pointers differ, check the translation unit correspondence pointers.
*/
#define same_base_classes(ptr1, ptr2)					\
  ((ptr1) == (ptr2) ||							\
   same_trans_unit_corresps((ptr1)->trans_unit_corresp,			\
                            (ptr2)->trans_unit_corresp))


extern a_boolean seek_type_corresp(a_type_ptr  type_1,
                                   a_type_ptr  type_2);

extern a_symbol_ptr find_corresponding_symbol_in_trans_unit(
					a_symbol_ptr		sym_to_find,
					a_translation_unit_ptr	tup);

extern a_symbol_ptr find_corresponding_class_instance_in_trans_unit(
				a_symbol_ptr		sym_to_find,
				a_translation_unit_ptr	tup);

extern void set_trans_unit_correspondences(void);

extern void record_instantiation(a_symbol_ptr                      inst,
                                 a_template_symbol_supplement_ptr  tssp);

extern void establish_class_instantiation_corresp(a_type_ptr  type);

extern void establish_function_instantiation_corresp(a_routine_ptr  routine);

extern void establish_variable_instantiation_corresp(a_variable_ptr  var);

extern void establish_block_extern_function_correspondence(
                                                      a_routine_ptr  routine);

extern void establish_block_extern_variable_correspondence(
                                                      a_variable_ptr  var);

extern void establish_friend_type_correspondence(a_type_ptr  type);

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
