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
  a_translation_unit_ptr
		next;
			/* Pointer to the next entry on a list of translation
			   units, or NULL for the last entry. */
  a_scope_ptr	primary_scope;
			/* The file scope of the translation unit. */
  a_void_ptr	variables_block;
			/* Pointer to the block of memory used to store
			   variables that are saved and restored when
			   switching between translation units. */
  a_scope_pointers_block
		file_scope_pointers_block;
			/* A block of pointers that are logically part of the
			   scope stack entry for the file scope.  This needs
			   to be a separate structure so that the file scope
			   can be reactivated while preserving the pointers
			   to lists of IL entries, symbols, etc. */
} a_translation_unit;


extern void trans_unit_early_init(void);

extern void process_translation_unit(a_boolean	is_primary);

extern void switch_translation_unit(a_translation_unit_ptr	tup);

extern void trans_unit_one_time_init(void);

extern void trans_unit_init(void);

EXTERN a_translation_unit_ptr
		curr_translation_unit;
			/* Pointer to the translation unit entry for the
			   translation unit that is being processed (and
			   whose per-translation unit data structures are
			   currently active). */

EXTERN a_boolean
		is_primary_translation_unit;
			/* TRUE when processing the primary translation
			   unit.  FALSE when processing secondary translation
			   units. */

EXTERN a_boolean
		translation_unit_needed_only_for_exported_templates;
			/* TRUE when processing a secondary translation unit
			   that is needed only for the exported templates
			   it contains. */

EXTERN a_translation_unit_ptr
		translation_units;
			/* Pointer to a list of translation units.  The first
			   entry on the list is the primary translation
			   unit. */

extern void f_register_trans_unit_variable(a_void_ptr	var,
					   sizeof_t	size);

/*
Macro used to register a variable that is related to a specific translation
unit.  This is used to save and restore the contents of the variable when
switching between translation units.
*/
#define register_trans_unit_variable(var)				\
  (f_register_trans_unit_variable((a_void_ptr)&var, sizeof(var)))


/*
Macro used to register an array that is related to a specific translation
unit.  This is used to save and restore the contents of the array when
switching between translation units.
*/
#define register_trans_unit_array(var)				\
  (f_register_trans_unit_variable((a_void_ptr)var, sizeof(var)))


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
