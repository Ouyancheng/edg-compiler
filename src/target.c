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

target.c -- Target configuration support

*/

#include "basic_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if CHECKING

/* Header files common to all files. */
#include "fe_common.h"

void check_target_configuration(void)
/*
Perform consistency check on target configuration variables.
*/
{
  a_targ_size_t    size, size_max_value;
  a_targ_alignment alignment;
  a_targ_ptrdiff_t diff_max_value;
  a_boolean        err;

  /* The target char may be no bigger than the host long. */
  get_integer_size_and_alignment((an_integer_kind)ik_char,
                                 &size, &alignment);
  if (size > sizeof(long)) {
    internal_error("check_target_config: target char is too large");
  }  /* if */
  /* The target wchar_t may be no bigger than the host long. */
  get_integer_size_and_alignment((an_integer_kind)targ_wchar_t_int_kind,
                                 &size, &alignment);
  if (size > sizeof(long)) {
    internal_error("check_target_config: target wchar_t is too large");
  }  /* if */
  /* targ_wchar_t_int_kind and targ_sizeof_wchar_t must be in consistent:
     if one is changed, the other should be changed, too. */
  if (size != targ_sizeof_wchar_t) {
    internal_error("check_target_config: target wchar_t size is inconsistent");
  }  /* if */
  /* targ_size_t_max must fit in the target integer type targ_size_t_int_kind
     (but it need not fit exactly). */
  get_integer_size_and_alignment((an_integer_kind)targ_size_t_int_kind,
                                 &size, &alignment);
  size *= targ_char_bit;
  if (size > sizeof(a_targ_size_t)*CHAR_BIT) {
    size = sizeof(a_targ_size_t)*CHAR_BIT;
  }  /* if */
  /* Make a mask of "size" 1 bits for the maximum value. */
  size_max_value = ((((a_targ_size_t)1 << (size-1))-1) << 1);
  /* Final "or" done separately to avoid a bug in Borland C++ 3.0 with -O. */
  size_max_value |= 1;
  if (size_max_value < targ_size_t_max) {
    internal_error("check_target_config: targ_size_t_max is too large");
  }  /* if */
  /* targ_ptrdiff_t_max and targ_ptrdiff_t_min must fit in the target
     integer type targ_ptrdiff_t_int_kind (but they need not fit exactly). */
  get_integer_size_and_alignment((an_integer_kind)targ_ptrdiff_t_int_kind,
                                 &size, &alignment);
  size *= targ_char_bit;
  if (size > sizeof(a_targ_ptrdiff_t)*CHAR_BIT) {
    size = sizeof(a_targ_ptrdiff_t)*CHAR_BIT;
  }  /* if */
  /* Make a mask of "size-1" 1 bits for the maximum value. */
  diff_max_value = ((((a_targ_ptrdiff_t)1 << (size-2))-1) << 1);
  /* Final "or" done separately to avoid a bug in Borland C++ 3.0 with -O. */
  diff_max_value |= 1;
  if (diff_max_value < targ_ptrdiff_t_max) {
    internal_error("check_target_config: targ_ptrdiff_t_max is too large");
  }  /* if */
  /* This test depends on a two's complement representation. */
  if (-targ_ptrdiff_t_max-1 != targ_ptrdiff_t_min) {
    internal_error("check_target_config: invalid targ_ptrdiff_t_min");
  }  /* if */
#if LONG_LONG_ALLOWED
  if (TARG_SIZEOF_LARGEST_INTEGER < targ_sizeof_long_long) {
    internal_error("check_target_config: invalid TARG_SIZEOF_LARGEST_INTEGER");
  }  /* if */
#else /* !LONG_LONG_ALLOWED */
  if (TARG_SIZEOF_LARGEST_INTEGER < targ_sizeof_long) {
    internal_error("check_target_config: invalid TARG_SIZEOF_LARGEST_INTEGER");
  }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  /* When using host integers to represent target integers, make sure the
     host integer selected is large enough. */
  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (TARG_SIZEOF_LARGEST_INTEGER*targ_char_bit >
         sizeof(an_integer_value)*CHAR_BIT);
  if (err) {
    internal_error("check_target_config: an_integer_value is too small");
  }  /* if */
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  /* When using the simulated large integer approach to represent target
     integers, the sizes must be right. */

  /* Use variable err instead of testing directly to avoid warnings about
     testing invariant values on some compilers. */
  err = (BITS_IN_HOST_LARGE_INTEGER != sizeof(a_host_large_integer)*CHAR_BIT);
  if (err) {
    internal_error("check_target_config: invalid BITS_IN_HOST_LARGE_INTEGER");
  }  /* if */
  err = (SIZEOF_INT_VALUE_PART > sizeof(an_int_value_part));
  if (err) {
    internal_error("check_target_config: invalid SIZEOF_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART != SIZEOF_INT_VALUE_PART*CHAR_BIT ||
         2*BITS_IN_INT_VALUE_PART > BITS_IN_HOST_LARGE_INTEGER);
  if (err) {
    internal_error("check_target_config: invalid BITS_IN_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART*INT_VALUE_PARTS_PER_INTEGER_VALUE !=
         TARG_SIZEOF_LARGEST_INTEGER*targ_char_bit);
  if (err) {
    internal_error(
             "check_target_config: invalid INT_VALUE_PARTS_PER_INTEGER_VALUE");
  }  /* if */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
  if (targ_host_string_char_bit > CHAR_BIT) {
    internal_error("check_target_config: targ_host_string_char_bit too large");
  }  /* if */

}  /* check_target_configuration */

#endif /* CHECKING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
