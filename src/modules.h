/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
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
                next;   /* A pointer to the next entry on the list.  Used
                           to queue entries on the symbol header (when
                           module entities have the same name). */
  a_module_ptr  module_info;
                        /* The module in which this entity is defined. */
  a_scope_ptr   scope;  /* The scope in which this entity is defined. */
  a_tagged_pointer
                entity; /* The IL entity that corresponds to the module
                           entity. */
  size_t        file_offset;
                        /* An offset from the beginning of the module file to
                           the associated module entity.  Used as a key to
                           uniquely identify this entity.  Note that an offset
                           is used rather than a pointer to avoid PCH
                           issues (should the underlying module file be mapped
                           to a different address). */
  a_bit_field   imminent:1;
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
  a_bit_field   def_imminent:1;
                        /* This flag is set when the definition of the
                           associated entity has started being loaded. */
  a_bit_field   uses_bound_token:1;
                        /* This flag is set when the IL entity is being
                           declared or defined via a reparse of a broader
                           cache.  This indicates less strict requirements may
                           be placed on recursive definition processing, as the
                           entity could have a severely "broken" token
                           representation or the entity may be invalid but not
                           yet marked as such (as the parse is still
                           ongoing). */
  a_bit_field   invalid:1;
                        /* TRUE if the associated entity cannot be constructed
                           from the module for any reason. */
  a_bit_field   global_module:1;
                        /* TRUE if this is an entity owned by the "global
                           module". */
  a_bit_field   non_exported:1;
                        /* TRUE if this is an entity that's reachable but not
                           visible (and thus was not exported from the imported
                           module). */
  union {
    an_ifc_partition_kind
                ifc_partition;
                        /* Specifies which IFC partition the module entity
                           belongs to. */
  } variant;
};  /* a_module_entity */


/*
An (effectively) abstract class used for interfacing with a module.  Different
module implementations can inherit from this to provide their own interface
without needing to make an IL change.
FIXME: For PCH, f_module needs to be re-opened and mmap_addr recomputed.
*/
struct a_module_interface {
  a_module_file_kind
                mod_kind;
                        /* What kind of module file this interfaces with.  This
                           indicates which class has inherited this module and
                           is used to emulate virtual function dispatch. */
  a_const_char  *primary_name = NULL;
                        /* The primary name of the module. */
  a_const_char  *partition_name = NULL;
                        /* The partition name of the module.  May be NULL if
                           this is not a module partition. */
  a_module_ptr  assoc_module_info = NULL;
                        /* The associated IL module pointer that references
                           this module interface. */
  a_diagnostic_counter
                suppressed_diagnostics = {};
                        /* A diagnostic counter storing the counts of any
                           diagnostics that were suppressed while processing
                           this module. */

  a_module_interface() = delete;
  a_module_interface(a_module_file_kind iface_kind)
    : mod_kind(iface_kind)
    {}
  ~a_module_interface() EDG_NOEXCEPT = default;

  /* State query functions. */
  a_boolean is_open() const;

  /* Disk interface functions. */
  a_boolean import(a_module_import_decl_ptr midp);
  void close();
  void pch_reset(a_module_import_decl_ptr midp);

  /* Module interface functions. */
  void set_name(a_const_char *module_name,
                a_boolean    header_unit);
  void report_suppressed_diagnostics() const;
};  /* a_module_interface */

#if DEBUG

EXTERN unsigned long
                num_module_decls_attempted;
                        /* The number of declarations that process_ifc_decl
                           attempted to process. */

EXTERN unsigned long
                num_module_decls_failed;
                        /* The number of declarations that process_ifc_decl
                           attempted to process, but marked invalid. */

#endif /* DEBUG */

inline a_boolean is_module_entity_globally_visible(a_module_entity_ptr mep)
/*
Return TRUE if the given module entity is globally visible to lookup.  For the
symbol to be globally visible to lookup, it must either be part of the global
module or it must have been exported from a module (that has been imported).
Otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  if (mep == NULL || mep->global_module) {
    /* This declaration belongs to the global module, so it's definitely
       lookup visible. */
    result = TRUE;
  } else if (!mep->non_exported) {
    /* This declaration was exported from an imported module. */
    result = TRUE;
  }  /* if */
  return result;
}  /* is_module_entity_globally_visible */


inline a_module_ptr skip_module_partitions(a_module_ptr mod)
/*
Return the module unit skipping over any module partition unit.
*/
{
  a_module_ptr result = mod;

  while (result != NULL && result->kind == mk_unit_partition) {
    result = result->variant.unit_partition.unit;
  }  /* while */
  return result;
}  /* skip_module_partitions */


inline a_module *lookup_module_for_mep(a_module_entity_ptr mep)
/*
Return a pointer to the module used for lookup of the given module entity
pointer.  Return NULL if there isn't one.
*/
{
  a_module_ptr result;

  if (is_module_entity_globally_visible(mep)) {
    result = NULL;
  } else {
    result = skip_module_partitions(mep->module_info);
  }  /* if */
  return result;
}  /* lookup_module_for_mep */


EXTERN a_symbol_ptr
                curr_module_sym;
                        /* If in a module unit, the current module symbol. */

/*
Entry used to maintain a stack of module contexts.  The top of the stack
is the "current" module for declarations.
*/
typedef struct a_module_entity_stack_entry *a_module_entity_stack_entry_ptr;
struct a_module_entity_stack_entry {
  void invalidate()
    { check_assertion(this->mep != NULL); this->mep->invalid = TRUE; }

  a_module_entity_ptr
                mep;    /* The current module entity pointer.  If mep is NULL,
                           this represents a return to the translation unit
                           module or the global module. */
};  /* a_module_entity_stack_entry */

namespace detail {

/*
The following specializations provide Is_trivially_copyable and
Is_trivially_destructible support for a_module_entity_stack_entry.
*/

template<>
struct Is_trivially_copyable_edg_impl<a_module_entity_stack_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_module_entity_stack_entry> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* detail */

using a_module_entity_stack = Small_dyn_array<a_module_entity_stack_entry, 5>;
                        /* The type used to represent the stack of module
                           contexts. */

EXTERN a_module_entity_stack
                *module_entity_stack;
                        /* A dynamic array of the active module entities. */

using a_module_entity_depth = ptrdiff_t;
                        /* The type used to represent the module entity scope
                           depth. */

inline a_module_entity_depth module_entity_stack_depth()
/*
Return the depth of the module entity stack.
*/
{
  a_module_entity_depth result = NO_SCOPE_DEPTH;

  if (module_entity_stack != NULL && !module_entity_stack->is_empty()) {
    result = module_entity_stack->length() - 1;
  }  /* if */
  return result;
}  /* module_entity_stack_depth */


extern void push_module_entity_state(a_module_entity_ptr mep);

extern void pop_module_entity_state();


/*
A class used to represent an element on the module entity state stack.  This is
an RAII object that automatically manages the calls to
push/pop_module_entity_state for the module entity given during construction.
*/
struct a_module_entity_stack_state {
  a_module_entity_stack_state(a_module_entity_ptr mep_val)
    { push_module_entity_state(mep_val); }
  ~a_module_entity_stack_state()
    { pop_module_entity_state(); }
};  /* a_module_entity_stack_state */


inline a_module_entity_ptr curr_module_entity()
/*
Return a pointer to the module entity at the top of the module entity stack, or
NULL if there is no current module entity.
*/
{
  a_module_entity_ptr mep = NULL;

  if (module_entity_stack != NULL && module_entity_stack->length() > 0) {
    a_module_entity_stack_entry_ptr mesep = &module_entity_stack->back_elem();

    mep = mesep->mep;
  }  /* if */
  return mep;
}  /* curr_module_entity */


inline a_module_ptr curr_lookup_module()
/*
Return the module that should be used for additional lookup (outside of
the global module and any exported entities from imported modules).
*/
{
  a_module_ptr        result = NULL;
  a_module_entity_ptr mep = curr_module_entity();

  if (mep != NULL && mep->module_info->kind != mk_header_unit) {
    result = mep->module_info;
  } else if (trans_unit_module != NULL) {
    result = trans_unit_module;
  }   /* if */
  result = skip_module_partitions(result);
  return result;
}  /* curr_lookup_module */


using a_module_lookup_array = Small_dyn_array<a_module_ptr, 2>;
                        /* An array used to represent the array of modules
                           to be used for lookup. */


inline a_module_lookup_array curr_lookup_modules()
/*
Return all modules that module that can be pulled from during lookup.  A NULL
value represents the global module and any exported entities from imported
modules.
*/
{
  a_module_lookup_array result;
  a_module_ptr          curr_module = curr_lookup_module();

  result.push_back(NULL);
  if (curr_module != NULL) {
    result.push_back(curr_module);
  }  /* if */
  return result;
}  /* curr_lookup_modules */


EXTERN a_boolean
                lazy_symbols_may_be_visible;
                        /* TRUE if symbols (and their definitions) may be
                           "lazily loaded" (i.e., because at least one module
                           has been imported). */

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

extern a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr module_pos);

extern a_module_ptr find_or_create_module(a_symbol_ptr module_sym);

extern a_boolean find_module_file(a_module_ptr  mod,
                                  a_module_kind kind);

extern void import_module_file(a_module_import_decl_ptr midp);

extern void define_names_from_scope(a_scope_ptr     scope,
                                    a_symbol_header *sym_hdr);

extern a_hash_value hash_module_entity(a_void_ptr  key);

extern a_boolean compare_for_module_entity(a_void_ptr  entry,
                                           a_void_ptr  key);

extern a_module_entity_ptr get_module_entity_ptr(a_module_ptr mod,
                                                 size_t       file_offset);

extern void import_header_module(a_module_import_decl_ptr midp);

extern void import_module(a_module_import_decl_ptr midp,
                          a_symbol_ptr             assoc_sym);

extern a_boolean has_variable_initializer_from_module(a_variable_ptr  vp);

extern a_boolean load_variable_initializer_from_module(a_variable_ptr  vp);

extern a_boolean has_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean load_routine_definition_from_module(a_routine_ptr  rp);

extern a_boolean has_template_definition_from_module(a_template_ptr  templ);

extern a_boolean has_pending_template_definition_from_module(
                                                        a_template_ptr  templ);

extern a_boolean load_template_definition_from_module(a_template_ptr  templ);

extern a_boolean has_pending_template_specializations_from_module(
                                                        a_template_ptr  templ);

extern a_boolean load_template_specializations_from_module(
                                                        a_template_ptr  templ);

extern a_boolean has_type_definition_from_module(a_type_ptr  ty);

extern a_boolean load_type_definition_from_module(a_type_ptr  ty);

/*
An internal token cache wrapper structure that represents additional state for
modules.
*/
struct a_module_token_cache {
  a_module_token_cache(a_source_position_ptr initial_hint = NULL)
    : underlying_cache(), valid(TRUE), position_hint(initial_hint)
    { clear_token_cache(&(this->underlying_cache), /*reusable=*/TRUE); }
  a_module_token_cache(const a_module_token_cache&) = delete;
  inline ~a_module_token_cache();

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

  a_source_position_ptr get_position_hint() const
    { return this->position_hint; }
  void set_position_hint(a_source_position_ptr new_position_hint)
    { this->position_hint = new_position_hint; }
  void suggest_source_position(a_source_position_ptr new_position_hint);
private:
  a_token_cache
                underlying_cache;
                        /* The underlying cache to insert tokens into. */
  a_boolean     valid;  /* TRUE if the underlying cache should be parsed after
                           caching; otherwise, FALSE. */
  a_source_position_ptr
                position_hint;
                        /* Current source position hint (used for source
                           position inference). */
};  /* a_module_token_cache */


inline void a_module_token_cache::suggest_source_position(
                                       a_source_position_ptr new_position_hint)
/*
Set the token position hint (only) if the new token position follows the most
recent token in the cache.
*/
{
  check_assertion(new_position_hint != NULL);
  if (this->get_last_token() == NULL) {
    this->position_hint = new_position_hint;
  } else {
    a_source_position_ptr last_pos = &this->get_last_token()->source_position;

    if (last_pos->seq < new_position_hint->seq ||
        (last_pos->seq == new_position_hint->seq &&
         last_pos->column < new_position_hint->column)) {
      this->position_hint = new_position_hint;
    }  /* if */
  }  /* if */
}  /* a_module_token_cache::suggest_source_position */


a_module_token_cache::~a_module_token_cache()
/*
Cleanup after the token cache.
*/
{
#if DEBUG
  if (!this->valid && db_flag_is_set("invalid_token_cache")) {
    fprintf(f_debug, "Discarded invalid module cache:\n");
    db_tokens(&(this->underlying_cache));
  }  /* if */
#endif /* DEBUG */
}  /* a_module_token_cache::~a_module_token_cache() */


using a_module_token_cache_ptr = a_module_token_cache*;

inline a_source_position_ptr
infer_next_source_position(a_module_token_cache_ptr cache,
                           a_source_position_ptr    pos = NULL)
/*
Perform source position inference on the given cache.  The inferred position
should only be requested when gathering the source position for a new token.

Position inference resolves the source position to one of the following:

  1. The provided source position (if not NULL).
  2. The current position hint (a_module_token_cache_ptr::get_position_hint).
  3. The previous token's source position.
  4. The null source position.

The current source position hint is cleared after inference as the new token's
source position (i.e., the result of the most recent call to this function for
this cache) should take priority.
*/
{
  if (pos != NULL) {
    goto done;
  }  /* if */
  pos = cache->get_position_hint();
  if (pos != NULL) {
    goto done;
  }  /* if */

  {
    a_cached_token_ptr last_tok = cache->get_last_token();

    if (last_tok != NULL) {
      pos = &last_tok->source_position;
    } else {
      pos = &null_source_position;
    }  /* if */
  }  /* if */
done:
  cache->set_position_hint(NULL);
  return pos;
}  /* infer_next_source_position */


inline a_token_sequence_number enter_module_token_rescan(
                                                a_module_token_cache_ptr cache)
/*
Begin a token rescan of the given module token cache.  The module token cache
will be terminated by this function and should not be pre-terminated.  The
token sequence number for the added terminator token (end of source) will be
returned.  The caller is responsible for calling exit_module_token_rescan after
the rescanned tokens have been used.

If possible, prefer use of the RAII class a_module_entity_rescan which handles
calls to exit_module_token_rescan automatically upon destruction.
*/
{
#if CHECKING
  {
    a_cached_token_ptr last_tok = cache->get_last_token();

    check_assertion_str(last_tok != NULL, "the cache cannot be empty");
    check_assertion_str(last_tok->token != tok_end_of_source,
                        "the cache cannot be pre-terminated.");
  }
#endif /* CHECKING */
  terminate_token_cache(cache->as_canonical());

  a_cached_token_ptr last_tok = cache->get_last_token();
  push_stop_token_stack();
  rescan_reusable_cache(cache->as_canonical());
  increment_dependent_scans_for_reusable_cache();
  return last_tok->token_sequence_number;
}  /* enter_module_token_rescan */


inline void exit_module_token_rescan(
                             a_const_char            *orig_start_of_curr_token,
                             a_const_char            *orig_end_of_curr_token,
                  ARG_UNUSED a_token_sequence_number expected_end_tsn,
                  ARG_UNUSED a_token_kind            final_token = tok_error)
/*
Restore the token stream state after processing a module token cache.

The given start_of_curr_token value (orig_start_of_curr_token) and
end_of_curr_token value (orig_end_of_curr_token) are the original start and end
of the current token prior to the start of the rescan.

The given expected_end_tsn argument is the expected token sequence number for
the associated terminator (end of source) token.  If upon clearing any
remaining tokens the encountered end of source token's sequence number doesn't
match, the front end will expect an error in CHECKING modes.

The given final token argument is the expected current token at the time this
function is called.  If the final token doesn't match the current token, the
front end will expect an error in CHECKING modes.  If there is no specific
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
  decrement_dependent_scans_for_reusable_cache();
  pop_stop_token_stack();
  check_assertion(curr_token == tok_end_of_source);
  check_assertion(curr_token_sequence_number == expected_end_tsn ||
                  curr_token_sequence_number == NO_TOKEN_SEQUENCE_NUMBER);
  (void)get_token();
  start_of_curr_token = orig_start_of_curr_token;
  end_of_curr_token = orig_end_of_curr_token;
}  /* exit_module_token_rescan */


/*
A structure for representing an automatically cleaned up module token rescan
operation.  This class should be used to reenter a module token cache for
parsing.
*/
struct a_module_entity_rescan {
  inline a_module_entity_rescan(
                         a_module_token_cache_ptr cache,
                         a_token_kind             *final_token_ptr_val = NULL);
  inline ~a_module_entity_rescan();
private:
  a_boolean     valid;  /* TRUE if the rescan successfully cached one or more
                           tokens; otherwise, FALSE. */
  a_token_kind  *final_token_ptr;
                        /* A pointer to the variable storing the final token
                           value token to be used on deconstruction, or NULL if
                           tok_error should be passed to
                           exit_module_token_rescan. */
#if CHECKING
  a_const_char*
                expected_curr_source_line;
                        /* The value of curr_source_line when the rescan was
                           started and thus, the expected value of
                           curr_source_line when the rescan ends. */
#endif /* CHECKING */
  a_const_char*
                save_start_of_curr_token;
                        /* The value of start_of_curr_token when the rescan was
                           started that will be restored to the current token
                           when the rescan ends. */
  a_const_char*
                save_end_of_curr_token;
                        /* The value of end_of_curr_token when the rescan was
                           started that will be restored to the current token
                           when the rescan ends. */
#if CHECKING
  a_token_sequence_number
                expected_end_tsn;
                        /* The expected ending token sequence number. */
#endif /* CHECKING */
};  /* a_module_entity_rescan */


a_module_entity_rescan::a_module_entity_rescan(
                                 a_module_token_cache_ptr cache,
                                 a_token_kind             *final_token_ptr_val)
/*
Apply the appropriate initialization logic to set up the parser for parsing the
tokens specified in the given cache.  final_token_ptr_val should be a pointer
to the expected token kind upon a correct parse, or NULL if no specific token
is expected.
*/
  : valid(cache->is_valid()), final_token_ptr(final_token_ptr_val),
#if CHECKING
    expected_curr_source_line(curr_source_line),
#endif /* CHECKING */
    save_start_of_curr_token(start_of_curr_token),
    save_end_of_curr_token(end_of_curr_token)
{
  if (this->valid) {
#if CHECKING
    this->expected_end_tsn =
#endif /* CHECKING */
      /* Do not put code here. */
      enter_module_token_rescan(cache);
  } else {
#if CHECKING
    this->expected_end_tsn = NO_TOKEN_SEQUENCE_NUMBER;
#endif /* CHECKING */
  }  /* if */
}  /* a_module_entity_rescan */


a_module_entity_rescan::~a_module_entity_rescan()
/*
Apply the appropriate token cleanup logic to restore the parser state prior to
the module entity rescan.
*/
{
  if (this->valid) {
    a_token_kind            final_token = tok_error;
    a_token_sequence_number end_tsn = NO_TOKEN_SEQUENCE_NUMBER;

    if (this->final_token_ptr != NULL) {
      final_token = *(this->final_token_ptr);
    }  /* if */
#if CHECKING
    end_tsn = this->expected_end_tsn;
#endif /* CHECKING */
#if CHECKING
    /* If this assertion fails, something has altered the current source line
       and needs to be modified to ensure it restores the original
       curr_source_line value. */
    check_assertion(this->expected_curr_source_line == curr_source_line);
#endif /* CHECKING */
    exit_module_token_rescan(this->save_start_of_curr_token,
                             this->save_end_of_curr_token,
                             end_tsn, final_token);
  }
}  /* a_module_entity_rescan::~a_module_entity_rescan */

#if DEBUG

extern void db_tokens(a_module_token_cache_ptr cache);

extern a_string s_db_module(a_module_ptr mod);

extern void db_module(a_module_ptr mod);

extern a_string s_basic_db_mep(a_module_entity_ptr mep);

extern void db_mep(a_module_entity_ptr mep);

extern void push_module_context(a_module_ptr mod_ptr);

extern void pop_module_context(void);

extern void db_module_entity(a_module_entity_ptr mep);

extern void db_module_stack();

extern void db_mep_stack();

#endif /* DEBUG */

extern void modules_pch_reset();

extern void modules_check_for_suppressed_errors();

extern void modules_one_time_init();

extern void modules_trans_unit_init();

extern void modules_trans_unit_wrapup_part_1();

extern void modules_trans_unit_wrapup_part_2();

#if MAKE_FRONT_END_CALLABLE
extern void modules_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern void modules_write_out();

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef MODULES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
