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
#include "trans_corresp.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#if DO_C99_IL_LOWERING
#include "lower_c99.h"
#endif /* DO_C99_IL_LOWERING */
#endif /* DO_IL_LOWERING */
#if DEBUG
#include "exprutil.h"
#include "macro.h"
#include "statements.h"
#endif /* DEBUG */
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */


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
  a_scope_ptr  file_scope;

  db_enter(1, "translation_unit_wrapup");

#if CHECKING
  /* Check that the stop_token_array elements all made it back to zero.
     (Every add_stop_token is supposed to have a corresponding
     remove_stop_token.)  Note that there is also a check in db_exit,
     which can be used to pin down problems that are initially
     spotted here. */
  check_all_stop_token_entries_are_reset(
                                   curr_stop_token_stack_entry->stop_tokens);
#endif /* CHECKING */

  if (is_primary_translation_unit) {
    /* Do any template instantiation that may be required.  This is called
       first because it may generate additional function bodies and class
       definitions that need to be processed by the operations that follow. */
    instantiation_wrapup();
  }  /* if */

  /* Pop the file scope off the scope stack. */
  pop_scope();

  /* If this is a secondary translation unit, establish any IL
     correspondences. */
  file_scope = curr_translation_unit->primary_scope;
  if (il_entry_prefix_of(file_scope).secondary_trans_unit) {
    establish_trans_unit_correspondences_for_scope(file_scope);
    verify_trans_unit_correspondences_for_scope(file_scope);
  }  /* if */

  db_exit();
}  /* translation_unit_wrapup */


static void secondary_trans_unit_file_scope_il_wrapup(void)
/*
Do any processing required to complete the file scope IL of a secondary
translation unit.  This is done when all processing (including any
instantiations) has been completed in the secondary translation unit.
*/
{
  a_scope_ptr	il_scope;

  il_scope = curr_translation_unit->primary_scope;
  /* Reactivate the file scope. */
  push_file_scope(/*is_reactivation=*/TRUE);
  /* Do the wrapup_scope processing on file and namespace scopes. */
  wrapup_scope(il_scope, (a_scope_kind)sck_file,
               &curr_translation_unit->file_scope_pointers_block,
               /*is_namespace_wrapup=*/TRUE);
  wrapup_namespace_scopes(il_scope);
  /* Pop the file scope. */
  pop_scope();
  check_for_done_with_memory_region(file_scope_region_number);
}  /* secondary_trans_unit_file_scope_il_wrapup */


static void file_scope_il_wrapup(void)
/*
Do the processing required to complete the file scope IL.  This is done
after any entries from secondary translation units have been copied to
the primary translation unit IL.
*/
{
  a_scope_ptr	il_scope;

  il_scope = curr_translation_unit->primary_scope;

  /* Reactivate the file scope. */
  push_file_scope(/*is_reactivation=*/TRUE);

  /* Do the wrapup_scope processing on file and namespace scopes. */
  wrapup_scope(il_scope, (a_scope_kind)sck_file,
               &curr_translation_unit->file_scope_pointers_block,
               /*is_namespace_wrapup=*/TRUE);
  wrapup_namespace_scopes(il_scope);

  if (!C_mode()) {
    /* Go through the fixup list for based-type entries and remove entities
       as required. */
    do_based_type_fixup();
  }  /* if */

#if DO_IL_LOWERING
  /* Lower the file scope. */
  lower_il_memory_region(file_scope_region_number);
#if DO_C99_IL_LOWERING
  if (c99_il_lowering_needed()) {
    lower_c99_il_memory_region(il_scope);
  }  /* if */
#endif /* DO_C99_IL_LOWERING */
#endif /* DO_IL_LOWERING */

  /* Clear out the shareable constants table for the file scope. */
  empty_shareable_constants_table();

  if (!C_mode()) {
    /* Pop the file scope object lifetime.  This must be done after IL
       lowering. */
    check_assertion(curr_object_lifetime ==
                    scope_stack[depth_scope_stack].curr_scope_object_lifetime);
    (void)pop_object_lifetime();
  }  /* if */

#if DO_IL_LOWERING
  if (!C_mode()) {
    if (il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the back
         end, unlink all object lifetimes from the IL tree.  This has to
         be done after the file scope object lifetime has been popped. */
      clean_up_all_object_lifetimes(il_scope);
    }  /* if */
  }  /* if */
#endif /* DO_IL_LOWERING */

#if MAINTAIN_NEEDED_FLAGS
  /* Set the "needed" flag in defined variables with external linkage --
     both in the file scope and in each of the namespace scopes. */
  set_needed_flags_at_end_of_file_scope(il_scope);
  /* Don't bother pruning the IL of unneeded entries if errors were seen. */
  if (total_errors != 0) okay_to_eliminate_unneeded_il_entries = FALSE;
  if (okay_to_eliminate_unneeded_il_entries) {
    /* Set the "keep_in_il" flag for all file-scope IL entries that must
       be kept to maintain the integrity of the IL. */
    end_of_file_scope_needed_flags_phase = TRUE;
    mark_to_keep_in_il((char *)il_scope, (an_il_entry_kind)iek_scope);
    end_of_file_scope_needed_flags_phase = FALSE;
    /* Now all IL entries that are really needed are so marked, and other
       entries that they may depend on are also marked, with "keep_in_il"
       set to TRUE.  Everything else can be eliminated from the IL. */
    /* Eliminate unneeded function bodies.  Note that the function
       declarations are not removed at this point. */
    eliminate_bodies_of_unneeded_functions();
    /* Now eliminate everything at file and namespace scope that does not
       need to be kept in the IL. */
    eliminate_unneeded_il_entries(il_scope);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  /* Check for memory regions that were not written out but now should
     be.  Among other things, this deals with functions that have
     keep_definition_in_il set but not definition_needed, and inline
     functions. */
  check_for_done_with_all_function_memory_regions();
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (total_errors == 0) {
    /* Set the IL flags used to pass automatic instantiation information to
       the link-time instantiation processor.  The timing of this call is
       important.  It must follow the call to eliminate_unneeded_il_entries,
       which may clear the instantiation_required flag in the associated
       template instance entry.  And it must precede the call to
       check_for_done_with_memory_region, since it modifies IL entries and
       (if DO_IL_LOWERING is TRUE) may allocate variables that are added to
       the IL. */
    update_auto_instantiation_flags();
    /* Do the similar processing for inline functions, when instantiating
       inline functions similarly to templates. */
    update_inline_function_flags();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  /* Pop the file scope. */
  pop_scope();
  check_for_done_with_memory_region(file_scope_region_number);
}  /* file_scope_il_wrapup */


static void wrap_up_file_scopes(void)
/*
Complete the file scope of each of the translation units.
*/
{
  a_translation_unit_ptr	tup;

  /* Process any secondary translation units. */
  tup = translation_units->next;
  for (; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    secondary_trans_unit_file_scope_il_wrapup();
  }  /* for */
  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Process the primary translation unit. */
  file_scope_il_wrapup();
}  /* wrap_up_file_scopes */


void fe_wrapup(void)
/*
Do any processing required at the end of execution of the front end,
and before the back end (if any) is executed.
*/
{

  db_enter(1, "fe_wrapup");

  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Make sure that we have switched back to processing the primary
     translation unit. */
  check_assertion_str2(is_primary_translation_unit,
                       "fe_wrapup:", "bad translation unit in fe_wrapup");

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

  /* Lower the file scope, remove unneeded entities, etc. */
  wrap_up_file_scopes();

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
