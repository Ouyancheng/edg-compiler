/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Linux.
*/

#ifdef DEMO_VERSION
/* Demo versions should support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define COMPILE_MULTIPLE_SOURCE_FILES 0
#endif /* ifdef DEMO_VERSION */

#define __ANSIC__ 1
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */
/* double and long long have two different alignments on Linux. */
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_DOUBLE_FIELD_ALIGNMENT 4
#define TARG_ALIGNOF_LONG_LONG 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT 4

#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 39
#define C_GEN_BE_GENERATES_ANSI_C 1
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE 1
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#define BUILTIN_VA_LIST_OVERRIDE_TYPE "__gnuc_va_list"
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#define GNU_EXTENSIONS_ALLOWED 1
#if defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED 1
#endif /* defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#define DEFAULT_GNU_COMPATIBILITY 0
#ifndef IA64_ABI
#define IA64_ABI 1
#endif /* IA64_ABI */

/* Settings needed in order for bit-field allocation to match gcc. */
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C TRUE

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


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1999-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

