/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2004 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

lower_c99.c -- Routines to transform C99 IL constructs into constructs
               available in classic ANSI/ISO C ("C89").  Some GNU C
               extensions are also lowered here.

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
#include "exprutil.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */

/* Forward declarations (needed because of mutual recursion situations). */
static void lower_c99_constant_list(a_constant_ptr constant_list);
static void lower_c99_statement(a_statement_ptr statement);
#if LOWER_FIXED_POINT
static void lower_c99_fixed_point_constant(a_constant_ptr constant);
static void lower_c99_fixed_point_operation(an_expr_node_ptr expr);
#endif /* LOWER_FIXED_POINT */

#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS

static a_type_list_entry_ptr
		vla_types;
			/* A list of all the VLA type entries.  The types are
			   collected as we traverse the IL for lowering.  The
			   actual lowering occurs later when we no longer need
			   the VLA expression information. */


static void record_vla_type_for_lowering(a_type_ptr  tp)
/*
The given type must be a VLA-based type.  Add it to the list of types to be
lowered later on.
*/
{
  a_type_list_entry_ptr  entry = alloc_type_list_entry();

  entry->type = tp;
  entry->next = vla_types;
  vla_types = entry;
}  /* record_vla_type_for_lowering */


/*ARGSUSED*/  /* <-- end_traversal is not used. */
static a_boolean ttt_record_vla_type_for_lowering(a_type_ptr  tp,
                                                  a_boolean   *end_traversal)
/*
If the given type is a VLA type, record it for later lowering.  This is a
routine meant to be used with traverse_type_tree.  It always returns FALSE
(so the whole type is traversed).  It takes advantage of a dedicated flag
in a_type entries to avoid visiting any type node more than once.
*/
{
  if (tp->visited_for_vla_lowering) {
    *end_traversal = TRUE;
  } else {
    tp->visited_for_vla_lowering = TRUE;
    if (tp->kind == (a_type_kind)tk_array && is_vla_type(tp)) {
      /* VLA types will be lowered to pointers to the underlying element
         type. */
      record_vla_type_for_lowering(tp);
    } else if (tp->kind == (a_type_kind)tk_pointer) {
      /* Pointers to VLA types must be lowered to pointers to the element type
         of the VLA. */
      a_type_ptr  tptp = type_pointed_to(tp);
      if (is_vla_type(tptp)) {
        record_vla_type_for_lowering(tp);
      }  /* if */
    }  /* if */
  }  /* if */
  return FALSE;
}  /* ttt_record_vla_type_for_lowering */


static void record_vla_component_types_for_lowering(a_type_ptr  tp)
/*
Go through the types underlying the given type and record any VLA components
for later lowering.  Stop at typedefs since variably modified typedefs should
have been treated separately.
*/
{
  a_type_tree_traversal_flag_set  tt_flags = TTT_STOP_AT_TYPEDEFS |
                                             TTT_RETURN_TYPE |
                                             TTT_PARAM_TYPES |
                                             TTT_THIS_PARAM_TYPE |
                                             TTT_TEMPLATE_ARGS |
                                             TTT_EXCEPTION_SPECS;

  (void)traverse_type_tree(tp, ttt_record_vla_type_for_lowering, tt_flags);
}  /* record_vla_component_types_for_lowering */


static void lower_vla_types(void)
/*
Lower all VLA types that were recorded by record_vla_type_for_lowering.
*/
{
  a_type_list_entry_ptr  entry;

  /* VLAs and pointer to VLAs must be lowered to pointers to the underlying
     element type.  To avoid ordering problems due to two types being lowered
     depending on one another, this is done in two passes.  In the first
     pass the underlying element type is brought up.  In the second pass,
     array types are turned into pointer types. */
  for (entry = vla_types; entry != NULL; entry = entry->next) {
    if (entry->type->kind == (a_type_kind)tk_array) {
      /* A VLA type to be lowered. */
      entry->type->variant.array.element_type =
                                    underlying_array_element_type(entry->type);
    } else if (entry->type->kind == (a_type_kind)tk_pointer) {
      /* This should be a pointer-to-VLA type: Make it point to the underlying
         element type (unless a previous iteration of this loop already removed
         the VLA component of this type).  That is all that is needed for this
         case: The second pass will not further transform the type. */
      a_type_ptr  tp = type_pointed_to(entry->type);
      entry->type->variant.pointer.type = underlying_array_element_type(tp);
    } else if (entry->type->kind == (a_type_kind)tk_typeref) {
      check_assertion(typeref_is_typedef(entry->type));
      entry->type->variant.typeref.has_variably_modified_type = FALSE;
    }  /* if */
  }  /* for */
  for (entry = vla_types; entry != NULL; entry = entry->next) {
    if (entry->type->kind == (a_type_kind)tk_array) {
      /* The previous pass made sure the element type of this array is not
         itself an array and conversely that no other array types have this
         array type as element type.  We can now safely turn the array into
         a pointer type. */
      *entry->type =
                   *make_pointer_type(entry->type->variant.array.element_type);
    }  /* if */
  }  /* for */
  free_list_of_type_list_entries(vla_types);
}  /* lower_vla_types */


static a_variable_ptr vla_dimension_variable(a_type_ptr        tp,
                                             an_expr_node_ptr  *inits,
                                             a_boolean         *new_var)
/*
Return a variable representing the total number of elements in the VLA type tp.
If there is no such variable yet, create one, assign to it the expression
recorded in the a_vla_dimension entry associated with tp, and add the
assignment to the given expression tree (which could be NULL initially).
*new_var is set to TRUE if a new variable was created and to FALSE otherwise.
*/
{
  a_vla_dimension_ptr  vla_dim = find_vla_dimension(tp);

  if (vla_dim->total_number_of_elements == NULL) {
    a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
    an_expr_node_ptr  expr = vla_dim->dimension_expr, assign_ops;
    /* Create a new temporary variable and assign to it the expression
       computing the array length. */
    vla_dim->total_number_of_elements = make_lowered_temporary(ptrdiff_type);
    assign_ops = var_lvalue_expr(vla_dim->total_number_of_elements);
    assign_ops->next = add_cast_if_necessary(expr, ptrdiff_type);
    assign_ops = make_operator_node((an_expr_operator_kind)eok_iassign,
                                    ptrdiff_type, assign_ops);
    if (*inits == NULL) {
      *inits = assign_ops;
    } else {
      *inits = make_comma_node(*inits, assign_ops);
    }  /* if */
    *new_var = TRUE;
  } else {
    *new_var = FALSE;
  }  /* if */
  return vla_dim->total_number_of_elements;
}  /* vla_dimension_variable */


static an_expr_node_ptr lower_vla_dimensions(a_type_ptr  tp)
/*
Create helper variables for the significant VLA components of the given type
and set them to the appropriate values.  Specifically, each variable is to
hold the total number or elements of the associated VLA.  The generated
assignments are aggregated via comma operators and returned as a single
expression (NULL if no variables needed to be created).

For example, a declaration like
	int (*p)[n][2*n][3][4*n][5];
will cause the creation of three variables (say _D1, _D2, and _D3) initialized
as follows:
	_D1 = n,
	_D2 = 2*n,
	_D3 = 4*n
followed by the following updates to count the total number of elements at
each level:
	_D3 *= 5,
	_D2 *= 3*_D3,
	_D1 *= _D2  // _D1 now holds the total number of elements in *p
Arranging the computations in this way provides the quantity needed to compute
any storage to be allocated for a VLA variable, but the other computed
variables also make indexing into the VLA arrays more efficient.
*/
{
  an_expr_node_ptr  inits = NULL, accums = NULL;
  a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);

  /* There may be multiple multilevel VLA components in a type.  For example:
       int (*const (a[n][n]))[m][m];
     The outer loop deals with that possibility. */
  while (is_variably_modified_type(tp)) {
    a_variable_ptr  dim_var;
    a_boolean       dim_var_created;
    /* Skip over pointer and type qualifier components.  If we encounter a
       typedef we can stop since variably modified typedefs have their own
       stmk_decl that would have caused this processing for the underlying
       type already.  For this reason we cannot easily call is_vla_type since
       it ignores typedefs. */
    for (;;) {
      if (tp->kind == (a_type_kind)tk_pointer) {
        tp = tp->variant.pointer.type;
      } else if (tp->kind == (a_type_kind)tk_routine) {
        tp = tp->variant.routine.return_type;
      } else if (tp->kind == (a_type_kind)tk_typeref) {
        if (typeref_is_typedef(tp)) {
          goto done;
        } else {
          tp = tp->variant.typeref.type;
        }  /* if */
      } else {
        /* Since tp was a variably-modified type, the only remaining case is
           tk_array.  Top-level non-VLA array components can be skipped (e.g.,
           "int[2][3][n][m][6]" can be handled as "int[n][m][6]". */
        check_assertion(tp->kind == (a_type_kind)tk_array);
        if (tp->variant.array.is_vla) {
          if (tp->variant.array.has_assoc_vla_dimension) {
            /* The normal case. */
            break;
          } else {
            /* A [*] dimension.  This dimension requires no work, but the
               underlying type may or may not contain more components to
               lower. */
            tp = tp->variant.array.element_type;
            goto skip_to_next_component;
          }  /* if */
        } else {
          tp = tp->variant.array.element_type;
        }  /* if */
      }  /* if */
    }  /* for */
    /* tp should now point to a VLA type: Create and initialize the variable
       associated with the top-level dimension. */
    dim_var = vla_dimension_variable(tp, &inits, &dim_var_created);
    tp = skip_typerefs(tp->variant.array.element_type);
    do {
      a_targ_size_t  constant_factor;
      if (!dim_var_created) {
        /* We ran into a VLA type that already has an updated variable.
           (Presumably as part of a typedef.) No more components need
           processing. */
        goto done;
      }  /* if */
      /* Look for additional dimension and create/update dimension variables
         accordingly. */
      constant_factor = 1;
      while (tp->kind == (a_type_kind)tk_array && !tp->variant.array.is_vla) {
        constant_factor *= tp->variant.array.variant.number_of_elements;
        tp = skip_typerefs(tp->variant.array.element_type);
      }  /* while */
      if (constant_factor == 1 && tp->kind != (a_type_kind)tk_array) {
        /* A bottom-level VLA (e.g., T[n]): No adjustment needed. */
        break;
      } else {
        an_expr_node_ptr  const_expr = NULL, var_expr = NULL, lhs, rhs, acc;
        lhs = var_lvalue_expr(dim_var);
        if (constant_factor != 1) {
          const_expr =
            node_for_host_large_integer((a_host_large_integer)constant_factor,
                                        targ_ptrdiff_t_int_kind);
        }  /* if */
        if (tp->kind == (a_type_kind)tk_array) {
          dim_var = vla_dimension_variable(tp, &inits, &dim_var_created);
          tp = skip_typerefs(tp->variant.array.element_type);
          var_expr = var_rvalue_expr(dim_var);
        }  /* if */
        if (var_expr != NULL && const_expr != NULL) {
          /* Multiply the constant and variable parts. */
          var_expr->next = const_expr;
          rhs = make_operator_node((an_expr_operator_kind)eok_imultiply,
                                   ptrdiff_type, var_expr);
        } else if (var_expr != NULL) {
          rhs = var_expr;
        } else {
          rhs = const_expr;
        }  /* if */
        /* Now multiply the variable associated with vla_dim with the rhs
           value. */
        lhs->next = rhs;
        acc = make_operator_node((an_expr_operator_kind)eok_imultiply_assign,
                                 ptrdiff_type, lhs);
        /* Insert the accumulation expression in the right location. */
        if (accums == NULL) {
          accums = acc;
        } else {
          accums = make_comma_node(acc, accums);
        }  /* if */
      }  /* if */
    }  while (tp->kind == (a_type_kind)tk_array);
skip_to_next_component:;
  }  /* while */
done:
  if (inits != NULL) {
    if (accums != NULL) {
      inits = make_comma_node(inits, accums);
    }  /* if */
  }  /* if */
  return inits;
}  /* lower_vla_dimensions */

#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_COMPLEX || (VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS)

static an_expr_node_ptr make_prototyped_runtime_call_full(
                                               char             *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               a_type_ptr       param1_type,
                                               a_type_ptr       param2_type,
                                               a_type_ptr       param3_type,
                                               an_expr_node_ptr arg_expr_list)
/*
Create a call node to a runtime routine with arguments given by arg_expr_list.
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
        if (param3_type != NULL) {
          first_param->next->next = alloc_param_type(param3_type);
        }  /* if */
      } else {
        check_assertion(param3_type == NULL);
      }  /* if */
    } else {
      check_assertion(param2_type == NULL && param3_type == NULL);
    }  /* if */
  }  /* if */
  /* Make the call node. */
  result = make_call_node(*routine, arg_expr_list, /*honor_virtual=*/FALSE,
                          (an_insert_location *)NULL);
  return result;
}  /* make_prototyped_runtime_call_full */


static an_expr_node_ptr make_prototyped_runtime_call(
                                               char             *name,
                                               a_routine_ptr    *routine,
                                               a_type_ptr       return_type,
                                               a_type_ptr       param1_type,
                                               a_type_ptr       param2_type,
                                               an_expr_node_ptr arg_expr_list)
/*
Version of make_prototyped_runtime_call that handles one or two parameter
types.
*/
{
  an_expr_node_ptr result;

  result = make_prototyped_runtime_call_full(name, routine, return_type,
                                             param1_type, param2_type,
                                             (a_type_ptr)NULL, arg_expr_list);
  return result;
}  /* make_prototyped_runtime_call */
 
#endif /* LOWER_COMPLEX || (VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS) */
#if LOWER_COMPLEX

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

  result->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
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


static a_field_ptr complex_vals_field(a_type_ptr ctype)
/*
ctype is a complex type, possibly lowered.  Return a pointer to the
single field in the struct for the lowered version of the type.
*/
{
  a_field_ptr field;

  ctype = skip_typerefs(ctype);
  if (ctype->kind == (a_type_kind)tk_complex) {
    /* Not lowered yet.  Substitute the proper lowered type. */
    ctype = lowered_complex_type(ctype->variant.float_kind);
  }  /* if */
  check_assertion(ctype->kind == (a_type_kind)tk_struct);
  field = ctype->variant.class_struct_union.field_list;
  check_assertion(field != NULL && field->next == NULL);
  return field;
}  /* complex_vals_field */


/* Complex arithmetic and comparison routines. */
static a_routine_ptr  xnegate_routine[(int)fk_last];
static a_routine_ptr  xadd_routine[(int)fk_last];
static a_routine_ptr  xsubtract_routine[(int)fk_last];
static a_routine_ptr  xmultiply_routine[(int)fk_last];
static a_routine_ptr  xdivide_routine[(int)fk_last];
static a_routine_ptr  xeq_routine[(int)fk_last];
static a_routine_ptr  xne_routine[(int)fk_last];

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


static char* select_name_from_float_kind(a_float_kind  fkind,
                                         char          *names[3])
/*
Return one of the three given strings depending on the given floating-point
precision.
*/
{
  char  *result;

  switch (fkind) {
    case fk_float:
      result = names[0];
      break;
    case fk_double:
      result = names[1];
      break;
    case fk_long_double:
      result = names[2];
      break;
    default:
      unexpected_condition_str("invalid floating-point kind");
  }  /* switch */
  return result;
}  /* select_name_from_float_kind */


/* Names of the complex negate runtime routines. */
static char *xnegate_routine_name[3] = {"__c99_complex_float_negate",
                                        "__c99_complex_double_negate",
                                        "__c99_complex_long_double_negate"};


static void lower_c99_xnegate(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("-z") into a function call (compatible
with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xnegate_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xnegate_routine_name);
  xnegate_call = make_prototyped_runtime_call(
                                rout_name, &xnegate_routine[(int)fkind],
                                return_type, return_type, (a_type_ptr)NULL,
                                expr->variant.operation.operands);
  overwrite_node(expr, xnegate_call);
}  /* lower_c99_xnegate */


/* Names of the complex add runtime routines. */
static char *xadd_routine_name[3] = {"__c99_complex_float_add",
                                     "__c99_complex_double_add",
                                     "__c99_complex_long_double_add"};


static void lower_c99_xadd(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1+z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xadd_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xadd_routine_name);
  xadd_call = make_prototyped_runtime_call(
                                        rout_name, &xadd_routine[(int)fkind],
                                        return_type, return_type, return_type,
                                        expr->variant.operation.operands);
  overwrite_node(expr, xadd_call);
}  /* lower_c99_xadd */


/* Names of the complex subtract runtime routines. */
static char *xsubtract_routine_name[3] = {
                                         "__c99_complex_float_subtract",
                                         "__c99_complex_double_subtract",
                                         "__c99_complex_long_double_subtract"};


static void lower_c99_xsubtract(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1-z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xsubtract_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xsubtract_routine_name);
  xsubtract_call = make_prototyped_runtime_call(
                                     rout_name, &xsubtract_routine[(int)fkind],
                                     return_type, return_type, return_type,
                                     expr->variant.operation.operands);
  overwrite_node(expr, xsubtract_call);
}  /* lower_c99_xsubtract */


/* Names of the complex multiply runtime routines. */
static char *xmultiply_routine_name[3] = {
                                         "__c99_complex_float_multiply",
                                         "__c99_complex_double_multiply",
                                         "__c99_complex_long_double_multiply"};


static void lower_c99_xmultiply(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1*z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xmultiply_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xmultiply_routine_name);
  xmultiply_call = make_prototyped_runtime_call(
                                     rout_name, &xmultiply_routine[(int)fkind],
                                     return_type, return_type, return_type,
                                     expr->variant.operation.operands);
  overwrite_node(expr, xmultiply_call);
}  /* lower_c99_xmultiply */


/* Names of the complex divide runtime routines. */
static char *xdivide_routine_name[3] = {
                                        "__c99_complex_float_divide",
                                        "__c99_complex_double_divide",
                                        "__c99_complex_long_double_divide"};


static void lower_c99_xdivide(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1/z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xdivide_call;

  check_assertion(is_complex_type(return_type));
  fkind = return_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xdivide_routine_name);
  xdivide_call = make_prototyped_runtime_call(
                                       rout_name, &xdivide_routine[(int)fkind],
                                       return_type, return_type, return_type,
                                       expr->variant.operation.operands);
  overwrite_node(expr, xdivide_call);
}  /* lower_c99_xdivide */


/* Names of the complex == runtime routines. */
static char *xeq_routine_name[3] = {"__c99_complex_float_eq",
                                    "__c99_complex_double_eq",
                                    "__c99_complex_long_double_eq"};


static void lower_c99_xeq(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1==z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xeq_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  fkind = op_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xeq_routine_name);
  xeq_call = make_prototyped_runtime_call(rout_name, &xeq_routine[(int)fkind],
                                          return_type, op_type, op_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xeq_call);
}  /* lower_c99_xeq */


/* Names of the complex != runtime routines. */
static char *xne_routine_name[3] = {"__c99_complex_float_ne",
                                    "__c99_complex_double_ne",
                                    "__c99_complex_long_double_ne"};


static void lower_c99_xne(an_expr_node_ptr  expr)
/*
Transform the given complex expression ("z1!=z2") into a function call
(compatible with C89).
*/
{
  a_type_ptr        return_type = skip_typerefs(expr->type);
  a_type_ptr        op_type = expr->variant.operation.operands->type;
  a_float_kind      fkind;
  char              *rout_name;
  an_expr_node_ptr  xne_call;

  op_type = skip_typerefs(op_type);
  check_assertion(is_complex_type(op_type));
  fkind = op_type->variant.float_kind;
  rout_name = select_name_from_float_kind(fkind, xne_routine_name);
  xne_call = make_prototyped_runtime_call(rout_name, &xne_routine[(int)fkind],
                                          return_type, op_type, op_type,
                                          expr->variant.operation.operands);
  overwrite_node(expr, xne_call);
}  /* lower_c99_xne */


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


static void lower_c99_jdivide(an_expr_node_ptr  expr)
/*
Turn the given division of a real value by an imaginary value into a real
division followed by a sign inversion ( a/(b*__I__) = -(a/b)*__I__ ).
*/
{
  expr->variant.operation.operands =
                  make_operator_node((an_expr_operator_kind)eok_fdivide,
                                     expr->type,
                                     expr->variant.operation.operands);
  expr->variant.operation.kind = (an_expr_operator_kind)eok_fnegate;
}  /* lower_c99_jdivide */


static void lower_real_imag_add_subtract(an_expr_node_ptr expr)
/*
Lower a mixed real/imaginary add/subtract operation, i.e.,

  eok_fjadd      real      + imaginary
  eok_jfadd      imaginary + real
  eok_fjsubtract real      - imaginary
  eok_jfsubtract imaginary - real

The code for these assembles a complex value from the two parts,
negating one part in the "-" case.
*/
{
  a_variable_ptr   temp_var = make_lowered_temporary(expr->type);
  an_expr_node_ptr real_part_lvalue, imag_part_lvalue;
  an_expr_node_ptr operand_1, operand_2, assign_1, assign_2, comma_node;
  a_field_ptr      vals_field = complex_vals_field(expr->type);
  a_type_ptr       ptr_to_elem_type;

  /* We will assign the proper values to the components in the temporary,
     then use the temporary as the result. */
  /* Make "*(temp._Vals)" as the lvalue for the real part. */
  real_part_lvalue = field_lvalue_selection_expr(var_lvalue_expr(temp_var),
                                                 vals_field);
  ptr_to_elem_type = type_after_array_to_pointer_transformation(
                                                             vals_field->type);
  real_part_lvalue = add_cast(real_part_lvalue, ptr_to_elem_type);
  /* Make "temp._Vals[1]" as the lvalue for the imaginary part. */
  imag_part_lvalue = field_lvalue_selection_expr(var_lvalue_expr(temp_var),
                                                 vals_field);
  imag_part_lvalue = add_cast(imag_part_lvalue, ptr_to_elem_type);
  imag_part_lvalue->next = node_for_integer_constant((long)1,
                                                     targ_ptrdiff_t_int_kind);
  imag_part_lvalue = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                        ptr_to_elem_type, imag_part_lvalue);
  operand_1 = expr->variant.operation.operands;
  operand_2 = operand_1->next;
  operand_1->next = NULL;
  switch (expr->variant.operation.kind) {
    case eok_fjadd:
      /* Real + imaginary.
           (temp.real = operand_1, temp.imag = operand_2)
      */
      real_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_1->type, real_part_lvalue);
      imag_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_2->type, imag_part_lvalue);
      break;
    case eok_jfadd:
      /* Imaginary + real.
           (temp.imag = operand_1, temp.real = operand_2)
      */
      imag_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_1->type, imag_part_lvalue);
      real_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_2->type, real_part_lvalue);
      break;
    case eok_fjsubtract:
      /* Real - imaginary.
           (temp.real = operand_1, temp.imag = -operand_2)
      */
      real_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_1->type, real_part_lvalue);
      operand_2 = make_operator_node((an_expr_operator_kind)eok_fnegate,
                                     operand_2->type, operand_2);
      imag_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_2->type, imag_part_lvalue);
      break;
    case eok_jfsubtract:
      /* Imaginary - real.
           (temp.imag = operand_1, temp.real = -operand_2)
      */
      imag_part_lvalue->next = operand_1;
      assign_1 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_1->type, imag_part_lvalue);
      operand_2 = make_operator_node((an_expr_operator_kind)eok_fnegate,
                                     operand_2->type, operand_2);
      real_part_lvalue->next = operand_2;
      assign_2 = make_operator_node((an_expr_operator_kind)eok_fassign,
                                    operand_2->type, real_part_lvalue);
      break;
    default:
      unexpected_condition_str("lower_real_imag_add_subtract: bad operator");
  }  /* switch */
  /* Combine the two assignments with a comma. */
  comma_node = make_comma_node(assign_1, assign_2);
  /* Add another comma to return the temporary as the result of the
     operation.  This node is created by overwriting the original node. */
  comma_node->next = var_rvalue_expr(temp_var);
  set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                    expr->type, comma_node);
}  /* lower_real_imag_add_subtract */


static void lower_c99_complex_cast(an_expr_node_ptr  expr)
/*
Transform the given complex cast expression into a function call
(compatible with C89).
*/
{
  an_expr_node_ptr  src = expr->variant.operation.operands, cast_call;
  a_type_ptr        src_type = skip_typerefs(src->type);
  a_type_ptr        dst_type = skip_typerefs(expr->type);
  a_routine_ptr     *routine;
  char              *routine_name;

  if (is_void_type(dst_type)) {
    /* A cast to void.  Nothing needs to be done. */
  } else if (il_identical_types(src_type, dst_type)) {
    /* A do-nothing cast. */
    if (is_imaginary_type(src_type)) {
      /* Imaginary types become floating-point types, so the cast can be
         left as it is.  This may actually be useful/necessary, because
         such a cast will drop extra precision on intermediate results. */
    } else {
      /* A cast to a complex type is eliminated because it would become
         a cast to struct type. */
      overwrite_node(expr, src);
    }  /* if */
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
                                 imaginary_type(dst_type->variant.float_kind));
      cast_call = make_prototyped_runtime_call(
                                         routine_name, routine,
                                         dst_type, src->type, (a_type_ptr)NULL,
                                         src);
    } else {
      /* Convert floating-point, fixed-point, or integral to complex. */
      check_assertion(is_arithmetic_or_enum_type(src_type));
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
    } else if (is_real_floating_type(src_type) ||
#if FIXED_POINT_ALLOWED
               is_fixed_point_type(src_type) ||
#endif /* FIXED_POINT_ALLOWED */
               is_integral_or_enum_type(src_type)) {
      /* A real, fixed-point, or integral value converted to an imaginary type
         is always zero.  Use a comma operator to preserve side-effects of the
         source expression. */
      a_constant        zero_constant;
      an_expr_node_ptr  new_expr;
      make_zero_of_proper_type(float_type(dst_type->variant.float_kind),
                               &zero_constant);
      new_expr = make_comma_node(src, alloc_node_for_constant(&zero_constant));
      overwrite_node(expr, new_expr);
    } else {
      /* Nothing to be done (imaginary->imaginary). */
      check_assertion(is_imaginary_type(src_type));
    }  /* if */
  } else {
    check_assertion(is_arithmetic_or_enum_type(dst_type));
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
    } else if (is_imaginary_type(src_type)) {
      /* An imaginary value converted to a real or integral type is always
         zero.  Use a comma operator to preserve side-effects of the source
         expression. */
      a_constant        zero_constant;
      an_expr_node_ptr  new_expr;
      make_zero_of_proper_type(dst_type, &zero_constant);
      new_expr = make_comma_node(src, alloc_node_for_constant(&zero_constant));
      overwrite_node(expr, new_expr);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
}  /* lower_c99_complex_cast */

#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT

/*
Fixed-point lowering:
--------------------

When LOWER_FIXED_POINT is TRUE, fixed-point constructs are lowered
to standard C.  Fixed-point types are lowered to appropriately-
sized integral types; fixed-point constants are lowered to
integral constants; and fixed-point operations and casts are
lowered to calls of runtime routines.  The runtime routines
are not supplied by EDG.  They can be obtained from Dinkumkware, Ltd.
(www.dinkumware.com).

Here's an overview of the runtime interface.

First, let's define a generic container called an "fxvalue"
that can hold the bits of any fixed-point value or any integer value:

  typedef unsigned long long fxvalue;

The underlying type is configurable, but usually it's the largest
integer type, e.g., unsigned long long if that is supported.
It must be big enough to fit all target integers and also all
fixed-point types represented in integral form.  For values taking
less than the full set of bits, the bits of the value are placed
at the least-significant end of the fxvalue (this is done simply
by casting; having the fxvalue type be unsigned prevents sign
extension).

Next, let's define a 5-bit field called an "fxtype" that describes
the type contained in an fxvalue.  Starting from the least-significant
bit:

(2 bits) Precision: 00 short
                    01 default
                    10 long
                    11 integral operand
(1 bit)  Kind:       0 _Fract
                     1 _Accum
(1 bit)  Sign:       0 signed     ]
                     1 unsigned   ]--- also used for integral operands
(1 bit)  Saturation: 0 not _Sat
                     1 _Sat

Next, let's define a 3-bit field called an "fxcontrol" that
describes the pragma state at the point of an operation.
Starting from the least-significant bit:

(1 bit)  FX_FRACT_OVERFLOW   0 DEFAULT
                             1 SAT
(1 bit)  FX_ACCUM_OVERFLOW   0 DEFAULT
                             1 SAT
(1 bit)  FX_FULL_PRECISION   0 OFF
                             1 ON

This may not be used by the runtime (and in fact at the moment the
EDG front end always passes zeroes for those bits).

These fields are combined into several sets of bits that 
describe operand and result types and pragma state.  In each case,
the fields are listed starting from the least-significant bit:

fxmask1: fxcontrol, fxtype for operand (also gives result type)
fxmask2: fxcontrol, fxtype for operand 1, fxtype for operand 2,
         fxtype for result
fxmaskr: fxcontrol, fxtype for operand 1, fxtype for operand 2
fxmaskc: fxcontrol, fxtype for operand, fxtype for result
fxmaskf: fxcontrol, fxtype for operand
fxmaskg: fxcontrol, fxtype for result

The integral types in which these are passed are configurable,
but the default (to match the Dinkumware runtime) is unsigned short
for all fxmasks except fxmask2, which is passed as unsigned long.

The runtime routines are as follows:

// Unary operators:
fxvalue _Fixed_negate(fxmask1, fxvalue);
fxvalue _Fixed_incr  (fxmask1, fxvalue);  // Used for ++
fxvalue _Fixed_decr  (fxmask1, fxvalue);  // Used for --

// Binary operators:
fxvalue _Fixed_add     (fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_subtract(fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_multiply(fxmask2, fxvalue, fxvalue);
fxvalue _Fixed_fivide  (fxmask2, fxvalue, fxvalue);

// Relational operators:
int     _Fixed_eq      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_ne      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_gt      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_lt      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_ge      (fxmaskr, fxvalue, fxvalue);
int     _Fixed_le      (fxmaskr, fxvalue, fxvalue);

// Shift
fxvalue _Fixed_shiftl  (fxmask1, fxvalue, int shift_count);
fxvalue _Fixed_shiftr  (fxmask1, fxvalue, int shift_count);

// Conversion
//   Fixed to (other) fixed, fixed-point to integer, and
//   integer to fixed
fxvalue     _Fixed_conv        (fxmaskc, fxvalue);
//   Fixed to floating-point
float       _Fixed_to_float    (fxmaskf, fxvalue);
double      _Fixed_to_double   (fxmaskf, fxvalue);
long double _Fixed_to_ldouble  (fxmaskf, fxvalue);
//   Floating-point to fixed
fxvalue     _Fixed_from_float  (fxmaskg, float);
fxvalue     _Fixed_from_double (fxmaskg, double);
fxvalue     _Fixed_from_ldouble(fxmaskg, long double);

Conversions to/from complex and imaginary are handled by using
the floating-point conversions on the real part and/or an
appropriate zero.

Compound assignments are handled by rewriting, e.g.,
x += 1 is turned into x = x + 1 (but x is evaluated only
once) and that is lowered.  Likewise increments and decrements
are rewritten as adds or subtracts of 1, e.g., ++x becomes
x = x + 1 and that is lowered.
*/

/*
Integer kind for the fxmask parameter to fixed-point runtime routines.
Must match the size chosen in the runtime provided by Dinkumware.
The fxmask2 case is used for two-operand routines like _Fixed_add,
which require a bigger fxmask because it includes two operand types
and a result type.
*/
#define FXMASK_INT_KIND ((an_integer_kind)ik_unsigned_short)
#define FXMASK2_INT_KIND ((an_integer_kind)ik_unsigned_long)


static int fxtype_value(a_fixed_point_type_descr descr)
/*
Return the "fxtype" value that describes the indicated fixed-point
type description.  See the documentation above for the bit values.
*/
{
#define FXTYPE_SIZE 5
  int fxtype;

  if (descr.precision == (a_fixed_point_precision)fpp_short) {
    fxtype = 0;
  } else if (descr.precision == (a_fixed_point_precision)fpp_default) {
    fxtype = 1;
  } else if (descr.precision == (a_fixed_point_precision)fpp_long) {
    fxtype = 2;
  } else {
    unexpected_condition();
  }  /* if */
  fxtype |= descr.is_fract_type ? 0 : 0x4;
  fxtype |= descr.is_unsigned ? 0x8 : 0;
  fxtype |= descr.saturating ? 0x10 : 0;
  return fxtype;
}  /* fxtype_value */


static int integral_fxtype_value(a_boolean is_signed)
/*
Return the fxtype value used to represent an integral operand,
signed if is_signed is TRUE.    See the documentation above for the
bit values.
*/
{
  int fxtype = 3;  /* 11 in bottom 2 bits means integral value. */

  if (!is_signed) fxtype |= 0x8;
  return fxtype;
}  /* integral_fxtype_value */


static int fxtype_value_for_type(a_type_ptr type)
/*
Return the fxtype value for the given (fixed-point or integral)
type.  See the documentation above for the bit values.
*/
{
  int fxtype;

  if (is_integral_or_enum_type(type)) {
    fxtype = integral_fxtype_value(is_signed_integral_type(type));
  } else if (is_fixed_point_type(type)) {
    fxtype = fxtype_value(skip_typerefs(type)->variant.fixed_point);
  } else {
    unexpected_condition();
  }  /* if */
  return fxtype;
}  /* fxtype_value_for_type */


static a_type_ptr fxvalue_type(void)
/*
Return the "fxvalue" type, which is the type of the container used to
pass fixed-point and integral values to and from the fixed-point runtime.
*/
{
  /* This will usually be a 64-bit data type if long long is supported,
     and a 32-bit data type otherwise.  One should ensure that the
     runtime is configured to use the same type. */
  a_type_ptr type = integer_type(targ_uintmax_kind);
  return type;
}  /* fxvalue_type */


static int fxcontrol_value(void)
/*
Return the "fxcontrol" value for the current location in the
program.  It describes the current fixed-point pragma state.
See the documentation above for the bit values.
*/
{
#define FXCONTROL_SIZE 3
  /* Currently always returns zero, because the Dinkumware runtime
     does not use the bits. */
  return 0;
}  /* fxcontrol_value */


static an_expr_node_ptr add_cast_to_fxvalue_type(an_expr_node_ptr expr)
/*
Cast the indicated expression to the fxvalue type used to interface
to the fixed-point runtime routines.
*/
{
  a_type_ptr type = expr->type;

  if (is_fixed_point_type(type)) {
    type = lowered_integer_type_for_fixed_point_type(type);
    if (is_signed_integral_type(type)) {
      /* For fixed-point types represented as signed integral types,
         cast to the same-sized unsigned type first before widening to
         avoid sign extension. */
      a_type_ptr unsigned_type =
                   other_signedness_integer_type(f_skip_typerefs(type)->
                                                     variant.integer.int_kind);
      expr = add_cast_if_necessary(expr, unsigned_type);
    }  /* if */
  }  /* if */
  expr = add_cast_if_necessary(expr, fxvalue_type());
  return expr;
}  /* add_cast_to_fxvalue_type */


/*
Runtime routine for fixed-point conversions (including integral
source or destination).
*/
static a_routine_ptr fixed_conv_routine;
/*
Runtime routines for conversions between floating point and
fixed point.
*/
static char *float_fixed_conv_routine_name[3] = {"_Fixed_from_float",
                                                 "_Fixed_from_double",
                                                 "_Fixed_from_ldouble"};
static a_routine_ptr float_fixed_conv_routine[(int)fk_last];
static char *fixed_float_conv_routine_name[3] = {"_Fixed_to_float",
                                                 "_Fixed_to_double",
                                                 "_Fixed_to_ldouble"};
static a_routine_ptr fixed_float_conv_routine[(int)fk_last];


static void lower_c99_fixed_point_cast(an_expr_node_ptr expr)
/*
Lower the indicated cast (which has a fixed-point source and/or
destination) to a runtime call).
*/
{
  an_expr_node_ptr  src = expr->variant.operation.operands;
  an_expr_node_ptr  fxmask_expr, new_expr;
  a_type_ptr        src_type = src->type;
  a_type_ptr        base_src_type = skip_typerefs(src_type);
  a_type_ptr        dst_type = expr->type;
  a_type_ptr        base_dst_type = skip_typerefs(dst_type);
  a_type_ptr        return_type;
  a_type_ptr        param2_type;
  a_routine_ptr     *routine;
  char              *routine_name;
  a_float_kind      fkind;
  unsigned long     fxmask;
  int               shift_amount = 0;
  a_constant        zero_constant;

  if (is_void_type(dst_type)) {
    /* A cast to void.  Nothing needs to be done. */
  } else if (il_identical_types(base_src_type, base_dst_type)) {
    /* A do-nothing cast.  Leave it as it is (it will be a cast between
       integral types). */
  } else if (is_imaginary_type(dst_type)) {
    /* A fixed-point value converted to an imaginary type is always zero.
       Use a comma operator to preserve side-effects of the source
       expression. */
    check_assertion(is_fixed_point_type(src_type));
#if LOWER_COMPLEX
    dst_type = float_type(base_dst_type->variant.float_kind);
#endif /* LOWER_COMPLEX */
    make_zero_of_proper_type(dst_type, &zero_constant);
    new_expr = make_comma_node(src, alloc_node_for_constant(&zero_constant));
    overwrite_node(expr, new_expr);
  } else if (is_imaginary_type(src_type)) {
    /* An imaginary value converted to a fixed-point type is always zero.
       Use a comma operator to preserve side-effects of the source
       expression. */
    check_assertion(is_fixed_point_type(dst_type));
    make_zero_of_proper_type(dst_type, &zero_constant);
    lower_c99_fixed_point_constant(&zero_constant);
    new_expr = make_comma_node(src, alloc_node_for_constant(&zero_constant));
    overwrite_node(expr, new_expr);
  } else {
    /* Generate a call to the runtime cast routine.  There's a primary
       routine _Fixed_conv that handles all the fixed-point/fixed-point
       and fixed-point/integer cases, and 3 routines each (for the different
       precisions) that handle the fixed-point/floating-point cases. */
    routine_name = "_Fixed_conv";
    routine = &fixed_conv_routine;
    return_type = param2_type = fxvalue_type();
    /* Build up the fxmask argument describing the operand and result
       types. */
    fxmask = fxcontrol_value();
    shift_amount = FXCONTROL_SIZE;
    if (is_fixed_point_type(src_type) || is_integral_or_enum_type(src_type)) {
      /* Add the mask for the source type. */
      fxmask |= (fxtype_value_for_type(src_type) << shift_amount);
      shift_amount += FXTYPE_SIZE;
      /* Convert the operand to the fxvalue type used to interface to the
         runtime. */
      src = add_cast_to_fxvalue_type(src);
    } else {
      /* Conversion from floating or complex to fixed point. */
      check_assertion(is_floating_type(src_type) ||
                      is_complex_type(src_type));
      check_assertion(is_fixed_point_type(dst_type));
      fkind = base_src_type->variant.float_kind;
      if (is_complex_type(src_type)) {
        /* Convert the operand from complex to the same-precision floating
           type. */
        src = add_cast_if_necessary(src, float_type(fkind));
#if LOWER_COMPLEX
        lower_c99_complex_cast(src);
#endif /* LOWER_COMPLEX */
      }  /* if */
      routine_name = select_name_from_float_kind(fkind,
                                                float_fixed_conv_routine_name);
      routine = &float_fixed_conv_routine[(int)fkind];
      param2_type = float_type(fkind);
    }  /* if */
    if (is_fixed_point_type(dst_type) || is_integral_or_enum_type(dst_type)) {
      /* Add the mask for the destination type. */
      fxmask |= (fxtype_value_for_type(dst_type) << shift_amount);
      shift_amount += FXTYPE_SIZE;
    } else {
      /* Conversion from fixed point to floating or complex. */
      check_assertion(is_floating_type(dst_type) ||
                      is_complex_type(dst_type));
      check_assertion(is_fixed_point_type(src_type));
      fkind = base_dst_type->variant.float_kind;
      routine_name = select_name_from_float_kind(fkind,
                                                fixed_float_conv_routine_name);
      routine = &fixed_float_conv_routine[(int)fkind];
      return_type = float_type(fkind);
    }  /* if */
    fxmask_expr = node_for_integer_constant((long)fxmask, FXMASK_INT_KIND);
    /* Make the call of the runtime cast routine. */
    fxmask_expr->next = src;
    new_expr = make_prototyped_runtime_call(routine_name, routine,
                                            return_type,
                                            integer_type(FXMASK_INT_KIND),
                                            param2_type,
                                            fxmask_expr);
    /* Cast the value returned by the runtime routine to the final
       desired type. */
    new_expr = add_cast_if_necessary(new_expr, expr->type);
#if LOWER_COMPLEX
    if (is_complex_type(dst_type)) {
      lower_c99_complex_cast(new_expr);
    }  /* if */
#endif /* LOWER_COMPLEX */
    /* Overwrite the original node with the lowered expression. */
    overwrite_node(expr, new_expr);
  }  /* if */
}  /* lower_c99_fixed_point_cast */

#endif /* LOWER_FIXED_POINT */

void post_lower_c99_bool_cast(an_expr_node_ptr expr)
/*
Called from lower_bool_cast to check for and do any additional
lowering on the "!= 0" comparison generated, e.g., for complex values.
*/
{
  check_assertion(is_operation_node(expr));
#if LOWER_COMPLEX
  if (expr->variant.operation.kind == (an_expr_operator_kind)eok_xne) {
    /* Do further lowering for complex != 0. */
    /* Lower the complex zero constant. */
    lower_c99_expr(expr->variant.operation.operands->next,
                   /*used_as_lvalue=*/FALSE);
    lower_c99_xne(expr);
  } else if (expr->variant.operation.kind == (an_expr_operator_kind)eok_fne&&
             is_imaginary_type(expr->variant.operation.operands->next->type)) {
    /* Do further lowering for imaginary != 0. */
    /* Lower the imaginary zero constant. */
    lower_c99_expr(expr->variant.operation.operands->next,
                   /*used_as_lvalue=*/FALSE);
  } else
#endif /* LOWER_COMPLEX */
  {
#if LOWER_FIXED_POINT
    if (expr->variant.operation.kind == (an_expr_operator_kind)eok_fxne) {
      /* Do further lowering for fixed-point != 0. */
      lower_c99_expr(expr->variant.operation.operands->next,
                     /*used_as_lvalue=*/FALSE);
      lower_c99_fixed_point_operation(expr);
    }  /* if */
#endif /* LOWER_FIXED_POINT */
  }
}  /* post_lower_c99_bool_cast */


void lower_c99_cast(an_expr_node_ptr  expr)
/*
Transform the given cast expression into a function call (compatible with C89).
*/
{
  if (expr->variant.operation.kind == (an_expr_operator_kind)eok_bool_cast) {
    /* Change a cast to bool to a "!= 0" test. */
    lower_bool_cast(expr);
  } else {
    a_type_ptr  tp = expr->type;
    a_type_ptr  src_tp = expr->variant.operation.operands->type;
    check_assertion(expr->variant.operation.kind ==
                                             (an_expr_operator_kind)eok_cast);
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_enabled && !tp->visited_for_vla_lowering &&
        !(tp->kind == (a_type_kind)tk_typeref && typeref_is_typedef(tp)) &&
        is_variably_modified_type(tp)) {
      /* If the cast introduces a VLA type, we need to compute its dimension
         variables. Note that compiler-generated casts may cast to variably
         modified types that have already been visited. */
      an_expr_node_ptr  vla_inits = lower_vla_dimensions(tp);
      if (vla_inits != NULL) {
        expr->variant.operation.operands =
                 make_comma_node(vla_inits, expr->variant.operation.operands);
      }  /* if */
      record_vla_component_types_for_lowering(expr->type);
    }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
#if LOWER_FIXED_POINT
    if (fixed_point_enabled &&
        (is_fixed_point_type(tp) ||
         is_fixed_point_type(src_tp))) {
      lower_c99_fixed_point_cast(expr);
    } else
#endif /* LOWER_FIXED_POINT */
    /* Do not add code here. */
    {
#if LOWER_COMPLEX
      if (is_nonreal_floating_type(tp) ||
          is_nonreal_floating_type(src_tp)) {
        lower_c99_complex_cast(expr);
      }  /* if */
#endif /* LOWER_COMPLEX */
    }  /* if */
  }  /* if */
}  /* lower_c99_cast */

#if LOWER_FIXED_POINT

/*
Runtime routines for fixed-point operations.
*/
static a_routine_ptr
		fixed_negate_routine,
		fixed_eq_routine,
		fixed_ne_routine,
		fixed_gt_routine,
		fixed_lt_routine,
		fixed_ge_routine,
		fixed_le_routine,
		fixed_add_routine,
		fixed_subtract_routine,
		fixed_multiply_routine,
		fixed_divide_routine,
		fixed_shiftl_routine,
		fixed_shiftr_routine,
		fixed_incr_routine,
		fixed_decr_routine;


static void lower_c99_fixed_point_operation(an_expr_node_ptr expr)
/*
Lower a fixed-point operation expression.
*/
{
  an_expr_operator_kind op = expr->variant.operation.kind;
  an_expr_node_ptr      op1 = expr->variant.operation.operands;
  an_expr_node_ptr      op2 = op1->next;
  an_expr_node_ptr      fxmask_expr, new_expr;
  char                  *routine_name;
  a_routine_ptr         *routine;
  unsigned long         fxmask;
  int                   shift_amount = 0;
  a_boolean             need_result_fxtype = FALSE;
  a_boolean             is_comparison = FALSE;
  a_boolean             is_shift = FALSE;
  a_boolean             is_unary = FALSE;
  an_integer_kind       fxmask_int_kind = FXMASK_INT_KIND;
  a_type_ptr            return_type;
  a_type_ptr            op2_arg_type = fxvalue_type();

  /* Select the proper runtime routine for the operation. */
  switch (op) {
    case eok_fxnegate:
      routine_name = "_Fixed_negate";
      routine = &fixed_negate_routine;
      is_unary = TRUE;
      break;
    case eok_fxeq:
      routine_name = "_Fixed_eq";
      routine = &fixed_eq_routine;
      is_comparison = TRUE;
      break;
    case eok_fxne:
      routine_name = "_Fixed_ne";
      routine = &fixed_ne_routine;
      is_comparison = TRUE;
      break;
    case eok_fxgt:
      routine_name = "_Fixed_gt";
      routine = &fixed_gt_routine;
      is_comparison = TRUE;
      break;
    case eok_fxlt:
      routine_name = "_Fixed_lt";
      routine = &fixed_lt_routine;
      is_comparison = TRUE;
      break;
    case eok_fxge:
      routine_name = "_Fixed_ge";
      routine = &fixed_ge_routine;
      is_comparison = TRUE;
      break;
    case eok_fxle:
      routine_name = "_Fixed_le";
      routine = &fixed_le_routine;
      is_comparison = TRUE;
      break;
    case eok_fxadd:
      routine_name = "_Fixed_add";
      routine = &fixed_add_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_fxsubtract:
      routine_name = "_Fixed_subtract";
      routine = &fixed_subtract_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_fxmultiply:
      routine_name = "_Fixed_multiply";
      routine = &fixed_multiply_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_fxdivide:
      routine_name = "_Fixed_divide";
      routine = &fixed_divide_routine;
      need_result_fxtype = TRUE;
      break;
    case eok_fxshiftl:
      routine_name = "_Fixed_shiftl";
      routine = &fixed_shiftl_routine;
      is_shift = TRUE;
      break;
    case eok_fxshiftr:
      routine_name = "_Fixed_shiftr";
      routine = &fixed_shiftr_routine;
      is_shift = TRUE;
      break;
    default:
      unexpected_condition_str("bad fixed point operator");
  }  /* switch */
  if (is_comparison) {
    /* Result type for comparisons is int. */
    return_type = integer_type((an_integer_kind)ik_int);
  } else {
    /* For most operations, it's the fxvalue type. */
    return_type = fxvalue_type();
  }  /* if */
  /* Build up the fxmask argument describing the operand types. */
  fxmask = fxcontrol_value();
  shift_amount = FXCONTROL_SIZE;
  /* First operand fxtype. */
  fxmask |= (fxtype_value_for_type(op1->type) << shift_amount);
  shift_amount += FXTYPE_SIZE;
  if (!is_shift && !is_unary) {
    /* Second operand fxtype. */
    fxmask |= (fxtype_value_for_type(op2->type) << shift_amount);
    shift_amount += FXTYPE_SIZE;
  }  /* if */
  if (need_result_fxtype) {
    /* Result fxtype. */
    fxmask |= (fxtype_value_for_type(expr->type) << shift_amount);
    /* Operations like "add" need the fxmask2 variant, which includes
       two operand types and a result type and is therefore bigger. */
    fxmask_int_kind = FXMASK2_INT_KIND;
  }  /* if */
  fxmask_expr = node_for_integer_constant((long)fxmask, fxmask_int_kind);
  /* Convert the first operand to the fxvalue type used to interface to the
     runtime. */
  op1->next = NULL;
  op1 = add_cast_to_fxvalue_type(op1);
  if (is_unary) {
    /* A unary operation has no second operand. */
    op2_arg_type = NULL;
  } else {
    /* Convert the second operand to the type used to interface to the
       runtime. */
    if (is_shift) {
      /* For a shift, the second operand is the int shift count. */
      op2_arg_type = integer_type((an_integer_kind)ik_int);
      op2 = add_cast_if_necessary(op2, op2_arg_type);
    } else {
      op2 = add_cast_to_fxvalue_type(op2);
    }  /* if */
  }  /* if */
  /* Make the call of the runtime comparison routine. */
  fxmask_expr->next = op1;
  op1->next = op2;
  new_expr = make_prototyped_runtime_call_full(routine_name, routine,
                                               return_type,
                                               integer_type(fxmask_int_kind),
                                               fxvalue_type(),
                                               op2_arg_type,
                                               fxmask_expr);
  /* Cast the value returned by the runtime routine to the final
     desired type (probably does nothing except add a typedef if
     appropriate). */
  new_expr = add_cast_if_necessary(new_expr, expr->type);
  /* Overwrite the original node with the lowered expression. */
  overwrite_node(expr, new_expr);
}  /* lower_c99_fixed_point_operation */


static void lower_c99_fixed_point_incr_decr(an_expr_node_ptr expr)
/*
Lower the indicated fixed-point increment or decrement operation.
*/
{
  an_expr_operator_kind op = expr->variant.operation.kind, assign_op;
  an_expr_node_ptr      op1 = expr->variant.operation.operands;
  an_expr_node_ptr      op1_for_argument, op1_for_assign, op_node, op2_node;
  an_expr_node_ptr      fxmask_expr;
  a_variable_ptr        temp_var = NULL;
  a_boolean             is_post_op, temp_init_used;
  a_type_ptr            result_type = rvalue_type(type_pointed_to(op1->type));
  char                  *routine_name;
  a_routine_ptr         *routine;
  unsigned long         fxmask;
  int                   shift_amount = 0;

  switch (op) {
    case eok_fxpost_incr:
      is_post_op = TRUE;
      routine_name = "_Fixed_incr";
      routine = &fixed_incr_routine;
      break;
    case eok_fxpost_decr:
      is_post_op = TRUE;
      routine_name = "_Fixed_decr";
      routine = &fixed_decr_routine;
      break;
    case eok_fxpre_incr:
      is_post_op = FALSE;
      routine_name = "_Fixed_incr";
      routine = &fixed_incr_routine;
      break;
    case eok_fxpre_decr:
      is_post_op = FALSE;
      routine_name = "_Fixed_decr";
      routine = &fixed_decr_routine;
      break;
    default:
      unexpected_condition_str(
                              "lower_c99_fixed_point_incr_decr: bad operator");
  }  /* if */
  if (is_post_op && expr->result_is_not_used) {
    /* We don't need the more complicated post-incr/decr code if the
       result is not used. */
    is_post_op = FALSE;
  }  /* if */
  /* The normal rewrite of
       ++x
     is
       x = _Fixed_incr(x);
     Make a copy of op1 to be used as the argument of the call.
     op1 itself will be used as the left operand of the assignment. */ 
  op1_for_argument = make_lvalue_reusable_copy_full(op1,
                                                    /*vars_can_change=*/FALSE,
                                                    &temp_init_used);
  op1_for_assign = op1;
  assign_op = lowered_assignment_operator(result_type);
  if (temp_init_used || is_post_op) {
    /* op1 is complicated and was assigned to a temporary.  Make sure that
       the temporary is initialized before it is used by doing the
       overall rewrite of
         ++x;
       as
         ((temp = *(t = &x)), *t = _Fixed_incr(temp))
       We also use the temporary if the operation is a post-increment
       or -decrement, because we want to save and return the original value. */
    temp_var = make_local_temporary(result_type);
    op1_for_assign = op1_for_argument;
    op1_for_argument = var_lvalue_expr(temp_var);
    /* Make the (temp = *(t = &x)) assignment, to be inserted later. */
    op2_node = make_var_assignment_expr(temp_var,
                                        assign_op,
                                        add_indirection_to_node(op1));
  }  /* if */
  /* Make the argument for the call. */
  op1_for_argument = add_indirection_to_node(op1_for_argument);
  op1_for_argument = add_cast_to_fxvalue_type(op1_for_argument);
  /* Build up the fxmask argument describing the operand and result types. */
  fxmask = fxcontrol_value();
  shift_amount = FXCONTROL_SIZE;
  fxmask |= (fxtype_value_for_type(result_type) << shift_amount);
  fxmask_expr = node_for_integer_constant((long)fxmask, FXMASK_INT_KIND);
  /* Make the call of the runtime cast routine. */
  fxmask_expr->next = op1_for_argument;
  op_node = make_prototyped_runtime_call(routine_name, routine,
                                         fxvalue_type(),
                                         integer_type(FXMASK_INT_KIND),
                                         fxvalue_type(),
                                         fxmask_expr);
  /* Cast the value returned by the runtime routine to the final
     desired type. */
  op_node = add_cast_if_necessary(op_node, result_type);
  /* Assign the result to op1 (or the temporary). */
  op_node = make_assignment_expr(op1_for_assign, assign_op, op_node);
  if (temp_var != NULL) {
    /* Combine the assignment to the temporary and the assignment that
       does the incr/decr call and stores it back in the original
       operand. */
    op_node = make_comma_node(op2_node, op_node);
  }  /* if */
  /* Here, op_node is "x = _Fixed_incr(x)" or a fancier but equivalent
     expression if a temporary was used.  For a pre-operation, that's all
     we need. */
  if (is_post_op) {
    /* A post-increment or post-decrement.  Add a comma expression to
       return the value of the temporary, which is the original value
       of the operand. */
    op_node = make_comma_node(op_node, var_rvalue_expr(temp_var));
  }  /* if */
  overwrite_node(expr, op_node);
}  /* lower_c99_fixed_point_incr_decr */

#endif /* LOWER_FIXED_POINT */
#if GNU_EXTENSIONS_ALLOWED

static void lower_binary_conditional(an_expr_node_ptr  expr)
/*
The given expression is a GNU-style binary conditional expression of the form
"op1 ?: op2".  Turn it into "(tmp = op1, tmp) ? tmp : op2".
*/
{
  an_expr_node_ptr  op1 = expr->variant.operation.operands, op2 = op1->next;
  an_expr_node_ptr  tmp;

  tmp = make_reusable_copy(op1, /*vars_can_change=*/TRUE);
  /* The first operand of a binary question operator has not been converted
     to the result type nor has it been transformed into a boolean expression.
     Those operations must therefore be applied here. */
  tmp = add_cast_if_necessary(tmp, expr->type);
  op1 = normalize_boolean_controlling_expr(op1);
  op1->next = tmp;
  tmp->next = op2;
  expr->variant.operation.operands = op1;
  expr->variant.operation.kind = (an_expr_operator_kind)eok_question;
  if (expr->result_is_not_used) set_expr_result_not_used(expr);
}  /* lower_binary_conditional */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if !FIXED_POINT_ALLOWED
/*ARGSUSED*/  /* <-- expr is not used in that case. */
#endif /* !FIXED_POINT_ALLOWED */
static void lower_c99_call(an_expr_node_ptr expr)
/*
Do any required lowering on an eok_call expression node.  The operands
have been lowered already.
*/
{
#if FIXED_POINT_ALLOWED
  if (fixed_point_enabled) {
    /* May need to widen some fixed-point arguments passed to
       unprototyped parameters. */
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    a_type_ptr       rout_type = f_skip_typerefs(type_pointed_to(op1->type));
    a_routine_type_supplement_ptr
                     rtsp = rout_type->variant.routine.extra_info;
    a_param_type_ptr param = rtsp->param_type_list;
    an_expr_node_ptr arg;
    if (!rtsp->prototyped) param = NULL;
    for (arg = op1->next; arg != NULL; arg = arg->next) {
      if (param == NULL) {
        /* An unprototyped parameter. */
        if (is_fixed_point_type(arg->type)) {
          do_default_arg_promotions_on_node(arg);
        }  /* if */
      } else {
        /* A prototyped parameter. */
        param = param->next;
      }  /* if */
    }  /* for */
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
}  /* lower_c99_call */

#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS

static an_expr_node_ptr vla_size_expr(a_type_ptr  vla_type,
                                      a_boolean   byte_count)
/*
Return an expression describing the (nonconstant) size of the given VLA type.
If byte_count is TRUE, the expression should reflect the size as a number of
bytes; otherwise, the size should be the number of elements.
*/
{
  an_expr_node_ptr     result;
  a_type_ptr           array_type = skip_typerefs(vla_type);
  a_type_ptr           ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  a_targ_size_t        constant_factor = 1;
  a_vla_dimension_ptr  vla_dim;

  /* Accumulate any constant dimensions first. */
  while (!array_type->variant.array.is_vla) {
    a_targ_size_t  length =
                         array_type->variant.array.variant.number_of_elements;
    constant_factor *= length;
    array_type = skip_typerefs(array_type->variant.array.element_type);
  }  /* while */
  if (byte_count) {
    /* Multiply the constant factor by the size of the underlying element
       type. */
    a_type_ptr  element_type = underlying_array_element_type(array_type);
    constant_factor *= skip_typerefs(element_type)->size;
  }  /* if */
  /* Retrieve the previously computed number of elements in the VLA. */
  vla_dim = find_vla_dimension(array_type);
  check_assertion(vla_dim->total_number_of_elements != NULL);
  result = var_rvalue_expr(vla_dim->total_number_of_elements);
  if (constant_factor != 1) {
    /* For an array_type like T[4][5][expr][7] constant_factor is 20 and
       result is an expression representing expr*7.  Multiply the
       two factors to obtain the total scaling. */
    result->next =
            node_for_host_large_integer((a_host_large_integer)constant_factor,
                                        targ_ptrdiff_t_int_kind);
    result = make_operator_node((an_expr_operator_kind)eok_imultiply,
                                ptrdiff_type, result);
  }  /* if */
  return result;
}  /* vla_size_expr */


static void lower_vla_pointer_integer_arithmetic(an_expr_node_ptr  expr)
/*
The given expression node adds or subtracts an integer to or from a pointer
to a VLA.  Scale up the integer to compensate for the fact that the pointer
will be lowered to a pointer to the underlying element type.  For pre- and
post-increment operators, the operator needs to be changed since the amount
incremented or decremented will no longer be one.
*/
{
  a_type_ptr             array_type = type_pointed_to(expr->type), new_type;
  a_type_ptr             ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  an_expr_node_ptr       offset, scale_factor, lval, lval_copy, result;
  an_expr_operator_kind  op = expr->variant.operation.kind;

  /* Scale the offset according to the (nonconstant) total number of elements
     in the underlying array type. */
  array_type = skip_typerefs(array_type);
  scale_factor = vla_size_expr(array_type, /*byte_count=*/FALSE);
  switch (op) {
    case eok_padd:
    case eok_psubtract:
    case eok_padd_subsc:
    case eok_padd_assign:
    case eok_psubtract_assign:
      /* Binary operators: Simply scale up the integer operand. */
      offset = expr->variant.operation.operands->next;
      offset = add_lowered_cast_if_necessary(offset, ptrdiff_type);
      scale_factor->next = offset;
      expr->variant.operation.operands->next =
                      make_operator_node((an_expr_operator_kind)eok_imultiply,
                                         ptrdiff_type, scale_factor);
      break;
    case eok_ppre_incr:
      /* Turn pre-increment into a += operator. */
      expr->variant.operation.kind = (an_expr_operator_kind)eok_padd_assign;
      expr->variant.operation.operands->next = scale_factor;
      break;
    case eok_ppre_decr:
      /* Turn pre-decrement into a -= operator. */
      expr->variant.operation.kind =
                                  (an_expr_operator_kind)eok_psubtract_assign;
      expr->variant.operation.operands->next = scale_factor;
      break;
    case eok_ppost_incr:
    case eok_ppost_decr:
      /* expr++ is transformed also transformed into a += operator, but we
         need to save the original value to produce the result of the
         expression.  expr-- is entirely similar. */
      op = (op == (an_expr_operator_kind)eok_ppost_incr) ?
                                   (an_expr_operator_kind)eok_padd_assign :
                                   (an_expr_operator_kind)eok_psubtract_assign;
      /* The original lvalue: */
      lval = expr->variant.operation.operands;
      /* Make a copy that we are going to use to increment/decrement the
         value pointed to: */
      lval_copy = make_lvalue_reusable_copy(lval, /*vars_can_change=*/TRUE);
      lval_copy->next = scale_factor;
      /* Save the original rvalue in a temporary that will be used to produce
         the result: */
      lval = add_indirection_to_node(lval);
      result = assign_expr_to_temp_and_make_expr_for_reuse(lval);
      /* Adjust the type of the pointer operand to point to the underlying
         element type. */
      new_type = make_pointer_type(underlying_array_element_type(array_type));
      /* Assemble the three expressions as a replacement for the given node. */
      overwrite_node(
        expr,
        make_comma_node(
          make_comma_node(lval, make_operator_node(op, new_type, lval_copy)),
          result));
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* lower_vla_pointer_integer_arithmetic */


static void lower_vla_pointer_difference(an_expr_node_ptr  expr)
/*
The given expression is a difference of pointers to VLAs.  The result must
be scaled down by the number of elements in the VLAs pointed to.
*/
{
  a_type_ptr        array_type =
                      type_pointed_to(expr->variant.operation.operands->type);
  a_type_ptr        ptrdiff_type = integer_type(targ_ptrdiff_t_int_kind);
  an_expr_node_ptr  scale_factor, copy;

  /* Scale the result according to the (nonconstant) total number of elements
     in the underlying array type. */
  array_type = skip_typerefs(array_type);
  scale_factor = vla_size_expr(array_type, /*byte_count=*/FALSE);
  copy = copy_node(expr);
  copy->next = scale_factor;
  overwrite_node(expr, make_operator_node((an_expr_operator_kind)eok_idivide,
                                          ptrdiff_type, copy));
}  /* lower_vla_pointer_difference */ 

#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */

static void lower_runtime_sizeof(an_expr_node_ptr  expr)
/*
Lower the given given enk_runtime_sizeof node.  If VLAs are lowered, the node
is replaced by an expression representing the number of bytes of the VLA type
underlying the sizeof expression.  Otherwise, if the eok_runtime_sizeof node
applies to an expression, the underlying expression is lowered.
*/
{
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  an_expr_node_ptr  byte_count, precomputation = NULL;
  a_type_ptr        vla_type;
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */

#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (expr->variant.runtime_sizeof.is_type) {
    /* Something like "sizeof(X[2][n][m/2])".  Unlike uses of VLAs in
       declarations there is no stmk_set_vla_size for VLA types named in
       expressions.  So we may have to perform computations on the fly. */
    vla_type = expr->variant.runtime_sizeof.variant.type;
    if (!(vla_enabled && is_vla_type(vla_type))) {
      goto done;
    }  /* if */
    precomputation = lower_vla_dimensions(vla_type);
  } else {
    /* sizeof was applied to a VLA expression. */
    precomputation = expr->variant.runtime_sizeof.variant.expr;
    vla_type = precomputation->type;
    if (expr->variant.runtime_sizeof.is_lvalue) {
      vla_type = type_pointed_to(vla_type);
    }  /* if */
    if (!(vla_enabled && is_vla_type(vla_type))) {
      goto done;
    }  /* if */
    /* Lower the argument expression, but be sure to have extracted the
       type first.  (The lowered type is no longer a VLA.) */
    lower_c99_expr(precomputation, /*used_as_lvalue=*/FALSE);
  }  /* if */
  byte_count = vla_size_expr(vla_type, /*byte_count=*/TRUE);
  byte_count = add_cast_if_necessary(byte_count,
                                     integer_type(targ_size_t_int_kind));
  if (precomputation != NULL) {
    byte_count = make_comma_node(precomputation, byte_count);
  }  /* if */
  overwrite_node(expr, byte_count);
done:;
#else /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
  /* We're not lowering the run-time sizeof operator, but we may have to
     lower the argument if that argument is an expression. */
  if (!expr->variant.runtime_sizeof.is_type) {
    lower_c99_expr(expr->variant.runtime_sizeof.variant.expr,
                   /*used_as_lvalue=*/FALSE);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_runtime_sizeof */


void lower_c99_operator(an_expr_node_ptr  expr)
/*
The given expression should be an operation: Replace it by IL that is
compatible with C89 IL.  Nontrivial transformations are needed (among
others) for operations involving complex types, fixed-point types, the
_Bool type, and VLA types.
*/
{
  check_assertion(expr->kind == (an_expr_node_kind)enk_operation);
  switch (expr->variant.operation.kind) {
#if LOWER_COMPLEX
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
      /* Complex assignment becomes structure assignment. */
      expr->variant.operation.kind = (an_expr_operator_kind)eok_sassign;
      break;
    case eok_xadd_assign:
    case eok_xsubtract_assign:
    case eok_xmultiply_assign:
    case eok_xdivide_assign:
      rewrite_compound_assignment(expr, /*is_lvalue=*/FALSE);
      break;
    case eok_jmultiply:
      lower_c99_jmultiply(expr);
      break;
    case eok_jdivide:
      lower_c99_jdivide(expr);
      break;
    case eok_fjadd:
    case eok_jfadd:
    case eok_fjsubtract:
    case eok_jfsubtract:
      /* Mixed real/imaginary add/subtract. */
      lower_real_imag_add_subtract(expr);
      break;
#endif /* LOWER_COMPLEX */
    case eok_cast:
    case eok_bool_cast:
      lower_c99_cast(expr);
      break;
#if FIXED_POINT_ALLOWED
    case eok_fxassign:
#if LOWER_FIXED_POINT
      /* Fixed-point assignment becomes integer assignment. */
      expr->variant.operation.kind = (an_expr_operator_kind)eok_iassign;
#endif /* LOWER_FIXED_POINT */
      break;
    case eok_fxnegate:
    case eok_fxadd:
    case eok_fxsubtract:
    case eok_fxmultiply:
    case eok_fxdivide:
    case eok_fxshiftl:
    case eok_fxshiftr:
    case eok_fxeq:
    case eok_fxne:
    case eok_fxgt:
    case eok_fxlt:
    case eok_fxge:
    case eok_fxle:
      /* Fixed-point operations. */
#if LOWER_FIXED_POINT
      lower_c99_fixed_point_operation(expr);
#endif /* LOWER_FIXED_POINT */
      break;
    case eok_fxadd_assign:
    case eok_fxsubtract_assign:
    case eok_fxmultiply_assign:
    case eok_fxdivide_assign:
    case eok_fxshiftl_assign:
    case eok_fxshiftr_assign:
#if LOWER_FIXED_POINT
      rewrite_compound_assignment(expr, /*is_lvalue=*/FALSE);
#endif /* LOWER_FIXED_POINT */
      break;
    case eok_fxpost_incr:
    case eok_fxpost_decr:
    case eok_fxpre_incr:
    case eok_fxpre_decr:
#if LOWER_FIXED_POINT
      lower_c99_fixed_point_incr_decr(expr);
#endif /* LOWER_FIXED_POINT */
      break;
#endif /* FIXED_POINT_ALLOWED */
    case eok_iadd_assign:
    case eok_isubtract_assign:
    case eok_imultiply_assign:
    case eok_idivide_assign:
    case eok_remainder_assign:
    case eok_shiftl_assign:
    case eok_shiftr_assign:
    case eok_and_assign:
    case eok_or_assign:
    case eok_xor_assign:
    case eok_fadd_assign:
    case eok_fsubtract_assign:
    case eok_fmultiply_assign:
    case eok_fdivide_assign:
      /* Compound assignments to bool don't exist in C89, and
         must be lowered to get the value reduced to 0/1.
         Also operations that involve a fixed-point operand but aren't
         a fixed-point operation (e.g., fixed_point += double). */
      if (is_bool_type(expr->type) ||
          (fixed_point_enabled &&
           is_fixed_point_type(type_pointed_to(
                                    expr->variant.operation.operands->type)))){
        rewrite_compound_assignment(expr, /*is_lvalue=*/FALSE);
      }  /* if */
      break;
    case eok_ipost_incr:
    case eok_ipre_incr:
    case eok_ipost_decr:
    case eok_ipre_decr:
      /* Increments/decrements of bool. */
      if (is_bool_type(expr->type)) {
        lower_bool_incr_decr(expr);
      }  /* if */
      break;
#if GNU_EXTENSIONS_ALLOWED
    case eok_binary_question:
      lower_binary_conditional(expr);
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    case eok_call:
      lower_c99_call(expr);
      break;
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
    case eok_padd:
    case eok_psubtract:
    case eok_padd_subsc:
    case eok_padd_assign:
    case eok_psubtract_assign:
    case eok_ppre_incr:
    case eok_ppre_decr:
    case eok_ppost_incr:
    case eok_ppost_decr:
      if (vla_enabled && is_vla_type(type_pointed_to(expr->type))) {
        lower_vla_pointer_integer_arithmetic(expr);
      }  /* if */
      break;
    case eok_pdiff:
      if (vla_enabled && is_vla_type(type_pointed_to(
                                   expr->variant.operation.operands->type))) {
        lower_vla_pointer_difference(expr);
      }  /* if */
      break;
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
    default:
      /* Nothing needs to be done. */
      break;
  }  /* switch */
}  /* lower_c99_operator */

#if LOWER_FIXED_POINT

static void lower_c99_fixed_point_constant(a_constant_ptr constant)
/*
Lower the indicated fixed-point constant.  The lowered form is an
integral constant.
*/
{
  an_integer_value int_value;

  int_value = constant->variant.fixed_point_value;
  set_constant_kind(constant, (a_constant_repr_kind)ck_integer);
  constant->variant.integer_value = int_value;
  constant->type = lowered_integer_type_for_fixed_point_type(constant->type);
}  /* lower_c99_fixed_point_constant */

#endif /* LOWER_FIXED_POINT */
#if LOWER_COMPLEX

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
  memcpy((char *)&real_part->variant.float_value,
         (char *)&constant->variant.complex_value->real,
         sizeof(an_internal_float_value));
  imag_part = fs_constant((a_constant_repr_kind)ck_float);
  imag_part->type = float_type(fkind);
  memcpy((char *)&imag_part->variant.float_value,
         (char *)&constant->variant.complex_value->imag,
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

#endif /* LOWER_COMPLEX */

void lower_c99_constant(a_constant_ptr  constant)
/*
If the given constant contains C99-specific constructs (like _Complex values),
replace them by a representation compatible with C89.
*/
{
  switch (constant->kind) {
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
#if LOWER_FIXED_POINT
      lower_c99_fixed_point_constant(constant);
#endif /* LOWER_FIXED_POINT */
      break;
#endif /* FIXED_POINT_ALLOWED */
    case ck_complex:
#if LOWER_COMPLEX
      lower_c99_complex_constant(constant);
#endif /* LOWER_COMPLEX */
      break;
    case ck_aggregate:
      lower_c99_constant_list(constant->variant.aggregate.first_constant);
      break;
    case ck_imaginary:
#if LOWER_COMPLEX
      /* Represent the constant as a regular floating-point constant.
         Its type will similarly be adjusted. */
      constant->kind = (a_constant_repr_kind)ck_float;
#endif /* LOWER_COMPLEX */
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
        case abk_label:
          /* Nothing to be done. */
          break;
        default:
          unexpected_condition_str("Bad c99 address const kind");
      }  /* switch */
      break;
    case ck_init_repeat:
      lower_c99_constant(constant->variant.init_repeat.constant);
      break;
    case ck_designator:
      /* Note that designated initializers for unions remain even when
         LOWER_DESIGNATED_INITIALIZERS is TRUE. */
      break;
    case ck_error:
    case ck_integer:
    case ck_float:
    case ck_string:
      /* Nothing to be done. */
      break;
    case ck_dynamic_init:  /* Not expected here. */
    default:
      unexpected_condition_str("Invalid C99 constant");
      break;
  }  /* switch */
}  /* lower_c99_constant */


#if !LOWER_COMPLEX
/*ARGSUSED*/  /* <-- expr is not used in that case. */
#endif /* !LOWER_COMPLEX */
static void lower_c99_constant_expr(an_expr_node_ptr  expr)
/*
Transform the given enk_constant expression to remove certain C99-specific
constructs.
*/
{
#if LOWER_COMPLEX
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
      if (!in_file_scope(constant)) {
        /* The constant is local to a function (this happens, for example,
           when RECORD_CONSTANT_EXPRESSIONS_IN_IL is TRUE).  Make a copy
           in the file scope so it can be pointed to from the file-scope
           variable. */
        a_memory_region_number region_to_switch_back_to;
        switch_to_file_scope_region(&region_to_switch_back_to);
        constant = alloc_unshared_constant(constant);
        switch_back_to_original_region(region_to_switch_back_to);
      }  /* if */
      tmp->initializer.constant = constant;
      lower_c99_constant(tmp->initializer.constant);
      constant->source_corresp.assoc_info = (char*)tmp;
    } else {
      /* Reuse the previously created temporary. */
      tmp = (a_variable_ptr)constant->source_corresp.assoc_info;
    }  /* if */
    overwrite_node(expr, var_rvalue_expr(tmp));
  } else
#endif /* LOWER_COMPLEX */
  {
#if LOWER_FIXED_POINT
    if (fixed_point_enabled && is_fixed_point_type(expr->type)) {
      lower_c99_constant(expr->variant.constant);
    }  /* if */
#endif /* LOWER_FIXED_POINT */
  }  /* if */
}  /* lower_c99_constant_expr */


static void lower_c99_temp_init(an_expr_node_ptr expr)
/*
Lower the given enk_temp_init expression.  An enk_temp_init is used
in C99 mode to represent a compound literal.
*/
{
  a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
  a_variable_ptr     var;
  a_type_ptr         temp_type;
  a_boolean          result_is_addr;
  an_insert_location insert_location;
  an_init_pos_descr  ipd;
  a_boolean          keep_dynamic_init;
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  an_expr_node_ptr   vla_inits = NULL;
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */

  /* This routine is similar to lower_temp_init. */
  /* The compound literal is rewritten to use a temporary.  The temporary
     is initialized to the constant part of the aggregate, and code is
     generated for any non-constant parts. */
  /* Determine the type of the temporary. */
  temp_type = expr->type;
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled && !(temp_type->kind == (a_type_kind)tk_typeref &&
                       typeref_is_typedef(temp_type)) &&
      is_variably_modified_type(temp_type)) {
    /* If the compound literal introduces a VLA type, we need to compute its
       dimension variables (this is similar to cast operations). */
    vla_inits = lower_vla_dimensions(temp_type);
    record_vla_component_types_for_lowering(expr->type);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
  result_is_addr = expr->variant.init.result_is_addr;
  if (result_is_addr) {
    /* The value of the enk_temp_init node is the address of the
       temporary, so drop the pointer-to to get the temporary type. */
    temp_type = type_pointed_to(temp_type);
  }  /* if */
  /* Create the temporary variable.  Note that compound literals created
     outside of functions do not use enk_temp_init so they are not
     seen here (the front end creates an initialized static variable
     for them). */
  dip->variable = var = make_lowered_temporary(temp_type);
  if (dip->is_partially_initialized_compound_literal) {
    var->is_partially_initialized = TRUE;
  }  /* if */
  if (vla_enabled && is_variably_modified_type(temp_type)) {
    var->has_variably_modified_type = TRUE;
  }  /* if */
  /* Change the enk_temp_init to a reference to the value or address
     of the temporary. */
  if (result_is_addr) {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable_address);
    /* The address of the temporary escapes (or might escape) into the
       surrounding context, so set its address_taken flag. */
    set_variable_address_taken(var);
  } else {
    set_expr_node_kind(expr, (an_expr_node_kind)enk_variable);
  }  /* if */
  expr->variant.variable = var;
  /* Set the insert point preceding the (modified) original expression. */
  set_expr_insert_location(expr, &insert_location);
  set_var_init_pos_descr(var, &ipd);
  /* Lower the initialization. */
  if (dip->kind == (a_dynamic_init_kind)dik_constant ||
      dip->kind == (a_dynamic_init_kind)dik_nonconstant_aggregate) {
    lower_designated_initializers(dip->variant.constant);
  }  /* if */
  lower_dynamic_init(dip, &ipd,
                     (a_constructor_init_ptr)NULL,
                     (a_variable_ptr)NULL,
                     LDIO_NONE,
                     /*others_follow_in_aggr=*/FALSE,
                     &insert_location,
                     &keep_dynamic_init,
                     (a_constant **)NULL);
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  /* After lowering, the type will no longer be variably-modified. */
  var->has_variably_modified_type = FALSE;
#else /* !(VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS) */
  if (var->has_variably_modified_type) {
    /* If the variable has variably-modified type, put out an stmk_vla_decl
       for it. */
    a_statement_ptr stmk_vla_decl_stmt =
                              alloc_statement((a_statement_kind)stmk_vla_decl);
    stmk_vla_decl_stmt->variant.vla.is_typedef_decl = FALSE;
    stmk_vla_decl_stmt->variant.vla.variant.variable = var;
    add_to_end_of_temp_init_statements_list(stmk_vla_decl_stmt);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
  if (keep_dynamic_init) {
    add_stmk_init_for_compound_literal(var, dip);
  }  /* if */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_inits != NULL) {
    /* Be sure to compute any needed VLA dimension variables before any
       expressions inside the compound literal braces. */
    an_expr_node_ptr  new_expr = make_comma_node(vla_inits, copy_node(expr));
    overwrite_node(expr, new_expr);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_temp_init */


static void lower_c99_expr_list(an_expr_node_ptr  list,
                                unsigned int      lvalue_mask)
/*
Lower the given (short) list of expressions (normally, the operands of an
operator).  lvalue_mask is a bit set indicating which of these expressions
are used as lvalues (the least significant bit corresponds to the first
expression).
*/
{
  an_expr_node_ptr  expr;

  for (expr = list; expr != NULL; expr = expr->next) {
    a_boolean  used_as_lvalue = (lvalue_mask & 1);
    lower_c99_expr(expr, used_as_lvalue);
    lvalue_mask >>= 1;
  }  /* if */
}  /* lower_c99_expr_list */


#if !MINIMAL_INLINING
/*ARGSUSED*/ /* <-- statement is not used in this case. */
#endif /* !MINIMAL_INLINING */
static void lower_c99_expr_full(an_expr_node_ptr  expr,
                                a_statement_ptr   statement,
                                a_boolean         used_as_lvalue)
/*
Transform the given expression to remove certain C99-specific constructs.
If statement is non-NULL, expr is the expression of the expression
statement "statement".  See lower_c99_expr for an interface without the
second parameter.
*/
{
  unsigned int  lvalue_mask = 0, bool_controlling_expr_mask = 0;

  switch (expr->kind) {
    case enk_operation:
      /* First lower all the operands (if any). */
      /* Determine which operands if any are lvalues, and whether or not
         the operand has boolean-controlling-expression operands. */
      set_lvalue_and_boolean_controlling_expr_masks(
                                                 expr, used_as_lvalue,
                                                 &lvalue_mask,
                                                 &bool_controlling_expr_mask);
      lower_c99_expr_list(expr->variant.operation.operands, lvalue_mask);
      /* Then transform the current operator if needed. */
      lower_c99_operator(expr);
#if LOWER_LVALUE_RETURNING_OPERATIONS
      /* Transform lvalue-returning "?" and "," operators into valid C. */
      lower_operations_returning_lvalue_instead_of_usual_rvalue(
                                                        expr, used_as_lvalue);
#endif /* LOWER_LVALUE_RETURNING_OPERATIONS */
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
    case enk_variable_address:
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
      if (expr->variant.variable->is_vla) {
        /* VLAs are lowered to pointers (to automatically managed storage).
           The pointer value should be used; not its address. */
        an_expr_node_ptr  new_expr = add_indirection_to_node(expr);
        /* For the enk_variable_address case, add_indirection_to_node should
           not create a wholly new entry. */
        check_assertion(expr == new_expr);
        /* Adjust the type of the expression node to the type the variable
           will eventually have (after lower_vla_types is complete) to
           avoid confusing parent nodes (e.g., an eok_pdiff operation
           expects operands of pointer type). */
        expr->type = make_pointer_type(
                                   underlying_array_element_type(expr->type));
      }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
      break;
    case enk_routine_address:
    case enk_variable:
    case enk_field:
    case enk_address_of_ellipsis:
      /* Nothing to be done. */
      break;
    case enk_runtime_sizeof:
      lower_runtime_sizeof(expr);
      break;
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:
      /* GNU C statement expression, ({...}). */
      {
#if MINIMAL_INLINING
        a_boolean saved_inlining_enabled = inlining_enabled;
        /* Turn off inlining, because the last statement creates a
           value that gets returned, and it needs to be an expression
           statement to get returned (inlining would turn it into a
           block statement). */
        inlining_enabled = FALSE;
#endif /* MINIMAL_INLINING */
        lower_c99_statement(expr->variant.statement);
#if MINIMAL_INLINING
        inlining_enabled = saved_inlining_enabled;
#endif /* MINIMAL_INLINING */
      }
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("Invalid C99 IL expression kind");
      break;
  }  /* switch */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled && !expr->type->visited_for_vla_lowering) {
    record_vla_component_types_for_lowering(expr->type);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_expr_full */


void lower_c99_expr(an_expr_node_ptr  expr,
                    a_boolean         used_as_lvalue)
/*
Do C99 lowering on the indicated expression.  If used_as_lvalue is TRUE, the
given expression is used as an lvalue (e.g., assigned to).
*/
{
  lower_c99_expr_full(expr, (a_statement_ptr)NULL, used_as_lvalue);
}  /* lower_c99_expr */


static void end_of_c99_full_expr(void)
/*
Do end-of-full-expression processing for C99 lowering.
*/
{
  /* Release any reable temporaries that were allocated. */
  release_reusable_temporaries();
}  /* end_of_c99_full_expr */


void lower_c99_full_expr(an_expr_node_ptr expr)
/*
Do C99 lowering on the indicated full expression.  A full expression is
one not contained inside another expression.
*/
{
  lower_c99_expr(expr, /*used_as_lvalue=*/FALSE);
  end_of_c99_full_expr();
}  /* lower_c99_full_expr */


static void lower_c99_vla_dimension(a_vla_dimension_ptr vdp)
/*
Lower the expression in a VLA dimension entry.
*/
{
  an_expr_node_ptr  expr = vdp->dimension_expr;

  if (expr != NULL) {
    lower_c99_full_expr(expr);
#if MINIMAL_INLINING
    /* Catch constant nonpositive sizes introduced by inlining. */
    if (is_constant_node(expr)) {
      a_constant_ptr con = expr->variant.constant;
      if (con->kind == (a_constant_repr_kind)ck_integer &&
          sign_of_integer_constant(con) <= 0) {
        pos_error(ec_array_size_must_be_positive, &vdp->position);
      }  /* if */
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
}  /* lower_c99_vla_dimension */


static void lower_c99_stmk_init(a_statement_ptr statement)
/*
Do C99 lowering on the indicated stmk_init statement.
*/
{
  a_dynamic_init_ptr dip = statement->variant.dynamic_init;

  /* This routine is similar to lower_stmk_init. */
  switch (dip->kind) {
    case dik_constant:
      /* A case that's valid in C89.  Lower the subtree but leave the
         stmk_init statement. */
      lower_c99_constant(dip->variant.constant);
      break;
    case dik_expression:
      /* A case that's valid in C89.  Lower the subtree but leave the
         stmk_init statement. */
      lower_c99_full_expr(dip->variant.expression);
      break;
    case dik_nonconstant_aggregate:
      /* An aggregate containing some non-constant parts. */
      /* This is not valid in C89, so convert the nonconstant parts to
         executable code. */
      { an_insert_location insert_location;
        a_boolean          keep_dynamic_init;
        an_init_pos_descr  ipd;
        a_variable_ptr     var = dip->variable;

        check_assertion(var != NULL);
        set_insert_location(statement, &insert_location);
        set_var_init_pos_descr(var, &ipd);
        lower_dynamic_init(dip, &ipd,
                           (a_constructor_init_ptr)NULL,
                           (a_variable_ptr)NULL,
                           LDIO_FULL_EXPR,
                           /*others_follow_in_aggr=*/FALSE,
                           &insert_location, &keep_dynamic_init,
                           (a_constant **)NULL);
        if (!keep_dynamic_init) {
          /* Delete the stmk_init statement. */
          turn_statement_into_noop(statement);
        }  /* if */
      }
      break;
    default:
      unexpected_condition_str("lower_c99_stmk_init: bad kind");
  }  /* switch */
}  /* lower_c99_stmk_init */


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

#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS

static void lower_set_vla_size(a_statement_ptr  stmt)
/*
Replace the stmk_set_vla_size statement by the computation of helper variables
holding the total number of elements at each level of the associated VLA type.
For multilevel VLA types, all the VLA components are handled with the first
stmk_set_vla_size statement and any other stmk_set_vla_size statement
associated with the same type is turned into a no-op.
*/
{
  a_vla_dimension_ptr  vla_dim = stmt->variant.vla_dimension;

  set_statement_kind(stmt, (a_statement_kind)stmk_expr);
  if (vla_dim->total_number_of_elements == NULL) {
    /* There is no associated variable yet: Compute all the necessary
       dimension quantities for the associated VLA type. */
    stmt->expr = lower_vla_dimensions(vla_dim->type);
  } else {
    /* Since there already is an associated variable, work for this entry was
       presumably done with a preceding stmk_set_vla_size entry.  Turn this
       statement into a no-op. */
    stmt->expr = node_for_host_large_integer((a_host_large_integer)0,
                                             (an_integer_kind)ik_int);
  }  /* if */
  /* The result of the expression is not used. */
  set_expr_result_not_used(stmt->expr);
}  /* lower_set_vla_size */


static a_routine_ptr  vla_alloc_routine = NULL;


static an_expr_node_ptr make_vla_allocation_expr(a_variable_ptr  vla_var)
/*
Create an expression that calls the run-time support library to allocate
storage for the given VLA variable.
*/
{
  an_expr_node_ptr  result = var_lvalue_expr(vla_var), size_expr;
  a_type_ptr        size_type = integer_type(targ_size_t_int_kind);

  result = add_lowered_cast_if_necessary(result, void_star_type());
  size_expr = vla_size_expr(vla_var->type, /*byte_count=*/TRUE);
  size_expr = add_lowered_cast_if_necessary(size_expr, size_type);
  result->next = size_expr;
  result = make_prototyped_runtime_call("__vla_alloc", &vla_alloc_routine,
                                        void_type(), void_star_type(),
                                        size_type, result);
  return result;
}  /* make_vla_allocation_expr */


static void lower_vla_decl(a_statement_ptr  stmt)
/*
Lower the given stmk_vla_decl statement.  If this is a VLA variable, allocate
memory for it.
*/
{
  a_variable_ptr  vla_var;

  if (stmt->variant.vla.is_typedef_decl) {
    vla_var = NULL;
  } else {
    vla_var = stmt->variant.vla.variant.variable;
  }  /* if */
  set_statement_kind(stmt, (a_statement_kind)stmk_expr);
  /* If necessary, allocate storage for the variable. */
  if (vla_var != NULL && vla_var->is_vla) {
    stmt->expr = make_vla_allocation_expr(vla_var);
  }  /* if */
  if (stmt->expr == NULL) {
    /* This may happen for simple VLA typedefs.  Create a dummy expression. */
    stmt->expr = node_for_host_large_integer((a_host_large_integer)0,
                                             (an_integer_kind)ik_int);
  }  /* if */
  /* The result of the statement expression is not used. */
  set_expr_result_not_used(stmt->expr);
}  /* lower_vla_decl */


static a_routine_ptr  vla_dealloc_routine = NULL;


static void lower_vla_dealloc(a_statement_ptr  stmt)
/*
Lower the given stmk_vla_dealloc statement.  Currently we call the run-time
support routine __vla_dealloc in all cases, but this could potentially be
optimized to only call that routine for the first allocated variable in the
scope (which would automatically deallocate all the other VLA variables as
well).
*/
{
  a_variable_ptr    vla_var = stmt->variant.vla_variable;
  an_expr_node_ptr  arg = var_lvalue_expr(vla_var);

  arg = add_lowered_cast_if_necessary(arg, void_star_type());
  set_statement_kind(stmt, (a_statement_kind)stmk_expr);
  stmt->expr = make_prototyped_runtime_call("__vla_dealloc",
                                            &vla_dealloc_routine,
                                            void_type(), void_star_type(),
                                            (a_type_ptr)NULL, arg);
  /* The result of the statement expression is not used. */
  set_expr_result_not_used(stmt->expr);
}  /* lower_vla_dealloc */

#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */

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
  if (statement != NULL) {
    a_statement_ptr   saved_temp_init_statements = temp_init_statements;
    a_source_position saved_error_position;

    temp_init_statements = NULL;
    /* Set the error position to the statement position, in case there is
       an error in lowering. */
    saved_error_position = error_position;
    set_position_from_stmt_source_position(error_position,
                                           statement->position);
    if (statement->expr != NULL &&
        statement->kind != (a_statement_kind)stmk_expr) {
      /* Lower the expression.  For an expression statement, that's done
         in a special way below. */
      lower_c99_expr_full(statement->expr, (a_statement_ptr)NULL,
                          /*used_as_lvalue=*/FALSE);
      end_of_c99_full_expr();
    }  /* if */
    switch (statement->kind) {
      case stmk_goto:
      case stmk_label:
#if GNU_EXTENSIONS_ALLOWED
      case stmk_assigned_goto:
#endif /* GNU_EXTENSIONS_ALLOWED */
      case stmk_return:
#if ASM_FUNCTION_ALLOWED
      case stmk_asm_func_body:
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      case stmk_decl:
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if REPRESENT_EMPTY_STATEMENTS_IN_IL
      case stmk_empty:
#endif /* REPRESENT_EMPTY_STATEMENTS_IN_IL */
#if UPC_EXTENSIONS_ALLOWED
      case stmk_upc_notify:
      case stmk_upc_wait:
      case stmk_upc_barrier:
      case stmk_upc_fence:
#endif /* UPC_EXTENSIONS_ALLOWED */
        /* Nothing to lower. */
        break; 
      case stmk_asm:
        lower_asm_statement(statement);
        break;
      case stmk_expr:
        /* Expression statement.  Pass in the statement to allow better
           inlining. */
        lower_c99_expr_full(statement->expr, statement,
                            /*used_as_lvalue=*/FALSE);
        end_of_c99_full_expr();
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
#if UPC_EXTENSIONS_ALLOWED
      case stmk_upc_forall:
#endif /* UPC_EXTENSIONS_ALLOWED */
        { a_for_loop_ptr flp = statement->variant.for_loop.extra_info;
          if (flp->initialization != NULL) {
            a_statement_ptr init_stmt = flp->initialization, init_stmt_next;
            lower_c99_statement(init_stmt);
            /* If the initialization was rewritten as a sequence of statements,
               make it into a block, because the stmk_for can only point at a
               single statement. */
            init_stmt_next = init_stmt->next;
            if (init_stmt_next != NULL) {
              init_stmt->next = NULL;
              change_statement_into_block(init_stmt, &init_stmt);
              init_stmt->next = init_stmt_next;
            }  /* if */
          }  /* if */
          if (flp->increment != NULL) {
            lower_c99_full_expr(flp->increment);
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
      case stmk_init:
        lower_c99_stmk_init(statement);
        break;
      case stmk_set_vla_size:
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
        lower_set_vla_size(statement);
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
        break;
      case stmk_vla_decl:
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
        lower_vla_decl(statement);
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
        break;
      case stmk_vla_dealloc:
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
        lower_vla_dealloc(statement);
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
        break;
      default:
        unexpected_condition_str("lower_c99_statement: bad statement kind");
    }  /* switch */
    insert_temp_init_statements(statement);
    temp_init_statements = saved_temp_init_statements;
    error_position = saved_error_position;
  }  /* if */
}  /* lower_c99_statement */


static void lower_c99_initializer(an_init_kind    init_kind,
                                  an_initializer  *initializer)
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
      /* That's handled when the stmk_init statement comes up. */
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


static void lower_c99_source_correspondence(
                                       a_source_correspondence *source_corresp)
/*
Do C99 lowering on the indicated source correspondence entry.
*/
{
#if REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
  rewrite_ucns_in_name(source_corresp);
#endif /* REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */
}  /* lower_c99_source_correspondence */


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
  lower_c99_source_correspondence(&var->source_corresp);
  lower_c99_initializer(var->init_kind, &var->initializer);
#if GNU_EXTENSIONS_ALLOWED
  if (force_variable_definition_via_zeroing && var->is_not_common &&
      var->storage_class == (a_storage_class)sc_unspecified &&
      var->init_kind == (an_init_kind)initk_none) {
    /* GNU C allows variables without initializers to be marked as "nocommon",
       which indicates that such variables are nontentative definitions.
       This can be translated to plain C IL by providing an explicitly
       zeroing initializer. */
    var->init_kind = (an_init_kind)initk_zero;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (var->has_variably_modified_type) {
    var->has_variably_modified_type = FALSE;
    record_vla_component_types_for_lowering(var->type);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
  error_position = saved_error_position;
}  /* lower_c99_variable */


static void lower_c99_type(a_type_ptr type)
/*
Do C99 lowering on the indicated type.  Note that, at present, this
need be called only for named types and tags, i.e., the things that are
on the scope types list.
*/
{
#if REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
  /* If there are no UCNs avoid some processing. */
  if (il_header.UCN_identifiers_used) {
    lower_c99_source_correspondence(&type->source_corresp);
    if (type->kind == (a_type_kind)tk_struct ||
        type->kind == (a_type_kind)tk_union) {
      /* Visit fields of structs and unions to rewrite UCNs in their names. */
      a_field_ptr field = type->variant.class_struct_union.field_list;
      for (; field != NULL; field = field->next) {
        lower_c99_source_correspondence(&field->source_corresp);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled &&
      type->kind == (a_type_kind)tk_typeref && typeref_is_typedef(type)) {
    record_vla_component_types_for_lowering(type->variant.typeref.type);
    if (type->variant.typeref.has_variably_modified_type) {
      /* The "has_variably_modified_type" flag will need to be cleared
         when the underlying type is lowered. */
      record_vla_type_for_lowering(type);
    }  /* if */
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_type */


static void lower_c99_routine(a_routine_ptr routine)
/*
Do C99 lowering on the indicated routine (the header, not the body).
*/
{
  lower_c99_source_correspondence(&routine->source_corresp);
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  if (vla_enabled) {
    record_vla_component_types_for_lowering(routine->type);
  }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_routine */


static void lower_c99_scope(a_scope_ptr scope)
/*
Do C99 lowering for all entities in and under the given scope.
*/
{
  a_variable_ptr                   variable;
  a_type_ptr                       type;
  a_routine_ptr                    routine;
  a_scope_ptr                      block_scope;
  a_vla_dimension_ptr              vla_dim;
  a_local_static_variable_init_ptr lsvip;
  a_context                        context;
  a_scope_ptr                      saved_innermost_function_scope =
                                                      innermost_function_scope;

  if (scope->kind == (a_scope_kind)sck_function) {
    /* Visit all VLA dimension expressions for parameters before pushing the
       function scope.  This matters when there are compound literals in
       the dimension expression. */
    for (vla_dim = scope->vla_dimensions;
         vla_dim != NULL;
         vla_dim = vla_dim->next) {
      /* Entries from prototype scopes are handled above. */
      if (vla_dim->in_prototype_scope) {
        /* Temporarily indicate that we're not inside a function. */
        innermost_function_scope = NULL;
        lower_c99_vla_dimension(vla_dim);
        innermost_function_scope = saved_innermost_function_scope;
      }  /* if */
    }  /* for */
  }  /* if */
  push_context(&context, scope, (an_object_lifetime_ptr)NULL);
  /* Mark the scope as lowered.  This is used by
     check_for_done_with_memory_region to tell whether the code for a function
     has been lowered yet. */
  mark_as_visited(scope);
  switch (scope->kind) {
    case sck_file:
    case sck_block:
      /* Nothing to lower. */
      break;
    case sck_function:
      innermost_function_scope = scope;
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
  /* Visit all types. */
  for (type = scope->types;
       type != NULL;
       type = type->next) {
    lower_c99_type(type);
  }  /* for */
  /* Visit all routines (the headers, not the bodies). */
  for (routine = scope->routines;
       routine != NULL;
       routine = routine->next) {
    lower_c99_routine(routine);
  }  /* for */
  /* Visit all block scopes (only present in function and block scopes). */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    lower_c99_scope(block_scope);
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
    /* Visit all VLA dimension expressions.  This must happen after the
       statements have been lowered to ensure that any needed VLA dimension
       variables have been created. */
    for (vla_dim = scope->vla_dimensions;
         vla_dim != NULL;
         vla_dim = vla_dim->next) {
      /* Entries from prototype scopes are handled above. */
      if (!vla_dim->in_prototype_scope) {
        lower_c99_vla_dimension(vla_dim);
      }  /* if */
    }  /* for */
    insert_temp_init_statements(scope->assoc_block);
#if MINIMAL_INLINING
    if (inlining_enabled && scope->variant.routine.ptr->is_inline) {
      /* For an inline routine, set the inlinable flag now that the body has
         been processed. */
      set_up_routine_for_inlining(scope);
    }  /* if */
#endif /* MINIMAL_INLINING */
  } else if (scope->kind == (a_scope_kind)sck_file) {
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
    if (vla_enabled) {
      lower_vla_types();
    }  /* if */
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
#if MINIMAL_INLINING
    if (inlining_enabled) {
      /* For any inline routines for which all calls were expanded inline,
         mark the routines as being unreferenced. */
      mark_inlined_routines_as_unreferenced();
    }  /* if */
#endif /* MINIMAL_INLINING */
  }  /* if */
  pop_context();
  innermost_function_scope = saved_innermost_function_scope;
}  /* lower_c99_scope */

#if LOWER_COMPLEX

static void lower_c99_imaginary_type(a_float_kind  kind,
                                     char          *name)
/*
Lower the C99 imaginary type whose precision is given by kind.
The lowered form is a typedef to one of the floating-point types.
The lowered type is given the name indicated by "name".
*/
{
  if (imaginary_type_used_in_primary_IL(kind)) {
    a_type_ptr  im_type = imaginary_type(kind);

    set_type_kind(im_type, (a_type_kind)tk_typeref);
    im_type->variant.typeref.type = float_type(kind);
    im_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    strcpy(im_type->source_corresp.name, name);
    add_to_front_of_file_scope_types_list(im_type);
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
  if (complex_type_used_in_primary_IL(kind)) {
    a_type_ptr   cmplx_type = complex_type(kind);
    a_type_ptr   lowered_repr = lowered_complex_type(kind);

    /* Typedef the complex type to its lowered representation. */
    set_type_kind(cmplx_type, (a_type_kind)tk_typeref);
    cmplx_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    strcpy(cmplx_type->source_corresp.name, name);
    cmplx_type->variant.typeref.type = lowered_repr;
#if MAINTAIN_NEEDED_FLAGS
    if (needed_flag_is_set(&cmplx_type->source_corresp)) {
      mark_as_needed((char *)lowered_repr, iek_type);
      set_class_definition_needed_flag(lowered_repr);
      set_class_keep_definition_in_il(lowered_repr);
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
    /* Link the types into the IL (in the right order). */
    add_to_front_of_file_scope_types_list(cmplx_type);
    add_to_front_of_file_scope_types_list(cmplx_type->variant.typeref.type);
  }  /* if */
}  /* lower_c99_complex_type */

#endif /* LOWER_COMPLEX */
#if LOWER_COMPLEX

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

#endif /* LOWER_COMPLEX */

static void lower_c99_bool_type(void)
/*
Replace the C99 _Bool type by its lowered representation.
*/
{
  if (bool_type_used_in_primary_IL()) {
    a_type_ptr type = bool_type();
    /* Clear the bool flag and make this a simple integral type. */
    type->variant.integer.bool_type = FALSE;
  }  /* if */
}  /* lower_c99_bool_type */

#if LOWER_FIXED_POINT

a_type_ptr lowered_integer_type_for_fixed_point_type(a_type_ptr fx_type)
/*
Return the integer type that is the lowered form of the indicated
fixed-point type.
*/
{
  an_integer_kind  ikind;
  a_targ_size_t    int_size;
  a_targ_alignment int_alignment;
  a_type_ptr       int_type;

  fx_type = skip_typerefs(fx_type);
  check_assertion(fx_type->kind == (a_type_kind)tk_fixed_point);
  for (ikind = 0; ; ikind++) {
    check_assertion_str(ikind < (int)ik_last,
            "lowered_integer_type_for_fixed_point_type: no suitable int type");
    get_integer_size_and_alignment(ikind, &int_size, &int_alignment);
    if (int_size == fx_type->size &&
        int_alignment == fx_type->alignment &&
        int_kind_is_signed[(int)ikind] ==
                                   !fx_type->variant.fixed_point.is_unsigned) {
      /* This integral type is okay. */
      break;
    }  /* if */
  }  /* for */
  int_type = integer_type(ikind);
  return int_type;
}  /* lowered_integer_type_for_fixed_point_type */


static void lower_c99_fixed_point_type(a_fixed_point_type_descr descr)
/*
Lower the C99 fixed-point type whose precision is given by kind.
The lowered form is a typedef to one of the integral types.
*/
{
  if (fixed_point_type_used_in_primary_IL(descr)) {
    a_type_ptr      fx_type = fixed_point_type(descr);
    char            name[30];
    a_type_ptr      int_type;

    /* Develop the name for the typedef. */
    (void)strcpy(name, "_Fixed_point_");
    if (descr.precision == (a_fixed_point_precision)fpp_short) {
      (void)strcat(name, "h");
    } else if (descr.precision == (a_fixed_point_precision)fpp_long) {
      (void)strcat(name, "l");
    }  /* if */
    if (descr.is_unsigned) {
      (void)strcat(name, "u");
    }  /* if */
    if (descr.is_fract_type) {
      (void)strcat(name, "r");
    } else {
      (void)strcat(name, "k");
    }  /* if */
    if (descr.saturating) {
      (void)strcat(name, "_sat");
    }  /* if */
    /* Determine the corresponding integral type (it must have the same
       size, alignment, and signedness). */
    int_type = lowered_integer_type_for_fixed_point_type(fx_type);
    set_type_kind(fx_type, (a_type_kind)tk_typeref);
    fx_type->variant.typeref.type = int_type;
    fx_type->source_corresp.name = alloc_il((sizeof_t)(strlen(name)+1));
    (void)strcpy(fx_type->source_corresp.name, name);
    add_to_front_of_file_scope_types_list(fx_type);
  }  /* if */
}  /* lower_c99_fixed_point_type */


static void lower_c99_fixed_point_types(void)
/*
Replace the fixed-point types by their lowered representations.
*/
{
  a_fixed_point_precision  precision;
  a_boolean                is_unsigned, is_fract_type, saturating;
  a_fixed_point_type_descr descr;

  for (precision = (a_fixed_point_precision)fpp_short;
       precision < (a_fixed_point_precision)fpp_last;
       precision++) {
    descr.precision = precision;
    for (is_unsigned = FALSE;; is_unsigned = TRUE) {
      descr.is_unsigned = is_unsigned;
      for (is_fract_type = FALSE;; is_fract_type = TRUE) {
        descr.is_fract_type = is_fract_type;
        for (saturating = FALSE;; saturating = TRUE) {
          descr.saturating = saturating;
          lower_c99_fixed_point_type(descr);
          if (saturating) break;
        }  /* for */
        if (is_fract_type) break;
      }  /* for */
      if (is_unsigned) break;
    }  /* for */
  }  /* for */
}  /* lower_c99_fixed_point_types */

#endif /* LOWER_FIXED_POINT */


void lower_c99_il_memory_region(a_memory_region_number region_number)
/*
Do C99 lowering for a memory region (for the file scope or a function scope).
*/
{
  a_scope_ptr scope = il_header.region_scope_entry[region_number];
  a_context   context;
  a_scope_ptr saved_innermost_function_scope = innermost_function_scope;

  il_lowering_underway = TRUE;
  curr_context = NULL;
  innermost_function_scope = NULL;
  curr_object_lifetime = NULL;
  switch_il_region(region_number);
  if (scope->kind == (a_scope_kind)sck_function) {
    /* Push the file scope around lowering of a function scope. */
    push_context(&context, il_header.primary_scope,
                 (an_object_lifetime_ptr)NULL);
  }  /* if */
  lower_c99_scope(scope);
  if (scope->kind == (a_scope_kind)sck_file) {
#if LOWER_COMPLEX
    lower_c99_nonreal_float_types();
#endif /* LOWER_COMPLEX */
    lower_c99_bool_type();
#if LOWER_FIXED_POINT
    if (fixed_point_enabled) {
      lower_c99_fixed_point_types();
    }  /* if */
#endif /* LOWER_FIXED_POINT */
  }  /* if */
  if (scope->kind == (a_scope_kind)sck_function) {
    pop_context();
  }  /* if */
  innermost_function_scope = saved_innermost_function_scope;
  il_lowering_underway = FALSE;
}  /* lower_c99_il_memory_region */


void lower_c99_one_time_init(void)
/*
Do one-time initialization of variables related to C99 IL lowering.
*/
{
  /* Save variables from lower_c99.c that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
#if LOWER_COMPLEX
      pch_saved_var_array_elem(lowered_complex_float),
      pch_saved_var_array_elem(lowered_complex_double),
      pch_saved_var_array_elem(lowered_complex_long_double),
      pch_array_saved_var_array_elem(xnegate_routine),
      pch_array_saved_var_array_elem(xadd_routine),
      pch_array_saved_var_array_elem(xsubtract_routine),
      pch_array_saved_var_array_elem(xmultiply_routine),
      pch_array_saved_var_array_elem(xdivide_routine),
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
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
      pch_saved_var_array_elem(fixed_conv_routine),
      pch_array_saved_var_array_elem(float_fixed_conv_routine),
      pch_array_saved_var_array_elem(fixed_float_conv_routine),
      pch_saved_var_array_elem(fixed_negate_routine),
      pch_saved_var_array_elem(fixed_eq_routine),
      pch_saved_var_array_elem(fixed_ne_routine),
      pch_saved_var_array_elem(fixed_gt_routine),
      pch_saved_var_array_elem(fixed_lt_routine),
      pch_saved_var_array_elem(fixed_ge_routine),
      pch_saved_var_array_elem(fixed_le_routine),
      pch_saved_var_array_elem(fixed_add_routine),
      pch_saved_var_array_elem(fixed_subtract_routine),
      pch_saved_var_array_elem(fixed_multiply_routine),
      pch_saved_var_array_elem(fixed_divide_routine),
      pch_saved_var_array_elem(fixed_shiftl_routine),
      pch_saved_var_array_elem(fixed_shiftr_routine),
      pch_saved_var_array_elem(fixed_incr_routine),
      pch_saved_var_array_elem(fixed_decr_routine),
#endif /* LOWER_FIXED_POINT */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
      pch_saved_var_array_elem(vla_types),
      pch_saved_var_array_elem(vla_alloc_routine),
      pch_saved_var_array_elem(vla_dealloc_routine),
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_one_time_init();
#endif /* MINIMAL_INLINING */
}  /* lower_c99_one_time_init */


void lower_c99_trans_unit_init(void)
/*
Initialize static variables related to C99 IL that must be initialized
for each translation unit.
*/
{
#if LOWER_COMPLEX
  { int k;
    for (k = 0; k < (int)fk_last; ++k) {
      xnegate_routine[k] = NULL;
      xadd_routine[k] = NULL;
      xsubtract_routine[k] = NULL;
      xmultiply_routine[k] = NULL;
      xdivide_routine[k] = NULL;
      xeq_routine[k] = NULL;
      xne_routine[k] = NULL;
    }  /* for */
  }
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
#endif /* LOWER_COMPLEX */
#if LOWER_FIXED_POINT
  fixed_conv_routine = NULL;
  fixed_negate_routine = NULL;
  fixed_eq_routine = NULL;
  fixed_ne_routine = NULL;
  fixed_gt_routine = NULL;
  fixed_lt_routine = NULL;
  fixed_ge_routine = NULL;
  fixed_le_routine = NULL;
  fixed_add_routine = NULL;
  fixed_subtract_routine = NULL;
  fixed_multiply_routine = NULL;
  fixed_divide_routine = NULL;
  fixed_shiftl_routine = NULL;
  fixed_shiftr_routine = NULL;
  fixed_incr_routine = NULL;
  fixed_decr_routine = NULL;
  { int k;
    for (k = 0; k < (int)fk_last; ++k) {
      float_fixed_conv_routine[k] = NULL;
      fixed_float_conv_routine[k] = NULL;
    }  /* for */
  }
#endif /* LOWER_FIXED_POINT */
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  vla_types = NULL;
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
  temp_init_statements = NULL;
#if MINIMAL_INLINING
  /* Do inline.c initialization. */
  if (inlining_enabled) inline_init();
#endif /* MINIMAL_INLINING */
  /* The following is also cleared in il_lower_init, but clear it here also
     to be sure. */
  il_lowering_underway = FALSE;
}  /* lower_c99_trans_unit_init */


void lower_c99_init(void)
/*
Initialize static variables related to C99 IL lowering that must be
initialized for each compilation.
*/
{
#if VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS
  /* The code to lower VLAs assumes that the deallocation points have been
     marked using stmk_vla_dealloc statements. */
  check_assertion(vla_dealloc_statements_in_il);
#endif /* VLA_ALLOWED && LOWER_VARIABLE_LENGTH_ARRAYS */
}  /* lower_c99_init */

#endif /* DO_C99_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2000-2004 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
