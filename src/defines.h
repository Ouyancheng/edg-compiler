/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

*/

/* Avoid including these declarations more than once. */
#ifndef DEFINES_H
#define DEFINES_H 1

/*
Note: This is the EDG internal version.  The version shipped as part of
the release should contain no defines.
*/

/*
High level EDG macros used solely in this file for easy configuration:

  DEMO_VERSION          Used to compile a demo version.
                        Sets SUN_TEST_VERSION, LINUX_TEST_VERSION,
                        MACOSX_TEST_VERSION (as appropriate for the host) to 0.

  OPTIMIZED_VERSION     Create an optimized version (smaller, faster, fewer
                        features).

  SUN_TEST_VERSION      Define a set of 'standard' language features for
                        a Sun hosted compiler.  Defined to 1 by default when
                        'sun' is defined.  Most run_tests were recorded
                        with this set of language features defined.

  LINUX_TEST_VERSION    Define a set of 'standard' language features for
                        a Linux hosted compiler.  Defined to 1 by default when
                        '__linux__' is defined.

  MACOSX_TEST_VERSION   Define a set of 'standard' language features for
                        a Mac OS X hosted compiler.  Defined to 1 by default
                        when '__APPLE__' and '__MACH__' are defined.

  EDG_TEST_VERSION      Define a set of 'standard' language features that
                        can be used on a variety of hosts.  This set is
                        meant to include many of the major language features.
                        Originally based on the set of SUN_TEST_VERSION
                        features so that many of the run_tests will continue
                        to work properly.

  CP_GEN_BE_VERSION     Flags to be set for any version that uses the 
                        C++ generating back end.

  SSI_VERSION           Generating instantiations in source sequence lists.
                        Only valid when CP_GEN_BE_VERSION is defined.

  SELFCOMP_VERSION      Self-compiled version.  Currently used only on
                        Sun and HP-UX platforms.

The macros SUN_TEST_VERSION, LINUX_TEST_VERSION, MACOSX_TEST_VERSION are
defined and used only within this file.  EDG_TEST_VERSION is defined
when SUN_TEST_VERSION is defined and may also be defined externally to this
file to cause inclusion of a standard set of language features, regardless
of the host system.

*/

/*
Set the test version flags to FALSE for demo versions.
*/
#ifdef DEMO_VERSION
#ifdef __sun
#define SUN_TEST_VERSION 0
#endif  /* ifdef __sun */
#ifdef __linux__
#define LINUX_TEST_VERSION 0
#endif /* ifdef __linux__ */
#if defined(__APPLE__) && defined(__MACH__)
#define MACOSX_TEST_VERSION 0
#endif /* defined(__APPLE__) && defined(__MACH__) */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */
#else /* !defined(DEMO_VERSION) */
#ifndef __CYGWIN32__
/* In development versions, allow values of gnu_version less than 30200 so
   some early gcc compatibility features can be tested. */
#define MIN_GNU_VERSION 29500
#endif /* ifndef __CYGWIN32__ */
#endif /* ifdef DEMO_VERSION */

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
#ifndef CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#define CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT 1
#endif /* ifndef CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
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

#ifdef __sun

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
#define SUN_IS_GENERATED_CODE_TARGET 1
#define SUN_TARGET_VERSION_NUMBER 0
#define ASM_FUNCTION_ALLOWED 0
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
/* Implement long double as double. */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define TARG_SIZEOF_LONG_DOUBLE 8
#else /* !SUNOS, i.e. SOLARIS */
#endif /* SUNOS */

/* Assume we are generating code for gcc when being compiled by gcc or
   codecenter. */
#ifndef CP_GEN_BE_VERSION
#ifndef GCC_IS_GENERATED_CODE_TARGET
#if (defined(__GNUC__) || defined(__CENTERLINE__))
#define GCC_IS_GENERATED_CODE_TARGET 1
#endif /* defined(__GNUC__) || defined(__CENTERLINE__) */
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef CP_GEN_BE_VERSION */

#ifndef SUN_TEST_VERSION
#define SUN_TEST_VERSION 1
#endif /* ifndef SUN_TEST_VERSION */

#if SUN_TEST_VERSION
/* Settings needed to make CodeCenter happy (it doesn't understand long
   double). */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define TARG_SIZEOF_LONG_DOUBLE 8
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE 1
#ifdef __sparc
#define CENTERLINE_CHECKING 1
#endif /* ifdef __sparc */
#ifndef UNICODE_SOURCE_SUPPORTED
#define UNICODE_SOURCE_SUPPORTED 1
#endif /* ifndef UNICODE_SOURCE_SUPPORTED */
#endif /* SUN_TEST_VERSION */

#include "defines_solaris.h"

#ifndef DEFAULT_EDG_BASE
#define DEFAULT_EDG_BASE "/edg/cpfe"
#endif /* DEFAULT_EDG_BASE */

#if SUN_TEST_VERSION

/* Specify a language feature set by defining EDG_TEST_VERSION.  Add or
   override any additional settings here, as well as any host specific 
   options. */
#define EDG_TEST_VERSION 1

/* Options common to Sun-hosted versions. */

#ifndef CP_GEN_BE_VERSION
#define LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS 1
#endif /* ifndef CP_GEN_BE_VERSION */
#ifndef INSTANTIATE_EXTERN_INLINE
#ifdef SUNOS
#define INSTANTIATE_EXTERN_INLINE 0
#else /* !defined(SUNOS) */
#define INSTANTIATE_EXTERN_INLINE 1
#endif /* ifdef SUNOS */
#endif /* INSTANTIATE_EXTERN_INLINE */
#ifdef SELFCOMP_VERSION
/* Self-compiled version. */
#define ALTERNATE_IL_FILE_FORMAT 0
#define ONE_INSTANTIATION_PER_OBJECT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#ifndef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 0
#endif /* ifndef LOWER_VARIABLE_LENGTH_ARRAYS */
#ifdef SOLARIS
#define __EXTENSIONS__ 1
#endif /* SOLARIS */
#define GCC_IS_GENERATED_CODE_TARGET 1
#endif /* SELFCOMP_VERSION */
#define DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS 1
#define DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS 1
#if GNU_EXTENSIONS_ALLOWED
#define GNU_VECTOR_TYPES_ALLOWED 1
#endif /* GNU_EXTENSIONS_ALLOWED */

#ifdef SOLARIS
#ifdef __SUNPRO_C
#if 0
/* Does not always work right. */
#define GUARD_MACRO2_FOR_VA_LIST "_SYS_VA_LIST_H"
#endif /* 0 */
#endif /* ifdef __SUNPRO_C */
#define REDEFINE_EXTNAME_PRAGMA_ENABLED 1
#if defined(IA64_ABI) && IA64_ABI
/* Use the <=4.0 virtual base class handling technique. */
#ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS FALSE
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS TRUE
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
#endif /* defined(IA64_ABI) && IA64_ABI */
#else /* !defined(SOLARIS) */
/* SunOS version. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
#ifndef GCC_IS_GENERATED_CODE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifdef SOLARIS */

#ifndef OPTIMIZED_VERSION
/* Options for Sun test version. */
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#ifdef SOLARIS
#define ASM_FUNCTION_ALLOWED 1
#endif /* ifdef SOLARIS */
/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 1
#define FIXED_ADDRESS_FOR_MMAP (0xa0000000)
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#else /* !1 */
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* 1 */

#endif /* ifndef OPTIMIZED_VERSION */

#endif /* SUN_TEST_VERSION */
#define SUN_EXTENSIONS_ALLOWED 1
#define DEFAULT_SUN_COMPATIBILITY 0

#else /* !defined(__sun) */

#ifdef _WIN32

/* Options for Windows-NT version. */

#include "defines_win32.h"

#else /* !defined(_WIN32) */

#ifdef __linux__

/* Linux version. */

#ifndef LINUX_TEST_VERSION
#define LINUX_TEST_VERSION 1
#endif /* ifndef LINUX_TEST_VERSION */

#if LINUX_TEST_VERSION
/* defines_linux.h sets this to TRUE if not already set. */
#ifndef IA64_ABI
#define IA64_ABI 0
#endif /* IA64_ABI */

#if IA64_ABI
/* Tie the GNU ABI version to the GNU version in the IA-64 test version. */
#define TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION 1
#undef MIN_GNU_VERSION
#define MIN_GNU_VERSION 30200
/* The IA-64 test version includes embedded C support. */
#define EMBEDDED_C_ALLOWED 1
#define DEFAULT_EMBEDDED_C_ENABLED 0
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* IA64_ABI */

#endif /* LINUX_TEST_VERSION */

#include "defines_linux.h"

#if LINUX_TEST_VERSION

/* Linux test version definitions. */
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define INCLUDE_EDG_TEST_ATTRIBUTES 1
#ifndef _lint
#endif /* ifndef _lint */
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
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
#define USER_CONTROL_OF_STRUCT_PACKING 1
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED 1
#endif /* ASM_FUNCTION_ALLOWED */
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
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#undef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#define DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE 0
#ifndef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 0
#endif /* ifndef LOWER_VARIABLE_LENGTH_ARRAYS */
#if !defined(UNICODE_SOURCE_SUPPORTED) || !UNICODE_SOURCE_SUPPORTED
#define ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR 1
#endif /* !defined(UNICODE_SOURCE_SUPPORTED) || !UNICODE_SOURCE_SUPPORTED */
#define LOWER_DESIGNATED_INITIALIZERS TRUE

#ifndef OPTIMIZED_VERSION
#ifndef EXPENSIVE_CHECKING
#define EXPENSIVE_CHECKING 1
#endif /* ifndef EXPENSIVE_CHECKING */
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
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#endif /* ifndef _lint */
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
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 1
#define C99_IL_EXTENSIONS_SUPPORTED 1
#define CPP0X_IL_EXTENSIONS_SUPPORTED 1
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
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#define DEFAULT_MICROSOFT_MODE 0
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#define DEFAULT_SVR4_C_MODE 0
#define DEFAULT_VLA_ENABLED 0

#endif /* !defined(OPTIMIZED_VERSION) */


#else /* ifndef __hpux */

#if defined(__APPLE__) && defined(__MACH__)
/* Options for MacOS X (10.2) version. */

#ifndef MACOSX_TEST_VERSION
#define MACOSX_TEST_VERSION 1
#endif /* ifndef MACOSX_TEST_VERSION */

#if MACOSX_TEST_VERSION
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define DEFAULT_MICROSOFT_MODE 0
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE 1
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#endif /* MACOSX_TEST_VERSION */

#include "defines_macosx.h"

#else /* !(defined(__APPLE__) && defined(__MACH__)) */
#ifdef __CYGWIN32__

/* Options for Windows/Cygwin version. */
#define DEFAULT_INSTANTIATION_MODE tim_all
#define UNICODE_SOURCE_SUPPORTED TRUE
#define DEFAULT_CHECK_CONCATENATIONS TRUE
#define ASM_FUNCTION_ALLOWED TRUE
#define FIXED_POINT_ALLOWED 1
#ifdef DEMO_VERSION
/* Demo versions should support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define COMPILE_MULTIPLE_SOURCE_FILES 0
#define DEBUG 0
#define EMBEDDED_C_ALLOWED 1
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* ifdef DEMO_VERSION */
#define TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION 1
#define MIN_GNU_VERSION 30200

#ifndef DEFAULT_EDG_BASE
#define DEFAULT_EDG_BASE "/c/edg/cpfe/release"
#endif /* DEFAULT_EDG_BASE */
#define __ANSIC__ 1
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */
#define C_GEN_BE_GENERATES_ANSI_C 1
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE 1
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#define BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME "__gnuc_va_list"
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#define GNU_EXTENSIONS_ALLOWED 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define SUN_EXTENSIONS_ALLOWED 1
#define DEFAULT_SUN_COMPATIBILITY 0
#if defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED 1
#endif /* defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#ifndef DEFAULT_GNU_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY 0
#endif /* ifndef DEFAULT_GNU_COMPATIBILITY */
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 1
#ifndef IA64_ABI
#define IA64_ABI 1
#endif /* IA64_ABI */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
/* Unless specified otherwise, Cygwin version will have full macro position
   and tracing facilities. */
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#define FULL_SOURCE_POS_IN_IL_STATEMENT 1
#ifndef FULLY_RESOLVED_MACRO_POSITIONS
#define FULLY_RESOLVED_MACRO_POSITIONS 1
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#ifndef MACRO_INVOCATION_TREE_IN_IL
#define MACRO_INVOCATION_TREE_IN_IL 1
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#ifndef DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS
#define DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS TRUE
#endif /* DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS */
/* Settings needed in order for bit-field allocation to match gcc. */
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C TRUE

#define USE_MMAP_FOR_MEMORY_REGIONS 0

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
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 1

#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 1

/* Configuration definitions determined by dettarg.c: */
#define TARG_LITTLE_ENDIAN TRUE
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
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_short)
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_SHORT
#endif /* ifndef _lint */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#define HOST_ALIGNMENT_REQUIRED 8
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE FALSE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 52
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)
#else /* ifndef __CYGWIN32__ */
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
#ifndef _lint
/* TARG_SIZEOF_WCHAR_T is only used by version 3.7 and earlier. */
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#endif /* ifndef _lint */
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
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

#endif /* ifdef __CYGWIN32__ */
#endif /* defined(__APPLE__) && defined(__MACH__) */
#endif /* ifdef __hpux */
#endif /* ifdef __linux__ */
#endif /* defined(_WIN32) */
#endif /* defined(__sun) */

#if EDG_TEST_VERSION

/* Define a full-featured set of language features. */

#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#endif /* ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#ifndef C99_IL_EXTENSIONS_SUPPORTED
#define C99_IL_EXTENSIONS_SUPPORTED 1
#endif /* ifndef C99_IL_EXTENSIONS_SUPPORTED */
#ifndef CPP0X_IL_EXTENSIONS_SUPPORTED
#define CPP0X_IL_EXTENSIONS_SUPPORTED 1
#endif /* ifndef CPP0X_IL_EXTENSIONS_SUPPORTED */
#ifndef INSTANTIATE_EXTERN_INLINE
#define INSTANTIATE_EXTERN_INLINE 1
#endif /* ifndef INSTANTIATE_EXTERN_INLINE */
#ifndef INSTANTIATE_BEFORE_PCH_CREATION
#define INSTANTIATE_BEFORE_PCH_CREATION TRUE
#endif /* ifndef INSTANTIATE_BEFORE_PCH_CREATION */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 1
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#ifndef EMBEDDED_C_ALLOWED
#define EMBEDDED_C_ALLOWED 1
#endif /* ifndef EMBEDDED_C_ALLOWED */
#ifndef DEFAULT_EMBEDDED_C_ENABLED
#define DEFAULT_EMBEDDED_C_ENABLED 0
#endif /* ifndef DEFAULT_EMBEDDED_C_ENABLED */
#ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES */
#ifndef INCLUDE_EDG_TEST_NAMED_REGISTERS
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_REGISTERS */
#ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED 1
#endif /* ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */

#ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED
#define REDEFINE_EXTNAME_PRAGMA_ENABLED 1
#endif /* ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED */

#ifdef OPTIMIZED_VERSION

/* Options for optimized version. */
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for full featured test version. */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS 1
#endif /* ifndef INCLUDE_EDG_TEST_PRAGMAS */
#ifndef INCLUDE_EDG_TEST_ATTRIBUTES
#define INCLUDE_EDG_TEST_ATTRIBUTES 1
#endif /* ifndef INCLUDE_EDG_TEST_ATTRIBUTES */
#ifndef MICROSOFT_EXTENSIONS_ALLOWED
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#endif /* ifndef MICROSOFT_EXTENSIONS_ALLOWED */
#ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#endif /* ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#ifndef USER_CONTROL_OF_STRUCT_PACKING
#define USER_CONTROL_OF_STRUCT_PACKING 1
#endif /* ifndef USER_CONTROL_OF_STRUCT_PACKING */
#ifndef DEFAULT_MICROSOFT_MODE
#define DEFAULT_MICROSOFT_MODE 0
#endif /* ifndef DEFAULT_MICROSOFT_MODE */
#ifndef UPC_EXTENSIONS_ALLOWED
#define UPC_EXTENSIONS_ALLOWED 1
#endif /* ifndef UPC_EXTENSIONS_ALLOWED */
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED 1
#endif /* ifdef ASM_FUNCTION_ALLOWED */
#ifndef EXTRA_SOURCE_POSITIONS_IN_IL
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#endif /* ifndef EXTRA_SOURCE_POSITIONS_IN_IL */
#ifndef DEFAULT_SVR4_C_MODE
#define DEFAULT_SVR4_C_MODE 0
#endif /* ifndef DEFAULT_SVR4_C_MODE */
#ifndef PRAGMA_WEAK_ALLOWED
#define PRAGMA_WEAK_ALLOWED 1
#endif /* ifndef PRAGMA_WEAK_ALLOWED */
#ifndef VLA_ALLOWED
#define VLA_ALLOWED 1
#endif /* ifndef VLA_ALLOWED */
#ifndef DEFAULT_VLA_ENABLED
#define DEFAULT_VLA_ENABLED 0
#endif /* ifndef DEFAULT_VLA_ENABLED */
#ifndef SUN_EXTENSIONS_ALLOWED
#define SUN_EXTENSIONS_ALLOWED 1
#endif /* ifndef SUN_EXTENSIONS_ALLOWED */
#ifndef DEFAULT_SUN_COMPATIBILITY
#define DEFAULT_SUN_COMPATIBILITY 0
#endif /* ifndef DEFAULT_SUN_COMPATIBILITY */
#ifndef GNU_EXTENSIONS_ALLOWED
#define GNU_EXTENSIONS_ALLOWED 1
#endif /* ifndef GNU_EXTENSIONS_ALLOWED */
#ifndef DEFAULT_GNU_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY 0
#endif /* ifndef DEFAULT_GNU_COMPATIBILITY */

#endif /* !defined(OPTIMIZED_VERSION) */

#endif /* EDG_TEST_VERSION */

/*
Enable recognition of Microsoft attributes for internal versions.
*/
#ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING
#define SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING 0
#endif /* ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING */

/*
For test versions, eschew the Microsoft approach to predeclaring type_info
in the global namespace since that doesn't match the EDG run-time support
library.
*/
#ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD
#if EDG_TEST_VERSION || MACOSX_TEST_VERSION || LINUX_TEST_VERSION
#define MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD 1
#endif /* EDG_TEST_VERSION || MACOSX_TEST_VERSION || LINUX_TEST_VERSION */
#endif /* ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD */


#ifndef LOWER_FIXED_POINT
#ifndef EMBEDDED_C_ALLOWED
#define EMBEDDED_C_ALLOWED 0
#endif /* ifndef EMBEDDED_C_ALLOWED */
#ifndef FIXED_POINT_ALLOWED
#if EMBEDDED_C_ALLOWED
#define FIXED_POINT_ALLOWED 1
#else /* !EMBEDDED_C_ALLOWED */
#define FIXED_POINT_ALLOWED 0
#endif /* EMBEDDED_C_ALLOWED */
#endif /* ifndef FIXED_POINT_ALLOWED */
#if !defined(CP_GEN_BE_VERSION) && (EMBEDDED_C_ALLOWED || FIXED_POINT_ALLOWED)
#define LOWER_FIXED_POINT 1
#endif /* !defined(CP_GEN_BE_VERSION) && ... */
#endif /* ifndef LOWER_FIXED_POINT */

/*
GNU target compiler configuration.  When not using a GNU compiler to compile
the front end, set GNU_TARGET_VERSION_NUMBER to a reasonable default.
*/
#if !defined(__GNUC__) || !defined(__GNUC_MINOR__) || \
    !defined(__GNUC_PATCHLEVEL)
#define GNU_TARGET_VERSION_NUMBER 30200
#endif /* !defined(__GNUC__) || !defined(__GNUC_MINOR__) || ... */

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

#ifndef SUN_IS_GENERATED_CODE_TARGET
#define SUN_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef SUN_IS_GENERATED_CODE_TARGET */
#ifndef BACK_END_IS_CP_GEN_BE
#define BACK_END_IS_CP_GEN_BE 0
#endif /* ifndef BACK_END_IS_CP_GEN_BE */

#if !defined(SUN_TARGET_VERSION_NUMBER) &&          \
    (SUN_IS_GENERATED_CODE_TARGET ||                \
     (BACK_END_IS_CP_GEN_BE &&                      \
      CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT &&    \
      SUN_EXTENSIONS_ALLOWED)) &&                   \
    !(defined(__SUNPRO_CC) || defined(__SUNPRO_C))
#define SUN_TARGET_VERSION_NUMBER 0x530
#endif /* !defined(SUN_TARGET_VERSION_NUMBER) && ... */

/*
Overwrite freed memory to detect later uses.
*/
#ifndef OVERWRITE_FREED_MEM_BLOCKS
#ifdef DEMO_VERSION
#define OVERWRITE_FREED_MEM_BLOCKS 0
#else /* ifndef DEMO_VERSION */
#define OVERWRITE_FREED_MEM_BLOCKS 1
#endif /* ifdef DEMO_VERSION */
#endif /* ifndef OVERWRITE_FREED_MEM_BLOCKS */

/*
If EXPENSIVE_CHECKING has been requested, also enable checking pragmas.
*/
#ifdef EXPENSIVE_CHECKING
#if EXPENSIVE_CHECKING
#define ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING 1
#endif /* EXPENSIVE_CHECKING */
#endif /* ifndef EXPENSIVE_CHECKING */

/*
If using lint on a non-Sun platform, define some features that are in the
SUN_TEST_VERSION but not the EDG_TEST_VERSION.
*/
#ifdef _lint
#ifndef __sun
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#undef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 1
#endif /* ifndef __sun */
#endif /* ifdef lint */

#endif /* ifndef DEFINES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2010 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
