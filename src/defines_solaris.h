/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

This version is for the Sun Solaris operating system.

*/

/* Configuration definitions determined by dettarg.c: */

#ifdef SUNOS
/* SPARC SunOS specific defines. */
#define TARG_LITTLE_ENDIAN FALSE
#define TARG_JMP_BUF_NUM_ELEMENTS 9
#else /* ifndef SUNOS */
#ifdef sparc
/* SPARC Solaris specific defines. */
#define TARG_LITTLE_ENDIAN FALSE
#define TARG_JMP_BUF_NUM_ELEMENTS 12
#else /* ifndef sparc */
/* Intel Solaris specific defines. */
#define TARG_LITTLE_ENDIAN TRUE
#define TARG_JMP_BUF_NUM_ELEMENTS 10
#endif /* ifdef sparc */
#endif /* ifdef SUNOS */

#define TARG_CHAR_BIT 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT TRUE
#define TARG_HAS_SIGNED_CHARS TRUE
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

#ifndef TARG_SIZEOF_LONG_DOUBLE
#define TARG_SIZEOF_LONG_DOUBLE 16
#endif /* ifndef TARG_SIZEOF_LONG_DOUBLE */

#define TARG_ALIGNOF_LONG_DOUBLE 8
#define TARG_SIZEOF_WCHAR_T 4
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)

/*
Definitions for Solaris:
*/
#ifndef __BSD__
/* __BSD__ may be set if building on SunOS. */
#define __ANSIC__ 1
#endif /* ifndef __BSD__ */
/* C_GEN_BE_GENERATES_ANSI_C may be set to 0 if building on SunOS. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 1
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#ifdef SUNOS
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 1
#else /* !defined(SUNOS) */
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#endif /* ifdef SUNOS */
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 0
#define USE_PRAGMA_IDENT_IN_GENERATED_CODE 1
#define STDC_ZERO_IN_NONSTRICT_MODE 1
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST"
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1

#ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 1
#endif /* ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */

/*
Determine the C compiler being used to configure initialization handling
in the C-generating back end.
*/
#ifdef SUNOS
/* Don't configure under SunOS. */
#else /* ifndef SUNOS */
#ifdef __SUNPRO_C
#else /* ifndef __SUNPRO_C */
#ifdef __GNUC__
#define GCC_IS_C_GEN_BE_TARGET 1
#else /* ifndef __GNUC__ */
#define USE_INIT_SECTION_IN_GENERATED_C 1
#endif /* ifdef __GNUC__ */
#endif /* ifdef __SUNPRO_C */
#endif /* ifdef SUNOS */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2000 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

