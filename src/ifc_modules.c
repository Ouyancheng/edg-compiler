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

ifc_modules.c -- IFC reading & writing common code.

*/

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "ifc_modules.h"
#include "ifc_map_functions.h"
#include "ifc_modules_internal.h"
#include "pch.h"

#if !STANDALONE_UTILITY_PROGRAM

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

STATIC_THREAD a_boolean
        ifc_modules_initialized_for_curr_tu;
                /* TRUE if IFC modules-related variables have been fully
                   initialized for the current translation unit. */


void ifc_modules_one_time_init()
/*
Do one-time initialization of static variables defined in this file.
*/
{
  /* Save variables from ifc_modules.h and ifc_modules.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(ifc_modules_initialized_for_curr_tu),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that have distinct copies for distinct translation
     units. */
  register_trans_unit_variable(ifc_modules_initialized_for_curr_tu);
  ifc_modules_read_one_time_init();
}  /* ifc_modules_one_time_init */


void require_ifc_modules()
/*
IFC module use has been detected in the current translation unit.  Initialize
the corresponding front end structures (if not already initialized).
*/
{
  if (!ifc_modules_initialized_for_curr_tu) {
    ifc_modules_read_trans_unit_delayed_init();
    ifc_modules_initialized_for_curr_tu = TRUE;
  }  /* if */
}  /* require_ifc_modules */


void ifc_modules_trans_unit_init()
/*
Initialize the variables necessary for using IFC modules in the current the
translation unit.
*/
{
  ifc_modules_initialized_for_curr_tu = FALSE;
  ifc_modules_read_trans_unit_init();
}  /* ifc_modules_trans_unit_init */


void ifc_modules_trans_unit_wrapup()
/*
Perform any wrapup operations needed for the translation unit.  This is
called after all processing for the translation unit (including template
instantiations, etc.) has been done.
*/
{
  ifc_modules_read_trans_unit_wrapup();
}  /* ifc_modules_trans_unit_wrapup */

#if MAKE_FRONT_END_CALLABLE

void ifc_modules_cleanup()
/*
This routine is called at the end of compilation, or if compilation is
terminated prematurely for some reason.
*/
{
  ifc_modules_read_cleanup();
}  /* ifc_modules_cleanup */

#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2023 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
