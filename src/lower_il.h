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

lower_il.h -- Declarations related to lower_il.c (having to do with
              lowering C++ intermediate language to C intermediate language).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_IL_H
#define LOWER_IL_H 1

/* Only include this code if it is needed.  A few routines are needed
   if name mangling is needed, even if IL lowering is not. */
/* NEED_NAME_MANGLING is always TRUE if DO_IL_LOWERING is TRUE. */
#if NEED_NAME_MANGLING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */


extern void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta);

extern void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset);

extern char *alloc_lowered_name_string(sizeof_t size);

#if DO_IL_LOWERING

EXTERN a_boolean
		lowering_file_scope;
			/* TRUE if lowering the file scope's IL, FALSE if
			   lowering a routine scope's IL. */
EXTERN a_boolean
		keep_object_lifetime_info_in_lowered_il;
			/* TRUE if object lifetime information should be
			   preserved by the lowering process (so a back end
			   can use it, e.g., for exception handling). */


/*
Access the il_lowering_flag in an IL entry.
*/
#define il_lowering_flag_of(entry_ptr)                                \
  (il_entry_prefix_of(entry_ptr).il_lowering_flag)

/*
Macro that tests whether or not a given entry has been visited yet.
*/
#define visited_yet(entry_ptr) (il_lowering_flag_of(entry_ptr))

/*
Set the flag to indicate that an entry has been visited.
*/
#define mark_as_visited(entry_ptr) (il_lowering_flag_of(entry_ptr) = TRUE)

/*
Set the flag to indicate that an entry has not been visited.  Used
when a just-allocated entry requires lowering.
*/
#define mark_as_not_visited(entry_ptr) (il_lowering_flag_of(entry_ptr) = FALSE)


typedef unsigned long a_cleanup_region_number;
			/* Number for a destructible region, used for
			   exception handling cleanup. */
typedef unsigned long a_handle_number;
			/* Number in the region table that identifies an
			   entry in the object address table or in the array
			   table. */

/*
Types used to describe a position within an initialization:
*/
typedef struct an_init_pos_modifier *an_init_pos_modifier_ptr;
typedef struct an_init_pos_modifier {
  /* Modifier for an_init_pos_descr.  Usually allocated on the stack, but
     allocated on the heap when saved as part of a cleanup action entry. */
  an_init_pos_modifier_ptr
		next;
			/* Pointer to the similar entry at the next
			   level out. */
  a_type_ptr	type;
			/* Type of entity being initialized at this level. */
  a_targ_size_t	curr_elem;
			/* If the entity is an array, this is the number of
			   the element currently being initialized.  Ignored
			   unless curr_field == NULL and curr_base == NULL. */
  a_field_ptr	curr_field;
			/* If the entity is a struct or union, this points
			   to the field currently being initialized.  NULL
			   otherwise. */
  a_base_class_ptr
		curr_base;
			/* If the entity is a base class, this points to the
			   base class entry.  NULL otherwise. */
} an_init_pos_modifier;

EXTERN an_init_pos_modifier_ptr
		avail_init_pos_modifiers;
			/* List of initialization position modifier entries
			   that have been freed and are available for reuse. */

typedef struct an_init_pos_descr *an_init_pos_descr_ptr;
typedef struct an_init_pos_descr {
  /* An initialization position description.  Starts with a variable (the
     variable itself or what it points to).  That base address may be
     modified by a modifiers list. */
  a_variable_ptr
		variable;
			/* The base variable. */
  a_byte_boolean
		indirect_through_variable;
			/* If TRUE, variable is a pointer and its value gives
			   the base address. */
  a_byte_boolean
		whole_array;
			/* TRUE if the entity is a whole array being
			   initialized as one unit. */
  a_type_ptr	base_type;
			/* Base entity type. */
  an_init_pos_modifier_ptr
		modifiers;
			/* Optional list of modifiers of the base variable,
			   NULL if none.  In order from innermost to outermost
			   modifier. */
  a_targ_ptrdiff_t
		array_element_count;
			/* If whole_array is TRUE, the count of elements in
			   the array, or -1 for an unknown-length array
			   (new/delete only).  Zero otherwise. */
} an_init_pos_descr;

typedef struct a_destructible_entity_descr *a_destructible_entity_descr_ptr;
typedef struct a_destructible_entity_descr {
  /* Description of an entity that requires destructor.  Dynamically
     allocated and pointed to from a_dynamic_init.  Contains the information
     IL lowering needs in addition to the dynamic init entry to destroy
     an entity. */
  a_destructible_entity_descr_ptr
		next;	/* Pointer to next entry when on an available list. */
  an_init_pos_descr
		init_pos_descr;
			/* Location of the entity. */
  a_variable_ptr
		conditional_flag_var;
			/* If non-NULL, points to a variable that is the
			   conditional flag variable that is set to non-zero
			   to indicate that the initialization has been
			   done. */
  a_handle_number
		conditional_flag_handle;
			/* If conditional_flag_var is non-NULL, this is
			   the object table index number for the conditional
			   flag variable. */
  a_cleanup_region_number
		region_number;
			/* When exceptions are enabled, this is the
			   destructible object region number, i.e., the index
			   into the region table.  This is the region number
			   to establish as the current region number once the
			   construction of the entity has been finished.
			   Usually, this is set when the region table entry
			   is created, but for constructor-inits in a
			   destructor it is preassigned.  Note that if the
			   initialization/destruction is part of an unordered
			   set, this number will be the number of the first
			   member of the set. */
  a_cleanup_region_number
		region_number_to_set_when_starting_destruction;
			/* When destroying this entity when exceptions are
			   enabled, this is the region number to establish
			   as the current region number when beginning the
			   destruction.  It's the next region table entry to
			   process after this entity is destroyed. */
  a_constant_ptr
		region_table_entry;
			/* When exceptions are enabled, this points to the
			   aggregate constant that defines the region table
			   entry for the destruction of this entity. */
  a_dynamic_init_ptr
		next_in_region_table;
			/* When exceptions are enabled, this points to the
			   initialization that follows this one in destruction
			   order.  Usually, this is the same as the
			   next_in_destruction_list pointer in the dynamic
			   initialization itself, but in the presence of
			   unordered initializations the IL, lowering traversal
			   order (reflected by this pointer) might be
			   slightly different than the front end order
			   (reflected by the dynamic init
			   next_in_destruction_list pointer).  Also different
			   when region table entries are cloned because of
			   long lifetime temporaries.  Also, this is not
			   always the same as the value in the "next"
			   field in the constant pointed to by
			   region_table_entry, when unordered entries are
			   involved.  Also, this does not leave a lifetime,
			   whereas the previous entry in the region table
			   might be from a previous lifetime. */
			   
} a_destructible_entity_descr;

EXTERN a_destructible_entity_descr_ptr
		avail_destructible_entity_descrs;
			/* List of destructible entity descriptions that
			   have been freed and are available for reuse. */

#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_init_pos_modifiers_allocated,
		num_destructible_entity_descrs_allocated;
#endif /* DEBUG */


/*
Structure put on a list to remember the locations of all return statements
in the current routine, so they can be rewritten to execute epilogue code.
*/
typedef struct a_return_memo *a_return_memo_ptr;
typedef struct a_return_memo {
  a_return_memo_ptr
		next;	/* Next entry on the list, or NULL if this is the
			   last. */
  a_statement_ptr
		stmt;
			/* Pointer to a return statement. */
} a_return_memo;


/*
Entry used to describe an insert location within a statement or expression
tree.
*/
typedef enum an_insert_location_kind {
  /* Kind of insert location: */
  ilk_after_statement,	/* Insert after a statement. */
  ilk_block_start,	/* Insert at the start of a block. */
  ilk_switch_clause_start,
			/* Insert at the start of a switch clause. */
  ilk_before_expr,	/* Insert before an expression. */
  ilk_after_expr	/* Insert after an expression. */
} an_insert_location_kind;
/* Test for the insertion kinds for insertions within expressions. */
#define is_expr_insert_location_kind(kind)                            \
 ((kind) == ilk_before_expr || (kind) == ilk_after_expr)
typedef struct an_insert_location *an_insert_location_ptr;
typedef struct an_insert_location {
  an_insert_location_kind
		kind;	/* Kind of insert location: after expression,
			   after statements, etc. */
  union {
    /* When kind == ilk_after_statement or kind == ilk_block_start: */
    a_statement_ptr
		stmt;	/* The statement to insert after, or the block to
			   insert at the start of.  In the ilk_after_statement
			   case, the statement must be part of a statement
			   sequence, not, for example, the dependent statement
			   of an "if". */
    /* When kind == ilk_switch_clause_start: */
    a_switch_clause_ptr
		switch_clause;
			/* The switch clause to insert at the start of. */
    /* When kind == ilk_before_expr or kind == ilk_after_expr: */
    an_expr_node_ptr
		expr;	/* The expression to insert before or after. */
  } variant;
} an_insert_location;

/*
Entry used to keep track of the context during the lowering operation.
A linked list of these runs from the current point back through the stack
to the outermost invocations, giving a history of the IL parents of
the IL object currently being considered.
*/
typedef struct a_context *a_context_ptr;
typedef struct a_context {
  a_context_ptr parent;	/* Parent context. */
  a_scope_ptr	scope;	/* Scope associated with this context. */
  an_object_lifetime_ptr
		lifetime;
			/* Object lifetime associated with this context.
			   If the context doesn't define a lifetime, the
			   lifetime is inherited from the parent.  NULL
			   only if there are no lifetimes at all, all
			   the way up. */
  a_byte_boolean
		new_lifetime;
			/* TRUE if this context entry defines a new object
			   lifetime (i.e., it has a lifetime and the lifetime
			   is not inherited from the parent context). */
  a_dynamic_init_ptr
		latest_initialization;
			/* The current position in the destructions list
			   of the lifetime, i.e., the latest encountered
			   dynamic initialization requiring destruction.
			   Note that this is not updated when destructions
			   are generated (e.g., at the end of a block or
			   on a goto or return), so it stays indicating
			   the "most-constructed" state.  That's different
			   than curr_cleanup_region_number, which is
			   updated on destructions. */
  an_object_lifetime_ptr
		saved_curr_object_lifetime;
			/* Used to save/restore the global variable
			   curr_object_lifetime over push_context/
			   pop_context. */
  a_cleanup_region_number
		saved_curr_cleanup_region_number;
			/* Used to save/restore the global variable
			   curr_cleanup_region_number over push_context/
			   pop_context. */
  an_object_lifetime_ptr
		successor_lifetime_at_statement;
			/* If the object lifetime has a successor that begins
			   at a label, this is the lifetime.  This helps us
			   watch for the appearance of the associated
			   statement, since there is no explicit indication
			   the the statement that it begins another
			   lifetime. */
  a_variable_ptr
		try_frame;
			/* For a context associated with a "try" block, this
			   points to the variable for the stack frame for the
			   try. */
} a_context;

EXTERN a_context_ptr
		curr_context;
			/* Current (bottom) end of the context chain. */
EXTERN a_context_ptr
		file_scope_context;
			/* The context for the file scope. */

EXTERN a_return_memo_ptr
		return_memo_list;
			/* List of return statements found in the current
			   routine, maintained so that epilogue code can be
			   added. */

EXTERN a_variable_ptr
		return_value_pointer_variable;
			/* While processing a routine that returns its
			   value via a copy constructor, this points to
			   the parameter variable for the implicit parameter
			   through which the caller sends the address
			   at which the result will be stored; NULL
			   otherwise. */

/*
Return TRUE if the indicated variable is the return value optimization
variable for the current function.
*/
#define var_is_return_value_variable(var)                             \
  (innermost_function_scope != NULL &&                                \
   innermost_function_scope->variant.routine.return_value_variable == (var))


EXTERN a_source_position
		code_pos_for_lowering;
			/* The source position associated with executable code
			   currently being lowered. */


extern a_boolean il_lowering_needed(void);

extern void pop_context(void);

extern void push_context(a_context              *context,
                         a_scope_ptr            scope,
                         an_object_lifetime_ptr lifetime);

extern void set_insert_location(a_statement_ptr    stmt,
                                an_insert_location *insert_location);

extern void set_block_start_insert_location(
                                          a_statement_ptr    stmt,
                                          an_insert_location *insert_location);

extern void set_expr_insert_location(an_expr_node_ptr   node,
                                     an_insert_location *insert_location);

extern void finish_class_type(a_type_ptr    class_type, 
                              a_targ_size_t *byte_offset);

extern void add_to_front_of_file_scope_types_list(a_type_ptr type);

extern a_type_ptr underlying_type(a_type_ptr type);

extern a_boolean is_or_was_ptr_to_data_member_type(a_type_ptr type);

extern an_expr_node_ptr au_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern an_expr_node_ptr make_base_class_lvalue(
                                             an_expr_node_ptr node,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);

extern an_expr_node_ptr make_base_class_lvalue_from_var(
                                             a_variable_ptr   var,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);

extern an_expr_node_ptr add_cast(an_expr_node_ptr node,
                                 a_type_ptr       new_type);

extern void change_to_cast(an_expr_node_ptr node,
                           an_expr_node_ptr operand_node,
                           a_type_ptr       new_type);

extern an_expr_node_ptr array_var_lvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr make_node_for_il_constant(a_constant_ptr constant);

extern an_expr_node_ptr make_vbptr_field_lvalue_from_var(a_variable_ptr   var,
                                                         a_base_class_ptr bcp);

extern an_expr_node_ptr make_vptr_field_lvalue(an_expr_node_ptr node);

extern an_expr_node_ptr make_vptr_field_lvalue_from_var(a_variable_ptr var);

extern an_expr_node_ptr make_vbase_class_lvalue_from_var(
                                             a_variable_ptr   var,
                                             a_base_class_ptr bcp,
                                             a_boolean        complete_object);

extern an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                              a_type_ptr       new_type);

extern an_expr_node_ptr make_reusable_copy(an_expr_node_ptr expr,
                                           a_boolean        vars_can_change);

extern void insert_expr(an_expr_node_ptr       inserted_expr,
                        an_insert_location_ptr insert_location);

extern void insert_statement(a_statement_ptr        statement,
                             an_insert_location_ptr insert_location);

extern a_statement_ptr make_call_statement(a_routine_ptr    routine,
                                           an_expr_node_ptr arg_list);

extern a_variable_ptr make_unnamed_local_static_variable(
                                                 a_type_ptr type,
                                                 a_boolean  in_function_scope);

extern a_variable_ptr make_lowered_variable(char            *var_name,
                                            a_boolean       already_il_name,
                                            a_type_ptr      var_type,
                                            a_storage_class var_storage_class);

extern a_variable_ptr make_lowered_param_variable(a_type_ptr type);

#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
extern a_variable_ptr make_instantiation_var(
                                      char                    *prefix,
                                      an_integer_kind         ikind,
                                      a_source_correspondence *source_corresp);
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

extern a_variable_ptr make_temporary_in_scope(a_type_ptr  temp_type,
                                              a_scope_ptr scope,
                                              a_boolean   force_static);

extern a_variable_ptr make_lowered_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_file_scope_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_function_scope_temporary(a_type_ptr temp_type);

extern void make_lowered_field(char          *field_name,
                               a_type_ptr    field_type,
                               a_targ_size_t *byte_offset,
                               a_type_ptr    struct_type,
                               a_field_ptr   *last_field);

extern a_type_ptr void_star_type(void);

extern a_type_ptr char_star_type(void);

extern a_type_ptr make_vptp_type(void);

extern void overwrite_node(an_expr_node_ptr node,
                           an_expr_node_ptr source_node);

extern void set_integer_constant_with_overflow_check(
                                              a_constant_ptr  con,
                                              long            con_val,
                                              an_integer_kind ikind);

extern void set_unsigned_integer_constant_with_overflow_check(
                                              a_constant_ptr  con,
                                              unsigned long   con_val,
                                              an_integer_kind ikind);

/* See also below -- this is defined as a macro if IL lowering is
   configured out. */
extern a_boolean virtual_dtor_should_be_generated_for_class(
                                                        a_type_ptr class_type);

extern void add_to_return_memo_list(a_statement_ptr return_stmt);

extern void free_return_memo_list(a_return_memo_ptr rmp);

extern void turn_statement_into_block(a_statement_ptr        statement,
                                      an_insert_location_ptr insert_location,
                                      a_statement_ptr        *orig_statement);

extern void turn_branch_into_block(a_statement_ptr        statement,
                                   an_insert_location_ptr insert_location,
                                   a_statement_ptr        *orig_statement);

extern void gen_cleanup_actions(an_object_lifetime_ptr outer_lifetime,
                                an_insert_location_ptr insert_location);

extern void prelower_class_type(a_type_ptr class_type);

extern void lower_ptr_to_member_constant(a_constant_ptr constant);

extern void lower_constant(a_constant_ptr constant);

extern void lower_type(a_type_ptr type);

/*
A "possibly other scope" version of lower_type; does nothing for
types in other scopes.  Note that because all types are in the file
scope, any reference to a type while lowering a function is a
reference to another scope, and is recorded as a potential orphan
to be processed later.  Note that a class member can never be
an orphan, so member types are not recorded as orphans.
*/
#define lower_os_type(type)                                           \
{ if (!lowering_file_scope) {                                         \
    if (type->source_corresp.class_of_which_a_member == NULL) {       \
      add_orphaned_file_scope_il_entry((char *)(type),                \
                                       (an_il_entry_kind)iek_type);   \
    }  /* if */                                                       \
  } else {                                                            \
    lower_type(type);                                                 \
  }  /* if */                                                         \
}  /* lower_os_type */


extern void lower_expr_list(an_expr_node_ptr expr_list,
                            unsigned int     is_lvalue_mask);

extern void lower_expr(an_expr_node_ptr expr,
                       a_boolean        is_lvalue);

#define lower_normal_expr(expr) lower_expr((expr), /*is_lvalue=*/FALSE)

extern a_param_type_ptr unlowered_param_type_list(a_type_ptr routine_type);

extern void lower_arg_expr_list(an_expr_node_ptr expr_list,
                                a_type_ptr       called_rout_type,
                                a_param_type_ptr param);

extern an_expr_operator_kind lowered_assignment_operator(a_type_ptr type);

extern void lower_virtual_function_call(an_expr_node_ptr expr);

extern void lower_call(an_expr_node_ptr      expr,
                       an_init_pos_descr_ptr ipdp);

extern void begin_object_lifetime(
                              an_object_lifetime_ptr lifetime,
                              an_insert_location     *insert_location);

extern void begin_block_object_lifetime(
                                       an_object_lifetime_ptr lifetime,
                                       an_insert_location_ptr insert_location);

extern void lower_statement_list(a_statement_ptr statement_list,
                                 a_statement_ptr *last_statement);

extern void lower_statement(a_statement_ptr statement);

extern void lower_il_memory_region(a_memory_region_number region_number);

extern void eliminate_object_lifetime_tree(an_object_lifetime_ptr olp);

extern void clean_up_all_object_lifetimes(a_scope_ptr scope);

#if DEBUG
extern unsigned long show_lowering_space_used(void);
#endif /* DEBUG */

extern void il_lower_one_time_init(void);

extern void il_lower_init(void);

#endif /* DO_IL_LOWERING */
#endif /* NEED_NAME_MANGLING */

#if !DO_IL_LOWERING

/* IL lowering is disabled. */

/*
#define a dummy version of virtual_dtor_should_be_generated_for_class
which always returns TRUE (meaning a virtual destructor for a class
should always be generated).  This is the answer that does the most
error checking, but FALSE would be equally proper.
*/
#define virtual_dtor_should_be_generated_for_class(class_type) TRUE
#endif /* !DO_IL_LOWERING */

#endif /* ifndef LOWER_IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
