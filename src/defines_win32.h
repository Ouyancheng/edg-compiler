/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This is the version for Windows 95/98/NT/etc.
*/

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
#define TARG_SIZEOF_LONG_DOUBLE 8
#define TARG_ALIGNOF_LONG_DOUBLE 8
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 16
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)

/*
Definitions for Windows NT/95/98:
*/
#define __ANSIC__ 1
#define USING_ISO_C 1
#define C_GEN_BE_GENERATES_ANSI_C 1
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define DEBUG 1
#define CHECKING 1
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#define CFRONT_3_0_OBJECT_CODE_COMPATIBILITY 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST_DEFINED"
#define ALLOW_FLOATING_POINT_TEMPLATE_PARAMETERS 1
#define DEFAULT_VA_LIST_IN_STD_NAMESPACE 0

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
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define DELETE_CAN_BE_FOLDED_INTO_DTOR 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#define PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED 0
#define DEFAULT_TYPE_INFO_IN_NAMESPACE_STD 0
#endif /* ifdef CP_GEN_BE_VERSION */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2002 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
