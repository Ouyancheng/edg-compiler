/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Linux.
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
USE_X86_64 should be set when targeting the x86-64 variant of the i386
platform.
*/
#ifndef USE_X86_64
#ifdef __x86_64
#define USE_X86_64 1
#else /* ifndef __x86_64 */
#define USE_X86_64 0
#endif /* ifdef __x86_64 */
#endif /* ifndef USE_X86_64 */

#if USE_X86_64
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
#define GNU_BUILTIN_SYNC_FUNCTIONS_ALLOWED 1
#else /* USE_X86_64 */
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
#endif /* USE_X86_64 */

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
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 1
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long long
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long long
#define PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE   "%lld"
#define PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE "%llu"
#define PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE      "%llx"
#define MAX_INTEGER_VALUE 9223372036854775807LL
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#define MAX_UNSIGNED_INTEGER_VALUE 18446744073709551615ULL

#endif /* ifndef DEFINES_LINUX_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

