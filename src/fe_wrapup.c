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
#include "trans_copy.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#include "lower_name.h"
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
(primary or secondary).  This is called after all the source code
for the translation unit has been read, but before any templates
are instantiated.
*/
{
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

  if (!do_preprocessing_only && any_cfront_mode()) {
    /* Determine whether any classes defined in this file require external
       linkage, and if so do the appropriate fixup.  No such fixup is
       required in non-cfront mode, since the initial linkage settings
       are already external, when appropriate. */
    check_class_linkage();
  }  /* if */

  /* Pop and repush the file scope.  This is done to move the symbols from
     the active list to the inactive list. */
  pop_scope();
  push_file_scope(/*is_reactivation=*/TRUE);

  /* If this is a secondary translation unit, establish any IL
     correspondences.  (If there were errors, the IL may be too
     damaged for reasonable results.) */
  if (!is_primary_translation_unit && !do_preprocessing_only &&
      total_errors == 0) {
    set_trans_unit_correspondences();
  }  /* if */

#if MODULE_ID_NEEDED
  /* Make sure the module id is generated for this translation unit.  This
     must be done after all non-templates have been compiled (because the
     module id uses the name of an external symbol sometimes), and before
     finalize_instantiation_wrapup is called (because the mangled names
     for some members of unnamed namespaces may have to be determined,
     and the mangled name for an unnamed namespace uses the module id). */
  (void)make_module_id();
#endif /* MODULE_ID_NEEDED */

  if (!is_primary_translation_unit && !do_preprocessing_only) {
    /* Check for the presence of a master instance established in a prior
       translation unit. */
    set_master_instance_information();
  }  /* if */

  db_exit();
}  /* translation_unit_wrapup */


static void file_scope_il_wrapup_part_1(void)
/*
Do the processing required to complete the file scope IL.  This is
called both for secondary translation units (is_primary_translation_unit
is FALSE) and for primary translation units (is_primary_translation_unit
is TRUE).  "Part 1" deals with processing and diagnostics on what's actually
in the translation unit, e.g., checks for unreferenced symbols.
When the primary translation unit is processed here, code from any
secondary translation units will not have been copied over already.
Code should be executed here instead of translation_unit_wrapup if
it needs to be executed after all templates have been instantiated.
*/
{
  a_scope_ptr	il_scope;

  il_scope = curr_translation_unit->primary_scope;

  if (is_primary_translation_unit && !do_preprocessing_only) {
    if (any_cfront_mode()) {
      /* Repeat the class linkage check that was first done during translation
         unit wrapup.  This is done again to catch any classes that may have
         been added during the instantiation process. */
      check_class_linkage();
    }  /* if */
  }  /* if */

  /* Do the wrapup_scope processing on file and namespace scopes. */
  wrapup_scope(il_scope, (a_scope_kind)sck_file,
               &curr_translation_unit->file_scope_pointers_block,
               /*is_namespace_wrapup=*/TRUE);
  wrapup_namespace_scopes(il_scope);

  if (!C_mode()) {
    /* Go through the fixup list for based-type entries and remove entities
       as required.  This must happen before the call to mark_secondary_-
       trans_unit_IL_entities_used_from_primary_as_needed to ensure that
       based type list entries in the primary translation unit pointing to
       types in secondary translation units are removed. */
    do_based_type_fixup();
  }  /* if */
}  /* file_scope_il_wrapup_part_1 */


static void file_scope_il_wrapup_needed_flag_processing(void)
/*
Do the needed-flag processing for the current translation unit
(primary or secondary).  The current translation unit is swept to
mark external entities and the things they reference as "needed".
*/
{
#if MAINTAIN_NEEDED_FLAGS
  /* Set the "needed" flag in defined variables with external linkage --
     both in the file scope and in each of the namespace scopes. */
  set_needed_flags_at_end_of_file_scope(curr_translation_unit->primary_scope);
#endif /* MAINTAIN_NEEDED_FLAGS */
#if DO_IL_LOWERING
  /* Any statics referenced from instantiation slices in
     one-instantiation-per-object mode must be made external so that
     they can be referenced from the instantiation object files.
     Likewise for statics referenced from exported templates. */
  /* Note: this needs to be done after the call of
     set_needed_flags_at_end_of_file_scope, so that statics referenced
     from instantiations are marked before we have to decide whether
     they need to be externalized. */
#if MAINTAIN_NEEDED_FLAGS
  end_of_file_scope_needed_flags_phase = TRUE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  make_statics_referenced_from_instantiations_external();
#if MAINTAIN_NEEDED_FLAGS
  end_of_file_scope_needed_flags_phase = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* DO_IL_LOWERING */
}  /* file_scope_il_wrapup_needed_flag_processing */


static void file_scope_il_wrapup_keep_in_il_processing(void)
/*
Do the needed-flag keep-in-il processing for the current translation unit
(primary or secondary).
*/
{
#if MAINTAIN_NEEDED_FLAGS
  a_scope_ptr il_scope = curr_translation_unit->primary_scope;

  if (total_errors == 0) {
    /* Set the "keep_in_il" flag for all file-scope IL entries that must
       be kept to maintain the integrity of the IL. */
    end_of_file_scope_needed_flags_phase = TRUE;
    mark_to_keep_in_il((char *)il_scope, (an_il_entry_kind)iek_scope);
    end_of_file_scope_needed_flags_phase = FALSE;
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* file_scope_il_wrapup_keep_in_il_processing */


static void file_scope_il_wrapup_remove_unneeded_il(void)
/*
Do removal of unneeded IL entities for the current translation unit
(primary or secondary).
*/
{
#if MAINTAIN_NEEDED_FLAGS
  a_scope_ptr il_scope = curr_translation_unit->primary_scope;

  /* Don't bother pruning the IL of unneeded entries if errors were seen. */
  if (total_errors != 0) okay_to_eliminate_unneeded_il_entries = FALSE;
  if (okay_to_eliminate_unneeded_il_entries) {
    /* Eliminate unneeded function bodies.  Note that the function
       declarations are not removed at this point. */
    eliminate_bodies_of_unneeded_functions();
    /* Now eliminate everything at file and namespace scope that does not
       need to be kept in the IL. */
    eliminate_unneeded_il_entries(il_scope);
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* file_scope_il_wrapup_remove_unneeded_il */


static void file_scope_il_wrapup_part_2(void)
/*
Do more wrapup processing on a translation unit.  A single call of
this routine handles all translation units.  "Part 2" does needed
flag processing and unneeded IL removal for secondary translation units.
*/
{
#if MAINTAIN_NEEDED_FLAGS
  a_translation_unit_ptr tup;

  /* The processing has three parts:
       1)  Needed-flag marking
       2)  Keep-in-il flag marking
       3)  Removal of unneeded entities
     Each one of those parts is done for all secondary translation
     units before the next part is done, because the secondary translation
     unit IL is all intermingled.  You can't remove entities in one
     secondary translation unit until you have accounted for references
     from all other secondary translation units. */
  if (total_errors == 0) {
    /* Sweep the primary translation unit IL tree and look for any
       pointers to entities in secondary translation units that it uses,
       and mark those entities as needed. */
    mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed();
  }  /* if */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
#if DO_IL_LOWERING
    if (il_lowering_needed()) {
      /* Do name mangling for the entities in the secondary translation
         unit.  This has to be done before the names for statics referenced
         from templates are externalized. */
      do_all_name_mangling();
    }  /* if */
#endif /* DO_IL_LOWERING */
    file_scope_il_wrapup_needed_flag_processing();
  }  /* for */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_keep_in_il_processing();
  }  /* for */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_remove_unneeded_il();
  }  /* for */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* file_scope_il_wrapup_part_2 */


static void file_scope_il_wrapup_part_3(void)
/*
Do the final wrapup processing on a translation unit.  This is
called both for secondary translation units (is_primary_translation_unit
is FALSE) and for primary translation units (is_primary_translation_unit
is TRUE).  "Part 3" does IL lowering and needed flag processing for
the primary translation unit, and pops the file scope for both primary
and secondary translation units.  When the primary translation unit is
processed here, code from any secondary translation units will have
already been copied over.
*/
{
  if (is_primary_translation_unit) {
#if DO_IL_LOWERING
    /* Lower the file scope. */
    lower_il_memory_region(file_scope_region_number);
#endif /* DO_IL_LOWERING */
  }  /* if */

  /* Clear out the shareable constants table for the file scope. */
  empty_shareable_constants_table();

  if (is_primary_translation_unit && !C_mode()) {
    /* Pop the file scope object lifetime.  This must be done after IL
       lowering. */
    check_assertion(curr_object_lifetime ==
                    scope_stack[depth_scope_stack].curr_scope_object_lifetime);
    (void)pop_object_lifetime();
#if DO_IL_LOWERING
    if (il_lowering_needed()) {
      /* If we're not supposed to pass object lifetime information to the back
         end, unlink all object lifetimes from the IL tree.  This has to
         be done after the file scope object lifetime has been popped. */
      clean_up_all_object_lifetimes(curr_translation_unit->primary_scope);
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
  /* Pop the file scope. */
  pop_scope();
  if (is_primary_translation_unit) {
    /* Do needed-flag processing for the primary translation unit.
       The needed-flag processing for secondary translation units
       was done in part 2. */
    file_scope_il_wrapup_needed_flag_processing();
    file_scope_il_wrapup_keep_in_il_processing();
    /* Do removal of unneeded IL entities for the primary translation
       unit.  That was done for secondary translation units in part 3. */
    file_scope_il_wrapup_remove_unneeded_il();
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
    /* Do any special processing needed to wrapup the automatic instantiation
       process at the end of the compilation. */
    wrapup_auto_instantiation_information();
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#if DO_IL_LOWERING
    if (il_lowering_needed()) {
      /* Clear class/namespace membership information and the
         is_local_to_function flag on entities promoted out of classes,
         namespaces, and functions.  This must be done very late, after
         the generation of instantiation flags, because after parents are
         cleared it becomes impossible to generate mangled names. */
      clear_parent_information();
    }  /* if */
#endif /* DO_IL_LOWERING */
    check_for_done_with_memory_region(file_scope_region_number);
  }  /* if */
}  /* file_scope_il_wrapup_part_3 */


static void wrap_up_file_scopes(void)
/*
Complete the file scope of each of the translation units.
*/
{
  a_translation_unit_ptr	tup;

  /* Do the initial wrapup processing for each of the secondary and
     primary translation units.  This includes all of the processing except
     for copying IL from secondary translation units, and popping of the
     file scope.  */
  tup = translation_units->next;
  for (; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_part_1();
  }  /* for */
  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Process the primary translation unit. */
  file_scope_il_wrapup_part_1();
  /* Do more wrapup processing for each of the secondary and primary
     translation units.  This call processes all the translation units. */
  file_scope_il_wrapup_part_2();
  /* Do the final wrapup processing for each of the secondary and primary
     translation units. */
  tup = translation_units->next;
  for (; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
    file_scope_il_wrapup_part_3();
  }  /* for */
  /* Copy IL from the secondary translation units to the primary IL.
     This must be done before the lowering of the primary IL.
     In trans_unit_test mode, we don't check for duplicate definitions,
     so we can't do the copy. */
  if (total_errors == 0 && !trans_unit_test_mode &&
      translation_units->next != NULL) {
    copy_secondary_trans_unit_IL_to_primary();
  }  /* if */
  /* Switch back to the primary translation unit. */
  switch_translation_unit(translation_units);
  /* Process the primary translation unit. */
  file_scope_il_wrapup_part_3();
  /* Free the secondary IL file-scope memory regions. */
  if (translation_units->next != NULL) {
    a_memory_region_number n;
    for (n = FILE_SCOPE_REGION_NUMBER + 1;
         n <= highest_used_region_number;
         n++) {
      if (mem_region_table[n] != NULL &&
          il_header.region_scope_entry[n]->kind == (a_scope_kind)sck_file) {
          free_memory_region(n);
      }  /* if */
    }  /* for */
  }
}  /* wrap_up_file_scopes */


static void template_and_inline_function_wrapup(void)
/*
For each translation unit, call instantiation_wrapup to generate any
instantiations needed by that translation unit; generate any virtual
destructors that may be required; and determine which extern inline
functions require definitions in this translation unit.
*/
{
  a_translation_unit_ptr	tup;

  /* Push the primary translation unit.  This should be the first entry
     on the stack. */
  check_assertion(curr_translation_unit_stack_entry == NULL);
  push_translation_unit_stack(translation_units);
  /* Do one-time processing (not per-translation unit) for instantiation
     wrapup. */
  instantiation_wrapup_setup();
  for (tup = translation_units; tup != NULL; tup = tup->next) {
    /* Push the translation unit (but don't repush the primary translation
       unit. */
    if (tup != translation_units) push_translation_unit_stack(tup);
    /* Do any template instantiation that may be required.  This is called
       first because it may generate additional function bodies and class
       definitions that need to be processed by the operations that follow. */
    instantiation_wrapup();

    if (C_dialect == C_dialect_cplusplus) {
      /* Go through the classes in the file scope and each namespace scope
         and generate bodies for virtual destructors, as required. */
      generate_required_virtual_destructor_bodies(il_header.primary_scope);
      /* Determine which extern inline functions should have bodies emitted
         as part of this translation unit. */
      inline_function_wrapup();
    }  /* if */
    /* Pop the translation unit if pushed above. */
    if (tup != translation_units) pop_translation_unit_stack();
  }  /* for */
  /* Pop the primary translation unit off of the stack. */
  pop_translation_unit_stack();
  /* Do processing that is required after instantiation wrapup has been
     performed for all translation units. */
  finalize_instantiation_wrapup();
}  /* template_and_inline_function_wrapup */


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

  /* For each translation unit, generate any instantiations that are
     needed, and determine which inline functions require definitions. */
  template_and_inline_function_wrapup();

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

  /* Don't keep checking the stop token stack in db_enter/db_exit because
     the storage goes away when the front end memory region is freed. */
  curr_stop_token_stack_entry = NULL;
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
  curr_translation_unit = NULL;

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
