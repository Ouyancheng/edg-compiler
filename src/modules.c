/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

modules.c -- Module handling classes and routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


static a_boolean same_module_name(a_symbol_ptr module_name,
                                  a_symbol_ptr other_module_name)
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
                                           a_source_position_ptr module_pos)
/*
Determine whether the given module has an interface dependency on the module
referred to by interface_sym.  Return TRUE if there exists an interface
dependency, FALSE otherwise.
*/
{
  a_boolean result = FALSE;

  /* The easy case of a module importing itself. */
  if (interface_sym != NULL &&
      same_module_name(module_sym->variant.module_info.primary_name,
                       interface_sym->variant.module_info.primary_name) &&
      same_module_name(module_sym->variant.module_info.partition_name,
                       interface_sym->variant.module_info.partition_name)) {
    pos_error(ec_module_cannot_depend_on_itself, module_pos);
    result = TRUE;
  }
  /* FIXME: Recurse over interface_sym's interface dependencies. */
  return result;
}  /* check_module_has_interface_dependency */


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
Returns a pointer to the module entity pointer for the entity at
file_offset in the specified module.
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
    (*p)->entity.kind = iek_none;
    (*p)->file_offset = file_offset;
  }  /* if */
  return *p;
}  /* get_module_entity_ptr */

#if DEBUG

void db_module(a_module_ptr mod)
/*
Display debug information about the specified module.
*/
{
  if (mod != NULL) {
    (void)fprintf(f_debug, "Module name: %s ",
                  (mod->name == NULL) ? "<NULL>" : mod->name);
    switch (mod->kind) {
      case mk_none:
        (void)fprintf(f_debug, "kind: mk_none\n");
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
}  /* db_module */


void db_module_entity(a_module_entity_ptr mep)
/*
Display information about a module entity.
*/
{
  (void)fprintf(f_debug, "module \"%s\"", mep->module_info->name);
  switch (mep->module_info->kind) {
    default:
      (void)fprintf(f_debug, "\n");
      break;
  }  /* switch */
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
    switch (midp->module_info->kind) {
      default:
        unexpected_condition();
    }  /* switch */
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
Closes any open module files.  Invoked at the end of primary and secondary
translation units.
*/
{
  a_module_import_decl_ptr midp;

  for (midp = il_header.imported_modules; midp != NULL; midp = midp->next) {
    switch (midp->module_info->kind) {
      case mk_none:
        /* No error if module kind was never determined. */
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* for */
  il_header.imported_modules = NULL;
}  /* modules_wrapup */


void modules_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
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
}  /* modules_init */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2020-2020 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
