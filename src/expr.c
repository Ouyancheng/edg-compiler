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

expr.c -- Expression scanning routines.

*/

#include "basics.h"
#include "mem_manage.h"
#include "error.h"
#include "lexical.h"
#include "il.h"
#include "symbol_tbl.h"
#include "expr.h"
#include "exprutil.h"
#include "overload.h"
#include "preproc.h"
#include "folding.h"
#include "const_ints.h"
#include "cmd_line.h"
#include "types.h"
#include "decls.h"
#include "decl_inits.h"
#include "target.h"
#include "lang_feat.h"
#include "templates.h"

/*
The operators and their precedences are:

Operators			Precedence	Associativity
[] () . -> ++ --		17		L       [] subscripting
							() function call
							++ -- postfix
++ -- & * + - ~ ! sizeof	16		R	Prefix operators
cast				15		R
.* ->* (C++ only)		14		L
* / %				13		L
+ -				12		L
<< >>				11		L
< > <= >=			10		L
== !=				9		L
&				8		L
^				7		L
|				6		L
&&				5		L
||				4		L
?				3		R
= += -= *= /= %=
&= ^= |= <<= >>=		2		R	Assignment operators
,				1		L

Higher-valued precedence means an operator binds more tightly.
Precedence level 0 is used to bracket a complete expression.
*/
#define LEFT_ASSOC  TRUE
#define RIGHT_ASSOC FALSE
#define PREC_POSTFIX    17
#define PREC_PREFIX     16
#define PREC_CAST       15
#define PREC_PTR_TO_MEMBER 14
#define PREC_MULT_DIV   13
#define PREC_PLUS_MINUS 12
#define PREC_SHIFT      11
#define PREC_RELATIONAL 10
#define PREC_EQ_NE       9
#define PREC_AND         8
#define PREC_EXCL_OR     7
#define PREC_OR          6
#define PREC_AND_AND     5
#define PREC_OR_OR       4
#define PREC_QUEST_MARK  3
#define PREC_ASSIGNMENT  2
#define PREC_COMMA       1
#define PREC_LOWEST      0


/* Flag bits used to indicate scanning options that apply to one level
   of expression scanning.  These are localized options that indicate
   special handling for an expression because of the context. */
#define EOPT_DISALLOW_COMMA_OPERATOR 0x1
			/* The comma operator should not be allowed at the top
			   level.  Certain contexts suppress the comma because
			   it has another meaning there (e.g., argument
			   lists). */
#define EOPT_OPERAND_OF_CAST 0x2
			/* This expression is the immediate operand of a cast.
			   Floating constants are allowed in integral constant
			   expressions when they are the immediate operand
			   of a cast. */
#define EOPT_OPERAND_OF_ADDRESS_OF 0x4
			/* This expression is the immediate operand of a
			   unary "&" operator.  This is significant when the
			   operand is a qualified name in C++ -- it indicates
			   a pointer-to-member. */
#define EOPT_TRAPPED_LEFT_PAREN 0x8
			/* The caller of scan_expr scanned over a left
			   parenthesis which it turned out should have begun
			   an expression.  scan_expr pretends that there is
			   a left parenthesis preceding the current token. */
#define EOPT_ALLOW_BOUND_FUNCTION 0x10
			/* A C++ bound function may be returned. */
#define EOPT_NO_OPTIONS 0
typedef int a_local_expr_options_set;


/* Forward declaration. */
static void scan_expr_full(an_operand               *result,
                           an_operand               *bound_function_selector,
                           int                      prec_level,
                           a_local_expr_options_set local_options);
/* Interface to scan_expr_full for the simple case where a bound function
   cannot be returned. */
#define scan_expr(result, prec_level, local_options)                  \
  scan_expr_full((result), (an_operand *)NULL, (prec_level),          \
                 (local_options))


/*
Interfaces to add_stop_token and remove_stop_token to be used for matching
closing tokens, like ")" for "(".  These mark the beginning and end of
a nested construct.
*/
#define add_matching_stop_token(token)                                \
{ add_stop_token(token);                                              \
  expr_stack->nested_construct_depth++;                               \
}  /* add_matching_stop_token */
#define remove_matching_stop_token(token)                             \
{ remove_stop_token(token);                                           \
  expr_stack->nested_construct_depth--;                               \
}  /* remove_matching_closing_token */


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
  a_type_ptr       operand_type;

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
    case eok_call:
    case eok_virtual_call:
    case eok_pm_call:
      /* These all cause side effects. */
      has_side_effects = TRUE;
      break;
    case eok_indirect:
    case eok_subscript:
      /* Causes a side effect if the type of the thing pointed to
         is volatile. */
      /* Note that we test the pointer operand's type, not the node type,
         because of an IL shorthand that allows omission of the cast to the
         unqualified version of the type. */
      operand_type = node->variant.operation.operands->type;
      if (is_pointer_type(operand_type)) {
        has_side_effects =
                     is_volatile_qualified_type(type_pointed_to(operand_type));
      }  /* if */
      break;
    case eok_vacuous_destructor_call:
      /* A vacuous destructor call like
           p->int::~int();
         is an expression that intentionally does nothing, so suppress
         the warning. */
      suppress = TRUE;
      break;
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
should be suppressed.
*/
{
  a_boolean has_side_effects = FALSE, suppress = FALSE;

  switch (node->kind) {
    case enk_error:
      /* Who knows what an error node might have done -- suppress the
         warning. */
      suppress = TRUE;
      break;
    case enk_constant:
    case enk_variable_address:
    case enk_routine_address:
    case enk_field:
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
#if CHECKING
    default:
      internal_error("node_has_side_effects: bad node kind");
#endif /* CHECKING */
  }  /* switch */

  *suppress_warning = suppress;
  return has_side_effects;
}  /* node_has_side_effects */


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
  an_expr_node_ptr node = *node_ptr;
  a_boolean        suppress = FALSE;

  /* This routine could do various kinds of pruning -- in fact, it used to;
     however, in accord with the philosophy that the front end does no
     optimization, it now only removes an unnecessary top-level cast to
     void. */
  /* Check for an explicit cast-to-void node, remove the node, and
     suppress the warning about a node with no effect in that case.
     This is because we assume that a programmer who casts something
     to void is doing so for some good reason, and also because the
     macro for "assert" expands to a (void)0 when NDEBUG is defined. */
  while ((node->kind == (an_expr_node_kind)enk_operation) &&
         (node->variant.operation.kind == (an_expr_operator_kind)eok_cast) &&
         is_void_type(node->type)) {
    /* This is a cast to void; remove the cast node. */
    node = node->variant.operation.operands;
    /* Set the flag to suppress the warning. */
    suppress = TRUE;
  }  /* while */
  /* See if the node has some effect. */
  if (!suppress && node_has_side_effects(node, &suppress)) suppress = TRUE;
  *suppress_warning = suppress;
  /* Put the possibly updated pointer back into *node_ptr. */
  *node_ptr = node;
}  /* simplify_void_node */


static void simplify_void_operand(an_operand *operand)
/*
Examine the operand given by *operand, which has been scanned as a void
expression, and simplify it if possible by removing parts that do nothing.
Issue a warning if the operand has no effect.
*/
{
  a_boolean suppress_warning;

  if (!is_expression_operand(operand)) {
    /* An operand that is not an expression cannot have side effects.
       For error operands, assume that the original form might have had
       an effect, and suppress the warning. */
    suppress_warning = is_error_operand(operand);
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


static a_boolean is_class_or_error_operand(an_operand *operand)
/*
Return TRUE if the given operand has a class type or is an error operand.
*/
{
  a_boolean is_class_or_error = is_error_operand(operand) ||
                                is_class_struct_union_type(operand->type);
  return is_class_or_error;
}  /* is_class_or_error_operand */


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
  a_boolean          err = FALSE, processed = FALSE;

  db_enter(4, "scan_subscript_operator");

  copy_source_position(pos_curr_token, operator_position);

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
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_or_error_operand(operand_1) ||
         is_class_or_error_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_subscript,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      /* One of the operands must have type "pointer to object type" and the 
         other must be an integral expression.  See section 3.3.2.1 of the
         standard.  If the first operand is integral, switch them. */
      if (is_integral_type(operand_1->type)) {
        /* The subscript value is outside the brackets and the pointer value is
           inside the brackets.  Switch them. */
        copy_operand(operand_1, &operand_temp);
        copy_operand(&operand_2, operand_1);
        copy_operand(&operand_temp, &operand_2);
      }  /* if */

      /* The first operand must be a pointer to object. */
      if (
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

      /* The subscript must be integral. */
      (void)check_integral_operand(&operand_2);

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
      result->type = result_type;
      result->state = (an_operand_state)os_lvalue;
      /* Preserve the reference entries from the first operand (the array). */
      result->ref_entries_list = operand_1->ref_entries_list;
    }  /* if */
  }  /* if */

  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_matching_stop_token(tok_rbracket);

  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_subscript_operator */


/*
Enumeration indicating the state on return from a scan of a printf format
string.
*/
typedef enum /*a_printf_scan_state*/ {
  pss_new_specifier,	/* Look for new specifier next time. */
  pss_after_field_width,/* Start after field width next time. */
  pss_after_precision	/* Start after precision next time. */
} a_printf_scan_state;


static a_type_ptr next_printf_scanf_arg_type(
                                          a_boolean           is_scanf,
                                          char                **fmt_string_ptr,
                                          a_printf_scan_state *pss_ptr,
                                          a_boolean           *indirect)
/*
Return the type that the next argument to a printf or scanf call should have,
by finding the next thing in the format string that consumes an argument.
Return NULL if no further arguments are needed.  is_scanf is TRUE for
scanf/FALSE for printf; *fmt_string_ptr points to the current position 
in the format string (it will be updated); and *pss_ptr is maintained
to handle resuming the scan after a "*" field width or precision.
If there is an error in the format string, issue a warning and set
*fmt_string_ptr to NULL.  *indirect is returned TRUE if the type returned
has an added pointer level relative to the type indicated in the formatting
string, e.g., for scanf.

See 4.9.6.1 in the standard for printf, 4.9.6.2 for scanf.
*/
{
  a_type_ptr          required_type;
  char                *fmt_string = *fmt_string_ptr;
  a_printf_scan_state pss = *pss_ptr;
  a_boolean           l_size, L_size, h_size, add_pointer;
#if LONG_LONG_ALLOWED
  a_boolean           ll_size;
#endif /* LONG_LONG_ALLOWED */
  a_boolean           suppress_assignment = FALSE;

  *indirect = FALSE;
  /* Pick up in the middle if the previous call returned a field width
     or precision. */
  if (pss == pss_after_field_width) goto after_field_width;
  if (pss == pss_after_precision) goto after_precision;

  /* Look for the next "%" in the string, or the null that terminates it. */
another_specifier:;
  suppress_assignment = FALSE;
  while (*fmt_string != '%' && *fmt_string != '\0') fmt_string++;
  /* If the null was found, there is no next argument. */
  if (*fmt_string == '\0') {
    required_type = NULL;
  } else {
    /* "%" was found. */
    fmt_string++;
    /* On "%%", go back and look for another specifier. */
    if (*fmt_string == '%') {
      fmt_string++;
      goto another_specifier;
    }  /* if */
    /* For printf, ignore a sequence of flags (-, +, space, #, or 0).
       For scanf, ignore the assignment-suppressing character "*". */
    if (is_scanf) {
      if (*fmt_string == '*') {
        fmt_string++;
        suppress_assignment = TRUE;
      }  /* if */
    } else {
      while (*fmt_string == '-' || *fmt_string == '+' || *fmt_string == ' ' ||
             *fmt_string == '#' || *fmt_string == '0') fmt_string++;
    }  /* if */
    /* An optional field width is next.  For printf, it can be a "*". */
    if (isdigit((unsigned char)*fmt_string)) {
      /* Decimal integer field width.  Skip over it. */
      do {} while (isdigit((unsigned char)*++fmt_string));
    } else if (!is_scanf && *fmt_string == '*') {
      /* "*" as field width.  The corresponding argument should be an 
         int.  Return that, and pick up next time after the field width. */
      required_type = integer_type((an_integer_kind)ik_int);
      fmt_string++;
      pss = pss_after_field_width;
      goto end_of_scan;
    }  /* if */
after_field_width:;
    /* For printf, an optional precision is next ("." followed by a
       decimal integer or "*"). */
    if (!is_scanf && *fmt_string == '.') {
      fmt_string++;
      if (isdigit((unsigned char)*fmt_string)) {
        /* Decimal integer precision.  Skip over it. */
        do {} while (isdigit((unsigned char)*++fmt_string));
      } else if (*fmt_string == '*') {
        /* "*" as precision.  The corresponding argument should be an 
           int.  Return that, and pick up next time after the precision. */
        required_type = integer_type((an_integer_kind)ik_int);
        fmt_string++;
        pss = pss_after_precision;
        goto end_of_scan;
      }  /* if */
    }  /* if */
after_precision:;
    /* The optional size character is next.  l indicates long integer
       (sometimes double), L long double, and h short integer. */
    l_size = L_size = h_size = FALSE;
#if LONG_LONG_ALLOWED
    ll_size = FALSE;
#endif /* LONG_LONG_ALLOWED */
    if (*fmt_string == 'l') {
#if LONG_LONG_ALLOWED
      if (fmt_string[1] == 'l') {
        /* "ll" for long long.  This is nonstandard. */
        if (strict_ansi_mode) goto default_case;
        ll_size = TRUE;
        fmt_string += 2;
      } else
#endif /* LONG_LONG_ALLOWED */
      {
        l_size = TRUE;
        fmt_string++;
      }
    } else if (*fmt_string == 'L') {
      L_size = TRUE;
      fmt_string++;
    } else if (*fmt_string == 'h') {
      h_size = TRUE;
      fmt_string++;
    }  /* if */
    /* The next character indicates the conversion type, e.g., "d" for
       decimal.  Determine the required type.  For most (but not all)
       scanf cases, "pointer to" will be added afterwards. */
    *indirect = add_pointer = is_scanf;
    switch (*fmt_string++) {
      case 'd':
      case 'i':
        /* int conversion.  If "l" was specified, long conversion;
           if "h" was specified for scanf, short conversion. */
#if LONG_LONG_ALLOWED
        /* If "ll" was specified, long long conversion. */
#endif /* LONG_LONG_ALLOWED */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_long_long);
#endif /* LONG_LONG_ALLOWED */
        } else if (h_size && is_scanf) {
          required_type = integer_type((an_integer_kind)ik_short);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        /* Unsigned int conversion.  If "l" was specified, unsigned long
           conversion; if "h" was specified for scanf, unsigned short 
           conversion. */
#if LONG_LONG_ALLOWED
        /* If "ll" was specified, unsigned long long conversion. */
#endif /* LONG_LONG_ALLOWED */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long);
#if LONG_LONG_ALLOWED
        } else if (ll_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long_long);
#endif /* LONG_LONG_ALLOWED */
        } else if (h_size && is_scanf) {
          required_type = integer_type((an_integer_kind)ik_unsigned_short);
        } else {
          required_type = integer_type((an_integer_kind)ik_unsigned_int);
        }  /* if */
        break;
      case 'f':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
        /* Floating-point conversions.
           For printf: double usually, long double if "L" was specified.
           For scanf:  float usually, double if "l" was specified, long
                       double if "L" was specified. */
        if (L_size) {
          required_type = float_type((a_float_kind)fk_long_double);
        } else if (!is_scanf || l_size) {
          required_type = float_type((a_float_kind)fk_double);
        } else {
          required_type = float_type((a_float_kind)fk_float);
        }  /* if */
        break;
      case 'c':
        /* Character conversion. */
        /* int conversion for printf, string conversion for scanf. */
        if (is_scanf) {
          /* add_pointer is TRUE, so "char" will become "char *". */
          required_type = integer_type((an_integer_kind)ik_char);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        break;
      case 's':
        /* String conversion.  "pointer to" will be added to make
           "char" into "char *", for both printf and scanf. */
        required_type = integer_type((an_integer_kind)ik_char);
        add_pointer = TRUE;
        /* *indirect is not set on purpose. */
        break;
      case 'p':
        /* Pointer conversion.  Basic type is "void *". */
        required_type = make_pointer_type(void_type());
        break;
      case 'n':
        /* Return number of characters read or written so far.
           Argument is "int *" for both printf and scanf, or "short *"
           if "h" was specified, or "long *" if "l" was specified. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
        } else if (h_size) {
          required_type = integer_type((an_integer_kind)ik_short);
        } else {
          required_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        *indirect = add_pointer = TRUE;
        break;
      case '[':
        /* For scanf only, a scanset.  Skip to the corresponding "]".
           Watch out for the special constructions "[]..." and "[^]...".  The
           input item is a pointer to character. */
        if (!is_scanf) goto default_case;
        if (*fmt_string == ']') {
          fmt_string++;
        } else if (*fmt_string == '^' && fmt_string[1] == ']') {
          fmt_string += 2;
        }  /* if */
        while (*fmt_string != ']' && *fmt_string != '\0') fmt_string++;
        required_type = integer_type((an_integer_kind)ik_char);
        break;
      default:
default_case:;
        /* Unknown formatting character.  Give warning and abandon checking. */
        warning(ec_bad_printf_format_string);
        required_type = NULL;
        fmt_string = NULL;
        goto end_of_scan;
    }  /* switch */
    /* Add a "pointer to" to the required type if necessary. */
    if (add_pointer) required_type = make_pointer_type(required_type);
  }  /* if */
  /* Next time around, look for a new specifier. */
  pss = pss_new_specifier;
  /* If there was an assignment-suppressing character "*" in a scanf, go
     get the next specifier. */
  if (suppress_assignment) goto another_specifier;
end_of_scan:;
  *fmt_string_ptr = fmt_string;
  *pss_ptr = pss;
  return required_type;
}  /* next_printf_scanf_arg_type */


static void check_printf_scanf_arg(an_operand          *argument_operand,
                                   a_boolean           is_scanf,
                                   char                **fmt_string_ptr,
                                   a_printf_scan_state *pss_ptr)
/*
Check an argument of a printf- or scanf-type function call to see if its
type matches the corresponding formatting specifier in the format string.
argument_operand points to the argument, is_scanf is TRUE for scanf/FALSE
for printf, and *fmt_string_ptr and *pss_ptr give the current position in the
format string (they are updated on return).
*/
{
  a_type_ptr required_type, eff_required_type, eff_argument_type;
  a_boolean  indirect;

  /* Find the next formatting specifier in the string. */
  required_type = next_printf_scanf_arg_type(is_scanf, fmt_string_ptr,
                                             pss_ptr, &indirect);
  /* If *fmt_string_ptr was set to NULL there was an error in the format
     string. */
  if (*fmt_string_ptr != NULL) {
    if (required_type == NULL) {
      /* There were no more formatting specifiers. */
      pos_warning(ec_too_many_printf_args, &argument_operand->position);
      /* Stop checking to avoid redundant errors. */
      *fmt_string_ptr = NULL;
    } else {
      /* Check that the argument type matches the specifier type.  Note
         the use of "interchangeable" rather than "compatible", because
         we want to allow things like "printf("%lx", (long)i);". */
      eff_required_type = required_type;
      eff_argument_type = argument_operand->type;
      if (indirect) {
        /* In cases where an extra indirection is added to the required
           type so that a value can be returned from the routine, remove
           the extra level of pointer type.  That allows matching things
           like "int *" and "unsigned int *".  This is slightly looser
           matching than is allowed without warning for normal function
           calls, but here we know what the runtime routine is doing. */
        if (!is_pointer_type(eff_argument_type)) goto mismatch;
        eff_argument_type = type_pointed_to(eff_argument_type);
        eff_required_type = type_pointed_to(eff_required_type);
      }  /* if */
      if (!interchangeable_types(eff_required_type, eff_argument_type)) {
        /* The argument type does not match the required type. */
mismatch:
        if (!is_error_type(eff_argument_type)) {
          pos_warning(ec_printf_arg_mismatch, &argument_operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_printf_scanf_arg */


static void scan_call_arguments(a_type_ptr         function_type,
                                a_boolean          already_after_left_paren,
                                an_expr_node_ptr   *p_argument_list,
                                a_boolean          overloaded_function_case,
                                an_arg_operand_ptr *arg_operand_list)
/*
Scan the arguments of a function call and return a list of argument
expressions in *p_argument_list.  The type of the function being called is
given by function_type; function_type is NULL if the type is not known.
The current token at the time of call is the opening "(" of the
argument list, unless already_after_left_paren is TRUE, in which case
it is the token following the left parenthesis (but the add_stop_token
call has not been done).  On return, the current token is the token
following the closing ")".  If overloaded_function_case is TRUE, this
call is scanning the arguments for a call of an overloaded function, so
build an argument operand list and return a pointer to it in
*arg_operand_list.
*/
{
  a_param_type_ptr    curr_param_type;
  a_boolean           prototyped;
  a_boolean           has_ellipsis;
  a_boolean           have_param_info;
  an_arg_pragma_kind  arg_kind;
  int                 varargs_count;
  int                 arg_ctr;
  an_operand          argument_operand;
  an_expr_node_ptr    argument_head;
  an_expr_node_ptr    argument_tail;
  an_expr_node_ptr    curr_node;
  a_boolean           do_default_promotion, indirect;
  a_type_ptr          formal_type;
  a_boolean           is_scanf = FALSE;  /* Initialized to make lint happy. */
  a_constant_ptr      con_ptr;
  char                *fmt_string = NULL;
  a_printf_scan_state pss;
  an_arg_operand_ptr  end_arg_operand_list, arg_operand;

  db_enter(4, "scan_call_arguments");
  if (overloaded_function_case) {
    /* Start with an empty list of argument operands. */
    *arg_operand_list = NULL;
    end_arg_operand_list = NULL;
  }  /* if */
  if (function_type != NULL) {
    a_routine_type_supplement_ptr extra_info;

    /* Get information on the parameters of the function. */
    function_type = skip_typerefs(function_type);
#if CHECKING
    if (function_type->kind != (a_type_kind)tk_routine) {
      internal_error("scan_call_arguments: bad function type");
    }  /* if */
#endif /* CHECKING */
    extra_info = function_type->variant.routine.extra_info;
    curr_param_type = extra_info->param_type_list;
    prototyped = extra_info->prototyped;
    has_ellipsis = extra_info->has_ellipsis;
    have_param_info = (prototyped || extra_info->assoc_routine != NULL);
    arg_kind = extra_info->arg_pragma;
    /* Get the varargs count. */
    varargs_count = extra_info->lint_varargs_count;
  } else {
    /* Bad function operand.  Therefore, we have no information on
       parameters. */
    curr_param_type = NULL;
    prototyped = FALSE;
    has_ellipsis = FALSE;
    have_param_info = FALSE;
    arg_kind = (an_arg_pragma_kind)apk_none;
    varargs_count = NOT_LINT_VARARGS;
  }  /* if */

  if (!already_after_left_paren) {
    /* Get past the opening parenthesis. */
    (void)get_token();
  }  /* if */
  /* Add ")" as a stop token. */
  add_matching_stop_token(tok_rparen);

  /* Count the arguments in case varargs is used. */
  arg_ctr = 0;
  argument_head = argument_tail = NULL;
  /* Check for an empty argument list. */
  if (curr_token != tok_rparen) {
    add_stop_token(tok_comma);
    /* Scan a comma-separated list of arguments. */
    do {
      /* In cfront mode, allow an extra comma at the end of the argument
         list. */
      if (cfront_compatibility_mode && curr_token == tok_rparen) break;
      /* Scan an argument expression.  Note that it is not converted to an
         rvalue yet. */
      scan_expr(&argument_operand, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
      arg_ctr++;
      /* Check for too many arguments and determine whether or not the
         default argument promotions apply to this argument. */
      do_default_promotion = TRUE;
      if (!have_param_info) {
        /* We have no information on parameter types, because
             (a) The called function has a bad type;
             (b) The called function has an old-style parameter list and no
                 body (hence no parameter declarations);
             (c) We're in the ellipsis section of a prototyped function call
                 (including a printf/scanf-type routine);
             (d) We're in the varargs section of an old-style function call;
             (e) We're scanning extra arguments after issuing an error about
                 there being too many arguments; or
             (f) We're scanning the arguments for an overloaded function call.
        */
      } else if (prototyped) {
        /* Prototyped parameter list. */
        if (curr_param_type != NULL) {
          do_default_promotion = FALSE;
        } else {
	  /* No more formal arguments in the list. */
          if (!has_ellipsis) {
            /* No ellipsis, so error: extra actual argument. */
            error(ec_too_many_arguments);
          }  /* if */
          have_param_info = FALSE;
        }  /* if */
      } else {
        /* Old-style parameter list, for a function with a body (i.e., we
           know the argument types). */
	if (curr_param_type == NULL) {
	  /* No more formal arguments in the list. */
	  if (varargs_count == NOT_LINT_VARARGS) {
	    /* A lint-style varargs comment does not apply, so warning:
               extra actual argument.*/
            warning(ec_too_many_arguments);
            have_param_info = FALSE;
	  }  /* if */
	}  /* if */
      }  /* if */

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
        /* Do the argument conversion or promotion. */
        if (do_default_promotion) {
	  /* Either an ellipsis was encountered or this is an old-style
             argument list; do the default argument promotion. */
          arg_default_promote_operand(&argument_operand);
	  /* If this is an old-style call and we have the list of types as
	     defined by the function body, check the promoted type of the
             actual against the formal. */
	  if (have_param_info && !prototyped && curr_param_type != NULL) {
	    /* Compare the type of the promoted actual with the promoted formal
	       without qualifiers. */
	    if (!is_error_type(curr_param_type->type)) {
              formal_type = default_argument_promotion(
                                         skip_typerefs(curr_param_type->type));
              if (!types_are_compatible(formal_type, argument_operand.type)) {
                if (interchangeable_types(formal_type,
                                          argument_operand.type)) {
                  /* Types are interchangeable but not compatible (e.g.,
                     unsigned int vs. int). */
                  remark(ec_old_style_incompatible_param);
#if TARG_NULL_IS_ALL_BITS_ZERO
                } else if (!strict_ansi_mode &&
                           is_pointer_type(formal_type) &&
                           is_integral_type(argument_operand.type) &&
                           op_is_zero_constant(&argument_operand) &&
                           skip_typerefs(formal_type)->size ==
                                  skip_typerefs(argument_operand.type)->size) {
                  /* An uncast zero can be passed for a pointer parameter if
                     the architecture uses all zero bits for a NULL pointer. */
                  remark(ec_old_style_incompatible_param);
#endif /* TARG_NULL_IS_ALL_BITS_ZERO */
                } else {
                  /* Types are outright incompatible. */
                  warning(ec_old_style_incompatible_param);
                }  /* if */
	      }  /* if */
	    }  /* if */
          } else if (fmt_string != NULL) {
            /* Check a printf or scanf argument type against the corresponding
               formatting specifier in the format string. */
            check_printf_scanf_arg(&argument_operand, is_scanf,
                                   &fmt_string, &pss);
	  }  /* if */
        } else {
	  /* Parameter is prototyped. */
          /* Check the argument for compatibility against the parameter,
             casting it if necessary.  Also convert from lvalue to rvalue
             when appropriate. */
          prep_argument_operand(&argument_operand, curr_param_type,
                                (a_user_conv_descr_ptr)NULL,
                                ec_incompatible_param);
        }  /* if */
        /* Link the new argument into the list of arguments. */
        curr_node = make_node_from_operand(&argument_operand);
        if (argument_head == NULL) {
          argument_head = curr_node;
        } else {
          argument_tail->next = curr_node;
        }  /* if */
        argument_tail = curr_node;
        argument_tail->next = NULL;
      }  /* if */

      if (curr_param_type != NULL) {
	/* Get the next parameter type entry. */
        curr_param_type = curr_param_type->next;
      }  /* if */

      /* If this is a call to a function with a printf- or scanf-style
         argument list and the ellipsis is next, the current argument is
         the format string.  See if it is constant; if so, we will be
         able to check the rest of the arguments against the format string
         as we scan them. */
      if ((arg_kind == (an_arg_pragma_kind)apk_printf ||
           arg_kind == (an_arg_pragma_kind)apk_scanf) &&
          have_param_info && curr_param_type == NULL) {
        /* See if the format string is a constant (actually, the address
           of a constant string). */
        if (curr_node != NULL) {
          if (curr_node->kind == (an_expr_node_kind)enk_constant) {
            /* The node is a constant. */
            con_ptr = curr_node->variant.constant;
            if (con_ptr->kind == (a_constant_repr_kind)ck_address &&
                con_ptr->variant.address.kind ==
                                          (an_address_base_kind)abk_constant) {
              /* The constant is a pointer to a constant.  We know the
                 type is right because we passed the prototyped parameter
                 type test above. */
              con_ptr = con_ptr->variant.address.variant.constant;
              if (con_ptr->kind == (a_constant_repr_kind)ck_string &&
                  char_int_kind_from_string_type(con_ptr->type) ==
                                                         plain_char_int_kind) {
                /* The constant pointed to is a string (and not a wide string).
                   Check that it is null-terminated. */
                fmt_string = con_ptr->variant.string.value;
                is_scanf = (arg_kind == (an_arg_pragma_kind)apk_scanf);
                pss = pss_new_specifier;
                if (fmt_string[con_ptr->variant.string.length-1] != '\0') {
                  /* String is not null-terminated. */
                  fmt_string = NULL;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */

    } while (loop_token(tok_comma));
    remove_stop_token(tok_comma);
  }  /* if */

  set_err_pos_to_curr_token();
  /* Check for additional parameters not accounted for in the call. */
  if (!have_param_info) {
    /* We don't have parameter information (anymore?), so we can't check. */
  } else if (prototyped) {
    /* Prototyped parameter list. */
    if (curr_param_type != NULL) {
      /* Not enough arguments? */
      /* If there is a default argument value, or several, use them. */
      if (curr_param_type->default_arg_expr != NULL) {
        curr_node = copy_default_arg_expr_list(curr_param_type);
        if (argument_head == NULL) {
          argument_head = curr_node;
        } else {
          argument_tail->next = curr_node;
        }  /* if */
        /* Note that argument_tail is not updated to the true end of list. */
      } else {
        /* No default arguments. */
        /* Error: too few actual arguments. */
        error(ec_too_few_arguments);
      }  /* if */
      /* Suppress the end-of-printf check below. */
      fmt_string = NULL;
    }  /* if */
  } else {
    /* Old-style parameter list. */
    if ((varargs_count == NOT_LINT_VARARGS && curr_param_type != NULL) ||
        arg_ctr < varargs_count) {
      /* Warning: too few actual arguments. */
      warning(ec_too_few_arguments);
    }  /* if */
  }  /* if */
  if (fmt_string != NULL) {
    /* For a printf- or scanf-like function, check that all the formatting
       specifiers were used. */
    if (next_printf_scanf_arg_type(is_scanf, &fmt_string, &pss, &indirect)
                                                                     != NULL) {
      /* There are no more arguments, but the format string has more
         formatting specifiers. */
      warning(ec_too_few_printf_args);
    }  /* if */
  }  /* if */

  /* Check for the closing paren. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);
  /* Return argument list pointer to caller. */
  *p_argument_list = argument_head;
  db_exit();
}  /* scan_call_arguments */


void check_closing_paren_after_expr_list(void)
/*
Check for the required closing parenthesis after an expression list, and
pass over it if it is found.  This is similar to required_token, but it
does some special error-recovery processing to handle additional
unexpected expressions more gracefully.
*/
{
  unsigned char save_comma_stop_token_count;

  /* Remove comma from the stop tokens set. */
  save_comma_stop_token_count = stop_token_array[(int)tok_comma];
  stop_token_array[(int)tok_comma] = 0;
  (void)required_token(tok_rparen, ec_exp_rparen);
  /* Restore comma as a stop token (if it was one). */
  stop_token_array[(int)tok_comma] = save_comma_stop_token_count;
}  /* check_closing_paren_after_expr_list */


static an_expr_node_ptr scan_parenthesized_initializer_expression(
                                            a_type_ptr         dest_type,
                                            an_error_code      err_code)
/*
Scan a single expression in parentheses as an initializer value, and convert
it to dest_type if necessary.  The current token is the token after the
opening left parenthesis.  On return, the current token is the token following
the closing parenthesis.  If the conversion cannot be done, issue the
error err_code.
*/
{
  an_expr_node_ptr expr;
  an_operand       result;

  add_matching_stop_token(tok_rparen);
  /* Since the syntax has an expression-list even in the single-expression
     case, a top-level comma is not allowed. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, dest_type, (a_user_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE, err_code);
   /* Check for the required closing parenthesis. */
  check_closing_paren_after_expr_list();
  remove_matching_stop_token(tok_rparen);
  expr = make_node_from_operand(&result);
  return expr;
}  /* scan_parenthesized_initializer_expression */


void scan_ctor_arguments(a_symbol_ptr       constructor_sym,
                         an_expr_node_ptr   *arg_expr_list,
                         a_routine_ptr      *conversion_routine,
                         a_source_position  *source_pos,
                         a_type_ptr         object_class_type)
/*
Scan the argument list for a C++ constructor call.  The current token is
the one right after the opening parenthesis of the argument list.  The
constructor symbol (possibly overloaded) is constructor_sym.
Scan the arguments and the closing parenthesis, and return the
argument list in *arg_expr_list and a pointer to the proper constructor
routine in *conversion_routine.  If the proper constructor cannot be
determined, return NULL.  *source_pos indicates the source position
of the call.  This routine may be called only in C++ mode.
It's used for parenthesis-enclosed initializers for classes that have
constructors, as in

  class A {...};
  A x(1, 2, 3);

The caller need not add the right parenthesis to the stop tokens set, or
remove it later, as this routine takes care of that.  object_class_type
is the type of the object being constructed which may be different than
the type of the constructor being called (e.g., when a base class constructor
is being called for a derived class object).
*/
{
  a_boolean           overloaded_function_case = FALSE;
  a_type_ptr          routine_type;
  a_source_position   start_position;
  an_arg_operand_ptr  arg_operand_list;
  an_expr_stack_entry expr_stack_entry;
  an_arg_match_summary_ptr
                      arg_match_list;

  db_enter(4, "scan_ctor_arguments");
  *conversion_routine = NULL;
  start_position = pos_curr_token;
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  if (constructor_sym->kind == (a_symbol_kind)sk_member_function) {
    /* Constructor is not overloaded.  In this case, the argument types
       can be checked as the argument list is scanned. */
    routine_type = routine_symbol_type(constructor_sym);
  } else {
#if CHECKING
    if (constructor_sym->kind != (a_symbol_kind)sk_overloaded_function) {
      internal_error("scan_ctor_arguments: sym not function");
    }  /* if */
#endif  /* CHECKING */
    /* Constructor is overloaded. */
    overloaded_function_case = TRUE;
    routine_type = NULL;
  }  /* if */

  /* Scan the arguments. */
  scan_call_arguments(routine_type, /*already_after_left_paren=*/TRUE,
                      arg_expr_list, overloaded_function_case,
                      &arg_operand_list);
  error_position = start_position;

  if (overloaded_function_case) {
    /* The constructors are overloaded.  Select the proper one. */
    /* Note that a special case allows passing have_selector == TRUE and
       NULL for the selector operand when dealing with constructors. */
    constructor_sym = select_overloaded_function(constructor_sym,
                                                 /*have_selector=*/TRUE,
                                                 (an_operand *)NULL,
                                                 arg_operand_list,
                                                 ec_no_matching_constructor,
                                                 ec_ambiguous_constructor,
                                                 &start_position,
                                                 &arg_match_list);
    /* Build an expression-form argument list.  Convert the arguments on
       the argument list to the right types.  The call is done even
       when constructor_sym is NULL because it also frees arg_operand_list
       and arg_match_list. */
    /* Again, note that a special case allows passing have_selector == TRUE and
       NULL for the selector operand when dealing with constructors. */
    adjust_overloaded_function_call_arguments(constructor_sym,
                                              /*have_selector=*/TRUE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              arg_match_list,
                                              arg_expr_list);
  }  /* if */
  if (constructor_sym != NULL) {
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(
                                         constructor_sym, source_pos,
                                         object_class_type,
                                         /*honor_virtual=*/FALSE,
                                         curr_expr_is_potentially_evaluated(),
                                         /*suppress_access_check=*/FALSE);
    *conversion_routine = constructor_sym->variant.routine.ptr;
  }  /* if */
  pop_expr_stack();
  db_exit();
}  /* scan_ctor_arguments */


static void cast_pointer_for_field_selection(
                                   an_operand         *operand_1,
                                   a_type_ptr         class_struct_union_type,
                                   a_boolean          *is_arrow_operator,
                                   a_symbol_locator   *member_locator)
/*
Adjust the left operand of a "->" or "." operation, if necessary, to make
it point to a class/struct/union of the type containing the indicated member.
This is significant in C++, where the member may be in a base class of the
left-operand class, and baseward casts are needed.  operand_1 is the left
operand; class_struct_union_type is its type; *is_arrow_operator is TRUE
for "->", FALSE for "." (it will be set to TRUE on return if the operation
is normalized into "->" form); member_locator is a locator for the member
symbol (possibly a projection symbol).
*/
{
  a_symbol_ptr     member_sym = member_locator->specific_symbol;
  a_type_ptr       desired_class = member_sym->class_of_which_a_member;
  a_base_class_ptr bcp;

  /* This routine is similar to make_this_pointer_operand. */
  /* Drop any typedefs on the class type. */
  class_struct_union_type = skip_typerefs(class_struct_union_type);
  /* If the member is protected, it can only be accessed through an object
     or pointer of a type to which we have member access (ARM 11.5). */
  if (!member_locator->access_control_error_reported) {
    check_protected_member_access(member_sym, &member_locator->source_position,
				  class_struct_union_type);
  }  /* if */
  /* Do nothing if the type is already okay (which it almost always
     will be; only in cases involving qualified names can it be different). */
  if (class_struct_union_type != desired_class) {
    /* Some adjustment is required.  Find out how the classes are
       related to one another. */
    bcp = find_base_class_of(class_struct_union_type, desired_class);
    check_assertion(bcp != NULL);
    /* Cast the left operand to the proper type. */
    base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                            /*check_cast_access=*/
                               !member_locator->access_control_error_reported);
  }  /* if */
  /* If the member symbol is a projection symbol (i.e., it's inherited
     into the class where it is being referenced), cast the left operand
     down to the base class in which the fundamental symbol is defined.
     There's no access check on this part of the cast because the access
     to the fundamental base class was checked as part of determining access
     to the symbol. */
  if (member_sym->kind == (a_symbol_kind)sk_projection) {
    bcp = member_sym->variant.projection.extra_info->fundamental_base_class;
    base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                            /*check_cast_access=*/FALSE);
  }  /* if */
}  /* cast_pointer_for_field_selection */


static a_routine_ptr routine_from_function_operand(an_operand *operand)
/*
operand is the operand identifying the function to call in a normal call.
If it is possible to determine the specific function being called, return
a pointer to its routine entry.  Otherwise, return NULL.
*/
{
  a_routine_ptr  routine = NULL;
  a_constant_ptr con;

  if (is_constant_operand(operand)) {
    con = &operand->variant.constant;
    if (con_is_exact_addr_of_routine(con)) {
      routine = con->variant.address.variant.routine;
    }  /* if */
  }  /* if */
  return routine;
}  /* routine_from_function_operand */


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
  a_symbol_ptr      function_symbol, member_function_symbol;
  a_boolean         overloaded_function_case = FALSE;
  a_boolean         vacuous_destructor_case = FALSE;
  a_source_position call_position, first_arg_position;
  an_arg_match_summary
                    this_match_summary;
  an_arg_operand_ptr
                    arg_operand_list;
  a_routine_ptr     routine = NULL;
  a_boolean         already_after_left_paren = FALSE;

  db_enter(4, "scan_function_call");

  call_position = operand->position;
  if (curr_expr_kind_is_const()) {
    /* Routine calls not allowed in constant expressions. */
    error_in_operand(ec_bad_constant_function_call, operand);
  } else if (is_expression_operand(operand) &&
             is_operation_node(operand->variant.expression) &&
             operand->variant.expression->variant.operation.kind ==
                          (an_expr_operator_kind)eok_vacuous_destructor_call) {
    /* This operand was generated from a vacuous destructor call, e.g.,
       p->int::~int().
    */
    vacuous_destructor_case = TRUE;
    /* routine = NULL; -- already set. */
    /* Move to after the left parenthesis. */
    (void)get_token();
    first_arg_position = pos_curr_token;
    already_after_left_paren = TRUE;
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand->type) &&
        (member_function_symbol = opname_member_function_symbol(
                                      (an_opname_kind)onk_function_call,
                                      skip_typerefs(operand->type))) != NULL) {
      /* There is a C++ function call operator function that overloads function
         calls for the class of the left operand.  The operand becomes
         the selector object, and the function call operator routine
         becomes the operand. */
      copy_operand(operand, bound_function_selector);
      conv_class_operand_to_object_pointer(bound_function_selector);
      /* If the member symbol is a projection symbol (i.e., it's inherited
         into the class where it is being referenced), cast the operand
         down to the base class in which the fundamental symbol is defined.
         There's no access check on this part of the cast because the access
         to the fundamental base class was checked as part of determining
         access to the symbol. */
      if (member_function_symbol->kind == (a_symbol_kind)sk_projection) {
        a_base_class_ptr bcp = member_function_symbol->variant.projection.
                                            extra_info->fundamental_base_class;
        a_boolean        is_arrow_operator = TRUE;
        base_class_cast_operand(bound_function_selector, bcp,
                                &is_arrow_operator,
                                /*check_cast_access=*/FALSE);
      }  /* if */
      /* We can use an indefinite function operand whether the operator()
         function is overloaded or not. */
      make_indefinite_function_operand(member_function_symbol,
                                       /*is_qualified_name=*/FALSE, operand);
      bind_member_function_operand_to_selector(operand,
                                               bound_function_selector);
    }  /* if */
    /* If the operand is a qualified name for a member function
       (e.g., "A::f") convert it to a bound member function
       (e.g., "this->A::f").  This is done late so that A::f can be
       converted to a pointer-to-member implicitly in other contexts
       (that's an extension). */
    if (is_sym_for_member_operand(operand) &&
        is_a_function_designator(operand) &&
        !operand->bound_function /* probably unnecessary */) {
      a_symbol_ptr func_sym = operand->variant.symbol;
      if (make_this_pointer_operand(func_sym,
                                    &call_position,
                                    /*check_cast_access=*/
                                       !operand->access_control_error_reported,
                                    bound_function_selector)) {
        /* Make an operand for the function bound to the "this" pointer. */
        reduce_projection_symbol_to_fundamental_symbol(func_sym);
        make_function_designator_operand(func_sym,
                                         /*is_qualified_name=*/TRUE,
                                         &call_position,
                                         operand->ref_entries_list, operand);
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
    do_operand_transformations(operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
    /* Function designator must be an expression or an undefined symbol. */
    if (is_undefined_symbol_operand(operand)) {
      /* If the function designator was an undefined symbol, implicitly
         declare it now as a function. */
      a_symbol_ptr func_sym = operand->variant.symbol;
      decl_default_function(func_sym);
      /* Issue a low-severity diagnostic, not usually displayed.  In C++,
         issue an error (implicit declaration of functions is not allowed). */
      if (C_dialect == C_dialect_cplusplus) {
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
      conv_function_designator_to_ptr_to_function(operand);
      routine = func_sym->variant.routine.ptr;
      routine_type = routine_symbol_type(func_sym);
    } else if (is_indefinite_function_operand(operand)) {
      /* Overloaded function.  That means the routine type is not known yet. */
      overloaded_function_case = TRUE;
      overloaded_function_symbol = operand->variant.symbol;
      /* routine_type = NULL;  -- already set. */
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
      } else if (check_function_pointer_operand(operand)) {
        routine_type = type_pointed_to(operand->type);
        /* If we can tell which routine is being called, set routine to
           the routine entry.  Otherwise, leave it NULL. */
        routine = routine_from_function_operand(operand);
      }  /* if */
    }  /* if */
    /* Change the kind in the reference entry for the function from an
       address-taken entry back to a simple reference. */
    change_some_ref_kinds(operand->ref_entries_list, SRK_ADDRESS_TAKEN,
                          SRK_REFERENCE);
  }  /* if */

  /* Scan the arguments of the call. */
  scan_call_arguments(routine_type,
                      already_after_left_paren, &argument_list,
                      overloaded_function_case, &arg_operand_list);
  error_position = call_position;

  if (overloaded_function_case) {
    /* Choose the proper function out of a set of overloaded functions based
       on the argument types. */
    function_symbol = select_and_prepare_to_call_overloaded_function(
                                            overloaded_function_symbol,
                                            (a_boolean)operand->bound_function,
                                            bound_function_selector,
                                            arg_operand_list,
                                         (a_boolean)operand->is_qualified_name,
                                            ec_no_matching_function,
                                            ec_ambiguous_overloaded_function,
                                            /* NOT &operand->position; it
                                               causes aliasing problems in the
                                               subroutines. */
                                            &call_position,
                                            operand,
                                            &argument_list);
    if (function_symbol == NULL) {
      /* None of the overloaded functions matches the argument list. */
      make_error_operand(operand);
    } else {
      routine_type = routine_symbol_type(function_symbol);
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
                                     /*conversion_function_case=*/FALSE,
                                     routine,
                                     routine_type, &this_match_summary);
      if (this_match_summary.match_level != aml_none) {
        /* The types are compatible.  No cast is required; if there's a
           difference between the types, it's just a const/non-const
           difference. */
        /* Issue any needed warning (e.g., cfront anachronism of calling
           a non-const function with a const selector). */
        issue_warning_from_arg_match_summary(&this_match_summary,
                                             &bound_function_selector->
                                                                     position);
      } else {
        /* Some mismatch (more qualifiers on selector than on "this" parameter
           type). */
        pos_error(ec_unqual_function_with_qual_object,
                  &bound_function_selector->position);
        conv_to_error_operand(bound_function_selector);
      }  /* if */
    }  /* if */
  }  /* if */
  if (vacuous_destructor_case) {
    /* Vacuous destructor case; leave the original operand alone. */
    copy_operand(operand, result);
  } else {
    /* Build the call node and an operand for it. */
    assemble_function_call(operand, bound_function_selector, argument_list,
                           result);
  }  /* if */
  copy_source_position(call_position, error_position);

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
        if (other_field_sym->variant.field.ptr->bit_offset !=
            temp_field_sym->variant.field.ptr->bit_offset) {
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


static void do_field_selection_operation(
                               an_operand        *operand_1,
                               a_type_ptr        class_struct_union_type,
                               a_boolean         is_arrow_operator,
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
*is_arrow_operator is TRUE, "." otherwise.  field_sym points to the symbol
for the right-side field.  *member_position gives its position.  rep
points to an associated reference entry, or is NULL if none is needed.
The result is placed in *result.
*/
{
  a_field_ptr           field;
  a_type_ptr            result_type;
  a_boolean             rvalue_selection;
  a_type_ptr            selection_type;
  an_expr_operator_kind op;
  a_boolean             did_not_fold;
  an_operand            field_operand;
    
  if (is_error_operand(operand_1)) {
    make_error_operand(result);
  } else {
    field = field_sym->variant.field.ptr;
    if (cfront_compatibility_mode && is_array_type(field->type)) {
      /* cfront 2.1 fouls up the qualifiers on arrays.  Duplicate the
         behavior.  (This comes up in the NIH libraries.) */
      result_type = field->type;
    } else {
      /* The result type is set to the type of the field with the union of the
         qualifiers of the field and the qualifiers of the class, struct,
         or union. */
      result_type = type_plus_qualifiers_from_second_type(field->type,
                                                      class_struct_union_type);
    }  /* if */
    rvalue_selection = (!is_arrow_operator && is_an_rvalue(operand_1));
    if (rvalue_selection && C_dialect == C_dialect_cplusplus &&
        !cfront_compatibility_mode) {
      /* In C++, a member selected from an rvalue is an lvalue (ARM 5.2.4).
         If the selector is an rvalue, turn it back into an lvalue. */
      conv_class_operand_to_object_pointer(operand_1);
      is_arrow_operator = TRUE;
      rvalue_selection = FALSE;
    }  /* if */
    /* The operator is eok_value_field if the left operand is an rvalue and
       the selection was via the dot operator; otherwise it's the eok_field
       operator.  Note that in both the "lvalue . field" case and the
       "rvalue -> field" case, the left operand gives the address; the
       lvalue/rvalue representation difference cancels the "."/"->"
       difference. */
    /* Use different operators for the bit-field case: 
         eok_value_field -> eok_value_bit_field
         eok_field       -> eok_bit_field
    */
    if (rvalue_selection) {
      /* For "rvalue . field", the type of the selection is the same
         as the result type. */
      selection_type = result_type;
      op = field->is_bit_field ? (an_expr_operator_kind)eok_value_bit_field :
                                 (an_expr_operator_kind)eok_value_field;
    } else {
      /* For "lvalue . field" and "rvalue -> field", the type of the
         selection (giving, as it does, the address of the resulting
         lvalue) is pointer-to the field type. */
      selection_type = make_pointer_type(result_type);
      op = field->is_bit_field ? (an_expr_operator_kind)eok_bit_field :
                                 (an_expr_operator_kind)eok_field;
    }  /* if */
    did_not_fold = TRUE;
    if (is_constant_operand(operand_1) && curr_expr_is_evaluated() &&
        expr_stack->fold_constant_addr_exprs) {
      /* Don't try to fold bit fields except when their addresses
         can be taken (as an extension). */
      if (!field->is_bit_field
#if ADDR_OF_BIT_FIELD_ALLOWED
          || is_bit_field_whose_address_can_be_taken(field, &selection_type)
#endif /* ADDR_OF_BIT_FIELD_ALLOWED */
                              ) {
        /* Fold a field selection relative to a constant address into another
           constant address.  Note that the "rvalue . field" case can't come
           here, since a struct/union rvalue cannot be a constant.  This
           folding is always done in constant expressions, but in some
           nonconstant expressions it's not done because it's clearer to
           have the field selection in the IL (the constant form has only
           an offset, and loses the field name). */
        clear_operand((an_operand_kind)ok_constant, result);
        fold_field_selection(&operand_1->variant.constant, field,
                             selection_type, &result->variant.constant);
        did_not_fold = FALSE;
      }  /* if */
    }  /* if */
    if (did_not_fold) {
      if (curr_expr_kind_is_const() && curr_expr_is_evaluated()) {
        /* The operation must fold to a constant in a constant expression. */
        if (field->is_bit_field) {
          /* A bit-field selection cannot be folded.  There will be a
             warning or error issued later.  Nothing is needed now. */
        } else {
          /* Some other case (none expected, but for future expansion...). */
          pos_error(ec_expr_not_constant, member_position);
          make_error_operand(result);
        }  /* if */
      } else {
        /* Construct the field selection expression tree. */
        make_field_operand(field, &field_operand);
        build_binary_result_operand(operand_1, &field_operand, op,
                                    selection_type, result);
      }  /* if */
    }  /* if */
    /* The operand type has one less level of "pointer-to" than does the
       address if the result is an lvalue. */
    result->type = result_type;
    if (rvalue_selection) {
      /* "rvalue . field": the result is an rvalue. */
      result->state = (an_operand_state)os_rvalue;
    } else {
      /* Other cases: the result is an lvalue. */
      result->state = (an_operand_state)os_lvalue;
    }  /* if */
    /* In C++, a field may have a reference type.  An implicit indirection
       is done to get the value or address of the thing pointed to. */
    if (C_dialect == C_dialect_cplusplus && is_reference_type(result_type)) {
      add_reference_indirection(result);
    }  /* if */
    /* Preserve the reference entries for the base struct. */
    result->ref_entries_list = operand_1->ref_entries_list;
    if (rep != NULL) {
      /* Add the reference entry for the field to the list of entries for
         the operand. */
      rep->next_operand_ref = result->ref_entries_list;
      result->ref_entries_list = rep;
    }  /* if */
  }  /* if */
}  /* do_field_selection_operation */


static void do_member_function_selection_operation(
                                     an_operand       *operand_1,
                                     a_symbol_ptr     routine_sym,
                                     a_symbol_locator *locator,
                                     a_ref_entry_ptr  rep,
                                     an_operand       *result,
                                     an_operand       *bound_function_selector)
/*
Generate the operand for a nonstatic member function selection operation.
operand_1 is the left operand of the selection (the object).
routine_sym points to the member function symbol entry (possibly overloaded).
*locator is a locator for the member function symbol (needed to get the
projection symbol for the member and to know if a qualified name was used).
rep points to an associated reference entry, or is NULL if none is needed.
The operand is constructed in *result, and the bound function selector
object (usually, a copy of operand_1) is placed in *bound_function_selector.
*/
{
  if (routine_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* Overloaded function. */
    make_indefinite_function_operand(locator->specific_symbol,
                                     (a_boolean)locator->is_qualified_name,
                                     result);
  } else {
    /* Non-overloaded function. */
    make_function_designator_operand(routine_sym,
                                     (a_boolean)locator->is_qualified_name,
                                     &locator->source_position, rep, result);
  }  /* if */
  copy_operand(operand_1, bound_function_selector);
  bind_member_function_operand_to_selector(result, bound_function_selector);
}  /* do_member_function_selection_operation */


static void scan_field_selection_operator
                                 (an_operand         *operand_1,
                                  an_operand         *result,
                                  an_operand         *bound_function_selector)
/*
Scan the "." and "->" operators.  The left operand must be (a pointer to) a
class, struct, or union.  The right operand must be a member of the class,
struct, or union.  Return the result of the selection in *result.
If the field selection produces a bound function in C++, return the object
bound with the function in *bound_function_selector.
*/
{
  a_symbol_ptr          member_sym, projection_member_sym;
  a_boolean             is_arrow_operator;
  a_type_ptr            class_struct_union_type, orig_class_struct_union_type;
  a_boolean             err = FALSE, processed = FALSE, found_id = FALSE;
  a_boolean             operand_1_is_complete_class = FALSE, local_err;
  a_boolean             need_operand_1_type_check = FALSE;
  a_boolean             need_member_sym_check;
  a_ref_entry_ptr       rep;
  a_routine_ptr         routine_ptr;
  a_boolean             is_qualified_name;
  a_boolean             is_vacuous_destructor_reference = FALSE;
  a_source_position     member_position, qualified_member_position;
  an_identifier_options_set
                        gid_flags;
  a_type_ptr            dtor_type;

  db_enter(4, "scan_field_selection_operator");

  /* Remember if this was an arrow or a dot selector. */
  is_arrow_operator = (curr_token == tok_arrow);

  if (curr_expr_kind_is(ek_pp)) {
    /* Field selection not allowed in preprocessor expression. */
    pos_error(ec_bad_pp_operator, &pos_curr_token);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Field selection not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &pos_curr_token);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Field selection not allowed in a template argument expression. */
    pos_error(ec_bad_templ_arg_expr_operator, &pos_curr_token);
    err = TRUE;
  }  /* if */
  if (err) {
    /* Operation is not allowed in this kind of expression. */
    change_operand_refs_to_error(operand_1);
  } else {
    /* In C++, the first operand of "->" may be a class object that is
       converted to a class pointer via an operator->() function.  The operator
       is treated as a unary operator (i.e., the field following the "->" is
       not significant at this point).  If the operator function returns a
       class object or reference to class object, look for another
       operator->() function. */
    if (is_arrow_operator && C_dialect == C_dialect_cplusplus) {
      /* Note that we do not use "is_class_or_error_operand" here.  That's
         deliberate: doing so could cause infinite loops. */
      while (is_class_struct_union_type(operand_1->type)) {
        check_for_operator_overloading((an_opname_kind)onk_arrow,
                                       /*unary_operator=*/TRUE,  /* sic */
                                       /*must_be_member_function=*/TRUE,
                                       /*try_conversions=*/FALSE,
                                       /*has_predef_meaning=*/TRUE,
                                       operand_1, (an_operand *)NULL,
                                       &operand_1->position,
                                       result, &processed);
        if (!processed) break;
        copy_operand(result, operand_1);
      }  /* while */
    }  /* if */
    /* Do implicit operand transformations.  In the "." case, keep an lvalue
       if we have one. */
    do_operand_transformations(operand_1,
                               is_arrow_operator ?
                                    TOPT_NO_OPTIONS :
                                    TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
    /* The left operand must be (a pointer to) a struct or union. */
    if (is_error_operand(operand_1)) {
      err = TRUE;
    } else {
      if (is_arrow_operator) {
        /* "->" operator.  The left operand must be a pointer. */
        if (check_pointer_operand(operand_1, ec_expr_not_pointer)) {
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
      /* Drop any qualifiers or typedefs on the class/struct/union type. */
      class_struct_union_type = skip_typerefs(orig_class_struct_union_type);
      if (is_class_struct_union_type(class_struct_union_type)) {
        /* Instantiate the class if it is a template class. */
        instantiate_template_class(class_struct_union_type);
        operand_1_is_complete_class =
                                  !is_incomplete_type(class_struct_union_type);
      }  /* if */
      /* No error is issued yet if the first operand is not (a pointer to)
         a class, because (a) pcc mode allows fields to be selected from
         non-class pointers, and (b) C++ allows p->int::~int(). */
      need_operand_1_type_check = TRUE;
    }  /* if */
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  /* See if an identifier (or equivalent) is next. */
  gid_flags = GID_DTOR_RECOGNIZED | GID_IS_FIELD_SELECTION_OPERAND;
  if (C_dialect == C_dialect_cplusplus) {
    /* In C++, explicit calls of destructors are allowed for simple types
       and classes without destructors.  For example, p->int::~int(). */
    gid_flags |= GID_VACUOUS_DTOR_RECOGNIZED;
    if (err || !is_class_struct_union_type(class_struct_union_type)) {
      /* If the first operand is not a class, the vacuous destructor calls
         can be things like p->~int(). */
      gid_flags |= GID_DTOR_MUST_BE_NONCLASS;
    }  /* if */
  }  /* if */
  if (is_generalized_identifier_start(gid_flags | GID_DEFER_ACCESS_ERRORS)) {
    found_id = TRUE;
    member_sym = NULL;
    /* See if the name following the operator is a C++ qualified name, as
       in "p->A::x". */
    /* Leading "::" is allowed as of the Portland X3J16/WG21 meeting;
       cfront always allowed it. */
    is_qualified_name = coalesce_and_lookup_qualified_name(gid_flags,
                                                           ilm_normal,
                                                           &local_err);
    err |= local_err;
    /* If the member is something like "A::x", member_position will give
       the position of the "x" and qualified_member_position will give the
       position of the "A". */
    member_position = locator_for_curr_id.source_position;
    qualified_member_position = pos_curr_token;
    if (locator_for_curr_id.is_vacuous_destructor_reference) {
      /* We have something like p->int::~int, a reference to a vacuous
         destructor. */
      is_vacuous_destructor_reference = TRUE;
      need_operand_1_type_check = FALSE;
      /* Watch out for error cases like p->int::~float. */
      if (is_error_locator(locator_for_curr_id)) err = TRUE;
      if (!err) {
        /* Check that the type of the thing named on the right side
           is the same as the type of the left side, or a base class. */
        dtor_type = locator_for_curr_id.qualifier_class_type;
        dtor_type = skip_typerefs(dtor_type);
        if (types_are_compatible(class_struct_union_type, dtor_type) ||
            (is_class_struct_union_type(dtor_type) &&
             operand_1_is_complete_class &&
             is_same_class_or_base_class_thereof(class_struct_union_type,
                                                 dtor_type))) {
          /* Okay. */
        } else {
          /* This is an error case like
               float *p;
               p->int::~int();
          */
          pos_error(ec_vacuous_destructor_name_mismatch,
                    &qualified_member_position);
          err = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Not a vacuous destructor case, i.e., normal case. */
      need_member_sym_check = TRUE;
      /* Further checking beyond the fact that this is an identifier is not
         possible if there was an error in the first operand. */
      if (operand_1_is_complete_class) {
        if (is_qualified_name) {
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
            /* Make sure the name is a member of the class indicated by the
               left-hand side, or one of its base classes.  The NULL check
               is needed to catch cases like p->::x. */
            if (projection_member_sym->class_of_which_a_member == NULL ||
                !is_same_class_or_base_class_thereof(class_struct_union_type,
                                                     projection_member_sym->
                                                    class_of_which_a_member)) {
              pos_ty_error(ec_name_not_member_of_class_or_base_classes,
                           &qualified_member_position,
                           class_struct_union_type);
              err = TRUE;
            }  /* if */
          }  /* if */
          need_member_sym_check = FALSE;
        } else {
          /* Normal case: not qualified member name. */
          /* Look up this identifier in the scope of the class, struct, or
             union. */
          member_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                                 class_struct_union_type,
                                                 IDL_NO_OPTIONS);
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
              locator_for_curr_id.qualifier_class_type = dtor_type;
              need_member_sym_check = FALSE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      /* If the field was not found in pcc mode, look for any field with
         that name.  If there's only one (or several with the same offsets),
         cast the left-side variable to the right struct/union type and do
         the selection with the found field. */
      if (member_sym == NULL && C_dialect == C_dialect_pcc &&
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
          orig_class_struct_union_type = member_sym->class_of_which_a_member;
          class_struct_union_type =skip_typerefs(orig_class_struct_union_type);
          operand_1_is_complete_class = TRUE;
          cast_operand(make_pointer_type(class_struct_union_type),
                       operand_1, /*is_implicit_cast=*/FALSE);
          /* Mark the struct or union type as referenced, since a field
             therein has been referenced. */
          orig_class_struct_union_type->source_corresp.referenced = TRUE;
        }  /* if */
      }  /* if */
      if (member_sym == NULL && need_member_sym_check) {
        /* The identifier is not a member of the operand_1 class, struct,
           or union. */
        if (!operand_1_is_complete_class) {
          /* An error will be produced below because the first operand is
             not (a pointer to) a class, so do not issue an error here. */
        } else {
          pos_stsy_error(C_mode() ? ec_not_a_field : ec_not_a_member,
                         &error_position,
                         locator_for_curr_id.symbol_header->identifier,
                         (a_symbol_ptr)class_struct_union_type->
                                                    source_corresp.assoc_info);
        }  /* if */
        err = TRUE;
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
    if (is_incomplete_type(class_struct_union_type)) {
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
    change_operand_refs_to_error(operand_1);
  } else if (is_vacuous_destructor_reference) {
    an_expr_node_ptr node;
    /* A reference to a destructor for a class or simple type that does not
       have one, e.g., p->int::~int(). */
    if (!is_arrow_operator && is_an_lvalue(operand_1)) {
      /* "." operator.  Convert the lvalue to an rvalue pointer, then
         use "->" instead. */
      take_address_of_lvalue(operand_1);
      is_arrow_operator = TRUE;
    }  /* if */
    if (operand_1_is_complete_class && class_struct_union_type != dtor_type) {
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
                              /*check_cast_access=*/TRUE);
    }  /* if */
    /* Make an eok_vacuous_destructor_call node and an operand for it.
       This is a pretty weird representation for this case, but it's a pretty
       weird case.  scan_function_call checks for this construct. */
    node = make_node_from_operand(operand_1);
    node = make_operator_node(
                            (an_expr_operator_kind)eok_vacuous_destructor_call,
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
    } else {
      projection_member_sym = locator_for_curr_id.specific_symbol;
      /* See what kind of member we have. */
      switch (member_sym->kind) {
        case sk_field:
          /* Normal field selection. */
          /* This operation uses the left-side operand, so cast the
             operand to the type of the member symbol. */
          cast_pointer_for_field_selection(operand_1, class_struct_union_type,
                                           &is_arrow_operator,
                                           &locator_for_curr_id);
          do_field_selection_operation(operand_1, orig_class_struct_union_type,
                                       is_arrow_operator,
                                       member_sym, &member_position, rep,
                                       result);
          break;
        case sk_static_data_member:
          /* Static data member reference.  The value of the left operand is
             discarded. */
          discard_operand(operand_1);
          make_lvalue_variable_operand(
                              member_sym->variant.static_data_member.variable,
                              result, rep);
          break;
        case sk_member_function:
          /* Member function (static or non-static). */
          routine_ptr = member_sym->variant.routine.ptr;
          if (routine_type_is_nonstatic_member_function(routine_ptr->type)) {
            /* Nonstatic member function. */
            /* Also continue here for an overloaded function. */
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
              /* Cast the selector to the class of the member symbol. */
              cast_pointer_for_field_selection(operand_1,
                                               class_struct_union_type,
                                               &is_arrow_operator,
                                               &locator_for_curr_id);
              /* Make an operand for the function with the selector bound
                 to it. */
              do_member_function_selection_operation(operand_1,
                                                     member_sym,
                                                     &locator_for_curr_id,
                                                     rep,
                                                     result,
                                                     bound_function_selector);
            }  /* if */
          } else {
            /* Static member function.  Discard the left operand. */
            discard_operand(operand_1);
            make_function_designator_operand(member_sym,
                                             is_qualified_name,
                                             &member_position,
                                             rep,
                                             result);
          }  /* if */
          break;
        case sk_overloaded_function:
          /* Overloaded function.  Only overloaded member functions are
             possible here, since we've checked that the member symbol
             is part of the left operand class or one of its base classes. */
#if CHECKING
          if (member_sym->class_of_which_a_member == NULL) {
            internal_error(
                  "scan_field_selection_operator: overloaded func not member");
          }  /* if */
#endif /* CHECKING */
          /* We don't know yet whether or not we will need a selector
             object, so save the selector.  It will be discarded later
             if it is not needed. */
          goto nonstatic_member_function;
        case sk_constant:
          /* Member constant (e.g., an enumerator). */
          discard_operand(operand_1);
          make_sym_constant_operand(member_sym, result);
          break;
        case sk_type:
        case sk_class_or_struct_tag:
        case sk_union_tag:
        case sk_enum_tag:
          /* The identifier is a type identifier. */
          discard_operand(operand_1);
          error_and_make_error_operand(ec_type_identifier_not_allowed, result);
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
    (void)get_token();
  }  /* if */

  /* The position of the operand is the start position of the selection
     except when the operand is a bound function, in which case it's the
     position of the function name. */
  if (result->bound_function) {
    result->position = member_position;
  } else {
    result->position = operand_1->position;
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
  a_base_class_ptr  bcp;
  an_expr_node_ptr  select_node, object_node, pm_node;
  a_boolean         rvalue_selection;

  db_enter(4, "scan_ptr_to_member_operator");

  /* Remember if this was an arrow or a dot selector. */
  is_arrow_operator = (curr_token == tok_arrow_star);
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

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
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
  } else {
    if (is_arrow_operator &&
        (is_class_or_error_operand(operand_1) ||
         is_class_or_error_operand(&operand_2))) {
      /* Look for C++ operator overloading cases ("->*" only). */
      check_for_operator_overloading((an_opname_kind)onk_arrow_star,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
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
            if (C_dialect == C_dialect_cplusplus) {
              err_code = is_arrow_operator ? ec_expr_not_ptr_to_class :
                                             ec_expr_not_class;
            } else {
              err_code = is_arrow_operator ?
                                         ec_expr_not_ptr_to_struct_or_union :
                                         ec_expr_not_struct_or_union;
            }  /* if */
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
        if (operand_1_type == operand_2_class) {
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
        /* Get an operand for the address of the object. */
        conv_selector_to_object_pointer(operand_1, &is_arrow_operator);
        /* Cast the left operand to a base class if necessary.  This does the
           ambiguity and accessibility checking. */
        if (bcp != NULL) {
          base_class_cast_operand(operand_1, bcp, &is_arrow_operator,
                                  /*check_cast_access=*/TRUE);
        }  /* if */
        /* The result type is the member type pointed to by the second
           operand. */
        result_type = pm_member_type(operand_2_type);
        if (is_function_type(result_type)) {
          /* Result is a bound function.  It can only be called or (as an
             anachronism) cast to a normal function pointer. */
          copy_operand(&operand_2, result);
          copy_operand(operand_1, bound_function_selector);
          bind_member_function_operand_to_selector(result,
                                                   bound_function_selector);
        } else {
          /* Result is a data member.  Use an eok_pm_field node with the
             pointer to object and pointer-to-member as the operands. */
          object_node = make_node_from_operand(operand_1);
          /* The result is an lvalue if the operator is "->*" or if the
             first operand is an lvalue.  Note that this is not what the ARM
             says (it says the result is an lvalue if the *second* operand is
             an lvalue, but that doesn't make sense).  It *is* the same
             rule used for "->" and ".". */
          rvalue_selection = (!is_arrow_operator && is_an_rvalue(operand_1));
          pm_node = make_node_from_operand(&operand_2);
          object_node->next = pm_node;
          select_node = make_operator_node((an_expr_operator_kind)eok_pm_field,
                                           make_pointer_type(result_type),
                                           object_node);
          if (rvalue_selection) {
            /* If the result is not an lvalue, add an indirection to turn
               the data member address into the data member value. */
            select_node = add_indirection_to_node(select_node);
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
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);
  db_exit();
}  /* scan_ptr_to_member_operator */


static void scan_postfix_incr_decr(an_operand *operand,
				   an_operand *result)
/*
Scan the postfix increment ("++") and decrement ("--") operators.  See section
3.3.2.4 of the standard.
*/
{
  an_expr_operator_kind op;
  a_type_ptr            result_type;
  a_boolean             err = FALSE, processed = FALSE;
  an_operand            zero_operand;
  an_opname_kind        opname_kind;

  db_enter(4, "scan_postfix_incr_decr");

  if (curr_expr_kind_is_const()) {
    /* Postfix ++/-- not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &pos_curr_token);
    make_error_operand(result);
    change_operand_refs_to_error(operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_or_error_operand(operand)) {
      /* Look for C++ operator overloading cases. */
      /* Note that postfix ++/-- use a two-argument function to distinguish
         them from the prefix ++/--, which use a one-argument function.
         See ARM 13.4.7.  The second compiler-supplied argument is an
         integer zero. */
      make_integer_constant_operand(&zero_operand, 0L);
      opname_kind = opname_kind_for_token[(int)curr_token];
      check_for_operator_overloading(opname_kind,
                                     /*unary_operator=*/FALSE,  /* sic! */
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/!allow_anachronisms,
                                     /*has_predef_meaning=*/allow_anachronisms,
                                     operand, &zero_operand,
                                     &operand->position,
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
                                       &operand->position,
                                       result, &processed);
        if (processed) {
          if (!is_error_operand(result)) {
            pos_st_diagnostic(anachronism_error_severity,
                              ec_single_arg_postfix_incr_decr_anachronism,
                              &operand->position,
                              token_names[(int)curr_token]);
          }  /* if */
        } else {
          /* The anachronism does not apply, so try finding a conversion
             function that will convert the operand to the right type for
             the builtin version of the operator.  Note that this call
             will also try the normal match again, and fail. */
          check_for_operator_overloading(opname_kind,
                                         /*unary_operator=*/FALSE,  /* sic! */
                                         /*must_be_member_function=*/FALSE,
                                         /*try_conversions=*/TRUE,
                                         /*has_predef_meaning=*/FALSE,
                                         operand, &zero_operand,
                                         &operand->position,
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
          if (!check_object_pointer_operand(operand,
                                            ec_expr_not_pointer_to_object)) {
            err = TRUE;
          }  /* if */
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
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
      } else if (!check_modifiable_lvalue_operand(operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(operand, /*value_used=*/TRUE);
        result_type = operand->type;
        if (curr_token == tok_plus_plus) {
          switch (skip_typerefs(result_type)->kind) {
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
          switch (skip_typerefs(result_type)->kind) {
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
      }  /* if */
      if (err) {
        /* Error of some kind. */
        make_error_operand(result);
      } else {
        build_unary_result_operand(operand, op, result_type, result);
      }  /* if */
    }  /* if */
  }  /* if */

  /* Get past the "++" or "--". */
  (void)get_token();
  copy_source_position(operand->position, error_position);
  copy_source_position(operand->position, result->position);

  db_exit();
}  /* scan_postfix_incr_decr */


static void change_assignment_result_to_lvalue(an_operand *result,
                                               an_operand *lvalue_operand)
/*
In C++ mode, assignment operators and prefix ++/-- return lvalues.
Change the operation in *result from an rvalue-returning operation to
an lvalue-returning operation.  *lvalue_operand is the operand for
the lvalue being operated upon.  This routine is called only in C++ mode.
*/
{
  an_expr_node_ptr node;

  if (!is_error_operand(result)) {
    node = result->variant.expression;
    node->variant.operation.returns_lvalue_instead_of_usual_rvalue = TRUE;
    node->type = make_pointer_type(node->type);
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
  an_expr_operator_kind op;
  a_boolean             is_increment;
  a_type_ptr            result_type;
  a_boolean             err = FALSE, processed = FALSE;

  db_enter(4, "scan_prefix_incr_decr");

  save_token = curr_token;
  is_increment = (curr_token == tok_plus_plus);
  copy_source_position(pos_curr_token, start_position);

  if (curr_expr_kind_is_const()) {
    /* Prefix ++ and -- are not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator not allowed in this kind of expression. */
    make_error_operand(result);
    change_operand_refs_to_error(&operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_or_error_operand(&operand)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     &start_position,
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
          if (!check_object_pointer_operand(&operand,
                                            ec_expr_not_pointer_to_object)) {
            err = TRUE;
          }  /* if */
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
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
      } else if (!check_modifiable_lvalue_operand(&operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(&operand, /*value_used=*/TRUE);
        result_type = operand.type;
        if (is_increment) {
          switch (skip_typerefs(result_type)->kind) {
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
          switch (skip_typerefs(result_type)->kind) {
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
      }  /* if */
      if (err) {
        /* Error of some kind. */
        make_error_operand(result);
      } else {
        build_unary_result_operand(&operand, op, result_type, result);
        /* In C++, the prefix ++ and -- operators return lvalues. */
        if (C_dialect == C_dialect_cplusplus) {
          change_assignment_result_to_lvalue(result, &operand);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

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
  a_boolean         err = FALSE, processed = FALSE;
  a_symbol_ptr      member_proj_sym, member_sym;

  db_enter(4, "scan_ampersand_operator");

  /* Save the source position of the operator. */
  copy_source_position(pos_curr_token, start_position);

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
  /* Scan the operand. */
  scan_expr(&operand, PREC_PREFIX, EOPT_OPERAND_OF_ADDRESS_OF);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
    change_operand_refs_to_error(&operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_or_error_operand(&operand)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_ampersand,
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     &operand, (an_operand *)NULL,
                                     &start_position,
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
                                 TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION);
      if (is_an_lvalue(&operand)) {
        if (C_dialect == C_dialect_pcc && is_array_type(operand.type)) {
          /* In pcc mode "&array" is the same as "array" implicitly converted
             to a pointer.  It has type "pointer-to-array-element" rather than
             "pointer to array" as in ANSI. */
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
        conv_function_designator_to_ptr_to_function(&operand);
        /* Note that the copy preserves ref_entries_list. */
        copy_operand(&operand, result);
      } else if (is_sym_for_member_operand(&operand)) {
        /* The operand is a qualified name for a nonstatic data member, so
           the "&" operator returns a pointer-to-member. */
        member_proj_sym = operand.variant.symbol;
        member_sym = fundamental_symbol_of(member_proj_sym);
        check_assertion(member_sym->kind == (a_symbol_kind)sk_field);
        /* Change the kind in the reference entries to address-taken. */
        change_ref_kinds(operand.ref_entries_list, SRK_ADDRESS_TAKEN);
        if (member_sym->variant.field.ptr->is_bit_field) {
          /* Cannot take the address of a bit field. */
          error_in_operand(ec_address_of_bit_field, &operand);
          make_error_operand(result);
        } else {
          /* Make an operand for a pointer-to-member constant. */
          make_ptr_to_member_constant_operand(
                                        member_sym,
                                        member_proj_sym,
                                        &start_position,
                                        !operand.access_control_error_reported,
                                        /*is_operand_of_address_of=*/TRUE,
                                        result);
        }  /* if */
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

  error_position = start_position;
  result->position = start_position;

  db_exit();
}  /* scan_ampersand_operator */


static void scan_indirection_operator(an_operand *result)
/*
Scan the "*" (indirection) operator.  The operand must have type pointer.
See section 3.3.3.2 of the standard.
*/
{
  an_operand        operand;
  a_source_position start_position;
  a_boolean         err = FALSE, processed = FALSE;

  db_enter(4, "scan_indirection_operator");

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

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
    change_operand_refs_to_error(&operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_or_error_operand(&operand)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_star,
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     &start_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(&operand, TOPT_NO_OPTIONS);
      if (check_pointer_operand(&operand, ec_bad_indirection_operand)) {
        operand.type = type_pointed_to(operand.type);
        /* Instantiate the underlying type if it is a template class. */
        check_for_uninstantiated_template_class(operand.type);
        if (is_function_type(operand.type)) {
          /* This will become a function designator. */
          operand.state = (an_operand_state)os_function_designator;
        } else if (is_void_type(operand.type) &&
                   !is_qualified_type(operand.type)) {
          /* If the type pointed to is void, the result is not an lvalue.
               void *p; *p;
             is legal, but *p is not an lvalue. */
          an_expr_node_ptr node = make_node_from_operand(&operand);
          node = make_operator_node((an_expr_operator_kind)eok_indirect,
                                    operand.type, node);
          make_expression_operand(node, node->type, &operand);
        } else {
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

  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_indirection_operator */


static a_boolean is_nonarithmetic_type(a_type_ptr type)
/*
Return TRUE if type is a nonarithmetic type.  This differs from
!is_arithmetic_type(type) in that it returns FALSE for an error type.
*/
{
  a_boolean is_nonarith;

  if (is_arithmetic_type(type)) {
    is_nonarith = FALSE;
  } else if (is_error_type(type)) {
    is_nonarith = FALSE;
  } else {
    is_nonarith = TRUE;
  }  /* if */
  return is_nonarith;
}  /* is_nonarithmetic_type */


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
  a_type_ptr            result_type;
  a_boolean             did_not_fold, template_constant;
  a_boolean             do_promotion, processed = FALSE;
  a_constant            result_constant;

  db_enter(4, "scan_arith_prefix_operator");

  save_token = curr_token;
  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      is_nonarithmetic_type(operand.type)) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &start_position);
    make_error_operand(result);
    change_operand_refs_to_error(&operand);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             is_class_or_error_operand(&operand)) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/TRUE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   &operand, (an_operand *)NULL,
                                   &start_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    do_operand_transformations(&operand, TOPT_NO_OPTIONS);
    /* Check the type of the operand. */
    do_promotion = TRUE;
    switch (save_token) {
      case tok_plus:
        /* op does not need to be set -- see below. */
        if (C_dialect == C_dialect_cplusplus &&
            is_pointer_type(operand.type)) {
          /* In C++, the operand may be a pointer (ARM 5.3). */
        } else {
          /* In C++ or C, the operand may be arithmetic. */
          (void)check_arithmetic_operand(&operand);
        }  /* if */
        break;
      case tok_not:
        (void)check_boolean_controlling_expr(&operand);
        op = (an_expr_operator_kind)eok_not;
        do_promotion = FALSE;
        result_type = integer_type((an_integer_kind)ik_int);
        break;
      case tok_minus:
        (void)check_arithmetic_operand(&operand);
        if (is_floating_type(operand.type)) {
          op = (an_expr_operator_kind)eok_fnegate;
        } else {
          op = (an_expr_operator_kind)eok_inegate;
        }  /* if */
        break;
      case tok_compl:
        (void)check_integral_operand(&operand);
        op = (an_expr_operator_kind)eok_complement;
        break;
#if CHECKING
      default:
        internal_error("scan_arith_prefix_operator: bad operator");
#endif /* CHECKING */
    }  /* switch */

    if (do_promotion) {
      /* Do integral promotions if required. */
      promote_operand(&operand);
      result_type = operand.type;
    }  /* if */

    if (is_error_operand(&operand)) {
      make_error_operand(result);
    } else {
      if (save_token == tok_plus) {
        /* The result of a unary plus is the promoted operand. */
        copy_operand(&operand, result);
      } else {
        /* Other operators (not unary "+"). */
        did_not_fold = TRUE;
        template_constant = FALSE;
        if (is_constant_operand(&operand)) {
          /* Fold the operation if the operand is constant.  In a nonconstant
             context, reduce any error to a warning and leave the operation
             to be done at runtime. */
          unary_operation(op, &operand.variant.constant,
                          result_type, &result_constant,
                          curr_expr_kind_is_const(),
                          curr_expr_is_evaluated(),
                          &did_not_fold, &template_constant, &start_position);
        }  /* if */
        if (did_not_fold) {
          if (template_constant) {
            /* For an expression based on a template parameter, scanned
               during the prototype instantiation, make a ck_template_param
               constant for the result. */
            make_template_param_expr_constant_operand(&operand,
                                                      (an_operand *)NULL, op,
                                                      result_type, result);
          } else if (curr_expr_kind_is_const() && curr_expr_is_evaluated()) {
            /* A constant operation could not be folded in a constant
               expression. */
            pos_error(ec_expr_not_constant, &start_position);
            make_error_operand(result);
          } else {
            /* The operation could not be folded to a constant, so build
               an expression node. */
            build_unary_result_operand(&operand, op, result_type, result);
          }  /* if */
        } else {
          /* The operation was folded to a constant. */
          make_constant_operand(&result_constant, result);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_arith_prefix_operator */


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
  an_operand            operand;
  a_constant            constant;
  a_boolean             trapped_left_paren = FALSE;
  a_boolean             parenthesized_type = FALSE;
  a_type_ptr            sizeof_type;
  a_local_expr_options_set
                        local_options;
  an_expr_stack_entry   expr_stack_entry;

  db_enter(4, "scan_sizeof_operator");

#if CHECKING
  if (curr_expr_kind_is(ek_pp)) {
    /* Sizeof not possible for preprocessing expressions. */
    internal_error("scan_sizeof_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry);
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
    copy_source_position(pos_curr_token, lparen_position);
    (void)get_token();
    if (is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                         /*real_declarator_allowed=*/FALSE,
                         /*single_type_required=*/TRUE)) {
      /* This is a type-name in parentheses. */
      parenthesized_type = TRUE;
    } else {
      /* This is an expression in parentheses. */
      trapped_left_paren = TRUE;
    }  /* if */
  }  /* if */

  if (parenthesized_type) {
    /* Scan the type-name for a parenthesized type. */
    copy_source_position(pos_curr_token, type_position);
    add_matching_stop_token(tok_rparen);
    type_name(&sizeof_type);
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_matching_stop_token(tok_rparen);
    /* If the top type is a reference, drop the reference so that the sizeof
       applies to the type referenced (ARM 5.3.2). */
    if (is_reference_type(sizeof_type)) {
      sizeof_type = type_pointed_to(sizeof_type);
    }  /* if */
  } else {
    /* It has been determined that the operand of the sizeof is an expression
       and not a type.  Scan the operand. */
    local_options = EOPT_NO_OPTIONS;
    if (trapped_left_paren) local_options |= EOPT_TRAPPED_LEFT_PAREN;
    scan_expr(&operand, PREC_PREFIX, local_options);
    /* Do not convert a type of "routine returning type" to "pointer to
       routine returning type".  See section 3.2.2.1 in the C standard.
       Likewise do not convert arrays to pointers, or lvalues to rvalues. */
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION);
    if (trapped_left_paren) {
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
  }  /* if */

  sizeof_type = skip_typerefs(sizeof_type);
  /* Instantiate the type if it is a template class. */
  check_for_uninstantiated_template_class(sizeof_type);
  /* The operand of a sizeof may not have function type or incomplete type. */
  if (is_function_type(sizeof_type)) {
    pos_error(ec_sizeof_function, &type_position);
    sizeof_type = error_type();
  } else if (is_incomplete_type(sizeof_type)) {
    pos_error(ec_incomplete_type_not_allowed, &type_position);
    sizeof_type = error_type();
  }  /* if */

  /* The result of a sizeof is an integer indicating the size of the operand
     in bytes, of type size_t (see 3.3.3.4 and <stddef.h>). */
  if (is_error_type(sizeof_type)) {
    set_error_constant(&constant);
  } else {
    set_unsigned_integer_constant(&constant, (unsigned long)sizeof_type->size,
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND);
  }  /* if */
  make_constant_operand(&constant, result);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  pop_expr_stack();

  db_exit();
}  /* scan_sizeof_operator */


static void scan_alignof_operator(an_operand *result)
/*
Scan the __ALIGNOF__ operator.  This is an extension that is similar
to sizeof, but returns the alignment requirement rather than the size.

Syntax:
        __ALIGNOF__ ( type-name )
        __ALIGNOF__ ( expression )

The parentheses are required, unlike for sizeof.  Fewer error checks
are done.  A warning about the use of this nonstandard feature would
be inappropriate, because the feature is probably used to implement
<stdarg.h>, a standard feature.
*/
{
  a_source_position   start_position;
  an_operand          operand;
  a_constant          constant;
  a_type_ptr          alignof_type;
  an_expr_stack_entry expr_stack_entry;

  db_enter(4, "scan_alignof_operator");

  push_expr_stack((an_expression_kind)ek_sizeof, &expr_stack_entry);
  expr_stack_entry.evaluated = FALSE;
  expr_stack_entry.potentially_evaluated = FALSE;
  /* Save the position of the __ALIGNOF__ keyword. */
  copy_source_position(pos_curr_token, start_position);
  (void)get_token();
  /* Check for and pass over the left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  if (is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                       /*real_declarator_allowed=*/FALSE,
                       /*single_type_required=*/TRUE)) {
    /* Scan a type name. */
    type_name(&alignof_type);
  } else {
    /* Scan an expression. */
    scan_expr(&operand, PREC_LOWEST, EOPT_NO_OPTIONS);
    /* Do not convert lvalues to rvalues, arrays to pointers,
       or functions to pointers. */
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION);
    alignof_type = operand.type;
  }  /* if */
  alignof_type = skip_typerefs(alignof_type);
  /* Instantiate the type if it is a template class. */
  check_for_uninstantiated_template_class(alignof_type);
  /* The result of __ALIGNOF__ is an integer indicating the alignment of
     the operand, of type size_t. */
  if (is_error_type(alignof_type)) {
    set_error_constant(&constant);
  } else {
    set_unsigned_integer_constant(&constant,
                                  (unsigned long)alignof_type->alignment,
                                  (an_integer_kind)TARG_SIZE_T_INT_KIND);
  }  /* if */
  make_constant_operand(&constant, result);
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  pop_expr_stack();

  db_exit();
}  /* scan_alignof_operator */


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
  a_source_position   start_position;
  a_constant          constant;
  a_boolean           err;
  an_expr_stack_entry expr_stack_entry;

  db_enter(4, "scan_intaddr_operator");

  push_expr_stack((an_expression_kind)ek_init_constant, &expr_stack_entry);
  /* Save the position of the __INTADDR__ keyword. */
  copy_source_position(pos_curr_token, start_position);
  /* Check for and pass over the left parenthesis. */
  (void)get_token();
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_matching_stop_token(tok_rparen);
  /* Scan the address expression. */
  scan_expr(result, PREC_LOWEST, EOPT_NO_OPTIONS);
  do_operand_transformations(result, TOPT_NO_OPTIONS);
  err = is_error_operand(result);
  if (!err) {
    /* Make a constant from the operand. */
    extract_constant_from_operand(result, &constant);
    /* Check that the constant is represented as an integer.  If this is not
       the case, a user is using __INTADDR__; presumably, offsetof would
       be using it correctly. */
    if (is_error_type(constant.type)) {
      err = TRUE;
    } else if (constant.kind != (a_constant_repr_kind)ck_integer) {
      /* The error message doesn't have to be very good, since the
         user is using something that is probably undocumented. */
      error(ec_expr_not_integral_constant);
      err = TRUE;
    } else {
      /* Address constant is okay. */
    }  /* if */
  }  /* if */
  if (err) {
    conv_to_error_operand(result);
  } else {
    /* Cast the constant to type size_t. */
    cast_operand(integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND),
                 result, /*is_implicit_cast=*/TRUE);
  }  /* if */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_matching_stop_token(tok_rparen);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  pop_expr_stack();

  db_exit();
}  /* scan_intaddr_operator */


static a_routine_ptr select_delete_routine(a_type_ptr        delete_type,
                                           a_boolean         use_global_delete,
                                           a_boolean         array_delete,
                                           a_source_position *delete_position)
/*
Determine the delete routine to be used to delete an object of type
delete_type, and return a pointer to the routine entry.  If use_global_delete
is TRUE, return ::operator delete.  If array_delete is TRUE, return the
right delete routine for an array.  The symbol is marked as referenced
at *delete_position, but the IL entry is not marked as referenced.
*/
{
  a_symbol_ptr     operator_delete_symbol = NULL;
  a_symbol_locator locator_for_delete;

  /* Select the proper "delete" routine.  If the type is a class type and
     the class has a "delete" operator, use it.  However, if use_global_delete
     or array_delete is TRUE, use the global ::operator delete. */
  if (!use_global_delete && !array_delete &&
      is_class_struct_union_type(delete_type)) {
    operator_delete_symbol = opname_member_function_symbol(
                                                    (an_opname_kind)onk_delete,
                                                    delete_type);
    if (operator_delete_symbol != NULL) {
      make_locator_for_symbol(operator_delete_symbol, &locator_for_delete);
      locator_for_delete.source_position = *delete_position;
      check_ambiguity_and_verify_access(&locator_for_delete);
      operator_delete_symbol = fundamental_symbol_of(operator_delete_symbol);
    }  /* if */
  }  /* if */
  if (operator_delete_symbol == NULL) {
    /* Use the global operator "delete". */
    operator_delete_symbol= opname_function_symbol((an_opname_kind)onk_delete);
  }  /* if */
  /* Since delete cannot be overloaded, the symbol should not be
     overloaded or a function template. */
  check_assertion(operator_delete_symbol->kind == (a_symbol_kind)sk_routine ||
                  operator_delete_symbol->kind ==
                                            (a_symbol_kind)sk_member_function);
  /* Mark the routine symbol referenced, but not the IL entry (yet). */
  reference_to_symbol(SRK_REFERENCE, operator_delete_symbol,
                      delete_position, /*update_il_entry=*/FALSE);
  return operator_delete_symbol->variant.routine.ptr;
}  /* select_delete_routine */


static a_dynamic_init_ptr add_array_nonconstant_aggregate_init(
                                         a_dynamic_init_ptr element_dip,
                                         a_type_ptr         elem_type,
                                         a_targ_size_t      number_of_elements)
/*
Change the indicated dynamic initialization into a dynamic initialization
for each member of an array of classes.  elem_type is the type of the array
elements.  number_of_elements is the number of elements in the array, or 0
if the number of elements is variable (and known only at runtime).  Return
a pointer to the dynamic init entry for the entire array.
*/
{
  a_dynamic_init_ptr  array_dip;

  /* The IL structure is
       dynamic init (ck_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init (ck_constructor)
  */
  array_dip =
          alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
  repeat_nonconstant_init(element_dip, elem_type, array_dip,
                          number_of_elements);
  return array_dip;
}  /* add_array_nonconstant_aggregate_init */


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
  a_source_position placement_position;
  a_type_ptr        new_type, base_new_type, ptr_new_type;
  a_type_ptr        unqual_new_type;
  an_expr_node_ptr  new_array_dimension, sizeof_node;
  an_operand        sizeof_operand;
  a_boolean         use_global_new = FALSE;
  a_symbol_ptr      operator_new_symbol, function_symbol, ctor_sym;
  a_routine_ptr     ctor_routine;
  a_boolean         needs_initialization, trapped_left_paren;
  an_expr_node_ptr  arg_expr_list, init_val_node;
  a_constant        sizeof_constant;
  an_arg_operand_ptr
                    arg_operand_list, sizeof_arg_operand;
  an_expr_node_ptr  dummy;
  a_boolean         placement_new = FALSE, array_new = FALSE;
  a_targ_size_t     effective_num_of_elements;
  an_arg_match_summary_ptr
                    arg_match_list = NULL;

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

  (void)get_token();
  /* Check for the presence of the "placement" term, which provides extra
     arguments for the operator new function.  It is a list of expressions
     in parentheses. */
  arg_operand_list = NULL;
  copy_source_position(pos_curr_token, placement_position);
  trapped_left_paren = FALSE;
  if (curr_token == tok_lparen) {
    (void)get_token();
    /* Both the placement term and the type can start with a parenthesis.
       Look inside to tell them apart.  For example:
         new (int(1.5)) A     // placement
         new (int(*  ))       // type
    */
    if (is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                         /*real_declarator_allowed=*/FALSE,
                         /*single_type_required=*/TRUE)) {
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
        scan_call_arguments((a_type_ptr)NULL,
                            /*already_after_left_paren=*/TRUE,
                            &dummy, /*overloaded_function_case=*/TRUE,
                            &arg_operand_list);
      }  /* if */
    }  /* if */
  }  /* if */
  copy_source_position(pos_curr_token, type_position);
  /* Scan the new-type-name or ( type-name ). */
  new_type_name(trapped_left_paren, &new_type);
  unqual_new_type = skip_typerefs(new_type);
  /* Instantiate the type if it is a template class. */
  check_for_uninstantiated_template_class(new_type);
  /* Determine the type of pointer returned from "new". */
  base_new_type = new_type;
  new_array_dimension = NULL;
  if (is_array_type(new_type)) {
    /* A "new" of an array returns a pointer to the initial element.
      Note that this is only done for one level, e.g., new int [i][10]
      returns int (*)[10] not int * (ARM 5.3.3). */
    base_new_type = array_element_type(new_type);
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
  ptr_new_type = make_pointer_type(base_new_type);
  /* The operand of a new must be an object type. */
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
    /* The type is an abstract class type or a type that contains one,
       so an object of the type cannot be allocated. */
    pos_error(ec_abstract_class_object_not_allowed, &type_position);
    err = TRUE;
  } else {
    /* Valid type. */
    /* Compute the allocation size in bytes. */
    if (new_array_dimension != NULL) {
      a_type_ptr element_type = skip_typerefs(base_new_type);
      /* The type is a variable-dimension array, as in
           new char[i+1]
         The amount to allocate is the size of the array element times
         the expression giving the number of elements. */
      /* Cast the dimension expression to size_t (it's already an integral
         type). */
      cast_node(&new_array_dimension,
                integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND),
                /*is_implicit_cast=*/TRUE, &error_position);
      if (element_type->size == 1) {
        /* If the element size is 1, skip the multiplication. */
        sizeof_node = new_array_dimension;
      } else {
        /* Multiply the number of elements by the size of each element. */
        sizeof_node = node_for_integer_constant((long)element_type->size,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
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
      */
      set_integer_constant(&sizeof_constant,
                           (long)unqual_new_type->size,
                           (an_integer_kind)TARG_SIZE_T_INT_KIND);
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
       the keyword "new", always use the global ::new.  Also note that
       since we test new_type instead of base_new_type, we will use
       the global ::new for arrays of classes, as we should. */
    operator_new_symbol = NULL;
    if (is_class_struct_union_type(new_type) && !use_global_new) {
      operator_new_symbol = opname_member_function_symbol(
                                                       (an_opname_kind)onk_new,
                                                       unqual_new_type);
    }  /* if */
    if (operator_new_symbol == NULL) {
      /* Use the global operator "new". */
      operator_new_symbol = opname_function_symbol((an_opname_kind)onk_new);
    }  /* if */
    /* Select the proper "new" function if there are several.  Note that
       this call does not adjust the argument types or build the function
       call, since we may yet fold the call into a constructor call. */
    function_symbol = select_overloaded_function(
                                              operator_new_symbol,
                                              /*have_selector=*/FALSE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              ec_no_matching_new_function,
                                              ec_ambiguous_overloaded_function,
                                              &placement_position,
                                              &arg_match_list);
    /* We check later for function_symbol != NULL.  We don't set err
       here for that case because it shouldn't affect the scanning of
       the initial value. */
  }  /* if */
  /* See if the object has or needs initialization.  Note that we need to
     scan the initializer (if there is one) even if an error was detected
     above. */
  needs_initialization = FALSE;
  if (array_new) {
    /* Array new.  Determine the effective number of elements. */
    if (new_array_dimension != NULL) {
      /* Variable-length array; count is deferred to runtime. */
      effective_num_of_elements = 0;
    } else {
      effective_num_of_elements =
                    unqual_new_type->variant.array.variant.number_of_elements;
    }  /* if */
    while (is_array_type(base_new_type)) {
      /* For multi-dimensional arrays: even though only one level of array is
         dropped to determine the pointer type and to do allocation, all levels
         must be dropped to get the real base type to do initialization.  In
         particular, we want to know if the underlying type of a
         multi-dimensional array is a class, so we can know whether or not
         to call a constructor. */
      base_new_type = skip_typerefs(base_new_type);
      check_assertion(!base_new_type->variant.array.is_variable_size_array);
      effective_num_of_elements *=
                    base_new_type->variant.array.variant.number_of_elements;
      base_new_type = array_element_type(base_new_type);
    }  /* while */
  }  /* if */
  /* Set ctor_sym non-NULL if the type is a class that has a constructor
     or an array with elements of such a class. */
  ctor_sym = NULL;
  ctor_routine = NULL;
  if (is_class_struct_union_type(base_new_type)) {
    ctor_sym = symbol_supplement_for_class(base_new_type)->constructor;
  }  /* if */
  if (ctor_sym != NULL) {
    /* Class with a constructor.  Initialization is required. */
    if (curr_token == tok_lparen) {
      /* There is a new-initializer.  It's treated as a constructor call. */
      a_source_position  lparen_pos;
      copy_source_position(pos_curr_token, lparen_pos);
      (void)get_token();
      if (array_new) {
        /* No initializer may be specified for an array type. */
        pos_error(ec_initializer_not_allowed_on_array_new, &lparen_pos);
        err = TRUE;
      }  /* if */
      /* No need to add tok_rparen to the stop tokens set: it's done by
         scan_ctor_arguments. */
      /* Scan the constructor arguments. */
      scan_ctor_arguments(ctor_sym, &arg_expr_list, &ctor_routine,
                          &lparen_pos, base_new_type);
      /* In the array case (an error), throw away the argument list. */
      if (array_new) arg_expr_list = NULL;
    } else {
      /* There is no new-initializer, so a default constructor should exist. */
      ctor_routine = select_default_constructor(base_new_type, &type_position,
						base_new_type,
                                         curr_expr_is_potentially_evaluated());
      arg_expr_list = NULL;
      if (ctor_routine != NULL) {
        /* Provide default arguments if any. */
        arg_expr_list = copy_default_arg_expr_list(
                skip_typerefs(ctor_routine->type)->variant.routine.extra_info->
                                                              param_type_list);
      }  /* if */
    }  /* if */
    needs_initialization = (ctor_routine != NULL);
  } else {
    /* Not a class with a constructor.  The new-initializer is optional. */
    if (curr_token == tok_lparen) {
      /* The new-initializer is present. */
      (void)get_token();
      if (array_new) {
        /* No initializer may be specified for an array type. */
        error(ec_initializer_not_allowed_on_array_new);
        err = TRUE;
      }  /* if */
      if (curr_token != tok_rparen) {
        /* The new-initializer is not empty.  Scan it. */
        init_val_node = scan_parenthesized_initializer_expression(
                                                      err ? error_type() :
                                                            new_type,
                                                      ec_bad_initializer_type);
        needs_initialization = TRUE;
      } else {
        /* The initializer is empty, i.e., "()". */
        (void)get_token();
      }  /* if */
    } else {
      /* No new-initializer is present.  Check for error cases like const
         entities not being initialized. */
      if (!err) check_for_missing_initializer((a_symbol_ptr)NULL, new_type);
    }  /* if */
  }  /* if */
  /* Now build the IL for the operation. */
  if (err || function_symbol == NULL) {
    /* Some error. */
    make_error_operand(result);
  } else {
    an_expr_node_ptr            new_node;
    a_new_delete_supplement_ptr ndsp;
    a_dynamic_init_ptr          dip;
    a_routine_ptr               new_routine, delete_routine;
    a_boolean                   access_error_reported;

    /* Use an enk_new_delete node to represent the "new". */
    new_node = alloc_expr_node((an_expr_node_kind)enk_new_delete);
    new_node->type = ptr_new_type;
    ndsp = new_node->variant.new_delete;
    ndsp->is_new = TRUE;
    ndsp->type = new_type;
    if (needs_initialization) {
      /* The allocated space must be initialized.  A dynamic init entry is
         used. */
      if (ctor_routine != NULL) {
        /* Constructor call. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
        dip->variant.constructor.ptr = ctor_routine;
        dip->variant.constructor.args = arg_expr_list;
        if (array_new) {
          /* The entity is an array whose elements have a class type that
             has a default constructor.  Use a dik_nonconstant_aggregate
             initialization. */
          dip = add_array_nonconstant_aggregate_init(dip, base_new_type,
                                                    effective_num_of_elements);
          /* If exceptions are enabled, put in a destructor.  It's needed
             to destroy elements if a throw is done part-way through the
             initialization of the array. */
          if (exceptions_enabled) {
            dip->destructor = select_destructor(
                                        base_new_type, base_new_type,
                                        &type_position,
                                        /*honor_virtual=*/FALSE,
                                        curr_expr_is_potentially_evaluated(),
                                        /*suppress_access_check=*/FALSE);
          }  /* if */
        }  /* if */
      } else {
        /* Expression as initial value. */
        dip = alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
        dip->variant.expression = init_val_node;
      }  /* if */
      ndsp->dynamic_init = dip;
    }  /* if */
    /* Work out the "new" routine and its arguments. */
    new_routine = function_symbol->variant.routine.ptr;
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    if (array_new) {
      /* If a allocating an array and a runtime routine will be used, the
         "new" routine can be implicit if it is the default global new. */
      if (new_or_delete_type_requires_array_handling(base_new_type)) {
        if (function_symbol == 
                       extract_default_operator_new_sym(operator_new_symbol)) {
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
      if (ctor_routine != NULL) {
        a_type_ptr unqual_base_new_type = skip_typerefs(base_new_type);
        set_class_assoc_operator_new_routine(unqual_base_new_type);
        if (unqual_base_new_type->variant.class_struct_union.extra_info->
                                   assoc_operator_new_routine == new_routine) {
          new_routine = NULL;
        }  /* if */
      }  /* if */
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
    }  /* if */
#endif /* NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE */
    /* Mark the "new" routine as referenced, check access to it. */
    overloaded_function_catch_up(function_symbol,
                                 operator_new_symbol,
                                 /*is_qualified_name=*/FALSE,
                                 &placement_position,
                                 /*elided_reference=*/(new_routine==NULL),
                                 (an_operand *)NULL,
                                 &access_error_reported);
    /* Adjust the argument types, issue any warnings, and free
       arg_operand_list and arg_match_list. */
    adjust_overloaded_function_call_arguments(function_symbol,
                                              /*have_selector=*/FALSE,
                                              (an_operand *)NULL,
                                              arg_operand_list,
                                              arg_match_list,
                                              &arg_expr_list);
    /* Avoid freeing the lists twice. */
    arg_operand_list = NULL;
    arg_match_list = NULL;
    /* If exceptions are enabled, record the delete routine to be used to
       undo the allocation if an exception is thrown.  Do not do this for
       a "placement" new; the storage in that case is not freed automatically
       when an exception is thrown. */
    delete_routine = NULL;
    if (exceptions_enabled && new_routine != NULL && !placement_new) {
      delete_routine = select_delete_routine(base_new_type,
                                             use_global_new,
                                             array_new,
                                             &placement_position);
      /* Mark the routine referenced. */
      if_evaluating_mark_routine_referenced(delete_routine);
    }  /* if */
    /* Put the routine and argument list into the supplement.  Note that
       the argument list is present even when the routine is NULL -- that's
       necessary so that the array size is available when the number of
       elements is nonconstant. */
    ndsp->routine = new_routine;
    ndsp->delete_routine = delete_routine;
    ndsp->arg = arg_expr_list;
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
  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_new_operator */


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
  a_boolean          err = FALSE;
  a_routine_ptr      delete_routine, dtor_routine;
  an_operand         operand;
  a_constant         constant;
  an_expr_node_ptr   expr;
  a_dynamic_init_ptr dip;
  a_routine_type_supplement_ptr
                     delete_routine_rtsp;
  a_param_type_ptr   param1;
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
      diagnostic(anachronism_error_severity, ec_delete_count_anachronism);
      scan_new_array_dimension_expression(&is_constant, &expr, &constant);
      /* The expression is ignored. */
    }  /* if */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    remove_matching_stop_token(tok_rbracket);
  }  /* if */
  /* Scan the pointer expression. */
  scan_expr(&operand, PREC_PREFIX, EOPT_NO_OPTIONS);
  do_operand_transformations(&operand, TOPT_NO_OPTIONS);
  /* The operand of a delete must be a pointer. */
  if (err || !check_pointer_operand(&operand, ec_expr_not_pointer)) {
    make_error_operand(result);
  } else {
    ptr_delete_type = operand.type;
    delete_type = type_pointed_to(ptr_delete_type);
    if (is_const_qualified_type(delete_type)) {
      /* The type pointed to may not be const-qualified. */
      error_in_operand(ec_delete_of_const_pointer, &operand);
      make_error_operand(result);
    } else if (is_function_type(delete_type)) {
      /* The type pointed to may not be a function type.  The ARM doesn't
         say that explicitly, but it does say it must be a pointer returned
         by "new". */
      error_in_operand(ec_delete_of_function_pointer, &operand);
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
      ndsp->type = delete_type;
      ndsp->arg = ptr_node;
      delete_type = skip_typerefs(delete_type);
      base_delete_type = delete_type;
      /* Get the underlying type for any array type. */
      while (is_array_type(base_delete_type)) {
        base_delete_type = array_element_type(base_delete_type);
      }  /* if */
      /* See if the object needs destruction. */
      dtor_routine = NULL;
      if (is_class_struct_union_type(base_delete_type)) {
        /* Instantiate the class if it is a template class. */
        instantiate_template_class(base_delete_type);
        if (is_incomplete_type(base_delete_type)) {
          /* Deleting a pointer to an incomplete class.  Give a warning,
             because we may not know how to do the right thing (like call
             a destructor). */
          pos_warning(ec_delete_of_incomplete_class, &operand.position);
        }  /* if */
        dtor_routine = select_destructor(base_delete_type, base_delete_type,
                                         &operand.position,
                                         /*honor_virtual=*/FALSE,
                                         curr_expr_is_potentially_evaluated(),
                                         /*suppress_access_check=*/FALSE);
        if (dtor_routine != NULL) {
          /* Class with destructor.  Destruction is required. */
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = dtor_routine;
          if (array_delete) {
            /* For a delete of an array of classes, generate a dynamic init
               that replicates the destructor call for the whole array. */
            dip = add_array_nonconstant_aggregate_init(dip, base_delete_type,
                                                       (a_targ_size_t)0);
          }  /* if */
          ndsp->dynamic_init = dip;
        }  /* if */
      }  /* if */
      /* Select the proper "delete" routine.  If the type is a class type and
         the class has a "delete" operator, use it.  However, if "::" preceded
         the keyword "delete", always use the global ::delete.  Also use the
         global ::delete for arrays of class objects. */
      delete_routine = select_delete_routine(delete_type,
                                             use_global_delete,
                                             array_delete,
                                             &delete_position);
#if NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
      if (array_delete) {
        /* If a deleting an array and a runtime routine will be used, the
           delete routine can be implicit if it is the default global
           delete. */
        if (new_or_delete_type_requires_array_handling(base_delete_type)) {
          if (delete_routine ==
                           opname_function_symbol((an_opname_kind)onk_delete)->
                                                         variant.routine.ptr) {
            delete_routine = NULL;
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
          set_class_assoc_operator_delete_routine(unqual_base_delete_type);
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
        /* If the delete routine is one with two arguments, pass the size
           of the entity as the second argument. */
        delete_routine_rtsp = f_skip_typerefs(delete_routine->type)->
                                                    variant.routine.extra_info;
        param1 = delete_routine_rtsp->param_type_list;
        check_assertion(param1 != NULL);
        if (param1->next != NULL) {
          /* Two-argument form.  Add a second argument of type size_t that
             indicates the (static) size of the object. */
          ptr_node->next =
              node_for_integer_constant((long)(delete_type->size),
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
        }  /* if */
      }  /* if */
      ndsp->routine = delete_routine;
      /* Make an operand for the result. */
      make_expression_operand(delete_node, void_type(), result);
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_delete_operator */


static void lvalue_cast(a_type_ptr type_cast_to,
                        an_operand *result)
/*
Cast an operand for an lvalue (result) to a new type.  This "lvalue cast" is
only done in pcc mode.
*/
{
  an_expr_node_ptr temp_node;

  /* Build an expression node for the lvalue cast.  Note that this is done
     even if the lvalue address is represented by a constant. */
  temp_node = make_node_from_operand(result);
  temp_node = make_operator_node((an_expr_operator_kind)eok_lvalue_cast,
                                 make_pointer_type(type_cast_to), temp_node);
  /* Make an expression operand for the node.  Change the old one rather
     than creating a new one so as not to disturb the other fields in the
     operand. */
  set_operand_kind(result, (an_operand_kind)ok_expression);
  result->variant.expression = temp_node;
  result->type = type_cast_to;
}  /* lvalue_cast */


static a_boolean cast_type_pre_check(
                                   a_type_ptr               *p_type_cast_to,
                                   a_boolean                *cast_to_reference,
                                   a_boolean                *int_to_ptr_case,
                                   a_boolean                *cast_to_func_ptr,
                                   a_boolean                *templ_cast_to_ptr,
                                   a_local_expr_options_set local_options)
/*
Do a first check on the destination type of a cast to see if it is legal.
That is, do a check that the type is legal as the destination type of a
cast without regard to the source type.  Return TRUE if there is an error.
*p_type_cast_to is the destination type of the cast.  On return,
*cast_to_reference is TRUE if the cast is to a reference type
(*p_type_cast_to will be adjusted to the corresponding pointer type in
that case), *int_to_ptr_case is TRUE if the destination type is a pointer
type in a context that requires that the source type be an integral type,
*cast_to_func_ptr is TRUE if the cast is to a pointer-to-function type
in C++, and *templ_cast_to_ptr is true if the cast is to a pointer or
pointer-to-member type in a template argument.  This routine is called
for both C-style casts and C++ functional-notation type conversions.
*/
{
  a_boolean  err = FALSE;
  a_type_ptr type_cast_to = *p_type_cast_to;

  *int_to_ptr_case = FALSE;
  *cast_to_reference = FALSE;
  *cast_to_func_ptr = FALSE;
  *templ_cast_to_ptr = FALSE;
  /* Instantiate the type if it is a template class. */
  check_for_uninstantiated_template_class(type_cast_to);
  /* Check the type to see if it's permissible. */
  if (is_error_type(type_cast_to)) {
    err = TRUE;
  } else if (is_template_param_type(type_cast_to)) {
    /* We are in a prototype instantiation of a template.  The type is
       a template parameter type, i.e., we don't know what it is.  Assume
       it's okay and go on. */
  } else if (is_incomplete_type(type_cast_to) && !is_void_type(type_cast_to)) {
    /* This check catches incomplete enum types.  Except for being
       incomplete, they look like integral types. */
    error(ec_incomplete_type_not_allowed);
    err = TRUE;
  } else if (curr_expr_kind_is(ek_integral_constant)) {
    /* Only casts to integral types are permitted in integral constant
       expressions.  When the cast is the immediate operand of another
       cast, allow integer --> pointer as an extension. */
    if (!is_integral_type(type_cast_to)) {
      if ((local_options & EOPT_OPERAND_OF_CAST) && 
          is_pointer_type(type_cast_to)) {
        *int_to_ptr_case = TRUE;
        if (strict_ansi_mode) {
          diagnostic(strict_ansi_error_severity, ec_cast_not_integral);
        }  /* if */
      } else {
        error(ec_cast_not_integral);
        err = TRUE;
      }  /* if */
    }  /* if */
  } else if (curr_expr_kind_is(ek_init_constant)) {
    /* Only casts to arithmetic or pointer types are allowed in initializer
       constants. */
    if (!is_scalar_type(type_cast_to)) {
      error(ec_cast_not_scalar);
      err = TRUE;
    }  /* if */
  } else if (curr_expr_kind_is(ek_template_arg)) {
    /* Only casts to arithmetic types are allowed in nontype template
       arguments. */
    if (!is_arithmetic_type(type_cast_to)) {
      /* However, a cast of a zero to a pointer or pointer-to-member type is
         allowed.  The other half of the check is in do_cast. */
      if (is_pointer_type(type_cast_to) ||
          is_ptr_to_member_type(type_cast_to)) {
        *templ_cast_to_ptr = TRUE;
      } else {
        error(ec_non_arith_operation_in_templ_arg);
        err = TRUE;
      }  /* if */
    }  /* if */
  } else {
    /* Not a special expression kind. */
    if (is_scalar_type(type_cast_to) || is_void_type(type_cast_to)) {
      /* Casting to a scalar type or void is always allowed. */
    } else if (C_dialect == C_dialect_cplusplus &&
               is_class_struct_union_type(type_cast_to)) {
      /* In C++, a cast to a class is allowed. */
      /* But not a cast to an abstract class. */
      if (skip_typerefs(type_cast_to)->variant.class_struct_union.abstract) {
        error(ec_cast_to_abstract_class);
        err = TRUE;
      }  /* if */
    } else if (is_reference_type(type_cast_to)) {
      /* Casting to a reference type in C++ is checked by casting the lvalue
         pointer to the corresponding pointer type.  ARM 5.4:  "An object
         may be explicitly converted to a reference type X& if a pointer to
         that object may be explicitly converted to an X*". */
      *cast_to_reference = TRUE;
      *p_type_cast_to = type_cast_to =
                              make_pointer_type(type_pointed_to(type_cast_to));
    } else if (is_ptr_to_member_type(type_cast_to)) {
      /* In C++, a cast to a pointer-to-member type is allowed. */
    } else if (cfront_compatibility_mode && is_array_type(type_cast_to)) {
      /* In C++, treat a cast to an array type as a cast to a pointer to
         the array element type.  This is an extension to match cfront 2.1
         and is only accepted in cfront compatibility mode.   A warning is
         issued even in cfront mode because this is a questionable
         practice. */
      *p_type_cast_to = type_cast_to =
                      type_after_array_to_pointer_transformation(type_cast_to);
      type_warning(ec_nonstd_array_cast, type_cast_to);
    } else {
      /* Invalid destination type for cast. */
      type_error(ec_cast_to_bad_type, type_cast_to);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    /* Casting to a qualified type, though valid, is pointless. */
    if (is_qualified_type(type_cast_to)) {
      warning(ec_cast_to_qualified_type);
      *p_type_cast_to = type_cast_to = make_unqualified_type(type_cast_to);
    }  /* if */
    /* Determine whether or not the cast is to a pointer-to-function type
       (this is needed in C++ to allow the anachronism of casting a bound
       function pointer to a normal function pointer). */
    if (C_dialect == C_dialect_cplusplus && allow_anachronisms &&
        is_pointer_type(type_cast_to) &&
        is_function_type(type_pointed_to(type_cast_to))) {
      *cast_to_func_ptr = TRUE;
    }  /* if */
  }  /* if */
  return err;
}  /* cast_type_pre_check */


static void cast_operand_to_void(an_operand *operand,
                                 a_type_ptr type_cast_to)
/*
Cast the indicated operand to void.  This is used for explicit casts
to void.  type_cast_to gives the (possibly qualified) void type.
*/
{
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
  make_expression_operand(make_operator_node((an_expr_operator_kind)eok_cast,
                                             type_cast_to,
                                             make_node_from_operand(operand)),
                          type_cast_to,
                          operand);
}  /* cast_operand_to_void */


static void do_cast(a_type_ptr         type_cast_to,
                    an_operand         *operand,
                    an_operand         *bound_function_selector,
                    a_boolean          err,
                    a_boolean          cast_to_reference,
                    a_boolean          int_to_ptr_case,
                    a_boolean          cast_to_func_ptr,
                    a_boolean          templ_cast_to_ptr,
                    a_source_position  *start_position)
/*
Do a cast operation.  The operand *operand is to be cast to the type
type_cast_to.  If is is a bound function (only in C++),
*bound_function_selector gives the associated object (cast_to_func_ptr
will be TRUE in that case, indicating a cast of a bound function pointer
to a normal function pointer, an anachronism).  err is TRUE if it has
already been determined that the cast is invalid (this routine does
additional checking).  cast_to_reference is TRUE if the cast is to a
reference type.  int_to_ptr_case is TRUE if cast_type_pre_check
determined that the source type must be integral (that must be checked
here).  templ_cast_to_ptr is TRUE if the cast is to a pointer or pointer-
to-member type in a template argument.  start_position is the source
position of the start of the cast.  This routine is called for both
C-style casts and C++ functional-notation type conversions.
*/
{
  a_type_ptr        source_type;
  an_error_code     warning_suggested;
  a_boolean         failed = FALSE;
  a_user_conv_descr user_conversion;
  an_expr_node_ptr  func_ptr_node, object_node;

  if (err) {
    /* There was a previous error (e.g., the type to cast to is invalid
       regardless of the type of the source).  Do no further checking. */
  } else {
    if (cast_to_reference) {
      /* In C++, "An object may be explicitly converted to a reference type
         X& if a pointer to that object may be explicitly converted
         to an X*" (ARM 5.4).  Rewrite the cast in that form.  Note that
         type_cast_to is already set to the proper pointer type. */
      /* It's not entirely clear what the ARM means about "a pointer
         to an object".  One interpretation would be that the expression
         must be an lvalue (that term in C++ includes function designators).
         We broaden that slightly by allowing class rvalues to be used
         as well. */
      if (is_an_lvalue(operand)) {
        take_address_of_lvalue(operand);
      } else if (is_a_function_designator(operand)) {
        conv_function_designator_to_ptr_to_function(operand);
      } else if (is_class_struct_union_type(operand->type)) {
        conv_class_operand_to_object_pointer(operand);
      } else {
        if (!is_error_operand(operand)) {
          error_in_operand(ec_expr_not_an_lvalue, operand);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Check for user-defined conversions, but not in constant expressions
       or in C, and not when casting to void or a template parameter (unknown)
       type. */
    if (C_dialect == C_dialect_cplusplus &&
        !curr_expr_kind_is_const() &&
        !is_void_type(type_cast_to) &&
        !is_template_param_type(type_cast_to) &&
        user_defined_conversion_possible(operand, type_cast_to,
                                         /*is_initialization=*/TRUE,
                                         &user_conversion,
                                         &failed)) {
      /* A user-defined conversion can be done. */
      if (!cast_to_reference) user_conversion.result_is_an_lvalue = FALSE;
      user_convert_operand(operand, type_cast_to, &user_conversion);
    } else if (failed) {
      /* A user-defined conversion was our only hope, and it failed.
         The error has already been issued. */
      err = TRUE;
    } else {
      /* No user-defined conversion applies. */
      if (!cast_to_reference) {
        /* Normal case (not a cast to reference).  Do array --> pointer and
           function --> pointer conversions.  They must be done now because
           they affect the type of the operand.  Don't do lvalue --> rvalue
           yet because of the pcc lvalue cast case. */
        do_operand_transformations(operand,
                                   TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      }  /* if */
      /* Get the source type after the transformations. */
      source_type = operand->type;
      /* cast_type_pre_check has already verified that the destination type
         is legal in broad terms.  Check for casts that are allowed in
         general but disallowed in specific modes. */
      if (is_error_type(source_type)) {
        /* There was a previous error.  Do no further checking. */
        err = TRUE;
      } else if (is_template_param_type(type_cast_to)) {
        /* Casting to a template parameter (unknown) type.  Assume okay,
           but produce an error operand. */
        err = TRUE;
      } else if (curr_expr_kind_is(ek_integral_constant)) {
        /* Integral constant expression: only arithmetic --> integral
           is standard.  cast_type_pre_check has already checked that the
           target type is integral (except that when this cast is the
           immediate operand of another cast, integer --> pointer is
           allowed as an extension). */
        if (int_to_ptr_case) {
          /* This is the extension case let by.  The source must be
             integral. */
          if (!is_integral_type(source_type)) {
            error(ec_expr_not_integral);
            err = TRUE;
          }  /* if */
        } else if (!is_arithmetic_type(source_type)) {
          /* As an extension, allow pointer --> int for pointer constants
             that come from casting an integer constant to a pointer type,
             as in (int)(char *)1. */
          if (is_pointer_type(source_type) && is_constant_operand(operand) &&
              operand->variant.constant.kind ==
                                            (a_constant_repr_kind)ck_integer) {
            if (strict_ansi_mode) {
              diagnostic(strict_ansi_error_severity, ec_expr_not_arithmetic);
            }  /* if */
          } else {
            error(ec_expr_not_arithmetic);
            err = TRUE;
          }  /* if */
        }  /* if */
      } else if (curr_expr_kind_is(ek_init_constant)) {
        /* Initializer constant expression: arithmetic --> arithmetic
           and scalar --> pointer are allowed, pointer --> integral as
           an extension.  The code above has already checked that the
           target type is scalar (arithmetic or pointer).  Check that a
           cast to arithmetic converts from an arithmetic type (see 3.4).
           The usual checks on the scalar --> pointer case are done below
           (to catch, e.g., float --> pointer). */
        if (is_arithmetic_type(type_cast_to)) {
          /* Casting to arithmetic, source must be arithmetic. */
          if (!is_arithmetic_type(source_type)) {
            if (is_pointer_type(source_type) &&
                is_integral_type(type_cast_to)) {
              /* Pointer --> integral.  Allowed as an extension.  The check
                 that the integral type is large enough is done below in the
                 call of expl_conversion_possible. */
              if (strict_ansi_mode) {
                diagnostic(strict_ansi_error_severity, ec_expr_not_arithmetic);
              }  /* if */
            } else {
              /* Non-arithmetic --> arithmetic. */
              error(ec_expr_not_arithmetic);
              err = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (curr_expr_kind_is(ek_template_arg)) {
        /* Only casts between arithmetic types are allowed in nontype template
           arguments, except for a cast of a zero to a pointer or
           pointer-to-member type. */
        if (templ_cast_to_ptr) {
          if (is_constant_operand(operand) &&
              is_null_pointer_constant(&operand->variant.constant)) {
            /* Cast of a null pointer constant to a pointer or
               pointer-to-member type.  Okay. */
          } else {
            /* Cast to a pointer or pointer-to-member type, but the source is
               not a null pointer constant. */
            error(ec_non_arith_operation_in_templ_arg);
            err = TRUE;
          }  /* if */
        } if (!is_arithmetic_type(source_type)) {
          error(ec_non_arith_operation_in_templ_arg);
          err = TRUE;
        }  /* if */
      }  /* if */
      /* Check that the combination of the source and target types is
         allowed, then do the cast.  See 3.4 in the ANSI C standard. */
      if (!err) {
        /* The bound function test is done first to make sure bound functions
           cannot wander into the rest of the cases. */
        if (operand->bound_function) {
          /* In C++, a bound function pointer may be cast to a normal function
             pointer, as in
               struct A {int f();};
               A *p = new A;
               int (*pf)() = (int (*)())p->f;
             This is an anachronism.  See ARM 18.3.4. */
          if (cast_to_func_ptr &&
              is_pointer_type(operand->type) &&
              is_function_type(type_pointed_to(operand->type))) {
            pos_diagnostic(anachronism_error_severity,
                           ec_bound_function_cast_anachronism, start_position);
            conv_lvalue_to_rvalue(operand);
            if (operand->virtual_function) {
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
              make_expression_operand(func_ptr_node, func_ptr_node->type,
                                      operand);
            } else {
              /* The function is not a virtual function, so discard the
                 selector object pointer and just use the routine address. */
              discard_operand(bound_function_selector);
              operand->bound_function = FALSE;
            }  /* if */
            /* Cast the node to the result type of the cast. */
            cast_operand(type_cast_to, operand, /*is_implicit_cast=*/FALSE);
          } else {
            /* Any other use of a bound function.  Error. */
            error_in_operand(ec_bound_function_must_be_called, operand);
            operand->bound_function = FALSE;
          }  /* if */
        } else if (is_void_type(type_cast_to)) {
          /* Anything --> void, allowed. */
          conv_lvalue_to_rvalue(operand);
          /* Do the cast to void as an expression. */
          cast_operand_to_void(operand, type_cast_to);
        } else if (!is_scalar_type(source_type) &&
                   !is_ptr_to_member_type(type_cast_to)) {
          /* Not casting to void or a class, and not casting to a
             pointer-to-member type, so the source type must be scalar. */
          error(ec_expr_not_scalar);
          err = TRUE;
        } else if (expl_conversion_possible(source_type,
                                            is_constant_operand(operand),
                                            &operand->variant.constant,
                                            type_cast_to,
                                            ec_bad_cast, &warning_suggested)) {
          /* Valid explicit conversion.  Issue warning on oddball cases. */
          if (warning_suggested != ec_no_error) {
            pos_warning(warning_suggested, start_position);
          }  /* if */
          /* In pcc mode, some lvalues cast to same-sized types remain lvalues
             (e.g., int to unsigned). */
          if (C_dialect == C_dialect_pcc && is_an_lvalue(operand) &&
              still_an_lvalue(source_type, type_cast_to)) {
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
            /* Not an lvalue cast. */
            /* Convert lvalue --> rvalue unless casting to a reference type
               (in that case, the operand has already been turned into a
               pointer; the conversion here wouldn't hurt, but it's not
               needed). */
            if (!cast_to_reference) {
              /* Normal cast.  All standard C cases. */
              conv_lvalue_to_rvalue(operand);
            }  /* if */
            /* Do the actual cast. */
            cast_operand(type_cast_to, operand, /*is_implicit_cast=*/FALSE);
            if (cast_to_reference) {
              /* The result of a cast to reference is an lvalue. */
              conv_object_pointer_to_lvalue(operand);
            }  /* if */
          }  /* if */
        } else {
          /* Not a valid conversion. */
          /* Note:  If this is changed to display the types involved,
             remember to check cast_to_reference. */
          err = TRUE;
          pos_error(ec_bad_cast, start_position);
        }  /* if */
      }  /* if */
    }  /* if */
    /* The result of a cast to a reference type is considered to have come
       from a reference. */
    if (cast_to_reference) operand->came_from_reference = TRUE;
  }  /* if */
  if (err) conv_to_error_operand(operand);
  operand->position = *start_position;
}  /* do_cast */


static void scan_cast_or_expr(
                             an_operand               *result,
                             an_operand               *bound_function_selector,
                             a_local_expr_options_set local_options)
/*
Scan something after an opening left paren.  This may be a cast operation or
just an expression in parentheses.  Return the scanned expression in
*result (and, if it is a C++ bound function, return the object in
*bound_function_selector).  See section 3.3.4 of the standard.

Syntax:
 	( type-name ) expression
or
	( expression )

*/
{
  a_source_position start_position;
  a_type_ptr        type_cast_to;
  a_boolean         err = FALSE;
  a_boolean         int_to_ptr_case, cast_to_reference, cast_to_func_ptr;
  a_boolean         templ_cast_to_ptr;
  a_local_expr_options_set
                    cast_options;
  an_operand        local_bound_function_selector;

  db_enter(4, "scan_cast_or_expr");

  /* Save the current source position.  Note that in the parenthesis-trapped
     case we are saving the position of the token after the left parenthesis,
     but that's okay; the caller straightens it out. */
  copy_source_position(pos_curr_token, start_position);

  add_matching_stop_token(tok_rparen);

  /* Get past the opening lparen.  If a left parenthesis was trapped,
     we're already past it, so do not advance. */
  if (!(local_options & EOPT_TRAPPED_LEFT_PAREN)) (void)get_token();

  /* Determine if this is a cast operation.  Cast operations are not
     allowed in preprocessing expressions (although the check is superfluous
     since identifiers are never recognized as type names, and therefore
     is_decl_not_expr would never return TRUE). */
  if (!curr_expr_kind_is(ek_pp) &&
      is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                       /*real_declarator_allowed=*/FALSE,
                       /*single_type_required=*/TRUE)) {
    /* This is a cast operation. */
    /* Get the type to cast to. */
    type_name(&type_cast_to);
    /* Check the type to see if it is valid.  This is done early to get a
       better error position. */
    err = cast_type_pre_check(&type_cast_to, &cast_to_reference,
                              &int_to_ptr_case, &cast_to_func_ptr,
                              &templ_cast_to_ptr, local_options);

    /* The next token should be the closing rparen. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_matching_stop_token(tok_rparen);

    /* Scan the expression to be cast. */
    cast_options = EOPT_OPERAND_OF_CAST;
    if (cast_to_func_ptr) {
      /* In C++, allow a bound function as the operand of a cast to a
         normal function pointer. */
      cast_options |= EOPT_ALLOW_BOUND_FUNCTION;
    }  /* if */
    scan_expr_full(result, &local_bound_function_selector, PREC_CAST,
                   cast_options);
    /* Check compatibility of the types and do the cast. */
    do_cast(type_cast_to, result, &local_bound_function_selector, err,
            cast_to_reference, int_to_ptr_case, cast_to_func_ptr,
            templ_cast_to_ptr, &start_position);
  } else {
    /* This is an expression in parentheses.  The parentheses do not
       affect the fact that the enclosed expression is an immediate operand
       of the surrounding context, so most of the option flags are
       passed down. */
    scan_expr_full(result, bound_function_selector, PREC_LOWEST,
                   (local_options &
                    (EOPT_OPERAND_OF_CAST | EOPT_OPERAND_OF_ADDRESS_OF)) |
                   EOPT_ALLOW_BOUND_FUNCTION);
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_matching_stop_token(tok_rparen);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_cast_or_expr */


static a_boolean conversion_has_one_argument(void)
/*
The current token position is a type identifier for a class that is
the start of a functional-notation cast.  Look ahead in the input and
determine the number of arguments of the conversion.  If the conversion
has exactly one argument, return TRUE; otherwise, return FALSE.
*/
{
  a_boolean          one_arg = FALSE;
  a_token_cache      cache;
  a_stop_token_array save_stop_token_array;

  clear_token_cache(&cache, /*reusable=*/FALSE);
  /* Put the class name token in the cache. */
  cache_curr_token(&cache);
  /* Advance to the "(". */
  (void)get_token();
  if (curr_token == tok_lparen) {
    cache_curr_token(&cache);
    /* Get the first token inside the parentheses. */
    (void)get_token();
    if (curr_token == tok_rparen) {
      /* The argument list is "()", i.e., zero arguments. */
    } else {
      /* One or more arguments. */
      /* Scan forward looking for a zero-level comma or right parenthesis. */
      /* Save the current stop token state, and reinitialize it. */
      copy_stop_tokens(stop_token_array, save_stop_token_array);
      clear_stop_tokens();
      add_stop_token(tok_comma);
      add_stop_token(tok_rparen);
      cache_token_stream(&cache);
      /* If we stopped on a right parenthesis, the argument list has exactly
         one argument. */
      if (curr_token == tok_rparen) one_arg = TRUE;
      /* Restore the original stop token state. */
      copy_stop_tokens(save_stop_token_array, stop_token_array);
    }  /* if */
  }  /* if */
  /* Restore the tokens. */
  rescan_cached_tokens(&cache);

  return one_arg;
}  /* conversion_has_one_argument */


static void scan_functional_notation_type_conversion(
                                      a_type_ptr               type_cast_to,
                                      an_operand               *result,
                                      a_local_expr_options_set local_options)
/*
Scan a C++ functional-notation type conversion, e.g., "int(1.5)" or "A(1,2)".
The type keyword or identifier is the current token, and the associated
type is passed in as type_cast_to.  The result is returned in *result.
*/
{
  a_source_position             start_position, lparen_pos;
  a_boolean                     err = FALSE;
  a_boolean                     int_to_ptr_case;
  a_boolean                     cast_to_reference;
  a_boolean                     cast_to_func_ptr, templ_cast_to_ptr;
  a_symbol_ptr                  ctor_sym;
  an_expr_node_ptr              arg_expr_list;
  a_routine_ptr                 ctor_routine;
  a_boolean                     ctor_case = FALSE;
  a_class_symbol_supplement_ptr cssp;
  a_local_expr_options_set      cast_options;
  an_operand                    local_bound_function_selector;

  db_enter(4, "scan_functional_notation_type_conversion");

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);
  /* Check the type to see if it is valid.  This is done early to get a
     better error position.  Note that this even does a worthwhile check
     for the class case (abstract class). */
  err = cast_type_pre_check(&type_cast_to, &cast_to_reference,
                            &int_to_ptr_case, &cast_to_func_ptr,
                            &templ_cast_to_ptr, local_options);
  /* See if we have a case that is clearly a constructor call. */
  if (is_class_struct_union_type(type_cast_to)) {
    cssp = symbol_supplement_for_class(type_cast_to);
    ctor_sym = cssp->constructor;
    if (ctor_sym != NULL) {
      /* The class has a constructor. */
      ctor_case = TRUE;
      if (cssp->target_of_conversion_function &&
          conversion_has_one_argument()) {
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
  /* Advance past the type keyword or identifier. */
  (void)get_token();
  /* Check for a left parenthesis. */
  copy_source_position(pos_curr_token, lparen_pos);
  (void)required_token(tok_lparen, ec_exp_lparen);
  if (ctor_case) {
    /* Converting to a class type.  The contents of the parentheses are
       arguments for a constructor call. */
    scan_ctor_arguments(ctor_sym, &arg_expr_list, &ctor_routine, &lparen_pos,
			type_cast_to);
    error_position = start_position;
    if (err || ctor_routine == NULL) {
      /* Error of some sort. */
      make_error_operand(result);
    } else {
      /* Make a dynamic init entry that calls constructor to initialize
         a temporary.  Make an operand for the value of the temporary. */
      make_constructor_dynamic_init(ctor_routine, arg_expr_list,
                                    /*result_is_addr=*/FALSE,
                                    &start_position, result);
    }  /* if */
  } else {
    /* Not a constructor case; obeys the same rules as a C-style cast. */
    add_matching_stop_token(tok_rparen);
    if (curr_token == tok_rparen) {
      /* Empty parentheses. */
      if (err) {
        /* Some previous error. */
        make_error_operand(result);
      } else if (is_class_struct_union_type(type_cast_to)) {
        /* A class with no constructor, followed by (), e.g., "A()" --
           this is an error. */
        pos_ty_error(ec_no_constructor, &lparen_pos, type_cast_to);
        make_error_operand(result);
      } else if (curr_expr_kind_is_const()) {
        /* This cast is inherently non-constant.  If it has not been
           rejected for some other reason in a constant expression,
           reject it now. */
        pos_error(ec_expr_not_constant, &lparen_pos);
        make_error_operand(result);
      } else if (cast_to_reference) {
        /* Disallow a cast to a reference type; this may or may not turn
           out to be allowed by the standard for C++. */
        pos_error(ec_bad_cast, &lparen_pos);
        make_error_operand(result);
      } else {
        /* A non-class type followed by (); generate an "undefined" value
           of the type.  We actually use 0, because it can be cast to all
           non-class types (arithmetic, pointer, pointer to member, void). */
        make_integer_constant_operand(result, 0L);
        if (is_void_type(type_cast_to)) {
          /* Use an expression for the void case, because a constant cannot
             be cast to void. */
          cast_operand_to_void(result, type_cast_to);
        } else {
          cast_operand(type_cast_to, result, /*is_implicit_cast=*/FALSE);
        }  /* if */
      }  /* if */
    } else {
      /* Non-empty parentheses. */
      /* Since the expression in parentheses is syntactically an
         expression list, a top-level comma is not allowed. */
      cast_options = EOPT_OPERAND_OF_CAST | EOPT_DISALLOW_COMMA_OPERATOR;
      if (cast_to_func_ptr) {
        /* In C++, allow a bound function as the operand of a cast to a
           normal function pointer. */
        cast_options |= EOPT_ALLOW_BOUND_FUNCTION;
      }  /* if */
      scan_expr_full(result, &local_bound_function_selector, PREC_LOWEST,
                     cast_options);
      /* Check compatibility of the types and do the cast. */
      do_cast(type_cast_to, result, &local_bound_function_selector, err,
              cast_to_reference, int_to_ptr_case, cast_to_func_ptr,
              templ_cast_to_ptr, &start_position);
    }  /* if */
    /* Check for the closing parenthesis. */
    check_closing_paren_after_expr_list();
    remove_matching_stop_token(tok_rparen);
  }  /* if */
  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);
  db_exit();
}  /* scan_functional_notation_type_conversion */


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
  an_expr_operator_kind op;
  a_type_ptr            result_type;
  a_boolean             processed = FALSE;

  db_enter(4, "scan_mult_operator");

  /* Save the current token kind. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_MULT_DIV, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be of arithmetic type (the remainder operator
       requires integral type). */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    if (save_token == tok_remainder) {
      (void)check_integral_operand(operand_1);
    } else {
      (void)check_arithmetic_operand(operand_1);
    }  /* if */
    /* The second operand must be of arithmetic type (the remainder operator
       requires integral type). */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    if (save_token == tok_remainder) {
      (void)check_integral_operand(&operand_2);
    } else {
      (void)check_arithmetic_operand(&operand_2);
    }  /* if */

    result_type = determine_arithmetic_conversions(operand_1, &operand_2);
    change_binary_operand_types(result_type, operand_1, &operand_2);
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
    op = which_binary_operator(save_token, result_type);
    do_binary_operation(op, operand_1, &operand_2,
                        result_type, result, &operator_position);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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
  a_boolean             operand_1_is_pointer;
  a_boolean             both_operands_are_arithmetic = FALSE;
  a_boolean		pointer_difference           = FALSE;
  a_boolean             err = FALSE, processed = FALSE;
  a_type_ptr            result_type;
  a_type_ptr            operation_type;

  db_enter(4, "scan_add_operator");

  /* Save the current token kind. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_PLUS_MINUS, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = FALSE;
    if (is_arithmetic_type(operand_1->type)) {
      /* Okay. */
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check whether the operand types are valid. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      err = TRUE;
    } else if (operand_1_is_pointer) {
      /* Operand 1 has pointer type. */
      if (is_integral_type(operand_2.type)) {
        /* Pointer +- integral. */
        /* The first operand must be a pointer to an object. */
#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
        /* Pointer to incomplete array is also allowed. */
        (void)check_object_or_incomp_array_pointer_operand(operand_1,
                                                 ec_expr_not_pointer_to_object,
                                                           &operand_2);
#else /* !PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
        (void)check_object_pointer_operand(operand_1,
                                           ec_expr_not_pointer_to_object);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
        /* The result type is the same as the pointer type in operand 1. */
        result_type = operation_type = operand_1->type;
      } else if (save_token == tok_minus && is_pointer_type(operand_2.type)) {
        /* Pointer - pointer. */
        pointer_difference = TRUE;
        /* In ANSI C, both operands must be pointers to qualified or
           unqualified members of compatible object types (ANSI C 3.3.6).
           In C++, the standard pointer conversions are also done
           (ARM 4.6, 5.7). */
        if (check_compatibility_of_pointer_operands(
                           operand_1, &operand_2, &operator_position,
                           /*pointer_normalization_standard_in_C=*/FALSE,
                           /*pointers_to_functions_standard_in_C=*/FALSE,
                           /*pointers_to_incomplete_standard_in_C=*/FALSE,
                           /*mixed_object_and_incomplete_standard_in_C=*/FALSE,
                           &operation_type) &&
            check_object_pointer_operand(operand_1,
                                         ec_expr_not_pointer_to_object) &&
            check_object_pointer_operand(&operand_2,
                                         ec_expr_not_pointer_to_object)) {
          /* Difference between compatible pointers.  Result has type
             ptrdiff_t (see 3.3.6 and <stddef.h>). */
          result_type = integer_type((an_integer_kind)TARG_PTRDIFF_T_INT_KIND);
        } else {
          /* Difference between incompatible pointers.  Error has already been
             issued. */
          err = TRUE;
        }  /* if */
      } else {
        /* Pointer +- non-integral.  Error. */
        error_in_operand(ec_expr_not_integral, &operand_2);
        err = TRUE;
      }  /* if */
    } else if (save_token == tok_plus &&
               is_pointer_type(operand_2.type) &&
               is_integral_type(operand_1->type)) {
      /* Integral + pointer. */
      /* The second operand must be a pointer to an object. */
#if PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED
      /* Pointer to incomplete array is also allowed. */
      (void)check_object_or_incomp_array_pointer_operand(&operand_2,
                                                 ec_expr_not_pointer_to_object,
                                                         operand_1);
#else /* !PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
      (void)check_object_pointer_operand(&operand_2,
                                         ec_expr_not_pointer_to_object);
#endif /* PTR_TO_INCOMP_ARRAY_ARITHMETIC_ALLOWED */
      /* The result type is the same as the pointer type in operand 2. */
      result_type = operation_type = operand_2.type;
      /* Reverse the operands so that the pointer is always first. */
      copy_operand(operand_1, &operand_temp);
      copy_operand(&operand_2, operand_1);
      copy_operand(&operand_temp, &operand_2);
    } else {
      /* Operand 1 is arithmetic. */
      if (is_arithmetic_type(operand_2.type)) {
        /* Arithmetic +- arithmetic. */
        /* Determine the result type based on the 2 operands. */
        result_type = operation_type = 
                       determine_arithmetic_conversions(operand_1, &operand_2);
        both_operands_are_arithmetic = TRUE;
      } else {
        /* Arithmetic +- non-arithmetic.  Error. */
        error_in_operand(ec_expr_not_arithmetic, &operand_2);
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
      if (both_operands_are_arithmetic) {
        change_binary_operand_types(result_type, operand_1, &operand_2);
      }  /* if */
      /* Determine the expression operator for this case. */
      if (pointer_difference) {
        /* Pointer - pointer is a special case, with its own operator. */
        op = (an_expr_operator_kind)eok_pdiff;
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

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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
  a_type_ptr            result_type;
  an_error_code         err_code;
  a_boolean             processed = FALSE;

  db_enter(4, "scan_shift_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_SHIFT, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    (void)check_integral_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    (void)check_integral_operand(&operand_2);

    if (C_dialect == C_dialect_pcc) {
      /* In K&R first edition (see appendix A, section 7.5), "perform the usual
         arithmetic conversions on their operands, each of which must be 
         integral.  Then the right operand is converted to int; the type of
         the result is that of the left operand."  This has the effect
         that a "long" shift count will force the shift to be done as long. */
      result_type = determine_arithmetic_conversions(operand_1, &operand_2);
      change_binary_operand_types(result_type, operand_1, &operand_2);
      cast_operand(integer_type((an_integer_kind)ik_int), &operand_2,
                   /*is_implicit_cast=*/TRUE);    
    } else {
      /* ANSI rules just call for the integral promotions; the type of
         the result is the type of the left operand. */
      promote_operand(operand_1);
      promote_operand(&operand_2);
    }  /* if */
    if (curr_expr_is_evaluated() && is_constant_operand(&operand_2) &&
        !is_constant_operand(operand_1) && !is_error_operand(operand_1)) {
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

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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
      /* Check to see if the nonconstant operand has an unsigned integral
         type. */
      if (is_integral_type(operand_type) &&
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
    if (is_integral_type(constant->type)) {
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

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_RELATIONAL, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = FALSE;
    if (is_arithmetic_type(operand_1->type)) {
      /* Okay. */
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check the operand types for compatibility. */
    operation_type = operand_1->type;  /* Assume. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      /* One or both of the operands has an error. */
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
        /* Both operands should be arithmetic (we have ruled out all the
           pointer cases above).  We already know that operand_1 is
           arithmetic. */
        if (check_arithmetic_operand(&operand_2)) {
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
    result_type = get_logical_result_type(operand_1, &operand_2);
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

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_EQ_NE, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    operand_1_is_pointer = operand_1_is_ptr_to_member = FALSE;
    if (is_arithmetic_type(operand_1->type)) {
      /* Okay. */
    } else if (is_ptr_to_member_type(operand_1->type)) {
      operand_1_is_ptr_to_member = TRUE;
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    /* Check the operand types for compatibility. */
    operation_type = operand_1->type;  /* Assume. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      /* One or both of the operands has an error. */
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
        /* Both operands should be arithmetic (we have ruled out all the
           pointer cases above).  We also know already that operand_1 is
           arithmetic. */
        if (check_arithmetic_operand(&operand_2)) {
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

    result_type = get_logical_result_type(operand_1, &operand_2);
    change_binary_operand_types(operation_type, operand_1, &operand_2);
    if (funny_unsigned_comparison) {
      /* Check for pointless comparisons of unsigned integers against negative
         constants:
           u == -n   (always false)
           u != -n   (always true)
         The expression is not simplified.  Note that we check the nonconstant
         operand type before any type promotions and the constant value after
         any type change. */
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

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, prec_level, EOPT_NO_OPTIONS);

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*try_conversions=*/TRUE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
    (void)check_integral_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
    (void)check_integral_operand(&operand_2);
    result_type = determine_arithmetic_conversions(operand_1, &operand_2);
    change_binary_operand_types(result_type, operand_1, &operand_2);
    op = which_binary_operator(save_token, result_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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
  a_boolean             operand_1_is_false = FALSE;
  long                  local_result;
  a_boolean             known_result       = FALSE;
  a_token_kind          save_token;
  a_type_ptr            result_type;
  a_boolean             processed = FALSE;
  a_boolean             might_be_overloaded = FALSE;
  int                   prec_level;
  a_boolean             operand_1_transformations_done = FALSE;
  a_boolean             saved_evaluated = curr_expr_is_evaluated();
  a_boolean             expr2_evaluated;

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
        is_class_or_error_operand(operand_1)) {
      /* The first operand is a class in C++ mode.  We cannot convert it to
         an rvalue because a conversion function might be applied to it.
         However, we lose nothing by not doing this -- we know the first
         operand is not a constant. */
    } else {
      /* See if the first operand is a constant. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
      operand_1_transformations_done = TRUE;
      /* Note that pointer to member constants are tested by
         op_is_false_constant; that's why the is_scalar_type test is needed. */
      if (is_constant_operand(operand_1) && is_scalar_type(operand_1->type)) {
        operand_1_is_false = op_is_false_constant(operand_1);
        if (save_token == tok_and_and && operand_1_is_false) {
          /* 0 && something -- this always evaluates to a zero value. */
          local_result = 0;
          known_result = TRUE;
          expr2_evaluated = FALSE;
        } else if (save_token == tok_or_or && !operand_1_is_false) {
          /* non-zero || something -- this always evaluates to a value of 1. */
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
  scan_expr(&operand_2, prec_level, EOPT_NO_OPTIONS);
  /* Restore the evaluated flag as it was on entry. */
  expr_stack->evaluated = saved_evaluated;

  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             (is_class_or_error_operand(operand_1) ||
              is_class_or_error_operand(&operand_2))) {
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
                                   result, &processed);
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
    if (!known_result) {
      /* Normal case: the result is not known. */
      result_type = get_logical_result_type(operand_1, &operand_2);
      op = which_binary_operator(save_token, result_type);
      do_binary_operation(op, operand_1, &operand_2, result_type, result,
                          &operator_position);
    } else {
      /* The expression evaluates to a constant. */
      make_integer_constant_operand(result, local_result);
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_logical_operator */


static void keep_enum_in_result_type(a_type_ptr op1_type,
                                     a_type_ptr op2_type,
                                     a_type_ptr *result_type)
/*
If both of the operands have the same enumerated type, keep that information
in the result type.
*/
{
  a_type_ptr op1_enum, op2_enum;

  /* In C++, enumeration constants have the same type as the enumeration, so
     this routine is not needed. */
  if (C_dialect != C_dialect_cplusplus) {
    op1_type = skip_typerefs(op1_type);
    op2_type = skip_typerefs(op2_type);
    if (is_integral_type(op1_type) && is_integral_type(op2_type)) {
      op1_enum = underlying_enum_type(op1_type);
      op2_enum = underlying_enum_type(op2_type);
      if (op1_enum != NULL && op1_enum == op2_enum) {
        /* Both types are the same enum type, so keep the enum tag in
           the result type. */
#if CHECKING
        if (!is_integral_type(*result_type)) {
          internal_error(
                        "keep_enum_in_result_type: bad result type for enums");
        }  /* if */
#endif /* CHECKING */
        if (skip_typerefs(*result_type)->variant.integer.int_kind ==
                                          op1_enum->variant.integer.int_kind) {
          /* The result type has the same size/sign as the enum types, so
             use the enum type as the result type. */
          *result_type = op1_enum;
        } else {
          /* In some cases involving bit-fields in pcc mode that get widened
             to unsigned int instead of int, create a tagged version of the
             unsigned type.  Note that the type is not shared, but this case
             should not come up often. */
          op1_type = alloc_type((a_type_kind)tk_integer);
          *op1_type = **result_type;
          op1_type->variant.integer.enum_type = FALSE;
          op1_type->variant.integer.enum_info.affiliated_type = op1_enum;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* keep_enum_in_result_type */


static void process_boolean_controlling_expression(an_operand *result)
/*
*result is the controlling expression of an if/while/do-while/for statement,
or of a "?" operator.  Check that it has the right type.  Convert it from a
class type if necessary.
*/
{
  a_boolean processed = FALSE;
  a_boolean pointer_case;

  /* Convert from a class type to a scalar if necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result->type)) {
    try_to_convert_class_operand_to_builtin_type(result,
                                                 (a_builtin_type_kind_set)
                                                           (BTK_INTEGRAL |
                                                            BTK_FLOATING |
                                                            BTK_POINTER |
                                                            BTK_PTR_TO_MEMBER),
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Do lvalue --> rvalue and other transformations for the non-overloaded
       case. */
    do_operand_transformations(result, TOPT_NO_OPTIONS);
  }  /* if */
  /* Remember whether or not the expression has pointer type.  This is
     needed later, and the check here standardizes the operation to
     an integer result. */
  pointer_case = is_pointer_type(result->type) ||
                 is_ptr_to_member_type(result->type);
  /* Check that the operand is scalar or a pointer to member.  Note that
     this is done even for the cases where a class type has been converted
     to such a type, because the subroutine does some additional checking
     and some normalization of the expression. */
  if (check_boolean_controlling_expr(result)) {
    /* Issue a remark if the expression is constant.  The check is here
       instead of check_boolean_controlling_expr because we don't want
       to issue diagnostics for things like "i = 1&&2;".  Do not issue
       the error in constant expressions (which can happen only for
       conditional operators, i.e., "?", not for statements). */
    if (is_constant_operand(result)) {
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


static void reference_cast(an_operand       *operand,
                           a_base_class_ptr bcp)
/*
Convert operand (an lvalue that came from a reference) to the type
of the base class indicated by bcp.  It remains an lvalue.
*/
{
  a_boolean is_arrow_operator = TRUE;

  /* Convert to a pointer to the object. */
  take_address_of_lvalue(operand);
  /* Cast the pointer to a pointer to the new type. */
  base_class_cast_operand(operand, bcp, &is_arrow_operator,
                          /*check_cast_access=*/TRUE);
  /* Make an address (an lvalue) for the base class object. */
  conv_object_pointer_to_lvalue(operand);
}  /* reference_cast */


static a_boolean check_reference_conversions(an_operand *operand_1,
                                             an_operand *operand_2)
/*
operand_1 and operand_2 are the second and third operands of a "?" operator,
and they are class lvalues that came from references.  Check to see if the
reference conversions of ARM 4.7 can be used to bring bring them to a
common type.  If so, apply the conversion and return TRUE; otherwise,
return FALSE.
*/
{
  a_boolean        ref_conversions_apply = FALSE;
  a_type_ptr       type_1 = operand_1->type;
  a_type_ptr       type_2 = operand_2->type;
  a_base_class_ptr bcp;

  if ((bcp = find_base_class_of(type_1, type_2)) != NULL) {
    /* type_2 is a base class of type_1, so cast operand_1 to type_2. */
    ref_conversions_apply = TRUE;
    reference_cast(operand_1, bcp);
  } else if ((bcp = find_base_class_of(type_2, type_1)) != NULL) {
    /* type_1 is a base class of type_2, so cast operand_2 to type_1. */
    ref_conversions_apply = TRUE;
    reference_cast(operand_2, bcp);
  }  /* if */
  return ref_conversions_apply;
}  /* check_reference_conversions */


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


static void scan_conditional_operator(an_operand *operand_1,
                                      an_operand *result)
/*
Scan the "?" operator.  See section 3.3.15 of the standard.
*/
{
  an_operand            operand_2;
  an_operand            operand_3;
  a_source_position     operator_position;
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

  db_enter(4, "scan_conditional_operator");

  /* Check the first operand's type. */
  process_boolean_controlling_expression(operand_1);
  /* There is a sequence point after the first operand. */
  potential_sequence_point_after_operand(operand_1);

  expr2_evaluated = expr3_evaluated = saved_evaluated;
  if (saved_evaluated) {
    /* If the first operand is a constant and if the constant is zero, evaluate
       the third operand only.  If the first operand is constant and is not a
       constant zero, evaluate the second operand only. */
    operand_1_is_const = is_constant_operand(operand_1);
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
  }  /* if */

  /* Scan the second operand.   Evaluate the expression if the first
     operand is non-constant or a non-zero constant, and if we are currently
     evaluating expressions. */
  (void)get_token();
  expr_stack->nested_construct_depth++;
  expr_stack->evaluated = expr2_evaluated;
  scan_expr(&operand_2, PREC_LOWEST, EOPT_NO_OPTIONS);
  do_operand_transformations(&operand_2,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
  expr_stack->evaluated = saved_evaluated;
  expr_stack->nested_construct_depth--;

  /* Save the position of the (expected) colon. */
  copy_source_position(pos_curr_token, operator_position);

  if (!required_token(tok_colon, ec_exp_colon)) {
    /* The colon is missing. */
    make_error_operand(result);
    goto error_exit;
  }  /* if */

  /* Scan the third operand.  Evaluate the expression if the first operand
     is non-constant or a zero constant, and if we are currently evaluating
     expressions. */
  expr_stack->evaluated = expr3_evaluated;
  /* In C++, the 3rd operand is an assignment-expression (this was changed
     after the ARM) to allow things like "a ? i=1 : j=2". */
  scan_expr(&operand_3, (C_dialect != C_dialect_cplusplus ||
                         cfront_compatibility_mode) ? PREC_QUEST_MARK :
                                                      PREC_ASSIGNMENT,
                         EOPT_NO_OPTIONS);
  do_operand_transformations(&operand_3,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
  expr_stack->evaluated = saved_evaluated;

  /* Check the operands for compatibility.  Both must be arithmetic,
     both compatible struct/union types, both void, or both pointers. */
  if (curr_expr_kind_is(ek_template_arg) &&
      (is_nonarithmetic_type(operand_1->type) ||
       is_nonarithmetic_type(operand_2.type) ||
       is_nonarithmetic_type(operand_3.type))) {
    /* Non-arithmetic operations are not allowed in a template argument. */
    pos_error(ec_non_arith_operation_in_templ_arg, &operator_position);
    make_error_operand(result);
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
    change_operand_refs_to_error(&operand_3);
    processed = TRUE;
  } else if (C_dialect == C_dialect_cplusplus) {
    if (types_are_compatible(operand_2.type, operand_3.type)) {
      /* In C++, if the types are the same the result has that type.
         No arithmetic conversions are done (e.g., integral promotions
         are not done). */
      types_are_the_same = TRUE;
    } else {
      /* Check for cases where the operands are classes. */
      a_boolean operand_2_is_class =is_class_struct_union_type(operand_2.type);
      a_boolean operand_3_is_class =is_class_struct_union_type(operand_3.type);
      if (operand_2_is_class || operand_3_is_class) {
        /* If both operands are references, see if the reference conversions 
           of ARM 4.7 apply. */
        if (operand_2_is_class && operand_3_is_class &&
            is_an_lvalue(&operand_2) && is_an_lvalue(&operand_3) &&
            operand_2.came_from_reference &&
            operand_3.came_from_reference &&
            check_reference_conversions(&operand_2, &operand_3)) {
          /* The reference conversions do apply.  The subroutine has done
             them already. */
        } else {
          /* Look for C++ operator overloading cases.  The operator itself
             cannot be overloaded, but this also checks for cases where
             conversion functions can be used to convert the operands to
             types suitable for the built-in meaning of the operator. */
          check_for_operator_overloading((an_opname_kind)onk_question,
                                         /*unary_operator=*/FALSE,
                                         /*must_be_member_function=*/FALSE,
                                         /*try_conversions=*/TRUE,
                                         /*has_predef_meaning=*/TRUE,
                                         &operand_2, &operand_3,
                                         &operator_position,
                                         result, &processed);
          /* processed TRUE means an error has been detected. */
          if (processed) {
            err = TRUE;
          } else if (types_are_compatible(operand_2.type, operand_3.type)) {
            /* Check again for the types being the same after doing the
               conversions. */
            types_are_the_same = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!processed) {
    result_type = operand_2.type;  /* Assume. */
    if (types_are_the_same &&
        is_an_lvalue(&operand_2) && is_an_lvalue(&operand_3)) {
      /* In C++, if the types are the same and the second and third operands
         are lvalues they are left as lvalues. */
      result_is_an_lvalue = TRUE;
    } else {
      /* Convert the operands to rvalues. */
      expr_stack->evaluated = expr2_evaluated;
      conv_lvalue_to_rvalue(&operand_2);
      expr_stack->evaluated = expr3_evaluated;
      conv_lvalue_to_rvalue(&operand_3);
      expr_stack->evaluated = saved_evaluated;
    }  /* if */
    if (types_are_the_same) {
      /* If the types are the same no further checking of types is needed. */
    } else if (is_throw_operand(&operand_2)) {
      /* The second operand is a throw expression and the third is not
         (because if they both were, they would have the same types),
         so use the type of the third. */
      result_type = operand_3.type;
    } else if (is_throw_operand(&operand_3)) {
      /* The third operand is a throw expression and the second is not,
         so use the type of the second. */
      /* result_type = operand_2.type; -- already set. */
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
          /* The operands are incompatible. */
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
      } else if (is_arithmetic_type(operand_2.type)) {
        /* Both operands should be arithmetic. */
        (void)check_arithmetic_operand(&operand_3);
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
        change_binary_operand_types(result_type, &operand_2, &operand_3);
      }  /* if */
    }  /* if */
  }  /* if */

  if (err || is_error_operand(operand_1)) {
    make_error_operand(result);
  } else if (operand_1_is_const) {
    if (operand_1_is_false) {
      /* The first operand is a zero constant; return the third operand as
	 the result. */
      copy_operand(&operand_3, result);
    } else {
      /* The first operand is a non-zero constant; return the second operand
	 as the result. */
      copy_operand(&operand_2, result);
    }  /* if */
    result->came_from_reference = FALSE;
  } else {
    /* The first operand is not a constant, so build the expression. */
    if (result_is_an_lvalue) {
      /* If the result is an lvalue, the type of the "?" node must be a
         pointer. */
      operation_type = make_pointer_type(result_type);
    } else {
      operation_type = result_type;
    }  /* if */
    /* Make an operator node with the first part of the expression. */
    build_unary_result_operand(operand_1,
                               (an_expr_operator_kind)eok_question,
			       operation_type, result);
    /* Now link the other two operands from this one. */
    result->variant.expression->variant.operation.operands->next =
                                            make_node_from_operand(&operand_2);
    result->variant.expression->variant.operation.operands->next->next =
                                            make_node_from_operand(&operand_3);
    /* The result is an lvalue in C++ if the second and third operands are. */
    if (result_is_an_lvalue) {
      result->state = (an_operand_state)os_lvalue;
      result->type = result_type;
      result->variant.expression->variant.operation.
                                 returns_lvalue_instead_of_usual_rvalue = TRUE;
      result->ref_entries_list = merge_ref_lists(operand_2.ref_entries_list,
                                                 operand_3.ref_entries_list);
    }  /* if */
  }  /* if */

error_exit:

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

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
                                       operand->ref_entries_list);
          current_routine_entry()->assignment_to_this_done = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_this;
}  /* check_assignment_to_this_pointer */

#endif /* ASSIGNMENT_TO_THIS_ALLOWED */

static void scan_simple_assignment_operator(an_operand *operand_1,
                                            an_operand *result)
/*
Scan the simple assignment operator ("=").  See section 3.3.16 of the standard.
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         has_predef_meaning;
  a_type_ptr        result_type;

  db_enter(4, "scan_simple_assignment_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

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
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand_1->type)) {
      /* Look for C++ operator overloading cases. */
      /* "=" has a predefined meaning for C-style classes (i.e., bitwise
         assignment).  Also go that way for incomplete classes, to get
         a clearer error message. */
      has_predef_meaning = symbol_supplement_for_class(operand_1->type)->
                                          assignment_by_bitwise_copy_allowed ||
                           is_incomplete_type(operand_1->type);
      check_for_operator_overloading((an_opname_kind)onk_assign,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*try_conversions=*/FALSE,
                                     has_predef_meaning,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
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
      /* The type of the assignment is the destination type with any qualifiers
         dropped. */
      result_type = make_unqualified_type(operand_1->type);
      /* It's okay for operand_2 to be an indefinite function. */
      /* Do not force operand_2 to an rvalue; in C++ there are cases where
         a conversion might apply and might require an lvalue. */
      do_operand_transformations(&operand_2,
                                 TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION |
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      prep_assignment_operand(&operand_2, result_type,
                              ec_incompatible_assignment_operands,
                              &operator_position);
      build_binary_result_operand(operand_1, &operand_2,
                                  which_binary_operator(tok_assign,
                                                        result_type),
                                  result_type, result);
      /* In C++, assignment operators return lvalues. */
      if (C_dialect == C_dialect_cplusplus) {
        change_assignment_result_to_lvalue(result, operand_1);
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_simple_assignment_operator */


static void scan_compound_assignment_operator(an_operand *operand_1,
                                              an_operand *result)
/*
Scan the compound assignment operators (*= /= %= += -= <<= >>= &= ^= |=).
See section 3.3.16 of the standard.
*/
{
  a_token_kind          save_token;
  an_operand            operand_2;
  a_source_position     operator_position;
  a_boolean             err               = FALSE, processed = FALSE;
  a_type_ptr            result_type;
  a_type_ptr            operation_type;
  a_boolean             pointer_add_sub   = FALSE;

  db_enter(4, "scan_compound_assignment_operator");

  /* Save the operator. */
  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

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
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_or_error_operand(operand_1) ||
         is_class_or_error_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      if (C_dialect == C_dialect_cplusplus && is_enum_type(operand_1->type)) {
        /* Enum types are not allowed (because the enum promotes to integer
           for the operation, and then can't get back to enum). */
        if (allow_anachronisms) {
          pos_diagnostic(anachronism_error_severity,
                         ec_mixed_enum_type_anachronism, &operand_1->position);
        } else {
          error_in_operand(ec_enum_type_not_allowed, operand_1);
        }  /* if */
      }  /* if */
      if (check_modifiable_lvalue_operand(operand_1)) {
        modifying_lvalue(operand_1, /*value_used=*/TRUE);
      }  /* if */
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS);
      /* Check the operand types. */
      switch (save_token) {
        case tok_times_assign:
        case tok_divide_assign:
          (void)check_arithmetic_operand(operand_1);
          (void)check_arithmetic_operand(&operand_2);
          break;
        case tok_plus_assign:
        case tok_minus_assign:
          if (is_arithmetic_type(operand_1->type)) {
            /* If the first operand is arithmetic, the second must be also. */
            (void)check_arithmetic_operand(&operand_2);
          } else if (check_object_pointer_operand
                                             (operand_1, ec_expr_not_scalar)) {
            /* The first operand is a pointer, so the second one must be
               integral. */
            if (check_integral_operand(&operand_2)) {
              pointer_add_sub = TRUE;
            }  /* if */
          }  /* if */
          break;
        case tok_remainder_assign:
        case tok_shift_left_assign:
        case tok_shift_right_assign:
        case tok_and_assign:
        case tok_excl_or_assign:
        case tok_or_assign:
          (void)check_integral_operand(operand_1);
          (void)check_integral_operand(&operand_2);
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
        result_type = make_unqualified_type(operand_1->type);
        if (pointer_add_sub) {
          /* For pointer += or -=, integral promotions are not done, and
             the operation type is the first operand's type.  This is
             like the processing for pointer + integer and
             pointer - integer. */
          operation_type = operand_1->type;
        } else {
          /* Normal case. */
          operation_type = determine_arithmetic_conversions(operand_1,
                                                            &operand_2);
          cast_operand(operation_type, &operand_2, /*is_implicit_cast=*/TRUE);
        }  /* if */
        build_binary_result_operand(operand_1, &operand_2,
                                    which_binary_operator(save_token,
                                                          operation_type),
                                    result_type, result);
        /* In C++, assignment operators return lvalues. */
        if (C_dialect == C_dialect_cplusplus) {
          change_assignment_result_to_lvalue(result, operand_1);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_compound_assignment_operator */


static void build_accessible_base_class_list_for_throw(
                                                   an_expr_node_ptr throw_node)
/*
Add the list of accessible base classes to the thrown node throw_node
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


static void scan_throw_operator(an_operand *result)
/*
Scan the C++ throw operator.  See 15.2 in the ARM.  The syntax is

  throw assignment-expression
                             opt
*/
{
  an_operand         operand;
  a_source_position  start_position;
  a_boolean          err = FALSE, expr_present;
  an_expr_node_ptr   node, throw_node;
  a_dynamic_init_ptr dip;
  a_type_ptr         throw_type;

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
  }  /* if */

#if CHECKING
  if (curr_token != tok_throw) {
    internal_error("scan_throw_operator: expected throw");
  }  /* if */
#endif /* CHECKING */
  (void)get_token();

  /* See if the expression is present. */
#if 0
  /* This needs to be more sophisticated.  Consider, however
       throw + 1;
  */
#endif /* 0 */
  if (curr_token == tok_semicolon || curr_token == tok_rparen   ||
      curr_token == tok_rbrace    || curr_token == tok_rbracket ||
      curr_token == tok_comma     || curr_token == tok_colon    ||
      curr_token == tok_quest_mark) {
    /* No. */
    expr_present = FALSE;
  } else {
    /* Scan the expression. */
    expr_present = TRUE;
    scan_expr(&operand, PREC_ASSIGNMENT, EOPT_NO_OPTIONS);
    if (is_void_type(operand.type)) {
      /* Cannot throw a void expression. */
      error_in_operand(ec_void_throw, &operand);
    }  /* if */
  }  /* if */

  if (err) {
    /* Operator not allowed in this kind of expression. */
    make_error_operand(result);
    if (expr_present) change_operand_refs_to_error(&operand);
  } else {
    /* Build the throw node. */
    throw_node = alloc_expr_node((an_expr_node_kind)enk_throw);
    throw_node->type = void_type();
    if (expr_present) {
      /* There is a throw expression. */
      if (is_class_struct_union_type(operand.type)) {
        /* For a class type operand, generate a dynamic initialization that
           copies the value to an undesignated location. */
        throw_type = operand.type;
        prep_elision_initializer_operand(&operand, operand.type, &dip);
      } else {
        /* For a nonclass operand, generate an expression and then make a
           dynamic initialization entry for the expression. */
        do_operand_transformations(&operand, TOPT_NO_OPTIONS);
        node = make_node_from_operand(&operand);
        throw_type = node->type;
        dip = alloc_dtor_dynamic_init((a_dynamic_init_kind)dik_expression,
                                      throw_type,
                                      curr_expr_is_potentially_evaluated(),
                                      /*in_return_by_cctor_expression=*/FALSE,
                                      &operand.position);
        dip->variant.expression = node;
      }  /* if */
      throw_node->variant.throw_info->dynamic_init = dip;
      throw_node->variant.throw_info->type = throw_type;
      /* Generate a list of accessible base classes. */
      build_accessible_base_class_list_for_throw(throw_node);
      /* Mark the type as having been used in an exception.  (Also, if it
         "contains" any classes, they are marked as requiring external
         linkage.) */
      set_used_in_exception_flag(throw_type);
    } else {
      /* There is no throw expression (i.e., this is a rethrow). */
      /* Discard the throw supplement. */
      throw_node->variant.throw_info = NULL;
    }  /* if */
    /* Make an operand for the result. */
    make_expression_operand(throw_node, throw_node->type, result);
  }  /* if */

  error_position = start_position;
  result->position = start_position;

  db_exit();
}  /* scan_throw_operator */


static void scan_comma_operator(an_operand *operand_1,
                                an_operand *result)
/*
Scan the "," operator.  Note that this routine is not called if the
comma operator if not allowed (local_options flag
EOPT_DISALLOW_COMMA_OPERATOR).
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_type_ptr        result_type, operation_type;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         result_is_an_lvalue = FALSE;

  db_enter(4, "scan_comma_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

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
    change_operand_refs_to_error(operand_1);
    change_operand_refs_to_error(&operand_2);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_or_error_operand(operand_1) ||
         is_class_or_error_operand(&operand_2))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_comma,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*try_conversions=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     operand_1, &operand_2,
                                     &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS);
      do_operand_transformations(&operand_2,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION);
      /* Simplify the void expression. */
      simplify_void_operand(operand_1);
      /* In C++ mode, an lvalue in the second operand is preserved.  In C
         mode, an lvalue is converted to an rvalue. */
      if (C_dialect == C_dialect_cplusplus) {
        result_is_an_lvalue = is_an_lvalue(&operand_2);
      } else {
        conv_lvalue_to_rvalue(&operand_2);
      }  /* if */
      /* The result type is the type of the second operand. */
      operation_type = result_type = operand_2.type;
      if (result_is_an_lvalue) operation_type = make_pointer_type(result_type);
      /* Make a comma operator expression. */
      build_binary_result_operand(operand_1, &operand_2,
                                  (an_expr_operator_kind)eok_comma,
                                  operation_type, result);
      /* In C++ mode, the result is an lvalue if the second operation
         is an lvalue. */
      if (result_is_an_lvalue) {
        result->state = operand_2.state;
        result->type = result_type;
        result->came_from_reference = operand_2.came_from_reference;
        result->variant.expression->variant.operation.
                                 returns_lvalue_instead_of_usual_rvalue = TRUE;
        result->ref_entries_list = operand_2.ref_entries_list;
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_comma_operator */


static void make_anonymous_union_field_operand(
                                            a_symbol_ptr      sym_ptr,
                                            a_source_position *source_position,
                                            a_ref_entry_ptr   rep,
                                            an_operand        *result)
/*
Make an operand for a field that is a member of a top-level anonymous union.
(That is, an anonymous union that is not inside a struct or union.)
sym_ptr is the field; source_position indicates the field identifier
source position; and rep points to a reference entry, or is NULL if
none is needed.  The operand is built in *operand.  It's an lvalue for
the field.
*/
{
  a_variable_ptr union_var = sym_ptr->variant.field.anonymous_union_variable;
  an_operand     operand_1;

  /* Start with an operand for the base anonymous union variable. */
  make_lvalue_variable_operand(union_var, &operand_1, (a_ref_entry_ptr)NULL);
  /* Add a field selection to get to the field. */
  do_field_selection_operation(&operand_1, union_var->type,
                               /*is_arrow_operator=*/FALSE,
                               sym_ptr, source_position, rep, result);
  result->position = *source_position;
}  /* make_anonymous_union_field_operand */


static a_boolean bad_nested_function_variable_ref(a_symbol_ptr sym_ptr)
/*
sym_ptr is a symbol for a variable being referenced in an expression.
Return TRUE if the reference is invalid because either

(1)  we are inside a local class, and the variable is a nonstatic variable
     from an enclosing function (ARM 9.8), or
(2)  we are inside a default argument expression, and the variable is a
     local variable of an enclosing function (ARM 8.2.6).

The symbol may be a member of an anonymous union.
*/
{
  a_boolean      bad_ref = FALSE;
  a_scope_depth  sd;
  a_variable_ptr var;

  /* This sort of bad reference is only possible when we are inside a local
     class (the class itself or one of its member functions) or a
     default argument expression. */
  if (inside_local_class || expr_stack->is_default_arg_expression) {
    if (sym_ptr->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
      /* A reference to the file scope is okay. */
    } else if (sym_ptr->class_of_which_a_member != NULL) {
      /* A reference to a class member is okay. */
    } else {
      /* Get the variable for the symbol. */
      if (sym_ptr->kind == (a_symbol_kind)sk_variable) {
        var = sym_ptr->variant.variable.ptr;
      } else {
#if CHECKING
        if (sym_ptr->kind != (a_symbol_kind)sk_field) {
          internal_error("bad_nested_function_variable_ref: bad sym kind");
        }  /* if */
#endif /* CHECKING */
        var = sym_ptr->variant.field.anonymous_union_variable;
#if CHECKING
        if (var == NULL) {
          internal_error(
                 "bad_nested_function_variable_ref: field var not anon union");
        }  /* if */
#endif /* CHECKING */
      }  /* if */
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
            bad_ref = TRUE;
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
  return bad_ref;
}  /* bad_nested_function_variable_ref */


static void scan_identifier(an_operand               *result,
                            an_operand               *bound_function_selector,
                            a_local_expr_options_set local_options)
/*
Scan an identifier, and return an operand for it in *operand.  In C++,
also handle qualified names like A::x and operator names like "operator+".
If the identifier refers to a nonstatic member function, also set
bound_function_selector to the associated "this" pointer.
*/
{
  a_symbol_ptr      sym_ptr, projection_sym_ptr;
  a_variable_ptr    var_ptr;
  a_routine_ptr     routine_ptr;
  a_source_position start_position;
  a_ref_entry_ptr   rep;
  an_operand        this_pointer_operand;
  a_boolean         address_of_qualified_member_name = FALSE;
  a_type_ptr        qual_class_type;
  a_boolean         err = FALSE, is_operand_of_address_of;

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
  is_operand_of_address_of = (local_options & EOPT_OPERAND_OF_ADDRESS_OF) != 0;

  /* If the identifier is the start of a C++ qualified name, get the whole
     name.  If not, look the name up as a normal identifier.  This routine
     also handles operator names. */
  sym_ptr = coalesce_and_lookup_generalized_identifier
                                            (GID_NO_OPTIONS, ilm_normal, &err);
  if (locator_for_curr_id.is_semivisible_nested_type) {
    /* The symbol in the locator is a nested class that is not visible
       according to the ARM lookup rules but is returned in support of the
       nested class anachronism (ARM 18.3.5).  Issue an anachronism
       diagnostic. */
    sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                   locator_for_curr_id.specific_symbol);
  }  /* if */
  if (sym_ptr == NULL) {
    /* The symbol is not defined. */
    if (curr_expr_kind_is_const()) {
      /* In a constant expression, an undefined identifier is still
         flagged as "undefined" -- it makes the error message clearer. */
      str_error(ec_undefined_identifier,
                locator_for_curr_id.symbol_header->identifier);
      make_error_operand(result);
    } else {
      /* The symbol was not in the symbol table; enter it now in case its use
         later is as a function call. */
      sym_ptr = enter_symbol((a_symbol_kind)sk_undefined, &locator_for_curr_id,
                             decl_scope_level,
			     /*suppress_error=*/TRUE);
      /* Make a transient undefined symbol operand that will be either turned
         into an implicitly declared function or diagnosed as an error. */
      clear_operand((an_operand_kind)ok_undefined_symbol, result);
      result->type = unknown_type();
      result->variant.symbol = sym_ptr;
      result->ref_entries_list = ref_entry(sym_ptr, &pos_curr_token);
    }  /* if */
  } else {
    /* The symbol is defined. */
    /* Create a reference entry for the symbol if needed. */
    /* Don't do this if the symbol is an overloaded function (we don't
       yet know which function is being called). */
    if (sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
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
    } else {
      /* If this identifier is a qualified name for a class member and is
         the immediate operand of a unary "&" operator, it might represent
         a pointer-to-member. */
      /* The token_ends_expr test guards against cases like
           &A::x++
         where the "++" binds more tightly than the "&". */
      if (is_operand_of_address_of &&
          locator_for_curr_id.is_qualified_name &&
          sym_ptr->class_of_which_a_member != NULL &&
          token_ends_expr(next_token(), PREC_PREFIX, local_options)) {
        address_of_qualified_member_name = TRUE;
      }  /* if */
      projection_sym_ptr = locator_for_curr_id.specific_symbol;
      /* What kind of symbol is it? */
      switch (sym_ptr->kind) {
        case sk_constant:
          /* Constant (e.g., an enum constant).  Make a constant operand. */
          make_sym_constant_operand(sym_ptr, result);
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* In an integral constant expression, check that the constant is
               integral.  This is needed for nontype template arguments.
               It might also be needed for the extension that allows
               definition of constants within a class if that extension
               were to allow non-integral constants. */
            if (is_template_param_type(result->type)) {
              /* The constant has a template parameter type, e.g.,
                   template <class T, T N> class A {
                     int a[N+1];  // "N" here being processed
                   };
                 We don't know what type the parameter has, but we do know that
                 it has to be an integral type, so convert to "int". */
              cast_operand(integer_type((an_integer_kind)ik_int), result,
                           /*is_implicit_cast=*/FALSE);
            } else {
              (void)check_integral_operand(result);
            }  /* if */
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
              make_lvalue_variable_operand(var_ptr, result, rep);
            } else if (C_dialect == C_dialect_cplusplus &&
                       is_const_variable(var_ptr)) {
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
                make_constant_operand(con_val, result);
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
            if (bad_nested_function_variable_ref(sym_ptr)) {
              error_and_make_error_operand(ec_ref_to_nested_function_var,
                                           result);
              /* Avoid further diagnostics by making this an error
                 reference. */
              change_refs_to_error(rep);
              rep = NULL;
            } else {
              /* Make a variable operand that is a variable address node.
                 The type of the operand is a pointer to the type of the
                 variable. */
              make_lvalue_variable_operand(var_ptr, result, rep);
            }  /* if */
          }  /* if */
          break;
        case sk_routine:
normal_function:
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Function identifiers are not allowed in integral constant
               expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
          } else {
            /* Make a function designator operand for the function. */
            make_function_designator_operand(sym_ptr,
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
          } else if (sym_ptr->variant.field.anonymous_union_variable != NULL) {
            /* This field is a member of a top-level anonymous union. */
            /* If we're inside a local class, we are not allowed to reference
               non-static variables of the containing function.  If we're
               inside a default argument expression, we're not allowed to
               reference local variables of any containing function.
               Check for those. */
            if (bad_nested_function_variable_ref(sym_ptr)) {
              error_and_make_error_operand(ec_ref_to_nested_function_var,
                                           result);
              /* Avoid further diagnostics by making this an error
                 reference. */
              change_refs_to_error(rep);
              rep = NULL;
            } else {
              make_anonymous_union_field_operand(sym_ptr,
                                                 &locator_for_curr_id.
                                                               source_position,
                                                 rep, result);
            }  /* if */
          } else {
            /* Normal case -- field is a nonstatic data member of a class. */
            if (address_of_qualified_member_name) {
              /* The field was referenced by a qualified name and is the
                 immediate operand of a unary "&"; make up an operand that
                 preserves the qualified name so scan_ampersand_operator can
                 turn it into a pointer-to-member. */
              make_sym_for_member_operand(projection_sym_ptr, rep, result);
            } else {
              /* Normal case: "x" is interpreted as "this->x". */
              /* Make an operand for the "this" pointer. */
              if (make_this_pointer_operand(projection_sym_ptr,
                                          &locator_for_curr_id.source_position,
                                            /*check_cast_access=*/
                                              !locator_for_curr_id.
                                                 access_control_error_reported,
                                            &this_pointer_operand)) {
                /* Do the field selection relative to the "this" pointer. */
                /* Extract the possibly qualified version of the class type
                   pointed to. */
                qual_class_type = type_pointed_to(this_pointer_operand.type);
                do_field_selection_operation(&this_pointer_operand,
                                             qual_class_type,
                                             /*is_arrow_operator=*/TRUE,
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
          } else if (locator_for_curr_id.is_qualified_name) {
            /* The routine was referenced by a qualified name and is the
               immediate operand of a unary "&"; make up an operand that
               preserves the qualified name so scan_ampersand_operator can
               turn it into a pointer-to-member. */
            /* Also keep a function that was referenced by a qualified name
               in symbol form so that it can potentially be converted to
               a pointer-to-member (an extension); more typically, it
               will just be called. */
            make_sym_for_member_operand(projection_sym_ptr, rep, result);
          } else {
            /* Normal simple case: "f" is interpreted as "this->f". */
            /* Make an operand for the "this" pointer. */
            if (make_this_pointer_operand(projection_sym_ptr,
                                          &locator_for_curr_id.source_position,
                                          /*check_cast_access=*/
                                            !locator_for_curr_id.
                                                 access_control_error_reported,
                                          &this_pointer_operand)) {
              /* Make an operand for the function bound to the "this"
                 pointer. */
              do_member_function_selection_operation(&this_pointer_operand,
                                                     sym_ptr,
                                                     &locator_for_curr_id,
                                                     rep,
                                                     result,
                                                     bound_function_selector);
            } else {
              /* There was some problem in constructing the "this" operand. */
              make_error_operand(result);
            }  /* if */
          }  /* if */
          break;
        case sk_overloaded_function:
          /* Overloaded function. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
          } else {
            /* Make an operand for an indefinite function.  Note that we
               do not generate a "this" parameter at this point, even if
               we could determine that all the functions in the overload
               cluster are nonstatic member functions.  It's easier to
               generate it at the other end of the overload resolution
               when we know for sure whether or not we need it. */
            make_indefinite_function_operand(projection_sym_ptr,
                                             (a_boolean)locator_for_curr_id.
                                                             is_qualified_name,
                                             result);
          }  /* if */
          break;
        case sk_function_template:
          /* Function template. */
          if (curr_expr_kind_is(ek_integral_constant)) {
            /* Not allowed in integral constant expressions. */
            error_and_make_error_operand(ec_expr_not_constant, result);
          } else {
            make_indefinite_function_operand(projection_sym_ptr,
                                             /*is_qualified_name=*/FALSE,
                                             result);
          }  /* if */
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
            scan_functional_notation_type_conversion(type_symbol_type(sym_ptr),
                                                     result,
                                                     local_options);
            goto after_advance_past_id;
          } else {
            /* Otherwise, an error. */
            error_and_make_error_operand(ec_type_identifier_not_allowed,
                                         result);
          }  /* if */
          break;
        case sk_parameter:
          if (expr_stack->is_default_arg_expression ||
              !curr_expr_kind_is(ek_sizeof)) {
            /* This is a parameter referenced within a C++ default argument
               expression, which is an error (ARM 8.2.6); or, any reference
               except in a sizeof. */
            error_and_make_error_operand(ec_param_not_allowed, result);
            change_refs_to_error(rep);
          } else {
            /* Use of a parameter in a sizeof expression, something like
                 void f(a, int b[sizeof(a)]);
               Create a constant pointer to the right type to make an lvalue
               of the right type, since there is no variable yet (the
               parameter is represented by an sk_parameter symbol). */
            a_constant constant;
            a_type_ptr param_type;
            check_assertion(sym_ptr->kind == (a_symbol_kind)sk_parameter);
            param_type = sym_ptr->variant.param_id->type;
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
  /* Advance past the identifier. */
  (void)get_token();
after_advance_past_id:

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

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

  db_enter(4, "scan_expr_full");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "precedence level = %d\n", prec_level);
  }  /* if */
#endif /* DEBUG */

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
      /* Watch out for something like "S::*". */
      if (!is_qualified_name_start()) goto bad_start_of_primary;
      scan_identifier(&local_result, &local_bound_function_selector,
                      local_options);
      break;
    case tok_this:
      /* In C++, "this" in a nonstatic member function is a non-lvalue that
         points to the object for which the member function was called. */
      if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        /* We're not inside a function. */
        error_and_make_error_operand(ec_this_used_incorrectly, &local_result);
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
          make_this_variable_operand(this_var, &local_result);
        }  /* if */
      }  /* if */
      (void)get_token();
      break;
    case tok_float_constant:
      make_constant_operand(&const_for_curr_token, &local_result);
      /* Floating constants are not allowed in preprocessing expressions.
         In integral constant expressions, they are allowed only as the 
         immediate operand of a cast. */
      if (curr_expr_kind_is(ek_pp) ||
	  (curr_expr_kind_is(ek_integral_constant) &&
           !(local_options & EOPT_OPERAND_OF_CAST))) {
	error_and_make_error_operand(ec_expr_not_integral, &local_result);
      }  /* if */
      (void)get_token();
      break;
    case tok_string_literal:
      make_string_constant_operand(&const_for_curr_token, &local_result);
      if (curr_expr_kind_is(ek_pp) ||
	  curr_expr_kind_is(ek_integral_constant)) {
	error_and_make_error_operand(ec_expr_not_integral, &local_result);
      }  /* if */
      (void)get_token();
      break;
    case tok_int_constant:
    case tok_char_constant:
      make_constant_operand(&const_for_curr_token, &local_result);
      (void)get_token();
      break;

    case tok_plus_plus:
    case tok_minus_minus:
      scan_prefix_incr_decr(&local_result);
      break;

    case tok_ampersand:
      scan_ampersand_operator(&local_result);
      break;

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
      /* In C++, these type keywords begin a functional-notation type
         conversion (ARM 5.2.3).  In C, they're a syntax error. */
      if (C_dialect != C_dialect_cplusplus) goto bad_start_of_primary;
      if (next_token() == tok_lparen) {
        scan_functional_notation_type_conversion(type_keyword(),
                                                 &local_result,
                                                 local_options);
      } else {
        /* No parenthesis following the type, so issue an error. */
        error_and_make_error_operand(ec_type_identifier_not_allowed,
                                     &local_result);
        (void)get_token();
      }  /* if */
      break;

    case tok_throw:
      scan_throw_operator(&local_result);
      break;
     
    default:
bad_start_of_primary:
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
        str_error(ec_undefined_identifier,
                  local_result.variant.symbol->header->identifier);
        make_error_operand(&local_result);
      }  /* if */
    }  /* if */
    if (local_result.bound_function) {
      /* Do not allow bound functions functions to survive unless they
         are about to be called. */
      if (curr_token == tok_lparen) {
        /* The bound function is about to be called, so it's okay. */
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
	scan_field_selection_operator(&operand, &local_result,
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
    str_error(ec_undefined_identifier,
              local_result.variant.symbol->header->identifier);
    make_error_operand(&local_result);
  }  /* if */
  /* Selector_ref_entry_list will be set to the reference entry list for the
     selector object if there is one. */
  selector_ref_entry_list = NULL;
  if (local_result.bound_function) {
    /* Do not allow bound functions to survive unless the caller
       permits it. */
    if (!(local_options & EOPT_ALLOW_BOUND_FUNCTION)) {
      /* Bound function not allowed. */
      error_in_operand(ec_bound_function_must_be_called, &local_result);
      local_result.bound_function = FALSE;
    } else {
      /* Bound function allowed.  Return the operand for the object to
         which the function is bound in *bound_function_selector. */
#if CHECKING
      if (bound_function_selector == NULL) {
        internal_error("scan_expr_full: bound_function_selector == NULL");
      }  /* if */
#endif /* CHECKING */
      copy_operand(&local_bound_function_selector, bound_function_selector);
      selector_ref_entry_list = bound_function_selector->ref_entries_list;
    }  /* if */
  }  /* if */

  copy_operand(&local_result, result);

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


an_expr_node_ptr scan_switch_expression(void)
/*
Scan an integral selector expression for a switch statement, and return
a pointer to the expression tree.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  a_boolean           processed = FALSE;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_switch_expression");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  /* Make sure it's an integer.  Convert from a class type to an integer if
     necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result,
                                                 (a_builtin_type_kind_set)
                                                                  BTK_INTEGRAL,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Non-class (i.e., normal) case. */
    do_operand_transformations(&result, TOPT_NO_OPTIONS);
    (void)check_integral_operand(&result);
  }  /* if */
  expression = make_node_from_operand(&result);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_switch_expression */


an_expr_node_ptr scan_void_expression(void)
/*
Scan a "void expression," i.e., one whose value is discarded.  This is
used for expression statements, the increment expression of a "for", etc.
This routine is not used for constant or not-evaluated expressions.
*/
{
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_void_expression");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  simplify_void_operand(&result);
  expression = make_node_from_operand(&result);
  /* Indicate that the value of the node is not used. */
  set_expr_result_not_used(expression);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();

  return expression;
}  /* scan_void_expression */


void scan_default_arg_expr(a_param_type_ptr ptp)
/*
Scan a default argument expression on a formal parameter declaration, change
its type as required by the type of the formal parameter, and attach the
expression node to the param type entry.  If an error is detected in the
expression scan, an error node is assigned.  If ptp is NULL (as the result of
a prior error) just do the scan.
*/
{
  an_operand          result;
  an_expr_node_ptr    node;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_default_arg_expr");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  expr_stack_entry.is_default_arg_expression = TRUE;
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST,
            EOPT_NO_OPTIONS | EOPT_DISALLOW_COMMA_OPERATOR);
  if (ptp != NULL) {
    /* Convert to the required type. */
    prep_argument_operand(&result, ptp, (a_user_conv_descr_ptr)NULL,
                          ec_bad_default_arg_type);
  } else {
    do_operand_transformations(&result, TOPT_NO_OPTIONS);
  }  /* if */
  node = make_node_from_operand(&result);
  if (ptp != NULL) ptp->default_arg_expr = node;
  pop_expr_stack();
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
the return expression in a routine that returns its value via a copy
constructor.  The destructor call on the topmost initialization is
optimized away, but there's no way to know that when it is generated,
so all such initializations are put in the dynamic init entries but
the destructor routines are not marked as referenced.  The dynamic
init entries are placed on a list and here the remaining referenced
destructor routines are marked as actually referenced.
*/
{
  a_dynamic_init_dtor_fixup_ptr didfp, didfp_next;
  a_routine_ptr                 dtor_routine;

  for (didfp = expr_stack->dynamic_init_dtor_fixup_list;
       didfp != NULL;
       didfp = didfp_next) {
    didfp_next = didfp->next;
    dtor_routine = didfp->dynamic_init->destructor;
    if (dtor_routine != NULL) {
      a_symbol_ptr dtor_sym =
                         (a_symbol_ptr)dtor_routine->source_corresp.assoc_info;
      /* Check access to the destructor and mark it referenced.  Note that
         we know that the dynamic initialization is in an evaluated part of the
         expression, because unevaluated initializations are not put on the
         fixup list. */
      reference_to_implicitly_invoked_function(dtor_sym, &didfp->position,
                                               dtor_routine->source_corresp.
                                                       class_of_which_a_member,
                                               /*honor_virtual=*/FALSE,
                                               /*evaluated=*/TRUE,
                                              /*suppress_access_check=*/FALSE);
    }  /* if */
    /* Free the one entry. */
    free_dynamic_init_dtor_fixup(didfp);
  }  /* for */
}  /* fix_up_dynamic_init_dtors */


an_expr_node_ptr scan_return_expression(a_type_ptr         required_type,
                                        an_error_code      err_code,
                                        a_dynamic_init_ptr *dip)
/*
Scan an expression on a return statement and convert it to the type
required_type; issue the error err_code if it cannot be converted to that
type.  Return a pointer to the expression.  If the current routine is
one that returns its value via a copy constructor, set *dip to point to
the appropriate dynamic initialization entry and return NULL.
*/
{
  a_routine_ptr       curr_routine = current_routine_entry();
  a_type_ptr          routine_type;
  an_expr_node_ptr    expression;
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           return_by_cctor_case;

  db_enter(3, "scan_return_expression");

  *dip = NULL;
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  return_by_cctor_case = FALSE;
  routine_type = skip_typerefs(curr_routine->type);
  if (routine_type->variant.routine.extra_info->value_returned_by_cctor) {
    /* The current routine returns its value via a copy constructor. */
    return_by_cctor_case = TRUE;
    expr_stack->in_return_by_cctor_expression = TRUE;
  }  /* if */
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  if (return_by_cctor_case) {
    /* The current routine returns its value via a copy constructor.
       Build a dynamic initialization entry for the return statement. */
    prep_return_by_cctor_operand(&result, required_type, err_code, dip);
    /* Fix up destructor references in the overall expression. */
    fix_up_dynamic_init_dtors();
    expression = NULL;
  } else {
    /* Normal case. */
    /* The required type can be void if we are in cfront mode.  If it is
       void just take the expression as we found it -- don't try to
       convert it to void. */
    if (cfront_compatibility_mode && is_void_type(required_type)) {
      /* Leave operand alone. */
    } else {
      /* Convert to the required type. */
      prep_initializer_operand(&result, required_type,
                               (a_user_conv_descr_ptr)NULL,
                               /*initializing_return_value=*/TRUE,
                               err_code);
    }  /* if */
    expression = make_node_from_operand(&result);
  }  /* if */
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
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
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_pp_expression");

  push_expr_stack((an_expression_kind)ek_pp, &expr_stack_entry);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();

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
Scan an integral constant expression.  See section 3.4 in the C standard.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_integral_constant_expression");

  push_expr_stack((an_expression_kind)ek_integral_constant, &expr_stack_entry);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS);
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_integral_constant_expression */


void scan_new_array_dimension_expression(a_boolean        *is_constant,
                                         an_expr_node_ptr *expression,
                                         a_constant       *constant)
/*
Scan an array dimension in a new-type-name (see 5.3.3 in the ARM); it's
an integral expression that might be non-constant.  (It's also required
to be non-negative, but the caller must check that.)  Return either
*is_constant TRUE and a constant value in *constant, or *is_constant FALSE
and a pointer to the expression tree in *expression.  Used only in
C++ mode.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  int                 constant_sign;
  a_boolean           processed = FALSE;

  db_enter(3, "scan_new_array_dimension_expression");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);
  /* Convert from a class type to integral if necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result,
                                                 (a_builtin_type_kind_set)
                                                                  BTK_INTEGRAL,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Do lvalue --> rvalue and other transformations for the non-overloaded
       case. */
    do_operand_transformations(&result, TOPT_NO_OPTIONS);
  }  /* if */
  /* Check that the expression is integral. */
  (void)check_integral_operand(&result);
  /* Return a constant or expression depending on what was scanned. */
  *is_constant = TRUE;
  switch (result.kind) {
    case ok_error:
      /* Some sort of error; message was already issued. */
      set_error_constant(constant);
      break;
    case ok_expression:
      *expression = result.variant.expression;
      *is_constant = FALSE;
      break;
    case ok_constant:
      /* Constant.  The constant must be non-negative.  If it is zero,
         it is rendered as an expression. */
      copy_constant(&result.variant.constant, constant);
      if (!is_error_constant(constant)) {
#if CHECKING
        if (constant->kind != (a_constant_repr_kind)ck_integer) {
          internal_error(
                    "scan_new_array_dimension_expression: array size not int");
        }  /* if */
#endif /* CHECKING */
        constant_sign = sign_of_integer_constant(constant);
        if (constant_sign < 0) {
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
      internal_error("scan_new_array_dimension_expression: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  pop_expr_stack();

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
}  /* scan_new_array_dimension_expression */


static a_boolean constant_references_non_external_entity(
                                                       a_constant_ptr constant)
/*
Return TRUE if the indicated constant references a non-external entity,
e.g., a local variable.
*/
{
  a_boolean               refs_non_ext = FALSE;
  a_source_correspondence *scp;

  if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* An address constant.  See if the object referenced is external. */
    /* Get a pointer to the source correspondence for the entity. */
    switch (constant->variant.address.kind) {
      case abk_routine:
        scp = &constant->variant.address.variant.routine->source_corresp;
        break;
      case abk_variable:
        scp = &constant->variant.address.variant.variable->source_corresp;
        break;
      case abk_constant:
        scp = &constant->variant.address.variant.constant->source_corresp;
        break;
#if CHECKING
      default:
        internal_error(
                  "constant_references_non_external_entity: bad address kind");
#endif /* CHECKING */
    }  /* switch */
    if (scp->class_of_which_a_member != NULL) {
      /* The entity is a class member.  If the class is a local class,
         the entity is non-external.  Otherwise, the class will be forced
         to be external by this reference. */
      a_type_ptr class_type = scp->class_of_which_a_member;
      if (class_type->source_corresp.is_local_to_function) {
        refs_non_ext = TRUE;
      } else {
        /* Force the class to be external. */
        set_force_external_linkage_flag(class_type);
      }  /* if */
    } else {
      /* Not a class member. */
      refs_non_ext = (scp->name_linkage != (a_name_linkage_kind)nlk_external &&
                      scp->name_linkage !=
                                  (a_name_linkage_kind)nlk_cplusplus_external);
    }  /* if */
  }  /* if */
  return refs_non_ext;
}  /* constant_references_non_external_entity */


void scan_template_argument_constant_expression(a_type_ptr param_type,
                                                a_constant *constant)
/*
Scan a constant argument in a template reference.  Issue an error if it
is incompatible with the corresponding parameter type, param_type.
Return the constant in *constant.
*/
{
  an_operand           result;
  an_expr_stack_entry  expr_stack_entry;
  an_arg_match_summary arg_summary;
  a_boolean            okay;

  db_enter(3, "scan_template_argument_constant_expression");

  push_expr_stack((an_expression_kind)ek_template_arg, &expr_stack_entry);
  expr_stack_entry.is_template_arg_expression = TRUE;
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Check that its type is correct. */
  determine_arg_match_level(&result, (a_type_ptr)NULL, param_type,
                            /*try_user_conversions=*/FALSE, &arg_summary);
  okay = FALSE;
  /* Only an "exact match" according to the overloading resolution rules
     (ARM 14.2) is allowed, but that does allow trivial conversions. */
  if (arg_summary.match_level == aml_exact) {
    okay = TRUE;
  } else if (arg_summary.match_level == aml_promotion ||
             arg_summary.match_level == aml_std_conversion) {
    /* In non-strict mode, allow promotions and standard conversions
       as an extension. */
    if (strict_ansi_mode) {
      if (strict_ansi_error_severity != es_error) {
        okay = TRUE;
        pos_ty2_warning(ec_bad_nontype_template_arg, &result.position,
                        result.type, param_type);
      }  /* if */
    } else {
      okay = TRUE;
      pos_ty2_remark(ec_bad_nontype_template_arg, &result.position,
                     result.type, param_type);
    }  /* if */
  }  /* if */
  if (okay) {
    /* Convert to the required type (i.e., do any required trivial
       conversions). */
    prep_initializer_operand(&result, param_type, (a_user_conv_descr_ptr)NULL,
                             /*initializing_return_value=*/FALSE,
                             ec_bad_nontype_template_arg);
    /* Make a constant from the operand. */
    extract_constant_from_operand(&result, constant);
    /* If the template parameter has a reference type, give the constant
       a reference type (instead of the pointer type it has). */
    if (is_reference_type(param_type) && !is_error_operand(&result)) {
      check_assertion(is_pointer_type(constant->type));
      constant->type = param_type;
    }  /* if */
    /* Make the sure that the constant does not use any local variables,
       etc., since the template will be created at the file scope. */
    if (constant_references_non_external_entity(constant)) {
      pos_error(ec_nonexternal_entity_in_template_arg, &result.position);
      set_error_constant(constant);
    }  /* if */
  } else {
    /* Some error. */
    if (arg_summary.match_level == aml_error || is_error_operand(&result)) {
      /* Error already issued. */
    } else {
      pos_ty2_error(ec_bad_nontype_template_arg, &result.position,
                    result.type, param_type);
    }  /* if */
    set_error_constant(constant);
  }  /* if */
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_template_argument_constant_expression */


void scan_constant_initializer_expression(a_type_ptr required_type,
                                          a_constant *constant)
/*
Scan a constant initializer expression.  Convert the constant to
required_type; issue an error if it is incompatible with that type.
See section 3.4 in the ANSI C standard.  Used in C++ for scanning
constant class members (an extension).
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_constant_initializer_expression");

  push_expr_stack((an_expression_kind)ek_init_constant, &expr_stack_entry);
  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type, (a_user_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           ec_bad_initializer_type);
  /* Make a constant from the operand. */
  extract_constant_from_operand(&result, constant);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_constant(constant);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_constant_initializer_expression */


void scan_initializer_expression(a_type_ptr       required_type,
                                 a_boolean        *is_constant,
                                 an_expr_node_ptr *expression,
                                 a_constant       *constant)
/*
Scan an initializer expression.  See sections 3.4 and 3.5.7 in the standard.
The expression is converted to required_type; an error is issued if it
is incompatible with that type.  The expression can be constant or
nonconstant; on return, *is_constant is set accordingly, and the result
is returned either in *expression or in *constant.  Note that the
required_type may not be an array type.  This routine is not used when
copy constructor elision is possible; see scan_class_initializer_expression.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_initializer_expression");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Do fold constant addressing expressions to constants so that static
     initialization can be more easily discerned. */
  expr_stack->fold_constant_addr_exprs = TRUE;
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type, (a_user_conv_descr_ptr)NULL,
                           /*initializing_return_value=*/FALSE,
                           ec_bad_initializer_type);
  /* Return a constant or expression depending on what was scanned. */
  *is_constant = TRUE;
  switch (result.kind) {
    case ok_error:
      /* Some sort of error; message was already issued. */
      set_error_constant(constant);
      break;
    case ok_expression:
      *expression = result.variant.expression;
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
  an_operand          operand;
  an_expr_stack_entry expr_stack_entry;

  /* Even though this is not an expression scan, make sure the expr_stack
     has something on it. */
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Make an operand for the expression. */
  make_expression_operand(expr, expr->type, &operand);
  operand.position = *err_pos;
  /* Do the conversion. */
  prep_argument_operand(&operand, param, (a_user_conv_descr_ptr)NULL,
                        ec_incompatible_param);
  /* Make an expression again. */
  expr = make_node_from_operand(&operand);
  pop_expr_stack();
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

The dynamic initialization entry will also indicate a destructor if
appropriate.
*/
{
  an_operand          result;
  an_expr_stack_entry expr_stack_entry;
  a_boolean           okay = TRUE;

  db_enter(3, "scan_class_initializer_expression");
  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_DISALLOW_COMMA_OPERATOR);
  /* Find out whether or not the conversion is possible, and
     build a dynamic initialization entry to describe the initialization. */
  prep_elision_initializer_operand(&result, required_type, dip);
  /* *dip == NULL means there was an error. */
  if (*dip == NULL) okay = FALSE;
  pop_expr_stack();
  db_exit();
  return okay;
}  /* scan_class_initializer_expression */


an_expr_node_ptr scan_boolean_controlling_expression(void)
/*
Scan an expression that is used in controlling contexts that need a boolean
result, such as if, while, do while, or for statements.  The type of the
expression must be scalar, a pointer-to-member type, or must be of a
class type that can be converted to such a type.
*/
{
  an_operand          result;
  an_expr_node_ptr    expr;
  an_expr_stack_entry expr_stack_entry;

  db_enter(3, "scan_boolean_controlling_expression");

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry);
  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, EOPT_NO_OPTIONS);

  /* Check its type and normalize it. */
  process_boolean_controlling_expression(&result);
  expr = make_node_from_operand(&result);
  pop_expr_stack();

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expr);
  }  /* if */
#endif /* DEBUG */

  db_exit();
  return expr;
}  /* scan_boolean_controlling_expression */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
