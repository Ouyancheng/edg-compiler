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
#include "preproc.h"
#include "folding.h"
#include "cmd_line.h"
#include "types.h"
#include "decls.h"
#include "target.h"

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
                           an_expression_kind       expression_kind,
                           a_local_expr_options_set local_options);
/* Interface to scan_expr_full for the simple case where a bound function
   cannot be returned. */
#define scan_expr(result, prec_level, expression_kind, local_options) \
  scan_expr_full((result), (an_operand *)NULL, (prec_level),          \
                 (expression_kind), (local_options))


static a_boolean operation_has_side_effects(an_expr_node_ptr node)
/*
Return TRUE if the (operation) node has side effects.
*/
{
  a_boolean        has_side_effects = FALSE;
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
  }  /* switch */

  /* For the operations that do not cause side effects, check the operands for
     side effects. */
  operand = node->variant.operation.operands;
  while (operand != NULL && !has_side_effects) {
    has_side_effects = node_has_side_effects(operand);
    operand = operand->next;
  }  /* while */

  return has_side_effects;
}  /* operation_has_side_effects */


a_boolean node_has_side_effects(an_expr_node_ptr node)
/*
Return TRUE if the expression node has side effects.
*/
{
  a_boolean has_side_effects = FALSE;

  switch (node->kind) {
    case enk_error:
      /* Who knows what an error node might have done -- suppress the
         warning. */
      has_side_effects = TRUE;
      break;
    case enk_constant:
    case enk_variable_address:
    case enk_routine_address:
    case enk_field:
      break;
    case enk_operation:
      has_side_effects = operation_has_side_effects(node);
      break;
    case enk_variable:
      /* Note that we test the variable's type, not the node type, because of
         an IL shorthand that allows omission of the cast to the unqualified
         version of the type. */
      has_side_effects =
                      is_volatile_qualified_type(node->variant.variable->type);
      break;
    case enk_temp_init:
    case enk_new_init:
      /* At the very least, these have the side effect of initializing
         something.  They might also call a constructor, etc. */
      has_side_effects = TRUE;
      break;
#if CHECKING
    default:
      internal_error("node_has_side_effects: bad node kind");
#endif /* CHECKING */
  }  /* switch */

  return has_side_effects;
}  /* node_has_side_effects */


static void simplify_void_node(an_expr_node_ptr *node_ptr,
                               a_boolean        *has_effect)
/*
The expression node pointed to by *node_ptr has been scanned as a void
expression.  Examine it to see if it can be simplified by removing parts
that do nothing.  Change *node_ptr to point to the simplified expression
tree.  Return *has_effect == TRUE if the node has some side effect or if it
is an explicit cast-to-void node, for which a warning should not be issued.
*/
{
  an_expr_node_ptr node = *node_ptr;

  /* This routine could do various kinds of pruning -- in fact, it used to;
     however, in accord with the philosophy that the front end does no
     optimization, it now only removes an unnecessary top-level cast to
     void. */
  *has_effect = FALSE;
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
    *has_effect = TRUE;
  }  /* while */
  /* See if the node has some effect. */
  if (!*has_effect) *has_effect = node_has_side_effects(node);
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
  a_boolean has_effect;

  if (!is_expression_operand(operand)) {
    /* An operand that is not an expression cannot have side effects.
       For error operands, assume that the original form might have had
       an effect, and suppress the warning. */
    has_effect = is_error_operand(operand);
  } else {
    /* For an expression, traverse the tree to see if it has side effects
       and to simplify it. */
    simplify_void_node(&operand->variant.expression, &has_effect);
  }  /* if */
  if (!has_effect) {
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
    case tok_lt:
    case tok_gt:
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


static void scan_subscript_operator
                           (an_operand               *operand_1,
	                    an_operand               *result,
	                    an_expression_kind       expression_kind)
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

  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Subscripting not allowed in preprocessing expression. */
    pos_error(ec_bad_pp_operator, &operator_position);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Subscripting not allowed in integral constant expression. */
    pos_error(ec_expr_not_integral, &operator_position);
    err = TRUE;
  }  /* if */

  /* Get past the opening bracket. */
  (void)get_token();
  add_stop_token(tok_rbracket);

  /* Scan the second operand. */
  scan_expr(&operand_2, PREC_LOWEST, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Subscripting is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand_1->type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_subscript,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
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

      /* The first operand must be a pointer. */
      if (check_object_pointer_operand
                                  (operand_1, ec_expr_not_pointer_to_object)) {
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
                          &operator_position, expression_kind);
      /* This is an lvalue; the expression or constant giving the address
         has type pointer-to-X, but the operand has type X. */
      result->type = result_type;
      result->state = (an_operand_state)os_lvalue;
      /* Preserve the cross-reference entries from the first operand (the
         array). */
      result->xref_entries_list = operand_1->xref_entries_list;
    }  /* if */
  }  /* if */

  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);

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
                                          a_printf_scan_state *pss_ptr)
/*
Return the type that the next argument to a printf or scanf call should have,
by finding the next thing in the format string that consumes an argument.
Return NULL if no further arguments are needed.  is_scanf is TRUE for
scanf/FALSE for printf; *fmt_string_ptr points to the current position 
in the format string (it will be updated); and *pss_ptr is maintained
to handle resuming the scan after a "*" field width or precision.
If there is an error in the format string, issue a warning and set
*fmt_string_ptr to NULL.

See 4.9.6.1 in the standard for printf, 4.9.6.2 for scanf.
*/
{
  a_type_ptr          required_type;
  char                *fmt_string = *fmt_string_ptr;
  a_printf_scan_state pss = *pss_ptr;
  a_boolean           l_size, L_size, h_size, add_pointer;

  /* Pick up in the middle if the previous call returned a field width
     or precision. */
  if (pss == pss_after_field_width) goto after_field_width;
  if (pss == pss_after_precision) goto after_precision;

  /* Look for the next "%" in the string, or the null that terminates it. */
another_specifier:;
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
      if (*fmt_string == '*') fmt_string++;
    } else {
      while (*fmt_string == '-' || *fmt_string == '+' || *fmt_string == ' ' ||
             *fmt_string == '#' || *fmt_string == '0') fmt_string++;
    }  /* if */
    /* An optional field width is next.  For printf, it can be a "*". */
    if (isdigit(*fmt_string)) {
      /* Decimal integer field width.  Skip over it. */
      do {} while (isdigit(*++fmt_string));
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
      if (isdigit(*fmt_string)) {
        /* Decimal integer precision.  Skip over it. */
        do {} while (isdigit(*++fmt_string));
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
    if (*fmt_string == 'l') {
      l_size = TRUE;
      fmt_string++;
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
    add_pointer = is_scanf;
    switch (*fmt_string++) {
      case 'd':
      case 'i':
        /* int conversion.  If "l" was specified, long conversion;
           if "h" was specified for scanf, short conversion. */
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_long);
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
        if (l_size) {
          required_type = integer_type((an_integer_kind)ik_unsigned_long);
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
        add_pointer = TRUE;
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
  a_type_ptr required_type;

  /* Find the next formatting specifier in the string. */
  required_type = next_printf_scanf_arg_type(is_scanf, fmt_string_ptr,
                                             pss_ptr);
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
      if (!interchangeable_types(required_type, argument_operand->type)) {
        /* The argument type does not match the required type. */
        if (!is_error_type(argument_operand->type)) {
          pos_warning(ec_printf_arg_mismatch, &argument_operand->position);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_printf_scanf_arg */


static void scan_call_arguments(
                              a_type_ptr              function_type,
                              an_expression_kind      expression_kind,
                              a_boolean               already_after_left_paren,
                              an_expr_node_ptr        *p_argument_list,
                              a_boolean               overloaded_function_case,
                              an_argument_summary_ptr *arg_summary_list)
/*
Scan the arguments of a function call and return a list of argument
expressions in *p_argument_list.  The type of the function being called is
given by function_type; function_type is NULL if the type is not known.
The current expression kind is given by expression_kind.  The current token
at the time of call is the opening "(" of the argument list, unless
already_after_left_paren is TRUE, in which case it is the token following the
left parenthesis (but the add_stop_token call has not been done).
On return, the current token is the token following the closing ")".
If overloaded_function_case is TRUE, this call is scanning the arguments
for a call of an overloaded function, so build an argument summary list
and return a pointer to it in *arg_summary_list.
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
  a_boolean           do_default_promotion;
  a_type_ptr          formal_type;
  a_boolean           is_scanf = FALSE;  /* Initialized to make lint happy. */
  a_constant_ptr      con_ptr;
  char                *fmt_string = NULL;
  a_printf_scan_state pss;
  an_argument_summary_ptr
                      end_arg_summary_list, arg_summary;

  db_enter(4, "scan_call_arguments");
  if (overloaded_function_case) {
    *arg_summary_list = NULL;
    end_arg_summary_list = NULL;
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
  add_stop_token(tok_rparen);

  /* Count the arguments in case varargs is used. */
  arg_ctr = 0;
  argument_head = argument_tail = NULL;
  /* Check for an empty argument list. */
  if (curr_token != tok_rparen) {
    add_stop_token(tok_comma);
    /* Scan a comma-separated list of arguments. */
    do {
      /* Scan an argument expression.  Note that it is not converted to an
         rvalue yet. */
      scan_expr(&argument_operand, PREC_LOWEST, expression_kind,
                EOPT_DISALLOW_COMMA_OPERATOR);
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
        /* Add an entry to the arg summary list. */
        arg_summary = alloc_argument_summary();
        copy_operand(&argument_operand, &arg_summary->operand);
        if (*arg_summary_list == NULL) {
          *arg_summary_list = arg_summary;
        } else {
          end_arg_summary_list->next = arg_summary;
        }  /* if */
        end_arg_summary_list = arg_summary;
      } else {
        /* Do the argument conversion or promotion. */
        if (do_default_promotion) {
	  /* Either an ellipsis was encountered or this is an old-style
             argument list; do the default argument promotion. */
          arg_default_promote_operand(&argument_operand, expression_kind);
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
                                ec_incompatible_param, expression_kind);
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
    } else if (fmt_string != NULL) {
      /* For a printf- or scanf-like function, check that all the formatting
         specifiers were used. */
      if (next_printf_scanf_arg_type(is_scanf, &fmt_string, &pss) != NULL) {
        /* There are no more arguments, but the format string has more
           formatting specifiers. */
        warning(ec_too_few_printf_args);
      }  /* if */
    }  /* if */
  } else {
    /* Old-style parameter list. */
    if ((varargs_count == NOT_LINT_VARARGS && curr_param_type != NULL) ||
        arg_ctr < varargs_count) {
      /* Warning: too few actual arguments. */
      warning(ec_too_few_arguments);
    }  /* if */
  }  /* if */

  /* Check for the closing paren. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
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
                                            an_error_code      err_code,
                                            an_expression_kind expression_kind)
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

  add_stop_token(tok_rparen);
  /* Since the syntax has an expression-list even in the single-expression
     case, a top-level comma is not allowed. */
  scan_expr(&result, PREC_LOWEST, expression_kind,
            EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, dest_type, expression_kind, err_code);
   /* Check for the required closing parenthesis. */
  check_closing_paren_after_expr_list();
  remove_stop_token(tok_rparen);
  expr = make_node_from_operand(&result);
  return expr;
}  /* scan_parenthesized_initializer_expression */


void scan_ctor_arguments(a_symbol_ptr     constructor_sym,
                         an_expr_node_ptr *arg_expr_list,
                         a_routine_ptr    *conversion_routine)
/*
Scan the argument list for a C++ constructor call.  The current token is
the one right after the opening parenthesis of the argument list.  The
constructor symbol (possibly overloaded) is constructor_sym.  (If
constructor_sym is NULL, there was an error previously, and the constructor
is not known.)  Scan the arguments and the closing parenthesis, and return
the argument list in *arg_expr_list and a pointer to the proper constructor
routine in *conversion_routine.  If the proper constructor cannot be
determined, return NULL.  This routine may be called only in C++ mode.
It's used for paren-enclosed initializers for classes that have
constructors, as in

  class A {...};
  A x(1, 2, 3);

The caller need not add the right parenthesis to the stop tokens set, or
remove it later, as this routine takes care of that.
*/
{
  a_boolean         overloaded_function_case = FALSE;
  a_type_ptr        routine_type;
  a_source_position start_position;
  an_argument_summary_ptr
                    arg_summary_list;
  an_expression_kind
                    expression_kind = (an_expression_kind)ek_normal;

  db_enter(4, "scan_ctor_arguments");
  *conversion_routine = NULL;
  start_position = pos_curr_token;
  if (constructor_sym == NULL) {
    /* There was a previous error. */
    routine_type = NULL;
  } else if (constructor_sym->kind == (a_symbol_kind)sk_member_function) {
    /* Constructor is not overloaded.  In this case, the argument types
       can be checked as the argument list is scanned. */
    routine_type = routine_symbol_type(constructor_sym);
  } else {
#if CHECKING
    if (constructor_sym->kind != (a_symbol_kind)sk_overloaded_function) {
      internal_error("scan_ctor_arguments: sym not function");
    }  /* if */
#endif
    /* Constructor is overloaded. */
    overloaded_function_case = TRUE;
    routine_type = NULL;
  }  /* if */
  /* Scan the arguments. */
  scan_call_arguments(routine_type, expression_kind,
                      /*already_after_left_paren=*/TRUE, arg_expr_list,
                      overloaded_function_case, &arg_summary_list);
  if (overloaded_function_case) {
    /* The constructors are overloaded.  Select the proper one. */
    constructor_sym = select_overloaded_function(constructor_sym,
                                                 /*have_selector=*/TRUE,
                                                 (an_operand *)NULL,
                                                 /*is_qualified_name=*/FALSE,
                                                 arg_summary_list,
                                                 expression_kind,
                                                 ec_no_matching_constructor,
                                                 ec_ambiguous_constructor,
                                                 &start_position,
                                                 (an_operand *)NULL,
                                                 arg_expr_list);
  }  /* if */
  if (constructor_sym != NULL) {
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(constructor_sym);
    *conversion_routine = constructor_sym->variant.routine;
  }  /* if */
  db_exit();
}  /* scan_ctor_arguments */


static void cast_pointer_for_field_selection(
                                   an_operand         *operand_1,
                                   a_type_ptr         *class_struct_union_type,
                                   a_boolean          *is_arrow_operator,
                                   a_symbol_ptr       member_sym,
                                   an_expression_kind expression_kind,
                                   an_error_code      err_code,
                                   a_source_position  *err_pos)
/*
Adjust the left operand of a "->" or "." operation, if necessary, to make
it point to a class/struct/union of the type containing the indicated member.
This is significant in C++, where the member may be in a base class of the
left-operand class, and downward casts are needed.  operand_1 is the left
operand; *class_struct_union_type is its type (which is updated if casts
are added); *is_arrow_operator is TRUE for "->", FALSE for "." (it will
be set to TRUE on return if the operation is normalized into "->" form);
member_sym points to the member symbol (possibly a projection symbol).
expression_kind is the current expression kind.  It is possible that the
member symbol is not related to the left operand class; in that case,
issue the error err_code at position *err_pos (the member symbol position).
*/
{
  a_type_ptr       desired_class = member_sym->class_of_which_a_member;
  a_base_class_ptr bcp;

  /* Drop any typedefs on the class type. */
  *class_struct_union_type = skip_typerefs(*class_struct_union_type);
  /* Do nothing if the type is already okay (which it almost always
     will be). */
  if (*class_struct_union_type != desired_class) {
    /* Some adjustment is required.  Find out how the classes are
       related to one another. */
    bcp = find_base_class_of(*class_struct_union_type, desired_class);
    if (bcp != NULL) {
      /* Cast the left operand to the proper type. */
      base_class_cast_operand(operand_1, bcp, is_arrow_operator,
                              /*check_cast_access=*/TRUE, expression_kind);
      *class_struct_union_type = bcp->type;
    } else {
      /* The classes are not related.  Error. */
      pos_error(err_code, err_pos);
      conv_to_error_operand(operand_1);
    }  /* if */
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
                            /*check_cast_access=*/FALSE, expression_kind);
    *class_struct_union_type = bcp->type;
  }  /* if */
}  /* cast_pointer_for_field_selection */


static a_boolean variable_this_exists(a_variable_ptr *this_var)
/*
Return TRUE if there is a currently-visible "this" variable.  If there is,
also set *this_var to point to the variable entry for it.  This routine
is called only in C++ mode.
*/
{
  a_boolean   this_exists;
  a_scope_ptr il_scope;

  *this_var = NULL;
  if (depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    /* We're not inside a function. */
    this_exists = FALSE;
  } else {
    il_scope = scope_stack[depth_innermost_function_scope].il_scope;
#if CHECKING
    if (il_scope == NULL) {
      internal_error("variable_this_exists: NULL IL scope for function");
    }  /* if */
#endif /* CHECKING */
    *this_var = il_scope->variant.routine.this_param_variable;
    this_exists = (*this_var != NULL);
  }  /* if */
  return this_exists;
}  /* variable_this_exists */


static void make_this_pointer_operand(a_symbol_ptr       member_sym,
                                      a_source_position  *position,
                                      an_operand         *result,
                                      an_expression_kind expression_kind)
/*
Make an operand for the "this" pointer of a C++ nonstatic member function.
The operand made is an rvalue for the value of the pointer, with the
source position given by *position.  If we are not currently in a
nonstatic member function, issue an error and return an error operand.
Also check that member_sym is a member of the class "this" points to
(member_sym may be a projection symbol); if it isn't, issue an error and
return an error operand.  If it is, adjust the "this" pointer so it
points to the class of which member_sym is a direct member.
expression_kind indicates the current expression kind.  This routine is
called only in C++ mode.
*/
{
  a_variable_ptr this_var;
  a_type_ptr     class_struct_union_type;
  a_boolean      is_arrow_operator = TRUE;

  if (is_const_expr_kind(expression_kind)) {
    /* Nonstatic members are not allowed in constant expressions. */
    error_and_make_error_operand(ec_expr_not_constant, result);
  } else if (!variable_this_exists(&this_var)) {
    /* We're not inside a function, or the function does not have a "this"
       variable. */
    error_and_make_error_operand(ec_member_ref_requires_object, result);
  } else {
    /* Make an operand for the value of the "this" pointer. */
    make_rvalue_variable_operand(this_var, result);
    class_struct_union_type = type_pointed_to(this_var->type);
    /* Adjust the "this" pointer to point to the right class if it's
       pointing to a derived class of the class of which the symbol
       is a member. */
    cast_pointer_for_field_selection(result, &class_struct_union_type,
                                     &is_arrow_operator,
                                     member_sym, expression_kind,
                                     ec_member_ref_requires_object,
                                     position);
  }  /* if */
}  /* make_this_pointer_operand */


static void scan_function_call(an_operand         *operand,
                               an_operand         *bound_function_selector,
			       an_operand         *result,
			       an_expression_kind expression_kind)
/*
Scan a function call.  The function to be called is given by *operand
(modified by *bound_function_selector if the function is bound).
expression_kind indicates the kind of the current expression.  On
return, *result is set to an operand for the entire call.
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
  a_source_position call_position;
  an_argument_summary
                    this_match_summary;
  an_argument_summary_ptr
                    arg_summary_list;

  db_enter(4, "scan_function_call");

  call_position = operand->position;
  if (is_const_expr_kind(expression_kind)) {
    /* Routine calls not allowed in constant expressions. */
    error_in_operand(ec_bad_constant_function_call, operand);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand->type) &&
        (member_function_symbol =
                           opname_member_function_symbol(
                                      (an_opname_kind)onk_function_call,
                                      skip_typerefs(operand->type))) != NULL) {
      /* There is a C++ function call operator function that overloads function
         calls for the class of the left operand.  The operand becomes
         the selector object, and the function call operator routine
         becomes the operand. */
      copy_operand(operand, bound_function_selector);
      conv_operand_to_object_pointer(bound_function_selector, expression_kind);
      make_indefinite_function_operand(member_function_symbol,
                                       /*is_qualified_name=*/FALSE, operand);
      bind_member_function_operand_to_selector(operand,
                                               bound_function_selector);
    }  /* if */
    do_operand_transformations(operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                               expression_kind);
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
                                       operand->xref_entries_list,
                                       operand);
      conv_function_designator_to_ptr_to_function(operand, expression_kind);
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
      conv_lvalue_to_rvalue(operand, expression_kind);
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
      }  /* if */
    }  /* if */
    if (routine_type != NULL) {
      /* Check the function return type. */
      check_return_type(operand, routine_type);
    }  /* if */
    if (expression_kind != (an_expression_kind)ek_not_evaluated) {
      /* Change the kind in the cross-reference entries to reference. */
      /* This changes the address-taken entry for the function back to
         a simple reference. */
      change_xref_kinds(operand->xref_entries_list, srk_reference);
    }  /* if */
  }  /* if */

  /* Scan the arguments of the call. */
  scan_call_arguments(routine_type, expression_kind,
                      /*already_after_left_paren=*/FALSE, &argument_list,
                      overloaded_function_case, &arg_summary_list);

  if (overloaded_function_case) {
    /* Choose the proper function out of a set of overloaded functions based
       on the argument types. */
    function_symbol = select_overloaded_function(
                                            overloaded_function_symbol,
                                            (a_boolean)operand->bound_function,
                                            bound_function_selector,
                                         (a_boolean)operand->is_qualified_name,
                                            arg_summary_list,
                                            expression_kind,
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
  } else {
    /* Non-overloaded function case. */
    if (operand->bound_function) {
      /* Non-static member function call. */
      /* Check that the selector pointer is compatible with the "this"
         parameter type.  It isn't, for example, if we are calling a
         non-const-qualified function with a const-qualified object.
         Note that if a base class cast was required, it has already been
         done during the function binding, so the differences at this
         point (other than for error cases) are const/non-const differences. */
      selector_match_with_this_param(bound_function_selector,
                                     /*selector_is_object_pointer=*/TRUE,
                                     routine_type, &this_match_summary);
      if (this_match_summary.match_level != aml_none) {
        /* The types are compatible.  No cast is required; if there's a
           difference between the types, it's just a const/non-const
           difference. */
        /* Issue any needed warning (e.g., anachronism of calling a non-const
           function with a const selector). */
        issue_warning_from_argument_summary(&this_match_summary);
      } else {
        /* Some mismatch (more qualifiers on selector than on "this" parameter
           type). */
        pos_error(ec_unqual_function_with_qual_object,
                  &bound_function_selector->position);
        conv_to_error_operand(bound_function_selector);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Build the call node and an operand for it. */
  assemble_function_call(operand, bound_function_selector, argument_list,
                         result);
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
                               an_operand         *operand_1,
                               a_type_ptr         class_struct_union_type,
                               a_boolean          is_arrow_operator,
                               an_expression_kind expression_kind,
                               a_symbol_ptr       field_sym,
                               a_source_position  *member_position,
                               an_xref_entry_ptr  xep,
                               an_operand         *result)
/*
Construct the result operand for a field selection operation.  The left
operand (the class/struct/union) is given by operand_1.  The type
of the class/struct/union (before C++ downward casts, if any) is given
by class_struct_union_type; it provides the type qualifiers that should
be attached to the result expression.  The operator is "->" if
*is_arrow_operator is TRUE, "." otherwise.  expression_kind indicates
the kind of expression.  field_sym points to the symbol for the
right-side field.  *member_position gives its position.  xep points
to an associated cross-reference entry, or is NULL if cross-reference
information is not being maintained.  The result is placed in *result.
*/
{
  a_field_ptr           field;
  a_type_ptr            result_type;
  a_boolean             rvalue_selection;
  a_boolean             is_bit_field;
  a_type_ptr            selection_type;
  an_expr_operator_kind op;
  a_boolean             did_not_fold;
  an_operand            field_operand;
    
  if (is_error_operand(operand_1)) {
    make_error_operand(result);
  } else {
    field = field_sym->variant.field.ptr;
    /* The result type is set to the type of the field with the union of the
       qualifiers of the field and the qualifiers of the class, struct,
       or union. */
    result_type = type_plus_qualifiers_from_second_type(field->type,
                                                      class_struct_union_type);
    rvalue_selection = (!is_arrow_operator && is_an_rvalue(operand_1));
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
    is_bit_field = (field->bit_size != 0);
    if (rvalue_selection) {
      /* For "rvalue . field", the type of the selection is the same
         as the result type. */
      selection_type = result_type;
      op = is_bit_field ? (an_expr_operator_kind)eok_value_bit_field :
                          (an_expr_operator_kind)eok_value_field;
    } else {
      /* For "lvalue . field" and "rvalue -> field", the type of the
         selection (giving, as it does, the address of the resulting
         lvalue) is pointer-to the field type. */
      selection_type = make_pointer_type(result_type);
      op = is_bit_field ? (an_expr_operator_kind)eok_bit_field :
                          (an_expr_operator_kind)eok_field;
    }  /* if */
    did_not_fold = TRUE;
    if (is_const_expr_kind(expression_kind) &&
        is_constant_operand(operand_1)) {
      /* Fold a field selection relative to a constant address into another
         constant address.  Note that the "rvalue . field" case can't come
         here, since a struct/union rvalue cannot be a constant.  This
         folding could be done even in nonconstant expressions (except 
         not-evaluated ones), but it's clearer to have the field selection in
         the IL (the constant form has only an offset, and loses the field
         name). */
      clear_operand((an_operand_kind)ok_constant, result);
      fold_field_selection(&operand_1->variant.constant, field,
                           selection_type, &result->variant.constant,
                           &did_not_fold);
    }  /* if */
    if (did_not_fold) {
      if (is_const_expr_kind(expression_kind)) {
        /* The operation must fold to a constant in a constant expression. */
        /* The only case where it won't is if the field is a bit field,
           so use that for a clearer error message. */
        pos_error(ec_address_of_bit_field, member_position);
        make_error_operand(result);
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
    /* Preserve the cross-reference entries for the base struct. */
    result->xref_entries_list = operand_1->xref_entries_list;
    if (xep != NULL) {
      /* Add the cross-reference entry for the field to the list of
         entries for the operand. */
      xep->next_operand_ref = result->xref_entries_list;
      result->xref_entries_list = xep;
    }  /* if */
  }  /* if */
}  /* do_field_selection_operation */


static void do_member_function_selection_operation(
                                   an_operand         *operand_1,
                                   a_symbol_ptr       routine_sym,
                                   a_symbol_locator   *locator,
                                   an_xref_entry_ptr  xep,
                                   an_operand         *result,
                                   an_operand         *bound_function_selector)
/*
Generate the operand for a nonstatic member function selection operation.
operand_1 is the left operand of the selection (the object).
routine_sym points to the member function symbol entry (possibly overloaded).
*locator is a locator for the member function symbol (needed to get the
projection symbol for the member and to know if a qualified name was used).
xep points to an associated cross-reference entry, or is NULL if
cross-reference information is not being maintained.  The operand is
constructed in *result, and the bound function selector object (usually,
a copy of operand_1) is placed in *bound_function_selector.  expression_kind
indicates the kind of the current expression.
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
                                     &locator->source_position, xep, result);
  }  /* if */
  copy_operand(operand_1, bound_function_selector);
  bind_member_function_operand_to_selector(result, bound_function_selector);
}  /* do_member_function_selection_operation */


static a_boolean overloaded_function_needs_selector(
                                       a_symbol_ptr overloaded_function_symbol,
                                       a_boolean    *maybe)
/*
overloaded_function_symbol points to an sk_overloaded_function symbol.  Return
TRUE if any specific function under that symbol requires a selector expression,
i.e, is a nonstatic member function.  Return *maybe == TRUE if only some of the
functions require a selector.  This routine is used only in C++ mode.
*/
{
  a_boolean     needs_selector;
  a_routine_ptr routine_ptr;

  *maybe = FALSE;
  if (overloaded_function_symbol->class_of_which_a_member == NULL) {
    /* Not a member function, so a selector expression is never needed. */
    needs_selector = FALSE;
  } else if (overloaded_function_symbol->variant.overloaded_function.
                                                      mixed_static_nonstatic) {
    /* The functions are member functions, and some of the functions are
       static and some nonstatic, so we don't know whether or not we need
       the selector expression.  Therefore we have to keep it. */
    needs_selector = TRUE;
    *maybe = TRUE;
  } else {
    /* All of the member functions are static or all are nonstatic.  Look at
       the first function to see which. */
    routine_ptr = overloaded_function_symbol->variant.overloaded_function.
                                                      symbols->variant.routine;
    if (routine_type_is_nonstatic_member_function(routine_ptr->type)) {
      /* Nonstatic member function. */
      needs_selector = TRUE;
    } else {
      /* Static member function. */
      needs_selector = FALSE;
    }  /* if */
  }  /* if */
  return needs_selector;
}  /* overloaded_function_needs_selector */


static void scan_field_selection_operator
                                 (an_operand         *operand_1,
                                  an_operand         *result,
                                  an_operand         *bound_function_selector,
                                  an_expression_kind expression_kind)
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
  a_boolean             err = FALSE, processed = FALSE;
  an_error_code         err_in_operand_1 = ec_no_error;
  an_xref_entry_ptr     xep;
  a_routine_ptr         routine_ptr;
  a_boolean             is_qualified_name;
  a_source_position     member_position, qualified_member_position;
  a_symbol_header_ptr   class_symbol_header;

  db_enter(4, "scan_field_selection_operator");

  /* Remember if this was an arrow or a dot selector. */
  is_arrow_operator = (curr_token == tok_arrow);

  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Field selection not allowed in preprocessor expression. */
    pos_error(ec_bad_pp_operator, &pos_curr_token);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Field selection not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &pos_curr_token);
    err = TRUE;
  } else {
    /* In C++, the first operand of "->" may be a class object that is
       converted to a class pointer via an operator->() function.  The operator
       is treated as a unary operator (i.e., the field following the "->" is
       not significant at this point).  If the operator function returns a
       class object or reference to class object, look for another
       operator->() function. */
    if (is_arrow_operator && C_dialect == C_dialect_cplusplus) {
      while (is_class_struct_union_type(operand_1->type)) {
        check_for_operator_overloading((an_opname_kind)onk_arrow,
                                       /*unary_operator=*/TRUE,  /* sic */
                                       /*must_be_member_function=*/TRUE,
                                       /*has_predef_meaning=*/TRUE,
                                       operand_1, (an_operand *)NULL,
                                       expression_kind, &operand_1->position,
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
                                     TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                               expression_kind);
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
      if (!is_complete_class_struct_union_type(class_struct_union_type)) {
        /* Not (a pointer to) a complete class, struct, or union. */
        err_in_operand_1 = is_arrow_operator ?
                                  ec_expr_not_ptr_to_struct_or_union :
				  ec_expr_not_struct_or_union;
        /* If the problem is that the class is incomplete, use a different
           error message. */
        if (is_class_struct_union_type(class_struct_union_type)) {
          err_in_operand_1 = is_arrow_operator ?
                                  ec_ptr_to_incomplete_class_type_not_allowed :
  				  ec_incomplete_type_not_allowed;
        }  /* if */
        if (C_dialect == C_dialect_pcc &&
            (is_arrow_operator || is_an_lvalue(operand_1))) {
           /* In pcc mode, for "->", or for "." with an lvalue as the
              left operand, delay issuing the error because one is
              allowed to select a field from a non-struct/union. */
        } else {
          /* In the usual case, issue the error right away. */
          error_in_operand(err_in_operand_1, operand_1);
          err_in_operand_1 = ec_no_error;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  if (is_qualified_name_start() ||  /* Identifier and "::". */
      curr_token == tok_compl ||    /* Destructor name like "~A". */
      curr_token == tok_operator) { /* Operator name like "operator+". */
    /* See if the name following the operator is a C++ qualified name, as
       in "p->A::x". */
    is_qualified_name = get_qualified_name(IDL_NO_OPTIONS);
    /* If the member is something like "A::x", member_position will give
       the position of the "x" and qualified_member_position will give the
       position of the "A". */
    member_position = locator_for_curr_id.source_position;
    qualified_member_position = pos_curr_token;
    /* Further checking beyond the fact that this is an identifier is not
       possible if there was an error in the first operand. */
    if (!is_error_operand(operand_1)) {
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
             left-hand side, or one of its base classes. */
          if (!is_same_class_or_base_class_thereof(class_struct_union_type,
                                                   projection_member_sym->
                                                    class_of_which_a_member)) {
            pos_error(ec_name_not_member_of_class_or_base_classes,
                      &qualified_member_position);
            err = TRUE;
          }  /* if */
        }  /* if */
      } else {
        /* Normal case: not qualified member name. */
        /* Handle destructor names ("~A") and operator names ("operator+"). */
        if (get_destructor_name(&class_symbol_header)) {
          /* The name is a destructor name. */
        } else if (get_opname()) {
          /* The name is an operator name. */
        }  /* if */
        /* Look up this identifier in the scope of the class, struct, or
           union. */
        member_sym = class_qualified_id_lookup(&locator_for_curr_id,
                                               class_struct_union_type,
                                               IDL_NO_OPTIONS);
        /* If the field was not found, in pcc mode, look for any field with
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
            /* Suppress the error on the first operand. */
            err_in_operand_1 = ec_no_error;
            if (is_arrow_operator) {
              /* "->" operator. */
              warning(ec_old_fashioned_ptr_field_selection);
            } else {
              /* "." operator.  Convert the lvalue to an rvalue pointer, then
                 use "->" instead.  Note that the test above has ensured that
                 operand_1 here is an lvalue. */
              warning(ec_old_fashioned_field_selection);
              take_address_of_lvalue(operand_1, expression_kind);
              is_arrow_operator = TRUE;
            }  /* if */
            /* Cast the pointer to a pointer to the proper struct or union. */
            orig_class_struct_union_type = member_sym->class_of_which_a_member;
            class_struct_union_type =
                                   skip_typerefs(orig_class_struct_union_type);
            cast_operand(make_pointer_type(class_struct_union_type),
                         operand_1, expression_kind,
                         /*is_implicit_cast=*/FALSE);
            /* Mark the struct or union type as referenced, since a field
               therein has been referenced. */
            orig_class_struct_union_type->source_corresp.referenced = TRUE;
          }  /* if */
        }  /* if */
        if (member_sym == NULL && err_in_operand_1 == ec_no_error) {
          /* The identifier is not a member of the operand_1 class, struct,
             or union. */
          error(ec_not_a_member);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    (void)get_token();
  } else {
    /* The token was not an identifier; error. */
    (void)required_token(tok_identifier, ec_exp_field_name);
    err = TRUE;
  }  /* if */

  /* Put out the delayed error for operand 1 now if it wasn't suppressed. */
  if (err_in_operand_1 != ec_no_error) {
    error_in_operand(err_in_operand_1, operand_1);
    err_in_operand_1 = ec_no_error;
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  if (err || is_error_operand(operand_1)) {
    /* If the operator is not allowed in this kind of expression, or
       if there was an error in the first operand, make an error operand out
       of the result. */
    make_error_operand(result);
  } else {
    /* Record that the field was referenced, for cross-reference (etc.)
       purposes. */
    /* Don't do this if the symbol is an overloaded function (we don't
       yet know which function is being called). */
    if (member_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      xep = NULL;
    } else {
      xep = xref_entry(member_sym, &pos_curr_token, expression_kind);
    }  /* if */
    /* Do ambiguity and access control checking on the member.  For overloaded
       functions, this checks ambiguity but not access (which can be different
       for each function in the set). */
    check_ambiguity_and_verify_access(&locator_for_curr_id);
    if (is_error_locator(locator_for_curr_id)) {
      /* Some error in ambiguity or access control checking. */
      make_error_operand(result);
    } else {
      projection_member_sym = locator_for_curr_id.specific_symbol;
      /* See what kind of member we have. */
      switch (member_sym->kind) {
        case sk_field:
          /* Normal field selection. */
          /* This operation uses the left-side operand, so cast the
             operand to the type of the member symbol.  The error test in this
             call cannot actually generate an error because the error in
             the qualified name case was checked for above. */
          cast_pointer_for_field_selection(operand_1, &class_struct_union_type,
                                           &is_arrow_operator,
                                           projection_member_sym,
                                           expression_kind, ec_no_error,
                                           &qualified_member_position);
          do_field_selection_operation(operand_1, orig_class_struct_union_type,
                                       is_arrow_operator, expression_kind,
                                       member_sym, &member_position, xep,
                                       result);
          break;
        case sk_static_data_member:
          /* Static data member reference.  The value of the left operand is
             discarded. */
          discard_operand(operand_1);
          make_lvalue_variable_operand(member_sym->variant.variable, result,
                                       xep);
          break;
        case sk_member_function:
          /* Member function (static or non-static). */
          routine_ptr = member_sym->variant.routine;
          if (routine_type_is_nonstatic_member_function(routine_ptr->type)) {
            /* Nonstatic member function. */
            /* Also continue here for an overloaded function. */
nonstatic_member_function:
            /* Such a reference is not allowed in an initializer constant
               expression.  In truth, though, it's almost impossible to get
               such a thing in C++. */
            if (expression_kind == (an_expression_kind)ek_init_constant) {
              error_and_make_error_operand(ec_expr_not_constant, result);
            } else {
              /* The function will require a "this" pointer, so get a pointer
                 (rather than an rvalue) for the first operand. */
              conv_selector_to_object_pointer(operand_1, &is_arrow_operator,
                                              expression_kind);
              /* Cast the selector to the type of the member symbol.  The
                 error test in this call cannot actually generate an error
                 because the error in the qualified name case was checked
                 for above. */
              cast_pointer_for_field_selection(operand_1,
                                               &class_struct_union_type,
                                               &is_arrow_operator,
                                               projection_member_sym,
                                               expression_kind, ec_no_error,
                                               &qualified_member_position);
              /* Make an operand for the function with the selector bound
                 to it. */
              do_member_function_selection_operation(operand_1,
                                                     member_sym,
                                                     &locator_for_curr_id,
                                                     xep,
                                                     result,
                                                     bound_function_selector);
            }  /* if */
          } else {
            /* Static member function.  Discard the left operand. */
            discard_operand(operand_1);
            make_function_designator_operand(member_sym,
                                             is_qualified_name,
                                             &member_position,
                                             xep,
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
          make_constant_operand(member_sym->variant.constant, result);
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


static void scan_ptr_to_member_operator
                                 (an_operand         *operand_1,
                                  an_operand         *result,
                                  an_operand         *bound_function_selector,
                                  an_expression_kind expression_kind)
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

  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Field selection not allowed in preprocessor expression. */
    pos_error(ec_bad_pp_operator, &operator_position);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Field selection not allowed in integral constant expression. */
    pos_error(ec_bad_integral_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_PTR_TO_MEMBER, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (is_arrow_operator &&
        (is_class_struct_union_type(operand_1->type) ||
         is_class_struct_union_type(operand_2.type))) {
      /* Look for C++ operator overloading cases ("->*" only). */
      check_for_operator_overloading((an_opname_kind)onk_arrow_star,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      /* Do implicit operand transformations.  In the ".*" case, keep an
         lvalue if we have one. */
      do_operand_transformations(operand_1,
                                 is_arrow_operator ?
                                     TOPT_NO_OPTIONS :
                                     TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
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
            error_in_operand(is_arrow_operator ?
                                           ec_expr_not_ptr_to_struct_or_union :
                                           ec_expr_not_struct_or_union,
                             operand_1);
            err = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      /* The type of the second operand must be pointer to member. */
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
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
          pos_error(ec_incompatible_operands, &operator_position);
          err = TRUE;
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error. */
        make_error_operand(result);
      } else {
        /* The operands are compatible. */
        /* Get an operand for the address of the object. */
        conv_selector_to_object_pointer(operand_1, &is_arrow_operator,
                                        expression_kind);
        /* Cast the left operand to a base class if necessary.  This does the
           ambiguity and accessibility checking. */
        if (bcp != NULL) {
          base_class_cast_operand(operand_1, bcp, &is_arrow_operator,
                                  /*check_cast_access=*/TRUE, expression_kind);
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


static void scan_postfix_incr_decr(an_operand         *operand,
				   an_operand         *result,
                                   an_expression_kind expression_kind)
/*
Scan the postfix increment ("++") and decrement ("--") operators.  See section
3.3.2.4 of the standard.
*/
{
  an_expr_operator_kind op;
  a_type_ptr            result_type;
  a_boolean             err = FALSE, processed = FALSE;
  an_operand            zero_operand;

  db_enter(4, "scan_postfix_incr_decr");

  if (is_const_expr_kind(expression_kind)) {
    /* Postfix ++/-- not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &pos_curr_token);
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand->type)) {
      /* Look for C++ operator overloading cases. */
      /* Note that postfix ++/-- use a two-argument function to distinguish
         them from the prefix ++/--, which use a one-argument function.
         See ARM 13.4.7.  The second compiler-supplied argument is an
         integer zero. */
      make_integer_constant_operand(&zero_operand, 0L);
      check_for_operator_overloading(opname_kind_for_token[(int)curr_token],
                                     /*unary_operator=*/FALSE,  /* sic! */
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,  /* sic */
                                     operand, &zero_operand,
                                     expression_kind, &operand->position,
                                     result, &processed);
      if (!processed) {
        /* Try the anachronism that allows a one-argument function to
           be used for both prefix and postfix ++/--. */
        check_for_operator_overloading(opname_kind_for_token[(int)curr_token],
                                       /*unary_operator=*/TRUE,
                                       /*must_be_member_function=*/FALSE,
                                       /*has_predef_meaning=*/FALSE,
                                       operand, (an_operand *)NULL,
                                       expression_kind, &operand->position,
                                       result, &processed);
        if (processed) {
          if (!is_error_operand(result)) {
            pos_warning(ec_single_arg_postfix_incr_decr_anachronism,
                        &operand->position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
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
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
      } else if (!check_modifiable_lvalue_operand(operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(operand, expression_kind);
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


static void scan_prefix_incr_decr(an_operand         *result,
				  an_expression_kind expression_kind)
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

  if (is_const_expr_kind(expression_kind)) {
    /* Prefix ++ and -- are not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand.type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     expression_kind, &start_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(&operand,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
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
        }  /* if */
      }  /* if */
      if (err) {
        /* Some error already. */
      } else if (!check_modifiable_lvalue_operand(&operand)) {
        /* Operand is not a modifiable lvalue. */
        err = TRUE;
      } else {
        /* Operand is okay. */
        modifying_lvalue(&operand, expression_kind);
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
      }  /* if */
    }  /* if */
  }  /* if */

  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_prefix_incr_decr */


static void scan_ampersand_operator(an_operand         *result,
                                    an_expression_kind expression_kind)
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

  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Address constants not allowed in preprocessing expressions. */
    pos_error(ec_bad_pp_operator, &start_position);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Address constants not allowed in integral constant expressions. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Advance past the "&". */
  (void)get_token();
  /* Scan the operand.  Do not convert a type of "routine returning type" to
     "pointer to routine returning type".  Likewise do not convert arrays
     to pointers to their first elements. */
  scan_expr(&operand, PREC_PREFIX, expression_kind,
            EOPT_OPERAND_OF_ADDRESS_OF);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand.type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_ampersand,
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     &operand, (an_operand *)NULL,
                                     expression_kind, &start_position,
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
                                 TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION,
                                 expression_kind);
      if (is_an_lvalue(&operand)) {
        if (C_dialect == C_dialect_pcc && is_array_type(operand.type)) {
          /* In pcc mode "&array" is the same as "array" implicitly converted
             to a pointer.  It has type "pointer-to-array-element" rather than
             "pointer to array" as in ANSI. */
          pos_warning(ec_pcc_address_of_array, &start_position);
          conv_array_operand_to_pointer_operand(&operand, expression_kind);
        } else {
          /* Check for taking the address of a register variable, and set the
             address_taken flag if the operand is a variable.  Convert the
             lvalue operand to an rvalue operand for the pointer. */
          take_address_of_lvalue(&operand, expression_kind);
        }  /* if */
        /* Note that the copy preserves xref_entries_list. */
        copy_operand(&operand, result);
      } else if (is_a_function_designator(&operand)) {
        /* "&" of a function designator.  Change it to a pointer to the
           function.  This includes overloaded functions, but not overloaded
           member functions specified by qualified name (see below). */
        conv_function_designator_to_ptr_to_function(&operand, expression_kind);
        /* Note that the copy preserves xref_entries_list. */
        copy_operand(&operand, result);
      } else if (is_sym_for_ptr_to_member_operand(&operand)) {
        /* The operand is a qualified name for a nonstatic class member, so
           the "&" operator returns a pointer-to-member. */
        if (expression_kind != (an_expression_kind)ek_not_evaluated) {
          /* Change the kind in the cross-reference entries to
             address-taken. */
          change_xref_kinds(operand.xref_entries_list, srk_address_taken);
        }  /* if */
        member_proj_sym = operand.variant.symbol;
        member_sym = fundamental_symbol_of(member_proj_sym);
        if (member_sym->kind == (a_symbol_kind)sk_field) {
          /* Pointer to nonstatic data member. */
          if (member_sym->variant.field.ptr->bit_size != 0) {
            /* The field may not be a bit field. */
            error_in_operand(ec_address_of_bit_field, &operand);
            make_error_operand(result);
          } else {
            make_ptr_to_member_constant_operand(member_proj_sym, result);
          }  /* if */
        } else if (member_sym->kind == (a_symbol_kind)sk_member_function) {
          /* Pointer to nonstatic member function. */
          make_ptr_to_member_constant_operand(member_proj_sym, result);
        } else {
#if CHECKING
          if (member_sym->kind != (a_symbol_kind)sk_overloaded_function) {
            internal_error("scan_ampersand_operator: & of bad qualified name");
          }  /* if */
#endif /* CHECKING */
          /* Address of overloaded member function. */
          make_indefinite_function_operand(member_proj_sym,
                                           /*is_qualified_name=*/TRUE,
                                           result);
          conv_function_designator_to_ptr_to_function(result, expression_kind);
        }  /* if */
      } else {
        /* "&" applied to something that is not an lvalue or a function
           designator. */
        if (!is_error_operand(&operand)) {
          error_in_operand(ec_expr_not_an_lvalue_or_function_designator,
                           &operand);
        }  /* if */
        make_error_operand(result);
      }  /* if */
    }  /* if */
  }  /* if */

  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_ampersand_operator */


static void scan_indirection_operator(an_operand         *result,
                                      an_expression_kind expression_kind)
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

  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Address indirection not allowed in preprocessing expressions. */
    pos_error(ec_bad_pp_operator, &start_position);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Address indirection not allowed in integral constant expressions. */
    pos_error(ec_bad_integral_operator, &start_position);
    err = TRUE;
  }  /* if */

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand.type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_star,
                                     /*unary_operator=*/TRUE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand, (an_operand *)NULL,
                                     expression_kind, &start_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(&operand, TOPT_NO_OPTIONS, expression_kind);
      /* Note that there is no check for void or incomplete types here.
           void *p; *p;
         is legal, although *p is not an lvalue. */
      if (check_pointer_operand(&operand, ec_bad_indirection_operand)) {
        operand.type = type_pointed_to(operand.type);
        if (is_function_type(operand.type)) {
          /* This will become a function designator. */
          operand.state = (an_operand_state)os_function_designator;
        } else {
          operand.state = (an_operand_state)os_lvalue;
        }  /* if */
        /* Note that the copy preserves xref_entries_list. */
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


static void scan_arith_prefix_operator(an_operand         *result,
                                       an_expression_kind expression_kind)
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
  a_boolean             did_not_fold;
  a_boolean             do_promotion, processed = FALSE;
  a_constant            result_constant;

  db_enter(4, "scan_arith_prefix_operator");

  save_token = curr_token;
  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

  /* Scan the operand. */
  (void)get_token();
  scan_expr(&operand, PREC_PREFIX, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(operand.type)) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/TRUE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   &operand, (an_operand *)NULL,
                                   expression_kind, &start_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    do_operand_transformations(&operand, TOPT_NO_OPTIONS, expression_kind);
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
      promote_operand(&operand, expression_kind);
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
        if (expression_kind != (an_expression_kind)ek_not_evaluated &&
            is_constant_operand(&operand)) {
          /* Fold the operation if the operand is constant.  In a nonconstant
             context, reduce any error to a warning and leave the operation
             to be done at runtime. */
          unary_operation(op, &operand.variant.constant,
                          result_type, &result_constant,
                          is_const_expr_kind(expression_kind),
                          &did_not_fold, &start_position);
        }  /* if */
        if (did_not_fold) {
          if (is_const_expr_kind(expression_kind)) {
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


/*ARGSUSED*/ /* <-- because "expression_kind" is unused if CHECKING==0. */
static void scan_sizeof_operator(an_operand         *result,
                                 an_expression_kind expression_kind)
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

  db_enter(4, "scan_sizeof_operator");

#if CHECKING
  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Sizeof not possible for preprocessing expressions. */
    internal_error("scan_sizeof_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
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
                         /*real_declarator_allowed=*/FALSE)) {
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
    add_stop_token(tok_rparen);
    type_name(&sizeof_type);
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
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
    scan_expr(&operand, PREC_PREFIX, (an_expression_kind)ek_not_evaluated,
              local_options);
    /* Do not convert a type of "routine returning type" to "pointer to
       routine returning type".  See section 3.2.2.1 in the C standard. */
    do_operand_transformations(&operand,
                               TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION |
                               TOPT_SUPPRESS_ARRAY_TO_POINTER_CONVERSION |
                               TOPT_SUPPRESS_FUNCTION_TO_POINTER_CONVERSION,
                               (an_expression_kind)ek_not_evaluated);
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
    clear_constant(&constant, (a_constant_repr_kind)ck_integer);
    constant.type = integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND);
    constant.variant.integer_value = sizeof_type->size;
  }  /* if */
  make_constant_operand(&constant, result);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_sizeof_operator */


static void scan_alignof_operator(an_operand *result)
/*
Scan the __ALIGNOF__ operator.  This is an extension that is similar
to sizeof, but returns the alignment requirement rather than the size.

Syntax:
	__ALIGNOF__ ( type-name )

The parentheses are required, unlike for sizeof.  Fewer error checks
are done.  A warning about the use of this nonstandard feature would
be inappropriate, because the feature is probably used to implement
<stdarg.h>, a standard feature.
*/
{
  a_source_position start_position;
  a_constant        constant;
  a_type_ptr        alignof_type;

  db_enter(4, "scan_alignof_operator");

  /* Save the position of the __ALIGNOF__ keyword. */
  copy_source_position(pos_curr_token, start_position);
  /* Check for and pass over the left parenthesis. */
  (void)get_token();
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the type name. */
  type_name(&alignof_type);
  alignof_type = skip_typerefs(alignof_type);
  /* The result of __ALIGNOF__ is an integer indicating the alignment of
     the operand, of type size_t. */
  if (is_error_type(alignof_type)) {
    set_error_constant(&constant);
  } else {
    clear_constant(&constant, (a_constant_repr_kind)ck_integer);
    constant.type = integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND);
    constant.variant.integer_value = alignof_type->alignment;
  }  /* if */
  make_constant_operand(&constant, result);
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

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
  a_source_position start_position;
  a_constant        constant;
  a_boolean         err;

  db_enter(4, "scan_intaddr_operator");

  /* Save the position of the __INTADDR__ keyword. */
  copy_source_position(pos_curr_token, start_position);
  /* Check for and pass over the left parenthesis. */
  (void)get_token();
  (void)required_token(tok_lparen, ec_exp_lparen);
  add_stop_token(tok_rparen);
  /* Scan the address expression. */
  scan_expr(result, PREC_LOWEST, (an_expression_kind)ek_init_constant,
            EOPT_NO_OPTIONS);
  do_operand_transformations(result, TOPT_NO_OPTIONS,
                             (an_expression_kind)ek_init_constant);
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
                 result, (an_expression_kind)ek_init_constant,
                 /*is_implicit_cast=*/TRUE);
  }  /* if */
  /* Check for and pass over the right parenthesis. */
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_intaddr_operator */


static a_dynamic_init_ptr add_array_nonconstant_aggregate_init(
                                         a_dynamic_init_ptr dip,
                                         a_targ_size_t      number_of_elements)
/*
Change the indicated dynamic initialization into a dynamic initialization
for each member of an array of classes.  number_of_elements is the number
of elements in the array, or 0 if the number of elements is variable (and
known only at runtime).  Return a pointer to the dynamic init entry for
the entire array.
*/
{
  a_constant_ptr dyn_init_con, init_repeat_con, aggr_con;

  /* The IL structure is
       dynamic init (ck_nonconstant_aggregate) ->
         constant (ck_aggregate) ->
           constant (ck_init_repeat) ->
             constant (ck_dynamic_init) ->
               original dynamic init (ck_constructor)
  */
  dyn_init_con = alloc_constant((a_constant_repr_kind)ck_dynamic_init);
  dyn_init_con->variant.dynamic_init = dip;
  init_repeat_con = alloc_constant((a_constant_repr_kind)ck_init_repeat);
  init_repeat_con->variant.init_repeat.constant = dyn_init_con;
  init_repeat_con->variant.init_repeat.count = number_of_elements;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = init_repeat_con;
  aggr_con->variant.aggregate.last_constant = init_repeat_con;
  dip = alloc_dynamic_init((a_dynamic_init_kind)dik_nonconstant_aggregate);
  dip->variant.aggregate.aggr_const = aggr_con;
  dip->variant.aggregate.dynamic_init_list =
                                            dyn_init_con->variant.dynamic_init;
  return dip;
}  /* add_array_nonconstant_aggregate_init */


static void scan_new_operator(an_operand         *result,
                              an_expression_kind expression_kind)
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
new-type-name called from this routine.  Note that both forms of type
specification allow a variable-sized array as the top type.
*/
{
  a_source_position start_position, new_position, type_position;
  a_source_position placement_position;
  a_type_ptr        new_type, base_new_type, element_type, ptr_new_type;
  an_expr_node_ptr  new_array_dimension, sizeof_node, function_node;
  an_operand        sizeof_operand, function_operand;
  a_boolean         use_global_new = FALSE;
  a_symbol_ptr      operator_new_symbol, ctor_sym;
  a_routine_ptr     new_routine, ctor_routine;
  a_boolean         needs_initialization, trapped_left_paren;
  a_dynamic_init_ptr
                    dip;
  an_expr_node_ptr  arg_expr_list;
  an_expr_node_ptr  init_node, init_val_node;
  a_constant        sizeof_constant;
  an_argument_summary_ptr
                    arg_summary_list, sizeof_arg_summary;
  an_expr_node_ptr  dummy;
  a_boolean         array_new;
  a_targ_size_t     effective_num_of_elements;

  db_enter(4, "scan_new_operator");

  /* Save the position of the start. */
  copy_source_position(pos_curr_token, start_position);
#if CHECKING
  if (expression_kind == (an_expression_kind)ek_pp) {
    /* New not possible for preprocessing expressions. */
    internal_error("scan_new_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
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
  arg_summary_list = NULL;
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
                         /*real_declarator_allowed=*/FALSE)) {
      /* This is the type name. */
      trapped_left_paren = TRUE;
    } else {
      /* This is the placement expression list. */
      if (curr_token == tok_rparen) {
        /* An empty list is not allowed. */
        error(ec_exp_primary_expr);
        (void)get_token();
      } else {
        /* Scan the expression list as an argument list for which we do not yet
           know the function.  The argument values are returned in a list
           headed by arg_summary_list. */
        scan_call_arguments((a_type_ptr)NULL, expression_kind,
                            /*already_after_left_paren=*/TRUE,
                            &dummy, /*overloaded_function_case=*/TRUE,
                            &arg_summary_list);
      }  /* if */
    }  /* if */
  }  /* if */
  copy_source_position(pos_curr_token, type_position);
  /* Scan the new-type-name or ( type-name ). */
  new_type_name(trapped_left_paren, &new_type, &new_array_dimension);
  new_type = skip_typerefs(new_type);
  /* The operand of a new must be an object type. */
  if (!is_object_type(new_type) &&
      (!is_array_type(new_type) || new_array_dimension == NULL)) {
    /* Invalid type. */
    if (is_error_type(new_type)) {
      /* Error already issued. */
    } else if (is_incomplete_type(new_type)) {
      pos_error(ec_incomplete_type_not_allowed, &type_position);
    } else {
      pos_error(ec_type_must_be_object_type, &type_position);
    }  /* if */
    make_error_operand(result);
    base_new_type = ptr_new_type = new_type = error_type();
  } else {
    /* Valid type. */
    /* Determine the type of pointer returned from "new". */
    base_new_type = new_type;
    array_new = FALSE;
    if (is_array_type(base_new_type)) {
      /* A "new" of an array returns a pointer to the initial element.
        Note that this is only done for one level, e.g., new int [i][10]
        returns int (*)[10] not int * (ARM 5.3.3). */
      base_new_type = array_element_type(base_new_type);
      array_new = TRUE;
    }  /* if */
    ptr_new_type = make_pointer_type(base_new_type);
    /* Compute the allocation size in bytes. */
    if (new_array_dimension != NULL) {
      /* The type is a variable-dimension array, as in
           new char[i+1]
         The amount to allocate is the size of the array element times
         the expression giving the number of elements. */
      /* Cast the dimension expression to size_t (it's already an integral
         type). */
      cast_node(&new_array_dimension,
                integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND),
                /*is_implicit_cast=*/TRUE, &error_position);
      element_type = array_element_type(new_type);
      element_type = skip_typerefs(element_type);
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
      set_integer_constant(&sizeof_constant, (long)new_type->size,
                           (an_integer_kind)TARG_SIZE_T_INT_KIND);
      make_constant_operand(&sizeof_constant, &sizeof_operand);
    }  /* if */
    /* Add the sizeof operand, in argument summary form, to the front of the
       list of expressions (if any) from the "placement" option.  This gives
       the full set of arguments for the "new" function call. */
    sizeof_arg_summary = alloc_argument_summary();
    copy_operand(&sizeof_operand, &sizeof_arg_summary->operand);
    sizeof_arg_summary->next = arg_summary_list;
    arg_summary_list = sizeof_arg_summary;
    /* Select the proper "new" routine.  If the type is a class type and
       the class has a "new" operator, use it.  However, if "::" preceded
       the keyword "new", always use the global ::new.  Also note that
       since we test new_type instead of base_new_type, we will use
       the global ::new for arrays of classes, as we should. */
    operator_new_symbol = NULL;
    if (is_class_struct_union_type(new_type) && !use_global_new) {
      operator_new_symbol = opname_member_function_symbol(
                                                       (an_opname_kind)onk_new,
                                                       new_type);
    }  /* if */
    if (operator_new_symbol == NULL) {
      /* Use the global operator "new". */
      operator_new_symbol = opname_function_symbol((an_opname_kind)onk_new);
    }  /* if */
    /* Select the proper "new" function if there are several; even if there
       is only one, check the argument types. */
    operator_new_symbol = select_overloaded_function(
                                              operator_new_symbol,
                                              /*have_selector=*/FALSE,
                                              (an_operand *)NULL,
                                              /*is_qualified_name=*/FALSE,
                                              arg_summary_list,
                                              expression_kind,
                                              ec_no_matching_new_function,
                                              ec_ambiguous_overloaded_function,
                                              &placement_position,
                                              &function_operand,
                                              &arg_expr_list);
    if (operator_new_symbol == NULL) {
      /* No "new" function matches, or several do. */
      make_error_operand(result);
    } else {
      /* Make an expression for the address of the function. */
      new_routine = operator_new_symbol->variant.routine;
      function_node = make_node_from_operand(&function_operand);
      /* Make a call of the new routine with the size argument. */
      function_node->next = arg_expr_list;
      make_function_call(function_node, new_routine->type,
                         (a_boolean)new_routine->is_virtual,
                         /*new_or_delete_call_for_array=*/array_new,
                         result);
      /* Cast the pointer returned by "new" to the right type. */
      cast_operand(ptr_new_type, result, expression_kind,
                   /*is_implicit_cast=*/TRUE);
#if ASSIGNMENT_TO_THIS_ALLOWED
      if (is_class_struct_union_type(new_type)) {
        /* Determine and remember the default operator new() routine for
           the class. */
        set_class_assoc_operator_new_routine(skip_typerefs(new_type));
      }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
    }  /* if */
  }  /* if */
  /* See if the object has or needs initialization. */
  needs_initialization = FALSE;
  ctor_routine = NULL;
  if (array_new) {
    /* Array new.  Determine the effective number of elements. */
    if (new_array_dimension != NULL) {
      /* Variable-length array; count is deferred to runtime. */
      effective_num_of_elements = 0;
    } else {
      effective_num_of_elements = new_type->variant.array.number_of_elements;
    }  /* if */
    while (is_array_type(base_new_type)) {
      /* For multi-dimensional arrays: even though only one level of array is
         dropped to determine the pointer type and to do allocation, all levels
         must be dropped to get the real base type to do initialization.  In
         particular, we want to know if the underlying type of a
         multi-dimensional array is a class, so we can know whether or not
         to call a constructor. */
      base_new_type = skip_typerefs(base_new_type);
      effective_num_of_elements *=
                               base_new_type->variant.array.number_of_elements;
      base_new_type = array_element_type(base_new_type);
    }  /* while */
  }  /* if */
  /* Set ctor_sym non-NULL if the type is a class that has a constructor
     or an array with elements of such a class. */
  ctor_sym = NULL;
  if (is_class_struct_union_type(base_new_type)) {
    ctor_sym = symbol_supplement_for_class(base_new_type)->constructor;
  }  /* if */
  if (ctor_sym != NULL) {
    /* Class with a constructor.  Initialization is required. */
    if (curr_token == tok_lparen) {
      /* There is a new-initializer.  It's treated as a constructor call. */
      (void)get_token();
      if (array_new) {
        /* No initializer may be specified for an array type. */
        error(ec_initializer_not_allowed_on_array_new);
      }  /* if */
      /* No need to add tok_rparen to the stop tokens set: it's done by
         scan_ctor_arguments. */
      /* Scan the constructor arguments. */
      scan_ctor_arguments(ctor_sym, &arg_expr_list, &ctor_routine);
      if (array_new) arg_expr_list = NULL;
    } else {
      /* There is no new-initializer, so a default constructor must exist. */
      ctor_routine = select_default_constructor(base_new_type, &type_position);
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
    /* Note that error cases come here too. */
    if (curr_token == tok_lparen) {
      /* The new-initializer is present. */
      (void)get_token();
      if (array_new) {
        /* No initializer may be specified for an array type. */
        error(ec_initializer_not_allowed_on_array_new);
        ptr_new_type = new_type = base_new_type = error_type();
      }  /* if */
      if (curr_token != tok_rparen) {
        /* The new-initializer is not empty.  Scan it. */
        init_val_node = scan_parenthesized_initializer_expression(
                                                       new_type,
                                                       ec_bad_initializer_type,
                                                       expression_kind);
        needs_initialization = TRUE;
      } else {
        /* The initializer is empty, i.e., "()". */
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  if (needs_initialization && !is_error_type(new_type)) {
    /* The allocated space must be initialized.  An enk_new_init is used.
       It only does the initialization if the address returned from the
       new routine is non-NULL. */
    init_node = alloc_expr_node((an_expr_node_kind)enk_new_init);
    /* The expression for the enk_new_init is the function call to the
       "new" routine. */
    init_node->variant.init.expr = make_node_from_operand(result);
    init_node->type = result->type;
    /* Make the dynamic init entry for the initialization. */
    if (ctor_routine != NULL) {
      /* Constructor call. */
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_constructor);
      dip->variant.constructor.routine = ctor_routine;
      dip->variant.constructor.args = arg_expr_list;
      if (array_new) {
        /* The entity is an array whose elements have a class type that
           has a default constructor.  Use a dik_nonconstant_aggregate
           initialization. */
        dip = add_array_nonconstant_aggregate_init(dip,
                                                   effective_num_of_elements);
      }  /* if */
    } else {
      /* Expression as initial value. */
      dip = alloc_dynamic_init((a_dynamic_init_kind)dik_expression);
      dip->variant.expression = init_val_node;
    }  /* if */
    init_node->variant.init.dynamic_init = dip;
    /* Make an operand for the final result. */
    make_expression_operand(init_node, ptr_new_type, result);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_new_operator */


static void scan_delete_operator(an_operand         *result,
                                 an_expression_kind expression_kind)
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
  an_expr_node_ptr   function_node;
  a_boolean          use_global_delete = FALSE, is_constant, array_delete;
  a_symbol_ptr       operator_delete_symbol;
  a_routine_ptr      delete_routine, dtor_routine;
  an_expr_node_ptr   ptr_node;
  an_operand         operand;
  a_constant         constant;
  an_expr_node_ptr   expr, init_node;
  a_symbol_locator   locator_for_delete;
  a_dynamic_init_ptr dip;

  db_enter(4, "scan_delete_operator");

  /* Save the position of the start. */
  copy_source_position(pos_curr_token, start_position);
#if CHECKING
  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Delete not possible for preprocessing expressions. */
    internal_error("scan_delete_operator: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
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
    add_stop_token(tok_rbracket);
    if (curr_token != tok_rbracket) {
      /* Anachronism -- there's an expression between the brackets, presumably
         indicating the number of elements in the array. */
      warning(ec_delete_count_anachronism);
      scan_new_array_dimension_expression(&is_constant, &expr, &constant);
      /* The expression is ignored. */
    }  /* if */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    remove_stop_token(tok_rbracket);
  }  /* if */
  /* Scan the pointer expression. */
  scan_expr(&operand, PREC_PREFIX, expression_kind, EOPT_NO_OPTIONS);
  do_operand_transformations(&operand, TOPT_NO_OPTIONS, expression_kind);
  /* The operand of a delete must be a pointer. */
  if (!check_pointer_operand(&operand, ec_expr_not_pointer)) {
    make_error_operand(result);
  } else {
    ptr_delete_type = operand.type;
    delete_type = type_pointed_to(ptr_delete_type);
    if (is_const_qualified_type(delete_type)) {
      /* The type pointed to may not be const-qualified. */
      error_in_operand(ec_delete_of_const_pointer, &operand);
      make_error_operand(result);
    } else {
      /* Valid type. */
      ptr_node = make_node_from_operand(&operand);
      base_delete_type = delete_type;
      /* Get the underlying type for any array type. */
      while (is_array_type(base_delete_type)) {
        base_delete_type = array_element_type(base_delete_type);
      }  /* if */
      /* See if the object needs destruction. */
      dtor_routine = NULL;
      if (is_class_struct_union_type(base_delete_type)) {
        dtor_routine = select_destructor(base_delete_type);
        if (dtor_routine != NULL) {
          /* Class with destructor.  Destruction is required.  Use an
             enk_new_init node to do the destruction. */
          init_node = alloc_expr_node((an_expr_node_kind)enk_new_init);
          init_node->variant.init.expr = ptr_node;
          init_node->type = ptr_node->type;
          dip = alloc_dynamic_init((a_dynamic_init_kind)dik_none);
          dip->destructor = dtor_routine;
          if (array_delete) {
            /* For a delete of an array of classes, generate a dynamic init
               that replicates the destructor call for the whole array. */
            dip = add_array_nonconstant_aggregate_init(dip, (a_targ_size_t)0);
          }  /* if */
          init_node->variant.init.dynamic_init = dip;
          ptr_node = init_node;
        }  /* if */
      }  /* if */
      /* Put together the call of the delete routine. */
      /* Cast the pointer to void *. */
      cast_node(&ptr_node, make_pointer_type(void_type()),
                /*is_implicit_cast=*/TRUE, &operand.position);
      /* Select the proper "delete" routine.  If the type is a class type and
         the class has a "delete" operator, use it.  However, if "::" preceded
         the keyword "delete", always use the global ::delete.  Also use the
         global ::delete for arrays of class objects. */
      operator_delete_symbol = NULL;
      if (is_class_struct_union_type(delete_type) && !use_global_delete &&
          !array_delete) {
        operator_delete_symbol = opname_member_function_symbol(
                                                    (an_opname_kind)onk_delete,
                                                    delete_type);
        if (operator_delete_symbol != NULL) {
          make_locator_for_symbol(operator_delete_symbol, &locator_for_delete);
          locator_for_delete.source_position = delete_position;
          check_ambiguity_and_verify_access(&locator_for_delete);
          operator_delete_symbol =
                                 fundamental_symbol_of(operator_delete_symbol);
        }  /* if */
      }  /* if */
      if (operator_delete_symbol == NULL) {
        /* Use the global operator "delete". */
        operator_delete_symbol =
                            opname_function_symbol((an_opname_kind)onk_delete);
      }  /* if */
      /* Make an expression for the address of the function. */
      delete_routine = operator_delete_symbol->variant.routine;
      /* Mark the routine referenced. */
      mark_referenced(operator_delete_symbol, &delete_position);
      function_node = function_addr_expr(delete_routine);
      /* Make a call of the delete routine with the pointer argument. */
      function_node->next = ptr_node;
      make_function_call(function_node, delete_routine->type,
                         (a_boolean)delete_routine->is_virtual,
                         /*new_or_delete_call_for_array=*/array_delete,
                         result);
#if ASSIGNMENT_TO_THIS_ALLOWED
      if (is_class_struct_union_type(delete_type)) {
        /* Determine and remember the default operator delete() routine for
           the class. */
        set_class_assoc_operator_delete_routine(skip_typerefs(delete_type));
      }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
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
                                   an_expression_kind       expression_kind,
                                   a_boolean                *cast_to_reference,
                                   a_boolean                *int_to_ptr_case,
                                   a_boolean                *cast_to_func_ptr,
                                   a_local_expr_options_set local_options)
/*
Do a first check on the destination type of a cast to see if it is legal.
That is, do a check that the type is legal as the destination type of
a cast without regard to the source type.  Return TRUE if there is an
error.  *p_type_cast_to is the destination type of the cast; expression_kind
is the kind of the current expression.  On return, *cast_to_reference is
TRUE if the cast is to a reference type (*p_type_cast_to will be adjusted
to the corresponding pointer type in that case), *int_to_ptr_case is TRUE
if the destination type is a pointer type in a context that requires
that the source type be an integral type, and *cast_to_func_ptr is TRUE
if the cast is to a pointer-to-function type in C++.  This routine is called
for both C-style casts and C++ functional-notation type conversions.
*/
{
  a_boolean  err = FALSE;
  a_type_ptr type_cast_to = *p_type_cast_to;

  *int_to_ptr_case = FALSE;
  *cast_to_reference = FALSE;
  *cast_to_func_ptr = FALSE;
  /* Check the type to see if it's permissible. */
  if (is_error_type(type_cast_to)) {
    err = TRUE;
  } else if (is_incomplete_type(type_cast_to) && !is_void_type(type_cast_to)) {
    /* This check catches incomplete enum types.  Except for being
       incomplete, they look like integral types. */
    error(ec_incomplete_type_not_allowed);
    err = TRUE;
  } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
    /* Only casts to integral types are permitted in integral constant
       expressions.  When the cast is the immediate operand of another
       cast, allow integer --> pointer as an extension. */
    if (!is_integral_type(type_cast_to)) {
      if ((local_options & EOPT_OPERAND_OF_CAST) && 
          is_pointer_type(type_cast_to)) {
        *int_to_ptr_case = TRUE;
        if (strict_ansi_mode) warning(ec_cast_not_integral);
      } else {
        error(ec_cast_not_integral);
        err = TRUE;
      }  /* if */
    }  /* if */
  } else if (expression_kind == (an_expression_kind)ek_init_constant) {
    /* Only casts to arithmetic or pointer types are allowed in initializer
       constants. */
    if (!is_scalar_type(type_cast_to)) {
      error(ec_cast_not_scalar);
      err = TRUE;
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
    } else if (C_dialect == C_dialect_cplusplus &&
               is_array_type(type_cast_to)) {
      /* In C++, treat a cast to an array type as a cast to a pointer to
         the array element type.  This is an extension to match cfront 2.1 */
      *p_type_cast_to = type_cast_to =
                           make_pointer_type(array_element_type(type_cast_to));
      warning(ec_nonstd_array_cast);
    } else {
      /* Invalid cast. */
      error(ec_cast_not_scalar_or_void);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    /* Casting to a qualified type, though valid, is pointless. */
    if (is_qualified_type(type_cast_to)) {
      warning(ec_cast_to_qualified_type);
      *p_type_cast_to = type_cast_to = make_unqualified_type(type_cast_to);
    }  /* if */
    /* Determine whether of not the cast is to a pointer-to-function type
       (this is needed in C++ to allow the anachronism of casting a bound
       function pointer to a normal function pointer). */
    if (C_dialect == C_dialect_cplusplus &&
        is_pointer_type(type_cast_to) &&
        is_function_type(type_pointed_to(type_cast_to))) {
      *cast_to_func_ptr = TRUE;
    }  /* if */
  }  /* if */
  return err;
}  /* cast_type_pre_check */


static void do_cast(a_type_ptr         type_cast_to,
                    an_operand         *operand,
                    an_operand         *bound_function_selector,
                    a_boolean          err,
                    a_boolean          cast_to_reference,
                    a_boolean          int_to_ptr_case,
                    a_boolean          cast_to_func_ptr,
                    an_expression_kind expression_kind,
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
here).  expression_kind indicates the current expression kind.
start_position is the source position of the start of the cast.  This
routine is called for both C-style casts and C++ functional-notation type
conversions.
*/
{
  a_type_ptr       source_type;
  an_error_code    warning_suggested;
  a_boolean        failed = FALSE, class_bitwise_copy;
  a_routine_ptr    conversion_routine;
  an_expr_node_ptr func_ptr_node, object_node;

  if (err) {
    /* There was a previous error (e.g., the type to cast to is invalid
       regardless of the type of the source).  Do no further checking. */
  } else {
    if (cast_to_reference) {
      /* In C++, "An object may be explicitly converted to a reference type
         X& if a pointer to that object may be explicitly converted
         to an X*" (ARM 5.4).  Rewrite the cast in that form.  Note that
         type_cast_to is already set to the proper pointer type. */
      /* The expression must be an lvalue (that term in C++ includes function
         designators). */
      if (is_an_lvalue(operand)) {
        take_address_of_lvalue(operand, expression_kind);
      } else if (is_a_function_designator(operand)) {
        conv_function_designator_to_ptr_to_function(operand, expression_kind);
      } else {
        if (!is_error_operand(operand)) {
          error_in_operand(ec_expr_not_an_lvalue, operand);
        }  /* if */
      }  /* if */
    }  /* if */
    /* Check for user-defined conversions, but not in constant expressions
       or in C, and not when casting to void. */
    if (C_dialect == C_dialect_cplusplus &&
        !is_const_expr_kind(expression_kind) &&
        !is_void_type(type_cast_to) &&
        user_defined_conversion_possible(operand, type_cast_to,
                                         /*is_initialization=*/TRUE,
                                         &conversion_routine,
                                         &class_bitwise_copy,
                                         &failed)) {
      /* A user-defined conversion can be done. */
      user_convert_operand(operand, type_cast_to,
                           /*result_may_be_lvalue=*/cast_to_reference,
                           conversion_routine, class_bitwise_copy,
                           expression_kind);
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
                                   TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                   expression_kind);
      }  /* if */
      /* Get the source type after the transformations. */
      source_type = operand->type;
      /* cast_type_pre_check has already verified that the destination type
         is legal in broad terms.  Check for casts that are allowed in
         general but disallowed in specific modes. */
      if (is_error_operand(operand)) {
        /* There was a previous error.  Do no further checking. */
        err = TRUE;
      } else if (expression_kind == (an_expression_kind)ek_integral_constant) {
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
            if (strict_ansi_mode) warning(ec_expr_not_arithmetic);
          } else {
            error(ec_expr_not_arithmetic);
            err = TRUE;
          }  /* if */
        }  /* if */
      } else if (expression_kind == (an_expression_kind)ek_init_constant) {
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
              if (strict_ansi_mode) warning(ec_expr_not_arithmetic);
            } else {
              /* Non-arithmetic --> arithmetic. */
              error(ec_expr_not_arithmetic);
              err = TRUE;
            }  /* if */
          }  /* if */
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
            pos_warning(ec_bound_function_cast_anachronism, start_position);
            conv_lvalue_to_rvalue(operand, expression_kind);
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
            cast_operand(type_cast_to, operand, expression_kind,
                         /*is_implicit_cast=*/FALSE);
          } else {
            /* Any other use of a bound function.  Error. */
            error_in_operand(ec_bound_function_must_be_called, operand);
          }  /* if */
        } else if (is_void_type(type_cast_to)) {
          /* Anything --> void, allowed. */
          conv_lvalue_to_rvalue(operand, expression_kind);
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
          make_expression_operand(
                          make_operator_node((an_expr_operator_kind)eok_cast,
                                             type_cast_to,
                                             make_node_from_operand(operand)),
                          type_cast_to,
                          operand);
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
              conv_lvalue_to_rvalue(operand, expression_kind);
            }  /* if */
            /* Do the actual cast. */
            cast_operand(type_cast_to, operand, expression_kind,
                         /*is_implicit_cast=*/FALSE);
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
  }  /* if */
  if (err) make_error_operand(operand);
  operand->position = *start_position;
}  /* do_cast */


static void scan_cast_or_expr(
                             an_operand               *result,
                             an_operand               *bound_function_selector,
                             an_expression_kind       expression_kind,
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
  a_local_expr_options_set
                    cast_options;
  an_operand        local_bound_function_selector;

  db_enter(4, "scan_cast_or_expr");

  /* Save the current source position.  Note that in the parenthesis-trapped
     case we are saving the position of the token after the left parenthesis,
     but that's okay; the caller straightens it out. */
  copy_source_position(pos_curr_token, start_position);

  add_stop_token(tok_rparen);

  /* Get past the opening lparen.  If a left parenthesis was trapped,
     we're already past it, so do not advance. */
  if (!(local_options & EOPT_TRAPPED_LEFT_PAREN)) (void)get_token();

  /* Determine if this is a cast operation.  Cast operations are not
     allowed in preprocessing expressions (although the check is superfluous
     since identifiers are never recognized as type names, and therefore
     is_decl_not_expr would never return TRUE). */
  if (expression_kind != (an_expression_kind)ek_pp &&
      is_decl_not_expr(/*abstract_declarator_allowed=*/TRUE,
                       /*real_declarator_allowed=*/FALSE)) {
    /* This is a cast operation. */
    /* Get the type to cast to. */
    type_name(&type_cast_to);
    /* Check the type to see if it is valid.  This is done early to get a
       better error position. */
    err = cast_type_pre_check(&type_cast_to, expression_kind,
                              &cast_to_reference, &int_to_ptr_case,
                              &cast_to_func_ptr,
                              local_options);

    /* The next token should be the closing rparen. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);

    /* Scan the expression to be cast. */
    cast_options = EOPT_OPERAND_OF_CAST;
    if (cast_to_func_ptr) {
      /* In C++, allow a bound function as the operand of a cast to a
         normal function pointer. */
      cast_options |= EOPT_ALLOW_BOUND_FUNCTION;
    }  /* if */
    scan_expr_full(result, &local_bound_function_selector, PREC_CAST,
                   expression_kind, cast_options);
    /* Check compatibility of the types and do the cast. */
    do_cast(type_cast_to, result, &local_bound_function_selector, err,
            cast_to_reference, int_to_ptr_case, cast_to_func_ptr,
            expression_kind, &start_position);
  } else {
    /* This is an expression in parentheses.  The parentheses do not
       affect the fact that the enclosed expression is an immediate operand
       of the surrounding context, so most of the option flags are
       passed down. */
    scan_expr_full(result, bound_function_selector,
                   PREC_LOWEST, expression_kind,
                   (local_options &
                    (EOPT_OPERAND_OF_CAST | EOPT_OPERAND_OF_ADDRESS_OF)) |
                   EOPT_ALLOW_BOUND_FUNCTION);
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
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

  clear_token_cache(&cache);
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
                                      an_expression_kind       expression_kind,
                                      a_local_expr_options_set local_options)
/*
Scan a C++ functional-notation type conversion, e.g., "int(1.5)" or "A(1,2)".
The type keyword or identifier is the current token, and the associated
type is passed in as type_cast_to.  The result is returned in *result.
expression_kind indicates the kind of the current expression.
*/
{
  a_source_position             start_position;
  a_boolean                     err = FALSE;
  a_boolean                     int_to_ptr_case;
  a_boolean                     cast_to_reference;
  a_boolean                     cast_to_func_ptr;
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
  err = cast_type_pre_check(&type_cast_to, expression_kind,
                            &cast_to_reference, &int_to_ptr_case,
                            &cast_to_func_ptr, local_options);
  /* See if we have a case that is clearly a constructor call. */
  if (is_class_struct_union_type(type_cast_to)) {
    cssp = symbol_supplement_for_class(type_cast_to);
    ctor_sym = cssp->constructor;
    if (ctor_sym != NULL) {
      /* The type name is the name of a class that has at least one
         constructor. */
      if (!cssp->target_of_conversion_function) {
        /* There is no conversion function that targets this class, so this
           must be a constructor call. */
        ctor_case = TRUE;
      } else {
        /* There is both a constructor for this class and a conversion function
           whose result is the class; look ahead to see if this conversion has
           exactly one argument.  If not, it must be a constructor call.
           If so, it could be either, so go into the usual conversion
           processing. */
        if (!conversion_has_one_argument()) ctor_case = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Advance past the type keyword or identifier. */
  (void)get_token();
  /* Check for a left parenthesis. */
  (void)required_token(tok_lparen, ec_exp_lparen);
  if (ctor_case) {
    /* Converting to a class type.  The contents of the parentheses are
       arguments for a constructor call. */
    scan_ctor_arguments(ctor_sym, &arg_expr_list, &ctor_routine);
    if (ctor_routine == NULL) {
      /* Error of some sort. */
      make_error_operand(result);
    } else {
      /* Make a dynamic init entry that calls constructor to initialize
         a temporary.  Make an operand for the value of the temporary. */
      make_constructor_dynamic_init(ctor_routine, arg_expr_list,
                                    /*result_is_addr=*/FALSE, result);
    }  /* if */
  } else {
    /* Not a constructor case; obeys the same rules as a C-style cast. */
    add_stop_token(tok_rparen);
    /* Scan the expression to be cast.  If the expression is omitted,
       use zero (the ARM says the result is undefined, so zero is
       acceptable; zero is used because it can be cast to any scalar
       type). */
    if (curr_token == tok_rparen) {
      make_integer_constant_operand(result, 0L);
    } else {
      /* Since the expression in parentheses is syntactically an
         expression list, a top-level comma is not allowed. */
      cast_options = EOPT_OPERAND_OF_CAST | EOPT_DISALLOW_COMMA_OPERATOR;
      if (cast_to_func_ptr) {
        /* In C++, allow a bound function as the operand of a cast to a
           normal function pointer. */
        cast_options |= EOPT_ALLOW_BOUND_FUNCTION;
      }  /* if */
      scan_expr_full(result, &local_bound_function_selector, PREC_LOWEST,
                     expression_kind, cast_options);
    }  /* if */
    /* Check compatibility of the types and do the cast. */
    do_cast(type_cast_to, result, &local_bound_function_selector, err,
            cast_to_reference, int_to_ptr_case, cast_to_func_ptr,
            expression_kind, &start_position);
    /* Check for the closing parenthesis. */
    check_closing_paren_after_expr_list();
    remove_stop_token(tok_rparen);
  }  /* if */
  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);
  db_exit();
}  /* scan_functional_notation_type_conversion */


static void scan_mult_operator(an_operand         *operand_1,
                               an_operand         *result,
			       an_expression_kind expression_kind)
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
  scan_expr(&operand_2, PREC_MULT_DIV, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be of arithmetic type (the remainder operator
       requires integral type). */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    if (save_token == tok_remainder) {
      (void)check_integral_operand(operand_1);
    } else {
      (void)check_arithmetic_operand(operand_1);
    }  /* if */
    /* The second operand must be of arithmetic type (the remainder operator
       requires integral type). */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
    if (save_token == tok_remainder) {
      (void)check_integral_operand(&operand_2);
    } else {
      (void)check_arithmetic_operand(&operand_2);
    }  /* if */

    result_type = determine_arithmetic_conversions(operand_1, &operand_2);
    change_binary_operand_types(result_type, operand_1, &operand_2,
                                expression_kind);
    if ((save_token == tok_divide || save_token == tok_remainder) &&
        expression_kind != (an_expression_kind)ek_not_evaluated &&
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
                        result_type, result, &operator_position,
                        expression_kind);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_mult_operator */


static void scan_add_operator(an_operand         *operand_1,
                              an_operand         *result,
			      an_expression_kind expression_kind)
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
  scan_expr(&operand_2, PREC_PLUS_MINUS, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    if (is_arithmetic_type(operand_1->type)) {
      operand_1_is_pointer = FALSE;
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      /* If the first operand is a pointer, it must be a pointer to an
         object. */
      (void)check_object_pointer_operand(operand_1,
                                         ec_expr_not_pointer_to_object);
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
    /* Check whether the operand types are valid. */
    if (is_error_operand(operand_1) || is_error_operand(&operand_2)) {
      err = TRUE;
    } else if (operand_1_is_pointer) {
      /* Operand 1 has pointer type. */
      if (is_integral_type(operand_2.type)) {
        /* Pointer +- integral. */
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
                           &operation_type)) {
          /* Difference between compatible pointers.  Result has type
             ptrdiff_t (see 3.3.6 and <stddef.h>). */
          result_type = integer_type((an_integer_kind)TARG_PTRDIFF_T_INT_KIND);
        } else {
          /* Difference between incompatible pointers. */
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
      /* The result type is the same as the pointer type in operand 2. */
      result_type = operation_type = operand_2.type;
      /* The pointer operand must be a pointer to an object. */
      (void)check_object_pointer_operand(&operand_2,
                                         ec_expr_not_pointer_to_object);
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
        change_binary_operand_types(result_type, operand_1, &operand_2,
                                    expression_kind);
      }  /* if */
      /* Determine the expression operator for this case. */
      if (pointer_difference) {
        /* Pointer - pointer is a special case, with its own operator. */
        op = (an_expr_operator_kind)eok_pdiff;
      } else {
        op = which_binary_operator(save_token, operation_type);
      }  /* if */
      do_binary_operation(op, operand_1, &operand_2,
                          result_type, result, &operator_position,
                          expression_kind);
      /* For pointer addition or subtraction (but not difference), preserve
         the cross-reference entries for the pointer operand.  This is in
         case the result is turned back into an lvalue, as in "*(arr + 1) = x".
         Recall that the pointer operand is always operand_1 by this point. */
      if (op == (an_expr_operator_kind)eok_padd ||
          op == (an_expr_operator_kind)eok_psubtract) {
        result->xref_entries_list = operand_1->xref_entries_list;
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_add_operator */


static void scan_shift_operator(an_operand         *operand_1,
                                an_operand         *result,
			        an_expression_kind expression_kind)
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
  scan_expr(&operand_2, PREC_SHIFT, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    (void)check_integral_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
    (void)check_integral_operand(&operand_2);

    if (C_dialect == C_dialect_pcc) {
      /* In K&R first edition (see appendix A, section 7.5), "perform the usual
         arithmetic conversions on their operands, each of which must be 
         integral.  Then the right operand is converted to int; the type of
         the result is that of the left operand."  This has the effect
         that a "long" shift count will force the shift to be done as long. */
      result_type = determine_arithmetic_conversions(operand_1, &operand_2);
      change_binary_operand_types(result_type, operand_1, &operand_2,
                                  expression_kind);
      cast_operand(integer_type((an_integer_kind)ik_int), &operand_2,
                   expression_kind, /*is_implicit_cast=*/TRUE);    
    } else {
      /* ANSI rules just call for the integral promotions; the type of
         the result is the type of the left operand. */
      promote_operand(operand_1, expression_kind);
      promote_operand(&operand_2, expression_kind);
    }  /* if */
    if (expression_kind != (an_expression_kind)ek_not_evaluated &&
        is_constant_operand(&operand_2) &&
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
                        &error_position, expression_kind);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_shift_operator */


static void scan_rel_operator(an_operand         *operand_1,
                              an_operand         *result,
			      an_expression_kind expression_kind)
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

  db_enter(4, "scan_rel_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_RELATIONAL, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    if (is_arithmetic_type(operand_1->type)) {
      operand_1_is_pointer = FALSE;
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
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
        (void)check_arithmetic_operand(&operand_2);
        if (expression_kind != (an_expression_kind)ek_not_evaluated &&
            (!is_constant_operand(operand_1) ||
             !is_constant_operand(&operand_2))) {
          /* Check for pointless comparisons of unsigned integers against 0,
             and give a warning.  The pointless cases are
               u >= 0    (always true)
               u <  0    (always false)
               0 >  u    (always false)
               0 >= u    (always true)
             The expression is not simplified.  This test must be done before
             the integral promotions, because they will turn (e.g.) unsigned
             char into (signed) int.
          */
          if (((save_token == tok_ge || save_token == tok_lt) &&
               is_integral_type(operand_1->type) &&
               !is_signed_integral_type(operand_1->type) &&
               op_is_zero_constant(&operand_2)) ||
              ((save_token == tok_gt || save_token == tok_ge) &&
               is_integral_type(operand_2.type) &&
               !is_signed_integral_type(operand_2.type) &&
               op_is_zero_constant(operand_1))) {
            pos_warning(ec_unsigned_compare_with_zero, &operator_position);
          }  /* if */
        }  /* if */
        operation_type = determine_arithmetic_conversions(operand_1,
                                                          &operand_2);
      }  /* if */
    }  /* if */

    result_type = get_logical_result_type(expression_kind, operand_1,
                                          &operand_2);
    change_binary_operand_types(operation_type, operand_1, &operand_2,
                                expression_kind);
    op = which_binary_operator(save_token, operation_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position, expression_kind);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_rel_operator */


static void scan_eq_operator(an_operand         *operand_1,
                             an_operand         *result,
			     an_expression_kind expression_kind)
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

  db_enter(4, "scan_eq_operator");

  save_token = curr_token;
  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_EQ_NE, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* The first operand must be arithmetic or a pointer. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    operand_1_is_pointer = operand_1_is_ptr_to_member = FALSE;
    if (is_arithmetic_type(operand_1->type)) {
      /* Okay. */
    } else if (is_ptr_to_member_type(operand_1->type)) {
      operand_1_is_ptr_to_member = TRUE;
    } else if (check_pointer_operand(operand_1, ec_expr_not_scalar)) {
      operand_1_is_pointer = TRUE;
    }  /* if */
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
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
        (void)check_arithmetic_operand(&operand_2);
        operation_type = determine_arithmetic_conversions(operand_1,
                                                          &operand_2);
      }  /* if */
    }  /* if */

    result_type = get_logical_result_type(expression_kind, operand_1,
                                          &operand_2);
    change_binary_operand_types(operation_type, operand_1, &operand_2,
                                expression_kind);
    op = which_binary_operator(save_token, operation_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position, expression_kind);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_eq_operator */


static void scan_bit_operator(an_operand         *operand_1,
                              an_operand         *result,
			      an_expression_kind expression_kind)
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
  scan_expr(&operand_2, prec_level, expression_kind, EOPT_NO_OPTIONS);

  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be integral. */
    do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
    (void)check_integral_operand(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
    (void)check_integral_operand(&operand_2);
    result_type = determine_arithmetic_conversions(operand_1, &operand_2);
    change_binary_operand_types(result_type, operand_1, &operand_2,
                                expression_kind);
    op = which_binary_operator(save_token, result_type);
    do_binary_operation(op, operand_1, &operand_2, result_type, result,
                        &operator_position, expression_kind);
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_bit_operator */


static void scan_logical_operator(an_operand         *operand_1,
                                  an_operand         *result,
			          an_expression_kind expression_kind)
/*
Scan the "&&" and "||" operators.  See sections 3.3.13 and 3.3.14 of the
standard.
*/
{
  an_expr_operator_kind op;
  an_operand            operand_2;
  a_source_position     operator_position;
  an_expression_kind    expr2_kind;
  a_boolean             operand_1_is_zero  = FALSE;
  long                  local_result;
  a_boolean             known_result       = FALSE;
  a_token_kind          save_token;
  a_type_ptr            result_type;
  a_boolean             processed = FALSE;
  a_boolean             might_be_overloaded = FALSE;
  int                   prec_level;
  a_boolean             operand_1_rvalue_conversion_done = FALSE;

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

  if (C_dialect == C_dialect_cplusplus &&
      opname_symbol_table[opname_kind_for_token[(int)save_token]] != NULL) {
    /* We are in C++ mode, and there is an operator function that overloads
       this operator. */
    might_be_overloaded = TRUE;
  }  /* if */

  /* Determine whether or not the second operand should be evaluated. */
  expr2_kind = expression_kind;
  if (expression_kind != (an_expression_kind)ek_not_evaluated &&
      !might_be_overloaded) {
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
      operand_1_rvalue_conversion_done = FALSE;
      conv_lvalue_to_rvalue(operand_1, expression_kind);
      if (is_constant_operand(operand_1)) {
        operand_1_is_zero = op_is_zero_constant(operand_1);
        if (save_token == tok_and_and && operand_1_is_zero) {
          /* 0 && something -- this always evaluates to a zero value. */
          local_result = 0;
          known_result = TRUE;
          expr2_kind = (an_expression_kind)ek_not_evaluated;
        } else if (save_token == tok_or_or && !operand_1_is_zero) {
          /* non-zero || something -- this always evaluates to a value of 1. */
          local_result = 1;
          known_result = TRUE;
          expr2_kind = (an_expression_kind)ek_not_evaluated;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, prec_level, expr2_kind, EOPT_NO_OPTIONS);

  /* Note that we do not test might_be_overloaded here, because we want
     to go to the subroutine to look for conversions from class types
     to built-in types. */
  if (C_dialect == C_dialect_cplusplus &&
      (is_class_struct_union_type(operand_1->type) ||
       is_class_struct_union_type(operand_2.type))) {
    /* Look for C++ operator overloading cases. */
    check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                   /*unary_operator=*/FALSE,
                                   /*must_be_member_function=*/FALSE,
                                   /*has_predef_meaning=*/FALSE,
                                   operand_1, &operand_2,
                                   expression_kind, &operator_position,
                                   result, &processed);
  }  /* if */
  if (!processed) {
    /* Non-operator-function cases. */
    /* Both operands must be scalar. */
    do_operand_transformations(operand_1,
                               operand_1_rvalue_conversion_done ?
                                    TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION :
                                    TOPT_NO_OPTIONS,
                               expression_kind);
    (void)check_boolean_controlling_expr(operand_1);
    do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
    (void)check_boolean_controlling_expr(&operand_2);
    if (!known_result) {
      /* Normal case: the result is not known. */
      result_type = get_logical_result_type(expression_kind, operand_1,
                                            &operand_2);
      op = which_binary_operator(save_token, result_type);
      do_binary_operation(op, operand_1, &operand_2, result_type, result,
                          &operator_position, expression_kind);
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
  a_constant_ptr op1_enum, op2_enum;

  /* In C++, enumeration constants have the same type as the enumeration, so
     this routine is not needed. */
  if (C_dialect != C_dialect_cplusplus) {
    op1_type = skip_typerefs(op1_type);
    op2_type = skip_typerefs(op2_type);
    if (is_integral_type(op1_type) && is_integral_type(op2_type)) {
      op1_enum = op1_type->variant.integer.enum_constant_list;
      op2_enum = op2_type->variant.integer.enum_constant_list;
      if (op1_enum != NULL && op1_enum == op2_enum) {
        /* Use the type associated with the first enumerated constant
           on the list, which should be "int" tagged as an enumeration
           type. */
        op1_type = op1_enum->type;
#if CHECKING
        if (!is_integral_type(*result_type) ||
            !is_integral_type(op1_type) ||
            skip_typerefs(*result_type)->variant.integer.int_kind !=
                           skip_typerefs(op1_type)->variant.integer.int_kind) {
          internal_error("keep_enum_in_result_type: bad enum result type");
        }  /* if */
#endif /* CHECKING */
        *result_type = op1_type;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* keep_enum_in_result_type */


static void scan_conditional_operator(an_operand         *operand_1,
                                      an_operand         *result,
                                      an_expression_kind expression_kind)
/*
Scan the "?" operator.  See section 3.3.15 of the standard.
*/
{
  an_operand            operand_2;
  an_operand            operand_3;
  a_source_position     operator_position;
  a_boolean             operand_1_is_const = FALSE;
  a_boolean             operand_1_is_zero  = FALSE;
  a_boolean             result_is_an_lvalue = FALSE;
  a_boolean             err = FALSE, processed = FALSE;
  a_type_ptr            result_type, ptr_result_type, operation_type;
  an_expression_kind    expr2_kind, expr3_kind;
  a_boolean             operand_2_is_pointer, operand_3_is_pointer;
  a_type_ptr            type_pointed_to_2, type_pointed_to_3;
  a_type_ptr            unqual_type_pointed_to_2, unqual_type_pointed_to_3;

  db_enter(4, "scan_conditional_operator");

  /* The first operand must be scalar. */
  do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
  (void)check_boolean_controlling_expr(operand_1);

  expr2_kind = expr3_kind = expression_kind;
  if (expression_kind != (an_expression_kind)ek_not_evaluated) {
    /* If the first operand is a constant and if the constant is zero, evaluate
       the third operand only.  If the first operand is constant and is not a
       constant zero, evaluate the second operand only. */
    operand_1_is_const = is_constant_operand(operand_1);
    if (operand_1_is_const) {
      operand_1_is_zero = op_is_zero_constant(operand_1);
      if (operand_1_is_zero) {
	/* The first operand is a constant zero, so do not evaluate the second
	   operand. */
        expr2_kind = (an_expression_kind)ek_not_evaluated;
      } else {
	/* The first operand is a constant non-zero, so do not evaluate the
           third operand. */
        expr3_kind = (an_expression_kind)ek_not_evaluated;
      }  /* if */
    }  /* if */
  }  /* if */

  /* Scan the second operand.   Evaluate the expression if the first
     operand is non-constant or a non-zero constant, and if we are currently
     evaluating expressions. */
  (void)get_token();
  scan_expr(&operand_2, PREC_LOWEST, expr2_kind, EOPT_NO_OPTIONS);
  do_operand_transformations(&operand_2,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                             expression_kind);

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
  scan_expr(&operand_3, PREC_QUEST_MARK, expr3_kind, EOPT_NO_OPTIONS);
  do_operand_transformations(&operand_3,
                             TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                             expression_kind);

  /* Check the operands for compatibility.  Both must be arithmetic,
     both compatible struct/union types, both void, or both pointers. */
  result_type = operand_2.type;  /* Assume. */
  if (is_error_operand(&operand_2) || is_error_operand(&operand_3)) {
    /* One or both of the operands has an error. */
    err = TRUE;
  } else if (C_dialect == C_dialect_cplusplus &&
             types_are_compatible(operand_2.type, operand_3.type)) {
    /* In C++, if the types are the same the result has that type.
       No arithmetic conversions are done (e.g., integral promotions
       are not done). */
    result_type = operand_2.type;
    /* The result is an lvalue if the second and third operands are lvalues. */
    if (is_an_lvalue(&operand_2) && is_an_lvalue(&operand_3)) {
      result_is_an_lvalue = TRUE;
      result_type = make_pointer_type(result_type);
    } else {
      /* If one is an rvalue, make them both rvalues. */
      conv_lvalue_to_rvalue(&operand_2, expr2_kind);
      conv_lvalue_to_rvalue(&operand_3, expr3_kind);
    }  /* if */
  } else {
    /* Not C++ mode, or the operand types are not the same. */
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_struct_union_type(operand_2.type) ||
         is_class_struct_union_type(operand_3.type))) {
      /* Look for C++ operator overloading cases.  The operator itself
         cannot be overloaded, but this also checks for cases where conversion
         functions can be used to convert the operands to types suitable for
         the built-in meaning of the operator. */
      check_for_operator_overloading((an_opname_kind)onk_question,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     &operand_2, &operand_3,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    /* "processed" at this point indicates an error already handled. */
    if (processed) {
      err = TRUE;
    } else {
      conv_lvalue_to_rvalue(&operand_2, expr2_kind);
      conv_lvalue_to_rvalue(&operand_3, expr3_kind);
      operand_2_is_pointer = is_pointer_type(operand_2.type);
      operand_3_is_pointer = is_pointer_type(operand_3.type);
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
          pos_error(ec_incompatible_operands, &operator_position);
          err = TRUE;
        }  /* if */
      } else {
        /* Incompatible operands. */
        pos_error(ec_incompatible_operands, &operator_position);
        err = TRUE;
      }  /* if */
      /* Cast operands 2 and 3 to the result type if necessary. */
      if (!err) {
        change_binary_operand_types(result_type, &operand_2, &operand_3,
                                    expression_kind);
      }  /* if */
    }  /* if */
  }  /* if */

  if (err || is_error_operand(operand_1)) {
    make_error_operand(result);
  } else if (operand_1_is_const) {
    if (operand_1_is_zero) {
      /* The first operand is a zero constant; return the third operand as
	 the result. */
      copy_operand(&operand_3, result);
    } else {
      /* The first operand is a non-zero constant; return the second operand
	 as the result. */
      copy_operand(&operand_2, result);
    }  /* if */
  } else {
    /* The first operand is not a constant, so build the expression. */
    /* Make an operator node with the first part of the expression. */
    build_unary_result_operand(operand_1,
                               (an_expr_operator_kind)eok_question,
			       result_type, result);
    /* Now link the other two operands from this one. */
    result->variant.expression->variant.operation.operands->next =
                                            make_node_from_operand(&operand_2);
    result->variant.expression->variant.operation.operands->next->next =
                                            make_node_from_operand(&operand_3);
    /* The result is an lvalue if the second and third operands are. */
    if (result_is_an_lvalue) {
      result->state = (an_operand_state)os_lvalue;
      result->type = type_pointed_to(result_type);
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
          /* Yes.  Issue a warning and change the operand to an lvalue for
             the "this" variable. */
          is_this = TRUE;
          pos_warning(ec_assignment_to_this, &operand->position);
          make_lvalue_variable_operand(this_var, operand,
                                       operand->xref_entries_list);
          current_routine_entry()->assignment_to_this_done = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_this;
}  /* check_assignment_to_this_pointer */

#endif /* ASSIGNMENT_TO_THIS_ALLOWED */

static void change_assignment_result_to_lvalue(an_operand *result)
/*
In C++ mode, assignment operators return lvalues.  Change the assignment
operation in *result from an rvalue-returning assignment to an lvalue-returning
assignment.  This routine is called only in C++ mode.
*/
{
  an_expr_node_ptr node;

  if (!is_error_operand(result)) {
    node = result->variant.expression;
    node->variant.operation.assignment_returns_lvalue = TRUE;
    node->type = make_pointer_type(node->type);
  }  /* if */
  result->state = (an_operand_state)os_lvalue;
}  /* change_assignment_result_to_lvalue */


static void scan_simple_assignment_operator(an_operand         *operand_1,
                                            an_operand         *result,
                                            an_expression_kind expression_kind)
/*
Scan the simple assignment operator ("=").  See section 3.3.16 of the standard.
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_boolean         err = FALSE, processed = FALSE;
  a_type_ptr        result_type;

  db_enter(4, "scan_simple_assignment_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  if (is_const_expr_kind(expression_kind)) {
    /* Assignment operation not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();

  scan_expr(&operand_2, PREC_ASSIGNMENT, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    /* The type of the assignment is the destination type with any qualifiers
       dropped. */
    if (C_dialect == C_dialect_cplusplus &&
        is_class_struct_union_type(operand_1->type)) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_assign,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/TRUE,
                                     /*has_predef_meaning=*/TRUE,
                                     operand_1, &operand_2,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases, including all C cases. */
      do_operand_transformations(operand_1,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
#if ASSIGNMENT_TO_THIS_ALLOWED
      if (C_dialect == C_dialect_cplusplus &&
          is_an_rvalue(operand_1) &&  /* For speed. */
          check_assignment_to_this_pointer(operand_1)) {
        /* Anachronism -- assigning to the "this" pointer. */
        /* The subroutine changes operand_1 to the proper lvalue. */
      } else {
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
        if (check_modifiable_lvalue_operand(operand_1)) {
          modifying_lvalue(operand_1, expression_kind);
        }  /* if */
#if ASSIGNMENT_TO_THIS_ALLOWED
      }  /* if */
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
      result_type = make_unqualified_type(operand_1->type);
      /* It's okay for operand_2 to be an indefinite function. */
      do_operand_transformations(&operand_2,
                                 TOPT_SUPPRESS_CHECK_FOR_INDEFINITE_FUNCTION,
                                 expression_kind);
      prep_assignment_operand(&operand_2, result_type, expression_kind,
                              ec_incompatible_operands, &operator_position);
      build_binary_result_operand(operand_1, &operand_2,
                                  which_binary_operator(tok_assign,
                                                        result_type),
                                  result_type, result);
      /* In C++, assignment operators return lvalues. */
      if (C_dialect == C_dialect_cplusplus) {
        change_assignment_result_to_lvalue(result);
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_simple_assignment_operator */


static void scan_compound_assignment_operator(
                                            an_operand         *operand_1,
                                            an_operand         *result,
                                            an_expression_kind expression_kind)
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

  if (is_const_expr_kind(expression_kind)) {
    /* Assignment operation not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &operator_position);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_ASSIGNMENT, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_struct_union_type(operand_1->type) ||
         is_class_struct_union_type(operand_2.type))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading(opname_kind_for_token[(int)save_token],
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/FALSE,
                                     operand_1, &operand_2,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
      if (check_modifiable_lvalue_operand(operand_1)) {
        modifying_lvalue(operand_1, expression_kind);
      }  /* if */
      do_operand_transformations(&operand_2, TOPT_NO_OPTIONS, expression_kind);
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
          cast_operand(operation_type, &operand_2, expression_kind,
                       /*is_implicit_cast=*/TRUE);
        }  /* if */
        build_binary_result_operand(operand_1, &operand_2,
                                    which_binary_operator(save_token,
                                                          operation_type),
                                    result_type, result);
        /* In C++, assignment operators return lvalues. */
        if (C_dialect == C_dialect_cplusplus) {
          change_assignment_result_to_lvalue(result);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_compound_assignment_operator */


static void scan_comma_operator(an_operand         *operand_1,
                                an_operand         *result,
			        an_expression_kind expression_kind)
/*
Scan the "," operator.  Note that this routine is not called if the
comma operator if not allowed (local_options flag
EOPT_DISALLOW_COMMA_OPERATOR).
*/
{
  an_operand        operand_2;
  a_source_position operator_position;
  a_type_ptr        result_type;
  a_boolean         err = FALSE, processed = FALSE;
  a_boolean         result_is_an_lvalue = FALSE;

  db_enter(4, "scan_comma_operator");

  /* Save the position of the operator in case of error. */
  copy_source_position(pos_curr_token, operator_position);

  if (is_const_expr_kind(expression_kind)) {
    /* Comma operator not allowed in constant expressions. */
    pos_error(ec_bad_constant_operator, &pos_curr_token);
    err = TRUE;
  }  /* if */

  /* Scan the second operand. */
  (void)get_token();
  scan_expr(&operand_2, PREC_COMMA, expression_kind, EOPT_NO_OPTIONS);

  if (err) {
    /* Operator is not allowed in this kind of expression. */
    make_error_operand(result);
  } else {
    if (C_dialect == C_dialect_cplusplus &&
        (is_class_struct_union_type(operand_1->type) ||
         is_class_struct_union_type(operand_2.type))) {
      /* Look for C++ operator overloading cases. */
      check_for_operator_overloading((an_opname_kind)onk_comma,
                                     /*unary_operator=*/FALSE,
                                     /*must_be_member_function=*/FALSE,
                                     /*has_predef_meaning=*/TRUE,
                                     operand_1, &operand_2,
                                     expression_kind, &operator_position,
                                     result, &processed);
    }  /* if */
    if (!processed) {
      /* Non-operator-function cases. */
      do_operand_transformations(operand_1, TOPT_NO_OPTIONS, expression_kind);
      do_operand_transformations(&operand_2,
                                 TOPT_SUPPRESS_LVALUE_TO_RVALUE_CONVERSION,
                                 expression_kind);
      /* Simplify the void expression. */
      simplify_void_operand(operand_1);
      /* The result type is the type of the second operand. */
      result_type = operand_2.type;
      /* In C++ mode, an lvalue in the second operand is preserved.  In C
         mode, an lvalue is converted to an rvalue. */
      if (C_dialect == C_dialect_cplusplus) {
        result_is_an_lvalue = is_an_lvalue(&operand_2);
        if (result_is_an_lvalue) result_type = make_pointer_type(result_type);
      } else {
        conv_lvalue_to_rvalue(&operand_2, expression_kind);
      }  /* if */
      /* Make a comma operator expression. */
      build_binary_result_operand(operand_1, &operand_2,
                                  (an_expr_operator_kind)eok_comma,
                                  result_type, result);
      /* In C++ mode, the result is an lvalue if the second operation
         is an lvalue. */
      if (result_is_an_lvalue) {
        result->state = operand_2.state;
        result->type = operand_2.type;
      }  /* if */
    }  /* if */
  }  /* if */

  /* Set the error position to the starting position. */
  copy_source_position(operand_1->position, error_position);
  copy_source_position(operand_1->position, result->position);

  db_exit();
}  /* scan_comma_operator */


static void make_anonymous_union_field_operand(
                                           a_symbol_ptr       sym_ptr,
                                           an_expression_kind expression_kind,
                                           a_source_position  *source_position,
                                           an_xref_entry_ptr  xep,
                                           an_operand         *result)
/*
Make an operand for a field that is a member of a top-level anonymous union.
(That is, an anonymous union that is not inside a struct or union.)
sym_ptr is the field; expression_kind is the current expression kind;
source_position indicates the field identifier source position; and xep
points to a cross-reference entry or is NULL if there isn't one.  The
operand is built in *operand.  It's an lvalue for the field.
*/
{
  a_variable_ptr union_var = sym_ptr->variant.field.anonymous_union_variable;
  an_operand     operand_1;

  /* Start with an operand for the base anonymous union variable. */
  make_lvalue_variable_operand(union_var, &operand_1, (an_xref_entry_ptr)NULL);
  /* Add a field selection to get to the field. */
  do_field_selection_operation(&operand_1, union_var->type,
                               /*is_arrow_operator=*/FALSE, expression_kind,
                               sym_ptr, source_position, xep, result);
  result->position = *source_position;
}  /* make_anonymous_union_field_operand */


static a_boolean bad_nested_function_variable_ref(a_symbol_ptr sym_ptr)
/*
Return TRUE if sym_ptr is a symbol for a nonstatic variable from a function
enclosing a local class we're inside of.  A reference to such a symbol
is not allowed.  The symbol may be a member of an anonymous union.
*/
{
  a_boolean      bad_ref = FALSE;
  a_scope_depth  sd;
  a_variable_ptr var;

  /* This sort of bad reference is only possible when we are inside a local
     class (the class itself or one of its member functions). */
  if (inside_local_class) {
    if (sym_ptr->decl_scope == scope_stack[DEPTH_OF_FILE_SCOPE].number) {
      /* A reference to the file scope is okay. */
    } else if (sym_ptr->class_of_which_a_member != NULL) {
      /* A reference to a class member is okay. */
    } else {
      /* Get the variable for the symbol. */
      if (sym_ptr->kind == (a_symbol_kind)sk_variable) {
        var = sym_ptr->variant.variable;
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
        if (skind == (a_scope_kind)sck_class_struct_union ||
            skind == (a_scope_kind)sck_class_reactivation) {
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


static void scan_identifier(an_expression_kind       expression_kind,
			    an_operand               *result,
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
  an_xref_entry_ptr xep;
  a_variable_ptr    this_var;
  an_operand        this_pointer_operand;
  a_boolean         maybe;
  a_boolean         address_of_qualified_member_name = FALSE;

  db_enter(4, "scan_identifier");

#if CHECKING
  if (expression_kind == (an_expression_kind)ek_pp) {
    /* Should never see an identifier in a preprocessing directive. */
    internal_error ("scan_identifier: in preprocessing expr");
  }  /* if */
#endif /* CHECKING */
  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

   /* Check for an operator name like "operator+". */
  if (get_opname()) {
    sym_ptr = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
  } else {
    /* If the identifier is the start of a C++ qualified name, get the whole
       name.  If not, look the name up as a normal identifier. */
    sym_ptr = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
  }  /* if */
  if (sym_ptr == NULL) {
    /* The symbol is not defined. */
    if (is_const_expr_kind(expression_kind)) {
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
      result->xref_entries_list = xref_entry(sym_ptr, &pos_curr_token,
                                             expression_kind);
    }  /* if */
  } else {
    /* The symbol is defined. */
    /* Create a cross-reference entry for the symbol if needed. */
    /* Don't do this if the symbol is an overloaded function (we don't
       yet know which function is being called). */
    if (sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
      xep = NULL;
    } else {
      xep = xref_entry(sym_ptr, &pos_curr_token, expression_kind);
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
      if ((local_options & EOPT_OPERAND_OF_ADDRESS_OF) &&
          locator_for_curr_id.is_qualified_name &&
          sym_ptr->class_of_which_a_member != NULL &&
          token_ends_expr(next_token(), PREC_PREFIX, local_options)) {
        address_of_qualified_member_name = TRUE;
      }  /* if */
      projection_sym_ptr = locator_for_curr_id.specific_symbol;
      /* What kind of symbol is it? */
      switch (sym_ptr->kind) {
        case sk_constant:
          /* Enumerated type constant.  Make a constant. */
          make_constant_operand(sym_ptr->variant.constant, result);
          break;
        case sk_variable:
        case sk_static_data_member:
          var_ptr = sym_ptr->variant.variable;
          if (is_const_expr_kind(expression_kind)) {
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
            /* In C++, integral const identifiers can be used in constant
               expressions.  Note that we need not keep the variable as an
               lvalue to guard against discovering later that the variable's
               address is being taken, because in C++ expressions that allow
               the "&" operator are nonconstant. */
            if (C_dialect == C_dialect_cplusplus &&
                is_const_variable(var_ptr)) {
              a_constant_ptr con_val = var_constant_value(var_ptr);
              if (con_val == NULL) {
                /* The variable is const, but its value is not known at
                   compile time. */
                error_and_make_error_operand(ec_constant_value_not_known,
                                             result);
              } else {
                /* The identifier is const and has a known constant value. */
                make_constant_operand(con_val, result);
              }  /* if */
            } else if (expression_kind ==
                                        (an_expression_kind)ek_init_constant &&
                       has_static_storage_duration(var_ptr->storage_class) &&
                       /* Disallow C++ reference variables in constant
                          expressions.  Could only matter if __INTADDR__
                          is used on a reference variable. */
                       !is_reference_type(var_ptr->type)) {
              /* Make an lvalue operand for the variable. */
              make_lvalue_variable_operand(var_ptr, result, xep);
            } else {
              /* All other cases are not allowed. */
              error_and_make_error_operand(ec_expr_not_constant, result);
            }  /* if */
          } else {
            /* Nonconstant expression. */
            /* If we're inside a local class, we are not allowed to reference
               non-static variables of the containing function.  Check for
               that. */
            if (inside_local_class &&
                bad_nested_function_variable_ref(sym_ptr)) {
              error_and_make_error_operand(ec_ref_to_nested_function_var,
                                           result);
            } else {
              /* Make a variable operand that is a variable address node.
                 The type of the operand is a pointer to the type of the
                 variable. */
              make_lvalue_variable_operand(var_ptr, result, xep);
            }  /* if */
          }  /* if */
          break;
        case sk_routine:
normal_function:
          if (expression_kind == (an_expression_kind)ek_integral_constant) {
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
                                             xep,
                                             result);
          }  /* if */
          break;
        case sk_field:
          /* In C++, in a member function, a reference to a nonstatic data
             member (field) is the same as "this->field".  Or, a field
             could be a member of an unnamed union at file scope or in
             a block. */
          if (sym_ptr->variant.field.anonymous_union_variable != NULL) {
            /* This field is a member of a top-level anonymous union. */
            /* If we're inside a local class, we are not allowed to reference
               non-static variables of the containing function.  Check for
               that. */
            if (inside_local_class &&
                bad_nested_function_variable_ref(sym_ptr)) {
              error_and_make_error_operand(ec_ref_to_nested_function_var,
                                           result);
            } else {
              make_anonymous_union_field_operand(sym_ptr,
                                                 expression_kind,
                                                 &locator_for_curr_id.
                                                               source_position,
                                                 xep, result);
            }  /* if */
          } else {
            /* Normal case -- field is a nonstatic data member of a class. */
            if (address_of_qualified_member_name) {
              /* The field was referenced by a qualified name and is the
                 immediate operand of a unary "&"; make up an operand that
                 preserves the qualified name so scan_ampersand_operator can
                 turn it into a pointer-to-member. */
              make_sym_for_ptr_to_member_operand(projection_sym_ptr, xep,
                                                 result);
            } else {
              /* Make an operand for the "this" pointer. */
              make_this_pointer_operand(projection_sym_ptr,
                                        &locator_for_curr_id.source_position,
                                        &this_pointer_operand,
                                        expression_kind);
              if (is_error_operand(&this_pointer_operand)) {
                /* There was some problem in constructing the "this"
                   operand. */
                make_error_operand(result);
              } else {
                /* Do the field selection relative to the "this" pointer. */
                do_field_selection_operation(&this_pointer_operand,
                                             sym_ptr->class_of_which_a_member,
                                             /*is_arrow_operator=*/TRUE,
                                             expression_kind,
                                             sym_ptr,
                                             &locator_for_curr_id.
                                                               source_position,
                                             xep, result);
              }  /* if */
            }  /* if */
          }  /* if */
          break;
        case sk_member_function:
          /* Static member functions get handled like normal functions. */
          routine_ptr = sym_ptr->variant.routine;
          if (!routine_type_is_nonstatic_member_function(routine_ptr->type)) {
            goto normal_function;
          }  /* if */
          /* Nonstatic member function. */
          if (address_of_qualified_member_name) {
            /* The routine was referenced by a qualified name and is the
               immediate operand of a unary "&"; make up an operand that
               preserves the qualified name so scan_ampersand_operator can
               turn it into a pointer-to-member. */
            make_sym_for_ptr_to_member_operand(projection_sym_ptr, xep,
                                               result);
          } else {
            /* Also continue here for overloaded functions that require a
               selector expression. */
nonstatic_member_function:
            /* Make an operand for the "this" pointer. */
            make_this_pointer_operand(projection_sym_ptr,
                                      &locator_for_curr_id.source_position,
                                      &this_pointer_operand, expression_kind);
            if (is_error_operand(&this_pointer_operand)) {
              /* There was some problem in constructing the "this" operand. */
              make_error_operand(result);
            } else {
              /* Make an operand for the function bound to the "this"
                 pointer. */
              do_member_function_selection_operation(&this_pointer_operand,
                                                     sym_ptr,
                                                     &locator_for_curr_id,
                                                     xep,
                                                     result,
                                                     bound_function_selector);
            }  /* if */
          }  /* if */
          break;
        case sk_overloaded_function:
          /* Overloaded function. */
          if (address_of_qualified_member_name) {
            /* The routine was referenced by a qualified name and is the
               immediate operand of a unary "&"; make up an operand that
               preserves the qualified name so scan_ampersand_operator can
               turn it into a pointer-to-member. */
            make_sym_for_ptr_to_member_operand(projection_sym_ptr, xep,
                                               result);
          } else {
            /* We don't know the specific routine, but we may be able to tell
               whether or not the function will need a selector expression. */
            if (overloaded_function_needs_selector(sym_ptr, &maybe)) {
              /* The function needs or may need a selector expression. */
              if (!maybe) {
                /* The function definitely needs a selector expression.
                   Process it like a normal nonstatic member function. */
                goto nonstatic_member_function;
              } else if (variable_this_exists(&this_var)) {
                /* The function may need a selector expression, and there is
                   a "this" variable in the current context, so process the
                   function like a normal nonstatic member function.  The
                   "this" operand will be discarded in
                   select_overloaded_function it it turns out not to be
                   needed. */
                goto nonstatic_member_function;
              }  /* if */
              /* The function may need a selector expression, but there is
                 no "this" variable in the current context, so assume we do
                 not need one (an error will be generated in
                 select_overloaded_function -- because there will be no
                 match -- if it turns out we do). */
            }  /* if */
            /* A "this" pointer is definitely not needed or not available. */
            make_indefinite_function_operand(projection_sym_ptr,
                                             (a_boolean)locator_for_curr_id.
                                                             is_qualified_name,
                                             result);
          }  /* if */
          break;
        case sk_undefined:
          /* Symbol was found in the symbol table, but it is undefined.  This
             means that it was encountered earlier but was never turned into a
             function call.  Or, there was an access check or ambiguity error.
             No error message is issued here because one was issued earlier.
             An error operand is returned. */
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
                                                     expression_kind,
                                                     local_options);
            goto after_advance_past_id;
          } else {
            /* Otherwise, an error. */
            error_and_make_error_operand(ec_type_identifier_not_allowed,
                                         result);
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

  /* Advance past the identifier. */
  (void)get_token();
after_advance_past_id:

  /* Set the error position to the starting position. */
  copy_source_position(start_position, error_position);
  copy_source_position(start_position, result->position);

  db_exit();
}  /* scan_identifier */


static void scan_expr_full(an_operand               *result,
                           an_operand               *bound_function_selector,
                           int                      prec_level,
                           an_expression_kind       expression_kind,
                           a_local_expr_options_set local_options)
/*
Scan an expression and return it in *result.  If the expression is for
a bound function in C++, also set *bound_function_selector to indicate the
object.  prec_level indicates the precedence level that controls this
scan; scan_expr_full stops on an operator with lower precedence than
prec_level.  expression_kind indicates the kind of expression to scan;
its values include three kinds of constant expressions (preprocessing,
integral constant, and initializer), not-evaluated expressions (as for
the operand of sizeof), and normal expressions.  local_options indicates
some temporary options that apply only for the top-level expression scanned
by this routine (disallow comma operator, suppress conversion of arrays
and/or functions to pointers, etc. -- see expr.h).
*/
{
  a_source_position start_position;
  an_operand        operand;
  an_operand        local_result, local_bound_function_selector;
  a_token_kind      ntoken;

  db_enter(4, "scan_expr_full");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "precedence level = %d\n", prec_level);
  }  /* if */
#endif /* DEBUG */

  /* Save the current source position. */
  copy_source_position(pos_curr_token, start_position);

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
      scan_identifier(expression_kind, &local_result,
                      &local_bound_function_selector, local_options);
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
          /* Make an lvalue for the "this" variable. */
          make_lvalue_variable_operand(this_var, &local_result,
                                       (an_xref_entry_ptr)NULL);
          /* Turn the lvalue into an rvalue. */
          conv_lvalue_to_rvalue(&local_result, expression_kind);
        }  /* if */
      }  /* if */
      (void)get_token();
      break;
    case tok_float_constant:
      make_constant_operand(&const_for_curr_token, &local_result);
      /* Floating constants are not allowed in preprocessing expressions.
         In integral constant expressions, they are allowed only as the 
         immediate operand of a cast. */
      if ((expression_kind == (an_expression_kind)ek_pp) ||
	  (expression_kind == (an_expression_kind)ek_integral_constant &&
           !(local_options & EOPT_OPERAND_OF_CAST))) {
	error_and_make_error_operand(ec_expr_not_integral, &local_result);
      }  /* if */
      (void)get_token();
      break;
    case tok_string_literal:
      make_string_constant_operand(&const_for_curr_token, &local_result);
      if ((expression_kind == (an_expression_kind)ek_pp) ||
	  (expression_kind == (an_expression_kind)ek_integral_constant)) {
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
      scan_prefix_incr_decr(&local_result, expression_kind);
      break;

    case tok_ampersand:
      scan_ampersand_operator(&local_result, expression_kind);
      break;

    case tok_star:
      scan_indirection_operator(&local_result, expression_kind);
      break;

    case tok_plus:
    case tok_minus:
    case tok_compl:
    case tok_not:
      scan_arith_prefix_operator(&local_result, expression_kind);
      break;

    case tok_sizeof:
      /* Sizeof operation. */
      scan_sizeof_operator(&local_result, expression_kind);
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
      scan_new_operator(&local_result, expression_kind);
      break;
    case tok_delete:
scan_delete:
      /* C++ "delete" operator. */
      scan_delete_operator(&local_result, expression_kind);
      break;
    case tok_lparen:
      /* This could be a cast operation or just an expression in
         parentheses. */
      /* Come here if a left parenthesis was trapped by the caller. */
handle_trapped_left_paren:
      scan_cast_or_expr(&local_result, &local_bound_function_selector,
                        expression_kind, local_options);
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
     scan_functional_notation_type_conversion(type_keyword(),
                                              &local_result,
                                              expression_kind, local_options);
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
  /* See if the current token is an operator, and if so, whether it ends
     the current expression given its precedence and associativity. */
  while (!token_ends_expr(curr_token, prec_level, local_options)) {
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
#if CHECKING
    } else if (is_sym_for_ptr_to_member_operand(&local_result)) {
      /* An operand was created to represent a qualified name used in a
         pointer-to-member context, i.e., as the operand of "&".  However,
         we now find we have an operator of higher precedence following the
         qualified name, meaning that the qualified name is not actually
         the direct operand of the "&", as in "&A::x++".  This case should
         have been handled inside scan_identifier. */
      internal_error("scan_expr_full: sym_for_ptr_to_member operand escaped");
#endif /* CHECKING */
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
      }  /* if */
    }  /* if */

    /* Copy the operand in "local_result" to "operand" in anticipation of
       creating another intermediate result in "local_result". */
    copy_operand(&local_result, &operand);

    switch (curr_token) {
      case tok_plus_plus:
      case tok_minus_minus:
	/* Postfix increment and decrement. */
        scan_postfix_incr_decr(&operand, &local_result, expression_kind);
	break;
      case tok_lbracket:
	/* Subscript. */
        scan_subscript_operator(&operand, &local_result, expression_kind);
	break;
      case tok_lparen:
	/* Routine call. */
        scan_function_call(&operand, &local_bound_function_selector,
                           &local_result, expression_kind);
	break;
      case tok_period:
      case tok_arrow:
	/* Field selectors. */
	scan_field_selection_operator(&operand, &local_result,
                                      &local_bound_function_selector,
				      expression_kind);
	break;
      case tok_period_star:
      case tok_arrow_star:
	/* C++ pointer-to-member operators (.* and ->*). */
	scan_ptr_to_member_operator(&operand, &local_result,
                                    &local_bound_function_selector,
				    expression_kind);
        break;
      case tok_star:
      case tok_divide:
      case tok_remainder:
	scan_mult_operator(&operand, &local_result, expression_kind);
	break;
      case tok_plus:
      case tok_minus:
	scan_add_operator(&operand, &local_result, expression_kind);
	break;
      case tok_shift_left:
      case tok_shift_right:
	scan_shift_operator(&operand, &local_result, expression_kind);
	break;
      case tok_lt:
      case tok_gt:
      case tok_le:
      case tok_ge:
	scan_rel_operator(&operand, &local_result, expression_kind);
	break;
      case tok_eq:
      case tok_ne:
	scan_eq_operator(&operand, &local_result, expression_kind);
	break;
      case tok_ampersand:
      case tok_excl_or:
      case tok_or:
	scan_bit_operator(&operand, &local_result, expression_kind);
	break;
      case tok_and_and:
      case tok_or_or:
	scan_logical_operator(&operand, &local_result, expression_kind);
	break;
      case tok_quest_mark:
	scan_conditional_operator(&operand, &local_result, expression_kind);
	break;
      case tok_assign:
        scan_simple_assignment_operator(&operand, &local_result,
				        expression_kind);
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
        scan_compound_assignment_operator(&operand, &local_result,
				          expression_kind);
	break;
      case tok_comma:
	scan_comma_operator(&operand, &local_result, expression_kind);
	break;
#if CHECKING
      default:
        internal_error("scan_expr_full: bad operator token in loop");
#endif /* CHECKING */
    }  /* switch */
  }  /* while */

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
  if (local_result.bound_function) {
    /* Do not allow bound functions to survive unless the caller
       permits it. */
    if (!(local_options & EOPT_ALLOW_BOUND_FUNCTION)) {
      /* Bound function not allowed. */
      error_in_operand(ec_bound_function_must_be_called, &local_result);
    } else {
      /* Bound function allowed.  Return the operand for the object to
         which the function is bound in *bound_function_selector. */
#if CHECKING
      if (bound_function_selector == NULL) {
        internal_error("scan_expr_full: bound_function_selector == NULL");
      }  /* if */
#endif /* CHECKING */
      copy_operand(&local_bound_function_selector, bound_function_selector);
    }  /* if */
  }  /* if */

  copy_operand(&local_result, result);

  db_exit();
}  /* scan_expr_full */


an_expr_node_ptr scan_switch_expression(void)
/*
Scan an integral selector expression for a switch statement, and return
a pointer to the expression tree.
*/
{
  an_expr_node_ptr expression;
  an_operand       result;
  a_boolean        processed = FALSE;

  db_enter(3, "scan_switch_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
	    EOPT_NO_OPTIONS);
  /* Make sure it's an integer.  Convert from a class type to an integer if
     necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result,
                                                 /*integral_allowed=*/TRUE,
                                                 /*floating_allowed=*/FALSE,
                                                 /*pointer_allowed=*/FALSE,
                                                /*result_may_be_lvalue=*/FALSE,
                                                 (an_expression_kind)ek_normal,
                                                 &processed);
  }  /* if */
  if (!processed) {
    /* Non-class (i.e., normal) case. */
    do_operand_transformations(&result, TOPT_NO_OPTIONS,
                               (an_expression_kind)ek_normal);
    (void)check_integral_operand(&result);
  }  /* if */
  expression = make_node_from_operand(&result);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
Scan an expression whose result is to be thrown away.  The idea here is to
scan an expression as "scan_expression" would and determine what parts (if
any) of the expression should be discarded.  Returns a pointer to the
expression, or NULL if the expression reduces to nothing (in theory; in
practice, that doesn't happen, because no meaningful simplification is
done.  The NULL return is provided as a hook for future expansion).
This routine is not used for constant or not-evaluated expressions.
*/
{
  an_expr_node_ptr expression;
  an_operand       result;

  db_enter(3, "scan_void_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_NO_OPTIONS);
  do_operand_transformations(&result, TOPT_NO_OPTIONS,
                             (an_expression_kind)ek_normal);
  simplify_void_operand(&result);
  expression = make_node_from_operand(&result);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

#if DEBUG
  if (debug_level >= 3) {
    if (expression != NULL) {
      db_expression(expression);
    } else {
      fprintf(f_debug, "void\n");
    }  /* if */
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
  an_operand       result;
  an_expr_node_ptr node;

  db_enter(3, "scan_default_arg_expr");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_NO_OPTIONS | EOPT_DISALLOW_COMMA_OPERATOR);
  if (ptp != NULL) {
    /* Convert to the required type. */
    prep_argument_operand(&result, ptp, ec_bad_default_arg_type,
                          (an_expression_kind)ek_normal);
  } else {
    do_operand_transformations(&result, TOPT_NO_OPTIONS,
                               (an_expression_kind)ek_normal);
  }  /* if */
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();
  node = make_node_from_operand(&result);
  if (ptp != NULL) {
    ptp->default_arg_expr = node;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_expression(node);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_default_arg_expr */


an_expr_node_ptr scan_return_expression(a_type_ptr    required_type,
                                        an_error_code err_code)
/*
Scan an expression on a return statement and convert it to the type
required_type; issue the error err_code if it cannot be converted to that
type.  Return a pointer to the expression.  routine_type is the type of
the current function.
*/
{
  an_expr_node_ptr expression;
  an_operand       result;

  db_enter(3, "scan_return_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_NO_OPTIONS);
  /* Convert to the required type. */
  prep_return_operand(&result, required_type,
                      (an_expression_kind)ek_normal, err_code);
  expression = make_node_from_operand(&result);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
  an_operand result;

  db_enter(3, "scan_pp_expression");

  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_pp,
            EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS,
                             (an_expression_kind)ek_pp);
  extract_constant_from_operand(&result, constant);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
Scan an integral constant expression.  See section 3.4 in the standard.
*/
{
  an_operand result;

  db_enter(3, "scan_integral_constant_expression");

  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_integral_constant,
            EOPT_DISALLOW_COMMA_OPERATOR);
  do_operand_transformations(&result, TOPT_NO_OPTIONS,
                             (an_expression_kind)ek_integral_constant);
  extract_constant_from_operand(&result, constant);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
  an_operand result;

  db_enter(3, "scan_new_array_dimension_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_NO_OPTIONS);
  do_operand_transformations(&result, TOPT_NO_OPTIONS,
                             (an_expression_kind)ek_normal);
  /* Check that expression is integral. */
#if 0
  /* Should conversions be allowed here? */
#endif
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
      copy_constant(&result.variant.constant, constant);
      break;
#if CHECKING
    default:
      internal_error("scan_new_array_dimension_expression: bad operand kind");
#endif /* CHECKING */
  }  /* switch */
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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


void scan_constant_initializer_expression(a_type_ptr required_type,
                                          a_constant *constant)
/*
Scan a constant initializer expression.  Convert the constant to
required_type; issue an error if it is incompatible with that type.
See section 3.4 in the ANSI C standard.  Not used in C++, because
there's no such thing as a C++ initializer that must be constant.
*/
{
  an_operand result;

  db_enter(3, "scan_constant_initializer_expression");

  /* Scan the constant expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_init_constant,
            EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type,
                           (an_expression_kind)ek_init_constant,
                           ec_bad_initializer_type);
  /* Make a constant from the operand. */
  extract_constant_from_operand(&result, constant);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
  an_operand result;

  db_enter(3, "scan_initializer_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_DISALLOW_COMMA_OPERATOR);
  /* Convert to the required type. */
  prep_initializer_operand(&result, required_type,
                           (an_expression_kind)ek_normal,
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
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
  an_operand operand;

  /* Make an operand for the expression. */
  make_expression_operand(expr, expr->type, &operand);
  operand.position = *err_pos;
  /* Do the conversion. */
  prep_argument_operand(&operand, param, ec_incompatible_param,
                        (an_expression_kind)ek_normal);
  /* Make an expression again. */
  expr = make_node_from_operand(&operand);
  return expr;
}  /* prep_rvalue_arg_expr */


an_expr_node_ptr scan_class_initializer_expression(
                                             a_type_ptr    required_type,
                                             a_routine_ptr *conversion_routine,
                                             a_boolean     *class_bitwise_copy)
/*
Scan an expression that is the initial value of an entity of class type.
required_type indicates the class type (it may have some qualifiers on
top of it).  Find a constructor (possibly a copy constructor; not a conversion
routine) that will convert the expression scanned into the class type.
Return a pointer to that conversion routine in *conversion_routine, or
NULL if no such routine exists, or NULL plus *class_bitwise_copy TRUE if
a bitwise copy can be done.  Return a pointer to the expression scanned.
Actually, it's an argument list for the call of the conversion routine or
for the bitwise copy; the caller must build the call of the conversion
routine using the expression returned as the argument.  This routine is
used in both C and C++ mode for initializers for classes.  It's particularly
useful for C++ cases where copy constructor elision might be done:

  struct A { A(int) {...} A(A&) {...} };
  A x = 1;            // A::A(int)
  A y[3] = {1, 2, 3}; // A::A(int) three times
  A z = x;            // A::A(A&)

The expression scanned is considered an implicit argument of a constructor,
and is therefore kept in lvalue form if possible.
Basically, this routine determines that a type conversion can be done
(and how), but leaves it to the caller to create the code that calls
the conversion routine (which is likely to be a dynamic init entry instead
of a statement).
*/
{
  a_type_ptr       class_type = skip_typerefs(required_type);
  an_expr_node_ptr expression;
  an_operand       result;

  db_enter(3, "scan_class_initializer_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
            EOPT_DISALLOW_COMMA_OPERATOR);
  /* Find out whether or not the conversion is possible, and if so,
     what the proper conversion routine is. */
  prep_elision_initializer_operand(&result, class_type, conversion_routine,
                                   &expression, class_bitwise_copy);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

#if DEBUG
  if (debug_level >= 3) {
    db_expression(expression);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return expression;
}  /* scan_class_initializer_expression */


an_expr_node_ptr scan_boolean_controlling_expression(void)
/*
Scan an expression that is used in controlling contexts that need a boolean
result, such as if, while, do while, for, and conditional statements.  The
type of the expression must be scalar.
*/
{
  an_operand       result;
  an_expr_node_ptr expr;
  a_boolean        processed = FALSE;

  db_enter(3, "scan_boolean_controlling_expression");

  /* Scan the expression. */
  scan_expr(&result, PREC_LOWEST, (an_expression_kind)ek_normal,
	    EOPT_NO_OPTIONS);

  /* Make sure it's a scalar.  Convert from a class type to a scalar if
     necessary. */
  if (C_dialect == C_dialect_cplusplus &&
      is_class_struct_union_type(result.type)) {
    try_to_convert_class_operand_to_builtin_type(&result,
                                                 /*integral_allowed=*/TRUE,
                                                 /*floating_allowed=*/TRUE,
                                                 /*pointer_allowed=*/TRUE,
                                                /*result_may_be_lvalue=*/FALSE,
                                                 (an_expression_kind)ek_normal,
                                                 &processed);
  }  /* if */
  if (!processed) {
    do_operand_transformations(&result, TOPT_NO_OPTIONS,
                               (an_expression_kind)ek_normal);
    if (check_boolean_controlling_expr(&result)) {
      /* Issue a remark if the expression is constant.  The check is here
         instead of check_boolean_controlling_expr because we don't want
         to issue diagnostics for things like "i = 1&&2;". */
      if (is_constant_operand(&result)) {
        remark(ec_boolean_controlling_expr_is_constant);
      }  /* if */
    }  /* if */
  }  /* if */
  expr = make_node_from_operand(&result);
  /* If generating cross-reference information, flush out the references
     for the current expression now.  If we are not generating such
     information, this is harmless. */
  flush_xref_entries_list();

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
