/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

folding.c -- Folding routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "folding.h"
#include "layout.h"
#include "exprutil.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#endif /* DO_IL_LOWERING */

/*
Determine the severity (error or warning) to be used for integer
operation overflows.
*/
#ifndef ES_INT_OVERFLOW
#if TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define ES_INT_OVERFLOW                                               \
  (strict_ansi_mode ? strict_ansi_error_severity : es_warning)
#else /* !TARG_NO_ERROR_ON_INTEGER_OVERFLOW */
#define ES_INT_OVERFLOW es_error
#endif /* TARG_NO_ERROR_ON_INTEGER_OVERFLOW */
#endif /* ifndef ES_INT_OVERFLOW */

#if FIXED_POINT_ALLOWED
/*
Determine the severity (error or warning) to be used for fixed-point
operation overflows.  This really has to be a warning even in strict
mode (unless we were to add a check for the current pragma state),
because some pragmas mandate saturating behavior (in which overflow
is by definition not an error).
*/
#ifndef ES_FIXED_POINT_OVERFLOW
#define ES_FIXED_POINT_OVERFLOW es_warning
#endif /* ifndef ES_FIXED_POINT_OVERFLOW */
#endif /* FIXED_POINT_ALLOWED */

a_boolean variable_has_non_null_address(a_variable_ptr vp)
/*
Return TRUE if the indicated variable has a non-NULL address.  That's
usually TRUE; the exceptions are variables like weak externals.
*/
{
  a_boolean has_non_null_addr =
                   vp->storage_class != (a_storage_class)sc_extern
#if GNU_EXTENSIONS_ALLOWED
                   && !vp->is_weak
#endif /* GNU_EXTENSIONS_ALLOWED */
                                  ;
  return has_non_null_addr;
}  /* variable_has_non_null_address */


a_boolean routine_has_non_null_address(a_routine_ptr rp)
/*
Return TRUE if the indicated routine has a non-NULL address.  That's
usually TRUE; the exceptions are routines like weak externals.
*/
{
  a_boolean has_non_null_addr =
                   rp->storage_class != (a_storage_class)sc_extern
#if GNU_EXTENSIONS_ALLOWED
                   && !rp->is_weak
#endif /* GNU_EXTENSIONS_ALLOWED */
                                  ;
  return has_non_null_addr;
}  /* routine_has_non_null_address */


a_boolean constant_bool_value_known_at_compile_time(a_constant_ptr con)
/*
con is a constant of a scalar type.  Return TRUE if the bool value it would
convert to is known at compile time.  (It might not be known if the
constant is an address that is not known until link time.)
*/
{
  a_boolean known_bool = TRUE;

  if (con->kind == (a_constant_repr_kind)ck_address) {
    an_address_base_kind kind = con->variant.address.kind;
    /* Addresses are non-null except possibly for extern variables and
       routines, which might have zero addresses because of linker magic
       like weak externals. */
    if (kind == (an_address_base_kind)abk_variable) {
      a_variable_ptr  vp = con->variant.address.variant.variable;
      known_bool = variable_has_non_null_address(vp);
    } else if (kind == (an_address_base_kind)abk_routine) {
      a_routine_ptr  rp = con->variant.address.variant.routine;
      known_bool = routine_has_non_null_address(rp);
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (con->kind == (a_constant_repr_kind)ck_label_difference) {
    a_constant_ptr  from = con->variant.label_difference.from_address;
    a_constant_ptr  to = con->variant.label_difference.to_address;
    if (constant_is_address_of_label(from) &&
        constant_is_address_of_label(to)) {
      known_bool = from->variant.address.variant.label ==
                                            to->variant.address.variant.label;
    } else {
      known_bool = FALSE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (con->kind == (a_constant_repr_kind)ck_template_param) {
    known_bool = FALSE;
#if UPC_EXTENSIONS_ALLOWED
  } else if (con->kind == (a_constant_repr_kind)ck_upc_mythread ||
             con->kind == (a_constant_repr_kind)ck_upc_threads) {
    /* The UPC pseudo-constants THREADS and MYTHREAD are not true constants. */
    known_bool = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  return known_bool;
}  /* constant_bool_value_known_at_compile_time */


void make_template_param_expr_constant(an_expr_node_ptr node,
                                       a_constant       *con)
/*
Create a template parameter constant that represents the indicated
expression.
*/
{
  clear_constant(con, (a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(con,
                              (a_template_param_constant_kind)tpck_expression);
  con->variant.template_param.variant.expr = node;
  con->type = node->type;
}  /* make_template_param_expr_constant */


void make_template_param_cast_constant(a_constant  *old_constant,
                                       a_constant  *new_constant,
                                       a_type_ptr  new_type,
                                       a_boolean   is_explicit)
/*
Make, in *new_constant, a ck_template_param/tpck_cast constant that
represents *old_constant cast to the type new_type.  is_explicit is set
to TRUE if the cast actually appeared in the source.
*/
{
  a_constant_ptr old_cp = alloc_shareable_constant(old_constant);

  clear_constant(new_constant, (a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(new_constant,
                                   (a_template_param_constant_kind)tpck_cast);
  new_constant->variant.template_param.variant.constant = old_cp;
  new_constant->type = new_type;
  new_constant->explicit_cast_applied = is_explicit;
}  /* make_template_param_cast_constant */


static void implicit_or_explicit_cast(a_constant_ptr cp,
                                      a_type_ptr     new_type,
                                      a_boolean      is_implicit_cast)
/*
Set the implicit_cast flag to indicate a type change of the indicated
constant to the indicated new type.  No representation change is implied.
The cast is implicit if is_implicit_cast is TRUE.
*/
{
  if (cp->expr != NULL &&
      (!is_implicit_cast || !identical_types(cp->type, new_type))) {
    cp->expr = make_operator_node((an_expr_operator_kind)eok_cast, new_type,
                                  cp->expr);
    cp->expr->variant.operation.compiler_generated = is_implicit_cast;
  }  /* if */
  cp->type = new_type;
  cp->implicit_cast = TRUE;
  if (!is_implicit_cast) {
    /* Note that the TRUE setting of explicit_cast_applied is sticky. */
    cp->explicit_cast_applied = TRUE;
  }  /* if */
  /* Clear the source correspondence information.  If this was a named
     constant, the new constant should no longer be
     associated with the original constant. */
  break_source_corresp(&cp->source_corresp);
}  /* implicit_or_explicit_cast */


void implicit_cast(a_constant_ptr cp,
                   a_type_ptr     new_type)
/*
Do an implicit cast of the indicated constant to the indicated new type.
No representation change is implied.  This is used for casting one
pointer type to another and casting integer constants to pointer types.
*/
{
  implicit_or_explicit_cast(cp, new_type, /*is_implicit_cast=*/TRUE);
}  /* implicit_cast */


void get_integer_attributes(a_constant      *cp,
                            an_integer_kind *ikind,
                            a_boolean       *is_signed,
                            int             *bit_size)
/*
For the integer type given by cp->type, return in *ikind the integer kind,
in *is_signed whether or not the type is signed, and in *bit_size the
size in bits of the integral type.
*/
{
  a_type_ptr    int_type = skip_typerefs(cp->type);
  a_targ_size_t size;

#if CHECKING
  if (int_type->kind != (a_type_kind)tk_integer) {
    internal_error("get_integer_attributes: not integral type");
  }  /* if */
#endif /* CHECKING */
  *ikind = int_type->variant.integer.int_kind;
  *is_signed = int_kind_is_signed[*ikind];
  size = int_type->size;
#if CHECKING
  if (size == 0) internal_error("get_integer_attributes: zero-sized integer");
#endif /* CHECKING */
  *bit_size = (int)(size * targ_char_bit);
}  /* get_integer_attributes */


void trunc_and_set_integer(an_integer_value  *result_value,
                           a_constant        *result,
                           a_boolean         check_overflow,
			   a_boolean	     saturate_on_overflow,
                           an_error_code     *err_code,
                           an_error_severity *err_severity)
/*
Truncate the integer result_value and store it in *result.  result->type
indicates the desired result type.  If check_overflow is TRUE and
*err_code indicates no previous error, check that the value fits in
the result type; if it does not, set *err_code and *err_severity to
indicate the error.  Whether or not the check is done, and whether or
not it succeeds, the value will be adjusted if necessary to ensure
that it fits.  saturate_on_overflow is TRUE if an overflow should
produce the largest (or smallest) value that will fit in the destination
type.
*/
{
  an_integer_kind  ikind;
  a_boolean        is_signed;
  int              bit_size;
  an_integer_value mask;

  /* Put the integer value into the result constant. */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = *result_value;
  get_integer_attributes(result, &ikind, &is_signed, &bit_size);
  /* Do the overflow check if necessary and if there's been no previous
     error. */
  if (in_range_for_integer_kind(result, result, ikind)) {
    /* The value is in the right range.  No truncation is needed. */
    goto after_truncation;
  }  /* if */
  /* The value will not fit in the destination integer type. */
  if (check_overflow && *err_code == ec_no_error) {
    /* Return an error code if the caller requested that we check for
       overflow. */
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  /* Truncate the value to the right size.  When saturate_on_overflow is
     TRUE, return the largest or smallest value that can be represented. */
  if (saturate_on_overflow) {
    if (sign_of_integer_constant(result) < 0) {
      result->variant.integer_value = min_integer_value_of_kind[ikind];
    } else {
      result->variant.integer_value = max_integer_value_of_kind[ikind];
    }  /* if */
  } else {
    make_integer_value_mask(&mask, bit_size);
    and_integer_values(&result->variant.integer_value, &mask);
  }  /* if */
  /* Sign-extend a signed result. */
  if (is_signed) {
    sign_extend_integer_value(&result->variant.integer_value, bit_size);
  }  /* if */
after_truncation:;
}  /* trunc_and_set_integer */


void conv_integer_to_integer(a_constant        *old_constant,
                             a_constant        *new_constant,
                             a_boolean         is_implicit_cast,
                             an_error_code     *err_code,
                             an_error_severity *err_severity)
/*
Convert an integral constant of some kind (in *old_constant) to a new
integral constant in *new_constant, with type as indicated therein.  Return
*err_code and *err_severity set to indicate any error/warning detected,
or *err_code == ec_no_error if everything went fine.  If is_implicit_cast
is FALSE, suppress any warnings.  The old_constant can have kind ck_integer,
ck_upc_threads, or ck_label_difference.  Generally it must have integral
type, but it may be an integer cast to a pointer type.
*/
{
  an_integer_value mask, old_value_copy;
  an_integer_kind  new_ikind, old_ikind;
  a_boolean        new_signed, old_signed;
  int              new_bit_size, old_bit_size;
  a_boolean        is_sign_change;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  if (is_incomplete_type(new_constant->type)) {
    /* In some severe error cases, the destination type may be an incomplete
       enum type. */
    *err_code = ec_incomplete_type_not_allowed;
    *err_severity = es_error;
    goto done;
  }  /* if */
  /* Copy the old value to the new value. */
  switch (old_constant->kind) {
    case ck_integer:
      set_constant_kind(new_constant, (a_constant_repr_kind)ck_integer);
      break;
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
      check_assertion(gnu_mode);
      set_constant_kind(new_constant,
                        (a_constant_repr_kind)ck_label_difference);
      /* This constant entry doesn't use variant.integer_value; so don't copy
         that variant field.  Instead copy the variant.label_difference
         part. */
      new_constant->variant.label_difference.from_address =
                          old_constant->variant.label_difference.from_address;
      new_constant->variant.label_difference.to_address =
                            old_constant->variant.label_difference.to_address;
      goto done;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
      check_assertion(upc_mode);
      set_constant_kind(new_constant,
                        (a_constant_repr_kind)ck_upc_threads);
      break;
#endif /* UPC_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition();
  }  /* switch */
  new_constant->variant.integer_value = old_constant->variant.integer_value;
  /* Determine attributes (size, signedness) of the new integer kind. */
  get_integer_attributes(new_constant, &new_ikind, &new_signed, &new_bit_size);
  /* Truncate the new value to the right size. */
  /* Note that the mask created here is used again later in this routine. */
  make_integer_value_mask(&mask, new_bit_size);
  and_integer_values(&new_constant->variant.integer_value, &mask);
  /* Sign-extend the new value if necessary. */
  if (new_signed) {
    sign_extend_integer_value(&new_constant->variant.integer_value,
                              new_bit_size);
  }  /* if */
  if (is_implicit_cast) {
    /* If the value changed, a warning is in order. */
    if (cmp_integer_constants(new_constant, old_constant) != 0 &&
        /* In some modes (e.g., Microsoft C mode), it is possible to
           implicitly convert a pointer to an integer type, so the old
           constant could be something like (void *)1.  Avoid the
           checking in such cases. */
        !is_pointer_type(old_constant->type)) {
      /* The new value is different than the old value.  See if the change
         is a truncation (dropping bits) or a sign change. */
      is_sign_change = FALSE;
      get_integer_attributes(old_constant, &old_ikind, &old_signed,
                             &old_bit_size);
      if (new_bit_size >= old_bit_size) {
        /* The new size is at least as big as the old size, so no truncation
           is possible.  Therefore, this must be a sign change. */
        is_sign_change = TRUE;
      } else {
        /* The new size is smaller than the old size, which means truncation
           is possible.  See if the significant part of the new value is the
           same as the old value.  If so, no bits have been lost, and
           this is a sign change. */
        old_value_copy = old_constant->variant.integer_value;
        if (old_signed && sign_of_integer_constant(old_constant) < 0) {
          /* Old constant is negative.  Turn on all the bits of the copy of
             the old constant down to where the sign bit is (or would be) in
             the new size.  If that gives a value that is equal to the
             old constant, then no information was lost, i.e., there is
             no interesting information -- just sign extension -- in the
             bits that don't fit into the new size. */
          make_integer_value_mask(&mask, new_bit_size-1);
          complement_integer_value(&mask);
          or_integer_values(&old_value_copy, &mask);
        } else {
          /* Old constant is unsigned or nonnegative.  Mask off all the
             bits of the old constant that do not fit in the new size.
             If that gives a value that is equal to the old constant,
             then no information was lost. */
          /* make_integer_value_mask(&mask, new_bit_size); -- already set. */
          and_integer_values(&old_value_copy, &mask);
        }  /* if */
        if (cmp_integer_values(&old_value_copy, old_signed,
                               &old_constant->variant.integer_value,
                               old_signed) == 0) {
          /* No significant bits were dropped, so this must be a sign
             change. */
          is_sign_change = TRUE;
        }  /* if */
      }  /* if */
      if (is_sign_change) {
        /* Sign change. */
        /* Do not issue this warning for non-arithmetic constants. */
        if (!old_constant->non_arithmetic) {
          *err_code = ec_integer_sign_change;
          *err_severity = es_warning;
        }  /* if */
      } else {
        /* Truncation. */
        *err_code = ec_integer_truncated;
        *err_severity = es_warning;
      }  /* if */
    }  /* if */
  }  /* if */
done:;
}  /* conv_integer_to_integer */


static void conv_integer_to_float(a_constant        *old_constant,
			          a_constant        *new_constant,
			          an_error_code     *err_code,
			          an_error_severity *err_severity)
/*
Convert an integer of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.
*/
{
  a_boolean               err;
  a_type_ptr              float_tp = skip_typerefs(new_constant->type);
  a_constant_repr_kind    constant_kind = (a_constant_repr_kind)ck_float;
  a_float_kind            float_kind = float_tp->variant.float_kind;
  an_internal_float_value *float_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;
#if C99_IL_EXTENSIONS_SUPPORTED
  /* We may be converting to a nonreal floating type. */
  if (float_tp->kind == (a_type_kind)tk_complex) {
    constant_kind = (a_constant_repr_kind)ck_complex;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    constant_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  set_constant_kind(new_constant, constant_kind);

#if C99_IL_EXTENSIONS_SUPPORTED
  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting to complex.  The integer value is converted into the real
       part, and the imaginary part is set to zero. */
    float_value = &new_constant->variant.complex_value->real;
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.complex_value->imag,
                                   &err);
    check_assertion_str2(!err, "conv_integer_to_float: cannot create zero",
                               "floating-point representation");
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting to imaginary.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &new_constant->variant.float_value, &err);
    check_assertion_str2(!err, "conv_integer_to_float: cannot create zero",
                               "floating-point representation");
    goto conversion_done;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    float_value = &new_constant->variant.float_value;
  }  /* if */
  /* Do the actual conversion of the integer constant to a float value. */
  conv_integer_value_to_float(&old_constant->variant.integer_value,
                              int_constant_is_signed(old_constant),
                              float_value, float_kind, &err);
#if C99_IL_EXTENSIONS_SUPPORTED
conversion_done:;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  if (err) {
    /* Some error. */
    *err_code = ec_integer_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_integer_to_float */


static void conv_float_to_integer(a_constant        *old_constant,
			          a_constant        *new_constant,
			          an_error_code     *err_code,
				  an_error_severity *err_severity,
                                  a_boolean         *depends_on_fp_mode,
				  a_boolean	    constant_context)
/*
Convert a float of some kind (in *old_constant) to an integer constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_fp_mode
is returned TRUE if the result has been determined but might be different
depending on the floating-point mode.  If constant_context is FALSE, this
operation is being evaluated as part of a nonconstant expression.
*/
{
  a_host_large_integer    int_value;
  a_host_large_unsigned   unsigned_int_value;
  an_integer_value        result_value;
  a_boolean               err, is_signed;
  a_type_ptr              float_tp = skip_typerefs(old_constant->type);
  a_float_kind            float_kind = float_tp->variant.float_kind;
  an_internal_float_value *float_value;
  a_boolean		  is_negative;
#if C99_IL_EXTENSIONS_SUPPORTED
  an_internal_float_value zero;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if C99_IL_EXTENSIONS_SUPPORTED
  if (float_tp->kind == (a_type_kind)tk_complex) {
    /* Converting from complex to integer.  The real part of the
       constant is converted to integer, and the imaginary part is
       discarded. */
    float_value = &old_constant->variant.complex_value->real;
  } else if (float_tp->kind == (a_type_kind)tk_imaginary) {
    /* Converting from imaginary to integer.  The result is zero. */
    fp_host_large_integer_to_float(float_kind, (a_host_large_integer)0,
                                   &zero, &err);
    float_value = &zero;
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  /* Do not insert code here. */
  {
    float_value = &old_constant->variant.float_value;
  }  /* if */

  *err_code = ec_no_error;
  *err_severity = es_warning;

  is_signed = int_constant_is_signed(new_constant);
  is_negative = fp_is_negative(float_kind, float_value);
  if (is_signed || is_negative) {
    /* Destination is a signed integer or the source value is negative.
       When the source value is negative, we convert to a signed value
       because we may use the resulting bit pattern as an unsigned value. */
    fp_to_host_large_integer(float_kind, float_value,
                             &int_value, &err, depends_on_fp_mode);
    /* We set the result value even if an error occurred.  This value
       is used in some modes. */
    set_integer_value(&result_value, int_value);
    /* Set the error flag if we the source value is negative and the result
       was intended to be unsigned. */
    if (!is_signed) err = TRUE;
  } else {
    /* Destination is an unsigned integer. */
    fp_to_host_large_unsigned(float_kind, float_value,
                              &unsigned_int_value, &err,
                              depends_on_fp_mode);
    /* We set the result value even if an error occurred.  This value
       is used in some modes. */
    set_unsigned_integer_value(&result_value, unsigned_int_value);
  }  /* if */
#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  if (err) {
    /* Try again with larger precision by converting the float to a
       character string then converting the string to an integer value. */
    a_boolean pos_infinity, neg_infinity, not_a_number;
    char *str = fp_to_string(float_kind, float_value,
                             &pos_infinity, &neg_infinity, &not_a_number);
    if (pos_infinity || neg_infinity || not_a_number) {
      err = TRUE;
    } else {
      if (!is_signed && is_negative) {
        /* The source value is negative but the result value is unsigned
           do the conversion to a signed value because the resulting bit
           pattern may be used later in some modes. */
        conv_float_string_to_integer_value(str, &result_value,
                                           /*is_signed=*/TRUE, &err);
        /* Always set the error flag in this case. */
        err = TRUE;
      } else {
        conv_float_string_to_integer_value(str, &result_value,
                                           is_signed, &err);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (err && gcc_mode && gnu_version >= 30400) {
    /* The float value cannot be represented as an integer value (or an
       unsigned integer value).  Use the largest or smallest (depending on
       the sign of the float) value that can be represented. */
    make_saturated_integer_for_float(float_kind, float_value, &result_value,
                                     new_constant);
  }  /* if */
  trunc_and_set_integer(&result_value, new_constant,
                        /*check_overflow=*/!err,
                        /*saturate_on_overflow=*/gcc_mode &&
                                                 gnu_version >= 30400,
                        err_code, err_severity);
  if (err || *err_code != ec_no_error) {
    /* Float value is too big to fit in the integer. */
    *err_code = ec_float_to_integer_conversion;
    /* In GNU C and Microsoft C mode, only give a warning on an out-of-range
       value in a constant context.  An es_error severity is returned in
       non-constant contexts.  This causes the folded value to be discarded
       and the operation to be evaluated at run time.  In such cases the
       severity is reduced to a warning by issue_folding_diagnostic. */
    *err_severity = constant_context &&
                    (gcc_mode || (microsoft_mode && C_mode())) ? es_warning
                                                               : es_error;
  }  /* if */
}  /* conv_float_to_integer */


static void conv_float_to_float(a_constant           *old_constant,
			        a_constant           *new_constant,
			        an_error_code        *err_code,
				an_error_severity    *err_severity,
                                a_boolean            *depends_on_fp_mode)
/*
Convert a float of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_fp_mode
is returned TRUE if the result has been determined but might be different
depending on the floating-point mode.
*/
{
  a_boolean            err;
  a_type_ptr           old_type = skip_typerefs(old_constant->type);
  a_type_ptr           new_type = skip_typerefs(new_constant->type);
  a_float_kind         old_kind = old_type->variant.float_kind;
  a_float_kind         new_kind = new_type->variant.float_kind;
  a_constant_repr_kind new_constant_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

#if C99_IL_EXTENSIONS_SUPPORTED
  /* We may be converting to a nonreal floating type. */
  if (new_type->kind == (a_type_kind)tk_complex) {
    new_constant_kind = (a_constant_repr_kind)ck_complex;
  } else if (new_type->kind == (a_type_kind)tk_imaginary) {
    new_constant_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

  set_constant_kind(new_constant, new_constant_kind);

#if C99_IL_EXTENSIONS_SUPPORTED
  if ((old_type->kind != (a_type_kind)tk_float ||
       new_type->kind != (a_type_kind)tk_float) &&
      (old_type->kind != (a_type_kind)tk_imaginary ||
       new_type->kind != (a_type_kind)tk_imaginary)) {
    /* Conversion involving complex or imaginary types, but not the
       simple imaginary --> imaginary case. */
    switch (old_type->kind) {
      case tk_float:
        switch (new_type->kind) {
          case tk_imaginary:
            /* Float to imaginary.  The result is zero. */
            fp_host_large_integer_to_float(new_kind, (a_host_large_integer)0,
                                           &new_constant->variant.float_value,
                                           &err);
            break;
          case tk_complex:
            /* Float to complex. */
            fp_change_kind(&old_constant->variant.float_value, old_kind,
                           &new_constant->variant.complex_value->real,
                           new_kind, &err, depends_on_fp_mode);
            fp_host_large_integer_to_float(
                                    new_kind, (a_host_large_integer)0,
                                    &new_constant->variant.complex_value->imag,
                                    &err);
            break;
          default:
            unexpected_condition_str(
                                "conv_float_to_float: from float to bad type");
        }  /* switch */
        break;
      case tk_imaginary:
        switch (new_type->kind) {
          case tk_float:
            /* Imaginary to float.  Result is zero. */
            fp_host_large_integer_to_float(new_kind, (a_host_large_integer)0,
                                           &new_constant->variant.float_value,
                                           &err);
            break;
          case tk_complex:
            /* Imaginary to complex. */
            fp_host_large_integer_to_float(
                                    new_kind, (a_host_large_integer)0,
                                    &new_constant->variant.complex_value->real,
                                    &err);
            fp_change_kind(&old_constant->variant.float_value, old_kind,
                           &new_constant->variant.complex_value->imag,
                           new_kind, &err, depends_on_fp_mode);
            break;
          default:
            unexpected_condition_str(
                            "conv_float_to_float: from imaginary to bad type");
        }  /* switch */
        break;
      case tk_complex:
        switch (new_type->kind) {
          case tk_float:
            /* Complex to float.  Retain the real part only. */
            fp_change_kind(&old_constant->variant.complex_value->real,
                           old_kind, &new_constant->variant.float_value,
                           new_kind, &err, depends_on_fp_mode);
            break;
          case tk_imaginary:
            /* Complex to imaginary.  Retain the imaginary part only. */
            fp_change_kind(&old_constant->variant.complex_value->imag,
                           old_kind, &new_constant->variant.float_value,
                           new_kind, &err, depends_on_fp_mode);
            break;
          case tk_complex:
            /* Complex to complex. */
            /* This is similar to the float-float or imaginary-imaginary cases,
               but both the real and the imaginary components must change. */
            fp_change_kind(&old_constant->variant.complex_value->real,
                           old_kind,
                           &new_constant->variant.complex_value->real,
                           new_kind, &err, depends_on_fp_mode);
            fp_change_kind(&old_constant->variant.complex_value->imag,
                           old_kind,
                           &new_constant->variant.complex_value->imag,
                           new_kind, &err, depends_on_fp_mode);
            break;
          default:
            unexpected_condition_str(
                              "conv_float_to_float: from complex to bad type");
        }  /* switch */
        break;
      default:
        unexpected_condition_str(
                               "conv_float_to_float: bad floating-point type");
    }  /* switch */
  } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  {
    /* Singular floating-point types in the same domain (i.e., two real or
       imaginary constants). */
    fp_change_kind(&old_constant->variant.float_value, old_kind,
                   &new_constant->variant.float_value, new_kind,
                   &err, depends_on_fp_mode);
  }  /* if */
  if (err) {
    *err_code = ec_float_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_float_to_float */


static void get_pointer_offset(a_constant_ptr constant,
                               a_constant_ptr offset)
/*
Retrieve and return the offset part of the given pointer constant in
integer constant form.  Note that this routine works when
applied to an address constant that has been cast to an integral type.
*/
{
  switch (constant->kind) {
    case ck_address:
      /* Address of a routine, variable, or constant, plus some offset. */
      set_integer_constant(
                offset, (a_host_large_integer)constant->variant.address.offset,
                targ_ptrdiff_t_int_kind);
      break;
    case ck_integer:
      /* Integer cast to a pointer type (probably 0/NULL). */
      *offset = *constant;
      break;
#if CHECKING
    default:
      internal_error("get_pointer_offset: bad kind");
#endif /* CHECKING */
  }  /* switch */
}  /* get_pointer_offset */


static void set_pointer_offset(a_constant_ptr constant,
                               a_constant_ptr offset,
                               a_boolean      *err)
/*
Put the indicated offset into the pointer constant.  Return *err TRUE
if the value will not fit in the pointer constant.  Note that this routine
works when applied to an address constant that has been cast to an
integral type.
*/
{
  switch (constant->kind) {
    case ck_address:
      constant->variant.address.offset= value_of_integer_constant(offset, err);
      break;
    case ck_integer:
      *constant = *offset;
      break;
#if CHECKING
    default:
      internal_error("set_pointer_offset: bad pointer constant kind");
#endif /* CHECKING */
  }  /* switch */
}  /* set_pointer_offset */


static char *base_object(a_constant *constant)
/*
Return a pointer to the "base object" that underlies the pointer constant.
This is NULL if the pointer is an integer cast to a pointer type.  Otherwise,
it points to the variable, routine, or constant entry.
*/
{
  char *object;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    /* No base object. */
    object = NULL;
  } else {
#if CHECKING
    if (constant->kind != (a_constant_repr_kind)ck_address) {
      internal_error("base_object: not ck_integer or ck_address");
    }  /* if */
#endif /* CHECKING */
    switch (constant->variant.address.kind) {
      case abk_variable:
        object = (char *)constant->variant.address.variant.variable;
        break;
      case abk_routine:
        object = (char *)constant->variant.address.variant.routine;
        break;
      case abk_constant:
        object = (char *)constant->variant.address.variant.constant;
        break;
      case abk_uuidof:
      case abk_typeid:
        /* Use the address constant as the "base object" for a __uuidof or
           typeid construct.  It's weird, but we need to return a non-NULL
           base object for this case, and the constant seems like the best of
           the possibilities. */
        object = (char *)constant;
        break;
      case abk_label:
        object = (char *)constant->variant.address.variant.label;
        break;
#if CHECKING
      default:
        internal_error("base_object: bad address constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return object;
}  /* base_object */


void fold_base_class_cast(a_constant        *constant_1,
                          a_base_class      *bcp,
                          a_type_ptr        qualifiers_model,
                          a_constant        *result,
                          a_boolean         check_cast_access,
                          a_boolean         check_ambiguity,
                          a_boolean         is_implicit_cast,
                          a_boolean         is_object_pointer,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos,
                          an_error_code     *error_detected)
/*
Fold a C++ cast of a class pointer to a base class pointer.  constant_1 is
an address of a class object.  It is converted to a pointer to the base
class indicated by bcp and the new constant is returned in *result.
qualifiers_model is a class type whose cv-qualification indicates
the cv-qualification desired on the result (i.e., the result type is
the base class type of bcp and the cv-qualifiers of qualifiers_model).
result->type need not be set on entry.  Do access control on the cast
if check_cast_access is TRUE.  Check for ambiguity on the cast if
check_ambiguity is TRUE.  The cast is implicit if is_implicit_cast is
TRUE.  The pointer is known to point to an object if is_object_pointer
is TRUE.  If the operation cannot be folded, *did_not_fold is returned
TRUE.  If there is an error, issue it at *err_pos.  If error_detected
is non-NULL, set *error_detected to the code for any error detected,
and do not issue the diagnostic, or set it to ec_no_error if there was
no error.
*/
{
  a_boolean             err;
  a_type_ptr            orig_type, curr_type, new_type;
  a_derivation_step_ptr dsp;
  a_constant            offset;
  an_integer_value      base_class_offset;
  a_base_class_ptr      base_class;

  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  /* The code here looks like add_base_class_casts. */
  if (bcp->ambiguous && check_ambiguity) {
    /* The base class is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_base_class;
    } else {
      pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    an_expr_node_ptr expr = constant_1->expr;
    constant_1->expr = NULL;
    copy_constant(constant_1, result);
    /* Loop through the classes between the derived class and the
       base class.  Check accessibility at each step and generate the
       necessary casts. */
    /* No access checking in prototype instantiations. */
    if (in_front_end &&
        scope_stack[depth_scope_stack].in_prototype_instantiation) {
      check_cast_access = FALSE;
    }  /* if */
    orig_type = type_pointed_to(constant_1->type);
    curr_type = skip_typerefs(orig_type);
    for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
      base_class = dsp->base_class;
      /* Check that the base class is accessible from the current class.
         Accessibility is not checked if the cast is explicit. */
      if (check_cast_access) {
        if (!is_accessible_imm_base_class(base_class, curr_type)) {
          /* The base class is inaccessible. */
          if (error_detected != NULL) {
            if (is_effective_error(ec_inaccessible_base_class,
                                   es_discretionary_error)) {
              *error_detected = ec_inaccessible_base_class;
            }  /* if */
          } else {
            pos_ty_diagnostic(es_discretionary_error,
                              ec_inaccessible_base_class, err_pos,
                              base_class->type);
          }  /* if */
          /* Keep going, but don't check access any further to avoid putting
             out more than one error. */
          check_cast_access = FALSE;
        }  /* if */
      }  /* if */
      /* Adjust the address to reflect the cast to the next level. */
      curr_type = base_class->type;
      get_pointer_offset(constant_1, &offset);
      if (!is_object_pointer &&
          cmplit_integer_constant(&offset, (a_host_large_integer)0) == 0 &&
          base_object(constant_1) == NULL) {
        /* Preserve a NULL pointer.  Note that we suppress this test when
           is_object_pointer is TRUE, to allow the usual idiom for the
           offsetof macro to work. */
      } else {
        if (any_virtual_steps_in_derivation(base_class)) {
          /* Casting to a virtual base class.  This can only be folded if we
             have a complete object of the derived class type. */
          if (pointer_con_complete_object_type(constant_1) != NULL) {
            /* The constant is the unmodified address of a variable.  We know
               the variable has the proper class type or we wouldn't have
               identified the cast as a base class cast.  We don't try to
               handle any cases where the address has been cast to another
               type because we don't have the history of casts -- there may
               have been several, and they might not all have been base
               class casts. */
          } else {
            /* We cannot fold the cast. */
            *did_not_fold = TRUE;
            break;
          }  /* if */
        }  /* if */
        /* Take the pointer offset, ... */
        /* ... add the offset to the base class, ... */
        set_unsigned_integer_value(&base_class_offset, base_class->offset);
        add_integer_values(&offset.variant.integer_value, &base_class_offset,
                           int_constant_is_signed(&offset), &err);
        /* ... and put the offset into the result pointer constant.  Note
           that no overflow/object-size checking is needed, since the base
           class has to be within the underlying object. */
        set_pointer_offset(result, &offset, &err);
      }  /* if */
    }  /* for */
    /* Set the constant type.  It includes all the type qualifiers from the
       qualifiers_model. */
    new_type = make_identically_qualified_type(curr_type, qualifiers_model);
    implicit_or_explicit_cast(result, make_pointer_type(new_type),
                              is_implicit_cast);
    /* Record the backing expression if the folding was successful. */
    if (*did_not_fold) {
      expr = NULL;
    } else if (expr != NULL) {
      a_boolean local_error_detected;
      add_base_class_casts(bcp, qualifiers_model, /*check_cast_access=*/FALSE,
                           /*check_ambiguity=*/FALSE,
                           is_implicit_cast, /*implicit_in_naming=*/FALSE,
                           &expr, err_pos, &local_error_detected);
      check_assertion(!local_error_detected);
    }  /* if */
    result->expr = expr;
  }  /* if */
}  /* fold_base_class_cast */


static void fold_derived_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_source_position *err_pos,
                                    an_error_code     *error_detected)
/*
Fold a C++ cast of a class pointer to a derived class pointer.  constant_1 is
an address of a class object.  It is converted to point to the pointer type
indicated by result->type, and the new constant is returned in *result.
bcp points to the base class entry for the current type relative to the
desired derived type.  If there is an error, it is issued at *err_pos.
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.
*/
{
  a_type_ptr       new_type = result->type, derived_class_type;
  a_constant       offset;
  an_integer_value base_class_offset;
  a_boolean        err;

  /* The code here looks like add_derived_class_casts. */
  if (error_detected != NULL) *error_detected = ec_no_error;
  derived_class_type = f_skip_typerefs(type_pointed_to(new_type));
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_derived_class;
    } else {
      pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                    bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    if (error_detected != NULL) {
      *error_detected = ec_derived_class_from_virtual_base;
    } else {
      pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                    derived_class_type, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    an_expr_node_ptr expr = constant_1->expr;
    constant_1->expr = NULL;
    copy_constant(constant_1, result);
    /* Determine the offset and adjust it for the cast. */
    get_pointer_offset(result, &offset);
    if (cmplit_integer_constant(&offset, (a_host_large_integer)0) == 0 &&
        base_object(result) == NULL) {
      /* Preserve a NULL pointer. */
    } else {
#if CHECKING
      if (any_virtual_steps_in_derivation(bcp)) {
        internal_error("fold_derived_class_cast: virtual base class");
      }  /* if */
#endif /* CHECKING */
      /* Take the pointer offset, ... */
      /* ... subtract the offset to the base class, ... */
      set_unsigned_integer_value(&base_class_offset, bcp->offset);
      subtract_integer_values(&offset.variant.integer_value,
                              &base_class_offset,
                              int_constant_is_signed(&offset), &err);
      /* ... and put the offset into the result pointer constant.  Note
         that no overflow/object-size checking is needed, since the base
         class has to be within the underlying object. */
      set_pointer_offset(result, &offset, &err);
    }  /* if */
    implicit_or_explicit_cast(result, new_type, /*is_implicit_cast=*/FALSE);
    /* Update the backing expression if one was present. */
    if (expr != NULL) {
      a_boolean local_error_detected;
      add_derived_class_casts(type_pointed_to(new_type), bcp,
                              /*check_ambiguity=*/FALSE,
                              /*requires_runtime_check=*/FALSE,
                              &expr, err_pos,
                              &local_error_detected);
      check_assertion(!local_error_detected);
    }  /* if */
    result->expr = expr;
  }  /* if */
}  /* fold_derived_class_cast */


static void conv_pointer_to_whatever(
                                    a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         check_cast_access,
                                    a_boolean         check_ambiguity,
                                    a_boolean         is_implicit_cast,
                                    a_boolean         fold_constant_addr_exprs,
                                    a_boolean         is_reinterpret_cast,
                                    a_boolean         *did_not_fold,
                                    a_source_position *err_pos,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity,
                                    a_boolean         suppress_complex_diags)
/*
Convert a pointer constant to a constant of type as specified by
"new_constant".  If check_cast_access is TRUE, do access checking.
If check_ambiguity is TRUE, check for ambiguous base class casts.
If is_implicit_cast is TRUE, the cast is implicit.  If
fold_constant_addr_exprs is TRUE, fold related class casts in constant
form; if it's FALSE, do not do such folding and return *did_not_fold
TRUE.  If is_reinterpret_cast is TRUE, this is a reinterpret_cast;
related class casts are treated like casts between unrelated classes.
If there is an error, either issue it immediately at *err_pos (if it
cannot be reduced to a warning in a nonconstant context), or return
*err_code and *err_severity set appropriately.  If suppress_complex_diags
is TRUE, suppress (and return in err_code/err_severity) also those
complex diagnostics (e.g., those for access errors) that can't be
issued simply from the error code.  Note that this routine
is also called when the old constant is an address constant that has
previously been cast to an integral type, and so does not have pointer
type.
*/
{
  a_type_ptr       new_type = new_constant->type;
  a_type_ptr       old_type = old_constant->type;
  a_boolean        conversion_handled = FALSE, baseward_cast;
  a_base_class_ptr bcp;
  an_error_code    *p_err_code = NULL;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (suppress_complex_diags) p_err_code = err_code;
#if CHECKING
  if (old_constant->kind != (a_constant_repr_kind)ck_address &&
      old_constant->kind != (a_constant_repr_kind)ck_integer) {
    internal_error("conv_pointer_to_whatever: not ck_address or ck_integer");
  }  /* if */
#endif /* CHECKING */
  /* Change of pointer type for an address constant. */
  /* Change of pointer type for an integer cast to a pointer type. */
  /* Change of an address constant previously cast to integer to another
     type. */
  if (is_integral_or_enum_type(new_type)) {
    /* Pointer value being forced into an integral type. */
    if (old_constant->kind == (a_constant_repr_kind)ck_integer) {
      /* A constant that is an integer, cast to some pointer type and back
         to integer, as in (int)(void*)-1: make sure the integer is
         truncated and sign-extended properly, with warnings if
         appropriate. */
      conv_integer_to_integer(old_constant, new_constant, is_implicit_cast,
                              err_code, err_severity);
      conversion_handled = TRUE;
    } else if (skip_typerefs(new_type)->size < skip_typerefs(old_type)->size) {
      /* The integral type is not large enough to hold a pointer. */
      *err_code = ec_integer_truncated;
      *err_severity = es_error;
    }  /* if */
  } else if (is_floating_type(new_type)) {
    /* Converting an address to a floating-point type cannot be done at
       compile-time. */
    *did_not_fold = TRUE;
  } else if (is_reinterpret_cast) {
    /* Suppress the related-class processing for reinterpret_casts.  If
       constant addressing expressions are not being folded, keep the
       reinterpret_cast in executable form. */
    if (!fold_constant_addr_exprs) {
      *did_not_fold = TRUE;
    }  /* if */
  } else if (related_class_pointers(old_type, new_type,
                                    &baseward_cast, &bcp)) {
    /* In C++, a cast of a pointer to a class to a pointer to a base class
       or derived class. */
    conversion_handled = TRUE;
    /* Do not fold such casts in constant form unless told to.  That's to
       preserve detailed addressing information in the IL. */
    if (!fold_constant_addr_exprs) {
      *did_not_fold = TRUE;
    } else if (baseward_cast) {
      /* Derived --> base.  Valid unless the cast is ambiguous or
         the base class is inaccessible. */
      fold_base_class_cast(old_constant, bcp, type_pointed_to(new_type),
                           new_constant,
                           check_cast_access, check_ambiguity,
                           is_implicit_cast,
                           /*is_object_pointer=*/FALSE, did_not_fold, err_pos,
                           p_err_code);
      if (p_err_code != NULL && *err_code != ec_no_error) {
        *err_severity = es_error;
      }  /* if */
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      fold_derived_class_cast(old_constant, bcp, new_constant, err_pos,
                              p_err_code);
      if (p_err_code != NULL && *err_code != ec_no_error) {
        *err_severity = es_error;
      }  /* if */
    }  /* if */
    /* If the qualifiers aren't right, adjust them. */
    if (!*did_not_fold && 
        !is_error_type(new_constant->type) &&
        !identical_types(new_constant->type, new_type)) {
      implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
    }  /* if */
  }  /* if */
  /* Do the cast (by calling implicit_cast) unless there was an error or
     the cast has already been handled. */
  if (!conversion_handled && !*did_not_fold &&
      (*err_code == ec_no_error || *err_severity != es_error)) {
    copy_constant(old_constant, new_constant);
    implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  }  /* if */
}  /* conv_pointer_to_whatever */


static a_boolean pm_constant_is_null(a_constant_ptr constant)
/*
constant is a pointer-to-member constant.  Return TRUE if it is a NULL
constant.
*/
{
  a_boolean is_null = constant->variant.ptr_to_member.is_function_ptr ?
                    (constant->variant.ptr_to_member.variant.routine == NULL) :
                    (constant->variant.ptr_to_member.variant.field == NULL);
  return is_null;
}  /* pm_constant_is_null */


static a_type_ptr pm_constant_member_class(a_constant_ptr constant)
/*
constant is a non-NULL pointer-to-member constant.  Return the class
of which the underlying member is a member.
*/
{
  a_type_ptr class_type;

  if (constant->variant.ptr_to_member.is_function_ptr) {
    /* The underlying member is a function. */
    class_type = parent_class_of(
                             constant->variant.ptr_to_member.variant.routine);
  } else {
    /* The underlying member is a nonstatic data member. */
    class_type =
               parent_class_of(constant->variant.ptr_to_member.variant.field);
  }  /* if */
  return class_type;
}  /* pm_constant_member_class */


static void set_pm_cast_base_class(a_constant_ptr   constant, 
                                   a_type_ptr       new_type,
                                   a_base_class_ptr bcp,
                                   a_boolean        cast_to_base,
                                   a_boolean        is_implicit_cast,
                                   a_boolean        *did_not_fold)
/*
constant is a pointer-to-member constant.  Cast it to new_type, which is
a pointer to member of the class indicated by bcp.  If cast_to_base is TRUE,
this cast is toward a base class; otherwise, it is toward a derived class
(in which case bcp gives the base class entry for the current class as
a base class of the derived class).  is_implicit_cast is TRUE if the
cast is implicit.  If the cast cannot be folded, *did_not_fold is
returned TRUE.
*/
{
  a_type_ptr       member_class, new_class;
  a_targ_ptrdiff_t offset;
  a_base_class_ptr casting_base_class;

  *did_not_fold = FALSE;
  if (pm_constant_is_null(constant)) {
    /* A NULL pointer-to-member keeps a NULL casting_base_class even
       when cast to another type. */
    implicit_or_explicit_cast(constant, new_type, is_implicit_cast);
  } else {
    /* Determine the class type we're casting to. */
    if (cast_to_base) {
      new_class = bcp->type;
    } else {
      new_class = bcp->derived_class;
    }  /* if */
    /* Determine the offset for any casting already done to the constant. */
    casting_base_class = constant->variant.ptr_to_member.casting_base_class;
    if (casting_base_class == NULL) {
      offset = 0;
    } else {
      offset = casting_base_class->offset;
      if (constant->variant.ptr_to_member.cast_to_base) offset = -offset;
    }  /* if */
    /* Add the offset for the new cast. */
    if (cast_to_base) {
      offset -= bcp->offset;
    } else {
      offset += bcp->offset;
    }  /* if */
    /* Find the original class of the member. */
    member_class = pm_constant_member_class(constant);
    /* Find the base class to use as casting_base_class. */
    if (same_entities(new_class, member_class)) {
      /* The casts take us back to the original member class, so no
         casting is needed. */
      casting_base_class = NULL;
      cast_to_base = FALSE;
      constant->implicit_cast = FALSE;
      constant->type = new_type;
    } else {
      /* Look for a base class of the new type which is the member class.
         If one is found, the cast is to a derived class.  Use the offset
         to distinguish different instances of the same class. */
      for (casting_base_class = base_classes_of(new_class);
           casting_base_class != NULL;
           casting_base_class = casting_base_class->next) {
        if (same_entities(casting_base_class->type, member_class) &&
            casting_base_class->offset == (a_targ_size_t)offset) {
          cast_to_base = FALSE;
          goto have_base_class;
        }  /* if */
      }  /* for */
      /* Look for a base class of the member class which is the new class.
         if one if found, the cast is to a base class.  This is an unusual
         case. */
      for (casting_base_class = base_classes_of(member_class);
           casting_base_class != NULL;
           casting_base_class = casting_base_class->next) {
        if (same_entities(casting_base_class->type, new_class) &&
            casting_base_class->offset == (a_targ_size_t)(-offset)) {
          cast_to_base = TRUE;
          goto have_base_class;
        }  /* if */
      }  /* for */
      /* Weird case (undefined behavior), e.g., member cast from base class
         A to derived class D, then to base class B (which does not contain
         the member). */
      *did_not_fold = TRUE;
      goto end_of_routine;
have_base_class:
      implicit_or_explicit_cast(constant, new_type, is_implicit_cast);
    }  /* if */
    constant->variant.ptr_to_member.casting_base_class = casting_base_class;
    constant->variant.ptr_to_member.cast_to_base = cast_to_base;
  }  /* if */
end_of_routine:;
}  /* set_pm_cast_base_class */


static void fold_pm_base_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_boolean         *did_not_fold,
                                    a_source_position *err_pos,
                                    an_error_code     *error_detected)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to a member
of a base class.  constant_1 is a pointer-to-member constant.  It is converted
to a pointer-to-member for the base class indicated by bcp and the new
constant is returned in *result.  result->type on entry indicates the
desired pointer-to-member type, possibly with qualifiers.  If there is an
error, issue it at *err_pos.  If the cast cannot be folded, *did_not_fold
is returned TRUE.  If error_detected is non-NULL, set *error_detected
to the code for any error detected, and do not issue the diagnostic,
or set it to ec_no_error if there was no error.  Note that casts of
this type always come from explicit casts, so checking for
accessibility of base classes is not necessary.
*/
{
  a_type_ptr new_type = result->type;

  /* The code here looks like add_pm_base_class_casts. */
  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  if (bcp->ambiguous) {
    /* The base class is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_base_class;
    } else {
      pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp) && !any_cfront_mode()) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    if (error_detected != NULL) {
      *error_detected = ec_pm_virtual_base_from_derived_class;
    } else {
      pos_ty2_error(ec_pm_virtual_base_from_derived_class, err_pos,
                    pm_class_type(constant_1->type), bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/TRUE,
                           /*is_implicit_cast=*/FALSE, did_not_fold);
  }  /* if */
}  /* fold_pm_base_class_cast */


static void fold_pm_derived_class_cast(a_constant        *constant_1,
                                       a_base_class      *bcp,
                                       a_constant        *result,
                                       a_boolean         is_implicit_cast,
                                       a_boolean         check_cast_access,
                                       a_boolean         *did_not_fold,
                                       a_source_position *err_pos,
                                       an_error_code     *error_detected)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to member
of a derived class.  constant_1 is a pointer-to-member constant.  It is
converted to a pointer-to-member for the derived class (given by
result->type) and the new constant is returned in *result.  bcp points
to the base class entry for the current type relative to the desired
derived type.  The cast is implicit if is_implicit_cast is TRUE.
Access should be checked if check_cast_access is TRUE.  If there is an
error, it is issued at *err_pos.  If the cast cannot be folded,
*did_not_fold is returned TRUE.  If error_detected is non-NULL, set
*error_detected to the code for any error detected, and do not issue
the diagnostic, or set it to ec_no_error if there was no error.
*/
{
  a_type_ptr            new_type = result->type, curr_type;
  a_type_ptr            derived_class_type;
  a_derivation_step_ptr dsp;
  a_base_class_ptr      base_class;

  /* The code here looks like add_pm_derived_class_casts. */
  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  derived_class_type = pm_class_type(new_type);
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    if (error_detected != NULL) {
      *error_detected = ec_ambiguous_derived_class;
    } else {
      pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                    bcp->type);
    }  /* if */
    set_error_constant(result);
  } else if (!(microsoft_mode &&
          PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE) && /*lint !e506*/
             any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class. */
    if (error_detected != NULL) {
      *error_detected = ec_pm_derived_class_from_virtual_base;
    } else {
      pos_ty2_error(ec_pm_derived_class_from_virtual_base, err_pos,
                    derived_class_type, bcp->type);
    }  /* if */
    set_error_constant(result);
  } else {
    /* No access checking in prototype instantiations. */
    if (in_front_end &&
        scope_stack[depth_scope_stack].in_prototype_instantiation) {
      check_cast_access = FALSE;
    }  /* if */
    if (check_cast_access) {
      /* Check the accessibility of the base class.  (Recall that casts
         to derived types can be done implicitly.) */
      curr_type = derived_class_type;
      for (dsp = cast_derivation_path_of(bcp); dsp != NULL; dsp = dsp->next) {
        /* Check that the base class is accessible from the current class. */
        base_class = dsp->base_class;
        if (!is_accessible_imm_base_class(base_class, curr_type)) {
          /* The base class is inaccessible. */
          if (error_detected != NULL) {
            if (is_effective_error(ec_conv_from_inaccessible_base_class,
                                   es_discretionary_error)) {
              *error_detected = ec_conv_from_inaccessible_base_class;
            }  /* if */
          } else {
            pos_ty_diagnostic(es_discretionary_error,
                              ec_conv_from_inaccessible_base_class,
                              err_pos, base_class->type);
          }  /* if */
          break;
        }  /* if */
        curr_type = base_class->type;
      }  /* for */
    }  /* if */
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/FALSE,
                           is_implicit_cast, did_not_fold);
  }  /* if */
}  /* fold_pm_derived_class_cast */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean related_classes_single_inh(a_type_ptr class_1,
                                            a_type_ptr class_2)
/*
Return TRUE if the class types given are related by inheritance or are
the same, and if there's inheritance the Microsoft inheritance kind is
single inheritance.
*/
{
  a_boolean result;

  result = (identical_types(class_1, class_2) ||
            (find_base_class_of(class_1, class_2) != NULL &&
             class_2->variant.class_struct_union.extra_info->inheritance_kind
                                         == (an_inheritance_kind)ihk_single) ||
            (find_base_class_of(class_2, class_1) != NULL &&
             class_1->variant.class_struct_union.extra_info->inheritance_kind
                                         == (an_inheritance_kind)ihk_single));
  return result;
}  /* related_classes_single_inh */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void conv_ptr_to_member_to_ptr_to_member(
                                      a_constant        *old_constant,
                                      a_constant        *new_constant,
                                      a_boolean         is_implicit_cast,
                                      a_boolean         check_cast_access,
                                      a_boolean         is_reinterpret_cast,
                                      a_boolean         *did_not_fold,
                                      a_source_position *err_pos,
                                      an_error_code     *err_code,
                                      an_error_severity *err_severity,
                                      a_boolean         suppress_complex_diags)
/*
Convert a pointer-to-member constant to a pointer-to-member constant of
a different type.  old_constant is the original constant.  new_constant->type
indicates the desired new type.  The converted constant is put into
*new_constant.  This is an implicit cast if is_implicit_cast is TRUE.
Check access if check_cast_access is TRUE.  This is a reinterpret_cast
if is_reinterpret_cast is TRUE.  If the cast cannot be folded,
*did_not_fold is returned TRUE.  Return err_code and *err_severity set
*to indicate any error/warning detected, or *err_code == ec_no_error
*if everything went fine.  If suppress_complex_diags is TRUE, suppress
(and return in err_code/err_severity) also those complex diagnostics
(e.g., those for access errors) that can't be issued simply from the
error code.
*/
{
  a_type_ptr       new_type = new_constant->type, new_class;
  a_type_ptr       old_type = old_constant->type, old_class;
  a_base_class_ptr bcp;
  an_error_code    *p_err_code = NULL;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *did_not_fold = FALSE;
  if (suppress_complex_diags) p_err_code = err_code;
  old_class = pm_class_type(old_type);
  new_class = pm_class_type(new_type);
  if (is_reinterpret_cast) {
    /* A reinterpret_cast. */
    if (!old_constant->is_reinterpret_cast &&
        old_constant->variant.ptr_to_member.casting_base_class != NULL) {
      /* The constant entry can't represent a static_cast followed by
         a reinterpret_cast. */
      *did_not_fold = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode &&
               !related_classes_single_inh(old_class, new_class)) {
      /* MSVC++ only allows reinterpret_casts like this when the offset
         is zero and inheritance is single.  Otherwise they get an error.
         We don't give an error but we don't fold them at compile time. */
      *did_not_fold = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Our interpretation is that a reinterpret_cast leaves the bits
         of the pointer-to-member alone.  That's what Cfront 3.0 does.
         It's not what g++ 3.n, for example, does. */
      copy_constant(old_constant, new_constant);
      implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
      new_constant->is_reinterpret_cast = TRUE;
    }  /* if */
  } else if (old_constant->is_reinterpret_cast && !is_reinterpret_cast) {
    /* The constant entry can't represent a reinterpret_cast followed
       by a static_cast. */
    *did_not_fold = TRUE;
  /* Using types_are_compatible so that A<x> and A<error> are considered
     the same type. */
  } else if (types_are_compatible(old_class, new_class)) {
    /* The classes are the same, so no error check is needed. */
    /* The fact that the class types are the same does not mean the
       pointer-to-member types are the same; the member type may be
       changing. */
    copy_constant(old_constant, new_constant);
    implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  } else if ((bcp = find_base_class_of(old_class, new_class)) != NULL) {
    /* Derived --> base (allowed only as an explicit cast).  Valid unless
       the cast is ambiguous. */
    fold_pm_base_class_cast(old_constant, bcp, new_constant, did_not_fold,
                            err_pos, p_err_code);
    if (p_err_code != NULL && *err_code != ec_no_error) {
      *err_severity = es_error;
    }  /* if */
  } else if ((bcp = find_base_class_of(new_class, old_class)) != NULL) {
    /* Base --> derived (allowed as an implicit or explicit cast).  Valid
       unless the cast is ambiguous, the base class is inaccessible (if
       the cast is implicit), or the base class is a virtual base of the
       derived class. */
    fold_pm_derived_class_cast(old_constant, bcp, new_constant,
                               is_implicit_cast, check_cast_access,
                               did_not_fold, err_pos, p_err_code);
    if (p_err_code != NULL && *err_code != ec_no_error) {
      *err_severity = es_error;
    }  /* if */
  } else {
    unexpected_condition_str(
                    "conv_ptr_to_member_to_ptr_to_member: unrelated classes");
  }  /* if */
}  /* conv_ptr_to_member_to_ptr_to_member */


static void conv_integer_to_pointer(a_constant        *old_constant,
				    a_constant        *new_constant,
			  	    a_boolean         is_implicit_cast,
				    an_error_code     *err_code,
				    an_error_severity *err_severity)
/*
Convert an integer constant to a pointer constant of type as specified by
"new_constant".
*/
{
  a_type_ptr       new_type = new_constant->type;
  an_integer_value mask;
  a_boolean        is_label_diff = FALSE;

#if GNU_EXTENSIONS_ALLOWED
  if (old_constant->kind == (a_constant_repr_kind)ck_label_difference) {
    is_label_diff = TRUE;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (is_implicit_cast) {
    if (is_label_diff ||
        cmplit_integer_constant(old_constant, (a_host_large_integer)0) != 0) {
      /* Any value other than zero (NULL).  Issue a warning. */
      *err_code = ec_non_zero_int_conv_to_pointer;
      *err_severity = es_warning;
    }  /* if */
  }  /* if */
  /* Make a new constant that is the old constant implicitly cast to the
     pointer type. */
  copy_constant(old_constant, new_constant);
  implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
  /* Mask the integer down to the size of pointer. */
  if (new_constant->kind == (a_constant_repr_kind)ck_integer) {
    make_integer_value_mask(&mask,
                          (int)(skip_typerefs(new_type)->size*targ_char_bit));
    and_integer_values(&new_constant->variant.integer_value, &mask);
  } else if (!is_label_diff) {
    unexpected_condition_str("conv_integer_to_pointer: not integer constant");
  }  /* if */
}  /* conv_integer_to_pointer */


#if !CHECKING
/*ARGSUSED*/ /* <-- old_constant is not used if CHECKING is FALSE. */
#endif /* !CHECKING */
static void conv_integer_to_ptr_to_member(a_constant *old_constant,
                                          a_constant *new_constant,
                                          a_boolean  is_implicit_cast)
/*
Convert an integer constant to a pointer to member.  is_implicit_cast
is TRUE if the cast is implicit.
*/
{
  a_type_ptr new_type = new_constant->type;
  a_boolean  is_function_ptr;

#if CHECKING
  /* The only valid constants are zero or nullptr. */
  if ((old_constant->kind != (a_constant_repr_kind)ck_integer ||
       old_constant->implicit_cast ||
       !is_zero_constant(old_constant)) &&
      !is_nullptr_type(old_constant->type)) {
    internal_error("conv_integer_to_ptr_to_member: bad source constant");
  }  /* if */
#endif /* CHECKING */
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_ptr_to_member);
  new_constant->variant.ptr_to_member.is_function_ptr = is_function_ptr =
                                    is_function_type(pm_member_type(new_type));
  /* NULL pointer implies a NULL pointer-to-member constant. */
  if (is_function_ptr) {
    new_constant->variant.ptr_to_member.variant.routine = NULL;
  } else {
    new_constant->variant.ptr_to_member.variant.field = NULL;
  }  /* if */
  implicit_or_explicit_cast(new_constant, new_type, is_implicit_cast);
}  /* conv_integer_to_ptr_to_member */


static void issue_folding_diagnostic(an_error_code     err_code,
                                     an_error_severity err_severity,
                                     a_boolean         constant_context,
                                     a_boolean         evaluated_context,
                                     a_boolean         *did_not_fold,
                                     an_error_code     *error_detected,
                                     a_source_position *err_pos,
                                     a_constant        *result)
/*
An error or warning has been detected in a folding operation; err_code
and err_severity indicate what it is.  If not in a constant_context, reduce
an error to a warning and set *did_not_fold to TRUE.  If not in an
evaluated_context, throw away the error and set *did_not_fold to TRUE.
If error_detected is non-NULL, the caller would like to know that an
error was detected but does not want it issued at this level.  Return
*error_detected set to the code for the error detected, or ec_no_error
if no error was detected, and suppress any diagnostic.  If a
diagnostic is issued, do so with source position *err_pos.  Set
*result to the proper result (often, an error constant).
*/
{
  if (error_detected != NULL) *error_detected = ec_no_error;
  if (!evaluated_context) {
    /* Discard a warning or error in a not-evaluated context. */
    err_severity = es_none;
    *did_not_fold = TRUE;
  } else if (!constant_context) {
    /* Nonconstant context, so an error will not be issued.  It will be
       downgraded to a warning. */
    if (err_severity == es_error) {
      /* Reduce an error to a warning. */
      err_severity = es_warning;
      *did_not_fold = TRUE;
    }  /* if */
  } else {
    /* Constant context, so errors can be issued.  Check for a requested
       increase of severity on a warning. */
    if (err_severity != es_error &&
        is_effective_error(err_code, err_severity)) {
      err_severity = es_error;
    }  /* if */
  }  /* if */
  if (err_severity == es_error) {
    /* We have an error. */
    if (error_detected != NULL) {
      /* The caller wants an error indication rather than a diagnostic. */
      *error_detected = err_code;
    } else {
      pos_error(err_code, err_pos);
    }  /* if */
    set_error_constant(result);
    *did_not_fold = FALSE;
  } else if (error_detected != NULL) {
    /* At most we have a warning, and we're suppressing diagnostics, so
       skip the rest of the checks. */
  } else if (err_severity == es_warning) {
    pos_warning(err_code, err_pos);
  }  /* if */
}  /* issue_folding_diagnostic */


void type_change_constant_full(a_constant        *constant,
                               a_type_ptr        new_type,
                               a_boolean         is_implicit_cast,
                               a_boolean         constant_context,
                               a_boolean         evaluated_context,
                               a_boolean         fold_constant_addr_exprs,
                               a_boolean         check_cast_access,
                               a_boolean         check_ambiguity,
                               a_boolean         is_reinterpret_cast,
                               a_boolean         maintain_expression,
                               a_boolean         *did_not_fold,
                               an_error_code     *error_detected,
                               a_source_position *err_pos)
/*
Convert the indicated constant to "new_type".  If is_implicit_cast is
TRUE, this is an implicit cast; more warnings are given.  If
constant_context is FALSE, this operation is being evaluated as part
of a nonconstant expression, so any error is reduced to a warning and
*did_not_fold is returned TRUE.  If evaluated_context is FALSE, this
operation is being done in a not-evaluated context (e.g., a sizeof or
a dead branch of a "?" operator), so any error is thrown away and
*did_not_fold is returned TRUE.  *did_not_fold is also returned TRUE
in other cases where the folding cannot be done.
fold_constant_addr_exprs is TRUE if constant address expressions
should be folded (e.g., base class casts); if it is FALSE,
*did_not_fold is set instead for those.  check_cast_access is TRUE if
access checking should be done on related-class casts.
check_ambiguity is TRUE if ambiguity checking should be done on
related-class casts.  If is_reinterpret_cast is TRUE, this cast is a
reinterpret_cast; related-class casts are treated like casts between
unrelated classes.  If maintain_expression is TRUE, any backing expression
attached to the constant is maintained, by adding a cast if necessary.
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.  *err_pos is used as the position
for any diagnostics issued.
*/
{
  a_type_ptr        constant_type, new_type_with_typedefs;
  a_constant        new_constant;
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;
  a_boolean         template_case;
  a_boolean         suppress_diags = (error_detected != NULL);

  db_enter(5, "type_change_constant_full");
  *did_not_fold = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  err_code = ec_no_error;
  err_severity = es_warning;
  clear_constant(&new_constant, (a_constant_repr_kind)ck_error);
  /* Preserve the null_pointer_constant_ruled_out flag. */
  new_constant.null_pointer_constant_ruled_out =
                                     constant->null_pointer_constant_ruled_out;

  /* Put the new type in the destination constant (preserving typedefs
     if any; that's important). */
  new_constant.type = new_type_with_typedefs = new_type;
  /* Remove any type qualifiers or typedefs from the types involved. */
  constant_type = skip_typerefs(constant->type);
  new_type = skip_typerefs(new_type);

  if (is_error_constant(constant) || is_error_type(new_type)) {
    /* Changing to an error type, or the old constant is an error constant,
       so produce an error constant as result. */
    set_error_constant(&new_constant);
    goto exit;
  }  /* if */
  /* Not using context_may_have_dependent_types here because we can get
     "auto" from type deductions in initializations. */
  template_case = (!C_mode() &&
                   (constant->kind == (a_constant_repr_kind)ck_template_param||
                    (in_front_end &&
                     is_template_dependent_type(new_type))));
  if (identical_types(constant_type, new_type) &&
      (is_implicit_cast || !template_case)) {
    /* The current and new types are the same, so no change is required. */
    copy_constant(constant, &new_constant);
    /* Put in the actual type wanted, as it may have typedefs. */
    new_constant.type = new_type_with_typedefs;
    goto exit;
  }  /* if */
  if (template_case) {
    /* Casting a template parameter constant, or casting to a template
       parameter type.  Use a special tpck_cast constant. */
    make_template_param_cast_constant(constant, &new_constant, new_type,
                                      !is_implicit_cast);
    goto exit;
  }  /* if */
  if (is_bool_type(new_type)) {
    /* Conversion of any type to bool.  Set the boolean value to FALSE (zero)
       if the source constant is some form of "false".  Otherwise, set it
       to TRUE (1). */
    if (!constant_bool_value_known_at_compile_time(constant)) {
      /* The constant's value is not known until link time, so the conversion
         cannot be folded at this time. */
      *did_not_fold = TRUE;
      goto exit;
    }  /* if */
    set_constant_kind(&new_constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&new_constant.variant.integer_value,
                      (a_host_large_integer)!is_false_constant(constant));
    goto exit;
  }  /* if */
  if (is_nullptr_type(new_type)) {
    /* Conversion to a nullptr type.  There is only one "value" of a
       nullptr type, so the result is an integer with value 0, just like
       old-style null pointer constants. */
    set_constant_kind(&new_constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&new_constant.variant.integer_value,
                      (a_host_large_integer)0);
    new_constant.implicit_cast = TRUE;
    goto exit;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && is_handle_type(new_type) &&
      boxing_conversion_possible(constant_type, new_type,
                                  (a_std_conv_descr *)NULL)) {
    /* A C++/CLI boxing conversion cannot be folded to a constant. */
    *did_not_fold = TRUE;
    goto exit;
  }  /* if */
#endif /*MICROSOFT_EXTENSIONS_ALLOWED */
  if (vla_enabled && !is_implicit_cast &&
      is_directly_variably_modified_type(new_type)) {
    /* A cast to a variably-modified type where the variable bound appears
       in the cast (an opposed to inside a typedef declared elsewhere) is
       a non-constant operation and cannot be folded. */
    *did_not_fold = TRUE;
    goto exit;
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  if (is_vector_type(new_type)) {
    /* We don't attempt to fold casts to vector types. */
    *did_not_fold = TRUE;
    goto exit;
  }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode &&
      (constant->kind == (a_constant_repr_kind)ck_upc_threads ||
       constant->kind == (a_constant_repr_kind)ck_upc_mythread)) {
    /* THREADS and MYTHREAD are not compile-time constants and should
       therefore not be folded. */
    *did_not_fold = TRUE;
    goto exit;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* Any case where the constant is represented as an address should be
       converted by setting the implicit_cast flag.  This test has to be
       early -- like this -- to catch ((unsigned)((int)&x)).  That case
       would have constant_type->kind == tk_integer and new_type->kind
       == tk_integer, and so would not look like it involves pointers. */
    conv_pointer_to_whatever(constant, &new_constant, check_cast_access,
                             check_ambiguity, is_implicit_cast,
                             fold_constant_addr_exprs, is_reinterpret_cast,
                             did_not_fold, err_pos, &err_code, &err_severity,
                             suppress_diags);
    goto exit;
  }  /* if */

  /* Determine the type we are converting from. */
  switch (constant_type->kind) {

    case tk_integer:
      /* Converting from integer. */
      switch(new_type->kind) {
        case tk_integer:
          /* Converting integer to integer. */
          conv_integer_to_integer(constant, &new_constant, is_implicit_cast,
                                  &err_code, &err_severity);
          break;
        case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
        case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          /* Converting integer to float. */
          conv_integer_to_float(constant, &new_constant,
                                &err_code, &err_severity);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Converting integer to fixed-point. */
          conv_integer_to_fixed_point(constant, &new_constant,
                                      &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        case tk_pointer:
          /* Converting integer to pointer. */
          conv_integer_to_pointer(constant, &new_constant, is_implicit_cast,
                                  &err_code, &err_severity);
          break;
        case tk_ptr_to_member:
          /* Converting integer to pointer-to-member. */
          conv_integer_to_ptr_to_member(constant, &new_constant,
                                        is_implicit_cast);
          break;
        default:
          unexpected_condition_str(
                             "type_change_constant_full: integer to bad type");
      }  /* switch */
      break;

    case tk_float:
      /* Converting from float. */
      switch (new_type->kind) {
        case tk_integer:
          /* Converting float to integer. */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting float to float. */
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
          /* Converting float to imaginary (produces zero). */
        case tk_complex:
          /* Converting float to complex. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          conv_float_to_float(constant, &new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Converting float to fixed-point. */
          conv_float_to_fixed_point(constant, &new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                               "type_change_constant_full: float to bad type");
      }  /* switch */
      break;

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting imaginary to integer (produces zero). */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting imaginary to float (produces zero). */
        case tk_imaginary:
          /* Converting imaginary to imaginary. */
        case tk_complex:
          /* Converting imaginary to complex. */
          conv_float_to_float(constant, &new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Imaginary to fixed-point. */
          conv_float_to_fixed_point(constant, &new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                           "type_change_constant_full: imaginary to bad type");
      }  /* switch */
      break;

    case tk_complex:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting complex to integer. */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_fp_mode, constant_context);
          break;
        case tk_float:
          /* Converting complex to float. */
        case tk_imaginary:
          /* Converting complex to imaginary. */
        case tk_complex:
          /* Converting complex to complex. */
          conv_float_to_float(constant, &new_constant,
                              &err_code, &err_severity,
                              &depends_on_fp_mode);
          break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          /* Complex to fixed-point. */
          conv_float_to_fixed_point(constant, &new_constant,
                                    &err_code, &err_severity);
          break;
#endif /* FIXED_POINT_ALLOWED */
        default:
          unexpected_condition_str(
                             "type_change_constant_full: complex to bad type");
      }  /* switch */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting fixed-point to integer. */
          conv_fixed_point_to_integer(constant, &new_constant,
                                      &err_code, &err_severity);
          break;
        case tk_float:
          /* Converting fixed-point to floating-point. */
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_imaginary:
          /* Fixed-point to imaginary. */
        case tk_complex:
          /* Fixed-point to complex. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          conv_fixed_point_to_float(constant, &new_constant,
                                    &err_code, &err_severity);
          break;
        case tk_fixed_point:
          /* Converting fixed-point to fixed-point. */
          conv_fixed_point_to_fixed_point(constant, &new_constant,
                                          &err_code, &err_severity);
          break;
        default:
          unexpected_condition_str(
                         "type_change_constant_full: fixed-point to bad type");
      }  /* switch */
      break;
#endif /* FIXED_POINT_ALLOWED */

    case tk_pointer:
      /* Converting from pointer. */
      conv_pointer_to_whatever(constant, &new_constant, check_cast_access,
                               check_ambiguity, is_implicit_cast,
                               fold_constant_addr_exprs, is_reinterpret_cast,
                               did_not_fold, err_pos,
                               &err_code, &err_severity,
                               suppress_diags);
      break;

    case tk_ptr_to_member:
      /* Converting from pointer-to-member to pointer-to-member. */
      conv_ptr_to_member_to_ptr_to_member(constant, &new_constant,
                                          is_implicit_cast,
                                          check_cast_access,
                                          is_reinterpret_cast,
                                          did_not_fold,
                                          err_pos, &err_code, &err_severity,
                                          suppress_diags);
      break;

    case tk_error:
      /* The old constant is an error constant. */
      /* Change the type of the new constant back to the original type of the
	 error constant, i.e., error. */
      new_constant.type = constant->type;
      break;

    case tk_nullptr:
      /* The old constant is a null pointer constant (the C++ "nullptr"
         keyword or another value with a nullptr type).  This is treated
         effectively like converting an integer 0, i.e., an old-style
         null pointer constant. */
      check_assertion(constant->kind == (a_constant_repr_kind)ck_integer);
      if (new_type->kind == (a_constant_repr_kind)tk_pointer) {
        conv_integer_to_pointer(constant, &new_constant, is_implicit_cast,
                                &err_code, &err_severity);
      } else if (new_type->kind == (a_constant_repr_kind)tk_ptr_to_member) {
        conv_integer_to_ptr_to_member(constant, &new_constant,
                                      is_implicit_cast);
      } else if (new_type->kind == (a_constant_repr_kind)tk_integer &&
                 is_reinterpret_cast) {
        conv_integer_to_integer(constant, &new_constant, is_implicit_cast,
                                &err_code, &err_severity);
      } else {
        unexpected_condition_str(
                      "type_change_constant_full: nullptr to bad type");
      }  /* if */
      break;

    default:
      unexpected_condition_str("type_change_constant_full: from bad type");
  }  /* switch */

exit:
  /* Looks for casts that rule out use of a constant as part of a null
     pointer constant.  In a null pointer constant, only casts from
     arithmetic to integral types, or, in C, from integral to "void *",
     are allowed.  This processing is to rule out things like
     (int)(float)0, which are not valid null pointer constants.
     Also really obscure things like (int)(float)2 - 2.  This
     processing is more or less tracking whether a constant could
     be an integral constant expression, even when it is scanned
     in other modes. */
  if (microsoft_bugs && !C_mode() && !is_implicit_cast &&
      microsoft_version <= 1300) {
    /* Microsoft C++ mode: any explicit cast makes a constant not a null
       pointer constant.  In particular, (int)0 is not a null pointer
       constant.  This was fixed in MSVC++ 7.1. */
    new_constant.null_pointer_constant_ruled_out = TRUE;
  } else if (is_integral_or_enum_type(new_type) &&
             is_arithmetic_or_enum_type(constant_type)) {
    /* Arithmetic --> integral.  Okay. */
  } else if (C_mode() &&
             is_void_star_type(new_type) &&
             is_integral_or_enum_type(constant_type)) {
    /* Integral --> void* in C mode, okay. */
  } else {
   /* Anything else: this constant cannot be part of a null pointer
      constant. */
    new_constant.null_pointer_constant_ruled_out = TRUE;
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "type_change_constant of ");
    db_constant(constant);
    fprintf(f_debug, ", result = ");
    db_constant(&new_constant);
    if (err_code != ec_no_error) {
      fprintf(f_debug, " with ");
      if (err_severity == es_error) {
        fprintf(f_debug, "error");
      } else if (err_severity == es_warning) {
        fprintf(f_debug, "warning");
      } else {
        fprintf(f_debug, "diagnostic");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  if (err_code != ec_no_error) {
    /* There was an error or warning. */
    issue_folding_diagnostic(err_code, err_severity, constant_context,
                             evaluated_context, did_not_fold,
                             error_detected, err_pos, &new_constant);
    if (err_severity == es_error) depends_on_fp_mode = FALSE;
  }  /* if */
  if (depends_on_fp_mode && !constant_context) {
    /* In a non-constant context, leave an operation to be done at runtime
       if its result depends on the floating-point mode. */
    *did_not_fold = TRUE;
  }  /* if */
  if (maintain_expression && constant->expr != NULL &&
      (int)err_severity < (int)es_error && !*did_not_fold) {
    /* Transfer the source expression from the old constant to the new one,
       adding a cast if there was a type change.  Note that the cast added
       is always an eok_cast, so this shouldn't be used if there's the
       possibility that a base-class cast or the like is involved. */
    if (is_implicit_cast &&
        identical_types(constant->type, new_constant.type)) {
      new_constant.expr = constant->expr;
    } else {
      an_expr_node_ptr cast_expr =
                            make_operator_node((an_expr_operator_kind)eok_cast,
                                               new_constant.type,
                                               constant->expr);
      cast_expr->variant.operation.compiler_generated = is_implicit_cast;
      cast_expr->variant.operation.is_reinterpret_cast = is_reinterpret_cast;
      new_constant.expr = cast_expr;
    }  /* if */
  } else {
    new_constant.expr = NULL;
  }  /* if */
  /* Return the new constant value. */
  copy_constant(&new_constant, constant);
  db_exit();
}  /* type_change_constant_full */


void type_change_constant(a_constant        *constant,
                          a_type_ptr        new_type,
                          a_boolean         is_implicit_cast,
                          a_boolean         maintain_expression,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos)
/*
Simple interface to type_change_constant_full.  See that routine for the
description of the parameters.
*/
{
  type_change_constant_full(constant, new_type, is_implicit_cast,
                            /*constant_context=*/TRUE,
                            /*evaluated_context=*/TRUE,
                            /*fold_constant_addr_exprs=*/TRUE,
                            /*check_cast_access=*/is_implicit_cast,
                            /*check_ambiguity=*/TRUE,
                            /*is_reinterpret_cast=*/FALSE,
                            maintain_expression, did_not_fold,
                            /*error_detected=*/(an_error_code *)NULL,
                            err_pos);
}  /* type_change_constant */


a_boolean is_false_constant(a_constant *constant)
/*
Return TRUE if the constant is an integer, floating, pointer, or
pointer to member zero.  This is supposed to duplicate the test on
the boolean controlling expressions in statements and the ?:, &&, and ||
operators.  Can also be used to test for a NULL pointer or pointer to member.
*/
{
  a_boolean is_false = FALSE;

  /* The value of a link-time constant is not known until link time, so
     one cannot decide whether it is true or false.  Such constants should
     not get here. */
  check_assertion_str(constant_bool_value_known_at_compile_time(constant),
                      "is_false_constant: link-time constant");
  /* ck_address constants that aren't link-time constants are assumed to
     be non-NULL.  For example, the address of an auto variable. */
  if (is_zero_constant(constant)) {
    /* Zero integral, fixed-point, or floating constant. */
    is_false = TRUE;
  } else if (constant->kind == (a_constant_repr_kind)ck_integer &&
             constant->implicit_cast) {
    /* Check for NULL pointer constant (the nullptr keyword or 0 cast to a
       pointer type). */
    is_false = (cmplit_integer_constant(constant,
                                        (a_host_large_integer)0) == 0);
  } else if (constant->kind == (a_constant_repr_kind)ck_ptr_to_member) {
    /* Pointer to member constant.  See if null. */
    is_false = pm_constant_is_null(constant);
  }  /* if */
  return is_false;
}  /* is_false_constant */


a_boolean is_null_pointer_constant(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer constant.
*/
{
  a_boolean is_null_pointer = FALSE;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    /* A null pointer constant either has a nullptr type or it has the
       value zero, perhaps cast to "void *" in C.  Only certain kinds of
       casts are allowed. */
    if (is_nullptr_type(constant->type)) {
      is_null_pointer = TRUE;
    } else if ((!constant->null_pointer_constant_ruled_out ||
                (gnu_mode && gnu_version < 40500 &&
                  /* g++/gcc allow (int)(int *)0 as a null pointer
                     constant.  Fixed in 4.2, but some variants of that
                     linger until eliminated in 4.5.  */
                 (is_integral_type(constant->type) ||
                  /* gcc allows (void*)(int *)0 as a null pointer constant.
                     Fixed in 4.5. */
                  (gcc_mode && is_void_star_type(constant->type))))) &&
               cmplit_integer_constant(constant,
                                       (a_host_large_integer)0) == 0) {
      if (!enum_type_is_integral && is_enum_type(constant->type)) {
        /* In C++ (except for cfront compatibility) an enumerator with value
           zero is not a null pointer constant. */
      } else {
        is_null_pointer = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */

  return is_null_pointer;
}  /* is_null_pointer_constant */


a_boolean is_or_might_be_null_pointer_constant(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer constant or is
a template parameter constant that might be a null pointer constant.
*/
{
  a_boolean might_be_null_pointer = FALSE;

  if (constant->kind != (a_constant_repr_kind)ck_template_param) {
    might_be_null_pointer = is_null_pointer_constant(constant);
  } else {
    /* Template parameter constant.  This might be a null pointer constant
       if its type is integral or a template parameter type (so not,
       for example, if it's a pointer to a template parameter type). */
    a_type_ptr type = skip_typerefs(constant->type);
    if (type->kind == (a_type_kind)tk_integer ||
        type->kind == (a_type_kind)tk_template_param) {
      a_constant_ptr eff_constant = constant;
      might_be_null_pointer = TRUE;
      /* Drop casts to get to the underlying constant. */
      while (eff_constant->kind == (a_constant_repr_kind)ck_template_param &&
             eff_constant->variant.template_param.kind ==
                                   (a_template_param_constant_kind)tpck_cast) {
        eff_constant  = eff_constant->variant.template_param.variant.constant;
      }  /* while */
      if (eff_constant->kind == (a_constant_repr_kind)ck_template_param &&
          eff_constant->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_sizeof) {
        /* A sizeof constant never has a value of zero, and therefore is
           never a null pointer constant. */
        might_be_null_pointer = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return might_be_null_pointer;
}  /* is_or_might_be_null_pointer_constant */


#if DEBUG
static void db_unary_operation(char          *operation,
			       a_constant    *operand,
                               a_constant    *result,
			       an_error_code err_code)
/*
Do a debug print giving the result of folding a one-operand constant operation.
*/
{
  if (db_flag_is_set("folding") || debug_level >= 5) {
    fprintf(f_debug, "%s ", operation);
    db_constant(operand);
    fprintf(f_debug, ", result = ");
    db_constant(result);
    if (err_code != ec_no_error) fprintf(f_debug, " with error");
    fprintf(f_debug, "\n");
  }  /* if */
}  /* db_unary_operation */
#endif /* DEBUG */


static void do_inegate(a_constant        *constant,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Do the negate operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        err, is_signed;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Compute 0 - constant. */
  set_integer_value(&result_value, (a_host_large_integer)0);
  is_signed = int_constant_is_signed(constant);
  subtract_integer_values(&result_value, &constant->variant.integer_value,
                          is_signed, &err);
  if (is_signed) {
    /* Negation of a signed integer. */
    if (err) {
      /* Folding error. */
      /* Suppress this error for non-arithmetic constants in K&R mode. */
      if (C_dialect != C_dialect_pcc || !constant->non_arithmetic) {
        *err_code = ec_integer_overflow;
        *err_severity = ES_INT_OVERFLOW;
      }  /* if */
    }  /* if */
  } else {
    /* Negation of an unsigned integer. */
    /* Negation of an unsigned quantity, producing as it does a value that
       can't be negative, is considered a non-arithmetic operation.  This
       is also convenient when -(INT_MAX+1) is used as a constant on
       twos complement machines; it avoids a warning. */
    result->non_arithmetic = TRUE;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_unary_operation("i-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_inegate */


static void do_fnegate(a_constant        *constant,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the negate operation on all types of float and imaginary values.
*/
{
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_boolean    err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Use original constant kind in case it is ck_imaginary. */
  set_constant_kind(result, constant->kind);

  fp_negate(float_kind, &constant->variant.float_value,
            &result->variant.float_value, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("f-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fnegate */

#if FIXED_POINT_ALLOWED

static void do_fxnegate(a_constant        *constant,
                        a_constant        *result,
                        an_error_code     *err_code,
                        an_error_severity *err_severity)
/*
Do the negate operation on all types of fixed-point values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant->kind == (a_constant_repr_kind)ck_fixed_point);
  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_negate(&constant->variant.fixed_point_value,
             fxp_descr_for_constant(constant),
             &result->variant.fixed_point_value,
             fxp_descr_for_constant(result),
             &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_unary_operation("fx-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fxnegate */

#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xnegate(a_constant        *constant,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the negate operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_negate(float_kind,
            &constant->variant.complex_value->real,
            &result->variant.complex_value->real,
            &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_negate(float_kind,
            &constant->variant.complex_value->imag,
            &result->variant.complex_value->imag,
            &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */
#if DEBUG
  db_unary_operation("x-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_xnegate */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static void do_complement(a_constant        *constant,
		          a_constant        *result,
			  an_error_code     *err_code,
			  an_error_severity *err_severity)
/*
Do the complement operation on all types of integers.
*/
{
  an_integer_value result_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant->variant.integer_value;
  complement_integer_value(&result_value);
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/FALSE,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);
  result->non_arithmetic = TRUE;

#if DEBUG
  db_unary_operation("~", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_complement */


static void do_not(a_constant        *constant,
		   a_constant        *result,
                   a_boolean         *did_not_fold)
/*
Do the "!" (not) operation on all types of scalars.
*/
{
  *did_not_fold = FALSE;
  if (!constant_bool_value_known_at_compile_time(constant)) {
    /* The constant's value is not known until link time, so the conversion
       cannot be folded at this time. */
    *did_not_fold = TRUE;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)is_false_constant(constant));
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "! did not fold\n");
  } else {
    db_unary_operation("!", constant, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_not */

#if GNU_COMPLEX_EXTENSIONS_ALLOWED

static void do_xconj(a_constant        *constant,
                     a_constant        *result,
                     an_error_code     *err_code,
                     an_error_severity *err_severity,
                     a_boolean         *depends_on_fp_mode)
/*
Do the complex conjugation operation (i.e., negate the imaginary part) on all
types of complex values.
*/
{
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_boolean    err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  copy_constant(constant, result);
  fp_negate(float_kind, &constant->variant.complex_value->imag,
            &result->variant.complex_value->imag, &err, depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("x~", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_xconj */


static void do_complex_projection(an_expr_operator_kind  op,
                                  a_constant             *constant,
                                  a_constant             *result)
/*
Extract the real or imaginary part of a complex constant.
*/
{
  check_assertion(is_complex_type(constant->type) &&
                  is_real_floating_type(result->type));
  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  if (op == (an_expr_operator_kind)eok_real_part) {
    result->variant.float_value = constant->variant.complex_value->real;
  } else {
    result->variant.float_value = constant->variant.complex_value->imag;
  }  /* if */
}  /* do_complex_projection */

#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */

/*
Return TRUE if the indicated constant is an address constant cast to
an integral type.  Such a constant is a link-time constant but not a
compile-time constant.
*/
#define is_addr_constant_cast_to_integral_type(constant)              \
  ((constant)->kind == (a_constant_repr_kind)ck_address &&            \
   (constant)->implicit_cast &&                                       \
   is_integral_or_enum_type((constant)->type))


void unary_operation(an_expr_operator_kind op,
		     a_constant            *constant,
                     a_type_ptr            result_type,
		     a_constant            *result,
                     a_boolean             constant_context,
                     a_boolean             evaluated_context,
                     a_boolean             *did_not_fold,
                     a_boolean             *template_constant,
                     an_error_code         *error_detected,
                     a_source_position     *err_pos)
/*
Fold unary operations on constants.  op indicates the operation,
constant the operand.  result_type indicates the desired result type.
The result constant is put into result.  If constant_context is FALSE,
this operation is being evaluated as part of a nonconstant expression,
so any error is reduced to a warning and *did_not_fold is returned TRUE.
If evaluated_context is FALSE, this operation is being done in a
not-evaluated context (e.g., a sizeof or a dead branch of a "?" operator),
so any error is thrown away and *did_not_fold is returned TRUE.
*did_not_fold is also returned TRUE if the operation could not be
folded for any other reason (*template_constant is returned TRUE if
the reason is that the constant is a template parameter constant).
If error_detected is non-NULL, set *error_detected to the code for any
error detected, and do not issue the diagnostic, or set it to
ec_no_error if there was no error.  *err_pos is used as the position
for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;

  db_enter(5, "unary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  err_code = ec_no_error;
  err_severity = es_warning;
  if (is_error_constant(constant)) {
    /* The constant is an error constant; set the result to an error
       constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant->kind == (a_constant_repr_kind)ck_template_param ||
              (context_may_have_dependent_types() &&
               is_template_dependent_type(result_type)))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
#if UPC_EXTENSIONS_ALLOWED  
  } else if (constant->kind == (a_constant_repr_kind)ck_upc_mythread ||  
             constant->kind == (a_constant_repr_kind)ck_upc_threads) {  
    /* The UPC pseudo-constants are not true constants.  As a result, we do
       not fold unary operations involving these constants. */
    *did_not_fold = TRUE;  
#endif /* UPC_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  } else if (constant->kind == (a_constant_repr_kind)ck_label_difference) {
    /* The representation for a GNU label difference (&&K-&&L) is not
       a constant known at compile time. */
    *did_not_fold = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (is_addr_constant_cast_to_integral_type(constant)) {
      /* An address constant cast to an integral type is a link-time
         constant, not a compile-time constant.  We cannot do operations
         on it. */
      *did_not_fold = TRUE;
    } else {
      a_type_kind  type_kind = skip_typerefs(constant->type)->kind;
      switch (op) {
        case eok_negate:
          switch (type_kind) {
            case tk_integer:
              do_inegate(constant, result, &err_code, &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxnegate(constant, result, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fnegate(constant, result, &err_code, &err_severity,
                         &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xnegate(constant, result, &err_code, &err_severity,
                         &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_unary_plus:
          copy_constant(constant, result);
          break;
        case eok_complement:
          do_complement(constant, result, &err_code, &err_severity);
          break;
        case eok_not:
          do_not(constant, result, did_not_fold);
          break;
#if GNU_COMPLEX_EXTENSIONS_ALLOWED
        case eok_xconj:
          do_xconj(constant, result, &err_code, &err_severity,
                   &depends_on_fp_mode);
          break;
        case eok_real_part:
        case eok_imag_part:
          do_complex_projection(op, constant, result);
          break;
#endif /* GNU_COMPLEX_EXTENSIONS_ALLOWED */
#if CHECKING
        default:
          internal_error("unary_operation: bad unary operator");
          break;
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    if (err_code != ec_no_error) {
      /* There was an error or warning. */
      issue_folding_diagnostic(err_code, err_severity, constant_context,
                               evaluated_context, did_not_fold,
                               error_detected, err_pos, result);
      if (err_severity == es_error) depends_on_fp_mode = FALSE;
    }  /* if */
    /* If the source constant was formed using operations that are not allowed
       in forming a null pointer constant, the result cannot be used as
       a null pointer constant. */
    result->null_pointer_constant_ruled_out =
                          constant->null_pointer_constant_ruled_out ||
                          constant->kind != (a_constant_repr_kind)ck_integer ||
                          constant->implicit_cast;
    if (depends_on_fp_mode && !constant_context) {
      /* In a non-constant context, leave an operation to be done at runtime
         if its result depends on the floating-point mode. */
      *did_not_fold = TRUE;
    }  /* if */
  }  /* if */

  db_exit();
}  /* unary_operation */


#if DEBUG
static void db_binary_operation(char           *operation,
				a_constant_ptr constant_1,
				a_constant_ptr constant_2,
				a_constant_ptr result,
				an_error_code  err_code)
/*
Do a debug print giving the result of folding a two-operand constant operation.
*/
{
  if (db_flag_is_set("folding") || debug_level >= 5) {
    db_constant(constant_1);
    fprintf(f_debug, " %s ", operation);
    db_constant(constant_2);
    fprintf(f_debug, ", result = ");
    db_constant(result);
    if (err_code != ec_no_error) {
      fprintf(f_debug, " with ");
      if (err_code == ec_integer_overflow) {
        fprintf(f_debug, "integer overflow");
      } else if (err_code == ec_divide_by_zero) {
        fprintf(f_debug, "divide by zero");
      } else if (err_code == ec_mod_by_zero) {
        fprintf(f_debug, "mod by zero");
      } else {
        fprintf(f_debug, "error");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
}  /* db_binary_operation */
#endif /* DEBUG */


static void do_iadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity)
/*
Do the addition operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  add_integer_values(&result_value, &constant_2->variant.integer_value,
                     is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_iadd */


static void do_isubtract(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the subtract operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  subtract_integer_values(&result_value, &constant_2->variant.integer_value,
                          is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_isubtract */


static void do_imultiply(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the multiply operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  multiply_integer_values(&result_value, &constant_2->variant.integer_value,
                          is_signed, &err);
  if (err && is_signed) {
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_imultiply */


static void do_idivide(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Do the divide operation on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  divide_integer_values(&result_value, &constant_2->variant.integer_value,
                        is_signed, &err);
  if (err) {
    if (cmplit_integer_constant(constant_2, (a_host_large_integer)0) == 0) {
      /* Division by zero. */
      *err_code = ec_divide_by_zero;
      *err_severity = es_error;
    } else if (is_signed) {
      /* Other overflow. */
      *err_code = ec_integer_overflow;
      *err_severity = ES_INT_OVERFLOW;
    }  /* if */
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("i/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_idivide */


static void do_remainder(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the remainder operation ("%") on all types of integers.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = constant_1->variant.integer_value;
  is_signed = int_constant_is_signed(constant_1);
  remainder_integer_values(&result_value, &constant_2->variant.integer_value,
                           is_signed, &err);
  if (err) {
    if (cmplit_integer_constant(constant_2, (a_host_large_integer)0) == 0) {
      /* Division (remainder) by zero. */
      *err_code = ec_mod_by_zero;
      *err_severity = es_error;
    } else if (is_signed) {
      /* Other overflow. */
      *err_code = ec_integer_overflow;
      *err_severity = ES_INT_OVERFLOW;
    }  /* if */
  }  /* if */
  trunc_and_set_integer(&result_value, result, /*check_overflow=*/is_signed,
                        /*saturate_on_overflow=*/FALSE,
                        err_code, err_severity);

#if DEBUG
  db_binary_operation("%", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_remainder */


void check_shift_count(a_constant    *shift_count_constant,
                       a_type_ptr    operand_type,
                       an_error_code *err_code)
/*
shift_count_constant is the constant shift count for a shift operation.
The entity being shifted has the type operand_type.  Check the shift
count to see if it is valid.  If so, return *err_code set to ec_no_error;
if not, return *err_code set to the proper error code.
*/
{
  a_targ_size_t size;

  *err_code = ec_no_error;

  if (shift_count_constant->kind == (a_constant_repr_kind)ck_integer) {
    /* Determine the size of the operand being shifted. */
    operand_type = skip_typerefs(operand_type);
#if CHECKING
    if (operand_type->kind != (a_type_kind)tk_integer
#if FIXED_POINT_ALLOWED
        && operand_type->kind != (a_type_kind)tk_fixed_point
#endif /* FIXED_POINT_ALLOWED */
                                                            ) {
      internal_error("check_shift_count: operand_type not integer");
    } else if (operand_type->size == 0) {
      internal_error("check_shift_count: integer type has size 0");
    }  /* if */
#endif /* CHECKING */
    size = operand_type->size * targ_char_bit;

    if (sign_of_integer_constant(shift_count_constant) < 0) {
      /* Negative shift count. */
      *err_code = ec_negative_shift_count;
    } else if (cmplit_integer_constant(shift_count_constant,
                                       (a_host_large_integer)size) >= 0) {
      /* Shift count is too large. */
      *err_code = ec_shift_count_too_large;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  } else if (shift_count_constant->kind ==
                                  (a_constant_repr_kind)ck_label_difference) {
    /* Unknown value: No check possible. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  } else if (shift_count_constant->kind ==
                                       (a_constant_repr_kind)ck_upc_threads) {
    /* Unknown value: No check possible. */
#endif /* UPC_EXTENSIONS_ALLOWED */
  } else {
    unexpected_condition_str("check_shift_count: unexpected constant kind");
  }  /* if */
}  /* check_shift_count */


static void do_shift(a_constant        *constant_1,
		     a_constant        *constant_2,
		     a_constant        *result,
		     a_boolean         shift_right,
		     an_error_code     *err_code,
		     an_error_severity *err_severity)
/*
Low-level routine to do a left or right shift on an integer.  Shift
*constant_1 by *constant_2 (right if shift_right is TRUE, left otherwise),
and put the result in *result.  *err_code and *err_severity are set to
indicate any error/warning detected, or *err_code == ec_no_error if
everything went fine.
*/
{
  an_integer_value result_value;
  a_boolean        is_signed, err, too_large = FALSE;
  int              shift_count, extra_shift_count = 0;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    if (*err_code == ec_shift_count_too_large) {
      /* The shift count is too large. */
      too_large = TRUE;
      if (microsoft_mode || gnu_mode) {
        /* Too-large shift counts are only warnings in Microsoft and GNU
           modes. */
      } else {
        *err_severity = es_error;
      }  /* if */
    } else {
      /* Other errors (e.g., negative shift count) are always errors.
         Microsoft doesn't give errors, but it's not clear what they do,
         so we don't try to emulate the folding. */
      *err_severity = es_error;
    }  /* if */
  }  /* if */
  if (*err_severity != es_error) {
    /* Fold the shift. */
    result_value = constant_1->variant.integer_value;
    if (too_large) {
      /* Adjust the shift count for a too-large value. */
      int object_bit_size =
                  (int)((skip_typerefs(constant_1->type)->size)*targ_char_bit);
      if (targ_too_large_shift_count_is_taken_modulo_size) {
        /* We're supposed to reduce the shift count modulo the bit size
           of the object. */
        shift_count = (int)value_of_integer_constant(constant_2, &err);
        if (err) {
          /* The number is huge.  Give up. */
          *err_severity = es_error;
          goto end_of_folding;
        }  /* if */
        shift_count %= object_bit_size;
      } else {
        /* We're supposed to treat the shift count as if we really shift
           that many bits.  Just shift the amount beyond which we would
           not get any further change, i.e., the number of bits in the
           object.  Do it in two steps so that the low-level routines
           need not deal with the odd cases. */
        shift_count = object_bit_size-1;
        extra_shift_count = 1;
      }  /* if */
    } else {
      /* Normal shift count, not too big. */
      shift_count = (int)value_of_integer_constant(constant_2, &err);
      /* No need to check err because check_shift_count has already
         established that the shift count is reasonable. */
    }  /* if */
    if (shift_right) {
      /* Shift right. */
      is_signed = int_constant_is_signed(constant_1);
      /* If the operation is signed but the shift is unsigned, mask off the
         high order bits of the integer value that are not actually part of
         the value to be shifted.  This prevents those high order bits from
         being shifted in to the result. */
      if (is_signed && !targ_right_shift_is_arithmetic) {
        an_integer_kind		tmp_ikind;
        a_boolean		tmp_is_signed;
        int			tmp_bit_size;
        an_integer_value	mask;
        /* Determine attributes (size, signedness) of the new integer kind. */
        get_integer_attributes(result, &tmp_ikind, &tmp_is_signed,
                               &tmp_bit_size);
        make_integer_value_mask(&mask, tmp_bit_size);
        and_integer_values(&result_value, &mask);
      }  /* if */
      shift_right_integer_value(&result_value, shift_count, is_signed,
                               /*sign_extend=*/targ_right_shift_is_arithmetic);
      if (extra_shift_count != 0) {
        shift_right_integer_value(&result_value, extra_shift_count, is_signed,
                               /*sign_extend=*/targ_right_shift_is_arithmetic);
      }  /* if */
    } else {
      /* Shift left. */
      shift_left_integer_value(&result_value, shift_count, &err);
      if (extra_shift_count != 0) {
        shift_left_integer_value(&result_value, extra_shift_count, &err);
      }  /* if */
    }  /* if */
    trunc_and_set_integer(&result_value, result, /*check_overflow=*/FALSE,
                          /*saturate_on_overflow=*/FALSE,
                          err_code, err_severity);
  }  /* if */
end_of_folding:;
}  /* do_shift */


static void do_shiftr(a_constant        *constant_1,
		      a_constant        *constant_2,
		      a_constant        *result,
		      an_error_code     *err_code,
		      an_error_severity *err_severity)
/*
Do the shift right operation on all types of integers.
*/
{
  do_shift(constant_1, constant_2, result, /*shift_right=*/TRUE,
	   err_code, err_severity);

#if DEBUG
  db_binary_operation(">>", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_shiftr */


static void do_shiftl(a_constant        *constant_1,
		      a_constant        *constant_2,
		      a_constant        *result,
		      an_error_code     *err_code,
		      an_error_severity *err_severity)
/*
Do the shift left operation on all types of integers.
*/
{
  do_shift(constant_1, constant_2, result, /*shift_right=*/FALSE,
	   err_code, err_severity);

#if DEBUG
  db_binary_operation("<<", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_shiftl */

#if FIXED_POINT_ALLOWED

static void do_fxshift(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       a_boolean         shift_right,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Low-level routine to do a left or right shift on a fixed-point value.
Shift *constant_1 by *constant_2 (right if shift_right is TRUE, left
otherwise), and put the result in *result.  *err_code and *err_severity are
set to indicate any error/warning detected, or *err_code == ec_no_error if
everything went fine.
*/
{
  int		shift_count;
  a_boolean	err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    *err_severity = es_error;
  }  /* if */
  if (*err_severity != es_error) {
    /* Fold the shift. */
    shift_count = (int)value_of_integer_constant(constant_2, &err);
    /* No need to check err because check_shift_count has already
       established that the shift count is reasonable. */
    fxp_shift(constant_1, shift_count, result, shift_right, &err);
    if (err) {
      *err_code = ec_bad_fixed_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
}  /* do_fxshift */


static void do_fxshiftr(a_constant        *constant_1,
		        a_constant        *constant_2,
		        a_constant        *result,
		        an_error_code     *err_code,
		        an_error_severity *err_severity)
/*
Do the shift right operation on fixed-point values.
*/
{
  do_fxshift(constant_1, constant_2, result, /*shift_right=*/TRUE,
	     err_code, err_severity);

#if DEBUG
  db_binary_operation("fx>>", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxshiftr */


static void do_fxshiftl(a_constant        *constant_1,
		        a_constant        *constant_2,
		        a_constant        *result,
		        an_error_code     *err_code,
		        an_error_severity *err_severity)
/*
Do the shift left operation on fixed-point values.
*/
{
  do_fxshift(constant_1, constant_2, result, /*shift_right=*/FALSE,
	     err_code, err_severity);

#if DEBUG
  db_binary_operation("fx<<", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxshiftl */

#endif /* FIXED_POINT_ALLOWED */

static void do_icompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result)
/*
Compare integers constant_1 and constant_2 according to the relational
operator "op", and return a 0 or 1 integer in "result".
*/
{
  int	cmp;
  int	result_value;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
  */
  cmp = cmp_integer_constants(constant_1, constant_2);
  /* Now determine the result value for this particular operator. */
  switch (op) {
    case eok_eq:  result_value = (cmp == 0); break;
    case eok_ne:  result_value = (cmp != 0); break;
    case eok_gt:  result_value = (cmp >  0); break;
    case eok_lt:  result_value = (cmp <  0); break;
    case eok_ge:  result_value = (cmp >= 0); break;
    case eok_le:  result_value = (cmp <= 0); break;
#if CHECKING
    default:       internal_error("do_icompare: bad operator");
#endif /* CHECKING */
  }  /* switch */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_icompare */

#if GNU_EXTENSIONS_ALLOWED

static void do_ignu_min_max(a_constant            *constant_1,
                            an_expr_operator_kind op,
                            a_constant            *constant_2,
                            a_constant            *result)
/*
Compare integers constant_1 and constant_2 and return the minimum or maximum
in result (depending on which operator is indicated by op).  This folds the
GNU C++ minimum and maximum operators ("<?" and ">?").
*/
{
  if (cmp_integer_constants(constant_1, constant_2) <= 0) {
    /* The first constant is no larger than the second one. */
    if (op == (an_expr_operator_kind)eok_gnu_min) {
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  } else {
    /* The first constant is larger. */
    if (op == (an_expr_operator_kind)eok_gnu_min) {
      copy_constant(constant_2, result);
    } else {
      copy_constant(constant_1, result);
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_ignu_min_max */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void do_and(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result)
/*
Do the bitwise "and" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  and_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("&", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_and */


static void do_or(a_constant    *constant_1,
		  a_constant    *constant_2,
		  a_constant    *result)
/*
Do the bitwise "or" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  or_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("|", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_or */


static void do_xor(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result)
/*
Do the bitwise "xor" operation on all types of integers.
*/
{
  an_integer_value result_value;

  result_value = constant_1->variant.integer_value;
  xor_integer_values(&result_value, &constant_2->variant.integer_value);
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("^", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_xor */


static void do_land(a_constant    *constant_1,
		    a_constant    *constant_2,
		    a_constant    *result,
                    a_boolean     *did_not_fold)
/*
Do the logical "and" (&&) operation on integers, floats, and pointers.
*/
{
  int res;

  *did_not_fold = FALSE;
  /* Fold the operation.  If either constant is a link-time constant, it
     may not be possible to fold at this time. */
  if (!constant_bool_value_known_at_compile_time(constant_1)) {
    *did_not_fold = TRUE;
  } else if (is_false_constant(constant_1)) {
    res = 0;
  } else if (!constant_bool_value_known_at_compile_time(constant_2)) {
    *did_not_fold = TRUE;
  } else if (is_false_constant(constant_2)) {
    res = 0;
  } else {
    res = 1;
  }  /* if */
  if (!*did_not_fold) {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)res);
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "&& did not fold\n");
  } else {
    db_binary_operation("&&", constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_land */


static void do_lor(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result,
                   a_boolean     *did_not_fold)
/*
Do the logical "or" (||) operation on integers, floats, and pointers.
*/
{
  int res;

  *did_not_fold = FALSE;
  /* Fold the operation.  If either constant is a link-time constant, it
     may not be possible to fold at this time. */
  if (!constant_bool_value_known_at_compile_time(constant_1)) {
    *did_not_fold = TRUE;
  } else if (!is_false_constant(constant_1)) {
    res = 1;
  } else if (!constant_bool_value_known_at_compile_time(constant_2)) {
    *did_not_fold = TRUE;
  } else if (!is_false_constant(constant_2)) {
    res = 1;
  } else {
    res = 0;
  }  /* if */
  if (!*did_not_fold) {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)res);
  }  /* if */
#if DEBUG
  if (*did_not_fold) {
    if (debug_level >= 5) fprintf(f_debug, "|| did not fold\n");
  } else {
    db_binary_operation("||", constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_lor */


static void do_fadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity,
                    a_boolean         *depends_on_fp_mode)
/*
Do the addition operation on all types of float and imaginary values.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant_1->kind == constant_2->kind);
  set_constant_kind(result, constant_1->kind);
  fp_add(float_kind,
         &constant_1->variant.float_value,
         &constant_2->variant.float_value,
         &result->variant.float_value, &err,
         depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fadd */


static void do_fsubtract(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the subtraction operation on all types of float and imaginary values.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_assertion(constant_1->kind == constant_2->kind);
  set_constant_kind(result, constant_1->kind);
  fp_subtract(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fsubtract */


static void do_fmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_constant_repr_kind
               result_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

#if C99_IL_EXTENSIONS_SUPPORTED
  /* Imaginary times float gives an imaginary result. */
  if ((constant_1->kind == (a_constant_repr_kind)ck_imaginary) !=
      (constant_2->kind == (a_constant_repr_kind)ck_imaginary)) {
    result_kind = (a_constant_repr_kind)ck_imaginary;
  }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  set_constant_kind(result, result_kind);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("f*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fmultiply */


static void do_fdivide(a_constant        *constant_1,
		       a_constant        *constant_2,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;
  a_constant_repr_kind
               result_kind = (a_constant_repr_kind)ck_float;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Check for division by zero to give a specific error message. */
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &constant_2->variant.float_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
#if C99_IL_EXTENSIONS_SUPPORTED
    /* Imaginary divided by float gives an imaginary result. */
    if ((constant_1->kind == (a_constant_repr_kind)ck_imaginary) !=
        (constant_2->kind == (a_constant_repr_kind)ck_imaginary)) {
      result_kind = (a_constant_repr_kind)ck_imaginary;
    }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    set_constant_kind(result, result_kind);
    fp_divide(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_fp_mode);
    if (err) {
      *err_code = ec_bad_float_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("f/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fdivide */


static void do_fcompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result,
                        a_boolean             *depends_on_fp_mode)
/*
Compare floating constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
*/
{
  int          cmp;
  int          result_value;
  a_boolean    unordered;
  a_float_kind float_kind =
                           skip_typerefs(constant_1->type)->variant.float_kind;

   *depends_on_fp_mode = FALSE;
  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
     "unordered" is set if the two values are unordered with respect to one
     another.
  */
  cmp = fp_compare(float_kind,
                   &constant_1->variant.float_value,
                   &constant_2->variant.float_value,
                   &unordered);
  /* Now determine the result value for this particular operator. */
  if (unordered) {
   *depends_on_fp_mode = TRUE;
   if (op == (an_expr_operator_kind)eok_ne) {
     /* If two values are unordered, they are unequal.  This is needed for
        NaN != NaN. */
     result_value = 1;
   } else {
     result_value = 0;
   }  /* if */
  } else {
    switch (op) {
      case eok_eq:  result_value = (cmp == 0); break;
      case eok_ne:  result_value = (cmp != 0); break;
      case eok_gt:  result_value = (cmp >  0); break;
      case eok_lt:  result_value = (cmp <  0); break;
      case eok_ge:  result_value = (cmp >= 0); break;
      case eok_le:  result_value = (cmp <= 0); break;
#if CHECKING
      default:       internal_error("do_fcompare: bad operator");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fcompare */

#if FIXED_POINT_ALLOWED

static void do_fxadd(a_constant        *constant_1,
                     a_constant        *constant_2,
                     a_constant        *result,
		     a_boolean	       *did_not_fold,
                     an_error_code     *err_code,
                     an_error_severity *err_severity)
/*
Do the addition operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_add(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxadd */


static void do_fxsubtract(a_constant        *constant_1,
                          a_constant        *constant_2,
                          a_constant        *result,
		          a_boolean	       *did_not_fold,
                          an_error_code     *err_code,
                          an_error_severity *err_severity)
/*
Do the subtraction operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_subtract(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxsubtract */


static void do_fxmultiply(a_constant        *constant_1,
                          a_constant        *constant_2,
                          a_constant        *result,
		          a_boolean	    *did_not_fold,
                          an_error_code     *err_code,
                          an_error_severity *err_severity)
/*
Do the multiplication operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
  fxp_multiply(constant_1, constant_2, result, did_not_fold, &err);
  if (err) {
    *err_code = ec_bad_fixed_operation_result;
    *err_severity = ES_FIXED_POINT_OVERFLOW;
  }  /* if */

#if DEBUG
  db_binary_operation("fx*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxmultiply */


static void do_fxdivide(a_constant        *constant_1,
                          a_constant        *constant_2,
                          a_constant        *result,
		          a_boolean	    *did_not_fold,
                          an_error_code     *err_code,
                          an_error_severity *err_severity)
/*
Do the division operation on all types of fixed-point values, and
combinations of fixed-point and integer values.
*/
{
  a_boolean err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Check for division by zero to give a specific error message. */
  if (fxp_value_is_zero(&constant_2->variant.fixed_point_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_fixed_point);
    fxp_divide(constant_1, constant_2, result, did_not_fold, &err);
    if (err) {
      *err_code = ec_bad_fixed_operation_result;
      *err_severity = ES_FIXED_POINT_OVERFLOW;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("fx/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_fxdivide */


static void do_fxcompare(a_constant            *constant_1,
                         an_expr_operator_kind op,
                         a_constant            *constant_2,
                         a_constant            *result)
/*
Compare fixed-point constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
*/
{
  int cmp;
  int result_value;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
  */
  check_assertion(constant_1->kind == constant_2->kind &&
                  constant_1->kind == (a_constant_repr_kind)ck_fixed_point);
  cmp = fxp_compare(constant_1, constant_2);
  /* Now determine the result value for this particular operator. */
  switch (op) {
    case eok_eq:  result_value = (cmp == 0); break;
    case eok_ne:  result_value = (cmp != 0); break;
    case eok_gt:  result_value = (cmp >  0); break;
    case eok_lt:  result_value = (cmp <  0); break;
    case eok_ge:  result_value = (cmp >= 0); break;
    case eok_le:  result_value = (cmp <= 0); break;
#if CHECKING
    default:        internal_error("do_fxcompare: bad operator");
#endif /* CHECKING */
  }  /* switch */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fxcompare */

#endif /* FIXED_POINT_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED

static void do_fgnu_min_max(a_constant            *constant_1,
                            an_expr_operator_kind op,
                            a_constant            *constant_2,
                            a_constant            *result)
/*
Compare floating constant_1 and constant_2 and return the minimum or maximum
in result (depending on which operator is indicated by op).  This folds the
GNU C++ minimum and maximum operators ("<?" and ">?").
*/
{
  a_boolean    unordered;
  a_float_kind float_kind =
                           skip_typerefs(constant_1->type)->variant.float_kind;
  int          order = fp_compare(float_kind,
                                  &constant_1->variant.float_value,
                                  &constant_2->variant.float_value,
                                  &unordered);

  if (op == (an_expr_operator_kind)eok_gnu_min) {
    if (!unordered && order < 0) {
      /* The first constant is less than the second. */
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  } else {
    /* Evaluate the C++ maximum operator. */
    if (!unordered && order > 0) {
      copy_constant(constant_1, result);
    } else {
      copy_constant(constant_2, result);
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fgnu_min_max */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity,
                    a_boolean         *depends_on_fp_mode)
/*
Do the addition operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_add(float_kind,
         &constant_1->variant.complex_value->real,
         &constant_2->variant.complex_value->real,
         &result->variant.complex_value->real, &err,
         &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_add(float_kind,
         &constant_1->variant.complex_value->imag,
         &constant_2->variant.complex_value->imag,
         &result->variant.complex_value->imag, &err,
         &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("x+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xadd */


static void do_xsubtract(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the subtraction operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_subtract(float_kind,
              &constant_1->variant.complex_value->real,
              &constant_2->variant.complex_value->real,
              &result->variant.complex_value->real, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_subtract(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &result->variant.complex_value->imag, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("x-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xsubtract */


static void do_xmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on all types of complex.
*/
{
  a_boolean                err, accum_err = FALSE, depends_on_mode;
  a_type_ptr               constant_type = skip_typerefs(constant_1->type);
  a_float_kind             float_kind = constant_type->variant.float_kind;
  an_internal_float_value  temp_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
#if 0
  /* This is an oversimplified algorithm that can exhibit dynamic range
     problems (e.g., catastrophic cancellation). */
#endif /* 0 */
  /* (a1 + b1*i) * (a2 + b2*i) = (a1a2 - b1b2) + (b1a2 + a1b2)i */
  /* Compute real part of the result. */
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->real,
              &constant_2->variant.complex_value->real,
              &result->variant.complex_value->real, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &temp_value, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_subtract(float_kind, &result->variant.complex_value->real, &temp_value,
              &result->variant.complex_value->real,
              &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  /* Compute imaginary part of the result. */
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->real,
              &constant_2->variant.complex_value->imag,
              &result->variant.complex_value->imag, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->real,
              &temp_value, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_add(float_kind, &result->variant.complex_value->imag, &temp_value,
         &result->variant.complex_value->imag,
         &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("x*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xmultiply */


static void do_xdivide(a_constant        *constant_1,
                       a_constant        *constant_2,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division operation on all types of complex.
*/
{
  a_boolean                err, accum_err = FALSE, depends_on_mode;
  a_type_ptr               constant_type = skip_typerefs(constant_1->type);
  a_float_kind             float_kind = constant_type->variant.float_kind;
  
  an_internal_float_value  quad_norm, temp_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
#if 0
  /* This is an oversimplified algorithm that can exhibit dynamic range
     problems (e.g., catastrophic cancellation). */
#endif /* 0 */
  /* Compute the real value quad_norm = real_2*real_2 + imag_2*imag_2. */
  fp_multiply(float_kind,
              &constant_2->variant.complex_value->real,
              &constant_2->variant.complex_value->real,
              &quad_norm, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_multiply(float_kind,
              &constant_2->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &temp_value, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  fp_add(float_kind, &quad_norm, &temp_value, &quad_norm,
         &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &quad_norm)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    /* Compute real part of the result. */
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->real,
                &constant_2->variant.complex_value->real,
                &result->variant.complex_value->real, &err,
                &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->imag,
                &constant_2->variant.complex_value->imag,
                &temp_value, &err,
                &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_add(float_kind, &result->variant.complex_value->real, &temp_value,
           &result->variant.complex_value->real,
           &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(float_kind, &result->variant.complex_value->real, &quad_norm,
              &result->variant.complex_value->real,
              &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    /* Compute imaginary part of the result. */
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->real,
                &constant_2->variant.complex_value->imag,
                &result->variant.complex_value->imag, &err,
                &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->imag,
                &constant_2->variant.complex_value->real,
                &temp_value, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_subtract(float_kind, &temp_value, &result->variant.complex_value->imag,
                &result->variant.complex_value->imag,
                &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    fp_divide(float_kind, &result->variant.complex_value->imag, &quad_norm,
              &result->variant.complex_value->imag,
              &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    if (accum_err) {
      *err_code = ec_bad_complex_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("x/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_xdivide */


static void do_xcompare(a_constant            *constant_1,
                        an_expr_operator_kind op,
                        a_constant            *constant_2,
                        a_constant            *result)
/*
Compare complex constants constant_1 and constant_2 according to the
relational operator "op", and return a 0 or 1 integer in "result".
Unlike real values, no ordering can be tested, only equality (or lack
thereof).
*/
{
  int          real_cmp, imag_cmp;
  int          result_value;
  a_boolean    real_unordered, imag_unordered;
  a_float_kind float_kind =
                           skip_typerefs(constant_1->type)->variant.float_kind;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
     "unordered" is set if the two values are unordered with respect to one
     another.
  */
  real_cmp = fp_compare(float_kind,
                        &constant_1->variant.complex_value->real,
                        &constant_2->variant.complex_value->real,
                        &real_unordered);
  imag_cmp = fp_compare(float_kind,
                        &constant_1->variant.complex_value->imag,
                        &constant_2->variant.complex_value->imag,
                        &imag_unordered);
  /* Now determine the result value for this particular operator. */
  /* If two values are unordered, they are unequal.  This is needed for
     NaN != NaN. */
  result_value = (real_cmp != 0) || (imag_cmp != 0) ||
                 real_unordered || imag_unordered;
  if (op == (an_expr_operator_kind)eok_eq) {
    result_value = !result_value;
  } else {
    check_assertion(op == (an_expr_operator_kind)eok_ne);
  }  /* if */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_xcompare */


static void do_jmultiply(a_constant        *constant_1,
                         a_constant        *constant_2,
                         a_constant        *result,
                         an_error_code     *err_code,
                         an_error_severity *err_severity,
                         a_boolean         *depends_on_fp_mode)
/*
Do the multiplication operation on two imaginary numbers (any precision).
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode = depends_on_mode;
  fp_negate(float_kind, &result->variant.float_value,
            &result->variant.float_value, &err, &depends_on_mode);
  accum_err |= err;
  *depends_on_fp_mode |= depends_on_mode;
  if (accum_err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_binary_operation("j*", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_jmultiply */


static void do_jdivide(a_constant        *constant_1,
                       a_constant        *constant_2,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity,
                       a_boolean         *depends_on_fp_mode)
/*
Do the division of a real number by an imaginary number (any precision).
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_mode;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *depends_on_fp_mode = FALSE;

  /* Check for division by zero to give a specific error message. */
  if (!IEEE_handling_on_float_operation_exceptions &&
      fp_is_zero_constant(float_kind, &constant_2->variant.float_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_imaginary);
    fp_divide(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode = depends_on_mode;
    fp_negate(float_kind, &result->variant.float_value,
              &result->variant.float_value, &err, &depends_on_mode);
    accum_err |= err;
    *depends_on_fp_mode |= depends_on_mode;
    if (accum_err) {
      *err_code = ec_bad_complex_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("j/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_jdivide */


static void do_real_imag_add_subtract(
                                     a_constant            *constant_1,
                                     an_expr_operator_kind op,
                                     a_constant            *constant_2,
                                     a_constant            *result,
                                     an_error_code         *err_code,
                                     an_error_severity     *err_severity,
                                     a_boolean             *depends_on_fp_mode)
/*
Do mixed real/imaginary addition and subtraction, i.e.,

  eok_fjadd      real      + imaginary
  eok_jfadd      imaginary + real
  eok_fjsubtract real      - imaginary
  eok_jfsubtract imaginary - real

These differ from simply converting to complex and adding, by the
preservation of negative zeroes.
*/
{
  a_boolean    err = FALSE;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  *depends_on_fp_mode = FALSE;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  switch (op) {
    case eok_fjadd:
      /* Real + imaginary. */
      result->variant.complex_value->real = constant_1->variant.float_value;
      result->variant.complex_value->imag = constant_2->variant.float_value;
      break;
    case eok_jfadd:
      /* Imaginary + real. */
      result->variant.complex_value->imag = constant_1->variant.float_value;
      result->variant.complex_value->real = constant_2->variant.float_value;
      break;
    case eok_fjsubtract:
      /* Real - imaginary. */
      result->variant.complex_value->real = constant_1->variant.float_value;
      fp_negate(float_kind,
                &constant_2->variant.float_value,
                &result->variant.complex_value->imag,
                &err, depends_on_fp_mode);
      break;
    case eok_jfsubtract:
      /* Imaginary - real. */
      result->variant.complex_value->imag = constant_1->variant.float_value;
      fp_negate(float_kind,
                &constant_2->variant.float_value,
                &result->variant.complex_value->real,
                &err, depends_on_fp_mode);
      break;
    default:
      unexpected_condition_str("do_real_imag_add_subtract: bad operator");
  }  /* switch */
  if (err) {
    *err_code = ec_bad_complex_operation_result;
    *err_severity = es_error;
  }  /* if */
#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_real_imag_add_subtract */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

static a_boolean valid_address_constant(a_constant *constant)
/*
Return TRUE if the given address constant is valid.  Specifically, check that
the offset in it falls within the base object.  This is used for subscript
checking.
*/
{
  a_boolean      valid;
  a_targ_size_t  object_size = 0;
  a_type_ptr     tp;
  a_constant_ptr cp;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    /* Integer cast to a pointer.  Don't know the underlying
       object.  Assume the pointer is okay. */
    valid = TRUE;
  } else {
#if CHECKING
    if (constant->kind != (a_constant_repr_kind)ck_address) {
      internal_error("valid_address_constant: not ck_address or ck_integer");
    }  /* if */
#endif /* CHECKING */
    /* Determine the size of the object pointed to. */
    switch(constant->variant.address.kind) {
      case abk_variable:
        tp = skip_typerefs(constant->variant.address.variant.variable->type);
        /* Ignore incomplete arrays and flexible arrays. */
        if (!is_incomplete_type(tp) &&
            !(is_immediate_class_type(tp) &&
              tp->variant.class_struct_union.contains_flexible_array_member)) {
          object_size = tp->size;
        }  /* if */
        break;
      case abk_routine:
      case abk_label:
        /* No size to check. */
        break;
      case abk_constant:
        cp = constant->variant.address.variant.constant;
        if (cp->kind == (a_constant_repr_kind)ck_string) {
          object_size = cp->variant.string.length;
        }  /* if */
        break;
      case abk_uuidof:
        tp = type_pointed_to(constant->type);
        object_size = tp->size;
        break;
      case abk_typeid:
        /* The object is std::type_info or a class derived from it.  So we
           don't really know the actual size. */
        break;
#if CHECKING
      default:
        internal_error("valid_address_constant: bad address constant kind");
#endif /* CHECKING */
    }  /* switch */
    /* See if the offset is valid given the size. */
    if (constant->variant.address.offset < 0) {
      /* A negative offset is never valid. */
      valid = FALSE;
    } else if (object_size != 0) {
      /* The offset right after the object is allowed.
         ANSI C allows that for arrays to simplify some coding.  That
         subscript value is flagged on a check of the subscript in expression
         form, which would be the form of any reference of the entity as
         an lvalue (see valid_node_if_subscript). */
      a_targ_size_t offset = (a_targ_size_t)constant->variant.address.offset;
      valid = (offset <= object_size);
    } else {
      /* Don't know what the size is, so assume the offset is valid. */
      valid = TRUE;
    }  /* if */
  }  /* if */

  return valid;
}  /* valid_address_constant */


static a_targ_size_t gcc_stride_size(a_type_ptr type)
/*
Return the stride size to be used for a pointer operation on pointer-to-type
in gcc mode.  gcc allows pointer addition and subtraction on pointer-to-void
and pointer-to-function.
*/
{
  a_targ_size_t size;

  type = skip_typerefs(type);
  if (is_void_type(type) ||
      is_function_type(type)) {
    size = 1;
  } else {
    size = type->size;
  }  /* if */
  return size;
}  /* gcc_stride_size */


static void accum_array_offset(a_constant_ptr  total_offset,
                               a_boolean       offset_is_signed,
                               a_boolean       subtract,
                               a_constant_ptr  count,
                               a_targ_size_t   elem_size,
                               a_boolean       no_ovflo_on_unsigned_add,
                               a_boolean       *ovflo,
                               a_boolean       *did_not_fold)
/*
Perform the multiply-add or multiply-subtract implied by array subscripting
or pointer arithmetic.  *total_offset is a constant to/from which the implied
offset must be added/subtracted (subtract determines which operation it is).
offset_is_signed determines whether *total_offset should be treated as a
signed value (this can be different from the signedness implied by the type).
count describes the number of array elements "added" or "subtracted", and
elem_size is the size of each of those elements.  *ovflo is set to TRUE if an
overflow occurs.  If an overflow resulting from an unsigned addition should
be ignored, no_ovflo_on_unsigned_add should be set to TRUE.  If the
operation could not be folded (because count is not a known integer
constant), return *did_not_fold TRUE.
*/
{
  *ovflo = FALSE;
  *did_not_fold = FALSE;
  if (count->kind != (a_constant_repr_kind)ck_integer) {
    *did_not_fold = TRUE;
  } else {
    an_integer_value  array_offset;
    a_boolean         count_is_signed = int_constant_is_signed(count);

    set_unsigned_integer_value(&array_offset, elem_size);
    multiply_integer_values(&array_offset, &count->variant.integer_value,
                            int_constant_is_signed(count), ovflo);
    if (!*ovflo) {
      /* Add/subtract the increment to/from the original offset. */
      if (subtract) {
        subtract_mixed_signed_integer_values(
            &total_offset->variant.integer_value, offset_is_signed,
            &array_offset, count_is_signed, ovflo);
      } else {
        add_mixed_signed_integer_values(
            &total_offset->variant.integer_value, offset_is_signed,
            &array_offset, count_is_signed, ovflo);
      }  /* if */
      /* If this was an unsigned integer operation, overflow is ignored. */
      if (no_ovflo_on_unsigned_add && !offset_is_signed) *ovflo = FALSE;
    }  /* if */
  }  /* if */
}  /* accum_array_offset */


static void do_padd(a_constant            *constant_1,
                    an_expr_operator_kind op,
                    a_constant            *constant_2,
                    a_constant            *result,
                    a_boolean             *did_not_fold,
                    an_error_code         *err_code,
                    an_error_severity     *err_severity)
/*
Do addition or subtraction on one pointer (constant_1) and one integer
(constant_2).  op indicates whether the source form was "+"
(eok_padd), "[]" (eok_subscript), or "-" (eok_psubtract).
Note that the integer can be of any type, specifically unsigned.
Also used to add or subtract a constant from an address constant
that has been cast to an integral type, as in "int i = (int)&j + 1;";
in that case, the operator is eok_add or eok_subtract.
*did_not_fold is returned TRUE if the operation cannot be folded.
*err_code and *err_severity are set to indicate any error/warning
detected, or *err_code == ec_no_error if everything went fine.
*/
{
  a_targ_size_t    size;
  a_constant       offset;
  a_boolean        err, offset_is_signed = FALSE;
  a_boolean        integer_case = FALSE;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;

  if (op == (an_expr_operator_kind)eok_add ||
      op == (an_expr_operator_kind)eok_subtract) {
    /* For the (int)address +- constant case, the size (scaling) is 1. */
    integer_case = TRUE;
    size = 1;
  } else {
    /* Get the size of the thing pointed to. */
    a_type_ptr  object_type =
                           f_skip_typerefs(type_pointed_to(constant_1->type));
    if (gcc_mode) {
      size = gcc_stride_size(object_type);
    } else {
      size = object_type->size;
    }  /* if */
    /* gnu mode allows empty classes with size zero, so pointers to
       such classes produce size zero here.  Likewise for some cases
       of arrays with zero bounds in gnu mode. */
    check_assertion_str(size != 0 || gnu_mode, "do_padd: size is zero");
  }  /* if */
  /* Get the offset from the first constant. */
  get_pointer_offset(constant_1, &offset);
  /* When dealing with an address cast to an integral type, treat the
     offset as having the signedness of the type cast to. */
  offset_is_signed = integer_case ? int_constant_is_signed(constant_1) :
                                    int_constant_is_signed(&offset);
  /* Perform the necessary multiply-add or multiply-subtract. */
  accum_array_offset(&offset, offset_is_signed,
                     (op == (an_expr_operator_kind)eok_psubtract ||
                      op == (an_expr_operator_kind)eok_subtract),
                      constant_2, size, (integer_case && !offset_is_signed),
                      &err, did_not_fold);
  if (!err && !*did_not_fold) {
    /* Build the result pointer constant. */
    copy_constant(constant_1, result);
    set_pointer_offset(result, &offset, &err);
    /* If this was an unsigned integer operation, overflow is ignored. */
    if (integer_case && !offset_is_signed) err = FALSE;
  }  /* if */
  if (err) {
    /* Some folding error. */
    *err_code = ec_integer_overflow;
    *err_severity = es_error;
  } else if (*did_not_fold) {
    set_error_constant(result);
  } else {
    /* Check that the offset lies within the base object. */
    if (!integer_case && !valid_address_constant(result)) {
      /* Use a different error message for cases where the original pointer
         addition was coded in [] form. */
      if (op == (an_expr_operator_kind)eok_subscript) {
        *err_code = ec_subscript_out_of_range;
      } else {
        *err_code = ec_pointer_outside_base_object;
      }  /* if */
      *err_severity = es_warning;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_padd */


static void do_pdiff(a_constant        *constant_1,
		     a_constant        *constant_2,
		     a_constant        *result,
		     a_boolean         *did_not_fold,
		     an_error_code     *err_code,
		     an_error_severity *err_severity)
/*
Do the pointer subtraction "pointer - pointer": pointer difference.
constant_1 and constant_2 are the two pointer constants.  The result
is returned in result.  If the operation cannot be folded to
a constant (because the pointers do not point to the same object),
*did_not_fold is returned TRUE.  *err_code and *err_severity are set
to indicate any error/warning detected, or *err_code == ec_no_error
if everything went fine.  Also handles address constants cast to an
integral type, as in "(int)&x - (int)&x".
*/
{
  a_constant       offset_2, offset_1;
  an_integer_value difference, size_intval;
  a_type_ptr       object_type;
  a_boolean        err, offset_1_is_signed, offset_2_is_signed;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* The two pointers must be in the same base object, or the operation
     cannot be folded.  An exception is the difference of two label addresses
     in GNU mode. */
  if (base_object(constant_1) != base_object(constant_2)) {
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_mode && constant_is_address_of_label(constant_1) &&
        constant_is_address_of_label(constant_2)) {
      clear_constant(result, (a_constant_repr_kind)ck_label_difference);
      result->variant.label_difference.from_address =
                                         alloc_shareable_constant(constant_2);
      result->variant.label_difference.to_address =
                                         alloc_shareable_constant(constant_1);
      result->type = integer_type(targ_ptrdiff_t_int_kind);
    } else
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      *did_not_fold = TRUE;
    }  /* if */
  } else {
    /* The pointers are in the same base object, so the difference of
       their offsets can be taken. */
    get_pointer_offset(constant_1, &offset_1);
    offset_1_is_signed = int_constant_is_signed(&offset_1);
    get_pointer_offset(constant_2, &offset_2);
    offset_2_is_signed = int_constant_is_signed(&offset_2);
    difference = offset_1.variant.integer_value;
    subtract_mixed_signed_integer_values(&difference,
                                         offset_1_is_signed,
                                         &offset_2.variant.integer_value,
                                         offset_2_is_signed, &err);
    if (!err) {
      /* Divide the difference by the size of the objects pointed to.
         The caller has already checked that the type pointed to is
         not incomplete, so the size is not zero (except possibly
         in gcc mode).  If the address constants have been cast to
         integer, there is no scaling. */
      if (!is_integral_type(constant_1->type)) {
        a_targ_size_t  object_size;
        object_type = type_pointed_to(constant_1->type);
        object_type = skip_typerefs(object_type);
        if (gcc_mode) {
          object_size = gcc_stride_size(object_type);
        } else {
          object_size = object_type->size;
        }  /* if */
        /* Division by zero can come up in GNU mode with pointers to empty
           class types or pointers to zero-length arrays. */
        check_assertion_str(object_size != 0 || gnu_mode,
                            "do_pdiff: size of object pointed to is zero");
        set_unsigned_integer_value(&size_intval, object_size);
        divide_integer_values(&difference, &size_intval,
                              int_constant_is_signed(result), &err);
      }  /* if */
    }  /* if */
    if (!err) {
      trunc_and_set_integer(&difference, result, /*check_overflow=*/TRUE,
                            /*saturate_on_overflow=*/FALSE,
                            err_code, err_severity);
    } else {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level  >= 5) {
    if (*did_not_fold) {
      fprintf(f_debug, "do_pdiff: did not fold\n");
    } else {
      db_binary_operation("pd", constant_1, constant_2, result, *err_code);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
} /* do_pdiff */


static void do_pcompare(a_constant            *constant_1,
			an_expr_operator_kind op,
			a_constant            *constant_2,
			a_constant            *result,
                        a_boolean             *did_not_fold,
			an_error_code         *err_code,
			an_error_severity     *err_severity)
/*
Fold a relational operation on two pointer constants.  constant_1 and
constant_2 are compared according to the indicated operator, and
*result is set to an integer 0 or 1 for the result.  *err_code and
*err_severity are set if there are warnings or errors, or
*err_code == ec_no_error is there were no problems.  *did_not_fold is
set if the operation cannot be folded.
*/
{
  a_constant offset_1, offset_2;
  int        result_value;
  int        cmp;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* The two pointers must be in the same base object, or the operation
     cannot be folded. */
  if (base_object(constant_1) != base_object(constant_2)) {
    /* The pointers are in different objects.  There are some cases here
       we might guess at folding, like
         int i, j;
         if (&i != &j) { ... }  <--- probably different
       However, that seems pointless, and could actually cause problems
       (maybe a smart compiler puts i and j at the same address because
       their lifetimes are disjoint).  So we never fold cases involving
       different base objects. */
    *did_not_fold = TRUE;
  } else {
    /* The pointers are in the same base object, so they can be compared. */
    get_pointer_offset(constant_1, &offset_1);
    get_pointer_offset(constant_2, &offset_2);
    /* Compare the offsets, then generate a result value. */
    cmp = cmp_integer_constants(&offset_1, &offset_2);
    switch (op) {
      case eok_eq:  result_value = (cmp == 0); break;
      case eok_ne:  result_value = (cmp != 0); break;
      case eok_gt:  result_value = (cmp >  0); break;
      case eok_lt:  result_value = (cmp <  0); break;
      case eok_ge:  result_value = (cmp >= 0); break;
      case eok_le:  result_value = (cmp <= 0); break;
#if CHECKING
      default:       internal_error("do_pcompare: bad operator");
#endif /* CHECKING */
    }  /* switch */
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    set_integer_value(&result->variant.integer_value,
                      (a_host_large_integer)result_value);
  }  /* if */
#if DEBUG
  if (debug_level  >= 5) {
    if (*did_not_fold) {
      fprintf(f_debug, "do_pcompare: did not fold\n");
    } else {
      db_binary_operation(db_operator_names[op],
                          constant_1, constant_2, result, *err_code);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
}  /* do_pcompare */


static void do_pmcompare(a_constant            *constant_1,
                         an_expr_operator_kind op,
                         a_constant            *constant_2,
                         a_constant            *result)
/*
Fold a relational operation on two pointer-to-member constants.
constant_1 and constant_2 are compared according to the indicated operator,
and *result is set to an integer 0 or 1 for the result.
*/
{
  int result_value = FALSE;

  if (constant_1->variant.ptr_to_member.casting_base_class ==
                        constant_2->variant.ptr_to_member.casting_base_class &&
      constant_1->variant.ptr_to_member.is_function_ptr ==
                           constant_2->variant.ptr_to_member.is_function_ptr) {
    if (constant_1->variant.ptr_to_member.is_function_ptr) {
      result_value = (constant_1->variant.ptr_to_member.variant.routine ==
                      constant_2->variant.ptr_to_member.variant.routine);
    } else {
      /* For fields, test offsets instead of just field because of
         union fields. */
      a_field_ptr field1 = constant_1->variant.ptr_to_member.variant.field;
      a_field_ptr field2 = constant_2->variant.ptr_to_member.variant.field;
      result_value = (field1 == field2 ||
                      (field1 != NULL && field2 != NULL &&
                       field1->offset == field2->offset &&
                       field1->offset_bit_remainder ==
                                                field2->offset_bit_remainder));
    }  /* if */
  }  /* if */
  /* result_value is now set for the "==" case.  Complement it for the "!="
     case. */
  if (op == (an_expr_operator_kind)eok_ne) result_value = !result_value;
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  set_integer_value(&result->variant.integer_value,
                    (a_host_large_integer)result_value);
#if DEBUG
  if (debug_level  >= 5) {
    db_binary_operation(db_operator_names[op],
                        constant_1, constant_2, result, ec_no_error);
  }  /* if */
#endif /* DEBUG */
}  /* do_pmcompare */

#if UPC_EXTENSIONS_ALLOWED

static void set_integer_constant_to_upc_threads(a_constant  *ic)
/*
Change the given integer constant N to N*THREADS (unless N is zero).
*/
{
  if (!is_zero_constant(ic)) {
    ic->kind = (a_constant_repr_kind)ck_upc_threads;
  }  /* if */
}  /* set_integer_constant_to_upc_threads */


static void convert_upc_threads_constant_to_integer(a_constant  *tc,
                                                    a_constant  *ic)
/*
tc is a constant representing N*THREADS.  Set ic to N.
*/
{
  copy_constant(tc, ic);
  ic->kind = (a_constant_repr_kind)ck_integer;
} /* convert_upc_threads_constant_to_integer */


static void binary_upc_threads_operation(
                                  an_expr_operator_kind op,
                                  a_constant            *constant_1,
                                  a_constant            *constant_2,
                                  a_type_ptr            result_type,
                                  a_constant            *result,
                                  a_boolean             constant_context,
                                  a_boolean             evaluated_context,
                                  a_boolean             *did_not_fold,
                                  a_boolean             *template_constant,
                                  a_source_position     *err_pos)
/*
Attempt to fold an operation (op) on two constants (constant_1, constant_2),
at least one of which is a UPC THREADS-based constant.  See binary_operation
(below) for the meaning of the other parameters.  Operations on UPC THREADS-
based constants are handled by converting them to integer constants, and
then converting the result back to being THREADS-based if appropriate.
*/
{
  a_constant  tmp;

  check_assertion(upc_dynamic_threads());
  if (constant_1->kind == (a_constant_repr_kind)ck_upc_threads &&
      constant_2->kind == (a_constant_repr_kind)ck_upc_threads) {
    /* E.g., THREADS/THREADS. */
    /* This case is not folded because the non-dynamic case might overflow.
       E.g., "3*THREADS/THREADS" cannot be folded to "3" because "3*THREADS"
       may be an overflow when THREADS is statically specified. */
    *did_not_fold = TRUE;
  } else {
    a_constant_ptr  nonthread_constant;
    if (constant_2->kind == (a_constant_repr_kind)ck_upc_threads) {
      /* E.g., 3*THREADS. */
      convert_upc_threads_constant_to_integer(constant_2, &tmp);
      constant_2 = &tmp;
      nonthread_constant = constant_1;
    } else {
      /* E.g., THREADS*3. */
      check_assertion(constant_1->kind ==
                                        (a_constant_repr_kind)ck_upc_threads);
      convert_upc_threads_constant_to_integer(constant_1, &tmp);
      constant_1 = &tmp;
      nonthread_constant = constant_2;
    }  /* if */
    switch (op) { 
      case eok_shiftl:
        if (nonthread_constant == constant_1) {
          *did_not_fold = TRUE;
          break;
        }  /* if */
        /*FALLTHROUGH*/
      case eok_multiply: 
        check_assertion(C_mode());  /* Would need error_detected in C++. */
        binary_operation(op, constant_1, constant_2, result_type, result, 
                         constant_context, evaluated_context, did_not_fold,
                         template_constant, (an_error_code *)NULL, err_pos); 
        if (!*did_not_fold) { 
          /* Convert the folded result back to a multiple of THREADS (unless
             it is zero). */
          set_integer_constant_to_upc_threads(result); 
        }  /* if */ 
        break; 
      case eok_add: 
      case eok_subtract: 
        /* Check for adding or subtracting zero */ 
        if (is_zero_constant(nonthread_constant)) {
          check_assertion(C_mode());  /* Would need error_detected in C++. */
          binary_operation(op, constant_1, constant_2, result_type, result, 
                           constant_context, evaluated_context, did_not_fold, 
                           template_constant, (an_error_code *)NULL, err_pos); 
          if (!*did_not_fold) { 
            set_integer_constant_to_upc_threads(result); 
          }  /* if */ 
        } else { 
          *did_not_fold = TRUE; 
        }  /* if */ 
        break; 
      default: 
        /* Cannot fold other operations */ 
        *did_not_fold = TRUE; 
        break; 
    }  /* switch */ 
  }  /* if */
}  /* binary_upc_threads_operation */

#endif /* UPC_EXTENSIONS_ALLOWED */

void binary_operation(an_expr_operator_kind op,
		      a_constant            *constant_1,
		      a_constant            *constant_2,
		      a_type_ptr            result_type,
		      a_constant            *result,
                      a_boolean             constant_context,
                      a_boolean             evaluated_context,
		      a_boolean             *did_not_fold,
                      a_boolean             *template_constant,
                      an_error_code         *error_detected,
                      a_source_position     *err_pos)
/*
Fold a two-operand constant operation.  op indicates the operation,
and constant_1 and constant_2 are the operands.  result_type indicates
the desired result type.  The result constant is placed in *result.
If constant_context is FALSE, this operation is being evaluated as
part of a nonconstant expression, so any error is reduced to a
warning and *did_not_fold is returned TRUE.  If evaluated_context
is FALSE, this operation is being done in a not-evaluated context
(e.g., a sizeof or a dead branch of a "?" operator), so any error
is thrown away and *did_not_fold is returned TRUE. *did_not_fold is
also returned TRUE if the operation could not be folded for any other
reason (*template_constant is returned TRUE if the reason is that
the constant is a template parameter constant).  If error_detected is
non-NULL, set *error_detected to the code for any error detected, and
do not issue the diagnostic, or set it to ec_no_error if there was no
error.  *err_pos is used as the position for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_fp_mode = FALSE;

  db_enter(5, "binary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  if (error_detected != NULL) *error_detected = ec_no_error;
  err_code = ec_no_error;
  err_severity = es_warning;

  if (is_error_constant(constant_1) || is_error_constant(constant_2)) {
    /* One and/or the other of the constants is an error constant; set the
       result to an error constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant_1->kind == (a_constant_repr_kind)ck_template_param ||
              constant_2->kind == (a_constant_repr_kind)ck_template_param ||
              (context_may_have_dependent_types() &&
               is_template_dependent_type(result_type)))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
#if GNU_EXTENSIONS_ALLOWED
  } else if (constant_1->kind == (a_constant_repr_kind)ck_label_difference ||
             constant_2->kind == (a_constant_repr_kind)ck_label_difference) {
    /* The representation for a GNU label difference (&&K-&&L) is not
       a constant known at compile time. */
    *did_not_fold = TRUE;
#endif /* GNU_EXTENSIONS_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  } else if (upc_mode &&
             (constant_1->kind == (a_constant_repr_kind)ck_upc_mythread || 
              constant_2->kind == (a_constant_repr_kind)ck_upc_mythread ||
              is_ptr_to_shared_type(constant_1->type) ||
              is_ptr_to_shared_type(constant_2->type))) {
    /* Operations involving MYTHREAD-based constants cannot be folded.
       Operations on addresses of shared data should not be folded in the
       front end either (though a back end might do so). */
    *did_not_fold = TRUE;
  } else if (upc_mode &&
             (constant_1->kind == (a_constant_repr_kind)ck_upc_threads ||
              constant_2->kind == (a_constant_repr_kind)ck_upc_threads)) {
    binary_upc_threads_operation(op, constant_1, constant_2, result_type,
                                 result, constant_context, evaluated_context,
                                 did_not_fold, template_constant, err_pos);
#endif /* UPC_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED
  } else if ((constant_1->kind == (a_constant_repr_kind)ck_fixed_point ||
              constant_2->kind == (a_constant_repr_kind)ck_fixed_point) &&
             (constant_1->kind != (a_constant_repr_kind)ck_fixed_point ||
              constant_2->kind != (a_constant_repr_kind)ck_fixed_point) &&
             (op != (an_expr_operator_kind)eok_shiftl) &&
             (op != (an_expr_operator_kind)eok_shiftr) &&
             (op != (an_expr_operator_kind)eok_add) &&
             (op != (an_expr_operator_kind)eok_subtract) &&
             (op != (an_expr_operator_kind)eok_multiply) &&
             (op != (an_expr_operator_kind)eok_divide)) {
    /* Fixed-point operations, except for the ones listed above,
       are not folded if the other operand is not also fixed-point. */
    *did_not_fold = TRUE;
#endif /* FIXED_POINT_ALLOWED */
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (is_addr_constant_cast_to_integral_type(constant_1)) {
      /* The first constant is an address constant cast to an integral
         type.  Such a constant is a link-time constant, not a compile-time
         constant.  We cannot in general do operations on it.  However,
         we can handle the special cases
           (int)addr_constant + int_constant
           (int)addr_constant - int_constant
         by using the pointer add/subtract routine. */
      if ((op == (an_expr_operator_kind)eok_add ||
           op == (an_expr_operator_kind)eok_subtract) &&
          constant_2->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_2->type)) {
          internal_error("binary_operation: address constant +- non-integer");
        }  /* if */
#endif /* CHECKING */
        do_padd(constant_1, op, constant_2, result, did_not_fold,
                &err_code, &err_severity);
      } else if ((gcc_mode ||
                  (gpp_mode && gnu_version < 40000)) &&
                 op == (an_expr_operator_kind)eok_subtract &&
                 is_addr_constant_cast_to_integral_type(constant_2)) {
        /* Allow
             (int)addr_constant - (int)addr_constant
           in GNU mode. */
        do_pdiff(constant_1, constant_2, result, did_not_fold,
                 &err_code, &err_severity);
      } else if (gnu_mode &&
                 op == (an_expr_operator_kind)eok_and &&
                 is_zero_constant(constant_2)) {
        /* gcc allows (int)"abc" & 0 as an integral constant. */
        do_and(constant_2 /* sic */, constant_2, result);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else if (is_addr_constant_cast_to_integral_type(constant_2)) {
      /* The second constant is an address constant cast to an integral
         type.  Such a constant is a link-time constant, not a compile-time
         constant.  We cannot in general do operations on it.  However,
         we can handle the special case
           int_constant + (int)addr_constant
         by using the pointer add routine. */
      if (op == (an_expr_operator_kind)eok_add &&
          constant_1->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_1->type)) {
          internal_error("binary_operation: non-integer + address constant");
        }  /* if */
#endif /* CHECKING */
        /* Note that we reverse the operands in the call so that the address
           constant is first. */
        do_padd(constant_2, op, constant_1, result, did_not_fold, &err_code,
                &err_severity);
      } else if (gnu_mode &&
                 op == (an_expr_operator_kind)eok_and &&
                 is_zero_constant(constant_1)) {
        /* gcc allows 0 & (int)"abc" as an integral constant. */
        do_and(constant_1, constant_1 /* sic */, result);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else {
      a_type_kind  operation_type_kind =
            binary_operation_type_kind(op, constant_1->type, constant_2->type);
      switch (op) {
        case eok_add:
          switch (operation_type_kind) {
            case tk_integer:
              do_iadd(constant_1, constant_2, result, &err_code,
                      &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxadd(constant_1, constant_2, result,
                       did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fadd(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xadd(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_subtract:
          switch (operation_type_kind) {
            case tk_integer:
              do_isubtract(constant_1, constant_2, result, &err_code,
                           &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxsubtract(constant_1, constant_2, result,
                            did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fsubtract(constant_1, constant_2, result, &err_code,
                           &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xsubtract(constant_1, constant_2, result, &err_code,
                      &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_multiply:
          switch (operation_type_kind) {
            case tk_integer:
              do_imultiply(constant_1, constant_2, result, &err_code,
                           &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxmultiply(constant_1, constant_2, result,
                            did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fmultiply(constant_1, constant_2, result,
                           &err_code, &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xmultiply(constant_1, constant_2, result,
                           &err_code, &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_divide:
          switch (operation_type_kind) {
            case tk_integer:
              do_idivide(constant_1, constant_2, result, &err_code,
                         &err_severity);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxdivide(constant_1, constant_2, result,
                          did_not_fold, &err_code, &err_severity);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
              do_fdivide(constant_1, constant_2, result,
                         &err_code, &err_severity, &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xdivide(constant_1, constant_2, result,
                         &err_code, &err_severity, &depends_on_fp_mode);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            default:
              unexpected_condition();
          }  /* switch */
          break;
        case eok_remainder:
          do_remainder(constant_1, constant_2, result, &err_code,
                       &err_severity);
          break;
        case eok_shiftl:
#if FIXED_POINT_ALLOWED
          if (operation_type_kind == (a_type_kind)tk_fixed_point) {
            do_fxshiftl(constant_1, constant_2, result, &err_code,
                        &err_severity);
          } else
#endif /* FIXED_POINT_ALLOWED */
          /* Do not insert code here. */
          {
            do_shiftl(constant_1, constant_2, result, &err_code,
                      &err_severity);
          }  /* if */
          break;
        case eok_shiftr:
#if FIXED_POINT_ALLOWED
          if (operation_type_kind == (a_type_kind)tk_fixed_point) {
            do_fxshiftr(constant_1, constant_2, result, &err_code,
                        &err_severity);
          } else
#endif /* FIXED_POINT_ALLOWED */
          /* Do not insert code here. */
          {
            do_shiftr(constant_1, constant_2, result, &err_code,
                      &err_severity);
          }  /* if */
          break;
        case eok_eq:
        case eok_ne:
        case eok_gt:
        case eok_lt:
        case eok_ge:
        case eok_le:
          switch (operation_type_kind) {
            case tk_integer:
              do_icompare(constant_1, op, constant_2, result);
              break;
#if FIXED_POINT_ALLOWED
            case tk_fixed_point:
              do_fxcompare(constant_1, op, constant_2, result);
              break;
#endif /* FIXED_POINT_ALLOWED */
            case tk_float:
              do_fcompare(constant_1, op, constant_2, result,
                          &depends_on_fp_mode);
              break;
#if C99_IL_EXTENSIONS_SUPPORTED
            case tk_complex:
              do_xcompare(constant_1, op, constant_2, result);
              break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
            case tk_pointer:
              do_pcompare(constant_1, op, constant_2, result, did_not_fold,
                          &err_code, &err_severity);
              break;
            case tk_ptr_to_member:
              do_pmcompare(constant_1, op, constant_2, result);
              break;
            case tk_nullptr:
              /* This is handled as an integer comparison, like an old-style
                 null pointer constant. */
              do_icompare(constant_1, op, constant_2, result);
              break;
            default:
              unexpected_condition();
          }  /* switch */
          break;
#if GNU_EXTENSIONS_ALLOWED
        case eok_gnu_max:
        case eok_gnu_min:
          switch (operation_type_kind) {
            case tk_integer:
              do_ignu_min_max(constant_1, op, constant_2, result);
              break;
            case tk_float:
              do_fgnu_min_max(constant_1, op, constant_2, result);
              break;
            case tk_pointer:
              *did_not_fold = TRUE;
              break;
            default:
              unexpected_condition();
          }  /* switch */
          break;
#endif /* GNU_EXTENSIONS_ALLOWED */
        case eok_and:
          do_and(constant_1, constant_2, result);
          break;
        case eok_or:
          do_or(constant_1, constant_2, result);
          break;
        case eok_xor:
          do_xor(constant_1, constant_2, result);
          break;
        case eok_land:
          do_land(constant_1, constant_2, result, did_not_fold);
          break;
        case eok_lor:
          do_lor(constant_1, constant_2, result, did_not_fold);
          break;

#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_jmultiply:
          do_jmultiply(constant_1, constant_2, result,
                       &err_code, &err_severity, &depends_on_fp_mode);
          break;
        case eok_jdivide:
          do_jdivide(constant_1, constant_2, result,
                     &err_code, &err_severity, &depends_on_fp_mode);
          break;
        case eok_fjadd:
        case eok_jfadd:
        case eok_fjsubtract:
        case eok_jfsubtract:
          /* Mixed real/imaginary addition/subtraction. */
          do_real_imag_add_subtract(constant_1, op, constant_2, result,
                                    &err_code, &err_severity,
                                    &depends_on_fp_mode);
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

        case eok_pdiff:
          do_pdiff(constant_1, constant_2, result, did_not_fold, &err_code,
                   &err_severity);
          break;
        case eok_padd:
          { a_constant_ptr ptr_con = constant_1;
            a_constant_ptr int_con = constant_2;
            /* The operands of pointer "+" can be in either order. */
            if (is_pointer_type(constant_2->type)) {
              ptr_con = constant_2;
              int_con = constant_1;
            }  /* if */
            do_padd(ptr_con, op, int_con, result, did_not_fold,
                    &err_code, &err_severity);
          }
          break;
        case eok_psubtract:
          do_padd(constant_1, op, constant_2, result, did_not_fold,
                  &err_code, &err_severity);
          break;
#if CHECKING
        default:
          internal_error("binary_operation: bad binary operator");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    if (err_code != ec_no_error) {
      /* There was an error or warning. */
      issue_folding_diagnostic(err_code, err_severity, constant_context,
                               evaluated_context, did_not_fold,
                               error_detected, err_pos, result);
      if (err_severity == es_error) depends_on_fp_mode = FALSE;
    }  /* if */
    /* If either constant was formed using operations that are not allowed
       in forming a null pointer constant, the result cannot be used as
       a null pointer constant. */
    result->null_pointer_constant_ruled_out =
                        constant_1->null_pointer_constant_ruled_out ||
                        constant_1->kind != (a_constant_repr_kind)ck_integer ||
                        (constant_1->implicit_cast &&
                         !is_nullptr_type(constant_1->type)) ||
                        constant_2->null_pointer_constant_ruled_out ||
                        constant_2->kind != (a_constant_repr_kind)ck_integer ||
                        (constant_2->implicit_cast &&
                         !is_nullptr_type(constant_2->type));
    if (depends_on_fp_mode && !constant_context) {
      /* In a non-constant context, leave an operation to be done at runtime
         if its result depends on the floating-point mode. */
      *did_not_fold = TRUE;
    }  /* if */
  }  /* if */

  db_exit();
}  /* binary_operation */


static void accum_field_offset(a_constant_ptr  total_offset,
                               a_field_ptr     field,
                               a_boolean       *ovflo)
/*
total_offset represents an offset: Add to it the offset of the given field,
and set *ovflo to TRUE if an overflow occurred.
*/
{
  an_integer_value  field_offset;

  set_unsigned_integer_value(&field_offset, field->offset);
  add_mixed_signed_integer_values(&total_offset->variant.integer_value,
                                  int_constant_is_signed(total_offset),
                                  &field_offset, /*is_signed=*/FALSE, ovflo);
}  /* accum_field_offset */


static void fold_field_selection(a_constant            *constant_1,
                                 a_field_ptr           field,
                                 a_type_ptr            result_type,
                                 a_constant            *result,
                                 a_boolean             *template_constant)
/*
Fold a constant field selection operation.  constant_1 is the pointer to the
struct/union; field is the selected field.  The result type (pointer to the
field type) is given by result_type.  The result is put in *result.
If constant_1 is a template parameter constant, return *template_constant
TRUE and do not fold the operation.  This folding operation is not done
through the usual interface because a field cannot be passed as a constant.
*/
{
  a_constant       offset;
  a_boolean        err;

  *template_constant = FALSE;
  copy_constant(constant_1, result);
  if (is_error_constant(constant_1)) {
    /* An error constant stays the same. */
  } else if (constant_1->kind == (a_constant_repr_kind)ck_template_param) {
    /* A template parameter constant.  This shows up in cases like
         ((T *)0)->x
       which can come up as part of the expansion of offsetof. */
    *template_constant = TRUE;
  } else {
    /* Take the pointer offset, ... */
    get_pointer_offset(constant_1, &offset);
    /* ... and add the offset of the field. */
    accum_field_offset(&offset, field, &err);
    if (!C_mode()) {
      /* In C++ mode, special care must be taken with members of (standard)
         anonymous unions, since such a union may introduce its own offset. */
      a_symbol_ptr  au_parent_sym =
                     symbol_for(field)->variant.field.anonymous_parent_object;
      /* Since anonymous unions can be nested, loop to find the outermost
         union. */
      while (au_parent_sym != NULL &&
             au_parent_sym->kind != (a_symbol_kind)sk_variable) {
        a_field_ptr  au_parent;
        check_assertion(au_parent_sym->kind == (a_symbol_kind)sk_field);
        au_parent = au_parent_sym->variant.field.ptr;
        if (au_parent->type->kind != (a_type_kind)tk_union) {
          /* A nonstandard anonymous union: The field selection is explicitly
             expanded for such cases, and so no special adjustment must be
             made here. */
          break;
        }  /* if */
        accum_field_offset(&offset, au_parent, &err);
        au_parent_sym = au_parent_sym->variant.field.anonymous_parent_object;
      }  /* while */
    }  /* if */
    /* Put the offset into the result pointer constant.  Note that no
       overflow/object-size checking is needed, since the field has to be
       within the underlying object. */
    set_pointer_offset(result, &offset, &err);
    implicit_cast(result, result_type);
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "fold_field_selection: offset = ");
    db_constant(&offset);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* CHECKING */
}  /* fold_field_selection */


static a_boolean constant_padd_or_subscript(
                                           an_expr_node_ptr expr,
                                           a_constant       *con,
                                           a_boolean        address_escapes,
                                           a_boolean        *template_constant)
/*
expr is an expression for an eok_padd, eok_psubtract, or eok_subscript
operation.  If its result (eok_padd, eok_psubtract) or address (lvalue
eok_subscript) is constant, return the value/address in *con, and return TRUE.
address_escapes and template_constant are as for constant_lvalue_address
(except that template_constant is always non-NULL).
*/
{
  a_boolean        is_constant = FALSE;
  an_expr_node_ptr ptr_op = expr->variant.operation.operands;
  an_expr_node_ptr int_op = ptr_op->next;
  a_constant       ptr_con;

  *template_constant = FALSE;
  if (expr->variant.operation.pointer_operand_is_second) {
    /* The operands are in the order integer + pointer or integer[pointer]. */
    int_op = expr->variant.operation.operands;
    ptr_op = int_op->next;
  }  /* if */
  if (is_constant_node(int_op) &&
      constant_rvalue_pointer(ptr_op, &ptr_con, address_escapes,
                              template_constant)) {
    /* Both operands are constant; fold to a constant address. */
    a_constant_ptr    int_con = int_op->variant.constant;
    an_error_code     err_code;
    an_error_severity err_severity;
    a_boolean         did_not_fold;
    if (int_con->kind == (a_constant_repr_kind)ck_template_param ||
        ptr_con.kind  == (a_constant_repr_kind)ck_template_param) {
      /* At least one constant is a template parameter, so we're not going
         to fold this to a constant address. */
    } else {
      do_padd(&ptr_con, expr->variant.operation.kind, int_con, con,
              &did_not_fold, &err_code, &err_severity);
      if (!did_not_fold &&
          (err_code == ec_no_error || err_severity == es_warning)) {
        is_constant = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_constant;
}  /* constant_padd_or_subscript */


static void make_constant_routine_address(a_routine_ptr  rout,
                                          a_constant_ptr con,
                                          a_boolean      address_escapes,
                                          a_boolean      *template_constant)
/*
Helper routine for constant_lvalue_address and constant_rvalue_pointer to
make a constant for the address of a routine.  address_escapes and
template_constant are as for constant_lvalue_address (except that
template_constant is always non-NULL).
*/
{
  set_routine_address_constant(rout, con,
                               /*set_address_taken=*/address_escapes);
  if (!routine_type_is_nonstatic_member_function(rout->type) &&
      rout->source_corresp.is_class_member &&
      scp_parent_class(&rout->source_corresp)->
                                 variant.class_struct_union.is_nonreal_class) {
    /* In a prototype instantiation, a static member function of the
       current class is template-dependent. */
    *template_constant = TRUE;
  } else if (context_may_have_dependent_types() &&
             is_template_dependent_type(rout->type)) {
    *template_constant = TRUE;
  }  /* if */
}  /* make_constant_routine_address */


a_boolean constant_lvalue_address(an_expr_node_ptr expr,
                                  a_constant       *con,
                                  a_boolean        address_escapes,
                                  a_boolean        *template_constant)
/*
expr is an lvalue expression.  If it has a constant address, put that
address in *con and return TRUE.  Otherwise, return FALSE.  address_escapes is
TRUE if the address might escape from its immediate context and get saved
somewhere (if in doubt, the safe value is TRUE).  *template_constant is
returned TRUE if the constant is template-dependent.  If template_constant
is NULL, a template-dependent constant is labeled as such at this level.
Passing it in as non-NULL is a signal that the caller would prefer to handle
that higher up.
*/
{
  a_boolean is_constant_addr = FALSE;
  a_boolean local_template_constant;

  if (template_constant == NULL) {
    template_constant = &local_template_constant;
  }  /* if */
  *template_constant = FALSE;
  expr = skip_parens(expr);
  check_assertion(expr->is_lvalue || is_error_node(expr));
  switch (expr->kind) {
    case enk_error:
      /* Assume an error expression could have been an lvalue with a
         constant address. */
      is_constant_addr = TRUE;
      set_error_constant(con);
      break;
    case enk_variable:
      /* An lvalue for a variable. */
      { a_variable_ptr var = expr->variant.variable;
        if (variable_has_constant_address(var)) {
          /* The variable has a constant address. */
          is_constant_addr = TRUE;
          set_variable_address_constant(var, con,
                                        /*set_address_taken=*/address_escapes);
          if (var->source_corresp.is_class_member &&
              scp_parent_class(&var->source_corresp)->
                     variant.class_struct_union.is_nonreal_class) {
            /* In a prototype instantiation, a static data member of the
               current class is template-dependent. */
            *template_constant = TRUE;
          }  /* if */
        }  /* if */
      }
      break;
    case enk_routine:
      /* An lvalue for a function. */
      make_constant_routine_address(expr->variant.routine.ptr, con,
                                    address_escapes,
                                    template_constant);
      is_constant_addr = TRUE;
      break;
    case enk_constant:
      /* The address of a string is a constant. */
      { a_constant_ptr econ = expr->variant.constant;
        if (econ->kind == (a_constant_repr_kind)ck_string) {
          is_constant_addr = TRUE;
          set_constant_address_constant(econ, con);
        }  /* if */
      }
      break;
    case enk_operation:
      { an_expr_node_ptr      op1 = expr->variant.operation.operands;
        an_expr_node_ptr      op2 = op1->next;
        an_expr_operator_kind op = expr->variant.operation.kind;
        a_constant            conaddr1;
        a_constant_ptr        pconaddr1;
        op1 = skip_parens(op1);
        if (op2 != NULL) op2 = skip_parens(op2);
        switch (op) {
          case eok_dot_field:
            /* Field selection, x.y.  If the left operand is an lvalue with a
               constant address, we can develop an address for the field. */
            if (op1->is_lvalue &&
                constant_lvalue_address(op1, &conaddr1, address_escapes,
                                        template_constant)) {
              pconaddr1 = &conaddr1;
              goto handle_field_selection;
            }  /* if */
            break;
          case eok_points_to_field:
            /* Field selection, p->y.  If the left operand is a constant
               address, we can develop an address for the field. */
            if (!is_constant_node(op1)) break;
            pconaddr1 = op1->variant.constant;
handle_field_selection:
            { a_field_ptr field;
              check_assertion(op2->kind == (an_expr_node_kind)enk_field);
              field = op2->variant.field;
              if (field->is_bit_field &&
                  !is_bit_field_whose_address_can_be_taken(field)) {
                /* You can't take the address of a bit field.  The error is
                   detected somewhere else.  Here, we just conclude we
                   can't produce a constant address. */
              } else {
                /* Not a bit field, or a bit field whose address can be taken
                   because it falls on byte boundaries. */
                is_constant_addr = TRUE;
                fold_field_selection(pconaddr1, field,
                                     make_pointer_type(expr->type),
                                     con, template_constant);
                if (*template_constant) is_constant_addr = FALSE;
              }  /* if */
            }
            break;
          case eok_subscript:
            /* Subscript operation. */
            if (constant_padd_or_subscript(expr, con, address_escapes,
                                           template_constant)) {
              is_constant_addr = TRUE;
            }  /* if */
            break;
          case eok_indirect:
            /* "*" operation.  If the operand is a constant address, we can
               use it as the address of the lvalue. */
            if (is_constant_node(op1) &&
                is_pointer_type(op1->type)) {
              is_constant_addr = TRUE;
              copy_constant(op1->variant.constant, con);
              if (con->kind == (a_constant_repr_kind)ck_template_param) {
                *template_constant = TRUE;
              }  /* if */
            }  /* if */
            break;
          case eok_ref_indirect:
            /* Reference "*" operation.  If the operand is a constant
               address, we can use it as the address of the lvalue. */
            if (is_constant_node(op1) &&
                is_any_reference_type(op1->type)) {
              is_constant_addr = TRUE;
              copy_constant(op1->variant.constant, con);
              con->type = make_pointer_type(type_pointed_to(op1->type));
              if (con->kind == (a_constant_repr_kind)ck_template_param) {
                *template_constant = TRUE;
              }  /* if */
            }  /* if */
            break;
          case eok_base_class_cast:
            /* A cast of a class lvalue to a base class. */
            check_assertion(op1->is_lvalue);
            if (constant_lvalue_address(op1, &conaddr1, address_escapes,
                                        template_constant)) {
              /* The operand has a constant address.  Fold the base class
                 cast into it. */
              if (is_template_dependent_type(expr->type) ||
                  *template_constant) {
                /* The type cast to is dependent or the source is dependent,
                   so add a template param cast. */
                make_template_param_cast_constant(&conaddr1, con, expr->type,
                                                  !expr->variant.operation.
                                                           compiler_generated);
                *template_constant = TRUE;
                is_constant_addr = TRUE;
              } else {
                a_boolean        did_not_fold;
                an_error_code    error_detected;
                a_base_class_ptr bcp;
                check_assertion(is_class_struct_union_type(op1->type) &&
                                is_class_struct_union_type(expr->type));
                bcp = find_base_class_of(op1->type, expr->type);
                check_assertion(bcp != NULL);
                fold_base_class_cast(&conaddr1, bcp, expr->type, con,
                                     /*check_cast_access=*/FALSE,
                                     /*check_ambiguity=*/FALSE,
                                     (a_boolean)expr->variant.operation.
                                                            compiler_generated,
                                     /*is_object_pointer=*/FALSE,
                                     &did_not_fold,
                                     &error_position,
                                     &error_detected);
                /* A cast to a virtual base class might not fold to a
                   constant even if the original pointer is a constant. */
                if (error_detected == ec_no_error && !did_not_fold) {
                  is_constant_addr = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            break;
          case eok_ref_cast:
          case eok_lvalue_adjust:
            /* These operations are used to adjust the type of an lvalue. */
            if (constant_lvalue_address(op1, &conaddr1, address_escapes,
                                        template_constant)) {
              /* The address of the operand is constant.  Adjust its type
                 and it is also the address of the result lvalue. */
              a_type_ptr new_type = make_pointer_type(expr->type);
              if (is_template_dependent_type(expr->type) ||
                  *template_constant) {
                /* The type cast to is dependent or the source is dependent,
                   so add a template param cast. */
                make_template_param_cast_constant(&conaddr1, con, new_type,
                                                  !expr->variant.operation.
                                                           compiler_generated);
                *template_constant = TRUE;
              } else {
                copy_constant(&conaddr1, con);
                implicit_or_explicit_cast(
                                   con, new_type,
                                   expr->variant.operation.compiler_generated);
              }  /* if */
              if (expr->variant.operation.is_reinterpret_cast) {
                con->is_reinterpret_cast = TRUE;
              }  /* if */
              is_constant_addr = TRUE;
            }  /* if */
            break;
          case eok_lvalue:
            /* The address of an eok_lvalue applied to a ck_template_param
               constant is sometimes a constant. */
            if (is_constant_node(op1)) {
              a_constant_ptr acon = op1->variant.constant;
              if (acon->kind == (a_constant_repr_kind)ck_template_param) {
                if (acon->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_member) {
                  /* The address of an lvalue for a tpck_member constant can
                     be represented by a tpck_address constant. */
                  clear_constant(con, (a_constant_repr_kind)ck_template_param);
                  set_template_param_constant_kind(
                                 con,
                                 (a_template_param_constant_kind)tpck_address);
                  con->variant.template_param.variant.constant = acon;
                  /* Note that we don't know whether the address is a pointer
                     or pointer to member. */
                  con->type = type_of_unknown_templ_param_nontype;
                  is_constant_addr = TRUE;
                  *template_constant = TRUE;
                } else if (acon->variant.template_param.kind ==
                                       (a_template_param_constant_kind)
                                                       tpck_unknown_function ||
                           acon->variant.template_param.kind ==
                                       (a_template_param_constant_kind)
                                                       tpck_template_ref) {
                  /* The address of an lvalue based on an unknown function
                     constant is the constant itself (which represents an
                     rvalue for the "address" of the function). */
                  copy_constant(acon, con);
                  is_constant_addr = TRUE;
                  *template_constant = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
            break;
          case eok_cli_subscript:  /* A C++/CLI array element is on the managed
                                      heap and therefore does not have a
                                      constant address. */
          default:
            /* Other operators cannot be folded. */
            break;
        }  /* switch */
      }
      break;
    case enk_param_ref:
      /* A reference to a parameter is similar to a variable with automatic
         storage duration: Its address is not a constant. */
      break;
    default:
      /* Other expression kinds cannot be folded. */
      break;
  }  /* switch */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && is_constant_addr) {
    /* A C++/CLI gc-lvalue should not ever be treated as a constant address,
       as its address might change if the garbage collector moves the
       underlying object. */
    if (is_gc_lvalue_expr(expr)) is_constant_addr = FALSE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (template_constant == &local_template_constant && is_constant_addr) {
    /* Handle tagging a template constant locally. */
    if (local_template_constant &&
        con->kind != (a_constant_repr_kind)ck_template_param) {
      /* Make sure there is a ck_template_param on top of a template-dependent
         case. */
      a_constant local_constant;
      copy_constant(con, &local_constant);
      make_template_param_cast_constant(&local_constant, con, con->type,
                                        /*is_explicit=*/FALSE);
    }  /* if */
  }  /* if */
  return is_constant_addr;
}  /* constant_lvalue_address */                                


a_boolean constant_rvalue_pointer(an_expr_node_ptr expr,
                                  a_constant       *con,
                                  a_boolean        address_escapes,
                                  a_boolean        *template_constant)
/*
expr is an rvalue expression of pointer type.  If it has a constant pointer
value, put that value in *con and return TRUE.  Otherwise, return FALSE.
address_escapes is TRUE if the address might escape from its immediate context
and get saved somewhere (if in doubt, the safe value is TRUE).
*template_constant is returned TRUE if the constant is template-dependent.
If template_constant is NULL, a template-dependent constant is labeled
as such at this level.  Passing it in as non-NULL is a signal that the
caller would prefer to handle that higher up.
*/
{
  a_boolean is_constant_ptr = FALSE;
  a_boolean local_template_constant;

  if (template_constant == NULL) {
    template_constant = &local_template_constant;
  }  /* if */
  *template_constant = FALSE;
  expr = skip_parens(expr);
  check_assertion(!expr->is_lvalue &&
                  ((is_pointer_type(expr->type) ||
                    is_template_param_type(expr->type) ||
                    is_error_type(expr->type)) ||
                   is_error_node(expr)));
  switch (expr->kind) {
    case enk_error:
      /* Assume an error expression could have been an rvalue constant
         pointer. */
      is_constant_ptr = TRUE;
      set_error_constant(con);
      break;
    case enk_variable:
      /* An rvalue for a variable.  There aren't any pointer-typed constant
         variables in the C or C++ languages currently. */
      break;
    case enk_param_ref:
      /* A reference to a parameter is similar to a variable in this
         respect. */
      break;
    case enk_routine:
      /* An rvalue for a function.  That's a function pointer, which can
         be rendered as a constant. */
      make_constant_routine_address(expr->variant.routine.ptr, con,
                                    address_escapes,
                                    template_constant);
      is_constant_ptr = TRUE;
      break;
    case enk_constant:
      /* A constant with pointer type is a constant pointer value. */
      copy_constant(expr->variant.constant, con);
      is_constant_ptr = TRUE;
      break;
    case enk_operation:
      { an_expr_node_ptr op1 = expr->variant.operation.operands;
        a_constant       conaddr1;
        op1 = skip_parens(op1);
        switch (expr->variant.operation.kind) {
          case eok_address_of:
            /* "&" operation.  If the operand is an lvalue with a constant
               address, the result is a constant pointer. */
            if (constant_lvalue_address(op1, con, address_escapes,
                                        template_constant)) {
              is_constant_ptr = TRUE;
            }  /* if */
            break;
          case eok_array_to_pointer:
            /* Array-to-pointer decay operation.  If the operand is an lvalue
               array with a constant address, the result is a constant
               pointer. */
            if (op1->is_lvalue &&
                constant_lvalue_address(op1, con, address_escapes,
                                        template_constant) &&
                is_pointer_type(con->type)) {
              a_type_ptr atype = type_pointed_to(con->type);
              if (is_array_type(atype)) {
                implicit_cast(con,
                            type_after_array_to_pointer_transformation(atype));
                is_constant_ptr = TRUE;
              }  /* if */
            }  /* if */
            break;
          case eok_padd:
          case eok_psubtract:
            /* p + i or i + p, or p - i.  These are constant if i is constant
               and p is or can be made constant. */
            if (constant_padd_or_subscript(expr, con, address_escapes,
                                           template_constant)) {
              is_constant_ptr = TRUE;
            }  /* if */
            break;
          case eok_cast:
            /* Pointer cast that passes through an address. */
            if (is_pointer_type(expr->type) &&
                is_pointer_type(op1->type)) {
              /* Allow only an identity cast or a cv-qualification change. */
              a_type_ptr target_type =
                                  f_skip_typerefs(type_pointed_to(expr->type));
              a_type_ptr source_type =
                                  f_skip_typerefs(type_pointed_to(op1->type));
              if (identical_types(target_type, source_type)) {
                goto cast_case;
              }  /* if */
            }  /* if */
            break;
          case eok_base_class_cast:
            /* Cast of a pointer to a base class pointer. */
            /* Casts of a class lvalue or rvalue shouldn't get here. */
cast_case:
            if (constant_rvalue_pointer(op1, &conaddr1, address_escapes,
                                        template_constant) &&
                !*template_constant) {
              an_error_code     err_code;
              an_error_severity err_severity;
              a_boolean         did_not_fold;
              clear_constant(con, (a_constant_repr_kind)ck_error);
              con->type = expr->type;
              /* Access checking is not done because it was already
                 done when the expression was put together. */
              conv_pointer_to_whatever(&conaddr1, con,
                                       /*check_cast_access=*/FALSE,
                                       /*check_ambiguity=*/FALSE,
                                       (a_boolean)expr->variant.operation.
                                                            compiler_generated,
                                       /*fold_constant_addr_exprs=*/TRUE,
                                       (a_boolean)expr->variant.operation.
                                                           is_reinterpret_cast,
                                       &did_not_fold,
                                       &error_position,
                                       &err_code, &err_severity,
                                       /*suppress_complex_diags=*/TRUE);
              /* A cast to a virtual base class might not fold to a
                 constant even if the original pointer is a constant. */
              if (err_code == ec_no_error && !did_not_fold) {
                is_constant_ptr = TRUE;
              }  /* if */
            }  /* if */
            break;
          default:
            /* Other operators cannot be folded. */
            break;
        }  /* switch */
      }
      break;
    default:
      /* Other expression kinds cannot be folded. */
      break;
  }  /* switch */
  if (template_constant == &local_template_constant && is_constant_ptr) {
    /* Handle tagging a template constant locally. */
    if (local_template_constant &&
        con->kind != (a_constant_repr_kind)ck_template_param) {
      /* Make sure there is a ck_template_param on top of a template-dependent
         case. */
      a_constant local_constant;
      copy_constant(con, &local_constant);
      make_template_param_cast_constant(&local_constant, con, con->type,
                                        /*is_explicit=*/FALSE);
    }  /* if */
  }  /* if */
  return is_constant_ptr;
}  /* constant_rvalue_pointer */                                


static a_boolean identical_pointer_types_ignoring_qualifiers(a_type_ptr type1,
                                                             a_type_ptr type2)
/*
Return TRUE if the two given types are pointer types whose underlying types
are the same ignoring cv-qualifiers.
*/
{
  a_boolean result = FALSE;

  if (is_pointer_type(type1) && is_pointer_type(type2)) {
    a_type_ptr under1 = type_pointed_to(type1);
    a_type_ptr under2 = type_pointed_to(type2);
    result = identical_types_ignoring_qualifiers(under1, under2);
  }  /* if */
  return result;
}  /* identical_pointer_types_ignoring_qualifiers */


a_boolean constant_is_pointer_to_string_literal(a_constant *con,
                                                a_constant **scon)
/*
Return TRUE if the indicated constant is a pointer to a string literal,
i.e., a string literal that has decayed (or been cast to) a pointer to
the underlying character type (possibly with different cv-qualifiers,
e.g., a const string could be cast to plain char *).  The string need not
be a narrow string literal.  If scon is non-NULL, *scon is set to point
to the string literal constant if there is one.
*/
{
  a_boolean result = FALSE;

  if (scon != NULL) *scon = NULL;
  if (con->kind == (a_constant_repr_kind)ck_address &&
      con->variant.address.kind == (an_address_base_kind)abk_constant &&
      con->variant.address.offset == 0 &&
      con->implicit_cast) {
    a_constant_ptr acon = con->variant.address.variant.constant;
    if (acon->kind == (a_constant_repr_kind)ck_string) {
      /* We have a ck_address constant pointing to a ck_string constant.
         Make sure the ck_address type is the type of the string after
         array-to-pointer decay. */
      a_type_ptr ts = type_after_array_to_pointer_transformation(acon->type);
      if (identical_pointer_types_ignoring_qualifiers(con->type, ts)) {
        result = TRUE;
        if (scon != NULL) *scon = acon;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* constant_is_pointer_to_string_literal */


a_boolean expr_is_pointer_to_string_literal(an_expr_node_ptr expr,
                                            a_constant       **scon)
/*
Return TRUE if the indicated expression is a pointer to a string literal,
i.e., a string literal that has decayed (or been cast to) a pointer to
the underlying character type (possibly with different cv-qualifiers,
e.g., a const string could be cast to plain char *).  The string need not
be a narrow string literal.  If scon is non-NULL, *scon is set to point
to the string literal constant if there is one.
*/
{
  a_boolean result = FALSE;

  if (scon != NULL) *scon = NULL;
  expr = skip_parens(expr);
  if (is_constant_node(expr)) {
    if (constant_is_pointer_to_string_literal(expr->variant.constant, scon)) {
      /* A constant for the address of a string literal, decayed to
         a pointer to the underlying type. */
      result = TRUE;
    }  /* if */
  } else if (is_operation_node(expr)) {
    an_expr_node_ptr cast_expr = NULL;
    if (node_operator_is(expr, eok_cast)) {
      /* Remember a cast on top of the expression for later testing. */
      cast_expr = expr;
      expr = skip_parens(expr->variant.operation.operands);
    }  /* if */
    if (is_operation_node(expr) &&
        node_operator_is(expr, eok_array_to_pointer)) {
      an_expr_node_ptr op1 = skip_parens(expr->variant.operation.operands);
      if (op1->is_lvalue && is_constant_node(op1) &&
          op1->variant.constant->kind == (a_constant_repr_kind)ck_string) {
        /* An expression for a string literal, decayed to a pointer to
           the underlying type. */
        result = TRUE;
        if (cast_expr != NULL) {
          /* For the cast case, make sure the cast type is the proper decayed
             type. */
          a_type_ptr decayed_type =
                         type_after_array_to_pointer_transformation(op1->type);
          if (!identical_pointer_types_ignoring_qualifiers(decayed_type,
                                                           cast_expr->type)) {
            /* The cast is to the wrong type. */
            result = FALSE;
          }  /* if */
        }  /* if */
        if (result && scon != NULL) *scon = op1->variant.constant;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* expr_is_pointer_to_string_literal */


static a_boolean add_offset_of_accessed_member(an_expr_node_ptr   expr,
                                               a_constant_ptr     offset,
                                               a_source_position  *pos)
/*
expr represents the element access operation of a builtin offsetof operator
(or a part thereof in multilevel cases).  Add to *offset the offset implied
by this access operation.  Multilevel cases (e.g., "offsetof(T, x[3].y)") are
handled through recursion.  Error cases can occur when accessing a member of a
virtual base, or when dealing with subscripts that are too large (overflow).
In such cases, return FALSE and issue a diagnostic at the given position (if
it is non-NULL).  Otherwise, return TRUE.
*/
{
  a_boolean         okay = TRUE, ovflo = FALSE;
  an_expr_node_ptr  args;
  an_integer_value  int_val;

  /* eok_parens shouldn't appear in these generated operations. */
  if (is_constant_node(expr)) {
    /* Presumably the null constant that is the root of the tree. */
    check_assertion(is_false_constant(expr->variant.constant));
    goto done;
  } else {
    check_assertion(is_operation_node(expr));
    /* Do a recursive call to process the bottom of the tree first. */
    args = expr->variant.operation.operands;
    okay = add_offset_of_accessed_member(args, offset, pos);
  }  /* if */
  switch (expr->variant.operation.kind) {
    case eok_dot_field:
    case eok_points_to_field:
      check_assertion(args->next->kind == (an_expr_node_kind)enk_field);
      accum_field_offset(offset, args->next->variant.field, &ovflo);
      break;
    case eok_subscript:
      { a_type_ptr       elem_type = type_pointed_to(args->type);
        an_expr_node_ptr arg2 = skip_parens(args->next);
        a_boolean        did_not_fold;
        check_assertion(is_constant_node(arg2));
        /* Note that while eok_subscript in general allows operands in
           either order, in offsetof the subscript is always the second
           operand. */
        accum_array_offset(offset, /*offset_is_signed=*/FALSE,
                           /*subtract=*/FALSE, arg2->variant.constant,
                           skip_typerefs(elem_type)->size,
                           /*no_ovflo_on_unsigned_add=*/FALSE, &ovflo,
                           &did_not_fold);
        if (did_not_fold) {
          okay = FALSE;
          if (pos != NULL) pos_error(ec_nonconstant_offsetof, pos);
        }  /* if */
      }
      break;
    case eok_base_class_cast:
      { a_type_ptr        dtype = args->type;
        a_type_ptr        btype = expr->type;
        a_base_class_ptr  bcp;
        /* Look for the base class to which the cast refers, and update
           *offset accordingly.  Since the field was unambiguous, the
           base class should be unambiguous too. */
        if (is_pointer_type(dtype)) {
          dtype = type_pointed_to(dtype);
          btype = type_pointed_to(btype);
        }  /* if */
        bcp = find_base_class_of(dtype, btype);
        check_assertion(bcp != NULL && !bcp->ambiguous);
        if (bcp->is_virtual) {
          /* We don't currently allow the offset of a member of a virtual
             base class to be taken (the GNU compiler produces a somewhat
             strange value). */
          okay = FALSE;
          if (pos != NULL) {
            pos_error(ec_offsetof_virtual_base_member, pos);
          }  /* if */
        } else {
          set_unsigned_integer_value(&int_val, bcp->offset);
          add_integer_values(&offset->variant.integer_value, &int_val,
                             /*is_signed=*/FALSE, &ovflo);
        }  /* if */
      }
      break;
    case eok_array_to_pointer:
    case eok_indirect:
    case eok_address_of:
      /* Just continue for these. */
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (okay && ovflo) {
    okay = FALSE;
    pos_error(ec_integer_overflow_internal, pos);
  }  /* if */
done:
  return okay;
}  /* add_offset_of_accessed_member */


static a_boolean is_template_dependent_offsetof_member(
                                            an_expr_node_ptr  expr,
                                            a_boolean         *not_a_constant)
/*
The given expression is the second operand of a bok_offsetof operation.
Return TRUE if that expression contains a template-dependent operation.
If it contains a non-constant subscript operation, set *not_a_constant to TRUE.
*/
{
  a_boolean  template_dependent = FALSE;

  while (!is_constant_node(expr)) {
    an_expr_node_ptr  args;
    check_assertion(is_operation_node(expr));
    args = expr->variant.operation.operands;
    if (node_operator_is(expr, eok_subscript)) {
      if (!is_constant_node(args->next)) {
        *not_a_constant = TRUE;
      } else if (args->next->variant.constant->kind ==
                                    (a_constant_repr_kind)ck_template_param) {
        template_dependent = TRUE;
      }  /* if */
    } else if (node_operator_is(expr, eok_dot_static)) {
      /* A dot-static operation comes up for something like
           __builtin_offsetof(A, T::m)
         in the prototype instantiation where we can't tell what kind of thing
         the lookup will find. */
      check_assertion(is_constant_node(args->next) &&
                      args->next->variant.constant->kind ==
                                     (a_constant_repr_kind)ck_template_param);
      template_dependent = TRUE;
    }  /* if */
    expr = args;
  }  /* while */
  return template_dependent;
}  /* is_template_dependent_offsetof_member */


static void fold_offsetof(an_expr_node_ptr   expr,
                          a_constant_ptr     constant,
                          a_boolean          maintain_expression,
                          a_source_position  *pos,
                          a_boolean          *not_a_constant)
/*
expr is an enk_builtin_operation node for a __builtin_offsetof operation
(currently only accepted in some GNU modes).  If any of the operands is
template-dependent, store a ck_template_param constant in *constant (the
constant will be of the tpck_expression variant and will point to the given
expression).  Otherwise, if the second operand contains a nonconstant
subscript, set *not_a_constant to TRUE and leave *constant unchanged.  In all
other cases, store the integer value of the offset being represented in
*constant (if maintain_expression is TRUE, the backing expression for the
returned constant will be set as well).  If a constant is returned through
*constant, *not_a_constant is set to FALSE.  If pos is non-NULL, diagnostics
are issued at the position it indicates.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;

  /* Start with the assumption that a constant will be produced. */
  *not_a_constant = FALSE;
  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand);
  if (is_template_dependent_type(arg1->variant.type_operand.type) ||
      is_template_dependent_offsetof_member(arg2, not_a_constant)) {
    /* The template-dependent case. */
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
    constant->type = expr->type;
  } else if (!*not_a_constant) {
    /* The foldable case. */
    set_unsigned_integer_constant(constant, (a_host_large_unsigned)0,
                                  targ_size_t_int_kind);
    if (add_offset_of_accessed_member(arg2, constant, pos)) {
      arg1->variant.type_operand.definition_needed = TRUE;
    } else {
      clear_constant(constant, (a_constant_repr_kind)ck_error);
    }  /* if */
    if (maintain_expression) constant->expr = expr;
    constant->type = expr->type;
  } else if (gpp_mode) {
    /* GNU C++ (as opposed to GNU C) does not allow nonconstant
       __builtin_offsetof operations. */
    if (pos != NULL) {
      pos_diagnostic(es_discretionary_error, ec_nonconstant_offsetof, pos);
    }  /* if */
  }  /* if */
}  /* fold_offsetof */


static void fold_is_base_of(an_expr_node_ptr   expr,
                            a_constant_ptr     constant,
                            a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_base_of operation.  If the
operand types are nondependent, store a boolean constant in *constant.  The
boolean constant will have value "true" if the operand types are (possibly
qualified) class types the first of which is a base class of the second one;
otherwise, the constant will have value "false".  If either of the operand
types is dependent, store a ck_template_param constant in *constant.  The
constant will be of the tpck_expression variant and will point to the given
expression.  If maintain_expression is TRUE, the backing expression for
the returned constant will be set as well.

*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    make_template_param_expr_constant(expr, constant);
  } else {
    a_boolean  result = FALSE;
    type1 = skip_typerefs(type1);
    type2 = skip_typerefs(type2);
    if (is_immediate_class_type(type1) && is_immediate_class_type(type2)) {
      result = (same_entities(type2, type1) ||
                find_base_class_of(type2, type1) != NULL);
    }  /* if */
    arg1->variant.type_operand.definition_needed = TRUE;
    arg2->variant.type_operand.definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_base_of */


static void fold_is_convertible_to(an_expr_node_ptr   expr,
                                   a_constant_ptr     constant,
                                   a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_convertible_to operation,
which implements the C++ TR1 is_convertible type relationship predicate
(see [lib.meta.rel]).  Store a boolean constant in *constant whose value
is "true" if the first operand type is "implicitly convertible to" the
second operand type.  If either of the operand types is dependent, store
a ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  from_rvalue = FALSE, ref_init = FALSE, result;
    /* The Microsoft version of this test considers the source as an rvalue
       in some cases. */
    if (microsoft_mode) {
      from_rvalue = TRUE;
      if (is_any_reference_type(type1)) {
        /* A reference on the source type is always ignored by the Microsoft
           compiler, except that without it conversions from rvalues are
           sometimes considered (instead of from lvalues as specified in
           the standard). */
        from_rvalue = FALSE;
        type1 = type_pointed_to(type1);
      } else if (is_array_type(type1)) {
        from_rvalue = FALSE;
      } else if (is_function_type(type1)) {
        from_rvalue = FALSE;
      } else if (is_class_struct_union_type(type1)) {
        from_rvalue = FALSE;
      }  /* if */
      if (is_any_reference_type(type2)) {
        a_type_ptr  under_type2 = type_pointed_to(type2);
        if (is_class_struct_union_type(under_type2) ||
            is_function_type(under_type2) || is_array_type(under_type2)) {
           /* A reference on the destination type appears to be ignored only if
             it is a reference to a class, array, or function type. */
          type2 = under_type2;
        } else {
          ref_init = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (microsoft_mode && from_rvalue && ref_init) {
      result = FALSE;
    } else if (microsoft_mode && identical_types(type1, type2)) {
      /* Microsoft returns TRUE when the types are the same, even
         if they are (e.g.) both arrays. */
      result = TRUE;
    } else if (microsoft_mode && is_void_type(type2)) {
      /* Microsoft considers a conversion to void to fail. */
      result = FALSE;
    } else {
      result = compute_is_convertible(type1, type2, from_rvalue);
    }  /* if */
    arg1->variant.type_operand.definition_needed = TRUE;
    arg2->variant.type_operand.definition_needed = TRUE;
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_is_convertible_to */


/*ARGSUSED*/  /* <-- FIXME: Function body not yet implemented. */
static void fold_is_constructible(an_expr_node_ptr   expr,
                                  a_constant_ptr     constant,
                                  a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for an __is_constructible or
__is_nothrow_constructible operation.  Store a boolean constant in *constant
whose value is "true" if the following variable definition
would be well-formed for some invented variable t:
      T t(declval<Args>()...);
If the built-in operation kind is bok_is_nothrow_constructible, the definition
must be known not to throw any exceptions.  If any of the operand types is
dependent, store a ck_template_param constant in *constant.  The constant will
be of the tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  /* FIXME: Not yet implemented. */
}

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean microsoft_has_assign_predicate(a_type_ptr                type,
                                                a_builtin_operation_kind  kind)
/*
Determine the value of the __has_assign or __has_nothrow_assign pseudo-function
(as indicated by kind) applied to the given type in Microsoft mode.
Ordinarily, the result for __has_nothrow_assign can be retrieved from a class'
symbol supplement, but in Microsoft mode, the result can depend on the order
of declaration of the assignment operators.
*/
{
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(type);
  a_symbol_ptr  sym = cssp->assignment_operator;
  a_boolean     is_list = FALSE, result = FALSE, found_copy_assign = FALSE;

  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        a_type_qualifier_set  qualifiers;
        a_boolean             ref_param, is_base_class_match;
        if (is_assignment_operator_for_copy(
                    sym, /*move_assign_okay=*/FALSE, &ref_param,
                    &qualifiers, &is_base_class_match)) {
          a_routine_ptr  rp = sym->variant.routine.ptr;
          if (kind == (a_builtin_operation_kind)bok_has_assign) {
            /* __has_assign returns true for any user-declared or nontrivial
               copy assignment (MSVC++ does not currently support defaulted
               assignment operators; we treat them like any other user-declared
               operators in that respect). */
            found_copy_assign = TRUE;
            if (!rp->compiler_generated || !rp->is_trivial_copy_function) {
              result = TRUE;
              break;
            }  /* if */
          } else if (!rp->compiler_generated) {
            /* __has_nothrow_assign: Return TRUE if the copy assignment
               operators are declared with "throw()" or a "nothrow"
               attribute. */
            found_copy_assign = TRUE;
            result = rp->never_throws ||
                     is_nothrow_type(skip_typerefs(rp->type));
            /* Microsoft compilers only consider the first declared copy
               assignment operator.  Since we store those operators in reverse
               order of declaration, continue the loop in case another such
               operator appears on the list. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!found_copy_assign &&
      kind == (a_builtin_operation_kind)bok_has_nothrow_assign) {
    /* If no copy assignment operator was found in the class, return the
       flag as recorded in the class supplement (which is independent of the
       declaration order of e.g. operator= in base classes). */
    result = cssp->has_nothrow_assign;
  }  /* if */
  return result;
}  /* microsoft_has_assign_predicate */


static a_boolean microsoft_has_copy_predicate(a_type_ptr                type,
                                              a_builtin_operation_kind  kind)
/*
Determine the value of the __has_copy or __has_nothrow_copy pseudo-function
(as indicated by kind) applied to the given type in Microsoft mode.
Ordinarily, the result for __has_nothrow_copy can be retrieved from a class'
symbol supplement, but in Microsoft mode, the result can depend on the order
of declaration of the constructors.
*/
{
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(type);
  a_symbol_ptr  sym = cssp->constructor;
  a_boolean     is_list = FALSE, result = FALSE, found_copy_ctor = FALSE;

  if (sym != NULL) {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        a_routine_ptr  rp = sym->variant.routine.ptr;
        a_type_ptr     rtp = skip_typerefs(rp->type);
        if (is_copy_constructor_type(rtp, type, (a_type_qualifier_set *)NULL,
                                     /*include_move_ctors=*/FALSE,
                                     /*is_declarative_context=*/TRUE)) {
          if (kind == (a_builtin_operation_kind)bok_has_copy) {
            /* __has_copy returns true for any user-declared or nontrivial
               copy constructor (MSVC++ does not currently support defaulted
               copy constructors; we treat them like any other user-declared
               constructors in that respect). */
            found_copy_ctor = TRUE;
            if (!rp->compiler_generated || !rp->is_trivial_copy_function) {
              result = TRUE;
              break;
            }  /* if */
          } else if (!rp->compiler_generated) {
            /* __has_nothrow_copy: Return TRUE if the copy constructors are
               declared with "throw()" or a "nothrow" attribute. */
            found_copy_ctor = TRUE;
            result = rp->never_throws ||
                     is_nothrow_type(skip_typerefs(rp->type));
            /* Microsoft compilers only consider the first declared copy
               constructor.  Since we store the constructors in reverse order
               of declaration, continue the loop in case another copy
               constructor appears on the list. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (!found_copy_ctor &&
      kind == (a_builtin_operation_kind)bok_has_nothrow_copy) {
    /* If no copy constructor was found in the class, return the flag as
       recorded in the class supplement (which is independent of the
       declaration order of e.g. constructors in base classes). */
    result = cssp->has_nothrow_copy;
  }  /* if */
  return result;
}  /* microsoft_has_copy_predicate */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean has_trivial_move_constructor(a_type_ptr  type)
/*
Return TRUE if the given class type has a trivial move constructor.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    /* We currently do not implicitly generate move constructors.  A trivial
       move therefore corresponds to a trivial copy. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->construction_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* has_trivial_move_constructor */


static a_boolean has_trivial_move_assign(a_type_ptr  type)
/*
Return TRUE if the given class type has a trivial move assignment operator.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    /* We currently do not implicitly generate move assignment operators.
       A trivial move therefore corresponds to a trivial copy. */
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->assignment_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* has_trivial_move_assign */


static a_boolean has_nothrow_move_assign(a_type_ptr  type)
/*
Return TRUE if the given class type has a trivial move assignment operator.
*/
{
  a_boolean  result;

  type = skip_array_types(type);
  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(type);
    if (cssp->assignment_by_bitwise_copy_allowed) {
      result = TRUE;
    } else {
      a_symbol_ptr  sym = cssp->assignment_operator;
      a_boolean     overloaded = symbol_is(sym, sk_overloaded_function);
      a_boolean     has_move_assign = FALSE;
      if (overloaded) sym = sym->variant.overloaded_function.symbols;
      result = TRUE;
      /* Look among the assignment operators for one that moves but doesn't
         throw. */
      for (; sym != NULL; sym = overloaded ? sym->next : NULL) {
        if (sym->kind == (a_symbol_kind)sk_member_function) {
          a_routine_ptr  rp = sym->variant.routine.ptr;
          if (routine_is_move_assignment_operator(rp)) {
            has_move_assign = TRUE;
            if (!rp->never_throws &&
                !is_nothrow_type(skip_typerefs(rp->type))) {
              /* We found a move-assignment operator that might throw. */
              result = FALSE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      /* If there was no move assignment operator at all, the result should be
         FALSE. */
      result = result && has_move_assign;
    }  /* if */
  } else {
    /* The argument type is not a class type and therefore has no copy
       assignment operators. */
    result = FALSE;
  }  /* if */
  return result;
}  /* has_nothrow_move_assign */


static void fold_unary_type_trait_helper(
                                    an_expr_node_ptr   expr,
                                    a_constant_ptr     constant,
                                    a_boolean          maintain_expression,
                                    a_source_position  *pos,
                                    a_boolean          complete_class_property)
/*
expr is an enk_builtin_operation node representing a boolean type predicate --
based on ISO/IEC 19768 -- with a single type operand (e.g., "__is_union").  If
the operand types is nondependent, store a boolean constant in *constant.  The
boolean constant will have value "true" if the associated type predicate is
true for the type represented by its operand.  otherwise, the constant will
have value "false".  If the operand type is dependent, store a
ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;
  a_type_ptr        type;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg != NULL && arg->next == NULL &&
                  arg->kind == (an_expr_node_kind)enk_type_operand);
  type = arg->variant.type_operand.type;
  if (is_template_dependent_type(type)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean                 result = FALSE, incomplete_class_error = FALSE;
    a_boolean                 is_list = FALSE;
    a_builtin_operation_kind  kind = expr->variant.builtin_operation.kind;
    a_symbol_ptr              sym = NULL;
    a_class_symbol_supplement_ptr
                              cssp = NULL;
    if (kind == (a_builtin_operation_kind)bok_is_trivial ||
        kind == (a_builtin_operation_kind)bok_is_standard_layout ||
        kind == (a_builtin_operation_kind)bok_is_literal_type) {
      type = skip_array_types(type);
    }  /* if */
    type = skip_typerefs(type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled &&
        (kind == (a_builtin_operation_kind)bok_is_sealed ||
         kind == (a_builtin_operation_kind)bok_is_simple_value_class ||
         kind == (a_builtin_operation_kind)bok_is_value_class)) {
      /* These operators apply to the boxed version of non-pointer value
         types. */
      if (is_boxable_type(type)) {
        type = boxed_type_for(type);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!is_immediate_class_type(type)) {
      /* Non-class types. */
      /* Note that g++ (checked in 4.5) treats scoped enums the same as
         unscoped enums. */
      switch (kind) {
        case bok_has_assign:
        case bok_has_copy:
        case bok_has_nothrow_assign:
        case bok_has_nothrow_constructor:
        case bok_has_nothrow_copy:
        case bok_has_trivial_assign:
        case bok_has_trivial_constructor:
        case bok_has_trivial_copy:
        case bok_has_trivial_destructor:
        case bok_is_pod:
        case bok_is_trivially_copyable:
        case bok_has_trivial_move_constructor:
        case bok_has_trivial_move_assign:
        case bok_has_nothrow_move_assign:
          if (microsoft_mode) {
            /* MSVC returns FALSE for all of these (which is, at least in
               some cases, weird, but there you have it). */
            result = FALSE;
          } else {
            result = TRUE;
          }  /* if */
          break;
        case bok_has_user_destructor:
        case bok_has_virtual_destructor:
        case bok_is_abstract:
        case bok_is_class:
        case bok_is_empty:
        case bok_is_polymorphic:
        case bok_is_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_has_finalizer:
        case bok_is_delegate:
        case bok_is_interface_class:
        case bok_is_ref_array:
        case bok_is_ref_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          result = FALSE;
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_is_simple_value_class:
        case bok_is_value_class:
          /* Microsoft compilers appear to use the boxed type when there is
             one (see above).  So enumerations are handled via the class
             case. */
          check_assertion(!is_immediate_enum_type(type));
          result = FALSE;
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        case bok_is_enum:
          result = is_immediate_enum_type(type);
          break;
        case bok_is_trivial:
        case bok_is_standard_layout:
          result = is_object_type(type);
          break;
        case bok_is_literal_type:
          result = is_literal_type(type);
          break;
#if MICROSOFT_EXTENSIONS_ALLOWED
        case bok_is_sealed:
          result = TRUE;
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        default:
          unexpected_condition();
      }  /* switch */
      goto result_known;
    } else if (complete_class_property) {
      /* An incomplete class type is invalid. */
      complete_type_is_needed(type);
      if (is_incomplete_type(type)) {
        incomplete_class_error = TRUE;
        goto result_known;
      } else {
        arg->variant.type_operand.definition_needed = TRUE;
        cssp = symbol_supplement_for_class(type);
      }  /* if */
    }  /* if */
    switch (kind) {
      case bok_has_assign:
      case bok_has_nothrow_assign:
        check_assertion(cssp != NULL);  /* For Coverity. */
        if (!microsoft_mode) {
          check_assertion(kind ==
                            (a_builtin_operation_kind)bok_has_nothrow_assign);
          result = cssp->has_nothrow_assign;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else {
          result = microsoft_has_assign_predicate(type, kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        break;
      case bok_has_copy:
      case bok_has_nothrow_copy:
        check_assertion(cssp != NULL);  /* For Coverity. */
        if (!microsoft_mode) {
          check_assertion(kind ==
                              (a_builtin_operation_kind)bok_has_nothrow_copy);
          result = cssp->has_nothrow_copy;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else {
          result = microsoft_has_copy_predicate(type, kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        break;
      case bok_has_nothrow_constructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        sym = cssp->constructor;
        if (sym == NULL) {
         /* __has_nothrow_constructor returns true if there is no recorded
            constructor (which implies that the default constructor is
            implicitly declared and trivial). */
          result = TRUE;
          goto result_known;
        } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          is_list = TRUE;
          sym = sym->variant.overloaded_function.symbols;
        }  /* if */
        for (; sym != NULL; sym = is_list ? sym->next : NULL) {
          if (sym->kind == (a_symbol_kind)sk_member_function) {
            a_routine_ptr  rp = sym->variant.routine.ptr;
            if (is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
              /* There may be more than one default constructor.  E.g.:
                   struct S { S(int = 0); S(short = 0); };  */
              result = (rp->compiler_generated ||
                        is_nothrow_type(skip_typerefs(rp->type)));
              if (microsoft_mode) {
                /* Microsoft compilers only consider the first declared default
                   constructor.  Since we store the constructors in reverse
                   order of declaration, continue the loop in case another
                   default constructor appears on the list. */
              } else if (!result) {
                /* If any of the default constructors may throw an exception,
                   __has_nothrow_constructor should return FALSE (in non-
                   Microsoft modes). */
                goto result_known;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* for */
        break;
      case bok_has_trivial_assign:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->assignment_by_bitwise_copy_allowed;
        break;
      case bok_has_trivial_constructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->is_POD || cssp->trivial_default_constructor != NULL;
        break;
      case bok_has_trivial_copy:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->construction_by_bitwise_copy_allowed;
        break;
      case bok_has_trivial_destructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->has_trivial_destructor;
        break;
      case bok_has_user_destructor:
        check_assertion(microsoft_mode);
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->destructor != NULL &&
                 !cssp->destructor->variant.routine.ptr->compiler_generated;
        break;
      case bok_has_virtual_destructor:
        check_assertion(cssp != NULL);  /* For Coverity. */
        /* In C++/CLI mode, ref class destructors are never virtual; however,
           because they are only ever called by Dispose(bool), which is
           virtual, they are considered virtual. */
        result = cssp->destructor != NULL &&
                 (cssp->destructor->variant.routine.ptr->is_virtual
                  if_microsoft_extensions(
                                  || cli_class_type_kind_is(type, cctk_ref)));
        break;
      case bok_is_abstract:
        result = type->variant.class_struct_union.abstract;
        break;
      case bok_is_class:
        /* In C++/CLI mode this really means: is_native_class. */
        result = is_class_or_struct(type)
                 if_microsoft_extensions(
                              && cli_class_type_kind_is(type, cctk_standard));
        break;
      case bok_is_empty:
        result = is_empty_class_type(type);
        break;
      case bok_is_enum:
        result = FALSE;
        break;
      case bok_is_pod:
        /* Note that only class types are considered by Microsoft compilers. */
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->is_POD;
        break;
      case bok_is_polymorphic:
        /* C++/CLI value classes are not polymorphic even though they can
           implement interfaces. */
        result = is_polymorphic_class_type(type)
                 if_microsoft_extensions(
                                && !cli_class_type_kind_is(type, cctk_value));
        break;
      case bok_is_union:
        result = (type->kind == (a_type_kind)tk_union);
        break;
      case bok_is_trivial:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = has_trivial_default_constructor(cssp) &&
                 cssp->has_trivial_destructor &&
                 cssp->construction_by_bitwise_copy_allowed &&
                 cssp->assignment_by_bitwise_copy_allowed;
        break;
      case bok_is_standard_layout:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->standard_layout;
        break;
      case bok_is_trivially_copyable:
        result = is_trivially_copyable_type(type);
        break;
      case bok_is_literal_type:
        result = is_literal_type(type);
        break;
      case bok_has_trivial_move_constructor:
        result = has_trivial_move_constructor(type);
        break;
      case bok_has_trivial_move_assign:
        result = has_trivial_move_assign(type);
        break;
      case bok_has_nothrow_move_assign:
        result = has_nothrow_move_assign(type);
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_has_finalizer:
        check_assertion(cssp != NULL);  /* For Coverity. */
        result = cssp->finalizer != NULL;
        break;
      case bok_is_delegate:
        if (cppcli_enabled) {
          a_type_ptr  delegate_tp = cli_class_type_for(csk_system_delegate);
          a_type_ptr  multicast_delegate_tp =
                            cli_class_type_for(csk_system_multicast_delegate);
          result = (type->variant.class_struct_union.is_delegate_class ||
                    identical_types(type, delegate_tp) ||
                    identical_types(type, multicast_delegate_tp));
        }  else {
          result = FALSE;
        }  /* if */
        break;
      case bok_is_interface_class:
        result = cli_class_type_kind_is(type, cctk_interface);
        break;
      case bok_is_ref_array:
        /* System::Array isn't technically a ref array, but it supports the
           subscript operator, and ref arrays all derive from it, so it is
           considered a ref array. */
        if (cppcli_enabled) {
          a_type_ptr  array_tp = cli_class_type_for(csk_system_array);
          result = class_type_supp(type)->is_cli_array ||
                   identical_types(type, array_tp);
        }  else {
          result = FALSE;
        }  /* if */
        break;
      case bok_is_ref_class:
        result = cli_class_type_kind_is(type, cctk_ref) && 
                 !class_type_supp(type)->is_cli_array;
        break;
      case bok_is_sealed:
        result = type->variant.class_struct_union.final;
        break;
      case bok_is_simple_value_class:
        result = is_simple_value_class_type(type);
        break;
      case bok_is_value_class:
        result = cli_class_type_kind_is(type, cctk_value);
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition();
    }  /* if */
result_known:
    if (incomplete_class_error) {
      clear_constant(constant, (a_constant_repr_kind)ck_error);
      if (pos != NULL) {
        pos_error(ec_incomplete_class_type, pos);
      }  /* if */
    } else {
      clear_constant(constant, (a_constant_repr_kind)ck_integer);
      set_integer_value(&constant->variant.integer_value,
                        (a_host_large_integer)result);
    }  /* if */
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_unary_type_trait_helper */

#if GNU_EXTENSIONS_ALLOWED

static void fold_types_compatible(an_expr_node_ptr   expr,
                                  a_constant_ptr     constant,
                                  a_boolean          maintain_expression)
/*
expr is an enk_builtin_operation node for a GNU C __builtin_types_compatible
operation.  If the operand types are nondependent, store a boolean constant in
*constant.  The boolean constant will have value "true" if the operand types
are "compatible" (ignoring top-level qualifiers); otherwise, the constant will
have value "false".  If either of the operand types is dependent, store a
ck_template_param constant in *constant.  The constant will be of the
tpck_expression variant and will point to the given expression.
If maintain_expression is TRUE, the backing expression for the returned
constant will be set as well.
*/
{
  an_expr_node_ptr  arg1 = expr->variant.builtin_operation.operands,
                    arg2 = arg1->next;
  a_type_ptr        type1, type2;

  /* eok_parens shouldn't appear here, since the construct is generated. */
  check_assertion(arg1 != NULL && arg2 != NULL && arg2->next == NULL &&
                  arg1->kind == (an_expr_node_kind)enk_type_operand &&
                  arg2->kind == (an_expr_node_kind)enk_type_operand);
  type1 = arg1->variant.type_operand.type;
  type2 = arg2->variant.type_operand.type;
  if (is_template_dependent_type(type1) ||
      is_template_dependent_type(type2)) {
    clear_constant(constant, (a_constant_repr_kind)ck_template_param);
    set_template_param_constant_kind(
                   constant, (a_template_param_constant_kind)tpck_expression);
    constant->variant.template_param.variant.expr = expr;
  } else {
    a_boolean  result = types_are_compatible_ignoring_qualifiers(type1, type2);
    clear_constant(constant, (a_constant_repr_kind)ck_integer);
    set_integer_value(&constant->variant.integer_value,
                      (a_host_large_integer)result);
    if (maintain_expression) constant->expr = expr;
  }  /* if */
  constant->type = expr->type;
}  /* fold_types_compatible */

#endif /* GNU_EXTENSIONS_ALLOWED */

void fold_builtin_operation_if_possible(an_expr_node_ptr   expr,
                                        a_constant_ptr     constant,
                                        a_boolean          maintain_expression,
                                        a_source_position  *pos,
                                        a_boolean          *not_a_constant)
/*
The given expression is a node of kind enk_builtin_operation.  If any of its
operands are template-dependent, the result is not foldable and a
ck_template_param constant (of the tpck_expression variant) is stored in
*constant (*not_a_constant is set to FALSE in such cases).  Similarly, the
result is not foldable if the operands are such that the result is not a
constant (*not_a_constant is set to TRUE in those cases).  Otherwise, an
attempt is made to fold the operation and *not_a_constant is set to FALSE.  If
the folding is successful, the result is returned through *constant.  If the
folding fails, an error constant is returned through *constant and if pos is
non-NULL diagnostics are issued at the indicated position.
If maintain_expression is TRUE, the backing expression for the returned
constant is set as well.
*/
{
  a_boolean         has_error = FALSE;
  an_expr_node_ptr  arg = expr->variant.builtin_operation.operands;

  /* Most built-in operations result in constants.  So we start with that
     assumption. */
  *not_a_constant = FALSE;
  check_assertion(expr->kind == (an_expr_node_kind)enk_builtin_operation);
  /* Check if an error was already encountered.  In that case, we silently
     produce an error constant. */
  for (; arg != NULL; arg =  arg->next) {
    if (arg->kind == (an_expr_node_kind)enk_error) {
      has_error = TRUE;
      break;
    }  /* if */
  }  /* for */
  if (has_error) {
    clear_constant(constant, (a_constant_repr_kind)ck_error);
  } else {
    switch (expr->variant.builtin_operation.kind) {
      case bok_offsetof:
        fold_offsetof(expr, constant, maintain_expression, pos,
                      not_a_constant);
        break;
#if GNU_EXTENSIONS_ALLOWED
      case bok_types_compatible:
        fold_types_compatible(expr, constant, maintain_expression);
        break;
#endif /* GNU_EXTENSIONS_ALLOWED */
      case bok_has_assign:
      case bok_has_copy:
      case bok_has_nothrow_assign:
      case bok_has_nothrow_constructor:
      case bok_has_nothrow_copy:
      case bok_has_trivial_assign:
      case bok_has_trivial_constructor:
      case bok_has_trivial_copy:
      case bok_has_trivial_destructor:
      case bok_has_user_destructor:
      case bok_has_virtual_destructor:
      case bok_is_abstract:
      case bok_is_empty:
      case bok_is_pod:
      case bok_is_polymorphic:
      case bok_is_trivial:
      case bok_is_standard_layout:
      case bok_is_trivially_copyable:
      case bok_is_literal_type:
      case bok_has_trivial_move_constructor:
      case bok_has_trivial_move_assign:
      case bok_has_nothrow_move_assign:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_has_finalizer:
      case bok_is_sealed:
      case bok_is_simple_value_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Various type trait helpers that require their single argument to be
           a complete class type. */
        fold_unary_type_trait_helper(expr, constant, maintain_expression, pos,
                                     /*complete_class_property=*/TRUE);
        break;
      case bok_is_class:
      case bok_is_enum:
      case bok_is_union:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case bok_is_delegate:
      case bok_is_interface_class:
      case bok_is_ref_array:
      case bok_is_ref_class:
      case bok_is_value_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Various type trait helpers that take a single argument. */
        fold_unary_type_trait_helper(expr, constant, maintain_expression, pos,
                                     /*complete_class_property=*/FALSE);
        break;
      case bok_is_base_of:
        fold_is_base_of(expr, constant, maintain_expression);
        break;
      case bok_is_convertible_to:
        fold_is_convertible_to(expr, constant, maintain_expression);
        break;
      case bok_is_constructible:
      case bok_is_nothrow_constructible:
        fold_is_constructible(expr, constant, maintain_expression);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
}  /* fold_builtin_operation_if_possible */

#if GNU_EXTENSIONS_ALLOWED

a_boolean fold_bit_count_operation_if_possible(a_routine_ptr     rp,
                                               an_expr_node_ptr  arg,
                                               a_constant        *result_con)
/*
rp represents a GNU builtin bit counting function which is being applied to
the given argument.  If the argument is a constant integer, set *result_con
to the result of that count and return TRUE.  Otherwise, return FALSE.

The bit counting functions are (for "unsigned int" arguments):
  ffs: index of the least-significant 1 (or zero if there is none)
  clz: number of leading zeros
  ctz: number of trailing zeros
  popcount: number of ones
  parity: number of ones modulo 2
Variants for unsigned long (e.g., ffsl) and unsigned long long (e.g., ctzll)
arguments are available in all cases.

This routine will fail to fold the operation (and hence return FALSE) if the
argument cannot be represented in a_host_large_unsigned.
*/
{
  a_boolean   success = FALSE;
  a_type_ptr  result_type;

  check_assertion(is_gnu_builtin_function(rp));
  result_type = return_type_of(rp->type);
  result_type = skip_typerefs(result_type);
  check_assertion(result_type->kind == (a_type_kind)tk_integer);
  if (is_constant_node(arg) &&
      arg->variant.constant->kind == (a_constant_repr_kind)ck_integer) {
    a_constant_ptr         cp = arg->variant.constant;
    a_boolean              err;
    a_host_large_unsigned  val = unsigned_value_of_integer_constant(cp, &err);
    if (!err) {
      a_targ_size_t  n_bits = skip_typerefs(cp->type)->size*targ_char_bit;
      a_targ_size_t  k, result = 0;
      /* Traverse the bits of the argument.  n_bits may be larger than the
         number of bits in a_host_large_unsigned but that is not a problem:
         The overflow case was avoided with the test for !err above and so
         the excess bits can be assumed to be zeros. */
      for (k = 0; k < n_bits; ++k, val >>= 1) {
        a_boolean  bit = ((val & 1) != 0);
        switch (rp->variant.builtin_function_kind) {
          case bfk_ffs:
          case bfk_ffsl:
#if LONG_LONG_ALLOWED
          case bfk_ffsll:
#endif /* LONG_LONG_ALLOWED */
            /* Index of the least significant 1-bit. */
            if (bit) {
              result = k+1;
              goto count_done;
            }  /* if */
            break;
          case bfk_clz:
          case bfk_clzl:
#if LONG_LONG_ALLOWED
          case bfk_clzll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of leading zeros. */
            result = bit ? 0 : result+1;
            break;
          case bfk_ctz:
          case bfk_ctzl:
#if LONG_LONG_ALLOWED
          case bfk_ctzll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of trailing zeros. */
            if (bit) {
              goto count_done;
            } else {
              ++result;
            }  /* if */
            break;
          case bfk_popcount:
          case bfk_popcountl:
#if LONG_LONG_ALLOWED
          case bfk_popcountll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of ones. */
            if (bit) result += 1;
            break;
          case bfk_parity:
          case bfk_parityl:
#if LONG_LONG_ALLOWED
          case bfk_parityll:
#endif /* LONG_LONG_ALLOWED */
            /* Count of ones. */
            if (bit) result = (result+1) & 1;
            break;
          default:
            unexpected_condition();
        }  /* switch */
      }  /* for */
count_done:
      set_unsigned_integer_constant(result_con, (a_host_large_unsigned)result,
                                    result_type->variant.integer.int_kind);
      success = TRUE;
    }  /* if */
  }  /* if */
  return success;
}  /* fold_bit_count_operation */

#if TARG_HAS_IEEE_FLOATING_POINT

a_boolean fold_fptest_if_possible(a_routine_ptr     rp,
                                  an_expr_node_ptr  arg,
                                  a_constant        *result_con)
/*
rp represents a GNU builtin floating-point test function (__builtin_isnan or
__builtin_isinf) which is being applied to the given argument.  If the argument
is a constant, set *result_con to the result of the test and return TRUE.
Otherwise, return FALSE.
*/
{
  a_boolean   success = FALSE, unknown_result = FALSE;
  a_type_ptr  result_type;

  check_assertion(is_gnu_builtin_function(rp));
  result_type = return_type_of(rp->type);
  result_type = skip_typerefs(result_type);
  check_assertion(result_type->kind == (a_type_kind)tk_integer);
  if (is_constant_node(arg) &&
      arg->variant.constant->kind == (a_constant_repr_kind)ck_float) {
    a_constant_ptr         cp = arg->variant.constant;
    a_host_large_unsigned  result;
    switch (rp->variant.builtin_function_kind) {
      case bfk_isnan:
        result = fp_is_nan(&cp->variant.float_value,
                           cp->type->variant.float_kind);
        break;
      case bfk_isinf:
        result = fp_is_infinity(&cp->variant.float_value,
                                cp->type->variant.float_kind);
        break;
      case bfk_isfinite:
        result = !fp_is_infinity(&cp->variant.float_value,
                                 cp->type->variant.float_kind) &&
                 !fp_is_nan(&cp->variant.float_value,
                            cp->type->variant.float_kind);
        break;
      case bfk_isnormal:
        result = fp_is_normalized(&cp->variant.float_value,
                                  cp->type->variant.float_kind,
                                  &unknown_result);
        break;
      default:
        unexpected_condition();
    }  /* switch */
    if (!unknown_result) {
      set_integer_constant(result_con, (a_host_large_integer)result,
                           result_type->variant.integer.int_kind);
      success = TRUE;
    }  /* if */
  }  /* if */
  return success;
}  /* fold_fptest_if_possible */

#endif /* TARG_HAS_IEEE_FLOATING_POINT */

a_boolean fold_pow_if_possible(a_constant_ptr  base,
                               a_constant_ptr  exp,
                               a_constant_ptr  result,
                               a_type_ptr      result_type)
/*
base and exp are floating-point constants.  If exp represents a small
nonnegative integer value, store the value of base raised to the power
indicated by exp in result and return TRUE (the final result is a
floating-point value of the given type).  Otherwise, return FALSE.
*/
{
  a_boolean   folded = FALSE, err = FALSE, mode_dep;
  a_host_large_integer
              e;
  an_internal_float_value
              b, acc, v;
  a_type_ptr  exp_type = skip_typerefs(exp->type),
              base_type = skip_typerefs(base->type);
  
  check_assertion(base->kind == (a_constant_repr_kind)ck_float &&
                  is_real_floating_type(base_type) &&
                  exp->kind == (a_constant_repr_kind)ck_float &&
                  is_real_floating_type(exp_type) &&
                  is_real_floating_type(result_type));
  /* First check whether exp represents a small integer. */
  fp_to_host_large_integer(exp_type->variant.float_kind,
                           &exp->variant.float_value, &e, &err, &mode_dep);
  if (!err && e >= 0 && e < 256) {
    /* Check whether exp represents an integer by converting e back to a
       floating-point value a testing if it remains unmodified. */
    fp_host_large_integer_to_float(exp_type->variant.float_kind, e, &v, &err);
    if (fp_compare(exp_type->variant.float_kind,
                   &exp->variant.float_value, &v, &err) == 0 && !err) {
      folded = TRUE;
    }  /* if */
  }  /* if */
  if (folded) {
    /* The actual computation will be done with "long double" precision. */
    fp_change_kind(&base->variant.float_value, base_type->variant.float_kind,
                   &b, (a_float_kind)fk_long_double,
                   &err, &mode_dep);
    if (err) folded = FALSE;
  }  /* if */
  if (folded) {
    fp_host_large_integer_to_float((a_float_kind)fk_long_double,
                                   (a_host_large_integer)1, &acc, &err);
    check_assertion(!err);
    /* The initial value b0 of b is the "base" value.  It is subsequently
       squared to compute powers b1 = b0^2, b2 = b1^2 = b0^4, etc.  Every
       time bit n is set in e (n = 0 for the least significant bit), an
       accumulator (starting with value 1) is multiplied by bn =  b0^(2^n). */
    while (e != 0) {
      if (e & 1) {
        /* Update the accumulator. */
        fp_multiply((a_float_kind)fk_long_double,
                    &b, &acc, &acc, &err, &mode_dep);
        if (err) {
          folded = FALSE;
          break;
        }  /* if */
      }  /* if */
      e = e/2;
      if (e != 0) {
        /* Compute the next value of b: b <- b*b. */
        fp_multiply((a_float_kind)fk_long_double, &b, &b, &b, &err, &mode_dep);
        if (err) {
          folded = FALSE;
          break;
        }  /* if */
      }  /* if */
    }  /* while */
    if (folded) {
      /* Store the final result with the required precision. */
      clear_constant(result, (a_constant_repr_kind)ck_float);
      result->type = result_type;
      fp_change_kind(&acc, (a_float_kind)fk_long_double,
                     &result->variant.float_value,
                     skip_typerefs(result_type)->variant.float_kind,
                     &err, &mode_dep);
      folded = !err;
    }  /* if */
  }  /* if */
  return folded;
}  /* fold_pow_if_possible */

#endif /* GNU_EXTENSIONS_ALLOWED */



/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
