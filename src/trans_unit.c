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

trans_unit.c -- Translation unit management routines.

*/

#include "basic_hdrs.h"

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "fe_init.h"
#include "fe_wrapup.h"

/*
Structure used to keep track of variables that are specific to a
given translation unit.

Variables are registered during one-time initialization.  When a
translation unit is started, space is allocated for all of the
registered variables.  When switching from one translation unit to
another, the registered variables are saved into the memory block
for that old translation unit and the values associated with the
new translation unit are copied into the registered variables.
*/
typedef struct a_variable_registration *a_variable_registration_ptr;
typedef struct a_variable_registration {
  a_variable_registration_ptr
		next;
			/* Pointer to the next entry on a list of
			   registered variables, or NULL for the last entry. */
  a_void_ptr	ptr;
			/* Pointer to the global variable to be saved and
			   restored. */
  sizeof_t	size;
			/* Size of the variable. */
  sizeof_t	offset;
			/* The location in the variables block at which this
			   variable is stored. */
} a_variable_registration;


static a_translation_unit_stack_entry_ptr
		avail_translation_unit_stack_entries;
			/* List of translation unit stack entries that have
			   been freed and are available for reuse. */

static a_variable_registration_ptr
		trans_unit_variables;
			/* Pointer to a list of variable registrations for
			   variables that are local to a given translation
			   unit. */

static a_variable_registration_ptr
		trans_unit_variables_tail;
			/* Pointer to the last entry on the list of variables
			   that are local to a given translation unit. */

static sizeof_t	trans_unit_var_block_size;
			/* Size of the memory block used to store variables
			   that are specific to a given translation unit. */

static a_translation_unit_ptr
		translation_units_tail;
			/* Pointer to the end of the list of translation
			   units. */

#if CHECKING
static a_boolean
		any_translation_units_allocated;
			/* Set to TRUE once the first translation unit
			   entry has been allocated.  Variable registrations
			   are not permitted after this point. */

static a_boolean
		any_exported_template_files_loaded;
			/* Set to TRUE once the first translation unit is
			   loaded for the purpose of defining an exported
			   template. */
#endif /* CHECKING */

#if DEBUG
static unsigned long
		num_translation_unit_stack_entries_allocated,
		num_translation_units_allocated,
		num_variable_registrations_allocated;
#endif /* DEBUG */


static
a_translation_unit_stack_entry_ptr alloc_translation_unit_stack_entry(void)
/*
Allocate a translation unit stack entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  if (avail_translation_unit_stack_entries != NULL) {
    /* Reuse an existing entry. */
    tusep = avail_translation_unit_stack_entries;
    avail_translation_unit_stack_entries = tusep->next;
  } else {
    /* Allocate a new entry. */
    tusep = alloc_general_of_type(a_translation_unit_stack_entry);
#if DEBUG
    num_translation_unit_stack_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  tusep->next = NULL;
  tusep->translation_unit = NULL;  
  return tusep;
}  /* alloc_translation_unit_stack_entry */


static a_variable_registration_ptr alloc_variable_registration(void)
/*
Allocate a variable registration entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_variable_registration_ptr	vrp;

  vrp = alloc_general_of_type(a_variable_registration);
#if DEBUG
  num_variable_registrations_allocated++;
#endif /* DEBUG */
  vrp->next = NULL;
  vrp->ptr = NULL;
  vrp->size = 0;
  vrp->offset = 0;
  return vrp;
}  /* alloc_variable_registration */


void f_register_trans_unit_variable(a_void_ptr	var,
				    sizeof_t	size)
/*
Register a variable that is specific to a given translation unit.
*/
{
  a_variable_registration_ptr	vrp;

  check_assertion_str2(!any_translation_units_allocated,
                       "f_register_trans_unit_variable:",
                       "registration too late");
  check_assertion_str2(var != NULL,
                       "f_register_trans_unit_variable:",
                       "NULL variable pointer");
#if EXPENSIVE_CHECKING
  {
    /* Make sure this variable is not already registered. */
    for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
      check_assertion_str2(vrp->ptr != var, "f_register_trans_unit_variable:",
                           "duplicate registration");
    }  /* for */
  }
#endif /* EXPENSIVE_CHECKING */
  vrp = alloc_variable_registration();
  vrp->ptr = var;
  vrp->size = size;
  vrp->offset = trans_unit_var_block_size;
  if (trans_unit_variables == NULL) trans_unit_variables = vrp;
  if (trans_unit_variables_tail != NULL) {
    trans_unit_variables_tail->next = vrp;
  }  /* if */
  trans_unit_variables_tail = vrp;
  /* Round the size up to the nearest increment of HOST_ALIGNMENT_REQUIRED. */
  do_host_alignment(size);
  /* Increment the size of the block required to store the translation unit
     variables. */
  trans_unit_var_block_size += size;
}  /* f_register_trans_unit_variable */


static void clear_scope_stack_related_information(void)
/*
This routine is used when saving the translation unit state.  It clears the
depth_in_scope_stack field of any scopes on the scope stack.
*/
{
  a_scope_stack_entry_ptr  ssep;

  /* Clear the information about using-directives in scopes on the scope
     stack. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/FALSE,
                                     NO_DECL_SEQUENCE_NUMBER);
  for (ssep = &scope_stack[depth_scope_stack]; ssep != NULL;
       ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
    a_scope_ptr	scope = ssep->il_scope;
    if (scope != NULL) {
      scope->depth_in_scope_stack = NO_SCOPE_DEPTH;
    }  /* if */
  }  /* for */
}  /* clear_scope_stack_related_information */


static void set_scope_stack_related_information(void)
/*
This routine is used when restoring the translation unit state.  It sets the
depth_in_scope_stack field of any scopes on the scope stack that have
associated IL scopes.
*/
{
  a_scope_stack_entry_ptr  ssep;

  for (ssep = &scope_stack[depth_scope_stack]; ssep != NULL;
       ssep = ssep->kind == (a_scope_kind)sck_file ? NULL : ssep - 1) {
    a_scope_ptr	scope = ssep->il_scope;
    /* Note that if a scope is on the stack more than once, this will have
       the effect of setting it to the outermost scope depth. */
    if (scope != NULL) {
      scope->depth_in_scope_stack = scope_depth_of(ssep);
    }  /* if */
  }  /* for */
  /* Reset the active using list flags to the values. */
  set_active_using_list_scope_depths(depth_scope_stack,
                                     /*set_value=*/TRUE,
                                     get_effective_decl_seq());
}  /* set_scope_stack_related_information */


static void save_translation_unit_state(a_translation_unit_ptr	tup)
/*
Copy the variables for a given translation unit to the variables block
pointed to by the translation unit entry.
*/
{
  a_variable_registration_ptr	vrp;
  a_void_ptr			var_block;

  var_block = tup->variables_block;
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    a_void_ptr	src;
    a_void_ptr	dest;
    src = vrp->ptr;
    dest = (a_void_ptr)(((char*)var_block) + vrp->offset);
    memcpy(dest, src, size_t_arg(vrp->size));
  }  /* for */
  /* Save several per-translation-unit fields of il_header. */
  tup->il_header.main_routine = il_header.main_routine;
#if RECORD_MACROS_IN_IL
  tup->il_header.macros = il_header.macros;
#endif /* RECORD_MACROS_IN_IL */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  tup->il_header.scope_orphaned_list_headers =
                                         il_header.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  tup->il_header.nontag_types_used_in_exception_or_rtti =
                              il_header.nontag_types_used_in_exception_or_rtti;
  /* Reset the depth_in_scope stack field of any scopes on the scope stack. */
  if (depth_scope_stack != NO_SCOPE_DEPTH) {
    clear_scope_stack_related_information();
  }  /* if */
}  /* save_translation_unit_state */


static void restore_translation_unit_state(a_translation_unit_ptr	tup)
/*
Copy the variables for a given translation unit from the variables block
pointed to by the translation unit entry.
*/
{
  a_variable_registration_ptr	vrp;
  a_void_ptr			var_block;

  var_block = tup->variables_block;
  for (vrp = trans_unit_variables; vrp != NULL; vrp = vrp->next) {
    a_void_ptr	src;
    a_void_ptr	dest;
    dest = vrp->ptr;
    src = (a_void_ptr)(((char*)var_block) + vrp->offset);
    memcpy(dest, src, size_t_arg(vrp->size));
  }  /* for */
  /* Restore several per-translation-unit fields of il_header. */
  il_header.primary_scope = tup->primary_scope;
  il_header.main_routine = tup->il_header.main_routine;
#if RECORD_MACROS_IN_IL
  il_header.macros = tup->il_header.macros;
#endif /* RECORD_MACROS_IN_IL */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  il_header.scope_orphaned_list_headers =
                                    tup->il_header.scope_orphaned_list_headers;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  il_header.nontag_types_used_in_exception_or_rtti =
                         tup->il_header.nontag_types_used_in_exception_or_rtti;
  /* Restore the depth_in_scope stack field of any scopes on the scope
     stack. */
  if (depth_scope_stack != NO_SCOPE_DEPTH) {
    set_scope_stack_related_information();
  }  /* if */
}  /* restore_translation_unit_state */

#if DEBUG

void db_translation_unit(a_translation_unit_ptr	tup)
/*
Display a translation unit, for debugging purposes.
*/
{
  fprintf(f_debug, "Translation unit %s\n", tup->source_file->file_name);
}  /* db_translation_unit */


void db_translation_unit_stack(void)
/*
Display the translation unit stack, for debugging purposes.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;
  int					count = 0;

  fprintf(f_debug, "Translation unit stack:\n");
  for (tusep = curr_translation_unit_stack_entry;
       tusep != NULL; tusep = tusep->next, count++) {
    fprintf(f_debug, "  %d: %s\n", count,
            tusep->translation_unit->source_file->file_name);
  }  /* for */
}  /* db_translation_unit_stack */
#endif /* DEBUG */

void switch_translation_unit(a_translation_unit_ptr	tup)
/*
Make the translation unit specified by "tup" the current translation unit.
*/
{
  check_assertion(curr_translation_unit != NULL);
  if (tup != curr_translation_unit) {
    /* Only switch if the current translation unit is not the one desired. */
    save_translation_unit_state(curr_translation_unit);
    restore_translation_unit_state(tup);
    curr_translation_unit = tup;
  }  /* if */
}  /* switch_translation_unit */


void push_translation_unit_stack(a_translation_unit_ptr	tup)
/*
Add an entry for "tup" to the top of the translation unit stack, and make
it the current translation unit.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  check_assertion(curr_translation_unit_stack_entry == NULL ||
                  curr_translation_unit_stack_entry->translation_unit ==
                  curr_translation_unit);
  tusep = alloc_translation_unit_stack_entry();
  tusep->next = curr_translation_unit_stack_entry;
  tusep->translation_unit = tup;
  if (curr_translation_unit_stack_entry != NULL) {
    /* Make the new translation unit the currently active one.  This should
       not be done when pushing the primary translation unit. */
    switch_translation_unit(tup);
  }  /* if */
  /* If this is a secondary translation unit, increment the count of
     secondary translation units on the stack. */
  if (tup != translation_units) secondary_trans_units_on_stack++;
  curr_translation_unit_stack_entry = tusep;
}  /* push_translation_unit_stack */


void pop_translation_unit_stack(void)
/*
Remove the top entry from the translation unit stack and make the
new top entry the current translation unit.
*/
{
  a_translation_unit_stack_entry_ptr	tusep;

  tusep = curr_translation_unit_stack_entry;
  check_assertion(tusep->translation_unit == curr_translation_unit);
  /* If this is a secondary translation unit, decrement the count of
     secondary translation units on the stack. */
  if (tusep->translation_unit != translation_units) {
    secondary_trans_units_on_stack--;
  }  /* if */
  /* Unlink this entry from the stack. */
  curr_translation_unit_stack_entry = tusep->next;
  /* Add the old entry to the list of available stack entries. */
  tusep->next = avail_translation_unit_stack_entries;
  avail_translation_unit_stack_entries = tusep;
  check_assertion(curr_translation_unit_stack_entry != NULL);
  /* Make the translation unit specified by the top of the stack into the
     active translation unit. */
  switch_translation_unit(curr_translation_unit_stack_entry->translation_unit);
}  /* push_translation_unit_stack */


static a_translation_unit_ptr alloc_translation_unit(void)
/*
Allocate a translation unit entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_translation_unit_ptr	tup;

#if CHECKING
  any_translation_units_allocated = TRUE;
#endif /* CHECKING */
  tup = alloc_fe_of_type(a_translation_unit);
#if DEBUG
  num_translation_units_allocated++;
#endif /* DEBUG */
  tup->next = NULL;
  /* Allocate the variable block for this translation unit. */
  tup->variables_block = alloc_fe(trans_unit_var_block_size);
  tup->primary_scope = NULL;
  clear_scope_pointers_block(&tup->file_scope_pointers_block);
  tup->source_file = NULL;
  memzero((char *)&tup->il_header, sizeof(an_il_header));
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
   tup->last_scope_orphaned_list_header = NULL;
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if RECORD_MACROS_IN_IL
  tup->last_macro = NULL;
#endif /* RECORD_MACROS_IN_IL */
  tup->based_type_fixup_list = NULL;
  tup->exported_template_file = NULL;
  return tup;
}  /* alloc_translation_unit */


void process_translation_unit(char				*file_name,
			      a_boolean				is_primary,
			      an_exported_template_file_ptr	exported_file)
/*
This routine processes a translation unit (a source file and any
files included by that source file).  file_name is the name of the
primary source file of the translation unit.  is_primary is TRUE if the
translation unit is the primary translation unit.   If the translation
unit is being processed for the purpose of defining exported templates,
exported_file describes the file to be processed.

There is usually one translation unit per compilation.  When the
COMPILE_MULTIPLE_SOURCE_FILES flag is TRUE, the front end can
perform multiple compilations, each of which will typically contain
one translation unit (but may contain more).  There is more than one
translation unit per compilation when making use of exported templates.
When using exported templates, the translation units containing the
definitions of the exported templates are processed as secondary
translation units.

When COMPILE_MULTIPLE_TRANSLATION_UNITS is TRUE, multiple source files
can be specified on the command-line and the source files are each
treated as separate translation units of a single compilation.
*/
{
  a_translation_unit_ptr	trans_unit;

#if DEBUG
  if (debug_level >= 1 || db_flag_is_set("trans_unit")) {
    fprintf(f_debug, "Processing translation unit %s\n", file_name);
  }  /* if */
#endif  /* DEBUG */
#if CHECKING
  if (!is_primary && exported_file == NULL) {
    /* We can't load a normal secondary translation unit after an exported
       file has been loaded because the command-line macro definition
       information and include search paths will not be correct. */
    check_assertion(!any_exported_template_files_loaded);
  }  /* if */
  if (exported_file != NULL) any_exported_template_files_loaded = TRUE;
#endif /* CHECKING */
  if (curr_translation_unit != NULL) {
    /* Save the currently active set of translation unit specific variables. */
    save_translation_unit_state(curr_translation_unit);
  }  /* if */
  /* Set a current position indicating we are in initialization.  This
     actually does something for a secondary translation unit. */
  pos_curr_token.seq = 0;
  pos_curr_token.column = SP_COL_UNKNOWN;
  set_err_pos_to_curr_token();
  /* Initialize the front end. */
  is_primary_translation_unit = is_primary;
  translation_unit_needed_only_for_exported_templates = exported_file != NULL;
  trans_unit_file_name = file_name;
  compute_il_prefix_size();
  if (is_primary_translation_unit) fe_init_part_1();
  trans_unit = alloc_translation_unit();
  trans_unit->exported_template_file = exported_file;
  /* Add this translation unit to the list of translation units. */
  if (translation_units == NULL) {
    translation_units = trans_unit;
    /* The primary translation unit must be first. */
    check_assertion(is_primary_translation_unit);
  }  /* if */
  if (is_primary) {
    /* Push this translation unit onto the translation unit stack. */
    push_translation_unit_stack(trans_unit);
  }  /* if */
  if (translation_units_tail != NULL) {
    translation_units_tail->next = trans_unit;
  }  /* if */
  translation_units_tail = trans_unit;
  curr_translation_unit = trans_unit;
  if (exported_file != NULL) {
    /* Set the include search path and macro define/undefines to be used for
       this exported template file.  For secondary translation units loaded
       from the command-line, these variables retain the values used for the
       primary translation unit. */
    defs_from_cmd_line = exported_file->define_list;
    undefs_from_cmd_line = exported_file->undefine_list;
    incl_search_path = exported_file->incl_search_path;
    sys_incl_search_path = exported_file->sys_incl_search_path;
  }  /* if */
  fe_translation_unit_init();
#if MODULE_ID_NEEDED
  if (exported_file != NULL) {
    /* When loading a file for the purpose of defining exported templates,
       the module ID must be restored to the value used when the file was
       originally compiled. */
    set_module_id(exported_file->module_id);
  }  /* if */
#endif /* MODULE_ID_NEEDED */
  if (do_preprocessing_only) {
    /* Compiler is to operate like cpp, and do just preprocessing. */
    fe_init_part_2();
    cpp_driver();
  } else {
    /* Compiler is to do preprocessing and compilation. */
    if (precompiled_header_processing_required &&
        !cannot_do_pch_processing) {
      fe_init_for_pch_prefix_scan();
      precompiled_header_processing();
    }  /* if */
    fe_init_part_2();
    translation_unit();
  }  /* if */
  translation_unit_wrapup();
#if DEBUG
  if (debug_level >= 1 || db_flag_is_set("trans_unit")) {
    fprintf(f_debug, "Done processing translation unit %s\n", file_name);
  }  /* if */
#endif  /* DEBUG */
#if COMPILE_MULTIPLE_TRANSLATION_UNITS
  if (is_primary) {
    /* Process any secondary translation units specified on the
       command line. */
    proc_secondary_translation_units();
  }  /* if */
#endif /* COMPILE_MULTIPLE_TRANSLATION_UNITS */
  /* No db_exit because the start and end of this routine are not in
     the same translation unit and that fouls up the stop tokens check. */
}  /* process_translation_unit */

#if DEBUG

unsigned long db_show_trans_unit_space_used(unsigned long grand_total)
/*
Show space used by the trans_unit routines.  This is called by
the symbol table space used routine.  The space used by the trans_unit
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used_general("translation units",
                        num_translation_units_allocated,
                        a_translation_unit);
  db_space_used_general("trans. unit stack entry",
                        num_translation_unit_stack_entries_allocated,
                        a_translation_unit_stack_entry);
  db_space_used_general("variable registration",
                        num_variable_registrations_allocated,
                        a_variable_registration);
  return grand_total;
}  /* db_show_trans_unit_space_used */

#endif /* DEBUG */


void trans_unit_one_time_init(void)
/*
One-time initialization for trans_unit variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_translation_unit),
      pch_saved_var_array_elem(translation_units),
      pch_saved_var_array_elem(translation_units_tail),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  register_trans_unit_variable(is_primary_translation_unit);
  register_trans_unit_variable(trans_unit_file_name);
  register_trans_unit_variable(
                          translation_unit_needed_only_for_exported_templates);
}  /* trans_unit_one_time_init */


void trans_unit_init(void)
/*
The per-compilation unit initialization routine for variables related to
translation unit processing.
*/
{
  curr_translation_unit = NULL;
  translation_units = NULL;
  translation_units_tail = NULL;
  translation_unit_needed_only_for_exported_templates = FALSE;
  curr_translation_unit_stack_entry = NULL;
  secondary_trans_units_on_stack = 0;
}  /* trans_unit_init */


void trans_unit_early_init(void)
/*
One time initialization that must occur early in the execution of the
front end.  This must occur before the one-time initialization routines
of the front end are called.
*/
{
  trans_unit_variables = NULL;
  trans_unit_variables_tail = NULL;
  trans_unit_var_block_size = 0;
  is_primary_translation_unit = FALSE;
  trans_unit_file_name = NULL;
  avail_translation_unit_stack_entries = NULL;
#if DEBUG
  num_translation_unit_stack_entries_allocated = 0;
  num_translation_units_allocated = 0;
  num_variable_registrations_allocated = 0;
#endif /* DEBUG */
#if CHECKING
  any_translation_units_allocated = FALSE;
  any_exported_template_files_loaded = FALSE;
#endif /* CHECKING */
}  /* trans_unit_early_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
