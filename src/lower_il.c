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

lower_il.c -- Lower C++ intermediate language to C intermediate language.

*/

#include "basics.h"
#include "host_envir.h"

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#include "lang_feat.h"
#include "lower_il.h"
#include "debug.h"
#include "error.h"
#include "il.h"
#include "cmd_line.h"
#include "types.h"
#include "expr.h"
#include "exprutil.h"
#include "lexical.h"
#include "folding.h"
#include "const_ints.h"
#include "float_pt.h"
#include "class_decl.h"
#include "layout.h"
#include "mem_manage.h"


/*
This switch controls whether or not types and static variables that are local
to function and block scopes are moved onto the file scope lists.  Such
entities are allocated in the file scope memory region, but they are
normally linked on the local scope types or variables list.  That accurately
reflects the source form, which is desirable for generating symbolic
debug information.  That form probably works fine when the IL is being
fed into a true back end, but will not work when the IL is being turned
into C output (as with the C-generating back end), because the local
types and variables will not be visible from member functions of local
classes and in the file-scope termination routine when it deals with
calling destructors for local static variables.  When the switch here
is TRUE, the local types and variables will be (selectively) promoted
to the actual file scope.
*/
#define PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE BACK_END_IS_C_GEN_BE

/*
This switch controls whether or not all functions and function calls will
be turned into old-style unprototyped form.  This is what cfront effectively
does, in generating old-style C that is compiled by a C compiler.
This change is important if one wants to be able to call libraries that
were compiled by cfront.  (cfront's +a1 option requests generation of ANSI C
code; if one wants compatibility with cfront in that mode, this option
should be set to FALSE.)
*/
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED CFRONT_OBJECT_CODE_COMPATIBILITY


/*
IL lowering is only needed in this compilation if the source language
is C++, there are no errors, and lowering hasn't been suppressed.
*/
#define il_lowering_needed()                                          \
  (C_dialect == C_dialect_cplusplus && !suppress_il_lowering &&       \
   total_errors == 0)


static a_boolean
		lowering_file_scope;
			/* TRUE if lowering the file scope's IL, FALSE if
			   lowering a routine scope's IL. */
static a_label_ptr
		destructor_epilogue_label;
			/* Set while lowering the body of a destructor;
			   return statements in the body are changed to
			   branch to this label. */
static unsigned long
		count_of_refs_to_destructor_epilogue_label;
			/* Number of returns converted to branches as above. */
static a_type_ptr
		*type_promotion_insert_location;
			/* If non-NULL, indicates the position in the
			   file-scope types list at which promoted types
			   should be inserted (it points to the "next"
			   pointer of the entry after which the insert should
			   be done, or to the head-of-list pointer for
			   the file-scope types list).  If NULL, promotions
			   should be moved to the end of the file-scope
			   list. */
static unsigned long
		num_conditional_exprs_inside_of;
			/* Count of conditional parts of expressions that we
			   are inside of.  Incremented on entering conditional
			   operands of "?:", "&&", and "||". */


#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
unsigned long	allocated_name_string_length,
		num_init_pos_modifiers_allocated,
		num_required_destructor_calls_allocated,
		num_orphaned_types_lists_allocated;
#endif /* DEBUG */


/*
Integer kind to use for an offset into a class.  Its size must match
TARG_SIZEOF_PTR_TO_DATA_MEMBER.
*/
#define TARG_DELTA_INT_KIND ((an_integer_kind)ik_short)

/*
Integer kind to use for an index into a virtual function table.  Must be
no smaller than the size of a_virtual_function_number.
*/
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND ((an_integer_kind)ik_short)


/*
Lists of "orphaned" local type entries.  These are entries allocated in
the file-scope memory region but pointed to from the types pointer in a
function or block scope.  They cannot be found during the lowering of the
file scope memory region without a separate data structure.  The list
here is built up as function memory regions are lowered, and the entries
on the list are processed at the end of the file-scope lowering.  Note
that the list here is in addition to the comprehensive list built
in orphaned_file_scope_il_entries, which is also used in IL lowering.
*/
typedef struct an_orphaned_types_list *an_orphaned_types_list_ptr;
typedef struct an_orphaned_types_list {
  an_orphaned_types_list_ptr
		next;	/* Pointer to the next entry (i.e., next scope)
			   on the list. */
  a_type_ptr	types;	/* The types list from that scope. */
} an_orphaned_types_list;
static an_orphaned_types_list_ptr
		orphaned_types_list,
		end_orphaned_types_list;

/*
Macro used to test for crossing over into the file scope, i.e., a
reference to a file-scope memory region entity from somewhere in a
function scope memory region.
*/
#define crossing_into_file_scope(entry_ptr)                           \
  (!lowering_file_scope && in_file_scope((char *)(entry_ptr)))

/*
Add an IL entry to the list of orphaned entries of its kind.  This is
done so the entry can be found when traversing the file scope.
*/
#define record_orphaned_il_entry(entry_ptr, kind)                     \
  add_orphaned_file_scope_il_entry((char *)(entry_ptr),               \
                                   (an_il_entry_kind)(kind))

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


static a_routine_ptr
		file_scope_init_routine,
		file_scope_term_routine;
			/* Pointers to the file-scope initialization and
			   termination routines once created.  NULL until
			   then. */
static a_memory_region_number
		file_scope_init_routine_il_region,
		file_scope_term_routine_il_region;
			/* IL memory region numbers for the above routines,
			   once they are created. */
static an_insert_location
		file_scope_init_routine_insert_location,
		file_scope_term_routine_insert_location;
			/* Insert locations in file_scope_init_routine and
			   file_scope_term_routine.  Only defined if the
			   corresponding routine pointers are non-NULL. */
static a_boolean
		processing_file_scope_init_routine;
			/* TRUE while generating the file-scope initialization
			   routine. */

static a_variable_ptr
		return_value_pointer_variable;
			/* While processing a routine that returns its
			   value via a copy constructor, this points to
			   the parameter variable for the implicit parameter
			   through which the caller sends the address
			   at which the result will be stored. */

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
			/* Pointers to the similar entry at the next
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
static an_init_pos_modifier_ptr
		avail_init_pos_modifiers;
			/* List of initialization position modifier entries
			   that have been freed and are available for reuse. */

/*
Entry used to record a destructor call that must be emitted on exit from
a scope.
*/
typedef struct a_required_destructor_call *a_required_destructor_call_ptr;
typedef struct a_required_destructor_call {
  a_required_destructor_call_ptr
		next;	/* Next entry on a list of required calls, NULL
			   if last. */
  a_dynamic_init
		dynamic_init;
			/* The dynamic initialization entry that describes the
			   required destructor call.  Note that this is a copy
			   of the entire entry, not a pointer to it, because
			   in the case of destructor calls for local static
			   variables the original entry may be long gone
			   when the destructor call is being generated. */
  a_variable_ptr
		first_time_test_var;
			/* If non-NULL, points to a first-time-test variable
			   which will be non-zero if the initialization has
			   been done.  This is needed for local static
			   variables and for temporaries initialized under
			   conditional operators. */
  an_init_pos_descr
		init_pos_descr;
			/* Description of the object to destroy. */
  a_byte_boolean
		is_expr_temporary;
			/* TRUE if the entity to be destroyed is a compiler-
			   generated expression temporary. */
  unsigned long
		label_count;
			/* The count of label definitions that precede the
			   point at which this entry was added to the list. */
} a_required_destructor_call;
static a_required_destructor_call_ptr
		avail_required_destructor_calls;
			/* List of required destructor call entries that have
			   been freed and are available for reuse. */
static a_required_destructor_call_ptr
		destructor_calls_for_local_static_variables,
		end_destructor_calls_for_local_static_variables;
			/* List of required destructor calls for local static
			   variables.  These are saved and output at the
			   file scope. */

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
  a_required_destructor_call_ptr
		required_destructor_calls;
			/* Destructor calls required on exit from the scope. */
  a_switch_clause_ptr
		assoc_switch_clause;
			/* Points to the current clause of a switch statement
			   if inside one; NULL otherwise. */
  a_dynamic_init_ptr
		latest_dynamic_init_processed;
			/* Points to the latest stmk_init dynamic
			   initialization processed in the block.  NULL until
			   set. */
  a_dynamic_init_ptr
		dynamic_init_preceding_switch_clause;
			/* A copy of latest_dynamic_init_processed as of
			   the start of the current switch clause, if
			   assoc_switch_clause is non-NULL.  NULL otherwise. */
  unsigned long	label_count;
			/* The number of labels processed so far in this
			   block. */
  a_statement_ptr
		latest_label_statement_processed;
			/* The stmk_label statement most recently processed
			   in the block, or NULL if none has been processed. */
  a_dynamic_init_ptr
		dynamic_init_preceding_label;
			/* A copy of latest_dynamic_init_processed as of
			   the label most recently processed if
			   latest_label_statement_processed is non-NULL.
			   NULL otherwise. */
} a_context;
a_context_ptr	curr_context;
			/* Current (bottom) end of the context chain. */
a_context_ptr	nearest_function_context;
			/* Nearest function context in the context chain. */
a_context_ptr	file_scope_context;
			/* The context for the file scope. */
a_scope_ptr	nearest_function_scope;
			/* Nearest function scope, as pushed by
			   push_context. */
a_variable_ptr	nearest_this_param_variable;
			/* The this_param_variable from
			   nearest_function_scope. */


/* Declarations needed because of forward references: */
static void prelower_class_type(a_type_ptr class_type);
static void lower_ptr_to_member_constant(a_constant_ptr constant);
static an_expr_node_ptr make_base_class_lvalue(an_expr_node_ptr node,
                                               a_base_class_ptr bcp);
static sizeof_t mangled_basic_class_name(a_type_ptr type,
                                         char       *store_at);
static sizeof_t mangled_encoding_for_type(a_type_ptr type,
                                          char       *store_at);
static sizeof_t mangled_function_name(a_routine_ptr routine,
                                      a_boolean     suppress_param_encoding,
                                      char          *store_at);
static sizeof_t mangled_static_data_member_name(a_variable_ptr variable,
                                                a_type_ptr     class_type,
                                                char           *store_at);
static void lower_constant(a_constant_ptr constant);
static void lower_os_constant(a_constant_ptr constant);
static void lower_type(a_type_ptr type);
static void lower_os_type(a_type_ptr type);
static void lower_variable(a_variable_ptr variable);

static void lower_field_list(a_field_ptr field_list);
static void lower_field(a_field_ptr field);
static void lower_routine(a_routine_ptr routine);
static void lower_label(a_label_ptr label);
static void lower_asm_entry(an_asm_entry_ptr asm_entry);
static void lower_arg_expr_list(an_expr_node_ptr expr_list,
                                a_type_ptr       called_rout_type);
static void lower_expr(an_expr_node_ptr expr,
                       a_boolean        is_lvalue);
#define lower_normal_expr(expr) lower_expr(expr, /*is_lvalue=*/FALSE)
static void lower_statement(a_statement_ptr statement);
static void lower_scope(a_scope_ptr scope);
static void lower_dynamic_init(a_dynamic_init_ptr       dip,
                               an_init_pos_descr_ptr    ipdp,
                               a_variable_ptr           first_time_test_var,
                               a_boolean                is_expr_temporary,
                               an_expr_node_ptr         implied_arg_list,
                               an_expr_node_ptr         end_implied_arg_list,
                               a_constructor_init_ptr   ctor_init,
                               an_insert_location_ptr   insert_location,
                               a_boolean                *keep_dynamic_init);
static void lower_destructor_dynamic_init(
                                       a_dynamic_init_ptr     dip,
                                       an_init_pos_descr_ptr  ipdp,
                                       an_insert_location_ptr insert_location);
static void lower_call(an_expr_node_ptr      expr,
                       an_init_pos_descr_ptr ipdp);
static void add_constructor_wrapper_code(a_scope_ptr        scope,
                                         an_insert_location *insert_location);
static void gen_required_destructor_calls(
                                       a_context_ptr          outer_context,
                                       an_insert_location_ptr insert_location);
static a_boolean any_required_destructor_calls(a_context_ptr outer_context);
static void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta);
static void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset);
static a_boolean check_for_troublesome_ptr_to_member_constant(
                                                     a_constant_ptr constant,
                                                     a_variable_ptr *temp_var);


static void clear_insert_location(an_insert_location *insert_location,
                                  a_boolean          expr_insert)
/*
Clear an insert location and set its expr_insert field to expr_insert.
*/
{
  insert_location->expr_insert = expr_insert;
  if (expr_insert) {
    insert_location->variant.expr.ptr = NULL;
    insert_location->variant.expr.insert_before = FALSE;
  } else {
    insert_location->variant.statement.ptr = NULL;
    insert_location->variant.statement.insert_at_block_start = FALSE;
  }  /* if */
}  /* clear_insert_location */

/*
Set *insert_location to indicate an insert location following stmt.
Note that stmt must be a statement in a statement sequence (e.g., in
block); it may not be a statement in a position that requires a single
statement rather than a sequence (e.g., the dependent statement of an "if").
*/
#define set_insert_location(stmt, insert_location)                    \
{ clear_insert_location(insert_location, FALSE);                      \
  (insert_location)->variant.statement.ptr = (stmt);                  \
}  /* set_insert_location */

/*
Set *insert_location to indicate an insert location at the start of
the block stmt.
*/
#define set_block_start_insert_location(stmt, insert_location)        \
{ clear_insert_location(insert_location, FALSE);                      \
  (insert_location)->variant.statement.ptr = (stmt);                  \
  (insert_location)->variant.statement.insert_at_block_start = TRUE;  \
}  /* set_block_start_insert_location */

/*
Set *insert_location to indicate an insert location before the indicated
expression node.
*/
#define set_expr_insert_location(node, insert_location)               \
{ clear_insert_location(insert_location, TRUE);                       \
  (insert_location)->variant.expr.ptr = (node);                       \
  (insert_location)->variant.expr.insert_before = TRUE;               \
}  /* set_expr_insert_location */


static void clear_init_pos_modifier(an_init_pos_modifier_ptr ipmp)
/*
Set the fields of the indicated initialization position modifier entry to
default values.
*/
{
  ipmp->next       = NULL;
  ipmp->type       = NULL;
  ipmp->curr_elem  = 0;
  ipmp->curr_field = NULL;
  ipmp->curr_base  = NULL;
}  /* clear_init_pos_modifier */


static void add_init_pos_modifier(an_init_pos_modifier_ptr ipmp,
                                  an_init_pos_descr_ptr    ipdp)
/*
Set the fields of the indicated initialization position modifier entry to
default values and link it on the front of the list of modifiers of *ipdp.
*/
{
  clear_init_pos_modifier(ipmp);
  ipmp->next = ipdp->modifiers;
  ipdp->modifiers = ipmp;
}  /* add_init_pos_modifier */


static an_init_pos_modifier_ptr alloc_init_pos_modifier(void)
/*
Allocate an initialization position modifier entry, set its fields
to default values, and return a pointer to it.  Note that such entries are
usually allocated on the stack, so this routine is not called much.
*/
{
  an_init_pos_modifier_ptr ipmp;

  if (avail_init_pos_modifiers != NULL) {
    /* Reuse a freed entry. */
    ipmp = avail_init_pos_modifiers;
    avail_init_pos_modifiers = ipmp->next;
  } else {
    /* Allocate a new entry. */
    ipmp = (an_init_pos_modifier_ptr)alloc_fe(sizeof(an_init_pos_modifier));
#if DEBUG
    num_init_pos_modifiers_allocated++;
#endif /* DEBUG */
  }  /* if */
  clear_init_pos_modifier(ipmp);
  return ipmp;
}  /* alloc_init_pos_modifier */


static void free_init_pos_modifier_list(an_init_pos_modifier_ptr ipmp)
/*
Free a list of initialization position modifier entries by putting them on
the available list.
*/
{
  an_init_pos_modifier_ptr ipmp_next;

  for (; ipmp != NULL; ipmp = ipmp_next) {
    ipmp_next = ipmp->next;
    ipmp->next = avail_init_pos_modifiers;
    avail_init_pos_modifiers = ipmp;
  }  /* for */
}  /* free_init_pos_modifier_list */


static an_init_pos_modifier_ptr copy_init_pos_modifier_list(
                                                 an_init_pos_modifier_ptr ipmp)
/*
Make a copy of an initialization position modifier list and return a pointer
to the copy.  This is used when a list made up of stack entries must be
saved so that a destructor may be called later.
*/
{
  an_init_pos_modifier_ptr copy_ipmp;

  copy_ipmp = alloc_init_pos_modifier();
  *copy_ipmp = *ipmp;
  if (ipmp->next != NULL) {
    copy_ipmp->next = copy_init_pos_modifier_list(ipmp->next);
  }  /* if */
  return copy_ipmp;
}  /* copy_init_pos_modifier_list */


static void clear_init_pos_descr(an_init_pos_descr_ptr ipdp)
/*
Clear an initialization position description entry to default values.
*/
{
  ipdp->variable                  = NULL;
  ipdp->indirect_through_variable = FALSE;
  ipdp->base_type                 = NULL;
  ipdp->modifiers                 = NULL;
  ipdp->whole_array               = FALSE;
  ipdp->array_element_count       = 0;
}  /* clear_init_pos_descr */


static void set_var_init_pos_descr(a_variable_ptr        var,
                                   an_init_pos_descr_ptr ipdp)
/*
Make an initialization position description entry for the variable var.
*/
{
  clear_init_pos_descr(ipdp);
  ipdp->variable = var;
  ipdp->base_type = var->type;
}  /* set_var_init_pos_descr */


static void set_var_indirect_init_pos_descr(a_variable_ptr        var,
                                            an_init_pos_descr_ptr ipdp)
/*
Make an initialization position description entry for the object pointed
to by variable var.
*/
{
  clear_init_pos_descr(ipdp);
  ipdp->variable = var;
  ipdp->indirect_through_variable = TRUE;
  ipdp->base_type = type_pointed_to(var->type);
}  /* set_var_indirect_init_pos_descr */


static void modify_ctor_init_pos_descr(a_constructor_init_ptr   ctor_init,
                                       an_init_pos_descr_ptr    ipdp,
                                       an_init_pos_modifier_ptr ipmp)
/*
Modify the initialization position description in *ipdp to describe the
entity being initialized by the constructor init entry ctor_init.  *ipdp is
already set to describe the base address of the entity, and this routine
adds the modifiers needed to get to the proper subobject of the entity.
ipmp points to a local variable in the caller that can be used for an
init position modifier.
*/
{
  switch (ctor_init->kind) {
    case cik_virtual_base_class:
    case cik_direct_base_class:
      /* Add a modifier that selects the base class relative to the "this"
         parameter. */
      add_init_pos_modifier(ipmp, ipdp);
      ipmp->curr_base = ctor_init->variant.base_class;
      ipmp->type = ctor_init->variant.base_class->type;
      break;
    case cik_field:
      /* Add a modifier that selects the field relative to the "this"
         parameter. */
      add_init_pos_modifier(ipmp, ipdp);
      ipmp->curr_field = ctor_init->variant.field;
      ipmp->type = ctor_init->variant.field->type;
      break;
#if CHECKING
    default:
      internal_error("modify_ctor_init_pos_descr: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* modify_ctor_init_pos_descr */


static void develop_ctor_init_pos_descr(
                                       a_constructor_init_ptr   ctor_init,
                                       a_variable_ptr           this_param_var,
                                       an_init_pos_descr_ptr    ipdp,
                                       an_init_pos_modifier_ptr ipmp)
/*
Develop an initialization position description in *ipdp to describe the
entity being initialized by the constructor init entry ctor_init.
this_param_var points to the "this" parameter variable for the constructor.
ipmp points to a local variable in the caller that can be used for an init
position modifier.
*/
{
  set_var_indirect_init_pos_descr(this_param_var, ipdp);
  modify_ctor_init_pos_descr(ctor_init, ipdp, ipmp);
}  /* develop_ctor_init_pos_descr */


static a_required_destructor_call_ptr alloc_required_destructor_call(void)
/*
Allocate a required destructor call entry, set its fields to default values,
and return a pointer to it.
*/
{
  a_required_destructor_call_ptr rdcp;

  if (avail_required_destructor_calls != NULL) {
    /* Reuse a freed entry. */
    rdcp = avail_required_destructor_calls;
    avail_required_destructor_calls = rdcp->next;
  } else {
    /* Allocate a new entry. */
    rdcp = (a_required_destructor_call_ptr)alloc_fe(
                                           sizeof(a_required_destructor_call));
#if DEBUG
    num_required_destructor_calls_allocated++;
#endif /* DEBUG */
  }  /* if */
  rdcp->next = NULL;
  clear_dynamic_init(&rdcp->dynamic_init, (a_dynamic_init_kind)dik_none);
  rdcp->first_time_test_var = NULL;
  clear_init_pos_descr(&rdcp->init_pos_descr);
  rdcp->is_expr_temporary = FALSE;
  rdcp->label_count = 0;
  return rdcp;
}  /* alloc_required_destructor_call */


static void free_required_destructor_call_list(
                                           a_required_destructor_call_ptr rdcp)
/*
Free a list of required destructor call entries by putting them on the
available list.
*/
{
  a_required_destructor_call_ptr rdcp_next;

  for (; rdcp != NULL; rdcp = rdcp_next) {
    /* Free the list of init_pos_modifier entries pointed to. */
    free_init_pos_modifier_list(rdcp->init_pos_descr.modifiers);
    rdcp_next = rdcp->next;
    rdcp->next = avail_required_destructor_calls;
    avail_required_destructor_calls = rdcp;
  }  /* for */
}  /* free_required_destructor_call_list */


static void push_context(a_context   *context,
                         a_scope_ptr scope)
/*
Add the context entry "context" to the context stack.  The associated scope
is "scope".
*/
{
  a_context_ptr parent_context = curr_context;

  curr_context = context;
  /* Remember the file scope context if this is the first push_context. */
  if (parent_context == NULL) file_scope_context = context;
  /* Set the fields. */
  context->parent = parent_context;
  context->scope = scope;
  context->required_destructor_calls = NULL;
  context->assoc_switch_clause = NULL;
  context->latest_dynamic_init_processed = NULL;
  context->dynamic_init_preceding_switch_clause = NULL;
  context->label_count = 0;
  context->latest_label_statement_processed = NULL;
  context->dynamic_init_preceding_label = NULL;
  /* Keep track of the innermost function context/scope. */
  if (scope->kind == (a_scope_kind)sck_function) {
    nearest_function_context = curr_context;
    nearest_function_scope = scope;
    nearest_this_param_variable = scope->variant.routine.this_param_variable;
  }  /* if */
}  /* push_context */


static void pop_context(void)
/*
Pop an entry off the context stack.
*/
{
  a_context_ptr cp, parent_context;

  parent_context = curr_context->parent;
  /* Free any required destructor call entries. */
  free_required_destructor_call_list(curr_context->required_destructor_calls);
  /* Keep track of the innermost function context/scope. */
  if (curr_context == nearest_function_context) {
    nearest_function_context = NULL;
    nearest_function_scope = NULL;
    nearest_this_param_variable = NULL;
    for (cp = parent_context; cp != NULL; cp = cp->parent) {
      if (cp->scope->kind == (a_scope_kind)sck_function) {
        nearest_function_context = cp;
        nearest_function_scope = cp->scope;
        nearest_this_param_variable = nearest_function_scope->
                                           variant.routine.this_param_variable;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* Pop to the surrounding context. */
  curr_context = parent_context;
}  /* pop_context */


static a_boolean operator_takes_lvalue_operand(an_expr_operator_kind op)
/*
Return TRUE if the given expression operator takes an lvalue as its first
operand.
*/
{
  a_boolean takes_lvalue;

  switch (op) {
    case eok_field:
    case eok_bit_field:
    case eok_extract_bit_field:
    case eok_pm_field:
    case eok_lvalue_cast:
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
    case eok_sassign:
    case eok_pmassign:
    case eok_iadd_assign:
    case eok_isubtract_assign:
    case eok_imultiply_assign:
    case eok_idivide_assign:
    case eok_remainder_assign:
    case eok_fadd_assign:
    case eok_fsubtract_assign:
    case eok_fmultiply_assign:
    case eok_fdivide_assign:
    case eok_padd_assign:
    case eok_psubtract_assign:
    case eok_shiftl_assign:
    case eok_shiftr_assign:
    case eok_and_assign:
    case eok_or_assign:
    case eok_xor_assign:
    case eok_ipost_decr:
    case eok_ipre_decr:
    case eok_fpost_decr:
    case eok_fpre_decr:
    case eok_ppost_decr:
    case eok_ppre_decr:
    case eok_ipost_incr:
    case eok_ipre_incr:
    case eok_fpost_incr:
    case eok_fpre_incr:
    case eok_ppost_incr:
    case eok_ppre_incr:
      takes_lvalue = TRUE;
      break;
    default:
      takes_lvalue = FALSE;
      break;
  }  /* switch */
  return takes_lvalue;
}  /* operator_takes_lvalue_operand */


static a_base_class_ptr find_direct_base_class_of(a_type_ptr derived_class,
                                                  a_type_ptr base_class)
/*
derived_class and base_class are both class types, and base_class is a
direct base class of derived_class.  Find the corresponding base class
entry and return a pointer to it.
*/
{
  a_base_class_ptr bcp;

#if CHECKING
  if (!is_immediate_class_type(derived_class)) {
    internal_error("find_direct_base_class_of: bad derived_class type");
  }  /* if */
  if (!is_immediate_class_type(base_class)) {
    internal_error("find_direct_base_class_of: bad base_class type");
  }  /* if */
#endif /* CHECKING */
  for (bcp = derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       ;
       bcp = bcp->next) {
#if CHECKING
    if (bcp == NULL) {
      internal_error("find_direct_base_class: virtual base class not found");
    }  /* if */
#endif /* CHECKING */
    if (bcp->direct && bcp->type == base_class) break;
  }  /* for */
  return bcp;
}  /* find_direct_base_class_of */


static a_base_class_ptr find_virtual_base_class_of(a_type_ptr derived_class,
                                                   a_type_ptr virt_base_class)
/*
Find the base class entry of derived_type that corresponds to the
virtual base class virt_base_class (not necessarily a direct base class)
and return a pointer to the base class entry.  It must be found.
*/
{
  a_base_class_ptr virt_bcp;

#if CHECKING
  if (!is_immediate_class_type(derived_class)) {
    internal_error("find_virtual_base_class_of: bad derived_class type");
  }  /* if */
  if (!is_immediate_class_type(virt_base_class)) {
    internal_error("find_virtual_base_class_of: bad virt_base_class type");
  }  /* if */
#endif /* CHECKING */
  for (virt_bcp = derived_class->variant.class_struct_union.
                                                      extra_info->base_classes;
       ;
       virt_bcp = virt_bcp->next) {
#if CHECKING
    if (virt_bcp == NULL) {
      internal_error("find_virtual_base_class: virtual base class not found");
    }  /* if */
#endif /* CHECKING */
    if (virt_bcp->type == virt_base_class && virt_bcp->is_virtual) break;
  }  /* for */
  return virt_bcp;
} /* find_virtual_base_class */


a_targ_ptrdiff_t related_class_offset(a_type_ptr class_1,
                                      a_type_ptr class_2)
/*
If class_1 and class_2 are related classes (one is derived from the other),
return the offset of class_2 relative to class_1.  If class_1 is a base
class of class_2, the offset may be negative.  If the classes are unrelated,
return 0.
*/
{
  a_targ_ptrdiff_t offset = 0;
  a_base_class_ptr bcp;

  if (class_1 == class_2) {
    /* Classes are the same.  Offset is zero (already set). */
  } else if ((bcp = find_base_class_of(class_1, class_2)) != NULL) {
    /* class_2 is a base class of class_1. */
    offset = bcp->offset;
  } else if ((bcp = find_base_class_of(class_2, class_1)) != NULL) {
    /* class_1 is a base class of class_2. */
    offset = -bcp->offset;
  }  /* if */
  return offset;
}  /* related_class_offset */


static void add_field(char          *field_name,
                      a_type_ptr    field_type,
                      a_targ_size_t field_offset,
                      a_type_ptr    struct_type)
/*
Make a field with the given type and add it at the right spot in the
list of fields attached to struct_type.  field_name gives the field name
(already allocated in the IL memory region).  field_offset gives the byte
offset for the field.
*/
{
  a_field_ptr   prev_field, next_field;
  a_field_ptr   field_ptr;
  a_targ_size_t bit_offset;

  /* Make the field. */
  field_ptr = alloc_field();
  field_ptr->source_corresp.name = field_name;
  field_ptr->source_corresp.class_of_which_a_member = struct_type;
  field_ptr->type = field_type;
  field_ptr->bit_offset = bit_offset = field_offset*TARG_CHAR_BIT;
  /* Find the spot at which to insert the field. */
  for (prev_field = NULL,
               next_field = struct_type->variant.class_struct_union.field_list;
       next_field != NULL && next_field->bit_offset < bit_offset;
       prev_field = next_field, next_field = next_field->next) {}
#if CHECKING
  if (next_field != NULL && next_field->bit_offset == bit_offset) {
#if DEBUG
    db_abbreviated_type(struct_type);
    fprintf(f_debug, ", bit offset = %lu, new field = %s, old field = ",
                     (unsigned long)bit_offset, field_name);
    db_name(&next_field->source_corresp);
    fputc('\n', f_debug);
#endif /* DEBUG */
    internal_error("add_field: two fields have the same offset");
  }  /* if */
#endif /* CHECKING */
  /* Insert the field at the right spot. */
  if (prev_field == NULL) {
    struct_type->variant.class_struct_union.field_list = field_ptr;
  } else {
    prev_field->next = field_ptr;
  }  /* if */
  field_ptr->next = next_field;
}  /* add_field */


static void add_dummy_field(char          *field_name,
                            a_type_ptr    field_type,
                            a_targ_size_t field_offset,
                            a_type_ptr    struct_type)
/*
Make a dummy field with the given type and add it at the right spot in the
list of fields attached to struct_type.  field_name gives the field name
(not allocated in the IL memory region, i.e., it must be copied).
field_offset gives the byte offset for the field.
*/
{
  sizeof_t name_length, alloc_length;
  char     *name_ptr;

  /* Determine the length of the name. */
  check_assertion(field_name != NULL);
  name_length = strlen(field_name);
  /* Allocate space for the name. */
  alloc_length = name_length + 1;
  name_ptr = alloc_il(alloc_length);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  /* Copy in the name. */
  (void)strcpy(name_ptr, field_name);
  /* Create the field. */
  add_field(name_ptr, field_type, field_offset, struct_type);
}  /* add_dummy_field */


static void add_base_class_dummy_field(a_type_ptr    base_class_type,
                                       char          *field_prefix,
                                       a_type_ptr    field_type,
                                       a_targ_size_t field_offset,
                                       a_type_ptr    struct_type)
/*
Make a dummy field for a base class or pointer thereto, and add it at the
right spot in the list of fields attached to struct_type.  The field name
is formed by concatenating field_prefix and the name from base_class_type.
field_type gives the type for the field.  field_offset gives the byte
offset for the field.
*/
{
  sizeof_t name_length, prefix_length, alloc_length;
  char     *name_ptr;

  /* Build the name for the field.  This is done by combining the
     field_prefix and the (possibly mangled) base class name. */
  prefix_length = strlen(field_prefix);
  /* Determine how long the base class name is. */
  name_length = mangled_basic_class_name(base_class_type, (char *)NULL);
  /* Allocate space for the whole name. */
  alloc_length = prefix_length + name_length + 1;
  name_ptr = alloc_il(alloc_length);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  /* Copy in the prefix. */
  (void)memcpy(name_ptr, field_prefix, size_t_arg(prefix_length));
  /* Store the base class name. */
  (void)mangled_basic_class_name(base_class_type, name_ptr+prefix_length);
  name_ptr[prefix_length+name_length] = '\0';
  /* Create the field. */
  add_field(name_ptr, field_type, field_offset, struct_type);
}  /* add_base_class_dummy_field */


static void copy_field(a_field_ptr old_field_ptr,
                       a_type_ptr  struct_type,
                       a_field_ptr *last_field)
/*
Make a copy of the field old_field_ptr and add it to the end of the
list of fields attached to struct_type.  *last_field points to the
last field, or is NULL if there are no fields yet; it is updated
on return.
*/
{
  char        *field_name;
  sizeof_t    name_length, alloc_length;
  a_field_ptr field_ptr;

  /* Copy the name into the file-scope IL memory region. */
  field_name = old_field_ptr->source_corresp.name;
  /* Watch out for anonymous union fields -- they have no name. */
  if (field_name != NULL) {
    name_length = strlen(field_name);
    alloc_length = name_length + 1;
    field_name = strcpy(alloc_il(alloc_length), field_name);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  }  /* if */
  /* Make the field entry. */
  field_ptr = alloc_field();
  /* Copy the whole entry, then adjust a few fields. */
  *field_ptr = *old_field_ptr;
  field_ptr->source_corresp.name = field_name;
  field_ptr->source_corresp.class_of_which_a_member = struct_type;
  field_ptr->next = NULL;
  /* Add the field to the end of the struct field list. */
  if (*last_field == NULL) {
    struct_type->variant.class_struct_union.field_list = field_ptr;
  } else {
    (*last_field)->next = field_ptr;
  }  /* if */
  *last_field = field_ptr;
}  /* copy_field */


static void make_field(char          *field_name,
                       a_type_ptr    field_type,
                       a_targ_size_t *byte_offset,
                       a_type_ptr    struct_type,
                       a_field_ptr   *last_field)
/*
Make a field with the given name and type and add it to the end of the list
of fields attached to struct_type.  The name pointed to by field_name is
copied into the file-scope IL memory region.  *last_field points to the
last field, or is NULL if there are no fields yet; it is updated on exit.
*byte_offset gives the next available position in the structure, both on
entry and (updated) on exit.  This routine is used for creating fields of
wholly-generated structs, not for adding fields to existing structs.
It cannot create bit fields.  field_name may not be NULL.
*/
{
  sizeof_t         name_length, alloc_length;
  a_field_ptr      field_ptr;
  a_targ_alignment alignment;
  int              bit_offset;

  /* Copy the name into the file-scope IL memory region. */
  name_length = strlen(field_name);
  alloc_length = name_length + 1;
  field_name = strcpy(alloc_il(alloc_length), field_name);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  /* Make the field entry. */
  field_ptr = alloc_field();
  field_ptr->source_corresp.name = field_name;
  field_ptr->source_corresp.class_of_which_a_member = struct_type;
  field_ptr->type = field_type;
  /* Add the field to the end of the struct field list. */
  if (*last_field == NULL) {
    struct_type->variant.class_struct_union.field_list = field_ptr;
  } else {
    (*last_field)->next = field_ptr;
  }  /* if */
  *last_field = field_ptr;
  /* Determine the field offset and update the offset and struct alignment. */
  alignment = struct_type->alignment;
  bit_offset = 0;
  (void)set_field_size_and_offset(field_ptr, byte_offset, &bit_offset,
                                  &alignment);
  struct_type->alignment = alignment;
}  /* make_field */


static void finish_class_type(a_type_ptr    class_type, 
                              a_targ_size_t *byte_offset)
/*
Finish off a created class type by doing final alignment and storing the
size and alignment.  Works for both structs and unions.
*/
{
  a_class_type_supplement_ptr ctsp;
  int                         bit_offset = 0;

  (void)do_alignment(byte_offset, &bit_offset, class_type->alignment);
  /* Put final size into the struct or union type. */
  class_type->size = *byte_offset;
  ctsp = class_type->variant.class_struct_union.extra_info;
  ctsp->size_without_virtual_base_classes = *byte_offset;
  ctsp->alignment_without_virtual_base_classes = class_type->alignment;
}  /* finish_class_type */


/*
Pointer to the generic function pointer type used in virtual function tables
and pointers to member functions, once it is created.  NULL until created.
*/
static a_type_ptr
		vptp_type;


static a_type_ptr make_vptp_type(void)
/*
Make a type that is used as a generic function pointer in virtual function
tables and pointers to member functions if it has not been made already,
and return a pointer to it.  The type looks like

  typedef int (*__vptp)();

except that it doesn't actually have a name.
*/
{
  a_type_ptr function_type;

  if (vptp_type == NULL) {
    /* Make function-of-no-parameters-returning-int. */
    function_type = alloc_type((a_type_kind)tk_routine);
    function_type->variant.routine.return_type =
                                         integer_type((an_integer_kind)ik_int);
    /* Make pointer to function-returning-int. */
    vptp_type = make_pointer_type(function_type);
  }  /* if */
  return vptp_type;
}  /* make_vptp_type */


static void add_to_front_of_file_scope_types_list(a_type_ptr type)
/*
Add the indicated type to the beginning of the file-scope types list.
This is used for simple lowering-generated structs that might be used
inside other user-written structs.
*/
{
  type->next = il_header.primary_scope->types;
  il_header.primary_scope->types = type;
  if (depth_scope_stack >= DEPTH_OF_FILE_SCOPE &&
      scope_stack[DEPTH_OF_FILE_SCOPE].last_type == NULL) {
    /* There are no types on the file scope list, so this type is also the
       last type on the list. */
    scope_stack[DEPTH_OF_FILE_SCOPE].last_type = type;
  }  /* if */
}  /* add_to_front_of_file_scope_types_list */


/*
Pointer to the struct type that defines pointers to member functions,
once it is created.  NULL until created.
*/
static a_type_ptr
		mptr_type;
static a_field_ptr
		mptr_d_field,
		mptr_i_field,
		mptr_f_field;


static a_type_ptr make_mptr_type(void)
/*
Make the struct type used in pointers to member functions if it has not been
made already, and return a pointer to it.  Its definition is

  struct __mptr { short d; short i; __vptp f; };

"d" is the offset delta, "i" is the index into the virtual function table
(or -1 for a nonvirtual function, or 0 for a NULL pointer), "f" is the
nonvirtual function pointer or the offset to the virtual table pointer
(appropriately cast) for the virtual function case; see ARM 8.1.2c.
This type is also used as the entry type in virtual function tables.
There, the "i" field is never needed.  (cfront does it that way, so for
compatibility we do too.)
*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (mptr_type == NULL) {
    /* Make the __mptr struct type.  It doesn't actually have a name. */
    mptr_type = alloc_type((a_type_kind)tk_struct);
    byte_offset = 0;
    last_field = NULL;
    /* field: short d; (delta) */
    make_field("d", integer_type(TARG_DELTA_INT_KIND),
               &byte_offset, mptr_type, &last_field);
    mptr_d_field = last_field;
    /* field: short i; (index into virtual function table) */
    make_field("i", integer_type(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND),
               &byte_offset, mptr_type, &last_field);
    mptr_i_field = last_field;
    /* field: __vptp f; (pointer to function for nonvirtual case, or
       offset to vtbl ptr, appropriately cast, in nonvirtual case) */
    make_field("f", make_vptp_type(), &byte_offset, mptr_type, &last_field);
    mptr_f_field = last_field;
    finish_class_type(mptr_type, &byte_offset);
    add_to_front_of_file_scope_types_list(mptr_type);
#if CHECKING
    if (mptr_type->size != TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION ||
        mptr_type->alignment != TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) {
      internal_error(
 "make_mptr_type: target.h config of pointer-to-member-function is incorrect");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  return mptr_type;
}  /* make_mptr_type */


static a_type_ptr underlying_pm_type(a_type_ptr type)
/*
type is (or was, before lowering) a pointer-to-member type.  If it is
a lowered pointer-to-member type, return the original pointer-to-member type.
Otherwise (if it is an unlowered pointer-to-member type), return the
type with typerefs dropped.
*/
{
  /* Drop typerefs, but look for a special entry that indicates that
     it was some other type rewritten by IL lowering. */
  while (type->kind == (a_type_kind)tk_typeref) {
    if (type->variant.typeref.orig_type != NULL) {
      /* A type transformed to something else (e.g., a pointer to
         data member changed to a small integer). */
      type = type->variant.typeref.orig_type;
      break;
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  return type;
}  /* underlying_pm_type */


static a_type_ptr pm_member_type_possibly_lowered(a_type_ptr type)
/*
type is (or was, before lowering) a pointer-to-member type.  Get and return
its member type.
*/
{
  a_type_ptr member_type;

  type = underlying_pm_type(type);
  member_type = pm_member_type(type);
  return member_type;
}  /* pm_member_type_possibly_lowered */


static a_type_ptr pm_class_type_possibly_lowered(a_type_ptr type)
/*
type is (or was, before lowering) a pointer-to-member type.  Get and return
its class type.
*/
{
  a_type_ptr class_type;

  type = underlying_pm_type(type);
  class_type = pm_class_type(type);
  return class_type;
}  /* pm_class_type_possibly_lowered */


static a_boolean is_or_was_ptr_to_data_member_type(a_type_ptr type)
/*
Return TRUE if type is (or was, before lowering) a pointer to data member.
*/
{
  a_boolean is_ptr_to_data = FALSE;

  /* Drop typerefs, but look for a special entry that indicates that
     it was some other type rewritten by IL lowering. */
  while (type->kind == (a_type_kind)tk_typeref) {
    if (type->variant.typeref.orig_type != NULL) {
      /* A type transformed to something else (e.g., a pointer to
         data member changed to a small integer). */
      type = type->variant.typeref.orig_type;
      break;
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  if (is_ptr_to_member_type(type)) {
    /* The type is a pointer-to-member type.  See if the member type is
       a non-function type. */
    if (!is_function_type(pm_member_type(type))) is_ptr_to_data = TRUE;
  }  /* if */
  return is_ptr_to_data;
}  /* is_or_was_ptr_to_data_member_type */


static a_boolean is_or_was_ptr_to_member_function_type(a_type_ptr type)
/*
Return TRUE if type is (or was, before lowering) a pointer to member
function.
*/
{
  a_boolean is_ptr_to_func = FALSE;

  type = skip_typerefs(type);
  if (type == mptr_type) {
    /* The type is the struct used for pointers to members, so this is
       a pointer to member function that has been lowered. */
    is_ptr_to_func = TRUE;
  } else if (is_ptr_to_member_type(type)) {
    /* The type is a pointer-to-member type.  See if the member type is
       a function type. */
    if (is_function_type(pm_member_type(type))) {
      is_ptr_to_func = TRUE;
    }  /* if */
  }  /* if */
  return is_ptr_to_func;
}  /* is_or_was_ptr_to_member_function_type */


static a_variable_ptr make_temporary_in_scope(a_type_ptr  temp_type,
                                              a_scope_ptr scope)
/*
Make a temporary variable whose type is temp_type in scope scope.
Return a pointer to it.
*/
{
  a_variable_ptr          temp;
  a_storage_class         storage_class;
  a_scope_stack_entry_ptr ssep;

  /* Allocate the variable, using auto storage class in functions and
     blocks, static elsewhere. */
  if (scope->kind == (a_scope_kind)sck_function ||
      scope->kind == (a_scope_kind)sck_block) {
    storage_class = (a_storage_class)sc_auto;
  } else {
    storage_class = (a_storage_class)sc_static;
  }  /* if */
  temp = alloc_variable(storage_class);
  temp->type = temp_type;
  temp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  /* See if the scope we are adding to is active on the scope stack.
     If so, we have to maintain the "last" pointer too. */
  for (ssep = scope_stack; ssep <= &scope_stack[depth_scope_stack]; ssep++) {
    if (ssep->il_scope == scope) goto have_ssep;
  }  /* for */
  ssep = NULL;
have_ssep:
  /* Add the temporary to the scope list (at the front).  We cannot use
     add_to_variables_list because we might be working on an internally-
     generated routine, like a constructor, for which a push_scope was
     not done. */
  if (storage_class == (a_storage_class)sc_static) {
    temp->next = scope->variables;
    scope->variables = temp;
    if (ssep != NULL && ssep->last_variable == NULL) {
      ssep->last_variable = temp;
    }  /* if */
  } else {
    temp->next = scope->nonstatic_variables;
    scope->nonstatic_variables = temp;
    if (ssep != NULL && ssep->last_nonstatic_variable == NULL) {
      ssep->last_nonstatic_variable = temp;
    }  /* if */
  }  /* if */
  return temp;
}  /* make_temporary_in_scope */


/*
Interface to make_temporary_in_scope for the common case where the
nearest scope should be used.  Allocates a temporary variable of the
indicated type and returns a pointer to the variable.
*/
#define make_temporary(temp_type)                                     \
  make_temporary_in_scope((temp_type), curr_context->scope)


static a_variable_ptr make_temporary_possibly_at_file_scope(
                                                      a_type_ptr temp_type,
                                                      a_boolean  at_file_scope)
/*
Make a variable for a temporary of type temp_type and return a pointer to
it.  If at_file_scope is TRUE, make the temporary in the file scope;
otherwise, allocate it in the current scope.
*/
{
  a_variable_ptr temp_var;

  if (!at_file_scope) {
    /* Normal case. */
    temp_var = make_temporary(temp_type);
  } else {
    /* Make the temporary in the file scope. */
    a_memory_region_number region_to_switch_back_to;
    switch_to_file_scope_region(&region_to_switch_back_to);
    temp_var = make_temporary_in_scope(temp_type, il_header.primary_scope);
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  return temp_var;
}  /* make_temporary_possibly_at_file_scope */


static a_variable_ptr make_variable(char            *var_name,
                                    a_boolean       already_il_name,
                                    a_type_ptr      var_type,
                                    a_storage_class var_storage_class)
/*
Make a file-scope variable whose name is var_name, whose type is var_type,
and whose storage class is var_storage_class.  Return a pointer to it.
already_il_name is TRUE if the name has already been allocated in the IL;
if not, it has to be allocated and copied.
*/
{
  a_variable_ptr var;
  sizeof_t       alloc_length;

  /* Allocate the variable. */
  var = alloc_variable(var_storage_class);
  if (!already_il_name) {
    /* Copy the name to the IL region. */
    alloc_length = strlen(var_name)+1;
    var_name = strcpy(alloc_il(alloc_length), var_name);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  }  /* if */
  var->source_corresp.name = var_name;
  var->type = var_type;
  var->source_corresp.name_linkage =
        (var_storage_class == (a_storage_class)sc_unspecified ||
         var_storage_class == (a_storage_class)sc_extern) ?
                                            (a_name_linkage_kind)nlk_external :
        (var_storage_class == (a_storage_class)sc_static && var_name != NULL) ?
                                            (a_name_linkage_kind)nlk_internal :
        /* Otherwise: */
                                            (a_name_linkage_kind)nlk_none;
#if CHECKING
  if (var_storage_class != (a_storage_class)sc_static &&
      var_storage_class != (a_storage_class)sc_extern &&
      var_storage_class != (a_storage_class)sc_unspecified) {
    internal_error("make_variable: bad storage class for file scope var");
  }  /* if */
#endif /* CHECKING */
  /* Add the variable to the file scope list. */
  add_to_variables_list(var, /*at_file_scope=*/TRUE);
  return var;
}  /* make_variable */


static a_variable_ptr make_param_variable(a_type_ptr type)
/*
Make a variable for a parameter of type "type" and return a pointer to
it.  The variable has no name.
*/
{
  a_variable_ptr param_var;

  param_var = alloc_variable((a_storage_class)sc_auto);
  param_var->type = type;
  param_var->is_parameter = TRUE;
  param_var->source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  return param_var;
}  /* make_param_variable */


static an_expr_node_ptr make_node_for_il_constant(a_constant_ptr constant)
/*
Make an expression node for the given constant and return a pointer to it.
This is used when the constant is already an allocated IL constant.
*/
{
  an_expr_node_ptr node;
  a_variable_ptr   temp_var;

  /* If the constant is a converted pointer-to-member-function constant.
     If so, it is now a ck_aggregate constant, which cannot be used directly
     in an expression.  For that case, create a temporary variable
     initialized with the ck_aggregate, and use the value of the variable. */
  if (check_for_troublesome_ptr_to_member_constant(constant, &temp_var)) {
    node = var_rvalue_expr(temp_var);
  } else {
    /* Normal case; make a constant node. */
    node = alloc_expr_node((an_expr_node_kind)enk_constant);
    node->variant.constant = constant;
    node->type = constant->type;
  }  /* if */
  return node;
}  /* make_node_for_il_constant */


static an_expr_node_ptr make_vtbl_address_node(a_variable_ptr var)
/*
Make an expression for the address of a virtual function table variable and
return a pointer to it.  The variable has an array type.  The pointer
has type pointer to element.
*/
{
  an_expr_node_ptr var_node;
  a_constant       addr_constant;
  a_type_ptr       ptr_element_type;

  ptr_element_type = make_pointer_type(array_element_type(var->type));
  /* Make a constant for the address of the array, implicitly cast it to
     pointer-to-element-type, and make an expression whose value is the
     address constant.  This gives an address with the right type. */
  clear_constant(&addr_constant, (a_constant_repr_kind)ck_address);
  addr_constant.type = make_pointer_type(var->type);
  addr_constant.variant.address.kind = (an_address_base_kind)abk_variable;
  addr_constant.variant.address.variant.variable = var;
  implicit_cast(&addr_constant, ptr_element_type);
  var_node = alloc_node_for_constant(&addr_constant);
  return var_node;
}  /* make_vtbl_address_node */


static an_expr_node_ptr node_to_select_field_from_rvalue(
                                                        an_expr_node_ptr node,
                                                        a_field_ptr      field)
/*
node is an rvalue.  Create an expression to select the value of the indicated
field from it, and return a pointer to the expression created.  Note that
this is not normal field selection; usually, field selection is done from
an address for the struct, and produces the address of the field.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      field_node;
  a_type_ptr            selection_type;

  /* This routine is similar to field_lvalue_selection_expr. */
  /* Make the expression node for the field. */
  field_node = alloc_expr_node((an_expr_node_kind)enk_field);
  field_node->type = field->type;
  field_node->variant.field = field;
  node->next = field_node;
  /* Use a different operator for bit field references. */
  op = (field->bit_size != 0) ? (an_expr_operator_kind)eok_value_bit_field :
                                (an_expr_operator_kind)eok_value_field;
  /* The selected field has all the type qualifiers of both the field
     and the selecting pointer. */
  selection_type = type_plus_qualifiers_from_second_type(field->type,
                                                         node->type);
  /* Make the field selection node. */
  node = make_operator_node(op, selection_type, node);
  return node;
}  /* node_to_select_field_from_rvalue */


static void adjust_field_selection_for_anonymous_union_references(
                                                         an_expr_node_ptr node)
/*
node is a field selection expression.  If it refers to a field in an anonymous
union, adjust it to make the anonymous union reference(s) explicit.
*/
{
  a_field_ptr	field, au_field;
  an_expr_node_ptr
		op1, op2, new_op1, au_field_node;
  a_type_ptr	field_class, new_selection_type;
  a_class_type_supplement_ptr
		ctsp;
  an_expr_operator_kind
		op, new_op;

  /* The loop here is for cases where there are several nested anonymous
     unions. */
  for (;;) {
    op1 = node->variant.operation.operands;
    op2 = op1->next;
    field = op2->variant.field;
    /* See if the field is from an anonymous union. */
    field_class = field->source_corresp.class_of_which_a_member;
    ctsp = field_class->variant.class_struct_union.extra_info;
    if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_field) {
      /* Stop when the field is not from an anonymous union. */
      break;
    }  /* if */
    /* Yes, it is from an anonymous union.  Rewrite the reference.
       Basically, we keep the field selection we have, but rewrite its
       first operand as a field selection of the proper anonymous union out
       of the original first operand. */
    au_field = ctsp->anonymous_union_field;
    /* If the original field selection takes an lvalue as its first operand,
       the added field selection is an eok_field; if the original field
       selection takes an rvalue as its first operand, the added field
       selection is an eok_value_field.  Here are the operators:
                                 in       out
         eok_field             lvalue   lvalue
         eok_value_field       rvalue   rvalue
         eok_bit_field         lvalue   lvalue
         eok_value_bit_field   rvalue   rvalue
         eok_extract_bit_field lvalue   rvalue
       Note that eok_field and eok_value_field produce as output that is
       the same as their input, which is why they are used for the added
       field selection -- whatever the first operand of the original field
       was, it's preserved by adding the right one of those two selections. */
    op = node->variant.operation.kind;
    new_selection_type = au_field->type;
    if (op == (an_expr_operator_kind)eok_value_field ||
        op == (an_expr_operator_kind)eok_value_bit_field) {
      /* These operators take an rvalue as their input, so use an
         eok_value_field for the added field selection. */
      new_op = (an_expr_operator_kind)eok_value_field;
    } else {
      /* These operators take an lvalue as their input, so use an
         eok_field for the added field selection. */
      new_op = (an_expr_operator_kind)eok_field;
      new_selection_type = make_pointer_type(new_selection_type);
    }  /* if */
    au_field_node = alloc_expr_node((an_expr_node_kind)enk_field);
    au_field_node->type = au_field->type;
    au_field_node->variant.field = au_field;
    op1->next = au_field_node;
    new_op1 = make_operator_node(new_op, new_selection_type, op1);
    /* Attach the new selection to the original selection. */
    new_op1->next = op2;
    node->variant.operation.operands = new_op1;
    /* Loop to see if the rewritten first operand still refers to an
       anonymous union field (because there are several nested anonymous
       unions), and if so, to rewrite it. */
    node = new_op1;
  }  /* for */
}  /* adjust_field_selection_for_anonymous_union_references */


static an_expr_node_ptr au_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field)
/*
Make an expression for an lvalue reference to field "field" of "node" and
return a pointer to it.  Differs from field_lvalue_selection_expr in that
it will deal with fields of anonymous unions and adding the necessary
intermediate field selections.
*/
{
  node = field_lvalue_selection_expr(node, field);
  adjust_field_selection_for_anonymous_union_references(node);
  return node;
}  /* au_field_lvalue_selection_expr */


static an_expr_node_ptr add_cast(an_expr_node_ptr node,
                                 a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.
*/
{
  return make_operator_node((an_expr_operator_kind)eok_cast, new_type, node);
}  /* add_cast */


static an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                              a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.  If the
type of the node is already new_type return the original node.
*/
{
  if (!il_identical_types(node->type, new_type)) {
    node = add_cast(node, new_type);
  }  /* if */
  return node;
}  /* add_cast_if_necessary */


static void change_to_cast(an_expr_node_ptr node,
                           an_expr_node_ptr operand_node,
                           a_type_ptr       new_type)
/*
Change an existing node into a cast of operand_node to new_type.
*/
{
  set_expr_node_kind(node, (an_expr_node_kind)enk_operation);
  set_node_operator(node, (an_expr_operator_kind)eok_cast,
                    new_type, operand_node);
  node->variant.operation.compiler_generated = TRUE;
}  /* change_to_cast */


static a_type_ptr char_star_type(void)
/*
Make and return a "char *" type.
*/
{
  return make_pointer_type(integer_type(plain_char_int_kind));
}  /* char_star_type */


static a_type_ptr void_star_type(void)
/*
Make and return a "void *" type.
*/
{
  return make_pointer_type(void_type());
}  /* void_star_type */


static an_expr_node_ptr add_cast_to_char_star(an_expr_node_ptr node)
/*
Add a cast to "char *" to the node and return the cast node.  If the
type of the node is already "char *" return the original node.
*/
{
  return add_cast_if_necessary(node, char_star_type());
}  /* add_cast_to_char_star */


static a_field_ptr field_at_offset(a_type_ptr    class_type,
                                   a_targ_size_t byte_offset)
/*
Return a pointer to the field at the indicated byte offset of the indicated
class type.
*/
{
  a_field_ptr   field_ptr;
  a_targ_size_t bit_offset = byte_offset * TARG_CHAR_BIT;

#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("field_at_offset: bad class type");
  }  /* if */
#endif /* CHECKING */
#if 0
  /* It may be necessary to come up with a faster way of doing this, such
     as storing two field pointers in the base class entry and a pointer to
     the virtual function table pointer field in the class type supplement. */
#endif
  for (field_ptr = class_type->variant.class_struct_union.field_list;
       ;
       field_ptr = field_ptr->next) {
#if CHECKING
    if (field_ptr == NULL) {
#if DEBUG
      db_abbreviated_type(class_type);
      fprintf(f_debug, ", byte offset = %lu\n", (unsigned long)byte_offset);
#endif /* DEBUG */
      internal_error("field_at_offset: field not found");
    }  /* if */
#endif /* CHECKING */
    if (field_ptr->bit_offset == bit_offset) break;
  }  /* for */
  return field_ptr;
}  /* field_at_offset */


static an_expr_node_ptr make_vbptr_field_lvalue(an_expr_node_ptr node,
                                                a_base_class_ptr bcp)
/*
Make an lvalue for the virtual base class pointer field for the base class
indicated by bcp of the object pointed to by node.
*/
{
  a_type_ptr       class_type, pointer_class_type;
  a_base_class_ptr pointer_bcp;
  a_targ_size_t    pointer_offset;

  class_type = type_pointed_to(node->type);
  class_type = skip_typerefs(class_type);
#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("make_vbptr_field_lvalue: not class type");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(class_type);
  pointer_class_type = class_type;
  pointer_offset = bcp->pointer_offset;
  pointer_bcp = bcp->pointer_base_class;
  if (pointer_bcp != NULL) {
    /* The pointer to the virtual base class is allocated in a base class.
       Cast down to the proper base class. */
    node = make_base_class_lvalue(node, pointer_bcp);
    /* The following line does not use pointer_bcp->type because the type here
       could be either that type or the corresponding type-as-subobject. */
    pointer_class_type = f_skip_typerefs(type_pointed_to(node->type));
#if CHECKING
    if (pointer_bcp->is_virtual) {
      internal_error("make_vbptr_field_lvalue: pointer_base_class is virtual");
    }  /* if */
#endif /* CHECKING */
    pointer_offset -= pointer_bcp->offset;
  }  /* if */
  /* Generate the field selection in the original class or the base class
     we got to. */
  node = field_lvalue_selection_expr(node,
                                     field_at_offset(pointer_class_type,
                                                     pointer_offset));
  return node;
}  /* make_vbptr_field_lvalue */


static an_expr_node_ptr make_vbptr_field_lvalue_from_var(a_variable_ptr   var,
                                                         a_base_class_ptr bcp)
/*
Make an lvalue for the virtual base class pointer field for the base class
indicated by bcp of the object pointed to by var.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_vbptr_field_lvalue(node, bcp);
  return node;
}  /* make_vbptr_field_lvalue_from_var */


static an_expr_node_ptr make_vptr_field_lvalue(an_expr_node_ptr node)
/*
Make an lvalue for the virtual table pointer of the object pointed to by node.
*/
{
  a_type_ptr                  class_type, vptr_class_type;
  a_targ_size_t               vptr_offset;
  a_base_class_ptr            vptr_bcp;
  a_class_type_supplement_ptr ctsp;

  class_type = type_pointed_to(node->type);
  class_type = skip_typerefs(class_type);
#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("make_vptr_field_lvalue: not class type");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(class_type);
  ctsp = class_type->variant.class_struct_union.extra_info;
  vptr_class_type = class_type;
  vptr_offset = ctsp->virtual_function_info_offset;
  vptr_bcp = ctsp->virtual_function_info_base_class;
  if (vptr_bcp != NULL) {
    /* The pointer to the virtual function table is allocated in a base
       class.  Cast down to the proper base class. */
    node = make_base_class_lvalue(node, vptr_bcp);
    /* The following line does not use vptr_bcp->type because the type here
       could be either that type or the corresponding type-as-subobject. */
    vptr_class_type = f_skip_typerefs(type_pointed_to(node->type));
#if CHECKING
    if (vptr_bcp->is_virtual) {
      internal_error(
        "make_vptr_field_lvalue: virtual_function_info_base_class is virtual");
    }  /* if */
#endif /* CHECKING */
    vptr_offset -= vptr_bcp->offset;
  }  /* if */
  /* Generate the field selection in the original class or the base class
     we got to. */
  node = field_lvalue_selection_expr(node,
                                     field_at_offset(vptr_class_type,
                                                     vptr_offset));
  return node;
}  /* make_vptr_field_lvalue */


static an_expr_node_ptr make_vptr_field_lvalue_from_var(a_variable_ptr var)
/*
Make an lvalue for the virtual table pointer of the object pointed to by var.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_vptr_field_lvalue(node);
  return node;
}  /* make_vptr_field_lvalue_from_var */


static an_expr_node_ptr make_base_class_lvalue(an_expr_node_ptr node,
                                               a_base_class_ptr bcp)
/*
Make an expression node that is an lvalue for the base class bcp of the
class object pointed to by node.  Return a pointer to the new node.  Does not
assume that node points to a complete object.
*/
{
  a_type_ptr            class_type, step_class_type, node_class_type;
  a_derivation_step_ptr dsp;
  a_base_class_ptr      derivation_bcp, step_bcp;

  class_type = type_pointed_to(node->type);
  class_type = skip_typerefs(class_type);
#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("make_base_class_lvalue: not class type");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(class_type);
  /* Put out a field selection or virtual base class pointer indirection
     for each step in the derivation. */
  /* Two class type variables are needed because the fields for the base
     classes may have the type of the base class as a subobject or the type
     of the base class itself (that's what node_class_type will contain)
     and the base class entries have the type of the base class itself
     (that's what step_class_type will contain). */
  node_class_type = step_class_type = class_type;
  for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
    /* The base class entry pointed to by dsp->base_class is the base
       class entry relative to the original class type.  Find the base
       class entry for this step relative to the intermediate class we
       have gotten to. */
    step_bcp = derivation_bcp = dsp->base_class;
    /* For the first step the information is already correct. */
    if (dsp != bcp->derivation) {
      step_bcp = find_direct_base_class_of(step_class_type, step_bcp->type);
    }  /* if */
    if (!step_bcp->is_virtual) {
      /* Non-virtual step. */
      node = field_lvalue_selection_expr(node,
                                         field_at_offset(node_class_type,
                                                         step_bcp->offset));
    } else {
      /* Virtual step.  Indirect through the base class pointer. */
      node = make_vbptr_field_lvalue(node, step_bcp);
      node = add_indirection_to_node(node);
    }  /* if */
    step_class_type = derivation_bcp->type;
    node_class_type = type_pointed_to(node->type);
    node_class_type = skip_typerefs(node_class_type);
#if CHECKING
    if (node_class_type != step_class_type &&
        node_class_type != step_class_type->variant.class_struct_union.
                                               extra_info->type_as_subobject) {
      internal_error("make_base_class_lvalue: node has wrong type");
    }  /* if */
#endif /* CHECKING */
  }  /* for */
  return node;
}  /* make_base_class_lvalue */


static an_expr_node_ptr make_base_class_lvalue_from_var(a_variable_ptr   var,
                                                        a_base_class_ptr bcp)
/*
Make an expression node that is an lvalue for the base class bcp of the
class pointed to by var.  Return a pointer to the node.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_base_class_lvalue(node, bcp);
  return node;
}  /* make_base_class_lvalue_from_var */


static an_expr_node_ptr make_cobj_vbase_class_lvalue(an_expr_node_ptr node,
                                                     a_base_class_ptr bcp)
/*
Make an expression node that is an lvalue for the base class bcp of the
class pointed to by node.  Return a pointer to the new node.  node is
assumed to point at a complete object.
*/
{
  a_type_ptr    class_type, data_section_class_type;
  a_targ_size_t data_section_offset;

  class_type = type_pointed_to(node->type);
  class_type = skip_typerefs(class_type);
#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("make_cobj_vbase_class_lvalue: not class type");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(class_type);
  data_section_class_type = class_type;
  data_section_offset = bcp->offset;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  { a_base_class_ptr data_section_bcp = bcp->data_section_base_class;
    if (data_section_bcp != NULL) {
      /* The virtual base class is allocated in a base class.  Get the address
         of the proper base class. */
      if (data_section_bcp->is_virtual) {
        node = make_cobj_vbase_class_lvalue(node, data_section_bcp);
      } else {
        node = make_base_class_lvalue(node, data_section_bcp);
      }  /* if */
      /* The following line does not use data_section_bcp->type because the
         type here could be either that type or the corresponding
         type-as-subobject. */
      data_section_class_type = f_skip_typerefs(type_pointed_to(node->type));
      data_section_offset -= data_section_bcp->offset;
    }  /* if */
  }
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  node = field_lvalue_selection_expr(node,
                                     field_at_offset(data_section_class_type,
                                                     data_section_offset));
  return node;
}  /* make_cobj_vbase_class_lvalue */


static an_expr_node_ptr make_cobj_vbase_class_lvalue_from_var(
                                                         a_variable_ptr   var,
                                                         a_base_class_ptr bcp)
/*
Make an expression node that is an lvalue for the base class bcp of the
class pointed to by node.  Return a pointer to the new node.  node is
assumed to point at a complete object.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_cobj_vbase_class_lvalue(node, bcp);
  return node;
}  /* make_cobj_vbase_class_lvalue_from_var */


static a_boolean cannot_be_null(an_expr_node_ptr expr)
/*
Return TRUE if the value of the indicated expression cannot be NULL (0).
This routine is used for an optimization, so it doesn't have to be perfect.
The safe return value is FALSE.
*/
{
  a_boolean cannot_be = FALSE;

  if (expr->kind == (an_expr_node_kind)enk_constant) {
    a_constant_ptr con = expr->variant.constant;
    if (con->kind == (a_constant_repr_kind)ck_integer) {
      /* An integer constant is non-NULL if it's non-zero. */
      cannot_be = !eqlit_integer_constant(con, 0L);
    } else if (con->kind == (a_constant_repr_kind)ck_address) {
      /* An address constant cannot be NULL. */
      cannot_be = TRUE;
    }  /* if */
  } else if (expr->kind == (an_expr_node_kind)enk_variable_address ||
             expr->kind == (an_expr_node_kind)enk_routine_address) {
    /* A variable or routine cannot have an address that's NULL. */
    cannot_be = TRUE;
  } else if (is_operation_node(expr)) {
    an_expr_operator_kind op = expr->variant.operation.kind;
    an_expr_node_ptr      operand = expr->variant.operation.operands;
    if (op == (an_expr_operator_kind)eok_field) {
      /* The address of a field reference can't be NULL in a legal program,
         but watch out for ((struct s *)0)->i. */
      cannot_be = (operand->next->variant.field->bit_offset != 0 ||
                   cannot_be_null(operand));
    } else if (op == (an_expr_operator_kind)eok_base_class_cast ||
               op == (an_expr_operator_kind)eok_derived_class_cast) {
      /* A cast to a related class will not turn a non-NULL into a NULL. */
      cannot_be = cannot_be_null(operand);
    }  /* if */
  } else if (expr->kind == (an_expr_node_kind)enk_variable) {
    /* If the expression if the "this" variable for the current function,
       it cannot be null. */
    if (nearest_this_param_variable == expr->variant.variable) {
      cannot_be = TRUE;
    }  /* if */
  }  /* if */
  return cannot_be;
}  /* cannot_be_null */


static an_expr_operator_kind lowered_ptr_to_member_assignment_operator(
                                                               a_type_ptr type)
/*
type is (or was, before lowering) a pointer-to-member type.  Return the
expression operator that does assignment (in lowered form) for that type.
*/
{
  an_expr_operator_kind op;

  if (is_or_was_ptr_to_member_function_type(type)) {
    /* Pointer to member function assignment. */
    op = (an_expr_operator_kind)eok_sassign;
  } else {
    /* Pointer to data member assignment. */
    op = (an_expr_operator_kind)eok_iassign;
  }  /* if */
  return op;
}  /* lowered_ptr_to_member_assignment_operator */


static an_expr_operator_kind lowered_assignment_operator(a_type_ptr type)
/*
Return the expression operator that does assignment for operands of the
indicated type.
*/
{
  an_expr_operator_kind op;

  op = which_binary_operator(tok_assign, type);
  if (op == (an_expr_operator_kind)eok_pmassign) {
    /* Pointer-to-member assignment.  Lower now. */
    op = lowered_ptr_to_member_assignment_operator(type);
  }  /* if */
  return op;
}  /* lowered_assignment_operator */


static an_expr_node_ptr make_reusable_copy(an_expr_node_ptr expr)
/*
Return a copy of the expression tree pointed to by expr.  If the expression
has side effects, the original expression will be changed so that its value is
stored in a temporary, and the copy will reference the temporary.
expr should be an rvalue (although make_lvalue_reusable_copy calls this
routine after it has discarded the troublesome lvalue cases).
*/
{
  an_expr_node_ptr expr_copy, temp_node;
  a_variable_ptr   temp;
  a_type_ptr       temp_type;

  if (!node_has_side_effects(expr)) {
    /* Node has no side effects, so a straight copy will work. */
    expr_copy = copy_expr_tree(expr);
  } else {
    /* Change the original expression to assign the value to a temporary. */
    temp_type = expr->type;
#if CHECKING
    /* Values of class types that have copy constructors can't be copied this
       way.  If such things did come up, they would probably come up
       as enk_temp_init nodes, and one could change to the address of the
       class temporary and store that in the temporary here. */
    if (is_class_struct_union_type(temp_type) &&
        !symbol_supplement_for_class(temp_type)->
                                        construction_by_bitwise_copy_allowed) {
      internal_error("make_reusable_copy: temp of class type with cctor");
    }  /* if */
#endif /* CHECKING */
    temp = make_temporary(temp_type);
    temp_node = var_lvalue_expr(temp);
    expr_copy = copy_node(expr);
    temp_node->next = expr_copy;
    set_expr_node_kind(expr, (an_expr_node_kind)enk_operation);
    set_node_operator(expr, lowered_assignment_operator(temp_type),
                      temp_type, temp_node);
    /* Make a reference to the temporary as the copy. */
    expr_copy = var_rvalue_expr(temp);
  }  /* if */
  return expr_copy;
}  /* make_reusable_copy */


static an_expr_node_ptr make_lvalue_reusable_copy(an_expr_node_ptr expr)
/*
Return a copy of the expression tree pointed to by expr.  If the expression
has side effects, the original expression will be changed so that its value is
stored in a temporary, and the copy will reference the temporary.
expr should be an lvalue.
*/
{
  a_boolean             special_case = FALSE;
  an_expr_node_ptr      expr_copy, operand1, operand2, operand3;
  an_expr_node_ptr      operand1_copy, operand2_copy, operand3_copy;
  an_expr_operator_kind op;

#if 0
  /* Something may be required for register lvalues. */
#endif /* 0 */
  if (is_operation_node(expr)) {
    op = expr->variant.operation.kind;
    operand1 = expr->variant.operation.operands;
    if (op == (an_expr_operator_kind)eok_bit_field) {
      /* For a bit-field reference, make a reusable copy of the struct
         address, then add the bit field selection to that. */
      special_case = TRUE;
      operand2 = operand1->next;
      operand1_copy = make_lvalue_reusable_copy(operand1);
      expr_copy = field_lvalue_selection_expr(operand1_copy,
                                              operand2->variant.field);
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* For a "?" operator, make reusable copies of all three operands,
         and a new "?" that uses the reusable copies. */
      special_case = TRUE;
      operand2 = operand1->next;
      operand3 = operand2->next;
      operand3_copy = make_lvalue_reusable_copy(operand3);
      operand2_copy = make_lvalue_reusable_copy(operand2);
      operand2_copy->next = operand3_copy;
      operand1_copy = make_reusable_copy(operand1);
      operand1_copy->next = operand2_copy;
      expr_copy = make_operator_node((an_expr_operator_kind)eok_question,
                                     expr->type, operand1_copy);
    } else if (op == (an_expr_operator_kind)eok_comma) {
      /* For a "," operator, make a reusable copy of the second operand. */
      special_case = TRUE;
      operand2 = operand1->next;
      expr_copy = make_lvalue_reusable_copy(operand2);
    }  /* if */
  }  /* if */
  if (!special_case) {
    /* For other cases, use the rvalue copy. */
    expr_copy = make_reusable_copy(expr);
  }  /* if */
  return expr_copy;
}  /* make_lvalue_reusable_copy */


static void overwrite_node(an_expr_node_ptr node,
                           an_expr_node_ptr source_node)
/*
Overwrite the expression node "node" with the contents of the node
"source_node".  This is used to remove do-nothing nodes by promoting
their operands.
*/
{
  an_expr_node_ptr node_next = node->next;

  /* Copy the node.  Preserve the original "next" field. */
  /* Note that the new/delete supplement from the source node is used
     by the destination node; no copy is needed. */
  *node = *source_node;
  node->next = node_next;
}  /* overwrite_node */


static a_type_ptr make_function_type(a_type_ptr return_type,
                                     a_type_ptr param_1_type)
/*
Make a function type for a prototyped function prototyped as
having a parameter with type param_1_type and returning return_type,
and return a pointer to it.  If no arguments are desired, param_1_type
should be specified as NULL.
*/
{
  a_type_ptr       rout_type;
  a_param_type_ptr ptp;

  rout_type = alloc_type((a_type_kind)tk_routine);
  rout_type->variant.routine.return_type = return_type;
  rout_type->variant.routine.extra_info->prototyped =
                                              !MAKE_ALL_FUNCTIONS_UNPROTOTYPED;
  if (param_1_type != NULL) {
    ptp = alloc_param_type(param_1_type);
    /* It is not necessary to clear il_lowering_flag; the entry does not need
       to be lowered. */
    rout_type->variant.routine.extra_info->param_type_list = ptp;
  }  /* if */
  return rout_type;
}  /* make_function_type */


static a_routine_ptr make_rout_entry(char            *name,
                                     a_storage_class rout_storage_class,
                                     a_type_ptr      return_type,
                                     a_type_ptr      param_1_type)
/*
Make a routine entry for a function with the given name, prototyped as
having a parameter with type param_1_type and returning return_type,
and having storage class rout_storage_class.  Return a pointer to the
routine entry created.  The routine entry and its type are allocated
in the file scope.  If no arguments are desired, param_1_type should
be specified as NULL.  The name may be NULL.
*/
{
  a_routine_ptr rout;
  a_type_ptr    rout_type;
  sizeof_t      alloc_length;

  rout_type = make_function_type(return_type, param_1_type);
  rout = alloc_routine();
  if (name != NULL) {
    alloc_length = strlen(name)+1;
    rout->source_corresp.name = strcpy(alloc_il(alloc_length), name);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  }  /* if */
  rout->storage_class = rout_storage_class;
  rout->source_corresp.name_linkage =
        (rout_storage_class == (a_storage_class)sc_unspecified ||
         rout_storage_class == (a_storage_class)sc_extern) ?
                                            (a_name_linkage_kind)nlk_external :
        (rout_storage_class == (a_storage_class)sc_static) ?
                                            (a_name_linkage_kind)nlk_internal :
        /* Otherwise: */
                                            (a_name_linkage_kind)nlk_none;
  rout->type = rout_type;
  /* Add the routine to the file scope list. */
  add_to_routines_list(rout, /*at_file_scope=*/TRUE);
  return rout;
}  /* make_rout_entry */


static a_scope_ptr make_routine_definition(a_routine_ptr          rout_ptr,
                                           a_boolean              make_return,
                                           a_memory_region_number *il_region)
/*
Make a definition for the given routine, i.e., create a new memory region,
scope, and top-level block.  Return the address of the scope created,
and return the memory region number of the IL memory region in *il_region.
If make_return is TRUE, a return statement will be put into the top-level
block.
*/
{
  a_scope_ptr            scope;
  a_memory_region_number region_to_switch_back_to = curr_il_region_number;
  a_statement_ptr        block_stmt;

  /* Make a new memory region and scope. */
  scope = new_il_region((a_scope_kind)sck_function, next_scope_number++,
                        rout_ptr);
  *il_region = curr_il_region_number;
  /* Link the routine to the scope.  new_il_region did the link in the
     other direction. */
  rout_ptr->type->variant.routine.extra_info->assoc_routine = rout_ptr;
  rout_ptr->assoc_scope = curr_il_region_number;
  /* Make the top-level block statement. */
  scope->assoc_block = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  if (make_return) {
    /* Make a return statement as the end of the block. */
    block_stmt->variant.block.statements =
                                alloc_statement((a_statement_kind)stmk_return);
  }  /* if */
  switch_il_region(region_to_switch_back_to);
  return scope;
}  /* make_routine_definition */


static a_routine_ptr make_runtime_routine(char          *name,
                                          a_routine_ptr *routine,
                                          a_type_ptr    return_type)
/*
Make a routine entry for the runtime routine named "name" and return a
pointer to it.  Also save the pointer in *routine.  If *routine is non-NULL
on entry, use that pointer.  The routine has unprototyped arguments and
its return type is return_type.
*/
{
  if (*routine == NULL) {
    *routine = make_rout_entry(name, (a_storage_class)sc_extern, return_type,
                               (a_type_ptr)NULL);
    (*routine)->type->variant.routine.extra_info->prototyped = FALSE;
  }  /* if */
  return *routine;
}  /* make_runtime_routine */


static void insert_expr(an_expr_node_ptr       inserted_expr,
                        an_insert_location_ptr insert_location)
/*
Insert the expression inserted_expr at *insert_location, which is an insert
location within an expression.  Update *insert_location so the next insertion
will be after the expression added.
*/
{
  an_expr_node_ptr orig_expr, orig_expr_copy, orig_expr_next;
  an_expr_node_ptr first_operand, second_operand;

#if CHECKING
  if (!insert_location->expr_insert) {
    internal_error("insert_expr: insert location is not expr insert");
  }  /* if */
#endif /* CHECKING */
  orig_expr = insert_location->variant.expr.ptr;
  /* Make a comma node that has the original node and the expression
     being inserted as its operands.  The original node is actually copied
     so that the comma node can be put at the address of the original node. */
  orig_expr_copy = copy_node(orig_expr);
  /* Order the operands of the comma operator depending on whether the
     insertion is supposed to be before or after the original expression. */
  if (insert_location->variant.expr.insert_before) {
    first_operand = inserted_expr;
    second_operand = orig_expr_copy;
  } else {
    first_operand = orig_expr_copy;
    second_operand = inserted_expr;
  }  /* if */
  first_operand->next = second_operand;
  second_operand->next = NULL;
  /* Turn the original node into a comma node. */
  orig_expr_next = orig_expr->next;
  clear_expr_node(orig_expr, (an_expr_node_kind)enk_operation);
  orig_expr->next = orig_expr_next;
  set_node_operator(orig_expr, (an_expr_operator_kind)eok_comma,
                    second_operand->type, first_operand);
  /* Change the insert location so that it inserts after the
     comma operator just created. */
  insert_location->variant.expr.insert_before = FALSE;
}  /* insert_expr */


static void insert_statement(a_statement_ptr        statement,
                             an_insert_location_ptr insert_location)
/*
Insert the statement "statement" at *insert_location.  Update *insert_location
so the next insertion will be after the statement added.
*/
{
  a_statement_ptr insert_stmt;

  if (insert_location->expr_insert) {
    /* Insert within an expression. */
#if CHECKING
    if (statement->kind != (a_statement_kind)stmk_expr) {
      internal_error("insert_statement: cannot insert non-expr statement");
    }  /* if */
#endif /* CHECKING */
    /* Note that the expression statement is just discarded. */
    insert_expr(statement->expr, insert_location);
  } else {
    /* Insert within a statement sequence. */
    insert_stmt = insert_location->variant.statement.ptr;
#if CHECKING
    if (insert_stmt == NULL) {
      internal_error("insert_statement: insert_stmt is NULL");
    }  /* if */
#endif /* CHECKING */
    if (insert_location->variant.statement.insert_at_block_start) {
      /* Insert at the start of a block. */
      statement->next = insert_stmt->variant.block.statements;
      insert_stmt->variant.block.statements = statement;
    } else {
      /* Normal case -- insert after insert_stmt. */
      statement->next = insert_stmt->next;
      insert_stmt->next = statement;
    }  /* if */
    /* Set *insert_location for the next insert. */
    set_insert_location(statement, insert_location);
  }  /* if */
}  /* insert_statement */


static void insert_if_statement(an_expr_node_ptr       test_expr,
                                an_insert_location_ptr insert_location,
                                an_insert_location_ptr insert_location2)
/*
Create an "if" statement that tests test_expr, and insert it at
insert_location.  Set insert_location2 to allow insertion of the
dependent statements of the "if".  This routine also handles the
case of inserting an if-equivalent into the middle of an expression.
*/
{
  a_statement_ptr  if_stmt, block_stmt;
  an_expr_node_ptr question_node, op2_node, op3_node, zero_node;
  a_type_ptr       void_type_ptr;

  if (insert_location->expr_insert) {
    /* Insert within an expression. */
    /* Insert "test_expr ? (void)0 : (void)0" at the right place. */
    void_type_ptr = void_type();
    /* The second and third operands are each "(void)0". */
    zero_node = node_for_integer_constant(0L, (an_integer_kind)ik_int);
    op2_node = make_operator_node((an_expr_operator_kind)eok_cast,
                                  void_type_ptr, zero_node);
    zero_node = node_for_integer_constant(0L, (an_integer_kind)ik_int);
    op3_node = make_operator_node((an_expr_operator_kind)eok_cast,
                                  void_type_ptr, zero_node);
    test_expr->next = op2_node;
    op2_node->next = op3_node;
    question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                       void_type_ptr, test_expr);
    insert_expr(question_node, insert_location);
    /* The insert location is before the "(void)0" of the second operand. */
    set_expr_insert_location(op2_node, insert_location2);
  } else {
    /* Insert within a statement sequence.  Allocate an "if" statement with
       a block statement under it. */
    if_stmt = alloc_statement((a_statement_kind)stmk_if);
    if_stmt->expr = test_expr;
    if_stmt->variant.if_stmt.then_statement = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
    insert_statement(if_stmt, insert_location);
    set_block_start_insert_location(block_stmt, insert_location2);
  }  /* if */
}  /* insert_if_statement */


static void enclose_routine_in_if(a_scope_ptr      scope,
                                  an_expr_node_ptr if_node,
                                  a_statement_ptr  *p_block_stmt,
                                  a_variable_ptr   return_var)
/*
Add an "if" statement around the entire body of the routine whose scope is
pointed to by scope.  if_node is the expression to be tested in the "if".
*block_stmt is set to point to the block statement that is made as the
dependent statement of the "if".  return_var is the variable to be returned
if a "return" statement must be generated, or NULL if no value needs
to be returned.
*/
{
  a_statement_ptr if_stmt, block_stmt, stmt, prev_stmt;

  if_stmt = alloc_statement((a_statement_kind)stmk_if);
  if_stmt->expr = if_node;
  if_stmt->variant.if_stmt.then_statement = *p_block_stmt = block_stmt =
                                 alloc_statement((a_statement_kind)stmk_block);
  /* Make the "if" the top-level statement in the routine, and put the
     original code under the "if". */
  block_stmt->variant.block.statements = stmt =
                                  scope->assoc_block->variant.block.statements;
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  scope->assoc_block->variant.block.statements = if_stmt;
  /* See if there is a return statement at the end of the original list of
     statements.  If so, move it outside the "if". */
  if (stmt != NULL) {
    for (prev_stmt = NULL;
         stmt->next != NULL;
         prev_stmt = stmt, stmt = stmt->next) {}
    if (stmt->kind == (a_statement_kind)stmk_return) {
      /* The last statement is a return.  Move it. */
      block_stmt->variant.block.extra_info->end_of_block_reachable = TRUE;
      if (prev_stmt == NULL) {
        block_stmt->variant.block.statements = NULL;
      } else {
        prev_stmt->next = NULL;
      }  /* if */
      if_stmt->next = stmt;
    }  /* if */
  }  /* if */
  /* If there is no return statement at the end of the routine (because the
     end of the original routine was not reachable), add one (because the
     end of the new routine is reachable if the "if" is not taken). */
  if (if_stmt->next == NULL) {
    a_statement_ptr return_stmt =
                                alloc_statement((a_statement_kind)stmk_return);
    if_stmt->next = return_stmt;
    if (return_var != NULL) return_stmt->expr = var_rvalue_expr(return_var);
  }  /* if */
}  /* enclose_routine_in_if */


/*
If variable != NULL, transfer the sequence number from it into stmt.
*/
#define transfer_seq_from_var_to_statement(variable, stmt)            \
{ if ((variable) != NULL && (stmt) != NULL) {                         \
    (stmt)->seq_number = (variable)->source_corresp.decl_position.seq;\
  }  /* if */                                                         \
}  /* transfer_seq_from_var_to_statement */


static a_statement_ptr insert_expr_statement(
                                        an_expr_node_ptr       node,
                                        an_insert_location_ptr insert_location)
/*
Make a statement from expression expr.  Insert the statement at
*insert_location and update *insert_location.  Return a pointer to the
statement, or NULL if no statement was created (in an expression insert
context).
*/
{
  a_statement_ptr stmt;

  if (insert_location->expr_insert) {
    /* Insert within an expression.  The statement need not be created. */
    insert_expr(node, insert_location);
    stmt = NULL;
  } else {
    /* Make the expression statement. */
    stmt = alloc_statement((a_statement_kind)stmk_expr);
    stmt->expr = node;
    /* Insert the statement at the right location. */
    insert_statement(stmt, insert_location);
  }  /* if */
  return stmt;
}  /* insert_expr_statement */


static a_statement_ptr insert_assignment_statement(
                                        an_expr_node_ptr       lvalue_expr,
                                        an_expr_operator_kind  op,
                                        an_expr_node_ptr       rvalue_expr,
                                        an_insert_location_ptr insert_location)
/*
Make a statement that assigns rvalue_expr to lvalue_expr using assignment
operator op.  Insert the statement at *insert_location and update
*insert_location.  Return a pointer to the statement, or NULL if no
statement was created (in an expression insert context).
*/
{
  a_statement_ptr  assign_stmt;
  an_expr_node_ptr assign_node;

  lvalue_expr->next = rvalue_expr;
  /* Make the assignment operation. */
  assign_node = make_operator_node(op, type_pointed_to(lvalue_expr->type),
                                   lvalue_expr);
  /* Make the expression statement. */
  assign_stmt = insert_expr_statement(assign_node, insert_location);
  return assign_stmt;
}  /* insert_assignment_statement */


static a_statement_ptr insert_var_assignment_statement(
                                        a_variable_ptr         lvalue_var,
                                        an_expr_operator_kind  op,
                                        an_expr_node_ptr       rvalue_expr,
                                        an_insert_location_ptr insert_location)
/*
Make a statement that assigns rvalue_expr to lvalue_var using assignment
operator op.  Insert the statement at *insert_location and update
*insert_location.  Return a pointer to the statement, or NULL if no
statement was created (in an expression insert context).
*/
{
  a_statement_ptr  assign_stmt;
  an_expr_node_ptr lvalue_expr;

  /* Make an expression for the lvalue address. */
  lvalue_expr = var_lvalue_expr(lvalue_var);
  /* Make and insert the assignment. */
  assign_stmt = insert_assignment_statement(lvalue_expr, op, rvalue_expr,
                                            insert_location);
  return assign_stmt;
}  /* insert_var_assignment_statement */


static void do_ptr_to_data_member_arg_promotion_on_node(an_expr_node_ptr expr)
/*
expr is an unprototyped argument of a function call, whose value is a
pointer to data member.  Do widening on it, needed because pointers
to data members are lowered into a small integer type.
*/
{
  a_type_ptr ptr_to_data_member_type = integer_type(TARG_DELTA_INT_KIND);
  a_type_ptr promoted_type =
                           default_argument_promotion(ptr_to_data_member_type);

  if (ptr_to_data_member_type != promoted_type) {
    /* Some widening is needed. */
    if (is_constant_node(expr)) {
      /* A constant.  Do the type change on a copy of the constant. */
      /* This must be done on a copy because the constant is typically shared
         and in the file scope, and therefore unlowerable at this point. */
      a_constant con;
      con = *expr->variant.constant;
      lower_ptr_to_member_constant(&con);
      /* Widen the constant by changing its type. */
#if CHECKING
      if (con.kind != (a_constant_repr_kind)ck_integer) {
        internal_error(
                "do_ptr_to_data_member_arg_promotion_on_node: pm not int con");
      }  /* if */
#endif /* CHECKING */
      con.type = promoted_type;
      /* Allocate a copy of the constant, and point the expression to it. */
      expr->variant.constant = alloc_shareable_constant(&con);
      expr->type = promoted_type;
    } else {
      /* Add a cast, but reuse the original node as the cast to preserve the
         expression address. */
      change_to_cast(expr, copy_node(expr), promoted_type);
    }  /* if */
  }  /* if */
}  /* do_ptr_to_data_member_arg_promotion_on_node */

#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED

static void do_default_arg_promotions_on_node(an_expr_node_ptr expr)
/*
expr is an argument to a call.  If necessary, add a cast to it to
do any default argument promotions needed to pass it as an argument to
an unprototyped function (or to an ellipsis position on a prototyped
function).
*/
{
  a_type_ptr arg_type = expr->type, promoted_type;

  /* Note that we drop type qualifiers so we won't add a cast to drop
     type qualifiers. */
  arg_type = skip_typerefs(arg_type);
  if (is_integral_type(arg_type)) {
    /* Note that default_argument_promotion is not used for integral
       expressions because special handling is required for bit fields. */
    promoted_type = node_type_after_integral_promotion(expr);
  } else if (is_floating_type(arg_type)) {
    promoted_type = default_argument_promotion(arg_type);
  } else if (is_or_was_ptr_to_data_member_type(arg_type)) {
    /* Widen pointers-to-data-members (which have been or will be turned into
       integers). */
    do_ptr_to_data_member_arg_promotion_on_node(expr);
    /* The subroutine does all the processing. */
    goto done;
  } else {
    promoted_type = arg_type;
  }  /* if */
  if (promoted_type != arg_type) {
    /* Put in the promotion cast. */
    an_expr_node_ptr expr_cast = expr, expr_next = expr->next;
    an_expr_node     node_copy;

    cast_node(&expr_cast, promoted_type, /*is_implicit_cast=*/TRUE,
              &error_position);
    expr_cast->next = expr_next;
    if (expr_cast != expr) {
      /* A cast was added, so swap the cast and the original node so that the
         cast node ends up at the original address. */
      node_copy = *expr;
      *expr = *expr_cast;
      *expr_cast = node_copy;
#if CHECKING
      if (!is_operation_node(expr) ||
          expr->variant.operation.kind != (an_expr_operator_kind)eok_cast ||
          expr->variant.operation.operands != expr) {
        internal_error("do_default_arg_promotions_on_node: bad cast node");
      }  /* if */
#endif /* CHECKING */
      expr->variant.operation.operands = expr_cast;
    }  /* if */
  }  /* if */
done:;
}  /* do_default_arg_promotions_on_node */

#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */

static an_expr_node_ptr make_call_node(a_routine_ptr    routine,
                                       an_expr_node_ptr arg_list,
                                       a_boolean        honor_virtual)
/*
Make an expression that calls routine "routine" with arguments "arg_list",
and return a pointer to it.  A virtual call is generated if the routine
is virtual and honor_virtual is TRUE.  The virtual call is *not* lowered.
*/
{
  an_expr_node_ptr      call_node, rout_node;
  a_type_ptr            rout_type, rout_return_type;
  an_expr_operator_kind op;

#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED
  /* If transforming all functions to old-style unprototyped form (for
     cfront compatibility), do default argument promotions on the arguments.
     It might seem wasteful to do this on every argument list, since
     not many of the arguments will require promotion.  However, doing it
     here guarantees that all calls created by IL lowering will have
     properly-promoted arguments without special-case checks all over the
     place. */
  { an_expr_node_ptr arg_node;
    for (arg_node = arg_list; arg_node != NULL; arg_node = arg_node->next) {
      do_default_arg_promotions_on_node(arg_node);
    }  /* for */
  }
#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */
  /* Make a node for the address of the routine. */
  rout_node = function_addr_expr(routine);
  routine->called = TRUE;
  rout_node->next = arg_list;
  /* Choose the right operation (virtual call or non-virtual call). */
  if (routine->is_virtual && honor_virtual) {
    op = (an_expr_operator_kind)eok_virtual_call;
  } else {
    op = (an_expr_operator_kind)eok_call;
    routine->source_corresp.referenced = TRUE;
  }  /* if */
  /* Make the call node. */
  rout_type = skip_typerefs(routine->type);
  rout_return_type = rout_type->variant.routine.return_type;
  call_node = make_operator_node(op, rout_return_type, rout_node);
  return call_node;
}  /* make_call_node */


static a_statement_ptr make_call_statement(a_routine_ptr    routine,
                                           an_expr_node_ptr arg_list)
/*
Make a statement that calls routine "routine" with arguments "arg_list",
and return pointer to it.
*/
{
  an_expr_node_ptr call_node;
  a_statement_ptr  call_stmt;

  /* Make the call node. */
  call_node = make_call_node(routine, arg_list, /*honor_virtual=*/FALSE);
  /* Allocate an expression statement and put the call into it. */
  call_stmt = alloc_statement((a_statement_kind)stmk_expr);
  call_stmt->expr = call_node;
  return call_stmt;
}  /* make_call_statement */


static an_expr_node_ptr make_runtime_rout_call(char             *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               an_expr_node_ptr arg_expr_list)
/*
Make an expression node that calls the runtime routine "name" with the
arguments given by arg_expr_list.  *routine is set to point to the runtime
routine entry; if it is non-NULL on entry, it is used.  The routine has
unprototyped arguments and its return type is return_type.
*/
{
  an_expr_node_ptr node;

  /* Make the routine entry if it does not exist already. */
  (void)make_runtime_routine(name, routine, return_type);
  /* Make the call node. */
  node = make_call_node(*routine, arg_expr_list, /*honor_virtual=*/FALSE);
  return node;
}  /* make_runtime_rout_call */


static sizeof_t digits_to_represent(unsigned long value)
/*
Return the number of digits needed for the decimal representation of value,
e.g., 1297 --> 4.
*/
{
  sizeof_t ndigits = 1;

  while (value > 9) {
    value /= 10;
    ndigits++;
  }  /* while */
  return ndigits;
}  /* digits_to_represent */


static sizeof_t mangled_encoding_for_function_type(a_type_ptr type,
                                                   char       *store_at)
/*
Determine the mangled encoding for the function type "type".  Place the
encoded form at *store_at if store_at != NULL, and (always) return the
length of the encoding.  See ARM 7.2.1c for name encoding.
*/
{
  sizeof_t                      mangled_name_length, section_length;
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param, existing_param;
  unsigned long                 existing_param_num, num_matching_types;
  sizeof_t                      digits;

  /* A mangled function type encoding is made up of:
       (1)  "F"
       (2)  For each parameter, the encoding for the type.  If a parameter
            has a type that has appeared already in the parameter list,
            "Tn" is used to repeat the type of parameter "n" ("n" can be
            a multi-digit number; the first parameter is numbered 1).
            If several consecutive parameters have the same type as a previous
            parameter, "Nmn" is used to indicate "m" repetitions of the
            type of parameter "n" ("n" is as for "Tn"; "m" is a one-digit
            number, so a maximum of 9 repetitions is possible).
            If the parameter list is empty, "v" for "void".
       (3)  If the parameter list ends with an ellipsis, "e".
     mangled_function_name takes care of putting out additional information
     preceding the "F" if the function is a member function.
  */
  mangled_name_length = 0;
  rtsp = type->variant.routine.extra_info;
  /* Add the "F" indicating a function type. */
  mangled_name_length++;
  if (store_at != NULL) *store_at++ = 'F';
  param = rtsp->param_type_list;
  if (param == NULL) {
    /* Void parameter list. */
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'v';
  } else {
    /* Output the parameter types. */
    for (; param != NULL; param = param->next) {
      /* See if the parameter type is the same as any existing parameter
         type. */
      for (existing_param = rtsp->param_type_list, existing_param_num = 1;
           existing_param != param;
           existing_param = existing_param->next, existing_param_num++) {
        if (types_are_compatible(existing_param->type, param->type)) {
          /* Found a type that is being reused.  See if there are more
             instances following this one, in which case we can use the "Nmn"
             encoding.  Stop when 9 matches are found, since that's the most
             that can be encoded in a single "Nmn" sequence. */
          for (num_matching_types = 1;
               num_matching_types < 9 && param->next != NULL &&
                 types_are_compatible(existing_param->type, param->next->type);
               num_matching_types++, param = param->next) {}
          if (num_matching_types == 1) {
            /* Only one match, so use the "Tn" form. */
            mangled_name_length++;
            if (store_at != NULL) *store_at++ = 'T';
          } else {
            /* More than one match, so use the "Nmn" form. */
            mangled_name_length++;
            if (store_at != NULL) *store_at++ = 'N';
            /* Output the "m" (repetition count). */
            digits = 1;  /* digits_to_represent(num_matching_types) */
            mangled_name_length += digits;
            if (store_at != NULL) {
              (void)sprintf(store_at, "%lu", num_matching_types);
              store_at += digits;
            }  /* if */
          }  /* if */
          /* Output the "n" (existing parameter number). */
          digits = digits_to_represent(existing_param_num);
          mangled_name_length += digits;
          if (store_at != NULL) {
            (void)sprintf(store_at, "%lu", existing_param_num);
            store_at += digits;
          }  /* if */
          goto arg_done;
        }  /* if */
      }  /* for */
      /* The parameter type does not match any of the previous parameter
         types, so just put it out. */
      section_length = mangled_encoding_for_type(param->type, store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
arg_done:;
    }  /* for */
  }  /* if */
  /* Output the final "e" for an ellipsis. */
  if (rtsp->has_ellipsis) {
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'e';
  }  /* if */
  return mangled_name_length;
}  /* mangled_encoding_for_function_type */


static sizeof_t literal_representation(a_constant_ptr con,
                                       char           *store_at)
/*
Place the literal form of the constant con at *store_at if store_at != NULL,
and (always) return the length of the literal representation.  This is
used to encode constants as part of the mangled names of template classes.
*/
{
  sizeof_t       literal_length, str_length, digits;
  char           *str;
  char           buffer[50];

  switch (con->kind) {
    case ck_integer:
      /* Integer: the encoding is like
           L3n12  <-- encoding for "-12"
              ^^----- Literal value.
             ^------- "n" indicates negative.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         This is compatible with cfront 3.0.1. */
      str = str_for_integer_constant(con);
      str_length = strlen(str);  /* Includes "-" sign if any. */
      digits = digits_to_represent((unsigned long)str_length);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        (void)sprintf(store_at, "%lu", (unsigned long)str_length);
        store_at += digits;
        (void)memcpy(store_at, str, size_t_arg(str_length));
        /* Use "n" to represent a minus sign. */
        if (*store_at == '-') *store_at = 'n';
        store_at += str_length;
      }  /* if */
      break;
    case ck_float:
      /* Float: the encoding is like
           L4n1p5 <-- encoding for "-1.5"
              ^^^---- Literal value ("p" for decimal point).
             ^------- "n" indicates negative.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         cfront 3.0.1 does not implement this, so we made it up. */
      /* Note that the fp_to_string conversion is not compact, so this
         makes a long name. */
      str = fp_to_string(skip_typerefs(con->type)->variant.float_kind,
                         &con->variant.float_value);
      str_length = strlen(str);  /* Includes "-" sign if any. */
      digits = digits_to_represent((unsigned long)str_length);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        (void)sprintf(store_at, "%lu", (unsigned long)str_length);
        store_at += digits;
        for (;str_length > 0; str_length--) {
          /* Move the string and recode non-alphanumeric characters. */
          char c = *str++;
          /* Use "n" to represent a minus sign. */
          if (c == '-') c = 'n';
          /* Use "d" to represent a decimal point. */
          if (c == '.') c = 'd';
          /* Use "p" to represent a plus sign. */
          if (c == '+') c = 'p';
          *store_at++ = c;
        }  /* for */
      }  /* if */
      break;
    case ck_address:
      /* Address.  Put out the name of the entity whose address is involved. */
      { a_variable_ptr       variable;
        a_type_ptr           class_type;
        a_routine_ptr        routine;
        an_address_base_kind abkind;
        a_targ_ptrdiff_t     offset;

        abkind = con->variant.address.kind;
#if CHECKING
        if (abkind == (an_address_base_kind)abk_constant) {
          internal_error("literal_representation: addr of const");
        }  /* if */
#endif /* CHECKING */
        /* Address of something other than a constant, i.e., a variable or
           routine.  The encoding is like
             4abcd <-- encoding for address of "abcd"
              ^^^^---- Name of entity.
             ^-------- Length of the name.
           This is compatible with cfront 3.0.1. */
        if (abkind == (an_address_base_kind)abk_variable) {
          variable = con->variant.address.variant.variable;
          class_type = variable->source_corresp.class_of_which_a_member;
          if (class_type != NULL) {
            /* Static data member. */
            str_length = mangled_static_data_member_name(variable,
                                                         class_type,
                                                         (char *)NULL);
          } else {
            /* Normal variable. */
            str = variable->source_corresp.name;
#if CHECKING
            if (str == NULL) {
              internal_error("literal_representation: addr of unnamed");
            }  /* if */
#endif /* CHECKING */
            str_length = strlen(str);
          }  /* if */
        } else {
#if CHECKING
          if (abkind != (an_address_base_kind)abk_routine) {
            internal_error("literal_representation: bad abkind");
          }  /* if */
#endif /* CHECKING */
          routine = con->variant.address.variant.routine;
          str_length = mangled_function_name(routine,
                                             /*suppress_param_encoding=*/TRUE,
                                             (char *)NULL);
        }  /* if */
        digits = digits_to_represent((unsigned long)str_length);
        literal_length = digits + str_length;
        if (store_at != NULL) {
          (void)sprintf(store_at, "%lu", (unsigned long)str_length);
          store_at += digits;
          if (abkind == (an_address_base_kind)abk_variable) {
            if (class_type != NULL) {
              /* Static data member. */
              (void)mangled_static_data_member_name(variable,
                                                    class_type,
                                                    store_at);
            } else {
              /* Normal variable. */
              (void)memcpy(store_at, str, size_t_arg(str_length));
            }  /* if */
          } else {
            (void)mangled_function_name(routine,
                                        /*suppress_param_encoding=*/TRUE,
                                        store_at);
          }  /* if */
          store_at += str_length;
        }  /* if */
        /* If the offset is non-zero, add it at the end, in a form similar
           to the integer constant form, except using "O", e.g., O3n12
           for -12.  This convention is not used by cfront; we invented it. */
        offset = con->variant.address.offset;
        if (offset != 0) {
          (void)sprintf(buffer, "%ld", (long)offset);
          str = buffer;
          str_length = strlen(str);  /* Includes "-" sign if any. */
          digits = digits_to_represent((unsigned long)str_length);
          literal_length += 1 + digits + str_length;
          if (store_at != NULL) {
            *store_at++ = 'O';
            (void)sprintf(store_at, "%lu", (unsigned long)str_length);
            store_at += digits;
            (void)memcpy(store_at, str, size_t_arg(str_length));
            /* Use "n" to represent a minus sign. */
            if (*store_at == '-') *store_at = 'n';
            store_at += str_length;
          }  /* if */
        }  /* if */
      }
      break;
    case ck_ptr_to_member:
      /* Pointer to member:
         For pointers to data members, the offset value encoded as
         an integer:
           L212  <--- encoding for an offset of "12"
             ^^------ Literal value.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         For pointers to member functions, the __mptr triplet of
         values (delta, index, function or offset), encoded as follows:
           LM0_L2n1_1j
                    ^^- Function name, or alternatively the offset value.
                        (e.g., LM0_L2n1_4)
               ^^^^---- Index value, encoded as an integer.
             ^--------- Delta value.
           ^^---------- "LM" indicates a pointer to member function.
         This is compatible with cfront 3.0.1.  Note that cfront always
         seems to put out "0" for the offset value, even when another
         value seems right. */
      if (!con->variant.ptr_to_member.is_function_ptr) {
        /* Pointer to data member. */
        a_targ_ptrdiff_t delta;
        repr_for_ptr_to_data_member_constant(con, &delta);
        (void)sprintf(buffer, "%ld", (long)delta);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        digits = digits_to_represent((unsigned long)str_length);
        literal_length = 1 + digits + str_length;
        if (store_at != NULL) {
          *store_at++ = 'L';
          (void)sprintf(store_at, "%lu", (unsigned long)str_length);
          store_at += digits;
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
        }  /* if */
      } else {
        /* Pointer to member function. */
        a_targ_ptrdiff_t delta, index, offset;
        a_routine_ptr    func;
        repr_for_ptr_to_member_function_constant(con, &delta, &index, &func,
                                                 &offset);
        literal_length = 2;  /* "LM" */
        if (store_at != NULL) {
          *store_at++ = 'L';
          *store_at++ = 'M';
        }  /* if */
        /* Delta value. */
        (void)sprintf(buffer, "%ld", (long)delta);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        literal_length += str_length;
        if (store_at != NULL) {
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
        }  /* if */
        /* Index value. */
        (void)sprintf(buffer, "%ld", (long)index);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        digits = digits_to_represent((unsigned long)str_length);
        literal_length += 2 + digits + str_length + 1;
        if (store_at != NULL) {
          *store_at++ = '_';
          *store_at++ = 'L';
          (void)sprintf(store_at, "%lu", (unsigned long)str_length);
          store_at += digits;
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
          *store_at++ = '_';
        }  /* if */
        if (func != NULL) {
          /* Name of function.  Note that this is the unmangled name. */
          str = func->source_corresp.name;
          /* Determine the size of the name.  Stop on two underscores. */
          for (str_length = 0;
               str[str_length] != '\0' &&
                 (str[str_length] != '_' || str[str_length+1] != '_');
               str_length++) {}
          digits = digits_to_represent((unsigned long)str_length);
          literal_length += digits + str_length;
          if (store_at != NULL) {
            (void)sprintf(store_at, "%lu", (unsigned long)str_length);
            store_at += digits;
            (void)memcpy(store_at, str, size_t_arg(str_length));
            store_at += str_length;
          }  /* if */
        } else {
          /* Offset. */
          (void)sprintf(buffer, "%ld", (long)offset);
          str = buffer;
          str_length = strlen(str);  /* Includes "-" sign if any. */
          literal_length += str_length;
          if (store_at != NULL) {
            (void)memcpy(store_at, str, size_t_arg(str_length));
            /* Use "n" to represent a minus sign. */
            if (*store_at == '-') *store_at = 'n';
            store_at += str_length;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
#if CHECKING
    case ck_string:
      /* Strings should be converted to addresses. */
    default:
      internal_error("literal_representation: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  return literal_length;
}  /* literal_representation */


/*
Seed number for unnamed class names.
*/
static unsigned long
		unnamed_class_name_seed;


static void give_unnamed_class_a_name(a_type_ptr type)
/*
If the indicated class type is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The class is unnamed, so make up a name. */
    /* The name is __Cnn, where nn is a unique number for the
       class.  This is not from the ARM.  cfront uses the __Cn form, but
       the number is different. */
    unnamed_class_name_seed++;
    name_len = digits_to_represent(unnamed_class_name_seed) + 4; /*"__C"+null*/
    name = alloc_il(name_len);
#if DEBUG
    allocated_name_string_length += name_len;
#endif /* DEBUG */
    (void)sprintf(name, "__C%lu", (unsigned long)unnamed_class_name_seed);
    type->source_corresp.name = name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_class_a_name */
    

static sizeof_t mangled_basic_class_name(a_type_ptr type,
                                         char       *store_at)
/*
Determine the mangled form of the basic name of the class "type".  This is
not the version that contains a leading count of the number of characters
in the name; here, the name is usually just the original name, but is
different if the class is a template class or is unnamed.  Place the mangled
name at *store_at if store_at != NULL, and (always) return the length of
the name.
*/
{
  sizeof_t           mangled_name_length, digits, arg_length, total_arg_length;
  sizeof_t           literal_length, type_length;
  char               *name;
  a_template_arg_ptr template_arg_list =
                            type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
  a_template_arg_ptr tap;
  a_constant_ptr     con;
  int                pass;

  /* Always start with the name of the class, which applies even in the
     template class case. */
  give_unnamed_class_a_name(type);
  name = type->source_corresp.name;
  mangled_name_length = strlen(name);
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(mangled_name_length));
    store_at += mangled_name_length;
  }  /* if */
  if (template_arg_list != NULL &&
      !type->source_corresp.name_has_been_mangled) {
    /* A template class.  The mangled form of the name is something like
         abc__pt__3_ii
                    ^^--- Two template arguments of type int.
                  ^------ Total length of template argument list string,
                          including the underscore.
              ^^--------- Fixed string, indicates "parameterized type".
         ^^^------------- The name of the class template.
    */
#define PT_STR "__pt__"
    mangled_name_length += sizeof(PT_STR) - 1;
    if (store_at != NULL) {
      (void)strcpy(store_at, PT_STR);
      store_at += sizeof(PT_STR) - 1;
    }  /* if */
#undef PT_STR
    /* Run through the template argument list, determining the representation
       for each argument.  The first time through, determine the size;
       the second, put out the string. */
    for (pass = 1; ; pass++) {
      total_arg_length = 0;
      for (tap = template_arg_list; tap != NULL; tap = tap->next) {
        if (tap->is_type) {
          /* Type argument. */
          if (pass == 1) {
            arg_length = mangled_encoding_for_type(tap->variant.type,
                                                   (char *)NULL);
          } else {
            type_length = mangled_encoding_for_type(tap->variant.type,
                                                    store_at);
            mangled_name_length += type_length;
            store_at += type_length;
          }  /* if */
        } else {
          /* Constant argument.  Representation is something like
               XCiL15   <-- integer constant 5
                    ^-- Literal constant representation.
                   ^--- Length of literal constant.
                  ^---- L indicates literal constant; c indicates address
                        of variable, etc.
                ^^----- Type of template argument, with "const" added.
               ^------- X indicates beginning of constant argument.
          */
          con = tap->variant.constant;
          if (pass == 1) {
            arg_length = 2; /* "XC" */
            arg_length += mangled_encoding_for_type(con->type, (char *)NULL);
            literal_length = literal_representation(con, (char *)NULL);
            arg_length += literal_length;
          } else {
            mangled_name_length += 2;
            *store_at++ = 'X';
            *store_at++ = 'C';
            type_length = mangled_encoding_for_type(con->type, store_at);
            mangled_name_length += type_length;
            store_at += type_length;
            literal_length = literal_representation(con, store_at);
            mangled_name_length += literal_length;
            store_at += literal_length;
          }  /* if */
        }  /* if */
        if (pass == 1) total_arg_length += arg_length;
      }  /* for */
      /* After the second pass, quit the loop. */
      if (pass == 2) break;
      /* First pass: */
      /* Put out the length of the entire argument section, and the "_". */
      total_arg_length++;  /* "_" */
      digits = digits_to_represent((unsigned long)total_arg_length);
      mangled_name_length += 1 + digits;
      if (store_at != NULL) {
        (void)sprintf(store_at, "%lu_", (unsigned long)total_arg_length);
        store_at += digits + 1;
      }  /* if */
      if (store_at == NULL) {
        /* If we are not storing, we do not need to do the second pass. */
        mangled_name_length += total_arg_length - 1;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return mangled_name_length;
}  /* mangled_basic_class_name */


static sizeof_t mangled_type_name(a_type_ptr    type,
                                  unsigned long nesting_level,
                                  char          *store_at)
/*
Determine the mangled form of the name of the type "type".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.  This routine is
used for named types (classes, enums, and typedefs) and for unnamed classes.
A top-level call is made with nesting_level == 1; this routine then makes
recursive calls to itself with higher nesting levels to process the
initial parts of the qualified names.
*/
{
  sizeof_t   mangled_name_length, name_length;
  char       *name;
  sizeof_t   digits;
  a_type_ptr parent_class;

  /* The mangled form of a type name is the type name with a length
       preceding it:
         AB          --> 2AB
         ABCDEFGHIJK --> 11ABCDEFGHIJK
     The ARM (7.2.1c) also gives a syntax for encoding qualified class names,
     like "outer::inner", using a "Q" description:
       Q2_5outer5inner
          ^-----^-----mangled class names, outer to inner
        ^----count of levels of qualification
     Note that the ARM description does not include the underscore, which
     is necessary if you allow more than 9 levels of nesting.
  */
  mangled_name_length = 0;
  parent_class = type->source_corresp.class_of_which_a_member;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  /* If this a nested type name promoted into the file scope in
     cfront 2.1 mode, do not use the nested form. */
  if (type->use_cfront_transitional_nested_type_name_mangling) {
  } else
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  if (parent_class != NULL) {
    /* Nested type.  Do the containing class names. */
    name_length = mangled_type_name(parent_class, nesting_level+1, store_at);
    mangled_name_length += name_length;
    if (store_at != NULL) store_at += name_length;
  } else {
    /* Got to the topmost class. */
    /* If the class is a local class, put out "Lnn__" using the declaration
       scope number for "nn".  This is not from the ARM.  cfront uses the
       same form but the numbers are probably different. */
    { a_symbol_ptr assoc_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      if (assoc_sym->decl_scope != scope_stack[DEPTH_OF_FILE_SCOPE].number) {
        /* This is a local name. */
        digits = digits_to_represent((unsigned long)assoc_sym->decl_scope);
        mangled_name_length += digits + 3;  /* "L" and "__" == 3 characters. */
        if (store_at != NULL) {
          (void)sprintf(store_at, "L%lu__",
                        (unsigned long)assoc_sym->decl_scope);
          store_at += digits + 3;
        }  /* if */
      }  /* if */
    }
    if (nesting_level > 1) {
      /* More than one level of nesting, so put out the "Qn_". */
      digits = digits_to_represent(nesting_level);
      mangled_name_length += 2 + digits;
      if (store_at != NULL) {
        /* Actually store the "Qn_". */
        (void)sprintf(store_at, "Q%lu_", nesting_level);
        store_at += 2 + digits;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Put the innermost type name onto the name. */
  /* The name is preceded by a count of the number of characters in
     the name. */
  if (is_immediate_class_type(type)) {
    /* Class name. */
    name_length = mangled_basic_class_name(type, (char *)NULL);
    digits = digits_to_represent((unsigned long)name_length);
    mangled_name_length += name_length + digits;
    if (store_at != NULL) {
      /* Actually store the name. */
      (void)sprintf(store_at, "%lu", (unsigned long)name_length);
      store_at += digits;
      store_at += mangled_basic_class_name(type, store_at);
    }  /* if */
  } else {
    /* Not a class name (typedef or enum). */
    name = type->source_corresp.name;
    name_length = strlen(name);
    digits = digits_to_represent((unsigned long)name_length);
    mangled_name_length += name_length + digits;
    if (store_at != NULL) {
      /* Actually store the name. */
      (void)sprintf(store_at, "%lu", (unsigned long)name_length);
      store_at += digits;
      (void)memcpy(store_at, name, size_t_arg(name_length));
      store_at += name_length;
    }  /* if */
  }  /* if */
  return mangled_name_length;
}  /* mangled_type_name */


static sizeof_t mangled_encoding_for_type(a_type_ptr type,
                                          char       *store_at)
/*
Determine the mangled encoding for the type "type".  Place the encoding at
*store_at if store_at != NULL, and (always) return the length of the name.
See ARM 7.2.1c for name encoding.
*/
{
  a_type_ptr named_type, named_typedef;
  sizeof_t   mangled_name_length, section_length;
  char       *s;
  a_boolean  is_const, is_volatile;

  mangled_name_length = 0;
  /* Walk through any typerefs above the type.  Remember type qualifiers,
     remember the bottommost named typedef, and skip down to the "real"
     underlying type. */
  named_typedef = NULL;
  is_const = is_volatile = FALSE;
  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    /* Remember type qualifiers encountered. */
    if (type->variant.typeref.is_const)    is_const = TRUE;
    if (type->variant.typeref.is_volatile) is_volatile = TRUE;
    /* Remember the bottommost named typedef encountered. */
    if (type->source_corresp.name != NULL) named_typedef = type;
  }  /* for */
  /* Put out type qualifiers, if any. */
  if (is_const) {
    mangled_name_length += 1;
    if (store_at != NULL) *store_at++ = 'C';
  }  /* if */
  if (is_volatile) {
    mangled_name_length += 1;
    if (store_at != NULL) *store_at++ = 'V';
  }  /* if */
  /* See if the type is a named class or enum. */
  named_type = NULL;
  if (is_enum_type(type)) {
    if (type->source_corresp.name != NULL) {
      /* Named enum. */
      named_type = type;
    } else {
      /* Unnamed enum; if there is a named typedef above the enum, use its
         name.  Note that we use the typedef name even it it's the name of
         a qualified version of the enum; that's what cfront does. */
      named_type = named_typedef;
    }  /* if */
  } else if (is_immediate_class_type(type)) {
    /* Class type. */
    if (type->source_corresp.name != NULL) named_type = type;
  }  /* if */
  /* If the type is named, use the name. */
  if (named_type != NULL) {
    /* Put out the mangled form of the name, e.g., "2AB" for "AB". */
    section_length = mangled_type_name(named_type, (unsigned long)1, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  } else {
    /* The type is not named, so develop a description string. */
    switch (type->kind) {
      case tk_void:
        s = "v";
        break;
      case tk_integer:
#if CHECKING
        if (type->variant.integer.enum_type) {
          internal_error("mangled_encoding_for_type: unnamed enum");
        }  /* if */
#endif /* CHECKING */
        switch (type->variant.integer.int_kind) {
          case ik_char:           s = "c";  break;
          case ik_signed_char:    s = "Sc"; break;
          case ik_unsigned_char:  s = "Uc"; break;
          case ik_short:          s = "s";  break;
          case ik_unsigned_short: s = "Us"; break;
          case ik_int:            s = "i";  break;
          case ik_unsigned_int:   s = "Ui"; break;
          case ik_long:           s = "l";  break;
          case ik_unsigned_long:  s = "Ul"; break;
#if LONG_LONG_ALLOWED
          case ik_long_long:      s = "ll"; break;
          case ik_unsigned_long_long:
                                  s = "Ull";break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
          default:
            internal_error("mangled_encoding_for_type: bad int kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_float:
        switch (type->variant.float_kind) {
          case fk_float:          s = "f";  break;
          case fk_double:         s = "d";  break;
          case fk_long_double:    s = "r";  break;
#if CHECKING
          default:
            internal_error("mangled_encoding_for_type: bad float kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_pointer:
        if (type->variant.pointer.is_reference) {
          s = "R";
        } else {
          s = "P";
        }  /* if */
        /* More of this below -- the "P" or "R" is followed by the
           type pointed to/referenced. */
        break;
      case tk_ptr_to_member:
        /* Pointer to member.  int S::* is put out as M1Si. */
        /* Put out "M". */
        s = "M";
        /* More of this below -- the "M" is followed by the class name and
           the type pointed to. */
        break;
      case tk_array:
        s = "A";
        /* More of this below -- int[10] is put out as A10_i. */
        break;
      case tk_routine:
        /* Function.  Put out the this-parameter-type (if any), "F",
           and the argument types. */
        mangled_name_length = mangled_encoding_for_function_type(type,
                                                                 store_at);
        if (store_at != NULL) store_at += mangled_name_length;
        /* Add the return type at the end, as "_" followed by the type. */
        mangled_name_length++;
        if (store_at != NULL) *store_at++ = '_';
        mangled_name_length +=
                  mangled_encoding_for_type(type->variant.routine.return_type,
                                            store_at);
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name will make up a name. */
        mangled_name_length = mangled_type_name(type, (unsigned long)1,
                                                store_at);
        goto have_whole_mangled_name;
#if CHECKING
      default:
        internal_error("mangled_encoding_for_type: bad type kind");
#endif /* CHECKING */
    }  /* switch */
    /* s is now set to a type description string to be output. */
    section_length = strlen(s);
    mangled_name_length += section_length;
    if (store_at != NULL) {
      (void)memcpy(store_at, s, size_t_arg(section_length));
      store_at += section_length;
    }  /* if */
    /* Do any processing needed after the description letter. */
    switch (type->kind) {
      case tk_pointer:
        /* Put out the type pointed to. */
        mangled_name_length +=
                          mangled_encoding_for_type(type->variant.pointer.type,
                                                    store_at);
        break;
      case tk_ptr_to_member:
        /* Put out the mangled name of the class for which this is a member
           pointer. */
        section_length = mangled_encoding_for_type(type->variant.ptr_to_member.
                                                       class_of_which_a_member,
                                                   store_at);
        mangled_name_length += section_length;
        if (store_at != NULL) store_at += section_length;
        /* Put out the type pointed to. */
        mangled_name_length +=
                    mangled_encoding_for_type(type->variant.ptr_to_member.type,
                                              store_at);
        break;
      case tk_array:
        /* Put out the array size, an underscore, and then the element type,
           i.e., int[10] is put out as A10_i. */
        section_length = digits_to_represent((unsigned long)type->variant.
                                                 array.number_of_elements) + 1;
        mangled_name_length += section_length;
        if (store_at != NULL) {
          (void)sprintf(store_at, "%lu_",
                        (unsigned long)type->variant.array.number_of_elements);
          store_at += section_length;
        }  /* if */
        mangled_name_length +=
                    mangled_encoding_for_type(type->variant.array.element_type,
                                              store_at);
        break;
      default:;
        /* Many cases don't require any handling. */
    }  /* switch */
  }  /* if */
have_whole_mangled_name:      
  return mangled_name_length;
}  /* mangled_encoding_for_type */


static char *mangled_operator_name(an_opname_kind kind)
/*
Return the string used to indicate the indicated operator name in mangled
names.
*/
{
  char *name;

  switch (kind) {
    case onk_new:               /* "new" */
      name = "__nw";
      break;
    case onk_delete:            /* "delete" */
      name = "__dl";
      break;
    case onk_plus:              /* "+" */
      name = "__pl";
      break;
    case onk_minus:             /* "-" */
      name = "__mi";
      break;
    case onk_star:              /* "*" */
      name = "__ml";
      break;
    case onk_divide:            /* "/" */
      name = "__dv";
      break;
    case onk_remainder:         /* "%" */
      name = "__md";
      break;
    case onk_excl_or:           /* "^" */
      name = "__er";
      break;
    case onk_ampersand:         /* "&" */
      name = "__ad";
      break;
    case onk_or:                /* "|" */
      name = "__or";
      break;
    case onk_compl:             /* "~" */
      name = "__co";
      break;
    case onk_not:               /* "!" */
      name = "__nt";
      break;
    case onk_assign:            /* "=" */
      name = "__as";
      break;
    case onk_lt:                /* "<" */
      name = "__lt";
      break;
    case onk_gt:                /* ">" */
      name = "__gt";
      break;
    case onk_plus_assign:       /* "+=" */
      name = "__apl";
      break;
    case onk_minus_assign:      /* "-=" */
      name = "__ami";
      break;
    case onk_times_assign:      /* "*=" */
      name = "__amu";
      break;
    case onk_divide_assign:     /* "/=" */
      name = "__adv";
      break;
    case onk_remainder_assign:  /* "%=" */
      name = "__amd";
      break;
    case onk_excl_or_assign:    /* "^=" */
      name = "__aer";
      break;
    case onk_and_assign:        /* "&=" */
      name = "__aad";
      break;
    case onk_or_assign:         /* "|=" */
      name = "__aor";
      break;
    case onk_shift_left:        /* "<<" */
      name = "__ls";
      break;
    case onk_shift_right:       /* ">>" */
      name = "__rs";
      break;
    case onk_shift_right_assign:/* ">>=" */
      name = "__ars";
      break;
    case onk_shift_left_assign: /* "<<=" */
      name = "__als";
      break;
    case onk_eq:                /* "==" */
      name = "__eq";
      break;
    case onk_ne:                /* "!=" */
      name = "__ne";
      break;
    case onk_le:                /* "<=" */
      name = "__le";
      break;
    case onk_ge:                /* ">=" */
      name = "__ge";
      break;
    case onk_and_and:           /* "&&" */
      name = "__aa";
      break;
    case onk_or_or:             /* "||" */
      name = "__oo";
      break;
    case onk_plus_plus:         /* "++" */
      name = "__pp";
      break;
    case onk_minus_minus:       /* "--" */
      name = "__mm";
      break;
    case onk_comma:             /* "," */
      name = "__cm";
      break;
    case onk_arrow_star:        /* "->*" */
      name = "__rm";
      break;
    case onk_arrow:             /* "->" */
      name = "__rf";
      break;
    case onk_function_call:     /* "()" */
      name = "__cl";
      break;
    case onk_subscript:         /* "[]" */
      name = "__vc";
      break;
#if CHECKING
    default:
      internal_error("mangled_operator_name: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return name;
}  /* mangled_operator_name */


static sizeof_t mangled_function_name(a_routine_ptr routine,
                                      a_boolean     suppress_param_encoding,
                                      char          *store_at)
/*
Determine the mangled form of the name of the function "routine".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.
*/
{
  sizeof_t     mangled_name_length, section_length;
  char         *name;
  a_type_ptr   class_type, conversion_type, routine_type, this_param_type;

  /* Most of the processing is done in mangled_encoding_for_function_type,
     but this routine handles:
       (1)  The output of the name of the function, followed by "__".
            For special member functions, a special name is used, e.g.,
            "__ct" for constructors.
       (2)  If the function is a member function, the name of the
            class pointed to, followed by
              (a) if the function is nonstatic, "C", "V", or "CV" if there
                  are type qualifiers on the "this" parameter type, or
              (b) if the function is static, "S".
     mangled_encoding_for_function_type is then called to do the rest of the
     processing.
  */
  routine_type = skip_typerefs(routine->type);
  mangled_name_length = 0;
  if (routine->special_kind == (a_special_function_kind)sfk_none) {
    /* Normal name. */
    name = routine->source_corresp.name;
#if CHECKING
    if (name == NULL) {
      internal_error("mangled_function_name: unnamed routine");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* Use a special name for the routine. */
    switch (routine->special_kind) {
      case sfk_constructor:
        name = "__ct";
        break;
      case sfk_destructor:
        name = "__dt";
        break;
      case sfk_conversion:
        name = "__op";
        /* Type signature is put out below. */
        break;
      case sfk_operator:
        name = mangled_operator_name(routine->opname_kind);
        break;
#if CHECKING
      default:
        internal_error("mangled_function_name: bad special kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* Copy the name. */
  section_length = strlen(name);
  mangled_name_length += section_length;
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(section_length));
    store_at += section_length;
  }  /* if */
  /* For a conversion function, add the type signature. */
  if (routine->special_kind == (a_special_function_kind)sfk_conversion) {
    conversion_type = routine_type->variant.routine.return_type;
    section_length = mangled_encoding_for_type(conversion_type, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  /* See if the function is a member function. */
  class_type = routine->source_corresp.class_of_which_a_member;
  /* If we will be adding the class name or the parameter types, put out
     two underscores to separate the function name from the rest. */
  if (class_type != NULL || !suppress_param_encoding) {
    /* Add two underscores after the name. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  if (class_type != NULL) {
    /* Put out the name of the class of which this function is a member. */
    section_length = mangled_encoding_for_type(class_type, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  if (!suppress_param_encoding) {
    if (class_type != NULL) {
      /* Member function. */
      this_param_type = routine_type->variant.routine.extra_info->
                                                      implicit_this_param_type;
      if (this_param_type != NULL) {
        /* The function is a nonstatic member function. */
        this_param_type = type_pointed_to(this_param_type);
        /* Add any qualifiers on the "this" parameter type (actually, the type
           pointed to by the "this" parameter). */
        if (is_const_qualified_type(this_param_type)) {
          mangled_name_length++;
          if (store_at != NULL) *store_at++ = 'C';
        }  /* if */
        if (is_volatile_qualified_type(this_param_type)) {
          mangled_name_length++;
          if (store_at != NULL) *store_at++ = 'V';
        }  /* if */
      } else {
        /* Static member function. */
        mangled_name_length += 1;
        if (store_at != NULL) *store_at++ = 'S';
      }  /* if */
    }  /* if */
    /* Now output the function type, including the parameter types. */
    section_length = mangled_encoding_for_function_type(routine_type,
                                                        store_at);
    mangled_name_length += section_length;
  }  /* if */
  return mangled_name_length;
}  /* mangled_function_name */


static void mangle_function_name(a_routine_ptr routine)
/*
Mangle the name of the indicated function, if necessary.
*/
{
  a_boolean mangling_needed, suppress_param_encoding;
  sizeof_t  mangled_name_length, alloc_length;
  char      *mangled_name;

  error_position = routine->source_corresp.decl_position;
  /* Compiler-generated routines have no name, and they are left alone. */
  if (routine->source_corresp.name != NULL) {
    mangling_needed = FALSE;
    /* All names except C external names must be mangled, because they might
       be overloaded.  All member function names must be mangled because
       they exist in a scope that does not exist in the C version of the
       program (of course, none of them have C external linkage, so no
       separate test is needed). */
    if (routine->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_external) {
      mangling_needed = TRUE;
      suppress_param_encoding = FALSE;
    } else if (routine->special_kind != (a_special_function_kind)sfk_none) {
      /* Operator function names must be somewhat mangled even if they are
         not C++ external, because their names are not normal C names --
         they contain special characters, etc. */
      mangling_needed = TRUE;
      suppress_param_encoding = TRUE;
    }  /* if */
    if (mangling_needed) {
      /* Mangle the function name. */
      /* Determine how long the mangled name is. */
      mangled_name_length = mangled_function_name(routine,
                                                  suppress_param_encoding,
                                                  (char *)NULL);
      /* Allocate space for the mangled name and build it.  The old name is
         just thrown away. */
      alloc_length = mangled_name_length + 1;
      mangled_name = alloc_il(alloc_length);
#if DEBUG
      allocated_name_string_length += alloc_length;
#endif /* DEBUG */
      (void)mangled_function_name(routine, suppress_param_encoding,
                                  mangled_name);
      /* Store the final null. */
      mangled_name[mangled_name_length] = '\0';
      routine->source_corresp.name = mangled_name;
      routine->source_corresp.name_has_been_mangled = TRUE;
    }  /* if */
  }  /* if */
}  /* mangle_function_name */


static sizeof_t mangled_static_data_member_name(a_variable_ptr variable,
                                                a_type_ptr     class_type,
                                                char           *store_at)
/*
Determine the mangled form of the name of the static data member "variable".
Place the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.  See ARM 7.2.1c for name encoding.  This routine
must only be called for static data member variables; class_type indicates
the class of which the variable is a member.
*/
{
  sizeof_t mangled_name_length, section_length;
  char     *name;

  /* The mangled name of a static data member is the original name followed
     by two underscores followed by the mangled class name.  For example:
       AB::xy --> xy__2AB
  */
  mangled_name_length = 0;
  name = variable->source_corresp.name;
#if CHECKING
  if (name == NULL) {
    internal_error(
                "mangled_static_data_member_name: unnamed static data member");
  }  /* if */
#endif /* CHECKING */
  /* Copy the name. */
  section_length = strlen(name);
  mangled_name_length += section_length;
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(section_length));
    store_at += section_length;
  }  /* if */
  /* Add two underscores after the name. */
  mangled_name_length += 2;
  if (store_at != NULL) {
    *store_at++ = '_';
    *store_at++ = '_';
  }  /* if */
  /* Output the mangled class name. */
  section_length = mangled_encoding_for_type(class_type, store_at);
  mangled_name_length += section_length;
  return mangled_name_length;
}  /* mangled_static_data_member_name */


static void mangle_static_data_member_name(a_variable_ptr variable,
                                           a_type_ptr     class_type)
/*
Mangle the name of the indicated static data member.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  error_position = variable->source_corresp.decl_position;
  /* Determine how long the mangled name is. */
  mangled_name_length = mangled_static_data_member_name(variable,
                                                        class_type,
                                                        (char *)NULL);
  /* Allocate space for the mangled name and build it.  The old name is
     just thrown away. */
  alloc_length = mangled_name_length + 1;
  mangled_name = alloc_il(alloc_length);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  (void)mangled_static_data_member_name(variable, class_type, mangled_name);
  /* Store the final null. */
  mangled_name[mangled_name_length] = '\0';
  variable->source_corresp.name = mangled_name;
  variable->source_corresp.name_has_been_mangled = TRUE;
}  /* mangle_static_data_member_name */


static void mangle_class_name(a_type_ptr class_type)
/*
Mangle the name of the indicated class, if necessary.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  error_position = class_type->source_corresp.decl_position;
  if (class_type->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL) {
    /* Template class names must be mangled because otherwise all instances
       of the same class template have the same name. */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_basic_class_name(class_type, (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_il(alloc_length);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
    (void)mangled_basic_class_name(class_type, mangled_name);
    mangled_name[mangled_name_length] = '\0';
    /* Note that the mangled name is not put into the type until after it has
       been completely built, because the old name is used in building the
       mangled form. */
    class_type->source_corresp.name = mangled_name;
    class_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_class_name */


static void mangle_nested_type_name(a_type_ptr type)
/*
Mangle the name of the indicated type, if it is nested.  This does special
processing for nested type names that must be delayed until all of the
other name mangling that might use the name is done.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  error_position = type->source_corresp.decl_position;
  if (type->source_corresp.class_of_which_a_member != NULL &&
      type->source_corresp.name != NULL
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
      /* If this a cfront 2.1 nested type, leave it in the unnested form. */
      && !type->use_cfront_transitional_nested_type_name_mangling
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
                                       ) {
    /* Nested type names must be mangled (because they exist in a scope
       that does not exist in the generated C code).  The mangled form
       is something like
         __Q2_1A1B
       The "Q2_1A1B" part is the normal representation for a mangled
       name, and the prefix makes it unique (i.e., makes it distinct
       from all user identifiers). */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_encoding_for_type(type, (char *)NULL) +
                          2;  /* "__" */
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_il(alloc_length);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
    mangled_name[0] = '_';
    mangled_name[1] = '_';
    (void)mangled_encoding_for_type(type, mangled_name + 2);
    mangled_name[mangled_name_length] = '\0';
    /* Note that the mangled name is not put into the type until after it has
       been completely built, because the old name is used in building the
       mangled form. */
    type->source_corresp.name = mangled_name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_nested_type_name */


static void do_scope_class_name_mangling(a_scope_ptr scope)
/*
Do name mangling for class names in scope and all its sub-scopes.  Note
that this does not include special processing for nested class names.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;

  /* Visit all types to find all class types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      mangle_class_name(type);
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) do_scope_class_name_mangling(class_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_class_name_mangling(block_scope);
  }  /* for */
}  /* do_scope_class_name_mangling */


static void do_scope_other_name_mangling(a_scope_ptr scope)
/*
Do name mangling for functions and static data members in scope and all its
sub-scopes.
*/
{
  a_routine_ptr  routine;
  a_variable_ptr variable;
  a_type_ptr     type;
  a_scope_ptr    class_scope, block_scope;

  /* Visit all types to find all class types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) do_scope_other_name_mangling(class_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_other_name_mangling(block_scope);
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    mangle_function_name(routine);
  }  /* for */
  /* If this is a class scope, visit the static data member variables. */
  if (scope->kind == (a_scope_kind)sck_class_struct_union) {
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      mangle_static_data_member_name(variable, scope->variant.assoc_type);
    }  /* for */
  }  /* if */
}  /* do_scope_other_name_mangling */


static void do_scope_nested_type_name_mangling(a_scope_ptr scope)
/*
Do name mangling for nested type names in scope and all its sub-scopes.
This must be done separately from and later than normal type name mangling
because the simple form of the name must remain available for use in
mangled names (e.g., virtual function table variable names).
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;

  /* Visit all types to find all named types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) {
        do_scope_nested_type_name_mangling(class_scope);
      }  /* if */
    }  /* if */
    /* Note that the call here must be done after all subscopes have been
       visited; we don't want to change the name of a class until the
       classes nested within it have been processed. */
    mangle_nested_type_name(type);
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_nested_type_name_mangling(block_scope);
  }  /* for */
}  /* do_scope_nested_type_name_mangling */


static void do_memory_region_name_mangling(a_scope_ptr scope)
/*
Do any required name mangling of members of the indicated scope and all
sub-scopes in the same memory region.
*/
{
  /* Mangle class names, not including special processing for nested
     class names. */
  do_scope_class_name_mangling(scope);
  /* Do function and static data member name mangling. */
  do_scope_other_name_mangling(scope);
  /* Mangle nested type names. */
  do_scope_nested_type_name_mangling(scope);
}  /* do_memory_region_name_mangling */


static sizeof_t mangled_derivation_name(a_derivation_step_ptr dsp,
                                        char                  *store_at)
/*
Determine the mangled form of the name of the indicated derivation.  Place
the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.
*/
{
  sizeof_t   mangled_name_length, name_length;
  a_type_ptr class_type;

  mangled_name_length = 0;
  /* The name must be put out backwards, so use recursion to get to the
     bottom of the list. */
  if (dsp->next != NULL) {
    mangled_name_length = mangled_derivation_name(dsp->next, store_at);
    if (store_at != NULL) store_at += mangled_name_length;
    /* Add two underscores to separate names. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  /* Put out the name on the first derivation step. */
  class_type = dsp->base_class->type;
  name_length = mangled_basic_class_name(class_type, store_at);
  mangled_name_length += name_length;
  if (store_at != NULL) store_at += name_length;
  return mangled_name_length;
}  /* mangled_derivation_name */


static sizeof_t mangled_vtbl_base_class_name(a_base_class_ptr bcp,
                                             char             *store_at)
/*
Determine the mangled form of the name of a base class in a virtual
function table.  The name describes the base class given by bcp.  Place
the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.
*/
{
  sizeof_t              mangled_name_length, name_length, digits;
  a_derivation_step_ptr dsp, temp_dsp;

  /* The form of the name is like
       4abcd
     or
       8abcd__ef  (this for base class "abcd" in "ef")
  */
  dsp = bcp->derivation;
  if (bcp->any_virtual_steps_in_derivation) {
    /* Drop the parts of the derivation preceding a virtual step. */
    for (temp_dsp = dsp; temp_dsp != NULL; temp_dsp = temp_dsp->next) {
      if (temp_dsp->base_class->is_virtual) dsp = temp_dsp;
    }  /* for */
  }  /* if */
  /* Determine the length. */
  name_length = mangled_derivation_name(dsp, (char *)NULL);
  digits = digits_to_represent((unsigned long)name_length);
  mangled_name_length = digits + name_length;
  if (store_at != NULL) {
    /* Put out the name length and the name. */
    (void)sprintf(store_at, "%lu", name_length);
    store_at += digits;
    (void)mangled_derivation_name(dsp, store_at);
    store_at += name_length;
  }  /* if */
  return mangled_name_length;
}  /* mangled_vtbl_base_class_name */


static sizeof_t mangled_vtbl_name(a_type_ptr       class_type,
                                  a_base_class_ptr bcp,
                                  char             *store_at)
/*
Determine the mangled form of the name of the virtual function table for
base class bcp of class class_type.  If bcp == NULL, the virtual
function table is for class_type itself.  Place the mangled name at
*store_at if store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length, section_length;

  /* Determine the mangled name.  It is
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
     The mangled-base-class-name is really a sort of pathname for the
     base class, giving the base class names from base to derived.
     For example, __vtbl__5X__X1__1B for base class X inside X1 inside
     a whole object of type B.
  */
#define VTBL_STR "__vtbl__"
  mangled_name_length = sizeof(VTBL_STR) - 1;
  if (store_at != NULL) {
    (void)memcpy(store_at, VTBL_STR, size_t_arg(mangled_name_length));
    store_at += mangled_name_length;
  }  /* if */
  if (bcp != NULL) {
    /* Add the base class name. */
    section_length = mangled_vtbl_base_class_name(bcp, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
    /* Add two underscores after the name. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  /* Add the derived class name. */
  section_length = mangled_encoding_for_type(class_type, store_at);
  mangled_name_length += section_length;
  if (store_at != NULL) store_at += section_length;
  return mangled_name_length;
#undef VTBL_STR
}  /* mangled_vtbl_name */

#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE

static void mangle_promoted_entity_name(a_source_correspondence *scp,
                                        a_routine_ptr           routine)
/*
scp points to the source correspondence field of an entity that is being
promoted out of the routine "routine" (or one of its block scopes) to
the file scope.  Give the entity a mangled name if necessary (e.g.,
if the function is a template function).
*/
{
  sizeof_t mangled_name_length, alloc_length, name_length, routine_name_length;
  char     *mangled_name, *store_at;

  if (routine->is_instantiation && routine->source_corresp.name != NULL &&
      scp->name != NULL && !scp->name_has_been_mangled) {
    /* The routine is an instantiation of a template, so name mangling is
       needed.  Without it, two instances of the same function might promote
       two instances of the same entity to file scope.  Everything about them
       looks the same, so they would clash. */
    /* The encoding is the original name, two underscores, and the
       mangled name of the routine.  Note that the routine name has not
       been mangled yet. */
    name_length = strlen(scp->name);
    check_assertion(!routine->source_corresp.name_has_been_mangled);
    routine_name_length =
                       mangled_function_name(routine,
                                             /*suppress_param_encoding=*/FALSE,
                                             (char *)NULL);
    mangled_name_length = name_length + 2 + routine_name_length;
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_il(alloc_length);
#if DEBUG
    allocated_name_string_length += alloc_length;
#endif /* DEBUG */
    (void)strcpy(mangled_name, scp->name);
    store_at = mangled_name + name_length;
    *store_at++ = '_';
    *store_at++ = '_';
    (void)mangled_function_name(routine, /*suppress_param_encoding=*/FALSE,
                                store_at);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_promoted_entity_name */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */


static void lower_source_correspondence(
                                       a_source_correspondence *source_corresp)
/*
Do IL lowering of the indicated source correspondence.
*/
{
  /* Track the source position for internal errors. */
  if (source_corresp->decl_position.seq != 0) {
    error_position = source_corresp->decl_position;
  }  /* if */
  if (source_corresp->name_linkage ==
                                 (a_name_linkage_kind)nlk_cplusplus_external) {
    source_corresp->name_linkage = (a_name_linkage_kind)nlk_external;
  }  /* if */
}  /* lower_source_correspondence */


static void set_delta_constant(a_targ_ptrdiff_t delta,
                               a_constant_ptr   delta_con)
/*
Set the constant entry delta_con to the constant integer value given by
delta.  The value is an address offset.  Issue an error if the constant
will not fit in an integer of kind TARG_DELTA_INT_KIND.
*/
{
  a_boolean did_not_fold;

  set_integer_constant(delta_con, (long)delta,
                       (an_integer_kind)TARG_PTRDIFF_T_INT_KIND);
  /* Convert ptrdiff_t to short to get any truncation error if the
     delta is too large for a short. */
  type_change_constant(delta_con,
                       integer_type(TARG_DELTA_INT_KIND),
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE, &did_not_fold,
                       &error_position);
}  /* set_delta_constant */


static void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                                 a_targ_ptrdiff_t *delta)
/*
Determine the lowered representation of the indicated pointer-to-data-member
constant, and return information about it in *delta.
*/
{
  a_field_ptr field;
  a_type_ptr  class_of_pm, field_class_type;

  /* Get the class type for which this is a pointer-to-member. */
  /* Note that by the time this routine is called the constant->type
     may have been lowered already; it cannot be used here. */
  class_of_pm = constant->variant.ptr_to_member.class_of_which_a_member;
  prelower_class_type(class_of_pm);
  field = constant->variant.ptr_to_member.variant.field;
  /* Use offset == 0 for NULL, otherwise the field offset. */
  if (field == NULL) {
    *delta = 0;
  } else {
    /* Add the offset of the field class relative to the pointer-to-member
       class and the offset of the field relative to its class.  Final
       "+1" is to reserve zero for NULL pointers. */
    field_class_type = field->source_corresp.class_of_which_a_member;
    *delta = related_class_offset(class_of_pm, field_class_type) +
             (field->bit_offset / TARG_CHAR_BIT) + 1;
  }  /* if */
}  /* repr_for_ptr_to_data_member_constant */


static void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
                                                     a_targ_ptrdiff_t *delta,
                                                     a_targ_ptrdiff_t *index,
                                                     a_routine_ptr    *func,
                                                     a_targ_ptrdiff_t *offset)
/*
Determine the lowered representation of the indicated pointer-to-member
function constant, and return information about it in *delta, *index,
*func, and *offset.  *offset is only meaningful if *func is returned
NULL.
*/
{
  a_type_ptr    class_of_pm, routine_class_type;
  a_routine_ptr routine;

  /* Get the class type for which this is a pointer-to-member. */
  /* Note that by the time this routine is called the constant->type
     may have been lowered already; it cannot be used here. */
  class_of_pm = constant->variant.ptr_to_member.class_of_which_a_member;
  prelower_class_type(class_of_pm);
  routine = constant->variant.ptr_to_member.variant.routine;
  /* The first field is the delta value, the offset of the class of the
     routine relative to the class pointed to by the pointer-to-member. */
  if (routine == NULL) {
    /* For a NULL ptr-to-member, delta is zero. */
    *delta = 0;
  } else {
    routine_class_type = routine->source_corresp.class_of_which_a_member;
    *delta = related_class_offset(class_of_pm, routine_class_type);
  }  /* if */
  /* The second field is
       0 for a NULL pointer;
       an index into the virtual function table (>0) is the function is
         virtual;
       -1 if the function is non-virtual.
  */
  if (routine == NULL) {
    /* For a NULL ptr-to-member, index is zero. */
    *index = 0;
  } else if (!routine->is_virtual) {
    /* For a non-virtual function, index is -1. */
    *index = (-1);
  } else {
    /* For a virtual function, index is the index in the virtual function
       table.  No "+1" to reserve the value zero for NULL pointers is
       needed, because the indices start with 1. */
    *index = routine->virtual_function_number;
  }  /* if */
  /* The third field is
       NULL for a null pointer;
       the offset of the virtual function table pointer in the class of
         the routine if the function is virtual;
       a pointer to the function if the function is non-virtual.
  */
  *offset = 0;
  if (routine == NULL) {
    /* For a NULL ptr-to-member, *func == NULL, *offset == 0. */
    *func = NULL;
  } else if (!routine->is_virtual) {
    /* For a non-virtual function, *func points to the routine. */
    *func = routine;
  } else {
    /* For a virtual function, the offset of the virtual function table
       pointer in the class of the routine is returned in *offset,
       *func == NULL. */
    *offset = routine_class_type->variant.class_struct_union.extra_info->
                                                  virtual_function_info_offset;
    *func = NULL;
  }  /* if */
}  /* repr_for_ptr_to_member_function_constant */


static void lower_ptr_to_member_constant(a_constant_ptr constant)
/*
Do IL lowering of a pointer-to-member constant.
*/
{
  a_routine_ptr    routine;
  a_constant_ptr   constant_next = constant->next;
  char             *constant_assoc_info = constant->source_corresp.assoc_info;
  a_boolean        did_not_fold;
  a_constant_ptr   delta_con, index_con, func_con;
  a_targ_ptrdiff_t delta, index, offset;
  a_memory_region_number
                   region_to_switch_back_to = NULL_region_number;

  /* A pointer-to-data-member becomes a short; a pointer-to-member-function
     becomes a ck_aggregate to initialize a struct.  Clearly, the places
     that reference such a ck_aggregate constant must be changed if they
     aren't static initializations. */
  if (constant->variant.ptr_to_member.is_function_ptr) {
    /* Pointer to member function. */
    repr_for_ptr_to_member_function_constant(constant, &delta, &index,
                                             &routine, &offset);
    if (processing_file_scope_init_routine) {
      /* Switch to the file scope.  Needed when lowering constants in a
         file-scope initialization routine: the current memory region would
         be the one for the generated routine, and the alloc_constant calls
         below must allocate the constants in the file scope memory region. */
      switch_to_file_scope_region(&region_to_switch_back_to);
    }  /* if */
    /* Make sure the struct type used to represent a pointer-to-member-function
       is allocated. */
    (void)make_mptr_type();
    /* Allocate the constants for the initial values. */
    /* The first field is the delta value, the offset of the class of the
       routine relative to the class pointed to by the pointer-to-member. */
    delta_con = alloc_constant((a_constant_repr_kind)ck_integer);
    set_delta_constant(delta, delta_con);
    /* The second field is
         0 for a NULL pointer;
         an index into the virtual function table (>0) is the function is
           virtual;
         -1 if the function is non-virtual.
    */
    index_con = alloc_constant((a_constant_repr_kind)ck_integer);
    set_delta_constant(index, index_con);
    /* The third field is
         NULL for a null pointer;
         the offset of the virtual function table pointer in the class of
           the routine if the function is virtual;
         a pointer to the function if the function is non-virtual.
       Note that all three are cast to a generic pointer-to-function
       type since they're initializing the first field of the union, which
       has that type. */
    func_con = alloc_constant((a_constant_repr_kind)ck_address);
    if (routine != NULL) {
      /* For a non-virtual function, a pointer to the routine. */
      func_con->variant.address.kind = (an_address_base_kind)abk_routine;
      func_con->variant.address.variant.routine = routine;
      func_con->type = make_pointer_type(routine->type);
    } else {
      /* For a virtual function, the offset of the virtual function table
         pointer in the class of the routine.  Also handles the NULL case. */
      set_delta_constant(offset, func_con);
    }  /* if */
    /* Convert the pointer or offset to the generic pointer-to-function
       type vptp.  is_implicit_cast must be FALSE to suppress a warning
       on converting a non-zero integer to a pointer. */
    type_change_constant(func_con, vptp_type,
                         /*is_implicit_cast=*/FALSE,
                         /*constant_context=*/TRUE, &did_not_fold,
                         &error_position);
    /* Change the original constant into a ck_aggregate constant. */
    set_constant_kind(constant, (a_constant_repr_kind)ck_aggregate);
    constant->variant.aggregate.first_constant = delta_con;
    delta_con->next = index_con;
    index_con->next = func_con;
    constant->variant.aggregate.last_constant = func_con;
    switch_back_to_original_region(region_to_switch_back_to);
  } else {
    /* Pointer to data member. */
    repr_for_ptr_to_data_member_constant(constant, &delta);
    set_delta_constant(delta, constant);
  }  /* if */
  constant->next = constant_next;
  /* The assoc_info field will be used to point to an associated temporary
     variable once one is created.  The variable may have already been created,
     so save the pointer if it's there. */
  constant->source_corresp.assoc_info = constant_assoc_info;
}  /* lower_ptr_to_member_constant */


static a_boolean check_for_troublesome_ptr_to_member_constant(
                                                      a_constant_ptr constant,
                                                      a_variable_ptr *temp_var)
/*
Return TRUE if constant is a pointer-to-member constant that has been
or will be changed into a ck_aggregate for a struct during lowering
(that's done for pointers to member functions).  If so, create a
temporary variable and initialize it with the ck_aggregate constant.
Return a pointer to the variable in *temp_var.  The variable is saved
and reused.  This trick is necessary for cases where such a pointer
to member constant is referenced from executable code, because a
ck_aggregate can only be referenced in an initialization.  The caller
will rewrite the reference to use the temporary variable instead of
the constant.
*/
{
  a_boolean      troublesome = FALSE;
  a_variable_ptr assoc_var = NULL;

  /* Note that the variable is allocated even if the constant is in
     a different memory region and will not at this time be turned into
     a ck_aggregate constant. */
  if (constant->kind == (a_constant_repr_kind)ck_aggregate ||
      (constant->kind == (a_constant_repr_kind)ck_ptr_to_member &&
       constant->variant.ptr_to_member.is_function_ptr)) {
    /* This is a troublesome pointer-to-member constant. */
    troublesome = TRUE;
    /* See if the variable has been allocated already.  If so, a pointer to
       the variable will have been stored in the assoc_info field. */
    assoc_var = (a_variable_ptr)constant->source_corresp.assoc_info;
    if (assoc_var == NULL) {
      /* The variable must be allocated. */
      assoc_var = make_temporary_possibly_at_file_scope(
                      make_mptr_type(),
                      !lowering_file_scope && in_file_scope((char *)constant));
      /* Save the pointer in the assoc_info field so the variable can be
         reused. */
      constant->source_corresp.assoc_info = (char *)assoc_var;
      /* Make the ck_aggregate constant the initial value of the variable. */
      assoc_var->init_kind = (an_init_kind)initk_static;
      assoc_var->initializer.constant = constant;
    }  /* if */
  }  /* if */
  *temp_var = assoc_var;
  return troublesome;
}  /* check_for_troublesome_ptr_to_member_constant */


static void lower_constant_list(a_constant_ptr constant_list)
/*
Do IL lowering of the indicated list of constants and everything under it.
*/
{
  a_constant_ptr constant;

  for (constant = constant_list; constant != NULL; constant = constant->next) {
    lower_constant(constant);
  }  /* for */
}  /* lower_constant_list */


static void lower_constant(a_constant_ptr constant)
/*
Do IL lowering of the indicated constant and everything under it.
*/
{
  a_variable_ptr temp_var;
  a_constant_ptr addressed_con;

  if (!visited_yet(constant)) {
    mark_as_visited(constant);
    lower_source_correspondence(&constant->source_corresp);
    if (constant->type != NULL) lower_os_type(constant->type);
    switch (constant->kind) {
      case ck_integer:
      case ck_string:
      case ck_float:
        /* No handling required. */
        break;
      case ck_address:
        switch (constant->variant.address.kind) {
          case abk_routine:
          case abk_variable:
            /* Variables and routines will have appeared on the list of
               variables and routines for some scope. */
            break;
          case abk_constant:
            addressed_con = constant->variant.address.variant.constant;
            lower_os_constant(addressed_con);
            if (check_for_troublesome_ptr_to_member_constant(addressed_con,
                                                             &temp_var)) {
              /* This constant node is using the address of a pointer-to-
                 member-function constant, which has or will become a
                 struct represented by a ck_aggregate constant.  Since a
                 ck_aggregate constant is not allowed here, use the address
                 of a temporary variable initialized with the ck_aggregate
                 constant. */
              constant->variant.address.kind =
                                            (an_address_base_kind)abk_variable;
              constant->variant.address.variant.variable = temp_var;
              /* Note that the type will already have been adjusted to the
                 proper pointer-to-struct type. */
            }  /* if */
            break;
#if CHECKING
          default:
            internal_error("lower_constant: bad address constant kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case ck_ptr_to_member:
        lower_ptr_to_member_constant(constant);
        break;
      case ck_aggregate:
        lower_constant_list(constant->variant.aggregate.first_constant);
        break;
#if CHECKING
      case ck_dynamic_init:
        /* Shouldn't come up here.  See
           lower_dynamic_init_aggregate_constant. */
      default:
        internal_error("lower_constant: bad kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_constant */


static void lower_os_constant(a_constant_ptr constant)
/*
A "possibly other scope" version of lower_constant; does nothing for
constants in other scopes.
*/
{
  if (crossing_into_file_scope(constant)) {
    /* Don't follow a pointer from the function scope into the file scope. */
    record_orphaned_il_entry(constant, iek_constant);
  } else {
    lower_constant(constant);
  }  /* if */
}  /* lower_os_constant */


static void make_var_for_virtual_function_table(a_type_ptr       class_type,
                                                a_base_class_ptr bcp)
/*
Create the variable to contain the virtual function table for base class bcp
when it appears in a complete object of type class_type.  If bcp is NULL,
create the variable for the virtual function table for the class_type itself.
The variable is an array of structs, each of which describes one virtual
function.  At this point, the variable is created an an extern variable.
It might be changed later to add a definition.
*/
{
  a_type_ptr     array_type;
  a_variable_ptr vtbl_var;
  char           *mangled_name;
  sizeof_t       mangled_name_length, alloc_length;

  /* Make an array of virtual table entries.  The size is [] until the virtual
     function table is defined, if it ever is in this compilation. */
  /* Note that the array type must not be shared, because it is modified
     later. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.number_of_elements = 0;  /* i.e., [] */
  array_type->variant.array.element_type = make_mptr_type();
  set_type_size(array_type);
  /* Make the variable. */
  /* Determine the length of the mangled name, which looks like
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
  */
  mangled_name_length = mangled_vtbl_name(class_type, bcp, (char *)NULL);
  /* Allocate space for the mangled name, including the final null. */
  alloc_length = mangled_name_length + 1;
  mangled_name = alloc_il(alloc_length);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  /* Build the mangled name. */
  (void)mangled_vtbl_name(class_type, bcp, mangled_name);
  mangled_name[mangled_name_length] = '\0';
  /* Note that the variable is made with extern storage class; it might
     be changed to internal linkage later, but the name linkage in the
     class at this time is not necessarily its final value, so we can't
     know the right storage class yet.  extern is the right value for
     cases where the definition is not put out, and if the definition
     is put out (and it always is for internally-linked classes) the
     storage class is adjusted at that point. */
  vtbl_var = make_variable(mangled_name, /*already_il_name=*/TRUE, array_type,
                           (a_storage_class)sc_extern);
  /* make_variable creates a variable with referenced set TRUE, but the
     variable is not necessarily going to be referenced, so clear the
     flag. */
  vtbl_var->source_corresp.referenced = FALSE;
  vtbl_var->source_corresp.name_has_been_mangled = TRUE;
  /* Remember the variable in the class type supplement or the base class
     entry so it can be found when constructor/destructor lowering is done. */
  if (bcp == NULL) {
    class_type->variant.class_struct_union.extra_info->
                                         virtual_function_table_var = vtbl_var;
  } else {
    bcp->virtual_function_table_var = vtbl_var;
  }  /* if */
}  /* make_var_for_virtual_function_table */


static a_boolean base_class_needs_virtual_function_table(
                                                   a_base_class_ptr bcp,
                                                   a_type_ptr       class_type,
                                                   a_boolean        *shared)
/*
Return TRUE if a virtual function table instance is needed for base class
bcp when it occurs as part of a complete object of class class_type.
FALSE means either the base class does not need a virtual function table
(at all) or that it can share another one.  If it can share the one
for class_type, return *shared TRUE.
*/
{
  a_boolean                   needed = FALSE;
  a_class_type_supplement_ptr class_type_ctsp, base_class_ctsp;

  *shared = FALSE;
  if (bcp->type->variant.class_struct_union.any_virtual_functions) {
    /* The base class declares virtual functions, so it needs a pointer
       to a virtual function table.  However, we may not need an
       instance of the function table specifically for bcp-in-class_type;
       some other instance may do. */
    /* See if the class type shares its virtual function pointer with
       the base class.  This test is done with offsets instead of comparing
       base class pointers because the latter gets complicated with
       multi-level sharing. */
    class_type_ctsp = class_type->variant.class_struct_union.extra_info;
    base_class_ctsp = bcp->type->variant.class_struct_union.extra_info;
    if (class_type->variant.class_struct_union.any_virtual_functions &&
        class_type_ctsp->virtual_function_info_offset ==
        base_class_ctsp->virtual_function_info_offset+bcp->offset) {
      /* The virtual function pointer for the base class we're considering
         is shared with the one for class_type.  The virtual function
         tables are therefore also shared. */
      *shared = TRUE;
      needed = FALSE;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    } else if (class_type->variant.class_struct_union.any_virtual_functions) {
      /* cfront puts outs virtual function tables whenever there is a virtual
         function in the derived class, which is more often than is really
         needed. */
      needed = TRUE;
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
    } else if (bcp->overriding_virtual_functions != NULL) {
      /* Some of the virtual functions in the base class are overridden
         in class_type, so a separate virtual function table instance is
         needed. */
      needed = TRUE;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    } else {
      /* In other cases, no separate instance is needed. */
      needed = FALSE;
    }  /* if */
  }  /* if */
  return needed;
}  /* base_class_needs_virtual_function_table */


static void make_vars_for_virtual_function_tables(a_type_ptr class_type)
/*
Generate the variables for the virtual function tables for the class type
class_type if any are needed and if they have not already been generated.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_base_class_ptr            bcp;
  a_boolean                   shared;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    if (class_type->variant.class_struct_union.any_virtual_functions) {
      /* The class has virtual functions, so it needs a virtual function table.
         Generate it if it has not already been generated. */
      if (ctsp->virtual_function_table_var == NULL) {
        /* Generate the virtual function table variable for the class
           itself. */
        make_var_for_virtual_function_table(class_type,
                                            (a_base_class_ptr)NULL);
      }  /* if */
    }  /* if */
    /* Generate the virtual function table for each base class when it
       is contained within a complete object of the primary class. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      /* Only generate virtual function tables for base classes that
         need them and only if the virtual function table has not yet
         been generated. */
      if (base_class_needs_virtual_function_table(bcp, class_type, &shared)) {
        if (bcp->virtual_function_table_var == NULL) {
          make_var_for_virtual_function_table(class_type, bcp);
        }  /* if */
      } else if (shared) {
        /* The base class shares class_type's virtual function table. */
        bcp->virtual_function_table_var = ctsp->virtual_function_table_var;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* make_vars_for_virtual_function_tables */


static a_boolean virtual_function_table_should_be_defined_here(
                                                      a_type_ptr class_type,
                                                      a_boolean  *force_static)
/*
Return TRUE if the virtual function tables for the class type class_type should
be defined (i.e., initialized) in this compilation.  Note that this should not
be called until the end of the file scope, as its value depends on whether
or not some function in the class has been defined.  If the virtual function
table should be forced to be local to this compilation, *force_static
is returned TRUE.
*/
{
  a_boolean                   defined_here;
  a_class_type_supplement_ptr ctsp;
  a_scope_ptr                 scope;
  a_routine_ptr               routine;

  *force_static = FALSE;
  /* The virtual function tables for a class are defined in the compilation
     that contains the definition of the lexically first non-inline, virtual,
     non-pure member function of the class.  See ARM 10.8.1c and "New Virtual
     Table Strategy" in the AT&T cfront 2.1 Release Notes. */
  if (!class_type->source_corresp.referenced) {
    /* The class is not referenced, so the virtual function table is not
       needed.  Note that externally-linked classes will always be marked
       as referenced. */
    defined_here = FALSE;
  } else if (class_type->source_corresp.name_linkage !=
                                 (a_name_linkage_kind)nlk_cplusplus_external) {
    /* Not C++ external linkage, therefore must define any virtual function
       table, if needed. */
    defined_here = TRUE;
  } else {
    ctsp = class_type->variant.class_struct_union.extra_info;
#if CHECKING
    if (ctsp == NULL) {
      internal_error(
                "virtual_function_table_should_be_defined_here: ctsp == NULL");
    }  /* if */
#endif /* CHECKING */
    scope = ctsp->assoc_scope;
    if (scope == NULL) {
      /* The class is declared but not defined. */
      defined_here = FALSE;
    } else {
      /* The class is defined.  Look at the member functions. */
      for (routine = scope->routines;
           routine != NULL;
           routine = routine->next) {
        if (!routine->is_inline && routine->is_virtual &&
            !routine->pure_virtual) {
          /* This is the first non-inline virtual non-pure member function in
             the class.  If it is defined in this compilation, we should put
             out the virtual function tables here. */
          defined_here = (routine->assoc_scope != NULL_region_number);
          /* If the routine is local because of the -tlocal instantiation
             mode, make the vtable local too. */
          if (routine->source_corresp.name_linkage ==
                                           (a_name_linkage_kind)nlk_internal) {
            check_assertion(instantiation_mode == tim_local);
            *force_static = TRUE;
          }  /* if */
          goto have_defined_here;
        }  /* if */
      }  /* for */
      /* There is no member function that meets the requirements, so we cannot
         decide automatically on whether or not to define the virtual function
         table.  Define the virtual function table here unless suppressed
         by user command line option. */
      defined_here = !suppress_virtual_function_table_definition;
      /* A definition put out by default when we cannot tell whether or not
         it is needed is made static, because each compilation with this
         same class will contain an instance of the definition. */
      if (defined_here) *force_static = TRUE;
    }  /* if */
  }  /* if */
have_defined_here:;
  return defined_here;
}  /* virtual_function_table_should_be_defined_here */


a_boolean virtual_dtor_should_be_generated_for_class(a_type_ptr class_type)
/*
Return TRUE if the virtual destructor for the indicated class should be
implicitly generated because of some requirement imposed by IL lowering.
Specifically, an otherwise unreferenced virtual destructor must be
generated if the virtual function table for the class must be defined in
this compilation, because a pointer to the destructor must be put in the
virtual function table.  This routine is meant to be called from the
front end proper rather than from within IL lowering.  It must be called
at the end of the translation unit, before IL lowering is done for
the file scope memory region.
*/
{
  a_boolean should_generate = FALSE, force_static;

  if (il_lowering_needed()) {
    /* Force generation of the virtual function table variable (if any) for the
       class. */
    prelower_class_type(class_type);
    /* See if the class has a virtual function table. */
    if (class_type->variant.class_struct_union.extra_info->
                                          virtual_function_table_var != NULL) {
      /* See if the virtual function table will be defined in this
         compilation. */
      if (virtual_function_table_should_be_defined_here(class_type,
                                                        &force_static)) {
        /* The virtual function table will be defined, and it will have a
           reference to the virtual destructor, so the virtual destructor
           should be generated. */
        should_generate = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return should_generate;
}  /* virtual_dtor_should_be_generated_for_class */


/*
Pointer to routine entry for the runtime routine __pure_virtual_called,
a pointer to which is placed in virtual function table slots for
pure virtual functions.
*/
static a_routine_ptr
		pure_virtual_called_routine;


static void add_vtbl_entry_init(a_targ_ptrdiff_t delta,
                                a_routine_ptr    func_to_call,
                                a_constant_ptr   aggr_con)
/*
Create a ck_aggregate constant and dependent constants to initialize
an entry of a virtual function table to (delta, 0, func_to_call), and add the
constant to the end of the aggr_con list.  func_to_call may be NULL;
in that case, a NULL pointer is put out for the function.
*/
{
  a_constant_ptr entry_aggr, delta_con, i_con, func_con;

  /* Allocate the subaggregate constant. */
  entry_aggr = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Add the constant to the end of the primary aggregate list. */
  if (aggr_con->variant.aggregate.first_constant == NULL) {
    aggr_con->variant.aggregate.first_constant = entry_aggr;
  } else {
    aggr_con->variant.aggregate.last_constant->next = entry_aggr;
  }  /* if */
  aggr_con->variant.aggregate.last_constant = entry_aggr;
  /* Allocate the constants for the initial values. */
  delta_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_delta_constant(delta, delta_con);
  /* Make the pointer to the function to call.  The type of the pointer
     is vptp_type, previously built. */
  func_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (func_to_call == NULL) {
    /* No function.  Put a NULL pointer in the table.  This is used for the
       [0] entry in the table. */
    make_zero_of_proper_type(vptp_type, func_con);
  } else {
    if (func_to_call->pure_virtual) {
      /* A pure virtual function.  Put the address of runtime routine
         __pure_virtual_called in the table. */
      func_to_call = make_runtime_routine("__pure_virtual_called",
                                          &pure_virtual_called_routine,
                                          void_type());
    }  /* if */
    /* Put the pointer to the function into the table. */
    func_con->variant.address.kind = (an_address_base_kind)abk_routine;
    func_con->variant.address.variant.routine = func_to_call;
    func_con->type = make_pointer_type(func_to_call->type);
    implicit_cast(func_con, vptp_type);
    /* Mark the routine as referenced. */
    func_to_call->source_corresp.referenced = TRUE;
  }  /* if */
  /* The "i" field is set to zero -- it's not used in virtual function
     tables, only in pointers to member functions. */
  i_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_integer_constant(i_con, (long)0, TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
  entry_aggr->variant.aggregate.first_constant = delta_con;
  delta_con->next = i_con;
  i_con->next = func_con;
  entry_aggr->variant.aggregate.last_constant = func_con;
}  /* add_vtbl_entry_init */


static a_routine_ptr find_virtual_function(
                                  a_virtual_function_number   entry_number,
                                  a_class_type_supplement_ptr ctsp,
                                  a_routine_ptr               primary_function)
/*
Find the virtual function with the number entry_number in the class whose
class type supplement is pointed to by ctsp, and return a  pointer to it.
The function must be found.  Start searching at the point in the member
functions list given by primary_function (and wrap the search around
to the beginning of the list if necessary), or start at the beginning if
primary_function is NULL.
*/
{
#if CHECKING
  int times_started_over = 0;
#endif /* CHECKING */

  for (;; primary_function = primary_function->next) {
    if (primary_function == NULL) {
#if CHECKING
      /* Make sure we don't loop if no routine with this number exists. */
      if (++times_started_over > 1) {
        internal_error("find_virtual_function: cannot find routine");
      }  /* if */
#endif /* CHECKING */
      /* Start at the beginning of the list. */
      primary_function = ctsp->assoc_scope->routines;
    }  /* if */
    /* Exit the loop when we find the routine we want. */
    if (primary_function->is_virtual &&
        primary_function->virtual_function_number == entry_number) break;
  }  /* for */
  return primary_function;
}  /* find_virtual_function */


static void fill_virtual_function_table(
                                  a_constant_ptr            aggr_con,
                                  a_type_ptr                class_type,
                                  a_base_class_ptr          bcp,
                                  a_virtual_function_number *next_entry_number)
/*
aggr_con is the aggregate constant that initializes a virtual function table.
Add to it the constants for the entries that define the virtual function
table for base class bcp when it is contained within a whole object of
type class_type.  If bcp is NULL, generate the virtual function table for
class_type itself.  On exit, return in *next_entry_number the next entry
number after the last one filled.
*/
{
  an_overriding_virtual_function_ptr override_list;
  a_type_ptr                         class_whose_vtbl_is_being_made;
  a_class_type_supplement_ptr        ctsp;
  a_routine_ptr                      primary_function;
  a_virtual_function_number          entry_number, highest_entry_number;
  a_targ_ptrdiff_t                   delta;
  a_routine_ptr                      func_to_call;
  a_base_class_ptr                   sharing_bcp, imm_bcp;

  /* Determine the override list to use. */
  if (bcp == NULL) {
    /* No overrides; we're doing the primary list. */
    override_list = NULL;
    class_whose_vtbl_is_being_made = class_type;
  } else {
    /* Get the list of overriding functions, i.e., functions in the base
       class that are overridden in the class_type. */
    override_list = bcp->overriding_virtual_functions;
    class_whose_vtbl_is_being_made = bcp->type;
  }  /* if */
  ctsp = class_whose_vtbl_is_being_made->variant.class_struct_union.extra_info;
  /* Start generating entries at entry 1. */
  entry_number = 1;
  /* See if we are generating a virtual function table in a case where the
     virtual function table is shared with a base class. */
  sharing_bcp = ctsp->virtual_function_info_base_class;
  if (sharing_bcp != NULL) {
    /* The class_whose_vtbl_is_being_made shares a virtual function pointer
       and (part of) a virtual function table with a base class. */
#if CHECKING
    if (sharing_bcp->offset != 0 ||
        sharing_bcp->any_virtual_steps_in_derivation) {
      internal_error("fill_virtual_function_table: bad vtbl sharing");
    }  /* if */
#endif /* CHECKING */
    /* Fill the part of the table that is shared with the immediate base
       class that is on the path to the base class that contains the shared
       pointer. */
    imm_bcp = sharing_bcp->derivation->base_class;
    if (bcp != NULL) {
      /* When doing this processing for a base class, we have to find the
         corresponding base class under class_type.  The base class we
         have was extracted from class_whose_vtbl_is_being_made. */
      imm_bcp = corresponding_base_class(imm_bcp,
                                         class_whose_vtbl_is_being_made,
                                         class_type);
    }  /* if */
    fill_virtual_function_table(aggr_con, class_type, imm_bcp, &entry_number);
    /* Now continue to fill the rest of the table, the unshared part, which
       contains the functions declared in class_type that do not appear
       in the base classes with which the virtual function table is
       shared. */
    /* Skip the entries on the override list that apply to the shared part
       of the table. */
    while (override_list != NULL &&
           override_list->primary_function->
                                      virtual_function_number < entry_number) {
      override_list = override_list->next;
    }  /* while */
  }  /* if */
  /* Merge the list of virtual functions under class_whose_vtbl_is_being_made
     and the overrides from the base class (if any) to create each entry of
     the table. */
  highest_entry_number = ctsp->highest_virtual_function_number;
  primary_function = NULL;
  for (; entry_number <= highest_entry_number; entry_number++) {
    /* Find the virtual function with the number "entry_number". */
    primary_function = find_virtual_function(entry_number,
                                             ctsp, primary_function);
    /* We have the routine entry.  See if there is an override entry on
       the override list.  The entries on the override list are in sorted
       order by number, so if the entry exists it will be first. */
    if (override_list != NULL &&
        override_list->primary_function == primary_function) {
      /* The primary function is overridden by another function.
         The delta (value to add to the "this" pointer) is
         the offset of the class containing the function to call minus
         the offset of the class whose vtbl we are building. */
      if (override_list->base_class == NULL) {
        /* The function is defined in the most-derived class. */
        delta = 0;
      } else {
        delta = override_list->base_class->offset;
      }  /* if */
      if (bcp != NULL) {
        /* Subtract the offset of the class whose vtbl we are building. */
        delta -= bcp->offset;
      }  /* if */
      func_to_call = override_list->overriding_function;
      override_list = override_list->next;
    } else {
      /* The primary function is not overridden.  Therefore the function
         to call is in the same class as the vtbl and the delta is 0 (the
         "this" pointer does not need adjustment). */
      delta = 0;
      func_to_call = primary_function;
    }  /* if */
    /* Create the initializing constants for this entry of the table. */
    add_vtbl_entry_init(delta, func_to_call, aggr_con);
    /* The functions are usually in order by number so set up for the
       next iteration in the common case. */
    primary_function = primary_function->next;
  }  /* for */
  *next_entry_number = entry_number;
}  /* fill_virtual_function_table */


static void define_one_for_virtual_function_table(
                                            a_type_ptr       class_type,
                                            a_base_class_ptr bcp,
                                            a_boolean        definition_needed,
                                            a_boolean        force_static)
                                              
/*
Finish the job begun by make_var_for_virtual_function_table: finish making
a virtual function table variable.  This routine handles things that could
not be handled yet on the other call, like setting the size and storage
class of the variable and generating the initial value for the variable
(i.e., the virtual function table itself).  The virtual function table
variable to be finished is the one for the base class indicated by bcp when
it appears within a complete object of the class type class_type, or the
one for class_type itself if bcp == NULL.  If definition_needed is FALSE,
the virtual function table should not be defined in this compilation.
If force_static is TRUE, the virtual function table is forced to be local
to the current compilation even if the class is externally linked.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_variable_ptr              vtbl_var;
  a_constant_ptr              aggr_con;
  a_virtual_function_number   next_entry_number;
  a_memory_region_number      region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Find the appropriate virtual function table variable. */
  if (bcp == NULL) {
    /* We're doing the virtual function table for class_type itself. */
    ctsp = class_type->variant.class_struct_union.extra_info;
    vtbl_var = ctsp->virtual_function_table_var;
  } else {
    /* We're doing the virtual function table for bcp in class_type. */
    ctsp = bcp->type->variant.class_struct_union.extra_info;
    vtbl_var = bcp->virtual_function_table_var;
  }  /* if */
  /* Change the array size from [] to the proper size.  Note that the type
     was created for this variable and is known not to be shared. */
  /* The "+1" is to skip the [0] entry, which makes the code to access
     the table a little cleaner.  It's also necessary for cfront
     compatibility. */
  vtbl_var->type->variant.array.number_of_elements =
                                     ctsp->highest_virtual_function_number + 1
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  /* Add an extra zeroed entry at the end of the table for full cfront
     compatibility (although we don't know why the entry is there). */
                                     + 1
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                                              ;
  set_type_size(vtbl_var->type);
  /* Set the linkage on the virtual function table variable. */
  if (class_type->source_corresp.name_linkage !=
                                 (a_name_linkage_kind)nlk_cplusplus_external ||
      force_static) {
    /* For an internally-linked class or one with no linkage, or when
       forced to by the flag force_static, change the storage class to
       static and the linkage to internal. */
    vtbl_var->storage_class = (a_storage_class)sc_static;
    vtbl_var->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  } else if (definition_needed) {
    /* For an externally-linked class whose definition is needed, change the
       variable to an external definition. */
    vtbl_var->storage_class = (a_storage_class)sc_unspecified;
    /* The variable can be referenced from another compilation unit. */
    vtbl_var->source_corresp.referenced = TRUE;
  }  /* if */
  /* Do not put out the initial value if the class should not be defined
     in this compilation. */
  if (definition_needed) {
    /* Start the initialization by creating a ck_aggregate constant and
       making it the initial value of the variable. */
    aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    vtbl_var->init_kind = (an_init_kind)initk_static;
    vtbl_var->initializer.constant = aggr_con;
    /* Put out the initialization for the [0] entry (skipped). */
    add_vtbl_entry_init((a_targ_ptrdiff_t)0, (a_routine_ptr)NULL, aggr_con);
    /* Put out the body of the table. */
    fill_virtual_function_table(aggr_con, class_type, bcp, &next_entry_number);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* Put out the initialization for an extra zeroed entry at the end, for
       cfront compatibility. */
    add_vtbl_entry_init((a_targ_ptrdiff_t)0, (a_routine_ptr)NULL, aggr_con);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* define_one_for_virtual_function_table */


static void define_virtual_function_tables(a_type_ptr class_type)
/*
Generate the definitions of the virtual function tables for the class type
class_type if any are needed.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_base_class_ptr            bcp;
  a_boolean                   need_determined = FALSE;
  a_boolean                   definition_needed, force_static;

  /* Make sure the class type has been pre-lowered. */
  prelower_class_type(class_type);
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    if (ctsp->virtual_function_table_var != NULL) {
      /* The class has a virtual function table.  Generate the definition
         if it is supposed to be generated in the present compilation. */
      definition_needed = 
                  virtual_function_table_should_be_defined_here(class_type,
                                                                &force_static);
      need_determined = TRUE;
      /* Generate the virtual function table for the class itself. */
      define_one_for_virtual_function_table(class_type, (a_base_class_ptr)NULL,
                                            definition_needed, force_static);
    }  /* if */
    /* Generate the virtual function table for each base class when it
       is contained within a complete object of the primary class. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->virtual_function_table_var != NULL) {
        if (!need_determined) {
          definition_needed = 
                  virtual_function_table_should_be_defined_here(class_type,
                                                                &force_static);
          need_determined = TRUE;
        }  /* if */
        /* If the base class and class_type share a virtual function table,
           it was already defined above; do not define it again. */
        if (bcp->virtual_function_table_var !=
                                            ctsp->virtual_function_table_var) {
          define_one_for_virtual_function_table(class_type, bcp,
                                                definition_needed,
                                                force_static);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* define_virtual_function_tables */


static void define_scope_virtual_function_tables(a_scope_ptr scope)
/*
Make all needed definitions of virtual function tables for all classes
in the indicated scope.

Note that the virtual function table variables were created during
prelowering; this routine adds initial values to those variables if
appropriate.  This routine is called at the beginning of lowering of
the file scope, because it must be called (1) after all function
definitions have been seen (because deciding on defining virtual function
tables depends on knowing whether member functions of the class are
defined in the present compilation) and (2) before classes have been
lowered (because that destroys the list of member functions, including
virtual functions, needed in this process).
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;

  /* Visit all types to find all class types. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      define_virtual_function_tables(type);
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) {
        define_scope_virtual_function_tables(class_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    define_scope_virtual_function_tables(block_scope);
  }  /* for */
}  /* define_scope_virtual_function_tables */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static a_boolean class_has_independently_allocated_virtual_base_classes(
                                                         a_type_ptr class_type)
/*
Return TRUE if the class class_type has any virtual base classes that
are allocated as part of a complete object of type class_type, as opposed
to within base classes of class_type.  Such a class requires a
type-as-subobject that is different from its normal type.
*/
{
  a_boolean        has_indep_virt_base_classes = FALSE;
  a_base_class_ptr bcp;

  for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual) {
      /* A virtual base class. */
      if (bcp->data_section_base_class == NULL) {
        /* A virtual base class that is not allocated within one of the
           base classes. */
        has_indep_virt_base_classes = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return has_indep_virt_base_classes;
}  /* class_has_independently_allocated_virtual_base_classes */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */


static void make_subobject_class_type(a_type_ptr class_type)
/*
Make a version of the indicated class type that is suitable for use when
the class is used as a subobject.  Record the subobject type in the
type_as_subobject field of the class type supplement.  If the original
class has no virtual base classes, the subobject type will be the same
type.  The original class type must have been lowered just to the point where
the fields for the virtual base class space would be added; that allows
this routine to do a relatively simple copy of the all the fields.
*/
{
  a_field_ptr                 old_field, last_field;
  a_type_ptr                  subobject_type;
  a_class_type_supplement_ptr ctsp, subobject_ctsp;
  sizeof_t                    name_length, alloc_length;
  char                        *name_ptr, *new_name_ptr;
  a_scope_depth               scope_depth;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (!class_type->variant.class_struct_union.any_virtual_base_classes
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      || !class_has_independently_allocated_virtual_base_classes(class_type)
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                                      ) {
    /* There are no virtual base classes, so the type to use as a subobject
       is the same as the class type itself. */
    subobject_type = class_type;
  } else {
    /* Make a copy of the class type for use as the subobject type. */
    subobject_type = alloc_type((a_type_kind)tk_struct);
    subobject_ctsp = subobject_type->variant.class_struct_union.extra_info;
    /* Give the struct a name that is a prefix followed by the original name.
       Also give it the same declaration position as the original type. */
    name_ptr = class_type->source_corresp.name;
    if (name_ptr != NULL) {
      name_length = strlen(name_ptr);
#define SUB_PREFIX "_"
      alloc_length = name_length + sizeof(SUB_PREFIX);
      new_name_ptr = alloc_il(alloc_length);
#if DEBUG
      allocated_name_string_length += alloc_length;
#endif /* DEBUG */
      (void)memcpy(new_name_ptr, SUB_PREFIX, size_t_arg(sizeof(SUB_PREFIX)-1));
      (void)strcpy(new_name_ptr + (sizeof(SUB_PREFIX)-1), name_ptr);
      subobject_type->source_corresp.name = new_name_ptr;
#undef SUB_PREFIX
    }  /* if */
    subobject_type->source_corresp.decl_position = 
                                      class_type->source_corresp.decl_position;
#if 0
    /* Ideally, the referenced flag would not be set if the class type is
       not referenced.  However, the class type might not be referenced now
       (part-way through the compilation) and then be referenced later. */
#endif
    subobject_type->source_corresp.referenced = TRUE;
    /* Put the struct type on the file-scope types list right after the
       associated type.  This is done instead of calling add_to_types_list
       because we want to get the type at the right place on the list.
       If the class type is the last entry on the some types list,
       use add_to_types_list to get the last_type pointer updated. */
    if (class_type->next == NULL) {
      /* The class type is the last on a list.  See if the list is one of the
         ones being tracked in the scope stack. */
#if 0
      /* Improve this? */
#endif
      for (scope_depth = depth_scope_stack; scope_depth >= 0; scope_depth--) {
        if (class_type == scope_stack[scope_depth].last_type) {
          /* Found the list.  Add the subobject type to its end. */
          add_to_types_list(subobject_type, scope_depth);
          goto added_to_list;
        }  /* if */
      }  /* for */
    }  /* if */
    /* The class type is not the last on a list we're tracking. */
    subobject_type->next = class_type->next;
    class_type->next = subobject_type;
added_to_list:;
    /* Copy the fields of the class to the subobject type.  This includes
       fields added for things like the virtual function table pointer. */
    last_field = NULL;
    for (old_field = class_type->variant.class_struct_union.field_list;
         old_field != NULL;
         old_field = old_field->next) {
      /* Copy a field. */
      copy_field(old_field, subobject_type, &last_field);
    }  /* for */
    subobject_type->size = ctsp->size_without_virtual_base_classes;
    subobject_type->alignment = ctsp->alignment_without_virtual_base_classes;
    /* add_to_types_list is not called on purpose.  See above. */
    subobject_ctsp->virtual_function_info_offset =
                                            ctsp->virtual_function_info_offset;
    /* Preserve the information on sharing of virtual function table pointers.
       This is not terribly clean, in that the base class pointed to is not
       a base class of the type-as-subobject.  However, by the end of IL
       lowering this field is no longer meaningful, so the value is strange
       only during IL lowering. */
    subobject_ctsp->virtual_function_info_base_class =
                                        ctsp->virtual_function_info_base_class;
#if 0
    /* Following would perhaps be dangerous; class would not get virtual
       function table variables set (Could it share the main class vars?
       Does it need any?). */
    /* The subobject type has no virtual base classes and is therefore its
       own type as subobject. */
    subobject_ctsp->type_as_subobject = subobject_type;
#endif
  }  /* if */
  ctsp->type_as_subobject = subobject_type;
}  /* make_subobject_class_type */


static void prelower_class_type(a_type_ptr class_type)
/*
Do processing that is required early in lowering for the indicated class
type.  Such processing builds information that is necessary during the
lowering process, but does not modify the class type.  Note that this
routine assumes the class type is as complete as it will ever get; if
the prelowering should not be done if the type is currently incomplete, call
prelower_class_type_if_complete instead.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_base_class_ptr            bcp;
  a_class_type_supplement_ptr base_ctsp;
  a_type_ptr                  base_class_type;
  a_source_position           saved_error_position;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    /* See if prelowering has already been done. */
    if (ctsp->type_as_subobject == NULL) {
      saved_error_position = error_position;
      error_position = class_type->source_corresp.decl_position;
      /* Make the virtual function table variables if they have not been made
         already. */
      make_vars_for_virtual_function_tables(class_type);
      /* Make dummy fields to reserve space for the direct base classes,
         and add them to the field list. */
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        /* Pre-lower base classes. */
        prelower_class_type(bcp->type);
        base_ctsp = bcp->type->variant.class_struct_union.extra_info;
        base_class_type = base_ctsp->type_as_subobject;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
        /* Decide whether to use the base class's type or its
           type-as-subobject for this instance as a base class.  The
           full type includes space for any virtual base classes; the
           type-as-subobject does not.  This only comes up in cfront
           compatibility mode (because cfront does not have a
           type-as-subobject mechanism and can only use the complete
           object type for base classes other than the first). */
        if (bcp->complete_subobject) base_class_type = bcp->type;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
        if (!bcp->is_virtual) {
          /* Non-virtual base class. */
          if (bcp->direct) {
            /* For a direct non-virtual base class, put out space for an object
               of the base class. */
            add_base_class_dummy_field(bcp->type, "__b_",
                                       base_class_type, bcp->offset,
                                       class_type);
          }  /* if */
        } else {
          /* Virtual base class.  See if a pointer to the base class is
             required. */
          /* Do not put out the pointer if it is shared with a base class. */
          if (bcp->pointer_base_class == NULL
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
              /* ... or if the base class is indirect. */
              && bcp->direct
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
                                             ) {
            add_base_class_dummy_field(bcp->type, "__p_",
                                       make_pointer_type(base_class_type),
                                       bcp->pointer_offset, class_type);
          }  /* if */
        }  /* if */
      }  /* for */
      if (class_type->variant.class_struct_union.any_virtual_functions &&
          ctsp->virtual_function_info_base_class == NULL) {
        /* The class has virtual functions, so it needs a virtual function
           table pointer.  Also, the pointer is not shared with a base
           class.  Make a dummy field for a virtual function table pointer. */
        add_dummy_field("__vptr", make_pointer_type(make_mptr_type()),
                        ctsp->virtual_function_info_offset, class_type);
      }  /* if */
      /* Make a type for the class for use when the class is a subobject.
         This will be the same as the class type if the class has no virtual
         base classes. */
      make_subobject_class_type(class_type);
      /* If there are virtual base classes, make dummy fields to reserve space
         for them and add those to the field list. */
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
          /* Ignore non-virtual base classes. */
          if (bcp->is_virtual) {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
            /* If the space for the virtual base class was allocated inside
               some other base class, it need not be allocated here. */
            if (bcp->data_section_base_class == NULL) {
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
              base_ctsp = bcp->type->variant.class_struct_union.extra_info;
              base_class_type = base_ctsp->type_as_subobject;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
              /* Decide whether to use the base class's type or its
                 type-as-subobject for this instance as a base class.
                 See comment above. */
              if (bcp->complete_subobject) base_class_type = bcp->type;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
              add_base_class_dummy_field(bcp->type, "__v_",
                                         base_class_type, bcp->offset,
                                         class_type);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
            }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
          }  /* if */
        }  /* for */
      }  /* if */
      /* Restore error_position as of entry to this routine. */
      error_position = saved_error_position;
    }  /* if */
  }  /* if */
}  /* prelower_class_type */


static void prelower_class_type_if_complete(a_type_ptr class_type)
/*
Do pre-lowering of a class type, but only if the class type is complete.
*/
{
  a_class_type_supplement_ptr ctsp;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL && ctsp->assoc_scope != NULL) {
    prelower_class_type(class_type);
  }  /* if */
}  /* prelower_class_type_if_complete */


static void lower_template_arg(a_template_arg_ptr template_arg)
/*
Do IL lowering of the indicated template argument and everything under it.
*/
{
  if (template_arg->is_type) {
    lower_type(template_arg->variant.type);
  } else {
    lower_constant(template_arg->variant.constant);
  }  /* if */
}  /* lower_template_arg */


static void lower_template_arg_list(a_template_arg_ptr template_arg_list)
/*
Do IL lowering of the indicated list of template arguments and everything
under it.
*/
{
  a_template_arg_ptr template_arg;

  for (template_arg = template_arg_list;
       template_arg != NULL;
       template_arg = template_arg->next) {
    lower_template_arg(template_arg);
  }  /* for */
}  /* lower_template_arg_list */



static void promote_constants(a_scope_ptr scope)
/*
Promote the constants on the scope list to the file scope.
*/
{
  a_constant_ptr constant, next_constant;

  for (constant = scope->constants;
       constant != NULL;
       constant = next_constant) {
    next_constant = constant->next;
    add_to_constants_list(constant);
  }  /* for */
  scope->constants = NULL;
}  /* promote_constants */


#if !PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
/*ARGSUSED*/ /* <-- routine is only used when local entities are promoted. */
#endif /* !PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
static void promote_types(a_scope_ptr   scope,
                          a_routine_ptr routine)
/*
Promote the types on the scope list to the file scope.  Types promoted
will be inserted at the point indicated by type_promotion_insert_location.
If routine is non-NULL, the scope is (directly or indirectly) part of the
indicated function (as opposed to a class).
*/
{
  a_type_ptr  type, next_type;

  for (type = scope->types; type != NULL; type = next_type) {
    next_type = type->next;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
    if (routine != NULL) {
      /* Mangle the name if necessary (e.g., if it is part of a template
         function). */
      mangle_promoted_entity_name(&type->source_corresp, routine);
    }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    if (type_promotion_insert_location != NULL) {
      /* Insert at the indicated point. */
      type->next = *type_promotion_insert_location;
      *type_promotion_insert_location = type;
      type_promotion_insert_location = &type->next;
    } else {
      /* Add to the end of the list. */
      add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
    }  /* if */
  }  /* for */
  scope->types = NULL;
}  /* promote_types */


#if !PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
/*ARGSUSED*/ /* <-- routine is only used when local entities are promoted. */
#endif /* !PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
static void promote_variables(a_scope_ptr   scope,
                              a_routine_ptr routine)
/*
Promote the variables on the scope list to the file scope.
If routine is non-NULL, the scope is (directly or indirectly) part of the
indicated function (as opposed to a class).
*/
{
  a_variable_ptr variable, next_variable;

  for (variable = scope->variables;
       variable != NULL;
       variable = next_variable) {
    next_variable = variable->next;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
    if (routine != NULL) {
      /* Mangle the name if necessary (e.g., if it is part of a template
         function). */
      mangle_promoted_entity_name(&variable->source_corresp, routine);
    }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    add_to_variables_list(variable, /*at_file_scope=*/TRUE);
  }  /* for */
  scope->variables = NULL;
}  /* promote_variables */


static void promote_routines(a_scope_ptr scope)
/*
Promote the routines on the scope list to the file scope.
*/
{
  a_routine_ptr routine, next_routine;

  for (routine = scope->routines; routine != NULL; routine = next_routine) {
    next_routine = routine->next;
    add_to_routines_list(routine, /*at_file_scope=*/TRUE);
  }  /* for */
  scope->routines = NULL;
}  /* promote_routines */


static void promote_class_members(a_type_ptr class_type)
/*
Promote member functions, static data members, and local types of the
indicated type into file scope.  Note that this promotion must
be done after all virtual function tables have been generated for the class
(including those needed in classes derived from this class), because
the promotion process makes the virtual functions unfindable.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_scope_ptr                 scope;

  class_type = skip_typerefs(class_type);  /* Probably not needed. */
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    scope = ctsp->assoc_scope;
    /* If the class has static data members, member functions, or local
       types, promote them into the file scope. */
    if (scope != NULL) {
      promote_constants(scope);
      promote_types(scope, (a_routine_ptr)NULL);
      promote_variables(scope, (a_routine_ptr)NULL);
      promote_routines(scope);
    }  /* if */
  }  /* if */
}  /* promote_class_members */


static void lower_class_struct_union_type(a_type_ptr class_type)
/*
Do IL lowering on the indicated class/struct/union type.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_base_class_ptr            bcp;
  a_source_position           saved_error_position;

  saved_error_position = error_position;
  prelower_class_type(class_type);
  /* Lower the nonstatic data members. */
  lower_field_list(class_type->variant.class_struct_union.field_list);
  error_position = class_type->source_corresp.decl_position;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    if (ctsp->assoc_scope != NULL) {
      /* Lower the base classes. */
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        lower_type(bcp->type);
      }  /* for */
      /* Lower the member functions, local types, etc. */
      lower_scope(ctsp->assoc_scope);
      /* Promote members into the file scope. */
      promote_class_members(class_type);
    }  /* if */
    /* Lower the template arg list, if any. */
    lower_template_arg_list(ctsp->template_arg_list);
    /* Lower the type-as-subobject. */
    lower_type(ctsp->type_as_subobject);
  }  /* if */
  if (class_type->kind == (a_type_kind)tk_class) {
    class_type->kind = (a_type_kind)tk_struct;
  }  /* if */
  error_position = saved_error_position;
}  /* lower_class_struct_union_type */


static void lower_type_list(a_type_ptr type_list)
/*
Do IL lowering of the indicated list of types and everything under it.
*/
{
  a_type_ptr type;
  a_boolean  is_file_scope_list;

  /* See if the list we are handling is the one for the file scope. */
  is_file_scope_list = (type_list == il_header.primary_scope->types);
  if (is_file_scope_list) {
    /* When lowering the file-scope list, get promoted types inserted into
       the middle of the types list. */
    type_promotion_insert_location = &il_header.primary_scope->types;
  }  /* if */
  for (type = type_list; type != NULL; type = type->next) {
    lower_type(type);
    if (is_immediate_class_type(type)) {
      /* There is special processing for class types.  The reason this is
         not done directly by lower_type is that promotion of types
         inside of classes into the file scope must be done in the right
         order.  We hit types in arbitrary order in lower_type (because it's
         called from lots of places).  Here, we know we're dealing with a
         type list and can process the classes in the order they appear
         on the type list.  Since all classes appear on a type list somewhere,
         we will process them all eventually and all in order within a
         given type list. */
      lower_class_struct_union_type(type);
    }  /* if */
    if (is_file_scope_list) type_promotion_insert_location = &type->next;
  }  /* for */
  if (is_file_scope_list) type_promotion_insert_location = NULL;
}  /* lower_type_list */


static void lower_type(a_type_ptr type)
/*
Do IL lowering of the indicated type and everything under it.
*/
{
  a_type_ptr	ptr_return_type, new_type, type_next, member_type;
  a_type_ptr	copy_of_pm_type;
  a_based_type_list_member_ptr
		btlmp;

  if (!visited_yet(type)) {
    mark_as_visited(type);
    lower_source_correspondence(&type->source_corresp);
    /* Lower the based types list (it points to types based on the present
       type, e.g., pointer-to the present type). */
    for (btlmp = type->based_types; btlmp != NULL; btlmp = btlmp->next) {
      lower_type(btlmp->based_type);
    }  /* for */
    switch (type->kind) {
      case tk_void:
      case tk_float:
        /* No processing required. */
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          lower_constant_list(type->variant.integer.enum_info.constant_list);
        } else if (type->variant.integer.enum_info.affiliated_type != NULL) {
          lower_type(type->variant.integer.enum_info.affiliated_type);
        }  /* if */
        break;
      case tk_pointer:
        lower_type(type->variant.pointer.type);
        break;
      case tk_ptr_to_member:
        lower_type(type->variant.ptr_to_member.class_of_which_a_member);
        member_type = type->variant.ptr_to_member.type;
        lower_type(member_type);
        if (is_function_type(member_type)) {
          /* Pointer to member function; gets replaced by a struct. */
          new_type = make_mptr_type();
        } else {
          /* Pointer to data member; gets replaced by a short. */
          new_type = integer_type(TARG_DELTA_INT_KIND);
#if CHECKING
          if (new_type->size != TARG_SIZEOF_PTR_TO_DATA_MEMBER ||
              new_type->alignment != TARG_ALIGNOF_PTR_TO_DATA_MEMBER) {
            internal_error(
         "lower_type: target.h config of pointer-to-data-member is incorrect");
          }  /* if */
#endif /* CHECKING */
        }  /* if */
        /* Make a copy of the original pointer-to-member type.  Note that
           this copy is for the use of IL lowering; it it not really part
           of the IL tree. */
        copy_of_pm_type = alloc_type(type->kind);
        copy_type(type, copy_of_pm_type);
        /* Change the type to a pure typeref to the new (lowered) type. */
        type_next = type->next;
        set_type_kind(type, (a_type_kind)tk_typeref);
        type->next = type_next;
        type->variant.typeref.type = new_type;
        /* Point to a copy of the original pointer-to-member type.  This
           preserves information otherwise destroyed: if, while lowering,
           one comes across a pointer-to-member type that has already been
           lowered, one needs to be able to get the original class and
           member type. */
        type->variant.typeref.orig_type = copy_of_pm_type;
        break;
      case tk_routine:
        lower_type(type->variant.routine.return_type);
        { a_routine_type_supplement_ptr rtsp =type->variant.routine.extra_info;
          a_param_type_ptr ptp;
#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED
          /* Make all function types unprototyped.  Note that the
             param_type_list is not cleared even if the function has no
             body.  This can create a function type with prototyped == FALSE,
             assoc_routine == NULL, and param_type_list != NULL, which is
             not otherwise possible. */
          rtsp->prototyped = FALSE;
#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */
          if (rtsp->value_returned_by_cctor) {
            /* Add an extra parameter in which the return address will be
               passed. */
            ptr_return_type = 
                          make_pointer_type(type->variant.routine.return_type);
            ptp = alloc_param_type(ptr_return_type);
            /* Force lowering in the loop that follows. */
            mark_as_not_visited(ptp);
            ptp->next = rtsp->param_type_list;
            rtsp->param_type_list = ptp;
            /* The return value type becomes "void". */
            type->variant.routine.return_type = void_type();
          }  /* if */
          /* If there is an implicit "this" parameter, make an explicit
             first parameter for it. */
          if (rtsp->implicit_this_param_type != NULL) {
            ptp = alloc_param_type(rtsp->implicit_this_param_type);
            /* Force lowering in the loop that follows. */
            mark_as_not_visited(ptp);
            ptp->next = rtsp->param_type_list;
            rtsp->param_type_list = ptp;
            /* Leave the implicit_this_param_type unchanged; it's helpful
               to have it there to determine the "this" parameter type
               whether or not the routine type has been lowered. */
          }  /* if */
          for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
            /* Only process each param type entry if it has not been
               previously visited. */
            if (!visited_yet(ptp)) {
              mark_as_visited(ptp);
              lower_type(ptp->type);
              /* If the parameter must be passed using a copy constructor,
                 change its type to pointer-to-class. */
              if (ptp->passed_via_copy_constructor) {
                ptp->type = make_pointer_type(ptp->type);
              } /* if */
              /* Clear the default_arg_expr field to make the IL more
                 like C IL.  Note this throws away the expression. */
              ptp->default_arg_expr = NULL;
            }  /* if */
          }  /* for */
          if (rtsp->prototype_scope != NULL) {
            lower_scope(rtsp->prototype_scope);
          }  /* if */
        }
        break;
      case tk_array:
        lower_type(type->variant.array.element_type);
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Classes are lowered by lower_class_struct_union_type in a separate
           pass through the data structure.  Here, just do pre-lowering. */
        prelower_class_type_if_complete(type);
        break;
      case tk_typeref:
        lower_type(type->variant.typeref.type);
        break;
      case tk_template_param:
        /* These shouldn't really get out of the front end, but they do
           sometimes get onto a based types list, so turn them into something
           mostly harmless. */
        set_type_kind(type, (a_type_kind)tk_error);
        break;
#if CHECKING
      case tk_unknown:  /* Shouldn't make it out of front end. */
      default:
        internal_error("lower_type: bad kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_type */


static void lower_os_type(a_type_ptr type)
/*
A "possibly other scope" version of lower_type; does nothing for
types in other scopes.
*/
{
  if (crossing_into_file_scope(type)) {
    /* Don't follow a pointer from the function scope into the file scope. */
    record_orphaned_il_entry(type, iek_type);
  } else {
    lower_type(type);
  }  /* if */
}  /* lower_os_type */


static an_expr_node_ptr modify_init_entity_node(
                                          an_expr_node_ptr         entity_node,
                                          an_init_pos_modifier_ptr modifiers)
/*
Add the address modifiers from the list given by "modifiers" to the
entity address "entity_node" and return a pointer to the modified expression
tree.
*/
{
  an_expr_node_ptr elem_num_node;

  /* If there are no modifiers, return the original node. */
  if (modifiers != NULL) {
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    entity_node = modify_init_entity_node(entity_node, modifiers->next);
    /* Add the final modifier. */
    if (modifiers->curr_field != NULL) {
      /* Add a field selection.  ("au_" for possibly from anonymous union.) */
      entity_node = au_field_lvalue_selection_expr(entity_node,
                                                   modifiers->curr_field);
    } else if (modifiers->curr_base != NULL) {
      /* Add a base class selection. */
      entity_node = make_base_class_lvalue(entity_node, modifiers->curr_base);
    } else {
      /* Add an array element selection. */
      /* Do the pointer decay from array to pointer to element. */
      a_type_ptr ptr_elem_type = make_pointer_type(
                                   array_element_type(
                                     type_pointed_to(entity_node->type)));
      entity_node = add_cast(entity_node, ptr_elem_type);
      if (modifiers->curr_elem != 0) {
        /* Add the subscript if it's non-zero. */
        elem_num_node = node_for_integer_constant(
                                        (long)modifiers->curr_elem,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
        entity_node->next = elem_num_node;
        entity_node = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         make_pointer_type(modifiers->type),
                                         entity_node);
      }  /* if */
    }  /* if */
  }  /* if */
  return entity_node;
}  /* modify_init_entity_node */


static an_expr_node_ptr make_init_entity_node(an_init_pos_descr_ptr ipdp)
/*
Make an expression for the entity described by ipdp, as an lvalue, and
return a pointer to it.
*/
{
  an_expr_node_ptr entity_node;

  /* Make a node for the base address. */
  if (ipdp->indirect_through_variable) {
    /* Indirect through the variable. */
    entity_node = var_rvalue_expr(ipdp->variable);
  } else {
    /* Normal case, a simple variable. */
    entity_node = var_lvalue_expr(ipdp->variable);
  }  /* if */
  /* Add the modifiers to the base address. */
  entity_node = modify_init_entity_node(entity_node, ipdp->modifiers);
  return entity_node;
}  /* make_init_entity_node */


static void add_init_assignment(a_dynamic_init_ptr     dip,
                                an_expr_node_ptr       entity_node,
                                an_insert_location_ptr insert_location)
/*
Make an assignment statement to implement the dynamic initialization
described by dip.  entity_node is an expression that gives the address
of the entity to be initialized.  Insert the statement at *insert_location
and update *insert_location.  The constant or expression initial value
pointed to by dip is lowered.
*/
{
  an_expr_node_ptr      init_val_node;
  a_statement_ptr       assign_stmt;
  an_expr_operator_kind op;

  switch (dip->kind) {
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      /* The constant has already been lowered. */
      init_val_node = make_node_for_il_constant(dip->variant.constant);
      break;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      /* The expression has already been lowered. */
      init_val_node = dip->variant.expression;
      break;
#if CHECKING
    default:
      internal_error("add_init_assignment: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* Make an assignment statement.  Note that we know that no constructor
     (copy or other) is involved because we have this kind of dynamic
     initialization.  We also know the thing being initialized is not an
     array. */
  op = lowered_assignment_operator(init_val_node->type);
  assign_stmt = insert_assignment_statement(entity_node, op, init_val_node,
                                            insert_location);
  /* If the initialization is for a whole variable, the position is available
     from the variable. */
  transfer_seq_from_var_to_statement(dip->variable, assign_stmt);
}  /* add_init_assignment */


static void make_ctor_implied_arg_list(a_routine_ptr    ctor_routine,
                                       an_expr_node_ptr *implied_arg_list,
                                       an_expr_node_ptr *end_implied_arg_list)
/*
Build and return a list of the implied arguments to be added to a call of
the constructor ctor_routine.  There is one implied argument for each
virtual base class of the associated base class, and they are used to
ensure that each virtual base class is constructed only once.  The
beginning and end of the list are returned in *implied_arg_list and
*end_implied_arg_list.  For an empty list, both will be set to NULL.
*/
{
  an_expr_node_ptr implied_arg_node;
  a_type_ptr       class_type, subobject_type;
  a_base_class_ptr bcp;
  a_constant       null_constant;

  *implied_arg_list = *end_implied_arg_list = NULL;
  /* Get the class type. */
  class_type = ctor_routine->source_corresp.class_of_which_a_member;
  prelower_class_type(class_type);
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* The class has at least one virtual base class. */
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Allocate an expression that is a NULL pointer to the virtual
           base class.  Use the type of the base class when used as a
           subobject. */
        subobject_type = bcp->type->variant.class_struct_union.extra_info->
                                                             type_as_subobject;
        make_zero_of_proper_type(make_pointer_type(subobject_type),
                                 &null_constant);
        implied_arg_node = alloc_node_for_constant(&null_constant);
        /* Add the node to the list. */
        if (*implied_arg_list == NULL) {
          *implied_arg_list = implied_arg_node;
        } else {
          (*end_implied_arg_list)->next = implied_arg_node;
        }  /* if */
        *end_implied_arg_list = implied_arg_node;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* make_ctor_implied_arg_list */


static void make_dtor_implied_arg_list(a_routine_ptr    dtor_routine,
                                       a_boolean        have_complete_object,
                                       an_expr_node_ptr *implied_arg_node)
/*
Build and return the implied argument to be added to a call of the destructor
dtor_routine.  A pointer to the argument is returned in *implied_arg_node,
or NULL if no implied argument is needed.  If have_complete_object is TRUE,
we know we are calling the destructor for a complete object.
*/
{
  a_type_ptr class_type;

  *implied_arg_node = NULL;
  /* Get the class type. */
  class_type = dtor_routine->source_corresp.class_of_which_a_member;
  prelower_class_type(class_type);
  /* 0x2 bit means "have complete object".  0x1 bit means "free storage"
     which does not apply here. */
  *implied_arg_node = node_for_integer_constant(have_complete_object ? 2L : 0L,
                                                (an_integer_kind)ik_int);
}  /* make_dtor_implied_arg_list */


static void add_constructor_call(a_dynamic_init_ptr     dip,
                                 an_expr_node_ptr       entity_node,
                                 an_expr_node_ptr       source_node,
                                 an_expr_node_ptr       implied_arg_list,
                                 an_expr_node_ptr       end_implied_arg_list,
                                 an_insert_location_ptr insert_location)
/*
Make a call statement that invokes a constructor as required in the dynamic
initialization entry pointed to by dip.  entity_node is an expression
that gives the address of the entity to be initialized.  If source_node
is non-NULL, it points to an expression that is the source for a copy
constructor call.  implied_arg_list is a list of implied extra virtual
base class pointer arguments for the constructor, or NULL if this routine
should generate them if required.  Insert the statement at *insert_location
and update *insert_location.  The additional-arguments list given by
dip->variant.constructor.args has already been lowered.
*/
{
  a_routine_ptr    constr_routine = dip->variant.constructor.ptr;
  an_expr_node_ptr last_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_constructor_call: bad kind");
  }  /* if */
#endif /* CHECKING */
  /* If no implied_arg_list is supplied and the constructor needs one
     (because it initializes a class that has virtual base classes), make
     the implied_arg_list (all entries are NULL pointer values). */
  if (implied_arg_list == NULL) {
    make_ctor_implied_arg_list(constr_routine, &implied_arg_list,
                               &end_implied_arg_list);
  }  /* if */
  /* Link the entity node, the implied arguments if any, the source node if
     any, and the other arguments together. */
  last_node = entity_node;
  if (implied_arg_list != NULL) {
    last_node->next = implied_arg_list;
    last_node = end_implied_arg_list;
  }  /* if */
  if (source_node != NULL) {
    last_node->next = source_node;
    last_node = source_node;
  }  /* if */
  last_node->next = dip->variant.constructor.args;
  /* Make an expression statement containing the call expression. */
  call_stmt = make_call_statement(constr_routine, entity_node);
  /* If the initialization is for a whole variable, the position is available
     from the variable. */
  transfer_seq_from_var_to_statement(dip->variable, call_stmt);
  /* Insert the statement at the right place. */
  insert_statement(call_stmt, insert_location);
}  /* add_constructor_call */

/*
Pointers to routine entries for runtime routines for call of a constructor,
copy constructor, or destructor for each element of an array, once created.
NULL until then.
*/
static a_routine_ptr
		vec_new_routine,
		vec_cctor_routine,
		vec_delete_routine;


static an_expr_node_ptr num_elem_node_from_count(long array_element_count)
/*
Build an expression for a constant that represents the number of elements
in an array for an array new/delete call.  -1 indicates a variable-length
array.
*/
{
  a_boolean        did_not_fold;
  an_expr_node_ptr num_elem_node;
  a_constant       num_elem_constant;

  /* Create the constant as long and then change it to int to get any
     truncation error. */
  set_integer_constant(&num_elem_constant, array_element_count,
                       (an_integer_kind)ik_long);
  type_change_constant(&num_elem_constant,
                       integer_type((an_integer_kind)ik_int),
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE, &did_not_fold,
                       &error_position);
  /* Allocate an expression node for the constant. */
  num_elem_node = alloc_node_for_constant(&num_elem_constant);
  return num_elem_node;
}  /* num_elem_node_from_count */


static a_type_ptr new_delete_base_type_from_operation_type(a_type_ptr type)
/*
type is the type operated on in a new or delete operation.
Extract and return the underlying entity type.
*/
{
  a_type_ptr base_type;

  base_type = type;
  /* For multi-dimensional array cases, drop down to the underlying class
     type. */
  while (is_array_type(base_type)) {
    base_type = array_element_type(base_type);
  }  /* while */
  base_type = skip_typerefs(base_type);
  return base_type;
}  /* new_delete_base_type_from_operation_type */


static an_expr_node_ptr size_elem_node_from_pointer_type(a_type_ptr ptr_type)
/*
Build an expression node for the constant that is the size of the element
type of the array pointed to by ptr_type, and return a pointer to it.
*/
{
  a_type_ptr       elem_type;
  an_expr_node_ptr size_elem_node;

  elem_type = new_delete_base_type_from_operation_type(
                                                    type_pointed_to(ptr_type));
  size_elem_node = node_for_integer_constant((long)elem_type->size,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
  return size_elem_node;
}  /* size_elem_node_from_pointer_type */


static an_expr_node_ptr make_vec_new_call(an_expr_node_ptr entity_node,
                                          an_expr_node_ptr num_elem_node,
                                          a_routine_ptr    ctor_routine)
/*
Make a call to a runtime routine (__vec_new) that will allocate an array
and call a constructor for each element of the array.  entity_node gives
the address of the array (for cases where the array is already
allocated).  num_elem_node gives (as an expression) the number of
elements in the array.  ctor_routine is the constructor routine to be
called, or NULL if no constructor is to be called.  A pointer to the
expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, size_elem_node;
  an_expr_node_ptr func_addr_node;
  a_constant       null_constant;

  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  if (ctor_routine != NULL) {
    func_addr_node = function_addr_expr(ctor_routine);
  } else {
    /* No constructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(make_vptp_type(), &null_constant);
    func_addr_node = alloc_node_for_constant(&null_constant);
  }  /* if */
  /* The call looks like
       __vec_new(entity_node, num_elems, size_elem, ctor_routine)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  call_node = make_runtime_rout_call("__vec_new", &vec_new_routine,
                                     void_star_type(), arg_expr_list);
  return call_node;
}  /* make_vec_new_call */


static an_expr_node_ptr make_vec_delete_call(
                                          an_expr_node_ptr entity_node,
                                          long             array_element_count,
                                          a_routine_ptr    dtor_routine,
                                          a_boolean        free_storage)
/*
Make a call to a runtime routine (__vec_delete) that will call a
destructor for each element of an array and then deallocate the array.
entity_node gives the address of the array.  array_element_count is the
number of elements in the array, or -1 for a variable-length array.
dtor_routine is the destructor routine to be called, or NULL if no
destructor is to be called.  free_storage is TRUE if the storage for the
array is to be freed.  A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node, free_storage_node;
  a_constant       null_constant;

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  /* Build the "free_storage" argument: 1 to free storage, 0 otherwise. */
  free_storage_node = node_for_integer_constant(free_storage ? 1L : 0L,
                                                (an_integer_kind)ik_int);
  if (dtor_routine != NULL) {
    func_addr_node = function_addr_expr(dtor_routine);
  } else {
    /* No destructor routine to call; use 0 cast to the right function
       pointer type. */
    make_zero_of_proper_type(make_vptp_type(), &null_constant);
    func_addr_node = alloc_node_for_constant(&null_constant);
  }  /* if */
  /* The call looks like
       __vec_delete(entity_node, num_elems, size_elem, dtor_routine,
                    free_storage, 0)
     The final argument is never used.  It's there for cfront compatibility.
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = free_storage_node;
  free_storage_node->next = node_for_integer_constant(0L,
                                                      (an_integer_kind)ik_int);
  call_node = make_runtime_rout_call("__vec_delete", &vec_delete_routine,
                                     void_type(), arg_expr_list);
  return call_node;
}  /* make_vec_delete_call */


static an_expr_node_ptr make_vec_cctor_call(
                                          an_expr_node_ptr entity_node,
                                          an_expr_node_ptr source_node,
                                          long             array_element_count,
                                          a_routine_ptr    cctor_routine)
/*
Make a call to a runtime routine (__vec_cctor) that will call a copy
constructor for each element of an array.  entity_node gives the address
of the array.  source_node gives the source for the copy.
array_element_count is the number of elements in the array.
cctor_routine is the copy constructor routine to be called.
A pointer to the expression created is returned.
*/
{
  an_expr_node_ptr call_node, arg_expr_list, num_elem_node, size_elem_node;
  an_expr_node_ptr func_addr_node;

  /* Build a constant node for the number of array elements. */
  num_elem_node = num_elem_node_from_count(array_element_count);
  /* Build a constant node for the size of the array elements. */
  size_elem_node = size_elem_node_from_pointer_type(entity_node->type);
  func_addr_node = function_addr_expr(cctor_routine);
  /* The call looks like
       __vec_cctor(entity_node, num_elems, size_elem, cctor_routine,
                   source_node)
  */
  arg_expr_list = entity_node;
  entity_node->next = num_elem_node;
  num_elem_node->next = size_elem_node;
  size_elem_node->next = func_addr_node;
  func_addr_node->next = source_node;
  call_node = make_runtime_rout_call("__vec_cctor", &vec_cctor_routine,
                                     void_type(), arg_expr_list);
  return call_node;
}  /* make_vec_cctor_call */


static a_routine_ptr default_version_of_routine(
                                             a_routine_ptr    routine,
                                             an_expr_node_ptr default_arg_list)
/*
Return a pointer to a routine that does the same thing as "routine" but
in which the parameters that have default argument expressions have been
removed.  The values to be used for those default arguments are given
by default_arg_list (the expressions are already lowered).  Implicitly-
generated parameters of constructors and destructors are also removed.
This is used to generate a version of a constructor or destructor that
can be called with just a "this" parameter, or of a copy constructor
that can be called with just a "this" parameter and a source pointer.
The routine must have a "this" parameter.
*/
{
  a_memory_region_number
                   region_to_switch_back_to = curr_il_region_number;
  an_expr_node_ptr implied_arg_list = NULL, end_implied_arg_list = NULL;
  an_expr_node_ptr call_node, temp_arg;
  a_routine_ptr    new_routine;
  a_type_ptr       routine_type = skip_typerefs(routine->type);
  a_type_ptr       this_param_type, pass_through_param_type;
  a_param_type_ptr src_param_type, param_type, last_param_type;
  a_routine_type_supplement_ptr
                   rtsp, new_rtsp;
  a_scope_ptr      new_routine_scope;
  an_insert_location
                   insert_location;
  a_memory_region_number
                   new_routine_il_region;
  a_variable_ptr   this_param_var, param_var, last_param_var;
  an_expr_node_ptr this_arg, pass_through_arg;
  a_statement_ptr  call_stmt, return_stmt;
  a_boolean        already_lowered;

  /* Determine any implicit arguments required for a constructor or
     destructor. */
  if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
    make_ctor_implied_arg_list(routine, &implied_arg_list,
                               &end_implied_arg_list);
  } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
    make_dtor_implied_arg_list(routine, /*have_complete_object=*/TRUE,
                               &implied_arg_list);
    end_implied_arg_list = implied_arg_list;
  }  /* if */
  if (default_arg_list != NULL || implied_arg_list != NULL) {
    /* There are some implicit or default arguments, so a wrapper routine
       must be created and used in place of the original routine. */
    /* Make a type and routine entry for the routine. */
    /* Note that the routine has no name. */
    /* The "this" parameter is generated in its lowered form (i.e., as a
       normal parameter). */
    rtsp = routine_type->variant.routine.extra_info;
    this_param_type = rtsp->implicit_this_param_type;
#if CHECKING
    /* The routine must have a "this" parameter. */
    if (this_param_type == NULL) {
      internal_error("default_version_of_routine: missing this param");
    }  /* if */
#endif /* CHECKING */
    /* Additional parameter types, if any, are added below. */
    new_routine = make_rout_entry((char *)NULL, (a_storage_class)sc_static,
                                  routine_type->variant.routine.return_type,
                                  this_param_type);
    new_rtsp = new_routine->type->variant.routine.extra_info;
#if CHECKING
    /* The routine is not allowed to be one that returns its value via
       a pointer provided by the caller (the extra code for that case
       is not implemented). */
    if (rtsp->value_returned_by_cctor) {
      internal_error("default_version_of_routine: return value ptr");
    }  /* if */
#endif /* CHECKING */
    /* Make a memory region, scope, and block for the routine definition. */
    new_routine_scope = make_routine_definition(new_routine,
                                                /*make_return=*/FALSE,
                                                &new_routine_il_region);
    switch_il_region(new_routine_il_region);
    /* Make a parameter variable for the "this" parameter (again, in lowered
       form as a normal parameter). */
    new_routine_scope->variant.routine.parameters = this_param_var =
                                          make_param_variable(this_param_type);
    this_param_var->assoc_param_type = new_rtsp->param_type_list;
    /* Make any additional parameter types and parameter vars beyond the
       "this" parameter (this comes up, for instance, on the copy
       constructor case). */
    src_param_type = rtsp->param_type_list;
    already_lowered = src_param_type != NULL && visited_yet(src_param_type);
    if (already_lowered) {
      /* The routine type has already been lowered (i.e., the "this"
         parameter type is already on the explicit parameter type list).
         Skip the first parameter type. */
      src_param_type = src_param_type->next;
      /* Also skip a parameter for each implicit parameter added (pointers
         to virtual base classes). */
      for (temp_arg = implied_arg_list;
           temp_arg != NULL;
           temp_arg = temp_arg->next) {
        src_param_type = src_param_type->next;
      }  /* if */
    }  /* if */
    last_param_type = new_rtsp->param_type_list;
    last_param_var = this_param_var;
    /* Do not process parameters with default argument values, since they
       are removed from the routine's interface. */
    for (; src_param_type != NULL && !src_param_type->has_default_arg;
         src_param_type = src_param_type->next) {
      pass_through_param_type = src_param_type->type;
      /* If the parameter is passed via a copy constructor and it has
         not been lowered, replace it by a pointer to the object.
         Note that a second copy constructor call (i.e., one within
         the generated routine) is not necessary. */
      if (!already_lowered && src_param_type->passed_via_copy_constructor) {
        pass_through_param_type = make_pointer_type(pass_through_param_type);
      }  /* if */
      param_type = alloc_param_type(pass_through_param_type);
      /* It is not necessary to clear il_lowering_flag; the entry does not need
         to be lowered.  Also note that the parameter types will be lowered
         when the original function is lowered, and do not need to be
         lowered here. */
      last_param_type->next = param_type;
      param_var = make_param_variable(pass_through_param_type);
      param_var->assoc_param_type = param_type;
      last_param_var->next = param_var;
      /* Add a reference to the parameter to the argument list to be used
         to call the original function.  This passes the parameter through
         unchanged.  Note that parameters of this type follow the
         implicit arguments, if any. */
      pass_through_arg = var_rvalue_expr(param_var);
      if (implied_arg_list == NULL) {
        implied_arg_list = pass_through_arg;
      } else {
        end_implied_arg_list->next = pass_through_arg;
      }  /* if */
      end_implied_arg_list = pass_through_arg;
      last_param_type = param_type;
      last_param_var = param_var;
    }  /* for */
    set_block_start_insert_location(new_routine_scope->assoc_block,
                                    &insert_location);
    /* Make a call statement that calls the original routine with all
       the implicit arguments, i.e., that passes all the extra arguments
       to the original routine. */
    /* Copy the default argument expressions into the function memory
       region. */
    default_arg_list = copy_list_of_expr_trees(default_arg_list);
    if (implied_arg_list != NULL) {
      /* Add the implicit arguments to the front of the default argument
         list. */
      end_implied_arg_list->next = default_arg_list;
      default_arg_list = implied_arg_list;
    }  /* if */
    /* Add the "this" parameter at the front of the argument list. */
    this_arg = var_rvalue_expr(this_param_var);
    this_arg->next = default_arg_list;
    call_node = make_call_node(routine, this_arg, /*honor_virtual=*/FALSE);
    /* If the routine has a void type, insert a statement for the call
       followed by a return statement.  Otherwise, attach the call directly
       to the return. */
    if (is_void_type(call_node->type)) {
      /* Insert the statement at the right place. */
      call_stmt = alloc_statement((a_statement_kind)stmk_expr);
      call_stmt->expr = call_node;
      insert_statement(call_stmt, &insert_location);
      call_node = NULL;
    }  /* if */
    return_stmt = alloc_statement((a_statement_kind)stmk_return);
    return_stmt->expr = call_node;
    insert_statement(return_stmt, &insert_location);
    done_with_memory_region(new_routine_il_region);
    switch_il_region(region_to_switch_back_to);
    routine = new_routine;
  }  /* if */
  return routine;
}  /* default_version_of_routine */


static void add_array_constructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   an_expr_node_ptr       source_node,
                                   long                   array_element_count,
                                   an_insert_location_ptr insert_location)
/*
Generate code that calls a constructor for each element of an array.
dip indicates the initialization to be performed; entity_node gives the
address of the array; source_node (if non-NULL) gives the address of
the source for a copy constructor call; and array_element_count gives the
number of elements in the array.  Insert the statements at *insert_location
and update *insert_location.  The additional-arguments list given by
dip->variant.constructor.args has already been lowered.
*/
{
  a_routine_ptr    ctor_routine;
  an_expr_node_ptr call_node, num_elem_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_constructor) {
    internal_error("add_array_constructor_call: not dik_constructor");
  }  /* if */
#endif /* CHECKING */
  ctor_routine = dip->variant.constructor.ptr;
  ctor_routine = default_version_of_routine(ctor_routine,
                                            dip->variant.constructor.args);
  if (source_node != NULL) {
    /* Copy constructor case. */
    call_node = make_vec_cctor_call(entity_node, source_node,
                                    array_element_count, ctor_routine);
  } else {
    /* Normal constructor case. */
    /* Build a constant node for the number of array elements. */
    num_elem_node = num_elem_node_from_count(array_element_count);
    call_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine);
  }  /* if */
  /* Make a statement containing the call. */
  call_stmt = alloc_statement((a_statement_kind)stmk_expr);
  call_stmt->expr = call_node;
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* add_array_constructor_call */


static void add_destructor_call(a_dynamic_init_ptr     dip,
                                an_expr_node_ptr       entity_node,
                                a_boolean              have_complete_object,
                                an_insert_location_ptr insert_location)
/*
Make a call statement that invokes a destructor as required in the dynamic
initialization entry pointed to by dip.  entity_node is an expression
that gives the address of the entity to be destroyed.  have_complete_object
is TRUE if the entity is a complete object.  Insert the statement at
*insert_location and update *insert_location.  The call generated is
not a virtual call even if the destructor is virtual.
*/
{
  a_routine_ptr    destr_routine = dip->destructor;
  a_statement_ptr  call_stmt;
  an_expr_node_ptr implied_arg_node;

#if CHECKING
  if (destr_routine == NULL) {
    internal_error("add_destructor_call: destructor == NULL");
  }  /* if */
#endif /* CHECKING */
  /* If the destructor is for a class that has virtual base classes, add
     the implicit complete-object argument. */
  make_dtor_implied_arg_list(destr_routine, have_complete_object,
                             &implied_arg_node);
  entity_node->next = implied_arg_node;
  /* Make an expression statement containing the call expression. */
  call_stmt = make_call_statement(destr_routine, entity_node);
  /* If the destruction is for a whole variable, the position is available
     from the variable. */
  transfer_seq_from_var_to_statement(dip->variable, call_stmt);
  /* Insert the statement at the right place. */
  insert_statement(call_stmt, insert_location);
}  /* add_destructor_call */


static void add_array_destructor_call(
                                   a_dynamic_init_ptr     dip,
                                   an_expr_node_ptr       entity_node,
                                   long                   array_element_count,
                                   an_insert_location_ptr insert_location)
/*
Generate code that calls a destructor for each element of an array.
dip indicates the destruction to be performed; entity_node gives the
address of the array; and array_element_count gives the number of elements
in the array.  Insert the statements at *insert_location and update
*insert_location.
*/
{
  a_routine_ptr    dtor_routine;
  an_expr_node_ptr call_node;
  a_statement_ptr  call_stmt;

#if CHECKING
  if (dip->destructor == NULL) {
    internal_error("add_array_destructor_call: no destructor");
  }  /* if */
#endif /* CHECKING */
  dtor_routine = dip->destructor;
  /* default_version_of_routine is not called on purpose; __vec_delete
     knows about the implicit argument for destructors and generates
     it automatically. */
  /* Generate the __vec_delete call. */
  call_node = make_vec_delete_call(entity_node, array_element_count,
                                   dtor_routine, /*free_storage=*/FALSE);
  /* Make a statement containing the call. */
  call_stmt = alloc_statement((a_statement_kind)stmk_expr);
  call_stmt->expr = call_node;
  /* Insert the statement at the right location. */
  insert_statement(call_stmt, insert_location);
}  /* add_array_destructor_call */


static void lower_ck_dynamic_init(a_constant_ptr         con_ptr,
                                  an_init_pos_descr_ptr  ipdp,
                                  a_variable_ptr         first_time_test_var,
                                  a_boolean              dtor_case,
                                  a_constructor_init_ptr ctor_init,
                                  an_insert_location_ptr insert_location)
/*
Generate executable code to handle a ck_dynamic_init constant (pointed
to by con_ptr).  The entity to be initialized is described by ipdp.
The necessary statements are inserted at *insert_location and
*insert_location is updated.  If ipdp->whole_array is TRUE, this
call is handling all the elements of an array.  If first_time_test_var
is non-NULL, it points to a variable entry for the first-time-test variable
that controls access to this initialization.  If dtor_case is TRUE, do
the destruction indicated in the dynamic init immediately and ignore the
initialization.  If the dynamic initialization is part of a constructor
initializer, ctor_init points to the constructor-init entry.
*/
{
  a_constant_ptr next_con;
  a_boolean      keep_dynamic_init;
  a_type_ptr     desired_type;

  if (dtor_case) {
    /* In a destructor case, so the "initialization" is really
       destruction. */
    lower_destructor_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                                  insert_location);
  } else {
    /* Normal initialization. */
    lower_dynamic_init(con_ptr->variant.dynamic_init, ipdp,
                       first_time_test_var, /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       ctor_init, insert_location, &keep_dynamic_init);
#if CHECKING
    if (keep_dynamic_init) {
      internal_error("lower_ck_dynamic_init: keep_dynamic_init unexpected");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Overwrite the constant with a harmless constant of the right kind.
     It's just a place-holder that gets overwritten by the dynamic
     initialization. */
  desired_type = ipdp->modifiers->type;
  if (is_aggregate_or_union_type(desired_type)) {
    /* An aggregate is initialized with a ck_dynamic_init.  This can
       come up in something like
         complex v[6] = {1, complex(1,2), complex(), 4};
       (From the ARM, 12.6.1).  Fortunately, if there's one of these
       cases in a ck_aggregate, there can be no "normal" constants
       in the aggregate, and the whole aggregate will be thrown
       away.   Therefore we just leave the ck_dynamic_init constant
       as it is. */
#if CHECKING
    con_ptr->kind = (a_constant_repr_kind)ck_error;
#endif /* CHECKING */
  } else {
    /* Not an aggregate: a zero of the right type will be fine. */
    next_con = con_ptr->next;
    make_zero_of_proper_type(desired_type, con_ptr);
    con_ptr->next = next_con;
  }  /* if */
}  /* lower_ck_dynamic_init */


static void lower_dynamic_init_aggregate_constant(
                                   a_constant_ptr         aggr_const,
                                   an_init_pos_descr_ptr  ipdp,
                                   a_variable_ptr         first_time_test_var,
                                   a_boolean              dtor_case,
                                   a_constructor_init_ptr ctor_init,
                                   an_insert_location_ptr insert_location,
                                   a_boolean              *keep_constant)
/*
aggr_const points to a ck_aggregate constant that contains one or more
ck_dynamic_init dynamic initializations.  The ck_aggregate constant is
the initial value for the entity described by ipdp.  If first_time_test_var
is non-NULL, it points to a variable entry for the first-time-test variable
that controls access to this initialization.  If dtor_case is TRUE, this
"initialization" is a destruction to be done immediately.  If the dynamic
initialization is part of a constructor initializer, ctor_init points to
the constructor-init entry.  Insert statements to implement the
initialization at *insert_location and update *insert_location.  If there
are any (genuine) constants in the aggregate, set *keep_constant to TRUE.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm, *ipmp;
  a_type_ptr           aggr_type;
  a_constant_ptr       con_ptr, repeated_con;

  /* Mark the constant as visited.  This is necessary if the aggregate
     constant ends up being kept because something constant remains after
     the non-constant parts have been rewritten. */
  mark_as_visited(aggr_const);
  /* Determine the type of the aggregate being initialized. */
  if (ipdp->modifiers != NULL) {
    aggr_type = ipdp->modifiers->type;
  } else {
    aggr_type = ipdp->base_type;
  }  /* if */
  aggr_type = skip_typerefs(aggr_type);
  /* Start a new level in the init_pos_modifier chain. */
  ipd = *ipdp;
  ipmp = &ipm;
  add_init_pos_modifier(ipmp, &ipd);
  /* Determine the type of the first element of the aggregate being
     initialized. */
  if (aggr_type->kind == (a_type_kind)tk_array) {
    /* Array -- get the element type. */
    ipmp->curr_elem = 0;
    ipmp->type = aggr_type->variant.array.element_type;
  } else {
#if CHECKING
    if (!is_immediate_class_type(aggr_type)) {
      internal_error("lower_dynamic_init_aggregate_constant: bad aggr kind");
    }  /* if */
#endif /* CHECKING */
    /* Class, struct, or union -- get first field (nonstatic data member). */
    ipmp->curr_field = aggr_type->variant.class_struct_union.field_list;
#if CHECKING
    if (ipmp->curr_field == NULL) {
      internal_error("lower_dynamic_init_aggregate_constant: no fields");
    }  /* if */
#endif /* CHECKING */
    ipmp->type = ipmp->curr_field->type;
  }  /* if */
  /* Look for dynamic init constants on the list of constants. */
  con_ptr = aggr_const->variant.aggregate.first_constant;
  for (;;) {
    if (con_ptr->kind == (a_constant_repr_kind)ck_dynamic_init) {
      /* Dynamic initialization. */
      lower_ck_dynamic_init(con_ptr, &ipd, first_time_test_var,
                            dtor_case, ctor_init, insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_init_repeat) {
      /* Repeated constant.  Must be initializing members of an array. */
#if CHECKING
      if (aggr_type->kind != (a_type_kind)tk_array) {
        internal_error(
                 "lower_dynamic_init_aggregate_constant: repeat on non-array");
      }  /* if */
#endif /* CHECKING */
      repeated_con = con_ptr->variant.init_repeat.constant;
      /* Repeat the constant the right number of times.  It must be a
         ck_dynamic_init constant. */
#if CHECKING
      if (repeated_con->kind != (a_constant_repr_kind)ck_dynamic_init) {
        internal_error(
    "lower_dynamic_init_aggregate_constant: repeated con not ck_dynamic_init");
      }  /* if */
#endif /* CHECKING */
      ipd.whole_array = TRUE;
      ipd.array_element_count = con_ptr->variant.init_repeat.count;
      lower_ck_dynamic_init(repeated_con, &ipd, first_time_test_var,
                            dtor_case, ctor_init, insert_location);
    } else if (con_ptr->kind == (a_constant_repr_kind)ck_aggregate) {
      /* Aggregate constant initializing a member of an aggregate. */
      lower_dynamic_init_aggregate_constant(con_ptr, &ipd, first_time_test_var,
                                            dtor_case, ctor_init,
                                            insert_location, keep_constant);
    } else {
      /* Normal constant. */
      lower_constant(con_ptr);
      *keep_constant = TRUE;
    }  /* if */
    /* Go on to the next constant if there is one. */
    con_ptr = con_ptr->next;
    if (con_ptr == NULL) break;
    /* Find the next element type in the aggregate. */
    if (aggr_type->kind == (a_type_kind)tk_array) {
      /* Array -- go on to next element; element type does not change. */
      ipmp->curr_elem++;
    } else {
#if CHECKING
      if (!is_immediate_class_type(aggr_type)) {
        internal_error(
                   "lower_dynamic_init_aggregate_constant: bad aggr kind (2)");
      }  /* if */
#endif /* CHECKING */
      /* Class, struct, or union -- go on to next field (nonstatic data
         member). */
      ipmp->curr_field = ipmp->curr_field->next;
#if CHECKING
      if (ipmp->curr_field == NULL) {
        internal_error(
                   "lower_dynamic_init_aggregate_constant: not enough fields");
      }  /* if */
#endif /* CHECKING */
      ipmp->type = ipmp->curr_field->type;
    }  /* if */
  }  /* for */
}  /* lower_dynamic_init_aggregate_constant */


static void make_code_to_invoke_file_scope_init_and_term_routines(void)
/*
Make the code that will ensure that the file-scope initialization and
termination routines (if any) are invoked at program startup and
termination.
*/
{
  a_type_ptr       func_type, struct_type, ptr_struct_type;
  a_type_ptr       ptr_func_type, char_type;
  a_targ_size_t    byte_offset;
  a_field_ptr      last_field;
  a_variable_ptr   link_var;
  a_constant_ptr   aggr_con, init_con1, init_con2, init_con3;
  a_memory_region_number
                   region_to_switch_back_to;

  /* Only generate the code if there is a startup or termination routine. */
  if (file_scope_init_routine != NULL || file_scope_term_routine != NULL) {
    /* Create a __link variable pointing to a struct that points to the
       initialization/termination routine, using the same form as cfront:
         char __sti__module_id() {...}
         char __std__module_id() {...}
         struct __linkl {
           struct __linkl *next;
           char           (*ctor)();
           char           (*dtor)();
         };
         static struct __linkl __link = {NULL, __sti__module_id,
                                               __std__module_id};
       The AT&T patch step will find the __link static variable
       and link it with other initialization code to be invoked by _main.
       Alternatively, the munch step will find the routines with names
       beginning "__sti__" and "__std__".
    */
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* Make the __linkl struct type.  It doesn't actually have a name. */
    struct_type = alloc_type((a_type_kind)tk_struct);
    byte_offset = 0;
    last_field = NULL;
    /* field: struct __linkl *next; */
    ptr_struct_type = make_pointer_type(struct_type);
    make_field("next", ptr_struct_type, &byte_offset, struct_type,
               &last_field);
    /* field: char (*ctor)(); */
    char_type = integer_type(plain_char_int_kind);
    func_type = make_function_type(char_type, (a_type_ptr)NULL);
    ptr_func_type = make_pointer_type(func_type);
    make_field("ctor", ptr_func_type, &byte_offset, struct_type, &last_field);
    /* field: char (*dtor)(); */
    make_field("dtor", ptr_func_type, &byte_offset, struct_type, &last_field);
    finish_class_type(struct_type, &byte_offset);
    add_to_front_of_file_scope_types_list(struct_type);
    /* Make the __link variable. */
    link_var = make_variable("__link", /*already_il_name=*/FALSE, struct_type,
                             (a_storage_class)sc_static);
    /* Give the __link variable the initial value
         {NULL, __sti__module_id, __std__module_id}
       If either routine does not exist, use a NULL instead. */
    aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
    link_var->init_kind = (an_init_kind)initk_static;
    link_var->initializer.constant = aggr_con;
    /* Zero for "next" field. */
    init_con1 = alloc_constant((a_constant_repr_kind)ck_address);
    make_zero_of_proper_type(ptr_struct_type, init_con1);
    /* Address of __sti__module_id for "ctor" field. */
    init_con2 = alloc_constant((a_constant_repr_kind)ck_address);
    if (file_scope_init_routine != NULL) {
      init_con2->variant.address.kind = (an_address_base_kind)abk_routine;
      init_con2->variant.address.variant.routine = file_scope_init_routine;
      init_con2->type = ptr_func_type;
    } else {
      /* No init routine.  Use NULL. */
      make_zero_of_proper_type(ptr_func_type, init_con2);
    }  /* if */
    /* Address of __std__module_id for "dtor" field. */
    init_con3 = alloc_constant((a_constant_repr_kind)ck_address);
    if (file_scope_term_routine != NULL) {
      init_con3->variant.address.kind = (an_address_base_kind)abk_routine;
      init_con3->variant.address.variant.routine = file_scope_term_routine;
      init_con3->type = ptr_func_type;
    } else {
      /* No init routine.  Use NULL. */
      make_zero_of_proper_type(ptr_func_type, init_con3);
    }  /* if */
    /* Link the constants together under the ck_aggregate constant. */
    aggr_con->variant.aggregate.first_constant = init_con1;
    init_con1->next = init_con2;
    init_con2->next = init_con3;
    aggr_con->variant.aggregate.last_constant  = init_con3;
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
}  /* make_code_to_invoke_file_scope_init_and_term_routines */


/*
String made from the primary source file name and the current date/time,
used to generate unique names for the initialization and termination routines.
NULL until set by make_module_id.
*/
static char	*module_id;


static void change_non_id_characters(char *str)
/*
Change any non-identifier characters in the indicated string to underscores.
*/
{
  for (; *str != '\0'; str++) if (!isalnum((unsigned char)*str)) *str = '_';
}  /* change_non_id_characters */


static void make_module_id(void)
/*
Make a string that is based on the name of the current module and is used to
qualify static names that are put out as external names, to make them unique.
Set module_id to the string.  Do not make the string again it it has already
been made.
*/
{
  char     *file_name = il_header.primary_source_file->file_name;
  char     *date_time = il_header.time_of_compilation;
  sizeof_t file_name_len = strlen(file_name);

  if (module_id == NULL) {
    /* The identifier is made of the primary source file name plus the
       current date and time, with non-identifier characters changed to
       underscores. */
    module_id = alloc_general(file_name_len + 1 + strlen(date_time) + 1);
    (void)strcpy(module_id, file_name);
    module_id[file_name_len] = '_';
    (void)strcpy(module_id+file_name_len+1, date_time);
    /* Change non-identifier characters to "_". */
    change_non_id_characters(module_id);
  }  /* if */
}  /* make_module_id */


static a_routine_ptr make_file_scope_init_or_term_routine(
                                       char                   *prefix,
                                       an_insert_location_ptr insert_location,
                                       a_scope_ptr            *init_rout_scope,
                                       a_memory_region_number *il_region)
/*
Make a routine to do file-scope initialization or termination.  prefix is
the prefix for the name of the routine.  Set *insert_location for insertion at
the start of the block statement that is the body of the routine, set
*init_rout_scope to point to the scope entry for the routine, set
*il_region to the IL memory region number for the routine, and return a
pointer to the routine.
*/
{
  a_routine_ptr init_rout;
  char          *name;
  sizeof_t      prefix_len = strlen(prefix), alloc_length;

  /* Combine the prefix and an identifier for the current module to make
     a name that is likely to be unique. */
  make_module_id();
  alloc_length = prefix_len + strlen(module_id) + 1;
  name = alloc_il(alloc_length);
#if DEBUG
  allocated_name_string_length += alloc_length;
#endif /* DEBUG */
  (void)memcpy(name, prefix, size_t_arg(prefix_len));
  (void)strcpy(name+prefix_len, module_id);
  /* Make a type and routine entry for the routine. */
  init_rout = make_rout_entry(name, (a_storage_class)sc_unspecified,
                              integer_type(plain_char_int_kind),
                              (a_type_ptr)NULL);
  /* Make a memory region, scope, and block for the init routine definition. */
  *init_rout_scope = make_routine_definition(init_rout, /*make_return=*/TRUE,
                                             il_region);
  set_block_start_insert_location((*init_rout_scope)->assoc_block,
                                  insert_location);
  return init_rout;
}  /* make_file_scope_init_or_term_routine */


static a_scope_ptr file_scope_init_insert_location(
                                        an_insert_location_ptr insert_location)
/*
Determine the insert location for a file-scope initialization statement.
*/
{
  a_scope_ptr scope;

  if (file_scope_init_routine == NULL) {
    file_scope_init_routine = make_file_scope_init_or_term_routine(
                                      IL_LOWERING_INIT_ROUTINE_PREFIX,
                                      &file_scope_init_routine_insert_location,
                                      &scope,
                                      &file_scope_init_routine_il_region);
  }  /* if */
  *insert_location = file_scope_init_routine_insert_location;
  return scope;
}  /* file_scope_init_insert_location */


static a_scope_ptr file_scope_term_insert_location(
                                        an_insert_location_ptr insert_location)
/*
Determine the insert location for a file-scope termination statement.
*/
{
  a_scope_ptr scope;

  if (file_scope_term_routine == NULL) {
    file_scope_term_routine = make_file_scope_init_or_term_routine(
                                      IL_LOWERING_TERM_ROUTINE_PREFIX,
                                      &file_scope_term_routine_insert_location,
                                      &scope,
                                      &file_scope_term_routine_il_region);
  }  /* if */
  *insert_location = file_scope_term_routine_insert_location;
  return scope;
}  /* file_scope_term_insert_location */


static a_variable_ptr var_for_copy_constructor_source(void)
/*
We are currently expanding the body of a copy constructor.  Return a pointer
for the source parameter of the copy constructor.
*/
{
  a_routine_ptr    curr_routine;
  a_variable_ptr   source_param_var;
  a_type_ptr       class_type;
  a_base_class_ptr bcp;

  curr_routine = nearest_function_scope->variant.routine.ptr;
#if CHECKING
  if (curr_routine->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error(
             "var_for_copy_constructor_source: curr routine not constructor");
  }  /* if */
#endif /* CHECKING */
  source_param_var = nearest_function_scope->variant.routine.parameters->next;
#if CHECKING
  if (source_param_var == NULL) {
    internal_error("var_for_copy_constructor_source: source param missing");
  }  /* if */
#endif /* CHECKING */
  /* Skip over any parameters added for virtual base class pointers.
     See add_constructor_params. */
  class_type = curr_routine->source_corresp.class_of_which_a_member;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        source_param_var = source_param_var->next;
#if CHECKING
        if (source_param_var == NULL) {
          internal_error(
                  "var_for_copy_constructor_source: source param missing (2)");
        }  /* if */
#endif /* CHECKING */
      }  /* if */
    }  /* for */
  }  /* if */
  return source_param_var;
}  /* var_for_copy_constructor_source */


static void add_conditional_destruction_temp(
                               a_required_destructor_call_ptr rdcp,
                               an_insert_location             *insert_location)
/*
rdcp points to a required destructor call being generated.  We are currently
inside a conditional operand of a "?", "&&", or "||" operation.  Since
the construction is conditional, we add a temporary variable, initialize
it to zero at the beginning of the current block, and insert an assignment
to set the temporary to 1 before insert_location.  The destruction generated
later will be made conditional on the temporary.
*/
{
  a_variable_ptr      temp;
  a_dynamic_init_ptr  dip, prev_dip;
  a_constant          zero_constant;
  a_statement_ptr     stmk_init_stmt, block, label_statement;
  a_scope_ptr         scope;
  a_switch_clause_ptr scp;
  an_insert_location  insert_before_location;

  rdcp->first_time_test_var = temp =
                         make_temporary(integer_type((an_integer_kind)ik_int));
  /* The temporary must be initialized to zero.  If it is static, that
     is done implicitly.  Otherwise, it must be done dynamically. */
  if (temp->storage_class != (a_storage_class)sc_static) {
    /* Use a dynamic init entry to do the initialization. */
    dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constant);
    dip->variable = temp;
    /* The dynamic init entry is pointed to by the variable. */
    temp->init_kind = (an_init_kind)initk_dynamic;
    temp->initializer.dynamic = dip;
    set_integer_constant(&zero_constant, 0L, (an_integer_kind)ik_int);
    dip->variant.constant = alloc_unshared_constant(&zero_constant);
    /* The dynamic init entry is pointed to by an stmk_init statement. */
    stmk_init_stmt = alloc_statement((a_statement_kind)stmk_init);
    stmk_init_stmt->variant.dynamic_init = dip;
    scope = curr_context->scope;
    scp = curr_context->assoc_switch_clause;
    /* The stmk_init statement must be inserted at the right place.  For
       most cases, the right place is the beginning of the current block.
       For switch clauses, it's the beginning of the clause.  When labels
       appear, the initialization goes after the latest label. */
    label_statement = curr_context->latest_label_statement_processed;
    if (label_statement != NULL) {
      /* Insert the stmk_init after the most recent label. */
      /* The dynamic init is not at the start of the scope. */
      dip->follows_an_exec_statement = TRUE;
      /* Add the stmk_init statement after the label. */
      stmk_init_stmt->next = label_statement->next;
      label_statement->next = stmk_init_stmt;
      prev_dip = curr_context->dynamic_init_preceding_label;
    } else if (scp != NULL) {
      /* Switch clause. */
      /* The dynamic init is not at the start of the scope. */
      dip->follows_an_exec_statement = TRUE;
      /* Add the stmk_init statement at the beginning of the clause. */
      stmk_init_stmt->next = scp->statements;
      scp->statements = stmk_init_stmt;
      prev_dip = curr_context->dynamic_init_preceding_switch_clause;
    } else {
      /* Normal case. */
      block = scope->assoc_block;
#if CHECKING
      if (block == NULL) {
        internal_error("add_conditional_destruction_temp: missing block");
      }  /* if */
#endif /* CHECKING */
      /* Add the stmk_init statement at the beginning of the block. */
      stmk_init_stmt->next = block->variant.block.statements;
      block->variant.block.statements = stmk_init_stmt;
      prev_dip = NULL;
    }  /* if */
    /* Add the dynamic init entry at the right spot in the dynamic inits
       list.  Note that we do the insert of the statement and the dynamic
       init entry at the front of the list each time, so they end up in
       reverse order of insertion. */
    if (prev_dip == NULL) {
      dip->next = scope->dynamic_inits;
      scope->dynamic_inits = dip;
    } else {
      dip->next = prev_dip->next;
      prev_dip->next = dip;
    }  /* if */
    /* If the dynamic init is the first one so far in the current
       clause (i.e., there are no previous initializations of this kind and
       no pre-existing dynamic inits), record it as the last dynamic init
       processed. */
    if (curr_context->latest_dynamic_init_processed == prev_dip) {
      curr_context->latest_dynamic_init_processed = dip;
    }  /* if */
  }  /* if */
  /* Make and insert an assignment statement to set the temporary to 1.
     The insertion is done before the indicated location, which is presumably
     the expression already generated to do the initialization. */
  insert_before_location = *insert_location;
#if CHECKING
  if (!insert_before_location.expr_insert ||
      insert_before_location.variant.expr.insert_before) {
    internal_error("add_conditional_destruction_temp: bad insert loc");
  }  /* if */
#endif /* CHECKING */
  insert_before_location.variant.expr.insert_before = TRUE;
  (void)insert_assignment_statement(var_lvalue_expr(temp),
                                    (an_expr_operator_kind)eok_iassign,
                                    node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                    &insert_before_location);
}  /* add_conditional_destruction_temp */


static void lower_dynamic_init(a_dynamic_init_ptr     dip,
                               an_init_pos_descr_ptr  ipdp,
                               a_variable_ptr         first_time_test_var,
                               a_boolean              is_expr_temporary,
                               an_expr_node_ptr       implied_arg_list,
                               an_expr_node_ptr       end_implied_arg_list,
                               a_constructor_init_ptr ctor_init,
                               an_insert_location_ptr insert_location,
                               a_boolean              *keep_dynamic_init)
/*
Do IL lowering of the indicated dynamic initialization and everything under
it.  ipdp indicates the entity to be initialized.  Ordinarily, that is the
entire variable indicated in the dynamic initialization entry (that happens
when the entry is pointed to by an stmk_init statement or when it appears
on a file-scope dynamic_inits list).  ipdp can, however, indicate a part of
an aggregate.

If first_time_test_var is non-NULL, it points to a variable entry for
the first-time-test variable that controls access to this initialization
of a local static variable.

If is_expr_temporary is TRUE, this dynamic initialization is pointed to by
an enk_temp_init expression node, i.e., it initializes a temporary in
an expression.

If implied_arg_list and end_implied_arg_list are non-NULL, they point to
the beginning and end of a list of implied arguments for a constructor
call (for implicit virtual base class arguments).

If the dynamic initialization is part of a constructor initializer,
ctor_init points to the constructor-init entry.

This routine is only called for non-C cases, and therefore it will always
generate some executable code.  (Well, almost always: a dynamic initialization
that contains a destructor but that could otherwise be rendered as a static
initialization will be turned into the static initialization, which means
no code will be generated.)  The code will be inserted at *insert_location.
*insert_location will be updated to indicate a location after the inserted
code.

On return, *keep_dynamic_init is TRUE if the dynamic init entry is to
be kept, FALSE if it should be deleted.
*/
{
  an_expr_node_ptr  entity_node, source_node;
  a_variable_ptr    variable;
  a_boolean         simple_constant_init = FALSE, keep_constant;
  a_constant_ptr    simple_constant;
  a_context_ptr     destructor_context;
  a_source_position saved_error_position;
  a_statement_ptr   expr_stmt;

  *keep_dynamic_init = FALSE;
  saved_error_position = error_position;
  variable = dip->variable;
  if (variable != NULL) {
    /* Whole-variable initialization. */
    /* Track the source position for internal errors. */
    error_position = variable->source_corresp.decl_position;
#if CHECKING
    if (variable != ipdp->variable) {
      internal_error("lower_dynamic_init: variable mismatch");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_constant:
      /* Assign a constant to the entity to be initialized. */
      lower_constant(dip->variant.constant);
      /* If there is a whole variable, this dynamic initialization can be
         rendered in C IL without generating an assignment.  It's probably
         here as a dynamic initialization because it has a destructor call
         too. */
      if (variable != NULL) {
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
        break;
      }  /* if */
      /* For the normal cases, go on and generate an assignment. */
      goto do_assignment;
    case dik_expression:
      /* Assign an expression to the entity to be initialized. */
      lower_normal_expr(dip->variant.expression);
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           an expression from the file scope that must be used in the function
           scope of the initialization routine, so copy it.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
do_assignment:;
#if CHECKING
      if (ipdp->whole_array) {
        internal_error("lower_dynamic_init: array for const or expr init");
      }  /* if */
#endif /* CHECKING */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp);
      add_init_assignment(dip, entity_node, insert_location);
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialize the entry by calling a routine that returns its result
         via a copy constructor. */
      /* The address of the temporary being initialized is added as an
         implicit argument of the call. */
      lower_call(dip->variant.expression, ipdp);
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           an expression from the file scope that must be used in the function
           scope of the initialization routine, so copy it.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.expression = copy_expr_tree(dip->variant.expression);
      }  /* if */
      expr_stmt = insert_expr_statement(dip->variant.expression,
                                        insert_location);
      transfer_seq_from_var_to_statement(variable, expr_stmt);
      break;
    case dik_constructor:
      /* Initialize the entity by calling a constructor. */
      /* The routine does not need to be lowered from here. */
      lower_arg_expr_list(dip->variant.constructor.args,
                          dip->variant.constructor.ptr->type);
      if (processing_file_scope_init_routine ||
          first_time_test_var != NULL) {
        /* When generating the file-scope initialization routine we have
           expressions from the file scope that must be used in the function
           scope of the initialization routine, so copy them.  Otherwise
           we have a difficult job keeping track of the nodes that are in
           the file scope and those that are in the function scope.
           Similar reasoning applies to local static variables. */
        dip->variant.constructor.args =
                        copy_list_of_expr_trees(dip->variant.constructor.args);
      }  /* if */
      /* Make a node for the entity to be initialized. */
      entity_node = make_init_entity_node(ipdp);
      source_node = NULL;
      if (dip->variant.constructor.is_copy_constructor_for_subobject) {
        an_init_pos_descr    cctor_source_ipd;
        an_init_pos_modifier cctor_source_ipm;
        /* The constructor being called is a copy constructor.  The argument
           for the source object is also implicit and needs to be generated.
           It's the first real parameter of the copy constructor routine
           being expanded, modified like the entity_node. */
        set_var_indirect_init_pos_descr(var_for_copy_constructor_source(),
                                        &cctor_source_ipd);
        modify_ctor_init_pos_descr(ctor_init, &cctor_source_ipd,
                                   &cctor_source_ipm);
        source_node = make_init_entity_node(&cctor_source_ipd);
      }  /* if */
      if (ipdp->whole_array) {
        /* Construct an array. */
#if CHECKING
        if (implied_arg_list != NULL) {
          internal_error("lower_dynamic_init: implied arg list for array");
        }  /* if */
#endif /* CHECKING */
        add_array_constructor_call(dip, entity_node, source_node,
                                   ipdp->array_element_count,
                                   insert_location);
      } else {
        /* Construct a simple entity (not an array). */
        add_constructor_call(dip, entity_node, source_node,
                             implied_arg_list, end_implied_arg_list,
                             insert_location);
      }  /* if */
      break;
    case dik_nonconstant_aggregate:
      /* Initialization with a nonconstant aggregate constant.  This is usually
         a whole-variable initialization, but can be used in a ctor-initializer
         to iterate over an array initialization, etc. */
      keep_constant = FALSE;
      lower_dynamic_init_aggregate_constant(dip->variant.constant,
                                            ipdp, first_time_test_var, 
                                            /*dtor_case=*/FALSE, ctor_init,
                                            insert_location,
                                            &keep_constant);
      if (keep_constant) {
        /* Keep a (now-)constant aggregate value as the static initial value
           of the variable.  The nonconstant parts have been put out as
           code and replaced with placeholder constants. */
        simple_constant_init = TRUE;
        simple_constant = dip->variant.constant;
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("lower_dynamic_init: bad kind");
#endif /* CHECKING */
  }  /* switch */
  /* If the dynamic init entry indicates a destructor call, put it on a
     list of destructor calls to be processed at the end of the scope.
     Note that the list gets built in the right order (i.e., the reverse of
     construction order) because each entry is added to the front of the
     list. */
  if (dip->destructor != NULL) {
    a_required_destructor_call_ptr rdcp;
    rdcp = alloc_required_destructor_call();
    /* Copy the entire dynamic init entry because in the case of a local
       static variable the dynamic init entry will be gone by the time the
       destructor call is put out (it's in the function scope memory
       region). */
    rdcp->dynamic_init = *dip;
    /* Clear the destructor field in the dynamic init entry to make it legal
       C IL. */
    dip->destructor = NULL;
    rdcp->init_pos_descr = *ipdp;
    rdcp->is_expr_temporary = is_expr_temporary;
    rdcp->label_count = curr_context->label_count;
    /* If this is an initialization within an aggregate, we must save the
       init_pos_modifier list.  However, the list runs through the stack,
       so we must make an allocated copy. */
    if (ipdp->modifiers != NULL) {
      rdcp->init_pos_descr.modifiers =
                                  copy_init_pos_modifier_list(ipdp->modifiers);
    }  /* if */
    if (first_time_test_var != NULL) {
      /* Destruction of local static variables must happen at the end of
         the file scope if the initialization has been done (i.e., if the
         first-time-test variable has been set to non-zero. */
      rdcp->first_time_test_var = first_time_test_var;
      /* Put the entry on the front of a special list. */
      rdcp->next = destructor_calls_for_local_static_variables;
      destructor_calls_for_local_static_variables = rdcp;
      if (end_destructor_calls_for_local_static_variables == NULL) {
        end_destructor_calls_for_local_static_variables = rdcp;
      }  /* if */
      /* Indicate to the back end that there will be a non-local reference to
         the variable (from the termination routine). */
      ipdp->variable->referenced_non_locally = TRUE;
    } else {
      /* Destruction of other variables must happen at the end of the current
         scope. */
      destructor_context = curr_context;
      /* For processing of file-scope dynamic inits, put the entry on the
         file-scope list. */
      if (processing_file_scope_init_routine) {
        destructor_context = file_scope_context;
      }  /* if */
      /* Put the new entry on the front of the existing list. */
      rdcp->next = destructor_context->required_destructor_calls;
      destructor_context->required_destructor_calls = rdcp;
      if (num_conditional_exprs_inside_of != 0) {
        /* Inside a conditional operand of a "?", "&&", or "||" operation.
           Since the construction is conditional, we add a temporary
           variable, initialize it to zero at the beginning of the current
           block, set the temporary to 1 here, and test the temporary
           variable later to decide whether or not to do the destruction. */
        add_conditional_destruction_temp(rdcp, insert_location);
      }  /* if */
    }  /* if */
  }  /* if */
  /* In the whole-variable cases, adjust the initialization specified in
     the variable (it points to the dynamic init entry). */
  if (variable != NULL) {
    if (simple_constant_init) {
      /* Initialization to a simple constant. */
      if (has_static_storage_duration(variable->storage_class)) {
        /* Initialization of a static variable to a constant.  Can be
           done as a static initialization. */
        variable->init_kind = (an_init_kind)initk_static;
        variable->initializer.constant = simple_constant;
      } else {
        /* Initialization of an automatic variable to a constant.  Can be done
           by keeping the dynamic init entry. */
        *keep_dynamic_init = TRUE;
        set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constant);
        dip->variant.constant = simple_constant;
      }  /* if */
    } else {
      /* The initialization is handled entirely by the generated code, so the
         variable should no longer be marked as initialized. */
      variable->init_kind = (an_init_kind)initk_none;
    }  /* if */
  }  /* if */
  error_position = saved_error_position;
}  /* lower_dynamic_init */


static void lower_destructor_dynamic_init(
                                        a_dynamic_init_ptr     dip,
                                        an_init_pos_descr_ptr  ipdp,
                                        an_insert_location_ptr insert_location)
/*
Do IL lowering on a dynamic initialization entry that represents a destructor
call in a destructor's init list.  dip points to the dynamic initialization,
and ipdp identifies the entity to be destroyed.  The statements are inserted at
*insert_location and *insert_location is updated.
*/
{
  an_expr_node_ptr  entity_node;
  a_variable_ptr    variable;
  a_source_position saved_error_position;

  saved_error_position = error_position;
  variable = dip->variable;
  if (variable != NULL) {
    /* Track the source position for internal errors. */
    error_position = variable->source_corresp.decl_position;
#if CHECKING
    if (variable != ipdp->variable) {
      internal_error("lower_destructor_dynamic_init: variable mismatch");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Make an expression for the object to be destroyed. */
  entity_node = make_init_entity_node(ipdp);
  /* Generate code for the destructor call. */
  if (ipdp->whole_array) {
    /* Destruction of whole array. */
    add_array_destructor_call(dip, entity_node, ipdp->array_element_count,
                              insert_location);
  } else {
    /* Destruction of simple entity (non-array). */
    add_destructor_call(dip, entity_node, /*have_complete_object=*/TRUE,
                        insert_location);
  }  /* if */
  error_position = saved_error_position;
}  /* lower_destructor_dynamic_init */


static void lower_variable_list(a_variable_ptr variable_list)
/*
Do IL lowering of the indicated list of variables and everything under it.
*/
{
  a_variable_ptr variable;

  for (variable = variable_list; variable != NULL; variable = variable->next) {
    lower_variable(variable);
  }  /* for */
}  /* lower_variable_list */


static void lower_variable(a_variable_ptr variable)
/*
Do IL lowering of the indicated variable and everything under it.
*/
{
  if (!visited_yet(variable)) {
    mark_as_visited(variable);
    lower_source_correspondence(&variable->source_corresp);
    lower_os_type(variable->type);
    if (variable->address_taken &&
        variable->storage_class == (a_storage_class)sc_register) {
      /* In C++, one can take the address of a register variable.  In C,
         one is not allowed to, so change "register" to "auto". */
      variable->storage_class = (a_storage_class)sc_auto;
    }  /* if */
    switch (variable->init_kind) {
      case initk_none:
        break;
      case initk_static:
        lower_constant(variable->initializer.constant);
        break;
      case initk_dynamic:
        /* The dynamic init entry is either pointed to from a stmk_init
           entry or appears on the file-scope dynamic inits list.  Handle
           it when seen in one of those places. */
        break;
#if CHECKING
      default:
        internal_error("lower_variable: bad kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_variable */


static void lower_field_list(a_field_ptr field_list)
/*
Do IL lowering of the indicated list of fields and everything under it.
*/
{
  a_field_ptr field;

  for (field = field_list; field != NULL; field = field->next) {
    lower_field(field);
  }  /* for */
}  /* lower_field_list */


static void lower_field(a_field_ptr field)
/*
Do IL lowering of the indicated field and everything under it.
*/
{
  if (!visited_yet(field)) {
    mark_as_visited(field);
    lower_source_correspondence(&field->source_corresp);
    lower_os_type(field->type);
  }  /* if */
}  /* lower_field */


static void lower_routine_list(a_routine_ptr routine_list)
/*
Do IL lowering of the indicated list of routines and everything under it.
*/
{
  a_routine_ptr routine;

  for (routine = routine_list; routine != NULL; routine = routine->next) {
    lower_routine(routine);
  }  /* for */
}  /* lower_routine_list */


static void lower_constructor_routine(a_routine_ptr routine)
/*
Do IL lowering of a constructor routine.  This is lowering of the routine
entry and the type, not of the routine body if any
(see lower_constructor_code).
*/
{
  a_type_ptr       class_type, subobject_type;
  a_param_type_ptr first_param, added_param, prev_param;
  a_base_class_ptr bcp;

  /* Get the "this" parameter entry.  The routine type has already been
     lowered, so it's the first on the list. */
  first_param = routine->type->variant.routine.extra_info->param_type_list;
  /* Get the class type from the "this" parameter type. */
  class_type = type_pointed_to(first_param->type);
  class_type = skip_typerefs(class_type);
  prelower_class_type(class_type);
  /* Add a parameter for each virtual base class.  See the ARM, top of
     p. 296.  add_constructor_params does the similar processing for param
     variables. */
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    prev_param = first_param;
    for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Use the type of the base class when used as a subobject. */
        subobject_type = bcp->type->variant.class_struct_union.extra_info->
                                                             type_as_subobject;
        added_param = alloc_param_type(make_pointer_type(subobject_type));
        /* Note that the original parameter entries have already been lowered,
           so it is not necessary to clear il_lowering_flag to ensure that the
           whole list will be visited. */
        added_param->next = prev_param->next;
        prev_param->next = added_param;
        prev_param = added_param;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* lower_constructor_routine */


static void lower_destructor_routine(a_routine_ptr routine)
/*
Do IL lowering of a destructor routine.  This is lowering of the routine
entry and the type, not of the routine body if any
(see lower_destructor_code).
*/
{
  a_type_ptr       class_type;
  a_param_type_ptr first_param, added_param;

  /* Get the "this" parameter entry.  The routine type has already been
     lowered, so it's the first on the list. */
  first_param = routine->type->variant.routine.extra_info->param_type_list;
  /* Get the class type from the "this" parameter type. */
  class_type = type_pointed_to(first_param->type);
  class_type = skip_typerefs(class_type);
  prelower_class_type(class_type);
  /* Add an int parameter that will indicate whether or not we have a
     complete object and whether or not the storage should be freed.
     add_destructor_params does the similar processing for param variables. */
  added_param = alloc_param_type(integer_type((an_integer_kind)ik_int));
  /* Note that the original parameter entries have already been lowered,
     so it is not necessary to set il_lowering_flag to ensure that the
     whole list will be visited. */
  added_param->next = first_param->next;
  first_param->next = added_param;
}  /* lower_destructor_routine */


static void lower_routine(a_routine_ptr routine)
/*
Do IL lowering of the indicated routine and everything under it.
*/
{
  if (!visited_yet(routine)) {
    mark_as_visited(routine);
    lower_source_correspondence(&routine->source_corresp);
    /* "lower_os_type" not needed; the routine and the type must both be
       in the file scope. */
    lower_type(routine->type);
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
      /* Special additional processing for constructors. */
      lower_constructor_routine(routine);
    } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
      /* Special additional processing for destructors. */
      lower_destructor_routine(routine);
    }  /* if */
    /* Clear the befriending classes field to make the routine entry legal
       C IL. */
    routine->befriending_classes = NULL;
  }  /* if */
}  /* lower_routine */


static void lower_label_list(a_label_ptr label_list)
/*
Do IL lowering of the indicated list of labels and everything under it.
*/
{
  a_label_ptr label;

  for (label = label_list; label != NULL; label = label->next) {
    lower_label(label);
  }  /* for */
}  /* lower_label_list */


static void lower_label(a_label_ptr label)
/*
Do IL lowering of the indicated label and everything under it.
*/
{
  if (!visited_yet(label)) {
    mark_as_visited(label);
    lower_source_correspondence(&label->source_corresp);
    /* label->variant.exec_stmt need not be processed since it will be
       found in the normal code traversal. */
  }  /* if */
}  /* lower_label */


static void lower_asm_entry_list(an_asm_entry_ptr asm_entry_list)
/*
Do IL lowering of the indicated list of asm entries and everything under it.
*/
{
  an_asm_entry_ptr asm_entry;

  for (asm_entry = asm_entry_list;
       asm_entry != NULL;
       asm_entry = asm_entry->next) {
    lower_asm_entry(asm_entry);
  }  /* for */
}  /* lower_asm_entry_list */


static void lower_asm_entry(an_asm_entry_ptr asm_entry)
/*
Do IL lowering of the indicated asm entry and everything under it.
*/
{
  if (!visited_yet(asm_entry)) {
    mark_as_visited(asm_entry);
    lower_source_correspondence(&asm_entry->source_corresp);
    lower_constant(asm_entry->asm_string);
  }  /* if */
}  /* lower_asm_entry */


static void lower_expr_list(an_expr_node_ptr expr_list,
                            unsigned int     is_lvalue_mask,
                            a_boolean        is_conditional_operator)
/*
Do IL lowering of the indicated list of expressions and everything under it.
is_lvalue_mask is a bit mask indicating which elements of the list are
lvalues (0x1 for first operand, 0x2 for second operand, etc.)
is_conditional_operator is TRUE if the expressions are operands of
a conditional operator ("?", "&&", or "||").
*/
{
  an_expr_node_ptr expr;

  for (expr = expr_list; expr != NULL; expr = expr->next) {
    /* On operands after the first operand of a conditional operator,
       increment the count of conditional operands. */
    if (is_conditional_operator && expr != expr_list) {
      num_conditional_exprs_inside_of++;
    }  /* if */
    /* Lower the expression on the list. */
    lower_expr(expr, (a_boolean)(is_lvalue_mask & 1));
    /* If the count of conditional operands was incremented above, restore it
       to what it was. */
    if (is_conditional_operator && expr != expr_list) {
      num_conditional_exprs_inside_of--;
    }  /* if */
    /* Move to the next bit in the lvalue mask. */
    is_lvalue_mask >>= 1;
  }  /* for */
}  /* lower_expr_list */


static void lower_arg_expr_list(an_expr_node_ptr expr_list,
                                a_type_ptr       called_rout_type)
/*
Do IL lowering of the indicated list of expressions and everything under it.
The expressions are the argument list for a call.  The type of the routine
being called is called_rout_type.  Note that if the routine requires
control arguments like a "this" pointer, such arguments are *not* in
expr_list.
*/
{
  an_expr_node_ptr              expr;
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param;

  called_rout_type = skip_typerefs(called_rout_type);
  rtsp = called_rout_type->variant.routine.extra_info;
  /* Track the current parameter type as we go through the list. */
  /* Note that we do not test rtsp->prototyped because it may have been
     cleared by lowering when MAKE_ALL_FUNCTIONS_UNPROTOTYPED is TRUE. */
  param = (!rtsp->old_style_params_scanned) ? rtsp->param_type_list : NULL;
  for (expr = expr_list; expr != NULL; expr = expr->next) {
    lower_expr(expr, FALSE);
    if (param != NULL) {
      /* Prototyped parameter. */
#if MAKE_ALL_FUNCTIONS_UNPROTOTYPED
      /* Do default argument promotions on any arguments that need it,
         because they were generated for a call to a prototyped function, but
         we're changing all functions to unprototyped (for cfront
         compatibility). */
      do_default_arg_promotions_on_node(expr);
#endif /* MAKE_ALL_FUNCTIONS_UNPROTOTYPED */
      param = param->next;
    } else {
      /* Unprototyped parameter: old-style function or ellipsis. */
      /* Widen pointers-to-data-members that have been turned into integers
         and are passed to an old-style function or ellipsis. */
      if (is_or_was_ptr_to_data_member_type(expr->type)) {
        /* A pointer to data member -- widen if necessary. */
        do_ptr_to_data_member_arg_promotion_on_node(expr);
      }  /* if */
    }  /* if */
  }  /* for */
}  /* lower_arg_expr_list */


static void add_null_preservation_code(an_expr_node_ptr expr,
                                       an_expr_node_ptr orig_source_node)
/*
Add the equivalent of
   (orig_source_node != NULL) ? expr : NULL
on top of expr.  The node pointed to by expr is overwritten, so the
pointer to the overall expression remains the same.
*/
{
  a_constant       null_constant;
  an_expr_node_ptr null_constant_node1, null_constant_node2, compare_node;
  an_expr_node_ptr source_node;

  /* Make a copy of the expr node; it will be reused below as the
     conditional operator node. */
  source_node = copy_node(expr);
  /* Make a NULL pointer constant of the right type. */
  make_zero_of_proper_type(orig_source_node->type, &null_constant);
  /* Make two expression nodes pointing to two NULL constants with
     possibly different types. */
  null_constant_node1 = alloc_node_for_constant(&null_constant);
  null_constant.type = source_node->type;
  null_constant_node2 = alloc_node_for_constant(&null_constant);
  /* Make a node comparing the original source node against NULL. */
  orig_source_node->next = null_constant_node1;
  compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                    integer_type((an_integer_kind)ik_int),
                                    orig_source_node);
  /* Make a conditional operator node out of the original node. */
  compare_node->next = source_node;
  source_node->next = null_constant_node2;
  set_node_operator(expr, (an_expr_operator_kind)eok_question,
                    source_node->type, compare_node);
}  /* add_null_preservation_code */


static an_expr_node_ptr add_decr_code_to_pointer_node(
                                                  an_expr_node_ptr source_node,
                                                  a_targ_size_t    byte_offset)
/*
Generate expression nodes to subtract byte_offset from the pointer represented
by source_node.  Return a pointer to the new expression.  The expression will
have type "char *" and the caller is responsible for the cast back to the
proper type if necessary.  If the offset is zero, return the original node.
*/
{
  an_expr_node_ptr cast_to_char_star_node, offset_constant_node;

  if (byte_offset != 0) {
    /* Cast the original node to char * to avoid scaling problems. */
    cast_to_char_star_node = add_cast_to_char_star(source_node);
    /* Make the node for the constant. */
    offset_constant_node = node_for_integer_constant((long)byte_offset,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
    cast_to_char_star_node->next = offset_constant_node;
    /* Make the node for the pointer subtraction. */
    source_node = make_operator_node((an_expr_operator_kind)eok_psubtract,
                                     cast_to_char_star_node->type,
                                     cast_to_char_star_node);
  }  /* if */
  return source_node;
}  /* add_decr_code_to_pointer_node */


static void related_class_cast_step(
                               an_expr_node_ptr node,
                               a_boolean        is_lvalue,
                               a_boolean        any_nonzero_offset,
                               a_type_ptr       virtual_step_class,
                               an_expr_node_ptr *null_preservation_source_node,
                               a_base_class_ptr *base_class_for_virtual_step,
                               an_expr_node_ptr *result_node,
                               a_targ_size_t    *derived_class_cast_offset)
/*
Do one step in the lowering of a base or derived class cast.  node points
to the cast.  On return, *result_node has been set to the lowered form;
however, for derived casts, the pointer subtraction must still be generated
by the caller (*derived_class_cast_offset indicates the amount to subtract),
and for base class casts, a final cast to the proper type may still be
required (for cases where the base class type is not the same as its
type-as-subobject).  If code to preserve NULL pointers is needed, it is
started, and *null_preservation_source_node is set to the original node
to be tested when the null-preservation code is completed.
*null_preservation_source_node == NULL means that no NULL-preservation code
is required.  is_lvalue is TRUE if the node is being used as an lvalue.
This routine descends recursively through a sequence of base-class or
derived-class casts so that the entire sequence can be treated as one
operation.  This allows optimization of the code to preserve NULL pointer
values: we may be able to determine that it is not needed at all by
examining the fundamental source node or the offsets in the base class
casts (any_nonzero_offset is maintained going down to aid that determination).
If we cannot suppress the NULL-preservation code, we can at least put it
just once around the whole sequence instead of around each cast in the
sequence.  The recursive technique also allows optimization of casts
to virtual base classes of complete objects.  When a virtual step is
found while descending, virtual_step_class is set to the virtual base class
type.  Then, when the bottom is reached, we can determine whether or not
we have a complete object, and if so, return *base_class_for_virtual_step
on the ascent, suppressing the casting steps preceding the virtual step
on the ascent.  The recursive descent also has the advantage that it
avoids the need to look up the base class entry for each step of the cast
more than once.
*/
{
  an_expr_operator_kind op;
  a_boolean             derived, handle_virtual_at_this_level;
  an_expr_node_ptr      source_node;
  a_type_ptr            source_class, dest_class;
  a_base_class_ptr      bcp, virt_bcp;
  a_boolean             need_null_preservation_code;

  *base_class_for_virtual_step = NULL;
  op = node->variant.operation.kind;
  derived = (op == (an_expr_operator_kind)eok_derived_class_cast);
  /* Determine the source and destination class types and the relationship
     between them. */
  source_node = node->variant.operation.operands;
#if CHECKING
  if (!is_ptr_or_ref_type(source_node->type)) {
    internal_error("related_class_cast_step: source not ptr");
  }  /* if */
  if (!is_ptr_or_ref_type(node->type)) {
    internal_error("related_class_cast_step: dest not ptr");
  }  /* if */
#endif /* CHECKING */
  source_class = type_pointed_to(source_node->type);
  source_class = skip_typerefs(source_class);
  dest_class = type_pointed_to(node->type);
  dest_class = skip_typerefs(dest_class);
#if CHECKING
  if (!is_immediate_class_type(source_class)) {
    internal_error("related_class_cast_step: source not class");
  }  /* if */
  if (!is_immediate_class_type(dest_class)) {
    internal_error("related_class_cast_step: dest not class");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(source_class);
  prelower_class_type(dest_class);
  /* Find the base class entry that relates the source_class to the
     dest_class. */
  if (!derived) {
    bcp = find_direct_base_class_of(source_class, dest_class);
  } else {
    bcp = find_direct_base_class_of(dest_class, source_class);
  }  /* if */
  /* If this step is to a virtual base class, and no previous step was a
     step to a virtual base class, pass the virtual base class type down
     in the recursive processing to let the bottom-most call find out whether
     we have a complete object and can optimize this cast. */
  handle_virtual_at_this_level = FALSE;
  if (virtual_step_class != NULL) {
    /* A previous step (higher up the tree) was a step to a virtual base
       class, so we do not test here. */
  } else if (bcp->is_virtual) {
    /* No previous step was virtual and this one is, so remember it as we
       continue down the tree. */
    handle_virtual_at_this_level = TRUE;
    virtual_step_class = bcp->type;
  } else {
    /* No previous step was virtual and this one is not either.  Remember if
       the offset is non-zero (if all offsets are zero, we do not need
       NULL-preservation code). */
    if (bcp->offset != 0) any_nonzero_offset = TRUE;
  }  /* if */
  /* See if there is another cast below this one.  If so, make a recursive
     call to get down to the bottom of the sequence of casts. */
  if (is_operation_node(source_node) &&
      source_node->variant.operation.kind == op) {
    related_class_cast_step(source_node,
                            is_lvalue,
                            any_nonzero_offset,
                            virtual_step_class,
                            null_preservation_source_node,
                            base_class_for_virtual_step,
                            &source_node,
                            derived_class_cast_offset);
  } else {
    /* The node below this one is not another cast, so we have reached the
       bottom of the sequence of casts. */
    /* Lower the source expression. */
    lower_expr(source_node, is_lvalue);
    /* The offsets for derived class casts are summed on the way back up. */
    *derived_class_cast_offset = 0;
    *base_class_for_virtual_step = NULL;
    if (virtual_step_class != NULL) {
      /* There was a virtual step somewhere in the sequence of casts.  See
         if we have a complete object, because if so, we can cast directly
         to the virtual base class type. */
#if CHECKING
      /* Cannot cast up from a virtual base class. */
      if (derived) {
        internal_error("related_class_cast_step: derived cast is virtual");
      }  /* if */
#endif /* CHECKING */
      if (node_complete_object_type(source_node) == source_class) {
        /* We have a complete object, so it is possible to go directly to the
           virtual base class without using a pointer indirection. */
        /* Find the base class entry that relates the complete object class
           and the virtual base class.  We cannot use find_base_class_of
           because we specifically want an entry with is_virtual TRUE --
           there might be others if the base class appears as both a virtual
           and a non-virtual base class. */
        virt_bcp = find_virtual_base_class_of(source_class,
                                              virtual_step_class);
        /* virt_bcp is now the base class entry that gets us from the
           original type (source_class) to the desired virtual base class
           (virtual_step_class).  Pass it back up to the invocation that
           will deal with the virtual step. */
        *base_class_for_virtual_step = virt_bcp;
        /* Keep track of whether or not the cast involves a non-zero offset. */
        if (virt_bcp->offset != 0) any_nonzero_offset = TRUE;
      }  /* if */
    }  /* if */
    /* See if we need code to preserve NULL values.  NULL is supposed to
       pass through a downward/upward cast unaltered.  We can suppress the
       special code if the expression being cast can be assumed to be
       non-NULL or if the transformation is a do-nothing transformation
       (the offset is zero). */
    if (is_lvalue) {
      /* The result of this cast is being used as an lvalue, so it must be
         non-NULL. */
      need_null_preservation_code = FALSE;
    } else if (cannot_be_null(source_node)) {
      /* The source address is known to not be NULL. */
      need_null_preservation_code = FALSE;
    } else if (virtual_step_class && *base_class_for_virtual_step == NULL) {
      /* There is a virtual step in the casts and it cannot be optimized.
         Therefore a pointer indirection will be required in the sequence
         and the NULL-preservation code is required. */
      need_null_preservation_code = TRUE;
    } else if (any_nonzero_offset) {
      /* There is a nonzero offset in at least one of the casts, so the
         sequence of casts modifies the address.  Therefore the NULL-
         preservation code is required.  Note that if the first step
         is an optimized virtual step, its offset and the offsets of all
         the non-virtual steps following that are reflected in
         any_nonzero_offset. */
      need_null_preservation_code = TRUE;
    } else {
      /* The casts add up to an identity transformation. */
      need_null_preservation_code = FALSE;
    }  /* if */
    *null_preservation_source_node = NULL;
    if (need_null_preservation_code) {
      /* Start the code to preserve a NULL value through the casts.
         This involves making a reusable copy of the current source node
         and passing it up to lower_related_class_cast which will
         call add_null_preservation_code. */
      *null_preservation_source_node = source_node;
      source_node = make_reusable_copy(source_node);
    }  /* if */
  }  /* if */
  /* Here, source_node points to the processed version of the operand
     of node.  Now generate the code for this step in the cast sequence. */
  /* Refetch the source class type in case what's been returned from lower
     levels has the class type-as-subobject instead of the class type. */
  source_class = type_pointed_to(source_node->type);
  source_class = skip_typerefs(source_class);
  if (derived) {
    /* The offsets in a sequence of derived casts are added together.
       lower_related_class_cast then puts out one pointer subtraction for
       the total.  The addition here is guaranteed to not overflow because
       it must be within one object. */
    *derived_class_cast_offset += bcp->offset;
  } else if (*base_class_for_virtual_step != NULL) {
    /* The lowest level discovered that optimization of a virtual step is
       possible.  If the present level is the level of the applicable
       step, process the cast now.  Otherwise, do nothing, because the
       present step is elided by the virtual step optimization. */
    if (!handle_virtual_at_this_level) {
      /* Do nothing; the present cast is elided. */
    } else {
      /* Make a field selection that selects the virtual base class directly
         from the complete object (i.e., without using a virtual base class
         pointer). */
      source_node = make_cobj_vbase_class_lvalue(source_node,
                                                 *base_class_for_virtual_step);
      /* Now that we've dealt with this optimization, put things back to normal
         for levels above this one. */
      *base_class_for_virtual_step = NULL;
    }  /* if */
  } else {
    /* Non-virtual step or virtual step that cannot be optimized. */
    source_node = make_base_class_lvalue(source_node, bcp);
  }  /* if */
  *result_node = source_node;
}  /* related_class_cast_step */


static void lower_related_class_cast(an_expr_node_ptr node,
                                     a_boolean        is_lvalue)
/*
Rewrite a cast from a class to a base class or from a base class to
a derived class.  The result of the cast is being used as an lvalue if
is_lvalue is TRUE.
*/
{
  an_expr_node_ptr null_preservation_source_node;
  a_base_class_ptr base_class_for_virtual_step;
  an_expr_node_ptr result_node;
  a_targ_size_t    derived_class_cast_offset;

  /* Use a recursive routine to pick up a sequence of base-class or
     derived-class casts and generate the code for it.  Using a recursive
     routine allows us to (a) find the whole sequence; (b) put
     NULL-preservation code around the whole sequence (instead of around
     each step); (c) optimize casts to virtual base classes of complete
     objects; (d) only look up the base class entry corresponding to
     each step once; and (e) add all the offsets for derived-class
     casts into a single offset. */
  related_class_cast_step(node,
                          is_lvalue,
                          /*any_nonzero_offset=*/FALSE,
                          /*virtual_step_class=*/(a_type_ptr)NULL,
                          &null_preservation_source_node,
                          &base_class_for_virtual_step,
                          &result_node,
                          &derived_class_cast_offset);
  if (node->variant.operation.kind ==
                               (an_expr_operator_kind)eok_derived_class_cast) {
    /* A cast from a base class to a derived class.  The call above collected
       the offsets and added them together.  The total is an amount to be
       subtracted from the pointer to the base class to get a pointer to the
       derived class.  Generate a pointer subtraction to actually adjust
       the pointer.  The cast back from "char *" is yet to be done below. */
    result_node = add_decr_code_to_pointer_node(result_node,
                                                derived_class_cast_offset);
  }  /* if */
  /* Wrap the expression in NULL-preservation code if necessary. */
  if (null_preservation_source_node != NULL) {
    /* Add the equivalent of
         (temp != NULL) ? expr : NULL
       on top of the expression we have. */
    add_null_preservation_code(result_node, null_preservation_source_node);
  }  /* if */
  /* The result of all the processing must overwrite the original node.
     If a final cast is required (as for the derived-class cast case or
     selection of a base class whose subobject type is different than
     its normal type), change the original node into a cast.  Otherwise,
     overwrite the original node with the result_node, and discard the
     result_node. */
  if (types_are_compatible(node->type, result_node->type)) {
    /* No final cast is needed, so overwrite the original node with the
       contents of the result_node.  The storage used for result_node is
       just lost. */
#if 0
    /* There's some reason for concern about that, since this is the most
       common case. */
#endif
    overwrite_node(node, result_node);
  } else {
    /* A final cast is needed, so change the original node into the proper
       cast. */
    change_to_cast(node, result_node, node->type);
  }  /* if */
}  /* lower_related_class_cast */


static void compute_pm_cast_offset(an_expr_node_ptr node,
                                   an_expr_node_ptr *underlying_node,
                                   a_targ_ptrdiff_t *offset)
/*
Compute the offset to be used to perform a related class cast on a
pointer to member.  node is an expression node for a pointer-to-member
related class cast.  Find the bottom of the chain of similar casts
under node and return a pointer to the non-cast node underlying that
chain in *underlying_node.  Return the offset from that underlying node's
class to the class of node in *offset.
*/
{
  an_expr_node_ptr operand = node->variant.operation.operands;
  a_base_class_ptr bcp;
  a_type_ptr       source_class, dest_class;

  /* See if the node under this one is a cast of the same kind.  If so,
     it can be combined with this one. */
  if (is_operation_node(operand) &&
      node->variant.operation.kind == operand->variant.operation.kind) {
    /* Use recursion to get to the bottom of the chain of similar
       casts.  Determine the type underlying the chain and compute the
       offset from there to here. */
    compute_pm_cast_offset(operand, underlying_node, offset);
  } else {
    /* The node below this one is not another cast, so we have reached the
       bottom of the sequence of casts. */
    *underlying_node = operand;
    /* Start accumulating the offset from here. */
    *offset = 0;
  }  /* if */
  source_class = pm_class_type_possibly_lowered(operand->type);
  dest_class = pm_class_type_possibly_lowered(node->type);
  prelower_class_type(source_class);
  prelower_class_type(dest_class);
  if (node->variant.operation.kind ==
                               (an_expr_operator_kind)eok_pm_base_class_cast) {
    /* Casting from a derived class to a base class. */
    bcp = find_direct_base_class_of(source_class, dest_class);
    if (bcp->is_virtual) {
      /* For a virtual base class skip, assume that we have a whole object
         and compute the offset from there.  The C++ language should probably
         prohibit casts like this, because they cannot be implemented with a
         simple offset. */
      source_class = pm_class_type_possibly_lowered((*underlying_node)->type);
      bcp = find_virtual_base_class_of(source_class, dest_class);
      *offset = -bcp->offset;
    } else {
      /* Non-virtual base class.  Subtract the offset from the running
         total. */
      *offset -= bcp->offset;
    }  /* if */
  } else {
    /* Casting from a base class to a derived class. */
    bcp = find_direct_base_class_of(dest_class, source_class);
#if CHECKING
    if (bcp->is_virtual) {
      internal_error("compute_pm_cast_offset: derived class is virtual");
    }  /* if */
#endif /* CHECKING */
    /* Add the offset to the running total. */
    *offset += bcp->offset;
  }  /* if */
}  /* compute_pm_cast_offset */


static void lower_pm_related_class_cast(an_expr_node_ptr node,
                                        a_boolean        is_lvalue)
/*
Rewrite a cast from a pointer to a member of a class to a pointer to a member
of a base or derived class of that class.  The result of the cast is being
used as an lvalue if is_lvalue is TRUE.
*/
{
  an_expr_node_ptr source_node, question_node, compare_node, plus_node;
  an_expr_node_ptr offset_node, temp_node, select_i_node;
  an_expr_node_ptr select_d_node, incr_node, assign_node, comma_node;
  a_constant       offset_constant;
  a_targ_ptrdiff_t offset;
  a_variable_ptr   temp_var;

  /* Use recursion to find a chain of similar casts and compute the overall
     class offset for the chain. */
  compute_pm_cast_offset(node, &source_node, &offset);
  /* Lower the underlying subtree. */
  lower_expr(source_node, is_lvalue);
  if (offset == 0) {
    /* When the offset is zero no cast is needed.  Overwrite the original
       node with the underlying node. */
    overwrite_node(node, source_node);
  } else {
    /* The offset is non-zero, so some work is needed. */
    /* Make a node for the offset constant. */
    set_delta_constant(offset, &offset_constant);
    offset_node = alloc_node_for_constant(&offset_constant);
    if (is_or_was_ptr_to_member_function_type(node->type)) {
      /* Pointer to member function.  Change the node to
           (temp = pmf, (temp.i != 0) ? temp.d += offset : 0, temp)
      */
      /* Create the temporary. */
      temp_var = make_temporary(make_mptr_type());
      /* Make "temp.i != 0". */
      temp_node = var_lvalue_expr(temp_var);
      select_i_node = field_rvalue_selection_expr(temp_node, mptr_i_field);
      select_i_node->next = node_for_integer_constant(0L,
                                         TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
      compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                        integer_type((an_integer_kind)ik_int),
                                        select_i_node);
      /* Make "temp.d += offset". */
      temp_node = var_lvalue_expr(temp_var);
      select_d_node = field_lvalue_selection_expr(temp_node, mptr_d_field);
      select_d_node->next = offset_node;
      incr_node = make_operator_node((an_expr_operator_kind)eok_iadd_assign,
                                     offset_node->type, select_d_node);
      /* Make "(temp.i != 0) ? temp.d += offset : 0". */
      compare_node->next = incr_node;
      incr_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
      question_node = make_operator_node((an_expr_operator_kind)eok_question,
                                         incr_node->type, compare_node);
      /* Make "temp = pmf". */
      temp_node = var_lvalue_expr(temp_var);
      temp_node->next = source_node;
      assign_node = make_operator_node((an_expr_operator_kind)eok_sassign,
                                       temp_node->type, temp_node);
      /* Make "(temp = pmf, (temp.i != 0) ? temp.d += offset : 0)". */
      assign_node->next = question_node;
      comma_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                       question_node->type, assign_node);
      /* Overwrite the original node with a "," operator to make the
         full expression. */
      comma_node->next = var_rvalue_expr(temp_var);
      set_node_operator(node, (an_expr_operator_kind)eok_comma,
                        temp_var->type, comma_node);
    } else {
      /* Pointer to data member.  Change the node to
           (pdm != 0) ? pdm + offset : 0
      */
      /* Make "pdm != 0". */
      source_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
      compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                        integer_type((an_integer_kind)ik_int),
                                        source_node);
      /* Make "pdm + offset". */
      source_node = make_reusable_copy(source_node);
      source_node->next = offset_node;
      plus_node = make_operator_node((an_expr_operator_kind)eok_iadd,
                                     source_node->type, source_node);
      /* Make the "?" operation by overwriting the original node. */
      compare_node->next = plus_node;
      plus_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
      set_node_operator(node, (an_expr_operator_kind)eok_question,
                        plus_node->type, compare_node);
    }  /* if */
  }  /* if */
}  /* lower_pm_related_class_cast */


static a_routine_ptr routine_from_node(an_expr_node_ptr node)
/*
node is an expression node that is the address of a specific routine.
Extract and return a pointer to the routine entry.
*/
{
  a_routine_ptr routine;

#if CHECKING
  if (node->kind != (an_expr_node_kind)enk_routine_address) {
    internal_error("routine_from_node: func node not rout addr");
  }  /* if */
#endif /* CHECKING */
  routine = node->variant.routine;
  return routine;
}  /* routine_from_node */


static void add_implied_args_to_call(an_expr_node_ptr call_expr,
                                     a_routine_ptr    rout)
/*
call_expr is an expression that calls the routine rout.  If the call
requires implied arguments (e.g., for a constructor or destructor), add
them.  The call has already been lowered.
*/
{
  an_expr_node_ptr implied_arg_list, end_implied_arg_list;
  an_expr_node_ptr func_addr_arg, this_arg;

  implied_arg_list = NULL;
  if (rout->special_kind == (a_special_function_kind)sfk_constructor) {
    /* Constructor. */
    make_ctor_implied_arg_list(rout, &implied_arg_list,
                               &end_implied_arg_list);
  } else if (rout->special_kind == (a_special_function_kind)sfk_destructor) {
    /* Destructor. */
    make_dtor_implied_arg_list(rout, /*have_complete_object=*/TRUE,
                               &implied_arg_list);
    end_implied_arg_list = implied_arg_list;
  }  /* if */
  if (implied_arg_list != NULL) {
    /* The implied arguments go after the "this" argument. */
    func_addr_arg = call_expr->variant.operation.operands;
    this_arg = func_addr_arg->next;
    end_implied_arg_list->next = this_arg->next;
    this_arg->next = implied_arg_list;
  }  /* if */
}  /* add_implied_args_to_call */


static an_expr_node_ptr make_vtbl_entry_node(an_expr_node_ptr func_node,
                                             an_expr_node_ptr object_node)
/*
Create an expression that computes the address of the virtual table entry
for the function whose address is given by func_node for the object whose
address is given by object_node.  Return a pointer to the expression
created.
*/
{
  a_routine_ptr             routine_ptr;
  an_expr_node_ptr          select_vptr_node, vptr_node, vtbl_entry_node;
  an_expr_node_ptr          index_node;
  a_constant_ptr            index_con;
  a_virtual_function_number index;

  /* Find the routine entry. */
  routine_ptr = routine_from_node(func_node);
  /* Make an expression tree for the value of the virtual function table
     pointer. */
  select_vptr_node = make_vptr_field_lvalue(object_node);
  vptr_node = add_indirection_to_node(select_vptr_node);
  /* Add the index for the function to the base address of the virtual
     function to get the address of the applicable entry of the table. */
  index = routine_ptr->virtual_function_number;
  index_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_integer_constant(index_con, (long)index, (an_integer_kind)ik_int);
  index_node = make_node_for_il_constant(index_con);
  vtbl_entry_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                       vptr_node->type, vptr_node);
  vptr_node->next = index_node;
  return vtbl_entry_node;
}  /* make_vtbl_entry_node */


static void lower_virtual_function_call(an_expr_node_ptr expr)
/*
Do IL Lowering of a virtual function call.  The operands of the expression
have already been lowered.
*/
{
  an_expr_node_ptr func_node, object_node, additional_args;
  an_expr_node_ptr cast_node, func_select_node, d_value_node;
  an_expr_node_ptr vtbl_entry_node, vtbl_temp_node;
  an_expr_node_ptr assign_node, padd_node;
  a_variable_ptr   vtbl_temp_var;

  /* The original tree has an eok_virtual_call node with operands as follows:
       (1) an enk_routine_address node for the virtual function.
       (2) a node for the object pointer.
       (3..n) optional additional arguments.
     Or, in C notation,
       eok_virtual_call(func, object, additional_args ...)
  */
  func_node = expr->variant.operation.operands;
  object_node = func_node->next;
  additional_args = object_node->next;
  func_node->next = NULL;
  object_node->next = NULL;
  /* The rewritten form is as follows:
       ((vtbl_temp = (object->__vptr)+index),
        eok_call(vtbl_temp->f, object+vtbl_temp->d, additional_args ...))
     index is the virtual function table index for the virtual function.
     If "object" is not a reusable expression, the first occurrence of
     "object" above is replaced by "(object_temp = object)", and the
     second by "object_temp". */
  /* Make a node for the address of the virtual table entry for the
     function. */
  vtbl_entry_node = make_vtbl_entry_node(func_node, object_node);
  /* Make the vtbl_temp temporary and an lvalue for it, and assign the
     virtual function table entry address to it. */
  vtbl_temp_var = make_temporary(vtbl_entry_node->type);
  vtbl_temp_node = var_lvalue_expr(vtbl_temp_var);
  assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                   vtbl_entry_node->type, vtbl_temp_node);
  vtbl_temp_node->next = vtbl_entry_node;
  /* Make an expression that extracts the "f" (function pointer) from the
     virtual table entry and casts it to the right function pointer type. */
  vtbl_temp_node = var_rvalue_expr(vtbl_temp_var);
  func_select_node = field_rvalue_selection_expr(vtbl_temp_node, mptr_f_field);
  func_select_node = add_cast_if_necessary(func_select_node, func_node->type);
  /* Make an expression that selects the "d" (delta) from the virtual
     table entry and adds it to the object pointer. */
  vtbl_temp_node = var_rvalue_expr(vtbl_temp_var);
  d_value_node = field_rvalue_selection_expr(vtbl_temp_node, mptr_d_field);
  /* Make a reusable copy of the object address. */
  object_node = make_reusable_copy(object_node);
  /* Cast the node to "char *" to suppress scaling on the pointer addition. */
  cast_node = add_cast_to_char_star(object_node);
  /* Add the object pointer and the delta value. */
  padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                 cast_node->type, cast_node);
  cast_node->next = d_value_node;
  /* Cast the result back to the "this" parameter type. */
  object_node = add_cast(padd_node, object_node->type);
  /* Reuse the node that was originally the first operand of
     the virtual call (i.e., the enk_routine_address node) as the new
     eok_call node.  Attach the function selection node, the object node, and
     the additional arguments to the call node as arguments. */
  clear_expr_node(func_node, (an_expr_node_kind)enk_operation);
  set_node_operator(func_node, (an_expr_operator_kind)eok_call, expr->type,
                    func_select_node);
  func_select_node->next = object_node;
  object_node->next = additional_args;
  /* Reuse the original eok_virtual_call node as a comma operator node and
     attach the vtbl_temp assignment and the eok_call nodes under it as
     operands. */
  set_node_operator(expr, (an_expr_operator_kind)eok_comma, expr->type,
                    assign_node);
  assign_node->next = func_node;
}  /* lower_virtual_function_call */


static void lower_virtual_function_ptr(an_expr_node_ptr expr)
/*
Do IL Lowering of an eok_virtual_function_ptr node, which is used to
get the address of a virtual function when a bound function is cast to
a normal function pointer (an anachronism).  The operands of the expression
have already been lowered.
*/
{
  an_expr_node_ptr func_node, object_node;
  an_expr_node_ptr func_select_node;
  an_expr_node_ptr vtbl_entry_node;

  /* The original tree has an eok_virtual_function_ptr node with operands
     as follows:
       (1) an enk_routine_address node for the virtual function.
       (2) a node for the object pointer.
  */
  func_node = expr->variant.operation.operands;
  object_node = func_node->next;
  func_node->next = NULL;
  /* The rewritten form is as follows:
       (object->__vptr)+index)->f
     index is the virtual function table index for the virtual function.
  */
  /* Make a node for the address of the virtual table entry for the
     function, i.e., "(object->__vptr)+index". */
  vtbl_entry_node = make_vtbl_entry_node(func_node, object_node);
  /* Make an expression that extracts the "f" (function pointer) from the
     virtual table entry, as an lvalue. */
  func_select_node = field_lvalue_selection_expr(vtbl_entry_node,
                                                 mptr_f_field);
  /* Convert the original node into an indirection node that fetches the
     value in the "f" field. */
  set_node_operator(expr, (an_expr_operator_kind)eok_indirect, expr->type,
                    func_select_node);
}  /* lower_virtual_function_ptr */


static void lower_pm_call(an_expr_node_ptr expr)
/*
Do IL lowering of a pointer-to-member function call.  The operands of
the expression have already been lowered.
*/
{
  an_expr_node_ptr pmf_node, object_node, additional_args, cast_object_node;
  an_expr_node_ptr this_temp_node, select_d_node, padd_node, call_node;
  an_expr_node_ptr this_temp_assign_node, vtbl_temp_assign_node;
  an_expr_node_ptr select_i_node, compare_node, select_f_node;
  an_expr_node_ptr select_f_for_cast_node, cast_node, vtbl_addr_node;
  an_expr_node_ptr offset_node, vtbl_temp_node, vtbl_d_value, vtbl_f_value;
  an_expr_node_ptr this_increment_node, comma_node, question_mark_node;
  an_expr_node_ptr func_addr_node;
  a_variable_ptr   this_temp_var, vtbl_temp_var;
  a_type_ptr       ptr_to_vtbl_entry_type, routine_type, object_type;
  a_type_ptr       class_type, ptr_routine_type;

  /* The original tree has an eok_pm_call node with operands as follows:
       (1) a node giving the value (not address) of the pointer-to-member.
       (2) a node for the object pointer.
       (3..n) optional additional arguments.
     Or, in C notation,
       eok_pm_call(pmf, object, additional_args ...)
  */
  pmf_node = expr->variant.operation.operands;
  routine_type = pm_member_type_possibly_lowered(pmf_node->type);
  ptr_routine_type = make_pointer_type(routine_type);
  class_type = pm_class_type_possibly_lowered(pmf_node->type);
  object_node = pmf_node->next;
  additional_args = object_node->next;
  pmf_node->next = NULL;
  object_node->next = NULL;
  object_type = object_node->type;
  /* The rewritten form is as follows:
       ((this_temp = (object_type *)((char *)object + pmf.d)),
                                       -- Adjust "this" pointer by delta
                                          from pointer-to-member.
        eok_call((function_type *)     -- The computed function address
                                          gets cast to the proper function
                                          type.
                 (pmf.i < 0) ?         -- Check virtual/non-virtual
                                          pointer-to-member.
                   pmf.f :             -- Function address for non-virtual
                                          case.
                                       -- Virtual function case:
                   ((vtbl_temp =       -- Virtual function table entry address
                                          is address of virtual function table
                                          plus offset in table.
                      *(__vtbl_entry **)((char *)this_temp + (short)(pmf.f)) +
                                       -- Pointer to virtual function table is
                                          found at offset pmf.f in the object.
                      pmf.i),          -- Offset into table.
                    this_temp = (object_type *)((char *)this_temp +
                                                vtbl_temp->d),
                                       -- Adjust "this" pointer to get pointer
                                          to subobject expected by the virtual
                                          function.
                    vtbl_temp->f),     -- Address of virtual function to call.
                  this_temp,           -- "this" pointer for call.
                  additional_args ...))
     If "pmf" is not a reusable expression, the first occurrence of
     "pmf" above is replaced by "(pmf_temp = pmf)", and the rest by
     "pmf_temp".  See ARM 8.1.2.c for some insight into the pointer-to-
      member-function data structure. */
  /* If the class and its base classes have no virtual functions,
     the following simpler version is used:
       ((this_temp = (object_type *)((char *)object + pmf.d)),
        eok_call((function_type *)pmf.f,
                 this_temp,
                 additional_args ...))
     This seems slightly more complicated than is needed, but it makes
     sure that if a reusable copy of pmf is needed, the temp for it
     is initialized before the call is begun.
  */
  /* Make sure __mptr (the struct that represents lowered pointers to member
     functions) has been created. */
  (void)make_mptr_type();
  /* Make the temporary variable for the "this_temp". */
  this_temp_var = make_temporary(object_type);
  /* Make "(char *)object + pmf.d". */
  select_d_node = node_to_select_field_from_rvalue(pmf_node, mptr_d_field);
  cast_object_node = add_cast_to_char_star(object_node);
  cast_object_node->next = select_d_node;
  padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                 cast_object_node->type, cast_object_node);
  /* Cast back to the object pointer type. */
  cast_node = add_cast(padd_node, object_type);
  /* Make "(this_temp = (object_type *)((char *)object + pmf.d)". */
  this_temp_node = var_lvalue_expr(this_temp_var);
  this_temp_node->next = cast_node;
  this_temp_assign_node = make_operator_node(
                                            (an_expr_operator_kind)eok_passign,
                                            object_type, this_temp_node);
  if (!class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    /* No virtual functions, so use the simpler form. */
    /* Make "pmf.f". */
    pmf_node = make_reusable_copy(pmf_node);
    select_f_node = node_to_select_field_from_rvalue(pmf_node, mptr_f_field);
    /* Add the cast to the right pointer to routine type. */
    func_addr_node = add_cast_if_necessary(select_f_node, ptr_routine_type);
  } else {
    /* Virtual functions, so use the more general form. */
    /* Make "pmf.i < 0". */
    pmf_node = make_reusable_copy(pmf_node);
    select_i_node = node_to_select_field_from_rvalue(pmf_node, mptr_i_field);
    select_i_node->next = node_for_integer_constant(0L,
                                         TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
    compare_node = make_operator_node((an_expr_operator_kind)eok_ilt,
                                      integer_type((an_integer_kind)ik_int),
                                      select_i_node);
    /* Make "pmf.f". */
    pmf_node = make_reusable_copy(pmf_node);
    select_f_node = node_to_select_field_from_rvalue(pmf_node, mptr_f_field);
    /* Make "*(__vtbl_entry **)((char *)this_temp + (short)(pmf.f))", which
       is the address of the virtual function table. */
    pmf_node = make_reusable_copy(pmf_node);
    select_f_for_cast_node = node_to_select_field_from_rvalue(pmf_node,
                                                              mptr_f_field);
    /* We're using the "f" field of __mptr as an offset to the virtual table
       pointer in the object, so it must be cast from pointer-to-function
       to an integral type. */
    cast_node = add_cast(select_f_for_cast_node,
                         integer_type(TARG_DELTA_INT_KIND));
    /* Add a cast to "char *" to avoid scaling on the pointer addition. */
    this_temp_node = add_cast_to_char_star(var_rvalue_expr(this_temp_var));
    this_temp_node->next = cast_node;
    padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                   this_temp_node->type, this_temp_node);
    /* We now have a pointer to the virtual table pointer in the object.
       Cast it to a pointer to a pointer and indirect to get the value of 
       the pointer to the virtual function table. */
    ptr_to_vtbl_entry_type = make_pointer_type(make_mptr_type());
    cast_node = add_cast(padd_node, make_pointer_type(ptr_to_vtbl_entry_type));
    vtbl_addr_node = add_indirection_to_node(cast_node);
    /* Make "pmf.i", the offset into the virtual function table. */
    pmf_node = make_reusable_copy(pmf_node);
    offset_node = node_to_select_field_from_rvalue(pmf_node, mptr_i_field);
    /* Add the virtual function table address and the offset, giving the
       address of the virtual function table entry, and store that in
       "vtbl_temp". */
    vtbl_addr_node->next = offset_node;
    padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                   ptr_to_vtbl_entry_type, vtbl_addr_node);
    /* Make the temporary variable for the "vtbl_temp". */
    vtbl_temp_var = make_temporary(ptr_to_vtbl_entry_type);
    vtbl_temp_node = var_lvalue_expr(vtbl_temp_var);
    vtbl_temp_node->next = padd_node;
    vtbl_temp_assign_node = make_operator_node(
                                            (an_expr_operator_kind)eok_passign,
                                            ptr_to_vtbl_entry_type,
                                            vtbl_temp_node);
    /* Make "this_temp = (object_type *)((char *)this_temp + vtbl_temp->d)",
       which adjusts the "this" pointer to be passed to the virtual
       function. */
    vtbl_d_value = field_rvalue_selection_expr(var_rvalue_expr(vtbl_temp_var),
                                               mptr_d_field);
    this_temp_node = var_rvalue_expr(this_temp_var);
    this_temp_node = add_cast_to_char_star(this_temp_node);
    this_temp_node->next = vtbl_d_value;
    padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                   this_temp_node->type,
                                   this_temp_node);
    cast_node = add_cast(padd_node, object_type);
    this_temp_node = var_lvalue_expr(this_temp_var);
    this_temp_node->next = cast_node;
    this_increment_node = make_operator_node(
                                            (an_expr_operator_kind)eok_passign,
                                            object_type, this_temp_node);
    /* Make "vtbl_temp->f", the address of the virtual function to call. */
    vtbl_f_value = field_rvalue_selection_expr(var_rvalue_expr(vtbl_temp_var),
                                               mptr_f_field);
    /* Combine the three expressions that make up the virtual function
       processing code. */
    vtbl_temp_assign_node->next = this_increment_node;
    comma_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                    this_increment_node->type,
                                    vtbl_temp_assign_node);
    comma_node->next = vtbl_f_value;
    comma_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                    vtbl_f_value->type,
                                    comma_node);
    /* Assemble the "?:" operator. */
    compare_node->next = select_f_node;
    select_f_node->next = comma_node;
    question_mark_node = make_operator_node(
                                           (an_expr_operator_kind)eok_question,
                                           make_vptp_type(), compare_node);
    /* Cast the generic function pointer returned from the question mark
       operator to the proper type (so that we know what the return type,
       etc. is). */
    func_addr_node = add_cast_if_necessary(question_mark_node,
                                           ptr_routine_type);
  }  /* if */
  /* Make the call operands: func_addr_node (giving the function pointer),
     the this_temp (giving the object address), and any additional
     arguments. */
  this_temp_node = var_rvalue_expr(this_temp_var);
  func_addr_node->next = this_temp_node;
  this_temp_node->next = additional_args;
  /* Assemble the call node. */
  call_node = make_operator_node((an_expr_operator_kind)eok_call,
                                 expr->type, func_addr_node);
  /* Replace the original node by a comma node with the assignment to
     this_temp and the call under it. */
  this_temp_assign_node->next = call_node;
  set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                    expr->type, this_temp_assign_node);
}  /* lower_pm_call */


static void lower_call(an_expr_node_ptr      expr,
                       an_init_pos_descr_ptr ipdp)
/*
Lower a call (normal, virtual, or pointer-to-member).  expr points to the
call node.  ipdp, if non-NULL, indicates an entity into which the
call should return its value.
*/
{
  a_type_ptr                    rout_type;
  a_routine_type_supplement_ptr rtsp;
  an_expr_node_ptr              prev_arg_node, arg_node, temp_node, first_arg;
  an_expr_operator_kind         op = expr->variant.operation.kind;

  first_arg = arg_node = expr->variant.operation.operands;
  /* Extract the routine type. */
  if (op == (an_expr_operator_kind)eok_pm_call) {
    rout_type = pm_member_type_possibly_lowered(first_arg->type);
  } else {
    rout_type = type_pointed_to(first_arg->type);
  }  /* if */
  rout_type = skip_typerefs(rout_type);
  rtsp = rout_type->variant.routine.extra_info;
  /* Lower the expression giving the address of the routine. */
  lower_normal_expr(arg_node);
  prev_arg_node = arg_node;
  arg_node = arg_node->next;
  /* If the routine has a "this" parameter, lower it separately. */
  if (rtsp->implicit_this_param_type != NULL) {
    /* Treat the "this" parameter as an lvalue to avoid extra tests for NULL
       on base class casts. */
    lower_expr(arg_node, /*is_lvalue=*/TRUE);
    prev_arg_node = arg_node;
    arg_node = arg_node->next;
  }  /* if */
  /* If the routine returns its result to a temporary supplied by the caller,
     add an argument for that temporary.  This only happens under a
     dik_call_returning_class_via_cctor dynamic initialization entry. */
  if (rtsp->value_returned_by_cctor) {
#if CHECKING
    if (ipdp == NULL) {
      internal_error("lower_call: missing location for result");
    }  /* if */
#endif /* CHECKING */
    temp_node = make_init_entity_node(ipdp);
    temp_node->next = arg_node;
    prev_arg_node->next = temp_node;
    /* Change the result type of the call to "void". */
    expr->type = void_type();
  }  /* if */
  /* Lower the rest of the arguments. */
  lower_arg_expr_list(arg_node, rout_type);
  if (op == (an_expr_operator_kind)eok_virtual_call) {
    /* Virtual function call. */
    /* If the call is of a destructor, add the implied argument. */
    add_implied_args_to_call(expr, routine_from_node(first_arg));
    lower_virtual_function_call(expr);
  } else if (op == (an_expr_operator_kind)eok_pm_call) {
    /* Call of a function specified by a pointer-to-member. */
    lower_pm_call(expr);
  } else {
    check_assertion(op == (an_expr_operator_kind)eok_call);
    /* Normal call. */
    if (first_arg->kind == (an_expr_node_kind)enk_routine_address) {
      /* We know the specific routine being called. */
      /* If the call is of a constructor or destructor, add the
         implied arguments. */
      add_implied_args_to_call(expr, routine_from_node(first_arg));
    }  /* if */
  }  /* if */
}  /* lower_call */


static void lower_pm_comparison(an_expr_node_ptr expr)
/*
Lower comparison of two pointers to members.
*/
{
  an_expr_node_ptr select1_node, select2_node, compare_i_node;
  an_expr_node_ptr op1_node, op2_node, compare_i0_node, compare_d_node;
  an_expr_node_ptr compare_f_node, and_node, or_node;
  a_type_ptr       int_type;
  a_boolean        ne_case = (expr->variant.operation.kind ==
                                              (an_expr_operator_kind)eok_pmne);

  op1_node = expr->variant.operation.operands;
  if (is_or_was_ptr_to_member_function_type(op1_node->type)) {
    /* Pointer-to-member-function comparison.  Turns into
         op1.i == op2.i ? (op1.i == 0 || (op1.d == op2.d && op1.f == op2.f)) :
                          0
       for the == case.  The != case is similar, transformed by DeMorgan's
       law.  Note that op1 and op2 are rvalues. */
    op2_node = op1_node->next;
    int_type = integer_type((an_integer_kind)ik_int);
    /* Make sure the struct type used to represent a pointer-to-member-function
       is allocated. */
    (void)make_mptr_type();
    /* Make "op1.i == op2.i". */
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_i_field);
    select2_node = node_to_select_field_from_rvalue(op2_node, mptr_i_field);
    select1_node->next = select2_node;
    compare_i_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                        int_type, select1_node);
    /* Make "op1.i == 0" (or "!= 0" for the ne_case). */
    op1_node = make_reusable_copy(op1_node);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_i_field);
    select1_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
    compare_i0_node = make_operator_node(ne_case ? 
                                               (an_expr_operator_kind)eok_ine :
                                               (an_expr_operator_kind)eok_ieq,
                                      int_type, select1_node);
    /* Make "op1.d == op2.d" (or "!=" for the ne_case). */
    op1_node = make_reusable_copy(op1_node);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_d_field);
    op2_node = make_reusable_copy(op2_node);
    select2_node = node_to_select_field_from_rvalue(op2_node, mptr_d_field);
    select1_node->next = select2_node;
    compare_d_node = make_operator_node(ne_case ? 
                                               (an_expr_operator_kind)eok_ine :
                                               (an_expr_operator_kind)eok_ieq,
                                        int_type, select1_node);
    /* Make "op1.f == op2.f" (or "!=" for the ne_case). */
    op1_node = make_reusable_copy(op1_node);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_f_field);
    op2_node = make_reusable_copy(op2_node);
    select2_node = node_to_select_field_from_rvalue(op2_node, mptr_f_field);
    select1_node->next = select2_node;
    compare_f_node = make_operator_node(ne_case ? 
                                               (an_expr_operator_kind)eok_ine :
                                               (an_expr_operator_kind)eok_ieq,
                                        int_type, select1_node);
    /* Make "(op1.d == op2.d && op1.f == op2.f)" (or "||" for the ne_case). */
    compare_d_node->next = compare_f_node;
    and_node = make_operator_node(ne_case ? (an_expr_operator_kind)eok_lor :
                                            (an_expr_operator_kind)eok_land,
                                  int_type, compare_d_node);
    /* Make "(op1.i == 0 || (op1.d == op2.d && op1.f == op2.f))" (or "&&"
       for the ne_case. */
    compare_i0_node->next = and_node;
    or_node = make_operator_node(ne_case ? (an_expr_operator_kind)eok_land :
                                           (an_expr_operator_kind)eok_lor,
                                 int_type, compare_i0_node);
    /* Overwrite the original node with the "?" operator to make the full
       expression. */
    compare_i_node->next = or_node;
    or_node->next = node_for_integer_constant(ne_case ? 1L : 0L,
                                              (an_integer_kind)ik_int);
    set_node_operator(expr, (an_expr_operator_kind)eok_question,
                      or_node->type, compare_i_node);
  } else {
    /* Pointer-to-data-member comparison: turns into integer comparison. */
    expr->variant.operation.kind = ne_case ? (an_expr_operator_kind)eok_ine :
                                             (an_expr_operator_kind)eok_ieq;
  }  /* if */
}  /* lower_pm_comparison */


static void lower_pm_field(an_expr_node_ptr expr)
/*
Lower a pointer-to-member selection of a data member.  The operands of
the expression have already been lowered.
*/
{
  an_expr_node_ptr pdm_node, one_node, minus_node, cast_node, plus_node;
  an_expr_node_ptr object_node;

  /* p->*pdm is lowered to (member-type *)(((char *)p)+(pdm-1)).
     pdm, the pointer to data member, has already been turned into an
     integral type.  The "-1" reverses the increment done to reserve 0
     as a NULL pointer to data member. */
  object_node = expr->variant.operation.operands;
  pdm_node = object_node->next;
  object_node->next = NULL;
  /* Cast the object pointer node to "char *" to avoid scaling on the
     pointer addition. */
  cast_node = add_cast_to_char_star(object_node);
  /* Make the node for "pdm-1". */
  one_node = node_for_integer_constant(1L, TARG_DELTA_INT_KIND);
  pdm_node->next = one_node;
  minus_node = make_operator_node((an_expr_operator_kind)eok_isubtract,
                                  integer_type(TARG_DELTA_INT_KIND),
                                  pdm_node);
  /* Make the pointer addition node "((char *)p)+(pdm-1)". */
  cast_node->next = minus_node;
  plus_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                 cast_node->type, cast_node);
  /* Change the original node into a cast of the pointer expression to a
     pointer to the data member type.  The original expression type is
     already the correct pointer type. */
  change_to_cast(expr, plus_node, expr->type);
}  /* lower_pm_field */


static a_dynamic_init_ptr elem_dynamic_init(a_dynamic_init_ptr dip)
/*
dip points to a dynamic init entry that initializes a whole array.  Find
the dynamic init entry that applies to each element and return a pointer to it.
*/
{
  a_constant_ptr     con;
  a_dynamic_init_ptr elem_dip;

#if CHECKING
  if (dip->kind != (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    internal_error("elem_dynamic_init: not nonconst aggregate");
  }  /* if */
#endif /* CHECKING */
  /* The nonconstant aggregate case has ck_aggregate constant ->
     ck_init_repeat constant -> ck_dynamic_init constant ->
     dynamic init entry. */
  con = dip->variant.constant;
  con = con->variant.aggregate.first_constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_init_repeat) {
    internal_error("elem_dynamic_init: not ck_init_repeat");
  }  /* if */
#endif /* CHECKING */
  con = con->variant.init_repeat.constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_dynamic_init) {
    internal_error("elem_dynamic_init: not ck_dynamic_init");
  }  /* if */
#endif /* CHECKING */
  elem_dip = con->variant.dynamic_init;
  return elem_dip;
}  /* elem_dynamic_init */


static void lower_array_new(an_expr_node_ptr expr)
/*
Do lowering of an array new operation.  expr points to the enk_new_delete
expression.  The subtrees of the original expressions have not been lowered
yet.  This routine is used for arrays that require special handling, i.e.,
arrays with class elements.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init, elem_dip;
  a_type_ptr                  array_type, elem_type, ptr_elem_type;
  an_expr_node_ptr            entity_node, new_node, temp_var_node;
  an_expr_node_ptr            assign_node, num_elem_node, vec_new_node;
  a_variable_ptr              temp_var;
  a_constant                  num_elem_constant;
  a_boolean                   preserve_size_node;
  a_targ_size_t               elem_size;
  an_expr_node_ptr            size_node, constant_node, nonconstant_node;
  a_constant                  size_constant, null_constant;
  a_targ_size_t               con_for_size;
  a_boolean                   ovflo;
  a_routine_ptr               ctor_routine;

  /* Get the array element type. */
  array_type = skip_typerefs(ndsp->type);
  elem_type = new_delete_base_type_from_operation_type(ndsp->type);
  ptr_elem_type = make_pointer_type(elem_type);
  /* Build the node for the address of the array (entity_node). */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  if (ndsp->routine == NULL) {
    /* The __vec_new routine should do the allocation of the array (the normal
       case).  The entity_node is therefore a NULL pointer. */
    make_zero_of_proper_type(ptr_elem_type, &null_constant);
    entity_node = alloc_node_for_constant(&null_constant);
    /* Lower "arg" even though it is usually ignored.  It is used when the
       array size is nonconstant.  Note that it is not necessary to lower
       this as an argument list because it will not be used directly as
       such (pieces might be put into an argument list). */
    lower_expr_list(ndsp->arg, 0, FALSE);
    preserve_size_node = FALSE;
  } else {
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* The allocation is not standard and must be done before calling
       the __vec_new routine.  This happens for something like
         A *p = new (x, y, z) A[3];
       The "new" call is assigned to a temporary, and entity_node uses
       the temporary, as in
         ((temp = (type *)new-call(...)), (type *)__vec_new(temp, ...))
       The comma expression is necessary because we can't count on the
       order of evaluation of arguments of the __vec_new call and the new-call
       may contain the assignment to a temporary needed to make a reusable
       copy of the size expression.  */
    /* Make the "new" call. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type);
    new_node = make_call_node(ndsp->routine, ndsp->arg,
                              /*honor_virtual=*/FALSE);
    /* Make "temp = (type *)new-call(...)". */
    temp_var = make_temporary(ptr_elem_type);
    temp_var_node = var_lvalue_expr(temp_var);
    temp_var_node->next = add_cast_if_necessary(new_node, ptr_elem_type);
    assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                     ptr_elem_type, temp_var_node);
    /* The comma node is built at the end of this routine. */
    entity_node = var_rvalue_expr(temp_var);
    /* The size node is used in the "new" call, so it cannot be destroyed. */
    preserve_size_node = TRUE;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  /* Here, we have entity_node pointing to an expression for the address
     of the entity. */
  /* Make a node for the number of elements in the array. */
  if (array_type->size != 0) {
    /* The easy and usual case -- the array has a constant number of
       elements.  Do a division to get the right answer for the
       multi-dimensional array case. */
    set_unsigned_integer_constant(&num_elem_constant,
                                  array_type->size / elem_type->size,
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND);
    num_elem_node = alloc_node_for_constant(&num_elem_constant);
  } else {
    /* Nonconstant number of elements in the array.  The number of elements
       must be extracted from the size expression.  If preserve_size_node
       is TRUE, the size expression is used in the "new" call, and therefore
       a reusable copy must be made of whatever part is reused here. */
    /* Get the node that gives the size of the allocation. */
    size_node = ndsp->arg;
    /* Get the size of each element, in bytes. */
    elem_size = elem_type->size;
    if (elem_size == 1) {
      /* The element size is 1, so the number of elements is equal to the
         total size. */
      if (preserve_size_node) {
        /* size_node must be preserved, so make a copy of it. */
        num_elem_node = make_reusable_copy(size_node);
      } else {
        num_elem_node = size_node;
      }  /* if */
    } else {
      /* A division by the element size is required.  The size expression
         should look like "expr*n" where "n" is the element size,
         i.e., a multiplication added while scanning the "new" to convert
         the number of elements to the total size.  The usual strategy
         is to remove the "*n".  Note however that for a multi-dimensional
         array case "n" is the product of the element size and the dimension
         bounds after the first; for that case we create a new constant
         that is "n" divided by the element size. */
      /* Drop any cast on the top of the expression, such as one added by
         lower_arg_expr_list to promote the expression for calling an old-style
         function. */
      while (is_operation_node(size_node) &&
             size_node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast) {
        size_node = size_node->variant.operation.operands;
      }  /* if */
      check_assertion(is_operation_node(size_node) &&
                      size_node->variant.operation.kind ==
                                         (an_expr_operator_kind)eok_imultiply);
      nonconstant_node = size_node->variant.operation.operands;
      constant_node = nonconstant_node->next;
      check_assertion(is_constant_node(constant_node));
      if (preserve_size_node) {
        /* We need to preserve size_node, and therefore we need a copy of the
           nonconstant node. */
        nonconstant_node = make_reusable_copy(nonconstant_node);
      } else {
        /* We can use the expression directly.  Break the connection
           between the first operand and second operand of the "*"
           operation. */
        nonconstant_node->next = NULL;
      }  /* if */
      /* Divide the constant by the element size. */
      size_constant = *constant_node->variant.constant;
      check_assertion(size_constant.kind == (a_constant_repr_kind)ck_integer);
      con_for_size = unsigned_value_of_integer_constant(&size_constant,
                                                        &ovflo);
      check_assertion(!ovflo);
      /* Note that we know the type is not incomplete, so the element size
         is not zero. */
      con_for_size /= elem_size;
      if (con_for_size == 1) {
        /* No multiplication is needed. */
        num_elem_node = nonconstant_node;
      } else {
        /* The multiplication is still needed.  This must be a multi-
           dimensional array case. */
        set_unsigned_integer_value(&size_constant.variant.integer_value,
                                   (unsigned long)con_for_size);
        constant_node = alloc_node_for_constant(&size_constant);
        nonconstant_node->next = constant_node;
        num_elem_node = make_operator_node(
                                          (an_expr_operator_kind)eok_imultiply,
                                          nonconstant_node->type,
                                          nonconstant_node);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Here, num_elem_node is an expression for the number of elements in
     the array. */
  /* Determine the constructor routine (if any) to be called. */
  if (dip != NULL) {
    /* There is a dynamic init entry to initialize the storage after it is
       allocated. */
    /* Get a pointer to the dynamic init entry that applies to the array
       elements instead of the whole array. */
    elem_dip = elem_dynamic_init(dip);
    check_assertion(elem_dip->kind == (a_dynamic_init_kind)dik_constructor) ;
    /* Get the constructor routine to call. */
    ctor_routine = elem_dip->variant.constructor.ptr;
    /* If the constructor has default arguments, make a routine that
       calls the constructor with the necessary default arguments. */
    lower_arg_expr_list(elem_dip->variant.constructor.args,
                        ctor_routine->type);
    ctor_routine = default_version_of_routine(ctor_routine,
                                           elem_dip->variant.constructor.args);
  } else {
    /* There is no dynamic init entry; the storage is not initialized after
       allocation. */
    ctor_routine = NULL;
  }  /* if */
  /* Construct the call of __vec_new. */
  vec_new_node = make_vec_new_call(entity_node, num_elem_node, ctor_routine);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  if (ndsp->routine != NULL) {
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* Build a comma node that encloses the allocation call and the
       __vec_new call.  See comment above. */
    assign_node->next = vec_new_node;
    vec_new_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                      vec_new_node->type, assign_node);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  /* Overwrite expr with a cast of the result of __vec_new (of type void *)
     to the right pointer type. */
  change_to_cast(expr, vec_new_node, expr->type);
}  /* lower_array_new */


static void lower_array_delete(an_expr_node_ptr expr)
/*
Do lowering of an array delete operation.  expr points to the enk_new_delete
for the operation.  The subtrees of the expression have not been lowered
yet.  This routine is used for arrays that require special handling,
i.e., arrays with class elements.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init, elem_dip;
  a_routine_ptr               dtor_routine;
  an_expr_node_ptr            vec_delete_node;

  /* Lower "arg"; do it as a list in case the delete routine is the
     two-argument version.  Drop the second argument if present. */
  lower_expr_list(ndsp->arg, 0, FALSE);
  ndsp->arg->next = NULL;
  if (dip != NULL) {
    /* A destructor must be called. */
    /* Get a pointer to the dynamic init entry that applies to the array
       elements instead of the whole array. */
    elem_dip = elem_dynamic_init(dip);
    /* Get the destructor to call. */
    dtor_routine = elem_dip->destructor;
    check_assertion(dtor_routine != NULL);
  } else {
    /* There is no dynamic init entry, and therefore no destruction need be
       done along with the deallocation. */
    dtor_routine = NULL;
  }  /* if */
  vec_delete_node = make_vec_delete_call(ndsp->arg,
                                         /*array_element_count=*/-1L,
                                         dtor_routine,
                                         /*free_storage=*/TRUE);
  /* Overwrite the original node with the __vec_delete call. */
  overwrite_node(expr, vec_delete_node);
}  /* lower_array_delete */


static a_boolean new_or_delete_type_requires_array_handling(a_type_ptr type)
/*
type is the base type underlying an array type involved in a new or delete.
Return TRUE if the new or delete operation requires special handling.
Special handling means the __vec_new and __vec_delete routines must be
called, so that constructors and destructors will be called, and so 
that the size of the array is recorded for use at the time of the delete
of the array pointer.
*/
{
  a_boolean                     special = FALSE;
  a_class_symbol_supplement_ptr cssp;

  /* Only types with a constructor or destructor require special handling. */
  if (is_class_struct_union_type(type)) {
    cssp = symbol_supplement_for_class(type);
    if (cssp->constructor != NULL || cssp->destructor != NULL) {
      special = TRUE;
    }  /* if */
  }  /* if */
  return special;
}  /* new_or_delete_type_requires_array_handling */


static void lower_new(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node for a "new".
The subtree of the node has not yet been lowered.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_type_ptr                  base_type, ptr_base_type;
  a_variable_ptr              temp_var;
  an_expr_node_ptr            temp_var_node, assign_node, compare_node;
  an_expr_node_ptr            init_node, call_node, null_node;
  a_constant                  null_constant;
  an_insert_location          insert_location;
  an_init_pos_descr           ipd;
  a_boolean                   keep_dynamic_init;

  
  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (is_array_type(ndsp->type) &&
      new_or_delete_type_requires_array_handling(base_type)) {
    /* An array "new". */
    lower_array_new(expr);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  } else if (ndsp->routine == NULL) {
    /* The "new" call has been folded into the constructor call. */
    a_routine_ptr    ctor_routine = dip->variant.constructor.ptr;
    an_expr_node_ptr implied_arg_list, end_implied_arg_list;
    /* ndsp->arg is not lowered because it is thrown away. */
    /* Pass a NULL for the "this" parameter to tell the constructor to
       do the allocation. */
    make_zero_of_proper_type(make_pointer_type(base_type), &null_constant);
    null_node = alloc_node_for_constant(&null_constant);
    /* Add any implicit arguments for the constructor. */
    make_ctor_implied_arg_list(ctor_routine, &implied_arg_list,
                               &end_implied_arg_list);
    if (implied_arg_list != NULL) {
      null_node->next = implied_arg_list;
    } else {
      end_implied_arg_list = null_node;
    }  /* if */
    /* Preserve any additional parameters from the constructor call. */
    if (dip->variant.constructor.args != NULL) {
      lower_arg_expr_list(dip->variant.constructor.args,
                          ctor_routine->type);
      end_implied_arg_list->next = dip->variant.constructor.args;
    }  /* if */
    /* Make the constructor call. */
    call_node = make_call_node(ctor_routine, null_node,
                               /*honor_virtual=*/FALSE);
    /* The constructor call returns a pointer to the object initialized.
       Cast the pointer to the right type if necessary. */
    call_node = add_cast_if_necessary(call_node, expr->type);
    /* Overwrite the enk_new_delete node with the call/cast. */
    overwrite_node(expr, call_node);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Create a call of the "new" routine. */
    lower_arg_expr_list(ndsp->arg, ndsp->routine->type);
    call_node = make_call_node(ndsp->routine, ndsp->arg,
                               /*honor_virtual=*/FALSE);
    /* Note that the type of the "new" call might be unrelated to the type
       we are allocating, e.g., it might be "void *"; a cast is done later. */
    if (dip != NULL) {
      /* Initialization is required.  It must be done only if the allocation
         succeeds, so build an expression like
           ((temp = (type *)new-call(...)) != NULL) ?
                                     (initialization, temp) : NULL
      */
      /* Allocate the temporary. */
      ptr_base_type = make_pointer_type(base_type);
      temp_var = make_temporary(ptr_base_type);
      temp_var_node = var_lvalue_expr(temp_var);
      /* Assign the entity address expression to the temporary. */
      temp_var_node->next = add_cast_if_necessary(call_node, ptr_base_type);
      assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                       ptr_base_type, temp_var_node);
      /* Compare the assignment node to a NULL constant of the right type. */
      make_zero_of_proper_type(ptr_base_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_node;
      compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                        integer_type((an_integer_kind)ik_int),
                                        assign_node);
      /* Start the initialization code as just the value of the temporary.
         Additional code will be inserted by putting comma operators on
         top of this node. */
      init_node = var_rvalue_expr(temp_var);
      set_expr_insert_location(init_node, &insert_location);
      /* Build a description of the entity to be initialized.  Adjust the
         type so that it is an array if necessary. */
      set_var_indirect_init_pos_descr(temp_var, &ipd);
      ipd.base_type = ndsp->type;
      /* Generate code for the initialization. */
      lower_dynamic_init(dip, &ipd,
                         /*first_time_test_var=*/(a_variable_ptr)NULL,
                         /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
      check_assertion(!keep_dynamic_init);
      /* Build the ?: operation.  Its first argument is the comparison of
         the temp pointer against NULL; its second is the initialization code;
         and its third is another NULL constant of the right type. */
      make_zero_of_proper_type(ptr_base_type, &null_constant);
      null_node = alloc_node_for_constant(&null_constant);
      compare_node->next = init_node;
      init_node->next = null_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                     ptr_base_type, compare_node);
    }  /* if */
    /* Turn the original enk_new_delete node into a cast to the right
       pointer type. */
    change_to_cast(expr, call_node, expr->type);
  }  /* if */
}  /* lower_new */


static an_expr_node_ptr make_dtor_call_for_delete(
                                                 a_dynamic_init_ptr dip,
                                                 an_expr_node_ptr   ptr_node,
                                                 a_boolean          deallocate)
/*
Generate a call of a destructor as part of a delete operation.  dip points
to a dynamic initialization entry that indicates the destructor.  ptr_node
points to the object to be destroyed.  deallocate is TRUE if the destructor
should be asked to deallocate the storage.  If the destructor is virtual,
it is called as a virtual function, which involves some special tricks.
*/
{
  an_expr_node_ptr ptr_node_copy, call_node;
  an_expr_node_ptr operand_node, compare_node;
  a_constant       null_constant;
  a_routine_ptr    dtor_routine = dip->destructor;
  long             bit_mask;

  check_assertion(dtor_routine != NULL);
  /* Add an implicit parameter to the destructor call with bits
     0x2 (whole object) + 0x1 (free storage, if deallocate is TRUE). */
  bit_mask = 2L;
  if (deallocate) bit_mask |= 1L;
  ptr_node->next = node_for_integer_constant(bit_mask,
                                             (an_integer_kind)ik_int);
  /* Make a call of the destructor. */
  call_node = make_call_node(dtor_routine, ptr_node, /*honor_virtual=*/TRUE);
  if (dtor_routine->is_virtual) {
    /* The destructor is virtual, so rewrite the call. */
    /* Make a copy of the "this" argument so it can be used twice. */
    ptr_node_copy = make_reusable_copy(ptr_node);
    /* Put the copy under the original destructor call; the original gets
       tested for NULL. */
    operand_node = call_node->variant.operation.operands;
    ptr_node_copy->next = ptr_node->next;
    operand_node->next = ptr_node_copy;
    /* Rewrite the virtual call as a normal call. */
    lower_virtual_function_call(call_node);
    /* Also ... if the pointer to be deleted is NULL, one can't use it to
       look up a virtual function, and therefore the destructor cannot
       be the one to do the NULL pointer test.  We must add the test here
       above the destructor call.  The test inside the destructor is still
       needed for those cases where the routine is called non-virtually. */
    /* Make "ptr_node != NULL". */
    make_zero_of_proper_type(ptr_node->type, &null_constant);
    ptr_node->next = alloc_node_for_constant(&null_constant);
    compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                      integer_type((an_integer_kind)ik_int),
                                      ptr_node);
    /* Make "(ptr_node != NULL) ? dtor(...) : (void)0". */
    compare_node->next = call_node;
    compare_node->next->next =
               add_cast(node_for_integer_constant(0L, (an_integer_kind)ik_int),
                        void_type());
    call_node = make_operator_node((an_expr_operator_kind)eok_question,
                                   call_node->type, compare_node);
  }  /* if */
  return call_node;
}  /* make_dtor_call_for_delete */


static void lower_delete(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node for a "delete".
The subtree of the node has not yet been lowered.
*/
{
  a_new_delete_supplement_ptr ndsp = expr->variant.new_delete;
  a_dynamic_init_ptr          dip = ndsp->dynamic_init;
  a_type_ptr                  base_type;
  an_expr_node_ptr            ptr_node = ndsp->arg, call_node, dtor_call_node;
  an_expr_node_ptr            second_arg_node;

  base_type = new_delete_base_type_from_operation_type(ndsp->type);
  if (ndsp->array_delete &&
      new_or_delete_type_requires_array_handling(base_type)) {
    /* An array "delete". */
    lower_array_delete(expr);
#if !DELETE_CAN_BE_FOLDED_INTO_DTOR
/* IL lowering requires that it be possible to fold the delete call into
   a destructor.  Without that, it has no way of getting the right size
   on a delete of a pointer to a class with a virtual destructor. */
??=error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
#endif /* !DELETE_CAN_BE_FOLDED_INTO_DTOR */
  } else if (ndsp->routine == NULL) {
    /* The "delete" call has been folded into the destructor call.
       Generate the destructor call with an implicit parameter to indicate
       deallocation. */
    /* Lower "arg"; do it as a list in case the delete routine is the
       two-argument version.  Drop the second argument if present. */
    lower_expr_list(ptr_node, 0, FALSE);
    ptr_node->next = NULL;
    dtor_call_node = make_dtor_call_for_delete(dip, ptr_node,
                                               /*deallocate=*/TRUE);
    /* Overwrite the enk_new_delete node with the call. */
    overwrite_node(expr, dtor_call_node);
  } else {
    /* Non-array case, or array case that does not require special handling. */
    /* Lower "arg"; do it as a list in case the delete routine is the
       two-argument version. */
    lower_arg_expr_list(ptr_node, ndsp->routine->type);
    /* Break off the second argument if there is one; it will be reattached
       later after ptr_node has been messed with. */
    second_arg_node = ptr_node->next;
    ptr_node->next = NULL;
    if (dip != NULL) {
      /* Case like
           struct A { ~A(); } *p;
           ::delete p;
         This cannot be folded into the destructor call because the delete
         routine is not the standard one.  Make something like
           (dtor(p), delete(p))
      */
      an_expr_node_ptr orig_ptr_node = ptr_node;
      /* Make a reusable copy of the pointer node for use in the
         delete call. */
      ptr_node = make_reusable_copy(orig_ptr_node);
      dtor_call_node = make_dtor_call_for_delete(dip, orig_ptr_node,
                                                 /*deallocate=*/FALSE);
      /* The comma node is built later in this routine. */
    }  /* if */
    /* Make the "delete" call.  It is not necessary to test for non-NULL;
       the delete routine does that.  Cast the argument to "void *", which
       is what the delete routine expects. */
    ptr_node = add_cast_if_necessary(ptr_node, void_star_type());
    /* Reattach the second operand to delete is there is one. */
    ptr_node->next = second_arg_node;
    call_node = make_call_node(ndsp->routine, ptr_node,
                               /*honor_virtual=*/FALSE);
    if (dip != NULL) {
      /* Finish the destructor case by building the comma node. */
      dtor_call_node->next = call_node;
      call_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                     call_node->type, dtor_call_node);
    }  /* if */
    /* Overwrite the enk_new_delete node with the call. */
    overwrite_node(expr, call_node);
  }  /* if */
}  /* lower_delete */


static void lower_new_delete(an_expr_node_ptr expr)
/*
Do IL lowering of an enk_new_delete expression node, used for a "new" or
"delete".  The subtree of the node has not yet been lowered.
*/
{
  if (expr->variant.new_delete->is_new) {
    /* "new" case. */
    lower_new(expr);
  } else {
    /* "delete" case. */
    lower_delete(expr);
  }  /* if */
}  /* lower_new_delete */


static void lower_expr(an_expr_node_ptr expr,
                       a_boolean        is_lvalue)
/*
Do IL lowering of the indicated expression and everything under it.
The expression is being used as an lvalue if is_lvalue is TRUE.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      operand_node;
  an_insert_location    insert_location;
  an_init_pos_descr     ipd;
  a_boolean             keep_dynamic_init, result_is_addr;
  a_dynamic_init_ptr    dip;
  a_variable_ptr        var, temp_var;
  unsigned int          is_lvalue_mask;
  a_boolean             is_conditional_operator;
  a_type_ptr            temp_type;

  lower_os_type(expr->type);
  switch (expr->kind) {
    case enk_routine_address:
    case enk_field:
      /* No processing required. */
      break;
    case enk_variable:
    case enk_variable_address:
      /* If the variable is a parameter that's passed by copy constructor,
         an implicit indirection must be added. */
      var = expr->variant.variable;
      /* assoc_param_type is NULL on the "this" parameter variable and
         the return value pointer variable. */
      if (var->is_parameter && var->assoc_param_type != NULL &&
          var->assoc_param_type->passed_via_copy_constructor) {
        /* Add an indirection. */
        if (expr->kind == (an_expr_node_kind)enk_variable_address) {
          /* Address of variable becomes value of variable. */
          expr->kind = (an_expr_node_kind)enk_variable;
          /* The expression type is already as we want it; it was "wrong"
             because the parameter type was changed in lowering, but the
             indirection here cancels out the extra pointer-to on
             the parameter type. */
        } else {
          /* Value of variable becomes value pointed to by variable.  Make
             a copy of the enk_variable node and overwrite the original
             node with an eok_indirect. */
          an_expr_node_ptr var_value = copy_node(expr), expr_next;
          expr_next = expr->next;
          clear_expr_node(expr, (an_expr_node_kind)enk_operation);
          expr->next = expr_next;
          set_node_operator(expr, (an_expr_operator_kind)eok_indirect,
                            var_value->type, var_value);
          /* Again, the type that was in the node is the one we want
             because the pointer-to and indirection cancel out.
             The type in the enk_variable node must be changed. */
          var_value->type = make_pointer_type(var_value->type);
        }  /* if */
      }  /* if */
      break;
    case enk_operation:
      operand_node = expr->variant.operation.operands;
      op = expr->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_base_class_cast ||
          op == (an_expr_operator_kind)eok_derived_class_cast) {
        /* Cast to base or derived class is rewritten.  This call also
           lowers any subtree. */
        lower_related_class_cast(expr, is_lvalue);
      } else if (op == (an_expr_operator_kind)eok_pm_base_class_cast ||
                 op == (an_expr_operator_kind)eok_pm_derived_class_cast) {
        /* Cast of pointer-to-member to base or derived class is rewritten.
           This call also lowers any subtree. */
        lower_pm_related_class_cast(expr, is_lvalue);
      } else if (op == (an_expr_operator_kind)eok_call ||
                 op == (an_expr_operator_kind)eok_virtual_call ||
                 op == (an_expr_operator_kind)eok_pm_call) {
        /* Calls of various kinds. */
        lower_call(expr, (an_init_pos_descr_ptr)NULL);
      } else {
        /* Determine which operands if any are lvalues, and whether or not
           the operand has conditional operands. */
        is_lvalue_mask = 0;
        is_conditional_operator = FALSE;
        if (op == (an_expr_operator_kind)eok_question) {
          /* Question mark's second and third operands are lvalues if the
             question mark itself is. */
          if (is_lvalue) is_lvalue_mask = 0x6;
          is_conditional_operator = TRUE;
        } else if (op == (an_expr_operator_kind)eok_comma) {
          /* Comma's second operand is an lvalue if the comma itself is. */
          if (is_lvalue) is_lvalue_mask = 0x2;
        } else if (op == (an_expr_operator_kind)eok_land ||
                   op == (an_expr_operator_kind)eok_lor) {
          /* "&&" and "||". */
          is_conditional_operator = TRUE;
        } else {
          /* Other operators.  See if the first operand is an lvalue. */
          if (operator_takes_lvalue_operand(op)) is_lvalue_mask = 0x1;
        }  /* if */
        /* Lower the operands of the expression. */
        lower_expr_list(operand_node, is_lvalue_mask, is_conditional_operator);
        if (expr->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
          /* lvalue-returning assignment operator or prefix ++/--.  Rewrite
               x = y          really: &x = y
             using an rvalue-returning operator as
               ((x = y), x)   really: ((&x = y), &x)
             If necessary, make a reusable copy of x.  The same kind of
             rewrite is done for the prefix ++/-- case. */
          an_expr_node_ptr new_assign_node;
          expr->variant.operation.returns_lvalue_instead_of_usual_rvalue=FALSE;
          /* Make a copy of the assignment node that is an rvalue
             assignment.  The same process works for the prefix ++/-- case
             because the second operand is not touched. */
          new_assign_node = copy_node(expr);
          new_assign_node->type = type_pointed_to(expr->type);
          /* Attach a copy of the lvalue address to it, for the second
             operand of the comma operator. */
          new_assign_node->next = make_lvalue_reusable_copy(operand_node);
          /* Change the original node to a comma node. */
          set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                            expr->type, new_assign_node);
          /* Continue lowering with the rvalue assignment node.  Note that
             operand_node is still set correctly. */
          expr = new_assign_node;
        }  /* if */
        /* Do any special lowering required for this operator after the
           operands have been lowered. */
        switch (op) {
          case eok_virtual_function_ptr:
            /* Determine virtual function address. */
            lower_virtual_function_ptr(expr);
            break;
          case eok_vacuous_destructor_call:
            /* A call of a "destructor" for a class or simple type that does
               not have one, e.g., p->int::~int().  Change the node into
               a cast to void. */
            change_to_cast(expr, operand_node, expr->type);
            break;
          case eok_cast:
            /* A cast from one pointer-to-member type to another does nothing.
               Note that pointer-to-data-member and pointer-to-member-function
               cannot be intercast.  A cast between pointers to data members
               cannot be recognized here (it's now an integral cast), but
               that's harmless.  The pointer to member function case must
               be rewritten, however, since it's now a cast of a struct
               type. */
            if (is_or_was_ptr_to_member_function_type(expr->type)) {
              overwrite_node(expr, operand_node);
            }  /* if */
            break;
          case eok_pmassign:
            /* Pointer-to-member assignment turns into integer assignment
               for pointers to data members, struct assignment for pointers
               to member functions. */
            expr->variant.operation.kind =
                 lowered_ptr_to_member_assignment_operator(operand_node->next->
                                                                         type);
            break;
          case eok_pmeq:
          case eok_pmne:
            /* Pointer-to-member comparison. */
            lower_pm_comparison(expr);
            break;
          case eok_pm_field:
            /* Pointer-to-member selection of a data member. */
            lower_pm_field(expr);
            break;
          case eok_field:
          case eok_value_field:
          case eok_bit_field:
          case eok_value_bit_field:
          case eok_extract_bit_field:
            /* If a field selection refers to an anonymous union field,
               adjust it to make the anonymous union reference(s) explicit. */
            adjust_field_selection_for_anonymous_union_references(expr);
            break;
#if ASSIGNMENT_TO_THIS_ALLOWED
          case eok_passign:
            /* Check for assignment to "this" in a constructor. */
            if (nearest_function_scope != NULL) {
              a_routine_ptr curr_routine =
                                   nearest_function_scope->variant.routine.ptr;
              if (curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
                a_variable_ptr this_param_var =
                            nearest_function_scope->variant.routine.parameters;

                if (operand_node->kind ==
                                     (an_expr_node_kind)enk_variable_address &&
                    operand_node->variant.variable == this_param_var) {
                  /* This is an assignment to "this".  Add the wrapper code
                     (to initialize base classes, etc.) following the
                     assignment to "this".  Add a comma expression on top
                     that produces the same value as the original assignment,
                     in case the value of the assignment is used.  That is,
                       this = expr
                     becomes
                       ((this = expr), initialization, this)
                  */
                  an_expr_node_ptr   new_expr;
                  an_insert_location insert_location;

                  if (expr->variant.operation.
                                      returns_lvalue_instead_of_usual_rvalue) {
                    /* The assignment returns an lvalue, i.e., the address
                       of the "this" parameter. */
                    new_expr = var_lvalue_expr(this_param_var);
                  } else {
                    /* The assignment returns an rvalue, i.e., the value
                       of the "this" parameter. */
                    new_expr = var_rvalue_expr(this_param_var);
                  }  /* if */
                  set_expr_insert_location(expr, &insert_location);
                  insert_location.variant.expr.insert_before = FALSE;
                  /* The insert location now specifies insertion before the
                     expression we just inserted.  Add the wrapper code
                     there. */
                  add_constructor_wrapper_code(nearest_function_scope,
                                               &insert_location);
                  insert_expr(new_expr, &insert_location);
                }  /* if */
              }  /* if */
            }  /* if */
            break;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
          default:
            /* No action on most operators. */
            break;
        }  /* switch */
      }  /* if */
      break;
    case enk_constant:
      lower_os_constant(expr->variant.constant);
      if (check_for_troublesome_ptr_to_member_constant(expr->variant.constant,
                                                       &temp_var)) {
        /* This expression node is loading the value of a pointer-to-
           member-function, which has or will become a struct represented by
           a ck_aggregate constant.  Since a ck_aggregate constant is
           not allowed here, use the value of a temporary variable
           initialized with the ck_aggregate constant. */
        set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
        expr->variant.variable = temp_var;
        /* Note that the type will already have been adjusted to the proper
           struct type. */
      }  /* if */
      break;
    case enk_temp_init:
      dip = expr->variant.init.dynamic_init;
      /* Determine the type of the temporary. */
      temp_type = expr->type;
      result_is_addr = expr->variant.init.result_is_addr;
      if (result_is_addr) {
        /* The value of the enk_temp_init node is the address of the temporary,
           so drop the pointer-to to get the temporary type. */
        temp_type = type_pointed_to(temp_type);
      }  /* if */
      /* Create a temporary variable. */
      dip->variable = make_temporary_possibly_at_file_scope(
                                           temp_type,
                                           processing_file_scope_init_routine);
      /* Change the enk_temp_init to a reference to the value or address
         of the temporary. */
      if (result_is_addr) {
        set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
      } else {
        set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
      }  /* if */
      expr->variant.variable = dip->variable;
      /* Generate code for the dynamic init. */
      set_var_init_pos_descr(dip->variable, &ipd);
      /* Any code generated for the dynamic initialization will be
         inserted before the original expression. */
      set_expr_insert_location(expr, &insert_location);
      lower_dynamic_init(dip, &ipd,
                         /*first_time_test_var=*/(a_variable_ptr)NULL,
                         /*is_expr_temporary=*/TRUE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
      check_assertion(!keep_dynamic_init);
      /* Optimization -- if the initialization is done by a constructor,
         and the enk_temp_init returns the address of the temporary,
         use the pointer returned from the constructor as the value of
         the expression. */
      if (result_is_addr &&
          dip->kind == (a_dynamic_init_kind)dik_constructor) {
        check_assertion(is_operation_node(expr) &&
                        expr->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_comma);
        overwrite_node(expr, expr->variant.operation.operands);
      }  /* if */
      break;
    case enk_new_delete:
      lower_new_delete(expr);
      break;
#if CHECKING
    default:
      internal_error("lower_expr: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* lower_expr */


static void lower_statement_list(a_statement_ptr statement_list,
                                 a_statement_ptr *last_statement)
/*
Do IL lowering of the indicated list of statements and everything under it.
Return a pointer to the last statement in *last_statement, or NULL if
there are no statements on the list.
*/
{
  a_statement_ptr statement, statement_next;

  for (*last_statement = NULL, statement = statement_list;
       statement != NULL;) {
    /* Save the "next" pointer now, so that any statements inserted by lowering
       will not be lowered (in particular, lowering of stmk_init statements
       inserts statements, and the expressions therein should not be lowered
       again). */
    statement_next = statement->next;
    lower_statement(statement);
    *last_statement = statement;
    statement = statement_next;
  }  /* for */
  /* If there were statements inserted at the end of a sequence, find the
     real last statement even though the inserted statements weren't (and
     shouldn't be) lowered. */
  while (*last_statement != NULL && (*last_statement)->next != NULL) {
    *last_statement = (*last_statement)->next;
  }  /* while */
}  /* lower_statement_list */


static void remove_temp_required_destructor_calls(void)
/*
Remove any required destructor calls in the current context that are
related to compiler-generated expression temporaries.  (Such temporaries
have shorter lifetimes than normal variables.)
*/
{
  a_required_destructor_call_ptr rdcp, prev_rdcp;

  /* Go through the list of required destructor calls, find the ones
     for temporaries, and unlink them. */
  for (prev_rdcp = NULL, rdcp = curr_context->required_destructor_calls;
       rdcp != NULL;
       rdcp = rdcp->next) {
    if (rdcp->is_expr_temporary) {
      /* Remove this entry from the list. */
      if (prev_rdcp == NULL) {
        curr_context->required_destructor_calls = rdcp->next;
      } else {
        prev_rdcp->next = rdcp->next;
      }  /* if */
    } else {
      /* Keep this entry. */
      prev_rdcp = rdcp;
    }  /* if */
  }  /* for */
}  /* remove_temp_required_destructor_calls */


static void lower_switch_clause_list(a_switch_clause_ptr clause_list,
                                     a_context_ptr       switch_context)
/*
Do IL lowering of the indicated switch clause list and everything under it.
If the switch statement has an associated context, switch_context points to
it; otherwise, switch_context is NULL.
*/
{
  a_switch_clause_ptr clause;
  a_statement_ptr     last_statement;
  an_insert_location  insert_location;
  a_boolean           break_reachable;

  for (clause = clause_list; clause != NULL; clause = clause->next) {
    /* Remember information about the current switch clause for use by
       add_conditional_destruction_temp. */
    curr_context->assoc_switch_clause = clause;
    curr_context->dynamic_init_preceding_switch_clause =
                                   curr_context->latest_dynamic_init_processed;
    lower_constant_list(clause->constant_list);
    lower_statement_list(clause->statements, &last_statement);
    /* If the last statement is not a branch, there is an implicit "break"
       at the end of the clause statements.  Any required destructor calls
       must be emitted on the "break". */
    if (last_statement == NULL) {
      /* No statements in the clause, so the end is reachable. */
      break_reachable = TRUE;
    } else if (last_statement->kind == (a_statement_kind)stmk_goto ||
               last_statement->kind == (a_statement_kind)stmk_return) {
      /* The last statement is a goto or return, so the end is not
         reachable. */
      break_reachable = FALSE;
    } else if (last_statement->kind == (a_statement_kind)stmk_block &&
               !last_statement->variant.block.extra_info->
                                                      end_of_block_reachable) {
      /* The last statement is a block whose end is not reachable, so the
         end is not reachable. */
      break_reachable = FALSE;
    } else {
      /* Otherwise the end is assumed to be reachable. */
      break_reachable = TRUE;
    }  /* if */
    if (break_reachable) {
      /* There is an implicit "break" at the end of the clause. */
      if (switch_context != NULL &&
          any_required_destructor_calls(switch_context)) {
        /* The switch statement has a context.  Generate any destructor calls
           required at the end of the context.  Note that the implicit "break"
           is only used at the top level within a switch; "break" statements
           from deeper (e.g., inside nested blocks) will be rendered as
           gotos. */
        if (last_statement == NULL) {
          /* The clause is empty, so add a block statement and insert inside
             it. */
          clause->statements = alloc_statement((a_statement_kind)stmk_block);
          set_block_start_insert_location(clause->statements,
                                          &insert_location);
        } else {
          /* Insert after the last statement. */
          set_insert_location(last_statement, &insert_location);
        }  /* if */
        gen_required_destructor_calls(switch_context, &insert_location);
      }  /* if */
    }  /* if */
    /* Get rid of the entries for required destructor calls on
       compiler-generated expression temporaries (the destructor calls
       have already been generated). */
    remove_temp_required_destructor_calls();
  }  /* for */
  curr_context->assoc_switch_clause = NULL;
}  /* lower_switch_clause_list */


static void add_first_time_test(an_insert_location_ptr insert_location,
                                a_variable_ptr         *first_time_test_var)
/*
Add a first-time test sequence that will surround the initialization of a
local static variable.  In effect:

  static int test_var;  // Global test var, implicitly init to 0
  {
    if (test_var == 0) {
      test_var = 1;
      ... real initialization of variable being initialized
    }
  }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion after the assignment statement.  A pointer to the
first-time-test variable is returned in *first_time_test_var.
*/
{
  a_variable_ptr     test_var;
  an_expr_node_ptr   test_var_node, compare_node;
  an_insert_location insert_location2;
  a_type_ptr         int_type;

  /* Make the static first-time-test variable at the file scope.  It's at the
     file scope so it can be reached in file-scope destructor code.
     (Local static variables are initialized when reached in their local
     blocks, but destroyed on exit from the whole program, and only if they
     were initialized). */
  int_type = integer_type((an_integer_kind)ik_int);
  *first_time_test_var = test_var = make_variable((char *)NULL,
                                                  /*already_il_name=*/TRUE,
                                                  int_type,
                                                  (a_storage_class)sc_static);
  /* Make "test_var == 0". */
  test_var_node = var_rvalue_expr(test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                    int_type, test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, insert_location, &insert_location2);
  /* Further inserts are done at the start of the block. */
  *insert_location = insert_location2;
  /* Make "test_var = 1" and insert it inside the "if" statement. */
  (void)insert_var_assignment_statement(test_var,
                                        (an_expr_operator_kind)eok_iassign,
                                        node_for_integer_constant(1L,
                                                      (an_integer_kind)ik_int),
                                        insert_location);
}  /* add_first_time_test */


static void add_last_time_test(a_variable_ptr         test_var,
                               an_insert_location_ptr insert_location,
                               an_insert_location_ptr insert_location2)
/*
Add a sequence of code that tests the variable set on first-time initialization
of a variable.  This is used in deciding whether or not to call a
destructor on the variable.  The sequence is

    if (test_var != 0) {
      ... destruction of variable being destroyed
    }

The sequence is inserted at *insert_location.  *insert_location is updated
for further insertion following the "if".  *insert_location2 is set for
insertion within the "if".
*/
{
  an_expr_node_ptr test_var_node, compare_node;

  /* Make "test_var != 0". */
  test_var_node = var_rvalue_expr(test_var);
  test_var_node->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                    integer_type((an_integer_kind)ik_int),
                                    test_var_node);
  /* Make an "if" statement and insert it into the program. */
  insert_if_statement(compare_node, insert_location, insert_location2);
}  /* add_last_time_test */


static void turn_statement_into_noop(a_statement_ptr statement)
/*
Convert the indicated statement into a no-op.
*/
{
  /* The general technique is to turn the statement into a block statement
     containing no statements.  If the statement was already a block statement,
     this loses the storage for the old block entry.  However, since it works,
     and since there's no reason to expect this routine to be called for
     block statements, we won't worry about that case. */
  set_statement_kind(statement, (a_statement_kind)stmk_block);
}  /* turn_statement_into_noop */


static void turn_statement_into_block(a_statement_ptr statement)
/*
Turn the indicated statement into a block statement with a copy of the
original statement under it.
*/
{
  a_statement_ptr copy_statement;

  /* Make a copy of the original statement. */
  copy_statement = alloc_statement(statement->kind);
  *copy_statement = *statement;
  copy_statement->next = NULL;
  /* Turn the statement into a block statement. */
  set_statement_kind(statement, (a_statement_kind)stmk_block);
  statement->variant.block.statements = copy_statement;
  statement->seq_number = 0;
}  /* turn_statement_into_block */


static void turn_branch_into_block(a_statement_ptr        statement,
                                   an_insert_location_ptr insert_location)
/*
Turn a branch statement (goto or return) into a block, and set *insert_location
so that statements can be inserted at the beginning of the block (i.e.,
in front of the original branch statement).
*/
{
  turn_statement_into_block(statement);
  /* We know the original statement is a branch of some sort, so
     the end of the block is not reachable. */
  statement->variant.block.extra_info->end_of_block_reachable = FALSE;
  /* Insert at the start of the added block. */
  set_block_start_insert_location(statement, insert_location);
}  /* turn_branch_into_block */


static void gen_one_required_destructor_call(
                                a_required_destructor_call_ptr rdcp,
                                an_insert_location_ptr         insert_location)
/*
Generate code for the required destructor call described by rdcp.  The code
is inserted at *insert_location and *insert_location is updated.
*/
{
  an_insert_location     insert_location2;
  an_insert_location_ptr effective_insert_loc;

  effective_insert_loc = insert_location;
  /* If the entity is a local static variable or a conditionally-created
     temporary, generate an "if" statement to test whether or not the
     variable was ever initialized.  Only do the destruction if it
     was. */
  if (rdcp->first_time_test_var != NULL) {
    add_last_time_test(rdcp->first_time_test_var, 
                       insert_location,
                       &insert_location2);
    effective_insert_loc = &insert_location2;
  }  /* if */
  lower_destructor_dynamic_init(&rdcp->dynamic_init,
                                &rdcp->init_pos_descr,
                                effective_insert_loc);
}  /* gen_one_required_destructor_call */


static void gen_and_remove_required_destructor_calls_up_to(
                                a_required_destructor_call_ptr stop_before,
                                an_insert_location_ptr         insert_location)
/*
Generate code for and then remove the required destructor call entries on the
list for the current context, up to before the entry stop_before.  stop_before
can be NULL to indicate the entire list.  The code is inserted at
*insert_location and *insert_location is updated.
*/
{
  a_required_destructor_call_ptr rdcp;

  /* Go through the list of required destructor calls, stopping when the
     indicated entry if reached.  Recall that the list is built by adding
     to its front, so the entries at the front are the later entries,
     those we want to process and remove. */
  for (rdcp = curr_context->required_destructor_calls;
       rdcp != stop_before;
       rdcp = rdcp->next) {
    gen_one_required_destructor_call(rdcp, insert_location);
  }  /* for */
  /* Remove the entries from the list. */
  curr_context->required_destructor_calls = stop_before;
}  /* gen_and_remove_required_destructor_calls_up_to */


static void lower_stmk_init(a_statement_ptr statement)
/*
Generate code for a stmk_init (dynamic initialization) statement.
*/
{
  a_dynamic_init_ptr dip = statement->variant.dynamic_init;
  a_boolean          non_C_case;

  curr_context->latest_dynamic_init_processed = dip;
  /* Only lower the cases that do not come up in C: */
  non_C_case = FALSE;
  if (dip->destructor != NULL) {
    /* Initialization with a later destructor. */
    non_C_case = TRUE;
  }  /* if */
  switch (dip->kind) {
    case dik_none:
      break;
    case dik_constant:
      break;
    case dik_expression:
      if (dip->variable->storage_class == (a_storage_class)sc_static) {
        /* Initialization of a local static variable to an expression, as in
               int f() {static int i = j+1;}
        */
        non_C_case = TRUE;
      }  /* if */
      break;
    case dik_call_returning_class_via_cctor:
      /* Initialization from class returned via copy constructor. */
      non_C_case = TRUE;
      break;
    case dik_constructor:
      /* Initialization using a constructor. */
      non_C_case = TRUE;
      break;
    case dik_nonconstant_aggregate:
      /* Initialization to a nonconstant aggregate, as in
                  auto int a[3] = {1, i+1, 3};
      */
      non_C_case = TRUE;
      break;
#if CHECKING
    default:
      internal_error("lower_stmk_init: bad dynamic init kind");
#endif /* CHECKING */
  }  /* switch */
  if (non_C_case) {
    /* Rewrite a non-C case. */
    an_insert_location insert_location;
    a_boolean          keep_dynamic_init;
    an_init_pos_descr  ipd;
    a_variable_ptr     first_time_test_var = NULL;
    a_required_destructor_call_ptr
                       required_destructor_calls_before;

    set_insert_location(statement, &insert_location);
    set_var_init_pos_descr(dip->variable, &ipd);
    /* If the variable is a local static, add a first-time flag and a
       test. */
    if (dip->variable->storage_class == (a_storage_class)sc_static) {
      add_first_time_test(&insert_location, &first_time_test_var);
      /* Remember the last required destruction at this point.  Anything
         added within the conditional should be generated and removed at
         the end of the conditional. */
      required_destructor_calls_before =
                                       curr_context->required_destructor_calls;
    }  /* if */
    lower_dynamic_init(dip, &ipd, first_time_test_var,
                       /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    if (!keep_dynamic_init) {
      /* Delete the stmk_init statement. */
      turn_statement_into_noop(statement);
    }  /* if */
    if (first_time_test_var != NULL) {
      /* Generate any required destructor calls for temporaries built within
         a first-time test conditional section. */
      gen_and_remove_required_destructor_calls_up_to(
                                              required_destructor_calls_before,
                                              &insert_location);
    }  /* if */
  } else {
    /* Normal C case.  Lower the subtree if any. */
    switch (dip->kind) {
      case dik_constant:
        lower_constant(dip->variant.constant);
        break;
      case dik_expression:
        lower_normal_expr(dip->variant.expression);
        break;
#if CHECKING
      default:
        internal_error("lower_stmk_init: bad dynamic init kind (2)");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_stmk_init */


static void gen_required_destructor_calls(
                                        a_context_ptr          outer_context,
                                        an_insert_location_ptr insert_location)
/*
Generate any destructor calls required to exit from the contexts indicated
by curr_context through outer_context, inclusive.  Insert the code for
the destructor calls at *insert_location.
*/
{
  a_context_ptr                  context_ptr;
  a_required_destructor_call_ptr rdcp;

  /* Loop outward through the indicated contexts. */
  for (context_ptr = curr_context;; context_ptr = context_ptr->parent) {
    /* Loop through the list of required destructor calls. */
    for (rdcp = context_ptr->required_destructor_calls;
         rdcp != NULL;
         rdcp = rdcp->next) {
      gen_one_required_destructor_call(rdcp, insert_location);
    }  /* for */
    /* Stop when the outer context is reached. */
    if (context_ptr == outer_context) break;
  }  /* for */
}  /* gen_required_destructor_calls */


static a_boolean any_required_destructor_calls(a_context_ptr outer_context)
/*
Return TRUE if any destructor calls are required to exit from the contexts
indicated by curr_context through outer_context, inclusive.
*/
{
  a_boolean     any_required = FALSE;
  a_context_ptr context_ptr;

  /* Loop outward through the indicated scopes. */
  for (context_ptr = curr_context;; context_ptr = context_ptr->parent) {
    if (context_ptr->required_destructor_calls != NULL) {
      /* There are some required destructor calls. */
      any_required = TRUE;
      break;
    }  /* if */
    /* Stop when the outer context is reached. */
    if (context_ptr == outer_context) break;
  }  /* for */
  return any_required;
}  /* any_required_destructor_calls */


static a_boolean block_is_on_parent_list(a_statement_ptr block,
                                         a_statement_ptr block_list)
/*
Return TRUE if the block statement "block" is on the list of blocks
and their parents headed by block_list.
*/
{
  a_boolean on_list = FALSE;

  for (; block_list != NULL;
       block_list = block_list->variant.block.extra_info->parent_block) {
    if (block_list == block) {
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* block_is_on_parent_list */


static void gen_goto_required_destructor_calls(a_statement_ptr statement)
/*
Generate any destructor calls required preceding the indicated goto statement.
*/
{
  a_statement_ptr    goto_block, label_block;
  a_context_ptr      goto_context, outermost_context_being_exited;
  an_insert_location insert_location;
  a_boolean          any_label_block_destructor_calls_needed;
  a_boolean          any_exited_block_destructor_calls_needed;
  unsigned long      label_number;
  a_required_destructor_call_ptr
                     rdcp;

  goto_context = curr_context;
  label_block = statement->variant.label->parent_block;
#if CHECKING
  if (label_block == NULL) {
    internal_error(
       "gen_goto_required_destructor_calls: goto label has NULL parent_block");
  }  /* if */
#endif /* CHECKING */
  outermost_context_being_exited = NULL;
  for (;; goto_context = goto_context->parent) {
    goto_block = goto_context->scope->assoc_block;
    /* End the loop when we find a block that both the goto and the
       label are inside of.  At the worst, the block for the function
       is such a block, so the loop would end on that block. */
    if (block_is_on_parent_list(goto_block, label_block)) break;
    /* Here, goto_context is a context that the goto is inside of whose
       block does not appear on the label block list; therefore, we are
       leaving the block.  */
    outermost_context_being_exited = goto_context;
  }  /* for */
  /* See if any destructor calls are needed. */
  any_label_block_destructor_calls_needed = FALSE;
  any_exited_block_destructor_calls_needed = FALSE;
  if (outermost_context_being_exited != NULL) {
    /* Some contexts are being exited.  See if any destructor calls are
       needed on leaving those contexts. */
    any_exited_block_destructor_calls_needed = 
                 any_required_destructor_calls(outermost_context_being_exited);
  }  /* if */
  if (label_block == goto_block) {
    /* The label is in the block that is the first one shared with the goto
       context.  Check for a case where the goto branches to a label
       preceding some initializations:
         struct A { ~A(); };
         void m() {
           label:
             A x;
             goto label;  // should destroy x
         }
    */
    a_statement_ptr label_stmt = statement->variant.label->variant.exec_stmt;
    a_statement_ptr temp_stmt;

    /* Determine the label number of the label within its block. */
    label_number = 0;
    for (temp_stmt = label_block->variant.block.statements;
         ;
         temp_stmt = temp_stmt->next) {
#if CHECKING
      if (temp_stmt == NULL) {
        internal_error("gen_goto_required_destructor_calls: label not found");
      }  /* if */
#endif /* CHECKING */
      if (temp_stmt->kind == (a_statement_kind)stmk_label) {
        label_number++;
        if (temp_stmt == label_stmt) break;
      }  /* if */
    }  /* for */
    /* See if any of the required destructions were initialized after the
       label's definition.  If so, they need to be generated. */
    for (rdcp = goto_context->required_destructor_calls;
         rdcp != NULL;
         rdcp = rdcp->next) {
      if (rdcp->label_count >= label_number) {
        any_label_block_destructor_calls_needed = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (any_exited_block_destructor_calls_needed ||
      any_label_block_destructor_calls_needed) {
    /* Some destructor calls are needed.  Generate them. */
    /* Turn the goto into a block so code can be inserted in front of it. */
    turn_branch_into_block(statement, &insert_location);
    if (any_exited_block_destructor_calls_needed) {
      gen_required_destructor_calls(outermost_context_being_exited,
                                    &insert_location);
    }  /* if */
    if (any_label_block_destructor_calls_needed) {
      /* Generate destructor calls corresponding to any initializations made
         after the label in the same block. */
      for (rdcp = goto_context->required_destructor_calls;
           rdcp != NULL && rdcp->label_count >= label_number;
           rdcp = rdcp->next) {
        gen_one_required_destructor_call(rdcp, &insert_location);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* gen_goto_required_destructor_calls */


static void pop_block_scope_context(a_statement_ptr last_statement)
/*
The current context is a context for a block statement.  Generate any
destructor calls required at the end of the block and pop the context.
last_statement points to the last statement within the block, or is
NULL if there are no statements in the block.
*/
{
  a_statement_ptr    block_statement;
  an_insert_location insert_location;

  block_statement = curr_context->scope->assoc_block;
  /* Insert any required destructor calls after the last statement
     in the block if the end of the block is reachable. */
  if (block_statement->variant.block.extra_info->end_of_block_reachable) {
    if (last_statement == NULL) {
      /* The block is empty, so insert at its beginning. */
      set_block_start_insert_location(block_statement, &insert_location);
    } else {
      /* Insert after the last statement. */
      set_insert_location(last_statement, &insert_location);
    }  /* if */
    gen_required_destructor_calls(curr_context, &insert_location);
  }  /* if */
  pop_context();
}  /* pop_block_scope_context */


static void lower_statement(a_statement_ptr statement)
/*
Do IL lowering of the indicated statement and everything under it.
*/
{
  a_routine_ptr      curr_routine;
  a_context          context;
  a_context_ptr      switch_context;
  a_scope_ptr        scope;
  an_insert_location insert_location;
  a_statement_ptr    last_statement, body_statement;
  a_boolean          make_block;
  an_expr_node_ptr   return_expr;
  a_variable_ptr     temp_var;
  a_dynamic_init_ptr dip;

  if (statement != NULL) {
    /* Track the source position for internal errors. */
    error_position.seq = statement->seq_number;
    error_position.column = 0;
    if (statement->expr != NULL) lower_normal_expr(statement->expr);
    switch (statement->kind) {
      case stmk_expr:
      case stmk_asm:
        /* No processing required. */
        break;
      case stmk_goto:
        /* Generate any destructor calls required on exit from any blocks
           that the goto is inside of but the label is not. */
        gen_goto_required_destructor_calls(statement);
        break;
      case stmk_label:
        /* Keep track of the number of labels encountered in this block.
           This is needed when generating destructor calls on gotos
           backward in a block. */
        curr_context->label_count++;
        curr_context->latest_label_statement_processed = statement;
        curr_context->dynamic_init_preceding_label =
                                   curr_context->latest_dynamic_init_processed;
        break;
      case stmk_return:
        dip = statement->variant.return_dynamic_init;
        return_expr = statement->expr;
        /* Keep track of whether or not we have already turned the return
           statement into a block.  We haven't so far. */
        make_block = TRUE;
        curr_routine = nearest_function_scope->variant.routine.ptr;
        if (curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor) {
          /* In a destructor, change returns into gotos to the epilogue
             label. */
          set_statement_kind(statement, (a_statement_kind)stmk_goto);
          statement->expr = NULL;
          statement->variant.label = destructor_epilogue_label;
          count_of_refs_to_destructor_epilogue_label++;
        } else {
          if (dip != NULL) {
            /* This routine returns its value via a copy constructor.
               The dynamic initialization entry indicates the operation to
               be done. */
            an_init_pos_descr ipd;
            a_boolean         keep_dynamic_init;
            statement->variant.return_dynamic_init = NULL;
            set_var_indirect_init_pos_descr(return_value_pointer_variable,
                                            &ipd);
            /* Put the return statement under a block so we can insert in
               front of it. */
            turn_branch_into_block(statement, &insert_location);
            make_block = FALSE;
            lower_dynamic_init(dip, &ipd,
                               /*first_time_test_var=*/(a_variable_ptr)NULL,
                               /*is_expr_temporary=*/FALSE,
                               (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                               (a_constructor_init_ptr)NULL,
                               &insert_location, &keep_dynamic_init);
            check_assertion(!keep_dynamic_init);
          }  /* if */
        }  /* if */
        if (any_required_destructor_calls(nearest_function_context)) {
          /* Generate any destructor calls required on exit from the
             routine.  If the return has an expression, it must be evaluated
             before the destructor calls are done, so change
               return expr;
             into
               {temp = expr; destructor-calls; return temp;}
          */
          if (return_expr != NULL) {
            /* There is a return expression, so use a temporary.  Note that
               the return type cannot call for a copy constructor, or the
               routine would be returning its value via an added parameter. */
            temp_var = make_temporary(return_expr->type);
            /* Change the return statement to return the temporary's value. */
            statement->expr = var_rvalue_expr(temp_var);
            /* Put the return statement under a block so we can insert in
               front of it. */
            /* Note that if we executed the similar code above we wouldn't
               be executing the code here, because a return can have either
               a dynamic init entry or an expression, but not both. */
            check_assertion(make_block);
            turn_branch_into_block(statement, &insert_location);
            make_block = FALSE;
            /* Insert the "temp = return-expr;" statement. */
            (void)insert_var_assignment_statement(
                                   temp_var,
                                   lowered_assignment_operator(temp_var->type),
                                   return_expr, &insert_location);
          }  /* if */
          if (make_block) {
            /* Turn the return into a block so that code can be inserted
               in front of the return. */
            turn_branch_into_block(statement, &insert_location);
          }  /* if */
          gen_required_destructor_calls(nearest_function_context,
                                        &insert_location);
        }  /* if */
        break;
      case stmk_if:
        lower_statement(statement->variant.if_stmt.then_statement);
        lower_statement(statement->variant.if_stmt.else_statement);
        break;
      case stmk_while:
      case stmk_end_test_while:
        lower_statement(statement->variant.loop_statement);
        break;
      case stmk_block:
        /* Push a block context around the processing of the block.
           Do not do that if the block has no associated scope.
           Note that the same test ensures that no push is done here for the
           topmost block in a function (it has a NULL assoc_scope); the
           push_context has already been done in lower_scope for that case. */
        scope = statement->variant.block.extra_info->assoc_scope;
        if (scope != NULL) push_context(&context, scope);
        lower_statement_list(statement->variant.block.statements,
                             &last_statement);
        /* Generate any required destructor calls and pop the context. */
        if (scope != NULL) pop_block_scope_context(last_statement);
        break;
      case stmk_switch:
        /* If there is a body statement and it has a scope, push it as
           context around the processing of the switch clauses. */
        scope = NULL;
        switch_context = NULL;
        body_statement = statement->variant.switch_stmt.body_statement;
        if (body_statement != NULL &&
            body_statement->kind == (a_statement_kind)stmk_block) {
          scope = body_statement->variant.block.extra_info->assoc_scope;
          if (scope != NULL) {
            push_context(&context, scope);
            switch_context = curr_context;
          }  /* if */
          body_statement = body_statement->variant.block.statements;
        }  /* if */
        lower_statement_list(body_statement, &last_statement);
        lower_switch_clause_list(statement->variant.switch_stmt.clause_list,
                                 switch_context);
        /* Generate any required destructor calls and pop the context. */
        if (scope != NULL) pop_block_scope_context(last_statement);
        break;
      case stmk_init:
        lower_stmk_init(statement);
        break;
#if CHECKING
      default:
        internal_error("lower_statement: bad kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
}  /* lower_statement */


static void lower_file_scope_dynamic_inits(void)
/*
Do lowering on the file-scope dynamic initializations list.
*/
{
  a_dynamic_init_ptr dip;
  an_insert_location insert_location;
  a_boolean          keep_dynamic_init;
  a_scope_ptr        file_scope = il_header.primary_scope, scope;
  an_init_pos_descr  ipd;
  a_context          context;

  dip = file_scope->dynamic_inits;
  if (dip != NULL) {
    /* There are some file-scope dynamic initializations.  Generate a routine
       containing them. */
    scope = file_scope_init_insert_location(&insert_location);
    push_context(&context, scope);
    switch_il_region(file_scope_init_routine_il_region);
    processing_file_scope_init_routine = TRUE;
    for (; dip != NULL; dip = dip->next) {
      set_var_init_pos_descr(dip->variable, &ipd);
      lower_dynamic_init(dip, &ipd,
                         /*first_time_test_var=*/(a_variable_ptr)NULL,
                         /*is_expr_temporary=*/FALSE,
                         (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                         (a_constructor_init_ptr)NULL,
                         &insert_location, &keep_dynamic_init);
#if CHECKING
      if (keep_dynamic_init) {
        internal_error(
               "lower_file_scope_dynamic_inits: keep_dynamic_init unexpected");
      }  /* if */
#endif /* CHECKING */
    }  /* for */
    processing_file_scope_init_routine = FALSE;
    pop_context();
    done_with_memory_region(file_scope_init_routine_il_region);
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
    file_scope->dynamic_inits = NULL;
  }  /* if */
  /* Put destructor calls for local static variables on the front of
     the file-scope list. */
  if (destructor_calls_for_local_static_variables != NULL) {
    end_destructor_calls_for_local_static_variables->next =
                                       curr_context->required_destructor_calls;
    curr_context->required_destructor_calls =
                                   destructor_calls_for_local_static_variables;
  }  /* if */
  /* Generate any destructor calls associated with the file scope. */
  if (curr_context->required_destructor_calls != NULL) {
    /* There are some file-scope required destructor calls.  Generate a
       routine containing them. */
    scope = file_scope_term_insert_location(&insert_location);
    push_context(&context, scope);
    switch_il_region(file_scope_term_routine_il_region);
    gen_required_destructor_calls(file_scope_context, &insert_location);
    pop_context();
    done_with_memory_region(file_scope_term_routine_il_region);
    switch_il_region(FILE_SCOPE_REGION_NUMBER);
  }  /* if */
}  /* lower_file_scope_dynamic_inits */


static a_variable_ptr implicit_virtual_base_parameter(
                                                a_type_ptr     class_type,
                                                a_type_ptr     base_class_type,
                                                a_variable_ptr this_param_var)
/*
Find the implicit virtual base class parameter under class_type that is
for the virtual base class base_class_type and return a pointer to it.
this_param_var points to the "this" parameter variable for class_type;
the implicit parameters follow it.
*/
{
  a_variable_ptr   vbase_param_var;
  a_base_class_ptr bcp;

  /* Find the base class entry under the main class that is for this
     same virtual base class.  While doing so, step through the added
     parameter entries so that at the end of the loop vbase_param_var
     is the added parameter for the base class indicated by base_class_type. */
  vbase_param_var = this_param_var;
  for (bcp = class_type->variant.class_struct_union.extra_info->base_classes;
       ;
       bcp = bcp->next) {
#if CHECKING
    if (bcp == NULL) {
      /* Virtual base class of a base class must be a virtual base class
         of the main class, by definition. */
      internal_error(
              "implicit_virtual_base_parameter: virtual base class not found");
    }  /* if */
#endif /* CHECKING */
    if (bcp->is_virtual) {
      vbase_param_var = vbase_param_var->next;
      /* Exit the inner loop when we've found the base class entry in
         the main class that is for the virtual base class of interest. */
      if (bcp->type == base_class_type) break;
    }  /* if */
  }  /* for */
#if CHECKING
  { a_type_ptr param_base_type =
                       f_skip_typerefs(type_pointed_to(vbase_param_var->type));
    if (param_base_type != base_class_type &&
        param_base_type != base_class_type->variant.class_struct_union.
                                               extra_info->type_as_subobject) {
      internal_error(
                    "implicit_virtual_base_parameter: param type not correct");
    }  /* if */
  }
#endif /* CHECKING */
  return vbase_param_var;
}  /* implicit_virtual_base_parameter */


static void add_member_copy(an_init_pos_descr_ptr  dest,
                            a_constructor_init_ptr ctor_init,
                            an_insert_location_ptr insert_location)
/*
Make a block move to implement a copy in a copy constructor.  dest
describes the destination of the move.  ctor_init is the constructor
initialization entry, which gives (along with var_for_copy_constructor_source)
the information needed to build the source expression.  Insert the
statement at *insert_location and update *insert_location.
*/
{
  an_expr_node_ptr      source_node, dest_node;
  an_init_pos_descr     ipd;
  an_init_pos_modifier  ipm;

  /* Make an expression for the address of the destination entity. */
  dest_node = make_init_entity_node(dest);
  /* Make an expression for the address of the source entity.  This is
     done by getting the source parameter of the copy constructor and
     modifying it to select the same subobject as in the destination. */
  set_var_indirect_init_pos_descr(var_for_copy_constructor_source(), &ipd);
  modify_ctor_init_pos_descr(ctor_init, &ipd, &ipm);
  source_node = make_init_entity_node(&ipd);
  /* Make an assignment statement. */
  (void)insert_assignment_statement(dest_node,
                                    (an_expr_operator_kind)eok_bassign,
                                    source_node,
                                    insert_location);
}  /* add_member_copy */


static void lower_ctor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              use_implicit_param,
                            a_type_ptr             class_type,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init.
this_param_var is the "this" parameter variable for the overall object
being initialized, whose class is class_type.  Implicit parameters for
virtual base classes, if any, follow the this_param_var.  If use_implicit_param
is TRUE, the entity being initialized is a virtual base class of class_type
and its address is available in an implicit parameter.  The statement(s)
created are inserted at *insert_location, and *insert_location is updated.
*/
{
  a_type_ptr           base_class_type;
  a_variable_ptr       vbase_param_var;
  a_base_class_ptr     bcp;
  an_expr_node_ptr     implied_arg_node;
  an_expr_node_ptr     implied_arg_list = NULL, end_implied_arg_list = NULL;
  a_dynamic_init_ptr   dip;
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_boolean            keep_dynamic_init;

  dip = ctor_init->initializer;
  if (ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class ||
      ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class) {
    /* Initializing a base class. */
    base_class_type = ctor_init->variant.base_class->type;
    /* Develop a position description for the entity to initialize. */
    if (use_implicit_param) {
      /* The sub-entity is a virtual base class and there is a parameter
         pointing to it. */
      vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                        base_class_type,
                                                        this_param_var);
      set_var_indirect_init_pos_descr(vbase_param_var, &ipd);
    } else {
      /* Simple case; develop the entity position description. */
      develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
    }  /* if */
    /* For a base class initialized by a constructor call, build a list
       of implicit virtual base class pointer arguments.  The required
       entries are expressions providing the value of the associated virtual
       base class pointer parameter for each virtual base class of the base
       class. */
    if (dip->kind == (a_dynamic_init_kind)dik_constructor) {
      for (bcp = base_class_type->variant.class_struct_union.extra_info->
                                                                  base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->is_virtual) {
          /* Find the implicit virtual base parameter under the main class
             that is for this same virtual base class. */
          vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                            bcp->type,
                                                            this_param_var);
          /* Build an expression specifying the value of the appropriate
             virtual base class parameter, and add it to the list. */
          implied_arg_node = var_rvalue_expr(vbase_param_var);
          if (implied_arg_list == NULL) {
            implied_arg_list = implied_arg_node;
          } else {
            end_implied_arg_list->next = implied_arg_node;
          }  /* if */
          end_implied_arg_list = implied_arg_node;
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    /* Initializing something other than a base class. */
    /* Develop a position description for the entity to initialize. */
    develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  }  /* if */
  /* Generate the code to do the initialization. */
  if (dip->kind == (a_dynamic_init_kind)dik_member_copy ||
      dip->kind == (a_dynamic_init_kind)dik_base_class_copy) {
    /* Special case -- copying a member or base class in a copy constructor. */
    add_member_copy(&ipd, ctor_init, insert_location);
  } else {
    lower_dynamic_init(dip, &ipd, /*first_time_test_var=*/(a_variable_ptr)NULL,
                       /*is_expr_temporary=*/FALSE,
                       implied_arg_list, end_implied_arg_list, ctor_init,
                       insert_location, &keep_dynamic_init);
#if CHECKING
    if (keep_dynamic_init) {
      internal_error("lower_ctor_init: keep_dynamic_init unexpected");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
}  /* lower_ctor_init */


static void add_constructor_wrapper_code(a_scope_ptr        scope,
                                         an_insert_location *insert_location)
/*
Insert constructor wrapper code at the indicated location in the indicated
constructor scope.  The insert location is usually at the beginning of the
constructor, but may instead be after an assignment to "this".
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var, vbase_param_var;
  a_type_ptr             class_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
  a_constant             null_constant;
  an_insert_location     insert_location2;
  an_expr_node_ptr       null_constant_node, vbase_param_node, compare_node;
  an_expr_node_ptr       vaddr_node, vbptr_node, vtbl_addr_node, vptr_node;
  a_variable_ptr         primary_vtbl_var, vtbl_var;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the constructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the constructor routine.

     [If current class has any virtual base classes:]
       If the first added parameter == NULL (indicating a complete object is
           being initialized and virtual base classes must be constructed):
         [For each virtual base class of the current class:]
           Set the parameter to the address of the virtual base class.
         [endfor]
         [For each virtual base class on the ctor-initializer list:]
           Call the constructor for the base class (arguments as indicated by
               the ctor-initializer list, plus any virtual base class pointer
               arguments, using the added parameters for those).
         [endfor]
       endif
       [For each virtual base class of the current class:]
         If the virtual base class pointer for the base class is allocated
             in the current class, initialize it to point to the base class.
             (The pointers allocated in base classes are set by the constructor
             calls for those base classes.)
       [endfor]
     [endif]
     [For each initialized direct nonvirtual base class (entries for these
         appear as the middle of the ctor-initializer list):]
       Call the constructor for the base class (arguments as indicated by
         the ctor-initializer list, plus any virtual base class pointer
         arguments, using the added parameters for those).
     [endfor]
     [For each initialized data member (entries for these are the rest
         of the ctor-initializer list):]
       Do the initialization (a constructor call or some other dynamic
           initialization).
     [endfor]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed using the virtual base class
             pointer parameters.
       [endif]
     [endfor]
  */
  /* The constructor_inits list contains a list of initializations.  Each
     initialization either appeared explicitly in the source or is a default
     initialization supplied by the front end.  Every base class and member
     that requires a constructor appears, in the order (1) virtual base
     classes, (2) normal base classes, (3) data members.  The order within
     each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  scope->variant.routine.constructor_inits = NULL;
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  class_type =
            scope->variant.routine.ptr->source_corresp.class_of_which_a_member;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* Put out code that tests whether or not the virtual base classes need
       to be initialized.  This is done by testing whether or not the
       first added parameter is NULL. */
    vbase_param_var = this_param_var->next;
    /* Make a NULL pointer constant of the right type. */
    make_zero_of_proper_type(vbase_param_var->type, &null_constant);
    /* Make an expression node pointing to the NULL constant. */
    null_constant_node = alloc_node_for_constant(&null_constant);
    /* Make an expression node for the parameter. */
    vbase_param_node = var_rvalue_expr(vbase_param_var);
    /* Make a node comparing the parameter against NULL. */
    vbase_param_node->next = null_constant_node;
    compare_node = make_operator_node((an_expr_operator_kind)eok_peq,
                                      integer_type((an_integer_kind)ik_int),
                                      vbase_param_node);
    /* Make an "if" statement with a block statement under it:
         if (param == NULL) {}
                             ^--- additional statements will be inserted.
    */
    insert_if_statement(compare_node, insert_location, &insert_location2);
    /* Set the added parameters to the addresses of the virtual base
       classes. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Make an expression whose value is the address of the virtual
           base class. */
        vaddr_node = make_cobj_vbase_class_lvalue_from_var(this_param_var,
                                                           bcp);
        /* Add a cast if necessary to convert from a pointer to the base
           class type to a pointer to the type-as-subobject for the base
           class type. */
        vaddr_node = add_cast_if_necessary(vaddr_node, vbase_param_var->type);
        /* Make an assignment to set the virtual base class parameter. */
        (void)insert_var_assignment_statement(vbase_param_var,
                                            (an_expr_operator_kind)eok_passign,
                                              vaddr_node,
                                              &insert_location2);
        /* Move on to the next added parameter for the next iteration of
           the loop. */
        vbase_param_var = vbase_param_var->next;
      }  /* if */
    }  /* for */
    /* Initialize any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL &&
            ctor_init->kind == (a_constructor_init_kind)cik_virtual_base_class;
         ctor_init = ctor_init->next) {
      lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/TRUE,
                      class_type, &insert_location2);
    }  /* for */
    /* Note that the "if" created above effectively ends here.  The code
       created below is executed even when we do not have a complete object. */
    /* For each virtual base class of the current class, set the
       virtual base class pointer in the current class to point to the value
       of the associated virtual base class parameter, i.e., the address
       of the virtual base class. */
    vbase_param_var = this_param_var;
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        vbase_param_var = vbase_param_var->next;
        /* Do not set the pointer if it is shared with a base class --
           the base class constructor has already or will set it. */
        if (bcp->pointer_base_class == NULL) {
          /* Make an expression for the value of the implicit parameter. */
          vbase_param_node = var_rvalue_expr(vbase_param_var);
          /* Make an expression node for the address of the virtual base
             class pointer. */
          vbptr_node = make_vbptr_field_lvalue_from_var(this_param_var, bcp);
          /* Add a cast if necessary to convert from a pointer to the
             type-as_subobject for the base class type to a pointer to the
             base class type. */
          vbase_param_node = add_cast_if_necessary(vbase_param_node,
                                            type_pointed_to(vbptr_node->type));
          /* Make an assignment statement that copies the implicit parameter
             value (set earlier in the constructor code) into the virtual
             base class pointer. */
          (void)insert_assignment_statement(vbptr_node,
                                            (an_expr_operator_kind)eok_passign,
                                            vbase_param_node,
                                            insert_location);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Generate initialization for each non-virtual base class that appears on
     the ctor_init list. */
  for (; ctor_init != NULL &&
          ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, insert_location);
  }  /* for */
  /* Generate initialization for each data member that appears on the
     ctor_init list. */
  for (; ctor_init != NULL; ctor_init = ctor_init->next) {
    lower_ctor_init(ctor_init, this_param_var, /*use_implicit_param=*/FALSE,
                    class_type, insert_location);
  }  /* for */
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  primary_vtbl_var = ctsp->virtual_function_table_var;
  if (primary_vtbl_var != NULL) {
    /* Assign the primary virtual table address to the virtual table pointer
       in the current class. */
    vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var);
    primary_vtbl_var->address_taken = TRUE;
    primary_vtbl_var->source_corresp.referenced = TRUE;
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_passign,
                                      vtbl_addr_node,
                                      insert_location);
  }  /* if */
  /* Set the virtual function table pointer in any base classes for which
     that is required. */
  /* Loop through the base classes of the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    vtbl_var = bcp->virtual_function_table_var;
    if (vtbl_var != NULL && vtbl_var != primary_vtbl_var) {
      /* The base class's virtual function table pointer must be set to
         reflect the fact that it exists as a subobject inside the current
         class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      vtbl_var->address_taken = TRUE;
      vtbl_var->source_corresp.referenced = TRUE;
      if (!bcp->is_virtual) {
        /* For non-virtual base classes, the base class can be accessed
           directly. */
        vptr_node = make_base_class_lvalue_from_var(this_param_var, bcp);
      } else {
        /* For virtual base classes, access the class by using the implicit
           parameter.  That works even when the current class is not a
           complete object. */
        vbase_param_var = implicit_virtual_base_parameter(class_type,
                                                          bcp->type,
                                                          this_param_var);
        vptr_node = var_rvalue_expr(vbase_param_var);
      }  /* if */
      vptr_node = make_vptr_field_lvalue(vptr_node);
      /* Make and insert the assignment statement. */
      (void)insert_assignment_statement(vptr_node,
                                        (an_expr_operator_kind)eok_passign,
                                        vtbl_addr_node,
                                        insert_location);
    }  /* if */
  }  /* for */
}  /* add_constructor_wrapper_code */


static void add_constructor_params(a_scope_ptr scope)
/*
Add any required implicit parameter variables to the indicated constructor
scope.
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         prev_param_var, vbase_param_var;
  a_type_ptr             class_type, subobject_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_routine_ptr          ctor_routine = scope->variant.routine.ptr;

  prev_param_var = scope->variant.routine.parameters;
  class_type = ctor_routine->source_corresp.class_of_which_a_member;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_type->variant.class_struct_union.any_virtual_base_classes) {
    /* Loop through the virtual base classes of the current class. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* Add a parameter for the virtual base class.  These parameters are
           added after the "this" parameter.  Each one points to the space
           allocated for the associated virtual base class once the base class
           has been constructed, or is NULL if the base class should be
           constructed on this call.  lower_constructor_routine does the
           similar processing for the param type list.  See ARM p. 296. */
        /* Use the type of the virtual base class when used as a subobject. */
        subobject_type = bcp->type->variant.class_struct_union.extra_info->
                                                             type_as_subobject;
        vbase_param_var =
                        make_param_variable(make_pointer_type(subobject_type));
        vbase_param_var->next = prev_param_var->next;
        prev_param_var->next = vbase_param_var;
        prev_param_var = vbase_param_var;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* add_constructor_params */


static void lower_constructor_code(a_scope_ptr scope)
/*
Insert constructor wrapper code around the user code in the indicated
constructor scope.
*/
{
  an_insert_location insert_location;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
  a_routine_ptr      ctor_routine = scope->variant.routine.ptr;

  /* Add the wrapper code at the start of the routine. */
#if ASSIGNMENT_TO_THIS_ALLOWED
  /* If there is an assignment to "this" in the body of the constructor,
     do not issue the wrapper code here; it will be issued after each
     assignment to "this". */
  if (!ctor_routine->assignment_to_this_done) {
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Start off with code to allocate storage if "this" is NULL:
         if (this != NULL || (this = new-rout(size)) != NULL)
       The entire rest of the routine (both wrapper code and user code)
       are placed in the dependent statement of the "if". */
    a_variable_ptr     this_param_var = scope->variant.routine.parameters;
    a_type_ptr         class_type, int_type;
    an_expr_node_ptr   size_node, call_node, assign_node;
    an_expr_node_ptr   new_compare_node, if_node, this_param_node;
    an_expr_node_ptr   null_constant_node, this_compare_node;
    a_statement_ptr    block_stmt;
    a_constant         null_constant;
    a_class_type_supplement_ptr
                       ctsp;
    a_routine_ptr      new_routine;

    /* Make "new-rout(size)". */
    class_type = ctor_routine->source_corresp.class_of_which_a_member;
    ctsp = class_type->variant.class_struct_union.extra_info;
    new_routine = ctsp->assoc_operator_new_routine;
    /* If there is no default new routine for the class, do not put out
       the code.  This happens if the class has a class-specific new but
       not one that takes a single argument. */
    if (new_routine != NULL) {
      size_node = node_for_integer_constant((long)class_type->size,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
      call_node = make_call_node(new_routine, size_node,
                                 /*honor_virtual=*/FALSE);
      /* Make "this = new_rout(size)". */
      call_node = add_cast_if_necessary(call_node, this_param_var->type);
      this_param_node = var_lvalue_expr(this_param_var);
      this_param_node->next = call_node;
      assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                       call_node->type, this_param_node);
      /* Make "(this = new_rout(size)) != NULL". */
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      assign_node->next = null_constant_node;
      int_type = integer_type((an_integer_kind)ik_int);
      new_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                            int_type, assign_node);
      /* Make "this != NULL || (this = new-rout(size)) != NULL". */
      this_param_node = var_rvalue_expr(this_param_var);
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      this_param_node->next = null_constant_node;
      this_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                             int_type, this_param_node);
      this_compare_node->next = new_compare_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_lor,
                                   int_type, this_compare_node);
      /* Make "if (this != NULL || (this = new-rout(size)) != NULL)". */
      enclose_routine_in_if(scope, if_node, &block_stmt, this_param_var);
      set_block_start_insert_location(block_stmt, &insert_location);
    } else {
      /* No default operator new. */
      set_block_start_insert_location(scope->assoc_block, &insert_location);
    }  /* if */
#else /* !NEW_CAN_BE_FOLDED_INTO_CTOR */
    set_block_start_insert_location(scope->assoc_block, &insert_location);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
    /* Add the wrapper code. */
    add_constructor_wrapper_code(scope, &insert_location);
#if NEW_CAN_BE_FOLDED_INTO_CTOR
#if ASSIGNMENT_TO_THIS_ALLOWED
  }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
}  /* lower_constructor_code */


static void lower_dtor_init(a_constructor_init_ptr ctor_init,
                            a_variable_ptr         this_param_var,
                            a_boolean              have_complete_object,
                            an_insert_location_ptr insert_location)
/*
Generate code to implement the constructor_init entry pointed to by ctor_init,
one that appears on the constructor_init list for a destructor.
this_param_var is the "this" parameter variable for the overall object
being destroyed.  If have_complete_object is TRUE, the entity being
initialized is a complete object.  The statements created are inserted
at *insert_location, and *insert_location is updated.
*/
{
  an_init_pos_descr    ipd;
  an_init_pos_modifier ipm;
  a_dynamic_init_ptr   dip;
  an_expr_node_ptr     subentity_node;
  a_boolean            keep_constant;

  /* Develop a position description for the entity to destroy. */
  develop_ctor_init_pos_descr(ctor_init, this_param_var, &ipd, &ipm);
  dip = ctor_init->initializer;
  if (dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    /* Odd case: destructor for an array.  The top level looks like an
       initialization; lower down we will find dynamic init entries for
       the destruction. */
#if CHECKING
    if (ctor_init->kind != (a_constructor_init_kind)cik_field) {
      internal_error("lower_dtor_init: aggr value for non-field");
    }  /* if */
    keep_constant = FALSE;
#endif /* CHECKING */
    lower_dynamic_init_aggregate_constant(dip->variant.constant,
                                          &ipd, (a_variable_ptr)NULL,
                                          /*dtor_case=*/TRUE,
                                          (a_constructor_init_ptr)NULL,
                                          insert_location,
                                          &keep_constant);
#if CHECKING
    if (keep_constant) {
      internal_error("lower_dtor_init: keep_constant unexpected");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* Normal case; generate the code to do the destruction. */
    subentity_node = make_init_entity_node(&ipd);
    add_destructor_call(dip, subentity_node, have_complete_object,
                        insert_location);
  }  /* if */
}  /* lower_dtor_init */


static void add_destructor_params(a_scope_ptr scope)
/*
Add any required implicit parameter variables to the indicated destructor
scope.
*/
{
  a_variable_ptr this_param_var, complete_obj_param_var;

  this_param_var = scope->variant.routine.parameters;
  /* Add a parameter of type int after the "this" parameter.  The new
     parameter has the 0x2 bit on if a complete object is being destroyed,
     and the 0x1 bit on if the storage should be freed. */
  /* lower_destructor_routine adds the parameter to the routine type
     param_type_list. */
  complete_obj_param_var =
                    make_param_variable(integer_type((an_integer_kind)ik_int));
  complete_obj_param_var->next = this_param_var->next;
  this_param_var->next = complete_obj_param_var;
}  /* add_destructor_params */


static void lower_destructor_code(a_scope_ptr scope)
/*
Insert destructor wrapper code around the user code in the indicated
destructor scope.
*/
{
  a_base_class_ptr       bcp;
  a_variable_ptr         this_param_var, complete_obj_param_var;
  a_type_ptr             class_type, int_type;
  a_class_type_supplement_ptr
                         ctsp;
  a_constructor_init_ptr ctor_init;
  an_insert_location     insert_location, insert_location2;
  a_statement_ptr        top_level_stmt, label_stmt;
  a_statement_ptr        return_stmt;
  an_expr_node_ptr       zero_constant_node, complete_obj_param_node;
  an_expr_node_ptr       compare_node;
  an_expr_node_ptr       vtbl_addr_node, vptr_node;
  a_variable_ptr         primary_vtbl_var, vtbl_var;
  a_routine_ptr          dtor_routine = scope->variant.routine.ptr;

  /* The following pseudo-code shows both the processing in this routine
     and the code added to the destructor routine.  Lines enclosed in [...]
     are tests and loops done in the processing in this routine; other
     lines are the code added to the destructor routine.

     [If a delete can be folded into the destructor:]
       If this != NULL test around entire routine.
     [endif]
     [If the current class has any virtual functions:]
       Set the virtual function table pointer in the current class.
     [endif]
     [For each base class of the current class:]
       [If the base class needs a virtual function table instance distinct
           from the derived class instance:]
         Set the virtual function table pointer in the base class.  Virtual
             base classes must be accessed through the virtual base class
             pointer.
       [endif]
     [endfor]
     ... user destructor code goes here ...
         -- returns in the user code are turned into gotos to the following
            code:
     [For each data member on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is TRUE.
     [endfor]
     [For each direct nonvirtual base class on the ctor-initializer list:]
       Call the destructor.  The complete-object implicit argument is FALSE.
     [endfor]
     [If there are any items left on the ctor-initializer list (which
         must be for virtual base classes):]
       If the added parameter != 0 (indicating a complete object is
           being destroyed and virtual base classes must be destroyed):
         [For each virtual base class on the ctor-initializer list:]
           Call the destructor.  The complete-object implicit argument is 0.
         [endfor]
       endif
     [endif]
     If (added parameter & 0x1) != 0:
       delete((void)*this)
     endif
     return;
  */
  int_type = integer_type((an_integer_kind)ik_int);
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* The constructor_inits list contains a list of destructions.  Each
     destruction is a default call supplied by the front end.  Every
     base class and member that requires a destructor appears, in the
     order (1) data members, (2) normal base classes, (3) virtual base
     classes.  The order within each section is source declaration order. */
  ctor_init = scope->variant.routine.constructor_inits;
  scope->variant.routine.constructor_inits = NULL;
  /* Get a pointer to the "this" parameter variable. */
  this_param_var = scope->variant.routine.parameters;
  complete_obj_param_var = this_param_var->next;
  class_type = dtor_routine->source_corresp.class_of_which_a_member;
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* If the current class has any virtual functions, generate code to
     set the virtual function table pointer in the current class. */
  primary_vtbl_var = ctsp->virtual_function_table_var;
  if (primary_vtbl_var != NULL) {
    /* Assign the primary virtual table address to the virtual table pointer
       in the current class. */
    vtbl_addr_node = make_vtbl_address_node(primary_vtbl_var);
    primary_vtbl_var->address_taken = TRUE;
    primary_vtbl_var->source_corresp.referenced = TRUE;
    vptr_node = make_vptr_field_lvalue_from_var(this_param_var);
    (void)insert_assignment_statement(vptr_node,
                                      (an_expr_operator_kind)eok_passign,
                                      vtbl_addr_node,
                                      &insert_location);
  }  /* if */
  /* For each base class of this class that needs it, generate code to
     set the virtual function table pointer in the base class.  This gets
     rid of entries in the virtual function table that point to functions
     of classes derived from the current class. */
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    vtbl_var = bcp->virtual_function_table_var;
    /* If class_type has no virtual functions but the base class does,
       it's possible that the virtual function table pointer in the base class
       is currently set for a class derived from class_type.  Consider:
         struct A {
           virtual void f() {}
           A() {}
           ~A() {}
         };
         struct B : public A {
            B() {}
           ~B() {f();}  // Should call A::f according to ARM 12.7
         };
         struct C : public B {
           void f() {}
         } c;
       Without this special case, when destroying a C object C::f would
       be called.  Don't do this in cfront mode.
    */
    if (vtbl_var == NULL && !cfront_compatibility_mode) {
      a_class_type_supplement_ptr base_class_ctsp =
                              bcp->type->variant.class_struct_union.extra_info;
      /* Use the virtual function table for the base class as a complete
         object, if there is one. */
      vtbl_var = base_class_ctsp->virtual_function_table_var;
    }  /* if */
    if (vtbl_var != NULL && vtbl_var != primary_vtbl_var) {
      /* The base class virtual function table pointer must be set
         to reflect the fact that it exists as a subobject inside the
         current class. */
      vtbl_addr_node = make_vtbl_address_node(vtbl_var);
      vtbl_var->address_taken = TRUE;
      vtbl_var->source_corresp.referenced = TRUE;
      /* Build a node to address the virtual table pointer.  Since we do not
         know whether or not we have a complete object, use the virtual base
         class pointers. */
      vptr_node = make_base_class_lvalue_from_var(this_param_var, bcp);
      vptr_node = make_vptr_field_lvalue(vptr_node);
      /* Make and insert the assignment statement. */
      (void)insert_assignment_statement(vptr_node,
                                        (an_expr_operator_kind)eok_passign,
                                        vtbl_addr_node,
                                        &insert_location);
    }  /* if */
  }  /* for */
  /* The user code in the destructor follows this point, so find the end of the
     top-level statement sequence and add the rest of the code there.  Return
     statements in the body of the destructor have already been turned into
     gotos to destructor_epilogue_label.
  */
  /* Note that there must be at least one statement in the top-level block. */
  for (top_level_stmt = scope->assoc_block->variant.block.statements;
       top_level_stmt->next != NULL;
       top_level_stmt = top_level_stmt->next) {}
  if (top_level_stmt->kind == (a_statement_kind)stmk_goto &&
      top_level_stmt->variant.label == destructor_epilogue_label) {
    /* The last top-level statement is a goto to the epilogue, so it can
       be turned into a no-op. */
    turn_statement_into_noop(top_level_stmt);
    count_of_refs_to_destructor_epilogue_label--;
  }  /* if */
  set_insert_location(top_level_stmt, &insert_location);
  destructor_epilogue_label->source_corresp.referenced =
                             (count_of_refs_to_destructor_epilogue_label != 0);
  if (destructor_epilogue_label->source_corresp.referenced) {
    /* Define the epilogue label. */
    label_stmt = alloc_statement((a_statement_kind)stmk_label);
    label_stmt->variant.label = destructor_epilogue_label;
    destructor_epilogue_label->variant.exec_stmt = label_stmt;
    destructor_epilogue_label->parent_block = scope->assoc_block;
    add_to_labels_list(destructor_epilogue_label);
    insert_statement(label_stmt, &insert_location);
  }  /* if */
  /* Generate a destructor call for each data member that appears on the
     ctor_init list. */
  for (; ctor_init != NULL &&
                         ctor_init->kind == (a_constructor_init_kind)cik_field;
       ctor_init = ctor_init->next) {
    lower_dtor_init(ctor_init, this_param_var, /*have_complete_object=*/TRUE,
                    &insert_location);
  }  /* for */
  /* Generate a destructor call for each non-virtual direct base class
     that appears on the ctor_init list. */
  for (; ctor_init != NULL &&
             ctor_init->kind == (a_constructor_init_kind)cik_direct_base_class;
       ctor_init = ctor_init->next) {
    lower_dtor_init(ctor_init, this_param_var, /*have_complete_object=*/FALSE,
                    &insert_location);
  }  /* for */
  /* If any items remain on the ctor_init list, they must be for virtual
     base classes. */
  if (ctor_init != NULL) {
#if CHECKING
    if (ctor_init->kind != (a_constructor_init_kind)cik_virtual_base_class) {
      internal_error("lower_destructor_code: bad ctor_init item kind");
    }  /* if */
#endif /* CHECKING */
    /* Put out code that tests whether or not the virtual base classes need
       to be destroyed.  This is done by testing whether or not the
       added parameter indicates we have a whole object. */
    /* Note that we can do a "!= 0" test instead of a bit test because the
       0x1 bit (for "free storage") would only be on for a whole object. */
    /* Make an expression node pointing to the zero constant. */
    zero_constant_node = node_for_integer_constant(0L,
                                                   (an_integer_kind)ik_int);
    /* Make an expression node for the parameter. */
    complete_obj_param_node = var_rvalue_expr(complete_obj_param_var);
    /* Make a node comparing the parameter against zero. */
    complete_obj_param_node->next = zero_constant_node;
    compare_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                      int_type, complete_obj_param_node);
    /* Make an "if" statement with a block statement under it:
         if (param != 0) {}
                          ^--- additional statements will be inserted.
    */
    insert_if_statement(compare_node, &insert_location, &insert_location2);
    /* Destroy any virtual base classes on the ctor_init list. */
    for (; ctor_init != NULL; ctor_init = ctor_init->next) {
      lower_dtor_init(ctor_init, this_param_var,
                      /*have_complete_object=*/FALSE, &insert_location2);
    }  /* for */
    /* Note that the "if" created above effectively ends here. */
  }  /* if */
  /* Add code to free the storage if the "free" bit (0x1) is on in the
     added parameter:
       if ((param & 0x1) != 0) delete-routine((void *)this);
  */
  { an_expr_node_ptr this_param_node;
    an_expr_node_ptr and_node, two_constant_node, if_node;
    a_statement_ptr  call_stmt;
    a_routine_ptr    delete_routine;
    a_routine_type_supplement_ptr
                     delete_routine_rtsp;
    a_param_type_ptr param1;

    /* Make "param & 0x1". */
    complete_obj_param_node = var_rvalue_expr(complete_obj_param_var);
    two_constant_node = node_for_integer_constant(1L, (an_integer_kind)ik_int);
    complete_obj_param_node->next = two_constant_node;
    and_node = make_operator_node((an_expr_operator_kind)eok_and,
                                  int_type, complete_obj_param_node);
    /* Make "(param & 0x1) != 0". */
    zero_constant_node = node_for_integer_constant(0L,
                                                   (an_integer_kind)ik_int);
    and_node->next = zero_constant_node;
    if_node = make_operator_node((an_expr_operator_kind)eok_ine,
                                 int_type, and_node);
#if ASSIGNMENT_TO_THIS_ALLOWED
    /* If an assignment to "this" was done in the body of the destructor,
       also test "this != NULL". */
    if (dtor_routine->assignment_to_this_done) {
      an_expr_node_ptr null_constant_node, this_compare_node;
      a_constant       null_constant;
      /* Make "this != NULL". */
      this_param_node = var_rvalue_expr(this_param_var);
      make_zero_of_proper_type(this_param_var->type, &null_constant);
      null_constant_node = alloc_node_for_constant(&null_constant);
      this_param_node->next = null_constant_node;
      this_compare_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                             int_type, this_param_node);
      /* Make "this != NULL && (param & 0x1) != 0". */
      this_compare_node->next = if_node;
      if_node = make_operator_node((an_expr_operator_kind)eok_land,
                                   int_type, this_compare_node);
    }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    /* Make "if ((param & 0x1) != 0)". */
    insert_if_statement(if_node, &insert_location, &insert_location2);
    /* Make "delete-routine((void *)this);" under the "if". */
    this_param_node = var_rvalue_expr(this_param_var);
    this_param_node = add_cast_if_necessary(this_param_node, void_star_type());
    /* If the delete routine takes two arguments, add a second argument
       of type size_t that gives the size of the class. */
    delete_routine = ctsp->assoc_operator_delete_routine;
    check_assertion(delete_routine != NULL);
    delete_routine_rtsp = f_skip_typerefs(delete_routine->type)->
                                                    variant.routine.extra_info;
    param1 = delete_routine_rtsp->param_type_list;
#if CHECKING
    if (param1 == NULL) {
      internal_error("lower_destructor_code: bad delete rout 1st param");
    }  /* if */
#endif /* CHECKING */
    if (param1->next != NULL) {
      /* Two-argument form.  Add a second argument of type size_t that
         indicates the (static) size of the object. */
      this_param_node->next =
              node_for_integer_constant((long)(class_type->size),
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
    }  /* if */
    delete_routine->source_corresp.referenced = TRUE;
    call_stmt = make_call_statement(delete_routine, this_param_node);
    insert_statement(call_stmt, &insert_location2);
  }
  /* Add a return statement at the end of the routine. */
  return_stmt = alloc_statement((a_statement_kind)stmk_return);
  insert_statement(return_stmt, &insert_location);
  { a_statement_ptr  block_stmt;
    an_expr_node_ptr this_param_node, null_constant_node, if_node;
    a_constant       null_constant;

    /* Make and add "if (this != NULL)" around the entire routine body.
       This is needed when the delete call can be folded into the
       destructor call, and is handy to avoid a test before the call
       even when that is not allowed. */
    this_param_node = var_rvalue_expr(this_param_var);
    make_zero_of_proper_type(this_param_var->type, &null_constant);
    null_constant_node = alloc_node_for_constant(&null_constant);
    this_param_node->next = null_constant_node;
    if_node = make_operator_node((an_expr_operator_kind)eok_pne,
                                 int_type, this_param_node);
    /* Make the "if" statement. */
    enclose_routine_in_if(scope, if_node, &block_stmt, (a_variable_ptr)NULL);
  }
}  /* lower_destructor_code */

#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE

static a_boolean local_entities_should_be_promoted(a_scope_ptr scope)
/*
Return TRUE if the local entities of the indicated scope or any of its
subscopes should be promoted to the file scope because they are referenced
by things that will be in the file scope.
*/
{
  a_boolean          promotion_needed = FALSE;
  a_routine_ptr      rout;
  a_variable_ptr     var;
  a_dynamic_init_ptr dip;
  a_type_ptr         type;
  a_scope_ptr        class_scope, block_scope;

  /* See if anything in this scope would force promotion of its types
     or static variables to the file scope. */
  if (scope->kind == (a_scope_kind)sck_function ||
      scope->kind == (a_scope_kind)sck_block) {
    /* Function or block scope. */
    /* Look for local static variables with destructors.  That would force
       the variable to be at the file scope. */
    for (var = scope->variables; var != NULL; var = var->next) {
      if (var->init_kind == (an_init_kind)initk_dynamic) {
        dip = var->initializer.dynamic;
        if (dip->destructor != NULL ||
            dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
          /* The initialization has a destructor, or it's an aggregate that
             might have a ck_dynamic_init with a destructor somewhere in
             it (it's not worth the effort to look). */
          promotion_needed = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
#if CHECKING
    if (scope->kind != (a_scope_kind)sck_class_struct_union) {
      internal_error("local_entities_should_be_promoted: bad scope kind");
    }  /* if */
#endif /* CHECKING */
    /* Class scope. */
    if (scope->constants != NULL ||
        scope->types != NULL ||
        scope->variables != NULL) {
      /* The class has its own local constants, types (including nested
         classes), or variables, so it must be promoted.  This test is
         probably not as refined as it could be, but that's not
         important. */
      promotion_needed = TRUE;
    } else {
      /* See if the class has defined member functions (just the existence
         of member functions is not enough, since all classes have an
         assignment operator function). */
      for (rout = scope->routines; rout != NULL; rout = rout->next) {
        if (rout->assoc_scope != NULL_region_number) {
          promotion_needed = TRUE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  if (!promotion_needed) {
    /* If nothing in this scope forces promotion, look at any subscopes.
       If something there requires promotion, the local entities of this
       scope will also have to be promoted. */
    /* Visit all types to find all class types and their scopes. */
    for (type = scope->types; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          if (local_entities_should_be_promoted(class_scope)) {
            promotion_needed = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Visit all block scopes. */
    for (block_scope = scope->scopes;
         block_scope != NULL;
         block_scope = block_scope->next) {
      if (local_entities_should_be_promoted(block_scope)) {
        promotion_needed = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
  return promotion_needed;
}  /* local_entities_should_be_promoted */


static void promote_local_entities_to_file_scope(a_scope_ptr   scope,
                                                 a_routine_ptr routine)
/*
Promote the local types and static variables of the indicated
scope and its subscopes to the file scope.  The scope is (directly or
indirectly) part of the indicated routine.
*/
{
  a_scope_ptr block_scope;

  /* Promote the local entities to file scope. */
  promote_types(scope, routine);
  promote_variables(scope, routine);
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    promote_local_entities_to_file_scope(block_scope, routine);
  }  /* for */
}  /* promote_local_entities_to_file_scope */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */


static void add_types_list_to_orphaned_types_list(a_type_ptr types_list)
/*
Add the types on the indicated list to the end of the orphaned types list.
*/
{
  an_orphaned_types_list_ptr otlp;

  if (types_list != NULL) {
    /* There are types on this list, so allocate an entry to remember the
       types list pointer. */
    otlp = (an_orphaned_types_list_ptr)
                                      alloc_fe(sizeof(an_orphaned_types_list));
#if DEBUG
    num_orphaned_types_lists_allocated++;
#endif /* DEBUG */
    otlp->next = NULL;
    otlp->types = types_list;
    if (orphaned_types_list == NULL) {
      orphaned_types_list = otlp;
    } else {
      end_orphaned_types_list->next = otlp;
    }  /* if */
    end_orphaned_types_list = otlp;
  }  /* if */
}  /* add_types_list_to_orphaned_types_list */


static void lower_scope_list(a_scope_ptr scope_list)
/*
Do IL lowering of the indicated list of scopes and everything under it.
(Well, actually, everything under it except statements, since they get
handled at the function level.)
*/
{
  a_scope_ptr scope;

  for (scope = scope_list; scope != NULL; scope = scope->next) {
    lower_scope(scope);
  }  /* for */
}  /* lower_scope_list */


static void lower_scope(a_scope_ptr scope)
/*
Do IL lowering of the indicated scope and everything under it.
*/
{
  a_context        context;
  a_routine_ptr    routine;
  a_type_ptr       routine_class_type, routine_type, return_type;
  a_variable_ptr   param_var, var;
  a_routine_type_supplement_ptr
                   rtsp;

  db_enter(2, "lower_scope");
  /* Add a context entry for the scope. */
  push_context(&context, scope);
  if (scope->kind == (a_scope_kind)sck_function) {
    /* The scope is for a function.  Rewrite the parameters if necessary. */
    routine = scope->variant.routine.ptr;
#if DEBUG
    if (debug_level >= 1) {
      if (routine->source_corresp.name != NULL) {
        fprintf(f_debug, "lower_scope: function \"%s\"\n",
                         routine->source_corresp.name);
      } /* if */
    }  /* if */
#endif /* DEBUG */
    routine_type = routine->type;
    routine_type = skip_typerefs(routine_type);
    rtsp = routine_type->variant.routine.extra_info;
    if (rtsp->value_returned_by_cctor) {
      /* If there is an implicit parameter for the return value address,
         add it as an explicit first parameter.  Note that the variable is
         then lowered as part of the parameters below. */
      /* The variable is saved in a global variable for use in processing
         return statements. */
      return_type = routine_type->variant.routine.return_type;
      return_value_pointer_variable =
                           make_param_variable(make_pointer_type(return_type));
      return_value_pointer_variable->next = scope->variant.routine.parameters;
      scope->variant.routine.parameters = return_value_pointer_variable;
    }  /* if */
    if (scope->variant.routine.this_param_variable != NULL) {
      /* If there is an implicit "this" parameter, add it as an explicit
         first parameter.  Note that the variable is then lowered as
         part of the parameters below. */
      param_var = scope->variant.routine.this_param_variable;
      param_var->next = scope->variant.routine.parameters;
      scope->variant.routine.parameters = param_var;
      /* this_param_variable is not cleared.  It's harmless and it's
         helpful to be able to check it when one does not know whether or
         not it has been lowered. */
    }  /* if */
    lower_variable_list(scope->variant.routine.parameters);
    /* For any parameters that are passed by copy constructor, change the
       parameter type to pointer-to-class. */
    for (param_var = scope->variant.routine.parameters;
         param_var != NULL;
         param_var = param_var->next) {
      /* assoc_param_type is NULL on the "this" parameter variable and
         the return value pointer variable. */
      if (param_var->assoc_param_type != NULL &&
          param_var->assoc_param_type->passed_via_copy_constructor) {
        param_var->type = make_pointer_type(param_var->type);
      }  /* if */
    }  /* for */
    /* If the routine is the main program, insert a call of _main at its
       start. */
    if (routine == il_header.main_routine) {
      a_routine_ptr      underscore_main = NULL;
      a_statement_ptr    call_stmt;
      an_insert_location insert_location;
      (void)make_runtime_routine("_main", &underscore_main, void_type());
      call_stmt = make_call_statement(underscore_main,
                                      (an_expr_node_ptr)NULL);
      set_block_start_insert_location(scope->assoc_block, &insert_location);
      insert_statement(call_stmt, &insert_location);
    }  /* if */
  }  /* if */
  lower_constant_list(scope->constants);
  if (lowering_file_scope) {
    /* Lower the file-scope lists or lists for a class scope. */
    lower_type_list(scope->types);
    lower_variable_list(scope->variables);
    if (scope->kind == (a_scope_kind)sck_class_struct_union &&
        allow_anachronisms) {
      /* Change the storage class of static data members that have external
         linkage to sc_unspecified to accommodate the anachronism that
         does not require static data members to be defined somewhere. */
      var = scope->variables;
      if (var != NULL) {
        if (var->source_corresp.class_of_which_a_member->
            variant.class_struct_union.extra_info->template_arg_list != NULL) {
          /* Don't do this for static data members of template classes. */
        } else {
          for (; var != NULL; var = var->next) {
            if (var->storage_class == (a_storage_class)sc_extern) {
              var->storage_class = (a_storage_class)sc_unspecified;
              var->source_corresp.referenced = TRUE;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* Function or block scope. */
    /* In function and block scopes, the types and variables lists point into
       the file scope memory region, and are not lowered at this time.
       They are lowered as part of lowering the file scope memory region. */
    /* Remember the orphaned type list so those types can be processed in
       the proper order at the end of lowering the file scope memory region. */
    add_types_list_to_orphaned_types_list(scope->types);
    /* Also remember the orphaned static variables. */
    if (scope->variables != NULL) {
      record_orphaned_il_entry(scope->variables, iek_variable);
    }  /* if */
  }  /* if */
  lower_variable_list(scope->nonstatic_variables);
  lower_label_list(scope->labels);
  lower_routine_list(scope->routines);
  lower_asm_entry_list(scope->asm_entries);
  /* If the current scope is a function, lower any block scopes within it.
     Note that statements are not lowered during this processing; they are
     handled in the lowering of assoc_block below. */
  lower_scope_list(scope->scopes);
  /* In functions and blocks, the dynamic inits are also pointed to from
     stmk_init statements, so they need not be handled here.  At file scope
     the dynamic inits are not pointed to from elsewhere and must be handled
     now. */
  if (scope->kind == (a_scope_kind)sck_file) {
    lower_file_scope_dynamic_inits();
    make_code_to_invoke_file_scope_init_and_term_routines();
  }  /* if */
  if (scope->kind == (a_scope_kind)sck_function) {
    routine_class_type = routine->source_corresp.class_of_which_a_member;
    if (routine_class_type != NULL) {
      /* Member function. */
      /* Make sure that the class it is a member of has been pre-lowered. */
      prelower_class_type(routine_class_type);
      /* Add implicit parameters to constructors and destructors. */
      if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
        add_constructor_params(scope);
      } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
        add_destructor_params(scope);
        /* For a destructor, make up a label that returns will be changed
           to branch to. */
        destructor_epilogue_label = alloc_label();
        count_of_refs_to_destructor_epilogue_label = 0;
      }  /* if */
    }  /* if */
    /* Lower the executable code. */
    /* Note that the statements are done after the declarations, and they
       are done only for functions, not for blocks; the statements in the
       block scopes get lowered from this call for the function, so it would
       be a mistake to do them again when lowering the block scopes. */
    lower_statement(scope->assoc_block);
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
      /* For a constructor, add wrapper code around the user code. */
      lower_constructor_code(scope);
    } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
      /* For a destructor, add wrapper code around the user code. */
      lower_destructor_code(scope);
    }  /* if */
  }  /* if */
  pop_context();
  db_exit();
}  /* lower_scope */


static void lower_orphaned_entries(void)
/*
Lower any "orphaned" entries in the file scope, entries that are pointed
to only from a function scope, and are therefore in the file scope but
not reachable from the normal file-scope IL tree.
*/
{
  an_orphaned_types_list_ptr otlp;
  an_il_entry_kind           kind;
  char                       *entry_ptr;

  /* First lower the list of types saved by IL lowering itself.  This is
     done as a separate step so that the class types can be processed in
     the original source order. */
  for (otlp = orphaned_types_list; otlp != NULL; otlp = otlp->next) {
    lower_type_list(otlp->types);
  }  /* if */
  /* Now visit all the orphaned entries recorded by the more general scheme. */
  /* Look at each IL entry kind (e.g., types, constants). */
  for (kind = (an_il_entry_kind)0;
       (int)kind < (int)iek_last;
       kind = (an_il_entry_kind)((int)kind + 1)) {
    /* Visit each entry on the list of orphaned entries of that kind. */
    for (entry_ptr = orphaned_file_scope_il_entries[(int)kind].first_entry;
         entry_ptr != NULL; 
         entry_ptr = fs_orphan_pointer_of(entry_ptr)) {
      switch (kind) {
        case iek_type:
          lower_type((a_type_ptr)entry_ptr);
          break;
        case iek_constant:
          lower_constant((a_constant_ptr)entry_ptr);
          break;
        case iek_variable:
          lower_variable((a_variable_ptr)entry_ptr);
          break;
        default:
          /* There may be other kinds of entries recorded by an IL walk,
             but we don't care about them. */
          goto next_kind;
      }  /* switch */
    }  /* for */
next_kind:;
  }  /* for */
}  /* lower_orphaned_entries */


void lower_il_memory_region(a_memory_region_number region_number)
/*
Rewrite the intermediate language in memory region region_number from
C++ to C, so that a C back end can handle it without change.
*/
{
  a_scope_ptr scope;
  a_context   context;

  db_enter(1, "lower_il_memory_region");
  /* The lowering is only needed if the source language is C++, if the
     lowering phase is to be run, and if there have been no errors. */
  if (il_lowering_needed()) {
#if DEBUG
    if (debug_level >= 1) {
      fprintf(f_debug, "Lowering IL in memory region %lu\n",
                       (unsigned long)region_number);
    }  /* if */
#endif /* DEBUG */
    curr_context = nearest_function_context = file_scope_context = NULL;
    nearest_function_scope = NULL;
    switch_il_region(region_number);
    /* Mark entries created during this traversal as having already been
       visited by IL lowering. */
    initial_value_for_il_lowering_flag = !initial_value_for_il_lowering_flag;
    if (region_number == FILE_SCOPE_REGION_NUMBER) {
      /* The file scope. */
      lowering_file_scope = TRUE;
      scope = il_header.primary_scope;
    } else {
      /* A function scope. */
      lowering_file_scope = FALSE;
      scope = il_header.region_scope_entry[region_number];
      /* Put the file-scope context on the context stack so it's above
         the function context. */
      push_context(&context, il_header.primary_scope);
    }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
    if (!lowering_file_scope) {
      /* Look at the function scope and its subscopes and promote local
         types and static variables to the file scope where appropriate. */
      check_assertion(scope->kind == (a_scope_kind)sck_function);
      if (local_entities_should_be_promoted(scope)) {
        promote_local_entities_to_file_scope(scope,
                                             scope->variant.routine.ptr);
      }  /* if */
    }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    /* Do name mangling.  This must be done early so that original type
       information is available (for example, references are still
       references and not yet pointers). */
    do_memory_region_name_mangling(scope);
    /* Create definitions for virtual function tables. */
    define_scope_virtual_function_tables(scope);
    /* Lower the scope and its subscopes in the same memory region. */
    lower_scope(scope);
    if (lowering_file_scope) {
      /* Lower any orphaned types and other entries from the function and
         block scopes.  They are allocated in the file scope memory region but
         are not linked into the file scope memory region IL tree, so they have
         to be found through a separate list. */
      lower_orphaned_entries();
    } else {
      /* Pop the file-scope context that was put around the function scope
         context. */
      pop_context();
    }  /* if */
    initial_value_for_il_lowering_flag = !initial_value_for_il_lowering_flag;
  }  /* if */
  db_exit();
}  /* lower_il_memory_region */


#if DEBUG
unsigned long show_lowering_space_used(void)
/*
Display and return the amount of space used for various IL lowering tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("IL lowering table use:");

  db_space_used("Name strings", allocated_name_string_length, char);
  db_space_used_lost("init pos modifier", avail_init_pos_modifiers,
                    num_init_pos_modifiers_allocated, an_init_pos_modifier);
  db_space_used_lost("required dtor call", avail_required_destructor_calls,
                     num_required_destructor_calls_allocated,
                     a_required_destructor_call);
  db_space_used("orphaned type list", num_orphaned_types_lists_allocated,
                an_orphaned_types_list);

  db_space_used_total();

  return grand_total;
}  /* show_lowering_space_used */
#endif /* DEBUG */


void il_lower_init(void)
/*
Initialize static variables related to IL lowering.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Static variables in lower_il.c: */
  module_id = NULL;
  avail_init_pos_modifiers = NULL;
  avail_required_destructor_calls = NULL;
  destructor_calls_for_local_static_variables = NULL;
  end_destructor_calls_for_local_static_variables = NULL;
  file_scope_init_routine = NULL;
  file_scope_term_routine = NULL;
  processing_file_scope_init_routine = FALSE;
  pure_virtual_called_routine = NULL;
  vptp_type = NULL;
  mptr_type = NULL;
  vec_new_routine = vec_cctor_routine = vec_delete_routine = NULL;
  orphaned_types_list = NULL;
  end_orphaned_types_list = NULL;
  type_promotion_insert_location = NULL;
  num_conditional_exprs_inside_of = 0;
  unnamed_class_name_seed = 0;
#if DEBUG
  allocated_name_string_length            = 0;
  num_init_pos_modifiers_allocated        = 0;
  num_required_destructor_calls_allocated = 0;
  num_orphaned_types_lists_allocated      = 0;
#endif /* DEBUG */
}  /* il_lower_init */

#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
