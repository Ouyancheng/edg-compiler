/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2003 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_walk.c -- Routines to walk the intermediate language tree.

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "il_walk.h"

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS

#if !ORPHAN_PROCESSING_NEEDED
 #error -- ORPHAN_PROCESSING_NEEDED must be set if IL walking is needed.
#endif /* !ORPHAN_PROCESSING_NEEDED */

static an_entry_process_function_ptr
		entry_process_func;
			/* The function to be called for each non-string entry.
			   NULL if no function is to be called. */
static a_string_entry_process_function_ptr
		string_entry_process_func;
			/* The function to be called for each string entry.
			   NULL if no function is to be called. */
static a_walk_termination_test_function_ptr
		walk_termination_test_func;
			/* The function to be called to decide on pruning
			   of the IL walk at a given entry, or NULL if the
			   default pruning algorithm (using il_walk_flag)
			   should be used. */
static a_remap_function_ptr
		walk_remap_func;
			/* The function to be used to remap each pointer
			   from an old value to a new value.  NULL if no
			   remapping is to be done. */
static a_remap_function_ptr
		walk_list_remap_func;
			/* The function to be used to remap each pointer
			   from an old value to a new value, for list pointers
			   ("next" pointers and start-of-list pointers).
			   NULL if no remapping is to be done. */
static a_boolean
		clear_fe_pointers_during_walk;
			/* If TRUE, pointers to front end information should
			   be cleared during the IL walk. */
static a_boolean
		walking_file_scope;
			/* TRUE if walking the file-scope IL, FALSE if
			   walking the IL for a function scope. */
static a_boolean
		walking_secondary_trans_unit;
			/* TRUE if we are walking an IL tree in a secondary
			   translation unit, FALSE if we are walking the
			   IL in a primary translation unit. */
typedef char	*a_char_ptr;
			/* Useful to indicate "char *" as a type in calling
			   remap_ptr or walk_ptr. */
			/*lint -esym(751,a_char_ptr)*/

/*
Structure used to save/restore global state information for the IL walk
routines.
*/
typedef struct an_il_walk_state {
  an_entry_process_function_ptr
		entry_process_func;
  a_string_entry_process_function_ptr
		string_entry_process_func;
  a_walk_termination_test_function_ptr
		walk_termination_test_func;
  a_remap_function_ptr
		walk_remap_func;
  a_remap_function_ptr
		walk_list_remap_func;
  a_boolean	walking_file_scope;
  a_boolean	walking_secondary_trans_unit;
  int		flag_value_meaning_visited;
  a_boolean	clear_fe_pointers_during_walk;
} an_il_walk_state;


/*
Save the current state of the global variables in the IL walk routines in
the variable saved_state for later restoration.
*/
#define save_il_walk_state(saved_state)                               \
{ (saved_state).entry_process_func         = entry_process_func;      \
  (saved_state).string_entry_process_func  = string_entry_process_func; \
  (saved_state).walk_termination_test_func = walk_termination_test_func; \
  (saved_state).walk_remap_func            = walk_remap_func;         \
  (saved_state).walk_list_remap_func       = walk_list_remap_func;    \
  (saved_state).walking_file_scope         = walking_file_scope;      \
  (saved_state).walking_secondary_trans_unit = walking_secondary_trans_unit;\
  (saved_state).flag_value_meaning_visited = flag_value_meaning_visited; \
  (saved_state).clear_fe_pointers_during_walk = \
                                       clear_fe_pointers_during_walk; \
}  /* save_il_walk_state */

/*
Restore the current state of the global variables in the IL walk routines
from the saved values in the variable saved_state.
*/
#define restore_il_walk_state(saved_state)                            \
{ entry_process_func         = (saved_state).entry_process_func;      \
  string_entry_process_func  = (saved_state).string_entry_process_func; \
  walk_termination_test_func = (saved_state).walk_termination_test_func; \
  walk_remap_func            = (saved_state).walk_remap_func;         \
  walk_list_remap_func       = (saved_state).walk_list_remap_func;    \
  walking_file_scope         = (saved_state).walking_file_scope;      \
  walking_secondary_trans_unit = (saved_state).walking_secondary_trans_unit;\
  flag_value_meaning_visited = (saved_state).flag_value_meaning_visited; \
  clear_fe_pointers_during_walk = \
                         (saved_state).clear_fe_pointers_during_walk; \
}  /* restore_il_walk_state */

#if IL_WALK_NEEDED
/* Generic IL walk routines (as opposed to, say, the versions that walk
   the IL to set the "needed" flag). */

/* Declarations required because of forward references. */
static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length);

/* Build a routine to walk entries and their subtrees. */
#define DO_SUBTREE_WALK TRUE
#define NEEDED_FLAG_WALK FALSE
#define KEEP_IN_IL_WALK FALSE
#define WALK_ENTRY_ROUTINE_STATIC static
#define WALK_ENTRY_ROUTINE_NAME walk_entry_and_subtree
#define WALK_ORPHANED_ENTRY_ROUTINE_NAME walk_orphaned_file_scope_il_entries
#undef UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"


static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind,
and has length given by "entry_length" if its kind is iek_string_text).
If entry_ptr is NULL, do nothing.  This routine should be called only for
string entries.  This routine should be called by way of the macro
walk_string_ptr.  Note that the entry is processed even if it is in the
file scope and a function scope is being traversed.  This is because
string entries are considered honorary members of the scope from which
they are referenced for purposes of tree walking.
*/
{
  /* Ignore NULL pointers. */
  if (entry_ptr != NULL) {
#if DEBUG
    if (debug_level >= 5) {
      char *s;
      switch (entry_kind) {
        case iek_id_name:       s = "id name";                 break;
        case iek_string_text:   s = "string text";             break;
        case iek_other_text:    s = "other text";              break;
        default:                s = "<bad kind>";              break;
      }  /* switch */
      fprintf(f_debug, "Walking IL tree, string entry kind = %s\n", s);
    }  /* if */
#endif /* DEBUG */
    /* Call the routine to process string entries only if there is one. */
    if (string_entry_process_func != NULL) {
      /* For entries other than iek_string_text, the length must be computed.
         Note that it includes the final null byte (that's the "+ 1"). */
      if (entry_kind != iek_string_text) entry_length = strlen(entry_ptr) + 1;
      string_entry_process_func(entry_ptr, entry_kind, entry_length);
    }  /* if */
  }  /* if */
}  /* walk_string_entry */


void walk_file_scope_il(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers)
/*
Walk the intermediate language tree for the file scope.  Begin with il_header
and visit the whole file-scope tree, but do not go down into the information
about each function.  Process each non-string entry by calling
entry_process_function on that entry, and each string entry by calling
string_entry_process_function on that entry.  Remap each pointer to a new
value by calling remap_function or, for list pointers, list_remap_function.
Test for termination (not processing an entry and not continuing deeper
into the tree) by calling termination_test_function.  entry_process_function,
string_entry_process_function, remap_function, list_remap_function, or
termination_test_function can be NULL to indicate that the corresponding
function is unnecessary.  If clear_fe_pointers is TRUE, pointers to front
end data structures are cleared as the traversal is done.

The remapping function is used when reading in an IL file.  The IL tree
was in memory in some way, and was written out exactly the way it
looked.  Now it has been read back in, and each memory block is probably
at a different location than when written out.  All of the pointers
need to be updated from their "old" values to the proper "new" values.
That is what the remap function does.
*/
{
  an_il_walk_state saved_state;
  a_scope_ptr      scope;

  db_enter(4, "walk_file_scope_il");
  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  walk_termination_test_func = termination_test_function;
  walk_remap_func = remap_function;
  walk_list_remap_func = list_remap_function;
  clear_fe_pointers_during_walk = clear_fe_pointers;
  walking_file_scope = TRUE;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the IL header.  Note that all of these pointers are to the
     file scope memory region. */
  /* Remap and walk the first pointer in two steps, so that we can find
     out what the proper il_walk_flag setting is. */
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  scope = il_header.primary_scope;
  flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
  walking_secondary_trans_unit = in_secondary_trans_unit(scope);
  /* The default termination test cannot be used when walking a
     secondary translation unit. */
  check_assertion(termination_test_function != NULL ||
                  !walking_secondary_trans_unit);
  /* Walk the main body of the IL. */
  walk_entry_and_subtree((char *)scope, iek_scope);
  walk_list(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  walk_string_ptr(il_header.compiler_version, iek_other_text, 0);
  walk_string_ptr(il_header.time_of_compilation, iek_other_text, 0);
  /* region_scope_entry should not be walked. */
  /* Walk the lists of local types and static variables for functions, which
     are logically in function scopes but allocated in the file scope
     memory region. */
  walk_list(il_header.scope_orphaned_list_headers,
            a_scope_orphaned_list_header_ptr,
            iek_scope_orphaned_list_header);
  /* Walk through the orphaned IL entries referenced from 
     function scopes, but in the file scope memory region. */
  walk_orphaned_file_scope_il_entries();
#if RECORD_MACROS_IN_IL
  /* Walk the list of entries representing macros. */
  walk_list(il_header.macros, a_macro_ptr, iek_macro);
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  walk_string_ptr(il_header.instantiation_dir_name, iek_other_text, 0);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  walk_list(il_header.nontag_types_used_in_exception_or_rtti,
            a_type_ptr, iek_type);
  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
  db_exit();
}  /* walk_file_scope_il */


void walk_routine_scope_il(
            a_memory_region_number               region_number,
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers)
/*
Walk the intermediate language tree for a routine scope.  Begin with the
scope entry for region region_number, and visit the whole scope tree.
Process each non-string entry by calling entry_process_function on that
entry, and each string entry by calling string_entry_process_function on
that entry.  Remap each pointer to a new value by calling
remap_function or, for list pointers, list_remap_function.  Test for
termination (not processing an entry and not continuing deeper into
the tree) by calling termination_test_function.
entry_process_function, string_entry_process_function, remap_function,
list_remap_function, or termination_test_function can be NULL to
indicate that the corresponding function is unnecessary.  If
clear_fe_pointers is TRUE, pointers to front end data structures are
cleared as the traversal is done.  Note that if pointer remapping is
being done il_header.region_scope_entry[region_number] is assumed to
have already been remapped.
*/
{
  a_scope_ptr      scope;
  an_il_walk_state saved_state;

  db_enter(4, "walk_routine_scope_il");
  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);

  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  walk_termination_test_func = termination_test_function;
  walk_remap_func = remap_function;
  walk_list_remap_func = list_remap_function;
  clear_fe_pointers_during_walk = clear_fe_pointers;
  /* Walking a routine scope, not the file scope. */
  walking_file_scope = FALSE;
  scope = il_header.region_scope_entry[region_number];
  flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
  walking_secondary_trans_unit = in_secondary_trans_unit(scope);
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */
  /* The default termination test cannot be used when walking a
     secondary translation unit. */
  check_assertion(termination_test_function != NULL ||
                  !walking_secondary_trans_unit);

  /* Process the scope and its subtree. */
  walk_entry_and_subtree((char *)scope, iek_scope);

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
  db_exit();
}  /* walk_routine_scope_il */


void walk_il_subtree(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers,
            char                                 *ptr,
            an_il_entry_kind                     kind)
/*
Walk the subtree of the intermediate language tree headed by ptr, whose
kind is "kind".  Process each non-string entry by calling
entry_process_function on that entry, and each string entry by calling
string_entry_process_function on that entry.  Remap each pointer to a new
value by calling remap_function or, for list pointers, list_remap_function.
Test for termination (not processing an entry and not continuing
deeper into the tree) by calling termination_test_function.
entry_process_function, string_entry_process_function, remap_function,
list_remap_function, or termination_test_function can be NULL to
indicate that the corresponding function is unnecessary.  If
clear_fe_pointers is TRUE, pointers to front end data structures are
cleared as the traversal is done.  The caller must set
flag_value_meaning_visited if it will be used by the termination test,
*/
{
  an_il_walk_state saved_state;

  db_enter(4, "walk_il_subtree");
  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  walk_termination_test_func = termination_test_function;
  walk_remap_func = remap_function;
  walk_list_remap_func = list_remap_function;
  clear_fe_pointers_during_walk = clear_fe_pointers;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  remap_ptr(ptr, a_char_ptr, kind);
  walking_file_scope = in_file_scope(ptr);
  walking_secondary_trans_unit = in_secondary_trans_unit(ptr);
  /* The default termination test cannot be used when walking a
     secondary translation unit. */
  check_assertion(termination_test_function != NULL ||
                  !walking_secondary_trans_unit);
  walk_entry_and_subtree(ptr, kind);
  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
  db_exit();
}  /* walk_il_subtree */

#endif /* IL_WALK_NEEDED */
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM

#if !DO_IL_LOWERING

static a_variable_ptr find_var_in_scope_with_type(a_type_ptr  type,
                                                  a_scope_ptr scope)
/*
Look through the variables list of the indicated scope to see if there is a
variable whose type is the indicated type.  If so, return a pointer to it;
otherwise, return NULL.
*/
{
  a_variable_ptr var;

  for (var = scope->variables; var != NULL; var = var->next) {
    if (var->type == type) break;
  }  /* for */
  if (var == NULL) {
    for (var = scope->nonstatic_variables; var != NULL; var = var->next) {
      if (var->type == type) break;
    }  /* for */
  }  /* if */
  return var;
}  /* find_var_in_scope_with_type */


static a_variable_ptr find_var_in_function_scope_with_type(a_type_ptr  type,
                                                           a_scope_ptr scope)
/*
Look through the variables lists of the indicated scope (a function or
block scope) and all its subscopes to see if there is a variable whose type
is the indicated type.  If so, return a pointer to it; otherwise, return
NULL.
*/
{
  a_variable_ptr var;

  var = find_var_in_scope_with_type(type, scope);
  if (var == NULL) {
    /* Search block scopes. */
    a_scope_ptr block_scope;
    for (block_scope = scope->scopes;
         block_scope != NULL;
         block_scope = block_scope->next) {
      var = find_var_in_function_scope_with_type(type, block_scope);
      if (var != NULL) break;
    }  /* if */
  }  /* if */
  return var;
}  /* find_var_in_function_scope_with_type */


static a_variable_ptr find_parent_var_of_anon_union_type(a_type_ptr type)
/*
type is a class type that is an anonymous union with kind auk_variable.
Find the associated anonymous union variable and return a pointer to it.
This routine is not very efficient, but it's used only in very strange
cases (anonymous unions containing types).
*/
{
  a_variable_ptr var;

  if (!type->source_corresp.is_class_member &&
      type->source_corresp.parent.namespace_ptr != NULL) {
    /* The type is a member of a namespace.  Search the namespace variable
       list. */
    a_namespace_ptr nsp = type->source_corresp.parent.namespace_ptr;
    check_assertion(!nsp->is_namespace_alias);
    var = find_var_in_scope_with_type(type, nsp->variant.assoc_scope);
  } else if (type->source_corresp.is_local_to_function) {
    /* The type is local to a function.  Search the function and block
       scopes. */
    var = find_var_in_function_scope_with_type(type, innermost_function_scope);
  } else {
    /* This type must be in the file scope. */
    var = find_var_in_scope_with_type(type, il_header.primary_scope);
  }  /* if */
  check_assertion_str(var != NULL,
                      "find_parent_var_of_anon_union_type: var not found");
  return var;
}  /* find_parent_var_of_anon_union_type */

#endif /* !DO_IL_LOWERING */
    
/* "needed" flag section: */
/* Generate walk_tree_and_set_needed from the walk_entry.h source. */
#undef DO_SUBTREE_WALK
#define DO_SUBTREE_WALK TRUE
#undef NEEDED_FLAG_WALK
#define NEEDED_FLAG_WALK TRUE
#undef KEEP_IN_IL_WALK
#define KEEP_IN_IL_WALK FALSE
#undef WALK_ENTRY_ROUTINE_STATIC
#define WALK_ENTRY_ROUTINE_STATIC static
#undef WALK_ENTRY_ROUTINE_NAME
#define WALK_ENTRY_ROUTINE_NAME walk_tree_and_set_needed
#undef WALK_ORPHANED_ENTRY_ROUTINE_NAME
#undef UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"


static void set_canonical_routine_definition_needed(a_routine_ptr rout)
/*
If the indicated routine has an associated canonical entry in a secondary
translation unit, mark the canonical entry's definition as needed.
*/
{
  if (trans_unit_corresp_of(rout) != NULL) {
    a_routine_ptr canonical_rout =
                         (a_routine_ptr)trans_unit_corresp_of(rout)->canonical;
    if (canonical_rout != rout &&
        in_secondary_trans_unit(canonical_rout)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      set_routine_definition_needed(canonical_rout);
    }  /* if */
  }  /* if */      
}  /* set_canonical_routine_definition_needed */


void set_routine_definition_needed(a_routine_ptr rout)
/*
Set the definition_needed flag on the indicated routine.  This means the
definition of the routine is needed, and not just the declaration.
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object && !in_secondary_trans_unit(rout)) {
    if (!treat_as_static_inline(rout)) {
      if (needed_flag_bit_number != 0) {
        /* If we reach this spot while doing a walk for a particular
           instantiation bit number, and that's not the bit number associated
           with this function (i.e., the function definition doesn't go
           in that slice), just return.  Inline functions are included in every
           instantiation file that uses them. */
        unsigned long eff_bit_number = rout->instantiation_needed_bit_number;
        if (eff_bit_number == 0) eff_bit_number = 1;
        if (needed_flag_bit_number != eff_bit_number) goto end_of_routine;
      } else {
        /* needed_flag_bit_number is zero.  Do a recursive call to set the
           routine definition needed flag for whatever bit slice this
           routine should appear in.  This must be done now so that it
           gets done before the routine body is written out.  Note that
           inline functions do not get here. */
        needed_flag_bit_number = rout->instantiation_needed_bit_number;
        /* If the bit number is 0, the function is not an instantiation with
           an associated bit; use bit number 1 (used for everything in the
           compilation excluding the instantiations). */
        if (needed_flag_bit_number == 0) needed_flag_bit_number = 1;
        set_routine_definition_needed(rout);
        needed_flag_bit_number = 0;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (walking_secondary_trans_unit &&
      !in_secondary_trans_unit(rout)) {
    /* If a routine in the primary IL is encountered while walking the
       IL for a secondary translation unit, do not set the definition needed
       flag, because we want the primary IL flags to be set only on the final
       IL after copying.  Do set the flag on the associated canonical
       entry if there is one, however. */
    set_canonical_routine_definition_needed(rout);
  } else if (!routine_definition_needed_flag_is_set(rout)) {
  /* Set the flag if it is not set already. */
    check_assertion_str(!rout->is_trivial_default_constructor,
                        "set_routine_definition_needed: trivial default ctor");
    set_routine_definition_needed_flag(rout);
#if DEBUG
    if (db_trace("needed_flags", rout, iek_routine)) {
#if ONE_INSTANTIATION_PER_OBJECT
      if (needed_flag_bit_number != 0) {
        fprintf(f_debug, "Setting definition_needed (%lu) on rout ",
                         needed_flag_bit_number);
      } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      /* Do not insert code here. */
      {
        fprintf(f_debug, "Setting definition_needed on rout ");
      }  /* if */
      db_name_full(&rout->source_corresp, iek_routine);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* If the definition is present, walk it.  set_routine_defined and
       remark_routine_definition_needed take care of calling this again
       later when the routine is defined if it has no body now. */
    if (rout->defined && rout->assoc_scope != NULL_region_number) {
      a_scope_ptr            saved_innermost_function_scope;
      a_memory_region_number saved_curr_il_region_number=curr_il_region_number;
      a_scope_ptr scope;
      check_assertion_str(mem_region_table[rout->assoc_scope] != NULL,
                          "set_routine_definition_needed: memory region gone");
      scope = il_header.region_scope_entry[rout->assoc_scope];
      /* Don't sweep functions until the bodies have been completely processed.
         In particular, don't sweep bodies in the primary IL until they
         have been lowered.  This comes up when lowering is delayed, e.g., to
         wait until references to secondary-translation-unit IL have
         been rewritten.  finish_function_body_processing calls
         remark_routine_definition_needed later to ensure that the
         sweep gets done. */
      if (scope->function_body_processing_finished) {
        /* Set curr_il_region_number for the duration of the sweeps here.
           This is necessary in case some
           a_per_instantiation_needed_flags_entry entries need to be
           allocated; we need to know what memory region to put them in. */
        curr_il_region_number = rout->assoc_scope;
        /* Set the innermost function scope.  This is needed for finding the
           variable associated with anonymous union types. */
        saved_innermost_function_scope = innermost_function_scope;
        innermost_function_scope = scope;
        /* walk_tree_and_set_needed is not used here so that this routine can
           be callable from outside of the needed flag walk. */
        mark_as_needed((char *)scope, iek_scope);
        innermost_function_scope = saved_innermost_function_scope;
        curr_il_region_number = saved_curr_il_region_number;
#if ONE_INSTANTIATION_PER_OBJECT
        if (needed_flag_bit_number == 0)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
        /* Do not insert code here. */
        {
          /* Do the keep_definition_in_il processing now so we can free the
             memory region as soon as possible. */
          set_routine_keep_definition_in_il(rout);
          /* Decide on disposing of the memory region. */
          if (scope->depth_in_scope_stack != NO_SCOPE_DEPTH ||
              innermost_function_scope == scope) {
            /* This function's scope is still on the scope stack, so do nothing
               now.  check_for_done_with_memory_region will be called when the
               scope is popped off the stack.  The innermost_function_scope
               test is needed for generated routines in IL lowering, since
               they're not on the scope stack. */
          } else {
            /* We may be able to dispose of the memory region now. */
            check_for_done_with_memory_region(rout->assoc_scope);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    if (rout->overriding_function_for_covariant_return_type != NULL) {
      /* For a thunk, set the definition needed on the actual routine
         referenced. */
      set_routine_definition_needed(rout->
                                overriding_function_for_covariant_return_type);
    }  /* if */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    /* For a routine that has linkage, mark the associated canonical entry
       to have its definition kept too, since that's the one that will be
       copied to the primary IL. */
    set_canonical_routine_definition_needed(rout);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
end_of_routine:;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* set_routine_definition_needed */


void remark_routine_definition_needed(a_routine_ptr rout)
/*
If the indicated routine is marked as having its definition needed,
clear the flag and set it again.  Do this also for any per-instantiation
definition-needed bits that are set.  This is used to sweep the
body of the function when the routine "defined" flag gets set after some
"definition needed" flags were set.  Also handles the keep_definition_in_il
flag.
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_needed_flag_bit_number = needed_flag_bit_number;

  if (one_instantiation_per_object &&
      !in_secondary_trans_unit(rout)) {
    unsigned long bit_number;
    an_instantiation_needed_flags_scan_state
                  state;

    /* For each "definition needed" bit set in the model, set the
       corresponding bit in the entry. */
    clear_instantiation_needed_flags_scan_state(&state, &rout->source_corresp);
    while ((bit_number = next_set_instantiation_needed_flag(&state)) != 0) {
      /* Ignore bits other than the "definition needed" bits. */
      if (bit_number % 2 == 0) {
        needed_flag_bit_number = bit_number - 1;
        /* Clear the bit if set, then set it. */
        set_instantiation_needed_flag(&rout->source_corresp, 1, 0);
        set_routine_definition_needed(rout);
      }  /* if */
    }  /* while */
  }  /* if */
  needed_flag_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (rout->definition_needed) {
    rout->definition_needed = FALSE;
    rout->keep_definition_in_il = FALSE;
    set_routine_definition_needed(rout);
  } else if (rout->keep_definition_in_il) {
    /* The routine has keep_definition_in_il but not definition_needed.
       Remark the body for the keep_definition_in_il. */
    rout->keep_definition_in_il = FALSE;
    set_routine_keep_definition_in_il(rout);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  needed_flag_bit_number = saved_needed_flag_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* remark_routine_definition_needed */


static void set_canonical_class_definition_needed(a_type_ptr type)
/*
If the indicated class type has an associated canonical entry in a secondary
translation unit, mark the canonical entry's definition as needed.
*/
{
  if (trans_unit_corresp_of(type) != NULL) {
    a_type_ptr canonical_type =
                            (a_type_ptr)trans_unit_corresp_of(type)->canonical;
    if (canonical_type != type &&
        in_secondary_trans_unit(canonical_type)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      set_class_definition_needed(canonical_type);
    }  /* if */
  }  /* if */      
}  /* set_canonical_class_definition_needed */


void set_class_definition_needed(a_type_ptr type)
/*
Set the definition_needed flag on the indicated class type.  This means the
definition of the class is needed, and not just the declaration.
*/
{
  if (walking_secondary_trans_unit &&
      !in_secondary_trans_unit(type)) {
    /* If a class in the primary IL is encountered while walking the
       IL for a secondary translation unit, do not set the definition needed
       flag, because we want the primary IL flags to be set only on the final
       IL after copying.  Do set the flag on the associated canonical
       entry if there is one, however. */
    set_canonical_class_definition_needed(type);
  } else if (!class_definition_needed_flag_is_set(type)) {
    /* Set the flag if it is not set already. */
    set_class_definition_needed_flag(type);
#if DEBUG
    if (db_trace("needed_flags", type, iek_type)) {
#if ONE_INSTANTIATION_PER_OBJECT
      if (needed_flag_bit_number != 0) {
        fprintf(f_debug, "Setting definition_needed (%lu) on ",
                         needed_flag_bit_number);
      } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
      /* Do not insert code here. */
      {
        fprintf(f_debug, "Setting definition_needed on ");
      }  /* if */
      db_abbreviated_type(type);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* If the class is already marked as needed, redo the sweep for that,
       because before the definition_needed flag is set the subtree of
       the class is not swept when the class needed flag is set. */
    remark_as_needed((char *)type, iek_type);
    /* For a type that has linkage, mark the associated canonical entry
       to have its definition kept too, since that's the one that will be
       copied to the primary IL. */
    set_canonical_class_definition_needed(type);
  }  /* if */
}  /* set_class_definition_needed */


static void mark_canonical_as_needed(char             *entry_ptr,
                                     an_il_entry_kind entry_kind)
/*
If the indicated entity has an associated canonical entry in a secondary
translation unit, mark the canonical entry as needed.  The entry is one
with a source correspondence field.
*/
{
  a_source_correspondence *scp = (a_source_correspondence *)entry_ptr;

  if (scp != NULL && scp->trans_unit_corresp != NULL) {
    char *canonical = scp->trans_unit_corresp->canonical;
    if (canonical != entry_ptr &&
        in_secondary_trans_unit(canonical)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      mark_as_needed(canonical, entry_kind);
#if LOWER_EXTERN_INLINE
      /* mark_as_needed usually sets the definition-needed flag on routines,
         but it doesn't when they are extern inline routines that will be
         lowered to static.  We do need to keep the definition in such
         cases (because another non-canonical instance has been
         referenced), so mark the definition as needed explicitly. */
      if (entry_kind == iek_routine) {
        a_routine_ptr rout = (a_routine_ptr)canonical;
        if (!C_mode() && treat_as_extern_inline(rout)) {
          set_routine_definition_needed(rout);
        }  /* if */
      }  /* if */
#endif /* LOWER_EXTERN_INLINE */
    }  /* if */
  }  /* if */    
}  /* mark_canonical_as_needed */


/*
Macro that returns TRUE if a class is local to a function.  This is the same
as saying it is not subject to the end-of-file-scope sweep to set "needed"
flags.
*/
#define class_is_function_local(type) \
  ((type)->source_corresp.is_local_to_function || \
   (type)->declared_in_function_prototype)

/*
Given an IL entry at entry_ptr with kind entry_kind, return TRUE if the
entry's subtree should be walked at this time.  This is used when setting the
"needed" or "keep_in_il" flags.  Entities that can be defined, redeclared or
otherwise changed (lowered, hidden name entries added) at a later stage
shouldn't have their subtrees walked until after there is no longer the
possibility of the subtree changing.  end_of_file_scope_needed_flags_phase is
set to TRUE in a phase where subtrees should finally be walked (see
set_needed_flags_at_end_of_file_scope).  The subtrees of local classes
and variables are walked once their okay_to_walk_subtree_of_local_entity
flags are set, which happens at the end of processing of the containing
function.  Subtrees of classes in prototype scopes (possible only in
C) are always walked immediately.  is_class is TRUE if the entity is a class.
*/
#define should_walk_subtree(entry_ptr, entry_kind, is_class) \
  (end_of_file_scope_needed_flags_phase || \
   ((!(is_class) || \
     ((a_type_ptr)(entry_ptr))->declared_in_function_prototype) && \
    (entry_kind) != iek_variable && \
    (entry_kind) != iek_routine) || \
   ((a_source_correspondence *)(entry_ptr))-> \
                                      okay_to_walk_subtree_of_local_entity)


#if DO_IL_LOWERING
/*
Macro that returns TRUE if the class parent information in an entry (of kind
entry_kind) will remain after IL lowering is done.  Note that lowering
is never done in secondary translation units.
*/
#define parent_will_exist_after_lowering(entry_ptr, entry_kind) \
  (suppress_il_lowering || in_secondary_trans_unit(entry_ptr) || \
   (entry_kind) == iek_field)
#endif /* DO_IL_LOWERING */


static a_boolean prune_needed_flag_il_walk(char             *entry_ptr,
                                           an_il_entry_kind entry_kind)
/*
Termination-test routine for the IL walk used to set the "needed" flag in
a tree of IL entries.  Returns TRUE to indicate that the IL walk should be
pruned at the given entry, because the entry has already been marked
as needed.
*/
{
  a_boolean               prune = FALSE;
  a_source_correspondence *scp;

  /* Note that this routine is very similar to prune_keep_in_il_walk. */
  /* Only certain entry kinds have a "needed" flag.  See if this one does. */
  scp = source_corresp_for_il_entry(entry_ptr, entry_kind);
  if (scp != NULL) {
    /* The entry does have a "needed" flag. */
    if (walking_secondary_trans_unit &&
        !in_secondary_trans_unit(entry_ptr)) {
      /* If an entry in the primary IL is encountered while walking the
         IL for a secondary translation unit, do not set the needed flag,
         because we want the primary IL flags to be set only on the final
         IL after copying.  Do set the flag on the associated canonical
         entry if there is one, however. */
      prune = TRUE;
      mark_canonical_as_needed(entry_ptr, entry_kind);
    } else if (needed_flag_is_set(scp)) {
      /* The flag is set already, so prune the walk at this entry.  */
      prune = TRUE;
    } else {
      /* The flag is not set, so set it and keep walking. */
      set_needed_flag(scp);
#if DEBUG
      if (db_trace("needed_flags", entry_ptr, entry_kind)) {
        if (entry_kind == iek_type ||
            entry_kind == iek_variable ||
            entry_kind == iek_routine ||
            entry_kind == iek_namespace) {
#if ONE_INSTANTIATION_PER_OBJECT
          if (needed_flag_bit_number != 0) {
            fprintf(f_debug, "Setting needed (%lu) on ",
                             needed_flag_bit_number);
          } else
#endif /* ONE_INSTANTIATION_PER_OBJECT */
          /* Do not insert code here. */
          {
            fprintf(f_debug, "Setting needed on ");
          }  /* if */
          if (entry_kind == iek_type) {
            fprintf(f_debug, "type ");
            db_abbreviated_type((a_type_ptr)entry_ptr);
          } else if (entry_kind == iek_variable) {
            fprintf(f_debug, "var  ");
            db_name_full(&((a_variable_ptr)entry_ptr)->source_corresp,
                         iek_variable);
          } else if (entry_kind == iek_routine) {
            fprintf(f_debug, "rout ");
            db_name_full(&((a_routine_ptr)entry_ptr)->source_corresp,
                         iek_routine);
          } else if (entry_kind == iek_namespace) {
            fprintf(f_debug, "namespace ");
            db_name_full(&((a_routine_ptr)entry_ptr)->source_corresp,
                         iek_namespace);
          }  /* if */
          fprintf(f_debug, "\n");
        }  /* if */
      }  /* if */
#endif /* DEBUG */
      /* Determine whether the subtree of this entry should be walked. */
      prune = !should_walk_subtree(
                            entry_ptr, entry_kind,
                            (entry_kind == iek_type &&
                             is_immediate_class_type((a_type_ptr)entry_ptr)));
      if (prune) {
        if (scp->is_class_member
#if DO_IL_LOWERING
            /* Do not process parent information that will be removed by
               IL lowering. */
            && parent_will_exist_after_lowering(entry_ptr, entry_kind)
#endif /* DO_IL_LOWERING */
                                                           ) {
          /* When the subtree is not going to be walked now and the entity is
             a class member, mark the parent as needed anyway.  This is done
             in the normal processing, but we're suppressing that by not
             walking the subtree. */
          a_type_ptr parent_class = scp->parent.class_type;
          walk_tree_and_set_needed((char *)parent_class, iek_type);
          set_class_definition_needed(parent_class);
        }  /* if */
      }  /* if */
      /* For an entity that has linkage, mark the associated canonical entry
         as needed too, since that's the one that will be copied to the
         primary IL. */
      mark_canonical_as_needed(entry_ptr, entry_kind);
    }  /* if */
  }  /* if */
  return prune;
}  /* prune_needed_flag_il_walk */

#if ONE_INSTANTIATION_PER_OBJECT

void set_per_instantiation_needed_flag(char             *entry_ptr,
                                       an_il_entry_kind entry_kind,
                                       unsigned long    bit_number)
/*
Set the per-instantiation "needed" bit numbered bit_number to indicate
everything referenced from the indicated externally-defined entity
(a variable or routine).  If bit_number is 0, the entity is not an
instantiation with an associated bit; use bit number 1 (used for everything
in the compilation excluding the instantiations).
*/
{
  unsigned long save_needed_flag_bit_number = needed_flag_bit_number;

  if (bit_number == 0) bit_number = 1;
  needed_flag_bit_number = bit_number;
  mark_as_needed(entry_ptr, entry_kind);
  needed_flag_bit_number = save_needed_flag_bit_number;
}  /* set_per_instantiation_needed_flag */

#endif /* ONE_INSTANTIATION_PER_OBJECT */

static void mark_as_needed_basic(char             *entry_ptr,
                                 an_il_entry_kind entry_kind)
/*
Set the "needed" flag in the indicated entity, and also on everything it
references.
*/
{
  an_il_walk_state saved_state;

  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Set up for this walk. */
  entry_process_func = NULL;
  string_entry_process_func = NULL;
  walk_termination_test_func = prune_needed_flag_il_walk;
  walk_remap_func = NULL;
  walk_list_remap_func = NULL;
  clear_fe_pointers_during_walk = FALSE;
  /* walking_file_scope need not be set. */
  walking_secondary_trans_unit = in_secondary_trans_unit(entry_ptr);

  /* Walk the IL tree. */
  walk_tree_and_set_needed(entry_ptr, entry_kind);

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);

  if (okay_to_eliminate_unneeded_il_entries) {
    /* Make sure the keep_in_il flag is set also.  This is necessary for
       externally-linked static data members and member functions. */
    mark_to_keep_in_il(entry_ptr, entry_kind);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object && needed_flag_bit_number == 0 &&
      !in_secondary_trans_unit(entry_ptr)) {
    /* Determine a separate set of "needed" flags for each instantiation,
       so each can be put out in a separate object file.  If this is
       an externally-defined routine or variable, do the processing. */
    if (entry_kind == (an_il_entry_kind)iek_routine) {
      a_routine_ptr rout = (a_routine_ptr)entry_ptr;
      if (rout->storage_class == (a_storage_class)sc_unspecified &&
          !treat_as_static_inline(rout)) {
        set_per_instantiation_needed_flag(entry_ptr, entry_kind,
                                        rout->instantiation_needed_bit_number);
      }  /* if */
    } else if (entry_kind == (an_il_entry_kind)iek_variable) {
      a_variable_ptr var = (a_variable_ptr)entry_ptr;
      if (var->storage_class == (a_storage_class)sc_unspecified) {
        set_per_instantiation_needed_flag(entry_ptr, entry_kind,
                                         var->instantiation_needed_bit_number);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* mark_as_needed_basic */


void mark_as_needed(char             *entry_ptr,
                    an_il_entry_kind entry_kind)
/*
Set the "needed" flag in the indicated entity, and also on everything it
references.  If the entity is a routine, also set its definition_needed
flag.
*/
{
  mark_as_needed_basic(entry_ptr, entry_kind);
  if (entry_kind == (an_il_entry_kind)iek_routine
#if ONE_INSTANTIATION_PER_OBJECT
      && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                                 ) {
    a_routine_ptr rout = (a_routine_ptr)entry_ptr;

    check_assertion_str(!rout->is_trivial_default_constructor,
                        "mark_as_needed: trivial default ctor");
    /* For an externally-linked non-inline function, mark the body as needed
       too, on the presumption that it will be referenced from other
       translation units.  The caller could reasonably be expected to do
       this, but doing it here reduces the possibility of error.  (In C++
       mode, extern inline functions may be lowered to static inline
       functions, in which case the definition may not be needed.) */
    if (rout->storage_class == (a_storage_class)sc_unspecified &&
        (C_mode() || !treat_as_static_inline(rout))) {
      set_routine_definition_needed(rout);
#if GNU_EXTENSIONS_ALLOWED
    } else if (rout->is_initialization_routine ||
               rout->is_finalization_routine ||
               rout->has_gnu_unused_attribute) {
      /* The routine definition for an initialization or finalization
         function is always needed since the function will be called
         at program start up.  Also, a routine marked as "unused" is
         assumed to be needed (perhaps from a debugger). */
      set_routine_definition_needed(rout);
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
}  /* mark_as_needed */


void mark_as_needed_like(char                    *entry_ptr,
                         an_il_entry_kind        entry_kind,
                         a_source_correspondence *model_scp,
                         a_boolean               set_class_defn_needed)
/*
Set the needed flag(s) of the entry pointed to by entry_ptr (of kind
entry_kind) to match the needed flag(s) of model_scp (which might be
from the same entry).  The needed flag(s) of the entry are cleared
before being set.  If set_class_defn_needed is TRUE, the
entry is for a class, and its definition needed flags(s) are set to
match the needed flags(s).
*/
{
#if ONE_INSTANTIATION_PER_OBJECT
  unsigned long saved_needed_flag_bit_number = needed_flag_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  a_source_correspondence *scp = (a_source_correspondence *)entry_ptr;

  if (scp != model_scp) {
    /* The model entry is not the same as the entry to set. */
    scp = source_corresp_for_il_entry(entry_ptr, entry_kind);
    check_assertion(scp != NULL);
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  if (one_instantiation_per_object &&
      !in_secondary_trans_unit(scp) &&
      !in_secondary_trans_unit(model_scp)) {
    /* For each "needed" bit set in the model, set the corresponding bit in the
       entry. */
    an_instantiation_needed_flags_scan_state state;

    clear_instantiation_needed_flags_scan_state(&state, model_scp);
    while ((needed_flag_bit_number =
                            next_set_instantiation_needed_flag(&state)) != 0) {
      /* Ignore "definition needed" bits. */
      if (needed_flag_bit_number % 2 != 0) {
        /* Clear the bit if set, then set it. */
        set_instantiation_needed_flag(scp, 0, 0);
        mark_as_needed_basic(entry_ptr, entry_kind);
        if (set_class_defn_needed) {
          set_class_definition_needed((a_type_ptr)entry_ptr);
        }  /* if */
      }  /* if */
    }  /* while */
  }  /* if */
  needed_flag_bit_number = 0;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  if (model_scp->needed) {
    ((a_source_correspondence *)entry_ptr)->needed = FALSE;
    mark_as_needed_basic(entry_ptr, entry_kind);
    if (set_class_defn_needed) {
      set_class_definition_needed((a_type_ptr)entry_ptr);
    }  /* if */
  }  /* if */
#if ONE_INSTANTIATION_PER_OBJECT
  needed_flag_bit_number = saved_needed_flag_bit_number;
#endif /* ONE_INSTANTIATION_PER_OBJECT */
}  /* mark_as_needed_like */


void remark_as_needed(char             *entry_ptr,
                      an_il_entry_kind entry_kind)
/*
If the "needed" flag in the indicated entity is already set, clear it and
set it again.  This is used when the subtree of the entity may have changed,
to make sure the entities in the subtree are marked as needed.
*/
{
  a_source_correspondence *scp =
                            source_corresp_for_il_entry(entry_ptr, entry_kind);

  check_assertion(scp != NULL);
  mark_as_needed_like(entry_ptr, entry_kind, scp,
                      /*set_class_defn_needed=*/FALSE);
}  /* remark_as_needed */


/* "keep_in_il" flag section: */

static void clear_keep_in_il_to_allow_subtree_walk(
                                                  char             *entry_ptr,
                                                  an_il_entry_kind entry_kind);
static void set_keep_in_il_on_befriending_classes(
                                   a_class_list_entry_ptr befriending_classes);

#if GENERATE_SOURCE_SEQUENCE_LISTS
static void set_keep_in_il_on_source_sequence_entries(a_scope_ptr scope);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

/* Generate walk_tree_and_set_keep_in_il from the walk_entry.h source. */
#undef DO_SUBTREE_WALK
#define DO_SUBTREE_WALK TRUE
#undef NEEDED_FLAG_WALK
#define NEEDED_FLAG_WALK FALSE
#undef KEEP_IN_IL_WALK
#define KEEP_IN_IL_WALK TRUE
#undef WALK_ENTRY_ROUTINE_STATIC
#define WALK_ENTRY_ROUTINE_STATIC static
#undef WALK_ENTRY_ROUTINE_NAME
#define WALK_ENTRY_ROUTINE_NAME walk_tree_and_set_keep_in_il
#undef WALK_ORPHANED_ENTRY_ROUTINE_NAME
#define WALK_ORPHANED_ENTRY_ROUTINE_NAME walk_orphaned_entries_set_keep_in_il
#undef UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"


static void clear_keep_in_il_to_allow_subtree_walk(char             *entry_ptr,
                                                   an_il_entry_kind entry_kind)
/*
As part of the keep_in_il walk, clear the keep_in_il flag on the indicated
entry in preparation for setting it again.  This is done to ensure that
the subtree is walked again if it has changed.
*/
{
  il_entry_prefix_of(entry_ptr).keep_in_il = FALSE;
  if (entry_kind == iek_type) {
    a_type_ptr type = (a_type_ptr)entry_ptr;
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.keep_definition_in_il) {
      /* For a class, also clear the keep_in_il flag in the associated
         class type supplement and scope. */
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      if (ctsp != NULL) {
        a_scope_ptr scope;
        il_entry_prefix_of(ctsp).keep_in_il = FALSE;
        scope = ctsp->assoc_scope;
        if (scope != NULL) {
          il_entry_prefix_of(scope).keep_in_il = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (entry_kind == iek_namespace) {
    /* For a namespace, also clear the keep_in_il flag on the associated
       scope. */
    a_namespace_ptr nsp = (a_namespace_ptr)entry_ptr;
    if (!nsp->is_namespace_alias) {
      a_scope_ptr scope = nsp->variant.assoc_scope;
      il_entry_prefix_of(scope).keep_in_il = FALSE;
    }  /* if */
  }  /* if */
}  /* clear_keep_in_il_to_allow_subtree_walk */


static void remark_to_keep_in_il(char             *entry_ptr,
                                 an_il_entry_kind entry_kind)
/*
If the keep_in_il flag in the indicated entry is already set, clear it
and set it again.  This is used when the subtree of the entity may have
changed, to make sure the entities in the subtree are marked to be kept.
*/
{
  if (il_entry_prefix_of(entry_ptr).keep_in_il) {
    clear_keep_in_il_to_allow_subtree_walk(entry_ptr, entry_kind);
    mark_to_keep_in_il(entry_ptr, entry_kind);
  }  /* if */
}  /* remark_to_keep_in_il */


static void r_keep_definitions_of_virtual_functions_in_scope(a_scope_ptr scope)
/*
Recursive helper routine for keep_definitions_of_virtual_functions_in_scope.
See the header comment of that routine for details.
*/
{
  a_type_ptr      type;
  a_namespace_ptr nsp;
  a_scope_ptr     block_scope;

  /* Visit all types to find all class types. */
  for (type = scope->types; type != NULL; type = type->next) {
    /* Note that class types are processed only if their keep_in_il flag is
       TRUE.  Since all members will have the same keep_in_il setting as the
       class, there's no point in looking further if the class has keep_in_il
       FALSE. */
    if (is_immediate_class_type(type) && il_entry_prefix_of(type).keep_in_il) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      if (ctsp != NULL && ctsp->assoc_scope != NULL) {
        r_keep_definitions_of_virtual_functions_in_scope(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      r_keep_definitions_of_virtual_functions_in_scope(
                                                     nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    r_keep_definitions_of_virtual_functions_in_scope(block_scope);
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_class_struct_union) {
    /* A class scope.  Check for virtual functions. */
    a_routine_ptr rout;
    for (rout = scope->routines; rout != NULL; rout = rout->next) {
      if (rout->is_virtual) {
        /* Force the definition of a virtual function to be kept. */
        set_routine_keep_definition_in_il(rout);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* r_keep_definitions_of_virtual_functions_in_scope */


static void keep_definitions_of_virtual_functions_in_scope(a_scope_ptr scope)
/*
Walk through the indicated scope and its subscopes, and set keep_in_il on
the bodies of any virtual member functions that are marked as keep_in_il.
This is called at a point where all setting of the "needed" flag has been
done already.  That's important, because when the keep_in_il flag is
set on the body of a function, the memory can be freed, and we don't
want that to happen before the "needed" flag is set.
*/
{
  a_boolean there_might_be_virtual_functions = TRUE;

  if (C_mode()) {
    /* There are no virtual functions in C, so there's no point in looking
       for them. */
    there_might_be_virtual_functions = FALSE;
#if DO_IL_LOWERING
  } else {
    /* If IL lowering is done, there will be no virtual functions, so there's
       no point in looking for them. */
    if (!suppress_il_lowering &&
        /* Lowering is not done in secondary translation units. */
        !in_secondary_trans_unit(scope)) {
      there_might_be_virtual_functions = FALSE;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* if */
  if (there_might_be_virtual_functions) {
    r_keep_definitions_of_virtual_functions_in_scope(scope);
  }  /* if */
}  /* keep_definitions_of_virtual_functions_in_scope */


static void set_canonical_routine_keep_definition_in_il(a_routine_ptr rout)
/*
If the indicated routine has an associated canonical entry in a secondary
translation unit, mark the canonical entry's definition to be kept in the
IL.
*/
{
  if (trans_unit_corresp_of(rout) != NULL) {
    a_routine_ptr canonical_rout =
                         (a_routine_ptr)trans_unit_corresp_of(rout)->canonical;
    if (canonical_rout != rout &&
        in_secondary_trans_unit(canonical_rout)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      set_routine_keep_definition_in_il(canonical_rout);
    }  /* if */
  }  /* if */      
}  /* set_canonical_routine_keep_definition_in_il */


void set_routine_keep_definition_in_il(a_routine_ptr rout)
/*
Set the keep_definition_in_il flag on the indicated routine.  This means
the definition of the routine must be kept in the IL, and not just the
declaration.
*/
{
  if (walking_secondary_trans_unit &&
      !in_secondary_trans_unit(rout)) {
    /* If a routine in the primary IL is encountered while walking the
       IL for a secondary translation unit, do not set the definition
       keep_in_il flag, because we want the primary IL flags to be set
       only on the final IL after copying.  Do set the flag on the
       associated canonical entry if there is one, however. */
    set_canonical_routine_keep_definition_in_il(rout);
  } else if (!rout->keep_definition_in_il) {
    /* Set the flag if it is not set already. */
    rout->keep_definition_in_il = TRUE;
#if DEBUG
    if (db_trace("needed_flags", rout, iek_routine)) {
      fprintf(f_debug, "Setting keep_definition_in_il on rout ");
      db_name_full(&rout->source_corresp, iek_routine);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* If the definition is present, walk it.  set_routine_defined takes
       care of calling this again later when the routine is defined
       if it is not defined now. */
    if (rout->defined && rout->assoc_scope != NULL_region_number) {
      a_scope_ptr scope;
      check_assertion_str(mem_region_table[rout->assoc_scope] != NULL,
                      "set_routine_keep_definition_in_il: memory region gone");
      scope = il_header.region_scope_entry[rout->assoc_scope];
      /* Don't walk the body if it hasn't been lowered yet. */
      if (scope->function_body_processing_finished) {
        /* Set the innermost function scope.  This is needed for finding the
           variable associated with anonymous union types. */
        a_scope_ptr saved_innermost_function_scope = innermost_function_scope;
        innermost_function_scope = scope;
        /* walk_tree_and_set_keep_in_il is not used here so that this
           routine can be callable from outside of the keep_in_il flag walk. */
        mark_to_keep_in_il((char *)scope, iek_scope);
        /* Make sure the definitions of virtual functions of local classes
           are kept. */
        keep_definitions_of_virtual_functions_in_scope(scope);
        innermost_function_scope = saved_innermost_function_scope;
      }  /* if */
    }  /* if */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    if (rout->overriding_function_for_covariant_return_type != NULL) {
      /* For a thunk, set the definition needed on the actual routine
         referenced. */
      set_routine_keep_definition_in_il(rout->
                                overriding_function_for_covariant_return_type);
    }  /* if */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    /* For a routine that has linkage, mark the associated canonical entry
       to have its definition kept too, since that's the one that will be
       copied to the primary IL. */
    set_canonical_routine_keep_definition_in_il(rout);
  }  /* if */
}  /* set_routine_keep_definition_in_il */


static void set_canonical_class_keep_definition_in_il(a_type_ptr type)
/*
If the indicated class type has an associated canonical entry in a secondary
translation unit, mark the canonical entry's definition to be kept in
the IL.
*/
{
  if (trans_unit_corresp_of(type) != NULL) {
    a_type_ptr canonical_type =
                            (a_type_ptr)trans_unit_corresp_of(type)->canonical;
    if (canonical_type != type &&
        in_secondary_trans_unit(canonical_type)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      set_class_keep_definition_in_il(canonical_type);
    }  /* if */
  }  /* if */      
}  /* set_canonical_class_keep_definition_in_il */


void set_class_keep_definition_in_il(a_type_ptr type)
/*
Set the keep_definition_in_il flag on the indicated class type.  This means
the definition of the class must be kept in the IL, and not just the
declaration.
*/
{
  if (walking_secondary_trans_unit &&
      !in_secondary_trans_unit(type)) {
    /* If a class in the primary IL is encountered while walking the
       IL for a secondary translation unit, do not set the definition
       keep-in-il flag, because we want the primary IL flags to be set
       only on the final IL after copying.  Do set the flag on the associated
       canonical entry if there is one, however. */
    set_canonical_class_keep_definition_in_il(type);
  } else if (!type->variant.class_struct_union.keep_definition_in_il) {
    /* Set the flag if it is not set already. */
    type->variant.class_struct_union.keep_definition_in_il = TRUE;
#if DEBUG
    if (db_trace("needed_flags", type, iek_type)) {
      fprintf(f_debug, "Setting keep_definition_in_il on ");
      db_abbreviated_type(type);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    /* If the class is already marked to be kept in the IL, redo the sweep
       for that, because before the keep_definition_in_il flag is set the
       subtree of the class is not swept when the class keep_in_il flag
       is set. */
    remark_to_keep_in_il((char *)type, iek_type);
    /* For a type that has linkage, mark the associated canonical entry
       to have its definition kept too, since that's the one that will
       be copied to the primary IL. */
    set_canonical_class_keep_definition_in_il(type);
  }  /* if */
}  /* set_class_keep_definition_in_il */


static void set_keep_in_il_on_befriending_classes(
                                    a_class_list_entry_ptr befriending_classes)
/*
Handle a befriending classes list (from a routine or class) during the
keep_in_il walk.
*/
{
  a_class_list_entry_ptr clep;
  a_type_ptr             befriending_class;

  for (clep = befriending_classes; clep != NULL; clep = clep->next) {
    befriending_class = clep->class_type;
    if (!class_definition_needed_flag_is_set(befriending_class) &&
        !befriending_class->variant.class_struct_union.keep_definition_in_il &&
        !class_is_function_local(befriending_class)
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* If source sequence entries are being used, it's too hard to
           promote the friend from inside the class if the class is not
           needed.  Therefore we always make the containing class needed. */
        && befriending_class->source_corresp.source_sequence_entry == NULL
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
                                                                          ) {
      /* The class will be removed or its definition will be removed,
         so the friendship will be eliminated. */
    } else {
      /* Record the friendship dependency.  Note that we do this for all
         friendship, even the case where a function declaration in an
         otherwise unneeded class grants friendship to a needed function.
         The danger in removing such things, when using the C++-generating
         back end, is that the friend declaration might be the one that
         first injects the function name and makes it visible, so the
         function declaration cannot be eliminated, and in the worst case
         it's very hard to promote the function declaration out of the
         enclosing class and get the injection scope the same, so we
         force the keeping of the class as well. */
      walk_ptr(befriending_class, a_type_ptr, iek_type);
      /* A class has to be complete to befriend something else. */
      set_class_keep_definition_in_il(befriending_class);
    }  /* if */
  }  /* for */
}  /* set_keep_in_il_on_befriending_classes */


static void mark_canonical_to_keep_in_il(char             *entry_ptr,
                                         an_il_entry_kind entry_kind)
/*
If the indicated entity has an associated canonical entry in a secondary
translation unit, mark the canonical entry to be kept in the IL.
*/
{
  a_source_correspondence *scp =
                            source_corresp_for_il_entry(entry_ptr, entry_kind);

  if (scp != NULL && scp->trans_unit_corresp != NULL) {
    char *canonical = scp->trans_unit_corresp->canonical;
    if (canonical != entry_ptr &&
        in_secondary_trans_unit(canonical)
#if ONE_INSTANTIATION_PER_OBJECT
        && needed_flag_bit_number == 0
#endif /* ONE_INSTANTIATION_PER_OBJECT */
                                      ) {
      mark_to_keep_in_il(canonical, entry_kind);
    }  /* if */
  }  /* if */    
}  /* mark_canonical_to_keep_in_il */


static a_boolean prune_keep_in_il_walk(char             *entry_ptr,
                                       an_il_entry_kind entry_kind)
/*
Termination-test routine for the IL walk used to set the "keep_in_il" flag in
a tree of IL entries.  Returns TRUE to indicate that the IL walk should be
pruned at the given entry, because the entry has already been marked
to be kept.
*/
{
  a_boolean prune = FALSE, is_class = FALSE, is_function_local_class = FALSE;

  /* Note that this routine is very similar to prune_needed_flag_il_walk. */
  if (walking_secondary_trans_unit &&
      !in_secondary_trans_unit(entry_ptr)) {
    /* If an entry in the primary IL is encountered while walking the
       IL for a secondary translation unit, do not set the keep-in-IL flag,
       because we want the primary IL flags to be set only on the final
       IL after copying.  Do set the flag on the associated canonical
       entry if there is one, however. */
    prune = TRUE;
    mark_canonical_to_keep_in_il(entry_ptr, entry_kind);
  } else if (il_entry_prefix_of(entry_ptr).keep_in_il) {
    /* The flag is set already, so prune the walk at this entry.  */
    prune = TRUE;
  } else {
    /* The flag is not set, so set it and keep walking. */
    is_class = (entry_kind == iek_type &&
                is_immediate_class_type((a_type_ptr)entry_ptr));
    if (is_class) {
      a_type_ptr class_type = (a_type_ptr)entry_ptr;
      /* For a function-local class, the definition will never be removed,
         so mark it to be kept as soon as the keep_in_il flag is set on
         the class.  This is done before setting keep_in_il to avoid
         the rewalk of the subtree. */
      is_function_local_class = class_is_function_local(class_type);
      /* Ditto for an unnamed class, because you can't remove the definition
         of an unnamed class. */
      if (is_function_local_class ||
          (!has_name(class_type) ||
           class_type->variant.class_struct_union.originally_unnamed)) {
        set_class_keep_definition_in_il(class_type);
      }  /* if */
    }  /* if */
    /* Set the flag. */
    il_entry_prefix_of(entry_ptr).keep_in_il = TRUE;
#if DEBUG
    if (db_trace("needed_flags", entry_ptr, entry_kind)) {
      if (entry_kind == iek_type) {
        fprintf(f_debug, "Setting keep_in_il on type ");
        db_abbreviated_type((a_type_ptr)entry_ptr);
        fprintf(f_debug, "\n");
      } else if (entry_kind == iek_variable) {
        fprintf(f_debug, "Setting keep_in_il on var  ");
        db_name_full(&((a_variable_ptr)entry_ptr)->source_corresp,
                     iek_variable);
        fprintf(f_debug, "\n");
      } else if (entry_kind == iek_routine) {
        fprintf(f_debug, "Setting keep_in_il on rout ");
        db_name_full(&((a_routine_ptr)entry_ptr)->source_corresp,
                     iek_routine);
        fprintf(f_debug, "\n");
      } else if (entry_kind == iek_namespace) {
        fprintf(f_debug, "Setting keep_in_il on namespace ");
        db_name_full(&((a_routine_ptr)entry_ptr)->source_corresp,
                     iek_namespace);
        fprintf(f_debug, "\n");
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    /* If this is an entry that might be redeclared or redefined later,
       do not walk its subtree now. */
    prune = !should_walk_subtree(entry_ptr, entry_kind, is_class);
    if (prune) {
      /* When the subtree is not going to be walked now and the entity is
         a class member, mark the parent as needed anyway.  This is done
         in the normal processing, but we're suppressing that by not walking
         the subtree.  This is needed in particular to make sure the
         definition of a class is kept if one of its member functions
         is kept. */
#if DO_IL_LOWERING
        /* Do not process parent information that will be removed by
           IL lowering. */
      if (parent_will_exist_after_lowering(entry_ptr, entry_kind))
#endif /* DO_IL_LOWERING */
      /* Do not insert code here. */
      { a_source_correspondence *scp =
                            source_corresp_for_il_entry(entry_ptr, entry_kind);
        check_assertion(scp != NULL);
        if (scp->is_class_member) {
          a_type_ptr parent_class = scp->parent.class_type;
          walk_tree_and_set_keep_in_il((char *)parent_class, iek_type);
          set_class_keep_definition_in_il(parent_class);
        }  /* if */
      }  /* if */
    }  /* if */
    /* For an entity that has linkage, mark the associated canonical entry
       to be kept in the IL too, since that's the one that will be copied
       to the primary IL. */
    mark_canonical_to_keep_in_il(entry_ptr, entry_kind);
  }  /* if */
  return prune;
}  /* prune_keep_in_il_walk */


void mark_to_keep_in_il(char             *entry_ptr,
                        an_il_entry_kind entry_kind)
/*
Set the "keep_in_il" flag in the indicated entity, and also on everything it
references.  When this is called for the file-scope scope entry, the lists
of variables, routines, etc. of that scope are walked in a special way:
only the entries marked as "needed" are marked to keep in the IL.
*/
{
  an_il_walk_state saved_state;
  a_boolean        file_scope_walk = FALSE;

  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Set up for this walk. */
  entry_process_func = NULL;
  string_entry_process_func = NULL;
  walk_termination_test_func = prune_keep_in_il_walk;
  walk_remap_func = NULL;
  walk_list_remap_func = NULL;
  clear_fe_pointers_during_walk = FALSE;
  /* walking_file_scope need not be set. */
  walking_secondary_trans_unit = in_secondary_trans_unit(entry_ptr);
  if (entry_kind == iek_scope &&
      ((a_scope_ptr)entry_ptr)->kind == (a_scope_kind)sck_file) {
    file_scope_walk = TRUE;
    il_entry_prefix_of(entry_ptr).keep_in_il = FALSE;
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "Beginning file scope keep_in_il walk\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */

  /* Walk the IL tree. */
  walk_tree_and_set_keep_in_il(entry_ptr, entry_kind);

  if (file_scope_walk) {
    a_scope_ptr                      scope = (a_scope_ptr)entry_ptr;
    a_scope_orphaned_list_header_ptr solhp;
    /* The file scope is being walked. */
    /* Keep the definitions of virtual member functions. */
    keep_definitions_of_virtual_functions_in_scope(scope);
#if RECORD_MACROS_IN_IL
    /* Mark all macros to be kept. */
    walk_list(il_header.macros, a_macro_ptr, iek_macro);
#endif /* RECORD_MACROS_IN_IL */
    /* Walk the orphaned list for scopes, marking only those entries that
       correspond to routines whose bodies are being kept in the IL.
       The process that eliminates unneeded IL entries looks at the
       lists for unneeded routines later, and will try to remove
       entries from those lists.  If each list is entirely removed,
       the header itself is removed, in which case the reference from
       the header to the associated routine should not cause the
       setting of keep_in_il on the routine entry itself.
       So we don't do that here, leaving it to be done later if
       appropriate. */
    check_assertion(end_of_file_scope_needed_flags_phase);
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      a_routine_ptr rout = solhp->assoc_routine;
      if (rout->keep_definition_in_il) {
        walk_ptr(solhp, a_scope_orphaned_list_header_ptr,
                 iek_scope_orphaned_list_header);
      }  /* if */
    }  /* for */
    /* Visit the orphan lists. */
    walk_orphaned_entries_set_keep_in_il();
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Set keep_in_il on source correspondence entries to match the
       IL entries pointed to.  This must be done late so that all the
       keep_in_il flags have been set. */
    set_keep_in_il_on_source_sequence_entries(scope);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DEBUG
    if (db_flag_is_set("needed_flags")) {
      fprintf(f_debug, "Ending file scope keep_in_il walk\n");
    }  /* if */
#endif /* DEBUG */
  }  /* if */

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
}  /* mark_to_keep_in_il */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void r_set_keep_in_il_on_sslist(
                                a_source_sequence_entry_ptr sslist,
                                a_boolean                   function_local,
                                a_boolean                   *need_another_pass)
/*
Walk the indicated source sequence list, and set the keep_in_il
flags in the source sequence entries thereon.  A source sequence entry
must be kept in the IL if and only if its associated entry must be
kept.  If function_local is TRUE, this list is local to a function that
is being kept, and all entries should be marked to be kept.  If this
routine modifies some entry that might be earlier on the list, set
*need_another_pass to TRUE so the caller can do the sweep again.
*/
{
  a_source_sequence_entry_ptr ssep;

  for (ssep = sslist; ssep != NULL; ssep = ssep->next) {
    if (ss_entry_kind(ssep) == iek_src_seq_sublist) {
      /* Where a file-scope sublist appears, do a recursive call to walk the
         entries on that list. */
      a_src_seq_sublist_ptr ssslp = ss_entry_ptr(ssep, a_src_seq_sublist_ptr);
      r_set_keep_in_il_on_sslist(ssslp->source_sequence_list,
                                 function_local,
                                 need_another_pass);
    } else {
      /* Not a sublist (normal source sequence entry). */
      char                           *entry_ptr;
      an_il_entry_kind               entry_kind;
      a_src_seq_secondary_decl_ptr   sec_decl = NULL;
      a_src_seq_end_of_construct_ptr ecp = NULL;
      an_instantiation_directive_ptr idp = NULL;
      a_boolean                      keep_in_il;

      if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
        /* This is a secondary declaration. */
        sec_decl = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        entry_ptr = sec_decl->entity.ptr;
        entry_kind = (an_il_entry_kind)sec_decl->entity.kind;
      } else if (ss_entry_kind(ssep) == iek_src_seq_end_of_construct) {
        /* This is an end-of-construct entry. */
        ecp = ss_entry_ptr(ssep, a_src_seq_end_of_construct_ptr);
        entry_ptr = ecp->entity.ptr;
        entry_kind = (an_il_entry_kind)ecp->entity.kind;
      } else if (ss_entry_kind(ssep) == iek_instantiation_directive) {
        /* This is an instantiation directive. */
        idp = ss_entry_ptr(ssep, an_instantiation_directive_ptr);
        entry_ptr = idp->entity.ptr;
        entry_kind = (an_il_entry_kind)idp->entity.kind;
      } else {
        /* This is a primary declaration. */
        entry_ptr = ssep->entity.ptr;
        entry_kind = (an_il_entry_kind)ssep->entity.kind;
        if (entry_kind == iek_type) {
          /* Check for a typedef that gives its name to a class for linkage
             purposes, and keep it if the class is being kept.  This is needed
             in cfront mode. */
          a_type_ptr type = (a_type_ptr)entry_ptr;
          if (type->kind == (a_type_kind)tk_typeref &&
              typeref_is_typedef(type)) {
            a_type_ptr subtype = type->variant.typeref.type;
            if (is_immediate_class_type(subtype) &&
                subtype->variant.class_struct_union.originally_unnamed &&
                subtype->source_corresp.name == type->source_corresp.name &&
                il_entry_prefix_of(subtype).keep_in_il) {
              il_entry_prefix_of(type).keep_in_il = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      if (function_local) {
        /* Keep all function-local source sequence entries. */
        keep_in_il = TRUE;
        /* Mark the associated entity to be kept in the IL.  This is generally
           unnecessary, but it is needed for block extern declarations. */
        walk_ptr(entry_ptr, a_char_ptr, entry_kind);
      } else {
        /* See if the associated IL entity is marked with keep_in_il. */
        keep_in_il = il_entry_prefix_of(entry_ptr).keep_in_il;
      }  /* if */
      /* Mark the source sequence entry the right way (and the secondary
         declaration or end-of-construct entry too, if there is one). */
      if (keep_in_il) {
        il_entry_prefix_of(ssep).keep_in_il = TRUE;
        if (ecp != NULL) {
          il_entry_prefix_of(ecp).keep_in_il = TRUE;
        } else if (idp != NULL) {
          il_entry_prefix_of(idp).keep_in_il = TRUE;
        } else if (sec_decl != NULL) {
          a_type_ptr decl_type = sec_decl->declared_type;
          il_entry_prefix_of(sec_decl).keep_in_il = TRUE;
          if (decl_type != NULL) {
            /* Mark the declared_type of the secondary declaration as needed
               if it's not already marked as needed.  This will require
               sweeping the source sequence list entries again, because there
               may be several source sequence entries pointing to this type. */
            if (!il_entry_prefix_of(decl_type).keep_in_il) {
              walk_ptr(decl_type, a_type_ptr, iek_type);
              /* When scanning a function list we don't need another pass if
                 the type is in the function list because we mark all entities
                 as needed, and if it is in the file scope we haven't yet
                 walked that list. */
              if (!function_local) *need_another_pass = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* r_set_keep_in_il_on_sslist */


static void set_keep_in_il_on_sslist(
                                    a_source_sequence_entry_ptr sslist,
                                    a_boolean                   function_local)
/*
Walk the indicated source sequence list, and set the keep_in_il
flags in the source sequence entries thereon.  A source sequence entry
must be kept in the IL if and only if its associated entry must be
kept.  If function_local is TRUE, this list is local to a function that
is being kept, and all entries should be marked to be kept.
*/
{
  a_boolean need_another_pass;

  /* Scan the list, and again if something was changed that forces a rescan. */
  do {
    need_another_pass = FALSE;
    r_set_keep_in_il_on_sslist(sslist, function_local, &need_another_pass);
  } while (need_another_pass);
}  /* set_keep_in_il_on_sslist */


static void set_keep_in_il_on_source_sequence_entries(a_scope_ptr scope)
/*
Walk the indicated scope's source sequence list, and set the keep_in_il
flags in the source sequence entries thereon.  A source sequence entry
must be kept in the IL if and only if its associated entry must be
kept.  This routine must be called late so that all the keep_in_il
flags are set already.  This processing is important for secondary
entries, which are not pointed to by the associated entry, and also
for entries like classes that get a "shallow walk" until the end of
the file scope because their subtree can change (e.g., on a definition
or redeclaration).
*/
{
  a_src_seq_sublist_ptr ssslp;
  a_boolean             function_local =
                                  (scope->kind == (a_scope_kind)sck_function ||
                                   scope->kind == (a_scope_kind)sck_block);

  set_keep_in_il_on_sslist(scope->source_sequence_list, function_local);
  /* For a function scope, visit the sublists in the file scope too. */
  for (ssslp = scope->src_seq_sublist_list;
       ssslp != NULL;
       ssslp = ssslp->next) {
    set_keep_in_il_on_sslist(ssslp->source_sequence_list,
                             function_local);
  }  /* for */
}  /* set_keep_in_il_on_source_sequence_entries */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void walk_subtrees_of_local_entities(a_scope_ptr scope)
/*
scope is a function or block scope.  Visit the local classes, variables,
and routines of the scope and set a flag indicating that henceforth their
subtrees can be walked in the "needed" flag and keep_in_il processing.
If any entries have already been marked, walk their subtrees now.
This is called at the end of the processing for the function of
which these are local declarations.
*/
{
  a_variable_ptr var;
  a_type_ptr     type;
  a_routine_ptr  rout;
  a_scope_ptr    subscope;

  if (scope->kind == (a_scope_kind)sck_function) {
    /* Process the parameter variables of the function. */
    for (var = scope->variant.routine.parameters;
         var != NULL;
         var = var->next) {
      var->source_corresp.okay_to_walk_subtree_of_local_entity = TRUE;
      remark_as_needed    ((char *)var, (an_il_entry_kind)iek_variable);
      remark_to_keep_in_il((char *)var, (an_il_entry_kind)iek_variable);
    }  /* for */
  }  /* if */
  for (var = scope->variables; var != NULL; var = var->next) {
    var->source_corresp.okay_to_walk_subtree_of_local_entity = TRUE;
    remark_as_needed    ((char *)var, (an_il_entry_kind)iek_variable);
    remark_to_keep_in_il((char *)var, (an_il_entry_kind)iek_variable);
  }  /* for */
  for (var = scope->nonstatic_variables; var != NULL; var = var->next) {
    var->source_corresp.okay_to_walk_subtree_of_local_entity = TRUE;
    remark_as_needed    ((char *)var, (an_il_entry_kind)iek_variable);
    remark_to_keep_in_il((char *)var, (an_il_entry_kind)iek_variable);
  }  /* for */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp;
      type->source_corresp.okay_to_walk_subtree_of_local_entity = TRUE;
      remark_as_needed    ((char *)type, (an_il_entry_kind)iek_type);
      remark_to_keep_in_il((char *)type, (an_il_entry_kind)iek_type);
      ctsp = type->variant.class_struct_union.extra_info;
      if (ctsp != NULL && ctsp->assoc_scope != NULL) {
        /* Set the flag on nested classes and other members too. */
        walk_subtrees_of_local_entities(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  for (rout = scope->routines; rout != NULL; rout = rout->next) {
    rout->source_corresp.okay_to_walk_subtree_of_local_entity = TRUE;
    remark_as_needed    ((char *)rout, (an_il_entry_kind)iek_routine);
    remark_to_keep_in_il((char *)rout, (an_il_entry_kind)iek_routine);
  }  /* for */
  /* Process nested block scopes. */
  for (subscope = scope->scopes; subscope != NULL; subscope = subscope->next) {
    walk_subtrees_of_local_entities(subscope);
  }  /* for */
}  /* walk_subtrees_of_local_entities */

#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */

#if IL_SHOULD_BE_WRITTEN_TO_FILE || REMAP_ONLY_ROUTINES_NEEDED
/*
Macro to remap an orphan IL entry pointer from an "old" value to a "new"
value.  This macro is similar to remap_ptr, but all pointers are processed
as (char *), ptr is the pointer, and entry_kind is the kind of entry
pointed to.
*/
#define remap_orphan_ptr(ptr, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (char *)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_orphan_ptr */
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE || REMAP_ONLY_ROUTINES_NEEDED */

#if REMAP_ONLY_ROUTINES_NEEDED

void remap_first_ptr_of_orphaned_file_scope_entry_array(
                                           a_remap_function_ptr remap_function)
/*
Remap the "first" pointers in the orphaned_file_scope_il_entries array by
running them through the indicated remapping function.
*/
{
  a_remap_function_ptr saved_walk_remap_func = walk_remap_func;
  a_remap_function_ptr saved_walk_list_remap_func = walk_list_remap_func;

  walk_remap_func = remap_function;
  walk_list_remap_func = NULL;  /* Not used. */

#define remap_orphan_entry_first(kind) \
  remap_orphan_ptr(orphaned_file_scope_il_entries[(int)(kind)].first_entry, \
                   (kind))

  remap_orphan_entry_first(iek_source_file);
  remap_orphan_entry_first(iek_constant);
  remap_orphan_entry_first(iek_param_type);
  remap_orphan_entry_first(iek_routine_type_supplement);
  remap_orphan_entry_first(iek_based_type_list_member);
  remap_orphan_entry_first(iek_type);
  remap_orphan_entry_first(iek_variable);
#ifdef CFE
  remap_orphan_entry_first(iek_field);
  remap_orphan_entry_first(iek_exception_specification);
  remap_orphan_entry_first(iek_exception_specification_type);
#endif /* ifdef CFE */
  remap_orphan_entry_first(iek_routine);
  remap_orphan_entry_first(iek_label);
  remap_orphan_entry_first(iek_expr_node);
#ifdef CFE
  remap_orphan_entry_first(iek_for_loop);
  remap_orphan_entry_first(iek_switch_clause);
  remap_orphan_entry_first(iek_handler);
  remap_orphan_entry_first(iek_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  remap_orphan_entry_first(iek_microsoft_try_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
  remap_orphan_entry_first(iek_block);
  remap_orphan_entry_first(iek_statement);
  remap_orphan_entry_first(iek_object_lifetime);
  remap_orphan_entry_first(iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#ifdef FFE
  remap_orphan_entry_first(iek_internal_complex_value);
  remap_orphan_entry_first(iek_bound_info_entry);
  remap_orphan_entry_first(iek_do_loop);
  remap_orphan_entry_first(iek_label_list_entry);
  remap_orphan_entry_first(iek_io_specifier);
  remap_orphan_entry_first(iek_io_list_item);
  remap_orphan_entry_first(iek_namelist_group_member);
  remap_orphan_entry_first(iek_namelist_group);
  remap_orphan_entry_first(iek_input_output_description);
  remap_orphan_entry_first(iek_entry_param);
  remap_orphan_entry_first(iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  remap_orphan_entry_first(iek_namespace);
  remap_orphan_entry_first(iek_using_decl);
  remap_orphan_entry_first(iek_dynamic_init);
  remap_orphan_entry_first(iek_local_static_variable_init);
  remap_orphan_entry_first(iek_vla_dimension);
  remap_orphan_entry_first(iek_overriding_virtual_function);
  remap_orphan_entry_first(iek_derivation_step);
  remap_orphan_entry_first(iek_base_class_derivation);
  remap_orphan_entry_first(iek_base_class);
  remap_orphan_entry_first(iek_class_list_entry);
  remap_orphan_entry_first(iek_routine_list_entry);
  remap_orphan_entry_first(iek_class_type_supplement);
  remap_orphan_entry_first(iek_template_param_type_supplement);
  remap_orphan_entry_first(iek_constructor_init);
  remap_orphan_entry_first(iek_asm_entry);
  remap_orphan_entry_first(iek_template_arg);
  remap_orphan_entry_first(iek_new_delete_supplement);
  remap_orphan_entry_first(iek_throw_supplement);
#if !ABI_CHANGES_FOR_RTTI
  remap_orphan_entry_first(iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  remap_orphan_entry_first(iek_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifdef CFE */
  remap_orphan_entry_first(iek_template_parameter);
  remap_orphan_entry_first(iek_template_decl);
#if RECORD_FORM_OF_NAME_REFERENCE
  remap_orphan_entry_first(iek_name_reference);
  remap_orphan_entry_first(iek_name_qualifier);
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_src_seq_secondary_decl and
     iek_src_seq_end_of_construct, since such entries will never appear on an
     orphan list.  Ditto for iek_src_seq_sublist. */
  /* Nothing needed for iek_scope_orphaned_list_header,
     iek_hidden_name, iek_pragma, iek_template, iek_macro, and
     iek_per_instantiation_needed_flags_entry. */
#undef remap_orphan_entry_first
  walk_remap_func = saved_walk_remap_func;
  walk_list_remap_func = saved_walk_list_remap_func;
}  /* remap_first_ptr_of_orphaned_file_scope_entry_array */

#endif /* REMAP_ONLY_ROUTINES_NEEDED */
#if IL_SHOULD_BE_WRITTEN_TO_FILE

void remap_last_ptr_of_orphaned_file_scope_entry_array(
                                           a_remap_function_ptr remap_function)
/*
Remap the "last" pointers in the orphaned_file_scope_il_entries array by
running them through the indicated remapping function.
*/
{
  a_remap_function_ptr saved_walk_remap_func = walk_remap_func;
  a_remap_function_ptr saved_walk_list_remap_func = walk_list_remap_func;

  walk_remap_func = remap_function;
  walk_list_remap_func = NULL;  /* Not used. */

#define remap_orphan_entry_last(kind) \
  remap_orphan_ptr(orphaned_file_scope_il_entries[(int)(kind)].last_entry, \
                   (kind))

  remap_orphan_entry_last(iek_source_file);
  remap_orphan_entry_last(iek_constant);
  remap_orphan_entry_last(iek_param_type);
  remap_orphan_entry_last(iek_routine_type_supplement);
  remap_orphan_entry_last(iek_based_type_list_member);
  remap_orphan_entry_last(iek_type);
  remap_orphan_entry_last(iek_variable);
#ifdef CFE
  remap_orphan_entry_last(iek_field);
  remap_orphan_entry_last(iek_exception_specification);
  remap_orphan_entry_last(iek_exception_specification_type);
#endif /* ifdef CFE */
  remap_orphan_entry_last(iek_routine);
  remap_orphan_entry_last(iek_label);
  remap_orphan_entry_last(iek_expr_node);
#ifdef CFE
  remap_orphan_entry_last(iek_for_loop);
  remap_orphan_entry_last(iek_switch_clause);
  remap_orphan_entry_last(iek_handler);
  remap_orphan_entry_last(iek_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  remap_orphan_entry_last(iek_microsoft_try_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
  remap_orphan_entry_last(iek_block);
  remap_orphan_entry_last(iek_statement);
  remap_orphan_entry_last(iek_object_lifetime);
  remap_orphan_entry_last(iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#ifdef FFE
  remap_orphan_entry_last(iek_internal_complex_value);
  remap_orphan_entry_last(iek_bound_info_entry);
  remap_orphan_entry_last(iek_do_loop);
  remap_orphan_entry_last(iek_label_list_entry);
  remap_orphan_entry_last(iek_io_specifier);
  remap_orphan_entry_last(iek_io_list_item);
  remap_orphan_entry_last(iek_namelist_group_member);
  remap_orphan_entry_last(iek_namelist_group);
  remap_orphan_entry_last(iek_input_output_description);
  remap_orphan_entry_last(iek_entry_param);
  remap_orphan_entry_last(iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  remap_orphan_entry_last(iek_namespace);
  remap_orphan_entry_last(iek_using_decl);
  remap_orphan_entry_last(iek_dynamic_init);
  remap_orphan_entry_last(iek_local_static_variable_init);
  remap_orphan_entry_last(iek_vla_dimension);
  remap_orphan_entry_last(iek_overriding_virtual_function);
  remap_orphan_entry_last(iek_derivation_step);
  remap_orphan_entry_last(iek_base_class_derivation);
  remap_orphan_entry_last(iek_base_class);
  remap_orphan_entry_last(iek_class_list_entry);
  remap_orphan_entry_last(iek_routine_list_entry);
  remap_orphan_entry_last(iek_class_type_supplement);
  remap_orphan_entry_last(iek_template_param_type_supplement);
  remap_orphan_entry_last(iek_constructor_init);
  remap_orphan_entry_last(iek_asm_entry);
  remap_orphan_entry_last(iek_template_arg);
  remap_orphan_entry_last(iek_new_delete_supplement);
  remap_orphan_entry_last(iek_throw_supplement);
#if !ABI_CHANGES_FOR_RTTI
  remap_orphan_entry_last(iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  remap_orphan_entry_last(iek_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifdef CFE */
  remap_orphan_entry_last(iek_template_parameter);
  remap_orphan_entry_last(iek_template_decl);
#if RECORD_FORM_OF_NAME_REFERENCE
  remap_orphan_entry_last(iek_name_reference);
  remap_orphan_entry_last(iek_name_qualifier);
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_src_seq_secondary_decl and
     iek_src_seq_end_of_construct, since such entries will never appear on an
     orphan list. */
  /* Nothing needed for iek_scope_orphaned_list_header,
     iek_hidden_name, iek_pragma, iek_template, iek_macro, and
     iek_per_instantiation_needed_flags_entry. */
#undef remap_orphan_entry_last
  walk_remap_func = saved_walk_remap_func;
  walk_list_remap_func = saved_walk_list_remap_func;
}  /* remap_last_ptr_of_orphaned_file_scope_entry_array */

#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Build a routine to walk entries in isolation (i.e., not as part
   of a tree walk).  This is built from the walk_entry.h source
   using special macro settings. */
#undef DO_SUBTREE_WALK
#define DO_SUBTREE_WALK FALSE
#undef NEEDED_FLAG_WALK
#define NEEDED_FLAG_WALK FALSE
#undef KEEP_IN_IL_WALK
#define KEEP_IN_IL_WALK FALSE
#undef WALK_ENTRY_ROUTINE_STATIC
#define WALK_ENTRY_ROUTINE_STATIC static
#undef WALK_ENTRY_ROUTINE_NAME
#define WALK_ENTRY_ROUTINE_NAME remap_pointers_in_entry
#undef WALK_ORPHANED_ENTRY_ROUTINE_NAME
#undef UNDEF_WALK_ENTRY_MACROS_AT_END
#define UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"


void remap_pointers_in_il_entry(char                 *entry_ptr,
                                an_il_entry_kind     entry_kind,
                                a_remap_function_ptr remap_function,
                                a_remap_function_ptr list_remap_function)
/*
Remap the pointers in the indicated entry (of kind entry_kind) by running
them through the indicated remapping routines.  list_remap_function is
used for list pointers, i.e., "next" pointers and start-of-list pointers.
The subtree is not processed.
*/
{
  a_remap_function_ptr saved_walk_remap_func = walk_remap_func;
  a_remap_function_ptr saved_walk_list_remap_func = walk_list_remap_func;

  walk_remap_func = remap_function;
  walk_list_remap_func = list_remap_function;

  remap_pointers_in_entry(entry_ptr, entry_kind);

  walk_remap_func = saved_walk_remap_func;
  walk_list_remap_func = saved_walk_list_remap_func;
}  /* remap_pointers_in_il_entry */


#undef DO_SUBTREE_WALK
#undef NEEDED_FLAG_WALK
#undef KEEP_IN_IL_WALK
#undef WALK_ENTRY_ROUTINE_STATIC
#undef WALK_ENTRY_ROUTINE_NAME
#undef WALK_ORPHANED_ENTRY_ROUTINE_NAME


void il_walk_init(void)
/*
Initialize static variables related to IL walking.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in il_walk.h: */
  flag_value_meaning_visited = 0;
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
  end_of_file_scope_needed_flags_phase = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
  /* Variables in il_walk.c: */
  entry_process_func = NULL;
  string_entry_process_func = NULL;
  walk_termination_test_func = NULL;
  walk_remap_func = NULL;
  walk_list_remap_func = NULL;
  clear_fe_pointers_during_walk = FALSE;
  walking_file_scope = FALSE;
  walking_secondary_trans_unit = FALSE;
}  /* il_walk_init */

#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

#if NEED_DECLARATIVE_WALK

void walk_declarative_entities_in_scope(
                          a_scope_ptr                   scope,
                          an_entry_process_function_ptr entry_process_function)
/*
Walk all the declarative entities contained in the indicated scope, and
call the given processing function for each entity.
*/
{
  a_type_ptr      type;
  a_variable_ptr  variable;
  a_routine_ptr   routine;
  a_scope_ptr     block_scope;
  a_namespace_ptr nsp;

  /* Some things not visited:
       -- Parameters of routines.
       -- Handler parameters (in exception catch clauses).
       -- enum constants.
  */
  /* Visit all types. */
  for (type = scope->types; type != NULL; type = type->next) {
    (*entry_process_function)((char *)type, iek_type);
    if (is_immediate_class_type(type)) {
      /* For a class, visit the members. */
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      a_field_ptr field;
      /* Visit the nonstatic data members (these exist even in C). */
      for (field = type->variant.class_struct_union.field_list;
           field != NULL;
           field = field->next) {
        (*entry_process_function)((char *)field, iek_field);
      }  /* for */
      /* Visit the class scope if it has one. */
      if (ctsp != NULL) {
        a_scope_ptr class_scope = ctsp->assoc_scope;
        if (class_scope != NULL) {
          walk_declarative_entities_in_scope(class_scope,
                                             entry_process_function);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all nonstatic variables (only present in function and block
     scopes). */
  for (variable = scope->nonstatic_variables;
       variable != NULL;
       variable = variable->next) {
    (*entry_process_function)((char *)variable, iek_variable);
  }  /* for */
  /* Visit all static variables (if this is a class scope, these are the
     static data members). */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    (*entry_process_function)((char *)variable, iek_variable);
  }  /* for */
  /* Visit all routines (if this is a class scope, these are the member
     functions). */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    (*entry_process_function)((char *)routine, iek_routine);
  }  /* for */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces;
       nsp != NULL;
       nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      /* Note that no call is made for the namespace itself. */
      walk_declarative_entities_in_scope(nsp->variant.assoc_scope,
                                         entry_process_function);
    }  /* if */
  }  /* for */
  /* Visit all block scopes (only present in function and block scopes). */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    walk_declarative_entities_in_scope(block_scope, entry_process_function);
  }  /* for */
}  /* walk_declarative_entities_in_scope */

#endif /* NEED_DECLARATIVE_WALK */

void process_local_types(
                    a_scope_ptr                        scope,
                    a_type_list_processing_routine_ptr list_processing_routine)
/*
Process local types in or under the indicated scope (a file, namespace, class,
function, or block scope) by calling the indicated routine to process a
type list.  Types are processed either from the orphan lists, when this
routine is called for the file scope, or from function and local scopes
in cases where the orphan lists have not been generated yet.
*/
{
  a_routine_ptr   routine;
  a_type_ptr      type;
  a_namespace_ptr nsp;
  a_scope_ptr     subscope;
  a_scope_ptr     saved_innermost_function_scope;

  if (scope->kind == (a_scope_kind)sck_file) {
    /* Process the types on the orphan lists. */
    a_scope_orphaned_list_header_ptr solhp;
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      saved_innermost_function_scope = innermost_function_scope;
      check_assertion(solhp->assoc_routine != NULL);
      if (solhp->assoc_routine->assoc_scope != NULL_region_number) {
        innermost_function_scope = 
               il_header.region_scope_entry[solhp->assoc_routine->assoc_scope];
      } else {
        innermost_function_scope = NULL;
      }  /* if */
      list_processing_routine(solhp->orphaned_types);
      innermost_function_scope = saved_innermost_function_scope;
    }  /* for */
#if DO_IL_LOWERING
    /* Templates instantiated late in the primary translation unit in
       the presence of exported templates have their lowering delayed
       (see pop_scope), and therefore their orphan lists are not
       constructed yet.  Visit the local scopes directly.  In all other
       cases, we're done. */
    if (C_mode() ||
        !is_primary_translation_unit ||
        !function_body_processing_delayed_on_some_func_in_primary_il)
#endif /* DO_IL_LOWERING */
    /* Do not insert code here. */
    {
      goto end_of_routine;
    }  /* if */
  }  /* if */
  /* Visit all functions. */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    if (routine->assoc_scope != NULL_region_number) {
      a_scope_ptr func_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
      /* Process a function's scope if its orphan lists have not yet
         been generated. */
      if (func_scope != NULL &&
          !func_scope->function_body_processing_finished) {
        saved_innermost_function_scope = innermost_function_scope;
        innermost_function_scope = func_scope;
        process_local_types(func_scope, list_processing_routine);
        innermost_function_scope = saved_innermost_function_scope;
      }  /* if */
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_function ||
      scope->kind == (a_scope_kind)sck_block) {
    /* Function and block scopes we encounter at this level should
       not have orphan lists generated. */
    check_assertion(!scope->scope_orphaned_list_header_generated);
    /* Process all local types. */
    list_processing_routine(scope->types);
  }  /* if */
  if (!C_mode()) {
    /* Visit classes with definitions in order to process any subscopes. */
    for (type = scope->types;
         type != NULL;
         type = type->next) {
      if (is_immediate_class_type(type) &&
          /* Watch out for classes generated by lowering. */
          type->variant.class_struct_union.extra_info != NULL) {
        a_scope_ptr class_scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          process_local_types(class_scope, list_processing_routine);
        }  /* if */
      }  /* if */
    }  /* for */
    /* Visit all namespaces. */
    for (nsp = scope->namespaces;
         nsp != NULL;
         nsp = nsp->next) {
      if (!nsp->is_namespace_alias) {
        process_local_types(nsp->variant.assoc_scope, list_processing_routine);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Visit all block scopes. */
  for (subscope = scope->scopes;
       subscope != NULL;
       subscope = subscope->next) {
    process_local_types(subscope, list_processing_routine);
  }  /* for */
end_of_routine:;
}  /* process_local_types */


void clear_expr_or_stmt_traversal_block(
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Clear the block used for expression or statement tree traversal to
default values.
*/
{
  tblock->process_expr = NULL;
  tblock->process_constant = NULL;
  tblock->process_dynamic_init = NULL;
  tblock->process_statement = NULL;
  tblock->terminate = FALSE;
  tblock->suppress_subtree_walk = FALSE;
  tblock->result = FALSE;
  tblock->process_non_dynamic_constants = FALSE;
}  /* clear_expr_or_stmt_traversal_block */


static void traverse_expr_list(an_expr_node_ptr                    expr_list,
                               an_expr_or_stmt_traversal_block_ptr tblock);


static void traverse_constant_list(
                             a_constant_ptr                      constant_list,
                             an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given constant list.  Call user-provided routines as
specified in the control block.
*/
{
  a_constant_ptr con;

  for (con = constant_list; con != NULL; con = con->next) {
    traverse_constant(con, tblock);
    if (tblock->terminate) break;
  }  /* for */
}  /* traverse_constant_list */


void traverse_constant(a_constant_ptr                      constant,
                       an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given constant.  Call user-provided routines as
specified in the control block.  A constant can have a "tree" when
it's the initializer for an aggregate.
*/
{
  if (tblock->process_constant != NULL) {
    /* Call the user-provided routine. */
    tblock->process_constant(constant, tblock);
    /* Terminate the walk if told to do so. */
    if (tblock->terminate) goto end_of_routine;
    /* Skip the subtree walk if told to do so. */
    if (tblock->suppress_subtree_walk) {
      tblock->suppress_subtree_walk = FALSE;
      goto end_of_routine;
    }  /* if */
  }  /* if */
  if (constant->kind == (a_constant_repr_kind)ck_aggregate) {
    traverse_constant_list(constant->variant.aggregate.first_constant,
                           tblock);
  } else if (constant->kind == (a_constant_repr_kind)ck_init_repeat) {
    traverse_constant(constant->variant.init_repeat.constant, tblock);
  } else if (constant->kind == (a_constant_repr_kind)ck_dynamic_init) {
    traverse_dynamic_init(constant->variant.dynamic_init, tblock);
  }  /* if */
end_of_routine:;
}  /* traverse_constant */


void traverse_dynamic_init(a_dynamic_init_ptr                  dip,
                           an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given dynamic initialization.  Call user-provided
routines as specified in the control block.
*/
{
  if (tblock->process_dynamic_init != NULL) {
    /* Call the user-provided routine. */
    tblock->process_dynamic_init(dip, tblock);
    /* Terminate the walk if told to do so. */
    if (tblock->terminate) goto end_of_routine;
    /* Skip the subtree walk if told to do so. */
    if (tblock->suppress_subtree_walk) {
      tblock->suppress_subtree_walk = FALSE;
      goto end_of_routine;
    }  /* if */
  }  /* if */
  switch (dip->kind) {
    case dik_none:
    case dik_zero:
      break;
    case dik_constant:
      if (tblock->process_non_dynamic_constants) {
        traverse_constant(dip->variant.constant, tblock);
      }  /* if */
      break;
    case dik_expression:
    case dik_call_returning_class_via_cctor:
      traverse_expr(dip->variant.expression, tblock);
      break;
    case dik_constructor:
      traverse_expr_list(dip->variant.constructor.args, tblock);
      break;
    case dik_nonconstant_aggregate:
      traverse_constant(dip->variant.constant, tblock);
      break;
    case dik_bitwise_copy:
      break;
    default:
      unexpected_condition_str("traverse_dynamic_init: bad kind");
  }  /* switch */
end_of_routine:;
}  /* traverse_dynamic_init */


static void traverse_expr_list(an_expr_node_ptr                    expr_list,
                               an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given expression list.  Call user-provided routines
as specified in the control block.
*/
{
  an_expr_node_ptr expr;

  for (expr = expr_list; expr != NULL; expr = expr->next) {
    traverse_expr(expr, tblock);
    /* Terminate the walk if told to do so. */
    if (tblock->terminate) break;
  }  /* for */
}  /* traverse_expr_list */


void traverse_expr(an_expr_node_ptr                    expr,
                   an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given expression.  Call user-provided routines
as specified in the control block.
*/
{
  if (tblock->process_expr != NULL) {
    /* Call the user-provided routine. */
    tblock->process_expr(expr, tblock);
    /* Terminate the walk if told to do so. */
    if (tblock->terminate) goto end_of_routine;
    /* Skip the subtree walk if told to do so. */
    if (tblock->suppress_subtree_walk) {
      tblock->suppress_subtree_walk = FALSE;
      goto end_of_routine;
    }  /* if */
  }  /* if */
  switch (expr->kind) {
    case enk_error:
      break;
    case enk_operation:
      traverse_expr_list(expr->variant.operation.operands, tblock);
      break;
    case enk_constant:
      if (tblock->process_non_dynamic_constants) {
        traverse_constant(expr->variant.constant, tblock);
      }  /* if */
      break;
    case enk_variable:
    case enk_variable_address:
    case enk_field:
      break;
    case enk_temp_init:
      traverse_dynamic_init(expr->variant.init.dynamic_init, tblock);
      break;
    case enk_new_delete:
      { a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
        if (ndsp->arg != NULL) {
          traverse_expr_list(ndsp->arg, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
        if (ndsp->dynamic_init != NULL) {
          traverse_dynamic_init(ndsp->dynamic_init, tblock);
          if (tblock->terminate) goto end_of_routine;
         }  /* if */
        if (ndsp->freeing_of_storage_on_exception != NULL) {
          traverse_dynamic_init(ndsp->freeing_of_storage_on_exception, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
      }
      break;
    case enk_throw:
      if (expr->variant.throw_info != NULL) {
        traverse_dynamic_init(expr->variant.throw_info->dynamic_init, tblock);
        if (tblock->terminate) goto end_of_routine;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        if (expr->variant.throw_info->expr != NULL) {
          traverse_expr(expr->variant.throw_info->expr, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
      }  /* if */
      break;
    case enk_condition:
      traverse_dynamic_init(expr->variant.condition->dynamic_init, tblock);
      if (tblock->terminate) goto end_of_routine;
      traverse_expr(expr->variant.condition->expr, tblock);
      break;
    case enk_object_lifetime:
      traverse_expr(expr->variant.object_lifetime.expr, tblock);
      break;
    case enk_typeid:
      if (expr->variant.typeid_info.expr != NULL) {
        traverse_expr(expr->variant.typeid_info.expr, tblock);
      }  /* if */
      break;
    case enk_runtime_sizeof:
      if (!expr->variant.runtime_sizeof.is_type) {
        traverse_expr(expr->variant.runtime_sizeof.variant.expr, tblock);
      }  /* if */
      break;
    case enk_address_of_ellipsis:
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      traverse_statement(expr->variant.statement, tblock);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case enk_lowered_eh_construct:
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    case enk_routine_address:
      break;
    default:
      unexpected_condition_str("traverse_expr: bad expr kind");
  }  /* switch */
end_of_routine:;
}  /* traverse_expr */


void traverse_statement_list(
                            a_statement_ptr                     statement_list,
                            an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given statement list.  Call user-provided routines
as specified in the control block.
*/
{
  a_statement_ptr stmt;

  for (stmt = statement_list; stmt != NULL; stmt = stmt->next) {
    traverse_statement(stmt, tblock);
    if (tblock->terminate) break;
  }  /* for */
}  /* traverse_statement_list */


void traverse_statement(a_statement_ptr                     statement,
                        an_expr_or_stmt_traversal_block_ptr tblock)
/*
Walk the tree of the given statement.  Call user-provided routines
as specified in the control block.
*/
{
  if (tblock->process_statement != NULL) {
    /* Call the user-provided routine. */
    tblock->process_statement(statement, tblock);
    /* Terminate the walk if told to do so. */
    if (tblock->terminate) goto end_of_routine;
    /* Skip the subtree walk if told to do so. */
    if (tblock->suppress_subtree_walk) {
      tblock->suppress_subtree_walk = FALSE;
      goto end_of_routine;
    }  /* if */
  }  /* if */
  switch (statement->kind) {
    case stmk_expr:
      traverse_expr(statement->expr, tblock);
      break;
    case stmk_if:
      traverse_expr(statement->expr, tblock);
      if (tblock->terminate) goto end_of_routine;
      traverse_statement(statement->variant.if_stmt.then_statement, tblock);
      if (tblock->terminate) goto end_of_routine;
      if (statement->variant.if_stmt.else_statement != NULL) {
        traverse_statement(statement->variant.if_stmt.else_statement, tblock);
      }  /* if */
      break;
    case stmk_while:
      traverse_expr(statement->expr, tblock);
      if (tblock->terminate) goto end_of_routine;
      traverse_statement(statement->variant.loop_statement, tblock);
      break;
    case stmk_end_test_while:
      traverse_statement(statement->variant.loop_statement, tblock);
      if (tblock->terminate) goto end_of_routine;
      traverse_expr(statement->expr, tblock);
      break;
    case stmk_goto:
    case stmk_label:
      break;
    case stmk_return:
      if (statement->variant.return_dynamic_init != NULL) {
        traverse_dynamic_init(statement->variant.return_dynamic_init, tblock);
      } else if (statement->expr != NULL) {
        traverse_expr(statement->expr, tblock);
      }  /* if */
      break;
    case stmk_block:
      traverse_statement_list(statement->variant.block.statements, tblock);
      break;
    case stmk_for:
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
      { a_for_loop_ptr flp = statement->variant.for_loop.extra_info;
        if (flp->initialization != NULL) {
          traverse_statement(flp->initialization, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
        /* "expr" is the termination test expression. */
        if (statement->expr != NULL) {
          traverse_expr(statement->expr, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
        traverse_statement(statement->variant.for_loop.statement, tblock);
        if (tblock->terminate) goto end_of_routine;
        if (flp->increment != NULL) {
          traverse_expr(flp->increment, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
#if UPC_EXTENSIONS_ALLOWED
        if (flp->affinity != NULL) {
          traverse_expr(flp->affinity, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
      }
      break;
    case stmk_switch:
      { a_switch_clause_ptr scp;
        traverse_expr(statement->expr, tblock);
        if (tblock->terminate) goto end_of_routine;
        traverse_statement(statement->variant.switch_stmt.body_statement,
                           tblock);
        if (tblock->terminate) goto end_of_routine;
        for (scp = statement->variant.switch_stmt.clause_list;
             scp != NULL;
             scp = scp->next) {
          if (tblock->process_non_dynamic_constants) {
            traverse_constant_list(scp->constant_list, tblock);
            if (tblock->terminate) goto end_of_routine;
          }  /* if */
          traverse_statement_list(scp->statements, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* for */
      }
      break;
    case stmk_init:
      traverse_dynamic_init(statement->variant.dynamic_init, tblock);
      break;
    case stmk_asm:
      break;
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
      break;
#endif /* ASM_FUNCTION_ALLOWED */
    case stmk_try_block:
      { a_try_supplement_ptr tsp = statement->variant.try_block;
        a_handler_ptr        handler;
        traverse_statement(tsp->statement, tblock);
        if (tblock->terminate) goto end_of_routine;
        for (handler = tsp->handlers;
             handler != NULL;
             handler = handler->next) {
          traverse_dynamic_init(handler->dynamic_init, tblock);
          if (tblock->terminate) goto end_of_routine;
          traverse_statement(handler->statement, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* for */
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case stmk_microsoft_try:
      { a_microsoft_try_supplement_ptr mtsp = statement->variant.microsoft_try;
        traverse_statement(mtsp->guarded_statement, tblock);
        if (tblock->terminate) goto end_of_routine;
        if (mtsp->except_expr != NULL) {
          traverse_expr(mtsp->except_expr, tblock);
          if (tblock->terminate) goto end_of_routine;
        }  /* if */
        traverse_statement(mtsp->cleanup_statement, tblock);
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case stmk_set_vla_size:
      { a_vla_dimension_ptr vlap = statement->variant.vla_dimension;
        traverse_expr(vlap->dimension_expr, tblock);
      }
      break;
    case stmk_vla_decl:
    case stmk_vla_dealloc:
      break;
#if UPC_EXTENSIONS_ALLOWED
    case stmk_upc_notify:
    case stmk_upc_wait:
    case stmk_upc_barrier:
    case stmk_upc_fence:
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    case stmk_assigned_goto:
      /* Used for GNU "goto *expr;". */
      traverse_expr(statement->expr, tblock);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
    case stmk_empty:
      break;
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
    default:
      unexpected_condition_str("traverse_statement: bad statement kind");
  }  /* if */
end_of_routine:;
}  /* traverse_statement */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2003 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
