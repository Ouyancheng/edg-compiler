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
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */


/*
Value of il_walk_flag that means "needs processing" for the IL walk.
*/
static a_boolean il_walk_flag_value_meaning_needs_processing;


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
     the location where the entry should be copied. */
  char *canonical = canonical_il_entry_of(ptr);
  trans_unit_corresp_pointer_of(ptr) = canonical;
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
     is done at this time).  The il_walk_flag of the source entry
     is set to indicate that further processing (e.g., copying)
     of the entry is required.  Some entries do not have correspondence
     pointers (those in function scope memory regions and those
     in the primary translation unit IL), and for those nothing
     is done. */
  if (ptr == NULL) {
    /* Ignore NULL pointers. */
  } else if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so do nothing. */
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region, so remap its
       pointers. */
    il_entry_prefix_of(ptr).il_walk_flag =
                                   il_walk_flag_value_meaning_needs_processing;
  } else if (il_entry_prefix_of(ptr).il_walk_flag ==
                                 il_walk_flag_value_meaning_needs_processing) {
    /* This entry has already been encountered and the correspondence
       pointer has been set. */
  } else if (has_corresp(ptr)) {
    /* This entry has a correspondence in the primary IL. */
    if (entry_to_be_merged(ptr)) {
      /* This is an entry that gets merged into its corresponding entry. */
      char *corresp = trans_unit_corresp_pointer_of(ptr);
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
        trans_unit_corresp_pointer_of(ptr) = copy;
        trans_unit_corresp_pointer_of(copy) = corresp;
        check_assertion(!is_string_entry_kind(kind));
        il_entry_prefix_of(ptr).il_walk_flag =
                                   il_walk_flag_value_meaning_needs_processing;
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
      trans_unit_corresp_pointer_of(ptr) = copy;
      /* Make the canonical entry for this entry point to the copy also if
         it's in a secondary translation unit. */
      if (canonical != ptr && in_secondary_trans_unit(canonical)) {
        trans_unit_corresp_pointer_of(canonical) = copy;
      }  /* if */
      il_entry_prefix_of(ptr).il_walk_flag =
                                   il_walk_flag_value_meaning_needs_processing;
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
  } else if (il_entry_prefix_of(ptr).il_walk_flag ==
                                !il_walk_flag_value_meaning_needs_processing) {
    /* This entry does not need any (more) processing. */
    prune = TRUE;
  } else {
    /* This entry still needs to be processed (i.e., copied and remapped). */
    il_entry_prefix_of(ptr).il_walk_flag =
                                  !il_walk_flag_value_meaning_needs_processing;
    prune = FALSE;
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
  char *copy = alloc_primary_file_scope_il(length);

  trans_unit_corresp_pointer_of(ptr) = copy;
  (void)memcpy(copy, ptr, length);
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
  if (!in_file_scope(ptr)) {
    /* Process an entry in a function scope memory region.  Remap
       the pointers but don't copy. */
    remap_pointers_in_entry(ptr, kind);
  } else {
    char                    *copy = trans_unit_corresp_pointer_of(ptr);
    a_source_correspondence *scp;

    check_assertion_str(copy != NULL,
                        "copy_entry: NULL correspondence pointer");
    /* Copy the entry to its corresponding space and remap the pointers
       in the copy. */
    (void)memcpy(copy, ptr, size_t_arg(sizeof_il_entry[(int)kind]));
    remap_pointers_in_entry(copy, kind);
    scp = source_corresp_for_il_entry(ptr, kind);
    if (scp != NULL) scp->copied_from_secondary_trans_unit = TRUE;
  }  /* if */
}  /* copy_entry */


static void clear_variable_initialization(a_variable_ptr variable)
/*
Eliminate the initialization of the indicated variable to turn it
into a declaration instead of a definition.
*/
{
  variable->init_kind = (an_init_kind)initk_none;
  if (variable->storage_class == (a_storage_class)sc_unspecified) {
    variable->storage_class = (a_storage_class)sc_extern;
  }  /* if */
  /* Note that the dynamic initialization for this variable, if any,
     will be removed from the scope dynamic_inits list later. */
}  /* clear_variable_initialization */


static void prepare_for_trans_unit_copy(a_scope_ptr scope)
/*
Scan the indicated scope (a file or namespace scope in a secondary
translation unit) and its subscopes and set up for copying the scope
to the primary translation unit IL.
*/
{
  a_type_ptr         type, prev_type;
  a_variable_ptr     variable, prev_variable;
  a_dynamic_init_ptr dyn_init, prev_dyn_init;
  a_routine_ptr      routine, prev_routine;
  a_template_ptr     templ, prev_templ;
  a_namespace_ptr    nsp;
  a_boolean          keep_on_list;

  /* Set the correspondence for this scope, if any. */
  if (scope->kind == (a_scope_kind)sck_file) {
    /* The file scope in a secondary translation unit corresponds to
       the file scope in the primary translation unit, and gets merged
       into it. */
    a_scope_ptr corresp_scope = translation_units->primary_scope;
    trans_unit_corresp_pointer_of(scope) = (char *)corresp_scope;
    mark_to_merge(scope);
    if (scope->lifetime != NULL && corresp_scope->lifetime != NULL) {
      /* The object lifetime of the file scope corresponds with the
         object lifetime of the corresponding scope, and gets merged into
         it. */
      trans_unit_corresp_pointer_of(scope->lifetime) =
                                               (char *)corresp_scope->lifetime;
      mark_to_merge(scope->lifetime);
    }  /* if */
  } else {
    /* For a namespace scope, go to the a_namespace entry to find out what
       correspondence there is, if any. */
    nsp = scope->variant.assoc_namespace;
    if (has_corresp(nsp)) {
      /* The namespace scope gets merged into the corresponding scope. */
      a_namespace_ptr corresp_nsp= (a_namespace_ptr)canonical_il_entry_of(nsp);
      a_scope_ptr     corresp_nsp_scope = corresp_nsp->variant.assoc_scope;
      trans_unit_corresp_pointer_of(scope) = (char *)corresp_nsp_scope;
      mark_to_merge(scope);
    }  /* if */
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
    if (has_corresp(type)) {
      /* This entry corresponds to something in the primary IL. */
      a_type_ptr corresp_type = (a_type_ptr)canonical_il_entry_of(type);
      keep_on_list = FALSE;
      if ((is_immediate_class_type(type) &&
           class_type_has_body(type) &&
           !class_type_has_body(corresp_type)) ||
          (is_immediate_enum_type(type) &&
           !is_incomplete_type(type) &&
           is_incomplete_type(corresp_type))) {
        /* This type is a struct, union, class, or enum with a definition,
           and the corresponding type has no definition.  Therefore the
           definition must be merged into the corresponding type. */
        mark_to_merge(type);
        keep_on_list = TRUE;
      }  /* if */
    }  /* if */
    if (keep_on_list) {
      prev_type = type;
    } else {
      /* Remove this entry from the list. */
      if (prev_type == NULL) {
        scope->types = type->next;
      } else {
        prev_type->next = type->next;
      }  /* if */
    }  /* if */
  }  /* for */
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
      if (variable->init_kind != (an_init_kind)initk_none) {
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
          if (variable->storage_class == (a_storage_class)sc_static) {
            /* A static variable referenced from a template is changed to an
               external declaration and copied over. */
#if 0
            /* FIXME */
#endif /* 0 */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (keep_on_list) {
      prev_variable = variable;
    } else {
      /* Remove this entry from the list. */
      if (prev_variable == NULL) {
        scope->variables = variable->next;
      } else {
        prev_variable->next = variable->next;
      }  /* if */
    }  /* if */
  }  /* for */
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
  /* Visit all routines. */
  prev_routine = NULL;
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    keep_on_list = TRUE;
    if (has_corresp(routine)) {
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (routine->assoc_scope != NULL_region_number) {
        /* This routine has a body. */
        a_routine_ptr corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
        /* Copy over the definition unless both instances of the routine
           have definitions (that can happen for inline functions). */
        if (corresp_routine->assoc_scope != NULL_region_number) {
          /* Both instances have definitions.  Eliminate the body of this
             function and do not copy over the function. */
          a_scope_ptr routine_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
          check_assertion(routine_scope != NULL);
          clear_function_body(routine_scope);
        } else {
          /* Merge the definition here into the corresponding routine. */
          mark_to_merge(routine);
          keep_on_list = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* This routine has no correspondence in the primary file IL. */
      if (translation_unit_needed_only_for_exported_templates) {
        /* We're supposed to copy only generated templates.  Other routines
           are made external (if necessary) and their definitions are
           dropped (the definition will be put out when the file
           is compiled as a primary file). */
        if (!routine->is_template_function || routine->is_specialized) {
          /* The function is not a generated template. */
          if (routine->assoc_scope != NULL_region_number) {
            /* Eliminate the body of this function. */
            a_scope_ptr routine_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
            check_assertion(routine_scope != NULL);
            clear_function_body(routine_scope);
          }  /* if */
          if (routine->storage_class == (a_storage_class)sc_static) {
            /* A static function referenced from a template is changed to an
               external declaration and copied over. */
#if 0
            /* FIXME */
#endif /* 0 */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (keep_on_list) {
      prev_routine = routine;
    } else {
      /* Remove this entry from the list. */
      if (prev_routine == NULL) {
        scope->routines = routine->next;
      } else {
        prev_routine->next = routine->next;
      }  /* if */
    }  /* if */
  }  /* for */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (scope->kind == (a_scope_kind)sck_file) {
    /* Remove scope orphaned list entries for eliminated functions. */
    a_scope_orphaned_list_header_ptr solhp, prev_solhp = NULL;
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      if (solhp->assoc_routine->assoc_scope == NULL_region_number) {
        /* Remove this entry. */
        if (prev_solhp == NULL) {
          il_header.scope_orphaned_list_headers = solhp->next;
        } else {
          prev_solhp->next = solhp->next;
        }  /* if */
      } else {
        prev_solhp = solhp;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
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
    } else {
      /* Remove this entry from the list. */
      if (prev_templ == NULL) {
        scope->templates = templ->next;
      } else {
        prev_templ->next = templ->next;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    /* Entities with correspondences don't get copied; they get merged
       into the corresponding entry.  Set a flag to indicate that. */
    if (has_corresp(nsp)) {
      mark_to_merge(nsp);
    }  /* if */
    if (!nsp->is_namespace_alias) {
      prepare_for_trans_unit_copy(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
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
    lifetime =
        (an_object_lifetime_ptr)trans_unit_corresp_pointer_of(scope->lifetime);
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
        while (last_destr->next != NULL) last_destr = last_destr->next;
        last_destr->next = primary_lifetime->destructions;
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


static void finish_trans_unit_copy(a_scope_ptr scope)
/*
scope is a file or namespace scope from the secondary file IL.  Do
processing required after the IL walk to copy IL entries from the
secondary scope to the primary file IL.
*/
{
  a_scope_ptr            primary_scope;
  a_scope_pointers_block *pointers_block = NULL;

  /* Process only scopes that must be merged into their counterparts. */
  if (entry_to_be_merged(scope)) {
    /* Find the corresponding scope. */
    primary_scope = (a_scope_ptr)trans_unit_corresp_pointer_of(scope);
    /* For the file scope, we will be using and updating the end pointers
       in the pointers block. */
    if (scope->kind == (a_scope_kind)sck_file) {
      pointers_block = &translation_units->file_scope_pointers_block;
    }  /* if */
    if (scope->types != NULL) {
      a_type_ptr type, last_type;
      /* Merge the types in the scope into the primary IL scope. */
      /* Get a pointer to the last entry in the primary IL scope. */
      if (pointers_block != NULL) {
        last_type = pointers_block->last_type;
      } else {
        last_type = primary_scope->types;
        if (last_type != NULL) {
          while (last_type->next != NULL) last_type = last_type->next;
        }  /* if */
      }  /* if */
      for (type = scope->types; type != NULL; type = type->next) {
        a_type_ptr corresp_type =
                               (a_type_ptr)trans_unit_corresp_pointer_of(type);
        if (!entry_to_be_merged(type)) {
          /* An entry that had no correspondence.  Add it to the end of
             the list. */
          if (last_type == NULL) {
            primary_scope->types = corresp_type;
          } else {
            last_type->next = corresp_type;
          }  /* if */
          corresp_type->next = NULL;
          last_type = corresp_type;
        } else {
          /* Merge the information from this type into the primary IL type
             (the secondary translation unit instance has a definition and
             the primary translation unit instance does not). */
          a_type_ptr primary_type =
                       (a_type_ptr)trans_unit_corresp_pointer_of(corresp_type);
          corresp_type->next = primary_type->next;
          *primary_type = *corresp_type;
        }  /* if */
      }  /* for */
    }  /* if */
    if (scope->variables != NULL) {
      a_variable_ptr variable, last_variable;
      /* Merge the variables in the scope into the primary IL scope. */
      /* Get a pointer to the last entry in the primary IL scope. */
      if (pointers_block != NULL) {
        last_variable = pointers_block->last_variable;
      } else {
        last_variable = primary_scope->variables;
        if (last_variable != NULL) {
          while (last_variable->next != NULL) {
            last_variable = last_variable->next;
          }  /* while */
        }  /* if */
      }  /* if */
      for (variable = scope->variables;
           variable != NULL;
           variable = variable->next) {
        a_variable_ptr corresp_variable =
                       (a_variable_ptr)trans_unit_corresp_pointer_of(variable);
        if (!entry_to_be_merged(variable)) {
          /* An entry that had no correspondence.  Add it to the end of
             the list. */
          if (last_variable == NULL) {
            primary_scope->variables = corresp_variable;
          } else {
            last_variable->next = corresp_variable;
          }  /* if */
          corresp_variable->next = NULL;
          last_variable = corresp_variable;
        } else {
          /* Merge the information from this variable into the primary IL
             variable (the secondary translation unit instance has a definition
             and the primary translation unit instance does not). */
          a_variable_ptr primary_variable =
               (a_variable_ptr)trans_unit_corresp_pointer_of(corresp_variable);
          corresp_variable->next = primary_variable->next;
          *primary_variable = *corresp_variable;
          corresp_variable = primary_variable;
        }  /* if */
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
        if (pointers_block != NULL) {
          last_dyn_init = pointers_block->last_dynamic_init;
        } else {
          while (last_dyn_init->next != NULL) {
            last_dyn_init = last_dyn_init->next;
          }  /* while */
        }  /* if */
        last_dyn_init->next = copied_inits;
      }  /* if */
      if (pointers_block != NULL) {
        last_dyn_init = copied_inits;
        while (last_dyn_init->next != NULL) {
          last_dyn_init = last_dyn_init->next;
        }  /* while */
        pointers_block->last_dynamic_init = last_dyn_init;
      }  /* if */
    }  /* if */
    if (scope->routines != NULL) {
      a_routine_ptr routine, last_routine;
      /* Merge the routines in the scope into the primary IL scope. */
      /* Get a pointer to the last entry in the primary IL scope. */
      if (pointers_block != NULL) {
        last_routine = pointers_block->last_routine;
      } else {
        last_routine = primary_scope->routines;
        if (last_routine != NULL) {
          while (last_routine->next != NULL) last_routine = last_routine->next;
        }  /* if */
      }  /* if */
      for (routine = scope->routines;
           routine != NULL;
           routine = routine->next) {
        a_routine_ptr corresp_routine =
                         (a_routine_ptr)trans_unit_corresp_pointer_of(routine);
        if (!entry_to_be_merged(routine)) {
          /* An entry that had no correspondence.  Add it to the end of
             the list. */
          if (last_routine == NULL) {
            primary_scope->routines = corresp_routine;
          } else {
            last_routine->next = corresp_routine;
          }  /* if */
          corresp_routine->next = NULL;
          last_routine = corresp_routine;
        } else {
          /* Merge the information from this routine into the primary IL
             routine (the secondary translation unit instance has a
             definition and the primary translation unit instance does not). */
          a_routine_ptr primary_routine =
                 (a_routine_ptr)trans_unit_corresp_pointer_of(corresp_routine);
          corresp_routine->next = primary_routine->next;
          *primary_routine = *corresp_routine;
          corresp_routine = primary_routine;
        }  /* if */
        if (corresp_routine->assoc_scope != NULL) {
          a_scope_ptr rout_scope =
                    il_header.region_scope_entry[corresp_routine->assoc_scope];
          /* For a routine with a body, the code in the function scope memory
             region needs to be processed too.  It doesn't need to be
             copied, but the pointers need to be remapped. */
          /* Make sure the same il_walk_flag value can be used for the function
             scope as was used for the file scope.  This would not be true
             if an extra IL walk of either region has been done. */
          check_assertion(il_walk_flag_value_meaning_needs_processing ==
                          !il_entry_prefix_of(rout_scope).il_walk_flag);
          walk_routine_scope_il(corresp_routine->assoc_scope,
                                copy_entry,
                                copy_string_entry,
                                (a_remap_function_ptr)NULL,
                                copy_termination_test,
                                /*clear_fe_pointers=*/FALSE);
          rout_scope->part_of_secondary_trans_unit = FALSE;
#if MAINTAIN_NEEDED_FLAGS
          if (routine_needed_even_if_unreferenced(corresp_routine)) {
            /* Mark an externally-defined routine as "needed". */
            mark_as_needed((char *)corresp_routine,
                           (an_il_entry_kind)iek_routine);
          }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
#if DO_IL_LOWERING
          /* Lower the code in the function. */
          lower_il_memory_region(corresp_routine->assoc_scope);
#endif /* DO_IL_LOWERING */
        }  /* if */
      }  /* for */
    }  /* if */
    if (scope->templates != NULL) {
      a_template_ptr templ, last_templ;
      /* Merge the templates in the scope into the primary IL scope. */
      /* Get a pointer to the last entry in the primary IL scope. */
      if (pointers_block != NULL) {
        last_templ = pointers_block->last_template;
      } else {
        last_templ = primary_scope->templates;
        if (last_templ != NULL) {
          while (last_templ->next != NULL) last_templ = last_templ->next;
        }  /* if */
      }  /* if */
      for (templ = scope->templates; templ != NULL; templ = templ->next) {
        a_template_ptr corresp_templ =
                          (a_template_ptr)trans_unit_corresp_pointer_of(templ);
        check_assertion(!entry_to_be_merged(templ));
        /* An entry that had no correspondence.  Add it to the end of
           the list. */
        if (last_templ == NULL) {
          primary_scope->templates = corresp_templ;
        } else {
          last_templ->next = corresp_templ;
        }  /* if */
        corresp_templ->next = NULL;
        last_templ = corresp_templ;
      }  /* for */
    }  /* if */
    if (scope->namespaces != NULL) {
      a_namespace_ptr nsp, last_nsp;
      /* Merge the namespaces in the scope into the primary IL scope. */
      /* Get a pointer to the last entry in the primary IL scope. */
      if (pointers_block != NULL) {
        last_nsp = pointers_block->last_namespace;
      } else {
        last_nsp = primary_scope->namespaces;
        if (last_nsp != NULL) {
          while (last_nsp->next != NULL) last_nsp = last_nsp->next;
        }  /* if */
      }  /* if */
      for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
        a_namespace_ptr corresp_nsp =
                          (a_namespace_ptr)trans_unit_corresp_pointer_of(nsp);
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
        if (!nsp->is_namespace_alias) {
          finish_trans_unit_copy(nsp->variant.assoc_scope);
        }  /* if */
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
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
  if (il_header.scope_orphaned_list_headers != NULL) {
    /* Add the scope orphaned list headers from "scope" to the end of the
       scope orphaned list headers list of "primary scope". */
    a_scope_orphaned_list_header_ptr last_solhp =
                            translation_units->last_scope_orphaned_list_header;
    if (last_solhp == NULL) {
     translation_units->il_header.scope_orphaned_list_headers =
                                         il_header.scope_orphaned_list_headers;
    } else {
      last_solhp->next = il_header.scope_orphaned_list_headers;
    }  /* if */
    last_solhp = il_header.scope_orphaned_list_headers;
    while (last_solhp->next != NULL) last_solhp = last_solhp->next;
    translation_units->last_scope_orphaned_list_header = last_solhp;
  }  /* if */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  if (il_header.main_routine != NULL) {
    /* "main" is defined in the secondary translation unit.  Indicate
       that it is now defined in the primary translation unit. */
    check_assertion(translation_units->il_header.main_routine == NULL);
    translation_units->il_header.main_routine =
                  (a_routine_ptr)canonical_il_entry_of(il_header.main_routine);
  }  /* if */
}  /* merge_il_headers */


static void copy_from_secondary_to_primary_IL(void)
/*
Copy everything from the current secondary translation unit IL to the
primary translation unit IL.
*/
{
  walk_file_scope_il(copy_entry, copy_string_entry,
                     (a_remap_function_ptr)NULL,
                     copy_termination_test,
                     /*clear_fe_pointers=*/FALSE);
}  /* copy_from_secondary_to_primary_IL */


void copy_secondary_trans_unit_IL_to_primary(void)
/*
Copy IL from the current translation unit, which is a secondary translation
unit, to the primary translation unit IL.  If needed flag processing
is configured in, unneeded entities have already been removed from the
secondary translation unit IL and therefore will not be copied.
*/
{
  a_scope_ptr primary_scope = il_header.primary_scope;

  check_assertion(total_errors == 0 && !is_primary_translation_unit);
  check_assertion(!il_entry_prefix_of(primary_scope).il_lowering_flag);
  initial_value_for_il_lowering_flag = FALSE;
  il_walk_flag_value_meaning_needs_processing =
                               !il_entry_prefix_of(primary_scope).il_walk_flag;
  prepare_for_trans_unit_copy(primary_scope);
  copy_from_secondary_to_primary_IL();
  finish_trans_unit_copy(primary_scope);
  merge_il_headers();
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
