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
               available in classic ANSI C ("C90").

*/

#if C99_IL_EXTENSIONS_SUPPORTED && DO_C99_IL_LOWERING

/* Header files common to all files. */
#include "fe_common.h"

#include "il_walk.h"
#include "lower_il.h"
#include "lower_init.h"


/* Complex arithmetic and comparison routines. */
a_routine_ptr  xnegate_routine = NULL;
a_routine_ptr  xadd_routine = NULL;
a_routine_ptr  xsubtract_routine = NULL;
a_routine_ptr  xmultiply_routine = NULL;
a_routine_ptr  xdivide_routine = NULL;
a_routine_ptr  xeq_routine = NULL;
a_routine_ptr  xne_routine = NULL;

/* Complex compound assignment routines. */
a_routine_ptr  xadd_assign_routine = NULL;
a_routine_ptr  xsubtract_assign_routine = NULL;
a_routine_ptr  xmultiply_assign_routine = NULL;
a_routine_ptr  xdivide_assign_routine = NULL;

/* Complex-to-complex conversion routines. */
a_routine_ptr  cast_cfloat_to_cdouble_routine = NULL;
a_routine_ptr  cast_cfloat_to_clong_double_routine = NULL;
a_routine_ptr  cast_cdouble_to_cfloat_routine = NULL;
a_routine_ptr  cast_cdouble_to_clong_double_routine = NULL;
a_routine_ptr  cast_clong_double_to_cfloat_routine = NULL;
a_routine_ptr  cast_clong_double_to_cdouble_routine = NULL;

/* Non-complex to complex conversion routines. */
a_routine_ptr  cast_float_to_cfloat = NULL;
a_routine_ptr  cast_double_to_cdouble = NULL;
a_routine_ptr  cast_long_double_to_clong_double = NULL;
a_routine_ptr  cast_ifloat_to_cfloat = NULL;
a_routine_ptr  cast_idouble_to_cdouble = NULL;
a_routine_ptr  cast_ilong_double_to_clong_double = NULL;

/* Complex to non-complex conversion routines. */
a_routine_ptr  cast_cfloat_to_float = NULL;
a_routine_ptr  cast_cdouble_to_double = NULL;
a_routine_ptr  cast_clong_double_to_long_double = NULL;
a_routine_ptr  cast_cfloat_to_ifloat = NULL;
a_routine_ptr  cast_cdouble_to_idouble = NULL;
a_routine_ptr  cast_clong_double_to_ilong_double = NULL;


static void lower_c99_xnegate(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("-z") into a function call (compatible
with C89).
*/
{
  a_type_ptr        return_type = expr->type;
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
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xnegate_call = make_runtime_rout_call(rout_name, &xnegate_routine,
                                        return_type,
                                        expr->variant.operation.operands);
  overwrite_node(expr, xnegate_call);
}  /* lower_c99_xnegate */


static void lower_c99_xadd(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1+z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = expr->type;
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
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xadd_call = make_runtime_rout_call(rout_name, &xadd_routine, return_type,
                                     expr->variant.operation.operands);
  overwrite_node(expr, xadd_call);
}  /* lower_c99_xadd */


static void lower_c99_xsubtract(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1-z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
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
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xsubtract_call = make_runtime_rout_call(rout_name, &xsubtract_routine,
                                          return_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xsubtract_call);
}  /* lower_c99_xsubtract */


static void lower_c99_xmultiply(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1*z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
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
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xmultiply_call = make_runtime_rout_call(rout_name, &xmultiply_routine,
                                          return_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xmultiply_call);
}  /* lower_c99_xmultiply */


static void lower_c99_xdivide(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1/z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
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
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xdivide_call = make_runtime_rout_call(rout_name, &xdivide_routine,
                                        return_type,
                                        expr->variant.operation.operands);
  overwrite_node(expr, xdivide_call);
}  /* lower_c99_xdivide */


static void lower_c99_xeq(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1==z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
  an_expr_node_ptr  xeq_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_eq";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_eq";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_eq";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xeq_call = make_runtime_rout_call(rout_name, &xeq_routine, return_type,
                                    expr->variant.operation.operands);
  overwrite_node(expr, xeq_call);
}  /* lower_c99_xeq */


static void lower_c99_xne(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1!=z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
  an_expr_node_ptr  xne_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_ne";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_ne";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_ne";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xne_call = make_runtime_rout_call(rout_name, &xne_routine, return_type,
                                    expr->variant.operation.operands);
  overwrite_node(expr, xne_call);
}  /* lower_c99_xne */


static void lower_c99_xadd_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 += z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr  return_type = expr->type;
  char        *rout_name;
  an_expr_node_ptr  xadd_assign_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_add_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_add_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_add_assign";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xadd_assign_call = make_runtime_rout_call(rout_name, &xadd_assign_routine,
                                            return_type,
                                            expr->variant.operation.operands);
  overwrite_node(expr, xadd_assign_call);
}  /* lower_c99_xadd_assign */


static void lower_c99_xsubtract_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 -= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = expr->type;
  char              *rout_name;
  an_expr_node_ptr  xsubtract_assign_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_subtract_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_subtract_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_subtract_assign";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xsubtract_assign_call = make_runtime_rout_call(
                            rout_name, &xsubtract_assign_routine, return_type,
                            expr->variant.operation.operands);
  overwrite_node(expr, xsubtract_assign_call);
}  /* lower_c99_xsubtract_assign */


static void lower_c99_xmultiply_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 *= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = expr->type;
  char              *rout_name;
  an_expr_node_ptr  xmultiply_assign_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_multiply_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_multiply_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_multiply_assign";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xmultiply_assign_call = make_runtime_rout_call(
                            rout_name, &xmultiply_assign_routine, return_type,
                            expr->variant.operation.operands);
  overwrite_node(expr, xmultiply_assign_call);
}  /* lower_c99_xmultiply_assign */


static void lower_c99_xdivide_assign(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1 /= z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = expr->type;
  char              *rout_name;
  an_expr_node_ptr  xdivide_assign_call;

  check_assertion(is_complex_type(return_type));
  switch (return_type->variant.float_kind) {
    case fk_float:
      rout_name = "__c99_complex_float_divide_assign";
      break;
    case fk_double:
      rout_name = "__c99_complex_double_divide_assign";
      break;
    case fk_long_double:
      rout_name = "__c99_complex_long_double_divide_assign";
      break;
#if CHECKING
    default:
      internal_error("invalid floating-point kind");
#endif /* CHECKING */
  }  /* switch */
  xdivide_assign_call = make_runtime_rout_call(
                              rout_name, &xdivide_assign_routine, return_type,
                              expr->variant.operation.operands);
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
  a_type_ptr        src_type = src->type, dst_type = expr->type;
  a_routine_ptr     *routine;
  char              *routine_name;

  if (il_identical_types(src_type, dst_type) || is_void_type(dst_type)) {
    /* Nothing needs to be done. */
  } else if (is_complex_type(dst_type)) {
    if (is_complex_type(src_type)) {
      /* A change in floating-point precision. */
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
#if CHECKING
            default:
              internal_error("invalid floating-point kind");
#endif /* CHECKING */
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
#if CHECKING
            default:
              internal_error("invalid floating-point kind");
#endif /* CHECKING */
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
#if CHECKING
            default:
              internal_error("invalid floating-point kind");
#endif /* CHECKING */
          }  /* switch */
          break;
#if CHECKING
          default:
            internal_error("invalid floating-point kind");
#endif /* CHECKING */
      }  /* switch */
      cast_call = make_runtime_rout_call(routine_name, routine, dst_type, src);
    } else if (is_imaginary_type(src_type)) {
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
#if CHECKING
        default:
          internal_error("invalid floating-point kind");
#endif /* CHECKING */
      }  /* switch */
      /* Before creating a complex value, be sure the imaginary value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                  float_type(dst_type->variant.float_kind));
      cast_call = make_runtime_rout_call(routine_name, routine, dst_type, src);
    } else {
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
#if CHECKING
        default:
          internal_error("invalid floating-point kind");
#endif /* CHECKING */
      }  /* switch */
      /* Before creating a complex value, be sure the real value is cast
         to the needed precision. */
      src = add_cast_if_necessary(src,
                                  float_type(dst_type->variant.float_kind));
      cast_call = make_runtime_rout_call(routine_name, routine, dst_type, src);
    }  /* if */
    overwrite_node(expr, cast_call);
  } else if (is_imaginary_type(dst_type)) {
    /* Converting a complex value to an imaginary type.  This amounts to
       keeping the imaginary part of the given value. */
    if (is_complex_type(src_type)) {
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
#if CHECKING
        default:
          internal_error("invalid floating-point kind");
#endif /* CHECKING */
      }  /* switch */
      cast_call = make_runtime_rout_call(
                           routine_name, routine,
                           imaginary_type(src_type->variant.float_kind), src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else if (is_imaginary_type(src_type)) {
    } else {
    }  /* if */
  } else {
    /* Converting a complex value to a real type.  This amounts to keeping the
       real part of the given value. */
    if (is_complex_type(src_type)) {
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
#if CHECKING
        default:
          internal_error("invalid floating-point kind");
#endif /* CHECKING */
      }  /* switch */
      cast_call = make_runtime_rout_call(
                           routine_name, routine,
                           imaginary_type(src_type->variant.float_kind), src);
      cast_call = add_cast_if_necessary(cast_call, dst_type);
      overwrite_node(expr, cast_call);
    } else if (is_imaginary_type(src_type)) {
    } else {
    }  /* if */
  }  /* if */
}  /* lower_c99_complex_cast */


void lower_c99_operator(an_expr_node_ptr  expr)
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
      lower_c99_complex_cast(expr);
      break;
    default:
      /* Nothing needs to be done. */
      break;
  }  /* switch */
}  /* lower_c99_operator */


static void lower_c99_imaginary_type(a_float_kind  kind,
                                     char          *name)
{
  a_type_ptr  im_type = imaginary_type(kind);

  set_type_kind(im_type, (a_type_kind)tk_typeref);
  im_type->variant.typeref.type = float_type(kind);
  im_type->source_corresp.name = alloc_il(strlen(name)+1);
  strcpy(im_type->source_corresp.name, name);
  add_to_types_list(im_type, DEPTH_OF_FILE_SCOPE);
}  /* lower_c99_imaginary_type */


static void lower_c99_complex_type(a_float_kind  kind,
                                   char          *name)
{
  a_type_ptr   cmplx_type = complex_type(kind);
  a_type_ptr   array_type;
  a_field_ptr  last_field = NULL;

  /* Create a struct type with a layout similar to that of the complex type. */
  set_type_kind(cmplx_type, (a_type_kind)tk_struct);
  cmplx_type->source_corresp.name = alloc_il(strlen(name)+1);
  strcpy(cmplx_type->source_corresp.name, name);
  /* Create a type "array for two real values". */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 2;
  array_type->variant.array.element_type = float_type(kind);
  set_type_size(array_type);
  /* Add the field. */
  make_lowered_field("_Vals", array_type, cmplx_type, &last_field);
  finish_class_type(cmplx_type);
  add_to_front_of_file_scope_types_list(cmplx_type);
#if MAINTAIN_NEEDED_FLAGS
  set_class_keep_definition_in_il(cmplx_type);
#endif /* MAINTAIN_NEEDED_FLAGS */
}  /* lower_c99_complex_type */


void lower_c99_nonreal_float_types(void)
{
  lower_c99_imaginary_type((a_float_kind)fk_float, "_Imaginary_float");
  lower_c99_imaginary_type((a_float_kind)fk_double, "_Imaginary_double");
  lower_c99_imaginary_type((a_float_kind)fk_long_double,
                           "_Imaginary_long_double");
  lower_c99_complex_type((a_float_kind)fk_float, "_Complex_float");
  lower_c99_complex_type((a_float_kind)fk_double, "_Complex_double");
  lower_c99_complex_type((a_float_kind)fk_long_double, "_Complex_long_double");
}  /*  */

#endif /* C99_IL_EXTENSIONS_SUPPORTED && DO_C99_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
