/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

*/

/*
Currently, thread_local support in the IA-64 ABI requires
GNU_EXTENSIONS_ENABLED.
*/

#define IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS 0
#define IA64_ABI 1

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2017 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/* Configuration definitions determined by dettarg.c: */
#define TARG_LITTLE_ENDIAN TRUE
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS TRUE
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT TRUE
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 8
#define TARG_ALIGNOF_LONG 8
#define TARG_SIZEOF_POINTER 8
#define TARG_ALIGNOF_POINTER 8
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 16
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_int)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZEOF_LONG_LONG 8
#define TARG_ALIGNOF_LONG_LONG 8
#define HOST_ALIGNMENT_REQUIRED 8
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE FALSE
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 1

#define GNU_EXTENSIONS_ENABLED 1
#define GNU_EXTENSIONS_ALLOWED 1
#define DEFAULT_EMULATE_GNU_ABI_BUGS 1
