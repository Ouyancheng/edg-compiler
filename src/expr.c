/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

expr.c -- Expression scanning routines.

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
#include "decl_inits.h"
#include "disambig.h"
#include "decl_spec.h"
#include "literals.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
/* The Microsoft-specific predefined identifier __FUNCDNAME__ refers to the
   mangled name of the current function.  Hence, we may need access to the
   mangling routines. */
#include "lower_name.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
/* Needed for GNU C statement expression, ({...}). */
#include "statements.h"
#endif /* GNU_EXTENSIONS_ALLOWED */

/* Forward declarations. */
static void fix_up_dynamic_init_dtors(void);
static a_boolean cast_type_pre_check(a_type_ptr *type_cast_to,
                                     a_boolean  has_explicit_cv_qualifiers);
static void process_boolean_controlling_expression(an_operand *result,
                                                   a_boolean  validate_only);
static void scan_compound_literal(a_type_ptr               *p_literal_type,
                                  a_source_position        *type_position,
                                  an_operand               *result,
                                  a_local_expr_options_set local_options);
static void scan_expr_full(an_operand              *result,
                           an_operand              *bound_function_selector,
                           int                      prec_level,
                           a_local_expr_options_set local_options);
/* Interface to scan_expr_full for the simple case where a bound function
   cannot be returned. */
#define scan_expr(result, prec_level, local_options)                  \
  scan_expr_full((result), (an_operand *)NULL, (prec_level),          \
                 (local_options))


static a_boolean operation_has_side_effects(an_expr_node_ptr node,
                                            a_boolean        *suppress_warning)
/*
Return TRUE if the (operation) node has side effects.  Return
*suppress_warning TRUE if a warning about the expression doing nothing
should be suppressed.
*/
{
  a_boolean        has_side_effects = FALSE, suppress = FALSE;
  an_expr_node_ptr operand;
  a_type_ptr       operand_type, node_type;

  switch (node->variant.operation.kind) {
    case eok_ipost_incr:
    case eok_fpost_incr:
    case eok_ppost_incr:
    case eok_ipost_decr:
    case eok_fpost_decr:
    case eok_ppost_decr:
    case eok_ipre_incr:
    case eok_fpre_incr:
    case eok_ppre_incr:
    case eok_ipre_decr:
    case eok_fpre_decr:
    case eok_ppre_decr:
    case eok_iassign:
    case eok_fassign:
    case eok_passign:
    case eok_sassign:
    case eok_bassign:
    case eok_pmassign:
    case eok_imultiply_assign:
    case eok_fmultiply_assign:
    case eok_idivide_assign:
    case eok_fdivide_assign:
    case eok_remainder_assign:
    case eok_iadd_assign:
    case eok_fadd_assign:
    case eok_padd_assign:
    case eok_isubtract_assign:
    case eok_fsubtract_assign:
    case eok_psubtract_assign:
    case eok_shiftl_assign:
    case eok_shiftr_assign:
    case eok_and_assign:
    case eok_or_assign:
    case eok_xor_assign:
#if C99_IL_EXTENSIONS_SUPPORTED
    case eok_xassign:
    case eok_xmultiply_assign:
    case eok_xdivide_assign:
    case eok_xadd_assign:
    case eok_xsubtract_assign:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case eok_call:
    case eok_virtual_call:
    case eok_pm_call:
    case eok_va_start:
    case eok_va_arg:
    case eok_va_end:
    case eok_va_copy:
    case eok_post_incr:
    case eok_post_decr:
    case eok_pre_incr:
    case eok_pre_decr:
    case eok_assign:
    case eok_add_assign:
    case eok_subtract_assign:
    case eok_multiply_assign:
    case eok_divide_assign:
    case eok_generic_call:
    case eok_generic_member_call:
      /* These all cause side effects. */
      has_side_effects = TRUE;
      break;
    case eok_extract_bit_field:
      /* A bit field extraction causes a side effect if the bit field is
         volatile.  This has to be tested separately because the volatile
         qualifier doesn't appear in the result type (it's an rvalue). */
      operand = node->variant.operation.operands->next;
      check_assertion(operand->kind == (an_expr_node_kind)enk_field);
      if (is_volatile_qualified_type(operand->variant.field->type)) {
        has_side_effects = TRUE;
        break;
      }  /* if */
      /* Go test whether the struct is volatile. */
      goto first_op_volatile_test;
    case eok_indirect:
    case eok_subscript:
first_op_volatile_test:
      /* Causes a side effect if the type of the thing pointed to
         is volatile. */
      /* Note that we test the pointer operand's type, not the node type,
         because of an IL shorthand that allows omission of the cast to the
         unqualified version of the type. */
      operand_type = node->variant.operation.operands->type;
      if (is_pointer_type(operand_type)) {
        a_type_ptr underlying_type = type_pointed_to(operand_type);
        has_side_effects = is_volatile_qualified_type(underlying_type);
      }  /* if */
      break;
    case eok_vacuous_destructor_call:
    case eok_value_vacuous_destructor_call:
      /* A vacuous destructor call like
           p->int::~int();
         is an expression that intentionally does nothing, so suppress
         the warning. */
      suppress = TRUE;
      break;
    case eok_dynamic_cast:
      /* A dynamic_cast to a reference to a polymorphic class type can throw
         an exception. */
      node_type = node->type;
      if (is_reference_type(node_type) &&
          is_polymorphic_class_type(type_pointed_to(node_type))) {
        has_side_effects = TRUE;
      }  /* if */
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:
      /* __assume(expr) intentionally does nothing, so suppress the
         warning. */
      suppress = TRUE;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:;
  }  /* switch */

  /* For the operations that do not cause side effects, check the operands for
     side effects. */
  for (operand = node->variant.operation.operands;
       operand != NULL && !has_side_effects;
       operand = operand->next) {
    a_boolean local_suppress;
    has_side_effects = node_has_side_effects(operand, &local_suppress);
    suppress |= local_suppress;
  }  /* for */

  *suppress_warning = suppress;
  return has_side_effects;
}  /* operation_has_side_effects */


a_boolean node_has_side_effects(an_expr_node_ptr node,
                                a_boolean        *suppress_warning)
/*
Return TRUE if the expression node has side effects.  Return
*suppress_warning TRUE if a warning about the expression doing nothing
should be suppressed.  If suppress_warning == NULL, it is not set.
*/
{
  a_boolean has_side_effects = FALSE, suppress = FALSE;

  switch (node->kind) {
    case enk_error:
      /* Who knows what an error node might have done -- suppress the
         warning. */
      has_side_effects = TRUE;
      suppress = TRUE;
      break;
    case enk_constant:
      if (is_error_constant(node->variant.constant)) {
        /* An error constant might have been anything -- suppress the
           warning. */
        has_side_effects = TRUE;
        suppress = TRUE;
      }  /* if */
      break;
    case enk_variable_address:
    case enk_routine_address:
    case enk_field:
    case enk_address_of_ellipsis:
      /* No side effects. */
      break;
    case enk_operation:
      has_side_effects = operation_has_side_effects(node, &suppress);
      break;
    case enk_variable:
      /* Note that we test the variable's type, not the node type, because of
         an IL shorthand that allows omission of the cast to the unqualified
         version of the type. */
      has_side_effects =
                      is_volatile_qualified_type(node->variant.variable->type);
      break;
    case enk_temp_init:
      /* At the very least, this has the side effect of initializing
         something.  It might also call a constructor, etc.  In C99
         mode, enk_temp_init is used for compound literals, which
         can be considered not to be side effects. */
      if (!c99_mode) {
        has_side_effects = TRUE;
      } else {
        has_side_effects = dynamic_init_has_side_effects(
                                               node->variant.init.dynamic_init,
                                               &suppress);
      }  /* if */
      break;
    case enk_condition:
      /* At the very least, this has the side effect of initializing
         something.  It might also call a constructor, etc. */
      has_side_effects = TRUE;
      break;
    case enk_new_delete:
      /* A new or delete always has a side effect. */
      has_side_effects = TRUE;
      break;
    case enk_throw:
      /* A throw always has side effects. */
      has_side_effects = TRUE;
      break;
    case enk_object_lifetime:
      has_side_effects = node_has_side_effects(
                                            node->variant.object_lifetime.expr,
                                            &suppress);
      break;
    case enk_typeid:
      if (node->variant.typeid_info.expr != NULL) {
        has_side_effects = node_has_side_effects(
                                            node->variant.typeid_info.expr,
                                            &suppress);
        /* A typeid applied to an expression that is a pointer to a
           polymorphic class type can throw an exception if the pointer is
           NULL. */
        if (is_polymorphic_class_type(node->variant.typeid_info.type)) {
          has_side_effects = TRUE;
        }  /* if */
      }  /* if */
      break;
    case enk_runtime_sizeof:
      if (!node->variant.runtime_sizeof.is_type) {
        has_side_effects = node_has_side_effects(
                                     node->variant.runtime_sizeof.variant.expr,
                                     &suppress);
      }  /* if */
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      /* Assume a statement has side effects. */
      has_side_effects = TRUE;
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
    /* Nodes generated by IL lowering for partial lowering of exception
       handling features. */
    case enk_lowered_eh_construct:
      has_side_effects = TRUE;
      break;
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
      /* Node generated as part of the body of an entry function used
         as a wrapper for a call of an overriding virtual function
         with a covariant return type. */
      /* Probably not expected, but give the safe answer just in case. */
      has_side_effects = TRUE;
      break;
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    default:
      unexpected_condition_str("node_has_side_effects: bad node kind");
  }  /* switch */

  if (!has_side_effects && !C_mode() && in_front_end &&
      is_template_dependent_context() &&
      is_template_dependent_type(node->type)) {
    /* A node with a template parameter type is considered to have
       side effects.  This is because it's possible that when the type
       is actually known an overloaded operator function would be chosen,
       which would mean a function call. */
    has_side_effects = TRUE;
  }  /* if */
  if (suppress_warning != NULL) *suppress_warning = suppress;
  return has_side_effects;
}  /* node_has_side_effects */


a_boolean is_invariant_expr(an_expr_node_ptr expr,
                            a_boolean        vars_can_change)
/*
Return TRUE if the indicated expression is invariant, meaning it has no
side effects and will give the same value if evaluated more than once.
vars_can_change indicates whether the values of variables should be
considered to be changeable between successive evaluations for purposes
of this determination.
*/
{
  a_boolean is_invariant = FALSE;

  if (vars_can_change) {
    /* For the vars_can_change case, do a crude analysis: if the expression
       is constant, it cannot be affected by changes in the values of
       variables.  This could be improved, but it probably doesn't matter. */
    if (is_constant_node(expr) || is_variable_address_node(expr) ||
        is_routine_address_node(expr)) {
      is_invariant = TRUE;
    } else if (is_variable_node(expr) &&
               expr->variant.variable->source_corresp.name == NULL) {
      /* An unnamed variable is a temporary.  Assume that such a thing is
         not changed in the "vars_can_change" mode.  This is important,
         because if the expression has been assigned to a temporary once,
         we want to use that temporary directly on subsequent calls to
         make_reusable_copy. */
      is_invariant = TRUE;
    }  /* if */
  } else {
    /* Variables cannot change.  See if the expression has side effects. */
    if (!node_has_side_effects(expr, (a_boolean *)NULL)) is_invariant = TRUE;
  }  /* if */
  return is_invariant;
}  /* is_invariant_expr */


static void simplify_void_node(an_expr_node_ptr *node_ptr,
                               a_boolean        *suppress_warning)
/*
The expression node pointed to by *node_ptr has been scanned as a void
expression.  Examine it to see if it can be simplified by removing parts
that do nothing.  Change *node_ptr to point to the simplified expression
tree.  Return *suppress_warning == TRUE if the node has some side effect or
if it is something that has no effect but for which a warning should not
be issued.
*/
{
  an_expr_node_ptr node = *node_ptr, check_node;
  a_boolean        suppress = FALSE, any_commas = FALSE;

  /* This routine could do various kinds of pruning -- in fact, it used to;
     however, in accord with the philosophy that the front end does no
     optimization, it now only removes an unnecessary top-level cast to
     void. */
  check_node = node;
  for (;;) {
    /* Check for an explicit cast-to-void node, remove the node, and
       suppress the warning about a node with no effect in that case.
       This is because we assume that a programmer who casts something
       to void is doing so for some good reason, and also because the
       macro for "assert" expands to a (void)0 when NDEBUG is defined. */
    if (is_operation_node(check_node) &&
         check_node->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast &&
         is_void_type(check_node->type)) {
      /* This is a cast to void; suppress the warning. */
      suppress = TRUE;
      check_node = check_node->variant.operation.operands;
      /* If this cast is at the top (not under a comma expression), remove
         it. */
      if (!any_commas) node = check_node;
      break;
    } else if (is_operation_node(check_node) &&
               check_node->variant.operation.kind ==
                                            (an_expr_operator_kind)eok_comma) {
      /* For a comma node, the check for side effects was already done
         on the first operand when it was scanned (and a warning issued
         if appropriate), so do not repeat that test.  Just check the
         second operand.  This allows use of a (void) cast on any
         operand of a comma expression to suppress the warning, e.g.,
         ((void)0, (void)0). */
      /* If the first operand has side effects, the whole operation has
         side effects, so suppress the warning on the whole operation. */
      if (node_has_side_effects(check_node->variant.operation.operands,
                                &suppress)) {
        suppress = TRUE;
        break;
      }  /* if */
      check_node = check_node->variant.operation.operands->next;
      any_commas = TRUE;
    } else {
      /* Not a cast to void or a comma operator, so exit the loop. */
      break;
    }  /* if */
  }  /* for */
  /* See if the node has some effect. */
  if (!suppress) {
    if (node_has_side_effects(check_node, &suppress)) suppress = TRUE;
  }  /* if */
  *suppress_warning = suppress;
  /* Put the possibly updated pointer back into *node_ptr. */
  *node_ptr = node;
}  /* simplify_void_node */


static void do_void_operand_transformations(an_operand *operand)
/*
Do whatever transformations are appropriate on a void expression operand,
e.g., lvalue-to-rvalue in C, not in C++.
*/
{
  a_transformation_options_set options = TOPT_NO_OPTIONS;

  if (!C_mode()) {
    /* In C++, lvalue-to-rvalue transformations are not done on an expression
       scanned as a void expression. */
    options |= (TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION);
  }  /* if */
  do_operand_transformations(operand, options);
}  /* do_void_operand_transformations */


static void simplify_void_operand(an_operand *operand)
/*
Examine the operand given by *operand, which has been scanned as a void
expression, and simplify it if possible by removing parts that do nothing.
Issue a warning if the operand has no effect.  Lvalue-to-rvalue
transformations are done if appropriate (yes in C, no in C++).  Other
transformations are done in all cases.
*/
{
  a_boolean suppress_warning = FALSE;

  /* Do lvalue-to-rvalue transformations, etc. as appropriate. */
  do_void_operand_transformations(operand);
  if (!is_expression_operand(operand)) {
    /* An operand that is not an expression cannot have side effects.
       For error operands, assume that the original form might have had
       an effect, and suppress the warning.  Likewise for template-dependent
       constants. */
    if (is_error_operand(operand)) {
      suppress_warning = TRUE;
    } else if (is_constant_operand(operand) &&
               operand->variant.constant.kind ==
                                     (a_constant_repr_kind)ck_template_param) {
      suppress_warning = TRUE;
    }  /* if */
  } else {
    /* For an expression, traverse the tree to see if it has side effects
       and to simplify it. */
    simplify_void_node(&operand->variant.expression, &suppress_warning);
  }  /* if */
  if (!suppress_warning) {
    /* Give a warning on an expression that has no effect. */
    pos_warning(ec_expr_has_no_effect, &operand->position);
  }  /* if */
}  /* simplify_void_operand */


static a_boolean token_ends_expr(a_token_kind             token,
                                 int                      prec_level,
                                 a_local_expr_options_set local_options)
/*
Return TRUE if the given token is not part of an expression being scanned
at precedence level prec_level.  Conversely, return FALSE if the given
token is an operator that binds tightly enough that it's part of the
current expression.  local_options is the set of options for the
current expression (used to decide how a comma should be treated).
*/
{
  a_boolean done;
  a_boolean new_assoc;
  int       new_prec;

  done = FALSE;
  new_assoc = LEFT_ASSOC;  /* More common case. */
  switch (token) {
    case tok_plus_plus:/* Postfix increment. */
    case tok_minus_minus:/* Postfix decrement. */
    case tok_lbracket: /* Subscript. */
    case tok_lparen:   /* Routine call. */
    case tok_period:   /* Field selector ".". */
    case tok_arrow:    /* Field selector "->". */
      new_prec = PREC_POSTFIX;
      break;
    case tok_arrow_star: /* ->* */
    case tok_period_star: /* .* */
      new_prec = PREC_PTR_TO_MEMBER;
      break;
    case tok_star:
    case tok_divide:
    case tok_remainder:
      new_prec = PREC_MULT_DIV;
      break;
    case tok_plus:
    case tok_minus:
      new_prec = PREC_PLUS_MINUS;
      break;
    case tok_shift_left:
    case tok_shift_right:
      new_prec = PREC_SHIFT;
      break;
    case tok_gt:
      /* ">" can be the end of a template argument list: A<int, 2> */
      /* It's not if there is a set of parentheses or the like inside the
         template argument expression, since the ">" is then not top-level. */
      if (expr_stack->is_template_arg_expression &&
          expr_stack->nested_construct_depth == 0) {
        done = TRUE;
      }  /* if */
      /* Not the end of a template argument list, so fall into the normal
         case. */
    case tok_lt:
    case tok_le:
    case tok_ge:
      new_prec = PREC_RELATIONAL;
      break;
    case tok_eq:
    case tok_ne:
      new_prec = PREC_EQ_NE;
      break;
    case tok_ampersand:
      new_prec = PREC_AND;
      break;
    case tok_excl_or:
      new_prec = PREC_EXCL_OR;
      break;
    case tok_or:
      new_prec = PREC_OR;
      break;
    case tok_and_and:
      new_prec = PREC_AND_AND;
      break;
    case tok_or_or:
      new_prec = PREC_OR_OR;
      break;
    case tok_quest_mark:
      new_prec = PREC_QUEST_MARK;
      new_assoc = RIGHT_ASSOC;
      break;
    case tok_assign:
    case tok_times_assign:
    case tok_divide_assign:
    case tok_remainder_assign:
    case tok_plus_assign:
    case tok_minus_assign:
    case tok_shift_left_assign:
    case tok_shift_right_assign:
    case tok_and_assign:
    case tok_excl_or_assign:
    case tok_or_assign:
      /* Note that assignment operators are recognized but not legal in
         constant expressions.  This produces clearer error messages. */
      new_prec = PREC_ASSIGNMENT;
      new_assoc = RIGHT_ASSOC;
      break;
    case tok_comma:
      /* Note that the comma operator is recognized but not legal in
          constant expressions.  This produces clearer error messages. */
      if (local_options & EOPT_DISALLOW_COMMA_OPERATOR) {
	/* The comma operator is not recognized here, because it means
           something else in this context (e.g., in a function argument
           list). */
	done = TRUE;
      } else {
        new_prec = PREC_COMMA;
      }  /* if */
      break;

    default:
      /* Not an operator; the expression ends. */
      done = TRUE;
  }  /* switch */

  /* See if the new operator precedence is such that the operator is not
     part of the current expression.  This is the case if the
     new operator has lower precedence than prec_level, or if the
     precedences are equal and the operator is left-associative.  End
     the expression if one of these conditions is met. */
  if (done || new_prec < prec_level ||
      (new_prec == prec_level && new_assoc == LEFT_ASSOC)) done = TRUE;
  return done;
}  /* token_ends_expr */


/*
Macro to record source positions in an_operand at the end of scanning
an expression.  result is the result operand.  start_pos and end_pos
give the beginning and ending source positions.  end_pos is used
only if EXTRA_SOURCE_POSITIONS_IN_IL is TRUE.  This macro does not set
the position in the underlying expression, if any (see set_operand_position).
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define set_base_operand_position(result, start_pos, end_pos) \
  { error_position = (result)->position = *(start_pos); \
    curr_construct_end_position = (result)->end_position = *(end_pos); \
  }
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define set_base_operand_position(result, start_pos, end_pos) \
  { error_position = (result)->position = *(start_pos); }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if EXTRA_SOURCE_POSITIONS_IN_IL

static void f_set_operand_position(an_operand        *result,
                                   a_source_position *start_pos,
                                   a_source_position *end_pos,
                                   a_source_position *operator_pos)
/*
Record the source position in an_operand at the end of scanning
an expression.  result is the result operand.  start_pos and end_pos
give the beginning and ending source positions.  operator_pos gives
the operator position; the pointer can be NULL if there is no operator
position.  Global variables error_position and curr_construct_end_position
are set appropriately.
*/
{
  set_base_operand_position(result, start_pos, end_pos);
  /* If the operand is an expression, record positions in the expression
     itself. */
  set_operand_expr_position_if_expr(result, operator_pos);
}  /* f_set_operand_position */

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

/*
Macro to record source positions in an_operand at the end of scanning
an expression.  result is the result operand.  start_pos and end_pos
give the beginning and ending source positions.  operator_pos gives
the operator position.  Some of these are used only if
EXTRA_SOURCE_POSITIONS_IN_IL is TRUE.
*/
#if EXTRA_SOURCE_POSITIONS_IN_IL
#define set_operand_position(result, start_pos, end_pos, operator_pos) \
  f_set_operand_position(result, start_pos, end_pos, operator_pos)
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
#define set_operand_position(result, start_pos, end_pos, operator_pos) \
  set_base_operand_position(result, start_pos, end_pos)
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */


static a_boolean is_overloadable_type_operand(an_operand *operand)
/*
Return TRUE if the given operand has a type for which operator overloading
should be considered.  Also return TRUE for template-dependent operands
in a prototype instantiation, because they might be overloadable (and
we want to go to check_for_operator_overloading to handle that).
*/
{
  /* Note that we check for all dependent types and not just
     ones that could be class or enum types.  That allows us to generate
     a generic operation in check_for_operator_overloading and
     avoid testing for template cases in each place that calls it. */
  a_boolean is_overloadable = is_error_operand(operand) ||
                              is_class_struct_union_type(operand->type) ||
                              (operator_overloading_on_enums_enabled &&
                               is_enum_type(operand->type)) ||
                              (is_template_dependent_context() &&
                               is_template_dependent_type(operand->type));
  return is_overloadable;
}  /* is_overloadable_type_operand */


static void scan_subscript_operator(an_operand *operand_1,
                                    an_operand *result)
/*
Scan array subscripting.  See section 3.3.2.1 of the standard.

Syntax:
	pointer-expression [ integral-expression ]
	integral-expression [ pointer-expression ]
*/
{
  an_operand         operand_2, operand_temp;
  a_type_ptr         result_type;
  a_source_position  operator_position;
  a_boolean          operands_have_been_reversed = FALSE;
  a_token_sequence_number
                     operator_tok_seq_number;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position  end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean          err = FALSE, processed = FALSE;

  db_enter(4, "scan_subscript_operator");

  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is(ek_pp)) {
    /* Subscripting not allowed in preprocessing expression. */
    pos_error(ec_bad_pp_operator, &operator_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Subscripting not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &operator_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Subscripting not allowed in a template argument expression. */
    pos_error(ec_bad_templ_arg_expr_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Get past the opening bracket. */
  (void)get_token();
  add_matching_stop_token(tok_rbracket);

  /* Scan the second operand. */
  scan_expr(&operand_2, PREC_LOWEST, EOPT_NO_OPTIONS);

  if (err) {
    /* Subscripting is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    operand_will_not_be_used_because_of_error(&operand_2);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode &&
             is_property_ref_operand(operand_1)) {
    /* The operand is a field selection for a field declared with the
       Microsoft C++ extension __declspec(property(...)).  Add the
       subscript expression to the operand.  It will be included as
       an argument in the call of a "get" or "put" function when this
       operand is rewritten later. */
    an_arg_operand_ptr last_subscript;
    an_arg_operand_ptr subscript = alloc_arg_operand();

    subscript->operand = operand_2;
    /* Attach the arg_operand for the subscript to the end of the existing
       list of subscripts (if any). */
    last_subscript = operand_1->variant.property_ref.subscripts;
    if (last_subscript == NULL) {
      operand_1->variant.property_ref.subscripts = subscript;
    } else {
      while (last_subscript->next != NULL) {
        last_subscript = last_subscript->next;
      }  /* while */
      last_subscript->next = subscript;
    }  /* if */
    *result = *operand_1;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_overloadable_type_operand(operand_1) ||
         is_overloadable_type_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_subscript,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      /* One of the operands must have type "pointer to object type" and the 
         other must be an integral expression.  See section 3.3.2.1 of the
         standard.  If the first operand is integral or enum, switch them. */
      if (is_integral_or_enum_type(operand_1->type)) {
        /* The subscript value is outside the brackets and the pointer value is
           inside the brackets.  Switch them. */
        copy_operand(operand_1, &operand_temp);
        copy_operand(&operand_2, operand_1);
        copy_operand(&operand_temp, &operand_2);
        /* Remember that we did this so that we can adjust the position
           information later on. */
        operands_have_been_reversed = TRUE;
      }  /* if */

      /* The first operand must be a pointer to object. */
      if (gcc_mode && is_pointer_type(operand_1->type) &&
                      is_void_type(type_pointed_to(operand_1->type))) {
        /* In some versions of GNU C a pointer to "void" can be subscripted. */
        pos_warning(ec_nonobject_pointer_arithmetic, &operator_position);
        result_type = type_pointed_to(operand_1->type);
      } else if (
#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
          /* Pointer to incomplete array is also allowed. */
          check_object_or_incomp_array_pointer_operand(operand_1,
                                                 ec_expr_not_pointer_to_object,
                                                       &operand_2)
#else /* !PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
          check_object_pointer_operand(operand_1,
                                       ec_expr_not_pointer_to_object)
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
                                                                            ) {
        result_type = type_pointed_to(operand_1->type);
      } else {
        result_type = error_type();
      }  /* if */

      /* The subscript must be integral or enum. */
      (void)check_integral_or_enum_operand(&operand_2);

      /* Build the expression.  The order of the operands is pointer and
         then the subscript, regardless of the original order of the two. */
      /* Note that the integral promotions are NOT done on the subscript;
         this is as the standard wants it (they are to be done only where
         indicated, and 3.3.2.1 does not say to do them). */
      do_binary_operation((an_expr_operator_kind)eok_padd_subsc,
                          operand_1, &operand_2, operand_1->type, result,
                          &operator_position);
      /* This is an lvalue; the expression or constant giving the address
         has type pointer-to-X, but the operand has type X. */
      if (!is_error_operand(result)) {
        result->type = result_type;
        result->state = (an_operand_state)os_lvalue;
        /* Preserve the reference entries from the first operand (the
           array). */
        result->ref_entries_list = operand_1->ref_entries_list;
      }  /* if */
    }  /* if */
  }  /* if */

#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Save the position of the "]". */
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_matching_stop_token(tok_rbracket);

  set_operand_position(
            result,
            &(operands_have_been_reversed ? &operand_2 : operand_1)->position,
            &end_position, &operator_position);

  db_exit();
}  /* scan_subscript_operator */


static void scan_call_arguments(a_type_ptr         function_type,
                                a_routine_ptr	   routine,
                                a_boolean          already_after_left_paren,
                                an_expr_node_ptr   *p_argument_list,
                                a_boolean          overloaded_function_case,
                                a_boolean          unknown_dependent_function,
                                an_arg_operand_ptr *arg_operand_list,
                                a_source_position  *closing_paren_position)
/*
Scan the arguments of a function call and return a list of argument
expressions in *p_argument_list.  The type of the function being called is
given by function_type; function_type is NULL if the type is not known,
or for an overloaded function case.  unknown_dependent_function is TRUE
if the function to be called is not known because it is specified by
a template-dependent expression.  The current token at the time of
call is the opening "(" of the argument list, unless already_after_left_paren
is TRUE, in which case it is the token following the left parenthesis (but
the add_stop_token call has not been done).  On return, the current token
is the token following the closing ")".  If overloaded_function_case is
TRUE, this call is scanning the arguments for a call of an overloaded
function, so build an argument operand list and return a pointer to it in
*arg_operand_list.  routine points to the routine being called; it's NULL
if the specific function being called is not known, e.g., when
overloaded_function_case is TRUE or when calling through a pointer.
If closing_paren_position is non-NULL, *closing_paren_position is set to
the source position of the closing parenthesis of the call.
*/
{
  an_operand         argument_operand;
  an_arg_operand_ptr end_arg_operand_list, arg_operand;
  an_arg_check_block arg_block;

  db_enter(4, "scan_call_arguments");
  if (overloaded_function_case) {
    /* Overloaded function.  We don't know anything about the type of
       function being called. */
    function_type = NULL;
    /* Start with an empty list of argument operands. */
    *arg_operand_list = NULL;
    end_arg_operand_list = NULL;
  }  /* if */
  /* Set the block used for checking argument types. */
  start_call_argument_processing(function_type, routine, &arg_block);
  if (unknown_dependent_function) {
    /* The function to be called is unknown because it's template-dependent. */
    check_assertion(!overloaded_function_case && function_type == NULL);
    arg_block.unknown_dependent_function = TRUE;
  } /* if */

  if (!already_after_left_paren) {
    /* Get past the opening parenthesis. */
    (void)get_token();
  }  /* if */
  /* Add ")" as a stop token. */
  add_matching_stop_token(tok_rparen);

  /* Check for an empty argument list. */
  if (curr_token != tok_rparen) {
    add_stop_token(tok_comma);
    /* Scan a comma-separated list of arguments. */
    do {
      /* In cfront mode, allow an extra comma at the end of the argument
         list. */
      if (any_cfront_mode() && curr_token == tok_rparen) break;
      /* Scan an argument expression.  Note that it is not converted to an
         rvalue yet. */
      scan_expr(&argument_operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
      if (overloaded_function_case) {
        /* For the overloaded function case, we do not know yet what the
           parameter type is, so save it as is.  The lvalue to rvalue
           and prototyped parameter conversions will be done once the
           specific function is identified (see select_overloaded_function). */
        /* Add an entry to the argument operand list. */
        arg_operand = alloc_arg_operand();
        copy_operand(&argument_operand, &arg_operand->operand);
        if (*arg_operand_list == NULL) {
          *arg_operand_list = arg_operand;
        } else {
          end_arg_operand_list->next = arg_operand;
        }  /* if */
        end_arg_operand_list = arg_operand;
      } else {
        /* Check the argument type against the parameter type and convert
           if necessary.  Add it to the list of argument expressions. */
        process_call_argument(&argument_operand, &arg_block);
      }  /* if */
    } while (loop_token(tok_comma));
    remove_stop_token(tok_comma);
  }  /* if */
  /* End of argument list. */
  set_err_pos_to_curr_token();
  arg_block.closing_paren_position = pos_curr_token;
  if (closing_paren_position != NULL) *closing_paren_position = pos_curr_token;
  if (!overloaded_function_case) {
    /* Do processing for the end of the argument list. */
    process_end_of_call_arguments(&arg_block);
  }  /* if */
  /* Check for the closing paren. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  /* Return argument list pointer to caller. */
  *p_argument_list = arg_block.argument_head;
  db_exit();
}  /* scan_call_arguments */


static void scan_dependent_parenthesized_initializer(a_dynamic_init_ptr *dip)
/*
Scan a parenthesized list of expressions that is the initializer of
an entity of a template-dependent type.  Build a dynamic initialization
entry for the initialization and return a pointer to it in *dip.
On entry, the current token is the one following the opening parenthesis.
On return, the current token is the one following the closing parenthesis.
*/
{
  an_expr_node_ptr  arg_list;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  /* Scan the argument list. */
  scan_call_arguments((a_type_ptr)NULL, (a_routine_ptr)NULL,
                      /*already_after_left_paren=*/TRUE,
                      &arg_list, /*overloaded_function_case=*/FALSE,
                      /*unknown_dependent_function=*/TRUE,
                      (an_arg_operand_ptr *)NULL, (a_source_position *)NULL);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Set the dynamic init entry to represent "constructor" initialization,
     leaving the constructor pointer NULL. */
  *dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_constructor);
  (*dip)->variant.constructor.ptr = NULL;
  (*dip)->variant.constructor.args = arg_list;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* scan_dependent_parenthesized_initializer */


void check_closing_paren_after_expr_list(void)
/*
Check for the required closing parenthesis after an expression list, and
pass over it if it is found.  This is similar to required_token, but it
does some special error-recovery processing to handle additional
unexpected expressions more gracefully.
*/
{
  a_token_set_array_element save_comma_stop_token_count;
  a_token_set_array_element *comma_entry_ptr;

  /* Remove comma from the stop tokens set. */
  comma_entry_ptr = &(curr_stop_token_stack_entry->
                                                stop_tokens[(int)tok_comma]);
  save_comma_stop_token_count = *comma_entry_ptr;
  *comma_entry_ptr = 0;
  (void)required_token(tok_rparen, ec_exp_rparen);
  /* Restore comma as a stop token (if it was one). */
  *comma_entry_ptr = save_comma_stop_token_count;
}  /* check_closing_paren_after_expr_list */


static an_expr_node_ptr scan_parenthesized_initializer_expression(
                                            a_type_ptr         dest_type,
                                            an_error_code      err_code)
/*
Scan a single expression in parentheses as an initializer value, and convert
it to dest_type if necessary.  The current token is the token after the
opening left parenthesis.  On return, the current token is the token following
the closing parenthesis.  If the conversion cannot be done, issue the
error err_code.  The entity being initialized is assumed not to be a
variable.
*/
{
  an_expr_node_ptr  expr;
  an_operand        result;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  add_matching_stop_token(tok_rparen);
  /* Since the syntax has an expression-list even in the single-expression
     case, a top-level comma is not allowed. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, dest_type, (a_boolean *)NULL,
                           (a_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           /*initializing_variable=*/FALSE,
                           /*static_lifetime=*/FALSE,
                           /*is_copy_initialization=*/FALSE,
                           /*processed_arg=*/FALSE,
                           /*nontype_template_arg=*/FALSE,
                           err_code);
  /* Check for the required closing parenthesis. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  check_closing_paren_after_expr_list();
  remove_matching_stop_token(tok_rparen);
  expr = make_node_from_operand(&result);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return expr;
}  /* scan_parenthesized_initializer_expression */


static void scan_ctor_arguments(a_symbol_ptr       constructor_sym,
                                an_expr_node_ptr   *arg_expr_list,
                                a_routine_ptr      *conversion_routine,
                                a_boolean          *unknown_dependent_function,
                                a_source_position  *source_pos,
                                a_type_ptr         object_class_type)
/*
Scan the argument list for a C++ constructor call.  The current token is
the one right after the opening parenthesis of the argument list.  The
constructor symbol (possibly overloaded) is constructor_sym.
Scan the arguments and the closing parenthesis, and return the
argument list in *arg_expr_list and a pointer to the proper constructor
routine in *conversion_routine.  If the proper constructor cannot be
determined, return NULL.  *unknown_dependent_function is returned TRUE
if the proper constructor could not be determined because one or more
of the arguments has a template-dependent type (in a prototype
instantiation).  *source_pos indicates the source position of the call.
This routine may be called only in C++ mode.  It's used for
parenthesis-enclosed initializers for classes that have constructors,
as in

  class A {...};
  A x(1, 2, 3);

The caller need not add the right parenthesis to the stop tokens set, or
remove it later, as this routine takes care of that.  object_class_type
is the type of the object being constructed, which may be different than
the type of the constructor being called (e.g., when a base class constructor
is being called for a derived class object).  On return, the source position
is after the closing parenthesis of the argument list.
*/
{
  a_boolean           overloaded_function_case = FALSE;
  a_routine_ptr       routine;
  a_type_ptr          routine_type;
  a_source_position   start_position;
  an_arg_operand_ptr  arg_operand_list;
  an_arg_match_summary_ptr
                      arg_match_list;

  db_enter(4, "scan_ctor_arguments");
  *conversion_routine = NULL;
  *unknown_dependent_function = FALSE;
  start_position = pos_curr_token;
  if (constructor_sym->kind == (a_symbol_kind)sk_member_function) {
    /* Constructor is not overloaded.  In this case, the argument types
       can be checked as the argument list is scanned. */
    routine_type = routine_symbol_type(constructor_sym);
    routine = constructor_sym->variant.routine.ptr;
  } else {
    check_assertion_str(
              constructor_sym->kind == (a_symbol_kind)sk_overloaded_function ||
              constructor_sym->kind == (a_symbol_kind)sk_function_template,
              "scan_ctor_arguments: sym not function");
    /* Constructor is overloaded or a template. */
    overloaded_function_case = TRUE;
    routine_type = NULL;
    routine = NULL;
  }  /* if */

  /* Scan the arguments. */
  scan_call_arguments(routine_type, routine,
                      /*already_after_left_paren=*/TRUE,
                      arg_expr_list, overloaded_function_case,
                      /*unknown_dependent_function=*/FALSE,
                      &arg_operand_list, (a_source_position *)NULL);
  error_position = start_position;

  if (overloaded_function_case) {
    /* The constructors are overloaded.  Select the proper one. */
    /* Note that a special case allows passing have_selector == TRUE and
       NULL for the selector operand when dealing with constructors. */
    constructor_sym = select_overloaded_function(constructor_sym,
                                                 /*is_template_id=*/FALSE,
                                                 (a_template_arg_ptr)NULL,
                                                 /*have_selector=*/TRUE,
                                                 (an_operand *)NULL,
                                                 arg_operand_list,
                                                 /*do_arg_dep_lookup=*/FALSE,
                                                 ec_no_matching_constructor,
                                                 ec_ambiguous_constructor,
                                                 &start_position,
                                                 (a_token_sequence_number)0,
                                                 (a_boolean *)NULL,
                                                 unknown_dependent_function,
                                                 (a_symbol_ptr *)NULL,
                                                 &arg_match_list);
    /* Build an expression-form argument list.  Convert the arguments on
       the argument list to the right types.  The call is done even
       when constructor_sym is NULL because it also frees arg_operand_list
       and arg_match_list. */
    /* Again, note that a special case allows passing have_selector == TRUE and
       NULL for the selector operand when dealing with constructors. */
    adjust_overloaded_function_call_arguments(constructor_sym,
                                              *unknown_dependent_function,
                                              (a_type_ptr)NULL,
                                              /*have_selector=*/TRUE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              arg_match_list,
                                              arg_expr_list);
  }  /* if */
  if (constructor_sym != NULL) {
    /* Check that the constructor is accessible and mark it referenced. */
    expr_reference_to_implicitly_invoked_function(constructor_sym,
                                                  source_pos,
                                                  object_class_type,
                                                  /*honor_virtual=*/FALSE);
    *conversion_routine = constructor_sym->variant.routine.ptr;
  }  /* if */
  db_exit();
}  /* scan_ctor_arguments */


static void scan_function_call(an_operand *operand,
                               an_operand *bound_function_selector,
			       an_operand *result)
/*
Scan a function call.  The function to be called is given by *operand,
modified by *bound_function_selector if the function is bound.  Even
if the function is not bound, bound_function_selector points to an operand
that can be filled in if an implicit selector is generated.
On return, *result is set to an operand for the entire call.
See section 3.3.2.2 of the standard.

Syntax:
	pointer-to-function-expression ( argument-expression-list    )
								 opt
*/
{
  an_expr_node_ptr  argument_list;
  a_type_ptr        routine_type = NULL;
  a_symbol_ptr      overloaded_function_symbol = NULL;
  a_boolean         overloaded_function_case = FALSE;
  a_boolean         vacuous_destructor_case = FALSE;
  a_source_position call_position, function_position, first_arg_position;
  a_source_position start_position, closing_paren_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position operator_position, end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_arg_match_summary
                    this_match_summary;
  an_arg_operand_ptr
                    arg_operand_list;
  a_routine_ptr     routine = NULL;
  a_boolean         already_after_left_paren = FALSE;
  an_expr_operator_kind
                    op;
  a_boolean         try_surrogate_functions = FALSE;
  a_token_sequence_number
                    opening_paren_tok_seq_number;
  a_boolean         unknown_dependent_function = FALSE;
  a_symbol_ptr      member_func_sym = NULL;

  db_enter(4, "scan_function_call");

#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Save the position of the "(". */
  operator_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  opening_paren_tok_seq_number = curr_token_sequence_number;
  function_position = call_position = operand->position;
  /* If the operand is a bound function, the start position of the call
     is the start of the selector.  Watch out for pointer to
     member calls like (x->*y)(z), where the position of the selector
     is later than the position of the operand, because the latter includes
     the left parenthesis. */
  start_position = (operand->bound_function &&
                    bound_function_selector->position.seq != 0 &&
                    compare_source_positions(
                                &bound_function_selector->position,
                                &call_position) < 0) ?
                                            bound_function_selector->position :
                                            call_position;
  if (curr_expr_kind_is_const()) {
    /* Routine calls not allowed in constant expressions. */
    error_in_operand(ec_bad_constant_function_call, operand);
  } else if (is_expression_operand(operand) &&
             is_operation_node(operand->variant.expression) &&
             (op = operand->variant.expression->variant.operation.kind,
              (op == (an_expr_operator_kind)eok_vacuous_destructor_call ||
               op == (an_expr_operator_kind)eok_value_vacuous_destructor_call)
                                                                           )) {
    /* This operand was generated from a vacuous destructor call, e.g.,
       p->int::~int().
    */
    vacuous_destructor_case = TRUE;
    /* routine = NULL; -- already set. */
    /* Move to after the left parenthesis. */
    (void)get_token();
    first_arg_position = pos_curr_token;
    already_after_left_paren = TRUE;
  } else if (!C_mode() &&
             is_class_struct_union_type(operand->type)) {
    /* The "called function" is a class object.  Look for operator() and
       surrogate functions. */
    a_symbol_ptr member_function_symbol;
    a_type_ptr   class_type = operand->type;
    class_type = skip_typerefs(class_type);
    if (class_type->variant.class_struct_union.is_nonreal_class) {
      /* A call of an object of a nonreal class type in a prototype
         instantiation cannot be resolved. */
      routine_type = NULL;
      prep_generic_operand(operand, /*lvalue_expected=*/FALSE);
      unknown_dependent_function = TRUE;
    } else {
      /* If the class is a template class make sure it is instantiated so its
         operator() functions are visible. */
      instantiate_template_class(class_type);
      try_surrogate_functions = TRUE;
      overloaded_function_case = TRUE;
      /* routine_type = NULL;  -- already set. */
      /* The operand becomes the selector object. */
      check_assertion(!operand->bound_function);
      copy_operand(operand, bound_function_selector);
      conv_class_operand_to_object_pointer(bound_function_selector);
      /* See if the class has an operator(). */
      member_function_symbol = opname_member_function_symbol(
                                        (an_opname_kind)onk_function_call,
                                        class_type);
      if (member_function_symbol != NULL) {
        /* There is an operator() function.  The operand has become
           the selector object, and the function call operator routine
           becomes the operand. */
        /* We can use an indefinite function operand whether the operator()
           function is overloaded or not. */
        make_indefinite_function_operand(member_function_symbol,
                                         /*curr_id=*/FALSE,
                                         operand);
        overloaded_function_symbol = member_function_symbol;
        bind_member_function_operand_to_selector(operand,
                                                 bound_function_selector);
        /* The function position is the position of the "(". */
        function_position = pos_curr_token;
      }  /* if */
    }  /* if */
  } else {
    /* If the operand is the name of a nonstatic member function
       (e.g., "A::f") convert it to a bound member function
       (e.g., "this->A::f").  This is done late so that A::f can be
       converted to a pointer-to-member implicitly in other contexts
       (that's an extension). */
    if (is_sym_for_member_operand(operand) &&
        is_a_function_designator(operand) &&
        !operand->bound_function) {
      member_func_sym = operand->variant.symbol;
      if (make_this_pointer_operand(member_func_sym,
                                    member_func_sym,
                                    &call_position,
                                    (a_boolean)operand->
                                                 access_control_error_reported,
                                    bound_function_selector)) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
        end_position = operand->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Make an operand for the function bound to the "this" pointer. */
        make_function_designator_operand(member_func_sym,
                                         (a_boolean)operand->is_qualified_name,
                                         &call_position,
                                         operand->ref_entries_list, operand);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        operand->end_position = end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Note that the function designator will be converted to a pointer
           by the do_operand_transformations call just below. */
      } else {
        /* There was some problem in constructing the "this" operand. */
        conv_to_error_operand(operand);
      }  /* if */
      bind_member_function_operand_to_selector(operand,
                                               bound_function_selector);
    }  /* if */
    /* Do standard transformations on the operand. */
    { a_transformation_options_set options =
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION;
      /* In Microsoft mode, allow an explicit call of a constructor, e.g.,
         "p->X::X()". */
      if (microsoft_mode) options |= TOPT_ADDR_OF_CTOR_ALLOWED;
      do_operand_transformations(operand, options);
    }
    if (is_undefined_symbol_operand(operand)) {
      /* The function designator is an undefined symbol. */
      a_symbol_ptr func_sym = operand->variant.symbol;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      /* Save the end position for later restoration. */
      a_source_position end_function_position;
      end_function_position = operand->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* In C++, it's an error, but not yet if argument-dependent lookup
         is enabled -- in that case, a function might be found in an
         argument-dependent class or namespace, and no error is issued. */
      if (!C_mode() && arg_dependent_lookup_enabled) {
        overloaded_function_case = TRUE;
        overloaded_function_symbol = func_sym;
        /* routine_type = NULL;  -- already set. */
      } else {
        /* Implicitly declare the symbol as a function. */
        enter_undefined_symbol(func_sym);
        decl_default_function(func_sym);
        /* Issue a low-severity diagnostic, not usually displayed.  In C++
           and C99, issue an error (implicit declaration of functions is not
           allowed). */
        if (C_dialect == C_dialect_cplusplus || c99_mode) {
          pos_st_error(ec_undefined_identifier, &operand->position,
                       func_sym->header->identifier);
        } else {
          pos_remark(ec_implicit_func_decl, &operand->position);
        }  /* if */
        make_function_designator_operand(func_sym,
                                         /*is_qualified_name=*/FALSE,
                                         &func_sym->decl_position,
                                         operand->ref_entries_list,
                                         operand);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        operand->end_position = end_function_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        conv_function_designator_to_ptr_to_function(operand,
                                                    /*allow_ctor=*/FALSE);
        routine = func_sym->variant.routine.ptr;
        routine_type = routine_symbol_type(func_sym);
      }  /* if */
    } else if (is_indefinite_function_operand(operand)) {
      /* Overloaded function.  That means the routine type is not known yet. */
      overloaded_function_case = TRUE;
      overloaded_function_symbol = operand->variant.symbol;
      /* routine_type = NULL;  -- already set. */
    } else if (!C_mode() &&
               is_template_dependent_context() &&
               is_template_param_type(operand->type)) {
      /* A call of a dependent expression in a prototype instantiation.
         Note that we test only for a top-level parameter type here, which
         might be a class.  More testing for other dependent cases
         is done below. */
      routine_type = NULL;
      prep_generic_operand(operand, /*lvalue_expected=*/FALSE);
      unknown_dependent_function = TRUE;
    } else {
      /* Normal function, or call using pointer-to-member-function. */
      /* Convert to rvalue.  This conversion is needed particularly for the
         case
           int (*fp)();  fp();
         I.e., a call using a pointer to function, with no explicit "*". */
      conv_lvalue_to_rvalue(operand);
      /* Expression must be of type "pointer to function" or "pointer to
         member function (bound)". */
      if (operand->bound_function &&
          is_ptr_to_member_type(operand->type)) {
        /* Call of a bound function pointer, as in
             (p->*pmf)(1, 2);
        */
        routine_type = pm_member_type(operand->type);
      } else if (!C_mode() &&
                 is_template_dependent_context() &&
                 is_pointer_type(operand->type) &&
                 is_template_param_type(type_pointed_to(operand->type))) {
        /* A pointer to a template parameter type, which might be a function
           type. */
        routine_type = NULL;
        routine = NULL;
        unknown_dependent_function = TRUE;
      } else if (check_function_pointer_operand(operand)) {
        routine_type = type_pointed_to(operand->type);
        /* If we can tell which routine is being called, set routine to
           the routine entry.  Otherwise, leave it NULL. */
        routine = routine_from_function_operand(operand);
      }  /* if */
      if (!C_mode() &&
          is_template_dependent_context() &&
          routine == NULL &&
          routine_type != NULL &&
          is_template_dependent_type(routine_type)) {
        /* Call through a template-dependent pointer to function or
           pointer to member function.  Suppress argument checking. */
        routine_type = NULL;
        unknown_dependent_function = TRUE;
      }  /* if */
    }  /* if */
    /* Change the kind in the reference entry for the function from an
       address-taken entry back to a simple reference. */
    change_some_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN,
                          SRK_REFERENCE);
  }  /* if */

  /* Scan the arguments of the call. */
  scan_call_arguments(routine_type, routine,
                      already_after_left_paren, &argument_list,
                      overloaded_function_case, unknown_dependent_function,
                      &arg_operand_list, &closing_paren_position);
  error_position = call_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  if (overloaded_function_case) {
    a_source_position id_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Save the end position for later restoration. */
    a_source_position end_function_position;
    end_function_position = operand->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (is_indefinite_function_operand(operand) ||
        is_undefined_symbol_operand(operand)) {
      id_position = operand->id_position;
    } else {
      id_position = function_position;
    }  /* if */
    /* Choose the proper function out of a set of overloaded functions based
       on the argument types. */
    routine_type = select_and_prepare_to_call_overloaded_function(
                                            overloaded_function_symbol,
                                            (a_boolean)operand->is_template_id,
                                            operand->template_arg_list,
                                            operand->bound_function ||
                                                       try_surrogate_functions,
                                            bound_function_selector,
                                            arg_operand_list,
                                            arg_dependent_lookup_enabled &&
                                                   !operand->is_qualified_name,
                                            try_surrogate_functions,
                                         (a_boolean)operand->is_qualified_name,
                                            ec_no_matching_function,
                                            ec_ambiguous_overloaded_function,
                                            /* NOT &operand->position; it
                                               causes aliasing problems in the
                                               subroutines. */
                                            &call_position,
                                            opening_paren_tok_seq_number,
                                            &function_position,
                                            &id_position,
                                            &closing_paren_position,
                                            &unknown_dependent_function,
                                            operand,
                                            &argument_list);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    operand->end_position = end_function_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (unknown_dependent_function) {
      /* The routine to be called cannot be determined because one or more
         of the arguments has a template-dependent type.  Use a generic
         function of the right name. */
      if (try_surrogate_functions) {
        /* Leave the operand alone if it's a class operand for which
           we tried surrogate functions.  The original operand became
           the selector pointer, so move/convert it back. */
        copy_operand(bound_function_selector, operand);
        conv_object_pointer_to_lvalue(operand);
        prep_generic_operand(operand, /*lvalue_expected=*/FALSE);
      } else {
        a_boolean have_selector = operand->bound_function;
        make_unknown_dependent_function_operand(
                                         overloaded_function_symbol,
                                         (a_boolean)operand->is_template_id,
                                         operand->template_arg_list,
                                         (a_boolean)operand->is_qualified_name,
                                         operand);
        if (have_selector) {
          /* This comes up with operator() cases. */
          combine_unneeded_selector_with_operand(bound_function_selector,
                                                 /*is_arrow_operator=*/TRUE,
                                                 operand);
        }  /* if */
      }  /* if */
    } else if (routine_type == NULL) {
      /* None of the overloaded functions matches the argument list. */
      make_error_operand(operand);
    }  /* if */
  } else if (vacuous_destructor_case) {
    /* A vacuous destructor call.  The argument list should have no
       arguments. */
    if (argument_list != NULL) {
      pos_error(ec_too_many_arguments, &first_arg_position);
      conv_to_error_operand(operand);
    }  /* if */
  } else {
    /* Non-overloaded function case. */
    if (operand->bound_function && routine_type != NULL) {
      /* Non-static member function call. */
      /* Check that the selector pointer is compatible with the "this"
         parameter type.  It isn't, for example, if we are calling a
         non-const-qualified function with a const-qualified object.
         Note that if a base class cast was required, it has already been
         done during the function binding, so the differences at this
         point (other than for error cases) are const/non-const differences. */
      selector_match_with_this_param(bound_function_selector,
                                     /*selector_is_object_pointer=*/TRUE,
                                     routine,
                                     implicit_this_param_type_of(routine_type),
                                     &this_match_summary);
      if (this_match_summary.match_level != aml_none) {
        /* The types are compatible. */
        /* Issue any needed warning (e.g., cfront anachronism of calling
           a non-const function with a const selector). */
        issue_warning_from_arg_match_summary(&this_match_summary,
                                             &bound_function_selector->
                                                                     position);
        /* If the function is const, change the reference kinds on the
           selector. */
        change_refs_on_selector_if_const_function(routine_type,
                                                  bound_function_selector);
      } else if (microsoft_bugs && routine == NULL &&
                 is_ptr_to_member_type(operand->type)) {
        /* MSVC++ 6.0 and 7.0 allow a call via a pointer to member
           where the object is constant and the function is not, e.g.,
             (const_obj_ptr->*pm_non_const_func)();
        */
        pos_warning(ec_unqual_function_with_qual_object,
                    &bound_function_selector->position);
      } else {
        /* Some mismatch (more qualifiers on selector than on "this" parameter
           type). */
        if (member_func_sym != NULL) {
          /* The member function called is known. */
          pos_sy_start_error(ec_unqual_named_function_with_qual_object,
                             &bound_function_selector->position,
                             member_func_sym);
        } else {
          pos_start_error(ec_unqual_function_with_qual_object,
                          &bound_function_selector->position);
        }  /* if */
        display_object_type(bound_function_selector->type);
        end_error();
        conv_to_error_operand(bound_function_selector);
      }  /* if */
    }  /* if */
  }  /* if */
  if (vacuous_destructor_case) {
    /* Vacuous destructor case; leave the original operand alone. */
    copy_operand(operand, result);
  } else if (unknown_dependent_function) {
    /* A call of a function whose type is not completely known, in
       a prototype instantiation.  Make a generic call. */
    an_expr_node_ptr function_node, call_node, implicit_this_argument;
    function_node = make_node_from_operand(operand);
    op = (an_expr_operator_kind)eok_generic_call;
    if (operand->bound_function) {
      implicit_this_argument = make_node_from_operand(bound_function_selector);
      implicit_this_argument->next = argument_list;
      argument_list = implicit_this_argument;
      op = (an_expr_operator_kind)eok_generic_member_call;
    }  /* if */
    function_node->next = argument_list;
    call_node = make_operator_node(op,
                                   type_of_unknown_templ_param_nontype,
                                   function_node);
    make_expression_operand(call_node, call_node->type, result);
  } else {
    /* Build the call node and an operand for it. */
    assemble_function_call(operand, bound_function_selector, argument_list,
                           /*compiler_generated=*/FALSE,
                           /*is_conversion=*/FALSE,
                           &call_position, result);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &operator_position);
  db_exit();
}  /* scan_function_call */
                           

static a_symbol_ptr other_field_with_same_name(void)
/*
In pcc mode, a field selection can refer to a field that is not in the
struct or union indicated by the left operand.  This routine finds the
symbol entry for any field of the same name as the current identifier.
If there is none, or if there is more than one and they do not all have
the same offset, NULL is returned.
*/
{
  a_symbol_ptr other_field_sym, temp_field_sym;

  other_field_sym = NULL;
  for (temp_field_sym = locator_for_curr_id.symbol_header->inactive_symbols;
       temp_field_sym != NULL;
       temp_field_sym = temp_field_sym->next) {
    if (temp_field_sym->kind == (a_symbol_kind)sk_field) {
      if (other_field_sym == NULL) {
        /* First field found.  Just save. */
        other_field_sym = temp_field_sym;
      } else {
        /* Field after the first.  All the offsets must match. */
        if ((other_field_sym->variant.field.ptr->offset !=
                temp_field_sym->variant.field.ptr->offset) ||
            (other_field_sym->variant.field.ptr->offset_bit_remainder !=
                temp_field_sym->variant.field.ptr->offset_bit_remainder)) {
          /* Mismatch, so the field reference cannot be unambiguously
             resolved. */
          other_field_sym = NULL;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return other_field_sym;
}  /* other_field_with_same_name */


static void make_field_selection_operand(an_operand            *operand_1,
                                         an_expr_operator_kind op,
                                         a_symbol_ptr          field_sym,
                                         a_type_ptr            selection_type,
                                         an_operand            *result)
/*
Make an operand for a field selection.  *operand_1 is the left operand.
op is the selection operator.  field_sym is the right operand (the field).
selection_type is the result type.  The operand for the selection
is created in *result.
*/
{
  a_field_ptr field = field_sym->variant.field.ptr;
  an_operand  field_operand;

  make_field_operand(field, &field_operand);
  build_binary_result_operand(operand_1, &field_operand, op,
                              selection_type, result);
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  /* When nonstandard anonymous unions are allowed, look for
     fields of such anonymous parents and insert the elided field
     selections. */
  if (field_sym->variant.field.anonymous_parent_object != NULL) {
    an_expr_node_ptr orig_node = make_node_from_operand(result);
    adjust_nonstandard_anonymous_object_field_references(orig_node,
                                                         field_sym,
                                                         /*std_also=*/FALSE);
    make_expression_operand(orig_node, result->type, result);
  }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
}  /* make_field_selection_operand */


static void do_field_selection_operation(
                               an_operand        *operand_1,
                               a_type_ptr        class_struct_union_type,
                               a_boolean         is_arrow_operator,
                               a_boolean         rvalue_result,
                               a_symbol_ptr      field_sym,
                               a_source_position *member_position,
                               a_ref_entry_ptr   rep,
                               an_operand        *result)
/*
Construct the result operand for a field selection operation.  The left
operand (the class/struct/union) is given by operand_1.  The type
of the class/struct/union (before C++ baseward casts, if any) is given
by class_struct_union_type; it provides the type qualifiers that should
be attached to the result expression.  The operator is "->" if
is_arrow_operator is TRUE, "." otherwise (more precisely, the flag means
operand_1 is an rvalue pointer; the operation might have started out as a
"." and have been rewritten in "->" form since then).  rvalue_result is
TRUE if the result should be an rvalue.  field_sym points to the symbol
for the right-side field.  *member_position gives its position.  rep
points to an associated reference entry, or is NULL if none is needed.
The result is placed in *result.
*/
{
  a_field_ptr           field;
  a_type_ptr            result_type;
  a_boolean             operand_1_is_pointer;
  a_type_ptr            selection_type;
  an_expr_operator_kind op;
  a_boolean             did_not_fold, template_constant;
  a_type_qualifier_set  qualifiers;
    
  field = field_sym->variant.field.ptr;
  if (is_error_operand(operand_1)) {
    make_error_operand(result);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode &&
             (field->get_property_name != NULL ||
              field->put_property_name != NULL)) {
    /* A field declared with __declspec(property(...)) in Microsoft C++ mode.
       Render as an ok_property_ref operand, which will be rewritten
       later as a function call. */
    clear_operand((an_operand_kind)ok_property_ref, result);
    result->state = (an_operand_state)os_lvalue;
    result->type = unknown_type();
    result->variant.property_ref.field = field;
    conv_selector_to_object_pointer(operand_1, &is_arrow_operator);
    result->variant.property_ref.object = make_node_from_operand(operand_1);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Determine the result type. */
    operand_1_is_pointer = (is_arrow_operator || !is_an_rvalue(operand_1));
    qualifiers = get_type_qualifiers(class_struct_union_type);
    if (cfront_2_1_mode) {
      /* cfront 2.1 ignores the cv-qualifiers on the left operand. */
      result_type = field->type;
      if (qualifiers != TQ_NONE) {
        a_type_ptr unqual_class_type;
        /* Drop the type qualifiers on the left operand to generate correct
           IL. */
        /* Adjust the type by turning the operand into a pointer (if
           necessary) and casting. */
        conv_selector_to_object_pointer(operand_1, &is_arrow_operator);
        /* Note that we cannot simply use the unqualified version of
           class_struct_union_type here because it might be a derived class. */
        unqual_class_type = type_pointed_to(operand_1->type);
        unqual_class_type = make_unqualified_type(unqual_class_type);
        cast_operand(make_pointer_type(unqual_class_type),
                     operand_1,
                     /*check_cast_access=*/FALSE,
                     /*is_implicit_cast=*/TRUE,
                     /*is_reinterpret_cast=*/FALSE,
                     /*reinterpret_semantics=*/FALSE);
        if (!operand_1_is_pointer) {
          /* For the class rvalue case, produce an rvalue again. */
          conv_object_pointer_to_lvalue(operand_1);
          conv_lvalue_to_rvalue(operand_1);
        }  /* if */
        qualifiers = TQ_NONE;
      }  /* if */
    } else {
      /* The result type is set to the type of the field with the union of the
         qualifiers of the field and the qualifiers of the class, struct,
         or union.  const is ignored if the field was declared mutable. */
      result_type = make_field_selection_type(field, qualifiers);
    }  /* if */
    /* Determine the IL operator to use.  If the first operand is a pointer
       (either explicitly, or because it's an lvalue and the operation is "."),
       eok_field is used.  Otherwise, the first operand is an rvalue and
       eok_value_field must be used. */
    /* Note that in both the "lvalue . field" case and the
       "rvalue -> field" case, the left operand gives the address; the
       lvalue/rvalue representation difference cancels the "."/"->"
       difference. */
    /* Different operators are used for the bit-field cases: 
         eok_value_field -> eok_value_bit_field
         eok_field       -> eok_bit_field
    */
    if (!operand_1_is_pointer) {
      /* For "rvalue . field", the type of the selection is the same
         as the result type. */
      check_assertion(rvalue_result);
      selection_type = result_type;
      op = field->is_bit_field ? (an_expr_operator_kind)eok_value_bit_field :
                                 (an_expr_operator_kind)eok_value_field;
    } else {
      /* For "lvalue . field" and "rvalue -> field", the type of the
         selection (giving, as it does, the address of the resulting
         lvalue) is pointer-to the field type. */
      /* "rvalue . field" also comes here if a base-class cast was
         needed on the rvalue, but it's been rewritten as "rvalue -> field",
         with rvalue_result TRUE. */
      selection_type = make_pointer_type(result_type);
      op = field->is_bit_field ? (an_expr_operator_kind)eok_bit_field :
                                 (an_expr_operator_kind)eok_field;
    }  /* if */
    did_not_fold = TRUE;
    template_constant = FALSE;
    if (is_constant_operand(operand_1) && curr_expr_is_evaluated() &&
        expr_stack->fold_constant_addr_exprs) {
      /* Don't try to fold bit fields except when their addresses
         can be taken (as an extension). */
      if (!field->is_bit_field ||
          (addr_of_bit_field_allowed &&
           is_bit_field_whose_address_can_be_taken(field, &selection_type))) {
        /* Fold a field selection relative to a constant address into another
           constant address.  Note that the "rvalue . field" case can't come
           here, since a struct/union rvalue cannot be a constant.  This
           folding is always done in constant expressions, but in some
           nonconstant expressions it's not done because it's clearer to
           have the field selection in the IL (the constant form has only
           an offset, and loses the field name). */
        clear_operand((an_operand_kind)ok_constant, result);
        fold_field_selection(&operand_1->variant.constant, field_sym,
                             selection_type, &result->variant.constant,
                             &template_constant);
        did_not_fold = template_constant;
      }  /* if */
    }  /* if */
    if (did_not_fold) {
      if (curr_expr_kind_is_const() && curr_expr_is_evaluated() &&
          !template_constant) {
        /* The operation must fold to a constant in a constant expression. */
        pos_error(ec_expr_not_constant, member_position);
        make_error_operand(result);
      } else {
        /* Construct the field selection expression tree. */
        make_field_selection_operand(operand_1, op, field_sym, selection_type,
                                     result);
        if (template_constant) {
          /* A field selection where the first operand is a template parameter
             constant.  The expression tree for the selection is placed under
             a template parameter constant. */
          make_template_param_expr_constant_operand(
                                                make_node_from_operand(result),
                                                result);
        }  /* if */
      }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
    } else if (curr_expr_kind_is_one_in_which_const_exprs_are_recorded()) {
      /* The field-selection operation was constant-folded; reconstruct the
         original expression and record it in the constant: */
      an_operand  result_op;
      copy_operand(operand_1, &result_op);
      make_field_selection_operand(operand_1, op, field_sym, selection_type,
                                   &result_op);
      check_assertion(is_expression_operand(&result_op));
      result->variant.constant.expr = result_op.variant.expression;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
    }  /* if */
    /* Set the operand type.  This is needed in particular if the result
       is an lvalue, because the operand type has one less level of
       "pointer-to" than does the expression node. */
    result->type = result_type;
    if (rvalue_result) {
      /* The result is an rvalue. */
      result->state = (an_operand_state)os_rvalue;
      if (operand_1_is_pointer) {
        /* This is the "rvalue . field" case where the first operand required
           a base class cast and was therefore turned into a pointer.  An
           indirection is needed after the field selection to turn the
           expression for the address into an expression for the rvalue. */
        an_expr_node_ptr node = make_node_from_operand(result);
        node = add_indirection_to_node(node);
        make_expression_operand(node, result_type, result);
      }  /* if */
    } else {
      /* The result is an lvalue. */
      result->state = (an_operand_state)os_lvalue;
    }  /* if */
    /* In C++, a field may have a reference type.  An implicit indirection
       is done to get the value or address of the thing pointed to. */
    if (C_dialect == C_dialect_cplusplus && is_reference_type(result_type)) {
      add_reference_indirection(result);
    } else {
      /* Preserve the reference entries for the base struct.  Don't do this
         if we are dereferencing a reference, because in that case the
         reference if not modified if the lvalue is modified. */
      result->ref_entries_list = operand_1->ref_entries_list;
      if (rep != NULL) {
        /* Add the reference entry for the field to the list of entries for
           the operand. */
        rep->next_operand_ref = result->ref_entries_list;
        result->ref_entries_list = rep;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* do_field_selection_operation */


static a_boolean is_valid_op_arrow_return_type(a_type_ptr return_type)
/*
Return TRUE if return_type is a valid return type for an operator-> function
that is called.
*/
{
  a_boolean  err = FALSE;

  if (is_error_type(return_type)) {
    /* No action. */
  } else if (is_pointer_type(return_type)) {
    /* It's a pointer type -- be sure it's a pointer to a class. */
    if (!is_class_struct_union_type(type_pointed_to(return_type))) {
      /* Not a pointer-to-class type. */
      err = TRUE;
    }  /* if */
  } else {
    /* Not a pointer type.  Be sure the type is a class type or a
       reference to a class type. */
    if (is_reference_type(return_type)) {
      return_type = type_pointed_to(return_type);
    }  /* if */
    return_type = skip_typerefs(return_type);
    if (!is_immediate_class_type(return_type)) {
      /* Not a class type. */
      err = TRUE;
    }  /* if */
  }  /* if */
  return !err;
}  /* is_valid_op_arrow_return_type */


/*
Data structure used by process_overloaded_operator_arrow to check for
loops in operator-> conversions.
*/
typedef struct an_operator_arrow_block *an_operator_arrow_block_ptr;
typedef struct an_operator_arrow_block {
  an_operator_arrow_block_ptr
		parent;
			/* The block for the previous iteration, or NULL if
			   this is the first. */
  a_type_ptr	class_type;
			/* The class type in this iteration. */
} an_operator_arrow_block;


static void process_overloaded_operator_arrow(
                                          an_operand                  *operand,
                                          a_token_sequence_number     tsn,
                                          an_operator_arrow_block_ptr parent)
/*
operand is the first operand of a "->" field selection in C++.  See
whether an operator-> function (or several) applies to convert the
operand to a class or pointer to class.  If so, do the transformation
and return the updated operand.  tsn is the token sequence number of the
"->" token.  parent points to a list of blocks indicating transformations
done so far on this operand, as a way to catch loops.
*/
{
  /* Note that we do not use "is_overloadable_type_operand" here.  That's
     deliberate: doing so could cause infinite loops. */
  if (is_class_struct_union_type(operand->type)) {
    an_operand                  result;
    a_boolean                   processed = FALSE;
    a_type_ptr                  class_type = skip_typerefs(operand->type);
    an_operator_arrow_block_ptr aobp;

    /* See whether the class type has been encountered previously.
       If so, we have a loop. */
    for (aobp = parent; aobp != NULL; aobp = aobp->parent) {
      if (same_entities(class_type, aobp->class_type)) {
        /* Loop in operator-> return types. */
        pos_ty_error(ec_op_arrow_loop, &operand->position, class_type);
        conv_to_error_operand(operand);
        goto end_of_routine;
      }  /* if */
    }  /* for */
    /* Don't process template-dependent classes in prototype instantiations. */
    if (!class_type->variant.class_struct_union.is_nonreal_class) {
      /* Check for an overloaded operator->. */
      /* The operator is treated as a unary operator (i.e., the field
         following the "->" is not significant at this point). */
      check_for_operator_overloading((an_opname_kind)onk_arrow,
                                     /*unary_operator=*/TRUE,  /* sic */
                                     /*must_be_member_function=*/TRUE,
                                     /*try_conversions=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     operand, (an_operand *)NULL,
                                     &operand->position,
                                     tsn,
                                     &result, &processed);
    }  /* if */
    if (processed) {
      /* An operator-> function was found and applied. */
      copy_operand(&result, operand);
      /* Check that the return type of the operator-> function is valid. */
      if (!is_valid_op_arrow_return_type(result.type)) {
        pos_ty2_error(ec_bad_return_type_for_op_arrow,
                      &operand->position, class_type, result.type);
        conv_to_error_operand(operand);
      } else {
        /* If the operator function returns a class object or reference to
           class object, look for another operator->() function.  Maintain
           a list of classes already encountered to allow checking for
           loops. */
        an_operator_arrow_block block;
        block.parent = parent;
        block.class_type = class_type;
        process_overloaded_operator_arrow(operand, tsn, &block);
      } /* if */
    }  /* if */
  }  /* if */
end_of_routine:;
}  /* process_overloaded_operator_arrow */


static void scan_field_selection_operator(
                            an_operand               *operand_1,
                            a_local_expr_options_set local_options,
                            an_operand               *result,
                            an_operand               *bound_function_selector)
/*
Scan the "." and "->" operators.  The left operand must be (a pointer to) a
class, struct, or union.  The right operand must be a member of the class,
struct, or union.  Return the result of the selection in *result.
If the field selection produces a bound function in C++, return the object
bound with the function in *bound_function_selector.  local_options
is the current set of expression-scanning options.
*/
{
  a_symbol_ptr          member_sym, projection_member_sym;
  a_boolean             is_arrow_operator, rvalue_result;
  a_type_ptr            class_struct_union_type = NULL;
  a_type_ptr            orig_class_struct_union_type;
  a_boolean             err = FALSE, found_id = FALSE;
  a_boolean             operand_1_is_complete_class = FALSE, local_err;
  a_boolean             need_operand_1_type_check = FALSE;
  a_boolean             allow_integral_constant_selection = FALSE;
  a_boolean             need_member_sym_check;
  a_ref_entry_ptr       rep;
  a_type_ptr            routine_type;
  a_boolean             is_qualified_name;
  a_boolean             is_vacuous_destructor_reference = FALSE;
  a_source_position     member_position, qualified_member_position;
  an_identifier_options_set
                        gid_flags;
  a_type_ptr            dtor_type;
  a_boolean             pcc_mode_integral_pointer_case = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position     operator_position;
  a_source_position     end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_token_sequence_number
                        operator_tok_seq_number;

  db_enter(4, "scan_field_selection_operator");

  /* Remember if this was an arrow or a dot selector. */
  is_arrow_operator = (curr_token == tok_arrow);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  operator_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is(ek_pp)) {
    /* Field selection not allowed in preprocessor expression. */
    pos_error(ec_bad_pp_operator, &pos_curr_token);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant) ||
             curr_expr_kind_is(ek_template_arg)) {
    /* Field selection is not allowed in integral constant expressions
       or template argument expressions. */
    if (any_cfront_mode() || (microsoft_mode && !C_mode())) {
      /* ... except in cfront or Microsoft C++ mode, where something like
           struct A { enum { e1 = 1 }; } a;
           int x[a.e1];
         is allowed.  The constant check is done at the end. */
      allow_integral_constant_selection = TRUE;
    } else if (curr_expr_kind_is(ek_integral_constant)) {
      /* Field selection not allowed in integral constant expression. */
      pos_error(ec_bad_integral_operator, &pos_curr_token);
      err = TRUE;
    } else {
      /* Field selection not allowed in a template argument expression. */
      pos_error(ec_bad_templ_arg_expr_operator, &pos_curr_token);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    /* Operation is not allowed in this kind of expression. */
    operand_will_not_be_used_because_of_error(operand_1);
  } else {
    if (is_arrow_operator && C_dialect == C_dialect_cplusplus) {
      /* Process overloaded operator->, if applicable. */
      process_overloaded_operator_arrow(operand_1,
                                        operator_tok_seq_number,
                                        (an_operator_arrow_block_ptr)NULL);
    }  /* if */
    { an_expression_kind saved_expr_kind = expr_stack->expression_kind;
      if (allow_integral_constant_selection) {
        /* For the cfront or Microsoft case that allows p->k in a constant
           expression, treat the "p" momentarily as part of a non-constant
           expression to get no error on the lvalue-to-rvalue conversion. */
        expr_stack->expression_kind = (an_expression_kind)ek_normal;
      }  /* if */
      /* Do implicit operand transformations.  In the "." case, keep an lvalue
         if we have one. */
      do_operand_transformations(operand_1,
                                 is_arrow_operator ?
                                    TOPT_NO_OPTIONS :
                                    TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      expr_stack->expression_kind = saved_expr_kind;
    }
    /* The left operand must be (a pointer to) a struct or union. */
    if (is_error_operand(operand_1)) {
      err = TRUE;
    } else {
      if (is_arrow_operator) {
        /* "->" operator.  The left operand must be a pointer. */
        if (C_dialect == C_dialect_pcc &&
            is_integral_or_enum_type(operand_1->type)) {
          /* In pcc mode, something like 0->x is valid. */
          pcc_mode_integral_pointer_case = TRUE;
          orig_class_struct_union_type = NULL;  /* Defensive programming. */
        } else if (is_template_param_or_nonreal_class_type(operand_1->type)) {
          /* In a prototype instantiation, allow a template parameter type,
             which might be a pointer type.  Also allow a nonreal class type,
             which might have an operator-> function. */
          orig_class_struct_union_type = type_of_unknown_templ_param_nontype;
        } else if (check_pointer_operand(operand_1, ec_expr_not_pointer)) {
          orig_class_struct_union_type = type_pointed_to(operand_1->type);
        } else {
          /* Not a pointer. */
          err = TRUE;
        }  /* if */
      } else {
        /* "." operator. */
        orig_class_struct_union_type = operand_1->type;
        if (is_an_lvalue(operand_1)) using_lvalue(operand_1);
      }  /* if */
    }  /* if */
    /* Check that the left operand is (a pointer to) a complete class,
       struct, or union, for either operator. */
    if (!err) {
      if (!pcc_mode_integral_pointer_case) {
        if (is_template_param_type(orig_class_struct_union_type)) {
          /* For a template parameter type, switch to the corresponding
             proxy class.  Preserve cv-qualifiers on the type. */
          a_type_qualifier_set qualifiers =
                             get_type_qualifiers(orig_class_struct_union_type);
          orig_class_struct_union_type =
                                   skip_typerefs(orig_class_struct_union_type);
          orig_class_struct_union_type =
                  proxy_class_for_template_param(orig_class_struct_union_type);
          orig_class_struct_union_type =
                              make_qualified_type(orig_class_struct_union_type,
                                                  qualifiers);

        }  /* if */
        /* Drop any qualifiers or typedefs on the class/struct/union type. */
        class_struct_union_type = skip_typerefs(orig_class_struct_union_type);
        if (is_class_struct_union_type(class_struct_union_type)) {
          /* Instantiate the class if it is a template class. */
          complete_class_type_is_needed(class_struct_union_type);
          operand_1_is_complete_class =
                                  !is_incomplete_type(class_struct_union_type);
        }  /* if */
      }  /* if */
      /* No error is issued yet if the first operand is not (a pointer to)
         a class, because (a) pcc mode allows fields to be selected from
         non-class pointers and integral values, (b) C++ allows
         p->int::~int(), and (c) prototype instantiations. */
      need_operand_1_type_check = TRUE;
    }  /* if */
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  /* See if an identifier (or equivalent) is next. */
  gid_flags = GID_DTOR_RECOGNIZED | GID_IS_FIELD_SELECTION_OPERAND |
              GID_IS_EXPR_CONTEXT;
  if (C_dialect == C_dialect_cplusplus) {
    if (curr_token == tok_template) {
      /* A construct like "p->template f<x>...".  Pass a flag to the
         identifier coalescing routine that the name is known to
         be a template. */
      gid_flags |= GID_FOLLOWS_TEMPLATE;
      if (!is_template_context()) {
        /* The template keyword, when used for syntactic disambiguation,
           may only appear within a template. */
        diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                    : es_warning,
                   ec_template_not_in_template);
      }  /* if */
      (void)get_token();
    }  /* if */
    /* In C++, explicit calls of destructors are allowed for simple types
       and classes without destructors.  For example, p->int::~int(). */
    gid_flags |= GID_VACUOUS_DTOR_RECOGNIZED;
    if (err || !is_class_struct_union_type(class_struct_union_type)) {
      /* If the first operand is not a class, the vacuous destructor calls
         can be things like p->~int(). */
      gid_flags |= GID_DTOR_MUST_BE_NONCLASS;
    }  /* if */
  }  /* if */
  if (f_is_generalized_identifier_start(gid_flags, class_struct_union_type)) {
    found_id = TRUE;
    member_sym = NULL;
    /* See if the name following the operator is a C++ qualified name, as
       in "p->A::x". */
    /* Leading "::" is allowed as of the Portland X3J16/WG21 meeting;
       cfront always allowed it. */
    is_qualified_name = coalesce_and_lookup_qualified_name(gid_flags,
                                                           ilm_expr,
                                                           &local_err);
    err |= local_err;
    /* If the member is something like "A::x", member_position will give
       the position of the "x" and qualified_member_position will give the
       position of the "A". */
    member_position = locator_for_curr_id.source_position;
    qualified_member_position = pos_curr_token;
    if (locator_for_curr_id.is_vacuous_destructor_reference) {
      /* We have something like p->int::~int, a reference to a vacuous
         destructor.  Also p->A::~A(), where A is a class without a
         destructor. */
      is_vacuous_destructor_reference = TRUE;
      need_operand_1_type_check = FALSE;
      /* Watch out for error cases like p->int::~float.  Also, in
         some error cases like p->~xxx, where xxx is either undefined
         or not a type name, class type will be NULL. */
      dtor_type = qualifier_class_type(locator_for_curr_id);
      if (is_error_locator(locator_for_curr_id) || dtor_type == NULL) {
        err = TRUE;
      }  /* if */
    } else {
      /* Not a vacuous destructor case, i.e., normal case. */
      need_member_sym_check = TRUE;
      /* Further checking beyond the fact that this is an identifier is not
         possible if there was an error in the first operand. */
      if (operand_1_is_complete_class) {
        if (is_qualified_name) {
qualified_name_check:
          /* There was a qualified member name, as in "p->A::x".  "A" in the
             preceding must be the class pointed to by p or a base class
             thereof, i.e., "A::x" must be a member of the class of the
             first operand or of one of its base classes. */
          if (is_error_locator(locator_for_curr_id)) {
            /* There was an error in the qualified name. */
            err = TRUE;
          } else {
            projection_member_sym = locator_for_curr_id.specific_symbol;
            member_sym = fundamental_symbol_of(projection_member_sym);
            if (!projection_member_sym->is_class_member) {
              /* The qualified name is not the name of a class member
                 (i.e., it's the name of a namespace member). */
              pos_sy_error(ec_not_class_member,
                           &qualified_member_position,
                           projection_member_sym);
              err = TRUE;
            } else if (class_struct_union_type->
                                 variant.class_struct_union.is_nonreal_class ||
                       projection_member_sym->parent.class_type->
                                 variant.class_struct_union.is_nonreal_class) {
              /* Skip the check for a nonreal class in a prototype
                 instantiation. */
            } else {
              /* Make sure the name is a member of the class indicated by the
                 left-hand side, or one of its base classes. */
              if (!projection_member_sym->is_class_member ||
                  !is_same_class_or_base_class_thereof(class_struct_union_type,
                                                       projection_member_sym->
                                                         parent.class_type)) {
                pos_ty_error(ec_name_not_member_of_class_or_base_classes,
                             &qualified_member_position,
                             class_struct_union_type);
                err = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          need_member_sym_check = FALSE;
        } else {
          /* Normal case: not qualified member name. */
          /* Look up this identifier in the scope of the class, struct, or
             union. */
          member_sym = class_qualified_id_lookup(
                                       &locator_for_curr_id,
                                       class_struct_union_type,
                                       (IDL_IS_EXPR_CONTEXT |
                                        IDL_IS_FIELD_SELECTION_OPERAND));
          if (member_sym == NULL && locator_for_curr_id.is_destructor_name) {
            /* This is a case like p->~A where the class has no destructor.
               This is a vacuous destructor case if the types match.
               Note that we do not allow ~A to be in a base class of the
               class pointed to by p, because destructor names are not
               inherited. */
            a_symbol_ptr class_sym =
              (a_symbol_ptr)class_struct_union_type->source_corresp.assoc_info;
            if (destructor_name_matches_class_name(class_sym)) {
              is_vacuous_destructor_reference = TRUE;
              locator_for_curr_id.is_vacuous_destructor_reference = TRUE;
              dtor_type = class_struct_union_type;
              locator_for_curr_id.parent.class_type = dtor_type;
              locator_for_curr_id.is_class_member = TRUE;
              need_member_sym_check = FALSE;
            }  /* if */
          } else if (member_sym != NULL &&
                     member_sym->kind == (a_symbol_kind)sk_class_template) {
            /* For a member template, coalesce the template reference.
               This will use the specific symbol already established. */
            if (is_generalized_identifier_start(gid_flags)) {
              member_sym = coalesce_and_lookup_generalized_identifier(
                                                                   gid_flags,
                                                                   ilm_expr,
                                                                   &local_err);
              projection_member_sym = locator_for_curr_id.specific_symbol;
              if (local_err) {
                err = TRUE;
              } else if (locator_for_curr_id.is_qualified_name) {
                /* For qualified names, go back and do the normal checking. */
                goto qualified_name_check;
              }  /* if */
            } else {
              unexpected_condition_str(
                            "scan_field_selection_operator: template problem");
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* If the field was not found in pcc or SVR4 C mode, look for any field
         with that name.  If there's only one (or several with the same
         offsets), cast the left-side variable to the right struct/union type
         and do the selection with the found field. */
      if (member_sym == NULL &&
          (C_dialect == C_dialect_pcc || SVR4_C_mode) &&
          /* Avoid the "rvalue . field" case. */
          (is_arrow_operator || is_an_lvalue(operand_1))) {
        a_symbol_ptr other_field_sym = other_field_with_same_name();
        if (other_field_sym != NULL) {
          /* We found a field we can use. */
          member_sym = other_field_sym;
          make_locator_for_symbol(member_sym, &locator_for_curr_id);
          locator_for_curr_id.source_position = member_position;
          if (is_arrow_operator) {
            /* "->" operator. */
            warning(ec_old_fashioned_ptr_field_selection);
          } else {
            /* "." operator.  Convert the lvalue to an rvalue pointer, then
               use "->" instead.  Note that the test above has ensured that
               operand_1 here is an lvalue. */
            warning(ec_old_fashioned_field_selection);
            take_address_of_lvalue(operand_1);
            is_arrow_operator = TRUE;
          }  /* if */
          /* Cast the pointer to a pointer to the proper struct or union. */
          orig_class_struct_union_type = member_sym->parent.class_type;
          class_struct_union_type =skip_typerefs(orig_class_struct_union_type);
          operand_1_is_complete_class = TRUE;
          cast_operand(make_pointer_type(class_struct_union_type),
                       operand_1, /*check_cast_access=*/TRUE,
                       /*is_implicit_cast=*/FALSE,
                       /*is_reinterpret_cast=*/FALSE,
                       /*reinterpret_semantics=*/FALSE);
          /* Mark the struct or union type as referenced, since a field
             therein has been referenced. */
          orig_class_struct_union_type->source_corresp.referenced = TRUE;
        }  /* if */
      }  /* if */
      if (member_sym == NULL && need_member_sym_check) {
        /* The identifier is not a member of the operand_1 class, struct,
           or union. */
        err = TRUE;
        if (!operand_1_is_complete_class) {
          /* An error will be produced below because the first operand is
             not (a pointer to) a class, so do not issue an error here. */
        } else if (is_error_locator(locator_for_curr_id)) {
          /* An error was previously issued. */
        } else {
          pos_stsy_error(C_mode() ? ec_not_a_field : ec_not_a_member,
                         &error_position,
                         locator_for_curr_id.symbol_header->identifier,
                         (a_symbol_ptr)class_struct_union_type->
                                                    source_corresp.assoc_info);
          /* Enter an undefined symbol and record a reference against it. */
          { a_symbol_ptr undef_sym_ptr =
                           enter_undefined_member_symbol(&locator_for_curr_id);
            record_symbol_reference((a_symbol_reference_kind)(SRK_REFERENCE |
                                                              SRK_ERROR),
                                    undef_sym_ptr,
                                    &error_position,
                                    /*update_il_entry=*/FALSE);
          }
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The identifier is not present; error. */
    (void)required_token(tok_identifier,
                         C_dialect == C_dialect_cplusplus ?
                                              ec_exp_member_name :
                                              ec_exp_field_name);
    err = TRUE;
  }  /* if */

  if (need_operand_1_type_check && !operand_1_is_complete_class) {
    /* The first operand is not (a pointer to) a complete class, struct,
       or union. */
    an_error_code err_code;
    /* If the problem is that the class is incomplete, use a different
       error message. */
    if (is_incomplete_type(class_struct_union_type) &&
        is_class_struct_union_type(class_struct_union_type)) {
      err_code = is_arrow_operator ?
                                  ec_ptr_to_incomplete_class_type_not_allowed :
  				  ec_incomplete_type_not_allowed;
    } else {
      if (C_dialect == C_dialect_cplusplus) {
        err_code = is_arrow_operator ? ec_expr_not_ptr_to_class :
                                       ec_expr_not_class;
      } else {
        err_code = is_arrow_operator ? ec_expr_not_ptr_to_struct_or_union :
                                       ec_expr_not_struct_or_union;
      }  /* if */
    }  /* if */
    error_in_operand(err_code, operand_1);
    err = TRUE;
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  if (err || is_error_operand(operand_1)) {
    /* If the operator is not allowed in this kind of expression, or
       if there was an error in the first operand, make an error operand out
       of the result. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
  } else if (is_vacuous_destructor_reference) {
    an_expr_node_ptr node;
    a_boolean        rvalue_case;
    /* A reference to a destructor for a class or simple type that does not
       have one, e.g., p->int::~int(). */
    if (!is_arrow_operator && is_an_lvalue(operand_1)) {
      /* "." operator.  Convert the lvalue to an rvalue pointer, then
         use "->" instead. */
      take_address_of_lvalue(operand_1);
      is_arrow_operator = TRUE;
    }  /* if */
    if (!is_class_struct_union_type(dtor_type)) {
      /* For a non-class case, make sure the operand type reflects the
         type used to refer to the "destructor".  This allows the
         C++-generating back end to deal with a case like
           typedef int T;
           int *p;
           p->T::~T();
         and get the name "T" in the output. */
      rvalue_case = !is_arrow_operator && is_an_rvalue(operand_1);
      cast_operand(rvalue_case ? dtor_type : make_pointer_type(dtor_type),
                   operand_1,
                   /*check_cast_access=*/FALSE,
                   /*is_implicit_cast=*/FALSE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    } else {
      /* Class case. */
      dtor_type = skip_typerefs(dtor_type);
      if (operand_1_is_complete_class &&
          !identical_types(class_struct_union_type, dtor_type) &&
          !class_struct_union_type->variant.class_struct_union.
                                                            is_nonreal_class &&
          !dtor_type->variant.class_struct_union.is_nonreal_class) {
        /* Cast to a base class in a case like
             struct A {};
             struct B : public A {};
             B *p;
             p->A::~A();
        */
        a_base_class_ptr bcp =
                        find_base_class_of(class_struct_union_type, dtor_type);
        check_assertion(bcp != NULL);
        base_class_cast_operand(operand_1, bcp, &is_arrow_operator,
                                /*check_cast_access=*/TRUE,
                                /*is_implicit_cast=*/TRUE,
                                /*implicit_in_naming=*/FALSE,
                                /*is_object_pointer=*/TRUE);
      }  /* if */
    }  /* if */
    /* Make an eok_vacuous_destructor_call node and an operand for it.
       This is a pretty weird representation for this case, but it's a pretty
       weird case.  scan_function_call checks for this construct. */
    node = make_node_from_operand(operand_1);
    rvalue_case = !is_arrow_operator && is_an_rvalue(operand_1);
    node = make_operator_node((an_expr_operator_kind)(rvalue_case ?
                                eok_value_vacuous_destructor_call :
                                eok_vacuous_destructor_call),
                              void_type(),
                              node);
    make_expression_operand(node, node->type, result);
  } else {
    /* Record that the field was referenced, for cross-reference (etc.)
       purposes. */
    /* Don't do this if the symbol is an overloaded function (we don't
       yet know which function is being called). */
    if (member_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      rep = NULL;
    } else {
      rep = ref_entry(member_sym, &member_position);
    }  /* if */
    projection_member_sym = locator_for_curr_id.specific_symbol;
    /* Do ambiguity and access control checking on the member.  For overloaded
       functions, this checks ambiguity but not access (which can be different
       for each function in the set). */
    check_ambiguity_and_verify_access(&locator_for_curr_id);
    if (is_error_locator(locator_for_curr_id)) {
      /* Some error in ambiguity or access control checking. */
      make_error_operand(result);
      /* Avoid further diagnostics by making this an error reference.
         This is necessary because with something like x.y where y is
         ambiguous, some versions of y might be nonstatic and some static,
         which means we do not know whether x is really used. */
      change_operand_refs_to_error(operand_1);
      change_refs_to_error(rep);
    } else {
      /* See what kind of member we have. */
      switch (member_sym->kind) {
        case sk_field:
          /* Normal field selection. */
          /* The result is an rvalue if the operator is "." and the left
             operand is an rvalue. */
          rvalue_result = !is_arrow_operator && is_an_rvalue(operand_1);
          /* This operation uses the left-side operand, so cast the
             operand to the type of the member symbol. */
          cast_pointer_for_field_selection(operand_1, &is_arrow_operator,
                                           member_sym, projection_member_sym,
                                           (a_boolean)locator_for_curr_id.
                                                 access_control_error_reported,
                                           /*do_protected_member_check=*/TRUE,
                                           &member_position);
          if (curr_expr_kind_is(ek_init_constant) &&
              (local_options & EOPT_OPERAND_OF_ADDRESS_OF) &&
              member_sym->variant.field.ptr->is_bit_field) {
            /* Can't take the address of a bit field.  This is checked
               specially to get a better error message. */
            pos_error(ec_address_of_bit_field, &member_position);
            make_error_operand(result);
          } else {
            do_field_selection_operation(operand_1,
                                         orig_class_struct_union_type,
                                         is_arrow_operator, rvalue_result,
                                         member_sym, &member_position, rep,
                                         result);
          }  /* if */
          break;
        case sk_static_data_member:
          /* Static data member reference. */
          make_lvalue_variable_operand(
                              member_sym->variant.static_data_member.variable,
                              result, rep,
                               /*record_expr=*/TRUE);
          combine_unneeded_selector_with_operand(operand_1, is_arrow_operator,
                                                 result);
          break;
        case sk_member_function:
          /* Member function (static or non-static). */
          routine_type = routine_symbol_type(member_sym);
          if (routine_type_is_nonstatic_member_function(routine_type)) {
            /* Nonstatic member function. */
            /* Also continue here for an overloaded function or a member
               template. */
nonstatic_member_function:
            /* Such a reference is not allowed in an initializer constant
               expression.  In truth, though, it's almost impossible to get
               such a thing in C++. */
            if (curr_expr_kind_is(ek_init_constant)) {
              error_and_make_error_operand(ec_expr_not_constant, result);
            } else {
              /* The function will require a "this" pointer, so get a pointer
                 (rather than an rvalue) for the first operand. */
              conv_selector_to_object_pointer(operand_1, &is_arrow_operator);
              if (member_sym->kind == (a_symbol_kind)sk_member_function) {
                /* For a simple non-overloaded function, adjust the selector
                   to point to the proper class.  We don't do this for the
                   cases that go through overload resolution, since that
                   adjustment is done there (it might not be done if a
                   static member function is selected). */
                cast_pointer_for_field_selection(operand_1,
                                                 &is_arrow_operator,
                                                 member_sym,
                                                 projection_member_sym,
                                                 (a_boolean)
                                                           locator_for_curr_id.
                                                 access_control_error_reported,
                                            /*do_protected_member_check=*/TRUE,
                                                 &member_position);
              }  /* if */
              /* Make an operand for the function with the selector bound
                 to it. */
              if (member_sym->kind == (a_symbol_kind)sk_overloaded_function ||
                  member_sym->kind == (a_symbol_kind)sk_function_template) {
                /* Overloaded function or member template. */
                make_indefinite_function_operand(locator_for_curr_id.
                                                               specific_symbol,
                                                 /*curr_id=*/TRUE,
                                                 result);
              } else {
                /* Non-overloaded function. */
                make_function_designator_operand(projection_member_sym,
                                                (a_boolean)locator_for_curr_id.
                                                             is_qualified_name,
                                                 &locator_for_curr_id.
                                                               source_position,
                                                 rep,
                                                 result);
              }  /* if */
              copy_operand(operand_1, bound_function_selector);
              bind_member_function_operand_to_selector(result,
                                                      bound_function_selector);
            }  /* if */
          } else {
            /* Static member function. */
            make_function_designator_operand(projection_member_sym,
                                             is_qualified_name,
                                             &member_position,
                                             rep,
                                             result);
            combine_unneeded_selector_with_operand(operand_1,
                                                   is_arrow_operator,
                                                   result);
          }  /* if */
          break;
        case sk_overloaded_function:
          /* Overloaded function.  Only overloaded member functions are
             possible here, since we've checked that the member symbol
             is part of the left operand class or one of its base classes. */
#if CHECKING
          if (!member_sym->is_class_member) {
            internal_error(
                  "scan_field_selection_operator: overloaded func not member");
          }  /* if */
#endif /* CHECKING */
          goto nonstatic_member_function;
        case sk_function_template:
          /* Member function template. */
          goto nonstatic_member_function;
        case sk_class_template:
          /* Class template.  Returned by symbol lookup for cases like
             x.template f<N> in prototype instantiations.  A class
             template is returned because there's only a representation
             for the class case as a member of a nonreal class, but
             it's really a function template. */
          check_assertion(locator_for_curr_id.is_template_id &&
                          is_template_dependent_context());
          make_unknown_dependent_function_operand(projection_member_sym,
                                                  /*is_template_id=*/TRUE,
                                                  locator_for_curr_id.
                                                             template_arg_list,
                                                  (a_boolean)
                                                        locator_for_curr_id.
                                                             is_qualified_name,
                                                  result);
          combine_unneeded_selector_with_operand(operand_1, is_arrow_operator,
                                                 result);
          break;
        case sk_constant:
          /* Member constant (e.g., an enumerator). */
          make_sym_constant_operand(member_sym, result);
          change_nonreal_member_constant_operand_to_lvalue(result);
          combine_unneeded_selector_with_operand(operand_1, is_arrow_operator,
                                                 result);
          break;
        case sk_type:
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          /* The identifier is a type identifier. */
          pos_error(ec_type_identifier_not_allowed,
                    &locator_for_curr_id.source_position);
          operand_will_not_be_used_because_of_error(operand_1);
          conv_to_error_operand(result);
          break;
#if CHECKING
        default:
          internal_error("scan_field_selection_operator: bad symbol kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
  }  /* if */

  if (found_id) {
    /* The identifier was present; advance past it.  This is done late
       in order not to disturb locator_for_curr_id while it's still needed. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  } else {
    end_position = operator_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */

  /* The position of the operand is the position of the selection
     except when the operand is a bound function, in which case it's the
     position of the function name (because the selector has its own
     operand). */
  if (result->bound_function) {
    set_operand_position(result, &member_position, &end_position,
                         (a_source_position *)NULL);
  } else {
    /* Not a bound function; the operand position reflects the entire
       selection. */
    set_operand_position(result, &operand_1->position, &end_position,
                         &operator_position);
  }  /* if */

  if (allow_integral_constant_selection) {
    /* If we are allowing field selection in an integral constant expression
       as an extension, check now that the result is constant and has
       integral type. */
    if (!is_constant_operand(result) ||
        !is_integral_or_enum_type(result->type)) {
      if (!is_error_operand(result)) {
        error_in_operand(ec_expr_not_constant, result);
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
}  /* scan_field_selection_operator */


static void scan_ptr_to_member_operator(an_operand *operand_1,
                                        an_operand *result,
                                        an_operand *bound_function_selector)
/*
Scan the ".*" and "->*" operators (C++ only).  The left operand must be
(a pointer to) a class.  The right operand must be a pointer-to-member
of the class or a base class thereof.  Return the result of the selection
in *result.  If the field selection produces a bound function, return the
object bound with the function in *bound_function_selector.  See ARM 5.5.
*/
{
  a_boolean         is_arrow_operator;
  a_boolean         err = FALSE, processed = FALSE;
  a_type_ptr        operand_1_type, qual_operand_1_type;
  a_type_ptr        operand_2_type, qual_operand_2_type;
  a_type_ptr        operand_2_class, result_type;
  an_operand        operand_2;
  a_source_position operator_position;
  a_token_sequence_number
                    operator_tok_seq_number;
  a_base_class_ptr  bcp;
  an_expr_node_ptr  select_node, object_node, pm_node;
  a_boolean         rvalue_selection;

  db_enter(4, "scan_ptr_to_member_operator");

  /* Remember if this was an arrow or a dot selector. */
  is_arrow_operator = (curr_token == tok_arrow_star);
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is(ek_pp)) {
    /* Field selection not allowed in preprocessor expression. */
    pos_error(ec_bad_pp_operator, &operator_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Field selection not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &operator_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Field selection not allowed in a template argument expression. */
    pos_error(ec_bad_templ_arg_expr_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_PTR_TO_MEMBER, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    operand_will_not_be_used_because_of_error(&operand_2);
  } else if (!is_arrow_operator && is_template_dependent_context() &&
             (is_template_dependent_type(operand_1->type) ||
              is_template_dependent_type(operand_2.type))) {
    /* If either operand has a template parameter type, we cannot
       check the operand types.  Just produce an expression with
       a generic operator.  This is for ".*" only; the "->*" case
       is handled by check_for_operator_overloading. */
    template_binary_operation((an_expr_operator_kind)eok_pm_dot_field,
                              operand_1, &operand_2,
                              result, &operator_position);
    processed = TRUE;
  } else {
    if (is_arrow_operator &&
        (is_overloadable_type_operand(operand_1) ||
         is_overloadable_type_operand(&operand_2))) {
      /* Look for C++ operator overloading cases ("->*" only). */
      check_for_operator_overloading((an_opname_kind)onk_arrow_star,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      /* Do implicit operand transformations.  In the ".*" case, keep an
         lvalue if we have one. */
      do_operand_transformations(operand_1,
                                 is_arrow_operator ?
                                    TOPT_NO_OPTIONS :
                                    TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      /* Check the first operand type.  It must be (a pointer to) a class. */
      if (is_error_operand(operand_1)) {
        err = TRUE;
      } else {
        if (is_arrow_operator) {
          /* "->*" operator.  The first operand must be a pointer. */
          if (check_pointer_operand(operand_1, ec_expr_not_pointer)) {
            qual_operand_1_type = type_pointed_to(operand_1->type);
          } else {
            /* Not a pointer. */
            err = TRUE;
          }  /* if */
        } else {
          /* ".*" operator. */
          qual_operand_1_type = operand_1->type;
          if (is_an_lvalue(operand_1)) using_lvalue(operand_1);
        }  /* if */
        if (!err) {
          /* Drop any qualifiers or typedefs on the underlying first operand
             type and see if it is a class. */
          operand_1_type = skip_typerefs(qual_operand_1_type);
          if (!is_class_struct_union_type(operand_1_type)) {
            /* Not (a pointer to) a class. */
            an_error_code err_code;
            err_code = is_arrow_operator ? ec_expr_not_ptr_to_class :
                                           ec_expr_not_class;
            error_in_operand(err_code, operand_1);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      /* The type of the second operand must be pointer to member. */
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      qual_operand_2_type = operand_2.type;
      operand_2_type = skip_typerefs(qual_operand_2_type);
      if (!is_ptr_to_member_type(operand_2_type)) {
        if (!is_error_type(operand_2_type)) {
          error_in_operand(ec_expr_not_ptr_to_member, &operand_2);
        }  /* if */
        err = TRUE;
      }  /* if */
      if (!err) {
        /* Check the combination of the operand types. */
        /* The class underlying the second operand type must be the same as
           the class of the first operand, or a base class thereof.  The check
           that the derivation is unambiguous and accessible is done later. */
        operand_2_class = pm_class_type(operand_2_type);
        if (same_entities(operand_1_type, operand_2_class)) {
          /* Same class. */
          bcp = NULL;
        } else if ((bcp = find_base_class_of(operand_1_type,
                                             operand_2_class)) != NULL) {
          /* Related classes. */
        } else {
          /* Bad combination. */
          pos_ty2_error(ec_incompatible_ptr_to_member_selection_operands,
                        &operator_position,
                        operand_1_type, operand_2_class);
          err = TRUE;
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error. */
        make_error_operand(result);
      } else {
        /* The operands are compatible. */
        if (any_cfront_mode() || microsoft_mode) {
          /* ARM rules: the result is always an lvalue and that doesn't
             depend on the lvalueness of the left operand. */
          rvalue_selection = FALSE;
        } else {
          /* The result is an lvalue if the operator is "->*" or if the
             first operand is an lvalue. */
          rvalue_selection = (!is_arrow_operator && is_an_rvalue(operand_1));
        }  /* if */
        /* Get an operand for the address of the object. */
        conv_selector_to_object_pointer(operand_1, &is_arrow_operator);
        /* Cast the left operand to a base class if necessary.  This does the
           ambiguity and accessibility checking. */
        if (bcp != NULL) {
          base_class_cast_operand(operand_1, bcp, &is_arrow_operator,
                                  /*check_cast_access=*/TRUE,
                                  /*is_implicit_cast=*/TRUE,
                                  /*implicit_in_naming=*/FALSE,
                                  /*is_object_pointer=*/TRUE);
        }  /* if */
        /* The result type is the member type pointed to by the second
           operand. */
        result_type = pm_member_type(operand_2_type);
        if (is_function_type(result_type)) {
          /* Result is a bound function.  It can only be called or (as an
             anachronism) cast to a normal function pointer. */
          copy_operand(&operand_2, result);
          /* Clear the reference list for the second operand, because
             if any pointers-to-members are in there we don't want to change
             the references from address-taken to reference on a call. */
          result->ref_entries_list = NULL;
          copy_operand(operand_1, bound_function_selector);
          bind_member_function_operand_to_selector(result,
                                                   bound_function_selector);
        } else {
          /* Result is a data member. */
          if (!microsoft_mode) {
            /* Add cv-qualifiers from the first operand to the result type.
               (This was not in the ARM, but it's in the WP, and cfront does
               it.)  Note: this isn't done for the pointer-to-member-function
               case, since applying type qualifiers to a function type is
               not allowed. */
            result_type = type_plus_qualifiers_from_second_type(
                                                          result_type,
                                                          qual_operand_1_type);
          }  /* if */
          /* Use an eok_pm_field node with the pointer to object and
             pointer-to-member as the operands. */
          object_node = make_node_from_operand(operand_1);
          pm_node = make_node_from_operand(&operand_2);
          object_node->next = pm_node;
          select_node = make_operator_node((an_expr_operator_kind)eok_pm_field,
                                           make_pointer_type(result_type),
                                           object_node);
          if (rvalue_selection) {
            /* If the result is not an lvalue, add an indirection to turn
               the data member address into the data member value. */
            select_node = add_indirection_to_node(select_node);
            /* Drop cv-qualifiers if necessary. */
            result_type = select_node->type;
          }  /* if */
          make_expression_operand(select_node, result_type, result);
          if (!rvalue_selection) {
            /* The result is an lvalue. */
            result->state = (an_operand_state)os_lvalue;
            /* Keep the references from the first operand. */
            result->ref_entries_list = operand_1->ref_entries_list;
          }  /* if */
          /* If the field is a reference add an implicit indirection. */
          if (C_dialect == C_dialect_cplusplus &&
              is_reference_type(result_type)) {
            add_reference_indirection(result);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* The position of the operand is the position of the selection
     except when the operand is a bound function, in which case it's the
     position of the second operand (because the selector has its own
     operand). */
  if (result->bound_function) {
    set_operand_position(result, &operand_2.position,
                         &operand_2.end_position, (a_source_position *)NULL);
  } else {
    /* Not a bound function; the operand position reflects the entire
       selection. */
    set_operand_position(result, &operand_1->position,
                         &operand_2.end_position, &operator_position);
  }  /* if */
  db_exit();
}  /* scan_ptr_to_member_operator */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void prepare_property_ref_incr_decr(
                                          a_boolean         is_increment,
                                          a_source_position *operator_position,
                                          an_operand        *operand,
                                          an_operand        *operand_clone,
                                          an_operand        *result,
                                          a_boolean         *processed)
/*
Do the first part of processing for an increment or decrement of a
reference to a field declared with the Microsoft C++ extension
__declspec(property(...)).  is_increment is TRUE if the operation is
an increment, FALSE for a decrement.  operator_position gives the position
of the "++" or "--" operator.  operand is the operand of the
increment/decrement; it is transformed to an rvalue that is a
call of the appropriate "get" routine.  operand_clone is set to a clone
of the operand, for use later when generating the "put" call.
Operator overloading is checked for, and if it applies, it is handled,
the result is placed in *result, and *processed is set to TRUE.
*/
{
  /* Make a clone of the operand, to be used in the store. */
  clone_operand(operand, operand_clone);
  /* Transform the operand to a call of the appropriate "get" function. */
  rewrite_property_field_reference(operand, (an_operand *)NULL);
  if (is_overloadable_type_operand(operand)) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading((an_opname_kind)(is_increment ?
                                                         onk_plus : onk_minus),
                                   /*unary_operator=*/TRUE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand, (an_operand *)NULL,
                                   operator_position,
                                   (a_token_sequence_number)0,
                                   result, processed);
  }  /* if */
}  /* prepare_property_ref_incr_decr */


static void process_property_ref_incr_decr(
                                          a_boolean         is_increment,
                                          a_source_position *operator_position,
                                          an_operand        *operand,
                                          an_operand        *operand_clone,
                                          an_operand        *result)
/*
Generate the IL operation for an increment or decrement operation on a
reference to a field declared with the Microsoft C++ extension
__declspec(property(...)).  is_increment is TRUE if the operation is an
increment, FALSE for a decrement.  operator_position gives the source
position of the operator.  "operand" is the operand to be
incremented/decremented, already transformed into a call of the
appropriate "get" function.  operand_clone is a clone of the original
operand, to be transformed into a call of the appropriate "put"
function.  The result is placed in *result.
*/
{
  an_operand one_operand;
  a_constant one_constant;
  a_type_ptr result_type;

  /* Make a constant "1" of the right type. */
  set_integer_constant(&one_constant, (a_host_large_integer)1L,
                       (an_integer_kind)ik_int);
  make_constant_operand(&one_constant, &one_operand);
  /* Determine the operation type. */
  result_type = determine_arithmetic_conversions(operand, &one_operand);
  /* Change the operands to the operation type. */
  change_binary_operand_types(result_type, operand, &one_operand);
  /* Generate the IL for the operation. */
  do_binary_operation(which_binary_operator(is_increment ? tok_plus :
                                                           tok_minus,
                                            operand->type),
                      operand, &one_operand,
                      result_type, result, operator_position);
  /* Add a call of the appropriate "put" routine. */
  rewrite_property_field_reference(operand_clone, result);
  copy_operand(operand_clone, result);
}  /* process_property_ref_incr_decr */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void scan_postfix_incr_decr(an_operand *operand,
				   an_operand *result)
/*
Scan the postfix increment ("++") and decrement ("--") operators.  See section
3.3.2.4 of the standard.
*/
{
  an_expr_operator_kind op;
  a_boolean             is_increment;
  a_type_ptr            result_type;
  a_boolean             err = FALSE, processed = FALSE;
  an_operand            zero_operand;
  an_opname_kind        opname_kind;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_boolean             property_ref_case = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_operand            operand_clone;
  a_boolean             operand_clone_unused = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position     end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  db_enter(4, "scan_postfix_incr_decr");

  operator_position = pos_curr_token;
  operator_tok_seq_number = curr_token_sequence_number;
  is_increment = (curr_token == tok_plus_plus);
  if (curr_expr_kind_is_const()) {
    /* Postfix ++/-- not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &operator_position);
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand);
  } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
    property_ref_case = is_property_ref_operand(operand);
    if (property_ref_case) {
      /* The operand is a reference to a field declared with the Microsoft
         C++ extension __declspec(property(...)).  The fetch of the field will
         be made via a call of a "get" function, and the store will be made
         via a call of a "put" function. */
      prepare_property_ref_incr_decr(is_increment, &operator_position,
                                     operand, &operand_clone,
                                     result, &processed);
      operand_clone_unused = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (C_dialect == C_dialect_cplusplus && !property_ref_case &&
        is_overloadable_type_operand(operand)) {
      /* Look for C++ operator overloading cases. */
      /* Note that postfix ++/-- use a two-argument function to distinguish
         them from the prefix ++/--, which use a one-argument function.
         See ARM 13.4.7.  The second compiler-supplied argument is an
         integer zero. */
      make_integer_constant_operand(&zero_operand, (a_host_large_integer)0L);
      opname_kind = opname_kind_for_token[(int)curr_token];
      check_for_operator_overloading(opname_kind,
                                     /*unary_operator=*/FALSE,  /* sic! */
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/!allow_anachronisms,
                                     /*has_predef_meaning=*/allow_anachronisms,
                                     operand, &zero_operand,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
      if (!processed && allow_anachronisms) {
        /* Try the anachronism that allows a one-argument function to
           be used for both prefix and postfix ++/--. */
        check_for_operator_overloading(opname_kind,
                                       /*unary_operator=*/TRUE,
                                       /*must_be_member_function=*/FALSE,
                                       /*try_conversions=*/FALSE,
                                       /*has_predef_meaning=*/TRUE,
                                       operand, (an_operand *)NULL,
                                       &operator_position,
                                       operator_tok_seq_number,
                                       result, &processed);
        if (processed) {
          if (!is_error_operand(result)) {
            pos_st_diagnostic(anachronism_error_severity,
                              ec_single_arg_postfix_incr_decr_anachronism,
                              &operator_position,
                              token_names[(int)curr_token]);
          }  /* if */
        } else {
          /* The anachronism does not apply, so try finding a conversion
             function that will convert the operand to the right type for
             the builtin version of the operator.  Note that this call
             will also try the normal match again, and fail. */
          check_for_operator_overloading(opname_kind,
                                         /*unary_operator=*/FALSE, /* sic! */
                                         /*must_be_member_function=*/FALSE,
                                         /*try_conversions=*/TRUE,
                                         /*has_predef_meaning=*/FALSE,
                                         operand, &zero_operand,
                                         &operator_position,
                                         operator_tok_seq_number,
                                         result, &processed);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      /* The lvalue must be for a scalar, and if it is a pointer, it must
         be a pointer to an object. */
      if (!check_scalar_operand(operand)) {
        /* Operand is not scalar. */
        err = TRUE;
      } else {
        if (is_pointer_type(operand->type)) {
          if (gcc_mode && (is_void_type(type_pointed_to(operand->type)) ||
                           is_function_type(type_pointed_to(operand->type)))) {
            /* In some versions of GNU C void and function pointers can be
               incremented and decremented. */
            pos_warning(ec_nonobject_pointer_arithmetic, &operator_position);
          } else if (!check_object_pointer_operand(
                                     operand, ec_expr_not_pointer_to_object)) {
            err = TRUE;
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (property_ref_case) {
          /* No further checking here. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (C_dialect == C_dialect_cplusplus &&
                   is_enum_type(operand->type)) {
          /* Enum types are not allowed (because the enum promotes to integer
             for the operation, and then can't get back to enum). */
          if (allow_anachronisms) {
            pos_diagnostic(anachronism_error_severity,
                           ec_mixed_enum_type_anachronism, &operand->position);
          } else {
            error_in_operand(ec_enum_type_not_allowed, operand);
            err = TRUE;
          }  /* if */
        } else if (!C_mode() && is_bool_type(operand->type)) {
          /* "++" on bool in C++ is allowed but deprecated.  "--" on bool
             is not allowed. */
          if (is_increment) {
            pos_warning(ec_incr_of_bool_deprecated, &operand->position);
          } else {
            error_in_operand(ec_bool_type_not_allowed, operand);
          }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
        } else if (is_nonreal_floating_type(operand->type)) {
          /* Complex and imaginary operands are not allowed (in C99).*/
          error_in_operand(ec_complex_type_not_allowed, operand);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (property_ref_case) {
        /* No further checking here. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (!check_modifiable_lvalue_operand(operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(operand, /*value_used=*/TRUE);
        result_type = rvalue_type(operand->type);
      }  /* if */
      if (err) {
        /* Error of some kind. */
        make_error_operand(result);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (property_ref_case) {
        /* Operand is a reference to a field declared with
           __declspec(property(...)). */
        process_property_ref_incr_decr(is_increment, &operator_position,
                                       operand, &operand_clone, result);
        operand_clone_unused = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* Determine the IL operator to use. */
        a_type_kind kind = skip_typerefs(result_type)->kind;
        if (is_increment) {
          switch (kind) {
            case tk_integer:
              op = (an_expr_operator_kind)eok_ipost_incr;
              break;
            case tk_float:
              op = (an_expr_operator_kind)eok_fpost_incr;
              break;
            case tk_pointer:
              op = (an_expr_operator_kind)eok_ppost_incr;
              break;
#if CHECKING
            default:
              internal_error("scan_postfix_incr_decr: bad type for ++");
#endif /* CHECKING */
          }  /* switch */
        } else {
          switch (kind) {
            case tk_integer:
              op = (an_expr_operator_kind)eok_ipost_decr;
              break;
            case tk_float:
              op = (an_expr_operator_kind)eok_fpost_decr;
              break;
            case tk_pointer:
              op = (an_expr_operator_kind)eok_ppost_decr;
              break;
#if CHECKING
            default:
              internal_error("scan_postfix_incr_decr: bad type for --");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        build_unary_result_operand(operand, op, result_type, result);
      }  /* if */
    }  /* if */
  }  /* if */

#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* Save the end position of the operator. */
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Get past the "++" or "--". */
  (void)get_token();
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (operand_clone_unused) {
    operand_will_not_be_used_because_of_error(&operand_clone);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  set_operand_position(result, &operand->position, &end_position,
                       &operator_position);

  db_exit();
}  /* scan_postfix_incr_decr */


static void change_assignment_result_to_lvalue(an_operand *result,
                                               an_operand *lvalue_operand,
                                               a_type_ptr result_type)
/*
In C++ mode, assignment operators and prefix ++/-- return lvalues.
Change the operation in *result from an rvalue-returning operation to
an lvalue-returning operation.  *lvalue_operand is the operand for
the lvalue being operated upon.  result_type is the original type of
the first operand, which may differ from the current type of the result
in having type qualifiers.  This routine is called only in C++ mode.
*/
{
  an_expr_node_ptr node;

  if (!is_error_operand(result)) {
    node = result->variant.expression;
    node->variant.operation.returns_lvalue_instead_of_usual_rvalue = TRUE;
    node->type = make_pointer_type(result_type);
    result->type = result_type;
    /* Keep the reference entries from the lvalue operand. */
    result->ref_entries_list = lvalue_operand->ref_entries_list;
  }  /* if */
  result->state = (an_operand_state)os_lvalue;
}  /* change_assignment_result_to_lvalue */


static void scan_prefix_incr_decr(an_operand *result)
/*
Scan the prefix increment ("++") and decrement ("--") operators.  See section
3.3.3.1 of the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand;
  a_source_position     start_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  an_expr_operator_kind op;
  a_boolean             is_increment;
  a_type_ptr            orig_result_type, result_type;
  a_boolean             err = FALSE, processed = FALSE;
  a_boolean             property_ref_case = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_operand            operand_clone;
  a_boolean             operand_clone_unused = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "scan_prefix_incr_decr");

  save_token = curr_token;
  operator_tok_seq_number = curr_token_sequence_number;
  is_increment = (curr_token == tok_plus_plus);
  copy_source_position(pos_curr_token, start_position);

  if (curr_expr_kind_is_const()) {
    /* Prefix ++ and -- are not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, EOPT_PRESERVE_PROPERTY_REF);

  if (err) {
    /* Operator not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(&operand);
  } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
    property_ref_case = is_property_ref_operand(&operand);
    if (property_ref_case && !err) {
      /* The operand is a reference to a field declared with the Microsoft
         C++ extension __declspec(property(...)).  The fetch of the field will
         be made via a call of a "get" function, and the store will be made
         via a call of a "put" function. */
      prepare_property_ref_incr_decr(is_increment, &start_position,
                                     &operand, &operand_clone, result,
                                     &processed);
      operand_clone_unused = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (C_dialect == C_dialect_cplusplus && !property_ref_case &&
        is_overloadable_type_operand(&operand)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     &start_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(&operand,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      /* The lvalue must be for a scalar, and if it is a pointer, it must
         be a pointer to an object. */
      if (!check_scalar_operand(&operand)) {
        /* Operand is not scalar. */
        err = TRUE;
      } else {
        if (is_pointer_type(operand.type)) {
          if (gcc_mode && (is_void_type(type_pointed_to(operand.type)) ||
                           is_function_type(type_pointed_to(operand.type)))) {
            /* In some versions of GNU C void and function pointers can be
               incremented and decremented. */
            pos_warning(ec_nonobject_pointer_arithmetic, &start_position);
          } else if (!check_object_pointer_operand(
                                    &operand, ec_expr_not_pointer_to_object)) {
            err = TRUE;
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (property_ref_case) {
          /* No further checking here. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (C_dialect == C_dialect_cplusplus &&
                   is_enum_type(operand.type)) {
          /* Enum types are not allowed (because the enum promotes to integer
             for the operation, and then can't get back to enum). */
          if (allow_anachronisms) {
            pos_diagnostic(anachronism_error_severity,
                           ec_mixed_enum_type_anachronism, &operand.position);
          } else {
            error_in_operand(ec_enum_type_not_allowed, &operand);
            err = TRUE;
          }  /* if */
        } else if (!C_mode() && is_bool_type(operand.type)) {
          /* "++" on bool in C++ is allowed but deprecated.  "--" on bool
             is not allowed. */
          if (is_increment) {
            pos_warning(ec_incr_of_bool_deprecated, &operand.position);
          } else {
            error_in_operand(ec_bool_type_not_allowed, &operand);
          }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
        } else if (is_nonreal_floating_type(operand.type)) {
          /* Complex and imaginary operands are not allowed (in C99).*/
          error_in_operand(ec_complex_type_not_allowed, &operand);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (property_ref_case) {
        /* No further checking here. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (!check_modifiable_lvalue_operand(&operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(&operand, /*value_used=*/TRUE);
        orig_result_type = operand.type;
        result_type = rvalue_type(orig_result_type);
      }  /* if */
      if (err) {
        /* Error of some kind. */
        make_error_operand(result);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (property_ref_case) {
        /* Operand is a reference to a field declared with
           __declspec(property(...)). */
        process_property_ref_incr_decr(is_increment, &start_position,
                                       &operand, &operand_clone, result);
        operand_clone_unused = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* Determine the IL operator to use. */
        a_type_kind kind = skip_typerefs(result_type)->kind;
        if (is_increment) {
          switch (kind) {
            case tk_integer:
              op = (an_expr_operator_kind)eok_ipre_incr;
              break;
            case tk_float:
              op = (an_expr_operator_kind)eok_fpre_incr;
              break;
            case tk_pointer:
              op = (an_expr_operator_kind)eok_ppre_incr;
              break;
#if CHECKING
            default:
              internal_error("scan_prefix_incr_decr: bad type for ++");
#endif /* CHECKING */
          }  /* switch */
        } else {
          switch (kind) {
            case tk_integer:
              op = (an_expr_operator_kind)eok_ipre_decr;
              break;
            case tk_float:
              op = (an_expr_operator_kind)eok_fpre_decr;
              break;
            case tk_pointer:
              op = (an_expr_operator_kind)eok_ppre_decr;
              break;
#if CHECKING
            default:
              internal_error("scan_prefix_incr_decr: bad type for --");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        build_unary_result_operand(&operand, op, result_type, result);
        /* In C++, the prefix ++ and -- operators return lvalues. */
        if (C_dialect == C_dialect_cplusplus) {
          change_assignment_result_to_lvalue(result, &operand,
                                             orig_result_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (operand_clone_unused) {
    operand_will_not_be_used_because_of_error(&operand_clone);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  set_operand_position(result, &start_position, &operand.end_position,
                       &start_position);

  db_exit();
}  /* scan_prefix_incr_decr */


static void scan_ampersand_operator(an_operand *result)
/*
Scan the "&" (address of) operator.  The operand of the "&" operator must be
either a function designator or an lvalue that is not a bit field or a register
variable.  See section 3.3.3.2 of the C standard.  In C++, the operand may
be a qualified name (a member of a class), in which case the value of the
operation is a pointer-to-member (see ARM 5.3).
*/
{
  an_operand        operand;
  a_source_position start_position;
  a_token_sequence_number
                    operator_tok_seq_number;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean         err = FALSE, processed = FALSE;

  db_enter(4, "scan_ampersand_operator");

  /* Save the source position of the operator. */
  copy_source_position(pos_curr_token, start_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is(ek_pp)) {
    /* Address constants not allowed in preprocessing expressions. */
    pos_error(ec_bad_pp_operator, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Address constants not allowed in integral constant expressions. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Advance past the "&". */
  (void)get_token();
  if (address_of_ellipsis_allowed &&
      curr_token == (a_token_kind)tok_ellipsis) {
    /* Allow the &... extension, used in stdarg.h macros to get the
       address of the ellipsis arguments. */
    if (depth_innermost_function_scope == NO_SCOPE_DEPTH ||
        !f_skip_typerefs(current_routine_entry()->type)->
                                    variant.routine.extra_info->has_ellipsis) {
      /* "&..." used outside a function, or in a function that does not have
         an ellipsis. */
      error(ec_bad_address_of_ellipsis);
      err = TRUE;
      make_error_operand(result);
    } else {
      a_type_ptr       void_star_type = make_pointer_type(void_type());
      an_expr_node_ptr node =
                   alloc_expr_node((an_expr_node_kind)enk_address_of_ellipsis);
      node->type = void_star_type;
      make_expression_operand(node, void_star_type, result);
      if (strict_ansi_mode) {
        diagnostic(strict_ansi_error_severity, ec_nonstd_address_of_ellipsis);
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Advance past the "...". */
    (void)get_token();
  } else {
    /* Scan the operand. */
    scan_expr(&operand, PREC_PREFIX,
              EOPT_OPERAND_OF_ADDRESS_OF | EOPT_PTR_TO_MEMBER_CONTEXT);

    if (err) {
      /* Operator is not allowed in this kind of expression. */
      make_error_operand(result);
      operand_will_not_be_used_because_of_error(&operand);
    } else {
      if (C_dialect == C_dialect_cplusplus &&
          is_overloadable_type_operand(&operand) &&
          !is_sym_for_member_operand(&operand)) {
        /* Look for C++ operator overloading cases. */
        check_for_operator_overloading((an_opname_kind)onk_ampersand,
                                       /*unary_operator=*/TRUE,
                                       /*must_be_member_function=*/FALSE,
                                       /*try_conversions=*/FALSE,
                                       /*has_predef_meaning=*/TRUE,
                                       &operand, (an_operand *)NULL,
                                       &start_position,
                                       operator_tok_seq_number,
                                       result, &processed);
      }  /* if */
      if (!processed) {
        /* Non-operator-function cases. */
        /* As of this writing, this call suppresses every known transformation,
           but it's here to allow for future transformations. */
        do_operand_transformations(&operand,
                                   TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                                   TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                                 TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION |
                                   TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION |
                                  TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
        if (is_an_lvalue(&operand)) {
          if (C_dialect == C_dialect_pcc && is_array_type(operand.type)) {
            /* In pcc mode "&array" is the same as "array" implicitly converted
               to a pointer.  It has type "pointer-to-array-element" rather
               than "pointer to array" as in ANSI. */
            pos_warning(ec_pcc_address_of_array, &start_position);
            conv_array_operand_to_pointer_operand(&operand);
          } else {
            /* Convert the lvalue operand to an rvalue operand for the
               pointer. */
            take_address_of_lvalue(&operand);
          }  /* if */
          /* Note that the copy preserves ref_entries_list. */
          copy_operand(&operand, result);
        } else if (is_a_function_designator(&operand)) {
          /* "&" of a function designator.  Change it to a pointer to the
             function.  This includes overloaded functions and 
             member functions specified by qualified name. */
          /* Change the error position to the "&". */
          operand.position = start_position;
          conv_function_designator_to_ptr_to_function(&operand,
                                                      /*allow_ctor=*/FALSE);
          /* Note that the copy preserves ref_entries_list. */
          copy_operand(&operand, result);
        } else if (is_sym_for_member_operand(&operand)) {
          /* The operand is the name of a nonstatic data member, so
             the "&" operator returns a pointer-to-member. */
          operand.position = start_position;
          conv_sym_for_member_operand_to_ptr_to_member(&operand);
          copy_operand(&operand, result);
        } else {
          /* "&" applied to something that is not an lvalue or a function
             designator or another permitted case. */
          if (!is_error_operand(&operand)) {
            error_in_operand(ec_expr_not_an_lvalue_or_function_designator,
                             &operand);
          }  /* if */
          make_error_operand(result);
        }  /* if */
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */

  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_ampersand_operator */

#if GNU_EXTENSIONS_ALLOWED

static void scan_address_of_label_expression(an_operand *result)
/*
Scan the GNU extended "&&label" expression, which evaluates to
the address (as a void *) of the label.  The "&&" operator is the
current token on entry.
*/
{
  a_label_ptr	    label;
  a_constant        constant;
  a_source_position start_position;
  a_boolean         err = FALSE;

  db_enter(4, "scan_address_of_label_operator");

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

  if (!gcc_mode) {
    /* Address-of-label only recognized in GCC mode. */
    pos_error(ec_nonstd_address_of_label, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_pp)) {
    /* Address-of-label not allowed in preprocessing expressions. */
    pos_error(ec_bad_pp_operator, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Address-of-label not allowed in integral constant expressions. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Address-of-label not allowed in a template argument expression. */
    pos_error(ec_bad_templ_arg_expr_operator, &start_position);
    err = TRUE;
  } else if (strict_ansi_mode) {
    pos_diagnostic(strict_ansi_error_severity, ec_nonstd_address_of_label,
		   &start_position);
    err = (strict_ansi_error_severity == es_error);
  }  /* if */

  /* Scan the operand.  This must be a single label.  */
  (void)get_token();
  label = scan_label(/*is_definition=*/FALSE, /*is_declaration=*/FALSE);

  if (err) {
    make_error_operand(result);
  } else {
    /* Create a constant operand representing the label. */
    set_label_address_constant(label, &constant);
    make_constant_operand(&constant, result);
  }  /* else */
  result->state = (an_operand_state)os_rvalue;

  set_operand_position(result, &start_position, &end_pos_curr_token, 
		       &start_position);
  db_exit();
}  /* scan_address_of_label_expression */


an_expr_node_ptr scan_asm_operand_expression(a_boolean output)
/*
Scan and return the expression associated with an asm operand.  This is similar
to scan_integer_expression with slightly different checks.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           processed = FALSE;

  db_enter(3, "scan_asm_operand_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/TRUE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  /* Convert from a class type to an integer if necessary. */
  if (!C_mode() && is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result, 
                                                 BTK_INTEGRAL |
                                                 BTK_ENUM |
                                                 BTK_FLOATING |
                                                 BTK_POINTER |
                                                 BTK_BOOL,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Non-class (i.e., normal) case. */
    do_operand_transformations(&result,
                               output ?
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION :
                               TOPT_NO_OPTIONS); 
    /* Can't check the type of a template parameter in a prototype
       instantiation. */
    if (!is_template_param_type(result.type)) {
      (void)check_scalar_operand(&result);
    }  /* if */
  }  /* if */
  if (output) {
    /* Output operands must be modifiable lvalues. */
    if (check_modifiable_lvalue_operand(&result)) {
      modifying_lvalue(&result, /*value_used=*/FALSE);
    }  /* if */
  }  /* if */
  expression = make_node_from_operand(&result);
  expression = wrap_up_full_expression(expression);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_asm_operand_expression */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void scan_indirection_operator(an_operand *result)
/*
Scan the "*" (indirection) operator.  The operand must have type pointer.
See section 3.3.3.2 of the standard.
*/
{
  an_operand        operand;
  a_source_position start_position;
  a_token_sequence_number
                    operator_tok_seq_number;
  a_boolean         err = FALSE, processed = FALSE;

  db_enter(4, "scan_indirection_operator");

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is(ek_pp)) {
    /* Address indirection not allowed in preprocessing expressions. */
    pos_error(ec_bad_pp_operator, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Address indirection not allowed in integral constant expressions. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Address indirection not allowed in a template argument expression. */
    pos_error(ec_bad_templ_arg_expr_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(&operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        /* Note: not is_overloadable_type_operand on purpose; we want
           to handle pointer-to-template-param better than the generic way. */
        is_overloadable_type(operand.type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_star,
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     &start_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(&operand, TOPT_NO_OPTIONS);
      if (check_pointer_operand(&operand, ec_bad_indirection_operand)) {
        operand.type = type_pointed_to(operand.type);
        if (is_function_type(operand.type)) {
          /* This will become a function designator. */
          operand.state = (an_operand_state)os_function_designator;
        } else if (is_void_type(operand.type)) {
          /* Indirection through a void * pointer. */
          if (!C_mode()) {
            /* In C++ mode, this is an error.*/
            error_in_operand(ec_expr_not_object_pointer, &operand);
          } else {
            /* In C mode, indirection through a void * pointer is valid,
               but the result is not an lvalue.  For example,
                 void *p; *p;
               is legal, but *p is not an lvalue.  This was discussed in
               Defect Report 12 of the C standards committee, with further
               discussion in Defect Report 106.  While those interpretations
               make it clear that this applies for void *, they seem to
               leave out cv-qualified void *; those apparently still convert
               to an lvalue.  (In GNU C mode, the result is always an
               lvalue). */
            if (!is_qualified_type(operand.type) && !gcc_mode) {
              an_expr_node_ptr node = make_node_from_operand(&operand);
              node = make_operator_node((an_expr_operator_kind)eok_indirect,
                                        operand.type, node);
              make_expression_operand(node, node->type, &operand);
            } else {
              /* Indirection through, e.g., const void * -- just convert to
                 an lvalue. */
              operand.state = (an_operand_state)os_lvalue;
            }  /* if */
          }  /* if */
        } else {
          /* Normal case -- just convert to an lvalue, keeping the same
             underlying value. */
          operand.state = (an_operand_state)os_lvalue;
        }  /* if */
         /* Note that the copy preserves ref_entries_list. */
        copy_operand(&operand, result);
      } else {
        /* There was some error in the operand. */
        make_error_operand(result);
      }  /* if */
    }  /* if */
  }  /* if */

  /* set_operand_position is not used on purpose, because we want to keep
     the position that is in the underlying expression. */
  set_base_operand_position(result, &start_position, &operand.end_position);

  db_exit();
}  /* scan_indirection_operator */


static void diagnose_bad_template_arg_operation(a_source_position *err_pos)
/*
Issue a diagnostic about an operation at the indicated source position
that is invalid within a template argument expression.
*/
{
  pos_error(ec_non_integral_operation_in_templ_arg, err_pos);
}  /* diagnose_bad_template_arg_operation */


static void check_for_bad_template_arg_operation(
                                          an_operand        *operand_1,
                                          an_operand        *operand_2,
                                          a_source_position *operator_position,
                                          an_operand        *result,
                                          a_boolean         *processed)
/*
Check for a non-integral operation in a template argument expression.
operand_1 and operand_2 are the operands of the operation.
*operator_position gives the source position of the operator.  If an
error is detected, a diagnostic is issued, *result is set to an error
operand, and *processed is set to TRUE.
*/
{
  if (is_bad_type_for_template_arg_operand(operand_1->type) ||
      (operand_2 != NULL &&
       is_bad_type_for_template_arg_operand(operand_2->type))) {
    diagnose_bad_template_arg_operation(operator_position);
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    if (operand_2 != NULL) {
      operand_will_not_be_used_because_of_error(operand_2);
    }  /* if */
    *processed = TRUE;
  }  /* if */
}  /* check_for_bad_template_arg_operation */


static void scan_arith_prefix_operator(an_operand *result)
/*
Scan the "+", "-", "~", and "!" prefix operators.  The operand of the "!"
operator must have scalar type.  The operand of "-" and "+" must have
arithmetic type.  The operand of "~" must have integral type.  See section
3.3.3.3 of the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand;
  an_expr_operator_kind op;
  a_source_position     start_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_type_ptr            result_type;
  a_boolean             do_promotion, processed = FALSE;

  db_enter(4, "scan_arith_prefix_operator");

  save_token = curr_token;
  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      is_overloadable_type_operand(&operand)) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/TRUE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   &operand, (an_operand *)NULL,
                                   &start_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    if (is_bad_type_for_template_arg_operand(operand.type) &&
        /* Allow negation of a floating point constant. */
        !(floating_point_template_parameters_allowed &&
          save_token == tok_minus &&
          is_floating_type(operand.type) &&
          is_constant_operand(&operand))) {
      /* Non-integral operations are not allowed in a template argument. */
      diagnose_bad_template_arg_operation(&start_position);
      make_error_operand(result);
      operand_will_not_be_used_because_of_error(&operand);
      processed = TRUE;
    }  /* if */
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    do_operand_transformations(&operand, TOPT_NO_OPTIONS);
    /* Check the type of the operand. */
    do_promotion = TRUE;
    switch (save_token) {
      case tok_plus:
        op = (an_expr_operator_kind)eok_unary_plus;
        if (C_dialect == C_dialect_cplusplus &&
            is_pointer_type(operand.type)) {
          /* In C++, the operand may be a pointer (ARM 5.3). */
        } else {
          /* In C++ or C, the operand may be arithmetic or enum. */
          (void)check_arithmetic_or_enum_operand(&operand);
        }  /* if */
        break;
      case tok_not:
        op = (an_expr_operator_kind)eok_not;
        (void)check_boolean_controlling_expr(&operand);
        do_promotion = FALSE;
        result_type = boolean_result_type();
        break;
      case tok_minus:
#if C99_IL_EXTENSIONS_SUPPORTED
        /* Note that imaginary types fall through to use the normal
           floating-point operator. */
        if (is_complex_type(operand.type)) {
          op = (an_expr_operator_kind)eok_xnegate;
        } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        if (is_floating_type(operand.type)) {
          op = (an_expr_operator_kind)eok_fnegate;
        } else {
          op = (an_expr_operator_kind)eok_inegate;
        }  /* if */
        (void)check_arithmetic_or_enum_operand(&operand);
        break;
      case tok_compl:
        op = (an_expr_operator_kind)eok_complement;
        (void)check_integral_or_enum_operand(&operand);
        break;
      default:
        unexpected_condition_str("scan_arith_prefix_operator: bad operator");
    }  /* switch */

    if (do_promotion) {
      /* Do integral promotions if required. */
      promote_operand(&operand);
      result_type = operand.type;
    }  /* if */
    /* Build the IL for the operation. */
    do_unary_operation(op, &operand, result_type, result,
                       &start_position);
  }  /* if */

  set_operand_position(result, &start_position, &operand.end_position,
                       &start_position);
  db_exit();
}  /* scan_arith_prefix_operator */


static an_expr_node_ptr make_runtime_sizeof_expr(a_boolean  is_type,
                                                 a_type_ptr type,
                                                 an_operand *operand)
/*
Create an enk_runtime_sizeof expression for a sizeof and return a pointer
to it.  If is_type is TRUE, this is a "sizeof(type)", and "type" indicates
the type.  If is_type is FALSE, this is a "sizeof expression", and
"operand" indicates the expression.
*/
{
  an_expr_node_ptr node =
                        alloc_expr_node((an_expr_node_kind)enk_runtime_sizeof);

  node->type = integer_type(targ_size_t_int_kind);
  node->variant.runtime_sizeof.is_type = is_type;
  if (is_type) {
    /* sizeof(type). */
    node->variant.runtime_sizeof.variant.type = type;
  } else {
    /* sizeof expression. */
    if (is_template_dependent_context()) {
      /* An expression in a prototype instantiation. */
      prep_generic_operand(operand, /*lvalue_expected=*/FALSE);
    }  /* if */
    node->variant.runtime_sizeof.is_lvalue = is_an_lvalue(operand);
    node->variant.runtime_sizeof.variant.expr= make_node_from_operand(operand);
  }  /* if */
  return node;
}  /* make_runtime_sizeof_expr */


static void scan_sizeof_operator(an_operand *result)
/*
Scan the sizeof operator.  The operand of the sizeof operator cannot be an
expression with function or incomplete type.  The operand cannot be the
parenthesized name of an incomplete or function type.  The operand cannot be
an lvalue that is a bit-field.  See section 3.3.3.4 of the standard.

Syntax:
	sizeof unary-expression
	sizeof ( type-name )
*/
{
  a_source_position     start_position, type_position;
  a_source_position     lparen_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position     end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand            operand;
  a_constant            constant;
  a_boolean             is_parenthesized = FALSE, is_type = FALSE;
  a_type_ptr            sizeof_type, orig_sizeof_type;
  a_local_expr_options_set
                        local_options;
  an_expr_stack_entry   expr_stack_entry;
  a_boolean             template_case = FALSE;
  a_boolean             in_constant_expression = (expr_stack != NULL &&
                                                  curr_expr_kind_is_const());
  db_enter(4, "scan_sizeof_operator");

#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* Sizeof not possible for preprocessing expressions. */
    internal_error("scan_sizeof_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Save the position of the sizeof keyword. */
  copy_source_position(pos_curr_token, start_position);

  (void)get_token();
  if (curr_token == tok_lparen) {
    /* A left parenthesis could indicate a type in parentheses or an expression
       in parentheses, i.e.,
         sizeof (int)  vs.
         sizeof (i)
       We can distinguish the two using the first token inside the parentheses.
       However, if the construct is an expression in parentheses, we must
       then scan it with a special flag indicating that a left parenthesis was
       trapped.  It's not enough to just scan the expression to the matching
       right parenthesis, as shown by the following:
         sizeof (v).b
       The sizeof should be applied to "(v).b", not just "(v)". */
    is_parenthesized = TRUE;
    copy_source_position(pos_curr_token, lparen_position);
    (void)get_token();
    if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                         DFS_SINGLE_TYPE_REQUIRED)) {
      /* This is a type-name in parentheses. */
      is_type = TRUE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode && !C_mode()) {
    /* Microsoft allows "sizeof T" without parentheses in C++ mode,
       where T is a type-name (not a keyword like "int"). */
    if (is_expr_qualified_name_start() &&
        is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                         DFS_SINGLE_TYPE_REQUIRED)) {
      /* Something like
           typedef int I;
           sizeof I;
         but not
           sizeof I();
      */
      is_type = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */

  if (is_type) {
    copy_source_position(pos_curr_token, type_position);
    if (is_parenthesized) {
      /* Scan the type-name for a parenthesized type. */
      add_matching_stop_token(tok_rparen);
      type_name(&sizeof_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_matching_stop_token(tok_rparen);
      if (compound_literals_allowed && curr_token == tok_lbrace) {
        /* Something like sizeof(int){37} -- the type is the beginning
           of a compound literal. */
        scan_compound_literal(&sizeof_type, &type_position, result,
                              EOPT_NO_OPTIONS);
        sizeof_type = result->type;
      }  /* if */
    } else {
      /* Unparenthesized type, e.g., "sizeof T" (Microsoft extension). */
#if MICROSOFT_EXTENSIONS_ALLOWED
      sizeof_type = simple_type_specifier_sequence();
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
      unexpected_condition();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    /* If the top type is a reference, drop the reference so that the sizeof
       applies to the type referenced (ARM 5.3.2). */
    if (is_reference_type(sizeof_type)) {
      sizeof_type = type_pointed_to(sizeof_type);
    }  /* if */
  } else {
    /* It has been determined that the operand of the sizeof is an expression
       and not a type.  Scan the operand. */
    local_options = EOPT_NO_OPTIONS;
    if (is_parenthesized) local_options |= EOPT_TRAPPED_LEFT_PAREN;
    scan_expr(&operand, PREC_PREFIX, local_options);
    /* Do not convert a type of "routine returning type" to "pointer to
       routine returning type".  See section 3.2.2.1 in the C standard.
       Likewise do not convert arrays to pointers, or lvalues to rvalues. */
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION);
    if (is_parenthesized) {
      /* When scanning the expression with a trapped left parenthesis, the
         position returned in the operand indicates the token following
         the left parenthesis, which is wrong.  Correct it. */
      copy_source_position(lparen_position, operand.position);
    }  /* if */
    if (is_bit_field_operand(&operand)) {
      /* This is a bit field; it is illegal except when in pcc compatibility
	 mode. */
      /* In pcc mode the size of a bit field is the size of the type of the
	 bit field (e.g., "unsigned int"). */
      if (C_dialect != C_dialect_pcc) {
        error_in_operand(ec_sizeof_bit_field, &operand);
      }  /* if */
    }  /* if */
    sizeof_type = operand.type;
    copy_source_position(operand.position, type_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */

  orig_sizeof_type = sizeof_type;
  sizeof_type = skip_typerefs(sizeof_type);
  /* Instantiate the type if it is a template class. */
  complete_type_is_needed(sizeof_type);
  /* The operand of a sizeof may not have function type or incomplete
     type (except in GNU C mode, where function types and void are treated
     as byte-sized). */
  if (!C_mode() && is_template_dependent_context() &&
      is_template_dependent_type(sizeof_type)) {
    /* Don't test a template-dependent type.  This is important, in
       particular, for some cases where, with implicit typename,
       disambiguation concludes that something is a function-type
       declaration instead of a functional-notation type conversion. */
    template_case = TRUE;
  } else if (is_function_type(sizeof_type)) {
    if (gcc_mode) {
      /* GCC evaluates sizeof(function-type) as 1. */
      sizeof_type = integer_type((an_integer_kind)ik_char);
    } else {
      pos_error(ec_sizeof_function, &type_position);
      sizeof_type = error_type();
    }  /* if */
  } else if (is_incomplete_type(sizeof_type)) {
    if (gcc_mode && is_void_type(sizeof_type)) {
      /* GCC evaluates sizeof(void) as 1. */
      sizeof_type = integer_type((an_integer_kind)ik_char);
    } else {
      pos_error(ec_incomplete_type_not_allowed, &type_position);
      sizeof_type = error_type();
    }  /* if */
  }  /* if */

  if (vla_enabled && is_vla_type(sizeof_type)) {
    /* One or more of the top array types is a variable-length array. */
    if (in_constant_expression) {
      /* Not allowed in a constant expression. */
      pos_error(ec_expr_not_constant, &start_position);
      make_error_operand(result);
    } else {
      /* Make an expression node to represent a sizeof that cannot be
         evaluated until runtime.  Note the use of orig_sizeof_type
         to preserve typedefs. */
      an_expr_node_ptr node =
                 make_runtime_sizeof_expr(is_type, orig_sizeof_type, &operand);
      make_expression_operand(node, node->type, result);
    }  /* if */
#ifdef SIZEOF_TYPE_IS_UNKNOWN
  } else if (SIZEOF_TYPE_IS_UNKNOWN(sizeof_type)) {
    /* The size of this type is not known at compile time.  This is
       used with the C++-generating back end when it is difficult or
       impossible to duplicate the layout algorithm of the target
       compiler.  Some types (e.g., int) might be easy to know, and
       some (e.g., non-POD classes) might not be.  SIZEOF_TYPE_IS_UNKNOWN
       is a function-like macro that returns TRUE for the complicated
       cases. */
    if (in_constant_expression) {
      /* Not allowed in a constant expression. */
      pos_error(ec_expr_not_constant, &start_position);
      make_error_operand(result);
    } else {
      /* Make an expression node to represent the sizeof. */
      an_expr_node_ptr node =
                 make_runtime_sizeof_expr(is_type, orig_sizeof_type, &operand);
      make_expression_operand(node, node->type, result);
    }  /* if */
#endif /* defined(SIZEOF_TYPE_IS_UNKNOWN) */
  } else {
    /* The result of a sizeof is an integer indicating the size of the operand
       in bytes, of type size_t (see ISO C 6.3.3.4 and <stddef.h>). */
    if (is_error_type(sizeof_type)) {
      set_error_constant(&constant);
    } else {
      if (template_case) {
        /* For the size of a template type, use a ck_template_param. */
        clear_constant(&constant, (a_constant_repr_kind)ck_template_param);
        set_template_param_constant_kind(&constant,
                                  (a_template_param_constant_kind)tpck_sizeof);
        constant.variant.template_param.variant.templ_sizeof.type= sizeof_type;
        if (!is_type) {
          prep_generic_operand(&operand, /*lvalue_expected=*/FALSE);
          constant.variant.template_param.variant.templ_sizeof.expr =
                                              make_node_from_operand(&operand);
        }  /* if */
        constant.type = integer_type(targ_size_t_int_kind);
      } else {
        /* Normal case; known constant sizeof. */
        set_unsigned_integer_constant(&constant,
                                      (a_host_large_unsigned)sizeof_type->size,
                                      targ_size_t_int_kind);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
        /* Make a sizeof expression that sits behind the constant and
           gives the original expression. */
        if (!is_type &&
            curr_il_region_number == file_scope_region_number &&
            innermost_function_scope != NULL) {
          /* An expression in a function scope might point to a local variable,
             which is in the function scope memory region.  Therefore it
             cannot be attached to a file-scope constant.  This comes up when
             a sizeof in an array bound uses a local variable in its
             expression.  We have no good way of checking whether the
             expression contains a local variable, so we suppress the
             recording of the expression in all cases, and just record
             the type. */
          is_type = TRUE;
        }  /* if */
        constant.expr = make_runtime_sizeof_expr(is_type, orig_sizeof_type,
                                                 &operand);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      }  /* if */
    }  /* if */
    make_constant_operand(&constant, result);
  }  /* if */

  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  pop_expr_stack();

  db_exit();
}  /* scan_sizeof_operator */


static void scan_alignof_operator(an_operand *result)
/*
Scan the __ALIGNOF__ operator.  This is an extension that is similar
to sizeof, but returns the alignment requirement rather than the size.

Syntax:
        __ALIGNOF__ ( type-name )    or   __alignof__ ( type-name )
        __ALIGNOF__ expression       or   __alignof__ expression

Fewer error checks are done.  A warning about the use of this nonstandard
feature would be inappropriate, because the feature is probably used to
implement <stdarg.h>, a standard feature.
*/
{
  a_source_position   start_position, type_position;
  a_source_position   lparen_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position   end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand          operand;
  a_constant          constant;
  a_boolean           is_parenthesized = FALSE, is_type = FALSE;
  a_type_ptr          alignof_type;
  an_expr_stack_entry expr_stack_entry;
#if GNU_EXTENSIONS_ALLOWED
  a_targ_alignment    alignment = 0;
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(4, "scan_alignof_operator");

  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Save the position of the __ALIGNOF__ keyword. */
  copy_source_position(pos_curr_token, start_position);
  (void)get_token();

  if (curr_token == tok_lparen) {
    /* A left parenthesis could indicate a type in parentheses or an expression
       in parentheses, i.e.,
         __ALIGNOF__ (int)  vs.
         __ALIGNOF__ (i)
       We can distinguish the two using the first token inside the parentheses.
       However, if the construct is an expression in parentheses, we must
       then scan it with a special flag indicating that a left parenthesis was
       trapped.  It's not enough to just scan the expression to the matching
       right parenthesis, as shown by the following:
         __ALIGNOF__ (v).b
       The __ALIGNOF__ should be applied to "(v).b", not just "(v)". */
    is_parenthesized = TRUE;
    copy_source_position(pos_curr_token, lparen_position);
    (void)get_token();
    if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                         DFS_SINGLE_TYPE_REQUIRED)) {
      /* This is a type-name in parentheses. */
      is_type = TRUE;
    }  /* if */
  }  /* if */

  if (is_type) {
    copy_source_position(pos_curr_token, type_position);
    if (is_parenthesized) {
      /* Scan the type-name for a parenthesized type. */
      add_matching_stop_token(tok_rparen);
      type_name(&alignof_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_matching_stop_token(tok_rparen);
      if (compound_literals_allowed && curr_token == tok_lbrace) {
        /* Something like __ALIGNOF__ (int){37} -- the type is the beginning
           of a compound literal. */
        scan_compound_literal(&alignof_type, &type_position, result,
                              EOPT_NO_OPTIONS);
        alignof_type = result->type;
      }  /* if */
    }  /* if */
    /* If the top type is a reference, drop the reference so that the operator
       applies to the type referenced (ARM 5.3.2). */
    if (is_reference_type(alignof_type)) {
      alignof_type = type_pointed_to(alignof_type);
    }  /* if */
  } else {
    /* It has been determined that the operand of __ALIGNOF__ is an expression
       and not a type.  Scan the operand. */
    a_local_expr_options_set  local_options = EOPT_NO_OPTIONS;
    if (is_parenthesized) local_options |= EOPT_TRAPPED_LEFT_PAREN;
    scan_expr(&operand, PREC_PREFIX, local_options);
    /* Do not convert a type of "routine returning type" to "pointer to
       routine returning type".  See section 3.2.2.1 in the C standard.
       Likewise do not convert arrays to pointers, or lvalues to rvalues. */
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION);
    if (is_parenthesized) {
      /* When scanning the expression with a trapped left parenthesis, the
         position returned in the operand indicates the token following
         the left parenthesis, which is wrong.  Correct it. */
      copy_source_position(lparen_position, operand.position);
    }  /* if */
    alignof_type = operand.type;
#if GNU_EXTENSIONS_ALLOWED
    if (gcc_mode) {
      /* If the expression is an lvalue for a variable with an
         explicit alignment, use it. */
      if (is_an_lvalue(&operand)) {
        if (is_expression_operand(&operand) &&
            is_variable_address_node(operand.variant.expression) &&
            operand.variant.expression->variant.variable->alignment != 0) {
          alignment = operand.variant.expression->variant.variable->alignment;
        } else if (is_constant_operand(&operand) && 
                   operand.variant.constant.kind ==
                                        (a_constant_repr_kind)ck_address &&
                   operand.variant.constant.variant.address.kind ==
                                      (an_address_base_kind)abk_variable &&
                   operand.variant.constant.variant.address.offset == 0 &&
                   operand.variant.constant.variant.address.
                                           variant.variable->alignment != 0) {
          alignment = operand.variant.constant.variant.address.
                                                variant.variable->alignment;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  alignof_type = skip_typerefs(alignof_type);
  /* Instantiate the type if it is a template class. */
  complete_type_is_needed(alignof_type);
  /* The result of __ALIGNOF__ is an integer indicating the alignment of
     the operand, of type size_t. */
  if (is_error_type(alignof_type)) {
    set_error_constant(&constant);
  } else if (!C_mode() && is_template_dependent_context() &&
             is_template_dependent_type(alignof_type)) {
    /* For __ALIGNOF__ of a template type, use a ck_template_param. */
    clear_constant(&constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(&constant,
                                 (a_template_param_constant_kind)tpck_alignof);
    constant.variant.template_param.variant.templ_sizeof.type = alignof_type;
    if (!is_type) {
      prep_generic_operand(&operand, /*lvalue_expected=*/FALSE);
      constant.variant.template_param.variant.templ_sizeof.expr =
                                              make_node_from_operand(&operand);
    }  /* if */
    constant.type = integer_type(targ_size_t_int_kind);
#if GNU_EXTENSIONS_ALLOWED
  } else if (alignment != 0) {
    set_unsigned_integer_constant(&constant, (a_host_large_unsigned)alignment,
                                  targ_size_t_int_kind);
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else {
    set_unsigned_integer_constant(
                     &constant, (a_host_large_unsigned)alignof_type->alignment,
                     targ_size_t_int_kind);
  }  /* if */
  make_constant_operand(&constant, result);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  pop_expr_stack();

  db_exit();
}  /* scan_alignof_operator */

#if GNU_EXTENSIONS_ALLOWED

a_type_ptr scan_typeof_operator(void)
/*
Scan the typeof operator.  This is a GNU C extension that is similar
to sizeof, but returns the type rather than the size.  It is used
in type contexts, not expression contexts.

Syntax:
        typeof ( type-name )    or   __typeof__ ( type-name )
        typeof ( expression )   or   __typeof__ ( expression )

The parentheses are required, unlike for sizeof.
*/
{
  a_type_ptr           result;
  an_expr_stack_entry  expr_stack_entry;
  an_operand           operand;

  /* Skip the typeof or __typeof__ token. */
  check_assertion(gcc_mode && curr_token == tok_typeof);
  (void)get_token();
  /* Prepare for the possibility of having to scan an expression. */
  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Distinguish between the type-name and expression case. */
  if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                       DFS_SINGLE_TYPE_REQUIRED)) {
    /* Scan a type name. */
    type_name(&result);
  } else {
    /* Scan an expression. */
    scan_expr(&operand, PREC_LOWEST, EOPT_NO_OPTIONS);
    result = operand.type;
  }  /* if */
  if (!is_error_type(result)) {
    a_type_ptr  typeof_type = alloc_type((a_type_kind)tk_typeref);
    typeof_type->variant.typeref.type = result;
    typeof_type->variant.typeref.is_typeof = TRUE;
    add_to_types_list(typeof_type, decl_scope_level);
    result = typeof_type;
  }  /* if */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  pop_expr_stack();
  return result;
}  /* scan_typeof_operator */


void typedef_initializer(a_symbol_ptr  symbol_ptr)
/*
Scan an initializer expression applied to a typedef (with the given symbol).
This is a GNU extension, with the effect of setting the type defined to the
type of the expression.
*/
{
  an_expr_stack_entry  expr_stack_entry;
  an_operand           operand;

  check_assertion(symbol_ptr->kind == (a_symbol_kind)sk_type);
  /* Prepare to scan an expression. */
  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Scan an expression. */
  scan_expr(&operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* The expression is treated as an rvalue. */
  do_operand_transformations(&operand,
                             (TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                              TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION));
  /* Remember the type. */
  symbol_ptr->variant.type.ptr = operand.type;
  pop_expr_stack();
}  /* typedef_initializer */

#endif /* GNU_EXTENSIONS_ALLOWED */


static a_type_ptr scan_type_generic_expression_and_return_type(void)
/*
Scan an expression (beginning at the current token) that is an argument to a
type-generic function.  Do not evaluate the expression; just determine its
floating or complex type, converting an integral type to double, and return
the type, stripped of typerefs.
*/
{
  an_operand  operand;
  a_type_ptr  tp;

  /* Scan the expression. */
  scan_expr(&operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Do not convert lvalues to rvalues, arrays to pointers, or functions to
     pointers. */
  do_operand_transformations(&operand,
                             (TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                              TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                              TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION));
  /* Get its type. */
  if (is_error_operand(&operand)) {
    tp = error_type();
  } else {
    tp = skip_typerefs(operand.type);
    if (is_integral_or_enum_type(tp)) {
      /* Integral types are converted to double. */
      tp = float_type((a_float_kind)fk_double);
    } else if (!is_arithmetic_or_enum_type(tp)) {
      pos_error(ec_expr_not_arithmetic, &operand.position);
      tp = error_type();
    }  /* if */
  }  /* if */
  return tp;
}  /* scan_type_generic_expression_and_return_type */


static void scan_optional_type_generic_operator_expression(
                                                          a_type_ptr *arg_type,
                                                          a_boolean  *err)
/*
Scan an optional expression (beginning at the current token) that is an
argument to a type-generic function.  Do not evaluate the expression; just
determine its type, and adjust the composite type in *arg_type
accordingly.  Set *err to TRUE if there is an error.
*/
{
  if (curr_token == tok_comma || curr_token == tok_rparen) {
    /* The expression is not present.  *arg_type is left as it is. */
  } else {
    /* There is an expression.  Scan it and choose the more inclusive
       of the two types. */
    a_type_ptr new_type = scan_type_generic_expression_and_return_type();
    if (is_error_type(new_type)) *err = TRUE;
    if (!*err && !same_entities(new_type, *arg_type)) {
      /* Reconcile the two types.  If either is long double, use long
         double.  Otherwise, if either is double, use double.  Otherwise,
         use float.  The standard doesn't cover complex cases (it forgot about
         "pow"), but do the sensible thing with those as well. */
      a_float_kind fkind;
      check_assertion(is_floating_type(new_type) &&
                      is_floating_type(*arg_type));
      /* *arg_type and new_type already have typerefs stripped. */
      if ((*arg_type)->variant.float_kind == (a_float_kind)fk_long_double ||
          new_type   ->variant.float_kind == (a_float_kind)fk_long_double) {
        fkind = (a_float_kind)fk_long_double;
      } else if ((*arg_type)->variant.float_kind == (a_float_kind)fk_double ||
                 new_type   ->variant.float_kind == (a_float_kind)fk_double) {
        fkind = (a_float_kind)fk_double;
      } else {
        fkind = (a_float_kind)fk_float;
      }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
      if (is_complex_type(*arg_type) || is_complex_type(new_type)) {
        *arg_type = complex_type(fkind);
      } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      {
        *arg_type = float_type(fkind);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* scan_optional_type_generic_operator_expression */


static void scan_type_generic_operator(an_operand *result)
/*
Scan the __generic operator, which implements C99 type-generic function
macros.  The form of the macro expansion is:

   __generic(x, [y], [z], fnc-d, fnc-f, fnc-l, fnc-cd, fnc-cf, fnc-cl)

where x and the optional y and z are the arguments with which a type-generic
function is called, and the remaining 6 arguments are the names of functions
from which is selected the actual function to be called.  (The suffixes with
which the function names are supplied here correspond to function parameter
types: double, float, long double, double _Complex, float _Complex, long
double _Complex, respectively.  The order is fixed.)  Function names may be
omitted.

For example, tgmath.h may have the following macros defined:

 #define sin(x)    __generic(x,,,   sin, sinf, sinl, csin, csinf, csinl)(x)
 #define fmax(x,y) __generic(x, y,, fmax, fmaxf, fmaxl,,,)(x, y)
 #define conjg(x)  __generic(x,,,   ,,, conjg, conjgf, conjgl)(x)

Note that sin and conjg take only one argument, that fmax has no forms
that accept complex arguments, and that conjg has no forms that accept real
arguments.
*/
{
  a_source_position   start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position   end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand          operand;
  a_type_ptr          arg_type;
  int                 arg_number, func_arg_number;
  a_boolean           err = FALSE;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           saved_evaluated, saved_potentially_evaluated;

  db_enter(4, "scan_type_generic_operator");

  check_assertion(c99_mode);
  /* Save the position of the __generic keyword. */
  start_position = pos_curr_token;
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  add_stop_token(tok_comma);
  /* Scan the first argument expression, but do not evaluate it -- just get
     its type. */
  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  arg_type = scan_type_generic_expression_and_return_type();
  if (is_error_type(arg_type)) err = TRUE;
  /* Bypass the comma, and move on to scan the optional second argument. */
  (void)required_token(tok_comma, ec_exp_comma);
  scan_optional_type_generic_operator_expression(&arg_type, &err);
  /* Bypass the comma, and move on to scan the optional third argument. */
  (void)required_token(tok_comma, ec_exp_comma);
  scan_optional_type_generic_operator_expression(&arg_type, &err);
  pop_expr_stack();
  /* Use arg_type to determine the argument number of the function that
     matches the type of the expression. */
  if (err) {
    func_arg_number = -1;
  } else {
    check_assertion(arg_type != NULL && is_floating_type(arg_type));
    /* Positions 4, 5, and 6 are occupied, respectively, by double,
       float, and long double versions of the function. */
    switch (arg_type->variant.float_kind) {
      case fk_double:      func_arg_number = 4; break;
      case fk_float:       func_arg_number = 5; break;
      case fk_long_double: func_arg_number = 6; break;
      default:
        unexpected_condition_str2("scan_type_generic_operator:",
                                  "bad float kind");
    }  /* switch */
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Positions 7, 8, and 9 are occupied, respectively, by complex
       double, complex float, and complex long double versions of the
       function. */
    if (arg_type->kind == (a_type_kind)tk_complex) func_arg_number += 3;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  }  /* if */
  check_assertion(func_arg_number == -1 ||
                  (func_arg_number >= 4 && func_arg_number <= 9));
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "func_arg_number = %d, arg_type = ", func_arg_number);
    if (arg_type == NULL) {
      fputs("NULL", f_debug);
    } else {
      db_type(arg_type);
    }  /* if */
    fputs("\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  saved_evaluated = expr_stack_entry.evaluated;
  saved_potentially_evaluated = expr_stack_entry.potentially_evaluated;
  /* Now loop through the remaining arguments (4-9), ignoring everything
     except the function name associated with the argument type -- i.e., the
     expression at the position specified by func_arg_number. */
  for (arg_number = 4; arg_number <= 9; arg_number++) {
    /* Bypass the comma. */
    (void)required_token(tok_comma, ec_exp_comma);
    /* Examine the expression, if any. */
    if (curr_token == tok_comma || curr_token == tok_rparen) {
      /* Missing expression. */
      if (arg_number == func_arg_number ||
          (curr_token == tok_rparen && arg_number < func_arg_number)) {
        /* There is no specific function corresponding to the type.  Issue
           an error. */
        pos_ty_error(ec_type_generic_function_mismatch, &start_position,
                     arg_type);
        err = TRUE;
        if (curr_token == tok_rparen) break;
      } else {
        /* Okay. */
      }  /* if */
    } else {
      /* The expression is evaluated if it's the one that is to be
         returned. */
      expr_stack_entry.evaluated = 
      expr_stack_entry.potentially_evaluated = (arg_number == func_arg_number);
      /* Scan the expression. */
      scan_expr(&operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
      if (arg_number == func_arg_number) {
        /* This is the target position in the list of functions. */
        copy_operand(&operand, result);
        do_operand_transformations(result, TOPT_NO_OPTIONS);
      }  /* if */
    }  /* if */
  }  /* for */
  expr_stack_entry.evaluated = saved_evaluated;
  expr_stack_entry.potentially_evaluated = saved_potentially_evaluated;
  remove_stop_token(tok_comma);
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  }  /* if */

#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);

  db_exit();
}  /* scan_type_generic_operator */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void scan_assume_operator(an_operand *result)
/*
Scan the Microsoft extension __assume(expr).  The operand is boolean,
and is not evaluated.  This is supposedly a hint to the optimizer that
the given expression is true.
*/
{
  a_source_position   start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position   end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_expr_node_ptr    expr;
  an_expr_stack_entry expr_stack_entry;

  db_enter(4, "scan_assume_operator");

  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = TRUE;
  /* Save the position of the __assume keyword. */
  copy_source_position(pos_curr_token, start_position);
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Scan the expression. */
  scan_expr(result, PREC_LOWEST, EOPT_NO_OPTIONS);
  /* Check its type and normalize it. */
  process_boolean_controlling_expression(result, /*validate_only=*/FALSE);
  expr = make_node_from_operand(result);
  expr = wrap_up_full_expression(expr);
  expr = make_operator_node((an_expr_operator_kind)eok_assume, void_type(),
                            expr);
  make_expression_operand(expr, expr->type, result);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);

  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  pop_expr_stack();

  db_exit();
}  /* scan_assume_operator */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void scan_typeid_operator(an_operand *result)
/*
Scan the C++ typeid operator.  See [expr.typeid].

Syntax:
	typeid ( expression )
	typeid ( type-id )

This is the C++ syntax.  C++ type-id is the same as C type-name.
*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand        operand;
  an_expr_node_ptr  expr = NULL, typeid_node;
  a_type_ptr        typeid_type;
  a_boolean         err = FALSE;

  db_enter(4, "scan_typeid_operator");
  /* Save the position of the typeid keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* Typeid not possible for preprocessing expressions. */
    internal_error("scan_typeid_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* RTTI is outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                              &pos_curr_token,
                                              ec_rtti_in_embedded_cplusplus);
  if (curr_expr_kind_is_const()) {
    /* typeid is not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */
  /* typeid is valid only after the type_info type has been defined in a
     header file. */
  if (!err && is_incomplete_type(type_of_type_info)) {
    error(ec_typeid_needs_typeinfo);
  }  /* if */
  /* Advance past typeid. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Disambiguate to choose between the type case and the expression case. */
  if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                       DFS_SINGLE_TYPE_REQUIRED)) {
    /* Scan a type name. */
    type_name(&typeid_type);
    /* If the type is a reference, drop that. */
    if (is_reference_type(typeid_type)) {
      typeid_type = type_pointed_to(typeid_type);
    }  /* if */
  } else {
    /* Scan an expression. */
    scan_expr(&operand, PREC_LOWEST, EOPT_NO_OPTIONS);
    /* Rule out indefinite functions. */
    do_operand_transformations(&operand,
                               (TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                                TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                                TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION |
                                TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION));
    typeid_type = operand.type;
    /* *p and p[expr] yielding polymorphic class objects are special cases
       that use runtime typeid determination. */
    if (is_an_lvalue(&operand) &&
        is_polymorphic_class_type(typeid_type)) {
      if (is_expression_operand(&operand)) {
        if (operand_complete_object_type(&operand,
                                         /*call_case=*/FALSE) != NULL) {
          /* The complete object type can be determined, so runtime processing
             is not needed. */
          expr = NULL;
        } else {
          /* The type must be determined dynamically. */
          expr = operand.variant.expression;
        }  /* if */
      } else if (is_constant_operand(&operand) &&
                 constant_bool_value_known_at_compile_time(
                                                  &operand.variant.constant) &&
                 /* "false" means zero, i.e., a null pointer. */
                 is_false_constant(&operand.variant.constant)) {
        /* Special case for (*(T *)0), which should throw an exception. */
        expr = make_node_from_operand(&operand);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Type qualifiers on the type are ignored [expr.typeid]. */
  /* The two calls here make sure typedefs are removed and qualifiers under
     arrays are removed. */
  typeid_type = make_unqualified_type(typeid_type);
  typeid_type = skip_typerefs(typeid_type);
  /* Instantiate the type if it is a template class. */
  complete_type_is_needed(typeid_type);
  /* The type cannot be incomplete if it is a class type. */
  if (is_class_struct_union_type(typeid_type)) {
    if (is_incomplete_type(typeid_type)) {
      error(ec_incomplete_type_not_allowed);
      err = TRUE;
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    /* Create a typeid expression node. */
    a_type_ptr const_type_info =
                           make_qualified_type(type_of_type_info,
                                               (a_type_qualifier_set)TQ_CONST);
    typeid_node = alloc_expr_node((an_expr_node_kind)enk_typeid);
    typeid_node->variant.typeid_info.expr = expr;
    typeid_node->variant.typeid_info.type = typeid_type;
    typeid_node->implicit_reference_indirection = TRUE;
    /* The result is a reference to type_info, which means a pointer to
       type_info as an lvalue address. */
    typeid_node->type = make_pointer_type(const_type_info);
    make_expression_operand(typeid_node, const_type_info, result);
    result->state = (an_operand_state)os_lvalue;
    set_used_in_exception_or_rtti_flag(typeid_type);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_typeid_operator */


static an_expr_node_ptr scan_va_list_lvalue_expr(a_boolean     value_used,
                                                 an_error_code err_code,
                                                 a_boolean     *err)
/*
Scan an lvalue expression and return a pointer to it.  Check that the
type of the expression is the builtin type va_list from <stdarg.h>.  If it
isn't, or the expression isn't an lvalue, issue the error err_code, set
*err to TRUE, and return NULL.  The value of the lvalue is used if
value_used is TRUE.  The lvalue is assumed always to be set (since we don't
know what the underlying implementation is).
*/
{
  an_operand       operand;
  an_expr_node_ptr node;

  scan_expr(&operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&operand,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
  /* The operand must be an lvalue of the builtin type va_list. */
  check_assertion(builtin_va_list_type != NULL);
  if (!is_an_lvalue(&operand) ||
      !types_are_compatible_ignoring_qualifiers(builtin_va_list_type,
                                                operand.type)) {
    if (!is_error_operand(&operand)) {
      error_in_operand(err_code, &operand);
    }  /* if */
    *err = TRUE;
  }  /* if */
  if (!*err) {
    modifying_lvalue(&operand, value_used);
    node = make_node_from_operand(&operand);
  } else {
    operand_will_not_be_used_because_of_error(&operand);
    node = NULL;
  }  /* if */
  return node;
}  /* scan_va_list_lvalue_expr */


static void scan_va_start_operator(an_operand *result,
                                   a_boolean  single_operand)
/*
Scan a reference to the <stdarg.h> or <varargs.h> va_start macro, when it is
treated as a builtin.  The <stdarg.h> form is expected when single_operand is
FALSE:

  va_start(va_list_var, last_param)

where va_list_var is a variable declared with the builtin type va_list,
and last_param is the last parameter before the "..." of the function.
When single_operand is TRUE, the <varargs.h> form is expected:

  va_start(va_list_var)

*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand        operand;
  an_expr_node_ptr  node1, node2;
  a_boolean         err = FALSE;

  db_enter(4, "scan_va_start_operator");
  /* Save the position of the va_start keyword. */
  start_position = pos_curr_token;
  /* va_start not possible in preprocessing expressions. */
  check_assertion_str(!curr_expr_kind_is(ek_pp),
                      "scan_va_start_operator: in preprocessing expr");
  if (curr_expr_kind_is_const()) {
    /* va_start is not allowed in constant expressions. */
    pos_error(ec_bad_va_start, &start_position);
    err = TRUE;
  } else {
    /* Check if we are in a valid function for the use of va_start.
       GNU C is a little stricter about this than our default mode. */
    a_boolean  bad_scope = TRUE;
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      a_routine_ptr  routine =
                     scope_stack[depth_innermost_function_scope].assoc_routine;
      a_type_ptr     routine_type = skip_typerefs(routine->type);
      if (routine_type->variant.routine.extra_info->has_ellipsis ||
          (!gcc_mode &&
           !routine_type->variant.routine.extra_info->prototyped)) {
        bad_scope = FALSE;
      }  /* if */
    }  /* if */
    if (bad_scope) {
      diagnostic(gcc_mode ? es_error : es_warning,
                 ec_va_start_requires_ellipsis_function);
    }  /* if */
  }  /* if */
  /* Advance past va_start. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  if (!single_operand) {
    add_stop_token(tok_comma);
  }  /* if */
  /* Scan the first expression. */
  node1 = scan_va_list_lvalue_expr(/*value_used=*/FALSE,
                                   ec_bad_va_start, &err);
  if (!single_operand) {
    /* Check for and pass over the comma. */
    add_stop_token(tok_identifier);
    (void)required_token(tok_comma, ec_exp_comma);
    remove_stop_token(tok_identifier);
    remove_stop_token(tok_comma);
    /* Scan the second expression. */
    scan_expr(&operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
    /* The expression must be a parameter of the function. */
    if (is_an_lvalue(&operand) &&
        is_expression_operand(&operand) &&
        is_variable_address_node(node2 = operand.variant.expression) &&
        node2->variant.variable->is_parameter) {
      /* Okay. */
#if BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE
      /* Many implementations of va_start expose the address of the
         parameter variable.  Also consider this a use of the parameter. */
      change_ref_kinds(operand.ref_entries_list, SRK_USE | SRK_ADDRESS_TAKEN);
#endif /* BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE */
      if (!err) {
        node1->next = node2;
      }  /* if */
    } else {
      if (!is_error_operand(&operand)) {
        error_in_operand(ec_bad_va_start, &operand);
      }  /* if */
      err = TRUE;
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    /* Create a va_start expression node. */
    an_expr_node_ptr va_start_node;

    va_start_node =
      make_operator_node((an_expr_operator_kind)(single_operand ?
                             (an_expr_operator_kind)eok_va_start_single_operand
                           : (an_expr_operator_kind)eok_va_start),
                         void_type(), node1);
    make_expression_operand(va_start_node, va_start_node->type, result);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_va_start_operator */


static void scan_va_arg_operator(an_operand *result)
/*
Scan a reference to the <stdarg.h> va_arg macro, when it is treated
as a builtin.  Its form is

  va_arg(va_list_var, type)

where va_list_var is a variable declared with the builtin type va_list,
and type is the type of the argument to be extracted.
*/
{
  a_source_position start_position, type_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_expr_node_ptr  node;
  a_type_ptr        type, type_to_cast_to = NULL;
  a_boolean         err = FALSE;

  db_enter(4, "scan_va_arg_operator");
  /* Save the position of the va_arg keyword. */
  start_position = pos_curr_token;
  /* va_arg not possible in preprocessing expressions. */
  check_assertion_str(!curr_expr_kind_is(ek_pp),
                      "scan_va_arg_operator: in preprocessing expr");
  if (curr_expr_kind_is_const()) {
    /* va_arg is not allowed in constant expressions. */
    pos_error(ec_bad_va_arg, &start_position);
    err = TRUE;
  }  /* if */
  /* Advance past va_arg. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  add_stop_token(tok_comma);
  /* Scan the expression. */
  node = scan_va_list_lvalue_expr(/*value_used=*/TRUE, ec_bad_va_arg, &err);
  /* Check for and pass over the comma. */
  add_stop_token(tok_identifier);
  (void)required_token(tok_comma, ec_exp_comma);
  remove_stop_token(tok_identifier);
  remove_stop_token(tok_comma);
  /* Scan the type. */
  type_position = pos_curr_token;
  type_name(&type);
  if (is_function_type(type) ||
      is_array_type(type) ||
      is_reference_type(type)) {
    /* The type is not allowed to be an array, function, or reference type. */
    pos_error(ec_bad_va_arg, &type_position);
    err = TRUE;
  } else {
    a_type_ptr  promoted_type = default_argument_promotion(type);
    if (!identical_types(type, promoted_type)) {
      an_error_severity severity = (an_error_severity)es_warning;
      if (gcc_mode) {
        severity = (an_error_severity)es_error;
        err = TRUE;
      }  /* if */
      /* The type must possibly be obtained after default promotion. */
      pos_ty2_diagnostic(severity, ec_va_arg_would_have_been_promoted,
                         &type_position, type, promoted_type);
      type_to_cast_to = type;
      type = promoted_type;
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    /* Create a va_arg expression node. */
    an_expr_node_ptr va_arg_node =
             make_operator_node((an_expr_operator_kind)eok_va_arg, type, node);
    if (type_to_cast_to != NULL) {
      cast_node(&va_arg_node, type_to_cast_to, /*check_cast_access=*/TRUE,
                /*is_implicit_cast=*/FALSE, /*is_reinterpret_cast=*/FALSE,
                /*reinterpret_semantics=*/FALSE, &start_position);
    }  /* if */
    make_expression_operand(va_arg_node, va_arg_node->type, result);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_va_arg_operator */


static void scan_va_end_operator(an_operand *result)
/*
Scan a reference to the <stdarg.h> va_end macro, when it is treated
as a builtin.  Its form is

  va_end(va_list_var)

where va_list_var is a variable declared with the builtin type va_list.
*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_expr_node_ptr  node;
  a_boolean         err = FALSE;

  db_enter(4, "scan_va_end_operator");
  /* Save the position of the va_end keyword. */
  start_position = pos_curr_token;
  /* va_end not possible in preprocessing expressions. */
  check_assertion_str(!curr_expr_kind_is(ek_pp),
                      "scan_va_end_operator: in preprocessing expr");
  if (curr_expr_kind_is_const()) {
    /* va_end is not allowed in constant expressions. */
    pos_error(ec_bad_va_end, &start_position);
    err = TRUE;
  }  /* if */
  /* Advance past va_end. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Scan the expression. */
  node = scan_va_list_lvalue_expr(/*value_used=*/TRUE, ec_bad_va_end, &err);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    /* Create a va_end expression node. */
    an_expr_node_ptr va_end_node =
      make_operator_node((an_expr_operator_kind)eok_va_end, void_type(), node);
    make_expression_operand(va_end_node, va_end_node->type, result);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_va_end_operator */


static void scan_va_copy_operator(an_operand *result)
/*
Scan a reference to the <stdarg.h> va_copy macro, when it is treated
as a builtin.  Its form is

  va_copy(va_list_dest, va_list_source)

where va_list_dest and va_list_source are variables declared with the
builtin type va_list.
*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_expr_node_ptr  node1, node2;
  a_boolean         err = FALSE;

  db_enter(4, "scan_va_copy_operator");
  /* Save the position of the va_copy keyword. */
  start_position = pos_curr_token;
  /* va_copy not possible in preprocessing expressions. */
  check_assertion_str(!curr_expr_kind_is(ek_pp),
                      "scan_va_copy_operator: in preprocessing expr");
  if (curr_expr_kind_is_const()) {
    /* va_copy is not allowed in constant expressions. */
    pos_error(ec_bad_va_copy, &start_position);
    err = TRUE;
  }  /* if */
  /* Advance past va_copy. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  add_stop_token(tok_comma);
  /* Scan the first expression. */
  node1 = scan_va_list_lvalue_expr(/*value_used=*/FALSE,
                                   ec_bad_va_copy, &err);
  /* Check for and pass over the comma. */
  add_stop_token(tok_identifier);
  (void)required_token(tok_comma, ec_exp_comma);
  remove_stop_token(tok_identifier);
  remove_stop_token(tok_comma);
  /* Scan the second expression. */
  node2 = scan_va_list_lvalue_expr(/*value_used=*/TRUE,
                                   ec_bad_va_copy, &err);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    /* Create a va_copy expression node. */
    an_expr_node_ptr va_copy_node;

    node1->next = node2;
    va_copy_node = make_operator_node((an_expr_operator_kind)eok_va_copy,
                                       void_type(), node1);
    make_expression_operand(va_copy_node, va_copy_node->type, result);
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_va_copy_operator */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_type_ptr underlying_uuidof_type(a_type_ptr uuidof_type,
                                         a_boolean  *template_case,
                                         a_boolean  *err)
/*
Extract and return the underlying type of uuidof_type, for a __uuidof
operator.  Levels like "array of" and "pointer to" are removed.
If the underlying type is not a class or enum with an associated uuid,
return NULL.  If the underlying type might be a class with an associated
uuid, but we can't tell for sure because there are template parameters
involved, set *template_case to TRUE.  If we can't tell because
of an error type, set *err TRUE.  Overall, returned-type != NULL
means the type has or could have a uuid (the returned-type might
be a template parameter type or an error type).  Returned-type == NULL
means the type had no uuid or more than one uuid.  *err == TRUE means
there was an error type somewhere in the type.
*/
{
  a_boolean  is_enum;

  if (is_array_type(uuidof_type)) {
    /* Reduce an array type to the underlying element type. */
    uuidof_type = underlying_array_element_type(uuidof_type);
  } else if (is_pointer_type(uuidof_type)) {
    /* Reduce a pointer to the underlying type. */
    uuidof_type = type_pointed_to(uuidof_type);
  }  /* if */
  uuidof_type = skip_typerefs(uuidof_type);
  is_enum = is_immediate_enum_type(uuidof_type);
  if (!is_class_struct_union_type(uuidof_type) && !is_enum) {
    /* uuidof_type is not a class or enum type, which is generally an error. */
    if (is_template_param_type(uuidof_type)) {
      /* A template parameter type could be a class type. */
      *template_case = TRUE;
    } else if (is_error_type(uuidof_type)) {
      /* An error type could have been intended to be a class type. */
      *err = TRUE;
    } else {
      uuidof_type = NULL;
    }  /* if */
  } else if (is_template_dependent_context() &&
             is_template_dependent_type(uuidof_type)) {
    /* A template-dependent class type.  We must be in a prototype
       instantiation.  Assume the type has a uuid. */
    *template_case = TRUE;
  } else if (is_enum) {
    if (uuidof_type->variant.integer.uuid_string == NULL) {
      /* No uuid on this enum. */
      uuidof_type = NULL;
    }  /* if */
  } else {
    /* uuidof_type is a class type. */
    if (uuidof_type->variant.class_struct_union.is_template_class) {
      /* Templates don't have a uuid themselves -- the uuid of a template
         argument type is used.  There must be only one argument with a
         uuid value. */
      a_class_type_supplement_ptr ctsp =
                            uuidof_type->variant.class_struct_union.extra_info;
      a_template_arg_ptr tap = ctsp->template_arg_list;
      uuidof_type = NULL;
      for (; tap != NULL; tap = tap->next) {
        if (tap->kind == (a_templ_arg_kind)tak_type) {
          a_type_ptr temp_type = underlying_uuidof_type(tap->variant.type,
                                                        template_case,
                                                        err);
          if (temp_type != NULL) {
            /* This template argument has a uuid.   It's an error if a
               previous argument also had a uuid. */
            if (uuidof_type != NULL) {
              uuidof_type = NULL;
              break;
            } else {
              /* Remember the underlying type for the first template
                 argument with a uuid. */
              uuidof_type = temp_type;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    } else {
      /* Non-template class type. */
      if (uuidof_type->variant.class_struct_union.extra_info->uuid_string
                                                                     == NULL) {
        /* No uuid on this class. */
        uuidof_type = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  return uuidof_type;
}  /* underlying_uuidof_type */
 

static void scan_uuidof_operator(an_operand *result)
/*
Scan the C++ __uuidof operator, a Microsoft C++ extension.

Syntax:
	__uuidof ( expression )
	__uuidof ( type-id )

The value of the operation is an lvalue of type "const struct _GUID".
*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  an_operand        operand;
  a_type_ptr        uuidof_type;
  a_boolean         err = FALSE, template_case = FALSE;
  a_boolean         is_type;

  db_enter(4, "scan_uuidof_operator");
  /* Save the position of the __uuidof keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* __uuidof not possible for preprocessing expressions. */
    internal_error("scan_uuidof_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  if (curr_expr_kind_is(ek_integral_constant)) {
    /* __uuidof is not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  }  /* if */
  /* Advance past __uuidof. */
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Disambiguate to choose between the type case and the expression case. */
  if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                       DFS_SINGLE_TYPE_REQUIRED)) {
    /* Scan a type name. */
    is_type = TRUE;
    type_name(&uuidof_type);
    /* If the type is a reference, drop that. */
    if (is_reference_type(uuidof_type)) {
      uuidof_type = type_pointed_to(uuidof_type);
    }  /* if */
  } else {
    /* Scan an expression. */
    /* The expression is not evaluated. */
    an_expr_stack_entry expr_stack_entry;

    push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry,
                    /*force_object_lifetime=*/FALSE,
                    /*suppress_object_lifetime=*/FALSE);
    expr_stack_entry.evaluated = FALSE;
    expr_stack_entry.potentially_evaluated = FALSE;
    is_type = FALSE;
    scan_expr(&operand, PREC_LOWEST, EOPT_NO_OPTIONS);
    /* Rule out indefinite functions. */
    do_operand_transformations(&operand,
                               (TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                                TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                                TOPT_SUPPRESS_MEMBER_FUNC_TO_PM_CONVERSION |
                                TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION));
    uuidof_type = operand.type;
    /* __uuidof(0) is a special case that yields a zero GUID. */
    if (is_constant_operand(&operand) &&
        is_or_might_be_null_pointer_constant(&operand.variant.constant)) {
      uuidof_type = NULL;
    }  /* if */
    pop_expr_stack();
  }  /* if */
  if (uuidof_type != NULL) {
    /* Get down to the underlying type, which must be a class for
       which __declspec(uuid(...)) was specified. */
    /* Drop "array of", "pointer to", etc. to get to the underlying type.
       The function returns NULL if the underlying type does not have
       an associated uuid. */
    a_boolean local_err = FALSE;
    uuidof_type = underlying_uuidof_type(uuidof_type, &template_case,
                                         &local_err);
    if (local_err) {
      /* There is an error type somewhere in the type, so it might have
         a uuid.  Issue no error here. */
      err = TRUE;
      uuidof_type = NULL;
    } else if (uuidof_type == NULL) {
      /* The type has no uuid, or more than one. */
      err = TRUE;
      error(ec_uuidof_requires_uuid_class_type);
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  if (err) {
    make_error_operand(result);
  } else {
    a_constant uuidof_con;
    a_type_ptr const_guid_type = make_qualified_type(
                                               type_of_guid,
                                               (a_type_qualifier_set)TQ_CONST);
    if (template_case) {
      /* For __uuidof a template type, use a ck_template_param. */
      clear_constant(&uuidof_con, (a_constant_repr_kind)ck_template_param);
      set_template_param_constant_kind(&uuidof_con,
                                  (a_template_param_constant_kind)tpck_uuidof);
      uuidof_con.variant.template_param.variant.templ_sizeof.type= uuidof_type;
      if (!is_type) {
        prep_generic_operand(&operand, /*lvalue_expected=*/FALSE);
        uuidof_con.variant.template_param.variant.templ_sizeof.expr =
                                              make_node_from_operand(&operand);
      }  /* if */
      uuidof_con.type = make_pointer_type(const_guid_type);
    } else {
      /* Create an expression node that is the value of a ck_address/abk_uuidof
         constant.  The value of such a constant is the address of the lvalue
         that is the result of the __uuidof operation. */
      make_uuidof_constant(uuidof_type, &uuidof_con);
    }  /* if */
    make_constant_operand(&uuidof_con, result);
    result->state = (an_operand_state)os_lvalue;
    result->type = const_guid_type;
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_uuidof_operator */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* <-- end_position is not used in that case. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_boolean scan_new_style_cast(a_type_ptr        *cast_type,
                                     a_source_position *type_position,
                                     a_source_position *end_position,   
                                     an_operand        *operand)
/*
Scan the sequence "< type-id > ( expression )" as part of a new-style cast.
Return the type in *cast_type (and its position in *type_position) and
the expression in *operand.  The position of the final ")" is returned
in *end_position.  Various error cases are checked for (e.g.,
the type defines something); FALSE is returned if there is an error.
*/
{
  a_boolean err = FALSE, explicit_cv_qualifiers;

  /* Check for and pass over the "<". */
  (void)required_token(tok_lt, ec_exp_lt);
  add_stop_token(tok_gt);
  /* Scan the type.  Note that type_name does not allow definition of types
     in the type-id. */
  *type_position = pos_curr_token;
  type_name_full(cast_type, &explicit_cv_qualifiers);
  /* Do initial checking on the type. */
  err = cast_type_pre_check(cast_type, explicit_cv_qualifiers);
  /* Check for and pass over the ">". */
  (void)required_token(tok_gt, ec_exp_gt);
  remove_stop_token(tok_gt);
  /* Check for and pass over the "(". */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Scan the expression. */
  scan_expr(operand, PREC_LOWEST, EOPT_OPERAND_OF_CAST);
  /* Check for and pass over the ")". */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  *end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  return !err;
}  /* scan_new_style_cast */


static void scan_dynamic_cast_operator(an_operand *result)
/*
Scan the C++ dynamic_cast operator.  See [expr.dynamic.cast].

Syntax:
	dynamic_cast < type-id > ( expression )

*/
{
  a_source_position start_position, type_position, end_position;
  an_operand        operand;
  a_type_ptr        cast_type, underlying_cast_type, operand_type;
  a_type_ptr        operation_type, underlying_operand_type;
  a_type_ptr        underlying_operation_type;
  a_boolean         cast_type_okay, operand_type_okay;
  a_boolean         reference_case = FALSE, err = FALSE, baseward_cast;
  a_boolean         template_param_case = FALSE;
  a_base_class_ptr  bcp;
  an_expr_node_ptr  expr;

  db_enter(4, "scan_dynamic_cast_operator");
  /* Save the position of the dynamic_cast keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* dynamic_cast not possible for preprocessing expressions. */
    internal_error("scan_dynamic_cast_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* New-style casts are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                              &pos_curr_token,
                                              ec_rtti_in_embedded_cplusplus);
  if (curr_expr_kind_is_const()) {
    /* dynamic_cast is not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */
  /* Advance past dynamic_cast. */
  (void)get_token();
  /* Scan "< type-id > ( expression )". */
  if (!scan_new_style_cast(&cast_type, &type_position, &end_position,
                           &operand)) {
    err = TRUE;
  }  /* if */
  if (!err) {
    /* The type cast to must be a pointer or reference to a complete class
       type, or void*. */
    cast_type_okay = FALSE;
    if (is_ptr_or_ref_type(cast_type)) {
      underlying_cast_type = type_pointed_to(cast_type);
      reference_case = is_reference_type(cast_type);
      if (is_class_struct_union_type(underlying_cast_type)) {
        /* Casting to a pointer to a complete class type is okay. */
        complete_class_type_is_needed(underlying_cast_type);
        if (!is_incomplete_type(underlying_cast_type)) {
          cast_type_okay = TRUE;
        } else if (f_skip_typerefs(underlying_cast_type)->
                                 variant.class_struct_union.is_nonreal_class) {
          /* Casting to a pointer or reference to a nonreal type is okay
             in a prototype instantiation. */
        }  /* if */
      } else if (!reference_case && is_void_type(underlying_cast_type)) {
        /* Casting to void * is okay. */
        cast_type_okay = TRUE;
      } else if (is_template_param_type(underlying_cast_type)) {
        /* Casting to a pointer or reference to a template parameter type
           is okay in a prototype instantiation. */
        cast_type_okay = TRUE;
        template_param_case = TRUE;
      }  /* if */
    } else if (is_template_param_type(cast_type)) {
      /* Casting to a template parameter type is okay in a prototype
         instantiation (it might be a pointer or reference type). */
      cast_type_okay = TRUE;
      template_param_case = TRUE;
      underlying_cast_type = type_of_unknown_templ_param_nontype;
    } else {
      /* cast_type is not a pointer or reference type; error. */
      cast_type_okay = FALSE;
    }  /* if */
    if (cast_type_okay) {
      /* The operation type for the cast is the type specified, except that
         for a cast to a reference type it is the corresponding pointer
         type. */
      operation_type = cast_type;
      if (reference_case) {
        operation_type = make_pointer_type(underlying_cast_type);
      }  /* if */
    } else {
      /* Bad dynamic cast type. */
      err = TRUE;
      if (!is_error_type(cast_type)) {
        pos_error(ec_bad_dynamic_cast_type, &type_position);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!err) {
    /* Check the type of the operand. */
    operand_type = operand.type;
    operand_type_okay = FALSE;
    if (is_template_dependent_context() &&
        is_template_dependent_type(operand_type)) {
      /* An operand of unknown type, in a prototype instantiation. */
      operand_type_okay = TRUE;
      template_param_case = TRUE;
    } else if (!reference_case) {
      /* When casting to a pointer type, the operand is treated as an
         rvalue. */
      do_operand_transformations(&operand, TOPT_NO_OPTIONS);
      operand_type = operand.type;
      /* The source operand must be a pointer to a complete class type. */
      underlying_operand_type = NULL;
      if (is_pointer_type(operand_type)) {
        underlying_operand_type = type_pointed_to(operand_type);
        if (is_class_struct_union_type(underlying_operand_type)) {
          complete_class_type_is_needed(underlying_operand_type);
          if (!is_incomplete_type(underlying_operand_type)) {
            operand_type_okay = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!operand_type_okay) {
        /* Bad operand type for a pointer dynamic_cast. */
        err = TRUE;
        if (!is_error_type(operand_type) &&
            (underlying_operand_type == NULL ||
             !is_error_type(underlying_operand_type))) {
          pos_error(ec_bad_ptr_dynamic_cast_operand, &operand.position);
        }  /* if */
      }  /* if */
    } else {
      /* Reference case. */
      /* The source operand must be an lvalue of a complete class type. */
      if (is_an_lvalue(&operand) &&
          is_class_struct_union_type(operand_type)) {
        complete_class_type_is_needed(operand_type);
        if (!is_incomplete_type(operand_type)) {
          operand_type_okay = TRUE;
        }  /* if */
      }  /* if */
      if (operand_type_okay) {
        /* Turn the lvalue into an address so we can deal with it as a
           pointer. */
        take_address_of_lvalue(&operand);
        operand_type = operand.type;
      } else {
        /* Bad operand type for a reference dynamic_cast. */
        err = TRUE;
        if (!is_error_type(operand_type)) {
          pos_error(ec_bad_ref_dynamic_cast_operand, &operand.position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* The source and destination types have been checked separately.  Now see
     if they go together. */
  /* Note that the cast has been turned into pointer form if it was a
     reference cast. */
  if (!err && !template_param_case) {
    /* The cast is not allowed to cast away constness, which really means
       it cannot drop qualifiers.  This is a simple version of that
       test, since only one-level pointers are involved. */
    underlying_operand_type = type_pointed_to(operand_type);
    underlying_operation_type = type_pointed_to(operation_type);
    if (any_qualifier_missing(underlying_operation_type,
                              underlying_operand_type)) {
      pos_st_error(ec_cannot_cast_away_const, &start_position, "dynamic_cast");
    }  /* if */
  }  /* if */
  if (err) {
    /* Some error, previously issued. */
  } else if (template_param_case) {
    /* The source operand type or the destination type is unknown, so
       generate a generic operation. */
    generic_cast_operand(&operand, operation_type, 
                         (an_expr_operator_kind)eok_dynamic_cast,
                         /*is_implicit_cast=*/FALSE,
                         /*is_reference_cast=*/FALSE);
    copy_operand(&operand, result);
  } else if (same_type_with_added_qualifiers(operand_type, operation_type,
                                             /*ignore_qualifiers=*/TRUE,
                                             (a_boolean *)NULL)) {
    /* The types are already the same except for qualifiers.  The result
       is just the source cast to the destination type. */
    cast_operand(operation_type, &operand, /*check_cast_access=*/FALSE,
                 /*is_implicit_cast=*/FALSE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
    copy_operand(&operand, result);
  } else if (related_class_pointers(operand_type, operation_type,
                                    &baseward_cast, &bcp) &&
             baseward_cast) {
    /* This is a known cast from derived to base. */
    base_class_cast_operand(&operand, bcp, (a_boolean *)NULL,
                            /*check_cast_access=*/TRUE,
                            /*is_implicit_cast=*/FALSE,
                            /*implicit_in_naming=*/FALSE,
                            /*is_object_pointer=*/FALSE);
    copy_operand(&operand, result);
  } else {
    /* For all other cases, the dynamic cast is done at runtime.  The operand
       must have a polymorphic class type. */
    if (!is_polymorphic_class_type(underlying_operand_type)) {
      err = TRUE;
      if (!is_error_type(underlying_operand_type)) {
        pos_error(ec_dynamic_cast_operand_must_be_polymorphic,
                  &operand.position);
      }  /* if */
    } else {
      /* Generate an eok_dynamic_cast operation. */
      expr = make_operator_node((an_expr_operator_kind)eok_dynamic_cast,
                                cast_type, /* sic: want reference type. */
                                make_node_from_operand(&operand));
      if (reference_case) {
        expr->implicit_reference_indirection = TRUE;
        expr->variant.operation.is_reference_cast = TRUE;
      }  /* if */
      make_expression_operand(expr, expr->type, result);
      set_used_in_exception_or_rtti_flag(operand_type);
      set_used_in_exception_or_rtti_flag(operation_type);
    }  /* if */
  }  /* if */
  if (err) {
    /* Some error, previously issued. */
    make_error_operand(result);
  } else {
    /* For a dynamic cast to a reference type, the result is an lvalue. */
    if (reference_case) {
      conv_object_pointer_to_lvalue(result);
    }  /* if */
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_dynamic_cast_operator */


static void scan_extended_integral_constant_expression(a_boolean  allow_comma,
                                                       int        prec_level,
                                                       an_operand *operand)
/*
Scan a constant expression that is an extended form of an integral constant
expression.  It is extended in that it allows addressing expressions that
produce an integer constant when cast to an integral type.  The caller
will be casting the result of this call to an integral type.  The
expression is scanned as an initializer constant expression, then checked
to see if it is a constant with integer or floating-point representation.
If not, an error is issued and the constant is changed to an error
constant.  The constant is returned in *operand.  If allow_comma is TRUE,
a top-level comma is allowed in the expression.  prec_level is the
precedence level to be used in scanning the expression.  The constant
returned might be an error constant or a template parameter constant.
This routine exists mainly to allow the sorts of constant expressions
used in the implementation of offsetof.
*/
{
  an_expr_stack_entry expr_stack_entry;
  a_constant          con;

  db_enter(4, "scan_extended_integral_constant_expression");
  push_expr_stack((an_expression_kind)ek_init_constant, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(operand, prec_level, allow_comma ? EOPT_NO_OPTIONS :
                                               EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(operand, TOPT_NO_OPTIONS);
  /* Make a constant from the operand. */
  extract_constant_from_operand(operand, &con);
  /* Check that the constant is represented as an integer or floating
     constant. */
  if (!is_error_constant(&con) &&
      con.kind != (a_constant_repr_kind)ck_integer &&
      con.kind != (a_constant_repr_kind)ck_float &&
      con.kind != (a_constant_repr_kind)ck_template_param) {
    /* The expression doesn't reduce to a value that will be an integer
       constant once cast to an integral type. */
    error_in_operand(ec_expr_not_integral_constant, operand);
  }  /* if */
  pop_expr_stack();
  db_exit();
}  /* scan_extended_integral_constant_expression */


static void scan_intaddr_operator(an_operand *result)
/*
Scan the __INTADDR__ operator.  This is an extension that is used
in the offsetof macro to scan a constant address expression and cast
it to an integer constant.

Syntax:
	__INTADDR__ ( expr )

The parentheses are required.  expr is a constant initializer expression.
A warning about the use of this nonstandard feature would be inappropriate,
because the feature is used to implement offsetof, a standard feature.
*/
{
  a_source_position start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  db_enter(4, "scan_intaddr_operator");
  /* Save the position of the __INTADDR__ keyword. */
  start_position = pos_curr_token;
  /* Check for and pass over the left parenthesis. */
  (void)get_token();
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Scan the address expression. */
  scan_extended_integral_constant_expression(/*allow_comma=*/TRUE,
                                             PREC_LOWEST, result);
  /* Cast the constant to type size_t. */
  cast_operand(integer_type(targ_size_t_int_kind), result,
               /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
               /*is_reinterpret_cast=*/FALSE,
               /*reinterpret_semantics=*/FALSE);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  /* There is no IL operator for __INTADDR__, so we cannot really record the
     expression that formed the resulting constant. */
  result->variant.constant.expr = NULL;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_intaddr_operator */


static a_dynamic_init_ptr add_array_nonconstant_aggregate_init(
                                         a_dynamic_init_ptr element_dip,
                                         a_type_ptr         array_type,
                                         a_type_ptr         elem_type,
                                         a_targ_size_t      number_of_elements)
/*
Change the indicated dynamic initialization into a dynamic initialization
for each member of an array of classes.  array_type is the type of the array,
and elem_type is the type of the array elements.  number_of_elements is the
number of elements in the array, or 0 if the number of elements is variable
(and known only at runtime).  Multi-dimensional arrays are treated as
single-dimensional arrays.  Return a pointer to the dynamic init entry for
the entire array.
*/
{
  a_dynamic_init_ptr  array_dip;

  /* The IL structure is
       new dynamic init (dik_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init (dik_constructor)
  */
  array_dip =
       alloc_expr_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
  repeat_nonconstant_init(element_dip, array_type, elem_type, array_dip,
                          number_of_elements);
  return array_dip;
}  /* add_array_nonconstant_aggregate_init */


a_boolean is_two_argument_delete(a_routine_ptr delete_routine)
/*
Return TRUE if the indicated delete routine is of the two-argument form.
*/
{
  a_boolean                     is_two_arg;
  a_routine_type_supplement_ptr delete_routine_rtsp =
                                        f_skip_typerefs(delete_routine->type)->
                                                    variant.routine.extra_info;
  a_param_type_ptr              param1 = delete_routine_rtsp->param_type_list;

  check_assertion(param1 != NULL);
  is_two_arg = (param1->next != NULL);
  return is_two_arg;
}  /* is_two_argument_delete */


a_boolean new_or_delete_type_requires_array_handling(a_type_ptr type)
/*
type is the base type underlying an array type involved in a new or delete.
Return TRUE if the new or delete operation requires special handling.
Special handling means routines like __vec_new and __vec_delete must be
called, so that constructors and destructors will be called, and so 
that the size of the array is recorded for use at the time of the delete
of the array pointer.
*/
{
  a_boolean special = FALSE;

  if (is_class_struct_union_type(type) &&
      /* Avoid problem with struct for lowered pointer to member in
         IL Lowering. */
      type->source_corresp.assoc_info != NULL) {
    /* Classes with a constructor or destructor require special handling. */
    a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
    if (cssp->constructor != NULL || cssp->destructor != NULL) {
      special = TRUE;
    } else {
      /* Classes with a two-argument array operator delete require special
         handling because the array size must be recorded at the time of the
         new so it can be passed to the delete routine. */
      a_symbol_ptr operator_delete_set =
                opname_member_function_symbol((an_opname_kind)onk_array_delete,
                                              type);
      if (operator_delete_set != NULL) {
        a_boolean    ambiguous;
        a_symbol_ptr operator_delete_symbol =
                         find_default_operator_delete_sym(operator_delete_set,
                                                          &ambiguous);
        if (!ambiguous && operator_delete_symbol != NULL) {
          a_routine_ptr delete_routine;
          a_symbol_ptr  fund_operator_delete =
                                 fundamental_symbol_of(operator_delete_symbol);
          check_assertion(fund_operator_delete->kind ==
                                            (a_symbol_kind)sk_member_function);
          delete_routine = fund_operator_delete->variant.routine.ptr;
          if (is_two_argument_delete(delete_routine)) {
            special = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return special;
}  /* new_or_delete_type_requires_array_handling */


static a_routine_ptr determine_deletion_for_new(
                                           a_type_ptr        base_new_type,
                                           a_symbol_ptr      new_sym,
                                           a_boolean         use_global_delete,
                                           a_source_position *position)
/*
Exceptions are enabled, and a "new" is being scanned.  Determine the
delete routine to be called if an exception is thrown between the time
that the allocation is done and the time the initialization is completed.
Return a pointer to the routine, or NULL if there is an error.  The delete
routine will not necessarily be used, e.g., if the "new" is folded
into a constructor call, but access checking is done for it anyway.
base_new_type is the type of entity being allocated (the element type
if an array is being allocated); new_sym is the "new" routine being
called to do the allocation, stripped to its fundamental symbol;
use_global_delete is TRUE if "::new" was used; and *position gives
the position to be used for errors.
*/
{
  a_routine_ptr delete_routine = NULL;
  a_type_ptr    class_type;
  a_symbol_ptr  delete_sym, overload_delete_sym;
  a_boolean     ambiguous;

  /* Select the delete routine that corresponds to the new routine selected. */
  class_type = NULL;
  if (!use_global_delete && is_class_struct_union_type(base_new_type)) {
    class_type = skip_typerefs(base_new_type);
  }  /* if */
  delete_sym = find_corresponding_operator_delete_sym(new_sym,
                                                      class_type,
                                                      /*template_okay=*/FALSE,
                                                      &ambiguous,
                                                      &overload_delete_sym);
  if (ambiguous) {
    /* The symbol is ambiguous. */
    pos_sy_error(ec_ambiguous_name, position, overload_delete_sym);
  } else if (delete_sym == NULL) {
    /* There is no available appropriate operator delete, so the deletion
       is just not done. */
  } else {
    /* There is an appropriate operator delete. */
    a_symbol_ptr fund_delete_sym = fundamental_symbol_of(delete_sym);

    check_assertion(fund_delete_sym->kind == (a_symbol_kind)sk_routine ||
                    fund_delete_sym->kind ==
                                            (a_symbol_kind)sk_member_function);
    delete_routine = fund_delete_sym->variant.routine.ptr;
    if (delete_sym->is_class_member) {
      /* Check access and ambiguity for class member operator deletes. */
      a_symbol_locator locator_for_delete;
      make_locator_for_symbol(delete_sym, &locator_for_delete);
      locator_for_delete.source_position = *position;
      overload_check_ambiguity_and_verify_access(&locator_for_delete,
                                                 overload_delete_sym);
    }  /* if */
    /* Mark the symbol referenced. */
    record_symbol_reference(SRK_REFERENCE, fund_delete_sym,
                            position, /*update_il_entry=*/FALSE);
  }  /* if */
  return delete_routine;
}  /* determine_deletion_for_new */


static a_dynamic_init_ptr f_make_dyn_init_for_deletion_for_throw(
                                                  a_routine_ptr delete_routine,
                                                  a_boolean     array_new)
/*
Exceptions are enabled, and a "new" with initialization is being scanned.
delete_routine is the operator delete to be called if an exception is
thrown between the time that the allocation is done and the time the
initialization is completed.  array_new is TRUE if the "new" is an
array "new".  Develop a dynamic initialization entry that describes
the deallocation and return a pointer to it.
*/
{
  a_dynamic_init_ptr dyn_init_to_free_storage = NULL;

  if (curr_expr_is_potentially_evaluated()) {
    /* Mark the routine IL entry referenced. */
    mark_routine_referenced(delete_routine);
    /* Mark the routine as called. */
    delete_routine->called = TRUE;
    /* The deletion is recorded in a dynamic initialization entry.
       The delete routine is used as the "destructor". */
    dyn_init_to_free_storage =
                        alloc_expr_dynamic_init((a_dynamic_init_kind)dik_none);
    dyn_init_to_free_storage->destructor = delete_routine;
    dyn_init_to_free_storage->has_temporary_lifetime = TRUE;
    dyn_init_to_free_storage->is_freeing_of_storage_on_exception = TRUE;
    dyn_init_to_free_storage->is_array_freeing = array_new;
    record_end_of_lifetime_destruction(dyn_init_to_free_storage,
                                       /*static_lifetime=*/FALSE,
                                       /*block_lifetime=*/FALSE);
  }  /* if */
  return dyn_init_to_free_storage;
}  /* f_make_dyn_init_for_deletion_for_throw */


/*
Macro used to call f_make_dyn_init_for_deletion_for_throw from within
scan_new_operator.  Sets dyn_init_to_free_storage, if necessary, to
point to a dynamic init entry that will free the storage allocated by
the "new" if an exception is thrown before the initialization of the
storage is completed.  This must be called only after it has been
determined that the "new" has initialization, but before that
initialization is scanned (so the cleanup entry gets onto the object
lifetime list in the right place).
*/
/* Do not record the deletion if no delete routine is needed or if
   allocation is folded into a constructor (new_routine == NULL). */
#define make_dyn_init_for_deletion_for_throw()                        \
{ if (delete_routine != NULL && new_routine != NULL) {                \
    dyn_init_to_free_storage =                                        \
      f_make_dyn_init_for_deletion_for_throw(delete_routine, array_new); \
  }  /* if */                                                         \
}  /* make_dyn_init_for_deletion_for_throw */


static void scan_new_operator(an_operand *result)
/*
Scan the C++ new operator.  See 5.3.3 in the ARM.

Syntax:
      allocation-expression:
		::    new placement    new-type-name new-initializer
		  opt              opt                              opt
		::    new placement    ( type-name ) new-initializer
		  opt              opt                              opt
      placement:
		( expression-list )
      new-initializer:
		( initializer-list    )
		                  opt

new-type-name and the "( type-name )" case are handled by the routine
new_type_name called from this routine.  Note that both forms of type
specification allow a variable-sized array as the top type.
*/
{
  a_boolean         err = FALSE;
  a_source_position start_position, type_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_position new_position;
  a_type_ptr        new_type, base_new_type, ptr_new_type, element_type;
  a_type_ptr        unqual_new_type, unqual_base_new_type;
  an_expr_node_ptr  new_array_dimension, sizeof_node;
  an_operand        sizeof_operand;
  a_boolean         use_global_new = FALSE;
  a_symbol_ptr      operator_new_symbol, function_symbol, ctor_sym;
  a_symbol_ptr      proj_function_symbol;
  a_routine_ptr     ctor_routine, delete_routine = NULL;
  a_boolean         needs_initialization, trapped_left_paren;
  a_boolean         zero_initialization, dependent_initialization;
  a_boolean         value_initialization;
  an_expr_node_ptr  arg_expr_list, init_arg_expr_list, init_val_node;
  a_constant        sizeof_constant;
  an_arg_operand_ptr
                    arg_operand_list, sizeof_arg_operand;
  an_expr_node_ptr  dummy;
  a_boolean         placement_new = FALSE, array_new = FALSE;
  a_targ_size_t     effective_num_of_elements;
  an_arg_match_summary_ptr
                    arg_match_list = NULL;
  a_routine_ptr     new_routine = NULL;
  a_dynamic_init_ptr
                    dyn_init_to_free_storage = NULL;
  a_boolean         saved_inside_conditional_expression =
                                     expr_stack->inside_conditional_expression;
  an_opname_kind    opname_kind;
  a_dynamic_init_ptr
                    dip;
  a_boolean         unknown_dependent_new = FALSE;
  a_boolean         unknown_dependent_ctor = FALSE;

  db_enter(4, "scan_new_operator");

  /* Save the position of the start. */
  copy_source_position(pos_curr_token, start_position);

  if (curr_expr_kind_is_const()) {
    /* "new" not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */

  if (curr_token == tok_colon_colon) {
    /* "::" appears first, meaning use the global new operator. */
    use_global_new = TRUE;
    (void)get_token();
  }  /* if */
#if CHECKING
  if (curr_token != tok_new) {
    internal_error("scan_new_operator: expected new");
  }  /* if */
#endif /* CHECKING */
  copy_source_position(pos_curr_token, new_position);

  (void)get_token();
  /* Check for the presence of the "placement" term, which provides extra
     arguments for the operator new function.  It is a list of expressions
     in parentheses. */
  arg_operand_list = NULL;
  trapped_left_paren = FALSE;
  if (curr_token == tok_lparen) {
    (void)get_token();
    /* Both the placement term and the type can start with a parenthesis.
       Look inside to tell them apart.  For example:
         new (int(1.5)) A     // placement
         new (int(*  ))       // type
    */
    if (is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                         DFS_SINGLE_TYPE_REQUIRED)) {
      /* This is the type name. */
      trapped_left_paren = TRUE;
    } else {
      /* This is the placement expression list. */
      placement_new = TRUE;
      if (curr_token == tok_rparen) {
        /* An empty list is not allowed. */
        error(ec_exp_primary_expr);
        (void)get_token();
      } else {
        /* Scan the expression list as an argument list for which we do not yet
           know the function.  The argument values are returned in a list
           headed by arg_operand_list. */
        scan_call_arguments((a_type_ptr)NULL, (a_routine_ptr)NULL,
                            /*already_after_left_paren=*/TRUE,
                            &dummy, /*overloaded_function_case=*/TRUE,
                            /*unknown_dependent_function=*/FALSE,
                            &arg_operand_list, (a_source_position *)NULL);
      }  /* if */
    }  /* if */
  }  /* if */
  copy_source_position(pos_curr_token, type_position);
  /* Scan the new-type-name or ( type-name ). */
  new_type_name(trapped_left_paren, &new_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  unqual_new_type = skip_typerefs(new_type);
  /* Instantiate the type if it is a template class. */
  complete_type_is_needed(new_type);
  /* Determine the type of pointer returned from "new". */
  base_new_type = new_type;
  new_array_dimension = NULL;
  if (is_array_type(new_type)) {
    /* A "new" of an array returns a pointer to the initial element.
       Note that this is only done for one level, e.g., new int [i][10]
       returns int (*)[10] not int * (ARM 5.3.3). */
    base_new_type = element_type = array_element_type(new_type);
    array_new = TRUE;
    /* Check for a variable size on the first dimension.  Extract the
       expression for the dimension. */
    if (unqual_new_type->variant.array.is_variable_size_array) {
      new_array_dimension =
                     unqual_new_type->variant.array.variant.element_count_expr;
      /* Change the array type to a simple incomplete array type so
         that the variable-size type does not escape from the front end. */
      unqual_new_type->variant.array.is_variable_size_array = FALSE;
      unqual_new_type->variant.array.variant.number_of_elements = 0;
      unqual_new_type->size = 0;
      set_type_size(unqual_new_type);
    } else if (is_incomplete_type(new_type)) {
      /* A case like "new int[]" -- an incomplete array type. */
      pos_error(ec_incomplete_type_not_allowed, &type_position);
      err = TRUE;
    }  /* if */
  }  /* if */
  unqual_base_new_type = skip_typerefs(base_new_type);
  ptr_new_type = make_pointer_type(base_new_type);
  /* Check that the type to be allocated is valid.  It must be an object
     type. */
  if (err) {
    /* Error already issued (operator not valid in this kind of expression). */
  } else if (!is_object_type(base_new_type)) {
    /* Invalid type.  Note that base_new_type is tested instead of
       new_type, so the first-level element type of arrays is tested. */
    if (is_error_type(base_new_type)) {
      /* Error already issued. */
    } else if (is_incomplete_type(base_new_type)) {
      pos_error(ec_incomplete_type_not_allowed, &type_position);
    } else {
      pos_error(ec_type_must_be_object_type, &type_position);
    }  /* if */
    err = TRUE;
  } else if (is_abstract_class_type(new_type)) {
    /* The type is an abstract class type, so an object of the type
       cannot be allocated. */
    report_abstract_class_error(ec_abstract_class_object_not_allowed,
                                new_type, &type_position);
    err = TRUE;
  } else {
    /* Valid type. */
  }  /* if */
  if (array_new) {
     /* For multi-dimensional arrays: even though only one level of array is
        dropped to determine the pointer type, all levels must be dropped
        to get the real base type to do allocation and initialization.  In
        particular, we want to know if the underlying type of a
        multi-dimensional array is a class, so we can know whether or not
        to call a constructor or a class-specific new[].  Note that the
        original first-level element type is retained in element_type. */
    /* Also determine the effective number of elements. */
    if (new_array_dimension != NULL) {
      /* Variable-length array; count is deferred to runtime. */
      effective_num_of_elements = 0;
    } else if (unqual_new_type->variant.array.
                                            is_template_dependent_size_array) {
      /* Template-dependent bound.  Count is constant but not known. */
      effective_num_of_elements = 0;
    } else {
      effective_num_of_elements =
                    unqual_new_type->variant.array.variant.number_of_elements;
    }  /* if */
    while (is_array_type(base_new_type)) {
      if (unqual_base_new_type->variant.array.
                                            is_template_dependent_size_array) {
        /* Arrays whose bounds are given by template-dependent constant
           expressions (in prototype instantiations) have unknown size. */
        effective_num_of_elements = 0;
      } else {
        check_assertion(!has_unknown_specified_bound(unqual_base_new_type));
        effective_num_of_elements *=
                unqual_base_new_type->variant.array.variant.number_of_elements;
      }  /* if */
      base_new_type = array_element_type(unqual_base_new_type);
      unqual_base_new_type = skip_typerefs(base_new_type);
    }  /* while */
  }  /* if */
  function_symbol = proj_function_symbol = NULL;
  if (!err) {
    /* Compute the allocation size in bytes. */
    if (new_array_dimension != NULL) {
      /* The type is a variable-dimension array, as in
           new char[i+1]
         The amount to allocate is the size of the array element times
         the expression giving the number of elements. */
      /* Note that the original first-level element type was retained in
         element_type (that matters for multi-dimension arrays). */
      element_type = skip_typerefs(element_type);
      /* Cast the dimension expression to size_t (it's already an integral
         type). */
      cast_node(&new_array_dimension, integer_type(targ_size_t_int_kind),
                /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
                /*is_reinterpret_cast=*/FALSE, /*reinterpret_semantics=*/FALSE,
                &error_position);
      if (element_type->size == 1) {
        /* If the element size is 1, skip the multiplication. */
        sizeof_node = new_array_dimension;
      } else {
        /* Multiply the number of elements by the size of each element. */
        sizeof_node = node_for_host_large_integer(
               (a_host_large_integer)element_type->size, targ_size_t_int_kind);
        new_array_dimension->next = sizeof_node;
        sizeof_node = make_operator_node((an_expr_operator_kind)eok_imultiply,
                                         sizeof_node->type,
                                         new_array_dimension);
      }  /* if */
      make_expression_operand(sizeof_node, sizeof_node->type, &sizeof_operand);
    } else {
      /* Not a variable-dimension array.  The size is known at compile
         time, as in
           new char[17]
         or
           new int
      */
      set_integer_constant(&sizeof_constant,
                           (a_host_large_integer)unqual_new_type->size,
                           targ_size_t_int_kind);
      make_constant_operand(&sizeof_constant, &sizeof_operand);
    }  /* if */
    /* Add the sizeof operand, in argument operand form, to the front of the
       list of expressions (if any) from the "placement" option.  This gives
       the full set of arguments for the "new" function call. */
    sizeof_arg_operand = alloc_arg_operand();
    copy_operand(&sizeof_operand, &sizeof_arg_operand->operand);
    sizeof_arg_operand->next = arg_operand_list;
    arg_operand_list = sizeof_arg_operand;
    /* Select the proper "new" routine.  If the type is a class type and
       the class has a "new" operator, use it.  However, if "::" preceded
       the keyword "new", always use the global ::new.  Choose new[]
       operators instead of the usual ones if the thing being allocated
       is an array. */
    opname_kind = (an_opname_kind)onk_new;
    if (array_new_and_delete_enabled && array_new) {
      opname_kind = (an_opname_kind)onk_array_new;
    }  /* if */
    operator_new_symbol = NULL;
    if (!use_global_new && (array_new_and_delete_enabled || !array_new)) {
      /* Check for a member "operator new" or "operator new[]". */
      if (is_template_param_or_nonreal_class_type(base_new_type)) {
        /* In a prototype instantiation, you might not be able to tell
           whether a class-specific operator new should be used. */
        unknown_dependent_new = TRUE;
      } else if (is_class_struct_union_type(base_new_type)) {
        operator_new_symbol = opname_member_function_symbol(
                                                         opname_kind,
                                                         unqual_base_new_type);
      }  /* if */
    }  /* if */
    if (operator_new_symbol == NULL && !unknown_dependent_new) {
      /* Use the global "operator new" or "operator new[]". */
      operator_new_symbol = opname_function_symbol(opname_kind);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode &&
          microsoft_version <= 1200 &&
          operator_new_symbol == NULL) {
        /* In Microsoft mode, if no array new is found, search for a
           non-array operator new.  Note that there is no predeclared
           operator new[] in Microsoft mode.  This behavior applies only
           up to MSVC++ 6.0. */
        opname_kind = (an_opname_kind)onk_new;
        operator_new_symbol = opname_function_symbol(opname_kind);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode &&
        microsoft_version > 1200 &&
        opname_kind == (an_opname_kind)onk_array_new) {
      /* As of MSVC++ 7.0, if no array operator new[] is found, try
         looking for a non-array operator new.  Do a tentative match
         on the array new, and if that fails fall back to the non-array
         new.*/
      if ((operator_new_symbol == NULL && !unknown_dependent_new) ||
          !overloaded_function_match_possible(
                                      operator_new_symbol,
                                      /*is_template_id=*/FALSE,
                                      (a_template_arg_ptr)NULL,
                                      arg_operand_list,
                                      /*have_selector=*/FALSE,
                                      (an_operand *)NULL,
                                      /*selector_is_object_pointer=*/TRUE)) {
        opname_kind = (an_opname_kind)onk_new;
        operator_new_symbol = opname_function_symbol(opname_kind);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!unknown_dependent_new) {
      /* Select the proper "new" function if there are several.  Note that
         this call does not adjust the argument types or build the function
         call, since we may yet fold the call into a constructor call. */
      proj_function_symbol = select_overloaded_function(
                                              operator_new_symbol,
                                              /*is_template_id=*/FALSE,
                                              (a_template_arg_ptr)NULL,
                                              /*have_selector=*/FALSE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              /*do_arg_dep_lookup=*/FALSE,
                                              ec_no_matching_new_function,
                                              ec_ambiguous_overloaded_function,
                                              &new_position,
                                              (a_token_sequence_number)0,
                                              (a_boolean *)NULL,
                                              &unknown_dependent_new,
                                              (a_symbol_ptr *)NULL,
                                              &arg_match_list);
      if (proj_function_symbol != NULL) {
        function_symbol = fundamental_symbol_of(proj_function_symbol);
      } else {
        function_symbol = NULL;
      }  /* if */
      /* We check later for function_symbol != NULL.  We don't set err
         here for that case because it shouldn't affect the scanning of
         the initial value. */
    }  /* if */
  }  /* if */
  /* Set ctor_sym non-NULL if the type is a class that has a constructor
     or an array with elements of such a class. */
  ctor_sym = NULL;
  if (is_class_struct_union_type(base_new_type)) {
    ctor_sym = symbol_supplement_for_class(base_new_type)->constructor;
  }  /* if */
  if (!err && function_symbol != NULL) {
    a_boolean access_error_reported;
    /* Work out the "new" routine and its arguments. */
    new_routine = function_symbol->variant.routine.ptr;
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    if (array_new) {
      /* If a allocating an array and a runtime routine will be used, the
         "new" routine can be implicit if it is the default global new[]. */
      if (new_or_delete_type_requires_array_handling(base_new_type)) {
        an_opname_kind array_opname_kind = array_new_and_delete_enabled ?
                                             (an_opname_kind)onk_array_new :
                                             (an_opname_kind)onk_new;
        a_symbol_ptr   sym = opname_function_symbol(array_opname_kind);
        a_boolean      ambiguous;

        /* In Microsoft mode, because the non-array new routine can be used
           for an array new, the symbol can be NULL. */
        if (sym != NULL &&
            function_symbol == find_default_operator_new_sym(sym, &ambiguous)){
          new_routine = NULL;
        }  /* if */
      }  /* if */
    } else {
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
#if NEW_CAN_BE_FOLDED_INTO_CTOR
      /* If allocating a class with a constructor, determine the default
         "new" routine for the class and see whether it is the one that
         was selected.  If so, the "new" call can be folded into the
         constructor call. */
      if (ctor_sym != NULL) {
        /* If the entity gets value-initialization, suppress this
           optimization, because there's no way to tell the constructor
           to do the necessary zeroing after the allocation. */
        a_boolean value_init = (curr_token == tok_lparen &&
                                next_token() == tok_rparen);
        if (!value_init) {
          set_class_assoc_operator_new_routine(unqual_base_new_type);
          if (unqual_base_new_type->variant.class_struct_union.extra_info->
                                   assoc_operator_new_routine == new_routine) {
            new_routine = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    }  /* if */
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
    if (new_routine != NULL) new_routine->called = TRUE;
    /* Mark the "new" routine as referenced, check access to it. */
    overloaded_function_catch_up(proj_function_symbol,
                                 operator_new_symbol,
                                 /*is_qualified_name=*/FALSE,
                                 &new_position,
                                 &new_position,
                                 /*elided_reference=*/(new_routine==NULL),
                                 /*address_taken=*/FALSE,
                                 (an_operand *)NULL,
                                 &access_error_reported);
  }  /* if */
  if (!err && (proj_function_symbol != NULL || unknown_dependent_new)) {
    /* Adjust the argument types, issue any warnings, create an
       argument expression list, and free arg_operand_list and
       arg_match_list. */
    adjust_overloaded_function_call_arguments(proj_function_symbol,
                                              unknown_dependent_new,
                                              (a_type_ptr)NULL,
                                              /*have_selector=*/FALSE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              arg_match_list,
                                              &arg_expr_list);
    /* Avoid freeing the lists twice. */
    arg_operand_list = NULL;
    arg_match_list = NULL;
  }  /* if */
  if (!err && exceptions_enabled && function_symbol != NULL
#if !ABI_CHANGES_FOR_PLACEMENT_DELETE
      /* When placement delete is not supported do not look for a delete
         routine. */
      && !placement_new
#endif /* !ABI_CHANGES_FOR_PLACEMENT_DELETE */
                                                           ) {
    /* Determine the delete routine to be called if an exception is
       thrown before the initialization completes. */
    delete_routine = determine_deletion_for_new(base_new_type,
                                                function_symbol,
                                                use_global_new,
                                                &new_position);
  }  /* if */
  /* If the new routine will be called (and not folded into a constructor),
     the initializer expression is actually inside a conditional expression
     context, because if the allocation fails the initialization will
     not be done. */
  if (new_routine != NULL) expr_stack->inside_conditional_expression = TRUE;
  /* See if the object has or needs initialization.  Note that we need to
     scan the initializer (if there is one) even if an error was detected
     above. */
  needs_initialization = FALSE;
  zero_initialization = FALSE;
  dependent_initialization = FALSE;
  value_initialization = FALSE;
  unknown_dependent_ctor = FALSE;
  ctor_routine = NULL;
  init_val_node = NULL;
  if (curr_token != tok_lparen) {
    /* No new-initializer is present. */
    if (is_class_struct_union_type(base_new_type) &&
        !symbol_supplement_for_class(base_new_type)->is_POD) {
      /* A non-POD class (or array thereof), with no new-initializer. */
      a_boolean is_generated_ctor = FALSE, do_const_test = FALSE;
      /* Look for a default constructor. */
      if (ctor_sym != NULL) {
        /* The class has one or more nontrivial constructors.  Look for
           a default constructor.  The call issues an error and returns NULL
           if no default constructor is found. */
        /* Develop the dynamic init entry, if any, used to free storage
           if an exception is thrown before the initialization is finished.
           This must be done after it has been determined that initialization
           is required, but before the initialization is actually processed. */
        make_dyn_init_for_deletion_for_throw();
        ctor_routine = select_default_constructor(base_new_type,
                                                  &type_position,
                                                  base_new_type,
                                         curr_expr_is_potentially_evaluated());
        init_arg_expr_list = NULL;
        if (ctor_routine != NULL) {
          needs_initialization = TRUE;
          /* Provide default arguments if any. */
          init_arg_expr_list = expr_copy_default_arg_expr_list(ctor_routine,
                skip_typerefs(ctor_routine->type)->variant.routine.extra_info->
                                                              param_type_list);
          do_const_test = TRUE;
          is_generated_ctor = ctor_routine->compiler_generated;
        }  /* if */
      } else if (reference_to_trivial_default_constructor(base_new_type,
                                                          &type_position)) {
        /* The class has an assumed trivial default constructor. */
        do_const_test = TRUE;
        is_generated_ctor = TRUE;
      } else if (is_template_dependent_context() &&
                 unqual_base_new_type->variant.class_struct_union.
                                                            is_nonreal_class) {
        /* A proxy class in a prototype instantiation. */
      } else {
        /* Note that this case comes up if the class type is incomplete.
           An error was issued previously. */
        check_assertion_str(err,
       "scan_new_operator: non-POD class has neither actual nor assumed ctor");
      }  /* if */
      if (do_const_test && (!any_cfront_mode() && !microsoft_mode)) {
        /* When the initializer is omitted on a "new" of a const class
           object, the default constructor is required to be explicitly
           declared; it can't be implicit. */
        if (is_generated_ctor && is_const_qualified_type(new_type)) {
          type_error(ec_missing_default_constructor_on_unnamed_const,
                     unqual_base_new_type);
          err = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Non-class type, or POD class, with no new-initializer.  Check for
         error cases like const entities not being initialized. */
      if (!err) check_for_missing_initializer((a_symbol_ptr)NULL, new_type);
    }  /* if */
  } else {
    /* A new-initializer is present. */
    a_source_position lparen_pos;
    lparen_pos = pos_curr_token;
    /* Advance past the "(". */
    (void)get_token();
    /* No need to add tok_rparen to the stop tokens set: it's done by
       scan_ctor_arguments or scan_parenthesized_initializer_expression. */
    if (array_new && curr_token != tok_rparen) {
      /* No initializer except "()" may be specified for an array type. */
      error(ec_initializer_not_allowed_on_array_new);
      err = TRUE;
    }  /* if */
    if (ctor_sym != NULL) {
      a_boolean empty_parens = (curr_token == tok_rparen);
      /* Class with a (nontrivial) constructor. */
      /* Develop the dynamic init entry, if any, used to free storage
         if an exception is thrown before the initialization is finished.
         This must be done after it has been determined that initialization
         is required, but before the initialization is actually processed. */
      make_dyn_init_for_deletion_for_throw();
      /* Scan the constructor arguments. */
      scan_ctor_arguments(ctor_sym, &init_arg_expr_list, &ctor_routine,
                          &unknown_dependent_ctor,
                          &lparen_pos, base_new_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* In the array case (an error), throw away the argument list. */
      if (array_new) init_arg_expr_list = NULL;
      needs_initialization = (ctor_routine != NULL ||
                              unknown_dependent_ctor);
      /* A "()" initializer implies value initialization. */
      value_initialization = (needs_initialization && empty_parens);
    } else if (is_template_dependent_context() &&
               is_template_dependent_type(new_type)) {
      /* A "new" of a template-dependent type, in a prototype instantiation. */
      scan_dependent_parenthesized_initializer(&dip);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      needs_initialization = TRUE;
      dependent_initialization = TRUE;
    } else {
      /* Not a class with a constructor. */
      if (curr_token != tok_rparen) {
        /* The new-initializer is not empty.  Scan it. */
        /* Develop the dynamic init entry, if any, used to free storage
           if an exception is thrown before the initialization is finished.
           This must be done after it has been determined that initialization
           is required, but before the initialization is actually processed. */
        make_dyn_init_for_deletion_for_throw();
        init_val_node = scan_parenthesized_initializer_expression(
                                                      err ? error_type() :
                                                            new_type,
                                                      ec_bad_initializer_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        needs_initialization = TRUE;
      } else {
        /* The initializer is empty, i.e., "()".  This means
           value-initialization.  Note that "()" for class types with
           (nontrivial) constructors is handled above, however, so
           value-initialization here is effectively zero-initialization. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        (void)get_token();
        needs_initialization = TRUE;
        zero_initialization = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  expr_stack->inside_conditional_expression =
                                           saved_inside_conditional_expression;
  /* Now build the IL for the operation. */
  if (err || (function_symbol == NULL && !unknown_dependent_new)) {
    /* Some error. */
    make_error_operand(result);
  } else {
    an_expr_node_ptr            new_node;
    a_new_delete_supplement_ptr ndsp;

    /* Use an enk_new_delete node to represent the "new". */
    new_node = alloc_expr_node((an_expr_node_kind)enk_new_delete);
    new_node->type = ptr_new_type;
    ndsp = new_node->variant.new_delete;
    ndsp->is_new = TRUE;
    ndsp->placement_new = placement_new;
    ndsp->global_new_or_delete = use_global_new;
    ndsp->type = new_type;
    /* Put the routine and argument list into the supplement.  Note that
       the argument list is present even when the routine is NULL -- that's
       necessary so that the array size is available when the number of
       elements is nonconstant. */
    ndsp->routine = new_routine;
    ndsp->arg = arg_expr_list;
    if (needs_initialization) {
      /* The allocated space must be initialized.  A dynamic init entry is
         used. */
      if (ctor_routine != NULL || unknown_dependent_ctor) {
        /* Constructor call. */
        dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_constructor);
        dip->variant.constructor.ptr = ctor_routine;
        dip->variant.constructor.args = init_arg_expr_list;
        dip->variant.constructor.value_initialization = value_initialization;
        if (array_new) {
          /* The entity is an array whose elements have a class type that
             has a default constructor.  Use a dik_nonconstant_aggregate
             initialization. */
          /* If exceptions are enabled, put in a destructor.  It's needed
             to destroy elements if a throw is done part-way through the
             initialization of the array. */
          if (exceptions_enabled) {
            dip->destructor = select_destructor(
                                        base_new_type, base_new_type,
                                        &type_position,
                                        /*honor_virtual=*/FALSE,
                                        curr_expr_is_potentially_evaluated());
            if (dip->destructor != NULL) {
              dip->destruction_is_for_partially_constructed_aggregate = TRUE;
            }  /* if */
          }  /* if */
          dip = add_array_nonconstant_aggregate_init(dip, new_type,
                                                     base_new_type,
                                                    effective_num_of_elements);
        }  /* if */
      } else if (zero_initialization) {
        /* Zero-initialization. */
        dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_zero);
      } else if (dependent_initialization) {
        /* Template-dependent initialization (i.e., we don't know the
           type).  dip is already set. */
      } else {
        /* Expression as initial value. */
        dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_expression);
        dip->variant.expression = init_val_node;
      }  /* if */
      ndsp->dynamic_init = dip;
      /* Remember the dynamic init entry, if any, used to free storage
         if an exception is thrown before the initialization is finished. */
      ndsp->freeing_of_storage_on_exception = dyn_init_to_free_storage;
    }  /* if */
    /* Make an operand for the result. */
    make_expression_operand(new_node, ptr_new_type, result);
  }  /* if */
  /* Free the lists if they have not been freed already. */
  if (arg_operand_list != NULL) {
    /* This list is only non-NULL if there was an error and the list was not
       used, so change its references to errors. */
    change_arg_operand_list_refs_to_error(arg_operand_list);
    free_arg_operand_list(arg_operand_list);
  }  /* if */
  free_arg_match_summary_list(arg_match_list);
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_new_operator */


static a_routine_ptr select_delete_routine(a_type_ptr        delete_type,
                                           a_boolean         use_global_delete,
                                           a_boolean         array_delete,
                                           a_source_position *delete_position)
/*
Determine the delete routine to be used to delete an object of type
delete_type, and return a pointer to the routine entry.  use_global_delete
indicates that a global delete routine should be used even if there is
a class-specific delete routine.  array_delete indicates that the deletion
is of an array (delete_type in that case is the type pointed to, i.e.,
the element type).  The symbol is marked as referenced at *delete_position,
but the IL entry is not marked as referenced.  This routine returns NULL
if the selected delete routine is ambiguous.
*/
{
  a_symbol_ptr   operator_delete_set = NULL, operator_delete_symbol = NULL;
  a_routine_ptr  delete_routine = NULL;
  an_opname_kind opname_kind;

  /* Select the proper "delete" routine.  If the type is a class type and
     the class has a "delete" operator, use it, unless a global delete routine
     is forced. */
  opname_kind = (an_opname_kind)onk_delete;
  /* For arrays, use "operator delete[]" instead of "operator delete". */
  if (array_new_and_delete_enabled && array_delete) {
    opname_kind = (an_opname_kind)onk_array_delete;
  }  /* if */
  if (!use_global_delete && (array_new_and_delete_enabled || !array_delete)) {
    /* See if the underlying type is a class. */
    if (is_class_struct_union_type(delete_type)) {
      /* Look for a class-specific "operator delete" or "operator delete[]". */
      operator_delete_set = opname_member_function_symbol(opname_kind,
                                                          delete_type);
    }  /* if */
  }  /* if */
  if (operator_delete_set == NULL) {
    /* Use the global "operator delete" or "operator delete[]". */
    operator_delete_set = opname_function_symbol(opname_kind);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && operator_delete_set == NULL) {
      /* In Microsoft mode, if no array delete is found, search for a
         non-array operator delete.  Note that there is no predeclared
         operator delete[] in Microsoft mode. */
      opname_kind = (an_opname_kind)onk_delete;
      operator_delete_set = opname_function_symbol(opname_kind);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (operator_delete_set != NULL) {
    a_boolean ambiguous;
    /* Pick the default operator delete out of an overload set, if any. */
    operator_delete_symbol =
                         find_default_operator_delete_sym(operator_delete_set,
                                                          &ambiguous);
    if (ambiguous) {
      /* The symbol is ambiguous. */
      pos_sy_error(ec_ambiguous_name, delete_position, operator_delete_set);
      operator_delete_symbol = NULL;
    } else if (operator_delete_symbol == NULL) {
      /* There is no available default operator delete.  (Perhaps this is
         a class with an operator delete, but there's no default operator
         delete.) */
      pos_error(ec_no_appropriate_delete, delete_position);
    } else {
      /* There is a default operator delete. */
      a_symbol_ptr fund_operator_delete =
                                 fundamental_symbol_of(operator_delete_symbol);
      /* Note that a template function is not possible here, since a template
         is not allowed to be the default delete. */
      check_assertion(fund_operator_delete->kind ==
                                                   (a_symbol_kind)sk_routine ||
                      fund_operator_delete->kind ==
                                            (a_symbol_kind)sk_member_function);
      delete_routine = fund_operator_delete->variant.routine.ptr;
      if (operator_delete_symbol->is_class_member) {
        /* Check access and ambiguity for class member operator deletes. */
        a_symbol_locator locator_for_delete;
        make_locator_for_symbol(operator_delete_symbol, &locator_for_delete);
        locator_for_delete.source_position = *delete_position;
        overload_check_ambiguity_and_verify_access(&locator_for_delete,
                                                   operator_delete_set);
      }  /* if */
      /* Mark the routine symbol referenced, but not the IL entry (yet). */
      record_symbol_reference(SRK_REFERENCE, fund_operator_delete,
                              delete_position, /*update_il_entry=*/FALSE);
    }  /* if */
  }  /* if */
  return delete_routine;
}  /* select_delete_routine */


static void scan_delete_operator(an_operand *result)
/*
Scan the C++ delete operator.  See 5.3.4 in the ARM.

Syntax:
      deallocation-expression:
		::    delete cast-expression
		  opt
		::    delete [ ] cast-expression
		  opt

As an anachronism, allow an expression inside the [ ].
*/
{
  a_source_position  start_position, delete_position;
  a_type_ptr         delete_type, ptr_delete_type, base_delete_type;
  an_expr_node_ptr   ptr_node, delete_node;
  a_boolean          use_global_delete = FALSE, is_constant, array_delete;
  a_boolean          err = FALSE, processed = FALSE, template_case = FALSE;
  a_routine_ptr      delete_routine = NULL, dtor_routine = NULL;
  an_operand         operand;
  a_constant         constant;
  an_expr_node_ptr   expr;
  a_dynamic_init_ptr dip;
  a_new_delete_supplement_ptr
                     ndsp;

  db_enter(4, "scan_delete_operator");

  /* Save the position of the start. */
  copy_source_position(pos_curr_token, start_position);

  if (curr_expr_kind_is_const()) {
    /* "delete" not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */

  if (curr_token == tok_colon_colon) {
    /* "::" appears first, meaning use the global delete operator. */
    use_global_delete = TRUE;
    (void)get_token();
  }  /* if */
#if CHECKING
  if (curr_token != tok_delete) {
    internal_error("scan_delete_operator: expected delete");
  }  /* if */
#endif /* CHECKING */
  copy_source_position(pos_curr_token, delete_position);
  (void)get_token();

  array_delete = FALSE;
  if (curr_token == tok_lbracket) {
    /* The [ ] for array deletion is present. */
    array_delete = TRUE;
    (void)get_token();
    add_matching_stop_token(tok_rbracket);
    if (curr_token != tok_rbracket) {
      /* Anachronism -- there's an expression between the brackets, presumably
         indicating the number of elements in the array. */
      an_error_severity sev = anachronism_error_severity;
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* This anachronism is allowed in Microsoft mode. */
      if (microsoft_mode) sev = (an_error_severity)es_warning;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      diagnostic(sev, ec_delete_count_anachronism);
      scan_nonconstant_dimension_expression(/*is_vla_decl=*/FALSE,
                                            &is_constant, &expr, &constant);
      /* The expression is ignored. */
    }  /* if */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    remove_matching_stop_token(tok_rbracket);
  }  /* if */
  /* Scan the pointer expression. */
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);
  if (is_template_dependent_context() &&
      (is_template_param_or_nonreal_class_type(operand.type) ||
       (is_pointer_type(operand.type) &&
        is_template_param_or_nonreal_class_type(
                                            type_pointed_to(operand.type))))) {
    /* A template parameter type or nonreal class type in a prototype
       instantiation. */
    template_case = TRUE;
  } else if (is_class_struct_union_type(operand.type)) {
    /* Convert from a class type to a pointer type if necessary. */
    try_to_convert_class_operand_to_builtin_type(&operand,
                                                 (a_builtin_type_kind_set)
                                                                  BTK_POINTER,
                                                 &processed);
  }  /* if */
  if (!processed) {
    do_operand_transformations(&operand, TOPT_NO_OPTIONS);
    /* The operand of a delete must be a pointer. */
    if (!err && !template_case) {
      if (!check_pointer_operand(&operand, ec_expr_not_pointer)) err = TRUE;
    }  /* if */
  } else if (is_error_operand(&operand)) {
    err = TRUE;
  }  /* if */
  if (!err) {
    ptr_delete_type = operand.type;
    if (template_case) {
      delete_type = type_of_unknown_templ_param_nontype;
    } else {
      delete_type = type_pointed_to(ptr_delete_type);
      if (is_function_type(delete_type)) {
        /* The type pointed to may not be a function type. */
        error_in_operand(ec_delete_of_function_pointer, &operand);
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    make_error_operand(result);
  } else {
    /* Valid type. */
    ptr_node = make_node_from_operand(&operand);
    /* Use an enk_new_delete node to represent the delete. */
    delete_node = alloc_expr_node((an_expr_node_kind)enk_new_delete);
    delete_node->type = void_type();
    ndsp = delete_node->variant.new_delete;
    ndsp->is_new = FALSE;
    ndsp->array_delete = array_delete;
    ndsp->global_new_or_delete = use_global_delete;
    ndsp->type = delete_type;
    ndsp->arg = ptr_node;
    delete_type = skip_typerefs(delete_type);
    base_delete_type = delete_type;
    /* Get the underlying type for any array type. */
    while (is_array_type(base_delete_type)) {
      base_delete_type = array_element_type(base_delete_type);
      base_delete_type = skip_typerefs(base_delete_type);
    }  /* if */
    /* See if the object needs destruction. */
    if (is_class_struct_union_type(base_delete_type)) {
      /* Instantiate the class if it is a template class. */
      complete_type_is_needed(base_delete_type);
      if (is_incomplete_type(base_delete_type)) {
        /* Deleting a pointer to an incomplete class.  Give a warning,
           because we may not know how to do the right thing (like call
           a destructor). */
        pos_warning(ec_delete_of_incomplete_class, &operand.position);
      }  /* if */
      dtor_routine = select_destructor(base_delete_type, base_delete_type,
                                       &operand.position,
                                       /*honor_virtual=*/TRUE,
                                       curr_expr_is_potentially_evaluated());
      if (dtor_routine != NULL) {
        /* Class with destructor.  Destruction is required. */
        dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_none);
        if (array_delete) {
          /* For a delete of an array of classes, generate a dynamic init
             that replicates the destructor call for the whole array. */
          a_type_ptr array_type = alloc_type((a_type_kind)tk_array);
          array_type->variant.array.element_type = base_delete_type;
          /* Array size is left as zero; size need not be set. */
          /* The destruction, if any, is indicated both at the array level
             (for the full delete) and at the element level (for cleanup if
             an exception is thrown during the processing). */
          if (exceptions_enabled) {
            dip->destructor = dtor_routine;
            dip->destruction_is_for_partially_constructed_aggregate = TRUE;
          }  /* if */
          dip = add_array_nonconstant_aggregate_init(dip, array_type,
                                                     base_delete_type,
                                                     (a_targ_size_t)0);
        }  /* if */
        dip->destructor = dtor_routine;
        ndsp->dynamic_init = dip;
      }  /* if */
    }  /* if */
    /* Select the proper "delete" routine.  If the type is a class type and
       the class has a "delete" operator, use it.  However, if "::" preceded
       the keyword "delete", always use the global ::delete. */
    if (!template_case) {
      delete_routine = select_delete_routine(base_delete_type,
                                             use_global_delete,
                                             array_delete,
                                             &delete_position);
    }  /* if */
    /* Note that delete_routine will be NULL if an ambiguity was found or
       when template_case is TRUE. */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    if (array_delete && !template_case) {
      /* If a deleting an array and a runtime routine will be used, the
         delete routine can be implicit if it is the default global
         delete. */
      if (new_or_delete_type_requires_array_handling(base_delete_type)) {
        an_opname_kind array_opname_kind = array_new_and_delete_enabled ?
                                             (an_opname_kind)onk_array_delete :
                                             (an_opname_kind)onk_delete;
        a_symbol_ptr   sym = opname_function_symbol(array_opname_kind);
        a_boolean      ambiguous;

        /* In Microsoft mode, because the non-array delete routine can be
           used for an array delete, the symbol can be NULL. */
        if (sym != NULL) {
          sym = find_default_operator_delete_sym(sym, &ambiguous);
        }  /* if */
        if (sym != NULL && delete_routine == sym->variant.routine.ptr) {
          delete_routine = NULL;
        }  /* if */
        /* Mark the destructor as referenced if it is virtual, because
           the call from the runtime routine will not be virtual (nor
           need it be, since this is an array of the class type). */
        if (dtor_routine != NULL && dtor_routine->is_virtual) {
          if_evaluating_mark_routine_referenced(dtor_routine);
        }  /* if */
      }  /* if */
    } else {
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
#if DELETE_CAN_BE_FOLDED_INTO_DTOR
      if (dtor_routine != NULL) {
        /* For a class with a destructor, see if the delete can be folded
           into the destructor. */
        a_type_ptr unqual_base_delete_type = skip_typerefs(base_delete_type);
        /* Determine and remember the default operator delete() routine for
           the class. */
        set_class_assoc_operator_delete_routine(unqual_base_delete_type,
                                                (a_routine_ptr)NULL);
        /* If the delete routine we are using is the default for the class,
           and the class has a destructor, we can fold the delete into the
           destructor call. */
        if (unqual_base_delete_type->variant.class_struct_union.extra_info->
                             assoc_operator_delete_routine == delete_routine) {
          delete_routine = NULL;
        }  /* if */
      }  /* if */
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    }  /* if */
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
    if (delete_routine != NULL) {
      /* The delete routine is actually being called. */
      /* Mark the routine referenced. */
      if_evaluating_mark_routine_referenced(delete_routine);
      /* Mark the routine as called. */
      delete_routine->called = TRUE;
    }  /* if */
    ndsp->routine = delete_routine;
    /* Make an operand for the result. */
    make_expression_operand(delete_node, void_type(), result);
  }  /* if */

  set_operand_position(result, &start_position, &operand.end_position,
                       &start_position);
  db_exit();
}  /* scan_delete_operator */


an_expr_node_ptr make_lvalue_cast_node(an_expr_node_ptr source_expr,
                                       a_type_ptr       type_cast_to)
/*
Make an lvalue cast expression node that casts source_expr to type_cast_to.
This is used only in C mode, and it's an extension.
*/
{
  an_expr_node_ptr lvalue_cast_node;

  check_assertion_str(C_mode(),
                      "make_lvalue_cast_node: lvalue cast in C++ mode");
  lvalue_cast_node = make_operator_node((an_expr_operator_kind)eok_lvalue_cast,
                                        make_pointer_type(type_cast_to),
                                        source_expr);
  return lvalue_cast_node;
}  /* make_lvalue_cast_node */


static void lvalue_cast(a_type_ptr type_cast_to,
                        an_operand *result)
/*
Cast an operand for an lvalue (result) to a new type.  This "lvalue cast" is
only done in C mode, and it's an extension.
*/
{
  an_expr_node_ptr temp_node;

  check_assertion_str(C_mode(), "lvalue_cast: lvalue cast in C++ mode");
  /* Build an expression node for the lvalue cast.  Note that this is done
     even if the lvalue address is represented by a constant. */
  temp_node = make_lvalue_cast_node(make_node_from_operand(result),
                                    type_cast_to);
  /* Make an expression operand for the node.  Change the old one rather
     than creating a new one so as not to disturb the other fields in the
     operand. */
  set_operand_kind(result, (an_operand_kind)ok_expression);
  result->variant.expression = temp_node;
  result->type = type_cast_to;
}  /* lvalue_cast */


static a_boolean cast_type_pre_check(a_type_ptr *p_type_cast_to,
                                     a_boolean  has_explicit_cv_qualifiers)
/*
Do a first check on the destination type of a cast to see if it is legal.
This is very top-level checking applicable to all casts.  Return TRUE if
there is an error.  *p_type_cast_to is the destination type of the cast,
which may be updated on return if the cast should be to some other type.
If explicit_cv_qualifiers is set, warn about those qualifiers being useless
when the type cast to is a nonclass type.
This routine is called for C-style casts, C++ functional-notation type
conversions, and C++ new-style casts.  The current error_position must
be set to the source position of the type.
*/
{
  a_boolean  err = FALSE;
  a_type_ptr type_cast_to = *p_type_cast_to;

  /* Instantiate the type if it is a template class. */
  complete_type_is_needed(type_cast_to);
  /* Check the type to see if it's permissible. */
  if (is_error_type(type_cast_to)) {
    err = TRUE;
  } else if (is_template_param_type(type_cast_to)) {
    /* We are in a prototype instantiation of a template.  The type is
       a template parameter type, i.e., we don't know what it is.  Assume
       it's okay and go on. */
  } else if (is_incomplete_type(type_cast_to) && !is_void_type(type_cast_to)) {
    /* This check catches incomplete enum types. */
    error(ec_incomplete_type_not_allowed);
    err = TRUE;
  } else if (is_class_struct_union_type(type_cast_to)) {
    /* Cast to a class type. */
    if (!C_mode()) {
      /* In C++, a cast to a class is allowed. */
      /* But not in a constant expression. */
      if (curr_expr_kind_is_const()) {
        error(ec_expr_not_constant);
        err = TRUE;
      }  /* if */
      /* But not a cast to an abstract class. */
      if (is_abstract_class_type(type_cast_to) &&
          /* Except in Microsoft mode. */
          !microsoft_bugs) {
        report_abstract_class_error(ec_cast_to_abstract_class, type_cast_to,
                                    &error_position);
        err = TRUE;
      }  /* if */
    } else {
#if GNU_EXTENSIONS_ALLOWED
      /* GNU C permits casting from a scalar to a union if the scalar's type
         is the type of a member of the union.  The detailed check happens
         in conversion_possible.  Also, do-nothing casts to struct types
         are allowed. */
      if (!gcc_mode)
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        /* In C, a cast to a class type is not allowed.  Note that compound
           literal cases do not get here. */
        type_error(ec_cast_to_bad_type, type_cast_to);
        err = TRUE;
      }  /* if */
    }  /* if */
  } else if (is_array_type(type_cast_to)) {
    /* Casting to an array type is not allowed. */
    if (cfront_2_1_mode) {
      /* In cfront 2.1 mode, treat a cast to an array type as a cast to
         a pointer to. */
      *p_type_cast_to = type_cast_to =
                      type_after_array_to_pointer_transformation(type_cast_to);
      type_warning(ec_nonstd_array_cast, type_cast_to);
    } else {
      /* Normal case.  Casting to an array type is an error. */
      type_error(ec_cast_to_bad_type, type_cast_to);
      err = TRUE;
    }  /* if */
  } else if (is_function_type(type_cast_to)) {
    /* Casting to a function type is not allowed. */
    type_error(ec_cast_to_bad_type, type_cast_to);
    err = TRUE;
  }  /* if */
  if (!err) {
    /* Casting to a qualified type, though valid, is pointless.  This is
       reported only when the cv-qualifiers are explicit in the cast, not,
       for example, when they are hidden in a typedef or template parameter
       type. */
    if (has_explicit_cv_qualifiers) {
      check_assertion(is_qualified_type(type_cast_to));
      if (!C_mode() && is_class_struct_union_type(type_cast_to)) {
        /* In C++ class rvalues can have qualifiers, so casting to a
           cv-qualified class type is okay. */
      } else if (microsoft_bugs) {
        /* Microsoft mode allows some lvalue casts where cv-qualifiers
           matter, so give no warning. */
      } else {
        warning(ec_cast_to_qualified_type);
        *p_type_cast_to = type_cast_to = make_unqualified_type(type_cast_to);
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) *p_type_cast_to = error_type();
  return err;
}  /* cast_type_pre_check */


static an_expr_node_ptr make_node_from_void_expression_operand(
                                                           an_operand *operand)
/*
*operand is an expression scanned as a void expression, or cast to void.
Determine an expression representation for the operand, and return a pointer
to the expression.  If the expression is an lvalue (possible only in C++),
set the void_expression_lvalue flag in the expression.
*/
{
  an_expr_node_ptr node = make_node_from_operand(operand);

  if (is_an_lvalue(operand)) {
    check_assertion(!C_mode());
    node->void_expression_lvalue = TRUE;
  }  /* if */
  return node;
}  /* make_node_from_void_expression_operand */


static void cast_operand_to_void(an_operand *operand,
                                 a_type_ptr type_cast_to)
/*
Cast the indicated operand to void.  This is used for explicit casts
to void.  type_cast_to gives the (possibly cv-qualified) void type.
Lvalue-to-rvalue transformations are done on the operand if appropriate
(yes in C, no in C++).  Other transformations are done in all cases.
*/
{
  an_expr_node_ptr             node;
  a_transformation_options_set options = TOPT_NO_OPTIONS;

  if (!C_mode()) {
    /* In C++, lvalue-to-rvalue transformations are not done on an expression
       cast to void. */
    options |= (TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION);
  }  /* if */
  do_operand_transformations(operand, options);
  /* For casts to void, we build an expression node that is a cast
     to void.  This special cast to void is only used for the
     case handled here, i.e., for an explicit cast to void.
     Later, in simplify_void_operand, the cast will probably be
     removed.  cast_operand is not used because we do not wish to
     try to change the types of constants to void.  We do not call
     simplify_void_operand here because (a) we want to keep the
     explicit cast to void as a signal to suppress the warning
     about an expression with no effect, and (b) we want to keep
     a non-NULL expression pointer all the way up to avoid
     special-case checks. */
  node = make_node_from_void_expression_operand(operand);
  node = make_operator_node((an_expr_operator_kind)eok_cast,
                            type_cast_to,
                            node);
  make_expression_operand(node, type_cast_to, operand);
}  /* cast_operand_to_void */


static a_boolean cast_is_valid_in_current_expression_kind(
                                     an_operand               *operand,
                                     a_type_ptr               dest_type,
                                     a_local_expr_options_set local_options,
                                     a_source_position        *type_position)
/*
Return TRUE if a cast of operand to dest_type is valid in the current kind
of expression.  local_options is the set of local expression options.
type_position gives the source position of the type in the cast.
Note that this routine does not do all validity checking.  It only checks
for certain restrictions that apply in certain kinds of expressions,
but apply for all kinds of casts.  If the operand is supposed to undergo
array --> pointer (etc.) transformations, they should have been done before
this routine is called.
*/
{
  a_boolean  err = FALSE;
  a_type_ptr source_type = operand->type;

  if (curr_expr_kind_is(ek_integral_constant)) {
    /* Only casts from arithmetic to integral or enum types are permitted in
       integral constant expressions. */
    if (is_integral_or_enum_type(dest_type)) {
      /* Okay, cast is to integral type. */
      /* The cast should be from an arithmetic or enum type. */
      if (is_arithmetic_or_enum_type(source_type)) {
        /* Okay. */
      } else if (is_pointer_type(source_type) &&
                 is_constant_operand(operand) &&
                 operand->variant.constant.kind ==
                                            (a_constant_repr_kind)ck_integer) {
        /* As an extension, allow pointer --> int for pointer constants
           that come from casting an integer constant to a pointer type,
           as in (int)(char *)1. */
        if (strict_ansi_mode) {
          pos_diagnostic(strict_ansi_error_severity,
                         enum_type_is_integral ?
                           ec_expr_not_arithmetic :
                           ec_expr_not_arithmetic_or_enum,
                         &operand->position);
          err = (strict_ansi_error_severity == es_error);
        }  /* if */
      } else if (is_template_param_type(source_type)) {
        /* Casting from an unknown template parameter type is okay. */
      } else {
        /* The destination type is integral, but the source type is not
           arithmetic. */
        if (!is_error_type(source_type)) {
          pos_error(enum_type_is_integral ?
                      ec_expr_not_arithmetic : ec_expr_not_arithmetic_or_enum,
                    &operand->position);
        }  /* if */
        err = TRUE;
      }  /* if */
    } else if ((local_options & (EOPT_OPERAND_OF_CAST |
                                 EOPT_MICROSOFT_CASE_LABEL)) &&
               is_pointer_type(dest_type) &&
               (is_integral_or_enum_type(source_type) ||
                is_template_param_type(source_type))) {
      /* When the cast is the immediate operand of another cast, allow
         integer --> pointer as an extension.  Also allowed for a case
         label in Microsoft mode. */
      if (strict_ansi_mode) {
        pos_diagnostic(strict_ansi_error_severity,
                       enum_type_is_integral ?
                         ec_cast_not_integral :
                         ec_cast_not_integral_or_enum,
                       type_position);
        err = (strict_ansi_error_severity == es_error);
      }  /* if */
    } else if (is_template_param_type(dest_type)) {
      /* Casting to an unknown template parameter type is okay. */
    } else {
      /* Casting to a non-integral type in an integral constant expression. */
      if (!is_error_type(dest_type)) {
        pos_error(enum_type_is_integral ?
                    ec_cast_not_integral : ec_cast_not_integral_or_enum,
                  type_position);
      }  /* if */
      err = TRUE;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (gcc_mode &&
             is_class_struct_union_type(dest_type) &&
             f_identical_types(f_skip_typerefs(source_type),
                               f_skip_typerefs(dest_type),
                               ITF_NO_FLAGS)) {
    /* GNU C allows a do-nothing cast to a struct or union type. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (curr_expr_kind_is(ek_init_constant)) {
    /* Initializer constant expression: arithmetic/enum --> arithmetic/enum
       and scalar --> pointer are allowed, pointer --> integral as
       an extension. */
    if (is_arithmetic_or_enum_type(dest_type)) {
      /* Casting to arithmetic or enum; source must be arithmetic or enum. */
      if (is_arithmetic_or_enum_type(source_type)) {
        /* Okay. */
      } else if (is_pointer_type(source_type) &&
                 is_integral_type(dest_type)) {
        /* Pointer --> integral.  Allowed as an extension.  The check
           that the integral type is large enough is done in
           reinterpret_cast_conversion_possible. */
        if (strict_ansi_mode) {
          pos_diagnostic(strict_ansi_error_severity,
                         enum_type_is_integral ?
                           ec_expr_not_arithmetic :
                           ec_expr_not_arithmetic_or_enum,
                         &operand->position);
          err = (strict_ansi_error_severity == es_error);
        }  /* if */
      } else if (is_template_param_type(source_type)) {
        /* Casting from an unknown template parameter type is okay. */
      } else {
        /* Non-arithmetic --> arithmetic or enum. */
        if (!is_error_type(source_type)) {
          pos_error(enum_type_is_integral ?
                      ec_expr_not_arithmetic : ec_expr_not_arithmetic_or_enum,
                    &operand->position);
        }  /* if */
        err = TRUE;
      }  /* if */
    } else if (is_pointer_type(dest_type)) {
      /* Casting to pointer; source must be scalar. */
      if (is_scalar_type(source_type) ||
          is_template_param_type(source_type)) {
        /* Okay. */
      } else {
        pos_error(enum_type_is_integral ?
                    ec_expr_not_scalar :
                    ec_expr_not_arithmetic_or_enum_or_pointer,
                  &operand->position);
        err = TRUE;
      }  /* if */
    } else if (is_template_param_type(dest_type)) {
      /* Casting to an unknown template parameter type is okay. */
    } else {
      /* Casting to a non-scalar type in an initializer expression. */
      if (!is_error_type(dest_type)) {
        pos_error(enum_type_is_integral ?
                    ec_cast_not_scalar :
                    ec_cast_not_arithmetic_or_enum_or_pointer,
                  type_position);
      }  /* if */
      err = TRUE;
    }  /* if */
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Only casts between integral or enum types are allowed in nontype
       template arguments. */
    if (is_integral_or_enum_type(dest_type)) {
      /* Destination is integral or enum.  Source should be arithmetic or
         enum (cast from float to integral is allowed). */
      if (is_arithmetic_or_enum_type(source_type)) {
        /* Okay. */
      } else if (is_pointer_type(source_type) &&
                 is_constant_operand(operand) &&
                 operand->variant.constant.kind ==
                                            (a_constant_repr_kind)ck_integer) {
        /* As an extension, allow pointer --> int for pointer constants
           that come from casting an integer constant to a pointer type,
           as in (int)(char *)1. */
        if (strict_ansi_mode) {
          pos_diagnostic(strict_ansi_error_severity,
                         enum_type_is_integral ?
                           ec_expr_not_arithmetic :
                           ec_expr_not_arithmetic_or_enum,
                         &operand->position);
          err = (strict_ansi_error_severity == es_error);
        }  /* if */
      } else if (is_template_param_type(source_type)) {
        /* Casting from an unknown template parameter type is okay. */
      } else {
        /* Cast from non-arithmetic to integral in a nontype template
           argument. */
        if (!is_error_type(source_type)) {
          pos_error(ec_non_arith_operation_in_templ_arg, &operand->position);
        }  /* if */
        err = TRUE;
      }  /* if */
    } else if (floating_point_template_parameters_allowed &&
               !strict_ansi_mode && is_floating_type(dest_type)) {
      /* Destination is floating.  Source should be arithmetic or enum.
         Allowed as an extension. */
      if (is_arithmetic_or_enum_type(source_type) ||
          is_template_param_type(source_type)) {
        /* Okay. */
      } else {
        /* Cast from non-arithmetic to floating in a nontype template
           argument. */
        if (!is_error_type(source_type)) {
          pos_error(ec_non_arith_operation_in_templ_arg, &operand->position);
        }  /* if */
        err = TRUE;
      }  /* if */
    } else if ((is_pointer_type(dest_type) ||
                is_ptr_to_member_type(dest_type)) &&
               is_constant_operand(operand) &&
               is_or_might_be_null_pointer_constant(
                                                 &operand->variant.constant)) {
      /* Cast of a null pointer constant to a pointer or pointer-to-member
         type.  Allowed as an extension. */
    } else if (microsoft_mode &&
               is_pointer_type(dest_type) && is_pointer_type(operand->type) &&
               f_same_entities(type_pointed_to(dest_type),
                            f_skip_typerefs(type_pointed_to(operand->type)))) {
      /* A cast that strips qualifiers from a pointer type.  Allow as an
         extension in Microsoft mode. */
    } else if (is_template_param_type(dest_type)) {
      /* Casting to an unknown template parameter type is okay. */
    } else {
      /* Cast to a non-integral type in a nontype template argument. */
      if (!is_error_type(dest_type)) {
        diagnose_bad_template_arg_operation(type_position);
      }  /* if */
      err = TRUE;
    }  /* if */
  }  /* if */
  return !err;
}  /* cast_is_valid_in_current_expression_kind */


static void check_user_defined_conversions_for_cast(
                                           a_type_ptr type_cast_to,
                                           an_operand *operand,
                                           a_boolean  *allow_rvalue_on_rewrite,
                                           a_boolean  *processed,
                                           a_boolean  *err)
/*
The expression indicated by *operand is being cast to the type type_cast_to.
This is a static_cast or old-style cast.  If the cast can be done by
a user-defined conversion, do it and return *processed TRUE.  If the
cast could only be done by a user-defined conversion and there was some
error with that, set *err TRUE as well.  If the cast (to a reference type)
is expected to be rewritten later and an rvalue should be allowed,
*allow_rvalue_on_rewrite is returned TRUE (note that this is set even
for non-class operands).  This routine is called only in C++ mode.
*/
{
  a_boolean    cast_to_reference, failed;
  a_conv_descr conversion, ctor_arg_conversion;

  *processed = FALSE;
  *allow_rvalue_on_rewrite = FALSE;
  cast_to_reference = is_reference_type(type_cast_to);
  /* Don't check for user-defined conversions in constant expressions. */
  if (!curr_expr_kind_is_const()) {
    if (cast_to_reference) {
      a_boolean    ref_to_const, ref_to_const_volatile;
      a_boolean    binding_to_rvalue_allowed, dropping_qualifiers;
      a_symbol_ptr function_symbol;
      if (direct_reference_binding_possible(operand,
                                            operand->type,
                                            type_cast_to,
                                            &ref_to_const,
                                            &ref_to_const_volatile,
                                            &binding_to_rvalue_allowed,
                                            &dropping_qualifiers,
                                            &function_symbol)) {
        /* The operand can be cast directly to the reference type,
           so don't look for a way to do the cast using a conversion
           function.  Note that even non-class operands are handled here. */
        *allow_rvalue_on_rewrite = binding_to_rvalue_allowed;
      } else {
        /* A cast from a class to a reference type can be handled by a
           conversion function that returns a reference.  Look for such
           a function, but if one is not found, go on to the general case
           of casting to a reference (below).  This is different than
           other user-defined conversion cases, where if there is a
           class operand and no user-defined conversion applies,
           we know we have an error.  That's the reason that
           user_defined_conversion_possible is not called. */
        a_type_ptr eff_type_cast_to = type_pointed_to(type_cast_to);
        if (is_class_struct_union_type(operand->type)) {
          a_boolean  ambiguous;
          if (conversion_from_class_possible(
                                           operand, eff_type_cast_to,
                                           (a_builtin_type_kind_set)BTK_NONE,
                                           /*need_lvalue_result=*/TRUE,
                                           /*is_copy_initialization=*/FALSE,
                                           /*is_reference_binding=*/TRUE,
                                           &conversion, &ambiguous,
                                           (a_candidate_function_ptr *)NULL)) {
            /* A user-defined conversion can be done. */
            user_convert_operand(operand, eff_type_cast_to, &conversion,
                                 (a_conv_descr *)NULL,
                                 /*force_temp_for_class_bitwise_copy=*/FALSE,
                                 /*is_explicit_cast=*/TRUE);
            *processed = TRUE;
          } else if (ambiguous) {
            /* The conversion is ambiguous.  Do the analysis again to get
               the error message. */
            *err = TRUE;
            *processed = TRUE;
            (void)user_defined_conversion_possible(
                                            operand, eff_type_cast_to,
                                            /*need_lvalue_result=*/TRUE,
                                            /*is_copy_initialization=*/FALSE,
                                            /*is_reference_binding=*/TRUE,
                                            /*processed_arg=*/FALSE,
                                            &conversion,
                                            (a_conv_descr *)NULL,
                                            &failed);
          }  /* if */
        } else if (is_template_param_type(operand->type)) {
          /* A template parameter type could be a class type, so assume that
             a conversion is possible. */
          *processed = TRUE;
          generic_cast_operand(operand, make_pointer_type(eff_type_cast_to),
                               (an_expr_operator_kind)eok_cast,
                               /*is_implicit_cast=*/FALSE,
                               /*is_reference_cast=*/TRUE);
        }  /* if */
      }  /* if */
    } else {
      /* Normal case (not a cast to a reference type). */
      /* Check for user-defined conversions, but not when casting to void. */
      if (!is_void_type(type_cast_to)) {
        if (user_defined_conversion_possible(operand, type_cast_to,
                                             /*need_lvalue_result=*/FALSE,
                                             /*is_copy_initialization=*/FALSE,
                                             /*is_reference_binding=*/FALSE,
                                             /*processed_arg=*/FALSE,
                                             &conversion,
                                             &ctor_arg_conversion,
                                             &failed)) {
          /* A user-defined conversion can be done. */
          /* Force the result to an rvalue because the cast is not
             to a reference type (otherwise, when a conversion function
             that returns a reference is used, the result would be an
             lvalue). */
          conversion.result_is_an_lvalue = FALSE;
          /* Except in cfront mode, force a temporary for a cast of a class
             object to the same class type, ignoring cv-qualifiers. */
          user_convert_operand(operand, type_cast_to, &conversion,
                               &ctor_arg_conversion,
                               /*force_temp_for_class_bitwise_copy=*/
                                                           !any_cfront_mode(),
                               /*is_explicit_cast=*/TRUE);
          *processed = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_bugs && is_class_struct_union_type(type_cast_to)) {
            /* In Microsoft C++ mode, a function that returns a class type is
               considered to return an lvalue. */
            conv_class_operand_to_object_pointer(operand);
            conv_object_pointer_to_lvalue(operand);
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (failed) {
          /* A user-defined conversion was our only hope, and it failed.
             The error has already been issued. */
          *err = TRUE;
          *processed = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_user_defined_conversions_for_cast */


static void rewrite_cast_to_reference_as_pointer_cast(
                                      a_type_ptr            *type_cast_to,
                                      an_operand            *operand,
                                      a_boolean             allow_rvalue,
                                      an_expr_operator_kind cast_op,
                                      a_boolean             *processed)
/*
Rewrite a cast to a reference as the equivalent cast to a pointer type.
From [expr.reinterpret.cast]:

  An lvalue expression of type T1 can be cast to the type "reference  to
  T2"  if  an  expression of type "pointer to T1" can be explicitly
  converted to the type "pointer to T2" using a reinterpret_cast.  That is,
  a  reference  cast  reinterpret_cast<T&>(x) has the same effect as the
  conversion *reinterpret_cast<T*>(&x) with the built-in & and *  operators.
  The  result is an lvalue that refers to the same object as the
  source lvalue, but with a different type.  No temporary is created, no
  copy  is made, and constructors (_class.ctor_) or conversion functions
  (_class.conv_) are not called.

[expr.static.cast] allows a similar conversion, but it's hidden in the words

  An expression e can be explicitly converted to a type T using a
  static_cast of the form static_cast<T>(e) if the declaration "T t(e);"
  is well-formed, for some invented temporary variable t (_dcl.init_).

*operand is the expression being cast, and *type_cast_to is the reference type.
On return, *type_cast_to has been changed to the corresponding pointer
type, and operand is a pointer rvalue.  allow_rvalue is TRUE if an rvalue
should be allowed (e.g., for a static_cast to a reference-to-const type).
Return *processed TRUE if the cast has been processed internally in this
routine; this happens for casts involving unknown template types, in which
case a generic cast using the operator cast_op is generated.
*/
{
  a_type_ptr orig_type_cast_to = *type_cast_to;

  *processed = FALSE;
  *type_cast_to = make_pointer_type(type_pointed_to(*type_cast_to));
  if (is_template_dependent_context() &&
      (is_template_dependent_type(*type_cast_to) ||
       is_template_dependent_type(operand->type))) {
    /* A template-dependent operation in a prototype instantiation. */
    generic_cast_operand(operand, *type_cast_to,
                         cast_op,
                         /*is_implicit_cast=*/FALSE,
                         /*is_reference_cast=*/TRUE);
    *processed = TRUE;
  } else if (is_an_lvalue(operand)) {
    take_address_of_lvalue(operand);
  } else if (is_a_function_designator(operand)) {
    conv_function_designator_to_ptr_to_function(operand,
                                                /*allow_ctor=*/FALSE);
  } else if ((allow_rvalue || any_cfront_mode() || sun_mode ||
              allow_nonconst_ref_anachronism) &&
             is_class_struct_union_type(operand->type)) {
    /* Allow a cast of a class rvalue to a reference type, when appropriate
       (e.g., for a static_cast to a reference-to-const type). */
    conv_class_operand_to_object_pointer(operand);
  } else if (allow_rvalue) {
    /* Allow a cast of a non-class rvalue when appropriate (e.g., for
       a static_cast to a reference-to-const type).  This requires a
       temporary. */
    a_dynamic_init_ptr dip;
    an_expr_node_ptr   temp_init_node;
    a_type_ptr         temp_type = type_pointed_to(orig_type_cast_to);

    temp_init_node = create_expr_temporary(temp_type,
                                           /*result_is_addr=*/TRUE,
                                           /*is_explicit_cast=*/TRUE,
                                           /*suppress_abstract_test=*/FALSE,
                                           (a_dynamic_init_kind)dik_expression,
                                           &operand->position,
                                           &dip);
    dip->variant.expression = make_node_from_operand(operand);
    make_expression_operand(temp_init_node, temp_init_node->type, operand);
  } else {
    if (!is_error_operand(operand)) {
      error_in_operand(ec_expr_not_an_lvalue, operand);
    }  /* if */
  }  /* if */
}  /* rewrite_cast_to_reference_as_pointer_cast */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean conv_bound_function_to_pointer_to_member(
                                           an_operand *operand,
                                           an_operand *bound_function_selector)
/*
Convert a bound function that is not being called (normally an error) to
a pointer-to-member.  This is an extension in Microsoft mode.  Returns
FALSE if the bound function case is not one that undergoes the conversion.
*/
{
  a_boolean converted = FALSE;

  check_assertion(operand->bound_function && bound_function_selector != NULL);
  /* Do not convert cases that result from the operators .* and ->*.
     It's not that there would be anything wrong with that; we just don't
     have a way of representing that in the IL. */
  if (is_constant_operand(operand)) {
    a_constant_ptr con;
    a_symbol_ptr   sym;
    an_operand     orig_operand;

    orig_operand = *operand;
    pos_warning(ec_bound_function_must_be_called, &operand->position);
    /* Find the function underlying the operand. */
    con = &operand->variant.constant;
    check_assertion(con->kind == (a_constant_repr_kind)ck_address &&
                    con->variant.address.kind ==
                                           (an_address_base_kind)abk_routine &&
                    !con->implicit_cast);
    sym = (a_symbol_ptr)
             (con->variant.address.variant.routine->source_corresp.assoc_info);
    check_assertion(sym != NULL);
    /* Make a pointer-to-member for the function. */
    make_ptr_to_member_constant_operand(sym,
                                        sym,
                                        &orig_operand.position,
                                        /*check_protected_access=*/FALSE,
                                        /*is_qualified_name=*/FALSE,
                                        /*is_operand_of_address_of=*/FALSE,
                                        operand);
    restore_operand_details(operand, &orig_operand);
    operand->bound_function = FALSE;
    operand->position = bound_function_selector->position;
    discard_operand(bound_function_selector);
    converted = TRUE;
  }  /* if */
  return converted;
}  /* conv_bound_function_to_pointer_to_member */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void bound_function_in_cast(a_type_ptr        type_cast_to,
                                   a_source_position *start_position,
                                   an_operand        *operand,
                                   an_operand        *bound_function_selector)
/*
The indicated operand is a bound function that is the operand of a cast.
bound_function_selector is the associated object.  type_cast_to is the type
being cast to.  Do any transformations that convert the operand to something
other than a bound function in this cast context (these are extensions).
If no such transformation applies, issue an error and change the operand
to an error operand.  In all cases the operand as returned will no longer
be a bound function.  *start_position gives the starting position of
the cast.  Note that the cast is not actually done; the operand is
merely transformed to something to which the cast may apply.
*/
{
  if (allow_anachronisms &&
      is_pointer_type(type_cast_to) &&
      is_function_type(type_pointed_to(type_cast_to)) &&
      is_pointer_type(operand->type) &&
      is_function_type(type_pointed_to(operand->type))) {
    /* In C++, a bound function may be cast to a normal function pointer
       as an anachronism, as in

         struct A {int f();};
         A *p = new A;
         int (*pf)() = (int (*)())p->f;

       See ARM 18.3.4. */
    pos_diagnostic(anachronism_error_severity,
                   ec_bound_function_cast_anachronism, start_position);
    conv_lvalue_to_rvalue(operand);
    if (operand->virtual_function) {
      an_expr_node_ptr func_ptr_node, object_node;
      /* The function is a virtual function, so use an
         eok_virtual_function_ptr operation to compute the address at
         runtime. */
      /* Make a node for the function pointer. */
      func_ptr_node = make_node_from_operand(operand);
      /* Make a node for the bound object address. */
      object_node = make_node_from_operand(bound_function_selector);
      func_ptr_node->next = object_node;
      func_ptr_node = make_operator_node(
                               (an_expr_operator_kind)eok_virtual_function_ptr,
                               operand->type, func_ptr_node);
      make_expression_operand(func_ptr_node, func_ptr_node->type, operand);
    } else {
      /* The function is not a virtual function, so discard the
         selector object pointer and just use the routine address. */
      discard_operand(bound_function_selector);
      operand->bound_function = FALSE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode &&
             is_ptr_to_member_type(type_cast_to) &&
             is_function_type(pm_member_type(type_cast_to)) &&
             is_pointer_type(operand->type) &&
             is_function_type(type_pointed_to(operand->type))) {
    /* The Microsoft compiler allows a bound function to be cast to
       a pointer to member function. */
    if (!conv_bound_function_to_pointer_to_member(operand,
                                                  bound_function_selector)) {
      error_in_operand(ec_bound_function_must_be_called, operand);
      operand->bound_function = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Any other use of a bound function.  Error. */
    error_in_operand(ec_bound_function_must_be_called, operand);
    operand->bound_function = FALSE;
  }  /* if */
}  /* bound_function_in_cast */


static void cast_overloaded_function(a_type_ptr type_cast_to,
                                     an_operand *operand)
/*
Cast an operand for an overloaded function (*operand) to type_cast_to.
If type_cast_to is a pointer or pointer-to-member type, the cast can serve
to select one of the functions in the overload set.  See [over.over].
*/
{
  an_arg_match_level match_level;
  a_std_conv_descr   std_conversion;
  a_boolean          ambiguous, unknown_dependent_function;

  if (find_addr_of_overloaded_function_match(operand->variant.symbol,
                                             (a_boolean)operand->
                                                      is_template_id,
                                             operand->template_arg_list,
                                             type_cast_to,
                                             /*is_cast=*/TRUE,
                                             &match_level,
                                             &std_conversion,
                                             &unknown_dependent_function,
                                             &ambiguous) != NULL) {
    /* The cast selects one of the overloaded functions and is valid. */
    cast_operand(type_cast_to, operand, /*check_cast_access=*/FALSE,
                 /*is_implicit_cast=*/FALSE, /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
  } else if (unknown_dependent_function) {
    /* The cast occurs in a prototype instantiation and it is not possible
       to determine which function to use. */
    make_unknown_dependent_function_operand(operand->variant.symbol,
                                            (a_boolean)operand->is_template_id,
                                            operand->template_arg_list,
                                            (a_boolean)operand->
                                                             is_qualified_name,
                                            operand);
  } else {
    /* The cast doesn't select one of the overloaded functions, so it's
       an error. */
    pos_sy_error(ec_indeterminate_overloaded_function,
                 &operand->position, operand->variant.symbol);
    conv_to_error_operand(operand);
  }  /* if */
}  /* cast_overloaded_function */


static void microsoft_lvalue_cv_qual_adjustment(an_operand *operand,
                                                a_type_ptr new_type)
/*
operand is being subjected to an lvalue cast to new_type in Microsoft
mode.  The cast can adjust only the cv-qualification of the lvalue;
the underlying type is the same.  If necessary, adjust the cv-qualification.
Note that this will get an error if the operand is a bit field reference.
The caller should check for that and avoid it.
*/
{
  if (!identical_types(operand->type, new_type)) {
    take_address_of_lvalue(operand);
    cast_operand(make_pointer_type(new_type),
                 operand, /*check_cast_access=*/FALSE,
                 /*is_implicit_cast=*/FALSE, 
                 /*is_reinterpret_cast=*/FALSE,
                 /*reinterpret_semantics=*/FALSE);
    conv_object_pointer_to_lvalue(operand);
  }  /* if */
}  /* microsoft_lvalue_cv_qual_adjustment */


static void do_cast(a_type_ptr               type_cast_to,
                    an_operand               *operand,
                    an_operand               *bound_function_selector,
                    a_local_expr_options_set local_options,
                    a_boolean                err,
                    a_source_position        *type_position,
                    a_source_position        *start_position)
/*
Do a cast operation.  The operand *operand is to be cast to the type
type_cast_to.  If it is a bound function (only in C++),
*bound_function_selector gives the associated object, if any.
err is TRUE if it has already been determined that the cast is invalid
(this routine does additional checking).  type_position is the source
position of the destination type in the cast.  start_position is the
source position of the start of the cast.  This routine is called for
C-style casts and C++ functional-notation type conversions.
*/
{
  a_type_ptr    source_type, orig_type_cast_to = type_cast_to;
  an_error_code warning_suggested;
  a_boolean     cast_to_void, cast_to_reference = FALSE, processed = FALSE;
  a_boolean     allow_rvalue_on_rewrite = FALSE;

  if (err) {
    /* There was a previous error (e.g., the type to cast to is invalid
       regardless of the type of the source).  Do no further checking. */
  } else {
    /* Check for user-defined conversions and casts to reference type,
       but not in C. */
    cast_to_void = is_void_type(type_cast_to);
    if (!C_mode()) {
      /* See if we're casting to a reference type. */
      cast_to_reference = is_reference_type(type_cast_to);
      check_user_defined_conversions_for_cast(type_cast_to, operand,
                                              &allow_rvalue_on_rewrite,
                                              &processed, &err);
    }  /* if */
    if (!processed) {
      /* No user-defined conversion applies. */
      if (!cast_to_reference && !cast_to_void) {
        /* Normal case (not a cast to reference or cast to void). */
        /* Do array --> pointer and function --> pointer conversions.
           They must be done now because they affect the type of the operand.
           Don't do lvalue --> rvalue yet because of the lvalue cast case. */
        /* Keep indefinite functions too, since a particular function can
           be chosen by a cast to a pointer or pointer-to-member type. */
        do_operand_transformations(operand,
                                   TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                                  TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
      }  /* if */
      /* Check for casts that aren't valid in this kind of expression.
         Note that this check is done after the operand transformations
         (e.g., turning arrays into pointers), but before the reference
         rewriting or anything else that changes type_cast_to. */
      if (!cast_is_valid_in_current_expression_kind(operand, type_cast_to,
                                                    local_options,
                                                    type_position)) {
        /* This cast is not valid in this kind of expression. */
        err = TRUE;
      }  /* if */
      /* Do any rewriting that changes the destination type. */
      if (err) {
        /* Some previous error. */
      } else if (cast_to_reference) {
        /* Rewrite a cast to a reference type as a cast to a pointer type.
           Note that the original type_cast_to is preserved in
           orig_type_cast_to. */
        /* Note that this is done after the check for user-defined
           conversions above, since if such a cast can be done by
           a conversion function, it should be. */
        rewrite_cast_to_reference_as_pointer_cast(
                                               &type_cast_to, operand,
                                               allow_rvalue_on_rewrite,
                                               (an_expr_operator_kind)eok_cast,
                                               &processed);
      }  /* if */
      /* Check for different types of casts and do the cast. */
      if (!err && !processed) {
        a_boolean      reinterpret_semantics = FALSE;
        a_boolean      operand_is_constant;
        a_constant_ptr operand_con = NULL;
        /* The bound function test is done first to make sure bound functions
           cannot wander into the rest of the cases. */
        if (operand->bound_function) {
          bound_function_in_cast(type_cast_to, start_position, operand,
                                 bound_function_selector);
        }  /* if */
        /* Get the source type after the transformations. */
        source_type = operand->type;
        operand_is_constant = is_constant_operand(operand);
        if (operand_is_constant) operand_con = &operand->variant.constant;
        if (is_indefinite_function_operand(operand)) {
          /* An overloaded function can be cast to a pointer type that
             disambiguates, but is not valid in any other kind of cast. */
          cast_overloaded_function(type_cast_to, operand);
          if (cast_to_reference) {
            /* The result of a cast to reference is an lvalue. */
            conv_object_pointer_to_lvalue(operand);
          }  /* if */
        } else if (any_cfront_mode() && operand_is_constant &&
                   operand_con->kind ==
                                      (a_constant_repr_kind)ck_ptr_to_member &&
                   !operand_con->implicit_cast &&
                   operand_con->variant.ptr_to_member.is_function_ptr &&
                   !cast_to_reference &&
                   is_pointer_type(type_cast_to) &&
                   is_function_type(type_pointed_to(type_cast_to))) {
          /* In cfront mode, it's okay to cast a pointer-to-member constant
             to a pointer to function:
               struct A {int f();};
               main () {
                 int (*p)() = (int (*)())A::f;
               }
          */
          a_routine_ptr routine = operand_con->variant.ptr_to_member.
                                                               variant.routine;
          a_symbol_ptr  rout_sym =
                            (a_symbol_ptr)(routine->source_corresp.assoc_info);
          pos_warning(ec_ptr_to_member_cast_to_ptr_to_function,
                      start_position);
          make_function_designator_operand(rout_sym,
                                         (a_boolean)operand->is_qualified_name,
                                           start_position,
                                           (a_ref_entry_ptr)NULL,
                                           operand);
          conv_function_designator_to_ptr_to_function(operand,
                                                      /*allow_ctor=*/FALSE);
          cast_operand(type_cast_to, operand, /*check_cast_access=*/FALSE,
                       /*is_implicit_cast=*/FALSE, 
                       /*is_reinterpret_cast=*/FALSE,
                       reinterpret_semantics);
        } else if (cast_to_void) {
          /* Cast to (possibly cv-qualified) void. */
          cast_operand_to_void(operand, type_cast_to);
        } else if (expl_conversion_possible(source_type, operand_is_constant,
                                            (a_boolean)operand->
                                                      is_simple_string_literal,
                                            operand_con, type_cast_to,
                                            &reinterpret_semantics,
                                            ec_bad_cast, &warning_suggested)) {
          /* Valid explicit conversion. */
          if (microsoft_bugs && is_an_lvalue(operand) &&
              f_identical_types(f_skip_typerefs(source_type),
                                f_skip_typerefs(type_cast_to),
                                ITF_NO_FLAGS) &&
              value_of_constant_var_lvalue_operand(operand) == NULL &&
              !is_bit_field_operand(operand)) {
            /* In Microsoft mode, a cast of an lvalue to the same type
               is just ignored, and the operand stays an lvalue.  Note that
               this applies in C++ as well as C. */
            /* The cast can add or drop cv-qualifiers.  If it does, we
               have to add a cast. */
            microsoft_lvalue_cv_qual_adjustment(operand, type_cast_to);
          } else if ((C_dialect == C_dialect_pcc || SVR4_C_mode || gcc_mode ||
                      (microsoft_mode && C_mode())) &&
                     is_an_lvalue(operand) &&
                     still_an_lvalue(source_type, type_cast_to)) {
            /* In pcc, SVR4 C, GNU C or Microsoft C mode, some lvalues cast to
               other types remain lvalues (e.g., int to unsigned). */
            /* Use a special "lvalue cast" operator.  Always do the cast on
               an expression node, even if the lvalue address is currently
               given by a constant.  This is because all lvalue casts should
               be clearly identifiable.  The lvalue cast operator looks a lot
               like a normal cast, but its operand is an lvalue, and therefore
               doesn't really have its address taken, which is important when
               (e.g.) register entities are subjected to an lvalue cast.  See
               the code in conv_lvalue_to_rvalue that removes the cast if
               the cast lvalue is then converted to an rvalue (the usual
               case). */
            lvalue_cast(type_cast_to, operand);
          } else {
            /* Not an lvalue cast.  Issue warning on oddball cases. */
            if (warning_suggested != ec_no_error) {
              pos_warning(warning_suggested, start_position);
            }  /* if */
            /* Convert lvalue --> rvalue unless casting to a reference type
               (in that case, the operand has already been turned into a
               pointer; the conversion here wouldn't hurt, but it's not
               needed). */
            if (!cast_to_reference) {
              /* Normal cast.  All standard C cases. */
              conv_lvalue_to_rvalue(operand);
            }  /* if */
            /* Do the actual cast. */
            cast_operand(type_cast_to, operand, /*check_cast_access=*/FALSE,
                         /*is_implicit_cast=*/FALSE, 
                         /*is_reinterpret_cast=*/FALSE,
                         reinterpret_semantics);
            if (cast_to_reference) {
              /* The result of a cast to reference is an lvalue. */
              conv_object_pointer_to_lvalue(operand);
            }  /* if */
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
        } else if (gcc_mode &&
                   is_class_struct_union_type(type_cast_to) &&
                   f_identical_types(f_skip_typerefs(source_type),
                                     f_skip_typerefs(type_cast_to),
                                     ITF_NO_FLAGS)) {
          /* GNU C allows a do-nothing cast to a struct or union type.
             The result does not change type (even if there is a cv-qualifier
             difference implied) and it is not forced to an rvalue. */
        } else if (gcc_mode && is_union_type(type_cast_to)) {
          /* It may be possible to convert *operand to the type of one of
             the members of the union.  If so, the conversion is allowed.  */
          a_field_ptr field =
                  transparent_union_conversion_possible(operand, type_cast_to);
          if (field != NULL) {
            /* Convert from the type of the field to the type of the
               union, using a dynamic initializer generated on the fly. */
            prep_transparent_union_conversion_operand(type_cast_to, field,
                                                      operand);
          } else {
            err = TRUE;
            pos_ty_error(ec_cast_to_bad_type, type_position,
                         orig_type_cast_to);
          }
#endif /* GNU_EXTENSIONS_ALLOWED */
        } else {
          /* Not a valid cast. */
          err = TRUE;
          if (is_class_struct_union_type(orig_type_cast_to)) {
            /* Use a special clearer message for casting to a class. */
            pos_ty_error(ec_cast_to_bad_type, type_position,
                         orig_type_cast_to);
          } else {
            /* Generic message. */
            /* Note: If this is changed to display the types involved,
               use orig_type_cast_to (because of the reference rewrite). */
            pos_error(ec_bad_cast, start_position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) conv_to_error_operand(operand);
  operand->position = *start_position;
}  /* do_cast */


static void scan_const_cast_operator(an_operand *result)
/*
Scan the C++ const_cast operator.  See [expr.const.cast].

Syntax:
	const_cast < type-id > ( expression )

*/
{
  a_source_position start_position, type_position, end_position;
  an_operand        operand;
  a_type_ptr        cast_type, underlying_cast_type, operand_type;
  a_type_ptr        operation_type;
  a_boolean         cast_type_okay, template_param_case = FALSE;
  a_boolean         reference_case = FALSE, err = FALSE;

  db_enter(4, "scan_const_cast_operator");
  /* Save the position of the const_cast keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* const_cast not possible for preprocessing expressions. */
    internal_error("scan_const_cast_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* New-style casts are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_new_cast_in_embedded_cplusplus);
  /* Advance past const_cast. */
  (void)get_token();
  /* Scan "< type-id > ( expression )". */
  if (!scan_new_style_cast(&cast_type, &type_position, &end_position,
                           &operand)) {
    err = TRUE;
  }  /* if */
  /* Except when casting to a reference type, do operand transformations
     on the source operand. */
  reference_case = is_reference_type(cast_type);
  if (!reference_case) {
    do_operand_transformations(&operand, TOPT_NO_OPTIONS);
  }  /* if */
  /* Check for casts that aren't valid in this kind of expression.
     Note that this check is done after the operand transformations
     (e.g., turning arrays into pointers), but before the reference
     rewriting or anything else that changes cast_type. */
  if (!cast_is_valid_in_current_expression_kind(&operand, cast_type,
                                                EOPT_NO_OPTIONS,
                                                &type_position)) {
    /* This cast is not valid in this kind of expression. */
    err = TRUE;
  }  /* if */
  if (!err) {
    /* The type cast to must be a pointer or reference to an object type, or
       a pointer to data member. */
    cast_type_okay = FALSE;
    if (is_ptr_or_ref_type(cast_type)) {
      underlying_cast_type = type_pointed_to(cast_type);
      if (!is_function_type(underlying_cast_type)) {
        /* Casting to a pointer or reference to an object type. */
        cast_type_okay = TRUE;
      }  /* if */
    } else if (is_ptr_to_member_type(cast_type)) {
      underlying_cast_type = pm_member_type(cast_type);
      if (!is_function_type(underlying_cast_type)) {
        /* Casting to a pointer to member of an object type. */
        cast_type_okay = TRUE;
      }  /* if */
    } else if (is_template_param_type(cast_type)) {
      /* A cast to a template parameter type is assumed to be okay. */
      template_param_case = TRUE;
      cast_type_okay = TRUE;
      underlying_cast_type = type_of_unknown_templ_param_nontype;
    } else {
      /* cast_type is not a pointer, reference, or pointer to member type;
         error. */
      cast_type_okay = FALSE;
    }  /* if */
    if (!cast_type_okay) {
      /* Bad const_cast type. */
      err = TRUE;
      if (!is_error_type(cast_type)) {
        pos_error(ec_bad_const_cast_type, &type_position);
      }  /* if */
    } else {
      /* The type cast to is okay. */
      if (is_template_dependent_context() && !template_param_case &&
          is_template_dependent_type(underlying_cast_type)) {
        /* Casting to an unknown type in a prototype instantiation. */
        template_param_case = TRUE;
      }  /* if */
      /* The operation type for the cast is the type specified, except that
         for a cast to a reference type it is the corresponding pointer
         type. */
      operation_type = cast_type;
      if (reference_case) {
        operation_type = make_pointer_type(underlying_cast_type);
      }  /* if */
      operand_type = operand.type;
      if (is_template_dependent_context() &&
          is_template_dependent_type(operand_type)) {
        /* An operand of unknown type in a prototype instantiation. */
        template_param_case = TRUE;
      } else if (reference_case) {
        /* Cast to reference type. */
        /* The source operand must be an lvalue. */
        if (!is_an_lvalue(&operand)) {
          err = TRUE;
          if (!is_error_operand(&operand)) {
            pos_error(ec_expr_not_an_lvalue, &operand.position);
          }  /* if */
        } else {
          /* Turn the lvalue into an address so we can deal with it as a
             pointer. */
          take_address_of_lvalue(&operand);
          operand_type = operand.type;
        }  /* if */
      }  /* if */
      if (!err && !template_param_case) {
        /* Check that the cast just changes qualifiers (or makes no change). */
        /* Note that this comparison considers error types equal to any
           other types. */
        if (!same_type_with_added_qualifiers(operand_type, operation_type,
                                             /*ignore_qualifiers=*/TRUE,
                                             (a_boolean *)NULL)) {
          err = TRUE;
          pos_error(ec_bad_const_cast, &operand.position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    /* Some error, previously issued. */
    make_error_operand(result);
  } else {
    if (template_param_case) {
      /* Put out a generic operator for a case involving template parameter
         types. */
      generic_cast_operand(&operand, operation_type, 
                           (an_expr_operator_kind)eok_const_cast,
                           /*is_implicit_cast=*/FALSE,
                           /*is_reference_cast=*/FALSE);
      copy_operand(&operand, result);
    } else {
      /* The types are already the same except for qualifiers.  The result
         is just the source cast to the destination type. */
      /* Note that the cast has been turned into pointer form if it was a
         reference cast. */
      cast_operand(operation_type, &operand, /*check_cast_access=*/FALSE,
                   /*is_implicit_cast=*/FALSE, /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
      copy_operand(&operand, result);
    }  /* if */
    /* For a cast to a reference type, the result is an lvalue. */
    if (reference_case) {
      conv_object_pointer_to_lvalue(result);
    }  /* if */
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_const_cast_operator */


static void scan_static_cast_operator(an_operand *result)
/*
Scan the C++ static_cast operator.  See [expr.static.cast].

Syntax:
	static_cast < type-id > ( expression )

*/
{
  a_source_position start_position, type_position, end_position;
  a_type_ptr        type_cast_to, orig_type_cast_to, source_type;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         allow_rvalue_on_rewrite = FALSE;
  an_error_code     warning_suggested;

  db_enter(4, "scan_static_cast_operator");
  /* Save the position of the static_cast keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* static_cast not possible for preprocessing expressions. */
    internal_error("scan_static_cast_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* New-style casts are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_new_cast_in_embedded_cplusplus);
  /* Advance past static_cast. */
  (void)get_token();
  /* Scan "< type-id > ( expression )". */
  if (!scan_new_style_cast(&type_cast_to, &type_position, &end_position,
                           result)) {
    err = TRUE;
  } else {
    a_boolean cast_to_reference = is_reference_type(type_cast_to);
    a_boolean cast_to_void      = is_void_type(type_cast_to);

    orig_type_cast_to = type_cast_to;
    /* Check for user-defined conversions and casts to reference type. */
    check_user_defined_conversions_for_cast(type_cast_to, result,
                                            &allow_rvalue_on_rewrite,
                                            &processed, &err);
    if (!processed) {
      /* No user-defined conversion applies. */
      if (!cast_to_reference && !cast_to_void) {
        /* Normal case (not a cast to reference or cast to void). */
        /* Do lvalue --> rvalue, array --> pointer, and function --> pointer
           conversions.  They must be done now because they affect the type
           of the operand. */
        /* Keep indefinite functions, since a particular function can
           be chosen by a cast to a pointer or pointer-to-member type. */
        do_operand_transformations(result,
                                  TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
      }  /* if */
      /* Check for casts that aren't valid in this kind of expression.
         Note that this check is done after the operand transformations
         (e.g., turning arrays into pointers), but before the reference
         rewriting or anything else that changes type_cast_to. */
      if (!cast_is_valid_in_current_expression_kind(result, type_cast_to,
                                                    EOPT_NO_OPTIONS,
                                                    &type_position)) {
        /* This cast is not valid in this kind of expression. */
        err = TRUE;
      }  /* if */
      /* Do any rewriting that changes the destination type. */
      if (err) {
        /* Some previous error. */
      } else if (cast_to_reference) {
        /* Rewrite a cast to a reference type as a cast to a pointer type.
           Note that the original type_cast_to is preserved in
           orig_type_cast_to. */
        /* Note that this is done after the check for user-defined
           conversions above, since if such a cast can be done by
           a conversion function, it should be. */
        rewrite_cast_to_reference_as_pointer_cast(
                                        &type_cast_to, result,
                                        allow_rvalue_on_rewrite,
                                        (an_expr_operator_kind)eok_static_cast,
                                        &processed);
      }  /* if */
      /* Get the source type after the transformations. */
      source_type = result->type;
      /* Check for different types of casts and do the cast. */
      if (!err && !processed) {
        a_boolean      operand_is_constant = is_constant_operand(result);
        a_constant_ptr operand_con = NULL;
        if (operand_is_constant) operand_con = &result->variant.constant;
        if (is_indefinite_function_operand(result)) {
          /* An overloaded function may be cast to a pointer type that
             disambiguates, but is not valid in any other kind of cast. */
          cast_overloaded_function(type_cast_to, result);
        } else if (cast_to_void) {
          /* Cast to (possibly cv-qualified) void. */
          cast_operand_to_void(result, type_cast_to);
        } else if (static_cast_conversion_possible(source_type,
                                                   operand_is_constant,
                                                   (a_boolean)result->
                                                      is_simple_string_literal,
                                                   operand_con,
                                                   type_cast_to,
                                      /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                                   ec_bad_cast,
                                                   &warning_suggested)) {
          /* Valid static_cast conversion. */
          if (cast_removes_qualifiers(source_type, type_cast_to)) {
            /* This static_cast casts away constness, which is not allowed. */
            pos_st_error(ec_cannot_cast_away_const, &start_position,
                         "static_cast");
          }  /* if */
          if (warning_suggested != ec_no_error) {
            /* Issue warning on oddball cases. */
            pos_warning(warning_suggested, &start_position);
          }  /* if */
          if (is_template_dependent_context() &&
              (is_template_dependent_type(source_type) ||
               is_template_dependent_type(type_cast_to))) {
            /* Put out a generic operator for a case involving template
               parameter types. */
            generic_cast_operand(result, type_cast_to,
                                 (an_expr_operator_kind)eok_static_cast,
                                 /*is_implicit_cast=*/FALSE,
                                 /*is_reference_cast=*/FALSE);
          } else {
            /* Do the actual cast. */
            cast_operand(type_cast_to, result, /*check_cast_access=*/TRUE,
                         /*is_implicit_cast=*/FALSE,
                         /*is_reinterpret_cast=*/FALSE,
                         /*reinterpret_semantics=*/FALSE);
          }  /* if */
          if (cast_to_reference) {
            /* The result of a cast to reference is an lvalue. */
            conv_object_pointer_to_lvalue(result);
          }  /* if */
        } else {
          /* Not a valid cast. */
          err = TRUE;
          if (is_class_struct_union_type(orig_type_cast_to)) {
            /* Use a special clearer message for casting to a class. */
            pos_ty_error(ec_cast_to_bad_type, &type_position,
                         orig_type_cast_to);
          } else {
            /* Generic message. */
            /* Note: If this is changed to display the types involved,
               use orig_type_cast_to (because of the reference rewrite). */
            pos_error(ec_bad_cast, &start_position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) conv_to_error_operand(result);
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_static_cast_operator */


static void scan_reinterpret_cast_operator(an_operand *result)
/*
Scan the C++ reinterpret_cast operator.  See [expr.reinterpret.cast].

Syntax:
	reinterpret_cast < type-id > ( expression )

*/
{
  a_source_position start_position, type_position, end_position;
  a_type_ptr        type_cast_to, orig_type_cast_to, source_type;
  a_boolean         cast_to_reference = FALSE, err = FALSE;
  an_error_code     warning_suggested;
  a_boolean         processed = FALSE;

  db_enter(4, "scan_reinterpret_cast_operator");
  /* Save the position of the reinterpret_cast keyword. */
  start_position = pos_curr_token;
#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* reinterpret_cast not possible for preprocessing expressions. */
    internal_error("scan_reinterpret_cast_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* New-style casts are outside the "Embedded C++" subset. */
  feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_new_cast_in_embedded_cplusplus);
  /* Advance past reinterpret_cast. */
  (void)get_token();
  /* Scan "< type-id > ( expression )". */
  if (!scan_new_style_cast(&type_cast_to, &type_position, &end_position,
                           result)) {
    err = TRUE;
  } else {
    orig_type_cast_to = type_cast_to;
    /* Check for casts to reference type. */
    cast_to_reference = is_reference_type(type_cast_to);
    if (!cast_to_reference) {
      /* Normal case (not a cast to reference). */
      /* Do lvalue --> rvalue, array --> pointer, and function --> pointer
         conversions.  They must be done now because they affect the type
         of the operand.  Also give errors on overloaded functions. */
      do_operand_transformations(result, TOPT_NO_OPTIONS);
    }  /* if */
    /* Check for casts that aren't valid in this kind of expression.
       Note that this check is done after the operand transformations
       (e.g., turning arrays into pointers), but before the reference
       rewriting or anything else that changes type_cast_to. */
    if (!cast_is_valid_in_current_expression_kind(result, type_cast_to,
                                                  EOPT_NO_OPTIONS,
                                                  &type_position)) {
      /* This cast is not valid in this kind of expression. */
      err = TRUE;
    }  /* if */
    /* Do any rewriting that changes the destination type. */
    if (err) {
      /* Some previous error. */
    } else if (cast_to_reference) {
      /* Rewrite a cast to a reference type as a cast to a pointer type.
         Note that the original type_cast_to is preserved in
         orig_type_cast_to. */
      rewrite_cast_to_reference_as_pointer_cast(
                                   &type_cast_to, result,
                                   /*allow_rvalue_on_rewrite=*/FALSE,
                                   (an_expr_operator_kind)eok_reinterpret_cast,
                                   &processed);
    }  /* if */
    /* Get the source type after the transformations. */
    source_type = result->type;
    /* Check for different types of casts and do the cast. */
    if (!err && !processed) {
      if (reinterpret_cast_conversion_possible(source_type,
                                               type_cast_to,
                                               &warning_suggested)) {
        /* Valid reinterpret_cast conversion. */
        if (cast_removes_qualifiers(source_type, type_cast_to)) {
          /* This reinterpret_cast casts away constness, which is not
             allowed. */
          pos_st_error(ec_cannot_cast_away_const, &start_position,
                       "reinterpret_cast");
        }  /* if */
        if (warning_suggested != ec_no_error) {
          /* Issue warning on oddball cases. */
          pos_warning(warning_suggested, &start_position);
        }  /* if */
        if (is_template_dependent_context() &&
            (is_template_dependent_type(source_type) ||
             is_template_dependent_type(type_cast_to))) {
          /* Put out a generic operator for a case involving template parameter
             types. */
          generic_cast_operand(result, type_cast_to,
                               (an_expr_operator_kind)eok_reinterpret_cast,
                               /*is_implicit_cast=*/FALSE,
                               /*is_reference_cast=*/FALSE);
        } else {
          /* Do the actual cast. */
          cast_operand(type_cast_to, result, /*check_cast_access=*/TRUE,
                      /*is_implicit_cast=*/FALSE, /*is_reinterpret_cast=*/TRUE,
                       /*reinterpret_semantics=*/TRUE);
        }  /* if */
        if (cast_to_reference) {
          /* The result of a cast to reference is an lvalue. */
          conv_object_pointer_to_lvalue(result);
        }  /* if */
      } else {
        /* Not a valid cast. */
        err = TRUE;
        if (is_class_struct_union_type(orig_type_cast_to)) {
          /* Use a special clearer message for casting to a class. */
          pos_ty_error(ec_cast_to_bad_type, &type_position,
                       orig_type_cast_to);
        } else {
          /* Generic message. */
          /* Note: If this is changed to display the types involved,
             use orig_type_cast_to (because of the reference rewrite). */
          pos_error(ec_bad_cast, &start_position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) conv_to_error_operand(result);
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_reinterpret_cast_operator */


static void scan_cast_expression(a_type_ptr type_cast_to,
                                 a_boolean  allow_comma,
                                 int        prec_level,
                                 an_operand *operand,
                                 an_operand *bound_function_selector)
/*
Scan an expression that is the operand of a cast.  type_cast_to is the
type to which the expression will be cast.  allow_comma is TRUE if a
top-level comma should be allowed in the expression.  prec_level is
the precedence level for the expression scan.  Return the expression
in *operand, and if a bound function is scanned, return the selector
in *bound_function_selector.
*/
{
  /* In non-strict mode, scan the operand of a cast to an integral type
     in an integral constant or template argument expression specially to
     allow address expressions that reduce to integer values.  This is
     to provide better support for common variants of offsetof. */
  if (!strict_ansi_mode &&
      (curr_expr_kind_is(ek_integral_constant) ||
       curr_expr_kind_is(ek_template_arg)) &&
      is_integral_type(type_cast_to)) {
    scan_extended_integral_constant_expression(allow_comma, prec_level,
                                               operand);
  } else {
    /* Normal case. */
    a_local_expr_options_set cast_options = EOPT_OPERAND_OF_CAST;
    if (!C_mode()) {
      /* In C++, allow a bound function as the operand of a cast. */
      cast_options |= EOPT_ALLOW_BOUND_FUNCTION;
    }  /* if */
    if (!allow_comma) cast_options |= EOPT_DISALLOW_COMMA_OPERATOR;
    scan_expr_full(operand, bound_function_selector, prec_level, cast_options);
  }  /* if */
}  /* scan_cast_expression */


static void save_expr_stack(an_expr_stack_entry_ptr *saved_expr_stack)
/*
Clear the expression stack, returning the old expression stack pointer
to the caller in *saved_expr_stack, for later restoration by calling
restore_expr_stack.  This is used at the start of processing of an
expression that is not part of the surrounding context.
*/
{
  *saved_expr_stack = expr_stack;
  expr_stack = NULL;
}  /* save_expr_stack */


static void restore_expr_stack(an_expr_stack_entry_ptr saved_expr_stack)
/*
Restore the expression stack to the state it had when save_expr_stack
was called.
*/
{
  expr_stack = saved_expr_stack;
}  /* restore_expr_stack */

#if GNU_EXTENSIONS_ALLOWED

static void scan_gnu_statement_expression(an_operand *result)
/*
Scan the GNU C statement expression:

  ({ statement; statement; })

Return an operand for the expression in *result.
*/
{
  a_boolean         err = FALSE;
  a_statement_ptr   sp;
  a_source_position start_position;

  start_position = pos_curr_token;
  if (curr_expr_kind_is_const()) {
    /* Not allowed in a constant expression. */
    error(ec_expr_not_constant);
    err = TRUE;
  }  /* if */
  if (depth_stmt_stack < 0) {
    /* We're not inside a function, so don't try to scan the statement.
       Just flush to the matching closing brace. */
    if (!err) {
      error(ec_statement_expression_in_function_only);
      err = TRUE;
    }  /* if */
    flush_until_matching_token();
    /* Skip the closing brace. */
    check_assertion(curr_token == tok_rbrace);
    (void)get_token();
  } else {
    /* Save, clear, and later restore the expression stack, since the
       statements are not part of any expression we may currently be
       inside of. */
    an_expr_stack_entry_ptr saved_expr_stack;
    save_expr_stack(&saved_expr_stack);
    check_assertion(innermost_function_scope != NULL);
    /* Scan the compound statement. */
    sp = compound_statement(/*at_function_level=*/FALSE,
                            /*explicit_return_type=*/FALSE,
                            /*is_catch_clause=*/FALSE,
                            /*is_statement_expr=*/TRUE);
    restore_expr_stack(saved_expr_stack);
  }  /* if */
  if (err) {
    make_error_operand(result);
  } else {
    a_statement_ptr  stmt, last_stmt;
    a_type_ptr       expr_type;
    an_expr_node_ptr expr;
    check_assertion(sp->kind == (a_statement_kind)stmk_block);
    /* The value of the expression is the value of the last statement
       in the block if it's an expression statement.  Otherwise, the
       expression is a void expression. */
    last_stmt = NULL;
    for (stmt = sp->variant.block.statements;
         stmt != NULL;
         stmt = stmt->next) {
      /* Remember the last statement, but don't count vla-dealloc
         statements. */
      if (stmt->kind != (a_statement_kind)stmk_vla_dealloc) last_stmt = stmt;
    }  /* for */
    if (last_stmt != NULL &&
        last_stmt->kind == (a_statement_kind)stmk_expr &&
        /* Watch out for a final empty statement when
           REPRESENT_EMPTY_STATEMENTS_IN_IL is FALSE. */
        !last_stmt->expr->result_is_not_used) {
      expr_type = last_stmt->expr->type;
    } else {
      expr_type = void_type();
    }  /* if */
    expr = alloc_expr_node((an_expr_node_kind)enk_statement);
    expr->variant.statement = sp;
    expr->type = expr_type;
    make_expression_operand(expr, expr_type, result);
    current_routine_entry()->contains_statement_expression = TRUE;
  }  /* if */
  (void)required_token(tok_rparen, ec_exp_rparen);
  set_operand_position(result, &start_position, &pos_curr_token,
                       &start_position);
}  /* scan_gnu_statement_expression */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void scan_compound_literal(a_type_ptr              *p_literal_type,
                                  a_source_position        *type_position,
                                  an_operand               *result,
                                  a_local_expr_options_set local_options)
/*
Scan a compound literal.  See 6.5.2.5 in the C99 standard.  A compound
literal looks like a cast in which the source expression is a brace-
enclosed initializer, e.g.,

  (int []){1, 2, 3}

On entry, the current token is the "{", *p_literal_type indicates the type
of the compound literal, and *type_position is the position of that type.
On exit, the current token is the token after the "}", and *result is set
to the compound literal.
*/
{
  a_boolean               err = FALSE;
  a_type_ptr              literal_type = *p_literal_type;
  a_dynamic_init_ptr      dip;
  a_boolean               is_static = curr_expr_kind_is_const();
  an_expr_stack_entry_ptr saved_expr_stack;
  a_memory_region_number  region_to_switch_back_to;

  check_assertion(C_mode() &&
                  !curr_expr_kind_is(ek_pp) &&
                  !curr_expr_kind_is(ek_template_arg));
  if (curr_expr_kind_is(ek_integral_constant)) {
    /* A compound literal is not allowed in an integral constant expression. */
    pos_error(ec_bad_integral_compound_literal, type_position);
    err = TRUE;
  } else if (vla_enabled && is_vla_type(literal_type)) {
    /* Variable-length arrays are not allowed. */
    pos_error(ec_vla_not_allowed, type_position);
    err = TRUE;
  } else if (is_error_type(literal_type)) {
    /* An error occurred earlier.  Suppress certain processing now. */
    err = TRUE;
  } else if (is_object_type(literal_type)) {
    /* Object type, okay. */
  } else if (is_array_type(literal_type) &&
             is_object_type(array_element_type(literal_type))) {
    /* Incomplete arrays are okay as long as the underlying type is
       complete. */
  } else {
    /* Some other type; error. */
    pos_ty_error(ec_bad_compound_literal_type, type_position, literal_type);
    err = TRUE;
  }  /* if */
  if (err) literal_type = error_type();
  /* Save, clear, and later restore the expression stack, since the
     initializer is not part of any expression we may currently be
     inside of. */
  save_expr_stack(&saved_expr_stack);
  if (is_static) switch_to_file_scope_region(&region_to_switch_back_to);
  /* Scan the brace-enclosed initializer. */
  scan_compound_literal_initializer(&literal_type, is_static, &dip);
  /* No dynamic init entry will be returned if an error occurred. */
  if (dip == NULL) err = TRUE;
  /* The type can be updated for an incomplete array. */
  *p_literal_type = literal_type;
  restore_expr_stack(saved_expr_stack);
  if (err) {
    make_error_operand(result);
  } else if (is_static) {
    /* Static case.  Allocate an unnamed static variable and initialize it
       with the compound literal. */
    a_constant_ptr literal_con;
    check_assertion(dip->kind == (a_dynamic_init_kind)dik_constant);
    literal_con = dip->variant.constant;
    if (gcc_mode && !(local_options & EOPT_OPERAND_OF_ADDRESS_OF)) {
      /* In GNU C mode, the compound literal is treated as a constant-
         expression.  In some cases, the constant may later be used to
         initialize a variable (if an lvalue is needed after all). */
      make_constant_operand(literal_con, result);
    } else {
      make_lvalue_operand_from_compound_constant(literal_con, result);
    }  /* if */
  } else {
    /* Allocate an enk_temp_init node. */
    an_expr_node_ptr expr =
             alloc_temp_init_node(literal_type, dip, /*result_is_addr=*/TRUE,
                                  /*is_explicit_cast=*/FALSE);
    make_expression_operand(expr, literal_type, result);
    result->state = (an_operand_state)os_lvalue;
  }  /* if */
  if (is_static) switch_back_to_original_region(region_to_switch_back_to);
}  /* scan_compound_literal */


static void scan_cast_or_expr(
                             an_operand               *result,
                             an_operand               *bound_function_selector,
                             a_local_expr_options_set local_options)
/*
Scan something after an opening left paren.  This may be a cast operation or
just an expression in parentheses.  Return the scanned expression in
*result (and, if it is a C++ bound function, return the object in
*bound_function_selector).  See section 6.3.4 of the ISO C89 standard.

Syntax:
 	( type-name ) expression
or
	( expression )

Also scans C99 compound literals:

        ( type-name ) { expression, expression, ... }

Also scans GNU C statement expressions:

        ({ statement; statement; })
*/
{
  a_source_position start_position, type_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_type_ptr        type_cast_to;
  a_boolean         err = FALSE;
  an_operand        local_bound_function_selector;

  db_enter(4, "scan_cast_or_expr");

  /* Save the current source position.  Note that in the parenthesis-trapped
     case we are saving the position of the token after the left parenthesis,
     but that's okay; the caller straightens it out. */
  copy_source_position(pos_curr_token, start_position);

  /* Get past the opening lparen.  If a left parenthesis was trapped,
     we're already past it, so do not advance. */
  if (!(local_options & EOPT_TRAPPED_LEFT_PAREN)) (void)get_token();

#if GNU_EXTENSIONS_ALLOWED
  if (gcc_mode && curr_token == tok_lbrace) {
    /* GNU C statement expression, ({...}). */
    scan_gnu_statement_expression(result);
  } else
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    add_matching_stop_token(tok_rparen);
    /* Determine if this is a cast operation.  Cast operations are not
       allowed in preprocessing expressions (although the check is superfluous
       since identifiers are never recognized as type names, and therefore
       is_decl_not_expr would never return TRUE). */
    if (!curr_expr_kind_is(ek_pp) &&
        is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                             DFS_SINGLE_TYPE_REQUIRED |
                             DFS_IS_CAST)) {
      /* This is a cast operation. */
      a_boolean explicit_cv_qualifiers;
      /* Get the type to cast to. */
      type_position = pos_curr_token;
      type_name_full(&type_cast_to, &explicit_cv_qualifiers);
      /* The next token should be the closing rparen. */
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_matching_stop_token(tok_rparen);

      if (compound_literals_allowed &&
          curr_token == tok_lbrace) {
        /* A compound literal, e.g., (int []){1, 2, 3}.  See 6.5.2.5 in C99. */
        scan_compound_literal(&type_cast_to, &type_position, result,
                              local_options);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      } else {
        /* Normal cast (not a compound literal). */
        /* Check the type to see if it is valid in general terms. */
        error_position = type_position;
        err = cast_type_pre_check(&type_cast_to, explicit_cv_qualifiers);
        set_err_pos_to_curr_token();
        /* Scan the expression to be cast. */
        scan_cast_expression(type_cast_to, /*allow_comma=*/TRUE, PREC_CAST,
                             result, &local_bound_function_selector);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        end_position = result->end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Check compatibility of the types and do the cast. */
        do_cast(type_cast_to, result, &local_bound_function_selector,
                local_options, err, &type_position, &start_position);
      }  /* if */
      set_operand_position(result, &start_position, &end_position,
                           &start_position);
    } else {
      /* This is an expression in parentheses. */
      /* Parentheses do not affect the fact that the expression is the
         immediate operand of a cast, so pass down that option. */
      a_local_expr_options_set options =
                                      (local_options &
                                                (EOPT_OPERAND_OF_CAST |
                                                 EOPT_MICROSOFT_CASE_LABEL |
                                                 EOPT_OPERAND_OF_ADDRESS_OF)) |
                                       EOPT_ALLOW_BOUND_FUNCTION |
                                       EOPT_PRESERVE_PROPERTY_REF;
      /* Ordinarily, parentheses do affect whether an expression is the
         immediate operand of a "&" (because the syntax for a pointer-to-member
         requires that there be no parentheses).  However, in cfront mode
         &(X::Y), where X::Y is a data member, can be a pointer-to-member,
         so pass down that option. */
      if (any_cfront_mode()) {
        options |= (local_options & EOPT_PTR_TO_MEMBER_CONTEXT);
      }  /* if */
      scan_expr_full(result, bound_function_selector, PREC_LOWEST, options);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_matching_stop_token(tok_rparen);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      if (is_constant_operand(result) &&
          result->variant.constant.expr == NULL) {
        /* Record an expression for a constant so that we have the position
           of the constant and also the position of the constant surrounded
           by parentheses. */
        result->variant.constant.expr = make_node_from_operand(result);
      }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      /* Do not use set_operand_position because we want to leave the
         position in any underlying expression unchanged (we didn't add
         anything to the expression to represent the parentheses, so the
         expression still represents the thing inside the parentheses). */
      set_base_operand_position(result, &start_position, &end_position);
    }  /* if */
  }  /* if */

  db_exit();
}  /* scan_cast_or_expr */


static a_boolean conversion_has_one_argument(void)
/*
The current token position is the opening parenthesis of a
functional-notation cast.  Look ahead in the input and determine the
number of arguments of the conversion.  If the conversion has exactly
one argument, return TRUE; otherwise, return FALSE.
*/
{
  a_boolean          one_arg = FALSE;
  a_token_cache      cache;
  a_token_set_array  stop_tokens;

  clear_token_cache(&cache, /*reusable=*/FALSE);
  if (curr_token == tok_lparen) {
    cache_curr_token(&cache);
    /* Get the first token inside the parentheses. */
    (void)get_token();
    if (curr_token == tok_rparen) {
      /* The argument list is "()", i.e., zero arguments. */
    } else {
      /* One or more arguments. */
      /* Scan forward looking for a zero-level comma or right parenthesis. */
      /* Initialize a local stop token set. */
      clear_token_set_array(stop_tokens);
      incr_token_set_array_element(stop_tokens, tok_comma);
      incr_token_set_array_element(stop_tokens, tok_rparen);
      /* Note that this really should use the version of cache_token_stream
         that coalesces ids, to get the right answer with template
         references.  That's hard to do, because you have to have a 
         cache pre-built containing the right tokens.  But this routine
         is now used only in some corner cases in some corner modes
         (e.g., cfront), so this answer is good enough.  (Before this
         was relegated to use in corner modes, it was in use for years,
         and we got no bug reports about it.) */
      cache_token_stream(&cache, stop_tokens);
      /* If we stopped on a right parenthesis, the argument list has exactly
         one argument. */
      if (curr_token == tok_rparen) one_arg = TRUE;
    }  /* if */
  }  /* if */
  /* Restore the tokens. */
  rescan_cached_tokens(&cache);

  return one_arg;
}  /* conversion_has_one_argument */


static void scan_functional_notation_type_conversion(
                                      a_type_ptr               type_cast_to,
                                      a_source_position        *start_position,
                                      an_operand               *result,
                                      a_local_expr_options_set local_options)
/*
Scan a C++ functional-notation type conversion, e.g., "int(1.5)" or "A(1,2)".
The type keyword or identifier has been scanned over (the current token is
the parenthesis following that), and the associated type is passed in as
type_cast_to.  The starting position of the type is given by *start_position.
The result is returned in *result.  See _expr.type.conv_ in the WP.
*/
{
  a_source_position             lparen_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position             end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean                     err = FALSE;
  a_symbol_ptr                  ctor_sym;
  an_expr_node_ptr              arg_expr_list;
  a_routine_ptr                 ctor_routine;
  a_boolean                     ctor_case = FALSE;
  a_class_symbol_supplement_ptr cssp;
  an_operand                    local_bound_function_selector;

  db_enter(4, "scan_functional_notation_type_conversion");

  error_position = *start_position;
  /* Check the type to see if it is valid in general terms.  Note that
     this does a worthwhile check even in the class case (abstract class).
     However, cv-qualifiers cannot syntactically appear in this sort of
     explicit conversion. */
  err = cast_type_pre_check(&type_cast_to, /*explicit_cv_qualifiers=*/FALSE);
  /* See if we have a case that is clearly a constructor call. */
  if (is_class_struct_union_type(type_cast_to)) {
    cssp = symbol_supplement_for_class(type_cast_to);
    ctor_sym = cssp->constructor;
    if (ctor_sym != NULL) {
      /* The class has a constructor. */
      ctor_case = TRUE;
      if (any_cfront_mode() && 
          cssp->target_of_conversion_function &&
          conversion_has_one_argument()) {
        /* Old rules for cfront mode: conversion functions compete with
           constructors. */
        /* The class has a constructor, there is at least one conversion
           function from some other class to this one, and the argument list
           contains a single value, so this is treated as a normal
           (non-constructor) case.  Note that there might or might not be
           a one-argument constructor for the class; if there is, it will
           be considered along with the conversion function. */
        ctor_case = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Check for a left parenthesis. */
  copy_source_position(pos_curr_token, lparen_pos);
  (void)required_token(tok_lparen, ec_exp_lparen);
  if (ctor_case) {
    /* Converting to a class type.  The contents of the parentheses are
       arguments for a constructor call. */
    a_boolean unknown_dependent_function;
    a_boolean empty_parens = (curr_token == tok_rparen);
    scan_ctor_arguments(ctor_sym, &arg_expr_list, &ctor_routine,
                        &unknown_dependent_function, &lparen_pos,
                        type_cast_to);
    error_position = *start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (err || (ctor_routine == NULL && !unknown_dependent_function)) {
      /* Error of some sort. */
      make_error_operand(result);
    } else {
      /* Make a dynamic init entry that calls constructor to initialize
         a temporary.  Make an operand for the value of the temporary. */
      make_constructor_dynamic_init(ctor_routine, arg_expr_list,
                                    type_cast_to, /*result_is_addr=*/FALSE,
                                    /*is_explicit_cast=*/TRUE,
                                    /*is_value_init=*/empty_parens,
                                    start_position, result);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_bugs) {
        /* In Microsoft C++ mode, a constructor is considered to return
           an lvalue. */
        conv_class_operand_to_object_pointer(result);
        conv_object_pointer_to_lvalue(result);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  } else if (!curr_expr_kind_is_const() &&
             is_template_dependent_context() &&
             is_template_dependent_type(type_cast_to)) {
    /* A cast to an unknown type in a prototype instantiation.  This is
       handled specially because it may have more than one argument.
       In a constant expression, a cast to a class type is not allowed,
       so go on to the normal cast code. */
    an_expr_node_ptr   temp_init_node;
    a_dynamic_init_ptr dip;
    scan_dependent_parenthesized_initializer(&dip);
    temp_init_node = alloc_temp_init_node(type_cast_to, dip,
                                          /*result_is_addr=*/FALSE,
                                          /*is_explicit_cast=*/TRUE);
    make_expression_operand(temp_init_node, temp_init_node->type, result);
  } else {
    /* Not a constructor case; obeys the same rules as a C-style cast. */
    add_matching_stop_token(tok_rparen);
    if (curr_token == tok_rparen) {
      /* Empty parentheses. */
      if (err) {
        /* Some previous error. */
        make_error_operand(result);
      } else if (is_reference_type(type_cast_to)) {
        /* Disallow a cast to a reference type without operands; you
           can't default-initialize a reference.  The standard as of
           TC1 makes this not an error, because the initialization is
           value-initialization, and there's no error for value-
           initializing a reference, but that has to be wrong. */
        pos_error(ec_bad_cast, &lparen_pos);
        make_error_operand(result);
      } else {
        /* See if the cast is valid in the current expression kind by
           seeing whether a constant zero can be cast to the destination
           type. */
        make_integer_constant_operand(result, (a_host_large_integer)0L);
        if (!cast_is_valid_in_current_expression_kind(result, type_cast_to,
                                                      local_options,
                                                      start_position)) {
          /* This cast is not valid in this kind of expression. */
          err = TRUE;
        } else if (is_void_type(type_cast_to)) {
          /* void(). */
          cast_operand_to_void(result, type_cast_to);
        } else if (is_class_struct_union_type(type_cast_to)) {
          /* A class with no constructor, followed by (), e.g., "A()".
             This is value-initialization, but we know the class has
             no non-trivial constructor, so it's effectively
             zero-initialization. */
          a_dynamic_init_ptr dip;
          an_expr_node_ptr   temp_init_node =
                  create_expr_temporary(type_cast_to,
                                        /*result_is_addr=*/FALSE,
                                        /*is_explicit_cast=*/TRUE,
                                        /* Abstract class test done
                                           previously. */
                                        /*suppress_abstract_test=*/TRUE,
                                        (a_dynamic_init_kind)dik_zero,
                                        start_position,
                                        &dip);
          /* Force generation of the trivial default constructor for a
             non-POD class to detect any errors.  This is correct according
             to the C++98 standard, but suspect after the TC1 changes
             for value-initialization (because the definition is not
             written in terms of calling the constructor, and therefore
             doesn't force the generation of the constructor). */
          (void)reference_to_trivial_default_constructor(type_cast_to,
                                                         &lparen_pos);
          make_expression_operand(temp_init_node, temp_init_node->type,
                                  result);
        } else {
          /* A scalar type followed by (); generate the value a static
             object of that type would get by default (WP _expr.type.conv_),
             which is to say zero converted to the type. */
          cast_operand(type_cast_to, result, /*check_cast_access=*/FALSE,
                       /*is_implicit_cast=*/FALSE,
                       /*is_reinterpret_cast=*/FALSE,
                       /*reinterpret_semantics=*/FALSE);
        }  /* if */
      }  /* if */
    } else {
      /* Non-empty parentheses. */
      /* Scan the expression inside the parentheses. */
      /* Since the expression in parentheses is syntactically an
         expression list, a top-level comma is not allowed. */
      scan_cast_expression(type_cast_to, /*allow_comma=*/FALSE, PREC_LOWEST,
                           result, &local_bound_function_selector);
      /* Check compatibility of the types and do the cast. */
      do_cast(type_cast_to, result, &local_bound_function_selector,
              local_options, err, start_position, start_position);
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for the closing parenthesis. */
    check_closing_paren_after_expr_list();
    remove_matching_stop_token(tok_rparen);
  }  /* if */
  set_operand_position(result, start_position, &end_position, start_position);
  db_exit();
}  /* scan_functional_notation_type_conversion */


#if MICROSOFT_EXTENSIONS_ALLOWED

static void adjust_operands_for_microsoft_int_long_bug(an_operand *operand_1,
                                                       an_operand *operand_2)
/*
Microsoft's Visual C++ compiler has a bug that makes an expression like
"l + i", where l is a long and i is an int, have a result type of int.
This routine is called in cases where that bug should be duplicated.
operand_1 and operand_2 are the first and second operands of an operation.
There are similar cases other than long and int, and the cases are not
symmetrical, e.g., "i + l" does not yield an int.
*/
{
  if (microsoft_bugs && 
      targ_sizeof_long == targ_sizeof_int &&
      is_integral_type(operand_1->type) &&
      is_integral_type(operand_2->type) &&
      (!is_constant_operand(operand_1) || !is_constant_operand(operand_2))) {
    a_type_ptr      op1_type = skip_typerefs(operand_1->type);
    a_type_ptr      op2_type = skip_typerefs(operand_2->type);
    an_integer_kind t1 = op1_type->variant.integer.int_kind;
    an_integer_kind t2 = op2_type->variant.integer.int_kind;
    an_integer_kind c1 = t1;
    an_integer_kind c2 = t2;
    if (t1 == (an_integer_kind)ik_long &&
        (t2 == (an_integer_kind)ik_int ||
         t2 == (an_integer_kind)ik_short ||
         t2 == (an_integer_kind)ik_unsigned_short ||
         t2 == (an_integer_kind)ik_char ||
         t2 == (an_integer_kind)ik_unsigned_char ||
         t2 == (an_integer_kind)ik_signed_char)) {
      c1 = (an_integer_kind)ik_int;
    } else if (t1 == (an_integer_kind)ik_long &&
               t2 == (an_integer_kind)ik_unsigned_int) {
      c1 = (an_integer_kind)ik_unsigned_int;
    } else if (t1 == (an_integer_kind)ik_unsigned_int &&
               (t2 == (an_integer_kind)ik_long ||
                t2 == (an_integer_kind)ik_unsigned_long)) {
      c2 = (an_integer_kind)ik_unsigned_int;
    }  /* if */
    if (c1 != t1) {
      cast_operand(integer_type(c1), operand_1,
                   /*check_cast_access=*/FALSE, /*is_implicit_cast=*/TRUE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    }  /* if */
    if (c2 != t2) {
      cast_operand(integer_type(c2), operand_2,
                   /*check_cast_access=*/FALSE, /*is_implicit_cast=*/TRUE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    }  /* if */
  }  /* if */
}  /* adjust_operands_for_microsoft_int_long_bug */

#else /* !MICROSOFT_EXTENSIONS_ALLOWED */

#define adjust_operands_for_microsoft_int_long_bug(op1, op2) /* Nothing */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void scan_mult_operator(an_operand *operand_1,
                               an_operand *result)
/*
Scan the "*", "/", and "%" operators.  The operands of the "*" and "/"
operators must be of arithmetic type.  The operands of the "%" operator must
be of integral type.  See section 3.3.5 of the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  an_expr_operator_kind op;
  a_type_ptr            result_type;
  a_boolean             processed = FALSE;

  db_enter(4, "scan_mult_operator");

  /* Save the current token kind. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_MULT_DIV, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be of arithmetic or enum type (the remainder
       operator requires integral or enum type). */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    if (save_token == tok_remainder) {
      (void)check_integral_or_enum_operand(operand_1);
    } else {
      (void)check_arithmetic_or_enum_operand(operand_1);
    }  /* if */
    /* The second operand must be of arithmetic or enum type (the remainder
       operator requires integral or enum type). */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    if (save_token == tok_remainder) {
      (void)check_integral_or_enum_operand(&operand_2);
    } else {
      (void)check_arithmetic_or_enum_operand(&operand_2);
    }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Check for cases involving imaginary types that do not fall out
       of the normal usual arithmetic conversion rules. */
    if (!c99_mode ||
        !determine_imaginary_operation_type(save_token, operand_1, &operand_2,
                                            &result_type, &op))
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    /* Do not insert code here. */
    {
      adjust_operands_for_microsoft_int_long_bug(operand_1, &operand_2);
      result_type = determine_arithmetic_conversions(operand_1, &operand_2);
      change_binary_operand_types(result_type, operand_1, &operand_2);
      op = which_binary_operator(save_token, result_type);
    }  /* if */
    if ((save_token == tok_divide || save_token == tok_remainder) &&
        curr_expr_is_evaluated() &&
        !is_constant_operand(operand_1) && op_is_zero_constant(&operand_2)) {
      /* Warn on a division or mod by zero.  This is handled in folding.c
         for the constant case, but here if only the second operand is
         constant. */
      pos_warning((save_token == tok_divide) ? ec_divide_by_zero :
                                               ec_mod_by_zero,
                  &operand_2.position);
    }  /* if */
    do_binary_operation(op, operand_1, &operand_2,
                        result_type, result, &operator_position);
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_mult_operator */


static void scan_add_operator(an_operand *operand_1,
                              an_operand *result)
/*
Scan the non-unary "+" and "-" operators.  See section 3.3.6 in the standard.
*/
{
  an_expr_operator_kind op;
  a_token_kind          save_token;
  an_operand            operand_2;
  an_operand            operand_temp;
  a_source_position     operator_position;
  a_boolean             operands_have_been_reversed = FALSE;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_boolean             operand_1_is_pointer;
  a_boolean             both_operands_are_arithmetic = FALSE;
  a_boolean		pointer_difference           = FALSE;
  a_boolean             err = FALSE, processed = FALSE;
  a_type_ptr            result_type;
  a_type_ptr            operation_type;
  a_boolean             imaginary_arithmetic = FALSE;

  db_enter(4, "scan_add_operator");

  /* Save the current token kind. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_PLUS_MINUS, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be an arithmetic or enum type or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = FALSE;
    if (is_arithmetic_or_enum_type(operand_1->type)) {
      /* Okay. */
    } else if (check_pointer_operand(
                               operand_1,
                               enum_type_is_integral ?
                                 ec_expr_not_scalar :
                                 ec_expr_not_arithmetic_or_enum_or_pointer)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check whether the operand types are valid. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      err = TRUE;
    } else if (operand_1_is_pointer) {
      /* Operand 1 has pointer type. */
      if (is_integral_or_enum_type(operand_2.type)) {
        /* Pointer +- integral/enum. */
        /* The first operand must be a pointer to an object. */
        if (gcc_mode && (is_void_type(type_pointed_to(operand_1->type)) ||
                         is_function_type(type_pointed_to(operand_1->type)))) {
          /* Some versions of GNU C accept arithmetic on void and function
             pointers. */
          pos_warning(ec_nonobject_pointer_arithmetic, &operator_position);
        } else {
#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
          /* Pointer to incomplete array is also allowed. */
          (void)check_object_or_incomp_array_pointer_operand(operand_1,
                                                 ec_expr_not_pointer_to_object,
                                                             &operand_2);
#else /* !PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
          (void)check_object_pointer_operand(operand_1,
                                             ec_expr_not_pointer_to_object);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
        }  /* if */
        /* The result type is the same as the pointer type in operand 1. */
        result_type = operation_type = operand_1->type;
      } else if (save_token == tok_minus && is_pointer_type(operand_2.type)) {
        /* Pointer - pointer. */
        a_type_ptr  type_1 = type_pointed_to(operand_1->type),
                    type_2 = type_pointed_to(operand_2.type);

        pointer_difference = TRUE;
        /* In ANSI C and C++, both operands must be pointers to qualified or
           unqualified members of compatible complete object types
           (ISO C 6.3.6, ISO C++ 5.7).  The result has type ptrdiff_t. */
        if (types_are_compatible_ignoring_qualifiers(type_1, type_2)) {
          operation_type = skip_typerefs(operand_1->type);
        } else if (check_compatibility_of_pointer_operands(
                          operand_1, &operand_2, &operator_position,
                          /*pointer_normalization_standard_in_C=*/FALSE,
                          /*pointers_to_functions_standard_in_C=*/FALSE,
                          /*pointers_to_incomplete_standard_in_C=*/FALSE,
                          /*mixed_object_and_incomplete_standard_in_C=*/FALSE,
                          &operation_type)) {
          /* Traditionally (ARM C++), certain differences in the types pointed
             to have been accepted. */
          if (!(any_cfront_mode() || microsoft_mode)) {
            pos_ty2_diagnostic(strict_ansi_mode ? strict_ansi_error_severity
                                                : es_warning,
                               ec_nonstandard_ptr_minus_ptr,
                               &operator_position,
                               operand_1->type, operand_2.type);
          }  /* if */
        } else {
          err = TRUE;
        }  /* if */
        /* Check that the types pointed to are complete (except perhaps in GNU
           C mode: void and function types are acceptable). */
        if (err) {
          /* An error message was already issued. */
        } else if (gcc_mode &&
                   (is_void_type(type_pointed_to(operand_1->type)) ||
                    is_function_type(type_pointed_to(operand_1->type)))) {
          /* Some versions of GNU C allows arithmetic on pointers to void and
             pointers to functions. */
          pos_warning(ec_nonobject_pointer_arithmetic, &operator_position);
          result_type = integer_type(targ_ptrdiff_t_int_kind);
        } else if (check_object_pointer_operand(
                                operand_1, ec_expr_not_pointer_to_object) &
                   check_object_pointer_operand(
                                &operand_2, ec_expr_not_pointer_to_object)) {
          /* Note use of "&" rather than "&&" to ensure that both tests
             are done even if the first detects an error. */
          result_type = integer_type(targ_ptrdiff_t_int_kind);
        } else {
          /* An error message was already issued. */
          err = TRUE;
        }  /* if */
      } else {
        /* Pointer +- non-integral.  Error. */
        error_in_operand(enum_type_is_integral ?
                           ec_expr_not_integral : ec_expr_not_integral_or_enum,
                         &operand_2);
        err = TRUE;
      }  /* if */
    } else if (save_token == tok_plus &&
               is_pointer_type(operand_2.type) &&
               is_integral_or_enum_type(operand_1->type)) {
      /* Integral/enum + pointer. */
      /* The second operand must be a pointer to an object.  GNU C allows
         arithmetic on pointers to void and pointers to functions. */
      if (gcc_mode && (is_void_type(type_pointed_to(operand_2.type)) ||
                       is_function_type(type_pointed_to(operand_2.type)))) {
        /* Fine, but issue a warning because some versions of GNU C are
           more strict. */
        pos_warning(ec_nonobject_pointer_arithmetic, &operator_position);
      } else {
#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
        /* Pointer to incomplete array is also allowed. */
        (void)check_object_or_incomp_array_pointer_operand(&operand_2,
                                                 ec_expr_not_pointer_to_object,
                                                         operand_1);
#else /* !PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
       (void)check_object_pointer_operand(&operand_2,
                                         ec_expr_not_pointer_to_object);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
      }  /* if */
      /* The result type is the same as the pointer type in operand 2. */
      result_type = operation_type = operand_2.type;
      /* Reverse the operands so that the pointer is always first. */
      copy_operand(operand_1, &operand_temp);
      copy_operand(&operand_2, operand_1);
      copy_operand(&operand_temp, &operand_2);
      /* Remember that we did this so that we can adjust the position
         information later on. */
      operands_have_been_reversed = TRUE;
    } else {
      /* Operand 1 is arithmetic or enum. */
      if (is_arithmetic_or_enum_type(operand_2.type)) {
        /* Arithmetic/enum +- arithmetic/enum. */
#if C99_IL_EXTENSIONS_SUPPORTED
        /* Check for cases involving imaginary types that do not fall out
           of the normal usual arithmetic conversion rules. */
        if (c99_mode &&
            determine_imaginary_operation_type(save_token,
                                               operand_1, &operand_2,
                                               &result_type, &op)) {
          imaginary_arithmetic = TRUE;
          operation_type = NULL;  /* Not used. */
        } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        /* Do not insert code here. */
        {
          /* Determine the result type based on the 2 operands. */
          adjust_operands_for_microsoft_int_long_bug(operand_1, &operand_2);
          result_type = operation_type =
                       determine_arithmetic_conversions(operand_1, &operand_2);
        }  /* if */
        both_operands_are_arithmetic = TRUE;
      } else {
        /* Arithmetic +- non-arithmetic.  Error. */
        error_in_operand(enum_type_is_integral ?
                           ec_expr_not_arithmetic :
                           ec_expr_not_arithmetic_or_enum,
                         &operand_2);
        err = TRUE;
      }  /* if */
    }  /* if */

    if (err) {
      make_error_operand(result);
    } else {
      /* Promote the operands if necessary. */
      /* Note that integral promotions are NOT done on the integer in
         "pointer + integer" and "pointer - integer".  This is as
         the standard wants it. */
      if (both_operands_are_arithmetic || pointer_difference) {
        if (!imaginary_arithmetic) {  /*lint !e774*/
          change_binary_operand_types(operation_type, operand_1, &operand_2);
        }  /* if */
      }  /* if */
      /* Determine the expression operator for this case. */
      if (pointer_difference) {
        /* Pointer - pointer is a special case, with its own operator. */
        op = (an_expr_operator_kind)eok_pdiff;
      } else if (imaginary_arithmetic) {  /*lint !e774*/
        /* op is already set. */
      } else {
        op = which_binary_operator(save_token, operation_type);
      }  /* if */
      do_binary_operation(op, operand_1, &operand_2,
                          result_type, result, &operator_position);
      /* For pointer addition or subtraction (but not difference), preserve
         the reference entries for the pointer operand.  This is in case the
         result is turned back into an lvalue, as in "*(arr + 1) = x".
         Recall that the pointer operand is always operand_1 by this point. */
      if (op == (an_expr_operator_kind)eok_padd ||
          op == (an_expr_operator_kind)eok_psubtract) {
        result->ref_entries_list = operand_1->ref_entries_list;
      }  /* if */
    }  /* if */
  }  /* if */

  set_operand_position(
        result,
        &(operands_have_been_reversed ? &operand_2 : operand_1)->position,
        &(operands_have_been_reversed ? operand_1 : &operand_2)->end_position,
        &operator_position);
  db_exit();
}  /* scan_add_operator */


static void scan_shift_operator(an_operand *operand_1,
                                an_operand *result)
/*
Scan the "<<" and ">>" operators.  See section 3.3.7 of the standard.
*/
{
  an_expr_operator_kind op;
  an_operand            operand_2;
  a_token_kind          save_token;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_type_ptr            result_type;
  an_error_code         err_code;
  a_boolean             processed = FALSE;

  db_enter(4, "scan_shift_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_SHIFT, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral or enum. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    (void)check_integral_or_enum_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    (void)check_integral_or_enum_operand(&operand_2);

    if (C_dialect == C_dialect_pcc) {
      /* In K&R first edition (see appendix A, section 7.5), "perform the usual
         arithmetic conversions on their operands, each of which must be 
         integral.  Then the right operand is converted to int; the type of
         the result is that of the left operand."  This has the effect
         that a "long" shift count will force the shift to be done as long. */
      result_type = determine_arithmetic_conversions(operand_1, &operand_2);
      change_binary_operand_types(result_type, operand_1, &operand_2);
      cast_operand(integer_type((an_integer_kind)ik_int), &operand_2,
                   /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    } else {
      /* ANSI rules just call for the integral promotions; the type of
         the result is the type of the left operand. */
      promote_operand(operand_1);
      promote_operand(&operand_2);
    }  /* if */
    if (curr_expr_is_evaluated() && is_constant_operand(&operand_2) &&
        !is_constant_operand(operand_1) && !is_error_operand(operand_1) &&
        operand_2.variant.constant.kind == (a_constant_repr_kind)ck_integer) {
      /* Check the shift count.  This is checked in folding.c for the
         fully constant case, but here if only the second operand is
         constant. */
      check_shift_count(&operand_2.variant.constant, operand_1->type,
                        &err_code);
      if (err_code != ec_no_error) pos_warning(err_code, &operand_2.position);
    }  /* if */
    result_type = operand_1->type;
    op = which_binary_operator(save_token, result_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &error_position);
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_shift_operator */


static a_boolean is_comparison_of_unsigned_with_constant(
                                                an_operand *operand_1,
                                                an_operand *operand_2,
                                                a_boolean  *second_is_constant)
/*
operand_1 and operand_2 are the operands of a comparison operation before
the usual arithmetic conversions.  Check to see if the comparison will
be comparing an unsigned integral value with a constant.  Return TRUE if so,
and also return *second_is_constant set to indicate which of the two operands
is the constant.
*/
{
  a_type_ptr operand_type;
  a_boolean  is_comparison = FALSE;

  *second_is_constant = FALSE;
  if (curr_expr_is_evaluated()) {
    operand_type = NULL;
    *second_is_constant = is_constant_operand(operand_2);
    if (!is_constant_operand(operand_1)) {
      if (*second_is_constant) {
        /* The first operand is nonconstant, the second constant. */
        operand_type = operand_1->type;
      }  /* if */
    } else {
      if (!*second_is_constant) {
        /* The second operand is nonconstant, the first constant. */
        operand_type = operand_2->type;
      }  /* if */
    }  /* if */
    if (operand_type != NULL) {
      /* Check to see if the nonconstant operand has an unsigned integral or
         enum type. */
      if (is_integral_or_enum_type(operand_type) &&
          !is_signed_integral_type(operand_type)) {
        /* Yes. */
        is_comparison = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_comparison;
}  /* is_comparison_of_unsigned_with_constant */


static a_boolean get_sign_for_constant_in_unsigned_operation(
                                                 an_operand *operand_1,
                                                 an_operand *operand_2,
                                                 a_boolean  second_is_constant,
                                                 a_boolean  *constant_sign)
/*
operand_1 and operand_2 are the operands of a comparison after the usual
arithmetic conversions.  is_comparison_of_unsigned_with_constant has
previously identified this operation as a comparison of an unsigned value
to a constant, and second_is_constant indicates which of the operands is
the constant.  Fetch the sign of the constant and return it in *constant_sign.
If the indicated operand is not an integral constant (e.g., because
of an error), return FALSE.
*/
{
  a_boolean      okay = FALSE;
  an_operand     *operand = second_is_constant ? operand_2 : operand_1;
  a_constant_ptr constant;

  /* Make sure the operand is still a constant. */
  if (is_constant_operand(operand)) {
    constant = &operand->variant.constant;
    if (is_integral_or_enum_type(constant->type) &&
        constant->kind == (a_constant_repr_kind)ck_integer) {
      okay = TRUE;
      *constant_sign = sign_of_integer_constant(constant);
    }  /* if */
  }  /* if */
  return okay;
}  /* get_sign_for_constant_in_unsigned_operation */


static void scan_rel_operator(an_operand *operand_1,
                              an_operand *result)
/*
Scan the "<", ">", "<=", and "=>" operators.  See section 3.3.8 of the
standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_type_ptr            operation_type;
  a_type_ptr            result_type;
  an_expr_operator_kind op;
  a_boolean             operand_1_is_pointer;
  a_boolean             processed = FALSE;
  a_boolean             funny_unsigned_comparison = FALSE, second_is_constant;

  db_enter(4, "scan_rel_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_RELATIONAL, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be an arithmetic or enum type or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = FALSE;
    if (is_arithmetic_or_enum_type(operand_1->type)) {
      /* Okay. */
    } else if (check_pointer_operand(
                               operand_1,
                               enum_type_is_integral ?
                                 ec_expr_not_scalar :
                                 ec_expr_not_arithmetic_or_enum_or_pointer)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check the operand types for compatibility. */
    operation_type = operand_1->type;  /* Assume. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      /* One or both of the operands has an error. */
      operation_type = error_type();
#if C99_IL_EXTENSIONS_SUPPORTED
    } else if (is_nonreal_floating_type(operand_1->type)) {
      /* Complex and imaginary operands are unordered. */
      pos_error(ec_complex_type_not_allowed, &operand_1->position);
      operation_type = error_type();
    } else if (is_nonreal_floating_type(operand_2.type)) {
      pos_error(ec_complex_type_not_allowed, &operand_2.position);
      operation_type = error_type();
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    } else {
      if (operand_1_is_pointer || is_pointer_type(operand_2.type)) {
        /* At least one of the operands is a pointer.  See if the operands are
           compatible.  In C, operands must be both pointers to objects or both
           pointers to incomplete types; pointers to functions are not allowed;
           and null pointer constants and "void *" pointers have no special
           meaning (ANSI C 3.3.8).  In C++, pointers to functions are allowed,
           and null pointer constants and "void *" pointers are specially
           handled (ARM 5.9).  We extend the C mode to be the same as the
           C++ mode, but issue warnings in strict ANSI mode. */
        (void)check_compatibility_of_pointer_operands(
                           operand_1, &operand_2, &operator_position,
                           /*pointer_normalization_standard_in_C=*/FALSE,
                           /*pointers_to_functions_standard_in_C=*/FALSE,
                           /*pointers_to_incomplete_standard_in_C=*/TRUE,
                           /*mixed_object_and_incomplete_standard_in_C=*/FALSE,
                           &operation_type);
      } else {
        /* Both operands should be arithmetic or enum (we have ruled out all
           the pointer cases above).  We already know that operand_1 is
           arithmetic or enum. */
        if (check_arithmetic_or_enum_operand(&operand_2)) {
          /* Check for comparisons of unsigned integers with zero or negative
             constants.  More below. */
          funny_unsigned_comparison = is_comparison_of_unsigned_with_constant(
                                                          operand_1,
                                                          &operand_2,
                                                          &second_is_constant);
        }  /* if */
        operation_type = determine_arithmetic_conversions(operand_1,
                                                          &operand_2);
      }  /* if */
    }  /* if */
    /* Determine the result type. */
    result_type = boolean_result_type();
    /* Convert the operands to a common type. */
    change_binary_operand_types(operation_type, operand_1, &operand_2);
    if (funny_unsigned_comparison) {
      /* Check for pointless comparisons of unsigned integers against 0,
         and give a warning.  The pointless cases are
           u >= 0    (always true)
           u <  0    (always false)
           0 >  u    (always false)
           0 <= u    (always true)
         There are also similar cases with negative constants.
         The expression is not simplified.  Note that we check the nonconstant
         operand type before any type promotions and the constant value after
         any type change. */
      a_boolean constant_sign;
      if (get_sign_for_constant_in_unsigned_operation(operand_1, &operand_2,
                                                      second_is_constant,
                                                      &constant_sign)) {
        if (constant_sign == 0) {
          /* Comparison of an unsigned value with zero.  Some cases make
             sense. */
          if (second_is_constant ?
                              (save_token == tok_ge || save_token == tok_lt) :
                              (save_token == tok_gt || save_token == tok_le)) {
            pos_warning(ec_unsigned_compare_with_zero, &operator_position);
          }  /* if */
        } else if (constant_sign < 0) {
          /* Comparison of an unsigned value with a negative constant.
             No cases make sense. */
          pos_warning(ec_unsigned_compare_with_negative, &operator_position);
        }  /* if */
      }  /* if */
    }  /* if */
    op = which_binary_operator(save_token, operation_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position);
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_rel_operator */


static void scan_eq_operator(an_operand *operand_1,
                             an_operand *result)
/*
Scan the "==" and "!=" operators.  See section 3.3.9 in the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_type_ptr            operation_type;
  a_type_ptr            result_type;
  an_expr_operator_kind op;
  a_boolean             operand_1_is_pointer, operand_1_is_ptr_to_member;
  a_boolean             processed = FALSE;
  a_boolean             funny_unsigned_comparison = FALSE, second_is_constant;

  db_enter(4, "scan_eq_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_EQ_NE, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be an arithmetic or enum type or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = operand_1_is_ptr_to_member = FALSE;
    if (is_arithmetic_or_enum_type(operand_1->type)) {
      /* Okay. */
    } else if (is_ptr_to_member_type(operand_1->type)) {
      operand_1_is_ptr_to_member = TRUE;
    } else if (check_pointer_operand(
                               operand_1,
                               enum_type_is_integral ?
                                 ec_expr_not_scalar :
                                 ec_expr_not_arithmetic_or_enum_or_pointer)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check the operand types for compatibility. */
    operation_type = operand_1->type;  /* Assume. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      /* One or both of the operands has an error. */
      operation_type = error_type();
    } else {
      if (operand_1_is_pointer || is_pointer_type(operand_2.type)) {
        /* At least one of the operands is a pointer.  See if the operands are
           compatible.  In C, the operands must be pointers to qualified or
           unqualified versions of compatible types (i.e., object, incomplete,
           or function types), and null pointer constants and "void *" pointers
           are specially handled (ANSI C 3.3.9).  Ditto in C++ (ARM 5.10). */
        (void)check_compatibility_of_pointer_operands(
                           operand_1, &operand_2, &operator_position,
                           /*pointer_normalization_standard_in_C=*/TRUE,
                           /*pointers_to_functions_standard_in_C=*/TRUE,
                           /*pointers_to_incomplete_standard_in_C=*/TRUE,
                           /*mixed_object_and_incomplete_standard_in_C=*/TRUE,
                           &operation_type);
      } else if (operand_1_is_ptr_to_member ||
                 is_ptr_to_member_type(operand_2.type)) {
        /* At least one operand is a pointer to member.  See if the operands
           are compatible. */
        (void)check_ptr_to_member_operands_for_compatibility(
                           operand_1, &operand_2, &operator_position,
                           &operation_type);
      } else {
        /* Both operands should be arithmetic or enum (we have ruled out all
           the pointer cases above).  We also know already that operand_1 is
           arithmetic or enum. */
        if (check_arithmetic_or_enum_operand(&operand_2)) {
          /* Check for comparisons like "unsignedvar == -1", which are true
             only in surprising cases. */
          funny_unsigned_comparison = is_comparison_of_unsigned_with_constant(
                                                          operand_1,
                                                          &operand_2,
                                                          &second_is_constant);
        }  /* if */
        operation_type = determine_arithmetic_conversions(operand_1,
                                                          &operand_2);
      }  /* if */
    }  /* if */

    result_type = boolean_result_type();
    change_binary_operand_types(operation_type, operand_1, &operand_2);
    if (funny_unsigned_comparison) {
      /* Check for pointless comparisons of unsigned integers against
         negative constants:
           u == -n   (always false)
           u != -n   (always true)
         The expression is not simplified.  Note that we check the
         nonconstant operand type before any type promotions and the
         constant value after any type change. */
      a_boolean constant_sign;
      if (get_sign_for_constant_in_unsigned_operation(operand_1, &operand_2,
                                                      second_is_constant,
                                                      &constant_sign) &&
          constant_sign < 0) {
        /* Comparison of an unsigned value with a negative constant. */
        pos_warning(ec_unsigned_compare_with_negative, &operator_position);
      }  /* if */
    }  /* if */
    op = which_binary_operator(save_token, operation_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position);
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_eq_operator */


static void scan_bit_operator(an_operand *operand_1,
                              an_operand *result)
/*
Scan the "&", "^", and "|" operators.  See sections 3.3.10, 3.3.11, and
3.3.12 of the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_type_ptr            result_type;
  an_expr_operator_kind op;
  a_boolean             processed = FALSE;
  int                   prec_level;

  db_enter(4, "scan_bit_operator");

  save_token = curr_token;
  switch (save_token) {
    case tok_ampersand: prec_level = PREC_AND;     break;
    case tok_excl_or:   prec_level = PREC_EXCL_OR; break;
    case tok_or:        prec_level = PREC_OR;      break;
#if CHECKING
    default: internal_error("scan_bit_operator: bad operator");
#endif /* CHECKING */
  }  /* switch */
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, prec_level, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral or enum. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    (void)check_integral_or_enum_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    (void)check_integral_or_enum_operand(&operand_2);
    adjust_operands_for_microsoft_int_long_bug(operand_1, &operand_2);
    result_type = determine_arithmetic_conversions(operand_1, &operand_2);
    change_binary_operand_types(result_type, operand_1, &operand_2);
    op = which_binary_operator(save_token, result_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position);
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_bit_operator */


static void potential_sequence_point_after_operand(an_operand *operand)
/*
There is a potential sequence point after the evaluation of the indicated
operand.  Commit all references other than those directly associated
with the operand.  In practice, there will almost never be any
unassociated references to flush at this point.  The references associated
with the operand are left mostly unchanged, because they can still be
updated if the operator is overloaded.  "Mostly unchanged" means that the
modifications on the list are recorded but kept on the list for further
updating.  If it turns out the operator is overloaded in this case,
there is no sequence point, but flushing the references at this point
is one of the valid interpretations, so it's okay. 
*/
{
  /* Flush the entries not directly associated with the operand. */
  flush_ref_entries_except(operand->ref_entries_list, (a_ref_entry_ptr)NULL);
  /* Record the modifications in the operand. */
  record_operand_modification_refs(operand);
}  /* potential_sequence_point_after_operand */


static void scan_logical_operator(an_operand *operand_1,
                                  an_operand *result)
/*
Scan the "&&" and "||" operators.  See sections 3.3.13 and 3.3.14 of the
standard.
*/
{
  an_expr_operator_kind op;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_boolean             operand_1_is_false = FALSE;
  a_host_large_integer  local_result;
  a_boolean             known_result       = FALSE;
  a_token_kind          save_token;
  a_type_ptr            result_type;
  a_boolean             processed = FALSE;
  a_boolean             might_be_overloaded = FALSE;
  int                   prec_level;
  a_boolean             operand_1_transformations_done = FALSE;
  a_boolean             saved_evaluated = curr_expr_is_evaluated();
  a_boolean             expr2_evaluated;
  a_boolean             saved_inside_conditional_expression =
                                     expr_stack->inside_conditional_expression;

  db_enter(4, "scan_logical_operator");

  save_token = curr_token;
  if (save_token == tok_and_and) {
    prec_level = PREC_AND_AND;
  } else {
#if CHECKING
    if (save_token != tok_or_or) {
      internal_error("scan_logical_operator: bad operator");
    }  /* if */
#endif /* CHECKING */
    prec_level = PREC_OR_OR;
  }  /* if */
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;
  /* There is a potential sequence point after the first operand. */
  potential_sequence_point_after_operand(operand_1);

  if (C_dialect == C_dialect_cplusplus &&
      opname_symbol_table[opname_kind_for_token[(int)save_token]] != NULL) {
    /* We are in C++ mode, and there is an operator function that overloads
       this operator. */
    might_be_overloaded = TRUE;
  }  /* if */

  /* Determine whether or not the second operand should be evaluated. */
  expr2_evaluated = saved_evaluated;
  if (saved_evaluated && !might_be_overloaded) {
    /* The operator is not overloaded, so it will have the built-in
       meaning.  Examine the first operand to see if it is a constant.
       If so, we can determine whether or not the second operand should be
       evaluated. */
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand_1->type)) {
      /* The first operand is a class in C++ mode.  We cannot convert it to
         an rvalue because a conversion function might be applied to it.
         However, we lose nothing by not doing this -- we know the first
         operand is not a constant. */
    } else {
      /* See if the first operand is a constant. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
      operand_1_transformations_done = TRUE;
      if (is_constant_operand(operand_1) &&
          /* The type of operand_1 has not been checked yet, so avoid
             problems. */
          is_scalar_type(operand_1->type) &&
          constant_bool_value_known_at_compile_time(
                                               &operand_1->variant.constant)) {
        operand_1_is_false = op_is_false_constant(operand_1);
        if (save_token == tok_and_and && operand_1_is_false) {
          /* 0 && something -- this always evaluates to a zero/false value. */
          local_result = 0;
          known_result = TRUE;
          expr2_evaluated = FALSE;
        } else if (save_token == tok_or_or && !operand_1_is_false) {
          /* non-zero || something -- this always evaluates to a value of
             1/true. */
          local_result = 1;
          known_result = TRUE;
          expr2_evaluated = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  expr_stack->evaluated = expr2_evaluated;
  expr_stack->inside_conditional_expression = TRUE;
  scan_expr(&operand_2, prec_level, EOPT_NO_OPTIONS);
  expr_stack->inside_conditional_expression =
                                           saved_inside_conditional_expression;
  /* Restore the evaluated flag as it was on entry. */
  expr_stack->evaluated = saved_evaluated;

  if (C_dialect == C_dialect_cplusplus &&
      (is_overloadable_type_operand(operand_1) ||
       is_overloadable_type_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    /* Note that we do not test might_be_overloaded here, because we want
       to go to the subroutine to look for conversions from class types
       to built-in types. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   operator_tok_seq_number,
                                   result, &processed);
  }  /* if */
  if (!processed && curr_expr_kind_is(ek_template_arg)) {
    /* Check for non-integral operations in a template argument expression. */
    check_for_bad_template_arg_operation(operand_1, &operand_2,
                                         &operator_position, result,
                                         &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be scalar. */
    if (!operand_1_transformations_done) {
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    }  /* if */
    (void)check_boolean_controlling_expr(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    (void)check_boolean_controlling_expr(&operand_2);
    result_type = boolean_result_type();
    if (!known_result ||
        /* In constant expressions we must always fold. */
        (!curr_expr_kind_is_const() &&
#if ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS
         /* Don't remove dead code that might contain destructions, because
            we don't want to run through the expression to find the destruction
            to unlink it. */
         curr_object_lifetime != NULL &&
         curr_object_lifetime->destructions != NULL
#else /* !ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS */
         /* When the first operand is constant and determines the result,
            but the second operand is not constant, retain the dead
            expression. */
         !is_constant_operand(&operand_2)
#endif /* ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS */
                                         )) {
      /* Make an expression. */
      op = which_binary_operator(save_token, result_type);
      do_binary_operation(op, operand_1, &operand_2, result_type, result,
                          &operator_position);
    } else {
      /* The expression evaluates to a constant. */
      make_integer_constant_operand(result, local_result);
      /* Cast if necessary (e.g., to bool). */
      cast_operand(result_type, result, /*check_cast_access=*/TRUE,
                   /*is_implicit_cast=*/TRUE, /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
      if (!is_constant_operand(&operand_2) ||
          operand_2.variant.constant.null_pointer_constant_ruled_out ||
          operand_1->variant.constant.null_pointer_constant_ruled_out) {
        /* The result is not a null pointer constant. */
        result->variant.constant.null_pointer_constant_ruled_out = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_logical_operator */


static void keep_enum_in_result_type(a_type_ptr op1_type,
                                     a_type_ptr op2_type,
                                     a_type_ptr *result_type)
/*
If both of the operands have the same enumerated type, keep that information
in the result type.  *result_type is set already, but if appropriate it
is modified to indicate that is affiliated with the enum type.
*/
{
  a_type_ptr op1_enum, op2_enum;

  /* In C++, enumeration constants have the same type as the enumeration, so
     this routine is not needed. */
  if (C_dialect != C_dialect_cplusplus) {
    op1_type = skip_typerefs(op1_type);
    op2_type = skip_typerefs(op2_type);
    if (is_integral_or_enum_type(op1_type) &&
        is_integral_or_enum_type(op2_type)) {
      op1_enum = underlying_enum_type(op1_type);
      op2_enum = underlying_enum_type(op2_type);
      if (op1_enum != NULL && same_entities(op1_enum, op2_enum)) {
        /* Both types are the same enum type, so keep the enum tag in
           the result type. */
        an_integer_kind result_kind;
        check_assertion_str(is_integral_or_enum_type(*result_type),
                        "keep_enum_in_result_type: bad result type for enums");
        result_kind = skip_typerefs(*result_type)->variant.integer.int_kind;
        if (result_kind == op1_type->variant.integer.int_kind) {
          /* Normal case -- the result type is the same integral type as
             the operand types.  Use the operand type instead because it
             includes the enum affiliation. */
          *result_type = op1_type;
        } else {
          /* The enum operands have types different than the result type,
             which means they are smaller than int and promote to the
             result type.  Find or create a version of the result type that
             includes the enum affiliation. */
          a_constant_ptr enum_con =
                             op1_enum->variant.integer.enum_info.constant_list;
          /* See if the type of the first enum constant is the right type. */
          if (enum_con != NULL &&
              enum_con->type->kind == (a_type_kind)tk_integer &&
              result_kind == enum_con->type->variant.integer.int_kind) {
            *result_type = enum_con->type;
          } else {
            /* Make the needed type. */
            a_type_ptr new_type = alloc_type((a_type_kind)tk_integer);
            *new_type = **result_type;
            new_type->variant.integer.enum_type = FALSE;
            new_type->variant.integer.enum_info.affiliated_type = op1_enum;
            *result_type = new_type;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* keep_enum_in_result_type */


static void process_boolean_controlling_expression(an_operand *result,
                                                   a_boolean  validate_only)
/*
*result is the controlling expression of an if/while/do-while/for statement,
or of a "?" operator.  Check that it has the right type.  Convert it from a
class type if necessary.  If validate_only is TRUE, do not modify the operand
except for standard operand transformations.
*/
{
  a_boolean processed = FALSE;
  a_boolean pointer_case, was_constant;

  /* Convert from a class type to bool or scalar/pointer-to-member if
     necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result->type) &&
      !validate_only) {
    a_builtin_type_kind_set type_kind_set;
    if (bool_is_keyword) {
      type_kind_set = BTK_BOOL;
    } else {
      type_kind_set = (a_builtin_type_kind_set)(BTK_INTEGRAL |
                                                BTK_ENUM |
                                                BTK_FLOATING |
                                                BTK_POINTER |
                                                BTK_PTR_TO_MEMBER);
    }  /* if */
    try_to_convert_class_operand_to_builtin_type(result, type_kind_set,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Do lvalue --> rvalue and other transformations for the non-class
       case. */
    do_operand_transformations(result, TOPT_NO_OPTIONS);
  }  /* if */
  /* Remember whether or not the expression has pointer type.  This is
     needed later, and the check standardizes the operation to a bool or
     integer result. */
  pointer_case = is_pointer_type(result->type) ||
                 is_ptr_to_member_type(result->type);
  was_constant = is_constant_operand(result);
  /* Check that the operand is scalar or a pointer to member.  Note that
     this is done even for the cases where a class type has been converted
     to such a type, because the subroutine does some additional checking
     and some normalization of the expression. */
  if (validate_boolean_controlling_expr(result, validate_only)) {
    /* Issue a remark if the expression is constant.  (Actually, if
       it WAS constant, because the address of an extern entity -- a
       constant -- converted to bool becomes an expression, because
       one can't tell at compile time whether the external's address
       is non-zero.)  The check is here instead of
       check_boolean_controlling_expr because we don't want to issue
       diagnostics for things like "i = 1&&2;".  Do not issue the error
       in constant expressions (which can happen only for conditional
       operators, i.e., "?", not for statements). */
    if (was_constant) {
      if (pointer_case) {
        /* A test of a constant address is always pretty suspicious. */
        pos_warning(ec_boolean_controlling_expr_is_constant,
                    &result->position);
      } else if (!curr_expr_kind_is_const()) {
        pos_remark(ec_boolean_controlling_expr_is_constant, &result->position);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* process_boolean_controlling_expression */


/*
Macro that returns TRUE if the given operand is an operand for a throw
expression.
*/
#define is_throw_operand(operand)                                     \
  (is_expression_operand(operand) &&                                  \
   (operand)->variant.expression->kind == (an_expr_node_kind)enk_throw)


static a_ref_entry_ptr merge_ref_lists(a_ref_entry_ptr list1,
                                       a_ref_entry_ptr list2)
/*
Return a pointer to a list of reference entries that is the concatenation
of list1 and list2.  The source lists and the destination list are linked
on the next_operand_ref field.
*/
{
  a_ref_entry_ptr merged_list;

  if (list1 == NULL) {
    merged_list = list2;
  } else if (list2 == NULL) {
    merged_list = list1;
  } else {
    /* Find the end of list1 and add list2 there. */
    merged_list = list1;
    while (list1->next_operand_ref != NULL) list1 = list1->next_operand_ref;
    list1->next_operand_ref = list2;
  }  /* if */
  return merged_list;
}  /* merge_ref_lists */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void adjust_void_operand_for_microsoft_void_vs_scalar_conditional(
                                                        an_operand *operand,
                                                        a_type_ptr result_type)
/*
Microsoft mode allows a void and a scalar operand to be supplied as the
second and third operands of the "?" operator.  "operand" is the void
operand of such a case, and result_type is the type of the other operand.
Turn "operand" into "(operand, (result_type)0)" so it will match the other
operand.
*/
{
  an_operand       orig_operand;
  a_constant       zero_constant;
  an_expr_node_ptr zero_node, void_node, comma_node;

  /* Save the operand's source position, etc. */
  orig_operand = *operand;
  /* Turn the void operand into (operand, (type)0) so its type matches
     that of the other operand. */
  make_zero_of_proper_type(result_type, &zero_constant);
  zero_node = alloc_node_for_constant(&zero_constant);
  void_node = make_node_from_operand(operand);
  void_node->next = zero_node;
  comma_node = make_operator_node((an_expr_operator_kind)eok_comma,
                                  result_type, void_node);
  make_expression_operand(comma_node, result_type, operand);
  restore_operand_details(operand, &orig_operand);
}  /* adjust_void_operand_for_microsoft_void_vs_scalar_conditional */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */


static a_boolean same_types_for_question_operator(an_operand *operand_2,
                                                  an_operand *operand_3)
/*
Return TRUE if the two indicated operands, which are the second and
third operands of a "?" operator, have the same type.
*/
{
  a_boolean  types_are_the_same;
  a_type_ptr type_2 = operand_2->type;
  a_type_ptr type_3 = operand_3->type;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_bugs && !is_class_struct_union_type(type_2)) {
    /* In Microsoft mode, cv-qualifiers are ignored in determining
       whether two non-class operands have the same type. */
    if ((is_qualified_type(type_2) && is_bit_field_operand(operand_2)) ||
        (is_qualified_type(type_3) && is_bit_field_operand(operand_3))) {
       /* We can't implement dropping of cv-qualifiers while keeping an
          lvalue for a bit field, so ignore those. */
    } else {
      type_2 = skip_typerefs(type_2);
      type_3 = skip_typerefs(type_3);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  types_are_the_same = types_are_compatible(type_2, type_3);
  return types_are_the_same;
}  /* same_types_for_question_operator */


static void scan_conditional_operator(an_operand *operand_1,
                                      an_operand *result)
/*
Scan the "?" operator.  See section 3.3.15 of the standard.
*/
{
  an_operand            operand_2;
  an_operand            *p_operand_2;
  an_operand            operand_3;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position     question_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean             operand_1_is_const = FALSE;
  a_boolean             operand_1_is_false = FALSE;
  a_boolean             result_is_an_lvalue = FALSE;
  a_boolean             err = FALSE, processed = FALSE;
  a_type_ptr            result_type, ptr_result_type, operation_type;
  a_boolean             operand_2_is_pointer, operand_3_is_pointer;
  a_type_ptr            type_pointed_to_2, type_pointed_to_3;
  a_type_ptr            unqual_type_pointed_to_2, unqual_type_pointed_to_3;
  a_type_ptr            operation_type_underlying_class;
  a_boolean             operand_2_is_ptr_to_member, operand_3_is_ptr_to_member;
  a_boolean             saved_evaluated = curr_expr_is_evaluated();
  a_boolean             expr2_evaluated, expr3_evaluated;
  a_boolean             types_are_the_same = FALSE;
  a_boolean             saved_inside_conditional_expression =
                                     expr_stack->inside_conditional_expression;
  a_boolean             binary_conditional;

  db_enter(4, "scan_conditional_operator");

#if EXTRA_SOURCE_POSITIONS_IN_IL
  question_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Skip the "?" token. */
  (void)get_token();

  /* Recognize the binary form "x ? : y" accepted in GNU C mode. */
  binary_conditional = (gcc_mode && curr_token == tok_colon);
  /* Check the first operand's type.  Do not transform the operand to a
     boolean value if we're dealing with the binary form, since the
     operand's original value is also needed in that case. */
  process_boolean_controlling_expression(operand_1,
                                         /*validate_only=*/binary_conditional);
  /* There is a sequence point after the first operand. */
  potential_sequence_point_after_operand(operand_1);

  expr2_evaluated = expr3_evaluated = saved_evaluated;
  /* If the first operand is a constant and if the constant is zero, evaluate
     the third operand only.  If the first operand is constant and is not a
     constant zero, evaluate the second operand only. */
  operand_1_is_const = is_constant_operand(operand_1) &&
                       constant_bool_value_known_at_compile_time(
                                                 &operand_1->variant.constant);
  if (operand_1_is_const) {
    operand_1_is_false = op_is_false_constant(operand_1);
    if (operand_1_is_false) {
      /* The first operand is a constant zero, so do not evaluate the second
         operand. */
      expr2_evaluated = FALSE;
    } else {
      /* The first operand is a constant non-zero, so do not evaluate the
         third operand. */
      expr3_evaluated = FALSE;
    }  /* if */
  }  /* if */

  if (!binary_conditional) {
    /* Scan the second operand.   Evaluate the expression if the first
       operand is non-constant or a non-zero constant, and if we are currently
       evaluating expressions. */
    expr_stack->nested_construct_depth++;
    expr_stack->evaluated = expr2_evaluated;
    expr_stack->inside_conditional_expression = TRUE;
    scan_expr(&operand_2, PREC_LOWEST, EOPT_NO_OPTIONS);
    expr_stack->inside_conditional_expression =
                                           saved_inside_conditional_expression;
    expr_stack->evaluated = saved_evaluated;
    expr_stack->nested_construct_depth--;
  } else {
    /* In the binary form, the second operand is omitted and instead the
       value of the first operand is used. */
    operand_2 = *operand_1;
  }  /* if */
  p_operand_2 = binary_conditional ? (an_operand*)NULL : &operand_2;

  /* Save the position of the (expected) colon. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (!required_token(tok_colon, ec_exp_colon)) {
    /* The colon is missing. */
    make_error_operand(result);
    goto error_exit;
  }  /* if */

  /* Scan the third operand.  Evaluate the expression if the first operand
     is non-constant or a zero constant, and if we are currently evaluating
     expressions. */
  expr_stack->evaluated = expr3_evaluated;
  expr_stack->inside_conditional_expression = TRUE;
  /* In C++, the 3rd operand is an assignment-expression (this was changed
     after the ARM) to allow things like "a ? i=1 : j=2". */
  scan_expr(&operand_3, (C_dialect != C_dialect_cplusplus ||
                         any_cfront_mode()) ? PREC_QUEST_MARK :
                                              PREC_ASSIGNMENT,
                         EOPT_NO_OPTIONS);
  expr_stack->inside_conditional_expression =
                                           saved_inside_conditional_expression;
  expr_stack->evaluated = saved_evaluated;

  /* Check the second and third operand types. */
  if (!C_mode()) {
    /* Checks specific to C++ mode: */
    types_are_the_same = same_types_for_question_operator(&operand_2,
                                                          &operand_3);
    if (is_template_dependent_context() &&
        (is_template_dependent_type(operand_1->type) ||
         is_template_dependent_type(operand_2.type) ||
         is_template_dependent_type(operand_3.type))) {
      /* If either operand has a template parameter type, we cannot
         check the operand types.  Just produce an expression with
         a generic operator. */
      template_question_operation(operand_1, &operand_2, &operand_3,
                                  result);
      processed = TRUE;
    } else if (curr_expr_kind_is(ek_template_arg) &&
               (is_bad_type_for_template_arg_operand(operand_1->type) ||
                is_bad_type_for_template_arg_operand(operand_2.type) ||
                is_bad_type_for_template_arg_operand(operand_3.type))) {
      /* Non-integral operations are not allowed in a template argument. */
      diagnose_bad_template_arg_operation(&operator_position);
      make_error_operand(result);
      operand_will_not_be_used_because_of_error(operand_1);
      operand_will_not_be_used_because_of_error(&operand_2);
      operand_will_not_be_used_because_of_error(&operand_3);
      err = TRUE;
    } else if (is_void_type(operand_2.type) ||
               is_void_type(operand_3.type)) {
      /* If either operand has type void, we do not look for conversions
         to or from class types. */
    } else if (types_are_the_same) {
      /* If the types are the same, we do not look for conversions to
         or from class types. */
    } else if (is_class_struct_union_type(operand_2.type) ||
               is_class_struct_union_type(operand_3.type)) {
      /* One or both of the operands has a class type, and they do not have
         the same type.  (One could have a cv-qualified version of the type
         of the other.)  Try converting each operand to the type of the
         other.  See 5.16 paragraph 3 in the C++ standard. */
      a_conv_descr conv_2_to_3, conv_3_to_2;
      a_boolean    conv_2_to_3_possible, conv_3_to_2_possible;
      a_boolean    ambig_2_to_3, ambig_3_to_2;
      conv_2_to_3_possible =
                       conditional_operator_conversion_possible(&operand_2,
                                                                &operand_3,
                                                                &conv_2_to_3,
                                                                &ambig_2_to_3);
      conv_3_to_2_possible =
                       conditional_operator_conversion_possible(&operand_3,
                                                                &operand_2,
                                                                &conv_3_to_2,
                                                                &ambig_3_to_2);
      if (microsoft_bugs && conv_2_to_3_possible && conv_3_to_2_possible &&
          !ambig_2_to_3 && !ambig_3_to_2) {
        /* The Microsoft compiler prefers a conversion using a constructor
           to one that uses a conversion function. */
        a_boolean conv_func_2_to_3 = (conv_2_to_3.routine != NULL &&
                                      conv_2_to_3.routine->special_kind ==
                                      (a_special_function_kind)sfk_conversion);
        a_boolean conv_func_3_to_2 = (conv_3_to_2.routine != NULL &&
                                      conv_3_to_2.routine->special_kind ==
                                      (a_special_function_kind)sfk_conversion);
        if (conv_func_2_to_3 && !conv_func_3_to_2) {
          conv_2_to_3_possible = FALSE;
        } else if (conv_func_3_to_2 && !conv_func_2_to_3) {
          conv_3_to_2_possible = FALSE;
        }  /* if */
      }  /* if */
      expr_stack->inside_conditional_expression = TRUE;
      if (conv_2_to_3_possible && conv_3_to_2_possible) {
        /* Each operand can be converted to the other, so the operation
           is ambiguous. */
        pos_ty2_error(ec_ambiguous_question_operator, &operator_position,
                      operand_2.type, operand_3.type);
        err = TRUE;
      } else if (conv_2_to_3_possible || conv_3_to_2_possible) {
        if (conv_2_to_3_possible) {
          /* Operand 2 can be converted to the type of operand 3.  Do so. */
          if (!ambig_2_to_3) {
            user_convert_operand(&operand_2, operand_3.type, &conv_2_to_3,
                                 (a_conv_descr *)NULL,
                                 /*force_temp_for_class_bitwise_copy=*/FALSE,
                                 /*is_explicit_cast=*/FALSE);
          } else {
            /* The conversion is ambiguous.  Do the test again and this
               time issue an error. */
            conv_2_to_3_possible =
                   conditional_operator_conversion_possible(&operand_2,
                                                            &operand_3,
                                                            &conv_2_to_3,
                                                            (a_boolean *)NULL);
            check_assertion(conv_2_to_3_possible);
          }  /* if */
        } else {
          /* Operand 3 can be converted to the type of operand 2.  Do so. */
          if (!ambig_3_to_2) {
            user_convert_operand(&operand_3, operand_2.type, &conv_3_to_2,
                                 (a_conv_descr *)NULL,
                                 /*force_temp_for_class_bitwise_copy=*/FALSE,
                                 /*is_explicit_cast=*/FALSE);
          } else {
            /* The conversion is ambiguous.  Do the test again and this
               time issue an error. */
            conv_3_to_2_possible =
                   conditional_operator_conversion_possible(&operand_3,
                                                            &operand_2,
                                                            &conv_3_to_2,
                                                            (a_boolean *)NULL);
            check_assertion(conv_3_to_2_possible);
          }  /* if */
        }  /* if */
        /* Determine if the types are the same after any conversions.*/
        types_are_the_same = same_types_for_question_operator(&operand_2,
                                                              &operand_3);
      } else {
        /* The operands do not have the same type, at least one of them
           has a class type, and there is no way to convert one to the
           type of the other.  Look for conversions to built-in types. */
        check_for_operator_overloading((an_opname_kind)onk_question,
                                       /*unary_operator=*/FALSE,
                                       /*must_be_member_function=*/FALSE,
                                       /*try_conversions=*/TRUE,
                                       /*has_predef_meaning=*/TRUE,
                                       &operand_2, &operand_3,
                                       &operator_position,
                                       operator_tok_seq_number,
                                       result, &processed);
        /* processed TRUE means an error has been detected. */
        if (processed) {
          err = TRUE;
        } else {
          /* Determine if the types are the same after any conversions.*/
          types_are_the_same = same_types_for_question_operator(&operand_2,
                                                                &operand_3);
        }  /* if */
      }  /* if */
      expr_stack->inside_conditional_expression =
                                           saved_inside_conditional_expression;
    }  /* if */
  }  /* if */
  if (!processed && !err) {
    if (!C_mode() && types_are_the_same &&
        is_a_cplusplus_lvalue(&operand_2) &&
        is_a_cplusplus_lvalue(&operand_3)) {
      /* In C++, if the second and third operands have the same type and
         they are lvalues, the result is also an lvalue. */
      result_is_an_lvalue = TRUE;
    } else {
      /* Do lvalue --> rvalue, array --> pointer, and function --> pointer
         transformations. */
      expr_stack->evaluated = expr2_evaluated;
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      expr_stack->evaluated = expr3_evaluated;
      do_operand_transformations(&operand_3, TOPT_NO_OPTIONS);
      expr_stack->evaluated = saved_evaluated;
      /* See if the types are the same in C++ mode after the
         transformations. */
      if (!C_mode()) {
        types_are_the_same = same_types_for_question_operator(&operand_2,
                                                              &operand_3);
      }  /* if */
    }  /* if */
    result_type = operand_2.type;  /* Assume. */
    if (!C_mode() && types_are_the_same) {
      /* If the types are the same in C++ mode, no further checking of types
         is needed. */
      /* If either operand has an error type, make sure the result type is
         an error type. */
      if (is_error_type(operand_3.type)) {
        result_type = operand_3.type;
      } else if (microsoft_bugs &&
                 !is_class_struct_union_type(result_type) &&
                 !is_error_type(result_type)) {
        /* In Microsoft mode, the cv-qualifiers are dropped on non-class
           operands. */
        if ((is_qualified_type(operand_2.type) &&
             is_bit_field_operand(&operand_2)) ||
            (is_qualified_type(operand_3.type) &&
             is_bit_field_operand(&operand_3))) {
          /* We can't do this on bit-field operands, however. */
        } else if (value_of_constant_var_lvalue_operand(&operand_2) != NULL ||
                   value_of_constant_var_lvalue_operand(&operand_3) != NULL) {
          /* This doesn't apply for constant-valued variables -- they are
             always treated as rvalues in the Microsoft compiler. */
        } else {
          result_type = make_unqualified_type(result_type);
          microsoft_lvalue_cv_qual_adjustment(&operand_2, result_type);
          microsoft_lvalue_cv_qual_adjustment(&operand_3, result_type);
        }  /* if */
      }  /* if */
    } else if (is_throw_operand(&operand_2)) {
      /* The second operand is a throw expression and the third is not
         (because if they both were, they would have the same types),
         so use the type of the third. */
      result_type = operand_3.type;
    } else if (is_throw_operand(&operand_3)) {
      /* The third operand is a throw expression and the second is not,
         so use the type of the second. */
      /* result_type = operand_2.type; -- already set. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && !curr_expr_kind_is_const() &&
               is_void_type(operand_2.type) &&
               is_scalar_type(operand_3.type)) {
      /* Microsoft mode allows void operands mixed with scalar.
         The second operand is a void expression and the third is not
         (because if they both were, they would have the same types),
         so use the type of the third. */
      result_type = operand_3.type;
      pos_ty2_warning(ec_incompatible_operands, &operator_position,
                      operand_2.type, operand_3.type);
      adjust_void_operand_for_microsoft_void_vs_scalar_conditional(&operand_2,
                                                                  result_type);
    } else if (microsoft_mode && !curr_expr_kind_is_const() &&
               is_void_type(operand_3.type) &&
               is_scalar_type(operand_2.type)) {
      /* Microsoft mode allows void operands mixed with scalar.
         The third operand is a void expression and the second is not,
         so use the type of the second. */
      /* result_type = operand_2.type; -- already set. */
      pos_ty2_warning(ec_incompatible_operands, &operator_position,
                      operand_2.type, operand_3.type);
      adjust_void_operand_for_microsoft_void_vs_scalar_conditional(&operand_3,
                                                                  result_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (gcc_mode &&
               (is_void_type(operand_2.type) ||
                is_void_type(operand_3.type))) {
      /* gcc allows mixed void/non-void operands.  The result type is void. */
      result_type = void_type();
      if (!is_void_type(operand_2.type)) {
        cast_operand_to_void(&operand_2, result_type);
      }  /* if */
      if (!is_void_type(operand_3.type)) {
        cast_operand_to_void(&operand_3, result_type);
      }  /* if */
    } else {
      operand_2_is_pointer = is_pointer_type(operand_2.type);
      operand_3_is_pointer = is_pointer_type(operand_3.type);
      if (C_dialect == C_dialect_cplusplus) {
        operand_2_is_ptr_to_member = is_ptr_to_member_type(operand_2.type);
        operand_3_is_ptr_to_member = is_ptr_to_member_type(operand_3.type);
      } else {
        operand_2_is_ptr_to_member = operand_3_is_ptr_to_member = FALSE;
      }  /* if */
      if (operand_2_is_pointer || operand_3_is_pointer) {
        /* At least one of the operands is a pointer.  See if the operands are
           compatible.  In C, the operands must be pointers to qualified or
           unqualified versions of compatible types (i.e., object, incomplete,
           or function types), and null pointer constants and "void *" pointers
           are specially handled (ANSI C 3.3.15).  Ditto in C++ (ARM 5.16). */
        if (check_compatibility_of_pointer_operands(
                           &operand_2, &operand_3, &operator_position,
                           /*pointer_normalization_standard_in_C=*/TRUE,
                           /*pointers_to_functions_standard_in_C=*/TRUE,
                           /*pointers_to_incomplete_standard_in_C=*/TRUE,
                           /*mixed_object_and_incomplete_standard_in_C=*/TRUE,
                           &operation_type)) {
          /* The operands are compatible.  Determine the result type.  Usually,
             it's the operation type just determined, but it can be a different
             type (a composite) if the two operands are pointers to compatible
             but not identical types. */
          if (!operand_2_is_pointer || !operand_3_is_pointer) {
            /* One of the operands is not a pointer (e.g., it's a null pointer
               constant).  Use the operation type. */
            result_type = operation_type;
          } else {
            /* Both operands are pointers. */
            type_pointed_to_2 = type_pointed_to(operand_2.type);
            unqual_type_pointed_to_2 = skip_typerefs(type_pointed_to_2);
            type_pointed_to_3 = type_pointed_to(operand_3.type);
            unqual_type_pointed_to_3 = skip_typerefs(type_pointed_to_3);
            if (types_are_compatible(unqual_type_pointed_to_2,
                                     unqual_type_pointed_to_3)) {
              /* The pointers point to compatible types, so form a composite
                 type. */
              ptr_result_type = composite_type(unqual_type_pointed_to_2,
                                               unqual_type_pointed_to_3);
            } else {
              /* The pointers do not point to compatible types (e.g., one
                 was "void *").  Use the operation type with qualifiers
                 rebuilt below. */
              ptr_result_type = type_pointed_to(operation_type);
              ptr_result_type = skip_typerefs(ptr_result_type);
            }  /* if */
            /* Add to the type pointed to any qualifiers present on either of
               the operand types pointed to. */
            ptr_result_type =
                      type_plus_qualifiers_from_second_type(ptr_result_type,
                                                            type_pointed_to_2);
            ptr_result_type =
                      type_plus_qualifiers_from_second_type(ptr_result_type,
                                                            type_pointed_to_3);
            /* The result type is an unqualified pointer to the
               properly-qualified underlying type. */
            result_type = make_pointer_type(ptr_result_type);
          }  /* if */
        }  else {
          /* The operands are incompatible.  (An error has already been
             issued.) */
          err = TRUE;
        }  /* if */
      } else if (operand_2_is_ptr_to_member || operand_3_is_ptr_to_member) {
        /* At least one of the operands is a pointer-to-member.  See if the
           operands are compatible. */
        if (check_ptr_to_member_operands_for_compatibility(
                                    &operand_2, &operand_3, &operator_position,
                                    &operation_type)) {
          /* The operands are compatible.  Determine the result type.  Usually,
             it's the operation type just determined, but it can be a different
             type (a composite) if the two operands are pointers to compatible
             but not identical types. */
          if (!operand_2_is_ptr_to_member || !operand_3_is_ptr_to_member) {
            /* One of the operands is not a pointer-to-member (e.g., it's a
               null pointer constant).  Use the operation type. */
            result_type = operation_type;
          } else {
            /* Both operands are pointers-to-members, of compatible underlying
               type if you ignore the type qualifiers.  (There is no
               equivalent of "void *" for pointers-to-members.) */
            type_pointed_to_2 = pm_member_type(operand_2.type);
            unqual_type_pointed_to_2 = skip_typerefs(type_pointed_to_2);
            type_pointed_to_3 = pm_member_type(operand_3.type);
            unqual_type_pointed_to_3 = skip_typerefs(type_pointed_to_3);
            /* If the member types are function types, make their "this"
               parameter types have the same underlying class. */
            operation_type_underlying_class = pm_class_type(operation_type);
            unqual_type_pointed_to_2 =
                          related_member_type(unqual_type_pointed_to_2,
                                              operation_type_underlying_class);
            unqual_type_pointed_to_3 =
                          related_member_type(unqual_type_pointed_to_3,
                                              operation_type_underlying_class);
            /* Form a composite of the member types. */
            ptr_result_type = composite_type(unqual_type_pointed_to_2,
                                             unqual_type_pointed_to_3);
            /* Add to the type pointed to any qualifiers present on either of
               the operand types pointed to. */
            ptr_result_type =
                      type_plus_qualifiers_from_second_type(ptr_result_type,
                                                            type_pointed_to_2);
            ptr_result_type =
                      type_plus_qualifiers_from_second_type(ptr_result_type,
                                                            type_pointed_to_3);
            /* The result type is an unqualified pointer-to-member to the
               properly-qualified underlying type. */
            result_type = ptr_to_member_type(ptr_result_type,
                                             operation_type_underlying_class);
          }  /* if */
        }  else {
          /* The operands are incompatible. */
          err = TRUE;
        }  /* if */
      } else if (is_arithmetic_or_enum_type(operand_2.type)) {
        /* Both operands should be arithmetic or enum. */
        (void)check_arithmetic_or_enum_operand(&operand_3);
        /* The Microsoft Visual C++ compiler treats "x ? long_expr : int_expr"
           and "x ? int_expr : long_expr" as having result type int. */
        adjust_operands_for_microsoft_int_long_bug(&operand_2, &operand_3);
        adjust_operands_for_microsoft_int_long_bug(&operand_3, &operand_2);
        result_type = determine_arithmetic_conversions(&operand_2, &operand_3);
        /* If both operands have the same enumerated type, keep that
           information in the result.  The "?" operator is unusual in that
           regard. */
        keep_enum_in_result_type(operand_2.type, operand_3.type, &result_type);
      } else if (is_class_struct_union_type(operand_2.type) ||
                 is_void_type(operand_2.type)) {
        /* The second operand has class, struct, union, or void type; the
           third operand must have a compatible type.  C struct/union cases
           are recognized here.  C++ class cases are handled above; this
           code deals only with error cases in C++. */
        if (!types_are_compatible(operand_2.type, operand_3.type)) {
          pos_ty2_error(ec_incompatible_operands, &operator_position,
                        operand_2.type, operand_3.type);
          err = TRUE;
        }  /* if */
      } else if (is_error_type(operand_2.type) ||
                 is_error_type(operand_3.type)) {
        /* One or both of the operands have an error type. */
        err = TRUE;
      } else {
        /* Incompatible operands. */
        pos_ty2_error(ec_incompatible_operands, &operator_position,
                      operand_2.type, operand_3.type);
        err = TRUE;
      }  /* if */
      /* Cast operands 2 and 3 to the result type if necessary. */
      if (!err) {
        /* The GNU C binary conditional case (without a "middle operand")
           may also arrive here.  In that case, p_operand_2 is NULL, while
           operand_2 is a copy of the controlling operand. */
        change_binary_operand_types(result_type, p_operand_2, &operand_3);
      }  /* if */
    }  /* if */
  }  /* if */

  if (err || is_error_operand(operand_1)) {
    make_error_operand(result);
  } else if (processed) {
    /* Already processed. */
  } else {
    /* Build the expression.  p_operand_2 is NULL for the GNU C extension
       of an omitted "middle operand" (eok_binary_question). */
    do_question_operation(operand_1, p_operand_2, &operand_3, result_type,
                          result_is_an_lvalue, result);
    if (result_is_an_lvalue) {
      /* The result is an lvalue, so its reference list is the union
         of the operand 2 and operand 3 reference lists. */
      result->ref_entries_list = merge_ref_lists(operand_2.ref_entries_list,
                                                 operand_3.ref_entries_list);
    }  /* if */
  }  /* if */
  if (string_literals_are_const && !strict_ansi_mode) {
    /* As an extension, allow a "?" operator where the second and third
       operands are string literals to be eligible for the deprecated
       conversion to "char *".  This allows things like
         char *p = x ? "abc" : "def";
    */
    result->is_simple_string_literal = (operand_2.is_simple_string_literal ||
                                        operand_3.is_simple_string_literal);
  }  /* if */
error_exit:

  set_operand_position(result, &operand_1->position, &operand_3.end_position,
                       &question_position);
  db_exit();
}  /* scan_conditional_operator */

#if ASSIGNMENT_TO_THIS_ALLOWED

static a_boolean check_assignment_to_this_pointer(an_operand *operand)
/*
If operand is an rvalue for the "this" parameter of the current routine,
issue a warning and change it to an lvalue for the "this" parameter.
This is used for checking/allowing assignment to "this" -- an anachronism.
*/
{
  a_boolean        is_this = FALSE;
  a_variable_ptr   this_var, operand_var;
  an_expr_node_ptr operand_expr;

  if (is_an_rvalue(operand) && is_expression_operand(operand)) {
    operand_expr = operand->variant.expression;
    if (is_variable_node(operand_expr)) {
      /* The operand is an rvalue that is the value of a simple variable. */
      operand_var = operand_expr->variant.variable;
      if (variable_this_exists(&this_var)) {
        /* There is a current "this" parameter.  See if it matches the
           variable in the operand. */
        if (this_var == operand_var) {
          /* Yes.  Issue an anachronism diagnostic and change the operand
             to an lvalue for the "this" variable. */
          is_this = TRUE;
          /* Assignment to "this" is not allowed if exceptions are enabled.
             For one thing, the code in IL lowering does not know how to
             build the right region table if there are several assignments
             to "this" in one constructor. */
          pos_diagnostic(exceptions_enabled ? es_error :
                                              anachronism_error_severity,
                         ec_assignment_to_this, &operand->position);
          make_lvalue_variable_operand(this_var, operand,
                                       operand->ref_entries_list,
                                       /*record_expr=*/TRUE);
          current_routine_entry()->assignment_to_this_done = TRUE;
          this_var->param_value_has_been_changed = TRUE;
          if (exceptions_enabled &&
              scope_stack[decl_scope_level].within_try_block) {
            /* Mark the this variable as modified within a try block. */
            this_var->modified_within_try_block = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_this;
}  /* check_assignment_to_this_pointer */

#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

static void process_microsoft_null_pointer_constant_bug(an_operand *operand,
                                                        a_type_ptr dest_type)
/*
Check for a Microsoft C mode bug that allows the expression (void)0 (sic;
not (void *)0) to be used as a null pointer constant in some contexts.
operand is the source operand, and dest_type is the destination type of
an initialization or assignment.  If source_operand is (void)0 and dest_type
is a pointer type, change source_operand to a simple 0 so it will be
accepted as a null pointer constant.
*/
{
  if (microsoft_bugs && C_mode() &&
      is_void_type(operand->type) &&
      is_pointer_type(dest_type)) {
    if (is_expression_operand(operand)) {
      an_expr_node_ptr expr = operand->variant.expression;
      if (is_operation_node(expr) &&
          expr->variant.operation.kind == (an_expr_operator_kind)eok_cast) {
        expr = expr->variant.operation.operands;
        if (is_constant_node(expr) &&
            is_null_pointer_constant(expr->variant.constant)) {
          /* The expression is (void)0.  Replace it by 0. */
          an_operand orig_operand;
          orig_operand = *operand;
          make_constant_operand(expr->variant.constant, operand);
          restore_operand_details(operand, &orig_operand);
          pos_ty2_warning(ec_bad_initializer_type, &operand->position,
                          orig_operand.type, dest_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* process_microsoft_null_pointer_constant_bug */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void scan_simple_assignment_operator(an_operand *operand_1,
                                            an_operand *result)
/*
Scan the simple assignment operator ("=").  See section 3.3.16 of the standard.
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_token_sequence_number
                    operator_tok_seq_number;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         has_predef_meaning;
  a_type_ptr        orig_result_type, result_type;

  db_enter(4, "scan_simple_assignment_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  if (curr_expr_kind_is_const()) {
    /* Assignment operation not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();

  scan_expr(&operand_2, PREC_ASSIGNMENT, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    operand_will_not_be_used_because_of_error(&operand_2);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_property_ref_operand(operand_1)) {
    /* The operand is a field selection for a field declared with the
       Microsoft extension __declspec(property(...)).  Rewrite it as
       a call of the "put" function for the field. */
    rewrite_property_field_reference(operand_1, &operand_2);
    *result = *operand_1;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_overloadable_type_operand(operand_1) ||
         is_overloadable_type_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      has_predef_meaning = TRUE;
      if (is_class_struct_union_type(operand_1->type)) {
        /* Instantiate the type if it is a template class.  This ensures that
           the operator= function is declared. */
        complete_type_is_needed(operand_1->type);
        /* Defined C++ classes will always have a generated operator=.
           For incomplete classes, assume a predefined meaning to get clearer
           error messages. */
        has_predef_meaning = is_incomplete_type(operand_1->type);
        if (any_cfront_mode()) {
          /* In cfront mode, an operator= is not generated in every case. */
          if (symbol_supplement_for_class(operand_1->type)->
                                          assignment_by_bitwise_copy_allowed) {
            has_predef_meaning = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      check_for_operator_overloading((an_opname_kind)onk_assign,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*try_conversions=*/FALSE,
                                     has_predef_meaning,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      an_expr_operator_kind  op;
      /* Non-operator-function cases, including all C cases. */
      do_operand_transformations(operand_1,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
#if ASSIGNMENT_TO_THIS_ALLOWED
      if (C_dialect == C_dialect_cplusplus &&
          is_an_rvalue(operand_1) &&  /* For speed. */
          check_assignment_to_this_pointer(operand_1)) {
        /* Anachronism -- assigning to the "this" pointer. */
        /* The subroutine changes operand_1 to the proper lvalue. */
      } else {
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
        if (check_modifiable_lvalue_operand(operand_1)) {
          modifying_lvalue(operand_1, /*value_used=*/FALSE);
        }  /* if */
#if ASSIGNMENT_TO_THIS_ALLOWED
      }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
      /* The type of the assignment is the destination type with qualifiers
         dropped as appropriate. */
      orig_result_type = operand_1->type;
      result_type = rvalue_type(orig_result_type);
      op = which_binary_operator(tok_assign, result_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Check for a bug related to null pointer constants in Microsoft C
         mode. */
      process_microsoft_null_pointer_constant_bug(&operand_2, result_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* do_operand_transformations is not done in the second operand,
         because the processing for that is done in the conversion stuff. */
      prep_assignment_operand(&operand_2, result_type,
                              ec_incompatible_assignment_operands,
                              &operator_position);
      build_binary_result_operand(operand_1, &operand_2, op,
                                  result_type, result);
      /* In C++, assignment operators return lvalues. */
      if (C_dialect == C_dialect_cplusplus) {
        change_assignment_result_to_lvalue(result, operand_1,
                                           orig_result_type);
      }  /* if */
    }  /* if */
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_simple_assignment_operator */


static void scan_compound_assignment_operator(an_operand *operand_1,
                                              an_operand *result)
/*
Scan the compound assignment operators (*= /= %= += -= <<= >>= &= ^= |=).
See section 3.3.16 of the standard.
*/
{
  a_token_kind          save_token, operator_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_token_sequence_number
                        operator_tok_seq_number;
  a_boolean             err               = FALSE, processed = FALSE;
  a_type_ptr            orig_result_type, result_type;
  a_type_ptr            operation_type;
  a_boolean             pointer_add_sub   = FALSE;
  a_boolean             property_ref_case = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_operand            operand_1_clone;
  a_boolean             operand_1_clone_unused = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_boolean             imaginary_arithmetic = FALSE;

  db_enter(4, "scan_compound_assignment_operator");

  /* Save the operator. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;
  operator_token = save_token;

  if (curr_expr_kind_is_const()) {
    /* Assignment operation not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &operator_position);
    err = TRUE;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  property_ref_case = is_property_ref_operand(operand_1);
  if (property_ref_case && !err) {
    /* The left operand is a reference to a field declared with the Microsoft
       C++ extension __declspec(property(...)).  The fetch of the field will
       be made via a call of a "get" function, and the store will be made
       via a call of a "put" function. */
    /* The operation gets performed as the corresponding non-assignment
       operation, e.g., "+=" becomes "+".  This is used for building the
       IL operation and for overload resolution. */
    switch (save_token) {
      case tok_times_assign:
        operator_token = tok_star;
        break;
      case tok_divide_assign:
        operator_token = tok_divide;
        break;
      case tok_plus_assign:
        operator_token = tok_plus;
        break;
      case tok_minus_assign:
        operator_token = tok_minus;
        break;
      case tok_remainder_assign:
        operator_token = tok_remainder;
        break;
      case tok_shift_left_assign:
        operator_token = tok_shift_left;
        break;
      case tok_shift_right_assign:
        operator_token = tok_shift_right;
        break;
      case tok_and_assign:
        operator_token = tok_ampersand;
        break;
      case tok_excl_or_assign:
        operator_token = tok_excl_or;
        break;
      case tok_or_assign:
        operator_token = tok_or;
        break;
      default:
        unexpected_condition_str(
                               "scan_compound_assignment_operator: bad token");
    }  /* switch */
    /* Make a clone of operand_1, to be used in the store. */
    clone_operand(operand_1, &operand_1_clone);
    operand_1_clone_unused = TRUE;
    /* Transform the left operand to a call of the appropriate "get"
       function. */
    rewrite_property_field_reference(operand_1, (an_operand *)NULL);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_ASSIGNMENT, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    operand_will_not_be_used_because_of_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_overloadable_type_operand(operand_1) ||
         is_overloadable_type_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[
                                                          (int)operator_token],
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      if (!property_ref_case) { /*lint !e774*/
        do_operand_transformations(operand_1,
                                   TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
        if (!C_mode() && is_enum_type(operand_1->type)) {
          /* Enum types are not allowed (because the enum promotes to integer
             for the operation, and then can't get back to enum). */
          if (allow_anachronisms) {
            pos_diagnostic(anachronism_error_severity,
                           ec_mixed_enum_type_anachronism,
                           &operand_1->position);
          } else {
            error_in_operand(ec_enum_type_not_allowed, operand_1);
          }  /* if */
        }  /* if */
        if (check_modifiable_lvalue_operand(operand_1)) {
          modifying_lvalue(operand_1, /*value_used=*/TRUE);
        }  /* if */
      }  /* if */
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      /* Check the operand types. */
      switch (save_token) {
        case tok_times_assign:
        case tok_divide_assign:
          (void)check_arithmetic_or_enum_operand(operand_1);
          (void)check_arithmetic_or_enum_operand(&operand_2);
          break;
        case tok_plus_assign:
        case tok_minus_assign:
          if (is_arithmetic_or_enum_type(operand_1->type)) {
            /* If the first operand is arithmetic or enum, the second must
               be also. */
            (void)check_arithmetic_or_enum_operand(&operand_2);
          } else {
            a_boolean  nonobject_pointer =
                    (gcc_mode &&
                     is_pointer_type(operand_1->type) &&
                     (is_void_type(type_pointed_to(operand_1->type)) ||
                      is_function_type(type_pointed_to(operand_1->type))));
            if (nonobject_pointer ||
                check_object_pointer_operand(
                              operand_1,
                               enum_type_is_integral ?
                                 ec_expr_not_scalar :
                                 ec_expr_not_arithmetic_or_enum_or_pointer)) {
              /* The first operand is a pointer, so the second one must be
                 integral or enum. */
              if (check_integral_or_enum_operand(&operand_2)) {
                if (nonobject_pointer) {
                  /* Some versions of GNU C accept arithmetic on void and
                     function pointers.  Issue a warning in any case. */
                  pos_warning(ec_nonobject_pointer_arithmetic,
                              &operator_position);
                }  /* if */
                pointer_add_sub = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          break;
        case tok_remainder_assign:
        case tok_shift_left_assign:
        case tok_shift_right_assign:
        case tok_and_assign:
        case tok_excl_or_assign:
        case tok_or_assign:
          (void)check_integral_or_enum_operand(operand_1);
          (void)check_integral_or_enum_operand(&operand_2);
          break;
#if CHECKING
        default:
          internal_error(
                 "scan_compound_assignment_operator: bad assignment operator");
#endif /* CHECKING */
      }  /* switch */

      if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
        make_error_operand(result);
      } else {
        an_expr_operator_kind op;        
        orig_result_type = operand_1->type;
        result_type = rvalue_type(orig_result_type);
        if (pointer_add_sub) {
          /* For pointer += or -=, integral promotions are not done, and
             the operation type is the first operand's type.  This is
             like the processing for pointer + integer and
             pointer - integer. */
          operation_type = operand_1->type;
        } else if (save_token == tok_shift_left_assign ||
                   save_token == tok_shift_right_assign) {
          /* <<= and >>=. */
          if (C_dialect == C_dialect_pcc) {
            /* In K&R first edition (see appendix A, section 7.5), the shift
               operators << and >> "perform the usual arithmetic conversions
               on their operands, each of which must be integral.  Then the
               right operand is converted to int; the type of the result is
               that of the left operand."  This has the effect that a "long"
               shift count will force the shift to be done as long. */
            operation_type = determine_arithmetic_conversions(operand_1,
                                                              &operand_2);
            cast_operand(integer_type((an_integer_kind)ik_int), &operand_2,
                         /*check_cast_access=*/TRUE,
                         /*is_implicit_cast=*/TRUE,
                         /*is_reinterpret_cast=*/FALSE,
                         /*reinterpret_semantics=*/FALSE);
          } else {
            /* Not pcc mode. */
            /* These operations do integral promotions instead of the usual
               arithmetic conversions. */
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (property_ref_case) promote_operand(operand_1);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            operation_type = operand_1->type;
            promote_operand(&operand_2);
          }  /* if */
        } else {
          /* Normal case. */
#if C99_IL_EXTENSIONS_SUPPORTED
          /* Check for cases involving imaginary types that do not fall out
             of the normal usual arithmetic conversion rules. */
          if (c99_mode &&
              determine_imaginary_operation_type(save_token,
                                                 operand_1, &operand_2,
                                                 &operation_type, &op)) {
            imaginary_arithmetic = TRUE;
          } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          /* Do not insert code here. */
          {
            operation_type = determine_arithmetic_conversions(operand_1,
                                                              &operand_2);
            cast_operand(operation_type, &operand_2,
                         /*check_cast_access=*/TRUE,
                         /*is_implicit_cast=*/TRUE,
                         /*is_reinterpret_cast=*/FALSE,
                         /*reinterpret_semantics=*/FALSE);
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (property_ref_case) {
          cast_operand(operation_type, operand_1,
                       /*check_cast_access=*/TRUE,
                       /*is_implicit_cast=*/TRUE,
                       /*is_reinterpret_cast=*/FALSE,
                       /*reinterpret_semantics=*/FALSE);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (!imaginary_arithmetic) {  /*lint !e774*/
          op = which_binary_operator(operator_token, operation_type);
        }  /* if */
        build_binary_result_operand(operand_1, &operand_2, op,
                                    result_type, result);
        if (C_dialect == C_dialect_cplusplus && !property_ref_case) {
          /* In C++, assignment operators return lvalues. */
          change_assignment_result_to_lvalue(result, operand_1,
                                             orig_result_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (property_ref_case && !err) {
    /* For a reference to a __declspec(property(...)) field, store
       the result by calling a "put" function. */
    rewrite_property_field_reference(&operand_1_clone, result);
    copy_operand(&operand_1_clone, result);
    operand_1_clone_unused = FALSE;
  }  /* if */
  if (operand_1_clone_unused) {
    operand_will_not_be_used_because_of_error(&operand_1_clone);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_compound_assignment_operator */

#if !ABI_CHANGES_FOR_RTTI

static void build_accessible_base_class_list_for_throw(
                                                   an_expr_node_ptr throw_node)
/*
Add the list of accessible base classes to the throw node throw_node
(if necessary).
*/
{
  a_type_ptr                   type = throw_node->variant.throw_info->type;
  a_base_class_ptr             bcp;
  an_accessible_base_class_ptr abcp, last_abcp = NULL;

  /* Remove a reference or pointer type to get to any underlying class
     type. */
  if (is_reference_type(type)) type = type_pointed_to(type);
  if (is_pointer_type(type)) type = type_pointed_to(type);
  type = f_skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    /* Go through the base classes and find out which ones are accessible. */
    for (bcp = type->variant.class_struct_union.extra_info->base_classes;
         bcp != NULL;
         bcp = bcp->next) {
      if (is_accessible_base_class(bcp)) {
        /* An accessible base class -- add it to the list. */
        abcp = alloc_accessible_base_class(bcp);
        if (last_abcp == NULL) {
          throw_node->variant.throw_info->accessible_base_classes = abcp;
        } else {
          last_abcp->next = abcp;
        }  /* if */
        last_abcp = abcp;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* build_accessible_base_class_list_for_throw */

#endif /* !ABI_CHANGES_FOR_RTTI */

a_boolean is_expr_start_token(a_token_kind tok)
/*
Return TRUE if the indicated token is one that could start an expression.
*/
{
  a_boolean is_expr_start;

  switch (tok) {
    case tok_colon_colon:
    case tok_identifier:
    case tok_operator:
    case tok_this:
    case tok_float_constant:
    case tok_string_literal:
    case tok_int_constant:
    case tok_char_constant:
    case tok_true:
    case tok_false:
    case tok_plus_plus:
    case tok_minus_minus:
    case tok_ampersand:
    case tok_star:
    case tok_plus:
    case tok_minus:
    case tok_compl:
    case tok_not:
    case tok_sizeof:
    case tok_alignof:
    case tok_typeid:
    case tok_va_start:
    case tok_va_arg:
    case tok_va_end:
    case tok_va_copy:
#if GNU_EXTENSIONS_ALLOWED
    case tok_va_start_single_operand:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_uuidof:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_dynamic_cast:
    case tok_const_cast:
    case tok_static_cast:
    case tok_reinterpret_cast:
    case tok_intaddr:
    case tok_new:
    case tok_delete:
    case tok_lparen:
    case tok_typename:
    case tok_throw:
    case tok_generic:
      is_expr_start = TRUE;
      break;
    default:
      if (!C_mode() && is_type_keyword(tok)) {
        /* A type keyword, like "int".  This could be the start of a
           functional notation type conversion.  Tested using the macro to
           pick up extensions. */
        is_expr_start = TRUE;
      } else {
        is_expr_start = FALSE;
      }  /* if */
      break;
  }  /* switch */
  return is_expr_start;
}  /* is_expr_start_token */


static void scan_throw_operator(an_operand *result)
/*
Scan the C++ throw operator.  See 15.2 in the ARM.  The syntax is

  throw assignment-expression
                             opt
*/
{
  an_operand          operand;
  a_source_position   start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position   end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean           err = FALSE, expr_present;
  an_expr_node_ptr    node, throw_node;
  a_dynamic_init_ptr  dip;
  a_type_ptr          throw_type;
  an_expr_stack_entry expr_stack_entry;

  db_enter(4, "scan_throw_operator");

  /* Save the source position of the operator. */
  start_position = pos_curr_token;

  if (!exceptions_enabled) {
    /* Support for exceptions is suppressed for this compilation.  Note that
       semantic errors will not be issued on this throw expression. */
    pos_error(ec_no_exception_support, &pos_curr_token);
    err = TRUE;
  } else if (curr_expr_kind_is_const()) {
    /* "throw" not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  } else {
    /* Exceptions are outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_exceptions_in_embedded_cplusplus);
  }  /* if */

#if CHECKING
  if (curr_token != tok_throw) {
    internal_error("scan_throw_operator: expected throw");
  }  /* if */
#endif /* CHECKING */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)get_token();

  /* See if the expression is present. */
  if (!is_expr_start_token(curr_token)) {
    /* No. */
    expr_present = FALSE;
  } else {
    /* Yes, an expression is present. */
    expr_present = TRUE;
    /* Delay recording a reference to the destructor for a class operand,
       because this is an elision optimization case and the destructor
       call may be optimized away (the recipient does the destruction
       for the object actually thrown).  This necessitates a call to
       fix_up_dynamic_init_dtors later. */
    push_expr_stack(expr_stack->expression_kind, &expr_stack_entry,
                    /*force_object_lifetime=*/FALSE,
                    /*suppress_object_lifetime=*/FALSE);
    expr_stack->in_cctor_elision_initializer = TRUE;
    /* Scan the expression. */
    scan_expr(&operand, PREC_ASSIGNMENT, EOPT_NO_OPTIONS);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_position = operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Instantiate the type if it is a template class.  The type has to be
       complete so we can copy it (and we do this now so we can test whether
       the type is an abstract class). */
    complete_type_is_needed(operand.type);
    if (is_void_type(operand.type)) {
      /* Cannot throw a void expression. */
      error_in_operand(ec_void_throw, &operand);
    } else if (is_abstract_class_type(operand.type)) {
      report_abstract_class_error(ec_abstract_class_object_not_allowed,
                                  operand.type, &operand.position);
      conv_to_error_operand(&operand);
    }  /* if */
  }  /* if */

  if (err) {
    /* Operator not allowed in this kind of expression. */
    make_error_operand(result);
    if (expr_present) operand_will_not_be_used_because_of_error(&operand);
  } else {
    /* Build the throw node. */
    throw_node = alloc_expr_node((an_expr_node_kind)enk_throw);
    throw_node->type = void_type();
    if (expr_present) {
      /* There is a throw expression. */
      a_type_ptr operand_type = operand.type;
      if (is_class_struct_union_type(operand_type)) {
        /* For a class type operand, generate a dynamic initialization that
           copies the value to an undesignated location. */
        throw_type = operand_type;
        prep_elision_initializer_operand(&operand, operand_type,
                                         /*fill_in_dtor=*/FALSE,
                                         ec_bad_initializer_type, &dip);
        if (dip == NULL) err = TRUE;
        /* Determine the destructor to be called.  This is done as
           a separate step because we don't want it indicated in the
           dynamic initialization.  Note that this also forces instantiation
           of the destructor if it's a template, which is desirable. */
        throw_node->variant.throw_info->destructor =
              select_destructor(operand_type, operand_type,
                                &operand.position,
                                /*honor_virtual=*/FALSE,
                                curr_expr_is_potentially_evaluated());
      } else {
        /* For a nonclass operand, generate an expression and then make a
           dynamic initialization entry for the expression. */
        do_operand_transformations(&operand, TOPT_NO_OPTIONS);
        node = make_node_from_operand(&operand);
        throw_type = node->type;
        dip = alloc_dtor_dynamic_init((a_dynamic_init_kind)dik_expression,
                                      throw_type, &operand.position);
        dip->variant.expression = node;
      }  /* if */
      throw_node->variant.throw_info->dynamic_init = dip;
      throw_node->variant.throw_info->type = throw_type;
#if !ABI_CHANGES_FOR_RTTI
      /* Generate a list of accessible base classes. */
      /* This was done for an older version of the ABI. */
      build_accessible_base_class_list_for_throw(throw_node);
#endif /* !ABI_CHANGES_FOR_RTTI */
      /* Mark the type as having been used in an exception.  (Also, if it
         "contains" any classes, they are marked as requiring external
         linkage.) */
      set_used_in_exception_or_rtti_flag(throw_type);
    } else {
      /* There is no throw expression (i.e., this is a rethrow). */
      /* Discard the throw supplement. */
      throw_node->variant.throw_info = NULL;
    }  /* if */
    /* Make an operand for the result. */
    if (err) {
      make_error_operand(result);
    } else {
      make_expression_operand(throw_node, throw_node->type, result);
    }  /* if */
  }  /* if */

  if (expr_present) {
    /* Fix up destructor references in the overall expression. */
    fix_up_dynamic_init_dtors();
    /* Pop the expression stack entry pushed above. */
    pop_expr_stack();
  }  /* if */
  set_operand_position(result, &start_position, &end_position,
                       &start_position);
  db_exit();
}  /* scan_throw_operator */


static void scan_comma_operator(an_operand *operand_1,
                                an_operand *result)
/*
Scan the "," operator.  Note that this routine is not called if the
comma operator is not allowed (local_options flag
EOPT_DISALLOW_COMMA_OPERATOR).
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_token_sequence_number
                    operator_tok_seq_number;
  a_type_ptr        result_type, operation_type;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         result_is_an_lvalue = FALSE;
  an_expr_node_ptr  node;

  db_enter(4, "scan_comma_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);
  operator_tok_seq_number = curr_token_sequence_number;

  /* There is a potential sequence point after the first operand. */
  potential_sequence_point_after_operand(operand_1);

  if (curr_expr_kind_is_const()) {
    /* Comma operator not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &pos_curr_token);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_COMMA, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    operand_will_not_be_used_because_of_error(operand_1);
    operand_will_not_be_used_because_of_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_overloadable_type_operand(operand_1) ||
         is_overloadable_type_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_comma,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     operator_tok_seq_number,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      simplify_void_operand(operand_1);
      /* In C++ mode, an lvalue in the second operand is preserved.
         In C mode, an lvalue is converted to an rvalue. */
      if (C_dialect == C_dialect_cplusplus) {
        do_operand_transformations(&operand_2,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                                 TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION);
        result_is_an_lvalue = is_a_cplusplus_lvalue(&operand_2);
      } else {
        do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      }  /* if */
      /* The result type is the type of the second operand. */
      operation_type = result_type = operand_2.type;
      if (result_is_an_lvalue) operation_type = make_pointer_type(result_type);
      /* Make a comma operator expression. */
      node = make_node_from_void_expression_operand(operand_1);
      node->next = make_node_from_operand(&operand_2);
      node = make_operator_node((an_expr_operator_kind)eok_comma,
                                operation_type, node);
      make_expression_operand(node, operation_type, result);
      /* In C++ mode, the result is an lvalue if the second operation
         is an lvalue. */
      if (result_is_an_lvalue) {
        result->state = operand_2.state;
        result->type = result_type;
        result->variant.expression->variant.operation.
                                 returns_lvalue_instead_of_usual_rvalue = TRUE;
        result->ref_entries_list = operand_2.ref_entries_list;
      }  /* if */
    }  /* if */
  }  /* if */

  set_operand_position(result, &operand_1->position, &operand_2.end_position,
                       &operator_position);
  db_exit();
}  /* scan_comma_operator */


static void make_anonymous_union_field_operand(
                                            a_symbol_ptr      sym_ptr,
                                            a_symbol_ptr      union_sym,
                                            a_source_position *source_position,
                                            a_ref_entry_ptr   rep,
                                            an_operand        *result)
/*
Make an operand for a field that is a member of a top-level anonymous union.
(That is, an anonymous union that is not inside a struct or union.)
sym_ptr is the field; union_sym is the symbol for the anonymous union;
source_position indicates the field identifier source position; and rep
points to a reference entry, or is NULL if none is needed.  The operand
is built in *operand.  It's an lvalue for the field.
*/
{
  a_variable_ptr union_var;
  an_operand     operand_1;

  check_assertion(union_sym != NULL &&
                  union_sym->kind == (a_symbol_kind)sk_variable);
  /* Start with an operand for the base anonymous union variable. */
  union_var = union_sym->variant.variable.ptr;
  make_lvalue_variable_operand(union_var, &operand_1, (a_ref_entry_ptr)NULL,
                               /*record_expr=*/FALSE);
  /* Add a field selection to get to the field. */
  do_field_selection_operation(&operand_1, union_var->type,
                               /*is_arrow_operator=*/FALSE,
                               /*rvalue_result=*/FALSE,
                               sym_ptr, source_position, rep, result);
  result->position = *source_position;
}  /* make_anonymous_union_field_operand */


static a_boolean bad_nested_function_variable_ref(a_symbol_ptr    sym_ptr,
                                                  an_operand      *operand,
                                                  a_ref_entry_ptr *rep)
/*
sym_ptr is a symbol for a variable being referenced in an expression.
Issue an error and return TRUE if the reference is invalid because either

(1)  we are inside a local class, and the variable is a nonstatic variable
     from an enclosing function (ARM 9.8), or
(2)  we are inside a default argument expression, and the variable is a
     local variable of an enclosing function (ARM 8.2.6).

The symbol may be a top-level anonymous union (references to field symbols
within that anonymous union result in the present routine being called
with the sk_variable symbol for the union).

error_position is used for the position of any error or warning.
operand is the operand for the variable reference, and *rep is the list
of reference entries for the reference.  On an error, they are updated
to reflect the error.
*/
{
  a_boolean      bad_ref = FALSE;
  a_scope_depth  sd;
  a_variable_ptr var;

  /* This sort of bad reference is only possible when we are inside a local
     class (the class itself or one of its member functions) or a
     default argument expression. */
  if (inside_local_class || expr_stack->is_default_arg_expression) {
    if (sym_ptr->decl_scope == file_scope_number) {
      /* A reference to the file scope is okay. */
    } else if (sym_ptr->is_class_member) {
      /* A reference to a class member is okay. */
    } else if (sym_ptr->parent.namespace_ptr != NULL) {
      /* A reference to a namespace member is okay. */
    } else {
      /* Get the variable for the symbol. */
      check_assertion_str(sym_ptr->kind == (a_symbol_kind)sk_variable,
                          "bad_nested_function_variable_ref: bad sym kind");
      var = sym_ptr->variant.variable.ptr;
      /* Find the scope of the variable in the scope stack. */
      for (sd = depth_scope_stack; ; sd--) {
        a_scope_kind skind;
        if (scope_stack[sd].number == sym_ptr->decl_scope) break;
        skind = scope_stack[sd].kind;
        if (inside_local_class &&
            (skind == (a_scope_kind)sck_class_struct_union ||
             skind == (a_scope_kind)sck_class_reactivation)) {
          /* We've hit a class and we haven't hit the variable yet, so the
             variable must be a local variable of some function that
             contains the class. */
          /* Only nonstatic variables are a problem. */
          if (!has_static_storage_duration(var->storage_class)) {
            if (!strict_ansi_mode && !expr_stack->potentially_evaluated) {
              /* As an extension, allow references to nonstatic variables
                 inside sizeof expressions. */
              warning(ec_ref_to_nested_function_var);
            } else {
              bad_ref = TRUE;
            }  /* if */
          } else {
            /* Static variable.  The reference is okay, but remember that
               it exists to help back-end aliasing analysis. */
            var->referenced_non_locally = TRUE;
          }  /* if */
          break;
        } else if (expr_stack->is_default_arg_expression &&
                   skind == (a_scope_kind)sck_func_prototype) {
          /* We've hit the function prototype scope, so the variable must
             be a local variable of some function that contains the
             function prototype.  Note that the ARM doesn't draw a
             distinction between static and nonstatic variables in this
             case. */
          bad_ref = TRUE;
          break;
        }  /* if */
#if CHECKING
        if (sd <= DEPTH_OF_FILE_SCOPE) {
          internal_error("bad_nested_function_variable_ref: scope not found");
        }  /* if */
#endif /* CHECKING */
      }  /* for */
    }  /* if */
  }  /* if */
  if (bad_ref) {
    /* Issue the error. */
    error_and_make_error_operand(ec_ref_to_nested_function_var, operand);
    /* Avoid further diagnostics by making this an error reference. */
    change_refs_to_error(*rep);
    *rep = NULL;
  }  /* if */
  return bad_ref;
}  /* bad_nested_function_variable_ref */


static void check_reference_from_inline_function(a_symbol_ptr  sym_ptr)
/*
sym_ptr is a symbol for a variable or routine being referenced in an
expression.  In C99 mode, if the reference occurs within an inline function
body with external linkage, be sure the reference is not to an entity with
internal linkage (see 6.7.4 of the C99 standard).  Issue a diagnostic if the
constraint is violated.
*/
{
  a_boolean          bad_ref;
  a_routine_ptr      curr_rout;
  an_error_severity  severity;

  check_assertion(c99_mode);
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    curr_rout = scope_stack[depth_innermost_function_scope].assoc_routine;
    check_assertion(curr_rout != NULL);
    if (curr_rout->is_inline &&
        curr_rout->storage_class != (a_storage_class)sc_static) {
      /* The current routine is an inline function with external linkage. */
      if (sym_ptr->kind == (a_symbol_kind)sk_variable) {
        /* A variable reference -- see if it is a non-local variable with
           internal linkage. */
        a_variable_ptr  vp = sym_ptr->variant.variable.ptr;
        bad_ref = (vp->storage_class == (a_storage_class)sc_static &&
                   !vp->source_corresp.is_local_to_function);
      } else {
        check_assertion(sym_ptr->kind == (a_symbol_kind)sk_routine);
        /* A function reference -- see if it is a function with internal
           linkage. */
        bad_ref = (sym_ptr->variant.routine.ptr->storage_class ==
                                              (a_storage_class)sc_static);
      }  /* if */
      if (bad_ref) {
        /* Issue the diagnostic. */
        severity = strict_ansi_mode ? strict_ansi_discretionary_severity :
                                      es_discretionary_error;
        diagnostic(severity, ec_bad_linkage_of_ref_within_inline_function);
      }  /* if */
    }  /* if */
  } /* if */
}  /* check_reference_from_inline_function */


static a_symbol_ptr anonymous_parent_variable_of(a_symbol_ptr field_sym)
/*
field_sym is an sk_field symbol with anonymous_parent_object non-NULL.
Find the ultimate anonymous parent, and if it is a variable (i.e., if the
field is a member of a top-level anonymous union) return a pointer to
the sk_variable symbol.  Otherwise, return NULL.
*/
{
  a_symbol_ptr parent_sym;

  for (parent_sym = field_sym;
       parent_sym != NULL && parent_sym->kind == (a_symbol_kind)sk_field;
       parent_sym = parent_sym->variant.field.anonymous_parent_object) {}
  check_assertion_str(parent_sym == NULL ||
                      parent_sym->kind == (a_symbol_kind)sk_variable,
                      "anonymous_parent_variable_of: bad symbol kind on list");
  return parent_sym;
}  /* anonymous_parent_variable_of */


static void scan_identifier(an_operand               *result,
                            a_local_expr_options_set local_options,
                            int                      prec_level,
                            a_symbol_ptr             *p_sym_ptr)
/*
Scan an identifier, and return an operand for it in *operand.  In C++,
also handle qualified names like A::x and operator names like "operator+".
prec_level is the precedence level (see comment in scan_expr_full).
If p_sym_ptr is not NULL, set *p_sym_ptr to point to the symbol scanned
(which might be a projection symbol), or to NULL if there is an error.
*/
{
  a_symbol_ptr      sym_ptr, projection_sym_ptr = NULL, anon_var_sym;
  a_variable_ptr    var_ptr;
  a_routine_ptr     routine_ptr;
  a_source_position start_position;
  a_ref_entry_ptr   rep;
  an_operand        this_pointer_operand;
  a_type_ptr        qual_class_type;
  a_boolean         err = FALSE, is_operand_of_address_of;
  a_boolean         force_indefinite_routine_due_to_arg_dependent_lookup =
                                                                         FALSE;

  db_enter(4, "scan_identifier");

#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* Should never see an identifier in a preprocessing directive. */
    internal_error ("scan_identifier: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);
  /* Find out if this identifier is the immediate operand of a "&". */
  is_operand_of_address_of = (local_options & EOPT_PTR_TO_MEMBER_CONTEXT) != 0;

  /* If the identifier is the start of a C++ qualified name, get the whole
     name.  If not, look the name up as a normal identifier.  This routine
     also handles operator names. */
  sym_ptr = coalesce_and_lookup_generalized_identifier
                                            (GID_IS_EXPR_CONTEXT,
                                             ilm_expr, &err);
  if (locator_for_curr_id.is_semivisible_nested_type) {
    /* The symbol in the locator is a nested class that is not visible
       according to the ARM lookup rules but is returned in support of the
       nested class anachronism (ARM 18.3.5).  Issue an anachronism
       diagnostic. */
    sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                   locator_for_curr_id.specific_symbol);
  }  /* if */
  if (sym_ptr == NULL) {
    if (is_error_locator(locator_for_curr_id)) {
      /* An error was already issued. */
      make_error_operand(result);
    } else {
      /* The symbol was not in the symbol table; create an sk_undefined
         symbol.  It is not entered into the symbol table at this time. */
      sym_ptr = alloc_symbol((a_symbol_kind)sk_undefined,
                             locator_for_curr_id.symbol_header,
                             &locator_for_curr_id.source_position);
      if (curr_expr_kind_is_const()) {
        /* In a constant expression, an undefined identifier is still
           flagged as "undefined" -- it makes the error message clearer. */
        enter_undefined_symbol(sym_ptr);
        str_error(ec_undefined_identifier,
                  locator_for_curr_id.symbol_header->identifier);
        record_symbol_reference((a_symbol_reference_kind)(SRK_REFERENCE |
                                                          SRK_ERROR),
                                sym_ptr, &locator_for_curr_id.source_position,
                                /*update_il_entry=*/FALSE);
        make_error_operand(result);
      } else {
        /* Make a transient undefined symbol operand that will be either
           turned into an implicitly declared function (in C) or ignored
           (in C++, with argument-dependent lookup), or diagnosed as an
           error. */
        clear_operand((an_operand_kind)ok_undefined_symbol, result);
        result->type = unknown_type();
        result->variant.symbol = sym_ptr;
        result->ref_entries_list = ref_entry(sym_ptr, &pos_curr_token);
        result->id_position = locator_for_curr_id.source_position;
      }  /* if */
    }  /* if */
  } else {
    /* The symbol is defined. */
    if (microsoft_mode && is_constructor_symbol(sym_ptr)) {
      /* In Microsoft mode, treat the name of a constructor as the name
         of the class, so that something like "C::C()" is seen as a
         functional-notation type conversion. */
      sym_ptr = (a_symbol_ptr)(sym_ptr->parent.class_type->
                                                    source_corresp.assoc_info);
    }  /* if */
    /* Create a reference entry for the symbol if needed. */
    /* Don't do this if the symbol is an overloaded function (we don't
       yet know which function is being called). */
    if (sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
      rep = NULL;
    } else if (sym_ptr->kind == (a_symbol_kind)sk_routine &&
               !C_mode() && arg_dependent_lookup_enabled &&
               next_token() == tok_lparen) {
      /* When argument-dependent lookup is enabled, even if the symbol
         is a simple routine name it might not be the routine that is
         called, so go to overload resolution and handle the reference
         there. */
      force_indefinite_routine_due_to_arg_dependent_lookup = TRUE;
      rep = NULL;
    } else {
      rep = ref_entry(sym_ptr, &locator_for_curr_id.source_position);
    }  /* if */
    /* Do ambiguity and access control checking on the member.  For overloaded
       functions, this checks ambiguity but not access (which can be different
       for each function in the set). */
    check_ambiguity_and_verify_access(&locator_for_curr_id);
    if (is_error_locator(locator_for_curr_id)) {
      /* Some kind of error in the ambiguity and access control checking. */
      make_error_operand(result);
      change_refs_to_error(rep);
      rep = NULL;
    } else {
      if (warning_on_for_init_difference) {
        /* Unless it is a qualified-name reference, if sym_ptr is visible with
           new-style for-init declaration scoping but would be hidden using
           the old (cfront compatible) rules, a warning is appropriate. */
        if (sym_ptr->hidden_by_old_for_init &&
            !locator_for_curr_id.is_qualified_name) {
          report_for_init_difference(sym_ptr,
                                     &locator_for_curr_id.source_position);
        }  /* if */
      }  /* if */
      projection_sym_ptr = locator_for_curr_id.specific_symbol;
      /* What kind of symbol is it? */
      switch (sym_ptr->kind) {
        case sk_constant:
          /* Constant (e.g., an enum constant).  Make a constant operand. */
          make_sym_constant_operand(sym_ptr, result);
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* In an integral constant expression, check that the constant
               is integral or enum.  This is needed for nontype template
               arguments.  It might also be needed for the extension that
               allows definition of constants within a class if that
               extension were to allow non-integral constants. */
            if (!is_template_param_type(result->type)) {
              (void)check_integral_or_enum_operand(result);
            }  /* if */
          } else if (!curr_expr_kind_is_const()) {
            /* In a non-constant expression, treat a nonreal member as
               an lvalue rather than a constant. */
            change_nonreal_member_constant_operand_to_lvalue(result);
          }  /* if */
          break;
        case sk_static_data_member:
          var_ptr = sym_ptr->variant.static_data_member.variable;
          goto variable;
        case sk_variable:
          var_ptr = sym_ptr->variant.variable.ptr;
variable:
          if (curr_expr_kind_is_const()) {
            /* A variable is allowed only in initializer expressions,
               and only if it is static and its address is being taken
               (i.e., it's not the variable's value that is wanted).
               The "static" part is checked here; the "its address is
               being taken" part is checked in conv_lvalue_to_rvalue.
               We allow an lvalue out, and if it's converted to an rvalue,
               it's an error.  This is necessary because of constructs
               like "int *p = &a.i" -- the address of "a" is not taken,
               but the lvalue never gets turned into an rvalue, so it's
               okay. */
            /* Note that in C++ initializer expressions are used only for
               class constants (an extension) and for the argument of
               __INTADDR__. */
            if ((curr_expr_kind_is(ek_init_constant) ||
                 curr_expr_kind_is(ek_template_arg)) &&
                has_static_storage_duration(var_ptr->storage_class) &&
                /* Disallow C++ reference variables in constant
                   expressions, because of the extra indirection. */
                !is_reference_type(var_ptr->type)) {
              /* Make an lvalue operand for the variable. */
              make_lvalue_variable_operand(var_ptr, result, rep,
                                           /*record_expr=*/TRUE);
            } else if ((any_cfront_mode() || (microsoft_mode && !C_mode())) &&
                       (curr_expr_kind_is(ek_integral_constant) ||
                        curr_expr_kind_is(ek_template_arg)) &&
                       ((is_class_struct_union_type(var_ptr->type) &&
                         next_token() == tok_period) ||
                        (is_pointer_type(var_ptr->type) &&
                         is_class_struct_union_type(
                                             type_pointed_to(var_ptr->type)) &&
                         next_token() == tok_arrow))) {
              /* In cfront or Microsoft C++ mode, allow a class variable
                 identifier followed by a field selection dot, or a pointer
                 to class variable identifier followed by "->".  This is
                 needed because cfront and MSVC++ allow things like x.e,
                 where e is something like an enumerator constant, as part
                 of a constant expression. */
              /* Make an lvalue operand for the variable. */
              make_lvalue_variable_operand(var_ptr, result, rep,
                                           /*record_expr=*/TRUE);
            } else if (!C_mode() && is_const_variable(var_ptr)) {
              /* In C++, integral const identifiers can be used in constant
                 expressions.  */
              a_constant_ptr con_val = var_constant_value(var_ptr);
              if (con_val == NULL) {
                /* The variable is const, but its value is not known at
                   compile time. */
                error_and_make_error_operand(ec_constant_value_not_known,
                                             result);
                change_refs_to_error(rep);
                rep = NULL;
              } else {
                /* The identifier is const and has a known constant value. */
                make_constant_variable_operand(con_val, var_ptr, result);
                /* The value of the variable is used. */
                change_ref_kinds(rep, SRK_USE);
              }  /* if */
            } else {
              /* All other cases are not allowed. */
              error_and_make_error_operand(ec_expr_not_constant, result);
              change_refs_to_error(rep);
              rep = NULL;
            }  /* if */
          } else {
            /* Nonconstant expression. */
            /* If we're inside a local class, we are not allowed to reference
               non-static variables of the containing function.  If we're
               inside a default argument expression, we're not allowed to
               reference local variables of any containing function.
               Check for those. */
            if (bad_nested_function_variable_ref(sym_ptr, result, &rep)) {
              /* Error. */
            } else {
              /* In C99 mode check that a variable referenced within an
                 inline function is valid. */
              if (c99_mode) check_reference_from_inline_function(sym_ptr);
              /* Make a variable operand that is a variable address node.
                 The type of the operand is a pointer to the type of the
                 variable. */
              make_lvalue_variable_operand(var_ptr, result, rep,
                                           /*record_expr=*/TRUE);
            }  /* if */
          }  /* if */
          break;
        case sk_routine:
          if (force_indefinite_routine_due_to_arg_dependent_lookup) {
            /* In C++, the name in a function call is subject to
               argument-dependent lookup, so treat this function as
               if it is an overloaded function. */
            goto overloaded_function;
          }  /* if */
normal_function:
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Function identifiers are not allowed in integral constant
               expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
            change_refs_to_error(rep);
            rep = NULL;
          } else {
            /* In C99 mode check that a unction referenced within an
               inline function is valid. */
            if (c99_mode) check_reference_from_inline_function(sym_ptr);
            /* Make a function designator operand for the function. */
            make_function_designator_operand(projection_sym_ptr,
                                             (a_boolean)locator_for_curr_id.
                                                             is_qualified_name,
                                             &locator_for_curr_id.
                                                               source_position,
                                             rep,
                                             result);
          }  /* if */
          break;
        case sk_field:
          /* In C++, in a member function, a reference to a nonstatic data
             member (field) is the same as "this->field".  Or, a field
             could be a member of an unnamed union at file scope or in
             a block. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
            change_refs_to_error(rep);
            rep = NULL;
          } else if (sym_ptr->variant.field.anonymous_parent_object != NULL &&
                     (anon_var_sym = anonymous_parent_variable_of(sym_ptr)) !=
                                                                        NULL) {
            /* This field is a member of a top-level (variable) anonymous
               union. */
            if (curr_expr_kind_is_const()) {
              /* This is not allowed in a constant expression. */
              error_and_make_error_operand(ec_expr_not_constant, result);
              change_refs_to_error(rep);
              rep = NULL;
            } else if (bad_nested_function_variable_ref(anon_var_sym,
                                                        result, &rep)) {
              /* If we're inside a local class, we are not allowed to reference
                 non-static variables of the containing function.  If we're
                 inside a default argument expression, we're not allowed to
                 reference local variables of any containing function.
                 Check for those. */
            } else {
              make_anonymous_union_field_operand(sym_ptr, anon_var_sym,
                                                 &locator_for_curr_id.
                                                               source_position,
                                                 rep, result);
            }  /* if */
          } else {
            /* Normal case -- field is a nonstatic data member of a class. */
            /* If this identifier is a qualified name for a class member and is
               the immediate operand of a unary "&" operator, it is a
               pointer-to-member. */
            /* The token_ends_expr test guards against cases like
                 &A::x++
               where the "++" binds more tightly than the "&". */
            if (is_operand_of_address_of &&
                locator_for_curr_id.is_qualified_name &&
                token_ends_expr(next_token(), prec_level, local_options)) {
              /* The field was referenced by a qualified name and is the
                 immediate operand of a unary "&"; make up an operand that
                 preserves the qualified name so scan_ampersand_operator can
                 turn it into a pointer-to-member. */
              make_sym_for_member_operand(projection_sym_ptr,
                                          /*is_qualified_name=*/TRUE,
                                          rep, result);
            } else {
              /* Normal case: "x" is interpreted as "this->x". */
              /* Make an operand for the "this" pointer. */
              if (make_this_pointer_operand(sym_ptr,
                                            projection_sym_ptr,
                                          &locator_for_curr_id.source_position,
                                            (a_boolean)locator_for_curr_id.
                                                 access_control_error_reported,
                                            &this_pointer_operand)) {
                /* Do the field selection relative to the "this" pointer. */
                /* Extract the possibly qualified version of the class type
                   pointed to. */
                qual_class_type = type_pointed_to(this_pointer_operand.type);
                do_field_selection_operation(&this_pointer_operand,
                                             qual_class_type,
                                             /*is_arrow_operator=*/TRUE,
                                             /*rvalue_result=*/FALSE,
                                             sym_ptr,
                                             &locator_for_curr_id.
                                                               source_position,
                                             rep, result);
              } else {
                /* There was some problem in constructing the "this"
                   operand. */
                make_error_operand(result);
              }  /* if */
            }  /* if */
          }  /* if */
          break;
        case sk_member_function:
          /* Static member functions are handled like normal functions. */
          routine_ptr = sym_ptr->variant.routine.ptr;
          if (!routine_type_is_nonstatic_member_function(routine_ptr->type)) {
            goto normal_function;
          }  /* if */
          /* Nonstatic nonoverloaded member function. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
            change_refs_to_error(rep);
            rep = NULL;
          } else {
            /* Build an operand representing an uninterpreted member name.
               This is done because we don't know yet whether a name like
               "A::f" should be treated as "this->A::f" because it is going
               to be called, or whether it will be a pointer to member,
               e.g., "&A::f" or (as an extension) simply "A::f".  Likewise
               for unqualified names, e.g., just "f" (in that case, both
               "&f" and "f" are nonstandard ways of getting a pointer-to-
               member). */
            make_sym_for_member_operand(projection_sym_ptr,
                                        (a_boolean)
                                         locator_for_curr_id.is_qualified_name,
                                        rep, result);
          }  /* if */
          break;
        case sk_overloaded_function:
overloaded_function:
          /* Overloaded function. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
            /* No need to call change_refs_to_error; rep is NULL. */
          } else {
            /* Make an operand for an indefinite function.  Note that we
               do not generate a "this" parameter at this point, even if
               we could determine that all the functions in the overload
               cluster are nonstatic member functions.  It's easier to
               generate it at the other end of the overload resolution
               when we know for sure whether or not we need it. */
            make_indefinite_function_operand(projection_sym_ptr,
                                             /*curr_id=*/TRUE,
                                             result);
          }  /* if */
          break;
        case sk_function_template:
          /* Function template. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
            /* No need to call change_refs_to_error; rep is NULL. */
          } else {
            make_indefinite_function_operand(projection_sym_ptr,
                                             /*curr_id=*/TRUE,
                                             result);
          }  /* if */
          break;
        case sk_class_template:
          /* Class template.  Returned by symbol lookup for cases like
             A<T>::template f<N> in prototype instantiations.  A class
             template is returned because there's only a representation
             for the class case as a member of a nonreal class, but it's
             really a function template. */
          check_assertion(locator_for_curr_id.is_template_id &&
                          is_template_dependent_context());
          make_unknown_dependent_function_operand(projection_sym_ptr,
                                                  /*is_template_id=*/TRUE,
                                                  locator_for_curr_id.
                                                             template_arg_list,
                                                  (a_boolean)
                                                       locator_for_curr_id.
                                                             is_qualified_name,
                                                  result);
          break;
        case sk_undefined:
          /* Symbol was found in the symbol table, but it is undefined.  This
             means that it was encountered earlier but was never turned into a
             function call.  Or, there was an ambiguity error.  No error
             message is issued here because one was issued earlier.  An error
             operand is returned. */
          make_error_operand(result);
          break;
        case sk_type:
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          /* The identifier is a type identifier. */
          if (C_dialect == C_dialect_cplusplus && next_token() == tok_lparen) {
            /* In C++, a functional-notation type conversion. */
            a_type_ptr cast_type = type_symbol_type(sym_ptr);
            (void)get_token();
            scan_functional_notation_type_conversion(cast_type,
                                                     &start_position,
                                                     result,
                                                     local_options);
            goto after_advance_past_id;
          } else {
            /* Otherwise, an error. */
            error_and_make_error_operand(ec_type_identifier_not_allowed,
                                         result);
            /* No need to call change_refs_to_error; rep is NULL. */
          }  /* if */
          break;
        case sk_namespace:
          /* The identifier is a namespace name. */
          error_and_make_error_operand(ec_namespace_name_not_allowed,
                                       result);
          /* No need to call change_refs_to_error; rep is NULL. */
          break;
        case sk_parameter:
          if (expr_stack->is_default_arg_expression ||
              (!curr_expr_kind_is(ek_sizeof) &&
               !expr_stack->is_vla_dimension_expression)) {
            /* This is a parameter referenced within a C++ default argument
               expression, which is an error (ARM 8.2.6); or, any reference
               except in a sizeof or function prototype VLA dimension
               expression. */
            error_and_make_error_operand(ec_param_not_allowed, result);
            change_refs_to_error(rep);
            rep = NULL;
          } else if (expr_stack->is_vla_dimension_expression) {
            /* Use of a parameter in function prototype VLA dimension
               expression, e.g.:
                   void f(a, int b[a]);
               Since no variable has been created for the parameter yet,
               create a dummy "placeholder" variable to use in the
               expression.  Also, create a_vla_fixup for the parameter
               so that the dummy variable can be replaced with the real
               variable for the parameter once it is created.  If the
               function prototype is part of a function declaration, not
               definition, the VLA dimension expression is thrown away. */
            /* Create a dummy placeholder variable with the same type
               as the parameter, if it hasn't been created yet. */
            var_ptr = sym_ptr->variant.param_id->dummy_vla_variable;
            if (var_ptr == NULL) {
              var_ptr = make_variable(sym_ptr->variant.param_id->type, 
                                      (a_storage_class)sc_auto,
                                      NO_SCOPE_DEPTH);
              var_ptr->source_corresp.assoc_info = (char *)sym_ptr;
              sym_ptr->variant.param_id->dummy_vla_variable = var_ptr;
            }  /* if */
            /* Generate an expression node referring to the dummy variable. */
            make_lvalue_variable_operand(var_ptr, result, rep,
                                         /*record_expr=*/FALSE);
            check_assertion(is_expression_operand(result));
            /* Create a_vla_fixup for the parameter, initialize its
               members, and link it into the list of vla fixups for the
               current function prototype scope. */
            add_vla_fixup_entry((a_type_ptr)NULL, result->variant.expression,
                                sym_ptr, &result->position);
          } else {
            /* Use of a parameter in a sizeof expression, something like
                 void f(a, int b[sizeof(a)]);
               Create a constant pointer to the right type to make an lvalue
               of the right type, since there is no variable yet (the
               parameter is represented by an sk_parameter symbol). */
            a_constant constant;
            a_type_ptr param_type = sym_ptr->variant.param_id->type;

            make_zero_of_proper_type(make_pointer_type(param_type), &constant);
            make_constant_operand(&constant, result);
            result->state = (an_operand_state)os_lvalue;
            result->type = param_type;
          }  /* if */
          break;
#if CHECKING
        case sk_keyword:
        default:
          internal_error("scan_identifier: bad symbol kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
  }  /* if */

  /* Remember whether or not an access control error was reported on the
     identifier.  This is useful for suppressing additional errors due
     to the ARM 11.5 protected member access check. */
  result->access_control_error_reported =
                             locator_for_curr_id.access_control_error_reported;
  /* Remember whether or not this operand is the immediate operand of
     a "&" operator. */
  result->is_operand_of_address_of = is_operand_of_address_of;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Advance past the identifier. */
  (void)get_token();
after_advance_past_id:

  error_position = result->position = start_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  result->end_position = curr_construct_end_position;
  /* If the operand has kind ok_expression, set the position in the
     expression too. */
  set_operand_expr_position_if_expr(result, (a_source_position *)NULL);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  if (p_sym_ptr != NULL) *p_sym_ptr = projection_sym_ptr;

  db_exit();
}  /* scan_identifier */


static void check_for_pcc_compound_assignment_operators(void)
/*
In pcc mode, compound assignment operators can be written in two
nonstandard ways:

  (1)  There can be whitespace between the characters, as in "- =".
       No warning is issued.
  (2)  The "=" can appear first, as in "=-".  No whitespace is
       allowed.  This is described in Appendix A, section 17
       (Anachronisms) of K&R I.  A warning is issued.  This form
       is accepted only if C_ANACHRONISMS_ALLOWED is TRUE.

These cases are handled here by coalescing two tokens.
*/
{
  a_token_kind      token = curr_token, compound_token;
  a_source_position start_position;

#if C_ANACHRONISMS_ALLOWED
  a_boolean         equals_first = FALSE;
  char              ch;

  if (token == tok_assign) {
    /* "=" is first.  If the next token follows immediately (i.e.,
       there is no white space), enable the check for things like "=-". */
    ch = *curr_char_loc;
    /* Note the special test for "/": "=/" followed by "*" is really "="
       followed by the start of a comment. */
    if (ch ==  '+' || ch ==  '-' || ch ==  '*' || ch ==  '%' ||
        (ch ==  '/' && *(curr_char_loc+1) != '*') ||
        ch ==  '&' || ch ==  '^' || ch ==  '|' || ch ==  '>' || ch ==  '<') {
      equals_first = TRUE;
      token = next_token();
    }  /* if */
  }  /* if */
#endif /* C_ANACHRONISMS_ALLOWED */
  compound_token = token;
  switch (token) {
    case tok_plus:
      compound_token = tok_plus_assign;
      break;
    case tok_minus:
      compound_token = tok_minus_assign;
      break;
    case tok_star:
      compound_token = tok_times_assign;
      break;
    case tok_divide:
      compound_token = tok_divide_assign;
      break;
    case tok_remainder:
      compound_token = tok_remainder_assign;
      break;
    case tok_ampersand:
      compound_token = tok_and_assign;
      break;
    case tok_excl_or:
      compound_token = tok_excl_or_assign;
      break;
    case tok_or:
      compound_token = tok_or_assign;
      break;
    case tok_shift_right:
      compound_token = tok_shift_right_assign;
      break;
    case tok_shift_left:
      compound_token = tok_shift_left_assign;
      break;
    default:;
      /* No action. */
  }  /* switch */
  if (compound_token != token) {
#if C_ANACHRONISMS_ALLOWED
    if (equals_first) {
      /* "=-" form. */
      start_position = pos_curr_token;
      pos_warning(ec_old_fashioned_assignment_operator, &start_position);
      (void)get_token();
      pos_curr_token = start_position;
      curr_token = compound_token;
    } else {
#endif /* C_ANACHRONISMS_ALLOWED */
      /* Check for "- =" form. */
      if (next_token() == tok_assign) {
        /* This is the "- =" form. */
        start_position = pos_curr_token;
        (void)get_token();
        pos_curr_token = start_position;
        curr_token = compound_token;
      }  /* if */
#if C_ANACHRONISMS_ALLOWED
    }  /* if */
#endif /* C_ANACHRONISMS_ALLOWED */
  }  /* if */        
}  /* check_for_pcc_compound_assignment_operator */


static void make_function_name_operand(an_operand *result,
                                       a_boolean  decorated_name)
/*
Create an operand referring to a constant local static string variable
containing the null-terminated name of the current function.  If the variable
has not yet been created for the current function, create it now.  The result
is stored in *result.  If decorated_name is TRUE, the mangled name is
returned instead of the unqualified function name.
*/
{
  a_variable_ptr           name_var = NULL;
  a_scope_stack_entry_ptr  ssep;

  check_assertion(microsoft_mode || gcc_mode || (c99_mode && !decorated_name));
  check_assertion(depth_innermost_function_scope != 0);
  ssep = &scope_stack[depth_innermost_function_scope];
  if (gcc_mode) {
    /* In GNU C mode, we create a constant operand. */
    set_curr_token_to_string_literal(ssep->assoc_routine->source_corresp.name);
    /* Make sure that e.g. __FUNCTION__ "(postfix)" is accepted. */
    concat_adjacent_string_literals(/*curr_token_set=*/TRUE);
    make_string_constant_operand(&const_for_curr_token, result);
  } else {
    /* Check if this scope already has an associated generated entity block. */
    if (ssep->generated_entities == NULL) {
      ssep->generated_entities = (a_generated_entity_block_ptr)
                                   alloc_fe(sizeof(a_generated_entity_block));
#if DEBUG
      ++num_generated_entity_blocks_allocated;
#endif /* DEBUG */
      ssep->generated_entities->decorated_function_name = NULL;
      ssep->generated_entities->function_name = NULL;
    }  /* if */
    name_var = decorated_name ?
                           ssep->generated_entities->decorated_function_name :
                           ssep->generated_entities->function_name;
    if (name_var == NULL) {
      /* The required constant string variable has not yet been created for
         this function.  Create it now. */
      a_routine_ptr          rp = ssep->assoc_routine;
      char                   *name_ptr =
#if MICROSOFT_EXTENSIONS_ALLOWED
                              decorated_name ? get_mangled_function_name(rp) :
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                               rp->source_corresp.name;
      a_constant_ptr         name_string;
      a_targ_size_t          length = ((a_targ_size_t)strlen(name_ptr))+1;
      a_memory_region_number region_to_switch_back_to;
      a_type_ptr             var_type;
      /* Create the string literal. */
      /* Make sure the string literal constant is allocated in file scope,
         so that we can directly point to it as an initializer from the
         variable. */
      switch_to_file_scope_region(&region_to_switch_back_to);
      name_string = alloc_constant((a_constant_repr_kind)ck_string);
      switch_back_to_original_region(region_to_switch_back_to);
      name_string->type = string_type(length);
      name_string->variant.string.length = length;
      name_string->variant.string.value =
                               alloc_text_of_string_literal((sizeof_t)length);
      (void)memcpy(name_string->variant.string.value, name_ptr,
                   size_t_arg(length));
      /* Create the local static const array and initialize it with the
         string constant. */
      /* In C99, the variable is an array of const.  In Microsoft mode,
         it has the same type as the string. */
      if (microsoft_mode) {
        var_type = name_string->type;
      } else {
        /* Create an array of const char type. */
        var_type = alloc_type((a_type_kind)tk_array);
        var_type->variant.array.element_type =
             make_qualified_type(integer_type(plain_char_int_kind), TQ_CONST);
        var_type->variant.array.variant.number_of_elements = length;
        set_type_size(var_type);
      }  /* if */
      name_var = make_variable(var_type, (a_storage_class)sc_static,
                               depth_innermost_function_scope);
      name_var->source_corresp.name =
                                locator_for_curr_id.symbol_header->identifier;
      name_var->source_corresp.is_local_to_function = TRUE;
      name_var->init_kind = (an_init_kind)initk_static;
      name_var->initializer.constant = name_string;
      /* To be sure, always consider the variable's address has been taken. */
      set_variable_address_taken(name_var);
      /* Remember the above construct for potential reuse. */
      if (decorated_name) {
        ssep->generated_entities->decorated_function_name = name_var;
      } else {
        ssep->generated_entities->function_name = name_var;
      }  /* if */
    }  /* if */
    /* Create an operand that refers to the implicit static variable. */
    make_lvalue_variable_operand(name_var, result, (a_ref_entry_ptr)NULL,
                                 /*record_expr=*/FALSE);
  }  /* if */
}  /* make_function_name_operand */


#if GNU_EXTENSIONS_ALLOWED

static void mark_operand_as_gnu_extension(an_operand  *op)
/*
The given operand (an expression or a constant) should be marked as having
been annotated in the source with the GNU keyword __extension__.
*/
{
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  an_expr_node_ptr  expr;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */

  switch (op->kind) {
    case ok_error:
      /* Ignore this expression. */
      break;
    case ok_expression:
      op->variant.expression->marked_as_gnu_extension = TRUE;
      break;
    case ok_constant:
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
      expr = op->variant.constant.expr;
      if (expr == NULL) {
        /* Create a constant expression to record the extension flag. */
        a_memory_region_number  region_to_switch_back_to;
        a_constant_ptr  con = fs_constant(op->variant.constant.kind);
        copy_constant(&op->variant.constant, con);
        switch_to_file_scope_region(&region_to_switch_back_to);
        expr = alloc_expr_node((an_expr_node_kind)enk_constant);
        expr->type = con->type;
        expr->variant.constant = con;
        op->variant.constant.expr = expr;
        switch_back_to_original_region(region_to_switch_back_to);
      }  /* if */
      expr->marked_as_gnu_extension = TRUE;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
      break;
    default:
      unexpected_condition();
  }  /* switch */
}  /* mark_operand_as_gnu_extension */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void scan_expr_full(an_operand               *result,
                           an_operand               *bound_function_selector,
                           int                      prec_level,
                           a_local_expr_options_set local_options)
/*
Scan an expression and return it in *result.  If the expression is for
a bound function in C++, also set *bound_function_selector to indicate the
object.  prec_level indicates the precedence level that controls this
scan; scan_expr_full stops on an operator with lower precedence than
prec_level.  local_options indicates some temporary options that apply
only for the top-level expression scanned by this routine (disallow comma
operator, suppress conversion of arrays and/or functions to pointers, etc. --
see expr.h).
*/
{
  a_source_position start_position;
  an_operand        operand;
  an_operand        local_result, local_bound_function_selector;
  a_token_kind      ntoken;
  a_ref_entry_ptr   saved_ref_list, selector_ref_entry_list, last_rep;
#if GNU_EXTENSIONS_ALLOWED
  a_boolean         marked_as_gnu_extension = 
                               (local_options & EOPT_MARKED_AS_GNU_EXTENSION);
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(4, "scan_expr_full");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "precedence level = %d\n", prec_level);
  }  /* if */
#endif /* DEBUG */

#if GNU_EXTENSIONS_ALLOWED
  if (gcc_mode && !marked_as_gnu_extension && curr_token == tok_extension) {
    /* Ignore the GNU C __extension__ annotation. */
    (void)get_token();
    marked_as_gnu_extension = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

  /* Save the reference entry list for the surrounding expression.  It will
     be restored on exit.  Start a new list for this expression. */
  saved_ref_list = curr_expr_ref_entries;
  curr_expr_ref_entries = NULL;

  /* Scan first one of the following:
     1)  A leaf operand, like an identifier or literal constant.
     2)  An expression in parentheses or a cast.
     3)  A unary operator followed by an expression.
     4)  A sizeof expression.
  */
  /* If a left parenthesis was trapped by the caller, go to the code that
     handles a left parenthesis. */
  if (local_options & EOPT_TRAPPED_LEFT_PAREN) goto handle_trapped_left_paren;
  switch ((int)curr_token) {
    case tok_colon_colon:
      ntoken = next_token();
      /* Check for ":: new". */
      if (ntoken == tok_new) goto scan_new;
      /* Check for ":: delete". */
      if (ntoken == tok_delete) goto scan_delete;
      /* Fall through to next case ("::" is the start of a qualified name). */
    case tok_identifier:
    case tok_operator:               /* Start of "operator+" and the like. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_super:                  /* Microsoft __super qualifier. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Watch out for something like "S::*". */
      if (!is_expr_qualified_name_start()) {
        goto bad_start_of_primary;
      }  /* if */
      scan_identifier(&local_result, local_options, prec_level,
                      (a_symbol_ptr *)NULL);
      break;
    case tok_this:
      /* In C++, "this" in a nonstatic member function is a non-lvalue that
         points to the object for which the member function was called. */
      if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        /* We're not inside a function. */
        error_and_make_error_operand(ec_this_used_incorrectly, &local_result);
      } else if (curr_expr_kind_is_const() &&
                 /* cfront and Microsoft allow this->k, where k is a constant,
                    in a constant expression. */
                 !((any_cfront_mode() || (microsoft_mode && !C_mode())) &&
                   next_token() == tok_arrow &&
                   (curr_expr_kind_is(ek_integral_constant) ||
                    curr_expr_kind_is(ek_template_arg)))) {
        /* "this" cannot be used in a constant expression. */
        error_and_make_error_operand(ec_expr_not_constant, &local_result);
      } else {
        a_variable_ptr this_var = 
                         scope_stack[depth_innermost_function_scope].il_scope->
                                           variant.routine.this_param_variable;
        if (this_var == NULL) {
          /* We're not inside a nonstatic member function. */
          error_and_make_error_operand(ec_this_used_incorrectly,
                                       &local_result);
        } else {
          /* Make an rvalue for the "this" variable. */
          make_this_variable_operand(this_var, /*is_implicit=*/FALSE,
                                     &local_result);
        }  /* if */
      }  /* if */
      (void)get_token();
      break;
    case tok_function_name:
    case tok_decorated_function_name:
      /* A magic identifier that expands to a string literal containing the
         name of the current function in C99, GNU C and Microsoft modes.
         (The "decorated" variant is recognized in Microsoft and GNU C modes
         only and in Microsoft mode it expands to the mangled name.) */
      check_assertion(microsoft_mode || gcc_mode ||
                      (c99_mode && curr_token == tok_function_name));
      if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        /* We're not inside a function. */
        str_error(ec_id_can_only_appear_in_function,
                  locator_for_curr_id.symbol_header->identifier);
        make_error_operand(&local_result);
      } else if (curr_expr_kind_is(ek_pp) ||
                 curr_expr_kind_is(ek_integral_constant)) {
        /* These are not allowed in an integral constant expression. */
        error_and_make_error_operand(enum_type_is_integral ?
                                       ec_expr_not_integral :
                                       ec_expr_not_integral_or_enum,
                                     &local_result);
      } else {
        make_function_name_operand(
                 &local_result, !C_mode() && curr_token != tok_function_name);
      }  /* if */
      (void)get_token();
      break;
#if TARG_HAS_IEEE_FLOATING_POINT
    case tok_nan:
      /* The EDG-specific token "__NAN__" representing a Not-a-Number
         constant. */
    case tok_infinity:
      /* The EDG-specific token "__INFINITY__" representing an Infinity
         constant. */
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tok_imaginary_unit:
      /* The EDG-specific token "__I__" representing an imaginary value such
         that __I__*__I__ == -1. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tok_float_constant:
      { a_boolean float_con_allowed = TRUE;
        if (curr_expr_kind_is(ek_pp)) {
          /* Floating constants are not allowed in preprocessing
             expressions. */
          float_con_allowed = FALSE;
        } else if (curr_expr_kind_is(ek_integral_constant) ||
                   (curr_expr_kind_is(ek_template_arg) &&
                    !floating_point_template_parameters_allowed)) {
          /* In integral constant expressions, they are allowed only as the 
             immediate operand of a cast.  Template argument expressions are
             usually the same as integral constant expressions. */
          float_con_allowed = FALSE;
          if ((local_options & EOPT_OPERAND_OF_CAST) &&
              /* Guard against something like "int(3.0/1)". */
              token_ends_expr(next_token(), prec_level, local_options)) {
            float_con_allowed = TRUE;
          }  /* if */
        }  /* if */
        if (float_con_allowed) {
#if TARG_HAS_IEEE_FLOATING_POINT
          if (curr_token == tok_nan) {
            /* __NAN__ */
            make_nan_operand(&local_result);
          } else if (curr_token == tok_infinity) {
            /* __INFINITY__ */
            make_infinity_operand(&local_result);
          } else
#endif /* TARG_HAS_IEEE_FLOATING_POINT */
#if C99_IL_EXTENSIONS_SUPPORTED
          if (curr_token == tok_imaginary_unit) {
            /* __I__ */
            make_imaginary_unit_operand(&local_result);
          } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          /* Do not insert code here. */
          {
            make_constant_operand(&const_for_curr_token, &local_result);
          }  /* if */
        } else {
          error_and_make_error_operand(enum_type_is_integral ?
                                         ec_expr_not_integral :
                                         ec_expr_not_integral_or_enum,
                                       &local_result);
        }  /* if */
      }  /* if */
      (void)get_token();
      break;
    case tok_string_literal:
      make_string_constant_operand(&const_for_curr_token, &local_result);
      if (curr_expr_kind_is(ek_pp) ||
	  curr_expr_kind_is(ek_integral_constant)) {
	error_and_make_error_operand(enum_type_is_integral ?
                                       ec_expr_not_integral :
                                       ec_expr_not_integral_or_enum,
                                     &local_result);
      }  /* if */
      (void)get_token();
      break;
    case tok_int_constant:
    case tok_char_constant:
    case tok_true:
    case tok_false:
      make_constant_operand(&const_for_curr_token, &local_result);
      if (any_cfront_mode() && const_for_curr_token.is_simple_zero) {
        /* Cfront accepts only a simple 0 as a null pointer constant.
           Keep track of that. */
        local_result.is_cfront_null_pointer_constant = TRUE;
      }  /* if */
      (void)get_token();
      break;

    case tok_plus_plus:
    case tok_minus_minus:
      scan_prefix_incr_decr(&local_result);
      break;

    case tok_ampersand:
      scan_ampersand_operator(&local_result);
      break;

#if GNU_EXTENSIONS_ALLOWED
    case tok_and_and:
      if (!gcc_mode ||
          next_token() != tok_identifier) goto bad_start_of_primary;
      scan_address_of_label_expression(&local_result);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */

    case tok_star:
      scan_indirection_operator(&local_result);
      break;

    case tok_plus:
    case tok_minus:
    case tok_compl:
    case tok_not:
      scan_arith_prefix_operator(&local_result);
      break;

    case tok_sizeof:
      /* Sizeof operation. */
      scan_sizeof_operator(&local_result);
      break;

    case tok_alignof:
      /* __ALIGNOF__ operation. */
      scan_alignof_operator(&local_result);
      break;

    case tok_generic:
      /* __generic operation, implementing type-generic functions in C99. */
      scan_type_generic_operator(&local_result);
      break;

#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_assume:
      /* Microsoft __assume(...). */
      scan_assume_operator(&local_result);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

    case tok_typeid:
      /* typeid operation. */
      scan_typeid_operator(&local_result);
      break;

    case tok_va_start:
      /* <stdarg.h> va_start macro, when treated as a builtin. */
      scan_va_start_operator(&local_result, /*single_operand=*/FALSE);
      break;

#if GNU_EXTENSIONS_ALLOWED
    case tok_va_start_single_operand:
      /* <varargs.h> va_start macro, when treated as a builtin. */
      scan_va_start_operator(&local_result, /*single_operand=*/TRUE);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */

    case tok_va_arg:
      /* <stdarg.h> va_arg macro, when treated as a builtin. */
      scan_va_arg_operator(&local_result);
      break;

    case tok_va_end:
      /* <stdarg.h> va_end macro, when treated as a builtin. */
      scan_va_end_operator(&local_result);
      break;

    case tok_va_copy:
      /* <stdarg.h> va_copy macro, when treated as a builtin. */
      scan_va_copy_operator(&local_result);
      break;

#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_uuidof:
      /* Microsoft __uuidof operation. */
      scan_uuidof_operator(&local_result);
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

    case tok_dynamic_cast:
      /* dynamic_cast operation. */
      scan_dynamic_cast_operator(&local_result);
      break;

    case tok_const_cast:
      /* const_cast operation. */
      scan_const_cast_operator(&local_result);
      break;

    case tok_static_cast:
      /* static_cast operation. */
      scan_static_cast_operator(&local_result);
      break;

    case tok_reinterpret_cast:
      /* reinterpret_cast operation. */
      scan_reinterpret_cast_operator(&local_result);
      break;

    case tok_intaddr:
      /* __INTADDR__ operation. */
      scan_intaddr_operator(&local_result);
      break;
    case tok_new:
scan_new:
      /* C++ "new" operator. */
      scan_new_operator(&local_result);
      break;
    case tok_delete:
scan_delete:
      /* C++ "delete" operator. */
      scan_delete_operator(&local_result);
      break;
    case tok_lparen:
      /* This could be a cast operation or just an expression in
         parentheses. */
      /* Come here if a left parenthesis was trapped by the caller. */
handle_trapped_left_paren:
      scan_cast_or_expr(&local_result, &local_bound_function_selector,
                        local_options);
      break;

    case tok_char:
    case tok_short:
    case tok_int:
    case tok_signed:
    case tok_long:
    case tok_unsigned:
    case tok_float:
    case tok_double:
    case tok_void:
    case tok_wchar_t:
    case tok_bool:
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_int8:
    case tok_int16:
    case tok_int32:
    case tok_int64:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    case tok_typename:
      /* In C++, these type keywords begin a functional-notation type
         conversion (ARM 5.2.3).  In C, they're a syntax error. */
      if (C_dialect != C_dialect_cplusplus) goto bad_start_of_primary;
      {
        a_type_ptr cast_type;

        if (curr_token == tok_typename) {
          /* "typename X::Y" is an allowed form of type. */
          typename_specifier(&cast_type, /*within_using_decl=*/FALSE,
                             (a_decl_pos_block_ptr)NULL);
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode) {
          /* The Microsoft compiler allows things like "unsigned int(x)". */
          cast_type = simple_type_specifier_sequence();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          cast_type = type_keyword();
          (void)get_token();
        }  /* if */
        error_position = start_position;
        if (curr_token != tok_lparen) {
          /* No parenthesis following the type, so issue an error. */
          error_and_make_error_operand(ec_type_identifier_not_allowed,
                                       &local_result);
        } else {
          scan_functional_notation_type_conversion(cast_type,
                                                   &start_position,
                                                   &local_result,
                                                   local_options);
        }  /* if */
      }  /* if */
      break;

    case tok_throw:
      scan_throw_operator(&local_result);
      break;
     
    default:
bad_start_of_primary:
      set_err_pos_to_curr_token();
      syntax_error(ec_exp_primary_expr);
      make_error_operand(&local_result);
  }  /* switch */

  /* At the top of the loop, the operand is in "local_result". */
  /* Loop, taking one or more operators of appropriate precedence.
     For example, if the code above has scanned the "2" of "1+2*3*4+5",
     the first iteration of the loop will scan "*3" and assemble "2*3",
     and the second iteration will scan "*4" and assemble "(2*3)*4".
     The loop will then end and this invocation of scan_expr_full will
     return to its caller (also scan_expr_full). */
  for (;;) {
    if (C_dialect == C_dialect_pcc) {
      /* In pcc mode, check for nonstandard assignment operators like "+ =". */
      check_for_pcc_compound_assignment_operators();
    }  /* if */
    /* See if the current token is an operator, and if so, whether it ends
       the current expression given its precedence and associativity. */
    if (token_ends_expr(curr_token, prec_level, local_options)) break;
    /* The operator is to be taken at this level.  Do any necessary
       transformations and error checks on it.  Do NOT obey the options
       flags passed in to this routine in local_options, because
       the expression we have here is not at the top level -- it will be
       placed under an operator. */
    if (is_undefined_symbol_operand(&local_result)) {
      /* Do not allow an undefined symbol to survive unless it is about
         to be called. */
      if (curr_token == tok_lparen) {
        /* The undefined symbol is about to be called, so it's an implicitly
           declared function and therefore okay. */
      } else {
        /* The undefined symbol is about to be the operand of some
           operation other than a call, so it's truly undefined. */
        enter_undefined_symbol(local_result.variant.symbol);
        str_error(ec_undefined_identifier,
                  local_result.variant.symbol->header->identifier);
        make_error_operand(&local_result);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (is_property_ref_operand(&local_result)) {
      /* If the operand is a field selection for a field declared with
         __declspec(property(...)), change it to a call of the appropriate 
        "get" function.  But preserve it for [], ++, --, and assignment
        operators, where the "put" interpretation may apply. */
      switch (curr_token) {
        case tok_plus_plus:
        case tok_minus_minus:
        case tok_lbracket:
        case tok_assign:
        case tok_times_assign:
        case tok_divide_assign:
        case tok_remainder_assign:
        case tok_plus_assign:
        case tok_minus_assign:
        case tok_shift_left_assign:
        case tok_shift_right_assign:
        case tok_and_assign:
        case tok_excl_or_assign:
        case tok_or_assign:
          /* Leave as is. */
          break;
        default:
          rewrite_property_field_reference(&local_result, (an_operand *)NULL);
          break;
      }  /* switch */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    if (local_result.bound_function) {
      /* Do not allow bound functions to survive unless they are about
         to be called. */
      if (curr_token == tok_lparen) {
        /* The bound function is about to be called, so it's okay. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_bugs &&
                 conv_bound_function_to_pointer_to_member(
                                             &local_result,
                                             &local_bound_function_selector)) {
        /* The Microsoft compiler converts a bound function that is not
           called into a pointer-to-member. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* The bound function is about to be the operand of some
           operation other than a call, so there's a problem. */
        error_in_operand(ec_bound_function_must_be_called, &local_result);
        local_result.bound_function = FALSE;
      }  /* if */
    }  /* if */

    /* Copy the operand in "local_result" to "operand" in anticipation of
       creating another intermediate result in "local_result". */
    copy_operand(&local_result, &operand);

    switch (curr_token) {
      case tok_plus_plus:
      case tok_minus_minus:
	/* Postfix increment and decrement. */
        scan_postfix_incr_decr(&operand, &local_result);
	break;
      case tok_lbracket:
	/* Subscript. */
        scan_subscript_operator(&operand, &local_result);
	break;
      case tok_lparen:
	/* Routine call. */
        scan_function_call(&operand, &local_bound_function_selector,
                           &local_result);
	break;
      case tok_period:
      case tok_arrow:
	/* Field selectors. */
	scan_field_selection_operator(&operand, local_options, &local_result,
                                      &local_bound_function_selector);
	break;
      case tok_period_star:
      case tok_arrow_star:
	/* C++ pointer-to-member operators (.* and ->*). */
	scan_ptr_to_member_operator(&operand, &local_result,
                                    &local_bound_function_selector);
        break;
      case tok_star:
      case tok_divide:
      case tok_remainder:
	scan_mult_operator(&operand, &local_result);
	break;
      case tok_plus:
      case tok_minus:
	scan_add_operator(&operand, &local_result);
	break;
      case tok_shift_left:
      case tok_shift_right:
	scan_shift_operator(&operand, &local_result);
	break;
      case tok_lt:
      case tok_gt:
      case tok_le:
      case tok_ge:
	scan_rel_operator(&operand, &local_result);
	break;
      case tok_eq:
      case tok_ne:
	scan_eq_operator(&operand, &local_result);
	break;
      case tok_ampersand:
      case tok_excl_or:
      case tok_or:
	scan_bit_operator(&operand, &local_result);
	break;
      case tok_and_and:
      case tok_or_or:
	scan_logical_operator(&operand, &local_result);
	break;
      case tok_quest_mark:
	scan_conditional_operator(&operand, &local_result);
	break;
      case tok_assign:
        scan_simple_assignment_operator(&operand, &local_result);
	break;
      case tok_times_assign:
      case tok_divide_assign:
      case tok_remainder_assign:
      case tok_plus_assign:
      case tok_minus_assign:
      case tok_shift_left_assign:
      case tok_shift_right_assign:
      case tok_and_assign:
      case tok_excl_or_assign:
      case tok_or_assign:
        scan_compound_assignment_operator(&operand, &local_result);
	break;
      case tok_comma:
	scan_comma_operator(&operand, &local_result);
	break;
#if CHECKING
      default:
        internal_error("scan_expr_full: bad operator token in loop");
#endif /* CHECKING */
    }  /* switch */
  }  /* for */

  /* Set error_position to the start of the expression. */
  copy_source_position(start_position, error_position);
  /* Finished scanning the expression.  Do various error checks.
     Here, we DO respect the flags in local_options; that's what
     they're for. */
  if (is_undefined_symbol_operand(&local_result)) {
    /* An undefined symbol was not followed by a "(", so it is truly
       an undefined symbol and not an implicitly-declared function name.
       Note that "(f)()" will not be treated as an implicit function
       declaration -- it will yield an error.  The standard says
       "expression ... consists solely of an identifier" (3.3.2.2). */
    enter_undefined_symbol(local_result.variant.symbol);
    str_error(ec_undefined_identifier,
              local_result.variant.symbol->header->identifier);
    make_error_operand(&local_result);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_property_ref_operand(&local_result)) {
    if (!(local_options & EOPT_PRESERVE_PROPERTY_REF)) {
      /* If the operand is a field selection for a field declared with
         __declspec(property(...)), change it to a call of the appropriate 
        "get" function. */
      rewrite_property_field_reference(&local_result, (an_operand *)NULL);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Selector_ref_entry_list will be set to the reference entry list for the
     selector object if there is one. */
  selector_ref_entry_list = NULL;
  if (local_result.bound_function) {
    /* Do not allow bound functions to survive unless the caller
       permits it. */
    if (local_options & EOPT_ALLOW_BOUND_FUNCTION) {
      /* Bound function allowed.  Return the operand for the object to
         which the function is bound in *bound_function_selector. */
#if CHECKING
      if (bound_function_selector == NULL) {
        internal_error("scan_expr_full: bound_function_selector == NULL");
      }  /* if */
#endif /* CHECKING */
      copy_operand(&local_bound_function_selector, bound_function_selector);
      selector_ref_entry_list = bound_function_selector->ref_entries_list;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_bugs &&
               conv_bound_function_to_pointer_to_member(
                                             &local_result,
                                             &local_bound_function_selector)) {
    /* The Microsoft compiler converts a bound function that is not
       called into a pointer-to-member. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Bound function not allowed. */
      error_in_operand(ec_bound_function_must_be_called, &local_result);
      local_result.bound_function = FALSE;
    }  /* if */
  }  /* if */

  copy_operand(&local_result, result);

#if GNU_EXTENSIONS_ALLOWED
  if (marked_as_gnu_extension) {
    mark_operand_as_gnu_extension(result);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */

  /* At this point, curr_expr_ref_entries is a list of all the ref entries
     generated in the expression.  There is also a list attached to result,
     of entries for which the reference kind may be affected by context.
     Clearly, any entries on the first list that are not also on the second
     list cannot be changed from here on, so they are removed from the
     global list now and the references they indicate are recorded.
     The references in the selector object (if there is one) are also kept.
     The global list of references is updated. */
  flush_ref_entries_except(result->ref_entries_list, selector_ref_entry_list);
  /* Put the references saved at the beginning of this routine back on
     the front of the global list. */
  if (saved_ref_list != NULL) {
    for (last_rep = saved_ref_list;
         last_rep->next != NULL;
         last_rep = last_rep->next) {}
    last_rep->next = curr_expr_ref_entries;
    curr_expr_ref_entries = saved_ref_list;
  }  /* if */
  db_exit();
}  /* scan_expr_full */


static void process_integer_expression(an_operand *operand,
                                       a_boolean  is_switch_expr)
/*
*operand represents an expression just scanned.  Check that it is integral,
converting from class to integral if necessary.  The expression is the one
in a switch statement if is_switch_expr is TRUE.
*/
{
  a_boolean processed = FALSE;

  /* Convert from a class type to an integer if necessary. */
  if (!C_mode() && is_class_struct_union_type(operand->type)) {
    a_builtin_type_kind_set type_kind_set = BTK_INTEGRAL;
    if (is_switch_expr) type_kind_set |= BTK_ENUM;
    try_to_convert_class_operand_to_builtin_type(operand, type_kind_set,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Non-class (i.e., normal) case. */
    do_operand_transformations(operand, TOPT_NO_OPTIONS);
    /* Can't check the type of a template parameter in a prototype
       instantiation. */
    if (!is_template_param_type(operand->type)) {
      (void)check_integral_or_enum_operand(operand);
    }  /* if */
  }  /* if */
  if (is_switch_expr) {
    /* A switch expression gets special processing. */
    if (C_dialect != C_dialect_pcc) {
      /* ANSI C or C++: the normal integral promotions are done. */
      /* Note that for the C++ condition declaration case the promotion
         is done elsewhere as part of the condition processing. */
      promote_operand(operand);
    } else {
      /* pcc treats all switch expressions as int.  This differs from
         ANSI C in that even long is cast to int. */
      cast_operand(integer_type((an_integer_kind)ik_int), operand,
                   /*check_cast_access=*/TRUE, /*is_implicit_cast=*/TRUE,
                   /*is_reinterpret_cast=*/FALSE,
                   /*reinterpret_semantics=*/FALSE);
    }  /* if */
  }  /* if */
}  /* process_integer_expression */


an_expr_node_ptr scan_integer_expression(a_boolean is_switch_expr)
/*
Scan an integral expression, e.g., the selector expression for a switch
statement, and return a pointer to the expression tree.  is_switch_expr
is TRUE if this is the expression in a switch statement.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_integer_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/TRUE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  /* Check that the expression is integral or convertible to an integral
     type. */
  process_integer_expression(&result, is_switch_expr);
  expression = make_node_from_operand(&result);
  expression = wrap_up_full_expression(expression);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_integer_expression */


an_expr_node_ptr scan_void_expression(a_boolean repeated_in_loop,
                                      a_boolean marked_as_gnu_extension,
                                      a_boolean is_statement_expr)
/*
Scan a "void expression," i.e., one whose value is discarded.  This is
used for expression statements, the increment expression of a "for", etc.
repeated_in_loop is TRUE for an expression repeated in a loop (e.g.,
the increment of a "for").  This routine is not used for constant
or not-evaluated expressions.  This routine should only be used to
scan full expressions (except for the GNU C statement expression case).
If marked_as_gnu_extension is TRUE, the upcoming expression was
preceded by the GNU __extension__ keyword.  is_statement_expr is
TRUE if this expression is being scanned as a statement inside a
GNU C statement expression.  Issue a warning for an expression that has
no side effects unless this expression is the last in a statement
expression.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           result_used = FALSE;

  db_enter(3, "scan_void_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/repeated_in_loop,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST,
            marked_as_gnu_extension ? EOPT_MARKED_AS_GNU_EXTENSION
                                    : EOPT_NO_OPTIONS);
  if (is_statement_expr &&
      curr_token == tok_semicolon &&
      next_token() == tok_rbrace) {
    /* This is the last statement in a GNU C statement expression.
       As such, it is the value of the expression. */
    result_used = TRUE;
  }  /* if * */
  if (!result_used) {
    simplify_void_operand(&result);
  } else {
    do_void_operand_transformations(&result);
  }  /* if */
  expression = make_node_from_void_expression_operand(&result);
  expression = wrap_up_full_expression(expression);
  /* Indicate that the value of the node is not used. */
  if (!result_used) set_expr_result_not_used(expression);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_void_expression */


an_expr_node_ptr scan_typed_expression(a_type_ptr         required_type,
                                       an_error_code      err_code)
/*
Scan a top-level expression and convert it to the type required_type;
issue the error err_code if it cannot be converted to that type.
Return a pointer to the expression.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_typed_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);

  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type, (a_boolean *)NULL,
                           (a_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           /*initializing_variable=*/FALSE,
                           /*static_lifetime=*/FALSE,
                           /*is_copy_initialization=*/FALSE,
                           /*processed_arg=*/FALSE,
                           /*nontype_template_arg=*/FALSE,
                           err_code);
  expression = make_node_from_operand(&result);
  expression = wrap_up_full_expression(expression);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3 && expression != NULL) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_typed_expression */


void scan_default_arg_expr(a_param_type_ptr ptp)
/*
Scan a default argument expression on a formal parameter declaration, change
its type as required by the type of the formal parameter, and attach the
expression node to the param type entry.  If an error is detected in the
expression scan, an error node is assigned.  If ptp is NULL (as the result of
a prior error, or when passing over a default argument expression, e.g.,
in a template instantiation) just do the scan.
*/
{
  an_operand              result;
  an_expr_node_ptr        node;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  db_enter(3, "scan_default_arg_expr");
  /* Save, clear, and later restore the expression stack, since this expression
     is not part of any expression we may currently be inside of.  Note
     that push_scope cleared the object lifetime stack on pushing the
     prototype scope. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/TRUE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.is_default_arg_expression = TRUE;
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST,
            EOPT_NO_OPTIONS | EOPT_DISALLOW_COMMA_OPERATOR);
  if (ptp != NULL) {
    /* Convert to the required type. */
    prep_argument_operand(&result, ptp, /*processed_arg=*/FALSE,
                          (a_conv_descr_ptr)NULL,
                          ec_bad_default_arg_type);
  } else {
    do_operand_transformations(&result, TOPT_NO_OPTIONS);
  }  /* if */
  node = make_node_from_operand(&result);
  if (ptp == NULL ||
      /* Eliminate object lifetimes in prototype instantiations if
         the instantiation is not being saved in the tree. */
      (!prototype_instantiations_in_il &&
       is_template_dependent_context())) {
    discard_curr_expr_object_lifetime();
  }  /* if */
  node = wrap_up_full_expression(node);
  if (ptp != NULL) ptp->default_arg_expr = node;
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  restore_expr_stack(saved_expr_stack);
#if DEBUG
  if (debug_level >= 3) {
    db_expression(node);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_default_arg_expr */


static void fix_up_dynamic_init_dtors(void)
/*
Process the fixup list of dynamic initializations attached to the current
level of the expression stack.  The dynamic initializations on the list
are ones whose destructor processing could not be completed when the
dynamic init entry was created because the initialization occurs within
an expression that is subject to the copy constructor elision optimization.
The destructor call on the topmost initialization is optimized away,
but there's no way to know that when it is generated, so all such
initializations are put in the dynamic init entries but the destructor
routines are not marked as referenced.  The dynamic init entries are
placed on a list and here the remaining referenced destructor routines
are marked as actually referenced.
*/
{
  a_dynamic_init_dtor_fixup_ptr didfp, didfp_next;
  a_routine_ptr                 dtor_routine;

  didfp = expr_stack->dynamic_init_dtor_fixup_list;
  expr_stack->dynamic_init_dtor_fixup_list = NULL;
  for (; didfp != NULL; didfp = didfp_next) {
    didfp_next = didfp->next;
    dtor_routine = didfp->dynamic_init->destructor;
    if (dtor_routine != NULL) {
      a_symbol_ptr dtor_sym =
                         (a_symbol_ptr)dtor_routine->source_corresp.assoc_info;
      /* Check access to the destructor and mark it referenced. */
      expr_reference_to_implicitly_invoked_function(
                                dtor_sym,
                                &didfp->position,
                                dtor_routine->source_corresp.parent.class_type,
                                /*honor_virtual=*/FALSE);
    }  /* if */
    /* Free the one entry. */
    free_dynamic_init_dtor_fixup(didfp);
  }  /* for */
}  /* fix_up_dynamic_init_dtors */


static void check_return_value_optimization(an_operand *operand)
/*
A return statement is returning the indicated operand in a function that
returns its value via a copy constructor.  Check to see if return value
optimization is or continues to be possible.  Return value optimization
is possible when all return statements in a function return the same nonstatic
local variable; the optimization is to rewrite all references to the local
variable as references to the return-value address passed by the caller,
thus avoiding a copy constructor call on exit.  Note that the front end
just discovers that the optimization is possible; it is left to IL
lowering or a back end to do the rewriting.
*/
{
  a_scope_stack_entry_ptr ssep = &scope_stack[depth_innermost_function_scope];
  a_scope_ptr             func_scope = ssep->il_scope;

  if (ssep->return_value_optimization_possible) {
    a_boolean possible = FALSE;
    /* If return value optimization is possible for this routine, see
       if this return expression invalidates it.  Return value optimization
       is possible if all return statements in the function return the
       same nonstatic local variable. */
    if (!is_expression_operand(operand) ||
        !is_an_lvalue(operand) ||
        !is_variable_address_node(operand->variant.expression)) {
      /* The expression is not a simple variable, so the optimization is no
         longer possible. */
      /* possible = FALSE; -- already set. */
    } else {
      a_variable_ptr return_var= operand->variant.expression->variant.variable;
      a_variable_ptr opt_var =
                             func_scope->variant.routine.return_value_variable;
      if (opt_var != NULL) {
        /* Previous return statements have returned the variable "opt_var".
           See if this return statement does also. */
        if (opt_var == return_var) possible = TRUE;
      } else {
        /* There have been no previous return statements, so this statement
           can establish the local variable involved in the optimization.
           It must be a nonstatic variable of the right type in the top
           scope of the function (the latter is what cfront does, and it
           helps to avoid some nasty interactions with exception handling). */
        a_type_ptr func_type = func_scope->variant.routine.ptr->type;
        func_type = skip_typerefs(func_type);
        if (!return_var->is_parameter &&
            return_var->storage_class != (a_storage_class)sc_static &&
            types_are_compatible(return_var->type,
                                 func_type->variant.routine.return_type)) {
          a_symbol_ptr sym =
                           (a_symbol_ptr)return_var->source_corresp.assoc_info;
          if (sym->decl_scope == ssep->number) {
            /* This variable is okay.  Record it as the variable for the
               return value optimization. */
            possible = TRUE;
            func_scope->variant.routine.return_value_variable = return_var;
#if DEBUG
            if (debug_level >= 3) {
              fprintf(f_debug, "Return value optimization variable = %s\n",
                      return_var->source_corresp.name);
            }  /* if */
#endif /* DEBUG */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (!possible) {
      /* The return value optimization has been ruled out. */
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "Return value optimization ruled out.\n");
      }  /* if */
#endif /* DEBUG */
      ssep->return_value_optimization_possible = FALSE;
      func_scope->variant.routine.return_value_variable = NULL;
    }  /* if */
  }  /* if */
}  /* check_return_value_optimization */


static void check_for_return_of_address_of_local_variable(
                                                    an_expr_node_ptr  expr,
                                                    a_source_position *err_pos)
/*
The given expression is the expression on a return statement.  Issue
a warning if the value returned is the address of a local variable.
*err_pos gives the source position for the warning.
*/
{
  an_error_code err_code = ec_no_error;

  /* Remove casts and lvalue field selections. */
  while (is_operation_node(expr) &&
         (expr->variant.operation.kind ==
                                  (an_expr_operator_kind)eok_cast ||
          expr->variant.operation.kind ==
                                  (an_expr_operator_kind)eok_base_class_cast ||
          expr->variant.operation.kind ==
                                  (an_expr_operator_kind)eok_field)) {
    expr = expr->variant.operation.operands;
  }  /* while */
  if (is_variable_address_node(expr)) {
    /* Check for the address of a variable that is non-static. */
    a_variable_ptr var = expr->variant.variable;
    if (!has_static_storage_duration(var->storage_class)) {
      err_code = ec_returning_ptr_to_local_variable;
    }  /* if */
  } else if (is_constant_node(expr)) {
    /* Check for a constant that is the address of a non-static variable. */
    a_constant_ptr con = expr->variant.constant;
    if (con->kind == (a_constant_repr_kind)ck_address &&
        con->variant.address.kind == (an_address_base_kind)abk_variable) {
      a_variable_ptr var = con->variant.address.variant.variable;
      if (!has_static_storage_duration(var->storage_class)) {
        err_code = ec_returning_ptr_to_local_variable;
      }  /* if */
    }  /* if */
  } else if (expr->kind == (an_expr_node_kind)enk_temp_init) {
    /* Check for the address of a temporary (e.g., the address of a compound
       literal in C99 mode). */
    if (expr->variant.init.result_is_addr &&
        !expr->variant.init.static_temp) {
      err_code = ec_returning_ptr_to_local_temp;
    }  /* if */
  }  /* if */
  if (err_code != ec_no_error) { 
    /* Issue the warning. */
    pos_warning(err_code, err_pos);
  }  /* if */
}  /* check_for_return_of_address_of_local_variable */


an_expr_node_ptr scan_return_expression(a_type_ptr         required_type,
                                        an_error_code      err_code,
                                        a_dynamic_init_ptr *dip)
/*
Scan an expression on a return statement and convert it to the type
required_type; issue the error err_code if it cannot be converted to that
type.  Return a pointer to the expression.  If the current routine is
one that returns its value via a copy constructor, set *dip to point to
the appropriate dynamic initialization entry and return NULL.
required_type will be void if the expression should have void type
(e.g., in a C++ function with void return type).
*/
{
  a_routine_ptr       curr_routine = current_routine_entry();
  a_type_ptr          routine_type;
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           return_by_cctor_case, void_return_case = FALSE;

  db_enter(3, "scan_return_expression");

  *dip = NULL;
  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  return_by_cctor_case = FALSE;
  routine_type = skip_typerefs(curr_routine->type);
  if (routine_type->variant.routine.extra_info->value_returned_by_cctor) {
    /* The current routine returns its value via a copy constructor. */
    return_by_cctor_case = TRUE;
    expr_stack->in_cctor_elision_initializer = TRUE;
  }  /* if */
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  if (return_by_cctor_case) {
    /* The current routine returns its value via a copy constructor. */
    /* Check for the possibility of the return value optimization. */
    check_return_value_optimization(&result);
    /* Build a dynamic initialization entry for the return statement. */
    prep_elision_initializer_operand(&result, required_type,
                                     /*fill_in_dtor=*/FALSE, err_code, dip);
    wrap_up_dynamic_init_full_expression(*dip);
    /* Fix up destructor references in the overall expression. */
    fix_up_dynamic_init_dtors();
    expression = NULL;
  } else {
    /* Normal case. */
    if (is_void_type(required_type)) {
      /* A void expression is expected. */
      void_return_case = TRUE;
      /* We don't use simplify_void_operand here on purpose.  We don't want
         to remove an explicit cast to void, and we don't want to issue a
         warning on an expression with no side effects. */
      do_void_operand_transformations(&result);
      expression = make_node_from_void_expression_operand(&result);
      if (microsoft_mode && C_mode()) {
        /* The type is not checked in Microsoft C mode. */
      } else if (gcc_mode) {
        /* In GNU C mode a type mismatch results in a warning only. */
        if (!is_void_type(result.type)) {
          pos_warning(err_code, &result.position);
        }  /* if */
      } else {
        /* Check that the expression has void type. */
        if (!is_void_type(result.type) &&
            !is_template_param_type(result.type)) {
          if (!is_error_operand(&result)) {
            error_in_operand(err_code, &result);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Convert to the required type. */
      prep_initializer_operand(&result, required_type, 
                               (a_boolean *)NULL,
                               (a_conv_descr_ptr)NULL,
                               /*initializing_return_value=*/TRUE,
                               /*initializing_variable=*/FALSE,
                               /*static_lifetime=*/FALSE,
                               /*is_copy_initialization=*/TRUE,
                               /*processed_arg=*/FALSE,
                               /*nontype_template_arg=*/FALSE,
                               err_code);
      expression = make_node_from_operand(&result);
      if (!is_reference_type(required_type)) {
        check_for_return_of_address_of_local_variable(expression,
                                                      &result.position);
      }  /* if */
    }  /* if */
    expression = wrap_up_full_expression(expression);
    if (void_return_case) set_expr_result_not_used(expression);
  }  /* if */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3 && expression != NULL) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_return_expression */


void scan_pp_expression(a_constant *constant)
/*
Scan a pre-processor expression.  See sections 3.4 and 3.8.1 in the standard.
*/
{
  an_operand              result;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  db_enter(3, "scan_pp_expression");
  /* Save the current expr_stack for later restoration, and start over, because
     this pp expression is not part of any expression we happen to be inside
     of. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_pp, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_pp_expression */


void scan_integral_constant_expression(a_constant *constant)
/*
Scan an integral constant expression.  See section 6.4 in the ISO C89 standard,
and [expr.const] in the ISO C++98 standard.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_integral_constant_expression");

  push_expr_stack((an_expression_kind)ek_integral_constant, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_integral_constant_expression */


void scan_fs_integral_constant_expression(a_constant *constant)
/*
Scan an integral constant expression.  The constant will be allocated
(by the caller) in the file scope memory region, so switch to the file
scope while scanning the constant, so that anything allocated during
the scan will be allocated in the file scope memory region.  (This is
significant when RECORD_CONSTANT_EXPRESSIONS_IN_IL is TRUE; we want the
expression for the constant to be in the file scope memory region so
that the constant can point to it).
*/
{
  a_memory_region_number  region_to_switch_back_to;

  switch_to_file_scope_region(&region_to_switch_back_to);
  scan_integral_constant_expression(constant);
  switch_back_to_original_region(region_to_switch_back_to);
}  /* scan_fs_integral_constant_expression */


void scan_nonconstant_dimension_expression(a_boolean        is_vla_decl,
                                           a_boolean        *is_constant,
                                           an_expr_node_ptr *expression,
                                           a_constant       *constant)
/*
Scan an array dimension that might be non-constant.  The array dimension
must be an integral expression.  (It's also required to be non-negative, but
the caller must check that.)  If is_vla_decl is FALSE, this function is being
called in C++ mode for new-type-name (see 5.3.3 in the ARM); if is_vla_decl
is TRUE it is being called for array dimensions which may be VLAs.  Return
either *is_constant TRUE and a constant value in *constant, or *is_constant
FALSE and a pointer to the expression tree in *expression.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  int                 constant_sign;
  a_boolean           processed = FALSE;
  an_expression_kind  ekind;

  db_enter(3, "scan_nonconstant_dimension_expression");

  ekind = (an_expression_kind)ek_normal;
  /* When a dimension bound expression is scanned inside a constant expression,
     it must be an integral constant. */
  if (expr_stack != NULL && curr_expr_kind_is_const()) {
    ekind = (an_expression_kind)ek_integral_constant;
  }  /* if */
  push_expr_stack(ekind, &expr_stack_entry, /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  if (is_vla_decl) expr_stack_entry.is_vla_dimension_expression = TRUE;
  /* Scan the expression. */
  if (c99_mode) {
    /* In C99 the expression is an assignment_expression. */
    scan_expr(&result, PREC_COMMA, EOPT_DISALLOW_COMMA_OPERATOR);
  } else {
    scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  }  /* if */
  /* Convert from a class type to integral if necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result,
                                                 (a_builtin_type_kind_set)
                                                     (BTK_INTEGRAL | BTK_ENUM),
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Do lvalue --> rvalue and other transformations for the non-overloaded
       case. */
    do_operand_transformations(&result, TOPT_NO_OPTIONS);
  }  /* if */
  /* Check that the expression is integral or enum. */
  if (!is_template_param_type(result.type)) {
    (void)check_integral_or_enum_operand(&result);
  }  /* if */
  /* Return a constant or expression depending on what was scanned. */
  *is_constant = TRUE;
  switch (result.kind) {
    case ok_error:
      /* Some sort of error; message was already issued. */
      set_error_constant(constant);
      discard_curr_expr_object_lifetime();
      break;
    case ok_expression:
      *expression = result.variant.expression;
      *expression = wrap_up_full_expression(*expression);
      *is_constant = FALSE;
      break;
    case ok_constant:
      /* Constant.  The constant must be non-negative.  If it is zero,
         it is rendered as an expression.  For a VLA, the size must
         be positive. */
      copy_constant(&result.variant.constant, constant);
      if (constant->kind != (a_constant_repr_kind)ck_integer) {
        if (!is_error_constant(constant)) {
          /* This case can occur with expressions like (int)&x which are
             represented as constants but aren't known until link time. */
          *expression = alloc_node_for_constant(constant);
          *is_constant = FALSE;
        }  /* if */
      } else {
        constant_sign = sign_of_integer_constant(constant);
        if (is_vla_decl) {
          if (constant_sign < 0) {
            /* VLA bounds must be positive (or possibly zero in GNU C mode;
               the zero-length case is checked by the caller). */
            error(ec_array_size_must_be_positive);
            set_error_constant(constant);
          }  /* if */
        } else if (constant_sign < 0) {
          /* A negative value is an error. */
          error(ec_new_array_size_must_be_nonnegative);
          set_error_constant(constant);
        } else if (constant_sign == 0) {
          /* A zero value is returned as an expression to avoid confusing
             array [] and array [0]. */
          *expression = alloc_node_for_constant(constant);
          *is_constant = FALSE;
        }  /* if */
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error(
               "scan_nonconstant_dimension_expression: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    if (*is_constant) {
      db_constant(constant);
    } else {
      db_expression(*expression);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_nonconstant_dimension_expression */


static void prep_nontype_template_argument_initializer(an_operand *operand,
                                                       a_type_ptr param_type,
                                                       a_constant *constant)
/*
operand points to an operand for a nontype template argument expression.
Convert it to the template parameter type param_type, and put a constant
for the converted result in *constant.  Do various error checks.
*/
{
  db_enter(3, "prep_nontype_template_argument_initializer");
  if (microsoft_mode && is_pointer_type(param_type) &&
      is_an_lvalue(operand) && is_constant_operand(operand) &&
      identical_types(operand->type, param_type)) {
    /* In Microsoft mode, an lvalue of type pointer to X can be used
       as the actual argument for a nontype template parameter of type
       pointer to X. */
    /* Make a constant from the operand. */
    extract_constant_from_operand(operand, constant);
    constant->type = make_reference_type(param_type);
  } else {
    /* Convert to the required type if necessary.  Do not use user-defined
       conversions. */
    prep_initializer_operand(operand, param_type, (a_boolean *)NULL,
                             (a_conv_descr_ptr)NULL,
                             /*initializing_return_value=*/FALSE,
                             /*initializing_variable=*/FALSE,
                             /*static_lifetime=*/FALSE,
                             /*is_copy_initialization=*/TRUE,
                             /*processed_arg=*/FALSE,
                             /*nontype_template_arg=*/TRUE,
                             ec_bad_nontype_template_arg);
    /* Make a constant from the operand. */
    extract_constant_from_operand(operand, constant);
    /* If the template parameter has a reference type, give the constant
       a reference type (instead of the pointer type it has). */
    if (is_reference_type(param_type) && !is_error_operand(operand)) {
      check_assertion(is_pointer_type(constant->type));
      constant->type = param_type;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* prep_nontype_template_argument_initializer */


void scan_template_argument_constant_expression(a_type_ptr param_type,
                                                a_constant *constant)
/*
Scan a constant argument in a template reference.  Issue an error if it
is incompatible with the corresponding parameter type, param_type.
Return the constant in *constant.  If param_type is NULL, the
parameter type is not known.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_memory_region_number region_to_switch_back_to;

  db_enter(3, "scan_template_argument_constant_expression");

  push_expr_stack((an_expression_kind)ek_template_arg, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.is_template_arg_expression = TRUE;
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type if necessary.  Do not use user-defined
     conversions. */
  if (param_type != NULL) {
    prep_nontype_template_argument_initializer(&result, param_type, constant);
  } else {
    /* No destination type.  Make a constant from the operand.  This comes
       up for errors and for nonreal templates in prototype instantiations. */
    if (is_template_dependent_context()) {
      prep_generic_nontype_template_argument(&result);
    } else {
      /* Error recovery. */
      check_assertion(total_errors != 0);
      error_if_indefinite_function(&result);
      if (is_sym_for_member_operand(&result)) {
        /* Replace a symbol-for-member operand by a pointer-to-member. */
        conv_sym_for_member_operand_to_ptr_to_member(&result);
      }  /* if */
    }  /* if */
    extract_constant_from_operand(&result, constant);
  }  /* if */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  /* There is no point in recording the constant expression for a nontype
     template argument, because it can differ from one instantiation point to
     another, and we can record only one expression. */
  constant->expr = NULL;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */

  switch_back_to_original_region(region_to_switch_back_to);
#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_template_argument_constant_expression */


an_arg_operand_ptr scan_nontype_template_argument(void)
/*
Scan a nontype template argument in a template reference.  Allocate
an arg_operand entry, fill it with information about the template
argument, and return a pointer to it to the caller.  The caller must
at some later point call free_arg_operand_list to free the entry.
*/
{
  an_arg_operand_ptr     arg_operand;
  an_expr_stack_entry    expr_stack_entry;
  a_memory_region_number region_to_switch_back_to;

  db_enter(3, "scan_nontype_template_argument");

  push_expr_stack((an_expression_kind)ek_template_arg, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.is_template_arg_expression = TRUE;
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Scan the constant expression. */
  arg_operand = alloc_arg_operand();
  scan_expr(&arg_operand->operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Don't do final processing on the attached cross-reference entries.
     They are given to the caller. */
  curr_expr_ref_entries = NULL;
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = arg_operand->operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_operand(&arg_operand->operand);
  }  /* if */
#endif /* DEBUG */
  switch_back_to_original_region(region_to_switch_back_to);
  db_exit();
  return arg_operand;
}  /* scan_nontype_template_argument */


a_boolean nontype_template_arg_is_compatible_with_param_type(
                                                an_arg_operand_ptr arg_operand,
                                                a_type_ptr         param_type)
/*
arg_operand points to an argument operand for a nontype template argument
expression previously scanned by scan_nontype_template_argument.  Determine
whether the expression can match a template parameter of type param_type,
and return TRUE if so.  This is callable from outside of the expression
processing routines.
*/
{
  a_boolean           compatible;
  an_operand          operand;
  an_expr_stack_entry expr_stack_entry;

  push_expr_stack((an_expression_kind)ek_template_arg, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.is_template_arg_expression = TRUE;
  /* Don't do anything with references on this operand, since this is only
     exploratory. */
  operand = arg_operand->operand;
  operand.ref_entries_list = NULL;
  compatible = nontype_template_arg_conversion_possible(&operand,
                                                        param_type);
  pop_expr_stack();
  return compatible;
}  /* nontype_template_arg_is_compatible_with_param_type */


static void copy_nontype_template_arg_operand(an_arg_operand_ptr arg_operand,
                                              an_operand         *operand)
/*
arg_operand is an_arg_operand for a nontype template argument.  Make a
copy of it in *operand.  If the operand has attached reference entries,
make a copy of the list for use in the current processing.  This is
necessary because the kinds of reference made to the template argument
differ depending on the type of the template parameter against which it
is matched up (e.g., a reference parameter uses the address of the
argument instead of the value).
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  check_assertion_str(arg_operand->operand.kind !=
                                              (an_operand_kind)ok_property_ref,
            "copy_nontype_template_arg_operand: escaped property-ref operand");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  *operand = arg_operand->operand;
  /* Copy the list of references, if any. */
  operand->ref_entries_list =
                    copy_ref_entry_list(arg_operand->operand.ref_entries_list);
}  /* copy_nontype_template_arg_operand */


void conv_nontype_template_arg_to_param_type(an_arg_operand_ptr arg_operand,
                                             a_type_ptr         param_type,
                                             a_constant         *constant)
/*
arg_operand points to an argument operand for a nontype template argument
expression previously scanned by scan_nontype_template_argument.  Convert it
to the template parameter type param_type, and put a constant for the
converted result in *constant.  This is callable from outside of the
expression processing routines.
*/
{
  an_operand          operand;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "conv_nontype_template_arg_to_param_type");

  push_expr_stack((an_expression_kind)ek_template_arg, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack_entry.is_template_arg_expression = TRUE;
  copy_nontype_template_arg_operand(arg_operand, &operand);
  /* Convert the operand to the parameter type and extract a constant. */
  prep_nontype_template_argument_initializer(&operand,
                                             param_type, constant);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* conv_nontype_template_arg_to_param_type */


void scan_member_constant_initializer_expression(a_type_ptr required_type,
                                                 a_constant *constant)
/*
Scan a member constant initializer expression.  Convert the constant to
required_type (an integral or enumeration type, or an error or template
type); issue an error if it is incompatible with that type.  Used in C++
for scanning member constants in classes (in the standard form).  Assumes
copy-initialization ("="-form).
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_member_constant_initializer_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_integral_constant, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type, (a_boolean *)NULL,
                           (a_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           /*initializing_variable=*/TRUE,  /* Arbitrary. */
                           /*static_lifetime=*/FALSE,
                           /*is_copy_initialization=*/TRUE,
                           /*processed_arg=*/FALSE,
                           /*nontype_template_arg=*/FALSE,
                           ec_bad_initializer_type);
  /* Make a constant from the operand. */
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_member_constant_initializer_expression */


void scan_constant_initializer_expression(a_type_ptr required_type,
                                          a_constant *constant)
/*
Scan a constant initializer expression.  Convert the constant to
required_type; issue an error if it is incompatible with that type.
See section 3.4 in the ANSI C standard.  Used in C++ for scanning
nonstandard class member constants.  Assumes copy-initialization
("="-form).
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_constant_initializer_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_init_constant, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  if (is_array_type(required_type) && is_array_type(result.type)) {
    /* If the initializer is a string literal we will need access to the
       string constant. */
    a_boolean  string_literal_case = is_string_type(result.type) &&
                                  result.kind == (an_operand_kind)ok_constant;
    a_constant_ptr  string_con = NULL;
    if (string_literal_case) {
      string_con = &result.variant.constant;
      if (is_an_lvalue(&result)) {
        /* The operand represents a &"..." form: strip the ck_address constant
           to recover the plain string literal. */
        check_assertion(string_con->kind == (a_constant_repr_kind)ck_address &&
                        string_con->variant.address.kind ==
                                           (an_address_base_kind)abk_constant);
        string_con = string_con->variant.address.variant.constant;
      }  /* if */
    }  /* if */
    check_assertion(gcc_mode || is_string_type(result.type));
    if ((string_literal_case &&
         !check_string_constant_initializer(&required_type, string_con)) ||
        (!string_literal_case &&
         !types_are_compatible(result.type, required_type))) {
      pos_ty2_error(ec_bad_initializer_type, &result.position,
                    result.type, required_type);
      conv_to_error_operand(&result);
    }  /* if */
    /* Make a constant from the operand. */
    if (is_an_lvalue(&result)) {
      /* Use the previously computed string_con. */
      check_assertion(string_literal_case);
      copy_constant(string_con, constant);
    } else {
      /* The operand could be a constant or an error. */
      extract_constant_from_operand(&result, constant);
    }  /* if */
  } else {
    /* Convert to the required type. */
    prep_initializer_operand(&result, required_type, (a_boolean *)NULL,
                             (a_conv_descr_ptr)NULL,
                             /*initializing_return_value=*/FALSE,
                             /*initializing_variable=*/TRUE,  /* Arbitrary. */
                             /*static_lifetime=*/FALSE,
                             /*is_copy_initialization=*/TRUE,
                             /*processed_arg=*/FALSE,
                             /*nontype_template_arg=*/FALSE,
                             ec_bad_initializer_type);
    /* The operand could be a constant or an error. */
    extract_constant_from_operand(&result, constant);
  }  /* if */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_constant_initializer_expression */


void scan_initializer_expression(a_type_ptr       required_type,
                                 a_boolean        static_lifetime,
                                 a_boolean        force_object_lifetime,
                                 a_boolean        is_copy_initialization,
                                 a_boolean        *is_constant,
                                 an_expr_node_ptr *expression,
                                 a_constant       *constant)
/*
Scan an initializer expression.  See sections 3.4 and 3.5.7 in the standard.
The expression is converted to required_type; an error is issued if it
is incompatible with that type.  The entity being initialized has static
lifetime if static_lifetime is TRUE.  Force an object lifetime around the
expression if force_object_lifetime is TRUE.  This initialization is
copy-initialization ("="-form) if is_copy_initialization is TRUE; otherwise,
it is direct_initialization ("()"-form).  The expression can be constant
or nonconstant; on return, *is_constant is set accordingly, and the result
is returned either in *expression or in *constant.  Note that the
required_type may not be an array type.  This routine is not used when
copy constructor elision is possible; see scan_class_initializer_expression
and scan_aggregate_initializer_expression.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_initializer_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  force_object_lifetime,
                  /*suppress_object_lifetime=*/FALSE);
  if (static_lifetime) {
    /* In initializations of static variables, fold constant addressing
       expressions to constants so that constant initialization can be
       more easily discerned. */
    expr_stack->fold_constant_addr_exprs = TRUE;
  }  /* if */
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* Check for a bug related to null pointer constants in Microsoft C mode. */
  process_microsoft_null_pointer_constant_bug(&result, required_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type, (a_boolean *)NULL,
                           (a_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           /*initializing_variable=*/TRUE,
                           static_lifetime,
                           is_copy_initialization,
                           /*processed_arg=*/FALSE,
                           /*nontype_template_arg=*/FALSE,
                           ec_bad_initializer_type);
  /* Return a constant or expression depending on what was scanned. */
  *is_constant = TRUE;
  switch (result.kind) {
    case ok_error:
      /* Some sort of error; message was already issued. */
      set_error_constant(constant);
      discard_curr_expr_object_lifetime();
      break;
    case ok_expression:
      *expression = result.variant.expression;
      *expression = wrap_up_full_expression(*expression);
      *is_constant = FALSE;
      break;
    case ok_constant:
      copy_constant(&result.variant.constant, constant);
      break;
#if CHECKING
    default:
      internal_error("scan_initializer_expression: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    if (*is_constant) {
      db_constant(constant);
    } else {
      db_expression(*expression);
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_initializer_expression */


an_expr_node_ptr prep_rvalue_arg_expr(an_expr_node_ptr  expr,
                                      a_param_type_ptr  param,
                                      a_source_position *err_pos)
/*
expr (an rvalue) is the actual argument of a call corresponding to the
formal parameter param.  Change its type appropriately and return a
pointer to the updated expression.  If an error is detected, use
err_pos as the error position.
*/
{
  an_operand              operand;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Even though this is not an expression scan, make sure the expr_stack
     has something on it.  If there is already something on the stack,
     save it, clear the stack, and restore it later. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Make an operand for the expression. */
  make_expression_operand(expr, expr->type, &operand);
  operand.position = *err_pos;
  /* Do the conversion. */
  prep_argument_operand(&operand, param, /*processed_arg=*/FALSE,
                        (a_conv_descr_ptr)NULL,
                        ec_incompatible_param);
  /* Make an expression again. */
  expr = make_node_from_operand(&operand);
  expr = wrap_up_full_expression(expr);
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
  return expr;
}  /* prep_rvalue_arg_expr */


a_boolean scan_class_initializer_expression(a_type_ptr         required_type,
                                            a_dynamic_init_ptr *dip)
/*
Scan an expression that is the initial value of an entity of class type.
required_type indicates the class type (it may have some qualifiers on top
of it).  When there is no error build a dynamic initialization entry that
describes the initialization to be done (placing the pointer to it in *dip)
and return TRUE.  If there is an error return FALSE.  This routine is used
in both C and C++, but it exists to allow copy constructor elision in C++
cases:

  struct A { A(int) {...} A(const A&) {...} };
  A x = 1;            // A::A(int)
  A y[3] = {1, 2, 3}; // A::A(int) three times
  A z = x;            // A::A(const A&)

As indicated, this is initialization with the "=" semantics
(copy-initialization).
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           okay = TRUE;

  db_enter(3, "scan_class_initializer_expression");
  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Find out whether or not the conversion is possible, and
     build a dynamic initialization entry to describe the initialization. */
  prep_elision_initializer_operand(&result, required_type,
                                   /*fill_in_dtor=*/TRUE,
                                   ec_bad_initializer_type, dip);
  wrap_up_dynamic_init_full_expression(*dip);
  /* *dip == NULL means there was an error. */
  if (*dip == NULL) okay = FALSE;
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
  return okay;
}  /* scan_class_initializer_expression */


a_boolean scan_aggregate_initializer_expression(
                                            a_type_ptr         required_type,
                                            a_boolean          static_lifetime,
                                            unsigned long      *levels_down,
                                            a_boolean          *is_constant,
                                            a_dynamic_init_ptr *dip,
                                            a_constant         *constant)
/*
Scan an expression that is the initial value of an entity of
aggregate type.  required_type indicates the type (it may have some
qualifiers on top of it).  This is copy-initialization.  static_lifetime
is TRUE if the variable being initialized has static lifetime.  Either
create a dynamic initialization entry and return a pointer to it in
*dip (along with *is_constant FALSE), or set *constant to a constant
value (along with *is_constant TRUE).  This routine exists to deal
with initialization of aggregate class types, but it can be called for
non-aggregate class types as well.  It also handles certain array
initialization cases.

The initializer for an aggregate can initialize either the whole
aggregate or the first member of the aggregate (or its first member, etc.).
This routine compares the type of the initializer expression to the type
of the aggregate, then its first member, etc. to determine which entity
should be initialized.  *levels_down is set to indicate the number of
levels down at which the expression was matched up (zero indicates the
aggregate itself).  The initializer expression undergoes appropriate
conversions to make it match up with the entity being initialized.
If the conversion cannot be done, an error is issued and FALSE is returned.

This routine is called to initialize a sub-aggregate, so the destructor
pointer in the dynamic initialization is not set.  The caller must set
it to indicate destruction for a partially-constructed aggregate (on
a thrown exception) if that is appropriate.

This routine is also called in C99 and GNU C modes.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           okay = TRUE, ambiguous;
  a_boolean           string_case = FALSE;
  a_conv_descr        conversion;
  an_expression_kind  expr_kind;

  db_enter(3, "scan_aggregate_initializer_expression");
  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  check_assertion(!C_mode() || c99_mode || gcc_mode);
  expr_kind = (an_expression_kind)ek_normal;
  if (C_mode() && static_lifetime) {
    /* C mode aggregate initializers for statics have to be constant. */
    expr_kind = (an_expression_kind)ek_init_constant;
  }  /* if */
  push_expr_stack(expr_kind, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  if (static_lifetime) {
    /* In initializations of static variables, fold constant addressing
       expressions to constants so that constant initialization can be
       more easily discerned. */
    expr_stack->fold_constant_addr_exprs = TRUE;
  }  /* if */
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  *is_constant = FALSE;
  *levels_down = 0;
  /* See whether the expression can initialize the aggregate class.  If not,
     go down to the first member of the class and try again.  Loop until the
     right level is found or until we can go no further. */
  for (;;) {
    if (is_class_struct_union_type(required_type) &&
        (c99_mode || gcc_mode ||
         symbol_supplement_for_class(required_type)->is_class_aggregate)) {
      a_field_ptr first_field = next_initializable_field(
                                    skip_typerefs(required_type)->
                                        variant.class_struct_union.field_list);
      /* Stop looping if the aggregate class has no members. */
      if (first_field == NULL) goto required_type_determined;
      /* See whether the expression can be converted to the aggregate class
         type. */
      if (c99_mode ?
            types_are_compatible_ignoring_qualifiers(result.type,
                                                     required_type) :
            (conversion_to_class_possible(&result,
                                          required_type,
                                          /*try_bitwise_copy=*/TRUE,
                                          /*is_copy_initialization=*/TRUE,
                                          /*is_reference_binding=*/FALSE,
                                          /*processed_arg=*/FALSE,
                                          &conversion,
                                          (a_conv_descr *)NULL,
                                          &ambiguous,
                                          (a_candidate_function_ptr *)NULL) ||
             ambiguous)) goto required_type_determined;
      /* Go down to the first member. */
      required_type = first_field->type;
      (*levels_down)++;
    } else if (is_array_type(required_type)) {
      do {
        /* An array is also an aggregate.  However, generally the whole
           array is not initialized -- the first member is initialized. */
        if (is_string_type(required_type) &&
            result.is_simple_string_literal) {
          /* char array initialized by string literal, either one possibly
             wide.  Don't go down to the member type. */
          string_case = TRUE;
          goto required_type_determined;
        } else if (is_array_type(result.type) &&
                   types_are_compatible(result.type, required_type)) {
          /* In GNU C mode a compound literal may initialize an element of
             array type. */
          check_assertion(gcc_mode);
          goto required_type_determined;
        } else {
          /* Normal case: initialize the first member of the array. */
          required_type = array_element_type(required_type);
          (*levels_down)++;
        }  /* if */
      } while (is_array_type(required_type));
    } else {
      break;
    }  /* if */
  }  /* for */
required_type_determined:
  if (is_aggregate_or_union_type(required_type)) {
    /* The entity being initialized has a class or array type. */
    if (string_case) {
      a_constant_ptr con;
      check_assertion(is_constant_operand(&result));
      con = &result.variant.constant;
      /* Return the ck_string constant under the ck_address constant. */
      check_assertion(con->kind == (a_constant_repr_kind)ck_address &&
                      con->variant.address.kind ==
                                           (an_address_base_kind)abk_constant);
      con = con->variant.address.variant.constant;
      check_assertion(con->kind == (a_constant_repr_kind)ck_string);
      copy_constant(con, constant);
      *is_constant = TRUE;
    } else if (gcc_mode && is_an_rvalue(&result) &&
               result.kind == (an_operand_kind)ok_constant) {
      /* In GNU C mode, compound literals can be constant-expressions. */
      copy_constant(&result.variant.constant, constant);
      *is_constant = TRUE;
    } else {
      /* Build a dynamic initialization entry to describe the initialization.
         */
      prep_elision_initializer_operand(&result, required_type,
                                       /*fill_in_dtor=*/FALSE,
                                       ec_bad_initializer_type, dip);
      wrap_up_dynamic_init_full_expression(*dip);
      /* *dip == NULL means there was an error. */
      if (*dip == NULL) okay = FALSE;
    }  /* if */
  } else {
    /* The entity being initialized has a non-class type. */
    /* Convert to the required type. */
    prep_initializer_operand(&result, required_type, (a_boolean *)NULL,
                             (a_conv_descr_ptr)NULL,
                             /*initializing_return_value=*/FALSE,
                             /*initializing_variable=*/TRUE,
                             static_lifetime,
                             /*is_copy_initialization=*/TRUE,
                             /*processed_arg=*/FALSE,
                             /*nontype_template_arg=*/FALSE,
                             ec_bad_initializer_type);
    switch (result.kind) {
      case ok_error:
        /* Some sort of error; message was already issued. */
        okay = FALSE;
        discard_curr_expr_object_lifetime();
        break;
      case ok_expression:
        { an_expr_node_ptr expr = result.variant.expression;
          expr = wrap_up_full_expression(expr);
          *dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_expression);
          (*dip)->variant.expression = expr;
        }
        break;
      case ok_constant:
        copy_constant(&result.variant.constant, constant);
        *is_constant = TRUE;
        break;
      default:
        unexpected_condition_str(
                   "scan_aggregate_initializer_expression: bad operand kind");
    }  /* switch */
  }  /* if */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
  return okay;
}  /* scan_aggregate_initializer_expression */


void scan_class_parenthesized_initializer(
                                      a_type_ptr         class_type,
                                      a_type_ptr         object_class_type,
                                      a_boolean          force_object_lifetime,
                                      a_source_position  *source_pos,
                                      a_boolean          fill_in_dtor,
                                      a_dynamic_init_ptr *dip)
/*
Scan a parenthesized initializer for an object of type class_type.
class_type must be a class type having at least one constructor.
Build a dynamic initialization entry for the initialization, and set
*dip pointing to it.  If there is an error, set *dip to NULL.
The current token is right after the left parenthesis of the initialization.
This routine is used for constructs like

  A a(1, 2, 3);

In other words, this is direct-initialization of a class object.
object_class_type indicates the class type of the full object being
initialized.  It is the same as class_type, or a derived type thereof.
An object lifetime is forced around the initialization if
force_object_lifetime is TRUE.  On return, the current position is
following the closing parenthesis of the initializer.  If fill_in_dtor
is TRUE, any required destruction will be indicated in the dynamic
initialization.  *source_pos is the source position to be used in
overall errors.
*/
{
  an_expr_stack_entry           expr_stack_entry;
  a_class_symbol_supplement_ptr cssp;
  an_expr_node_ptr              arg_list;
  a_routine_ptr                 conversion_routine;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position             end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_boolean                     unknown_dependent_function;
  a_boolean                     empty_parens;

  db_enter(4, "scan_class_parenthesized_initializer");
  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  force_object_lifetime,
                  /*suppress_object_lifetime=*/FALSE);
  check_assertion(C_dialect == C_dialect_cplusplus &&
                  is_class_struct_union_type(class_type));
  empty_parens = (curr_token == tok_rparen);
  cssp = symbol_supplement_for_class(class_type);
  check_assertion(cssp->constructor != NULL);
  /* Scan the constructor argument list. */
  scan_ctor_arguments(cssp->constructor, &arg_list, &conversion_routine,
                      &unknown_dependent_function, source_pos,
                      object_class_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (conversion_routine == NULL && !unknown_dependent_function) {
    /* An error. */
    *dip = NULL;
    discard_curr_expr_object_lifetime();
  } else {
    /* Set the dynamic init entry to represent constructor initialization. */
    *dip = alloc_expr_dynamic_init((a_dynamic_init_kind)dik_constructor);
    (*dip)->variant.constructor.ptr = conversion_routine;
    (*dip)->variant.constructor.args = arg_list;
    /* The entity is value-initialized if the parentheses were empty. */
    (*dip)->variant.constructor.value_initialization = empty_parens;
    if (fill_in_dtor) {
      /* Fill in the destructor information.  Note that we cannot use
         alloc_dtor_dynamic_init because it does not allow for the
         object_class_type to differ from the class_type. */
      (*dip)->destructor = select_destructor(class_type, object_class_type,
                                             source_pos,
                                             /*honor_virtual=*/FALSE,
                                             /*evaluated=*/TRUE);
    }  /* if */
    /* If there's an object lifetime around the initialization, transfer it
       to the dynamic initialization entry. */
    wrap_up_dynamic_init_full_expression(*dip);
  }  /* if */
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* scan_class_parenthesized_initializer */


void scan_dependent_type_parenthesized_initializer(
                                      a_boolean          force_object_lifetime,
                                      a_dynamic_init_ptr *dip)
/*
Scan a parenthesized initializer that initializes an object of a template
parameter type in a prototype instantiation.  Create a dynamic initialization
entry to describe the initialization and return a pointer to it in *dip.
An object lifetime is forced around the initialization if
force_object_lifetime is TRUE.  On entry, the current token is the one
following the opening parenthesis.  On return, the current token is the
one following the closing parenthesis.
*/
{
  an_expr_stack_entry expr_stack_entry;

  db_enter(4, "scan_dependent_type_parenthesized_initializer");
  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  force_object_lifetime,
                  /*suppress_object_lifetime=*/FALSE);
  check_assertion(!C_mode());
  scan_dependent_parenthesized_initializer(dip);
  /* If there's an object lifetime around the initialization, transfer it
     to the dynamic initialization entry. */
  wrap_up_dynamic_init_full_expression(*dip);
  pop_expr_stack();
  db_exit();
}  /* scan_dependent_type_parenthesized_initializer */

#if MICROSOFT_EXTENSIONS_ALLOWED

void scan_microsoft_case_label_constant_expression(a_constant *constant)
/*
Scan an integral constant expression for a Microsoft case label constant,
and return the value of the constant in *constant.  MSVC++ allows
things like (void *)1 as case constants.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_microsoft_case_label_constant_expression");
  push_expr_stack((an_expression_kind)ek_integral_constant, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST,
            (EOPT_DISALLOW_COMMA_OPERATOR | EOPT_MICROSOFT_CASE_LABEL));
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  /* Make a constant from the operand. */
  extract_constant_from_operand(&result, constant);
  /* Check that the constant is represented as an integer constant. */
  if (is_error_constant(constant)) {
    /* Previous error, okay. */
  } else if (constant->kind == (a_constant_repr_kind)ck_template_param) {
    /* Template parameter constant, okay. */
  } else if (constant->kind != (a_constant_repr_kind)ck_integer) {
    /* The expression doesn't reduce to a value that will be an integer
       constant once cast to an integral type. */
    error_in_operand(ec_expr_not_integral_constant, &result);
    set_error_constant(constant);
  } else if (!is_integral_or_enum_type(constant->type)) {
    pos_warning(ec_expr_not_integral_constant, &result.position);
  }  /* if */
  pop_expr_stack();
  db_exit();
}  /* scan_microsoft_case_label_constant_expression */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

an_expr_node_ptr scan_boolean_controlling_expression(void)
/*
Scan an expression that is used in controlling contexts that need a boolean
result, such as if, while, do while, or for statements.  The type of the
expression must be (if bool is enabled) bool or convertible to bool, or
(if bool is disabled) scalar or a pointer-to-member type; or it must be of a
class type that can be converted to those types.
*/
{
  an_operand          result;
  an_expr_node_ptr    expr;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_boolean_controlling_expression");

  check_assertion(expr_stack == NULL); /* Check this is a full expression. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/TRUE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);

  /* Check its type and normalize it. */
  process_boolean_controlling_expression(&result, /*validate_only=*/FALSE);
  expr = make_node_from_operand(&result);
  expr = wrap_up_full_expression(expr);
  pop_expr_stack();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = result.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expr);
  }  /* if */
#endif /* DEBUG */

  db_exit();
  return expr;
}  /* scan_boolean_controlling_expression */


an_expr_node_ptr make_condition_value_expression(a_variable_ptr var,
                                                 a_boolean      is_switch_expr)
/*
The variable var has been declared in a condition declaration, e.g.,

  if (float var = f()) {...}

The condition is the one in a switch statement if is_switch_expr is TRUE.
Create an expression for the value of the condition, that is, the value
of the variable converted to the appropriate type (usually bool, but
arithmetic/pointer/pointer-to-member when bool is disabled, and int
(or some variant thereof) when is_switch_expr is TRUE).  Return a pointer
to the expression created.  The variable var must have an associated symbol.
*/
{
  an_operand              operand;
  an_expr_node_ptr        expr;
  a_ref_entry_ptr         ref;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Even though this is not an expression scan, make sure the expr_stack
     has something on it.  If there is already something on the stack,
     save it, clear the stack, and restore it later. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  /* Make an operand for the value of the variable. */
  check_assertion(var->source_corresp.assoc_info != NULL);
  ref = ref_entry((a_symbol_ptr)var->source_corresp.assoc_info,
                  &var->source_corresp.decl_position);
  make_lvalue_variable_operand(var, &operand, ref, /*record_expr=*/TRUE);
  set_operand_position(&operand, &var->source_corresp.decl_position,
                      &var->source_corresp.decl_pos_info->identifier_range.end,
                       (a_source_position *)NULL);
  do_operand_transformations(&operand, TOPT_NO_OPTIONS);
  if (is_switch_expr) {
    /* A switch condition (must be integral). */
    process_integer_expression(&operand, /*is_switch_expr=*/TRUE);
  } else {
    /* Other cases are boolean controlling expressions. */
    process_boolean_controlling_expression(&operand, /*validate_only=*/FALSE);
  }  /* if */
  expr = make_node_from_operand(&operand);
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
  return expr;
}  /* make_condition_value_expression */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_variable_ptr based_variable(void)
/*
Scan the variable specification in a __based specifier, e.g.,

   int __based(p) * q;
               ^this variable

The variable must be a pointer variable.  A pointer to the variable is
returned, or NULL if there is an error.  This is a Microsoft extension;
this routine is called only when microsoft_mode is TRUE.
*/
{
  an_operand              operand;
  a_variable_ptr          variable = NULL;
  a_symbol_ptr            sym_ptr, projection_sym_ptr;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Save the current expr_stack for later restoration, and start over, because
     this processing is not part of any expression we happen to be inside
     of. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/TRUE);
  /* The variable is not evaluated (at least not here). */
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Scan the identifier. */
  scan_identifier(&operand, (a_local_expr_options_set)EOPT_NO_OPTIONS,
                  PREC_LOWEST, &projection_sym_ptr);
  if (is_error_operand(&operand) || projection_sym_ptr == NULL) {
    /* Some previous error. */
  } else {
    sym_ptr = fundamental_symbol_of(projection_sym_ptr);
    /* Make sure the name referenced is a variable. */
    /* Note that if a functional-notation type conversion was seen by
       scan_identifier, the symbol returned is still that for the type name,
       and therefore an error will be issued here. */
    switch (sym_ptr->kind) {
      case sk_variable:
        variable = sym_ptr->variant.variable.ptr;
        break;
      case sk_static_data_member:
        variable = sym_ptr->variant.static_data_member.variable;
        break;
      default:
        pos_sy_error(ec_based_requires_variable_name, &operand.position,
                     projection_sym_ptr);
        break;
    }  /* if */
    if (variable != NULL) {
      /* Make sure the variable has a pointer type. */
      if (!is_pointer_type(variable->type)) {
        if (!is_error_type(variable->type)) {
          pos_error(ec_based_var_must_be_ptr, &operand.position);
        }  /* if */
        variable = NULL;
      } else if (!in_file_scope(variable)) {
        /* Types are allocated in file scope memory and can therefore not
           point to function scope memory entities.  This would happen if
           we allowed non-static local __based variables.*/
        if (!is_error_type(variable->type)) {
          pos_error(ec_based_var_cannot_be_local, &operand.position);
        }  /* if */
        variable = NULL;
      }  /* if */
    }  /* if */
  }  /* if */
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = operand.end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return variable;
}  /* based_variable */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean in_expression_context(void)
/*
Return TRUE if we are currently inside an expression context.
*/
{
  return (expr_stack != NULL);
}  /* in_expression_context */


a_boolean arg_operand_contains_template_param(an_arg_operand_ptr arg_operand)
/*
Return TRUE if the given arg_operand has a template-dependent value.  This
is used for testing nontype template arguments in determining whether
a template argument list is dependent.  Nontype template arguments that are
not yet associated with a template parameter, as in explicit template
argument lists on functions (e.g., f<int,1>(x)), are represented as
a_template_arg IL entries with the field arg_operand pointing to an
arg_operand entry.
*/
{
  a_boolean  contains_template_param = FALSE;
  an_operand *operand = &arg_operand->operand;

  if (is_template_param_constant_operand(operand)) {
    contains_template_param = TRUE;
  }  /* if */
  return contains_template_param;
}  /* arg_operand_contains_template_param */


a_symbol_ptr find_copy_constructor(a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *class_bitwise_copy)
/*
Find and return a pointer to a symbol representing a copy constructor for
the class indicated by class_type and accepting a first parameter whose type
is qualified as specified by required_qualifiers.  It is assumed that
the object to be copied is not an rvalue.  If no acceptable copy
constructor is found, return NULL.  If more than one acceptable copy
constructor is found and only one of them is the best match, return
that one; otherwise set *ambiguous to TRUE and return NULL.  If a
bitwise copy is allowed, return NULL and *class_bitwise_copy TRUE.
This routine is used only in C++ mode.
*/
{
  a_symbol_ptr            cctor_sym;
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Save the current expr_stack for later restoration, and start over, because
     this processing is not part of any expression we happen to be inside
     of. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/TRUE);
  cctor_sym = select_overloaded_copy_constructor(class_type,
                                                 required_qualifiers,
                                                 /*source_is_rvalue=*/FALSE,
                                                 pos,
                                                 ambiguous,
                                                 (a_boolean *)NULL,
                                                 class_bitwise_copy);
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
  return cctor_sym;
}  /* find_copy_constructor */


void process_unattached_template_argument_list(
                                          a_template_arg_ptr template_arg_list)
/*
The template argument list pointed to by template_arg_list is going to be
saved as the template argument list for an unknown template in a prototype
instantiation.  Go through it and do any necessary processing for that.
*/
{
  an_expr_stack_entry     expr_stack_entry;
  an_expr_stack_entry_ptr saved_expr_stack;

  /* Even though this is not an expression scan, make sure the expr_stack
     has something on it.  If there is already something on the stack,
     save it, clear the stack, and restore it later. */
  save_expr_stack(&saved_expr_stack);
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/TRUE);
  prep_generic_template_argument_list(template_arg_list);
  pop_expr_stack();
  restore_expr_stack(saved_expr_stack);
}  /* process_unattached_template_argument_list */


an_expr_node_ptr make_assignment_expr(an_expr_node_ptr       lvalue_expr,
                                      an_expr_operator_kind  op,
                                      an_expr_node_ptr       rvalue_expr)
/*
Make an expression that assigns rvalue_expr to lvalue_expr using assignment
operator op, and return a pointer to it.
*/
{
  an_expr_node_ptr assign_node;
  a_type_ptr       result_type =
                     make_unqualified_type(type_pointed_to(lvalue_expr->type));

  lvalue_expr->next = rvalue_expr;
  /* Make the assignment node. */
  assign_node = make_operator_node(op, result_type, lvalue_expr);
  return assign_node;
}  /* make_assignment_expr */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
