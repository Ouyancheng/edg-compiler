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
Type declaration for a pointer to a list of fixups to be applied to based-type
lists.  The definition of the fixup structure is private to il.c.
*/
typedef struct a_based_type_fixup *a_based_type_fixup_ptr;


/*
Structure used to record information about a translation unit.

The front end processes more than one translation unit at once when doing
processing for exported templates.

Note that when simply compiling multiple source files, there is not more
than one translation unit being used.  Instead, the front end is reinitialized
and the subsequent files are processed one at a time, each as a primary
translation unit.
*/
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
  a_source_file_ptr
		source_file;
			/* The source file for the primary source file of the
			   translation unit. */
  an_il_header	il_header;
			/* Copy of il_header for this translation unit.
			   Note that only the translation-unit-specific
			   field are maintained.  See
			   save_translation_unit_state to see the list. */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  a_scope_orphaned_list_header_ptr
		last_scope_orphaned_list_header;
			/* End of the il_header.scope_orphaned_list_headers
			   list; NULL if the list is empty. */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  a_macro_ptr	last_macro;
			/* End of the il_header.macros list; NULL if the
			   list is empty. */
#endif /* RECORD_MACROS_IN_IL */
  a_based_type_fixup_ptr
		based_type_fixup_list;
			/* Head of a linked list of entries identifying
			   types whose based-type lists include entries that
			   must not be remove (either because they refer to
			   type entries that are for front-end use only, or
			   because they refer to types from other translation
			   units. */

} a_translation_unit;


/*
Entry used to maintain a stack of translation units.  This is not used
when initially scanning the translation units, but is used when the
translation units are reactivated for the purpose of generating the
instantiations of exported templates.
*/
typedef struct a_translation_unit_stack_entry
                                           *a_translation_unit_stack_entry_ptr;
typedef struct a_translation_unit_stack_entry {
  a_translation_unit_stack_entry_ptr
		next;
			/* Pointer to the previous stack entry (e.g., the
			   entry that should become the current entry when
			   this one is popped off of the stack. */
  a_translation_unit_ptr
		translation_unit;
			/* Pointer to the translation unit that should be the
			   current translation unit when this entry is at the
			   top of the stack.*/
} a_translation_unit_stack_entry;


extern void trans_unit_early_init(void);

extern void process_translation_unit(
				char				*file_name,
				a_boolean			is_primary,
				an_exported_template_file_ptr	exported_file);

extern void switch_translation_unit(a_translation_unit_ptr	tup);

extern void trans_unit_one_time_init(void);

extern void trans_unit_init(void);

EXTERN a_translation_unit_stack_entry_ptr
		curr_translation_unit_stack_entry;
			/* Pointer to the top of the translation unit stack. */

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

EXTERN char	*trans_unit_file_name;
			/* Name of the primary source file for the current
			   translation unit. */

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

extern void push_translation_unit_stack(a_translation_unit_ptr	tup);

extern void pop_translation_unit_stack(void);

extern void f_register_trans_unit_variable(a_void_ptr	var,
					   sizeof_t	size);


/*
Macro that returns whether a secondary translation unit has been seen.
*/
#define secondary_translation_unit_seen()          \
  (translation_units->next != NULL)


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

#if DEBUG
unsigned long db_show_trans_unit_space_used(unsigned long grand_total);
#endif /* DEBUG */

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
