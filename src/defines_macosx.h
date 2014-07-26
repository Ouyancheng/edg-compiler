/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2002-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This version is for the Apple MacOS X operating system.

*/

/* Avoid including these declarations more than once. */
#ifndef DEFINES_MACOS_H
#define DEFINES_MACOS_H 1

#ifdef DEMO_VERSION
/* Demo versions should support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define DEBUG 0
#endif /* ifdef DEMO_VERSION */

/* Configuration definitions determined by dettarg.c: */
#ifdef __x86_64__
#define TARG_SIZEOF_LONG 8
#define TARG_ALIGNOF_LONG 8
#define TARG_SIZEOF_POINTER 8
#define TARG_ALIGNOF_POINTER 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_int)
#define HOST_ALIGNMENT_REQUIRED 8
#define TARG_ALIGNOF_LONG_LONG 8
#else /* ifndef __x86_64__ */
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_ALIGNOF_LONG_LONG 4
#endif /* ifdef __x86_64__ */
#ifdef __ppc__
#define TARG_LITTLE_ENDIAN FALSE
#define TARG_JMP_BUF_NUM_ELEMENTS 192
#define TARG_SIZEOF_LONG_DOUBLE 8
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#else /* ifndef __ppc__ */
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 16
#define TARG_LITTLE_ENDIAN TRUE
#ifdef __x86_64__
#define TARG_JMP_BUF_NUM_ELEMENTS 37
#else /* ifndef __x86_64__ */
#define TARG_JMP_BUF_NUM_ELEMENTS 18
#endif /* ifdef __x86_64__ */
#endif /* ifndef __ppc__ */

#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS TRUE
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT TRUE
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#endif /* ifndef _lint */
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)
#ifndef ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
#define ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS 1
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST"


/* Language extensions. */
#if !defined(GNU_EXTENSIONS_ALLOWED)
#define GNU_EXTENSIONS_ALLOWED 1
#endif /* !defined(GNU_EXTENSIONS_ALLOWED) */
#define DEFAULT_GNU_COMPATIBILITY 0
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 1
#define C99_IL_EXTENSIONS_SUPPORTED 1
#ifndef MICROSOFT_EXTENSIONS_ALLOWED
#define MICROSOFT_EXTENSIONS_ALLOWED 0
#endif /* ifndef MICROSOFT_EXTENSIONS_ALLOWED */
#define LONG_LONG_ALLOWED 1
#define UNICODE_SOURCE_SUPPORTED 1
#define DEFAULT_UNICODE_SOURCE_KIND usk_utf8


/* ABI selection. */
#ifndef IA64_ABI
#define IA64_ABI 1
#endif /* ifndef IA64_ABI */
#define DEFAULT_EMULATE_GNU_ABI_BUGS 0
#ifndef CP_GEN_BE_VERSION
#ifndef GCC_IS_GENERATED_CODE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#ifndef CLANG_IS_GENERATED_CODE_TARGET
#define CLANG_IS_GENERATED_CODE_TARGET 1
#endif /* ifndef CLANG_IS_GENERATED_CODE_TARGET */
#endif /* ifndef CP_GEN_BE_VERSION */
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 1
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C 1
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE 1
#if defined(__i386__)
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES 1
#elif defined(__x86_64__)
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES 1
#elif defined(__ppc__)
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES 0
#else /* Not a recognized architecture. */
#error -- Unexpected MacOS X platform
#endif /* defined(__i386__) */

#endif /* ifndef DEFINES_MACOS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2002-2014 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
