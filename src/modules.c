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
  a_const_char	*suffix;
			/* The suffix associated with the module file. */
  a_module_kind	kind;
			/* The kind of module this suffix implies. */
};  /* a_module_file_suffix */

constexpr a_module_file_suffix module_file_suffixes[] = {
  { "edgm", (a_module_kind)mk_edg },
#if MICROSOFT_EXTENSIONS_ALLOWED
  { "ifc", (a_module_kind)mk_ifc }
#endif  /* MICROSOFT_EXTENSIONS_ALLOWED */
};

constexpr a_byte edg_magic_numbers[] = { 0x9A, 0x13, 0x37, 0x7D };

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
       module_sym->variant.module_info.partition_name != NULL) &&
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
}  /* get_module_file_name */


static a_module_kind determine_module_file_kind(FILE *file)
/*
Given an already open file pointer, determine what kind of module file is open
(if any).  Return the kind of module file.
*/
{
  a_module_kind kind = (a_module_kind)mk_none;
  a_byte        magic[4];

  /* Ensure we're at the start of the file. */
  if (fseek(file, 0, SEEK_SET) != 0) {
    catastrophe(ec_module_read_error);
  }  /* if */
  if (fread(magic, (size_t)1, sizeof(magic), file) == sizeof(magic)) {
    if (magic_numbers_match(magic, edg_magic_numbers)) {
      kind = (a_module_kind)mk_edg;
    } else if (magic_numbers_match(magic, ifc_magic_numbers)) {
      kind = (a_module_kind)mk_ifc;
    }  /* if */
  }  /* if */
  return kind;
}  /* determine_module_file_kind */


static inline a_const_char *err_string_for_module_kind(a_module_kind kind)
/*
Given a module kind, return a string for that kind for use in error messages.
*/
{
  a_const_char *str;

  switch (kind) {
    case mk_none:
      str = error_text(ec_module_kind_none);
      break;
    case mk_header:
      str = error_text(ec_module_kind_header);
      break;
    case mk_edg:
      str = error_text(ec_module_kind_edg);
      break;
    case mk_ifc:
      str = error_text(ec_module_kind_ifc);
      break;
    case mk_any:
      str = error_text(ec_module_kind_any);
      break;
    default:
      str = error_text(ec_module_kind_unexpected);
      unexpected_condition_str("Unexpected module kind");
  }  /* switch */
  return str;
}  /* err_string_for_module_kind */


static void diagnose_mismatched_module_file_kind(a_module_kind file_kind,
                                                 a_module_kind expected_kind,
                                                 a_const_char  *module_file)
/*
Given a module file, issue diagnostics for a mismatch between expected_kind and
file_kind (which may range from remarks to catastrophic errors).
*/
{
  an_error_severity severity;
  a_diagnostic_ptr  dp;

  if (file_kind == expected_kind) {
    /* No mismatch, no diagnostics to issue. */
    goto done;
  }  /* if */
  switch (expected_kind) {
    case mk_edg:
      severity = es_catastrophe;
      break;
    case mk_any:
      /* Match anything except for unknown module files and header units. */
      if (file_kind != (a_module_kind)mk_none &&
          file_kind != (a_module_kind)mk_header) {
        goto done;
      }  /* if */
      FALLTHROUGH
    case mk_ifc:
      /* Visual Studio skips files that don't appear to be IFCs. */
      severity = es_remark;
      break;
    case mk_none:
    case mk_header:
    default:
      severity = es_catastrophe;
      unexpected_condition_str("Unexpected module kind");
  }  /* switch */
  dp = pos_st2_start_diagnostic(severity, ec_mismatched_module_file_kind,
                                &error_position,
                                err_string_for_module_kind(expected_kind),
                                err_string_for_module_kind(file_kind));
  str_add_diag_info(dp, ec_mismatched_module_file_context, module_file);
  end_diagnostic(dp);
done:;
}  /* diagnose_mismatched_module_file_kind */


static a_boolean check_module_file(a_module_kind *kind,
                                   a_const_char  *module_file)
/*
Return TRUE if the provided module file exists and is the given kind, FALSE
otherwise.  If *kind == mk_any, update *kind with the determined kind of the
module file (any supported kind is a match).  This function may not return and
instead issue a catastrophic error if the module file exists but cannot be
opened, or if it does not match the expected kind and such a mismatch cannot be
ignored.
*/
{
  a_boolean           result = FALSE;
  FILE*               file;
  an_open_file_result open_result;
  a_module_kind       file_kind;

  file = fopen_with_result(module_file, FOPEN_MODE_FOR_BINARY_READ,
                           &open_result);
  if (file == NULL) {
    /* Open failed.  Most reasons are likely valid, but check for specific
       problem cases. */
    if (open_result.flags & OFR_CANNOT_OPEN) {
      /* Note that file_open_error does not return when called from here. */
      file_open_error(es_catastrophe, ec_module_file, module_file,
                      &open_result);
    } /* if */
    goto done;
  }  /* if */
  /* We've found a file - determine what kind it is. */
  file_kind = determine_module_file_kind(file);
  (void)fclose(file);
  if (file_kind != (a_module_kind)mk_none && *kind == (a_module_kind)mk_any) {
    *kind = file_kind;
  }  /* if */
  if (*kind == file_kind) {
    result = TRUE;
  } else {
    /* Module file matched the expected extension for this type, but did
       not match the expected content indicators.  This may or may not
       issue a catastrophic error - if this returns, that means we ignore
       the file and continue on searching. */
    diagnose_mismatched_module_file_kind(file_kind, *kind, module_file);
  }  /* if */
done:
  return result;
}  /* check_module_file */


static a_boolean module_file_matches(a_const_char  *module_name,
                                     a_const_char  *module_file,
                                     a_module_kind kind)
/*
Return TRUE if module_file is a module file for module_name (i.e., the name of
the module in the module file matches module_name), FALSE otherwise.  kind is
the kind of module_file.
*/
{
  a_boolean result = FALSE;

  switch (kind) {
    case mk_none:
    case mk_any:
    case mk_header:
      unexpected_condition_str("Unexpected module kind");
      break;
    case mk_edg:
      { an_edg_module mod;
        result = mod.matches_module(module_name, module_file);
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      { an_ifc_module mod;
        result = mod.matches_module(module_name, module_file);
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* module_file_matches */


static a_boolean find_module_file_in_map(a_module_ptr  mod,
                                         a_module_kind kind)
/*
Find the module file associated with mod in the module map and update mod with
the path to the file.  If kind == mk_any and a mapping exists, determine the
kind of (supported) module file encountered and update the kind of mod.
Otherwise, only consider module files of the kind indicated by kind.  Return
TRUE if a module file was found, FALSE otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean      found = FALSE;
  a_C_str_handle module_name{mod->name};
  a_const_char   *module_path;

  module_path = mod_map->get(module_name);
  if (module_path != NULL) {
    if (check_module_file(&kind, module_path)) {
      mod->kind = kind;
      mod->full_name = copy_string_to_region(file_scope_region_number,
                                             module_path);
      found = TRUE;
    } else {
      /* A mapping for this file exists but either the file cannot be read
         (doesn't exist, insufficient permissions, etc.) or it's not the right
         kind. */
      pos_st_catastrophe(ec_invalid_module_file_map, &error_position,
                         mod->name);
    }  /* if */
  }  /* if */
  return found;
}  /* find_module_file_in_map */


static a_boolean find_header_unit_in_map(a_module_ptr  mod,
                                         a_module_kind kind)
/*
Find the module file associated with mod in the header unit map and update mod
with the path to the file.  If kind == mk_any and a mapping exists, determine
the kind of (supported) module file encountered and update the kind of mod.
Otherwise, only consider module files of the kind indicated by kind.  Return
TRUE if a module file was found, FALSE otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean     found = FALSE;
  a_const_char  *module_path;

  module_path = resolve_header_in_map(mod->name, mod->resolved_header,
                                      mod->is_sys_include);
  if (module_path != NULL) {
    if (check_module_file(&kind, module_path)) {
      mod->kind = kind;
      mod->full_name = copy_string_to_region(file_scope_region_number,
                                             module_path);
      found = TRUE;
    } else {
      /* A mapping for this file exists but either the file cannot be read
         (doesn't exist, insufficient permissions, etc.) or it's not the right
         kind. */
      pos_st_catastrophe(ec_invalid_module_file_map, &error_position,
                         mod->name);
    }  /* if */
  }  /* if */
  return found;
}  /* find_header_unit_in_map */


static a_boolean find_module_file_in_list(a_module_ptr  mod,
                                          a_module_kind kind)
/*
Find the module file associated with mod in the list of module files and
update mod with the path to the file.  If kind == mk_any, select the first
(supported) module file encountered and update the kind of mod.  Otherwise,
only consider module files of the kind indicated by kind.  Return TRUE if a
module file was found, FALSE otherwise.

This routine is a helper for find_module_file and assumes the module file has
not already been found - deferring diagnostics related to failing to find the
module file to the caller.
*/
{
  a_boolean                  found = FALSE;
  a_directory_name_entry_ptr mod_list = mod_map_search_path;

  for (; mod_list != NULL; mod_list = mod_list->next) {
    a_module_kind this_kind = kind;
    if (check_module_file(&this_kind, mod_list->dir_name) &&
        module_file_matches(mod->name, mod_list->dir_name, this_kind)) {
      /* The file exists, is a valid kind, and matches the module. */
      if (found) {
        /* More than one match was found. */
        pos_st_catastrophe(ec_multiple_module_matches, &error_position,
                           mod->name);
        /* break; */  /* Unreachable as pos_st_catastrophe exits. */
      } else {
        found = TRUE;
        mod->kind = this_kind;
        mod->full_name = copy_string_to_region(file_scope_region_number,
                                               mod_list->dir_name);
        /* MSVC issues an error if more than one file in the list matches, so
           we must search the entire list even if we've found one. */
        if (!microsoft_mode) {
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return found;
}  /* find_module_file_in_list */


static a_boolean find_module_file_in_dirs(a_module_ptr  mod,
                                          a_module_kind kind)
/*
Find the module file associated with mod in the module search paths and
update mod with the path to the file.  If kind == mk_any, select the first
(supported) module file encountered and update the kind of mod.  Otherwise,
only consider module files of the kind indicated by kind.  Return TRUE if a
module file was found, FALSE otherwise.

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
      a_module_kind skind = suffix.kind;
      if (kind != (a_module_kind)mk_any && suffix.kind != kind) continue;
      replace_file_name_suffix(suffix.suffix, module_search_buffer);
      if (check_module_file(&skind, module_search_buffer->buffer)) {
        found = TRUE;
        mod->kind = suffix.kind;
        mod->full_name = copy_string_to_region(file_scope_region_number,
                                               module_search_buffer->buffer);
        break;
      }  /* if */
    }  /* for */
  }  /* for */
  return found;
}  /* find_module_file_in_dirs */


a_boolean find_module_file(a_module_ptr  mod,
                           a_module_kind kind)
/*
Find the module file associated with mod and update mod with the path to the
file.  If kind == mk_any, select the first (supported) module file encountered
and update the kind of mod.  Otherwise, only consider module files of the kind
indicated by kind.  Return TRUE if a module file was found, FALSE otherwise.
*/
{
  a_boolean found = FALSE;

  if (mod->full_name != NULL) {
    /* Module file has already been found. */
    found = TRUE;
  }  /* if */
  if (found || skip_module_imports) {
    goto done;
  }  /* if */
  if (mod->kind == (a_module_kind)mk_header) {
    a_const_char *header_path;

    header_path = resolve_header(mod->name, mod->is_sys_include,
                                 /*is_include_next=*/FALSE,
                                 /*suppress_diagnostics=*/FALSE);
    if (header_path == NULL) {
      pos_st_catastrophe(ec_cannot_find_header_for_import, &error_position,
                         mod->name);
    } else {
      mod->resolved_header = header_path;
      found = find_header_unit_in_map(mod, kind);
    }  /* if */
  } else {
    found = find_module_file_in_map(mod, kind);
    if (!found) {
      found = find_module_file_in_list(mod, kind);
    }  /* if */
    if (!found) {
      found = find_module_file_in_dirs(mod, kind);
    }  /* if */
    if (!found) {
      pos_st_catastrophe(ec_module_file_not_found, &error_position, mod->name);
    } else if (!module_file_matches(mod->name, mod->full_name, mod->kind)) {
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

  check_assertion(midp->module_info->full_name != NULL);
  switch (midp->module_info->kind) {
    case mk_edg:
      iface = new_general<an_edg_module>();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      iface = new_general<an_ifc_module>();
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case mk_none:
    case mk_any:
    case mk_header:
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
scope.
*/
{
  a_boolean           scope_pushed = FALSE;
  a_module_entity_ptr mep, *mepp = &(sym_hdr->deferred_module_entities);

  check_assertion(sym_hdr->deferred_module_entities != NULL);
  scope_pushed = push_module_declaration_context(scope);
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
      *mepp = mep->next;
      switch (mep->module_info->kind) {
#if MICROSOFT_EXTENSIONS_ALLOWED
        case mk_ifc:
          ((an_ifc_module*)mep->module_info->module_interface)->
            process_ifc_declaration(mep, /*defer=*/FALSE, (a_type_ptr)NULL);
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case mk_edg:
        default:
          unexpected_condition();
      }  /* switch */
    } else {
      mepp = &(*mepp)->next;
    }  /* if */
  }  /* for */
  pop_module_declaration_context(scope_pushed);
}  /* define_names_from_scope */


void complete_definition_of_module_class(a_type_ptr class_type)
/*
This routine is called (via complete_class_type_is_needed) when the front end
has determined that the class is defined in a module and now needs a
definition.  Complete the given class's definition from the information
contained in the module that provided the class.
*/
{
  a_class_type_supplement_ptr   ctsp = class_type_supp(class_type);
  a_module_entity_ptr           mep = ctsp->module_entity;
  a_boolean                     scope_pushed = FALSE;

  check_assertion(mep != NULL);
  if (!class_type->definition_pending) {
    class_type->definition_pending = TRUE;
    scope_pushed = push_module_declaration_context(mep->scope);
    mep->module_info->module_interface->
                                      complete_definition_of_module_class(mep);
    /* Once the class is defined, there is no need for this information. */
    ctsp->module_entity = NULL;
    class_type->definition_pending = FALSE;
    pop_module_declaration_context(scope_pushed);
  }  /* if */
}  /* complete_definition_of_module_class */


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
    (*p)->entity.kind = (a_byte_il_entry_kind)iek_none;
    (*p)->file_offset = file_offset;
    (*p)->imminent = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    (*p)->variant.ifc_partition = ifc_none;
#endif /* MICROSOFT_EXTENSIONS_ALLWED */
  }  /* if */
  return *p;
}  /* get_module_entity_ptr */

#if !USE_VIRTUAL_FUNCTIONS

/*lint -esym(1714,*a_module_interface::is_open)*/ /* FIXME: temporary*/
a_boolean a_module_interface::is_open() const
/*
Dispatch the is_open() call to the variant for the actual object.
*/
{
  a_boolean result = FALSE;

  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      result = ((an_edg_module*)this)->is_open();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      result = ((an_ifc_module*)this)->is_open();
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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

  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      result = ((an_edg_module*)this)->import(midp);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      result = ((an_ifc_module*)this)->import(midp);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      ((an_edg_module*)this)->close();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      ((an_ifc_module*)this)->close();
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
}  /* close */


/*lint -esym(1762,*a_module_interface::pch_reset)*/ /* FIXME: temporary*/
void a_module_interface::pch_reset(a_module_import_decl_ptr midp)
/*
Dispatch the pch_reset() call to the variant for the actual object.
*/
{
  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      ((an_edg_module*)this)->pch_reset(midp);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      ((an_ifc_module*)this)->pch_reset(midp);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
}  /* pch_reset */

#endif /* !USE_VIRTUAL_FUNCTIONS */

void a_module_interface::set_name(a_const_char *module_name)
/*
Set the name of this module to the provided module_name.
*/
{
  a_const_char *name;

  name = get_module_primary_name(module_name);
  primary_name = copy_string_to_region(file_scope_region_number, name);
  name = get_module_partition_name(module_name);
  if (name[0] != '\0') {
    partition_name = copy_string_to_region(file_scope_region_number, name);
  } else {
    partition_name = NULL;
  }  /* if */
}  /* set_name */

#if !USE_VIRTUAL_FUNCTIONS

void a_module_interface::complete_definition_of_module_class(
                                                       a_module_entity_ptr mep)
                                                                          const
/*
Dispatch the complete_definition_of_module_class() call to the variant for the
actual object.
*/
{
  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      ((an_edg_module*)this)->complete_definition_of_module_class(mep);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      ((an_ifc_module*)this)->complete_definition_of_module_class(mep);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
}  /* complete_definition_of_module_class */

#if DEBUG

void a_module_interface::debug() const
/*
Dispatch the debug() call to the variant for the actual object.
*/
{
  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      ((an_edg_module*)this)->debug();
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      ((an_ifc_module*)this)->debug();
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
}  /* debug */


void a_module_interface::db_module_entity(a_module_entity_ptr mep) const
/*
Dispatch the db_module_entity() call to the variant for the actual object.
*/
{
  switch (mod_kind) {
    case mk_none:
      /* This is the actual object. */
      break;
    case mk_edg:
      ((an_edg_module*)this)->db_module_entity(mep);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case mk_ifc:
      ((an_ifc_module*)this)->db_module_entity(mep);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default_is_unexpected();
  }  /* switch */
}  /* db_module_entity */

#endif /* DEBUG */
#endif /* !USE_VIRTUAL_FUNCTIONS */

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
    same_file = (mod->full_name != NULL &&
                 strcmp(ptr->module_info->full_name, mod->full_name) == 0);
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
  if (find_module_file(midp->module_info, (a_module_kind)mk_any)) {
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
    if (find_module_file(midp->module_info, (a_module_kind)mk_any)) {
      import_module_file(midp);
    }  /* if */
    /* Add this to the list of imported modules regardless of whether the
       import was successful - future attempts to import the same module
       aren't likely to succeed if this one failed. */
    midp->next = il_header.imported_modules;
    il_header.imported_modules = midp;
  }  /* if */
}  /* import_module */

a_boolean load_routine_definition_from_module(ARG_UNUSED a_routine_ptr  rp)
/*
If the given routine has a definition available in an imported module, load and
process that definition now, and return TRUE.  Otherwise, return FALSE.
*/
{
  a_boolean  result = FALSE;

#if MICROSOFT_EXTENSIONS_ALLOWED
  result = load_routine_definition_from_ifc_module(rp);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* load_routine_definition_from_module */

#if DEBUG

void db_module(a_module_ptr mod)
/*
Display debug information about the specified module.
*/
{
  if (mod != NULL) {
    (void)fprintf(f_debug, "Module name: %s ",
                  (mod->name == NULL) ? "<NULL>" : mod->name);
    if (mod->module_interface == NULL) {
      (void)fprintf(f_debug, "NULL interface");
    } else {
      mod->module_interface->debug();
    }  /* if */
  }  /* if */
}  /* db_module */


/*lint -esym(714,*db_module_entity)*/
void db_module_entity(a_module_entity_ptr mep)
/*
Display information about a module entity.
*/
{
  (void)fprintf(f_debug, "module \"%s\"", mep->module_info->name);
  if (mep->module_info->module_interface != NULL) {
    mep->module_info->module_interface->db_module_entity(mep);
  } else {
    (void)fprintf(f_debug, "\n");
  }  /* if */
  if (mep->entity.ptr != NULL) {
    db_entity_info(mep->entity.ptr, (an_il_entry_kind)mep->entity.kind);
  }  /* if */
}  /* db_module_entity */

#endif /* DEBUG */

void modules_pch_reset(void)
/*
Called when a PCH file has just been read to re-open any module files that
had been opened at the time the PCH file was created.
*/
{
  a_module_import_decl_ptr midp;

  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    if (midp->module_info->module_interface != NULL) {
      midp->module_info->module_interface->pch_reset(midp);
    }  /* if */
  }  /* for */
}  /* modules_pch_reset */

#if MAKE_FRONT_END_CALLABLE

void modules_cleanup()
/*
Called to ensure files are closed after a (possibly aborted) compilation.
*/
{
  modules_wrapup();
}  /* modules_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

void modules_wrapup(void)
/*
Close any open module files.  Invoked at the end of primary and secondary
translation units.
*/
{
  a_module_import_decl_ptr midp;

  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    if (midp->module_info->module_interface != NULL) {
      midp->module_info->module_interface->close();
    }  /* if */
  }  /* for */
  il_header.imported_modules = NULL;
}  /* modules_wrapup */


void modules_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
  module_search_buffer = alloc_text_buffer(256);
  module_file_name_buffer = alloc_text_buffer(64);
  module_primary_name_buffer = alloc_text_buffer(64);
  module_partition_name_buffer = alloc_text_buffer(64);
#if MICROSOFT_EXTENSIONS_ALLOWED
  ifc_modules_one_time_init();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* modules_one_time_init */


/* FIXME: PCH interactions? */
void modules_init(void)
/*
Initialize static variables related to this file that must be initialized
for each compilation.
*/
{
  curr_module_sym = NULL;
  module_entity_hash_table = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  ifc_modules_init();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* modules_init */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
