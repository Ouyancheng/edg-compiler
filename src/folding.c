/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

folding.c -- Folding routines.

*/

#include "basics.h"
#include "error.h"
#include "lexical.h"
#include "il.h"
#include "target.h"
#include "expr.h"
#include "types.h"
#include "float_pt.h"
#include "cmd_line.h"


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
  /* Clear the source correspondence information.  If this was a
     manifest constant macro, the new constant should no longer be
     associated with the original constant. */
  set_default_source_corresp(&cp->source_corresp);
}  /* implicit_cast */


static a_boolean int_constant_is_signed(a_constant_ptr constant)
/*
Return TRUE if the given integer constant's type is signed.
*/
{
  a_type_ptr tp = skip_typerefs(constant->type);
#if CHECKING
  if (tp->kind != (a_type_kind)tk_integer) {
    internal_error("int_kind_of: constant type not integer");
  }  /* if */
#endif /* CHECKING */
  return (int_kind_is_signed(tp->variant.integer.int_kind));
}  /* int_constant_is_signed */


static void get_integer_attributes(a_constant      *cp,
                                   unsigned long   *sign_bit,
                                   unsigned long   *mask)
/*
For the integer type given by cp->type, return in *sign_bit a mask for
the sign bit, and in *mask a mask that can be used to truncate a value
to the right number of bytes.
*/
{
  a_type_ptr    int_type = skip_typerefs(cp->type);
  a_targ_size_t size;

  size = int_type->size;
#if CHECKING
  if (size == 0) internal_error("get_integer_attributes: zero-sized integer");
#endif /* CHECKING */
  /* Build the required bit masks. */
  *sign_bit = (unsigned long)1 << (unsigned long)((size * TARG_CHAR_BIT)-1);
  *mask = *sign_bit | (*sign_bit-1);
}  /* get_integer_attributes */


static void trunc_and_store_integer(long              result_value,
                                    a_constant        *result,
                                    an_error_code     *err_code,
                                    an_error_severity *err_severity)
/*
Truncate the integer result_value and store it in *result.  Set *err_code
and *err_severity to indicate any truncation error.  If *err_code is
already set to an error code, do not change it.
*/
{
  unsigned long sign_bit, mask;
  long          max_val, min_val;
  a_boolean     result_signed;

  get_integer_attributes(result, &sign_bit, &mask);
  result_signed = int_constant_is_signed(result);
  /* Do the error checks only if there's no previous error code. */
  if (*err_code == ec_no_error) {
    if (result_signed) {
      /* Destination is a signed integer. */
      /* Use variables here because some C compilers throw away the cast
         to long. */
      max_val = sign_bit-1;
      min_val = ~max_val;  /* Assuming twos' complement. */
      if (result_value > max_val || result_value < min_val) {
        /* The value will not fit in the destination integer type. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
      }  /* if */
    } else {
      /* Destination is an unsigned integer. */
      if ((unsigned long)result_value > mask) {
        /* The value will not fit in the destination integer type. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Truncate the value to the right size. */
  result_value &= mask;
  /* Sign-extend a signed negative result. */
  if (result_signed && (result_value & sign_bit)) result_value |= ~mask;
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;
}  /* trunc_and_store_integer */


static void conv_integer_to_integer(a_constant        *old_constant,
				    a_constant        *new_constant,
				    a_boolean         issue_type_chg_warning,
				    an_error_code     *err_code,
				    an_error_severity *err_severity)
/*
Convert an integral constant of some kind (in *old_constant) to a new
integral constant in *new_constant, with type as indicated therein.  Return
*err_code and *err_severity set to indicate any error/warning detected,
or *err_code == ec_no_error if everything went fine.  If
issue_type_chg_warning is TRUE, suppress any warnings.
*/
{
  long          old_value = old_constant->variant.integer_value;
  long          new_value;
  a_boolean     old_signed, new_signed;
  unsigned long new_sign_bit, new_mask, old_sign_bit, old_mask;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Determine attributes (size, signedness) of the old/new integer kinds. */
  old_signed = int_constant_is_signed(old_constant);
  new_signed = int_constant_is_signed(new_constant);
  get_integer_attributes(new_constant, &new_sign_bit, &new_mask);

  /* NOTE: This routine is designed to work only when the host and
     target computers are twos complement. */

  new_value = old_value;
  /* Do any necessary truncation. */
  new_value &= new_mask;
  /* Sign-extend the result if necessary. */
  if (new_signed && (new_value & new_sign_bit)) new_value |= ~new_mask;
  if (issue_type_chg_warning) {
    /* If the value changed, a warning is in order.  The value has changed
       if the bit pattern changed ... */
    if (new_value != old_value ||
        /* ... or if the new sign is different than the old sign. */
        ((new_signed && new_value < 0) != (old_signed && old_value < 0))) {
      /* The new value is different than the old value.  See if the change
         is a truncation (dropping bits) or a sign change. */
      get_integer_attributes(old_constant, &old_sign_bit, &old_mask);
      if (new_mask >= old_mask) {  /* i.e., new size >= old size */
        /* The new size is at least as big as the old size, so no truncation
           is possible.  Therefore, this must be a sign change. */
        /* Do not issue this warning for non-arithmetic constants. */
        if (!old_constant->non_arithmetic) {
          *err_code = ec_integer_sign_change;
          *err_severity = es_warning;
        }  /* if */
      } else {
        /* The new size is smaller than the old size, which means truncation
           is possible.  See if the significant part of the new value is the
           same as the old value.  If so, no bits have been lost, and
           this is a sign change.  Note that the old value must have all
           zero bits outside of the bits saved in the new value for
           this to be so. */
        if ((new_value & new_mask) == old_value) {
          /* The significant bit pattern is the same, so this is
             a sign change. */
          /* Do not issue this warning for non-arithmetic constants. */
          if (!old_constant->non_arithmetic) {
            *err_code = ec_integer_sign_change;
            *err_severity = es_warning;
          }  /* if */
        } else {
          /* Some part of the significant bit pattern has changed, so this
             is a truncation. */
          *err_code = ec_integer_truncated;
          *err_severity = es_warning;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  set_constant_kind(new_constant, (a_constant_repr_kind)ck_integer);
  new_constant->variant.integer_value = new_value;
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
  long          old_value;
  unsigned long unsigned_old_value;
  a_boolean     err;
  a_type_ptr    constant_type = skip_typerefs(new_constant->type);
  a_float_kind  float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(new_constant, (a_constant_repr_kind)ck_float);

  if (int_constant_is_signed(old_constant)) {
    /* The source is a signed integer value. */
    old_value = old_constant->variant.integer_value;
    fp_long_to_float(float_kind, old_value,
                     &new_constant->variant.float_value, &err);
  } else {
    /* The source is an unsigned integer value. */
    unsigned_old_value = (unsigned long)old_constant->variant.integer_value;
    fp_unsigned_long_to_float(float_kind, unsigned_old_value, 
                              &new_constant->variant.float_value, &err);
  }  /* if */
  if (err) {
    /* Some error.  This is probably impossible unless integers and floats
       are defined in some very strange way. */
    *err_code = ec_integer_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_integer_to_float */


static void conv_float_to_integer(a_constant        *old_constant,
			          a_constant        *new_constant,
			          an_error_code     *err_code,
				  an_error_severity *err_severity)
/*
Convert a float of some kind (in *old_constant) to an integer constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.
*/
{
  long          result_value;
  unsigned long unsigned_result_value;
  a_boolean     result_signed;
  a_boolean     err;
  a_float_kind  float_kind =
                         skip_typerefs(old_constant->type)->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_signed = int_constant_is_signed(new_constant);
  if (result_signed) {
    /* Destination is a signed integer. */
    fp_to_long(float_kind,
               &old_constant->variant.float_value, &result_value, &err);
  } else {
    /* Destination is an unsigned integer. */
    fp_to_unsigned_long(float_kind,
                        &old_constant->variant.float_value,
                        &unsigned_result_value, &err);
    if (!err) result_value = unsigned_result_value;
  }  /* if */
  if (err) result_value = 0;
  trunc_and_store_integer(result_value, new_constant, err_code, err_severity);
  if (err || *err_code != ec_no_error) {
    /* Float value is too big to fit in the integer. */
    *err_code = ec_float_to_integer_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_float_to_integer */


static void conv_float_to_float(a_constant        *old_constant,
			        a_constant        *new_constant,
			        an_error_code     *err_code,
				an_error_severity *err_severity)
/*
Convert a float of some kind (in *old_constant) to a float constant
in *new_constant, with type as indicated therein.  Return *err_code and
*err_severity set to indicate any error/warning detected, or
*err_code == ec_no_error if everything went fine.
*/
{
  a_boolean    err;
  a_float_kind old_kind =
                         skip_typerefs(old_constant->type)->variant.float_kind;
  a_float_kind float_kind =
                         skip_typerefs(new_constant->type)->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(new_constant, (a_constant_repr_kind)ck_float);
  fp_change_kind(&old_constant->variant.float_value, old_kind,
                 &new_constant->variant.float_value, float_kind,
                 &err);
  if (err) {
    *err_code = ec_float_to_float_conversion;
    *err_severity = es_error;
  }  /* if */
}  /* conv_float_to_float */


static void conv_pointer_to_whatever(a_constant        *old_constant,
				     a_constant        *new_constant,
				     an_error_code     *err_code,
				     an_error_severity *err_severity)
/*
Convert a pointer constant to a constant of type as specified by
"new_constant".
*/
{
  a_type_ptr new_type = new_constant->type;

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
  if (is_integral_type(new_type)) {
    /* Pointer value being forced into an integral type.  Make sure the
       integral type is large enough to hold a pointer. */
#if TARG_ALL_POINTERS_SAME_SIZE
    /* All pointers are the same size. */
    if (skip_typerefs(new_type)->size < TARG_SIZEOF_POINTER) {
      *err_code = ec_integer_truncated;
      *err_severity = es_error;
    }  /* if */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
??=error conv_pointer_to_whatever: different-sized pointers not implemented.
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  }  /* if */
  if (*err_code != ec_no_error) {
    set_error_constant(new_constant);
  } else {
    copy_constant(old_constant, new_constant);
    implicit_cast(new_constant, new_type);
  }  /* if */
}  /* conv_pointer_to_whatever */


static void conv_integer_to_pointer(a_constant        *old_constant,
				    a_constant        *new_constant,
			  	    a_boolean         issue_type_chg_warning,
				    an_error_code     *err_code,
				    an_error_severity *err_severity)
/*
Convert an integer constant to a pointer constant of type as specified by
"new_constant".
*/
{
  a_type_ptr new_type = new_constant->type;

  *err_code = ec_no_error;
  *err_severity = es_warning;
  if (issue_type_chg_warning) {
    if (old_constant->variant.integer_value != 0) {
      /* Any value other than zero (NULL).  Issue a warning. */
      *err_code = ec_non_zero_int_conv_to_pointer;
      *err_severity = es_warning;
    }  /* if */
  }  /* if */
  /* Make a new constant that is the old constant implicitly cast to the
     pointer type. */
  copy_constant(old_constant, new_constant);
  implicit_cast(new_constant, new_type);
}  /* conv_integer_to_pointer */


void type_change_constant(a_constant        *constant,
			  a_type_ptr        new_type,
			  a_boolean         issue_type_chg_warning,
			  an_error_code     *err_code,
			  an_error_severity *err_severity)
/*
Convert the indicated constant to "new_type".  Set *err_code and *err_severity
to indicate any errors or warnings; if none were found, set *err_code to
ec_no_error.  Warnings for loss of precision or change of sign are given
only if issue_type_chg_warning is TRUE (usually, TRUE means the type conversion
is implicit, and FALSE means there was an explicit cast).
*/
{
  a_type_ptr constant_type;
  a_constant new_constant;

  db_enter(5, "type_change_constant");
  *err_code = ec_no_error;
  *err_severity = es_warning;
  clear_constant(&new_constant, (a_constant_repr_kind)ck_error);

  /* Remove any type qualifiers or typedefs from the types involved. */
  constant_type = skip_typerefs(constant->type);
  new_constant.type = new_type = skip_typerefs(new_type);

  if (is_error_type(new_type)) {
    /* Changing to an error type, so produce an error constant as result.
       new_constant is already set appropriately. */
    goto exit;
  } else if (identical_types(constant_type, new_type)) {
    /* The current and new types are the same, so no change is required. */
    copy_constant(constant, &new_constant);
    /* Put in the actual type wanted, as it may have qualifiers. */
    new_constant.type = new_type;
    goto exit;
  } else if (constant->kind == (a_constant_repr_kind)ck_address) {
    /* Any case where the constant is represented as an address should be
       converted by setting the implicit_cast flag.  This test has to be
       early -- like this -- to catch ((unsigned)((int)&x)).  That case
       would have constant_type->kind == tk_integer and new_type->kind
       == tk_integer, and so would not look like it involves pointers. */
    conv_pointer_to_whatever(constant, &new_constant, err_code, err_severity);
    goto exit;
  }  /* if */

  /* Determine the type we are converting from. */
  switch (constant_type->kind) {

    case tk_integer:
      /* Converting from integer. */
      switch(new_type->kind) {
        case tk_integer:
          /* Converting integer to integer. */
          conv_integer_to_integer(constant, &new_constant,
                                  issue_type_chg_warning,
                                  err_code, err_severity);
          break;
        case tk_float:
          /* Converting integer to float. */
          conv_integer_to_float(constant, &new_constant,
                                err_code, err_severity);
          break;
        case tk_pointer:
          /* Converting integer to pointer. */
          conv_integer_to_pointer(constant, &new_constant,
                                  issue_type_chg_warning,
                                  err_code, err_severity);
          break;
#if CHECKING
        default:
          internal_error("type_change_constant: integer to bad type");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_float:
      /* Converting from float. */
      switch (new_type->kind) {
        case tk_integer:
          /* Converting float to integer. */
          conv_float_to_integer(constant, &new_constant,
                                err_code, err_severity);
          break;
        case tk_float:
          /* Converting float to float. */
          conv_float_to_float(constant, &new_constant,
                              err_code, err_severity);
          break;
#if CHECKING
        default:
          internal_error("type_change_constant: float to bad type");
#endif /* CHECKING */
      }  /* switch */
      break;

    case tk_pointer:
      /* Converting from pointer. */
      conv_pointer_to_whatever(constant, &new_constant,
                               err_code, err_severity);
      break;

    case tk_error:
      /* The old constant is an error constant. */
      /* Change the type of the new constant back to the original type of the
	 error constant, i.e., error. */
      new_constant.type = constant->type;
      break;

#if CHECKING
    default:
      internal_error("type_change_constant: from bad type");
#endif /* CHECKING */
  }  /* switch */

exit:
  if (*err_code != ec_no_error && *err_severity == es_error) {
    set_error_constant(&new_constant);
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "type_change_constant of ");
    db_constant(constant);
    fprintf(f_debug, ", result = ");
    db_constant(&new_constant);
    if (*err_code != ec_no_error) {
      fprintf(f_debug, " with ");
      if (*err_severity == es_error) {
        fprintf(f_debug, "error");
      } else if (*err_severity == es_warning) {
        fprintf(f_debug, "warning");
      } else {
        fprintf(f_debug, "diagnostic");
      }  /* if */
    }  /* if */
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  /* Return the new constant value. */
  copy_constant(&new_constant, constant);
  db_exit();
}  /* type_change_constant */


a_boolean is_zero_constant(a_constant *constant)
/*
Return TRUE if the constant is an integer, pointer, or floating zero.
This is supposed to duplicate the test on the boolean controlling 
expressions in statements and the ?:, &&, and || operators.
*/
{
  a_boolean is_zero = FALSE;
  a_float_kind float_kind;

  switch (constant->kind) {
    case ck_integer:
      /* Either an integer zero or a zero cast to a pointer type is
         acceptable, so it is not necessary to check the constant type. */
      is_zero = (constant->variant.integer_value == 0);
      break;
    case ck_float:
      float_kind = skip_typerefs(constant->type)->variant.float_kind;
      is_zero = fp_is_zero_constant(float_kind,
                                    &constant->variant.float_value);
      break;
    /* Note that ck_address constants are always non-NULL and therefore
       is_zero is left FALSE. */
  }  /* switch */

  return (is_zero);
}  /* is_zero_constant */


a_boolean is_null_pointer_constant(a_constant *constant)
/*
Return TRUE if the given constant is a null pointer constant.
*/
{
  a_boolean  is_null_pointer = FALSE;
  a_type_ptr ptr_type;

  if (constant->kind == (a_constant_repr_kind)ck_integer) {
    if (constant->variant.integer_value == 0L) {
      if (constant->implicit_cast) {
        /* Must be cast to (void *) to be a null pointer constant.
           Qualifiers are not allowed on the pointer or the void type
           pointed to (see 3.2.2.3; it says "void *" without mentioning
           the possibility of qualifiers). */
        if (is_pointer_type(constant->type) &&
            !is_qualified_type(constant->type)) {
          ptr_type = type_pointed_to(constant->type);
	  if (is_void_type(ptr_type) && !is_qualified_type(ptr_type)) {
	    is_null_pointer = TRUE;
	  }  /* if */
	}  /* if */
      } else {
	/* If not implicitly cast, it's just a plain integer zero, which is
           also a null pointer constant. */
	is_null_pointer = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */

  return is_null_pointer;
}  /* is_null_pointer_constant */


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
  long operand_value, result_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  operand_value = constant->variant.integer_value;
  result_value = (long)(-((unsigned long)operand_value));
  if (int_constant_is_signed(constant)) {
    /* Negation of a signed integer. */
    if ((operand_value < 0) == (result_value < 0) &&
	operand_value != 0) {
      /* Result has the same sign as the operand: overflow. */
      /* Suppress this error for non-arithmetic constants in K&R mode. */
      if (C_dialect != C_dialect_pcc || !constant->non_arithmetic) {
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
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
  trunc_and_store_integer(result_value, result, err_code, err_severity);

#if DEBUG
  db_unary_operation("i-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_inegate */


static void do_fnegate(a_constant        *constant,
		       a_constant        *result,
		       an_error_code     *err_code,
		       an_error_severity *err_severity)
/*
Do the negate operation on types of floats.
*/
{
  an_internal_float_value temp_zero;
  a_type_ptr              constant_type = skip_typerefs(constant->type);
  a_float_kind            float_kind = constant_type->variant.float_kind;
  a_boolean               err;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);

  /* Make a floating-point zero, and subtract the constant from it. */
  fp_long_to_float(float_kind, 0L, &temp_zero, &err);
  fp_subtract(float_kind, &temp_zero, &constant->variant.float_value,
              &result->variant.float_value, &err);
  if (err) {
    *err_code = ec_bad_float_operation_result;
    *err_severity = es_error;
  }  /* if */

#if DEBUG
  db_unary_operation("f-", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_fnegate */


static void do_complement(a_constant        *constant,
		          a_constant        *result,
			  an_error_code     *err_code,
			  an_error_severity *err_severity)
/*
Do the complement operation on all type of integers.
*/
{
  long result_value;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  result_value = (long)(~(unsigned long)constant->variant.integer_value);
  trunc_and_store_integer(result_value, result, err_code, err_severity);
  result->non_arithmetic = TRUE;

#if DEBUG
  db_unary_operation("~", constant, result, *err_code);
#endif /* DEBUG */
}  /* do_complement */


static void do_not(a_constant        *constant,
		   a_constant        *result)
/*
Do the "!" (not) operation on all types of scalars.
*/
{
  long         result_value;
  a_float_kind float_kind;

  switch (constant->kind) {
    case ck_integer:
      result_value = !constant->variant.integer_value;
      break;
    case ck_float:
      float_kind = skip_typerefs(constant->type)->variant.float_kind;
      result_value = !fp_is_zero_constant(float_kind,
                                          &constant->variant.float_value);
      break;
    case ck_address:
      /* A pointer to a variable, routine, or constant.  Always non-NULL, so
           the "not" is 0. */
      result_value = 0;
      break;
#if CHECKING
    default:
      internal_error("do_not: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = result_value;

#if DEBUG
  db_unary_operation("!", constant, result, ec_no_error);
#endif /* DEBUG */
}  /* do_not */


void unary_operation(an_expr_operator_kind op,
		     a_constant            *constant,
                     a_type_ptr            result_type,
		     a_constant            *result,
                     a_boolean             *did_not_fold,
		     an_error_code         *err_code,
		     an_error_severity     *err_severity)
/*
Fold unary operations on constants.  op indicates the operation,
constant the operand.  result_type indicates the desired result type.
The result constant is put into result.  If the operation could not
be folded, *did_not_fold is returned TRUE.  *err_code and *err_severity are
set to indicate any errors or warnings found; if none are found,
*err_code is set to ec_no_error.
*/
{
  db_enter(5, "unary_operation");

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
  clear_constant(result, (a_constant_repr_kind)ck_error);
  result->type = result_type;
  if (constant->kind == (a_constant_repr_kind)ck_error) {
    /* The constant is an error constant; set the result to an error
       constant and return. */
    set_error_constant(result);
  } else if (constant->kind == (a_constant_repr_kind)ck_address &&
             constant->implicit_cast && is_integral_type(constant->type)) {
    /* An address constant cast to an integral type is a link-time
       constant, not a compile-time constant.  We cannot do operations
       on it. */
    *did_not_fold = TRUE;
  } else {
    switch (op) {
      case eok_fnegate:
        do_fnegate(constant, result, err_code, err_severity);
        break;
      case eok_inegate:
        do_inegate(constant, result, err_code, err_severity);
        break;
      case eok_complement:
        do_complement(constant, result, err_code, err_severity);
        break;
      case eok_not:
        do_not(constant, result);
        break;
#if CHECKING
      default:
        internal_error("unary_operation: bad unary operator");
        break;
#endif /* CHECKING */
    }  /* switch */
    if (*err_code != ec_no_error && *err_severity == es_error) {
      set_error_constant(result);
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
  long result_value, value_1, value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  result_value = (long)((unsigned long)value_1 + (unsigned long)value_2);
  if (int_constant_is_signed(result)) {
    /* Addition of signed integers. */
    /* Check overflow possibilities. */
    if (value_1 >= 0 && value_2 >= 0) {
      if (result_value < 0) {
        /* Nonnegative + nonnegative produced negative: overflow. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
      }  /* if */
    } else if (value_1 < 0 && value_2 < 0) {
      if (result_value >= 0) {
        /* Negative + negative produced positive: overflow. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
      }  /* if */
    }  /* if */
  }  /* if */
  trunc_and_store_integer(result_value, result, err_code, err_severity);

#if DEBUG
  db_binary_operation("i+", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_iadd */


static a_boolean subtract_protected(long      value_1,
                                    long      value_2,
                                    long      *result_value)
/*
Do "*result_value = value_1*value_2", with overflow checking.  Return
TRUE if there is no overflow, FALSE if there is overflow.
*/
{
  a_boolean err = FALSE;

  *result_value = (long)((unsigned long)value_1 - (unsigned long)value_2);
  /* Check overflow possibilities. */
  if (value_1 >= 0 && value_2 < 0) {
    if (*result_value < 0) {
      /* Nonnegative - negative produced negative: overflow. */
      err = TRUE;
    }  /* if */
  } else if (value_1 < 0 && value_2 >= 0) {
    if (*result_value >= 0) {
      /* Negative - nonnegative produced nonnegative: overflow. */
      err = TRUE;
    }  /* if */
  }  /* if */
  return !err;
}  /* subtract_protected */


static void do_isubtract(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the subtract operation on all types of integers.
*/
{
  long result_value, value_1, value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  if (int_constant_is_signed(result)) {
    /* Subtraction of signed integers. */
    if (!subtract_protected(value_1, value_2, &result_value)) {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  } else {
    /* Subtraction of unsigned integers. */
    result_value = (long)((unsigned long)value_1 - (unsigned long)value_2);
  }  /* if */
  trunc_and_store_integer(result_value, result, err_code, err_severity);

#if DEBUG
  db_binary_operation("i-", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_isubtract */


static long divide_integers(long value_1,
                            long value_2)
/*
Divide value_1 by value_2 and return the quotient.  This routine forces
truncation toward zero on division involving negative numbers, which
is not guaranteed by C.  The caller must ensure that value_2 is not zero
and that if value_2 == -1, value_1 != LONG_MIN on a twos' complement
machine.
*/
{
  long result_value = value_1 / value_2;

  /* If either of the values is negative, check for truncation away from zero
     and compensate for it.  Since by definition
       (value_1/value_2)*value_2 + value_1%value_2 == value_1,
     if the sign of value_1%value_2 is different than the sign of value_1
     truncation was away from zero. */
  if (value_1 < 0) {
    if (value_1 % value_2 > 0) result_value++;
  } else if (value_2 < 0) {
    if (value_1 % value_2 < 0) result_value++;
  }  /* if */
  return result_value;
}  /* divide_integers */


static a_boolean multiply_protected(long      value_1,
                                    long      value_2,
                                    long      *result_value)
/*
Do "*result_value = value_1*value_2", with overflow checking.  Return
TRUE if there is no overflow, FALSE if there is overflow.
*/
{
  a_boolean err = FALSE;

  *result_value = (long)((unsigned long)value_1 * (unsigned long)value_2);
  /* Check overflow possibilities. */
  if (value_1 > 0 && value_2 > 0) {
    /* divide_integers is not needed here, since both numbers are positive. */
    if (LONG_MAX / value_2 < value_1) err = TRUE;
  } else if (value_1 > 0 && value_2 < -1) {
    /* As an example of the kind of problem that calls for divide_integers,
       on a 32-bit twos' complement machine:
         value_1 == 715827883, value_2 == -3 -- product overflows.
         LONG_MIN == -2147483648
       on a machine where truncation is not towards zero,
         LONG_MIN / value_2 == 715827883
         LONG_MIN % value_2 == 1
       which fulfills the definition
         (LONG_MIN/value_2)*value_2 + LONG_MIN%value_2 == LONG_MIN
       but LONG_MIN/value_2 == value_1, so no error is detected. */
    if (divide_integers(LONG_MIN, value_2) < value_1) err = TRUE;
  } else if (value_1 < -1 && value_2 > 0) {
    if (divide_integers(LONG_MIN, value_1) < value_2) err = TRUE;
  } else if (value_1 < -1 && value_2 < 0) {
    if (divide_integers(LONG_MAX, value_1) > value_2) err = TRUE;
  } else if ((LONG_MIN + LONG_MAX) < 0 &&
             ((value_1 == -1 && value_2 == LONG_MIN) ||
              (value_1 == LONG_MIN && value_2 == -1))) {
    /* -1 times the smallest integer is an overflow on a 2's complement
       machine. */
    err = TRUE;
  }  /* if */
  return !err;
}  /* multiply_protected */


static void do_imultiply(a_constant        *constant_1,
		         a_constant        *constant_2,
		         a_constant        *result,
		         an_error_code     *err_code,
			 an_error_severity *err_severity)
/*
Do the multiply operation on all types of integers.
*/
{
  long result_value, value_1, value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  if (int_constant_is_signed(result)) {
    /* Multiplication of signed integers. */
    if (!multiply_protected(value_1, value_2, &result_value)) {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  } else {
    /* Multiplication of unsigned integers. */
    result_value = (long)((unsigned long)value_1 * (unsigned long)value_2);
  }  /* if */
  trunc_and_store_integer(result_value, result, err_code, err_severity);

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
  long result_value, value_1, value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  if (value_2 == 0) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
    result_value = 0;
  } else {
    if (int_constant_is_signed(result)) {
      /* Division of signed integers. */
      /* Check for overflow possibility on a twos' complement machine. */
      if ((LONG_MIN + LONG_MAX) < 0 &&
          value_1 == LONG_MIN && value_2 == -1) {
        /* Smallest integer / -1 -- Overflow on 2's complement machines. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
        result_value = 0;
      } else {
        /* No overflow. */
        result_value = value_1 / value_2;
      }  /* if */
    } else {
      /* Division of unsigned integers. */
      result_value = (long)((unsigned long)value_1 / (unsigned long)value_2);
    }  /* if */
  }  /* if */
  trunc_and_store_integer(result_value, result, err_code, err_severity);

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
  long result_value, value_1, value_2;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  if (value_2 == 0) {
    *err_code = ec_mod_by_zero;
    *err_severity = es_error;
    result_value = 0;
  } else {
    if (int_constant_is_signed(result)) {
      /* Remainder on signed integers. */
      /* Check for overflow possibility on a twos' complement machine. */
      if ((LONG_MIN + LONG_MAX) < 0 &&
          value_1 == LONG_MIN && value_2 == -1) {
        /* Smallest integer / -1 -- Overflow on 2's complement machines. */
        *err_code = ec_integer_overflow;
        *err_severity = es_error;
        result_value = 0;
      } else {
        /* No overflow. */
        result_value = value_1 % value_2;
      }  /* if */
    } else {
      /* Remainder on unsigned integers. */
      result_value = (long)((unsigned long)value_1 % (unsigned long)value_2);
    }  /* if */
  }  /* if */
  trunc_and_store_integer(result_value, result, err_code, err_severity);

#if DEBUG
  db_binary_operation("%", constant_1, constant_2, result, *err_code);
#endif /* DEBUG */
}  /* do_remainder */


void check_shift_count(a_constant    *shift_count_constant,
                       a_type_ptr    operand_type,
                       an_error_code *err_code)
/*
shift_count_constant is the constant shift count for a shift operation.
The entity being shifted has the type operand_1_type.  Check the shift
count to see if it is valid.  If so, return *err_code set to ec_no_error;
if not, return *err_code set to the proper error code.
*/
{
  a_targ_size_t size;
  long          signed_shift_count;
  unsigned long unsigned_shift_count;

  *err_code = ec_no_error;

  /* Determine the size of the operand being shifted. */
  operand_type = skip_typerefs(operand_type);
#if CHECKING
  if (operand_type->kind != (a_type_kind)tk_integer) {
    internal_error("check_shift_count: operand_type not integer");
  } else if (operand_type->size == 0) {
    internal_error("check_shift_count: integer type has size 0");
  }  /* if */
#endif /* CHECKING */
  size = operand_type->size;

  if (int_constant_is_signed(shift_count_constant)) {
    /* The shift count is signed. */
    signed_shift_count = shift_count_constant->variant.integer_value;
    if (signed_shift_count < 0) {
      *err_code = ec_negative_shift_count;
    } else if (signed_shift_count/TARG_CHAR_BIT >= size) {
      *err_code = ec_shift_count_too_large;
    }  /* if */
  } else {
    /* The shift count is unsigned. */
    unsigned_shift_count =
                  (unsigned long)(shift_count_constant->variant.integer_value);
    if (unsigned_shift_count/TARG_CHAR_BIT >= (unsigned long)size) {
      *err_code = ec_shift_count_too_large;
    }  /* if */
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
  a_boolean     is_signed;
  long          value_1, value_2, result_value;
  unsigned long mask;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  check_shift_count(constant_2, constant_1->type, err_code);
  if (*err_code != ec_no_error) {
    /* Something wrong with the shift count. */
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    value_1 = constant_1->variant.integer_value;
    value_2 = constant_2->variant.integer_value;
    /* The operand to be shifted is either signed or unsigned, and the
       shift must be done accordingly. */
    is_signed = int_constant_is_signed(constant_1);
    if (shift_right) {
      if (is_signed) {
        result_value = value_1 >> value_2;
        if (value_1 < 0) {
          /* The operand shifted is signed and negative. */
          /* Just in case the host C shifts do not match the target,
             adjust for proper target sign extension. */
          /* Make a mask with zeroes at the top and "value_2" one bits at
             the bottom. */
          mask = (~(unsigned long)0) >> value_2;
#if TARG_RIGHT_SHIFT_IS_ARITHMETIC
          /* Ensure that the sign bit is propagated on the shift. */
          result_value |= ~mask;
#else /* !TARG_RIGHT_SHIFT_IS_ARITHMETIC */
          /* Ensure that the bits shifted in are zeroed. */
          result_value &= mask;
#endif /* TARG_RIGHT_SHIFT_IS_ARITHMETIC */
        }  /* if */
      } else {
	result_value = (unsigned long)value_1 >> value_2;
      }  /* if */
    } else {
      if (is_signed) {
        result_value = value_1 << value_2;
      } else {
        result_value = (unsigned long)value_1 << value_2;
      }  /* if */
    }  /* if */
    result->variant.integer_value = result_value;
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
  int  cmp;
  long result_value, value_1, value_2;

  /* Develop a strcmp-like relation value in cmp:
       constant_1 > constant_2   1
       constant_1 = constant_2   0
       constant_1 < constant_2  -1
  */
  value_1 = constant_1->variant.integer_value;
  value_2 = constant_2->variant.integer_value;
  if (int_constant_is_signed(constant_1)) {
    /* Signed integer comparison. */
    if (value_1 > value_2) {
      cmp = 1;
    } else if (value_1 == value_2) {
      cmp = 0;
    } else {
      cmp = -1;
    }  /* if */
  } else {
    /* Unsigned integer comparison. */
    if ((unsigned long)value_1 > (unsigned long)value_2) {
      cmp = 1;
    } else if ((unsigned long)value_1 == (unsigned long)value_2) {
      cmp = 0;
    } else {
      cmp = -1;
    }  /* if */
  }  /* if */
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
  result->variant.integer_value = result_value;

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
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = constant_1->variant.integer_value &
	                          constant_2->variant.integer_value;
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
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = constant_1->variant.integer_value |
	                          constant_2->variant.integer_value;
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
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = constant_1->variant.integer_value ^
	                          constant_2->variant.integer_value;
  result->non_arithmetic = TRUE;
#if DEBUG
  db_binary_operation("^", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_xor */


static void do_land(a_constant    *constant_1,
		    a_constant    *constant_2,
		    a_constant    *result)
/*
Do the logical "and" operation on integers, floats, and pointers.
*/
{
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = !is_zero_constant(constant_1) &&
                                  !is_zero_constant(constant_2);
#if DEBUG
  db_binary_operation("&&", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_land */


static void do_lor(a_constant    *constant_1,
		   a_constant    *constant_2,
		   a_constant    *result)
/*
Do the logical "or" operation on integers, floats, and pointers.
*/
{
  set_constant_kind(result, (a_constant_repr_kind)ck_integer);
  result->variant.integer_value = !is_zero_constant(constant_1) ||
                                  !is_zero_constant(constant_2);
#if DEBUG
  db_binary_operation("||", constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_lor */


static void do_fadd(a_constant        *constant_1,
		    a_constant        *constant_2,
		    a_constant        *result,
		    an_error_code     *err_code,
		    an_error_severity *err_severity)
/*
Do the addition operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_add(float_kind,
         &constant_1->variant.float_value,
         &constant_2->variant.float_value,
         &result->variant.float_value, &err);
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
			 an_error_severity *err_severity)
/*
Do the subtraction operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_subtract(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err);
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
			 an_error_severity *err_severity)
/*
Do the multiplication operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  set_constant_kind(result, (a_constant_repr_kind)ck_float);
  fp_multiply(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err);
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
		       an_error_severity *err_severity)
/*
Do the division operation on all types of float.
*/
{
  a_boolean    err;
  a_type_ptr   constant_type = skip_typerefs(constant_1->type);
  a_float_kind float_kind = constant_type->variant.float_kind;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  /* Check for division by zero to give a specific error message. */
  if (fp_is_zero_constant(float_kind, &constant_2->variant.float_value)) {
    *err_code = ec_divide_by_zero;
    *err_severity = es_error;
  } else {
    set_constant_kind(result, (a_constant_repr_kind)ck_float);
    fp_divide(float_kind,
              &constant_1->variant.float_value,
              &constant_2->variant.float_value,
              &result->variant.float_value, &err);
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
  long         result_value;
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
    result_value = 0;
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
  result->variant.integer_value = result_value;

#if DEBUG
  db_binary_operation(db_operator_names[op],
                      constant_1, constant_2, result, ec_no_error);
#endif /* DEBUG */
}  /* do_fcompare */


static a_targ_ptrdiff_t pointer_offset(a_constant_ptr constant)
/*
Retrieve and return the offset part of the given pointer constant.
Note that this routine must work when applied to an address constant
that has been cast to an integral type.
*/
{
  a_targ_ptrdiff_t offset;

  switch (constant->kind) {
    case ck_address:
      /* Address of a routine, variable, or constant, plus some offset. */
      offset = constant->variant.address.offset;
      break;
    case ck_integer:
      /* Integer cast to a pointer type (probably 0/NULL). */
      offset = constant->variant.integer_value;
      break;
#if CHECKING
    default:
      internal_error("pointer_offset: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return (offset);
}  /* pointer_offset */


static void set_pointer_offset(a_constant_ptr   constant,
                               a_targ_ptrdiff_t offset)
/*
Put the indicated offset into the pointer constant.
Note that this routine must work when applied to an address constant
that has been cast to an integral type.
*/
{
  switch (constant->kind) {
    case ck_integer:
      constant->variant.integer_value = offset;
      break;
    case ck_address:
      constant->variant.address.offset = offset;
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
This is NULL is the pointer is an integer cast to a pointer type.  Otherwise,
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
#if CHECKING
      default:
        internal_error("base_object: bad address constant kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return (object);
}  /* base_object */


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
        /* No size to check. */
        break;
      case abk_constant:
        cp = constant->variant.address.variant.constant;
        if (cp->kind == (a_constant_repr_kind)ck_string) {
          object_size = cp->variant.string.length;
        }  /* if */
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
         ANSI allows that for arrays to simplify some coding.  That
         subscript value is flagged later as an error by using_lvalue
         (it calls this routine again). */
      valid = (constant->variant.address.offset <= object_size);
      *just_past_end = (constant->variant.address.offset == object_size);
    } else {
      /* Don't know what the size is, so assume the offset is valid. */
      valid = TRUE;
    }  /* if */
  }  /* if */

  return (valid);
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
  a_targ_size_t    size, incr_val;
  a_targ_ptrdiff_t offset;
  a_boolean        negative_incr_val;
  a_boolean        just_past_end;

  *err_code = ec_no_error;
  *err_severity = es_warning;

  if (op == (an_expr_operator_kind)eok_iadd ||
      op == (an_expr_operator_kind)eok_isubtract) {
    /* For the (int)address +- constant case, the size (scaling) is 1. */
    size = 1;
  } else {
    /* Get the size of the thing pointed to. */
    size = skip_typerefs(type_pointed_to(constant_1->type))->size;
#if CHECKING
    if (size == 0) internal_error("do_padd: size is zero");
#endif /* CHECKING */
  }  /* if */
  /* Get the increment constant.  Process it properly if it's unsigned. */
#if CHECKING
  if (constant_2->kind != (a_constant_repr_kind)ck_integer) {
    internal_error("do_padd: constant_2 not integer");
  }  /* if */
#endif /* CHECKING */
  negative_incr_val = FALSE;
  if (int_constant_is_signed(constant_2)) {
    if (constant_2->variant.integer_value < 0) {
      negative_incr_val = TRUE;
      /* Negate the constant in such a way that the smallest integer
         does not cause an overflow. */
      incr_val = -(unsigned long)constant_2->variant.integer_value;
    } else {
      incr_val = constant_2->variant.integer_value;
    }  /* if */
  } else {
    /* Unsigned constant. */
    incr_val = constant_2->variant.integer_value;
  }  /* if */
  /* Subtraction is just adding a negative. */
  if (op == (an_expr_operator_kind)eok_psubtract ||
      op == (an_expr_operator_kind)eok_isubtract) {
    negative_incr_val = !negative_incr_val;
  }  /* if */
  /* Scale incr_val by the size of the object pointed to.  Watch for
     overflow cases.  Note that both size and incr_val are unsigned. */
  if ((TARG_SIZE_T_MAX / size) < incr_val) {
    *err_code = ec_integer_overflow;
    *err_severity = es_error;
  } else {
    incr_val = incr_val * size;
  }  /* if */
  /* Add the scaled incr_val to the offset from constant_1 (or subtract, as
     appropriate).  Note that offset is signed and incr_val is unsigned. */
  offset = pointer_offset(constant_1);
  if (negative_incr_val) {
    /* We want offset -= incr_val.  The overflow check is for integer
       underflow.  To check
         offset - incr_val >= LONG_MIN
       we check
         offset >= LONG_MIN + incr_val
       where the addition can't overflow.
    */
    if (offset >= (long)((unsigned long)LONG_MIN + incr_val)) {
      offset -= incr_val;
    } else {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  } else {
    /* We want offset += incr_val.  The overflow check is for integer 
       overflow.  To check
         offset + incr_val <= LONG_MAX
       we check
         offset <= LONG_MAX - incr_val
       where the subtraction can't overflow.
    */
    if (offset <= (long)(LONG_MAX - incr_val)) {
      offset += incr_val;
    } else {
      *err_code = ec_integer_overflow;
      *err_severity = es_error;
    }  /* if */
  }  /* if */

  if (*err_code == ec_no_error) {
    /* Build the result pointer constant. */
    copy_constant(constant_1, result);
    set_pointer_offset(result, offset);
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

  if (*err_code != ec_no_error && *err_severity == es_error) {
    set_error_constant(result);
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
  a_targ_ptrdiff_t offset_1, offset_2, difference;

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;
  /* The two pointers must be in the same base object, or the operation
     cannot be folded. */
  if (base_object(constant_1) != base_object(constant_2)) {
    *did_not_fold = TRUE;
  } else {
    /* The pointers are in the same base object, so the difference of
       their offsets can be taken.  It's a signed quantity.  Note that
       since the offsets are kept as byte offsets, there's no need to
       divide by the size of the elements pointed to. */
    offset_1 = pointer_offset(constant_1);
    offset_2 = pointer_offset(constant_2);
    if (subtract_protected(offset_1, offset_2, &difference) &&
        difference >= TARG_PTRDIFF_T_MIN &&
        difference <= TARG_PTRDIFF_T_MAX) {
      set_constant_kind(result, (a_constant_repr_kind)ck_integer);
      result->variant.integer_value = difference;
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
  a_targ_ptrdiff_t offset_1, offset_2;
  long             result_value;

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
    offset_1 = pointer_offset(constant_1);
    offset_2 = pointer_offset(constant_2);
    switch (op) {
      case eok_peq:
        result_value = (offset_1 == offset_2);
        break;
      case eok_pne:
        result_value = (offset_1 != offset_2);
        break;
      case eok_pgt:
        result_value = (offset_1 > offset_2);
        break;
      case eok_plt:
        result_value = (offset_1 < offset_2);
        break;
      case eok_pge:
        result_value = (offset_1 >= offset_2);
        break;
      case eok_ple:
        result_value = (offset_1 <= offset_2);
        break;
#if CHECKING
      default:
        internal_error("do_pcompare: bad operator kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  if (!*did_not_fold) {
    set_constant_kind(result, (a_constant_repr_kind)ck_integer);
    result->variant.integer_value = result_value;
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


void binary_operation(an_expr_operator_kind op,
		      a_constant            *constant_1,
		      a_constant            *constant_2,
		      a_type_ptr            result_type,
		      a_constant            *result,
		      a_boolean             *did_not_fold,
		      an_error_code         *err_code,
		      an_error_severity     *err_severity)
/*
Fold a two-operand constant operation.  op indicates the operation,
and constant_1 and constant_2 are the operands.  result_type indicates
the desired result type.  The result constant is placed in *result.
*err_code and *err_severity are set to indicate any errors or warnings;
if there are none, *err_code is set to ec_no_error.  If the operation
cannot be folded, *did_not_fold is set to TRUE.
*/
{
  db_enter(5, "binary_operation");

  *did_not_fold = FALSE;
  *err_code = ec_no_error;
  *err_severity = es_warning;

  if ((constant_1->kind == (a_constant_repr_kind)ck_error) ||
      (constant_2->kind == (a_constant_repr_kind)ck_error)) {
    /* One and/or the other of the constants is an error constant; set the
       result to an error constant and return. */
    set_error_constant(result);
  } else {
    clear_constant(result, (a_constant_repr_kind)ck_error);
    result->type = result_type;
    if (constant_1->kind == (a_constant_repr_kind)ck_address &&
        constant_1->implicit_cast &&
        is_integral_type(constant_1->type)) {
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
        if (!is_integral_type(constant_2->type)) {
          internal_error("binary_operation: address constant +- non-integer");
        }  /* if */
#endif /* CHECKING */
        do_padd(constant_1, op, constant_2, result, err_code,
                err_severity);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else if (constant_2->kind == (a_constant_repr_kind)ck_address &&
               constant_2->implicit_cast &&
               is_integral_type(constant_2->type)) {
      /* The second constant is an address constant cast to an integral
         type.  Such a constant is a link-time constant, not a compile-time
         constant.  We cannot in general do operations on it.  However,
         we can handle the special case
           int_constant + (int)addr_constant
         by using the pointer add routine. */
      if (op == (an_expr_operator_kind)eok_iadd &&
          constant_1->kind == (a_constant_repr_kind)ck_integer) {
#if CHECKING
        if (!is_integral_type(constant_1->type)) {
          internal_error("binary_operation: non-integer + address constant");
        }  /* if */
#endif /* CHECKING */
        /* Note that we reverse the operands in the call so that the address
           constant is first. */
        do_padd(constant_2, op, constant_1, result, err_code,
                err_severity);
      } else {
        *did_not_fold = TRUE;
      }  /* if */
    } else {
      switch (op) {
        case eok_iadd:
          do_iadd(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_isubtract:
          do_isubtract(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_imultiply:
          do_imultiply(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_remainder:
          do_remainder(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_idivide:
          do_idivide(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_shiftl:
          do_shiftl(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_shiftr:
          do_shiftr(constant_1, constant_2, result, err_code, err_severity);
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
          do_land(constant_1, constant_2, result);
          break;
        case eok_lor:
          do_lor(constant_1, constant_2, result);
          break;
        case eok_fadd:
          do_fadd(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_fsubtract:
          do_fsubtract(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_fmultiply:
          do_fmultiply(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_fdivide:
          do_fdivide(constant_1, constant_2, result, err_code, err_severity);
          break;
        case eok_feq:
        case eok_fne:
        case eok_fgt:
        case eok_flt:
        case eok_fge:
        case eok_fle:
          do_fcompare(constant_1, op, constant_2, result);
          break;
        case eok_pdiff:
          do_pdiff(constant_1, constant_2, result, did_not_fold, err_code,
                   err_severity);
          break;
        case eok_padd:
        case eok_padd_subsc:
        case eok_psubtract:
          do_padd(constant_1, op, constant_2, result, err_code,
                  err_severity);
          break;
        case eok_pge:
        case eok_plt:
        case eok_pgt:
        case eok_pne:
        case eok_peq:
        case eok_ple:
          do_pcompare(constant_1, op, constant_2, result, did_not_fold,
                      err_code, err_severity);
          break;
#if CHECKING
        default:
          internal_error("binary_operation: bad binary operator");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */

    if (*err_code != ec_no_error && *err_severity == es_error) {
      set_error_constant(result);
    }  /* if */

  }  /* if */

  db_exit();
}  /* binary_operation */


void fold_field_selection(a_constant            *constant_1,
                          a_field_ptr           field,
                          a_type_ptr            result_type,
                          a_constant            *result,
                          a_boolean             *did_not_fold)
/*
Fold a constant field selection operation.  constant_1 is the pointer to the
struct/union, field points to the field.  The result type (pointer to the
field type) is given by result_type.  The result is put in *result.
*did_not_fold is set to TRUE if the operation cannot be folded.
This folding operation is not done through the usual interface because a
field cannot be passed as a constant.
*/
{
  a_targ_ptrdiff_t offset;

  *did_not_fold = FALSE;
  copy_constant(constant_1, result);
  if (constant_1->kind == (a_constant_repr_kind)ck_error) {
    /* An error constant stays the same. */
  } else if (field->bit_size != 0) {
    /* Cannot fold bit-field selection. */
    *did_not_fold = TRUE;
  } else {
    /* Take the pointer offset, ... */
    offset = pointer_offset(constant_1);
    /* ... add the offset of the field (converting from bits to bytes), ... */
    offset += field->bit_offset / TARG_CHAR_BIT;
    /* ... and put the offset into the result pointer constant.  Note that
       no overflow/object-size checking is needed, since the field has
       to be within the underlying object. */
    set_pointer_offset(result, offset);
    implicit_cast(result, result_type);
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "fold_field_selection: ");
    if (*did_not_fold) {
      fprintf(f_debug, "did not fold\n");
    } else {
      fprintf(f_debug, "offset = %lu\n", (unsigned long)offset);
    }  /* if */
  }  /* if */
#endif /* CHECKING */
}  /* fold_field_selection */


void fold_base_class_cast(a_constant   *constant_1,
                          a_base_class *base_class,
                          a_constant   *result,
                          a_boolean    *did_not_fold)
/*
Fold a C++ cast of a class pointer to a base class pointer.  constant_1 is
an address of a class object.  It is converted to point to the class
indicated by base_class, and the new constant is returned in *result.
If the operation cannot be folded, *did_not_fold is returned TRUE.
*/
{
  a_targ_ptrdiff_t offset;

  *did_not_fold = FALSE;
  copy_constant(constant_1, result);
  if (constant_1->kind == (a_constant_repr_kind)ck_error) {
    /* An error constant stays the same. */
  } else {
    offset = pointer_offset(constant_1);
    if (offset == 0 && base_object(constant_1) == NULL) {
      /* Preserve a NULL pointer. */
    } else {
      if (base_class->is_virtual) {
        /* Casting to a virtual base class.  This can only be folded if we
           have a whole object of the derived class type. */
        *did_not_fold = TRUE;
        if (constant_1->kind == (a_constant_repr_kind)ck_address &&
            constant_1->variant.address.kind ==
                                          (an_address_base_kind)abk_variable &&
            offset == 0 &&
            !constant_1->implicit_cast) {
          /* The constant is the unmodified address of a variable. */
          a_variable_ptr variable =
                                  constant_1->variant.address.variant.variable;
          a_type_ptr     var_type = skip_typerefs(variable->type);
          if (is_class_struct_union_type(var_type)) {
            /* The constant is the address of a class variable. */
            *did_not_fold = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!*did_not_fold) {
        /* Take the pointer offset, ... */
        /* ... add the offset to the base class, ... */
        offset += base_class->offset;
        /* ... and put the offset into the result pointer constant.  Note that
           no overflow/object-size checking is needed, since the base class has
           to be within the underlying object. */
        set_pointer_offset(result, offset);
        implicit_cast(result, make_pointer_type(base_class->type));
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "fold_base_class_cast: ");
    if (*did_not_fold) {
      fprintf(f_debug, "did not fold\n");
    } else {
      fprintf(f_debug, "offset = %lu\n", (unsigned long)offset);
    }  /* if */
  }  /* if */
#endif /* CHECKING */
}  /* fold_base_class_cast */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
