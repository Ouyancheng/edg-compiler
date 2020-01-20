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

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Control whether the module interface is abstract.  If this macro is TRUE, the
module interface instead provides an implementation of all methods that will
abort if they're called.
*/
#ifndef ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED
/* FIXME: Change this to default to FALSE so that we can know that our
   implementations always implement all the interface functions. */
#define ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED TRUE
#endif /* ifndef ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED */

/* FIXME: Also defined in ifc_modules.h */
typedef uint32_t an_ifc_partition_kind;

/*
The following structure is associated with every entity in a module file that
is either referenced or whose definition is deferred.  It serves as a central
point to accumulate information about the module entity.  These entries are
kept in a hash table and accessed by a unique key.  Once referenced, the
associated IL entry is also stored.
*/
struct a_module_entity {
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
};  /* a_module_entity */

#if ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED
#define ABSTRACT                                                              \
  { unexpected_condition_str("Unimplemented interface function"); }
#else /* !ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED */
#define ABSTRACT = 0;
#endif /* ABSTRACT_MODULE_INTERFACE_IS_UNIMPLEMENTED */

/*
An (effectively) abstract class used for interfacing with a module.  Different
module implementations can inherit from this to provide their own interface
without needing to make an IL change.
*/
struct a_module_interface {
  a_module_interface() noexcept = default;
  virtual ~a_module_interface() noexcept;

/* State query functions. */
  virtual a_boolean is_open() const noexcept ABSTRACT

/* Disk interface functions. */
  virtual a_boolean import(a_module_import_decl_ptr midp) noexcept ABSTRACT;
  virtual a_boolean close() noexcept ABSTRACT

/* Module interface functions. */
  void set_name(a_const_char *module_name) noexcept;
/* FIXME: Implement these. */

/* Data members. */
  a_const_char *primary_name = NULL;
  a_const_char *partition_name = NULL;
};  /* a_module_interface */

/* If in a module unit, the current module symbol. */
EXTERN a_symbol_ptr    curr_module_sym;

extern a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos);

extern a_boolean find_module_file(a_module_ptr  midp,
                                  a_module_kind kind);

extern void import_module_file(a_module_import_decl_ptr midp);

extern void define_names_from_scope(a_scope_ptr     scope,
                                    a_symbol_header *sym_hdr);

extern void get_definition_of_module_class(a_type_ptr    class_type,
                                           a_text_buffer *buffer);

extern a_hash_value hash_module_entity(a_void_ptr  key);

extern a_boolean compare_for_module_entity(a_void_ptr  entry,
                                           a_void_ptr  key);

extern a_module_entity_ptr get_module_entity_ptr(a_module_ptr mod,
                                                 size_t        file_offset);


/*
EDG implementation of modules.
*/
struct an_edg_module : public a_module_interface {
/* FIXME: Implement. */
};  /* an_edg_module */

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
