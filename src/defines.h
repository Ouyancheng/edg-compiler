/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
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

#ifdef sun
/* Options Common to Sun hosted versions. */
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define USING_QUANTIFY 1

#ifdef OPTIMIZED_VERSION

/* Options for Sun optimized version. */
#define CHECKING 1
#define DEBUG 0

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for Sun test version. */
#define __BSD__ 1
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define FIL 1
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0

#endif /* !defined(OPTIMIZED_VERSION) */

#else /* !defined(sun) */

#ifdef _WIN32

/* Options for Windows-NT version. */

#define __MSDOS__
#define __MSC__ 1
#define __ANSIC__ 1
#define TARG_LITTLE_ENDIAN TRUE
#define TARG_JMP_BUF_NUM_ELEMENTS 8
#define STAT_FIRST_PARAM_IS_CONST 1
#define DEBUG 1
#define CHECKING 1

#ifndef OPTIMIZED_VERSION
#define OPTIMIZED_VERSION 1
#endif /* !defined(OPTIMIZED_VERSION) */

#if OPTIMIZED_VERSION
#define IL_SHOULD_BE_WRITTEN_TO_FILE 0
#else /* !OPTIMIZED_VERSION */
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define ALTERNATE_IL_FILE_FORMAT 0
#endif /* OPTIMIZED_VERSION */

#else /* !defined(_WIN32) */

/* Options for UnixWare test version. */
#define __SYSV__
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define STAT_FIRST_PARAM_IS_CONST 1
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 10
#define CHECKING 1
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

#endif /* defined(_WIN32) */
#endif /* defined(sun) */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
