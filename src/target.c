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

/* Header files common to all files. */
#include "fe_common.h"


#if STANDALONE_UTILITY_PROGRAM
static a_boolean plain_char_int_kind;
			/* Ordinarily in cmd_line.h, but not available
			   in standalone programs. */
#endif /* STANDALONE_UTILITY_PROGRAM */


void set_plain_char_int_kind(a_boolean plain_chars_are_signed)
/*
Set plain_char_int_kind, which indicates the integer kind for "plain"
(neither signed or unsigned) char.  plain_chars_are_signed indicates
whether it should be signed.
*/
{
  if (C_dialect == C_dialect_pcc ||
      (microsoft_mode && C_mode())) {
    /* In pcc mode, a "plain" char is the same as either "signed char"
       or "unsigned char".  Likewise in Microsoft C mode. */
    plain_char_int_kind = plain_chars_are_signed ?
                               (an_integer_kind)ik_signed_char :
                               (an_integer_kind)ik_unsigned_char;
  } else {
    /* In standard mode, a "plain" char is different than "signed char" and
       "unsigned char". */
    plain_char_int_kind = (an_integer_kind)ik_char;
  }  /* if */
}  /* set_plain_char_int_kind */

#if MICROSOFT_EXTENSIONS_ALLOWED

void init_microsoft_sized_int_types(void)
/*
Map __int8, __int16, __int32, and __int64 to the appropriate integer
kinds.  Leave the variables set to ik_none if a match can't be found;
only if a corresponding integer kind is found will the corresponding
keyword be entered into the symbol table.
*/
{
#if STANDALONE_UTILITY_PROGRAM
  /* The signedness of characters can be set on the command line, but for
     standalone utilities, we determine this from the IL header. */
  set_plain_char_int_kind(il_header.plain_chars_are_signed);
#endif /* STANDALONE_UTILITY_PROGRAM */
  /* Map __int8 to plain char if and only if 8-bit chars are being used. */
  if (targ_char_bit == 8) {
    targ_int8_int_kind = plain_char_int_kind;
    targ_unsigned_int8_int_kind = (an_integer_kind)ik_unsigned_char;
  }  /* if */
  /* For the other cases, find the first integer kinds, signed and unsigned,
     that hold exactly 16, 32 and 64 bits, respectively. */  
  targ_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/TRUE);
  if (targ_int16_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int16_int_kind = int_kind_for_bit_size(16, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int16_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int16");
  }  /* if */
  targ_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/TRUE);
  if (targ_int32_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int32_int_kind = int_kind_for_bit_size(32, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int32_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int32");
  }  /* if */
  targ_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/TRUE);
  if (targ_int64_int_kind != (an_integer_kind)ik_none) {
    targ_unsigned_int64_int_kind = int_kind_for_bit_size(64, /*signed=*/FALSE);
    check_assertion_str(targ_unsigned_int64_int_kind !=
                                              (an_integer_kind)ik_none,
                       "target_init: can't set int kind for unsigned __int64");
  }  /* if */
}  /* init_microsoft_sized_int_types */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if TARG_ALL_POINTERS_SAME_SIZE
/*ARGSUSED*/ /* Because tp is not used. */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
a_targ_size_t size_of_pointer_to(a_type_ptr        tp,
                                 a_targ_alignment  *alignment)
/*
Return the size and alignment for a pointer type that points to the indicated
type.  This routine should be rewritten for implementations in which
TARG_ALL_POINTERS_SAME_SIZE may not always be TRUE.
*/
{
  a_targ_size_t size;

#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Pointers come in "near" and "far" sizes (e.g., Microsoft 16-bit
       mode). */
    if (is_far_type(tp)) {
      size = targ_sizeof_far_pointer;
      *alignment = targ_alignof_far_pointer;
    } else {
      size = targ_sizeof_near_pointer;
      *alignment = targ_alignof_near_pointer;
    }  /* if */
  } else
#endif /* NEAR_AND_FAR_ALLOWED */
  /* Do not add code here. */
  {
#if TARG_ALL_POINTERS_SAME_SIZE
    /* All pointers have the same size and alignment. */
    size = targ_sizeof_pointer;
    *alignment = targ_alignof_pointer;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
 #error -- code must be added here to determine the size of pointers.
/* If you set TARG_ALL_POINTERS_SAME_SIZE FALSE only because you want 
   to support near/far, see the comments on TARG_ALL_POINTERS_SAME_SIZE
   in targ_def.h and the internal documentation; it's probably not what
   you want. */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  }
  return size;
}  /* size_of_pointer_to */

#if CHECKING

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
  /* targ_wchar_t_int_kind and targ_sizeof_wchar_t must be consistent:
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
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid BITS_IN_HOST_LARGE_INTEGER");
  }  /* if */
  err = (SIZEOF_INT_VALUE_PART > sizeof(an_int_value_part));
  if (err) { /*lint !e774*/
    internal_error("check_target_config: invalid SIZEOF_INT_VALUE_PART");
  }  /* if */
  err = (BITS_IN_INT_VALUE_PART != SIZEOF_INT_VALUE_PART*CHAR_BIT ||
         2*BITS_IN_INT_VALUE_PART > BITS_IN_HOST_LARGE_INTEGER); /*lint !e506*/
  if (err) { /*lint !e774*/
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
#if USER_CONTROL_OF_STRUCT_PACKING
  /* Be sure the maximum and minimum values for "pack alignment" are
     appropriate and may be stored within a_targ_alignment, which is a_byte
     (= unsigned char). */
  if (targ_minimum_pack_alignment < 1 ||
      targ_minimum_pack_alignment > UCHAR_MAX) { /*lint !e685*/
    internal_error("check_target_config: invalid targ_minimum_pack_alignment");
  }  /* if */
  if (targ_maximum_pack_alignment < targ_minimum_pack_alignment ||
      (a_targ_alignment)targ_maximum_pack_alignment !=
                                                targ_maximum_pack_alignment) {
    internal_error("check_target_config: invalid targ_maximum_pack_alignment");
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

}  /* check_target_configuration */

#endif /* CHECKING */

void target_one_time_init(void)
/*
Do one-time initialization of variables related to the target.  This is
executed once after command-line processing, and not again for each source
file.
*/
{
#if CHECKING
  check_target_configuration();
#endif /* CHECKING */
}  /* target_one_time_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
