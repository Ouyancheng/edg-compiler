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
#include "mem_manage.h"
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
#include "templates.h"

/*
Information used when creating cross-reference information.  This is
done only when f_xref_info != NULL.
*/
static an_xref_entry_ptr
		avail_xref_entries;
			/* List of cross-reference entries that have been freed
			   and are available for reuse. */
static an_xref_entry_ptr
		curr_expr_xref_entries;
			/* List of all the cross-reference entries for
			   the current expression.  They are written out at
			   the end of the expression.  Before that, the kind
			   of reference each indicates might be adjusted. */

/*
Entry describing a function that is a candidate instance of an overloaded
function.  This entry is used in resolving overloaded function calls.
*/
typedef struct a_candidate_function *a_candidate_function_ptr;
typedef struct a_candidate_function {
  a_candidate_function_ptr
		next;	/* Next entry on the list of candidates, or NULL
			   if this is the last entry. */
  a_symbol_ptr	function_symbol;
			/* Pointer to the symbol for the function.  NULL if
			   the "function" is a built-in operator. */
  a_byte_boolean
		is_function_template;
			/* TRUE if function_symbol is a function template. */
  char		*operand_type_pattern;
			/* For a built-in operator, the operand type pattern
			   string (see operand_type_pattern_for_operator).
			   Specifically, the appropriate one- or two-character
			   segment of the operand pattern string.  NULL
			   if not a built-in operator. */
  a_type_ptr	pointer_type;
			/* For a built-in operator with an operand pattern
			   including pointers, this indicates the pointer
			   type. */
  an_arg_match_summary_ptr
		arg_matches;
			/* List of entries describing how well each actual
			   argument matches this function's corresponding
			   formal parameter.  If there was a selector object
			   (for the "this" parameter), it appears first. */
  an_arg_operand_ptr
		arg_operand_list;
			/* If is_function_template is TRUE, the list of
			   argument operands for the call.  NULL otherwise. */
  /* Fields used by select_best_candidate_functions: */
  an_arg_match_summary_ptr
		current_arg_match;
			/* The argument match entry currently being considered
			   by select_best_candidate_functions. */
  an_arg_match_summary_ptr
		prev_func_arg_match_with_same_match_level;
			/* If non-NULL, points to an argument match summary
			   for the same argument on a previous candidate
			   function that has the same match level.  This is
			   used to keep track of the set of best-matching
			   arguments: they are the set that has this field
			   pointing to the argument match that was chosen as
			   best (i.e., all the argument matches that tied
			   for "best"). */
  a_byte_boolean
		in_best_match_set;
			/* TRUE if the function is in the set of best-matching
			   functions. */
  a_byte_boolean
		in_best_match_set_for_some_argument;
			/* TRUE if the function is in the set of best-matching
			   functions for some argument. */
  a_byte_boolean
		std_conversion_after_conversion_function;
			/* If TRUE, this entry is for a conversion function
			   and a standard conversion is required after the
			   conversion to get to the desired type. */
} a_candidate_function;

/*
Information used in overload resolution:
*/
static an_arg_operand_ptr
		avail_arg_operands;
			/* List of argument operand entries that have been
			   freed and are available for reuse. */
static a_candidate_function_ptr
		avail_candidate_functions;
			/* List of candidate function entries that have been
			   freed and are available for reuse. */
static an_arg_match_summary_ptr
		avail_arg_match_summaries;
			/* List of argument match summary entries that have
			   been freed and are available for reuse. */


/* Declarations needed because of mutual recursion or forward references: */
static a_boolean conversion_to_class_possible(
                                  an_operand               *source_operand,
                                  a_type_ptr               dest_type,
                                  a_routine_ptr            *conversion_routine,
                                  a_boolean                *ambiguous,
                                  a_candidate_function_ptr *ambiguity_list);

static a_boolean conversion_from_class_possible(
                               an_operand               *source_operand,
                               a_type_ptr               dest_type,
                               a_builtin_type_kind_set  builtin_types_allowed,
                               a_routine_ptr            *conversion_routine,
                               a_boolean                *std_conversion_needed,
                               a_boolean                *ambiguous,
                               a_candidate_function_ptr *ambiguity_list);

static void prep_conversion_operand(an_operand         *source_operand,
                                    a_type_ptr         dest_type,
                                    a_boolean          result_may_be_lvalue,
                                    a_boolean          is_initialization,
                                    an_expression_kind expression_kind,
                                    an_error_code      incompatible_err,
                                    a_source_position  *err_pos);


static an_xref_entry_ptr alloc_xref_entry(a_symbol_reference_kind kind,
                                          an_expression_kind   expression_kind,
                                          a_symbol_ptr            sym_ptr,
                                          a_source_position       *pos)
/*
Allocate a cross-reference entry and return a pointer to it.  The
information provided (kind of reference, kind of expression, symbol
pointer, and location of reference) is placed in the entry.  An entry
of this kind is used to hold information on a single reference to a
symbol in an expression.  This is only used when cross-reference
information is being generated.  The information is held, rather than
written immediately, because the kind of reference may be revised as
more of the expression is scanned.
*/
{
  an_xref_entry_ptr xep;

  if (avail_xref_entries != NULL) {
    /* Reuse a previously-freed entry. */
    xep = avail_xref_entries;
    avail_xref_entries = xep->next;
  } else {
    /* Allocate a new entry. */
    xep = (an_xref_entry_ptr)alloc_fe(sizeof(an_xref_entry));
  }  /* if */
  xep->kind = kind;
  xep->expression_kind = expression_kind;
  xep->symbol = sym_ptr;
  copy_source_position(*pos, xep->position);
  xep->next = NULL;
  xep->next_operand_ref = NULL;
  return xep;
}  /* alloc_xref_entry */


static void free_xref_entry(an_xref_entry_ptr xep)
/*
Free the cross-reference entry pointed to by xep.
*/
{
  /* Add the entry to the available list. */
  xep->next = avail_xref_entries;
  avail_xref_entries = xep;
}  /* free_xref_entry */


static void flush_xref_entries_list(void)
/*
If there are any entries on the list of cross-reference entries for the
current expression, output them now.
*/
{
  an_xref_entry_ptr xep;

  while (curr_expr_xref_entries != NULL) {
    xep = curr_expr_xref_entries;
    curr_expr_xref_entries = xep->next;
    /* Go write information on this entry to a file. */
    /* References in not-evaluated expressions should not set the IL
       entry referenced flag. */
    if (xep->expression_kind == (an_expression_kind)ek_not_evaluated) {
      mark_symbol_referenced(xep->kind, xep->symbol, &xep->position);
    } else {
      reference_to_symbol(xep->kind, xep->symbol, &xep->position);
    }  /* if */
    free_xref_entry(xep);
  }  /* while */
}  /* flush_xref_entries_list */


an_xref_entry_ptr xref_entry(a_symbol_ptr            sym_ptr,
                             a_source_position       *source_position,
                             an_expression_kind      expression_kind)
/*
Allocate a cross-reference entry for a reference to the symbol sym_ptr
at position source_position, put the entry on the list of entries for
the current expression, and return a pointer to it.  If cross-reference
information is not being generated, just record a reference immediately,
do not allocate the entry, and return NULL.  expression_kind indicates
the current expression kind.
*/
{
  an_xref_entry_ptr xep;

  if (f_xref_info == NULL) {
    /* Cross-reference information is not being generated. */
    /* References in not-evaluated expressions should not set the IL
       entry referenced flag. */
    if (expression_kind == (an_expression_kind)ek_not_evaluated) {
      mark_symbol_referenced(srk_reference, sym_ptr, source_position);
    } else {
      mark_referenced(sym_ptr, source_position);
    }  /* if */
    xep = NULL;
  } else {
    xep = alloc_xref_entry(srk_reference, expression_kind, sym_ptr,
                           source_position);
    /* Put the entry on the list of entries for the current expression.
       The list is dumped when flush_xref_entries_list is called. */
    xep->next = curr_expr_xref_entries;
    curr_expr_xref_entries = xep;
  }  /* if */
  return xep;
}  /* xref_entry */


void change_xref_kinds(an_xref_entry_ptr       xref_list,
                       a_symbol_reference_kind kind)
/*
Change the kind-of-reference field to "kind" in each of the cross-reference
entries on the list xref_list.
*/
{
  an_xref_entry_ptr xep;

  for (xep = xref_list; xep != NULL; xep = xep->next_operand_ref) {
    xep->kind = kind;
  }  /* for */
}  /* change_xref_kinds */


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
  new_entry->old_xref_entries_list = curr_expr_xref_entries;
  curr_expr_xref_entries = NULL;
  new_entry->is_default_arg_expression = FALSE;
  new_entry->is_template_arg_expression = FALSE;
  new_entry->nested_construct_depth = 0;
  expr_stack = new_entry;
}  /* push_expr_stack */


void pop_expr_stack(void)
/*
Pop the top entry off the expr_stack.  This is done at the end of a
major expression.
*/
{
  /* Flush the cross-reference entries list for the current expression. */
  flush_xref_entries_list();
  /* Restore the old xref entries list, if any. */
  curr_expr_xref_entries = expr_stack->old_xref_entries_list;
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
    case ok_sym_for_ptr_to_member:
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
  operand->position.seq = 0;
  operand->position.column = SP_COL_UNKNOWN;
  operand->xref_entries_list = NULL;
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
      /* Create a constant node and copy the constant in the operand to the
	 node. */
      node = alloc_node_for_constant(&operand->variant.constant);
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


/*ARGSUSED*/ /* <-- Doesn't use operand presently. */
void discard_operand(an_operand *operand)
/*
Discard the indicated operand.  It has been scanned as a normal operand,
but it's now known that it should have been a not-evaluated operand.
This is only used in C++, for left operands of field selections.
*/
{
#if 0
  /* What do do?? */
#endif
}  /* discard_operand */


static void restore_operand_details(an_operand *operand,
                                    an_operand *orig_operand)
/*
*operand has been subjected to some sort of modification, which may have
destroyed its source position, etc.  Restore such things from
*orig_operand, which is a copy of *operand before the modification.
*/
{
  operand->position = orig_operand->position;
  operand->bound_function = orig_operand->bound_function;
}  /* restore_operand_details */


static void restore_operand_details_incl_xref(an_operand *operand,
                                              an_operand *orig_operand)
/*
*operand has been subjected to some sort of modification, which may have
destroyed its source position, etc.  Restore such things from
*orig_operand, which is a copy of *operand before the modification.
Restore the xref_entries_list too (not usually wanted).
*/
{
  restore_operand_details(operand, orig_operand);
  operand->xref_entries_list = orig_operand->xref_entries_list;
}  /* restore_operand_details_incl_xref */


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
  set_operand_kind(operand, (an_operand_kind)ok_error);
  operand->type = error_type();
  operand->state = (an_operand_state)os_none;
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
  clear_constant(&addr_constant, (a_constant_repr_kind)ck_address);
  addr_constant.variant.address.kind = (an_address_base_kind)abk_constant;
  addr_constant.variant.address.variant.constant = string_constant;
  /* Note that the type is left as "pointer to array of char" here;
     the implicit conversion to "pointer to char" is done separately. */
  addr_constant.type = make_pointer_type(constant->type);
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


void make_sym_for_ptr_to_member_operand(a_symbol_ptr      member_sym,
                                        an_xref_entry_ptr xep,
                                        an_operand        *operand)
/*
Make an operand for a class member name, used in C++ for qualified
names that appear in a context that calls for a pointer-to-member.
member_sym points to the symbol entry for the member.  xep points to an
associated cross-reference entry, or is NULL if cross-reference information
is not being maintained.  The operand is put into *operand.
*/
{
  a_symbol_ptr  fund_sym = fundamental_symbol_of(member_sym);
  a_routine_ptr rout;

  clear_operand((an_operand_kind)ok_sym_for_ptr_to_member, operand);
  operand->state = (an_operand_state)os_rvalue;
  operand->type = unknown_type();
  operand->variant.symbol = member_sym;
  copy_source_position(pos_curr_token, operand->position);
  operand->xref_entries_list = xep;
  /* The address of a constructor or destructor may not be taken (ARM 12.1,
     12.4). */
  if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    fund_sym = fund_sym->variant.overloaded_function.symbols;
  }  /* if */
  if (fund_sym->kind == (a_symbol_kind)sk_member_function) {
    rout = fund_sym->variant.routine.ptr;
    if (rout->special_kind == (a_special_function_kind)sfk_constructor ||
        rout->special_kind == (a_special_function_kind)sfk_destructor) {
      error_in_operand(ec_addr_of_constructor_or_destructor, operand);
    }  /* if */
  }  /* if */
}  /* make_sym_for_ptr_to_member_operand */


/*
Test an expression node to see if it's a bit-field extraction.
*/
#define is_bit_field_extract_node(node) \
  (is_operation_node(node) && \
   ((node)->variant.operation.kind == \
                                (an_expr_operator_kind)eok_value_bit_field || \
    (node)->variant.operation.kind == \
                               (an_expr_operator_kind)eok_extract_bit_field))


static void add_base_class_casts(a_base_class_ptr  bcp,
                                 a_boolean         check_cast_access,
                                 a_boolean         is_implicit_cast,
                                 an_expr_node_ptr  *p_node,
                                 a_source_position *err_pos)
/*
Add casts to *p_node to change its type from a pointer to a class type to
a pointer to a base class of that class; bcp indicates the base class.
Access control is done on the cast if check_cast_access is TRUE.
is_implicit_cast is TRUE if the cast is implicit; access control checking
is done on the cast in that case.  *err_pos indicates a source position
to be used for errors.  This routine is only used in C++ mode.
*/
{
  a_boolean             access_okay;
  a_type_ptr            orig_type, curr_type, qual_curr_type;
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
    orig_type = type_pointed_to((*p_node)->type);
    curr_type = skip_typerefs(orig_type);
    for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
      base_class = dsp->base_class;
      /* Check that the base class is accessible from the current class. */
      if (check_cast_access) {
        if (!is_accessible_base_class(base_class, curr_type)) {
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
      /* The type should have all the qualifiers of the original type pointed
         to. */
      qual_curr_type = make_identically_qualified_type(curr_type, orig_type);
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
  } else if (bcp->any_virtual_steps_in_derivation) {
    /* The base class is a virtual base of the derived class. */
    pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                  new_type_pointed_to, bcp->type);
    *p_node = error_node();
  } else {
    /* Use recursion to process the list backwards to generate casts. */
    add_a_derived_class_cast(new_type_pointed_to, bcp->derivation, p_node);
  }  /* if */
}  /* add_derived_class_casts */


a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                      a_type_ptr class_type)
/*
Make a pointer-to-member type having the indicated member type and class
type and return a pointer to it.  If the member_type is a function type,
alter the underlying "this" parameter type to be the new class_type.
This is used when casting a pointer-to-member type to a related class
type.
*/
{
  a_type_ptr type, new_member_type, old_this_type, new_this_type;
  a_type_ptr old_this_underlying_type;

  if (is_function_type(member_type)) {
    /* Alter a function type's "this" parameter. */
    /* Build a type for the new "this" parameter.  Start with the new type
       and build up, adding the qualifiers (both under and over the
       pointer type) from the old "this" type. */
    old_this_type = skip_typerefs(member_type)->variant.routine.extra_info->
                                                      implicit_this_param_type;
    old_this_underlying_type = type_pointed_to(old_this_type);
    new_this_type = make_identically_qualified_type(class_type,
                                                    old_this_underlying_type);
    new_this_type = make_pointer_type(new_this_type);    
    new_this_type = make_identically_qualified_type(new_this_type,
                                                    old_this_type);
    /* Allocate the new function type and copy into it. */
    new_member_type = alloc_type((a_type_kind)tk_routine);
    copy_type(member_type, new_member_type);
    /* Insert the new "this" parameter type. */
    new_member_type->variant.routine.extra_info->implicit_this_param_type =
                                                                 new_this_type;
    member_type = new_member_type;
  }  /* if */
  /* Make the pointer-to-member type. */
  type = ptr_to_member_type(member_type, class_type);
  return type;
}  /* related_ptr_to_member_type */


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
    for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
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
  } else if (bcp->any_virtual_steps_in_derivation) {
    /* The base class is a virtual base of the derived class. */
    pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                  new_class_pointed_to, bcp->type);
    *p_node = error_node();
  } else {
    if (check_cast_access) {
      /* Check the accessibility of the base class.  (Recall that casts
         to derived types can be done implicitly.) */
      curr_type = new_class_pointed_to;
      for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
        base_class = dsp->base_class;
        /* Check that the base class is accessible from the current class. */
        if (!is_accessible_base_class(base_class, curr_type)) {
          pos_ty_error(ec_inaccessible_base_class, err_pos, base_class->type);
          break;
        }  /* if */
        curr_type = base_class->type;
      }  /* for */
    }  /* if */
    /* Use recursion to process the list backwards to generate casts. */
    add_a_pm_derived_class_cast(new_class_pointed_to, bcp->derivation,
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
    if (downward_cast) {
      /* Derived --> base.  Valid unless the cast is ambiguous or
         the base class is inaccessible. */
      add_base_class_casts(bcp, /*check_cast_access=*/is_implicit_cast,
                           is_implicit_cast, p_node, err_pos);
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      add_derived_class_casts(type_pointed_to(new_type), bcp, p_node, err_pos);
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
  } else if (is_error_type(new_type)) {
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
      /* The operand is not constant. */
      /* On a bit-field extract, a cast to an integral type can be folded
         in by setting the node type.  No cast node is required.  This is
         allowed because the extract involves a type change anyway.  It comes
         up most often for the integral promotions (e.g., a 3-bit unsigned
         bit-field becomes int). */
      if (is_bit_field_extract_node(*node) && is_integral_type(new_type)) {
        (*node)->type = new_type;
      } else {
        /* Put in a cast. */
        /* Note that if the constant type-change was attempted, it was
           done on a copy of the constant.  The original constant and
           expression were not changed, and therefore can be used here. */
        add_cast_to_node(node, new_type, is_implicit_cast, err_pos);
      }  /* if */
    } else {
      /* The operation was successfully folded to a constant. */
      (*node)->variant.constant = alloc_shareable_constant(&local_constant);
      (*node)->type = new_type;
    }  /* if */
  }  /* if */
}  /* cast_node */


static a_symbol_ptr find_addr_of_overloaded_function_match(
                                               a_symbol_ptr       ovl_sym,
                                               a_type_ptr         dest_type,
                                               a_source_position  *source_pos,
                                               an_arg_match_level *match_level,
                                               a_boolean          *ambiguous)
/*
ovl_sym is the symbol from an indefinite function operand representing the
address of an overloaded function.  It is being converted to a destination type
dest_type.  If dest_type is a pointer or pointer-to-member type that could be
a pointer to one of the overloaded functions, return a pointer to that
function's symbol; otherwise, return NULL.  Also set *match_level to indicate
whether or not any conversion is needed after the coercion to a specific
function pointer.  If more than one function matches, return NULL and
*ambiguous TRUE.  source_pos is the source position of the reference.
See ARM 13.3, "Address of Overloaded Function".
*/
{
  a_boolean     is_ptr = FALSE, is_ptr_to_member = FALSE;
  a_boolean     sym_is_list, any_function_templates;
  a_type_ptr    routine_type, dest_class, ptr_routine_type;
  a_type_ptr    dest_underlying_type;
  an_error_code warning_suggested;
  a_symbol_ptr  sym, match_sym = NULL, instance_sym;
  unsigned long number_of_matches = 0;

  db_enter(4, "find_addr_of_overloaded_function_match");
  *ambiguous = FALSE;
  if (is_pointer_type(dest_type)) {
    dest_class = NULL;
    is_ptr = TRUE;
    dest_underlying_type = type_pointed_to(dest_type);
  } else if (is_ptr_to_member_type(dest_type)) {
    dest_class = pm_class_type(dest_type);
    is_ptr_to_member = TRUE;
    dest_underlying_type = pm_member_type(dest_type);
  }  /* if */
  if (is_ptr || is_ptr_to_member) {
    /* dest_type is a pointer or pointer-to-member type, but the underlying
       type is not necessarily a function type. */
    dest_underlying_type = skip_typerefs(dest_underlying_type);
    reduce_projection_symbol_to_fundamental_symbol(ovl_sym);
    if (ovl_sym->kind == (a_symbol_kind)sk_function_template) {
      /* A single function template represents multiple instantiations of
         that template. */
      sym_is_list = FALSE;
    } else {
#if CHECKING
      if (ovl_sym->kind != (a_symbol_kind)sk_overloaded_function) {
        internal_error(
                "find_addr_of_overloaded_function_match: not overloaded func");
      }  /* if */
#endif /* CHECKING */
      /* A list of overloaded functions. */
      sym_is_list = TRUE;
      ovl_sym = ovl_sym->variant.overloaded_function.symbols;
    }  /* if */
    /* Check each function in the overload set to see if its type matches
       the one desired.  The algorithm is the one for template matching
       (ARM 14.4):
         (1)  Look for an exact match.
         (2)  Look for a function template that can yield a function with
              exactly the right type.
         (3)  Look for a match involving a conversion (this is possible only
              for pointers-to-members, because there are no implicit
              conversions defined on pointers).
       If there is more than one match at any level, the operation is
       ambiguous.  That's probably possible only when function templates
       are involved. */
    /* Check first for an exact match. */
    any_function_templates = FALSE;
    for (sym = ovl_sym;
         sym != NULL;
         sym = (sym_is_list ? sym->next : NULL)) {
      if (sym->kind == (a_symbol_kind)sk_function_template) {
        /* Function template.  Ignore on this pass, but enable a second pass
           to try matching it. */
        any_function_templates = TRUE;
      } else {
        /* Not a function template (i.e., a normal function). */
        routine_type = routine_symbol_type(sym);
        /* Note that the type qualifiers on both types have already been
           dropped. */
        if (identical_types(routine_type, dest_underlying_type) &&
            sym->class_of_which_a_member == dest_class) {
          /* Exact match. */
          match_sym = sym;
          *match_level = (an_arg_match_level)aml_exact;
          number_of_matches++;
        }  /* if */
      }  /* if */
    }  /* for */
    if (number_of_matches == 0 && any_function_templates &&
        is_function_type(dest_underlying_type)) {
      /* Try matching function templates.  Do not try if the underlying type
         is not a function type. */
      for (sym = ovl_sym;
           sym != NULL;
           sym = (sym_is_list ? sym->next : NULL)) {
        if (sym->kind == (a_symbol_kind)sk_function_template) {
          /* Function template. */
          instance_sym = matching_template_function(sym, dest_underlying_type,
                                                    source_pos);
          if (instance_sym != NULL) {
            /* Template match. */
            match_sym = instance_sym;
            *match_level = (an_arg_match_level)aml_exact;
            number_of_matches++;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches == 0) {
      /* Try matches involving an implicit conversion.  This is here
         primarily for the pointer-to-member case, but it makes sense to
         handle the normal pointer case too in case the implicit conversion
         rules change (also, it makes the error message clearer in the
         case where dest_type is "void *"). */
      for (sym = ovl_sym;
           sym != NULL;
           sym = (sym_is_list ? sym->next : NULL)) {
        if (sym->kind != (a_symbol_kind)sk_function_template) {
          /* Not a function template (i.e., a normal function). */
          routine_type = routine_symbol_type(sym);
          if (is_ptr) {
            ptr_routine_type = make_pointer_type(routine_type);
          } else {
            ptr_routine_type = ptr_to_member_type(routine_type, dest_class);
          }  /* if */
          if (impl_conversion_possible(ptr_routine_type,
                                       /*source_is_constant=*/FALSE,
                                       (a_constant_ptr)NULL,
                                       dest_type,
                                       /*suppress_extensions=*/TRUE,
                                       ec_no_error,
                                       &warning_suggested)) {
            /* A match. */
            match_sym = sym;
            *match_level = (an_arg_match_level)aml_std_conversion;
            number_of_matches++;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
    if (number_of_matches > 1) {
      /* Ambiguous case. */
      *ambiguous = TRUE;
      match_sym = NULL;
    }  /* if */
  } else {
    /* dest_type is not a pointer type, so no function can match. */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    if (match_sym == NULL) {
      fprintf(f_debug, "find_addr_of_overloaded_function_match: %s\n",
              *ambiguous ? "ambiguous" : "no match");
    } else {
      db_symbol(match_sym, "find_addr_of_overloaded_function_match: ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return match_sym;
}  /* find_addr_of_overloaded_function_match */


void cast_operand(a_type_ptr         new_type,
		  an_operand         *operand,
                  an_expression_kind expression_kind,
		  a_boolean          is_implicit_cast)
/*
Cast the operand to the new type.  If is_implicit_cast is TRUE, this
is an implicit cast rather than an explicit one.  If there are any
warnings detected on the type change, issue them only if is_implicit_cast
is TRUE.  The operand must be an rvalue or error operand.
expression_kind indicates the kind of expression being scanned.
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
    if (is_error_type(new_type)) {
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
          copy_constant(&operand->variant.constant, &local_constant);
          type_change_constant(&local_constant, new_type, is_implicit_cast,
                               is_const_expr_kind(expression_kind),
                               &did_not_fold, &operand->position);
          if (did_not_fold) {
            /* Cast of a constant did not fold. */
            if (is_const_expr_kind(expression_kind)) {
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
            copy_constant(&local_constant, &operand->variant.constant);
            operand->type = new_type;
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
                                         &access_error_reported,
                                         expression_kind);
            make_ptr_to_member_constant_operand(function_symbol,
                                                &orig_operand.position,
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
                                         &access_error_reported,
                                         expression_kind);
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("cast_operand: bad operand kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    /* Restore the original source position, etc. */
    restore_operand_details(operand, &orig_operand);
  }  /* if */
}  /* cast_operand */


void conv_selector_to_object_pointer(an_operand         *operand,
                                     a_boolean          *is_arrow_operator,
                                     an_expression_kind expression_kind)
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
    conv_operand_to_object_pointer(operand, expression_kind);
    *is_arrow_operator = TRUE;
  }  /* if */
}  /* conv_selector_to_object_pointer */


void base_class_cast_operand(an_operand         *operand,
                             a_base_class_ptr   bcp,
                             a_boolean          *is_arrow_operator,
                             a_boolean          check_cast_access,
                             an_expression_kind expression_kind)
/*
Cast operand (of class or pointer-to-class type) to its base class
identified by bcp.  If *is_arrow_operator is TRUE, operand is being
used as a pointer ("->"); otherwise, it is being used as an object (".").
*is_arrow_operator will be set to TRUE on return to indicate that the
operation was normalized into "->" form.  expression_kind indicates the
current expression kind.  Do access control checking on the cast if
check_cast_access is TRUE.  This routine is only used in C++ mode.
*/
{
  a_boolean        did_not_fold;
  an_expr_node_ptr node;
  a_constant       temp_con;
  an_operand       orig_operand;

  /* Save the original operand position, etc. */
  orig_operand = *operand;
  /* Convert to "->" form by getting an address for the operand. */
  conv_selector_to_object_pointer(operand, is_arrow_operator,
                                  expression_kind);
  if (is_error_operand(operand)) {
    /* Leave an error operand alone. */
  } else {
    did_not_fold = TRUE;
    if (is_const_expr_kind(expression_kind) &&
        is_constant_operand(operand)) {
      /* Fold a cast of a constant address into another constant address.
         This folding could be done even in non-constant expressions (except 
         not-evaluated ones), but it's clearer to have the cast in
         the IL (the constant form has only an offset, and loses the sequence
         of casts). */
      fold_base_class_cast(&operand->variant.constant, bcp,
                           &temp_con, check_cast_access, &did_not_fold,
                           &orig_operand.position);
    }  /* if */
    if (did_not_fold) {
      /* The cast could not be folded to a constant. */
      if (is_const_expr_kind(expression_kind)) {
        /* The cast must fold to a constant in a constant expression. */
        error_in_operand(ec_expr_not_constant, operand);
      } else {
        /* Build an expression node or nodes for the cast. */
        node = make_node_from_operand(operand);
        add_base_class_casts(bcp, check_cast_access, /*is_implicit_cast=*/TRUE,
                             &node, &orig_operand.position);
        make_expression_operand(node, node->type, operand);
      }  /* if */
    } else {
      /* The cast was folded to a constant. */
      make_constant_operand(&temp_con, operand);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details_incl_xref(operand, &orig_operand);
}  /* base_class_cast_operand */


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


void promote_operand(an_operand         *operand,
                     an_expression_kind expression_kind)
/*
Determine the integral promotion and do the promotion on an operand.
See 3.2.1.1 in the standard.  expression_kind indicates the kind of
expression being scanned.
*/
{
  cast_operand(operand_type_after_integral_promotion(operand), operand,
               expression_kind, /*is_implicit_cast=*/TRUE);
}  /* promote_operand */


void arg_default_promote_operand(an_operand         *argument_operand,
                                 an_expression_kind expression_kind)
/*
Do default argument promotions on an argument operand.
*/
{
  /* Convert the operand to an rvalue if necessary. */
  do_operand_transformations(argument_operand, TOPT_NO_OPTIONS,
                             expression_kind);
  /* Do the integral promotions part of the default argument promotions
     directly on the operand because of the special case with 
     bit-fields (which can't be handled from just the type). */
  if (is_integral_type(argument_operand->type)) {
    promote_operand(argument_operand, expression_kind);
  } else if (is_incomplete_type(argument_operand->type)) {
    /* Catch a case like "f((void)2)" -- an argument with an incomplete
       type is not allowed. */
    error_in_operand(ec_incomplete_type_not_allowed, argument_operand);
  } else {
    cast_operand(default_argument_promotion(argument_operand->type),
                 argument_operand, expression_kind,
                 /*is_implicit_cast=*/TRUE);
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
  a_boolean     operand_1_is_pointer = is_pointer_type(operand_1->type);
  a_boolean     operand_2_is_pointer = is_pointer_type(operand_2->type);
  a_boolean     pointer_normalization_needed;
  an_error_code warning_suggested;

  if (operand_1_is_pointer) {
    /* See if the second operand can be converted to the type of the
       first operand. */
    if (impl_pointer_conversion(operand_2->type,
                                is_constant_operand(operand_2),
                                &operand_2->variant.constant,
                                operand_1->type,
                                /*check_as_operands_not_conversion=*/TRUE,
                                &pointer_normalization_needed,
                                /*suppress_extensions=*/FALSE,
                                ec_incompatible_operands,
                                &warning_suggested)) {
      *operation_type = operand_1->type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay && operand_2_is_pointer) {
    /* See if the first operand can be converted to the type of the
       second operand. */
    if (impl_pointer_conversion(operand_1->type,
                                is_constant_operand(operand_1),
                                &operand_1->variant.constant,
                                operand_2->type,
                                /*check_as_operands_not_conversion=*/TRUE,
                                &pointer_normalization_needed,
                                /*suppress_extensions=*/FALSE,
                                ec_incompatible_operands,
                                &warning_suggested)) {
      *operation_type = operand_2->type;
      okay = TRUE;
    }  /* if */
  }  /* if */
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
          operand_1_type_pointed_to = type_pointed_to(operand_1->type);
          operand_1_type_pointed_to = skip_typerefs(operand_1_type_pointed_to);
        }  /* if */
        if (operand_2_is_pointer) {
          operand_2_type_pointed_to = type_pointed_to(operand_2->type);
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
        pos_diagnostic(strict_ansi_error_severity,
                       ec_incompatible_operands, operator_position);
      }  /* if */
    }  /* if */
    if (warning_suggested != ec_no_error && !nonstd_case) {
      /* Oddball cases call for a warning.  Suppress this if we issued a
         diagnostic about nonstandard use. */
      pos_warning(warning_suggested, operator_position);
    }  /* if */
  } else {
    /* The operands are not compatible. */
    pos_error(ec_incompatible_operands, operator_position);
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
  a_boolean okay = FALSE;

  if (is_ptr_to_member_type(operand_1->type)) {
    /* See if the second operand can be converted to the type of the
       first operand. */
    if (impl_ptr_to_member_conversion(operand_2->type,
                                      is_constant_operand(operand_2),
                                      &operand_2->variant.constant,
                                      operand_1->type,
                                  /*check_as_operands_not_conversion=*/TRUE)) {
      *operation_type = operand_1->type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay && is_ptr_to_member_type(operand_2->type)) {
    /* See if the first operand can be converted to the type of the
       second operand. */
    if (impl_ptr_to_member_conversion(operand_1->type,
                                      is_constant_operand(operand_1),
                                      &operand_1->variant.constant,
                                      operand_2->type,
                                  /*check_as_operands_not_conversion=*/TRUE)) {
      *operation_type = operand_2->type;
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay) {
    /* The operands are not compatible. */
    pos_error(ec_incompatible_operands, operator_position);
    *operation_type = error_type();
  }  /* if */
  return okay;
}  /* check_ptr_to_member_operands_for_compatibility */


void change_binary_operand_types(a_type_ptr         type,
				 an_operand         *operand_1,
				 an_operand         *operand_2,
                                 an_expression_kind expression_kind)
/*
If the current types of the operands do not match the new type, cast the
operands to the new type.  This is used for the operands of an operation,
with the type probably determined by determine_arithmetic_conversions.
expression_kind indicates the kind of expression being scanned.
*/
{
  if (!is_error_type(type)) {
    if (operand_1->type != type) {
      /* Cast operand 1 to match the desired type. */
      cast_operand(type, operand_1, expression_kind,
                   /*is_implicit_cast=*/TRUE);
    }  /* if */
    if (operand_2->type != type) {
      /* Cast operand 2 to match the desired type. */
      cast_operand(type, operand_2, expression_kind,
                   /*is_implicit_cast=*/TRUE);
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
                  num_elements = array_type->variant.array.number_of_elements;
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
			 a_source_position     *operator_position,
                         an_expression_kind    expression_kind)
/*
Perform a binary operation on 2 operands yielding a result.  operator
indicates the operation, and operand_1 and operand_2 are the operands.
result_type indicates the type of result; the result is placed in
*result.  expression_kind indicates the kind of expression this is,
in particular whether it's a constant expression of some kind.
If the operands are constant, the operation will be folded if possible.
*/
{
  a_boolean did_not_fold;
  a_boolean just_past_end;
  a_boolean try_folding;

  if (is_error_operand(operand_1) || is_error_operand(operand_2)) {
    make_error_operand(result);
  } else {
    /* Some addressing operations should not be folded in nonconstant
       contexts, because the expression form provides more explicit
       addressing information (which is useful for aliasing analysis). */
    /* Field-selection operations don't come through this routine, so there's
       no point in checking them. */
    if (op == (an_expr_operator_kind)eok_padd_subsc ||
        op == (an_expr_operator_kind)eok_padd) {
      /* Try folding if the operands are constant and the expression kind
         is a constant expression. */
      try_folding = is_const_expr_kind(expression_kind);
    } else {
      /* Not an addressing operation (normal case).  Try folding if the
         operands are constant and the expression is being evaluated. */
      try_folding = (expression_kind != (an_expression_kind)ek_not_evaluated);
    }  /* if */
    /* Try to fold the operation if both operands are constants. */
    did_not_fold = TRUE;
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
                         is_const_expr_kind(expression_kind),
                         &did_not_fold, operator_position);
      }  /* if */
    }  /* if */
    if (did_not_fold) {
      if (is_const_expr_kind(expression_kind)) {
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
  /* Instantiate the underlying type if it is a template class. */
  check_for_uninstantiated_template_class(result_type);
  /* Restore the original source position, etc. */
  restore_operand_details_incl_xref(result, &orig_result);
}  /* add_reference_indirection */


void make_lvalue_variable_operand(a_variable_ptr    variable,
                                  an_operand        *result,
                                  an_xref_entry_ptr xep)
/*
Make an operand for the address of a variable.  The source position of
the operand is set to pos_curr_token.  xep points to an associated
cross-reference entry, or is NULL if cross-reference information is
not being maintained.
*/
{
  an_expr_node_ptr node;
  a_type_ptr       variable_type = variable->type;

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
    result->type = variable_type;
    clear_constant(&result->variant.constant,
                   (a_constant_repr_kind)ck_address);
    result->variant.constant.type = make_pointer_type(variable_type);
    result->variant.constant.variant.address.kind =
                                            (an_address_base_kind)abk_variable;
    result->variant.constant.variant.address.variant.variable = variable;
  }  /* if */
  result->state = (an_operand_state)os_lvalue;
  copy_source_position(pos_curr_token, result->position);
  /* Instantiate the underlying type if it is a template class. */
  check_for_uninstantiated_template_class(variable_type);
  /* Start a list of cross-reference entries related to the operand. */
  result->xref_entries_list = xep;
  /* If the variable has a reference type, add an implicit indirection. */
  if (C_dialect == C_dialect_cplusplus && is_reference_type(variable_type)) {
    add_reference_indirection(result);
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

  
static void f_replace_const_variable_by_its_value(an_operand *operand)
/*
In C++ mode, replace an operand for a const variable by the value of the
variable.  Called by the macro replace_const_variable_by_its_value.
Called only in C++ mode, and only when the operand is an expression operand
for a const variable's value.
*/
{
  a_constant_ptr con_val;
  an_operand     orig_operand;

#if CHECKING
  if (!is_expression_operand(operand) ||
      !is_variable_node(operand->variant.expression)) {
    internal_error("f_replace_const_variable_by_its_value: not expr/var");
  }  /* if */
#endif /* CHECKING */
  con_val = var_constant_value(operand->variant.expression->variant.variable);
  if (con_val != NULL) {
    /* The variable has a known constant value.  Use it. */
    /* Preserve the source position in the operand. */
    orig_operand = *operand;
    make_constant_operand(con_val, operand);
    restore_operand_details(operand, &orig_operand);
    /* The cross-reference entries, it any, are not re-attached. */
  }  /* if */
}  /* f_replace_const_variable_by_its_value */


/*
Replace an operand for a const variable by the value of the variable.
Only used in C++ mode.
*/
#define replace_const_variable_by_its_value(operand)                  \
{ if (is_expression_operand(operand) &&                               \
      is_variable_node(operand->variant.expression) &&                \
      is_const_variable(operand->variant.expression->variant.variable)) { \
    f_replace_const_variable_by_its_value(operand);                   \
  }  /* if */                                                         \
}  /* replace_const_variable_by_its_value */


void make_rvalue_variable_operand(a_variable_ptr variable,
                                  an_operand     *result)
/*
Make an operand for the value of a variable.  The source position of
the operand is set to "pos_curr_token".
*/
{
  an_expr_node_ptr node;

  /* Make a variable value node for the variable. */
  node = var_rvalue_expr(variable);
  /* Make an operand for the node. */
  make_expression_operand(node, node->type, result);
  if (C_dialect == C_dialect_cplusplus) {
    /* Instantiate the underlying type if it is a template class. */
    check_for_uninstantiated_template_class(variable->type);
    /* In C++, replace a const variable by its value. */
    replace_const_variable_by_its_value(result);
    /* If the variable has a reference type, add an implicit indirection. */
    if (is_reference_type(variable->type)) {
      add_reference_indirection(result);
    }  /* if */
  }  /* if */
}  /* make_rvalue_variable_operand */


static void f_force_definition_of_compiler_generated_routine(
                                                   a_routine_ptr     routine,
                                                   a_source_position *position)
/*
routine points to a compiler-generated routine that is being referenced
and whose definition has not yet been generated.  Force the definition
now.
*/
{
  a_type_ptr              class_type =
                               routine->source_corresp.class_of_which_a_member;
  a_special_function_kind skind = routine->special_kind;

  /* Only force a definition for constructors, destructors, and
     operator= functions.  In particular, do not try to define operator new
     and delete functions. */
  if (skind == (a_special_function_kind)sfk_constructor ||
      skind == (a_special_function_kind)sfk_destructor  ||
      (skind == (a_special_function_kind)sfk_operator &&
       routine->opname_kind == (an_opname_kind)onk_assign)) {
    define_special_member_function(routine, class_type, position);
  }  /* if */
}  /* f_force_definition_of_compiler_generated_routine */


/*
routine points to a routine that is being referenced.  If it is
a compiler-generated routine whose definition has not yet been generated,
force the definition now.
*/
#define force_definition_of_compiler_generated_routine(rout, pos)     \
{ if ((rout)->compiler_generated && (rout)->assoc_scope == NULL) {    \
    f_force_definition_of_compiler_generated_routine(rout, pos);      \
  }  /* if */                                                         \
}  /* force_definition_of_compiler_generated_routine */


void mark_routine_referenced(a_routine_ptr     routine,
                             a_source_position *position)
/*
Mark the indicated routine as actually referenced.  "Actually" means
as opposed to referenced in a virtual function call that may call some
other virtual function.  This forces instantiation if the function
is a template function.
*/
{
  a_symbol_ptr                       assoc_sym;
  a_function_instantiation_entry_ptr instance_ptr;

  /* Set the referenced flag.  This is only necessary for virtual
     functions referenced by qualified name.  For non-virtual functions,
     the normal reference-processing routines have already set the IL
     referenced flag. */
  routine->source_corresp.referenced = TRUE;
  /* If the routine is a nonstatic member function, mark the class of which
     it is a member as referenced.  This ensures that a class will not
     end up marked as unreferenced when one of its nonstatic member functions
     (which references the class at least in its "this" parameter) is
     marked referenced. */
  if (routine_type_is_nonstatic_member_function(routine->type)) {
    routine->source_corresp.class_of_which_a_member->
                                              source_corresp.referenced = TRUE;
  }  /* if */
  /* If the routine is compiler-generated and its definition has not
     yet been put out, force the definition now. */
  force_definition_of_compiler_generated_routine(routine, position);
  /* If the function is an instance of a function template, mark it
     as requiring an instantiation. */
  assoc_sym = (a_symbol_ptr)routine->source_corresp.assoc_info;
  if (assoc_sym != NULL) {
    instance_ptr = assoc_sym->variant.routine.instance_ptr;
    if (instance_ptr != NULL) {
      update_instantiation_required_flag(instance_ptr, TRUE);
    }  /* if */
  }  /* if */
}  /* mark_routine_referenced */


void make_ptr_to_member_constant_operand(a_symbol_ptr      member_proj_sym,
                                         a_source_position *position,
                                         an_operand        *result)
/*
Make an operand for a constant representing a C++ pointer to member.
member_proj_sym gives the (projection) symbol for the member.
*position gives the source position to put into the operand.
*/
{
  a_constant    constant;
  a_symbol_ptr  member_sym;
  a_type_ptr    member_type, member_class;
  a_field_ptr   field;
  a_routine_ptr rout;

  clear_constant(&constant, (a_constant_repr_kind)ck_ptr_to_member);
  member_sym = fundamental_symbol_of(member_proj_sym);
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
    /* If the routine is compiler-generated and its definition has not
       yet been put out, force the definition now.  Do not force generation
       of virtual compiler-generated routines. */
    if (!rout->is_virtual) {
      force_definition_of_compiler_generated_routine(rout, position);
    }  /* if */
  }  /* if */
  /* Note that the class of the pointer is always the class in which
     the member was defined, not any derived class.  See ARM 5.3. */
  member_class = member_sym->class_of_which_a_member;
  constant.variant.ptr_to_member.class_of_which_a_member = member_class;
  constant.type = ptr_to_member_type(member_type, member_class);
  make_constant_operand(&constant, result);
  result->position = *position;
}  /* make_ptr_to_member_constant_operand */


void make_function_designator_operand(a_symbol_ptr      routine_sym,
                                      a_boolean         is_qualified_name,
                                      a_source_position *position,
                                      an_xref_entry_ptr xep,
				      an_operand        *result)
/*
Make an operand for a function designator.  routine_sym points to the
routine symbol entry (not overloaded, not a projection symbol).
is_qualified_name is TRUE if the function was named by a qualified name.
The source position of the operand is set to *position.  xep points to an
associated cross-reference entry, or is NULL if cross-reference
information is not being maintained.
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
  /* Set up an address-of-function constant. */
  clear_operand((an_operand_kind)ok_constant, result);
  /* The type of the operand is the function type. */
  result->type = routine->type;
  clear_constant(&result->variant.constant,
                 (a_constant_repr_kind)ck_address);
  /* The type of the constant is pointer to function. */
  result->variant.constant.type = make_pointer_type(routine->type);
  result->variant.constant.variant.address.kind =
                                             (an_address_base_kind)abk_routine;
  result->variant.constant.variant.address.variant.routine = routine;
  result->state = (an_operand_state)os_function_designator;
  /* Remember whether or not the routine is virtual.  Use of a qualified
     name suppresses the virtual-ness of the function (ARM 10.2). */
  result->virtual_function = routine->is_virtual && !is_qualified_name;
  result->position = *position;
  /* Start a list of cross-reference entries related to the operand. */
  result->xref_entries_list = xep;
  /* If this is a non-virtual call, mark the routine entry as actually
     referenced. */
  if (!result->virtual_function) {
    mark_routine_referenced(routine, position);
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


static void make_function_call(an_expr_node_ptr  function_node,
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
                             call_pos);
  /* Make an operand for the overall call (etc.). */
  make_expression_operand(call_node, call_node->type, result);
  result->position = *call_pos;
  /* A function call returning a reference is an lvalue. */
  function_type = skip_typerefs(function_type);
  return_type = skip_typerefs(function_type->variant.routine.return_type);
  if (is_reference_type(return_type)) {
    conv_object_pointer_to_lvalue(result);
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


a_boolean is_bit_field_operand(an_operand *operand)
/*
Return TRUE if the operand is a bit field.
*/
{
  a_boolean             is_bit_field = FALSE;
  an_expr_node_ptr      node;
  an_expr_operator_kind op;

  if (is_expression_operand(operand)) {
    node = operand->variant.expression;
    if (is_operation_node(node)) {
      op = node->variant.operation.kind;
      /* Note that the extra tests to classify the operand as an lvalue or
         rvalue are probably unnecessary, but just to be sure we do them
         anyway. */
      if (is_an_lvalue(operand) &&
          op == (an_expr_operator_kind)eok_bit_field) {
        is_bit_field = TRUE;
      } else if (is_an_rvalue(operand) &&
                 (op == (an_expr_operator_kind)eok_value_bit_field ||
                  op == (an_expr_operator_kind)eok_extract_bit_field)) {
        is_bit_field = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_bit_field;
}  /* is_bit_field_operand */


static void set_variable_address_taken(a_variable_ptr     variable,
                                       a_source_position  *err_pos,
                                       an_expression_kind expression_kind)
/*
Set the address_taken flag in the indicated variable.  Issue an error at
*err_pos if the address of the variable cannot be taken.  expression_kind
is the kind of the current expression.
*/
{
  /* Check for taking the address of a register variable. */
  /* In C++, taking the address of a register variable is allowed. */
  if (variable->storage_class == (a_storage_class)sc_register &&
      C_dialect != C_dialect_cplusplus) {
    pos_error(ec_address_of_register_variable, err_pos);
  } else {
    /* The address is not "really" taken if it's not evaluated. */
    if (expression_kind != (an_expression_kind)ek_not_evaluated) {
      /* Set the address_taken flag in the variable. */
      variable->address_taken = TRUE;
    }  /* if */
  }  /* if */
}  /* set_variable_address_taken */


static void set_address_taken_on_variable_in_constant(
                                            a_constant_ptr     con,
                                            a_source_position  *err_pos,
                                            an_expression_kind expression_kind)
/*
The constant con is being used as the address in an lvalue whose address
is being taken.  Set the address_taken flag in any variable underlying
the constant.  Issue an error at *err_pos if the address of the variable
cannot be taken.  expression_kind is the kind of the current expression.
*/
{      
  if (con->kind == (a_constant_repr_kind)ck_address &&
      con->variant.address.kind == (an_address_base_kind)abk_variable) {
    /* The constant is the address of a variable. */
    set_variable_address_taken(con->variant.address.variant.variable,
                               err_pos, expression_kind);
  }  /* if */
}  /* set_address_taken_on_variable_in_constant */


static void set_address_taken_on_variables_in_expr(
                                            an_expr_node_ptr   node,
                                            a_source_position  *err_pos,
                                            an_expression_kind expression_kind)
/*
node points to an expression whose address is being taken.  Set the
address_taken flag on the variable(s) in the lvalue.  Issue an error at
*err_pos if the address of the variable cannot be taken.  expression_kind
is the kind of the current expression.
*/
{
  an_expr_operator_kind op;
  an_expr_node_ptr      op1;

  if (is_constant_node(node)) {
    /* A constant (address) node. */
    set_address_taken_on_variable_in_constant(node->variant.constant,
                                              err_pos, expression_kind);
  } else if (is_variable_address_node(node)) {
    /* A variable address node. */
    set_variable_address_taken(node->variant.variable,
                               err_pos, expression_kind);
  } else if (is_operation_node(node)) {
    /* An operation node. */
    op = node->variant.operation.kind;
    op1 = node->variant.operation.operands;
    if (op == (an_expr_operator_kind)eok_field ||
        node->variant.operation.assignment_returns_lvalue) {
      /* Field selection, or assignment that returns an lvalue.  The first
         operand gives the lvalue. */
      set_address_taken_on_variables_in_expr(op1, err_pos, expression_kind);
    } else if (op == (an_expr_operator_kind)eok_comma) {
      /* Comma operator.  The second operand gives the lvalue. */
      set_address_taken_on_variables_in_expr(op1->next,
                                             err_pos, expression_kind);
    } else if (op == (an_expr_operator_kind)eok_question) {
      /* "?" operator.  The second and third operands give the lvalue.
         Note that an expression like
           &(i ? j : k)
         (valid only in C++) takes the address of both j and k. */
      set_address_taken_on_variables_in_expr(op1->next,
                                             err_pos, expression_kind);
      set_address_taken_on_variables_in_expr(op1->next->next,
                                             err_pos, expression_kind);
    }  /* if */
  }  /* if */
}  /* set_address_taken_on_variables_in_expr */


void take_address_of_lvalue(an_operand         *operand,
                            an_expression_kind expression_kind)
/*
Change operand (an lvalue) to an rvalue that is a pointer to the
object.  This is the function of the "&" operator.  Check that the
operand isn't a register variable or a bit field, and set the
address_taken flag.  This operation is inside of an expression of
kind expression_kind.
*/
{
  an_expr_node_ptr node;
  a_constant_ptr   con;
  an_operand       orig_operand;

  orig_operand = *operand;
#if CHECKING
  if (!is_an_lvalue(operand) && operand->state != (an_operand_state)os_none) {
    internal_error("take_address_of_lvalue: not an lvalue");
  }  /* if */
#endif /* CHECKING */
  /* Note: If you add any additional tests here for entities whose addresses
     cannot be taken, you probably should add the same tests in
     conv_operand_to_object_pointer. */
  /* Check for taking the address of a bit field. */
  if (is_bit_field_operand(operand)) {
    error_in_operand(ec_address_of_bit_field, operand);
  } else if (is_void_type(operand->type)) {
    /* Check for taking the address of something of type void.  Strictly
       speaking, a void expression is not an lvalue, so is_an_lvalue should
       not return TRUE for such a thing.  However, it's more straightforward
       to check that in places where an lvalue is used. */
    error_in_operand(ec_address_of_void, operand);
  } else {
    /* Find the base variable and set its address_taken flag, and change the
       type of the operand to pointer-to-operand. */
    switch (operand->kind) {
      case ok_error:
        break;
      case ok_expression:
        node = operand->variant.expression;
        operand->type = node->type;
        set_address_taken_on_variables_in_expr(node, &operand->position,
                                               expression_kind);
        break;
      case ok_constant:
        con = &operand->variant.constant;
        operand->type = con->type;
        set_address_taken_on_variable_in_constant(con, &operand->position,
                                                  expression_kind);
        break;
#if CHECKING
      default:
        internal_error("take_address_of_lvalue: bad operand kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* The operand is now an rvalue. */
  operand->state = (an_operand_state)os_rvalue;
  if (expression_kind != (an_expression_kind)ek_not_evaluated) {
    /* Change the kind in the cross-reference entries to address-taken. */
    change_xref_kinds(operand->xref_entries_list, srk_address_taken);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* take_address_of_lvalue */


void modifying_lvalue(an_operand         *operand,
                      an_expression_kind expression_kind)
/*
The entity indicated by operand (an lvalue) is being modified (e.g.,
assigned to, incremented, ...).  This is being done inside an expression
of kind expression_kind.
*/
{
#if CHECKING
  if (!is_an_lvalue(operand) && !is_error_operand(operand)) {
    internal_error("modifying_lvalue: not an lvalue");
  }  /* if */
#endif /* CHECKING */
  using_lvalue(operand);
  if (expression_kind != (an_expression_kind)ek_not_evaluated) {
    /* Change the kind in the cross-reference entries to modification. */
    change_xref_kinds(operand->xref_entries_list, srk_modification);
  }  /* if */
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


static void prep_possible_ellipsis_argument_operand(
                                            an_operand         *operand,
                                            a_param_type_ptr   param,
                                            an_error_code      err_code,
                                            an_expression_kind expression_kind)
/*
operand is the actual argument value for the parameter described by param.
Adjust it for use in the call.  Specifically, check its type and cast it
to the proper type.  Issue the error err_code for a type incompatibility.
expression_kind indicates the kind of the current expression.  param may
be NULL to indicate that the argument falls under an ellipsis or old-style
function.
*/
{
  if (param == NULL) {
    /* The actual argument was accepted under an ellipsis.  Do default
       argument promotions. */
    arg_default_promote_operand(operand, expression_kind);
  } else {
    /* Cast the argument to the right type. */
    prep_argument_operand(operand, param, err_code, expression_kind);
  }  /* if */
}  /* prep_possible_ellipsis_argument_operand */


static void set_up_for_constructor_call(an_operand         *operand,
                                        a_routine_ptr      ctor_routine,
                                        an_expression_kind expression_kind,
                                        an_expr_node_ptr   *arg_expr_list)
/*
Prepare for generating a call of a constructor, but do not actually
create the call.  Check accessibility of the routine and adjust the operand
type if necessary so that it will be appropriate for the call.  Return
an argument list for the call in *arg_expr_list.  This routine is used
only in C++ mode.
*/
{
  a_symbol_ptr     ctor_symbol;
  a_type_ptr       routine_type;
  a_param_type_ptr param_list;
  a_routine_type_supplement_ptr
                   rtsp;

  /* Check that the constructor is accessible and mark it as referenced. */
  ctor_symbol = (a_symbol_ptr)(ctor_routine->source_corresp.assoc_info);
  reference_to_implicitly_invoked_function(ctor_symbol, &error_position);
  routine_type = skip_typerefs(ctor_routine->type);
  /* Convert the operand to the proper type to be an argument of the
     constructor. */
  rtsp = routine_type->variant.routine.extra_info;
  param_list = rtsp->param_type_list;
#if CHECKING
  if (param_list == NULL && !rtsp->has_ellipsis) {
    internal_error("set_up_for_constructor_call: no first parameter");
  }  /* if */
#endif  /* CHECKING */
  /* Convert the argument to the right type.  We don't expect an error
     here, since presumably we've chosen the proper function to call
     through overload resolution. */
  prep_possible_ellipsis_argument_operand(operand, param_list,
                                          ec_incompatible_param,
                                          expression_kind);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
  /* If the constructor has default arguments after the first, add
     arguments for them. */
  if (param_list != NULL) {
    (*arg_expr_list)->next = copy_default_arg_expr_list(param_list->next);
  }  /* if */
}  /* set_up_for_constructor_call */


static void prep_special_selector_operand(an_operand         *operand,
                                          a_type_ptr         routine_type,
                                          an_expression_kind expression_kind)
/*
For unconventional "this" arguments, convert the selector to an object
pointer and cast it to a base class if necessary.  This is needed for
operator functions and conversion functions, but not for function calls
using the usual notation (in those cases, the base class cast is done
as part of the "->" or ".").  operand gives the selector, and routine_type
gives the type of the routine being called.
*/
{
  conv_operand_to_object_pointer(operand, expression_kind);
  routine_type = skip_typerefs(routine_type);
  /* The cast here handles base class casts and also const/volatile
     differences. */
  cast_operand(routine_type->variant.routine.
                                          extra_info->implicit_this_param_type,
               operand, expression_kind, /*is_implicit_cast=*/TRUE);
}  /* prep_special_selector_operand */


static void set_up_for_conversion_function_call(
                                         an_operand         *operand,
                                         a_routine_ptr      conversion_routine,
                                         an_expression_kind expression_kind,
                                         an_expr_node_ptr   *arg_expr_list)
/*
Prepare for generating a call of a conversion function, but do not
actually create the call.  Check accessibility of the routine and adjust
the operand type if necessary so that it will be appropriate for the call.
Return an argument list for the call in *arg_expr_list.  This routine
is used only in C++ mode.
*/
{
  a_symbol_ptr conversion_symbol;
  a_type_ptr   this_param_type, routine_type;

  /* Check that the conversion function is accessible and mark it as
     referenced. */
  conversion_symbol =
                 (a_symbol_ptr)(conversion_routine->source_corresp.assoc_info);
  reference_to_implicitly_invoked_function(conversion_symbol, &error_position);
  routine_type = skip_typerefs(conversion_routine->type);
  /* Convert the operand to the proper type to be an argument of the
     conversion function. */
  this_param_type = routine_type->variant.routine.extra_info->
                                                      implicit_this_param_type;
#if CHECKING
  if (this_param_type == NULL) {
    internal_error("set_up_for_conversion_function_call: no this parameter");
  }  /* if */
#endif  /* CHECKING */
  /* Check for the cfront anachronism that allows a non-const function to be
     called for a const selector (see selector_match_with_this_param). */
  if (cfront_compatibility_mode &&
      is_const_qualified_type(operand->type) &&
      !is_const_qualified_type(type_pointed_to(this_param_type))) {
    pos_warning(ec_unqual_function_with_qual_object, &operand->position);
    /* prep_special_selector_operand (call below) will drop the const. */
  }  /* if */
  /* Make a pointer for the selector, and cast it to a base class
     if necessary. */
  prep_special_selector_operand(operand, routine_type, expression_kind);
  /* Make an expression for the argument. */
  *arg_expr_list = make_node_from_operand(operand);
}  /* set_up_for_conversion_function_call */


void make_constructor_dynamic_init(a_routine_ptr    ctor_routine,
                                   an_expr_node_ptr arg_expr_list,
                                   a_boolean        result_is_addr,
                                   an_operand       *result)
/*
Create a temporary and an enk_temp_init node that calls the constructor
routine ctor_routine with the argument list arg_expr_list.  Return an
operand for the value (result_is_addr == FALSE) or address (result_is_addr
== TRUE) of the temporary in *result.
*/
{
  a_type_ptr         class_type;
  a_variable_ptr     temp_var;
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node, return_value_node;

#if CHECKING
  if (ctor_routine->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error("make_constructor_dynamic_init: routine not constructor");
  }  /* if */
#endif /* CHECKING */
  class_type = skip_typerefs(ctor_routine->type)->variant.routine.extra_info->
                                                      implicit_this_param_type;
  class_type = type_pointed_to(class_type);
  class_type = skip_typerefs(class_type);
  /* Allocate the temporary, the dynamic initialization entry, and the
     enk_temp_init node.  find_class_rvalue_var_node recognizes
     the form of the expression created here. */
  temp_var = create_expr_temporary(class_type, /*force_temp_init=*/TRUE,
                                   &temp_init_node);
  dip = temp_init_node->variant.init.dynamic_init;
  /* Use a dik_constructor to call the constructor routine. */
  set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_constructor);
  dip->variant.constructor.routine = ctor_routine;
  dip->variant.constructor.args = arg_expr_list;
  if (result_is_addr) {
    /* Make a node for the address of the temporary. */
    return_value_node = var_lvalue_expr(temp_var);
  } else {
    /* Make a node for the value of the temporary. */
    return_value_node = var_rvalue_expr(temp_var);
  }  /* if */
  /* Attach the value node under the enk_temp_init. */
  attach_expr_under_temp_init(&return_value_node, temp_init_node);
  /* Make an operand for the overall expression. */
  make_expression_operand(return_value_node, return_value_node->type, result);
}  /* make_constructor_dynamic_init */


static void temp_init_from_operand(an_operand *operand)
/*
Create a temporary variable of type temp_type and make an enk_temp_init
node that initializes the temporary to the indicated operand.
The source operand can be an rvalue or an lvalue.  On return, *operand
will have been changed to an rvalue for the address of the temporary.
*/
{
  a_variable_ptr     temp_var;
  a_dynamic_init_ptr dip;
  an_expr_node_ptr   temp_init_node, temp_lvalue_node;
  a_boolean          cctor_case, class_bitwise_copy;
  a_type_ptr         temp_type;
  a_routine_ptr      cctor_routine;
  an_expr_node_ptr   cctor_arg;
  an_operand         orig_operand;

  orig_operand = *operand;
  /* The temporary has the type of the operand minus type qualifiers. */
  temp_type = skip_typerefs(operand->type);
  cctor_case = FALSE;
  if (is_class_struct_union_type(temp_type)) {
    /* The operand and temporary have a class type.  If it's a C-style
       struct, a direct copy can be done.  Otherwise, look for a copy
       constructor to use. */
    if (C_dialect != C_dialect_cplusplus) {
      /* C struct. */
      /* cctor_case = FALSE;  -- already set */
    } else {
      /* A copy constructor must be used.  An error is issued if an appropriate
         one does not exist or is inaccessible. */
      cctor_routine = select_copy_constructor(
                                     temp_type,
                                     is_const_qualified_type(operand->type),
                                     is_volatile_qualified_type(operand->type),
                                     &operand->position, &class_bitwise_copy);
      if (class_bitwise_copy) {
        /* A bitwise copy can be done. */
        /* cctor_case = FALSE;  -- already set */
      } else if (cctor_routine == NULL) {
        /* No appropriate copy constructor.  The error has already been
           issued.  */
        cctor_case = TRUE;
        conv_to_error_operand(operand);
      } else {
        /* Make the dynamic init call the copy constructor. */
        cctor_case = TRUE;
        set_up_for_constructor_call(operand, cctor_routine,
                                    (an_expression_kind)ek_normal,
                                    &cctor_arg);
        make_constructor_dynamic_init(cctor_routine, cctor_arg,
                                      /*result_is_addr=*/TRUE, operand);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!cctor_case) {
    /* Normal case -- use a dik_expression initialization to copy the
       operand into the temporary. */
    /* Allocate the temporary, the dynamic initialization entry, and the
       enk_temp_init node. */
    temp_var = create_expr_temporary(temp_type, /*force_temp_init=*/TRUE,
                                     &temp_init_node);
    dip = temp_init_node->variant.init.dynamic_init;
    conv_lvalue_to_rvalue(operand, (an_expression_kind)ek_normal);
    set_dynamic_init_kind(dip, (a_dynamic_init_kind)dik_expression);
    dip->variant.expression = make_node_from_operand(operand);
    /* Make a node for the address of the temporary. */
    temp_lvalue_node = var_lvalue_expr(temp_var);
    /* Attach the value node under the enk_temp_init. */
    attach_expr_under_temp_init(&temp_lvalue_node, temp_init_node);
    /* Make an operand for the overall expression. */
    make_expression_operand(temp_lvalue_node, temp_lvalue_node->type, operand);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* temp_init_from_operand */


static an_expr_node_ptr find_class_rvalue_var_node(an_expr_node_ptr node,
                                                   a_type_ptr       new_type)
/*
Walk down an expression tree representing a class rvalue, looking for
an enk_variable node that gives the value of the tree.  We want to find
such a node so that we can change it to an enk_variable_address node and
make the overall tree into an lvalue for the same object.  Return
a pointer to the enk_variable node (but do not change it).  If the
enk_variable is not found, return NULL.  While descending to the
enk_variable node, set the node types to new_type.  This is used on a
second call of this routine to change the node types to add a pointer-to
to reflect the change made to the enk_variable node.  On the first call,
new_type will be the original type and the assignments will not change
anything.

This routine *must* recognize the expression forms generated for
rvalue classes, specifically those generated by func_call_expr and
make_constructor_dynamic_init.  The general form is some number of
enk_temp_init nodes, some number of comma nodes, and an enk_variable
node at the bottom.
*/
{
  /* Drop any enk_temp_init nodes. */
  while (node->kind == (an_expr_node_kind)enk_temp_init) {
    node->type = new_type;
    node = node->variant.init.expr;
  }  /* while */
  /* Drop any comma operators, moving down to the second operand of
     the comma each time. */
  while (is_operation_node(node) &&
         node->variant.operation.kind == (an_expr_operator_kind)eok_comma) {
    node->type = new_type;
    node = node->variant.operation.operands->next;
  }  /* while */
  /* Check to see that we've found the enk_variable node. */
  if (node->kind != (an_expr_node_kind)enk_variable) {
    node = NULL;
  }  /* if */
  return node;
}  /* find_class_rvalue_var_node */


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
  an_expr_node_ptr node = *p_node, var_node, op1, op2, op3;
  a_type_ptr       new_type;
  a_boolean        is_operation, op1_possible, op2_possible, op3_possible;

  *converted = FALSE;
  is_operation = is_operation_node(node);
  if (is_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_indirect) {
    /* The top operator is an indirection, so we can just remove it. */
    *converted = TRUE;
    if (!see_if_possible) {
      node = node->variant.operation.operands;
    }  /* if */
  } else if (is_operation && node->variant.operation.kind ==
                                         (an_expr_operator_kind)eok_question) {
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
      *converted = TRUE;
      if (!see_if_possible) {
        conv_class_rvalue_expr_to_object_pointer(&op2, &op2_possible,
                                                 /*see_if_possible=*/FALSE);
        conv_class_rvalue_expr_to_object_pointer(&op3, &op3_possible,
                                                 /*see_if_possible=*/FALSE);
        op1->next = op2;
        op2->next = op3;
        node->type = op2->type;
      }  /* if */
    }  /* if */
  } else if (is_operation &&
      node->variant.operation.kind == (an_expr_operator_kind)eok_value_field) {
    /* Selection of a field from an rvalue.  Try to find an lvalue in
       the struct rvalue, and if one can be found rewrite the operation
       as a normal field selection. */
    op1 = node->variant.operation.operands;
    /* See if both branches can be rewritten. */
    conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                             /*see_if_possible=*/TRUE);
    if (op1_possible) {
      *converted = TRUE;
      if (!see_if_possible) {
        conv_class_rvalue_expr_to_object_pointer(&op1, &op1_possible,
                                                 /*see_if_possible=*/FALSE);
        node->variant.operation.kind = (an_expr_operator_kind)eok_field;
        node->type = make_pointer_type(node->type);
      }  /* if */
    }  /* if */
  } else if (is_operation &&
             node->variant.operation.kind ==
                                          (an_expr_operator_kind)eok_sassign &&
             !node->variant.operation.assignment_returns_lvalue) {
    /* An assignment operation that returns an rvalue.  It can be optimized
       by changing it to the lvalue case. */
    /* This case is here for the sake of completeness.  It's probably not
       needed. */
    *converted = TRUE;
    if (!see_if_possible) {
      node->variant.operation.assignment_returns_lvalue = TRUE;
      node->type = make_pointer_type(node->type);
    }  /* if */
  } else {
    /* Try to find a class temporary in the tree.  If we can find one,
       we can use its address. */
    var_node = find_class_rvalue_var_node(node, node->type);
    if (var_node != NULL) {
      /* This is a case we can optimize. */
      *converted = TRUE;
      if (!see_if_possible) {
        /* Change the value of the variable to the address of the variable. */
        new_type = make_pointer_type(var_node->type);
        var_node->kind = (an_expr_node_kind)enk_variable_address;
        var_node->type = new_type;
        /* Visit the intermediate nodes (if any) again and change their
           types to the new pointer-to type. */
        (void)find_class_rvalue_var_node(node, new_type);
      }  /* if */
    }  /* if */
  }  /* if */
  *p_node = node;
}  /* conv_class_rvalue_expr_to_object_pointer */


void conv_operand_to_object_pointer(an_operand         *operand,
                                    an_expression_kind expression_kind)
/*
Convert an operand for an object into an operand for a pointer to the
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
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                             expression_kind);
  if (is_an_lvalue(operand)) {
    /* Test for lvalues whose address cannot be taken.  See
       take_address_of_lvalue.  Note that "register" is not tested since it
       is legal to take the address of a register entity in C++ mode. */
    if (is_bit_field_operand(operand)) {
      /* One cannot take the address of a bit-field lvalue, so convert
         the operand to an rvalue and use the rvalue code. */
      conv_lvalue_to_rvalue(operand, expression_kind);
    }  /* if */
  }  /* if */
  if (is_error_operand(operand)) {
    /* Error operand -- leave alone. */
  } else if (is_an_lvalue(operand)) {
    /* The operand is an lvalue.  This is the easy case, since the lvalue
       is already an address. */
    take_address_of_lvalue(operand, expression_kind);
  } else if (is_an_rvalue(operand)) {
    /* The operand is an rvalue.  In general, we will have to copy the
       rvalue to a temporary and use the address of the temporary.  However,
       there are some cases that can be optimized. */
    optimized_case = FALSE;
    if (is_expression_operand(operand)) {
      node = operand->variant.expression;
      if (is_class_struct_union_type(operand->type)) {
        /* Class rvalue.  Change it to an object pointer if possible. */
        conv_class_rvalue_expr_to_object_pointer(&node, &optimized_case,
                                                 /*see_if_possible=*/FALSE);
        if (optimized_case) {
          /* The expression has been rewritten as an object pointer. */
          make_expression_operand(node, node->type, operand);
        } else {
          /* Couldn't find the variable value.  The rvalue will have to be
             copied to a temporary, and the temporary address used. */
#if CHECKING
          /* Avoid recursion loops if the class does not allow bitwise copy.
             The search for the variable really must succeed (i.e., it's not
             merely an optimization) if a "real" copy constructor would have
             to be used, since in that case we would need the address of
             this rvalue to be able to call the copy constructor. */
          { a_class_symbol_supplement_ptr cssp =
                                    symbol_supplement_for_class(operand->type);
            if (!cssp->construction_by_bitwise_copy_allowed) {
#if DEBUG
              db_expression(node);
#endif /* DEBUG */
              internal_error(
                          "conv_operand_to_object_pointer: couldn't find var");
            }  /* if */
          }
#endif /* CHECKING */
        }  /* if */
      } else if (is_operation_node(node) && node->variant.operation.kind ==
                                         (an_expr_operator_kind)eok_indirect) {
        /* The top operator is an indirection, so we can just remove the
           indirection. */
        optimized_case = TRUE;
        node = node->variant.operation.operands;
        make_expression_operand(node, node->type, operand);
      }  /* if */
    }  /* if */
    if (!optimized_case) {
      /* Create a temporary, copy the rvalue into the temporary, and return
         the address of the temporary. */
      temp_init_from_operand(operand);
    }  /* if */
#if CHECKING
  } else {
    internal_error("conv_operand_to_object_pointer: unexpected state");
#endif /* CHECKING */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* conv_operand_to_object_pointer */


static an_expr_node_ptr conv_lvalue_expr_to_rvalue(an_expr_node_ptr node)
/*
mode is an expression that is the address for an lvalue.  Create an
expression for the corresponding rvalue, and return a pointer to it.
*/
{
  a_boolean             optimized_case = FALSE;
  an_expr_operator_kind op;
  an_expr_node_ptr      op1, op2, op3;
  a_type_ptr            new_type;

  if (is_operation_node(node)) {
    /* An operation node. */
    new_type = type_pointed_to(node->type);
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
    } else if (C_dialect == C_dialect_cplusplus &&
               op == (an_expr_operator_kind)eok_question) {
      /* "?" operator.  Convert each branch to an rvalue.  This is
         particularly useful for a case like
           &(i ? j : k)
         (only valid in C++). */
      optimized_case = TRUE;
      op2 = op1->next;
      op3 = op2->next;
      op1->next = op2 = conv_lvalue_expr_to_rvalue(op2);
      op2->next = conv_lvalue_expr_to_rvalue(op3);
    } else if (C_dialect == C_dialect_cplusplus &&
               op == (an_expr_operator_kind)eok_comma) {
      /* Comma operator.  Apply the transformation to the second operand
         of the ",".  This is useful for a case like
           (p = f(x), *p)
      */
      optimized_case = TRUE;
      op2 = op1->next;
      op1->next = conv_lvalue_expr_to_rvalue(op2);
    } else if (node->variant.operation.assignment_returns_lvalue) {
      /* The operation is an assignment that returns an lvalue.
         Change it to one that returns an rvalue. */
      optimized_case = TRUE;
      node->variant.operation.assignment_returns_lvalue = FALSE;
    }  /* if */
  }  /* if */
  if (!optimized_case) {
    /* Not an optimized case.  Just add an indirection. */
    node = add_indirection_to_node(node);
  } else {
    /* For the optimized cases, set the node type to the type pointed to. */
    node->type = new_type;
  }  /* if */
  return node;
}  /* conv_lvalue_expr_to_rvalue */


void conv_lvalue_to_rvalue(an_operand         *operand,
                           an_expression_kind expression_kind)
/*
Convert an lvalue operand to an rvalue operand.  See section 3.2.2.1 of the
standard.  In the general case, the lvalue is the address of something,
and this conversion adds an indirection so that the operand refers to
the rvalue pointed to.  Some cases are optimized.  If the operand is
not an lvalue, it is left alone.  expression_kind indicates the kind
of expression being scanned.  This is needed to detect the error of an
lvalue being converted to an rvalue in a constant expression.
*/
{
  an_expr_node_ptr node;
  a_constant_ptr   constant;
  an_operand       orig_operand;
  a_boolean        optimized_case;
  a_variable_ptr   variable;
  an_expr_node_ptr operand_node, cast_node;
  a_type_ptr       cast_orig_type, unqualified_type;

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
    if (is_const_expr_kind(expression_kind)) {
      /* An lvalue cannot be converted to an rvalue in a constant
         expression. */
      error_in_operand(ec_expr_not_constant, operand);
    } else if (is_error_operand(operand)) {
      /* Error operand -- leave it alone (but make sure it's not an lvalue
         any more). */
      conv_to_error_operand(operand);
    } else if (is_incomplete_type(operand->type)) {
      /* Converting an lvalue with incomplete type to an rvalue is undefined
         behavior (standard, 3.2.2.1); we treat it as an error. */
      error_in_operand(ec_incomplete_type_not_allowed, operand);
    } else {
      using_lvalue(operand);
      if (expression_kind != (an_expression_kind)ek_not_evaluated) {
        /* Change the kind in the cross-reference entries to reference. */
        /* This changes address-taken entries for arrays to references. */
        change_xref_kinds(operand->xref_entries_list, srk_reference);
      }  /* if */
      if (is_constant_operand(operand)) {
        /* The lvalue address is specified by a constant.  Some optimizations
           are possible for address constants. */
        optimized_case = FALSE;
        constant = &operand->variant.constant;
        if (constant->kind == (a_constant_repr_kind)ck_address) {
          if (constant->variant.address.kind ==
                                          (an_address_base_kind)abk_variable) {
            /* The lvalue address is the address of a variable.  Therefore,
               the rvalue is the variable itself.  The optimization doesn't
               apply if there is an offset relative to the variable or
               if the pointer type has been cast to something else. */
            if (constant->variant.address.offset == 0 &&
                !constant->implicit_cast) {
              optimized_case = TRUE;
              variable = constant->variant.address.variant.variable;
              make_rvalue_variable_operand(variable, operand);
            }  /* if */
          }  /* if */
        }  /* if */
        if (!optimized_case) {
          /* Not a special case -- add an indirection operator. */
          build_unary_result_operand(operand,
                                     (an_expr_operator_kind)eok_indirect,
                                     operand->type, operand);
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
          conv_lvalue_to_rvalue(operand, expression_kind);
          if (!is_expression_operand(operand) ||
               is_bit_field_extract_node(operand->variant.expression)) {
            /* The operand is not based on an expression node (unexpected,
               but checked just to be safe), or the operand is a bit-field
               extraction (where the cast can be folded into the extraction).
               Throw away the cast node and do a cast. */
            cast_operand(cast_orig_type, operand, expression_kind,
                         /*is_implicit_cast=*/FALSE);
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
          /* Not an lvalue cast (normal case). */
          /* Convert the expression to an rvalue. */
          operand->variant.expression = conv_lvalue_expr_to_rvalue(node);
          operand->state = (an_operand_state)os_rvalue;
          if (C_dialect == C_dialect_cplusplus) {
            /* In C++, replace a const variable by its value. */
            replace_const_variable_by_its_value(operand);
          }  /* if */
        }  /* if */
      }  /* if */
      /* Drop any type qualifiers on the operand type. */
      if (is_qualified_type(operand->type)) {
        unqualified_type = make_unqualified_type(operand->type);
        if (is_expression_operand(operand)) {
          /* For an expression node, just change the expression type.
             That's an IL shorthand form for this case, and avoids a
             cast to a struct or union type. */
          operand->type = operand->variant.expression->type = unqualified_type;
        } else {
          /* For other cases (including constants), do the cast the normal
             way. */
          cast_operand(unqualified_type, operand, expression_kind,
                       /*is_implicit_cast=*/TRUE);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Restore the operand's source position. */
    restore_operand_details(operand, &orig_operand);
    /* The xref_entries_list is cleared because it should only contain
       information on lvalue addresses. */
    operand->xref_entries_list = NULL;
  }  /* if */
}  /* conv_lvalue_to_rvalue */


void conv_array_operand_to_pointer_operand(an_operand         *operand,
                                           an_expression_kind expression_kind)
/*
Apply the implicit array to pointer-to-first-element-of-array transformation
of 3.2.2.1 in the standard to the operand.  In addition to the usual case of
an array lvalue becoming a pointer to the first element of the array,
this routine also converts a string literal to a pointer to its first
character.  If the operand is an rvalue array, an error is issued.
All other cases are left alone.  expression_kind indicates the kind
of expression being scanned.
*/
{
  a_type_ptr ptr_type;
  an_operand orig_operand;

  if (is_array_type(operand->type)) {
    orig_operand = *operand;
    if (is_an_rvalue(operand)) {
      /* An array rvalue -- error. */
      error_in_operand(ec_bad_rvalue_array, operand);
    } else if (is_an_lvalue(operand)) {
      /* An array lvalue -- convert to a pointer. */
      /* Convert to an rvalue that is the pointer, and change its type
         from pointer-to-array to pointer-to-array-element. */
      ptr_type = make_pointer_type(array_element_type(operand->type));
      take_address_of_lvalue(operand, expression_kind);
      cast_operand(ptr_type, operand, expression_kind,
                   /*is_implicit_cast=*/TRUE);
    }  /* if */
    /* Restore the original source position, etc. */
    restore_operand_details(operand, &orig_operand);
  }  /* if */
}  /* conv_array_operand_to_pointer_operand */


void conv_function_designator_to_ptr_to_function(
                                            an_operand         *operand,
                                            an_expression_kind expression_kind)
/*
Convert a function designator operand to a pointer to function expression 
operand.  Change the state from "os_function_designator" to "os_rvalue" and
convert the type from "function" to "pointer to function".  expression_kind
indicates the current expression kind.
*/
{
  an_operand orig_operand;

  orig_operand = *operand;
  if (is_error_operand(operand)) {
    /* Error operand; leave it alone. */
  } else if (is_constant_operand(operand)) {
    /* Since the operand becomes "pointer-to" and the constant already has that
       type, just copy the type from the constant. */
    operand->type = operand->variant.constant.type;
    if (C_dialect == C_dialect_cplusplus) {
      a_constant_ptr rout_con = &operand->variant.constant;
      a_routine_ptr  rout;
      if (rout_con->kind == (a_constant_repr_kind)ck_address &&
          rout_con->variant.address.kind ==
                                           (an_address_base_kind)abk_routine) {
        rout = rout_con->variant.address.variant.routine;
        if (rout == il_header.main_routine) {
          /* In C++, "main" cannot be called and cannot have its address
             taken (ARM 3.4). */
          error_in_operand(ec_bad_use_of_main, operand);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_expression_operand(operand)) {
    /* Expression operand.  Since the operand becomes "pointer-to" and the
       expression already has that type, just copy the type from the
       expression. */
    operand->type = operand->variant.expression->type;
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
  }  /* if */
  operand->state = (an_operand_state)os_rvalue;
  if (expression_kind != (an_expression_kind)ek_not_evaluated) {
    /* Change the kind in the cross-reference entries to address-taken. */
    change_xref_kinds(operand->xref_entries_list, srk_address_taken);
  }  /* if */
  /* Restore the original source position etc. */
  restore_operand_details(operand, &orig_operand);
}  /* conv_function_designator_to_ptr_to_function */


void do_operand_transformations(an_operand                   *operand,
                                a_transformation_options_set options,
                                an_expression_kind           expression_kind)
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
    /* An array lvalue (or rvalue, which is used for string literals). */
    if (!(options & TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION)) {
      /* In most contexts, an lvalue of array type is changed to
         "pointer to first element of array".  See section 3.2.2.1 in the
         ANSI C standard. */
      conv_array_operand_to_pointer_operand(operand, expression_kind);
    }  /* if */
  } else if (is_an_lvalue(operand)) {
    /* A non-array lvalue. */
    if (!(options & TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION)) {
      /* Convert an lvalue to an rvalue. */
      conv_lvalue_to_rvalue(operand, expression_kind);
    }  /* if */
  } else if (is_a_function_designator(operand)) {
    if (!(options & TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION)) {
      /* In most contexts, an entity of type "function returning type"
         is changed to "pointer to function returning type".  
         See section 3.2.2.1 in the ANSI C standard. */
      conv_function_designator_to_ptr_to_function(operand, expression_kind);
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


static void clear_arg_match_summary(an_arg_match_summary_ptr amsp)
/*
Clear the fields of the indicated argument match summary entry to default
values.
*/
{
  amsp->next                     = NULL;
  amsp->match_level              = aml_none;
  amsp->less_desirable_exact_match
                                 = FALSE;
  amsp->downward_cast_derivation = NULL;
  amsp->reversed_derivation      = FALSE;
  amsp->const_anachronism        = FALSE;
  amsp->base_param_type          = NULL;
  amsp->conversion_routine       = NULL;
  amsp->std_conversion_after_user_conversion
                                 = FALSE;
  amsp->warning_suggested        = ec_no_error;
}  /* clear_arg_match_summary */


static an_arg_match_summary_ptr alloc_arg_match_summary(void)
/*
Allocate an argument match summary entry and return a pointer to it.
Such entries are used in resolving calls to overloaded functions.
*/
{
  an_arg_match_summary_ptr amsp;

  if (avail_arg_match_summaries != NULL) {
    /* Reuse a previously-freed entry. */
    amsp = avail_arg_match_summaries;
    avail_arg_match_summaries = amsp->next;
  } else {
    /* Allocate a new entry. */
    amsp = (an_arg_match_summary_ptr)alloc_fe(sizeof(an_arg_match_summary));
  }  /* if */
  clear_arg_match_summary(amsp);
  return amsp;
}  /* alloc_arg_match_summary */


void free_arg_match_summary_list(an_arg_match_summary_ptr amsp)
/*
Free the list of argument match summary entries pointed to by amsp.
*/
{
  an_arg_match_summary_ptr amsp_next;

  for (; amsp != NULL; amsp = amsp_next) {
    amsp_next = amsp->next;
    /* Add the entry to the available list. */
    amsp->next = avail_arg_match_summaries;
    avail_arg_match_summaries = amsp;
  }  /* for */
}  /* free_arg_match_summary_list */

#if DEBUG

static void db_arg_match_summary(an_arg_match_summary_ptr amsp)
/*
Print an argument match summary for debug purposes.
*/
{
  char                  *str;
  a_derivation_step_ptr dsp;
  unsigned long         step_count;

  switch (amsp->match_level) {
    case aml_exact:             str = "exact";               break;
    case aml_promotion:         str = "promotion";           break;
    case aml_std_conversion:    str = "std conversion";      break;
    case aml_user_conversion:   str = "user conversion";     break;
    case aml_ellipsis:          str = "ellipsis";            break;
    case aml_error:             str = "error";               break;
    case aml_none:              str = "none";                break;
    default:                    str = "**BAD MATCH LEVEL**";
  }  /* if */
  fprintf(f_debug, "match level = %s", str);
  if (amsp->less_desirable_exact_match) {
    fprintf(f_debug, " (less desirable)");
  }  /* if */
  if (amsp->const_anachronism) {
    fprintf(f_debug, " (const anachronism)");
  }  /* if */
  if (amsp->std_conversion_after_user_conversion) {
    fprintf(f_debug, " (std conversion)");
  }  /* if */
  dsp = amsp->downward_cast_derivation;
  if (dsp != NULL) {
    /* Count derivation steps and print the count. */
    for (step_count = 0; dsp != NULL; dsp = dsp->next, step_count++) {}
    fprintf(f_debug, " (%lu step%s)", step_count,
                     (step_count != 1) ? "s" : "");
  }  /* if */
  fprintf(f_debug, "\n");
}  /* db_arg_match_summary */

#endif /* DEBUG */

static a_candidate_function_ptr alloc_candidate_function(void)
/*
Allocate a candidate function entry and return a pointer to it.  Such entries
are used in resolving calls to overloaded functions.
*/
{
  a_candidate_function_ptr cfp;

  if (avail_candidate_functions != NULL) {
    /* Reuse a previously-freed entry. */
    cfp = avail_candidate_functions;
    avail_candidate_functions = cfp->next;
  } else {
    /* Allocate a new entry. */
    cfp = (a_candidate_function_ptr)alloc_fe(sizeof(a_candidate_function));
  }  /* if */
  cfp->next = NULL;
  cfp->function_symbol = NULL;
  cfp->is_function_template = FALSE;
  cfp->operand_type_pattern = NULL;
  cfp->pointer_type = NULL;
  cfp->arg_matches = NULL;
  cfp->arg_operand_list = NULL;
  cfp->current_arg_match = NULL;
  cfp->prev_func_arg_match_with_same_match_level = NULL;
  cfp->in_best_match_set = FALSE;
  cfp->in_best_match_set_for_some_argument = FALSE;
  cfp->std_conversion_after_conversion_function = FALSE;
  return cfp;
}  /* alloc_candidate_function */


static void free_candidate_function_list(a_candidate_function_ptr cfp)
/*
Free the list of candidate function entries pointed to by cfp.
*/
{
  a_candidate_function_ptr cfp_next;

  for (; cfp != NULL; cfp = cfp_next) {
    cfp_next = cfp->next;
    /* Free the argument match entries if any. */
    if (cfp->arg_matches != NULL) {
      free_arg_match_summary_list(cfp->arg_matches);
    }  /* if */
    /* arg_operand_list is deliberately not freed. */
    /* Add the entry to the available list. */
    cfp->next = avail_candidate_functions;
    avail_candidate_functions = cfp;
  }  /* for */
}  /* free_candidate_function */

#if DEBUG

static void db_candidate_function(a_candidate_function_ptr cfp)
/*
Print a candidate function entry for debugging purposes.
*/
{
  unsigned long            narg;
  an_arg_match_summary_ptr amsp;

  if (cfp->function_symbol != NULL) {
    /* Normal function case. */
    db_symbol(cfp->function_symbol, "", 2);
  } else {
    /* Built-in operator case. */
    fprintf(f_debug, "Built-in %s", cfp->operand_type_pattern);
    if (cfp->pointer_type != NULL) {
      fprintf(f_debug, "pointer_type = ");
      db_type(cfp->pointer_type);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
  if (cfp->is_function_template) {
    fprintf(f_debug, "(function template)\n");
  } else {
    /* Display the arg match list. */
    for (narg = 1, amsp = cfp->arg_matches;
         amsp != NULL;
         narg++, amsp = amsp->next) {
      fprintf(f_debug, "  arg %lu: ", narg);
      db_arg_match_summary(amsp);
    }  /* for */
  }  /* if */
}  /* db_candidate_function */

#endif /* DEBUG */
#if DEBUG

static void db_candidate_function_list(a_candidate_function_ptr cfp_list)
/*
Print a candidate function entry list for debugging purposes.
*/
{
  a_candidate_function_ptr cfp;

  fprintf(f_debug, "Candidate functions list:");
  if (cfp_list == NULL) {
    fprintf(f_debug, "NULL\n");
  } else {
    fprintf(f_debug, "\n");
    for (cfp = cfp_list; cfp != NULL; cfp = cfp->next) {
      db_candidate_function(cfp);
    }  /* for */
  }  /* if */
}  /* db_candidate_function_list */

#endif /* DEBUG */

static void add_function_to_candidate_functions_list(
                                 a_symbol_ptr             function_symbol,
                                 an_arg_match_summary_ptr arg_matches,
                                 a_candidate_function_ptr *candidate_functions)
/*
Add the function identified by function_symbol to the front of the
candidate_functions list.  arg_matches gives information about how well
the actual arguments we have match the function's formal parameters.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = function_symbol;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "add_function_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_function_to_candidate_functions_list */


static void add_builtin_operator_to_candidate_functions_list(
                                char                     *operand_type_pattern,
                                a_type_ptr               pointer_type,
                                an_arg_match_summary_ptr arg_matches,
                                a_candidate_function_ptr *candidate_functions)
/*
Add the built-in operator identified by operand_type_pattern and pointer_type
to the candidate_functions list.  arg_matches gives information about how well
the operands we have match the operator's required operand types.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->operand_type_pattern = operand_type_pattern;
  candidate->pointer_type = pointer_type;
  candidate->arg_matches = arg_matches;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "add_builtin_operator_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_builtin_operator_to_candidate_functions_list */


static void add_function_template_to_candidate_functions_list(
                                 a_symbol_ptr             function_symbol,
                                 an_arg_match_summary_ptr arg_matches,
                                 an_arg_operand_ptr       arg_operand_list,
                                 a_candidate_function_ptr *candidate_functions)
/*
Add the function template identified by function_symbol to the front of the
candidate_functions list.    arg_matches gives information about how well
the actual arguments we have match the function's formal parameters (but
the entries are really just place-holders).  arg_operand_list gives the
operand list.
*/
{
  a_candidate_function_ptr candidate;

  candidate = alloc_candidate_function();
  candidate->function_symbol = function_symbol;
  candidate->is_function_template = TRUE;
  candidate->arg_matches = arg_matches;
  candidate->arg_operand_list = arg_operand_list;
  candidate->next = *candidate_functions;
  *candidate_functions = candidate;
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug,
            "add_function_template_to_candidate_functions_list: added\n"); 
    db_candidate_function(candidate);
  }  /* if */
#endif /* DEBUG */
}  /* add_function_template_to_candidate_functions_list */


/*
Type codes used in type patterns that describe built-in operators
for overload resolution.
*/
#define POINTER_TYPE_CODE 'P'
#define INTEGRAL_TYPE_CODE 'I'
#define ARITH_TYPE_CODE 'A'
#define SCALAR_TYPE_CODE 'S'
#define CORRESP_POINTER_TYPE_CODE 'C'


static char *name_for_type_code(char type_code)
/*
Return a printable string describing a type code.
*/
{
  char *str;

  if (type_code == POINTER_TYPE_CODE ||
      type_code == CORRESP_POINTER_TYPE_CODE) {
    str = "pointer";
  } else if (type_code == INTEGRAL_TYPE_CODE) {
    str = "integer";
  } else if (type_code == ARITH_TYPE_CODE) {
    str = "arithmetic";
  } else {
#if CHECKING
    if (type_code != SCALAR_TYPE_CODE) {
      internal_error("name_for_type_code: bad type code");
    }  /* if */
#endif /* CHECKING */
    str = "scalar";
  }  /* if */
  return str;
}  /* name_for_type_code */


static void diagnose_overload_ambiguity(
                                  a_candidate_function_ptr candidate_functions,
                                  an_opname_kind           kind)
/*
Issue the add-on diagnostics to describe an overloading ambiguity.
candidate_functions gives the list of functions in the best-match set.
kind gives the operator associated with any entries in the set for
built-in operators.  The start_error or equivalent has already been done.
*/
{
  a_candidate_function_ptr cfp;
  a_symbol_ptr             function_sym;
  an_error_code            err_code;

  for (cfp = candidate_functions; cfp != NULL; cfp = cfp->next) {
    /* Print each candidate function. */
    function_sym = cfp->function_symbol;
    if (function_sym != NULL) {
      /* Normal function case. */
      if (function_sym->kind == (a_symbol_kind)sk_projection &&
          function_sym->variant.projection.ambiguous) {
        /* Function symbol is ambiguous by inheritance.  Use a special
           message.  This happens for conversion functions inherited
           into a derived class. */
        err_code = ec_ambiguous_by_inheritance_add_on;
      } else {
        /* Normal case. */
        err_code = ec_ambiguous_function_add_on;
      }  /* if */
      sym_add_diag_info(err_code, function_sym);
    } else {
      /* Built-in operator case. */
      /* Put out something like
           built-in operator "pointer + integer"
      */
      char buf[100]; /* Big enough for "arithmetic <= arithmetic". */
      char *pattern = cfp->operand_type_pattern;
      char *opname = opname_names[(int)kind];
      if (pattern[1] == '\0' || pattern[1] == ';') {
        /* Unary operator. */
        (void)sprintf(buf, "%s %s", opname, name_for_type_code(pattern[0]));
      } else {
        /* Binary operator. */
        (void)sprintf(buf, "%s %s %s", name_for_type_code(pattern[0]), opname,
                                       name_for_type_code(pattern[1]));
      }  /* if */
      str_add_diag_info(ec_builtin_operator_add_on, buf);
    }  /* if */
  }  /* for */
  end_error();
}  /* diagnose_overload_ambiguity */


static void determine_downward_cast_derivation(
                                             a_type_ptr           source_type,
                                             a_type_ptr           dest_type,
                                             an_arg_match_summary *arg_summary)
/*
source_type --> dest_type is a standard conversion.  If it is a cast to
a related class, fill in downward_cast_derivation in *arg_summary.
*/
{
  a_boolean        downward_cast;
  a_base_class_ptr bcp;

  if (related_class_pointers(source_type, dest_type, &downward_cast, &bcp)) {
    /* Cast to base class (no need to check downward_cast; downward
       is the only direction allowed as an implicit conversion). */
    arg_summary->downward_cast_derivation = bcp->derivation;
  } else if (related_member_pointers(source_type, dest_type, &downward_cast,
                                     &bcp)) {
    /* Likewise for casts of pointers-to-members; note, however, that
       implicit casts there are from base to derived. */
    arg_summary->downward_cast_derivation = bcp->derivation;
    arg_summary->reversed_derivation = TRUE;
  }  /* if */
}  /* determine_downward_cast_derivation */


static void set_arg_summary_for_user_conversion(
                                    an_arg_match_summary *arg_summary,
                                    a_type_ptr           param_type,
                                    a_routine_ptr        conversion_routine,
                                    a_boolean            std_conversion_needed)
/*
Set *arg_summary to indicate an argument match involving a user-defined
conversion using a conversion function.  param_type is the parameter
type, conversion_routine is the conversion function being called, and
std_conversion_needed is TRUE if a standard conversion is needed after
the conversion function.
*/
{
  a_type_ptr conversion_type;

  arg_summary->match_level = aml_user_conversion;
  arg_summary->conversion_routine = conversion_routine;
  if (std_conversion_needed) {
    arg_summary->std_conversion_after_user_conversion = TRUE;
    /* Get the return type of the conversion routine. */
    conversion_type = f_skip_typerefs(conversion_routine->type);
    conversion_type = conversion_type->variant.routine.return_type;
    /* If the standard conversion is a cast between related classes,
       set downward_cast_derivation. */
    determine_downward_cast_derivation(conversion_type, param_type,
                                       arg_summary);
  }  /* if */
}  /* set_arg_summary_for_user_conversion */


void determine_arg_match_level(an_operand           *arg_operand,
                               a_type_ptr           arg_type,
                               a_type_ptr           param_type,
                               a_boolean            try_user_conversions,
                               an_arg_match_summary *arg_summary)
/*
Determine how well an actual argument matches a formal parameter with type
param_type.  The actual argument is usually given by arg_operand, but
if arg_type is non-NULL, it provides the argument type for an argument
about which nothing else is known (and arg_operand is ignored).  arg_summary
is set to indicate the level of match.  This is used in resolving overloaded
function calls.  See ARM 13.2.  User-defined conversions will be attempted
only if try_user_conversions is TRUE; it must be FALSE if arg_type is non-NULL.
*/
{
  a_boolean        param_is_reference;
  a_boolean        ref_type_qualifiers_dropped, ref_type_qualifiers_added;
  an_error_code    warning_suggested;
  a_boolean        std_conversion_needed;
  a_base_class_ptr bcp;
  a_routine_ptr    conversion_routine;
  a_boolean        ambiguous;
  a_boolean        arg_operand_is_constant;
  a_constant_ptr   arg_operand_constant;
  an_operand       implicit_arg_operand;

  db_enter(4, "determine_arg_match_level");
  clear_arg_match_summary(arg_summary);
  if (arg_type == NULL) {
    /* Get the actual argument type from arg_operand. */
    arg_type = arg_operand->type;
  } else {
    /* arg_type is supplied, so arg_operand should be ignored. */
    arg_operand = NULL;
#if CHECKING
    if (try_user_conversions) {
      /* Cannot try user conversions without a full operand. */
      internal_error(
        "determine_arg_match_level: arg_type != NULL && try_user_conversions");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  /* Try an exact match or one involving trivial conversions.  This is
     case [1] in the ARM.  Trivial conversions are
       From:      To:
       T          T&
       T          qualified T
       T*         (qualified T)*
       T[]        T*
       T(args)    (*T)(args)
     T& --> T is done automatically in expression processing; an argument
     here never has a reference type T& -- it's an lvalue of type T.  Cases
     that involve
       From:      To:
       T*         (qualified T)*
       T          (qualified T)&
     (the latter coming from T --> qualified T --> (qualified T)&, and making
     more sense here than the ARM's T& --> (qualified T)& because the argument
     cannot be a reference) are considered worse than those that do not
     involve them.
  */
  /* Remove parts of the param type that could be added by trivial
     conversions, hoping thereby to end up with the arg type. */
  ref_type_qualifiers_dropped = ref_type_qualifiers_added = FALSE;
  param_is_reference = is_reference_type(param_type);
  if (param_is_reference) {
    /* The parameter type is a reference.  Drop the reference and remember
       we have one.  This is the "T --> T&" case.  Note that we're dropping any
       qualifiers above the reference type, but that's okay; they don't really
       mean anything ("int &const a" is meaningless). */
    param_type = type_pointed_to(param_type);
    /* Check the type qualifiers to see if they can be reconciled by
       trivial conversions. */
    if (type_qualifiers_match(param_type, arg_type)) {
      /* The qualifiers are the same: okay. */
    } else if (any_qualifier_missing(param_type, arg_type)) {
      /* There are some type qualifiers on the argument type that do not
         appear on the parameter type, so some type qualifiers are being
         dropped. */
      ref_type_qualifiers_dropped = TRUE;
    } else {
      /* Some type qualifiers are being added.  That's okay, but it's
         one of the "less desirable" cases. */
      ref_type_qualifiers_added = TRUE;
    }  /* if */
  } else {
    /* The parameter type is not a reference, which means the argument would
       have to be converted from an lvalue to an rvalue.  In the process,
       it would lose its top-level type qualifiers.  That means the type
       qualifiers must be compatible. */
    arg_type = skip_typerefs(arg_type);
    /* Qualifiers on the parameter type are also not significant when dealing
       with rvalues.  One cannot distinguish f(int) and f(const int). */
    param_type = skip_typerefs(param_type);
    /* See if the "T[] --> T*" and "T(args) --> T(*)(args)" cases apply. */
    if (arg_operand != NULL) {
      if (is_array_type(arg_type)) {
        if (is_an_lvalue(arg_operand)) {
          /* An array lvalue, or a string literal represented as an lvalue.
             This is the "T[] --> T*" case.  Make a operand to crudely
             simulate the operand one would get if one converted the
             operand to a pointer.  The real operand cannot be made
             without copying and allocating IL entries that would probably
             be wasted, and the crude simulation will act like the operand
             would in the argument match process.  The crude simulation
             is a NULL pointer constant of the right type. */
          arg_type = make_pointer_type(array_element_type(arg_type));
          clear_operand((an_operand_kind)ok_constant,
                        &implicit_arg_operand);
          implicit_arg_operand.position = arg_operand->position;
          make_zero_of_proper_type(arg_type,
                                   &implicit_arg_operand.variant.constant);
          implicit_arg_operand.type = arg_type;
          arg_operand = &implicit_arg_operand;
        }  /* if */
      } else if (is_a_function_designator(arg_operand)) {
        /* Function designator.  This is the "T(args) --> T(*)(args)" case.
           Make an operand that is the function converted to an rvalue
           that's a pointer to the function.  Do not use
           conv_function_designator_to_ptr_to_function because it
           can generate errors. */
        copy_operand(arg_operand, &implicit_arg_operand);
        arg_operand = &implicit_arg_operand;
        /* Watch out for indefinite function designators.  Leave their types
           unknown (or leave arg_type as determined by the match-up code
           above). */
        if (!is_indefinite_function_operand(arg_operand)) {
          arg_operand->type = arg_type = make_pointer_type(arg_operand->type);
        }  /* if */
        arg_operand->state = (an_operand_state)os_rvalue;
      }  /* if */
    }  /* if */
  }  /* if */
  /* We've now done transformations for all the trivial conversions except
     those that involve adding type qualifiers.  We therefore now have
     the essential underlying types for the rest of the checking. */
  arg_summary->base_param_type = param_type;
  if (is_error_type(arg_type) || is_error_type(param_type)) {
    /* An error type matches anything, but not very well. */
    arg_summary->match_level = aml_error;
    goto have_level;
  }  /* if */
  /* If the type qualifiers are not okay, do not check for an exact match
     or a match with promotions.  Cases other than those do their own
     checking of type qualifiers. */
  if (!ref_type_qualifiers_dropped) {
    a_type_ptr unqual_arg_type = skip_typerefs(arg_type);
    a_type_ptr unqual_param_type = skip_typerefs(param_type);
    /* Check for an exact match.  This is case [1] in the ARM. */
    if (types_are_compatible(unqual_arg_type, unqual_param_type)) {
      /* There is an exact match, possibly involving trivial conversions. */
      arg_summary->match_level = aml_exact;
      if (ref_type_qualifiers_added) {
        /* This is the "T --> (qualified T)& case, which is one of the
           "less desirable" cases. */
        arg_summary->less_desirable_exact_match = TRUE;
      }  /* if */
      goto have_level;
    }  /* if */
    /* Check for another exact match case, for pointers involving addition
       of type qualifiers on the type pointed to (the "T* --> (qualified T)*"
       case). */
    if (is_pointer_type(param_type) && is_pointer_type(arg_type)) {
      a_type_ptr arg_type_pointed_to = type_pointed_to(arg_type);
      a_type_ptr param_type_pointed_to = type_pointed_to(param_type);
      if (types_are_compatible(skip_typerefs(arg_type_pointed_to),
                               skip_typerefs(param_type_pointed_to))) {
        /* The types pointed to are compatible.  See if the qualifiers
           are okay.  Note that the case where the qualifiers are the same
           need not be checked for, since it would have been handled
           above in the normal exact-match case. */
        if (any_qualifier_missing(param_type_pointed_to,
                                  arg_type_pointed_to)) {
          /* There are some qualifiers being dropped, so the pointer types
             are not compatible. */
        } else {
          /* Some qualifiers are being added.  This is the
             "T* --> (qualified T)*" case, one of the "less desirable"
             cases. */
          arg_summary->match_level = aml_exact;
          arg_summary->less_desirable_exact_match = TRUE;
          goto have_level;
        }  /* if */
      }  /* if */
    }  /* if */
    if (arg_operand != NULL && is_indefinite_function_operand(arg_operand)) {
      /* The source is an indefinite function, i.e., the address of an
         overloaded function.  It can be converted to an appropriate
         pointer (ARM 13.3) or pointer-to-member type (not mentioned in ARM,
         but sensible).  Note that in the pointer-to-member case standard
         conversions (to a derived class) may also be required. */
      if (find_addr_of_overloaded_function_match(arg_operand->variant.symbol,
                                                 param_type,
                                                 &arg_operand->position,
                                                 &arg_summary->match_level,
                                                 &ambiguous) || ambiguous) {
        /* There is a suitable indefinite function, or more than one.
           arg_summary->match_level has been set appropriately. */
        goto have_level;
      }  /* if */
    }  /* if */
    /* Try a match involving promotions.  This is case [2] in the ARM.
       Promotions are the default argument promotions (integral promotions
       and float --> double). */
    if (types_are_compatible(default_argument_promotion(unqual_arg_type),
                             unqual_param_type)) {
      arg_summary->match_level = aml_promotion;
      goto have_level;
    }  /* if */
  }  /* if */
  /* Try a match involving standard conversions.  This is case [3] in
     the ARM. */
  arg_operand_is_constant = FALSE;
  arg_operand_constant = NULL;
  if (arg_operand != NULL && is_an_rvalue(arg_operand)) {
    arg_operand_is_constant = is_constant_operand(arg_operand);
    if (arg_operand_is_constant) {
      arg_operand_constant = &arg_operand->variant.constant;
    }  /* if */
  }  /* if */
  if (impl_conversion_possible(arg_type,
                               arg_operand_is_constant,
                               arg_operand_constant,
                               param_type, /*suppress_extensions=*/TRUE,
                               ec_incompatible_param, &warning_suggested)) {
    /* Match with standard conversions. */
    arg_summary->match_level = aml_std_conversion;
    arg_summary->warning_suggested = warning_suggested;
    /* If the cast is from a pointer to a derived class to a pointer to a
       base class, set downward_cast_derivation. */
    determine_downward_cast_derivation(arg_type, param_type, arg_summary);
    if (cfront_compatibility_mode && param_is_reference &&
        arg_summary->downward_cast_derivation == NULL) {
      /* cfront 2.1 has a bug: when a reference parameter is initialized
         with something that requires a standard conversion that isn't
         class-related, the cost is considered to be a user-defined
         conversion. */
      arg_summary->match_level = aml_user_conversion;
    }  /* if */
    goto have_level;
  }  /* if */
  if (is_class_struct_union_type(param_type) &&
      is_class_struct_union_type(arg_type) &&
      !ref_type_qualifiers_dropped &&
      (bcp = find_base_class_of(arg_type, param_type)) != NULL) {
    /* The argument is a derived class and the parameter is a base class,
       so the conversion can be done. */
    /* This is permitted by the reference standard conversions (ARM 4.7) when
       param_is_reference is TRUE, and by the aggregate initialization rules
       (ARM 8.4.1) and the copy constructor rules (ARM 12.8) when
       param_is_reference is FALSE. */
    arg_summary->match_level = aml_std_conversion;
    arg_summary->downward_cast_derivation = bcp->derivation;
    goto have_level;
  }  /* if */
  if (try_user_conversions) {
    /* Try a match involving user-defined conversions.  This is case [4]
       in the ARM. */
    /* Note that we will not get here if arg_operand is NULL, that is, if we
       have an argument type but no argument operand. */
    if (is_class_struct_union_type(param_type) &&
        (conversion_to_class_possible(arg_operand, param_type,
                                      &conversion_routine, &ambiguous,
                                      (a_candidate_function_ptr *)NULL) ||
         ambiguous)) {
      /* There is a constructor or conversion function (or several) that
         will convert the argument type to the parameter class type. */
      arg_summary->match_level = aml_user_conversion;
      goto have_level;
    } else if (is_class_struct_union_type(arg_type) &&
               (conversion_from_class_possible(arg_operand, param_type,
                                               (a_builtin_type_kind_set)
                                                                      BTK_NONE,
                                               &conversion_routine,
                                               &std_conversion_needed,
                                               &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
                ambiguous)) {
      /* There is a conversion function (or several) that will convert the
         argument class type into the parameter type or to some type that
         can be converted to the parameter type via a standard conversion. */
      set_arg_summary_for_user_conversion(arg_summary, param_type,
                                          conversion_routine,
                                          std_conversion_needed);
      goto have_level;
    }  /* if */
  }  /* if */
  /* Case [5] in the ARM, match with ellipsis, is handled by the caller. */
  /* No match is possible. */
  arg_summary->match_level = aml_none;
have_level:;
#if DEBUG
  if (debug_level >= 4) {
    if (arg_summary->match_level == aml_none) {
      fprintf(f_debug, "determine_arg_match_level: no match\n");
    } else {
      fprintf(f_debug, "determine_arg_match_level: ");
      db_arg_match_summary(arg_summary);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* determine_arg_match_level */


void selector_match_with_this_param(
                               an_operand           *bound_function_selector,
                               a_boolean            selector_is_object_pointer,
                               a_boolean            conversion_function_case,
                               a_routine_ptr        rout,
                               a_type_ptr           routine_type,
                               an_arg_match_summary *this_match_summary)
/*
Determine how well the selector object indicated by *bound_function_selector
matches the "this" parameter of the routine type indicated by *routine_type.
If selector_is_object_pointer is TRUE, *bound_function_selector has
already been converted to object pointer form.  Otherwise, it's just
an object (lvalue or rvalue).  Return the match summary in *this_match_summary.
If the specific routine being called is known, rout points to the
routine entry; otherwise, rout is NULL.  rout must be non-NULL when
calling a constructor or destructor, so that those can be treated as a
special case: constructors and destructors can be called for const- and
volatile-qualified objects even though they themselves are not (and
cannot be) const- or volatile-qualified.  If conversion_function_case
is TRUE, the underlying type of the selector is assumed to be the proper
class or a derived class thereof (except for error cases).
*/
{
  a_type_ptr selector_type, this_param_type, this_param_base_type;
  a_type_ptr this_param_class_type, const_this_param_base_type;
  a_type_ptr ptr_selector_type, const_this_param_type;

  db_enter(4, "selector_match_with_this_param");
  if (rout != NULL &&
      (rout->special_kind == (a_special_function_kind)sfk_constructor ||
       rout->special_kind == (a_special_function_kind)sfk_destructor)) {
    /* The routine is a constructor or destructor, so the check is
       suppressed. */
    clear_arg_match_summary(this_match_summary);
    this_match_summary->match_level = aml_exact;
  } else {
    /* Get the "this" parameter type. */
    this_param_type =
            routine_type->variant.routine.extra_info->implicit_this_param_type;
#if CHECKING
    if (this_param_type == NULL) {
      internal_error("selector_match_with_this_param: this_param_type NULL");
    }  /* if */
#endif /* CHECKING */
    this_param_base_type = type_pointed_to(this_param_type);
    this_param_class_type = skip_typerefs(this_param_base_type);
    /* Determine the effective selector type. */
    selector_type = bound_function_selector->type;
    if (!is_error_type(selector_type)) {
      if (selector_is_object_pointer) {
        selector_type = type_pointed_to(selector_type);
      }  /* if */
      /* For conversion functions projected into a derived class,
         the base class cast has not been done yet (and cannot be, since we
         are just investigating whether or not the function is appropriate).
         However, the cast to the base class is not supposed to be counted
         as a standard conversion in the overload resolution "cost", because
         the reference can be thought of as being to the projection of the
         function into the derived class.  Change the selector type to the
         properly-qualified version of the proper base class (the class of
         the function), thus in effect doing any required base class cast.
         The underlying class of the selector must already be the base
         class or a derived class, or the function would not have been
         found. */
      if (conversion_function_case) {
        selector_type = make_identically_qualified_type(this_param_class_type,
                                                        selector_type);
      }  /* if */
    }  /* if */
    ptr_selector_type = make_pointer_type(selector_type);
    /* See how well the selector type and the "this" parameter type
       match up. */
    determine_arg_match_level((an_operand *)NULL, ptr_selector_type,
                              this_param_type,
                              /*try_user_conversions=*/FALSE,
                              this_match_summary);
    if (cfront_compatibility_mode &&
        this_match_summary->match_level == aml_none) {
      /* No match.  Try the cfront anachronism of calling a function that
         does not require a const "this" with a const selector.  See also
         set_up_for_conversion_function_call. */
      /* Make the type that the "this" parameter would have if the routine
         were const, and try again. */
      const_this_param_base_type = make_qualified_type(this_param_base_type,
                                                       /*is_const=*/TRUE,
                                                       /*is_volatile=*/FALSE);
      const_this_param_type = make_pointer_type(const_this_param_base_type);
      const_this_param_type = make_qualified_type(const_this_param_type,
                                                  /*is_const=*/TRUE,
                                                  /*is_volatile=*/FALSE);
      determine_arg_match_level((an_operand *)NULL, ptr_selector_type,
                                const_this_param_type,
                                /*try_user_conversions=*/FALSE,
                                this_match_summary);
      if (this_match_summary->match_level != aml_none) {
        /* Anachronism -- calling non-const function with const object. */
	this_match_summary->const_anachronism = TRUE;
        this_match_summary->warning_suggested =
                                           ec_unqual_function_with_qual_object;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* selector_match_with_this_param */


static void try_overloaded_function_match(
                 a_symbol_ptr             overloaded_function_symbol,
                 an_arg_operand_ptr       arg_operand_list,
                 a_boolean                have_selector,
                 an_operand               *bound_function_selector,
                 a_boolean                selector_is_object_pointer,
                 a_boolean                try_user_conversions,
                 a_candidate_function_ptr *candidate_functions,
                 a_boolean                *matched_except_for_missing_selector)
/*
Find out how well the functions described by overloaded_function_symbol
match the argument list given by arg_operand_list and the selector given
(if have_selector is TRUE) by bound_function_selector.
overloaded_function_symbol may be an overloaded function, a simple
function, or a projection symbol for one of those.  bound_function_selector
is an object pointer if selector_is_object_pointer is TRUE, an object
otherwise.  User-defined conversions are tried on argument matches only
if try_user_conversions is TRUE.  Any viable functions are added to the
candidate_functions list along with information on the level of argument
matches.  Note that, for constructor calls, bound_function_selector can be
NULL when have_selector is TRUE.  If a match would have been found except
for the absence of a selector, set *matched_except_for_missing_selector
TRUE; that allows a different error message.
*/
{
  a_boolean                overloaded_function_case;
  a_symbol_ptr             function_symbol;
  a_type_ptr               routine_type;
  a_param_type_ptr         param;
  a_boolean                reached_ellipsis;
  an_arg_match_summary_ptr this_match, this_match_next;
  an_arg_match_summary_ptr arg_match, arg_match_list, end_arg_match_list;
  an_arg_operand_ptr       arg_operand;
  a_boolean                function_is_nonstatic_member_function;
  a_boolean                function_template_case;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */

  reduce_projection_symbol_to_fundamental_symbol(overloaded_function_symbol);
  /* Determine whether or not the symbol is an overloaded function. */
  overloaded_function_case = (overloaded_function_symbol->kind ==
                                        (a_symbol_kind)sk_overloaded_function);
  if (overloaded_function_case) {
    /* Overloaded functions. */
    function_symbol =
               overloaded_function_symbol->variant.overloaded_function.symbols;
  } else {
    /* Non-overloaded function. */
    function_symbol = overloaded_function_symbol;
  }  /* if */
  /* Look at each instance of the overloaded function and see whether or
     not it can match the actual arguments, and if so, how well. */
  for (; function_symbol != NULL;
       function_symbol = overloaded_function_case ? function_symbol->next :
                                                    NULL) {
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(function_symbol, "try_overloaded_function_match: considering ",
                2); 
    }  /* if */
    narg = 0;
#endif /* DEBUG */
    function_template_case = (function_symbol->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (function_template_case) {
      /* The symbol is a function template. */
      routine_type = function_symbol->variant.template_info->
                                                variant.function.routine->type;
      routine_type = skip_typerefs(routine_type);      
    } else {
      /* The symbol is not a function template (i.e., it's a normal
         function). */
      routine_type = routine_symbol_type(function_symbol);
    }  /* if */
    /* Look at each argument and see whether or not it can match the formal
       parameter, and if so, how well. */
    param = routine_type->variant.routine.extra_info->param_type_list;
    reached_ellipsis = FALSE;
    arg_match_list = end_arg_match_list = NULL;
    for (arg_operand = arg_operand_list;
         arg_operand != NULL;
         arg_operand = arg_operand->next) {
#if DEBUG
      narg++;
      if (debug_level >= 4) {
        fprintf(f_debug, "try_overloaded_function_match: arg %lu\n", narg);
      }  /* if */
#endif /* DEBUG */
      /* Add an entry to the end of the arg_match_list to record whether or
         not this argument matches. */
      arg_match = alloc_arg_match_summary();
      if (arg_match_list == NULL) {
        arg_match_list = arg_match;
      } else {
        end_arg_match_list->next = arg_match;
      }  /* if */
      end_arg_match_list = arg_match;
      /* See if the parameter list is exhausted. */
      if (param == NULL) {
        /* More arguments than required.  No match unless there is an
           ellipsis. */
        reached_ellipsis =
                        routine_type->variant.routine.extra_info->has_ellipsis;
        if (!reached_ellipsis) goto reject_function;
        /* There is an ellipsis, so there is a match, but with a low
           desirability. */
        arg_match->match_level = aml_ellipsis;
#if DEBUG
        if (debug_level >= 4) {
          fprintf(f_debug, "try_overloaded_function_match: ellipsis match\n");
        }  /* if */
#endif /* DEBUG */
      } else {
        /* Both the actual argument and formal parameter are available. */
        if (!function_template_case) {
          /* Compare their types. */
          determine_arg_match_level(&arg_operand->operand, (a_type_ptr)NULL,
                                    param->type,
                                    try_user_conversions,
                                    arg_match);
          /* If no match is possible, go on to the next function. */
          if (arg_match->match_level == aml_none) goto reject_function;
        }  /* if */
      }  /* if */
      /* Go on to the next parameter. */
      if (!reached_ellipsis) param = param->next;
    }  /* for */
    /* Check that the argument and parameter lists ended at the same place. */
    if (param != NULL) {
      /* Fewer arguments than required.  No match unless there are default
         argument values. */
      if (param->default_arg_expr == NULL) goto reject_function;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "try_overloaded_function_match: default arg match\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
    /* All the arguments can be made to match the parameters. */
    /* See if the "this" parameter, if any, matches. */
    /* Template functions do not have "this" parameters. */
    if (!function_template_case) {
      function_is_nonstatic_member_function =
                       routine_type_is_nonstatic_member_function(routine_type);
      if (have_selector) {
        /* We have a selector. */
        /* Put a match entry for it on the front of the match list. */
        this_match = alloc_arg_match_summary();
        this_match->next = this_match_next = arg_match_list;
        arg_match_list = this_match;
        if (!function_is_nonstatic_member_function) {
          /* The function has no "this" parameter, so it does not need a
             selector.  We would discard it if this function is chosen,
             but we still need a match entry for it.  It counts as an
             exact match. */
          this_match->match_level = aml_exact;
        } else {
          /* The function requires a "this" parameter. */
          if (bound_function_selector == NULL) {
            /* We're dealing with a constructor case, the selector
               expression is not available, and we can assume that it matches
               perfectly (const- and volatile- qualifiers are not allowed
               on constructors). */
            this_match->match_level = aml_exact;
          } else {
            /* See how the selector expression matches the "this" parameter
               type. */
            selector_match_with_this_param(bound_function_selector,
                                           selector_is_object_pointer,
                                           /*conversion_function_case=*/FALSE,
                                          function_symbol->variant.routine.ptr,
                                           routine_type, this_match);
            /* Set the "next" pointer again, because it is cleared by
               selector_match_with_this_param. */
            this_match->next = this_match_next;
            if (this_match->match_level == aml_none) goto reject_function;
          }  /* if */
        }  /* if */
      } else {
        /* We have no selector. */
        if (function_is_nonstatic_member_function) {
          /* The function has a "this" parameter, so it is not suitable.
             Remember this case to select a different error message if it
             turns out no function matches. */
          *matched_except_for_missing_selector = TRUE;
          goto reject_function;
        }  /* if */
      }  /* if */
    }  /* if */
    /* The function is a viable candidate.  Add it to the candidates
       list. */
    if (function_template_case) {
      /* The symbol is a function template. */
      add_function_template_to_candidate_functions_list(function_symbol,
                                                        arg_match_list,
                                                        arg_operand_list,
                                                        candidate_functions);
    } else {
      /* The symbol is a normal function. */
      add_function_to_candidate_functions_list(function_symbol,
                                               arg_match_list,
                                               candidate_functions);
    }  /* if */
    goto next_function;
reject_function:
    /* The function is not suitable. */
    /* Free any argument match summary entries built for it. */
    free_arg_match_summary_list(arg_match_list);
next_function:;
    /* Keep looping to try all the functions in the overload set. */
  }  /* for */
}  /* try_overloaded_function_match */


static a_derivation_step_ptr prev_derivation(
                                            a_derivation_step_ptr derivation,
                                            an_arg_match_summary  *arg_summary)
/*
Find the derivation entry on the downward_cast_derivation list of arg_summary
that precedes "derivation" and return a pointer to it.  Return NULL if there
is no previous entry.
*/
{
  a_derivation_step_ptr prev;

  prev = arg_summary->downward_cast_derivation;
  if (prev == derivation) {
    /* The derivation is the first on the list, so there's no previous
       entry. */
    prev = NULL;
  } else {
    /* Find the previous entry. */
    for (; prev->next != derivation; prev = prev->next) {}
  }  /* if */
  return prev;
}  /* prev_derivation */


static int compare_arg_match_levels(an_arg_match_summary *arg_match1,
                                    an_arg_match_summary *arg_match2)
/*
Compare two argument match summary entries and return

  +1 if arg_match1 is a better match than arg_match2,
   0 if the two matches are equal, or
  -1 if arg_match1 is a worse match than arg_match2.

*/
{
  int                   cmp;
  a_derivation_step_ptr derivation_1, derivation_2;
  a_boolean             reversed_derivation;
  a_type_ptr            param_type1, param_type2;
  a_type_ptr            under_type1, under_type2;

  if ((int)arg_match1->match_level < (int)arg_match2->match_level) {
    /* arg_match1 is better. */
    cmp = 1;
  } else if ((int)arg_match1->match_level > (int)arg_match2->match_level) {
    /* arg_match2 is better. */
    cmp = -1;
  } else {
    /* The major match levels are equal.  Look for tie-breakers. */
    /* Exact matches can be distinguished by the presence of "less
       desirable" trivial conversions, those that add type qualifiers
       to the underlying types of reference and pointer types. */
    if (arg_match1->match_level == (an_arg_match_level)aml_exact &&
        arg_match1->less_desirable_exact_match !=
                                      arg_match2->less_desirable_exact_match) {
      if (arg_match1->less_desirable_exact_match) {
        /* arg_match1 is a less desirable exact match, and arg_match2 is
           not, so arg_match2 is better. */
        cmp = -1;
      } else {
        /* arg_match2 is a less desirable exact match, and arg_match1 is
           not, so arg_match1 is better. */
        cmp = 1;
      }  /* if */
    } else if (cfront_compatibility_mode &&
               arg_match1->const_anachronism != arg_match2->const_anachronism){
      /* In cfront compatibility mode, the anachronism that allows a
         non-const function to be called for a const object causes matches
         that are considered worse than the corresponding matches that do
         not involve the anachronism. */
      if (arg_match1->const_anachronism) {
        /* arg_match1 uses the const anachronism and arg_match2 does not,
           so arg_match2 is better. */
        cmp = -1;
      } else {
        /* arg_match2 uses the const anachronism and arg_match1 does not,
           so arg_match1 is better. */
        cmp = 1;
      }  /* if */
    } else {
      /* The matches are equal in terms of match level.  One can still be
         better than the other if the conversion in one case is a
         subsequence of the conversion in the other case. */
      /* Check first for subsequence cases involving user-defined conversions,
         like
           A->int
         versus
           A->int->float
      */
      if (arg_match1->conversion_routine != arg_match2->conversion_routine) {
        /* Two different conversion functions are involved, so no subsequence
           is possible. */
        goto end_subsequence_check;
      } else if (arg_match1->conversion_routine != NULL &&
                 arg_match1->std_conversion_after_user_conversion !=
                            arg_match2->std_conversion_after_user_conversion) {
        /* Two user-defined conversions involving the same conversion routine.
           One does not have a standard conversion after the user-defined
           conversion and the other does, so the one without the standard
           conversion is better. */
        if (arg_match1->std_conversion_after_user_conversion) {
          /* arg_match1 has the standard conversion and arg_match2 does not,
             so arg_match2 is better. */
          cmp = -1;
          goto have_cmp;
        } else {
          /* arg_match2 has the standard conversion and arg_match1 does not,
             so arg_match1 is better. */
          cmp = 1;
          goto have_cmp;
        }  /* if */
      }  /* if */
      /* More subsequence checking: check for subsequences in standard
         conversions, involving casts to base or derived class types. */
      if ((arg_match1->std_conversion_after_user_conversion ||
           arg_match1->match_level == (an_arg_match_level)aml_std_conversion)&&
          (arg_match2->std_conversion_after_user_conversion ||
           arg_match2->match_level == (an_arg_match_level)aml_std_conversion)){
        /* Both matches involve a standard conversion. */
        derivation_1 = arg_match1->downward_cast_derivation;
        derivation_2 = arg_match2->downward_cast_derivation;
        if (derivation_1 != NULL && derivation_2 != NULL) {
          /* Both entries have downward casts, so they can be compared.  If one
             is a subsequence of the other, the shorter derivation is
             preferable. */
          reversed_derivation = arg_match1->reversed_derivation;
          if (reversed_derivation) {
            /* For pointers to members, the derivation given is in reverse
               order.  We still want the shorter derivation, but extra steps
               on the longer derivation are at the beginning of the list
               rather than the end. */
            /* Find the last entry on each list so we can start there. */
            while (derivation_1->next != NULL) {
              derivation_1 = derivation_1->next;
            }  /* while */
            while (derivation_2->next != NULL) {
              derivation_2 = derivation_2->next;
            }  /* while */
          }  /* if */
          /* Loop comparing entries as long as they match. */
          do {
            /* Note that the "->type" part in the following comparison is not
               needed for the non-reversed case, but it IS needed in the
               reversed case. */
            if (derivation_1->base_class->type !=
                                              derivation_2->base_class->type) {
              /* The derivations go different ways, so they cannot be compared
                 and are considered equal in terms of argument match level. */
              goto end_subsequence_check;
            }  /* if */
            /* Advance to the next entries.  For the reversed case, this means
               backing up. */
            if (!reversed_derivation) {
              derivation_1 = derivation_1->next;
              derivation_2 = derivation_2->next;
            } else {
              derivation_1 = prev_derivation(derivation_1, arg_match1);
              derivation_2 = prev_derivation(derivation_2, arg_match2);
            }  /* if */
          } while (derivation_1 != NULL && derivation_2 != NULL);
          /* See if the two lists (equal so far) ended together. */
          if (derivation_2 != NULL) {
            /* derivation_1 is shorter and thus preferable. */
            cmp = 1;
            goto have_cmp;
          } else if (derivation_1 != NULL) {
            /* derivation_2 is shorter and thus preferable. */
            cmp = -1;
            goto have_cmp;
          }  /* if */
        } else if (derivation_1 != NULL) {
          /* derivation_1 != NULL, derivation_2 == NULL.  A base class cast is
             preferable to another kind of cast (e.g., a cast to "void *"),
             so arg_match1 is better. */
          cmp = 1;
          goto have_cmp;
        } else if (derivation_2 != NULL) {
          /* derivation_1 == NULL, derivation_2 != NULL.  A base class cast is
             preferable to another kind of cast (e.g., a cast to "void *"),
             so arg_match2 is better. */
          cmp = -1;
          goto have_cmp;
        }  /* if */
      }  /* if */
      /* More subsequence checking: check for differences of type qualifiers
         at the end of conversions (trivial conversions), like
           float->int
         versus
           float->int->const int
      */
      param_type1 = arg_match1->base_param_type;
      param_type2 = arg_match2->base_param_type;
      /* Some cases (e.g., ellipsis) have no base param type. */
      if (param_type1 != NULL && param_type2 != NULL) {
        if (type_qualifiers_match(param_type1, param_type2)) {
          /* The two types have the same qualifiers, so one cannot be different
             than the other on the basis of qualifiers. */
        } else {
          /* The qualifiers are different, so it's worth checking further. */
          if (types_are_compatible(skip_typerefs(param_type1),
                                   skip_typerefs(param_type2))) {
            /* The underlying types are the same, so it's possible than
               one has a subset of the other's qualifiers. */
            if (!any_qualifier_missing(param_type1, param_type2)) {
              /* param_type2 has a proper subset of the qualifiers in
                 param_type1, so arg_match2 is the better match. */
              cmp = -1;
              goto have_cmp;
            } else if (!any_qualifier_missing(param_type2, param_type1)) {
              /* param_type1 has a proper subset of the qualifiers in
                 param_type2, so arg_match1 is the better match. */
              cmp = 1;
              goto have_cmp;
            }  /* if */
          }  /* if */
        }  /* if */
        /* More subsequence checking: check for differences of type qualifiers
           at the end of conversions to pointer types (trivial conversions),
           like
             char*->void*
           versus
             char*->void*->const void*
        */
        if (is_pointer_type(param_type1) && is_pointer_type(param_type2)) {
          under_type1 = type_pointed_to(param_type1);
          under_type2 = type_pointed_to(param_type2);
          if (type_qualifiers_match(under_type1, under_type2)) {
            /* The two types have the same qualifiers, so one cannot be
               different than the other on the basis of qualifiers. */
          } else {
            /* The qualifiers are different, so it's worth checking further. */
            if (types_are_compatible(skip_typerefs(under_type1),
                                     skip_typerefs(under_type2))) {
              /* The underlying types are the same, so it's possible than
                 one has a subset of the other's qualifiers. */
              if (!any_qualifier_missing(under_type1, under_type2)) {
                /* under_type2 has a proper subset of the qualifiers in
                   under_type1, so arg_match2 is the better match. */
                cmp = -1;
                goto have_cmp;
              } else if (!any_qualifier_missing(under_type2, under_type1)) {
                /* under_type1 has a proper subset of the qualifiers in
                   under_type2, so arg_match1 is the better match. */
                cmp = 1;
                goto have_cmp;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
end_subsequence_check:
      /* No subsequence was found, so the matches are equal. */
      cmp = 0;
    }  /* if */
  }  /* if */
have_cmp:
  return cmp;
}  /* compare_arg_match_levels */


/*
Set the current argument pointer in the indicated candidate function entry
to the first argument match.
*/
#define set_first_arg_match(cfp)                                      \
  ((cfp)->current_arg_match = (cfp)->arg_matches)

/*
Advance the current argument pointer in the indicated candidate function
entry to the next argument match.
*/
#define advance_arg_match(cfp)                                        \
  ((cfp)->current_arg_match = (cfp)->current_arg_match->next)


static a_boolean function_template_matches_operand_list(
                                           a_symbol_ptr       templ_sym,
                                           an_arg_operand_ptr arg_operand_list,
                                           a_source_position  *source_pos,
                                           a_symbol_ptr       *instance_symbol)
/*
Find out whether or not an instantiation of the function template
templ_sym can be made to match the operands in arg_operand_list.
If so, return TRUE and set *instance_symbol to the symbol for the specific
instance of the template.  *source_pos is the source position of the
reference.  Note that it has already been determined that the function
template has the right number of parameters.
*/
{
  a_param_type_ptr   ptp;
  a_template_arg_ptr templ_arg_list = NULL;
  a_symbol_ptr       sym = NULL;
  a_routine_ptr      routine;
  an_arg_operand_ptr arg_operand;
  a_routine_type_supplement_ptr
                     rtsp;
  a_type_ptr         param_type, arg_type;

  db_enter(4, "function_template_matches_operand_list");
#if CHECKING
  if (templ_sym->kind != (a_symbol_kind)sk_function_template) {
    internal_error("function_template_matches_operand_list: bad symbol");
  }  /* if */
#endif /* CHECKING */
  routine = templ_sym->variant.template_info->variant.function.routine;
  rtsp = routine->type->variant.routine.extra_info;
  /* Compare the types of the arguments to the parameter types. */
  ptp = rtsp->param_type_list;
  arg_operand = arg_operand_list;
  for (;ptp != NULL && arg_operand != NULL;
       ptp = ptp->next, arg_operand = arg_operand->next) {
    /* Try to match up the parameter type and the argument type. */
    param_type = ptp->type;
    arg_type = arg_operand->operand.type;
    if (is_reference_type(param_type)) {
      /* For a reference type, the argument must be an lvalue or a function
         designator. */
      /* Also allow error operands. */
      if (is_an_rvalue(&arg_operand->operand)) goto done;
      /* Drop the reference type. */
      param_type = type_pointed_to(param_type);
    } else {
      /* Not a reference. */
      /* Do the array-->pointer and function-->pointer transformations. */
      if (is_array_type(arg_type)) {
        arg_type = make_pointer_type(array_element_type(arg_type));
      } else if (is_function_type(arg_type)) {
        arg_type = make_pointer_type(arg_type);
      }  /* if */
    }  /* if */
    /* As the matching is attempted, templ_arg_list is filled in with
       the bindings for the template arguments.  This is needed during the
       matching process to ensure that each argument is used consistently
       and also later in this routine to build the instantiation. */
    if (!matches_template_type(arg_type, param_type, &templ_arg_list)) {
      goto done;
    }  /* if */
  }  /* for */
#if CHECKING
  if (arg_operand != NULL) {
    /* We ran out of parameters, but we still have arguments.  There should
       be an ellipsis. */
    if (!rtsp->has_ellipsis) {
      internal_error(
                   "function_template_matches_operand_list: missing ellipsis");
    }  /* if */
  } else if (ptp != NULL) {
    /* We ran out of arguments, but we still have parameters.  The parameter
       should have a default argument expression. */
    if (ptp->default_arg_expr == NULL) {
      internal_error(
           "function_template_matches_operand_list: missing default arg expr");
    }  /* if */
  }  /* if */
#endif /* CHECKING */
  /* The function template matches the operand list.  Make an instantiation
     of the template. */
  sym = find_template_function(templ_sym, &templ_arg_list, source_pos);
done:
  if (sym == NULL && templ_arg_list != NULL) {
    /* Free the template argument list if we did not use it. */
    free_template_arg_list(templ_arg_list);
  }  /* if */
  *instance_symbol = sym;
  db_exit();
  return (sym != NULL);
}  /* function_template_matches_operand_list */


static a_boolean match_is_better_on_at_least_one_arg(
                                           a_candidate_function_ptr best_cfp,
                                           a_candidate_function_ptr candidates)
/*
Compare the candidate function best_cfp against all the other candidate
functions in candidates.  Return TRUE if best_cfp's arguments matches are
a strictly better match for at least one argument for each of the other
functions (not necessarily the same argument for each function).  This is
the final test required for overload resolution (see ARM 13.2).
*/
{
  a_candidate_function_ptr cfp;
  a_boolean                match_is_better = TRUE;
  an_arg_match_summary_ptr curr_arg, best_curr_arg;

  /* Note that we start with the assumption that the function we're checking
     is at least as good on every argument as all of the other functions.
     That's established by the first part of overload resolution.  Here, we
     only have to look for some argument on which the chosen function is
     better than each of the other functions. */
  /* Loop through all the other functions. */
  for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
    /* Skip the chosen function itself. */
    if (cfp != best_cfp) {
      /* Compare the match level of each argument of the chosen function
         with the same argument of another function. */
      set_first_arg_match(best_cfp);
      set_first_arg_match(cfp);
      for (;;) {
        best_curr_arg = best_cfp->current_arg_match;
        if (best_curr_arg == NULL) break;
        curr_arg = cfp->current_arg_match;
        /* As soon as we find an argument for which the chosen
           function is better than the other function, we can stop
           checking.  Consider an error match to be (possibly) better
           than another match. */
        if (compare_arg_match_levels(best_curr_arg, curr_arg) > 0 ||
            best_curr_arg->match_level == aml_error) {
          goto check_next_function;
        }  /* if */
        advance_arg_match(best_cfp);
        advance_arg_match(cfp);
      }  /* for */
      /* All the argument matches have the same level.  The fact that a
         standard conversion is needed after a conversion function can still
         serve as a tie-breaker. */
      if (!best_cfp->std_conversion_after_conversion_function &&
          cfp->std_conversion_after_conversion_function) {
        goto check_next_function;
      }  /* if */
      /* The chosen function is not any better than this other function. */
      match_is_better = FALSE;
      break;
    }  /* if */
check_next_function:;
  }  /* for */
  return match_is_better;
}  /* match_is_better_on_at_least_one_arg */


static void select_best_candidate_functions(
                        a_candidate_function_ptr *candidate_functions,
                        a_source_position        *source_pos,
                        a_boolean                *undecidable_because_of_error)
/*
*candidate_functions is the list of viable functions for a particular
overloaded function call.  From that set, select the best functions
and set *candidate_functions to that set.  Other candidate functions
that do not make the "best" set are freed.  On return from this function,
the *candidate_functions list has no elements if there are no viable
functions, has more than one element if the call is ambiguous, and
has exactly one member if the call is valid.  *source_pos is the source
position of the reference.  If the best functions could not be selected
because there were error arguments in the matches,
*undecidable_because_of_error is returned TRUE and *candidate_functions
is set to NULL.
*/
{
  a_candidate_function_ptr candidates = *candidate_functions;
  a_candidate_function_ptr cfp, best_cfp, end_candidate_functions, cfp_next;
  unsigned long            number_in_best_match_set;
  an_arg_match_summary_ptr best_match_for_curr_arg, curr_arg;
  int                      cmp;
  a_boolean                overall_ambiguity = FALSE, any_error_arg = FALSE;
  a_boolean                some_require_std_conversion;
  a_boolean                some_do_not_require_std_conversion;
  a_boolean                any_function_templates;
  a_symbol_ptr             instance_symbol;

  db_enter(4, "select_best_candidate_functions");
  *undecidable_because_of_error = FALSE;
  /* See if there are any function templates. */
  any_function_templates = FALSE;
  for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
    if (cfp->is_function_template) {
      any_function_templates = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (any_function_templates) {
    /* There is at least one function template.  The resolution algorithm
       is therefore the one described in ARM 14.4:
         (1)  Look for an exact match on a normal function.  If there is
              exactly one, take it.  There shouldn't be more than one
              because two such functions shouldn't be allowed to be
              declared.
         (2)  Look for a function template that can match the arguments
              we have.  If there is exactly one, take it.  If there is
              more than one, the call is ambiguous.
         (3)  Remove the function templates from the candidate functions
              set and do the normal overload resolution.
    */
    /* Look for exact matches. */
    number_in_best_match_set = 0;
    for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
      cfp->in_best_match_set = FALSE;
      cfp->in_best_match_set_for_some_argument = FALSE;
      if (!cfp->is_function_template) {
        set_first_arg_match(cfp);
        /* Loop for each argument. */
        while (cfp->current_arg_match != NULL) {
          if (cfp->current_arg_match->match_level !=
                                               (an_arg_match_level)aml_exact) {
            /* This argument, and therefore this function, is not an exact
               match. */
            goto end_exact_test;
          }  /* if */
          cfp->in_best_match_set_for_some_argument = TRUE;
          advance_arg_match(cfp);
        }  /* while */
        /* This function is an exact match. */
        cfp->in_best_match_set = TRUE;
        number_in_best_match_set++;
end_exact_test:;
      }  /* if */
    }  /* for */
    if (number_in_best_match_set != 0) {
      /* There is an exact match. */
#if CHECKING
      if (number_in_best_match_set > 1) {
        /* It shouldn't be possible to get more than one exact match, because
           it shouldn't be possible to declare two functions with
           type signatures that close.  See overload_distinguishable. */
        internal_error("select_best_candidate_functions: >1 exact match");
      }  /* if */
#endif /* CHECKING */
      /* Take the exact match. */
      goto create_final_list;
    }  /* if */
    /* There is no exact match.  Try matching the function templates. */
    number_in_best_match_set = 0;
    for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
      cfp->in_best_match_set = FALSE;
      cfp->in_best_match_set_for_some_argument = FALSE;
      if (cfp->is_function_template) {
        if (function_template_matches_operand_list(cfp->function_symbol,
                                                   cfp->arg_operand_list,
                                                   source_pos,
                                                   &instance_symbol)) {
          /* The template function can be made to match the operands
             we have. */
          cfp->in_best_match_set = TRUE;
          cfp->function_symbol = instance_symbol;
          cfp->is_function_template = FALSE;
          number_in_best_match_set++;
        }  /* if */
      }  /* if */
    }  /* for */
    if (number_in_best_match_set == 1) {
      /* Exactly one function template matches. */
      goto create_final_list;
    } else if (number_in_best_match_set > 1) {
      /* More than one function template matches.  Ambiguity. */
      goto create_final_list;
    }  /* if */
    /* No function templates match, so take them out of the candidate
       functions set and free the entries. */
    *candidate_functions = end_candidate_functions = NULL;
    for (cfp = candidates; cfp != NULL; cfp = cfp_next) {
      cfp_next = cfp->next;
      cfp->next = NULL;
      if (cfp->is_function_template) {
        /* Free the candidate function entry for a function template.
           Note that this call only frees one entry because the "next"
           pointer has been cleared. */
        free_candidate_function_list(cfp);
      } else {
        /* Not a function template entry, so keep it on the list. */
        if (end_candidate_functions == NULL) {
          *candidate_functions = cfp;
        } else {
          end_candidate_functions->next = cfp;
        }  /* if */
        end_candidate_functions = cfp;
      }  /* if */
    }  /* for */
    candidates = *candidate_functions;
  }  /* if */
  /* At this point, there are no function template entries on the
     candidate functions list. */
  /* If there are no functions or there is exactly one function the
     list is already correct. */
  if (candidates != NULL && candidates->next != NULL) {
    /* The algorithm here is the one described in ARM 13.2: "The best-matching
       function is the intersection of sets of functions that best match on
       each argument.  Unless this intersection has exactly one member, the
       call is illegal.  The function thus selected must be a strictly better
       match for at least one argument than every other possible function
       (but not necessarily the same argument for each function).  Otherwise,
       the call is illegal." */
    /* We form the intersection of best-match sets by putting all functions
       in the best-match set and then doing an intersection after each argument
       best-match set is determined. */
    /* Put all functions in the best-match set, and set the current argument
       for each function to the first one. */
    number_in_best_match_set = 0;
    for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
      cfp->in_best_match_set = TRUE;
      cfp->in_best_match_set_for_some_argument = FALSE;
      number_in_best_match_set++;
      set_first_arg_match(cfp);
    }  /* for */
    /* Loop for each argument. */
    while (candidates->current_arg_match != NULL) {
      /* Find the best-match set for this argument. */
      best_match_for_curr_arg = NULL;
      /* Loop for each candidate function.  Look at the current argument
         under each function to find the best matches. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        curr_arg = cfp->current_arg_match;
        if (best_match_for_curr_arg == NULL) {
          /* First argument.  It's the best so far by definition. */
          cmp = 1;
        } else {
          /* Compare the current argument match level against the best
             match so far. */
          cmp = compare_arg_match_levels(curr_arg, best_match_for_curr_arg);
        }  /* if */
        if (cmp < 0) {
          /* The argument match being examined is not as good as the best
             match so far.  Ignore it. */
          cfp->prev_func_arg_match_with_same_match_level = NULL;
        } else {
          /* The argument match being examined is at least as good as the
             best match so far. */
          if (cmp > 0) {
            /* The argument match being examined is better than any seen
               so far.  Remember it as the best so far. */
            best_match_for_curr_arg = curr_arg;
          }  /* if */
          /* Remember that this argument match is a member of a best-match
             set, at least at the moment. */
          cfp->prev_func_arg_match_with_same_match_level =
                                                       best_match_for_curr_arg;
        }  /* if */
      }  /* for */
      /* Here, the current argument matches with 
         prev_func_arg_match_with_same_match_level == best_match_for_curr_arg
         are the best-match set for the current argument. */
      if (best_match_for_curr_arg->match_level == aml_error) {
        /* The best match for this argument was an error match.  Remember that
           there are some error matches. */
        any_error_arg = TRUE;
      }  /* if */
      /* Loop through the functions and form the intersection of the
         best-match set for this argument and the overall best-match
         set to date. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        if (cfp->prev_func_arg_match_with_same_match_level ==
                                                     best_match_for_curr_arg) {
          /* This function is in the best-match set for the current
             argument. */
          cfp->in_best_match_set_for_some_argument = TRUE;
        } else {
          /* This function is not in the best-match set for the current
             argument. */
          if (cfp->in_best_match_set) {
            /* Remove the function from the overall best-match set. */
            cfp->in_best_match_set = FALSE;
            number_in_best_match_set--;
            /* Note that we continue here even if number_in_best_match_set is
               zero so that in_best_match_set_for_some_argument will be
               set so that we will get the best error message. */
          }  /* if */
        }  /* if */
        /* Advance the current argument for this function to the next one,
           in preparation for the next iteration of the outer loop. */
        advance_arg_match(cfp);
      }  /* for */
      /* Loop to consider the next argument. */
    }  /* while */
    /* Here, the intersection of the best-match sets has been made, and
       the candidates with in_best_match_set TRUE are in that set. */
    if (number_in_best_match_set > 1) {
      /* If the set of functions being examined is the candidates to do
         a conversion, and if some of the candidates require a standard
         conversion after the user-defined conversion and others do not,
         select the candidates that do not require the user-defined
         conversion. */
      some_require_std_conversion = FALSE;
      some_do_not_require_std_conversion = FALSE;
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        if (cfp->std_conversion_after_conversion_function) {
          some_require_std_conversion = TRUE;
        } else {
          some_do_not_require_std_conversion = TRUE;
        }  /* if */
      }  /* for */
      if (some_require_std_conversion && some_do_not_require_std_conversion) {
        /* Eliminate the candidates that require a standard conversion after
           the user-defined conversion performed by the candidate function. */
        for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
          if (cfp->std_conversion_after_conversion_function) {
            cfp->in_best_match_set = FALSE;
            /* If there aren't any functions left, there's no point in
               continuing. */
            if (--number_in_best_match_set == 0) goto create_final_list;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    /* If there is only one function in the overall best-match set, perform
       the additional check that it must be strictly better than every other
       function on at least one argument. */
    if (number_in_best_match_set == 1) {
      /* Find the chosen function. */
      for (best_cfp = candidates;
           !best_cfp->in_best_match_set;
           best_cfp = best_cfp->next) {}
      if (!match_is_better_on_at_least_one_arg(best_cfp, candidates)) {
        /* The chosen function is not better than some other function.
           That means the call is ambiguous. */
        overall_ambiguity = TRUE;
        goto create_final_list;
      }  /* if */
    } else if (any_error_arg && number_in_best_match_set > 1) {
      /* There are some error matches, and we have more than one "best"
         function.  See if we can select one of those functions on the
         basis of the better-on-at-least-one-argument test. */
      for (cfp = candidates; cfp != NULL; cfp = cfp->next) {
        /* Only look at functions in the best-match set. */
        if (cfp->in_best_match_set) {
          if (!match_is_better_on_at_least_one_arg(cfp, candidates)) {
            /* This function couldn't have been chosen. */
            cfp->in_best_match_set = FALSE;
            /* If there aren't any functions left, there's no point in
               continuing. */
            if (--number_in_best_match_set == 0) goto create_final_list;
          }  /* if */
        }  /* if */
      }  /* for */
      /* Here, the best-match set is the functions that could have been
         selected  if the error argument had been something else.  If there
         is exactly one function left, use it.  Otherwise, return
         undecidable_because_of_error TRUE. */
      if (number_in_best_match_set > 1) {
        *undecidable_because_of_error = TRUE;
      }  /* if */
    }  /* if */
create_final_list:
    /* Make the final list.  If overall_ambiguity is TRUE, use the
       functions that were likely contenders, i.e., those that have
       in_best_match_set_for_some_argument TRUE. */
    /* If there are no functions left in the best-match set, there is
       overall ambiguity. */
    if (number_in_best_match_set == 0) overall_ambiguity = TRUE;
    /* Here, the best candidate functions have in_best_match_set TRUE.
       Make *candidate_functions a list of just those.  (Or, when
       overall_ambiguity is TRUE, make a list of the candidate functions
       with in_best_match_set_for_some_argument TRUE.)  Discard the
       other functions.  If *undecidable_because_of_error is TRUE,
       throw away all entries regardless of the in_best_match_set flag. */
    *candidate_functions = end_candidate_functions = NULL;
    for (cfp = candidates; cfp != NULL; cfp = cfp_next) {
      cfp_next = cfp->next;
      cfp->next = NULL;
      if (!*undecidable_because_of_error &&
          (overall_ambiguity ? cfp->in_best_match_set_for_some_argument :
                               cfp->in_best_match_set)) {
        /* Keep an entry that made the final list. */
        if (end_candidate_functions == NULL) {
          *candidate_functions = cfp;
        } else {
          end_candidate_functions->next = cfp;
        }  /* if */
        end_candidate_functions = cfp;
      } else {
        /* Free an entry that didn't make the final list. */
        /* Note that this call just frees one entry because we've already
           cleared the next pointer. */
        free_candidate_function_list(cfp);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* select_best_candidate_functions */


a_symbol_ptr select_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_arg_match_summary_ptr *arg_match_list)
/*
Determine which of the functions under overloaded_function_symbol should
be called given an argument list arg_operand_list.  The symbol may be an
overloaded function, a simple member or nonmember function, or a projection
symbol for one of those.  If have_selector is TRUE, *bound_function_selector
is a selector object.  (Note that, for constructor calls,
bound_function_selector can be NULL when have_selector is TRUE; we have a
selector, but it's not available.  That's okay for constructors, because
they cannot be const- or volatile-qualified, and the selector expression
is only needed for that discrimination.)  call_position is the source
position of the call.  If an error of some sort is detected, issue an
error at that position and return NULL.  err_none_applies is the error
code to use when no function applies, and err_ambiguous is the error code
to use when more than one function applies.  If there is no error,
an argument match list is returned in *arg_match_list (the caller must
free this) and the symbol selected is returned.  This routine is called
only in C++ mode.
*/
{
  a_candidate_function_ptr candidate_functions;
  a_symbol_ptr             function_symbol;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                undecidable_because_of_error;

  db_enter(4, "select_overloaded_function");
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  /* Evaluate all matches in the function set. */
  try_overloaded_function_match(overloaded_function_symbol,
                                arg_operand_list,
                                have_selector,
                                bound_function_selector,
                                /*selector_is_object_pointer=*/TRUE,
                                /*try_user_conversions=*/TRUE,
                                &candidate_functions,
                                &matched_except_for_missing_selector);
  /* The candidate_functions list now contains all the viable functions.
     Find the best one(s). */
  select_best_candidate_functions(&candidate_functions, call_position,
                                  &undecidable_because_of_error);
  function_symbol = NULL;
  *arg_match_list = NULL;
  if (undecidable_because_of_error) {
    /* There was some previous error, so do not put out an error message. */
  } else if (candidate_functions == NULL) {
    /* None of the functions applies. */
    if (matched_except_for_missing_selector) {
      /* At least one of the functions would have matched if we had had a
         selector expression, so issue a different error message. */
      /* A nonstatic member function is used someplace where there is no
         "this" available, e.g., outside of a member function. */
      pos_error(ec_member_ref_requires_object, call_position);
    } else {
      /* Normal case. */
      pos_sy_error(err_none_applies, call_position,
                   overloaded_function_symbol);
    }  /* if */
  } else if (candidate_functions->next != NULL) {
    /* More than one function applies and is a best match -- ambiguity. */
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
    pos_sy_start_error(err_ambiguous, call_position,
                       overloaded_function_symbol);
    diagnose_overload_ambiguity(candidate_functions, (an_opname_kind)onk_none);
  } else {
    /* Exactly one function applies and is best. */
    function_symbol = candidate_functions->function_symbol;
    *arg_match_list = candidate_functions->arg_matches;
    /* Prevent freeing of the arg_match_list when the candidate_functions
       list is freed. */
    candidate_functions->arg_matches = NULL;
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(function_symbol, "select_overloaded_function: selected ", 2); 
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* Free the candidate functions list. */
  free_candidate_function_list(candidate_functions);
  db_exit();
  return function_symbol;
}  /* select_overloaded_function */


void issue_warning_from_arg_match_summary(an_arg_match_summary_ptr amsp,
                                          a_source_position        *err_pos)
/*
Issue any suggested warning recorded in an argument match summary.
*err_pos is the source position to be used.
*/
{
  if (amsp->warning_suggested != ec_no_error) {
    pos_warning(amsp->warning_suggested, err_pos);
  }  /* if */
}  /* issue_warning_from_arg_match_summary */


static a_type_ptr operand_complete_object_type(an_operand *operand)
/*
Return the type of the complete object that contains the location indicated
by operand (an address), or NULL if no complete object can be determined.
NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.
*/
{
  a_type_ptr complete_object_type = NULL;

  if (is_constant_operand(operand)) {
    complete_object_type =
                          con_complete_object_type(&operand->variant.constant);
  } else if (is_expression_operand(operand)) {
    complete_object_type = 
                        node_complete_object_type(operand->variant.expression);
  }  /* if */
  return complete_object_type;
}  /* operand_complete_object_type */


static a_routine_ptr function_from_virtual_function_operand(
                                                  an_operand *function_operand)
/*
Given an operand for the address of a virtual function, extract and return a
pointer to the routine entry for the function.  This is possible in general
with virtual functions (and not all functions) because the function operand
for a virtual function is always just a simple address (you can't use an
expression to identify a virtual function; you can use a pointer to member
function, but that case does not come here).
*/
{
  a_constant_ptr con;

#if CHECKING
  if (!is_constant_operand(function_operand)) {
    internal_error("function_from_virtual_function_operand: bad operand");
  }  /* if */
#endif /* CHECKING */
  con = &function_operand->variant.constant;
#if CHECKING
  if (con->kind != (a_constant_repr_kind)ck_address ||
      con->variant.address.kind != (an_address_base_kind)abk_routine) {
    internal_error("function_from_virtual_function_operand: bad constant");
  }  /* if */
#endif /* CHECKING */
  return con->variant.address.variant.routine;
}  /* function_from_virtual_function_operand */


void bind_member_function_operand_to_selector(
                                           an_operand *function_operand,
                                           an_operand *bound_function_selector)
/*
Bind the operand for a function to an associated selector object.
*/
{
  a_routine_ptr function;

  function_operand->bound_function = TRUE;
  if (function_operand->virtual_function) {
    /* Virtual function call. */
    /* If the left operand is a complete object, we know the routine to
       call without going through the virtual function mechanism.
       We do a simple test, that the left operand type matches the
       complete object type.  Fancier schemes run into trouble when
       an overload set contains both virtual and nonvirtual functions:
         struct A {
           virtual int f();
                   int f(int);
         };
         struct B : public A {
                   int f();
                   int f(int);
         };
         B b;
         ((A *)&b)->f();  // B::f()
         ((A *)&b)->f(1); // A::f(int);
       One would have to have a way to adjust the "this" pointer back
       to the derived class to optimize the first case.
    */
    if (operand_complete_object_type(bound_function_selector) ==
        type_pointed_to(bound_function_selector->type)) {
      function_operand->virtual_function = FALSE;
      /* Set the IL referenced flag for the function.  It wasn't set
         when the call was thought to be virtual, since a virtual call
         does not necessarily end up at the indicated routine. */
      /* Extract the routine being called.  For virtual function calls, the
         operand identifying the function is always just a simple address
         of a function. */
      function = function_from_virtual_function_operand(function_operand);
      mark_routine_referenced(function, &function_operand->position);
    }  /* if */
  }  /* if */
}  /* bind_member_function_operand_to_selector */


void overloaded_function_catch_up(
                                 a_symbol_ptr       function_symbol,
                                 a_symbol_ptr       overloaded_function_symbol,
                                 a_boolean          is_qualified_name,
                                 a_source_position  *call_position,
                                 a_boolean          elided_reference,
                                 an_operand         *operand,
                                 a_boolean          *access_error_reported,
                                 an_expression_kind expression_kind)
/*
We've just determined which specific function within a set of overloaded
functions is being referenced, i.e., function_symbol is being called
within the set given by overloaded_function_symbol.  (function_symbol
may not be an overloaded function or a projection symbol.)  Do whatever
would have been done with the function along the way if we had known all
along which specific function was intended.  That is, "catch up" with the
processing that would have been done up to this point for a non-overloaded
function.  While this is usually used for overloaded functions, it is
also used for a few cases where the function is not overloaded but it
is not convenient to note that fact before scanning the arguments (e.g.,
operator overloading).  Therefore, while overloaded_function_symbol is
typically an sk_overloaded_function containing function_symbol, it may
be the same as function_symbol, or it may be a projection symbol
for one of those.  Generate an operand for a pointer to the specific
function in *operand.  call_position is used as the source position for
that operand.  is_qualified_name is TRUE if a qualified name was used to
name the function (that suppresses the virtual-ness of the function).
Access control and ambiguity checking are always done, even if the
overloaded_function_symbol is a non-overloaded function.  expression_kind
indicates the kind of the current expression.  operand can be NULL
if it is not necessary to generate the function designator operand.
elided_reference is TRUE if the routine was referenced in the program
but the reference is being elided in the intermediate language.
On return, *access_error_reported is TRUE if an access control checking
error was detected.
*/
{
  a_symbol_locator  function_symbol_locator;
  an_xref_entry_ptr xep;

#if CHECKING
  /* Overloaded functions and projection symbols are not allowed for
     function_symbol. */
  if (function_symbol->kind != (a_symbol_kind)sk_routine &&
      function_symbol->kind != (a_symbol_kind)sk_member_function) {
    internal_error("overloaded_function_catch_up: bad function_symbol");
  }  /* if */
#endif /* CHECKING */
  /* Check ambiguity and access. */
  if (fundamental_symbol_of(overloaded_function_symbol)->kind ==
                                       (a_symbol_kind)sk_overloaded_function) {
    /* Use a special routine for overloaded functions because (a) overloaded
       functions are considered always accessible when checked through the
       normal routine and (b) the projection symbol here may point to the
       overloaded function symbol rather than to the specific function
       symbol. */
    make_locator_for_symbol(function_symbol, &function_symbol_locator);
    function_symbol_locator.source_position = *call_position;
    overload_check_ambiguity_and_verify_access(&function_symbol_locator,
                                               overloaded_function_symbol);
  } else {
    /* Non-overloaded function; use the normal routine.
       overloaded_function_symbol is either the same as function_symbol
       or is a projection symbol for it. */
    make_locator_for_symbol(overloaded_function_symbol,
                            &function_symbol_locator);
    function_symbol_locator.source_position = *call_position;
    check_ambiguity_and_verify_access(&function_symbol_locator);
  }  /* if */
  *access_error_reported =
                         function_symbol_locator.access_control_error_reported;
  if (is_error_locator(function_symbol_locator)) {
    /* An error locator is returned for an ambiguous case. */
    if (operand != NULL) {
      make_error_operand(operand);
      operand->position = *call_position;
    }  /* if */
  } else {
    if (elided_reference ||
        expression_kind == (an_expression_kind)ek_not_evaluated) {
      /* The reference to the routine was elided or is not being evaluated.
         Mark the symbol as referenced, but not the IL entry. */
      mark_symbol_referenced(srk_reference, function_symbol, call_position);
    } else {
      /* The routine is actually called. */
      if (operand == NULL) {
        /* We don't want an operand, presumably because the function is
           being used in some unusual way and the caller will build the
           operand.  Mark the function as referenced.  Note that we are
           ignoring whether or not the function is virtual; we are assuming
           that the reference is to exactly that function. */
        mark_routine_referenced(function_symbol->variant.routine.ptr,
                                call_position);
      } else {
        /* Normal case: build an operand for the function. */
        /* Record that the function was referenced, for cross-reference (etc.)
           purposes. */
        xep = xref_entry(function_symbol, call_position, expression_kind);
        make_function_designator_operand(function_symbol, is_qualified_name,
                                         call_position, xep, operand);
        /* Convert the operand to a function pointer. */
        conv_function_designator_to_ptr_to_function(operand, expression_kind);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* overloaded_function_catch_up */


static a_type_ptr underlying_selector_class(an_operand *selector)
/*
Extract and return the class type underlying the given selector.  If the
class type cannot be determined, return NULL.  If there is an error in
the selector, return an error type.  The "underlying type" is the class
type of the object or pointer used to make the selection; any implicit
base class casts on top of that object are ignored in determining the
underlying type.
*/
{
  a_type_ptr       underlying_type = NULL;
  an_expr_node_ptr expr;
  a_constant_ptr   con;

  switch (selector->kind) {
    case ok_expression:
      /* Expression.  Drop implicit base class casts. */
      for (expr = selector->variant.expression;
           is_operation_node(expr) &&
             expr->variant.operation.kind ==
                                  (an_expr_operator_kind)eok_base_class_cast &&
             expr->variant.operation.compiler_generated;
           expr = expr->variant.operation.operands) {}
      if (is_pointer_type(expr->type)) {
        underlying_type = type_pointed_to(expr->type);
      }  /* if */
      break;
    case ok_constant:
      /* Constant.  See if it is the address of a variable. */
      con = &selector->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_address &&
          con->variant.address.kind == (an_address_base_kind)abk_variable &&
          !con->implicit_cast && con->variant.address.offset == 0) {
        underlying_type = con->variant.address.variant.variable->type;
      }  /* if */
      break;
    case ok_error:
      break;
#if CHECKING
    default:
      internal_error("underlying_selector_class: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  if (underlying_type != NULL) {
    underlying_type = skip_typerefs(underlying_type);
#if CHECKING
    if (!is_immediate_class_type(underlying_type) &&
        !is_error_type(underlying_type)) {
      internal_error("underlying_selector_class: bad underlying type");
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  return underlying_type;
}  /* underlying_selector_class */


static void f_check_protected_member_access_catch_up(
                                   a_symbol_ptr      sym,
                                   an_operand        *bound_function_selector,
                                   a_source_position *err_pos)
/*
This routine implements the access control check mandated by ARM 11.5, which
requires that a protected member be accessed only through a pointer or
object of a type to which we have member access.  This routine is used
specifically for the case where an overloaded function is a protected
member.  After overloaded_function_catch_up, this routine is called to
do the catch-up on the protected check.  sym points to the symbol for
the function being referenced.  *bound_function_selector is the selector
being used to access the function.  *err_pos is the source position for
an error.
*/
{
  a_type_ptr       class_type;
  a_symbol_locator locator;

  /* Get the underlying type from the selector. */
  class_type = underlying_selector_class(bound_function_selector);
  /* Do the access check. */
  make_locator_for_symbol(sym, &locator);
  locator.source_position = *err_pos;
  f_check_protected_member_access(&locator, class_type);
}  /* f_check_protected_member_access_catch_up */

/*
If sym is a protected member, do the access check of ARM 11.5.  sym
is being accessed through the selector "selector".  *err_pos is the
source position for an error.  This is being done after
overloaded_function_catch_up.
*/
#define check_protected_member_access_catch_up(sym, selector, err_pos)\
{ if (access_for_symbol(sym) == (an_access_specifier)as_protected) {  \
    f_check_protected_member_access_catch_up(sym, selector, err_pos); \
  }  /* if */                                                         \
}  /* check_protected_member_access_catch_up */


static void make_resolved_overloaded_function_operand(
                                 a_symbol_ptr       function_symbol,
                                 a_symbol_ptr       overloaded_function_symbol,
                                 a_boolean          have_selector,
                                 an_operand         *bound_function_selector,
                                 a_boolean          is_qualified_name,
                                 an_expression_kind expression_kind,
                                 a_source_position  *call_position,
                                 an_operand         *function_operand)
/*
Overload resolution has been done, and it has been decided that, of the
functions in overloaded_function_symbol (which may be a projection symbol
and/or just a simple function), function_symbol is the specific function to
be called (and not a projection symbol).  Create a function designator
operand for the function in *function_operand.  The function was named with
a qualified name if is_qualified_name is TRUE.  The reference has an
associated selector object if have_selector is TRUE; if that case,
bound_function_selector gives the object, and function_operand is bound to
that object.  call_position gives the source position of the call.
expression_kind indicates the kind of the current expression.
*/
{                                 
  a_boolean access_error_reported;

  /* Do whatever would have been done to the function had we known
     originally which specific function was intended. */
  overloaded_function_catch_up(function_symbol,
                               overloaded_function_symbol,
                               is_qualified_name,
                               call_position,
                               /*elided_reference=*/FALSE,
                               function_operand,
                               &access_error_reported,
                               expression_kind);
  /* Check whether or not a selector is needed. */
  if (routine_type_is_nonstatic_member_function(
                                       routine_symbol_type(function_symbol))) {
    /* The function needs a selector. */
#if CHECKING
    if (!have_selector) {
      internal_error(
                "make_resolved_overloaded_function_operand: missing selector");
    }  /* if */
#endif /* CHECKING */
    /* Do the ARM 11.5 access checking for the type of selector used
       to access a protected member. */
    if (!access_error_reported) {
      check_protected_member_access_catch_up(function_symbol,
                                             bound_function_selector,
                                             call_position);
    }  /* if */
    /* Bind the function to the selector. */
    bind_member_function_operand_to_selector(function_operand,
                                             bound_function_selector);
  } else {
    /* The routine does not need a selector. */
    if (have_selector) {
      /* Discard the selector provided. */
      discard_operand(bound_function_selector);
      have_selector = FALSE;
    }  /* if */
  }  /* if */
}  /* make_resolved_overloaded_function_operand */


static an_expr_node_ptr node_for_arg_of_overloaded_function_call(
                                      an_arg_operand_ptr       arg_operand,
                                      an_arg_match_summary_ptr arg_match,
                                      a_param_type_ptr         param,
                                      an_expression_kind       expression_kind)
/*
arg_operand represents an argument to an overloaded function call (including
operator cases); the call has now been resolved to a specific function.
arg_match indicates how well the actual argument matches the formal parameter,
which is described by param.  Cast the argument value to the proper type,
convert it to expression form, and return a pointer to the expression.
expression_kind indicates the current expression kind.  arg_operand can
be NULL to indicate that we've run out of actual arguments (default argument
values will be used).  param can be NULL to indicate that we've run out
of parameters (remaining arguments will be processed under an ellipsis).
*/
{
  an_expr_node_ptr arg;

  if (arg_operand == NULL) {
    /* Match uses a default argument value.  Get it from the parameter type
       entry. */
#if CHECKING
    if (param == NULL) {
      internal_error(
    "node_for_arg_of_overloaded_function_call: missing param for default arg");
    }  /* if */
#endif /* CHECKING */
    arg = param->default_arg_expr;
#if CHECKING
    if (arg == NULL) {
      internal_error(
              "node_for_arg_of_overloaded_function_call: missing default arg");
    }  /* if */
#endif /* CHECKING */
    arg = copy_expr_tree(arg, /*clone_temps=*/TRUE);
  } else {
    /* Actual argument is present (normal case). */
    /* Issue any warning about the conversion detected while evaluating the
       alternatives. */
    issue_warning_from_arg_match_summary(arg_match,
                                         &arg_operand->operand.position);
    /* Cast the argument to the right type. */
    prep_possible_ellipsis_argument_operand(&arg_operand->operand, param,
                                            ec_incompatible_param,
                                            expression_kind);
    arg = make_node_from_operand(&arg_operand->operand);
  }  /* if */
  return arg;
}  /* node_for_arg_of_overloaded_function_call */


void adjust_overloaded_function_call_arguments(
                             a_symbol_ptr             function_symbol,
                             a_boolean                have_selector,
                             an_operand               *bound_function_selector,
                             an_arg_operand_ptr       arg_operand_list,
                             an_arg_match_summary_ptr arg_match_list,
                             an_expression_kind       expression_kind,
                             an_expr_node_ptr         *arg_expr_list)
/*
Overload resolution has been done, and it has been decided that the function
identified by function_symbol is the specific function to be called for
the argument list given by arg_operand_list.  If have_selector is TRUE,
there is also a selector object, given by bound_function_selector (or,
as a special case, bound_function_selector can be NULL for a constructor
case; we have a selector, but we don't know what it is).  Adjust the
selector object and arguments to the proper types, issue any warnings
detected on those arguments during the overload resolution process, and
return a list of argument expressions in *arg_expr_list.  arg_match_list
gives the argument match summaries for the selector object and the
arguments.  expression_kind indicates the current expression kind.
arg_operand_list and arg_match_list are freed.  function_symbol can be
NULL to indicate that the overload resolution failed; in that case, this
routine does nothing except for freeing the lists.  This routine is used
for cases that look like calls (i.e., they have argument lists in parentheses);
it is not used for overloaded operator cases.
*/
{
  a_type_ptr               routine_type;
  a_boolean                old_style_function;
  an_arg_match_summary_ptr arg_match;
  an_arg_operand_ptr       arg_operand;
  an_expr_node_ptr         arg, prev_arg;
  a_param_type_ptr         param;

  /* If there was an error, skip the processing except for freeing the
     lists. */
  if (function_symbol != NULL) {
    routine_type = routine_symbol_type(function_symbol);
    old_style_function = !routine_type->variant.routine.extra_info->prototyped;
    arg_match = arg_match_list;
    if (have_selector) {
      /* Issue any warning about the "this" parameter detected while
         evaluating the alternatives. */
      if (bound_function_selector != NULL) {
        issue_warning_from_arg_match_summary(arg_match,
                                           &bound_function_selector->position);
      }  /* if */
      /* Note that no cast is done here.  It was done when the "." or "->"
         operator was processed (that still may leave a difference here
         involving type qualifiers, but it's not meaningful). */
      /* Move past the match entry for the selector. */
      arg_match = arg_match->next;
    }  /* if */
    prev_arg = NULL;
    /* Scan though the argument list. */
    for (arg_operand = arg_operand_list,
             param = routine_type->variant.routine.extra_info->param_type_list;
         arg_operand != NULL || param != NULL;) {
      arg = node_for_arg_of_overloaded_function_call(arg_operand, arg_match,
                                                     param,
                                                     expression_kind);
      /* If the function is an old-style unprototyped function (an anachronism;
         yes, they can participate in overloading), promote the argument
         value if necessary (e.g., short --> int). */
      if (old_style_function) {
        cast_node(&arg, default_argument_promotion(arg->type),
                  /*is_implicit_cast=*/TRUE, &arg_operand->operand.position);
      }  /* if */
      /* Add this argument to the end of the expression-form argument list
         being built up. */
      if (prev_arg == NULL) {
        *arg_expr_list = arg;
      } else {
        prev_arg->next = arg;
      }  /* if */
      prev_arg = arg;
      /* Advance to the next argument unless we've run out (additional
         arguments will come from default argument values). */
      if (arg_operand != NULL) {
        arg_operand = arg_operand->next;
        arg_match = arg_match->next;
      }  /* if */
      /* Advance to the next parameter unless we've run out (additional
         arguments will be processed under an ellipsis). */
      if (param != NULL) param = param->next;
    }  /* for */
  }  /* if */
  /* Free the argument match list. */
  free_arg_match_summary_list(arg_match_list);
  /* Free the argument list. */
  free_arg_operand_list(arg_operand_list);
}  /* adjust_overloaded_function_call_arguments */


a_symbol_ptr select_and_prepare_to_call_overloaded_function(
                           a_symbol_ptr             overloaded_function_symbol,
                           a_boolean                have_selector,
                           an_operand               *bound_function_selector,
                           an_arg_operand_ptr       arg_operand_list,
                           a_boolean                is_qualified_name,
                           an_expression_kind       expression_kind,
                           an_error_code            err_none_applies,
                           an_error_code            err_ambiguous,
                           a_source_position        *call_position,
                           an_operand               *function_operand,
                           an_expr_node_ptr         *arg_expr_list)
/*
Determine which of the functions under overloaded_function_symbol should
be called given an argument list arg_operand_list.  The symbol may be an
overloaded function, a simple member or nonmember function, or a projection
symbol for one of those.  If have_selector is TRUE, *bound_function_selector
is a selector object.  (Note that, for constructor calls,
bound_function_selector can be NULL when have_selector is TRUE; we have a
selector, but it's not available.  That's okay for constructors, because
they cannot be const- or volatile-qualified, and the selector expression
is only needed for that discrimination.)  is_qualified_name is TRUE if a
qualified name was used to name the function (that suppresses the
virtual-ness of the function).  expression_kind indicates the current
expression kind (non-constant).  arg_operand_list is freed by this routine.
call_position is the source position of the call.  If an error of some
sort is detected, issue an error at that position and return NULL.
err_none_applies is the error code to use when no function applies, and
err_ambiguous is the error code to use when more than one function
applies.  If there is no error, an operand for the function is built in
*function_operand, an expression-form argument list is built and returned
in *arg_expr_list (with the arguments cast to the proper types), and the
symbol selected is returned.  This routine is called only in C++ mode.
*/
{
  an_arg_match_summary_ptr arg_match_list;
  a_symbol_ptr             function_symbol;

  db_enter(4, "select_and_prepare_to_call_overloaded_function");
  /* Select the best function out of the overload set. */
  function_symbol = select_overloaded_function(overloaded_function_symbol,
                                               have_selector,
                                               bound_function_selector,
                                               arg_operand_list,
                                               err_none_applies,
                                               err_ambiguous,
                                               call_position,
                                               &arg_match_list);
  *arg_expr_list = NULL;
  if (function_symbol != NULL) {
    /* There was no error, i.e., a best function was chosen. */
    /* Do the things that would have been done to the symbol but weren't
       because the specific symbol was not known, and build an operand
       for the function. */
    make_resolved_overloaded_function_operand(function_symbol,
                                              overloaded_function_symbol,
                                              have_selector,
                                              bound_function_selector,
                                              is_qualified_name,
                                              expression_kind,
                                              call_position,
                                              function_operand);
  }  /* if */
  /* Build an expression-form argument list.  Convert the arguments on
     the argument list to the right types.  Free arg_operand_list
     and_arg_match_list (the call is done even when function_symbol
     is NULL so that the freeing will be done). */
  adjust_overloaded_function_call_arguments(function_symbol,
                                            have_selector,
                                            bound_function_selector,
                                            arg_operand_list,
                                            arg_match_list,
                                            expression_kind,
                                            arg_expr_list);
  db_exit();
  return function_symbol;
}  /* select_and_prepare_to_call_overloaded_function */


static void try_conversion_function_match(
                                an_operand               *source_operand,
                                a_type_ptr               dest_type,
                                a_builtin_type_kind_set  builtin_types_allowed,
                                a_candidate_function_ptr *candidate_functions)

/*
See if a class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL (a standard conversion can be
    done after the conversion function, if necessary), or
(b) a built-in type in the set given by builtin_types_allowed, if
    dest_type is NULL.

If a conversion function to do that conversion exists, evaluate how
well it matches the arguments and add it to the candidate_functions list.
If a standard conversion is needed after the conversion function,
std_conversion_after_conversion_function will be set in the candidate
function entry.  This routine is only used in C++ mode.
*/
{
  a_symbol_ptr                conversion_symbol, base_conversion_symbol;
  a_conversion_list_entry_ptr clep;
  a_type_ptr                  source_type, conv_routine_type, return_type;
  an_arg_match_summary        this_match;
  an_arg_match_summary_ptr    this_match_ptr;
  an_error_code               warning_suggested;
  a_boolean                   compatible, std_conversion_needed;

  db_enter(4, "try_conversion_function_match");
  /* This routine is similar to try_overloaded_function_match. */
  source_type = source_operand->type;
  /* Look at all the conversion functions for the source class. */
  for (clep = symbol_supplement_for_class(source_type)->conversion_list;
       clep != NULL;
       clep = clep->next) {
    conversion_symbol = clep->symbol;
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(conversion_symbol,
                "try_conversion_function_match: considering ", 2); 
    }  /* if */
#endif /* DEBUG */
    base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
    conv_routine_type = routine_symbol_type(base_conversion_symbol);
    /* Is the type returned by this routine a type we want? */
    compatible = FALSE;
    std_conversion_needed = FALSE;
    return_type = conv_routine_type->variant.routine.return_type;
    /* Drop type qualifiers for the normal case, when the return value
       is an rvalue. */
    return_type = skip_typerefs(return_type);
    /* If the conversion function returns a reference type, drop the 
       reference. */
    if (is_reference_type(return_type)) {
      return_type = type_pointed_to(return_type);
      /* In this case, the return value is an lvalue, and type qualifiers
         are not dropped. */
    }  /* if */
    if (dest_type != NULL) {
      /* We're looking for a specific type. */
      if (types_are_compatible(skip_typerefs(dest_type),
                               skip_typerefs(return_type))) {
        /* This conversion function returns the type we want, ignoring
           type qualifiers.  See if the type qualifiers are okay. */
        if (type_qualifiers_match(dest_type, return_type) ||
            !any_qualifier_missing(dest_type, return_type)) {
          /* The type qualifiers on the desired type are the same as or a
             proper superset of what the conversion function returns.  Okay. */
          compatible = TRUE;
        }  /* if */
      } else if (impl_conversion_possible(return_type,
                                          /*source_is_constant=*/FALSE,
                                          (a_constant_ptr)NULL, dest_type,
                                          /*suppress_extensions=*/TRUE,
                                          ec_no_error, &warning_suggested)) {
        /* This conversion function returns a type that can be converted
           via a standard conversion to the type we want. */
        compatible = TRUE;
        std_conversion_needed = TRUE;
      }  /* if */
    } else {
      /* We're looking for a built-in type described in general terms. */
#if 0
      /* Different test for enum? */
#endif
      if (((builtin_types_allowed & BTK_INTEGRAL) != 0 &&
                                              is_integral_type(return_type)) ||
          ((builtin_types_allowed & BTK_FLOATING) != 0 &&
                                              is_floating_type(return_type)) ||
          ((builtin_types_allowed & BTK_POINTER) != 0 &&
                                              is_pointer_type(return_type)) ||
          ((builtin_types_allowed & BTK_PTR_TO_MEMBER) != 0 &&
                                         is_ptr_to_member_type(return_type))) {
        /* This conversion function returns an acceptable built-in type. */
        compatible = TRUE;
      }  /* if */
    }  /* if */
    if (compatible) {
      /* This conversion function meets the requirements for result type.
          However, we must also see whether or not it can be called for this
          argument (i.e., are the type qualifiers okay), and how good the
          match is. */
      selector_match_with_this_param(source_operand,
                                     /*selector_is_object_pointer=*/FALSE,
                                     /*conversion_function_case=*/TRUE,
                                     base_conversion_symbol->
                                                           variant.routine.ptr,
                                     conv_routine_type,
                                     &this_match);
      /* Ignore this function if it cannot be called for this argument. */
      if (this_match.match_level == aml_none) goto next_function;
      /* The routine is viable. */
      /* Add the conversion function to the candidate functions list. */
      this_match_ptr = alloc_arg_match_summary();
      *this_match_ptr = this_match;
      add_function_to_candidate_functions_list(conversion_symbol,
                                               this_match_ptr,
                                               candidate_functions);
      /* If a standard conversion was needed, remember that in the
         candidate function entry.  A difference of a standard conversion
         can be used to distinguish between different user-defined
         conversions. */
      if (std_conversion_needed) {
        (*candidate_functions)->std_conversion_after_conversion_function= TRUE;
      }  /* if */
    }  /* if */
next_function:;
  }  /* for */
  db_exit();
}  /* try_conversion_function_match */


static a_symbol_ptr find_conversion_function(a_type_ptr class_type,
                                             a_type_ptr dest_type)
/*
See if there is a conversion function that converts class_type (a class type)
to dest_type.  If so, return a pointer to the symbol.  If not, return NULL.

Note:  This looks like a general-purpose routine, but it's not.  It won't
deal with differences like

  operator int() const;
  operator int();

For which one needs to know the exact type of the operand to be converted.
This routine is useful as a quick way of seeing whether or not a particular
type appears on the list of conversion functions.
*/
{
  a_symbol_ptr                conversion_symbol;
  a_conversion_list_entry_ptr clep;
  a_type_ptr                  conv_routine_type, return_type;

#if CHECKING
  if (!is_class_struct_union_type(class_type)) {
    internal_error("find_conversion_function: source not class type");
  }  /* if */
#endif /* CHECKING */
  /* Examine each conversion function from the source class. */
  for (clep = symbol_supplement_for_class(class_type)->conversion_list;
       clep != NULL;
       clep = clep->next) {
    conversion_symbol = clep->symbol;
    reduce_projection_symbol_to_fundamental_symbol(conversion_symbol);
    conv_routine_type = routine_symbol_type(conversion_symbol);
    return_type = conv_routine_type->variant.routine.return_type;
    if (identical_types(dest_type, return_type)) {
      /* Found the required function. */
      goto end_of_search;
    }  /* if */
  }  /* for */
  /* No matching function was found. */
  conversion_symbol = NULL;
end_of_search:
  return conversion_symbol;
}  /* find_conversion_function */


void try_to_convert_class_operand_to_builtin_type(
                                 an_operand              *operand,
                                 a_builtin_type_kind_set builtin_types_allowed,
                                 a_boolean               result_may_be_lvalue,
                                 an_expression_kind      expression_kind,
                                 a_boolean               *processed)
/*
If *operand has a class type, see if it can be converted (via a conversion
function) to a built-in type of the set allowed by builtin_types_allowed.
If so, convert it.  Unless result_may_be_lvalue is TRUE, force the result
to be an rvalue.  Issue an error and set *processed to TRUE if the conversion
is ambiguous.  expression_kind indicates the current expression kind.
*/
{
  a_routine_ptr            conversion_routine;
  a_boolean                ambiguous, std_conversion_needed;
  a_candidate_function_ptr ambiguity_list;

  /* Only look at this operand if it has a class type. */
  if (is_class_struct_union_type(operand->type)) {
    /* See if the class type can be converted to an acceptable built-in
       type. */
    if (conversion_from_class_possible(operand, (a_type_ptr)NULL,
                                       builtin_types_allowed,
                                       &conversion_routine,
                                       &std_conversion_needed,
                                       &ambiguous, &ambiguity_list)) {
      /* The conversion is possible -- do it. */
      user_convert_operand(operand, /*dest_type=*/(a_type_ptr)NULL,
                           result_may_be_lvalue,
                           conversion_routine, /*class_bitwise_copy=*/FALSE,
                           expression_kind);
      *processed = TRUE;
    } else if (ambiguous) {
      /* There is more than one possible conversion to a built-in type. */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        pos_ty_start_error(ec_ambiguous_conversion_to_builtin,
                           &operand->position, operand->type);
        diagnose_overload_ambiguity(ambiguity_list, (an_opname_kind)onk_none);
        free_candidate_function_list(ambiguity_list);
      }  /* if */
      conv_to_error_operand(operand);
      *processed = TRUE;
    }  /* if */
  }  /* if */
}  /* try_to_convert_class_operand_to_builtin_type */


static char *operand_type_pattern_for_operator(an_opname_kind kind,
                                               a_boolean      unary_operator)
/*
Return a string describing the argument type patterns permitted for the
indicated operator (the unary version if unary_operator is TRUE).
The argument string contains one or more possible patterns separated
by semicolons, e.g., "AA;PI;IP"; each pattern has one letter (for
unary operators) or two letters (for binary operators) giving the type
code for the associated operand:
  P  Pointer
  I  Integral
  A  Arithmetic
  S  Scalar (pointer or arithmetic)
  C  Corresponding pointer, when two pointer operands must match in type
Note that "pointer" and "arithmetic" are not combined into a separate
code for "scalar" on purpose, because processing for pointer cases is
quite different than for non-pointer cases.
The first character of the overall string is "L" if the operator requires
an lvalue as its first operand, e.g., "LAA;PI;IP".
*/
{
  char *operand_type_pattern;

  if (unary_operator) {
    switch (kind) {
      case onk_plus:
      case onk_minus:
        /* Unary "+" and "-" take an arithmetic operand. */
        operand_type_pattern = "A";
        break;
      case onk_not:
        /* "!" takes an arithmetic or pointer operand. */
        operand_type_pattern = "S";
        break;
      case onk_compl:
        /* "~" takes an integral operand. */
        operand_type_pattern = "I";
        break;
      case onk_star:
        /* "*" takes a pointer operand. */
        operand_type_pattern = "P";
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (prefix) take an arithmetic or pointer lvalue.
           See below for postfix (which shows up as a two-operand operator). */
        operand_type_pattern = "LS";
        break;
#if CHECKING
      default:
        internal_error("operand_type_pattern_for_operator: bad unary op");
#endif /* CHECKING */
    }  /* switch */
  } else {
    /* Binary operator. */
    switch (kind) {
      case onk_star:
      case onk_divide:
        /* "*" and "/" take arithmetic operands. */
        operand_type_pattern = "AA";
        break;
      case onk_remainder:
      case onk_shift_left:
      case onk_shift_right:
      case onk_ampersand:
      case onk_or:
      case onk_excl_or:
        /* "%", "<<", ">>", "&", "|", and "^" take integral operands. */
        operand_type_pattern = "II";
        break;
      case onk_plus:
        /* "+" takes arith+arith, pointer+int, or int+pointer. */
        operand_type_pattern = "AA;PI;IP";
        break;
      case onk_minus:
        /* "-" takes arith-arith, pointer-int, or pointer-pointer. */
        operand_type_pattern = "AA;PI;CC";
        break;
      case onk_lt:
      case onk_le:
      case onk_gt:
      case onk_ge:
      case onk_eq:
      case onk_ne:
        /* Relational operators take arithmetic or pointer operands. */
        operand_type_pattern = "AA;CC";
        break;
      case onk_and_and:
      case onk_or_or:
        /* "&&" and "||" take arithmetic or pointer operands, but they can
           be mixed. */
        operand_type_pattern = "SS";
        break;
      case onk_times_assign:
      case onk_divide_assign:
        /* "*=" and "/=" take arithmetic operands, the first an lvalue. */
        operand_type_pattern = "LAA";
        break;
      case onk_remainder_assign:
      case onk_shift_left_assign:
      case onk_shift_right_assign:
      case onk_and_assign:
      case onk_or_assign:
      case onk_excl_or_assign:
        /* "%=", "<<=", ">>=", "&=", "|=", and "^=" take integral operands,
           the first an lvalue. */
        operand_type_pattern = "LII";
        break;
      case onk_plus_assign:
        /* "+=" takes arith+arith or pointer+int, the first an lvalue. */
        operand_type_pattern = "LAA;PI";
        break;
      case onk_minus_assign:
        /* "-=" takes arith-arith or pointer-int, the first an lvalue. */
        operand_type_pattern = "LAA;PI";
        break;
      case onk_subscript:
        /* "[]" takes pointer[int] or int[pointer]. */
        operand_type_pattern = "PI;IP";
        break;
      case onk_plus_plus:
      case onk_minus_minus:
        /* "++" and "--" (postfix, which show up as two-operand operators)
           take an arithmetic or pointer lvalue.  A second implied
           operand is integer. */
        operand_type_pattern = "LSI";
        break;
      case onk_question:
        /* "?" (which shows up here as a two-operand operator) takes
           two operands (really the second and third) of arithmetic or
           pointer type (the void and class cases are handled outside of
           this routine). */
        operand_type_pattern = "AA;CC";
        break;
#if CHECKING
      default:
        internal_error("operand_type_pattern_for_operator: bad binary op");
#endif /* CHECKING */
    }  /* switch */
  } /* if */
  return operand_type_pattern;
}  /* operand_type_pattern_for_operator */


static a_builtin_type_kind_set builtin_type_set_for_type_code(char type_code)
/*
Build and return the built-in type kind set that corresponds to the indicated
type_code.
*/
{
  a_builtin_type_kind_set builtin_types_allowed = BTK_NONE;

  if (type_code == INTEGRAL_TYPE_CODE ||
      type_code == ARITH_TYPE_CODE ||
      type_code == SCALAR_TYPE_CODE) {
    builtin_types_allowed |= BTK_INTEGRAL;
  }  /* if */
  if (type_code == ARITH_TYPE_CODE ||
      type_code == SCALAR_TYPE_CODE) {
    builtin_types_allowed |= BTK_FLOATING;
  }  /* if */
  if (type_code == POINTER_TYPE_CODE ||
      type_code == SCALAR_TYPE_CODE) {
    builtin_types_allowed |= BTK_POINTER;
  }  /* if */
  /* There are no built-in operators that take pointers to members
     at the moment, so BTK_PTR_TO_MEMBER is not tested. */
  return builtin_types_allowed;
}  /* builtin_type_set_for_type_code */


static void try_builtin_operands_match(
                       char                     *operand_type_pattern,
                       an_arg_operand_ptr       arg_operand_list,
                       a_candidate_function_ptr *candidate_functions,
                       char                     *pointer_type_pattern_position,
                       a_type_ptr               pointer_type)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.
This is used for the case where the pattern string contains no corresponding
pointer types, with pointer_type_pattern_position == pointer_type == NULL,
and with those set non-NULL for pattern strings containing corresponding
pointer types (they indicate the target pointer type to be used and, to
allow a speed optimization, the pattern position which suggested that
pointer type).
*/
{
  a_boolean                okay;
  char                     type_code;
  char                     *type_pattern_position;
  an_arg_operand_ptr       arg_operand;
  an_arg_match_summary_ptr arg_match, arg_match_list, end_arg_match_list;
  a_type_ptr               operand_type;
  an_error_code            warning_suggested;
  a_routine_ptr            conversion_routine;
  a_boolean                ambiguous, std_conversion_needed;
  a_boolean                pointer_normalization_needed;
#if DEBUG
  unsigned long            narg;
#endif /* DEBUG */

  /* This routine is similar to try_overloaded_function_match (but it only
     looks at one type pattern per call). */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "try_builtin_operands_match: considering %s\n",
                     operand_type_pattern);
  }  /* if */
  narg = 0;
#endif /* DEBUG */
  arg_match_list = end_arg_match_list = NULL;
  okay = TRUE;
  /* Go through the operands and determine the match level on each operand. */
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, arg_operand = arg_operand->next) {
#if CHECKING
    if (*type_pattern_position == ';' ||
        *type_pattern_position == '\0') {
      /* The operand list and the type pattern do not have the same number
         of elements. */
      internal_error("try_builtin_operands_match: ran off pattern");
    }  /* if */
#endif /* CHECKING */
#if DEBUG
    narg++;
    if (debug_level >= 4) {
      fprintf(f_debug, "try_builtin_operands_match: operand %lu\n", narg);
    }  /* if */
#endif /* DEBUG */
    /* Get the operand type. */
    operand_type = arg_operand->operand.type;
    /* Add an entry to the end of the arg_match_list to record whether or
       not this argument matches. */
    arg_match = alloc_arg_match_summary();
    if (arg_match_list == NULL) {
      arg_match_list = arg_match;
    } else {
      end_arg_match_list->next = arg_match;
    }  /* if */
    end_arg_match_list = arg_match;
    /* See if the operand type matches the type code.  Any match that
       does not require a conversion function is considered a standard
       conversion match, even if an exact match is possible.
       This is different than for function argument matching.  The ARM
       is not very explicit about this.  13.2, page 314: "It follows that
       the binary operands for built-in types do not obey these ambiguity
       rules ... The problem with the (C and C++) rules for conversion of
       arithmetic types is that a value of any arithmetic type can be
       implicitly converted into any other arithmetic type." */
    type_code = *type_pattern_position;
    if (type_code != CORRESP_POINTER_TYPE_CODE) {
      /* Arithmetic or non-specific pointer type required. */
      if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           an appropriate built-in type. */
        if (conversion_from_class_possible(&arg_operand->operand,
                                           (a_type_ptr)NULL,
                                     builtin_type_set_for_type_code(type_code),
                                           &conversion_routine,
                                           &std_conversion_needed,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done. */
          arg_match->match_level = aml_user_conversion;
        }  /* if */
      } else {
        /* A non-class operand.  See if it can be converted to the required
           type.  All arithmetic types can be inter-converted, but
           pointer and arithmetic types cannot be. */
        /* Note that we do not try 0 --> pointer, because in this case it
           would require just making up a pointer type out of nowhere --
           there's no other operand that provides guidance on which pointer
           type is required. */
#if 0
        /* Enum? */
#endif
        if (((type_code == INTEGRAL_TYPE_CODE ||
              type_code == ARITH_TYPE_CODE ||
              type_code == SCALAR_TYPE_CODE) &&
                                           is_arithmetic_type(operand_type)) ||
            ((type_code == POINTER_TYPE_CODE ||
              type_code == SCALAR_TYPE_CODE) &&
                                              is_pointer_type(operand_type))) {
          /* As noted above, any match here is considered a standard
             conversion. */
          arg_match->match_level = aml_std_conversion;
        }  /* if */
      }  /* if */
    } else {
      /* A specific pointer type is required.  Check that the operand
         can be converted to the pointer type passed in. */
      if (is_class_struct_union_type(operand_type)) {
        /* The operand has a class type, so see if it can be converted to
           the pointer type. */
        /* If this operand is the one that suggested this pointer type,
           we already know it is compatible.  This is a speed optimization. */
        conversion_routine = NULL;
        std_conversion_needed = FALSE;
        if (pointer_type_pattern_position == type_pattern_position ||
            conversion_from_class_possible(&arg_operand->operand, pointer_type,
                                           (a_builtin_type_kind_set)BTK_NONE,
                                           &conversion_routine,
                                           &std_conversion_needed,
                                           &ambiguous,
                                           (a_candidate_function_ptr *)NULL) ||
            ambiguous) {
          /* The conversion can be done with a conversion function. */
          set_arg_summary_for_user_conversion(arg_match, pointer_type,
                                              conversion_routine,
                                              std_conversion_needed);
        }  /* if */
      } else {
        /* A non-class operand. */
        /* If this operand is the one that suggested this pointer type,
           we already know it is compatible.  This is a speed optimization. */
        if (pointer_type_pattern_position == type_pattern_position ||
            impl_pointer_conversion(operand_type,
                                    is_constant_operand(&arg_operand->operand),
                                    &arg_operand->operand.variant.constant,
                                    pointer_type,
                                    /*check_as_operands_not_conversion=*/TRUE,
                                    &pointer_normalization_needed,
                                    /*suppress_extensions=*/TRUE,
                                    ec_incompatible_operands,
                                    &warning_suggested)) {
          /* The conversion can be done. */
          /* As noted above, any match here is considered a standard
             conversion. */
          arg_match->match_level = aml_std_conversion;
        }  /* if */
      }  /* if */
    }  /* if */
    /* If this operand cannot be made to match, give up on this version of
       the built-in operator. */
    if (arg_match->match_level == aml_none) {
      okay = FALSE;
      break;
    }  /* if */
  }  /* for */
  if (okay) {
    /* The built-in operator can be used.  Add it to the list of
       candidate functions. */
    add_builtin_operator_to_candidate_functions_list(operand_type_pattern,
                                                     pointer_type,
                                                     arg_match_list,
                                                     candidate_functions);
  } else {
    /* The built-in operator cannot be used. */
    free_arg_match_summary_list(arg_match_list);
  }  /* if */
}  /* try_builtin_operands_match */


static a_boolean pointer_type_previously_handled(
                                   a_type_ptr pointer_type,
                                   a_type_ptr class_type,
                                   a_type_ptr previous_class_type_considered,
                                   a_type_ptr previous_pointer_type_considered)
/*
Helper routine for try_pointer_builtin_operands_match.  Return TRUE if
pointer_type has already been tried as a target pointer type.
class_type, if non-NULL, indicates the current operand class type.
previous_class_type_considered, if non-NULL, indicates the class type of
a previous operand converted to pointer; previous_pointer_type_considered,
if non-NULL, indicates the pointer type of a previous non-class operand.
*/
{
  a_boolean previously_handled;

  previously_handled = FALSE;
  if (previous_pointer_type_considered == pointer_type) {
    /* This type was the type of a previous non-class operand;
       it's already been considered. */
    previously_handled = TRUE;
  } else if (previous_class_type_considered != NULL) {
    /* Some pointer types were tried on a previous class operand, so
       check to see if the pointer type we're considering was
       already processed on the previous operand.  It was if the
       underlying class types of the operands are the same or if
       the previous class has a conversion function that converts
       to the pointer type we're considering. */
    if (class_type == previous_class_type_considered ||
        find_conversion_function(previous_class_type_considered,
                                 pointer_type) != NULL) {
      /* This pointer type was tried when the first operand was
         processed, so do not try it again (if we did, it would
        look like an ambiguity). */
      previously_handled = TRUE;
    }  /* if */
  }  /* if */
  return previously_handled;
}  /* pointer_type_previously_handled */



static void try_pointer_builtin_operands_match(
                                char                     *operand_type_pattern,
                                an_arg_operand_ptr       arg_operand_list,
                                a_candidate_function_ptr *candidate_functions)
/*
Subroutine for try_conversions_for_builtin_operator.  Check to see how
well the operand values given by arg_operand_list match the operand type
pattern string given by operand_type_pattern.  If they match, add
the built-in operator to the candidate_functions list.  This is used for the
case where the pattern string contains "CC", meaning two pointer operands
that must have the same type.
*/
{
  an_arg_operand_ptr          arg_operand;
  a_type_ptr                  pointer_type, operand_type, class_type;
  char                        *type_pattern_position;
  a_symbol_ptr                conversion_symbol, base_conversion_symbol;
  a_conversion_list_entry_ptr clep;
  a_type_ptr                  conv_routine_type, return_type;
  a_type_ptr                  previous_class_type_considered;
  a_type_ptr                  previous_pointer_type_considered;
  a_boolean                   any_ptr_conversion_function_this_operand;

  db_enter(4, "try_pointer_builtin_operands_match");
  /* The reason the pointer case is more complicated than other cases is
     that it is not sufficient to ask "can this class-type operand be
     converted to any pointer type?" -- we must ask whether both of the
     pointer operands can be converted to a specific pointer type or
     something compatible with it. */
  /* Loop through the two operands. */
  previous_class_type_considered = NULL;
  previous_pointer_type_considered = NULL;
  for (type_pattern_position = operand_type_pattern,
         arg_operand = arg_operand_list;
       arg_operand != NULL;
       type_pattern_position++, arg_operand = arg_operand->next) {
    operand_type = arg_operand->operand.type;
    if (is_class_struct_union_type(operand_type)) {
      /* This operand takes a pointer type and the operand value has a class
         type.  Look for conversion functions that convert the class type
         to any pointer type. */
      class_type = skip_typerefs(operand_type);
      any_ptr_conversion_function_this_operand = FALSE;
      /* Look at all the conversion functions for the source class. */
      for (clep = symbol_supplement_for_class(class_type)->conversion_list;
           clep != NULL;
           clep = clep->next) {
        conversion_symbol = clep->symbol;
        base_conversion_symbol = fundamental_symbol_of(conversion_symbol);
        conv_routine_type = routine_symbol_type(base_conversion_symbol);
        return_type = conv_routine_type->variant.routine.return_type;
        if (is_pointer_type(return_type)) {
          /* We've found a conversion function to a pointer type.  Make
             sure it's not a type we've already checked while examining a
             previous operand.  If it is, ignore it. */
          pointer_type = return_type;
          if (!pointer_type_previously_handled(
                                           pointer_type, class_type,
                                           previous_class_type_considered,
                                           previous_pointer_type_considered)) {
            /* Try matching the operands, with the chosen pointer type
               as the target type for operands that must be pointers. */
            any_ptr_conversion_function_this_operand = TRUE;
            try_builtin_operands_match(operand_type_pattern,
                                       arg_operand_list,
                                       candidate_functions,
                                       type_pattern_position,
                                       pointer_type);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Remember if we've processed any class types with pointer conversion
         functions. */
      if (any_ptr_conversion_function_this_operand) {
        previous_class_type_considered = class_type;
      }  /* if */
    } else {
      /* The operand requires a pointer and the value supplied does not
         have a class type.  If it has a pointer type try that type as the
         target type. */
      if (is_pointer_type(operand_type)) {
        pointer_type = operand_type;
        /* If the type has been previously handled, ignore it. */
        if (!pointer_type_previously_handled(
                                           pointer_type, (a_type_ptr)NULL,
                                           previous_class_type_considered,
                                           previous_pointer_type_considered)) {
          previous_pointer_type_considered = pointer_type;
          /* Try matching the operands, with the chosen pointer type
             as the target type for operands that must be pointers. */
          try_builtin_operands_match(operand_type_pattern,
                                     arg_operand_list,
                                     candidate_functions,
                                     type_pattern_position,
                                     pointer_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* try_pointer_builtin_operands_match */


static void try_conversions_for_builtin_operator(
                                 an_opname_kind           kind,
                                 a_boolean                unary_operator,
                                 an_arg_operand_ptr       arg_operand_list,
                                 a_candidate_function_ptr *candidate_functions)
/*
See if conversion functions can be used to convert the operands of an
operator to built-in types that would be suitable for the built-in
version of the operator.  The operator is specified by kind and unary_operator.
The operands are specified by arg_operand_list.  If the built-in operator
can be used, it is added to the candidate_functions list.
*/
{
  char       *operand_type_pattern;
  an_operand *first_operand;

  db_enter(4, "try_conversions_for_builtin_operator");
  /* Determine the argument pattern for the operator, and whether or not
     the first operand must be an lvalue.  The pattern begins with "L"
     if the operator requires an lvalue as its first operand.  Following that
     are one or more semicolon-separated argument patterns, each one consisting
     of one letter (for unary operators) or two letters (for binary operators)
     indicating the allowed argument types.  As a concrete example, the
     pattern for "-=" is "LAA;PI;CC", indicating that the operator requires
     an lvalue and takes operands of types arith-arith, pointer-int, or
     corresponding pointer-pointer. */
  operand_type_pattern = operand_type_pattern_for_operator(kind,
                                                           unary_operator);
  if (*operand_type_pattern == 'L') {
    /* The operator requires an lvalue as its first operand.  Check that.
       If the operand has a class type it might be convertible to an lvalue
       via a conversion function returning a reference.  Note that we need not
       check for a modifiable lvalue, because the processing for the
       built-in operator will do that if necessary.  In fact, the
       processing for the built-in operator will do full checking, so
       the checking here is just looking for obvious mismatches. */
    first_operand = &arg_operand_list->operand;
    if (!is_an_lvalue(first_operand) &&
        !is_class_struct_union_type(first_operand->type)) {
      /* The first operand is not an lvalue so the built-in operator cannot
         be used.  Give up. */
      goto end_of_check;
    }  /* if */
    /* Advance past the "L". */
    operand_type_pattern++;
  }  /* if */
  /* Check the operands to see if they can be converted to the proper
     types. */
  /* Loop for each ";"-separated pattern in the string. */
  do {
    if (operand_type_pattern[0] == CORRESP_POINTER_TYPE_CODE) {
      /* Both operands must have the same pointer type.  This case is more
         complicated because it involves enumerating the pointer types that
         can be generated by the applicable conversion functions. */
      try_pointer_builtin_operands_match(operand_type_pattern,
                                         arg_operand_list,
                                         candidate_functions);
    } else {
      /* There are no pointer types in the argument pattern. */
      try_builtin_operands_match(operand_type_pattern,
                                 arg_operand_list,
                                 candidate_functions,
                                 (char *)NULL, (a_type_ptr)NULL);
    }  /* if */
    /* Advance to the next pattern or stop the loop at the end of the
       string. */
    operand_type_pattern++;
    if (!unary_operator) operand_type_pattern++;
  } while (*operand_type_pattern++ == ';');
end_of_check:;
  db_exit();
}  /* try_conversions_for_builtin_operator */


static void adjust_operand_for_built_in_operator(
                                   an_operand               *operand,
                                   a_candidate_function_ptr candidate_function,
                                   int                      operand_num,
                                   an_expression_kind       expression_kind)
/*
operand is the operand_num-th operand of a built-in operator described by
candidate function.  Adjust the operand type to match the type requirement.
expression_kind indicates the current expression kind.
*/
{
  char       type_code;
  a_boolean  processed;
  a_type_ptr pointer_type;

  if (!is_class_struct_union_type(operand->type)) {
    /* Non-class operands need not be adjusted here; the built-in operator
       processing will do it. */
  } else {
    /* Get the type code for this operand (see
       operand_type_pattern_for_operator). */
    type_code = candidate_function->operand_type_pattern[operand_num-1];
    if (type_code != CORRESP_POINTER_TYPE_CODE) {
      /* Non-pointer cases are easy.  There must be a conversion function
         from the class type to an appropriate built-in type.  There must
         be one, or we wouldn't have matched.  There cannot be more than
         one, or we would have detected an error and not gotten here. */
      processed = FALSE;
      try_to_convert_class_operand_to_builtin_type(operand,
                                     builtin_type_set_for_type_code(type_code),
                                     /*result_may_be_lvalue=*/TRUE,
                                     expression_kind,
                                     &processed);
#if CHECKING
      if (!processed) {
        internal_error(
                    "adjust_operand_for_built_in_operator: conversion failed");
      }  /* if */
#endif /* CHECKING */
    } else {
      /* Pointer cases.  Convert to the pointer type indicated in
         candidate_function. */      
      pointer_type = candidate_function->pointer_type;
#if CHECKING
      if (pointer_type == NULL) {
        internal_error(
                     "adjust_operand_for_built_in_operator: missing ptr type");
      }  /* if */
#endif /* CHECKING */
      prep_conversion_operand(operand, pointer_type,
                              /*result_may_be_lvalue=*/TRUE,
                              /*is_initialization=*/TRUE,  /* arbitrary */
                              expression_kind, ec_no_error,
                              &operand->position);
    }  /* if */
  }  /* if */
}  /* adjust_operand_for_built_in_operator */


void check_for_operator_overloading(an_opname_kind     kind,
                                    a_boolean          unary_operator,
                                    a_boolean          must_be_member_function,
                                    a_boolean          try_conversions,
                                    a_boolean          has_predef_meaning,
                                    an_operand         *operand_1,
                                    an_operand         *operand_2,
                                    an_expression_kind expression_kind,
                                    a_source_position  *operator_position,
                                    an_operand         *result,
                                    a_boolean          *processed)
/*
operand_1 and operand_2 are the operands of an operator indicated by kind.
If unary_operator is TRUE, the operation has only one operand, which is
given by operand_1.  If must_be_member_function is TRUE, this operator
is one where the operator function must be a member function (=, [], (), ->).
try_conversions is TRUE if we should look for conversion functions that
can convert the operands to built-in types that are acceptable for the
built-in version of the operator.  has_predef_meaning is TRUE if the
operator has a predefined meaning for classes (comma, ->, =, and unary &).
It's also used for calls where we don't want an error if no function
matches, because we want to try something else (e.g., the anachronism that
allows a one-argument function for postfix "++" and "--").  Check to see
if there is an operator function that can be invoked for the arguments.
If so, create an operand for the call in *result and return *processed
TRUE.  Also return *processed TRUE if the operation is an ambiguous
overloaded function call.  Otherwise, return *processed FALSE, which means
the caller should try the built-in meaning of the operator.  In that case,
conversion functions will have been applied to the operands if that's
appropriate.  In cases where none of the operands has a class type, the
operands are returned unchanged.  Note that this routine is called for
operator "?", with unary_operator FALSE; the two operands are the second
and third operands of the "?" ("?" cannot be overloaded, but conversion
functions could still apply).
*/
{
  an_arg_operand_ptr       arg_operand_list, arg_operand_list2, arg_operand;
  an_expr_node_ptr         arg_expr_list, end_arg_expr_list;
  a_symbol_ptr             nonmember_functions_symbol;
  a_symbol_ptr             member_functions_symbol;
  a_symbol_ptr             function_symbol;
  a_boolean                operand_1_is_class;
  an_operand               function_operand;
  a_candidate_function_ptr candidate_functions;
  an_arg_match_summary_ptr arg_match;
  a_boolean                matched_except_for_missing_selector = FALSE;
  a_boolean                member_is_best_match;
  an_expr_node_ptr         arg;
  a_type_ptr               routine_type;
  a_param_type_ptr         param;
  an_operand               *bound_function_selector;
  a_boolean                undecidable_because_of_error;

  db_enter(4, "check_for_operator_overloading");
  *processed = FALSE;
  /* Operator overloading should not be tried in constant expressions. */
  if (!is_const_expr_kind(expression_kind)) {
    if (is_error_operand(operand_1) || 
        (!unary_operator && is_error_operand(operand_2))) {
      /* One or both of the operands is an error operand. */
      if (opname_symbol_table[kind] != NULL) {
        /* There exists a function that overloads the operator.  Therefore,
           a match might have been possible.  However, we cannot tell.
           Assume there is a match and give up. */
        *processed = TRUE;
        make_error_operand(result);
      } else {
        /* The operator is not overloaded.  Return to the caller to do
           the built-in operator processing. */
      }  /* if */
    } else {
      /* At least one operand must have a class type.  An error operand for
         the second operand counts as a class operand. */
      operand_1_is_class = is_class_struct_union_type(operand_1->type);
      if (operand_1_is_class ||
          (!unary_operator &&
           (is_class_struct_union_type(operand_2->type) ||
            is_error_operand(operand_2)))) {
        /* Operator overloading may apply.  That is, the operation may be a
           call of an overloaded operator function or the operands may be
           convertible to built-in types appropriate for the built-in
           operator. */
        /* Change the operands into argument operand form. */
        arg_operand_list = alloc_arg_operand();
        copy_operand(operand_1, &arg_operand_list->operand);
        if (unary_operator) {
          arg_operand_list2 = NULL;
        } else {
          arg_operand_list2 = alloc_arg_operand();
          copy_operand(operand_2, &arg_operand_list2->operand);
          arg_operand_list->next = arg_operand_list2;
        }  /* if */
        /* candidate_functions will contain the list of viable functions. */
        candidate_functions = NULL;
        /* Find any member function for the operator. */
        if (operand_1_is_class) {
          member_functions_symbol = opname_member_function_symbol(kind,
                                               skip_typerefs(operand_1->type));
          if (member_functions_symbol != NULL) {
            /* There are member functions for this class type.  See how well
               they match up. */
#if CHECKING
            if (member_functions_symbol->class_of_which_a_member == NULL) {
              internal_error(
                            "check_for_operator_overloading: func not member");
            }  /* if */
#endif /* CHECKING */
            /* Use the first operand as the selector expression, and
               the second operand as the first actual argument . */
            try_overloaded_function_match(member_functions_symbol,
                                          arg_operand_list2,
                                          /*have_selector=*/TRUE,
                                          operand_1,
                                          /*selector_is_object_pointer=*/FALSE,
                                          /*try_user_conversions=*/TRUE,
                                          &candidate_functions,
                                         &matched_except_for_missing_selector);
          }  /* if */
        }  /* if */
        /* Find any non-member function for the operator. */
        if (!must_be_member_function) {
          nonmember_functions_symbol = opname_function_symbol(kind);
          /* There are non-member functions.  See how well they match up. */
          if (nonmember_functions_symbol != NULL) {
            try_overloaded_function_match(nonmember_functions_symbol,
                                          arg_operand_list,
                                          /*have_selector=*/FALSE,
                                          (an_operand *)NULL,
                                          /*selector_is_object_pointer=*/TRUE,
                                          /*try_user_conversions=*/TRUE,
                                          &candidate_functions,
                                         &matched_except_for_missing_selector);
          }  /* if */
        }  /* if */
        /* See if the built-in meaning of the operator can apply if we convert
           the class operand(s) to a built-in type through use of a conversion
           function. */
        if (try_conversions) {
          /* See if we can find user-defined conversions to built-in types that
             will make the built-in operator feasible.  The argument matches
             are compared to the best match so far from the above searches. */
          try_conversions_for_builtin_operator(kind, unary_operator,
                                               arg_operand_list,
                                               &candidate_functions);
        }  /* if */
        /* The candidate_functions list now contains all the viable
           functions.  Find the best. */
        select_best_candidate_functions(&candidate_functions,
                                        operator_position,
                                        &undecidable_because_of_error);
        function_symbol = NULL;
        arg_expr_list = NULL;
        if (undecidable_because_of_error) {
          /* There was a previous error. */
          *processed = TRUE;
          make_error_operand(result);
        } else if (candidate_functions == NULL) {
          /* None of the functions applies. */
          if (has_predef_meaning) {
            /* The operator is one that has a predefined meaning when applied
               to classes (e.g., unary "&").  Leave the operator unprocessed
               and let the caller apply the built-in meaning. */
          } else {
            /* Error: no applicable operator function. */
            *processed = TRUE;
            pos_error(ec_no_matching_operator_function, operator_position);
            make_error_operand(result);
          }  /* if */
        } else if (candidate_functions->next != NULL) {
          /* More than one function applies and is a best match --
             ambiguity. */
          *processed = TRUE;
#if DEBUG
          if (debug_level >= 4) {
            db_candidate_function_list(candidate_functions);
          }  /* if */
#endif /* DEBUG */
          pos_st_start_error(ec_ambiguous_operator_function, operator_position,
                             opname_names[(int)kind]);
          diagnose_overload_ambiguity(candidate_functions, kind);
          make_error_operand(result);
        } else {
          /* Exactly one function applies and is best. */
          function_symbol = candidate_functions->function_symbol;
          if (function_symbol == NULL) {
            /* A built-in operator was selected. */
#if DEBUG
            if (debug_level >= 4) {
              fprintf(f_debug, "check_for_operator_overloading: selected\n");
              db_candidate_function(candidate_functions);
            }  /* if */
#endif /* DEBUG */
            /* *processed is left FALSE so the caller will try the built-in
               meaning. */
            /* Convert the operands to the proper types. */
            adjust_operand_for_built_in_operator(operand_1,
                                                 candidate_functions, 1,
                                                 expression_kind);
            if (!unary_operator) {
              adjust_operand_for_built_in_operator(operand_2,
                                                   candidate_functions, 2,
                                                   expression_kind);
            }  /* if */
          } else {
            /* An operator function was selected. */
#if DEBUG
            if (debug_level >= 4) {
              db_symbol(function_symbol,
                        "check_for_operator_overloading: selected ", 2);
            }  /* if */
#endif /* DEBUG */
            *processed = TRUE;
            routine_type = routine_symbol_type(function_symbol);
            arg_match = candidate_functions->arg_matches;
            arg_operand = arg_operand_list;
            bound_function_selector = NULL;
            member_is_best_match = 
                       routine_type_is_nonstatic_member_function(routine_type);
            if (member_is_best_match) {
              /* The function selected is a non-static member function.
                 Therefore, the first argument is to be used as the selector
                 object.  We must convert it to an object pointer and then
                 cast it to a base class if necessary. */
              bound_function_selector = &arg_operand_list->operand;
              /* Issue any warning about the "this" parameter detected while
                 evaluating the alternatives. */
              issue_warning_from_arg_match_summary(
                                           arg_match,
                                           &bound_function_selector->position);
              /* Make a pointer for the selector, and cast it to a base class
                 if necessary. */
              prep_special_selector_operand(bound_function_selector,
                                            routine_type,
                                            expression_kind);
              /* The "real" argument list starts with the second argument. */
              arg_operand = arg_operand->next;
              arg_match = arg_match->next;
            }  /* if */
            /* Build an expression-form argument list.  Convert the arguments
               on the argument list to the right types.  Note that for the
               member function case we start at the second operand. */
            param = routine_type->variant.routine.extra_info->param_type_list;
            arg_expr_list = end_arg_expr_list = NULL;
            for (; arg_operand != NULL;
                 arg_operand = arg_operand->next,arg_match = arg_match->next) {
              arg = node_for_arg_of_overloaded_function_call(arg_operand,
                                                             arg_match,
                                                             param,
                                                             expression_kind);
              if (arg_expr_list == NULL) {
                arg_expr_list = arg;
              } else {
                end_arg_expr_list->next = arg;
              }  /* if */
              end_arg_expr_list = arg;
              /* Advance to the next parameter unless we're at an ellipsis. */
              if (param != NULL) param = param->next;
            }  /* for */
            /* Do the things that would have been done to the symbol but
               weren't because the specific symbol was not known, and build an
               operand for the function. */
            make_resolved_overloaded_function_operand(
                                                 function_symbol,
                                                 member_is_best_match ?
                                                    member_functions_symbol :
                                                    nonmember_functions_symbol,
                                                 /*have_selector=*/
                                                          member_is_best_match,
                                                 bound_function_selector,
                                                 /*is_qualified_name=*/FALSE,
                                                 expression_kind,
                                                 operator_position,
                                                 &function_operand);
            /* Make the call node and an operand for it. */
            assemble_function_call(&function_operand, bound_function_selector,
                                   arg_expr_list, result);
          }  /* if */
        }  /* if */
        /* Free the candidate functions list. */
        free_candidate_function_list(candidate_functions);
        /* Free the argument list. */
        free_arg_operand_list(arg_operand_list);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If an operand was created, put the right position in it. */
  if (*processed) result->position = *operator_position;
  db_exit();
}  /* check_for_operator_overloading */


/*
If symbol points to a projection symbol that is ambiguous by inheritance,
set *ambiguous to TRUE.  Always set symbol to its fundamental symbol.
*/
#define check_symbol_ambiguous_by_inheritance(symbol, ambiguous)      \
{ if ((symbol)->kind == (a_symbol_kind)sk_projection) {               \
    if ((symbol)->variant.projection.ambiguous) *(ambiguous) = TRUE;  \
    symbol = (symbol)->variant.projection.extra_info->fundamental_symbol; \
  }  /* if */                                                         \
}  /* check_symbol_ambiguous_by_inheritance */


static a_boolean conversion_to_class_possible(
                                  an_operand               *source_operand,
                                  a_type_ptr               dest_type,
                                  a_routine_ptr            *conversion_routine,
                                  a_boolean                *ambiguous,
                                  a_candidate_function_ptr *ambiguity_list)
/*
If source_operand can be converted to the class type dest_type
(via a constructor or conversion function) set *conversion_routine to point
to the routine that can do the conversion and return TRUE.  Otherwise
return FALSE.  If more than one function matches, set *ambiguous to TRUE
and return FALSE.  If ambiguity_list is non-NULL in that case, it is set
to point to a list describing the set of ambiguous functions; the caller must
free that list.  *ambiguity_list is set to NULL to indicate a case that
is undecidable because of an error.  This routine is only used in C++.
*/
{
  a_boolean                     okay;
  a_type_ptr                    class_type;
  a_candidate_function_ptr      candidate_functions;
  a_boolean                     matched_except_for_missing_selector = FALSE;
  a_symbol_ptr                  class_symbol, constructor_symbol;
  a_symbol_ptr                  conversion_symbol;
  a_class_symbol_supplement_ptr cssp;
  an_arg_operand_ptr            arg_operand_list;
  a_boolean                     undecidable_because_of_error;

  db_enter(4, "conversion_to_class_possible");
  /* Note that this routine is like a simplified version of
     select_overloaded_function that works for user-defined conversion
     functions (no arguments, just a "this" parameter). */
  class_type = skip_typerefs(dest_type);
  class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
  cssp = class_symbol->variant.class_struct_union.extra_info;
  /* candidate_functions will contain the list of viable functions. */
  candidate_functions = NULL;
  /* Make an argument list with just the source operand. */
  arg_operand_list = alloc_arg_operand();
  copy_operand(source_operand, &arg_operand_list->operand);
  constructor_symbol = cssp->constructor;
  if (constructor_symbol != NULL) {
    /* The class has constructors. */
    /* Try all the constructors with that argument list. */
    try_overloaded_function_match(constructor_symbol,
                                  arg_operand_list,
                                  /*have_selector=*/TRUE,
                                  (an_operand *)NULL, /* constructor */
                                  /*selector_is_object_pointer=*/FALSE,
                                  /*try_user_conversions=*/FALSE,
                                  &candidate_functions,
                                  &matched_except_for_missing_selector);
  }  /* if */
  if (is_class_struct_union_type(source_operand->type) &&
      cssp->target_of_conversion_function) {
    /* There is at least one conversion function that converts some other
       class into the desired class, and the source type is a class.
       See if there is a conversion function that does the job. */
    try_conversion_function_match(source_operand, dest_type,
                                  (a_builtin_type_kind_set)BTK_NONE,
                                  &candidate_functions);
  }  /* if */
  /* The candidate_functions list now contains all the viable functions.
     Find the best ones. */
  select_best_candidate_functions(&candidate_functions,
                                  &source_operand->position,
                                  &undecidable_because_of_error);
  *conversion_routine = NULL;
  *ambiguous = FALSE;
  okay = FALSE;
  if (undecidable_because_of_error) {
    *ambiguous = TRUE;
    /* Note that candidate_functions is NULL (select_best_candidate_functions
       returns it that way in this case), so a NULL ambiguity_list will
       be returned to indicate "undecidable because of error". */
  } else if (candidate_functions == NULL) {
    /* No constructor or conversion function is suitable. */
  } else if (candidate_functions->next != NULL) {
    /* More than one constructor or conversion function matches at the same
       level.  Ambiguity. */
    *ambiguous = TRUE;
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* Exactly one constructor or conversion function matches best. */
    conversion_symbol = candidate_functions->function_symbol;
    /* If the function is a conversion function that is inherited from a base
       class, check to see if it's ambiguous by inheritance. */
    check_symbol_ambiguous_by_inheritance(conversion_symbol, ambiguous);
    if (!*ambiguous) {
      okay = TRUE;
      *conversion_routine = conversion_symbol->variant.routine.ptr;
    }  /* if */
  }  /* if */
  if (*ambiguous && ambiguity_list != NULL) {
    /* Return the candidate functions list to the caller, for use in generating
       an ambiguity error.  The caller will free the list. */
    *ambiguity_list = candidate_functions;
  } else {
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
  }  /* if */
  free_arg_operand_list(arg_operand_list);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_to_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_to_class_possible */


static a_boolean conversion_from_class_possible(
                               an_operand               *source_operand,
                               a_type_ptr               dest_type,
                               a_builtin_type_kind_set  builtin_types_allowed,
                               a_routine_ptr            *conversion_routine,
                               a_boolean                *std_conversion_needed,
                               a_boolean                *ambiguous,
                               a_candidate_function_ptr *ambiguity_list)
/*
If the class operand source_operand can be converted by a conversion function
to either

(a) dest_type, if dest_type is non-NULL (a standard conversion can be
    done after the conversion function, if necessary), or
(b) a built-in type in the set given by builtin_types_allowed, if
    dest_type is NULL.

then set *conversion_routine to point to the routine that can do the
conversion, and return TRUE.  Otherwise return FALSE.  If more than one
function matches, set *ambiguous to TRUE and return FALSE.  If ambiguity_list
is non-NULL in that case, it is set to point to a list describing the set
of ambiguous functions; the caller must free that list.  *ambiguity_list
is set to NULL to indicate a case that is undecidable because of an error.
If a standard conversion is required after the conversion function, return
*std_conversion_needed TRUE.  This routine is only used in C++ mode.
*/
{
  a_boolean                okay;
  a_symbol_ptr             conversion_symbol;
  a_candidate_function_ptr candidate_functions;
  a_boolean                undecidable_because_of_error;

  db_enter(4, "conversion_from_class_possible");
  /* This routine is similar to select_overloaded_function. */
  candidate_functions = NULL;
  *std_conversion_needed = FALSE;
  /* Find any viable conversion functions. */
  try_conversion_function_match(source_operand, dest_type,
                                builtin_types_allowed,
                                &candidate_functions);
  /* Of the viable functions, select the best. */
  select_best_candidate_functions(&candidate_functions,
                                  &source_operand->position,
                                  &undecidable_because_of_error);
  *ambiguous = FALSE;
  *conversion_routine = NULL;
  okay = FALSE;
  if (undecidable_because_of_error) {
    *ambiguous = TRUE;
    /* Note that candidate_functions is NULL (select_best_candidate_functions
       returns it that way in this case), so a NULL ambiguity_list will
       be returned to indicate "undecidable because of error". */
  } else if (candidate_functions == NULL) {
    /* There are no viable conversion functions. */
  } else if (candidate_functions->next != NULL) {
    /* There are several equally desirable functions. */
    *ambiguous = TRUE;
#if DEBUG
    if (debug_level >= 4) {
      db_candidate_function_list(candidate_functions);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* There is exactly one best conversion function. */
    conversion_symbol = candidate_functions->function_symbol;
    /* If the function is a conversion function that is inherited from a base
       class, check to see if it's ambiguous by inheritance. */
    check_symbol_ambiguous_by_inheritance(conversion_symbol, ambiguous);
    if (!*ambiguous) {
      okay = TRUE;
      *conversion_routine = conversion_symbol->variant.routine.ptr;
      *std_conversion_needed =
                 candidate_functions->std_conversion_after_conversion_function;
    }  /* if */
  }  /* if */
  if (*ambiguous && ambiguity_list != NULL) {
    /* Return the candidate functions list to the caller, for use in generating
       an ambiguity error.  The caller will free the list. */
    *ambiguity_list = candidate_functions;
  } else {
    /* Free the candidate functions list. */
    free_candidate_function_list(candidate_functions);
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_from_class_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_from_class_possible */


static a_boolean class_bitwise_copy_possible(a_type_ptr source_type,
                                             a_type_ptr dest_type,
                                             a_boolean  is_initialization)
/*
Return TRUE if an entity of type dest_type (a class type) can be initialized
from (is_initialization == TRUE) or assigned from (is_initialization == FALSE)
an entity of type source_type using a bitwise copy.  This routine is
used only in C++ mode.
*/
{
  a_boolean                     bitwise_copy_allowed = FALSE;
  a_class_symbol_supplement_ptr cssp;

  dest_type = skip_typerefs(dest_type);
  cssp = symbol_supplement_for_class(dest_type);
  source_type = skip_typerefs(source_type);
  if (is_class_struct_union_type(source_type)) {
    if (is_initialization ? cssp->construction_by_bitwise_copy_allowed :
                            cssp->assignment_by_bitwise_copy_allowed) {
      /* The destination class can be set by a bitwise copy from something
         of the same type or a derived type thereof. */
      if (types_are_compatible(dest_type, source_type) ||
          find_base_class_of(source_type, dest_type) != NULL) {
        bitwise_copy_allowed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return bitwise_copy_allowed;
}  /* class_bitwise_copy_possible */


a_boolean user_defined_conversion_possible(an_operand    *source_operand,
                                           a_type_ptr    dest_type,
                                           a_boolean     is_initialization,
                                           a_routine_ptr *conversion_routine,
                                           a_boolean     *class_bitwise_copy,
                                           a_boolean     *failed)
/*
Check whether or not the source operand can be converted to the
destination type by a user-defined conversion (constructor or
conversion routine), in an initialization (is_initialization == TRUE)
or assignment (is_initialization == FALSE).  If so, set
*conversion_routine to point to the conversion routine (or set
*class_bitwise_copy to TRUE) and return TRUE.  If not, return FALSE.  If
a user-defined conversion is the only hope of converting the source
operand to the destination type (i.e., one or the other has a class
type), and no conversion was found, issue an error, change
source_operand to an error operand, set *failed to TRUE, and return
FALSE.  Note that this routine should only be called when the
conversion must be done, not when we're just wondering if it can be
done, because it issues errors.  See 12.3 in the ARM.  This routine is
only called in C++ mode.  The destination type may not be a reference
type (the caller should have rewritten that case in terms of the
equivalent pointer case).
*/
{
  a_boolean                okay = FALSE, ambiguous, to_class;
  a_boolean                std_conversion_needed;
  a_type_ptr               source_type;
  an_error_code            err_code;
  a_candidate_function_ptr ambiguity_list;

  *conversion_routine = NULL;
  *class_bitwise_copy = FALSE;
  *failed = FALSE;
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("user_defined_conversion_possible: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  source_type = source_operand->type;
  to_class = is_class_struct_union_type(dest_type);
  if (to_class) {
    /* The destination type is a class. */
    to_class = TRUE;
    if (class_bitwise_copy_possible(source_type, dest_type,
                                    is_initialization)) {
      /* A bitwise copy of the class is allowed. */
      *class_bitwise_copy = TRUE;
      okay = TRUE;
    } else if (conversion_to_class_possible(source_operand, dest_type,
                                            conversion_routine, &ambiguous,
                                            &ambiguity_list)) {
      /* A user-defined conversion (constructor or conversion function) is
         available to convert to the destination type. */
      okay = TRUE;
    } else {
      /* The conversion is not possible. */
      *failed = TRUE;
      /* Pick the right error code. */
      if (is_class_struct_union_type(source_type)) {
        /* Both the source and destination types are classes, so the
           message should indicate that both constructors and conversion
           functions were considered. */
        err_code = ambiguous ? ec_ambiguous_user_defined_conversion :
                               ec_no_user_defined_conversion;
      } else {
        /* The source type is a non-class and the destination type is a class,
           so the message should indicate that constructors were considered. */
        err_code = ambiguous ? ec_ambiguous_constructor_for_conversion :
                               ec_no_constructor_for_conversion;
      }  /* if */
    }  /* if */
  } else if (is_class_struct_union_type(source_type)) {
    /* The source type is a class (and the destination type is not a class). */
    if (conversion_from_class_possible(source_operand, dest_type,
                                       (a_builtin_type_kind_set)BTK_NONE,
                                       conversion_routine,
                                       &std_conversion_needed,
                                       &ambiguous, &ambiguity_list)) {
      /* There is a conversion function that converts from the source class
         type to the destination type. */
      okay = TRUE;
    } else {
      *failed = TRUE;
      /* Pick the right error code. */
      err_code = ambiguous ? ec_ambiguous_conversion_function :
                             ec_no_conversion_function;
    }  /* if */
  }  /* if */
  if (*failed) {
    /* The conversion failed. */
    if (!ambiguous) {
      /* No conversion applies. */
      if (is_incomplete_type(dest_type) || is_incomplete_type(source_type)) {
        /* Conversion to or from an incomplete type is not possible (in this
           case, anyway).  Use a different message for clarity. */
        error_in_operand(ec_incomplete_type_not_allowed, source_operand);
      } else {
        /* Put out the usual message (which has already been chosen to
           describe the problem). */
        type2_error_in_operand(err_code, source_operand,
                               source_type, dest_type);
      }  /* if */
    } else {
      /* More than one conversion applies (ambiguity). */
      /* A NULL ambiguity_list indicates a case that was undecidable because
         of an error (no additional error is needed). */
      if (ambiguity_list != NULL) {
        pos_ty2_start_error(err_code, &source_operand->position,
                            source_type, dest_type);
        diagnose_overload_ambiguity(ambiguity_list, (an_opname_kind)onk_none);
        free_candidate_function_list(ambiguity_list);
      }  /* if */
      conv_to_error_operand(source_operand);
    }  /* if */
  }  /* if */
  return okay;
}  /* user_defined_conversion_possible */


static a_boolean conversion_possible(an_operand         *source_operand,
                                     a_type_ptr         dest_type,
                                     a_boolean          is_initialization,
                                     an_expression_kind expression_kind,
                                     an_error_code      incompatible_err,
                                     a_source_position  *err_pos,
                                     a_routine_ptr      *conversion_routine,
                                     a_boolean          *class_bitwise_copy)
/*
Check whether or not the source operand can be converted to the destination
type, implicitly, in an initialization (is_initialization == TRUE) or
assignment (is_initialization == FALSE).  If so, return TRUE.  If not, issue
the error incompatible_err at the position err_pos, change the operand to
an error operand, and return FALSE.  If the conversion is valid and
requires a conversion routine (constructor or conversion function),
*conversion_routine is set to point to the routine; otherwise, it is set
to NULL.  If the conversion is a class bitwise copy, *class_bitwise_copy
is set to TRUE.  expression_kind indicates the current expression kind.
See 3.3.16.1 in the ANSI C standard and 12.3 in the ARM.  Note that this
routine should only be called when the conversion must be done, not when
we're just wondering if it can be done, because it does operand
transformations on source_operand and issues errors.  The destination
type may not be a reference type (the caller should have rewritten that
case in terms of the equivalent pointer case).
*/
{
  a_boolean     okay = FALSE, failed = FALSE, ambiguous;
  a_type_ptr    source_type;
  an_error_code warning_suggested;
  an_arg_match_level
                match_level;

  db_enter(4, "conversion_possible");
  *conversion_routine = NULL;
  *class_bitwise_copy = FALSE;
  /* Convert array --> pointer and function --> pointer. */
  do_operand_transformations(source_operand,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                             TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION,
                             expression_kind);
#if CHECKING
  if (is_reference_type(dest_type)) {
    internal_error("conversion_possible: dest_type is reference");
  }  /* if */
#endif /* CHECKING */
  source_type = source_operand->type;
  /* Note that we do not drop type qualifiers on the source and destination
     types yet, because we may still be dealing with lvalue cases. */
  if (is_error_type(source_type) || is_error_type(dest_type)) {
    /* An error type is compatible with anything. */
    okay = TRUE;
    /* If the source is an lvalue, convert it to an rvalue. */
    conv_lvalue_to_rvalue(source_operand, expression_kind);
  } else if (C_dialect != C_dialect_cplusplus &&
             is_class_struct_union_type(dest_type) &&
             types_are_compatible(f_skip_typerefs(source_type),
                                  f_skip_typerefs(dest_type))) {
    /* In C, a struct or union is compatible with the same struct or union.
       Type qualifiers are ignored because they will be dropped on the source
       type in the conversion to an rvalue, and any qualifiers on the
       destination type can be added after that. */
    *class_bitwise_copy = TRUE;
    okay = TRUE;
    /* If the source is an lvalue, convert it to an rvalue. */
    conv_lvalue_to_rvalue(source_operand, expression_kind);
  } else if (is_indefinite_function_operand(source_operand)) {
    /* The source is an indefinite function, i.e., the address of an
       overloaded function.  It can be converted to an appropriate
       pointer (ARM 13.3) or pointer-to-member type (not mentioned in ARM,
       but sensible). */
    if (find_addr_of_overloaded_function_match(source_operand->variant.symbol,
                                               dest_type,
                                               &source_operand->position,
                                               &match_level, &ambiguous)
                                                                     != NULL) {
      okay = TRUE;
    } else if (ambiguous) {
      /* More than one function matches. */
      pos_sy_error(ec_ambiguous_ptr_to_overloaded_function, err_pos,
                   source_operand->variant.symbol);
      conv_to_error_operand(source_operand);
    } else {
      /* No match. */
      pos_sy_error(ec_no_match_for_addr_of_overloaded_function, err_pos,
                   source_operand->variant.symbol);
      conv_to_error_operand(source_operand);
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus &&
             user_defined_conversion_possible(source_operand, dest_type,
                                              is_initialization,
                                              conversion_routine,
                                              class_bitwise_copy, &failed)) {
    /* A user-defined conversion can be done. */
    okay = TRUE;
  } else if (!failed) {
    /* If the source is an lvalue, convert it to an rvalue. */
    conv_lvalue_to_rvalue(source_operand, expression_kind);
    /* Re-fetch source type in case of an error in the lvalue --> rvalue
       conversion, and also because any type qualifiers on the source type
       have been dropped in the conversion to an rvalue. */
    source_type = source_operand->type;
    /* See if there is a valid implicit conversion from the source type to
       the destination type. */
    if (impl_conversion_possible(source_type,
                                 is_constant_operand(source_operand),
                                 &source_operand->variant.constant,
                                 dest_type,
                                 /*suppress_extensions=*/FALSE,
                                 incompatible_err, &warning_suggested)) {
      /* An implicit conversion is legal. */
      okay = TRUE;
      /* Warn on oddball conversions. */
      if (warning_suggested != ec_no_error) {
        pos_warning(warning_suggested, err_pos);
      }  /* if */
    } else {
      /* The conversion is not legal. */
      /* Note:  If this is ever changed to display the types, remember that
         prep_initializer_operand changes reference types to pointer types
         before calling this routine. */
      pos_error(incompatible_err, err_pos);
      conv_to_error_operand(source_operand);
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "conversion_possible: %s\n", okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* conversion_possible */


static void prep_class_bitwise_copy_operand(an_operand         *source_operand,
                                            a_type_ptr         dest_type,
                                            an_expression_kind expression_kind)
/*
source_operand is to be copied bitwise to an entity of type dest_type.
Both have class types.  Adjust source_operand if necessary, specifically
for the case where the source type is a derived class of dest_type.
This is used both for initialization and for assignment.  See ARM 8.4.1
(aggregate initialization) and 5.17 (assignment operators).
*/
{
  a_type_ptr       source_type;
  a_boolean        is_arrow_operator = TRUE;
  a_base_class_ptr bcp;

  source_type = skip_typerefs(source_operand->type);
  dest_type = skip_typerefs(dest_type);
  if (types_are_compatible(source_type, dest_type)) {
    /* The source and destination types are the same type, so no conversion
       is necessary. */
  } else {
    /* An entity of a base class is being initialized from an object
       of a derived class.  Get its address, cast it to the base class,
       then indirect through that to get an object of the base class. */
    conv_operand_to_object_pointer(source_operand, expression_kind);
    bcp = find_base_class_of(source_type, dest_type);
#if CHECKING
    if (bcp == NULL) {
      internal_error("prep_class_bitwise_copy_operand: base class not found");
    }  /* if */
#endif /* CHECKING */
    base_class_cast_operand(source_operand, bcp, &is_arrow_operator,
                            /*is_implicit_cast=*/TRUE, expression_kind);
    /* Make an address (an lvalue) for the base class object. */
    conv_object_pointer_to_lvalue(source_operand);
  }  /* if */
  /* Make the source an rvalue. */
  do_operand_transformations(source_operand, TOPT_NO_OPTIONS, expression_kind);
}  /* prep_class_bitwise_copy_operand */


void user_convert_operand(an_operand         *operand,
                          a_type_ptr         dest_type,
                          a_boolean          result_may_be_lvalue,
                          a_routine_ptr      conversion_routine,
                          a_boolean          class_bitwise_copy,
                          an_expression_kind expression_kind)
/*
Create a call of the indicated user-defined conversion routine with the
indicated operand to convert the operand to dest_type.  Change the operand
into the result of the conversion.  dest_type may be NULL to indicate that
no additional conversion is needed after the conversion function is called.
If class_bitwise_copy is TRUE, the "conversion" is simply a bitwise copy
of a class.  Unless result_may_be_lvalue is TRUE, force the result to be
an rvalue.
*/
{
  an_expr_node_ptr  rout_node, arg_expr_list;
  an_operand        orig_operand;
  a_boolean         do_std_conversion;

  orig_operand = *operand;
  if (class_bitwise_copy) {
    /* Bitwise copy of a class. */
    prep_class_bitwise_copy_operand(operand, dest_type, expression_kind);
  } else if (conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* Conversion function. */
    set_up_for_conversion_function_call(operand, conversion_routine,
                                        expression_kind, &arg_expr_list);
    /* Conversion routines are called directly. */
    /* Make a node for the address of the function. */
    rout_node = function_addr_expr(conversion_routine);
    rout_node->next = arg_expr_list;
    /* Make an operand for the call. */
    make_function_call(rout_node, conversion_routine->type,
                       (a_boolean)conversion_routine->is_virtual,
                       &orig_operand.position, operand);
    /* A standard conversion may be required after a conversion function. */
    do_std_conversion = (dest_type != NULL &&
                         !types_are_compatible(dest_type, operand->type));
    if (!result_may_be_lvalue || do_std_conversion) {
      /* The caller will not accept an lvalue, or a standard conversion
         must be done, so convert an lvalue to an rvalue.  The operand
         could only be an lvalue if the conversion function returns a
         reference. */
      conv_lvalue_to_rvalue(operand, expression_kind);
    }  /* if */
    if (do_std_conversion) {
      /* Do a necessary standard conversion. */
      cast_operand(dest_type, operand, expression_kind,
                   /*is_implicit_cast=*/TRUE);
    }  /* if */
  } else {
#if CHECKING
    if (conversion_routine->special_kind !=
                                    (a_special_function_kind)sfk_constructor) {
      internal_error("user_convert_operand: not conversion or constructor");
    }  /* if */
#endif /* CHECKING */
    /* Constructor. */
    set_up_for_constructor_call(operand, conversion_routine,
                                expression_kind, &arg_expr_list);
    /* Make a constructor dynamic init into a temporary, and an operand for
       the value it produces. */
    make_constructor_dynamic_init(conversion_routine, arg_expr_list,
                                  /*result_is_addr=*/FALSE, operand);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(operand, &orig_operand);
}  /* user_convert_operand */


static void convert_operand(an_operand         *source_operand,
                            a_type_ptr         dest_type,
                            a_boolean          result_may_be_lvalue,
                            a_routine_ptr      conversion_routine,
                            a_boolean          class_bitwise_copy,
                            an_expression_kind expression_kind)
/*
Convert source_operand to dest_type.  If conversion_routine is non-NULL,
the conversion is done by calling that conversion routine.  If
class_bitwise_copy is TRUE, a bitwise copy of a class is done.
Unless result_may_be_lvalue is TRUE, force the result to be an rvalue.
*/
{
  if (class_bitwise_copy || conversion_routine != NULL) {
    /* Call a user-defined conversion routine. */
    user_convert_operand(source_operand, dest_type, result_may_be_lvalue,
                         conversion_routine, class_bitwise_copy,
                         expression_kind);
  } else {
    /* Cast the operand to the result type. */
    cast_operand(dest_type, source_operand, expression_kind,
                 /*is_implicit_cast=*/TRUE);
  }  /* if */
}  /* convert_operand */


static void prep_conversion_operand(an_operand         *source_operand,
                                    a_type_ptr         dest_type,
                                    a_boolean          result_may_be_lvalue,
                                    a_boolean          is_initialization,
                                    an_expression_kind expression_kind,
                                    an_error_code      incompatible_err,
                                    a_source_position  *err_pos)
/*
Convert source_operand to dest_type if that is possible.  If not, issue
incompatible_err at *err_pos.  This routine is used for initialization
(is_initialization == TRUE) and assignment (is_initialization == FALSE).
source_operand may be an rvalue or an lvalue.  Unless
result_may_be_lvalue is TRUE, force the result to be an rvalue.
*/
{
  a_routine_ptr conversion_routine;
  a_boolean     class_bitwise_copy;

  /* See if the source and destination types are compatible.  This also
     converts from an lvalue to an rvalue if necessary. */
  if (conversion_possible(source_operand, dest_type, is_initialization,
                          expression_kind,
                          incompatible_err, err_pos, &conversion_routine,
                          &class_bitwise_copy)) {
    /* The types are compatible.  Do the conversion. */
    convert_operand(source_operand, dest_type, result_may_be_lvalue,
                    conversion_routine, class_bitwise_copy, expression_kind);
  }  /* if */
}  /* prep_conversion_operand */


static void check_access_to_elided_copy_constructor(
                                                  a_type_ptr        class_type,
                                                  a_source_position *err_pos)
/*
A conversion to class_type is being done by eliding a copy constructor.
Check that the copy constructor that would have been referenced exists
and is accessible (ARM 12.6.1).  Issue an error at *err_pos if not.
*/
{
  a_symbol_ptr cctor_sym;
  a_boolean    ambiguous;
  a_boolean    class_bitwise_copy;

  /* The entity being copied is the output of a constructor, so it
     has no type qualifiers on it. */
  cctor_sym = find_copy_constructor(class_type,
                                    /*const_required=*/FALSE,
                                    /*volatile_required=*/FALSE,
                                    &ambiguous, &class_bitwise_copy);
  if (class_bitwise_copy) {
    /* A bitwise copy is allowed, so the "copy constructor" is accessible. */
  } else if (cctor_sym == NULL) {
    if (!ambiguous) {
      /* No applicable copy constructor. */
      pos_ty_error(ec_no_suitable_copy_constructor, err_pos, class_type);
    } else {
      /* More than one applicable copy constructor. */
      pos_ty_error(ec_ambiguous_copy_constructor, err_pos, class_type);
    }  /* if */
  } else {
    if (!have_access_to_symbol(cctor_sym)) {
      pos_sy_error(ec_inaccessible_special_function, err_pos, cctor_sym);
    }  /* if */
  }  /* if */
}  /* check_access_to_elided_copy_constructor */


static void determine_ctor_for_class_init(
                                        an_operand         *source_operand,
                                        a_type_ptr         dest_type,
                                        an_expression_kind expression_kind,
                                        a_routine_ptr      *conversion_routine,
                                        an_expr_node_ptr   *arg_expr_list,
                                        a_boolean          *class_bitwise_copy)
/*
An entity of type dest_type (a class type) is being initialized from
source_operand.  The constructor or conversion function required to do the
copy and/or conversion is given by conversion_routine.  What's supposed
to happen is that the conversion routine is called for the operand, and
then the result is copied to the destination using a copy constructor.
However, if the conversion routine is a constructor (copy or not), the
initialization can be done by calling the constructor to build its result
directly in the destination.  If that can be done, and the constructor is
a non-copy constructor, we have in effect optimized out a call of a copy
constructor (i.e., we have elided it).  The language requires that we
still check to see that the copy constructor we would have used exists
and is accessible.  On return from this routine, *conversion_routine is
always a constructor (copy or not) so the caller can use that routine to
construct the result.  If the original *conversion_routine is a conversion
function, the operand is converted using that function and then a copy
constructor is returned that will copy the converted operand to the
destination.  If no appropriate copy constructor exists in that case,
*conversion_routine is returned NULL.  Alternatively, if a bitwise
copy is possible *class_bitwise_copy will be returned TRUE.  dest_type
is allowed to be a class having no constructors at all.  When
*conversion_routine is returned non-NULL, an argument list for the
call of that routine is returned in *arg_expr_list.  If *class_bitwise_copy
is TRUE on input, just set *arg_expr_list and return.  This routine
is used in both C and C++ mode, although *class_bitwise_copy should
always be TRUE in C mode.  expression_kind is the current expression kind.
*/
{
  a_boolean    dummy_arg;
  a_type_ptr   class_type = skip_typerefs(dest_type);

  if (*class_bitwise_copy) {
    /* The operation is already a class bitwise copy on input, so leave it
       that way. */
    *conversion_routine = NULL;
  } else if (*conversion_routine == NULL) {
    /* There was a previous error. */
#if CHECKING
    if (!is_error_operand(source_operand)) {
      internal_error(
                "determine_ctor_for_class_init: not bitwise copy, no routine");
    }  /* if */
#endif /* CHECKING */
  } else {
    if ((*conversion_routine)->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
      /* The routine is a constructor (copy or not).  *conversion_routine
         is therefore already appropriate for return to the caller. */
      if (is_copy_constructor(*conversion_routine, class_type,
                              &dummy_arg, &dummy_arg)) {
        /* The conversion routine is a copy constructor, so no copy constructor
           elision is being done. */
      } else {
        /* The conversion routine is a non-copy constructor, so copy
           constructor elision is being done. */
        check_access_to_elided_copy_constructor(class_type,
                                                &source_operand->position);
      }  /* if */
    } else {
#if CHECKING
      if ((*conversion_routine)->special_kind !=
                                     (a_special_function_kind)sfk_conversion) {
        internal_error("determine_ctor_for_class_init: bad special kind");
      }  /* if */
#endif /* CHECKING */
      /* The routine is a conversion function.  Do the conversion and then
         try to find a copy constructor that can copy the result of the
         conversion for the caller. */
      user_convert_operand(source_operand, /*dest_type=*/(a_type_ptr)NULL,
                           /*result_may_be_lvalue=*/FALSE,
                           *conversion_routine, /*class_bitwise_copy=*/FALSE,
                           expression_kind);
      /* See if an appropriate copy constructor exists. */
      *conversion_routine = select_copy_constructor(
                              class_type,
                              is_const_qualified_type(source_operand->type),
                              is_volatile_qualified_type(source_operand->type),
                              &source_operand->position, class_bitwise_copy);
    }  /* if */
  }  /* if */
  if (*conversion_routine != NULL) {
    /* Prepare for the call of the constructor. */
    set_up_for_constructor_call(source_operand, *conversion_routine,
                                expression_kind, arg_expr_list);
  } else if (*class_bitwise_copy) {
    /* A bitwise copy should be done. */
    prep_class_bitwise_copy_operand(source_operand, dest_type,
                                    expression_kind);
    *arg_expr_list = make_node_from_operand(source_operand);
  } else {
    *arg_expr_list = NULL;
  }  /* if */
}  /* determine_ctor_for_class_init */


void prep_elision_initializer_operand(an_operand       *source_operand,
                                      a_type_ptr       dest_type,
                                      a_routine_ptr    *conversion_routine,
                                      an_expr_node_ptr *arg_expr_list,
                                      a_boolean        *class_bitwise_copy)
/*
Prepare an initializer value for an initialization of a class where we
know the identity of the object being initialized (as opposed to, for
example, an argument initialization, where the object being initialized
is somewhat of an abstraction).  Issue an error if the source operand
cannot be converted to the destination class type.  Return a pointer to
the conversion routine to call in *conversion_routine, or NULL if there
is an error, or NULL and *class_bitwise_copy TRUE if a bitwise copy should
be done.  Return an argument list for the call or the expression to
be copied bitwise in *arg_expr_list.  Basically, this routine determines
that a type conversion can be done (and how), but leaves it to the caller
to create the code that calls the conversion routine (which is likely to
be a dynamic init entry instead of a statement).  This routine is used
used in both C and C++ mode, but it exists to do copy constructor
elision in C++ mode.
*/
{
  /* Look for a constructor to convert the expression to the required
     class type. */
  if (conversion_possible(source_operand, dest_type,
                          /*is_initialization=*/TRUE,
                          (an_expression_kind)ek_normal,
                          ec_bad_initializer_type,
                          &source_operand->position,
                          conversion_routine,
                          class_bitwise_copy)) {
    /* The conversion is possible.  Determine the routine and argument
       list to return to the caller. */
    determine_ctor_for_class_init(source_operand, dest_type,
                                  (an_expression_kind)ek_normal,
                                  conversion_routine, arg_expr_list,
                                  class_bitwise_copy);
  } else {
    *conversion_routine = NULL;
    *arg_expr_list = NULL;
  }  /* if */
}  /* prep_elision_initializer_operand */


static void convert_operand_into_temp(an_operand         *source_operand,
                                      a_type_ptr         dest_type,
                                      an_expression_kind expression_kind,
                                      an_error_code      incompatible_err,
                                      a_boolean          *err)
/*
Convert source_operand to dest_type, put it into a newly-created temporary,
and return an rvalue for the address of the temporary in source_operand.
If the conversion is not possible, issue the error incompatible_err,
convert source_operand to an error operand, and return *err TRUE.
*/
{
  a_boolean        class_bitwise_copy;
  a_routine_ptr    conversion_routine;
  an_expr_node_ptr arg_expr_list;
  an_operand       orig_operand;

  *err = FALSE;
  orig_operand = *source_operand;
  /* See if the conversion is possible. */
  if (conversion_possible(source_operand, dest_type,
                          /*is_initialization=*/TRUE,
                          expression_kind,
                          incompatible_err, &source_operand->position,
                          &conversion_routine,
                          &class_bitwise_copy)) {
    /* Yes. */
    if (conversion_routine != NULL &&
        conversion_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
      /* Class initialization using a constructor.  Do the initialization
         directly into the temporary.  Note that we do not do the copy
         constructor elision check here.  The ARM is not clear on that,
         but cfront doesn't do it, and we can justify that by saying this is
         a conversion instead of an initialization. */
      set_up_for_constructor_call(source_operand, conversion_routine,
                                  expression_kind, &arg_expr_list);
      make_constructor_dynamic_init(conversion_routine, arg_expr_list,
                                    /*result_is_addr=*/TRUE, source_operand);
    } else {
      /* Non-constructor case.  Convert the operand. */
      convert_operand(source_operand, dest_type,
                      /*result_may_be_lvalue=*/FALSE,
                      conversion_routine, class_bitwise_copy, expression_kind);
      if (conversion_routine != NULL &&
          conversion_routine->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
        /* The conversion was done by a conversion function, and the
           result of the conversion is already a temporary.  Convert the
           operand from the value of the temporary to the address. */
        conv_operand_to_object_pointer(source_operand, expression_kind);
      } else {
        /* Initialize a temporary with the converted value. */
        temp_init_from_operand(source_operand);
      }  /* if */
    }  /* if */
  } else {
    /* The conversion is not possible.  The error has already been issued. */
    *err = TRUE;
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* convert_operand_into_temp */


static a_boolean is_field_selection_lvalue_operand(an_operand *operand)
/*
Return TRUE if the operand is a field selection lvalue.  This is used for a
limited loophole allowed in cfront compatibility mode.
*/
{
  a_boolean is_field_selection = FALSE;

  if (is_expression_operand(operand) && is_an_lvalue(operand)) {
    an_expr_node_ptr node = operand->variant.expression;
    if (is_operation_node(node)) {
      if (node->variant.operation.kind == (an_expr_operator_kind)eok_field) {
        is_field_selection = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_field_selection;
}  /* is_field_selection_lvalue_operand */


static a_boolean same_pointer_type_with_added_qualifiers(
                                                        a_type_ptr dest_type,
                                                        a_type_ptr source_type)
/*
Return TRUE if source_type and dest_type (which are pointer types) are
compatible types except that dest_type may have some additional type qualifiers
at some level(s).
*/
{
  a_boolean same = FALSE;

  do {
    dest_type = type_pointed_to(dest_type);
    source_type = type_pointed_to(source_type);
    if (any_qualifier_missing(dest_type, source_type)) {
      /* Some qualifier is missing, so give up. */
      goto end_of_routine;
    }  /* if */
    dest_type = skip_typerefs(dest_type);
    source_type = skip_typerefs(source_type);
  } while (is_pointer_type(dest_type) && is_pointer_type(source_type));
  /* After the pointers are stripped, the remaining types must be the same. */
  if (types_are_compatible(dest_type, source_type)) same = TRUE;
end_of_routine:
  return same;
}  /* same_pointer_type_with_added_qualifiers */


void prep_initializer_operand(an_operand         *source_operand,
                              a_type_ptr         dest_type,
                              a_boolean          initializing_return_value,
                              an_expression_kind expression_kind,
                              an_error_code      incompatible_err)
/*
Check the operand for initializer compatibility against the type supplied.
Cast the operand if required to make it the right type.  Convert the
operand from an lvalue to an rvalue if necessary (it usually is).
initializing_return_value is TRUE if the initialization is being done
to return a value in a return statement.  expression_kind indicates the
kind of expression being scanned (but it's assumed that constant
expressions would not come here).  If the operand and type are
incompatible, issue the error incompatible_err.  This routine is used for
initialization, function call arguments, and return expressions.  It is
not used when copy constructor elision is possible; see
prep_elision_initializer_operand.
*/
{
  a_type_ptr base_dest_type, base_source_type;
  a_type_ptr unqual_dest_type, unqual_source_type;
  a_boolean  type_is_correct_or_derived, err = FALSE, dropping_qualifiers;
  a_boolean  conversion_to_temp_done, ref_to_nonconst, warn = FALSE;
  an_operand orig_operand;

  orig_operand = *source_operand;
  base_source_type = source_operand->type;
  if (is_error_operand(source_operand)) {
     /* Previous error.  Leave the operand alone. */
  } else if (is_reference_type(dest_type)) {
    /* When initializing a reference T&, there are two cases (ARM 8.4.3):
         (1)  If the initializer is an lvalue of type T or of
              a type derived from T for which T is an accessible base,
              the initialization is done directly;
         (2)  Otherwise, the reference must be const.  A temporary of
              type T is created and initialized with the initializer,
              and the reference points to the temporary.
    */
    base_dest_type = type_pointed_to(dest_type);
    unqual_source_type = skip_typerefs(base_source_type);
    unqual_dest_type = skip_typerefs(base_dest_type);
    type_is_correct_or_derived = FALSE;
    if (types_are_compatible(unqual_dest_type, unqual_source_type)) {
      /* The type is correct. */
      type_is_correct_or_derived = TRUE;
    } else if (is_class_struct_union_type(unqual_dest_type) &&
               is_class_struct_union_type(unqual_source_type) &&
               find_base_class_of(unqual_source_type,
                                  unqual_dest_type) != NULL) {
      /* The initializer has a derived type. */
      type_is_correct_or_derived = TRUE;
    } else if (cfront_compatibility_mode &&
               is_pointer_type(unqual_dest_type) &&
               is_pointer_type(unqual_source_type) &&
               same_pointer_type_with_added_qualifiers(unqual_dest_type,
                                                       unqual_source_type)) {
      /* The type is a pointer type and is correct, except that the
         destination type has some qualifiers that are not present on
         the source type (at any level).  Standard C++ processing can
         only add type qualifiers at the top level. */
      type_is_correct_or_derived = TRUE;
    }  /* if */
    ref_to_nonconst = !is_const_qualified_type(base_dest_type);
    /* The destination type must have no fewer type qualifiers than the source
       type to be usable without conversion (ARM 8.4.3). */
    dropping_qualifiers = type_is_correct_or_derived &&
                          any_qualifier_missing(base_dest_type,
                                                base_source_type);
    if (dropping_qualifiers) {
      /* There are fewer qualifiers on the destination than on the source,
         so the initialization would involve dropping qualifiers. */
      /* cfront makes a field selected from a const structure compatible
         with a non-const reference to the underlying type:
           struct A {};
           struct B {
             A a;
             B() {}
           };
           const B bb;
           A &r = bb.a;  // okay according to cfront, no warning
           const B *pb;
           A &rr = pb->a;  // okay according to cfront, warning
      */
      if (cfront_compatibility_mode &&
          ref_to_nonconst && is_const_qualified_type(base_source_type) &&
          is_field_selection_lvalue_operand(source_operand)) {
        /* Okay.  Note that a temporary will not be used in these cases. */
        pos_warning(ec_cfront_nonconst_ref_init, &source_operand->position);
        dropping_qualifiers = FALSE;
      } else {
        /* Qualifiers are being dropped. */
        type_is_correct_or_derived = FALSE;
      }  /* if */
    }  /* if */
    if (type_is_correct_or_derived && is_an_lvalue(source_operand)) {
      /* The initial value is an lvalue of the right type; the initialization
         can be done directly. */
      /* Convert the lvalue to an rvalue pointer to the object. */
      take_address_of_lvalue(source_operand, expression_kind);
      if (is_constant_operand(source_operand) &&
          /* "false" means zero, i.e., a null pointer. */
          is_false_constant(&source_operand->variant.constant)) {
        /* Initializing a reference to NULL, which is not allowed:
             int &p = *(int *)0;
        */
        error_in_operand(ec_null_reference, source_operand);
      } else {
        /* Use a pointer type instead of a reference type on the
           destination. */
        dest_type = make_pointer_type(base_dest_type);
        /* Cast the operand to the result type. */
        cast_operand(dest_type, source_operand, expression_kind,
                     /*is_implicit_cast=*/TRUE);
      }  /* if */
    } else if (type_is_correct_or_derived &&
               is_a_function_designator(source_operand)) {
      /* The initial value is a function designator of the right type;
         the initialization can be done directly. */
      conv_function_designator_to_ptr_to_function(source_operand,
                                                  expression_kind);
    } else {
      /* The initialization cannot be done directly; a temporary must be
         used. */
      conversion_to_temp_done = FALSE;
      if (is_const_expr_kind(expression_kind)) {
        /* In a constant context (e.g., a nontype template argument),
           a temporary is not allowed. */
        error_in_operand(ec_init_needing_temp_not_allowed, source_operand);
        err = TRUE;
      } else if (type_is_correct_or_derived) {          
        /* The source is an rvalue but otherwise has the right type.
           Get the address of the rvalue, then cast the pointer to the right
           type to handle the derived-class case. */
        conv_operand_to_object_pointer(source_operand, expression_kind);
        /* Use a pointer type instead of a reference type on the
           destination. */
        dest_type = make_pointer_type(base_dest_type);
        cast_operand(dest_type, source_operand, expression_kind,
                     /*is_implicit_cast=*/TRUE);
      } else {
        /* Allocate a temporary and copy the operand into it, converting
           if necessary.  source_operand is set to the address of the
           temporary. */
        /* The temp has the same type as the operand, but without
           type qualifiers. */
        convert_operand_into_temp(source_operand,
                                  unqual_dest_type,
                                  expression_kind, incompatible_err, &err);
        conversion_to_temp_done = TRUE;
      }  /* if */
      if (!err) {
        if (dropping_qualifiers) {
          /* Type qualifiers were dropped. */
          error_in_operand(ec_qualifier_dropped_in_ref_init, source_operand);
          err = TRUE;
        } else if (ref_to_nonconst) {
          /* The reference must be to a const object (otherwise the user might
             change the temporary thinking he is changing the original
             object). */
          /* A reference to non-const; this is an error according to the ARM
             (8.4.3), but we allow it as an anachronism. */
          if (allow_anachronisms) {
            pos_diagnostic(anachronism_error_severity,
                           ec_nonconst_ref_init_anachronism,
                           &source_operand->position);
            if (anachronism_error_severity == es_error) {
              err = TRUE;
            } else {
              warn = TRUE;
            }  /* if */
          } else {
            /* Anachronism is not allowed. */
            /* Use a different message for the case where the type is right but
               the operand is an rvalue. */
            error_in_operand(type_is_correct_or_derived ?
                               ec_nonconst_ref_init_from_rvalue :
                               ec_bad_nonconst_ref_init,
                             source_operand);
            err = TRUE;
          }  /* if */
        }  /* if */
        if (!err && initializing_return_value) {
          /* A temporary should not be created to return a value, since
             what would happen immediately is that the address of the
             (stack-based) temporary would be returned to the caller. */
          pos_error(ec_return_ref_init_requires_temp,
                    &source_operand->position);
          err = TRUE;
        }  /* if */
        if (!err && !warn && conversion_to_temp_done) {
          /* Let the user know a temp was used. */
          pos_remark(ec_temp_used_for_ref_init, &source_operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* Normal case (not initializing a reference). */
    prep_conversion_operand(source_operand, dest_type,
                            /*result_may_be_lvalue=*/FALSE,
                            /*is_initialization=*/TRUE,
                            expression_kind, incompatible_err,
                            &source_operand->position);
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_initializer_operand */


void prep_argument_operand(an_operand         *source_operand,
                           a_param_type_ptr   formal_param,
                           an_error_code      err_code,
                           an_expression_kind expression_kind)
/*
Check that *source_operand is acceptable as an actual argument for the
formal parameter described by formal_param.  If not, issue the error err_code.
If so, convert the operand to the formal parameter type.
*/
{
  a_boolean err;

  set_arg_transfer_method_flag(formal_param);
  if (formal_param->passed_via_copy_constructor) {
    /* Argument is initialized by a copy constructor. */
    /* Allocate a temporary and convert the operand into it.  source_operand
       is set to the address of the temporary. */
    convert_operand_into_temp(source_operand, formal_param->type,
                              expression_kind, err_code, &err);
  } else {
    /* Normal argument. */
    prep_initializer_operand(source_operand, formal_param->type,
                             /*initializing_return_value=*/FALSE,
                             expression_kind, err_code);
  }  /* if */
}  /* prep_argument_operand */


void prep_return_operand(an_operand         *source_operand,
                         a_type_ptr         required_type,
                         an_expression_kind expression_kind,
                         an_error_code      err_code)
/*
Check that *source_operand is acceptable as an expression on a return
statement.  If not, issue the error err_code.  If so, convert it to the
right type.  required_type is the function return type, or an error type
if some error has been discovered.  This routine is a special case because
of the handling required when a copy constructor must be used to return
the value from a function.
*/
{
  a_routine_ptr    curr_routine = current_routine_entry();
  a_type_ptr       routine_type;
  a_variable_ptr   result_value_pointer_var;
  a_boolean        class_bitwise_copy;
  a_routine_ptr    conversion_routine;
  an_expr_node_ptr rout_node, result_value_pointer_node, arg_expr_list;
  an_expr_node_ptr node;
  an_operand       orig_operand;

  orig_operand = *source_operand;
  if (is_error_operand(source_operand) || is_error_type(required_type)) {
    /* Get rid of the error cases. */
  } else {
    routine_type = skip_typerefs(curr_routine->type);
    if (routine_type->variant.routine.extra_info->
                                   caller_provides_place_to_put_return_value) {
      /* The return value must be placed in space allocated by the caller. */
#if CHECKING
      if (!is_class_struct_union_type(required_type)) {
        internal_error("prep_return_operand: required type not class");
      }  /* if */
#endif /* CHECKING */
      result_value_pointer_var = 
                     scope_stack[depth_innermost_function_scope].il_scope->
                                 variant.routine.return_value_pointer_variable;
      /* See if the conversion is possible. */
      if (conversion_possible(source_operand, required_type,
                              /*is_initialization=*/TRUE,
                              expression_kind,
                              err_code, &source_operand->position,
                              &conversion_routine,
                              &class_bitwise_copy)) {
        /* Yes.  Determine the constructor to call and the argument list
           to use. */
        determine_ctor_for_class_init(source_operand, required_type,
                                      expression_kind,
                                      &conversion_routine, &arg_expr_list,
                                      &class_bitwise_copy);
        /* Make a node for the address of the result value. */
        result_value_pointer_node = var_rvalue_expr(result_value_pointer_var);
        if (class_bitwise_copy) {
          /* A bitwise copy can be done on the class. */
          node = make_node_from_operand(source_operand);
          result_value_pointer_node->next = node;
          /* Use an assignment that returns an lvalue, since the lvalue
             address is what we want to return. */
          node = make_operator_node((an_expr_operator_kind)eok_sassign,
                                    result_value_pointer_node->type,
                                    result_value_pointer_node);
          node->variant.operation.assignment_returns_lvalue = TRUE;
          make_expression_operand(node, node->type, source_operand);
        } else if (conversion_routine == NULL) {
          /* No appropriate copy constructor (error already issued). */
          conv_to_error_operand(source_operand);
        } else {
          /* The value is returned by calling a constructor. */
          /* Make a call of the constructor.  The value returned by a
             constructor is the "this" parameter, which is the result value
             pointer, which is just what we want. */
          /* Make a node for the address of the function. */
          rout_node = function_addr_expr(conversion_routine);
          rout_node->next = result_value_pointer_node;
          result_value_pointer_node->next = arg_expr_list;
          make_function_call(rout_node, conversion_routine->type,
                             /*is_virtual=*/FALSE,
                             &orig_operand.position, source_operand);
        }  /* if */
      }  /* if */
    } else {
      /* Normal return mechanism. */
      prep_initializer_operand(source_operand, required_type,
                               /*initializing_return_value=*/TRUE,
                               expression_kind, err_code);
    }  /* if */
  }  /* if */
  /* Restore the original source position, etc. */
  restore_operand_details(source_operand, &orig_operand);
}  /* prep_return_operand */


void prep_assignment_operand(an_operand         *source_operand,
                             a_type_ptr         dest_type,
                             an_expression_kind expression_kind,
                             an_error_code      incompatible_err,
                             a_source_position  *err_pos)
/*
Check the operand for assignment compatibility against the type supplied.
Cast the operand if required to make it the right type.  The operand is
an rvalue.  expression_kind indicates the kind of expression being scanned
(not a constant expression kind, since assignment is not valid in constant
expressions).  If the operand and type are incompatible, issue the error
incompatible_err at position *err_pos.
*/
{
  /* See if the source and destination types are compatible, and convert the
     source operand to the destination type. */
  prep_conversion_operand(source_operand, dest_type,
                          /*result_may_be_lvalue=*/FALSE,
                          /*is_initialization=*/FALSE,
                          expression_kind, incompatible_err, err_pos);
}  /* prep_assignment_operand */


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


a_type_ptr get_logical_result_type(an_expression_kind expression_kind,
				   an_operand         *operand_1,
				   an_operand         *operand_2)
/*
Return the result type for a logical expression (one that returns a logical
value of 0 or 1) with the indicated two operands.  expression_kind
indicates the type of expression being scanned.
*/
{
  a_type_ptr result_type;

  if (is_error_operand(operand_1) || is_error_operand(operand_2)) {
    result_type = error_type();
  } else if (expression_kind == (an_expression_kind)ek_pp) {
    /* All integers (logicals) have a type of long in the preprocessor. */
    result_type = integer_type((an_integer_kind)ik_long);
  } else {
    /* All logical and relational expressions have a result type of int. */
    result_type = integer_type((an_integer_kind)ik_int);
  }  /* if */

  return result_type;
}  /* get_logical_result_type */


a_boolean check_boolean_controlling_expr(an_operand *operand,
                                         a_boolean  ptr_to_member_okay)
/*
Do some checks on a boolean controlling expression (e.g., "i != 0" in 
"(i != 0) ? j : k").  Check that it's a scalar (arithmetic or pointer),
or, if ptr_to_member_okay is TRUE, a pointer to member; return FALSE if
not.  Also normalize the expression to "!= 0" form if necessary.
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
  if (ptr_to_member_okay && is_ptr_to_member_type(operand->type)) {
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


void expr_init(void)
/* 
Initialize things related to expression scanning.
*/
{
  /* Variables in exprutil.h: */
  expr_stack = NULL;
  
  /* Static variables in exprutil.c: */
  avail_xref_entries = NULL;
  curr_expr_xref_entries = NULL;
  avail_arg_operands = NULL;
  avail_candidate_functions = NULL;
  avail_arg_match_summaries = NULL;
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
