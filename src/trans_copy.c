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

trans_copy.c -- Copy IL from secondary translation units to the
                primary translation unit.

*/

#include "basic_hdrs.h"

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "trans_copy.h"
#include "trans_corresp.h"
#include "il_walk.h"
#include "scope_stk.h"
#include "templates.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#include "lower_name.h"
#endif /* DO_IL_LOWERING */


static a_boolean f_has_corresp(char *ptr)
/*
Return TRUE if the indicated entry has a correspondence in the primary
file IL.
*/
{
  char      *corresp = canonical_il_entry_of(ptr);
  a_boolean has_corr = !in_secondary_trans_unit(corresp);

  return has_corr;
}  /* f_has_corresp */


/*
Macro interface to f_has_corresp, which allows it to be called for
entries of various kinds.
*/
#define has_corresp(entry) f_has_corresp((char *)(entry))


static void f_mark_to_merge(char *ptr)
/*
Mark the given entry as one that must be merged with its counterpart
in the primary IL.
*/
{
  /* Make the correspondence pointer go directly to the canonical
     entry if it is currently a multi-step chain.  This is needed
     to allow adding an intervening step on the chain to indicate
     the location where the entry should be copied (more precisely,
     to make it possible to tell whether the intervening step has
     been added). */
  char *canonical = canonical_il_entry_of(ptr);
  checked_trans_unit_corresp_pointer_of(ptr) = canonical;
  /* The IL lowering flag is borrowed for this process because IL
     lowering is not done on secondary translation units. */
  il_entry_prefix_of(ptr).il_lowering_flag = TRUE;
}  /* f_mark_to_merge */

/*
Macro interface to f_mark_to_merge, which allows it to be called for
entries of various kinds.
*/
#define mark_to_merge(ptr) f_mark_to_merge((char *)(ptr))

/*
Return TRUE if the given entry is to be merged with its counterpart
in the primary IL.
*/
#define entry_to_be_merged(ptr) \
  (il_entry_prefix_of(ptr).il_lowering_flag)


static void corresp_setup(char             *ptr,
                          an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit to set up the correspondence
pointer of the entry pointed to by ptr, of kind "kind".
*/
{
  /* Ensure that the entry pointed to has a correspondence pointer
     that points to an entry in the primary IL.  For entries with
     linkage correspondence, the correspondence pointer is already set.
     For others, space is allocated in the primary IL and the
     correspondence pointer is set to point to it (but no copy
     is done at this time).  Some entries do not have correspondence
     pointers (those in function scope memory regions and those
     in the primary translation unit IL), and for those nothing
     is done.  The il_walk_flag of the source entry is set to
     indicate that further processing (e.g., copying) of the
     entry is required.  For file-scope entries, the il_walk_flag
     is used in an unusual way: it is set to indicate that more
     processing is needed, and then cleared once that processing
     has been done. */
  if (ptr == NULL) {
    /* Ignore NULL pointers. */
  } else if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so do nothing. */
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region, so do nothing. */
  } else if (il_entry_prefix_of(ptr).il_walk_flag ==
                                                  flag_value_meaning_visited) {
    /* This entry has already been encountered and the correspondence
       pointer has been set, and we're awaiting copying. */
  } else if (has_corresp(ptr)) {
    /* This entry has a correspondence in the primary IL. */
    if (entry_to_be_merged(ptr)) {
      /* This is an entry that gets merged into its corresponding entry. */
      char *corresp = checked_trans_unit_corresp_pointer_of(ptr);
      /* The first time through, a copy is made, the subtree is walked,
         and a two-step correspondence-pointer chain is set up.  If the
         chain is already present, this is not the first time through,
         so do nothing. */
      if (!in_secondary_trans_unit(corresp)) {
        /* Make a copy, so we will have a version with all the pointers
           remapped appropriately.  The original entry points to the
           copy, which points to the canonical entry.  This allows us to get
           to the copy via trans_unit_corresp_pointer_of, while ensuring
           that references to the original entry are remapped to the
           canonical entry (because canonical_il_entry_of loops through to
           the end of the list).  Note that the copy is in the
           secondary translation unit file scope memory region. */
        char *copy = alloc_il(sizeof_il_entry[(int)kind]);
        checked_trans_unit_corresp_pointer_of(ptr) = copy;
        checked_trans_unit_corresp_pointer_of(copy) = corresp;
        check_assertion(!is_string_entry_kind(kind));
        /* Set the il_walk_flag to request copying. */
        il_entry_prefix_of(ptr).il_walk_flag = flag_value_meaning_visited;
      }  /* if */
    }  /* if */
  } else {
    /* The entry has no correspondence.  Allocate space for it in the primary
       file scope, and set the correspondence pointer to point to that
       space.  The entry is copied into that space a little later. */
    /* String entries are allocated in copy_string_entry. */
    if (!is_string_entry_kind(kind)) {
      char *copy = alloc_primary_file_scope_il(sizeof_il_entry[(int)kind]);
      char *canonical = canonical_il_entry_of(ptr);
      checked_trans_unit_corresp_pointer_of(ptr) = copy;
      /* Make the canonical entry for this entry point to the copy also if
         it's in a secondary translation unit. */
      if (canonical != ptr && in_secondary_trans_unit(canonical)) {
        checked_trans_unit_corresp_pointer_of(canonical) = copy;
      }  /* if */
      /* Set the il_walk_flag to request copying. */
      il_entry_prefix_of(ptr).il_walk_flag = flag_value_meaning_visited;
      if (!walking_file_scope) {
        /* A reference from a function scope to the file scope.  Make sure
           we come back to this entry if it's an orphan. */
        add_orphaned_file_scope_il_entry(ptr, kind);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* corresp_setup */


/*ARGSUSED*/ /* <-- "kind" is not used. */
static a_boolean copy_termination_test(char             *ptr,
                                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit.  Returns TRUE if the walk should be
pruned at the entry pointed to by ptr, of kind "kind".
*/
{
  a_boolean prune;

  /* Make sure the correspondence pointer, if any, is set. */
  corresp_setup(ptr, kind);
  if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so stop and don't process
       it. */
    prune = TRUE;
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region of a secondary
       translation unit.  Use the secondary_trans_unit flag as
       a "visited" flag; it needs to get cleared anyway.  Using
       il_walk_flag itself is a bad idea because flipping it would
       cause the entries in the function scope memory region to
       have a value different from that in other function scope
       memory regions that weren't moved from secondary translation
       units. */
    il_entry_prefix_of(ptr).secondary_trans_unit = FALSE;
    if (kind == iek_scope) {
      a_scope_ptr scope = (a_scope_ptr)ptr;
      trans_unit_for_scope[scope->number] = translation_units;
    }  /* if */
    prune = FALSE;
  } else {
    /* This entry is in the file scope memory region of a secondary translation
       unit.  The il_walk_flag is on to indicate that copying is needed, and
       then turned off once the copying has been done. */
    if (il_entry_prefix_of(ptr).il_walk_flag == !flag_value_meaning_visited) {
      /* This entry does not need any (more) processing. */
      prune = TRUE;
    } else {
      /* This entry still needs to be processed (i.e., copied and remapped). */
      il_entry_prefix_of(ptr).il_walk_flag = !flag_value_meaning_visited;
      prune = FALSE;
    }  /* if */
  }  /* if */
  return prune;
}  /* copy_termination_test */


/*ARGSUSED*/ /* <-- "kind" is not used. */
static char *remap_secondary_ptr_to_primary(char             *ptr,
                                            an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to remap a pointer to something
in a secondary translation unit ("ptr", of kind "kind") to the
corresponding entry in the primary file IL.
*/
{
  char *corresp;

  /* If the pointer wasn't encountered previously, make sure its
     correspondence pointer is set.  This happens for "next" pointers. */
  corresp_setup(ptr, kind);
  if (ptr == NULL) {
    /* Leave a NULL pointer unchanged. */
    corresp = NULL;
  } else if (!in_file_scope(ptr)) {
    /* Leave a function scope pointer unchanged. */
    corresp = ptr;
  } else {
    corresp = canonical_il_entry_of(ptr);
    check_assertion_str(!in_secondary_trans_unit(corresp),
                        "remap_secondary_ptr_to_primary: bad corresp ptr");
  }  /* if */
  return corresp;
}  /* remap_secondary_ptr_to_primary */


static void remap_pointers_in_entry(char             *ptr,
                                    an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to remap the pointers in the IL
entry at ptr (of kind "kind").
*/
{
  a_remap_function_ptr saved_walk_remap_func = walk_remap_func;

  walk_remap_func = remap_secondary_ptr_to_primary;
  remap_pointers_in_il_entry(ptr, kind);
  walk_remap_func = saved_walk_remap_func;
}  /* remap_pointers_in_entry */


/*ARGSUSED*/ /* <-- "kind" is not used. */
static void copy_string_entry(char             *ptr,
                              an_il_entry_kind kind,
                              sizeof_t         length)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the string IL entry at ptr
(of kind "kind", and length "length") to the primary file IL, setting its
correspondence pointer to point to the copy.
*/
{
  /* Ignore strings that are already in the primary file IL (such as
     name strings from the symbol header). */
  if (in_secondary_trans_unit(ptr)) {
    char *copy = alloc_primary_file_scope_il(length);

    check_assertion(in_file_scope(ptr));
    checked_trans_unit_corresp_pointer_of(ptr) = copy;
    (void)memcpy(copy, ptr, size_t_arg(length));
  }  /* if */
}  /* copy_string_entry */


static void copy_entry(char             *ptr,
                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the IL entry at ptr
(of kind "kind") to the space indicated by its correspondence pointer,
and remap the pointers in the copy.
*/
{
  a_source_correspondence *scp = NULL;
  char                    *copy;

  if (!in_file_scope(ptr)) {
    /* Process an entry in a function scope memory region.  Remap
       the pointers but don't copy. */
    remap_pointers_in_entry(ptr, kind);
#if MAINTAIN_NEEDED_FLAGS
    copy = ptr;
    scp = source_corresp_for_il_entry(copy, kind);
#endif /* MAINTAIN_NEEDED_FLAGS */
  } else {
    copy = checked_trans_unit_corresp_pointer_of(ptr);
    check_assertion_str(copy != NULL,
                        "copy_entry: NULL correspondence pointer");
    /* Copy the entry to its corresponding space and remap the pointers
       in the copy. */
    (void)memcpy(copy, ptr, size_t_arg(sizeof_il_entry[(int)kind]));
    remap_pointers_in_entry(copy, kind);
    scp = source_corresp_for_il_entry(copy, kind);
    if (scp != NULL) scp->copied_from_secondary_trans_unit = TRUE;
    if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)ptr;
      if (rout->assoc_scope != NULL_region_number) {
        /* For a routine with a body, the code in the function scope memory
           region needs to be processed too.  It doesn't need to be
           copied, but the pointers need to be remapped. */
        walk_routine_scope_il(rout->assoc_scope,
                              copy_entry,
                              copy_string_entry,
                              (a_remap_function_ptr)NULL,
                              copy_termination_test,
                              /*clear_fe_pointers=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  /* Clear the needed and keep_in_il flags in the copy (or original,
     for an entry in a file scope memory region), so that they can be
     recomputed in the context of the primary IL. */
  il_entry_prefix_of(copy).keep_in_il = FALSE;
  if (scp != NULL) {
    scp->needed = FALSE;
#if ONE_INSTANTIATION_PER_OBJECT
    scp->per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
    if (kind == iek_type) {
      a_type_ptr type = (a_type_ptr)copy;
      if (is_immediate_class_type(type)) {
        type->variant.class_struct_union.definition_needed = FALSE;
        type->variant.class_struct_union.keep_definition_in_il = FALSE;
      }  /* if */
    } else if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)copy;
      rout->definition_needed = FALSE;
      rout->keep_definition_in_il = FALSE;
    }  /* if */
  }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* copy_entry */


static void copy_from_secondary_to_primary_IL(void)
/*
Copy everything from the current secondary translation unit IL to the
primary translation unit IL.
*/
{
  db_enter(1, "copy_from_secondary_to_primary_il");
  walk_file_scope_il(copy_entry, copy_string_entry,
                     (a_remap_function_ptr)NULL,
                     copy_termination_test,
                     /*clear_fe_pointers=*/FALSE);
  db_exit();
}  /* copy_from_secondary_to_primary_IL */


static void remove_dynamic_initialization(a_dynamic_init_ptr dip)
/*
Remove the indicated dynamic initialization from any destruction lists.
Also remove any nested object lifetimes.
*/
{
  if (dip->init_expr_lifetime) {
    /* There is a nested object lifetime.  Eliminate it and everything in
       it. */
    detach_from_object_lifetime_tree(dip->init_expr_lifetime);
    dip->init_expr_lifetime = NULL;
  }  /* if */
  remove_from_destruction_list(dip);
}  /* remove_dynamic_initialization */


static void clear_variable_initialization(a_variable_ptr variable)
/*
Eliminate the initialization of the indicated variable to turn it
into a declaration instead of a definition.
*/
{
  if (variable->init_kind == (an_init_kind)initk_dynamic) {
    /* Eliminate any destructions and object lifetimes associated with
       this initialization. */
    /* Note that the dynamic initialization for this variable
       will be removed from the scope dynamic_inits list later. */
    a_dynamic_init_ptr dip = variable->initializer.dynamic;
    remove_dynamic_initialization(dip);
  }  /* if */
  variable->init_kind = (an_init_kind)initk_none;
  if (variable->storage_class == (a_storage_class)sc_unspecified) {
    variable->storage_class = (a_storage_class)sc_extern;
  }  /* if */
}  /* clear_variable_initialization */


static void clear_body_for_routine(a_routine_ptr routine)
/*
Eliminate the body of the indicated routine.
*/
{
  a_scope_ptr routine_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
  check_assertion(routine_scope != NULL);
  clear_function_body(routine_scope);
}  /* clear_body_for_routine */


static a_boolean type_should_be_merged(a_type_ptr type)
/*
Return TRUE if the indicated type, which has a corresponding type in the
primary IL, should be merged into that type.
*/
{
  a_boolean  merge = FALSE;
  a_type_ptr corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if ((is_immediate_class_type(type) &&
       class_type_has_body(type) &&
       !class_type_has_body(corresp_type)) ||
      (is_immediate_enum_type(type) &&
       !is_incomplete_type(type) &&
       is_incomplete_type(corresp_type))) {
    /* This type is a struct, union, class, or enum with a definition,
       and the corresponding type has no definition.  Therefore the
       definition must be merged into the corresponding type. */
    merge = TRUE;
  }  /* if */
  return merge;
}  /* type_should_be_merged */


static a_boolean variable_should_be_merged(a_variable_ptr variable)
/*
Return TRUE if the indicated variable, which has a corresponding variable
in the primary IL, should be merged into that variable.
*/
{
  a_boolean      merge = FALSE;
  a_variable_ptr corresp_variable =
                               (a_variable_ptr)canonical_il_entry_of(variable);

  if (variable->init_kind != (an_init_kind)initk_none) {
    if (corresp_variable->init_kind == (an_init_kind)initk_none) {
      /* This variable has a definition, and the corresponding variable
         has no definition.  Therefore the definition must be merged into
         the corresponding variable. */
      merge = TRUE;
    } else if (variable->is_specialized && !corresp_variable->is_specialized) {
      /* This variable has a definition that is a specialization, and the
         other variable has a definition that is not a specialization.
         Copy this definition over to the corresponding variable. */
      merge = TRUE;
    }  /* if */
  }  /* if */
  return merge;
}  /* variable_should_be_merged */


static a_boolean routine_should_be_merged(
                                    a_routine_ptr routine,
                                    a_boolean     *any_removed_function_bodies)
/*
Return TRUE if the indicated routine, which has a corresponding routine
in the primary IL, should be merged into that routine.  If both this
routine and the corresponding routine have bodies, the one here is
deleted, and *any_removed_function_bodies is set to TRUE.  If
any_removed_function_bodies is NULL, that deletion is suppressed.
*/
{
  a_boolean     merge = FALSE;
  a_routine_ptr corresp_routine= (a_routine_ptr)canonical_il_entry_of(routine);

  if (routine->assoc_scope != NULL_region_number) {
    if (corresp_routine->assoc_scope == NULL_region_number) {
      /* This routine has a definition, and the corresponding routine
         has no definition.  Therefore the definition must be merged into
         the corresponding routine. */
      merge = TRUE;
    } else if (routine->is_specialized && !corresp_routine->is_specialized) {
      /* This routine has a definition that is a specialization, and the
         other routine has a definition that is not a specialization.
         Copy this definition over to the corresponding routine. */
      merge = TRUE;
    } else if (any_removed_function_bodies != NULL) {
      /* Both instances have definitions.  Eliminate the body of this copy. */
      clear_body_for_routine(routine);
      *any_removed_function_bodies = TRUE;
    }  /* if */
  }  /* if */
  return merge;
}  /* routine_should_be_merged */


static a_boolean entry_should_be_merged(char             *ptr,
                                        an_il_entry_kind kind)
/*
Return TRUE if the indicated entry, which has the indicated kind, and
which has a corresponding entry in the primary IL, should be merged
into that entry.
*/
{
  a_boolean merge;

  /* For certain kinds, use a special routine (which can deal with
     references to entries that have not been processed yet).  For the
     others, use the generic macro, which depends on the entry having
     been processed previously. */
  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)ptr;
        merge = type_should_be_merged(type);
      }
      break;
    case iek_variable:
      { a_variable_ptr var = (a_variable_ptr)ptr;
        merge = variable_should_be_merged(var);
      }
      break;
    case iek_routine:
      { a_routine_ptr rout = (a_routine_ptr)ptr;
        merge = routine_should_be_merged(rout, (a_boolean *)NULL);
      }
      break;
    default:
      merge = entry_to_be_merged(ptr);
      break;
  }
  return merge;
}  /* entry_should_be_merged */


/*
Macro that returns TRUE if the indicated entry should be copied to
the primary file IL.  That can be because it's new (it has no
correspondence) or because it provides a definition for a corresponding
entry that is already in the primary file IL.
*/
#define entry_should_be_copied(ptr, kind) \
  (!has_corresp(ptr) || entry_should_be_merged((char *)(ptr), (kind)))


static a_boolean prepare_for_trans_unit_copy(
                                      a_scope_ptr scope,
                                      a_boolean   *any_removed_function_bodies)
/*
Scan the indicated scope (a file, namespace, or class scope in a secondary
translation unit) and its subscopes and set up for copying the scope
to the primary translation unit IL.  *any_removed_function_bodies is
set to TRUE if the body of a routine is eliminated.  Returns TRUE if the
entity associated with the scope should be kept on the caller's list
(because something in it needs to be copied or merged later).
The entity lists of the scope are pruned so that only entities
that must be copied to or merged into the primary IL are left on
the lists.
*/
{
  a_boolean          keep_on_parent_list = FALSE;
  a_boolean          check_member_merges = FALSE;
  a_boolean          any_members_to_process = FALSE;
  a_type_ptr         type, prev_type;
  a_variable_ptr     variable, prev_variable;
  a_dynamic_init_ptr dyn_init, prev_dyn_init;
  a_routine_ptr      routine, prev_routine;
  a_template_ptr     templ, prev_templ;
  a_namespace_ptr    nsp, prev_nsp;
  a_pragma_ptr       pragma, prev_pragma;
  a_type_ptr         class_type;
  a_boolean          keep_on_list;
  a_scope_pointers_block
                     *pointers_block;

  /* Set the correspondence for this scope, if any. */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* The file scope in a secondary translation unit corresponds to
       the file scope in the primary translation unit, and gets merged
       into it. */
    a_scope_ptr corresp_scope = translation_units->primary_scope;
    checked_trans_unit_corresp_pointer_of(scope) = (char *)corresp_scope;
    mark_to_merge(scope);
    if (scope->lifetime != NULL && corresp_scope->lifetime != NULL) {
      /* The object lifetime of the file scope corresponds with the
         object lifetime of the corresponding scope, and gets merged into
         it. */
      checked_trans_unit_corresp_pointer_of(scope->lifetime) =
                                               (char *)corresp_scope->lifetime;
      mark_to_merge(scope->lifetime);
    }  /* if */
    keep_on_parent_list = TRUE;
    check_member_merges = TRUE;
    pointers_block = get_pointers_block_for_scope(scope);
    check_assertion(pointers_block != NULL);
  } else if (scope->kind == (a_scope_kind)sck_namespace) {
    /* For a namespace scope, go to the a_namespace entry to find out what
       correspondence there is, if any. */
    nsp = scope->variant.assoc_namespace;
    if (!has_corresp(nsp)) {
      /* The namespace doesn't exist in the primary IL, and just gets
         copied over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else {
      /* The namespace scope gets merged into the corresponding scope. */
      a_namespace_ptr corresp_nsp= (a_namespace_ptr)canonical_il_entry_of(nsp);
      a_scope_ptr     corresp_nsp_scope = corresp_nsp->variant.assoc_scope;
      checked_trans_unit_corresp_pointer_of(scope) = (char *)corresp_nsp_scope;
      /* The namespace needs to be kept only if some of its members need
         to be processed.  Assume there are none, and correct that assumption
         as we look at the members. */
      keep_on_parent_list = FALSE;
      check_member_merges = TRUE;
    }  /* if */
    pointers_block = get_pointers_block_for_scope(scope);
    check_assertion(pointers_block != NULL);
  } else {
    /* A class scope. */
    check_assertion(scope->kind == (a_scope_kind)sck_class_struct_union);
    class_type = scope->variant.assoc_type;
    if (!has_corresp(class_type)) {
      /* The class doesn't exist in the primary IL, and just gets copied
         over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else {
      /* The class gets merged into the corresponding class. */
      a_type_ptr corresp_class = (a_type_ptr)canonical_il_entry_of(class_type);
      if (!class_type_has_body(corresp_class)) {
        /* The class here has a definition, but the class in the primary
           IL has only a declaration.  The definition will be copied over,
           but we don't need to check the members. */
        keep_on_parent_list = TRUE;
        check_member_merges = FALSE;
        mark_to_merge(class_type);
      } else {
        /* Both instances of the class have definitions. */
        a_scope_ptr corresp_class_scope = corresp_class->variant.
                                    class_struct_union.extra_info->assoc_scope;
        checked_trans_unit_corresp_pointer_of(scope) =
                                                   (char *)corresp_class_scope;
        /* The class here needs to be kept only if some of its members need
           to be processed.  Assume there are none, and correct that assumption
           as we look at the members. */
        keep_on_parent_list = FALSE;
        check_member_merges = TRUE;
      }  /* if */
    }  /* if */
    pointers_block = NULL;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Source sequence entries aren't generated in secondary translation
     units. */
  check_assertion(scope->source_sequence_list == NULL &&
                  scope->src_seq_sublist_list == NULL);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  /* The hidden name table is not generated in secondary translation units.
     If that is changed, keep two things in mind:
       1)  With instantiated exported templates, it's going to be hard to
           define what the hidden name table should contain for the
           synthesized lookup context in the instantiation.
       2)  The hidden name table from the scope here would have to be
           merged into the destination scope, with duplicates eliminated. */
  check_assertion(scope->hidden_names == NULL);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  /* Visit all types. */
  prev_type = NULL;
  for (type = scope->types; type != NULL; type = type->next) {
    keep_on_list = TRUE;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
      /* A class with a scope.  Do a recursive call to process it. */
      a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      keep_on_list = prepare_for_trans_unit_copy(class_scope,
                                                 any_removed_function_bodies);
    } else if (has_corresp(type)) {
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (check_member_merges &&
          type_should_be_merged(type)) {
        /* This type should be merged into the corresponding type. */
        mark_to_merge(type);
        keep_on_list = TRUE;
      }  /* if */
    } else if (type->kind == (a_type_kind)tk_typeref &&
               !typeref_is_typedef(type)) {
      /* This is a placeholder typeref, used to give guidance to IL lowering
         on the order of types promoted out of classes and namespaces.
         Keep the placeholder only if the type pointed to is being kept. */
      a_type_ptr ref_type = type->variant.typeref.type;
      keep_on_list = entry_should_be_copied(ref_type, iek_type);
    }  /* if */
    if (keep_on_list) {
      prev_type = type;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_type == NULL) {
        scope->types = type->next;
      } else {
        prev_type->next = type->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_type = prev_type;
  /* Visit all static variables (non-static variables come up only
     in function and block scopes, which don't come here). */
  prev_variable = NULL;
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    keep_on_list = TRUE;
    if (has_corresp(variable)) {
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (check_member_merges && variable_should_be_merged(variable)) {
        /* This variable has an initializer, which must be merged into the
           corresponding variable. */
        mark_to_merge(variable);
        keep_on_list = TRUE;
      }  /* if */
    } else {
      /* This variable has no correspondence in the primary file IL. */
      if (translation_unit_needed_only_for_exported_templates) {
        /* We're supposed to copy only generated templates.  Other variables
           are made external (if necessary) and their definitions are
           dropped (the definition will be put out when the file
           is compiled as a primary file). */
        if (!variable->is_template_static_data_member ||
            variable->is_specialized) {
          /* The variable is not a generated template. */
          if (variable->init_kind != (an_init_kind)initk_none) {
            clear_variable_initialization(variable);
          }  /* if */
#if DO_IL_LOWERING
          if (il_lowering_needed() &&
              variable->storage_class == (a_storage_class)sc_static) {
            /* A static variable referenced from a template is changed to an
               external declaration and copied over. */
            /* The name must be processed now because we want to use
               the module id from the secondary translation unit. */
            externalize_source_correspondence(&variable->source_corresp,
                                              /*is_variable=*/TRUE);
            variable->storage_class = (a_storage_class)sc_extern;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */
      }  /* if */
    }  /* if */
    if (keep_on_list) {
      prev_variable = variable;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_variable == NULL) {
        scope->variables = variable->next;
      } else {
        prev_variable->next = variable->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_variable = prev_variable;
  /* Visit all dynamic initializations. */
  prev_dyn_init = NULL;
  for (dyn_init = scope->dynamic_inits;
       dyn_init != NULL;
       dyn_init = dyn_init->next) {
    variable = dyn_init->variable;
    /* Remove an entry if the corresponding variable is no longer
       initialized.  See clear_variable_initialization. */
    if (variable->init_kind == (an_init_kind)initk_none) {
      if (prev_dyn_init == NULL) {
        scope->dynamic_inits = dyn_init->next;
      } else {
        prev_dyn_init->next = dyn_init->next;
      }  /* if */
    } else {
      /* Keep this entry on the list. */
      prev_dyn_init = dyn_init;
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) {
    pointers_block->last_dynamic_init = prev_dyn_init;
  }  /* if */
  /* Visit all routines. */
  prev_routine = NULL;
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    keep_on_list = TRUE;
    if (has_corresp(routine)) {
      a_routine_ptr corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (check_member_merges &&
          routine_should_be_merged(routine, any_removed_function_bodies)) {
        /* Merge the definition here into the corresponding routine. */
        mark_to_merge(routine);
        keep_on_list = TRUE;
      }  /* if */
      /* Update some information regarding inline functions. */
      check_assertion(routine->is_inline == corresp_routine->is_inline);
#if INSTANTIATE_EXTERN_INLINE
      corresp_routine->inline_instance_required |=
                                             routine->inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
      /* Note that suppress_inline_body is meaningful only when the routine
         has a body, and the interesting value -- the one that sticks --
         is FALSE. */
      if (routine->assoc_scope != NULL_region_number &&
          corresp_routine->assoc_scope != NULL_region_number) {
        corresp_routine->suppress_inline_body &= routine->suppress_inline_body;
      }  /* if */
      { a_symbol_ptr sym = (a_symbol_ptr)(routine->source_corresp.assoc_info);
        a_symbol_ptr corresp_sym =
                    (a_symbol_ptr)(corresp_routine->source_corresp.assoc_info);
        if (sym != NULL && corresp_sym != NULL) {
          a_template_instance_ptr instance = sym->variant.routine.instance_ptr;
          a_template_instance_ptr corresp_instance =
                                     corresp_sym->variant.routine.instance_ptr;
          if (instance != NULL && corresp_instance != NULL) {
            /* Transfer some information regarding instantiations. */
            corresp_instance->instantiation_required |=
                                              instance->instantiation_required;
          }  /* if */
        }  /* if */
      }
    } else {
      /* This routine has no correspondence in the primary file IL. */
      if (translation_unit_needed_only_for_exported_templates) {
        /* We're supposed to copy only generated templates.  Other routines
           are made external (if necessary) and their definitions are
           dropped. */
        if (!routine->is_template_function || routine->is_specialized) {
          /* The function is not a generated template. */
          /* The definition should have been eliminated at pop_scope
             time (the definition will be put out when the file is compiled
             as a primary file) unless the routine is inline. */
          if (routine->assoc_scope != NULL_region_number) {
            check_assertion(routine->is_inline);
            clear_body_for_routine(routine);
          }  /* if */
#if DO_IL_LOWERING
          if (il_lowering_needed() &&
              routine->storage_class == (a_storage_class)sc_static) {
            /* A static function referenced from a template is changed to an
               external declaration and copied over. */
            /* The name must be processed now because we want to use
               the module id from the secondary translation unit. */
            mangle_function_name(routine);
            externalize_source_correspondence(&routine->source_corresp,
                                              /*is_variable=*/FALSE);
            routine->storage_class = (a_storage_class)sc_extern;
          }  /* if */
#endif /* DO_IL_LOWERING */
        }  /* if */
      }  /* if */
    }  /* if */
    if (keep_on_list) {
      prev_routine = routine;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_routine == NULL) {
        scope->routines = routine->next;
      } else {
        prev_routine->next = routine->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_routine = prev_routine;
  /* Visit all templates. */
  prev_templ = NULL;
  for (templ = scope->templates;
       templ != NULL;
       templ = templ->next) {
    keep_on_list = TRUE;
    if (has_corresp(templ)) {
      /* This entry corresponds to something in the primary IL, so remove
         it from the list. */
      keep_on_list = FALSE;
    }  /* if */
    if (keep_on_list) {
      prev_templ = templ;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_templ == NULL) {
        scope->templates = templ->next;
      } else {
        prev_templ->next = templ->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_template = prev_templ;
  /* Visit all namespaces. */
  prev_nsp = NULL;
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    /* Entities with correspondences don't get copied; they get merged
       into the corresponding entry.  Set a flag to indicate that. */
    if (!nsp->is_namespace_alias) {
      keep_on_list = prepare_for_trans_unit_copy(nsp->variant.assoc_scope,
                                                 any_removed_function_bodies);
    } else {
      /* A namespace alias.  Keep it only if there's not already a copy in
         the primary IL. */
      keep_on_list = !has_corresp(nsp);
    }  /* if */
    if (keep_on_list) {
      prev_nsp = nsp;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_nsp == NULL) {
        scope->namespaces = nsp->next;
      } else {
        prev_nsp->next = nsp->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_namespace = prev_nsp;
  /* Visit all pragmas. */
  prev_pragma = NULL;
  for (pragma = scope->pragmas;
       pragma != NULL;
       pragma = pragma->next) {
    /* Keep the pragma if it has an associated entity that will be kept. */
    keep_on_list = FALSE;
    if (pragma->entity.ptr != NULL &&
        entry_should_be_copied(pragma->entity.ptr,
                               (an_il_entry_kind)pragma->entity.kind)) {
      keep_on_list = TRUE;
    }  /* if */
    if (keep_on_list) {
      prev_pragma = pragma;
      any_members_to_process = TRUE;
    } else {
      /* Remove this entry from the list. */
      if (prev_pragma == NULL) {
        scope->pragmas = pragma->next;
      } else {
        prev_pragma->next = pragma->next;
      }  /* if */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_pragma = prev_pragma;
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (scope->kind == (a_scope_kind)sck_file &&
      *any_removed_function_bodies) {
    /* Remove scope orphaned list entries for eliminated functions. */
#if MAINTAIN_NEEDED_FLAGS
    if (okay_to_eliminate_unneeded_il_entries) {
      eliminate_unneeded_scope_orphaned_list_entries();
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  if (check_member_merges && any_members_to_process) {
    /* There are some members of this scope that need processing, so we have
       to keep the scope's associated entity on the list to be able to
       perform the merges. */
    keep_on_parent_list = TRUE;
    mark_to_merge(scope);
    if (scope->kind == (a_scope_kind)sck_namespace) {
      nsp = scope->variant.assoc_namespace;
      mark_to_merge(nsp);
    } else if (scope->kind == (a_scope_kind)sck_class_struct_union) {
      class_type = scope->variant.assoc_type;
      mark_to_merge(class_type);
    }  /* if */
  }  /* if */
  return keep_on_parent_list;
}  /* prepare_for_trans_unit_copy */


static void merge_object_lifetimes(a_scope_ptr scope,
                                   a_scope_ptr primary_scope)
/*
Merge the object lifetimes, if any, attached to scope and primary_scope.
Those are corresponding scopes (from a secondary translation unit and
the primary translation unit, respectively) that are being merged.
*/
{
  an_object_lifetime_ptr lifetime, primary_lifetime;

  if (scope->lifetime != NULL) {
    check_assertion(scope->kind == (a_scope_kind)sck_file);
    /* Use the copy of the scope lifetime, which has remapped pointers. */
    lifetime = (an_object_lifetime_ptr)checked_trans_unit_corresp_pointer_of(
                                                              scope->lifetime);
    primary_lifetime = primary_scope->lifetime;
    if (primary_lifetime == NULL) {
      /* There is no object lifetime in the primary scope, so the
         lifetime is copied not merged. */
      primary_scope->lifetime = lifetime;
    } else {
      an_object_lifetime_ptr child, last_child;
      a_dynamic_init_ptr     last_destr;
      primary_lifetime->has_implicit_child |= lifetime->has_implicit_child;
      /* Copy the children of "lifetime" to the end of the list of children
         of "primary lifetime". */
      last_child = primary_lifetime->child_lifetime;
      if (last_child == NULL) {
        primary_lifetime->child_lifetime = lifetime->child_lifetime;
      } else {
        while (last_child->next != NULL) last_child = last_child->next;
        last_child->next = lifetime->child_lifetime;
      }  /* if */
      /* Copy the destructions list of "lifetime" to the front of the
         destructions list of "primary_lifetime" (the list is in order
         of destruction -- backwards -- so new entries go at the front
         of the list). */
      last_destr = lifetime->destructions;
      if (last_destr != NULL) {
        /* Find the end of the list. */
        while (last_destr->next_in_destruction_list != NULL) {
          last_destr = last_destr->next_in_destruction_list;
        }  /* while */
        last_destr->next_in_destruction_list = primary_lifetime->destructions;
        primary_lifetime->destructions = lifetime->destructions;
        /* Adjust the parent_destruction_sublist pointer in children:
           a NULL pointer, meaning end of the list, becomes the first
           destruction on the primary_lifetime list. */
        for (child = lifetime->child_lifetime;
             child != NULL;
             child = child->next) {
          if (child->parent_destruction_sublist == NULL) {
            child->parent_destruction_sublist = primary_lifetime->destructions;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* merge_object_lifetimes */


static void remove_from_primary_file_variables_list(a_variable_ptr variable)
/*
Remove the indicated variable from its variables list in the primary file
IL.
*/
{
   a_translation_unit_ptr saved_tup = curr_translation_unit;

   /* Partially switch to the primary translation unit temporarily. */
   curr_translation_unit = translation_units;
   remove_from_variables_list(variable, NO_SCOPE_DEPTH);
   curr_translation_unit = saved_tup;
}  /* remove_from_primary_file_variables_list */


static void remove_from_primary_file_routines_list(a_routine_ptr routine)
/*
Remove the indicated routine from its routines list in the primary file
IL.
*/
{
   a_translation_unit_ptr saved_tup = curr_translation_unit;

   /* Partially switch to the primary translation unit temporarily. */
   curr_translation_unit = translation_units;
   remove_from_routines_list(routine, NO_SCOPE_DEPTH);
   curr_translation_unit = saved_tup;
}  /* remove_from_primary_file_routines_list */


static void move_to_end_of_primary_file_types_list(a_type_ptr type)
/*
Move the indicated type entry to the end of its type list in the
primary file IL.
*/
{
   a_translation_unit_ptr saved_tup = curr_translation_unit;

   /* Partially switch to the primary translation unit temporarily. */
   curr_translation_unit = translation_units;
   /* Move the type to the end of the list.  Also remove any associated
      namespace placeholder, but do not move it to the end of the list.
      There will be a placeholder in the secondary IL that gets moved
      over. */
   move_to_end_of_types_list(type, NO_SCOPE_DEPTH,
                             /*delete_placeholder=*/TRUE);
   curr_translation_unit = saved_tup;
}  /* move_to_end_of_primary_file_types_list */


/*
Macros that do saves/restores needed for each overwrite_primary_xxx
routine, used when an IL entry in the secondary translation unit
IL is copied on top of an existing entry in the primary IL.
*/
#if MAINTAIN_NEEDED_FLAGS
#define save_needed_flag_for_overwrite(primary_entry) \
  a_boolean saved_needed = (primary_entry)->source_corresp.needed;
#define restore_needed_flag_for_overwrite(primary_entry) \
  (primary_entry)->source_corresp.needed = saved_needed;
#else /* !MAINTAIN_NEEDED_FLAGS */
#define save_needed_flag_for_overwrite(primary_entry) /* Nothing */
#define restore_needed_flag_for_overwrite(primary_entry) /* Nothing */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if ONE_INSTANTIATION_PER_OBJECT
#define save_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  a_per_instantiation_needed_flags_entry_ptr saved_pi_needed = \
        (primary_entry)->source_corresp.per_instantiation_needed_flags;
#define restore_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  (primary_entry)->source_corresp.per_instantiation_needed_flags = \
                                                       saved_pi_needed;
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define save_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  /* Nothing */
#define restore_per_instantiation_needed_flags_for_overwrite(primary_entry) \
  /* Nothing */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#define do_saves_for_overwrite(primary_entry, entry_ptr_type) \
  entry_ptr_type saved_next = (primary_entry)->next; \
  char *saved_assoc_info = (primary_entry)->source_corresp.assoc_info; \
  save_needed_flag_for_overwrite(primary_entry) \
  save_per_instantiation_needed_flags_for_overwrite(primary_entry)
/* Note that the assoc_info pointer in the source of the copy is
   set to the pointer from the destination, as a way to preserve a
   pointer to the original associated symbol after the copy has
   been done. */
#define do_restores_for_overwrite(primary_entry, entry) \
  (primary_entry)->next = saved_next; \
  (entry)->source_corresp.assoc_info = saved_assoc_info; \
  restore_needed_flag_for_overwrite(primary_entry) \
  restore_per_instantiation_needed_flags_for_overwrite(primary_entry)


static void overwrite_primary_type(a_type_ptr type,
                                   a_type_ptr primary_type)
/*
Overwrite the type primary_type (in the primary IL) with type (in
the secondary translation unit IL).
*/
{
  do_saves_for_overwrite(primary_type, a_type_ptr);
  *primary_type = *type;
  do_restores_for_overwrite(primary_type, type);
}  /* overwrite_primary_type */


static void overwrite_primary_variable(a_variable_ptr var,
                                       a_variable_ptr primary_var)
/*
Overwrite the variable primary_var (in the primary IL) with var (in
the secondary translation unit IL).
*/
{
  do_saves_for_overwrite(primary_var, a_variable_ptr);
  *primary_var = *var;
  do_restores_for_overwrite(primary_var, var);
}  /* overwrite_primary_variable */


static void overwrite_primary_routine(a_routine_ptr rout,
                                      a_routine_ptr primary_rout)
/*
Overwrite the routine primary_rout (in the primary IL) with rout (in
the secondary translation unit IL).
*/
{
#if INSTANTIATE_EXTERN_INLINE
  a_boolean saved_inline_instance_required =
                                        primary_rout->inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
  /* suppress_inline_body is only valid on routines with bodies.  Save the
     destination value only if the destination routine already has a
     body. */
  a_boolean saved_suppress_inline_body =
                            (primary_rout->assoc_scope != NULL_region_number) ?
                                           primary_rout->suppress_inline_body :
                                           rout->suppress_inline_body;
  do_saves_for_overwrite(primary_rout, a_routine_ptr);
  *primary_rout = *rout;
  do_restores_for_overwrite(primary_rout, rout);
  /* Note that inline_instance_required etc. were previously updated in
     the primary routine, so we just save the value determined. */
#if INSTANTIATE_EXTERN_INLINE
  primary_rout->inline_instance_required = saved_inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
  primary_rout->suppress_inline_body = saved_suppress_inline_body;
}  /* overwrite_primary_routine */


static void finish_trans_unit_copy(a_scope_ptr scope)
/*
scope is a file, namespace, or class scope from the secondary file IL.  Do
processing required after the IL walk to copy IL entries from the
secondary scope to the primary file IL.
*/
{
  a_scope_ptr            primary_scope;
  a_scope_pointers_block *pointers_block;
  a_boolean              is_class_scope =
                         (scope->kind == (a_scope_kind)sck_class_struct_union);

  /* Process only scopes that must be merged into their counterparts. */
  if (entry_to_be_merged(scope)) {
    /* Find the corresponding scope. */
    primary_scope = (a_scope_ptr)canonical_il_entry_of(scope);
    /* Get the pointers block for the primary IL scope. */
    pointers_block = get_pointers_block_for_scope(primary_scope);
    check_assertion(pointers_block != NULL || is_class_scope);
    if (scope->types != NULL) {
      a_type_ptr type, last_type;
      /* Merge the types in the scope into the primary IL scope. */
      /* Get a pointer to the last type in the primary scope. */
      if (pointers_block != NULL) {
        last_type = pointers_block->last_type;
      } else {
        /* Actual last type will be determined below if/when needed. */
        last_type = NULL;
      }  /* if */
      for (type = scope->types; type != NULL; type = type->next) {
        a_type_ptr corresp_type =
                       (a_type_ptr)checked_trans_unit_corresp_pointer_of(type);
        if (is_immediate_class_type(type) &&
            type->variant.class_struct_union.extra_info != NULL &&
            type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
          /* A class with a scope.  Do a recursive call to process it. */
          a_scope_ptr class_scope = type->variant.class_struct_union.
                                                       extra_info->assoc_scope;
          finish_trans_unit_copy(class_scope);
        }  /* if */
        if (!entry_to_be_merged(type)) {
          /* An entry that had no correspondence. */
          if (is_class_scope && last_type == NULL) {
            /* Determine the last type the first time it is needed. */
            last_type = primary_scope->types;
            if (last_type != NULL) {
              while (last_type->next != NULL) last_type = last_type->next;
            }  /* if */
          }  /* if */
          /* Add the type to the end of the list. */
          if (last_type == NULL) {
            primary_scope->types = corresp_type;
          } else {
            last_type->next = corresp_type;
          }  /* if */
        } else {
          a_type_ptr primary_type =
               (a_type_ptr)checked_trans_unit_corresp_pointer_of(corresp_type);
          /* If both copies have a definition, leave the primary definition
             alone.  The class was presumably marked to be merged because
             some of its member definitions needed to be merged. */
          if (is_immediate_class_type(primary_type) &&
              class_type_has_body(primary_type)) goto end_of_type_list_add;
          /* Merge the information from this type into the primary IL type
             (the secondary translation unit instance has a definition and
             the primary translation unit instance does not).  Move the
             primary IL type to the end of the types list so that it
             appears on the list at the point where the definition
             appears.  Class members are not moved to the end of the list. */
          if (!is_class_scope) {
            move_to_end_of_primary_file_types_list(primary_type);
          }  /* if */
          overwrite_primary_type(corresp_type, primary_type);
          corresp_type = primary_type;
          if (is_class_scope) goto end_of_type_list_add;
        } /* if */
        corresp_type->next = NULL;
        last_type = corresp_type;
        if (pointers_block != NULL) pointers_block->last_type = last_type;
end_of_type_list_add:;
      }  /* for */
    }  /* if */
    if (scope->variables != NULL) {
      a_variable_ptr variable, last_variable;
      /* Merge the variables in the scope into the primary IL scope. */
      /* Get a pointer to the last variable in the primary scope. */
      if (pointers_block != NULL) {
        last_variable = pointers_block->last_variable;
      } else {
        /* Actual last variable will be determined below if/when needed. */
        last_variable = NULL;
      }  /* if */
      for (variable = scope->variables;
           variable != NULL;
           variable = variable->next) {
        a_variable_ptr corresp_variable =
               (a_variable_ptr)checked_trans_unit_corresp_pointer_of(variable);
        if (entry_to_be_merged(variable)) {
          /* Merge the information from this variable into the primary IL
             variable (the secondary translation unit instance has a definition
             and the primary translation unit instance does not).  Move
             the primary IL variable to the end of the variables list so
             that it appears on the list at the point where the definition
             appears.  Class members are not moved to the end of the list. */
          a_variable_ptr primary_variable =
                   (a_variable_ptr)checked_trans_unit_corresp_pointer_of(
                                                             corresp_variable);
          if (primary_variable->init_kind != (an_init_kind)initk_none) {
            /* Eliminate the body of the primary variable (this happens when
               the secondary has a specialization and the primary does not). */
            clear_variable_initialization(primary_variable);
          }  /* if */
          if (!is_class_scope) {
            remove_from_primary_file_variables_list(primary_variable);
            last_variable = pointers_block->last_variable;
          }  /* if */
          overwrite_primary_variable(corresp_variable, primary_variable);
          corresp_variable = primary_variable;
          if (is_class_scope) goto end_of_variable_list_add;
        }  /* if */
        if (is_class_scope && last_variable == NULL) {
          /* Determine the last variable the first time it is needed. */
          last_variable = primary_scope->variables;
          if (last_variable != NULL) {
            while (last_variable->next != NULL) {
              last_variable = last_variable->next;
            }  /* while */
          }  /* if */
        }  /* if */
        /* Add the variable to the end of the list. */
        if (last_variable == NULL) {
          primary_scope->variables = corresp_variable;
        } else {
          last_variable->next = corresp_variable;
        }  /* if */
        corresp_variable->next = NULL;
        last_variable = corresp_variable;
        if (pointers_block != NULL) {
          pointers_block->last_variable = last_variable;
        }  /* if */
end_of_variable_list_add:;
#if MAINTAIN_NEEDED_FLAGS
        if (corresp_variable->storage_class ==
                                             (a_storage_class)sc_unspecified ||
            corresp_variable->init_kind == (an_init_kind)initk_dynamic) {
          /* Mark an externally-defined variable or one with initialization
             side effects as "needed". */
          mark_as_needed((char *)corresp_variable,
                         (an_il_entry_kind)iek_variable);
        }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
      }  /* for */
    }  /* if */
    if (scope->dynamic_inits != NULL) {
      /* Add the dynamic initializations of "scope" to the end of the
         dynamic initializations list of "primary scope". */
      a_dynamic_init_ptr copied_inits =
               (a_dynamic_init_ptr)canonical_il_entry_of(scope->dynamic_inits);
      a_dynamic_init_ptr last_dyn_init = primary_scope->dynamic_inits;
      if (last_dyn_init == NULL) {
        primary_scope->dynamic_inits = copied_inits;
      } else {
        last_dyn_init = pointers_block->last_dynamic_init;
        last_dyn_init->next = copied_inits;
      }  /* if */
      last_dyn_init = copied_inits;
      while (last_dyn_init->next != NULL) {
        last_dyn_init = last_dyn_init->next;
      }  /* while */
      check_assertion(pointers_block != NULL);  /* No dynamic init list
                                                   in classes. */
      pointers_block->last_dynamic_init = last_dyn_init;
    }  /* if */
    if (scope->routines != NULL) {
      a_routine_ptr routine, last_routine;
      /* Merge the routines in the scope into the primary IL scope. */
      /* Get a pointer to the last routine in the primary scope. */
      if (pointers_block != NULL) {
        last_routine = pointers_block->last_routine;
      } else {
        /* Actual last routine will be determined below if/when needed. */
        last_routine = NULL;
      }  /* if */
      for (routine = scope->routines;
           routine != NULL;
           routine = routine->next) {
        a_routine_ptr corresp_routine =
                 (a_routine_ptr)checked_trans_unit_corresp_pointer_of(routine);
        if (entry_to_be_merged(routine)) {
          /* Merge the information from this routine into the primary IL
             routine (the secondary translation unit instance has a
             definition and the primary translation unit instance does not).
             Move the primary IL routine to the end of the routines list
             so that it appears on the list at the point where the
             definition appears.  Class members are not moved to the
             end of the list. */
          a_routine_ptr primary_routine =
                   (a_routine_ptr)checked_trans_unit_corresp_pointer_of(
                                                              corresp_routine);
          if (primary_routine->assoc_scope != NULL_region_number) {
            /* Eliminate the body of the primary routine (this happens when
               the secondary has a specialization and the primary does not). */
            clear_body_for_routine(primary_routine);
          }  /* if */
          if (!is_class_scope) {
            remove_from_primary_file_routines_list(primary_routine);
            last_routine = pointers_block->last_routine;
          }  /* if */
          overwrite_primary_routine(corresp_routine, primary_routine);
          corresp_routine = primary_routine;
          if (is_class_scope) goto end_of_routine_list_add;
        }  /* if */
        if (is_class_scope && last_routine == NULL) {
          /* Determine the last routine the first time it is needed. */
          last_routine = primary_scope->routines;
          if (last_routine != NULL) {
            while (last_routine->next != NULL) {
              last_routine = last_routine->next;
            }  /* while */
          }  /* if */
        }  /* if */
        /* Add the routine to the end of the list. */
        if (last_routine == NULL) {
          primary_scope->routines = corresp_routine;
        } else {
          last_routine->next = corresp_routine;
        }  /* if */
        corresp_routine->next = NULL;
        last_routine = corresp_routine;
        if (pointers_block != NULL) {
          pointers_block->last_routine = last_routine;
        }  /* if */
end_of_routine_list_add:;
      }  /* for */
    }  /* if */
    if (scope->templates != NULL) {
      a_template_ptr templ, last_templ;
      /* Merge the templates in the scope into the primary IL scope. */
      /* Get a pointer to the last template in the primary scope. */
      if (pointers_block != NULL) {
        last_templ = pointers_block->last_template;
      } else {
        /* Actual last template will be determined below if/when needed. */
        last_templ = NULL;
      }  /* if */
      for (templ = scope->templates; templ != NULL; templ = templ->next) {
        a_template_ptr corresp_templ =
                  (a_template_ptr)checked_trans_unit_corresp_pointer_of(templ);
        check_assertion(!entry_to_be_merged(templ));
        /* An entry that had no correspondence. */
        if (is_class_scope && last_templ == NULL) {
          /* Determine the last template the first time it is needed. */
          last_templ = primary_scope->templates;
          if (last_templ != NULL) {
            while (last_templ->next != NULL) last_templ = last_templ->next;
          }  /* if */
        }  /* if */
        /* Add the template to the end of the list. */
        if (last_templ == NULL) {
          primary_scope->templates = corresp_templ;
        } else {
          last_templ->next = corresp_templ;
        }  /* if */
        corresp_templ->next = NULL;
        last_templ = corresp_templ;
        if (pointers_block != NULL) pointers_block->last_template = last_templ;
      }  /* for */
    }  /* if */
    if (scope->namespaces != NULL) {
      a_namespace_ptr nsp, last_nsp;
      /* Merge the namespaces in the scope into the primary IL scope. */
      check_assertion(pointers_block != NULL);  /* No namespaces in classes. */
      last_nsp = pointers_block->last_namespace;
      for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
        a_namespace_ptr corresp_nsp =
                   (a_namespace_ptr)checked_trans_unit_corresp_pointer_of(nsp);
        if (!entry_to_be_merged(nsp)) {
          /* An entry that had no correspondence.  Add it to the end of
             the list. */
          if (last_nsp == NULL) {
            primary_scope->namespaces = corresp_nsp;
          } else {
            last_nsp->next = corresp_nsp;
          }  /* if */
          corresp_nsp->next = NULL;
          last_nsp = corresp_nsp;
        }  /* if */
        pointers_block->last_namespace = last_nsp;
        if (!nsp->is_namespace_alias) {
          finish_trans_unit_copy(nsp->variant.assoc_scope);
        }  /* if */
      }  /* for */
    }  /* if */
    if (scope->pragmas != NULL) {
      a_pragma_ptr pragma, last_pragma;
      /* Merge the pragmas in the scope into the primary IL scope. */
      /* Get a pointer to the last pragma in the primary scope. */
      if (pointers_block != NULL) {
        last_pragma = pointers_block->last_pragma;
      } else {
        /* Actual last pragma will be determined below if/when needed. */
        last_pragma = NULL;
      }  /* if */
      for (pragma = scope->pragmas; pragma != NULL; pragma = pragma->next) {
        a_pragma_ptr corresp_pragma =
                   (a_pragma_ptr)checked_trans_unit_corresp_pointer_of(pragma);
        if (is_class_scope && last_pragma == NULL) {
          /* Determine the last pragma the first time it is needed. */
          last_pragma = primary_scope->pragmas;
          if (last_pragma != NULL) {
            while (last_pragma->next != NULL) last_pragma = last_pragma->next;
          }  /* if */
        }  /* if */
        /* Add the pragma to the end of the list. */
        if (last_pragma == NULL) {
          primary_scope->pragmas = corresp_pragma;
        } else {
          last_pragma->next = corresp_pragma;
        }  /* if */
        corresp_pragma->next = NULL;
        last_pragma = corresp_pragma;
        if (pointers_block != NULL) pointers_block->last_pragma = last_pragma;
      }  /* for */
    }  /* if */
    /* Merge the object lifetime from "scope" into that from
       "primary_scope". */
    merge_object_lifetimes(scope, primary_scope);
  }  /* if */
}  /* finish_trans_unit_copy */


static void merge_il_headers(void)
/*
Do merging of the il_header of the current secondary translation unit
into the primary translation unit il_header.
*/
{
  if (il_header.main_routine != NULL) {
    /* "main" is defined in the secondary translation unit.  Indicate
       that it is now defined in the primary translation unit. */
    check_assertion(translation_units->il_header.main_routine == NULL);
    translation_units->il_header.main_routine =
                  (a_routine_ptr)canonical_il_entry_of(il_header.main_routine);
  }  /* if */
}  /* merge_il_headers */


static void copy_instantiation_info_for_routine(a_routine_ptr routine)
/*
The indicated routine has been copied from the secondary translation
unit IL to the primary IL.  routine points to the copy in the
secondary translation unit.  Update any instantiation list information
associated with the routine.  Also handle the "instantiation" lists for
extern inline functions, if appropriate.
*/
{
  a_boolean     overwrite = entry_to_be_merged(routine);
  a_routine_ptr corresp_routine =
                 (a_routine_ptr)checked_trans_unit_corresp_pointer_of(routine);
  a_routine_ptr primary_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
  a_symbol_ptr  sym = (a_symbol_ptr)(routine->source_corresp.assoc_info);
  /* Note that if the routine was copied on top of an original routine
     in the primary IL the symbol pointer from the original entry was
     saved in the assoc_info field of the intermediate copy. */
  a_symbol_ptr  orig_sym = overwrite ?
                   (a_symbol_ptr)(corresp_routine->source_corresp.assoc_info) :
                   (a_symbol_ptr)NULL;

  /* This routine runs while switched to the primary translation unit. */
  check_assertion(is_primary_translation_unit);
  if (instantiate_extern_inline && routine->is_inline &&
      routine->storage_class == (a_storage_class)sc_unspecified) {
    /* extern inline functions are put on a list so they can be
       "instantiated".  If a function is both a template instance and
       extern inline, it goes on both lists. */
    if (overwrite && orig_sym->defined) {
      /* The corresponding routine already had a definition, so there is
         already a list entry for the routine in the primary IL. */
    } else {
      /* Add an entry for the routine. */
      add_to_inline_function_list(primary_routine);
    }  /* if */
  }  /* if */
  if (sym != NULL) {
    a_template_instance_ptr instance = sym->variant.routine.instance_ptr;
    if (instance != NULL) {
      a_template_instance_ptr copy_instance;
      a_template_instance_ptr saved_next = NULL;
      a_template_instance_ptr saved_next_in_instantiation_list = NULL;
      /* The routine is a template instance. */
      if (overwrite) {
        /* There is already a copy of this instance in the primary IL,
           which must have an associated template instance entry.  We
           will overwrite that instance entry. */
        check_assertion(orig_sym != NULL);
        copy_instance = orig_sym->variant.routine.instance_ptr;
        check_assertion(copy_instance != NULL);
        saved_next = copy_instance->next;
        saved_next_in_instantiation_list =
                     copy_instance->next_in_instantiation_list;
      } else {
        /* This is a new instance, for which there is no copy in the primary
           IL.  Create a new instantiation list entry by making a copy of the
           one from the secondary translation unit. */
        copy_instance = alloc_template_instance();
      }  /* if */
      *copy_instance = *instance;
      copy_instance->next = saved_next;
      copy_instance->next_in_instantiation_list =
                            saved_next_in_instantiation_list;
      copy_instance->referencing_namespace = NULL;
      if (!overwrite) add_to_instantiations_required_list(copy_instance);
    }  /* if */
  }  /* if */
}  /* copy_instantiation_info_for_routine */


static void wrap_up_moved_function(a_routine_ptr rout)
/*
rout identifies a function which has been moved from a secondary
translation unit to the primary translation unit IL.  Do final processing,
which includes IL lowering if appropriate.  rout points to the instance
of the routine in the secondary translation unit.
*/
{
  a_routine_ptr primary_rout = (a_routine_ptr)canonical_il_entry_of(rout);

  check_assertion(!in_secondary_trans_unit(primary_rout) &&
                  primary_rout->source_corresp.
                                             copied_from_secondary_trans_unit);
  /* If the routine is a template (or an extern inline function),
     copy instantiation information. */
  copy_instantiation_info_for_routine(rout);
  if (primary_rout->assoc_scope != NULL_region_number) {
    /* The routine body was moved. */
    a_scope_ptr scope= il_header.region_scope_entry[primary_rout->assoc_scope];
    check_assertion_str(scope != NULL, "wrap_up_moved_function: body missing");
    finish_function_body_processing(scope, /*after_copy=*/TRUE,
                                    /*discard_function_body=*/FALSE);
  }  /* if */
}  /* wrap_up_moved_function */


static void finish_moved_function_processing(a_scope_ptr scope)
/*
Finish processing in the indicated scope and its subscopes for any
functions whose bodies were moved from the secondary translation unit IL
to the primary IL.  This includes lowering if necessary.  The scope
passed in is from the secondary translation unit.
*/
{
  a_routine_ptr   routine;
  a_type_ptr      type;
  a_namespace_ptr nsp;

  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      finish_moved_function_processing(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  if (!C_mode()) {
    /* Look for class types and process their member functions. */
    for (type = scope->types; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          finish_moved_function_processing(class_scope);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    wrap_up_moved_function(routine);
  }  /* for */
}  /* finish_moved_function_processing */


void copy_secondary_trans_unit_IL_to_primary(void)
/*
Copy IL from the current translation unit, which is a secondary translation
unit, to the primary translation unit IL.  If needed flag processing
is configured in, unneeded entities have already been removed from the
secondary translation unit IL and therefore will not be copied.
*/
{
  a_scope_ptr            top_scope = il_header.primary_scope;
  a_boolean              any_removed_function_bodies = FALSE;
  a_translation_unit_ptr saved_translation_unit = curr_translation_unit;

  db_enter(1, "copy_secondary_trans_unit_IL_to_primary");
#if DEBUG
  if (debug_level >= 1) {
    fprintf(f_debug, "Beginning copy from secondary translation unit %s:\n",
            curr_translation_unit->source_file->name_as_written);
  }  /* if */
#endif /* DEBUG */
  check_assertion(total_errors == 0 && !is_primary_translation_unit);
  /* This code doesn't handle source sequence lists, so the result won't
     work with the C++-generating back end. */
  { a_boolean okay = !BACK_END_IS_CP_GEN_BE;
    check_assertion(okay);
  }
  check_assertion(!il_entry_prefix_of(top_scope).il_lowering_flag);
  initial_value_for_il_lowering_flag = FALSE;
  (void)prepare_for_trans_unit_copy(top_scope, &any_removed_function_bodies);
  copy_from_secondary_to_primary_IL();
  finish_trans_unit_copy(top_scope);
  /* Do final processing on moved function bodies.  This must be
     done in the context of the primary translation unit. */
  switch_translation_unit(translation_units);
  finish_moved_function_processing(top_scope);
  switch_translation_unit(saved_translation_unit);
  merge_il_headers();
#if DEBUG
  if (debug_level >= 1) {
    fprintf(f_debug, "Done with copy from secondary translation unit %s\n",
            curr_translation_unit->source_file->name_as_written);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* copy_secondary_trans_unit_IL_to_primary */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
