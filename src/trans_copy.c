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
#endif /* DO_IL_LOWERING */


static void f_mark_to_merge(char *ptr)
/*
Mark the given entry as one that must be merged with its counterpart
in the primary IL.
*/
{
  /* Make the correspondence pointer go directly to the canonical
     entry if it is currently a multi-step chain.  This makes it
     convenient to deal with in the later processing, because the
     original entry points to the copy which points to the
     corresponding primary IL entry, with no extra steps. */
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

  
/*
Provide access to the flag of an entry that indicates that
the entry's correspondence pointer has been set to point to space into
which the entry will be or has been copied.  This macro can be used to
fetch or set the flag.  The entry_written flag can be reused for
this purpose because secondary translation units are never written
to an IL file.
*/
#define entry_copy_address_assigned(ptr) \
  (il_entry_prefix_of(ptr).entry_written)


static a_boolean f_has_corresp(char *ptr)
/*
Return TRUE if the indicated entry has a corresponding address assigned
in the primary IL, either by trans_corresp.c or because of an assigned
copy address.
*/
{
  a_boolean has_corr;
  char      *new_ptr;

  for (;;) {
    new_ptr = checked_trans_unit_corresp_pointer_of(ptr);
    if (new_ptr == NULL || new_ptr == ptr) {
      /* A NULL pointer, or an entry pointing to itself, is the end of
         list, with no correspondence. */
      has_corr = FALSE;
      break;
    }  /* if */
    ptr = new_ptr;
    if (!in_secondary_trans_unit(ptr)) {
      /* We made it to the primary IL, so this pointer does have a
         corresponding primary IL address. */
      has_corr = TRUE;
      break;
    }  /* if */
  }  /* for */
  return has_corr;
}  /* f_has_corresp */

/*
Macro interface to f_has_corresp, which allows it to be called for
entries of various kinds.
*/
#define has_corresp(entry) f_has_corresp((char *)(entry))

/*
Return TRUE if an entry has a correspondence in the primary IL
with which the entry may have to be merged.  If the entry has
a copy address assigned, then the corresponding entry is just
the destination of the copy, and not another entry for which
a merge should be considered.  Note that if it's necessary to
use this macro after preassign_copy_address is called it would
be necessary to test the merge flag as well.
*/
#define has_corresp_that_may_require_merge(ptr) \
  (has_corresp(ptr) && !entry_copy_address_assigned(ptr))


/*
Return TRUE if the given entry has the flag set that indicates that
it needs to be copied.  The il_walk_flag is used for this purpose.
*/
#define entry_needs_copy_flag_is_set(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag == flag_value_meaning_visited)

/*
Set the flag that indicates that an entry needs to be copied.
*/
#define set_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = flag_value_meaning_visited)

/*
Reset the flag that indicates that an entry needs to be copied.
*/
#define reset_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = !flag_value_meaning_visited)


static a_boolean in_other_secondary_trans_unit(char             *ptr,
                                               an_il_entry_kind kind,
                                               a_boolean        *known)
/*
Return TRUE if the indicated IL entry is in a different secondary translation
unit.  This is accurately determined only in some cases, including for
declarative entries with associated symbols.  For the other cases, FALSE
is returned, and *known is returned FALSE.
*/
{
  a_boolean in_other_trans_unit = FALSE;

  *known = FALSE;
  if (!in_secondary_trans_unit(ptr)) {
    /* If the pointer is not in a secondary translation unit, it can't be
       in a different secondary translation unit. */
    *known = TRUE;
  } else if (kind == (an_il_entry_kind)iek_expr_node ||
             kind == (an_il_entry_kind)iek_statement ||
             kind == (an_il_entry_kind)iek_object_lifetime) {
    /* Certain kinds of entries are always in the current translation unit. */
    *known = TRUE;
  } else {
    a_source_correspondence *scp = source_corresp_for_il_entry(ptr, kind);
    if (scp != NULL) {
      if (scp->is_local_to_function) {
        /* Function-local entities can't be from another translation unit.
           This is tested early this way because the scope for local
           entities might already have been moved to the primary IL. */
        *known = TRUE;
      } else {
        a_symbol_ptr sym = (a_symbol_ptr)(scp->assoc_info);
        if (sym != NULL) {
          *known = TRUE;
          if (sym->decl_scope != NO_SCOPE_NUMBER &&
              trans_unit_for_scope[sym->decl_scope] != curr_translation_unit) {
            /* This entity is from a different secondary translation unit. */
            in_other_trans_unit = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return in_other_trans_unit;
}  /* in_other_secondary_trans_unit */


static void corresp_setup(char             *ptr,
                          an_il_entry_kind kind,
                          a_boolean        known_in_curr_trans_unit,
                          a_boolean        known_will_process_in_curr_walk)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit to set up the correspondence
pointer of the entry pointed to by ptr, of kind "kind".
known_in_curr_trans_unit is TRUE if it is known that the entry is
in the current (secondary) translation unit.  known_will_process_in_curr_walk
is TRUE if it is known that the entry has been or will be processed
(and not merely have its address remapped) in the current IL walk.
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
  } else if (entry_needs_copy_flag_is_set(ptr)) {
    /* This entry has already been encountered and the correspondence
       pointer has been set, and we're awaiting copying. */
  } else if (entry_copy_address_assigned(ptr)) {
    /* A copy address has already been assigned to this entry. */
  } else if (has_corresp(ptr)) {
    /* This entry has a correspondence in the primary IL, either because
       one was assigned by trans_corresp or because a copy address
       was assigned to some corresponding entry.  Either way, the entry
       already has a mapping in the primary IL. */
    if (entry_to_be_merged(ptr)) {
      /* This is an entry that gets merged into its corresponding entry. */
      char *corresp = checked_trans_unit_corresp_pointer_of(ptr);
      /* Make a copy, so we will have a version with all the pointers
         remapped appropriately.  The original entry points to the
         copy, which points to the canonical entry.  This allows us to get
         to the copy via trans_unit_corresp_pointer_of, while ensuring
         that references to the original entry are remapped to the
         canonical entry (because canonical_il_entry_of loops through to
         the end of the list).  Note that the copy is in the
         secondary translation unit file scope memory region. */
      char *copy = alloc_il(sizeof_il_entry[(int)kind]);
      check_assertion(!is_string_entry_kind(kind));
      checked_trans_unit_corresp_pointer_of(ptr) = copy;
      checked_trans_unit_corresp_pointer_of(copy) = corresp;
      /* Set the flag to indicate that a copy address has been assigned.
         Note that that prevents us from getting to the code here again. */
      entry_copy_address_assigned(ptr) = TRUE;
      /* Set the flag to request copying. */
      set_entry_needs_copy_flag(ptr);
#if DEBUG
      if (db_trace("trans_copy", ptr, kind)) {
        fprintf(f_debug, "assigned addr for copy in secondary at %lx:\n",
                         (unsigned long)copy);
        db_entity_info(ptr, kind);
      }  /* if */
#endif /* DEBUG */
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
      /* Set the flag to indicate that a copy address has been assigned. */
      entry_copy_address_assigned(ptr) = TRUE;
      /* Set the flag to request copying. */
      set_entry_needs_copy_flag(ptr);
#if DEBUG
      if (db_trace("trans_copy", ptr, kind)) {
        fprintf(f_debug, "assigned addr for copy to primary at %lx:\n",
                         (unsigned long)copy);
        db_entity_info(ptr, kind);
      }  /* if */
#endif /* DEBUG */
      if (!known_will_process_in_curr_walk) {
        /* We don't know for sure that the entry will be processed in the
           current IL walk.  It might be from another secondary translation
           unit.  Unless we know it's from the current translation unit,
           do the copy now to be sure. */
        a_boolean known;
        if (known_in_curr_trans_unit ||
            (!in_other_secondary_trans_unit(ptr, kind, &known) && known)) {
          /* The entity is in the current translation unit, so it will
             get copied in the current IL walk. */
          if (!walking_file_scope) {
            /* A reference from a function scope to the file scope.  Make sure
               we come back to this entry if it's an orphan. */
            add_orphaned_file_scope_il_entry(ptr, kind);
          }  /* if */
        } else {
          /* We don't know for sure whether the entry is in the current
             translation unit, so copy it now to ensure that it will get
             copied. */
          /* Clear the walk_remap_function around the call. */
          a_remap_function_ptr saved_walk_remap_func = walk_remap_func;
          walk_remap_func = NULL;
          walk_entry_and_subtree(ptr, kind);
          walk_remap_func = saved_walk_remap_func;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* corresp_setup */


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
  corresp_setup(ptr, kind,
                /*known_in_curr_trans_unit=*/FALSE,
                /*known_will_process_in_curr_walk=*/TRUE);
  if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so stop and don't process
       it. */
    prune = TRUE;
  } else if (kind == iek_based_type_list_member) {
    /* All based type lists get removed. */
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
       unit. */
    if (!entry_needs_copy_flag_is_set(ptr)) {
      /* This entry does not need any (more) processing. */
      prune = TRUE;
    } else {
      /* This entry still needs to be processed (i.e., copied and remapped). */
      reset_entry_needs_copy_flag(ptr);
      prune = FALSE;
    }  /* if */
  }  /* if */
  return prune;
}  /* copy_termination_test */


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
  corresp_setup(ptr, kind,
                /*known_in_curr_trans_unit=*/FALSE,
                /*known_will_process_in_curr_walk=*/FALSE);
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
    /* Set the flag to indicate that a copy address has been assigned. */
    entry_copy_address_assigned(ptr) = TRUE;
    (void)memcpy(copy, ptr, size_t_arg(length));
  }  /* if */
}  /* copy_string_entry */


static void copy_entry_basic(char                 *ptr,
                             an_il_entry_kind     kind,
                             a_remap_function_ptr remap_function)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the IL entry at ptr
(of kind "kind") to the space indicated by its correspondence pointer,
and remap the pointers in the copy by calling remap_function.
*/
{
  a_source_correspondence *scp = NULL;
  char                    *copy;
  a_remap_function_ptr    saved_walk_remap_func = walk_remap_func;

  if (!in_file_scope(ptr)) {
    /* Process an entry in a function scope memory region.  Remap
       the pointers but don't copy. */
    walk_remap_func = remap_function;
    remap_pointers_in_il_entry(ptr, kind);
    walk_remap_func = saved_walk_remap_func;
#if MAINTAIN_NEEDED_FLAGS
    copy = ptr;
    scp = source_corresp_for_il_entry(copy, kind);
#endif /* MAINTAIN_NEEDED_FLAGS */
  } else {
    copy = checked_trans_unit_corresp_pointer_of(ptr);
    check_assertion_str(copy != NULL,
                        "copy_entry_basic: NULL correspondence pointer");
    /* Copy the entry to its corresponding space and remap the pointers
       in the copy. */
    (void)memcpy(copy, ptr, size_t_arg(sizeof_il_entry[(int)kind]));
    if (kind == iek_type) {
      a_type_ptr type = (a_type_ptr)copy;
      /* Based type lists don't get copied. */
      type->based_types = NULL;
    }  /* if */
    walk_remap_func = remap_function;
    remap_pointers_in_il_entry(copy, kind);
    walk_remap_func = saved_walk_remap_func;
    scp = source_corresp_for_il_entry(copy, kind);
    if (scp != NULL) scp->copied_from_secondary_trans_unit = TRUE;
#if DEBUG
    if (db_trace("trans_copy", ptr, kind)) {
      fprintf(f_debug, "copying from secondary to %lx:\n",
                       (unsigned long)copy);
      db_entity_info(ptr, kind);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
#if MAINTAIN_NEEDED_FLAGS
  /* Clear the needed and keep_in_il flags in the copy (or original,
     for an entry in a file scope memory region), so that they can be
     recomputed in the context of the primary IL. */
  il_entry_prefix_of(copy).keep_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  if (scp != NULL) {
#if MAINTAIN_NEEDED_FLAGS
    scp->needed = FALSE;
#if ONE_INSTANTIATION_PER_OBJECT
    scp->per_instantiation_needed_flags = NULL;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* MAINTAIN_NEEDED_FLAGS */
    if (kind == iek_type) {
#if MAINTAIN_NEEDED_FLAGS
      a_type_ptr type = (a_type_ptr)copy;
      if (is_immediate_class_type(type)) {
        type->variant.class_struct_union.definition_needed = FALSE;
        type->variant.class_struct_union.keep_definition_in_il = FALSE;
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    } else if (kind == iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)copy;
#if MAINTAIN_NEEDED_FLAGS
      rout->definition_needed = FALSE;
      rout->keep_definition_in_il = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
      rout->on_inline_function_list = FALSE;
    }  /* if */
  }  /* if */
}  /* copy_entry_basic */


static void copy_entry(char             *ptr,
                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the IL entry at ptr
(of kind "kind") to the space indicated by its correspondence pointer,
and remap the pointers in the copy.
*/
{
  copy_entry_basic(ptr, kind, remap_secondary_ptr_to_primary);
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


static void move_routine_body_to_primary(a_routine_ptr routine)
/*
Move the body of the indicated routine to the primary IL by walking
it and remapping pointers.
*/
{
  a_scope_ptr scope;

  check_assertion(in_secondary_trans_unit(routine) &&
                  routine->assoc_scope != NULL_region_number);
  scope = il_header.region_scope_entry[routine->assoc_scope];
  check_assertion(scope != NULL);
  walk_routine_scope_il(routine->assoc_scope,
                        copy_entry,
                        copy_string_entry,
                        (a_remap_function_ptr)NULL,
                        copy_termination_test,
                        /*clear_fe_pointers=*/FALSE);
  scope->function_body_processing_finished = FALSE;
}  /* move_routine_body_to_primary */


static void copy_function_bodies_from_secondary_to_primary_IL(
                                                            a_scope_ptr scope);


static void copy_type_list_function_bodies_from_secondary_to_primary_IL(
                                                          a_type_ptr type_list)
/*
Copy the bodies of any functions on the indicated type list to the
primary translation unit IL.
*/
{
  a_type_ptr type;

  if (!C_mode()) {
    for (type = type_list; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          copy_function_bodies_from_secondary_to_primary_IL(class_scope);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* copy_type_list_function_bodies_from_secondary_to_primary_IL */


static void copy_function_bodies_from_secondary_to_primary_IL(
                                                             a_scope_ptr scope)
/*
Copy the bodies of any functions in the indicated scope (a file,
namespace, or class scope in a secondary translation unit) to the
primary translation unit IL.
*/
{
  a_routine_ptr   routine;
  a_namespace_ptr nsp;

  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    if (routine->assoc_scope != NULL_region_number) {
      /* Move the routine body to the primary IL. */
      a_scope_ptr rout_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
      check_assertion_str(rout_scope != NULL,
            "copy_function_bodies_from_secondary_to_primary_IL: body missing");
      /* Handle local classes (and their member functions). */
      copy_type_list_function_bodies_from_secondary_to_primary_IL(
                                                            rout_scope->types);
      move_routine_body_to_primary(routine);
    }  /* if */
  }  /* for */
  copy_type_list_function_bodies_from_secondary_to_primary_IL(scope->types);
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      copy_function_bodies_from_secondary_to_primary_IL(
                                                     nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* copy_function_bodies_from_secondary_to_primary_IL */


static void remove_dynamic_initialization(a_dynamic_init_ptr dip)
/*
Remove the indicated dynamic initialization from any initialization
and destruction lists.  Also remove any nested object lifetimes.
*/
{
  a_variable_ptr         variable = dip->variable;
  an_object_lifetime_ptr lifetime;

  check_assertion(variable != NULL);
  lifetime = init_expr_lifetime_of(dip);
  if (lifetime != NULL) {
    /* There is a nested object lifetime.  Eliminate it and everything in
       it. */
    detach_from_object_lifetime_tree(lifetime);
    dip->init_expr_lifetime = NULL;  /* To be neat. */
  }  /* if */
  remove_from_destruction_list(dip);
  if (!variable->source_corresp.is_local_to_function) {
    /* Remove the dynamic initialization from the file-scope initializations
       list. */
    a_dynamic_init_ptr prev_dip;
    a_scope_ptr        sp = il_header.primary_scope;
    a_scope_pointers_block_ptr
                       pointers_block =
                             &curr_translation_unit->file_scope_pointers_block;
    if (dip == sp->dynamic_inits) {
      /* The entry is first on the list. */
      sp->dynamic_inits = dip->next;
      prev_dip = NULL;
    } else {
      /* The entry is not the first on the list. */
      for (prev_dip = sp->dynamic_inits;
           ;
           prev_dip = prev_dip->next) {
        check_assertion_str(prev_dip != NULL,
                            "remove_dynamic_initialization: entry not found");
        if (prev_dip->next == dip) break;
      }  /* for */
      prev_dip->next = dip->next;
    }  /* if */
    if (dip->next == NULL) {
      pointers_block->last_dynamic_init = prev_dip;
    }  /* if */
    dip->next = NULL;  /* To be neat. */
  }  /* if */
}  /* remove_dynamic_initialization */


static void clear_variable_definition(a_variable_ptr variable)
/*
Eliminate the definition of the indicated variable, if any, to turn it
into a declaration instead of a definition.  Among other things, this
includes removing any initialization.
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
}  /* clear_variable_definition */


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


static a_boolean befriending_lists_need_to_be_merged(
                                                  a_class_list_entry_ptr list1,
                                                  a_class_list_entry_ptr list2)
/*
Return TRUE if the indicated befriending_classes lists are not identical
and therefore require merging.
*/
{
  a_boolean need_merge = FALSE;

  if (list1 != NULL || list2 != NULL) {
    a_class_list_entry_ptr clep1, clep2;
    /* Try a first pass on the assumption that the entries will be in the
       same order.  list1 and list2 are updated to point past the initial
       matching sequence, if any. */
    for (;
         list1 != NULL && list2 != NULL;
         list1 = list1->next, list2 = list2->next) {
      if (canonical_il_entry_of(list1->class_type) !=
          canonical_il_entry_of(list2->class_type)) break;
    }  /* for */
    if (list1 == NULL && list2 == NULL) {
      /* The lists matched up in the same order. */
      /* need_merge = FALSE;  -- already set. */
    } else {
      /* Try the match in any order on the remaining entries. */
      for (clep1 = list1; clep1 != NULL; clep1 = clep1->next) {
        for (clep2 = list2; clep2 != NULL; clep2 = clep2->next) {
          if (canonical_il_entry_of(clep1->class_type) ==
              canonical_il_entry_of(clep2->class_type)) break;
        }  /* for */
        if (clep2 == NULL) {
          need_merge = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return need_merge;
}  /* befriending_lists_need_to_be_merged */


static a_boolean class_befriending_lists_need_to_be_merged(a_type_ptr type1,
                                                           a_type_ptr type2)
/*
Return TRUE if the befriending_classes lists of the two indicated
classes need to be merged.
*/
{
  a_boolean                   need_merge = FALSE;
  a_class_type_supplement_ptr ctsp1 =
                                  type1->variant.class_struct_union.extra_info;
  a_class_type_supplement_ptr ctsp2 =
                                  type2->variant.class_struct_union.extra_info;

  check_assertion(ctsp1 != NULL && ctsp2 != NULL);
  if (befriending_lists_need_to_be_merged(ctsp1->befriending_classes,
                                          ctsp2->befriending_classes)) {
    need_merge = TRUE;
  }  /* if */
  return need_merge;
}  /* class_befriending_lists_need_to_be_merged */


static a_boolean class_body_should_be_copied(a_type_ptr type,
                                             a_type_ptr primary_type)
/*
Return TRUE if the class type "type", in a secondary translation unit, has
a definition that should be copied onto the type "primary_type", in
the primary IL.
*/
{
  a_boolean should_copy = FALSE;

  if (class_type_has_body(type)) {
    if (!class_type_has_body(primary_type)) {
      /* The class in the secondary translation unit has a body, and the
         one in the primary translation unit does not, so copy. */
      should_copy = TRUE;
    } else if (type->variant.class_struct_union.is_specialized &&
               !primary_type->variant.class_struct_union.is_specialized) {
      /* The class in the secondary translation unit is a specialization,
         and the one the primary translation unit is not, so copy. */
      should_copy = TRUE;
    }  /* if */
  }  /* if */
  return should_copy;
}  /* class_body_should_be_copied */


static a_boolean enum_body_should_be_copied(a_type_ptr type,
                                            a_type_ptr primary_type)
/*
Return TRUE if the enum type "type", in a secondary translation unit, has
a definition that should be copied onto the type "primary_type", in
the primary IL.
*/
{
  a_boolean should_copy = FALSE;

  if (!is_incomplete_type(type)) {
    if (is_incomplete_type(primary_type)) {
      /* The enum in the secondary translation unit has a body, and the
         one in the primary translation unit does not, so copy. */
      should_copy = TRUE;
    }  /* if */
  }  /* if */
  return should_copy;
}  /* enum_body_should_be_copied */


static a_boolean type_should_be_merged(a_type_ptr type)
/*
Return TRUE if the indicated type, which has a corresponding type in the
primary IL, should be merged into that type.
*/
{
  a_boolean  merge = FALSE;
  a_type_ptr corresp_type = (a_type_ptr)canonical_il_entry_of(type);

  if (is_immediate_class_type(type)) {
    if (class_body_should_be_copied(type, corresp_type)) {
      /* The class definition needs to be copied to corresp_type. */
      merge = TRUE;
    } else if (!C_mode() &&
               class_befriending_lists_need_to_be_merged(type, corresp_type)) {
      /* The befriending lists of the classes need to be merged. */
      merge = TRUE;
    }  /* if */
  } else if (is_immediate_enum_type(type)) {
    if (enum_body_should_be_copied(type, corresp_type)) {
      /* This type is an enum with a definition, and the corresponding type
         has no definition.  Therefore the definition must be merged into
         the corresponding type. */
      merge = TRUE;
    }  /* if */
  }  /* if */
  return merge;
}  /* type_should_be_merged */


static a_boolean variable_body_should_be_copied(
                                               a_variable_ptr variable,
                                               a_variable_ptr primary_variable)
/*
Return TRUE if the variable "variable", in a secondary translation unit, has
a definition that should be copied onto the variable "primary_variable", in
the primary IL.
*/
{
  a_boolean should_copy = FALSE;

  if (variable->storage_class == (a_storage_class)sc_unspecified) {
    if (primary_variable->storage_class != (a_storage_class)sc_unspecified) {
      /* This variable has a definition, and the corresponding variable
         has no definition.  Therefore the definition must be merged into
         the corresponding variable. */
      should_copy = TRUE;
    } else if (variable->is_specialized && !primary_variable->is_specialized) {
      /* This variable has a definition that is a specialization, and the
         other variable has a definition that is not a specialization.
         Copy this definition over to the corresponding variable. */
      should_copy = TRUE;
    }  /* if */
  }  /* if */
  return should_copy;
}  /* variable_body_should_be_copied */


static a_boolean variable_should_be_merged(a_variable_ptr variable)
/*
Return TRUE if the indicated variable, which has a corresponding variable
in the primary IL, should be merged into that variable.
*/
{
  a_boolean      merge;
  a_variable_ptr corresp_variable =
                               (a_variable_ptr)canonical_il_entry_of(variable);

  merge = variable_body_should_be_copied(variable, corresp_variable);
  return merge;
}  /* variable_should_be_merged */


static a_boolean routine_body_should_be_copied(a_routine_ptr routine,
                                               a_routine_ptr primary_routine)
/*
Return TRUE if the routine "routine", in a secondary translation unit, has
a definition that should be copied onto the routine "primary_routine", in
the primary IL.
*/
{
  a_boolean should_copy = FALSE;

  if (routine->assoc_scope != NULL_region_number) {
    if (primary_routine->assoc_scope == NULL_region_number) {
      /* This routine has a definition, and the corresponding routine
         has no definition.  Therefore the definition must be merged into
         the corresponding routine. */
      should_copy = TRUE;
    } else if (routine->is_specialized && !primary_routine->is_specialized) {
      /* This routine has a definition that is a specialization, and the
         other routine has a definition that is not a specialization.
         Copy this definition over to the corresponding routine. */
      should_copy = TRUE;
    }  /* if */
  }  /* if */
  return should_copy;
}  /* routine_body_should_be_copied */


static a_boolean routine_should_be_merged(
                                    a_routine_ptr routine,
                                    a_boolean     *any_removed_function_bodies)
/*
Return TRUE if the indicated routine, which has a corresponding routine
in the primary IL, should be merged into that routine.  If both this
routine and the corresponding routine have bodies, the one here is
deleted, and *any_removed_function_bodies is set to TRUE.
*/
{
  a_boolean     merge = FALSE;
  a_routine_ptr corresp_routine= (a_routine_ptr)canonical_il_entry_of(routine);

  if (routine_body_should_be_copied(routine, corresp_routine)) {
    /* The routine definition needs to be copied to corresp_routine. */
    merge = TRUE;
  } else if (routine->assoc_scope != NULL_region_number &&
             corresp_routine->assoc_scope != NULL_region_number) {
    /* Both instances have definitions.  Eliminate the body of this copy. */
    clear_body_for_routine(routine);
    *any_removed_function_bodies = TRUE;
  }  /* if */
  if (!C_mode() && !merge &&
      befriending_lists_need_to_be_merged(
                                       routine->befriending_classes,
                                       corresp_routine->befriending_classes)) {
    /* The befriending lists of the routines need to be merged. */
    merge = TRUE;
  }  /* if */
  return merge;
}  /* routine_should_be_merged */


static a_boolean entry_will_overwrite(char             *ptr,
                                      an_il_entry_kind kind)
/*
Return TRUE if the indicated entry, which has the indicated kind, and
which has a corresponding entry in the primary IL, will overwrite
that entry (e.g., because it has a definition and the other entry
does not).
*/
{
  a_boolean overwrite;

  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)ptr;
        a_type_ptr corresp_type = (a_type_ptr)canonical_il_entry_of(type);
        if (is_immediate_class_type(type)) {
          overwrite = class_body_should_be_copied(type, corresp_type);
        } else if (is_immediate_enum_type(type)) {
          overwrite = enum_body_should_be_copied(type, corresp_type);
        } else {
          overwrite = FALSE;
        }  /* if */
      }
      break;
    case iek_variable:
      { a_variable_ptr var = (a_variable_ptr)ptr;
        a_variable_ptr corresp_var= (a_variable_ptr)canonical_il_entry_of(var);
        overwrite = variable_body_should_be_copied(var, corresp_var);
      }
      break;
    case iek_routine:
      { a_routine_ptr rout = (a_routine_ptr)ptr;
        a_routine_ptr corresp_rout= (a_routine_ptr)canonical_il_entry_of(rout);
        overwrite = routine_body_should_be_copied(rout, corresp_rout);
      }
      break;
    default:
      overwrite = FALSE;
      break;
  }
  return overwrite;
}  /* entry_will_overwrite */


/*
Macro that returns TRUE if the indicated entry will be copied to
the primary file IL.  An entry can be copied because it's new
(it has no correspondence) or because it provides a definition
for a corresponding entry that is already in the primary file IL.
Not included are cases that are kept on the list only because
they need to be merged for details (and not to copy the definition).
Note that this macro needs to work for entries that may not have
been encountered yet in the prepare_for_trans_unit_copy processing.
The entry copy address is already assigned for entries that have been
processed.
*/
#define entry_should_be_copied(ptr, kind) \
  ((entry_copy_address_assigned(ptr) ? \
     !entry_to_be_merged(ptr) : \
     !has_corresp(ptr)) || \
   entry_will_overwrite((char *)(ptr), (kind)))


static void process_variable_if_unneeded_template(a_variable_ptr variable)
/*
If we're processing the current secondary translation unit only to
get exported templates, and the given variable is not a generated template,
do any necessary processing, e.g., externalizing it if it is static.
*/
{
  if (translation_unit_needed_only_for_exported_templates) {
    if (!variable->is_template_static_data_member ||
        variable->is_specialized) {
      /* The variable is not a generated template. */
      clear_variable_definition(variable);
#if DO_IL_LOWERING
      if (il_lowering_needed() &&
          variable->storage_class == (a_storage_class)sc_static) {
        /* A static variable referenced from a template is changed to an
           external declaration and copied over. */
        externalize_source_correspondence(&variable->source_corresp,
                                          /*is_variable=*/TRUE);
        variable->storage_class = (a_storage_class)sc_extern;
      }  /* if */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* if */
}  /* process_variable_if_unneeded_template */


static void process_routine_if_unneeded_non_template(a_routine_ptr routine)
/*
If we're processing the current secondary translation unit only to
get exported templates, and the given routine is not a generated template,
do any necessary processing, e.g., externalizing it if it is static.
*/
{
  if (translation_unit_needed_only_for_exported_templates) {
    if (!routine->is_template_function || routine->is_specialized) {
      /* The function is not a generated template. */
      /* The definition should have been eliminated at pop_scope
         time (the definition will be put out when the file is compiled
         as a primary file) unless the routine is inline. */
      check_assertion(routine->assoc_scope == NULL_region_number ||
                      routine->is_inline);
#if DO_IL_LOWERING
      if (il_lowering_needed() &&
          routine->storage_class == (a_storage_class)sc_static) {
        /* A static function referenced from a template is changed to an
           external declaration and copied over. */
        externalize_source_correspondence(&routine->source_corresp,
                                          /*is_variable=*/FALSE);
        if (routine->assoc_scope == NULL_region_number) {
          routine->storage_class = (a_storage_class)sc_extern;
        } else {
          /* A static inline function becomes external, but not exactly
             extern inline (it isn't instantiated). */
          check_assertion(routine->is_inline);
          routine->storage_class = (a_storage_class)sc_unspecified;
#if INSTANTIATE_EXTERN_INLINE
          routine->suppress_inline_body = TRUE;
#endif /* INSTANTIATE_EXTERN_INLINE */
        }  /* if */
      }  /* if */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* if */
}  /* process_routine_if_unneeded_non_template */

#if CHECKING

static void f_check_no_pending_copies(char *ptr)
/*
Check that there are no pending copies on the entry pointed to by ptr
or any of the entities on its correspondence list.
*/
{
  char *new_ptr;
  do {
    check_assertion_str(!entry_needs_copy_flag_is_set(ptr),
                        "f_check_no_pending_copies: pending copy flag");
    new_ptr = checked_trans_unit_corresp_pointer_of(ptr);
    if (new_ptr == NULL || new_ptr == ptr) break;
    ptr = new_ptr;
  } while (in_secondary_trans_unit(ptr));
}  /* f_check_no_pending_copies */

#endif /* CHECKING */

/*
Interface macro for f_check_no_pending_copies.
*/
#if CHECKING
#define check_no_pending_copies(ptr) \
  f_check_no_pending_copies((char *)(ptr))
#else /* !CHECKING */
#define check_no_pending_copies(ptr) /* Nothing */
#endif /* CHECKING */

#if CHECKING

static void f_check_parent_correspondences(char             *ptr,
                                           an_il_entry_kind kind)
/*
ptr points to an entity of kind "kind" that has a source correspondence field.
Check that if the member has a correspondence its parents do too.
*/
{
  if (trans_unit_corresp_pointer_of(ptr) != NULL &&
      trans_unit_corresp_pointer_of(ptr) != ptr &&
      /* Ignore extern "C" functions because when they are in
         namespaces the parent information is weird. */
      (kind != (an_il_entry_kind)iek_routine || C_mode() ||
       ((a_routine_ptr)ptr)->source_corresp.name_linkage !=
                                          (a_name_linkage_kind)nlk_external)) {
    a_source_correspondence *scp = (a_source_correspondence *)ptr;
    for (;;) {
      an_il_entry_kind parent_kind;
      if (scp->is_class_member) {
        scp = &scp->parent.class_type->source_corresp;
        parent_kind = (an_il_entry_kind)iek_type;
      } else if (scp->parent.namespace_ptr != NULL) {
        scp = &scp->parent.namespace_ptr->source_corresp;
        parent_kind = (an_il_entry_kind)iek_namespace;
      } else {
        break;
      }  /* if */
      if (!(trans_unit_corresp_pointer_of(scp) != NULL &&
            trans_unit_corresp_pointer_of(scp) != (char *)scp)) {
        db_entity_info(ptr, kind);
        db_entity_info((char *)scp, parent_kind);
        internal_error("entity has correspondence but parent does not");
      }  /* if */
    }  /* for */
  }  /* if */
}  /* f_check_parent_correspondences */

#endif /* CHECKING */

/*
Interface macro for f_check_parent_correspondences.
*/
#if CHECKING
#define check_parent_correspondences(ptr, kind) \
  f_check_parent_correspondences((char *)(ptr), (kind))
#else /* !CHECKING */
#define check_parent_correspondences(ptr, kind) /* Nothing */
#endif /* CHECKING */


/*
Preassign a copy address (if one is not already assigned) to an entry
kept on the lists during prepare_for_trans_unit_copy.  This is important
to ensure that if the entry is a canonical entry, and another entry
(from a different secondary translation unit) that points to the
canonical entry is encountered during the copy walk, the entry in
the current translation is copied rather than the other one.
*/
#define preassign_copy_address(ptr, kind) \
  corresp_setup((char *)(ptr), (kind), \
                /*known_in_curr_trans_unit=*/TRUE, \
                /*known_will_process_in_curr_walk=*/TRUE)


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
    /* Make sure flag_value_meaning_visited is set so that
       entry_needs_copy_flag_is_set can be used. */
    flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
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
    if (!has_corresp_that_may_require_merge(nsp)) {
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
    if (!has_corresp_that_may_require_merge(class_type)) {
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
        /* Check for other reasons to do a merge, e.g., befriending lists. */
        if (type_should_be_merged(class_type)) {
          keep_on_parent_list = TRUE;
          mark_to_merge(class_type);
        }  /* if */
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
    check_no_pending_copies(type);
    check_parent_correspondences(type, iek_type);
    keep_on_list = TRUE;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
      /* A class with a scope.  Do a recursive call to process it. */
      a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      keep_on_list = prepare_for_trans_unit_copy(class_scope,
                                                 any_removed_function_bodies);
    } else if (has_corresp_that_may_require_merge(type)) {
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
      a_type_ptr ref_type = type;
      /* Loop to handle placeholders that point to placeholders. */
      do {
        ref_type = ref_type->variant.typeref.type;
      } while (ref_type->kind == (a_type_kind)tk_typeref &&
               !typeref_is_typedef(ref_type));
      keep_on_list = entry_should_be_copied(ref_type, iek_type);
    }  /* if */
#if DEBUG
    if (db_trace("trans_copy", type, iek_type)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(type) ? "" : "not ");
      db_entity_info((char *)type, iek_type);
    }  /* if */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_type = type;
      any_members_to_process = TRUE;
      preassign_copy_address(type, iek_type);
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
    a_variable_ptr corresp_variable =
                               (a_variable_ptr)canonical_il_entry_of(variable);
    check_no_pending_copies(variable);
    check_parent_correspondences(variable, iek_variable);
    keep_on_list = TRUE;
    /* If we're supposed to copy only generated templates, other variables
       are made external (if necessary) and their definitions are
       dropped (the definition will be put out when the file
       is compiled as a primary file). */
    process_variable_if_unneeded_template(variable);
    if (has_corresp_that_may_require_merge(variable)) {
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (check_member_merges && variable_should_be_merged(variable)) {
        /* This variable has an initializer, which must be merged into the
           corresponding variable. */
        mark_to_merge(variable);
        keep_on_list = TRUE;
      }  /* if */
    }  /* if */
#if DEBUG
    if (db_trace("trans_copy", variable, iek_variable)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(variable) ? "" : "not ");
      db_entity_info((char *)variable, iek_variable);
    }  /* if */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_variable = variable;
      any_members_to_process = TRUE;
      preassign_copy_address(variable, iek_variable);
    } else {
      /* Remove this entry from the list. */
      if (prev_variable == NULL) {
        scope->variables = variable->next;
      } else {
        prev_variable->next = variable->next;
      }  /* if */
      /* If we need the type of this variable, the type of the corresponding
         variable can be used. */
      if (in_secondary_trans_unit(variable->type) &&
          !has_corresp(variable->type)) {
        checked_trans_unit_corresp_pointer_of(variable->type) =
                                                (char *)corresp_variable->type;
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
       initialized.  See clear_variable_definition. */
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
    a_routine_ptr corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
    check_no_pending_copies(routine);
    check_parent_correspondences(routine, iek_routine);
    keep_on_list = TRUE;
    /* If we're supposed to copy only generated templates, other routines
       are made external (if necessary) and their definitions are
       dropped. */
    process_routine_if_unneeded_non_template(routine);
    if (has_corresp_that_may_require_merge(routine)) {
      /* This entry corresponds to something in the primary IL. */
      keep_on_list = FALSE;
      if (check_member_merges &&
          routine_should_be_merged(routine, any_removed_function_bodies)) {
        /* Merge the definition here into the corresponding routine. */
        mark_to_merge(routine);
        keep_on_list = TRUE;
      }  /* if */
      /* Update some information regarding inline functions. */
      check_assertion(routine->is_inline == corresp_routine->is_inline ||
                      /* The is_inline flag in templates is not set until
                         the function is fully instantiated. */
                      (routine->is_template_function &&
                       routine->assoc_scope == NULL_region_number) ||
                      (corresp_routine->is_template_function &&
                       corresp_routine->assoc_scope == NULL_region_number));
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
    } else {
      /* This routine has no correspondence in the primary file IL. */
    }  /* if */
#if DEBUG
    if (db_trace("trans_copy", routine, iek_routine)) {
      fprintf(f_debug, "prepare_for_trans_unit_copy, ");
      fprintf(f_debug, "%skeeping on list, ", keep_on_list ? "" : "not ");
      fprintf(f_debug, "%smerging:\n",
                       entry_to_be_merged(routine) ? "" : "not ");
      db_entity_info((char *)routine, iek_routine);
    }  /* if */
#endif /* DEBUG */
    if (keep_on_list) {
      prev_routine = routine;
      any_members_to_process = TRUE;
      preassign_copy_address(routine, iek_routine);
    } else {
      /* Remove this entry from the list. */
      if (prev_routine == NULL) {
        scope->routines = routine->next;
      } else {
        prev_routine->next = routine->next;
      }  /* if */
      /* If we need the type of this routine, the type of the corresponding
         variable can be used. */
      if (in_secondary_trans_unit(routine->type) &&
          !has_corresp(routine->type)) {
        checked_trans_unit_corresp_pointer_of(routine->type) =
                                                 (char *)corresp_routine->type;
      }  /* if */
#if MAINTAIN_NEEDED_FLAGS
      /* The routine will not be copied over, so eliminate any
         default argument object lifetimes so they will not be copied
         over. */
      eliminate_routine_default_arg_object_lifetimes(routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* if */
  }  /* for */
  if (pointers_block != NULL) pointers_block->last_routine = prev_routine;
  /* Visit all templates. */
  prev_templ = NULL;
  for (templ = scope->templates;
       templ != NULL;
       templ = templ->next) {
    check_no_pending_copies(templ);
    check_parent_correspondences(templ, iek_template);
    keep_on_list = TRUE;
    if (has_corresp_that_may_require_merge(templ)) {
      /* This entry corresponds to something in the primary IL, so remove
         it from the list. */
      keep_on_list = FALSE;
    }  /* if */
    if (keep_on_list) {
      prev_templ = templ;
      any_members_to_process = TRUE;
      preassign_copy_address(templ, iek_template);
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
    check_no_pending_copies(nsp);
    check_parent_correspondences(nsp, iek_namespace);
    /* Entities with correspondences don't get copied; they get merged
       into the corresponding entry.  Set a flag to indicate that. */
    if (!nsp->is_namespace_alias) {
      keep_on_list = prepare_for_trans_unit_copy(nsp->variant.assoc_scope,
                                                 any_removed_function_bodies);
    } else {
      /* A namespace alias.  Keep it only if there's not already a copy in
         the primary IL. */
      keep_on_list = !has_corresp_that_may_require_merge(nsp);
    }  /* if */
    if (keep_on_list) {
      prev_nsp = nsp;
      any_members_to_process = TRUE;
      preassign_copy_address(nsp, iek_namespace);
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
    check_no_pending_copies(pragma);
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


static void merge_befriending_classes_lists(a_class_list_entry_ptr *plist1,
                                            a_class_list_entry_ptr list2)
/*
Merge the befriending lists pointed to by *plist1 and plist2, and
update *plist1 to point to the merged list.
*/
{
  a_class_list_entry_ptr clep1, clep2, clep2_next, list1 = *plist1;

  for (clep2 = list2; clep2 != NULL; clep2 = clep2_next) {
    clep2_next = clep2->next;
    for (clep1 = list1; clep1 != NULL; clep1 = clep1->next) {
      if (clep1->class_type == clep2->class_type) break;
    }  /* for */
    if (clep1 == NULL) {
      /* Add the entry from clep2 to the *plist1 list. */
      clep2->next = *plist1;
      *plist1 = clep2;
    }  /* if */
  }  /* for */
}  /* merge_befriending_classes_lists */


static void merge_class_details(a_type_ptr type,
                                a_type_ptr primary_type)
/*
The class primary_type (in the primary IL) has just been overwritten by,
or is otherwise being merged with, the class "type" from a secondary
translation unit.  Do merging of minor information.
*/
{
  a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
  a_class_type_supplement_ptr primary_ctsp =
                           primary_type->variant.class_struct_union.extra_info;

  check_assertion(ctsp != NULL && primary_ctsp != NULL);
  merge_befriending_classes_lists(&primary_ctsp->befriending_classes,
                                  ctsp->befriending_classes);
}  /* merge_class_details */


static void merge_routine_details(a_routine_ptr rout,
                                  a_routine_ptr primary_rout)
/*
The routine primary_rout (in the primary IL) has just been overwritten by,
or is otherwise being merged with, the routine rout from a secondary
translation unit.  Do merging of minor information.
*/
{
  merge_befriending_classes_lists(&primary_rout->befriending_classes,
                                  rout->befriending_classes);
}  /* merge_routine_details */


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
  save_needed_flag_for_overwrite(primary_entry) \
  save_per_instantiation_needed_flags_for_overwrite(primary_entry)
#define do_restores_for_overwrite(primary_entry, entry) \
  (primary_entry)->next = saved_next; \
  restore_needed_flag_for_overwrite(primary_entry) \
  restore_per_instantiation_needed_flags_for_overwrite(primary_entry)


static void overwrite_primary_type(a_type_ptr type,
                                   a_type_ptr primary_type)
/*
Overwrite the type primary_type (in the primary IL) with type (in
the secondary translation unit IL).
*/
{
  a_boolean                   is_class = is_immediate_class_type(type);
  a_class_list_entry_ptr      saved_befriending_classes;
  a_class_type_supplement_ptr primary_ctsp;
  do_saves_for_overwrite(primary_type, a_type_ptr);
  if (is_class) {
    primary_ctsp = primary_type->variant.class_struct_union.extra_info;
    saved_befriending_classes = primary_ctsp->befriending_classes;
  }  /* if */
  *primary_type = *type;
  do_restores_for_overwrite(primary_type, type);
  if (is_class) {
    primary_ctsp = primary_type->variant.class_struct_union.extra_info;
    primary_ctsp->befriending_classes = saved_befriending_classes;
  }  /* if */
}  /* overwrite_primary_type */


static void overwrite_primary_variable(a_variable_ptr var,
                                       a_variable_ptr primary_var)
/*
Overwrite the variable primary_var (in the primary IL) with var (in
the secondary translation unit IL).
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_instantiation_needed_bit_number =
                                  primary_var->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  do_saves_for_overwrite(primary_var, a_variable_ptr);
  *primary_var = *var;
  do_restores_for_overwrite(primary_var, var);
#if ONE_INSTANTIATION_PER_OBJECT
  primary_var->instantiation_needed_bit_number =
                                         saved_instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
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
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_instantiation_needed_bit_number =
                                 primary_rout->instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* suppress_inline_body is only valid on routines with bodies.  Save the
     destination value only if the destination routine already has a
     body. */
  a_boolean saved_suppress_inline_body =
                            (primary_rout->assoc_scope != NULL_region_number) ?
                                           primary_rout->suppress_inline_body :
                                           rout->suppress_inline_body;
  a_class_list_entry_ptr saved_befriending_classes =
                                             primary_rout->befriending_classes;
  a_boolean saved_on_inline_function_list =
                                         primary_rout->on_inline_function_list;
  do_saves_for_overwrite(primary_rout, a_routine_ptr);
#if MAINTAIN_NEEDED_FLAGS
  /* Eliminate any default argument object lifetimes associated with the
     entry that is about to be overwritten. */
  eliminate_routine_default_arg_object_lifetimes(primary_rout);
#endif /* MAINTAIN_NEEDED_FLAGS */
  *primary_rout = *rout;
  do_restores_for_overwrite(primary_rout, rout);
#if ONE_INSTANTIATION_PER_OBJECT
  primary_rout->instantiation_needed_bit_number =
                                         saved_instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  /* Note that inline_instance_required etc. were previously updated in
     the primary routine, so we just save the value determined. */
#if INSTANTIATE_EXTERN_INLINE
  primary_rout->inline_instance_required = saved_inline_instance_required;
#endif /* INSTANTIATE_EXTERN_INLINE */
  primary_rout->suppress_inline_body = saved_suppress_inline_body;
  primary_rout->befriending_classes = saved_befriending_classes;
  primary_rout->on_inline_function_list = saved_on_inline_function_list;
}  /* overwrite_primary_routine */


/*
Change a pointer to its canonical value.
*/
#define change_pointer_to_canonical(ptr, ptr_type) \
{ if ((ptr) != NULL) (ptr) = (ptr_type)canonical_il_entry_of(ptr); }


static void update_namespace_pointers_block(a_scope_ptr scope)
/*
scope is the secondary translation unit scope for a namespace that has
been copied to the primary IL rather than merged.  Update its
pointers block so that its last-pointers point to the copied
entries in the primary IL.
*/
{
  a_scope_ptr            primary_scope =
                                     (a_scope_ptr)canonical_il_entry_of(scope);
  a_scope_pointers_block *pointers_block =
                                   get_pointers_block_for_scope(primary_scope);
  a_namespace_ptr        sub_nsp;

  change_pointer_to_canonical(pointers_block->last_constant,
                              a_constant_ptr);
  change_pointer_to_canonical(pointers_block->last_type,
                              a_type_ptr);
  change_pointer_to_canonical(pointers_block->last_variable,
                              a_variable_ptr);
  change_pointer_to_canonical(pointers_block->last_routine,
                              a_routine_ptr);
  change_pointer_to_canonical(pointers_block->last_asm_entry,
                              an_asm_entry_ptr);
  change_pointer_to_canonical(pointers_block->last_dynamic_init,
                              a_dynamic_init_ptr);
  change_pointer_to_canonical(pointers_block->last_namespace,
                              a_namespace_ptr);
  change_pointer_to_canonical(pointers_block->last_using_decl,
                              a_using_decl_ptr);
  change_pointer_to_canonical(pointers_block->last_template,
                              a_template_ptr);
  change_pointer_to_canonical(pointers_block->last_pragma,
                              a_pragma_ptr);
  change_pointer_to_canonical(pointers_block->last_template,
                              a_template_ptr);
  /* Process any namespaces under this one. */
  for (sub_nsp = scope->namespaces;
       sub_nsp != NULL;
       sub_nsp = sub_nsp->next) {
    if (!sub_nsp->is_namespace_alias) {
      update_namespace_pointers_block(sub_nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* update_namespace_pointers_block */


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

  if (scope->kind == (a_scope_kind)sck_file) {
    /* Top-level call. */
    /* Make sure flag_value_meaning_visited is set so that
       entry_needs_copy_flag_is_set can be used. */
    flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
  }  /* if */
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
        check_no_pending_copies(type);
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
#if DEBUG
          if (db_trace("trans_copy", corresp_type, iek_type)) {
            fprintf(f_debug,
                    "finish_trans_unit_copy, adding to list after copy:\n");
            db_entity_info((char *)corresp_type, iek_type);
          }  /* if */
#endif /* DEBUG */
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
          /* The entry gets merged into the corresponding type. */
          a_type_ptr primary_type =
               (a_type_ptr)checked_trans_unit_corresp_pointer_of(corresp_type);
#if DEBUG
          if (db_trace("trans_copy", corresp_type, iek_type)) {
            fprintf(f_debug,
                    "finish_trans_unit_copy, merging into %lx after copy:\n",
                    (unsigned long)primary_type);
            db_entity_info((char *)corresp_type, iek_type);
          }  /* if */
#endif /* DEBUG */
          if (is_immediate_class_type(corresp_type)) {
            merge_class_details(corresp_type, primary_type);
            if (!class_body_should_be_copied(corresp_type, primary_type)) {
              /* No overwriting is needed, so we're done.  This happens,
                 for example, when the only reason for merging is to merge
                 the befriending lists, or when the class is marked to be
                 merged because some of its members need to be merged. */
              goto end_of_type_list_add;
            }  /* if */
            check_assertion(class_type_has_body(corresp_type));
          }  /* if */
          /* Copy this type and its definition, overwriting the
             existing primary type.  Move the primary IL type
             to the end of the types list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list. */
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
        check_no_pending_copies(variable);
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
#if DEBUG
          if (db_trace("trans_copy", corresp_variable, iek_variable)) {
            fprintf(f_debug,
                    "finish_trans_unit_copy, merging into %lx after copy:\n",
                    (unsigned long)primary_variable);
            db_entity_info((char *)corresp_variable, iek_variable);
          }  /* if */
#endif /* DEBUG */
          if (primary_variable->storage_class ==
                                             (a_storage_class)sc_unspecified) {
            /* Eliminate the definition of the primary variable (this happens
               when the secondary has a specialization and the primary does
               not). */
            if (corresp_variable->is_specialized &&
                !primary_variable->is_specialized) {
              /* This variable is both specialized and used in the
                 non-specialized version.  That's an error. */
              report_bad_trans_unit_corresp(corresp_variable);
            } else {
              clear_variable_definition(primary_variable);
            }  /* if */
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
#if DEBUG
        if (db_trace("trans_copy", corresp_variable, iek_variable)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, adding to list after copy:\n");
          db_entity_info((char *)corresp_variable, iek_variable);
        }  /* if */
#endif /* DEBUG */
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
        check_no_pending_copies(routine);
        if (entry_to_be_merged(routine)) {
          /* The entry gets merged into the corresponding type. */
          a_routine_ptr primary_routine =
                   (a_routine_ptr)checked_trans_unit_corresp_pointer_of(
                                                              corresp_routine);
#if DEBUG
          if (db_trace("trans_copy", corresp_routine, iek_routine)) {
            fprintf(f_debug,
                    "finish_trans_unit_copy, merging into %lx after copy:\n",
                    (unsigned long)primary_routine);
            db_entity_info((char *)corresp_routine, iek_routine);
          }  /* if */
#endif /* DEBUG */
          merge_routine_details(corresp_routine, primary_routine);
          if (!routine_body_should_be_copied(corresp_routine,
                                             primary_routine)) {
            /* No overwriting is needed, so we're done.  This happens,
               for example, when the only reason for merging is to merge
               the befriending lists. */
            goto end_of_routine_list_add;
          }  /* if */
          check_assertion(corresp_routine->assoc_scope != NULL_region_number);
          if (primary_routine->assoc_scope != NULL_region_number) {
            /* Eliminate the body of the primary routine (this happens when
               the secondary has a specialization and the primary does not). */
            if (corresp_routine->is_specialized &&
                !primary_routine->is_specialized) {
              /* This routine is both specialized and used in the
                 non-specialized version.  That's an error. */
              report_bad_trans_unit_corresp(corresp_routine);
            } else {
              clear_body_for_routine(primary_routine);
            }  /* if */
          }  /* if */
          /* Copy this routine and its definition, overwriting the
             existing primary routine.  Move the primary IL routine
             to the end of the routines list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list. */
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
#if DEBUG
        if (db_trace("trans_copy", corresp_routine, iek_routine)) {
          fprintf(f_debug,
                  "finish_trans_unit_copy, adding to list after copy:\n");
          db_entity_info((char *)corresp_routine, iek_routine);
        }  /* if */
#endif /* DEBUG */
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
        check_no_pending_copies(templ);
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
        check_no_pending_copies(nsp);
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
        check_no_pending_copies(pragma);
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
    if (scope->asm_entries != NULL) {
      an_asm_entry_ptr asm_entry, last_asm_entry;
      /* Merge the asm entries in the scope into the primary IL scope. */
      /* Get a pointer to the last asm entry in the primary scope. */
      if (pointers_block != NULL) {
        last_asm_entry = pointers_block->last_asm_entry;
      } else {
        /* Actual last asm entry will be determined below if/when needed. */
        last_asm_entry = NULL;
      }  /* if */
      for (asm_entry = scope->asm_entries;
           asm_entry != NULL;
           asm_entry = asm_entry->next) {
        an_asm_entry_ptr corresp_asm_entry =
            (an_asm_entry_ptr)checked_trans_unit_corresp_pointer_of(asm_entry);
        check_no_pending_copies(asm_entry);
        if (is_class_scope && last_asm_entry == NULL) {
          /* Determine the last asm entry the first time it is needed. */
          last_asm_entry = primary_scope->asm_entries;
          if (last_asm_entry != NULL) {
            while (last_asm_entry->next != NULL) {
              last_asm_entry = last_asm_entry->next;
            }  /* while */
          }  /* if */
        }  /* if */
        /* Add the asm entry to the end of the list. */
        if (last_asm_entry == NULL) {
          primary_scope->asm_entries = corresp_asm_entry;
        } else {
          last_asm_entry->next = corresp_asm_entry;
        }  /* if */
        corresp_asm_entry->next = NULL;
        last_asm_entry = corresp_asm_entry;
        if (pointers_block != NULL) {
          pointers_block->last_asm_entry = last_asm_entry;
        }  /* if */
      }  /* for */
    }  /* if */
    /* Merge the object lifetime from "scope" into that from
       "primary_scope". */
    merge_object_lifetimes(scope, primary_scope);
  } else {
    /* This scope is not being merged into a counterpart in the primary
       IL.  It was just copied over. */
    /* For a namespace scope, update the end-of-list pointers in the
       pointers block to match to addresses of the copies. */
    if (scope->kind == (a_scope_kind)sck_namespace) {
      update_namespace_pointers_block(scope);
    }  /* if */
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


void copy_secondary_trans_unit_IL_to_primary(void)
/*
Copy IL from the current translation unit, which is a secondary translation
unit, to the primary translation unit IL.  If needed flag processing
is configured in, unneeded entities have already been removed from the
secondary translation unit IL and therefore will not be copied.
*/
{
  a_scope_ptr top_scope = il_header.primary_scope;
  a_boolean   any_removed_function_bodies = FALSE;

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
  check_assertion(!il_entry_prefix_of(top_scope).
                  il_lowering_flag);/*lint !e527*/
  initial_value_for_il_lowering_flag = FALSE;
  (void)prepare_for_trans_unit_copy(top_scope, &any_removed_function_bodies);
  copy_from_secondary_to_primary_IL();
  copy_function_bodies_from_secondary_to_primary_IL(top_scope);
  finish_trans_unit_copy(top_scope);
  merge_il_headers();
#if DEBUG
  if (debug_level >= 1) {
    fprintf(f_debug, "Done with copy from secondary translation unit %s\n",
            curr_translation_unit->source_file->name_as_written);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* copy_secondary_trans_unit_IL_to_primary */


static void copy_info_for_inline_routine(a_routine_ptr routine)
/*
The indicated routine has been copied from the secondary translation
unit IL to the primary IL.  routine points to the copy in the
secondary translation unit, except for member functions of local
classes, where it points to the primary IL copy.  Update the
"instantiation" lists for extern inline functions, if appropriate.
*/
{
  a_boolean local_member_function = !in_secondary_trans_unit(routine);

  /* This routine runs while switched to the primary translation unit. */
  check_assertion(is_primary_translation_unit &&
                  (!local_member_function ||
                   routine->source_corresp.is_local_to_function));
  if (instantiate_extern_inline && routine->is_inline &&
      routine->storage_class == (a_storage_class)sc_unspecified &&
      !routine->source_corresp.static_used_by_instantiation) {
    /* extern inline functions are put on a list so they can be
       "instantiated". */
    a_boolean     overwrite = (!local_member_function &&
                               entry_to_be_merged(routine));
    a_routine_ptr primary_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
    if (overwrite && primary_routine->on_inline_function_list) {
      /* There is already a list entry for the routine in the primary IL. */
    } else {
      /* Add an entry for the routine. */
      add_to_inline_function_list(primary_routine);
    }  /* if */
  }  /* if */
}  /* copy_info_for_inline_routine */


static void wrap_up_moved_function(a_routine_ptr rout)
/*
rout identifies a function which has been moved from a secondary
translation unit to the primary translation unit IL.  Do final processing,
which includes IL lowering if appropriate.  rout points to the instance
of the routine in the secondary translation unit, except for member
functions of local classes, where it points to the primary IL copy.
*/
{
  a_routine_ptr primary_rout = (a_routine_ptr)canonical_il_entry_of(rout);

  check_assertion(!in_secondary_trans_unit(primary_rout) &&
                  primary_rout->source_corresp.
                                             copied_from_secondary_trans_unit);
  /* If the routine is an extern inline function, copy instantiation
     information. */
  copy_info_for_inline_routine(rout);
  /* Note use of rout here instead of primary_rout. */
  if (rout->assoc_scope != NULL_region_number) {
    /* The routine body was moved. */
    a_scope_ptr scope= il_header.region_scope_entry[primary_rout->assoc_scope];
    check_assertion_str(scope != NULL, "wrap_up_moved_function: body missing");
    finish_function_body_processing(scope, /*discard_function_body=*/FALSE);
  }  /* if */
}  /* wrap_up_moved_function */


static void finish_moved_function_processing(a_scope_ptr scope,
                                             a_boolean   do_inlines);


static void finish_type_list_moved_function_processing(a_type_ptr type_list,
                                                       a_boolean  do_inlines)
/*
Finish processing in the indicated type list and its subscopes for any
functions whose bodies were moved from the secondary translation unit IL
to the primary IL.
*/
{
  a_type_ptr type;

  if (!C_mode()) {
    /* Look for class types and process their member functions. */
    for (type = type_list; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          finish_moved_function_processing(class_scope, do_inlines);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* finish_type_list_moved_function_processing */


static void finish_moved_function_processing(a_scope_ptr scope,
                                             a_boolean   do_inlines)
/*
Finish processing in the indicated scope and its subscopes for any
functions whose bodies were moved from the secondary translation unit IL
to the primary IL.  This includes lowering if necessary.  The scope
passed in is from the secondary translation unit except for local
class scopes.  Inline functions are processed only if do_inlines is
TRUE, other functions only if do_inlines is FALSE, thus allowing a
two-pass sweep.
*/
{
  a_routine_ptr   routine;
  a_namespace_ptr nsp;

  check_assertion(in_secondary_trans_unit(scope) ||
                  (scope->kind == (a_scope_kind)sck_class_struct_union &&
                   scope->variant.assoc_type->source_corresp.
                                                        is_local_to_function));
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      finish_moved_function_processing(nsp->variant.assoc_scope, do_inlines);
    }  /* if */
  }  /* for */
  finish_type_list_moved_function_processing(scope->types, do_inlines);
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    a_boolean eff_inline = (routine->is_inline != 0);
    if (routine->assoc_scope != NULL_region_number) {
      a_scope_ptr rout_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
      if (rout_scope == NULL) {
        /* The body might be missing if it was deleted on the first pass
           because the routine is inline and it has no local types. */
        check_assertion_str(!do_inlines && routine->is_inline,
                            "finish_moved_function_processing: body missing");
      } else {
        /* Handle local classes (and their member functions).  Note that,
           because the routine scope has already been moved, the types
           list here is in the primary IL. */
        finish_type_list_moved_function_processing(rout_scope->types,
                                                   do_inlines);
        /* Force a routine with local types to be done on the second pass
           so that we do not lose its body -- and its local types list --
           on the first pass. */
        if (rout_scope->types != NULL) eff_inline = FALSE;
      }  /* if */
    }  /* if */
    /* Process functions on the right pass (inline/noninline). */
    if ((do_inlines != 0) == eff_inline) {
      wrap_up_moved_function(routine);
    }  /* if */
  }  /* for */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object &&
      scope->kind == (a_scope_kind)sck_class_struct_union &&
      /* Do this only on the second pass. */
      !do_inlines) {
    /* Assign one-instantiation-per-object needed bit numbers to
       static data members. */
    a_variable_ptr var;
    for (var = scope->variables; var != NULL; var = var->next) {
      a_variable_ptr corresp_var = (a_variable_ptr)canonical_il_entry_of(var);
      set_variable_instantiation_needed_bit_number(corresp_var);
    }  /* for */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* finish_moved_function_processing */

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED

static void finish_scope_orphaned_list_processing(
                                    a_scope_orphaned_list_header_ptr solh_list)
/*
Final processing on scope orphaned list headers.  For functions that
were actually copied over, a new version of the orphaned list header
entries was generated on the other side (after lowering).  For functions
whose bodies were deleted, however, there may be dangling types etc.
in the scope orphaned list headers in the secondary translation unit.
The entries have been copied over, but they're not linked into the IL
tree.  Link them in now if appropriate.  solh_list points to the list
of scope orphaned list headers from the secondary translation unit.
The current translation unit is the primary translation unit.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  check_assertion(solh_list == NULL || in_secondary_trans_unit(solh_list));
  check_assertion(is_primary_translation_unit);
  for (solhp = solh_list; solhp != NULL; solhp = solhp->next) {
    if (solhp->assoc_routine->assoc_scope == NULL_region_number) {
      /* The routine associated with this entry was deleted, so link the
         copy of this entry into the scope orphaned headers list in the
         primary IL. */
      a_scope_orphaned_list_header_ptr corresp_solhp =
                               (a_scope_orphaned_list_header_ptr)
                                  checked_trans_unit_corresp_pointer_of(solhp);
      if (il_header.scope_orphaned_list_headers == NULL) {
        il_header.scope_orphaned_list_headers = corresp_solhp;
      } else {
        curr_translation_unit->last_scope_orphaned_list_header->next =
                                                                 corresp_solhp;
      }  /* if */
      curr_translation_unit->last_scope_orphaned_list_header = corresp_solhp;
    }  /* if */
  }  /* for */
}  /* finish_scope_orphaned_list_processing */

#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

void process_functions_moved_from_secondary_trans_units(void)
/*
Do final processing on any functions moved from secondary translation
units when copy_secondary_trans_unit_IL_to_primary was called.  This
includes lowering of the function bodies.  This routine is called when in
the primary translation unit, after all copying from secondary
translation units has been done.
*/
{
  a_translation_unit_ptr tup;

  db_enter(1, "process_functions_moved_from_secondary_trans_units");
  check_assertion(is_primary_translation_unit);
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    finish_moved_function_processing(tup->primary_scope, /*do_inlines=*/TRUE);
    finish_moved_function_processing(tup->primary_scope, /*do_inlines=*/FALSE);
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
    finish_scope_orphaned_list_processing(
                                   tup->il_header.scope_orphaned_list_headers);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
  }  /* for */
  db_exit();
}  /* process_functions_moved_from_secondary_trans_units */


/*
Flag used by mark_secondary_termination_test.
*/
static a_boolean
		mark_secondary_first_pass;


#if !MAINTAIN_NEEDED_FLAGS
/*ARGSUSED*/  /* <-- "kind" is not used in that case. */
#endif /* !MAINTAIN_NEEDED_FLAGS */
static a_boolean mark_secondary_termination_test(char             *ptr,
                                                 an_il_entry_kind kind)
/*
Called during the IL walk for
mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed.
If ptr points to a secondary translation unit entry, mark that entry
as needed and prune the walk.
*/
{
  a_boolean prune;

  if (in_secondary_trans_unit(ptr)) {
#if MAINTAIN_NEEDED_FLAGS
    if (mark_secondary_first_pass) {
      mark_as_needed(ptr, kind);
      /* If the entity is a class type with a definition, mark its definition
         as needed as well.  We don't actually know whether it is needed,
         so we assume it is. */
      if (kind == iek_type) {
        a_type_ptr type = (a_type_ptr)ptr;
        if (is_immediate_class_type(type) &&
            class_type_has_body(type)) {
          set_class_keep_definition_in_il(type);
          set_class_definition_needed(type);
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    prune = TRUE;
  } else if (il_entry_prefix_of(ptr).il_walk_flag ==
                                                  flag_value_meaning_visited) {
    /* This entry has already been visited on this walk. */
    prune = TRUE;
  } else {
    il_entry_prefix_of(ptr).il_walk_flag = flag_value_meaning_visited;
    prune = FALSE;
  }  /* if */
  return prune;
}  /* mark_secondary_termination_test */


static a_boolean mem_region_is_primary_func_scope(
                                                 a_memory_region_number number)
/*
Return TRUE if the indicated memory region is a function scope memory
region of the primary IL.
*/
{
  a_boolean result = FALSE;

  if (mem_region_table[number] == NULL) {
    /* This memory has already been freed. */
  } else {
    a_scope_ptr sp = il_header.region_scope_entry[number];
    a_boolean   from_secondary_trans_unit =
                       (trans_unit_for_scope[sp->number] != translation_units);
    if (!from_secondary_trans_unit &&
        sp->kind != (a_scope_kind)sck_file &&
        /* Ignore functions copied from a secondary translation unit. */
        !sp->variant.routine.ptr->source_corresp.
                                            copied_from_secondary_trans_unit) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* mem_region_is_primary_func_scope */


void mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed(void)
/*
Walk through the primary translation unit IL tree, looking for pointers
to entities in secondary translation unit IL.  Mark such secondary IL
entities as needed, so that they will be copied to the primary IL later.
(This is done before the copying of secondary translation unit IL to the
primary IL.)
*/
{
  a_memory_region_number n;

  db_enter(1,
          "mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed");
  if (primary_il_may_reference_other_trans_units) {
    /* Do two passes so that the il_walk_flag returns to its original value. */
    mark_secondary_first_pass = TRUE;
    for (;;) {
      walk_file_scope_il((an_entry_process_function_ptr)NULL,
                         (a_string_entry_process_function_ptr)NULL,
                         (a_remap_function_ptr)NULL,
                         mark_secondary_termination_test,
                         /*clear_fe_pointers=*/FALSE);
      /* Loop through the memory regions looking for functions in the
         primary IL, and process them too. */
      for (n = FILE_SCOPE_REGION_NUMBER + 1;
           n <= highest_used_region_number;
           ++n) {
        if (mem_region_is_primary_func_scope(n)) {
          walk_routine_scope_il(n,
                                (an_entry_process_function_ptr)NULL,
                                (a_string_entry_process_function_ptr)NULL,
                                (a_remap_function_ptr)NULL,
                                mark_secondary_termination_test,
                                /*clear_fe_pointers=*/FALSE);
        }  /* if */
      }  /* for */
      if (!mark_secondary_first_pass) break;
      mark_secondary_first_pass = FALSE;
    }  /* for */
  }  /* if */
  db_exit();
}  /* mark_secondary_trans_unit_IL_entities_used_from_primary_as_needed */


/*ARGSUSED*/ /* <-- "kind" is not used. */
static char *remap_secondary_pointer(char             *old_ptr,
                                     an_il_entry_kind kind)
/*
Called as part of the IL walk for 
rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary to
do the pointer remapping.  Remaps pointers to entities in secondary
translation units to pointers to the corresponding entities in the
primary IL.  The correspondences must exist, at least for entities
with linkage.
*/
{
  char *new_ptr = old_ptr;

  if (old_ptr == NULL) {
    /* Leave a NULL pointer alone. */
  } else if (in_secondary_trans_unit(old_ptr)) {
    check_assertion_str(in_file_scope(old_ptr),
                        "remap_secondary_pointer: not in file scope");
    if (trans_unit_corresp_pointer_of(old_ptr) == NULL) {
      /* No correspondence established.  This is okay for things
         that don't go on lists.  For example, the type "pointer to int"
         wouldn't necessarily have a correspondence here, but A<int>
         must. */
#if CHECKING
      /* Check whether the entity is okay. */
      { a_boolean err = FALSE;
        switch (kind) {
          case iek_constant:
            { a_constant_ptr con = (a_constant_ptr)old_ptr;
              if (has_name(con)) err = TRUE;
            }
            break;
          case iek_type:
            { a_type_ptr type = (a_type_ptr)old_ptr;
              if (has_name(type) ||
                  is_immediate_class_type(type) ||
                  (type->kind == (a_type_kind)tk_enum &&
                   type->variant.integer.enum_type)) err = TRUE;
            }
            break;
          case iek_template_arg:
            break;
          default:
            err = TRUE;
        }  /* switch */
        if (err) {
          unexpected_condition_str(
                 "remap_secondary_pointer: missing primary IL correspondence");
        }  /* if */
      }
#endif /* CHECKING */
      /* Make a copy of the entry in the primary IL. */
      new_ptr = alloc_il(sizeof_il_entry[(int)kind]);
      trans_unit_corresp_pointer_of(old_ptr) = new_ptr;
      /* Set the flag to indicate that a copy address has been assigned. */
      entry_copy_address_assigned(old_ptr) = TRUE;
      copy_entry_basic(old_ptr, kind, remap_secondary_pointer);
      /* Make sure the copy is processed. */
      il_entry_prefix_of(new_ptr).il_walk_flag = !flag_value_meaning_visited;
    } else {
      new_ptr = canonical_il_entry_of(old_ptr);
      check_assertion_str(!in_secondary_trans_unit(new_ptr),
                       "remap_secondary_pointer: correspondence to secondary");
    }  /* if */
  }  /* if */
  return new_ptr;
}  /* remap_secondary_pointer */


/*ARGSUSED*/ /* <-- "kind" is not used. */
static a_boolean rewrite_secondary_termination_test(char             *ptr,
                                                    an_il_entry_kind kind)
/*
Called during the IL walk for
rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary to
do the termination test.
*/
{
  a_boolean prune;

  /* There shouldn't be any secondary translation unit pointers left at
     this point -- the remap routine eliminates them. */
  check_assertion_str(!in_secondary_trans_unit(ptr),
         "rewrite_secondary_termination_test: remaining secondary IL pointer");
  if (il_entry_prefix_of(ptr).il_walk_flag == flag_value_meaning_visited) {
    /* This entry has already been visited on this walk. */
    prune = TRUE;
  } else {
    il_entry_prefix_of(ptr).il_walk_flag = flag_value_meaning_visited;
    prune = FALSE;
  }  /* if */
  return prune;
}  /* rewrite_secondary_termination_test */


void rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary(void)
/*
Walk through the primary translation unit IL tree, looking for pointers
to entities in secondary translation unit IL.  Rewrite such pointers
as pointers to the corresponding primary IL entities.  This is done after
the copying of secondary translation unit IL to the primary IL, and
before lowering and needed flag marking of the primary IL.
*/
{
  a_memory_region_number n;

  db_enter(1,
           "rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary");
  if (primary_il_may_reference_other_trans_units) {
    a_boolean first_pass = TRUE;
    /* Do two passes so that the il_walk_flag returns to its original value. */
    for (;;) {
      a_remap_function_ptr remap_func = NULL;
      if (first_pass) remap_func = remap_secondary_pointer;
      walk_file_scope_il((an_entry_process_function_ptr)NULL,
                         (a_string_entry_process_function_ptr)NULL,
                         remap_func,
                         rewrite_secondary_termination_test,
                         /*clear_fe_pointers=*/FALSE);
      /* Loop through the memory regions looking for functions in the
         primary IL, and process them too. */
      for (n = FILE_SCOPE_REGION_NUMBER + 1;
           n <= highest_used_region_number;
           ++n) {
        if (mem_region_is_primary_func_scope(n)) {
          walk_routine_scope_il(n,
                                (an_entry_process_function_ptr)NULL,
                                (a_string_entry_process_function_ptr)NULL,
                                remap_func,
                                rewrite_secondary_termination_test,
                                /*clear_fe_pointers=*/FALSE);
        }  /* if */
      }  /* for */
      if (!first_pass) break;
      first_pass = FALSE;
    }  /* for */
#if DO_IL_LOWERING
    if (any_lowering_needed()) {
      /* Do any required lowering etc. that wasn't done earlier on
         function bodies in the primary IL.  Lowering is delayed on
         some instantiations in the primary translation unit when
         there are exported templates so that we can rewrite any
         references to secondary translation unit entities before
         the lowering is done. */
      for (n = FILE_SCOPE_REGION_NUMBER + 1;
           n <= highest_used_region_number;
           ++n) {
        if (mem_region_is_primary_func_scope(n)) {
          a_scope_ptr sp = il_header.region_scope_entry[n];
          if (!il_entry_prefix_of(sp).il_lowering_flag) {
            finish_function_body_processing(sp,
                                            /*discard_function_body=*/FALSE);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
  db_exit();
}  /* rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
