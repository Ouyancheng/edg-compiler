/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
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

#define ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS 1

#ifdef CP_GEN_BE_VERSION
/*
Flags to be set for any version that uses the C++ generating back end.
*/
#define BACK_END_IS_C_GEN_BE 0
#define BACK_END_IS_CP_GEN_BE 1
#define DO_IL_LOWERING 0
#define AUTOMATIC_TEMPLATE_INSTANTIATION 0
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 0
#ifdef _WIN32
/* On NT, don't generate an IL file. */
#define IL_SHOULD_BE_WRITTEN_TO_FILE 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define DELETE_CAN_BE_FOLDED_INTO_DTOR 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#endif /* ifdef _WIN32 */
#ifdef SSI_VERSION
/* Generating instantiations in source sequence lists. */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
#define NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
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
/* Options common to Sun-hosted versions. */
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define USING_QUANTIFY 1
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 1
#ifndef __ANSIC__
#define __BSD__ 1
#endif /* ifndef __ANSIC__ */
#if SELFCOMP_VERSION
/* Self-compiled version. */
#define ALTERNATE_IL_FILE_FORMAT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define MAINTAIN_PER_INSTANTIATION_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
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
#define C_GEN_BE_GENERATES_ANSI_C 1
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define TARG_WCHAR_T_INT_KIND ik_unsigned_long 
#define USE_INIT_SECTION_IN_GENERATED_C 1
#define LONG_LONG_ALLOWED 1  /* Since gcc is used to compile output. */
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#else /* !defined(SOLARIS) */
/* SunOS version. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
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
#define FIL 1
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define LONG_LONG_ALLOWED 1
#define RESTRICT_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATES_IN_IL 1
#define RECORD_MACROS_IN_IL 1
#define RECORD_NAME_IN_PARAM_TYPE_ENTRY 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#define USER_CONTROL_OF_STRUCT_PACKING 1
#define DEFAULT_MICROSOFT_MODE 0
/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 1
#define FIXED_ADDRESS_FOR_MMAP (0xa0000000)
#else /* !1 */
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* 1 */
#define DEFAULT_SVR4_C_MODE 0
#define PRAGMA_WEAK_ALLOWED 1
#define VLA_ALLOWED 1
#define DEFAULT_VLA_ENABLED 0

#endif /* !defined(OPTIMIZED_VERSION) */

#else /* !defined(sun) */

#ifdef _WIN32

/* Options for Windows-NT version. */

#define __ANSIC__ 1
#define USING_ISO_C 1
#define C_GEN_BE_GENERATES_ANSI_C 1
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define TARG_LITTLE_ENDIAN TRUE
#define DEBUG 1
#define CHECKING 1
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#ifndef OPTIMIZED_VERSION
#define OPTIMIZED_VERSION 1
#endif /* !defined(OPTIMIZED_VERSION) */
#define ONE_INSTANTIATION_PER_OBJECT 0

#define TARG_LITTLE_ENDIAN TRUE
#define DEFAULT_TARG_HAS_SIGNED_CHARS TRUE
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT TRUE
#define TARG_JMP_BUF_NUM_ELEMENTS 16
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)

#if OPTIMIZED_VERSION
#define IL_SHOULD_BE_WRITTEN_TO_FILE 0
#else /* !OPTIMIZED_VERSION */
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define ALTERNATE_IL_FILE_FORMAT 0
#endif /* OPTIMIZED_VERSION */

#else /* !defined(_WIN32) */

#ifdef __linux__

/* Linux version. */

#define __ANSIC__ 1
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 6
#define C_GEN_BE_GENERATES_ANSI_C 1
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_LONG
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0

#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0

#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */

#ifdef __ELF__
/* On ELF linux systems, use the INIT section for static initialization. */
#define USE_INIT_SECTION_IN_GENERATED_C 1
#endif /* __ELF__ */


#else /* ifndef __linux__ */

/* Options for UnixWare test version. */
#define __ANSIC__ 1
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define INCLUDE_EDG_TEST_PRAGMAS 1
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 10
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
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */

#ifdef OPTIMIZED_VERSION
#define SVR4_TRAP_NULL_POINTER_REFERENCES 0
#else /* ifndef OPTIMIZED_VERSION */
#define EXPENSIVE_CHECKING 1
#endif /* ifdef OPTIMIZED_VERSION */

#ifndef SVR4_TRAP_NULL_POINTER_REFERENCES
#define SVR4_TRAP_NULL_POINTER_REFERENCES 1
#endif /* ifndef SVR4_TRAP_NULL_POINTER_REFERENCES */

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
#endif /* ABI_COMPATIBILITY_VERSION */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
