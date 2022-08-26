/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2021 Edison Design Group Inc.                   [_]          *
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

/* FIXME: Also defined in ifc_modules.h */
enum an_ifc_partition_kind : uint32_t;

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
  a_bit_field	imminent:1;
			/* This flag is typically set to TRUE when deferred
			   processing starts; i.e., when the creation of an
			   actual associated IL entity is imminent.  This is
			   used to avoid unbounded recursion.  The flag is
			   also set to TRUE for IFC partial specializations
			   that are deferred,  That causes the main entity
			   loading mechanism to skip partial specializations;
			   instead, those entries are processed explicitly
			   immediately after the associated primary template
			   has been processed. */
  a_bit_field	invalid:1;
			/* TRUE if the associated entity cannot be constructed
			   from the module for any reason. */
  a_bit_field	global_module:1;
			/* TRUE if this is an entity owned by the "global
			   module". */
  a_bit_field	has_definition:1;
			/* TRUE if the presence of a definition ("body") has
			   been recorded. */
  union {
#if MICROSOFT_EXTENSIONS_ALLOWED
    an_ifc_partition_kind
		ifc_partition;
			/* Specifies which IFC partition the module entity
			   belongs to. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } variant;
};  /* a_module_entity */

/*
An (effectively) abstract class used for interfacing with a module.  Different
module implementations can inherit from this to provide their own interface
without needing to make an IL change.
FIXME: For PCH, f_module needs to be re-opened and mmap_addr recomputed.
*/
struct a_module_interface {
#if !USE_VIRTUAL_FUNCTIONS
  a_module_kind	mod_kind = (a_module_kind)mk_none;
			/* What kind of module this interface is for.  This
			   indicates which class has inherited this module and
			   is used to emulate virtual function dispatch. */
#endif /* !USE_VIRTUAL_FUNCTIONS */
  a_const_char	*primary_name = NULL;
			/* The primary name of the module. */
  a_const_char	*partition_name = NULL;
			/* The partition name of the module.  May be NULL if
			   this is not a module partition. */
  a_module_ptr	assoc_module_info = NULL;
			/* The associated IL module pointer that references
			   this module interface. */
  FILE		*f_module = NULL;
			/* The file descriptor for the file. */
  size_t	f_size = 0;
			/* The size of the module file. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  void		*mmap_addr = NULL;
			/* A pointer to the memory-mapped beginning of the
			   module file. */
  size_t	mmap_size = 0;
			/* The size of the memory-mapped partition. */
#if EDG_WIN32
  a_windows_handle
		mapped_input = NULL;
			/* A HANDLE returned by CreateFile_interface during
			   the mapping process on Windows. */
  a_windows_handle
		map_object = NULL;
			/* A HANDLE returned by CreateFileMapping during the
			   mapping process on Windows. */
#endif /* EDG_WIN32 */
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
  a_diagnostic_counter
		suppressed_diagnostics = {};
			/* A diagnostic counter storing the counts of any
			   diagnostics that were suppressed while processing
			   this module. */

  a_module_interface() = delete;
  a_module_interface(a_module_kind iface_kind)
#if !USE_VIRTUAL_FUNCTIONS
    : mod_kind(iface_kind)
#endif /* !USE_VIRTUAL_FUNCTIONS */
    {}
  VIRTUAL ~a_module_interface() EDG_NOEXCEPT = default;

/* State query functions. */
  VIRTUAL a_boolean is_open() const ABSTRACT;

/* Disk interface functions. */
  VIRTUAL a_boolean import(a_module_import_decl_ptr midp) ABSTRACT;
  VIRTUAL void close() ABSTRACT;
  VIRTUAL void pch_reset(a_module_import_decl_ptr midp) ABSTRACT;

/* Module interface functions. */
  void set_name(a_const_char *module_name,
                a_boolean    header_unit);
  VIRTUAL void complete_definition_of_module_class(a_module_entity_ptr mep)
                                                                      ABSTRACT;
  void report_suppressed_diagnostics() const;
#if DEBUG
  VIRTUAL void debug() const ABSTRACT;
  VIRTUAL void db_module_entity(a_module_entity_ptr mep) const ABSTRACT;
#endif /* DEBUG */
};  /* a_module_interface */

/* If in a module unit, the current module symbol. */
EXTERN a_symbol_ptr    curr_module_sym;

/*
When processing a module entity, this points to a description of the entity in
the ("binary") module file until the IL entry associated with the entity is
created and made to point to the description.  (Be sure to save/clear/restore
this variable in contexts that may trigger the creation of new IL entries
before the actual module entity is represented.)
*/
EXTERN a_module_entity_ptr curr_module_entity;

inline a_boolean magic_numbers_match(const a_byte magic[4],
                                     const a_byte expected[4])
/*
Return true if the provided magic numbers match their expected magic numbers.
*/
{
  return magic[0] == expected[0] &&
         magic[1] == expected[1] &&
         magic[2] == expected[2] &&
         magic[3] == expected[3];
}  /* magic_numbers_match */


inline a_boolean is_header_unit(a_module_ptr mod)
/*
Return TRUE if the provided module is a header unit, FALSE otherwise.
*/
{
  return (mod != NULL &&
          (mod->kind == mk_header || mod->resolved_header != NULL));
}  /* is_header_unit */

extern a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos);

extern a_boolean find_module_file(a_module_ptr  mod,
                                  a_module_kind kind);

extern void import_module_file(a_module_import_decl_ptr midp);

extern void define_names_from_scope(a_scope_ptr     scope,
                                    a_symbol_header *sym_hdr);

extern void complete_definition_of_module_class(a_type_ptr class_type);

extern a_hash_value hash_module_entity(a_void_ptr  key);

extern a_boolean compare_for_module_entity(a_void_ptr  entry,
                                           a_void_ptr  key);

extern a_module_entity_ptr get_module_entity_ptr(a_module_ptr mod,
                                                 size_t       file_offset);


/*
EDG implementation of modules.
*/
/*lint -save -e1511 -e1762 -e1790*/
struct an_edg_module : public a_module_interface {
  an_edg_module() : a_module_interface((a_module_kind)mk_edg) {}
  ~an_edg_module() EDG_NOEXCEPT = default;

  a_boolean matches_module(ARG_UNUSED a_const_char *module_name,
                           ARG_UNUSED a_const_char *module_file)
    { unexpected_condition_str("Unimplemented"); /*lint -e527*/ return FALSE; }

  a_boolean is_open() const OVERRIDE
    { unexpected_condition_str("Unimplemented"); /*lint -e527*/ return FALSE; }

  a_boolean import(ARG_UNUSED a_module_import_decl_ptr midp) OVERRIDE
    { unexpected_condition_str("Unimplemented"); /*lint -e527*/ return FALSE; }
  NORETURN void close() OVERRIDE
    { unexpected_condition_str("Unimplemented"); }
  NORETURN void pch_reset(ARG_UNUSED a_module_import_decl_ptr midp) OVERRIDE
    { unexpected_condition_str("Unimplemented"); }

  NORETURN void complete_definition_of_module_class(
                                            ARG_UNUSED a_module_entity_ptr mep)
                                                                       OVERRIDE
    { unexpected_condition_str("Unimplemented"); }

#if DEBUG
  NORETURN void debug() const OVERRIDE
    { unexpected_condition_str("Unimplemented"); }
  NORETURN void db_module_entity(ARG_UNUSED a_module_entity_ptr mep)
                                                                 const OVERRIDE
    { unexpected_condition_str("Unimplemented"); }
#endif /* DEBUG */
};  /* an_edg_module */
/*lint -restore*/

extern void import_header_module(a_module_import_decl_ptr midp);

extern void import_module(a_module_import_decl_ptr midp,
                          a_symbol_ptr             assoc_sym);

extern a_boolean has_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean load_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean load_template_definition_from_module(a_template_ptr  templ);

extern a_dynamic_init_ptr load_variable_init_from_module(
                                         a_type_ptr                    tp,
                                         a_lexical_ifc_index_reference *index);

extern a_boolean extract_tokens_for_module_expr(
                                         a_lexical_ifc_index_reference *index);


/*
An internal token cache wrapper structure that represents additional state for
modules.
*/
struct a_module_token_cache {
  a_module_token_cache()
    : underlying_cache(), valid(TRUE)
    { clear_token_cache(&(this->underlying_cache), /*reusable=*/FALSE); }
  a_module_token_cache(const a_module_token_cache&) = delete;

  a_boolean is_valid() const
    { return valid; }
  void invalidate()
    { this->valid = FALSE; }

  a_token_cache_ptr as_canonical()
    { return &(this->underlying_cache); }
  const a_token_cache* as_canonical() const
    { return &(this->underlying_cache); }

  a_cached_token_ptr get_first_token()
    { return this->underlying_cache.first_token; }
  a_cached_token_ptr get_last_token()
    { return this->underlying_cache.last_token; }
private:
  a_token_cache
                underlying_cache;
                        /* The underlying cache to insert tokens into. */
  a_boolean     valid;  /* TRUE if the underlying cache should be parsed after
                           caching; otherwise, FALSE. */
};  /* a_module_token_cache */

using a_module_token_cache_ptr = a_module_token_cache*;


inline void enter_module_token_rescan(a_module_token_cache_ptr cache)
/*
Begin a token rescan of the given module token cache ptr.  The caller is
responsible for calling exit_module_token_rescan after the rescanned tokens
have been used.

If possible, prefer use of the RAII class a_module_entity_rescan which handles
calls to exit_module_token_rescan automatically upon destruction.
*/
{
  push_stop_token_stack();
  terminate_token_cache(cache->as_canonical());
  rescan_cached_tokens(cache->as_canonical());
}  /* enter_module_token_rescan */


inline void exit_module_token_rescan(
                               ARG_UNUSED a_token_kind final_token = tok_error)
/*
Restore the token stream state after processing a module token cache.  The
given final token argument is the expected current token at the time this
function is called.  If the given final token doesn't match the current token,
the front end will expect an error in CHECKING modes.  If there is no specific
expected token, tok_error can be used to safely skip this check.
*/
{
#if CHECKING
  if (final_token != tok_error && curr_token != final_token) {
    expect_error();
  }  /* if */
#endif /* CHECKING */
  clear_stop_tokens();
  flush_to_end_of_source(/*suppress_warning=*/TRUE);
  pop_stop_token_stack();
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
}  /* exit_module_token_rescan */


/*
A structure for representing an automatically cleaned up module token rescan
operation.  This class should be used to reenter a module token cache for
parsing.
*/
struct a_module_entity_rescan {
  a_module_entity_rescan(a_module_token_cache_ptr cache,
                         a_token_kind             *final_token_ptr_val = NULL)
    : valid(cache->is_valid()), final_token_ptr(final_token_ptr_val)
    { if (this->valid) enter_module_token_rescan(cache); }
  inline ~a_module_entity_rescan();
private:
  a_boolean     valid;  /* TRUE if the rescan successfully cached one or more
                           tokens; otherwise, FALSE. */
  a_token_kind  *final_token_ptr;
                        /* A pointer to the variable storing the final token
                           value token to be used on deconstruction, or NULL if
                           tok_error should be passed to
                           exit_module_token_rescan. */

};  /* a_module_entity_rescan */


a_module_entity_rescan::~a_module_entity_rescan()
/*
Apply the appropriate token cleanup logic to restore the parser state prior to
the module entity rescan.
*/
{
  if (this->valid) {
    a_token_kind final_token = tok_error;

    if (this->final_token_ptr != NULL) {
      final_token = *(this->final_token_ptr);
    }  /* if */
    exit_module_token_rescan(final_token);
  }
}  /* ~a_module_entity_rescan */


#if DEBUG
extern void db_module(a_module_ptr mod);

extern void db_module_entity(a_module_entity_ptr mep);
#endif /* DEBUG */

extern void modules_pch_reset(void);

extern void modules_check_for_suppressed_errors(void);

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
* Copyright 2020-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
