/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

lower_c99.c -- Routines to transform C99 IL constructs into constructs
               available in classic ANSI/ISO C ("C89").

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_C99_IL_LOWERING

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
/* Additional header files. */
#include "lower_c99.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/* Forward declarations (needed because of mutual recursion situations). */
static void lower_c99_dynamic_init(a_dynamic_init_ptr dip);
static void lower_c99_constant_list(a_constant_ptr constant_list);
static void lower_c99_statement(a_statement_ptr statement);

/* Pointers to lowered versions of complex types, once allocated. */
static a_type_ptr lowered_complex_float = NULL;
static a_type_ptr lowered_complex_double = NULL;
static a_type_ptr lowered_complex_long_double = NULL;


static a_type_ptr make_lowered_complex_type(a_float_kind  fkind,
                                            char          *name)
/*
Create a struct type with the given name to represent a complex type of the
given precision.  The struct contains a single field that is an array of two
floating point elements.
*/
{
  a_type_ptr   result = alloc_type((a_type_kind)tk_struct);
  a_type_ptr   array_type;
  a_field_ptr  last_field = NULL;

  result->source_corresp.name = alloc_il(strlen(name)+1);
  strcpy(result->source_corresp.name, name);
  /* Create a type "array of two real values". */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 2;
  array_type->variant.array.element_type = float_type(fkind);
  set_type_size(array_type);
  /* Add the field. */
  make_lowered_field("_Vals", array_type, result, &last_field);
  finish_class_type(result);
  return result;
}  /* make_lowered_complex_type */


static a_type_ptr lowered_complex_type(a_float_kind fkind)
/*
Return the structure used to represent a complex type of the kind fkind in
lowered IL.
*/
{
  a_type_ptr  result = NULL;

  switch (fkind) {
    case fk_float:
      if (lowered_complex_float == NULL) {
        lowered_complex_float = make_lowered_complex_type(
                                                     fkind, "_Complex_float");
      }  /* if */
      result = lowered_complex_float;
      break;
    case fk_double:
      if (lowered_complex_double == NULL) {
        lowered_complex_double = make_lowered_complex_type(
                                                    fkind, "_Complex_double");
      }  /* if */
      result = lowered_complex_double;
      break;
    case fk_long_double:
      if (lowered_complex_long_double == NULL) {
        lowered_complex_long_double = make_lowered_complex_type(
                                               fkind, "_Complex_long_double");
      }  /* if */
      result = lowered_complex_long_double;
      break;
    default:
      unexpected_condition_str("lowered_complex_type: invalid float kind");
  }  /* switch */
  return result;
}  /* lowered_complex_type */


/* Complex arithmetic and comparison routines. */
static a_routine_ptr  xnegate_routine = NULL;
static a_routine_ptr  xadd_routine = NULL;
static a_routine_ptr  xsubtract_routine = NULL;
static a_routine_ptr  xmultiply_routine = NULL;
static a_routine_ptr  xdivide_routine = NULL;
static a_routine_ptr  xeq_routine = NULL;
static a_routine_ptr  xne_routine = NULL;

/* Complex compound assignment routines. */
static a_routine_ptr  xadd_assign_routine = NULL;
static a_routine_ptr  xsubtract_assign_routine = NULL;
static a_routine_ptr  xmultiply_assign_routine = NULL;
static a_routine_ptr  xdivide_assign_routine = NULL;

/* Complex-to-complex conversion routines. */
static a_routine_ptr  cast_cfloat_to_cdouble_routine = NULL;
static a_routine_ptr  cast_cfloat_to_clong_double_routine = NULL;
static a_routine_ptr  cast_cdouble_to_cfloat_routine = NULL;
static a_routine_ptr  cast_cdouble_to_clong_double_routine = NULL;
static a_routine_ptr  cast_clong_double_to_cfloat_routine = NULL;
static a_routine_ptr  cast_clong_double_to_cdouble_routine = NULL;

/* Non-complex to complex conversion routines. */
static a_routine_ptr  cast_float_to_cfloat = NULL;
static a_routine_ptr  cast_double_to_cdouble = NULL;
static a_routine_ptr  cast_long_double_to_clong_double = NULL;
static a_routine_ptr  cast_ifloat_to_cfloat = NULL;
static a_routine_ptr  cast_idouble_to_cdouble = NULL;
static a_routine_ptr  cast_ilong_double_to_clong_double = NULL;

/* Complex to non-complex conversion routines. */
static a_routine_ptr  cast_cfloat_to_float = NULL;
static a_routine_ptr  cast_cdouble_to_double = NULL;
static a_routine_ptr  cast_clong_double_to_long_double = NULL;
static a_routine_ptr  cast_cfloat_to_ifloat = NULL;
static a_routine_ptr  cast_cdouble_to_idouble = NULL;
static a_routine_ptr  cast_clong_double_to_ilong_double = NULL;


static an_expr_node_ptr make_prototyped_runtime_call(
                                               char             *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               a_type_ptr       param1_type,
                                               a_type_ptr       param2_type,
                                               an_expr_node_ptr arg_expr_list)
/*
Create a call node with arguments given by arg_expr_list to a runtime routine.
The called routine is *routine and is created with the given name and types if
*routine is NULL (*routine is updated to point to the new routine).  Parameters
can be left out by passing NULL parameter types (e.g., a non-NULL param1_type
and a NULL param2_type creates a prototype for a function taking a single
argument).
*/
{
  an_expr_node_ptr  result;
  if (*routine == NULL) {
    /* Make the routine entry if it does not exist already. */
    a_type_ptr        rout_type;
    (void)make_runtime_routine(name, routine, return_type);
    rout_type = (*routine)->type;
    /* Prototype parameter list. */
    rout_type->variant.routine.extra_info->prototyped = TRUE;
    if (param1_type != NULL) {
      a_param_type_ptr  first_param = alloc_param_type(param1_type);
      rout_type->variant.routine.extra_info->param_type_list = first_param;
      if (param2_type != NULL) {
        first_param->next = alloc_param_type(param2_type);
      }  /* if */
    } else {
      check_assertion(param2_type == NULL);
    }  /* if */
  }  /* if */
  /* Make the call node. */
  result = make_call_node(*routine, arg_expr_list, /*honor_virtual=*/FALSE,
                          (an_insert_location *)NULL);
  return result;
}  /* make_prototyped_runtime_call */


static void lower_c99_xnegate(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("-z") into a function call (compatible
with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  char              *rout_name;
  an_expr_node_ptr  xnegate_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_negate";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_negate";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_negate";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xnegate_call = make_prototyped_runtime_call(
                                            rout_name, &xnegate_routine,
                                            return_type,
                                            return_type, /*param2_type=*/NULL,
                                            expr->variant.operation.operands);
  overwrite_node(expr, xnegate_call);
}  /* lower_c99_xnegate */


static void lower_c99_xadd(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1+z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  char              *rout_name;
  an_expr_node_ptr  xadd_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_add";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_add";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_add";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xadd_call = make_prototyped_runtime_call(rout_name, &xadd_routine,
                                           return_type,
                                           return_type, return_type,
                                           expr->variant.operation.operands);
  overwrite_node(expr, xadd_call);
}  /* lower_c99_xadd */


static void lower_c99_xsubtract(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1-z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  char              *rout_name;
  an_expr_node_ptr  xsubtract_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_subtract";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_subtract";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_subtract";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xsubtract_call = make_prototyped_runtime_call(
                                            rout_name, &xsubtract_routine,
                                            return_type,
                                            return_type, return_type,
                                            expr->variant.operation.operands);
  overwrite_node(expr, xsubtract_call);
}  /* lower_c99_xsubtract */


static void lower_c99_xmultiply(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1*z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  char              *rout_name;
  an_expr_node_ptr  xmultiply_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_multiply";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_multiply";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_multiply";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xmultiply_call = make_prototyped_runtime_call(
                                            rout_name, &xmultiply_routine,
                                            return_type,
                                            return_type, return_type,
                                            expr->variant.operation.operands);
  overwrite_node(expr, xmultiply_call);
}  /* lower_c99_xmultiply */


static void lower_c99_xdivide(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1/z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  char              *rout_name;
  an_expr_node_ptr  xdivide_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_divide";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_divide";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_divide";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xdivide_call = make_prototyped_runtime_call(
                                            rout_name, &xdivide_routine,
                                            return_type,
                                            return_type, return_type,
                                            expr->variant.operation.operands);
  overwrite_node(expr, xdivide_call);
}  /* lower_c99_xdivide */


static void lower_c99_xeq(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1==z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  char              *rout_name;
  an_expr_node_ptr  xeq_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_eq";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_eq";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_eq";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xeq_call = make_prototyped_runtime_call(rout_name, &xeq_routine, return_type,
                                          op_type, op_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xeq_call);
}  /* lower_c99_xeq */


static void lower_c99_xne(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1!=z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  char              *rout_name;
  an_expr_node_ptr  xne_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_ne";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_ne";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_ne";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xne_call = make_prototyped_runtime_call(rout_name, &xne_routine, return_type,
                                          op_type, op_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xne_call);
}  /* lower_c99_xne */


static void lower_c99_xadd_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 += z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        op_type = expr->variant.operation.operands->next->type;
  char              *rout_name;
  an_expr_node_ptr  xadd_assign_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_add_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_add_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_add_assign";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xadd_assign_call = make_prototyped_runtime_call(
                                       rout_name, &xadd_assign_routine,
                                       op_type,
                                       expr->variant.operation.operands->type,
                                       op_type,
                                       expr->variant.operation.operands);
  xadd_assign_call = add_cast_if_necessary(xadd_assign_call, expr->type);
  overwrite_node(expr, xadd_assign_call);
}  /* lower_c99_xadd_assign */


static void lower_c99_xsubtract_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 -= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        op_type = expr->variant.operation.operands->next->type;
  char              *rout_name;
  an_expr_node_ptr  xsubtract_assign_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_subtract_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_subtract_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_subtract_assign";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xsubtract_assign_call = make_prototyped_runtime_call(
                                       rout_name, &xsubtract_assign_routine,
                                       op_type,
                                       expr->variant.operation.operands->type,
                                       op_type,
                                       expr->variant.operation.operands);
  xsubtract_assign_call = add_cast_if_necessary(xsubtract_assign_call,
                                                expr->type);
  overwrite_node(expr, xsubtract_assign_call);
}  /* lower_c99_xsubtract_assign */


static void lower_c99_xmultiply_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 *= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        op_type = expr->variant.operation.operands->next->type;
  char              *rout_name;
  an_expr_node_ptr  xmultiply_assign_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_multiply_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_multiply_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_multiply_assign";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xmultiply_assign_call = make_prototyped_runtime_call(
                                       rout_name, &xmultiply_assign_routine,
                                       op_type,
                                       expr->variant.operation.operands->type,
                                       op_type,
                                       expr->variant.operation.operands);
  xmultiply_assign_call = add_cast_if_necessary(xmultiply_assign_call,
                                                expr->type);
  overwrite_node(expr, xmultiply_assign_call);
}  /* lower_c99_xmultiply_assign */


static void lower_c99_xdivide_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 /= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        op_type = expr->variant.operation.operands->next->type;
  char              *rout_name;
  an_expr_node_ptr  xdivide_assign_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  switch (op_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_divide_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_divide_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_divide_assign";
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  xdivide_assign_call = make_prototyped_runtime_call(
                                       rout_name, &xdivide_assign_routine,
                                       op_type,
                                       expr->variant.operation.operands->type,
                                       op_type,
                                       expr->variant.operation.operands);
  xdivide_assign_call = add_cast_if_necessary(xdivide_assign_call, expr->type);
  overwrite_node(expr, xdivide_assign_call);
}  /* lower_c99_xdivide_assign */


static void lower_c99_jmultiply(an_expr_node_ptr  expr)
/*
Turn the given multiplication of two imaginary values into a real multiply
followed by a sign inversion ( (__I__*a)*(__I__*b) = -(a*b) ).
*/
{
  expr->variant.operation.operands =
                  make_operator_node((an_expr_operator_kind)eok_fmultiply,
                                     expr->type,
                                     expr->variant.operation.operands);
  expr->variant.operation.kind = (an_expr_operator_kind)eok_fnegate;
}  /* lower_c99_jmultiply */


static void lower_c99_complex_cast(an_expr_node_ptr  expr)
/*
Transform the given cast expression into a function call (compatible with C89).
*/
{
  an_expr_node_ptr  src = expr->variant.operation.operands, cast_call;
  a_type_ptr        src_type = skip_typerefs(src->type);
  a_type_ptr        dst_type = skip_typerefs(expr->type);
  a_routine_ptr     *routine;
  char              *routine_name;

  if (il_identical_types(src_type, dst_type) || is_void_type(dst_type)) {
    /* Nothing needs to be done. */
  } else if (is_complex_type(dst_type)) {
    if (is_complex_type(src_type)) {
      /* A change in floating-point precision, complex to complex. */
      switch (src_type->variant.float_kind) {
        case fk_float:
          switch (dst_type->variant.float_kind) {
            case fk_double:
              routine_name = "__c99_cfloat_to_cdouble";
              routine = &cast_cfloat_to_cdouble_routine;
              break;
            case fk_long_double:
              routine_name = "__c99_cfloat_to_clong_double";
              routine = &cast_cfloat_to_clong_double_routine;
              break;
            default:
              unexpected_condition_str("invalid floating-point kind");
          }  /* switch */
          break;
        case fk_double:
          switch (dst_type->variant.float_kind) {
            case fk_float:
              routine_name = "__c99_cdouble_to_cfloat";
              routine = &cast_cdouble_to_cfloat_routine;
              break;
            case fk_long_double:
              routine_name = "__c99_cdouble_to_clong_double";
              routine = &cast_cdouble_to_clong_double_routine;
              break;
            default:
              unexpected_condition_str("invalid floating-point kind");
          }  /* switch */
          break;
        case fk_long_double:
          switch (dst_type->variant.float_kind) {
            case fk_float:
              routine_name = "__c99_clong_double_to_cfloat";
              routine = &cast_clong_double_to_cfloat_routine;
              break;
            case fk_double:
              routine_name = "__c99_clong_double_to_cdouble";
              routine = &cast_clong_double_to_cdouble_routine;
              break;
            default:
              unexpected_condition_str("invalid floating-point kind");
          }  /* switch */
          break;
          default:
            unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    } else if (is_imaginary_type(src_type)) {
      /* Convert imaginary to complex. */
      /* Create a new complex value 0.0 + x*__I__. */
      switch (dst_type->variant.float_kind) {
        case fk_float:
          routine_name = "__c99_ifloat_to_cfloat";
          routine = &cast_ifloat_to_cfloat;
          break;
        case fk_double:
          routine_name = "__c99_idouble_to_cdouble";
          routine = &cast_idouble_to_cdouble;
          break;
        case fk_long_double:
          routine_name = "__c99_ilong_double_to_clong_double";
          routine = &cast_ilong_double_to_clong_double;
          break;
        default:
          unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      /* Before creating a complex value, be sure the imaginary value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                  float_type(dst_type->variant.float_kind));
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    } else {
      /* Convert float to complex. */
      /* Create a new complex value x + 0.0*__I__. */
      switch (dst_type->variant.float_kind) {
        case fk_float:
          routine_name = "__c99_float_to_cfloat";
          routine = &cast_float_to_cfloat;
          break;
        case fk_double:
          routine_name = "__c99_double_to_cdouble";
          routine = &cast_double_to_cdouble;
          break;
        case fk_long_double:
          routine_name = "__c99_long_double_to_clong_double";
          routine = &cast_long_double_to_clong_double;
          break;
        default:
          unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      /* Before creating a complex value, be sure the real value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                  float_type(dst_type->variant.float_kind));
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    }  /* if */
    overwrite_node(expr, cast_call);
  } else if (is_imaginary_type(dst_type)) {
    if (is_complex_type(src_type)) {
      /* Converting a complex value to an imaginary type.  This amounts to
         keeping the imaginary part of the given value. */
      switch (src_type->variant.float_kind) {
        case fk_float:
          routine_name = "__c99_cfloat_to_ifloat";
          routine = &cast_cfloat_to_ifloat;
          break;
        case fk_double:
          routine_name = "__c99_cdouble_to_idouble";
          routine = &cast_cdouble_to_idouble;
          break;
        case fk_long_double:
          routine_name = "__c99_clong_double_to_ilong_double";
          routine = &cast_clong_double_to_ilong_double;
          break;
        default:
          unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      cast_call = make_prototyped_runtime_call(
                           routine_name, routine,
                           imaginary_type(src_type->variant.float_kind),
                           src->type, (a_type_ptr)NULL, src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else {
      /* Nothing to be done (imaginary->imaginary becomes real->real once the
         imaginary types have been lowered, and real->imaginary becomes
         real->real once the imaginary types have been lowered). */
    }  /* if */
  } else {
    if (is_complex_type(src_type)) {
      /* Converting a complex value to a real type.  This amounts to keeping
         the real part of the given value. */
      switch (src_type->variant.float_kind) {
        case fk_float:
          routine_name = "__c99_cfloat_to_float";
          routine = &cast_cfloat_to_float;
          break;
        case fk_double:
          routine_name = "__c99_cdouble_to_double";
          routine = &cast_cdouble_to_double;
          break;
        case fk_long_double:
          routine_name = "__c99_clong_double_to_long_double";
          routine = &cast_clong_double_to_long_double;
          break;
        default:
          unexpected_condition_str("invalid floating-point kind");
      }  /* switch */
      cast_call = make_prototyped_runtime_call(
                           routine_name, routine,
                           imaginary_type(src_type->variant.float_kind),
                           src->type, (a_type_ptr)NULL, src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else {
      /* Nothing to be done (imaginary->real becomes real->real once the
         imaginary types have been lowered, and real->real is already
         correct). */
    }  /* if */
  }  /* if */
}  /* lower_c99_complex_cast */


static void lower_c99_operator(an_expr_node_ptr  expr)
/*
The given expression should be an operation.  If it is a complex or an
imaginary operation, replace it by IL that is compatible with C89 IL.
Otherwise, do nothing.
*/
{
  check_assertion(expr->kind == (an_expr_node_kind)enk_operation);
  switch (expr->variant.operation.kind) {
    case eok_xnegate:
      lower_c99_xnegate(expr);
      break;
    case eok_xadd:
      lower_c99_xadd(expr);
      break;
    case eok_xsubtract:
      lower_c99_xsubtract(expr);
      break;
    case eok_xmultiply:
      lower_c99_xmultiply(expr);
      break;
    case eok_xdivide:
      lower_c99_xdivide(expr);
      break;
    case eok_xeq:
      lower_c99_xeq(expr);
      break;
    case eok_xne:
      lower_c99_xne(expr);
      break;
    case eok_xassign:
      expr->variant.operation.kind = (an_expr_operator_kind)eok_sassign;
      break;
    case eok_xadd_assign:
      lower_c99_xadd_assign(expr);
      break;
    case eok_xsubtract_assign:
      lower_c99_xsubtract_assign(expr);
      break;
    case eok_xmultiply_assign:
      lower_c99_xmultiply_assign(expr);
      break;
    case eok_xdivide_assign:
      lower_c99_xdivide_assign(expr);
      break;
    case eok_jmultiply:
      lower_c99_jmultiply(expr);
      break;
    case eok_cast:
      if (is_nonreal_floating_type(expr->type) ||
          is_nonreal_floating_type(expr->variant.operation.operands->type)) {
        lower_c99_complex_cast(expr);
      }  /* if */
      break;
    default:
      /* Nothing needs to be done. */
      break;
  }  /* switch */
}  /* lower_c99_operator */


static void lower_c99_complex_constant(a_constant_ptr  constant)
/*
Replace the given ck_complex constant by a ck_aggregate constant structure
that can initialize a lowered complex variable.  (Since complex constants are
allocated in file scope, the lowered structure must also be placed there.)
*/
{
  a_constant_ptr  real_part, imag_part, pair;
  a_float_kind    fkind = skip_typerefs(constant->type)->variant.float_kind;
  a_type_ptr      lowered_type = lowered_complex_type(fkind);

  real_part = fs_constant((a_constant_repr_kind)ck_float);
  real_part->type = float_type(fkind);
  memcpy(&real_part->variant.float_value,
         &constant->variant.complex_value->real,
         sizeof(an_internal_float_value));
  imag_part = fs_constant((a_constant_repr_kind)ck_float);
  imag_part->type = float_type(fkind);
  memcpy(&imag_part->variant.float_value,
         &constant->variant.complex_value->imag,
         sizeof(an_internal_float_value));
  real_part->next = imag_part;

  pair = fs_constant((a_constant_repr_kind)ck_aggregate);
  pair->type = lowered_type->variant.class_struct_union.field_list->type;
  check_assertion(is_array_type(pair->type));
  pair->variant.aggregate.first_constant = real_part;
  pair->variant.aggregate.last_constant = imag_part;

  set_constant_kind(constant, (a_constant_repr_kind)ck_aggregate);
  constant->type = lowered_type;
  constant->variant.aggregate.first_constant = pair;
  constant->variant.aggregate.last_constant = pair;
}  /* lower_c99_complex_constant */


static void lower_c99_constant(a_constant_ptr  constant)
/*
If the given constant contains C99-specific constructs (like _Complex values),
replace them by a representation compatible with C89.
*/
{
  switch (constant->kind) {
    case ck_complex:
      lower_c99_complex_constant(constant);
      break;
    case ck_aggregate:
      lower_c99_constant_list(constant->variant.aggregate.first_constant);
      break;
    case ck_imaginary:
      /* Represent the constant as a regular floating-point constant.
         Its type will similarly be adjusted. */
      constant->kind = (a_constant_repr_kind)ck_float;
      break;
    case ck_address:
      switch (constant->variant.address.kind) {
        case abk_routine:
          /* Routines will be visited from the scope. */
          break;
        case abk_variable:
          /* Variables will be visited from the scope. */
          break;
        case abk_constant:
          /* Nothing to be done (appears only for addresses of string
             constants). */
          break;
        default:
          unexpected_condition_str("Bad c99 address const kind");
      }  /* switch */
      break;
    case ck_dynamic_init:
      lower_c99_dynamic_init(constant->variant.dynamic_init);
      break;
    case ck_init_repeat:
      lower_c99_constant(constant->variant.init_repeat.constant);
      /* FIXME?  Move lowering code here? */
      break;
    case ck_designator:
      /* This is always encountered while processing a list of constants in
         an aggregate constant. */
      /* FIXME?  Move lowering code here? */
      break;
    case ck_error:
    case ck_integer:
    case ck_float:
    case ck_string:
      /* Nothing to be done. */
      break;
    default:
      unexpected_condition_str("Invalid C99 constant");
      break;
  }  /* switch */
}  /* lower_c99_constant */


static void lower_c99_constant_expr(an_expr_node_ptr  expr)
/*
Transform the given expression to remove certain C99-specific constructs.
*/
{
  if (is_imaginary_type(expr->type)) {
    /* Turn the imaginary constant into a real floating point constant. */
    lower_c99_constant(expr->variant.constant);
  } else if (is_complex_type(expr->type)) {
    /* Replace this node by a reference to a static variable initialized
       with an aggregate representing the constant complex value. */
    a_variable_ptr  tmp;
    a_constant_ptr  constant = expr->variant.constant;
    if (constant->source_corresp.assoc_info == NULL) {
      /* No static variable was created for this constant yet. */
      tmp = make_temporary_in_scope(expr->type,
                                    scope_stack[DEPTH_OF_FILE_SCOPE].il_scope,
                                    /*force_static=*/FALSE);
      tmp->init_kind = (an_init_kind)initk_static;
      tmp->initializer.constant = constant;
      lower_c99_constant(tmp->initializer.constant);
      constant->source_corresp.assoc_info = (char*)tmp;
    } else {
      /* Reuse the previously created temporary. */
      tmp = (a_variable_ptr)constant->source_corresp.assoc_info;
    }  /* if */
    overwrite_node(expr, var_rvalue_expr(tmp));
  }  /* if */
}  /* lower_c99_constant_expr */


static void lower_c99_temp_init(an_expr_node_ptr  expr)
/*
Transform the given enk_temp_init expression to remove certain C99-specific
constructs.
*/
{
  lower_c99_dynamic_init(expr->variant.init.dynamic_init);
}  /* lower_c99_temp_init */


/* Macro interface to lower_c99_expr_full for the normal case. */
#define lower_c99_expr(expr) lower_c99_expr_full(expr, (a_statement_ptr)NULL)

#if !MINIMAL_INLINING
/*ARGSUSED*/ /* <-- statement is not used in this case. */
#endif /* !MINIMAL_INLINING */
static void lower_c99_expr_full(an_expr_node_ptr  expr,
                                a_statement_ptr   statement)
/*
Transform the given expression to remove certain C99-specific constructs.
If statement is non-NULL, expr is the expression of the expression
statement statement.
*/
{
  an_expr_node_ptr  operand;

  switch (expr->kind) {
    case enk_operation:
      /* First lower all the operands (if any). */
      for (operand = expr->variant.operation.operands;
           operand != NULL;
           operand = operand->next) {
        lower_c99_expr(operand);
      }  /* if */
      /* Then transform the current operator if needed. */
      lower_c99_operator(expr);
#if MINIMAL_INLINING
      if (expr->variant.operation.kind == (an_expr_operator_kind)eok_call) {
        /* Do inlining of a call if appropriate. */
        if (inlining_enabled) do_inlining_of_call(expr, statement);
      }  /* if */
#endif /* MINIMAL_INLINING */
      break;
    case enk_constant:
      lower_c99_constant_expr(expr);
      break;
    case enk_temp_init:
      lower_c99_temp_init(expr);
      break;
    case enk_routine_address:
    case enk_variable:
    case enk_variable_address:
    case enk_field:
      /* Nothing to be done. */
      break;
    case enk_runtime_sizeof:
      if (!expr->variant.runtime_sizeof.is_type) {
        lower_c99_expr(expr->variant.runtime_sizeof.variant.expr);
      }  /* if */
      break;
    default:
      unexpected_condition_str("Invalid C99 IL expression kind");
      break;
  }  /* switch */
}  /* lower_c99_expr_full */


static void lower_c99_dynamic_init(a_dynamic_init_ptr dip)
/*
Do C99 lowering on the indicated dynamic initialization entry.
*/
{
  switch (dip->kind) {
    case dik_constant:
      lower_c99_constant(dip->variant.constant);
      break;
    case dik_expression:
      lower_c99_expr(dip->variant.expression);
      break;
    default:
      unexpected_condition_str("lower_c99_dynamic_init: bad kind");
  }  /* switch */
}  /* lower_c99_dynamic_init */


static void lower_c99_constant_list(a_constant_ptr constant_list)
/*
Do C99 lowering on a constant list.
*/
{
  a_constant_ptr constant;

  for (constant = constant_list;
       constant != NULL;
       constant = constant->next) {
    lower_c99_constant(constant);
  }  /* if */
}  /* lower_c99_constant_list */


static void lower_c99_statement_list(a_statement_ptr statement_list)
/*
Do C99 lowering on the indicated statement list.
*/
{
  a_statement_ptr statement;

  for (statement = statement_list;
       statement != NULL;
       statement = statement->next) {
    lower_c99_statement(statement);
  }  /* for */
}  /* lower_c99_statement_list */


static void lower_c99_statement(a_statement_ptr statement)
/*
Do C99 lowering on the indicated statement.
*/
{
  a_source_position saved_error_position;

  /* Set the error position to the statement position, in case there is
     an error in lowering. */
  saved_error_position = error_position;
  set_position_from_stmt_source_position(error_position, statement->position);
  if (statement->expr != NULL) {
    /* Lower the expression.  For an expression statement, pass the
       statement pointer to allow better inlining. */
    lower_c99_expr_full(statement->expr,
                        (statement->kind == (a_statement_kind)stmk_expr) ?
                                            statement : (a_statement_ptr)NULL);
  }  /* if */
  switch (statement->kind) {
    case stmk_expr:
    case stmk_goto:
    case stmk_label:
    case stmk_return:
    case stmk_init:
    case stmk_asm:
#if ASM_FUNCTION_ALLOWED
    case stmk_asm_func_body:
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    case stmk_decl:
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    case stmk_set_vla_size:
    case stmk_vla_decl:
    case stmk_vla_dealloc:
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
    case stmk_empty:
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
      /* Nothing to lower. */
      break; 
    case stmk_if:
      lower_c99_statement(statement->variant.if_stmt.then_statement);
      if (statement->variant.if_stmt.else_statement != NULL) {
        lower_c99_statement(statement->variant.if_stmt.else_statement);
      }  /* if */
      break;
    case stmk_while:
    case stmk_end_test_while:
      lower_c99_statement(statement->variant.loop_statement);
      break;
    case stmk_for:
      { a_for_loop_ptr flp = statement->variant.for_loop.extra_info;
        if (flp->initialization != NULL) {
          lower_c99_statement(flp->initialization);
        }  /* if */
        if (flp->increment != NULL) {
          lower_c99_expr(flp->increment);
        }  /* if */
      }
      lower_c99_statement(statement->variant.for_loop.statement);
      break;
    case stmk_block:
      { a_context context;
        a_scope_ptr scope = statement->variant.block.extra_info->assoc_scope;
        if (scope != NULL) {
          /* The block has an associated scope.  Push it. */
          push_context(&context, scope, (an_object_lifetime_ptr)NULL);
        }  /* if */
        lower_c99_statement_list(statement->variant.block.statements);
        if (scope != NULL) pop_context();
      }
      break;
    case stmk_switch:
      { a_switch_clause_ptr scp;
        /* Walk the switch clause list. */
        for (scp = statement->variant.switch_stmt.clause_list;
             scp != NULL;
             scp = scp->next) {
          lower_c99_constant_list(scp->constant_list);
          lower_c99_statement_list(scp->statements);
        }  /* for */
      }
      lower_c99_statement(statement->variant.switch_stmt.body_statement);
      break;
    default:
      unexpected_condition_str("lower_c99_statement: bad statement kind");
  }  /* switch */
  error_position = saved_error_position;
}  /* lower_c99_statement */


static void lower_c99_initializer(an_init_kind   init_kind,
                                  an_initializer *initializer)
/*
Do C99 lowering for an initializer (e.g., from a variable).  init_kind
indicates the kind of initialization, and *initializer provides the details.
*/
{
  switch (init_kind) {
    case initk_static:
      /* The initializer is a constant. */
      lower_c99_constant(initializer->constant);
      break;
    case initk_dynamic:
      /* The initializer is dynamic. */
      lower_c99_dynamic_init(initializer->dynamic);
      break;
    case initk_function_local:
      /* Local static variable inits are handled at the scope level. */
      break;
    case initk_none:
      /* Nothing to be done. */
      break;
    case initk_zero:
      /* Should only appear when lowering C++ IL. */
    default:
      unexpected_condition_str("lower_c99_initializer: bad init kind");
  }  /* switch */
}  /* lower_c99_initializer */


static void lower_c99_variable(a_variable_ptr var)
/*
Do C99 lowering on the indicated variable and its subtree.
*/
{
  a_source_position saved_error_position;

  /* Set the error position to the variable position, in case there is
     an error in lowering. */
  saved_error_position = error_position;
  error_position = var->source_corresp.decl_position;
  lower_c99_initializer(var->init_kind, &var->initializer);
  error_position = saved_error_position;
}  /* lower_c99_variable */
  

static void lower_c99_scope(a_scope_ptr scope)
/*
Do C99 lowering for all entities in and under the given scope.
*/
{
  a_variable_ptr                   variable;
  a_scope_ptr                      block_scope;
  a_vla_dimension_ptr              vla_dim;
  a_local_static_variable_init_ptr lsvip;
  a_context                        context;

  push_context(&context, scope, (an_object_lifetime_ptr)NULL);
  switch (scope->kind) {
    case sck_file:
    case sck_block:
      /* Nothing to lower. */
      break;
    case sck_function:
      /* Lower all parameters. */
      for (variable = scope->variant.routine.parameters;
           variable != NULL;
           variable = variable->next) {
        lower_c99_variable(variable);
      }  /* for */
      break;
    default:
      unexpected_condition_str("lower_c99_scope: bad scope kind");
  }  /* switch */
  /* Visit all nonstatic variables. */
  for (variable = scope->nonstatic_variables;
       variable != NULL;
       variable = variable->next) {
    lower_c99_variable(variable);
  }  /* for */
  /* Visit all static variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    lower_c99_variable(variable);
  }  /* for */
  /* Visit all block scopes (only present in function and block scopes). */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    lower_c99_scope(block_scope);
  }  /* for */
  /* Visit all VLA dimension expressions. */
  for (vla_dim = scope->vla_dimensions;
       vla_dim != NULL;
       vla_dim = vla_dim->next) {
    lower_c99_expr(vla_dim->dimension_expr);
  }  /* for */
  /* Visit all initializers for local static variables. */
  for (lsvip = scope->local_static_variable_inits;
       lsvip != NULL;
       lsvip = lsvip->next) {
    lower_c99_initializer(lsvip->init_kind, &lsvip->initializer);
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_function) {
    /* Lower the function block statement. */
    lower_c99_statement(scope->assoc_block);
#if MINIMAL_INLINING
    if (inlining_enabled && scope->variant.routine.ptr->is_inline) {
      /* For an inline routine, set the inlinable flag now that the body has
         been processed. */
      set_up_routine_for_inlining(scope);
    }  /* if */
#endif /* MINIMAL_INLINING */
  } else if (scope->kind == (a_scope_kind)sck_file) {
#if MINIMAL_INLINING
    if (inlining_enabled) {
      /* For any inline routines for which all calls were expanded inline,
         mark the routines as being unreferenced. */
      mark_inlined_routines_as_unreferenced();
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
  pop_context();
}  /* lower_c99_scope */


static void lower_c99_imaginary_type(a_float_kind  kind,
                                     char          *name)
/*
Lower the C99 imaginary type whose precision is given by kind.
The lowered form is a typedef to one of the floating-point types.
The lowered type is given the name indicated by "name".
*/
{
  if (imaginary_type_used(kind)) {
    a_type_ptr  im_type = imaginary_type(kind);

    set_type_kind(im_type, (a_type_kind)tk_typeref);
    im_type->variant.typeref.type = float_type(kind);
    im_type->source_corresp.name = alloc_il(strlen(name)+1);
    strcpy(im_type->source_corresp.name, name);
    add_to_types_list(im_type, DEPTH_OF_FILE_SCOPE);
  }  /* if */
}  /* lower_c99_imaginary_type */


static void lower_c99_complex_type(a_float_kind  kind,
                                   char          *name)
/*
Lower the C99 complex type whose precision is given by kind (if it was used).
The lowered form is a typedef to a struct containing an array of
two floating-point values of the appropriate kind.
The lowered type is given the name indicated by "name".
*/
{
  if (complex_type_used(kind)) {
    a_type_ptr   cmplx_type = complex_type(kind);
    a_type_ptr   lowered_repr = lowered_complex_type(kind);

    /* Typedef the complex type to its lowered representation. */
    set_type_kind(cmplx_type, (a_type_kind)tk_typeref);
    cmplx_type->source_corresp.name = alloc_il(strlen(name)+1);
    strcpy(cmplx_type->source_corresp.name, name);
    cmplx_type->variant.typeref.type = lowered_repr;
#if MAINTAIN_NEEDED_FLAGS
    /* Ensure it is kept in the IL. */
    mark_as_needed((char *)lowered_repr, iek_type);
    set_class_definition_needed_flag(lowered_repr);
    set_class_keep_definition_in_il(lowered_repr);
    mark_as_needed((char *)cmplx_type, iek_type);
#endif /* MAINTAIN_NEEDED_FLAGS */
    /* Link the types into the IL (in the right order). */
    add_to_front_of_file_scope_types_list(cmplx_type);
    add_to_front_of_file_scope_types_list(cmplx_type->variant.typeref.type);
  }  /* if */
}  /* lower_c99_complex_type */


static void lower_c99_nonreal_float_types(void)
/*
Replace the imaginary and complex C99 types by their lowered representations.
*/
{
  lower_c99_imaginary_type((a_float_kind)fk_float, "_Imaginary_float");
  lower_c99_imaginary_type((a_float_kind)fk_double, "_Imaginary_double");
  lower_c99_imaginary_type((a_float_kind)fk_long_double,
                           "_Imaginary_long_double");
  lower_c99_complex_type((a_float_kind)fk_float, "_Complex_float");
  lower_c99_complex_type((a_float_kind)fk_double, "_Complex_double");
  lower_c99_complex_type((a_float_kind)fk_long_double, "_Complex_long_double");
}  /* lower_c99_nonreal_float_types */


static void lower_c99_bool_type(void)
/*
Replace the C99 _Bool type by its lowered representation.
*/
{
  if (bool_type_used()) {
    a_type_ptr type = bool_type();
    /* Clear the bool flag and make this a simple integral type. */
    type->variant.integer.bool_type = FALSE;
  }  /* if */
}  /* lower_c99_bool_type */


void lower_c99_il_memory_region(a_scope_ptr scope)
/*
Do C99 lowering for a memory region.  scope is the top-level scope for
the memory region, i.e., either the file scope or a function scope.
*/
{
  il_lowering_underway = TRUE;
  lower_c99_scope(scope);
  if (scope->kind == (a_scope_kind)sck_file) {
    lower_c99_nonreal_float_types();
    lower_c99_bool_type();
  }  /* if */
  il_lowering_underway = FALSE;
}  /* lower_c99_il_memory_region */


void lower_c99_one_time_init(void)
/*
Do one-time initialization of variables related to C99 IL lowering.
(Variables that need to be reinitialized with each new translation unit
are handled in lower_c99_init.)
*/
{
  /* Save variables from lower_c99.c that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(lowered_complex_float),
      pch_saved_var_array_elem(lowered_complex_double),
      pch_saved_var_array_elem(lowered_complex_long_double),
      pch_saved_var_array_elem(xnegate_routine),
      pch_saved_var_array_elem(xadd_routine),
      pch_saved_var_array_elem(xsubtract_routine),
      pch_saved_var_array_elem(xmultiply_routine),
      pch_saved_var_array_elem(xdivide_routine),
      pch_saved_var_array_elem(xadd_assign_routine),
      pch_saved_var_array_elem(xsubtract_assign_routine),
      pch_saved_var_array_elem(xmultiply_assign_routine),
      pch_saved_var_array_elem(xdivide_assign_routine),
      pch_saved_var_array_elem(cast_cfloat_to_cdouble_routine),
      pch_saved_var_array_elem(cast_cfloat_to_clong_double_routine),
      pch_saved_var_array_elem(cast_cdouble_to_cfloat_routine),
      pch_saved_var_array_elem(cast_cdouble_to_clong_double_routine),
      pch_saved_var_array_elem(cast_clong_double_to_cfloat_routine),
      pch_saved_var_array_elem(cast_clong_double_to_cdouble_routine),
      pch_saved_var_array_elem(cast_float_to_cfloat),
      pch_saved_var_array_elem(cast_double_to_cdouble),
      pch_saved_var_array_elem(cast_long_double_to_clong_double),
      pch_saved_var_array_elem(cast_ifloat_to_cfloat),
      pch_saved_var_array_elem(cast_idouble_to_cdouble),
      pch_saved_var_array_elem(cast_ilong_double_to_clong_double),
      pch_saved_var_array_elem(cast_cfloat_to_float),
      pch_saved_var_array_elem(cast_cdouble_to_double),
      pch_saved_var_array_elem(cast_clong_double_to_long_double),
      pch_saved_var_array_elem(cast_cfloat_to_ifloat),
      pch_saved_var_array_elem(cast_cdouble_to_idouble),
      pch_saved_var_array_elem(cast_clong_double_to_ilong_double),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_one_time_init();
#endif /* MINIMAL_INLINING */
}  /* lower_c99_one_time_init */


void lower_c99_init(void)
/*
Initialize static variables related to C99 IL lowering.  This is done as a
subroutine (rather than relying on static initialization) so that it can be
redone to compile more than one source file in a single invocation of the
front end.
*/
{
  xnegate_routine = NULL;
  xadd_routine = NULL;
  xsubtract_routine = NULL;
  xmultiply_routine = NULL;
  xdivide_routine = NULL;
  xeq_routine = NULL;
  xne_routine = NULL;
  xadd_assign_routine = NULL;
  xsubtract_assign_routine = NULL;
  xmultiply_assign_routine = NULL;
  xdivide_assign_routine = NULL;
  cast_cfloat_to_cdouble_routine = NULL;
  cast_cfloat_to_clong_double_routine = NULL;
  cast_cdouble_to_cfloat_routine = NULL;
  cast_cdouble_to_clong_double_routine = NULL;
  cast_clong_double_to_cfloat_routine = NULL;
  cast_clong_double_to_cdouble_routine = NULL;
  cast_float_to_cfloat = NULL;
  cast_double_to_cdouble = NULL;
  cast_long_double_to_clong_double = NULL;
  cast_ifloat_to_cfloat = NULL;
  cast_idouble_to_cdouble = NULL;
  cast_ilong_double_to_clong_double = NULL;
  cast_cfloat_to_float = NULL;
  cast_cdouble_to_double = NULL;
  cast_clong_double_to_long_double = NULL;
  cast_cfloat_to_ifloat = NULL;
  cast_cdouble_to_idouble = NULL;
  cast_clong_double_to_ilong_double = NULL;
  lowered_complex_float = NULL;
  lowered_complex_double = NULL;
  lowered_complex_long_double = NULL;

#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_init();
#endif /* MINIMAL_INLINING */
  /* The following is also cleared in il_lower_init, but clear it here also
     to be sure. */
  il_lowering_underway = FALSE;
}  /* lower_c99_init */

#endif /* DO_C99_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
