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

exprutil.c -- Expression scanning utility routines.

*/

#include "basics.h"
#include "host_envir.h"
#include "mem_manage.h"
#include "debug.h"
#include "error.h"
#include "lexical.h"
#include "il.h"
#include "symbol_tbl.h"
#include "expr.h"
#include "exprutil.h"
#include "preproc.h"
#include "folding.h"
#include "const_ints.h"
#include "cmd_line.h"
#include "types.h"
#include "decls.h"
#include "class_decl.h"
#include "templates.h"

/*
Information on references to symbols, held until the kind of reference to
the symbol is known.
*/
static a_ref_entry_ptr
		avail_ref_entries;
			/* List of reference entries that have been freed
			   and are available for reuse. */

static an_arg_operand_ptr
		avail_arg_operands;
			/* List of argument operand entries that have been
			   freed and are available for reuse. */

static a_dynamic_init_dtor_fixup_ptr
		avail_dynamic_init_dtor_fixups;
			/* List of dynamic init dtor fixup entries that have
			   been freed and are available for reuse. */



#if DEBUG
/*
Counts of entries allocated, for debugging purposes.
*/
unsigned long	num_arg_operands_allocated,
		num_ref_entries_allocated,
		num_dynamic_init_dtor_fixups_allocated;
#endif /* DEBUG */


static a_ref_entry_ptr alloc_ref_entry(a_symbol_ptr            sym_ptr,
                                       a_source_position       *pos)
/*
Allocate a reference entry and return a pointer to it.  The information
provided (kind of expression, symbol pointer, and location of reference) is
placed in the entry.  At this point the kind of reference is the generic
SRK_REFERENCE.  An entry of this kind is used to hold information on a single
reference to a symbol in an expression.  The information is held, rather than
recorded immediately, because the kind of reference may be revised as more of
the expression is scanned.
*/
{
  a_ref_entry_ptr rep;

  if (avail_ref_entries != NULL) {
    /* Reuse a previously-freed entry. */
    rep = avail_ref_entries;
    avail_ref_entries = rep->next;
  } else {
    /* Allocate a new entry. */
    rep = (a_ref_entry_ptr)alloc_fe(sizeof(a_ref_entry));
#if DEBUG
    num_ref_entries_allocated++;
#endif /* DEBUG */
  }  /* if */
  rep->kind = SRK_REFERENCE;
  rep->already_recorded = FALSE;
  rep->symbol = sym_ptr;
  copy_source_position(*pos, rep->position);
  rep->next = NULL;
  rep->next_operand_ref = NULL;
  return rep;
}  /* alloc_ref_entry */


static void free_ref_entry(a_ref_entry_ptr rep)
/*
Free the reference entry pointed to by rep.
*/
{
  /* Add the entry to the available list. */
  rep->next = avail_ref_entries;
  avail_ref_entries = rep;
}  /* free_ref_entry */


static void record_reference(a_ref_entry_ptr rep)
/*
Record the symbol reference described by the reference entry rep.
*/
{
  /* Do not record the reference if it has already been recorded. */
  if (!rep->already_recorded) {
    reference_to_symbol(rep->kind, rep->symbol, &rep->position,
                        /*update_il_entry=*/TRUE);
    rep->already_recorded = TRUE;
  }  /* if */
}  /* record_reference */


static void record_and_free_ref_entry(a_ref_entry_ptr rep)
/*
Record the reference indicated in the reference entry rep, and free the entry.
*/
{
  record_reference(rep);
  free_ref_entry(rep);
}  /* record_and_free_ref_entry */


static a_boolean on_operand_ref_list(a_ref_entry_ptr rep,
                                     a_ref_entry_ptr list)
/*
Return TRUE if the reference entry "rep" appears on the list of reference
entries headed by "list".  The list considered is the one linked on the
next_operand_ref field, i.e., the list attached to an operand.
*/
{
  a_boolean on_list = FALSE;

  for (; list != NULL; list = list->next_operand_ref) {
    if (rep == list) {
      /* The entry is on the list. */
      on_list = TRUE;
      break;
    }  /* if */
  }  /* for */
  return on_list;
}  /* on_operand_ref_list */


void flush_ref_entries_except(a_ref_entry_ptr keep_list1,
                              a_ref_entry_ptr keep_list2)
/*
Look for reference entries on the curr_expr_ref_entries list that are not
also on either of the keep_list1 or keep_list2 lists.  For each such entry,
record the indicated reference and free the entry.  This is used at the
ends of expressions to commit the references that are not being passed
up to the caller and therefore cannot be modified further.
*/
{
  a_ref_entry_ptr rep, last_rep, next_rep;

  rep = curr_expr_ref_entries;
  curr_expr_ref_entries = NULL;
  last_rep = NULL;
  for (; rep != NULL; rep = next_rep) {
    next_rep = rep->next;
    if (on_operand_ref_list(rep, keep_list1) ||
        on_operand_ref_list(rep, keep_list2)) {
      /* This entry appears on a list, so keep it on the global list. */
      if (curr_expr_ref_entries == NULL) {
        curr_expr_ref_entries = rep;
      } else {
        last_rep->next = rep;
      }  /* if */
      last_rep = rep;
      rep->next = NULL;
    } else {
      /* This entry does not appear on a list, so record it and free it. */
      record_and_free_ref_entry(rep);
    }  /* if */
  }  /* for */
}  /* flush_ref_entries_except */


static void flush_ref_entries_list(void)
/*
If there are any entries on the list of reference entries for the current
expression, record and free them now.
*/
{
  a_ref_entry_ptr rep;

  while (curr_expr_ref_entries != NULL) {
    rep = curr_expr_ref_entries;
    curr_expr_ref_entries = rep->next;
    record_and_free_ref_entry(rep);
  }  /* while */
}  /* flush_ref_entries_list */


a_ref_entry_ptr ref_entry(a_symbol_ptr      sym_ptr,
                          a_source_position *source_position)
/*
Allocate a reference entry for a reference to the symbol sym_ptr
at position source_position, put the entry on the list of entries for
the current expression, and return a pointer to it.  This is needed when
the kind of reference is not known yet because it can be affected by
context.  When the proper reference kind has been determined, the entry
will be updated, and the reference will be recorded when the entry is
freed.  If the proper kind of reference is known right away, it is
recorded right away and no entry is created; NULL is returned.
*/
{
  a_ref_entry_ptr rep, last_rep;
  a_boolean       ref_kind_can_be_affected_by_context;
  a_boolean       evaluated = curr_expr_is_potentially_evaluated();
  a_symbol_ptr    fund_sym = fundamental_symbol_of(sym_ptr);

  /* For only certain kinds of symbols can the kind of reference be affected
     by context: for example, variables can have SRK_USE, SRK_MODIFICATION,
     and SRK_ADDRESS_TAKEN references, but types can only have SRK_REFERENCE
     references. */
  switch (fund_sym->kind) {
    case sk_constant:            /* Constant (enumerator). */
    case sk_variable:            /* Variable or parameter. */
    case sk_field:               /* Nonstatic data member of a class. */
    case sk_static_data_member:  /* Static data member of a class. */
    case sk_member_function:     /* Member function of a class. */
    case sk_routine:             /* Nonmember function. */
      ref_kind_can_be_affected_by_context = TRUE;
      break;
#if CHECKING
    case sk_overloaded_function: /* Overloaded function (member or not). */
      internal_error("ref_entry: overloaded function");
#endif /* CHECKING */
    default:;
      ref_kind_can_be_affected_by_context = FALSE;
      break;
  }  /* switch */
  /* References in not-evaluated expressions are always plain references,
     since they don't use or affect the values of variables. */
  if (!ref_kind_can_be_affected_by_context || !evaluated) {
    /* The kind of reference is independent of context, so record it right
       away and do not build an entry. */
    reference_to_symbol(SRK_REFERENCE, sym_ptr, source_position,
                        /*update_il_entry=*/evaluated);
    rep = NULL;
  } else {
    /* The kind of reference can be affected by context, so build an entry
       for it. */
    rep = alloc_ref_entry(sym_ptr, source_position);
    /* Put the entry on the list of entries for the current expression.
       The list is dumped when flush_ref_entries_list is called.
       The entry is put at the end of the list to preserve source order. */
    if (curr_expr_ref_entries == NULL) {
      curr_expr_ref_entries = rep;
    } else {
      for (last_rep = curr_expr_ref_entries;
           last_rep->next != NULL;
           last_rep = last_rep->next) {}
      last_rep->next = rep;
    }  /* if */
  }  /* if */
  return rep;
}  /* ref_entry */


static void f_check_address_taken_ref(a_ref_entry_ptr rep)
/*
The reference entry pointed to by rep includes an address-taken
reference.  Check to see if the symbol in the reference can have its
address taken, and if not issue an error.
*/
{
  a_symbol_ptr sym = rep->symbol;

  /* Note that this routine is not the right place to set the address_taken
     flag in variables and routines, because not all references that
     are address-taken initially stay that way.  In "a[1] = 1", for
     example, an error should be issued (here), but the address_taken
     flag on the variable "a" is not set because the address-taken reference
     gets changed later to a simple modification. */
  /* The only possible error cases are variables, and only register variables
     at that.  Functions can always have their addresses taken.  Also
     static data members (they cannot be "register"). */
  if (sym->kind == (a_symbol_kind)sk_variable) {
    a_variable_ptr var = sym->variant.variable.ptr;
    if (C_dialect != C_dialect_cplusplus &&
        var->storage_class == (a_storage_class)sc_register) {
      /* Error -- cannot take the address of a register variable.
         (This is allowed in C++.) */
      pos_error(ec_address_of_register_variable, &rep->position);
      /* Turn the reference into an error reference so that the error will
         be issued only once. */
      rep->kind = (rep->kind & SRK_ALL_REFERENCES) | SRK_ERROR;
    }  /* if */
  }  /* if */
}  /* f_check_address_taken_ref */


/*
If the reference entry pointed to by rep includes an address-taken
reference, check to see if the symbol in the reference can have its address
taken, and if not issue an error.
*/
#define check_address_taken_ref(rep)                                  \
{ if ((rep)->kind & SRK_ADDRESS_TAKEN) f_check_address_taken_ref(rep); }


void change_ref_kinds(a_ref_entry_ptr         ref_list,
                      a_symbol_reference_kind new_kind)
/*
Change the kind-of-reference field to new_kind in each of the reference
entries on the list ref_list.  The list is linked by the next_operand_ref
field.
*/
{
  a_ref_entry_ptr         rep;
  a_symbol_reference_kind old_kind;

  /* Go through the list of references and change the reference kinds. */
  for (rep = ref_list; rep != NULL; rep = rep->next_operand_ref) {
    old_kind = rep->kind;
    if (old_kind & SRK_ERROR) {
      /* An error reference is never changed to something else. */
    } else {
      /* For some cases, the old kind of reference is put out before the new
         kind is set.  That's necessary, for example, in
           void f(int &);
           int j;
           void m () {
             f(j = 1);  // Modification gets replaced by address taken
           }
         One really wants both kinds of references. */
      if ((old_kind & SRK_MODIFICATION) &&
          new_kind == SRK_ADDRESS_TAKEN) {
        record_reference(rep);
      }  /* if */
      /* Set the new reference kind. Turn off all old bits, then turn on
         new bits.  SRK_REFERENCE remains set in all cases. */
      rep->kind = (old_kind & ~SRK_ALL_REFERENCES) | new_kind;
      /* If the reference kinds include address-taken, check for errors
         related to that. */
      check_address_taken_ref(rep);
    }  /* if */
  }  /* for */
}  /* change_ref_kinds */


void change_refs_to_error(a_ref_entry_ptr ref_list)
/*
Change the reference entries on the list ref_list to error references.
The list is linked by the next_operand_ref field.
*/
{
  change_ref_kinds(ref_list, SRK_ERROR);
}  /* change_refs_to_error */


void change_operand_refs_to_error(an_operand *operand)
/*
Change the reference kind in any references attached to operand to SRK_ERROR.
*/
{
  change_refs_to_error(operand->ref_entries_list);
  /* Error entries cannot be changed to anything else, so there's no point in
     keeping them attached to the operand. */
  operand->ref_entries_list = NULL;
}  /* change_operand_refs_to_error */


void change_arg_operand_list_refs_to_error(an_arg_operand_ptr arg_operand_list)
/*
Change the references on each operand in the list of operands headed by
arg_operand_list to error references.
*/
{
  an_arg_operand_ptr arg_operand;

  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    change_operand_refs_to_error(&arg_operand->operand);
  }  /* for */
}  /* change_arg_operand_list_refs_to_error */


void change_some_ref_kinds(a_ref_entry_ptr         ref_list,
                           a_symbol_reference_kind old_kind,
                           a_symbol_reference_kind new_kind)
/*
Change the kind-of-reference field to "new_kind" in each of the reference
entries on the list ref_list that currently has the kind "old_kind".
The list is linked by the next_operand_ref field.
*/
{
  a_ref_entry_ptr rep;
  a_boolean       changed;

  for (rep = ref_list; rep != NULL; rep = rep->next_operand_ref) {
    changed = FALSE;
    if (old_kind == SRK_REFERENCE) {
      /* Changing a generic reference (SRK_REFERENCE alone) to another
         kind of reference.  Check to see if the rep entry is generic. */
      if ((rep->kind & SRK_ALL_REFERENCES) == 0) {
        /* Yes.  Change the reference kind. */
        rep->kind |= new_kind;
        changed = TRUE;
      }  /* if */
    } else if ((rep->kind & old_kind) != 0) {
      /* Changing a specific reference to another kind of reference.
         Turn off the old bits, then turn on the new bits.
         SRK_REFERENCE will stay set. */
      rep->kind = (rep->kind & ~SRK_ALL_REFERENCES) | new_kind;
      changed = TRUE;
    }  /* if */
    /* If the reference kinds include address-taken, check for errors
       related to that. */
    if (changed) check_address_taken_ref(rep);
  }  /* for */
}  /* change_some_ref_kinds */


void record_operand_modification_refs(an_operand *operand)
/*
Record any modification references indicated on the references list for
*operand.  This is used at potential sequence points. The reference entries
are left on the list but are marked as having been already recorded.
*/
{
  a_ref_entry_ptr rep;

  for (rep = operand->ref_entries_list;
       rep != NULL;
       rep = rep->next_operand_ref) {
    if (rep->kind & SRK_MODIFICATION) {
      record_reference(rep);
    }  /* if */
  }  /* for */
}  /* record_operand_modification_refs */


an_arg_operand_ptr alloc_arg_operand(void)
/*
Allocate an argument operand entry and return a pointer to it.  Such
entries are used to hold arguments of function calls.
*/
{
  an_arg_operand_ptr aop;

  if (avail_arg_operands != NULL) {
    /* Reuse a previously-freed entry. */
    aop = avail_arg_operands;
    avail_arg_operands = aop->next;
  } else {
    /* Allocate a new entry. */
    aop = (an_arg_operand_ptr)alloc_fe(sizeof(an_arg_operand));
#if DEBUG
    num_arg_operands_allocated++;
#endif /* DEBUG */
  }  /* if */
  aop->next = NULL;
  clear_operand((an_operand_kind)ok_error, &aop->operand);
  return aop;
}  /* alloc_arg_operand */


void free_arg_operand_list(an_arg_operand_ptr aop)
/*
Free the list of argument operands pointed to by aop.
*/
{
  an_arg_operand_ptr aop_next;

  for (; aop != NULL; aop = aop_next) {
    aop_next = aop->next;
    /* Add the entry to the available list. */
    aop->next = avail_arg_operands;
    avail_arg_operands = aop;
  }  /* for */
}  /* free_arg_operand_list */


a_dynamic_init_dtor_fixup_ptr alloc_dynamic_init_dtor_fixup(
                                               a_dynamic_init_ptr dynamic_init,
                                               a_source_position  *position)
/*
Allocate an entry to record a fixup to be done on the dynamic initialization
entry dynamic_init.  The associated source position is given by "position".
The entry is put on the fixup list for the current expression context.
*/
{
  a_dynamic_init_dtor_fixup_ptr didfp;

  if (avail_dynamic_init_dtor_fixups != NULL) {
    /* Reuse a previously-freed entry. */
    didfp = avail_dynamic_init_dtor_fixups;
    avail_dynamic_init_dtor_fixups = didfp->next;
  } else {
    /* Allocate a new entry. */
    didfp = (a_dynamic_init_dtor_fixup_ptr)
                                   alloc_fe(sizeof(a_dynamic_init_dtor_fixup));
#if DEBUG
    num_dynamic_init_dtor_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
  didfp->next = expr_stack->dynamic_init_dtor_fixup_list;
  expr_stack->dynamic_init_dtor_fixup_list = didfp;
  didfp->dynamic_init = dynamic_init;
  didfp->position = *position;
  return didfp;
}  /* alloc_dynamic_init_dtor_fixup */


void free_dynamic_init_dtor_fixup(a_dynamic_init_dtor_fixup_ptr didfp)
/*
Free the dynamic init dtor fixup entry pointed to by didfp.
*/
{
  /* Add the entry to the available list. */
  didfp->next = avail_dynamic_init_dtor_fixups;
  avail_dynamic_init_dtor_fixups = didfp;
}  /* free_dynamic_init_dtor_fixup */


void if_evaluating_mark_routine_referenced(a_routine_ptr     routine)
/*
Mark the indicated routine as actually referenced, but only if the current
expression is being evaluated.  This routine is an interface to
mark_routine_referenced.
*/
{
  if (curr_expr_is_potentially_evaluated()) {
    mark_routine_referenced(routine);
  }  /* if */
}  /* if_evaluating_mark_routine_referenced */


void push_expr_stack(an_expression_kind      expression_kind,
                     an_expr_stack_entry_ptr new_entry)
/*
Push a new entry on the top of the expr_stack.  expression_kind indicates
the kind of the expression.  new_entry is used as the new top-of-stack
entry (the entries are local variables on the stack).  This is done
at the start of a major expression.
*/
{
  new_entry->prev = expr_stack;
  new_entry->expression_kind = expression_kind;
  new_entry->old_ref_entries_list = curr_expr_ref_entries;
  curr_expr_ref_entries = NULL;
  new_entry->evaluated = TRUE;
  new_entry->potentially_evaluated = TRUE;
  new_entry->is_default_arg_expression = FALSE;
  new_entry->is_template_arg_expression = FALSE;
  new_entry->in_return_by_cctor_expression = FALSE;
  new_entry->dynamic_init_dtor_fixup_list = NULL;
  new_entry->nested_construct_depth = 0;
  if (expr_stack != NULL) {
    /* There is a previous stack entry; set any of the flags that are affected
       by the enclosing stack entry. */
    new_entry->evaluated = expr_stack->evaluated;
    new_entry->potentially_evaluated = expr_stack->potentially_evaluated;
    new_entry->is_default_arg_expression =
                                         expr_stack->is_default_arg_expression;
  }  /* if */
  expr_stack = new_entry;
  /* Constant addressing expressions should be folded to constants inside
     constant expressions.  This is set late so that
     curr_expr_kind_is_const can be used. */
  expr_stack->fold_constant_addr_exprs = curr_expr_kind_is_const();
}  /* push_expr_stack */


void pop_expr_stack(void)
/*
Pop the top entry off the expr_stack.  This is done at the end of a
major expression.
*/
{
  /* Flush the reference entries list for the current expression. */
  flush_ref_entries_list();
  /* Restore the old reference entries list, if any. */
  curr_expr_ref_entries = expr_stack->old_ref_entries_list;
  /* Pop the stack. */
  expr_stack = expr_stack->prev;
}  /* pop_expr_stack */


void set_operand_kind(an_operand      *operand,
                      an_operand_kind kind)
/*
Set the kind of the indicated operand.  Also set associated variant fields
to default values.
*/
{
  operand->kind = kind;
  /* Set the variant fields based on the kind of operand. */
  switch (kind) {
    case ok_error:
      /* No variant fields. */
      break;
    case ok_expression:
      operand->variant.expression = NULL;
      break;
    case ok_constant:
      clear_constant(&operand->variant.constant,
		     (a_constant_repr_kind)ck_error);
      break;
    case ok_indefinite_function:
    case ok_sym_for_member:
    case ok_undefined_symbol:
      operand->variant.symbol = NULL;
      break;
#if CHECKING
    default:
      internal_error("set_operand_kind: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_operand_kind */


void clear_operand(an_operand_kind kind,
		   an_operand      *operand)
/*
Clear an operand and set its kind to "kind".  Set other fields to default
values.
*/
{
  /* Set the non-variant fields to a safe state. */
  operand->type  = NULL;
  operand->state = (an_operand_state)os_none;
  operand->bound_function = FALSE;
  operand->virtual_function = FALSE;
  operand->is_qualified_name = FALSE;
  operand->came_from_reference = FALSE;
  operand->access_control_error_reported = FALSE;
  operand->is_operand_of_address_of = FALSE;
  operand->position.seq = 0;
  operand->position.column = SP_COL_UNKNOWN;
  operand->ref_entries_list = NULL;
  set_operand_kind(operand, kind);
}  /* clear_operand */


an_expr_node_ptr make_node_from_operand(an_operand *operand)
/*
Make a node from an operand.  If the operand contains a constant, allocate a
constant record and copy the constant value to it.  If the operand is an error
operand, create an error node.  If the operand is an expression, return the
expression node.
*/
{
  register an_expr_node_ptr node;
  a_constant_ptr            con;

  switch (operand->kind) {
    case ok_error:
      /* Make an error node.  Its type will be error. */
      node = error_node();
      break;
    case ok_expression:
      /* Just return the node in the operand. */
      node = operand->variant.expression;
      break;
    case ok_constant:
      con = &operand->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_template_param &&
          con->variant.template_param.kind ==
                             (a_template_param_constant_kind)tpck_expression) {
        /* For a ck_template_param case that represents an expression,
           return the expression. */
        node = con->variant.template_param.variant.expr;
      } else {
        /* Create a constant node and copy the constant in the operand to the
           node. */
        node = alloc_node_for_constant(con);
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error
	("make_node_from_operand: converting unexpected operand kind");
#endif /* CHECKING */
  }  /* switch */

  return node;
}  /* make_node_from_operand */


void extract_constant_from_operand(an_operand     *operand,
                                   a_constant_ptr constant)
/*
Extract the constant value from the operand *operand and place it in
*constant.
*/
{
  switch (operand->kind) {
    case ok_error:
      set_error_constant(constant);
      break;
    case ok_constant:
      copy_constant(&operand->variant.constant, constant);
      break;
#if CHECKING
    default:
      internal_error("extract_constant_from_operand: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
}  /* extract_constant_from_operand */


void discard_operand(an_operand *operand)
/*
Discard the indicated operand.  It has been scanned as a normal operand,
but it's now known that it should have been a not-evaluated operand.
This is only used in C++, for left operands of field selections.
*/
{
  /* The references in the operand aren't real references. */
  change_ref_kinds(operand->ref_entries_list, SRK_REFERENCE);
}  /* discard_operand */


void restore_operand_details(an_operand *operand,
                             an_operand *orig_operand)
/*
*operand has been subjected to some sort of modification, which may have
destroyed its source position, etc.  Restore such things from
*orig_operand, which is a copy of *operand before the modification.
*/
{
  operand->position = orig_operand->position;
  operand->bound_function = orig_operand->bound_function;
  operand->is_qualified_name = orig_operand->is_qualified_name;
  operand->access_control_error_reported =
                                   orig_operand->access_control_error_reported;
  operand->is_operand_of_address_of = orig_operand->is_operand_of_address_of;
}  /* restore_operand_details */


static void restore_operand_details_incl_ref(an_operand *operand,
                                             an_operand *orig_operand)
/*
*operand has been subjected to some sort of modification, which may have
destroyed its source position, etc.  Restore such things from
*orig_operand, which is a copy of *operand before the modification.
Restore the ref_entries_list too (not usually wanted).
*/
{
  restore_operand_details(operand, orig_operand);
  operand->ref_entries_list = orig_operand->ref_entries_list;
}  /* restore_operand_details_incl_ref */


void make_error_operand(an_operand *operand)
/*
Create an error operand.
*/
{
  clear_operand((an_operand_kind)ok_error, operand);
  operand->type = error_type();
  copy_source_position(error_position, operand->position);
}  /* make_error_operand */


void conv_to_error_operand(an_operand *operand)
/*
Take an existing operand and convert it to an error operand.  Retain the
position field as the error position.
*/
{
  /* Change the references to errors. */
  change_operand_refs_to_error(operand);
  set_operand_kind(operand, (an_operand_kind)ok_error);
  operand->type = error_type();
  operand->state = (an_operand_state)os_none;
  operand->came_from_reference = FALSE;
  /* bound_function is not cleared on purpose. */
}  /* conv_to_error_operand */


void error_and_make_error_operand(an_error_code error_code,
				  an_operand    *operand)
/*
Announce an error and set the operand to an error operand.
*/
{
  error(error_code);
  make_error_operand(operand);
}  /* error_and_make_error_operand */


void error_in_operand(an_error_code error_code,
		      an_operand    *operand)
/*
Announce an error at the position in the operand and convert the operand to an
error operand.
*/
{
  pos_error(error_code, &operand->position);
  conv_to_error_operand(operand);
}  /* error_in_operand */


void sym_error_in_operand(an_error_code error_code,
                          an_operand    *operand,
                          a_symbol_ptr  sym)
/*
Announce an error at the position in the operand and convert the operand to an
error operand.  The symbol sym is cited in the error message.
*/
{
  pos_sy_error(error_code, &operand->position, sym);
  conv_to_error_operand(operand);
}  /* sym_error_in_operand */


void type2_error_in_operand(an_error_code error_code,
                            an_operand    *operand,
                            a_type_ptr    type1,
                            a_type_ptr    type2)
/*
Announce an error at the position in the operand and convert the operand to an
error operand.  The two types are cited in the error message.
*/
{
  pos_ty2_error(error_code, &operand->position, type1, type2);
  conv_to_error_operand(operand);
}  /* type2_error_in_operand */


void make_constant_operand(a_constant *constant,
			   an_operand *operand)
/*
Make a constant operand for the given constant.  The position of the
current token will be used as the operand position.
*/
{
  if (is_error_type(constant->type)) {
    make_error_operand(operand);
  } else {
    clear_operand((an_operand_kind)ok_constant, operand);
    copy_constant(constant, &operand->variant.constant);
    operand->type = constant->type;
  }  /* if */
  operand->state = (an_operand_state)os_rvalue;
  copy_source_position(pos_curr_token, operand->position);
}  /* make_constant_operand */


void make_sym_constant_operand(a_symbol_ptr sym,
			       an_operand   *operand)
/*
Make a constant operand for the value of the given sk_constant symbol.
The position of the current token will be used as the operand position.
*/
{
  a_constant *con_ptr, constant;
  a_type_ptr underlying_type;

  check_assertion(sym->kind == (a_symbol_kind)sk_constant);
  con_ptr = sym->variant.constant;
  if (is_reference_type(con_ptr->type)) {
    /* The constant has a reference type.  This happens for a constant
       that is an argument for a nontype template parameter that has
       a reference type. */
    /* Make a version of the constant with pointer type, and make an lvalue
       based on that constant. */
    copy_constant(con_ptr, &constant);
    underlying_type = type_pointed_to(constant.type);
    constant.type = make_pointer_type(underlying_type);
    make_constant_operand(&constant, operand);
    operand->state = (an_operand_state)os_lvalue;
    operand->type = underlying_type;
  } else {
    /* Normal (non-reference) case. */
    make_constant_operand(con_ptr, operand);
  }  /* if */
}  /* make_sym_constant_operand */


void make_string_constant_operand(a_constant *constant,
                                  an_operand *operand)
/*
Make a constant operand for the given string constant.  The position of the
current token will be used as the operand position.
*/
{
  a_constant_ptr string_constant;
  a_constant     addr_constant;

  /* Make an allocated copy of the string constant. */
  if (string_literals_shared) {
    /* Strings are shareable. */
    string_constant = alloc_shareable_constant(constant);
  } else {
    /* Strings are not shared. */
    string_constant = alloc_unshared_constant(constant);
  }  /* if */
  /* Make an address constant that points to the string. */
  set_constant_address_constant(string_constant, &addr_constant);
  /* Note that the type is left as "pointer to array of char" here;
     the implicit conversion to "pointer to char" is done separately. */
  make_constant_operand(&addr_constant, operand);
  /* Treat a string literal constant as an lvalue. */
  operand->state = (an_operand_state)os_lvalue;
  operand->type = constant->type;
}  /* make_string_constant_operand */


void make_integer_constant_operand(an_operand *operand,
				   long       value)
/*
Make a constant operand and set it to some integer value.
*/
{
  clear_operand((an_operand_kind)ok_constant, operand);
  set_integer_constant(&operand->variant.constant, value,
                       (an_integer_kind)ik_int);
  operand->type = operand->variant.constant.type;
  operand->state = (an_operand_state)os_rvalue;
  copy_source_position(pos_curr_token, operand->position);
}  /* make_integer_constant_operand */


void make_expression_operand(an_expr_node_ptr node,
                             a_type_ptr       type,
			     an_operand       *operand)
/*
Make an expression operand for the expression "node", with type "type".
The operand is made an rvalue; the caller should change that if it's not
appropriate.  The position of the current token will be used as the
operand position.
*/
{
  clear_operand((an_operand_kind)ok_expression, operand);
  operand->type = type;
  operand->state = (an_operand_state)os_rvalue;
  operand->variant.expression = node;
  copy_source_position(pos_curr_token, operand->position);
}  /* make_expression_operand */


void make_indefinite_function_operand(a_symbol_ptr routine_sym,
                                      a_boolean    is_qualified_name,
                                      an_operand   *operand)
/*
Make an operand for a C++ overloaded function symbol.  routine_sym points
to the symbol entry for the function (possibly a projection symbol).
is_qualified_name is TRUE if the function was named by a qualified name
(e.g., "A::f").  The operand is put into *operand and is a function designator.
*/
{
  clear_operand((an_operand_kind)ok_indefinite_function, operand);
  operand->state = (an_operand_state)os_function_designator;
  operand->type = unknown_type();
  operand->is_qualified_name = is_qualified_name;
  operand->variant.symbol = routine_sym;
  copy_source_position(pos_curr_token, operand->position);
}  /* make_indefinite_function_operand */


void make_sym_for_member_operand(a_symbol_ptr    member_sym,
                                 a_ref_entry_ptr rep,
                                 an_operand      *operand)
/*
Make an operand for a class member name, used in C++ for qualified
names that appear in a context that calls for (or might call for) a
pointer-to-member.  For nonstatic data members, that means only cases
like &A::x.  For nonstatic member functions, it means all cases like
A::f, because such a thing might decay to a pointer-to-member (that's
an extension) or it might be called.  Not used for overloaded functions.
member_sym points to the symbol entry for the member.  rep points to
an associated reference entry, or is NULL if none is needed.  The
operand is put into *operand.  It is a function designator if the
symbol is a function, an rvalue otherwise.
*/
{
  a_symbol_ptr fund_sym = fundamental_symbol_of(member_sym);

  clear_operand((an_operand_kind)ok_sym_for_member, operand);
  if (fund_sym->kind == (a_symbol_kind)sk_field) {
    /* Data member. */
    operand->state = (an_operand_state)os_rvalue;
    operand->type = fund_sym->variant.field.ptr->type;
  } else {
    check_assertion(fund_sym->kind == (a_symbol_kind)sk_member_function);
    /* Member function. */
    operand->state = (an_operand_state)os_function_designator;
    operand->type = fund_sym->variant.routine.ptr->type;
  }  /* if */
  operand->variant.symbol = member_sym;
  operand->is_qualified_name = TRUE;  /* By definition. */
  copy_source_position(pos_curr_token, operand->position);
  operand->ref_entries_list = rep;
}  /* make_sym_for_member_operand */


void make_template_param_expr_constant_operand(
                                              an_operand            *operand_1,
                                              an_operand            *operand_2,
                                              an_expr_operator_kind op,
                                              a_type_ptr            type,
                                              an_operand            *result)
/*
Build an operand for a ck_template_param constant for the expression
"operand_1 op operand_2" (or, if operand_2 is NULL, "op operand_1").
The result type is "type".  Return the operand in *result.
*/
{
  an_expr_node_ptr node;
  a_constant       con;

  node = make_node_from_operand(operand_1);
  if (operand_2 != NULL) {
    node ->next = make_node_from_operand(operand_2);
  }  /* if */
  node = make_operator_node(op, type, node);
  /* Build the ck_template_param constant. */
  clear_constant(&con, (a_constant_repr_kind)ck_template_param);
  con.variant.template_param.kind =
                               (a_template_param_constant_kind)tpck_expression;
  con.variant.template_param.variant.expr = node;
  con.type = type;
  /* Make the operand. */
  make_constant_operand(&con, result);
}  /* make_template_param_expr_constant_operand */


static void add_base_class_casts(a_base_class_ptr  bcp,
                                 a_type_ptr        qualifiers_model,
                                 a_boolean         check_cast_access,
                                 a_boolean         is_implicit_cast,
                                 an_expr_node_ptr  *p_node,
                                 a_source_position *err_pos)
/*
Add casts to *p_node to change its type from a pointer to a class type to
a pointer to a base class of that class; bcp indicates the base class
and qualifiers_model indicates the qualifiers to be placed on that class
type.  Access control is done on the cast if check_cast_access is TRUE.
is_implicit_cast is TRUE if the cast is implicit.  *err_pos indicates a
source position to be used for errors.  This routine is only used in C++ mode.
*/
{
  a_boolean             access_okay;
  a_type_ptr            curr_type, qual_curr_type;
  a_derivation_step_ptr dsp;
  a_base_class_ptr      base_class;

  /* The code here looks like fold_base_class_cast. */
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    *p_node = error_node();
  } else {
    /* Loop through the classes between the derived class and the
       base class.  Check accessibility at each step and generate the
       necessary casts. */
    access_okay = TRUE;
    curr_type = type_pointed_to((*p_node)->type);
    curr_type = skip_typerefs(curr_type);
    for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
      base_class = dsp->base_class;
      /* Check that the base class is accessible from the current class. */
      if (check_cast_access) {
        if (!is_accessible_imm_base_class(base_class, curr_type)) {
          /* The base class is inaccessible.  Keep going, but issue the
             error only once. */
          if (access_okay) {
            pos_ty_error(ec_inaccessible_base_class, err_pos,
                         base_class->type);
            access_okay = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Add the cast to the next level. */
      curr_type = base_class->type;
      /* The type should have all the qualifiers of the qualifiers_model
         type. */
      qual_curr_type = make_identically_qualified_type(curr_type,
                                                       qualifiers_model);
      *p_node = make_operator_node((an_expr_operator_kind)eok_base_class_cast,
                                   make_pointer_type(qual_curr_type), *p_node);
      (*p_node)->variant.operation.compiler_generated = is_implicit_cast;
    }  /* for */
  }  /* if */
}  /* add_base_class_casts */
    

static void add_a_derived_class_cast(a_type_ptr            new_type_pointed_to,
                                     a_derivation_step_ptr dsp,
                                     an_expr_node_ptr      *p_node)
/*
Helper routine for add_derived_class_casts: adds casts to *p_node to change
its type to pointer to new_type_pointed_to.  dsp points to the derivation
list from the desired type to the current type (i.e., it's backwards from
what's needed).
*/
{
  /* Use recursion to get to the bottom of the list and work upwards.
     Add casts to get the type we have to the type just below the type
     we want. */
  if (dsp->next != NULL) {
    add_a_derived_class_cast(dsp->base_class->type, dsp->next, p_node);
  }  /* if */
  /* Add the node to do the final cast. */
  *p_node = make_operator_node((an_expr_operator_kind)eok_derived_class_cast,
                               make_pointer_type(new_type_pointed_to),
                               *p_node);
  /* No need to set compiler_generated; a derived class cast is always
     explicit. */
}  /* add_a_derived_class_cast */


static void add_derived_class_casts(a_type_ptr        new_type_pointed_to,
                                    a_base_class_ptr  bcp,
                                    an_expr_node_ptr  *p_node,
                                    a_source_position *err_pos)
/*
Add casts to *p_node to change its type from pointer to a class type to
pointer to new_type_pointed_to, a derived class of that class; bcp indicates
the base class of the derived class that corresponds to the current type
(i.e., its derivation list is backwards from what's needed).  *err_pos
indicates a source position to be used for errors.  This routine is only
used in C++ mode.
*/
{
  /* The code here looks like fold_derived_class_cast. */
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty2_error(ec_ambiguous_derived_class, err_pos,
                  new_type_pointed_to, bcp->type);
    *p_node = error_node();
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                  new_type_pointed_to, bcp->type);
    *p_node = error_node();
  } else {
    /* Use recursion to process the list backwards to generate casts. */
    add_a_derived_class_cast(new_type_pointed_to, cast_derivation_path_of(bcp),
                             p_node);
  }  /* if */
}  /* add_derived_class_casts */


static void add_pm_base_class_casts(a_base_class_ptr  bcp,
                                    an_expr_node_ptr  *p_node,
                                    a_source_position *err_pos)
/*
Add casts to *p_node to change its type from a pointer to a member of
a class type to a pointer to a member of a base class of that class; bcp
indicates the base class.  *err_pos indicates a source position to be used
for errors.  This routine is only used in C++ mode.  Note that casts of
this type always come from explicit casts, so checking for accessibility
of base classes is not necessary.
*/
{
  a_type_ptr            curr_type;
  a_type_ptr            member_type = pm_member_type((*p_node)->type);
  a_derivation_step_ptr dsp;

  /* The code here looks like fold_pm_base_class_cast. */
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    *p_node = error_node();
  } else {
    /* Loop through the classes between the derived class and the
       base class.  Generate the necessary casts. */
    for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
      /* Add the cast to the next level. */
      curr_type = dsp->base_class->type;
      *p_node = make_operator_node(
                                 (an_expr_operator_kind)eok_pm_base_class_cast,
                                 related_ptr_to_member_type(member_type,
                                                            curr_type),
                                 *p_node);
      /* No need to set compiler_generated; this cast cannot be implicit. */
    }  /* for */
  }  /* if */
}  /* add_pm_base_class_casts */
    

static void add_a_pm_derived_class_cast(
                                    a_type_ptr            new_class_pointed_to,
                                    a_derivation_step_ptr dsp,
                                    a_boolean             is_implicit_cast,
                                    an_expr_node_ptr      *p_node)
/*
Helper routine for add_pm_derived_class_casts: adds casts to *p_node to change
its type to pointer to member of new_class_pointed_to.  dsp points to the
derivation list from the desired type to the current type (i.e., it's
backwards from what's needed).  is_implicit_cast is TRUE if the cast
is implicit.
*/
{
  a_type_ptr member_type = pm_member_type((*p_node)->type);

  /* Use recursion to get to the bottom of the list and work upwards.
     Add casts to get the type we have to the type just below the type
     we want. */
  if (dsp->next != NULL) {
    add_a_pm_derived_class_cast(dsp->base_class->type, dsp->next,
                                is_implicit_cast, p_node);
  }  /* if */
  /* Add the node to do the final cast. */
  *p_node = make_operator_node(
                              (an_expr_operator_kind)eok_pm_derived_class_cast,
                              related_ptr_to_member_type(member_type,
                                                         new_class_pointed_to),
                              *p_node);
  (*p_node)->variant.operation.compiler_generated = is_implicit_cast;
}  /* add_a_pm_derived_class_cast */


static void add_pm_derived_class_casts(a_type_ptr        new_class_pointed_to,
                                       a_base_class_ptr  bcp,
                                       a_boolean         check_cast_access,
                                       a_boolean         is_implicit_cast,
                                       an_expr_node_ptr  *p_node,
                                       a_source_position *err_pos)
/*
Add casts to *p_node to change its type from pointer to member of a class type
to pointer to a member of new_class_pointed_to, a derived class of that
class; bcp indicates the base class of the derived class that corresponds
to the current type (i.e., its derivation list is backwards from what's
needed).  Do access control on the cast if check_cast_access is TRUE.
is_implicit_cast is TRUE if the cast is implicit.  *err_pos indicates a
source position to be used for errors.  This routine is only used in C++ mode.
*/
{
  a_type_ptr            curr_type;
  a_derivation_step_ptr dsp;
  a_base_class_ptr      base_class;

  /* The code here looks like fold_pm_derived_class_cast. */
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty2_error(ec_ambiguous_derived_class, err_pos,
                  new_class_pointed_to, bcp->type);
    *p_node = error_node();
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                  new_class_pointed_to, bcp->type);
    *p_node = error_node();
  } else {
    if (check_cast_access) {
      /* Check the accessibility of the base class.  (Recall that casts
         to derived types can be done implicitly.) */
      curr_type = new_class_pointed_to;
      for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
        base_class = dsp->base_class;
        /* Check that the base class is accessible from the current class. */
        if (!is_accessible_imm_base_class(base_class, curr_type)) {
          pos_ty_error(ec_inaccessible_base_class, err_pos, base_class->type);
          break;
        }  /* if */
        curr_type = base_class->type;
      }  /* for */
    }  /* if */
    /* Use recursion to process the list backwards to generate casts. */
    add_a_pm_derived_class_cast(new_class_pointed_to,
                                cast_derivation_path_of(bcp),
                                is_implicit_cast, p_node);
  }  /* if */
}  /* add_pm_derived_class_casts */


static void add_cast_to_node(an_expr_node_ptr  *p_node,
                             a_type_ptr        new_type,
                             a_boolean         is_implicit_cast,
                             a_source_position *err_pos)
/*
Add a cast node to the expression tree pointed to by *p_node, and update
*p_node to point to the cast node.  The old node is cast to the type new_type.
*err_pos gives the source position for errors.  If is_implicit_cast is TRUE,
this is an implicit cast rather than an explicit one.  This routine generates
the special IL operators used for base-->derived and derived-->base class
pointer casts, when they are appropriate.  It also issues errors for
invalid casts of that kind (e.g., ambiguous).
*/
{
  a_type_ptr       old_type = (*p_node)->type, new_type_pointed_to;
  a_boolean        downward_cast;
  a_base_class_ptr bcp;

  /* Make sure the next field of the node is cleared.  The caller must 
     make sure it's copied if that's necessary (it can't be done here,
     because we can't link a previous expression in a list to this
     new expression). */
  (*p_node)->next = NULL;
  if (related_class_pointers(old_type, new_type, &downward_cast, &bcp)) {
    /* C++ cast from a pointer to a class to a pointer to a related
       (base or derived) class. */
    new_type_pointed_to = type_pointed_to(new_type);
    if (downward_cast) {
      /* Derived --> base.  Valid unless the cast is ambiguous or
         the base class is inaccessible. */
      add_base_class_casts(bcp, new_type_pointed_to,
                           /*check_cast_access=*/is_implicit_cast,
                           is_implicit_cast, p_node, err_pos);
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      add_derived_class_casts(new_type_pointed_to, bcp, p_node, err_pos);
    }  /* if */
  } else if (related_member_pointers(old_type, new_type, &downward_cast,
                                     &bcp)) {
    /* C++ cast from pointer-to-member to
       pointer-to-member-of-related-class. */
    if (downward_cast) {
      /* Derived --> base (allowed only as an explicit cast).  Valid unless
         the cast is ambiguous. */
      add_pm_base_class_casts(bcp, p_node, err_pos);
    } else {
      /* Base --> derived (allowed as an implicit or explicit cast).  Valid
         unless the cast is ambiguous, the base class is inaccessible (if
         the cast is implicit), or the base class is a virtual base of the
         derived class. */
      new_type_pointed_to = pm_class_type(new_type);
      add_pm_derived_class_casts(new_type_pointed_to, bcp,
                                 /*check_cast_access=*/is_implicit_cast,
                                 is_implicit_cast, p_node, err_pos);
    }  /* if */
  } else {
    /* For an ordinary cast, generate the eok_cast node. */
    *p_node = make_operator_node((an_expr_operator_kind)eok_cast, new_type,
                                 *p_node);
    (*p_node)->variant.operation.compiler_generated = is_implicit_cast;
  }  /* if */
}  /* add_cast_to_node */


void cast_node(an_expr_node_ptr  *node,
               a_type_ptr        new_type,
	       a_boolean         is_implicit_cast,
               a_source_position *err_pos)
/*
Change the type of a node.  If the node is a constant, a conversion is done on
the constant value; otherwise a cast operator is added on top of the node.
If is_implicit_cast is TRUE, this is an implicit cast rather than an explicit
one.  Warnings about truncation etc. are issued only if is_implicit_cast
is TRUE.  *err_pos gives the source position for errors.  The current
expression is assumed to be a nonconstant expression; if it were a
constant expression, we wouldn't have an expression node.
The caller must have already determined that the conversion is allowed,
except for casts to ambiguous or inaccessible base classes.
*/
{
  a_constant local_constant;
  a_boolean  did_not_fold;

  /* Drop any qualifiers on the destination type.  Qualifiers on an rvalue
     have no meaning. */
  new_type = make_unqualified_type(new_type);
  if (il_identical_types((*node)->type, new_type)) {
    /* If the new type is identical to the old type, just put the new type
       in the node (since it may be "identical" but not exactly the same). */
    (*node)->type = new_type;
  } else if (m_is_error_type(new_type)) {
    /* Casting to an error type changes the node to an error node. */
    *node = error_node();
  } else {
    did_not_fold = TRUE;
    if (is_constant_node(*node)) {
      /* Copy the constant to a local copy.  Type-change the local constant
         and copy the result to a new constant.  Since we are in a
         nonconstant context, reduce any error to a warning and leave
         the conversion to be done at runtime. */
      copy_constant((*node)->variant.constant, &local_constant);
      type_change_constant(&local_constant, new_type, is_implicit_cast,
                           /*constant_context=*/FALSE, &did_not_fold,
                           err_pos);
    }  /* if */
    if (did_not_fold) {
      /* The operand is not constant.  Put in a cast. */
      /* Note that if the constant type-change was attempted, it was
         done on a copy of the constant.  The original constant and
         expression were not changed, and therefore can be used here. */
      add_cast_to_node(node, new_type, is_implicit_cast, err_pos);
    } else {
      /* The operation was successfully folded to a constant. */
      (*node)->variant.constant = alloc_shareable_constant(&local_constant);
      (*node)->type = new_type;
    }  /* if */
  }  /* if */
}  /* cast_node */


void cast_operand(a_type_ptr new_type,
		  an_operand *operand,
		  a_boolean  is_implicit_cast)
/*
Cast the operand to the new type.  If is_implicit_cast is TRUE, this
is an implicit cast rather than an explicit one.  If there are any
warnings detected on the type change, issue them only if is_implicit_cast
is TRUE.  The operand must be an rvalue or error operand.
The caller must have already determined that the conversion is allowed,
except for casts to ambiguous or inaccessible base classes.
*/
{
  a_boolean         did_not_fold, access_error_reported, ambiguous;
  a_constant        local_constant;
  an_expr_node_ptr  node;
  an_operand        orig_operand;
  a_symbol_ptr      overloaded_function_symbol, function_symbol;
  an_arg_match_level
                    match_level;

#if CHECKING
  if (!is_an_rvalue(operand) && !is_error_operand(operand)) {
    internal_error("cast_operand: operand is not an rvalue");
  }  /* if */
#endif /* CHECKING */

  /* Drop any qualifiers on the destination type.  Qualifiers on an rvalue
     have no meaning. */
  new_type = make_unqualified_type(new_type);
  /* Can't test for il_identical_types at this point, since for an
     ok_expression the node type would have to be adjusted as well.
     Leave that to cast_node.  However, we can check for exact pointer
     equality ("il_identical_types" includes some cases where the pointers
     aren't exactly the same). */
  if (new_type != operand->type) {
    /* Save the operand's source position, etc. */
    orig_operand = *operand;
    if (m_is_error_type(new_type)) {
      conv_to_error_operand(operand);
    } else {
      switch (operand->kind) {
        case ok_error:
          /* Do nothing. */
          break;
        case ok_expression:
          /* Cast the expression node.  If the expression is a constant,
             change its type in place.  Otherwise, add a cast expression
             node. */
          node = operand->variant.expression;
          cast_node(&node, new_type, is_implicit_cast, &operand->position);
          make_expression_operand(node, new_type, operand);
          break;
        case ok_constant:
          /* Cast the constant by changing its type.  In a nonconstant
             context, reduce any error to a warning and leave the
             conversion to be done at runtime. */
          did_not_fold = TRUE;
          if (curr_expr_is_evaluated() ||
              /* In C mode, (void *)0 is a null pointer constant and must
                 be folded even when not evaluated so it can be recognized
                 as a null pointer constant. */
              (C_dialect != C_dialect_cplusplus &&
               is_zero_constant(&operand->variant.constant) &&
               is_pointer_type(new_type))) {
            copy_constant(&operand->variant.constant, &local_constant);
            type_change_constant(&local_constant, new_type, is_implicit_cast,
                                 curr_expr_kind_is_const(),
                                 &did_not_fold, &operand->position);
          }  /* if */
          if (did_not_fold) {
            /* Cast of a constant did not fold. */
            if (curr_expr_kind_is_const() && curr_expr_is_evaluated()) {
              error_in_operand(ec_expr_not_constant, operand);
            } else if (il_identical_types(operand->type, new_type)) {
              /* If the new type is identical to the old type, just put the
                 new type in the node (since it may be "identical" but not
                 exactly the same). */
              operand->type = new_type;
            } else {
              /* Create an expression node for the cast of the constant. */
              /* Note that the constant type-change was attempted on a
                 copy of the constant.  The original constant was not
                 changed, and therefore can be used here. */
              node = make_node_from_operand(operand);
              add_cast_to_node(&node, new_type, is_implicit_cast,
                               &operand->position);
              make_expression_operand(node, new_type, operand);
            }  /* if */
          } else {
            /* The operation was successfully folded to a constant. */
            make_constant_operand(&local_constant, operand);
          }  /* if */
          break;
        case ok_indefinite_function:
          /* Cast of overloaded function to a pointer type.  Note that
             the legality of such casts is checked by conversion_possible.
             A cast cannot get here unless allowed by that routine. */
          overloaded_function_symbol = operand->variant.symbol;
          function_symbol = find_addr_of_overloaded_function_match(
                                                    overloaded_function_symbol,
                                                    new_type,
                                                    &operand->position,
                                                    &match_level,
                                                    &ambiguous);
#if CHECKING
          if (function_symbol == NULL) {
            internal_error("cast_operand: bad func symbol");
          }  /* if */
#endif /* CHECKING */
          if (is_ptr_to_member_type(new_type)) {
            /* Casting an overloaded member function to a pointer to
               member function. */
            /* Do whatever would have been done with the function if we had
               known all along which function was intended.  Do not create
               a function designator operand. */
            overloaded_function_catch_up(function_symbol,
                                         overloaded_function_symbol,
                                         (a_boolean)operand->is_qualified_name,
                                         &operand->position,
                                         /*elided_reference=*/FALSE,
                                         (an_operand *)NULL,
                                         &access_error_reported);
            if (strict_ansi_mode && !operand->is_qualified_name &&
                operand->is_operand_of_address_of) {
              /* Taking the address of a member function without using
                 a qualified name is nonstandard.  Suppress the message here
                 if the message about not using "&" will be generated
                 by make_ptr_to_member_constant_operand. */
              pos_diagnostic(strict_ansi_error_severity,
                             ec_nonstd_member_function_address,
                             &operand->position);
            }  /* if */
            make_ptr_to_member_constant_operand(function_symbol,
                                                overloaded_function_symbol,
                                                &orig_operand.position,
                                                !access_error_reported,
                                                (a_boolean)operand->
                                                      is_operand_of_address_of,
                                                operand);
          } else {
            /* Casting an overloaded nonmember or static member function
               to a pointer to function. */
            /* Do whatever would have been done with the function if we had
               known all along which function was intended.  Make an operand
               for the specific function's address. */
            overloaded_function_catch_up(function_symbol,
                                         overloaded_function_symbol,
                                         (a_boolean)operand->is_qualified_name,
                                         &orig_operand.position,
                                         /*elided_reference=*/FALSE,
                                         operand,
                                         &access_error_reported);
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("cast_operand: bad operand kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    /* Restore the original source position, etc.  Keep the reference
       information (useful when this is a pointer to a class being cast to
       a base class, or a pointer to an array being cast to a pointer to
       the first element). */
    restore_operand_details_incl_ref(operand, &orig_operand);
  }  /* if */
}  /* cast_operand */


void conv_selector_to_object_pointer(an_operand *operand,
                                     a_boolean  *is_arrow_operator)
/*
operand is the left operand of a selection operation (".", "->", ".*",
or "->*").  *is_arrow_operator is TRUE if the operand is an rvalue address,
FALSE if it's an object (lvalue or rvalue).  We need to be able to deal
with the operand as an address, so if *is_arrow_operator is FALSE,
convert operand to an address and set *is_arrow_operator to TRUE.
*/
{
  if (!*is_arrow_operator) {
    /* Convert the operand to an address. */
    conv_class_operand_to_object_pointer(operand);
    *is_arrow_operator = TRUE;
  }  /* if */
}  /* conv_selector_to_object_pointer */


void base_class_cast_operand(an_operand       *operand,
                             a_base_class_ptr bcp,
                             a_boolean        *is_arrow_operator,
                             a_boolean        check_cast_access)
/*
Cast operand (of class or pointer-to-class type) to its base class
identified by bcp.  If *is_arrow_operator is TRUE, operand is being
used as a pointer ("->"); otherwise, it is being used as an object (".").
*is_arrow_operator will be set to TRUE on return to indicate that the
operation was normalized into "->" form.  Do access control checking
on the cast if check_cast_access is TRUE.  This routine is only used
in C++ mode.
*/
{
  a_boolean        did_not_fold;
  an_expr_node_ptr node;
  a_constant       temp_con;
  an_operand       orig_operand;

  /* Save the original operand position, etc. */
  orig_operand = *operand;
  /* Convert to "->" form by getting an address for the operand. */
  conv_selector_to_object_pointer(operand, is_arrow_operator);
  if (is_error_operand(operand)) {
    /* Leave an error operand alone. */
  } else {
    did_not_fold = TRUE;
    if (curr_expr_is_evaluated() && expr_stack->fold_constant_addr_exprs &&
        is_constant_operand(operand)) {
      /* Fold a cast of a constant address into another constant address.
         This folding is always done in constant expressions, but in some
         nonconstant expressions it's not done because it's clearer to
         have the cast in the IL (the constant form has only an offset,
         and loses the sequence of casts). */
      fold_base_class_cast(&operand->variant.constant, bcp,
                           &temp_con, check_cast_access, &did_not_fold,
                           &orig_operand.position);
    }  /* if */
    if (did_not_fold) {
      /* The cast could not be folded to a constant. */
      if (curr_expr_kind_is_const() && curr_expr_is_evaluated()) {
        /* The cast must fold to a constant in a constant expression. */
        error_in_operand(ec_expr_not_constant, operand);
      } else {
        /* Build an expression node or nodes for the cast. */
        node = make_node_from_operand(operand);
        add_base_class_casts(bcp, type_pointed_to(operand->type),
                             check_cast_access, /*is_implicit_cast=*/TRUE,
                             &node, &orig_operand.position);
        make_expression_operand(node, node->type, operand);
      }  /* if */
    } else {
      /* The cast was folded to a constant. */
      make_constant_operand(&temp_con, operand);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details_incl_ref(operand, &orig_operand);
}  /* base_class_cast_operand */


/*
Test an expression node to see if it's a bit-field extraction.
*/
#define is_bit_field_extract_node(node) \
  (is_operation_node(node) && \
   ((node)->variant.operation.kind == \
                                (an_expr_operator_kind)eok_value_bit_field || \
    (node)->variant.operation.kind == \
                               (an_expr_operator_kind)eok_extract_bit_field))


a_type_ptr node_type_after_integral_promotion(an_expr_node_ptr node)
/*
Determine the type that would result from applying the integral promotions
(3.2.1.1) to the type of node.  Return the promoted type, which may be
the same as the original type.  Type qualifiers, if any, are dropped.
The node is not actually promoted; it is up to the caller to do the cast
if desired.  This routine handles a special case involving integral promotions
of bit-fields, where the size in bits is needed in addition to the base type.
*/
{
  a_type_ptr      promoted_type;
  a_field_ptr     field;
  an_integer_kind ikind;
  a_boolean       is_signed;

  db_enter(4, "node_type_after_integral_promotion");

  if (is_bit_field_extract_node(node)) {
    /* This is a bit-field reference. */
    field = node->variant.operation.operands->next->variant.field;
    promoted_type = skip_typerefs(node->type);
#if CHECKING
    /* The type of a bit-field should be integral. */
    if (promoted_type->kind != (a_type_kind)tk_integer) {
      internal_error(
                 "node_type_after_integral_promotion: bit-field not integral");
    }  /* if */
    if (field->bit_size > TARG_SIZEOF_LONG*TARG_CHAR_BIT) {
      /* This is supposedly prevented by the definition of
         TARG_MAX_BIT_FIELD_SIZE. */
      internal_error("node_type_after_integral_promotion: bit-field too big");
    }  /* if */
#endif /* CHECKING */
    ikind = promoted_type->variant.integer.int_kind;
    if (C_dialect == C_dialect_pcc) {
      /* In pcc mode, we use unsigned-preserving rules, so the promoted type
         is int or unsigned int depending on the signedness of the original
         type.  If the size is bigger than int, the promoted type is long
         or unsigned long. */
#if LONG_LONG_ALLOWED
      /* ... or long long or unsigned long long. */
#endif /* LONG_LONG_ALLOWED */
      is_signed = int_kind_is_signed[(int)ikind];
      if (field->bit_size <= TARG_SIZEOF_INT*TARG_CHAR_BIT) {
        ikind = is_signed ? (an_integer_kind)ik_int :
                            (an_integer_kind)ik_unsigned_int;
      } else {
#if LONG_LONG_ALLOWED
        if (field->bit_size <= TARG_SIZEOF_LONG*TARG_CHAR_BIT) {
#endif /* LONG_LONG_ALLOWED */
          ikind = is_signed ? (an_integer_kind)ik_long :
                              (an_integer_kind)ik_unsigned_long;
#if LONG_LONG_ALLOWED
        } else {
          ikind = is_signed ? (an_integer_kind)ik_long_long :
                              (an_integer_kind)ik_unsigned_long_long;
        }  /* if */
#endif /* LONG_LONG_ALLOWED */
      }  /* if */
    } else {
      /* ANSI mode, so value-preserving rules apply. */
      if (ikind == (an_integer_kind)ik_int) {
        /* Bit-field is signed, so it is promoted to the first of int or
           long into which all its values will fit. */
#if LONG_LONG_ALLOWED
        /* ... or long long. */
#endif /* LONG_LONG_ALLOWED */
        if (field->bit_size <= TARG_SIZEOF_INT*TARG_CHAR_BIT) {
          ikind = (an_integer_kind)ik_int;
        } else {
#if LONG_LONG_ALLOWED
          if (field->bit_size <= TARG_SIZEOF_LONG*TARG_CHAR_BIT) {
#endif /* LONG_LONG_ALLOWED */
            ikind = (an_integer_kind)ik_long;
#if LONG_LONG_ALLOWED
          } else {
            ikind = (an_integer_kind)ik_long_long;
          }  /* if */
#endif /* LONG_LONG_ALLOWED */
        }  /* if */
      } else {
        /* Bit-field is unsigned, so it is promoted to the first of int,
           unsigned int, long, and unsigned long into which all its values
           will fit. */
#if LONG_LONG_ALLOWED
        /* ... or long long or unsigned long long. */
#endif /* LONG_LONG_ALLOWED */
        if (field->bit_size < TARG_SIZEOF_INT*TARG_CHAR_BIT) {
          ikind = (an_integer_kind)ik_int;
        } else if (field->bit_size == TARG_SIZEOF_INT*TARG_CHAR_BIT) {
          ikind = (an_integer_kind)ik_unsigned_int;
        } else if (field->bit_size < TARG_SIZEOF_LONG*TARG_CHAR_BIT) {
          ikind = (an_integer_kind)ik_long;
        } else {
#if LONG_LONG_ALLOWED
          if (field->bit_size == TARG_SIZEOF_LONG*TARG_CHAR_BIT) {
#endif /* LONG_LONG_ALLOWED */
            ikind = (an_integer_kind)ik_unsigned_long;
#if LONG_LONG_ALLOWED
          } else if (field->bit_size < TARG_SIZEOF_LONG_LONG*TARG_CHAR_BIT) {
            ikind = (an_integer_kind)ik_long_long;
          } else {
            ikind = (an_integer_kind)ik_unsigned_long_long;
          }  /* if */
#endif /* LONG_LONG_ALLOWED */
        }  /* if */
      }  /* if */
    }  /* if */
    promoted_type = integer_type(ikind);
  } else {
    /* Not the bit-field case, so determine the promoted type from the
       node type. */
    promoted_type = type_after_integral_promotion(node->type);
  }  /* if */

  db_exit();
  return promoted_type;
}  /* node_type_after_integral_promotion */


void integral_promote_node(an_expr_node_ptr *node)
/*
Determine the integral promotion and do the promotion on a node.
See 3.2.1.1 in the standard.  error_position is used for the error position
if an error is issued.
*/
{
  a_type_ptr new_type;

  new_type = node_type_after_integral_promotion(*node);
  cast_node(node, new_type, /*is_implicit_cast=*/TRUE, &error_position);
}  /* integral_promote_node */


static a_type_ptr operand_type_after_integral_promotion(an_operand *operand)
/*
Determine the type that would result from applying the integral promotions
(3.2.1.1) to *operand.  Return the promoted type, which may be
the same as the original type.  Type qualifiers, if any, are dropped.
*/
{
  a_type_ptr promoted_type;

  /* If the operand is an expression node, use a special routine to
     catch the special bit-field case. */
  if (is_expression_operand(operand) && is_an_rvalue(operand)) {
    promoted_type =
               node_type_after_integral_promotion(operand->variant.expression);
  } else {
    promoted_type = type_after_integral_promotion(operand->type);
  }  /* if */

  return promoted_type;
}  /* operand_type_after_integral_promotion */


void promote_operand(an_operand *operand)
/*
Determine the integral promotion and do the promotion on an operand.
See 3.2.1.1 in the standard.
*/
{
  cast_operand(operand_type_after_integral_promotion(operand), operand,
               /*is_implicit_cast=*/TRUE);
}  /* promote_operand */


void arg_default_promote_operand(an_operand *argument_operand)
/*
Do default argument promotions on an argument operand.
*/
{
  /* Convert the operand to an rvalue if necessary. */
  do_operand_transformations(argument_operand, TOPT_NO_OPTIONS);
  /* Do the integral promotions part of the default argument promotions
     directly on the operand because of the special case with 
     bit-fields (which can't be handled from just the type). */
  if (is_integral_type(argument_operand->type)) {
    promote_operand(argument_operand);
  } else if (is_incomplete_type(argument_operand->type)) {
    /* Catch a case like "f((void)2)" -- an argument with an incomplete
       type is not allowed. */
    error_in_operand(ec_incomplete_type_not_allowed, argument_operand);
  } else {
    cast_operand(default_argument_promotion(argument_operand->type),
                 argument_operand, /*is_implicit_cast=*/TRUE);
  }  /* if */
}  /* arg_default_promote_operand */


void build_unary_result_operand(an_operand            *operand,
			        an_expr_operator_kind kind,
			        a_type_ptr            type,
	                        an_operand            *result)
/*
Build an operand for the expression that is the operator "kind" operating
on "operand", with result type "type".  The operand is an rvalue.  Note that
this is used also to begin building a node for the (three-operand) ?:
operator.
*/
{
  an_expr_node_ptr  node;

  node = make_node_from_operand(operand);
  node = make_operator_node(kind, type, node);
  make_expression_operand(node, type, result);
}  /* build_unary_result_operand */


void build_binary_result_operand(an_operand            *operand_1,
	       			 an_operand            *operand_2,
				 an_expr_operator_kind kind,
				 a_type_ptr            type,
	       			 an_operand            *result)
/*
Build an operand for the expression that is the operator "kind" operating
on "operand_1" and "operand_2", with result type "type".
*/
{
  an_expr_node_ptr node;

  if (kind == (an_expr_operator_kind)eok_error) {
    /* If the operator is an error, the expression tree cannot be built; return
       an error operand.  eok_error is returned by which_binary_operator
       to indicate a case where the operator cannot be determined. */
    make_error_operand(result);
  } else {
    /* Make nodes from the operands, and link the first node to the second. */
    node = make_node_from_operand(operand_1);
    node->next = make_node_from_operand(operand_2);
    /* Make an expression operator node which has the above operand nodes. */
    node = make_operator_node(kind, type, node);
    /* Make an operand of the expression. */
    make_expression_operand(node, type, result);
  }  /* if */
}  /* build_binary_result_operand */


/* Type predicates used by determine_arithmetic_conversions. */

#define is_long_double(fkind)                                         \
  ((fkind) == (a_float_kind)fk_long_double)

#define is_double(fkind)                                              \
  ((fkind) == (a_float_kind)fk_double)

#define is_float(fkind)                                               \
  ((fkind) == (a_float_kind)fk_float)

#if LONG_LONG_ALLOWED
#define is_unsigned_long_long(ikind)                                  \
  ((ikind) == (an_integer_kind)ik_unsigned_long_long)

#define is_long_long(ikind)                                           \
  ((ikind) == (an_integer_kind)ik_long_long)
#endif /* LONG_LONG_ALLOWED */

#define is_unsigned_long(ikind)                                       \
  ((ikind) == (an_integer_kind)ik_unsigned_long)

#define is_long(ikind)                                                \
  ((ikind) == (an_integer_kind)ik_long)

#define is_unsigned_int(ikind)                                        \
  ((ikind) == (an_integer_kind)ik_unsigned_int)


a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
					    an_operand *operand_2)
/*
Determine the "usual arithmetic conversions" on the operands to make them
compatible, and return the type of the result.  Note that this routine assumes
that the type is arithmetic, and does not actually change the result type.
See section 3.2.1.5 of the standard.
*/
{
  a_type_ptr      type_1;
  a_type_ptr      type_2;
  a_type_ptr      result_type;
  a_float_kind    fkind_1, fkind_2;
  an_integer_kind ikind_1, ikind_2;

  db_enter(4, "determine_arithmetic_conversions");

  if (is_error_type(operand_1->type) || is_error_type(operand_2->type)) {
    result_type = error_type();
  } else {
    /* Get past possible typerefs. */
    type_1 = skip_typerefs(operand_1->type);
    type_2 = skip_typerefs(operand_2->type);

    fkind_1 = is_floating_type(type_1) ? type_1->variant.float_kind :
                                         (a_float_kind)fk_last;
    fkind_2 = is_floating_type(type_2) ? type_2->variant.float_kind :
                                         (a_float_kind)fk_last;

    if (is_long_double(fkind_1) || is_long_double(fkind_2)) {
      /* If either operand has type "long double", the other operand is
	 converted to "long double". */
      result_type = float_type((a_float_kind)fk_long_double);
    } else if (is_double(fkind_1) || is_double(fkind_2)) {
      /* If either operand has type "double", the other operand is converted to
         "double". */
      result_type = float_type((a_float_kind)fk_double);
    } else if (is_float(fkind_1) || is_float(fkind_2)) {
      /* If either operand has type "float", the other operand is converted to
         "float". */
      if (C_dialect == C_dialect_pcc) {
        /* When in pcc mode, all float operations are done as double. */
        result_type = float_type((a_float_kind)fk_double);
      } else {
        result_type = float_type((a_float_kind)fk_float);
      }  /* if */
    } else {
      /* Neither operand had type float; do the integral promotions on both
	 operands and try to get the result type from that. */
      type_1 = operand_type_after_integral_promotion(operand_1);
      type_2 = operand_type_after_integral_promotion(operand_2);

      ikind_1 = is_integral_type(type_1) ? type_1->variant.integer.int_kind :
                                           (an_integer_kind)ik_last;
      ikind_2 = is_integral_type(type_2) ? type_2->variant.integer.int_kind :
                                           (an_integer_kind)ik_last;

#if LONG_LONG_ALLOWED
      if (is_unsigned_long_long(ikind_1) || is_unsigned_long_long(ikind_2)) {
        /* If either operand has type "unsigned long long", the other operand
           is converted to "unsigned long long". */
        result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
      } else if (is_long_long(ikind_1) || is_long_long(ikind_2)) {
        /* At least one operand has type "long long". */
#if TARG_SIZEOF_LONG_LONG == TARG_SIZEOF_LONG
        /* A long long cannot represent all unsigned long values, so check for
           unsigned long values. */
        if (is_unsigned_long(ikind_1) || is_unsigned_long(ikind_2)) {
          /* One operand has type "long long" and the other has type
             "unsigned long", and all values of "unsigned long" cannot be
             represented by "long long", so both operands are converted to
             "unsigned long long". */
          result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
        } else
#endif /* TARG_SIZEOF_LONG_LONG == TARG_SIZEOF_LONG */
#if TARG_SIZEOF_LONG_LONG == TARG_SIZEOF_INT
        /* A long long cannot represent all unsigned int values, so check for
           unsigned int values. */
        if (is_unsigned_int(ikind_1) || is_unsigned_int(ikind_2)) {
          /* One operand has type "long long" and the other has type
             "unsigned int", and all values of "unsigned int" cannot be
             represented by "long long", so both operands are converted to
             "unsigned long long". */
          result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
        } else
#endif /* TARG_SIZEOF_LONG_LONG == TARG_SIZEOF_INT */
        {
          /* One operand has type "long long"; the other operand is
             converted to "long long". */
          result_type = integer_type((an_integer_kind)ik_long_long);
        }
      } else 
#endif /* LONG_LONG_ALLOWED */
      if (is_unsigned_long(ikind_1) || is_unsigned_long(ikind_2)) {
        /* If either operand has type "unsigned long", the other operand is
	   converted to "unsigned long". */
        result_type = integer_type((an_integer_kind)ik_unsigned_long);
      } else {
        if (is_long(ikind_1) || is_long(ikind_2)) {
          /* At least one operand has type "long". */
#if TARG_SIZEOF_LONG == TARG_SIZEOF_INT
          /* A "long" cannot represent all "unsigned int" values, so check for
             "unsigned int" values. */
          if (is_unsigned_int(ikind_1) || is_unsigned_int(ikind_2)) {
            /* One operand has type "long" and the other has type
               "unsigned int", and all values of "unsigned int" cannot be
               represented by "long", so both operands are converted to
               "unsigned long". */
            result_type = integer_type((an_integer_kind)ik_unsigned_long);
          } else
#endif /* TARG_SIZEOF_LONG == TARG_SIZEOF_INT */
          {
            /* One operand has type "long"; the other operand is
               converted to "long". */
            result_type = integer_type((an_integer_kind)ik_long);
          }
        } else if (is_unsigned_int(ikind_1) || is_unsigned_int(ikind_2)) {
          /* If either operand has type "unsigned int", the other operand is
             converted to "unsigned int". */
          result_type = integer_type((an_integer_kind)ik_unsigned_int);
        } else {
          /* Otherwise, both operands are converted to "int". */
          result_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
  return result_type;
}  /* determine_arithmetic_conversions */


a_boolean check_compatibility_of_pointer_operands(
                   an_operand        *operand_1,
                   an_operand        *operand_2,
                   a_source_position *operator_position,
                   a_boolean         pointer_normalization_standard_in_C,
                   a_boolean         pointers_to_functions_standard_in_C,
                   a_boolean         pointers_to_incomplete_standard_in_C,
                   a_boolean         mixed_object_and_incomplete_standard_in_C,
                   a_type_ptr        *operation_type)
/*
operand_1 and operand_2 are the operands of a pointer operation.  Check
to see that the operands are compatible or can be made compatible.
Return the operation type in *operation_type.  (The operands are not cast
to the operation type; the caller must do that.)  operator_position gives the
operator position (for errors).  The other switches indicate the legality
of certain constructs in ANSI C.  "Illegal" constructs are accepted
anyway, but warnings are issued in strict ANSI C mode.  The switches are
used only in strict ANSI mode.  Return FALSE if there is an error.
*/
{
  a_boolean     okay = FALSE;
  a_type_ptr    operand_1_type = operand_1->type;
  a_type_ptr    operand_2_type = operand_2->type;
  a_boolean     operand_1_is_pointer = is_pointer_type(operand_1_type);
  a_boolean     operand_2_is_pointer = is_pointer_type(operand_2_type);
  a_boolean     pointer_normalization_needed, suppress_extensions;
  an_error_code warning_suggested;

  /* The loop here tries the conversions once without extensions
     allowed, and (if that fails) again with extensions allowed.
     This is necessary for cases like the following in C mode:
       int f();
       void m() { 0 ? (void *)0 : f }
     The conversion from f --> void * can be done, but only as an
     extension.  (void *)0 --> pointer to function is standard.  If we
     did not do the tests twice, we would find the nonstandard conversion
     first and use it, and issue a warning, when in fact there is a
     standard conversion that could be used instead. */
  suppress_extensions = TRUE;
  for (;;) {
    if (operand_1_is_pointer) {
      /* See if the second operand can be converted to the type of the
         first operand. */
      if (impl_pointer_conversion(operand_2_type,
                                  is_constant_operand(operand_2),
                                  &operand_2->variant.constant,
                                  operand_1_type,
                                  /*check_as_operands_not_conversion=*/TRUE,
                                  &pointer_normalization_needed,
                                  suppress_extensions,
                                  ec_incompatible_operands,
                                  &warning_suggested)) {
        *operation_type = operand_1_type;
        okay = TRUE;
        break;
      }  /* if */
    }  /* if */
    if (operand_2_is_pointer) {
      /* See if the first operand can be converted to the type of the
         second operand. */
      if (impl_pointer_conversion(operand_1_type,
                                  is_constant_operand(operand_1),
                                  &operand_1->variant.constant,
                                  operand_2_type,
                                  /*check_as_operands_not_conversion=*/TRUE,
                                  &pointer_normalization_needed,
                                  suppress_extensions,
                                  ec_incompatible_operands,
                                  &warning_suggested)) {
        *operation_type = operand_2_type;
        okay = TRUE;
        break;
      }  /* if */
    }  /* if */
    /* Stop after second time around loop. */
    if (suppress_extensions == FALSE) break;
    /* Go back for the second iteration with extensions allowed. */
    suppress_extensions = FALSE;
  }  /* for */
  if (okay) {
    a_boolean nonstd_case = FALSE;
    if (strict_ansi_mode && C_dialect == C_dialect_ANSI) {
      /* In strict ANSI C mode, issue warnings for the extensions let by
         above. */
      if (!pointer_normalization_standard_in_C &&
          pointer_normalization_needed) {
        /* Conversion of null pointer constants to pointers, and conversion
           of pointers to "void *", are not standard in the present case. */
        nonstd_case = TRUE;
      } else {
        a_type_ptr operand_1_type_pointed_to, operand_2_type_pointed_to;
        /* Fetch the types pointed to by the pointer operands. */
        if (operand_1_is_pointer) {
          operand_1_type_pointed_to = type_pointed_to(operand_1_type);
          operand_1_type_pointed_to = skip_typerefs(operand_1_type_pointed_to);
        }  /* if */
        if (operand_2_is_pointer) {
          operand_2_type_pointed_to = type_pointed_to(operand_2_type);
          operand_2_type_pointed_to = skip_typerefs(operand_2_type_pointed_to);
        }  /* if */
        if (!pointers_to_functions_standard_in_C &&
            ((operand_1_is_pointer &&
                               is_function_type(operand_1_type_pointed_to)) ||
             (operand_2_is_pointer &&
                               is_function_type(operand_2_type_pointed_to)))) {
          /* Pointers to functions are not standard in the present case. */
          nonstd_case = TRUE;
        } else if (!pointers_to_incomplete_standard_in_C &&
                   ((operand_1_is_pointer &&
                             is_incomplete_type(operand_1_type_pointed_to)) ||
                    (operand_2_is_pointer &&
                             is_incomplete_type(operand_2_type_pointed_to)))) {
          /* Pointers to incomplete are not standard in the present case. */
          nonstd_case = TRUE;
        } else if (!mixed_object_and_incomplete_standard_in_C &&
                   operand_1_is_pointer && operand_2_is_pointer &&
                   ((is_incomplete_type(operand_1_type_pointed_to) &&
                               is_object_type(operand_2_type_pointed_to)) ||
                    (is_incomplete_type(operand_2_type_pointed_to) &&
                               is_object_type(operand_1_type_pointed_to)))) {
          /* One pointer to object and one pointer to incomplete are not
             standard in the present case. */
          nonstd_case = TRUE;
        }  /* if */
      }  /* if */
      if (nonstd_case) {
        /* A nonstandard case. */
        pos_ty2_diagnostic(strict_ansi_error_severity,
                           ec_incompatible_operands, operator_position,
                           operand_1_type, operand_2_type);
      }  /* if */
    }  /* if */
    if (warning_suggested != ec_no_error && !nonstd_case) {
      /* Oddball cases call for a warning.  Suppress this if we issued a
         diagnostic about nonstandard use. */
      pos_opt_ty2_warning(warning_suggested, operator_position,
                          operand_1_type, operand_2_type);
    }  /* if */
  } else {
    /* The operands are not compatible. */
    pos_ty2_error(ec_incompatible_operands, operator_position,
                  operand_1_type, operand_2_type);
    *operation_type = error_type();
  }  /* if */
  return okay;
}  /* check_compatibility_of_pointer_operands */


a_boolean check_ptr_to_member_operands_for_compatibility(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          a_type_ptr        *operation_type)
/*
operand_1 and operand_2 are the operands of a pointer-to-member operation.
Check to see that the operands are compatible or can be made compatible.
Return the operation type in *operation_type.  (The operands are not cast
to the operation type; the caller must do that.)  operator_position gives the
operator position (for errors).  Return FALSE if there is an error.
*/
{
  a_boolean  okay = FALSE;
  a_type_ptr operand_1_type = operand_1->type;
  a_type_ptr operand_2_type = operand_2->type;

  if (is_ptr_to_member_type(operand_1_type)) {
    /* See if the second operand can be converted to the type of the
       first operand. */
    if (impl_ptr_to_member_conversion(operand_2_type,
                                      is_constant_operand(operand_2),
                                      &operand_2->variant.constant,
                                      operand_1_type,
                                  /*check_as_operands_not_conversion=*/TRUE)) {
      *operation_type = operand_1_type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay && is_ptr_to_member_type(operand_2_type)) {
    /* See if the first operand can be converted to the type of the
       second operand. */
    if (impl_ptr_to_member_conversion(operand_1_type,
                                      is_constant_operand(operand_1),
                                      &operand_1->variant.constant,
                                      operand_2_type,
                                  /*check_as_operands_not_conversion=*/TRUE)) {
      *operation_type = operand_2_type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay) {
    /* The operands are not compatible. */
    pos_ty2_error(ec_incompatible_operands, operator_position,
                  operand_1_type, operand_2_type);
    *operation_type = error_type();
  }  /* if */
  return okay;
}  /* check_ptr_to_member_operands_for_compatibility */


void change_binary_operand_types(a_type_ptr type,
				 an_operand *operand_1,
				 an_operand *operand_2)
/*
If the current types of the operands do not match the new type, cast the
operands to the new type.  This is used for the operands of an operation,
with the type probably determined by determine_arithmetic_conversions.
*/
{
  if (!is_error_type(type)) {
    if (operand_1->type != type) {
      /* Cast operand 1 to match the desired type. */
      cast_operand(type, operand_1, /*is_implicit_cast=*/TRUE);
    }  /* if */
    if (operand_2->type != type) {
      /* Cast operand 2 to match the desired type. */
      cast_operand(type, operand_2, /*is_implicit_cast=*/TRUE);
    }  /* if */
  }  /* if */
}  /* change_binary_operand_types */


a_boolean check_modifiable_lvalue_operand(an_operand *operand)
/*
Return FALSE and issue an error message if the operand is not a modifiable
lvalue.  If there is an error, change the operand to an error operand.
*/
{
  a_boolean  okay = FALSE;
  a_type_ptr type;

  /* 3.2.2.1:  A modifiable lvalue has to
       (a)  be an lvalue.
       (b)  not have array type.
       (c)  not have an incomplete type.
       (d)  not have a const-qualified type.
       (e)  if it is a struct or union, not have any const member.
     The check for (b) is not done, because arrays get converted
     to pointers to arrays in all the contexts where one wants a
     modifiable lvalue.
  */
  type = operand->type;
  if (is_an_lvalue(operand) &&
      !is_const_qualified_type(type) &&
      !is_incomplete_type(type)) {
    okay = TRUE;
    if (is_class_struct_union_type(type)) {
      if (type->variant.class_struct_union.any_const_member) okay = FALSE;
    }  /* if */
  }  /* if */
  if (!okay) {
    if (is_error_operand(operand)) {
      /* An error message has already been issued for this operand. */
    } else {
      error_in_operand(ec_expr_not_a_modifiable_lvalue, operand);
    }  /* if */
  }  /* if */

  return okay;
}  /* check_modifiable_lvalue_operand */


a_boolean check_integral_operand(an_operand *operand)
/*
Return FALSE and issue an error message if the operand is not of integral
type.  If there is an error change "operand" to an error operand.
See section 3.1.2.5 of the standard.
*/
{
  register a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If the operand has a type of error, an error message has already been
       issued. */
    okay = FALSE;
  } else if (!is_integral_type(operand->type)) {
    error_in_operand(ec_expr_not_integral, operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_integral_operand */


a_boolean check_arithmetic_operand(an_operand *operand)
/*
Return FALSE if the operand is not of arithmetic type.  If there is an error,
change the operand to an error operand.  See section 3.1.2.5 of the standard.
*/
{
  register a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If it is an error type, an error message has already been issued. */
    okay = FALSE;
  } else if (!is_arithmetic_type(operand->type)) {
    error_in_operand(ec_expr_not_arithmetic, operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_arithmetic_operand */


a_boolean check_pointer_operand(an_operand    *operand,
				an_error_code err_code)
/*
Return FALSE and issue an error message if the operand is not a pointer type.
If there is an error, make "operand" into an error operand.
*/
{
  register a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If it is an error operand, an error message has already been issued. */
    okay = FALSE;
  } else if (!is_pointer_type(operand->type)) {
    error_in_operand(err_code, operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_pointer_operand */


a_boolean check_object_pointer_operand(an_operand    *operand,
		    		       an_error_code err_code)
/*
Return FALSE and issue an error message if the operand is not a pointer to
object.  If there is an error, change "operand" to an error operand.
See section 3.1.2.5 of the standard.
*/
{
  a_boolean  okay = TRUE;
  a_type_ptr underlying_type;

  if (!check_pointer_operand(operand, err_code)) {
    okay = FALSE;
  } else {
    /* Instantiate the underlying type if it is a template class. */
    underlying_type = type_pointed_to(operand->type);
    check_for_uninstantiated_template_class(underlying_type);
    if (!is_object_type(underlying_type)) {
      error_in_operand(err_code, operand);
      okay = FALSE;
    }  /* if */
  }  /* if */
  return okay;
}  /* check_object_pointer_operand */

#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED

a_boolean check_object_or_incomp_array_pointer_operand(an_operand    *operand,
                                                       an_error_code err_code,
                                                       an_operand    *otherop)
/*
Return FALSE and issue an error message if the operand is not a pointer to
object or incomplete array.  If there is an error, change "operand" to an
error operand.  See section 3.1.2.5 of the standard.  If the operand is
a pointer to incomplete array, issue a remark if otherop is a constant
zero, a warning otherwise.  Do not accept the pointer to incomplete
array case in strict ANSI mode.
*/
{
  a_boolean  okay = TRUE;
  a_type_ptr underlying_type;

  if (!check_pointer_operand(operand, err_code)) {
    okay = FALSE;
  } else {
    underlying_type = type_pointed_to(operand->type);
    /* Instantiate the underlying type if it is a template class. */
    check_for_uninstantiated_template_class(underlying_type);
    if (is_object_type(underlying_type)) {
      /* okay = TRUE; -- Already set. */
    } else if ((!strict_ansi_mode ||
               strict_ansi_error_severity == es_warning) &&
               is_array_type(underlying_type) &&
               is_incomplete_type(underlying_type)) {
      /* Pointer to incomplete array.  If the other operand (the one being
         combined with the pointer) is a constant zero, issue a remark
         (that's a case like p[0]); otherwise, issue a warning. */
      if (op_is_zero_constant(otherop) && !strict_ansi_mode) {
        pos_remark(err_code, &operand->position);
      } else {
        pos_warning(err_code, &operand->position);
      }  /* if */
    } else {
      /* A pointer, but not a valid pointer. */
      error_in_operand(err_code, operand);
      okay = FALSE;
    }  /* if */
  }  /* if */
  return okay;
}  /* check_object_or_incomp_pointer_operand */

#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */

a_boolean check_function_pointer_operand(an_operand *operand)
/*
Return FALSE and issue an error message if the operand is not a pointer to a
function type.  If there is an error, change the operand to an error operand.
*/
{
  register a_boolean  okay = TRUE;
  register a_type_ptr pointer_type;

  if (is_error_operand(operand)) {
    /* If the operand has a type of error, an error message has already been
       issued. */
    okay = FALSE;
  } else {
    pointer_type = skip_typerefs(operand->type);
    if (!is_pointer_type(pointer_type) ||
        !is_function_type(type_pointed_to(pointer_type))) {
      error_in_operand(ec_expr_not_ptr_to_function, operand);
      okay = FALSE;
    }  /* if */
  }  /* if */

  return okay;
}  /* check_function_pointer_operand */


a_boolean check_scalar_operand(an_operand *operand)
/*
Return FALSE and issue an error message if the operand is not of scalar type.
If there is an error, change "operand" to an error operand.
See section 3.1.2.5 of the standard.
*/
{
  register a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If the operand has a type of error, an error message has already been
       issued. */
    okay = FALSE;
  } else if (!is_scalar_type(operand->type)) {
    error_in_operand(ec_expr_not_scalar, operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_scalar_operand */


a_boolean op_is_zero_constant(an_operand *operand)
/*
Return TRUE if the operand contains a constant zero value (of integral
or floating type).
*/
{
  register a_boolean is_constant_zero = FALSE;
  an_expr_node_ptr   node;

  if (is_expression_operand(operand)) {
    /* Check if the expression is a constant zero. */
    node = operand->variant.expression;
    if (is_constant_node(node) && is_zero_constant(node->variant.constant)) {
      is_constant_zero = TRUE;
    }  /* if */
  } else if (is_constant_operand(operand)) {
    /* Check the constant in the operand itself. */
    is_constant_zero = is_zero_constant(&operand->variant.constant);
  }  /* if */
  return is_constant_zero;
}  /* op_is_zero_constant */


a_boolean op_is_false_constant(an_operand *operand)
/*
Return TRUE if the operand contains a false constant value (a zero of
integral, floating, pointer, or pointer to member type).
*/
{
  register a_boolean is_constant_false = FALSE;
  an_expr_node_ptr   node;

  if (is_expression_operand(operand)) {
    /* Check if the expression is a false constant. */
    node = operand->variant.expression;
    if (is_constant_node(node) && is_false_constant(node->variant.constant)) {
      is_constant_false = TRUE;
    }  /* if */
  } else if (is_constant_operand(operand)) {
    /* Check the constant in the operand itself. */
    is_constant_false = is_false_constant(&operand->variant.constant);
  }  /* if */
  return is_constant_false;
}  /* op_is_false_constant */


static a_boolean valid_node_if_subscript(an_expr_node_ptr node,
                                         a_boolean        *just_past_end)
/*
Check the given node to see if it represents a subscript operation with a
constant subscript.  If so, check the subscript and return FALSE if it's not
valid.  Return *just_past_end TRUE if the subscript is just past the end
of the array (which is legal when the address is used, but not when the
value is used).
*/
{
  a_boolean        valid = TRUE;
  an_expr_node_ptr lhs_node, rhs_node;
  a_type_ptr       ptr_type, underlying_type, array_type, element_type;
  a_type_ptr       ptr_element_type;
  a_constant_ptr   rhs_con, con;
  a_targ_size_t    num_elements;
  int              cmp;

  *just_past_end = FALSE;
  if (is_operation_node(node) &&
      (node->variant.operation.kind == (an_expr_operator_kind)eok_padd ||
       node->variant.operation.kind == (an_expr_operator_kind)eok_padd_subsc)){
    /* The node is a pointer addition or subscript operation. */
    lhs_node = node->variant.operation.operands;
    rhs_node = lhs_node->next;
    if (is_constant_node(rhs_node)) {
      rhs_con = rhs_node->variant.constant;
      if (rhs_con->kind == (a_constant_repr_kind)ck_integer) {
        /* The subscript is an integer constant.  The left-hand side should
           have type "pointer to x" (the test rules out error cases). */
        ptr_type = lhs_node->type;
        if (is_pointer_type(ptr_type)) {
          /* See if we can find the array type "array of x" underneath
             that. */
          /* Drop one or more casts. */
          while (is_operation_node(lhs_node) &&
                 lhs_node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast) {
            lhs_node = lhs_node->variant.operation.operands;
          }  /* if */
          underlying_type = NULL;
          if (is_pointer_type(lhs_node->type)) {
            if (is_constant_node(lhs_node)) {
              /* The left side is a constant.  See if it is the address of
                 a variable implicitly cast to another type, in which case
                 we have the underlying type. */
              con = lhs_node->variant.constant;
              if (con->kind == (a_constant_repr_kind)ck_address &&
                  con->variant.address.kind ==
                                          (an_address_base_kind)abk_variable &&
                  con->implicit_cast && con->variant.address.offset == 0) {
                underlying_type = con->variant.address.variant.variable->type;
              }  /* if */
            } else if (is_variable_address_node(lhs_node)) {
              /* Address of a variable. */
              underlying_type = lhs_node->variant.variable->type;
            } else if (is_operation_node(lhs_node) &&
                       lhs_node->variant.operation.kind ==
                                            (an_expr_operator_kind)eok_field) {
              /* Field selection. */
              underlying_type = type_pointed_to(lhs_node->type);
            }  /* if */
          }  /* if */
          if (underlying_type != NULL) {
            /* See if the underlying_type is an array type. */
            /* Note that is_array_type returns TRUE for incomplete array
               types.  We can only check the subscript if the array type
               is complete. */
            if (is_array_type(underlying_type) &&
                !is_incomplete_type(underlying_type)) {
              array_type = skip_typerefs(underlying_type);
              /* See if the element type of the array type matches the
                 type pointed to by ptr_type. */
              ptr_element_type = type_pointed_to(ptr_type);
              ptr_element_type = skip_typerefs(ptr_element_type);
              element_type = array_element_type(array_type);
              element_type = skip_typerefs(element_type);
              if (identical_types(ptr_element_type, element_type)) {
                /* Everything's as we want it.  Check the subscript. */
                if (sign_of_integer_constant(rhs_con) < 0) {
                  /* Negative subscript. */
                  valid = FALSE;
                } else {
                  check_assertion(!array_type->
                                       variant.array.is_variable_size_array);
                  num_elements = array_type->
                                     variant.array.variant.number_of_elements;
                  /* Do not check subscripts on arrays dimensioned as having
                     size 1, since that's probably a clue that the programmer
                     is cheating. */
                  if (num_elements > 1) {
                    cmp = cmpulit_integer_constant(rhs_con,
                                                  (unsigned long)num_elements);
                    valid = (cmp <= 0);  /* Subscript <= number of elements */
                    *just_past_end = (cmp == 0);
                                         /* Subscript == number of elements */
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return valid;
}  /* valid_node_if_subscript */


an_expr_operator_kind which_binary_operator(a_token_kind token,
					    a_type_ptr   type)
/*
Return a binary expression operator based on the token and type.  If the
type is an error type, return eok_error.
*/
{
  an_expr_operator_kind op;

  switch (skip_typerefs(type)->kind) {
    case tk_integer:
      switch (token) {
	case tok_plus:
	  op = (an_expr_operator_kind)eok_iadd;
	  break;
	case tok_minus:
	  op = (an_expr_operator_kind)eok_isubtract;
	  break;
        case tok_star:
          op = (an_expr_operator_kind)eok_imultiply;
          break;
        case tok_divide:
          op = (an_expr_operator_kind)eok_idivide;
          break;
        case tok_remainder:
          op = (an_expr_operator_kind)eok_remainder;
          break;
	case tok_shift_right:
	  op = (an_expr_operator_kind)eok_shiftr;
	  break;
	case tok_shift_left:
	  op = (an_expr_operator_kind)eok_shiftl;
	  break;
	case tok_lt:
	  op = (an_expr_operator_kind)eok_ilt;
	  break;
	case tok_gt:
	  op = (an_expr_operator_kind)eok_igt;
	  break;
	case tok_le:
	  op = (an_expr_operator_kind)eok_ile;
	  break;
	case tok_ge:
	  op = (an_expr_operator_kind)eok_ige;
	  break;
	case tok_eq:
	  op = (an_expr_operator_kind)eok_ieq;
	  break;
	case tok_ne:
	  op = (an_expr_operator_kind)eok_ine;
	  break;
        case tok_ampersand:
          op = (an_expr_operator_kind)eok_and;
          break;
        case tok_excl_or:
          op = (an_expr_operator_kind)eok_xor;
          break;
        case tok_or:
          op = (an_expr_operator_kind)eok_or;
          break;
        case tok_and_and:
          op = (an_expr_operator_kind)eok_land;
          break;
        case tok_or_or:
          op = (an_expr_operator_kind)eok_lor;
          break;
	case tok_assign:
	  op = (an_expr_operator_kind)eok_iassign;
	  break;
	case tok_times_assign:
	  op = (an_expr_operator_kind)eok_imultiply_assign;
	  break;
	case tok_divide_assign:
	  op = (an_expr_operator_kind)eok_idivide_assign;
	  break;
	case tok_remainder_assign:
	  op = (an_expr_operator_kind)eok_remainder_assign;
	  break;
	case tok_plus_assign:
	  op = (an_expr_operator_kind)eok_iadd_assign;
	  break;
	case tok_minus_assign:
	  op = (an_expr_operator_kind)eok_isubtract_assign;
	  break;
	case tok_shift_left_assign:
	  op = (an_expr_operator_kind)eok_shiftl_assign;
	  break;
	case tok_shift_right_assign:
	  op = (an_expr_operator_kind)eok_shiftr_assign;
	  break;
	case tok_and_assign:
	  op = (an_expr_operator_kind)eok_and_assign;
	  break;
	case tok_excl_or_assign:
	  op = (an_expr_operator_kind)eok_xor_assign;
	  break;
	case tok_or_assign:
	  op = (an_expr_operator_kind)eok_or_assign;
	  break;
#if CHECKING
        default:
          internal_error("which_binary_operator: bad int operator");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_float:
      switch (token) {
	case tok_plus:
	  op = (an_expr_operator_kind)eok_fadd;
	  break;
	case tok_minus:
	  op = (an_expr_operator_kind)eok_fsubtract;
	  break;
        case tok_star:
          op = (an_expr_operator_kind)eok_fmultiply;
          break;
        case tok_divide:
          op = (an_expr_operator_kind)eok_fdivide;
          break;
	case tok_lt:
	  op = (an_expr_operator_kind)eok_flt;
	  break;
	case tok_gt:
	  op = (an_expr_operator_kind)eok_fgt;
	  break;
	case tok_le:
	  op = (an_expr_operator_kind)eok_fle;
	  break;
	case tok_ge:
	  op = (an_expr_operator_kind)eok_fge;
	  break;
	case tok_eq:
	  op = (an_expr_operator_kind)eok_feq;
	  break;
	case tok_ne:
	  op = (an_expr_operator_kind)eok_fne;
	  break;
	case tok_assign:
	  op = (an_expr_operator_kind)eok_fassign;
	  break;
	case tok_times_assign:
	  op = (an_expr_operator_kind)eok_fmultiply_assign;
	  break;
	case tok_divide_assign:
	  op = (an_expr_operator_kind)eok_fdivide_assign;
	  break;
	case tok_plus_assign:
	  op = (an_expr_operator_kind)eok_fadd_assign;
	  break;
	case tok_minus_assign:
	  op = (an_expr_operator_kind)eok_fsubtract_assign;
	  break;
#if CHECKING
        default:
	  internal_error("which_binary_operator: bad float operator");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_pointer:
      switch (token) {
	case tok_lt:
	  op = (an_expr_operator_kind)eok_plt;
	  break;
	case tok_gt:
	  op = (an_expr_operator_kind)eok_pgt;
	  break;
	case tok_le:
	  op = (an_expr_operator_kind)eok_ple;
	  break;
	case tok_ge:
	  op = (an_expr_operator_kind)eok_pge;
	  break;
	case tok_eq:
	  op = (an_expr_operator_kind)eok_peq;
	  break;
	case tok_ne:
	  op = (an_expr_operator_kind)eok_pne;
	  break;
	case tok_assign:
	  op = (an_expr_operator_kind)eok_passign;
	  break;
	case tok_plus:
	  op = (an_expr_operator_kind)eok_padd;
	  break;
	case tok_minus:
	  op = (an_expr_operator_kind)eok_psubtract;
	  break;
	case tok_plus_assign:
	  op = (an_expr_operator_kind)eok_padd_assign;
	  break;
	case tok_minus_assign:
	  op = (an_expr_operator_kind)eok_psubtract_assign;
	  break;
#if CHECKING
        default:
	  internal_error("which_binary_operator: bad ptr operator");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_ptr_to_member:
      switch (token) {
	case tok_eq:
	  op = (an_expr_operator_kind)eok_pmeq;
	  break;
	case tok_ne:
	  op = (an_expr_operator_kind)eok_pmne;
	  break;
	case tok_assign:
	  op = (an_expr_operator_kind)eok_pmassign;
	  break;
#if CHECKING
        default:
	  internal_error("which_binary_operator: bad ptr-to-member operator");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_class:
    case tk_struct:
    case tk_union:
      switch (token) {
	case tok_assign:
	  op = (an_expr_operator_kind)eok_sassign;
	  break;
#if CHECKING
        default:
	  internal_error("which_binary_operator: bad struct operator");
#endif /* CHECKING */
      }  /* switch */
      break;
    case tk_error:
      op = (an_expr_operator_kind)eok_error;
      break;
#if CHECKING
    default:
      internal_error("which_binary_operator: bad type");
#endif /* CHECKING */
  }  /* switch */
  return op;
}  /* which_binary_operator */


void do_binary_operation(an_expr_operator_kind op,
			 an_operand            *operand_1,
			 an_operand            *operand_2,
			 a_type_ptr            result_type,
			 an_operand            *result,
			 a_source_position     *operator_position)
/*
Perform a binary operation on 2 operands yielding a result.  operator
indicates the operation, and operand_1 and operand_2 are the operands.
result_type indicates the type of result; the result is placed in
*result.  If the operands are constant, the operation will be folded
if possible.
*/
{
  a_boolean did_not_fold, template_constant;
  a_boolean just_past_end;
  a_boolean try_folding;

  if (is_error_operand(operand_1) || is_error_operand(operand_2)) {
    make_error_operand(result);
  } else {
    /* Some addressing operations should not be folded in some nonconstant
       contexts, because the expression form provides more explicit
       addressing information (which is useful for aliasing analysis). */
    /* Field-selection operations don't come through this routine, so there's
       no point in checking them. */
    if (op == (an_expr_operator_kind)eok_padd_subsc ||
        op == (an_expr_operator_kind)eok_padd) {
      /* Try folding only if the current expression is a constant
         expression. */
      try_folding = expr_stack->fold_constant_addr_exprs;
    } else {
      /* Not an addressing operation (normal case). */
      try_folding = TRUE;
    }  /* if */
    /* Try to fold the operation if both operands are constants and the
       current expression is being evaluated. */
    did_not_fold = TRUE;
    template_constant = FALSE;
    if (try_folding && curr_expr_is_evaluated() &&
        is_constant_operand(operand_1) && is_constant_operand(operand_2)) {
      clear_operand((an_operand_kind)ok_constant, result);
      /* If the operator could not be determined (because the operand types
         are incompatible), fold the operation to an error constant. */
      if (op == (an_expr_operator_kind)eok_error) {
        set_error_constant(&result->variant.constant);
        result->type = error_type();
        did_not_fold = FALSE;
      } else {
        result->type = result_type;
        /* In a nonconstant context, reduce any error to a warning
           and leave the operation to be done at runtime. */
        binary_operation(op,
                         &operand_1->variant.constant,
                         &operand_2->variant.constant,
                         result_type, &result->variant.constant,
                         curr_expr_kind_is_const(),
                         &did_not_fold, &template_constant, operator_position);
      }  /* if */
    }  /* if */
    if (did_not_fold) {
      if (template_constant) {
        /* For an expression based on a template parameter, scanned
           during the prototype instantiation, make a ck_template_param
           constant for the result. */
        make_template_param_expr_constant_operand(operand_1, operand_2,
                                                  op, result_type, result);
      } else if (curr_expr_kind_is_const() && curr_expr_is_evaluated()) {
        /* An operation on constants could not be folded.  For example,
           a pointer comparison between pointers that aren't in the
           same object can't be represented as a constant.  In a
           constant expression, that's an error. */
        pos_error(ec_expr_not_constant, operator_position);
        make_error_operand(result);
      } else {
        /* The constant operation was not folded; create an expression
           operand. */
        build_binary_result_operand(operand_1, operand_2, op,
                                    result_type, result);
        /* Check for invalid constant subscripts when the first operand
           is not a constant (as happens when the array is an auto array). */
        if (is_expression_operand(result)) {
          if (!valid_node_if_subscript(result->variant.expression,
                                       &just_past_end)) {
            pos_warning(ec_subscript_out_of_range, &operand_2->position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  result->state = (an_operand_state)os_rvalue;
}  /* do_binary_operation */


void add_reference_indirection(an_operand *result)
/*
*result has a C++ reference type; add an implicit indirection to it.
*result may be an rvalue or an lvalue.
*/
{
  a_type_ptr       result_type;
  an_expr_node_ptr node;
  an_operand_state result_state;
  an_operand       orig_result;

  result_type = result->type;
#if CHECKING
  if (!is_reference_type(result_type)) {
    internal_error("add_reference_indirection: not reference type");
  }  /* if */
#endif /* CHECKING */
  result_state = result->state;
  orig_result = *result;
  /* Change the references to "use". */
  change_some_ref_kinds(result->ref_entries_list, SRK_REFERENCE, SRK_USE);
  node = add_indirection_to_node(make_node_from_operand(result));
  result_type = type_pointed_to(result_type);
  if (is_an_lvalue(result)) {
    /* Make the node have a pointer type instead of a reference type. */
    node->type = make_pointer_type(result_type);
    if (is_function_type(result_type)) {
      /* The thing pointed to is a function, so the result is a function
         designator. */
      result_state = (an_operand_state)os_function_designator;
    }  /* if */
  } else if (is_an_rvalue(result)) {
    /* Drop type qualifiers on an rvalue.  An IL shorthand allows them to
       be dropped without an explicit cast. */
    node->type = result_type = make_unqualified_type(result_type);
  }  /* if */
  make_expression_operand(node, result_type, result);
  result->state = result_state;
  result->came_from_reference = TRUE;
  /* Instantiate the underlying type if it is a template class. */
  check_for_uninstantiated_template_class(result_type);
  /* Restore the original source position, etc.  Note that the reference
     entries are NOT restored, on purpose. */
  restore_operand_details(result, &orig_result);
  result->ref_entries_list = NULL;
}  /* add_reference_indirection */


void make_lvalue_variable_operand(a_variable_ptr  variable,
                                  an_operand      *result,
                                  a_ref_entry_ptr rep)
/*
Make an operand for the address of a variable.  The source position of
the operand is set to pos_curr_token.  rep points to an associated
reference entry, or is NULL if none is needed.
*/
{
  an_expr_node_ptr node;
  a_type_ptr       variable_type = variable->type;

  if (is_void_type(variable_type) && !is_qualified_type(variable_type)) {
    /* If the variable has type void, make an rvalue instead of an lvalue.
       See ANSI C 3.2.2.1.  This helps with
         extern void x;
         x;
         &x;
    */
    make_expression_operand(var_rvalue_expr(variable), variable_type,
                            result);
    copy_source_position(pos_curr_token, result->position);
  } else {
    if (variable->storage_class == (a_storage_class)sc_register ||
        variable->storage_class == (a_storage_class)sc_auto) {
      /* Register variables do not have addresses, and auto variables
         do not have constant addresses, so use a variable-address
         expression instead of a constant.  Note that one can use
         a variable-address expression node on a register variable,
         but only for lvalue address notational convenience.  The actual
         address can never be used. */
      node = var_lvalue_expr(variable);
      make_expression_operand(node, variable_type, result);
    } else {
      /* Normal case; set up an address-of-variable constant. */
      clear_operand((an_operand_kind)ok_constant, result);
      set_variable_address_constant(variable, &result->variant.constant);
      result->type = variable_type;
    }  /* if */
    result->state = (an_operand_state)os_lvalue;
    copy_source_position(pos_curr_token, result->position);
    /* Instantiate the underlying type if it is a template class. */
    check_for_uninstantiated_template_class(variable_type);
    /* Start a list of reference entries related to the operand. */
    result->ref_entries_list = rep;
    /* If the variable has a reference type, add an implicit indirection. */
    if (C_dialect == C_dialect_cplusplus && is_reference_type(variable_type)) {
      add_reference_indirection(result);
    }  /* if */
  }  /* if */
}  /* make_lvalue_variable_operand */


a_constant_ptr var_constant_value(a_variable_ptr var)
/*
If the variable var has a constant initial value, return a pointer to it;
otherwise, return NULL.
*/
{
  a_constant_ptr con_val = NULL;

  /* See if the variable has a known constant value. */
  if (C_dialect == C_dialect_cplusplus &&
      is_const_variable(var) &&
      !is_volatile_qualified_type(var->type)) {
    if (var->init_kind == (an_init_kind)initk_static) {
      /* The variable has a constant initial value. */
      con_val = var->initializer.constant;
    } else if (var->init_kind == (an_init_kind)initk_dynamic) {
      /* The variable is dynamically initialized.  See if the initialization
         is to a constant. */
      if (var->initializer.dynamic->kind ==
                                           (a_dynamic_init_kind)dik_constant) {
        con_val = var->initializer.dynamic->variant.constant;
      }  /* if */
    }  /* if */
    if (con_val != NULL) {
      if (con_val->kind == (a_constant_repr_kind)ck_aggregate) {
        /* An aggregate cannot be considered a value. */
        con_val = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  return con_val;
}  /* var_constant_value */


void make_ptr_to_member_constant_operand(
                                    a_symbol_ptr      member_sym,
                                    a_symbol_ptr      member_proj_sym,
                                    a_source_position *position,
                                    a_boolean         check_protected_access,
                                    a_boolean         is_operand_of_address_of,
                                    an_operand        *result)
/*
Make an operand for a constant representing a C++ pointer to member.
member_sym is the member (not overloaded, not a projection symbol).
member_proj_sym is the same as member_sym, or is the overloaded function
symbol that contains member_sym, or it can be a projection symbol for
either of those.  *position gives the source position to put into the
operand.  If check_protected_access is TRUE and the symbol is a
protected member, do the ARM 11.5 protected member access check.
The name that generated this pointer-to-member constant is the operand
of a "&" operator if is_operand_of_address_of is TRUE.
*/
{
  a_constant    constant;
  a_type_ptr    member_type, member_class;
  a_field_ptr   field;
  a_routine_ptr rout;

  /* The ARM only allows this when the name is preceded by a "&" (5.3).
     We allow it without "&" as an extension -- it's very much common
     practice. */
  if (strict_ansi_mode && !is_operand_of_address_of) {
    pos_diagnostic(strict_ansi_error_severity,
                   ec_nonstd_member_function_address, position);
  }  /* if */
  /* Protected members of a base class can only be accessed through an
     object of a derived class (ARM 11.5).  It is not very clear how
     this should affect address of member processing.  We allow the
     address of a protected member to be taken as a member of the
     derived class but not as a member of the base class.  For example:
       class A { protected: int i; };
       class B : public A { void mf(); };
       void B::mf() {
         int A::* pmi = &A::i;	// error - protected member
         int B::* pmj = &B::i;	// OK
       }
     Cfront does not do this checking, so we omit it in cfront mode.
     Also skip this check if an access control error has already been
     issued for the identifier. */
  if (!cfront_compatibility_mode && check_protected_access) {
    check_protected_member_access(member_sym, position,
                                  member_proj_sym->class_of_which_a_member);
  }  /* if */
  /* No need to instantiate the class; since we have a member of it, it must
     be instantiated already. */
  /* Build the constant. */
  clear_constant(&constant, (a_constant_repr_kind)ck_ptr_to_member);
  if (member_sym->kind == (a_symbol_kind)sk_field) {
    /* Pointer to nonstatic data member. */
    constant.variant.ptr_to_member.is_function_ptr = FALSE;
    constant.variant.ptr_to_member.variant.field = field =
                                                 member_sym->variant.field.ptr;
    member_type = field->type;
  } else {
#if CHECKING
    if (member_sym->kind != (a_symbol_kind)sk_member_function) {
      internal_error("make_ptr_to_member_constant_operand: bad kind");
    }  /* if */
#endif /* CHECKING */
    /* Pointer to nonstatic member function. */
    constant.variant.ptr_to_member.is_function_ptr = TRUE;
    constant.variant.ptr_to_member.variant.routine = rout =
                                               member_sym->variant.routine.ptr;
    member_type = rout->type;
    if (!rout->is_virtual) {
      /* Force the routine to be instantiated or generated. */
      if_evaluating_mark_routine_referenced(rout);
    }  /* if */
  }  /* if */
  /* Note that the class of the pointer is always the class in which
     the member was defined, not any derived class.  See ARM 5.3. */
  member_class = member_sym->class_of_which_a_member;
  constant.type = ptr_to_member_type(member_type, member_class);
  make_constant_operand(&constant, result);
  result->position = *position;
}  /* make_ptr_to_member_constant_operand */


void make_function_designator_operand(a_symbol_ptr      routine_sym,
                                      a_boolean         is_qualified_name,
                                      a_source_position *position,
                                      a_ref_entry_ptr   rep,
                                      an_operand        *result)
/*
Make an operand for a function designator.  routine_sym points to the
routine symbol entry (not overloaded, not a projection symbol).
is_qualified_name is TRUE if the function was named by a qualified name.
The source position of the operand is set to *position.  rep points to an
associated reference entry, or is NULL if none is needed.
*/
{
  a_routine_ptr routine;

#if CHECKING
  if (routine_sym->kind != (a_symbol_kind)sk_routine &&
      routine_sym->kind != (a_symbol_kind)sk_member_function) {
    internal_error("make_function_designator_operand: sym not function");
  }  /* if */
#endif /* CHECKING */
  routine = routine_sym->variant.routine.ptr;
  if (C_dialect == C_dialect_cplusplus &&
      curr_expr_is_potentially_evaluated()) {
    if (routine == il_header.main_routine) {
      /* In C++, "main" cannot be called and cannot have its address
         taken (ARM 3.4). */
      pos_error(ec_bad_use_of_main, position);
    }  /* if */
  }  /* if */
  /* Set up an address-of-function constant. */
  clear_operand((an_operand_kind)ok_constant, result);
  set_routine_address_constant(routine, &result->variant.constant);
  /* The type of the operand is the function type. */
  result->type = routine->type;
  result->state = (an_operand_state)os_function_designator;
  /* Remember whether or not the routine is virtual.  Use of a qualified
     name suppresses the virtual-ness of the function (ARM 10.2). */
  result->virtual_function = routine->is_virtual && !is_qualified_name;
  result->position = *position;
  /* Start a list of reference entries related to the operand. */
  result->ref_entries_list = rep;
  /* If this is a non-virtual call, mark the routine entry as actually
     referenced. */
  if (!result->virtual_function) {
    if_evaluating_mark_routine_referenced(routine);
  }  /* if */
}  /* make_function_designator_operand */


void make_field_operand(a_field_ptr field,
			an_operand  *result)
/*
Allocate an expression node to contain a field reference, link it to the
operand, and set the operand type to the type of the field.
*/
{
  register an_expr_node_ptr node;

  /* Make the expression node first. */
  node = alloc_expr_node((an_expr_node_kind)enk_field);
  node->type = field->type;
  node->variant.field = field;
  /* Make the operand with the node. */
  make_expression_operand(node, node->type, result);
  result->state = (an_operand_state)os_none;
}  /* make_field_operand */


void make_function_call(an_expr_node_ptr  function_node,
                        a_type_ptr        function_type,
                        a_boolean         is_virtual,
                        a_source_position *call_pos,
                        an_operand        *result)
/*
Make an operand for a call of the function indicated by function_node, whose
type is function_type, and which is virtual if is_virtual is TRUE or
a pointer-to-member-function call if the type of function_node is
pointer-to-member-function.  The arguments of the call are already
attached to function_node.  A skip_typerefs need not have been done
on function_type.  *call_pos gives the source position of the call.
*/
{
  an_expr_node_ptr call_node;
  a_type_ptr       return_type;

  /* Make the function call expression node. */
  call_node = func_call_expr(function_node, function_type, is_virtual,
                             curr_expr_is_potentially_evaluated(),
                             (a_boolean)expr_stack->
                                                 in_return_by_cctor_expression,
                             call_pos);
  /* Make an operand for the overall call (etc.). */
  make_expression_operand(call_node, call_node->type, result);
  result->position = *call_pos;
  /* A function call returning a reference is an lvalue. */
  function_type = skip_typerefs(function_type);
  return_type = skip_typerefs(function_type->variant.routine.return_type);
  if (is_reference_type(return_type)) {
    conv_object_pointer_to_lvalue(result);
    result->came_from_reference = TRUE;
  }  /* if */
}  /* make_function_call */


void assemble_function_call(an_operand       *function_operand,
                            an_operand       *bound_function_selector,
                            an_expr_node_ptr argument_list,
                            an_operand       *result)
/*
Assemble a function call from the various pieces.  *function_operand
identifies the function to be called.  If a selector object is needed,
it is provided by *bound_function_selector.  argument_list points to the
(explicit) argument list.  An operand for the overall call is constructed
in *result.
*/
{
  an_expr_node_ptr function_node;
  an_expr_node_ptr implicit_this_argument;
  a_type_ptr       function_type;

  if (is_error_operand(function_operand)) {
    /* If the function operand is an error, the arguments cannot be linked to
       it; just make an error operand for the result. */
    make_error_operand(result);
  } else {
    /* Make an expression node for the function, link it to the argument
       list, then build a call node that points to the function/arguments
       list. */
    if (function_operand->bound_function) {
      /* Bound function.  bound_function_selector indicates the object. */
      implicit_this_argument = make_node_from_operand(bound_function_selector);
      /* Pass a "this" pointer as the first argument. */
      implicit_this_argument->next = argument_list;
      argument_list = implicit_this_argument;
    }  /* if */
    /* Make the function address node.  This might have type pointer-to-
       member-function in a case like (p->*pmf)(). */
    function_node = make_node_from_operand(function_operand);
    /* Link the function address node to the argument list. */
    function_node->next = argument_list;
    if (is_ptr_to_member_type(function_node->type)) {
      /* Call using a pointer-to-member-function. */
      function_type = pm_member_type(function_node->type);
    } else {
      /* Normal call using a pointer to function. */
      function_type = type_pointed_to(function_node->type);
    }  /* if */
    /* Make the call node. */
    make_function_call(function_node, function_type,
                       (a_boolean)function_operand->virtual_function, 
                       &function_operand->position, result);
  }  /* if */
  result->position = function_operand->position;
}  /* assemble_function_call */


void using_lvalue(an_operand *operand)
/*
Called when an lvalue is going to be used: the left side of assignment
operators, the operand of increment/decrement operators, the left
operand of the "." operator, and when an lvalue is converted to an
rvalue.  This routine checks that the address indicated by the lvalue
is valid; specifically, it checks for actually using the element just
past the end of an array.  It's okay to take the address of that
element, but it's not okay to actually reference it.
*/
{
  a_boolean just_past_end = FALSE;
  
  if (is_constant_operand(operand)) {
    /* Lvalue address is given by a constant.  If the constant is an
       address constant, check that the offset is not just past the end
       of the object. */
    if (operand->variant.constant.kind == (a_constant_repr_kind)ck_address) {
      (void)valid_address_constant(&operand->variant.constant, &just_past_end);
    }  /* if */
  } else if (is_expression_operand(operand)) {
    /* Lvalue address given by an expression.  Check for a subscript just past
       the end of an array. */
    (void)valid_node_if_subscript(operand->variant.expression, &just_past_end);
  }  /* if */
  if (just_past_end) {
    pos_warning(ec_subscript_out_of_range, &operand->position);
  }  /* if */
}  /* using_lvalue */


static a_boolean is_bit_field_expr(an_expr_node_ptr node,
                                   a_boolean        is_lvalue)
/*
Return TRUE if the expression "node" is a bit field expression.  The expression
is an lvalue if is_lvalue is TRUE.
*/
{
  a_boolean             is_bit_field = FALSE;
  an_expr_operator_kind op;
  an_expr_node_ptr      expr1, expr2, expr3;

  op = node->variant.operation.kind;
  if (is_lvalue && op == (an_expr_operator_kind)eok_bit_field) {
    is_bit_field = TRUE;
  } else if (!is_lvalue &&
             (op == (an_expr_operator_kind)eok_value_bit_field ||
              op == (an_expr_operator_kind)eok_extract_bit_field)) {
    is_bit_field = TRUE;
  } else if (is_lvalue && op == (an_expr_operator_kind)eok_question) {
    /* An lvalue-returning "?" operator; check its second and third
       operands. */
    expr1 = node->variant.operation.operands;
    expr2 = expr1->next;
    expr3 = expr2->next;
    if (is_bit_field_expr(expr2, is_lvalue) ||
        is_bit_field_expr(expr3, is_lvalue)) {
      is_bit_field = TRUE;
    }  /* if */
  } else if (is_lvalue && op == (an_expr_operator_kind)eok_comma) {
    /* An lvalue-returning "," operator; check its second operand. */
    expr1 = node->variant.operation.operands;
    expr2 = expr1->next;
    if (is_bit_field_expr(expr2, is_lvalue)) {
      is_bit_field = TRUE;
    }  /* if */
  }  /* if */
  return is_bit_field;
}  /* is_bit_field_expr */


a_boolean is_bit_field_operand(an_operand *operand)
/*
Return TRUE if the operand is a bit field.
*/
{
  a_boolean is_bit_field = FALSE;

  if (is_expression_operand(operand)) {
    if (is_bit_field_expr(operand->variant.expression,
                          is_an_lvalue(operand))) {
      is_bit_field = TRUE;
    }  /* if */
  }  /* if */
  return is_bit_field;
}  /* is_bit_field_operand */

#if ADDR_OF_BIT_FIELD_ALLOWED

a_boolean is_bit_field_whose_address_can_be_taken(a_field_ptr field,
                                                  a_type_ptr  *ptr_type)
/*
Return TRUE if the indicated field (a bit field) is one whose address
can be taken (as an extension).  If returning TRUE, also return the type
of the pointer to that bit field, in *ptr_type.
*/
{
  a_boolean        addr_can_be_taken = FALSE;
  a_targ_size_t    field_size, field_offset, type_size;
  a_targ_alignment type_alignment, struct_alignment;
  an_integer_kind  int_kind;
  a_type_ptr       int_type;

  /* In strict ANSI mode, don't allow this. */
  if (!strict_ansi_mode) {
    /* See if the bit field is an even number of bytes long. */
    field_size = field->bit_size;
    if (field_size % TARG_CHAR_BIT == 0) {
      field_size /= TARG_CHAR_BIT;
      /* See if the bit field is at an even byte offset. */
      field_offset = field->bit_offset;
      if (field_offset % TARG_CHAR_BIT == 0) {
        field_offset /= TARG_CHAR_BIT;
        /* Get the overall alignment of the structure of which this field is
           a member. */
        struct_alignment =
                      field->source_corresp.class_of_which_a_member->alignment;
        /* Look for an integral type that matches the bit field size. */
        for (int_kind = (an_integer_kind)0;
             (int)int_kind < (int)ik_last;
             int_kind = (an_integer_kind)((int)int_kind + 1)) {
          /* The signedness must match. */
          if (int_kind_is_signed[(int)int_kind] ==
                                                  field->bit_field_is_signed) {
            /* The size and alignment must match. */
            get_integer_size_and_alignment(int_kind, &type_size,
                                           &type_alignment);
            if (type_size == field_size &&
                type_alignment <= struct_alignment &&
                field_offset % type_alignment == 0) {
              addr_can_be_taken = TRUE;
              int_type = integer_type(int_kind);
              *ptr_type = make_pointer_type(int_type);
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
  return addr_can_be_taken;
}  /* is_bit_field_whose_address_can_be_taken */

#endif /* ADDR_OF_BIT_FIELD_ALLOWED */
#if ADDR_OF_BIT_FIELD_ALLOWED

static a_boolean take_address_of_bit_field(an_operand *operand)
/*
*operand is a bit-field operand lvalue whose address is being taken.
See if it is okay to take the address of the bit field (as an extension),
and if so, change *operand to indicate the address.  If not, return FALSE.
*/
{
  a_boolean        address_taken = FALSE;
  an_expr_node_ptr node;
  a_field_ptr      field;
  a_type_ptr       ptr_type;

  check_assertion(is_expression_operand(operand));
  node = operand->variant.expression;
  /* Only handle the simplest case, not something like "&(i ? x.a : x.b)".
     A case like that could be handled, but it's tricky, since the
     subexpressions could have different pointer types. */
  if (is_operation_node(node) &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_bit_field) {
    field = node->variant.operation.operands->next->variant.field;
    if (is_bit_field_whose_address_can_be_taken(field, &ptr_type)) {
      /* The bit field is one whose size and alignment are such that its
         address can be taken. */
      address_taken = TRUE;
      pos_warning(ec_address_of_bit_field, &operand->position);
      /* Change the field selection to a normal field selection. */
      node->variant.operation.kind = (an_expr_operator_kind)eok_field;
      /* Cast the field selection to the right pointer type. */
      cast_node(&node, ptr_type, /*is_implicit_cast=*/TRUE,
                &operand->position);
      /* Make an rvalue operand for the address. */
      make_expression_operand(node, ptr_type, operand);
    }  /* if */
  }  /* if */
  return address_taken;
}  /* take_address_of_bit_field */

#endif /* ADDR_OF_BIT_FIELD_ALLOWED */

void take_address_of_lvalue(an_operand *operand)
/*
Change operand (an lvalue) to an rvalue that is a pointer to the
object.  This is the function of the "&" operator.  Check that the
operand isn't a register variable or a bit field, and set the
address_taken flag.
*/
{
  an_operand orig_operand;

  if (is_error_operand(operand)) {
    /* Leave an error operand alone. */
  } else {
    orig_operand = *operand;
#if CHECKING
    if (!is_an_lvalue(operand)) {
      internal_error("take_address_of_lvalue: not an lvalue");
    }  /* if */
#endif /* CHECKING */
    /* Note that by and large this "transformation" consists of changing the
       kind of the operand to "rvalue," since the value stays the same before
       and after.  However, there are some error checks to be done. */
    /* Check for taking the address of a bit field. */
    if (is_bit_field_operand(operand)) {
#if ADDR_OF_BIT_FIELD_ALLOWED
      /* As an extension, the address of a bit field can be taken if it has
         the same size and alignment as one of the integral types. */
      if (!take_address_of_bit_field(operand)) {
        error_in_operand(ec_address_of_bit_field, operand);
      }  /* if */
#else /* !ADDR_OF_BIT_FIELD_ALLOWED */
      error_in_operand(ec_address_of_bit_field, operand);
#endif /* ADDR_OF_BIT_FIELD_ALLOWED */
    } else {
      /* Not a bit field reference. */
      /* The operand becomes an rvalue. */
      operand->state = (an_operand_state)os_rvalue;
      operand->came_from_reference = FALSE;
      operand->type = make_pointer_type(operand->type);
      /* Change the kind in the reference entries to address-taken. */
      /* This will check for taking the address of a register variable. */
      change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
    }  /* if */
    /* Restore the original source position, etc. */
    restore_operand_details(operand, &orig_operand);
  }  /* if */
}  /* take_address_of_lvalue */


void modifying_lvalue(an_operand *operand,
                      a_boolean  value_used)
/*
The entity indicated by operand (an lvalue) is being modified (e.g.,
assigned to, incremented, ...).  If value_used, the value of the lvalue is
used before it is modified.
*/
{
#if CHECKING
  if (!is_an_lvalue(operand) && !is_error_operand(operand)) {
    internal_error("modifying_lvalue: not an lvalue");
  }  /* if */
#endif /* CHECKING */
  using_lvalue(operand);
  /* Change the kind in the reference entries to modification or
     use/modification. */
  change_ref_kinds(operand->ref_entries_list,
                   value_used ?
                          (SRK_USE | SRK_MODIFICATION) : SRK_MODIFICATION);
}  /* modifying_lvalue */


void conv_object_pointer_to_lvalue(an_operand *operand)
/*
operand is an rvalue pointer.  Change it to an lvalue for the object pointed
to (or a function designator).
*/
{
  /* Leave an error operand alone. */
  if (!is_error_operand(operand)) {
    operand->type = type_pointed_to(operand->type);
    if (is_function_type(operand->type)) {
      operand->state = (an_operand_state)os_function_designator;
    } else {
      operand->state = (an_operand_state)os_lvalue;
    }  /* if */
  }  /* if */
}  /* conv_object_pointer_to_lvalue */


static void conv_class_rvalue_expr_to_object_pointer(
                                              an_expr_node_ptr *p_node,
                                              a_boolean        *converted,
                                              a_boolean        see_if_possible)
/*
*p_node is an expression tree for a class rvalue.  If possible, rewrite it
as an object pointer for the class object, and set *p_node to the new
pointer and *converted to TRUE.  If not possible, return *converted FALSE
and *p_node unchanged.  If see_if_possible is TRUE, just see if the
rewriting is possible, and set *converted accordingly; do not change
the expression.
*/
{
  an_expr_node_ptr      node = *p_node, op1, op2, op3;
  an_expr_operator_kind op;
  a_boolean             possible;
  a_boolean             op1_possible, op2_possible, op3_possible;
  a_type_ptr            orig_type = node->type;

  possible = FALSE;
  if (is_variable_node(node)) {
    /* The value of a variable.  Change it to the address of the variable. */
    possible = TRUE;
    if (!see_if_possible) {
      node->kind = (an_expr_node_kind)enk_variable_address;
    }  /* if */
  } else if (node->kind == (an_expr_node_kind)enk_temp_init &&
             !node->variant.init.result_is_addr) {
    /* A temporary initialization indicating the value of a temporary.
       Change it to the address of the temporary. */
    possible = TRUE;
    if (!see_if_possible) {
      node->variant.init.result_is_addr = TRUE;
    }  /* if */
  } else if (is_operation_node(node)) {
    /* An operator node. */
    op = node->variant.operation.kind;
    if (op == (an_expr_operator_kind)eok_indirect) {
      /* The top operator is an indirection, so we can just remove it. */
      possible = TRUE;
      if (!see_if_possible) {
        node = node->variant.operation.operands;
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_subscript) {
      /* "[]" operator -- transform to pointer addition. */
      possible = TRUE;
      if (!see_if_possible) {
        node->variant.operation.kind = (an_expr_operator_kind)eok_padd_subsc;
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* "?" operator -- transform each branch independently to an address. */
      op1 = node->variant.operation.operands;
      op2 = op1->next;
      op3 = op2->next;
      /* See if both branches can be rewritten. */
      conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                               /*see_if_possible=*/TRUE);
      conv_class_rvalue_expr_to_object_pointer(&op3, &op3_possible,
                                               /*see_if_possible=*/TRUE);
      if (op2_possible && op3_possible) {
        /* Both branches can be rewritten, so rewrite the whole expression. */
        possible = TRUE;
        if (!see_if_possible) {
          conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                                   /*see_if_possible=*/FALSE);
          conv_class_rvalue_expr_to_object_pointer(&op3, &op3_possible,
                                                   /*see_if_possible=*/FALSE);
          op1->next = op2;
          op2->next = op3;
        }  /* if */
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_comma) {
      /* "," operator -- try to transform the second operand to an lvalue. */
      op1 = node->variant.operation.operands;
      op2 = op1->next;
      conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                               /*see_if_possible=*/TRUE);
      if (op2_possible) {
        possible = TRUE;
        if (!see_if_possible) {
          conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                                   /*see_if_possible=*/FALSE);
          op1->next = op2;
        }  /* if */
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_value_field) {
      /* Selection of a field from an rvalue.  Try to find an lvalue in
         the struct rvalue, and if one can be found rewrite the operation
         as a normal field selection. */
      op1 = node->variant.operation.operands;
      /* See if the operand can be rewritten. */
      conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                               /*see_if_possible=*/TRUE);
      if (op1_possible) {
        possible = TRUE;
        if (!see_if_possible) {
          conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                                   /*see_if_possible=*/FALSE);
          node->variant.operation.kind = (an_expr_operator_kind)eok_field;
        }  /* if */
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_sassign &&
             !node->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
      /* An assignment operation that returns an rvalue.  It can be optimized
         by changing it to the lvalue case. */
      /* This case is here for the sake of completeness.  It's probably not
         needed. */
      possible = TRUE;
      if (!see_if_possible) {
        node->variant.operation.returns_lvalue_instead_of_usual_rvalue = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* If the node was transformed, its type is now a pointer to the type it
     had previously. */
  if (!see_if_possible && possible) {
    node->type = make_pointer_type(orig_type);
  }  /* if */
  *p_node = node;
  *converted = possible;
}  /* conv_class_rvalue_expr_to_object_pointer */


void conv_class_operand_to_object_pointer(an_operand *operand)
/*
Convert a class operand for an object into an operand for a pointer to the
object.  The operand may be either an lvalue or an rvalue; in the rvalue
case, a temporary is created and initialized with the rvalue, and the
address of the temporary is returned.  This routine is only used in C++ mode.
*/
{
  an_operand        orig_operand;
  a_boolean         optimized_case;
  an_expr_node_ptr  node;

  orig_operand = *operand;
  do_operand_transformations(operand,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
  if (is_error_operand(operand)) {
    /* Error operand -- leave alone. */
#if CHECKING
  } else if (!is_class_struct_union_type(operand->type)) {
    internal_error("conv_class_operand_to_object_pointer: not a class");
#endif /* CHECKING */
  } else if (is_an_lvalue(operand)) {
    /* The operand is an lvalue.  This is the easy case, since the lvalue
       is already an address. */
    take_address_of_lvalue(operand);
  } else if (is_an_rvalue(operand)) {
    /* The operand is an rvalue.  In general, we will have to copy the
       rvalue to a temporary and use the address of the temporary.  However,
       there are some cases that can be optimized. */
    optimized_case = FALSE;
    if (is_expression_operand(operand)) {
      node = operand->variant.expression;
      /* Class rvalue.  Change it to an object pointer if possible. */
      conv_class_rvalue_expr_to_object_pointer(&node, &optimized_case,
                                               /*see_if_possible=*/FALSE);
      if (optimized_case) {
        /* The expression has been rewritten as an object pointer. */
        make_expression_operand(node, node->type, operand);
      } else {
        /* Couldn't convert to an object pointer.  The rvalue will have to be
           copied to a temporary, and the temporary address used. */
#if CHECKING
        /* Avoid recursion loops if the class does not allow bitwise copy.
           The conversion to an object pointer really must succeed (i.e.,
           it's not merely an optimization) if a "real" copy constructor
           would have to be used, since in that case we would need the
           address of this rvalue to be able to call the copy constructor. */
        { a_class_symbol_supplement_ptr cssp =
                                    symbol_supplement_for_class(operand->type);
          if (!cssp->construction_by_bitwise_copy_allowed) {
#if DEBUG
            db_expression(node);
#endif /* DEBUG */
            internal_error(
              "conv_class_operand_to_object_pointer: couldn't convert to ptr");
          }  /* if */
        }
#endif /* CHECKING */
      }  /* if */
    }  /* if */
    if (!optimized_case) {
      /* Create a temporary, copy the rvalue into the temporary, and return
         the address of the temporary. */
      temp_init_from_operand(operand);
    }  /* if */
#if CHECKING
  } else {
    internal_error("conv_class_operand_to_object_pointer: unexpected state");
#endif /* CHECKING */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* conv_class_operand_to_object_pointer */


a_constant_ptr value_of_constant_var_lvalue_expr(an_expr_node_ptr node)
/*
node is an expression for the address of an lvalue.  If it is an lvalue
for a constant-valued variable, return a pointer to the constant that is
the variable's value.  Otherwise, return NULL.
*/
{
  a_constant_ptr con_var_value = NULL;

  if (is_constant_node(node)) {
    a_constant_ptr con = node->variant.constant;
    if (con_is_exact_addr_of_variable(con)) {
      /* There is an underlying variable.  See if it has a constant value
         known at compile time. */
      con_var_value= var_constant_value(con->variant.address.variant.variable);
    }  /* if */
  } else if (is_variable_address_node(node)) {
    /* The lvalue address is given by an enk_variable_address node.  See
       if the variable is constant-valued. */
    con_var_value = var_constant_value(node->variant.variable);
  }  /* if */
  return con_var_value;
}  /* value_of_constant_var_lvalue_expr */


static an_expr_node_ptr conv_lvalue_expr_to_rvalue(
                                               an_expr_node_ptr node,
                                               a_boolean        *constant_case,
                                               a_constant_ptr   *con_value)
/*
mode is an expression that is the address for an lvalue.  Create an
expression for the corresponding rvalue, and return a pointer to it.
If the difference between the two is only that a constant variable was
replaced by its value, return *constant_case TRUE.  If con_value is non-NULL
and *is_constant is returned TRUE and the entire expression has a constant
value because it was a constant variable that was replaced by its value,
return a pointer to the constant value in *con_value, do not build an
updated expression tree, and return NULL.  Otherwise, if con_value is
non-NULL return *con_value == NULL.
*/
{
  a_boolean             optimized_case = FALSE;
  an_expr_operator_kind op;
  an_expr_node_ptr      op1, op2, op3;
  a_type_ptr            orig_type = node->type;
  a_boolean             constant_case2, constant_case3;
  a_constant_ptr        con_var_value = NULL;

  *constant_case = FALSE;
  if (con_value != NULL) *con_value = NULL;
  if (C_dialect == C_dialect_cplusplus) {
    /* Look for constant-valued variables in C++. */
    con_var_value = value_of_constant_var_lvalue_expr(node);
  }  /* if */
  if (con_var_value != NULL) {
    /* The lvalue address is the address of a constant-valued
       variable.  Substitute the constant value. */
    optimized_case = TRUE;
    *constant_case = TRUE;
    if (con_value != NULL) {
      /* The caller wants the constant instead of an expression node for
         the constant. */
      *con_value = con_var_value;
      node = NULL;
    } else {
      /* The caller wants an expression node for the constant. */
      node = alloc_node_for_constant(con_var_value);
    }  /* if */
  } else if (is_variable_address_node(node)) {
    /* A variable address node.  Change to the value of the variable. */
    optimized_case = TRUE;
    node->kind = (an_expr_node_kind)enk_variable;
  } else if (node->kind == (an_expr_node_kind)enk_temp_init &&
             node->variant.init.result_is_addr) {
    /* enk_temp_init node.  Change from "address of temporary" to "value
       of temporary". */
    optimized_case = TRUE;
    node->variant.init.result_is_addr = FALSE;
  } else if (is_operation_node(node)) {
    /* An operation node. */
    op = node->variant.operation.kind;
    op1 = node->variant.operation.operands;
    if (op == (an_expr_operator_kind)eok_padd ||
        op == (an_expr_operator_kind)eok_padd_subsc) {
      /* A pointer addition; change to a subscripting operation. */
      optimized_case = TRUE;
      node->variant.operation.kind = (an_expr_operator_kind)eok_subscript;
    } else if (op == (an_expr_operator_kind)eok_bit_field) {
      /* The value is the "address" of a bit-field.  Therefore, the
         indirect version is an extract of the bit-field. */
      optimized_case = TRUE;
      node->variant.operation.kind =
                                  (an_expr_operator_kind)eok_extract_bit_field;
    } else if (node->variant.operation.returns_lvalue_instead_of_usual_rvalue){
      /* Operation that returns an lvalue where the C case would return
         an rvalue. */
      if (op == (an_expr_operator_kind)eok_question) {
        /* "?" operator.  Convert each branch to an rvalue.  This is
           particularly useful for a case like
             &(i ? j : k)
           (only valid in C++). */
        optimized_case = TRUE;
        op2 = op1->next;
        op3 = op2->next;
        op1->next = op2 = conv_lvalue_expr_to_rvalue(op2, &constant_case2,
                                                     (a_constant_ptr *)NULL);
        op2->next = conv_lvalue_expr_to_rvalue(op3, &constant_case3,
                                               (a_constant_ptr *)NULL);
        *constant_case = constant_case2 && constant_case3;
      } else if (op == (an_expr_operator_kind)eok_comma) {
        /* Comma operator.  Apply the transformation to the second operand
           of the ",".  This is useful for a case like
             (p = f(x), *p)
        */
        optimized_case = TRUE;
        op2 = op1->next;
        op1->next = conv_lvalue_expr_to_rvalue(op2, &constant_case2,
                                               (a_constant_ptr *)NULL);
        *constant_case = constant_case2;
      } else {
        /* The operation is an assignment or prefix ++/-- that returns an
           lvalue.  Change it to one that returns an rvalue. */
        optimized_case = TRUE;
      }  /* if */
      node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
    }  /* if */
  }  /* if */
  /* If a constant is being returned instead of an updated expression tree,
     node is NULL and nothing further should be done. */
  if (node != NULL) {
    if (optimized_case) {
      /* For the optimized cases, set the node type to the type pointed to. */
      node->type = type_pointed_to(orig_type);
    } else {
      /* Not an optimized case.  Just add an indirection. */
      node = add_indirection_to_node(node);
    }  /* if */
    /* Drop type qualifiers because they are meaningless on rvalues.
       Note that no cast is needed to drop the qualifiers: an IL shorthand
       applies in this case. */
    if (is_qualified_type(node->type)) {
      node->type = make_unqualified_type(node->type);
    }  /* if */
  }  /* if */
  return node;
}  /* conv_lvalue_expr_to_rvalue */


void conv_lvalue_to_rvalue(an_operand *operand)
/*
Convert an lvalue operand to an rvalue operand.  See section 3.2.2.1 of the
standard.  In the general case, the lvalue is the address of something,
and this conversion adds an indirection so that the operand refers to
the rvalue pointed to.  Some cases are optimized.  If the operand is
not an lvalue, it is left alone.
*/
{
  an_expr_node_ptr node;
  an_operand       orig_operand;
  an_expr_node_ptr operand_node, cast_node;
  a_type_ptr       cast_orig_type, unqualified_type;
  a_boolean        constant_case = FALSE, qualifiers_dropped = FALSE;
  a_constant_ptr   con_value;

  /* Ignore non-lvalues. */
  if (is_an_lvalue(operand)) {
    /* An lvalue becomes an rvalue. */
#if CHECKING
    /* Array rvalues are not allowed. */
    if (is_array_type(operand->type)) {
      internal_error("conv_lvalue_to_rvalue: array lvalue");
    }  /* if */
#endif /* CHECKING */
    /* Save the operand's source position. */
    orig_operand = *operand;
    /* Change simple "reference" references to "use" references. */
    /* Note that what we want to avoid here is changing "modified" references
       to "use" references, as would happen for references surviving
       from an lvalue-returning assignment. */
    change_some_ref_kinds(operand->ref_entries_list, SRK_REFERENCE, SRK_USE);
    /* Change the kind in the reference entry for a subscripted array from an
       address-taken entry to a simple "use" reference. */
    change_some_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN,
                          SRK_USE);
    if (is_error_operand(operand)) {
      /* Error operand -- leave it alone (but make sure it's not an lvalue
         anymore). */
      conv_to_error_operand(operand);
    } else if (is_incomplete_type(operand->type)) {
      /* Converting an lvalue with incomplete type to an rvalue is undefined
         behavior (standard, 3.2.2.1); we treat it as an error. */
      error_in_operand(ec_incomplete_type_not_allowed, operand);
    } else {
      using_lvalue(operand);
      if (is_constant_operand(operand)) {
        /* The lvalue address is specified by a constant. */
        a_constant_ptr con = &operand->variant.constant;
        if (con_is_exact_addr_of_variable(con)) {
          /* The constant is the address of a variable. */
          /* See if the variable is constant-valued. */
          a_variable_ptr variable = con->variant.address.variant.variable;
          a_constant_ptr con_var_value = var_constant_value(variable);
          if (con_var_value != NULL) {
            /* Replace a constant-valued variable by its value. */
            make_constant_operand(con_var_value, operand);
            constant_case = TRUE;
          } else {
            /* Not constant-valued; the rvalue is the value of the variable. */
            node = var_rvalue_expr(variable);
            qualifiers_dropped = TRUE;
            make_expression_operand(node, node->type, operand);
          }  /* if */
        } else {
          /* Not the address of a variable; add an indirection. */
          node = alloc_node_for_constant(&operand->variant.constant);
          node = add_indirection_to_node(node);
          make_expression_operand(node, node->type, operand);
        }  /* if */
      } else {
#if CHECKING
        /* Since the expression is not a constant, it must be an expression. */
        if (!is_expression_operand(operand)) {
          internal_error("conv_lvalue_to_rvalue: addr not constant or expr");
        }  /* if */
#endif /* CHECKING */
        /* The lvalue address is represented by some kind of expression
           node. */
        node = operand->variant.expression;
        if (C_dialect == C_dialect_pcc && is_operation_node(node) &&
            node->variant.operation.kind ==
                                      (an_expr_operator_kind)eok_lvalue_cast) {
          /* In pcc mode, lvalues cast to a same-sized type can stay
             lvalues.  This is indicated by casting the lvalue address
             to pointer-to-new-type.  Here, turn such a case back into an
             ordinary cast on the rvalue. */
          cast_node = node;
          operand_node = cast_node->variant.operation.operands;
          cast_orig_type = type_pointed_to(cast_node->type);
          /* Save the cast node on the side, and make the operand back
             into the lvalue it was before the lvalue cast.  Then convert
             that lvalue to an rvalue (by a recursive call), and
             cast the resulting rvalue using the saved cast node. */
          operand->type = type_pointed_to(operand_node->type);
          operand->variant.expression = operand_node;
          conv_lvalue_to_rvalue(operand);
          if (!is_expression_operand(operand)) {
            /* The operand is not based on an expression node (unexpected,
               but checked just to be safe).  Throw away the cast node and
               do a cast. */
            cast_operand(cast_orig_type, operand, /*is_implicit_cast=*/FALSE);
          } else {
            /* The cast node can be reused (usual case). */
            operand->type = cast_node->type = cast_orig_type;
            cast_node->variant.operation.kind =
                                               (an_expr_operator_kind)eok_cast;
            /* The expression pointer may have been changed in the 
               conversion to lvalue, so put it in the cast node again. */
            cast_node->variant.operation.operands =
                                                   operand->variant.expression;
            operand->variant.expression = cast_node;
          }  /* if */
        } else {
          /* Normal expression case (not an lvalue cast). */
          /* Convert the expression to an rvalue. */
          node = conv_lvalue_expr_to_rvalue(node, &constant_case, &con_value);
          if (con_value != NULL) {
            /* The value of the expression is a constant.  Make a constant
               operand instead of the expression operand. */
            make_constant_operand(con_value, operand);
          } else {
            /* The value of the expression is not a constant. */
            operand->variant.expression = node;
            operand->type = node->type;
            operand->state = (an_operand_state)os_rvalue;
          }  /* if */
          /* The subroutine handles dropping type qualifiers. */
          qualifiers_dropped = TRUE;
        }  /* if */
      }  /* if */
      /* Drop any type qualifiers on the operand type. */
      if (!qualifiers_dropped && is_qualified_type(operand->type)) {
        unqualified_type = make_unqualified_type(operand->type);
        if (is_expression_operand(operand)) {
          /* For an expression node, just change the expression type.
             That's an IL shorthand form for this case, and avoids a
             cast to a struct or union type. */
          operand->type = operand->variant.expression->type = unqualified_type;
        } else {
          /* For other cases (including constants), do the cast the normal
             way. */
          cast_operand(unqualified_type, operand, /*is_implicit_cast=*/TRUE);
        }  /* if */
      }  /* if */
      if (curr_expr_kind_is_const() && !constant_case) {
        /* An lvalue cannot be converted to an rvalue in a constant
           expression.  The constant_case flag indicates cases where a
           constant-valued variable has been replaced by its value,
           which are allowed in C++. */
        operand->position = orig_operand.position;
        error_in_operand(ec_expr_not_constant, operand);
      }  /* if */
    }  /* if */
    /* Restore the operand's source position. */
    restore_operand_details(operand, &orig_operand);
    /* The ref_entries_list is cleared because it should only contain
       information on lvalue addresses. */
    operand->ref_entries_list = NULL;
    /* Clear the came-from-reference flag, as it is meaningful only for
       lvalues. */
    operand->came_from_reference = FALSE;
  }  /* if */
}  /* conv_lvalue_to_rvalue */


a_type_ptr type_after_array_to_pointer_transformation(a_type_ptr type)
/*
Do the array --> pointer type transformation and return the transformed
type.
*/
{
  /* The array --> pointer transformation converts "array of X" to
     "pointer to X". */
  type = make_pointer_type(array_element_type(type));
  return type;
}  /* type_after_array_to_pointer_transformation */


void conv_array_operand_to_pointer_operand(an_operand *operand)
/*
Apply the implicit array to pointer-to-first-element-of-array transformation
of 3.2.2.1 in the standard to the operand.  If the operand is an array
lvalue it is changed to a pointer to the first element of the array.
If the operand is an rvalue array, an error is issued.  All other cases
are left alone.
*/
{
  a_type_ptr ptr_type;
  an_operand orig_operand;

  if (is_array_type(operand->type)) {
    if (is_an_rvalue(operand)) {
      /* An array rvalue -- error. */
      error_in_operand(ec_bad_rvalue_array, operand);
    } else if (is_an_lvalue(operand)) {
      /* An array lvalue -- convert to a pointer. */
      orig_operand = *operand;
      /* Convert to an rvalue that is the pointer, and change its type
         from pointer-to-array to pointer-to-array-element. */
      ptr_type = type_after_array_to_pointer_transformation(operand->type);
      take_address_of_lvalue(operand);
      cast_operand(ptr_type, operand, /*is_implicit_cast=*/TRUE);
      /* Restore the original source position, etc.  Keep the
         reference entries because if the pointer to the array is
         used in a subscript operation or the like we would like to
         change the kind of reference back to modified or used
         instead of address-taken. */
      restore_operand_details_incl_ref(operand, &orig_operand);
    }  /* if */
  }  /* if */
}  /* conv_array_operand_to_pointer_operand */


a_type_ptr type_after_function_to_pointer_transformation(
                                                       a_type_ptr arg_type,
                                                       an_operand *arg_operand)
/*
Determine the type of an argument of type arg_type (a function type) after
the function --> pointer transformation.  Return the resulting pointer type.
If arg_operand is non-NULL, it points to an operand for the argument.
*/
{
  a_type_ptr ptr_type;

  if (arg_operand != NULL && is_sym_for_member_operand(arg_operand)) {
    /* Member function, so the pointer is a pointer to member.
       This is actually an extension -- the ARM doesn't allow
       a member function reference to decay to a pointer to
       member implicitly.  No warning is needed here, even in
       strict mode; the diagnostic is issued later. */
    a_symbol_ptr  func_sym = arg_operand->variant.symbol;
    a_symbol_ptr  fund_sym = fundamental_symbol_of(func_sym);
    a_routine_ptr rout;
    check_assertion(fund_sym->kind == (a_symbol_kind)sk_member_function);
    rout = fund_sym->variant.routine.ptr;
    ptr_type = ptr_to_member_type(rout->type,
                                 rout->source_corresp.class_of_which_a_member);
  } else {
    /* Nonmember function. */
   ptr_type = make_pointer_type(arg_type);
  }  /* if */
  return ptr_type;
}  /* type_after_function_to_pointer_transformation */


void conv_function_designator_to_ptr_to_function(an_operand *operand)
/*
Convert a function designator operand to a pointer to function expression 
operand.
*/
{
  an_operand   orig_operand;
  a_symbol_ptr func_sym, fund_sym;

  /* If you change this routine, see also the code in
     type_after_function_to_pointer_transformation that does a similar
     transformation without generating errors. */
  orig_operand = *operand;
  /* See if there's an underlying symbol. */
  if (is_sym_for_member_operand(operand) ||
      is_indefinite_function_operand(operand)) {
    /* There is an underlying function symbol. */
    func_sym = operand->variant.symbol;
    /* Check for taking the address of a constructor or destructor, which
       is not allowed (ARM 12.1, 12.4). */
    fund_sym = fundamental_symbol_of(func_sym);
    if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      fund_sym = fund_sym->variant.overloaded_function.symbols;
    }  /* if */
    if (fund_sym->kind == (a_symbol_kind)sk_member_function) {
      a_routine_ptr rout = fund_sym->variant.routine.ptr;
      if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
          rout->special_kind == (a_special_function_kind)sfk_destructor) {
        error_in_operand(ec_addr_of_constructor_or_destructor, operand);
      }  /* if */
    }  /* if */
  }  /* if */

  if (is_error_operand(operand)) {
    /* Error operand; leave it alone. */
  } else if (is_constant_operand(operand)) {
    /* Since the operand becomes "pointer-to" and the constant already has that
       type, just copy the type from the constant. */
    operand->type = operand->variant.constant.type;
  } else if (is_expression_operand(operand)) {
    /* Expression operand.  Since the operand becomes "pointer-to" and the
       expression already has that type, just copy the type from the
       expression. */
    operand->type = operand->variant.expression->type;
  } else if (is_sym_for_member_operand(operand)) {
    /* Converting a qualified member name to a pointer-to-member. */
    func_sym = operand->variant.symbol;
    fund_sym = fundamental_symbol_of(func_sym);
    /* Make an operand for a pointer-to-member constant. */
    make_ptr_to_member_constant_operand(fund_sym, func_sym,
                                        &orig_operand.position,
                                       !operand->access_control_error_reported,
                                        (a_boolean)operand->
                                                      is_operand_of_address_of,
                                        operand);
  } else {
#if CHECKING
    if (!is_indefinite_function_operand(operand)) {
      internal_error(
                   "conv_function_designator_to_ptr_to_function: bad operand");
    }  /* if */
#endif /* CHECKING */
    /* Function symbol -- an overloaded function where we do not
       yet have arguments that will select a specific instance of the function.
       Change to a pointer to an indefinite function by changing the state
       to rvalue. */
    /* Note that we do not check for the nonstandard "taking address of member
       function without using &" here; it will be checked once we know
       which of the functions is actually wanted. */
  }  /* if */
  operand->state = (an_operand_state)os_rvalue;
  operand->came_from_reference = FALSE;
  /* Restore the original source position etc.  Keep the reference
     entries because if the function is called we would like to be able
     to change the reference to referenced instead of address-taken. */
  restore_operand_details_incl_ref(operand, &orig_operand);
  /* Change the kind in the reference entries to address-taken. */
  change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
}  /* conv_function_designator_to_ptr_to_function */


a_type_ptr do_implicit_type_transformations(a_type_ptr type,
                                            an_operand *operand)
/*
Do the implicit array --> pointer and function --> pointer transformations on
a type.  If operand != NULL, it is the associated operand (needed for the
member function --> pointer to member function transformation).
*/
{
  if (is_array_type(type)) {
    type = type_after_array_to_pointer_transformation(type);
  } else if (is_function_type(type)) {
    type = type_after_function_to_pointer_transformation(type, operand);
  }  /* if */
  return type;
}  /* do_implicit_type_transformations */


void do_operand_transformations(an_operand                   *operand,
                                a_transformation_options_set options)
/*
Do some implicit operand transformations on the indicated operand.
The transformations are:
  (1)  Conversion of a function designator to a pointer-to-function.
  (2)  Conversion of an array lvalue to pointer-to-first-element.
  (3)  (Not really a transformation, but...) Checking for indefinite functions.
  (4)  Conversion of an lvalue to an rvalue.
The flags in options can be used to suppress one or more of these
transformations.
*/
{
  if (is_array_type(operand->type)) {
    /* An array lvalue (or rvalue, which is an error). */
    if (!(options & TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION)) {
      /* In most contexts, an lvalue of array type is changed to
         "pointer to first element of array".  See section 3.2.2.1 in the
         ANSI C standard. */
      conv_array_operand_to_pointer_operand(operand);
    }  /* if */
  } else if (is_an_lvalue(operand)) {
    /* A non-array lvalue. */
    if (!(options & TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION)) {
      /* Convert an lvalue to an rvalue. */
      conv_lvalue_to_rvalue(operand);
    }  /* if */
  } else if (is_a_function_designator(operand)) {
    if (!(options & TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION)) {
      /* In most contexts, an entity of type "function returning type"
         is changed to "pointer to function returning type".  
         See section 3.2.2.1 in the ANSI C standard. */
      conv_function_designator_to_ptr_to_function(operand);
    }  /* if */
  }  /* if */
  if (is_indefinite_function_operand(operand)) {
    if (!(options & TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION)) {
      /* Issue an error for an indefinite function (i.e., a C++ overloaded
         function that wasn't called, so we were never able to determine
         which function was intended). */
      sym_error_in_operand(ec_indeterminate_overloaded_function,
                           operand, operand->variant.symbol);
    }  /* if */
  }  /* if */
}  /* do_operand_transformations */


a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			  a_type_ptr type_cast_to)
/*
In pcc mode certain lvalues when cast remain as lvalues after the cast.
Return TRUE if a cast with the before/after types given should leave
its result still an lvalue.
*/
{
  a_boolean is_still_an_lvalue = FALSE;

  type_before_cast = skip_typerefs(type_before_cast);
  type_cast_to = skip_typerefs(type_cast_to);

  /* The result stays an lvalue if the result type size is the same as the
     source type size.  However, casts involving floats require actual
     changes in representation, and are not lvalue-preserving. */
  if (identical_types(type_cast_to, type_before_cast)) {
    /* Same type, operand stays an lvalue. */
    is_still_an_lvalue = TRUE;
  } else if (is_floating_type(type_before_cast) ||
             is_floating_type(type_cast_to)) {
    /* The source or destination types are floating types, so there's
       actual conversion involved. */
    /* is_still_an_lvalue = FALSE; -- already set. */
  } else if (type_cast_to->size == type_before_cast->size &&
             type_cast_to->alignment == type_before_cast->alignment) {
    /* The types are not floating types, and they have the same size
       and alignment. */
    is_still_an_lvalue = TRUE;
  }  /* if */

  return is_still_an_lvalue;
}  /* still_an_lvalue */


a_type_ptr get_logical_result_type(an_operand *operand_1,
				   an_operand *operand_2)
/*
Return the result type for a logical expression (one that returns a logical
value of 0 or 1) with the indicated two operands.
*/
{
  a_type_ptr result_type;

  if (is_error_operand(operand_1) || is_error_operand(operand_2)) {
    result_type = error_type();
  } else if (curr_expr_kind_is(ek_pp)) {
    /* All integers (logicals) have a type of long in the preprocessor. */
    result_type = integer_type((an_integer_kind)ik_long);
  } else {
    /* All logical and relational expressions have a result type of int. */
    result_type = integer_type((an_integer_kind)ik_int);
  }  /* if */

  return result_type;
}  /* get_logical_result_type */


a_boolean check_boolean_controlling_expr(an_operand *operand)
/*
Do some checks on a boolean controlling expression (e.g., "i != 0" in 
"(i != 0) ? j : k").  Check that it's a scalar (arithmetic or pointer)
or a pointer to member; return FALSE if not.  Also normalize the
expression to "!= 0" form if necessary.
*/
{
  a_boolean             okay, add_ne_0;
  an_expr_node_ptr      expr;
  an_expr_operator_kind op;
  an_expr_node_ptr      operand1;
  a_constant            con;
  an_expr_node_ptr      zero_node, new_expr;
  an_operand            orig_operand;

  /* Save the operand's source position. */
  orig_operand = *operand;
  if (is_ptr_to_member_type(operand->type)) {
    /* Pointer to member type is okay. */
    okay = TRUE;
  } else {
    /* Check that the operand is a scalar. */
    okay = check_scalar_operand(operand);
  }  /* if */
  if (okay) {
    switch (operand->kind) {
      case ok_error:
        /* No action. */
        break;
      case ok_expression:
        expr = operand->variant.expression;
        if (!is_operation_node(expr)) {
          /* Add an appropriate "!= 0" on top of variable and variable
             address references. */
          add_ne_0 = TRUE;
        } else {
          op = expr->variant.operation.kind;
          operand1 = expr->variant.operation.operands;
          if (op == (an_expr_operator_kind)eok_iassign ||
              op == (an_expr_operator_kind)eok_fassign ||
              op == (an_expr_operator_kind)eok_passign) {
            /* An assignment operator at the top level.  Check for
               "x = constant", which was probably intended to be
               "x == constant". */
            if (is_constant_node(operand1->next)) {
              pos_warning(ec_assign_where_compare_meant, &operand->position);
            }  /* if */
          }  /* if */
          /* If the top of the expression is not an operator that returns
             a boolean 0/1, add a "!= 0" of the right kind on top. */
          switch (op) {
            case eok_land: case eok_lor: case eok_not:
            case eok_ieq: case eok_feq: case eok_peq:
            case eok_ine: case eok_fne: case eok_pne:
            case eok_igt: case eok_fgt: case eok_pgt:
            case eok_ilt: case eok_flt: case eok_plt:
            case eok_ige: case eok_fge: case eok_pge:
            case eok_ile: case eok_fle: case eok_ple:
            case eok_pmne: case eok_pmeq:
              /* The expression already has a appropriate operator on top,
                 so leave it alone. */
              add_ne_0 = FALSE;
              break;
            default:
              /* Add an appropriate "!= 0" on top of the expression. */
              add_ne_0 = TRUE;
              break;
          }  /* switch */
	}  /* if */
        if (add_ne_0) {
          /* Add a "!= 0" of the appropriate type on top of the expression
             to standardize it. */
          make_zero_of_proper_type(expr->type, &con);
          zero_node = alloc_node_for_constant(&con);
          /* Build a "!=" node of the right kind, pointing to the original
             expression and the zero constant node. */
          new_expr = make_operator_node(which_binary_operator(tok_ne,
                                                              expr->type),
                                        integer_type((an_integer_kind)ik_int),
                                        expr);
          new_expr->next = expr->next;
          expr->next = zero_node;
          make_expression_operand(new_expr, new_expr->type, operand);
        }  /* if */
        break;
      case ok_constant:
        /* The expression is constant.  Make a standard integer 0 or 1
           constant. */
        make_integer_constant_operand(operand,
                                      (long)(!op_is_false_constant(operand)));
        break;
#if CHECKING
      default:
        internal_error("check_boolean_controlling_expr: bad operand kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* Restore the original source position. */
  restore_operand_details(operand, &orig_operand);
  return okay;
}  /* check_boolean_controlling_expr */


#if DEBUG
unsigned long show_expr_space_used(void)
/*
Display and return the amount of space used for various expression tables.
*/
{
  unsigned long num, size, total, grand_total = 0;

  db_space_used_header("Expression table use:");

  db_space_used_lost("arg operands", avail_arg_operands,
                     num_arg_operands_allocated, an_arg_operand);
  db_space_used_lost("arg match summary", avail_arg_match_summaries,
                     num_arg_match_summaries_allocated, an_arg_match_summary);
  db_space_used_lost("candidate function", avail_candidate_functions,
                     num_candidate_functions_allocated, a_candidate_function);
  db_space_used_lost("ref entry", avail_ref_entries,
                      num_ref_entries_allocated, a_ref_entry);
  db_space_used_lost("dynamic init dtor fixup", avail_dynamic_init_dtor_fixups,
                      num_dynamic_init_dtor_fixups_allocated,
                      a_dynamic_init_dtor_fixup);

  db_space_used_total();

  return grand_total;
}  /* show_expr_space_used */
#endif /* DEBUG */


void expr_init(void)
/* 
Initialize things related to expression scanning.
*/
{
  /* Variables in exprutil.h: */
  expr_stack = NULL;
  curr_expr_ref_entries = NULL;
#if DEBUG
  num_arg_match_summaries_allocated = 0;
#endif /* DEBUG */
  
  /* Static variables in exprutil.c: */
  avail_ref_entries = NULL;
  avail_arg_operands = NULL;
  avail_dynamic_init_dtor_fixups = NULL;
#if DEBUG
  num_arg_operands_allocated             = 0;
  num_ref_entries_allocated              = 0;
  num_dynamic_init_dtor_fixups_allocated = 0;
#endif /* DEBUG */

  /* Do initialization for overload.c: */
  overload_init();
}  /* expr_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
