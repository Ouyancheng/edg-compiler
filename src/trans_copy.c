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


/*
Flag that is TRUE if we are in the setup phase for trans_copy.c.
*/
static a_boolean in_trans_copy_setup;


/*
Return TRUE if the given entry has the flag set that indicates that
it needs to be copied.  The il_walk_flag is used for this purpose.
*/
#define entry_needs_copy_flag_is_set(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag)

/*
Set the flag that indicates that an entry needs to be copied.
*/
#define set_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = TRUE)

/*
Reset the flag that indicates that an entry needs to be copied.
*/
#define reset_entry_needs_copy_flag(ptr) \
  (il_entry_prefix_of(ptr).il_walk_flag = FALSE)


static void copy_entry(char             *ptr,
                       an_il_entry_kind kind);
static void copy_string_entry(char             *ptr,
                              an_il_entry_kind kind,
                              sizeof_t         length);
static a_boolean copy_termination_test(char             *ptr,
                                       an_il_entry_kind kind);
static void copy_address_setup(
                             char             *ptr,
                             an_il_entry_kind kind,
                             a_boolean        known_will_process_in_curr_walk);
static
void rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary(void);


static char *f_transitive_copy_address_of(char *ptr)
/*
Return the copy address for the indicated entry.  In the case where the
entry is to be merged, and the copy address points to the intermediate
copy, get the ultimate primary IL copy address from the copy.  The
copy address must be set (i.e., the return value is always non-NULL).
*/
{
  ptr = checked_trans_unit_copy_address_of(ptr);
  check_assertion(ptr != NULL);
  if (in_secondary_trans_unit(ptr)) {
    /* In a merged entry, the copy address points to the space for a
       copy in the secondary translation unit, and that in turn points to
       the address in the primary IL. */
    ptr = checked_trans_unit_copy_address_of(ptr);
    check_assertion(ptr != NULL);
  }  /* if */
  check_assertion(!in_secondary_trans_unit(ptr));
  return ptr;
}  /* f_transitive_copy_address_of */


/*
Macro that provides a convenient interface to f_transitive_copy_address_of.
*/
#define transitive_copy_address_of(ptr) \
  f_transitive_copy_address_of((char *)(ptr))


static char *primary_il_entry_of(char             *ptr,
                                  an_il_entry_kind kind)
/*
Return the address in the primary IL that the entry at "ptr" of kind "kind"
corresponds to.  ptr must either be an address in the primary IL (in
which case it is returned) or must be an address of an entry in a secondary
translation unit, in which case the copy address of the entry will be set
if it is not already set.
*/
{
  if (in_secondary_trans_unit(ptr) && in_file_scope(ptr)) {
    copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/FALSE);
    ptr = transitive_copy_address_of(ptr);
  }  /* if */
  return ptr;
}  /* primary_il_entry_of */


static void copy_address_setup(
                              char             *ptr,
                              an_il_entry_kind kind,
                              a_boolean        known_will_process_in_curr_walk)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit to set up the copy address
pointer of the entry pointed to by ptr, of kind "kind".
known_will_process_in_curr_walk is TRUE if it is known that the entry
has been or will be processed (and not merely have its address remapped)
in the current IL walk.
*/
{

  if (ptr == NULL) {
    /* Ignore NULL pointers. */
  } else if (!in_file_scope(ptr)) {
    /* This entry is in a function scope memory region, so do nothing. */
  } else if (!in_secondary_trans_unit(ptr)) {
    /* This entry is in the primary file IL, so do nothing. */
    /* Add it as an orphan in case this reference from a secondary translation
       unit is the only one to it. */
    f_possibly_add_orphaned_file_scope_il_entry(ptr, kind, translation_units);
  } else if (trans_unit_copy_address_of(ptr) != NULL) {
    /* A copy address has already been assigned to this entry. */
  } else {
    a_source_correspondence_ptr  scp;
    a_trans_unit_corresp_ptr     tucp = NULL;
    /* See whether the entry has a source correspondence field.
       If it does, it may correspond to something in another translation
       unit. */
    scp = source_corresp_for_il_entry(ptr, kind);
    if (scp != NULL) tucp = scp->trans_unit_corresp;
    if (tucp != NULL && tucp->canonical != ptr) {
      /* This entry is in a correspondence set but it's not the canonical
         entry.  Set the copy address on the canonical entry and then use
         the same address here. */
      trans_unit_copy_address_of(ptr) =
                                    primary_il_entry_of(tucp->canonical, kind);
    } else if (tucp != NULL && tucp->primary != NULL) {
      /* This entry corresponds to an entry in the primary IL, so
         set the copy address to that entry. */
      trans_unit_copy_address_of(ptr) = tucp->primary;
    } else {
      /* The entry needs to be copied to the primary IL.  Allocate space
         for it in the primary file scope, and set the copy address pointer
         to point to that space.  The entry is copied into that space a
         little later. */
      /* String entries are allocated in copy_string_entry because the
         length is known there. */
      if (!is_string_entry_kind(kind)) {
        char *copy = alloc_primary_file_scope_il(sizeof_il_entry[(int)kind]);
        trans_unit_copy_address_of(ptr) = copy;
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
             unit, or it might be an unrecorded orphan.  Do the copy now
             to make sure it gets done. */
          /* Don't do this copy now if we're still in
             prepare_for_trans_unit_copy and not yet in the copy phase. */
          if (!in_trans_copy_setup) {
            walk_il_subtree(copy_entry, copy_string_entry,
                            (a_remap_function_ptr)NULL,
                            copy_termination_test,
                            /*clear_fe_pointers=*/FALSE,
                            ptr, kind);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* copy_address_setup */


static a_boolean copy_termination_test(char             *ptr,
                                       an_il_entry_kind kind)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit.  Returns TRUE if the walk should be
pruned at the entry pointed to by ptr, of kind "kind".
*/
{
  a_boolean prune;

  /* Make sure the copy address pointer, if any, is set. */
  copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/TRUE);
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

  if (ptr == NULL) {
    /* Leave a NULL pointer unchanged. */
    corresp = NULL;
  } else if (!in_secondary_trans_unit(ptr)) {
    /* Leave a primary IL pointer unchanged. */
    corresp = ptr;
  } else if (!in_file_scope(ptr)) {
    /* Leave a function scope pointer unchanged. */
    corresp = ptr;
  } else {
    /* If the pointer wasn't encountered previously, make sure its
       copy address pointer is set.  This happens for "next" pointers. */
    /* When this routine is called for remap_ptr references in walk_entry.h,
       we don't know that the entry will be seen on this walk.  The
       entities pointed to by "next" pointers actually will appear in
       this walk, but we don't have any way of identifying those. */
    copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/FALSE);
    /* Fetch the copy address assigned by copy_address_setup. */
    corresp = transitive_copy_address_of(ptr);
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
copy address pointer to point to the copy.
*/
{
  /* Ignore strings that are already in the primary file IL (such as
     name strings from the symbol header). */
  if (in_secondary_trans_unit(ptr)) {
    char *copy = alloc_primary_file_scope_il(length);
    checked_trans_unit_copy_address_of(ptr) = copy;
    (void)memcpy(copy, ptr, size_t_arg(length));
  }  /* if */
}  /* copy_string_entry */


static void copy_entry_basic(char                 *ptr,
                             an_il_entry_kind     kind,
                             a_remap_function_ptr remap_function)
/*
Called during the IL walk that copies IL from the secondary translation
unit to the primary translation unit, to copy the IL entry at ptr
(of kind "kind") to the space indicated by its copy address pointer,
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
    copy = checked_trans_unit_copy_address_of(ptr);
    check_assertion_str(copy != NULL,
                        "copy_entry_basic: NULL copy address pointer");
    /* Copy the entry to its corresponding space and remap the pointers
       in the copy. */
    (void)memcpy(copy, ptr, size_t_arg(sizeof_il_entry[(int)kind]));
    walk_remap_func = remap_function;
    remap_pointers_in_il_entry(copy, kind);
    walk_remap_func = saved_walk_remap_func;
    scp = source_corresp_for_il_entry(copy, kind);
    if (scp != NULL) {
      a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;
      if (tucp != NULL && !in_secondary_trans_unit(copy)) {
        /* This entry is the canonical one, so update the canonical pointer
           to point to the copy in the primary IL.  For the "merge" case,
           the overwrite_primary_xxx routine updates the canonical pointer. */
        check_assertion(tucp->canonical == ptr);
        tucp->canonical = copy;
      }  /* if */
      scp->copied_from_secondary_trans_unit = TRUE;
    }  /* if */
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
(of kind "kind") to the space indicated by its copy address pointer,
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


static void establish_as_canonical(a_source_correspondence *scp)
/*
If the entity with the indicated source correspondence has a trans-unit
correspondence, make it the canonical entry of the correspondence set.
This is used when an entry is copied to the primary IL or overwrites
the primary IL entry, to establish the copy as the canonical entry.
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp != NULL) {
    tucp->canonical = (char *)scp;
  }  /* if */
}  /* establish_as_canonical */


static void switch_canonical_for_deleted_definition(
                                                  a_source_correspondence *scp)
/*
The definition of the entity with the indicated source correspondence has
been deleted.  If the entity is the canonical entry or a correspondence
set, and there is a primary IL entry that's now just as good, switch the
canonical entry to the primary IL entry.  Note that the entity passed
in must not be a specialization (in that case, the secondary IL copy
remains better than the primary IL copy).
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp != NULL && tucp->canonical == (char *)scp) {
    /* This entity is the canonical entry. */
    if (tucp->primary != NULL) {
      tucp->canonical = tucp->primary;
    }  /* if */
  }  /* if */
}  /* switch_canonical_for_deleted_definition */


static void remove_dynamic_initialization(a_dynamic_init_ptr dip);


static void remove_constant_initializer_lifetimes(a_constant_ptr con)
/*
The indicated constant is part of an initializer.  The initializer
is being deleted.  Unlink any object lifetimes associated with
the constant (e.g., if it's a nonconstant aggregate).
*/
{
  if (con->kind == (a_constant_repr_kind)ck_aggregate) {
    a_constant_ptr sub_con;
    for (sub_con = con->variant.aggregate.first_constant;
         sub_con != NULL;
         sub_con = sub_con->next) {
      remove_constant_initializer_lifetimes(sub_con);
    }  /* for */
  } else if (con->kind == (a_constant_repr_kind)ck_dynamic_init) {
    remove_dynamic_initialization(con->variant.dynamic_init);
  } else if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    remove_constant_initializer_lifetimes(con->variant.init_repeat.constant);
  }  /* if */
}  /* remove_constant_initializer_lifetimes */


static void remove_dynamic_initialization(a_dynamic_init_ptr dip)
/*
Remove the indicated dynamic initialization from any initialization
and destruction lists.  Also remove any nested object lifetimes.
*/
{
  an_object_lifetime_ptr lifetime;

  lifetime = init_expr_lifetime_of(dip);
  if (lifetime != NULL) {
    /* There is a nested object lifetime.  Eliminate it and everything in
       it. */
    detach_from_object_lifetime_tree(lifetime);
    dip->init_expr_lifetime = NULL;  /* To be neat. */
  } else if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    /* Remove any lifetimes on aggregate member initializers. */
    remove_constant_initializer_lifetimes(dip->variant.constant);
  }  /* if */
  remove_from_destruction_list(dip);
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
    a_dynamic_init_ptr dip = variable->initializer.dynamic;
    remove_dynamic_initialization(dip);
    check_assertion(!variable->source_corresp.is_local_to_function);
    /* Remove the dynamic initialization from the file-scope initializations
       list. */
    { a_dynamic_init_ptr prev_dip;
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
                              "clear_variable_definition: entry not found");
          if (prev_dip->next == dip) break;
        }  /* for */
        prev_dip->next = dip->next;
      }  /* if */
      if (dip->next == NULL) {
        pointers_block->last_dynamic_init = prev_dip;
      }  /* if */
      dip->next = NULL;  /* To be neat. */
    }
  }  /* if */
  variable->init_kind = (an_init_kind)initk_none;
  if (variable->storage_class == (a_storage_class)sc_unspecified) {
    variable->storage_class = (a_storage_class)sc_extern;
  }  /* if */
  if (!variable->is_specialized) {
    switch_canonical_for_deleted_definition(&variable->source_corresp);
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
  if (!routine->is_specialized) {
    switch_canonical_for_deleted_definition(&routine->source_corresp);
  }  /* if */
}  /* clear_body_for_routine */


/*
Return TRUE if an entry should be copied (and not merged) to the primary IL.
The entry must be of a kind that has a source correspondence field.
The entry is copied if either (a) it has no correspondence, e.g., because
it has no linkage, or (b) it has a correspondence and it is the canonical
entry of that correspondence set (i.e., it's the one that should be copied),
and there is no primary IL entry that the entry must overwrite.
*/
#define entry_should_be_copied(ptr) \
  (trans_unit_corresp_of(ptr) == NULL || \
   (trans_unit_corresp_of(ptr)->canonical == (char *)(ptr) && \
    trans_unit_corresp_of(ptr)->primary == NULL))


/*
Return TRUE if an entry should overwrite a corresponding IL entry in
the primary IL.  Overwriting is one kind of "merge".
*/
#define entry_should_overwrite_primary_entry(ptr) \
  (trans_unit_corresp_of(ptr) != NULL && \
   trans_unit_corresp_of(ptr)->canonical == (char *)(ptr) && \
   trans_unit_corresp_of(ptr)->primary != NULL)


/*
Return TRUE if the indicated entry should be kept in the IL, either
so that it can be copied or so that it can be merged.  Note that for
classes and routines there may be other reasons for keeping the entity.
*/
#define entry_should_be_kept(ptr) \
  (entry_should_be_copied(ptr) || entry_should_overwrite_primary_entry(ptr))


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
      if (!same_entities(list1->class_type, list2->class_type)) break;
    }  /* for */
    if (list1 == NULL && list2 == NULL) {
      /* The lists matched up in the same order. */
      /* need_merge = FALSE;  -- already set. */
    } else {
      /* Try the match in any order on the remaining entries. */
      for (clep1 = list1; clep1 != NULL; clep1 = clep1->next) {
        for (clep2 = list2; clep2 != NULL; clep2 = clep2->next) {
          if (same_entities(clep1->class_type, clep2->class_type)) break;
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


static void process_variable_if_unneeded_non_template(a_variable_ptr variable)
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
}  /* process_variable_if_unneeded_non_template */


static void process_routine_if_unneeded_non_template(a_routine_ptr routine)
/*
If we're processing the current secondary translation unit only to
get exported templates, and the given routine is not a generated template,
do any necessary processing, e.g., externalizing it if it is static.
*/
{
  if (is_nontemplate_routine_from_exported_trans_unit(routine)) {
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
}  /* process_routine_if_unneeded_non_template */

#if CHECKING

static int corresp_ranking(char             *ptr,
                           an_il_entry_kind kind)
/*
Return a correspondence ranking for the indicated entry, of kind "kind".
The entry with the highest value should be the canonical entry.
*/
{
  int rank = 0;

  switch (kind) {
    case iek_type:
      { a_type_ptr type = (a_type_ptr)ptr;
        if (is_immediate_class_type(type)) {
          rank = class_type_has_body(type);
          if (type->variant.class_struct_union.is_specialized) rank += 2;
        } else if (is_immediate_enum_type(type)) {
          rank = !is_incomplete_type(type);
        } else {
          rank = 0;
        }  /* if */
      }
      break;
    case iek_variable:
      { a_variable_ptr var = (a_variable_ptr)ptr;
        rank = (var->storage_class == (a_storage_class)sc_unspecified);
        if (var->is_specialized) rank += 2;
      }
      break;
    case iek_routine:
      { a_routine_ptr rout = (a_routine_ptr)ptr;
        rank = (rout->assoc_scope != NULL_region_number);
        if (rout->is_specialized) rank += 2;
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  return rank;
}  /* corresp_ranking */


static void check_correspondences(a_source_correspondence *scp,
                                  an_il_entry_kind        kind)
/*
Check that the correspondences, if any, established for the entity whose
source correspodence field is scp and whose kind is "kind" are consistent.
*/
{
  a_trans_unit_corresp_ptr tucp = scp->trans_unit_corresp;

  if (tucp == NULL) {
    /* An entry without a correspondence should not have external linkage. */
    if (scp->name != NULL &&
        (scp->name_linkage == (a_name_linkage_kind)nlk_external ||
         scp->name_linkage == (a_name_linkage_kind)nlk_cplusplus_external)) {
#if DEBUG
      db_entity_info((char *)scp, kind);
#endif /* DEBUG */
      unexpected_condition_str(
                    "entity with external linkage does not have corresp info");
    }  /* if */
  } else {
    if (tucp->canonical == tucp->primary) {
      if (corresp_ranking(tucp->primary, kind) <
          corresp_ranking((char*)scp, kind)) {
        /* The primary IL entry is the canonical one, but the current entry
           is better.  That means the correspondence information is wrong. */
#if DEBUG
        db_entity_info(tucp->primary, kind);
        db_entity_info((char *)scp, kind);
#endif /* DEBUG */
        unexpected_condition_str("primary entry should not be canonical");
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_correspondences */

#else /* !CHECKING */
#define check_correspondences(scp, kind) /* Nothing */
#endif /* CHECKING */


static void f_mark_to_merge(char             *ptr,
                            an_il_entry_kind kind)
/*
Mark the given entry (of kind "kind") as one that must be merged with its
counterpart in the primary IL.  copy_address_setup will be called
to set the copy address if it is not set already, so if a special
copy address is required it should be established before this routine
is called.
*/
{
  char *primary, *copy;

  check_assertion_str(in_file_scope(ptr) && in_secondary_trans_unit(ptr),
                      "f_mark_to_merge: bad input pointer");
  /* If the flag is set already, do nothing. */
  if (!il_entry_prefix_of(ptr).il_lowering_flag) {
    /* Set a flag to request merging.  The IL lowering flag is borrowed
       for this process because IL lowering is not done on secondary
       translation units. */
    il_entry_prefix_of(ptr).il_lowering_flag = TRUE;
    /* Get the copy address set if it is not set already. */
    copy_address_setup(ptr, kind, /*known_will_process_in_curr_walk=*/TRUE);
    primary = trans_unit_copy_address_of(ptr);
    check_assertion_str(primary != NULL,
                        "f_mark_to_merge: copy address is not set");
    check_assertion_str(!in_secondary_trans_unit(primary),
                        "f_mark_to_merge: copy address is in sec trans unit");
    /* Allocate space for a copy in the secondary translation unit IL
       so we will have a version with all the pointers remapped appropriately.
       The original entry points to the copy, which points (via its
       copy address pointer) to the primary IL. */
    check_assertion(!is_string_entry_kind(kind));
    copy = alloc_il(sizeof_il_entry[(int)kind]);
    trans_unit_copy_address_of(ptr) = copy;
    trans_unit_copy_address_of(copy) = primary;
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
}  /* f_mark_to_merge */

/*
Macro interface to f_mark_to_merge, which allows it to be called for
entries of various kinds.
*/
#define mark_to_merge(ptr, kind) f_mark_to_merge((char *)(ptr), (kind))

/*
Return TRUE if the given entry is to be merged with its counterpart
in the primary IL (this tests a flag, which must have been set previously).
*/
#define entry_to_be_merged(ptr) \
  (il_entry_prefix_of(ptr).il_lowering_flag)


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
the lists, and entities that must be merged are marked with the merge
flag.  This routine is called with the current translation unit set
to the secondary translation unit.
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
    check_assertion(il_entry_prefix_of(scope).il_walk_flag == 0);
    checked_trans_unit_copy_address_of(scope) = (char *)corresp_scope;
    mark_to_merge(scope, iek_scope);
    if (scope->lifetime != NULL && corresp_scope->lifetime != NULL) {
      /* The object lifetime of the file scope corresponds with the
         object lifetime of the corresponding scope, and gets merged into
         it. */
      checked_trans_unit_copy_address_of(scope->lifetime) =
                                               (char *)corresp_scope->lifetime;
      mark_to_merge(scope->lifetime, iek_object_lifetime);
    }  /* if */
    keep_on_parent_list = TRUE;
    check_member_merges = TRUE;
    pointers_block = get_pointers_block_for_scope(scope);
    check_assertion(pointers_block != NULL);
  } else if (scope->kind == (a_scope_kind)sck_namespace) {
    /* For a namespace scope, go to the a_namespace entry to find out what
       correspondence there is, if any. */
    nsp = scope->variant.assoc_namespace;
    if (entry_should_be_copied(nsp)) {
      /* The namespace doesn't exist in the primary IL, and just gets
         copied over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else {
      /* The namespace scope gets merged into the corresponding scope. */
      a_namespace_ptr corresp_nsp= (a_namespace_ptr)canonical_il_entry_of(nsp);
      checked_trans_unit_copy_address_of(scope) =
                  primary_il_entry_of((char *)corresp_nsp->variant.assoc_scope,
                                      iek_scope);
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
    if (entry_should_be_copied(class_type)) {
      /* The class doesn't exist in the primary IL, and just gets copied
         over.  We don't need to check its members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
    } else if (entry_should_overwrite_primary_entry(class_type)) {
      /* The class overwrites the corresponding class in the primary IL.
         This happens, for example, when the class in the primary IL has
         a declaration and the class in the secondary IL has a definition.
         The definition will be copied over, but we don't need to check
         the members. */
      keep_on_parent_list = TRUE;
      check_member_merges = FALSE;
      mark_to_merge(class_type, iek_type);
    } else {
      /* The class gets merged into the corresponding class. */
      a_type_ptr corresp_class = (a_type_ptr)canonical_il_entry_of(class_type);
      check_assertion(same_entities(corresp_class, class_type));
      if (class_type_has_body(corresp_class)) {
        /* Both instances of the class have definitions, so their scopes
           correspond. */
        a_scope_ptr corresp_class_scope = corresp_class->variant.
                                    class_struct_union.extra_info->assoc_scope;
        checked_trans_unit_copy_address_of(scope) =
                       (char *)primary_il_entry_of((char *)corresp_class_scope,
                                                   iek_scope);
      }  /* if */
      /* The class here needs to be kept only if some of its members need
         to be processed.  Assume there are none, and correct that assumption
         as we look at the members. */
      keep_on_parent_list = FALSE;
      check_member_merges = TRUE;
      /* Check for other reasons to do a merge, e.g., befriending lists. */
      if (!C_mode() &&
          class_befriending_lists_need_to_be_merged(class_type,
                                                    corresp_class)) {
        keep_on_parent_list = TRUE;
        mark_to_merge(class_type, iek_type);
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
    check_correspondences(&type->source_corresp, iek_type);
    keep_on_list = TRUE;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL &&
        type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
      /* A class with a scope.  Do a recursive call to process it. */
      a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
      keep_on_list = prepare_for_trans_unit_copy(class_scope,
                                                 any_removed_function_bodies);
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
      keep_on_list = entry_should_be_kept(ref_type);
    } else if (entry_should_be_copied(type)) {
      /* The type doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(type)) {
      /* The type overwrites the corresponding type in the primary IL.
         This happens, for example, when the type is an enum with a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      keep_on_list = TRUE;
      mark_to_merge(type, iek_type);
    } else {
      /* The type is a duplicate of one elsewhere and should be discarded. */
      keep_on_list = FALSE;
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
    check_correspondences(&variable->source_corresp, iek_variable);
    keep_on_list = TRUE;
    /* If we're supposed to copy only generated templates, other variables
       are made external (if necessary) and their definitions are
       dropped (the definition will be put out when the file
       is compiled as a primary file). */
    process_variable_if_unneeded_non_template(variable);
    if (entry_should_be_copied(variable)) {
      /* The variable doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(variable)) {
      /* The variable overwrites the corresponding variable in the primary IL.
         This happens, for example, when the variable has a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      keep_on_list = TRUE;
      mark_to_merge(variable, iek_variable);
    } else {
      /* The variable is a duplicate of one elsewhere and should be
         discarded. */
      keep_on_list = FALSE;
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
          trans_unit_corresp_of(variable->type) == NULL &&
          trans_unit_copy_address_of(variable->type) == NULL) {
        a_variable_ptr corresp_variable =
                               (a_variable_ptr)canonical_il_entry_of(variable);
        trans_unit_copy_address_of(variable->type) =
                 primary_il_entry_of((char *)corresp_variable->type, iek_type);
      }  /* if */
      if (variable->storage_class == (a_storage_class)sc_unspecified) {
        /* Delete the definition of this variable. */
        clear_variable_definition(variable);
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
    check_correspondences(&routine->source_corresp, iek_routine);
    keep_on_list = TRUE;
    /* If we're supposed to copy only generated templates, other routines
       are made external (if necessary) and their definitions are
       dropped. */
    process_routine_if_unneeded_non_template(routine);
    if (entry_should_be_copied(routine)) {
      /* The routine doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else {
      a_routine_ptr corresp_routine =
                                 (a_routine_ptr)canonical_il_entry_of(routine);
      if (entry_should_overwrite_primary_entry(routine)) {
        /* The routine overwrites the corresponding routine in the primary IL.
           This happens, for example, when the routine has a definition
           in the secondary translation unit but only a declaration in the
           primary IL. */
        check_assertion(check_member_merges);
        keep_on_list = TRUE;
        mark_to_merge(routine, iek_routine);
        /* The inline flag merging below gets done against the primary IL
           routine. */
        corresp_routine = (a_routine_ptr)transitive_copy_address_of(routine);
      } else if (check_member_merges && !C_mode() &&
                 befriending_lists_need_to_be_merged(
                                       routine->befriending_classes,
                                       corresp_routine->befriending_classes)) {
        /* For the most part, this routine is a duplicate of one elsewhere,
           but it does happen to have some additional befriending information,
           so mark it to have that information merged. */
        keep_on_list = TRUE;
        mark_to_merge(routine, iek_routine);
      } else {
        /* The routine is a duplicate of one elsewhere and should be
           discarded. */
        keep_on_list = FALSE;
      }  /* if */
      /* Preserve some information regarding inline functions.  Note that
         this is done for routines that are merged and routines that are
         discarded, but not for routines that are just copied. */
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
    } else {
      /* Remove this entry from the list. */
      if (prev_routine == NULL) {
        scope->routines = routine->next;
      } else {
        prev_routine->next = routine->next;
      }  /* if */
      /* If we need the type of this routine, the type of the corresponding
         routine can be used. */
      if (in_secondary_trans_unit(routine->type) &&
          trans_unit_corresp_of(routine->type) == NULL &&
          trans_unit_copy_address_of(routine->type) == NULL) {
        a_routine_ptr corresp_routine =
                               (a_routine_ptr)canonical_il_entry_of(routine);
        trans_unit_copy_address_of(routine->type) =
                  primary_il_entry_of((char *)corresp_routine->type, iek_type);
      }  /* if */
      if (routine->assoc_scope != NULL_region_number) {
        /* Delete the body of this routine. */
        clear_body_for_routine(routine);
        *any_removed_function_bodies = TRUE;
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
    if (entry_should_be_copied(templ)) {
      /* The template doesn't exist in the primary IL, and just gets copied
         over. */
      keep_on_list = TRUE;
    } else if (entry_should_overwrite_primary_entry(templ)) {
      /* The template overwrites the corresponding template in the primary IL.
         This happens, for example, when the template has a definition
         in the secondary translation unit but only a declaration in the
         primary IL. */
      check_assertion(check_member_merges);
      /* Eliminate the link to the primary IL copy in this case, to
         keep both entries. */
      trans_unit_corresp_of(templ)->primary = NULL;
      keep_on_list = TRUE;
    } else {
      /* The template is a duplicate of one elsewhere and should be
         discarded. */
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
    if (!nsp->is_namespace_alias) {
      /* Do a recursive call to process the namespace. */
      keep_on_list = prepare_for_trans_unit_copy(nsp->variant.assoc_scope,
                                                 any_removed_function_bodies);
    } else {
      /* A namespace alias.  Keep it only if it needs to be copied to the
         primary IL. */
      keep_on_list = entry_should_be_copied(nsp);
      check_assertion(!entry_should_overwrite_primary_entry(nsp));
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
    a_source_correspondence_ptr scp;
    /* Keep the pragma if it has an associated entity that will be kept. */
    keep_on_list = FALSE;
    if (pragma->entity.ptr != NULL &&
        (scp = source_corresp_for_il_entry(
                             pragma->entity.ptr,
                             (an_il_entry_kind)pragma->entity.kind)) != NULL &&
        /* "a_constant_ptr" here is arbitrary. */
        entry_should_be_kept((a_constant_ptr)scp)) {
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
    mark_to_merge(scope, iek_scope);
    if (scope->kind == (a_scope_kind)sck_namespace) {
      nsp = scope->variant.assoc_namespace;
      mark_to_merge(nsp, iek_namespace);
    } else if (scope->kind == (a_scope_kind)sck_class_struct_union) {
      class_type = scope->variant.assoc_type;
      mark_to_merge(class_type, iek_type);
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
    lifetime = (an_object_lifetime_ptr)checked_trans_unit_copy_address_of(
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


static void merge_befriending_classes_lists(a_class_list_entry_ptr *plist1,
                                            a_class_list_entry_ptr list2)
/*
Merge the befriending lists pointed to by *plist1 and list2, and
update *plist1 to point to the merged list.
*/
{
  a_class_list_entry_ptr clep1, clep2, clep2_next, list1 = *plist1;

  for (clep2 = list2; clep2 != NULL; clep2 = clep2_next) {
    clep2_next = clep2->next;
    for (clep1 = list1; clep1 != NULL; clep1 = clep1->next) {
      if (same_entities(clep1->class_type, clep2->class_type)) break;
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
  a_symbol_ptr                sym =
                               (a_symbol_ptr)(type->source_corresp.assoc_info);
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
  establish_as_canonical(&primary_type->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the type in the primary IL. */
    switch (sym->kind) {
      case sk_type:
        sym->variant.type.ptr = primary_type;
        break;
      case sk_enum_tag:
        sym->variant.enumeration.type = primary_type;
        break;
      case sk_class_or_struct_tag:
      case sk_union_tag:
        sym->variant.class_struct_union.type = primary_type;
        break;
      default:
        unexpected_condition_str("overwrite_primary_type: bad symbol kind");
    }  /* switch */
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
  a_symbol_ptr  sym = (a_symbol_ptr)(var->source_corresp.assoc_info);
  do_saves_for_overwrite(primary_var, a_variable_ptr);
  *primary_var = *var;
  do_restores_for_overwrite(primary_var, var);
#if ONE_INSTANTIATION_PER_OBJECT
  primary_var->instantiation_needed_bit_number =
                                         saved_instantiation_needed_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  establish_as_canonical(&primary_var->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the variable in the primary IL. */
    switch (sym->kind) {
      case sk_variable:
        sym->variant.variable.ptr = primary_var;
        break;
      case sk_static_data_member:
        sym->variant.static_data_member.variable = primary_var;
        break;
      default:
        unexpected_condition_str(
                                "overwrite_primary_variable: bad symbol kind");
    }  /* switch */
  }  /* if */
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
  a_symbol_ptr sym = (a_symbol_ptr)(rout->source_corresp.assoc_info);
  do_saves_for_overwrite(primary_rout, a_routine_ptr);
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
  establish_as_canonical(&primary_rout->source_corresp);
  if (sym != NULL) {
    /* Make the symbol (in a secondary translation unit) point to the
       copy of the routine in the primary IL. */
    switch (sym->kind) {
      case sk_routine:
      case sk_member_function:
        sym->variant.routine.ptr = primary_rout;
        break;
      default:
        unexpected_condition_str("overwrite_primary_routine: bad symbol kind");
    }  /* switch */
  }  /* if */
}  /* overwrite_primary_routine */


/*
Change a pointer to the corresponding address in the primary IL.
*/
#define change_pointer_to_primary_IL_pointer(ptr, ptr_type) \
{ if ((ptr) != NULL) \
  (ptr) = (ptr_type)checked_trans_unit_copy_address_of(ptr); }


static void update_namespace_pointers_block(a_scope_ptr scope)
/*
scope is the secondary translation unit scope for a namespace that has
been copied to the primary IL rather than merged.  Update its
pointers block so that its last-pointers point to the copied
entries in the primary IL.
*/
{
  a_scope_ptr            primary_scope =
                        (a_scope_ptr)checked_trans_unit_copy_address_of(scope);
  a_scope_pointers_block *pointers_block =
                                   get_pointers_block_for_scope(primary_scope);
  a_namespace_ptr        sub_nsp;

  change_pointer_to_primary_IL_pointer(pointers_block->last_constant,
                                       a_constant_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_type,
                                       a_type_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_variable,
                                       a_variable_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_routine,
                                       a_routine_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_asm_entry,
                                       an_asm_entry_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_dynamic_init,
                                       a_dynamic_init_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_namespace,
                                       a_namespace_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_using_decl,
                                       a_using_decl_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_pragma,
                                       a_pragma_ptr);
  change_pointer_to_primary_IL_pointer(pointers_block->last_template,
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
processing required after the IL walk has copied IL entries from the
secondary scope to the primary file IL.  This includes putting copied
entries on primary IL lists and merging entries into the corresponding
primary IL entries.  This routine is called with the current translation
unit set to the primary translation unit.
*/
{
  a_scope_ptr            primary_scope;
  a_scope_pointers_block *pointers_block;
  a_boolean              is_class_scope =
                         (scope->kind == (a_scope_kind)sck_class_struct_union);
  a_boolean              move_to_end;

  check_assertion(is_primary_translation_unit);
  /* Process only scopes that must be merged into their counterparts. */
  if (entry_to_be_merged(scope)) {
    /* Find the corresponding scope. */
    primary_scope = (a_scope_ptr)transitive_copy_address_of(scope);
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
                          (a_type_ptr)checked_trans_unit_copy_address_of(type);
        if (is_immediate_class_type(type) &&
            type->variant.class_struct_union.extra_info != NULL &&
            type->variant.class_struct_union.extra_info->assoc_scope != NULL) {
          /* A class with a scope.  Do a recursive call to process it. */
          a_scope_ptr class_scope = type->variant.class_struct_union.
                                                       extra_info->assoc_scope;
          finish_trans_unit_copy(class_scope);
        }  /* if */
        if (!entry_to_be_merged(type)) {
          /* An entry that was simply copied. */
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
                  (a_type_ptr)checked_trans_unit_copy_address_of(corresp_type);
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
            if (!entry_should_overwrite_primary_entry(type)) {
              /* No overwriting is needed, so we're done.  This happens,
                 for example, when the only reason for merging is to merge
                 the befriending lists, or when the class is marked to be
                 merged because some of its members need to be merged. */
              goto end_of_type_list_add;
            }  /* if */
          }  /* if */
          /* Copy this type and its definition, overwriting the
             existing primary type.  Move the primary IL type
             to the end of the types list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move to the end of the list when the
             type being moved is a declaration (that can
             happen when a definition in a secondary translation
             unit is chosen as the canonical entry, and then its
             definition is not needed anywhere and is removed by
             the unneeded-entity removal processing). */
          move_to_end = (!is_class_scope &&
                         class_type_has_body(corresp_type));
          if (move_to_end) {
            /* Also remove any associated namespace placeholder, but do
               not move it to the end of the list.  There will be a
               placeholder in the secondary IL that gets moved over. */
            move_to_end_of_types_list(primary_type, NO_SCOPE_DEPTH,
                                      /*delete_placeholder=*/TRUE);
          }  /* if */
          overwrite_primary_type(corresp_type, primary_type);
          corresp_type = primary_type;
          if (!move_to_end) goto end_of_type_list_add;
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
                  (a_variable_ptr)checked_trans_unit_copy_address_of(variable);
        if (entry_to_be_merged(variable)) {
          /* The entry gets merged into the corresponding variable. */
          a_variable_ptr primary_variable =
                     (a_variable_ptr)checked_trans_unit_copy_address_of(
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
            clear_variable_definition(primary_variable);
          }  /* if */
          /* Copy this variable and its definition, overwriting the
             existing primary variable.  Move the primary IL variable
             to the end of the variables list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move if a specialization declaration replaces
             an unspecialized variable (with or without a definition). */
          move_to_end = (!is_class_scope &&
                         corresp_variable->storage_class ==
                                              (a_storage_class)sc_unspecified);
          if (move_to_end) {
            remove_from_variables_list(primary_variable, NO_SCOPE_DEPTH);
            last_variable = pointers_block->last_variable;
          }  /* if */
          overwrite_primary_variable(corresp_variable, primary_variable);
          corresp_variable = primary_variable;
          if (!move_to_end) goto end_of_variable_list_add;
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
      }  /* for */
    }  /* if */
    if (scope->dynamic_inits != NULL) {
      /* Add the dynamic initializations of "scope" to the end of the
         dynamic initializations list of "primary scope". */
      a_dynamic_init_ptr copied_inits =
          (a_dynamic_init_ptr)transitive_copy_address_of(scope->dynamic_inits);
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
                    (a_routine_ptr)checked_trans_unit_copy_address_of(routine);
        if (entry_to_be_merged(routine)) {
          /* The entry gets merged into the corresponding routine. */
          a_routine_ptr primary_routine =
                   (a_routine_ptr)checked_trans_unit_copy_address_of(
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
          if (!entry_should_overwrite_primary_entry(routine)) {
            /* No overwriting is needed, so we're done.  This happens,
               for example, when the only reason for merging is to merge
               the befriending lists. */
            goto end_of_routine_list_add;
          }  /* if */
          if (primary_routine->assoc_scope != NULL_region_number) {
            /* Eliminate the body of the primary routine (this happens when
               the secondary has a specialization and the primary does not). */
            clear_body_for_routine(primary_routine);
          }  /* if */
          /* Copy this routine and its definition, overwriting the
             existing primary routine.  Move the primary IL routine
             to the end of the routines list so that it appears on
             the list at the point where the definition appears.
             Class members are not moved to the end of the list.
             Also do not move if a specialization declaration replaces
             an unspecialized routine (with or without a definition). */
          move_to_end = (!is_class_scope &&
                         corresp_routine->assoc_scope != NULL_region_number);
          if (move_to_end) {
            remove_from_routines_list(primary_routine, NO_SCOPE_DEPTH);
            last_routine = pointers_block->last_routine;
          }  /* if */
#if MAINTAIN_NEEDED_FLAGS
          /* Eliminate any default argument object lifetimes associated with
             the entry that is about to be overwritten. */
          eliminate_routine_default_arg_object_lifetimes(primary_routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
          overwrite_primary_routine(corresp_routine, primary_routine);
          corresp_routine = primary_routine;
          if (!move_to_end) goto end_of_routine_list_add;
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
                     (a_template_ptr)checked_trans_unit_copy_address_of(templ);
        check_assertion(!entry_to_be_merged(templ));
        /* An entry that was copied. */
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
                      (a_namespace_ptr)checked_trans_unit_copy_address_of(nsp);
        if (!entry_to_be_merged(nsp)) {
          /* An entry that was simply copied.  Add it to the end of the
             list. */
          if (last_nsp == NULL) {
            primary_scope->namespaces = corresp_nsp;
          } else {
            last_nsp->next = corresp_nsp;
          }  /* if */
          corresp_nsp->next = NULL;
          last_nsp = corresp_nsp;
          pointers_block->last_namespace = last_nsp;
        }  /* if */
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
                      (a_pragma_ptr)checked_trans_unit_copy_address_of(pragma);
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
               (an_asm_entry_ptr)checked_trans_unit_copy_address_of(asm_entry);
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


static void merge_il_headers(a_translation_unit_ptr tup)
/*
Do merging of the il_header of the indicated secondary translation unit
into the primary translation unit il_header.
*/
{
  check_assertion(is_primary_translation_unit);
  if (tup->il_header.main_routine != NULL) {
    /* "main" is defined in the secondary translation unit.  Indicate
       that it is now defined in the primary translation unit. */
    check_assertion(il_header.main_routine == NULL);
    il_header.main_routine =
        (a_routine_ptr)transitive_copy_address_of(tup->il_header.main_routine);
  }  /* if */
}  /* merge_il_headers */


static void ensure_routine_is_on_inline_list(a_routine_ptr routine)
/*
The indicated routine has been copied or merged from the secondary
translation unit IL to the primary IL.  routine points to the copy in the
secondary translation unit except for members of local class scopes.
Update the "instantiation" lists for extern inline functions, if appropriate.
*/
{
  /* This routine runs while switched to the primary translation unit. */
  check_assertion(is_primary_translation_unit);
  if (instantiate_extern_inline) {
    a_routine_ptr primary_routine;
    if (in_secondary_trans_unit(routine)) {
      primary_routine = (a_routine_ptr)transitive_copy_address_of(routine);
    } else {
      check_assertion(routine->source_corresp.is_local_to_function);
      primary_routine = routine;
    }  /* if */
    if (primary_routine->is_inline &&
        primary_routine->storage_class == (a_storage_class)sc_unspecified &&
        !primary_routine->source_corresp.static_used_by_instantiation) {
      if (primary_routine->on_inline_function_list) {
        /* There is already a list entry for the routine in the primary IL. */
      } else {
        /* Add an entry for the routine. */
        add_to_inline_function_list(primary_routine);
#if DEBUG
        if (db_trace("trans_copy", primary_routine, iek_routine)) {
          fprintf(f_debug, "ensure_routine_is_on_inline_list: adding:\n");
          db_entity_info((char *)primary_routine, iek_routine);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* ensure_routine_is_on_inline_list */


static void finish_moved_entity_processing(a_scope_ptr scope);


static void finish_type_list_moved_entity_processing(a_type_ptr type_list)
/*
Finish processing in the indicated type list and its subscopes for any
entities that were moved or merged from the secondary translation unit IL
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
          finish_moved_entity_processing(class_scope);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* finish_type_list_moved_entity_processing */


static void finish_moved_entity_processing(a_scope_ptr scope)
/*
Finish processing in the indicated scope and its subscopes for any
entities that were moved or merged from the secondary translation unit IL
to the primary IL.  The scope passed in is from the secondary translation
unit except for local class and block scopes.  The processing done here
differs from that done in finish_trans_unit_copy in that all entities, even
those that are members of non-merged scopes (e.g., local classes) are
processed here.  If some processing needs to be done on every entity
moved or merged from the secondary IL, it must be done here.
*/
{
  a_routine_ptr   routine;
  a_namespace_ptr nsp;
  a_scope_ptr     sub_scope;
  a_boolean       is_class_scope = (scope->kind ==
                                         (a_scope_kind)sck_class_struct_union);

  check_assertion(in_secondary_trans_unit(scope) ||
                  (is_class_scope &&
                   scope->variant.assoc_type->source_corresp.
                                                        is_local_to_function));
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      finish_moved_entity_processing(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  finish_type_list_moved_entity_processing(scope->types);
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    ensure_routine_is_on_inline_list(routine);
  }  /* for */
  for (sub_scope = scope->scopes;
       sub_scope != NULL;
       sub_scope = sub_scope->next) {
    finish_moved_entity_processing(sub_scope);
  }  /* for */
#if ONE_INSTANTIATION_PER_OBJECT || MAINTAIN_NEEDED_FLAGS
  { a_variable_ptr  variable;
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      a_variable_ptr primary_variable;

      if (in_secondary_trans_unit(variable)) {
        primary_variable= (a_variable_ptr)transitive_copy_address_of(variable);
      } else {
        check_assertion(variable->source_corresp.is_local_to_function);
        primary_variable = variable;
      }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
      if (one_instantiation_per_object && is_class_scope) {
        /* Assign one-instantiation-per-object needed bit numbers to
           static data members. */
        set_variable_instantiation_needed_bit_number(primary_variable);
      }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#if MAINTAIN_NEEDED_FLAGS
      if (primary_variable->storage_class == (a_storage_class)sc_unspecified ||
          primary_variable->init_kind == (an_init_kind)initk_dynamic) {
        /* Mark an externally-defined variable or one with initialization
           side effects as "needed". */
        mark_as_needed((char *)primary_variable,
                       (an_il_entry_kind)iek_variable);
      }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    }  /* for */
  }
#endif /* ONE_INSTANTIATION_PER_OBJECT || MAINTAIN_NEEDED_FLAGS */
}  /* finish_moved_entity_processing */

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
                                     checked_trans_unit_copy_address_of(solhp);
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

void copy_secondary_trans_unit_IL_to_primary(void)
/*
Copy IL from any secondary translation units to the primary translation
unit IL.  If needed flag processing is configured in, unneeded entities
have already been removed from the secondary translation unit IL and
therefore will not be copied.
*/
{
  a_translation_unit_ptr tup;
  a_scope_ptr            top_scope;

  db_enter(1, "copy_secondary_trans_unit_IL_to_primary");
  check_assertion(total_errors == 0 && !trans_unit_test_mode);
  /* This code doesn't handle source sequence lists, so the result won't
     work with the C++-generating back end. */
  { a_boolean okay = !BACK_END_IS_CP_GEN_BE;
    check_assertion(okay);
  }
  check_assertion(initial_value_for_il_lowering_flag == FALSE);
  in_trans_copy_setup = TRUE;
  /* Loop over each translation unit, preparing for the copy.  This
     decides which entities should be copied, which should be merged,
     and which are duplicates that can be dropped. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    a_boolean   any_removed_function_bodies = FALSE;
    switch_translation_unit(tup);
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "Preparing copy from sec trans unit %s:\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
    top_scope = il_header.primary_scope;
    check_assertion(!il_entry_prefix_of(top_scope).
                    il_lowering_flag);/*lint !e527*/
    (void)prepare_for_trans_unit_copy(top_scope, &any_removed_function_bodies);
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug,
              "Done preparing copy from sec trans unit %s\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  in_trans_copy_setup = FALSE;
  /* Loop over each translation unit, copying the IL. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
    switch_translation_unit(tup);
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "Copying from sec trans unit %s:\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
    top_scope = il_header.primary_scope;
    copy_from_secondary_to_primary_IL();
    /* Also remap pointers in bodies of functions. */
    copy_function_bodies_from_secondary_to_primary_IL(top_scope);
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug,
              "Done copying from sec trans unit %s\n",
              curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  switch_translation_unit(translation_units);
  /* Loop over each translation unit, linking copied entities into the
     primary IL.  Note that this part runs while switched to the
     primary translation unit. */
  for (tup = translation_units->next; tup != NULL; tup = tup->next) {
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "Wrapping up copy from sec trans unit %s:\n",
                       curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
    top_scope = tup->primary_scope;
    finish_trans_unit_copy(top_scope);
    merge_il_headers(tup);
    finish_moved_entity_processing(top_scope);
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
    finish_scope_orphaned_list_processing(
                                   tup->il_header.scope_orphaned_list_headers);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "Done wrapping up copy from sec trans unit %s:\n",
                       curr_translation_unit->source_file->name_as_written);
    }  /* if */
#endif /* DEBUG */
  }  /* for */
  /* Sweep the primary translation unit IL tree and look for any
     pointers to entities in secondary translation units that it uses,
     and rewrite the pointers as the corresponding primary IL entities. */
  rewrite_secondary_trans_unit_IL_entity_pointers_used_in_primary();
  /* Finish processing on function bodies moved to the primary IL,
     including IL lowering if appropriate.  Do this also on any
     function bodies in the primary IL whose lowering was delayed.
     Lowering is delayed on some instantiations in the primary
     translation unit when there are exported templates so that we
     can rewrite any references to secondary translation unit entities
     before the lowering is done. */
  /* Do inline functions in a first pass to have a better chance of inlining
     calls to them. */
  { a_boolean inline_pass = TRUE; 
    for (;;) {
      a_memory_region_number n;
      for (n = FILE_SCOPE_REGION_NUMBER + 1;
           n <= highest_used_region_number;
           n++) {
        if (mem_region_table[n] == NULL) {
          /* This memory has already been freed. */
        } else {
          a_scope_ptr sp = il_header.region_scope_entry[n];
          if (sp->kind == (a_scope_kind)sck_function &&
              sp->variant.routine.ptr->is_inline == inline_pass
#if DO_IL_LOWERING
              && !il_entry_prefix_of(sp).il_lowering_flag
#endif /* DO_IL_LOWERING */
                                                         ) {
            finish_function_body_processing(sp,
                                            /*discard_function_body=*/FALSE);
          }  /* if */
        }  /* if */
      }  /* for */
      if (!inline_pass) break;
      inline_pass = FALSE;
    }  /* for */
  }
  db_exit();
}  /* copy_secondary_trans_unit_IL_to_primary */


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
         so we have to assume it is. */
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
           n++) {
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
primary IL.
*/
{
  char *new_ptr = old_ptr;

  if (old_ptr == NULL) {
    /* Leave a NULL pointer alone. */
  } else if (in_secondary_trans_unit(old_ptr)) {
    check_assertion_str(in_file_scope(old_ptr),
                        "remap_secondary_pointer: not in file scope");
    if (trans_unit_copy_address_of(old_ptr) != NULL) {
      /* The entry already has a copy address assigned. */
      new_ptr = transitive_copy_address_of(old_ptr);
    } else {
      /* No copy address established.  For entities with linkage,
         we can get an address from the associated canonical entry.
         Other things we can copy.  However, we can't copy things
         that go on lists, because we won't get a chance to link
         them on the lists.  For example, the type "pointer to int"
         wouldn't necessarily have a copy address here, but A<int>
         must. */
      /* Basically, primary_il_entry_of can do the copy, but we
         do some extra checking here to make sure that we're not
         copying things that should have been copied already. */
#if CHECKING
      /* Check whether the entity is okay. */
      { a_boolean                err = FALSE;
        a_trans_unit_corresp_ptr tucp;
        /* For entities with linkage, an address should have been
           assigned to the correspondence set, but it might not
           have been established in this entry.  This entry shouldn't
           be the canonical entry -- if it were, its address should
           have been assigned already. */
        if (source_corresp_for_il_entry(old_ptr, kind) != NULL &&
            (tucp = trans_unit_corresp_of_unknown_entry(old_ptr),
             tucp != NULL)) {
          check_assertion_str(tucp->canonical != old_ptr &&
                              (!in_secondary_trans_unit(tucp->canonical) ||
                               checked_trans_unit_copy_address_of(
                                                    tucp->canonical) != NULL),
                  "remap_secondary_pointer: canonical copy addr not assigned");
        } else {
          /* Entity does not have a linkage correspondence. */
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
        }  /* if */
      }
#endif /* CHECKING */
      new_ptr = primary_il_entry_of(old_ptr, kind);
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


static
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
  check_assertion(is_primary_translation_unit);
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
           n++) {
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
