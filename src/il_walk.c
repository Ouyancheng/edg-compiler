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

il_walk.c -- Routines to walk the intermediate language tree.

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if IL_WALK_NEEDED || NEED_DECLARATIVE_WALK

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "il_walk.h"

#if IL_WALK_NEEDED

#if !ORPHAN_PROCESSING_NEEDED
 #error -- ORPHAN_PROCESSING_NEEDED must be set if IL walking is needed.
#endif /* !ORPHAN_PROCESSING_NEEDED */

/* Type of function called to test for termination in the IL walk.
   First parameter is the pointer to the entry and second is the kind
   of entry.  Returned value of TRUE means prune the walk at this entry. */
typedef a_boolean a_walk_termination_test_function(char *, an_il_entry_kind);
typedef a_walk_termination_test_function *a_walk_termination_test_function_ptr;

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
static a_boolean
		walking_file_scope;
			/* TRUE if walking the file-scope IL, FALSE if
			   walking the IL for a function scope. */
static unsigned int
		flag_value_meaning_visited;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between 0 and 1. */
typedef char	*a_char_ptr;
			/* Useful to indicate "char *" as a type in calling
			   remap_ptr or walk_ptr. */


/* Declarations required because of forward references. */
static void walk_string_entry(char             *entry_ptr,
                              an_il_entry_kind entry_kind,
                              sizeof_t         entry_length);
#if MAINTAIN_NEEDED_FLAGS
#if GENERATE_SOURCE_SEQUENCE_LISTS
static void set_keep_in_il_on_source_sequence_entries(a_scope_ptr scope);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* MAINTAIN_NEEDED_FLAGS */


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
  a_boolean	walking_file_scope;
  int		flag_value_meaning_visited;
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
  (saved_state).walking_file_scope         = walking_file_scope;      \
  (saved_state).flag_value_meaning_visited = flag_value_meaning_visited; \
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
  walking_file_scope         = (saved_state).walking_file_scope;      \
  flag_value_meaning_visited = (saved_state).flag_value_meaning_visited; \
}  /* restore_il_walk_state */


void walk_file_scope_il(
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function)
/*
Walk the intermediate language tree for the file scope.  Begin with il_header
and visit the whole file-scope tree, but do not go down into the information
about each function.  Process each non-string entry by calling
entry_process_function on that entry, and each string entry by calling
string_entry_process_function on that entry.  Remap each pointer to a new
value by calling remap_function.  entry_process_function,
string_entry_process_function, or remap_function can be NULL to indicate
that the corresponding function is unnecessary.

The remapping function is used when reading in an IL file.  The IL tree
was in memory in some way, and was written out exactly the way it
looked.  Now it has been read back in, and each memory block is probably
at a different location than when written out.  All of the pointers
need to be updated from their "old" values to the proper "new" values.
That is what the remap function does.
*/
{
  an_il_walk_state saved_state;

  db_enter(4, "walk_file_scope_il");
  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Save the function pointers so they don't have to be passed around. */
  entry_process_func = entry_process_function;
  string_entry_process_func = string_entry_process_function;
  walk_termination_test_func = NULL;
  walk_remap_func = remap_function;
  walking_file_scope = TRUE;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the IL header.  Note that all of these pointers are to the
     file scope memory region. */
  /* Remap and walk the first pointer in two steps, so that we can find
     out what the proper il_walk_flag setting is. */
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  flag_value_meaning_visited =
                     !il_entry_prefix_of(il_header.primary_scope).il_walk_flag;
  /* Walk the main body of the IL. */
  walk_entry_and_subtree((char *)il_header.primary_scope, iek_scope);
  walk_ptr(il_header.primary_source_file, a_source_file_ptr, iek_source_file);
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
  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
  db_exit();
}  /* walk_file_scope_il */


void walk_routine_scope_il(
             a_memory_region_number              region_number,
             an_entry_process_function_ptr       entry_process_function,
             a_string_entry_process_function_ptr string_entry_process_function,
             a_remap_function_ptr                remap_function)
/*
Walk the intermediate language tree for a routine scope.  Begin with the
scope entry for region region_number, and visit the whole scope tree.
Process each non-string entry by calling entry_process_function on that
entry, and each string entry by calling string_entry_process_function on
that entry.  Remap each pointer to a new value by calling remap_function.
entry_process_function, string_entry_process_function, or remap_function
can be NULL to indicate that the corresponding function is unnecessary.
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
  walk_termination_test_func = NULL;
  walk_remap_func = remap_function;
  /* Walking a routine scope, not the file scope. */
  walking_file_scope = FALSE;
  scope = il_header.region_scope_entry[region_number];
  flag_value_meaning_visited = !il_entry_prefix_of(scope).il_walk_flag;
#ifdef FFE
  array_bound_walk_index = 0;
#endif /* ifdef FFE */

  /* Process the scope and its subtree. */
  walk_entry_and_subtree((char *)scope, iek_scope);

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
  db_exit();
}  /* walk_routine_scope_il */

#if MAINTAIN_NEEDED_FLAGS

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
#define UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"


/*
Given an IL entry at entry_ptr with kind entry_kind, return TRUE if the
entry's subtree should not be walked at this time.  This is used when
setting the "needed" or "keep_in_il" flags.  Entities that can be
defined or redeclared later (e.g., classes) shouldn't have their subtrees
walked until after there is no longer the possibility of the subtree changing.
end_of_file_scope_needed_flags_phase is set to TRUE in a phase where subtrees
should finally be walked.  Entities local to functions are always fully
walked immediately.
*/
#define should_not_walk_subtree(entry_ptr, entry_kind) \
 (!end_of_file_scope_needed_flags_phase && \
  (((entry_kind) == iek_type && \
    is_immediate_class_type((a_type_ptr)(entry_ptr)) && \
    !((a_type_ptr)(entry_ptr))->source_corresp.is_local_to_function && \
    !((a_type_ptr)(entry_ptr))->declared_in_function_prototype) || \
   ((entry_kind) == iek_variable && \
    !((a_variable_ptr)(entry_ptr))->source_corresp.is_local_to_function) || \
   ((entry_kind) == iek_routine)))


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

  /* Only certain entry kinds have a "needed" flag.  See if this one does. */
  scp = source_corresp_for_il_entry(entry_ptr, entry_kind);
  if (scp != NULL) {
    /* The entry does have a "needed" flag. */
    if (scp->needed) {
      /* The flag is set already, so prune the walk at this entry.  */
      prune = TRUE;
    } else {
      /* The flag is not set, so set it and keep walking. */
      scp->needed = TRUE;
      if (entry_kind == iek_routine) {
        /* The entry is a routine.  If it has a definition, walk it now.
           Note that walking the routine and its subtree will not
           automatically walk the body. */
        a_routine_ptr rout = (a_routine_ptr)entry_ptr;
        if (rout->defined) {
          a_scope_ptr scope = il_header.region_scope_entry[rout->assoc_scope];
          check_assertion_str(scope != NULL,
              "prune_needed_flag_il_walk: needed routine scope not in memory");
          walk_tree_and_set_needed((char *)scope, iek_scope);
        }  /* if */
      }  /* if */
      /* If this is an entry that might be redeclared or redefined later,
         do not walk its subtree now. */
      if (should_not_walk_subtree(entry_ptr, entry_kind)) prune = TRUE;
#if 0
#else /* 0 */
      /* For now, set the definition_needed flag on a class whenever the needed
         flag is set. */
      if (entry_kind == iek_type) {
        a_type_ptr type = (a_type_ptr)entry_ptr;
        if (is_immediate_class_type(type)) {
          type->variant.class_struct_union.definition_needed = TRUE;
        }  /* if */
      }  /* if */
#endif /* 0 */
    }  /* if */
  }  /* if */
  return prune;
}  /* prune_needed_flag_il_walk */


static void needed_flag_walk_entry_process(char             *entry_ptr,
                                           an_il_entry_kind entry_kind)
/*
Routine called during the "needed" flag IL walk, to process an entry after
its subtree has been walked.
*/
{
  /* After we've processed a function scope, we can dispose of the IL for
     the function. */
  if (entry_kind == iek_scope) {
    a_scope_ptr scope = (a_scope_ptr)entry_ptr;
    if (scope->kind == (a_scope_kind)sck_function) {
      /* This is a function scope. */
      /* The IL for the function will not be changing any more, so walk
         it to note what needs to be kept in the IL (specifically, what
         in the file scope memory region needs to be kept in the IL). */
      mark_to_keep_in_il((char *)scope, iek_scope);
      /* Decide on disposing of the memory region. */
      if (scope->depth_in_scope_stack != NO_SCOPE_DEPTH ||
          innermost_function_scope == scope) {
        /* This function's scope is still on the scope stack, so do nothing
           now.  check_for_done_with_memory_region will be called when the
           scope is popped off the stack.  The innermost_function_scope
           test is needed for generated routines in IL lowering, since they're
           not on the scope stack. */
      } else {
        /* We may be able to dispose of the memory region now. */
        a_routine_ptr rout = scope->variant.routine.ptr;
        check_for_done_with_memory_region(rout->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* needed_flag_walk_entry_process */


void mark_as_needed(char             *entry_ptr,
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
  entry_process_func = needed_flag_walk_entry_process;
  string_entry_process_func = NULL;
  walk_termination_test_func = prune_needed_flag_il_walk;
  walk_remap_func = NULL;
  /* walking_file_scope need not be set. */

  /* Walk the IL tree. */
  walk_tree_and_set_needed(entry_ptr, entry_kind);

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
}  /* mark_as_needed */


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


static a_boolean prune_keep_in_il_walk(char             *entry_ptr,
                                       an_il_entry_kind entry_kind)
/*
Termination-test routine for the IL walk used to set the "keep_in_il" flag in
a tree of IL entries.  Returns TRUE to indicate that the IL walk should be
pruned at the given entry, because the entry has already been marked
to be kept.
*/
{
  a_boolean prune = FALSE;

  if (il_entry_prefix_of(entry_ptr).keep_in_il) {
    /* The flag is set already, so prune the walk at this entry.  */
    prune = TRUE;
  } else {
    /* The flag is not set, so set it and keep walking. */
    il_entry_prefix_of(entry_ptr).keep_in_il = TRUE;
    /* If this is an entry that might be redeclared or redefined later,
       do not walk its subtree now. */
    if (should_not_walk_subtree(entry_ptr, entry_kind)) prune = TRUE;
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

  /* Save the state of global variables for later restoration. */
  save_il_walk_state(saved_state);
  /* Set up for this walk. */
  entry_process_func = NULL;
  string_entry_process_func = NULL;
  walk_termination_test_func = prune_keep_in_il_walk;
  walk_remap_func = NULL;
  /* walking_file_scope need not be set. */

  /* Walk the IL tree. */
  walk_tree_and_set_keep_in_il(entry_ptr, entry_kind);

  if (entry_kind == iek_scope) {
    a_scope_ptr scope = (a_scope_ptr)entry_ptr;
    if (scope->kind == (a_scope_kind)sck_file) {
      /* The file scope is being walked. */
      /* Walk the orphaned list for scopes, marking only those entries that
         correspond to needed routines.  The process that eliminates unneeded
         IL entries looks at the lists for unneeded routines later, and
         will try to remove entries from those lists.  If each list is
         entirely removed, the header itself is removed, in which case
         the reference from the header to the associated routine should
         not cause the setting of keep_in_il on the routine entry itself.
         So we don't do that here, leaving it to be done later if
         appropriate. */
      a_scope_orphaned_list_header_ptr *ptr_ptr =
                                        &il_header.scope_orphaned_list_headers;
      check_assertion(end_of_file_scope_needed_flags_phase);
      for (; *ptr_ptr != NULL; ptr_ptr = &(*ptr_ptr)->next) {
        if ((*ptr_ptr)->assoc_routine->source_corresp.needed) {
          walk_ptr(*ptr_ptr, a_scope_orphaned_list_header_ptr,
                   iek_scope_orphaned_list_header);
        }  /* if */
      }  /* for */
      /* Visit the orphan lists. */
      walk_orphaned_entries_set_keep_in_il();
    } /* if */
  }  /* if */

  /* Restore the state of global variables. */
  restore_il_walk_state(saved_state);
}  /* mark_to_keep_in_il */

#if GENERATE_SOURCE_SEQUENCE_LISTS

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
  a_source_sequence_entry_ptr ssep;
  a_boolean                   keep_in_il;

  for (ssep = sslist; ssep != NULL; ssep = ssep->next) {
    if (function_local) {
      /* Keep all function-local source sequence entries. */
      keep_in_il = TRUE;
    } else {
      char *entry_ptr;
      if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
        /* This is a secondary declaration. */
        a_src_seq_secondary_decl_ptr sec_decl =
                              ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        entry_ptr = sec_decl->entity.ptr;
      } else {
        /* This is a primary declaration. */
        entry_ptr = ssep->entity.ptr;
      }  /* if */
      /* See if the associated IL entity is marked with keep_in_il. */
      keep_in_il = il_entry_prefix_of(entry_ptr).keep_in_il;
    }  /* if */
    /* Mark the source sequence entry the right way (and the secondary
       declaration entry too, if there is one). */
    if (keep_in_il) {
      walk_ptr(ssep, a_source_sequence_entry, iek_source_sequence_entry);
    }  /* if */
  }  /* for */
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

#endif /* MAINTAIN_NEEDED_FLAGS */

/*
Macro to remap an orphan IL entry pointer from an "old" value to a "new"
value.  This macro is similar to remap_ptr, but all pointers are processed
as (char *),  ptr is the pointer, and entry_kind is the kind of entry
pointed to.
*/
#define remap_orphan_ptr(ptr, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (char *)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_orphan_ptr */

#if REMAP_ONLY_ROUTINES_NEEDED

void remap_first_ptr_of_orphaned_file_scope_entry_array(void)
/*
Remap the "first" pointers in the orphaned_file_scope_il_entries array by
running them through walk_remap_func.
*/
{
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
  remap_orphan_entry_first(iek_using_directive);
  remap_orphan_entry_first(iek_dynamic_init);
  remap_orphan_entry_first(iek_local_static_variable_init);
  remap_orphan_entry_first(iek_class_member_using_decl);
  remap_orphan_entry_first(iek_overriding_virtual_function);
  remap_orphan_entry_first(iek_derivation_step);
  remap_orphan_entry_first(iek_base_class_derivation);
  remap_orphan_entry_first(iek_base_class);
  remap_orphan_entry_first(iek_class_list_entry);
  remap_orphan_entry_first(iek_routine_list_entry);
  remap_orphan_entry_first(iek_class_type_supplement);
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
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_comment, iek_src_seq_secondary_decl, and
     iek_src_seq_end_of_construct, since such entries will never appear on an
     orphan list.  Ditto for iek_src_seq_sublist. */
  /* Nothing needed for iek_comment, iek_scope_orphaned_list_header,
     iek_hidden_name, iek_pragma, iek_template, and iek_macro. */
#undef remap_orphan_entry_first
}  /* remap_first_ptr_of_orphaned_file_scope_entry_array */

#endif /* REMAP_ONLY_ROUTINES_NEEDED */

void remap_last_ptr_of_orphaned_file_scope_entry_array(void)
/*
Remap the "last" pointers in the orphaned_file_scope_il_entries array by
running them through walk_remap_func.
*/
{
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
  remap_orphan_entry_last(iek_using_directive);
  remap_orphan_entry_last(iek_dynamic_init);
  remap_orphan_entry_last(iek_local_static_variable_init);
  remap_orphan_entry_last(iek_class_member_using_decl);
  remap_orphan_entry_last(iek_overriding_virtual_function);
  remap_orphan_entry_last(iek_derivation_step);
  remap_orphan_entry_last(iek_base_class_derivation);
  remap_orphan_entry_last(iek_base_class);
  remap_orphan_entry_last(iek_class_list_entry);
  remap_orphan_entry_last(iek_routine_list_entry);
  remap_orphan_entry_last(iek_class_type_supplement);
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
  /* Note that nothing is needed for iek_source_sequence_entry nor for its
     subordinate entries like iek_comment, iek_src_seq_secondary_decl, and
     iek_src_seq_end_of_construct, since such entries will never appear on an
     orphan list. */
  /* Nothing needed for iek_comment, iek_scope_orphaned_list_header,
     iek_hidden_name, iek_pragma, iek_template, and iek_macro. */
#undef remap_orphan_entry_last
}  /* remap_last_ptr_of_orphaned_file_scope_entry_array */


/* If necessary, build a routine to walk entries in isolation (i.e.,
   not as part of a tree walk).  This is built from the walk_entry.h
   source using special macro settings. */
#if REMAP_ONLY_ROUTINES_NEEDED
#undef DO_SUBTREE_WALK
#define DO_SUBTREE_WALK FALSE
#undef NEEDED_FLAG_WALK
#define NEEDED_FLAG_WALK FALSE
#undef KEEP_IN_IL_WALK
#define KEEP_IN_IL_WALK FALSE
#undef WALK_ENTRY_ROUTINE_STATIC
#define WALK_ENTRY_ROUTINE_STATIC /* extern */
#undef WALK_ENTRY_ROUTINE_NAME
#define WALK_ENTRY_ROUTINE_NAME remap_pointers_in_il_entry
#undef WALK_ORPHANED_ENTRY_ROUTINE_NAME
#undef UNDEF_WALK_ENTRY_MACROS_AT_END
#define UNDEF_WALK_ENTRY_MACROS_AT_END
#include "walk_entry.h"
#endif /* REMAP_ONLY_ROUTINES_NEEDED */

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
  walk_remap_func = NULL;
#if MAINTAIN_NEEDED_FLAGS
  end_of_file_scope_needed_flags_phase = FALSE;
#endif /* MAINTAIN_NEEDED_FLAGS */
  /* Variables in il_walk.c: */
  entry_process_func = NULL;
  string_entry_process_func = NULL;
  walk_termination_test_func = NULL;
  walking_file_scope = FALSE;
  flag_value_meaning_visited = 0;
}  /* il_walk_init */

#endif /* IL_WALK_NEEDED */

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

#endif /* IL_WALK_NEEDED || NEED_DECLARATIVE_WALK */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
