/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2003 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_walk.h -- Declarations related to il_walk.c (walking the intermediate
             language tree).

*/

/* Avoid including these declarations more than once. */
#ifndef IL_WALK_H
#define IL_WALK_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* Type of function called to process each non-string entry.  First arg
   is the (new) pointer to the entry and second is the kind of entry. */
typedef void an_entry_process_function(char *, an_il_entry_kind);
typedef an_entry_process_function *an_entry_process_function_ptr;

#if IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS

/* Type of function called to process each string entry.  First arg
   is the (new) pointer to the entry, second is the kind of entry, and
   third is the string length in bytes. */
typedef void a_string_entry_process_function(char *, an_il_entry_kind, 
                                             sizeof_t);
typedef a_string_entry_process_function *a_string_entry_process_function_ptr;
/* Type of function called to remap an old entry pointer to a new entry
   pointer. */
typedef char *a_remap_function(char *, an_il_entry_kind);
typedef a_remap_function *a_remap_function_ptr;
/* Type of function called to test for termination in the IL walk.
   First parameter is the pointer to the entry and second is the kind
   of entry.  Returned value of TRUE means prune the walk at this entry. */
typedef a_boolean a_walk_termination_test_function(char *, an_il_entry_kind);
typedef a_walk_termination_test_function *a_walk_termination_test_function_ptr;

/*
If this flag is TRUE, the IL walk routines that allow remapping of the
pointers in an entry in isolation (i.e., not as part of an IL tree walk) are
compiled.  Note that the top-level routine remap_pointers_in_il_entry is
compiled regardless of the setting of this flag, because it is used
by the trans_copy.c code.
*/
#if IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define REMAP_ONLY_ROUTINES_NEEDED TRUE
#else /* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ...) */
#define REMAP_ONLY_ROUTINES_NEEDED FALSE
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE && ... */

#ifdef FFE
/*
Array bound information entries cause some problems, because they are
laid out as a variable-length array of fixed-length entries.  In the
alternate file format, there is no room preceding each entry for the
storage of the entry number.  To deal with this, the IL walk routines
make the current index in the array of bound info entries available
so that one can tell which entry one is dealing with.  num_walk_array_bounds
indicates the total number of entries.
*/
EXTERN unsigned long array_bound_walk_index;
EXTERN unsigned long num_walk_array_bounds;
#endif /* ifdef FFE */

EXTERN unsigned int
		flag_value_meaning_visited;
			/* Value to be placed in the il_walk_flag field
			   to indicate that an entry has been visited.
			   The value alternates between 0 and 1. */

#if IL_WALK_NEEDED 

/* Walk the intermediate language tree for the file scope. */
extern void walk_file_scope_il(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

/* Walk the intermediate language tree for a routine scope. */
extern void walk_routine_scope_il(
            a_memory_region_number               region_number,
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers);

/* Walk a subtree of the IL. */
extern void walk_il_subtree(
            an_entry_process_function_ptr        entry_process_function,
            a_string_entry_process_function_ptr  string_entry_process_function,
            a_remap_function_ptr                 remap_function,
            a_remap_function_ptr                 list_remap_function,
            a_walk_termination_test_function_ptr termination_test_function,
            a_boolean                            clear_fe_pointers,
            char                                 *ptr,
            an_il_entry_kind                     kind);

#endif /* IL_WALK_NEEDED */

#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
#if ONE_INSTANTIATION_PER_OBJECT
extern void set_per_instantiation_needed_flag(char             *entry_ptr,
                                              an_il_entry_kind entry_kind,
                                              unsigned long    bit_number);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void mark_as_needed(char             *entry_ptr,
                           an_il_entry_kind entry_kind);

extern void mark_as_needed_like(char                    *entry_ptr,
                                an_il_entry_kind        entry_kind,
                                a_source_correspondence *model_scp,
                                a_boolean               set_class_defn_needed);

extern void remark_as_needed(char             *entry_ptr,
                             an_il_entry_kind entry_kind);

extern void mark_to_keep_in_il(char             *entry_ptr,
                               an_il_entry_kind entry_kind);

extern void remark_routine_definition_needed(a_routine_ptr rout);

extern void set_routine_definition_needed(a_routine_ptr rout);

extern void set_class_definition_needed(a_type_ptr type);

extern void set_routine_keep_definition_in_il(a_routine_ptr rout);

extern void set_class_keep_definition_in_il(a_type_ptr type);

extern void walk_subtrees_of_local_entities(a_scope_ptr scope);

EXTERN a_boolean
		end_of_file_scope_needed_flags_phase;
			/* TRUE during the phase at the end of the file scope
			   that deals with walking the subtrees of variables
			   and classes to set needed flags. */
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */

extern
void remap_pointers_in_il_entry(char                 *entry_ptr,
                                an_il_entry_kind     entry_kind,
                                a_remap_function_ptr remap_function,
                                a_remap_function_ptr list_remap_function);

#if REMAP_ONLY_ROUTINES_NEEDED
extern void remap_il_header_pointers(a_remap_function_ptr remap_function,
                                     a_remap_function_ptr list_remap_function);

extern void remap_first_ptr_of_orphaned_file_scope_entry_array(
                                          a_remap_function_ptr remap_function);
#endif /* REMAP_ONLY_ROUTINES_NEEDED */

#if IL_SHOULD_BE_WRITTEN_TO_FILE
extern void remap_last_ptr_of_orphaned_file_scope_entry_array(
                                          a_remap_function_ptr remap_function);
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */

extern void il_walk_init(void);

#endif /* IL_WALK_NEEDED || MAINTAIN_NEEDED_FLAGS */

#if NEED_DECLARATIVE_WALK

extern void walk_declarative_entities_in_scope(
                         a_scope_ptr                   scope,
                         an_entry_process_function_ptr entry_process_function);

#endif /* NEED_DECLARATIVE_WALK */

typedef void a_type_list_processing_routine(a_type_ptr type_list);
typedef a_type_list_processing_routine *a_type_list_processing_routine_ptr;

extern void process_local_types(
                   a_scope_ptr                        scope,
                   a_type_list_processing_routine_ptr list_processing_routine);


/*
Types for expression and statement traversal routines.
*/
typedef struct an_expr_or_stmt_traversal_block
		*an_expr_or_stmt_traversal_block_ptr;
/* Type of function called to process an expression node. */
typedef void a_traversal_expr_process_function(
                                          an_expr_node_ptr,
                                          an_expr_or_stmt_traversal_block_ptr);
typedef a_traversal_expr_process_function
		*a_traversal_expr_process_function_ptr;
/* Type of function called to process a constant. */
typedef void a_traversal_constant_process_function(
                                          a_constant_ptr,
                                          an_expr_or_stmt_traversal_block_ptr);
typedef a_traversal_constant_process_function
		*a_traversal_constant_process_function_ptr;
/* Type of function called to process a dynamic initialization. */
typedef void a_traversal_dynamic_init_process_function(
                                          a_dynamic_init_ptr,
                                          an_expr_or_stmt_traversal_block_ptr);
typedef a_traversal_dynamic_init_process_function
		*a_traversal_dynamic_init_process_function_ptr;
/* Type of function called to process a statement. */
typedef void a_traversal_statement_process_function(
                                          a_statement_ptr,
                                          an_expr_or_stmt_traversal_block_ptr);
typedef a_traversal_statement_process_function
		*a_traversal_statement_process_function_ptr;
typedef struct an_expr_or_stmt_traversal_block {
  /* If you add fields here, also add them to
     clear_expr_or_stmt_traversal_block. */
  /* For the callback routines, if the pointer is NULL no routine is
     called.  The tree is still traversed below that node. */
  a_traversal_expr_process_function_ptr
		process_expr;
			/* Function called for each expression node. */
  a_traversal_constant_process_function_ptr
		process_constant;
			/* Function called for each constant. */
  a_traversal_dynamic_init_process_function_ptr
		process_dynamic_init;
			/* Function called for each dynamic init. */
  a_traversal_statement_process_function_ptr
		process_statement;
			/* Function called for each statement. */
  a_boolean	terminate;
			/* A called routine can set this to TRUE to
			   terminate the tree walk. */
  a_boolean	suppress_subtree_walk;
			/* A called routine can set this to TRUE to
			   suppress the walk of the subtree of the
			   current entry. */
  a_boolean	result;
			/* A place for called routines to store a boolean
			   result for the overall walk. */
  a_boolean	process_non_dynamic_constants;
			/* If TRUE, constants that are not dynamic (i.e.,
			   that do not potentially include executable code/
			   expressions) are also walked.  Ordinarily, they
			   are not walked because we are primarily looking for
			   expressions and statements. */
} an_expr_or_stmt_traversal_block;

extern void clear_expr_or_stmt_traversal_block(
                                   an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_constant(a_constant_ptr                      constant,
                              an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_dynamic_init(a_dynamic_init_ptr                  dip,
                                  an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_expr(an_expr_node_ptr                    expr,
                          an_expr_or_stmt_traversal_block_ptr tblock);

extern void traverse_statement(a_statement_ptr                     statement,
                               an_expr_or_stmt_traversal_block_ptr tblock);

#endif /* ifndef IL_WALK_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2003 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
