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

folding.c -- Folding routines.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#include "folding.h"

/*
Determine the severity (error or warning) to be used for integer
operation overflows.
*/
#if TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define ES_INT_OVERFLOW                                               \
  (strict_ansi_mode ? strict_ansi_error_severity : es_warning)
#else /* !TARG_NO_ERROR_ON_INTEGER_OVERFLOW */
#define ES_INT_OVERFLOW es_error
#endif /* TARG_NO_ERROR_ON_INTEGER_OVERFLOW */


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
      known_bool = (con->variant.address.variant.variable->storage_class !=
                    (a_storage_class)sc_extern);
    } else if (kind == (an_address_base_kind)abk_routine) {
      known_bool = (con->variant.address.variant.routine->storage_class !=
                    (a_storage_class)sc_extern);
    }  /* if */
  } else if (con->kind == (a_constant_repr_kind)ck_template_param) {
    known_bool = FALSE;
  }  /* if */
  return known_bool;
}  /* constant_bool_value_known_at_compile_time */


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


void implicit_cast(a_constant_ptr cp,
                   a_type_ptr     new_type)
/*
Do an implicit cast of the indicated constant to the indicated new type.
No representation change is implied.  This is used for casting one
pointer type to another and casting integer constants to pointer types.
*/
{
  cp->type = new_type;
  cp->implicit_cast = TRUE;
  /* Clear the source correspondence information.  If this was a named
     constant, the new constant should no longer be
     associated with the original constant. */
  break_source_corresp(&cp->source_corresp);
}  /* implicit_cast */


static void get_integer_attributes(a_constant      *cp,
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


static void trunc_and_set_integer(an_integer_value  *result_value,
                                  a_constant        *result,
                                  a_boolean         check_overflow,
                                  an_error_code     *err_code,
                                  an_error_severity *err_severity)
/*
Truncate the integer result_value and store it in *result.  result->type
indicates the desired result type.  If check_overflow is TRUE and
*err_code indicates no previous error, check that the value fits in
the result type; if it does not, set *err_code and *err_severity to
indicate the error.  Whether or not the check is done, and whether or
not it succeeds, the value will be adjusted if necessary to ensure
that it fits.
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
  if (check_overflow && *err_code == ec_no_error) {
    if (in_range_for_integer_kind(result, result, ikind)) {
      /* The value is in the right range.  No truncation is needed. */
      goto after_truncation;
    }  /* if */
    /* The value will not fit in the destination integer type. */
    *err_code = ec_integer_overflow;
    *err_severity = ES_INT_OVERFLOW;
  }  /* if */
  /* Truncate the value to the right size. */
  make_integer_value_mask(&mask, bit_size);
  and_integer_values(&result->variant.integer_value, &mask);
  /* Sign-extend a signed result. */
  if (is_signed) {
    sign_extend_integer_value(&result->variant.integer_value, bit_size);
  }  /* if */
after_truncation:;
}  /* trunc_and_set_integer */


static void conv_integer_to_integer(a_constant        *old_constant,
				    a_constant        *new_constant,
				    a_boolean         is_implicit_cast,
				    an_error_code     *err_code,
				    an_error_severity *err_severity)
/*
Convert an integral constant of some kind (in *old_constant) to a new
integral constant in *new_constant, with type as indicated therein.  Return
*err_code and *err_severity set to indicate any error/warning detected,
or *err_code == ec_no_error if everything went fine.  If is_implicit_cast
is FALSE, suppress any warnings.  The old_constant must have kind ==
ck_integer, and generally it must have integral type, but it may be
an integer cast to a pointer type.
*/
{
  an_integer_value mask, old_value_copy;
  an_integer_kind  new_ikind, old_ikind;
  a_boolean        new_signed, old_signed;
  int              new_bit_size, old_bit_size;
  a_boolean        is_sign_change;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Copy the old value to the new value. */
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_integer); 
  check_assertion(old_constant->kind == (a_constant_repr_kind)ck_integer);
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
           implicitly convert a pointer to an integer tpe, so the old
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
  a_host_large_integer    old_value;
  a_host_large_unsigned   unsigned_old_value;
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
  if (int_constant_is_signed(old_constant)) {
    /* The source is a signed integer value. */
    old_value = value_of_integer_constant(old_constant, &err);
    if (!err) {
      fp_host_large_integer_to_float(float_kind, old_value, float_value, &err);
    }  /* if */
  } else {
    /* The source is an unsigned integer value. */
    unsigned_old_value = unsigned_value_of_integer_constant(old_constant,
                                                            &err);
    if (!err) {
      fp_host_large_unsigned_to_float(float_kind, unsigned_old_value, 
                                      float_value, &err);
    }  /* if */
  }  /* if */
#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  if (err) {
    /* Try again with larger precision by converting the integer to a
       character string then converting the string to a float value. */
    char *str = str_for_integer_constant(old_constant);
    fp_string_to_float(float_kind, str, float_value, &err);
  }  /* if */
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
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
                                  a_boolean         *depends_on_rounding_mode)
/*
Convert a float of some kind (in *old_constant) to an integer constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_rounding_mode
is returned TRUE if the result has been determined but might be different
depending on the rounding mode.
*/
{
  a_host_large_integer    int_value;
  a_host_large_unsigned   unsigned_int_value;
  an_integer_value        result_value;
  a_boolean               err, is_signed;
  a_type_ptr              float_tp = skip_typerefs(old_constant->type);
  a_float_kind            float_kind = float_tp->variant.float_kind;
  an_internal_float_value *float_value;
#if C99_IL_EXTENSIONS_SUPPORTED
  an_internal_float_value zero;

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
  if (is_signed) {
    /* Destination is a signed integer. */
    fp_to_host_large_integer(float_kind, float_value,
                             &int_value, &err, depends_on_rounding_mode);
    if (!err) set_integer_value(&result_value, int_value);
  } else {
    /* Destination is an unsigned integer. */
    fp_to_host_large_unsigned(float_kind, float_value,
                              &unsigned_int_value, &err,
                              depends_on_rounding_mode);
    if (!err) set_unsigned_integer_value(&result_value, unsigned_int_value);
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
      conv_float_string_to_integer_value(str, &result_value, is_signed, &err);
    }  /* if */
  }  /* if */
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (!err) {
    trunc_and_set_integer(&result_value, new_constant, /*check_overflow=*/TRUE,
                          err_code, err_severity);
  }  /* if */
  if (err || *err_code != ec_no_error) {
    /* Float value is too big to fit in the integer. */
    *err_code = ec_float_to_integer_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_float_to_integer */


static void conv_float_to_float(a_constant           *old_constant,
			        a_constant           *new_constant,
			        an_error_code        *err_code,
				an_error_severity    *err_severity,
                                a_boolean            *depends_on_rounding_mode)
/*
Convert a float of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.  *depends_on_rounding_mode
is returned TRUE if the result has been determined but might be different
depending on the rounding mode.
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
                           new_kind, &err, depends_on_rounding_mode);
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
                           new_kind, &err, depends_on_rounding_mode);
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
                           new_kind, &err, depends_on_rounding_mode);
            break;
          case tk_imaginary:
            /* Complex to imaginary.  Retain the imaginary part only. */
            fp_change_kind(&old_constant->variant.complex_value->imag,
                           old_kind, &new_constant->variant.float_value,
                           new_kind, &err, depends_on_rounding_mode);
            break;
          case tk_complex:
            /* Complex to complex. */
            /* This is similar to the float-float or imaginary-imaginary cases,
               but both the real and the imaginary components must change. */
            fp_change_kind(&old_constant->variant.complex_value->real,
                           old_kind,
                           &new_constant->variant.complex_value->real,
                           new_kind, &err, depends_on_rounding_mode);
            fp_change_kind(&old_constant->variant.complex_value->imag,
                           old_kind,
                           &new_constant->variant.complex_value->imag,
                           new_kind, &err, depends_on_rounding_mode);
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
                   &err, depends_on_rounding_mode);
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
        /* Use the address constant as the "base object" for a __uuidof.
           It's weird, but we need to return a non-NULL base object for this
           case, and the constant seems like the best of the possibilities. */
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
                          a_constant        *result,
                          a_boolean         check_cast_access,
                          a_boolean         is_object_pointer,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos)
/*
Fold a C++ cast of a class pointer to a base class pointer.  constant_1 is
an address of a class object.  It is converted to a pointer to the base
class indicated by bcp and the new constant is returned in *result.
Do access control on the cast if check_cast_access is TRUE.  The
pointer is known to point to an object if is_object_pointer is TRUE.
If the operation cannot be folded, *did_not_fold is returned TRUE.
If there is an error, issue it at *err_pos.  result->type need not be
set on entry.
*/
{
  a_boolean             access_okay, err;
  a_type_ptr            orig_type, curr_type, new_type;
  a_derivation_step_ptr dsp;
  a_constant            offset;
  an_integer_value      base_class_offset;
  a_base_class_ptr      base_class;

  *did_not_fold = FALSE;
  /* The code here looks like add_base_class_casts. */
  if (bcp->ambiguous) {
    /* The base class is ambiguous. */
    pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    set_error_constant(result);
  } else {
    copy_constant(constant_1, result);
    /* Loop through the classes between the derived class and the
       base class.  Check accessibility at each step and generate the
       necessary casts. */
    access_okay = TRUE;
    /* No access checking in prototype instantiations. */
    if (in_front_end && is_template_dependent_context()) {
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
          /* Keep going, and put out the error only the first time. */
          if (access_okay) {
            pos_ty_diagnostic(es_discretionary_error,
                              ec_inaccessible_base_class, err_pos,
                              base_class->type);
            access_okay = FALSE;
          }  /* if */
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
          if (con_complete_object_type(constant_1) != NULL) {
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
       original pointer. */
    new_type = make_identically_qualified_type(curr_type, orig_type);
    implicit_cast(result, make_pointer_type(new_type));
  }  /* if */
}  /* fold_base_class_cast */


static void fold_derived_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_source_position *err_pos)
/*
Fold a C++ cast of a class pointer to a derived class pointer.  constant_1 is
an address of a class object.  It is converted to point to the pointer type
indicated by result->type, and the new constant is returned in *result.
bcp points to the base class entry for the current type relative to the
desired derived type.  If there is an error, it is issued at *err_pos.
*/
{
  a_type_ptr       new_type = result->type, derived_class_type;
  a_constant       offset;
  an_integer_value base_class_offset;
  a_boolean        err;

  /* The code here looks like add_derived_class_casts. */
  derived_class_type = f_skip_typerefs(type_pointed_to(new_type));
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                  bcp->type);
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    pos_ty2_error(ec_derived_class_from_virtual_base, err_pos,
                  derived_class_type, bcp->type);
    set_error_constant(result);
  } else {
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
    implicit_cast(result, new_type);
  }  /* if */
}  /* fold_derived_class_cast */


static void conv_pointer_to_whatever(
                                    a_constant        *old_constant,
                                    a_constant        *new_constant,
                                    a_boolean         is_implicit_cast,
                                    a_boolean         fold_constant_addr_exprs,
                                    a_boolean         is_reinterpret_cast,
                                    a_boolean         *did_not_fold,
                                    a_source_position *err_pos,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity)
/*
Convert a pointer constant to a constant of type as specified by
"new_constant".  If is_implicit_cast is TRUE, the cast is implicit.
If fold_constant_addr_exprs is TRUE, fold related class casts in constant
form; if it's FALSE, do not do such folding and return *did_not_fold TRUE.
If is_reinterpret_cast is TRUE, this is a reinterpret_cast; related
class casts are treated like casts between unrelated classes.
If there is an error, either issue it immediately at *err_pos (if it
cannot be reduced to a warning in a nonconstant context), or return
*err_code and *err_severity set appropriately.  Note that this routine
is also called when the old constant is an address constant that has
previously been cast to an integral type, and so does not have pointer
type.
*/
{
  a_type_ptr       new_type = new_constant->type;
  a_type_ptr       old_type = old_constant->type;
  a_boolean        conversion_handled = FALSE, baseward_cast;
  a_base_class_ptr bcp;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
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
      fold_base_class_cast(old_constant, bcp, new_constant, is_implicit_cast,
                           /*is_object_pointer=*/FALSE, did_not_fold, err_pos);
    } else {
      /* Base --> derived.  Valid unless the cast is ambiguous or the base
         class is a virtual base of the derived class. */
      fold_derived_class_cast(old_constant, bcp, new_constant, err_pos);
    }  /* if */
    /* If the qualifiers aren't right, adjust them. */
    if (!*did_not_fold && 
        !is_error_type(new_constant->type) &&
        !identical_types(new_constant->type, new_type)) {
      implicit_cast(new_constant, new_type);
    }  /* if */
  }  /* if */
  /* Do the cast (by calling implicit_cast) unless there was an error or
     the cast has already been handled. */
  if (!conversion_handled && !*did_not_fold &&
      (*err_code == ec_no_error || *err_severity != es_error)) {
    copy_constant(old_constant, new_constant);
    implicit_cast(new_constant, new_type);
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
    class_type = constant->variant.ptr_to_member.variant.routine->
                                        source_corresp.parent.class_type;
  } else {
    /* The underlying member is a nonstatic data member. */
    class_type = constant->variant.ptr_to_member.variant.field->
                                        source_corresp.parent.class_type;
  }  /* if */
  return class_type;
}  /* pm_constant_member_class */


static void set_pm_cast_base_class(a_constant_ptr   constant, 
                                   a_type_ptr       new_type,
                                   a_base_class_ptr bcp,
                                   a_boolean        cast_to_base)
/*
constant is a pointer-to-member constant.  Cast it to new_type, which is
a pointer to member of the class indicated by bcp.  If cast_to_base is TRUE,
this cast is toward a base class; otherwise, it is toward a derived class
(in which case bcp gives the base class entry for the current class as
a base class of the derived class).
*/
{
  a_type_ptr       member_class, new_class;
  a_targ_ptrdiff_t offset;
  a_base_class_ptr casting_base_class;

  if (pm_constant_is_null(constant)) {
    /* A NULL pointer-to-member keeps a NULL casting_base_class even
       when cast to another type. */
    implicit_cast(constant, new_type);
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
#if CHECKING
      internal_error("set_pm_cast_base_class: could not find base class");
#endif /* CHECKING */
have_base_class:
      implicit_cast(constant, new_type);
    }  /* if */
    constant->variant.ptr_to_member.casting_base_class = casting_base_class;
    constant->variant.ptr_to_member.cast_to_base = cast_to_base;
  }  /* if */
}  /* set_pm_cast_base_class */


static void fold_pm_base_class_cast(a_constant        *constant_1,
                                    a_base_class      *bcp,
                                    a_constant        *result,
                                    a_source_position *err_pos)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to a member
of a base class.  constant_1 is a pointer-to-member constant.  It is converted
to a pointer-to-member for the base class indicated by bcp and the new
constant is returned in *result.  result->type on entry indicates the
desired pointer-to-member type, possibly with qualifiers.  If there is an
error, issue it at *err_pos.  Note that casts of this type always come from
explicit casts, so checking for accessibility of base classes is not necessary.
*/
{
  a_type_ptr new_type = result->type;

  /* The code here looks like add_pm_base_class_casts. */
  if (bcp->ambiguous) {
    /* The base class is ambiguous. */
    pos_ty_error(ec_ambiguous_base_class, err_pos, bcp->type);
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp) && !any_cfront_mode()) {
    /* The base class is a virtual base of the derived class, or there's a
       virtual step on the derivation path. */
    pos_ty2_error(ec_pm_virtual_base_from_derived_class, err_pos,
                  pm_class_type(constant_1->type), bcp->type);
    set_error_constant(result);
  } else {
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/TRUE);
  }  /* if */
}  /* fold_pm_base_class_cast */


static void fold_pm_derived_class_cast(a_constant        *constant_1,
                                       a_base_class      *bcp,
                                       a_constant        *result,
                                       a_boolean         check_cast_access,
                                       a_source_position *err_pos)
/*
Fold a C++ cast of a pointer to a member of a class to pointer to member
of a derived class.  constant_1 is a pointer-to-member constant.  It is
converted to a pointer-to-member for the derived class (given by
result->type) and the new constant is returned in *result.  Do access
control on the cast if check_cast_access is TRUE.  bcp points to the base
class entry for the current type relative to the desired derived type.
If there is an error, it is issued at *err_pos.
*/
{
  a_type_ptr            new_type = result->type, curr_type;
  a_type_ptr            derived_class_type;
  a_derivation_step_ptr dsp;
  a_base_class_ptr      base_class;

  /* The code here looks like add_pm_derived_class_casts. */
  derived_class_type = pm_class_type(new_type);
  if (bcp->ambiguous) {
    /* The cast is ambiguous. */
    pos_ty2_error(ec_ambiguous_derived_class, err_pos, derived_class_type,
                  bcp->type);
    set_error_constant(result);
  } else if (any_virtual_steps_in_derivation(bcp)) {
    /* The base class is a virtual base of the derived class. */
    pos_ty2_error(ec_pm_derived_class_from_virtual_base, err_pos,
                  derived_class_type, bcp->type);
    set_error_constant(result);
  } else {
    /* No access checking in prototype instantiations. */
    if (in_front_end && is_template_dependent_context()) {
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
          pos_ty_diagnostic(es_discretionary_error, ec_inaccessible_base_class,
                            err_pos, base_class->type);
          break;
        }  /* if */
        curr_type = base_class->type;
      }  /* for */
    }  /* if */
    copy_constant(constant_1, result);
    /* Set the constant to indicate the cast. */
    set_pm_cast_base_class(result, new_type, bcp, /*cast_to_base=*/FALSE);
  }  /* if */
}  /* fold_pm_derived_class_cast */


static void conv_ptr_to_member_to_ptr_to_member(
                                         a_constant        *old_constant,
                                         a_constant        *new_constant,
                                         a_boolean         is_implicit_cast,
                                         a_source_position *err_pos,
                                         an_error_code     *err_code,
                                         an_error_severity *err_severity)
/*
Convert a pointer-to-member constant to a pointer-to-member constant of
a different type.  old_constant is the original constant.  new_constant->type
indicates the desired new type.  The converted constant is put into
*new_constant.  This is an implicit cast if is_implicit_cast is TRUE.
Note that this should not be called to implement a reinterpret_cast operation
since such casts on pointer-to-member types are not "constant operations".
*/
{
  a_type_ptr       new_type = new_constant->type, new_class;
  a_type_ptr       old_type = old_constant->type, old_class;
  a_base_class_ptr bcp;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* Basically, all that's needed is to change the type of the constant and
     set implicit_cast.  However, one must also check for an ambiguous
     cast and (when the cast is implicit) for accessibility. */
  old_class = pm_class_type(old_type);
  new_class = pm_class_type(new_type);
  /* Using types_are_compatible so that A<x> and A<error> are considered
     the same type. */
  if (types_are_compatible(old_class, new_class)) {
    /* The classes are the same, so no error check is needed. */
    /* The fact that the class types are the same does not mean the
       pointer-to-member types are the same; the member type may be
       changing. */
    copy_constant(old_constant, new_constant);
    implicit_cast(new_constant, new_type);
  } else if ((bcp = find_base_class_of(old_class, new_class)) != NULL) {
    /* Derived --> base (allowed only as an explicit cast).  Valid unless
       the cast is ambiguous. */
    fold_pm_base_class_cast(old_constant, bcp, new_constant, err_pos);
  } else if ((bcp = find_base_class_of(new_class, old_class)) != NULL) {
    /* Base --> derived (allowed as an implicit or explicit cast).  Valid
       unless the cast is ambiguous, the base class is inaccessible (if
       the cast is implicit), or the base class is a virtual base of the
       derived class. */
    fold_pm_derived_class_cast(old_constant, bcp, new_constant,
                               is_implicit_cast, err_pos);
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

  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (is_implicit_cast) {
    if (cmplit_integer_constant(old_constant, (a_host_large_integer)0) != 0) {
      /* Any value other than zero (NULL).  Issue a warning. */
      *err_code = ec_non_zero_int_conv_to_pointer;
      *err_severity = es_warning;
    }  /* if */
  }  /* if */
  /* Make a new constant that is the old constant implicitly cast to the
     pointer type. */
  copy_constant(old_constant, new_constant);
  implicit_cast(new_constant, new_type);
  /* Mask the integer down to the size of pointer. */
#if CHECKING
  if (new_constant->kind != (a_constant_repr_kind)ck_integer) {
    internal_error("conv_integer_to_pointer: not integer constant");
  }  /* if */
#endif /* CHECKING */
  make_integer_value_mask(&mask,
                          (int)(skip_typerefs(new_type)->size*targ_char_bit));
  and_integer_values(&new_constant->variant.integer_value, &mask);
}  /* conv_integer_to_pointer */


#if !CHECKING
/*ARGSUSED*/ /* <-- old_constant is not used if CHECKING is FALSE. */
#endif /* !CHECKING */
static void conv_integer_to_ptr_to_member(a_constant *old_constant,
                                          a_constant *new_constant)
/*
Convert an integer constant to a pointer to member.
*/
{
  a_type_ptr new_type = new_constant->type;
  a_boolean  is_function_ptr;

#if CHECKING
  /* The only valid constant is zero. */
  if (old_constant->kind != (a_constant_repr_kind)ck_integer ||
      old_constant->implicit_cast ||
      !is_zero_constant(old_constant)) {
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
  implicit_cast(new_constant, new_type);
}  /* conv_integer_to_ptr_to_member */


static void issue_folding_diagnostic(an_error_code     err_code,
                                     an_error_severity err_severity,
                                     a_boolean         constant_context,
                                     a_boolean         evaluated_context,
                                     a_boolean         *did_not_fold,
                                     a_source_position *err_pos,
                                     a_constant        *result)
/*
An error or warning has been detected in a folding operation; err_code
and err_severity indicate what it is.  If not in a constant_context, reduce
an error to a warning and set *did_not_fold to TRUE.  If not in an
evaluated_context, throw away the error and set *did_not_fold to TRUE.
Issue the diagnostic at source position *err_pos.  Set *result to the
proper result (often, an error constant).  
*/
{
  if (!evaluated_context) {
    /* Discard a warning or error in a not-evaluated context. */
    err_severity = es_none;
    *did_not_fold = TRUE;
  } else if (!constant_context && err_severity == es_error) {
    /* Reduce an error to a warning in a nonconstant context. */
    err_severity = es_warning;
    *did_not_fold = TRUE;
  }  /* if */
  if (err_severity == es_error) {
    pos_error(err_code, err_pos);
    set_error_constant(result);
  } else if (err_severity == es_warning) {
    pos_warning(err_code, err_pos);
  }  /* if */
}  /* issue_folding_diagnostic */


static a_boolean related_ptr_to_members(a_type_ptr  type_1,
                                        a_type_ptr  type_2)
/*
Return TRUE if the class types into which the given pointer-to-member types
point are related by inheritance.
*/
{
  a_type_ptr  class_1 = pm_class_type(type_1);
  a_type_ptr  class_2 = pm_class_type(type_2);

  return find_base_class_of(class_1, class_2) != NULL ||
         find_base_class_of(class_2, class_1) != NULL;
}  /* related_ptr_to_members */


#if !RECORD_CONSTANT_EXPRESSIONS_IN_IL
/*ARGSUSED*/ /* <-- maintain_expression is unused in that case. */
#endif /* !RECORD_CONSTANT_EXPRESSIONS_IN_IL */
void type_change_constant(a_constant        *constant,
			  a_type_ptr        new_type,
			  a_boolean         is_implicit_cast,
                          a_boolean         constant_context,
                          a_boolean         evaluated_context,
                          a_boolean         fold_constant_addr_exprs,
                          a_boolean         is_reinterpret_cast,
                          a_boolean         maintain_expression,
                          a_boolean         *did_not_fold,
                          a_source_position *err_pos)
/*
Convert the indicated constant to "new_type".  Issue errors or warnings
using the position *err_pos.  If is_implicit_cast is TRUE, this is an
implicit cast; more warnings are given.  If constant_context is FALSE, this
operation is being evaluated as part of a nonconstant expression, so
any error is reduced to a warning and *did_not_fold is returned TRUE.
If evaluated_context is FALSE, this operation is being done in a
not-evaluated context (e.g., a sizeof or a dead branch of a "?" operator),
so any error is thrown away and *did_not_fold is returned TRUE.
*did_not_fold is also returned TRUE in other cases where the folding
cannot be done.  fold_constant_addr_exprs is TRUE if constant address
expressions should be folded (e.g., base class casts); if it is FALSE,
*did_not_fold is set instead for those.  If is_reinterpret_cast is TRUE,
this cast is a reinterpret_cast; related-class casts are treated like
casts between unrelated classes.  If maintain_expression is TRUE,
and RECORD_CONSTANT_EXPRESSIONS_IN_IL is TRUE, any expression attached
to the constant is maintained, by adding a cast if necessary.
*/
{
  a_type_ptr        constant_type, new_type_with_typedefs;
  a_constant        new_constant;
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_rounding_mode = FALSE;

  db_enter(5, "type_change_constant");
  *did_not_fold = FALSE;
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
  if (identical_types(constant_type, new_type)) {
    /* The current and new types are the same, so no change is required. */
    copy_constant(constant, &new_constant);
    /* Put in the actual type wanted, as it may have typedefs. */
    new_constant.type = new_type_with_typedefs;
    goto exit;
  }  /* if */
  if (!C_mode() &&
      (constant->kind == (a_constant_repr_kind)ck_template_param ||
       (in_front_end && is_template_dependent_context() &&
        is_template_dependent_type(new_type)))) {
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
  if (vla_enabled && !is_implicit_cast &&
      is_directly_variably_modified_type(new_type)) {
    /* A cast to a variably-modified type where the variable bound appears
       in the cast (an opposed to inside a typedef declared elsewhere) is
       a non-constant operation and cannot be folded. */
    *did_not_fold = TRUE;
    goto exit;
  }  /* if */
  if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* Any case where the constant is represented as an address should be
       converted by setting the implicit_cast flag.  This test has to be
       early -- like this -- to catch ((unsigned)((int)&x)).  That case
       would have constant_type->kind == tk_integer and new_type->kind
       == tk_integer, and so would not look like it involves pointers. */
    conv_pointer_to_whatever(constant, &new_constant, is_implicit_cast,
                             fold_constant_addr_exprs, is_reinterpret_cast,
                             did_not_fold, err_pos, &err_code, &err_severity);
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
        case tk_pointer:
          /* Converting integer to pointer. */
          conv_integer_to_pointer(constant, &new_constant, is_implicit_cast,
                                  &err_code, &err_severity);
          break;
        case tk_ptr_to_member:
          /* Converting integer to pointer-to-member. */
          conv_integer_to_ptr_to_member(constant, &new_constant);
          break;
        default:
          unexpected_condition_str(
                                  "type_change_constant: integer to bad type");
      }  /* switch */
      break;

    case tk_float:
      /* Converting from float. */
      switch (new_type->kind) {
        case tk_integer:
          /* Converting float to integer. */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_rounding_mode);
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
                              &depends_on_rounding_mode);
          break;
        default:
          unexpected_condition_str("type_change_constant: float to bad type");
      }  /* switch */
      break;

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting imaginary to integer (produces zero). */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_rounding_mode);
          break;
        case tk_float:
          /* Converting imaginary to float (produces zero). */
        case tk_imaginary:
          /* Converting imaginary to imaginary. */
        case tk_complex:
          /* Converting imaginary to complex. */
          conv_float_to_float(constant, &new_constant,
                              &err_code, &err_severity,
                              &depends_on_rounding_mode);
          break;
        default:
          unexpected_condition_str(
                                "type_change_constant: imaginary to bad type");
      }  /* switch */
      break;

    case tk_complex:
      switch (new_type->kind) {
        case tk_integer:
          /* Converting complex to integer. */
          conv_float_to_integer(constant, &new_constant,
                                &err_code, &err_severity,
                                &depends_on_rounding_mode);
          break;
        case tk_float:
          /* Converting complex to float. */
        case tk_imaginary:
          /* Converting complex to imaginary. */
        case tk_complex:
          /* Converting complex to complex. */
          conv_float_to_float(constant, &new_constant,
                              &err_code, &err_severity,
                              &depends_on_rounding_mode);
          break;
        default:
          unexpected_condition_str(
                                  "type_change_constant: complex to bad type");
      }  /* switch */
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

    case tk_pointer:
      /* Converting from pointer. */
      conv_pointer_to_whatever(constant, &new_constant, is_implicit_cast,
                               fold_constant_addr_exprs, is_reinterpret_cast,
                               did_not_fold, err_pos,
                               &err_code, &err_severity);
      break;

    case tk_ptr_to_member:
      /* Converting from pointer-to-member to pointer-to-member. */
      if (!is_reinterpret_cast ||
          /* In Microsoft mode a reinterpret-like cast is OK, if only the
             member type is reinterpreted; not the class type. */
          (microsoft_mode &&
           related_ptr_to_members(constant_type, new_type))) {
        conv_ptr_to_member_to_ptr_to_member(constant, &new_constant,
                                            is_implicit_cast, err_pos,
                                            &err_code, &err_severity);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
      break;

    case tk_error:
      /* The old constant is an error constant. */
      /* Change the type of the new constant back to the original type of the
	 error constant, i.e., error. */
      new_constant.type = constant->type;
      break;

    default:
      unexpected_condition_str("type_change_constant: from bad type");
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
  if (microsoft_bugs && !C_mode() && !is_implicit_cast) {
    /* Microsoft C++ mode: any explicit cast makes a constant not a null
       pointer constant.  In particular, (int)0 is not a null pointer
       constant. */
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
                             err_pos, &new_constant);
    if (err_severity == es_error) depends_on_rounding_mode = FALSE;
  }  /* if */
  if (depends_on_rounding_mode && !constant_context) {
    /* In a non-constant context, leave an operation to be done at runtime
       if its result depends on the floating-point rounding mode. */
    *did_not_fold = TRUE;
  }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  if (maintain_expression && constant->expr != NULL &&
      (int)err_severity < (int)es_error && !*did_not_fold) {
    /* Transfer the source expression from the old constant to the new one,
       adding a cast if there was a type change.  Note that the cast added
       is always an eok_cast, so this shouldn't be used if there's the
       possibility that a base-class cast or the like is involved. */
    if (constant->type == new_constant.type) {
      new_constant.expr = constant->expr;
    } else {
      an_expr_node_ptr cast_node =
                            make_operator_node((an_expr_operator_kind)eok_cast,
                                               new_constant.type,
                                               constant->expr);
      cast_node->variant.operation.compiler_generated = is_implicit_cast;
      cast_node->variant.operation.is_reinterpret_cast = is_reinterpret_cast;
      new_constant.expr = cast_node;
    }  /* if */
  } else {
    new_constant.expr = NULL;
  }  /* if */
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  /* Return the new constant value. */
  copy_constant(&new_constant, constant);
  db_exit();
}  /* type_change_constant */


a_boolean is_zero_constant(a_constant *constant)
/*
Return TRUE if the constant is an integer or floating zero.
*/
{
  a_boolean is_zero = FALSE;
  a_float_kind float_kind;

  if (constant->kind == (a_constant_repr_kind)ck_integer &&
      !constant->implicit_cast) {
    is_zero = (cmplit_integer_constant(constant,
                                       (a_host_large_integer)0) == 0);
  } else if (constant->kind == (a_constant_repr_kind)ck_float
#if C99_IL_EXTENSIONS_SUPPORTED
             || constant->kind == (a_constant_repr_kind)ck_imaginary
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
                                                                    ) {
    float_kind = skip_typerefs(constant->type)->variant.float_kind;
    is_zero = fp_is_zero_constant(float_kind,
                                  &constant->variant.float_value);
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (constant->kind == (a_constant_repr_kind)ck_complex) {
    float_kind = skip_typerefs(constant->type)->variant.float_kind;
    is_zero = fp_is_zero_constant(float_kind,
                                  &constant->variant.complex_value->real) &&
              fp_is_zero_constant(float_kind,
                                  &constant->variant.complex_value->imag);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
   }  /* if */
  return is_zero;
}  /* is_zero_constant */


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
    /* Zero integral or floating constant. */
    is_false = TRUE;
  } else if (constant->kind == (a_constant_repr_kind)ck_integer &&
             constant->implicit_cast) {
    /* Check for NULL pointer constant (0 cast to a pointer type). */
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
    /* A null pointer constant has the value zero, perhaps cast to "void *"
       in C.  Only certain kinds of casts are allowed. */
    if (!constant->null_pointer_constant_ruled_out &&
        cmplit_integer_constant(constant, (a_host_large_integer)0) == 0) {
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
  if (debug_level >= 5) {
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
                        err_code, err_severity);

#if DEBUG
  db_unary_operation("i-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_inegate */


static void do_fnegate(a_constant        *constant,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
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
            &result->variant.float_value, &err);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("f-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fnegate */

#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xnegate(a_constant        *constant,
                       a_constant        *result,
                       an_error_code     *err_code,
                       an_error_severity *err_severity)
/*
Do the negate operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE;
  a_type_ptr   constant_type = skip_typerefs(constant->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_negate(float_kind,
            &constant->variant.complex_value->real,
            &result->variant.complex_value->real,
            &err);
  accum_err |= err;
  fp_negate(float_kind,
            &constant->variant.complex_value->imag,
            &result->variant.complex_value->imag,
            &err);
  accum_err |= err;
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
*err_pos is used as the position for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;

  db_enter(5, "unary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  err_code = ec_no_error;
  err_severity = es_warning;
  if (is_error_constant(constant)) {
    /* The constant is an error constant; set the result to an error
       constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant->kind == (a_constant_repr_kind)ck_template_param ||
              (is_template_dependent_context() &&
               is_template_dependent_type(result_type)))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (is_addr_constant_cast_to_integral_type(constant)) {
      /* An address constant cast to an integral type is a link-time
         constant, not a compile-time constant.  We cannot do operations
         on it. */
      *did_not_fold = TRUE;
    } else {
      switch (op) {
        case eok_fnegate:
          do_fnegate(constant, result, &err_code, &err_severity);
          break;
        case eok_inegate:
          do_inegate(constant, result, &err_code, &err_severity);
          break;
#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xnegate:
          do_xnegate(constant, result, &err_code, &err_severity);
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case eok_unary_plus:
          copy_constant(constant, result);
          break;
        case eok_complement:
          do_complement(constant, result, &err_code, &err_severity);
          break;
        case eok_not:
          do_not(constant, result, did_not_fold);
          break;
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
                               err_pos, result);
    }  /* if */
    /* If the source constant was formed using operations that are not allowed
       in forming a null pointer constant, the result cannot be used as
       a null pointer constant. */
    result->null_pointer_constant_ruled_out =
                          constant->null_pointer_constant_ruled_out ||
                          constant->kind != (a_constant_repr_kind)ck_integer ||
                          constant->implicit_cast;
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
  if (debug_level >= 5) {
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

  check_assertion_str(shift_count_constant->kind ==
                                              (a_constant_repr_kind)ck_integer,
                      "check_shift_count: shift count not ck_integer");
  /* Determine the size of the operand being shifted. */
  operand_type = skip_typerefs(operand_type);
#if CHECKING
  if (operand_type->kind != (a_type_kind)tk_integer) {
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
  a_boolean        is_signed, err;
  int              value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    *err_severity = es_error;
  } else {
    result_value = constant_1->variant.integer_value;
    value_2 = (int)value_of_integer_constant(constant_2, &err);
    /* No need to check err because check_shift_count has already
       established that the shift count is reasonable. */
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
      shift_right_integer_value(&result_value, value_2, is_signed,
                               /*sign_extend=*/targ_right_shift_is_arithmetic);
    } else {
      /* Shift left. */
      shift_left_integer_value(&result_value, value_2, &err);
    }  /* if */
    trunc_and_set_integer(&result_value, result, /*check_overflow=*/FALSE,
                          err_code, err_severity);
  }  /* if */
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
    case eok_ieq:  result_value = (cmp == 0); break;
    case eok_ine:  result_value = (cmp != 0); break;
    case eok_igt:  result_value = (cmp >  0); break;
    case eok_ilt:  result_value = (cmp <  0); break;
    case eok_ige:  result_value = (cmp >= 0); break;
    case eok_ile:  result_value = (cmp <= 0); break;
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
                    a_boolean         *depends_on_rounding_mode)
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
         depends_on_rounding_mode);
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
                         a_boolean         *depends_on_rounding_mode)
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
              depends_on_rounding_mode);
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
                         a_boolean         *depends_on_rounding_mode)
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
              depends_on_rounding_mode);
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
                       a_boolean         *depends_on_rounding_mode)
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
              depends_on_rounding_mode);
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
                        a_constant            *result)
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
   if (op == (an_expr_operator_kind)eok_fne) {
     /* If two values are unordered, they are unequal.  This is needed for
        NaN != NaN. */
     result_value = 1;
   } else {
     result_value = 0;
   }  /* if */
  } else {
    switch (op) {
      case eok_feq:  result_value = (cmp == 0); break;
      case eok_fne:  result_value = (cmp != 0); break;
      case eok_fgt:  result_value = (cmp >  0); break;
      case eok_flt:  result_value = (cmp <  0); break;
      case eok_fge:  result_value = (cmp >= 0); break;
      case eok_fle:  result_value = (cmp <= 0); break;
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

#if C99_IL_EXTENSIONS_SUPPORTED

static void do_xadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity,
                    a_boolean         *depends_on_rounding_mode)
/*
Do the addition operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_rounding;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_add(float_kind,
         &constant_1->variant.complex_value->real,
         &constant_2->variant.complex_value->real,
         &result->variant.complex_value->real, &err,
         &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode = depends_on_rounding;
  fp_add(float_kind,
         &constant_1->variant.complex_value->imag,
         &constant_2->variant.complex_value->imag,
         &result->variant.complex_value->imag, &err,
         &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
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
                         a_boolean         *depends_on_rounding_mode)
/*
Do the subtraction operation on all types of complex.
*/
{
  a_boolean    err, accum_err = FALSE, depends_on_rounding;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_complex);
  fp_subtract(float_kind,
              &constant_1->variant.complex_value->real,
              &constant_2->variant.complex_value->real,
              &result->variant.complex_value->real, &err,
              &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode = depends_on_rounding;
  fp_subtract(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &result->variant.complex_value->imag, &err,
              &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
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
                         a_boolean         *depends_on_rounding_mode)
/*
Do the multiplication operation on all types of complex.
*/
{
  a_boolean                err, accum_err = FALSE, depends_on_rounding;
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
              &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode = depends_on_rounding;
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &temp_value, &err,
              &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
  fp_subtract(float_kind, &result->variant.complex_value->real, &temp_value,
              &result->variant.complex_value->real,
              &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
  /* Compute imaginary part of the result. */
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->real,
              &constant_2->variant.complex_value->imag,
              &result->variant.complex_value->imag, &err,
              &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
  fp_multiply(float_kind,
              &constant_1->variant.complex_value->imag,
              &constant_2->variant.complex_value->real,
              &temp_value, &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
  fp_add(float_kind, &result->variant.complex_value->imag, &temp_value,
         &result->variant.complex_value->imag,
         &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
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
                       a_boolean         *depends_on_rounding_mode)
/*
Do the division operation on all types of complex.
*/
{
  a_boolean                err, accum_err = FALSE, depends_on_rounding;
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
              &quad_norm, &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode = depends_on_rounding;
  fp_multiply(float_kind,
              &constant_2->variant.complex_value->imag,
              &constant_2->variant.complex_value->imag,
              &temp_value, &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
  fp_add(float_kind, &quad_norm, &temp_value, &quad_norm,
         &err, &depends_on_rounding);
  accum_err |= err;
  *depends_on_rounding_mode |= depends_on_rounding;
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
                &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->imag,
                &constant_2->variant.complex_value->imag,
                &temp_value, &err,
                &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_add(float_kind, &result->variant.complex_value->real, &temp_value,
           &result->variant.complex_value->real,
           &err, &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_divide(float_kind, &result->variant.complex_value->real, &quad_norm,
              &result->variant.complex_value->real,
              &err, &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    /* Compute imaginary part of the result. */
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->real,
                &constant_2->variant.complex_value->imag,
                &result->variant.complex_value->imag, &err,
                &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_multiply(float_kind,
                &constant_1->variant.complex_value->imag,
                &constant_2->variant.complex_value->real,
                &temp_value, &err, &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_subtract(float_kind, &temp_value, &result->variant.complex_value->imag,
                &result->variant.complex_value->imag,
                &err, &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
    fp_divide(float_kind, &result->variant.complex_value->imag, &quad_norm,
              &result->variant.complex_value->imag,
              &err, &depends_on_rounding);
    accum_err |= err;
    *depends_on_rounding_mode |= depends_on_rounding;
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
  if (op == (an_expr_operator_kind)eok_xeq) {
    result_value = !result_value;
  } else {
    check_assertion(op == (an_expr_operator_kind)eok_xne);
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
                         a_boolean         *depends_on_rounding_mode)
/*
Do the multiplication operation on two imaginary numbers (any precision).
*/
{
  a_boolean    err, accum_err = FALSE;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err,
              depends_on_rounding_mode);
  accum_err |= err;
  fp_negate(float_kind, &result->variant.float_value,
            &result->variant.float_value, &err);
  accum_err |= err;
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
                       a_boolean         *depends_on_rounding_mode)
/*
Do the division of a real number by an imaginary number (any precision).
*/
{
  a_boolean    err, accum_err = FALSE;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

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
              depends_on_rounding_mode);
    accum_err |= err;
    fp_negate(float_kind, &result->variant.float_value,
              &result->variant.float_value, &err);
    accum_err |= err;
    if (accum_err) {
      *err_code = ec_bad_complex_operation_result;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

#if DEBUG
  db_binary_operation("j/", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_jdivide */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

a_boolean valid_address_constant(a_constant *constant,
                                 a_boolean  *just_past_end)
/*
Return TRUE if the given address constant is valid.  Specifically, check that
the offset in it falls within the base object.  This is used for subscript
checking.  Return *just_past_end TRUE if the offset is just past the end of
the object.
*/
{
  a_boolean      valid;
  a_targ_size_t  object_size = 0;
  a_type_ptr     tp;
  a_constant_ptr cp;

  *just_past_end = FALSE;
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
        /* Ignore incomplete arrays. */
        if (tp->size != 0) {
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
         subscript value is flagged later as an error by using_lvalue
         (it calls this routine again). */
      valid =
           (constant->variant.address.offset <= (a_targ_ptrdiff_t)object_size);
      *just_past_end =
           (constant->variant.address.offset == (a_targ_ptrdiff_t)object_size);
    } else {
      /* Don't know what the size is, so assume the offset is valid. */
      valid = TRUE;
    }  /* if */
  }  /* if */

  return valid;
}  /* valid_address_constant */


static void do_padd(a_constant            *constant_1,
                    an_expr_operator_kind op,
		    a_constant            *constant_2,
		    a_constant            *result,
		    an_error_code         *err_code,
		    an_error_severity     *err_severity)
/*
Do addition or subtraction on one pointer (constant_1) and one integer
(constant_2).  op indicates whether the source form was "+"
(eok_padd), "[]" (eok_padd_subsc), or "-" (eok_psubtract).
Note that the integer can be of any type, specifically unsigned.
Also used to add or subtract a constant from an address constant
that has been cast to an integral type, as in "int i = (int)&j + 1;";
in that case, the operator is eok_iadd or eok_isubtract.
*err_code and *err_severity are set to indicate any error/warning
detected, or *err_code == ec_no_error if everything went fine.
*/
{
  a_targ_size_t    size;
  an_integer_value op2;
  a_constant       offset;
  a_boolean        err, offset_is_signed, op2_is_signed, just_past_end;
  a_boolean        integer_case = FALSE;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  if (op == (an_expr_operator_kind)eok_iadd ||
      op == (an_expr_operator_kind)eok_isubtract) {
    /* For the (int)address +- constant case, the size (scaling) is 1. */
    integer_case = TRUE;
    size = 1;
  } else {
    /* Get the size of the thing pointed to. */
    a_type_ptr  object_type =
                           f_skip_typerefs(type_pointed_to(constant_1->type));
    if (gcc_mode && (is_void_type(object_type) ||
                     is_function_type(object_type))) {
      size = 1;
    } else {
      size = object_type->size;
    }  /* if */
    check_assertion_str(size != 0, "do_padd: size is zero");
  }  /* if */
  /* Multiply the increment constant by the size. */
  set_unsigned_integer_value(&op2, size);
  op2_is_signed = int_constant_is_signed(constant_2);
  multiply_integer_values(&op2, &constant_2->variant.integer_value,
                          op2_is_signed, &err);
  if (!err) {
    /* Get the offset from the first constant. */
    get_pointer_offset(constant_1, &offset);
    /* When dealing with an address cast to an integral type, treat the
       offset as having the signedness of the type cast to. */
    offset_is_signed = integer_case ? int_constant_is_signed(constant_1) :
                                      int_constant_is_signed(&offset);
    /* Add/subtract the increment to/from the original offset. */
    if (op == (an_expr_operator_kind)eok_psubtract ||
        op == (an_expr_operator_kind)eok_isubtract) {
      subtract_mixed_signed_integer_values(&offset.variant.integer_value,
                                           offset_is_signed,
                                           &op2, op2_is_signed, &err);
    } else {
      add_mixed_signed_integer_values(&offset.variant.integer_value,
                                      offset_is_signed,
                                      &op2, op2_is_signed, &err);
    }  /* if */
    /* If this was an unsigned integer operation, overflow is ignored. */
    if (integer_case && !offset_is_signed) err = FALSE;
  }  /* if */
  if (!err) {
    /* Build the result pointer constant. */
    copy_constant(constant_1, result);
    set_pointer_offset(result, &offset, &err);
  }  /* if */
  if (err) {
    /* Some folding error. */
    *err_code = ec_integer_overflow;
    *err_severity = es_error;
  } else {
    /* Check that the offset lies within the base object. */
    if (!valid_address_constant(result, &just_past_end)) {
      /* Use a different error message for cases where the original pointer
         addition was coded in [] form. */
      if (op == (an_expr_operator_kind)eok_padd_subsc) {
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
if everything went fine.
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
     cannot be folded. */
  if (base_object(constant_1) != base_object(constant_2)) {
    *did_not_fold = TRUE;
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
         not incomplete, so the size is not zero. */
      a_targ_size_t  object_size;
      object_type = type_pointed_to(constant_1->type);
      object_type = skip_typerefs(object_type);
      if (gcc_mode && (is_void_type(object_type) ||
                       is_function_type(object_type))) {
        object_size = 1;
      } else {
        object_size = object_type->size;
      }  /* if */
      check_assertion_str(object_size != 0,
                          "do_pdiff: size of object pointed to is zero");
      set_unsigned_integer_value(&size_intval, object_size);
      /* Note that we treat &difference as signed here even if it was unsigned
         above, since the difference is defined to be signed. */
      divide_integer_values(&difference, &size_intval,
                            /*is_signed=*/TRUE, &err);
    }  /* if */
    if (!err) {
      trunc_and_set_integer(&difference, result, /*check_overflow=*/TRUE,
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
      case eok_peq:  result_value = (cmp == 0); break;
      case eok_pne:  result_value = (cmp != 0); break;
      case eok_pgt:  result_value = (cmp >  0); break;
      case eok_plt:  result_value = (cmp <  0); break;
      case eok_pge:  result_value = (cmp >= 0); break;
      case eok_ple:  result_value = (cmp <= 0); break;
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
  if (op == (an_expr_operator_kind)eok_pmne) result_value = !result_value;
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


void binary_operation(an_expr_operator_kind op,
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
the constant is a template parameter constant).  *err_pos is used
as the position for any diagnostics issued.
*/
{
  an_error_code     err_code;
  an_error_severity err_severity;
  a_boolean         depends_on_rounding_mode = FALSE;


  db_enter(5, "binary_operation");

  *did_not_fold = FALSE;
  *template_constant = FALSE;
  err_code = ec_no_error;
  err_severity = es_warning;

  if (is_error_constant(constant_1) || is_error_constant(constant_2)) {
    /* One and/or the other of the constants is an error constant; set the
       result to an error constant and return. */
    set_error_constant(result);
  } else if (!C_mode() &&
             (constant_1->kind == (a_constant_repr_kind)ck_template_param ||
              constant_2->kind == (a_constant_repr_kind)ck_template_param ||
              (is_template_dependent_context() &&
               is_template_dependent_type(result_type)))) {
    /* An operation on a template parameter constant cannot be folded. */
    *did_not_fold = TRUE;
    *template_constant = TRUE;
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
      if ((op == (an_expr_operator_kind)eok_iadd ||
           op == (an_expr_operator_kind)eok_isubtract) &&
          constant_2->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_2->type)) {
          internal_error("binary_operation: address constant +- non-integer");
        }  /* if */
#endif /* CHECKING */
        do_padd(constant_1, op, constant_2, result, &err_code,
                &err_severity);
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
      if (op == (an_expr_operator_kind)eok_iadd &&
          constant_1->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_or_enum_type(constant_1->type)) {
          internal_error("binary_operation: non-integer + address constant");
        }  /* if */
#endif /* CHECKING */
        /* Note that we reverse the operands in the call so that the address
           constant is first. */
        do_padd(constant_2, op, constant_1, result, &err_code,
                &err_severity);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else {
      switch (op) {
        case eok_iadd:
          do_iadd(constant_1, constant_2, result, &err_code, &err_severity);
          break;
        case eok_isubtract:
          do_isubtract(constant_1, constant_2, result, &err_code,
                       &err_severity);
          break;
        case eok_imultiply:
          do_imultiply(constant_1, constant_2, result, &err_code,
                       &err_severity);
          break;
        case eok_remainder:
          do_remainder(constant_1, constant_2, result, &err_code,
                       &err_severity);
          break;
        case eok_idivide:
          do_idivide(constant_1, constant_2, result, &err_code, &err_severity);
          break;
        case eok_shiftl:
          do_shiftl(constant_1, constant_2, result, &err_code, &err_severity);
          break;
        case eok_shiftr:
          do_shiftr(constant_1, constant_2, result, &err_code, &err_severity);
          break;
        case eok_ieq:
        case eok_ine:
        case eok_igt:
        case eok_ilt:
        case eok_ige:
        case eok_ile:
          do_icompare(constant_1, op, constant_2, result);
          break;
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
        case eok_fadd:
          do_fadd(constant_1, constant_2, result, &err_code, &err_severity,
                  &depends_on_rounding_mode);
          break;
        case eok_fsubtract:
          do_fsubtract(constant_1, constant_2, result, &err_code,
                       &err_severity, &depends_on_rounding_mode);
          break;
        case eok_fmultiply:
          do_fmultiply(constant_1, constant_2, result, &err_code,
                       &err_severity, &depends_on_rounding_mode);
          break;
        case eok_fdivide:
          do_fdivide(constant_1, constant_2, result, &err_code, &err_severity,
                     &depends_on_rounding_mode);
          break;
        case eok_feq:
        case eok_fne:
        case eok_fgt:
        case eok_flt:
        case eok_fge:
        case eok_fle:
          do_fcompare(constant_1, op, constant_2, result);
          break;

#if C99_IL_EXTENSIONS_SUPPORTED
        case eok_xadd:
          do_xadd(constant_1, constant_2, result, &err_code, &err_severity,
                  &depends_on_rounding_mode);
          break;
        case eok_xsubtract:
          do_xsubtract(constant_1, constant_2, result,
                       &err_code, &err_severity, &depends_on_rounding_mode);
          break;
        case eok_xmultiply:
          do_xmultiply(constant_1, constant_2, result,
                       &err_code, &err_severity, &depends_on_rounding_mode);
          break;
        case eok_xdivide:
          do_xdivide(constant_1, constant_2, result,
                     &err_code, &err_severity, &depends_on_rounding_mode);
          break;
        case eok_xeq:
        case eok_xne:
          do_xcompare(constant_1, op, constant_2, result);
          break;
        case eok_jmultiply:
          do_jmultiply(constant_1, constant_2, result,
                       &err_code, &err_severity, &depends_on_rounding_mode);
          break;
        case eok_jdivide:
          do_jdivide(constant_1, constant_2, result,
                     &err_code, &err_severity, &depends_on_rounding_mode);
          break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

        case eok_pdiff:
          do_pdiff(constant_1, constant_2, result, did_not_fold, &err_code,
                   &err_severity);
          break;
        case eok_padd:
        case eok_padd_subsc:
        case eok_psubtract:
          do_padd(constant_1, op, constant_2, result, &err_code,
                  &err_severity);
          break;
        case eok_pge:
        case eok_plt:
        case eok_pgt:
        case eok_pne:
        case eok_peq:
        case eok_ple:
          do_pcompare(constant_1, op, constant_2, result, did_not_fold,
                      &err_code, &err_severity);
          break;
        case eok_pmeq:
        case eok_pmne:
          do_pmcompare(constant_1, op, constant_2, result);
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
                               err_pos, result);
      if (err_severity == es_error) depends_on_rounding_mode = FALSE;
    }  /* if */
    /* If either constant was formed using operations that are not allowed
       in forming a null pointer constant, the result cannot be used as
       a null pointer constant. */
    result->null_pointer_constant_ruled_out =
                        constant_1->null_pointer_constant_ruled_out ||
                        constant_1->kind != (a_constant_repr_kind)ck_integer ||
                        constant_1->implicit_cast ||
                        constant_2->null_pointer_constant_ruled_out ||
                        constant_2->kind != (a_constant_repr_kind)ck_integer ||
                        constant_2->implicit_cast;
    if (depends_on_rounding_mode && !constant_context) {
      /* In a non-constant context, leave an operation to be done at runtime
         if its result depends on the floating-point rounding mode. */
      *did_not_fold = TRUE;
    }  /* if */
  }  /* if */

  db_exit();
}  /* binary_operation */


void fold_field_selection(a_constant            *constant_1,
                          a_symbol_ptr          field_sym,
                          a_type_ptr            result_type,
                          a_constant            *result,
                          a_boolean             *template_constant)
/*
Fold a constant field selection operation.  constant_1 is the pointer to the
struct/union; field_sym points to the field.  The result type (pointer to the
field type) is given by result_type.  The result is put in *result.
If constant_1 is a template parameter constant, return *template_constant
TRUE and do not fold the operation.  This folding operation is not done
through the usual interface because a field cannot be passed as a constant.
*/
{
  a_field_ptr      field;
  a_constant       offset;
  an_integer_value field_offset;
  a_boolean        err;
  a_symbol_ptr     anon_parent_sym;

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
    check_assertion(field_sym->kind == (a_symbol_kind)sk_field);
    /* If the field is a member of an anonymous union, add in the offset
       of the anonymous union.  There may be multiple levels of anonymous
       unions, so loop to do this. */
    /* This works for the nonstandard anonymous unions too.  In fact, it's
       only really needed for those, since it's only for anonymous structs
       that the offset can be non-zero.  Still, for the sake of completeness
       do it in all cases. */
    anon_parent_sym = field_sym;
    while ((anon_parent_sym =
            anon_parent_sym->variant.field.anonymous_parent_object) != NULL &&
           /* Ignore the last step if it's for a top-level (variable)
              anonymous union. */
           anon_parent_sym->kind != (a_symbol_kind)sk_variable) {
      check_assertion(anon_parent_sym->kind == (a_symbol_kind)sk_field);
      /* ... add the offset of the anonymous union, ... */
      set_unsigned_integer_value(&field_offset,
                                 anon_parent_sym->variant.field.ptr->offset);
      add_mixed_signed_integer_values(&offset.variant.integer_value,
                                      int_constant_is_signed(&offset),
                                      &field_offset,
                                      /*is_signed=*/FALSE, &err);
    }  /* while */
    field = field_sym->variant.field.ptr;
    /* ... add the offset of the field, ... */
    set_unsigned_integer_value(&field_offset, field->offset);
    add_mixed_signed_integer_values(&offset.variant.integer_value,
                                    int_constant_is_signed(&offset),
                                    &field_offset,
                                    /*is_signed=*/FALSE, &err);
    /* ... and put the offset into the result pointer constant.  Note that
       no overflow/object-size checking is needed, since the field has
       to be within the underlying object. */
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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
