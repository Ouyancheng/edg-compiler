/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

fe_wrapup.c - End of front end processing.

*/

#include "basics.h"
#include "host_envir.h"
#include "lexical.h"
#include "symbol_tbl.h"
#include "templates.h"
#include "mem_manage.h"
#include "error.h"
#include "cmd_line.h"
#include "macro.h"
#include "class_decl.h"
#include "exprutil.h"
#include "statements.h"

#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */


#if DEBUG
static void show_space_used(void)
/*
Show the amount of memory allocated.
*/
{
  unsigned long total_space = 0;

  /* Show space use in various categories. */
  total_space += show_symbol_space_used();
  total_space += show_macro_space_used();
  total_space += show_lexical_space_used();
  total_space += show_expr_space_used();
  total_space += show_il_space_used();
  total_space += show_statements_space_used();
#if DO_IL_LOWERING
  total_space += show_lowering_space_used();
#endif /* DO_IL_LOWERING */

  show_mem_manage_space_used(total_space);
}  /* show_space_used */
#endif /* DEBUG */


void fe_wrapup(void)
/*
Do any processing required at the end of execution of the front end.
*/
{
  db_enter(1, "fe_wrapup");

  /* Do any template instantiation that may be required.  This is called
     first because it may generate additional function bodies and class
     definitions that need to be processed by the operations that follow. */
  instantiation_wrapup();

#if CHECKING
  /* Check that the stop_token_array elements all made it back to zero.
     (Every add_stop_token is supposed to have a corresponding
     remove_stop_token.)  Note that there is also a check in db_exit,
     which can be used to pin down problems that are initially
     spotted here. */
  { int       token;
    a_boolean any_error = FALSE;

    for (token = 0; token != (int)tok_last; token++) {
      if (stop_token_array[token] != 0) {
        any_error = TRUE;
#if DEBUG
        if (debug_level != 0) {
          fprintf(f_debug, "In fe_wrapup: stop_token_array[\"%s\"] != 0\n",
                           token_names[token]);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
    }  /* for */
    if (any_error) internal_error("fe_wrapup: stop_token_array not all zero");
  }
#endif /* CHECKING */

  if (C_dialect == C_dialect_cplusplus) {
    /* Determine whether any classes defined in this file require external
       linkage, and if so do the appropriate fixup. */
    check_class_linkage();
  }  /* if */

  /* Pop the file declaration scope off the scope stack. */
  pop_scope();

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Finish writing the IL file, if there is one. */
  finish_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
  /* Create or remove the instantiation information file if necessary. */
  if (!do_preprocessing_only && automatic_instantiation_mode) {
    /* When only doing preprocessing we cannot determine whether or not the
       instantiation information file is needed.  By not calling this
       routine we keep the old version if one was present and don't create
       one if one did not already exist. */
    create_or_remove_instantiation_information_file();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */


  /* Close the raw listing file if one is being generated. */
  if (f_raw_listing != NULL) {
    if (fflush(f_raw_listing) || ferror(f_raw_listing) ||
        fclose(f_raw_listing)) {
      str_catastrophe(ec_file_write_error, "raw listing");
    }  /* if */
  }  /* if */

  /* Close the cross-reference file if one is being generated. */
  if (f_xref_info != NULL) {
    if (fflush(f_xref_info) || ferror(f_xref_info) || fclose(f_xref_info)) {
      str_catastrophe(ec_file_write_error, "cross-reference");
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level > 0) {
    /* Print total memory used. */
    show_space_used();
  }  /* if */
#endif /* DEBUG */

  /* Free front-end-only storage. */
  free_memory_region(NULL_region_number);

  /* Clear the file index list maintained by the error routines (it was
     allocated in front-end storage). */
  clear_file_index_list();

  db_exit();
}  /* fe_wrapup */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
