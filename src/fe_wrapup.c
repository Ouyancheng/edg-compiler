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

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "fe_wrapup.h"
#include "class_decl.h"
#include "func_def.h"
#if IL_SHOULD_BE_WRITTEN_TO_FILE
#include "il_write.h"
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#include "templates.h"
#if DEBUG
#include "exprutil.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */
#include "macro.h"
#include "statements.h"
#endif /* DEBUG */


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


void translation_unit_wrapup(void)
/*
Do any processing that is required at the end of a translation unit
(primary or secondary).
*/
{
  db_enter(1, "translation_unit_wrapup");

  if (is_primary_translation_unit) {
    /* Do any template instantiation that may be required.  This is called
       first because it may generate additional function bodies and class
       definitions that need to be processed by the operations that follow. */
    instantiation_wrapup();
  }  /* if */

  db_exit();
}  /* translation_unit_wrapup */


void fe_wrapup(void)
/*
Do any processing required at the end of execution of the front end,
and before the back end (if any) is executed.
*/
{

  db_enter(1, "fe_wrapup");

#if CHECKING
  /* Check that the stop_token_array elements all made it back to zero.
     (Every add_stop_token is supposed to have a corresponding
     remove_stop_token.)  Note that there is also a check in db_exit,
     which can be used to pin down problems that are initially
     spotted here. */
  check_all_stop_token_entries_are_reset(
                                   curr_stop_token_stack_entry->stop_tokens);
#endif /* CHECKING */

  if (C_dialect == C_dialect_cplusplus) {
    if (any_cfront_mode()) {
      /* Determine whether any classes defined in this file require external
         linkage, and if so do the appropriate fixup.  No such fixup is
         required in non-cfront mode, since the initial linkage settings
         are already external, when appropriate. */
      check_class_linkage();
    }  /* if */
    /* Go through the classes in the file scope and each namespace scope
       and generate bodies for virtual destructors, as required. */
    generate_required_virtual_destructor_bodies(il_header.primary_scope);
    /* Determine which extern inline functions should have bodies emitted
       as part of this translation unit. */
    inline_function_wrapup();
  }  /* if */

  /* Pop the file declaration scope off the scope stack. */
  pop_scope();

#if CHECKING
  /* Ensure that unexpected situations did not occur without at least one
     error being issued (otherwise, abort compilation). */
  check_expected_errors();
#endif /* CHECKING */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Finish writing the IL file, if there is one. */
  finish_il_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Close the preprocessing output file, if needed. */
  if (f_pp_output != NULL) {
    /* Check for errors in writing the pp output file, then close it. */
    if (fflush(f_pp_output) || ferror(f_pp_output) ||
        (f_pp_output != stdout && fclose(f_pp_output))) {
      str_catastrophe(ec_file_write_error, "preprocessing output");
    }  /* if */
  }  /* if */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
  /* Do any special processing needed to wrapup the automatic instantiation
     process at the end of the translation unit. */
  wrapup_auto_instantiation_information();
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
  if (debug_level > 0 || db_flag_is_set("space_used")) {
    /* Print total memory used. */
    show_space_used();
  }  /* if */
#endif /* DEBUG */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Free all memory regions.  Anything left in any of the IL memory
     regions should be discarded as it will be reread by the back end. */
  free_all_memory_regions();
#else /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Free front-end-only storage. */
  free_memory_region(NULL_region_number);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

  /* Clear the file index list maintained by the error routines (it was
     allocated in front-end storage). */
  clear_file_index_list();

  in_front_end = FALSE;

  db_exit();
}  /* fe_wrapup */


void fe_wrapup_part_2(void)
/*
Do any processing required at the end of execution of the front end,
and after the back end (if any) is executed.
*/
{
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  /* Close the IL output file, be it a temporary or actual file. */
  close_il_output_file();
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
  /* Free the front end memory region, file-scope IL and all function scope
     IL.  This is necessary if an IL file is not written, or if some regions
     were kept because of inlining, but it's a good idea in all cases.
     Some of the regions may have already been freed.  That is okay because
     freeing a region a second time does nothing. */
  free_all_memory_regions();
  /* Write a signoff message (with count of errors) if necessary. */
  write_signoff();
}  /* fe_wrapup_part_2 */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
