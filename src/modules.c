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
#include "pch.h"
#include "util.h"

/* Other required header files. */
#include "templates.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_module_entity::a_module_entity(a_module_ptr module_info_val)
/*
Construct a new module entity in the given module.
*/
  : module_info(module_info_val), sym_header(NULL), scope(NULL),
    entity{iek_none, NULL}, locators(), primary_locator_idx(0),
    imminent(FALSE), def_imminent(FALSE), uses_bound_token(FALSE),
    invalid(FALSE), global_module(FALSE), non_exported(FALSE)
{
}  /* a_module_entity::a_module_entity */

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
                        /* A mapping of module names to their corresponding
                           module IL entry. */


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
Find or create an IL module entity corresponding to the given module symbol.
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
      /* The name cannot be resolved; return an empty optional. */
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
    default_is_unexpected();
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
  a_deferred_module_entry_array
                *deferred_entries = sym_hdr->deferred_module_entries;

  /* This function should only be called if lexical processing is still
     enabled. */
  check_assertion(curr_lexical_state_stack_entry != NULL);
  /* This function should only be called if there are deferred module
     entities available. */
  check_assertion(deferred_entries != NULL);
#if DEBUG
  if (db_flag_is_set("ifc_symbols")) {
    a_string dbg_msg("Symbol load started (depth = ",
                     deferred_entries->num_active_scopes,
                     ") for \"", sym_hdr->identifier, "\" in ");

    print(dbg_msg, f_debug, /*end=*/"");
    db_scope(scope);
    (void)fputs("\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  ++deferred_entries->num_active_scopes;
  for (size_t i = 0; i < deferred_entries->entries.length(); ++i) {
    size_t                  entry_idx = (deferred_entries->entries.length() -
                                         (i + 1));
    a_deferred_module_entry &entry = deferred_entries->entries[entry_idx];
    if (entry.scope != scope) {
      continue;
    }  /* if */
#if DEBUG
    if (db_flag_is_set("ifc_symbols")) {
      (void)fprintf(f_debug, "Symbol load of \"%s\" in ", sym_hdr->identifier);
      db_scope(scope);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    ++deferred_entries->num_processed;
    entry.scope = NULL;

    a_module_entity_ptr mep = locate_module_entity(entry.locator);
    if (mep->scope == NULL) {
      mep->scope = scope;
    }  /* if */
    (void)request_entity(mep);
  }  /* for */
#if DEBUG
  if (db_flag_is_set("ifc_symbols")) {
    a_string dbg_msg("Symbol load finished (depth = ",
                     deferred_entries->num_active_scopes,
                     ") for \"", sym_hdr->identifier, "\" in ");

    print(dbg_msg, f_debug, /*end=*/"");
    db_scope(scope);
    (void)fputs("\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  if (--deferred_entries->num_active_scopes == 0) {
    size_t total_num_entries = deferred_entries->entries.length();

    if (deferred_entries->num_processed == total_num_entries) {
      /* Free the entire list as it's now unused. */
#if DEBUG
      if (db_flag_is_set("ifc_symbols")) {
        a_string dbg_msg("Symbol loading completed for \"",
                         sym_hdr->identifier, "\"");

        print(dbg_msg, f_debug);
      }  /* if */
#endif /* DEBUG */
      delete_fe(&sym_hdr->deferred_module_entries);
    } else if (deferred_entries->num_processed > 0) {
      /* Clean up any entries that match the current scope. */
#if DEBUG
      if (db_flag_is_set("ifc_symbols")) {
        a_string dbg_msg("Symbol loading partially completed for \"",
                         sym_hdr->identifier, "\" (",
                         deferred_entries->num_processed,
                         " entries completed)");

        print(dbg_msg, f_debug);
      }  /* if */
#endif /* DEBUG */
      auto has_been_processed =
                        [](const a_deferred_module_entry &entry) -> a_boolean {
        return entry.scope == NULL;
      };
      deferred_entries->entries.remove_if(has_been_processed);
      /* Since the counter num_active_scopes ensures this is the top level call
         to define_names_from_scope for this particular symbol header, it is
         safe to assume all entries that were considered processed have been
         removed. */
      check_assertion(((total_num_entries -
                        deferred_entries->entries.length()) ==
                       deferred_entries->num_processed));
      deferred_entries->num_processed = 0;
    }  /* if */
  }  /* if */
}  /* define_names_from_scope */


/*
A lightweight representation of a scope used for module entity hashing.  All
instances of a module entity scope should be obtained through
get_module_entity_scope.  This class has external linkage but a hidden
definition to prevent misuse and (slightly) reduce the size of the modules.h
header.
*/
struct a_module_entity_scope {
  a_symbol_header_ptr
                name;   /* The symbol header that represents the name of this
                           module entity scope. */
  a_module_entity_scope
                *parent;
                        /* The parent scope of this scope. */

  a_module_entity_scope()
    : name(NULL), parent(NULL)
    {}
  a_module_entity_scope(a_symbol_header_ptr   name_val,
                        a_module_entity_scope *parent_val)
    : name(name_val), parent(parent_val)
    {}
};  /*  a_module_entity_scope */


static uintptr_t hash_ptr(const a_module_entity_scope &key)
/*
Return a hash value for the given module entity scope.
*/
{
  uintptr_t  result = hash_ptr((void*)key.name);

  result = result*31 + hash_ptr((void*)key.parent);
  return result;
}  /* hash_ptr */


static a_boolean operator==(const a_module_entity_scope &a,
                            const a_module_entity_scope &b)
/*
Return TRUE if the given module entity scopes are equal; otherwise, return
FALSE.
*/
{
  a_boolean result = TRUE;

  if (a.name != b.name) {
    result = FALSE;
  } else if (a.parent != b.parent) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


static inline a_boolean operator!=(const a_module_entity_scope &a,
                                   const a_module_entity_scope &b)
/*
Return TRUE if the given module entity scopes are not equal; otherwise, return
FALSE.
*/
{
  return !(a == b);
}  /* operator!= */


a_module_template_parameter::a_module_template_parameter(
                                           a_module_template_parameter &&other)
/*
Move construct from the given module template parameter.
*/
  : kind(other.kind)
{
  switch (kind) {
    case mtpk_type:
      /* No op. */
      break;
    case mtpk_non_type:
      this->variant.type = other.variant.type;
      break;
    case mtpk_template:
      this->variant.params_list = other.variant.params_list;
      other.variant.params_list = NULL;
      break;
    default_is_unexpected();
  }  /* switch */
}  /* a_module_entity_key::~a_module_entity_key */


a_module_template_parameter::~a_module_template_parameter()
/*
Destroy the given module template parameter.
*/
{
  switch (kind) {
    case mtpk_type:
    case mtpk_non_type:
      /* No op. */
      break;
    case mtpk_template:
      delete_fe(&this->variant.params_list);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* a_module_template_parameter::~a_module_template_parameter */


a_module_template_parameter& a_module_template_parameter::operator=(
                                           a_module_template_parameter &&other)
/*
Move assign from the given module template parameter.
*/
{
  if (this != &other) {
    destroy(this);
    construct(this, move_from(&other));
  }  /* if */
  return *this;
}  /* a_module_template_parameter::operator== */

a_boolean operator==(const a_module_template_parameter &a,
                     const a_module_template_parameter &b)
/*
Return TRUE if the given module template parameters are equal; otherwise,
return FALSE.
*/
{
  a_boolean result = TRUE;

  if (a.kind != b.kind) {
    result = FALSE;
  } else {
    switch (a.kind) {
      case mtpk_non_type:
        { a_type_ptr type_a = a.variant.type;
          a_type_ptr type_b = b.variant.type;

          if (!il_identical_types(type_a, type_b)) {
            result = FALSE;
          }  /* if */
        }
        break;
      case mtpk_template:
        { a_module_template_parameter_list *params_a = a.variant.params_list;
          a_module_template_parameter_list *params_b = b.variant.params_list;

          if (params_a->length() != params_b->length()) {
            result = FALSE;
          } else {
            for (size_t i = 0; i < params_a->length(); ++i) {
              a_module_template_parameter &param_a = (*params_a)[i];
              a_module_template_parameter &param_b = (*params_b)[i];

              if (!(param_a == param_b)) {
                result = FALSE;
              }  /* if */
            }  /* for */
          }  /* if */
        }
        break;
      case mtpk_type:
        /* Nothing to compare. */
        break;
      default_is_unexpected();
    }  /* switch */
  }  /* if */
  return result;
}  /* operator== */

namespace {

/*
A key structure used for module entity hashing of functions.
*/
struct a_module_entity_function_key {
  Owning_ptr<a_module_func_param_list>
                parameter_types;
                        /* The parameter types. */
  a_boolean     has_ellipsis;
                        /* TRUE if the given function key represents a function
                           with a C-style ellipsis argument. */
};  /* a_module_entity_function_key */

/*
A key structure used for module entity hashing of specializations.
*/
struct a_module_entity_specialization_key {
  an_owned_template_arg_list
                arguments;
                        /* The argument set this entity is specialized on. */
};  /* a_module_entity_specialization_key */

/*
A key structure used for module entity hashing of function templates.
*/
struct a_module_entity_func_templ_key {
  a_module_entity_function_key
                function;
                        /* The function parameters for the function
                           template. */
  Owning_ptr<a_module_template_parameter_list>
                parameters;
                        /* The template parameters for the function
                           template. */
};  /* a_module_entity_func_templ_key */

/*
A key structure used for module entity hashing of function specializations.
*/
struct a_module_entity_func_spec_key {
  a_module_entity_function_key
                function;
                        /* The function parameters for the function
                           specialization. */
  a_module_entity_specialization_key
                specialization;
                        /* The function parameters for the function
                           specialization. */
};  /* a_module_entity_func_spec_key */

/*
A key structure used for module entity hashing of deduction guides.
*/
struct a_module_entity_deduct_guide_key {
  Owning_ptr<a_module_template_parameter_list>
                template_params;
                        /* The template parameters for the deduction guide. */
  Owning_ptr<a_module_deduct_guide_param_list>
                param_list;
                        /* The parameter types for the deduction guide. */
};  /* a_module_entity_deduct_guide_key */


enum a_module_entity_extra_info_kind {
  meeik_none,           /* A basic module entity that doesn't need extra
                           information. */
  meeik_alias,          /* A module entity for an alias. */
  meeik_function,       /* A module entity for a function. */
  meeik_specialization, /* A module entity for a specialized entity. */
  meeik_func_templ,     /* A module entity for a function template. */
  meeik_func_spec,      /* A module entity for a function specialization. */
  meeik_deduct_guide    /* A module entity for a deduction guide. */
};

/*
The primary key structure used for module entity hashing.  Non-common state is
dynamically allocated to reduce the size of the key structure (and by proxy,
the size of the module entity hash table -- see module_entity_hash_table).
*/
struct a_module_entity_key {
  a_module      *mod;   /* The module this entity is owned by. */
  a_module_entity_scope
                *scope; /* The module entity scope this entity resides in. */
  a_symbol_header_ptr
                name;   /* The symbol header that represents the name of this
                           module entity. */
  a_module_entity_extra_info_kind
                kind;   /* The extra info kind. */
  union {
    /* When kind == meeik_none or meeik_alias, no variant fields. */
    /* When kind == meeik_function: */
    a_module_entity_function_key
                *function;
                        /* A pointer to extra information about the function's
                           identity. */
    /* When kind == meeik_specialization: */
    a_module_entity_specialization_key
                *specialization;
                        /* A pointer to extra information about the
                           specialization's identity. */
    /* When kind == meeik_func_templ: */
    a_module_entity_func_templ_key
                *func_templ;
                        /* A pointer to extra information about the function
                           template's identity. */
    /* When kind == meeik_func_spec: */
    a_module_entity_func_spec_key
                *func_spec;
                        /* A pointer to extra information about the function
                           specialization's identity. */
    /* When kind == meeik_deduct_guide: */
    a_module_entity_deduct_guide_key
                *deduct_guide;
                        /* A pointer to extra information about the deduction
                           guide's identity. */
  } variant;
  inline a_module_entity_key() = default;
  a_module_entity_key(a_module_entity_key &&other);
  a_module_entity_key(const a_module_entity_key&) = delete;
  ~a_module_entity_key();
};  /* a_module_entity_key */


a_module_entity_key::a_module_entity_key(a_module_entity_key &&other)
/*
Move construct from the given module entity key.
*/
  : mod(other.mod), scope(other.scope), name(other.name), kind(other.kind),
    variant(other.variant)
{
  other.mod = NULL;
  other.scope = NULL;
  other.name = NULL;
  other.kind = meeik_none;
}  /* a_module_entity_key::~a_module_entity_key */


a_module_entity_key::~a_module_entity_key()
/*
Destroy the given module entity key.
*/
{
  switch (this->kind) {
    case meeik_none:
    case meeik_alias:
      /* No additional information to free. */
      break;
    case meeik_function:
      delete_fe(&this->variant.function);
      break;
    case meeik_specialization:
      delete_fe(&this->variant.specialization);
      break;
    case meeik_func_templ:
      delete_fe(&this->variant.func_templ);
      break;
    case meeik_func_spec:
      delete_fe(&this->variant.func_spec);
      break;
    case meeik_deduct_guide:
      delete_fe(&this->variant.deduct_guide);
      break;
  }  /* switch */
}  /* a_module_entity_key::~a_module_entity_key */


static a_boolean operator==(const a_module_entity_function_key &a,
                            const a_module_entity_function_key &b)
/*
Return TRUE if the given module entity function keys are equal; otherwise,
return FALSE.
*/
{
  a_boolean             result = TRUE;
  Dyn_array<a_type_ptr> *a_params = a.parameter_types.raw();
  Dyn_array<a_type_ptr> *b_params = b.parameter_types.raw();

  if (a_params->length() != b_params->length()) {
    result = FALSE;
  } else if (a.has_ellipsis != b.has_ellipsis) {
    result = FALSE;
  } else {
    for (size_t i = 0; i < a_params->length(); ++i) {
      a_type_ptr a_param = (*a_params)[i];
      a_type_ptr b_param = (*b_params)[i];

      /* Compare on object identity as all equal module entity pointers
         have the same address. */
      if (!routine_types_are_redecl_compatible(a_param, b_param,
                                               TCF_NO_FLAGS)) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* operator== */


static a_boolean operator==(const a_module_entity_specialization_key &a,
                            const a_module_entity_specialization_key &b)
/*
Return TRUE if the given module entity specialization keys are equal;
otherwise, return FALSE.
*/
{
  a_boolean          result = TRUE;
  a_template_arg_ptr a_args = a.arguments.raw();
  a_template_arg_ptr b_args = b.arguments.raw();
  size_t             a_n_args = count_list_elements(a_args);
  size_t             b_n_args = count_list_elements(b_args);

  if (a_n_args != b_n_args) {
    result = FALSE;
  } else if (a_n_args == 0) {
    /* This is an empty list compared to an empty list, this is equal. */
  } else {
    /* FIXME: Do we need to have eta_options_for_template (or some
       equivalent) factored in here? */
    an_equiv_templ_arg_options_set
                     eta_options = ETA_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED;

    if (!equiv_template_arg_lists(a_args, b_args, eta_options)) {
      result = FALSE;
    }  /* if */
  }
  return result;
}  /* operator== */


static a_boolean operator==(const a_module_entity_func_templ_key &a,
                            const a_module_entity_func_templ_key &b)
/*
Return TRUE if the given module entity function template keys are equal;
otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (!(a.function == b.function)) {
    result = FALSE;
  } else {
    a_module_template_parameter_list *a_params = a.parameters.raw();
    a_module_template_parameter_list *b_params = b.parameters.raw();

    if (a_params->length() != b_params->length()) {
      result = FALSE;
    } else {
      for (size_t i = 0; i < a_params->length(); ++i) {
        const a_module_template_parameter &a_param = (*a_params)[i];
        const a_module_template_parameter &b_param = (*b_params)[i];

        if (!(a_param == b_param)) {
          result = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return result;
}  /* operator== */


static a_boolean operator==(const a_module_entity_func_spec_key &a,
                            const a_module_entity_func_spec_key &b)
/*
Return TRUE if the given module entity function specialization keys are equal;
otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (!(a.function == b.function)) {
    result = FALSE;
  } else if (!(a.specialization == b.specialization)) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


static a_boolean operator==(const a_module_entity_deduct_guide_key &a,
                            const a_module_entity_deduct_guide_key &b)
/*
Return TRUE if the given module entity deduction guide keys are equal;
otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  {
    a_module_template_parameter_list *a_params = a.template_params.raw();
    a_module_template_parameter_list *b_params = b.template_params.raw();

    if (a_params->length() != b_params->length()) {
      result = FALSE;
      goto done;
    } else {
      for (size_t i = 0; i < a_params->length(); ++i) {
        const a_module_template_parameter &a_param = (*a_params)[i];
        const a_module_template_parameter &b_param = (*b_params)[i];

        if (!(a_param == b_param)) {
          result = FALSE;
          goto done;
        }  /* if */
      }  /* for */
    }  /* if */
  }
  {
    Dyn_array<a_type_ptr> *a_params = a.param_list.raw();
    Dyn_array<a_type_ptr> *b_params = b.param_list.raw();

    if (a_params->length() != b_params->length()) {
      result = FALSE;
      goto done;
    } else {
      for (size_t i = 0; i < a_params->length(); ++i) {
        a_type_ptr a_param = (*a_params)[i];
        a_type_ptr b_param = (*b_params)[i];

        /* Compare on object identity as all equal module entity pointers
           have the same address. */
        if (!routine_types_are_redecl_compatible(a_param, b_param,
                                                 TCF_NO_FLAGS)) {
          result = FALSE;
          goto done;
        }  /* if */
      }  /* for */
    }  /* if */
  }
done:
  return result;
}  /* operator== */


static uintptr_t hash_ptr(const a_module_entity_key &key)
/*
Return a hash value for the given module entity key.
*/
{
  uintptr_t  result = 0;

  if (key.mod != NULL) {
    a_module_ptr true_module = skip_module_partitions(key.mod);

    if (true_module->kind == mk_unit) {
      result = EDG_PREFIX::hash_ptr((void*)true_module);
    }  /* if */
  }  /* if */
  result = result*31 + EDG_PREFIX::hash_ptr((void*)key.scope);
  result = result*31 + EDG_PREFIX::hash_ptr((void*)key.name);
  result += key.kind;
  switch (key.kind) {
    case meeik_none:
    case meeik_alias:
      /* No additional information to hash. */
      break;
    case meeik_function:
      /* FIXME: Implement this. */
      break;
    case meeik_specialization:
      check_assertion(key.variant.specialization != NULL);
      /* FIXME: This hash isn't sufficiently stable, the "same" template
         argument list hashes differently. */
      /* result += hash_template_arg_list(
                                     key.variant.specialization->arguments); */
      break;
    case meeik_func_templ:
      /* FIXME: Implement this. */
      break;
    case meeik_func_spec:
      /* FIXME: Implement this. */
      break;
    case meeik_deduct_guide:
      /* FIXME: Implement this. */
      break;
  }  /* switch */
  return result;
}  /* hash_ptr */


static inline a_boolean is_same_module_or_global_module(
                                                  const a_module_entity_key &a,
                                                  const a_module_entity_key &b)
/*
Return TRUE if both of the given module entity keys reside in the same module
or both reside in the global module; otherwise, return FALSE.
*/
{
  a_boolean    result = FALSE;
  a_module_ptr true_module_a = skip_module_partitions(a.mod);
  a_module_ptr true_module_b = skip_module_partitions(b.mod);

  if (true_module_a == true_module_b) {
    /* The same IL module is being used, these are definitely the same. */
    result = TRUE;
  } else if (true_module_a != NULL && true_module_b != NULL) {
    if (true_module_a->kind == mk_header_unit &&
        true_module_a->kind == mk_header_unit) {
      /* These are both header units, and thus are the same module by virtue
         of being part of the global module. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_same_module_or_global_module */


static a_boolean operator==(const a_module_entity_key &a,
                            const a_module_entity_key &b)
/*
Return TRUE if the given module entity keys are equal; otherwise, return FALSE.
*/
{
  a_boolean result = TRUE;

  if (!is_same_module_or_global_module(a, b)) {
    result = FALSE;
  } else if (a.scope != b.scope) {
    result = FALSE;
  } else if (a.name != b.name) {
    result = FALSE;
  } else if (a.kind != b.kind) {
    result = FALSE;
  } else {
    /* FIXME: Use a dedicated operator== for each of these rather than inline
       implementations. */
    switch (a.kind) {
      case meeik_none:
      case meeik_alias:
        /* No additional information to compare. */
        break;
      case meeik_function:
        if (!(*a.variant.function == *b.variant.function)) {
          result = FALSE;
        }  /* if */
        break;
      case meeik_specialization:
        if (!(*a.variant.specialization == *b.variant.specialization)) {
          result = FALSE;
        }  /* if */
        break;
      case meeik_func_templ:
        if (!(*a.variant.func_templ == *b.variant.func_templ)) {
          result = FALSE;
        }  /* if */
        break;
      case meeik_func_spec:
        if (!(*a.variant.func_spec == *b.variant.func_spec)) {
          result = FALSE;
        }  /* if */
        break;
      case meeik_deduct_guide:
        if (!(*a.variant.deduct_guide == *b.variant.deduct_guide)) {
          result = FALSE;
        }  /* if */
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* operator== */


static inline a_boolean operator!=(const a_module_entity_key &a,
                                   const a_module_entity_key &b)
/*
Return TRUE if the given module entity keys are not equal; otherwise, return
FALSE.
*/
{
  return !(a == b);
}  /* operator!= */

}  /* namespace */

using a_module_entity_scope_hash_table = Ptr_map<a_module_entity_scope,
                                                 a_module_entity_scope*>;
                        /* The type used for the module entity scope hash
                           table. */

static a_module_entity_scope_hash_table
                *module_entity_scope_hash_table;
                        /* A hash table to find module entities, mapping module
                           entity scopes to a single (equivalent) instance.
                           The (module entity scope) values are not directly
                           represented to ensure they have a stable address
                           that does not changed during reallocation of the
                           hash table. */

static a_module_entity_scope
                *trans_unit_module_entity_scope;
                        /* The translation unit module entity scope. */


a_module_entity_scope* get_module_entity_scope(a_symbol_header_ptr   name,
                                               a_module_entity_scope *parent)
/*
Return a pointer to the module entity scope with the given name and parent
module entity scope.  The same module entity scopes is returned by this
function given equivalent arguments.  All module entity scope objects should be
obtained through this function to ensure consistent representation.
*/
{
  a_module_entity_scope *result = NULL;

  if (name == NULL && parent == NULL) {
    /* This is the translation unit scope, which is handled via a special
       case. */
    if (trans_unit_module_entity_scope == NULL) {
      trans_unit_module_entity_scope = new_fe<a_module_entity_scope>();
    }  /* if */
    result = trans_unit_module_entity_scope;
  } else {
    /* Find or create a module entity scope for the given name and parent
       scope. */
    a_module_entity_scope key{name, parent};
    uint8_t               hashed_key = hash_ptr(key);

    result = module_entity_scope_hash_table->get_with_hash(key, hashed_key);
    if (result == NULL) {
      result = new_fe<a_module_entity_scope>(key);
      /* Update the hash table to point to the new module entity scope. */
      module_entity_scope_hash_table->map_with_hash(key, result, hashed_key);
    }  /* if */
  }  /* if */
  return result;
}  /* get_module_entity_scope */


using a_module_entity_hash_table = Ptr_map<a_module_entity_key,
                                           a_module_entity_ptr>;
                        /* The type used for the module entity hash table. */

static a_module_entity_hash_table
                *module_entity_hash_table;
                        /* A hash table to find module entities, mapping module
                           entity keys to module entity pointer values.  The
                           (module entity) values are not directly represented
                           to ensure they have a stable address that does not
                           changed during reallocation of the hash table. */


static a_module_entity_ptr get_module_entity_from_key(
                                                     a_module_entity_key &&key)
/*
Return the module entity pointer corresponding to the given module entity key.

If no module entity exists for the given key value, the key value will be moved
into the hash table with a new module entity value (that is constructed with an
initial representation derived from the key).
*/
{
  /* A module interface must be present, otherwise this module entity pointer
     is not viable. */
  check_assertion(key.mod != NULL && key.mod->module_interface != NULL);
  uintptr_t           hashed_key = hash_ptr(key);
  a_module_entity_ptr mep =
                           module_entity_hash_table->get_with_hash(key,
                                                                   hashed_key);
  if (mep == NULL) {
    /* Create a new module entity.  These are allocated in front end memory
       (so they are saved in PCH files) and never freed. */
    mep = new_fe<a_module_entity>(key.mod);
    mep->sym_header = key.name;
    /* Update the hash table to point to the new module entity. */
    module_entity_hash_table->map_with_hash(move_from(&key), mep, hashed_key);
  }  /* if */
  return mep;
}  /* get_module_entity_from_key */


a_module_entity_ptr get_module_entity(a_module_ptr          mod,
                                      a_module_entity_scope *scope,
                                      a_symbol_header_ptr   name)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope and name.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_none;

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_module_entity */


a_module_entity_ptr get_alias_module_entity(a_module_ptr          mod,
                                            a_module_entity_scope *scope,
                                            a_symbol_header_ptr   name)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope and name.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_alias;

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_module_entity */


a_module_entity_ptr get_function_module_entity(
                            a_module_ptr                         mod,
                            a_module_entity_scope                *scope,
                            a_symbol_header_ptr                  name,
                            Owning_ptr<a_module_func_param_list> &&func_params,
                            a_boolean                            has_ellipsis)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope, name, and function parameters.  has_ellipsis should be TRUE if
the given module entity is a C-style variable argument function.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_function;
  key.variant.function = new_fe<a_module_entity_function_key>();
  key.variant.function->parameter_types = move_from(&func_params);
  key.variant.function->has_ellipsis = has_ellipsis;

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_function_module_entity */


a_module_entity_ptr get_specialized_module_entity(
                                    a_module_ptr               mod,
                                    a_module_entity_scope      *scope,
                                    a_symbol_header_ptr        name,
                                    an_owned_template_arg_list &&template_args)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope, name, and template arguments.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_specialization;
  key.variant.specialization = new_fe<a_module_entity_specialization_key>();
  key.variant.specialization->arguments = move_from(&template_args);

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_specialized_module_entity */


a_module_entity_ptr get_function_template_module_entity(
                a_module_ptr                                 mod,
                a_module_entity_scope                        *scope,
                a_symbol_header_ptr                          name,
                Owning_ptr<a_module_template_parameter_list> &&template_params,
                Owning_ptr<a_module_func_param_list>         &&func_params,
                a_boolean                                    has_ellipsis)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope, name, and template arguments.  has_ellipsis should be TRUE if
the given module entity is a C-style variable argument function.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_func_templ;
  key.variant.func_templ = new_fe<a_module_entity_func_templ_key>();
  key.variant.func_templ->function.parameter_types = move_from(&func_params);
  key.variant.func_templ->function.has_ellipsis = has_ellipsis;
  key.variant.func_templ->parameters = move_from(&template_params);

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_function_template_module_entity */


a_module_entity_ptr get_specialized_function_module_entity(
                          a_module_ptr                         mod,
                          a_module_entity_scope                *scope,
                          a_symbol_header_ptr                  name,
                          an_owned_template_arg_list           &&template_args,
                          Owning_ptr<a_module_func_param_list> &&func_params,
                          a_boolean                            has_ellipsis)
/*
Return a pointer to the module entity for the entity in the given module, with
the given scope, name, and template arguments.  has_ellipsis should be TRUE if
the given module entity is a C-style variable argument function.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_func_spec;
  key.variant.func_spec = new_fe<a_module_entity_func_spec_key>();
  key.variant.func_spec->function.parameter_types = move_from(&func_params);
  key.variant.func_spec->function.has_ellipsis = has_ellipsis;
  key.variant.func_spec->specialization.arguments = move_from(&template_args);

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_specialized_function_module_entity */


a_module_entity_ptr get_deduction_guide_module_entity(
                a_module_ptr                                 mod,
                a_module_entity_scope                        *scope,
                a_symbol_header_ptr                          name,
                Owning_ptr<a_module_template_parameter_list> &&template_params,
                Owning_ptr<a_module_deduct_guide_param_list> &&param_list)
/*
Return a pointer to the deduction guide module entity for the entity in the
given module, with the given scope, name, template parameters, and deduction
guide parameter list.
*/
{
  a_module_entity_key key;
  key.mod = mod;
  key.scope = scope;
  key.name = name;
  key.kind = meeik_deduct_guide;
  key.variant.deduct_guide = new_fe<a_module_entity_deduct_guide_key>();
  key.variant.deduct_guide->template_params = move_from(&template_params);
  key.variant.deduct_guide->param_list = move_from(&param_list);

  a_module_entity_ptr result = get_module_entity_from_key(move_from(&key));
  return result;
}  /* get_deduction_guide_module_entity */


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
  a_module_entity_stack_entry mese{};

  mese.mep = mep;
  mese.saved_error_position = error_position;
  module_entity_stack->push_back(mese);
  if (mep != NULL) {
    Opt<a_source_position> opt_pos_info = source_position_of(mep);

    if (opt_pos_info.has_value()) {
      error_position = *opt_pos_info;
    } else {
      error_position = null_source_position;
    }  /* if */
  }  /* if */
}  /* push_module_entity_state */


void pop_module_entity_state()
/*
Pop the current module entity from the module entity stack.
*/
{
 a_module_entity_stack_entry &mese = module_entity_stack->back_elem();

 error_position = mese.saved_error_position;
 module_entity_stack->pop_back();
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


a_module_entity_ptr locate_module_entity(a_module_entry_locator loc)
/*
Return the module entity referenced by the given module entry locator.
*/
{
  a_module_entity_ptr result = NULL;

  switch (loc.kind) {
    case melk_none:
      /* An unknown locator should never be passed to this function. */
      unexpected_condition();
    case melk_ifc:
      result = locate_ifc_module_entity(loc);
      break;
    default_is_unexpected();
  }  /* switch */
  /* The module entity should never be NULL.  Instead, an error module entity
     locator should be returned. */
  check_assertion(result != NULL);
  return result;
}  /* locate_module_entity */


a_boolean request_entity(a_module_entity_ptr mep)
/*
Request that the given module entity be processed (if not already being
processed).  If the entity's processing is complete, return TRUE; otherwise,
return FALSE.

This function should be preferred when immediate processing is not required
(i.e., the exact entity doesn't need to be known).  process_ifc_declaration
should be preferred if the entity should be processed immediately.
*/
{
  if (!is_entity_resolved(mep) && !is_entity_imminent(mep)) {
    switch (mep->locators[mep->primary_locator_idx].kind) {
      case melk_none:
        /* If this assertion is hit, a locator with no usable information was
           put onto the list of deferred entities; this should never happen. */
        unexpected_condition();
        break;
      case melk_ifc:
        process_ifc_declaration(mep);
        break;
      default_is_unexpected();
    }  /* switch */
  }  /* if */
  return is_entity_resolved(mep);
}  /* load_namespace_elements_from_locator */


void load_namespace_elements_from_locator(a_module_entity_ptr    mep,
                                          a_module_entry_locator loc)
/*
Load the namespace elements specified by the given module entry locator.
*/
{
  switch (loc.kind) {
    case melk_none:
      /* An unknown locator should never be passed to this function. */
      unexpected_condition();
    case melk_ifc:
      load_namespace_elements_from_ifc_locator(mep, loc);
      break;
    default_is_unexpected();
  }  /* switch */
}  /* load_namespace_elements_from_locator */


void mark_locator_as_primary(a_module_entity_ptr    mep,
                             a_module_entry_locator loc)
/*
Mark the given locator as the primary locator for the given module entity.
*/
{
  /* In most cases the locator being marked primary is the most recently
     added locator so traverse in reverse. */
  for (unsigned k = mep->locators.length(); k > 0; --k) {
    unsigned               idx = k - 1;
    a_module_entry_locator &idx_loc = mep->locators[idx];

    if (idx_loc == loc) {
      mep->primary_locator_idx = idx;
      goto done;
    }  /* if */
  }  /* if */
  /* If this condition is reached, the locator was not found in the module
     entity's locators.  The locator should have been added by
     update_entity_from_new_locator. */
  unexpected_condition();
done:;
}  /* load_namespace_elements_from_locator */


void update_entity_from_new_locator(a_module_entity_ptr    mep,
                                    a_module_entry_locator new_loc)
/*
A new locator has been discovered for the given module entity.  Add the locator
to the list of known locators and (if necessary) inform the corresponding
module implementation about the new information.
*/
{
  mep->locators.push_back(new_loc);
#if DEBUG
  if (db_flag_is_set("module_entity_locator") && mep->locators.length() > 1) {
    a_string  dbg_msg("Entity identified by multiple locators: ");
    a_boolean first = TRUE;

    for (const a_module_entry_locator &loc : mep->locators) {
      if (!first) {
        dbg_msg.append(", ");
      }  /* if */
      first = FALSE;
      dbg_msg.append(s_db_module_entry_locator(loc));
    }  /* for */
    print(dbg_msg, f_debug);
  }  /* if */
#endif /* DEBUG */
  if (mep->entity.ptr != NULL) {
    /* Notify the appropriate module implementation. */
    switch (new_loc.kind) {
      case melk_none:
        /* An unknown locator should never be passed to this function. */
        unexpected_condition();
      case melk_ifc:
        update_entity_from_new_ifc_locator(mep, new_loc);
        break;
      default_is_unexpected();
    }  /* switch */
    if (mep->entity.kind == iek_namespace) {
      /* Load any new namespace elements. */
      load_namespace_elements_from_locator(mep, new_loc);
    }  /* if */
  }  /* if */
}  /* update_entity_from_new_locator */


Opt<a_source_position> source_position_of(a_module_entity_ptr mep)
/*
Return the source position of the given module entity pointer if available;
otherwise, return an empty optional.
*/
{
  Opt<a_source_position> result;
  a_module_entry_locator &primary = mep->locators[mep->primary_locator_idx];

  switch (primary.kind) {
    case melk_ifc:
      result = source_position_from_ifc_of(mep);
      break;
    case melk_none:
      unexpected_condition();
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* source_position_of */

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


a_string s_db_module_entry_locator(a_module_entry_locator loc)
/*
Return a string containing a string representation of the given module entry
locator.
*/
{
  a_string result;

  switch (loc.kind) {
    case melk_none:
      result = "NONE";
      break;
    case melk_ifc:
      result = s_db_ifc_locator(loc);
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* s_db_module_entry_locator */


void db_module_entry_locator(a_module_entry_locator loc)
/*
Display debug information about the specified module entry locator.
*/
{
  print(s_db_module_entry_locator(loc), f_debug);
}  /* db_module_entry_locator */


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

static inline void close_module_files()
/*
Close any open module file handles.
*/
{
  for (a_module_import_decl_ptr midp = il_header.imported_modules;
       midp != NULL; midp = midp->next) {
    if (midp->module_info->module_interface != NULL) {
      midp->module_info->module_interface->close();
    }  /* if */
  }  /* for */
}  /* close_module_files */


void modules_pch_prepare()
/*
Called when a PCH file is about to be written to prepare the modules system
for the PCH write.
*/
{
  /* Module files must be reopened by the new process.

     Note that we cannot free instances of the module interface an_ifc_module
     prior to PCH writing as the address of the an_ifc_module::file member is
     required for various internal IFC module Ptr_map keys to work (e.g.,
     various Ptr_maps use Index_entity values as keys, these values use
     an_ifc_module_file* pointer values for hashing and equality
     operations). */
  close_module_files();
}  /* modules_pch_prepare */


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
  /* Save variables from ifc_modules.h and ifc_modules.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_module_sym),
      pch_saved_var_array_elem(lazy_symbols_may_be_visible),
      pch_saved_var_array_elem(module_entity_scope_hash_table),
      pch_saved_var_array_elem(trans_unit_module_entity_scope),
      pch_saved_var_array_elem(module_entity_hash_table),
      pch_saved_var_array_elem(module_entity_stack),
      pch_saved_var_array_elem(known_modules),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
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
  register_trans_unit_variable(module_entity_scope_hash_table);
  register_trans_unit_variable(trans_unit_module_entity_scope);
  register_trans_unit_variable(module_entity_hash_table);
  register_trans_unit_variable(module_entity_stack);
  register_trans_unit_variable(known_modules);
}  /* modules_one_time_init */


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
  module_entity_scope_hash_table =
                   new_fe<a_module_entity_scope_hash_table>(/*mask_width=*/10);
  trans_unit_module_entity_scope = NULL;
  module_entity_hash_table =
                         new_fe<a_module_entity_hash_table>(/*mask_width=*/10);
  module_entity_stack = new_fe<a_module_entity_stack>();
  known_modules = new_fe<a_module_name_map>(/*mask_width=*/10);
  ifc_modules_trans_unit_init();
}  /* modules_trans_unit_init */


void modules_trans_unit_wrapup_part_1()
/*
Perform the initial modules-related wrapup operations needed for the
translation unit.  This is called after all processing for the translation unit
(including template instantiations, etc.) has been done but before scopes have
been wrapped up.

This phase is primarily used to shutdown the lazy loading system and ensure no
additional entities are added to the IL.
*/
{
  lazy_symbols_may_be_visible = FALSE;
}  /* modules_trans_unit_wrapup_part_1 */


static inline void free_module_interfaces()
/*
Free any module interfaces.
*/
{
  for (a_module_import_decl_ptr midp = il_header.imported_modules;
       midp != NULL; midp = midp->next) {
    if (midp->module_info->module_interface != NULL) {
      switch (midp->module_info->file_kind) {
        case mfk_unknown:
          /* The module file kind should have been set if this module import
             declaration appeared on the list of imported modules. */
          unexpected_condition();
        case mfk_edg_ifc:
        case mfk_ms_ifc:
          delete_fe<an_ifc_module>(
                        (an_ifc_module**)&midp->module_info->module_interface);
          break;
        default_is_unexpected();
      }  /* switch */
    }  /* if */
  }  /* for */
}  /* free_module_interfaces */


void modules_trans_unit_wrapup_part_2()
/*
Perform final modules-related wrapup operations needed for the translation
unit.  This is called after all processing for the translation unit (including
template instantiations, etc.) and scope wrapup have been done.

This phase is primarily used to deallocate objects allocated for modules
support in the current translation unit.
*/
{
  ifc_modules_trans_unit_wrapup();
  close_module_files();
  free_module_interfaces();
  delete_fe(&known_modules);
  delete_fe(&module_entity_stack);
  delete_fe(&module_entity_hash_table);
  if (trans_unit_module_entity_scope != NULL) {
    delete_fe(&trans_unit_module_entity_scope);
  }  /* if */
  delete_fe(&module_entity_scope_hash_table);
}  /* modules_trans_unit_wrapup_part_2 */

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
  ifc_modules_cleanup();
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
