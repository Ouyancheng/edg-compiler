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
		targ_host_string_char_bit
#if VAR_INITIALIZERS
                                          = TARG_HOST_STRING_CHAR_BIT
#endif /* VAR_INITIALIZERS */
                                                                     ;
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

EXTERN an_integer_kind
		targ_bool_int_kind
#if VAR_INITIALIZERS
                                      = TARG_BOOL_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                          ;
			/* Integer kind associated with bool.  Initialized
			   to the default value but reconfigurable. */
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

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN an_integer_kind
		targ_int8_int_kind
#if VAR_INITIALIZERS
                                   = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                               ;
			/* Integer kind associated with __int8.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int8_int_kind
#if VAR_INITIALIZERS
                                            = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                        ;
			/* Integer kind associated with unsigned __int8.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int16_int_kind
#if VAR_INITIALIZERS
                                    = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* Integer kind associated with __int16.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int16_int_kind
#if VAR_INITIALIZERS
                                             = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* Integer kind associated with unsigned __int16.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int32_int_kind
#if VAR_INITIALIZERS
                                    = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* Integer kind associated with __int32.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int32_int_kind
#if VAR_INITIALIZERS
                                             = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* Integer kind associated with unsigned __int32.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int64_int_kind
#if VAR_INITIALIZERS
                                    = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* Integer kind associated with __int64.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int64_int_kind
#if VAR_INITIALIZERS
                                             = ((an_integer_kind)ik_none)
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* Integer kind associated with unsigned __int64.
			   Initialized to ik_none and reset later. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN an_integer_kind
		targ_intmax_kind;
			/* Integer kind associated with the largest signed
			   integer type.  In C99, this is intmax_t. */

EXTERN an_integer_kind
		targ_uintmax_kind;
			/* Integer kind associated with the largest unsigned
			   integer type.  In C99, this is uintmax_t. */

EXTERN a_targ_size_t
		targ_max_class_object_size
#if VAR_INITIALIZERS
                                           = TARG_MAX_CLASS_OBJECT_SIZE
#endif /* VAR_INITIALIZERS */
								       ;
			/* Maximum size of a class object.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN a_targ_size_t
		targ_max_base_class_offset
#if VAR_INITIALIZERS
                                           = TARG_MAX_BASE_CLASS_OFFSET
#endif /* VAR_INITIALIZERS */
								       ;
			/* Maximum offset of a base class.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN a_boolean
		targ_optimize_empty_base_class_layout
#if VAR_INITIALIZERS
                                       = TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#endif /* VAR_INITIALIZERS */
								       ;
			/* TRUE if the layout mechanism should attempt to
			   allocate empty base classes at the same offset as
			   other subobjects. */

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
		targ_microsoft_bit_field_allocation
#if VAR_INITIALIZERS
                                       = TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* If this flag is TRUE, bit-field allocation follows
			   the conventions of Microsoft C/C++.  The value of
			   targ_bit_field_container_size must be -1 and there
			   is a two-stage allocation: first, a bit-field
			   container based on the bit-field type is allocated
			   (as though it were a field in its own right), and
			   then bit fields are allocated within it.  When the
			   bit-field type changes or the container fills up,
			   a new container is allocated. */

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
		targ_force_one_bit_bit_field_to_be_unsigned
#if VAR_INITIALIZERS
                                 = TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE when a "plain" int bit field of length 1 is
			   to be treated as unsigned regardless of the
			   setting of targ_plain_int_bit_field_is_unsigned
			   (because a bit field consisting of only a sign
			   is not very useful). */

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

EXTERN int	targ_zero_width_bit_field_alignment
#if VAR_INITIALIZERS
                                         = TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Alignment adjustment to be made when a zero-width
			   (unnamed) bit field is declared.  If > 0 it is the
			   alignment to be used (typically the alignment of
			   one of the integral types, in which case the value
			   should be cast to a_targ_alignment).  A value of
			   zero means "use the minimal alignment", which is
			   single-byte alignment.  Any value less than zero
			   means "use the alignment of the base type given in
			   the declaration". */

EXTERN int	targ_zero_width_bit_field_affects_struct_alignment
#if VAR_INITIALIZERS
                         = TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* TRUE when the alignment adjustment when a
			   zero-width (unnamed) bit-field is declared
			   affects the overall alignment of the struct as
			   well as the alignment of the next field. */

EXTERN int	targ_unnamed_bit_field_affects_struct_alignment
#if VAR_INITIALIZERS
                            = TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if the alignment adjustment when an unnamed
			   bit-field is declared affects the overall alignment
			   of the struct as well as the alignment of the next
			   field. */

EXTERN int	targ_user_control_of_struct_packing_affects_bit_fields
#if VAR_INITIALIZERS
                     = TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if "#pragma pack(n)" and the command-line
			   option "--pack_alignment=n" affect the alignment of
			   bit field containers (when bit fields straddle
			   container alignment boundaries). */

EXTERN int	targ_pad_bit_fields_larger_than_base_type
#if VAR_INITIALIZERS
                                  = TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if bit fields longer than their base types are
			   padded out to the full declared length.  FALSE
			   means allocate only as many bits as are in the
			   base type.  In either case, the bit field itself
			   has the same number of bits; the extra bits are
			   padding bits. */

/*
Pointer types:
*/
#if TARG_ALL_POINTERS_SAME_SIZE
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
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
#if NEAR_AND_FAR_ALLOWED
EXTERN a_targ_size_t
		targ_sizeof_far_pointer
#if VAR_INITIALIZERS
                                        = TARG_SIZEOF_FAR_POINTER
#endif /* VAR_INITIALIZERS */
                                                                 ;
			/* Size of a far pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN a_targ_alignment
		targ_alignof_far_pointer
#if VAR_INITIALIZERS
                                         = TARG_ALIGNOF_FAR_POINTER
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* Alignment of a far pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
EXTERN a_targ_size_t
		targ_sizeof_near_pointer
#if VAR_INITIALIZERS
                                         = TARG_SIZEOF_NEAR_POINTER
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* Size of a near pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN a_targ_alignment
		targ_alignof_near_pointer
#if VAR_INITIALIZERS
                                          = TARG_ALIGNOF_NEAR_POINTER
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* Alignment of a near pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
#endif /* NEAR_AND_FAR_ALLOWED */

EXTERN a_targ_ptrdiff_t
		targ_ptrdiff_t_max
#if VAR_INITIALIZERS
                                   = TARG_PTRDIFF_T_MAX
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Maximum ptrdiff_t value. Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_ptrdiff_t
		targ_ptrdiff_t_min
#if VAR_INITIALIZERS
                                   = TARG_PTRDIFF_T_MIN
#endif /* VAR_INITIALIZERS */
                                                       ;
			/* Minimum ptrdiff_t value.  Initialized to the default
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

#if GNU_EXTENSIONS_ALLOWED

EXTERN a_type_mode_kind
		targ_word_mode
#if VAR_INITIALIZERS
                               = (a_type_mode_kind)TARG_WORD_MODE
#endif /* VAR_INITIALIZERS */
                                                                 ;
			/* Mode of a word, i.e., the natural integer
			   size for the target.  Initialized to the
			   default value but reconfigurable. */

#if TARG_ALL_POINTERS_SAME_SIZE

EXTERN a_type_mode_kind
		targ_pointer_mode
#if VAR_INITIALIZERS
                                  = (a_type_mode_kind)TARG_POINTER_MODE
#endif /* VAR_INITIALIZERS */
                                                                       ;
			/* Mode of a pointer.  Initialized to the
			   default value but reconfigurable. */

#endif /* TARG_ALL_POINTERS_SAME_SIZE */

#endif /* GNU_EXTENSIONS_ALLOWED */

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

#if !IA64_ABI
/*
Pointer to virtual base class.
*/
EXTERN a_targ_size_t
		targ_sizeof_ptr_to_virtual_base_class
#if VAR_INITIALIZERS
                                    = TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#endif /* VAR_INITIALIZERS */
                                                         ;
			/* Size of a "pointer-to-virtual-base-class" member.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_virtual_base_class
#if VAR_INITIALIZERS
                                     = TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#endif /* VAR_INITIALIZERS */
                                                           ;
			/* Alignment of a "pointer-to-virtual-base-class"
			   member.  Initialized to the default value but
			   reconfigurable. */
#endif /* !IA64_ABI */

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

#if USER_CONTROL_OF_STRUCT_PACKING
EXTERN a_targ_alignment
		targ_minimum_pack_alignment
#if VAR_INITIALIZERS
                                            = TARG_MINIMUM_PACK_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* The minimum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_maximum_pack_alignment
#if VAR_INITIALIZERS
                                            = TARG_MAXIMUM_PACK_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                         ;
			/* The maximum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_maximum_intrinsic_alignment
#if VAR_INITIALIZERS
                                            = TARG_MAXIMUM_INTRINSIC_ALIGNMENT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* The maximum alignment value which the target can
			   take advantage of.  Initialized to the default
			   value but reconfigurable. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

EXTERN a_boolean
		distinct_template_signatures
#if VAR_INITIALIZERS
                                      = DEFAULT_DISTINCT_TEMPLATE_SIGNATURES
#endif /* VAR_INITIALIZERS */
                                               ;
			/* If TRUE, template functions are given mangled names
			   that are distinct from the names for nontemplate
			   functions. */

#if DO_IL_LOWERING

EXTERN a_boolean
		force_variable_definition_via_zeroing
#if VAR_INITIALIZERS
                                       = FORCE_VARIABLE_DEFINITION_VIA_ZEROING
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* If TRUE, add zeroing to variable definitions
			   to make them definitions in C. */

EXTERN a_boolean
		make_all_functions_unprototyped
#if VAR_INITIALIZERS
                                          = MAKE_ALL_FUNCTIONS_UNPROTOTYPED
#endif /* VAR_INITIALIZERS */
                                                                           ;
			/* If TRUE, all functions are rewritten to be
			   unprototyped. */

#if DO_FULL_PORTABLE_EH_LOWERING

EXTERN unsigned int
		targ_jmp_buf_num_elements
#if VAR_INITIALIZERS
                                          = TARG_JMP_BUF_NUM_ELEMENTS
#endif /* VAR_INITIALIZERS */
                                                                     ;
			/* Number of elements in a jmp_buf array.  Initialized
			   to the default value but reconfigurable. */
EXTERN a_boolean
		targ_jmp_buf_elements_are_float
#if VAR_INITIALIZERS
                                              = TARG_JMP_BUF_ELEMENTS_ARE_FLOAT
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* Choose between integer and float members of the
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN an_integer_kind
		targ_jmp_buf_element_int_kind
#if VAR_INITIALIZERS
                                              = TARG_JMP_BUF_ELEMENT_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* Integer kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN a_float_kind
		targ_jmp_buf_element_float_kind
#if VAR_INITIALIZERS
                                              = TARG_JMP_BUF_ELEMENT_FLOAT_KIND
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* Float kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if GENERATE_EH_TABLES
EXTERN an_integer_kind
		targ_var_handle_int_kind
#if VAR_INITIALIZERS
                                         = TARG_VAR_HANDLE_INT_KIND
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* Integer kind to be used for a "handle" in
			   exception handling tables. */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */

EXTERN int	targ_flt_mant_dig
#if VAR_INITIALIZERS
                                  = TARG_FLT_MANT_DIG
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The number of bits in the mantissa of a float. */

EXTERN int	targ_flt_min_exp
#if VAR_INITIALIZERS
                                  = TARG_FLT_MIN_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The minimum exponent value of a float. */

EXTERN int	targ_flt_max_exp
#if VAR_INITIALIZERS
                                  = TARG_FLT_MAX_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The maximum exponent value of a float. */

EXTERN int	targ_dbl_mant_dig
#if VAR_INITIALIZERS
                                  = TARG_DBL_MANT_DIG
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The number of bits in the mantissa of a double. */

EXTERN int	targ_dbl_min_exp
#if VAR_INITIALIZERS
                                  = TARG_DBL_MIN_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The minimum exponent value of a double. */

EXTERN int	targ_dbl_max_exp
#if VAR_INITIALIZERS
                                  = TARG_DBL_MAX_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The maximum exponent value of a double. */

EXTERN int	targ_ldbl_mant_dig
#if VAR_INITIALIZERS
                                  = TARG_LDBL_MANT_DIG
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The number of bits in the mantissa of a long
                           double. */

EXTERN int	targ_ldbl_min_exp
#if VAR_INITIALIZERS
                                  = TARG_LDBL_MIN_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The minimum exponent value of a long double. */

EXTERN int	targ_ldbl_max_exp
#if VAR_INITIALIZERS
                                  = TARG_LDBL_MAX_EXP
#endif /* VAR_INITIALIZERS */
                                                     ;
			/* The maximum exponent value of a long double. */

EXTERN a_boolean
		remove_qualifiers_from_param_types
#if VAR_INITIALIZERS
                                = DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES
#endif /* VAR_INITIALIZERS */
                                                                            ;
			/* True when type qualifiers should be removed from
			   function parameter types (e.g., a "const int"
			   parameter is seen simply as "int"). */

EXTERN a_boolean
		c_and_cpp_function_types_are_distinct
#if VAR_INITIALIZERS
                              = DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* If TRUE, function types are considered distinct if
			   their only difference is that one has extern "C"
			   routine linkage and the other has extern "C++"
			   routine linkage.  This affects, among other things,
			   overload resolution and name mangling.  (See also
			   impl_conv_between_c_and_cpp_function_ptrs_allowed,
			   defined in cmd_line.h.) */

#if BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		old_specializations_for_generated_instances
#if VAR_INITIALIZERS
                         = DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* If TRUE, specializations for generated template
			   instances in generated code (C++-generating back
			   end) should use the old syntax instead of the
			   modern "template <>" prefix form. */
#endif /* BACK_END_IS_CP_GEN_BE */

EXTERN a_boolean
		type_info_in_namespace_std
#if VAR_INITIALIZERS
                            = DEFAULT_TYPE_INFO_IN_NAMESPACE_STD
#endif /* VAR_INITIALIZERS */
                                                                ;
			/* If TRUE,  class type_info is defined as a member
			   of namespace "std". */

EXTERN a_boolean
		pass_stdarg_references_to_generated_code
#if VAR_INITIALIZERS
                            = DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE
#endif /* VAR_INITIALIZERS */
                                                                              ;
			/* If TRUE, references to the macros in <stdarg.h>
			   are passed through to the output unchanged. */


EXTERN a_boolean
		instantiate_extern_inline
#if VAR_INITIALIZERS
                            = INSTANTIATE_EXTERN_INLINE
#endif /* VAR_INITIALIZERS */
                                 ;
			/* TRUE if the instantiation mechanism should be used
			   to control the definition of extern inline
			   functions. */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		msvc_is_generated_code_target
#if VAR_INITIALIZERS
                                              = MSVC_IS_GENERATED_CODE_TARGET
#endif /* VAR_INITIALIZERS */
                                                                             ;
			/* TRUE if code is being generated for the Microsoft
			   MSVC++ compiler. */

EXTERN int	msvc_target_version
#if VAR_INITIALIZERS
                                              = MSVC_TARGET_VERSION
#endif /* VAR_INITIALIZERS */
                                                                   ;
			/* The version number (i.e., 1300 for 7.0) of the
			   Microsoft MSVC compiler being targeted. */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		msvc_target_version_number
#if VAR_INITIALIZERS
                                           = DEFAULT_MICROSOFT_VERSION
#endif /* VAR_INITIALIZERS */
                                                                      ;
			/* The version of MSVC++ being targeted. */
#endif /* BACK_END_IS_CP_GEN_BE */


/* Aside from occasional references in targ_def.h, the following values
   should be used *only* to initialize the variables declared in this file.
   To enforce this convention, they are undefined at this time.  (This is
   not foolproof, but it should catch most such misuses).  */
#undef TARG_LITTLE_ENDIAN
#undef TARG_CHAR_BIT
#undef TARG_HOST_STRING_CHAR_BIT
#undef TARG_HAS_SIGNED_CHARS
#undef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#undef TARG_WCHAR_T_INT_KIND
#undef TARG_SIZEOF_WCHAR_T
#undef TARG_BOOL_INT_KIND
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
#undef TARG_MAX_CLASS_OBJECT_SIZE
#undef TARG_MAX_BASE_CLASS_OFFSET
#undef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#undef TARG_BIT_FIELD_CONTAINER_SIZE
#undef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#undef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#undef TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED
#undef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#undef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#undef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS
#undef TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE
#if TARG_ALL_POINTERS_SAME_SIZE
#undef TARG_SIZEOF_POINTER
#undef TARG_ALIGNOF_POINTER
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
#if MICROSOFT_EXTENSIONS_ALLOWED
#undef TARG_SIZEOF_FAR_POINTER
#undef TARG_ALIGNOF_FAR_POINTER
#undef TARG_SIZEOF_NEAR_POINTER
#undef TARG_ALIGNOF_NEAR_POINTER
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#undef TARG_POINTER_MODE
#undef TARG_WORD_MODE
#endif /* GNU_EXTENSIONS_ALLOWED */
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
#undef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#undef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#undef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#undef TARG_RIGHT_SHIFT_IS_ARITHMETIC
#undef TARG_MINIMUM_STRUCT_ALIGNMENT
#undef TARG_MINIMUM_PACK_ALIGNMENT
#undef MAKE_ALL_FUNCTIONS_UNPROTOTYPED
#undef TARG_JMP_BUF_NUM_ELEMENTS
#undef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT
#undef TARG_JMP_BUF_ELEMENT_INT_KIND
#undef TARG_JMP_BUF_ELEMENT_FLOAT_KIND
#undef TARG_VAR_HANDLE_INT_KIND
#undef TARG_FLT_MANT_DIG
#undef TARG_FLT_MIN_EXP
#undef TARG_FLT_MAX_EXP
#undef TARG_DBL_MANT_DIG
#undef TARG_DBL_MIN_EXP
#undef TARG_DBL_MAX_EXP
#undef TARG_LDBL_MANT_DIG
#undef TARG_LDBL_MIN_EXP
#undef TARG_LDBL_MAX_EXP
#undef MSVC_IS_GENERATED_CODE_TARGET
#undef MSVC_TARGET_VERSION

#ifndef MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define MAKE_TARG_NAMES_REFER_TO_VARIABLES 0
#endif /* ifndef MAKE_TARG_NAMES_REFER_TO_VARIABLES */
/* The following macro name redefinitions are provided to help accommodate
   implementations that have code of their own that depends on these names'
   being defined.  These TARG_xxx names should not reappear in code supplied
   by EDG. */
#if MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define TARG_LITTLE_ENDIAN targ_little_endian
#define TARG_CHAR_BIT targ_char_bit
#define TARG_HOST_STRING_CHAR_BIT targ_host_string_char_bit
#define TARG_HAS_SIGNED_CHARS targ_has_signed_chars
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT                   \
                        targ_char_constant_first_char_most_significant
#define TARG_WCHAR_T_INT_KIND targ_wchar_t_int_kind
#define TARG_SIZEOF_WCHAR_T targ_sizeof_wchar_t
#define TARG_SIZEOF_SHORT targ_sizeof_short
#define TARG_ALIGNOF_SHORT targ_alignof_short
#define TARG_SIZEOF_INT targ_sizeof_int
#define TARG_ALIGNOF_INT targ_alignof_int
#define TARG_SIZEOF_LONG targ_sizeof_long
#define TARG_ALIGNOF_LONG targ_alignof_long
#if LONG_LONG_ALLOWED
#define TARG_SIZEOF_LONG_LONG targ_sizeof_long_long
#define TARG_ALIGNOF_LONG_LONG targ_alignof_long_long
#endif /* LONG_LONG_ALLOWED */
#define TARG_MAX_CLASS_OBJECT_SIZE targ_max_class_object_size
#define TARG_MAX_BASE_CLASS_OFFSET targ_max_base_class_offset
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT                           \
                        targ_optimize_empty_base_class_layout
#define TARG_BIT_FIELD_CONTAINER_SIZE targ_bit_field_container_size
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION targ_microsoft_bit_field_allocation
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED                            \
                        targ_plain_int_bit_field_is_unsigned
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED                     \
                        targ_force_one_bit_bit_field_to_be_unsigned
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED                        \
                        targ_enum_bit_fields_are_always_unsigned
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT                             \
                        targ_zero_width_bit_field_alignment
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS          \
                        targ_user_control_of_struct_packing_affects_bit_fields
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE \
                        targ_pad_bit_fields_larger_than_base_type
#if TARG_ALL_POINTERS_SAME_SIZE
#define TARG_SIZEOF_POINTER targ_sizeof_pointer
#define TARG_ALIGNOF_POINTER targ_alignof_pointer
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define TARG_SIZEOF_FAR_POINTER targ_sizeof_far_pointer
#define TARG_ALIGNOF_FAR_POINTER targ_alignof_far_pointer
#define TARG_SIZEOF_NEAR_POINTER targ_sizeof_near_pointer
#define TARG_ALIGNOF_NEAR_POINTER targ_alignof_near_pointer
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
#define TARG_POINTER_MODE targ_pointer_mode
#define TARG_WORD_MODE targ_word_mode
#endif /* GNU_EXTENSIONS_ALLOWED */
#define TARG_PTRDIFF_T_MAX targ_ptrdiff_t_max
#define TARG_PTRDIFF_T_MIN targ_ptrdiff_t_min
#define TARG_PTRDIFF_T_INT_KIND targ_ptrdiff_t_int_kind
#define TARG_SIZE_T_MAX targ_size_t_max
#define TARG_SIZE_T_INT_KIND targ_size_t_int_kind
#define TARG_SIZEOF_FLOAT targ_sizeof_float
#define TARG_ALIGNOF_FLOAT targ_alignof_float
#define TARG_SIZEOF_DOUBLE targ_sizeof_double
#define TARG_ALIGNOF_DOUBLE targ_alignof_double
#define TARG_SIZEOF_LONG_DOUBLE targ_sizeof_long_double
#define TARG_ALIGNOF_LONG_DOUBLE targ_alignof_long_double
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER targ_sizeof_ptr_to_data_member
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER targ_alignof_ptr_to_data_member
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                              \
                        targ_sizeof_ptr_to_member_function
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION                             \
                        targ_alignof_ptr_to_member_function
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO                               \
                        targ_sizeof_virtual_function_info
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO                              \
                        targ_alignof_virtual_function_info
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS                           \
                        targ_sizeof_ptr_to_virtual_base_class
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS                          \
                        targ_alignof_ptr_to_virtual_base_class
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT                         \
                        targ_enum_types_can_be_smaller_than_int
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC targ_right_shift_is_arithmetic
#define TARG_MINIMUM_STRUCT_ALIGNMENT targ_minimum_struct_alignment
#define TARG_MINIMUM_PACK_ALIGNMENT targ_minimum_pack_alignment
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED make_all_functions_unprototyped
#define TARG_JMP_BUF_NUM_ELEMENTS targ_jmp_buf_num_elements
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT targ_jmp_buf_elements_are_float
#define TARG_JMP_BUF_ELEMENT_INT_KIND targ_jmp_buf_element_int_kind
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND targ_jmp_buf_element_float_kind
#define TARG_VAR_HANDLE_INT_KIND targ_var_handle_int_kind
#define TARG_FLT_MANT_DIG targ_flt_mant_dig
#define TARG_FLT_MIN_EXP targ_flt_min_exp
#define TARG_FLT_MAX_EXP targ_flt_max_exp
#define TARG_DBL_MANT_DIG targ_dbl_mant_dig
#define TARG_DBL_MIN_EXP targ_dbl_min_exp
#define TARG_DBL_MAX_EXP targ_dbl_max_exp
#define TARG_LDBL_MANT_DIG targ_ldbl_mant_dig
#define TARG_LDBL_MIN_EXP targ_ldbl_min_exp
#define TARG_LDBL_MAX_EXP targ_ldbl_max_exp
#define MSVC_IS_GENERATED_CODE_TARGET msvc_is_generated_code_target
#define MSVC_TARGET_VERSION msvc_target_version
#endif /* MAKE_TARG_NAMES_REFER_TO_VARIABLES */

extern void set_plain_char_int_kind(a_boolean plain_chars_are_signed);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void init_microsoft_sized_int_types(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_targ_size_t size_of_pointer_to(a_type_ptr        type_pointed_to,
                                 a_targ_alignment  *alignment);

#if CHECKING
extern void check_target_configuration(void);
#endif /* CHECKING */

extern void target_one_time_init(void);

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
