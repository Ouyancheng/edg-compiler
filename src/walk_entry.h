/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

walk_entry.h -- Routines used by il_walk.c to walk IL entries.

Placed in a separate file so they can be expanded several ways:

1)  With DO_SUBTREE_WALK TRUE, the routines walk not only the entry itself
    but also its subtree.

2)  With DO_SUBTREE_WALK and NEEDED_FLAG_WALK TRUE, the routines walk
    the subtree and do so in the special way required for setting the
    needed flag.  Entries related to the following are not walked
    (because they do not affect whether the entities are needed to
    create an executable program):
      -- Access control (including friendship)
      -- "using" declarations and directives
      -- The hidden name table
      -- Source sequence entries
      -- The based types list
    Also, some back-pointers are not walked if they introduce cycles
    in the data structure where no entry in the cycle has a "needed"
    flag, since such cycles would cause recursion loops in this walk.

3)  With DO_SUBTREE_WALK and KEEP_IN_IL_WALK TRUE, the routines walk
    the subtree and do so in the special way required for setting the
    keep_in_il flag.

4)  With DO_SUBTREE_WALK FALSE, the routines walk just the entry itself.
    This is used for remapping of pointers.
  
*/

/*
Macro to remap a pointer from an "old" value to a "new" value.  ptr is
the pointer, ptr_type the type of ptr, and entry_kind is the kind of entry
pointed to.  For the NEEDED_FLAG_WALK and KEEP_IN_IL_WALK cases, just
walks the pointer.

This macro is used for secondary references to IL entities, e.g., a
reference from executable code to a declarative entry, where it is known
that the declarative entry will be reached from a primary pointer while
walking the declarative structure.  When in doubt, a walk_ptr is the safer
choice; it may be unnecessarily slow, but it will work correctly even if
the entry is not reached from elsewhere.
*/
#undef remap_ptr
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* When doing the IL walk to set the "needed" or "keep_in_il" flags, all
   references are significant and must be followed. */
#define remap_ptr(ptr, ptr_type, entry_kind) \
  walk_ptr(ptr, ptr_type, entry_kind)
#else /* !(NEEDED_FLAG_WALK && ...) */
#define remap_ptr(ptr, ptr_type, entry_kind) \
{ if (walk_remap_func != NULL) { \
    (ptr) = (ptr_type)walk_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_ptr */
#endif /* NEEDED_FLAG_WALK && ... */

/*
Like remap_ptr, but used for pointers in lists, i.e., "next" pointers
and start-of-list pointers.
*/
#undef remap_list_ptr
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* When doing the IL walk to set the "needed" or "keep_in_il" flags, all
   references are significant and must be followed. */
#define remap_list_ptr(ptr, ptr_type, entry_kind) \
  walk_list_ptr(ptr, ptr_type, entry_kind)
#else /* !(NEEDED_FLAG_WALK && ...) */
#define remap_list_ptr(ptr, ptr_type, entry_kind) \
{ if (walk_list_remap_func != NULL) { \
    (ptr) = (ptr_type)walk_list_remap_func((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}  /* remap_list_ptr */
#endif /* NEEDED_FLAG_WALK && ... */

/*
Like remap_ptr, but used for "next" pointers in entries.  These are
remapped only if not processing subtrees (if subtrees are being processed,
walk_list handles the remapping when doing the parent of this entry).
*/
#undef remap_next_ptr
#if DO_SUBTREE_WALK
#define remap_next_ptr(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !DO_SUBTREE_WALK */
#define remap_next_ptr(ptr, ptr_type, entry_kind) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Similar to remap_ptr, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef remap_ptr_not_needed
#if NEEDED_FLAG_WALK
#define remap_ptr_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define remap_ptr_not_needed(ptr, ptr_type, entry_kind) \
  remap_ptr(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Macro to remap a pointer to its new value, walk the subtree of the pointer
(if appropriate), and process the entry pointed to.  ptr is the pointer,
ptr_type is the type of ptr, and entry_kind is the kind of entry pointed to.
*/
#undef walk_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK
/* Walking to set the "needed" flag. */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ if ((ptr) != NULL) walk_tree_and_set_needed((char *)(ptr), (entry_kind)); }
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
/* Walking to set the "keep_in_il" flag. */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ if ((ptr) != NULL) { \
    walk_tree_and_set_keep_in_il((char *)(ptr), (entry_kind)); \
  }  /* if */ \
}
#else /* !KEEP_IN_IL_WALK */
#define walk_ptr(ptr, ptr_type, entry_kind) \
{ remap_ptr((ptr), ptr_type, (entry_kind)); \
  if ((ptr) != NULL) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_ptr */
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */
#else /* !DO_SUBTREE_WALK */
#define walk_ptr(ptr, ptr_type, entry_kind) \
  remap_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Like walk_ptr, but used for pointers in lists, i.e., "next" pointers
and start-of-list pointers.
*/
#undef walk_list_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
/* Walking to set the "needed" or "keep_in_il" flag.  Same as walk_ptr. */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
  walk_ptr((ptr), ptr_type, (entry_kind))
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
{ remap_list_ptr((ptr), ptr_type, (entry_kind)); \
  if ((ptr) != NULL) walk_entry_and_subtree((char *)(ptr), (entry_kind)); \
}  /* walk_list_ptr */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#else /* !DO_SUBTREE_WALK */
#define walk_list_ptr(ptr, ptr_type, entry_kind) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Similar to walk_ptr, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef walk_ptr_not_needed
#if NEEDED_FLAG_WALK
#define walk_ptr_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define walk_ptr_not_needed(ptr, ptr_type, entry_kind) \
  walk_ptr(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Macro similar to walk_ptr, but used for string entries.  ptr is the pointer
to the entry, entry_kind is the kind of entry pointed to, and entry_length is
the string length for iek_string_text entries, unused otherwise.
*/
#undef walk_string_ptr
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define walk_string_ptr(ptr, entry_kind, entry_length) /* Nothing. */
#else /* !(NEEDED_FLAG_WALK && ...) */
#define walk_string_ptr(ptr, entry_kind, entry_length) \
{ remap_ptr((ptr), a_char_ptr, (entry_kind)); \
  walk_string_entry((char *)(ptr), (entry_kind), (sizeof_t)(entry_length)); \
}  /* walk_string_ptr */
#endif /* NEEDED_FLAG_WALK && ... */
#else /* !DO_SUBTREE_WALK */
#define walk_string_ptr(ptr, entry_kind, entry_length) \
  remap_ptr((ptr), a_char_ptr, (entry_kind))
#endif /* DO_SUBTREE_WALK */

/*
Process a list, each entry linked to the next by the "next" field.
ptr is the pointer to the list, ptr_type is the type of ptr, and entry_kind
is the kind of entries on the list.  If walking subtrees, each entry is
processed; if not, ptr is remapped but the list is not traversed.
walk_list_on_link_field can be used when the link field is called something
other than "next".
*/
#undef walk_list_on_link_field
#if DO_SUBTREE_WALK
#define walk_list_on_link_field(ptr, ptr_type, entry_kind, link_field) \
{ ptr_type *ptr_ptr = &(ptr); \
  for (; *ptr_ptr != NULL; ptr_ptr = &(*ptr_ptr)->link_field) { \
    walk_list_ptr(*ptr_ptr, ptr_type, (entry_kind)); \
  }  /* for */ \
}  /* walk_list_on_link_field */
#else /* !DO_SUBTREE_WALK */
#define walk_list_on_link_field(ptr, ptr_type, entry_kind, link_field) \
  remap_list_ptr((ptr), ptr_type, (entry_kind))
#endif /* DO_SUBTREE_WALK */
#undef walk_list
#define walk_list(ptr, ptr_type, entry_kind) \
  walk_list_on_link_field(ptr, ptr_type, entry_kind, next)

/*
Similar to walk_list, but expands to nothing in the NEEDED_FLAG_WALK mode.
*/
#undef walk_list_not_needed
#if NEEDED_FLAG_WALK
#define walk_list_not_needed(ptr, ptr_type, entry_kind) /* Nothing */
#else /* !NEEDED_FLAG_WALK */
#define walk_list_not_needed(ptr, ptr_type, entry_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* NEEDED_FLAG_WALK */

/*
Similar to walk_list, but used to walk lists attached to a scope.
scope_kind indicates the scope kind.  When KEEP_IN_IL_WALK is TRUE,
expands to code that acts differently for certain kinds of scopes:
  -- For the file scope and namespace scopes, it walks the list but
     calls walk_ptr only on those entries with the "needed" flag TRUE
     (and on those, it clears the keep_in_il flag before doing the walk,
     to deal with entities that can be redeclared and whose subtrees
     can therefore change).
  -- For class scopes, every entry gets keep_in_il set (because classes
     are kept or removed in their entirety).  keep_in_il is cleared
     and set again, as above, because of changing subtrees.
  -- For other scopes, it does a normal walk_list.
In NEEDED_FLAG_WALK mode, expands to nothing.  In other modes, expands to a
simple walk_list.
*/
#undef walk_needed_on_list
#if NEEDED_FLAG_WALK
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind)/* Nothing */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind) \
{ if ((scope_kind) == (a_scope_kind)sck_file || \
      (scope_kind) == (a_scope_kind)sck_namespace || \
      (scope_kind) == (a_scope_kind)sck_class_struct_union) { \
    ptr_type local_ptr = (ptr); \
    for (; local_ptr != NULL; local_ptr = local_ptr->next) { \
      if ((scope_kind) == (a_scope_kind)sck_class_struct_union || \
          needed_flag_is_set(&local_ptr->source_corresp) || \
          il_entry_prefix_of(local_ptr).keep_in_il) { \
        clear_keep_in_il_to_allow_subtree_walk((char *)local_ptr, entry_kind);\
        walk_list_ptr(local_ptr, ptr_type, (entry_kind)); \
      }  /* if */ \
    }  /* for */ \
  } else { \
    walk_list(ptr, ptr_type, entry_kind); \
  }  /* if */ \
}  /* walk_needed_on_list */
#else /* !KEEP_IN_IL_WALK */
#define walk_needed_on_list(ptr, ptr_type, entry_kind, scope_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */

/*
Similar to walk_list, but when KEEP_IN_IL_WALK is TRUE, does a
special walk that clears the keep_in_il flag and resets it, to
ensure that subtrees are walked.  In NEEDED_FLAG_WALK mode, expands
to nothing.  In other modes, expands to a simple walk_list.
*/
#undef walk_list_with_keep_in_il_reset
#if NEEDED_FLAG_WALK
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind)/* Nothing */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind) \
{ ptr_type local_ptr = (ptr); \
  for (; local_ptr != NULL; local_ptr = local_ptr->next) { \
    clear_keep_in_il_to_allow_subtree_walk((char *)local_ptr, entry_kind); \
    walk_list_ptr(local_ptr, ptr_type, (entry_kind)); \
  }  /* for */ \
}  /* walk_list_with_keep_in_il_reset */
#else /* !KEEP_IN_IL_WALK */
#define walk_list_with_keep_in_il_reset(ptr, ptr_type, entry_kind) \
  walk_list(ptr, ptr_type, entry_kind)
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */

/*
Set the definition_needed or keep_definition_in_il flag in a type if the
type is a class type.  Used to indicate cases that require the full type
of a class rather than just a declaration.
set_proper_definition_needed_flag sets either definition_needed or
keep_definition_in_il, or does nothing, depending on the configuration.
If prototype instantiations are recorded in the IL, they are marked along with
the real instantiations that the template generated.
*/
#undef definition_needed_if_class
#undef set_proper_definition_needed_flag
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#if NEEDED_FLAG_WALK
#define set_proper_definition_needed_flag(ptr)                               \
  set_class_definition_needed(ptr);
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
#define set_proper_definition_needed_flag(ptr)                               \
  set_class_keep_definition_in_il(ptr);
#endif /* NEEDED_FLAG_WALK */
#define definition_needed_if_class(ptr) \
{ a_type_ptr local_ptr = skip_typerefs(ptr); \
  if (local_ptr->kind == (a_type_kind)tk_array) { \
    local_ptr = underlying_array_element_type(local_ptr); \
    local_ptr = skip_typerefs(local_ptr); \
  }  /* if */ \
  if (is_immediate_class_type(local_ptr) && (local_ptr)->size != 0) { \
    set_proper_definition_needed_flag(local_ptr); \
  }  /* if */ \
}  /* definition_needed_if_class */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define definition_needed_if_class(ptr) /* Nothing */
#define set_proper_definition_needed_flag(ptr) /* Nothing */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Set the definition_needed or keep_definition_in_il flag in a routine.
*/
#undef set_proper_routine_definition_needed_flag
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#if NEEDED_FLAG_WALK
#define set_proper_routine_definition_needed_flag(ptr)   \
  set_routine_definition_needed(ptr);
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
#define set_proper_routine_definition_needed_flag(ptr)       \
  set_routine_keep_definition_in_il(ptr);
#endif /* NEEDED_FLAG_WALK */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
#define set_proper_routine_definition_needed_flag(ptr) /* Nothing */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */

/*
Process the source correspondence field pointed to by ptr.
*/
/* Macro to remap class or namespace parent only if it exists. */
#undef remap_parent
#ifdef CFE
#define remap_parent(ptr) \
{ if ((ptr).is_class_member) {  \
    remap_ptr((ptr).parent.class_type, a_type_ptr, iek_type);  \
    set_proper_definition_needed_flag((ptr).parent.class_type); \
  } else {  \
    remap_ptr((ptr).parent.namespace_ptr, a_namespace_ptr, iek_namespace); \
  }  /* if */  \
}  /* remap_parent */
#else /* !defined(CFE) */
#define remap_parent(ptr) /* Nothing */
#endif /* ifdef CFE */

/*
Clear a front end pointer (to avoid passing it to the next phase) if
necessary.
*/
#undef conditionally_clear_fe_pointer
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
#define conditionally_clear_fe_pointer(ptr) /* Nothing */
#else /* !(NEEDED_FLAG_WALK && ...) */
#define conditionally_clear_fe_pointer(ptr) \
{ if (clear_fe_pointers_during_walk) (ptr) = NULL; }
#endif /* NEEDED_FLAG_WALK && ... */

#undef remap_source_sequence_entry
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
#define remap_source_sequence_entry(ptr) \
  remap_ptr((ptr).source_sequence_entry, a_source_sequence_entry_ptr, \
            iek_source_sequence_entry)
#else /* !(GENERATE_SOURCE_SEQUENCE_LISTS && ...) */
#define remap_source_sequence_entry(ptr) /* Nothing */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS & ... */

#undef walk_name_reference_list
#if RECORD_FORM_OF_NAME_REFERENCE
#define walk_name_reference_list(ptr) \
  walk_list((ptr).name_references, a_name_reference_ptr, iek_name_reference)
#else /* !RECORD_FORM_OF_NAME_REFERENCE */
#define walk_name_reference_list(ptr) /* Nothing */  
#endif /* RECORD_FORM_OF_NAME_REFERENCE */

#undef walk_source_corresp
#if NEEDED_FLAG_WALK
#define walk_source_corresp(ptr) \
{ \
  remap_parent(ptr); \
  walk_name_reference_list(ptr); \
}  /* walk_source_corresp */
#else /* !NEEDED_FLAG_WALK */
#undef walk_unmangled_name
#if NEED_NAME_MANGLING
#define walk_unmangled_name(ptr) \
  walk_string_ptr((ptr).unmangled_name, iek_id_name, 0)
#else /* !NEED_NAME_MANGLING */
#define walk_unmangled_name(ptr) /* Nothing */
#endif /* NEED_NAME_MANGLING */
#undef walk_per_instantiation_needed_flags
#if ONE_INSTANTIATION_PER_OBJECT
#define walk_per_instantiation_needed_flags(ptr) \
  walk_list((ptr).per_instantiation_needed_flags, \
            a_per_instantiation_needed_flags_entry_ptr, \
            iek_per_instantiation_needed_flags_entry)
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define walk_per_instantiation_needed_flags(ptr) /* Nothing */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#undef walk_decl_position_supplement
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define walk_decl_position_supplement(ptr) \
  walk_ptr((ptr).decl_pos_info, a_decl_position_supplement_ptr, \
           iek_decl_position_supplement)
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define walk_decl_position_supplement(ptr) /* Nothing */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#define walk_source_corresp(ptr) \
{ walk_string_ptr((ptr).name, iek_id_name, 0); \
  walk_unmangled_name(ptr); \
  conditionally_clear_fe_pointer((ptr).trans_unit_corresp); \
  remap_parent(ptr); \
  remap_source_sequence_entry(ptr); \
  conditionally_clear_fe_pointer((ptr).assoc_info); \
  walk_per_instantiation_needed_flags(ptr); \
  walk_decl_position_supplement(ptr); \
  walk_name_reference_list(ptr); \
}  /* walk_source_corresp */
#endif /* NEEDED_FLAG_WALK */

#undef report_bad_init_kind
#if CHECKING
#define report_bad_init_kind()                                        \
  internal_error("walk_entry_and_subtree: bad init kind")
#else /* !CHECKING */
#define report_bad_init_kind()  /* Nothing */
#endif /* CHECKING */

#undef walk_initializer
#define walk_initializer(init_kind, initializer)                      \
{ switch (init_kind) {                                                \
    case initk_none:                                                  \
    case initk_zero:                                                  \
    case initk_function_local:                                        \
      /* No pointers. */                                              \
      break;                                                          \
    case initk_static:                                                \
      walk_ptr((initializer).constant, a_constant_ptr, iek_constant); \
      break;                                                          \
    case initk_dynamic:                                               \
      walk_ptr((initializer).dynamic, a_dynamic_init_ptr,             \
               iek_dynamic_init);                                     \
      break;                                                          \
    default:                                                          \
      report_bad_init_kind();                                         \
  }  /* switch */                                                     \
}  /* walk_initializer */


/* The name is provided by a macro so it can be different things, e.g.,
    walk_entry_and_subtree and remap_pointers_in_entry. */
WALK_ENTRY_ROUTINE_STATIC /* "static" if routine should be static. */
void WALK_ENTRY_ROUTINE_NAME(char             *entry_ptr,
                             an_il_entry_kind entry_kind)
/*
Process the entry at entry_ptr (which is of kind indicated by entry_kind)
by remapping its pointers, and, if DO_SUBTREE_WALK is TRUE, walking its
subtree and calling the entry_process_func.  When DO_SUBTREE_WALK is
TRUE, if the entry has already been seen, or if the pointer crosses into
the file scope, do not process it (but record an orphan in the latter case).
*/
{
#if DO_SUBTREE_WALK
  /* Do a termination test (to prune the walk) if walking subtrees. */
  /* See if there is a termination-test function provided by the caller.
     If so, call it to see if we just return on encountering this entry. */
  if (walk_termination_test_func != NULL) {
    if (walk_termination_test_func(entry_ptr, entry_kind)) {
      goto end_of_routine;
    }  /* if */
  } else {
    /* The default termination test (a) stops on crossing from a function
       scope memory region into the file scope memory region (recording
       an orphan on the entry in the file scope memory region), and
       (b) sets the il_walk_flag in the entry prefix to mark the entries
       that have been seen already, stopping on encountering one already
       marked. */
    /* This default termination test cannot be used when walking
       secondary translation units, because the IL for those is intermixed.
       If we were to record an orphan, we might do so in the wrong translation
       unit. */
    an_il_entry_prefix_ptr epp = &il_entry_prefix_of(entry_ptr);
    /* If we are walking through a function scope, and the entry here is
       in the file scope, just return. */
    if (!walking_file_scope && epp->file_scope) {
      /* Add non-string file scope IL entries referenced from a
         function scope to the orphaned IL entries lists. */
      possibly_add_orphaned_file_scope_il_entry(entry_ptr, entry_kind);
      goto end_of_routine;
    }  /* if */
    /* See if this entry has been reached already, and if so, don't process
       it or its subtree.  This is indicated by the il_walk_flag field of the
       entry prefix. */
    if (epp->il_walk_flag == flag_value_meaning_visited) {
      /* Entry has already been visited. */
      goto end_of_routine;
    }  /* if */
    /* Set the flag to indicate that this entry has been visited. */
    epp->il_walk_flag = flag_value_meaning_visited;
  }  /* if */
#endif /* DO_SUBTREE_WALK */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "Walking IL tree, entry kind = %s\n",
                     il_entry_kind_names[(int)entry_kind]);
  }  /* if */
#endif /* DEBUG */
  /* For each pointer in the entry, remap it and walk the subtree.
     In general, linked lists are traversed while processing the entry
     that contains the head-of-list pointer.  This is done to avoid
     using recursion to process very long lists (the stack space 
     requirements could be ridiculous).  It also means the "next"
     pointers should not be processed or remapped during the processing
     of the entries that contain them, because they've already been
     handled.  Of course if the subtrees are not being walked
     the "next" fields must be processed as they are encountered. */
  switch (entry_kind) {
    case iek_source_file:
      {
        a_source_file_ptr ptr = (a_source_file_ptr)entry_ptr;
        walk_string_ptr(ptr->file_name, iek_other_text, 0);
        walk_string_ptr(ptr->full_name, iek_other_text, 0);
        walk_string_ptr(ptr->name_as_written, iek_other_text, 0);
        walk_list(ptr->first_child_file, a_source_file_ptr, iek_source_file);
        remap_ptr(ptr->last_child_file, a_source_file_ptr, iek_source_file);
        remap_next_ptr(ptr->next, a_source_file_ptr, iek_source_file);
      }
      break;
    case iek_constant:
      {
        a_constant_ptr ptr = (a_constant_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_constant_ptr, iek_constant);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
        walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        if (ptr->type != NULL) {
          definition_needed_if_class(ptr->type);
        }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        switch (ptr->kind) {
          case ck_error:
          case ck_integer:
#if UPC_EXTENSIONS_ALLOWED
          case ck_upc_threads:
          case ck_upc_mythread:
#endif /* UPC_EXTENSIONS_ALLOWED */
          case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
          case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            /* No pointers. */
            break;
          case ck_string:
            walk_string_ptr(ptr->variant.string.value, iek_string_text,
                            ptr->variant.string.length);
            break;
#if defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED
          case ck_complex:
            walk_ptr(ptr->variant.complex_value, an_internal_complex_value_ptr,
                     iek_internal_complex_value);
            break;
#endif /* defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED */
#ifdef CFE
          case ck_address:
            switch (ptr->variant.address.kind) {
              case abk_routine:
                /* Routines will be visited from the scope. */
                remap_ptr(ptr->variant.address.variant.routine, a_routine_ptr,
                          iek_routine);
                set_proper_routine_definition_needed_flag(
                                         ptr->variant.address.variant.routine);
                break;
              case abk_variable:
                /* Variables will be visited from the scope. */
                remap_ptr(ptr->variant.address.variant.variable,
                          a_variable_ptr, iek_variable);
                break;
              case abk_constant:
                /* Constants might not be on the scope constant list, so visit
                   their subtrees. */
                walk_ptr(ptr->variant.address.variant.constant, a_constant_ptr,
                         iek_constant);
                break;
              case abk_uuidof:
                /* Class types will be visited from the scope. */
                remap_ptr(ptr->variant.address.variant.type, a_type_ptr,
                          iek_type);
                break;
              case abk_label:
                /* Labels will be visited from the scope. */
                remap_ptr(ptr->variant.address.variant.label, a_label_ptr,
                          iek_label);
                break;
              default:
                unexpected_condition_str(
                             "walk_entry_and_subtree: bad address const kind");
            }  /* switch */
            break;
          case ck_ptr_to_member:
            remap_ptr(ptr->variant.ptr_to_member.casting_base_class,
                      a_base_class_ptr, iek_base_class);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            set_proper_definition_needed_flag(pm_class_type(ptr->type));
            if (ptr->variant.ptr_to_member.cast_to_base) {
              /* On a cast from a derived to a base class, mark the derived
                 class's definition as needed. */
              set_proper_definition_needed_flag(
                 ptr->variant.ptr_to_member.casting_base_class->derived_class);
            }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            if (ptr->variant.ptr_to_member.is_function_ptr) {
              remap_ptr(ptr->variant.ptr_to_member.variant.routine,
                        a_routine_ptr, iek_routine);
              if (ptr->variant.ptr_to_member.variant.routine != NULL) {
                set_proper_routine_definition_needed_flag(
                                   ptr->variant.ptr_to_member.variant.routine);
              }  /* if */
            } else {
              remap_ptr(ptr->variant.ptr_to_member.variant.field, a_field_ptr,
                        iek_field);
            }  /* if */
            break;
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
          case ck_stack_offset:
            remap_ptr(ptr->variant.stack_offset.variable, a_variable_ptr,
                      iek_variable);
            break;
#endif /* DO_IL_LOWERING && ... */
          case ck_dynamic_init:
              walk_ptr(ptr->variant.dynamic_init, a_dynamic_init_ptr,
                       iek_dynamic_init);
            break;
#endif /* ifdef CFE */
          case ck_aggregate:
            walk_list(ptr->variant.aggregate.first_constant, a_constant_ptr,
                      iek_constant);
            remap_ptr(ptr->variant.aggregate.last_constant, a_constant_ptr,
                      iek_constant);
            break;
          case ck_init_repeat:
            walk_ptr(ptr->variant.init_repeat.constant, a_constant_ptr,
                     iek_constant);
            break;
          case ck_designator:
            remap_ptr(ptr->variant.designator.field, a_field_ptr, iek_field);
            break;
#ifdef FFE
          case ck_init_position:
            break;
#endif /* ifdef FFE */
          case ck_template_param:
            switch (ptr->variant.template_param.kind) {
              case tpck_param:
              case tpck_member:
                /* No action required. */
                break;
              case tpck_expression:
                walk_ptr(ptr->variant.template_param.variant.expr,
                         an_expr_node_ptr, iek_expr_node);
                break;
              case tpck_unknown_function:
                walk_ptr(ptr->variant.template_param.variant.unknown_function.
                                                               conversion_type,
                         a_type_ptr, iek_type);
                conditionally_clear_fe_pointer(ptr->variant.template_param.
                                              variant.unknown_function.symbol);
                break;
              case tpck_cast:
              case tpck_address:
                walk_ptr(ptr->variant.template_param.variant.constant,
                         a_constant_ptr, iek_constant);
                break;
              case tpck_sizeof:
              case tpck_alignof:
              case tpck_uuidof:
                walk_ptr(ptr->variant.template_param.variant.templ_sizeof.type,
                         a_type_ptr, iek_type);
                walk_ptr(ptr->variant.template_param.variant.templ_sizeof.expr,
                         an_expr_node_ptr, iek_expr_node);
                break;
              case tpck_template_ref:
                walk_ptr(ptr->variant.template_param.variant.template_ref.con,
                         a_constant_ptr, iek_constant);
                walk_list(ptr->variant.template_param.variant.
                                                         template_ref.arg_list,
                          a_template_arg_ptr, iek_template_arg);
                break;
              default:
                unexpected_condition_str(
                   "walk_entry_and_subtree: bad template param constant kind");
            }  /* switch */
            break;
          default:
            unexpected_condition_str(
                                  "walk_entry_and_subtree: bad constant kind");
        }  /* switch */
        walk_source_corresp(ptr->source_corresp);
      }
      break;
    case iek_param_type:
      {
        a_param_type_ptr ptr = (a_param_type_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_param_type_ptr, iek_param_type);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        walk_ptr(ptr->declared_type, a_type_ptr, iek_type);
        /* The C language doesn't actually require that parameter types
           be complete if the function is not called.  However, some
           C compilers (e.g., gcc) warn on an incomplete parameter
           type, so keep the class definition to avoid such warnings. */
        definition_needed_if_class(ptr->type);
#if RECORD_NAME_IN_PARAM_TYPE_ENTRY
        walk_string_ptr(ptr->name, iek_id_name, 0);
#endif /* RECORD_NAME_IN_PARAM_TYPE_ENTRY */
        walk_ptr(ptr->default_arg_expr, an_expr_node_ptr, iek_expr_node);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(ptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
    case iek_routine_type_supplement:
      {
        a_routine_type_supplement_ptr ptr =
                                      (a_routine_type_supplement_ptr)entry_ptr;
        walk_list(ptr->param_type_list, a_param_type_ptr, iek_param_type);
#ifdef CFE
        remap_ptr(ptr->this_class, a_type_ptr, iek_type);
        walk_ptr(ptr->prototype_scope, a_scope_ptr, iek_scope);
        walk_ptr(ptr->exception_specification, an_exception_specification_ptr,
                 iek_exception_specification);
#endif /* ifdef CFE */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Do not walk the assoc_routine pointer for the needed or keep-in-il
           traversal.  We don't want this to force keeping of the routine
           definition if it's not otherwise needed. */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
        remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
      }
      break;
#if !NEEDED_FLAG_WALK
    case iek_based_type_list_member:
      {
        a_based_type_list_member_ptr ptr =
                                       (a_based_type_list_member_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_based_type_list_member_ptr,
                       iek_based_type_list_member);
        /* Do walk_ptr instead of remap_ptr because the reference might
           be to an entity not otherwise in the IL tree, e.g., a front-end-only
           type. */
        walk_ptr(ptr->based_type, a_type_ptr, iek_type);
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_type:
      {
        a_type_ptr ptr = (a_type_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK
        /* When walking to set "needed" flags, the based types list in
           general is not walked, but if there is a based type entry for the
           unqualified version of an array type, walk it. */
        { a_type_ptr unqual_array_type;
          if (ptr->kind == (a_type_kind)tk_array &&
              is_qualified_version_of_array_typedef(ptr, &unqual_array_type)) {
            walk_ptr(unqual_array_type, a_type_ptr, iek_type);
          }  /* if */
        }
#else /* !NEEDED_FLAG_WALK */
        walk_list(ptr->based_types, a_based_type_list_member_ptr,
                  iek_based_type_list_member);
#endif /* NEEDED_FLAG_WALK */
#if DO_IL_LOWERING
        remap_ptr_not_needed(ptr->typeinfo_var, a_variable_ptr, iek_variable);
#endif /* DO_IL_LOWERING */
        switch (ptr->kind) {
          case tk_error:
          case tk_unknown:
          case tk_void:
          case tk_float:
#if defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED
          case tk_complex:
#endif /* defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED */
#if C99_IL_EXTENSIONS_SUPPORTED
          case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#ifdef FFE
          case tk_fcharacter:
          case tk_hollerith:
          case tk_stmt_label:
          case tk_format:
          case tk_association:
          case tk_unspec_routine:
          case tk_blockdata:
#endif /* ifdef FFE */
            /* No pointers. */
            break;
          case tk_integer:
#ifdef CFE
            if (ptr->variant.integer.enum_type) {
              walk_list(ptr->variant.integer.enum_info.constant_list,
                        a_constant_ptr, iek_constant);
            } else {
              walk_ptr(ptr->variant.integer.enum_info.affiliated_type,
                       a_type_ptr, iek_type);
            }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
            walk_string_ptr(ptr->variant.integer.uuid_string,
                            iek_other_text, 0);
#if DO_IL_LOWERING
            conditionally_clear_fe_pointer(ptr->variant.integer.uuid_variable);
#endif /* DO_IL_LOWERING */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
            break;
          case tk_pointer:
            walk_ptr(ptr->variant.pointer.type, a_type_ptr, iek_type);
#ifdef CFE
#if MICROSOFT_EXTENSIONS_ALLOWED
            remap_ptr(ptr->variant.pointer.base_variable, a_variable_ptr,
                      iek_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
            break;
#ifdef CFE
          case tk_array:
            if (ptr->variant.array.is_variable_size_array &&
                !ptr->variant.array.is_vla) {
              walk_ptr(ptr->variant.array.variant.element_count_expr,
                       an_expr_node_ptr, iek_expr_node);
            } else if (ptr->variant.array.is_template_dependent_size_array) {
              walk_ptr(ptr->variant.array.variant.element_count_constant,
                       a_constant_ptr, iek_constant);
            }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
            walk_ptr(ptr->variant.array.bound_constant,
                     a_constant_ptr, iek_constant);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* In some error cases, some array types on the vla_dimensions
               list are incomplete during the needed flag and keep-in-IL
               walks. */
            if (ptr->variant.array.element_type == NULL) break;
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            walk_ptr(ptr->variant.array.element_type, a_type_ptr, iek_type);
            definition_needed_if_class(ptr->variant.array.element_type);
            break;
          case tk_class:
          case tk_struct:
          case tk_union:
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* The field list is part of the definition and is walked only if
               the definition should be walked. */
            if (
#if NEEDED_FLAG_WALK
                class_definition_needed_flag_is_set(ptr)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
                ptr->variant.class_struct_union.keep_definition_in_il
#endif /* NEEDED_FLAG_WALK */
                                                                     )
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            /* Do not insert code here. */
            {
                walk_list(ptr->variant.class_struct_union.field_list,
                          a_field_ptr, iek_field);
            }  /* if */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* Handle the class type supplement inline, because we need
               to have a pointer to the class to decide whether or not to
               process definition-related fields. */
            if (ptr->variant.class_struct_union.extra_info != NULL) {
              goto handle_class_type_supplement_for_class;
            }  /* if */
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
            walk_ptr(ptr->variant.class_struct_union.extra_info,
                     a_class_type_supplement_ptr, iek_class_type_supplement);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            break;
          case tk_typeref:
            walk_ptr(ptr->variant.typeref.type, a_type_ptr, iek_type);
#if DO_IL_LOWERING
#if KEEP_IN_IL_WALK
            walk_ptr(ptr->variant.typeref.orig_type, a_type_ptr, iek_type);
#else /* !KEEP_IN_IL_WALK */
            conditionally_clear_fe_pointer(ptr->variant.typeref.orig_type);
#endif /* KEEP_IN_IL_WALK */
#endif /* DO_IL_LOWERING */
            break;
          case tk_ptr_to_member:
            remap_ptr(ptr->variant.ptr_to_member.class_of_which_a_member,
                      a_type_ptr, iek_type);
            walk_ptr(ptr->variant.ptr_to_member.type, a_type_ptr, iek_type);
            break;
#endif /* ifdef CFE */
          case tk_routine:
            walk_ptr(ptr->variant.routine.return_type, a_type_ptr, iek_type);
            walk_ptr(ptr->variant.routine.extra_info,
                     a_routine_type_supplement_ptr,
                     iek_routine_type_supplement);
            break;
#ifdef FFE
          case tk_farray:
            walk_ptr(ptr->variant.farray.element_type, a_type_ptr, iek_type);
            remap_ptr(ptr->variant.farray.bound_info, a_bound_info_entry_ptr,
                     iek_bound_info_entry);
#if DO_SUBTREE_WALK
            /* Walk each of the bound info entries; make the index in the array
               available to facilitate writing these entries in the alternate
               file format (the problem is that there is no room for the
               entry number preceding each entry). */
            {
              int save_array_bound_walk_index = array_bound_walk_index;
              int save_num_walk_array_bounds = num_walk_array_bounds;
              a_bound_info_entry_ptr biptr;
              num_walk_array_bounds =
                                  2 * ptr->variant.farray.number_of_dimensions;
              for (array_bound_walk_index = 0,
                                        biptr = ptr->variant.farray.bound_info;
                   array_bound_walk_index < num_walk_array_bounds;
                   array_bound_walk_index++, biptr++) {
                walk_ptr(biptr, a_bound_info_entry_ptr, iek_bound_info_entry);
              }  /* for */
              array_bound_walk_index = save_array_bound_walk_index;
              num_walk_array_bounds = save_num_walk_array_bounds;
            }
#endif /* DO_SUBTREE_WALK */
            break;
#endif /* ifdef FFE */
          case tk_template_param:
            walk_ptr(ptr->variant.template_param.extra_info,
                     a_template_param_type_supplement_ptr,
                     iek_template_param_type_supplement);
            break;
          default:
            unexpected_condition_str("walk_entry_and_subtree: bad type kind");
        }  /* switch */
      }
      break;
    case iek_variable:
      { a_variable_ptr ptr = (a_variable_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_variable_ptr, iek_variable);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(ptr->type);
        remap_ptr_not_needed(ptr->assoc_param_type, a_param_type_ptr,
                             iek_param_type);
        walk_initializer(ptr->init_kind, ptr->initializer);
        remap_ptr(ptr->assoc_template, a_template_ptr, iek_template);
#if GNU_EXTENSIONS_ALLOWED
        if (ptr->asm_name_is_valid) {
          walk_string_ptr(ptr->asm_name_or_reg.name, iek_other_text, 0);
        }  /* if */
        walk_string_ptr(ptr->section, iek_other_text, 0);
        walk_ptr(ptr->aliased_variable, a_variable_ptr, iek_variable);
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_ptr(ptr->declared_type, a_type_ptr, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(ptr->allocate_segname, iek_other_text, 0);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MINIMAL_INLINING
        conditionally_clear_fe_pointer(ptr->remapping_for_inlining);
#endif /* MINIMAL_INLINING */
#ifdef FFE
        remap_ptr(ptr->base_var, a_variable_ptr, iek_variable);
        remap_ptr(ptr->function_result_var_function, a_routine_ptr,
                  iek_routine);
#endif /* ifdef FFE */
      }
      break;
#ifdef CFE
    case iek_field:
      {
        a_field_ptr ptr = (a_field_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_field_ptr, iek_field);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(ptr->type);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
        walk_ptr(ptr->bit_size_constant, a_constant_ptr, iek_constant);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(ptr->get_property_name, iek_other_text, 0);
        walk_string_ptr(ptr->put_property_name, iek_other_text, 0);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }
      break;
    case iek_exception_specification:
      {
        an_exception_specification_ptr ptr =
                             (an_exception_specification_ptr)entry_ptr;
        walk_list(ptr->exception_specification_type_list,
                  an_exception_specification_type_ptr,
                  iek_exception_specification_type);
      }
      break;
    case iek_exception_specification_type:
      {
        an_exception_specification_type_ptr ptr =
                             (an_exception_specification_type_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_exception_specification_type_ptr,
                       iek_exception_specification_type);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* If the exception specification is a class or a pointer to class,
           the class must be complete. */
        { a_type_ptr temp_type = ptr->type;
          if (temp_type != NULL) {
            temp_type = skip_typerefs(temp_type);
            if (temp_type->kind == (a_type_kind)tk_pointer) {
              temp_type = temp_type->variant.pointer.type;
            }  /* if */
            definition_needed_if_class(temp_type);
          }  /* if */
        }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
      }
      break;
#endif /* ifdef CFE */
    case iek_routine:
      {
        a_routine_ptr ptr = (a_routine_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_routine_ptr, iek_routine);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* If the routine is a virtual function with a covariant return
           type, the class type in the return type must be complete. */
        if (ptr->covariant_return_virtual_override) {
          a_type_ptr temp_type = ptr->type;
          temp_type = skip_typerefs(temp_type);
          check_assertion_str2(temp_type->kind == (a_type_kind)tk_routine,
                               "walk_entry_and_subtree:",
                               "type of virtual function is not tk_routine");
          temp_type = temp_type->variant.routine.return_type;
          temp_type = skip_typerefs(temp_type);
          check_assertion_str2(temp_type->kind == (a_type_kind)tk_pointer,
                               "walk_entry_and_subtree:",
                            "return type of covariant virtual is not pointer");
          temp_type = temp_type->variant.pointer.type;
          definition_needed_if_class(temp_type);
        }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* assoc_scope points to a different memory region and is not
           walked automatically.  The entry_process_func can arrange
           to call walk_routine_scope_il if it wants to. */
#ifdef CFE
        walk_list(ptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        remap_ptr(ptr->assoc_template, a_template_ptr, iek_template);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Note that we do not test "defined" here because defined gets cleared
           before some calls to walk the IL. */
        if (ptr->assoc_scope != NULL_region_number) {
          /* This is a defined routine, so its return type must be complete. */
          a_type_ptr rout_type = ptr->type;
          rout_type = skip_typerefs(rout_type);
          definition_needed_if_class(rout_type->variant.routine.return_type);
        }  /* if */
        /* If this is a function generated from a template, we have used a
           template (and we may not be able to accurately remove unneeded
           entities). */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* No processing of befriending_classes for the "needed" sweep. */
#if !NEEDED_FLAG_WALK
#if KEEP_IN_IL_WALK
        /* Visit befriending classes for the "keep_in_il" sweep. */
        set_keep_in_il_on_befriending_classes(ptr->befriending_classes);
#else /* !KEEP_IN_IL_WALK */
        /* All cases except NEEDED_FLAG_WALK and KEEP_IN_IL_WALK. */
        walk_list(ptr->befriending_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
#endif /* KEEP_IN_IL_WALK */
#endif /* !NEEDED_FLAG_WALK */
#endif /* ifdef CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        walk_ptr(ptr->declared_type, a_type_ptr, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
        remap_ptr(ptr->overriding_function_for_covariant_return_type,
                  a_routine_ptr, iek_routine);
        remap_ptr(ptr->overridden_function_for_covariant_return_type,
                  a_routine_ptr, iek_routine);
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if GNU_EXTENSIONS_ALLOWED
        walk_string_ptr(ptr->section, iek_other_text, 0);
        walk_ptr(ptr->aliased_routine, a_routine_ptr, iek_routine);
        if (ptr->aliased_routine != NULL) {
          set_proper_routine_definition_needed_flag(ptr->aliased_routine);
        }  /* if */
        walk_string_ptr(ptr->asm_name, iek_other_text, 0);
#endif /* GNU_EXTENSIONS_ALLOWED */
#ifdef FFE
        walk_ptr(ptr->local_routine_scope, a_scope_ptr, iek_scope);
#endif /* ifdef FFE */
      }
      break;
    case iek_label:
      {
        a_label_ptr ptr = (a_label_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_label_ptr, iek_label);
#ifdef FFE
        switch (ptr->kind) {
          case lk_unknown:
          case lk_specification:
            /* No pointers. */
            break;
          case lk_executable:
          case lk_else_or_elseif:
#endif /* ifdef FFE */
            remap_ptr_not_needed(ptr->variant.exec_stmt, a_statement_ptr,
                                 iek_statement);
#ifdef FFE
            break;
          case lk_format:
            walk_ptr(ptr->variant.format_constant, a_constant_ptr,
                     iek_constant);
            break;
          default:
            unexpected_condition_str("walk_entry_and_subtree: bad label kind");
        }  /* switch */
#endif /* ifdef FFE */
      }
      break;
    case iek_expr_node:
      {
        an_expr_node_ptr ptr = (an_expr_node_ptr)entry_ptr;
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(ptr->type);
        remap_next_ptr(ptr->next, an_expr_node_ptr, iek_expr_node);
#if RECORD_FORM_OF_NAME_REFERENCE
        walk_ptr(ptr->name_reference, a_name_reference_ptr,
                 iek_name_reference);
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
        switch (ptr->kind) {
          case enk_error:
            /* No pointers. */
            break;
          case enk_operation:
            walk_list(ptr->variant.operation.operands, an_expr_node_ptr,
                      iek_expr_node);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* Certain operators on pointers require that the type pointed
               to be complete. */
            { a_type_ptr optype;
              a_type_ptr op1_type = ptr->variant.operation.operands->type;

              switch (ptr->variant.operation.kind) {
                case eok_ppost_incr:
                case eok_ppost_decr:
                case eok_ppre_incr:
                case eok_ppre_decr:
                case eok_padd_assign:
                case eok_psubtract_assign:
                  /* First operand is an lvalue for a pointer. */
                  /* Avoid problems in prototype instantiations. */
                  if (!is_pointer_type(op1_type)) break;
                  optype = type_pointed_to(op1_type);
                  if (!is_pointer_type(optype)) break;
                  optype = type_pointed_to(optype);
                  goto do_definition_needed_if_class;
                case eok_subscript:
                case eok_padd:
                case eok_padd_subsc:
                case eok_psubtract:
                case eok_pdiff:
                  /* First operand is a pointer. */
                  /* Avoid problems in prototype instantiations. */
                  if (!is_pointer_type(op1_type)) break;
                  optype = type_pointed_to(op1_type);
do_definition_needed_if_class:
                  definition_needed_if_class(optype);
                  break;
                case eok_dynamic_cast:
                  /* Destination class (pointed to by result type) must be
                     complete.  Watch out for the case where the result type
                     is "void *", and watch out for prototype instantiation
                     cases. */
                  if (is_pointer_type(ptr->type))
                  {
                    optype = type_pointed_to(ptr->type);
                    definition_needed_if_class(optype);
                  }  /* if */
                  /* Source type must also be complete, but watch out for
                     prototype instantiation cases where the first operand
                     isn't a pointer to class. */
                  if (!is_pointer_type(op1_type) ||
                      !is_class_struct_union_type(type_pointed_to(op1_type))) {
                    break;
                  }  /* if */
                  goto cast_source_type_must_be_pointer_to_complete_class;
                case eok_base_class_cast:
cast_source_type_must_be_pointer_to_complete_class:
                  /* First operand is a pointer to class. */
                  optype = f_skip_typerefs(type_pointed_to(op1_type));
                  goto do_set_proper_definition_needed_flag;
                case eok_derived_class_cast:
                  /* Destination class (pointed to by result type) must be
                     complete. */
                  optype = f_skip_typerefs(type_pointed_to(ptr->type));
                  goto do_set_proper_definition_needed_flag;
                case eok_pm_base_class_cast:
                  /* First operand is a pointer to member. */
                  optype = pm_class_type(op1_type);
                  goto do_set_proper_definition_needed_flag;
                case eok_pm_derived_class_cast:
                  /* Destination class (pointed to by result type) must be
                     complete. */
                  optype = pm_class_type(ptr->type);
do_set_proper_definition_needed_flag:
                  set_proper_definition_needed_flag(optype);
                  break;
                default:
                  break;
              }  /* switch */
            }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            break;
          case enk_constant:
            walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
            break;
          case enk_variable:
          case enk_variable_address:
#ifdef FFE
          case enk_char_variable_length:
#endif /* ifdef FFE */
            /* Variables are handled from the scope that contains them.  Do
               not visit them here. */
            remap_ptr(ptr->variant.variable, a_variable_ptr, iek_variable);
            break;
          case enk_routine_address:
            /* Functions are handled from the scope that contains them.  Do
               not visit them here. */
            remap_ptr(ptr->variant.routine, a_routine_ptr, iek_routine);
            set_proper_routine_definition_needed_flag(ptr->variant.routine);
            break;
#ifdef CFE
          case enk_field:
            /* Fields are handled in processing the tag that contains
               them. */
            remap_ptr(ptr->variant.field, a_field_ptr, iek_field);
            break;
          case enk_temp_init:
            walk_ptr(ptr->variant.init.dynamic_init,
                     a_dynamic_init_ptr, iek_dynamic_init);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            /* If the temp init returns the address of the temporary, mark
               the underlying type as requiring a definition.  When the
               return type is not a temporary, the normal processing on the
               type of the expression will do the marking. */
            if (ptr->variant.init.result_is_addr) {
              a_type_ptr temp_type = type_pointed_to(ptr->type);
              definition_needed_if_class(temp_type);
            }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            break;
          case enk_new_delete:
            walk_ptr(ptr->variant.new_delete, a_new_delete_supplement_ptr,
                     iek_new_delete_supplement);
            break;
          case enk_throw:
            walk_ptr(ptr->variant.throw_info, a_throw_supplement_ptr,
                     iek_throw_supplement);
            break;
          case enk_condition:
            walk_ptr(ptr->variant.condition, a_condition_supplement_ptr,
                     iek_condition_supplement);
            break;
          case enk_object_lifetime:
            walk_ptr(ptr->variant.object_lifetime.expr, an_expr_node_ptr,
                     iek_expr_node);
            remap_ptr_not_needed(ptr->variant.object_lifetime.ptr,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case enk_typeid:
            walk_ptr(ptr->variant.typeid_info.type, a_type_ptr, iek_type);
            definition_needed_if_class(ptr->variant.typeid_info.type);
            walk_ptr(ptr->variant.typeid_info.expr, an_expr_node_ptr,
                     iek_expr_node);
            /* Make sure the definition of type_info is retained, even though
               the node only uses a pointer to it.  This is necessary with
               cp_gen_be output. */
            set_proper_definition_needed_flag(
                                  f_skip_typerefs(type_pointed_to(ptr->type)));
            break;
          case enk_runtime_sizeof:
            if (ptr->variant.runtime_sizeof.is_type) {
              walk_ptr(ptr->variant.runtime_sizeof.variant.type, a_type_ptr,
                       iek_type);
              definition_needed_if_class(
                                     ptr->variant.runtime_sizeof.variant.type);
            } else {
              walk_ptr(ptr->variant.runtime_sizeof.variant.expr,
                       an_expr_node_ptr, iek_expr_node);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
              { a_type_ptr sizeof_type =
                                ptr->variant.runtime_sizeof.variant.expr->type;
                if (ptr->variant.runtime_sizeof.is_lvalue) {
                  /* Watch out for prototype instantiations. */
                  if (!is_pointer_type(sizeof_type)) goto end_sizeof;
                  sizeof_type = type_pointed_to(sizeof_type);
                }  /* if */
                definition_needed_if_class(sizeof_type);
end_sizeof:;
              }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
            }  /* if */
            break;
          case enk_address_of_ellipsis:
            /* No pointers. */
            break;
#if GNU_EXTENSIONS_ALLOWED
          case enk_statement:
            walk_ptr(ptr->variant.statement, a_statement_ptr, iek_statement);
            break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
          /* Nodes generated by IL lowering for partial lowering of exception
             handling features. */
          case enk_lowered_eh_construct:
            switch (ptr->variant.lowered_eh.kind) {
              case leck_caught_object_address:
                remap_ptr_not_needed(ptr->variant.lowered_eh.variant.
                                                         caught_object_handler,
                                     a_handler_ptr, iek_handler);
                break;
              case leck_thrown_object_address:
                /* No pointers. */
                break;
              case leck_cleanup_state:
              case leck_unreachable_cleanup_state:
#if !GENERATE_EH_TABLES
                remap_ptr_not_needed(ptr->variant.lowered_eh.variant.
                                                                   cleanup_ptr,
                                     a_dynamic_init_ptr, iek_dynamic_init);
#endif /* !GENERATE_EH_TABLES */
                break;
              case leck_function_prologue:
                walk_ptr(ptr->variant.lowered_eh.variant.prologue_info,
                         an_eh_prologue_supplement_ptr,
                         iek_eh_prologue_supplement);
                break;
              case leck_function_epilogue:
                remap_ptr_not_needed(
                          ptr->variant.lowered_eh.variant.epilogue_routine,
                          a_routine_ptr, iek_routine);
                break;
              case leck_catch_epilogue:
                remap_ptr_not_needed(
                          ptr->variant.lowered_eh.variant.epilogue_handler,
                          a_handler_ptr, iek_handler);
                break;
              case leck_try_epilogue:
                remap_ptr_not_needed(
                          ptr->variant.lowered_eh.variant.epilogue_try_block,
                          a_try_supplement_ptr, iek_try_supplement);
                break;
              case leck_exception_caught:
              case leck_exception_started:
                /* No pointers. */
                break;
#if !GENERATE_EH_TABLES
              case leck_initialization_completed:
                remap_ptr_not_needed(ptr->variant.lowered_eh.variant.
                                                                  dynamic_init,
                                     a_dynamic_init_ptr, iek_dynamic_init);
                break;
#endif /* !GENERATE_EH_TABLES */
              case leck_internal_try:
                walk_ptr(ptr->variant.lowered_eh.variant.
                                                         internal_try.try_expr,
                          an_expr_node_ptr, iek_expr_node);
                walk_ptr(ptr->variant.lowered_eh.variant.
                                                       internal_try.catch_expr,
                          an_expr_node_ptr, iek_expr_node);
                break;
              default:
                unexpected_condition_str(
                      "walk_entry_and_subtree: bad lowered eh construct kind");
            }  /* switch */
            break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          case enk_result_of_overriding_function:
            /* Node generated as part of the body of an entry function used
               as a wrapper for a call of an overriding virtual function
               with a covariant return type. */
            break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#endif /* ifdef CFE */
#ifdef FFE
          case enk_stmt_label_value:
            remap_ptr(ptr->variant.stmt_label_value, a_label_ptr, iek_label);
            break;
#endif /* ifdef FFE */
          default:
            unexpected_condition_str(
                                 "walk_entry_and_subtree: bad expr node kind");
        }  /* switch */
      }
      break;
#ifdef CFE
    case iek_for_loop:
      {
        a_for_loop_ptr ptr = (a_for_loop_ptr)entry_ptr;
        walk_ptr(ptr->initialization, a_statement_ptr, iek_statement);
        walk_ptr(ptr->increment, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->for_init_scope, a_scope_ptr, iek_scope);
#if UPC_EXTENSIONS_ALLOWED
        walk_ptr(ptr->affinity, an_expr_node_ptr, iek_expr_node);
#endif /* UPC_EXTENSIONS_ALLOWED */
      }
      break;
    case iek_switch_clause:
      {
        a_switch_clause_ptr ptr = (a_switch_clause_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_switch_clause_ptr, iek_switch_clause);
        walk_list(ptr->constant_list, a_constant_ptr, iek_constant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_list(ptr->case_positions, a_switch_case_entry_ptr,
                  iek_switch_case_entry);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        walk_list(ptr->statements, a_statement_ptr, iek_statement);
      }
      break;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    case iek_switch_case_entry:
      {
        a_switch_case_entry_ptr ptr = (a_switch_case_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_switch_case_entry_ptr,
                       iek_switch_case_entry);
        walk_ptr(ptr->constant, a_constant_ptr, iek_constant);
      }
      break;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    case iek_handler:
      {
        a_handler_ptr ptr = (a_handler_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_handler_ptr, iek_handler);
#if NEEDED_FLAG_WALK
        walk_ptr(ptr->parameter, a_variable_ptr, iek_variable);
#else /* !NEEDED_FLAG_WALK */
        /* The associated parameter, if any, will appear on the variables
           list of the current scope.  Therefore, here we just remap the
           pointer but do not walk the subtree. */
        remap_ptr(ptr->parameter, a_variable_ptr, iek_variable);
#endif /* NEEDED_FLAG_WALK */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        { a_variable_ptr parameter = ptr->parameter;
          if (parameter != NULL) {
            a_type_ptr param_type = parameter->type;
            if (is_ptr_or_ref_type(param_type)) {
              param_type = type_pointed_to(param_type);
              definition_needed_if_class(param_type);
            }  /* if */
          }  /* if */
        }
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        walk_ptr(ptr->statement, a_statement_ptr, iek_statement);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
      }
      break;
    case iek_try_supplement:
      {
        a_try_supplement_ptr ptr = (a_try_supplement_ptr)entry_ptr;
        walk_ptr(ptr->statement, a_statement_ptr, iek_statement);
        walk_list(ptr->handlers, a_handler_ptr, iek_handler);
        remap_ptr_not_needed(ptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
      }
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case iek_microsoft_try_supplement:
      {
        a_microsoft_try_supplement_ptr ptr =
                                     (a_microsoft_try_supplement_ptr)entry_ptr;
        walk_ptr(ptr->guarded_statement, a_statement_ptr, iek_statement);
        walk_ptr(ptr->except_expr, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->cleanup_statement, a_statement_ptr, iek_statement);
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
    case iek_block:
#ifdef CFE
      {
#if !NEEDED_FLAG_WALK
        a_block_ptr ptr = (a_block_ptr)entry_ptr;
        /* The associated scope, if any, will appear on the list of local
           scopes for the current scope.  Therefore, here we just remap
           the pointer but do not walk the subtree. */
        remap_ptr_not_needed(ptr->assoc_scope, a_scope_ptr, iek_scope);
        remap_ptr_not_needed(ptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
#endif /* !NEEDED_FLAG_WALK */
      }
#endif /* ifdef CFE */
      break;
    case iek_statement:
      /* Statements account for more than 10% of the IL nodes (they're the
         second most common, after expr nodes), so do not call a subroutine
         for them. */
      {
        a_statement_ptr ptr = (a_statement_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_statement_ptr, iek_statement);
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(ptr->source_sequence_entry,
                  a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
        walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
        switch (ptr->kind) {
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
          case stmk_empty:
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
          case stmk_expr:
#ifdef FFE
          case stmk_alt_return:
#endif /* ifdef FFE */
#if GNU_EXTENSIONS_ALLOWED
          case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
          case stmk_upc_notify:
          case stmk_upc_wait:
          case stmk_upc_barrier:
          case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
            /* No additional pointers. */
            break;
          case stmk_if:
            walk_ptr(ptr->variant.if_stmt.then_statement, a_statement_ptr,
                     iek_statement);
            walk_ptr(ptr->variant.if_stmt.else_statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_while:
#ifdef CFE
          case stmk_end_test_while:
#endif /* ifdef CFE */
            walk_ptr(ptr->variant.loop_statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_goto:
            remap_ptr(ptr->variant.label.ptr, a_label_ptr, iek_label);
            remap_ptr_not_needed(ptr->variant.label.lifetime,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case stmk_label:
            remap_ptr_not_needed(ptr->variant.label.ptr, a_label_ptr,
                                 iek_label);
            remap_ptr_not_needed(ptr->variant.label.lifetime,
                                 an_object_lifetime_ptr, iek_object_lifetime);
            break;
          case stmk_return:
            walk_ptr(ptr->variant.return_dynamic_init, a_dynamic_init_ptr,
                     iek_dynamic_init);
            break;
          case stmk_block:
            /* Do extra_info before statements to get declarations out
               before the statements that use them. */
            walk_ptr(ptr->variant.block.extra_info, a_block_ptr, iek_block);
            walk_list(ptr->variant.block.statements, a_statement_ptr,
                      iek_statement);
            break;
#ifdef CFE
#if UPC_EXTENSIONS_ALLOWED
          /* The upc_forall statement is handled like a for statement. */
          case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
          case stmk_for:
            walk_ptr(ptr->variant.for_loop.extra_info, a_for_loop_ptr,
                     iek_for_loop);
            walk_ptr(ptr->variant.for_loop.statement, a_statement_ptr,
                     iek_statement);
            break;
          case stmk_switch:
            walk_list(ptr->variant.switch_stmt.clause_list,
                      a_switch_clause_ptr, iek_switch_clause);
            walk_ptr(ptr->variant.switch_stmt.body_statement,
                     a_statement_ptr, iek_statement);
            break;
          case stmk_init:
            remap_ptr(ptr->variant.dynamic_init, a_dynamic_init_ptr,
                      iek_dynamic_init);
            break;
          case stmk_asm:
            walk_ptr(ptr->variant.asm_entry, an_asm_entry_ptr,
                     iek_asm_entry);
            break;
#if ASM_FUNCTION_ALLOWED
          case stmk_asm_func_body:
            walk_string_ptr(ptr->variant.asm_func_body, iek_other_text, 0);
            break;
#endif /* ASM_FUNCTION_ALLOWED */
          case stmk_try_block:
            walk_ptr(ptr->variant.try_block, a_try_supplement_ptr,
                     iek_try_supplement);
            break;
#if MICROSOFT_EXTENSIONS_ALLOWED
          case stmk_microsoft_try:
            walk_ptr(ptr->variant.microsoft_try,
                     a_microsoft_try_supplement_ptr,
                     iek_microsoft_try_supplement);
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
          case stmk_decl:
            /* No pointers */
            break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          case stmk_set_vla_size:
            remap_ptr(ptr->variant.vla_dimension, a_vla_dimension_ptr,
                      iek_vla_dimension);
            break;
          case stmk_vla_decl:
            if (ptr->variant.vla.is_typedef_decl) {
              remap_ptr(ptr->variant.vla.variant.typedef_type, a_type_ptr,
                        iek_type);
            } else {
              remap_ptr(ptr->variant.vla.variant.variable, a_variable_ptr,
                        iek_variable);
            }  /* if */
            break;
          case stmk_vla_dealloc:
            remap_ptr(ptr->variant.vla_variable, a_variable_ptr, iek_variable);
            break;
#endif /* ifdef CFE */
#ifdef FFE
          case stmk_fentry:
            remap_ptr(ptr->variant.fentry.assoc_routine, a_routine_ptr,
                      iek_routine);
            walk_list(ptr->variant.fentry.prologue, a_statement_ptr,
                      iek_statement);
            break;
          case stmk_ido:
          case stmk_fdo:
            walk_ptr(ptr->variant.do_stmt.loop_statement, a_statement_ptr,
                     iek_statement);
            walk_ptr(ptr->variant.do_stmt.do_info, a_do_loop_ptr,
                     iek_do_loop);
            break;
          case stmk_iarith_if:
          case stmk_farith_if:
          case stmk_computed_goto:
          case stmk_assigned_goto:
            walk_list(ptr->variant.label_list, a_label_list_entry_ptr,
                      iek_label_list_entry);
            break;
          case stmk_stop:
          case stmk_pause:
            walk_ptr(ptr->variant.stop_pause_string, a_constant_ptr,
                     iek_constant);
            break;
          case stmk_set_array_shape:
            remap_ptr(ptr->variant.array_variable, a_variable_ptr,
                      iek_variable);
            break;
          case stmk_input_output:
            walk_ptr(ptr->variant.input_output,
                     an_input_output_description_ptr,
                     iek_input_output_description);
            break;
#endif /* ifdef FFE */
          default:
            unexpected_condition_str(
                                 "walk_entry_and_subtree: bad statement kind");
        }  /* switch */
      }
      break;
    case iek_pragma:
      {
        a_pragma_ptr ptr = (a_pragma_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_pragma_ptr, iek_pragma);
        remap_ptr(ptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)ptr->entity.kind);
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        remap_ptr(ptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
        walk_string_ptr(ptr->pragma_text, iek_other_text, 0);
#if IDENT_DIRECTIVE_AND_PRAGMA
        if (ptr->kind == (a_pragma_kind)pk_ident) {
          walk_ptr(ptr->variant.ident_string, a_constant_ptr, iek_constant);
        }  /* if */
#endif /* IDENT_DIRECTIVE_AND_PRAGMA */
      }
      break;
#if RECORD_HIDDEN_NAMES_IN_IL
#if !NEEDED_FLAG_WALK
    case iek_hidden_name:
      {
        a_hidden_name_ptr ptr = (a_hidden_name_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_hidden_name_ptr, iek_hidden_name);
        remap_ptr(ptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)ptr->entity.kind);
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
    case iek_template_parameter:
      {
        a_template_parameter_ptr ptr = (a_template_parameter_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_template_parameter_ptr,
                       iek_template_parameter);
        switch (ptr->kind) {
          case tpk_error:
            break;
          case tpk_type:
            walk_ptr(ptr->variant.type.ptr, a_type_ptr, iek_type);
            walk_ptr(ptr->variant.type.default_arg_type, a_type_ptr,
                     iek_type);
            break;
          case tpk_nontype:
            walk_ptr(ptr->variant.nontype.constant, a_constant_ptr,
                     iek_constant);
            walk_ptr(ptr->variant.nontype.default_arg_constant,
                     a_constant_ptr, iek_constant);
            break;
          case tpk_template:
            walk_ptr(ptr->variant.templ.class_template, a_template_ptr,
                     iek_template);
            walk_ptr(ptr->variant.templ.default_arg_template, a_template_ptr,
                     iek_template);
            break;
          default:
            unexpected_condition_str("unexpected template parameter kind");
        }  /* switch */
      }
      break;
    case iek_template_decl:
      {
        a_template_decl_ptr ptr = (a_template_decl_ptr)entry_ptr;
        walk_ptr(ptr->parent, a_template_decl_ptr, iek_template_decl);
        walk_list(ptr->param_list, a_template_parameter_ptr,
                  iek_template_parameter);
      }
      break;
    case iek_template:
      {
        a_template_ptr ptr = (a_template_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_template_ptr, iek_template);
#if RECORD_TEMPLATE_STRINGS
        walk_string_ptr(ptr->text, iek_other_text, 0);
#endif /* RECORD_TEMPLATE_STRINGS */
        walk_ptr(ptr->template_decl, a_template_decl_ptr, iek_template_decl);
        switch (ptr->kind) {
          case templk_none:
            /* This is an error case; presumably diagnosed in the front end. */
            break;
          case templk_function:
          case templk_member_function:
            remap_ptr(ptr->prototype_instantiation.routine, a_routine_ptr,
                      iek_routine);
            break;
          case templk_class:
          case templk_member_class:
            remap_ptr(ptr->prototype_instantiation.type, a_type_ptr,
                      iek_type);
            break;
          case templk_static_data_member:
            remap_ptr(ptr->prototype_instantiation.variable, a_variable_ptr,
                      iek_variable);
            break;
          case templk_template_template_param:
            /* No active variant field. */
            break;
          default:
            unexpected_condition_str(
                               "walk_entry_and_subtree: bad template kind");
            break;
        }  /* switch */
        remap_ptr(ptr->canonical_template, a_template_ptr, iek_template);
        remap_ptr(ptr->definition_template, a_template_ptr, iek_template);
        remap_ptr(ptr->prototype_template, a_template_ptr, iek_template);
        /* The template_info pointer should be NULL for any entry actually
           written and read. */
        conditionally_clear_fe_pointer(ptr->template_info);
      }
      break;
#if RECORD_MACROS_IN_IL
    case iek_macro:
      {
        a_macro_ptr ptr = (a_macro_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_macro_ptr, iek_macro);
        walk_string_ptr(ptr->text, iek_other_text, 0);
      }
      break;
#endif /* RECORD_MACROS_IN_IL */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    case iek_decl_position_supplement:
      /* No pointers. */
      break;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_FORM_OF_NAME_REFERENCE
    case iek_name_qualifier:
      {
        a_name_qualifier_ptr ptr = (a_name_qualifier_ptr)entry_ptr;
        /* The "next" pointer is for front end use only. */
        conditionally_clear_fe_pointer(ptr->next);
        if (ptr->is_class) {
          walk_ptr(ptr->qualifier.class_type, a_type_ptr, iek_type);
        } else {
          walk_ptr(ptr->qualifier.namespace_ptr, a_namespace_ptr,
                   iek_namespace);
        }  /* if */
        walk_ptr(ptr->previous_qualifier, a_name_qualifier_ptr,
                 iek_name_qualifier);
      }
      break;
    case iek_name_reference:
      {
        a_name_reference_ptr ptr = (a_name_reference_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_name_reference_ptr, iek_name_reference);
        walk_ptr(ptr->qualifier, a_name_qualifier_ptr, iek_name_qualifier);
      }
      break;
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
    case iek_object_lifetime:
      {
        an_object_lifetime_ptr ptr = (an_object_lifetime_ptr)entry_ptr;
        remap_ptr_not_needed(ptr->entity.ptr, a_char_ptr,
                             (an_il_entry_kind)ptr->entity.kind);
        /* The destructors list is linked on the field
           "next_in_destruction_list" because the usual "next" is used for
           a different list. */
        walk_list_on_link_field(ptr->destructions, a_dynamic_init_ptr,
                                iek_dynamic_init, next_in_destruction_list);
#if !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        /* Don't walk the parent pointer for keep_in_il processing because
           that can cause visits to siblings that aren't going to stay in the
           tree. */
        remap_ptr(ptr->parent_lifetime, an_object_lifetime_ptr,
                  iek_object_lifetime);
#endif /* !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK */
        remap_ptr_not_needed(ptr->parent_destruction_sublist,
                             a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(ptr->child_lifetime, an_object_lifetime_ptr,
                  iek_object_lifetime);
        remap_next_ptr(ptr->next, an_object_lifetime_ptr, iek_object_lifetime);
      }
      break;
    case iek_scope:
      {
        a_scope_ptr  ptr = (a_scope_ptr)entry_ptr;
        a_scope_kind kind = ptr->kind;
        remap_next_ptr(ptr->next, a_scope_ptr, iek_scope);
        switch (kind) {
          case sck_file:
#ifdef FFE
          case sck_stmt_function:
#endif  /* ifdef FFE */
            /* No pointers */
            break;
#ifdef CFE
          case sck_block:
            /* Call remap_ptr on the handler entry since it is also on a list
               pointed to from the try-block statement. */
            remap_ptr_not_needed(ptr->variant.assoc_handler, a_handler_ptr,
                                 iek_handler);
            /* Also see assoc_block below. */
            break;
          case sck_func_prototype:
          case sck_class_struct_union:
            remap_ptr_not_needed(ptr->variant.assoc_type, a_type_ptr,
                                 iek_type);
            break;
          case sck_condition:
            remap_ptr_not_needed(ptr->variant.assoc_statement, a_statement_ptr,
                                 iek_statement);
            break;
          case sck_namespace:
            remap_ptr_not_needed(ptr->variant.assoc_namespace, a_namespace_ptr,
                                 iek_namespace);
            break;
#endif  /* ifdef CFE */
          case sck_function:
            /* "ptr", which points to the routine associated with this scope,
               is done after the declarations. */
            walk_list(ptr->variant.routine.parameters, a_variable_ptr,
                      iek_variable);
#ifdef CFE
            walk_list(ptr->variant.routine.constructor_inits,
                      a_constructor_init_ptr, iek_constructor_init);
            walk_ptr(ptr->variant.routine.lifetime_of_local_static_vars,
                     an_object_lifetime_ptr, iek_object_lifetime);
            walk_ptr(ptr->variant.routine.this_param_variable, a_variable_ptr,
                     iek_variable);
            remap_ptr_not_needed(ptr->variant.routine.return_value_variable,
                                 a_variable_ptr, iek_variable);
#endif  /* ifdef CFE */
#ifdef FFE
            walk_ptr(ptr->variant.routine.function_result_var, a_variable_ptr,
                     iek_variable);
#endif /* ifdef FFE */
            break;
          case sck_template_declaration:
          case sck_template_instantiation:
            /* Front end only. */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
            break;
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
          default:
            unexpected_condition_str("walk_entry_and_subtree: bad scope kind");
        }  /* switch */
        /* "assoc_block" is done after the declarations. */
        /* The lifetime pointer needs to be walked and not remapped in
           the file scope and function scopes. */
        walk_ptr(ptr->lifetime, an_object_lifetime_ptr, iek_object_lifetime);
        walk_list(ptr->constants, a_constant_ptr, iek_constant);
#ifdef CFE
#if DO_SUBTREE_WALK
#if NEEDED_FLAG_WALK
        /* Do not walk the types and variables lists to set the "needed"
           flag; a variable or type is not needed simply because it's
           declared. */
        /* On the "needed" flag walk for a class, mark all the virtual
           functions as needed.  Note that if IL lowering is done, there
           will be no functions attached to the class anymore. */
        if (kind == (a_scope_kind)sck_class_struct_union) {
          a_routine_ptr rout = ptr->routines;
          for (; rout != NULL; rout = rout->next) {
            if (rout->is_virtual) {
              walk_ptr(rout, a_routine_ptr, iek_routine);
            }  /* if */
          }  /* for */
        }  /* if */
#else /* !NEEDED_FLAG_WALK */
#if KEEP_IN_IL_WALK
        if (kind == (a_scope_kind)sck_function ||
            kind == (a_scope_kind)sck_block ||
            (kind == (a_scope_kind)sck_class_struct_union &&
             ptr->variant.assoc_type->source_corresp.is_local_to_function)) {
          /* For lists within a function, mark everything to be kept, because
             we don't remove individual entities within function bodies. */
          walk_list(ptr->types, a_type_ptr, iek_type);
          walk_list(ptr->variables, a_variable_ptr, iek_variable);
          walk_list(ptr->routines, a_routine_ptr, iek_routine);
        } else {
          /* For lists not within a function, mark only the needed entities
             to be kept. */
          walk_needed_on_list(ptr->types, a_type_ptr, iek_type, kind);
          walk_needed_on_list(ptr->variables, a_variable_ptr, iek_variable,
                              kind);
          walk_needed_on_list(ptr->routines, a_routine_ptr, iek_routine, kind);
        }  /* if */
#else /* !KEEP_IN_IL_WALK */
        /* Not needed flag walk or keep_in_il walk. */
        if (ptr->scope_orphaned_list_header_generated) {
          /* The local types and static variables at function scope or
             block scope within a function are in the file scope memory region.
             They will be processed during the file scope memory region
             walk because a_scope_orphaned_list_header entry for these lists
             would have been created. */
          remap_list_ptr(ptr->types, a_type_ptr, iek_type);
          remap_list_ptr(ptr->variables, a_variable_ptr, iek_variable);
        } else {
          /* Not a function or block scope, or one for which the orphan
             lists have not been generated yet. */
          walk_list(ptr->types, a_type_ptr, iek_type);
          walk_list(ptr->variables, a_variable_ptr, iek_variable);
        }  /* if */
        walk_list(ptr->routines, a_routine_ptr, iek_routine);
#endif /* KEEP_IN_IL_WALK */
#endif /* NEEDED_FLAG_WALK */
#else /* !DO_SUBTREE_WALK */
        /* Not walking subtrees.  Just remap the pointers. */
        remap_list_ptr(ptr->types, a_type_ptr, iek_type);
        remap_list_ptr(ptr->variables, a_variable_ptr, iek_variable);
        remap_list_ptr(ptr->routines, a_routine_ptr, iek_routine);
#endif /* DO_SUBTREE_WALK */
        walk_list_not_needed(ptr->nonstatic_variables, a_variable_ptr,
                             iek_variable);
#else /* ifndef CFE */
        /* Not the C/C++ front end. */
        walk_list(ptr->types, a_type_ptr, iek_type);
        walk_list(ptr->variables, a_variable_ptr, iek_variable);
        walk_list(ptr->routines, a_routine_ptr, iek_routine);
#endif /* ifdef CFE */
        walk_list_not_needed(ptr->labels, a_label_ptr, iek_label);
#ifdef CFE
        walk_list(ptr->scopes, a_scope_ptr, iek_scope);
        walk_list_with_keep_in_il_reset(ptr->namespaces, a_namespace_ptr,
                                        iek_namespace);
        walk_list_not_needed(ptr->using_decls, a_using_decl_ptr,
                             iek_using_decl);
        walk_list(ptr->asm_entries, an_asm_entry_ptr, iek_asm_entry);
        walk_list(ptr->dynamic_inits, a_dynamic_init_ptr, iek_dynamic_init);
        walk_list(ptr->local_static_variable_inits,
                  a_local_static_variable_init_ptr,
                  iek_local_static_variable_init);
        walk_list(ptr->vla_dimensions, a_vla_dimension_ptr, iek_vla_dimension);
#endif /* ifdef CFE */
        walk_list(ptr->pragmas, a_pragma_ptr, iek_pragma);
        walk_list(ptr->templates, a_template_ptr, iek_template);
#ifdef FFE
        walk_list(ptr->entries, an_entry_description_ptr,
                  iek_entry_description);
        walk_list(ptr->namelist_groups, a_namelist_group_ptr,
                  iek_namelist_group);
#endif /* ifdef FFE */
        if (kind == (a_scope_kind)sck_function) {
          remap_ptr(ptr->variant.routine.ptr, a_routine_ptr, iek_routine);
          walk_ptr(ptr->assoc_block, a_statement_ptr, iek_statement);
        } else {
          remap_ptr(ptr->assoc_block, a_statement_ptr, iek_statement);
        }  /* if */
#if RECORD_HIDDEN_NAMES_IN_IL
        walk_list_not_needed(ptr->hidden_names, a_hidden_name_ptr,
                             iek_hidden_name);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if KEEP_IN_IL_WALK
        /* When setting the keep_in_il flag, source sequence entries are
           kept if and only if the associated IL entry is kept.  (In
           function scopes, all entries are kept.)  Note that this is
           done last so all the keep_in_il flags are set already.
           The file scope is not processed here, but rather in
           mark_to_keep_in_il, because it must be done after orphan
           processing. */
        if (kind == (a_scope_kind)sck_function) {
          set_keep_in_il_on_source_sequence_entries(ptr);
        }  /* if */
#else /* !KEEP_IN_IL_WALK */
        walk_list_not_needed(ptr->source_sequence_list,
                             a_source_sequence_entry_ptr,
                             iek_source_sequence_entry);
        /* The src_seq_sublist_list, which appears only on function scopes,
           is not walked at this time: it is handled during orphan list
           processing. */
#if !NEEDED_FLAG_WALK
        remap_list_ptr(ptr->src_seq_sublist_list, a_src_seq_sublist_ptr,
                       iek_src_seq_sublist);
#endif /* !NEEDED_FLAG_WALK */
#endif /* KEEP_IN_IL_WALK */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }
      break;
#if defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED
    case iek_internal_complex_value:
      /* No pointers. */
      break;
#endif /* defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED */
#ifdef FFE
    case iek_bound_info_entry:
      {
        a_bound_info_entry_ptr ptr = (a_bound_info_entry_ptr)entry_ptr;
        if (ptr->kind == (a_bound_kind)bk_adjustable) {
          walk_ptr(ptr->variant.adjustable_bound, an_expr_node_ptr,
                   iek_expr_node);
        }  /* if */
      }
      break;
    case iek_do_loop:
      {
        a_do_loop_ptr ptr = (a_do_loop_ptr)entry_ptr;
        remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
        walk_ptr(ptr->initial_value, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->final_value, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->increment, an_expr_node_ptr, iek_expr_node);
      }
      break;
    case iek_label_list_entry:
      {
        a_label_list_entry_ptr ptr = (a_label_list_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_label_list_entry_ptr,
                       iek_label_list_entry);
        remap_ptr(ptr->label, a_label_ptr, iek_label);
      }
      break;
    case iek_io_specifier:
      {
        an_io_specifier_ptr ptr = (an_io_specifier_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_io_specifier_ptr, iek_io_specifier);
        switch (ptr->transfer) {
          case iost_label:
            remap_ptr(ptr->variant.label, a_label_ptr, iek_label);
            break;
          case iost_expr_in:
          case iost_var_out:
            walk_ptr(ptr->variant.expr, an_expr_node_ptr, iek_expr_node);
            break;
          default:
            unexpected_condition_str(
                          "walk_entry_and_subtree: bad io specifier transfer");
        }  /* switch */
      }
      break;
    case iek_io_list_item:
      {
        an_io_list_item_ptr ptr = (an_io_list_item_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_io_list_item_ptr, iek_io_list_item);
        switch (ptr->kind) {
          case iol_expr:
          case iol_variable:
            walk_ptr(ptr->variant.expr, an_expr_node_ptr, iek_expr_node);
            break;
          case iol_array:
            remap_ptr(ptr->variant.array_var, a_variable_ptr, iek_variable);
            break;
          case iol_implied_do:
            remap_ptr(ptr->variant.implied_do.variable, a_variable_ptr,
                      iek_variable);
            walk_ptr(ptr->variant.implied_do.initial_value, an_expr_node_ptr,
                     iek_expr_node);
            walk_ptr(ptr->variant.implied_do.final_value, an_expr_node_ptr,
                     iek_expr_node);
            walk_ptr(ptr->variant.implied_do.increment, an_expr_node_ptr,
                     iek_expr_node);
            walk_list(ptr->variant.implied_do.list, an_io_list_item_ptr,
                      iek_io_list_item);
            break;
          default:
            unexpected_condition_str(
                              "walk_entry_and_subtree: bad io list item kind");
        }  /* switch */
      }
      break;
    case iek_namelist_group_member:
      {
        a_namelist_group_member_ptr ptr =
                                        (a_namelist_group_member_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_namelist_group_member_ptr,
                       iek_namelist_group_member);
        remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
      }
      break;
    case iek_namelist_group:
      {
        a_namelist_group_ptr ptr = (a_namelist_group_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_namelist_group_ptr, iek_namelist_group);
        walk_list(ptr->member_list, a_namelist_group_member_ptr,
                  iek_namelist_group_member);
      }
      break;
    case iek_input_output_description:
      {
        an_input_output_description_ptr ptr =
                                    (an_input_output_description_ptr)entry_ptr;
        if (ptr->unit_kind == (an_io_unit_kind)iou_external ||
            ptr->unit_kind == (an_io_unit_kind)iou_internal) {
          walk_ptr(ptr->unit_expr, an_expr_node_ptr, iek_expr_node);
        }  /* if */
        if (ptr->kind == (an_io_statement_kind)ios_encode ||
            ptr->kind == (an_io_statement_kind)ios_decode) {
          walk_ptr(ptr->encode_decode_length, an_expr_node_ptr,
                   iek_expr_node);
        }  /* if */
        switch (ptr->format_kind) {
          case iof_none:
          case iof_error:
          case iof_list_directed:
          case iof_unformatted:
            /* No pointers. */
            break;
          case iof_format_label:
            remap_ptr(ptr->format.label, a_label_ptr, iek_label);
            break;
          case iof_assigned_var:
          case iof_char_expr:
            walk_ptr(ptr->format.expr, an_expr_node_ptr, iek_expr_node);
            break;
          case iof_namelist_directed:
            remap_ptr(ptr->format.namelist_group, a_namelist_group_ptr,
                      iek_namelist_group);
            break;
          default:
            unexpected_condition_str(
                       "walk_entry_and_subtree: bad input output format kind");
        }  /* switch */
        walk_list(ptr->specifier_list, an_io_specifier_ptr, iek_io_specifier);
        walk_list(ptr->item_list, an_io_list_item_ptr, iek_io_list_item);
      }
      break;
    case iek_entry_param:
      {
        an_entry_param_ptr ptr = (an_entry_param_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_entry_param_ptr, iek_entry_param);
        remap_ptr(ptr->param_var, a_variable_ptr, iek_variable);
      }
      break;
    case iek_entry_description:
      {
        an_entry_description_ptr ptr = (an_entry_description_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_entry_description_ptr,
                       iek_entry_description);
        remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
        walk_list(ptr->parameters, an_entry_param_ptr, iek_entry_param);
        walk_ptr(ptr->function_result_var, a_variable_ptr, iek_variable);
      }
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case iek_namespace:
      {
        a_namespace_ptr ptr = (a_namespace_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, a_namespace_ptr, iek_namespace);
        if (ptr->is_namespace_alias) {
          remap_ptr(ptr->variant.assoc_namespace, a_namespace_ptr,
                    iek_namespace);
        } else {
          /* When doing the "needed" flag walk, the members of a namespace are
             not considered needed merely because the namespace itself is
             needed. */
          walk_ptr_not_needed(ptr->variant.assoc_scope, a_scope_ptr,
                              iek_scope);
        }  /* if */
      }
      break;
#if !NEEDED_FLAG_WALK
    case iek_using_decl:
      {
        a_using_decl_ptr ptr = (a_using_decl_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_using_decl_ptr, iek_using_decl);
        /* walk_ptr used instead of remap_ptr because the entity referenced
           can be a constant, e.g., in a prototype instantiation. */
        walk_ptr(ptr->entity.ptr, a_char_ptr,
                 (an_il_entry_kind)ptr->entity.kind);
        if (ptr->is_class_member) {
          remap_ptr(ptr->qualifier.class_type, a_type_ptr, iek_type);
        } else {
          remap_ptr(ptr->qualifier.namespace_ptr, a_namespace_ptr,
                    iek_namespace);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !KEEP_IN_IL_WALK
        remap_ptr(ptr->source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        remap_ptr(ptr->next_in_overload_set, a_using_decl_ptr, iek_using_decl);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_dynamic_init:
      {
        a_dynamic_init_ptr ptr = (a_dynamic_init_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_dynamic_init_ptr, iek_dynamic_init);
        remap_ptr(ptr->variable, a_variable_ptr, iek_variable);
        remap_ptr(ptr->destructor, a_routine_ptr, iek_routine);
        if (ptr->destructor != NULL) {
          set_proper_routine_definition_needed_flag(ptr->destructor);
        }  /* if */
        remap_ptr_not_needed(ptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
#if !NEEDED_FLAG_WALK
        remap_next_ptr(ptr->next_in_destruction_list, a_dynamic_init_ptr,
                       iek_dynamic_init);
#endif /* !NEEDED_FLAG_WALK */
        remap_ptr_not_needed(ptr->init_expr_lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
        switch (ptr->kind) {
          case dik_none:
          case dik_zero:
          case dik_bitwise_copy:
            /* No pointers. */
            break;
          case dik_constant:
          case dik_nonconstant_aggregate:
            walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
            break;
          case dik_expression:
          case dik_call_returning_class_via_cctor:
            walk_ptr(ptr->variant.expression, an_expr_node_ptr, iek_expr_node);
            break;
          case dik_constructor:
            if (ptr->variant.constructor.ptr != NULL) {
              remap_ptr(ptr->variant.constructor.ptr, a_routine_ptr,
                        iek_routine);
              set_proper_routine_definition_needed_flag(
                                                 ptr->variant.constructor.ptr);
            }  /* if */
            walk_list(ptr->variant.constructor.args, an_expr_node_ptr,
                      iek_expr_node);
            break;
          default:
            unexpected_condition_str(
                              "walk_entry_and_subtree: bad dynamic init kind");
        }  /* switch */
#if DO_IL_LOWERING
        conditionally_clear_fe_pointer(ptr->destructible_entity_descr);
#endif /* DO_IL_LOWERING */
        remap_ptr(ptr->lifetime_of_overlapping_temps, an_object_lifetime_ptr,
                  iek_object_lifetime);
      }
      break;
    case iek_local_static_variable_init:
      {
        a_local_static_variable_init_ptr ptr =
                                  (a_local_static_variable_init_ptr)entry_ptr;

        remap_next_ptr(ptr->next, a_local_static_variable_init_ptr,
                       iek_local_static_variable_init);
        remap_ptr_not_needed(ptr->variable, a_variable_ptr, iek_variable);
        walk_initializer(ptr->init_kind, ptr->initializer);
        remap_ptr_not_needed(ptr->lifetime, an_object_lifetime_ptr,
                             iek_object_lifetime);
      }
      break;
    case iek_vla_dimension:
      {
        a_vla_dimension_ptr ptr = (a_vla_dimension_ptr)entry_ptr;

        remap_next_ptr(ptr->next, a_vla_dimension_ptr, iek_vla_dimension);
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        walk_ptr(ptr->dimension_expr, an_expr_node_ptr, iek_expr_node);
      }
      break;
#if !NEEDED_FLAG_WALK
    case iek_overriding_virtual_function:
      {
        an_overriding_virtual_function_ptr ptr =
                                 (an_overriding_virtual_function_ptr)entry_ptr;
        remap_next_ptr(ptr->next, an_overriding_virtual_function_ptr,
                       iek_overriding_virtual_function);
        remap_ptr(ptr->overriding_function, a_routine_ptr, iek_routine);
        remap_ptr(ptr->primary_function, a_routine_ptr, iek_routine);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
        remap_ptr(ptr->return_adjustment_base_class, a_base_class_ptr,
                  iek_base_class);
      }
      break;
    case iek_derivation_step:
      {
        a_derivation_step_ptr ptr = (a_derivation_step_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_derivation_step_ptr,
                       iek_derivation_step);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
      }
      break;
    case iek_base_class_derivation:
      {
        a_base_class_derivation_ptr ptr =
                                       (a_base_class_derivation_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_base_class_derivation_ptr,
                       iek_base_class_derivation);
        walk_list(ptr->path, a_derivation_step_ptr, iek_derivation_step);
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
    case iek_base_class:
      {
        a_base_class_ptr ptr = (a_base_class_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_base_class_ptr, iek_base_class);
        remap_ptr(ptr->type, a_type_ptr, iek_type);
        set_proper_definition_needed_flag(ptr->type);
        remap_ptr(ptr->derived_class, a_type_ptr, iek_type);
        set_proper_definition_needed_flag(ptr->derived_class);
        conditionally_clear_fe_pointer(ptr->trans_unit_corresp);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
        remap_ptr_not_needed(ptr->data_section_base_class, a_base_class_ptr,
                             iek_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
        remap_ptr_not_needed(ptr->pointer_base_class, a_base_class_ptr,
                             iek_base_class);
        walk_list_not_needed(ptr->derivation, a_base_class_derivation_ptr,
                             iek_base_class_derivation);
        walk_list_not_needed(ptr->overriding_virtual_functions,
                             an_overriding_virtual_function_ptr,
                             iek_overriding_virtual_function);
#if DO_IL_LOWERING
        conditionally_clear_fe_pointer(ptr->virtual_function_table_var);
#endif /* DO_IL_LOWERING */
      }
      break;
    case iek_class_list_entry:
      {
        a_class_list_entry_ptr ptr = (a_class_list_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_class_list_entry_ptr,
                       iek_class_list_entry);
        remap_ptr(ptr->class_type, a_type_ptr, iek_type);
      }
      break;
    case iek_routine_list_entry:
      {
        a_routine_list_entry_ptr ptr = (a_routine_list_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_routine_list_entry_ptr,
                       iek_routine_list_entry);
        remap_ptr(ptr->routine, a_routine_ptr, iek_routine);
      }
      break;
    case iek_class_type_supplement:
      {
        a_class_type_supplement_ptr ptr;
        ptr = (a_class_type_supplement_ptr)entry_ptr;
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        goto after_entry_from_class;
handle_class_type_supplement_for_class:
        /* Processing comes here from the class type.  For the "needed" and
           "keep_in_il" walk we have to be able to know where the class type
           is. */
        ptr = ((a_type_ptr)entry_ptr)->variant.class_struct_union.extra_info;
after_entry_from_class:
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* Fields to be processed even if the definition of the class is
           not to be processed: */
        remap_ptr(ptr->assoc_template, a_template_ptr, iek_template);
        walk_list(ptr->template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
        walk_list(ptr->partial_spec_template_arg_list, a_template_arg_ptr,
                  iek_template_arg);
#if MICROSOFT_EXTENSIONS_ALLOWED
        walk_string_ptr(ptr->uuid_string, iek_other_text, 0);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if !NEEDED_FLAG_WALK
#if KEEP_IN_IL_WALK
        /* Visit befriending classes for the "keep_in_il" sweep. */
        set_keep_in_il_on_befriending_classes(ptr->befriending_classes);
#else /* !KEEP_IN_IL_WALK */
        /* All cases except NEEDED_FLAG_WALK and KEEP_IN_IL_WALK. */
        walk_list(ptr->befriending_classes, a_class_list_entry_ptr,
                  iek_class_list_entry);
#endif /* KEEP_IN_IL_WALK */
#endif /* !NEEDED_FLAG_WALK */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* During these walks, visit the definition only if necessary. */
        if (
#if NEEDED_FLAG_WALK
            class_definition_needed_flag_is_set((a_type_ptr)entry_ptr)
#else /* !NEEDED_FLAG_WALK (i.e., KEEP_IN_IL_WALK) */
            ((a_type_ptr)entry_ptr)->variant.class_struct_union.
                                                   keep_definition_in_il
#endif /* NEEDED_FLAG_WALK */
                                                                        )
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
        /* Do not insert code here. */
        {
          /* Fields to be processed only if the definition of the class
             is to be processed: */
          walk_list(ptr->base_classes, a_base_class_ptr, iek_base_class);
          remap_ptr(ptr->anonymous_union_field, a_field_ptr, iek_field);
          walk_ptr(ptr->assoc_scope, a_scope_ptr, iek_scope);
          remap_ptr_not_needed(ptr->virtual_function_info_base_class,
                               a_base_class_ptr, iek_base_class);
          walk_list_not_needed(ptr->friend_routines, a_routine_list_entry_ptr,
                               iek_routine_list_entry);
          walk_list_not_needed(ptr->friend_classes, a_class_list_entry_ptr,
                               iek_class_list_entry);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
          remap_ptr(ptr->assoc_operator_new_routine, a_routine_ptr,
                    iek_routine);
#if !DO_IL_LOWERING
          if (ptr->assoc_operator_new_routine != NULL) {
            set_proper_routine_definition_needed_flag(
                                              ptr->assoc_operator_new_routine);
          }  /* if */
#endif /* !DO_IL_LOWERING */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
          remap_ptr(ptr->assoc_operator_delete_routine, a_routine_ptr,
                    iek_routine);
#if !DO_IL_LOWERING
          if (ptr->assoc_operator_delete_routine != NULL) {
            set_proper_routine_definition_needed_flag(
                                           ptr->assoc_operator_delete_routine);
          }  /* if */
#endif /* !DO_IL_LOWERING */
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if DO_IL_LOWERING
          conditionally_clear_fe_pointer(ptr->virtual_function_table_var);
          conditionally_clear_fe_pointer(ptr->type_as_subobject);
#if MICROSOFT_EXTENSIONS_ALLOWED
          conditionally_clear_fe_pointer(ptr->uuid_variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
          conditionally_clear_fe_pointer(ptr->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#else /* !DO_IL_LOWERING */
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
          if (ptr->anonymous_union_kind ==
                                       (an_anonymous_union_kind)auk_variable) {
            /* Deal with cases like
                 static union { typedef int T; };
                 T x;
               (Defining types in an anonymous union is no longer allowed,
               but we accept it for backwards compatibility.)
               When the type is marked as needed, the variable for the
               anonymous union must also be marked.  This is a problem only
               for types in anonymous unions; references to data members will
               include a reference to the variable. */
            if (ptr->assoc_scope->types != NULL) {
              a_variable_ptr anon_union_var;
              /* Recall that entry_ptr is the class pointer. */
              anon_union_var = find_parent_var_of_anon_union_type(
                                                        (a_type_ptr)entry_ptr);
              walk_ptr(anon_union_var, a_variable_ptr, iek_variable);
            }  /* if */
          }  /* if */
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#endif /* DO_IL_LOWERING */
        }  /* if */
      }
      break;
    case iek_template_param_type_supplement:
      {
        a_template_param_type_supplement_ptr ptr =
                               (a_template_param_type_supplement_ptr)entry_ptr;
        /* Use walk_ptr instead of remap_ptr because proxy classes are
           not linked into the IL. */
        walk_ptr(ptr->class_type, a_type_ptr, iek_type);
      }
      break;
    case iek_constructor_init:
      {
        a_constructor_init_ptr ptr = (a_constructor_init_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_constructor_init_ptr,
                       iek_constructor_init);
        switch (ptr->kind) {
          case cik_virtual_base_class:
          case cik_direct_base_class:
            /* With prototype instantiations, there can be generated base
               class entries. */
            walk_ptr(ptr->variant.base_class, a_base_class_ptr,
                     iek_base_class);
            break;
          case cik_field:
            remap_ptr(ptr->variant.field, a_field_ptr, iek_field);
            break;
          default:
            unexpected_condition_str(
                          "walk_entry_and_subtree: bad constructor init kind");
        }  /* switch */
        walk_ptr(ptr->initializer, a_dynamic_init_ptr, iek_dynamic_init);
      }
      break;
    case iek_asm_entry:
      {
        an_asm_entry_ptr ptr = (an_asm_entry_ptr)entry_ptr;
        walk_source_corresp(ptr->source_corresp);
        remap_next_ptr(ptr->next, an_asm_entry_ptr, iek_asm_entry);
        walk_ptr(ptr->asm_string, a_constant_ptr, iek_constant);
#if GNU_EXTENSIONS_ALLOWED
        walk_list(ptr->operands, an_asm_operand_ptr, iek_asm_operand);
        walk_list(ptr->clobbers, a_named_register_list_ptr, 
                  iek_named_register_list);
#endif /* GNU_EXTENSIONS_ALLOWED */
      }
      break;
#if GNU_EXTENSIONS_ALLOWED
    case iek_asm_operand:
      {
        an_asm_operand_ptr ptr = (an_asm_operand_ptr)entry_ptr;
        walk_list(ptr->constraints,
                  an_asm_operand_constraint_ptr, iek_asm_operand_constraint);
        walk_ptr(ptr->expression, an_expr_node_ptr, iek_expr_node);
      }
      break;
    case iek_asm_operand_constraint:
      break;
    case iek_named_register_list:
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case iek_template_arg:
      {
        a_template_arg_ptr ptr = (a_template_arg_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_template_arg_ptr, iek_template_arg);
        if (is_type_templ_arg(ptr)) {
          walk_ptr(ptr->variant.type, a_type_ptr, iek_type);
        } else if (is_nontype_templ_arg(ptr)) {
          if (!ptr->is_array_bound_of_unknown_type) {
            walk_ptr(ptr->variant.constant, a_constant_ptr, iek_constant);
          }  /* if */
        } else {
          /* A template template argument. */
          walk_ptr(ptr->variant.templ, a_template_ptr, iek_template);
        }  /* if */
        conditionally_clear_fe_pointer(ptr->arg_operand);
      }
      break;
    case iek_new_delete_supplement:
      {
        a_new_delete_supplement_ptr ptr =
                                        (a_new_delete_supplement_ptr)entry_ptr;
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(ptr->type);
        walk_ptr(ptr->routine, a_routine_ptr, iek_routine);
        if (ptr->routine != NULL) {
          set_proper_routine_definition_needed_flag(ptr->routine);
        }  /* if */
        walk_list(ptr->arg, an_expr_node_ptr, iek_expr_node);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(ptr->freeing_of_storage_on_exception, a_dynamic_init_ptr,
                 iek_dynamic_init);
      }
      break;
    case iek_throw_supplement:
      {
        a_throw_supplement_ptr ptr = (a_throw_supplement_ptr)entry_ptr;
        walk_ptr(ptr->type, a_type_ptr, iek_type);
        definition_needed_if_class(ptr->type);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
        walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if !ABI_CHANGES_FOR_RTTI
        walk_list_not_needed(ptr->accessible_base_classes,
                             an_accessible_base_class_ptr,
                             iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
        remap_ptr(ptr->destructor, a_routine_ptr, iek_routine);
        if (ptr->destructor != NULL) {
          set_proper_routine_definition_needed_flag(ptr->destructor);
        }  /* if */
      }
      break;
    case iek_condition_supplement:
      {
        a_condition_supplement_ptr ptr = (a_condition_supplement_ptr)entry_ptr;

        walk_ptr_not_needed(ptr->scope, a_scope_ptr, iek_scope);
        walk_ptr(ptr->dynamic_init, a_dynamic_init_ptr, iek_dynamic_init);
        walk_ptr(ptr->expr, an_expr_node_ptr, iek_expr_node);
      }
      break;
#if !ABI_CHANGES_FOR_RTTI
#if !NEEDED_FLAG_WALK
    case iek_accessible_base_class:
      {
        an_accessible_base_class_ptr ptr =
                                      (an_accessible_base_class_ptr)entry_ptr;
        
        remap_next_ptr(ptr->next, an_accessible_base_class_ptr,
                       iek_accessible_base_class);
        remap_ptr(ptr->base_class, a_base_class_ptr, iek_base_class);
      }
      break;
#endif /* !NEEDED_FLAG_WALK */
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    case iek_eh_prologue_supplement:
      {
        an_eh_prologue_supplement_ptr ptr =
                                      (an_eh_prologue_supplement_ptr)entry_ptr;
        remap_ptr(ptr->routine, a_routine_ptr, iek_routine);
#if GENERATE_EH_TABLES
        remap_ptr(ptr->region_table, a_variable_ptr, iek_variable);
        remap_ptr(ptr->array_table, a_variable_ptr, iek_variable);
#endif /* GENERATE_EH_TABLES */
      }
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifdef CFE */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK
    case iek_source_sequence_entry:
      {
        a_source_sequence_entry_ptr ptr =
                                       (a_source_sequence_entry_ptr)entry_ptr;
        an_il_entry_kind            kind = (an_il_entry_kind)ptr->entity.kind;

#if !KEEP_IN_IL_WALK
        remap_next_ptr(ptr->next, a_source_sequence_entry_ptr,
                       iek_source_sequence_entry);
        remap_ptr(ptr->prev, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
#endif /* !KEEP_IN_IL_WALK */
#if CHECKING
        /* Check for empty source sequence entries that remain in the IL. */
        if (kind == (an_il_entry_kind)iek_none) {
          unexpected_condition_str(
                        "walk_entry_and_subtree: empty source sequence entry");
        }  /* if */
#endif /* CHECKING */
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL.  Secondary declarations, end of construct entries, and
           instantiation directives get walked because they are in effect
           supplements to the source sequence entry rather than free-standing
           IL entries; they aren't pointed to from elsewhere in the IL tree. */
        if (kind == iek_type ||
            kind == iek_src_seq_secondary_decl ||
            kind == iek_src_seq_end_of_construct ||
            kind == iek_instantiation_directive) {
          walk_ptr(ptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(ptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
      }
      break;
    case iek_src_seq_secondary_decl:
      {
        a_src_seq_secondary_decl_ptr ptr =
                                      (a_src_seq_secondary_decl_ptr)entry_ptr;
        an_il_entry_kind             kind = (an_il_entry_kind)ptr->entity.kind;
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL.  Similarly, friend function declarations may refer to routines
           that do not appear on any list (e.g., dependent class members) and
           must be walked here. */
        if (kind == iek_type || ptr->friend_decl) {
          walk_ptr(ptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(ptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
        walk_ptr(ptr->declared_type, a_type_ptr, iek_type);
#if RECORD_FORM_OF_NAME_REFERENCE
        remap_ptr(ptr->name_reference, a_name_reference_ptr,
                  iek_name_reference);

#endif /* RECORD_FORM_OF_NAME_REFERENCE */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(ptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
    case iek_src_seq_end_of_construct:
      {
        a_src_seq_end_of_construct_ptr ptr =
                                    (a_src_seq_end_of_construct_ptr)entry_ptr;
        an_il_entry_kind             kind = (an_il_entry_kind)ptr->entity.kind;
        /* Types get walked instead of remapped because some types defined
           in prototype scopes in C (e.g., in a cast) get eliminated from the
           IL. */
        if (kind == iek_type) {
          walk_ptr(ptr->entity.ptr, a_char_ptr, kind);
        } else {
          remap_ptr(ptr->entity.ptr, a_char_ptr, kind);
        }  /* if */
      }
      break;
#if !KEEP_IN_IL_WALK
    case iek_src_seq_sublist:
      {
        a_src_seq_sublist_ptr ptr = (a_src_seq_sublist_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_src_seq_sublist_ptr, iek_src_seq_sublist);
        walk_list(ptr->source_sequence_list, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
        remap_ptr(ptr->last_source_sequence_entry, a_source_sequence_entry_ptr,
                  iek_source_sequence_entry);
      }
      break;
#endif /* !KEEP_IN_IL_WALK */
    case iek_instantiation_directive:
      {
        an_instantiation_directive_ptr ptr =
                                    (an_instantiation_directive_ptr)entry_ptr;
        remap_ptr(ptr->entity.ptr, a_char_ptr,
                  (an_il_entry_kind)ptr->entity.kind);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        walk_ptr(ptr->decl_pos_info, a_decl_position_supplement_ptr,
                 iek_decl_position_supplement);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }
      break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && ... */
    case iek_scope_orphaned_list_header:
      {
        a_scope_orphaned_list_header_ptr ptr =
                                   (a_scope_orphaned_list_header_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_scope_orphaned_list_header_ptr,
                       iek_scope_orphaned_list_header);
        remap_ptr(ptr->assoc_routine, a_routine_ptr, iek_routine);
#if NEEDED_FLAG_WALK || KEEP_IN_IL_WALK
        /* Don't walk these lists.  They will have been walked from the
           function scope if necessary. */
        remap_list_ptr(ptr->orphaned_types, a_type_ptr, iek_type);
        remap_list_ptr(ptr->orphaned_variables, a_variable_ptr, iek_variable);
#else /* !(NEEDED_FLAG_WALK || KEEP_IN_IL_WALK) */
        walk_list(ptr->orphaned_types, a_type_ptr, iek_type);
        walk_list(ptr->orphaned_variables, a_variable_ptr, iek_variable);
#endif /* NEEDED_FLAG_WALK || KEEP_IN_IL_WALK */
#if GENERATE_SOURCE_SEQUENCE_LISTS && !NEEDED_FLAG_WALK && !KEEP_IN_IL_WALK
        walk_list(ptr->orphaned_src_seq_sublists,
                  a_src_seq_sublist_ptr, iek_src_seq_sublist);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS && */
      }
      break;
#if ONE_INSTANTIATION_PER_OBJECT
    case iek_per_instantiation_needed_flags_entry:
      {
#if !DO_SUBTREE_WALK
        a_per_instantiation_needed_flags_entry_ptr ptr =
                         (a_per_instantiation_needed_flags_entry_ptr)entry_ptr;
        remap_next_ptr(ptr->next, a_per_instantiation_needed_flags_entry_ptr,
                       iek_per_instantiation_needed_flags_entry);
#endif /* !DO_SUBTREE_WALK */
      }
      break;
#endif /* ONE_INSTANTIATION_PER_OBJECT */

    case iek_id_name:
    case iek_string_text:
    case iek_other_text:
      /* String entries should go to walk_string_entry */
    case iek_none:
    case iek_last:
    default:
      unexpected_condition_str("walk_entry_and_subtree: bad entry kind");
  }  /* switch */
#if DO_SUBTREE_WALK
  /* Call the routine to process the entry if there is such a routine. */
  if (entry_process_func != NULL) entry_process_func(entry_ptr, entry_kind);
end_of_routine:;
#endif /* DO_SUBTREE_WALK */
}  /* walk_entry_and_subtree */

#undef remap_parent
#undef walk_source_corresp

#ifdef WALK_ORPHANED_ENTRY_ROUTINE_NAME

/*
Process a list of identical type orphaned file scope IL entries linked
together by the orphaned pointer preceding the IL entry structure.  Each
IL entry will be processed as an individual IL entry.  walk_ptr will be used
to process each entry in the list.  ptr is the pointer to the first IL entry,
ptr_type is the type of the pointer and entry_kind is the kind of entries.
*/
#undef walk_orphan_entry_list
#define walk_orphan_entry_list(ptr, ptr_type, entry_kind) \
{ ptr_type *orph_ptr = (ptr_type *)&(ptr); \
  for (; *orph_ptr != NULL; \
       orph_ptr = (ptr_type *)&fs_orphan_pointer_of(*orph_ptr)) { \
    walk_ptr(*orph_ptr, ptr_type, entry_kind) \
  }  /* for */ \
}  /* walk_orphan_entry_list */

/*
Local macro to ease stepping through the orphaned_file_scopes_il_entries
array and process the lists of IL entries of each type pointed to by the
"first_entry" of each array element.
*/
#undef walk_orphan_entry_list_for_entry_kind
#define walk_orphan_entry_list_for_entry_kind(ptr_type, entry_kind) \
  walk_orphan_entry_list( \
               orphaned_file_scope_il_entries[(int)entry_kind].first_entry, \
               ptr_type, entry_kind)


/* The routine name is a macro so it can be expanded different ways, e.g.,
   as walk_orphaned_file_scope_il_entries. */
static void WALK_ORPHANED_ENTRY_ROUTINE_NAME(void)
/*
For each IL entry kind, process any orphaned file scope IL entries chained
to the orphaned_file_scope_il_entries table.  As function scopes were walked,
any file scope IL entries referenced were added onto the list of orphaned
IL entries.  These IL entries may not be and probably are not referenced
from the file scope IL tree.  Walk through the lists of orphaned IL entries
of each kind.  
*/
{

  /* Process the list of individual IL entries of each IL type. */
  walk_orphan_entry_list_for_entry_kind(a_source_file_ptr, iek_source_file);
  walk_orphan_entry_list_for_entry_kind(a_constant_ptr, iek_constant);
  walk_orphan_entry_list_for_entry_kind(a_param_type_ptr, iek_param_type);
  walk_orphan_entry_list_for_entry_kind(a_routine_type_supplement_ptr,
                                        iek_routine_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_based_type_list_member_ptr,
                                        iek_based_type_list_member);
  walk_orphan_entry_list_for_entry_kind(a_type_ptr, iek_type);
  walk_orphan_entry_list_for_entry_kind(a_variable_ptr, iek_variable);
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_field_ptr, iek_field);
  walk_orphan_entry_list_for_entry_kind(an_exception_specification_ptr,
                                        iek_exception_specification);
  walk_orphan_entry_list_for_entry_kind(an_exception_specification_type_ptr,
                                        iek_exception_specification_type);
#endif /* ifdef CFE */
  walk_orphan_entry_list_for_entry_kind(a_routine_ptr, iek_routine);
  walk_orphan_entry_list_for_entry_kind(a_label_ptr, iek_label);
  walk_orphan_entry_list_for_entry_kind(an_expr_node_ptr, iek_expr_node);
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_for_loop_ptr, iek_for_loop);
  walk_orphan_entry_list_for_entry_kind(a_switch_clause_ptr,
                                        iek_switch_clause);
  walk_orphan_entry_list_for_entry_kind(a_handler_ptr, iek_handler);
  walk_orphan_entry_list_for_entry_kind(a_try_supplement_ptr,
                                        iek_try_supplement);
#if MICROSOFT_EXTENSIONS_ALLOWED
  walk_orphan_entry_list_for_entry_kind(a_microsoft_try_supplement_ptr,
                                        iek_microsoft_try_supplement);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
  walk_orphan_entry_list_for_entry_kind(a_block_ptr, iek_block);
  walk_orphan_entry_list_for_entry_kind(a_statement_ptr, iek_statement);
  walk_orphan_entry_list_for_entry_kind(an_object_lifetime_ptr,
                                        iek_object_lifetime);
  walk_orphan_entry_list_for_entry_kind(a_scope_ptr, iek_scope);
  /* The string types iek_id_name, iek_string_text, and iek_other_text
     are not maintained on an orphan list.  String types at the file
     scope that are referenced from a function scope are written in that
     function scope region. */
#if defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED
  walk_orphan_entry_list_for_entry_kind(an_internal_complex_value_ptr,
                                        iek_internal_complex_value);
#endif /* defined(FFE) || C99_IL_EXTENSIONS_SUPPORTED */
#ifdef FFE
  walk_orphan_entry_list_for_entry_kind(a_bound_info_entry_ptr,
                                        iek_bound_info_entry);
  walk_orphan_entry_list_for_entry_kind(a_do_loop_ptr, iek_do_loop);
  walk_orphan_entry_list_for_entry_kind(a_label_list_entry_ptr,
                                        iek_label_list_entry);
  walk_orphan_entry_list_for_entry_kind(an_io_specifier_ptr, iek_io_specifier);
  walk_orphan_entry_list_for_entry_kind(an_io_list_item_ptr, iek_io_list_item);
  walk_orphan_entry_list_for_entry_kind(a_namelist_group_member_ptr,
                                        iek_namelist_group_member);
  walk_orphan_entry_list_for_entry_kind(a_namelist_group_ptr,
                                        iek_namelist_group);
  walk_orphan_entry_list_for_entry_kind(an_input_output_description_ptr,
                                        iek_input_output_description);
  walk_orphan_entry_list_for_entry_kind(an_entry_param_ptr, iek_entry_param);
  walk_orphan_entry_list_for_entry_kind(an_entry_description_ptr,
                                        iek_entry_description);
#endif /* ifdef FFE */
#ifdef CFE
  walk_orphan_entry_list_for_entry_kind(a_namespace_ptr, iek_namespace);
  walk_orphan_entry_list_for_entry_kind(a_using_decl_ptr, iek_using_decl);
  walk_orphan_entry_list_for_entry_kind(a_dynamic_init_ptr, iek_dynamic_init);
  walk_orphan_entry_list_for_entry_kind(an_overriding_virtual_function_ptr,
                                        iek_overriding_virtual_function);
  walk_orphan_entry_list_for_entry_kind(a_derivation_step_ptr,
                                        iek_derivation_step);
  walk_orphan_entry_list_for_entry_kind(a_base_class_derivation_ptr,
                                        iek_base_class_derivation);
  walk_orphan_entry_list_for_entry_kind(a_base_class_ptr, iek_base_class);
  walk_orphan_entry_list_for_entry_kind(a_class_list_entry_ptr,
                                        iek_class_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_routine_list_entry_ptr,
                                        iek_routine_list_entry);
  walk_orphan_entry_list_for_entry_kind(a_class_type_supplement_ptr,
                                        iek_class_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_template_param_type_supplement_ptr,
                                        iek_template_param_type_supplement);
  walk_orphan_entry_list_for_entry_kind(a_constructor_init_ptr,
                                        iek_constructor_init);
  walk_orphan_entry_list_for_entry_kind(an_asm_entry_ptr, iek_asm_entry);
  walk_orphan_entry_list_for_entry_kind(a_template_arg_ptr, iek_template_arg);
  walk_orphan_entry_list_for_entry_kind(a_new_delete_supplement_ptr,
                                        iek_new_delete_supplement);
  walk_orphan_entry_list_for_entry_kind(a_throw_supplement_ptr,
                                        iek_throw_supplement);
#if !ABI_CHANGES_FOR_RTTI
  walk_orphan_entry_list_for_entry_kind(an_accessible_base_class_ptr,
                                        iek_accessible_base_class);
#endif /* !ABI_CHANGES_FOR_RTTI */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  walk_orphan_entry_list_for_entry_kind(an_eh_prologue_supplement_ptr,
                                        iek_eh_prologue_supplement);
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifdef CFE */
  walk_orphan_entry_list_for_entry_kind(a_template_parameter_ptr,
                                        iek_template_parameter);
  walk_orphan_entry_list_for_entry_kind(a_template_decl_ptr,
                                        iek_template_decl);
#if RECORD_FORM_OF_NAME_REFERENCE
  walk_orphan_entry_list_for_entry_kind(a_name_reference_ptr,
                                        iek_name_reference);
  walk_orphan_entry_list_for_entry_kind(a_name_qualifier_ptr,
                                        iek_name_qualifier);
#endif /* RECORD_FORM_OF_NAME_REFERENCE */
  /* Note that no orphan list walking is needed for iek_source_sequence_entry
     nor for its subordinate entries like iek_src_seq_secondary_decl
     and iek_src_seq_end_of_construct, since such entries will
     never appear on an orphan list.  Ditto for iek_src_seq_sublist. */
  /* Likewise iek_per_instantiation_needed_flags_entry. */
}  /* walk_orphaned_file_scope_il_entries */

#endif /* ifdef WALK_ORPHANED_ENTRY_ROUTINE_NAME */

#if !DO_SUBTREE_WALK
#if REMAP_ONLY_ROUTINES_NEEDED

void remap_il_header_pointers(a_remap_function_ptr remap_function,
                              a_remap_function_ptr list_remap_function)
/*
Remap the pointers in il_header by running them through the indicated
remapping routines.  list_remap_function is used for start-of-list
pointers.  The subtree is not processed.
*/
{
  a_remap_function_ptr saved_walk_remap_func = walk_remap_func;
  a_remap_function_ptr saved_walk_list_remap_func = walk_list_remap_func;

  walk_remap_func = remap_function;
  walk_list_remap_func = list_remap_function;
  remap_list_ptr(il_header.primary_source_file, a_source_file_ptr,
                 iek_source_file);
  remap_ptr(il_header.primary_scope, a_scope_ptr, iek_scope);
  remap_ptr(il_header.main_routine, a_routine_ptr, iek_routine);
  remap_ptr(il_header.compiler_version, a_char_ptr, iek_other_text);
  remap_ptr(il_header.time_of_compilation, a_char_ptr, iek_other_text);
  remap_list_ptr(il_header.scope_orphaned_list_headers,
                 a_scope_orphaned_list_header_ptr,
                 iek_scope_orphaned_list_header);
#if RECORD_MACROS_IN_IL
  remap_list_ptr(il_header.macros, a_macro_ptr, iek_macro);
#endif /* RECORD_MACROS_IN_IL */
#if ONE_INSTANTIATION_PER_OBJECT
  remap_ptr(il_header.instantiation_dir_name, a_char_ptr, iek_other_text);
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  remap_list_ptr(il_header.nontag_types_used_in_exception_or_rtti, a_type_ptr,
                 iek_type);
  /* region_scope_entry should not be changed; it's not a pointer into
     IL memory in the usual way.  It's changed explicitly as needed. */
  walk_remap_func = saved_walk_remap_func;
  walk_list_remap_func = saved_walk_list_remap_func;
}  /* remap_il_header_pointers. */

#endif /* REMAP_ONLY_ROUTINES_NEEDED */
#endif /* !DO_SUBTREE_WALK */

#ifdef UNDEF_WALK_ENTRY_MACROS_AT_END
/*
Get rid of the macros defined in this file so they aren't used accidentally.
*/
#undef remap_ptr
#undef remap_list_ptr
#undef remap_next_ptr
#undef remap_ptr_not_needed
#undef walk_ptr
#undef walk_list_ptr
#undef walk_ptr_not_needed
#undef walk_string_ptr
#undef walk_list_on_link_field
#undef walk_list
#undef walk_list_not_needed
#undef walk_needed_on_list
#undef walk_list_with_keep_in_il_reset
#undef definition_needed_if_class
#undef set_proper_definition_needed_flag
#undef set_proper_routine_definition_needed_flag
#undef remap_parent
#undef remap_source_sequence_entry
#undef walk_source_corresp
#undef walk_unmangled_name
#undef conditionally_clear_fe_pointer
#undef report_bad_init_kind
#undef walk_initializer
#undef walk_orphan_entry_list
#undef walk_orphan_entry_list_for_entry_kind
#undef walk_name_reference_list
#endif /* ifdef UNDEF_WALK_ENTRY_MACROS_AT_END */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
