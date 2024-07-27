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

modules.c -- Classes and routines handling modules.

*/

/* Header files common to all files. */
#include "fe_common.h"
#include "ifc_modules.h"
#include "util.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

namespace {

struct a_module_file_suffix {
  a_const_char  *suffix;
                        /* The suffix associated with the module file. */
  a_module_file_kind
                kind;
                        /* The kind of module this suffix implies. */
};  /* a_module_file_suffix */

constexpr a_module_file_suffix module_file_suffixes[] = {
  { "eifc", mfk_edg_ifc },
  { "ifc", mfk_ms_ifc }
};

a_text_buffer_ptr module_search_buffer, module_file_name_buffer;
a_text_buffer_ptr module_primary_name_buffer, module_partition_name_buffer;

}  /* namespace */

static a_boolean same_module_name(a_symbol_ptr module_name,
                                  a_symbol_ptr other_module_name)
/*
Determine whether two modules have the same name.
*/
{
  a_boolean result = FALSE;

  if (module_name == other_module_name) {
    result = TRUE;
  } else if (module_name == NULL || other_module_name == NULL) {
    /* result = FALSE */
  } else if (module_name->header->identifier_length !=
             other_module_name->header->identifier_length) {
    /* result = FALSE */
  } else if (strncmp(module_name->header->identifier,
                     other_module_name->header->identifier,
                     module_name->header->identifier_length) != 0) {
    /* result = FALSE */
  } else {
    /* This portion of the name was the same - check the rest. */
    result = same_module_name(module_name->next, other_module_name->next);
  }
  return result;
}  /* same_module_name */


a_boolean check_module_has_interface_dependency(
                                           a_symbol_ptr          module_sym,
                                           a_symbol_ptr          interface_sym,
                                           a_source_position_ptr pos)
/*
Determine whether the given module has an interface dependency on the module
referred to by interface_sym.  pos is the position to use for diagnostics (if
any).  Return TRUE if there exists an interface dependency, FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  /* The easy case of a module importing itself.  The only exception is an
     implementation unit which imports its own primary interface unit. */
  if (interface_sym != NULL &&
      (module_sym->variant.module_info.is_interface_unit ||
       (!module_partition_implicitly_imports_self &&
        module_sym->variant.module_info.partition_name != NULL)) &&
      same_module_name(module_sym->variant.module_info.primary_name,
                       interface_sym->variant.module_info.primary_name) &&
      same_module_name(module_sym->variant.module_info.partition_name,
                       interface_sym->variant.module_info.partition_name)) {
    pos_error(ec_module_cannot_depend_on_itself, pos);
    result = TRUE;
  }
  /* FIXME: Recurse over interface_sym's interface dependencies. */
  return result;
}  /* check_module_has_interface_dependency */


using a_module_name_map = Ptr_map<a_string_view, a_module_ptr>;

static a_module_name_map
                *known_modules;
                        /* A map of names to the known module. */


static a_module_ptr find_or_create_module(a_string_view module_sym_id,
                                          a_module_kind module_kind)
/*
Find or create an IL module entity corresponding to the given module symbol
identifier and module kind.

Note: module_sym_id must be backed by long term storage; this function does not
create its own copy of the module symbol identifier.
*/
{
  a_module_ptr  result = known_modules->get(module_sym_id);

  if (result == NULL) {
    result = alloc_module(module_kind);
    result->name = module_sym_id.start;
    known_modules->map(module_sym_id, result);
  }  /* if */
  return result;
}  /* find_or_create_module */


a_module_ptr find_or_create_module(a_symbol_ptr module_sym)
/*
Find or create an IL module entity corresponding to the given module symbol
identifier and module kind.
*/
{
  check_assertion(module_sym->kind == sk_module);
  a_string_view module_sym_id(module_sym->header->identifier,
                              module_sym->header->identifier_length);
  a_symbol_ptr  partition_name =
                                module_sym->variant.module_info.partition_name;
  a_module_kind module_kind = partition_name == NULL ? mk_unit
                                                     : mk_unit_partition;
  a_module_ptr  result = find_or_create_module(module_sym_id, module_kind);

  if (result->kind == mk_unit_partition &&
      result->variant.unit_partition.unit == NULL) {
    a_symbol_ptr unit_sym = module_sym->variant.module_info.primary_name;

    check_assertion_or_expect_error(unit_sym != NULL);
    if (unit_sym != NULL) {
      a_string_view unit_sym_id(unit_sym->header->identifier,
                                unit_sym->header->identifier_length);

      result->variant.unit_partition.unit =
                                   find_or_create_module(unit_sym_id, mk_unit);
    }  /* if */
  }  /* if */
  return result;
}  /* find_or_create_module */


static a_const_char *get_module_primary_name(a_const_char *name)
/*
Get just the module primary name from the given module's name (i.e., remove the
partition if it's present) and return it.  If no name is present, return an
empty string.  Note that the name will be stored in reuseable memory and
should be copied if it's wanted to be kept long-term.
*/
{
  sizeof_t     name_len = strlen(name);

  reset_text_buffer(module_primary_name_buffer);
  for (sizeof_t idx = 0; idx < name_len; ++idx) {
    if (name[idx] == ':') {
      name_len = idx;
      break;
    }  /* if */
  }  /* for */
  add_to_text_buffer(module_primary_name_buffer, name, name_len);
  add_char_to_text_buffer(module_primary_name_buffer, '\0');
  return module_primary_name_buffer->buffer;
}  /* get_module_primary_name */


static a_const_char *get_module_partition_name(a_const_char *name)
/*
Get just the module partition name from the given module's name (if present)
and return it.  If no name is present, return an empty string.  Note that the
name will be stored in reuseable memory and should be copied if it's wanted to
be kept long-term.
*/
{
  sizeof_t     name_len = 0;

  reset_text_buffer(module_partition_name_buffer);
  for (; *name != '\0'; ++name) {
    if (*name == ':') {
      ++name; /*lint !e850*/
      name_len = strlen(name);
      break;
    }  /* if */
  }  /* for */
  add_to_text_buffer(module_partition_name_buffer, name, name_len);
  add_char_to_text_buffer(module_partition_name_buffer, '\0');
  return module_partition_name_buffer->buffer;
}  /* get_module_partition_name */


static a_const_char *get_module_file_base_name(a_const_char *name)
/*
Get the base name of the module file for the given module name.  A module name
takes the form <primary>[:<partition>], and the resulting base name is
<primary>[-<partition>].  Note that the name will be stored in reuseable memory
and should be copied if it's wanted to be kept long-term.
*/
{
  reset_text_buffer(module_file_name_buffer);
  (void)get_module_primary_name(name);
  (void)get_module_partition_name(name);
  add_to_text_buffer(module_file_name_buffer,
                     module_primary_name_buffer->buffer,
                     module_primary_name_buffer->size);
  if (module_partition_name_buffer->size > 1) {
    remove_null_terminator_from_text_buffer(module_file_name_buffer);
    add_char_to_text_buffer(module_file_name_buffer, '-');
    add_to_text_buffer(module_file_name_buffer,
                       module_partition_name_buffer->buffer,
                       module_partition_name_buffer->size);
  }  /* if */
  return module_file_name_buffer->buffer;
}  /* get_module_file_base_name */


static a_module_file_kind determine_module_file_kind(FILE *file)
/*
Given an already open file pointer, determine what kind of module file is open
(if any).  Return the kind of module file.
*/
{
  a_module_file_kind result = mfk_unknown;
  a_byte             magic[4];

  /* Ensure we're at the start of the file. */
  if (fseek(file, 0, SEEK_SET) != 0) {
    catastrophe(ec_module_read_error);
  }  /* if */
  if (fread(magic, (size_t)1, sizeof(magic), file) == sizeof(magic)) {
    if (magic_numbers_match(magic, edg_ifc_magic_numbers)) {
      result = mfk_edg_ifc;
    } else if (magic_numbers_match(magic, ms_ifc_magic_numbers)) {
      result = mfk_ms_ifc;
    }  /* if */
  }  /* if */
  return result;
}  /* determine_module_file_kind */


static a_boolean is_module_kind_available(a_module_file_kind kind)
/*
Given a supported module file kind, return TRUE if it's currently usable;
otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;

  switch (kind) {
    case mfk_edg_ifc:
      /* Always enabled. */
      result = TRUE;
      break;
    case mfk_unknown:
      /* Always disabled. */
      break;
    case mfk_ms_ifc:
#if MICROSOFT_EXTENSIONS_ALLOWED
      result = microsoft_mode;
#endif /* #if MICROSOFT_EXTENSIONS_ALLOWED */
      break;
    default_is_unexpected();
  }  /* if */
  return result;
}  /* is_module_kind_available */


static a_boolean check_module_file(a_const_char       *module_file,
                                   a_module_file_kind *file_kind)
/*
Return TRUE if the provided module file exists and is the given kind, FALSE
otherwise.  Set *file_kind to the module file kind discovered.

This function may not return and instead issue a catastrophic error if the
module file exists but cannot be opened.
*/
{
  a_boolean           result = FALSE;
  FILE*               file;
  an_open_file_result open_result;

  file = fopen_with_result(module_file, FOPEN_MODE_FOR_BINARY_READ,
                           &open_result);
  *file_kind = mfk_unknown;
  if (file == NULL) {
    /* Open failed.  Most reasons are likely valid, but check for specific
       problem cases. */
    if (open_result.flags & OFR_CANNOT_OPEN) {
      /* Note that file_open_error does not return when called from here. */
      file_open_error(es_error, ec_module_file, module_file,
                      &open_result);
    }  /* if */

    a_diagnostic_ptr dp = pos_start_catastrophe(ec_file_for_module_not_found,
                                                &error_position,
                                                module_file);
    /* Note any module mappings pointing to this file. */
    for (const a_module_file_map::an_entry &entry : *mod_map) {
      if (!entry.has_value()) {
        continue;
      }  /* if */
      if (strcmp(entry.value(), module_file) == 0) {
        /* A mapping for this file exists, add it to the diagnostic. */
        a_string_view module_name = entry.key();
        add_diag_info(dp, ec_found_from_module_map, module_name);
      }  /* if */
    }  /* for */
    /* FIXME: Should lazy_mod_map_arr elements be reported here? */
    /* Note any header unit mappings pointing to this file. */
    for (const a_header_unit_map::an_entry &entry : *header_unit_map) {
      if (!entry.has_value()) {
        continue;
      }  /* if */
      if (strcmp(entry.value(), module_file) == 0) {
        /* A mapping for this file exists, add it to the diagnostic. */
        a_const_char *header_path = entry.key().ptr;
        str_add_diag_info(dp, ec_found_from_header_unit_map, header_path);
      }  /* if */
    }  /* for */
    /* Emit the diagnostic. */
    end_diagnostic(dp);
    goto done;
  }  /* if */
  { /* We've found a file - determine what kind it is. */
    *file_kind = determine_module_file_kind(file);

    if (!is_module_kind_available(*file_kind)) {
      str_catastrophe(ec_ms_ifc_unavailable, module_file);
    }  /* if */
  }
  result = TRUE;
done:
  if (file != NULL) {
    (void)fclose(file);
  }  /* if */
  return result;
}  /* check_module_file */


static a_module_file_kind get_module_kind(a_const_char *module_file)
/*
Given a module file, return the module's kind.
*/
{
  a_module_file_kind  result = mfk_unknown;
  FILE*               file;
  an_open_file_result open_result;

  file = fopen_with_result(module_file, FOPEN_MODE_FOR_BINARY_READ,
                           &open_result);
  if (file == NULL) {
    goto done;
  }  /* if */
  /* We've found a file - determine what kind it is. */
  result = determine_module_file_kind(file);
done:
  if (file != NULL) {
    (void)fclose(file);
  }  /* if */
  return result;
}  /* get_module_kind */


static Opt<a_string> get_name_of_module(a_const_char *module_file)
/*
Given a module file, return the module's name.  If the name could not be
determined, return an empty optional.
*/
{
  Opt<a_string>      result;
  a_module_file_kind kind = get_module_kind(module_file);

  switch (kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      result = get_name_of_ifc_module(module_file);
      break;
    case mfk_unknown:
      /* The name cannot be resolved, return an empty optional. */
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* get_name_of_module */


static a_boolean module_file_matches(a_const_char *module_name,
                                     a_const_char *module_file)
/*
Return TRUE if module_file is a module file for module_name (i.e., the name of
the module in the module file matches module_name), FALSE otherwise.
*/
{
  a_boolean     result = FALSE;
  Opt<a_string> opt_mod_name = get_name_of_module(module_file);

  if (opt_mod_name.has_value()) {
    a_string mod_name = *opt_mod_name;

    if (mod_name == module_name) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* module_file_matches */


static void resolve_lazy_mod_map_element()
/*
Resolve one pending lazy_mod_map_arr element.  This function converts the last
element of the lazy_mod_map_arr into a mod_map element.  If a mod_map element
already exists, it takes priority and this element is silently ignored.
*/
{
  check_assertion(!lazy_mod_map_arr->is_empty());
  a_const_char   *mod_path = lazy_mod_map_arr->back_elem();
  Opt<a_string>  opt_mod_name = get_name_of_module(mod_path);

  if (opt_mod_name.has_value()) {
    a_string_view tmp_mod_handle(opt_mod_name->as_temp_characters(),
                                 opt_mod_name->length());

    /* Using a temporary module handle, allocate a "permanent" module name
       string. */
    if (mod_map->get(tmp_mod_handle) == NULL) {
      General_allocator<char>
                    allocator;
      a_const_char  *mod_name_chars =
                                 opt_mod_name->to_allocated_storage(allocator);
      a_string_view mod_handle(mod_name_chars, opt_mod_name->length());

      mod_map->map(mod_handle, mod_path);
    } else {
      pos_catastrophe(ec_multiple_module_matches, &error_position,
                      tmp_mod_handle);
    }  /* if */
  }  /* if */
  lazy_mod_map_arr->pop_back();
}  /* resolve_lazy_mod_map_element */


static a_boolean find_module_file_in_map(a_module_ptr  mod)
/*
Find the module file associated with mod in the module map and update mod with
the path to the file.  Return TRUE if a module file was found, FALSE otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean    found = FALSE;
  a_const_char *module_path;

  module_path = mod_map->get(mod->name);
  /* FIXME: This emulates our previous behavior.  If we fail to find an element
     in the map, we process the entire lazy mod map array (with diagnostics for
     duplicates). */
  if (module_path == NULL && !lazy_mod_map_arr->is_empty()) {
    while (!lazy_mod_map_arr->is_empty()) {
      resolve_lazy_mod_map_element();
    }  /* while */
    module_path = mod_map->get(mod->name);
  }  /* if */
  if (module_path != NULL) {
    if (check_module_file(module_path, &mod->file_kind)) {
      mod->resolved_file = copy_string_to_region(file_scope_region_number,
                                                 module_path);
      found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* find_module_file_in_map */


static a_boolean find_header_unit_in_map(a_module_ptr mod)
/*
Find the module file associated with mod in the header unit map and update mod
with the path to the file.  Return TRUE if a module file was found, FALSE
otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean     found = FALSE;
  a_const_char  *module_path;

  /* The module must be a header unit module to use this search function. */
  check_assertion(mod->kind == mk_header_unit);
  module_path = resolve_header_in_map(mod->name,
                                      mod->variant.header_unit.resolved_header,
                                      mod->variant.header_unit.is_sys_include);
  if (module_path != NULL) {
    if (check_module_file(module_path, &mod->file_kind)) {
      mod->resolved_file = copy_string_to_region(file_scope_region_number,
                                                 module_path);
      found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* find_header_unit_in_map */


static a_boolean find_module_file_in_dirs(a_module_ptr mod)
/*
Find the module file associated with mod in the module search paths and update
mod with the path to the file.  Return TRUE if a module file was found, FALSE
otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean                  found = FALSE;
  a_directory_name_entry_ptr dir = module_search_path;
  a_const_char               *module_name;

  module_name = get_module_file_base_name(mod->name);
  for (; !found && dir != NULL; dir = dir->next) {
    /* combine_dir_and_file_name clears the buffer for us. */
    (void)combine_dir_and_file_name(dir->dir_name, module_name,
                                    module_search_buffer);
    /* Add an arbitrary extension to prevent replace_file_name_suffix from
       replacing part of the actual module name. */
    remove_null_terminator_from_text_buffer(module_search_buffer);
    add_to_text_buffer(module_search_buffer, ".ext", 5);
    for (const auto& suffix : module_file_suffixes) {
      replace_file_name_suffix(suffix.suffix, module_search_buffer);
      if (!file_exists(module_search_buffer->buffer)) {
        continue;
      }  /* if */
      if (check_module_file(module_search_buffer->buffer, &mod->file_kind)) {
        found = TRUE;
        mod->resolved_file = copy_string_to_region(
                                                 file_scope_region_number,
                                                 module_search_buffer->buffer);
        break;
      }  /* if */
    }  /* for */
  }  /* for */
  return found;
}  /* find_module_file_in_dirs */


a_boolean find_module_file(a_module_ptr mod)
/*
Find the module file associated with mod and update mod with the path to the
file.  Return TRUE if a module file was found, FALSE otherwise.
*/
{
  a_boolean found = FALSE;

  if (mod->resolved_file != NULL && mod->file_kind != mfk_unknown) {
    /* Module file has already been found. */
    found = TRUE;
  }  /* if */
  if (found || skip_module_imports) {
    goto done;
  }  /* if */
  if (mod->kind == mk_header_unit) {
    Value_saver<a_boolean> windows_path_saver(&windows_paths_allowed, TRUE);
    Value_saver<a_boolean> backslash_saver(&backslash_is_also_dir_separator,
                                           TRUE);
    a_const_char           *header_path;
    a_const_char           *name_for_search = mod->name;

    if (ignore_absolute_paths_for_header_units &&
        is_absolute_file_name(name_for_search)) {
      name_for_search = get_base_name(name_for_search);
    }  /* if */
    header_path = resolve_header(name_for_search,
                                 mod->variant.header_unit.is_sys_include,
                                 /*is_include_next=*/FALSE,
                                 /*suppress_diagnostics=*/FALSE);
    if (header_path == NULL) {
      pos_st_catastrophe(ec_cannot_find_header_for_import, &error_position,
                         mod->name);
    } else {
      mod->variant.header_unit.resolved_header = header_path;
      found = find_header_unit_in_map(mod);
    }  /* if */
  } else {
    found = find_module_file_in_map(mod);
    if (!found) {
      found = find_module_file_in_dirs(mod);
    }  /* if */
    if (!found) {
      pos_st_catastrophe(ec_module_file_not_found, &error_position, mod->name);
    } else if (!module_file_matches(mod->name, mod->resolved_file)) {
      pos_st_catastrophe(ec_module_file_mismatch, &error_position, mod->name);
    }  /* if */
  }  /* if */
done:
  return found;
}  /* find_module_file */


void import_module_file(a_module_import_decl_ptr midp)
/*
Import the module file specified in the module-import-declaration.
*/
{
  a_module_interface_ptr iface = NULL;

  check_assertion(midp->module_info->resolved_file != NULL);
  switch (midp->module_info->file_kind) {
    case mfk_unknown:
      /* The module file kind should be set if the resolved path is set. */
      unexpected_condition();
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      iface = new_fe<an_ifc_module>(midp->module_info->file_kind);
      break;
    default:
      unexpected_condition_str("Unexpected module kind for import.");
  }  /* switch */
  midp->module_info->module_interface = iface;
  (void)iface->import(midp);
}  /* import_module_file */


void define_names_from_scope(a_scope_ptr     scope,
                             a_symbol_header *sym_hdr)
/*
Called during name lookup when the name associated with sym_hdr has been
referred to in the indicated scope.  Scan through the list of deferred name
entries for this symbol header and process declarations for any that match the
scope.  The caller is responsible for ensuring there are deferred module
entities associated with the given symbol header.  Similarly, the caller is
responsible for making sure this function is not called after lexical
processing has ended.
*/
{
  a_module_entity_ptr mep, *mepp = &(sym_hdr->deferred_module_entities);

  /* This function should only be called if lexical processing is still
     enabled. */
  check_assertion(curr_lexical_state_stack_entry != NULL);
  /* This function should only be called if there are deferred module entities
     available. */
  check_assertion(sym_hdr->deferred_module_entities != NULL);
  while (*mepp != NULL) {
    if ((*mepp)->scope == scope) {
#if DEBUG
      if (db_flag_is_set("ms_symbols")) {
        (void)fprintf(f_debug, "Loading symbol %s in ",
                      sym_hdr->identifier);
        db_scope(scope);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* Remove the entry from the queue (so it's not recursively processed)
         but don't free it until it's been processed. */
      mep = *mepp;
      if (mep->imminent) {
        /* If the entry is marked as being processed, skip it.  (Usually,
           such entries have already been removed from the list, but partial
           specializations are marked early and processed later.) */
        mepp = &(*mepp)->next;
        continue;
      }  /* if */
      *mepp = mep->next;
      switch (mep->module_info->file_kind) {
        case mfk_edg_ifc:
        case mfk_ms_ifc:
          process_ifc_declaration(mep);
          break;
        default:
          unexpected_condition();
      }  /* switch */
    } else {
      mepp = &(*mepp)->next;
    }  /* if */
  }  /* for */
}  /* define_names_from_scope */


static a_hash_table_ptr
       module_entity_hash_table;
                        /* A hash table to find module entities. */


a_hash_value hash_module_entity(a_void_ptr  key)
/*
Produce a hash value for the given pointer (a_module_entity_ptr).
*/
{
  a_module_entity_ptr mep = (a_module_entity_ptr)key;

  check_assertion(mep->module_info != NULL);
  return (a_hash_value)mep->file_offset;
}  /* hash_module_entity */


a_boolean compare_for_module_entity(a_void_ptr  entry,
                                    a_void_ptr  key)
/*
Compare the file offset associated with entry (a_module_entity_ptr) to the
given key (a_module_entity_ptr).  Return TRUE if they represent the same module
entity.
*/
{
  return (((a_module_entity_ptr)entry)->module_info ==
          ((a_module_entity_ptr)key)->module_info) &&
         (((a_module_entity_ptr)entry)->file_offset ==
          ((a_module_entity_ptr)key)->file_offset);
}  /* compare_for_module_entity */


a_module_entity_ptr get_module_entity_ptr(a_module_ptr mod,
                                          size_t       file_offset)
/*
Return a pointer to the module entity pointer for the entity at file_offset in
the specified module.
*/
{
  a_module_entity  key, **p;

  if (module_entity_hash_table == NULL) {
      /* FIXME: what is a good hash table size here? */
      module_entity_hash_table = alloc_hash_table(FRONT_END_REGION_NUMBER,
                                  (a_hash_table_size)1000,
                                  fn_for_function(hash_module_entity),
                                  fn_for_function(compare_for_module_entity));
  }  /* if */
  /* A module interface must be present, otherwise this module entity pointer
     is not viable. */
  check_assertion(mod->module_interface != NULL);
  key.module_info = mod;
  key.file_offset = file_offset;
  p = (a_module_entity_ptr*)hash_find(module_entity_hash_table, &key,
                                      /*create=*/TRUE);
  check_assertion(p != NULL);
  if (*p == NULL) {
    /* Create a new module entity.  These are allocated in front end memory
       (so they are saved in PCH files) and never freed. */
    *p = (a_module_entity*)alloc_fe(sizeof(a_module_entity));
    (*p)->next = NULL;
    (*p)->module_info = mod;
    (*p)->scope = NULL;
    (*p)->entity.ptr = NULL;
    (*p)->entity.kind = iek_none;
    (*p)->file_offset = file_offset;
    (*p)->imminent = FALSE;
    (*p)->def_imminent = FALSE;
    (*p)->uses_bound_token = FALSE;
    (*p)->invalid = FALSE;
    (*p)->global_module = FALSE;
    (*p)->non_exported = FALSE;
    (*p)->variant.ifc_partition = ifc_pk_none;
  }  /* if */
  return *p;
}  /* get_module_entity_ptr */

/*lint -esym(1714,*a_module_interface::is_open)*/ /* FIXME: temporary*/
a_boolean a_module_interface::is_open() const
/*
Dispatch the is_open() call to the variant for the actual object.
*/
{
  a_boolean result = FALSE;

  switch (this->mod_kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      result = ((an_ifc_module*)this)->is_open();
      break;
    case mfk_unknown:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* is_open */


/*lint -esym(1762,*a_module_interface::import)*/ /* FIXME: temporary*/
a_boolean a_module_interface::import(a_module_import_decl_ptr midp)
/*
Dispatch the import() call to the variant for the actual object.
*/
{
  a_boolean result = FALSE;

  switch (this->mod_kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      result = ((an_ifc_module*)this)->import(midp);
      break;
    case mfk_unknown:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* import */


/*lint -esym(1762,*a_module_interface::close)*/ /* FIXME: temporary*/
void a_module_interface::close()
/*
Dispatch the close() call to the variant for the actual object.
*/
{
  switch (this->mod_kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      ((an_ifc_module*)this)->close();
      break;
    case mfk_unknown:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
}  /* close */


/*lint -esym(1762,*a_module_interface::pch_reset)*/ /* FIXME: temporary*/
void a_module_interface::pch_reset(a_module_import_decl_ptr midp)
/*
Dispatch the pch_reset() call to the variant for the actual object.
*/
{
  switch (this->mod_kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      ((an_ifc_module*)this)->pch_reset(midp);
      break;
    case mfk_unknown:
      unexpected_condition();
    default_is_unexpected();
  }  /* switch */
}  /* pch_reset */


void a_module_interface::set_name(a_const_char *module_name,
                                  a_boolean    header_unit)
/*
Set the name of this module to the provided module_name.  If header_unit is
TRUE, module_name is the path to the header file (not the header unit BMI, if
it exists).
*/
{
  if (header_unit) {
    /* This convention is purely arbitrary but matches how MSVC encodes the
       primary/partition names for header units. */
    this->primary_name = NULL;
    this->partition_name = copy_string_to_region(file_scope_region_number,
                                                 module_name);
  } else {
    a_const_char *tmp_prim_name = get_module_primary_name(module_name);

    this->primary_name = copy_string_to_region(file_scope_region_number,
                                               tmp_prim_name);

    a_const_char *tmp_part_name = get_module_partition_name(module_name);
    if (tmp_part_name[0] != '\0') {
      this->partition_name = copy_string_to_region(file_scope_region_number,
                                                   tmp_part_name);
    } else {
      this->partition_name = NULL;
    }  /* if */
  }  /* if */
}  /* set_name */


void a_module_interface::report_suppressed_diagnostics() const
/*
Called to report suppressed errors, catastrophic errors, and warnings during
processing of the imported module entities for this module.
*/
{
  unsigned long errors = suppressed_diagnostics.errors;
  unsigned long warnings = suppressed_diagnostics.warnings;

  if (errors > 0) {
    a_boolean plural = errors > 1;

    st_num_diagnostic(es_error,
                      (plural ? ec_suppressed_module_errors_diag
                              : ec_suppressed_module_error_diag),
                      assoc_module_info->name,
                      errors);
  }  /* if */
  if (warnings > 0) {
    a_boolean plural = warnings > 1;

    st_num_diagnostic(es_warning,
                      (plural ? ec_suppressed_module_warnings_diag
                              : ec_suppressed_module_warning_diag),
                      assoc_module_info->name,
                      warnings);
  }  /* if */
}  /* report_suppressed_diagnostics */


void push_module_entity_state(a_module_entity_ptr mep)
/*
Push a new module entity to the module entity stack.  If mep is NULL, this
represents a return to the translation unit module or the global module.
*/
{
  /* To avoid issues where the module entity stack is unavailable in the back
     end, do nothing outside of the front end. */
  if (in_front_end) {
    a_module_entity_stack_entry mese{};

    mese.mep = mep;
    module_entity_stack->push_back(mese);
  }  /* if */
}  /* push_module_entity_state */


void pop_module_entity_state()
/*
Pop the current module entity from the module entity stack.
*/
{
  /* To avoid issues where the module entity stack is unavailable in the back
     end, do nothing outside of the front end. */
  if (in_front_end) {
    module_entity_stack->pop_back();
  }  /* if */
}  /* pop_module_entity_state */


static a_boolean check_module_already_imported(a_module_import_decl_ptr midp)
/*
Check if the module import declaration imports a module that has already been
imported.  If so, return TRUE and update *midp to refer to the original import
declaration.  Otherwise, return FALSE and leave *midp unmodified.
*/
{
  a_boolean    already_imported = FALSE;
  a_module_ptr mod = midp->module_info;

  for (a_module_import_decl_ptr ptr = il_header.imported_modules;
       ptr != NULL; ptr = ptr->next) {
    a_boolean    same_name, same_file;

    /* Check both the name of the module and the resolved module file (a header
       unit could possibly reference the same module file from multiple
       paths). */
    same_name = (strcmp(ptr->module_info->name, mod->name) == 0);
    same_file = (mod->resolved_file != NULL &&
                 (strcmp(ptr->module_info->resolved_file,
                         mod->resolved_file) == 0));
    if (same_name || same_file) {
      pos_st_remark(ec_module_already_imported, &midp->module_name_position,
                    mod->name);
      *midp = *ptr;
      already_imported = TRUE;
      break;
    }  /* if */
  }  /* for */
  return already_imported;
}  /* check_module_already_imported */


void import_header_module(a_module_import_decl_ptr midp)
/*
Import the given header module.
*/
{
  /* See if there's a known module file for the imported header and import
     that file if so. */
  if (find_module_file(midp->module_info)) {
    if (!check_module_already_imported(midp)) {
      import_module_file(midp);
      /* Add this to the list of imported modules regardless of whether the
         import was successful - future attempts to import the same module
         aren't likely to succeed if this one failed. */
      midp->next = il_header.imported_modules;
      il_header.imported_modules = midp;
    }  /* if */
  } else if (skip_module_imports) {
    /* For testing purposes, this import was skipped.  Issue a warning. */
    pos_st_warning(ec_import_skipped, &midp->module_name_position,
                   midp->module_info->name);
  } else {
    /* The set of importable headers is implementation-defined.  Currently no
       headers are importable. */
    pos_st_catastrophe(ec_header_not_importable, &midp->module_name_position,
                       midp->module_info->name);
  }  /* if */
}  /* import_header_module */


void import_module(a_module_import_decl_ptr midp,
                   a_symbol_ptr             assoc_sym)
/*
Import the given module.  assoc_sym is the associated symbol for the module.
*/
{
  a_boolean already_imported = FALSE;

  /* See if this module has already been imported.  If so, ignore it. */
  already_imported = check_module_already_imported(midp);
  if (!already_imported &&
      !check_module_has_interface_dependency(assoc_sym, curr_module_sym,
                                             &midp->module_name_position)) {
    if (find_module_file(midp->module_info)) {
      import_module_file(midp);
    }  /* if */
    /* Add this to the list of imported modules regardless of whether the
       import was successful - future attempts to import the same module
       aren't likely to succeed if this one failed. */
    midp->next = il_header.imported_modules;
    il_header.imported_modules = midp;
  }  /* if */
}  /* import_module */


a_boolean has_variable_initializer_from_module(a_variable_ptr vp)
/*
If the given variable has an initializer available in an imported module,
return TRUE.  Otherwise, return FALSE.
*/
{
  return (lazy_symbols_may_be_visible &&
          has_variable_initializer_from_ifc_module(vp));
}  /* has_variable_initializer_from_module */


a_boolean load_variable_initializer_from_module(a_variable_ptr vp)
/*
The given variable has an initializer available in an imported module (i.e.,
has_variable_initializer_from_module(vp) has returned TRUE); load and process
that initializer now and return TRUE. If an error occurs, return FALSE.
*/
{
  return load_variable_initializer_from_ifc_module(vp);
}  /* load_variable_initializer_from_module */


a_boolean has_routine_definition_from_module(a_routine_ptr rp)
/*
If the given routine has a definition available in an imported module, return
TRUE.  Otherwise, return FALSE.
*/
{
  return (lazy_symbols_may_be_visible &&
          has_routine_definition_from_ifc_module(rp));
}  /* has_routine_definition_from_module */


a_boolean load_routine_definition_from_module(a_routine_ptr rp)
/*
The given routine has a definition available in an imported module (i.e.,
has_routine_definition_from_module(rp) has returned TRUE); load and process
that definition now and return TRUE. If an error occurs, return FALSE.
*/
{
  return load_routine_definition_from_ifc_module(rp);
}  /* load_routine_definition_from_module */


a_boolean has_template_definition_from_module(a_template_ptr templ)
/*
If the given template has a definition available in an imported module, return
TRUE.  Otherwise, return FALSE.
*/
{
  return (lazy_symbols_may_be_visible &&
          has_template_definition_from_ifc_module(templ));
}  /* has_template_definition_from_module */


a_boolean has_pending_template_definition_from_module(a_template_ptr templ)
/*
If the given template is not already defined and has a definition available in
an imported module, return TRUE.  Otherwise, return FALSE.  Note that templ
must refer to the canonical template.
*/
{
  check_assertion(templ != NULL);
  return (!symbol_for(templ)->defined &&
          has_template_definition_from_module(templ));
}  /* has_pending_template_definition_from_module */


a_boolean load_template_definition_from_module(a_template_ptr templ)
/*
The given template has a definition available in an imported module (i.e.,
has_template_definition_from_module(templ) has returned TRUE); load and process
that definition now and return TRUE. If an error occurs, return FALSE.
*/
{
  return load_template_definition_from_ifc_module(templ);
}  /* load_template_definition_from_module */


a_boolean has_pending_template_specializations_from_module(
                                                          a_template_ptr templ)
/*
If the given template has specializations available in an imported module,
return TRUE.  Otherwise, return FALSE.  Note that templ must refer to the
canonical template.
*/
{
  check_assertion(templ != NULL);
  return (lazy_symbols_may_be_visible &&
          has_template_specializations_from_ifc_module(templ));
}  /* has_pending_template_specializations_from_module */


a_boolean load_template_specializations_from_module(a_template_ptr templ)
/*
The given template has one or more specializations available in an imported
module (i.e., has_pending_template_specializations_from_module(templ) has
returned TRUE); load and process those specializations now and return TRUE. If
an error occurs, return FALSE.
*/
{
  return load_template_specializations_from_ifc_module(templ);
}  /* load_template_specializations_from_module */


a_boolean has_type_definition_from_module(a_type_ptr ty)
/*
If the given type has a definition available in an imported module, return
TRUE.  Otherwise, return FALSE.
*/
{
  return (lazy_symbols_may_be_visible &&
          has_type_definition_from_ifc_module(ty));
}  /* has_type_definition_from_module */


a_boolean load_type_definition_from_module(a_type_ptr ty)
/*
The given type has a definition available in an imported module (i.e.,
has_type_definition_from_module(ty) has returned TRUE); load and process that
definition now and return TRUE. If an error occurs, return FALSE.
*/
{
  a_boolean  result = FALSE;

  if (!ty->definition_pending) {
    ty->definition_pending = TRUE;
    result = load_type_definition_from_ifc_module(ty);
    /* Once the class is defined, there is no need for this information. */
    ty->definition_pending = FALSE;
  }  /* if */
  return result;
}  /* load_type_definition_from_module */

#if DEBUG

void db_mep_stack()
/*
Print information about the module entity stack.
*/
{
  int  size = module_entity_stack->length();
  for (int i = size-1; i >= 0; i--) {
    a_module_entity_stack_entry &mese = (*module_entity_stack)[i];

    db_mep(mese.mep);
  }  /* for */
}  /* db_mep_stack */


void db_tokens(a_module_token_cache_ptr cache)
/*
This function proxies calls to the common db_tokens function when using
a module token cache pointer.
*/
{
  db_tokens(cache->as_canonical());
}  /* db_tokens */

a_string s_db_module(a_module_ptr mod)
/*
Return a string containing debug information about the given module.
*/
{
  a_string      result = "";
  a_module_kind m_kind = mk_none;
  if (mod != NULL) {
    m_kind = mod->kind;
  }  /* if */
  result.append("module name: ");
  if (mod != NULL && mod->name != NULL) {
    result.append(mod->name);
  } else {
    result.append("<NULL>");
  }  /* if */
  result.append(", kind: ");
  switch (m_kind) {
    case mk_header_unit:
      result.append("Header Unit");
      break;
    case mk_unit:
      result.append("Module Unit");
      break;
    case mk_unit_partition:
      result.append("Module Unit (Partition)");
      break;
    default:
      result.append("UNKNOWN");
      break;
  }  /* switch */
  result.append(", file: ");
  if (mod != NULL && mod->resolved_file != NULL) {
    result.append(mod->resolved_file);
    result.append(", file kind: ");
    switch (mod->file_kind) {
      case mfk_edg_ifc:
        result.append("IFC (EDG)");
        break;
      case mfk_ms_ifc:
        result.append("IFC (Microsoft)");
        break;
      default:
        result.append("UNKNOWN");
        break;
    }  /* switch */
  } else {
    result.append("<NULL>");
  }  /* if */
  return result;
}  /* s_db_module */


void db_module(a_module_ptr mod)
/*
Display debug information about the specified module.
*/
{
  print(s_db_module(mod), f_debug);
}  /* db_module */


a_string s_basic_db_mep(a_module_entity_ptr mep)
/*
Return a string containing debug information about the given module entity.
This string does not contain extensive information about the module.
*/
{
  a_string               result = s_db_module(mep->module_info);
  a_module_interface_ptr m_iface = NULL;
  a_module_file_kind     m_kind = mfk_unknown;

  if (mep->module_info != NULL) {
    m_iface = mep->module_info->module_interface;
    if (m_iface != NULL) {
      m_kind = m_iface->mod_kind;
    }  /* if */
  }  /* if */
  result.append(", entity id: ");
  switch (m_kind) {
    case mfk_edg_ifc:
    case mfk_ms_ifc:
      result.append(s_db_id_of_ifc_mep(mep));
      break;
    default:
      result.append("UNKNOWN");
      break;
  }  /* switch */
  result.append(", valid: ", (mep->invalid ? "FALSE" : "TRUE"));
  return result;
}  /* s_basic_db_mep */


void db_mep(a_module_entity_ptr mep)
/*
Display debug information about a module entity.
*/
{
  print(s_basic_db_mep(mep), f_debug);
  if (mep->entity.kind != iek_none) {
    (void)fputs("source location: ", f_debug);
    db_scp((a_source_correspondence*)mep->entity.ptr);
  }
  if (mep->scope != NULL) {
    (void)fputs("scope: ", f_debug);
    db_scope(mep->scope);
    (void)fputs("\n", f_debug);
  }  /* if */
}  /* db_mep */

#endif /* DEBUG */

void modules_pch_reset()
/*
Called when a PCH file has just been read to re-open any module files that
had been opened at the time the PCH file was created.
*/
{
  a_module_import_decl_ptr midp;

  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    a_module_interface_ptr iface = midp->module_info->module_interface;

    if (iface != NULL) {
      iface->pch_reset(midp);
    }  /* if */
  }  /* for */
}  /* modules_pch_reset */


void modules_check_for_suppressed_errors()
/*
Called after the translation unit has otherwise been processed to check
for suppressed errors while processing the module.
*/
{
  a_module_import_decl_ptr midp;

  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    a_module_interface_ptr iface = midp->module_info->module_interface;

    if (iface != NULL) {
      iface->report_suppressed_diagnostics();
    }  /* if */
  }  /* for */
}  /* modules_check_for_suppressed_errors */


void modules_one_time_init()
/*
Do one-time initialization of static variables defined in this file.
*/
{
  module_search_buffer = alloc_text_buffer(256);
  module_file_name_buffer = alloc_text_buffer(64);
  module_primary_name_buffer = alloc_text_buffer(64);
  module_partition_name_buffer = alloc_text_buffer(64);
  ifc_modules_one_time_init();
  /* Register variables that have distinct copies for distinct translation
     units. */
#if DEBUG
  register_trans_unit_variable(num_module_decls_attempted);
  register_trans_unit_variable(num_module_decls_failed);
#endif /* DEBUG */
  register_trans_unit_variable(curr_module_sym);
  register_trans_unit_variable(lazy_symbols_may_be_visible);
  register_trans_unit_variable(module_entity_hash_table);
  register_trans_unit_variable(module_entity_stack);
  register_trans_unit_variable(known_modules);
}  /* modules_one_time_init */


/* FIXME: PCH interactions? */
void modules_trans_unit_init()
/*
Initialization of things related to modules that must be repeated for every
translation unit.
*/
{
#if DEBUG
  num_module_decls_attempted = 0;
  num_module_decls_failed = 0;
#endif /* DEBUG */
  curr_module_sym = NULL;
  lazy_symbols_may_be_visible = FALSE;
  module_entity_hash_table = NULL;
  module_entity_stack = new_fe<a_module_entity_stack>();
  known_modules = new_fe<a_module_name_map>(/*mask_width=*/10);
  ifc_modules_trans_unit_init();
}  /* modules_trans_unit_init */


void modules_trans_unit_wrapup()
/*
Perform any wrapup operations needed for the translation unit.  This is
called after all processing for the translation unit (including template
instantiations, etc.) has been done.
*/
{
  ifc_modules_trans_unit_wrapup();
  delete_fe(&known_modules);
  delete_fe(&module_entity_stack);
  lazy_symbols_may_be_visible = FALSE;
}  /* modules_trans_unit_wrapup */

#if MAKE_FRONT_END_CALLABLE

void modules_cleanup()
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.  It performs any cleanup operations
required.  In particular, it closes any files that may have been open at
the point at which the compilation was terminated and makes sure any required
destructors are invoked.
*/
{
  a_module_import_decl_ptr midp;

  ifc_modules_cleanup();
  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    if (midp->module_info->module_interface != NULL) {
      midp->module_info->module_interface->close();
      delete_fe<a_module_interface>(&midp->module_info->module_interface);
    }  /* if */
  }  /* for */
}  /* modules_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

void modules_write_out()
/*
Write out the module files for the current translation unit.  This is called
after all processing for the translation unit (including template
instantiations, etc.) has been done.
*/
{
  Value_saver<a_boolean> in_front_end_saved(&in_front_end,
                                            /*new_value=*/FALSE);

  ifc_modules_write_out();
}  /* modules_write_out */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
