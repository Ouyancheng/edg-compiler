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

defines.h -- Defines configuration parameters for a given version of the
             front end.

*/

/*
Note: This is the EDG internal version.  The version shipped as part of
the release should contain no defines.
*/

#define ENABLE_TRANS_UNIT_TEST_MODE 1
#define DEFAULT_FLOATING_POINT_TEMPLATE_PARAMETERS_ALLOWED 1

#ifdef IA64_ABI
#if IA64_ABI
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT (-1)
#endif /* IA64_ABI */
#endif /* ifdef IA64_ABI */

#ifdef CP_GEN_BE_VERSION
/*
Flags to be set for any version that uses the C++ generating back end.
*/
#define BACK_END_IS_C_GEN_BE 0
#define BACK_END_IS_CP_GEN_BE 1
#define DEFAULT_EXCEPTIONS_ENABLED 0
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 0
#define DO_IL_LOWERING 0
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 0
#ifdef SSI_VERSION
/* Generating instantiations in source sequence lists. */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
#define NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
#define AUTOMATIC_TEMPLATE_INSTANTIATION 1
#define RECORD_FORM_OF_NAME_REFERENCE 0
#else /* !defined(SSI_VERSION) */
#define INSTANTIATE_EXTERN_INLINE 0
#define AUTOMATIC_TEMPLATE_INSTANTIATION 0
#endif /* ifdef SSI_VERSION */
#endif /* ifdef CP_GEN_BE_VERSION */

/*
Flags to be set when using the KAI inliner.
*/
#ifdef INLINER_VERSION
#define USING_KAI_INLINER 1
#define ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C 1
#define IL_SHOULD_BE_WRITTEN_TO_FILE 0
#define SVR4_TRAP_NULL_POINTER_REFERENCES 0
#endif /* ifdef INLINER_VERSION */

#ifdef sun

/* Default to SOLARIS unless SUNOS is defined. */
#ifndef SUNOS
#ifndef SOLARIS
#define SOLARIS 1
#endif /* ifndef SOLARIS */
#endif /* ifndef SUNOS */

#ifdef SUNOS
/* Default to __BSD__ on SunOS, unless __ANSIC__ has been defined. */
#ifndef __BSD__
#ifndef __ANSIC__
#define __BSD__ 1
#endif /* ifndef __ANSIC__ */
#endif /* ifndef __BSD__ */
/* Default to generating pcc C on SunOS. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
#define GCC_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
/* Implement long double as double. */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define TARG_SIZEOF_LONG_DOUBLE 8
#endif /* SUNOS */

#ifndef SUN_TEST_VERSION
#define SUN_TEST_VERSION 1
#endif /* ifndef SUN_TEST_VERSION */

#if SUN_TEST_VERSION
/* Settings needed to make CodeCenter happy (it doesn't understand long
   double). */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define TARG_SIZEOF_LONG_DOUBLE 8
#endif /* SUN_TEST_VERSION */

#include "defines_solaris.h"

#if SUN_TEST_VERSION

/* Options common to Sun-hosted versions. */

#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 1
#define C99_IL_EXTENSIONS_SUPPORTED 1
#ifndef SUNOS
#ifndef INSTANTIATE_EXTERN_INLINE
#define INSTANTIATE_EXTERN_INLINE 1
#endif /* INSTANTIATE_EXTERN_INLINE */
#endif /* ifndef SUNOS */
#ifdef SELFCOMP_VERSION
/* Self-compiled version. */
#define ALTERNATE_IL_FILE_FORMAT 0
#define ONE_INSTANTIATION_PER_OBJECT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#ifdef SOLARIS
#define _POSIX_C_SOURCE 3
#define _XOPEN_VERSION 0
#define _XOPEN_SOURCE 0
#define _XOPEN_SOURCE_EXTENDED 1
#endif /* SOLARIS */
#endif /* SELFCOMP_VERSION */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 1
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */

#ifdef SOLARIS
#ifdef __SUNPRO_C
#if 0
/* Does not always work right. */
#define GUARD_MACRO2_FOR_VA_LIST "_SYS_VA_LIST_H"
#endif /* 0 */
#endif /* ifdef __SUNPRO_C */

#else /* !defined(SOLARIS) */
/* SunOS version. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
#ifndef GCC_IS_GENERATED_CODE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifdef SOLARIS */

#ifdef OPTIMIZED_VERSION

/* Options for Sun optimized version. */
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for Sun test version. */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define RECORD_NAME_IN_PARAM_TYPE_ENTRY 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#define USER_CONTROL_OF_STRUCT_PACKING 1
#define DEFAULT_MICROSOFT_MODE 0
#define UPC_EXTENSIONS_ALLOWED 1
#ifdef SOLARIS
#define ASM_FUNCTION_ALLOWED 1
#endif /* ifdef SOLARIS */
#define REPRESENT_EMPTY_STATEMENTS_IN_IL 1
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 1
#define FIXED_ADDRESS_FOR_MMAP (0xa0000000)
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#else /* !1 */
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* 1 */
#define DEFAULT_SVR4_C_MODE 0
#define PRAGMA_WEAK_ALLOWED 1
#define VLA_ALLOWED 1
#define DEFAULT_VLA_ENABLED 0

#endif /* !defined(OPTIMIZED_VERSION) */

#endif /* SUN_TEST_VERSION */

#else /* !defined(sun) */

#ifdef _WIN32

/* Options for Windows-NT version. */

#include "defines_win32.h"

#else /* !defined(_WIN32) */

#ifdef __linux__

/* Linux version. */

#include "defines_linux.h"

#ifndef LINUX_TEST_VERSION
#define LINUX_TEST_VERSION 1
#endif /* ifndef LINUX_TEST_VERSION */

#if LINUX_TEST_VERSION

/* Linux test version definitions. */
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define FIL 1
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 1
#endif /* ifndef DEBUG */
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#define DEFAULT_ALLOW_ANACHRONISMS 0
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG 0
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
#define ADDRESS_OF_ELLIPSIS_ALLOWED 1
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE 1
#define USER_CONTROL_OF_STRUCT_PACKING 1
#define ASM_FUNCTION_ALLOWED 1
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY 1
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 1
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 4
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */
#define ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS 1
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#define DEFAULT_FRIEND_INJECTION TRUE
#undef COMPILE_MULTIPLE_SOURCE_FILES
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#define DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE 0

#ifndef OPTIMIZED_VERSION
#define EXPENSIVE_CHECKING 1
#endif /* ifndef OPTIMIZED_VERSION */

#endif /* LINUX_TEST_VERSION */

#else /* ifndef __linux__ */

#ifdef __hpux

/* Options for HP-UX version. */

/* Options to enable quasi-standard Unix features: */
#define _INCLUDE_POSIX_SOURCE 1
#define _INCLUDE_XOPEN_SOURCE 1
#define _INCLUDE_AES_SOURCE 1

/* >>> HP-UX Options determined with dettarg: */
#define TARG_LITTLE_ENDIAN FALSE
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS TRUE
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT TRUE
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 8
#define TARG_SIZEOF_WCHAR_T 4
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
/* --- End of options determined with dettarg. */

/* jmp_buf settings for portable EH on HP-UX: */
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT 1
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND ((a_float_kind)fk_double)
#define TARG_JMP_BUF_NUM_ELEMENTS 25

#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 1
#define C99_IL_EXTENSIONS_SUPPORTED 1
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define USER_CONTROL_OF_STRUCT_PACKING 1
#define PRAGMA_WEAK_ALLOWED 1
#define VLA_ALLOWED 1

#ifdef SELFCOMP_VERSION
/* Self-compiled version (HP-UX). */
#define ALTERNATE_IL_FILE_FORMAT 0
#define ONE_INSTANTIATION_PER_OBJECT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#define _POSIX_C_SOURCE 1
#define _XOPEN_VERSION 0
#define _XOPEN_SOURCE_EXTENDED 0
#define _XOPEN_SOURCE 0
#define _XOPEN_SOURCE_EXTENDED 0
#endif /* SELFCOMP_VERSION */

#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 1
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */

#define __ANSIC__ 1
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 1
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 0
#define USE_PRAGMA_IDENT_IN_GENERATED_CODE 1
#define STDC_ZERO_IN_NONSTRICT_MODE 1
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST"
/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 0
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#else /* !1 */
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* 1 */

#ifdef OPTIMIZED_VERSION

/* Options for HP-UX optimized version. */
#ifndef CHECKING
#define CHECKING 0
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for HP-UX test version. */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define RECORD_NAME_IN_PARAM_TYPE_ENTRY 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#define DEFAULT_MICROSOFT_MODE 0
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#define DEFAULT_SVR4_C_MODE 0
#define DEFAULT_VLA_ENABLED 0

#endif /* !defined(OPTIMIZED_VERSION) */


#else /* ifndef __hpux */

/* Options for UnixWare test version. */
#define __ANSIC__ 1
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 10
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS 0
#define FIL 1
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#define DEBUG 1
#define C_GEN_BE_GENERATES_ANSI_C 1
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#define DEFAULT_ALLOW_ANACHRONISMS 0
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG 0
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
#define ADDRESS_OF_ELLIPSIS_ALLOWED 1
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_C_MODE 1
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define USER_CONTROL_OF_STRUCT_PACKING 1
#define ASM_FUNCTION_ALLOWED 1
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY 1
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 1
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 4
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#define DEFAULT_FRIEND_INJECTION TRUE

#ifdef OPTIMIZED_VERSION
#define SVR4_TRAP_NULL_POINTER_REFERENCES 0
#else /* ifndef OPTIMIZED_VERSION */
#define EXPENSIVE_CHECKING 1
#endif /* ifdef OPTIMIZED_VERSION */

#ifndef SVR4_TRAP_NULL_POINTER_REFERENCES
#define SVR4_TRAP_NULL_POINTER_REFERENCES 1
#endif /* ifndef SVR4_TRAP_NULL_POINTER_REFERENCES */

#endif /* ifdef __hpux */
#endif /* ifdef __linux__ */
#endif /* defined(_WIN32) */
#endif /* defined(sun) */

/*
Set ABI-related switches.  This is done late so that individual configurations
(above) can do something different from the EDG default by setting the
switches before this point.
*/
#ifndef ABI_COMPATIBILITY_VERSION
#define ABI_COMPATIBILITY_VERSION 99999 /* Use latest version. */
#ifndef IA64_ABI
/* We want enough cfront compatibility to be able to use I/O streams compiled
   by cfront, but we also want the latest features. */
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 1
#endif /* ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#ifndef ABI_CHANGES_FOR_RTTI
#define ABI_CHANGES_FOR_RTTI 1
#endif /* ifndef ABI_CHANGES_FOR_RTTI */
#ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE 1
#endif /* ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES 1
#endif /* ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES */
#ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT 1
#endif /* ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT */
#ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES TRUE
#endif /* ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES */
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE
#define ABI_CHANGES_FOR_PLACEMENT_DELETE 1
#endif /* ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE */
#ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN 1
#endif /* ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS 1
#endif /* ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#ifndef DEFAULT_COMPRESS_MANGLED_NAMES
#define DEFAULT_COMPRESS_MANGLED_NAMES 1
#endif /* ifndef DEFAULT_COMPRESS_MANGLED_NAMES */
#ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT 1
#endif /* ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */
#endif /* ifndef IA64_ABI */
#endif /* ifndef ABI_COMPATIBILITY_VERSION */

#if ABI_COMPATIBILITY_VERSION == 228
#ifdef IL_SHOULD_BE_WRITTEN_TO_FILE
#if IL_SHOULD_BE_WRITTEN_TO_FILE
/* When doing 2.28 ABI testing, also use the non-alternate IL file format.
   Only set this here if IL_SHOULD_BE_WRITTEN_TO_FILE has previously been
   defined to TRUE. */
#define ALTERNATE_IL_FILE_FORMAT 0
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* ifdef IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Use cfront 2.1 object compatibility for 2.28 ABI testing. */
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 1
#endif /* ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */

#endif /* ABI_COMPATIBILITY_VERSION */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
