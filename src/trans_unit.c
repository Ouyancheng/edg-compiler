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

#if CHECKING
static a_boolean
		any_translation_units_allocated;
			/* Set to TRUE once the first translation unit
			   entry has been allocated.  Variable registrations
			   are not permitted after this point. */
#endif /* CHECKING */


static a_variable_registration_ptr alloc_variable_registration(void)
/*
Allocate a variable registration entry, initialize its fields, and return
a pointer to the entry created.
*/
{
  a_variable_registration_ptr	vrp;

  vrp = alloc_general_of_type(a_variable_registration);
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
      check_assertion_str2(vrp->ptr != var, "f_register_trans_unit_variables:",
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
    memcpy(dest, src, vrp->size);
  }  /* for */
}  /* save_translation_unit_state */

#if 0

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
    memcpy(dest, src, vrp->size);
  }  /* for */
}  /* restore_translation_unit_state */

#endif /* 0 */


a_translation_unit_ptr alloc_translation_unit(void)
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
  /* Allocate the variable block for this translation unit. */
  tup->variables_block = alloc_fe(trans_unit_var_block_size);
  tup->primary_scope = NULL;
  return tup;
}  /* alloc_translation_unit */


void process_translation_unit(a_boolean	is_primary)
/*
This routine processes a translation unit (a source file and any
files included by that source file).  is_primary is TRUE if the
translation unit is the primary translation unit.

There is usually one translation unit per compilation.  When the
COMPILE_MULTIPLE_SOURCE_FILES flag is TRUE, the front end can
perform multiple compilations, each of which will typically contain
one translation unit (but may contain more).  There is more than one
translation unit per compilation when making use of exported templates.
When using exported templates, the translation units containing the
definitions of the exported templates are processed as secondary
translation units.
*/
{
  a_translation_unit_ptr	trans_unit;

  if (curr_translation_unit != NULL) {
    /* Save the currently active set of translation unit specific variables. */
    save_translation_unit_state(curr_translation_unit);
  }  /* if */
  /* Initialize the front end. */
  is_primary_translation_unit = is_primary;
  if (is_primary_translation_unit) fe_init_part_1();
  trans_unit = alloc_translation_unit();
  curr_translation_unit = trans_unit;
  fe_translation_unit_init();
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
    translation_unit_wrapup();
  }  /* if */
#if 0
#else /* 0 */
  /* Temporary code to process secondary translation units. */
  {
    extern void proc_secondary_translation_units(void);
    proc_secondary_translation_units();
  }
#endif /* 0 */
}  /* process_translation_unit */


void trans_unit_one_time_init(void)
/*
One-time initialization for trans_unit variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(curr_translation_unit),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* trans_unit_one_time_init */


void trans_unit_init(void)
/*
The per-compilation unit initialization routine for variables related to
translation unit processing.
*/
{
  curr_translation_unit = NULL;
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
  /* This will be moved to the translation unit driver. */
  is_primary_translation_unit = TRUE;
#if CHECKING
  any_translation_units_allocated = FALSE;
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
