/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

modules.h -- Declarations related to module handling.

*/

/* Avoid including these declarations more than once: */
#ifndef MODULES_H
#define MODULES_H 1

#include "ifc_modules.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The following structure is associated with every entity in a module file that
is either referenced or whose definition is deferred.  It serves as a central
point to accumulate information about the module entity.  These entries are
kept in a hash table and accessed by a unique key.  Once referenced, the
associated IL entry is also stored.
*/
typedef struct a_module_entity {
  a_module_entity_ptr
		next;	/* A pointer to the next entry on the list.  Used
			   to queue entries on the symbol header (when
			   module entities have the same name). */
  a_module_ptr	module_info;
			/* The module in which this entity is defined. */
  a_scope_ptr	scope;	/* The scope in which this entity is defined. */
  a_tagged_pointer
		entity;	/* The IL entity that corresponds to the module
			   entity. */
  size_t	file_offset;
			/* An offset from the beginning of the module file to
			   the associated module entity.  Used as a key to
			   uniquely identify this entity.  Note that an offset
			   is used rather than a pointer to avoid PCH
			   issues (should the underlying module file be mapped
			   to a different address). */
  union {
#if MICROSOFT_EXTENSIONS_ALLOWED
    an_ifc_partition_kind
		ifc_partition;
			/* Specifies which IFC partition the module entity
			   belongs to. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } variant;
} a_module_entity;

/* If in a module unit, the current module symbol. */
EXTERN a_symbol_ptr    curr_module_sym;

extern a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos);

extern a_boolean find_module_file(a_module_ptr  midp,
                                  a_module_kind *kind);

extern void define_names_from_scope(a_scope_ptr     scope,
                                    a_symbol_header *sym_hdr);

extern void get_definition_of_module_class(a_type_ptr    class_type,
                                           a_text_buffer *buffer);

extern void import_module_file(a_module_import_decl_ptr midp);

extern a_hash_value hash_module_entity(a_void_ptr  key);

extern a_boolean compare_for_module_entity(a_void_ptr  entry,
                                           a_void_ptr  key);

extern a_module_entity_ptr get_module_entity_ptr(a_module_ptr mod,
                                                 size_t        file_offset);

#if DEBUG
extern void db_module(a_module_ptr mod);

extern void db_module_entity(a_module_entity_ptr mep);
#endif /* DEBUG */

extern void modules_pch_reset(void);

#if MAKE_FRONT_END_CALLABLE
extern void modules_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

extern void modules_wrapup(void);

extern void modules_one_time_init(void);

extern void modules_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef MODULES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
