/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

target.h -- Declaration of variables specifying target machine characteristics.

Note that target.h declares configuration variables that, in principle, can
be reset whenever the compiler is invoked, whereas targ_def.h contains
configuration values that are incorporated when the compiler is built
(#define values, typedefs).

*/

/* Avoid including these declarations more than once: */
#ifndef TARGET_H
#define TARGET_H 1

#ifndef TARG_DEF_H
#include "targ_def.h"
#endif /* ifndef TARG_DEF_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/*
Except as noted, the following variables are initialized to values defined
for the expected target machine but may be reset to permit reconfiguring
the EDG front end to different targets with each invocation.
*/
EXTERN a_boolean
		targ_little_endian
#if VAR_INITIALIZERS
                                   = TARG_LITTLE_ENDIAN
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* When TRUE the least significant part of a multi-byte
			   target integer is at the lowest memory address. */

/*
Char types:
*/
EXTERN unsigned int
		targ_char_bit
#if VAR_INITIALIZERS
                              = TARG_CHAR_BIT
#endif /* VAR_INITIALIZERS */
                                             ;
			/* Number of bits in a target char. */

EXTERN unsigned int
		targ_host_string_char_bit;
			/* The number of data bits per character used when
			   representing target characters as a string on the
			   host; dependent on targ_char_bit and CHAR_BIT.
			   One is allowed to make the target char larger than
			   the host char, but individual characters in string
			   literals will be limited by what is representable
			   in a host char. */

EXTERN a_boolean
		targ_has_signed_chars
#if VAR_INITIALIZERS
                                      = TARG_HAS_SIGNED_CHARS
#endif /* VAR_INITIALIZERS */
                                                             ;
			/* TRUE if the target has signed characters.  This
			   is selectable on the command line. */

EXTERN a_boolean
		targ_char_constant_first_char_most_significant
#if VAR_INITIALIZERS
                              = TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE when the first character in a target char
			   constant is most significant -- e.g., 'ab' == 0x6162
			   instead of 0x6261. */

EXTERN an_integer_kind
		targ_wchar_t_int_kind
#if VAR_INITIALIZERS
                                      = TARG_WCHAR_T_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                             ;
			/* Integer kind associated with wchar_t.  Initialized
			   to the default value but reconfigurable. */
EXTERN a_targ_size_t
		targ_sizeof_wchar_t
#if VAR_INITIALIZERS
                                    = TARG_SIZEOF_WCHAR_T
#endif /* VAR_INITIALIZERS */
                                                         ;
			/* Size of a wchar_t entity.  Initialized to the
			   default value but reconfigurable. */

/*
Integer types:
*/
EXTERN a_targ_size_t
		targ_sizeof_short
#if VAR_INITIALIZERS
                                  = TARG_SIZEOF_SHORT
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* Size of a short int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_short
#if VAR_INITIALIZERS
                                   = TARG_ALIGNOF_SHORT
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Alignment of a short int.  Initialized to the
			   default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_int
#if VAR_INITIALIZERS
                                = TARG_SIZEOF_INT
#endif /* VAR_INITIALIZERS */
                                                 ;
			/* Size of an int.  Initialized to the default value
			   but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_int
#if VAR_INITIALIZERS
                                 = TARG_ALIGNOF_INT
#endif /* VAR_INITIALIZERS */
                                                   ;
			/* Alignment of an int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_long
#if VAR_INITIALIZERS
                                 = TARG_SIZEOF_LONG
#endif /* VAR_INITIALIZERS */
                                                   ;
			/* Size of a long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long
#if VAR_INITIALIZERS
                                  = TARG_ALIGNOF_LONG
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* Alignment of a long int.  Initialized to the
			   default value but reconfigurable. */

#if LONG_LONG_ALLOWED
EXTERN a_targ_size_t
		targ_sizeof_long_long
#if VAR_INITIALIZERS
                                      = TARG_SIZEOF_LONG_LONG
#endif /* VAR_INITIALIZERS */
                                                             ;
			/* Size of a long long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long_long
#if VAR_INITIALIZERS
                                       = TARG_ALIGNOF_LONG_LONG
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* Alignment of a long long int.  Initialized to the
			   default value but reconfigurable. */
#endif /* LONG_LONG_ALLOWED */

EXTERN a_targ_size_t
		targ_max_bit_field_size
#if VAR_INITIALIZERS
                                        = TARG_MAX_BIT_FIELD_SIZE
#endif /* VAR_INITIALIZERS */
                                                                 ;
			/* Maximum size of a bit field.  Initialized to the
			   default value but reconfigurable. */

EXTERN int	targ_bit_field_container_size
#if VAR_INITIALIZERS
                                              = TARG_BIT_FIELD_CONTAINER_SIZE
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* Container size to be used for bit-fields.  If > 0,
			   indicates the size in bytes of one of the integral
			   types.  0 means "use the smallest integral type
			   into which the field will fit".  < 0 means "use the
			   base type given in the declaration".  Initialized
			   to the default value but reconfigurable. */

EXTERN a_boolean
		targ_plain_int_bit_field_is_unsigned
#if VAR_INITIALIZERS
                                        = TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE when a "plain" int bit field is to be treated
			   as unsigned.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_boolean
		targ_enum_bit_fields_are_always_unsigned
#if VAR_INITIALIZERS
                                    = TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Signedness for enum bit fields (an extension): if
			   TRUE, enum bit fields are always unsigned.  If
			   FALSE, the rules are as described in target.h: it
			   depends on the signedness and size of the values of
			   the enum and defaults to the signedness indicated
			   by targ_plain_int_bit_field_is_unsigned. */

/*
Pointer types:
*/
EXTERN a_targ_size_t
		targ_sizeof_pointer
#if VAR_INITIALIZERS
                                    = TARG_SIZEOF_POINTER
#endif /* VAR_INITIALIZERS */
                                                         ;
			/* Size of a pointer.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_pointer
#if VAR_INITIALIZERS
                                     = TARG_ALIGNOF_POINTER
#endif /* VAR_INITIALIZERS */
                                                           ;
			/* Alignment of a pointer.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_ptrdiff_t
		targ_ptrdiff_t_max
#if VAR_INITIALIZERS
                                   = TARG_PTRDIFF_T_MAX
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Alignment of a pointer.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_ptrdiff_t
		targ_ptrdiff_t_min
#if VAR_INITIALIZERS
                                   = TARG_PTRDIFF_T_MIN
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Alignment of a pointer.  Initialized to the default
			   value but reconfigurable. */

EXTERN an_integer_kind
		targ_ptrdiff_t_int_kind
#if VAR_INITIALIZERS
                                        = TARG_PTRDIFF_T_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                                 ;
			/* Representation for ptrdiff_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_size_t_max
#if VAR_INITIALIZERS
                                = TARG_SIZE_T_MAX
#endif /* VAR_INITIALIZERS */
                                                 ;
			/* The limit of the host representation of size_t
			   constants; the range it defines can be equal to
			   or smaller than the integer size implied by
			   TARG_SIZE_T_INT_KIND.  Initialized to the default
			   value but reconfigurable. */

EXTERN an_integer_kind
		targ_size_t_int_kind
#if VAR_INITIALIZERS
                                     = TARG_SIZE_T_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                           ;
			/* Representation for size_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

/*
Float types:
*/
EXTERN a_targ_size_t
		targ_sizeof_float
#if VAR_INITIALIZERS
                                  = TARG_SIZEOF_FLOAT
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* Size of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_float
#if VAR_INITIALIZERS
                                   = TARG_ALIGNOF_FLOAT
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Alignment of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_double
#if VAR_INITIALIZERS
                                   = TARG_SIZEOF_DOUBLE
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Size of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_double
#if VAR_INITIALIZERS
                                    = TARG_ALIGNOF_DOUBLE
#endif /* VAR_INITIALIZERS */
                                                         ;
			/* Alignment of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_long_double
#if VAR_INITIALIZERS
                                        = TARG_SIZEOF_LONG_DOUBLE
#endif /* VAR_INITIALIZERS */
                                                                 ;
			/* Size of a long double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long_double
#if VAR_INITIALIZERS
                                         = TARG_ALIGNOF_LONG_DOUBLE
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* Alignment of a long double.  Initialized to the
			   default value but reconfigurable. */

/*
C++ pointer-to-member type.
*/
EXTERN a_targ_size_t
		targ_sizeof_ptr_to_data_member
#if VAR_INITIALIZERS
                                              = TARG_SIZEOF_PTR_TO_DATA_MEMBER
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Size of a pointer-to-data-member.  Initialized to
			   the default value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_data_member
#if VAR_INITIALIZERS
                                             = TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Alignment of a pointer-to-data-member.  Initialized
			   to the default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_ptr_to_member_function
#if VAR_INITIALIZERS
                                          = TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Size of a ptr-to-member-function.  Initialized to
			   the default value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_member_function
#if VAR_INITIALIZERS
                                         = TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Alignment of a pointer-to-member-function.
			   Initialized to the default value but
			   reconfigurable. */


/*
Virtual function info.
*/
EXTERN a_targ_size_t
		targ_sizeof_virtual_function_info
#if VAR_INITIALIZERS
                                           = TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Size of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_virtual_function_info
#if VAR_INITIALIZERS
                                          = TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Alignment of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

/*
Miscellaneous
*/
EXTERN a_boolean
		targ_enum_types_can_be_smaller_than_int
#if VAR_INITIALIZERS
                                     = TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* When TRUE, enum types will be allocated in the
			   smallest integral type in which they will fit; if
			   FALSE, int will be used (e.g., for cfront
			   compatibility).  Initialized to the default value
			   but reconfigurable. */

EXTERN a_boolean
		targ_right_shift_is_arithmetic
#if VAR_INITIALIZERS
                                              = TARG_RIGHT_SHIFT_IS_ARITHMETIC
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* When TRUE a right shift on a signed quantity does
			   sign extension.  Initialized to the default value
			   but reconfigurable. */

EXTERN a_targ_alignment
		targ_minimum_struct_alignment
#if VAR_INITIALIZERS
                                              = TARG_MINIMUM_STRUCT_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* The minimum alignment required for objects of class,
			   struct, and union type in the target environment.
			   Initialized to the default value but
			   reconfigurable. */

#if DO_IL_LOWERING

EXTERN unsigned int
		targ_jmp_buf_num_elements
#if VAR_INITIALIZERS
                                          = TARG_JMP_BUF_NUM_ELEMENTS
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* Number of elements in a jmp_buf array.  Initialized
			   to the default value but reconfigurable. */

EXTERN an_integer_kind
		targ_jmp_buf_element_int_kind
#if VAR_INITIALIZERS
                                              = TARG_JMP_BUF_ELEMENT_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* Integer kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */

#endif /* DO_IL_LOWERING */

#if CHECKING
/* Aside from occasional references in targ_def.h, the following values
   should be used *only* to initialize the variables declared in this file.
   To enforce this convention, they are undefined at this time.  (This is
   not foolproof, but it should catch most such misuses).  */
#undef TARG_CHAR_BIT
#undef TARG_LITTLE_ENDIAN
#undef TARG_HAS_SIGNED_CHARS
#undef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#undef TARG_WCHAR_T_INT_KIND
#undef TARG_SIZEOF_WCHAR_T
#undef TARG_SIZEOF_SHORT
#undef TARG_ALIGNOF_SHORT
#undef TARG_SIZEOF_INT
#undef TARG_ALIGNOF_INT
#undef TARG_SIZEOF_LONG
#undef TARG_ALIGNOF_LONG
#if LONG_LONG_ALLOWED
#undef TARG_SIZEOF_LONG_LONG
#undef TARG_ALIGNOF_LONG_LONG
#endif /* LONG_LONG_ALLOWED */
#undef TARG_MAX_BIT_FIELD_SIZE
#undef TARG_BIT_FIELD_CONTAINER_SIZE
#undef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#undef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#undef TARG_SIZEOF_POINTER
#undef TARG_ALIGNOF_POINTER
#undef TARG_PTRDIFF_T_MAX
#undef TARG_PTRDIFF_T_MIN
#undef TARG_PTRDIFF_T_INT_KIND
#undef TARG_SIZE_T_MAX
#undef TARG_SIZE_T_INT_KIND
#undef TARG_SIZEOF_FLOAT
#undef TARG_ALIGNOF_FLOAT
#undef TARG_SIZEOF_DOUBLE
#undef TARG_ALIGNOF_DOUBLE
#undef TARG_SIZEOF_LONG_DOUBLE
#undef TARG_ALIGNOF_LONG_DOUBLE
#undef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#undef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#undef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#undef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#undef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#undef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#undef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#undef TARG_RIGHT_SHIFT_IS_ARITHMETIC
#undef TARG_MINIMUM_STRUCT_ALIGNMENT
#undef TARG_JMP_BUF_NUM_ELEMENTS
#undef TARG_JMP_BUF_ELEMENT_INT_KIND
#endif /* CHECKING */

#endif /* ifndef TARGET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
