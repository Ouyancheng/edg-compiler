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

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in expression processing. */
#include "expr_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "preproc.h"
#include "pch.h"
#include "func_def.h"

/* Forward declaration required: */
static void conv_array_rvalue_to_lvalue(an_operand *operand);


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
static unsigned long
		num_arg_operands_allocated,
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
the expression is scanned.  The entry is put on the reference list for
the current expression, headed by curr_expr_ref_entries.
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
  if (expr_stack->is_default_arg_expression) {
    rep->kind |= SRK_DEFAULT_ARG_EXPR;
  }  /* if */
  rep->already_recorded = FALSE;
  rep->symbol = sym_ptr;
  copy_source_position(*pos, rep->position);
  rep->next = NULL;
  rep->next_operand_ref = NULL;
  /* Put the entry on the list of entries for the current expression.
     The list is dumped when flush_ref_entries_list is called.
     The entry is put at the end of the list to preserve source order. */
  if (curr_expr_ref_entries == NULL) {
    curr_expr_ref_entries = rep;
  } else {
    a_ref_entry_ptr last_rep;
    for (last_rep = curr_expr_ref_entries;
         last_rep->next != NULL;
         last_rep = last_rep->next) {}
    last_rep->next = rep;
  }  /* if */
  return rep;
}  /* alloc_ref_entry */


a_ref_entry_ptr copy_ref_entry_list(a_ref_entry_ptr ref_list)
/*
Make a copy of a list of reference entries, and return a pointer to the
copied list.  The list of entries is for a single operand, and is linked
on the next_operand_ref field.  The new entries are placed on the reference
list for the current expression, headed by curr_expr_ref_entries.
*/
{
  a_ref_entry_ptr copy_list = NULL, copy_list_end = NULL;

  for (; ref_list != NULL; ref_list = ref_list->next_operand_ref) {
    a_ref_entry_ptr new_ref = alloc_ref_entry(ref_list->symbol,
                                              &ref_list->position);
    *new_ref = *ref_list;
    new_ref->next_operand_ref = NULL;
    if (copy_list == NULL) {
      copy_list = new_ref;
    } else {
      copy_list_end->next_operand_ref = new_ref;
    }  /* if */
    copy_list_end = new_ref;
  }  /* for */
  return copy_list;
}  /* copy_ref_entry_list */


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
    record_symbol_reference(rep->kind, rep->symbol, &rep->position,
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
  a_ref_entry_ptr rep;
  a_boolean       ref_kind_can_be_affected_by_context;
  a_boolean       evaluated = curr_expr_is_potentially_evaluated();
  a_symbol_ptr    fund_sym = fundamental_symbol_of(sym_ptr);

  if (sym_ptr->ambiguous) {
    /* Do not record references to ambiguous symbols, since we don't
       know which symbol is referenced. */
    rep = NULL;
  } else {
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
      record_symbol_reference(SRK_REFERENCE, fund_sym, source_position,
                              /*update_il_entry=*/evaluated);
      rep = NULL;
    } else {
      /* The kind of reference can be affected by context, so build an entry
         for it. */
      rep = alloc_ref_entry(fund_sym, source_position);
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
     flag in routines, because not all references that are address-taken
     initially stay that way.  See the comment for variables below. */
  /* The only possible error cases are variables, and only register variables
     at that.  Functions can always have their addresses taken.  Also
     static data members (they cannot be "register"). */
  if (sym->kind == (a_symbol_kind)sk_variable) {
    a_variable_ptr var = sym->variant.variable.ptr;
    if (C_mode() &&
        var->storage_class == (a_storage_class)sc_register) {
      /* Cannot take the address of a register variable in C. 
         This is allowed in C++, and is allowed (with a warning) in
	 C (except in strict error mode). */
      if (SVR4_C_mode || strict_ansi_error_severity != es_error) {
	pos_warning(ec_address_of_register_variable, &rep->position);
      } else {
	pos_error(ec_address_of_register_variable, &rep->position);
	/* Turn the reference into an error reference so that the error will
	   be issued only once. */
	rep->kind = (rep->kind & ~SRK_ALL_REFERENCES) | SRK_ERROR;
	rep->already_recorded = FALSE;
      }  /* if */
    }  /* if */
    /* Indicate that the address of the variable has been taken.  Setting
       the flag here means it is set even for cases where an address
       is used at some intermediate step but doesn't escape, e.g.,
         a[2] = 1;  // address_taken on "a"
       but a simple interpretation of address_taken seems to be what
       people prefer.  Note that the flag is also set when the
       reference is recorded later, but only if the address-taken
       reference survives to that point, so the setting there is
       more discriminating.  One can remove the assignment here to
       get the other interpretation.  If one does so, one should
       consider whether one wants register variables and parameters
       treated in some special way (e.g., one might want to set the
       address_taken flag here anyway for those), since the address_taken
       flag might be used to decide whether to allocate storage
       for those variables. */
    set_variable_address_taken(var);
  } else if (sym->kind == (a_symbol_kind)sk_static_data_member) {
    /* For simple interpretation of address_taken, set address_taken
        on static data members here.  This can be removed.  See comments
        above. */
    set_variable_address_taken(sym->variant.static_data_member.variable);
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
         One really wants both kinds of references.  A similar case is
             (j = k)++;
         One wants to record the modification separately and before the
         use of the value to avoid a diagnostic about using the variable
         before it is set. */
      if ((old_kind & SRK_MODIFICATION) &&
          (new_kind & (SRK_ADDRESS_TAKEN | SRK_USE))) {
        record_reference(rep);
      }  /* if */
      /* Set the new reference kind. Turn off all old bits, then turn on
         new bits.  SRK_REFERENCE remains set in all cases. */
      rep->kind = (old_kind & ~SRK_ALL_REFERENCES) | new_kind;
      rep->already_recorded = FALSE;
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
        rep->already_recorded = FALSE;
        changed = TRUE;
      }  /* if */
    } else if ((rep->kind & old_kind) != 0) {
      /* Changing a specific reference to another kind of reference.
         Turn off the old bits, then turn on the new bits.
         SRK_REFERENCE will stay set. */
      rep->kind = (rep->kind & ~SRK_ALL_REFERENCES) | new_kind;
      rep->already_recorded = FALSE;
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


#if !MICROSOFT_EXTENSIONS_ALLOWED
/* ARGSUSED */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void free_attachments_to_operand(an_operand *operand)
/*
Free any dynamically-allocated attachments to the indicated operand.
The operand will not be used further.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_property_ref_operand(operand)) {
    /* An ok_property_ref operand has some an_arg_operand entries attached. */
    free_arg_operand_list(operand->variant.property_ref.subscripts);
    operand->variant.property_ref.subscripts = NULL;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* free_attachments_to_operand */


void free_arg_operand_list(an_arg_operand_ptr aop)
/*
Free the list of argument operands pointed to by aop.
*/
{
  an_arg_operand_ptr aop_next;

  for (; aop != NULL; aop = aop_next) {
    aop_next = aop->next;
    /* Free any dynamically-allocated attachments to the operand. */
    free_attachments_to_operand(&aop->operand);
#if CHECKING && DEBUG
    /* Make the sure the entry was not previously freed. */
    if (db_active) {
      an_arg_operand_ptr taop;
      for (taop = avail_arg_operands;
           taop != NULL;
           taop = taop->next) {
        if (taop == aop) {
          internal_error("free_arg_operand_list: entry freed twice");
        }  /* if */
      }  /* for */
    }
#endif /* CHECKING && DEBUG */
    /* Add the entry to the available list. */
    aop->next = avail_arg_operands;
    avail_arg_operands = aop;
  }  /* for */
}  /* free_arg_operand_list */


static a_dynamic_init_dtor_fixup_ptr alloc_dynamic_init_dtor_fixup(
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
    /* Routines referenced in default argument expressions are not
       instantiated until there is a use of the default argument expression. */
    mark_routine_referenced_full(routine,
                                 /*instantiate=*/
                                       !expr_stack->is_default_arg_expression);
  }  /* if */
}  /* if_evaluating_mark_routine_referenced */


void expr_reference_to_implicitly_invoked_function(
                                             a_symbol_ptr      sym,
                                             a_source_position *pos,
                                             a_type_ptr        class_of_object,
                                             a_boolean         honor_virtual)
/*
Interface to reference_to_implicit_invoked_function to be used when
calling it from within the expression-processing routines.  Supplies the
last two parameters from values on the expression stack.
*/
{
  /* Routines referenced in default argument expressions are not
     instantiated until there is a use of the default argument expression. */
  reference_to_implicitly_invoked_function(
                       sym, pos, class_of_object, honor_virtual,
                       curr_expr_is_potentially_evaluated(),
                       /*instantiate=*/!expr_stack->is_default_arg_expression);
}  /* expr_reference_to_implicitly_invoked_function */


an_expr_node_ptr expr_copy_default_arg_expr_list(a_routine_ptr    rout,
                                                 a_param_type_ptr ptp)
/*
Interface to copy_default_arg_expr_list to be used when calling it from
within the expression-processing routines.  Supplies the last two parameters
from values on the expression stack.
*/
{
  an_expr_node_ptr expr;

  expr = copy_default_arg_expr_list(
                          rout, ptp,
                          (a_boolean)expr_stack->inside_conditional_expression,
                          curr_expr_is_potentially_evaluated());
  return expr;
}  /* expr_copy_default_arg_expr_list */


void push_expr_stack(an_expression_kind      expression_kind,
                     an_expr_stack_entry_ptr new_entry,
                     a_boolean               force_object_lifetime,
                     a_boolean               suppress_object_lifetime)
/*
Push a new entry on the top of the expr_stack.  expression_kind indicates
the kind of the expression.  new_entry is used as the new top-of-stack
entry (the entries are local variables on the stack).  This is done
at the start of a major expression.  An object lifetime is pushed
if necessary for a full expression; that's forced by force_object_lifetime
TRUE (which is used to say that even if temporaries have lifetime to
end-of-scope, this expression's temporaries need to be destroyed at
the end of the expression, e.g., because it's an expression repeated
in a loop).  If suppress_object_lifetime is TRUE, no object lifetime
is pushed regardless of any of the other factors.
*/
{
  /* A "full expression" is one not inside another expression.  That's
     significant for the lifetime of temporaries. */
  a_boolean full_expr = (expr_stack == NULL);

  new_entry->prev = expr_stack;
  new_entry->expression_kind = expression_kind;
  new_entry->old_ref_entries_list = curr_expr_ref_entries;
  curr_expr_ref_entries = NULL;
  new_entry->evaluated = TRUE;
  new_entry->potentially_evaluated = TRUE;
  new_entry->is_default_arg_expression = FALSE;
  new_entry->is_template_arg_expression = FALSE;
  new_entry->is_vla_dimension_expression = FALSE;
  new_entry->in_cctor_elision_initializer = FALSE;
  new_entry->fold_constant_addr_exprs = FALSE;
  new_entry->inside_conditional_expression = FALSE;
  new_entry->dynamic_init_dtor_fixup_list = NULL;
  new_entry->nested_construct_depth = 0;
  new_entry->lifetime = NULL;
  new_entry->destructions_preceding_expr = NULL;
  if (expr_stack != NULL) {
    /* There is a previous stack entry; set any of the flags that are affected
       by the enclosing stack entry. */
    /* is_template_arg_expression is not copied down, because it indicates the
       top level in a template argument expression.  Likewise for
       is_vla_dimension_expression.
       in_cctor_elision_initializer is also not copied down; nested expressions
       in an elision initializer are not subject to the optimization. */
    new_entry->evaluated = expr_stack->evaluated;
    new_entry->potentially_evaluated = expr_stack->potentially_evaluated;
    new_entry->is_default_arg_expression =
                                         expr_stack->is_default_arg_expression;
    new_entry->inside_conditional_expression =
                                     expr_stack->inside_conditional_expression;
  }  /* if */
  expr_stack = new_entry;
  /* Do special handling for constant expressions.  This is done late so that
     curr_expr_kind_is_const can be used. */
  if (curr_expr_kind_is_const()) {
    /* Constant addressing expressions should be folded to constants inside
       constant expressions. */
    expr_stack->fold_constant_addr_exprs = TRUE;
    /* Constant expressions are always evaluated even when inside a
       not-evaluated expression.  For example, in sizeof(int[1+1])
       the 1+1 must be evaluated. */
    expr_stack->evaluated = TRUE;
    expr_stack->potentially_evaluated = TRUE;
  }  /* if */
  if (!C_mode() && !suppress_object_lifetime && full_expr) {
    /* Full expression in C++ mode.  We may want to start an object
       lifetime.  Do so if the lifetime of temporaries is a full
       expression, or if the caller explicitly requests a lifetime.
       Don't push a lifetime in a constant expression. */
    if ((!long_lifetime_temps || force_object_lifetime) &&
        !curr_expr_kind_is_const()) {
      push_object_lifetime(iek_none, (char *)NULL,
                           (an_object_lifetime_kind)olk_expr_temporary);
      expr_stack->lifetime = curr_object_lifetime;
    } else {
      /* Don't start a new lifetime, but remember the destructions pointer
         from the current lifetime so we can identify any added for the
         new expression. */
      expr_stack->destructions_preceding_expr =
                                            curr_object_lifetime->destructions;
    }  /* if */
  }  /* if */
}  /* push_expr_stack */


void pop_expr_stack(void)
/*
Pop the top entry off the expr_stack.  This is done at the end of a
major expression.
*/
{
  if (expr_stack->lifetime != NULL) {
    /* An object lifetime was pushed for the expression, so it must be
       popped now. */
    (void)pop_object_lifetime();
  }  /* if */
  /* Flush the reference entries list for the current expression. */
  flush_ref_entries_list();
  /* Restore the old reference entries list, if any. */
  curr_expr_ref_entries = expr_stack->old_ref_entries_list;
  /* Pop the stack. */
  expr_stack = expr_stack->prev;
}  /* pop_expr_stack */


/* Declaration needed because of mutual recursion: */
static a_boolean examine_expr_for_unordered_temp_inits(
                                        an_expr_node_ptr   expr,
                                        a_boolean          mark_all_unordered,
                                        a_dynamic_init_ptr *last_processed);


static a_boolean examine_expr_list_for_unordered_temp_inits(
                                      an_expr_node_ptr   expr_list,
                                      a_boolean          seq_point_after_first,
                                      a_boolean          mark_all_unordered,
                                      a_dynamic_init_ptr *last_processed)
/*
Examine the list of expressions headed by expr_list, and their subtrees,
looking for unordered enk_temp_init initializations.  If any are found,
mark their dynamic initialization entries as unordered.  If mark_all_unordered
is TRUE, mark all enk_temp_init dynamic initializations as unordered
(because of something detected higher up in the expression tree).
If seq_point_after_first is TRUE, there is a sequence point after the
first expression on the list.  Return TRUE if there are any temp inits
(unordered or not) in the expression list.  *last_processed points to
the latest dynamic initialization with destruction processed, and is
updated on output.
*/
{
  a_boolean          any_temp_inits = FALSE;
  an_expr_node_ptr   expr;
  a_dynamic_init_ptr orig_last_processed = *last_processed;

  /* In the first pass, see if there are temp inits in more than one operand.
     If there are, those temp inits are unordered with respect to one
     another.  If we're told to mark everything as unordered, we don't need
     to do this check. */
  if (!mark_all_unordered) {
    for (expr = expr_list; expr != NULL; expr = expr->next) {
      if (examine_expr_for_unordered_temp_inits(expr,
                                                /*mark_all_unordered=*/FALSE,
                                                last_processed)) {
        /* There is at least one temp init in this expression. */
        /* If there was a previous operand with a temp init, there's an
           ordering problem, except with operators with a sequence point after
           the first operand (because in those, temp inits in one operand
           are never unordered with respect to those in another operand;
           also, in "a ? b : c", temp inits in b and c are not considered
           unordered with respect to one another because only one of
           the two expressions will be evaluated). */
        if (any_temp_inits && !seq_point_after_first) {
          mark_all_unordered = TRUE;
          /* There's no need to look at the rest of the operands in this
             loop; we'll loop through all of them below. */
          break;
        }  /* if */
        any_temp_inits = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
  /* Mark all temp inits as unordered if appropriate. */
  /* This also sets or finishes setting any_temp_inits. */
  if (mark_all_unordered) {
    /* Restore *last_processed if we changed it in the loop above. */
    *last_processed = orig_last_processed;
    for (expr = expr_list; expr != NULL; expr = expr->next) {
      if (examine_expr_for_unordered_temp_inits(expr,
                                                /*mark_all_unordered=*/TRUE,
                                                last_processed)) {
        any_temp_inits = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
  return any_temp_inits;
}  /* examine_expr_list_for_unordered_temp_inits */


static void update_last_processed_dynamic_init(
                                            a_dynamic_init_ptr dip,
                                            a_dynamic_init_ptr *last_processed)
/*
The dynamic initialization entry pointed to by dip has been encountered
while traversing an expression tree looking for unordered enk_temp_inits.
Record it in *last_processed as the latest entry encountered.
If dip is not the expected next entry, rearrange the destructions list.
This comes up when user-defined conversions are added to arguments of
an overloaded function call.  When *last_processed is NULL, assume the
entry is in the right place; other entries from the expression will be
moved following it as necessary when they come up (this is necessary
in long lifetime temporaries mode, where there might be unrelated
destructions preceding the first destruction from the expression).
*/
{
  /* Process only entries on a lifetime list, and only those on the
     current lifetime list */
  if (dip->lifetime != NULL && dip->lifetime == curr_object_lifetime) {
    if (*last_processed != NULL &&
        dip->next_in_destruction_list != *last_processed) {
      /* The destruction is not at the right place on the list.  Move it. */
      a_dynamic_init_ptr tdip = curr_object_lifetime->destructions;
      /* Remove dip from the destructions list. */
      if (tdip == dip) {
        /* It's the first entry on the list. */
        curr_object_lifetime->destructions = dip->next_in_destruction_list;
      } else {
        /* It's not the first entry on the list.  Find the entry preceding
           it. */
        for (;; tdip = tdip->next_in_destruction_list) {
#if CHECKING
          if (tdip == NULL) {
#if DEBUG 
            fprintf(f_debug, "Dynamic init not found:\n");
            db_dynamic_initializer(dip, 2);
            db_object_lifetime(curr_object_lifetime);
#endif /* DEBUG */
            unexpected_condition_str2("update_last_processed_dynamic_init:",
                                      "dip not found");
          }  /* if */
#endif /* CHECKING */
          if (tdip->next_in_destruction_list == dip) break;
        }  /* for */
        tdip->next_in_destruction_list = dip->next_in_destruction_list;
      }  /* if */
      /* Insert dip in the list, preceding *last_processed. */
      tdip = curr_object_lifetime->destructions;
      if (tdip == *last_processed) {
        /* Insert at the beginning of the list. */
        curr_object_lifetime->destructions = dip;
      } else {
        /* Find the point at which the insert is to be done. */
        for (;; tdip = tdip->next_in_destruction_list) {
#if CHECKING
          if (tdip == NULL) {
#if DEBUG 
            fprintf(f_debug, "Dynamic init to insert:\n");
            db_dynamic_initializer(dip, 2);
            fprintf(f_debug, "Dynamic init not found:\n");
            db_dynamic_initializer(*last_processed, 2);
            db_object_lifetime(curr_object_lifetime);
#endif /* DEBUG */
            unexpected_condition_str2("update_last_processed_dynamic_init:",
                                      "insert point not found");
          }  /* if */
#endif /* CHECKING */
          if (tdip->next_in_destruction_list == *last_processed) break;
        }  /* for */
        tdip->next_in_destruction_list = dip;
      }  /* if */
      dip->next_in_destruction_list = *last_processed;
    }  /* if */
    *last_processed = dip;
  }  /* if */
}  /* update_last_processed_dynamic_init */


static a_boolean examine_dynamic_init_for_unordered_temp_inits(
                                         a_dynamic_init_ptr dip,
                                         a_boolean          mark_all_unordered,
                                         a_dynamic_init_ptr *last_processed)
/*
Examine the dynamic initialization entry pointed to by dip, and its subtree,
looking for unordered enk_temp_init initializations.  Do nothing if
dip == NULL.  If any unordered temp inits are found, mark their dynamic
initialization entries as unordered.  If mark_all_unordered
is TRUE, mark all enk_temp_init dynamic initializations as unordered
(because of something detected higher up in the expression tree).
Return TRUE if there are any temp inits (unordered or not) in the dynamic
initialization.  *last_processed points to the latest dynamic initialization
with destruction processed, and is updated on output.
*/
{
  a_boolean any_temp_inits = FALSE;

  if (dip != NULL) {
    switch (dip->kind) {
      case dik_none:
      case dik_zero:
      case dik_constant:
        break;
      case dik_expression:
      case dik_call_returning_class_via_cctor:
        any_temp_inits = examine_expr_for_unordered_temp_inits(
                                               dip->variant.expression,
                                               mark_all_unordered,
                                               last_processed);
        break;
      case dik_constructor:
        any_temp_inits = examine_expr_list_for_unordered_temp_inits(
                                               dip->variant.constructor.args,
                                               /*seq_point_after_first=*/FALSE,
                                               mark_all_unordered,
                                               last_processed);
        break;
      case dik_nonconstant_aggregate:
        /* These can come up for an array new. */
        { a_constant_ptr aggr = dip->variant.constant, di_con;
          a_constant_ptr first_con = aggr->variant.aggregate.first_constant;
          check_assertion(first_con->kind ==
                                        (a_constant_repr_kind)ck_init_repeat &&
                          first_con->next == NULL);
          di_con = first_con->variant.init_repeat.constant;
          check_assertion(di_con->kind==(a_constant_repr_kind)ck_dynamic_init);
          any_temp_inits = examine_dynamic_init_for_unordered_temp_inits(
                                                  di_con->variant.dynamic_init,
                                                  mark_all_unordered,
                                                  last_processed);
        }
        break;
      case dik_bitwise_copy:
        /* These are not expected under expressions. */
      default:
        unexpected_condition_str(
       "examine_dynamic_init_for_unordered_temp_inits: bad dynamic init kind");
    }  /* switch */
    /* Remember the last dynamic initialization processed. */
    update_last_processed_dynamic_init(dip, last_processed);
  }  /* if */
  return any_temp_inits;
}  /* examine_dynamic_init_for_unordered_temp_inits */


static a_boolean examine_expr_for_unordered_temp_inits(
                                         an_expr_node_ptr   expr,
                                         a_boolean          mark_all_unordered,
                                         a_dynamic_init_ptr *last_processed)
/*
Examine expr and its subtree looking for unordered enk_temp_init
initializations.  If any are found, mark their dynamic initialization
entries as unordered.  If mark_all_unordered is TRUE, mark all
enk_temp_init dynamic initializations as unordered (because of
something detected higher up in the expression tree).  Return TRUE
if there are any temp inits (unordered or not) in the expression.
*last_processed points to the latest dynamic initialization with
destruction processed, and is updated on output.
*/
{
  a_boolean             any_temp_inits = FALSE, any_temp_inits_part_2;
  a_boolean             seq_point_after_first;
  an_expr_operator_kind op;
  a_dynamic_init_ptr    dip, dyn_init_to_free_storage;
  a_dynamic_init_ptr    orig_last_processed = *last_processed;

  switch (expr->kind) {
    case enk_error:
    case enk_constant:
    case enk_variable:
    case enk_variable_address:
    case enk_routine_address:
    case enk_field:
    case enk_address_of_ellipsis:
      /* No temp inits. */
      break;
    case enk_operation:
      seq_point_after_first = FALSE;
      op = expr->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_land ||
          op == (an_expr_operator_kind)eok_lor ||
          op == (an_expr_operator_kind)eok_comma ||
          op == (an_expr_operator_kind)eok_question) {
        /* Operators with a sequence point after the first operand. */
        seq_point_after_first = TRUE;
      }  /* if */
      any_temp_inits = examine_expr_list_for_unordered_temp_inits(
                                        expr->variant.operation.operands,
                                        seq_point_after_first,
                                        mark_all_unordered,
                                        last_processed);
      break;
    case enk_temp_init:
      any_temp_inits = examine_dynamic_init_for_unordered_temp_inits(
                                        expr->variant.init.dynamic_init,
                                        mark_all_unordered,
                                        last_processed);
      /* An enk_temp_init only counts if it is on a lifetime list (which
         implies it has an associated destruction). */
      dip = expr->variant.init.dynamic_init;
      if (dip->lifetime != NULL) {
        any_temp_inits = TRUE;
        if (mark_all_unordered) dip->unordered = TRUE;
      }  /* if */
      break;
    case enk_new_delete:
      any_temp_inits = examine_expr_list_for_unordered_temp_inits(
                                        expr->variant.new_delete->arg,
                                        /*seq_point_after_first=*/FALSE,
                                        mark_all_unordered,
                                        last_processed);
      /* The strange dynamic initialization entry that describes the freeing
         of uninitialized storage on an exception is guaranteed to happen
         after the "new" arguments are evaluated (you can't free storage
         until after you've allocated it), so bundle it into the
         setting of any_temp_inits. */
      dyn_init_to_free_storage =
                     expr->variant.new_delete->freeing_of_storage_on_exception;
      if (dyn_init_to_free_storage != NULL) {
        any_temp_inits = TRUE;
        if (mark_all_unordered) dyn_init_to_free_storage->unordered = TRUE;
        /* Remember the last dynamic initialization processed. */
        update_last_processed_dynamic_init(dyn_init_to_free_storage,
                                           last_processed);
      }  /* if */
      /* The "new" arguments and the initialization are unordered with
         respect to one another.  If there were temp inits in the
         "new" arguments, we mark the temp inits in the initialization as we
         check them here. */
      any_temp_inits_part_2 = examine_dynamic_init_for_unordered_temp_inits(
                                        expr->variant.new_delete->dynamic_init,
                                        mark_all_unordered || any_temp_inits,
                                        last_processed);
      /* The unordered flag in the dynamic init never has to be set because it
         doesn't do destruction and therefore is never on a lifetime list. */
      if (any_temp_inits && any_temp_inits_part_2 && !mark_all_unordered) {
        /* The "new" arguments and the initialization each contain at
           least one temp init, so those are unordered with respect to
           one another.  Go back and mark the temp inits in the
           "new" arguments. */
        *last_processed = orig_last_processed;
        (void)examine_expr_list_for_unordered_temp_inits(
                                        expr->variant.new_delete->arg,
                                        /*seq_point_after_first=*/FALSE,
                                        /*mark_all_unordered=*/TRUE,
                                        last_processed);
        /* Also mark the dynamic initialization entry that frees storage as
           unordered. */
        if (dyn_init_to_free_storage != NULL) {
          dyn_init_to_free_storage->unordered = TRUE;
        }  /* if */
      }  /* if */
      any_temp_inits |= any_temp_inits_part_2;
      break;
    case enk_throw:
      any_temp_inits = examine_dynamic_init_for_unordered_temp_inits(
                                        expr->variant.throw_info->dynamic_init,
                                        mark_all_unordered,
                                        last_processed);
      break;
    case enk_typeid:
      if (expr->variant.typeid_info.expr != NULL) {
        any_temp_inits = examine_expr_for_unordered_temp_inits(
                                               expr->variant.typeid_info.expr,
                                               mark_all_unordered,
                                               last_processed);
      }  /* if */
      break;
    case enk_runtime_sizeof:
      if (!expr->variant.runtime_sizeof.is_type) {
        any_temp_inits = examine_expr_for_unordered_temp_inits(
                                     expr->variant.runtime_sizeof.variant.expr,
                                     mark_all_unordered,
                                     last_processed);
      }  /* if */
      break;
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      /* No temp inits. */
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      /* No temp inits. */
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    case enk_object_lifetime:  /* Not expected at this level. */
    case enk_condition:        /* Not expected at this level. */
    default:
      unexpected_condition_str(
                       "examine_expr_for_unordered_temp_inits: bad expr kind");
  }  /* switch */
  return any_temp_inits;
}  /* examine_expr_for_unordered_temp_inits */


static a_boolean curr_expr_may_contain_unordered_temp_inits(void)
/*
Return TRUE if the current expression potentially contains unordered
enk_temp_inits.  This is tested by checking whether there are two or
more temp inits in the whole expression.
*/
{
  a_boolean          may_contain_unordered_temp_inits = TRUE;
  a_dynamic_init_ptr curr_destruction = curr_object_lifetime->destructions;
  a_dynamic_init_ptr after_last_destruction =
                                       expr_stack->destructions_preceding_expr;

  if (curr_destruction == after_last_destruction ||
      curr_destruction->next_in_destruction_list == after_last_destruction) {
    /* The dynamic init contains no temp inits or only one, so there can be
       no unordered temp inits. */
    may_contain_unordered_temp_inits = FALSE;
  }  /* if */
  return may_contain_unordered_temp_inits;
}  /* curr_expr_may_contain_unordered_temp_inits */
    

an_expr_node_ptr wrap_up_full_expression(an_expr_node_ptr expr)
/*
Do any processing required at the end of a "full expression" that is expr.
Do nothing if the expression is not a full expression (according to
the expr_stack).  Also do nothing in C mode.
*/
{
  an_object_lifetime_ptr lifetime = expr_stack->lifetime;
  a_dynamic_init_ptr     last_processed = NULL;

  if (!C_mode() && expr_stack->prev == NULL) {
    /* Full expression in C++ mode. */
    /* If the expression contains more than one enk_temp_init, see if they
       are unordered with respect to one another.  This must be done at the
       end because of temp inits that get optimized out. */
    if (curr_expr_may_contain_unordered_temp_inits()) {
      (void)examine_expr_for_unordered_temp_inits(expr,
                                                 /*mark_all_unordered=*/FALSE,
                                                 &last_processed);
    }  /* if */
    /* If the current expression has an associated object lifetime with
       something in it, add an enk_object_lifetime node on the top of the
       expression tree. */
    if (lifetime != NULL) {
      /* Check to see if the object lifetime has anything in it.  If not,
         there is no need to add the enk_object_lifetime node. */
      if (!is_useless_object_lifetime(lifetime)) {
        /* An error node stays the same. */
        if (is_error_node(expr)) {
          mark_object_lifetime_as_useless(lifetime);
        } else {
          expr = add_object_lifetime_to_expr(expr, lifetime);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return expr;
}  /* wrap_up_full_expression */


void wrap_up_dynamic_init_full_expression(a_dynamic_init_ptr dip)
/*
Do any processing required at the end of a "full expression" that is
the constructor call implied by a parenthesized initializer.  dip
points to the dynamic initialization.
*/
{
  an_object_lifetime_ptr lifetime = expr_stack->lifetime;
  a_dynamic_init_ptr     last_processed = NULL;

  if (!C_mode()) {
    /* If the initialization contains more than one enk_temp_init, see if they
       are unordered with respect to one another.  This must be done at the
       end because of temp inits that get optimized out. */
    if (curr_expr_may_contain_unordered_temp_inits()) {
      (void)examine_dynamic_init_for_unordered_temp_inits(
                                                  dip,
                                                  /*mark_all_unordered=*/FALSE,
                                                  &last_processed);
    }  /* if */
    if (lifetime != NULL) {
      if (dip != NULL) {
        bind_object_lifetime(lifetime, iek_dynamic_init, (char *)dip);
      } else {
        /* Error. */
        mark_object_lifetime_as_useless(lifetime);
      }  /* if */      
    }  /* if */
  }  /* if */
}  /* wrap_up_dynamic_init_full_expression */


void discard_curr_expr_object_lifetime(void)
/*
If the current expression stack entry has an associated object lifetime,
mark it so it will be discarded later.  This is done for errors.
*/
{
  an_object_lifetime_ptr lifetime = expr_stack->lifetime;

  if (lifetime != NULL) mark_object_lifetime_as_useless(lifetime);
}  /* discard_curr_expr_object_lifetime */


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
#if MICROSOFT_EXTENSIONS_ALLOWED
    case ok_property_ref:
      operand->variant.property_ref.object = NULL;
      operand->variant.property_ref.field = NULL;
      operand->variant.property_ref.subscripts = NULL;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  operand->access_control_error_reported = FALSE;
  operand->is_operand_of_address_of = FALSE;
  operand->is_template_id = FALSE;
  operand->is_simple_string_literal = FALSE;
  operand->is_cfront_null_pointer_constant = FALSE;
  operand->is_using_decl_name = FALSE;
  operand->position = null_source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  operand->end_position = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  operand->ref_entries_list = NULL;
  operand->template_arg_list = NULL;
  operand->id_position = null_source_position;
  set_operand_kind(operand, kind);
}  /* clear_operand */

#if EXTRA_SOURCE_POSITIONS_IN_IL

void set_operand_expr_position_if_expr(an_operand *operand)
/*
If operand is an expression operand, set the source positions in the
underlying expression.
*/
{
  if (is_expression_operand(operand)) {
    an_expr_node_ptr expr = operand->variant.expression;
    expr->expr_range.start = operand->position;
    expr->expr_range.end   = operand->end_position;
  }  /* if */
}  /* set_operand_expr_position_if_expr */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

static void set_operand_position_to_pos_curr_token(an_operand *operand)
/*
Set the position in the given operand to the current token position.
If the operand is an expression, do not set the position in the expression.
*/
{
  operand->position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  operand->end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* set_operand_position_to_pos_curr_token */

#if DEBUG

void db_operand(an_operand *operand)
/*
Display an expression operand for debugging purposes.
*/
{
  switch (operand->state) {
    case os_none:
      break;
    case os_lvalue:
      (void)fprintf(f_debug, "lvalue, ");
      break;
    case os_rvalue:
      (void)fprintf(f_debug, "rvalue, ");
      break;
    case os_function_designator:
      (void)fprintf(f_debug, "function, ");
      break;
    default:
      (void)fprintf(f_debug, "<bad operand state>, ");
      break;
  }  /* switch */
  (void)fprintf(f_debug, "type = ");
  if (operand->type == NULL) {
    (void)fprintf(f_debug, "NULL");
  } else {
    db_abbreviated_type(operand->type);
  }  /* if */
  (void)fprintf(f_debug, ", ");
  switch (operand->kind) {
    case ok_error:
      (void)fprintf(f_debug, "error");
      break;
    case ok_expression:
      (void)fprintf(f_debug, "expression = \n");
      db_expression(operand->variant.expression);
      break;
    case ok_constant:
      (void)fprintf(f_debug, "constant = ");
      db_constant(&operand->variant.constant);
      break;
    case ok_indefinite_function:
      (void)fprintf(f_debug, "indefinite function = ");
      db_symbol(operand->variant.symbol, "", 0);
      break;
    case ok_sym_for_member:
      (void)fprintf(f_debug, "sym for member = ");
      db_symbol(operand->variant.symbol, "", 0);
      break;
    case ok_undefined_symbol:
      (void)fprintf(f_debug, "undefined symbol = ");
      db_symbol(operand->variant.symbol, "", 0);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case ok_property_ref:
      (void)fprintf(f_debug, "property ref = \n");
      (void)fprintf(f_debug, "object =\n");
      db_expression(operand->variant.property_ref.object);
      (void)fprintf(f_debug, "field = ");
      db_name(&operand->variant.property_ref.field->source_corresp);
      (void)fprintf(f_debug, "\nsubscripts =\n");
      { an_arg_operand_ptr aop;
        for (aop = operand->variant.property_ref.subscripts;
             aop != NULL;
             aop = aop->next) {
          db_operand(&aop->operand);
        }  /* for */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      (void)fprintf(f_debug, "<bad operand kind>");
      break;
  }  /* switch */
  (void)fprintf(f_debug, "\n");
}  /* db_operand */

#endif /* DEBUG */
#if MICROSOFT_EXTENSIONS_ALLOWED

void clone_operand(an_operand *operand,
                   an_operand *operand_clone)
/*
Make a clone of the operand "operand" and put it in "operand_clone".
The cloning process copies the subtrees of the operand, e.g., if it's
an expression, a separate copy of the expression is made for the
operand clone.
*/
{
  an_expr_copy_options_set copy_options =
                                    expr_stack->inside_conditional_expression ?
                                             CE_INSIDE_CONDITIONAL_EXPRESSION :
                                             CE_NO_OPTIONS;

  copy_operand(operand, operand_clone);
  switch (operand->kind) {
    case ok_error:
    case ok_constant:
    case ok_indefinite_function:
    case ok_sym_for_member:
    case ok_undefined_symbol:
      /* Nothing to copy. */
      break;
    case ok_expression:
      operand_clone->variant.expression =
                                    copy_expr_tree(operand->variant.expression,
                                                   copy_options);
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case ok_property_ref:
      operand_clone->variant.property_ref.object =
                           copy_expr_tree(operand->variant.property_ref.object,
                                          copy_options);
      /* Copy the list of subscript operands. */
      { an_arg_operand_ptr aop, last_clone_aop = NULL;
        for (aop = operand->variant.property_ref.subscripts;
             aop != NULL;
             aop = aop->next) {
          an_arg_operand_ptr aop_clone = alloc_arg_operand();
          clone_operand(&aop->operand, &aop_clone->operand);
          if (last_clone_aop == NULL) {
            operand_clone->variant.property_ref.subscripts = aop_clone;
          } else {
            last_clone_aop->next = aop_clone;
          }  /* if */
          last_clone_aop = aop_clone;
        }  /* for */
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("clone_operand: unexpected operand kind");
  }  /* switch */
}  /* clone_operand */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


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
        copy_operand_position_to_expr(operand, node);
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
This is only used in C++, for some strange cases.
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  operand->end_position = orig_operand->end_position;
  /* If necessary, set the position in the expression too. */
  if (is_expression_operand(orig_operand) &&
      is_expression_operand(operand) &&
      orig_operand->variant.expression == operand->variant.expression) {
    /* The expression is the same, so don't update it.  This is important
       for cases where we've deliberately set a different position on
       the operand and the expression (e.g., because of indirection) and
       we'd like to preserve that over some transformation like
       lvalue-to-rvalue conversion. */
  } else {
    set_operand_expr_position_if_expr(operand);
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  operand->bound_function = orig_operand->bound_function;
  operand->is_qualified_name = orig_operand->is_qualified_name;
  operand->access_control_error_reported =
                                   orig_operand->access_control_error_reported;
  operand->is_operand_of_address_of = orig_operand->is_operand_of_address_of;
  operand->is_using_decl_name = orig_operand->is_using_decl_name;
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


void operand_will_not_be_used_because_of_error(an_operand *operand)
/*
The indicated operand will not be used further because an error has
been detected.  There is also the implication that because of the
error we cannot tell how the operand would have been used.  Do any
cleanup required.
*/
{
  /* Change the references to errors. */
  change_operand_refs_to_error(operand);
  /* Free any dynamically-allocated attachments to the operand. */
  free_attachments_to_operand(operand);
}  /* operand_will_not_be_used_because_of_error */


void conv_to_error_operand(an_operand *operand)
/*
Take an existing operand and convert it to an error operand.  Retain the
position field as the error position.
*/
{
  operand_will_not_be_used_because_of_error(operand);
  set_operand_kind(operand, (an_operand_kind)ok_error);
  operand->type = error_type();
  operand->state = (an_operand_state)os_none;
  operand->is_simple_string_literal = FALSE;
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


static void sym_error_in_operand(an_error_code error_code,
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
  set_operand_position_to_pos_curr_token(operand);
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
    if (curr_expr_kind_is_const() ||
        /* ck_address constants as nontype template arguments are funny --
           they've been given reference type so that they get the special
           processing here and are treated as lvalues, but when they're
           put out by il_to_str they will appear to have pointer type.
           So use the constant form rather than the form marked with
           implicit_reference_indirection (below). */
        constant.kind == (a_constant_repr_kind)ck_address) {
      make_constant_operand(&constant, operand);
    } else {
      /* Non-constant expression.  Make an expression node so that we
         can set the implicit_reference_indirection flag therein (there's
         no similar flag for constants). */
      an_expr_node_ptr expr = alloc_node_for_constant(&constant);
      expr->implicit_reference_indirection = TRUE;
      make_expression_operand(expr, expr->type, operand);
    }  /* if */
    if (is_function_type(underlying_type)) {
      operand->state = (an_operand_state)os_function_designator;
    } else {
      operand->state = (an_operand_state)os_lvalue;
    }  /* if */
    operand->type = underlying_type;
  } else {
    /* Normal (non-reference) case. */
    make_constant_operand(con_ptr, operand);
    if (is_template_dependent_context() &&
        con_ptr->kind == (a_constant_repr_kind)ck_template_param &&
        con_ptr->variant.template_param.kind ==
                       (a_template_param_constant_kind)tpck_unknown_function) {
      /* Unknown functions in prototype instantiations start out as function
         designators. */
      operand->state = (an_operand_state)os_function_designator;
    }  /* if */
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
  operand->is_simple_string_literal = TRUE;
}  /* make_string_constant_operand */


void make_integer_constant_operand(an_operand		*operand,
				   a_host_large_integer	value)
/*
Make a constant operand and set it to some integer value.
*/
{
  clear_operand((an_operand_kind)ok_constant, operand);
  set_integer_constant(&operand->variant.constant, value,
                       (an_integer_kind)ik_int);
  operand->type = operand->variant.constant.type;
  operand->state = (an_operand_state)os_rvalue;
  set_operand_position_to_pos_curr_token(operand);
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
  set_operand_position_to_pos_curr_token(operand);
}  /* make_expression_operand */


void make_indefinite_function_operand(a_symbol_ptr routine_sym,
                                      a_boolean    curr_id,
                                      an_operand   *operand)
/*
Make an operand for a C++ overloaded function symbol.  routine_sym points
to the symbol entry for the function (possibly a projection symbol).
If curr_id is TRUE, locator_for_curr_id is used for additional information.
The current token position is used for the overall operand position.
The operand is put into *operand and is a function designator.
*/
{
  clear_operand((an_operand_kind)ok_indefinite_function, operand);
  operand->state = (an_operand_state)os_function_designator;
  operand->type = unknown_type();
  operand->variant.symbol = routine_sym;
  set_operand_position_to_pos_curr_token(operand);
  if (curr_id) {
    operand->is_qualified_name = locator_for_curr_id.is_qualified_name;
    operand->is_template_id = locator_for_curr_id.is_template_id;
    operand->template_arg_list = locator_for_curr_id.template_arg_list;
    operand->id_position = locator_for_curr_id.source_position;
  } else {
    operand->id_position = operand->position;
  }  /* if */
}  /* make_indefinite_function_operand */


void make_sym_for_member_operand(a_symbol_ptr    member_sym,
                                 a_boolean       is_qualified_name,
                                 a_ref_entry_ptr rep,
                                 an_operand      *operand)
/*
Make an operand for a nonstatic class member name.  Used in C++ to
represent a member name per se, that is, to represent an uninterpreted
member name from when it is scanned until it becomes clear from the
context how the member is being used.  Not used for overloaded functions.
member_sym points to the symbol entry for the member (possibly a
projection symbol).  is_qualified_name is TRUE if the name was 
qualified, e.g., "A::f" instead of just "f".  rep points to
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
  operand->is_qualified_name = is_qualified_name;
  set_operand_position_to_pos_curr_token(operand);
  operand->ref_entries_list = rep;
}  /* make_sym_for_member_operand */


void make_template_param_expr_constant_operand(an_expr_node_ptr node,
                                               an_operand       *result)
/*
Build an operand for a ck_template_param constant for the expression
"node".  This makes a constant of subkind tpck_expression.  Return the
operand in *result.
*/
{
  a_constant con;

  /* Build the ck_template_param constant. */
  clear_constant(&con, (a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(&con,
                              (a_template_param_constant_kind)tpck_expression);
  con.variant.template_param.variant.expr = node;
  con.type = node->type;
  /* Make the operand. */
  make_constant_operand(&con, result);
}  /* make_template_param_expr_constant_operand */


void add_base_class_casts(a_base_class_ptr  bcp,
                          a_type_ptr        qualifiers_model,
                          a_boolean         check_cast_access,
                          a_boolean         is_implicit_cast,
                          a_boolean         implicit_in_naming,
                          an_expr_node_ptr  *p_node,
                          a_source_position *err_pos)
/*
Add casts to *p_node to change its type from a pointer to a class type to
a pointer to a base class of that class; bcp indicates the base class
and qualifiers_model indicates the qualifiers to be placed on that class
type.  Access control is done on the cast if check_cast_access is TRUE.
is_implicit_cast is TRUE if the cast is implicit.  implicit_in_naming
is TRUE for casts that are generated implicitly in referencing a member
of a class (roughly, in getting from the name used in the source --
the projection symbol -- to the member actually used in the IL).
*err_pos indicates a source position to be used for errors.  This
routine is only used in C++ mode.
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
    /* No access checking in prototype instantiations. */
    if (is_template_dependent_context()) check_cast_access = FALSE;
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
            pos_ty_diagnostic(es_discretionary_error,
                              ec_inaccessible_base_class, err_pos,
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
      (*p_node)->variant.operation.implicit_in_member_naming =
                                                            implicit_in_naming;
      if (!is_implicit_cast && dsp->next != NULL) {
        (*p_node)->variant.operation.implicit_step_of_explicit_cast = TRUE;
      }  /* if */
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
    check_assertion(is_operation_node(*p_node) &&
                    (*p_node)->variant.operation.kind ==
                                (an_expr_operator_kind)eok_derived_class_cast);
    (*p_node)->variant.operation.implicit_step_of_explicit_cast = TRUE;
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
  } else if (any_virtual_steps_in_derivation(bcp) && !any_cfront_mode()) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    pos_ty2_error(ec_pm_virtual_base_from_derived_class, err_pos,
                  pm_class_type((*p_node)->type), bcp->type);
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
      if (dsp->next != NULL) {
        (*p_node)->variant.operation.implicit_step_of_explicit_cast = TRUE;
      }  /* if */
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
    if (!is_implicit_cast) {
      check_assertion(is_operation_node(*p_node) &&
                      (*p_node)->variant.operation.kind ==
                             (an_expr_operator_kind)eok_pm_derived_class_cast);
      (*p_node)->variant.operation.implicit_step_of_explicit_cast = TRUE;
    }  /* if */
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
    pos_ty2_error(ec_pm_derived_class_from_virtual_base, err_pos,
                  new_class_pointed_to, bcp->type);
    *p_node = error_node();
  } else {
    /* No access checking in prototype instantiations. */
    if (is_template_dependent_context()) check_cast_access = FALSE;
    if (check_cast_access) {
      /* Check the accessibility of the base class.  (Recall that casts
         to derived types can be done implicitly.) */
      curr_type = new_class_pointed_to;
      for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
        base_class = dsp->base_class;
        /* Check that the base class is accessible from the current class. */
        if (!is_accessible_imm_base_class(base_class, curr_type)) {
          pos_ty_diagnostic(es_discretionary_error, ec_inaccessible_base_class,
                            err_pos, base_class->type);
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
                             a_boolean         check_cast_access,
                             a_boolean         is_implicit_cast,
                             a_boolean         is_reinterpret_cast,
                             a_boolean         reinterpret_semantics,
                             a_source_position *err_pos)
/*
Add a cast node to the expression tree pointed to by *p_node, and update
*p_node to point to the cast node.  The old node is cast to the type new_type.
*err_pos gives the source position for errors.  Check access on the
cast if check_cast_access is TRUE.  If is_implicit_cast is TRUE, this
is an implicit cast rather than an explicit one.  This routine generates
the special IL operators used for base-->derived and derived-->base class
pointer casts, when they are appropriate.  It also issues errors for
invalid casts of that kind (e.g., ambiguous).  If reinterpret_semantics
is TRUE, those related class casts are not checked for.  is_reinterpret_cast
indicates that the cast comes from a reinterpret_cast construct in the source.
*/
{
  a_type_ptr       old_type = (*p_node)->type, new_type_pointed_to;
  a_boolean        baseward_cast;
  a_base_class_ptr bcp;

  /* Make sure the next field of the node is cleared.  The caller must 
     make sure it's copied if that's necessary (it can't be done here,
     because we can't link a previous expression in a list to this
     new expression). */
  (*p_node)->next = NULL;
  if (!C_mode() && !reinterpret_semantics &&
      related_class_pointers(old_type, new_type, &baseward_cast, &bcp)) {
    /* C++ cast from a pointer to a class to a pointer to a related
       (base or derived) class. */
    new_type_pointed_to = type_pointed_to(new_type);
    if (baseward_cast) {
      /* Derived --> base.  Valid unless the cast is ambiguous or
         the base class is inaccessible. */
      add_base_class_casts(bcp, new_type_pointed_to,
                           check_cast_access, is_implicit_cast,
                           /*implicit_in_naming=*/FALSE,
                           p_node, err_pos);
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      add_derived_class_casts(new_type_pointed_to, bcp, p_node, err_pos);
    }  /* if */
  } else if (!C_mode() && !reinterpret_semantics &&
             related_member_pointers(old_type, new_type, &baseward_cast,
                                     &bcp)) {
    /* C++ cast from pointer-to-member to pointer-to-member-of-related-class.
       Note that the underlying member types may be different. */
    if (baseward_cast) {
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
                                 check_cast_access, is_implicit_cast,
                                 p_node, err_pos);
    }  /* if */
  } else if ((!C_mode() || c99_mode) && is_bool_type(new_type)) {
    /* Cast to bool.  Use eok_bool_cast. */
    *p_node = make_operator_node((an_expr_operator_kind)eok_bool_cast,
                                 new_type, *p_node);
    (*p_node)->variant.operation.compiler_generated = is_implicit_cast;
    if (c99_mode) {
      /* Lowering will not be invoked, but we want to nonetheless reduce the
         cast to a comparison to ensure correct behavior on traditional C
         compilers. */
      transform_bool_cast(*p_node);
    }  /* if */
  } else {
    /* For an ordinary cast, generate the eok_cast node. */
    *p_node = make_operator_node((an_expr_operator_kind)eok_cast, new_type,
                                 *p_node);
    (*p_node)->variant.operation.compiler_generated = is_implicit_cast;
    (*p_node)->variant.operation.is_reinterpret_cast = is_reinterpret_cast;
  }  /* if */
}  /* add_cast_to_node */


/*
Test an expression node to see if it's a bit-field extraction.
*/
#define is_bit_field_extract_node(node) \
  (is_operation_node(node) && \
   ((node)->variant.operation.kind == \
                                (an_expr_operator_kind)eok_value_bit_field || \
    (node)->variant.operation.kind == \
                               (an_expr_operator_kind)eok_extract_bit_field))


void cast_node(an_expr_node_ptr  *p_node,
               a_type_ptr        new_type,
               a_boolean         check_cast_access,
               a_boolean         is_implicit_cast,
               a_boolean         is_reinterpret_cast,
               a_boolean         reinterpret_semantics,
               a_source_position *err_pos)
/*
Change the type of a node.  If the node is a constant, a conversion is done on
the constant value; otherwise a cast operator is added on top of the
node.  Check access on the cast if check_cast_access is TRUE.  If
is_implicit_cast is TRUE, this is an implicit cast rather than an
explicit one.  Warnings about truncation etc. are issued only if
is_implicit_cast is TRUE.  is_reinterpret_cast is TRUE if this is
a reinterpret_cast in the source; reinterpret_semantics is TRUE if the
behavior is the same as a reinterpret_cast (without necessarily having that
construct appear in the source).  *err_pos gives the source position for
errors.  The current expression is assumed to be a nonconstant
expression; if it were a constant expression, we wouldn't have an
expression node.  It's also assumed to be an evaluated expression (for
purposes of error diagnosis).  The caller must have already determined
that the conversion is allowed, except for casts to ambiguous or
inaccessible base classes.  This routine does not handle user-defined
conversions.
*/
{
  a_constant       local_constant;
  a_boolean        did_not_fold;
  a_boolean        need_cast;
  an_expr_node_ptr node = *p_node;

  /* Drop any qualifiers on the destination type, as appropriate. */
  new_type = rvalue_type(new_type);
  /* See whether the cast is actually needed.  Implicit casts that do
     not change the type, for example, are not needed. */
  if (!il_identical_types(node->type, new_type)) {
    /* A cast that changes the type is needed. */
    need_cast = TRUE;
  } else if (is_bit_field_extract_node(node)) {
    /* Don't allow dropping a cast to the same type over a bit-field
       extraction node, because the node with the cast has different
       integral promotion behavior. */
    need_cast = TRUE;
  } else if (is_operation_node(node) &&
             node->variant.operation.kind ==
                                     (an_expr_operator_kind)eok_dynamic_cast &&
             is_reference_type(node->type) != is_reference_type(new_type)) {
    /* Don't allow an implicit change of a dynamic-cast-to-reference to
       a dynamic-cast-to-pointer, because the runtime semantics are
       different. */
    need_cast = TRUE;
  } else if (!is_implicit_cast) {
    /* Do-nothing explicit casts are preserved in some configurations. */
    need_cast = PRESERVE_EFFECTLESS_EXPLICIT_CASTS_IN_IL;
  } else {
    /* Do-nothing implicit casts are not needed. */
    need_cast = FALSE;
  }  /* if */
  if (!need_cast) {
    /* We don't need to add a cast. */
    if (!is_implicit_cast) {
      /* For an explicit cast, put the new type in the node (since it may
         be "identical" but not exactly the same). */
      node->type = new_type;
      if (is_operation_node(node) &&
          node->variant.operation.kind == (an_expr_operator_kind)eok_cast &&
          node->variant.operation.compiler_generated) {
        /* An explicit cast over an equivalent implicit cast, and we wouldn't
           otherwise keep the new cast.  Turn the old cast into an explicit
           cast. */
        node->variant.operation.compiler_generated = FALSE;
      }  /* if */
    }  /* if */
  } else if (m_is_error_type(new_type)) {
    /* Casting to an error type changes the node to an error node. */
    *p_node = error_node();
  } else {
    /* Casting a node to a class type is not allowed. */
    check_assertion_str(!is_class_struct_union_type(new_type),
                        "cast_node: cast to class type");
    did_not_fold = TRUE;
    if (is_constant_node(node)) {
      /* Copy the constant to a local copy.  Type-change the local constant
         and copy the result to a new constant.  Since we are in a
         nonconstant context, reduce any error to a warning and leave
         the conversion to be done at runtime. */
      copy_constant(node->variant.constant, &local_constant);
      type_change_constant(&local_constant, new_type, is_implicit_cast,
                           /*constant_context=*/FALSE,
                           /*evaluated_context=*/TRUE,
                           /*fold_constant_addr_exprs=*/FALSE,
                           reinterpret_semantics,
                           /*maintain_expression=*/FALSE,
                           &did_not_fold, err_pos);
    }  /* if */
    if (did_not_fold) {
      /* The operand is not constant.  Put in a cast. */
      /* Note that if the constant type-change was attempted, it was
         done on a copy of the constant.  The original constant and
         expression were not changed, and therefore can be used here. */
      add_cast_to_node(p_node, new_type, check_cast_access, is_implicit_cast,
                       is_reinterpret_cast, reinterpret_semantics, err_pos);
    } else {
      /* The operation was successfully folded to a constant. */
      node->variant.constant = alloc_shareable_constant(&local_constant);
      node->variant.constant->is_reinterpret_cast = is_reinterpret_cast;
      node->type = new_type;
    }  /* if */
  }  /* if */
}  /* cast_node */


void prep_generic_nontype_template_argument(an_operand *operand)
/*
The nontype template argument indicated by "operand" is going to be saved
in the template argument list for an unknown template in a prototype
instantiation.  Process it so it can be turned into a constant
and saved in the IL with enough information to recover whether it
was an lvalue or rvalue, etc.
*/
{
  prep_generic_operand(operand, /*lvalue_expected=*/FALSE);
  if (is_error_operand(operand)) {
    /* Note that is_error_operand returns TRUE if the type is error, so
       make sure we have an actual error operand. */
    conv_to_error_operand(operand);
  } else if (is_constant_operand(operand) && is_an_rvalue(operand)) {
    /* The operand is a constant rvalue, which we can use directly. */
  } else {
    /* The argument is something more complicated, e.g., an expression.
       Make an expression and put it under a tpck_expression constant. */
    an_expr_node_ptr expr = make_node_from_operand(operand);
    make_template_param_expr_constant_operand(expr, operand);
  }  /* if */
}  /* prep_generic_nontype_template_argument */


void prep_generic_template_argument_list(a_template_arg_ptr template_arg_list)
/*
The template argument list pointed to by template_arg_list is going to be
saved as the template argument list for an unknown template in a prototype
instantiation.  Go through it and add constants for any arg_operand entries
so it can go into the IL.
*/
{
  a_template_arg_ptr tap;

  for (tap = template_arg_list; tap != NULL; tap = tap->next) {
    if (tap->arg_operand != NULL) {
      /* A template argument in arg_operand form. */
      an_operand             *operand = &tap->arg_operand->operand;
      a_constant             constant;
      a_memory_region_number region_to_switch_back_to;

      prep_generic_nontype_template_argument(operand);
      /* Fetch the constant and use it as the template argument. */
      extract_constant_from_operand(operand, &constant);
      switch_to_file_scope_region(&region_to_switch_back_to);
      tap->variant.constant = alloc_shareable_constant(&constant);
      switch_back_to_original_region(region_to_switch_back_to);
    } else if (tap->kind == (a_templ_arg_kind)tak_type) {
      /* Eliminate any local or nonreal typedefs. */
      tap->variant.type = strip_local_and_nonreal_typedefs(tap->variant.type);
    }  /* if */
  }  /* if */
}  /* prep_generic_template_argument_list */


void make_unknown_dependent_function_operand(
                                          a_symbol_ptr       sym,
                                          a_boolean          is_template_id,
                                          a_template_arg_ptr template_arg_list,
                                          a_boolean          is_qualified_name,
                                          an_operand         *operand)
/*
Make an operand for the address of an unknown function from the set
of overloaded functions indicated by sym.  This is used in prototype
instantiations when the function to be selected is not known.  If the
function name is followed by a list of explicit template arguments,
is_template_id is TRUE and template_arg_list gives the argument list.
is_qualified_name is TRUE if the source form used a qualified name.
*/
{
  a_symbol_ptr unk_sym = find_unknown_function_symbol(sym, is_qualified_name);

  if (!is_template_id) {
    /* The symbol is a constant whose value is the "address" of the
       unknown function. */
    make_sym_constant_operand(unk_sym, operand);
  } else {
    /* The function name has an explicit template argument list.  Record
       it in a tpck_template_ref constant that points to the constant for
       the template (from sym). */
    a_constant con;
    clear_constant(&con, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(&con,
                            (a_template_param_constant_kind)tpck_template_ref);
    check_assertion(unk_sym->kind == (a_symbol_kind)sk_constant &&
                    in_file_scope(unk_sym->variant.constant));
    con.variant.template_param.variant.template_ref.con =
                                                     unk_sym->variant.constant;
    con.variant.template_param.variant.template_ref.arg_list=template_arg_list;
    con.type = type_of_unknown_templ_param_nontype;
    prep_generic_template_argument_list(template_arg_list);
    make_constant_operand(&con, operand);
  }  /* if */
}  /* make_unknown_dependent_function_operand */


void cast_operand(a_type_ptr new_type,
                  an_operand *operand,
                  a_boolean  check_cast_access,
                  a_boolean  is_implicit_cast,
                  a_boolean  is_reinterpret_cast,
                  a_boolean  reinterpret_semantics)
/*
Cast the operand to the new type.  Check access on the cast if
check_cast_access is TRUE.  If is_implicit_cast is TRUE, this is an
implicit cast rather than an explicit one.  If there are any warnings
detected on the type change, issue them only if is_implicit_cast is TRUE.
If is_reinterpret_cast is TRUE, this cast appeared as reinterpret_cast in
the source.  If reinterpret_semantics is TRUE, the operation has the same
meaning as a reinterpret_cast, but it may come from another construct.
The operand must be an rvalue or error operand.  The caller must have
already determined that the conversion is allowed, except for casts to
ambiguous or inaccessible base classes.  This routine does not handle
user-defined conversions.
*/
{
  a_boolean         did_not_fold, access_error_reported, ambiguous;
  a_constant        local_constant;
  an_expr_node_ptr  node;
  an_operand        orig_operand;
  a_symbol_ptr      overloaded_function_symbol, function_symbol;
  an_arg_match_level
                    match_level;
  a_std_conv_descr  std_conversion;
  a_boolean         ptr_to_member_case, unknown_dependent_function;

#if CHECKING
  if (!is_an_rvalue(operand) && !is_error_operand(operand)) {
    internal_error("cast_operand: operand is not an rvalue");
  }  /* if */
#endif /* CHECKING */

  /* Drop any qualifiers on the destination type, as appropriate. */
  new_type = rvalue_type(new_type);
  /* Save the operand's source position, etc. */
  orig_operand = *operand;
  if (m_is_error_type(new_type)) {
    /* Casting to an error type.  Produce an error operand. */
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
        cast_node(&node, new_type, check_cast_access, is_implicit_cast,
                  is_reinterpret_cast, reinterpret_semantics,
                  &operand->position);
        make_expression_operand(node, new_type, operand);
        break;
      case ok_constant:
        /* Cast the constant by changing its type.  In a nonconstant
           context, reduce any error to a warning and leave the
           conversion to be done at runtime. */
        did_not_fold = TRUE;
        copy_constant(&operand->variant.constant, &local_constant);
        type_change_constant(&local_constant, new_type, is_implicit_cast,
                             curr_expr_kind_is_const(),
                             curr_expr_is_evaluated(),
                             (a_boolean)expr_stack->fold_constant_addr_exprs,
                             reinterpret_semantics,
                             /*maintain_expression=*/FALSE, /* Done below */
                             &did_not_fold, &operand->position);
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
            add_cast_to_node(&node, new_type, check_cast_access,
                             is_implicit_cast, is_reinterpret_cast,
                             reinterpret_semantics, &operand->position);
            make_expression_operand(node, new_type, operand);
          }  /* if */
        } else {
          /* The operation was successfully folded to a constant. */
          /* Mark the constant as the result of a reinterpret_cast if it
             is.  Don't clear the flag once it gets set (an implicit cast
             after a reinterpret_cast still counts as a reinterpret_cast). */
          local_constant.is_reinterpret_cast |= is_reinterpret_cast;
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
          if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
            an_expr_node_ptr orig_expr = operand->variant.constant.expr;
            local_constant.expr = orig_expr;
            if (!is_implicit_cast || operand->type != new_type) {
              /* Record a cast expression for the constant (inhibit normal
                 diagnostics during that process, since they were already
                 issued). */
              an_error_severity  saved_error_threshold = error_threshold;

              error_threshold = es_catastrophe;
              if (local_constant.expr == NULL) {
                /* The cast is applied to a simple constant that contains no
                   operations.  Create an expression node to which the cast
                   history can be attached. */
                local_constant.expr = make_node_from_operand(operand);
              }  /* if */
              add_cast_to_node(&local_constant.expr, new_type,
                               check_cast_access, is_implicit_cast,
                               is_reinterpret_cast, reinterpret_semantics,
                               &operand->position);
              error_threshold = saved_error_threshold;
            }  /* if */
          }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
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
                                                   (a_boolean)operand->
                                                                is_template_id,
                                                   operand->template_arg_list,
                                                   new_type,
                                                   /*is_cast=*/
                                                             !is_implicit_cast,
                                                   &match_level,
                                                   &std_conversion,
                                                   &unknown_dependent_function,
                                                   &ambiguous);
        if (unknown_dependent_function) {
          /* The cast is in a prototype instantiation, and we don't know
             which function is selected. */
          make_unknown_dependent_function_operand(overloaded_function_symbol,
                                                  (a_boolean)operand->
                                                                is_template_id,
                                                  operand->template_arg_list,
                                                  (a_boolean)operand->
                                                             is_qualified_name,
                                                  operand);
        } else {
#if CHECKING
          if (function_symbol == NULL) {
            internal_error("cast_operand: bad func symbol");
          }  /* if */
#endif /* CHECKING */
          ptr_to_member_case = is_ptr_to_member_type(new_type);
          /* Do whatever would have been done with the function if we had
             known all along which function was intended.  Make an operand
             for the specific function's address, except for the
             pointer to member case. */
          overloaded_function_catch_up(function_symbol,
                                       overloaded_function_symbol,
                                       (a_boolean)operand->is_qualified_name,
                                       &orig_operand.position,
                                       &orig_operand.id_position,
                                       /*elided_reference=*/FALSE,
                                       /*address_taken=*/TRUE,
                                       ptr_to_member_case ? (an_operand *)NULL:
                                                            operand,
                                       &access_error_reported);
          if (ptr_to_member_case) {
            /* Make an operand for the pointer-to-member case. */
            make_ptr_to_member_constant_operand(fundamental_symbol_of(
                                                              function_symbol),
                                                overloaded_function_symbol,
                                                &orig_operand.position,
                                                !access_error_reported,
                                                (a_boolean)operand->
                                                      is_qualified_name,
                                                (a_boolean)operand->
                                                      is_operand_of_address_of,
                                                operand);
          }  /* if */
        }  /* if */
        /* If the pointer to member is to a related class, or the pointer
           to function differs because of a conversion (e.g., a C++ vs.
           C linkage on the function type), adjust the operand. */
        if (operand->type != new_type) {
          cast_operand(new_type, operand, check_cast_access,
                       is_implicit_cast,
                       is_reinterpret_cast, reinterpret_semantics);
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
                             a_boolean        check_cast_access,
                             a_boolean        is_implicit_cast,
                             a_boolean        implicit_in_naming,
                             a_boolean        is_object_pointer)
/*
Cast operand (of class or pointer-to-class type) to its base class
identified by bcp.  If *is_arrow_operator is TRUE, operand is being
used as a pointer ("->"); otherwise, it is being used as an object (".").
*is_arrow_operator will be set to TRUE on return to indicate that the
operation was normalized into "->" form.  Alternatively, if the caller
passes is_arrow_operator == NULL, the operation is assumed to be in
pointer form.  Do access control checking on the cast if
check_cast_access is TRUE.  The cast is implicit if is_implicit_cast
is TRUE.  implicit_in_naming is TRUE for casts that are generated
implicitly in referencing a member of a class (roughly, in getting
from the name used in the source -- the projection symbol -- to the
member actually used in the IL).  is_object_pointer is TRUE if the
pointer is asserted to be an object pointer (meaning it points at an
object and is not a null pointer, though in fact the reason for this
flag has to do with using 0 as a pointer in the usual version of
the offsetof macro).  This routine is only used in C++ mode.
*/
{
  a_boolean        did_not_fold;
  an_expr_node_ptr node;
  a_constant       temp_con;
  an_operand       orig_operand;

  /* Save the original operand position, etc. */
  orig_operand = *operand;
  if (is_arrow_operator != NULL) {
    /* Convert to "->" form by getting an address for the operand. */
    conv_selector_to_object_pointer(operand, is_arrow_operator);
  }  /* if */
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
                           &temp_con, check_cast_access,
                           is_object_pointer, &did_not_fold,
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
                             check_cast_access, is_implicit_cast,
                             implicit_in_naming,
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


static a_type_ptr type_after_bit_field_integral_promotion(
                                                         an_expr_node_ptr node,
                                                         a_type_ptr       type)
/*
node is a bit-field selection operation (lvalue or rvalue).  type is the
rvalue type of the field selection (it differs from node->type in the lvalue
case).  Determine the type that would result from applying the integral
promotions to the selection type.  Return the promoted type, which may be
the same as the original type.  The node is not actually promoted; it is
up to the caller to do the cast if desired.
*/
{
  a_type_ptr      promoted_type;
  a_field_ptr     field;
  an_integer_kind ikind, orig_ikind;

  db_enter(4, "type_after_bit_field_integral_promotion");
  field = node->variant.operation.operands->next->variant.field;
  promoted_type = skip_typerefs(type);
#if CHECKING
  /* The type of a bit-field should be integral. */
  if (promoted_type->kind != (a_type_kind)tk_integer) {
    internal_error(
            "type_after_bit_field_integral_promotion: bit-field not integral");
  }  /* if */
  if (field->bit_size >
               (unsigned int)(TARG_SIZEOF_LARGEST_INTEGER*targ_char_bit)) {
    internal_error(
                 "type_after_bit_field_integral_promotion: bit-field too big");
  }  /* if */
#endif /* CHECKING */
  orig_ikind = ikind = promoted_type->variant.integer.int_kind;
  if (field->bit_field_is_signed) {
    /* Bit-field is signed, so it is promoted to the first of int or
       long into which all its values will fit. */
#if LONG_LONG_ALLOWED
    /* ... or long long. */
#endif /* LONG_LONG_ALLOWED */
    if (field->bit_size <= (unsigned int)(targ_sizeof_int*targ_char_bit)) {
      ikind = (an_integer_kind)ik_int;
    } else {
#if LONG_LONG_ALLOWED
      if (field->bit_size <= (unsigned int)(targ_sizeof_long*targ_char_bit)) {
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
    if (field->bit_size < (unsigned int)(targ_sizeof_int*targ_char_bit)) {
      ikind = (an_integer_kind)ik_int;
    } else if (field->bit_size ==
                               (unsigned int)(targ_sizeof_int*targ_char_bit)) {
      ikind = (an_integer_kind)ik_unsigned_int;
    } else if (field->bit_size <
                              (unsigned int)(targ_sizeof_long*targ_char_bit)) {
      ikind = (an_integer_kind)ik_long;
    } else {
#if LONG_LONG_ALLOWED
      if (field->bit_size == (unsigned int)(targ_sizeof_long*targ_char_bit)) {
#endif /* LONG_LONG_ALLOWED */
        ikind = (an_integer_kind)ik_unsigned_long;
#if LONG_LONG_ALLOWED
      } else if (field->bit_size <
                         (unsigned int)(targ_sizeof_long_long*targ_char_bit)) {
        ikind = (an_integer_kind)ik_long_long;
      } else {
        ikind = (an_integer_kind)ik_unsigned_long_long;
      }  /* if */
#endif /* LONG_LONG_ALLOWED */
    }  /* if */
  }  /* if */
  if (ikind != orig_ikind) promoted_type = integer_type(ikind);
  db_exit();
  return promoted_type;
}  /* type_after_bit_field_integral_promotion */


a_type_ptr operand_type_after_integral_promotion(an_operand *operand)
/*
Determine the type that would result from applying the integral promotions
(3.2.1.1) to *operand.  Return the promoted type, which may be
the same as the original type.  The operand may be an rvalue or an lvalue;
if it is an lvalue, the type returned is the promoted version of the
rvalue type for that lvalue (i.e., some cv-qualifiers are dropped even
if the type is not integral).
*/
{
  a_type_ptr promoted_type = NULL;

  /* Check for bit-field accesses, which require special handling.
     The special processing is not done in pcc mode. */
  if (C_dialect != C_dialect_pcc && is_expression_operand(operand)) {
    an_expr_node_ptr node = operand->variant.expression;
    if (is_an_rvalue(operand)) {
      if (is_bit_field_extract_node(node)) {
        promoted_type = type_after_bit_field_integral_promotion(node,
                                                                node->type);
      }  /* if */
    } else if (is_an_lvalue(operand)) {
      /* An lvalue. */
      if (is_operation_node(node) &&
          node->variant.operation.kind ==
                                        (an_expr_operator_kind)eok_bit_field) {
        a_type_ptr sel_type = rvalue_type(operand->type);
        promoted_type = type_after_bit_field_integral_promotion(node,
                                                                sel_type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (promoted_type == NULL) {
    /* Non-bit-field cases.  Determine the promoted type on the basis of
       the operand type. */
    promoted_type = operand->type;
    if (is_an_lvalue(operand)) {
      /* For an lvalue, use the type it would have if it were an rvalue. */
      promoted_type = rvalue_type(promoted_type);
    }  /* if */
    promoted_type = type_after_integral_promotion(promoted_type);
  }  /* if */
  return promoted_type;
}  /* operand_type_after_integral_promotion */


static a_type_ptr node_type_after_integral_promotion(an_expr_node_ptr node)
/*
Determine the type that would result from applying the integral promotions
to the indicated expression.  Return the promoted type, which may be the
same as the original type.  The expression is an rvalue.
*/
{
  a_type_ptr promoted_type;

  /* Check for bit-field accesses, which require special handling.
     The special processing is not done in pcc mode. */
  if (C_dialect != C_dialect_pcc &&
      is_bit_field_extract_node(node)) {
    promoted_type = type_after_bit_field_integral_promotion(node,
                                                            node->type);
  } else {
    promoted_type = type_after_integral_promotion(node->type);
  }  /* if */
  return promoted_type;
}  /* node_type_after_integral_promotion */


void promote_operand(an_operand *operand)
/*
Determine the integral promotion and do the promotion on an operand.
See 3.2.1.1 in the standard.  The operand must be an rvalue.
*/
{
  cast_operand(operand_type_after_integral_promotion(operand), operand,
               /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
               /*is_reinterpret_cast=*/FALSE, /*reinterpret_semantics=*/FALSE);
}  /* promote_operand */


static void conv_array_rvalue_operand_to_pointer_operand(an_operand *operand)
/*
operand is an array rvalue in C mode.  Convert it to a pointer to the
first element of the array.  This is nonstandard.  A diagnostic is issued
in strict mode.
*/
{
  if (strict_ansi_mode) {
    pos_diagnostic(strict_ansi_discretionary_severity,
                   ec_bad_rvalue_array, &operand->position);
  }  /* if */
  /* Convert the array rvalue to a pointer to the first element. */
  conv_array_rvalue_to_lvalue(operand);
  conv_array_operand_to_pointer_operand(operand);
}  /* conv_array_rvalue_operand_to_pointer_operand */


void arg_default_promote_operand(an_operand *argument_operand)
/*
Do default argument promotions on an argument operand.  If the operand is
an lvalue, it is converted to an rvalue before doing the promotions.
*/
{
  a_type_ptr arg_type;

  /* Convert the operand to an rvalue if necessary. */
  do_operand_transformations(argument_operand, TOPT_NO_OPTIONS);
  arg_type = argument_operand->type;
  if (C_mode() && is_array_type(arg_type)) {
    /* Catch array rvalues in C.  In strict mode they are not allowed.
       Otherwise, do the special array --> pointer decay as an extension. */
    check_assertion(is_an_rvalue(argument_operand));
    conv_array_rvalue_operand_to_pointer_operand(argument_operand);
    arg_type = argument_operand->type;
  }  /* if */
  /* Do the integral promotions part of the default argument promotions
     directly on the operand because of the special case with 
     bit-fields (which can't be handled from just the type). */
  if (is_integral_or_enum_type(arg_type)) {
    promote_operand(argument_operand);
  } else if (is_incomplete_type(arg_type)) {
    /* Catch a case like "f((void)2)" -- an argument with an incomplete
       type is not allowed. */
    error_in_operand(ec_incomplete_type_not_allowed, argument_operand);
  } else if (is_class_struct_union_type(arg_type)) {
    /* Class.  No promotion needed. */
#if USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS
    /* If configured that way, call the copy constructor for a class object,
       and pass the address of the temporary into which the copy was placed.
       This falls under undefined behavior.  The Sun CC compiler uses the
       copy constructor in this case. */
    if (!C_mode()) {
      prep_arg_passed_via_copy_constructor(argument_operand, arg_type,
                                           (a_conv_descr *)NULL,
                                           ec_no_suitable_copy_constructor);
    }  /* if */
#endif /* USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS */
  } else {
    cast_operand(default_argument_promotion(arg_type),
                 argument_operand, /*check_cast_access=*/TRUE,
                 /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
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


static void build_question_result_operand(an_operand *operand_1,
                                          an_operand *operand_2,
                                          an_operand *operand_3,
                                          a_type_ptr result_type,
                                          an_operand *result)
/*
Build an operand for the expression that is the operator "?" operating on
operand_1, operand_2, and operand_3, with result type result_type.
*/
{
  /* Make an operator node with the first part of the expression. */
  build_unary_result_operand(operand_1,
                             (an_expr_operator_kind)eok_question,
                             result_type, result);
  /* Now link the other two operands from this one. */
  result->variant.expression->variant.operation.operands->next =
                                             make_node_from_operand(operand_2);
  result->variant.expression->variant.operation.operands->next->next =
                                             make_node_from_operand(operand_3);
}  /* build_question_result_operand */


/* Type predicates used by determine_arithmetic_conversions_full. */

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


static a_float_kind promoted_float_kind(a_float_kind  fkind_1,
                                        a_float_kind  fkind_2)
/*
fkind_1 and fkind_2 represent the precision of a floating-point types
involved in a binary operation (or fk_last if the corresponding operand
does not have a floating-point type).  Return the precision of the
result (i.e., the precision to which both operands should be promoted).
If both fkind_1 and fkind_2 are fk_last, then fk_last is returned.
*/
{
  a_float_kind result = (a_float_kind)fk_last;
  if (is_long_double(fkind_1) || is_long_double(fkind_2)) {
    /* If either operand has type "long double", the other operand is
       converted to "long double". */
    result = (a_float_kind)fk_long_double;
  } else if (is_double(fkind_1) || is_double(fkind_2)) {
    /* If either operand has type "double", the other operand is converted to
       "double". */
    result = (a_float_kind)fk_double;
  } else if (is_float(fkind_1) || is_float(fkind_2)) {
    /* If either operand has type "float", the other operand is converted to
       "float". */
    if (C_dialect == C_dialect_pcc) {
      /* When in pcc mode, all float operations are done as double. */
      result = (a_float_kind)fk_double;
    } else {
      result = (a_float_kind)fk_float;
    }  /* if */
  }  /* if */
  return result;
}  /* promoted_float_kind */


static a_type_ptr determine_arithmetic_conversions_full(
                                                 an_operand *operand_1,
                                                 a_type_ptr operand_1_type,
                                                 an_operand *operand_2,
                                                 a_type_ptr operand_2_type)
/*
Determine the "usual arithmetic conversions" on the operands to make them
compatible, and return the type of the result.  Note that this routine assumes
that the type is arithmetic, and does not actually change the result type.
See section 6.1.2.5 of the ISO C89 standard.  In C++, when wchar_t is a
keyword, wchar_t is represented by one of the normal integral types and
obeys the same conversion rules as its underlying type.  Likewise for bool.
The operands can be lvalues or rvalues.  If operand_1 is available only
as a type, operand_1 == NULL and operand_1_type indicates the type.
Likewise for operand_2/operand_2_type.
*/
{
  a_type_ptr      type_1 = (operand_1 != NULL) ? operand_1->type :
                                                 operand_1_type;
  a_type_ptr      type_2 = (operand_2 != NULL) ? operand_2->type :
                                                 operand_2_type;
  a_type_ptr      result_type;
  an_integer_kind ikind_1, ikind_2;

  db_enter(4, "determine_arithmetic_conversions_full");

  if (is_error_type(type_1) || is_error_type(type_2)) {
    result_type = error_type();
  } else {
    a_float_kind    fkind_1, fkind_2, result_fkind;
    /* Get past possible typerefs. */
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);

    fkind_1 = is_floating_type(type_1) ? type_1->variant.float_kind :
                                         (a_float_kind)fk_last;
    fkind_2 = is_floating_type(type_2) ? type_2->variant.float_kind :
                                         (a_float_kind)fk_last;
    result_fkind = promoted_float_kind(fkind_1, fkind_2);
    if (result_fkind != fk_last) {
      /* One of the operands had a (possibly complex) floating-point type. */
#if C99_IL_EXTENSIONS_SUPPORTED
      if (type_1->kind == (a_type_kind)tk_complex ||
          type_2->kind == (a_type_kind)tk_complex) {
        /* If either operand has a complex type, the domain of the result is
           also "_Complex".  The "_Imaginary" case requires operator-specific
           treatment, however. */
        result_type = complex_type(result_fkind);
      } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* Do not insert code here. */
      {
        result_type = float_type(result_fkind);
      }  /* if */
    } else {
      /* Neither operand had type float; do the integral promotions on both
	 operands and try to get the result type from that. */
      if (operand_1 != NULL) {
        type_1 = operand_type_after_integral_promotion(operand_1);
      } else {
        type_1 = type_after_integral_promotion(type_1);
      }  /* if */
      type_1 = skip_typerefs(type_1);
      if (operand_2 != NULL) {
        type_2 = operand_type_after_integral_promotion(operand_2);
      } else {
        type_2 = type_after_integral_promotion(type_2);
      }  /* if */
      type_2 = skip_typerefs(type_2);

      ikind_1 = is_integral_or_enum_type(type_1) ?
                  type_1->variant.integer.int_kind : (an_integer_kind)ik_last;
      ikind_2 = is_integral_or_enum_type(type_2) ?
                  type_2->variant.integer.int_kind : (an_integer_kind)ik_last;
#if LONG_LONG_ALLOWED
      if (is_unsigned_long_long(ikind_1) || is_unsigned_long_long(ikind_2)) {
        /* If either operand has type "unsigned long long", the other operand
           is converted to "unsigned long long". */
        result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
        goto done;
      }  /* if */
      if (is_long_long(ikind_1) || is_long_long(ikind_2)) {
        /* At least one operand has type "long long". */
        if (targ_sizeof_long_long == targ_sizeof_long) {
          /* A long long cannot represent all unsigned long values, so check
             for unsigned long values. */
          if (is_unsigned_long(ikind_1) || is_unsigned_long(ikind_2)) {
            /* One operand has type "long long" and the other has type
               "unsigned long", and all values of "unsigned long" cannot be
               represented by "long long", so both operands are converted to
               "unsigned long long". */
            result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
            goto done;
          }  /* if */
        }  /* if */
        if (targ_sizeof_long_long == targ_sizeof_int) {
          /* A long long cannot represent all unsigned int values, so check for
             unsigned int values. */
          if (is_unsigned_int(ikind_1) || is_unsigned_int(ikind_2)) {
            /* One operand has type "long long" and the other has type
               "unsigned int", and all values of "unsigned int" cannot be
               represented by "long long", so both operands are converted to
               "unsigned long long". */
            result_type = integer_type((an_integer_kind)ik_unsigned_long_long);
            goto done;
          }  /* if */
        }  /* if */
        /* One operand has type "long long"; the other operand is
           converted to "long long". */
        result_type = integer_type((an_integer_kind)ik_long_long);
        goto done;
      }  /* if */
#endif /* LONG_LONG_ALLOWED */
      if (is_unsigned_long(ikind_1) || is_unsigned_long(ikind_2)) {
        /* If either operand has type "unsigned long", the other operand is
	   converted to "unsigned long". */
        result_type = integer_type((an_integer_kind)ik_unsigned_long);
      } else if (is_long(ikind_1) || is_long(ikind_2)) {
        /* At least one operand has type "long". */
        if (long_preserving_rules) {
          /* In K&R I, Appendix A, 6.6, there is no special case based on
             the size of long vs. int, so "long + unsigned int" has a
             result type of long even if long and int have the same size. */
        } else if (targ_sizeof_long == targ_sizeof_int) {
          /* A "long" cannot represent all "unsigned int" values, so check for
             "unsigned int" values. */
          if (is_unsigned_int(ikind_1) || is_unsigned_int(ikind_2)) {
            /* One operand has type "long" and the other has type
               "unsigned int", and all values of "unsigned int" cannot be
               represented by "long", so both operands are converted to
               "unsigned long". */
            result_type = integer_type((an_integer_kind)ik_unsigned_long);
            goto done;
          }  /* if */
        }  /* if */
        /* One operand has type "long"; the other operand is
           converted to "long". */
        result_type = integer_type((an_integer_kind)ik_long);
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
done:;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && is_integral_or_enum_type(result_type)) {
    /* Preserve the microsoft_sized_int_type flag in the result type if
       it is there in the operand types.  That prevents returning, e.g.,
       long long when the right answer is __int64. */
    a_boolean  result_is_microsoft_sized_int = FALSE;
    a_type_ptr base_result_type = skip_typerefs(result_type);
    a_type_ptr base_type_1 = skip_typerefs(type_1);
    a_type_ptr base_type_2 = skip_typerefs(type_2);
    a_boolean  type_1_same_int_as_result = 
                            (is_integral_or_enum_type(base_type_1) &&
                             base_type_1->variant.integer.int_kind ==
                                   base_result_type->variant.integer.int_kind);
    a_boolean  type_2_same_int_as_result = 
                            (is_integral_or_enum_type(base_type_2) &&
                             base_type_2->variant.integer.int_kind ==
                                   base_result_type->variant.integer.int_kind);
    if (type_1_same_int_as_result && type_2_same_int_as_result) {
      if (base_type_1->variant.integer.microsoft_sized_int_type &&
          base_type_2->variant.integer.microsoft_sized_int_type) {
        result_is_microsoft_sized_int = TRUE;
      }  /* if */
    } else if (type_1_same_int_as_result) {
      if (base_type_1->variant.integer.microsoft_sized_int_type) {
        result_is_microsoft_sized_int = TRUE;
      }  /* if */
    } else if (type_2_same_int_as_result) {
      if (base_type_2->variant.integer.microsoft_sized_int_type) {
        result_is_microsoft_sized_int = TRUE;
      }  /* if */
    }  /* if */
    if (result_is_microsoft_sized_int) {
      result_type = microsoft_sized_integer_type(
                                   base_result_type->variant.integer.int_kind);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_exit();
  return result_type;
}  /* determine_arithmetic_conversions_full */


a_type_ptr determine_arithmetic_conversions(an_operand *operand_1,
                                            an_operand *operand_2)
/*
Determine the "usual arithmetic conversions" on the operands to make them
compatible, and return the type of the result.  This is an interface to
determine_arithmetic_conversions_full for a simple case; see the header
comment of that routine for details.
*/
{
  a_type_ptr result_type;

  result_type = determine_arithmetic_conversions_full(operand_1,
                                                      (a_type_ptr)NULL,
                                                      operand_2,
                                                      (a_type_ptr)NULL);
  return result_type;
}  /* determine_arithmetic_conversions */


a_type_ptr usual_arithmetic_conversions(a_type_ptr operand_1_type,
                                        a_type_ptr operand_2_type)
/*
Determine the "usual arithmetic conversions" on the indicated operand
types to make them compatible, and return the type of the result.  This
is an interface to determine_arithmetic_conversions_full for a simple
case; see the header comment of that routine for details.
*/
{
  a_type_ptr result_type;

  result_type = determine_arithmetic_conversions_full((an_operand *)NULL,
                                                      operand_1_type,
                                                      (an_operand *)NULL,
                                                      operand_2_type);
  return result_type;
}  /* usual_arithmetic_conversions */

#if C99_IL_EXTENSIONS_SUPPORTED

static void promote_operand_for_imaginary_operation(
                                              an_operand       *operand,
                                              a_float_kind  new_fkind,
                                              a_boolean        complex_domain)
/*
Promote the given operand to a floating-point type precision specified by
fkind.  If complex_domain is TRUE, the operand should also be promoted to
the complex domain; otherwise, the domain of the operand (real or imaginary)
should be preserved.
*/
{
  a_type_ptr  type = operand->type;
  a_float_kind  fkind = is_floating_type(type) ? type->variant.float_kind :
                                                 (a_float_kind)fk_last;

  check_assertion(type->kind != tk_complex);
  if (new_fkind != fkind || complex_domain) {
    a_type_ptr  promoted_type;
    if (complex_domain) {
      promoted_type = complex_type(new_fkind);
    } else if (type->kind == tk_imaginary) {
      promoted_type = imaginary_type(new_fkind);
    } else {
      promoted_type = float_type(new_fkind);
    }  /* if */
    cast_operand(promoted_type, operand, /*check_cast_access=*/FALSE,
                 /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
  }  /* if */
}  /* promote_operand_for_imaginary_operation */


void prepare_imaginary_operation(a_token_kind           op_token,
                                 an_operand             *operand_1,
                                 an_operand             *operand_2,
                                 a_type_ptr             *result_type,
                                 an_expr_operator_kind  *op)
/*
Operations on imaginary floating-point types are a little peculiar in the
sense that the result type is not necessarily a type to which both operands
(operand_1 and operand_2) are promoted.  Instead, one of the operands may be
promoted and the result type (returned through result_type) depends on the
particular operation (represented by op_token).  This routine also determines
the IL operator (*op) implementing the given arithmetic operation.
*/
{
  a_type_ptr    type_1 = skip_typerefs(operand_1->type),
                type_2 = skip_typerefs(operand_2->type);
  a_float_kind  fkind_1 = is_floating_type(type_1) ?
                           type_1->variant.float_kind : (a_float_kind)fk_last,
                fkind_2 = is_floating_type(type_2) ?
                           type_2->variant.float_kind : (a_float_kind)fk_last;
  a_float_kind  fkind_result = promoted_float_kind(fkind_1, fkind_2);
  a_boolean     type_1_is_imaginary =
                                  (type_1->kind == (a_type_kind)tk_imaginary),
                type_2_is_imaginary =
                                  (type_2->kind == (a_type_kind)tk_imaginary);

  check_assertion(type_1_is_imaginary || type_2_is_imaginary);
  /* Determine the appropriate IL operator and corresponding result type. */
  switch (op_token) {
    case tok_plus:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *op = (an_expr_operator_kind)eok_fadd;
        *result_type = imaginary_type(fkind_result);
      } else {
        *op = (an_expr_operator_kind)eok_xadd;
        *result_type = complex_type(fkind_result);
      }  /* if */
      break;
    case tok_minus:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *op = (an_expr_operator_kind)eok_fsubtract;
        *result_type = imaginary_type(fkind_result);
      } else {
        *op = (an_expr_operator_kind)eok_xsubtract;
        *result_type = complex_type(fkind_result);
      }  /* if */
      break;
    case tok_star:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *op = (an_expr_operator_kind)eok_jmultiply;
        *result_type = float_type(fkind_result);
      } else {
        *op = (an_expr_operator_kind)eok_fmultiply;
        *result_type = imaginary_type(fkind_result);
      }  /* if */
      break;
    case tok_divide:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      *op = (an_expr_operator_kind)eok_fdivide;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = float_type(fkind_result);
      } else {
        *result_type = imaginary_type(fkind_result);
      }  /* if */
      break;
    case tok_assign:
      fkind_result = fkind_1;
      *op = (an_expr_operator_kind)eok_fassign;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = type_1;
      } else {
        pos_ty2_error(ec_incompatible_assignment_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_plus_assign:
      fkind_result = fkind_1;
      *op = (an_expr_operator_kind)eok_fadd_assign;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = type_1;
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_minus_assign:
      fkind_result = fkind_1;
      *op = (an_expr_operator_kind)eok_fsubtract_assign;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = type_1;
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_times_assign:
      fkind_result = fkind_1;
      *op = (an_expr_operator_kind)eok_fmultiply_assign;
      if (type_1_is_imaginary && !is_nonreal_floating_type(type_2)) {
        *result_type = type_1;
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_divide_assign:
      fkind_result = fkind_1;
      *op = (an_expr_operator_kind)eok_fdivide_assign;
      if (type_1_is_imaginary && !is_nonreal_floating_type(type_2)) {
        *result_type = type_1;
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_eq:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      *op = (an_expr_operator_kind)eok_feq;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = imaginary_type(fkind_result);
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    case tok_ne:
      fkind_result = promoted_float_kind(fkind_1, fkind_2);
      *op = (an_expr_operator_kind)eok_fne;
      if (type_1_is_imaginary && type_2_is_imaginary) {
        *result_type = imaginary_type(fkind_result);
      } else {
        pos_ty2_error(ec_incompatible_operands,
                      &error_position, type_2, type_1);
        *result_type = error_type();
      }  /* if */
      break;
    default:
      error(ec_invalid_complex_operator);
      *result_type = error_type();
  }  /* switch */
  if (!is_error_type(*result_type)) {
    promote_operand_for_imaginary_operation(
     operand_1, fkind_result, (*result_type)->kind == (a_type_kind)tk_complex);
    promote_operand_for_imaginary_operation(
     operand_2, fkind_result, (*result_type)->kind == (a_type_kind)tk_complex);
  } else {
    *op = (an_expr_operator_kind)eok_error;
  }  /* if */
}  /* prepare_imaginary_operation */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if MICROSOFT_EXTENSIONS_ALLOWED

void adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                               an_operand *operand,
                                               a_boolean  *operand_is_constant,
                                               a_constant **operand_constant)
/*
In Microsoft mode, expressions like (x, 0) are allowed as null pointer
constants.  This routine examines *operand to see if it is such a thing,
and sets *operand_is_constant and *operand_constant to the underlying
null pointer constant.  Those are then presumably passed to a function
like impl_conversion_possible, which would then conclude that it is
possible to implicitly convert the *operand expression to a pointer type.
This routine is called only in Microsoft mode.
*/
{
  if (is_expression_operand(operand)) {
    an_expr_node_ptr expr = operand->variant.expression;
    if (is_operation_node(expr) &&
        expr->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
      /* The operand is a comma expression. */
      expr = expr->variant.operation.operands->next;
      if (is_constant_node(expr) &&
          is_or_might_be_null_pointer_constant(expr->variant.constant)) {
        /* The operand is a comma node with a second operand that is a
           null pointer constant, e.g., (x, 0). */
        *operand_is_constant = TRUE;
        *operand_constant = expr->variant.constant;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* adjust_constant_operand_info_for_microsoft_null_pointer_test */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
One or the other of the operands, or both, must have a pointer type.
Return the operation type in *operation_type.  (The operands are not cast
to the operation type; the caller must do that.)  operator_position gives the
operator position (for errors).  The other switches indicate the legality
of certain constructs in ANSI C.  "Illegal" constructs are accepted
anyway, but warnings are issued in strict ANSI C mode.  The switches are
used only in strict ANSI mode.  Return FALSE if there is an error.
*/
{
  a_boolean        okay = FALSE;
  a_type_ptr       operand_1_type = operand_1->type;
  a_type_ptr       operand_2_type = operand_2->type;
  a_type_ptr       local_operation_type;
  a_boolean        operand_1_is_pointer = is_pointer_type(operand_1_type);
  a_boolean        operand_2_is_pointer = is_pointer_type(operand_2_type);
  a_boolean        suppress_extensions;
  a_std_conv_descr std_conv;

  /* The loop here tries the conversions once without extensions
     allowed, and (if that fails) again with extensions allowed,
     so we won't pick a conversion direction that requires an
     extension when the opposite direction doesn't. */
  suppress_extensions = TRUE;
  for (;;) {
    if (operand_1_is_pointer) {
      a_boolean      operand_2_is_constant = is_constant_operand(operand_2);
      a_constant_ptr operand_2_constant    = &operand_2->variant.constant;

#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && !operand_2_is_constant) {
        /* Microsoft mode allows some expressions as null pointer constants. */
        adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                        operand_2,
                                                        &operand_2_is_constant,
                                                        &operand_2_constant);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* See if the second operand can be converted to the type of the
         first operand. */
      if (C_mode() &&
          operand_2_is_pointer &&
          ((is_constant_operand(operand_1) &&
            is_null_pointer_constant(&operand_1->variant.constant)) ||
           (is_void_type(type_pointed_to(operand_2_type)) &&
            !(is_constant_operand(operand_2) &&
              is_null_pointer_constant(&operand_2->variant.constant))))) {
        /* The first operand is a C null pointer constant of (void *)0.  Don't
           try to convert the second operand to void *, because the conversion
           in the other direction should be preferred.  Or, the second operand
           has type void *, but it's not a null pointer constant of (void *)0.
           Don't try to convert it to the type of the first operand, because
           the conversion in the other direction should be preferred. */
      } else if (impl_pointer_conversion(operand_2_type,
                                         operand_2_is_constant,
                                         (a_boolean)operand_2->
                                                      is_simple_string_literal,
                                         operand_2_constant,
                                         operand_1_type,
                                       /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                         suppress_extensions,
                                         ec_incompatible_operands,
                                         &std_conv)) {
        local_operation_type = operand_1_type;
        okay = TRUE;
        break;
      }  /* if */
    }  /* if */
    if (operand_2_is_pointer) {
      a_boolean      operand_1_is_constant = is_constant_operand(operand_1);
      a_constant_ptr operand_1_constant    = &operand_1->variant.constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && !operand_1_is_constant) {
        /* Microsoft mode allows some expressions as null pointer constants. */
        adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                        operand_1,
                                                        &operand_1_is_constant,
                                                        &operand_1_constant);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (impl_pointer_conversion(operand_1_type,
                                  operand_1_is_constant,
                                  (a_boolean)operand_1->
                                                      is_simple_string_literal,
                                  operand_1_constant,
                                  operand_2_type,
                                  /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                  suppress_extensions,
                                  ec_incompatible_operands,
                                  &std_conv)) {
        local_operation_type = operand_2_type;
        okay = TRUE;
        break;
      }  /* if */
    }  /* if */
    if (suppress_extensions == FALSE) {
      /* Stop after second time around loop. */
      break;
    } else if (!C_mode()) {
      /* In C++, multilevel pointer types are compatible if they differ only
         in the various qualifications.  The composite type picks the unions
         of those qualifiers. */
      *operation_type = multilevel_composite_pointer_type(operand_1_type,
                                                          operand_2_type);
      if (*operation_type != NULL) {
        /* The types are compatible and we have computed the result-type.
           So we can proceed directly to the end of the function (the
           tests between here and there assume impl_pointer_conversion
           found compatibility). */
        okay = TRUE;
        goto done;
      }  /* if */
    }  /* if */
    /* Go back for the second iteration with extensions allowed. */
    suppress_extensions = FALSE;
  }  /* for */
  if (okay && operand_1_is_pointer && operand_2_is_pointer &&
      operand_1_type != operand_2_type) {
    /* Make sure the operation type has all the cv-qualifiers present on
       each of the operands. */
    a_type_ptr type_pointed_to_1 = type_pointed_to(operand_1_type);
    a_type_ptr type_pointed_to_2 = type_pointed_to(operand_2_type);
    a_type_ptr operation_type_pointed_to;
    if (local_operation_type == operand_1_type) {
      operation_type_pointed_to =
                      type_plus_qualifiers_from_second_type(type_pointed_to_1,
                                                            type_pointed_to_2);
    } else {
      check_assertion(local_operation_type == operand_2_type);
      operation_type_pointed_to =
                      type_plus_qualifiers_from_second_type(type_pointed_to_2,
                                                            type_pointed_to_1);
    }  /* if */
    local_operation_type = make_pointer_type(operation_type_pointed_to);
  }  /* if */
  if (okay) {
    a_boolean nonstd_case = FALSE;
    if (strict_ansi_mode && C_dialect == C_dialect_ANSI) {
      /* In strict ANSI C mode, issue warnings for the extensions let by
         above. */
      if (!pointer_normalization_standard_in_C &&
          std_conv.pointer_normalization_needed) {
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
    if (std_conv.warning_suggested != ec_no_error && !nonstd_case) {
      /* Oddball cases call for a warning.  Suppress this if we issued a
         diagnostic about nonstandard use. */
      pos_opt_ty2_warning(std_conv.warning_suggested, operator_position,
                          operand_1_type, operand_2_type);
    }  /* if */
  } else {
    /* The operands are not compatible. */
    pos_ty2_error(ec_incompatible_operands, operator_position,
                  operand_1_type, operand_2_type);
    local_operation_type = error_type();
  }  /* if */
  *operation_type = local_operation_type;
done:
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
  a_boolean        okay = FALSE;
  a_type_ptr       operand_1_type = operand_1->type;
  a_type_ptr       operand_2_type = operand_2->type;
  a_std_conv_descr std_conv;

  if (is_ptr_to_member_type(operand_1_type)) {
    a_boolean      operand_2_is_constant = is_constant_operand(operand_2);
    a_constant_ptr operand_2_constant    = &operand_2->variant.constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && !operand_2_is_constant) {
      /* Microsoft mode allows some expressions as null pointer constants. */
      adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                        operand_2,
                                                        &operand_2_is_constant,
                                                        &operand_2_constant);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* See if the second operand can be converted to the type of the
       first operand. */
    if (impl_ptr_to_member_conversion(operand_2_type,
                                      operand_2_is_constant,
                                      operand_2_constant,
                                      operand_1_type,
                                      /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                      &std_conv)) {
      *operation_type = operand_1_type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay && is_ptr_to_member_type(operand_2_type)) {
    a_boolean      operand_1_is_constant = is_constant_operand(operand_1);
    a_constant_ptr operand_1_constant    = &operand_1->variant.constant;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && !operand_1_is_constant) {
      /* Microsoft mode allows some expressions as null pointer constants. */
      adjust_constant_operand_info_for_microsoft_null_pointer_test(
                                                        operand_1,
                                                        &operand_1_is_constant,
                                                        &operand_1_constant);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* See if the first operand can be converted to the type of the
       second operand. */
    if (impl_ptr_to_member_conversion(operand_1_type,
                                      operand_1_is_constant,
                                      operand_1_constant,
                                      operand_2_type,
                                      /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                      &std_conv)) {
      *operation_type = operand_2_type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay) {
    /* The operands are not compatible. */
    pos_ty2_error(ec_incompatible_operands, operator_position,
                  operand_1_type, operand_2_type);
    *operation_type = error_type();
  } else if (any_cfront_mode()) {
    /* Cfront doesn't allow casts of pointers to members from virtual base
       to derived, but it does allow comparisons of such things:
         struct A {int j;};
         struct B : public virtual A {int k;};
         void f() {
           int A::*pa = &A::j;
           int B::*pb = &B::k;
           (void)(pa == pb);    // allowed
         }
       Of course, it gets these wrong sometimes, because it doesn't do any
       adjustment.  We allow the comparison, but we do it "right" by doing
       the cast in the other direction and then comparing. */
    a_base_class_ptr bcp = std_conv.cast_base_class;
    if (bcp != NULL && !bcp->ambiguous &&
        any_virtual_steps_in_derivation(bcp)) {
      if (*operation_type == operand_1_type) {
        cast_operand(operand_2_type, operand_1, /*check_cast_access=*/TRUE,
                     /*is_implicit_cast=*/FALSE,
                     /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_semantics=*/FALSE);
        *operation_type = operand_2_type;
      } else {
        cast_operand(operand_1_type, operand_2, /*check_cast_access=*/TRUE,
                     /*is_implicit_cast=*/FALSE,
                     /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_semantics=*/FALSE);
        *operation_type = operand_1_type;
      }  /* if */
    }  /* if */
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
      cast_operand(type, operand_1, /*check_cast_access=*/TRUE,
                   /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    }  /* if */
    if (operand_2->type != type) {
      /* Cast operand 2 to match the desired type. */
      cast_operand(type, operand_2, /*check_cast_access=*/TRUE,
                   /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    }  /* if */
  }  /* if */
}  /* change_binary_operand_types */


void change_nonreal_member_constant_operand_to_lvalue(an_operand *operand)
/*
If the indicated operand is an rvalue indicating the value of a
member of a nonreal class, change it to an lvalue that refers to
the member.
*/
{
  if (is_an_rvalue(operand) && is_constant_operand(operand)) {
    a_constant_ptr con = &operand->variant.constant;
    if (con->kind == (a_constant_repr_kind)ck_template_param &&
        con->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_member &&
        /* Avoid problems with template-dependent enum constant values. */
        operand->type == type_of_unknown_templ_param_nontype) {
      /* Change the constant to one that refers to the address of the
         member. */
      a_constant_ptr memcon = alloc_shareable_constant(con);
      clear_constant(con, (a_constant_repr_kind)ck_template_param);
      set_template_param_constant_kind(
                                 con,
                                 (a_template_param_constant_kind)tpck_address);
      con->variant.template_param.variant.constant = memcon;
      con->type = make_pointer_type(memcon->type);
      operand->state = (an_operand_state)os_lvalue;
    }  /* if */
  }  /* if */
}  /* change_nonreal_member_constant_operand_to_lvalue */


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
  complete_type_is_needed(type);
  if (is_an_lvalue(operand) &&
      !is_const_qualified_type(type) &&
      !is_incomplete_type(type)) {
    /* In SVR4 C compatibility mode, this routine can be called for an
       lvalue cast that would normally be illegal.  Issue a warning. */
    if (SVR4_C_mode &&
        is_expression_operand(operand) &&
        is_operation_node(operand->variant.expression) &&
        operand->variant.expression->variant.operation.kind ==
                                      (an_expr_operator_kind)eok_lvalue_cast) {
      pos_warning(ec_expr_not_a_modifiable_lvalue, &operand->position);
    }  /* if */
    okay = TRUE;
    if (is_class_struct_union_type(type)) {
      type = skip_typerefs(type);
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


a_boolean check_integral_or_enum_operand(an_operand *operand)
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
  } else if (!is_integral_or_enum_type(operand->type)) {
    error_in_operand(enum_type_is_integral ?
                       ec_expr_not_integral : ec_expr_not_integral_or_enum,
                     operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_integral_or_enum_operand */


a_boolean check_arithmetic_or_enum_operand(an_operand *operand)
/*
Return FALSE if the operand is not of arithmetic type.  If there is an error,
change the operand to an error operand.  See section 3.1.2.5 of the standard.
*/
{
  register a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If it is an error type, an error message has already been issued. */
    okay = FALSE;
  } else if (!is_arithmetic_or_enum_type(operand->type)) {
    error_in_operand(enum_type_is_integral ?
                       ec_expr_not_arithmetic : ec_expr_not_arithmetic_or_enum,
                     operand);
    okay = FALSE;
  }  /* if */

  return okay;
}  /* check_arithmetic_or_enum_operand */


a_boolean check_pointer_operand(an_operand    *operand,
				an_error_code err_code)
/*
Return FALSE and issue an error message if the operand is not a pointer type.
If there is an error, make "operand" into an error operand.
*/
{
  a_boolean okay = TRUE;

  if (C_mode() && is_an_rvalue(operand) && is_array_type(operand->type)) {
    /* Attempt to use an array rvalue in C.  This is nonstandard.
       Convert it to a pointer to the first element of the array. */
    conv_array_rvalue_operand_to_pointer_operand(operand);
  }  /* if */
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
    complete_type_is_needed(underlying_type);
    if (!is_object_type(underlying_type)) {
      error_in_operand(ec_expr_not_object_pointer, operand);
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
    complete_type_is_needed(underlying_type);
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
  a_boolean  okay = TRUE;
  a_type_ptr pointer_type;

  if (is_error_operand(operand)) {
    /* If the operand has a type of error, an error message has already been
       issued. */
    okay = FALSE;
  } else {
    pointer_type = operand->type;
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
  a_boolean okay = TRUE;

  if (is_error_operand(operand)) {
    /* If the operand has a type of error, an error message has already been
       issued. */
    okay = FALSE;
  } else if (!is_scalar_type(operand->type)) {
    error_in_operand(enum_type_is_integral ?
                       ec_expr_not_scalar :
                       ec_expr_not_arithmetic_or_enum_or_pointer,
                     operand);
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
  a_boolean        is_constant_zero = FALSE;
  an_expr_node_ptr node;

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
integral, floating, pointer, or pointer to member type).  Note that
it may be necessary to call constant_bool_value_known_at_compile_time
to rule out certain cases before calling this routine.
*/
{
  a_boolean        is_constant_false = FALSE;
  an_expr_node_ptr node;

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
          underlying_type = NULL;
          if (is_pointer_type(lhs_node->type)) {
            if (is_constant_node(lhs_node)) {
              /* The left-hand operand address is given by a constant.
                 See if it is the address of a variable or constant
                 implicitly cast to another type, in which case we may
                 have an underlying array type. */
              con = lhs_node->variant.constant;
              if (con->kind == (a_constant_repr_kind)ck_address &&
                  con->implicit_cast && con->variant.address.offset == 0) {
                if (con->variant.address.kind ==
                                          (an_address_base_kind)abk_variable) {
                   /* Address of variable. */
                  underlying_type= con->variant.address.variant.variable->type;
                } else if (con->variant.address.kind ==
                                          (an_address_base_kind)abk_constant) {
                  con = con->variant.address.variant.constant;
                  if (con->kind == (a_constant_repr_kind)ck_string &&
                      !con-> implicit_cast) {
                    /* Address of string literal. */
                    underlying_type = con->type;
                  }  /* if */
                }  /* if */
              }  /* if */
            } else if (is_operation_node(lhs_node)) {
              /* The left-hand operand address is given by an expression.
                 An array-type-decay cast must be present.  The node underneath
                 that, if it has pointer-to-array type, gives the proper array
                 type. */
              if (lhs_node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast &&
                  lhs_node->variant.operation.compiler_generated) {
                lhs_node = lhs_node->variant.operation.operands;
                if (is_pointer_type(lhs_node->type)) {
                  underlying_type = type_pointed_to(lhs_node->type);
                }  /* if */
              }  /* if */
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
                } else if (vla_enabled && is_vla_type(underlying_type)) {
                  /* Variable-length arrays cannot be checked for non-negative
                     subscripts. */
                } else {
                  check_assertion(!has_unknown_specified_bound(array_type));
                  num_elements = array_type->
                                     variant.array.variant.number_of_elements;
                  /* Do not check subscripts on arrays dimensioned as having
                     size 1, since that's probably a clue that the programmer
                     is cheating. */
                  if (num_elements > 1) {
                    cmp = cmpulit_integer_constant(
                                 rhs_con, (a_host_large_unsigned)num_elements);
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

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      switch (token) {
	case tok_plus:
	  op = (an_expr_operator_kind)eok_fadd;
	  break;
	case tok_minus:
	  op = (an_expr_operator_kind)eok_fsubtract;
	  break;
        case tok_star:
          op = (an_expr_operator_kind)eok_jmultiply;
          break;
        case tok_divide:
          op = (an_expr_operator_kind)eok_fdivide;
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

    case tk_complex:
      switch (token) {
	case tok_plus:
	  op = (an_expr_operator_kind)eok_xadd;
	  break;
	case tok_minus:
	  op = (an_expr_operator_kind)eok_xsubtract;
	  break;
        case tok_star:
          op = (an_expr_operator_kind)eok_xmultiply;
          break;
        case tok_divide:
          op = (an_expr_operator_kind)eok_xdivide;
          break;
	case tok_eq:
	  op = (an_expr_operator_kind)eok_xeq;
	  break;
	case tok_ne:
	  op = (an_expr_operator_kind)eok_xne;
	  break;
	case tok_assign:
	  op = (an_expr_operator_kind)eok_xassign;
	  break;
	case tok_times_assign:
	  op = (an_expr_operator_kind)eok_xmultiply_assign;
	  break;
	case tok_divide_assign:
	  op = (an_expr_operator_kind)eok_xdivide_assign;
	  break;
	case tok_plus_assign:
	  op = (an_expr_operator_kind)eok_xadd_assign;
	  break;
	case tok_minus_assign:
	  op = (an_expr_operator_kind)eok_xsubtract_assign;
	  break;
#if CHECKING
        default:
	  internal_error("which_binary_operator: bad float operator");
#endif /* CHECKING */
      }  /* switch */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

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


an_expr_operator_kind generic_operator_for_opname_kind(
                                                 an_opname_kind kind,
                                                 a_boolean      unary_operator)
/*
Return the generic operator that corresponds to the indicated operator
kind.  The operation is a unary operation if unary_operator is TRUE.
A "generic operator" is used for template-dependent operations for
which it's not possible to know the operand types or the result type.
In some cases, the operator is the same as the one usually used,
e.g., eok_complement for "~".  In others, it's a special generic untyped
version of the operator, e.g., eok_add for "+" instead of eok_iadd or
eok_fadd or eok_padd.
*/
{
  an_expr_operator_kind op;

  if (unary_operator) {
    /* Unary operations. */
    switch (kind) {
      case onk_plus:
        op = (an_expr_operator_kind)eok_unary_plus;
        break;
      case onk_minus:
        op = (an_expr_operator_kind)eok_negate;
        break;
      case onk_star:
        op = (an_expr_operator_kind)eok_indirect;
        break;
      case onk_ampersand:
        op = (an_expr_operator_kind)eok_address;
        break;
      case onk_compl:
        op = (an_expr_operator_kind)eok_complement;
        break;
      case onk_not:
        op = (an_expr_operator_kind)eok_not;
        break;
      case onk_plus_plus:
        /* Note that postfix ++ comes in as a non-unary operation. */
        op = (an_expr_operator_kind)eok_pre_incr;
        break;
      case onk_minus_minus:
        /* Note that postfix -- comes in as a non-unary operation. */
        op = (an_expr_operator_kind)eok_pre_decr;
        break;
      default:
        unexpected_condition_str("bad unary opname kind");
    }  /* switch */
  } else {
    /* Non-unary operations. */
    switch (kind) {
      case onk_plus:
        op = (an_expr_operator_kind)eok_add;
        break;
      case onk_minus:
        op = (an_expr_operator_kind)eok_subtract;
        break;
      case onk_star:
        op = (an_expr_operator_kind)eok_multiply;
        break;
      case onk_divide:
        op = (an_expr_operator_kind)eok_divide;
        break;
      case onk_remainder:
        op = (an_expr_operator_kind)eok_remainder;
        break;
      case onk_excl_or:
        op = (an_expr_operator_kind)eok_xor;
        break;
      case onk_ampersand:
        op = (an_expr_operator_kind)eok_and;
        break;
      case onk_or:
        op = (an_expr_operator_kind)eok_or;
        break;
      case onk_assign:
        op = (an_expr_operator_kind)eok_assign;
        break;
      case onk_lt:
        op = (an_expr_operator_kind)eok_lt;
        break;
      case onk_gt:
        op = (an_expr_operator_kind)eok_gt;
        break;
      case onk_plus_assign:
        op = (an_expr_operator_kind)eok_add_assign;
        break;
      case onk_minus_assign:
        op = (an_expr_operator_kind)eok_subtract_assign;
        break;
      case onk_times_assign:
        op = (an_expr_operator_kind)eok_multiply_assign;
        break;
      case onk_divide_assign:
        op = (an_expr_operator_kind)eok_divide_assign;
        break;
      case onk_remainder_assign:
        op = (an_expr_operator_kind)eok_remainder_assign;
        break;
      case onk_excl_or_assign:
        op = (an_expr_operator_kind)eok_xor_assign;
        break;
      case onk_and_assign:
        op = (an_expr_operator_kind)eok_and_assign;
        break;
      case onk_or_assign:
        op = (an_expr_operator_kind)eok_or_assign;
        break;
      case onk_shift_left:
        op = (an_expr_operator_kind)eok_shiftl;
        break;
      case onk_shift_right:
        op = (an_expr_operator_kind)eok_shiftr;
        break;
      case onk_shift_right_assign:
        op = (an_expr_operator_kind)eok_shiftr_assign;
        break;
      case onk_shift_left_assign:
        op = (an_expr_operator_kind)eok_shiftl_assign;
        break;
      case onk_eq:
        op = (an_expr_operator_kind)eok_eq;
        break;
      case onk_ne:
        op = (an_expr_operator_kind)eok_ne;
        break;
      case onk_le:
        op = (an_expr_operator_kind)eok_le;
        break;
      case onk_ge:
        op = (an_expr_operator_kind)eok_ge;
        break;
      case onk_and_and:
        op = (an_expr_operator_kind)eok_land;
        break;
      case onk_or_or:
        op = (an_expr_operator_kind)eok_lor;
        break;
      case onk_plus_plus:
        /* Note that postfix ++ comes in as a non-unary operation. */
        op = (an_expr_operator_kind)eok_post_incr;
        break;
      case onk_minus_minus:
        /* Note that postfix -- comes in as a non-unary operation. */
        op = (an_expr_operator_kind)eok_post_decr;
        break;
      case onk_comma:
        op = (an_expr_operator_kind)eok_comma;
        break;
      case onk_arrow_star:
        op = (an_expr_operator_kind)eok_pm_arrow_field;
        break;
      case onk_subscript:
        op = (an_expr_operator_kind)eok_subscript;
        break;
      default:
        unexpected_condition_str("bad opname kind");
    }  /* switch */
  }  /* if */
  return op;
}  /* generic_operator_for_opname_kind */


a_boolean operator_takes_lvalue_operand(an_expr_operator_kind op)
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
    case eok_va_start:
    case eok_va_arg:
    case eok_va_end:
    case eok_post_incr:
    case eok_post_decr:
    case eok_pre_incr:
    case eok_pre_decr:
    case eok_assign:
    case eok_add_assign:
    case eok_subtract_assign:
    case eok_multiply_assign:
    case eok_divide_assign:
    case eok_address:
      takes_lvalue = TRUE;
      break;
    default:
      takes_lvalue = FALSE;
      break;
  }  /* switch */
  return takes_lvalue;
}  /* operator_takes_lvalue_operand */


void do_binary_operation(an_expr_operator_kind op,
			 an_operand            *operand_1,
			 an_operand            *operand_2,
			 a_type_ptr            result_type,
			 an_operand            *result,
			 a_source_position     *operator_position)
/*
Perform a binary operation on 2 operands yielding a result.  op
indicates the operation, and operand_1 and operand_2 are the operands.
result_type indicates the type of result; the result is placed in
*result.  If the operands are constant, the operation will be folded
if possible.  operator_position indicates the operator position.
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
    if (try_folding && 
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
                         curr_expr_is_evaluated(),
                         &did_not_fold, &template_constant, operator_position);
      }  /* if */
    }  /* if */
    if (did_not_fold) {
      if (!template_constant && curr_expr_kind_is_const() &&
          curr_expr_is_evaluated()) {
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
            pos_warning(ec_subscript_out_of_range, operator_position);
          }  /* if */
        }  /* if */
        if (template_constant) {
          /* For an expression based on a template parameter, scanned
             during the prototype instantiation, make a ck_template_param
             constant for the result. */
          make_template_param_expr_constant_operand(
                                       make_node_from_operand(result), result);
        }  /* if */
      }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
    } else if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
      /* The operation was folded to a constant.  We're recording
         expressions for constants, so build an expression and point the
         constant to it. */
      an_operand  result_expr;
      build_binary_result_operand(operand_1, operand_2, op,
                                  result_type, &result_expr);
      if (!is_error_operand(&result_expr)) {
        check_assertion(is_expression_operand(&result_expr));
        result->variant.constant.expr = result_expr.variant.expression;
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    }  /* if */
  }  /* if */
  result->state = (an_operand_state)os_rvalue;
}  /* do_binary_operation */


static void do_generic_operand_transformations(an_operand *operand)
/*
Do transformations that are appropriate on a generic operand, i.e.,
an operand in a template-dependent operation, where we can't tell
what will be done with the operand.
*/
{
  check_assertion(is_template_dependent_context());
  if (is_indefinite_function_operand(operand)) {
    /* Replace an indefinite function by the address of an unknown
       function in the set.  The result is always an rvalue. */
    make_unknown_dependent_function_operand(operand->variant.symbol,
                                            (a_boolean)operand->is_template_id,
                                            operand->template_arg_list,
                                            (a_boolean)operand->
                                                             is_qualified_name,
                                            operand);
  } else if (is_sym_for_member_operand(operand)) {
    /* Replace a symbol-for-member operand by a pointer-to-member. */
    conv_sym_for_member_operand_to_ptr_to_member(operand);
  }  /* if */
}  /* do_generic_operand_transformations */


static void prep_generic_operand_full(an_operand *operand,
                                      a_boolean  lvalue_expected,
                                      a_boolean  rvalue_expected)
/*
The indicated operand is about to be used as the operand of an expression
involving template parameter types.  Adjust it as needed: add an eok_lvalue
or eok_rvalue node to the operand to mark it as an lvalue or rvalue.
lvalue_expected is TRUE to indicate that an lvalue is expected, and
rvalue_expected is TRUE to indicate that an rvalue is expected.  The
eok_lvalue/eok_rvalue node is inserted only for the unexpected cases.
*/
{
  an_expr_node_ptr expr;
  an_operand       orig_operand;

  check_assertion(is_template_dependent_context());
  orig_operand = *operand;
  do_generic_operand_transformations(operand);
  if (is_an_lvalue(operand) || is_a_function_designator(operand)) {
    if (!lvalue_expected) {
      /* The operand is an lvalue, and the operation expects an rvalue.
         Add an eok_lvalue node. */
      expr = make_node_from_operand(operand);
      expr = make_operator_node((an_expr_operator_kind)eok_lvalue,
                                operand->type, expr);
      make_expression_operand(expr, rvalue_type(operand->type), operand);
    }  /* if */
  } else if (is_an_rvalue(operand)) {
    if (!rvalue_expected) {
      /* The operand is an rvalue, and the operation expects an lvalue.
         Add an eok_rvalue node. */
      expr = make_node_from_operand(operand);
      expr = make_operator_node((an_expr_operator_kind)eok_rvalue,
                                expr->type, expr);
      make_expression_operand(expr, operand->type, operand);
    }  /* if */
  }  /* if */
  restore_operand_details_incl_ref(operand, &orig_operand);
  /* We don't know how this operand is used, so set a special kind
     of reference. */
  change_ref_kinds(operand->ref_entries_list, SRK_PROTO_INST_REF);
}  /* prep_generic_operand_full */


void prep_generic_operand(an_operand *operand,
                          a_boolean  lvalue_expected)
/*
The indicated operand is about to be used as the operand of an expression
involving template parameter types.  Adjust it as needed: add an eok_lvalue
or eok_rvalue node to the operand to mark it as an lvalue or rvalue.
lvalue_expected is TRUE to indicate that an lvalue is expected, or FALSE
to indicate that an rvalue is expected.  The eok_lvalue/eok_rvalue node
is inserted only for the unexpected case.
*/
{
  prep_generic_operand_full(operand, lvalue_expected, !lvalue_expected);
}  /* prep_generic_operand */


void generic_cast_operand(an_operand            *operand,
                          a_type_ptr            dest_type,
                          an_expr_operator_kind op,
                          a_boolean             is_implicit_cast,
                          a_boolean             is_reference_cast)
/*
Add a generic cast that casts the given operand to dest_type.  This is used
in prototype instantiations to represent conversions to unknown types.
op is the expression operator to be used (e.g., eok_cast, eok_static_cast).
is_implicit_cast is TRUE if the cast is implicit.  is_reference_cast is
TRUE if the cast is a cast to a reference type in its original form
(and a cast to a pointer type here).  Note that the cast can be bizarre
in a number of ways, e.g., if the source operand is an lvalue.
*/
{
  an_operand orig_operand;
  a_boolean  can_fold = FALSE;

  orig_operand = *operand;
  check_assertion(!is_reference_type(dest_type) &&
                  is_template_dependent_context());
  /* See whether we know that the operand will be used as an rvalue. */
  if (!curr_expr_kind_is_const() &&
      (is_class_struct_union_type(dest_type) ||
       is_template_param_type(dest_type) ||
       is_class_struct_union_type(operand->type) ||
       is_template_param_type(operand->type))) {
    /* This might be a cast to or from a class type, and thus might
       involve a user-defined conversion.  Therefore we don't know whether
       the operand will be used as an lvalue or an rvalue.  Or, the
       destination type is a template parameter type that might turn out
       to be a reference type, which has the same consequence. */
    if (is_an_lvalue(operand) && is_constant_operand(operand)) {
      a_constant_ptr con = &operand->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_template_param &&
          con->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_address &&
          /* Avoid problems with reference binding. */
          !is_reference_cast) {
        /* The constant is the address of a member of a nonreal class.
           It won't matter whether this is considered an lvalue or an
           rvalue as an operand to the generic cast, so convert it to
           an rvalue because that allows folding of the cast and
           preservation of the possibility that this expression can be
           used as a constant expression. */
        conv_lvalue_to_rvalue(operand);
      }  /* if */
    }  /* if */
  } else {
    /* The operand will definitely be used as an rvalue. Convert it if
       necessary. */
    /* Doing the generic transformations first avoids an error on
       an indefinite function. */
    do_generic_operand_transformations(operand);
    do_operand_transformations(operand, TOPT_NO_OPTIONS);
  }  /* if */
  /* Determine whether we can fold the cast to a constant. */
  /* This is necessary in constant expressions, and also for initializers
     for entities of const integral or enum type (so their values can be used
     in constant expressions). */
  if (curr_expr_kind_is_const()) {
    can_fold = TRUE;
  } else if (is_constant_operand(operand) &&
             is_an_rvalue(operand) &&
             !is_class_struct_union_type(dest_type)) {
    /* In a non-constant expression, fold casts of constant rvalues,
       except casts to class types. */
    can_fold = TRUE;
  }  /* if */
  if (is_error_operand(operand)) {
    /* Leave an error operand alone. */
  } else if (can_fold) {
    /* Fold a constant cast. */
    check_assertion_str(is_constant_operand(operand) && is_an_rvalue(operand),
                        "generic_cast_operand: non-const or lvalue operand");
    /* Note that we don't use cast_operand or type_change_constant,
       because this conversion might be highly invalid. */
    if (!il_identical_types(operand->type, dest_type)) {
      a_type_ptr con_dest_type = dest_type;
      a_constant orig_constant;
      orig_constant = operand->variant.constant;
      if (operand->state == (an_operand_state)os_lvalue ||
          operand->state == (an_operand_state)os_function_designator) {
        /* If we're casting an lvalue constant, the constant type has
           one more level of "pointer to" than the operand does. */
        if (con_dest_type != type_of_unknown_templ_param_nontype) {
          con_dest_type = make_pointer_type(con_dest_type);
        }  /* if */
      }  /* if */
      make_template_param_cast_constant(&orig_constant,
                                        &operand->variant.constant,
                                        con_dest_type, !is_implicit_cast);
      operand->type = dest_type;
    }  /* if */
  } else {
    /* Non-constant expression.  Generate a cast expression. */
    /* We don't know whether to expect an rvalue or an lvalue, so
       make both explicit. */
    prep_generic_operand_full(operand,
                              /*lvalue_expected=*/FALSE,
                              /*rvalue_expected=*/FALSE);
    if (!il_identical_types(operand->type, dest_type)) {
      an_expr_node_ptr expr, opexpr = make_node_from_operand(operand);
      if (!is_class_struct_union_type(dest_type) ||
          op != (an_expr_operator_kind)eok_cast) {
        /* Cast to a non-class type.  Render as eok_cast operator or the
           like. */
        expr = make_operator_node(op, dest_type, opexpr);
        if (is_implicit_cast) {
          expr->variant.operation.compiler_generated = TRUE;
        }  /* if */
        expr->variant.operation.is_reference_cast = is_reference_cast;
      } else {
        /* Cast to a class type.  Use an enk_temp_init/dik_constructor. */
        a_dynamic_init_ptr dip;
        expr = create_expr_temporary(dest_type, /*result_is_addr=*/FALSE,
                                     /*is_explicit_cast=*/!is_implicit_cast,
                                     &orig_operand.position);
        dip = expr->variant.init.dynamic_init;
        /* A dik_constructor with a NULL constructor is used for generic
           construction. */
        set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constructor);
        dip->variant.constructor.ptr = NULL;
        dip->variant.constructor.args = opexpr;
      }  /* if */
      make_expression_operand(expr, dest_type, operand);
    }  /* if */
  }  /* if */
  restore_operand_details_incl_ref(operand, &orig_operand);
}  /* generic_cast_operand */


an_expr_node_ptr prep_generic_argument_list(an_arg_operand *arg_operand_list)
/*
Prepare a generic argument list, i.e., one scanned during a prototype
instantiation for which we do not know the actual function to be called.
Return a list of argument expressions.
*/
{
  an_expr_node_ptr   arg, prev_arg, arg_list;
  an_arg_operand_ptr arg_operand;

  prev_arg = NULL;
  arg_list = NULL;
  for (arg_operand = arg_operand_list;
       arg_operand != NULL;
       arg_operand = arg_operand->next) {
    prep_generic_operand(&arg_operand->operand, /*lvalue_expected=*/FALSE);
    arg = make_node_from_operand(&arg_operand->operand);
    /* Add this argument to the end of the expression-form argument list
       being built up. */
    if (prev_arg == NULL) {
      arg_list = arg;
    } else {
      prev_arg->next = arg;
    }  /* if */
    prev_arg = arg;
  }  /* for */
  return arg_list;
}  /* prep_generic_argument_list */


void template_binary_operation(an_expr_operator_kind op,
                               an_operand            *operand_1,
                               an_operand            *operand_2,
                               an_operand            *result,
                               a_source_position     *operator_position)
/*
This routine is a wrapper for do_binary_operation for the case where
an expression is being built from operands whose types are based
on template parameter types.  When the current expression kind is
constant, this can happen in nontype template arguments.  Otherwise,
it happens in prototype instantiations.  op is the generic operator to
be used (e.g., eok_add, not eok_iadd).
*/
{
  if (curr_expr_kind_is_const()) {
    do_generic_operand_transformations(operand_1);
    do_generic_operand_transformations(operand_2);
    check_assertion_str((is_constant_operand(operand_1) ||
                         is_error_operand(operand_1)) &&
                        (is_constant_operand(operand_2) ||
                         is_error_operand(operand_2)),
                        "template_binary_operation: non-const operand");
    /* In a constant expression, only operations on integral types are
       allowed on operands involving template parameter types, so
       switch to the integral version of the generic operator if
       there is one. */
    switch (op) {
      case eok_add:
        op = (an_expr_operator_kind)eok_iadd;
        break;
      case eok_subtract:
        op = (an_expr_operator_kind)eok_isubtract;
        break;
      case eok_multiply:
        op = (an_expr_operator_kind)eok_imultiply;
        break;
      case eok_divide:
        op = (an_expr_operator_kind)eok_idivide;
        break;
      case eok_eq:
        op = (an_expr_operator_kind)eok_ieq;
        break;
      case eok_ne:
        op = (an_expr_operator_kind)eok_ine;
        break;
      case eok_gt:
        op = (an_expr_operator_kind)eok_igt;
        break;
      case eok_lt:
        op = (an_expr_operator_kind)eok_ilt;
        break;
      case eok_ge:
        op = (an_expr_operator_kind)eok_ige;
        break;
      case eok_le:
        op = (an_expr_operator_kind)eok_ile;
        break;
      default:;
        /* Other operators are unchanged. */
    }  /* switch */
  } else {
    /* The current expression is not a constant expression. */
    prep_generic_operand(operand_1, operator_takes_lvalue_operand(op));
    prep_generic_operand(operand_2, /*lvalue_expected=*/FALSE);
  }  /* if */
  do_binary_operation(op, operand_1, operand_2,
                      type_of_unknown_templ_param_nontype,
                      result, operator_position);
}  /* template_binary_operation */


void do_unary_operation(an_expr_operator_kind op,
                        an_operand            *operand,
                        a_type_ptr            result_type,
                        an_operand            *result,
                        a_source_position     *start_position)
/*
Perform a unary operation on one operand yielding a result.  op
indicates the operation, and operand is the operand.  result_type indicates
the type of result; the result is placed in *result.  If the operand is
constant, the operation will be folded if possible.  start_position
indicates the operator position.
*/
{
  a_boolean  did_not_fold, template_constant;
  a_constant result_constant;

  if (is_error_operand(operand)) {
    make_error_operand(result);
  } else {
    did_not_fold = TRUE;
    template_constant = FALSE;
    if (is_constant_operand(operand) &&
        /* "&" isn't handled by unary_operation. */
        op != (an_expr_operator_kind)eok_address) {
      /* Fold the operation if the operand is constant.  In a nonconstant
         context, reduce any error to a warning and leave the operation
         to be done at runtime. */
      unary_operation(op, &operand->variant.constant,
                      result_type, &result_constant,
                      curr_expr_kind_is_const(),
                      curr_expr_is_evaluated(),
                      &did_not_fold, &template_constant, start_position);
    }  /* if */
    if (did_not_fold) {
      if (!template_constant && curr_expr_kind_is_const() &&
          curr_expr_is_evaluated()) {
        /* A constant operation could not be folded in a constant
           expression. */
        pos_error(ec_expr_not_constant, start_position);
        make_error_operand(result);
      } else {
        /* The operation could not be folded to a constant, so build
           an expression node. */
        build_unary_result_operand(operand, op, result_type, result);
#if !UNARY_PLUS_IN_IL
        if (op == (an_expr_operator_kind)eok_unary_plus &&
            !is_template_dependent_context()) {
          /* Do not put the unary "+" in the IL.  The unary "+" operator
             was added in version 2.44, and pre-existing back ends didn't
             know about it. */
          copy_operand(operand, result);
        }  /* if */
#endif /* !UNARY_PLUS_IN_IL */
        if (template_constant) {
          /* For an expression based on a template parameter, scanned
             during the prototype instantiation, make a ck_template_param
             constant for the result. */
          make_template_param_expr_constant_operand(
                                       make_node_from_operand(result), result);
        }  /* if */
      }  /* if */
    } else {
      /* The operation was folded to a constant. */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
        /* The operation was folded to a constant.  We're recording
           expressions for constants, so build an expression and point the
           constant to it. */
        an_operand  result_expr;
        build_unary_result_operand(operand, op, result_type, &result_expr);
        result_constant.expr = result_expr.variant.expression;
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      make_constant_operand(&result_constant, result);
    }  /* if */
  }  /* if */
}  /* do_unary_operation */


void template_unary_operation(an_expr_operator_kind op,
                              an_operand            *operand,
                              an_operand            *result,
                              a_source_position     *start_position)
/*
This routine is a wrapper for do_unary_operation for the case where
an expression is being built from an operand whose type is based
on template parameter types.  When the current expression kind is
constant, this can happen in nontype template arguments.  Otherwise,
it happens in prototype instantiations.  op is the generic operator to
be used (e.g., eok_negate, not eok_inegate).
*/
{
  if (curr_expr_kind_is_const()) {
    do_generic_operand_transformations(operand);
    check_assertion_str(is_constant_operand(operand) ||
                        is_error_operand(operand),
                        "template_unary_operation: non-const operand");
    /* In a constant expression, only operations on integral types are
       allowed on operands involving template parameter types, so
       switch to the integral version of the generic operator if
       there is one. */
    if (op == (an_expr_operator_kind)eok_negate) {
      op = (an_expr_operator_kind)eok_inegate;
    }  /* if */
  } else {
    /* The current expression is not a constant expression. */
    prep_generic_operand(operand, operator_takes_lvalue_operand(op));
  }  /* if */
  if (op == (an_expr_operator_kind)eok_address &&
      curr_expr_kind_is_const()) {
    /* "&" operator in a constant expression, which cannot be folded
       the usual way. */
    if (is_an_lvalue(operand)) {
      copy_operand(operand, result);
      take_address_of_lvalue(result);
    } else if (is_a_function_designator(operand)) {
      copy_operand(operand, result);
      conv_function_designator_to_ptr_to_function(result,
                                                  /*allow_ctor=*/FALSE);
    } else {
      check_assertion(is_error_operand(operand));
      make_error_operand(result);
    }  /* if */
  } else {
    /* Normal case. */
    do_unary_operation(op, operand,
                       type_of_unknown_templ_param_nontype,
                       result, start_position);
  }  /* if */
}  /* template_unary_operation */


void do_question_operation(an_operand *operand_1,
                           an_operand *operand_2,
                           an_operand *operand_3,
                           a_type_ptr result_type,
                           an_operand *result)
/*
Build an operand for a "?" operation.  operand_1, operand_2, and operand_3
are the operands.  result_type is the result type.  The operand is built
in *result.  Constant operations are not folded.
*/
{
  a_boolean operand_1_is_const, do_folding = FALSE;

  /* If the first operand is a known constant, the operation can be
     folded. */
  operand_1_is_const = is_constant_operand(operand_1) &&
                       constant_bool_value_known_at_compile_time(
                                                 &operand_1->variant.constant);
  if (operand_1_is_const) {
    if (curr_expr_kind_is_const()) {
      /* In constant expressions we must always fold. */
      do_folding = TRUE;
#if ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS
    } else if (curr_object_lifetime == NULL ||
               curr_object_lifetime->destructions == NULL) {
      /* We are supposed to remove dead code under conditional operators.
         However, if the second or third operands might contain destructions
         in this case, don't remove the dead code, because we don't
         want to run through the expression to find the destruction to
         unlink it. */
      do_folding = TRUE;
#else /* !ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS */
    } else if (is_constant_operand(operand_2) &&
               is_constant_operand(operand_3)) {
      /* Fold if the second and third operands are constants. */
      do_folding = TRUE;
#endif /* ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS */
    }  /* if */
  }  /* if */
  if (do_folding) {
    /* The first operand is a constant.  Fold the operation to the
       second or third operand. */
    an_operand *other_operand;
    if (op_is_false_constant(operand_1)) {
      /* The first operand is false; return the third operand as the result. */
      copy_operand(operand_3, result);
      other_operand = operand_2;
    } else {
      /* The first operand is true; return the second operand as the result. */
      copy_operand(operand_2, result);
      other_operand = operand_3;
    }  /* if */
    result->is_simple_string_literal = FALSE;
    result->is_cfront_null_pointer_constant = FALSE;
    if (is_constant_operand(result)) {
      if (!is_constant_operand(other_operand) ||
          other_operand->variant.constant.null_pointer_constant_ruled_out ||
          operand_1->variant.constant.null_pointer_constant_ruled_out) {
        /* The result is not a null pointer constant. */
        result->variant.constant.null_pointer_constant_ruled_out = TRUE;
      }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
        /* Create an expression to be recorded in the constant. */
        an_operand result_expr;
        build_question_result_operand(operand_1, operand_2, operand_3,
                                      result_type, &result_expr);
        result->variant.constant.expr = result_expr.variant.expression;
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    }  /* if */
  } else if (is_error_operand(operand_1) ||
             is_error_operand(operand_2) ||
             is_error_operand(operand_3)) {
    /* Some error. */
    make_error_operand(result);
  } else {
    /* Build the expression tree for the operation. */
    build_question_result_operand(operand_1, operand_2, operand_3,
                                  result_type, result);
    if (is_template_param_constant_operand(operand_1) ||
        is_template_param_constant_operand(operand_2) ||
        is_template_param_constant_operand(operand_3)) {
      /* For an expression based on a template parameter, scanned
         during the prototype instantiation, make a ck_template_param
         constant for the result. */
      make_template_param_expr_constant_operand(make_node_from_operand(result),
                                                result);
    }  /* if */
  }  /* if */
}  /* do_question_operation */


void template_question_operation(an_operand *operand_1,
                                 an_operand *operand_2,
                                 an_operand *operand_3,
                                 an_operand *result)
/*
This routine is a wrapper for do_question_operation for the case where
an expression is being built from operands whose types are based
on template parameter types.  When the current expression kind is
constant, this can happen in nontype template arguments.  Otherwise,
it happens in prototype instantiations.
*/
{
  if (curr_expr_kind_is_const()) {
    do_generic_operand_transformations(operand_1);
    do_generic_operand_transformations(operand_2);
    do_generic_operand_transformations(operand_3);
    check_assertion_str((is_constant_operand(operand_1) ||
                         is_error_operand(operand_1)) &&
                        (is_constant_operand(operand_2) ||
                         is_error_operand(operand_2)) &&
                        (is_constant_operand(operand_3) ||
                         is_error_operand(operand_3)),
                        "template_question_operation: non-const operand");
  } else {
    /* The current expression is not a constant expression. */
    prep_generic_operand(operand_1, /*lvalue_expected=*/FALSE);
    prep_generic_operand(operand_2, /*lvalue_expected=*/FALSE);
    prep_generic_operand(operand_3, /*lvalue_expected=*/FALSE);
  }  /* if */
  do_question_operation(operand_1, operand_2, operand_3,
                        type_of_unknown_templ_param_nontype,
                        result);
}  /* template_question_operation */


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
  /* Mark the node as being an implicit indirection generated for a
     reference. */
  node->implicit_reference_indirection = TRUE;
  if (is_operation_node(node)) {
    node->variant.operation.compiler_generated = TRUE;
  }  /* if */
  if (is_an_lvalue(result)) {
    result_type = type_pointed_to(result_type);
    /* Make the node have a pointer type instead of a reference type. */
    node->type = make_pointer_type(result_type);
    if (is_function_type(result_type)) {
      /* The thing pointed to is a function, so the result is a function
         designator. */
      result_state = (an_operand_state)os_function_designator;
    }  /* if */
  } else if (is_an_rvalue(result)) {
    result_type = node->type;
  }  /* if */
  make_expression_operand(node, result_type, result);
  result->state = result_state;
  /* Restore the original source position, etc.  Note that the reference
     entries are NOT restored, on purpose. */
  restore_operand_details(result, &orig_result);
  result->ref_entries_list = NULL;
}  /* add_reference_indirection */

#if RECORD_CONSTANT_EXPRESSIONS_IN_IL

static an_expr_node_ptr expr_to_record_for_variable(a_variable_ptr variable,
                                                    a_boolean      is_lvalue)
/*
Return the expression to be recorded in the "expr" field of a constant.
The expression is a reference to the indicated variable, and an lvalue
if is_lvalue is TRUE.  Return NULL if the expression cannot be generated.
*/
{
  an_expr_node_ptr expr = NULL;

  if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
    if (curr_il_region_number == FILE_SCOPE_REGION_NUMBER &&
        !in_file_scope(variable)) {
      /* Can't record a local variable in a file-scope expression.  This
         comes up in constant expressions (like array bounds) that
         reference const-valued local variables. */
      check_assertion(curr_expr_kind_is_const());
    } else {
      expr = is_lvalue ? var_lvalue_expr(variable) : var_rvalue_expr(variable);
    }  /* if */
  }  /* if */
  return expr;
}  /* expr_to_record_for_variable */

#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */

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

  if ((strict_ansi_mode || !C_mode()) &&
      is_void_type(variable_type) &&
      !is_qualified_type(variable_type)) {
    /* If the variable has type void, make an rvalue instead of an lvalue.
       See ANSI C 3.2.2.1.  This helps with
         extern void x;
         x;
         &x;
       Do this only in C++ and strict C mode.
    */
    make_expression_operand(var_rvalue_expr(variable), variable_type,
                            result);
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
      set_variable_address_constant(variable, &result->variant.constant,
                                    /*set_address_taken_flag=*/FALSE);
      result->type = variable_type;
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      /* Record the lvalue variable expression as IL in the constant. */
      result->variant.constant.expr =
                               expr_to_record_for_variable(variable,
                                                           /*is_lvalue=*/TRUE);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    }  /* if */
    result->state = (an_operand_state)os_lvalue;
    /* Start a list of reference entries related to the operand. */
    result->ref_entries_list = rep;
    /* If the variable has a reference type, add an implicit indirection. */
    if (C_dialect == C_dialect_cplusplus && is_reference_type(variable_type)) {
      add_reference_indirection(result);
    }  /* if */
  }  /* if */
  set_operand_position_to_pos_curr_token(result);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* If the operand has kind ok_expression, set the position in the
     expression too. */
  set_operand_expr_position_if_expr(result);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* make_lvalue_variable_operand */


a_constant_ptr var_constant_value(a_variable_ptr var)
/*
If the variable var has a constant initial value, return a pointer to it;
otherwise, return NULL.  A variable with an aggregate initial value is
considered to have no initial value.  Also, a variable with an address-
constant initial value is treated as having a nonconstant initial value.
*/
{
  a_constant_ptr con_val = NULL;

  /* See if the variable has a known constant value. */
  if (C_dialect == C_dialect_cplusplus &&
      is_const_variable(var) &&
      !is_volatile_qualified_type(var->type)) {
    an_init_kind       init_kind;
    an_initializer_ptr initializer;
    /* initk_function_local initialization can come up with local static
       variables when RECORD_CONSTANT_EXPRESSIONS_IN_IL is TRUE (the
       expression is function-local, and that forces the initializer
       constant to be made function-local as well). */
    get_variable_initializer(var, (a_scope_ptr)NULL, &init_kind, &initializer);
    if (init_kind == (an_init_kind)initk_static) {
      /* The variable has a constant initial value. */
      con_val = initializer->constant;
    } else if (init_kind == (an_init_kind)initk_dynamic) {
      /* The variable is dynamically initialized.  See if the initialization
         is to a constant. */
      if (initializer->dynamic->kind == (a_dynamic_init_kind)dik_constant) {
        con_val = initializer->dynamic->variant.constant;
      }  /* if */
    }  /* if */
    if (con_val != NULL) {
      if (con_val->kind == (a_constant_repr_kind)ck_aggregate ||
          con_val->kind == (a_constant_repr_kind)ck_address) {
        /* An aggregate or the address of a variable cannot be considered a
           constant value. */
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
                                    a_boolean         is_qualified_name,
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
The name that generated this pointer-to-member constant is a qualified
name if is_qualified_name is TRUE; it is the operand of a "&" operator if
is_operand_of_address_of is TRUE.  If the member is a bit field,
issue an error.  
*/
{
  a_constant constant;

  /* The ARM only allows this when a qualified name is preceded by a "&" (5.3).
     We allow it without "&" or without a qualified name as an extension --
     it's common practice. */
  if (strict_ansi_mode &&
      (!is_operand_of_address_of || !is_qualified_name)) {
    pos_diagnostic(strict_ansi_error_severity,
                   ec_nonstd_member_function_address, position);
  }  /* if */
  /* Protected members of a base class can only be accessed through an
     object of a derived class (ARM 11.5).  Post-ARM revisions have made
     it clear this applies also to pointers to members.  We allow the
     address of a protected member to be taken as a member of the
     derived class but not as a member of the base class.  For example:
       class A { protected: int i; };
       class B : public A { void mf(); };
       void B::mf() {
         int A::* pmi = &A::i;	// error - protected member
         int B::* pmj = &B::i;	// OK
       }
     Cfront does not do this checking, so we omit it in cfront mode.
     Also skip this check if it shouldn't be done (e.g., because 
     an access control error has already been issued for the identifier). */
  if (!any_cfront_mode() && check_protected_access
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* The Microsoft compiler does not do this check. */
      && !microsoft_mode
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                  ) {
    check_protected_member_access(member_sym, position,
                                  member_proj_sym->parent.class_type);
  }  /* if */
  /* No need to instantiate the class; since we have a member of it, it must
     be instantiated already. */
  if (member_sym->kind == (a_symbol_kind)sk_field) {
    /* Pointer to nonstatic data member. */
    a_field_ptr field = member_sym->variant.field.ptr;
    if (field->is_bit_field) {
      /* Cannot make a pointer-to-member of a bit field. */
      pos_error(ec_address_of_bit_field, position);
    }  /* if */
    set_ptr_to_data_member_constant(field, &constant);
  } else {
    a_routine_ptr rout;
#if CHECKING
    if (member_sym->kind != (a_symbol_kind)sk_member_function) {
      internal_error("make_ptr_to_member_constant_operand: bad kind");
    }  /* if */
#endif /* CHECKING */
    /* Pointer to nonstatic member function. */
    rout = member_sym->variant.routine.ptr;
    set_ptr_to_member_function_constant(rout, &constant);
    if (!rout->is_virtual) {
      /* Force the routine to be instantiated or generated. */
      if_evaluating_mark_routine_referenced(rout);
    }  /* if */
  }  /* if */
  if (is_template_dependent_context() &&
      is_template_dependent_type(constant.type)) {
    /* In a prototype instantiation, a member of the current class is
       template-dependent.  Make a template param constant by adding a
       do-nothing cast. */
    a_constant constant_copy;
    copy_constant(&constant, &constant_copy);
    make_template_param_cast_constant(&constant_copy, &constant,
                                      constant.type, /*is_explicit=*/FALSE);
  }  /* if */
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
routine symbol entry (not overloaded, but can be a projection symbol).
is_qualified_name is TRUE if the function was named by a qualified name.
The source position of the operand is set to *position.  rep points to an
associated reference entry, or is NULL if none is needed.
*/
{
  a_routine_ptr routine;

  clear_operand((an_operand_kind)ok_constant, result);
  /* Remember whether this symbol corresponds to a using-declaration.
     This has an effect on a virtual function call optimization. */
  if (is_class_member_using_decl_symbol(routine_sym)) {
    result->is_using_decl_name = TRUE;
  }  /* if */
  reduce_projection_symbol_to_fundamental_symbol(routine_sym);
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
  set_routine_address_constant(routine, &result->variant.constant,
                               /*set_address_taken_flag=*/FALSE);
  /* The type of the operand is the function type. */
  result->type = routine->type;
  result->state = (an_operand_state)os_function_designator;
  /* Remember whether or not the routine is virtual.  Use of a qualified
     name suppresses the virtual-ness of the function (ARM 10.2). */
  result->virtual_function = routine->is_virtual && !is_qualified_name;
  result->is_qualified_name = is_qualified_name;
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Set the position in the expression too. */
  set_operand_expr_position_if_expr(result);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  result->state = (an_operand_state)os_none;
}  /* make_field_operand */


a_dynamic_init_ptr alloc_expr_dynamic_init(a_dynamic_init_kind kind)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.  This should be used
instead of alloc_dynamic_init within the expression processing routines.
It does some extra processing based on the knowledge that we are within
an expression.
*/
{
  a_dynamic_init_ptr dip;

  /* Allocate and initialize the entry. */
  dip = alloc_dynamic_init(kind);
  if (expr_stack->inside_conditional_expression) {
    dip->inside_conditional_expression = TRUE;
  }  /* if */
  return dip;
}  /* alloc_expr_dynamic_init */


a_dynamic_init_ptr alloc_dtor_dynamic_init(a_dynamic_init_kind kind,
                                           a_type_ptr          type,
                                           a_source_position   *position)
/*
Allocate a dynamic initialization entry, clear it to default values, set
its kind to kind, and return a pointer to it.  If type is a type that
requires a destructor, put the destructor routine pointer into the dynamic
initialization entry.  *position gives the associated source position.
*/
{
  a_dynamic_init_ptr dip = alloc_expr_dynamic_init(kind);

  if (C_dialect == C_dialect_cplusplus && is_class_struct_union_type(type)) {
    /* The type is a class.  If it has a destructor, indicate it in
       the dynamic initialization. */
    if (!expr_stack->in_cctor_elision_initializer) {
      dip->destructor = select_destructor(type, type, position,
                                          /*honor_virtual=*/FALSE,
                                        curr_expr_is_potentially_evaluated());
    } else {
      /* In a cctor elision expression.  Put the destructor in the entry,
         but do not do the access checking etc. at this time.  Build a fixup
         entry to remind us to do the check later, and put it on the list
         for the current expression. */
      a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
      if (cssp != NULL) {
        a_symbol_ptr dtor_sym = cssp->destructor;
        if (dtor_sym != NULL) {
          dip->destructor = dtor_sym->variant.routine.ptr;
          (void)alloc_dynamic_init_dtor_fixup(dip, position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return dip;
}  /* alloc_dtor_dynamic_init */


void set_temp_init_dynamic_init_lifetime(an_expr_node_ptr temp_init_node)
/*
If the dynamic initialization attached to the indicated enk_temp_init
requires a later destruction, put it into the current object lifetime.
*/
{
  if (curr_expr_is_potentially_evaluated()) {
    a_dynamic_init_ptr dip = temp_init_node->variant.init.dynamic_init;
    /* Put the destruction (if any) on the list for the current object
       lifetime. */
    record_end_of_lifetime_destruction(dip, /*static_lifetime=*/FALSE,
                                       /*block_lifetime=*/FALSE);
    dip->has_temporary_lifetime = TRUE;
    /* If the lifetime happens to turn out to be static (e.g., when
       long lifetime temps are enabled), mark the temp init as requiring
       a static temporary. */
    if (dip->lifetime != NULL &&
        dip->lifetime->kind == (an_object_lifetime_kind)olk_global_static) {
      temp_init_node->variant.init.static_temp = TRUE;
    }  /* if */
  }  /* if */
}  /* set_temp_init_dynamic_init_lifetime */


an_expr_node_ptr alloc_temp_init_node(a_type_ptr         temp_type,
                                      a_dynamic_init_ptr dip,
                                      a_boolean          result_is_addr,
                                      a_boolean          is_explicit_cast)
/*
Create an enk_temp_init node and return a pointer to it.  The implied
temporary has type temp_type.  The initialization to be done is pointed
to by dip.  The value of the enk_temp_init is the address (rather than
the value) of the temporary if result_is_addr is TRUE.  is_explicit_cast
is TRUE if this node represents an explicit cast.
*/
{
  an_expr_node_ptr         temp_init_node;
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];

  temp_init_node = alloc_expr_node((an_expr_node_kind)enk_temp_init);
  temp_init_node->variant.init.result_is_addr = result_is_addr;
  if (result_is_addr) {
    /* The result is the address of the temporary, so the type is a pointer
       to the type of the temporary. */
    temp_init_node->type = make_pointer_type(temp_type);
  } else {
    /* The result is the value of the temporary, so the type is the type
       of the temporary as an rvalue. */
    temp_init_node->type = rvalue_type(temp_type);
  }  /* if */
  dip->is_explicit_cast = is_explicit_cast;
  /* Make sure the IL scope that the temporary is part of exists.  Even though
     the temporary does not exist as a variable, it's still (from a language
     point of view) part of this scope.  That's important, because it has to
     be destroyed at the right point.  (Note, however, that when a temp is
     created for a default argument in the context of a function prototype
     scope, or a template declaration scope, no IL scope will be created;
     that's okay, since the expression will be copied in a context that
     will have an IL scope.) */
  if (ssep->kind != (a_scope_kind)sck_func_prototype &&
      ssep->kind != (a_scope_kind)sck_template_declaration) {
    (void)ensure_il_scope_exists(ssep);
  }  /* if */
  temp_init_node->variant.init.dynamic_init = dip;
  /* Put the dynamic initialization on a destruction list if appropriate. */
  set_temp_init_dynamic_init_lifetime(temp_init_node);
  return temp_init_node;
}  /* alloc_temp_init_node */


an_expr_node_ptr create_expr_temporary(a_type_ptr        temp_type,
                                       a_boolean         result_is_addr,
                                       a_boolean         is_explicit_cast,
                                       a_source_position *position)
/*
Create an enk_temp_init node and return a pointer to it.  The implied
temporary has type temp_type.  A dynamic initialization entry indicating
no initialization (but indicating destruction if appropriate) is attached
under the enk_temp_init node.  The value of the enk_temp_init is the address
(rather than the value) of the temporary if result_is_addr is TRUE.
is_explicit_cast is TRUE if this node represents an explicit cast.
*position is the position of the reference.  Only used in C++.
*/
{
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node;

  /* Allocate the dynamic initialization entry. */
  dip = alloc_dtor_dynamic_init((a_dynamic_init_kind)dik_none, temp_type,
                                position);
  /* Make an enk_temp_init node that points at the dynamic init entry. */
  temp_init_node = alloc_temp_init_node(temp_type, dip, result_is_addr,
                                        is_explicit_cast);
  return temp_init_node;
}  /* create_expr_temporary */


static an_expr_node_ptr func_call_expr(an_expr_node_ptr  function_node,
                                       a_type_ptr        function_type,
                                       a_boolean         is_virtual,
                                       a_boolean         virtual_suppressed,
                                       a_boolean         compiler_generated,
                                       a_source_position *err_pos)
/*
Make an expression for a call of the function indicated by function_node,
whose type is function_type, and which is virtual if is_virtual is TRUE or
a pointer-to-member-function call if the type of function_node is
pointer-to-member-function.  The arguments of the call are already
attached to function_node.  A skip_typerefs need not have been done
on function_type.  Return a pointer to the call node.  *err_pos
gives an error position for the case where the function return type is
invalid (i.e., incomplete); an error node is returned for that case.
If virtual_suppressed is TRUE, the function was named via a qualified name
and that has suppressed calling it as virtual; that's also reflected in
is_virtual, but knowing that the user did it explicitly controls whether
a diagnostic is put out in some cases.  compiler_generated is TRUE if
this call is compiler-generated (e.g., for an implicit conversion via
a conversion function).
*/
{
  an_expr_operator_kind         op;
  an_expr_node_ptr              call_node;
  a_type_ptr                    return_type;
  a_routine_type_supplement_ptr rtsp;
  an_expr_node_ptr              temp_init_node = NULL;
  a_dynamic_init_ptr            dip;
  a_routine_ptr                 rp = NULL;

  function_type = skip_typerefs(function_type);
  if (function_node->kind == (an_expr_node_kind)enk_routine_address) {
    /* We know which routine is being called. */
    rp = function_node->variant.routine;
  }  /* if */
  /* The function return type must be void or object type and not array
     type.  Half of this check is in add_to_derived_type_list.
     The check here is necessary because it is valid to declare a
     function returning a class/struct/enum type that is incomplete
     at the point of declaration of the function so long as it is completed
     by the time the function is defined or called (if it is). */
  if (!check_function_return_type(function_type, err_pos,
                                  /*is_expr_use=*/TRUE, rp)) {
    /* There was some error in the return type, and a diagnostic was issued. */
    call_node = error_node();
    goto done;
  } /* if */
  if (rp != NULL) {
    /* We know which routine is being called. */
    if (curr_expr_is_potentially_evaluated()) {
      /* It is being called. */
      rp->called = TRUE;
      if (rp->pure_virtual && !is_virtual && !virtual_suppressed) {
        /* Non-virtual call of a pure virtual function, and not written
           explicitly to suppress virtualness. */
        pos_warning(ec_call_of_pure_virtual, err_pos);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Determine the return type, dealing with reference types and
     cv-qualifiers. */
  return_type = il_return_type_of(function_type);
  /* Determine the operator to use for the call. */
  if (is_ptr_to_member_type(function_node->type)) {
    /* Call using a pointer-to-member-function. */
    op = (an_expr_operator_kind)eok_pm_call;
  } else if (is_virtual) {
    /* Call of a virtual function. */
    op = (an_expr_operator_kind)eok_virtual_call;
  } else {
    /* Normal call. */
    op = (an_expr_operator_kind)eok_call;
  }  /* if */
  /* Make an expression for the function call. */
  call_node = make_operator_node(op, return_type, function_node);
  call_node->variant.operation.compiler_generated = compiler_generated;
  rtsp = function_type->variant.routine.extra_info;
  if (rtsp->value_returned_by_cctor) {
    temp_init_node = create_expr_temporary(return_type,
                                           /*result_is_addr=*/FALSE,
                                           /*is_explicit_cast=*/FALSE,
                                           err_pos);
    dip = temp_init_node->variant.init.dynamic_init;
    set_dynamic_init_kind(dip,
                      (a_dynamic_init_kind)dik_call_returning_class_via_cctor);
    dip->variant.expression = call_node;
    call_node = temp_init_node;
  }  /* if */
done:
  return call_node;
}  /* func_call_expr */


void make_function_call(an_expr_node_ptr  function_node,
                        a_type_ptr        function_type,
                        a_boolean         is_virtual,
                        a_boolean         virtual_suppressed,
                        a_boolean         compiler_generated,
                        a_source_position *call_pos,
                        an_operand        *result)
/*
Make an operand for a call of the function indicated by function_node, whose
type is function_type, and which is virtual if is_virtual is TRUE or
a pointer-to-member-function call if the type of function_node is
pointer-to-member-function.  The arguments of the call are already
attached to function_node.  A skip_typerefs need not have been done
on function_type.  compiler_generated is TRUE if this is a compiler-
generated call (e.g., for an implicit conversion via a conversion
function).  *call_pos gives the source position of the call.
*/
{
  an_expr_node_ptr call_node;
  a_type_ptr       return_type;

  function_type = skip_typerefs(function_type);
  /* Make the function call expression node. */
  call_node = func_call_expr(function_node, function_type, is_virtual,
                             virtual_suppressed, compiler_generated, call_pos);
  /* Make an operand for the overall call (etc.). */
  make_expression_operand(call_node, call_node->type, result);
  result->position = *call_pos;
  /* A function call returning a reference is an lvalue. */
  return_type = function_type->variant.routine.return_type;
  if (is_reference_type(return_type)) {
    conv_object_pointer_to_lvalue(result);
    call_node->implicit_reference_indirection = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_bugs && !C_mode() &&
             is_class_struct_union_type(return_type)) {
    /* In Microsoft C++ mode, a function that returns a class type is
       considered to return an lvalue. */
    conv_class_operand_to_object_pointer(result);
    conv_object_pointer_to_lvalue(result);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* make_function_call */


void assemble_function_call(an_operand        *function_operand,
                            an_operand        *bound_function_selector,
                            an_expr_node_ptr  argument_list,
                            a_boolean         compiler_generated,
                            a_source_position *call_position,
                            an_operand        *result)
/*
Assemble a function call from the various pieces.  *function_operand
identifies the function to be called.  If a selector object is needed,
it is provided by *bound_function_selector.  argument_list points to the
(explicit) argument list.  compiler_generated is TRUE if this is a compiler-
generated call (e.g., for an implicit conversion via a conversion
function).  call_position gives the source position of the call.
An operand for the overall call is constructed in *result.
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
    /* Make the function address node.  This might have type pointer-to-
       member-function in a case like (p->*pmf)(). */
    function_node = make_node_from_operand(function_operand);
    if (is_ptr_to_member_type(function_node->type)) {
      /* Call using a pointer-to-member-function. */
      function_type = pm_member_type(function_node->type);
    } else {
      /* Normal call using a pointer to function. */
      function_type = type_pointed_to(function_node->type);
    }  /* if */
    if (function_operand->bound_function) {
      /* Bound function.  bound_function_selector indicates the object. */
      implicit_this_argument = make_node_from_operand(bound_function_selector);
      /* Cast if necessary to handle any const etc. adjustment. */
      /* There might be a cast to a base class here if the function has
         been projected into a derived class with a using declaration.
         No access checking is done on the cast, because the using
         declaration adjusts access. */
      cast_node(&implicit_this_argument,
                implicit_this_param_type_of(function_type),
                /*check_cast_access=*/FALSE,  /* sic */
                /*is_implicit_cast=*/TRUE,
                /*is_reinterpret_cast=*/FALSE,
                /*reinterpret_semantics=*/FALSE,
                &bound_function_selector->position);
      /* Pass a "this" pointer as the first argument. */
      implicit_this_argument->next = argument_list;
      argument_list = implicit_this_argument;
    }  /* if */
    /* Make the call node. */
    function_node->next = argument_list;
    make_function_call(function_node, function_type,
                       (a_boolean)function_operand->virtual_function, 
                       (a_boolean)function_operand->is_qualified_name,
                       compiler_generated, call_position, result);
  }  /* if */
  result->position = *call_position;
}  /* assemble_function_call */


a_statement_ptr make_call_assignment_statement(
                                            a_routine_ptr     rout,
                                            a_boolean         suppress_virtual,
                                            an_expr_node_ptr  dest,
                                            an_expr_node_ptr  source,
                                            a_source_position *err_pos)
/*
Create an expression statement pointing to a call operator that
calls "rout" to assign the lvalue "source" to the lvalue "dest".  rout
is called non-virtually if suppress_virtual is TRUE.  Return a pointer
to the statement.  *err_pos is a source position to be used for errors
(e.g., the function has an invalid return type).  This routine is
intended to be called from outside of the expression routines.
*/
{
  a_statement_ptr         stmt;
  an_expr_node_ptr        node, func_addr_node;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Even though this is not an expression scan, make sure the expr_stack
     has something on it.  If there is already something on the stack,
     save it, clear the stack, and restore it later. */
  saved_expr_stack = expr_stack;
  expr_stack = NULL;
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/TRUE);
  if (source->kind == (an_expr_node_kind)enk_object_lifetime) {
    /* The source expression already has an object lifetime on top, so
       use that as the lifetime for the entire expression.  This happens
       when a call is generated to an operator= that takes its parameter
       by value, for a class that has a destructor. */
    an_object_lifetime_ptr lifetime = source->variant.object_lifetime.ptr;
    unbind_object_lifetime(lifetime);
    lifetime->parent_lifetime = curr_object_lifetime;
    curr_object_lifetime = lifetime;
    expr_stack->lifetime = lifetime;
    source = source->variant.object_lifetime.expr;
  }  /* if */
  /* Make a node for the address of the function. */
  func_addr_node = function_addr_expr(rout, /*set_address_taken_flag=*/FALSE);
  /* Link the operands to the function address node. */
  func_addr_node->next = dest;
  dest->next = source;
  /* Make the call node. */
  node = func_call_expr(func_addr_node, rout->type,
                        rout->is_virtual && !suppress_virtual,
                        rout->is_virtual && suppress_virtual,
                        /*compiler_generated=*/TRUE, err_pos);
  node = wrap_up_full_expression(node);
  /* Allocate the statement. */
  stmt = alloc_expr_statement(node);
  pop_expr_stack();
  expr_stack = saved_expr_stack;
  return stmt;
}  /* make_call_assignment_statement */


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

  if (is_operation_node(node)) {
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
  a_targ_size_t    field_size, type_size;
  a_targ_alignment type_alignment, struct_alignment;
  an_integer_kind  int_kind;
  a_type_ptr       int_type;

  /* In strict ANSI mode, don't allow this. */
  if (!strict_ansi_mode) {
    /* See if the bit field is an even number of bytes long. */
    field_size = field->bit_size;
    if (field_size > 0 && (field_size % targ_char_bit == 0)) {
      field_size /= targ_char_bit;
      /* See if the bit field is at an even byte offset. */
      if (field->offset_bit_remainder == 0) {
        /* Get the overall alignment of the structure of which this field is
           a member. */
        struct_alignment = field->source_corresp.parent.class_type->alignment;
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
                field->offset % type_alignment == 0) {
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
  an_operand       orig_operand;

  check_assertion(is_expression_operand(operand));
  /* Save the operand's source position, etc. */
  orig_operand = *operand;
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
      cast_node(&node, ptr_type, /*check_cast_access=*/TRUE,
                /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                /*reinterpret_semantics=*/FALSE, &operand->position);
      /* Make an rvalue operand for the address. */
      make_expression_operand(node, ptr_type, operand);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc.  Restore the references too. */
  restore_operand_details_incl_ref(operand, &orig_operand);
  if (address_taken) {
    /* Change the kind in the reference entries to address-taken. */
    /* This will check for taking the address of a register variable. */
    change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
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
  if (is_error_operand(operand)) {
    /* Leave an error operand alone. */
  } else {
#if CHECKING
    if (!is_an_lvalue(operand)) {
#if DEBUG
      db_operand(operand);
#endif /* DEBUG */
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
      if (operand->type != type_of_unknown_templ_param_nontype) {
        operand->type = make_pointer_type(operand->type);
      }  /* if */
      /* Change the kind in the reference entries to address-taken. */
      /* This will check for taking the address of a register variable. */
      change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
    }  /* if */
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
    if (is_template_param_type(operand->type)) {
      operand->type = type_of_unknown_templ_param_nontype;
    } else {
      operand->type = type_pointed_to(operand->type);
    }  /* if */
    if (is_function_type(operand->type) ||
        /* Address of an unknown function in a prototype instantiation should
           be a function designator. */
        (is_constant_operand(operand) &&
         operand->variant.constant.kind ==
                                     (a_constant_repr_kind)ck_template_param &&
         (operand->variant.constant.variant.template_param.kind ==
                      (a_template_param_constant_kind)tpck_unknown_function ||
          operand->variant.constant.variant.template_param.kind ==
                      (a_template_param_constant_kind)tpck_template_ref))) {
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
      set_variable_address_taken(node->variant.variable);
      node->implicit_reference_indirection = FALSE;
    }  /* if */
  } else if (is_constant_node(node)) {
    a_constant_ptr con = node->variant.constant;
    if (con->kind == (a_constant_repr_kind)ck_template_param &&
        con->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_member) {
      /* The value of a member of a nonreal class.  Change it to the
         address of the member. */
      possible = TRUE;
      if (!see_if_possible) {
        a_constant addr_con;
        clear_constant(&addr_con, (a_constant_repr_kind)ck_template_param);
        set_template_param_constant_kind(
                                 &addr_con,
                                 (a_template_param_constant_kind)tpck_address);
        addr_con.variant.template_param.variant.constant = con;
        addr_con.type = type_of_unknown_templ_param_nontype;
        node->variant.constant = alloc_shareable_constant(&addr_con);
      }  /* if */
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
          node->variant.operation.returns_lvalue_instead_of_usual_rvalue= TRUE;
          conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                                   /*see_if_possible=*/FALSE);
          conv_class_rvalue_expr_to_object_pointer(&op3, &op3_possible,
                                                   /*see_if_possible=*/FALSE);
          op1->next = op2;
          op2->next = op3;
        }  /* if */
      }  /* if */
    } else if (op == (an_expr_operator_kind)eok_comma ||
               op == (an_expr_operator_kind)eok_points_to_static ||
               op == (an_expr_operator_kind)eok_lvalue_dot_static ||
               op == (an_expr_operator_kind)eok_rvalue_dot_static) {
      /* "," operator -- try to transform the second operand to an lvalue. */
      /* Same processing for static selection. */
      op1 = node->variant.operation.operands;
      op2 = op1->next;
      conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                               /*see_if_possible=*/TRUE);
      if (op2_possible) {
        possible = TRUE;
        if (!see_if_possible) {
          node->variant.operation.returns_lvalue_instead_of_usual_rvalue= TRUE;
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
      op2 = op1->next;
      /* See if the operand can be rewritten. */
      conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                               /*see_if_possible=*/TRUE);
      if (op1_possible) {
        possible = TRUE;
        if (!see_if_possible) {
          conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                                   /*see_if_possible=*/FALSE);
          node->variant.operation.operands = op1;
          op1->next = op2;
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
    } else if (op == (an_expr_operator_kind)eok_cast) {
      /* Generic cast, in a prototype instantiation.  Try to rewrite
         the operand, and change the cast to a pointer cast. */
      op1 = node->variant.operation.operands;
      /* See if the operand can be rewritten. */
      conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                               /*see_if_possible=*/TRUE);
      if (op1_possible) {
        possible = TRUE;
        if (!see_if_possible) {
          conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                                   /*see_if_possible=*/FALSE);
          node->variant.operation.operands = op1;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_error_node(node)) {
    /* An error node stays the same. */
    possible = TRUE;
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
  if (is_error_operand(operand)) {
    /* Error operand -- leave alone. */
#if CHECKING
  } else if (!is_class_struct_union_type(operand->type) &&
             !is_template_param_type(operand->type)) {
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
        /* Avoid recursion loops if the class does not allow bitwise copy.
           The conversion to an object pointer really must succeed (i.e.,
           it's not merely an optimization) if a "real" copy constructor
           would have to be used, since in that case we would need the
           address of this rvalue to be able to call the copy constructor. */
        /* Ignore template parameter cases. */
        if (is_class_struct_union_type(operand->type)) {
          a_class_symbol_supplement_ptr cssp =
                                    symbol_supplement_for_class(operand->type);
          if (!cssp->construction_by_bitwise_copy_allowed) {
            /* Cases like this can come up when an implicitly-generated
               copy constructor is later defined explicitly outside the
               class. */
#if CHECKING
            if (total_errors == 0) {
#if DEBUG
              db_expression(node);
#endif /* DEBUG */
              internal_error(
              "conv_class_operand_to_object_pointer: couldn't convert to ptr");
            }  /* if */
#endif /* CHECKING */
            conv_to_error_operand(operand);
            optimized_case = TRUE;
          }  /* if */
        }  /* if */
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


a_constant_ptr value_of_constant_var_lvalue_expr(an_expr_node_ptr node,
                                                 a_variable_ptr   *p_var)
/*
node is an expression for the address of an lvalue.  If it is an lvalue
for a constant-valued variable, return a pointer to the constant that is
the variable's value.  Otherwise, return NULL.  If p_var is non-NULL and
the expression is an lvalue for a variable, *p_var is set to point to the
variable.
*/
{
  a_constant_ptr con_var_value = NULL;
  a_variable_ptr var = NULL;

  if (p_var != NULL) *p_var = NULL;
  if (is_constant_node(node)) {
    a_constant_ptr con = node->variant.constant;
    if (con_is_exact_addr_of_variable(con)) {
      /* The lvalue is the address of an variable. */
      var = con->variant.address.variant.variable;
    }  /* if */
  } else if (is_variable_address_node(node)) {
    /* The lvalue address an enk_variable_address node. */
    var = node->variant.variable;
  }  /* if */
  if (var != NULL) {
    /* The expression is an lvalue for a variable. */
    if (p_var != NULL) *p_var = var;
    /* See if the variable has a constant value known at compile time. */
    con_var_value = var_constant_value(var);
  }  /* if */
  return con_var_value;
}  /* value_of_constant_var_lvalue_expr */


static an_expr_node_ptr conv_lvalue_expr_to_rvalue(
                                               an_expr_node_ptr node,
                                               a_boolean        *constant_case,
                                               a_constant_ptr   *con_value)
/*
node is an expression that is the address for an lvalue.  Create an
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
  a_constant_ptr con_expr_value = NULL;
  a_boolean      optimized_case = FALSE;

  *constant_case = FALSE;
  if (con_value != NULL) *con_value = NULL;
  if (C_dialect == C_dialect_cplusplus) {
    /* Look for constant-valued variables in C++. */
    a_variable_ptr variable;
    con_expr_value = value_of_constant_var_lvalue_expr(node, &variable);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
    if (con_expr_value != NULL) {
      /* Below, we'll record the expression for the constant, so make the
         rvalue version of the expression. */
      node = expr_to_record_for_variable(variable, /*is_lvalue=*/FALSE);
    }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  }  /* if */
  if (con_expr_value == NULL) {
    /* Do the transformation on the expression node. */
    if (is_operation_node(node)) {
      an_expr_operator_kind op = node->variant.operation.kind;
      an_expr_node_ptr      op1 = node->variant.operation.operands, op2, op3;
      a_boolean             constant_case2, constant_case3;
      if (node->variant.operation.returns_lvalue_instead_of_usual_rvalue) {
        /* Operation that returns an lvalue where the C case would return
           an rvalue. */
        if (op == (an_expr_operator_kind)eok_question) {
          /* "?" operator.  Convert each branch to an rvalue.  This is
             particularly useful for a case like
               &(i ? j : k)
             (only valid in C++). */
          op2 = op1->next;
          op3 = op2->next;
          op1->next = op2 = conv_lvalue_expr_to_rvalue(op2, &constant_case2,
                                                       (a_constant_ptr *)NULL);
          op2->next = op3 = conv_lvalue_expr_to_rvalue(op3, &constant_case3,
                                                       (a_constant_ptr *)NULL);
          *constant_case = constant_case2 && constant_case3;
          /* If all three operands are now constant, the overall result is
             constant. */
          if (is_constant_node(op1) &&
              is_constant_node(op2) &&
              is_constant_node(op3) &&
              constant_bool_value_known_at_compile_time(
                                                      op1->variant.constant)) {
            an_expr_node_ptr result =
                          is_false_constant(op1->variant.constant) ? op3 : op2;
            con_expr_value = result->variant.constant;
          }  /* if */
        } else if (op == (an_expr_operator_kind)eok_comma ||
                   op == (an_expr_operator_kind)eok_points_to_static ||
                   op == (an_expr_operator_kind)eok_lvalue_dot_static ||
                   op == (an_expr_operator_kind)eok_rvalue_dot_static) {
          /* Comma operator.  Apply the transformation to the second operand
             of the ",".  This is useful for a case like
               (p = f(x), *p)
          */
          /* The same processing applies to a static selection operation. */
          op2 = op1->next;
          op1->next = conv_lvalue_expr_to_rvalue(op2, &constant_case2,
                                                 (a_constant_ptr *)NULL);
          *constant_case = constant_case2;
        } else {
          /* The operation is an assignment or prefix ++/-- that returns an
             lvalue.  Change it to one that returns an rvalue. */
        }  /* if */
        optimized_case = TRUE;
        node->variant.operation.returns_lvalue_instead_of_usual_rvalue = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (con_expr_value != NULL) {
    *constant_case = TRUE;
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
    if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded() &&
        node != NULL) {
      /* Record the expression in the constant.  The expression "node" has
         already been converted to rvalue form. */
      /* Make a distinct copy of the constant so we can change it. */
      con_expr_value = alloc_unshared_constant(con_expr_value);
      con_expr_value->expr = node;
    }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    if (con_value != NULL) {
      /* The caller wants the constant instead of an expression node for
         the constant. */
      *con_value = con_expr_value;
      node = NULL;
    } else {
      /* The caller wants an expression node for the constant. */
      node = alloc_node_for_constant(con_expr_value);
    }  /* if */
  } else if (optimized_case) {
    /* For the optimized cases, set the node type to the type pointed to. */
    if (is_template_param_type(node->type)) {
      node->type = type_of_unknown_templ_param_nontype;
    } else {
      node->type = type_pointed_to(node->type);
      /* Drop type qualifiers as appropriate for an rvalue.  Note that no
         cast is needed to drop the qualifiers: an IL shorthand applies in
         this case. */
      if (is_qualified_type(node->type)) {
        node->type = rvalue_type(node->type);
      }  /* if */
    }  /* if */
  } else {
    /* Not an optimized case.  Just add an indirection.  This also drops
       the type qualifiers as appropriate. */
    node = add_indirection_to_node(node);
  }  /* if */
  return node;
}  /* conv_lvalue_expr_to_rvalue */


static void conv_lvalue_in_string_to_char_rvalue(an_operand *operand,
                                                 a_boolean  *optimized_case)
/*
"operand" is an lvalue for the address of a string.  Convert it to an
rvalue for the character value at the proper position in the string, and
return *optimized_case = TRUE.  If the character cannot be extracted,
return without setting *optimized_case to TRUE.
*/
{
  /* The address constant must have type "pointer to char" (signed or
     unsigned) for this optimization to work. */
  a_type_ptr addr_type = operand->variant.constant.type;
  if (is_pointer_type(addr_type)) {
    a_type_ptr char_type = f_skip_typerefs(type_pointed_to(addr_type));
    if (is_character_type(char_type)) {
      a_constant_ptr con = &operand->variant.constant;
      a_constant_ptr string_constant = con->variant.address.variant.constant;
      /* Check that the offset is within the string. */
      a_targ_ptrdiff_t offset = con->variant.address.offset;
      if (offset >= 0 &&
          (a_targ_size_t)offset < string_constant->variant.string.length) {
        /* The address is a valid address of a character in the string.
           Build an operand for the character from the string. */
        an_integer_kind ikind = char_type->variant.integer.int_kind;
        a_host_large_integer char_value;
        char_value = string_constant->variant.string.value[offset];
        /* Remove any sign extension. */
        char_value &= (long)(~((~(unsigned long)0) << targ_char_bit));
        clear_operand((an_operand_kind)ok_constant, operand);
        set_integer_constant(&operand->variant.constant, char_value, ikind);
        /* Sign-extend the character if necessary. */
        if (int_kind_is_signed[(int)ikind]) {
          sign_extend_integer_value(&operand->variant.constant.
                                                         variant.integer_value,
                                    (int)targ_char_bit);
        }  /* if */ 
        operand->type = char_type;
        operand->state = (an_operand_state)os_rvalue;
        *optimized_case = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* conv_lvalue_in_string_to_char_rvalue */


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
  an_expr_node_ptr operand_node, cast_expr;
  a_type_ptr       operand_type, cast_orig_type;
  a_boolean        constant_case = FALSE, qualifiers_dropped = FALSE;
  a_constant_ptr   con_value;

  /* Ignore non-lvalues. */
  if (is_an_lvalue(operand)) {
    /* An lvalue becomes an rvalue. */
    operand_type = operand->type;
#if CHECKING
    /* Array rvalues are not allowed. */
    if (is_array_type(operand_type)) {
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
    /* Instantiate the type if it is a template. */
    complete_type_is_needed(operand_type);
    if (is_error_operand(operand)) {
      /* Error operand -- leave it alone (but make sure it's not an lvalue
         anymore). */
      conv_to_error_operand(operand);
    } else if (is_incomplete_type(operand_type) &&
               (!C_mode() || !is_void_type(operand_type))) {
      /* Converting an lvalue with incomplete type to an rvalue is an
         error in C++ ([conv.lval]), and undefined behavior in C (ISO C
         6.2.2.1).  We treat it as an error in C mode except when the
         lvalue has (possibly cv-qualified) void type.  That latter
         qualification is needed to pass DR 106. */
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
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
            /* Save the expression for the constant. */
            operand->variant.constant.expr =
                              expr_to_record_for_variable(variable,
                                                          /*is_lvalue=*/FALSE);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
            constant_case = TRUE;
          } else {
            /* Not constant-valued; the rvalue is the value of the variable. */
            node = var_rvalue_expr(variable);
            qualifiers_dropped = TRUE;
            make_expression_operand(node, node->type, operand);
          }  /* if */
        } else if (con->kind == (a_constant_repr_kind)ck_template_param &&
                   con->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_address) {
          /* The constant is the address of a member of a nonreal class.
             The rvalue is the value of the member. */
          a_constant_ptr memcon = con->variant.template_param.variant.constant;
          check_assertion(memcon->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
                          memcon->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_member);
          make_constant_operand(memcon, operand);
          constant_case = TRUE;
        } else {
          /* Not the address of a variable.  Check for something like
             "abc"[2]. */
          a_boolean optimized_case = FALSE;
          if (con->kind == (a_constant_repr_kind)ck_address &&
              con->variant.address.kind== (an_address_base_kind)abk_constant &&
              con->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string) {
            /* The lvalue address is an address within a string constant.
               Therefore, the rvalue is the value of the character at that
               position. */
            conv_lvalue_in_string_to_char_rvalue(operand, &optimized_case);
            /* Suppress an error except in strict mode. */
            if (optimized_case && !strict_ansi_mode) constant_case = TRUE;
          }  /* if */
          if (!optimized_case) {
            /* Not the address of a variable or string; add an indirection. */
            node = alloc_node_for_constant(&operand->variant.constant);
            node = add_indirection_to_node(node);
            qualifiers_dropped = TRUE;
            make_expression_operand(node, node->type, operand);
          }  /* if */
        }  /* if */
      } else {
#if CHECKING
        /* Since the expression is not a constant, it must be an expression. */
        if (!is_expression_operand(operand)) {
#if DEBUG
          db_operand(operand);
#endif /* DEBUG */
          internal_error("conv_lvalue_to_rvalue: addr not constant or expr");
        }  /* if */
#endif /* CHECKING */
        /* The lvalue address is represented by some kind of expression
           node. */
        node = operand->variant.expression;
        if (is_operation_node(node) &&
            node->variant.operation.kind ==
                                      (an_expr_operator_kind)eok_lvalue_cast) {
          /* In certain modes, lvalues cast to another type can stay lvalues.
             This is indicated by casting the lvalue address to
             pointer-to-new-type using an eok_lvalue_cast.  Here, turn
             such a case back into an ordinary cast on the rvalue. */
          cast_expr = node;
          operand_node = cast_expr->variant.operation.operands;
          cast_orig_type = type_pointed_to(cast_expr->type);
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
            cast_operand(cast_orig_type, operand, /*check_cast_access=*/FALSE,
                         /*is_implicit_cast=*/FALSE,
                         /*is_reinterpret_cast=*/FALSE,
                         /*reinterpret_semantics=*/FALSE);
          } else {
            /* The cast node can be reused (usual case). */
            operand->type = cast_expr->type = cast_orig_type;
            cast_expr->variant.operation.kind =
                                               (an_expr_operator_kind)eok_cast;
            /* The expression pointer may have been changed in the 
               conversion to lvalue, so put it in the cast node again. */
            cast_expr->variant.operation.operands =
                                                   operand->variant.expression;
            operand->variant.expression = cast_expr;
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
      /* Drop type qualifiers on the operand type as appropriate (if they
         have not been dropped already). */
      if (!qualifiers_dropped && is_qualified_type(operand->type)) {
        a_type_ptr new_type = rvalue_type(operand->type);
        if (is_expression_operand(operand)) {
          /* For an expression node, just change the expression type.
             That's an IL shorthand form for this case, and avoids a
             cast to a struct or union type. */
          operand->type = operand->variant.expression->type = new_type;
        } else {
          /* For other cases (including constants), do the cast the normal
             way. */
          cast_operand(new_type, operand, /*check_cast_access=*/TRUE,
                       /*is_implicit_cast=*/TRUE,
                       /*is_reinterpret_cast=*/FALSE,
                       /*reinterpret_semantics=*/FALSE);
        }  /* if */
      }  /* if */
      if (curr_expr_kind_is_const() && !constant_case) {
        /* An lvalue cannot be converted to an rvalue in a constant
           expression.  The constant_case flag indicates cases where a
           constant-valued variable has been replaced by its value,
           which is allowed in C++. */
        operand->position = orig_operand.position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        operand->end_position = orig_operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        error_in_operand(ec_expr_not_constant, operand);
      }  /* if */
    }  /* if */
    /* Restore the operand's source position. */
    restore_operand_details(operand, &orig_operand);
    /* The ref_entries_list is cleared because it should only contain
       information on lvalue addresses. */
    operand->ref_entries_list = NULL;
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


static an_expr_node_ptr conv_array_rvalue_expr_to_object_pointer(
                                                         an_expr_node_ptr expr)
/*
expr is an expression tree for an array rvalue.  Make an expression for
an object pointer to the array, and return a pointer to it.  Can be called
in both C++ and C modes.  Calls itself recursively with expressions from the
subtree, some of which will no longer have array type.
*/
{
  if (is_operation_node(expr) &&
      expr->variant.operation.kind == (an_expr_operator_kind)eok_value_field) {
    /* Rewrite an rvalue field selection by rewriting the rvalue as an
       lvalue address, and then using a normal eok_field selection. */
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    an_expr_node_ptr op1_next = op1->next;
    an_expr_node_ptr op1_modified;

    op1->next = NULL;
    op1_modified = conv_array_rvalue_expr_to_object_pointer(op1);
    op1_modified->next = op1_next;
    if (op1_modified != op1) {
      expr->variant.operation.operands = op1_modified;
    }  /* if */
    expr->type = make_pointer_type(expr->type);
    expr->variant.operation.kind = (an_expr_operator_kind)eok_field;
  } else if (is_operation_node(expr) &&
             expr->variant.operation.kind ==
                                         (an_expr_operator_kind)eok_indirect) {
    /* In cases like X().arr, where arr is a member of a base class of the
       class of X, the top operator is an indirection.  Remove it. */
    expr = expr->variant.operation.operands;
  } else {
    /* We should have worked our way up to a class rvalue, because the only
       way to produce an array rvalue is to select one out of a class
       rvalue. */
    a_type_ptr expr_type = expr->type;
    check_assertion(is_class_struct_union_type(expr_type));
    if (!C_mode()) {
      /* C++ mode.  Use the normal mechanism to get a pointer to the
         class object. */
      an_operand operand;

      make_expression_operand(expr, expr_type, &operand);
      conv_class_operand_to_object_pointer(&operand);
      expr = make_node_from_operand(&operand);
    } else {
      /* C mode.  Use an eok_lvalue_from_struct_rvalue node. */
      expr = make_operator_node(
                          (an_expr_operator_kind)eok_lvalue_from_struct_rvalue,
                          make_pointer_type(expr->type), expr);
    }  /* if */
  }  /* if */
  return expr;
}  /* conv_array_rvalue_expr_to_object_pointer */


static void conv_array_rvalue_to_lvalue(an_operand *operand)
/*
operand is an array rvalue.  Convert it to an lvalue for the array.
*/
{
  an_expr_node_ptr expr;

  check_assertion(is_expression_operand(operand) && is_an_rvalue(operand) &&
                  is_array_type(operand->type));
  expr = operand->variant.expression;
  expr = conv_array_rvalue_expr_to_object_pointer(expr);
  make_expression_operand(expr, expr->type, operand);
  conv_object_pointer_to_lvalue(operand);
}  /* conv_array_rvalue_to_lvalue */


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

  if (is_array_type(operand->type)) {
    if (is_an_rvalue(operand) && !C_mode()) {
      /* In C++ (but not in C), an array rvalue is converted to a pointer
         to its first element.  Make an lvalue so the conversion below
         will apply. */
      conv_array_rvalue_to_lvalue(operand);
    }  /* if */
    if (is_an_lvalue(operand)) {
      /* An array lvalue -- convert to a pointer. */
      an_operand orig_operand;
      orig_operand = *operand;
      /* Convert to an rvalue that is the pointer, and change its type
         from pointer-to-array to pointer-to-array-element. */
      ptr_type = type_after_array_to_pointer_transformation(operand->type);
      take_address_of_lvalue(operand);
      cast_operand(ptr_type, operand, /*check_cast_access=*/TRUE,
                   /*is_implicit_cast=*/TRUE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
      /* Restore the original source position, etc.  Keep the
         reference entries because if the pointer to the array is
         used in a subscript operation or the like we would like to
         change the kind of reference back to modified or used
         instead of address-taken. */
      restore_operand_details_incl_ref(operand, &orig_operand);
      operand->is_simple_string_literal= orig_operand.is_simple_string_literal;
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
                                  rout->source_corresp.parent.class_type);
  } else {
    /* Nonmember function. */
   ptr_type = make_pointer_type(arg_type);
  }  /* if */
  return ptr_type;
}  /* type_after_function_to_pointer_transformation */


void conv_sym_for_member_operand_to_ptr_to_member(an_operand *operand)
/*
Convert an operand for a member symbol into the corresponding pointer
to member constant.
*/
{
  an_operand   orig_operand;
  a_symbol_ptr member_sym, member_proj_sym;

  orig_operand = *operand;
  check_assertion(is_sym_for_member_operand(operand));
  member_proj_sym = operand->variant.symbol;
  member_sym = fundamental_symbol_of(member_proj_sym);
  /* Make an operand for a pointer-to-member constant. */
  make_ptr_to_member_constant_operand(member_sym, member_proj_sym,
                                      &orig_operand.position,
                                      !operand->access_control_error_reported,
                                      (a_boolean)operand->is_qualified_name,
                                      (a_boolean)operand->
                                                      is_operand_of_address_of,
                                      operand);
  /* Change the kind in the reference entries to address-taken. */
  change_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN);
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* conv_sym_for_member_operand_to_ptr_to_member */


void conv_function_designator_to_ptr_to_function(an_operand *operand,
                                                 a_boolean  allow_ctor)
/*
Convert a function designator operand to a pointer to function expression 
operand.  allow_ctor is TRUE if this is allowed if the operand is a
constructor (ordinarily, taking the address of a constructor is not
allowed).
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
      if ((!allow_ctor &&
           rout->special_kind == (a_special_function_kind)sfk_constructor) ||
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
    /* Convert a member name to a pointer-to-member. */
    conv_sym_for_member_operand_to_ptr_to_member(operand);
  } else {
#if CHECKING
    if (!is_indefinite_function_operand(operand)) {
#if DEBUG
      db_operand(operand);
#endif /* DEBUG */
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
a type.  Also do the lvalue --> rvalue type transformation (dropping
qualifiers as appropriate).  If operand != NULL, it is the associated operand
(needed for the member function --> pointer to member function transformation).
*/
{
  if (is_array_type(type)) {
    type = type_after_array_to_pointer_transformation(type);
  } else if (is_function_type(type)) {
    type = type_after_function_to_pointer_transformation(type, operand);
  } else {
    type = rvalue_type(type);
  }  /* if */
  return type;
}  /* do_implicit_type_transformations */

#if MICROSOFT_EXTENSIONS_ALLOWED

void rewrite_property_field_reference(an_operand *operand,
                                      an_operand *put_operand)
/*
*operand is an operand for a reference to a field declared with the
Microsoft C++ extension __declspec(property(...)).  Transform it to a
call of an accessor routine.  The access is a "put" if put_operand
is non-NULL (and *put_operand gives the value to be put); the access
is a "get" if put_operand is NULL.
*/
{
  an_expr_node_ptr  object_expr = operand->variant.property_ref.object;
  a_field_ptr       field = operand->variant.property_ref.field;
  char              *getput_property_name;
  a_source_position operand_position;

  operand_position = operand->position;
  /* Get the "get" or "put" function name from the field. */
  getput_property_name = (put_operand != NULL) ? field->put_property_name :
                                                 field->get_property_name;
  if (getput_property_name == NULL) {
    error_in_operand(put_operand != NULL ? ec_no_put_property :
                                           ec_no_get_property,
                     operand);
  } else {
    a_symbol_locator locator;
    a_symbol_ptr     getput_sym;
    a_type_ptr       class_type, tp;

    /* Look up the "get" or "put" function name in the symbol table to get
       the locator set. */
    clear_locator(&locator, &operand->position);
    (void)find_symbol(getput_property_name,
                      (sizeof_t)strlen(getput_property_name),
                      &locator);
    /* Get the class type from the object pointer expression. */
    class_type = NULL;
    tp = object_expr->type;
    if (is_pointer_type(tp)) {
      tp = type_pointed_to(tp);
      tp = skip_typerefs(tp);
      if (is_class_struct_union_type(tp)) class_type = tp;
    }  /* if */
    if (class_type == NULL) {
      /* Some previous error. */
      conv_to_error_operand(operand);
    } else {
      /* Look for the "get" or "put" function by name in the class. */
      getput_sym = class_qualified_id_lookup(&locator, class_type,
                                             IDL_NO_OPTIONS);
      if (getput_sym == NULL || !is_member_function_symbol(getput_sym)) {
        pos_st_error(put_operand != NULL ? ec_put_property_function_missing :
                                           ec_get_property_function_missing,
                     &operand->position, getput_property_name);
        conv_to_error_operand(operand);
      } else {
        an_operand         function_operand;
        an_operand         bound_function_selector;
        an_arg_operand_ptr arg_operand_list;
        an_expr_node_ptr   argument_list;

        /* Use a projection symbol if there is one. */
        getput_sym = locator.specific_symbol;
        /* Make an operand for the object pointer. */
        make_expression_operand(operand->variant.property_ref.object,
                                operand->variant.property_ref.object->type,
                                &bound_function_selector);
        /* The arg_operand list is the subscript expression list, if any. */
        arg_operand_list = operand->variant.property_ref.subscripts;
        /* The subscript arg_operands will be freed by the overload
           resolution process, so detach them from the operand. */
        operand->variant.property_ref.subscripts = NULL;
        if (put_operand != NULL) {
          /* The last argument for a "put" is the value to be put. */
          an_arg_operand_ptr new_arg_operand = alloc_arg_operand();
          new_arg_operand->operand = *put_operand;
          if (arg_operand_list == NULL) {
            arg_operand_list = new_arg_operand;
          } else {
            an_arg_operand_ptr end_arg_operand_list = arg_operand_list;
            while (end_arg_operand_list->next != NULL) {
              end_arg_operand_list = end_arg_operand_list->next;
            }  /* if */
            end_arg_operand_list->next = new_arg_operand;
          } /* if */
        }  /* if */
        /* Do overload resolution to determine the function to call. */
        if (select_and_prepare_to_call_overloaded_function(
                                            getput_sym,
                                            /*is_template_id=*/FALSE,
                                            (a_template_arg_ptr)NULL,
                                            /*have_selector=*/TRUE,
                                            &bound_function_selector,
                                            arg_operand_list,
                                            /*do_arg_dep_lookup=*/FALSE,
                                            /*try_surrogate_functions=*/FALSE,
                                            /*is_qualified_name=*/FALSE,
                                            ec_no_matching_function,
                                            ec_ambiguous_overloaded_function,
                                            &locator.source_position,
                                            (a_token_sequence_number)0,
                                            &locator.source_position,
                                            &locator.source_position,
                                            (a_source_position *)NULL,
                                            (a_boolean *)NULL,
                                            &function_operand,
                                            &argument_list) == NULL) {
          /* Some error. */
          conv_to_error_operand(operand);
        } else {
          /* Create the function call. */
          assemble_function_call(&function_operand, &bound_function_selector,
                                 argument_list,
                                 /*compiler_generated=*/TRUE,
                                 &operand_position, operand);
          /* Convert lvalue to rvalue, etc. */
          do_operand_transformations(operand, TOPT_NO_OPTIONS);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* rewrite_property_field_reference */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void error_if_indefinite_function(an_operand *operand)
/*
If the given operand is an indefinite function, issue an error and
change the operand to an error operand.
*/
{
  if (is_indefinite_function_operand(operand)) {
    sym_error_in_operand(ec_indeterminate_overloaded_function,
                         operand, operand->variant.symbol);
  }  /* if */
}  /* error_if_indefinite_function */


void do_operand_transformations(an_operand                   *operand,
                                a_transformation_options_set options)
/*
Do some implicit operand transformations on the indicated operand.
The transformations are:
  (1)  Conversion of a function designator to a pointer-to-function.
  (2)  Conversion of an array lvalue to pointer-to-first-element.
  (3)  (Not really a transformation, but...) Checking for indefinite functions.
  (4)  Conversion of an lvalue to an rvalue.
  (5)  Conversion of a reference to a field declared with
       __declspec(property(...)) (a Microsoft extension) to a call of the
       appropriate "get" function.
The flags in options can be used to suppress one or more of these
transformations.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_property_ref_operand(operand)) {
    if (!(options & TOPT_SUPPRESS_RVALUE_PROPERTY_REWRITE)) {
      /* This operand is a field selection for a field declared with
         __declspec(property(...)).  Change it to a call of the appropriate
         "get" function. */
      rewrite_property_field_reference(operand, (an_operand *)NULL);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_array_type(operand->type)) {
    /* An array lvalue or rvalue. */
    if (!(options & TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION)) {
      /* In most contexts, an operand of array type is changed to
         "pointer to first element of array". */
      conv_array_operand_to_pointer_operand(operand);
    }  /* if */
  } else if (is_an_lvalue(operand)) {
    /* A non-array lvalue. */
    if (!(options & TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION)) {
      /* Convert an lvalue to an rvalue. */
      conv_lvalue_to_rvalue(operand);
    }  /* if */
  } else if (is_a_function_designator(operand)) {
    if (is_sym_for_member_operand(operand) ?
                    (options & TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION) :
                    (options & TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION)) {
      /* The applicable pointer-to-function conversion is suppressed. */
    } else {
      /* In most contexts, an entity of type "function returning type"
         is changed to "pointer to function returning type".  
         See section 3.2.2.1 in the ANSI C standard.  Also, member function
         to pointer to member (not a standard conversion, but allowed as
         an accommodation to existing practice). */
      conv_function_designator_to_ptr_to_function(operand,
                    /*allow_ctor=*/(options & TOPT_ADDR_OF_CTOR_ALLOWED) != 0);
    }  /* if */
  }  /* if */
  if (!(options & TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION)) {
    /* Issue an error for an indefinite function (i.e., a C++ overloaded
       function that wasn't called, so we were never able to determine
       which function was intended). */
    error_if_indefinite_function(operand);
  }  /* if */
}  /* do_operand_transformations */


a_boolean still_an_lvalue(a_type_ptr type_before_cast,
			  a_type_ptr type_cast_to)
/*
In pcc, SVR4, and Microsoft C modes certain lvalues when cast remain as lvalues
after the cast.  Return TRUE if a cast with the before/after types given
should leave its result still an lvalue.  This routine is called only in
C mode.
*/
{
  a_boolean is_still_an_lvalue = FALSE;

  type_before_cast = skip_typerefs(type_before_cast);
  type_cast_to = skip_typerefs(type_cast_to);

  check_assertion(C_mode());
  /* The result stays an lvalue if the result type size is the same as the
     source type size.  However, casts involving floats require actual
     changes in representation, and are not lvalue-preserving. */
  if (identical_types(type_cast_to, type_before_cast)) {
    /* Same type, operand stays an lvalue.  This applies in pcc mode,
       SVR4 C mode, and in Microsoft C mode (it also applies in Microsoft
       C++ mode, but that case doesn't get to this routine). */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode &&
             (is_integral_or_enum_type(type_before_cast) ||
              is_pointer_type(type_before_cast)) &&
             is_integral_or_enum_type(type_cast_to)) {
    /* In Microsoft C mode lvalue casts involving integral types of different
       sizes are allowed -- e.g.,
         long l; ++(char)l;   // affects only the low-order 8 bits
       Also casts of pointer types to integral type.
    */
    is_still_an_lvalue = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */

  return is_still_an_lvalue;
}  /* still_an_lvalue */


a_type_ptr boolean_result_type(void)
/*
Return the result type for a boolean expression (one that returns 0 or 1 in
C, and false or true in C++).
*/
{
  a_type_ptr result_type;

  if (bool_is_keyword) {
    /* bool exists (C++), so the result type is bool. */
    result_type = bool_type();
  } else if (curr_expr_kind_is(ek_pp)) {
    /* All integers have a type of long in the preprocessor. */
    result_type = integer_type((an_integer_kind)ik_long);
  } else {
    /* Result type is int. */
    result_type = integer_type((an_integer_kind)ik_int);
  }  /* if */

  return result_type;
}  /* boolean_result_type */


static an_expr_node_ptr normalize_boolean_controlling_expr(
                                                         an_expr_node_ptr expr)
/*
expr is a boolean controlling expression, and the keyword bool is disabled.
Add a "!= 0" test on top of the given expression if necessary to normalize
it, and return a pointer to the possibly-modified expression.
*/
{
  a_boolean  add_ne_0;
  a_constant con;

  if (!is_operation_node(expr)) {
    /* Add an appropriate "!= 0" on top of variable and variable address
       references. */
    add_ne_0 = TRUE;
  } else {
    /* If the top of the expression is not an operator that returns
       a boolean 0/1, add a "!= 0" of the right kind on top. */
    add_ne_0 = !is_operator_returning_bool(expr->variant.operation.kind);
  }  /* if */
  if (add_ne_0) {
    /* Add a "!= 0" of the appropriate type on top of the expression
       to standardize it. */
    a_type_ptr type = expr->type;
    if (is_integral_type(type)) {
      /* Simulate the usual arithmetic conversions. */
      type = node_type_after_integral_promotion(expr);
      if (type != expr->type) {
        cast_node(&expr, type,
                  /*check_cast_access=*/FALSE,
                  /*is_implicit_cast=*/TRUE,
                  /*is_reinterpret_cast=*/FALSE,
                  /*reinterpret_semantics=*/FALSE,
                  &error_position);
      }  /* if */
    }  /* if */
    make_zero_of_proper_type(type, &con);
    expr->next = alloc_node_for_constant(&con);
    /* Build a "!=" node of the right kind, pointing to the original
       expression and the zero constant node. */
    expr = make_operator_node(which_binary_operator(tok_ne, type),
                              integer_type((an_integer_kind)ik_int),
                              expr);
    expr->variant.operation.compiler_generated = TRUE;
  }  /* if */
  return expr;
}  /* normalize_boolean_controlling_expr */


a_boolean check_boolean_controlling_expr(an_operand *operand)
/*
Do some checks on a boolean controlling expression (e.g., "i != 0" in 
"(i != 0) ? j : k").  Check that (if bool is enabled) it has bool type
or can be converted to it, or (if bool is disabled) it's a scalar
(arithmetic or pointer) or a pointer to member; return FALSE if not.
Also (if bool is enabled) convert the expression to bool, or (if
bool is disabled) normalize the expression to "!= 0" form if necessary.
This routine does not attempt conversions from class types to built-in
types to get a boolean expression (see process_boolean_controlling_expression).
*/
{
  a_boolean             okay = FALSE;
  an_expr_node_ptr      expr, norm_expr;
  an_expr_operator_kind op;
  an_expr_node_ptr      operand1;
  an_operand            orig_operand;

  /* Save the operand's source position. */
  orig_operand = *operand;
  /* Check for possible misuse of "=" where "==" was intended. */
  if (is_expression_operand(operand)) {
    expr = operand->variant.expression;
    if (is_operation_node(expr)) {
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
    }  /* if */
  }  /* if */
  if (bool_is_keyword) {
    /* bool is enabled.  The expression must have bool type or be convertible
       to bool. */
    if (is_bool_type(operand->type)) {
      okay = TRUE;
    } else {
      /* The expression must be convertible to bool. */
      a_std_conv_descr std_conv;
      if (impl_conversion_possible(operand->type,
                                   is_constant_operand(operand),
                                   (a_boolean)operand->
                                                      is_simple_string_literal,
                                   &operand->variant.constant,
                                   bool_type(),
                                   /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                   /*suppress_extensions=*/FALSE,
                                   ec_expr_not_bool,
                                   &std_conv)) {
        okay = TRUE;
        /* Convert the expression to bool. */
        cast_operand(bool_type(), operand, /*check_cast_access=*/TRUE,
                     /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_semantics=*/FALSE);
      } else {
        error_in_operand(ec_expr_not_bool, operand);
      }  /* if */
    }  /* if */
  } else {
    /* bool is disabled.  The expression must have scalar or pointer to member
       type. */
    if (is_ptr_to_member_type(operand->type)) {
      /* Pointer to member type is okay. */
      okay = TRUE;
    } else {
      /* Check that the operand is a scalar. */
      okay = check_scalar_operand(operand);
    }  /* if */
    if (okay) {
      /* Standardize the operand. */
      switch (operand->kind) {
        case ok_error:
          /* No action. */
          break;
        case ok_expression:
          expr = operand->variant.expression;
          /* Add a "!= 0" of the appropriate type on top of the expression
             if necessary to normalize it. */
          norm_expr = normalize_boolean_controlling_expr(expr);
          if (norm_expr != expr) {
            make_expression_operand(norm_expr, norm_expr->type, operand);
          }  /* if */
          break;
        case ok_constant:
          /* The expression is constant.  Make a standard integer 0 or 1
             constant. */
          if (!constant_bool_value_known_at_compile_time(
                                                 &operand->variant.constant)) {
            /* The value of this constant is not known until link time
               and therefore this has to be left as an expression. */
            expr = alloc_node_for_constant(&operand->variant.constant);
            norm_expr = normalize_boolean_controlling_expr(expr);
            if (operand->variant.constant.kind ==
                                     (a_constant_repr_kind)ck_template_param) {
              /* For a template parameter constant, make a ck_template_param
                 expression constant as the result. */
              make_template_param_expr_constant_operand(norm_expr, operand);
            } else {
              make_expression_operand(norm_expr, norm_expr->type, operand);
            }  /* if */
          } else {
            /* Normal case (constant bool value is known at compile time). */
            make_integer_constant_operand(operand,
                       (a_host_large_integer)(!op_is_false_constant(operand)));
            operand->variant.constant.null_pointer_constant_ruled_out =
                 orig_operand.variant.constant.null_pointer_constant_ruled_out;
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("check_boolean_controlling_expr: bad operand kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
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


void expr_one_time_init(void)
/*
Do one-time initialization of variables related to expression processing.
(Variables that need to be reinitialized with each new translation unit
are handled in expr_init.)
*/
{
  /* Save variables from exprutil.h, exprutil.c, and overload.h that are
     needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_ref_entries),
      pch_saved_var_array_elem(avail_arg_operands),
      pch_saved_var_array_elem(avail_dynamic_init_dtor_fixups),
      pch_saved_var_array_elem(avail_arg_match_summaries),
      pch_saved_var_array_elem(avail_candidate_functions),
#if DEBUG
      pch_saved_var_array_elem(num_arg_operands_allocated),
      pch_saved_var_array_elem(num_ref_entries_allocated),
      pch_saved_var_array_elem(num_dynamic_init_dtor_fixups_allocated),
      pch_saved_var_array_elem(num_arg_match_summaries_allocated),
      pch_saved_var_array_elem(num_candidate_functions_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* expr_one_time_init */


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
