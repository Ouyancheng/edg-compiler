/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Linux.
*/

/*
A word of caution about using TRUE/FALSE values in this header file: The
macros TRUE and FALSE are defined in basics.h, but their definition occurs
only after defines.h is included.  A side-effect of this is that any #if
test of a macro whose value is either TRUE or FALSE will result in the false
branch being taken.  Consequently, it's best to use 1 or 0 instead of TRUE and
FALSE in this header file.
*/

/* Avoid including these declarations more than once. */
#ifndef DEFINES_LINUX_H
#define DEFINES_LINUX_H 1

#ifdef DEMO_VERSION
/* Demo versions should support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define COMPILE_MULTIPLE_SOURCE_FILES 0
#define DEBUG 0
#define EMBEDDED_C_ALLOWED 1
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* ifdef DEMO_VERSION */

#define __ANSIC__ 1
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */

/*
When compiling with __STDC__ non-zero, set _BSD_SOURCE to get the
declarations needed for the mmap routines.
*/
#if __STDC__ != 0
#define _BSD_SOURCE
#endif /* __STDC__ != 0 */

/*
TARG_SUPPORTS_X86_64 should be set when targeting the x86-64 variant of the x86
platform.
*/
#ifndef TARG_SUPPORTS_X86_64
#ifdef __x86_64
#define TARG_SUPPORTS_X86_64 1
#else /* ifndef __x86_64 */
#define TARG_SUPPORTS_X86_64 0
#endif /* ifdef __x86_64 */
#endif /* ifndef TARG_SUPPORTS_X86_64 */

#ifndef BUILTIN_FUNCTIONS_ENABLED
#define BUILTIN_FUNCTIONS_ENABLED 1
#endif /* ifndef BUILTIN_FUNCTIONS_ENABLED */

/*
Configure the legacy configuration as 32-bit or 64-bit (depending on the
value of TARG_SUPPORTS_X86_64).  An additional configuration (i.e., the
"other" one of 32-bit or 64-bit) will be defined below.
*/
#if TARG_SUPPORTS_X86_64
#define TARG_SIZEOF_LONG 8
#define TARG_ALIGNOF_LONG 8
#define TARG_SIZEOF_POINTER 8
#define TARG_ALIGNOF_POINTER 8
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 16
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_int)
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_INT
#endif /* ifndef _lint */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#define HOST_ALIGNMENT_REQUIRED 8
#define TYPE_FOR_AN_FP_VALUE_PART unsigned int
#define TARG_JMP_BUF_NUM_ELEMENTS 25
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_long)
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES 0
#else /* TARG_SUPPORTS_X86_64 */
#ifdef __x86_64
/* Building a 32 bit target configuration on a 64 bit host. */
#define TYPE_FOR_AN_FP_VALUE_PART unsigned int
#endif /* ifdef __x86_64 */
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 39
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 1
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#endif /* ifndef _lint */
/* double and long long have two different alignments on Linux. */
#define TARG_DOUBLE_FIELD_ALIGNMENT 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT 4
#endif /* TARG_SUPPORTS_X86_64 */

/*
wint_t has a different signedness from wchar_t with both 32-bit and 64-bit
Linux.
*/
#define TARG_WINT_T_INT_KIND ((an_integer_kind)ik_unsigned_int)

#define TARG_ALIGNOF_DOUBLE 8
#define TARG_ALIGNOF_LONG_LONG 8

#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE 1
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#define BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME "__gnuc_va_list"
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE 1
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#ifndef GNU_EXTENSIONS_ALLOWED
#define GNU_EXTENSIONS_ALLOWED 1
#endif /* ifndef GNU_EXTENSIONS_ALLOWED */
#if defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED 1
#endif /* defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#if !defined(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS)
#define RECORD_RAW_ASM_OPERAND_DESCRIPTIONS 0
#endif /* !defined(RECORD_RAW_ASM_OPERAND_DESCRIPTIONS) */
#define DEFAULT_GNU_COMPATIBILITY 0
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 1
#ifndef IA64_ABI
#define IA64_ABI 1
#endif /* IA64_ABI */

/* Settings needed in order for bit-field allocation to match gcc. */
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED 0
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED 1

#define LONG_LONG_ALLOWED 1
#ifndef INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 1
#endif /* ifndef INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long long
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long long
#define PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE   "%lld"
#define PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE "%llu"
#define PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE      "%llx"
#define MAX_INTEGER_VALUE 9223372036854775807LL
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#define MAX_UNSIGNED_INTEGER_VALUE 18446744073709551615ULL

/* By default, don't include the "other" target configuration. */
#ifndef INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
#define INCLUDE_ADDITIONAL_TARGET_CONFIGURATION 0
#endif /* defined(INCLUDE_ADDITIONAL_TARGET_CONFIGURATION) */

#if INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
/*
The legacy configuration (either a 32-bit or a 64-bit configuration as dictated
by the setting of TARG_SUPPORTS_X86_64) has been defined above.  Give that
target configuration the appropriate name, and define the "other" target
configuration.  Note that two sets of configurations are included here, one set
for the IA-64 ABI and one set for the Cfront ABI.  Note also that these target
configurations are primarily for demonstration purposes as the actual set of
target-specific configuration macros depends on the set of features that have
been selected, and some of the values here may not be correct for your
configuration(s).  These sets of target-specific configuration macros were
created using the --dump_legacy_as_target command-line option; additional
configurations can be created in the same manner.
*/

#if TARG_SUPPORTS_X86_64
#define LEGACY_TARGET_CONFIGURATION_NAME "linux_x86_64"
#else /* !TARG_SUPPORTS_X86_64 */
#define LEGACY_TARGET_CONFIGURATION_NAME "linux_i686"
#endif /* TARG_SUPPORTS_X86_64 */

/*
Don't specify a default configuration (this leaves the legacy configuration
as the default and doesn't require a name change for $EDG_BASE/lib unless
the --target option is used).
*/
#undef DEFAULT_TARGET_CONFIGURATION_NAME

#if IA64_ABI

#if TARG_SUPPORTS_X86_64

/* "Other" target is IA-64 ABI 32-bit configuration. */
/* Target configuration: linux_i686 */
#define TARGET_CONFIGURATION_1 linux_i686
#define TARG_ALIGNOF_DOUBLE_linux_i686 8
#define TARG_ALIGNOF_FAR_POINTER_linux_i686 4
#define TARG_ALIGNOF_FLOAT_linux_i686 4
#define TARG_ALIGNOF_FLOAT128_linux_i686 16
#define TARG_ALIGNOF_FLOAT80_linux_i686 4
#define TARG_ALIGNOF_INT_linux_i686 4
#define TARG_ALIGNOF_INT128_linux_i686 16
#define TARG_ALIGNOF_LONG_linux_i686 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_i686 4
#define TARG_ALIGNOF_LONG_LONG_linux_i686 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_i686 2
#define TARG_ALIGNOF_POINTER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_i686 4
#define TARG_ALIGNOF_SHORT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_i686 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_i686 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_i686 (-1)
#define TARG_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_i686 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_i686 1
#define TARG_DBL_MANT_DIG_linux_i686 53
#define TARG_DBL_MAX_EXP_linux_i686 1024
#define TARG_DBL_MIN_EXP_linux_i686 (-1021)
#define TARG_DELTA_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_i686 1
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_i686 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_i686 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_i686 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLT_MANT_DIG_linux_i686 24
#define TARG_FLT_MAX_EXP_linux_i686 128
#define TARG_FLT_MIN_EXP_linux_i686 (-125)
#define TARG_FLT128_MANT_DIG_linux_i686 113
#define TARG_FLT128_MAX_EXP_linux_i686 (16384)
#define TARG_FLT128_MIN_EXP_linux_i686 (-16381)
#define TARG_FLT80_MANT_DIG_linux_i686 64
#define TARG_FLT80_MAX_EXP_linux_i686 (16384)
#define TARG_FLT80_MIN_EXP_linux_i686 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_i686 0
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_i686 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_i686 8
#define TARG_HAS_SIGNED_CHARS_linux_i686 1
#define TARG_HOST_STRING_CHAR_BIT_linux_i686 8
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_INT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_i686 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_i686 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_i686 39
#define TARG_LDBL_MANT_DIG_linux_i686 64
#define TARG_LDBL_MAX_EXP_linux_i686 16384
#define TARG_LDBL_MIN_EXP_linux_i686 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_i686 0
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_i686 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_i686 128
#define TARG_MAX_BASE_CLASS_OFFSET_linux_i686 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_i686 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_i686 0
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_i686 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_i686 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_i686 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_i686 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_i686 0
#define TARG_POINTER_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_REUSE_TAIL_PADDING_linux_i686 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_i686 1
#define TARG_SHORT_FIELD_ALIGNMENT_linux_i686 2
#define TARG_SIZEOF_DOUBLE_linux_i686 8
#define TARG_SIZEOF_FAR_POINTER_linux_i686 4
#define TARG_SIZEOF_FLOAT_linux_i686 4
#define TARG_SIZEOF_FLOAT128_linux_i686 16
#define TARG_SIZEOF_FLOAT80_linux_i686 12
#define TARG_SIZEOF_INT_linux_i686 4
#define TARG_SIZEOF_INT128_linux_i686 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_i686 8
#define TARG_SIZEOF_LONG_linux_i686 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_i686 12
#define TARG_SIZEOF_LONG_LONG_linux_i686 8
#define TARG_SIZEOF_NEAR_POINTER_linux_i686 2
#define TARG_SIZEOF_POINTER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_i686 (4+4)
#define TARG_SIZEOF_SHORT_linux_i686 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_SIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_i686 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_X86_64_linux_i686 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_i686 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 0
#define TARG_UNWIND_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_i686 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_i686 1
#define TARG_VAR_HANDLE_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_i686 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_i686 ((an_integer_kind)ik_long)
#define TARG_WINT_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_i686 (-1)

#else /* !TARG_SUPPORTS_X86_64 */

/* "Other" target is IA-64 ABI 64-bit configuration. */
/* Target configuration: linux_x86_64 */
#define TARGET_CONFIGURATION_1 linux_x86_64
#define TARG_ALIGNOF_DOUBLE_linux_x86_64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT128_x86_64 16
#define TARG_ALIGNOF_FLOAT80_x86_64 4
#define TARG_ALIGNOF_INT_linux_x86_64 4
#define TARG_ALIGNOF_INT128_linux_x86_64 16
#define TARG_ALIGNOF_LONG_linux_x86_64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_ALIGNOF_LONG_LONG_linux_x86_64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_x86_64 2
#define TARG_ALIGNOF_POINTER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 8
#define TARG_ALIGNOF_SHORT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_x86_64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_x86_64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_x86_64 (-1)
#define TARG_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_x86_64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_x86_64 1
#define TARG_DBL_MANT_DIG_linux_x86_64 53
#define TARG_DBL_MAX_EXP_linux_x86_64 1024
#define TARG_DBL_MIN_EXP_linux_x86_64 (-1021)
#define TARG_DELTA_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_x86_64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_x86_64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_x86_64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_x86_64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_x86_64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_x86_64 4
#define TARG_FLT_MANT_DIG_linux_x86_64 24
#define TARG_FLT_MAX_EXP_linux_x86_64 128
#define TARG_FLT_MIN_EXP_linux_x86_64 (-125)
#define TARG_FLT128_MANT_DIG_x86_64 113
#define TARG_FLT128_MAX_EXP_x86_64 (16384)
#define TARG_FLT128_MIN_EXP_x86_64 (-16381)
#define TARG_FLT80_MANT_DIG_x86_64 64
#define TARG_FLT80_MAX_EXP_x86_64 (16384)
#define TARG_FLT80_MIN_EXP_x86_64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_x86_64 0
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_x86_64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_x86_64 8
#define TARG_HAS_SIGNED_CHARS_linux_x86_64 1
#define TARG_HOST_STRING_CHAR_BIT_linux_x86_64 8
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_INT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_x86_64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_x86_64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_x86_64 25
#define TARG_LDBL_MANT_DIG_linux_x86_64 64
#define TARG_LDBL_MAX_EXP_linux_x86_64 16384
#define TARG_LDBL_MIN_EXP_linux_x86_64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_x86_64 0
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_x86_64 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_x86_64 128
#define TARG_MAX_BASE_CLASS_OFFSET_linux_x86_64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_x86_64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_x86_64 0
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_x86_64 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_x86_64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_x86_64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_x86_64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_x86_64 0
#define TARG_POINTER_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_REUSE_TAIL_PADDING_linux_x86_64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_x86_64 1
#define TARG_SHORT_FIELD_ALIGNMENT_linux_x86_64 2
#define TARG_SIZEOF_DOUBLE_linux_x86_64 8
#define TARG_SIZEOF_FAR_POINTER_linux_x86_64 4
#define TARG_SIZEOF_FLOAT_linux_x86_64 4
#define TARG_SIZEOF_FLOAT128_x86_64 16
#define TARG_SIZEOF_FLOAT80_x86_64 12
#define TARG_SIZEOF_INT_linux_x86_64 4
#define TARG_SIZEOF_INT128_linux_x86_64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_x86_64 8
#define TARG_SIZEOF_LONG_linux_x86_64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_SIZEOF_LONG_LONG_linux_x86_64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_x86_64 2
#define TARG_SIZEOF_POINTER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 (8+8)
#define TARG_SIZEOF_SHORT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_SIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_x86_64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_X86_64_linux_x86_64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_x86_64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 0
#define TARG_UNWIND_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_x86_64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_x86_64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_x86_64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_x86_64 (-1)

#endif /* TARG_SUPPORTS_X86_64 */

#else /* !IA64_ABI */

#if TARG_SUPPORTS_X86_64

/* "Other" target is Cfront 32-bit configuration. */
/* Target configuration: linux_i686 */
#define TARGET_CONFIGURATION_1 linux_i686
#define TARG_ALIGNOF_DOUBLE_linux_i686 8
#define TARG_ALIGNOF_FAR_POINTER_linux_i686 4
#define TARG_ALIGNOF_FLOAT_linux_i686 4
#define TARG_ALIGNOF_FLOAT128_linux_i686 16
#define TARG_ALIGNOF_FLOAT80_linux_i686 4
#define TARG_ALIGNOF_INT_linux_i686 4
#define TARG_ALIGNOF_INT128_linux_i686 16
#define TARG_ALIGNOF_LONG_linux_i686 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_i686 4
#define TARG_ALIGNOF_LONG_LONG_linux_i686 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_i686 2
#define TARG_ALIGNOF_POINTER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_i686 4
#define TARG_ALIGNOF_SHORT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_i686 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_i686 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_i686 (-1)
#define TARG_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_i686 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_i686 1
#define TARG_DBL_MANT_DIG_linux_i686 53
#define TARG_DBL_MAX_EXP_linux_i686 1024
#define TARG_DBL_MIN_EXP_linux_i686 (-1021)
#define TARG_DELTA_INT_KIND_linux_i686 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_i686 1
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_i686 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_i686 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_i686 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLT_MANT_DIG_linux_i686 24
#define TARG_FLT_MAX_EXP_linux_i686 128
#define TARG_FLT_MIN_EXP_linux_i686 (-125)
#define TARG_FLT128_MANT_DIG_linux_i686 113
#define TARG_FLT128_MAX_EXP_linux_i686 (16384)
#define TARG_FLT128_MIN_EXP_linux_i686 (-16381)
#define TARG_FLT80_MANT_DIG_linux_i686 64
#define TARG_FLT80_MAX_EXP_linux_i686 (16384)
#define TARG_FLT80_MIN_EXP_linux_i686 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_i686 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_i686 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_i686 8
#define TARG_HAS_SIGNED_CHARS_linux_i686 1
#define TARG_HOST_STRING_CHAR_BIT_linux_i686 8
#define TARG_INT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_i686 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_i686 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_i686 39
#define TARG_LDBL_MANT_DIG_linux_i686 64
#define TARG_LDBL_MAX_EXP_linux_i686 16384
#define TARG_LDBL_MIN_EXP_linux_i686 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_i686 0
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_i686 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_i686 128
#define TARG_MAX_BASE_CLASS_OFFSET_linux_i686 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_i686 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_i686 0
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_i686 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_i686 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_i686 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_i686 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_i686 0
#define TARG_POINTER_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_i686 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_i686 2
#define TARG_SIZEOF_DOUBLE_linux_i686 8
#define TARG_SIZEOF_FAR_POINTER_linux_i686 4
#define TARG_SIZEOF_FLOAT_linux_i686 4
#define TARG_SIZEOF_FLOAT128_linux_i686 16
#define TARG_SIZEOF_FLOAT80_linux_i686 12
#define TARG_SIZEOF_INT_linux_i686 4
#define TARG_SIZEOF_INT128_linux_i686 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_i686 8
#define TARG_SIZEOF_LONG_linux_i686 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_i686 12
#define TARG_SIZEOF_LONG_LONG_linux_i686 8
#define TARG_SIZEOF_NEAR_POINTER_linux_i686 2
#define TARG_SIZEOF_POINTER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_i686 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_i686 4
#define TARG_SIZEOF_SHORT_linux_i686 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_SIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_i686 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_X86_64_linux_i686 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_i686 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 0
#define TARG_UNWIND_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_i686 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_i686 1
#define TARG_VAR_HANDLE_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_i686 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_i686 ((an_integer_kind)ik_long)
#define TARG_WINT_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_i686 4

#else /* !TARG_SUPPORTS_X86_64 */

/* "Other" target is Cfront 64-bit configuration. */
/* Target configuration: linux_x86_64 */
#define TARGET_CONFIGURATION_1 linux_x86_64
#define TARG_ALIGNOF_DOUBLE_linux_x86_64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT128_x86_64 16
#define TARG_ALIGNOF_FLOAT80_x86_64 4
#define TARG_ALIGNOF_INT_linux_x86_64 4
#define TARG_ALIGNOF_INT128_linux_x86_64 16
#define TARG_ALIGNOF_LONG_linux_x86_64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_ALIGNOF_LONG_LONG_linux_x86_64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_x86_64 2
#define TARG_ALIGNOF_POINTER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_x86_64 8
#define TARG_ALIGNOF_SHORT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_x86_64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_x86_64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_x86_64 (-1)
#define TARG_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_x86_64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_x86_64 1
#define TARG_DBL_MANT_DIG_linux_x86_64 53
#define TARG_DBL_MAX_EXP_linux_x86_64 1024
#define TARG_DBL_MIN_EXP_linux_x86_64 (-1021)
#define TARG_DELTA_INT_KIND_linux_x86_64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_x86_64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_x86_64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_x86_64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_x86_64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_x86_64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_x86_64 4
#define TARG_FLT_MANT_DIG_linux_x86_64 24
#define TARG_FLT_MAX_EXP_linux_x86_64 128
#define TARG_FLT_MIN_EXP_linux_x86_64 (-125)
#define TARG_FLT128_MANT_DIG_x86_64 113
#define TARG_FLT128_MAX_EXP_x86_64 (16384)
#define TARG_FLT128_MIN_EXP_x86_64 (-16381)
#define TARG_FLT80_MANT_DIG_x86_64 64
#define TARG_FLT80_MAX_EXP_x86_64 (16384)
#define TARG_FLT80_MIN_EXP_x86_64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_x86_64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_x86_64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_x86_64 8
#define TARG_HAS_SIGNED_CHARS_linux_x86_64 1
#define TARG_HOST_STRING_CHAR_BIT_linux_x86_64 8
#define TARG_INT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_x86_64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_x86_64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_x86_64 25
#define TARG_LDBL_MANT_DIG_linux_x86_64 64
#define TARG_LDBL_MAX_EXP_linux_x86_64 16384
#define TARG_LDBL_MIN_EXP_linux_x86_64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_x86_64 0
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_x86_64 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_x86_64 128
#define TARG_MAX_BASE_CLASS_OFFSET_linux_x86_64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_x86_64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_x86_64 0
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_x86_64 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_x86_64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_x86_64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_x86_64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_x86_64 0
#define TARG_POINTER_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_x86_64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_x86_64 2
#define TARG_SIZEOF_DOUBLE_linux_x86_64 8
#define TARG_SIZEOF_FAR_POINTER_linux_x86_64 4
#define TARG_SIZEOF_FLOAT_linux_x86_64 4
#define TARG_SIZEOF_FLOAT128_x86_64 16
#define TARG_SIZEOF_FLOAT80_x86_64 12
#define TARG_SIZEOF_INT_linux_x86_64 4
#define TARG_SIZEOF_INT128_linux_x86_64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_x86_64 8
#define TARG_SIZEOF_LONG_linux_x86_64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_SIZEOF_LONG_LONG_linux_x86_64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_x86_64 2
#define TARG_SIZEOF_POINTER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_x86_64 8
#define TARG_SIZEOF_SHORT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_SIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_x86_64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_X86_64_linux_x86_64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_x86_64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 0
#define TARG_UNWIND_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_x86_64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_x86_64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_x86_64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_x86_64 4

#endif /* TARG_SUPPORTS_X86_64 */

#endif /* IA64_ABI */

#endif /* INCLUDE_ADDITIONAL_TARGET_CONFIGURATION */

#endif /* ifndef DEFINES_LINUX_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

