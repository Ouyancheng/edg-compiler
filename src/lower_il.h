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

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_DEF_H */


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


/*
Types used to describe a position within an initialization:
*/
typedef struct an_init_pos_modifier *an_init_pos_modifier_ptr;
typedef struct an_init_pos_modifier {
  /* Modifier for an_init_pos_descr.  Usually allocated on the stack, but
     allocated on the heap when saved as part of a required destructor
     call entry. */
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
  a_boolean	indirect_through_variable;
			/* If TRUE, variable is a pointer and its value gives
			   the base address. */
  a_type_ptr	base_type;
			/* Base entity type. */
  an_init_pos_modifier_ptr
		modifiers;
			/* Optional list of modifiers of the base variable,
			   NULL if none.  In order from innermost to outermost
			   modifier. */
  a_boolean	whole_array;
			/* TRUE if the entity is a whole array being
			   initialized as one unit. */
  long		array_element_count;
			/* If whole_array is TRUE, the count of elements in
			   the array, or -1 for an unknown-length array
			   (new/delete only).  Zero otherwise. */
} an_init_pos_descr;


/*
Entry used to record a destructor call that must be emitted on exit from
a scope.
*/
typedef struct a_required_destructor_call *a_required_destructor_call_ptr;
typedef struct a_required_destructor_call {
  a_required_destructor_call_ptr
		next;	/* Next entry on a list of required calls, NULL
			   if last. */
  a_label_ptr	label_marker;
			/* If this is non-NULL, this entry does not describe
			   a required destructor call; it is a marker that
			   indicates where in the list a label was declared.
			   the other fields (below) are not meaningful. */
  a_dynamic_init
		dynamic_init;
			/* The dynamic initialization entry that describes the
			   required destructor call.  Note that this is a copy
			   of the entire entry, not a pointer to it, because
			   the original entry may have been modified into
			   an entry that is valid in C. */
  a_variable_ptr
		first_time_test_var;
			/* If non-NULL, points to a first-time-test variable
			   which will be non-zero if the initialization has
			   been done.  This is needed for local static
			   variables and for temporaries initialized under
			   conditional operators. */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
  a_variable_ptr
		template_static_data_member_init_guard_var;
			/* If non-NULL, points to a variable tested in guard
			   code around the initialization and destruction of
			   a static data member of a template. */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
  an_init_pos_descr
		init_pos_descr;
			/* Description of the object to destroy. */
  a_byte_boolean
		is_expr_temporary;
			/* TRUE if the entity to be destroyed is a compiler-
			   generated expression temporary. */
  unsigned long	region_number;
			/* Destructible object region number for exception
			   handling. */
} a_required_destructor_call;

/*
Value used to indicate "no region number" for exception handling regions.
*/
#define NULL_EH_REGION_NUMBER (~(unsigned long)0)


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
Entry used to describe an insert position within a statement or expression
tree.
*/
typedef struct an_insert_location *an_insert_location_ptr;
typedef struct an_insert_location {
  a_byte_boolean
		expr_insert;
			/* If TRUE, the insertion is within an expression
			   tree; if FALSE, it's within a statement sequence. */
  union {
    /* When expr_insert == TRUE: */
    struct {
      an_expr_node_ptr
		ptr;	/* The expression relative to which the insertion
			   is to be done. */
      a_byte_boolean
		insert_before;
			/* If TRUE, the insertion is to be done before the
			   indicated expression. */
    } expr;
    struct {
      a_statement_ptr
		ptr;	/* The statement relative to which insertion is to be
			   done. */
      a_byte_boolean
		insert_at_block_start;
			/* If TRUE, "statement" points to an stmk_block
			   statement and insertion is to be done before the
			   first statement (if any) in that block.  If FALSE,
			   "statement" may be any kind of statement and
			   insertion is to be done following it.  Note that
			   in this latter case the statement pointed to must
			   be part of a statement sequence, not for example
			   the dependent statement of an "if". */
    } statement;
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
  a_byte_boolean
		subscope_region;
			/* TRUE if this context is for a region that is
			   not a full scope.  Used for dependent statements
			   in cfront compatibility mode, for first-time
			   test code for local static variables, and for
			   some expressions.  In all of those cases, any
			   temporary constructed within the region must
			   be destroyed at the end of the region.  The
			   scope field indicates the nearest enclosing
			   scope.  Note that if labels and gotos are
			   permitted within the region, all labels must
			   appear before all gotos. */
  an_expr_node_ptr
		assoc_expr;
			/* If non-NULL (only when subscope_region is TRUE),
			   this points to an expression that is the entire
			   subscope region, e.g., the test expression in
			   an stmk_for statement. */
  a_switch_clause_ptr
		assoc_switch_clause;
			/* Points to the current clause of a switch statement
			   if inside one; NULL otherwise. */
  a_required_destructor_call_ptr
		required_destructor_calls;
			/* Destructor calls required on exit from the scope. */
  a_statement_ptr
		latest_label_statement_processed;
			/* The stmk_label statement most recently processed
			   in (this clause of) the block, or NULL if none
			   has been processed. */
  a_byte_boolean
		any_conditional_destruction_var_initializations_deferred;
			/* TRUE if one or more initializations of
			   flag variables for conditional destructions were
			   deferred by add_conditional_destruction_temp.
			   Only happens when assoc_expr is non-NULL. */
} a_context;
EXTERN a_context_ptr
		curr_context;
			/* Current (bottom) end of the context chain. */
EXTERN a_scope_ptr
		nearest_function_scope;
			/* Nearest function scope, as pushed by
			   push_context. */
EXTERN a_context_ptr
		file_scope_context;
			/* The context for the file scope. */
EXTERN unsigned long
		num_conditional_exprs_inside_of;
			/* Count of conditional parts of expressions that we
			   are inside of.  Incremented on entering conditional
			   operands of "?:", "&&", and "||". */
EXTERN a_return_memo_ptr
		return_memo_list;
			/* List of return statements found in the current
			   routine, maintained so that epilogue code can be
			   added. */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
EXTERN sizeof_t	size_mangled_name_buffer /* = 0*/;
			/* Current allocated size of mangled_name_buffer.
			   Not per-file.  See lower_name.c for the definition
			   of mangled_name_buffer. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_init_pos_modifiers_allocated;
#endif /* DEBUG */


extern void pop_context(void);

extern void push_context(a_context   *context,
                         a_scope_ptr scope,
                         a_boolean   dependent_statement);

extern void set_insert_location(a_statement_ptr    stmt,
                                an_insert_location *insert_location);

extern void set_block_start_insert_location(
                                          a_statement_ptr    stmt,
                                          an_insert_location *insert_location);

extern void set_expr_insert_location(an_expr_node_ptr   node,
                                     an_insert_location *insert_location);

extern char *alloc_lowered_name_string(sizeof_t size);

extern void finish_class_type(a_type_ptr    class_type, 
                              a_targ_size_t *byte_offset);

extern void add_to_front_of_file_scope_types_list(a_type_ptr type);

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

extern a_variable_ptr make_unnamed_local_static_variable(a_type_ptr type);

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

extern a_variable_ptr make_lowered_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_file_scope_temporary(a_type_ptr temp_type);

extern a_variable_ptr make_temporary_possibly_at_file_scope(
                                                     a_type_ptr temp_type,
                                                     a_boolean  at_file_scope);
extern void make_lowered_field(char          *field_name,
                               a_type_ptr    field_type,
                               a_targ_size_t *byte_offset,
                               a_type_ptr    struct_type,
                               a_field_ptr   *last_field);

extern a_type_ptr void_star_type(void);

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

extern void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta);

extern void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset);

extern a_boolean virtual_dtor_should_be_generated_for_class(
                                                        a_type_ptr class_type);

extern a_required_destructor_call_ptr alloc_required_destructor_call(void);

extern void add_to_return_memo_list(a_statement_ptr return_stmt);

extern void free_return_memo_list(a_return_memo_ptr rmp);

extern void turn_statement_into_block(a_statement_ptr statement);

extern void turn_branch_into_block(a_statement_ptr        statement,
                                   an_insert_location_ptr insert_location,
                                   a_statement_ptr        *orig_statement);

extern void gen_required_destructor_calls(
                                   a_context_ptr          outer_context,
                                   an_insert_location_ptr insert_location);

extern void prelower_class_type(a_type_ptr class_type);

extern void lower_ptr_to_member_constant(a_constant_ptr constant);

extern void lower_constant(a_constant_ptr constant);

extern void lower_os_type(a_type_ptr type);

extern void lower_expr_list(an_expr_node_ptr expr_list,
                            unsigned int     is_lvalue_mask,
                            a_boolean        is_conditional_operator);

extern void lower_expr(an_expr_node_ptr expr,
                       a_boolean        is_lvalue);

#define lower_normal_expr(expr) lower_expr(expr, /*is_lvalue=*/FALSE)

extern void lower_arg_expr_list(an_expr_node_ptr expr_list,
                                a_type_ptr       called_rout_type);

extern an_expr_operator_kind lowered_assignment_operator(a_type_ptr type);

extern void lower_virtual_function_call(an_expr_node_ptr expr);

extern void lower_call(an_expr_node_ptr      expr,
                       an_init_pos_descr_ptr ipdp);

extern void lower_statement(a_statement_ptr statement);

extern void lower_il_memory_region(a_memory_region_number region_number);

#if DEBUG
extern unsigned long show_lowering_space_used(void);
#endif /* DEBUG */

extern void il_lower_init(void);

#endif /* DO_IL_LOWERING */
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
