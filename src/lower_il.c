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

#include "basic_hdrs.h"
#if NEED_NAME_MANGLING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* NEED_NAME_MANGLING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if DO_IL_LOWERING
/* Additional header files. */
#include "exprutil.h"
#include "class_decl.h"
#include "layout.h"
#endif /* DO_IL_LOWERING */

/* Only include this code if it is needed.  The first few routines are
   needed if name mangling is needed, even if IL lowering is not. */
/* NEED_NAME_MANGLING is always TRUE if DO_IL_LOWERING is TRUE. */
#if NEED_NAME_MANGLING

static a_targ_ptrdiff_t pm_cast_offset(a_constant_ptr constant)
/*
constant is a pointer to member constant.  Return the byte offset to be
added to the basic member offset to account for casts done on the pointer
to member.
*/
{
  a_targ_ptrdiff_t offset;
  a_base_class_ptr bcp;

  bcp = constant->variant.ptr_to_member.casting_base_class;
  if (bcp == NULL) {
    offset = 0;
  } else {
    offset = bcp->offset;
    if (constant->variant.ptr_to_member.cast_to_base) offset = -offset;
  }  /* if */
  return offset;
}  /* pm_cast_offset */


void repr_for_ptr_to_data_member_constant(a_constant_ptr   constant, 
                                          a_targ_ptrdiff_t *delta)
/*
Determine the lowered representation of the indicated pointer-to-data-member
constant, and return information about it in *delta.
*/
{
  a_field_ptr      field;
  a_targ_ptrdiff_t offset = 0;

  field = constant->variant.ptr_to_member.variant.field;
  /* Use offset == 0 for NULL, otherwise the field offset. */
  if (field != NULL) {
    /* Determine the offset of the field within the class. */
    /* If the field is a member of an anonymous union, add in the offset of
       the anonymous union.  Several may be nested inside one another. */
    for (;;) {
      a_type_ptr field_class = field->source_corresp.class_of_which_a_member;
      a_class_type_supplement_ptr
                 ctsp = field_class->variant.class_struct_union.extra_info;
      offset += (a_targ_ptrdiff_t)field->offset;
      if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_field) {
        break;
      }  /* if */
      field = ctsp->anonymous_union_field;
    }  /* for */
    /* Add the offset of the field class relative to the pointer-to-member
       class and the offset of the field relative to its class.  Final
       "+1" is to reserve zero for NULL pointers. */
    offset = pm_cast_offset(constant) + offset + 1;
  }  /* if */
  *delta = offset;
}  /* repr_for_ptr_to_data_member_constant */


void repr_for_ptr_to_member_function_constant(a_constant_ptr   constant,
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
  a_routine_ptr routine;

  routine = constant->variant.ptr_to_member.variant.routine;
  /* The first field is the delta value, the offset of the class of the
     routine relative to the class pointed to by the pointer-to-member. */
  if (routine == NULL) {
    /* For a NULL ptr-to-member, delta is zero. */
    *delta = 0;
  } else {
    *delta = pm_cast_offset(constant);
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
    *offset = routine->source_corresp.class_of_which_a_member->
                                        variant.class_struct_union.extra_info->
                                                  virtual_function_info_offset;
    *func = NULL;
  }  /* if */
}  /* repr_for_ptr_to_member_function_constant */


#if DEBUG && DO_IL_LOWERING
/*
Count of entries allocated, for debugging purposes.
*/
static unsigned long
		allocated_name_string_length,
		num_return_memos_allocated;
#endif /* DEBUG && DO_IL_LOWERING */


char *alloc_lowered_name_string(sizeof_t size)
/*
Allocate a name string of length "size" and return a pointer to it.
This is used for names added or replaced (e.g., mangled names) during
IL lowering.
*/
{
  char *ptr = alloc_il(size);
#if DEBUG && DO_IL_LOWERING
  allocated_name_string_length += size;
#endif /* DEBUG && DO_IL_LOWERING */
  return ptr;
}  /* alloc_lowered_name_string */


/* The things above this point are the routines needed for name mangling
   even if lowering is not done. */
/* Everything below this point is related to IL lowering. */
#if DO_IL_LOWERING


a_boolean il_lowering_needed(void)
/*
Return TRUE if IL lowering is needed.  IL lowering is only needed in
this compilation if the source language is C++, there are no errors, and
lowering hasn't been suppressed.
*/
{
  a_boolean needed = (C_dialect == C_dialect_cplusplus &&
                      !suppress_il_lowering &&
                      total_errors == 0);
  return needed;
}  /* il_lowering_needed */


/*
Macro used to test for crossing over into the file scope, i.e., a
reference to a file-scope memory region entity from somewhere in a
function scope memory region.
*/
#define crossing_into_file_scope(entry_ptr)                           \
  (!lowering_file_scope && in_file_scope((char *)(entry_ptr)))


static a_return_memo_ptr
		avail_return_memos;
			/* List of return memo entries that have been freed
			   and are available for reuse. */


/* Declarations needed because of forward references: */
static void change_node_to_operation(an_expr_node_ptr      node,
                                     an_expr_operator_kind op,
                                     a_type_ptr            type,
                                     an_expr_node_ptr      operand);
static void lower_os_constant(a_constant_ptr constant);
static void lower_variable(a_variable_ptr variable);
static void lower_field_list(a_field_ptr field_list);
static void lower_field(a_field_ptr field);
static void lower_routine(a_routine_ptr routine);
static void lower_label(a_label_ptr label);
static void lower_asm_entry(an_asm_entry_ptr asm_entry);
static void lower_scope(a_scope_ptr scope);
static a_boolean any_cleanup_actions(an_object_lifetime_ptr outer_lifetime);
static a_boolean check_for_troublesome_ptr_to_member_constant(
                                                     a_constant_ptr constant,
                                                     a_variable_ptr *temp_var);
static void promote_class_members(a_type_ptr  class_type,
                                  a_scope_ptr promotion_scope,
                                  a_type_ptr  *insert_pointer);


static void clear_insert_location(an_insert_location      *insert_location,
                                  an_insert_location_kind kind)
/*
Clear an insert location, set its kind to the indicated value, and set
the associated variant fields to default values.
*/
{
  insert_location->kind = kind;
  switch (kind) {
    case ilk_after_statement:
    case ilk_block_start:
      insert_location->variant.stmt = NULL;
      break;
    case ilk_switch_clause_start:
      insert_location->variant.switch_clause = NULL;
      break;
    case ilk_before_expr:
    case ilk_after_expr:
      insert_location->variant.expr = NULL;
      break;
    default:
      unexpected_condition();
  }  /* switch */
}  /* clear_insert_location */


void set_insert_location(a_statement_ptr    stmt,
                         an_insert_location *insert_location)
/*
Set *insert_location to indicate an insert location following stmt.
Note that stmt must be a statement in a statement sequence (e.g., in
block); it may not be a statement in a position that requires a single
statement rather than a sequence (e.g., the dependent statement of an "if").
*/
{
  clear_insert_location(insert_location, ilk_after_statement);
  insert_location->variant.stmt = stmt;
}  /* set_insert_location */


void set_block_start_insert_location(a_statement_ptr    stmt,
                                     an_insert_location *insert_location)
/*
Set *insert_location to indicate an insert location at the start of
the block stmt.
*/
{ 
  clear_insert_location(insert_location, ilk_block_start);
  insert_location->variant.stmt = stmt;
}  /* set_block_start_insert_location */


static void set_switch_clause_start_insert_location(
                                          a_switch_clause_ptr scp,
                                          an_insert_location  *insert_location)
/*
Set *insert_location to indicate an insert location at the start of
the indicated switch clause.
*/
{ 
  clear_insert_location(insert_location, ilk_switch_clause_start);
  insert_location->variant.switch_clause = scp;
}  /* set_switch_clause_start_insert_location */


void set_expr_insert_location(an_expr_node_ptr   node,
                              an_insert_location *insert_location)
/*
Set *insert_location to indicate an insert location before the indicated
expression node.
*/
{
  clear_insert_location(insert_location, ilk_before_expr);
  insert_location->variant.expr = node;
}  /* set_expr_insert_location */


static void set_after_expr_insert_location(an_expr_node_ptr   node,
                                           an_insert_location *insert_location)
/*
Set *insert_location to indicate an insert location after the indicated
expression node.  This is a special and tricky mode, and should only be
used at the top of an expression tree, since it may change the type of
the node (and it wouldn't be possible to change the types of parent comma
and question-mark nodes).  The expression must have type void
or an assignable type.  In the assignable case, this routine changes the
expression tree, so this routine should only be called when it is known
that an insertion will be made.
*/
{
  a_type_ptr       node_type;
  a_variable_ptr   temp_var;
  an_expr_node_ptr assign_node, node_copy, temp_node;

  clear_insert_location(insert_location, ilk_after_expr);
  insert_location->variant.expr = node;
  node_type = node->type;
  if (node->result_is_not_used || is_void_type(node_type)) {
    /* The expression is a void expression.  Nothing special is required. */
    /* Also true if the expression result is not used. */
  } else {
    /* The expression has a non-void type.  Rewrite it to store the value
       in a temporary, then pick it up later, e.g.,
         x
       is rewritten as
         ((temp = x) , temp)
       and the insert point is set to insert after the assignment. */
    temp_var = make_lowered_temporary(node_type);
    /* Make a copy of the original node, then assign it to the temporary. */
    node_copy = copy_node(node);
    temp_node = var_lvalue_expr(temp_var);
    temp_node->next = node_copy;
    assign_node = make_operator_node(lowered_assignment_operator(node_type),
                                     node_type, temp_node);
    /* Change the original node to a comma expression. */
    assign_node->next = var_rvalue_expr(temp_var);
    change_node_to_operation(node, (an_expr_operator_kind)eok_comma,
                             node_type, assign_node);
    /* The insert point is after the assignment. */
    insert_location->variant.expr = assign_node;
  }  /* if */
}  /* set_after_expr_insert_location */


void add_to_return_memo_list(a_statement_ptr return_stmt)
/*
Allocate a return memo entry to record the existence of the indicated return
statement, and add it to the list of memo entries.
*/
{
  a_return_memo_ptr rmp;

  check_assertion_str(return_stmt != NULL &&
                      return_stmt->kind == (a_statement_kind)stmk_return,
                      "add_to_return_memo_list: bad stmt");
  if (avail_return_memos != NULL) {
    /* Reuse a freed entry. */
    rmp = avail_return_memos;
    avail_return_memos = rmp->next;
  } else {
    /* Allocate a new entry. */
    rmp = (a_return_memo_ptr)alloc_fe(sizeof(a_return_memo));
#if DEBUG
    num_return_memos_allocated++;
#endif /* DEBUG */
  }  /* if */
  rmp->next = return_memo_list;
  return_memo_list = rmp;
  rmp->stmt = return_stmt;
}  /* add_to_return_memo_list */


void free_return_memo_list(a_return_memo_ptr rmp)
/*
Free a list of return memo entries by putting them on the available list.
*/
{
  a_return_memo_ptr rmp_next;

  for (; rmp != NULL; rmp = rmp_next) {
    rmp_next = rmp->next;
    rmp->next = avail_return_memos;
    avail_return_memos = rmp;
  }  /* for */
}  /* free_return_memo_list */


void push_context(a_context              *context,
                  a_scope_ptr            scope,
                  an_object_lifetime_ptr lifetime)
/*
Add the context entry "context" to the context stack.  The associated scope
is "scope"; if scope is NULL, curr_context->scope is used.  The associated
object lifetime is "lifetime"; if lifetime is NULL, the lifetime from the
scope, or the lifetime from the parent context, will be used.
*/
{
  a_context_ptr parent_context = curr_context;
  a_boolean     new_lifetime;

  curr_context = context;
  /* Set the fields. */
  context->parent = parent_context;
  /* For the scope, use (1) the parameter passed in, or (2) the scope from
     the parent context. */
  if (scope == NULL) scope = parent_context->scope;
  context->scope = scope;
  /* For the lifetime, use (1) the parameter passed in, (2) the lifetime from
     the scope, or (3) the lifetime from the parent context. */
  if (lifetime == NULL) lifetime = scope->lifetime;
  new_lifetime = (lifetime != NULL);
  if (lifetime == NULL && parent_context != NULL) {
    lifetime = parent_context->lifetime;
  }  /* if */
  context->lifetime = lifetime;
  context->new_lifetime = new_lifetime;
  /* If this context begins a new object lifetime, set curr_object_lifetime.
     Also save the old value for restoration by pop_context.  Likewise save
     curr_cleanup_region_number. */
  if (new_lifetime) {
    context->saved_curr_object_lifetime = curr_object_lifetime;
    curr_object_lifetime = lifetime;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    context->saved_curr_cleanup_region_number = curr_cleanup_region_number;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
#if CHECKING
  } else {
    /* Clear entries to be neat, even though they are not used. */
    context->saved_curr_object_lifetime = NULL;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    context->saved_curr_cleanup_region_number = null_eh_region_number;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
#endif /* CHECKING */
  }  /* if */
  /* The latest_initialization list starts at NULL for a new object lifetime,
     or is inherited from the parent if there is no new object lifetime. */
  context->latest_initialization = NULL;
  if (!new_lifetime && parent_context != NULL) {
    context->latest_initialization = parent_context->latest_initialization;
  }  /* if */
  context->successor_lifetime_at_statement = NULL;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  context->try_frame = NULL;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
}  /* push_context */


void pop_context(void)
/*
Pop an entry off the context stack.
*/
{
  a_context_ptr context = curr_context, parent_context = context->parent;

  if (context->new_lifetime) {
    /* This context has its own object lifetime, so curr_object_lifetime
       and curr_cleanup_region_number are restored to what they were
       at push_context time. */
    curr_object_lifetime = context->saved_curr_object_lifetime;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    curr_cleanup_region_number = context->saved_curr_cleanup_region_number;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
  } else {
    /* This context does not have its own object lifetime, so the
       latest_initialization pointer is propagated up to the parent (it's
       lifetime-related). */
    if (parent_context != NULL) {
      parent_context->latest_initialization = context->latest_initialization;
    }  /* if */
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
      internal_error(
                   "find_virtual_base_class_of: virtual base class not found");
    }  /* if */
#endif /* CHECKING */
    if (virt_bcp->type == virt_base_class && virt_bcp->is_virtual) break;
  }  /* for */
  return virt_bcp;
}  /* find_virtual_base_class_of */


static a_base_class_ptr find_direct_or_virtual_base_class_of(
                                                   a_type_ptr  derived_class,
                                                   a_type_ptr  base_class_type)
/*
Return a pointer to the direct or virtual base class of derived_class with
a type identical to base_class_type.  It must be found.
*/
{
  a_base_class_ptr  bcp;

  bcp = base_classes_of(derived_class);
  for (;;) {
    check_assertion(bcp != NULL);
    if ((bcp->direct || bcp->is_virtual) && bcp->type == base_class_type) {
      /* Found. */
      break;
    }  /* if */
    bcp = bcp->next;
  }  /* for */
  return bcp;
}  /* find_direct_or_virtual_base_class_of */


/*
Macro to test for a zero-length bit field.
*/
#define field_is_zero_length_bit_field(field)                         \
  ((field)->is_bit_field && (field)->bit_size == 0)


static void add_field(char          *field_name,
                      a_type_ptr    field_type,
                      a_targ_size_t field_offset,
                      a_type_ptr    struct_type)
/*
Make a field with the given type and add it at the right spot in the
list of fields attached to struct_type.  field_name gives the field name
(already allocated in the IL memory region).  field_offset gives the byte
offset for the field.  The field allocated is not a bit field.
*/
{
  a_field_ptr prev_field, next_field;
  a_field_ptr field_ptr;

  /* Make the field. */
  field_ptr = alloc_field();
  field_ptr->source_corresp.name = field_name;
  field_ptr->source_corresp.class_of_which_a_member = struct_type;
  field_ptr->type = field_type;
  field_ptr->offset = field_offset;
  /* Find the spot at which to insert the field. */
  for (prev_field = NULL,
               next_field = struct_type->variant.class_struct_union.field_list;
       next_field != NULL && next_field->offset <= field_offset;
       prev_field = next_field, next_field = next_field->next) {
#if CHECKING
    /* Check for fields with the same offset, but watch out for zero-length
       bit fields. */
    if (next_field->offset == field_offset &&
        next_field->offset_bit_remainder == 0 &&
        !field_is_zero_length_bit_field(next_field)) {
#if DEBUG
      db_abbreviated_type(struct_type);
      fprintf(f_debug, ", offset = %lu, new field = %s, old field = ",
                       (unsigned long)field_offset, field_name);
      db_name(&next_field->source_corresp);
      fputc('\n', f_debug);
#endif /* DEBUG */
      internal_error("add_field: two fields have the same offset");
    }  /* if */
#endif /* CHECKING */
  }  /* for */
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
  name_ptr = alloc_lowered_name_string(alloc_length);
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
  name_length = mangled_class_name(base_class_type, (char *)NULL);
  /* Allocate space for the whole name. */
  alloc_length = prefix_length + name_length + 1;
  name_ptr = alloc_lowered_name_string(alloc_length);
  /* Copy in the prefix. */
  (void)memcpy(name_ptr, field_prefix, size_t_arg(prefix_length));
  /* Store the base class name. */
  (void)mangled_class_name(base_class_type, name_ptr+prefix_length);
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
    field_name = strcpy(alloc_lowered_name_string(alloc_length), field_name);
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


void make_lowered_field(char          *field_name,
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
entry and (updated) on exit.  If struct_type is a union, each field is
added at offset 0, and *byte_offset is used to hold the size of the
largest field seen so far.  This routine is used for creating fields of
wholly-generated structs, not for adding fields to existing structs.
It cannot create bit fields.  field_name may not be NULL.
*/
{
  sizeof_t                   name_length, alloc_length;
  a_field_ptr                field_ptr;
  a_targ_alignment           alignment;
  an_unnormalized_bit_offset bit_offset;
  a_targ_size_t              old_byte_offset;

  /* Copy the name into the file-scope IL memory region. */
  name_length = strlen(field_name);
  alloc_length = name_length + 1;
  field_name = strcpy(alloc_lowered_name_string(alloc_length), field_name);
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
  if (struct_type->kind == (a_type_kind)tk_union) {
    /* For a union, each field is at offset 0. */
    old_byte_offset = *byte_offset;
    *byte_offset = 0;
  }  /* if */
  (void)set_field_size_and_offset(field_ptr, byte_offset, &bit_offset,
                                  &alignment);
  struct_type->alignment = alignment;
  if (struct_type->kind == (a_type_kind)tk_union) {
    /* For unions, maintain the size of the largest field. */
    if (*byte_offset < old_byte_offset) *byte_offset = old_byte_offset;
  }  /* if */
}  /* make_lowered_field */


void finish_class_type(a_type_ptr    class_type, 
                       a_targ_size_t *byte_offset)
/*
Finish off a created class type by doing final alignment and storing the
size and alignment.  Works for both structs and unions.
*/
{
  a_class_type_supplement_ptr ctsp;
  an_unnormalized_bit_offset  bit_offset = 0;

  (void)do_alignment(byte_offset, &bit_offset, class_type->alignment);
  /* Put final size into the struct or union type. */
  class_type->size = *byte_offset;
  ctsp = class_type->variant.class_struct_union.extra_info;
  ctsp->size_without_virtual_base_classes = *byte_offset;
  ctsp->alignment_without_virtual_base_classes = class_type->alignment;
}  /* finish_class_type */


a_type_ptr void_star_type(void)
/*
Make and return a "void *" type.
*/
{
  return make_pointer_type(void_type());
}  /* void_star_type */


a_type_ptr char_star_type(void)
/*
Make and return a "char *" type.
*/
{
  return make_pointer_type(integer_type(plain_char_int_kind));
}  /* char_star_type */


/*
Pointer to the generic function pointer type used in virtual function tables
and pointers to member functions, once it is created.  NULL until created.
*/
static a_type_ptr
		vptp_type;


a_type_ptr make_vptp_type(void)
/*
Make a type that is used as a generic function pointer in virtual function
tables and pointers to member functions if it has not been made already,
and return a pointer to it.  The type looks like

  typedef void (*__vptp)();

except that it doesn't actually have a name.
*/
{
  a_type_ptr function_type;

  if (vptp_type == NULL) {
    /* Make function-of-no-parameters-returning-void. */
    function_type = alloc_type((a_type_kind)tk_routine);
    function_type->variant.routine.return_type = void_type();
    /* Make pointer to function-returning-void. */
    vptp_type = make_pointer_type(function_type);
  }  /* if */
  return vptp_type;
}  /* make_vptp_type */


void add_to_front_of_file_scope_types_list(a_type_ptr type)
/*
Add the indicated type to the beginning of the file-scope types list.
This is used for simple lowering-generated structs that might be used
inside other user-written structs.
*/
{
  type->next = il_header.primary_scope->types;
  il_header.primary_scope->types = type;
  if (type->next == NULL && depth_scope_stack >= DEPTH_OF_FILE_SCOPE) {
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
    add_to_front_of_file_scope_types_list(mptr_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: short d; (delta) */
    make_lowered_field("d", integer_type(TARG_DELTA_INT_KIND),
                       &byte_offset, mptr_type, &last_field);
    mptr_d_field = last_field;
    /* field: short i; (index into virtual function table) */
    make_lowered_field("i", integer_type(TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND),
                       &byte_offset, mptr_type, &last_field);
    mptr_i_field = last_field;
    /* field: __vptp f; (pointer to function for nonvirtual case, or
       offset to vtbl ptr, appropriately cast, in nonvirtual case) */
    make_lowered_field("f", make_vptp_type(), &byte_offset, mptr_type,
                       &last_field);
    mptr_f_field = last_field;
    finish_class_type(mptr_type, &byte_offset);
#if CHECKING
    if (mptr_type->size != targ_sizeof_ptr_to_member_function ||
        mptr_type->alignment != targ_alignof_ptr_to_member_function) {
      internal_error(
 "make_mptr_type: target config of pointer-to-member-function is incorrect");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  return mptr_type;
}  /* make_mptr_type */


a_type_ptr underlying_type(a_type_ptr type)
/*
Drop typerefs, watching out for a typeref with orig_type set.  For that
case, return the original type.
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
}  /* underlying_type */


static a_type_ptr pm_member_type_possibly_lowered(a_type_ptr type)
/*
type is (or was, before lowering) a pointer-to-member type.  Get and return
its member type.
*/
{
  a_type_ptr member_type;

  type = underlying_type(type);
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

  type = underlying_type(type);
  class_type = pm_class_type(type);
  return class_type;
}  /* pm_class_type_possibly_lowered */


a_boolean is_or_was_ptr_to_data_member_type(a_type_ptr type)
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


a_variable_ptr make_temporary_in_scope(a_type_ptr  temp_type,
                                       a_scope_ptr scope,
                                       a_boolean   force_static)
/*
Make a temporary variable in scope "scope" whose type is "temp_type" and
whose storage class is static if force_static is TRUE or if the scope
is the file scope.  Return a pointer to it.
*/
{
  a_variable_ptr          temp;
  a_storage_class         storage_class;
  a_scope_stack_entry_ptr ssep;
  a_variable_ptr          *prev_ptr_ptr, *last_ptr_ptr;

  /* Allocate the variable, using auto storage class in functions and
     blocks, static elsewhere.  If force_static is TRUE, use static. */
  if (!force_static &&
      (scope->kind == (a_scope_kind)sck_function ||
       scope->kind == (a_scope_kind)sck_block)) {
    storage_class = (a_storage_class)sc_auto;
  } else {
    storage_class = (a_storage_class)sc_static;
  }  /* if */
  temp = alloc_variable(storage_class);
  temp->type = temp_type;
  temp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_none;
  if (scope->kind == (a_scope_kind)sck_block ||
      scope->kind == (a_scope_kind)sck_function) {
    /* Mark local variables of functions. */
    temp->source_corresp.is_local_to_function = TRUE;
  }  /* if */
  /* See if the scope we are adding to is active on the scope stack.
     If so, we have to maintain the "last" pointer too. */
  ssep = NULL;
  if (scope->depth_in_scope_stack != NO_SCOPE_DEPTH) {
    ssep = &scope_stack[scope->depth_in_scope_stack];
  }  /* if */
  /* Add the temporary to the scope list (at the front).  We cannot use
     add_to_variables_list because we might be working on a scope that is
     not on the stack. */
  /* The variable goes on either the static or the nonstatic variables list,
     so determine the proper pointers to adjust. */
  last_ptr_ptr = NULL;
  if (storage_class == (a_storage_class)sc_static) {
    prev_ptr_ptr = &scope->variables;
    if (ssep != NULL) last_ptr_ptr = &ssep->last_variable;
  } else {
    prev_ptr_ptr = &scope->nonstatic_variables;
    if (ssep != NULL) last_ptr_ptr = &ssep->last_nonstatic_variable;
  }  /* if */
  /* The temporary goes at the front, but after any unnamed entities.  That
     ensures that temporaries built later come after temporaries built
     earlier, which is needed when record_needed_destruction is called
     for a temporary. */
  while (*prev_ptr_ptr != NULL && !has_name(*prev_ptr_ptr)) {
    prev_ptr_ptr = &(*prev_ptr_ptr)->next;
  }  /* while */
  temp->next = *prev_ptr_ptr;
  *prev_ptr_ptr = temp;
  if (last_ptr_ptr != NULL && temp->next == NULL) *last_ptr_ptr = temp;
  return temp;
}  /* make_temporary_in_scope */


a_variable_ptr make_lowered_temporary(a_type_ptr temp_type)
/*
Interface to make_temporary_in_scope for the common case where the
nearest scope should be used.  Allocates a temporary variable of the
indicated type and returns a pointer to the variable.
*/
{
  return make_temporary_in_scope(temp_type, curr_context->scope,
                                 /*force_static=*/FALSE);
}  /* make_lowered_temporary */


a_variable_ptr make_file_scope_temporary(a_type_ptr temp_type)
/*
Make an unnamed static variable of type temp_type in the file scope.
Return a pointer to the variable.
*/
{
  a_variable_ptr temp_var;

  /* Note that the allocation will be in the file scope memory region
     regardless of the current IL region. */
  temp_var = make_temporary_in_scope(temp_type, il_header.primary_scope,
                                     /*force_static=*/TRUE);
  return temp_var;
}  /* make_file_scope_temporary */


a_variable_ptr make_function_scope_temporary(a_type_ptr temp_type)
/*
Make an unnamed auto variable of type temp_type in the nearest function
scope.  Return a pointer to the variable.
*/
{
  a_variable_ptr temp_var;

  temp_var = make_temporary_in_scope(temp_type, innermost_function_scope,
                                     /*force_static=*/FALSE);
  return temp_var;
}  /* make_function_scope_temporary */


a_variable_ptr make_unnamed_local_static_variable(a_type_ptr type,
                                                  a_boolean  in_function_scope)
/*
Make an unnamed local static variable with the indicated type and return
a pointer to it.  If in_function_scope is TRUE, add it to the function scope
instead of the current context (which might be a block scope).
*/
{
  check_assertion_str(curr_context != NULL,
                   "make_unnamed_local_static_variable: curr_context is NULL");
  return make_temporary_in_scope(type,
                                 in_function_scope ? innermost_function_scope :
                                                     curr_context->scope,
                                 /*force_static=*/TRUE);
}  /* make_unnamed_local_static_variable */


a_variable_ptr make_lowered_variable(char            *var_name,
                                     a_boolean       already_il_name,
                                     a_type_ptr      var_type,
                                     a_storage_class var_storage_class)
/*
Make a file-scope variable whose name is var_name, whose type is var_type,
and whose storage class is var_storage_class.  Return a pointer to it.
already_il_name is TRUE if the name has already been allocated in the IL;
if not, it has to be allocated and copied.  var_name may be NULL if
already_il_name is TRUE.
*/
{
  a_variable_ptr var;
  sizeof_t       alloc_length;

  /* Allocate the variable.  Note that the subroutine allocates the variable
     in the file scope if the storage class is static. */
  var = alloc_variable(var_storage_class);
  if (!already_il_name) {
    /* Copy the name to the IL region. */
    alloc_length = strlen(var_name)+1;
    var_name = strcpy(alloc_lowered_name_string(alloc_length), var_name);
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
    internal_error(
                "make_lowered_variable: bad storage class for file scope var");
  }  /* if */
#endif /* CHECKING */
  /* Add the variable to the file scope list. */
  add_to_variables_list(var, /*at_file_scope=*/TRUE);
  return var;
}  /* make_lowered_variable */


a_variable_ptr make_lowered_param_variable(a_type_ptr type)
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
}  /* make_lowered_param_variable */

/* Determine whether or not we need make_instantiation_var, and if
   so, whether or not it needs to be external. */
#if TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
#define MAKE_INSTANTIATION_VAR_LINKAGE /*external*/
#else /* !TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
#define MAKE_INSTANTIATION_VAR_LINKAGE static
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */
#ifdef MAKE_INSTANTIATION_VAR_LINKAGE

MAKE_INSTANTIATION_VAR_LINKAGE
a_variable_ptr make_instantiation_var(char                    *prefix,
                                      an_integer_kind         ikind,
                                      a_source_correspondence *source_corresp)
/*
Create a variable related to instantiation of some template entity,
and return a pointer to it.  source_corresp identifies the template
entity (variable or routine).  The name of the generated variable
consists of the indicated prefix followed by the mangled name of the
entity.  The variable has the integral type indicated by ikind.
*/
{
  a_variable_ptr var;
  char           *mangled_name, *info_name;
  sizeof_t       mangled_name_length, info_name_length;
  sizeof_t       prefix_length, alloc_length;

  /* The name of the entity should be mangled already. */
  check_assertion(source_corresp->name_has_been_mangled);
  mangled_name = source_corresp->name;
  mangled_name_length = strlen(mangled_name);
  prefix_length = strlen(prefix);
  info_name_length = prefix_length + mangled_name_length;
  /* Allocate space for the info name, including the final null. */
  alloc_length = info_name_length + 1;
  info_name = alloc_lowered_name_string(alloc_length);
  /* Build the mangled name. */
  (void)strcpy(info_name, prefix);
  (void)strcpy(info_name+prefix_length, mangled_name);
  /* Make the variable.  Note that it is a definition of an external name. */  
  var = make_lowered_variable(info_name, /*already_il_name=*/TRUE,
                              integer_type(ikind),
                              (a_storage_class)sc_unspecified);
  return var;
}  /* make_instantiation_var */

#endif /* ifdef MAKE_INSTANTIATION_VAR_LINKAGE */
#if AUTOMATIC_TEMPLATE_INSTANTIATION

static void make_instantiation_info_var(
                                       char                    *prefix,
                                       a_source_correspondence *source_corresp)
/*
Create a variable whose name records information on instantiation of some
entity.  Such variables are used as part of the automatic instantiation scheme.
source_corresp identifies the entity (variable or routine) for which some
information is to be encoded.  The name of the generated variable encodes
the information about that entity; it consists of the indicated prefix
(e.g., something like "__DNI__" to indicate "do not instantiate") followed
by the mangled name of the entity.  The variable has type char (arbitrarily).
*/
{
  (void)make_instantiation_var(prefix, (an_integer_kind)ik_char,
                               source_corresp);
}  /* make_instantiation_info_var */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

an_expr_node_ptr make_node_for_il_constant(a_constant_ptr constant)
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
  op = (field->is_bit_field) ? (an_expr_operator_kind)eok_value_bit_field :
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
  a_field_ptr                 field, au_field;
  an_expr_node_ptr            op2;
  a_type_ptr                  field_class;
  a_class_type_supplement_ptr ctsp;

  /* The loop here is for cases where there are several nested anonymous
     unions. */
  for (;;) {
    op2 = node->variant.operation.operands->next;
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
    /* Change "x.y" to "x.au_field.y". */
    adjust_anonymous_union_field_selection(node, au_field);
    /* Loop to see if the rewritten first operand still refers to an
       anonymous union field (because there are several nested anonymous
       unions), and if so, to rewrite it. */
    node = node->variant.operation.operands;
  }  /* for */
}  /* adjust_field_selection_for_anonymous_union_references */


an_expr_node_ptr au_field_lvalue_selection_expr(an_expr_node_ptr node,
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


an_expr_node_ptr add_cast(an_expr_node_ptr node,
                          a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.
new_type should not have any top-level type qualifiers.
*/
{
  return make_operator_node((an_expr_operator_kind)eok_cast, new_type, node);
}  /* add_cast */


an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                       a_type_ptr       new_type)
/*
Add a cast to new_type to the node and return the cast node.  If the
type of the node is already new_type return the original node.
new_type should not have any top-level type qualifiers.
*/
{
  if (!il_identical_types(node->type, new_type)) {
    node = add_cast(node, new_type);
  }  /* if */
  return node;
}  /* add_cast_if_necessary */


void change_to_cast(an_expr_node_ptr node,
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


static an_expr_node_ptr add_cast_to_char_star(an_expr_node_ptr node)
/*
Add a cast to "char *" to the node and return the cast node.  If the
type of the node is already "char *" return the original node.
*/
{
  return add_cast_if_necessary(node, char_star_type());
}  /* add_cast_to_char_star */


an_expr_node_ptr array_var_lvalue_expr(a_variable_ptr var)
/*
Create an expression tree for the address of the array associated with the
variable var, and return a pointer to it.  This differs from var_lvalue_expr
in that it does the cast to pointer-to-element.
*/
{
  an_expr_node_ptr node;

  node = var_lvalue_expr(var);
  node = add_cast(node, make_pointer_type(array_element_type(var->type)));
  return node;
}  /* array_var_lvalue_expr */


static a_field_ptr field_at_offset(a_type_ptr    class_type,
                                   a_targ_size_t byte_offset)
/*
Return a pointer to the field at the indicated byte offset of the indicated
class type.
*/
{
  a_field_ptr field_ptr;

#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("field_at_offset: bad class type");
  }  /* if */
#endif /* CHECKING */
#if 0
  /* It may be necessary to come up with a faster way of doing this, such
     as storing two field pointers in the base class entry and a pointer to
     the virtual function table pointer field in the class type supplement. */
#endif /* 0 */
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
    /* Don't pick a zero-length bit field as the answer.  The field
       following it is probably what's wanted. */
    if (field_ptr->offset == byte_offset &&
        field_ptr->offset_bit_remainder == 0 &&
        !field_is_zero_length_bit_field(field_ptr)) break;
  }  /* for */
  return field_ptr;
}  /* field_at_offset */


static an_expr_node_ptr make_vbptr_field_lvalue(an_expr_node_ptr node,
                                                a_base_class_ptr bcp)
/*
Make an lvalue for the virtual base class pointer field for the base class
indicated by bcp of the object pointed to by node.
The class object is not known to be a complete object (but we couldn't do
any better if we did know it was a complete object).
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
    check_assertion(!any_virtual_steps_in_derivation(pointer_bcp));
    node = make_base_class_lvalue(node, pointer_bcp,
                                  /*complete_object=*/FALSE);
    /* The following line does not use pointer_bcp->type because the type here
       could be either that type or the corresponding type-as-subobject. */
    pointer_class_type = f_skip_typerefs(type_pointed_to(node->type));
    pointer_offset -= pointer_bcp->offset;
  }  /* if */
  /* Generate the field selection in the original class or the base class
     we got to. */
  node = field_lvalue_selection_expr(node,
                                     field_at_offset(pointer_class_type,
                                                     pointer_offset));
  return node;
}  /* make_vbptr_field_lvalue */


an_expr_node_ptr make_vbptr_field_lvalue_from_var(a_variable_ptr   var,
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


an_expr_node_ptr make_vptr_field_lvalue(an_expr_node_ptr node)
/*
Make an lvalue for the virtual table pointer of the object pointed to by node.
The class object is not known to be a complete object (but we couldn't do
any better if we did know it was a complete object).
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
    check_assertion(!any_virtual_steps_in_derivation(vptr_bcp));
    node = make_base_class_lvalue(node, vptr_bcp, /*complete_object=*/FALSE);
    /* The following line does not use vptr_bcp->type because the type here
       could be either that type or the corresponding type-as-subobject. */
    vptr_class_type = f_skip_typerefs(type_pointed_to(node->type));
    vptr_offset -= vptr_bcp->offset;
  }  /* if */
  /* Generate the field selection in the original class or the base class
     we got to. */
  node = field_lvalue_selection_expr(node,
                                     field_at_offset(vptr_class_type,
                                                     vptr_offset));
  return node;
}  /* make_vptr_field_lvalue */


an_expr_node_ptr make_vptr_field_lvalue_from_var(a_variable_ptr var)
/*
Make an lvalue for the virtual table pointer of the object pointed to by var.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_vptr_field_lvalue(node);
  return node;
}  /* make_vptr_field_lvalue_from_var */


static an_expr_node_ptr make_vbase_class_lvalue(
                                              an_expr_node_ptr node,
                                              a_base_class_ptr bcp,
                                              a_boolean        complete_object)
/*
Make an expression node that is an lvalue for the virtual base class bcp
of the class pointed to by node.  The class object is known to be a complete
object if complete_object is TRUE.  Return a pointer to the new node.
*/
{
  a_type_ptr    class_type, data_section_class_type;
  a_targ_size_t data_section_offset;

  class_type = type_pointed_to(node->type);
  class_type = skip_typerefs(class_type);
#if CHECKING
  if (!is_immediate_class_type(class_type)) {
    internal_error("make_vbase_class_lvalue: not class type");
  }  /* if */
#endif /* CHECKING */
  prelower_class_type(class_type);
  if (complete_object) {
    /* We have a complete object, so we know exactly where the virtual
       base class is (we don't have to go indirect through a pointer). */
    data_section_class_type = class_type;
    data_section_offset = bcp->offset;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    { a_base_class_ptr data_section_bcp = bcp->data_section_base_class;
      if (data_section_bcp != NULL) {
        /* The virtual base class is allocated in a base class.  Get the 
           address of the proper base class. */
        node = make_base_class_lvalue(node, data_section_bcp,
                                      /*complete_object=*/TRUE);
        /* The following line does not use data_section_bcp->type because the
           type here could be either that type or the corresponding
           type-as-subobject. */
        data_section_class_type = f_skip_typerefs(type_pointed_to(node->type));
        data_section_offset -= data_section_bcp->offset;
      }  /* if */
    }
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    /* Select the data section for the virtual base class. */
    node = field_lvalue_selection_expr(node,
                                       field_at_offset(data_section_class_type,
                                                       data_section_offset));
  } else {
    /* We do not know whether or not we have a complete object, so we must
       go indirect through a pointer to get to the virtual base class. */
    node = make_vbptr_field_lvalue(node, bcp);
    node = add_indirection_to_node(node);
  }  /* if */
  return node;
}  /* make_vbase_class_lvalue */


an_expr_node_ptr make_vbase_class_lvalue_from_var(
                                              a_variable_ptr   var,
                                              a_base_class_ptr bcp,
                                              a_boolean        complete_object)
/*
Make an expression node that is an lvalue for the base class bcp of the
class pointed to by var.  The class object is known to be a complete
object if complete_object is TRUE.  Return a pointer to the new node.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_vbase_class_lvalue(node, bcp, complete_object);
  return node;
}  /* make_vbase_class_lvalue_from_var */


an_expr_node_ptr make_base_class_lvalue(an_expr_node_ptr node,
                                        a_base_class_ptr bcp,
                                        a_boolean        complete_object)
/*
Make an expression node that is an lvalue for the base class bcp of the
class object pointed to by node.  bcp may be a direct or indirect base
class, and its derivation may include virtual steps.  The class object
is known to be a complete object if complete_object is TRUE.  Return a
pointer to the new node.
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
  if (bcp->is_virtual) {
    /* The base class is a virtual base class.  Generate the code
       to get to it. */
    node = make_vbase_class_lvalue(node, bcp, complete_object);
  } else {
    /* The base class is not a virtual base class (so it has only one
       derivation). */
    step_class_type = class_type;
    dsp = bcp->derivation->path;
    /* See if there are any virtual steps in the derivation of the base class.
       If so, the first step is to a virtual base class (the front end
       standardizes the derivation steps up to the last virtual step into
       an initial "leap" to the virtual base class). */
    step_bcp = dsp->base_class;
    if (step_bcp->is_virtual) {
      /* Generate code for the hop to the virtual base class. */
      node = make_vbase_class_lvalue(node, step_bcp, complete_object);
      step_class_type = step_bcp->type;
      /* The non-virtual steps start after the virtual step. */
      dsp = dsp->next;
    }  /* if */
    /* Now any initial virtual hop has been done.  dsp indicates the
       remaining derivation steps.  step_class_type is the unqualified
       class type we've gotten to so far. */
    /* Put out a field selection for each step in the derivation. */
    /* Two class type variables are needed because the fields for the base
       classes may have the type of the base class as a subobject or the type
       of the base class itself (that's what node_class_type will contain)
       and the base class entries have the type of the base class itself
       (that's what step_class_type will contain). */
    for (; dsp != NULL; dsp = dsp->next) {
      /* The base class entry pointed to by dsp->base_class is the base
         class entry relative to the original class type.  Find the base
         class entry for this step relative to the intermediate class we
         have gotten to. */
      step_bcp = derivation_bcp = dsp->base_class;
      /* For the first step the information is already correct. */
      if (dsp != bcp->derivation->path) {
        step_bcp = find_direct_base_class_of(step_class_type, step_bcp->type);
        check_assertion(step_bcp != NULL);
      }  /* if */
      node_class_type = type_pointed_to(node->type);
      node_class_type = skip_typerefs(node_class_type);
#if CHECKING
      if (node_class_type != step_class_type &&
          node_class_type != step_class_type->variant.class_struct_union.
                                               extra_info->type_as_subobject) {
        internal_error("make_base_class_lvalue: node has wrong type");
      }  /* if */
#endif /* CHECKING */
      /* Create a field selection to select the next non-virtual base class. */
      node = field_lvalue_selection_expr(node,
                                         field_at_offset(node_class_type,
                                                         step_bcp->offset));
      step_class_type = derivation_bcp->type;
    }  /* for */
  }  /* if */
  return node;
}  /* make_base_class_lvalue */


an_expr_node_ptr make_base_class_lvalue_from_var(
                                              a_variable_ptr   var,
                                              a_base_class_ptr bcp,
                                              a_boolean        complete_object)
/*
Make an expression node that is an lvalue for the base class bcp of the
class pointed to by var.  The class object is known to be a complete
object if complete_object is TRUE.  Return a pointer to the node.
*/
{
  an_expr_node_ptr node = var_rvalue_expr(var);

  node = make_base_class_lvalue(node, bcp, complete_object);
  return node;
}  /* make_base_class_lvalue_from_var */


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
      cannot_be = (operand->next->variant.field->offset != 0 ||
                   cannot_be_null(operand));
    } else if (op == (an_expr_operator_kind)eok_base_class_cast ||
               op == (an_expr_operator_kind)eok_derived_class_cast) {
      /* A cast to a related class will not turn a non-NULL into a NULL. */
      cannot_be = cannot_be_null(operand);
    }  /* if */
  } else if (expr->kind == (an_expr_node_kind)enk_variable) {
    /* If the expression if the "this" variable for the current function,
       it cannot be null. */
    if (innermost_function_scope != NULL &&
        innermost_function_scope->variant.routine.this_param_variable ==
                                                      expr->variant.variable) {
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


an_expr_operator_kind lowered_assignment_operator(a_type_ptr type)
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


an_expr_node_ptr make_reusable_copy(an_expr_node_ptr expr,
                                    a_boolean        vars_can_change)
/*
Return a copy of the expression tree pointed to by expr.  If the expression
has side effects, or if its value is affected by the values of variables
and vars_can_change is TRUE, the original expression will be changed so
that its value is stored in a temporary, and the copy will reference the
temporary.  expr should be an rvalue (although make_lvalue_reusable_copy
calls this routine after it has discarded the troublesome lvalue cases).
*/
{
  an_expr_node_ptr expr_copy, temp_node;
  a_variable_ptr   temp;
  a_type_ptr       temp_type;
  a_boolean        need_temp, suppress_warning;

  need_temp = TRUE;
  if (vars_can_change) {
    /* For the vars_can_change case, do a crude analysis: if the expression
       is constant, it cannot be affected by changes in the values of
       variables.  This could be improved, but it probably doesn't matter. */
    if (is_constant_node(expr) || is_variable_address_node(expr) ||
        is_routine_address_node(expr)) {
      need_temp = FALSE;
    } else if (is_variable_node(expr) &&
               expr->variant.variable->source_corresp.name == NULL) {
      /* An unnamed variable is a temporary.  Assume that such a thing is
         not changed in the "vars_can_change" mode.  This is important,
         because if the expression has been assigned to a temporary once,
         we want to use that temporary directly on subsequent calls to
         make_reusable_copy. */
      need_temp = FALSE;
    }  /* if */
  } else {
    /* Variables cannot change.  See if the expression has side effects. */
    if (!node_has_side_effects(expr, &suppress_warning)) need_temp = FALSE;
  }  /* if */
  if (!need_temp) {
    /* A straight copy will work. */
    /* Note that this expression will not have temporaries or object lifetimes
       in it since it has no side effects. */
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
        /* Watch out for types created by IL lowering. */
        temp_type->source_corresp.assoc_info != NULL) {
      if (!symbol_supplement_for_class(temp_type)->
                                        construction_by_bitwise_copy_allowed) {
        internal_error("make_reusable_copy: temp of class type with cctor");
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    temp = make_lowered_temporary(temp_type);
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


static an_expr_node_ptr make_lvalue_reusable_copy(
                                              an_expr_node_ptr expr,
                                              a_boolean        vars_can_change)
/*
Return a copy of the expression tree pointed to by expr.  If the expression
has side effects, or if its value is affected by the values of variables
and vars_can_change is TRUE, the original expression will be changed so
that its value is stored in a temporary, and the copy will reference the
temporary.  expr should be an lvalue.
*/
{
  a_boolean             special_case = FALSE;
  an_expr_node_ptr      expr_copy, operand1, operand2, operand3;
  an_expr_node_ptr      operand1_copy, operand2_copy, operand3_copy;
  an_expr_operator_kind op;

  if (is_operation_node(expr)) {
    op = expr->variant.operation.kind;
    operand1 = expr->variant.operation.operands;
    if (op == (an_expr_operator_kind)eok_bit_field) {
      /* For a bit-field reference, make a reusable copy of the struct
         address, then add the bit field selection to that. */
      special_case = TRUE;
      operand2 = operand1->next;
      operand1_copy = make_lvalue_reusable_copy(operand1, vars_can_change);
      expr_copy = field_lvalue_selection_expr(operand1_copy,
                                              operand2->variant.field);
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* For a "?" operator, make reusable copies of all three operands,
         and a new "?" that uses the reusable copies. */
      special_case = TRUE;
      operand2 = operand1->next;
      operand3 = operand2->next;
      operand3_copy = make_lvalue_reusable_copy(operand3, vars_can_change);
      operand2_copy = make_lvalue_reusable_copy(operand2, vars_can_change);
      operand2_copy->next = operand3_copy;
      operand1_copy = make_reusable_copy(operand1, vars_can_change);
      operand1_copy->next = operand2_copy;
      expr_copy = make_operator_node((an_expr_operator_kind)eok_question,
                                     expr->type, operand1_copy);
    } else if (op == (an_expr_operator_kind)eok_comma) {
      /* For a "," operator, make a reusable copy of the second operand. */
      special_case = TRUE;
      operand2 = operand1->next;
      expr_copy = make_lvalue_reusable_copy(operand2, vars_can_change);
    }  /* if */
  }  /* if */
  if (!special_case) {
    /* For other cases, use the rvalue copy. */
    expr_copy = make_reusable_copy(expr, vars_can_change);
  }  /* if */
  return expr_copy;
}  /* make_lvalue_reusable_copy */


void overwrite_node(an_expr_node_ptr node,
                    an_expr_node_ptr source_node)
/*
Overwrite the expression node "node" with the contents of the node
"source_node".  This is used to remove do-nothing nodes by promoting
their operands.
*/
{
  an_expr_node_ptr node_next = node->next;
  a_boolean        result_is_not_used = node->result_is_not_used;

  /* Copy the node.  Preserve the original "next" field and the 
     result_is_not_used flag. */
  /* Note that the new/delete supplement from the source node is used
     by the destination node; no copy is needed. */
  *node = *source_node;
  node->next = node_next;
  node->result_is_not_used = result_is_not_used;
  if (result_is_not_used) set_expr_result_not_used(node);
}  /* overwrite_node */


static void change_node_to_operation(an_expr_node_ptr      node,
                                     an_expr_operator_kind op,
                                     a_type_ptr            type,
                                     an_expr_node_ptr      operand)
/*
Change node to an operation node with operator op, type type, and operand
list as given by operand.
*/
{
  an_expr_node_ptr node_next;
  a_boolean        node_result_is_not_used;

  /* Preserve the next pointer and the result_is_not_used flag. */
  node_next = node->next;
  node_result_is_not_used = node->result_is_not_used;
  clear_expr_node(node, (an_expr_node_kind)enk_operation);
  node->next = node_next;
  /* Don't need to call set_expr_result_not_used here; set_node_operator
     calls it. */
  node->result_is_not_used = node_result_is_not_used;
  set_node_operator(node, op, type, operand);
}  /* change_node_to_operation */


void insert_expr(an_expr_node_ptr       inserted_expr,
                 an_insert_location_ptr insert_location)
/*
Insert the expression inserted_expr at *insert_location, which is an insert
location within an expression.  Update *insert_location so the next insertion
will be after the expression added.
*/
{
  an_expr_node_ptr        orig_expr, orig_expr_copy;
  an_expr_node_ptr        first_operand, second_operand;
  an_insert_location_kind kind = insert_location->kind;

  check_assertion_str(is_expr_insert_location_kind(kind),
                      "insert_expr: insert location is not expr insert");
  orig_expr = insert_location->variant.expr;
  /* Make a comma node that has the original node and the expression
     being inserted as its operands.  The original node is actually copied
     so that the comma node can be put at the address of the original node. */
  orig_expr_copy = copy_node(orig_expr);
  /* Order the operands of the comma operator depending on whether the
     insertion is supposed to be before or after the original expression. */
  if (kind == ilk_before_expr) {
    first_operand = inserted_expr;
    second_operand = orig_expr_copy;
    /* Change the insert location so that it inserts before the second
       expression (the original one).  Note that we cannot make an "after"
       insertion implicitly; they are tricky and must be made explicitly. */
    insert_location->variant.expr = second_operand;
  } else {
    first_operand = orig_expr_copy;
    second_operand = inserted_expr;
    /* The insert location is left as it is, for insertion after the original
       node, which will be the comma node.  That's used instead of the
       second operand to avoid problems with keeping the type of comma
       nodes up to date when inserts are done under them in their second
       operands. */
  }  /* if */
  first_operand->next = second_operand;
  second_operand->next = NULL;
  /* Turn the original node into a comma node. */
  change_node_to_operation(orig_expr, (an_expr_operator_kind)eok_comma,
                           second_operand->type, first_operand);
  if (second_operand->kind == (an_expr_operator_kind)enk_operation) {
    orig_expr->variant.operation.returns_lvalue_instead_of_usual_rvalue =
      second_operand->variant.operation.returns_lvalue_instead_of_usual_rvalue;
  }  /* if */
}  /* insert_expr */


void insert_statement(a_statement_ptr        statement,
                      an_insert_location_ptr insert_location)
/*
Insert the statement "statement" at *insert_location.  Update *insert_location
so the next insertion will be after the statement added.
*/
{
  a_statement_ptr         insert_stmt;
  a_switch_clause_ptr     scp;
  an_insert_location_kind kind = insert_location->kind;

  if (is_expr_insert_location_kind(kind)) {
    /* Insert within an expression. */
#if CHECKING
    if (statement->kind != (a_statement_kind)stmk_expr) {
      internal_error("insert_statement: cannot insert non-expr statement");
    }  /* if */
#endif /* CHECKING */
    /* Note that the expression statement is just discarded. */
    insert_expr(statement->expr, insert_location);
  } else {
    /* Insert in a statement sequence. */
    if (kind == ilk_switch_clause_start) {
      /* Insert at the start of a switch clause. */
      scp = insert_location->variant.switch_clause;
      statement->next = scp->statements;
      scp->statements = statement;
    } else {
      insert_stmt = insert_location->variant.stmt;
      if (kind == ilk_block_start) {
        /* Insert at the start of a block. */
        statement->next = insert_stmt->variant.block.statements;
        insert_stmt->variant.block.statements = statement;
      } else {
        check_assertion_str(kind == ilk_after_statement,
                            "insert_statement: bad insert location kind");
        /* Normal case -- insert after insert_stmt. */
        statement->next = insert_stmt->next;
        insert_stmt->next = statement;
      }  /* if */
    }  /* if */
    /* Set *insert_location for the next insertion. */
    set_insert_location(statement, insert_location);
    if (statement->kind != (a_statement_kind)stmk_init) {
      /* The statement inserted is an executable statement rather than an
         stmk_init.  Any stmk_init statements following this statement
         must have follows_an_exec_statement TRUE. */
      a_statement_ptr foll_stmt;
      for (foll_stmt = statement->next;
           foll_stmt != NULL;
           foll_stmt = foll_stmt->next) {
        if (foll_stmt->kind == (a_statement_kind)stmk_init) {
          foll_stmt->variant.dynamic_init->follows_an_exec_statement = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        } else if (foll_stmt->kind == (a_statement_kind)stmk_decl) {
          /* Ignore stmk_decl statements. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        } else {
          /* Some other kind of statement. */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* insert_statement */


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


void set_integer_constant_with_overflow_check(a_constant_ptr  con,
                                              long            con_val,
                                              an_integer_kind ikind)
/*
Set the constant "con" to the integer value "con_val" with integer kind
"ikind".  Check to make sure that the value will fit an integer of that size,
and if not, issue an error.  This version is for signed integer kinds.
*/
{
  a_boolean did_not_fold;

  /* Create the constant as a long and then change its type to get any
     truncation error. */
  set_integer_constant(con, con_val, (an_integer_kind)ik_long);
  type_change_constant(con, integer_type(ikind),
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE,
                       /*evaluated_context=*/TRUE,
                       /*fold_constant_addr_exprs=*/TRUE,
                       &did_not_fold, &error_position);
}  /* set_integer_constant_with_overflow_check */


void set_unsigned_integer_constant_with_overflow_check(
                                              a_constant_ptr  con,
                                              unsigned long   con_val,
                                              an_integer_kind ikind)
/*
Set the constant "con" to the integer value "con_val" with integer kind
"ikind".  Check to make sure that the value will fit an integer of that size,
and if not, issue an error.  This version is for unsigned integer kinds.
*/
{
  a_boolean did_not_fold;

  /* Create the constant as an unsigned long and then change its type to
     get any truncation error. */
  set_unsigned_integer_constant(con, con_val,
                                (an_integer_kind)ik_unsigned_long);
  type_change_constant(con, integer_type(ikind),
                       /*is_implicit_cast=*/TRUE,
                       /*constant_context=*/TRUE,
                       /*evaluated_context=*/TRUE,
                       /*fold_constant_addr_exprs=*/TRUE,
                       &did_not_fold, &error_position);
}  /* set_unsigned_integer_constant_with_overflow_check */


static void set_delta_constant(a_targ_ptrdiff_t delta,
                               a_constant_ptr   delta_con)
/*
Set the constant entry delta_con to the constant integer value given by
delta.  The value is an address offset.  Issue an error if the constant
will not fit in an integer of kind TARG_DELTA_INT_KIND.
*/
{
  set_integer_constant_with_overflow_check(delta_con, delta,
                                           TARG_DELTA_INT_KIND);
}  /* set_delta_constant */


void lower_ptr_to_member_constant(a_constant_ptr constant)
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
      set_routine_address_constant(routine, func_con,
                                   /*set_address_taken_flag=*/TRUE);
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
                         /*constant_context=*/TRUE,
                         /*evaluated_context=*/TRUE,
                         /*fold_constant_addr_exprs=*/TRUE,
                         &did_not_fold, &error_position);
    /* Change the original constant into a ck_aggregate constant. */
    set_constant_kind(constant, (a_constant_repr_kind)ck_aggregate);
    constant->type = NULL;
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
    if (constant->assoc_var_assigned) {
      assoc_var = (a_variable_ptr)constant->source_corresp.assoc_info;
    } else {
      /* The variable must be allocated. */
      (void)make_mptr_type();
      if (in_file_scope((char *)constant)) {
        /* The constant is in the file scope, so use a file-scope variable.
           The constant is possibly shared, but we're going to rewrite
           every use of it to reference the variable instead, so the constant
           will end up being used only in the initialization of the
           variable (and therefore unshared). */
        assoc_var = make_file_scope_temporary(mptr_type);
        /* Make the constant the initial value of the variable. */
        assoc_var->init_kind = (an_init_kind)initk_static;
        assoc_var->initializer.constant = constant;
      } else {
        /* The constant is in the function scope, so use a function-local
           static variable. */
        assoc_var = make_unnamed_local_static_variable(mptr_type,
                                                   /*in_function_scope=*/TRUE);
        /* To initialize a local static variable to an aggregate we use
           a local-static-variable-init entry (to avoid memory region
           problems). */
        (void)alloc_local_static_variable_init(assoc_var, 
                                               innermost_function_scope,
                                               (an_init_kind)initk_static,
                                               constant,
                                               (a_dynamic_init_ptr)NULL);
      }  /* if */
      /* Save the pointer in the assoc_info field so the variable can be
         reused. */
      constant->source_corresp.assoc_info = (char *)assoc_var;
      constant->assoc_var_assigned = TRUE;
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


void lower_constant(a_constant_ptr constant)
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
              set_variable_address_constant(temp_var, constant,
                                            /*set_address_taken_flag=*/TRUE);
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
    /* Don't follow a pointer from the function scope into the file scope;
       record it as a potential orphan instead.  Note that a class member
       can never be an orphan, so member constants are not recorded as
       orphans. */
    if (constant->source_corresp.class_of_which_a_member == NULL) {
      add_orphaned_file_scope_il_entry((char *)constant, iek_constant);
    }  /* if */
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
function.  At this point, the variable is created as an extern variable.
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
  array_type->variant.array.variant.number_of_elements = 0;  /* i.e., [] */
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
  mangled_name = alloc_lowered_name_string(alloc_length);
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
  vtbl_var = make_lowered_variable(mangled_name, /*already_il_name=*/TRUE,
                                   array_type, (a_storage_class)sc_extern);
  /* make_lowered_variable creates a variable with referenced set TRUE, but the
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


#if !CFRONT_OBJECT_CODE_COMPATIBILITY
/*ARGSUSED*/  /* <-- Because class_type is not used in that case. */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
static a_boolean base_class_needs_virtual_function_table(
                                                   a_base_class_ptr bcp,
                                                   a_type_ptr       class_type)
/*
Return TRUE if a virtual function table instance is needed for base class
bcp when it occurs as part of a complete object of class class_type.
FALSE means either the base class does not need a virtual function table
(at all) or that it can share another one.
*/
{
  a_boolean needed = FALSE;

  if (bcp->type->variant.class_struct_union.any_virtual_functions) {
    /* The base class declares virtual functions, so it needs a pointer
       to a virtual function table.  However, we may not need an
       instance of the function table specifically for bcp-in-class_type;
       some other instance may do. */
    if (bcp->shares_virtual_function_info) {
      /* The base class shares its virtual function table with a more-derived
          base class or with class_type itself, so it does not need its own
          virtual function table instance. */
      needed = FALSE;
    } else if (bcp->overriding_virtual_functions != NULL) {
      /* Some of the virtual functions in the base class are overridden
         in class_type, so a separate virtual function table instance is
         needed. */
      needed = TRUE;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    } else if (class_type->variant.class_struct_union.any_virtual_functions) {
      /* cfront puts outs virtual function tables whenever there is a virtual
         function in the derived class, which is more often than is really
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
      if (base_class_needs_virtual_function_table(bcp, class_type)) {
        if (bcp->virtual_function_table_var == NULL) {
          make_var_for_virtual_function_table(class_type, bcp);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* make_vars_for_virtual_function_tables */


static a_boolean virtual_function_table_should_be_defined_here(
                                                  a_type_ptr    class_type,
                                                  a_boolean     *force_static,
                                                  a_routine_ptr *first_virtual)
/*
Return TRUE if the virtual function tables for the class type class_type should
be defined (i.e., initialized) in this compilation.  Note that this should not
be called until the end of the file scope, as its value depends on whether
or not some function in the class has been defined.  If the virtual function
table should be forced to be local to this compilation, *force_static
is returned TRUE.  *first_virtual is set to point to the first noninline
virtual function of the class if that function was used in deciding whether
or not to put out the definition; otherwise, it's set to NULL.
*/
{
  a_boolean                   defined_here;
  a_class_type_supplement_ptr ctsp;
  a_scope_ptr                 scope;
  a_routine_ptr               routine;

  *force_static = FALSE;
  *first_virtual = NULL;
  ctsp = class_type->variant.class_struct_union.extra_info;
#if CHECKING
  if (ctsp == NULL) {
    internal_error(
                "virtual_function_table_should_be_defined_here: ctsp == NULL");
  }  /* if */
#endif /* CHECKING */
  /* The virtual function tables for a class are defined in the compilation
     that contains the definition of the lexically first non-inline, virtual,
     non-pure member function of the class.  See ARM 10.8.1c and "New Virtual
     Table Strategy" in the AT&T cfront 2.1 Release Notes. */
  if (class_type->source_corresp.name_linkage !=
                                 (a_name_linkage_kind)nlk_cplusplus_external) {
    /* Not C++ external linkage, therefore any definition of the virtual
       function table would have to be here, and static.  If the class is
       not referenced at all, then no definition is needed. */
    defined_here = class_type->source_corresp.referenced;
    *force_static = TRUE;
  } else {
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
          *first_virtual = routine;
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
         table.  See if a command-line option gives guidance. */
      if (virtual_function_table_definition == vfd_force) {
        defined_here = TRUE;
      } else if (virtual_function_table_definition == vfd_suppress) {
        defined_here = FALSE;
      } else {
        /* No command-line option.  Put out the virtual function table, but
           make it static, because each compilation with this same class
           will contain an instance of the definition. */
        defined_here = TRUE;
        *force_static = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
have_defined_here:;
  if (*force_static && defined_here) {
    /* If the definition is forced to be static, then it cannot be referenced
       from anywhere else.  If there aren't any (real) references in this
       compilation unit, then the definition isn't needed here either. */
    /* The virtual function table variable is marked as referenced for
       references to the virtual function table (which only occur in
       constructor and destructor wrapper code), so if the referenced flag
       is FALSE the virtual function table is not referenced at all. */
    a_variable_ptr vtbl_var = ctsp->virtual_function_table_var;
    if (vtbl_var == NULL) {
      /* The class itself has no virtual function table, so look at the
         base classes.  At least one of them must have one (otherwise, we
         wouldn't be asking whether a virtual function table should be
         defined).  Do the test on the first one found. */
      a_base_class_ptr bcp;
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        vtbl_var = bcp->virtual_function_table_var;
        if (vtbl_var != NULL) break;
      }  /* for */
    }  /* if */
    check_assertion(vtbl_var != NULL);
    if (!vtbl_var->source_corresp.referenced) {
      defined_here = FALSE;
    }  /* if */
  }  /* if */
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
at the end of the translation unit, but before IL lowering is done for
the file scope memory region.
*/
{
  a_boolean     should_generate = FALSE, force_static;
  a_routine_ptr first_virtual;

  if (il_lowering_needed()) {
    /* Force generation of the virtual function table variable (if any) for the
       class. */
    prelower_class_type(class_type);
    /* The class should have virtual function table, since it has a
       virtual destructor. */
    check_assertion(class_type->variant.class_struct_union.extra_info->
                                           virtual_function_table_var != NULL);
    /* See if the virtual function table will be defined in this
       compilation. */
    if (virtual_function_table_should_be_defined_here(class_type,
                                                      &force_static,
                                                      &first_virtual)) {
      /* The virtual function table will be defined, and it will have a
         reference to the virtual destructor, so the virtual destructor
         should be generated. */
      should_generate = TRUE;
    }  /* if */
  }  /* if */
  return should_generate;
}  /* virtual_dtor_should_be_generated_for_class */


/*
Pointer to routine entry for the runtime routine __pure_virtual_called,
a pointer to which is placed in virtual function table slots for
pure virtual functions.  NULL until allocated.
*/
static a_routine_ptr
		pure_virtual_called_routine;


#if !AUTOMATIC_TEMPLATE_INSTANTIATION
/*ARGSUSED*/ /* <-- first_virtual is not used if no automatic instantiation. */
#endif /* !AUTOMATIC_TEMPLATE_INSTANTIATION */
static void add_vtbl_entry_init(a_targ_ptrdiff_t delta,
                                a_routine_ptr    func_to_call,
                                a_constant_ptr   aggr_con,
                                a_routine_ptr    first_virtual)
/*
Create a ck_aggregate constant and dependent constants to initialize
an entry of a virtual function table to (delta, 0, func_to_call), and add the
constant to the end of the aggr_con list.  func_to_call may be NULL;
in that case, a NULL pointer is put out for the function.
If first_virtual is non-NULL, it points to the virtual function that
was used as the basis for a decision on whether or not to put out the
virtual function table.
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
    set_routine_address_constant(func_to_call, func_con,
                                 /*set_address_taken_flag=*/TRUE);
    implicit_cast(func_con, vptp_type);
    /* Mark the routine as referenced. */
    func_to_call->source_corresp.referenced = TRUE;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode) {
      /* If the function is a template function, now marked as referenced,
         an instantiation is now required somewhere.  Don't do this on the
         function that was used to decide to put out the virtual function
         table, since that function forces the virtual function table to be
         put out, and not the other way around. */
      if (func_to_call->is_template_function &&
          func_to_call != first_virtual) {
        func_to_call->instance_required = TRUE;
      }  /* if */
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
  /* The "i" field is set to zero -- it's not used in virtual function
     tables, only in pointers to member functions. */
  i_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_integer_constant(i_con, (long)0, TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
  /* Put together the aggregate constant. */
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
                                  a_virtual_function_number *next_entry_number,
                                  a_routine_ptr             first_virtual)
/*
aggr_con is the aggregate constant that initializes a virtual function table.
Add to it the constants for the entries that define the virtual function
table for base class bcp when it is contained within a whole object of
type class_type.  If bcp is NULL, generate the virtual function table for
class_type itself.  On exit, return in *next_entry_number the next entry
number after the last one filled.  If first_virtual is non-NULL, it points
to the virtual function that was used as the basis for a decision on
whether or not to put out the virtual function table.
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
        any_virtual_steps_in_derivation(sharing_bcp)) {
      internal_error("fill_virtual_function_table: bad vtbl sharing");
    }  /* if */
#endif /* CHECKING */
    /* Fill the part of the table that is shared with the immediate base
       class that is on the path to the base class that contains the shared
       pointer. */
    imm_bcp = sharing_bcp->derivation->path->base_class;
    if (bcp != NULL) {
      /* When doing this processing for a base class, we have to find the
         corresponding base class under class_type.  The base class we
         have was extracted from class_whose_vtbl_is_being_made. */
      imm_bcp = corresponding_base_class(imm_bcp, class_type,
                                         (a_base_class_ptr)NULL);
    }  /* if */
    fill_virtual_function_table(aggr_con, class_type, imm_bcp, &entry_number,
                                first_virtual);
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
    add_vtbl_entry_init(delta, func_to_call, aggr_con, first_virtual);
    /* The functions are usually in order by number so set up for the
       next iteration in the common case. */
    primary_function = primary_function->next;
  }  /* for */
  *next_entry_number = entry_number;
}  /* fill_virtual_function_table */


static void define_one_virtual_function_table(
                                            a_type_ptr       class_type,
                                            a_base_class_ptr bcp,
                                            a_boolean        definition_needed,
                                            a_boolean        force_static,
                                            a_routine_ptr    first_virtual)
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
If first_virtual is non-NULL, it points to the virtual function that
was used as the basis for a decision on whether or not to put out the
virtual function table.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_variable_ptr              vtbl_var;
  a_constant_ptr              aggr_con;
  a_virtual_function_number   next_entry_number;
  a_memory_region_number      region_to_switch_back_to;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  a_type_ptr                  class_whose_vtbl_is_being_made;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */

  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Find the appropriate virtual function table variable. */
  if (bcp == NULL) {
    /* We're doing the virtual function table for class_type itself. */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    class_whose_vtbl_is_being_made = class_type;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
    ctsp = class_type->variant.class_struct_union.extra_info;
    vtbl_var = ctsp->virtual_function_table_var;
  } else {
    /* We're doing the virtual function table for bcp in class_type. */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    class_whose_vtbl_is_being_made = bcp->type;
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
    ctsp = bcp->type->variant.class_struct_union.extra_info;
    vtbl_var = bcp->virtual_function_table_var;
  }  /* if */
  /* Change the array size from [] to the proper size.  Note that the type
     was created for this variable and is known not to be shared. */
  /* The "+1" is to skip the [0] entry, which makes the code to access
     the table a little cleaner.  It's also necessary for cfront
     compatibility. */
  vtbl_var->type->variant.array.variant.number_of_elements =
                                     ctsp->highest_virtual_function_number + 1
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  /* Add an extra zeroed entry at the end of the table for full cfront
     compatibility (although we don't know why the entry is there). */
                                     + 1
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                                              ;
  set_type_size(vtbl_var->type);
  /* Set the linkage on the virtual function table variable. */
  if (force_static) {
    /* When told to by the flag force_static (e.g., for an internally-linked
       class or one with no linkage), change the storage class to
       static and the linkage to internal. */
    vtbl_var->storage_class = (a_storage_class)sc_static;
    vtbl_var->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  } else if (definition_needed) {
    /* For an externally-linked class whose definition is needed, change the
       variable to an external definition. */
    vtbl_var->storage_class = (a_storage_class)sc_unspecified;
    /* The variable can be referenced from another compilation unit. */
    vtbl_var->source_corresp.referenced = TRUE;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    /* If exceptions are enabled, force generation of the typeinfo variable
       for the type because it might be referenced from some other compilation
       unit. */
    if (exceptions_enabled) {
      type_is_used_in_exception(class_whose_vtbl_is_being_made);
    }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
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
    add_vtbl_entry_init((a_targ_ptrdiff_t)0, (a_routine_ptr)NULL, aggr_con,
                        first_virtual);
    /* Put out the body of the table. */
    fill_virtual_function_table(aggr_con, class_type, bcp, &next_entry_number,
                                first_virtual);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* Put out the initialization for an extra zeroed entry at the end, for
       cfront compatibility. */
    add_vtbl_entry_init((a_targ_ptrdiff_t)0, (a_routine_ptr)NULL, aggr_con,
                        first_virtual);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* define_one_virtual_function_table */


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
  a_routine_ptr               first_virtual;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  a_boolean                   any_vtbl_ref = FALSE;
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

  /* Make sure the class type has been pre-lowered. */
  prelower_class_type(class_type);
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    if (ctsp->virtual_function_table_var != NULL) {
      /* The class has a virtual function table.  Generate the definition
         if it is supposed to be generated in the present compilation. */
      definition_needed = 
                 virtual_function_table_should_be_defined_here(class_type,
                                                               &force_static,
                                                               &first_virtual);
      need_determined = TRUE;
      /* Generate the virtual function table for the class itself. */
      define_one_virtual_function_table(class_type, (a_base_class_ptr)NULL,
                                        definition_needed, force_static,
                                        first_virtual);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
      /* Remember whether or not there's any reference to a virtual function
         table. */
      if (ctsp->virtual_function_table_var->source_corresp.referenced) {
        any_vtbl_ref = TRUE;
      }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
    }  /* if */
    /* Generate the virtual function table for each base class when it
       is contained within a complete object of the primary class. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->virtual_function_table_var != NULL) {
        if (!need_determined) {
          definition_needed = 
                 virtual_function_table_should_be_defined_here(class_type,
                                                               &force_static,
                                                               &first_virtual);
          need_determined = TRUE;
        }  /* if */
        define_one_virtual_function_table(class_type, bcp,
                                          definition_needed, force_static,
                                          first_virtual);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
        /* Remember whether or not there's any reference to a virtual
           function table. */
        if (bcp->virtual_function_table_var->source_corresp.referenced) {
          any_vtbl_ref = TRUE;
        }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
      }  /* if */
    }  /* for */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode &&
        ctsp->template_arg_list != NULL && any_vtbl_ref &&
        first_virtual != NULL && first_virtual->is_template_function) {
      /* Automatic template instantiation is being done.  The class is a
         template class with virtual functions, and the decision on whether
         or not to put out the virtual function table is based on the function
         first_virtual, which can be generated from a template.  There was
         a reference to some virtual function table related to the class
         (e.g., a reference from a constructor) in this compilation, which
         means that the virtual function table needs to be generated
         somewhere in the program.  Therefore, somewhere in the program
         there needs to be an instance of first_virtual (so that
         the virtual function table will be generated at that point).  Set
         the instance_required flag unless there is a body for first_virtual
         in this compilation. */
      if (first_virtual->assoc_scope == NULL_region_number) {
        first_virtual->instance_required = TRUE;
      }  /* if */
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
}  /* define_virtual_function_tables */


static void define_scope_virtual_function_tables(a_scope_ptr scope)
/*
Make all needed definitions of virtual function tables for all classes
in the indicated scope.

Note that the virtual function table variables were created during
prelowering; this routine adds initial values to those variables if
appropriate.  This routine is called at the beginning of lowering of
each memory region, because it must be called (1) after all function
definitions have been seen (because deciding on defining virtual function
tables depends on knowing whether member functions of the class are
defined in the present compilation) and (2) before members have been
promoted out of classes (because that destroys the list of member
functions, including virtual functions, needed in this process).
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
#define SUB_PREFIX "_"
      name_length = mangled_class_name(class_type, (char *)NULL) +
                    sizeof(SUB_PREFIX) - 1;
      alloc_length = name_length + 1;
      new_name_ptr = alloc_lowered_name_string(alloc_length);
      (void)memcpy(new_name_ptr, SUB_PREFIX, size_t_arg(sizeof(SUB_PREFIX)-1));
      (void)mangled_class_name(class_type,
                               new_name_ptr + (sizeof(SUB_PREFIX)-1));
      new_name_ptr[name_length] = '\0';
      subobject_type->source_corresp.name = new_name_ptr;
#undef SUB_PREFIX
    }  /* if */
    subobject_type->source_corresp.decl_position = 
                                      class_type->source_corresp.decl_position;
    subobject_type->source_corresp.class_of_which_a_member =
                            class_type->source_corresp.class_of_which_a_member;
#if 0
    /* Ideally, the referenced flag would not be set if the class type is
       not referenced.  However, the class type might not be referenced now
       (part-way through the compilation) and then be referenced later. */
#endif /* 0 */
    subobject_type->source_corresp.referenced = TRUE;
    /* Put the struct type on the file-scope types list right after the
       associated type.  This is done instead of calling add_to_types_list
       because we want to get the type at the right place on the list.
       If the class type is the last entry on some type list,
       use add_to_types_list to get the last_type pointer updated. */
    if (class_type->next == NULL) {
      /* The class type is the last on a list.  See if the list is one of the
         ones being tracked in the scope stack. */
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
#endif /* 0 */
  }  /* if */
  ctsp->type_as_subobject = subobject_type;
}  /* make_subobject_class_type */


void prelower_class_type(a_type_ptr class_type)
/*
Do processing that is required early in lowering for the indicated class
type.  Such processing builds information that is necessary during the
lowering process, but does not modify the class type.  Note that this
routine assumes the class type is as complete as it will ever get.
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
      /* If the class has a definition, the processing of the definition
         should be complete. */
      check_assertion_str(ctsp->assoc_scope == NULL || class_type->size != 0,
                        "prelower_class_type: class definition not completed");
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
          if (bcp->pointer_base_class == NULL) {
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


static void lower_type_list(a_type_ptr type_list)
/*
Do IL lowering of the indicated list of types and everything under it.
*/
{
  a_type_ptr type;

  for (type = type_list; type != NULL; type = type->next) {
    lower_type(type);
  }  /* for */
}  /* lower_type_list */


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
      /* There is a definition for the class. */
      /* Lower the base classes. */
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        lower_type(bcp->type);
      }  /* for */
      /* Lower the member functions, local types, etc.  Note that nothing
         is promoted out of the class at this point; the promotions get done
         at the end of lowering the memory region of which this is a class.
         Note that in the case of a local class, the promotion may be done
         at the end of lowering the function scope memory region, and this
         routine is not called until later (when processing orphans for
         the file scope).  In that case, the scope is already empty here
         and the erstwhile members get lowered in their promoted positions. */
      lower_scope(ctsp->assoc_scope);
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, lower them now too. */
      lower_type_list(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
    /* Lower the template argument list, if any. */
    lower_template_arg_list(ctsp->template_arg_list);
    /* Lower the type-as-subobject. */
    lower_type(ctsp->type_as_subobject);
  }  /* if */
  if (class_type->kind == (a_type_kind)tk_class) {
    class_type->kind = (a_type_kind)tk_struct;
  }  /* if */
  error_position = saved_error_position;
}  /* lower_class_struct_union_type */


static void lower_constructor_routine_type(a_type_ptr routine_type)
/*
Do IL lowering of a constructor routine type.  The lowering applicable to
all routine types has already been done.  The routine body (if any) is
not lowered at this time (see lower_constructor_code).
*/
{
  a_type_ptr       class_type, subobject_type;
  a_param_type_ptr first_param, added_param, prev_param;
  a_base_class_ptr bcp;

  routine_type = skip_typerefs(routine_type);
  /* Get the "this" parameter entry.  The routine type has already been
     lowered, so it's the first on the list. */
  first_param = routine_type->variant.routine.extra_info->param_type_list;
  /* Get the class type from the "this" parameter type. */
  class_type = type_pointed_to(first_param->type);
  class_type = skip_typerefs(class_type);
  prelower_class_type(class_type);
  /* Add a parameter for each virtual base class.  See the ARM, top of
     p. 296.  add_constructor_params does the similar processing for param
     variables. */
  /* If you change this, see also unlowered_param_type_list. */
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
}  /* lower_constructor_routine_type */


static void lower_destructor_routine_type(a_type_ptr routine_type)
/*
Do IL lowering of a destructor routine type.  The lowering applicable to
all routine types has already been done.  The routine body (if any) is
not lowered at this time (see lower_destructor_code).
*/
{
  a_type_ptr       class_type;
  a_param_type_ptr first_param, added_param;

  routine_type = skip_typerefs(routine_type);
  /* Get the "this" parameter entry.  The routine type has already been
     lowered, so it's the first on the list. */
  first_param = routine_type->variant.routine.extra_info->param_type_list;
  /* Get the class type from the "this" parameter type. */
  class_type = type_pointed_to(first_param->type);
  class_type = skip_typerefs(class_type);
  prelower_class_type(class_type);
  /* Add an int parameter that will indicate whether or not we have a
     complete object and whether or not the storage should be freed.
     add_destructor_params does the similar processing for param variables. */
  /* If you change this, see also unlowered_param_type_list. */
  added_param = alloc_param_type(integer_type((an_integer_kind)ik_int));
  /* Note that the original parameter entries have already been lowered,
     so it is not necessary to set il_lowering_flag to ensure that the
     whole list will be visited. */
  added_param->next = first_param->next;
  first_param->next = added_param;
}  /* lower_destructor_routine_type */


void lower_type(a_type_ptr type)
/*
Do IL lowering of the indicated type and everything under it.
*/
{
  a_type_ptr	ptr_return_type, new_type, type_next, member_type;
  a_type_ptr	copy_of_pm_type;
  a_based_type_list_member_ptr
		btlmp;

  /* Note that within this routine "lower_os_type" need not be used.
     The fact that we are lowering a type means we are lowering the
     file scope and therefore no other type can be in a different
     memory region. */
  if (!visited_yet(type)) {
    mark_as_visited(type);
    lower_source_correspondence(&type->source_corresp);
    /* Lower the based types list (it points to types based on the present
       type, e.g., pointer-to the present type). */
    for (btlmp = type->based_types; btlmp != NULL; btlmp = btlmp->next) {
      lower_type(btlmp->based_type);
    }  /* for */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    if (type->used_in_exception) {
      /* If the type was used in an exception context, generate typeinfo
         information for it. */
      type_is_used_in_exception(type);
    }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
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
          if (new_type->size != targ_sizeof_ptr_to_data_member ||
              new_type->alignment != targ_alignof_ptr_to_data_member) {
            internal_error(
           "lower_type: target config of pointer-to-data-member is incorrect");
          }  /* if */
#endif /* CHECKING */
        }  /* if */
        /* Make a copy of the original pointer-to-member type.  Note that
           this copy is for the use of IL lowering; it is not really part
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
          if (make_all_functions_unprototyped) {
            /* Make all function types unprototyped.  Note that the
               param_type_list is not cleared even if the function has no
               body.  This can create a function type with prototyped == FALSE,
               assoc_routine == NULL, and param_type_list != NULL, which is
               not otherwise possible. */
            rtsp->prototyped = FALSE;
            /* We do not change old_style_params_scanned, because it's used in
               another part of lowering to tell whether the interface used to
               be old-style.  Back ends must watch out for that. */
            /* We do not clear has_ellipsis on purpose.  The C-generating
               back end depends on it in this case. */
          }  /* if */
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
              if (keep_object_lifetime_info_in_lowered_il) {
                an_expr_node_ptr expr = ptp->default_arg_expr;
                if (expr != NULL &&
                    expr->kind == (an_expr_node_kind)enk_object_lifetime) {
                  /* This expression has an object lifetime, and we'll be
                     keeping information on object lifetimes.  This part of
                     it, however, we throw away, because the expression
                     isn't part of the IL tree any more. */
                  eliminate_object_lifetime_tree(
                                            expr->variant.object_lifetime.ptr);
                }  /* if */
              }  /* if */
              ptp->default_arg_expr = NULL;
            }  /* if */
          }  /* for */
          if (rtsp->prototype_scope != NULL) {
            lower_scope(rtsp->prototype_scope);
          }  /* if */
          /* Do special lowering for constructors and destructors. */
          if (rtsp->assoc_routine_is_ctor) {
            lower_constructor_routine_type(type);
          } else if (rtsp->assoc_routine_is_dtor) {
            lower_destructor_routine_type(type);
          }  /* if */
        }
        break;
      case tk_array:
        lower_type(type->variant.array.element_type);
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        lower_class_struct_union_type(type);
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


static void lower_initializer(an_init_kind       init_kind,
                              an_initializer_ptr initializer)
/*
Lower an initializer, which might be in a variable or a
local-variable-static-init entry.
*/
{
  switch (init_kind) {
    case initk_none:
    case initk_zero:
      break;
    case initk_static:
      lower_constant(initializer->constant);
      break;
    case initk_dynamic:
      /* The dynamic init entry is either pointed to from a stmk_init
         entry or appears on the file-scope dynamic inits list.  Handle
         it when seen in one of those places. */
      break;
    case initk_function_local:
      /* Initialization of a local static variable that is described
         remotely by a local-static-variable-init.  Do nothing here;
         the initialization is lowered from the function or block scope,
         by scanning the local_static_variable_inits list.
         That's necessary because the local static variable will be lowered
         as part of the file scope, and the initialization (in the function
         scope memory region) is gone by then. */
      break;
#if CHECKING
    default:
      internal_error("lower_initializer: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* lower_initializer */


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
    } else if (variable->storage_class == (a_storage_class)sc_unspecified &&
               variable->init_kind == (an_init_kind)initk_none) {
      /* In C++, there are no tentative definitions.  Use initk_zero to
         indicate that this variable is "really" defined. */
      variable->init_kind = (an_init_kind)initk_zero;
    }  /* if */
    if (var_is_return_value_variable(variable)) {
      /* The variable is the return value optimization variable for the
         function.  All references to it will be rewritten to refer instead
         to the implicit parameter that gives the return-copy address.
         Mark the variable as unreferenced.  Also turn off any initialization.
         If there is initialization, the initialization will be put out as
         an assignment when the stmk_init for this initialization is
         processed. */
      variable->source_corresp.referenced = FALSE;
      variable->init_kind = (an_init_kind)initk_none;
    }  /* if */
    /* Lower the initializer if any. */
    lower_initializer(variable->init_kind, &variable->initializer);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode &&
        variable->source_corresp.class_of_which_a_member != NULL) {
      /* Static data member. */
      if (variable->instance_required) {
        /* This variable is template-based. */
        make_instantiation_info_var("__TIR__", &variable->source_corresp);
      }  /* if */
      if (variable->do_not_instantiate) {
        /* This variable cannot be instantiated. */
        make_instantiation_info_var("__DNI__", &variable->source_corresp);
      }  /* if */
      if (variable->can_be_instantiated) {
        /* This variable can be instantiated. */
        make_instantiation_info_var("__CBI__", &variable->source_corresp);
      }  /* if */
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  }  /* if */
}  /* lower_variable */


static void lower_local_static_variable_init_list(
                                        a_local_static_variable_init_ptr lsvip)
/*
Lower a list of local static variable initialization descriptions.  These
are used to represent (in the function scope memory region) initialization
of local static variables (which are in the file scope memory region
and therefore cannot point at the initialization in the function scope
memory region).
*/
{
  for (; lsvip != NULL; lsvip = lsvip->next) {
    lower_initializer(lsvip->init_kind, &lsvip->initializer);
  }  /* for */
}  /* lower_local_static_variable_init_list */


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
    /* Clear the befriending classes field to make the routine entry legal
       C IL. */
    routine->befriending_classes = NULL;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
    if (automatic_instantiation_mode) {
      /* For automatic instantiation, generate a variable or variables with
         names that encode instantiation information. */
      if (routine->instance_required) {
        /* This routine is template-based. */
        make_instantiation_info_var("__TIR__", &routine->source_corresp);
      }  /* if */
      if (routine->do_not_instantiate) {
        /* This routine cannot be instantiated. */
        make_instantiation_info_var("__DNI__", &routine->source_corresp);
      }  /* if */
      if (routine->can_be_instantiated) {
        /* This routine can be instantiated. */
        make_instantiation_info_var("__CBI__", &routine->source_corresp);
      }  /* if */
    }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
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


void lower_expr_list(an_expr_node_ptr expr_list,
                     unsigned int     is_lvalue_mask)
/*
Do IL lowering of the indicated list of expressions and everything under it.
is_lvalue_mask is a bit mask indicating which elements of the list are
lvalues (0x1 for first operand, 0x2 for second operand, etc.)
*/
{
  an_expr_node_ptr expr;

  for (expr = expr_list; expr != NULL; expr = expr->next) {
    /* Lower the expression on the list. */
    lower_expr(expr, (a_boolean)(is_lvalue_mask & 1));
    /* Move to the next bit in the lvalue mask. */
    is_lvalue_mask >>= 1;
  }  /* for */
}  /* lower_expr_list */


a_param_type_ptr unlowered_param_type_list(a_type_ptr routine_type)
/*
routine_type is a lowered or unlowered routine type.  Return the original
unlowered parameter type list for the routine, i.e., if the type has been
lowered, return the list after any implicit parameters added by lowering.
*/
{
  a_param_type_ptr              param;
  a_routine_type_supplement_ptr rtsp;

  routine_type = skip_typerefs(routine_type);
  rtsp = routine_type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  /* Special processing is needed only if the type has already been lowered. */
  if (visited_yet(routine_type)) {
    /* The routine type has already been lowered, so advance past the
       extra arguments added by lowering, if any. */
    /* "this" parameter. */
    if (rtsp->implicit_this_param_type != NULL) param = param->next;
    if (rtsp->assoc_routine_is_ctor) {
      /* For constructors, a parameter is added for each virtual base
         class. */
      /* Get the class type from the "this" parameter type. */
      a_type_ptr class_type = type_pointed_to(rtsp->implicit_this_param_type);
      class_type = skip_typerefs(class_type);
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        a_base_class_ptr bcp;
        for (bcp = class_type->variant.class_struct_union.extra_info->
                                                                  base_classes;
             bcp != NULL;
             bcp = bcp->next) {
          if (bcp->is_virtual) param = param->next;
        }  /* for */
      }  /* if */
    } else if (rtsp->assoc_routine_is_dtor) {
      /* For destructors, a single parameter is always added. */
      param = param->next;
    }  /* if */
    /* If the routine returns its value via a copy constructor, an extra
       parameter is used to pass the address for the return value. */
    if (rtsp->value_returned_by_cctor) param = param->next;
  }  /* if */
  return param;
}  /* unlowered_param_type_list */


void lower_arg_expr_list(an_expr_node_ptr expr_list,
                         a_type_ptr       called_rout_type,
                         a_param_type_ptr param)
/*
Do IL lowering of the indicated list of expressions and everything under it.
The expressions are the argument list for a call.  The type of the routine
being called is called_rout_type (the type may be lowered or not).  Note
that if the routine requires control arguments like a "this" pointer, such
arguments are *not* in expr_list.  If param is non-NULL, start at that
parameter (this is used when the called routine is a copy constructor,
to skip the input parameter).
*/
{
  an_expr_node_ptr              expr;
  a_routine_type_supplement_ptr rtsp;

  called_rout_type = skip_typerefs(called_rout_type);
  rtsp = called_rout_type->variant.routine.extra_info;
  /* Get the first parameter type. */
  if (param != NULL) {
    /* The caller is telling us where to start in the list. */
  } else if (rtsp->old_style_params_scanned) {
    /* Old-style parameter list, so no parameter information. */
    /* Note that we do not test rtsp->prototyped because it may have been
       cleared by lowering when make_all_functions_unprototyped is TRUE. */
    param = NULL;
  } else {
    /* Start with the first parameter. */
    param = unlowered_param_type_list(called_rout_type);
  }  /* if */
  /* Track the current parameter type as we go through the list. */
  for (expr = expr_list; expr != NULL; expr = expr->next) {
    lower_expr(expr, FALSE);
    if (param != NULL) {
      /* Prototyped parameter. */
      if (make_all_functions_unprototyped) {
        /* Do default argument promotions on any arguments that need it,
           because they were generated for a call to a prototyped function, but
           we're changing all functions to unprototyped (for cfront
           compatibility). */
        do_default_arg_promotions_on_node(expr);
      }  /* if */
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
                                                     targ_size_t_int_kind);
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
                               a_boolean        *complete_object,
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
to virtual base classes.  When a virtual step is found while descending,
virtual_step_class is set to the virtual base class type.  Then, when the
bottom is reached, we can set *base_class_for_virtual_step to correspond
to the hop from the bottommost type to the step of the virtual base class,
and also set *complete_object to indicate whether or not the underlying
object is a complete object (if so, the virtual step can be done without
an indirection).  Then, on the ascent, we suppress the casting steps
preceding the virtual step, and generate optimized code for the hop to
the virtual step.  The recursive descent also has the advantage that it
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
  *complete_object = FALSE;
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
    bcp = find_direct_or_virtual_base_class_of(source_class, dest_class);
  } else {
    bcp = find_direct_or_virtual_base_class_of(dest_class, source_class);
  }  /* if */
  /* If this step is to a virtual base class, and no previous step was a
     step to a virtual base class, pass the virtual base class type down
     in the recursive processing to let the bottom-most call deal with
     the virtual step.  One would think that, because the front end
     standardizes derivations to/through a virtual base class as a single
     initial leap to the virtual base class, there would never be another
     (nonvirtual) cast below (in front of) a virtual step.  Such things
     can occur, however, when two cast sequences abut.  By doing the
     processing in this way, we can process the sequences as one sequence
     and optimize the step to the virtual base class. */
  handle_virtual_at_this_level = FALSE;
  if (virtual_step_class != NULL) {
    /* A previous step (higher up the tree, i.e., further in the sequence
       of casts) was a step to a virtual base class, so we do not test here. */
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
                            complete_object,
                            &source_node,
                            derived_class_cast_offset);
  } else {
    /* The node below this one is not another cast, so we have reached the
       bottom of the sequence of casts. */
    /* Lower the source expression. */
    lower_expr(source_node, is_lvalue);
    /* The offsets for derived class casts are summed on the way back up. */
    *derived_class_cast_offset = 0;
    if (virtual_step_class != NULL) {
      /* There was a virtual step somewhere in the sequence of casts.
         The overall cast can be viewed as an initial hop to the bottommost
         virtual base class (this hop possibly corresponding to several cast
         steps in the expression) followed by any remaining (non-virtual)
         cast steps.  If we start with a complete object, the cast to the
         virtual base class can be done without an indirection. */
#if CHECKING
      /* Cannot cast up from a virtual base class. */
      if (derived) {
        internal_error("related_class_cast_step: derived cast is virtual");
      }  /* if */
#endif /* CHECKING */
      /* Find the base class entry that relates the source class
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
      if (node_complete_object_type(source_node, /*call_case=*/FALSE) ==
                                                                source_class) {
        /* We have a complete object, so it is possible to go directly to the
           virtual base class without using a pointer indirection. */
        *complete_object = TRUE;
        /* Keep track of whether or not the cast involves a non-zero offset. */
        if (virt_bcp->offset != 0) any_nonzero_offset = TRUE;
      }  /* if */
    }  /* if */
    /* See if we need code to preserve NULL values.  NULL is supposed to
       pass through a derived class cast unaltered.  We can suppress the
       special code if the expression being cast can be assumed to be
       non-NULL or if the transformation is a do-nothing transformation
       (the offset is zero). */
    if (is_lvalue) {
      /* The result of this cast is being used as an lvalue, so it must be
         non-NULL. */
      need_null_preservation_code = FALSE;
    } else if (cannot_be_null(source_node)) {
      /* The source address is known not to be NULL. */
      need_null_preservation_code = FALSE;
    } else if (virtual_step_class != NULL && !*complete_object) {
      /* There is a virtual step in the casts and it cannot be optimized
         as an offset within a complete object.  Therefore a pointer
         indirection will be required in the sequence and the
         NULL-preservation code is required. */
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
      /* The casts add up to an identity transformation, so the NULL-
         preservation code is not needed. */
      need_null_preservation_code = FALSE;
    }  /* if */
    *null_preservation_source_node = NULL;
    if (need_null_preservation_code) {
      /* Start the code to preserve a NULL value through the casts.
         This involves making a reusable copy of the current source node
         and passing it up to lower_related_class_cast which will
         call add_null_preservation_code. */
      *null_preservation_source_node = source_node;
      source_node = make_reusable_copy(source_node, /*vars_can_change=*/FALSE);
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
       the total.  The addition here is guaranteed not to overflow because
       it must be within one object. */
    *derived_class_cast_offset += bcp->offset;
  } else if (*base_class_for_virtual_step != NULL) {
    /* There is a virtual base class step in the sequence of casts.
       If the present level is the level of the applicable step,
       process the cast now.  Otherwise, do nothing, because the
       present step is elided by the virtual step optimization. */
    if (!handle_virtual_at_this_level) {
      /* Do nothing; the present cast is elided. */
    } else {
      /* Generate the code for the virtual base class cast.  There are two
         subcases: when the source is known to be a complete object (in
         which case no indirection is needed), and when it is not. */
      source_node = make_vbase_class_lvalue(source_node,
                                            *base_class_for_virtual_step,
                                            *complete_object);
      /* Now that we've dealt with the virtual hop, put things back to normal
         for levels above this one. */
      *base_class_for_virtual_step = NULL;
    }  /* if */
  } else {
    /* Non-virtual step. */
    source_node = make_base_class_lvalue(source_node, bcp,
                                         /*complete_object=*/FALSE);
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
  a_boolean        complete_object;

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
                          &complete_object,
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
#endif /* 0 */
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
    bcp = find_direct_or_virtual_base_class_of(source_class, dest_class);
    if (bcp->is_virtual) {
      /* For a virtual base class skip, assume that we have a whole object
         and compute the offset from there.  The C++ language should probably
         prohibit casts like this, because they cannot be implemented with a
         simple offset. */
      source_class = pm_class_type_possibly_lowered((*underlying_node)->type);
      bcp = find_virtual_base_class_of(source_class, dest_class);
      *offset = -(a_targ_ptrdiff_t)(bcp->offset);
    } else {
      /* Non-virtual base class.  Subtract the offset from the running
         total. */
      *offset -= bcp->offset;
    }  /* if */
  } else {
    /* Casting from a base class to a derived class. */
    bcp = find_direct_or_virtual_base_class_of(dest_class, source_class);
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
  a_type_ptr       dest_type = node->type;

  /* Use recursion to find a chain of similar casts and compute the overall
     class offset for the chain. */
  compute_pm_cast_offset(node, &source_node, &offset);
  /* Lower the underlying subtree. */
  lower_expr(source_node, is_lvalue);
  if (offset == 0) {
    /* When the offset is zero no cast is needed.  Overwrite the original
       node with the underlying node. */
    overwrite_node(node, source_node);
    node->type = dest_type;
  } else {
    /* The offset is non-zero, so some work is needed. */
    /* Make a node for the offset constant. */
    set_delta_constant(offset, &offset_constant);
    offset_node = alloc_node_for_constant(&offset_constant);
    if (is_or_was_ptr_to_member_function_type(dest_type)) {
      /* Pointer to member function.  Change the node to
           (temp = pmf, (temp.i != 0) ? temp.d += offset : 0, temp)
      */
      /* Create the temporary. */
      temp_var = make_lowered_temporary(make_mptr_type());
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
      /* Note the use of dest_type here to ensure that information on the
         original pointer-to-member-function type is available in case
         there is an eok_pm_call operation above this one. */
      set_node_operator(node, (an_expr_operator_kind)eok_comma,
                        dest_type, comma_node);
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
      source_node = make_reusable_copy(source_node, /*vars_can_change=*/FALSE);
      source_node->next = offset_node;
      plus_node = make_operator_node((an_expr_operator_kind)eok_iadd,
                                     source_node->type, source_node);
      /* Make the "?" operation by overwriting the original node. */
      compare_node->next = plus_node;
      plus_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
      set_node_operator(node, (an_expr_operator_kind)eok_question,
                        dest_type, compare_node);
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


void lower_virtual_function_call(an_expr_node_ptr expr)
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
  vtbl_temp_var = make_lowered_temporary(vtbl_entry_node->type);
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
  object_node = make_reusable_copy(object_node, /*vars_can_change=*/FALSE);
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
  func_select_node->next = object_node;
  object_node->next = additional_args;
  change_node_to_operation(func_node, (an_expr_operator_kind)eok_call,
                           expr->type, func_select_node);
  /* Reuse the original eok_virtual_call node as a comma operator node and
     attach the vtbl_temp assignment and the eok_call nodes under it as
     operands. */
  assign_node->next = func_node;
  set_node_operator(expr, (an_expr_operator_kind)eok_comma, expr->type,
                    assign_node);
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
  an_expr_node_ptr func_addr_node, func_temp_node, func_temp_assign_node;
  a_variable_ptr   this_temp_var, func_temp_var, vtbl_temp_var;
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
        (func_temp = (function_type *) -- The computed function address
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
                    vtbl_temp->f)),    -- Address of virtual function to call.
         eok_call(func_temp,           -- Call the function.
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
  this_temp_var = make_lowered_temporary(object_type);
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
  if (class_type->variant.class_struct_union.extra_info->assoc_scope != NULL &&
      !class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    /* No virtual functions, so use the simpler form. */
    /* Make "pmf.f". */
    pmf_node = make_reusable_copy(pmf_node, /*vars_can_change=*/FALSE);
    select_f_node = node_to_select_field_from_rvalue(pmf_node, mptr_f_field);
    /* Add the cast to the right pointer to routine type. */
    func_addr_node = add_cast_if_necessary(select_f_node, ptr_routine_type);
  } else {
    /* Virtual functions, so use the more general form. */
    /* Make "pmf.i < 0". */
    pmf_node = make_reusable_copy(pmf_node, /*vars_can_change=*/FALSE);
    select_i_node = node_to_select_field_from_rvalue(pmf_node, mptr_i_field);
    select_i_node->next = node_for_integer_constant(0L,
                                         TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND);
    compare_node = make_operator_node((an_expr_operator_kind)eok_ilt,
                                      integer_type((an_integer_kind)ik_int),
                                      select_i_node);
    /* Make "pmf.f". */
    pmf_node = make_reusable_copy(pmf_node, /*vars_can_change=*/FALSE);
    select_f_node = node_to_select_field_from_rvalue(pmf_node, mptr_f_field);
    /* Make "*(__vtbl_entry **)((char *)this_temp + (short)(pmf.f))", which
       is the address of the virtual function table. */
    pmf_node = make_reusable_copy(pmf_node, /*vars_can_change=*/FALSE);
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
    pmf_node = make_reusable_copy(pmf_node, /*vars_can_change=*/FALSE);
    offset_node = node_to_select_field_from_rvalue(pmf_node, mptr_i_field);
    /* Add the virtual function table address and the offset, giving the
       address of the virtual function table entry, and store that in
       "vtbl_temp". */
    vtbl_addr_node->next = offset_node;
    padd_node = make_operator_node((an_expr_operator_kind)eok_padd,
                                   ptr_to_vtbl_entry_type, vtbl_addr_node);
    /* Make the temporary variable for the "vtbl_temp". */
    vtbl_temp_var = make_lowered_temporary(ptr_to_vtbl_entry_type);
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
       operator to the proper function type. */
    func_addr_node = add_cast_if_necessary(question_mark_node,
                                           ptr_routine_type);
    /* Store it in func_temp. */
    func_temp_var = make_lowered_temporary(ptr_routine_type);
    func_temp_node = var_lvalue_expr(func_temp_var);
    func_temp_node->next = func_addr_node;
    func_temp_assign_node = make_operator_node(
                                            (an_expr_operator_kind)eok_passign,
                                            ptr_routine_type,
                                            func_temp_node);
    /* Combine the assignment to this_temp and the assignment to
       func_temp into one expression using a comma operator. */
    this_temp_assign_node->next = func_temp_assign_node;
    this_temp_assign_node = make_operator_node(
                                              (an_expr_operator_kind)eok_comma,
                                              ptr_routine_type,
                                              this_temp_assign_node);
    /* Get the address of the function from func_temp for the call. */
    func_addr_node = var_rvalue_expr(func_temp_var);
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
     this_temp (and perhaps also func_temp) and the call under it. */
  this_temp_assign_node->next = call_node;
  set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                    expr->type, this_temp_assign_node);
}  /* lower_pm_call */


void lower_call(an_expr_node_ptr      expr,
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
  /* Note that the routine type can be lowered or unlowered at this point.
     Usually it will be unlowered. */
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
    temp_node = make_init_entity_node(ipdp, /*using_as_address=*/TRUE,
                                      /*using_as_dest=*/TRUE);
    temp_node->next = arg_node;
    prev_arg_node->next = temp_node;
    /* Change the result type of the call to "void". */
    expr->type = void_type();
  }  /* if */
  /* Lower the rest of the arguments. */
  lower_arg_expr_list(arg_node, rout_type, (a_param_type_ptr)NULL);
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
  a_boolean        vars_can_change, suppress_warning;

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
    vars_can_change = node_has_side_effects(op1_node, &suppress_warning) ||
                      node_has_side_effects(op2_node, &suppress_warning);
    /* Make "op1.i == 0" (or "!= 0" for the ne_case). */
    op1_node = make_reusable_copy(op1_node, vars_can_change);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_i_field);
    select1_node->next = node_for_integer_constant(0L, TARG_DELTA_INT_KIND);
    compare_i0_node = make_operator_node
                        ((an_expr_operator_kind) (ne_case ? eok_ine : eok_ieq),
                         int_type, select1_node);
    /* Make "op1.d == op2.d" (or "!=" for the ne_case). */
    op1_node = make_reusable_copy(op1_node, vars_can_change);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_d_field);
    op2_node = make_reusable_copy(op2_node, vars_can_change);
    select2_node = node_to_select_field_from_rvalue(op2_node, mptr_d_field);
    select1_node->next = select2_node;
    compare_d_node = make_operator_node
                       ((an_expr_operator_kind) (ne_case ? eok_ine : eok_ieq),
                        int_type, select1_node);
    /* Make "op1.f == op2.f" (or "!=" for the ne_case). */
    op1_node = make_reusable_copy(op1_node, vars_can_change);
    select1_node = node_to_select_field_from_rvalue(op1_node, mptr_f_field);
    op2_node = make_reusable_copy(op2_node, vars_can_change);
    select2_node = node_to_select_field_from_rvalue(op2_node, mptr_f_field);
    select1_node->next = select2_node;
    compare_f_node = make_operator_node
                       ((an_expr_operator_kind) (ne_case ? eok_pne : eok_peq),
                        int_type, select1_node);
    /* Make "(op1.d == op2.d && op1.f == op2.f)" (or "||" for the ne_case). */
    compare_d_node->next = compare_f_node;
    and_node = make_operator_node
                 ((an_expr_operator_kind) (ne_case ? eok_lor : eok_land),
                  int_type, compare_d_node);
    /* Make "(op1.i == 0 || (op1.d == op2.d && op1.f == op2.f))" (or "&&"
       for the ne_case. */
    compare_i0_node->next = and_node;
    or_node = make_operator_node
                ((an_expr_operator_kind) (ne_case ? eok_land : eok_lor),
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

#if LOWER_LVALUE_RETURNING_OPERATIONS

static void lower_operations_returning_lvalue_instead_of_usual_rvalue(
                                                    an_expr_node_ptr expr,
                                                    a_boolean        is_lvalue)
/*
Transform lvalue-returning assignments, prefix ++/-- operators, and "?" and
"," operators to valid C.  If the expression passed in is not one of those
it is left alone.  expr is being used as an lvalue if is_lvalue is TRUE.
*/
{
  if (is_operation_node(expr)) {
    an_expr_operator_kind op = expr->variant.operation.kind, child_op;
    an_expr_node_ptr      child1 = expr->variant.operation.operands;
    /* Look at the first operand of this operation to see if it is an
       lvalue-returning "?" or ",". */
    if (is_operation_node(child1) &&
        child1->variant.operation.returns_lvalue_instead_of_usual_rvalue &&
        ((child_op = child1->variant.operation.kind) ==
                                         (an_expr_operator_kind)eok_question ||
         child_op == (an_expr_operator_kind)eok_comma)) {
      /* The first operand of expr is an lvalue-returning "?" or ",".
         That is, expr is the node on top of a "?" or ",". */
      an_expr_node_ptr child2 = child1->next;
      /* "gchild" stands for "grandchild". */
      an_expr_node_ptr gchild1 = child1->variant.operation.operands;
      an_expr_node_ptr gchild2 = gchild1->next;
      an_expr_node_ptr gchild3, newop1, newop2;
      a_type_ptr       expr_type = expr->type;
      if (child_op == (an_expr_operator_kind)eok_question) {
        /* Lvalue "?" rewrite.  Change
             ((g1 ? g2 : g3) = c2)
                             S
           to
             (g1 ? (g2 = c2) : (g3 = c2))
                 D     D           D
           The operations marked "D" are lvalue operations iff the operation
           marked "S" is an lvalue operation.  The operation indicated as
           "=" can in fact be any operation (e.g., simple or complex
           assignment, prefix ++/--, field selection, cast).  c2 isn't present
           for unary operations. */
        gchild3 = gchild2->next;
        /* Build (g2 = c2). */
        newop1 = copy_node(expr);
        /* newop1->result_is_not_used is FALSE, which is right, regardless
           of whether the result of the "?" is used, because the "?" does
           not have void type (it's an lvalue, so it has a pointer type). */
        newop1->variant.operation.operands = gchild2;
        gchild2->next = child2;
        /* Build (g3 = c2) using a copy of c2. */
        newop2 = copy_node(expr);
        /* Likewise, newop2->result_is_not_used is properly FALSE. */
        newop2->variant.operation.operands = gchild3;
        gchild3->next = (child2 != NULL) ? copy_expr_tree(child2) : NULL;
        /* Replace the original top node with a "?" node. */
        gchild1->next = newop1;
        newop1->next = newop2;
        overwrite_node(expr, child1);
      } else {
        /* Lvalue "," rewrite.  Change
             ((g1 , g2) = c2)
                        S
           to
             (g1 , (g2 = c2))
                 D     D
           The operations marked "D" are lvalue operations iff the operation
           marked "S" is an lvalue operation.  The operation indicated as
           "=" can in fact be any operation (e.g., simple or complex
           assignment, prefix ++/--, field selection).  c2 isn't present
           for unary operations. */
        /* Build (g2 = c2). */
        newop1 = copy_node(expr);
        /* newop1->result_is_not_used is properly FALSE; see comment above. */
        newop1->variant.operation.operands = gchild2;
        gchild2->next = child2;
        newop2 = NULL;
        /* Replace the original top node with a "," node. */
        gchild1->next = newop1;
        overwrite_node(expr, child1);
      }  /* if */
      if (!is_lvalue) {
        /* The "S" above was an rvalue.  Change the result node to an
           rvalue. */
        expr->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
        expr->type = newop1->type;
      }  /* if */
      /* Do further rewriting on the operations just inserted. */
      lower_operations_returning_lvalue_instead_of_usual_rvalue(newop1,
                                                                is_lvalue);
      if (newop2 != NULL) {
        lower_operations_returning_lvalue_instead_of_usual_rvalue(newop2,
                                                                  is_lvalue);
      }  /* if */
      /* Restore the original expression type.  This matters when the
         operation above the "?" or "," is a cast. */
      expr->type = expr_type;
    } else if (expr->variant.operation.returns_lvalue_instead_of_usual_rvalue&&
               (op != (an_expr_operator_kind)eok_question &&
                op != (an_expr_operator_kind)eok_comma)) {
      an_expr_node_ptr child2 = child1->next;
      an_expr_node_ptr newop;
      a_boolean        suppress_warning, vars_can_change;
      /* expr is an lvalue-returning operation that is not a "?" or ",".
         Rewrite
           x = y          really: &x = y
         using an rvalue-returning operator as
           ((x = y), x)   really: ((&x = y), &x)
         The comma operator in the rewrite is lvalue-returning and will
         be rewritten in its turn later.  If necessary, make a reusable copy
         of x.  The same kind of rewrite is done for the prefix ++/-- case. */
      /* Make a copy of the assignment node that is an rvalue
         assignment.  The same process works for the prefix ++/-- case
         because the second operand is not touched. */
      newop = copy_node(expr);
      /* Drop type qualifiers on the result type. */
      newop->type = f_skip_typerefs(type_pointed_to(expr->type));
      newop->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
      /* newop->result_is_not_used is cleared by copy_type, as it should be. */
      /* For assignments, see if the source expression can have side
         effects on the variables used in the destination expression. */
      vars_can_change = FALSE;
      if (child2 != NULL) {
        vars_can_change = node_has_side_effects(child2, &suppress_warning);
      }  /* if */
      /* Attach a copy of the lvalue address to the assignment node, as the
         second operand of the comma operator. */
      newop->next = make_lvalue_reusable_copy(child1, vars_can_change);
      /* newop->next->result_is_not_used is properly FALSE, since the
         comma expression has non-void type (it's an lvalue, so it has
         a pointer type). */
      /* Change the original node to an lvalue-returning comma node. */
      set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                        expr->type, newop);
      expr->variant.operation.returns_lvalue_instead_of_usual_rvalue = TRUE;
    }  /* if */
  }  /* if */
}  /* lower_operations_returning_lvalue_instead_of_usual_rvalue */

#endif /* LOWER_LVALUE_RETURNING_OPERATIONS */

static void wrap_throw(an_expr_node_ptr node,
                       a_type_ptr       other_operand_type)
/*
node is a lowered throw expression under a "?" operator, and the other
operand of the operation has type other_operand_type, which is a non-void
type.  Wrap the throw in a comma expression to give it the same type as the
other operand.
*/
{
  a_boolean        nonscalar;
  a_type_ptr       zero_type;
  a_constant       null_constant;
  an_expr_node_ptr node_copy, zero_node;

  /* Make an operand that has the same type as the other operand.  If the
     type is scalar, use a zero cast to that type.  Otherwise (e.g., if
     it's a struct), indirect through a null pointer to the right kind. */
  zero_type = other_operand_type;
  nonscalar = !is_scalar_type(other_operand_type);
  if (nonscalar) zero_type = make_pointer_type(other_operand_type);
  make_zero_of_proper_type(zero_type, &null_constant);
  zero_node = alloc_node_for_constant(&null_constant);
  if (nonscalar) zero_node = add_indirection_to_node(zero_node);
  /* Make a copy of the original throw node so the original node can be
     overwritten by a comma node. */
  node_copy = copy_node(node);
  node_copy->next = zero_node;
  /* Change the original node to a comma expression. */
  change_node_to_operation(node, (an_expr_operator_kind)eok_comma,
                           other_operand_type, node_copy);
}  /* wrap_throw */


static void lower_enk_object_lifetime(an_expr_node_ptr expr,
                                      a_boolean        is_lvalue)
/*
Lower an enk_object_lifetime expression and its subtree.  This defines
an object lifetime for the evaluation of the subexpression.  The expression
is being used as an lvalue if is_lvalue is TRUE.  The expression is
a full expression (i.e., it's not part of some larger expression), because
an enk_object_lifetime should only occur at the top of a full expression.
*/
{
  a_context              context;
  an_insert_location     insert_location;
  an_expr_node_ptr       expr_to_lower = expr->variant.object_lifetime.expr;
  an_object_lifetime_ptr lifetime = expr->variant.object_lifetime.ptr;

  push_context(&context, (a_scope_ptr)NULL, lifetime);
  /* Lower the subexpression. */
  lower_expr(expr_to_lower, is_lvalue);
  if (any_cleanup_actions(lifetime)) {
    /* Generate any cleanup actions for temporaries built within
       the expression.  Note that this is a special "insert after"
       mode, which can only be used in very limited circumstances,
       e.g., at the top of an expression tree. */
    set_after_expr_insert_location(expr_to_lower, &insert_location);
    gen_cleanup_actions(lifetime, &insert_location);
    /* The insertions may have changed the type of the node, so copy the
       type up to the enk_object_lifetime node. */
    expr->type = expr_to_lower->type;
  }  /* if */
  pop_context();
  if (!keep_object_lifetime_info_in_lowered_il) {
    /* Not keeping object lifetime information, so eliminate this node. */
    unbind_object_lifetime(expr->variant.object_lifetime.ptr);
    overwrite_node(expr, expr_to_lower);
  }  /* if */
}  /* lower_enk_object_lifetime */


void lower_expr(an_expr_node_ptr expr,
                a_boolean        is_lvalue)
/*
Do IL lowering of the indicated expression and everything under it.
The expression is being used as an lvalue if is_lvalue is TRUE.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      operand_node, operand2, operand3, throw_operand;
  a_variable_ptr        var, temp_var;
  unsigned int          is_lvalue_mask;

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
          an_expr_node_ptr var_value = copy_node(expr);
          change_node_to_operation(expr, (an_expr_operator_kind)eok_indirect,
                                   var_value->type, var_value);
          /* Again, the type that was in the node is the one we want
             because the pointer-to and indirection cancel out.
             The type in the enk_variable node must be changed. */
          var_value->type = make_pointer_type(var_value->type);
        }  /* if */
      } else if (var_is_return_value_variable(var)) {
        /* The variable is the return value optimization variable for the
           current function, so rewrite it as a reference to the implicit
           parameter through which the return address is passed by the
           caller. */
        if (expr->kind == (an_expr_node_kind)enk_variable_address) {
          /* Rewrite address-of-local-variable as value-of-pointer-
             parameter. */
          set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
          expr->variant.variable = return_value_pointer_variable;
        } else {
          /* Rewrite value-of-local-variable as indirection through
             return-value-parameter.  This comes up when the class has
             a copy constructor but no assignment operator function. */
          operand_node = var_rvalue_expr(return_value_pointer_variable);
          change_node_to_operation(expr, (an_expr_operator_kind)eok_indirect,
                                   expr->type, operand_node);
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
        if (op == (an_expr_operator_kind)eok_question) {
          /* Question mark's second and third operands are lvalues if the
             question mark itself is. */
          if (is_lvalue) is_lvalue_mask = 0x6;
          /* Look for a "?" operator where one of the operands is a throw
             expression and the other has a non-void type.  The throw
             operation will be adjusted by putting a comma operation over
             it to give that operand the right type. */
          /* Note that we test now, before lowering, when it's easy to spot
             a throw node, but we do the rewrite after lowering. */
          throw_operand = NULL;
          /* Rule out cases where both operands are throws or one is a throw
             and the other one is void. */
          if (!is_void_type(expr->type)) {
            operand2 = operand_node->next;
            operand3 = operand2->next;
            if (operand2->kind == (an_expr_node_kind)enk_throw) {
              /* operand2 is a throw and operand3 is not. */
              throw_operand = operand2;
            } else if (operand3->kind == (an_expr_node_kind)enk_throw) {
              /* operand3 is a throw and operand2 is not. */
              throw_operand = operand3;
            }  /* if */
          }  /* if */
        } else if (op == (an_expr_operator_kind)eok_comma) {
          /* Comma's second operand is an lvalue if the comma itself is. */
          if (is_lvalue) is_lvalue_mask = 0x2;
        } else {
          /* Other operators.  See if the first operand is an lvalue. */
          if (operator_takes_lvalue_operand(op)) is_lvalue_mask = 0x1;
        }  /* if */
        /* Lower the operands of the expression. */
        lower_expr_list(operand_node, is_lvalue_mask);
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
              /* Preserve the result type because it tells us how to call
                 the kind of routine we've selected. */
              a_type_ptr type = expr->type;
              overwrite_node(expr, operand_node);
              expr->type = type;
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
          case eok_question:
            /* If one operand is a throw and the other is non-void, wrap
               the throw in a comma expression to give it the right type. */
            if (throw_operand != NULL) wrap_throw(throw_operand, expr->type);
            break;
#if ASSIGNMENT_TO_THIS_ALLOWED
          case eok_passign:
            /* Check for assignment to "this" in a constructor. */
            if (innermost_function_scope != NULL) {
              a_routine_ptr curr_routine =
                                 innermost_function_scope->variant.routine.ptr;
              if (curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
                a_variable_ptr this_param_var =
                          innermost_function_scope->variant.routine.parameters;

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
                       ((this = expr), this)
                     which then becomes
                       ((this = expr), (initialization, this))
                  */
                  an_expr_node_ptr   new_expr, orig_expr_copy;
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
                  /* Change "this = expr" to "((this = expr), this)" by
                     converting the original expression to a comma node.
                     Make a copy of the original node so the original node can
                     be overwritten. */
                  orig_expr_copy = copy_node(expr);
                  /* The copy is not an lvalue-returning operation. */
                  orig_expr_copy->variant.operation.
                                returns_lvalue_instead_of_usual_rvalue = FALSE;
                  orig_expr_copy->type = this_param_var->type;
                  orig_expr_copy->next = new_expr;
                  change_node_to_operation(expr,
                                           (an_expr_operator_kind)eok_comma,
                                           expr->type, orig_expr_copy);
                  set_expr_insert_location(new_expr, &insert_location);
                  /* The insert location now specifies insertion before the
                     final expression.  Add the wrapper code there. */
                  add_constructor_wrapper_code(innermost_function_scope,
                                               &insert_location);
                }  /* if */
              }  /* if */
            }  /* if */
            break;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
          default:
            /* No action on most operators. */
            break;
        }  /* switch */
#if LOWER_LVALUE_RETURNING_OPERATIONS
        /* Transform lvalue-returning assignments, prefix ++/--, and "?" and
           "," operators into valid C. */
        lower_operations_returning_lvalue_instead_of_usual_rvalue(expr,
                                                                  is_lvalue);
#endif /* LOWER_LVALUE_RETURNING_OPERATIONS */
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
      lower_temp_init(expr);
      break;
    case enk_new_delete:
      lower_new_delete(expr);
      break;
    case enk_throw:
#if DO_LOWERING_OF_EXCEPTION_HANDLING
      lower_throw(expr);
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
      break;
    case enk_object_lifetime:
      lower_enk_object_lifetime(expr, is_lvalue);
      break;
#if CHECKING
    default:
      internal_error("lower_expr: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* lower_expr */


static void lower_boolean_controlling_expr(an_expr_node_ptr expr)
/*
Lower a boolean controlling expression, e.g., the expression in an "if"
statement.  The expression is not an lvalue.
*/
{
  lower_normal_expr(expr);
  /* This expression is supposed to have something on top that guarantees
     a 0/1 value.  If the rewriting has disturbed that, add a "!= 0" test. */
  check_assertion(is_integral_type(expr->type));
  if (is_operation_node(expr) &&
      is_operator_returning_bool(expr->variant.operation.kind)) {
    /* The top of the expression is an operator that returns a boolean
       value, so it's okay. */
  } else if (is_constant_node(expr)) {
    /* A constant here ought to be okay already. */
  } else {
    /* A variable (e.g., a generated temporary), an operator that is
       not guaranteed to return a boolean value, or something else
       that is not guaranteed to return 0/1.  Add a "!= 0". */
    an_expr_node_ptr copy_expr = copy_node(expr);
    a_constant       zero_constant;
    an_expr_node_ptr zero_node;

    make_zero_of_proper_type(expr->type, &zero_constant);
    zero_node = alloc_node_for_constant(&zero_constant);
    copy_expr->next = zero_node;
    change_node_to_operation(expr, (an_expr_operator_kind)eok_ine,
                             copy_expr->type, copy_expr);
  }  /* if */
}  /* lower_boolean_controlling_expr */


static void add_conditional_flag(a_dynamic_init_ptr dip)
/*
Add a conditional flag to a dynamic initialization (pointed to by dip).
This is needed, for example, inside a conditional operand of a "?", "&&",
or "||" operation, to make the corresponding destruction dependent on
whether the construction was done.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_variable_ptr                  cond_var;

  cond_var = make_lowered_temporary(integer_type((an_integer_kind)ik_int));
  dedp->conditional_flag_var = cond_var;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  if (exceptions_enabled) {
    /* Pre-assign the object address table slot for the conditional variable,
       because we're going to have to set that entry of the object address
       table right away. */
    dedp->conditional_flag_handle = object_addr_table_index();
  }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
}  /* add_conditional_flag */


void initial_processing_on_destructible_initialization(
                                           a_dynamic_init_ptr dip,
                                           an_insert_location *insert_location)
/*
Do initial processing on a dynamic initialization entry that indicates
destruction.  That includes allocating the destructible entity description
entry and generating code to initialize any conditional flag.  Any generated
code is inserted at *insert_location, and *insert_location is updated.
*/
{
  /* Allocate a destructible entity description entry pointed to by
     the dynamic init entry. */
  check_assertion_str(dip->destructible_entity_descr == NULL,
  "initial_processing_on_destr...: destructible entity descr already present");
  dip->destructible_entity_descr = alloc_destructible_entity_descr();
  if (dip->inside_conditional_expression
#if DO_LOWERING_OF_EXCEPTION_HANDLING
      || (exceptions_enabled &&
          (dip->is_freeing_of_storage_on_exception
#if DO_UNORDERED_EH_PROCESSING
           || dip->unordered
#endif /* DO_UNORDERED_EH_PROCESSING */
                                                  ))
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
                                                    ) {
    /* This destruction requires a conditional flag that indicates that
       the construction was done; add one and initialize it to zero.
       This normally comes up for conditionally-executed parts of
       expressions, but it's also used for the cleanup for a new-allocation,
       which frees the storage if an exception is thrown before the storage
       is initialized. */
#if DO_UNORDERED_EH_PROCESSING
    /* A conditional flag is used for the unordered case if we can't
       predict the order in which certain initializations will be
       done (because the C language leaves evaluation order weakly
       defined; a real back end could figure out the actual evaluation
       order and would not need the flags for this case). */
#endif /* DO_UNORDERED_EH_PROCESSING */
    add_conditional_flag(dip);
    init_conditional_flag_var(dip->destructible_entity_descr->
                                                          conditional_flag_var,
#if DO_LOWERING_OF_EXCEPTION_HANDLING
                              dip->destructible_entity_descr->
                                                       conditional_flag_handle,
#else /* !DO_LOWERING_OF_EXCEPTION_HANDLING */
                              (a_handle_number)0,
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
                              insert_location);
  }  /* if */
}  /* initial_processing_on_destructible_initialization */


void begin_object_lifetime(an_object_lifetime_ptr lifetime,
                           an_insert_location     *insert_location)
/*
Do processing required at the beginning of the indicated object lifetime
(a block, block-after-label, or expression temporary lifetime).
If any code needs to be inserted, it is inserted at *insert_location,
and *insert_location is updated.
*/
{
  a_dynamic_init_ptr     dip;
  an_object_lifetime_ptr olp;

  for (dip = lifetime->destructions;
       dip != NULL;
       dip = dip->next_in_destruction_list) {
    initial_processing_on_destructible_initialization(dip, insert_location);
  }  /* for */
  /* Visit all children of this lifetime and process the expr-temporary
     lifetimes.  Other children will be processed when the associated
     block is entered. */
  for (olp = lifetime->child_lifetime; olp != NULL; olp = olp->next) {
    if (olp->kind == (an_object_lifetime_kind)olk_expr_temporary) {
      begin_object_lifetime(olp, insert_location);
    }  /* if */
  }  /* for */
}  /* begin_object_lifetime */


static an_object_lifetime_ptr label_successor_lifetime(
                                          an_object_lifetime_ptr lifetime,
                                          a_boolean              switch_clause)
/*
Given an olk_block or olk_block_after_label lifetime, return the successor
lifetime at the next label, or NULL if there isn't one.  If switch_clause is
TRUE, only consider successors at switch clauses.  If switch_clause is
FALSE, only consider successors that aren't at switch clauses.
*/
{
  for (;;) {
    if (!lifetime->has_block_after_label_child_lifetime) {
      /* This lifetime has no label successor, so we can save time and not
         look for one. */
      lifetime = NULL;
      break;
    } else {
      /* Find the label successor lifetime (there must be one, because the flag
         is set). */
      for (lifetime = lifetime->child_lifetime;
           lifetime->kind != (an_object_lifetime_kind)olk_block_after_label;
           lifetime = lifetime->next) {}
      /* See if this lifetime is for a switch clause or not, depending on
         what we want. */
      if ((lifetime->entity.kind == (a_byte_il_entry_kind)iek_switch_clause) ==
          (switch_clause != 0)) {
        /* This lifetime is one we want. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return lifetime;
}  /* label_successor_lifetime */


static void start_label_region_of_lifetime(
                                          an_object_lifetime_ptr lifetime,
                                          a_boolean              switch_clause)
/*
Start a new label region in the current context (either the original
olk_block lifetime or a successor olk_block_after_label lifetime).
lifetime indicates the lifetime to begin.  switch_clause is TRUE if the
lifetime begins at the start of a switch clause.
*/
{
  an_object_lifetime_ptr next_lifetime;

  curr_object_lifetime = curr_context->lifetime = lifetime;
  curr_context->latest_initialization = NULL;
  /* curr_cleanup_region_number is not changed on purpose. */
  if (!switch_clause) {
    /* Set up the context field to watch for the appearance of the
       statement that begins the next label lifetime.  The switch clause
       case is handled in the caller. */
    next_lifetime = label_successor_lifetime(lifetime,
                                             /*switch_clause=*/FALSE);
    curr_context->successor_lifetime_at_statement = next_lifetime;
  }  /* if */
}  /* start_label_region_of_lifetime */


void begin_block_object_lifetime(an_object_lifetime_ptr lifetime,
                                 an_insert_location_ptr insert_location)
/*
Do processing required at the beginning of an olk_block lifetime associated
with a block.  This includes generating code for things like conditional flag
initializations.  Such code is inserted at *insert_location.  Do nothing
if lifetime is NULL.
*/
{
  if (lifetime != NULL) {
    check_assertion(lifetime->kind == (an_object_lifetime_kind)olk_block);
    /* Visit all object lifetimes in this lifetime, and all destructions
       within those lifetimes. */
    begin_object_lifetime(lifetime, insert_location);
    start_label_region_of_lifetime(lifetime, /*switch_clause=*/FALSE);
  }  /* if */
}  /* begin_block_object_lifetime */


static void begin_block_label_object_lifetime(an_object_lifetime_ptr lifetime)
/*
Do processing required at the beginning of an olk_block_after_label lifetime
associated with a label in a block.
*/
{
  a_statement_ptr    stmt;
  an_insert_location insert_location;

  /* Get the statement pointer from the lifetime. */
  check_assertion(lifetime->entity.kind==(a_byte_il_entry_kind)iek_statement);
  stmt = (a_statement_ptr)(lifetime->entity.ptr);
  set_insert_location(stmt, &insert_location);
  /* Visit all object lifetimes in this lifetime, and all destructions
     within those lifetimes. */
  begin_object_lifetime(lifetime, &insert_location);
  start_label_region_of_lifetime(lifetime, /*switch_clause=*/FALSE);
}  /* begin_block_label_object_lifetime */


static void begin_switch_clause_object_lifetime(
                                               an_object_lifetime_ptr lifetime)
/*
Do processing required at the beginning of an olk_block_after_label lifetime
associated with a switch clause.
*/
{
  a_switch_clause_ptr switch_clause;
  an_insert_location  insert_location;

  /* Get the switch clause pointer from the lifetime. */
  check_assertion(lifetime->entity.kind ==
                                      (a_byte_il_entry_kind)iek_switch_clause);
  switch_clause = (a_switch_clause_ptr)(lifetime->entity.ptr);
  set_switch_clause_start_insert_location(switch_clause, &insert_location);
  /* Visit all object lifetimes in this lifetime, and all destructions
     within those lifetimes. */
  begin_object_lifetime(lifetime, &insert_location);
  start_label_region_of_lifetime(lifetime, /*switch_clause=*/TRUE);
}  /* begin_switch_clause_object_lifetime */

#if DO_LOWERING_OF_EXCEPTION_HANDLING

static void adjust_region_table_to_remove_long_lifetime_temps(
                                              a_boolean need_regions_for_temps)
/*
We've reached a label or switch clause in long-lifetime temporaries mode.
We haven't yet started any object lifetime that begins at the label or
switch clause.  Here, logically remove the temporaries from the cleanup
region table so they will no longer be part of the cleanup chain.  Called
only when exceptions are enabled.  If need_regions_for_temps is TRUE,
we will be destroying the temporaries, so we need cleanup regions that
will cover them while we destroy them.  Only called when exceptions
are enabled.
*/
{
  a_dynamic_init_ptr dip;
  a_dynamic_init_ptr first_temp = NULL, last_temp = NULL;
  a_dynamic_init_ptr first_nontemp = NULL, last_nontemp = NULL;

  /* If there are cleanup regions for non-temporaries that are followed
     by temporaries, the entries for the non-temporaries must be cloned
     so we can set the current region to the clones and have the
     destructions for the temporaries out of the list (to reflect the
     fact that the temporaries no longer need to be cleaned up on a
     throw).  If need_regions_for_temps is TRUE, we need to clone all
     but the first of the temporaries too, so we can have a cleanup
     region number while doing the temporary destructions.  The
     temporaries get moved to the front of the cloned list.  In the
     loop here, we split the region table entries into two lists
     (one for temporaries, one for nontemporaries), and then rejoin
     them with the temporaries first.  Note that the lists we're
     working with here are those linked on the next_in_region_table
     field, which indicates the next region table entry. */
  check_assertion_str2(curr_object_lifetime != NULL &&
                       curr_object_lifetime->destructions ==
                                           curr_context->latest_initialization,
                       "adjust_region_table_to_remove_long_lifetime_temps:",
                       "bad current object lifetime");
  for (dip = curr_context->latest_initialization;
       dip != NULL;
       dip = dip->destructible_entity_descr->next_in_region_table) {
    if (dip->has_temporary_lifetime) {
      /* An initialization for a temporary. */
      if (first_temp == NULL) first_temp = dip;
      if (last_temp != NULL) {
        last_temp->destructible_entity_descr->next_in_region_table = dip;
      }  /* if */
      last_temp = dip;
    } else {
      /* An initialization for a nontemporary. */
      if (first_nontemp == NULL) first_nontemp = dip;
      if (last_nontemp != NULL) {
        last_nontemp->destructible_entity_descr->next_in_region_table = dip;
      }  /* if */
      last_nontemp = dip;
    }  /* if */
  }  /* for */
  if (first_temp != NULL) {
    /* There were some temporaries in the lifetime. */
    /* Put the list back together again, with the temporaries first.  Note
       that this ruins the list for use by a "real" back end.  Even if we
       don't need to clone the temporaries, put them back on the list so that
       all entries can be found and detached at the end of lowering. */
    a_dynamic_init_ptr first_nontemp_after_temps =
                    last_temp->destructible_entity_descr->next_in_region_table;
    last_temp->destructible_entity_descr->next_in_region_table = first_nontemp;
    if (last_nontemp != NULL) {
      last_nontemp->destructible_entity_descr->next_in_region_table = NULL;
    }  /* if */
    if (first_nontemp_after_temps != first_nontemp) {
      /* Some region table entries must be cloned. */
      if (need_regions_for_temps) {
        /* We need to clone the entries for the temporaries too, except
           for the first temporary. */
        dip = first_temp->destructible_entity_descr->next_in_region_table;
      } else {
        /* We need to clone just the nontemporaries. */
        dip = first_nontemp;
      }  /* if */
      clone_region_table_entry_list(dip, first_nontemp_after_temps);
      if (need_regions_for_temps) {
        /* The entry for the first temporary need not be cloned, but its
           region_number_to_set_when_starting_destruction pointer needs to
           be updated so we set the right region number when we do the
           destruction. */
        first_temp->destructible_entity_descr->
                              region_number_to_set_when_starting_destruction =
                                                    cleanup_region_number(dip);
      }  /* if */
    }  /* if */
    /* The current position is at the beginning of the regions for the
       temporaries if we cloned regions for those because we will be
       destroying them, otherwise at the first region for a nontemp. */
    dip = need_regions_for_temps ? first_temp : first_nontemp;
    curr_context->latest_initialization = dip;
    curr_cleanup_region_number = cleanup_region_number(dip);
    /* set_eh_curr_region is not called on purpose. */
  }  /* if */
}  /* adjust_region_table_to_remove_long_lifetime_temps */

#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */

static void destroy_long_lifetime_temporaries_before_statement(
                                                    a_statement_ptr *statement)
/*
Destroy any long lifetime temporaries that are presently active.
The destruction code is inserted preceding the indicated statement.
If the statement is turned into a block, *statement will be updated
on return to point to the new location of the original statement.
Called only in long lifetime temporaries mode.
*/
{
  a_boolean          any_temps_destroyed = FALSE;
  a_boolean          need_to_destroy_temps;
  an_insert_location insert_location;
  a_dynamic_init_ptr dip;

  /* We need to destroy the temporaries only if the statement is reachable
     by flowing into it from the preceding code.  For statements other than
     labels, assume the statement is reachable because we don't know. */
  need_to_destroy_temps = FALSE;
  if ((*statement)->kind != (a_statement_kind)stmk_label ||
      (*statement)->variant.label.ptr->reachable_by_fall_through) {
    need_to_destroy_temps = TRUE;
  }  /* if */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  if (exceptions_enabled) {
    /* If necessary, adjust the cleanup region table to reflect the
       fact that the temporaries are no longer in the cleanup chain. */
    adjust_region_table_to_remove_long_lifetime_temps(need_to_destroy_temps);
  }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
  if (need_to_destroy_temps) {
    /* Go through the list of destructions, find the ones for temporaries,
       and generate destruction code. */
    for (dip = curr_context->latest_initialization;
         dip != NULL;
         dip = dip->next_in_destruction_list) {
      if (dip->has_temporary_lifetime &&
          !dip->is_freeing_of_storage_on_exception) {
        /* Found a destruction for a temporary.  */
        /* If this is the first one, make an insert location by rewriting
           the label as a block. */
        if (!any_temps_destroyed) {
          any_temps_destroyed = TRUE;
          turn_statement_into_block(*statement, &insert_location, statement);
        }  /* if */
        /* Generate the cleanup action. */
        gen_one_destruction(dip, &insert_location);
      }  /* if */
    }  /* for */
  }  /* if */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  if (exceptions_enabled && any_temps_destroyed) {
    /* Set the current region number, but not if the current statement
       is a label (because in that case it will be set in a moment
       anyway). */
    if ((*statement)->kind != (a_statement_kind)stmk_label) {
      set_eh_curr_region(curr_cleanup_region_number, &insert_location);
    }  /* if */
  }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
}  /* destroy_long_lifetime_temporaries_before_statement */


/*
Return TRUE if the indicated statement is a do-nothing statement
created by IL lowering (presumably, it was some other kind of statement
and it was replaced by something else; see turn_statement_into_noop).
*/
#define is_noop_statement(statement)                                  \
  ((statement)->kind == (a_statement_kind)stmk_block &&               \
   (statement)->variant.block.statements == NULL &&                   \
   seq_number_from_stmt_source_position((statement)->                 \
                 variant.block.extra_info->final_position) == 0)


void lower_statement_list(a_statement_ptr statement_list,
                          a_statement_ptr *p_last_statement)
/*
Do IL lowering of the indicated list of statements and everything under it.
Return a pointer to the last statement in *p_last_statement, or NULL if
there are no statements on the list.
*/
{
  a_statement_ptr        statement, statement_next, last_statement = NULL;
  a_statement_ptr        eff_statement;
  an_object_lifetime_ptr next_lifetime;
  a_boolean              stmt_begins_label_lifetime;

  for (statement = statement_list;
       statement != NULL;
       statement = statement_next) {
    /* Save the "next" pointer now, so that any statements inserted by lowering
       will not be lowered (in particular, lowering of stmk_init statements
       inserts statements, and the expressions therein should not be lowered
       again). */
    statement_next = statement->next;
    eff_statement = statement;
    set_position_from_stmt_source_position(code_pos_for_lowering,
                                           statement->position);
    error_position = code_pos_for_lowering;
    /* See if a new object lifetime begins at this statement because
       it is or contains a label. */
    stmt_begins_label_lifetime = FALSE;
    next_lifetime = curr_context->successor_lifetime_at_statement;
    if (next_lifetime != NULL &&
        (a_statement_ptr)next_lifetime->entity.ptr == statement) {
      /* A new object lifetime begins at this statement. */
      stmt_begins_label_lifetime = TRUE;
      if (long_lifetime_temps) {
        /* Destroy any long lifetime temporaries.  If the statement is turned
           into a block, eff_statement will be updated to point to the original
           statement. */
        destroy_long_lifetime_temporaries_before_statement(&eff_statement);
      }  /* if */
    }  /* if */
    /* Lower a statement. */
    lower_statement(eff_statement);
    if (stmt_begins_label_lifetime) {
      /* Start a new object lifetime because of a label. */
      begin_block_label_object_lifetime(next_lifetime);
    }  /* if */
    /* Remove extra no-op statements (empty blocks) left in the statement
       sequence by lowering of some statements (e.g., stmk_init).
       Note that this is done in a way that doesn't change the address
       of the first statement on the list.  In particular, that means
       a block containing just a no-op statement cannot be changed. */
    if (is_noop_statement(statement) && statement->next != statement_next) {
      /* This is a no-op statement followed by a statement generated
         by IL lowering, so we can copy the generated statement on top
         of the no-op statement.  Note that in general we cannot move a
         statement to a different address, since back ends may depend on
         the address to establish identity, but in this case we can because
         the generated statement is presumably part of the expansion of
         the statement that's now a no-op. */
      copy_statement(statement->next, statement);
    }  /* if */
    /* Keep track of the last statement (so far) in the statement list. */
    last_statement = statement;
    /* If there were statements inserted after the current statement, find
       the last of those statements.  Those inserted statements have already
       been lowered. */
    while (last_statement->next != statement_next) {
      last_statement = last_statement->next;
      check_assertion_str(last_statement != NULL,
                          "lower_statement_list: did not find statement_next");
    }  /* while */
  }  /* for */
  *p_last_statement = last_statement;
}  /* lower_statement_list */


static void lower_switch_clause_list(a_switch_clause_ptr    clause_list,
                                     an_object_lifetime_ptr switch_lifetime)
/*
Do IL lowering of the indicated switch clause list and everything under it.
If the switch statement has an associated lifetime, switch_lifetime points to
it; otherwise, switch_lifetime is NULL.
*/
{
  a_switch_clause_ptr    clause;
  a_statement_ptr        clause_statements, last_statement;
  an_insert_location     insert_location;
  an_object_lifetime_ptr lifetime;

  /* Find the first switch clause lifetime. */
  if (switch_lifetime != NULL) {
    lifetime = label_successor_lifetime(switch_lifetime,
                                        /*switch_clause=*/TRUE);
  } else {
    lifetime = NULL;
  }  /* if */
  /* Loop through the switch clauses. */
  for (clause = clause_list; clause != NULL; clause = clause->next) {
    /* If the switch clause contains a statement, get a source position from
       that and use it as the position for any code created. */
    if (clause->statements != NULL) {
      set_position_from_stmt_source_position(code_pos_for_lowering,
                                             clause->statements->position);
      error_position = code_pos_for_lowering;
    }  /* if */
    lower_constant_list(clause->constant_list);
    /* Get the statement list before any insertions done for the start
       of an object lifetime. */
    clause_statements = clause->statements;
    /* See if this clause is associated with the next object lifetime
       in sequence. */
    if (lifetime != NULL &&
        (a_switch_clause_ptr)lifetime->entity.ptr == clause) {
      /* A different object lifetime begins at the beginning of this
         clause. */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
      if (exceptions_enabled && long_lifetime_temps) {
        /* If necessary, adjust the cleanup region table to reflect the
           fact that the temporaries are no longer in the cleanup chain. */
        adjust_region_table_to_remove_long_lifetime_temps(
                                             /*need_regions_for_temps=*/FALSE);
      }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
      begin_switch_clause_object_lifetime(lifetime);
      lifetime = label_successor_lifetime(lifetime, /*switch_clause=*/TRUE);
    }  /* if */
    lower_statement_list(clause_statements, &last_statement);
    if (switch_lifetime != NULL) {
      /* Generate any cleanup actions required at the end of the
         clause.  Note that the implicit "break" is only used at the
         top level within a switch; "break" statements from deeper
         (e.g., inside nested blocks) will be rendered as gotos. */
      if (clause->implied_break_at_end) {
        /* There is an implicit "break" at the end of the clause. */
        if (any_cleanup_actions(switch_lifetime)) {
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
          gen_cleanup_actions(switch_lifetime, &insert_location);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* lower_switch_clause_list */


void turn_statement_into_block(a_statement_ptr        statement,
                               an_insert_location_ptr insert_location,
                               a_statement_ptr        *orig_statement)
/*
Turn a statement into a block containing a copy of the statement, and
set *insert_location so that statements can be inserted at the beginning
of the block (i.e., in front of the original statement).  *orig_statement
is set to point to the original statement in its new location.
*/
{
  a_statement_ptr stmt_copy;

  /* Make a copy of the original statement. */
  *orig_statement = stmt_copy = alloc_statement(statement->kind);
  copy_statement(statement, stmt_copy);
  stmt_copy->next = NULL;
  /* Turn the statement into a block statement. */
  set_statement_kind(statement, (a_statement_kind)stmk_block);
  statement->variant.block.statements = stmt_copy;
  clear_stmt_source_position(statement->position);
  /* Insert at the start of the added block. */
  set_block_start_insert_location(statement, insert_location);
}  /* turn_statement_into_block */


void turn_branch_into_block(a_statement_ptr        statement,
                            an_insert_location_ptr insert_location,
                            a_statement_ptr        *orig_statement)
/*
Turn a branch statement (goto or return) into a block, and set *insert_location
so that statements can be inserted at the beginning of the block (i.e.,
in front of the original branch statement).  *orig_statement is set to point
to the original statement in its new location.
*/
{
  turn_statement_into_block(statement, insert_location, orig_statement);
  /* We know the original statement is a branch of some sort, so
     the end of the block is not reachable. */
  statement->variant.block.extra_info->end_of_block_reachable = FALSE;
}  /* turn_branch_into_block */


a_context_ptr context_for_lifetime(an_object_lifetime_ptr lifetime)
/*
Find and return a pointer to the context associated with the given object
lifetime.
*/
{
  a_context_ptr context;

  for (context = curr_context;
       !context->new_lifetime || context->lifetime != lifetime;
       context = context->parent) {}
  return context;
}  /* context_for_lifetime */


static a_boolean gen_cleanup_actions_or_check_if_needed(
                                        an_object_lifetime_ptr outer_lifetime,
                                        an_insert_location_ptr insert_location,
                                        a_boolean              check_only)
/*
Generate any cleanup actions required to exit from the current context
(as indicated by curr_object_lifetime and curr_context) through
outer_lifetime, inclusive.  Insert the code for the cleanup at
*insert_location.  If any cleanup code was needed, return TRUE.
If check_only is TRUE, just return that value; do not generate any
code.
*/
{
  a_boolean              any_cleanup_needed = FALSE, skip_temporaries = FALSE;
  a_dynamic_init_ptr     dip = curr_context->latest_initialization;
  an_object_lifetime_ptr lifetime = curr_object_lifetime;

  /* Do nothing at all if there are no lifetimes involved. */
  if (outer_lifetime != NULL) {
    /* Loop outward through the indicated scopes.  At each level, there may
       be destructions from the current position back to the beginning
       of the lifetime, and there may be cleanup actions associated with the
       lifetime itself. */
    for (;;) {
      /* Generate destructions in this context. */
      for (; dip != NULL; dip = dip->next_in_destruction_list) {
        if (dip->has_temporary_lifetime && skip_temporaries) {
          /* Skipping temporaries, so skip this destruction. */
        } else if (dip->is_constructor_init ||
                   dip->is_freeing_of_storage_on_exception) {
          /* Also skip entries for constructor inits (in constructors and
             destructors).  They apply for exception cleanup but not on
             exit via branch.  Ditto for freeing storage for a new-allocation
             if an exception is thrown before the initialization is
             completed. */
        } else if (dip->variable == NULL &&
                   dip->destructible_entity_descr->init_pos_descr.variable ==
                                               return_value_pointer_variable) {
          /* This is the initialization of the parameter substituted for the
             return value optimization variable.  The destruction doesn't get
             done on exit from the routine (the caller does it). */
        } else {
          any_cleanup_needed = TRUE;
          if (check_only) goto done;
          gen_one_destruction(dip, insert_location);
        }  /* if */
      }  /* for */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
      { a_scope_ptr            scope;
        /* In some cases, the context itself requires cleanup. */
        if (lifetime->kind == (an_object_lifetime_kind)olk_try_block) {
          /* Exit from a "try" block. */
          any_cleanup_needed = TRUE;
          if (check_only) goto done;
          cleanup_on_exit_from_try_block(context_for_lifetime(lifetime),
                                         insert_location);
        } else if ((an_il_entry_kind)lifetime->entity.kind == iek_scope &&
                   (scope = (a_scope_ptr)lifetime->entity.ptr,
                    (scope->kind == (a_scope_kind)sck_block &&
                     scope->variant.assoc_handler != NULL))) {
          /* Exit from a "catch" clause. */
          any_cleanup_needed = TRUE;
          if (check_only) goto done;
          cleanup_on_exit_from_catch(insert_location);
        }  /* if */
      }
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
      /* Stop when the outer lifetime has been processed. */
      if (lifetime == outer_lifetime) break;
      /* Continuing into the parent. */
      /* The lifetime indicates where to start in the destructions list
         of the parent. */
      dip = lifetime->parent_destruction_sublist;
      skip_temporaries = FALSE;
      if (lifetime->kind == (an_object_lifetime_kind)olk_block_after_label) {
        /* We're going from an olk_block_after_label to a previous lifetime
           in the same scope, so skip temporaries in the previous lifetime,
           because temporaries will have been destroyed at the label. */
        skip_temporaries = TRUE;
      }  /* if */
      lifetime = lifetime->parent_lifetime;
    }  /* for */
  }  /* if */
done:
  return any_cleanup_needed;
}  /* gen_cleanup_actions_or_check_if_needed */


void gen_cleanup_actions(an_object_lifetime_ptr outer_lifetime,
                         an_insert_location_ptr insert_location)
/*
Generate any cleanup actions required to exit from the current context
(as indicated by curr_object_lifetime and curr_context) through
outer_lifetime, inclusive.  Insert the code for the cleanup at
*insert_location.
*/
{
  (void)gen_cleanup_actions_or_check_if_needed(outer_lifetime, insert_location,
                                               /*check_only=*/FALSE);
}  /* gen_cleanup_actions */


static a_boolean any_cleanup_actions(an_object_lifetime_ptr outer_lifetime)
/*
Return TRUE if any cleanup actions are required to exit from the current
context (as indicated by curr_object_lifetime and curr_context) through
outer_lifetime, inclusive.
*/
{
  a_boolean any_cleanup_needed = gen_cleanup_actions_or_check_if_needed(
                                                    outer_lifetime,
                                                    (an_insert_location *)NULL,
                                                    /*check_only=*/TRUE);
  return any_cleanup_needed;
}  /* any_cleanup_actions */


static void gen_goto_cleanup_actions(a_statement_ptr statement)
/*
Generate any cleanup actions required preceding the indicated goto statement.
*/
{
  an_object_lifetime_ptr common_lifetime = statement->variant.label.lifetime;

  /* If not keeping lifetimes in the IL, clear the lifetime pointer. */
  if (!keep_object_lifetime_info_in_lowered_il) {
    statement->variant.label.lifetime = NULL;
  }  /* if */
  /* The goto statement has a pointer to an object lifetime that is the
     innermost object lifetime that it has in common with the label.
     Generate cleanup actions for all lifetimes from the current position
     out to the indicated lifetime. */
  /* Do nothing if there are no lifetimes involved. */
  if (common_lifetime != NULL) {
    an_object_lifetime_ptr outer_lifetime = curr_object_lifetime;
#if CHECKING
    if (curr_object_lifetime == NULL) {
#if DEBUG
      fprintf(f_debug, "common_lifetime:\n");
      db_object_lifetime(common_lifetime);
#endif /* DEBUG */
      unexpected_condition_str2("gen_goto_cleanup_actions:",
                       "curr_object_lifetime is NULL, common_lifetime is not");
    }  /* if */
#endif /* CHECKING */
    if (outer_lifetime != common_lifetime) {
      /* Some lifetimes are being exited. */
      /* Find the last lifetime in the cleanup chain rising from the goto that
         should be terminated, i.e., the one right before common_lifetime. */
      while (outer_lifetime->parent_lifetime != common_lifetime) {
        outer_lifetime = outer_lifetime->parent_lifetime;
#if CHECKING
        if (outer_lifetime == NULL) {
#if DEBUG
          fprintf(f_debug, "common_lifetime:\n");
          db_object_lifetime(common_lifetime);
          db_object_lifetime_stack();
#endif /* DEBUG */
          unexpected_condition_str2(
                                "gen_goto_cleanup_actions: common lifetime",
                                "not found in curr lifetime parents");
        }  /* if */
#endif /* CHECKING */
      }  /* while */
      /* See if any cleanup actions are required on leaving the indicated
         lifetimes. */
      if (any_cleanup_actions(outer_lifetime)) {
        /* Some cleanup actions are needed.  Generate them. */
        an_insert_location insert_location;
        a_statement_ptr    orig_statement;
        /* Turn the goto into a block so code can be inserted in front
           of it. */
        turn_branch_into_block(statement, &insert_location, &orig_statement);
        gen_cleanup_actions(outer_lifetime, &insert_location);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* gen_goto_cleanup_actions */


static void push_block_statement_context(a_statement_ptr block_statement,
                                         a_context       *context,
                                         a_boolean       *context_pushed,
                                         a_boolean       *new_lifetime)
/*
Push a context and start an object lifetime, if necessary, for the
indicated block statement.  If a context is pushed, context (a local
variable in the caller) is used as the stack entry and *context_pushed
is returned TRUE.  *new_lifetime is returned TRUE if a new object lifetime
is begun.
*/
{
  a_block_ptr            block = block_statement->variant.block.extra_info;
  a_scope_ptr            scope = block->assoc_scope;
  an_object_lifetime_ptr lifetime = block->lifetime;

  *context_pushed = FALSE;
  *new_lifetime = FALSE;
  if (scope != NULL || lifetime != NULL) {
    push_context(context, scope, lifetime);
    *context_pushed = TRUE;
    *new_lifetime = curr_context->new_lifetime;
    if (scope != NULL) lifetime = scope->lifetime;
  } else if (block_statement == innermost_function_scope->assoc_block) {
    /* For the topmost block in a function, assoc_scope is NULL, so
       no push_context is done.  That's correct, because the caller has
       done the push_context already.  A new lifetime may begin here,
       however. */
    scope = innermost_function_scope;
    lifetime = scope->lifetime;
    *new_lifetime = (lifetime != NULL);
  }  /* if */
  if (*new_lifetime) {
    /* A new lifetime was pushed. */
    /* Do initial processing for the lifetime (e.g., generate conditional
       flag variables). */
    an_insert_location insert_location;
    set_block_start_insert_location(block_statement, &insert_location);
    begin_block_object_lifetime(lifetime, &insert_location);
  }  /* if */
}  /* push_block_statement_context */


static void pop_block_statement_context(a_statement_ptr block_statement,
                                        a_statement_ptr last_statement,
                                        a_boolean       context_pushed,
                                        a_boolean       new_lifetime)
/*
Pop a context and end an object lifetime, if necessary, for the
indicated block statement.  context_pushed indicates whether or
not a context was pushed and therefore should be popped.  new_lifetime
indicates whether or not an object lifetime was begun and therefore
should be ended.  If an object lifetime is ended, generate any cleanup
actions required at the end of the block.  last_statement points to the
last statement within the block, or is NULL if there are no statements
in the block or to ask this routine to find the last statement itself.
Any cleanup code inserted is placed after the last statement.
*/
{
  a_block_ptr            block = block_statement->variant.block.extra_info;
  a_scope_ptr            scope = block->assoc_scope;
  an_object_lifetime_ptr lifetime = block->lifetime;
  an_insert_location     insert_location;

  if (new_lifetime) {
    /* An object lifetime must be ended.  If there were labels in the
       block, this may end several object lifetimes.  (That's one reason
       why we can't just use curr_context->lifetime here.)   Note also
       that for the topmost block in a function, we end the lifetime
       here but do not pop the context. */
    if (block_statement == innermost_function_scope->assoc_block) {
      scope = innermost_function_scope;
    }  /* if */
    if (scope != NULL) lifetime = scope->lifetime;
    /* Insert any cleanup actions after the last statement in the block
       if the end of the block is reachable. */
    if (block->end_of_block_reachable) {
      /* If the block was originally empty but some statements were
         added (e.g., to initialize the catch handler parameter), find the
         last statement. */
      if (last_statement == NULL &&
          block_statement->variant.block.statements != NULL) {
        for (last_statement = block_statement->variant.block.statements;
             last_statement->next != NULL;
             last_statement = last_statement->next) {}
      }  /* if */
      if (last_statement == NULL) {
        /* The block is empty, so insert at its beginning. */
        set_block_start_insert_location(block_statement, &insert_location);
      } else {
        /* Insert after the last statement. */
        set_insert_location(last_statement, &insert_location);
      }  /* if */
      gen_cleanup_actions(lifetime, &insert_location);
    }  /* if */
  }  /* if */
  if (context_pushed) {
    /* Pop the context pushed by push_block_statement_context. */
    pop_context();
  }  /* if */
}  /* pop_block_statement_context */


void lower_statement(a_statement_ptr statement)
/*
Do IL lowering of the indicated statement and everything under it.
*/
{
  a_context            context;
  a_scope_ptr          scope;
  an_insert_location   insert_location;
  a_statement_ptr      statement_list;
  a_statement_ptr      last_statement, body_statement, return_statement;
  a_boolean            make_block, any_cleanup_on_return;
  an_expr_node_ptr     return_expr;
  a_variable_ptr       temp_var;
  a_dynamic_init_ptr   dip;
  a_source_position    saved_error_position, saved_code_pos;
  a_block_ptr          block;
  a_boolean            context_pushed, new_lifetime;

  if (statement != NULL) {
    /* Track the source position. */
    saved_code_pos = code_pos_for_lowering;
    set_position_from_stmt_source_position(code_pos_for_lowering,
                                           statement->position);
    saved_error_position = error_position;
    error_position = code_pos_for_lowering;
    switch (statement->kind) {
      case stmk_expr:
        lower_normal_expr(statement->expr);
        break;
      case stmk_asm:
        /* No processing required. */
        break;
      case stmk_goto:
        /* Generate any cleanup actions required on exit from any blocks
           that the goto is inside of but the label is not. */
        gen_goto_cleanup_actions(statement);
        break;
      case stmk_label:
#if DO_LOWERING_OF_EXCEPTION_HANDLING
        if (exceptions_enabled) {
          /* Exceptions are enabled.  Set __eh_curr_region. */
          set_insert_location(statement, &insert_location);
          set_eh_curr_region(curr_cleanup_region_number, &insert_location);
        }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
        break;
      case stmk_return:
        return_expr = statement->expr;
        if (return_expr != NULL) {
          lower_normal_expr(return_expr);
        }  /* if */
        /* Keep track of whether or not we have already turned the return
           statement into a block.  We haven't so far. */
        return_statement = statement;
        make_block = TRUE;
        dip = statement->variant.return_dynamic_init;
        /* If the routine returns its value via a copy constructor, generate
           code for the dynamic initialization.  However, if return value
           optimization applies, just skip the copy constructor call
           altogether. */
        if (dip != NULL &&
            innermost_function_scope->variant.routine.
                                               return_value_variable == NULL) {
          /* This routine returns its value via a copy constructor.
             The dynamic initialization entry indicates the operation to
             be done. */
          an_init_pos_descr ipd;
          a_boolean         keep_dynamic_init;
          statement->variant.return_dynamic_init = NULL;
          set_var_indirect_init_pos_descr(return_value_pointer_variable, &ipd);
          /* Put the return statement under a block so we can insert in
             front of it. */
          turn_branch_into_block(statement, &insert_location,
                                 &return_statement);
          make_block = FALSE;
          lower_dynamic_init(dip, &ipd,
                             (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                             (a_constructor_init_ptr)NULL,
                             &insert_location, &keep_dynamic_init);
          check_assertion(!keep_dynamic_init);
        }  /* if */
        any_cleanup_on_return =
                       any_cleanup_actions(innermost_function_scope->lifetime);
        if (any_cleanup_on_return
#if DO_LOWERING_OF_EXCEPTION_HANDLING
            || (exceptions_enabled &&
                innermost_function_scope->lifetime != NULL)
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
                                                           ) {
          /* Some code will have to be inserted on return, either for
             cleanup or to pop the exception handling stack entry.  It has
             to be inserted after the evaluation of the return expression,
             if any, and before the return, so change
               return expr;
             into
               {temp = expr; return temp;}
                            ^--- to allow insertion of code here.
             Don't rewrite constant expressions or the return expression
             in a constructor (it's "return this;").
          */
          if (return_expr != NULL && !is_constant_node(return_expr) &&
              innermost_function_scope->variant.routine.ptr->special_kind !=
                                    (a_special_function_kind)sfk_constructor) {
            /* There is a nonconstant return expression, so use a temporary.
               Note that the return type cannot call for a copy constructor,
               or the routine would be returning its value via an added
               parameter. */
            temp_var = make_lowered_temporary(return_expr->type);
            /* Change the return statement to return the temporary's value. */
            statement->expr = var_rvalue_expr(temp_var);
            /* Put the return statement under a block so we can insert in
               front of it. */
            /* Note that if we executed the similar code above we wouldn't
               be executing the code here, because a return can have either
               a dynamic init entry or an expression, but not both. */
            check_assertion(make_block);
            turn_branch_into_block(statement, &insert_location,
                                   &return_statement);
            make_block = FALSE;
            /* Insert the "temp = return-expr;" statement. */
            (void)insert_var_assignment_statement(
                                   temp_var,
                                   lowered_assignment_operator(temp_var->type),
                                   return_expr, &insert_location);
          }  /* if */
        }  /* if */
        if (any_cleanup_on_return) {
          /* Generate any cleanup actions required on exit from the
             routine.  */
          if (make_block) {
            /* Turn the return into a block so that code can be inserted
               in front of the return. */
            turn_branch_into_block(statement, &insert_location,
                                   &return_statement);
          }  /* if */
          gen_cleanup_actions(innermost_function_scope->lifetime,
                              &insert_location);
        }  /* if */
        /* Maintain a list of all returns in the routine so that epilogue code
           can be added for destructors and for exception handling. */
        add_to_return_memo_list(return_statement);
        break;
      case stmk_if:
        lower_boolean_controlling_expr(statement->expr);
        lower_statement(statement->variant.if_stmt.then_statement);
        lower_statement(statement->variant.if_stmt.else_statement);
        break;
      case stmk_while:
        lower_boolean_controlling_expr(statement->expr);
        lower_statement(statement->variant.loop_statement);
        break;
      case stmk_end_test_while:
        lower_boolean_controlling_expr(statement->expr);
        lower_statement(statement->variant.loop_statement);
        break;
      case stmk_for:
        { a_for_loop_ptr  extra_info = statement->variant.for_loop.extra_info;
          a_statement_ptr init_stmt = extra_info->initialization;
          if (init_stmt != NULL) {
            a_statement_ptr init_stmt_next;
            lower_statement(init_stmt);
            /* If the initialization was rewritten as a sequence of statements,
               make it into a block, because the stmk_for can only point at a
               single statement. */
            init_stmt_next = init_stmt->next;
            if (init_stmt_next != NULL) {
              init_stmt->next = NULL;
              turn_statement_into_block(init_stmt, &insert_location,
                                        &init_stmt);
              init_stmt->next = init_stmt_next;
            }  /* if */
          }  /* if */
          if (statement->expr != NULL) {
            lower_boolean_controlling_expr(statement->expr);
          }  /* if */
          lower_statement(statement->variant.for_loop.statement);
          if (extra_info->increment != NULL) {
            lower_normal_expr(extra_info->increment);
          }  /* if */
        }
        break;
      case stmk_block:
        /* Save the statement list pointer early in case code is inserted
           to initialize conditional flags or the catch handler parameter. */
        statement_list = statement->variant.block.statements;
        /* Push a context around the processing of the block if it has a scope
           or an object lifetime. */
        push_block_statement_context(statement, &context,
                                     &context_pushed, &new_lifetime);
        block = statement->variant.block.extra_info;
        scope = block->assoc_scope;
        if (scope != NULL) {
#if DO_LOWERING_OF_EXCEPTION_HANDLING
          if (scope->variant.assoc_handler != NULL) {
            /* This statement is the dependent statement of a catch handler.
               Generate code to start the catch clause. */
            begin_catch_clause(scope->variant.assoc_handler);
          }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
        }  /* if */
        lower_statement_list(statement_list, &last_statement);
        /* Generate any cleanup actions and pop the context. */
        pop_block_statement_context(statement, last_statement,
                                    context_pushed, new_lifetime);
        break;
      case stmk_switch:
        lower_normal_expr(statement->expr);
        /* If there is a body statement that is a block, push a context
           around the processing of the switch clauses. */
        body_statement = statement->variant.switch_stmt.body_statement;
        if (body_statement != NULL &&
            body_statement->kind == (a_statement_kind)stmk_block) {
          /* The body statement is a block. */
          /* Save the statement list pointer early in case code is inserted
             to initialize conditional flags. */
          statement_list = body_statement->variant.block.statements;
          push_block_statement_context(body_statement, &context,
                                       &context_pushed, &new_lifetime);
          lower_statement_list(statement_list, &last_statement);
          lower_switch_clause_list(statement->variant.switch_stmt.clause_list,
                                   new_lifetime? curr_context->lifetime :
                                                 (an_object_lifetime_ptr)NULL);
          pop_block_statement_context(body_statement, last_statement,
                                      context_pushed, new_lifetime);
        } else {
          /* There is no body statement, or the body statement is something
             other than a block statement. */
          lower_statement(body_statement);
          lower_switch_clause_list(statement->variant.switch_stmt.clause_list,
                                   (an_object_lifetime_ptr)NULL);
        }  /* if */
        break;
      case stmk_init:
        lower_stmk_init(statement);
        break;
      case stmk_try_block:
#if DO_LOWERING_OF_EXCEPTION_HANDLING
        lower_try_block(statement);
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
        break;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
        /* Statement that marks the location of declarations.  Ignored here. */
        break;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if CHECKING
      default:
        internal_error("lower_statement: bad kind");
#endif /* CHECKING */
    }  /* switch */
    error_position = saved_error_position;
    code_pos_for_lowering = saved_code_pos;
  }  /* if */
}  /* lower_statement */


static void promote_constants(a_scope_ptr scope)
/*
Promote the constants on the constants list of the indicated scope
(a class scope) into the file scope.
*/
{
  a_constant_ptr constant, next_constant;

  /* Why is promotion into the file scope?  Because there are no constants
     on the constants lists of function and block scopes otherwise, so
     it seems unwise to put any there in this case.  Besides, the constants
     we are promoting here are rare -- they're member constants of classes,
     which are an extension (enum constants don't appear on the constant
     list).  Note that since we are promoting constants out of a class,
     all the constants will already be allocated in the file scope memory
     region (fortunately). */
  /* Promote the constants to the end of the file-scope constants list. */
  for (constant = scope->constants;
       constant != NULL;
       constant = next_constant) {
    next_constant = constant->next;
#if DEBUG
    if (debug_level >= 4) {
      (void)fprintf(f_debug, "Promoting constant out of class ");
      db_name(&scope->variant.assoc_type->source_corresp);
      (void)fprintf(f_debug, ": ");
      db_name(&constant->source_corresp);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    add_to_constants_list(constant, /*at_file_scope=*/TRUE);
  }  /* for */
  /* Clear the list of promoted constants.  Since the scope is for a class,
     we know it cannot be on the scope stack now, and therefore we do
     not need to update a corresponding last pointer. */
  scope->constants = NULL;
}  /* promote_constants */


static void promote_variables(a_scope_ptr scope)
/*
Promote the static variables on the variables list of the indicated scope
(a class scope) into the file scope.
*/
{
  a_variable_ptr variable, next_variable;

  /* Why is promotion into the file scope?  Well, we are promoting static
     data members here, and local classes cannot have static data members.
     That means the only cases that come up involve promoting static data
     members out of file-scope classes or classes nested within them.
     For those, the file scope is the right place to promote to. */
  /* Promote the variables to the end of the proper variables list. */
  for (variable = scope->variables;
       variable != NULL;
       variable = next_variable) {
    next_variable = variable->next;
#if DEBUG
    if (debug_level >= 4) {
      (void)fprintf(f_debug, "Promoting variable out of class ");
      db_name(&scope->variant.assoc_type->source_corresp);
      (void)fprintf(f_debug, ": ");
      db_variable(variable);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    add_to_variables_list(variable, /*at_file_scope=*/TRUE);
  }  /* for */
  /* Clear the list of promoted variables.  Since the scope is for a class,
     we know it cannot be on the scope stack now, and therefore we do
     not need to update a corresponding last pointer. */
  scope->variables = NULL;
}  /* promote_variables */


static void promote_routines(a_scope_ptr scope)
/*
Promote the routines on the routines list of the indicated scope (a class
scope) into the file scope.
*/
{
  a_routine_ptr routine, next_routine;

  /* Why is promotion into the file scope?  Because routines are only
     allowed in the file scope and in class scopes. */
  for (routine = scope->routines; routine != NULL; routine = next_routine) {
    next_routine = routine->next;
#if DEBUG
    if (debug_level >= 4) {
      (void)fprintf(f_debug, "Promoting routine out of class ");
      db_name(&scope->variant.assoc_type->source_corresp);
      (void)fprintf(f_debug, ": ");
      db_name(&routine->source_corresp);
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    add_to_routines_list(routine, /*at_file_scope=*/TRUE);
  }  /* for */
  /* Clear the list of promoted routines.  Since the scope is for a class,
     we know it cannot be on the scope stack now, and therefore we do
     not need to update a corresponding last pointer. */
  scope->routines = NULL;
}  /* promote_routines */


static void promote_type_list(a_type_ptr  type,
                              a_scope_ptr promotion_scope,
                              a_type_ptr  *insert_pointer)
/*
Promote the types on the indicated types list into the scope promotion_scope.
*insert_pointer indicates the insertion position, and is updated after
the insertion.
*/
{
  a_type_ptr next_type;

  for (; type != NULL; type = next_type) {
    next_type = type->next;
    if (type->kind == (a_type_kind)tk_typeref &&
        type->variant.typeref.is_placeholder_for_file_scope_type) {
      /* This type is a placeholder typeref that indicates the point at
         which a file-scope type appeared in the class.  For example:
           template<class T> struct TMPL {};
           struct A {
             typedef int I;
             TMPL<I> a;
           };
         The type for TMPL<I> was placed immediately into the file scope,
         preceding A.  When I is promoted out of A, it must end up in
         front of TMPL<I>.  That's accomplished by placing a placeholder
         type on the types list of A to indicate the spot where TMPL<I>
         appeared.  TMPL<I> is removed from the file-scope list when
         encountered there in the promotion process, and then put back
         at the right spot when the placeholder appears. */
      type = type->variant.typeref.type;
#if DEBUG
      if (debug_level >= 4) {
        (void)fprintf(f_debug, "Promoting placeholder type ");
        db_type_name(type);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* If the type-as-subobject for the type followed it on the file scope
         list, it was also removed from the list, and left attached to
         the primary type by the "next" pointer. */
      if (type->next != NULL) {
        /* The type-as-subobject is present, so arrange to have it be the
           type processed the next time around the loop.  That will get
           it placed back on the file-scope types list and get its
           members promoted out. */
        a_type_ptr type_as_subobject = type->next;
        type_as_subobject->next = next_type;
        next_type = type_as_subobject;
      }  /* if */
      /* Go on to promote the class' members and put the file-scope type
         on the file-scope list. */
    }  /* if */
    /* If the type is a class, promote its members. */
    if (is_immediate_class_type(type)) {
      promote_class_members(type, promotion_scope, insert_pointer);
    }  /* if */
#if DEBUG
    if (debug_level >= 4) {
      (void)fprintf(f_debug, "Promoting type out of class: ");
      db_type_name(type);
      (void)fprintf(f_debug, "; promotion_scope = ");
      db_scope(promotion_scope);
      (void)fprintf(f_debug, "; *insert_pointer = ");
      if (*insert_pointer == NULL) {
        (void)fprintf(f_debug, "<null>");
      } else {
        db_type_name(*insert_pointer);
      }  /* if */
      (void)fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (*insert_pointer == NULL) {
      type->next = promotion_scope->types;
      promotion_scope->types = type;
    } else {
      type->next = (*insert_pointer)->next;
      (*insert_pointer)->next = type;
    }  /* if */
    *insert_pointer = type;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, promote them now too.
         They go out after the class itself.  There will only be
         types on this list for non-nested classes (because the promoted
         types go to the outermost enclosing class). */
      promote_type_list(ctsp->promoted_local_types, promotion_scope,
                        insert_pointer);
      ctsp->promoted_local_types = NULL;
    }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  }  /* for */
}  /* promote_type_list */


static void promote_types(a_scope_ptr scope,
                          a_scope_ptr promotion_scope,
                          a_type_ptr  *insert_pointer)
/*
Promote the types on the types list of the indicated scope (a class scope)
into the scope promotion_scope.  *insert_pointer indicates the insertion
position, and is updated after the insertion.
*/
{
  check_assertion(scope->kind == (a_scope_kind)sck_class_struct_union);
  /* Promote the types to the end of the proper types list.  Also promote
     members out of any classes encountered on the types list. */
  promote_type_list(scope->types, promotion_scope, insert_pointer);
  /* Clear the list of promoted types.  Since the scope is for a class,
     we know it cannot be on the scope stack now, and therefore we do
     not need to update a corresponding last pointer. */
  scope->types = NULL;
}  /* promote_types */


static void promote_class_members(a_type_ptr  class_type,
                                  a_scope_ptr promotion_scope,
                                  a_type_ptr  *insert_pointer)
/*
Promote the members of the class class_type out of the class.  Most
members are promoted into the file scope.  Types are promoted into
promotion_scope, at the position indicated by *insert_pointer, and
*insert_pointer is updated.
*/
{
  a_class_type_supplement_ptr ctsp;
  a_scope_ptr                 scope;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp != NULL) {
    scope = ctsp->assoc_scope;
    /* If the class has constants, static data members, member functions,
       or local types, promote them out of the class scope. */
    if (scope != NULL) {
#if DEBUG
      if (debug_level >= 4) {
        (void)fprintf(f_debug, "Promoting the members out of ");
        db_scope(scope);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* Constants, static data members, and member functions are promoted
         to the file scope. */
      promote_constants(scope);
      promote_variables(scope);
      promote_routines(scope);
      /* Types are promoted to promotion_scope. */
      promote_types(scope, promotion_scope, insert_pointer);
      if (scope->pragmas != NULL) {
        /* There are pragmas in the class, so promote them to the file
           scope too. */
        a_pragma_ptr class_pragmas = scope->pragmas;
        a_pragma_ptr pp, last_fs_pragma;
        /* Find the end of the class pragma list. */
        for (pp = class_pragmas; pp->next != NULL; pp = pp->next) {}
        /* Put the class pragma list on the end of the file-scope pragma
           list. */
        last_fs_pragma = scope_stack[DEPTH_OF_FILE_SCOPE].last_pragma;
        if (last_fs_pragma == NULL) {
          il_header.primary_scope->pragmas = class_pragmas;
        } else {
          last_fs_pragma->next = class_pragmas;
        }  /* if */
        scope_stack[DEPTH_OF_FILE_SCOPE].last_pragma = pp;
        scope->pragmas = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* promote_class_members */


static void do_scope_class_member_promotion(a_scope_ptr scope)
/*
Do promotion of members of classes out of those classes in the indicated
scope and all subscopes.
*/
{
  a_type_ptr    type, next_type, insert_pointer;
  a_scope_ptr   block_scope;
  a_scope_depth depth;

#if DEBUG
  if (debug_level >= 4) {
    (void)fprintf(f_debug, "do_scope_class_member_promotion on ");
    db_scope(scope);
    (void)fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Visit all types to find all classes. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these types are truly local types
     and are not used in the file scope, so it's okay to promote their
     members now.  In fact, it's necessary -- we have to update the scope
     types and variables pointers before they are recorded in the orphan
     lists.  Note that in that case the class has not been lowered yet
     when the members are promoted out of it, and that's as intended.
     The members must be promoted out, but the lowering cannot be done yet
     (because if you did the lowering, you wouldn't know where to stop
     lowering, because you wouldn't know where the local types shade over
     into normal file-scope types). */
  /* See if there are any types to process. */
  type = scope->types;
  if (type != NULL) {
    insert_pointer = NULL;
    for (; type != NULL; type = next_type) {
      next_type = type->next;
      /* If the type is a class, promote its members out of the class. */
      if (is_immediate_class_type(type)) {
        a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
        if (type->variant.class_struct_union.
                                           referenced_by_placeholder_typeref) {
          /* This type is on the file scope types list but it was created while
             scanning a class definition.  There is a placeholder typeref
             within the class to indicate the point at which the class should
             go, and that's where promotion of class members should happen,
             so do nothing now except taking the type out of the list. */
#if DEBUG
          if (debug_level >= 4) {
            (void)fprintf(f_debug, "Placeholder for class ");
            db_type_name(type);
            (void)fprintf(f_debug, " ignored for the moment\n");
          }  /* if */
#endif /* DEBUG */
          /* If the next type on the list is the type-as-subobject version
             of this type, remove it as well, keeping it linked to the
             primary type. */
          if (next_type != NULL && ctsp->type_as_subobject == next_type) {
            /* Yes, the next type is the corresponding type-as-subobject, so
               remove it along with the primary type. */
            a_type_ptr type_as_subobject = next_type;
            next_type = next_type->next;
            type_as_subobject->next = NULL;
          } else {
            /* The type-as-subobject is not there, so remove just the
               primary type. */
            type->next = NULL;
          }  /* if */
          /* Link around the removed type(s). */
          if (insert_pointer == NULL) {
            scope->types = next_type;
          } else {
            insert_pointer->next = next_type;
          }  /* if */
          /* Do not update insert_pointer. */
        } else {
          /* Normal class case. */
          promote_class_members(type, scope, &insert_pointer);
          insert_pointer = type;
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
          /* If some local types of member functions were promoted into the
             class on their way to the file scope, promote them now too.
             They go out after the class itself.  There will only be
             types on this list for non-nested classes (because the promoted
             types go to the outermost enclosing class). */
          promote_type_list(ctsp->promoted_local_types, scope,
                            &insert_pointer);
          ctsp->promoted_local_types = NULL;
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
        }  /* if */
      } else {
        /* Not a class type.  Set the insert location after it. */
        insert_pointer = type;
      }  /* if */
    }  /* for */
    /* If this scope is in the scope_stack, update its last_type pointer. */
    depth = scope->depth_in_scope_stack;
    if (depth != NO_SCOPE_DEPTH) {
      scope_stack[depth].last_type = insert_pointer;
    }  /* if */
  }  /* if */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_class_member_promotion(block_scope);
  }  /* for */
}  /* do_scope_class_member_promotion */

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
    /* Look for a local static variable with a destructor.  That might force
       the variable to be at the file scope, because if the destruction is
       complicated, a routine is generated to contain the destruction
       code. */
    for (var = scope->variables; var != NULL; var = var->next) {
      an_init_kind       init_kind;
      an_initializer_ptr initializer;
      get_variable_initializer(var, scope, &init_kind, &initializer);
      if (init_kind == (an_init_kind)initk_dynamic) {
        dip = initializer->dynamic;
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
    /* See if the class has defined member functions (just the existence
       of member functions is not enough, since all classes have an
       assignment operator function).  The member function definition
       will have its own memory region, so anything from this function
       that it references must be in the file scope. */
    for (rout = scope->routines; rout != NULL; rout = rout->next) {
      if (rout->assoc_scope != NULL_region_number) {
        promotion_needed = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (!promotion_needed) {
    /* If nothing in this scope forces promotion, look at any subscopes.
       If something there requires promotion, the local entities of this
       scope will also have to be promoted (because the promoted entities
       might refer to the types etc. of the surrounding scopes). */
    /* Visit all types to find all class types and their scopes. */
    for (type = scope->types; type != NULL; type = type->next) {
      if (is_immediate_class_type(type)) {
        class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
        if (class_scope != NULL) {
          if (local_entities_should_be_promoted(class_scope)) {
            promotion_needed = TRUE;
            break;
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
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return promotion_needed;
}  /* local_entities_should_be_promoted */


static void clear_is_local_to_function_flag_in_type(a_type_ptr type)
/*
The indicated type is (part of something) being promoted to file scope.
Clear its is_local_to_function flag and the flags of any subtypes.
*/
{
  type->source_corresp.is_local_to_function = FALSE;
  /* If the type is a class, process its type list. */
  if (is_immediate_class_type(type)) {
    a_scope_ptr scope =
                      type->variant.class_struct_union.extra_info->assoc_scope;
    if (scope != NULL) {
      a_type_ptr subtype;
      for (subtype = scope->types; subtype != NULL; subtype = subtype->next) {
        clear_is_local_to_function_flag_in_type(subtype);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* clear_is_local_to_function_flag_in_type */


static void promote_local_entities_to_file_scope(a_scope_ptr   scope,
                                                 a_routine_ptr routine)
/*
Promote the local types and static variables of the indicated
scope and its subscopes to the file scope.  The scope is a function or
block scope and is (directly or indirectly) part of the indicated routine.
Note that the entities being promoted have not been lowered yet; they will
get lowered (as normal list members, not as orphans) as part of the
lowering of the file scope memory region.  When promoting out of a member
function, the local types are placed on a list associated with the outermost
enclosing class, for later promotion out of the class (and into the file
scope) along with the class members.
*/
{
  a_type_ptr     type, next_type;
  a_variable_ptr variable, next_variable;
  a_scope_ptr    block_scope;
  a_scope_depth  depth;

#if DEBUG
  if (debug_level >= 4) {
    (void)fprintf(f_debug, "Promoting local entities out of ");
    db_scope(scope);
    (void)fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Note that any pragmas associated with promoted entities are already on
     the file scope list, so they do not need to be moved. */
  depth = scope->depth_in_scope_stack;
  /* See if there are types to promote. */
  type = scope->types;
  if (type != NULL) {
    /* Promote local types to file scope.  When promoting out of a member
       function, promote the types to the end of the promoted_local_types
       list of the class. */
    a_type_ptr last_class_type;
    a_type_ptr routine_class = routine->source_corresp.class_of_which_a_member;
    if (routine_class != NULL) {
      /* Promoting out of a member function.  Get the promoted_local_types
         list. */
      /* If the class is a nested class, work out to the outermost
         enclosing class.  This is important for ordering reasons, because
         we want all these promoted local types to have access to all of
         the types in all of the surrounding classes. */
      while (routine_class->source_corresp.class_of_which_a_member != NULL) {
        routine_class = routine_class->source_corresp.class_of_which_a_member;
      }  /* while */
      last_class_type = routine_class->variant.class_struct_union.extra_info->
                                                          promoted_local_types;
      if (last_class_type != NULL) {
        /* Find the end of the list. */
        while (last_class_type->next != NULL) {
          last_class_type = last_class_type->next;
        }  /* while */
      }  /* if */
    }  /* if */
    for (; type != NULL; type = next_type) {
      next_type = type->next;
#if DEBUG
      if (debug_level >= 4) {
        (void)fprintf(f_debug, "Promoting local type out of routine ");
        db_name(&routine->source_corresp);
        (void)fprintf(f_debug, ": ");
        db_type_name(type);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* Mangle the name if necessary (e.g., if it is part of a template
         function). */
      mangle_promoted_entity_name(&type->source_corresp, routine, scope);
      /* Clear the is_local_function flag in the type and any subtypes. */
      clear_is_local_to_function_flag_in_type(type);
      if (routine_class == NULL) {
        /* Not promoting from a member function: just add to the file-scope
           types list. */
        add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
      } else {
        /* Promoting from a member function: add to the list associated with
           the class. */
        if (last_class_type == NULL) {
          routine_class->variant.class_struct_union.extra_info->
                                                   promoted_local_types = type;
        } else {
          last_class_type->next = type;
        }  /* if */
        last_class_type = type;
        type->next = NULL;
      }  /* if */
      /* If the type is an enum, mangle the names of its constants. */
      if (is_immediate_enum_type(type)) {
        a_constant_ptr enum_con;
#if DEBUG
        if (debug_level >= 4) {
          (void)fprintf(f_debug, "Enum constants promoted too\n");
        }  /* if */
#endif /* DEBUG */
        for (enum_con = type->variant.integer.enum_info.constant_list;
             enum_con != NULL;
             enum_con = enum_con->next) {
          mangle_promoted_entity_name(&enum_con->source_corresp, routine,
                                      scope);
        }  /* for */
      }  /* if */
    }  /* for */
    /* Clear the types list now that all types have been promoted. */
    scope->types = NULL;
    if (depth != NO_SCOPE_DEPTH) scope_stack[depth].last_type = NULL;
  }  /* if */
  /* See if there are local static variables to promote. */
  variable = scope->variables;
  if (variable != NULL) {
    /* Promote local static variables to file scope. */
    for (; variable != NULL; variable = next_variable) {
      next_variable = variable->next;
#if DEBUG
      if (debug_level >= 4) {
        (void)fprintf(f_debug, "Promoting local variable out of routine ");
        db_name(&routine->source_corresp);
        (void)fprintf(f_debug, ": ");
        db_variable(variable);
        (void)fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
      /* Mangle the name if necessary (e.g., if it is part of a template
         function). */
      mangle_promoted_entity_name(&variable->source_corresp, routine, scope);
      variable->source_corresp.is_local_to_function = FALSE;
      add_to_variables_list(variable, /*at_file_scope=*/TRUE);
      /* If the variable has an associated local-static-variable-init
         entry, transfer any initialization to the variable itself. */
      if (variable->init_kind == (an_init_kind)initk_function_local) {
        a_local_static_variable_init_ptr lsvip =
                              find_local_static_variable_init(variable, scope);
        variable->init_kind = lsvip->init_kind;
        switch (lsvip->init_kind) {
          case initk_none:
          case initk_zero:
            break;
          case initk_static:
            /* For a static initial value, copy the constant to file scope.
               This might be expensive space-wise, since this might be
               an aggregate, but there are no good alternatives. */
            { a_memory_region_number region_to_switch_back_to =
                                                            NULL_region_number;
              switch_to_file_scope_region(&region_to_switch_back_to);
              /* Make sure the copy is created with flags indicating it
                 has not been lowered yet. */
              initial_value_for_il_lowering_flag =
                                           !initial_value_for_il_lowering_flag;
              variable->initializer.constant =
                           copy_unshared_constant(lsvip->initializer.constant);
              initial_value_for_il_lowering_flag =
                                           !initial_value_for_il_lowering_flag;
              switch_back_to_original_region(region_to_switch_back_to);
            }
            break;
          case initk_dynamic:
            /* This dynamic initialization will be rewritten when the
               stmk_init is processed, so leave it alone for now.  The code
               there will copy the remaining constant if necessary. */
            variable->initializer.dynamic = lsvip->initializer.dynamic;
            break;
          default:
            unexpected_condition_str(
             "promote_local_entities_to_file_scope: bad static var init_kind");
        }  /* switch */
      }  /* if */
    }  /* for */
    /* Clear the variables list now that all variables have been promoted. */
    scope->variables = NULL;
    scope->local_static_variable_inits = NULL;
    if (depth != NO_SCOPE_DEPTH) scope_stack[depth].last_variable = NULL;
  }  /* if */
  /* Visit all block scopes and promote the local entities therein. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    promote_local_entities_to_file_scope(block_scope, routine);
  }  /* for */
}  /* promote_local_entities_to_file_scope */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */

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
                make_lowered_param_variable(make_pointer_type(subobject_type));
        vbase_param_var->next = prev_param_var->next;
        prev_param_var->next = vbase_param_var;
        prev_param_var = vbase_param_var;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* add_constructor_params */


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
            make_lowered_param_variable(integer_type((an_integer_kind)ik_int));
  complete_obj_param_var->next = this_param_var->next;
  this_param_var->next = complete_obj_param_var;
}  /* add_destructor_params */


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
  a_scope_kind     scope_kind = scope->kind;

  db_enter(2, "lower_scope");
  /* Add a context entry for the scope, but not for the file scope (the caller
     has done that already). */
  if (scope_kind != (a_scope_kind)sck_file) {
    push_context(&context, scope, (an_object_lifetime_ptr)NULL);
  }  /* if */
  if (scope_kind == (a_scope_kind)sck_function) {
    /* The scope is for a function. */
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
    return_value_pointer_variable = NULL;
    /* Rewrite the parameters if necessary. */
    if (rtsp->value_returned_by_cctor) {
      /* If there is an implicit parameter for the return value address,
         add it as an explicit first parameter.  Note that the variable is
         then lowered as part of the parameters below. */
      /* The variable is saved in a global variable for use in processing
         return statements. */
      return_type = routine_type->variant.routine.return_type;
      return_value_pointer_variable =
                   make_lowered_param_variable(make_pointer_type(return_type));
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
#if ASSIGNMENT_TO_THIS_ALLOWED || NEW_CAN_BE_FOLDED_INTO_CTOR
      /* If an assignment to "this" will be done in this routine, because it
         contains a user-written assignment to "this" or because it's a
         constructor that will do the "new" allocation internally, drop the
         top-level "const" on the "this" parameter variable type. */
      { a_boolean drop_const = FALSE;
#if NEW_CAN_BE_FOLDED_INTO_CTOR
        if (routine->special_kind ==
            (a_special_function_kind)sfk_constructor) drop_const = TRUE;
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if ASSIGNMENT_TO_THIS_ALLOWED
        if (routine->assignment_to_this_done) drop_const = TRUE;
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
        if (drop_const) {
          /* Drop the top-level "const" on the "this" parameter. */
          param_var->type = rtsp->implicit_this_param_type =
                                              f_skip_typerefs(param_var->type);
        }  /* if */
      }
#endif /* ASSIGNMENT_TO_THIS_ALLOWED || NEW_CAN_BE_FOLDED_INTO_CTOR */
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
  }  /* if */
  lower_constant_list(scope->constants);
  if (lowering_file_scope) {
    /* Lower the file-scope lists or the lists for a class scope. */
    lower_type_list(scope->types);
    lower_variable_list(scope->variables);
    if (scope_kind == (a_scope_kind)sck_class_struct_union &&
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
            /* Don't do this for static data members with incomplete types. */
            if (var->storage_class == (a_storage_class)sc_extern &&
                !is_incomplete_type(var->type)) {
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
    /* Note that an entry for this scope will be placed on the
       il_header.scope_orphaned_list_headers list if either of those
       pointers is non-NULL.  The entry is created after IL lowering
       runs so that IL lowering can alter those local lists. */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
    /* If there is reason to promote the local types and static variables
       to the file scope, do that now and clear the lists.  That makes the
       promoted entities part of the file scope and no longer orphans.
       For member functions, the local types get lowered, put onto a list
       associated with the class, and promoted when the class members get
       promoted out later. */
    if (scope_kind == (a_scope_kind)sck_function) {
      if (local_entities_should_be_promoted(scope)) {
        promote_local_entities_to_file_scope(scope,
                                             scope->variant.routine.ptr);
      }  /* if */
    }  /* if */
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
  }  /* if */
  lower_variable_list(scope->nonstatic_variables);
  lower_local_static_variable_init_list(scope->local_static_variable_inits);
  lower_label_list(scope->labels);
  lower_routine_list(scope->routines);
  lower_asm_entry_list(scope->asm_entries);
  if (scope_kind == (a_scope_kind)sck_function) {
    /* A function scope. */
    /* Lower any block scopes within it.  Note that statements are not
       lowered during this processing; they are handled in the lowering
       of assoc_block below. */
    lower_scope_list(scope->scopes);
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
      }  /* if */
    }  /* if */
    /* Clear the list of return statements found in the routine.  This list
       is built so that epilogue code can be added at each return. */
    return_memo_list = NULL;
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    if (exceptions_enabled) {
      /* Initialize for exception handling lowering. */
      eh_function_lower_init();
    }  /* if */
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
    /* Lower the executable code. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor) {
      /* For a constructor, add wrapper code around the user code, and also
         lower the user code. */
      lower_constructor_code(scope);
    } else if (routine->special_kind ==
                                     (a_special_function_kind)sfk_destructor) {
      /* For a destructor, add wrapper code around the user code, and also
         lower the user code. */
      lower_destructor_code(scope);
    } else {
      /* Normal case.  Lower the statements.  Note that this call (at the
         function level) lowers all the statements in the function, even those
         inside block scopes. */
      lower_statement(scope->assoc_block);
    }  /* if */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    /* Add prologue code for exceptions. */
    if (exceptions_enabled) add_eh_function_prologue(scope);
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
    /* If the routine is the main program, insert a call of _main at its
       start.  This is done after inserting the exception handling function
       prologue, if any, so that the call to _main is always first. */
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
    /* Free any return memos that were not used. */
    free_return_memo_list(return_memo_list);
    return_memo_list = NULL;
    return_value_pointer_variable = NULL;
  }  /* if */
  if (scope_kind != (a_scope_kind)sck_file) pop_context();
  db_exit();
}  /* lower_scope */


static void lower_orphaned_entries(void)
/*
Lower any "orphaned" entries in the file scope, entries that are pointed
to only from a function scope, and are therefore in the file scope but
not reachable from the normal file-scope IL tree.
*/
{
  a_scope_orphaned_list_header_ptr solhp;
  an_il_entry_kind                 kind;
  char                             *entry_ptr;

  /* First lower the list of orphaned types and variables from function
     and block scopes. */
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    lower_type_list(solhp->orphaned_types);
    lower_variable_list(solhp->orphaned_variables);
    /* The source sequence sublist list need not be visited. */
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
  /* Save/restore curr_object_lifetime in this routine. */
  an_object_lifetime_ptr
              saved_curr_object_lifetime = curr_object_lifetime;
  a_scope_ptr saved_innermost_function_scope = innermost_function_scope;

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
    curr_context = file_scope_context = NULL;
    innermost_function_scope = NULL;
    curr_object_lifetime = il_header.primary_scope->lifetime;
    switch_il_region(region_number);
    /* Mark entries created during this traversal as having already been
       visited by IL lowering. */
    initial_value_for_il_lowering_flag = !initial_value_for_il_lowering_flag;
    if (region_number == FILE_SCOPE_REGION_NUMBER) {
      /* The file scope. */
      lowering_file_scope = TRUE;
      scope = il_header.primary_scope;
      /* Do name mangling.  This must be done early when original type
         information is available (for example, references are still
         references and not yet pointers).  Note that this is done only
         in the file scope memory region. */
      do_all_name_mangling();
    } else {
      /* A function scope. */
      lowering_file_scope = FALSE;
      innermost_function_scope = scope =
                                   il_header.region_scope_entry[region_number];
    }  /* if */
    /* Put the file-scope context on the context stack.  This is also done
       for function scope memory regions so there will be a file-scope
       context above the function context. */
    push_context(&context, il_header.primary_scope,
                 (an_object_lifetime_ptr)NULL);
    file_scope_context = curr_context;
    /* Create definitions for virtual function tables.  This must be done
       early when virtual function information is still available. */
    define_scope_virtual_function_tables(scope);
    /* Lower the scope and its subscopes in the same memory region. */
    lower_scope(scope);
    if (lowering_file_scope) {
      /* Lower any orphaned types and other entries from the function and
         block scopes.  They are allocated in the file scope memory region but
         are not linked into the file scope memory region IL tree, so they have
         to be found through a separate list. */
      lower_orphaned_entries();
    }  /* if */
    /* Promote class members out of the classes. */
    do_scope_class_member_promotion(scope);
    if (lowering_file_scope) {
      /* Generate code to handle file-scope dynamic initializations and
         the corresponding destructions.  This is done after scope class
         member promotions so that the initialization routine is last. */
      lower_file_scope_dynamic_inits();
      make_code_to_invoke_file_scope_init_routine();
    }  /* if */
#if DO_LOWERING_OF_EXCEPTION_HANDLING
    /* Add definitions for any typeinfo variables generated for classes.
       This must be done late so that all the required typeinfo variables
       will have been created already. */
    define_scope_class_typeinfo_vars(scope);
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
    /* Pop the file-scope context. */
    pop_context();
    initial_value_for_il_lowering_flag = !initial_value_for_il_lowering_flag;
  }  /* if */
  curr_object_lifetime = saved_curr_object_lifetime;
  innermost_function_scope = saved_innermost_function_scope;
  db_exit();
}  /* lower_il_memory_region */


static void visit_object_lifetime_tree(an_object_lifetime_ptr olp,
                                       a_boolean              detach)
/*
Visit the indicated object lifetime and all its children to do end-of-lowering
cleanup.  If detach is TRUE, detach all the object lifetimes from the IL tree
so they're not reachable.  Do nothing if olp is NULL.
*/
{
  an_object_lifetime_ptr child_olp;
  a_dynamic_init_ptr     dip, dip_next;

  if (olp != NULL) {
    /* Visit all children. */
    for (child_olp = olp->child_lifetime;
         child_olp != NULL;
         child_olp = child_olp->next) {
      visit_object_lifetime_tree(child_olp, detach);
    }  /* if */
    /* Visit the dynamic init entries on the destructions list of
       the lifetime. */
    for (dip = olp->destructions; dip != NULL; dip = dip_next) {
      dip_next = dip->next_in_destruction_list;
      check_assertion_str2(dip->lifetime == olp,
                           "visit_object_lifetime_tree:",
                           "bad lifetime pointer in dynamic init");
      /* Disassociate the dynamic init entry from the lifetime. */
      if (detach) remove_from_destruction_list(dip);
      /* Free any attached entity description.  The pointer would
         normally be expected to be non-NULL, but if a subtree is
         detached and then visited again later, the pointer will be
         NULL the second time. */
      if (dip->destructible_entity_descr != NULL) {
        free_destructible_entity_descr(dip->destructible_entity_descr);
        dip->destructible_entity_descr = NULL;
      }  /* if */
    }  /* while */
    if (detach) {
      /* Unbind this object lifetime from its attached entity. */
      /* Label lifetimes don't have two-way binding with an entity. */
      if (olp->kind != (an_object_lifetime_kind)olk_block_after_label) {
        /* Watch out for lifetimes that have already been unbound (e.g., those
           associated with enk_object_lifetime nodes). */
        if (olp->entity.kind != (a_byte_il_entry_kind)iek_none) {
          unbind_object_lifetime(olp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* visit_object_lifetime_tree */


void eliminate_object_lifetime_tree(an_object_lifetime_ptr olp)
/*
Eliminate the indicated object lifetime and all its children.  "Eliminate"
means to detach them from the IL tree so they're not reachable.  Do nothing
if olp is NULL.
*/
{
  visit_object_lifetime_tree(olp, /*detach=*/TRUE);
}  /* eliminate_object_lifetime_tree */


void clean_up_all_object_lifetimes(a_scope_ptr scope)
/*
Visit all object lifetime entries attached to the indicated scope and
its subtree to do end-of-lowering cleanup.  If we're not supposed to
pass object lifetime information to the back end, detach all the object
lifetime information from the IL tree.  This is done late so that the
object lifetimes are available during the entire lowering process.
The scope is the top scope in a memory region.
*/
{
  a_boolean detach = !keep_object_lifetime_info_in_lowered_il;

  visit_object_lifetime_tree(scope->lifetime, detach);
  if (scope->kind == (a_scope_kind)sck_function) {
    visit_object_lifetime_tree(
                         scope->variant.routine.lifetime_of_local_static_vars,
                         detach);
    if (detach) {
      /* Clear the object lifetime pointers in all stmk_label statements.
         They are cleared late (rather than when the label is encountered)
         because they are needed each time the label is referenced. */
      a_label_ptr lab;
      for (lab = scope->labels; lab != NULL; lab = lab->next) {
        a_statement_ptr lab_stmt = lab->variant.exec_stmt;
        lab_stmt->variant.label.lifetime = NULL;
      }  /* for */
    }  /* detach */
  } else {
    /* File scope. */
#if ORPHAN_PROCESSING_NEEDED
    if (detach) {
      /* Clear the orphan list for object lifetimes. */
      char                      *entry_ptr, *next_entry_ptr;
      an_orphaned_il_entry_list *orphan_header =
                     &orphaned_file_scope_il_entries[(int)iek_object_lifetime];
      for (entry_ptr = orphan_header->first_entry;
           entry_ptr != NULL; 
           entry_ptr = next_entry_ptr) {
        next_entry_ptr = fs_orphan_pointer_of(entry_ptr);
        fs_orphan_pointer_of(entry_ptr) = NULL;
      }  /* for */
      orphan_header->first_entry = NULL;
      orphan_header->last_entry = NULL;
    }  /* if */
#endif /* ORPHAN_PROCESSING_NEEDED */
  }  /* if */
}  /* clean_up_all_object_lifetimes */


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
  db_space_used_lost("destr. entity descrs", avail_destructible_entity_descrs,
                     num_destructible_entity_descrs_allocated,
                     a_destructible_entity_descr);
  db_space_used_lost("return memos", avail_return_memos,
                     num_return_memos_allocated, a_return_memo);

  db_space_used_total();

  return grand_total;
}  /* show_lowering_space_used */
#endif /* DEBUG */


void il_lower_one_time_init(void)
/*
Do one-time initialization of variables related to IL lowering.
(Variables that need to be reinitialized with each new translation unit
are handled in il_lower_init.)
*/
{
  /* Save variables from lower_il.h and lower_il.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_init_pos_modifiers),
      pch_saved_var_array_elem(avail_destructible_entity_descrs),
      pch_saved_var_array_elem(avail_return_memos),
      pch_saved_var_array_elem(pure_virtual_called_routine),
      pch_saved_var_array_elem(vptp_type),
      pch_saved_var_array_elem(mptr_type),
      pch_saved_var_array_elem(mptr_d_field),
      pch_saved_var_array_elem(mptr_i_field),
      pch_saved_var_array_elem(mptr_f_field),
#if DEBUG
      pch_saved_var_array_elem(num_init_pos_modifiers_allocated),
      pch_saved_var_array_elem(num_destructible_entity_descrs_allocated),
      pch_saved_var_array_elem(allocated_name_string_length),
      pch_saved_var_array_elem(num_return_memos_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  init_lower_one_time_init();
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  eh_lower_one_time_init();
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
}  /* il_lower_one_time_init */


void il_lower_init(void)
/*
Initialize static variables related to IL lowering.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Variables in lower_il.h: */
  /* Object lifetime information is only kept if it will be needed by the
     back end.  It's only needed if exception handling is enabled. */
#if KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
  keep_object_lifetime_info_in_lowered_il = exceptions_enabled;
#else /* ! KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
  keep_object_lifetime_info_in_lowered_il = FALSE;
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#if CHECKING && ASSIGNMENT_TO_THIS_ALLOWED
  /* lower_dynamic_init can't handle preserving an object lifetime
     for a constructor init in an assignment to "this".  Assignment to
     "this" is disabled when exception handling is enabled.  Don't allow
     the combination that can't be handled. */
  if (!exceptions_enabled && keep_object_lifetime_info_in_lowered_il) {
    unexpected_condition_str2("ASSIGNMENT_TO_THIS must be disabled",
                              "to keep object lifetimes when EH is disabled");
  }  /* if */
#endif /* CHECKING && ASSIGNMENT_TO_THIS_ALLOWED */
  avail_init_pos_modifiers = NULL;
  avail_destructible_entity_descrs = NULL;
#if DEBUG
  num_init_pos_modifiers_allocated = 0;
  num_destructible_entity_descrs_allocated = 0;
#endif /* DEBUG */
  return_value_pointer_variable = NULL;
  code_pos_for_lowering = null_source_position;
  /* Static variables in lower_il.c: */
  avail_return_memos = NULL;
  pure_virtual_called_routine = NULL;
  vptp_type = NULL;
  mptr_type = NULL;
#if DEBUG
  allocated_name_string_length  = 0;
  num_return_memos_allocated    = 0;
#endif /* DEBUG */
  /* name_lower_init is called from fe_init.c because name mangling can
     be used separately from the rest of IL lowering. */
  /* Do lower_init.c initialization. */
  init_lower_init();
#if DO_LOWERING_OF_EXCEPTION_HANDLING
  /* Do lower_eh.c initialization. */
  eh_lower_init();
#endif /* DO_LOWERING_OF_EXCEPTION_HANDLING */
}  /* il_lower_init */

#endif /* DO_IL_LOWERING */
#endif /* NEED_NAME_MANGLING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
