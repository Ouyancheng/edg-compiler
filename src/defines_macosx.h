/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2002-2003 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/* Configuration definitions determined by dettarg.c: */
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
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 8
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_WCHAR_T 4
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 192
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)


/* Language extensions. */
#define GNU_EXTENSIONS_ALLOWED 1
#define DEFAULT_GNU_COMPATIBILITY 0
#define C99_IL_EXTENSIONS_SUPPORTED 1
#define MICROSOFT_EXTENSIONS_ALLOWED 0
#define LONG_LONG_ALLOWED 1


/* ABI selection. */
#define IA64_ABI 0
#define DEFAULT_EMULATE_GNU_ABI_BUGS 0
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES 0
#define GCC_IS_GENERATED_CODE_TARGET 1
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 1
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C 1

