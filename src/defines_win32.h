/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Windows 95/98/NT/etc.
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
#ifndef DEFINES_WIN32_H
#define DEFINES_WIN32_H 1

#ifdef DEMO_VERSION
#define DEBUG 0
#ifndef CPPCLI_ENABLING_POSSIBLE
#define CPPCLI_ENABLING_POSSIBLE 1
#endif /* ifndef CPPCLI_ENABLING_POSSIBLE */
#ifndef CP_GEN_BE_VERSION
/* Demo versions that do not use the C++-generating back end should
   support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
/* Allow lowering with CPPCLI_ENABLING_POSSIBLE. */
#ifndef ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING
#if CPPCLI_ENABLING_POSSIBLE
#define ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING 1
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* ifndef ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING */
#endif /* ifndef CP_GEN_BE_VERSION */
#endif /* ifdef DEMO_VERSION */

#define FRONT_END_C_FILES_COMPILED_AS_CPP 0

/* Configuration definitions determined by dettarg.c: */
#define TARG_LITTLE_ENDIAN 1
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS 1
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 8
#define TARG_ALIGNOF_LONG_DOUBLE 8
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 16
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)

/*
TARG_SUPPORTS_X86_64 should be set when targeting the x86-64 variant of the x86
platform in the legacy configuration.  As a heuristic, define it when _WIN64
is defined.
*/
#ifndef TARG_SUPPORTS_X86_64
#ifdef _WIN64
#define TARG_SUPPORTS_X86_64 1
#else /* !_WIN64 */
#define TARG_SUPPORTS_X86_64 0
#endif /* _WIN64 */
#endif /* ifndef TARG_SUPPORTS_X86_64 */

#if TARG_SUPPORTS_X86_64
#define TARG_SIZEOF_POINTER 8
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long_long)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long_long)
#define HOST_ALIGNMENT_REQUIRED 8
#define HOST_POINTER_ALIGNMENT 8
#define TARG_ALIGNOF_POINTER 8
#else /* !TARG_SUPPORTS_X86_64 */
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
#endif /* TARG_SUPPORTS_X86_64 */

/*
Definitions for Windows (WIN32)
*/
#define __ANSIC__ 1
#define USING_ISO_C 1
#define C_GEN_BE_GENERATES_ANSI_C 1
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#define CFRONT_3_0_OBJECT_CODE_COMPATIBILITY 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST_DEFINED"
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE 1
#define DEFAULT_VA_LIST_IN_STD_NAMESPACE 0
#ifndef DEFAULT_EXCEPTIONS_ENABLED
#define DEFAULT_EXCEPTIONS_ENABLED 1
#endif /* ifndef DEFAULT_EXCEPTIONS_ENABLED */
#define UNICODE_SOURCE_SUPPORTED 1
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#ifndef NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE
#define NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE 1
#endif /* NATIVE_MULTIBYTE_CHARS_SUPPORTED_WITH_UNICODE */
#define DEFAULT_MULTIBYTE_CHARS_IN_SOURCE_ENABLED 1
#ifndef RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK
#define RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK 0
#endif /* RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK */

#ifndef DEFAULT_USE_PREDEFINED_MACRO_FILE
/*
Assume that a predefined_macros.txt file should be used (to define target-
specific macros such as _M_IX86 and _M_X64, etc. as appropriate for the
target).
*/
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 1
#endif /* defined(DEFAULT_USE_PREDEFINED_MACRO_FILE) */
/*
Use fixed address for mmap to work around issues with address space
layout randomization (ASLR) on Windows Vista.
*/
#define USE_FIXED_ADDRESS_FOR_MMAP 1
#define FIXED_ADDRESS_FOR_MMAP 0x21000000

#ifndef CP_GEN_BE_VERSION
/*
Use this define if the type_info from the EDG runtime library is to
be used.  This is enabled by default in versions that generate C code
as such versions cannot link with the Microsoft C++ libraries.
*/
#define MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD 1
#endif /* ifdef CP_GEN_BE_VERSION */


/* The EDG driver on NT does not support one instantiation per object mode. */
#define ONE_INSTANTIATION_PER_OBJECT 0

#ifndef OPTIMIZED_VERSION
#define OPTIMIZED_VERSION 1
#endif /* !defined(OPTIMIZED_VERSION) */
#if OPTIMIZED_VERSION
#define IL_SHOULD_BE_WRITTEN_TO_FILE 0
#else /* !OPTIMIZED_VERSION */
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define ALTERNATE_IL_FILE_FORMAT 0
#endif /* OPTIMIZED_VERSION */

#ifdef CP_GEN_BE_VERSION
/*
Flags to be set for any version that uses the C++ generating back end.
*/
#define BACK_END_IS_C_GEN_BE 0
#define BACK_END_IS_CP_GEN_BE 1
#define DO_IL_LOWERING 0
#define AUTOMATIC_TEMPLATE_INSTANTIATION 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define DELETE_CAN_BE_FOLDED_INTO_DTOR 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#define PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED 0
#define DEFAULT_TYPE_INFO_IN_NAMESPACE_STD 0
#endif /* ifdef CP_GEN_BE_VERSION */

/* Suppress Microsoft 8.0 warnings about deprecated C library functions. */
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE

/* By default, don't include the "other" target configuration. */
#ifndef INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
#define INCLUDE_ADDITIONAL_TARGET_CONFIGURATION 0
#endif /* defined(INCLUDE_ADDITIONAL_TARGET_CONFIGURATION) */

#if INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
/*
The legacy configuration (either a 32-bit or a 64-bit configuration as dictated
by the setting of TARG_SUPPORTS_X86_64) has been defined above.  Give that
target configuration the appropriate name (i.e., either "win32" or "win64"),
and define a target configuration for the "other" target.  Note that these
target configurations are primarily for demonstration purposes as the actual
set of target-specific configuration macros depends on the set of features that
have been selected, and some of the values here may not be correct for your
configuration(s).  These sets of target-specific configuration macros were
created using the --dump_legacy_as_target command-line option; additional
configurations can be created in the same manner.  Note that these target
configurations are specific to the Cfront ABI (though IA-64 ABI-specific
ones can be created with --dump_legacy_as_target).
*/
#if defined(IA64_ABI) && IA64_ABI
 #error Supplied target configurations cannot be used with IA64_ABI
#endif /* defined(IA64_ABI) && IA64_ABI */

#if TARG_SUPPORTS_X86_64
#define LEGACY_TARGET_CONFIGURATION_NAME "win64"
#else /* !TARG_SUPPORTS_X86_64 */
#define LEGACY_TARGET_CONFIGURATION_NAME "win32"
#endif /* TARG_SUPPORTS_X86_64 */

/*
Don't specify a default configuration (this leaves the legacy configuration
as the default and doesn't require a name change for $EDG_BASE/lib unless
the --target option is used).
*/
#undef DEFAULT_TARGET_CONFIGURATION_NAME

#if TARG_SUPPORTS_X86_64

/* "Other" target is Windows 32-bit configuration. */
/* Target configuration: win32 */
#define TARGET_CONFIGURATION_1 win32
#define TARG_ALIGNOF_DOUBLE_win32 8
#define TARG_ALIGNOF_FAR_POINTER_win32 4
#define TARG_ALIGNOF_FLOAT_win32 4
#define TARG_ALIGNOF_INT_win32 4
#define TARG_ALIGNOF_INT128_win32 16
#define TARG_ALIGNOF_LONG_win32 4
#define TARG_ALIGNOF_LONG_DOUBLE_win32 8
#define TARG_ALIGNOF_LONG_LONG_win32 8
#define TARG_ALIGNOF_NEAR_POINTER_win32 2
#define TARG_ALIGNOF_POINTER_win32 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win32 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_ALIGNOF_SHORT_win32 2
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_ALL_POINTERS_SAME_SIZE_win32 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win32 0
#define TARG_BIT_FIELD_CONTAINER_SIZE_win32 (-1)
#define TARG_BOOL_INT_KIND_win32 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win32 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win32 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win32 1
#define TARG_DBL_MANT_DIG_win32 53
#define TARG_DBL_MAX_EXP_win32 1024
#define TARG_DBL_MIN_EXP_win32 (-1021)
#define TARG_DELTA_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win32 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win32 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win32 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win32 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win32 4
#define TARG_FLT_MANT_DIG_win32 24
#define TARG_FLT_MAX_EXP_win32 128
#define TARG_FLT_MIN_EXP_win32 (-125)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win32 1
#define TARG_HAS_SIGNED_CHARS_win32 1
#define TARG_HOST_STRING_CHAR_BIT_win32 8
#define TARG_INT_FIELD_ALIGNMENT_win32 4
#define TARG_INT128_FIELD_ALIGNMENT_win32 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win32 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win32 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_win32 16
#define TARG_LDBL_MANT_DIG_win32 53
#define TARG_LDBL_MAX_EXP_win32 1024
#define TARG_LDBL_MIN_EXP_win32 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win32 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_LONG_FIELD_ALIGNMENT_win32 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win32 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win32 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_win32 128
#define TARG_MAX_BASE_CLASS_OFFSET_win32 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win32 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win32 1
#define TARG_MINIMUM_PACK_ALIGNMENT_win32 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win32 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win32 0
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win32 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win32 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win32 ( !1)
#define TARG_POINTER_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win32 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_win32 2
#define TARG_SIZEOF_DOUBLE_win32 8
#define TARG_SIZEOF_FAR_POINTER_win32 4
#define TARG_SIZEOF_FLOAT_win32 4
#define TARG_SIZEOF_INT_win32 4
#define TARG_SIZEOF_INT128_win32 16
#define TARG_SIZEOF_LONG_win32 4
#define TARG_SIZEOF_LONG_DOUBLE_win32 8
#define TARG_SIZEOF_LONG_LONG_win32 8
#define TARG_SIZEOF_NEAR_POINTER_win32 2
#define TARG_SIZEOF_POINTER_win32 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win32 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_SIZEOF_SHORT_win32 2
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_SIZE_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_win32 ((a_targ_size_t)0xffffffff)
#define TARG_SSIZE_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_X86_64_win32 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win32 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_UNWIND_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win32 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win32 1
#define TARG_VAR_HANDLE_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win32 (-1)

#else /* !TARG_SUPPORTS_X86_64 */

/* "Other" target is Windows 64-bit configuration. */
/* Target configuration: win64 */
#define TARGET_CONFIGURATION_1 win64
#define TARG_ALIGNOF_DOUBLE_win64 8
#define TARG_ALIGNOF_FAR_POINTER_win64 4
#define TARG_ALIGNOF_FLOAT_win64 4
#define TARG_ALIGNOF_INT_win64 4
#define TARG_ALIGNOF_INT128_win64 16
#define TARG_ALIGNOF_LONG_win64 4
#define TARG_ALIGNOF_LONG_DOUBLE_win64 8
#define TARG_ALIGNOF_LONG_LONG_win64 8
#define TARG_ALIGNOF_NEAR_POINTER_win64 2
#define TARG_ALIGNOF_POINTER_win64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 8
#define TARG_ALIGNOF_SHORT_win64 2
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_ALL_POINTERS_SAME_SIZE_win64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win64 0
#define TARG_BIT_FIELD_CONTAINER_SIZE_win64 (-1)
#define TARG_BOOL_INT_KIND_win64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win64 1
#define TARG_DBL_MANT_DIG_win64 53
#define TARG_DBL_MAX_EXP_win64 1024
#define TARG_DBL_MIN_EXP_win64 (-1021)
#define TARG_DELTA_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win64 4
#define TARG_FLT_MANT_DIG_win64 24
#define TARG_FLT_MAX_EXP_win64 128
#define TARG_FLT_MIN_EXP_win64 (-125)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win64 1
#define TARG_HAS_SIGNED_CHARS_win64 1
#define TARG_HOST_STRING_CHAR_BIT_win64 8
#define TARG_INT_FIELD_ALIGNMENT_win64 4
#define TARG_INT128_FIELD_ALIGNMENT_win64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win64 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_win64 16
#define TARG_LDBL_MANT_DIG_win64 53
#define TARG_LDBL_MAX_EXP_win64 1024
#define TARG_LDBL_MIN_EXP_win64 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win64 8
#define TARG_LONG_FIELD_ALIGNMENT_win64 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win64 16
#define TARG_MAXIMUM_PACK_ALIGNMENT_win64 128
#define TARG_MAX_BASE_CLASS_OFFSET_win64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win64 1
#define TARG_MINIMUM_PACK_ALIGNMENT_win64 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win64 0
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win64 ( !1)
#define TARG_POINTER_MODE_win64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_REGION_NUMBER_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SHORT_FIELD_ALIGNMENT_win64 2
#define TARG_SIZEOF_DOUBLE_win64 8
#define TARG_SIZEOF_FAR_POINTER_win64 4
#define TARG_SIZEOF_FLOAT_win64 4
#define TARG_SIZEOF_INT_win64 4
#define TARG_SIZEOF_INT128_win64 16
#define TARG_SIZEOF_LONG_win64 4
#define TARG_SIZEOF_LONG_DOUBLE_win64 8
#define TARG_SIZEOF_LONG_LONG_win64 8
#define TARG_SIZEOF_NEAR_POINTER_win64 2
#define TARG_SIZEOF_POINTER_win64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 8
#define TARG_SIZEOF_SHORT_win64 2
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_SIZE_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_long_long)
#define TARG_SIZE_T_MAX_win64 ((a_targ_size_t)0xffffffffUL)
#define TARG_SSIZE_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SUPPORTS_X86_64_win64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 1
#define TARG_UNWIND_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win64 1
#define TARG_VAR_HANDLE_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win64 (-1)

#endif /* TARG_SUPPORTS_X86_64 */
#endif /* INCLUDE_ADDITIONAL_TARGET_CONFIGURATION */

#endif /* ifndef DEFINES_WIN32_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
