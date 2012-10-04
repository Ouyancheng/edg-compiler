/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
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
		targ_little_endian;
			/* When TRUE the least significant part of a multi-byte
			   target integer is at the lowest memory address. */

/*
Char types:
*/
EXTERN unsigned int
		targ_char_bit;
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
		targ_has_signed_chars;
			/* TRUE if the target has signed characters.  This
			   is selectable on the command line. */

EXTERN a_boolean
		targ_char_constant_first_char_most_significant;
			/* TRUE when the first character in a target char
			   constant is most significant -- e.g., 'ab' == 0x6162
			   instead of 0x6261. */

EXTERN an_integer_kind
		targ_wchar_t_int_kind;
			/* Integer kind associated with wchar_t.  Initialized
			   to the default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_wchar_t;
			/* Size of a wchar_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN an_integer_kind
		targ_wint_t_int_kind;
			/* Integer kind associated with wint_t.  Initialized
			   to the default value but reconfigurable. */

EXTERN an_integer_kind
		targ_char16_t_int_kind;
			/* Integer kind associated with char16_t.  Initialized
			   to the default value but reconfigurable. */
EXTERN a_targ_size_t
		targ_sizeof_char16_t;
			/* Size of a char16_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN an_integer_kind
		targ_char32_t_int_kind;
			/* Integer kind associated with char32_t.  Initialized
			   to the default value but reconfigurable. */
EXTERN a_targ_size_t
		targ_sizeof_char32_t;
			/* Size of a char32_t entity.  Initialized to the
			   default value but reconfigurable. */

EXTERN a_targ_size_t
		character_size[(int)chk_last];
			/* A table of sizes for the various character kinds. */

EXTERN an_integer_kind
		targ_bool_int_kind;
			/* Integer kind associated with bool.  Initialized
			   to the default value but reconfigurable. */
/*
Integer types:
*/
EXTERN a_targ_size_t
		targ_sizeof_short;
			/* Size of a short int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_short;
			/* Alignment of a short int.  Initialized to the
			   default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_int;
			/* Size of an int.  Initialized to the default value
			   but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_int;
			/* Alignment of an int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_long;
			/* Size of a long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long;
			/* Alignment of a long int.  Initialized to the
			   default value but reconfigurable. */

#if LONG_LONG_ALLOWED
EXTERN a_targ_size_t
		targ_sizeof_long_long;
			/* Size of a long long int.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long_long;
			/* Alignment of a long long int.  Initialized to the
			   default value but reconfigurable. */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
EXTERN a_targ_size_t
		targ_sizeof_int128;
			/* Size of a 128-bit integer type.  Initialized to the
			   default value but reconfigurable (in practice, this
			   almost certainly equals 16). */

EXTERN a_targ_alignment
		targ_alignof_int128;
			/* Alignment of a 128-bit integer type.  Initialized to
			   the default value but reconfigurable (in practice,
			   this almost certainly equals 16). */
#endif /* INT128_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_boolean
		is_64bit_target;
			/* TRUE if the target platform is a 64-bit target;
			   i.e., TRUE if size_t is 64 bits wide. */

EXTERN an_integer_kind
		targ_int8_int_kind;
			/* Integer kind associated with __int8.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int8_int_kind;
			/* Integer kind associated with unsigned __int8.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int16_int_kind;
			/* Integer kind associated with __int16.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int16_int_kind;
			/* Integer kind associated with unsigned __int16.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int32_int_kind;
			/* Integer kind associated with __int32.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int32_int_kind;
			/* Integer kind associated with unsigned __int32.
			   Initialized to ik_none and reset later. */

EXTERN an_integer_kind
		targ_int64_int_kind;
			/* Integer kind associated with __int64.  Initialized
			   to ik_none and reset later. */

EXTERN an_integer_kind
		targ_unsigned_int64_int_kind;
			/* Integer kind associated with unsigned __int64.
			   Initialized to ik_none and reset later. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN an_integer_kind
		targ_intmax_kind;
			/* Integer kind associated with the largest signed
			   integer type (excluding ik_int128).  In C99, this
			   is intmax_t. */

EXTERN an_integer_kind
		targ_uintmax_kind;
			/* Integer kind associated with the largest unsigned
			   integer type (excluding ik_unsigned_int128).  In
			   C99, this is uintmax_t. */

EXTERN a_targ_size_t
		targ_max_class_object_size;
			/* Maximum size of a class object.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN a_targ_size_t
		targ_max_base_class_offset;
			/* Maximum offset of a base class.  Initialized to the
			   default value but may be reset in target_init. */

EXTERN a_boolean
		targ_optimize_empty_base_class_layout;
			/* TRUE if the layout mechanism should attempt to
			   allocate empty base classes at the same offset as
			   other subobjects. */

EXTERN int	targ_bit_field_container_size;
			/* Container size to be used for bit-fields.  If > 0,
			   indicates the size in bytes of one of the integral
			   types.  0 means "use the smallest integral type
			   into which the field will fit".  < 0 means "use the
			   base type given in the declaration".  Initialized
			   to the default value but reconfigurable. */

EXTERN a_boolean
		targ_microsoft_bit_field_allocation;
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
		targ_plain_int_bit_field_is_unsigned;
			/* TRUE when a "plain" int bit field is to be treated
			   as unsigned.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_boolean
		targ_force_one_bit_bit_field_to_be_unsigned;
			/* TRUE when a "plain" int bit field of length 1 is
			   to be treated as unsigned regardless of the
			   setting of targ_plain_int_bit_field_is_unsigned
			   (because a bit field consisting of only a sign
			   is not very useful). */

EXTERN a_boolean
		targ_enum_bit_fields_are_always_unsigned;
			/* Signedness for enum bit fields (an extension in C):
			   if TRUE, enum bit fields are always unsigned.  If
			   FALSE, the rules are as described in target.h: it
			   depends on the signedness and size of the values of
			   the enum and defaults to the signedness indicated
			   by targ_nonnegative_enum_bit_field_is_unsigned.
			   This needs to be FALSE to allow fully-standard
			   C++. */

EXTERN a_boolean
		targ_nonnegative_enum_bit_field_is_unsigned;
			/* Signedness for enum bit fields whose enum types have
			   enumerators that could all fit in the nonnegative
			   range of the bit field if it were signed.  (Ignored
			   if targ_enum_bit_fields_are_always_unsigned is
			   TRUE.) */

EXTERN int	targ_zero_width_bit_field_alignment;
			/* Alignment adjustment to be made when a zero-width
			   (unnamed) bit field is declared.  If > 0 it is the
			   alignment to be used (typically the alignment of
			   one of the integral types, in which case the value
			   should be cast to a_targ_alignment).  A value of
			   zero means "use the minimal alignment", which is
			   single-byte alignment.  Any value less than zero
			   means "use the alignment of the base type given in
			   the declaration". */

EXTERN int	targ_zero_width_bit_field_affects_struct_alignment;
			/* TRUE when the alignment adjustment when a
			   zero-width (unnamed) bit-field is declared
			   affects the overall alignment of the struct as
			   well as the alignment of the next field. */

EXTERN int	targ_unnamed_bit_field_affects_struct_alignment;
			/* TRUE if the alignment adjustment when an unnamed
			   bit-field is declared affects the overall alignment
			   of the struct as well as the alignment of the next
			   field. */

EXTERN a_boolean
		targ_bit_field_affects_union_alignment;
			/* TRUE if a bit field in a union type affects its
			   alignment. */

EXTERN int	targ_user_control_of_struct_packing_affects_bit_fields;
			/* TRUE if "#pragma pack(n)" and the command-line
			   option "--pack_alignment=n" affect the alignment of
			   bit field containers (when bit fields straddle
			   container alignment boundaries). */

EXTERN int	targ_pad_bit_fields_larger_than_base_type;
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
		targ_sizeof_pointer;
			/* Size of a pointer.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_pointer;
			/* Alignment of a pointer.  Initialized to the default
			   value but reconfigurable. */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
#if NEAR_AND_FAR_ALLOWED
EXTERN a_targ_size_t
		targ_sizeof_far_pointer;
			/* Size of a far pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN a_targ_alignment
		targ_alignof_far_pointer;
			/* Alignment of a far pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
EXTERN a_targ_size_t
		targ_sizeof_near_pointer;
			/* Size of a near pointer.  Initialized to the default
			   value but reconfigurable.  Used only when support
			   for near and far is enabled (e.g., in 16-bit
			   Microsoft mode). */

EXTERN a_targ_alignment
		targ_alignof_near_pointer;
			/* Alignment of a near pointer.  Initialized to the
			   default value but reconfigurable.  Used only when
			   support for near and far is enabled (e.g., in
			   16-bit Microsoft mode). */
#endif /* NEAR_AND_FAR_ALLOWED */

EXTERN an_integer_kind
		targ_ptrdiff_t_int_kind;
			/* Representation for ptrdiff_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_size_t_max;
			/* The limit of the host representation of size_t
			   constants; the range it defines can be equal to
			   or smaller than the integer size implied by
			   TARG_SIZE_T_INT_KIND.  Initialized to the default
			   value but reconfigurable. */

EXTERN an_integer_kind
		targ_size_t_int_kind;
			/* Representation for size_t -- the integer kind
			   large enough to hold a pointer value.  Initialized
			   to the default value but reconfigurable. */

#if GNU_EXTENSIONS_ALLOWED
EXTERN an_integer_kind
		targ_ssize_t_int_kind;
			/* Representation for ssize_t (a POSIX type used to
			   count bytes in certain I/O functions). */
#endif /* GNU_EXTENSIONS_ALLOWED */

#if FIXED_POINT_ALLOWED
/*
Fixed-point types:
*/
EXTERN a_targ_size_t
	targ_sizeof_fixed_point[/*is_unsigned*/2]
	                       [(int)fpp_last]
	                       [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_SIZEOF_SIGNED_SHORT_ACCUM,
		        TARG_SIZEOF_SIGNED_SHORT_FRACT },
		      { TARG_SIZEOF_SIGNED_ACCUM,
		        TARG_SIZEOF_SIGNED_FRACT },
		      { TARG_SIZEOF_SIGNED_LONG_ACCUM,
		        TARG_SIZEOF_SIGNED_LONG_FRACT } },
		    { { TARG_SIZEOF_UNSIGNED_SHORT_ACCUM,
		        TARG_SIZEOF_UNSIGNED_SHORT_FRACT },
		      { TARG_SIZEOF_UNSIGNED_ACCUM,
		        TARG_SIZEOF_UNSIGNED_FRACT },
		      { TARG_SIZEOF_UNSIGNED_LONG_ACCUM,
		        TARG_SIZEOF_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                             ;

EXTERN a_targ_alignment
	targ_alignof_fixed_point[/*is_unsigned*/2]
	                        [(int)fpp_last]
	                        [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_ALIGNOF_SIGNED_SHORT_ACCUM,
		        TARG_ALIGNOF_SIGNED_SHORT_FRACT },
		      { TARG_ALIGNOF_SIGNED_ACCUM,
		        TARG_ALIGNOF_SIGNED_FRACT },
		      { TARG_ALIGNOF_SIGNED_LONG_ACCUM,
		        TARG_ALIGNOF_SIGNED_LONG_FRACT } },
		    { { TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_SHORT_FRACT },
		      { TARG_ALIGNOF_UNSIGNED_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_FRACT },
		      { TARG_ALIGNOF_UNSIGNED_LONG_ACCUM,
		        TARG_ALIGNOF_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                              ;

EXTERN a_targ_alignment
	targ_fractional_bits_for_fixed_point[/*is_unsigned*/2]
	                                    [(int)fpp_last]
	                                    [/*is_fract*/2]
#if VAR_INITIALIZERS
		= { { { TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT } },
		    { { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT },
		      { TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM,
		        TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT } } }
#endif /* VAR_INITIALIZERS */
		                                                          ;

#endif /* FIXED_POINT_ALLOWED */

/*
Float types:
*/
EXTERN a_targ_size_t
		targ_sizeof_float;
			/* Size of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_float;
			/* Alignment of a float.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_double;
			/* Size of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_double;
			/* Alignment of a double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_long_double;
			/* Size of a long double.  Initialized to the default
			   value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_long_double;
			/* Alignment of a long double.  Initialized to the
			   default value but reconfigurable. */

#if GNU_EXTENSIONS_ALLOWED

EXTERN a_type_mode_kind
		targ_word_mode;
			/* Mode of a word, i.e., the natural integer
			   size for the target.  Initialized to the
			   default value but reconfigurable. */

EXTERN a_type_mode_kind
		targ_unwind_word_mode;
			/* Mode of an "unwind word", i.e., the integer size
			   used in unwind descriptors for the target.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN a_type_mode_kind
		targ_libgcc_cmp_return_mode;
			/* Mode used by GNU's libgcc for compare instruction
			   results.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_type_mode_kind
		targ_libgcc_shift_count_mode;
			/* Mode used by GNU's libgcc for shift counts.
			   Initialized to the default value but
			   reconfigurable. */

#if TARG_ALL_POINTERS_SAME_SIZE

EXTERN a_type_mode_kind
		targ_pointer_mode;
			/* Mode of a pointer.  Initialized to the
			   default value but reconfigurable. */

#endif /* TARG_ALL_POINTERS_SAME_SIZE */

#endif /* GNU_EXTENSIONS_ALLOWED */

#if TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES

EXTERN a_targ_alignment
		targ_short_field_alignment;
			/* Default alignment for fields of type short. */

EXTERN a_targ_alignment
		targ_int_field_alignment;
			/* Default alignment for fields of type int. */

EXTERN a_targ_alignment
		targ_long_field_alignment;
			/* Default alignment for fields of type long. */

#if LONG_LONG_ALLOWED
EXTERN a_targ_alignment
		targ_long_long_field_alignment;
			/* Default alignment for fields of type long long. */
#endif /* LONG_LONG_ALLOWED */

#if INT128_EXTENSIONS_ALLOWED
EXTERN a_targ_alignment
		targ_int128_field_alignment;
			/* Default alignment for fields of the 128-bit integer
			   type. */
#endif /* INT128_EXTENSIONS_ALLOWED */

EXTERN a_targ_alignment
		targ_float_field_alignment;
			/* Default alignment for fields of type float. */

EXTERN a_targ_alignment
		targ_double_field_alignment;
			/* Default alignment for fields of type double. */

EXTERN a_targ_alignment
		targ_long_double_field_alignment;
			/* Default alignment for fields of type long double. */

#endif /* TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */

/*
C++ pointer-to-member type.
*/
EXTERN a_targ_size_t
		targ_sizeof_ptr_to_data_member;
			/* Size of a pointer-to-data-member.  Initialized to
			   the default value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_data_member;
			/* Alignment of a pointer-to-data-member.  Initialized
			   to the default value but reconfigurable. */

EXTERN a_targ_size_t
		targ_sizeof_ptr_to_member_function;
			/* Size of a ptr-to-member-function.  Initialized to
			   the default value but reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_member_function;
			/* Alignment of a pointer-to-member-function.
			   Initialized to the default value but
			   reconfigurable. */


/*
Virtual function info.
*/
EXTERN a_targ_size_t
		targ_sizeof_virtual_function_info;
			/* Size of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_virtual_function_info;
			/* Alignment of a virtual-function-info entity.
			   Initialized to the default value but
			   reconfigurable. */

#if !IA64_ABI
/*
Pointer to virtual base class.
*/
EXTERN a_targ_size_t
		targ_sizeof_ptr_to_virtual_base_class;
			/* Size of a "pointer-to-virtual-base-class" member.
			   Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_alignof_ptr_to_virtual_base_class;
			/* Alignment of a "pointer-to-virtual-base-class"
			   member.  Initialized to the default value but
			   reconfigurable. */
#endif /* !IA64_ABI */

/*
Miscellaneous
*/
EXTERN a_boolean
		targ_enum_types_can_be_smaller_than_int;
			/* When TRUE, enum types will be allocated in the
			   smallest integral type in which they will fit; if
			   FALSE, int will be used (e.g., for cfront
			   compatibility).  Initialized to the default value
			   but reconfigurable. */

EXTERN a_boolean
		targ_right_shift_is_arithmetic;
			/* When TRUE a right shift on a signed quantity does
			   sign extension.  Initialized to the default value
			   but reconfigurable. */

EXTERN a_boolean
		targ_too_large_shift_count_is_taken_modulo_size;
			/* When TRUE a shift with a too-large shift count is
			   treated as if the shift count is reduced modulo
			   the bit size of the object. */

EXTERN a_targ_alignment
		targ_minimum_struct_alignment;
			/* The minimum alignment required for objects of class,
			   struct, and union type in the target environment.
			   Initialized to the default value but
			   reconfigurable. */

#if USER_CONTROL_OF_STRUCT_PACKING
EXTERN a_targ_alignment
		targ_minimum_pack_alignment;
			/* The minimum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_maximum_pack_alignment;
			/* The maximum value which a "pack alignment" value
			   may have.  Initialized to the default value but
			   reconfigurable. */

EXTERN a_targ_alignment
		targ_maximum_intrinsic_alignment;
			/* The maximum alignment value which the target can
			   take advantage of.  Initialized to the default
			   value but reconfigurable. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

EXTERN a_boolean
		distinct_template_signatures;
			/* If TRUE, template functions are given mangled names
			   that are distinct from the names for nontemplate
			   functions. */

EXTERN a_boolean
		assume_references_cannot_be_null;
			/* If TRUE, C++ references are assumed never to have
			   NULL addresses in them.  That's as required by the
			   C++ standard, but some implementations allow
			   that. */

#if DO_IL_LOWERING

EXTERN a_boolean
		force_variable_definition_via_zeroing;
			/* If TRUE, add zeroing to variable definitions
			   to make them definitions in C. */

EXTERN a_boolean
		make_all_functions_unprototyped;
			/* If TRUE, all functions are rewritten to be
			   unprototyped. */

EXTERN a_boolean
		assume_this_cannot_be_null_in_conditional_operators;
			/* If TRUE, assume "this" cannot be null in conditional
			   operators, allowing some additional optimizations
			   (i.e., dead code removal) in "?", "&&", and "||"
			   operations. */

#if DO_FULL_PORTABLE_EH_LOWERING

EXTERN unsigned int
		targ_jmp_buf_num_elements;
			/* Number of elements in a jmp_buf array.  Initialized
			   to the default value but reconfigurable. */
EXTERN a_boolean
		targ_jmp_buf_elements_are_float;
			/* Choose between integer and float members of the
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN an_integer_kind
		targ_jmp_buf_element_int_kind;
			/* Integer kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */
EXTERN a_float_kind
		targ_jmp_buf_element_float_kind;
			/* Float kind indicating the kind of element in a
			   jmp_buf array.  Initialized to the default value
			   but reconfigurable. */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if GENERATE_EH_TABLES
EXTERN an_integer_kind
		targ_var_handle_int_kind;
			/* Integer kind to be used for a "handle" in
			   exception handling tables. */
#endif /* GENERATE_EH_TABLES */
#endif /* DO_IL_LOWERING */

EXTERN int	targ_flt_mant_dig;
			/* The number of bits in the mantissa of a float. */

EXTERN int	targ_flt_min_exp;
			/* The minimum exponent value of a float. */

EXTERN int	targ_flt_max_exp;
			/* The maximum exponent value of a float. */

EXTERN int	targ_dbl_mant_dig;
			/* The number of bits in the mantissa of a double. */

EXTERN int	targ_dbl_min_exp;
			/* The minimum exponent value of a double. */

EXTERN int	targ_dbl_max_exp;
			/* The maximum exponent value of a double. */

EXTERN int	targ_ldbl_mant_dig;
			/* The number of bits in the mantissa of a long
                           double. */

EXTERN int	targ_ldbl_min_exp;
			/* The minimum exponent value of a long double. */

EXTERN int	targ_ldbl_max_exp;
			/* The maximum exponent value of a long double. */

EXTERN a_boolean
		remove_qualifiers_from_param_types;
			/* True when type qualifiers should be removed from
			   function parameter types (e.g., a "const int"
			   parameter is seen simply as "int"). */

EXTERN a_boolean
		c_and_cpp_function_types_are_distinct;
			/* If TRUE, function types are considered distinct if
			   their only difference is that one has extern "C"
			   routine linkage and the other has extern "C++"
			   routine linkage.  This affects, among other things,
			   overload resolution and name mangling.  (See also
			   impl_conv_between_c_and_cpp_function_ptrs_allowed,
			   defined in cmd_line.h.) */

#if BACK_END_IS_CP_GEN_BE
EXTERN a_boolean
		old_specializations_for_generated_instances;
			/* If TRUE, specializations for generated template
			   instances in generated code (C++-generating back
			   end) should use the old syntax instead of the
			   modern "template <>" prefix form. */
#endif /* BACK_END_IS_CP_GEN_BE */

EXTERN a_boolean
		type_info_in_namespace_std;
			/* If TRUE,  class type_info is defined as a member
			   of namespace "std". */

EXTERN a_boolean
		pass_stdarg_references_to_generated_code;
			/* If TRUE, references to the macros in <stdarg.h>
			   are passed through to the output unchanged. */

EXTERN a_boolean
		va_list_in_std_namespace;
			/* If TRUE, the va_list type created when passing
			   stdarg references to generated code is placed in
			   the std namespace. */

EXTERN a_boolean
		va_list_using_using_decl_in_std_namespace;
			/* When va_list_in_std_namespace is FALSE, this is
			   TRUE if a using-declaration for va_list should
			   be created in the std namespace. */

EXTERN a_boolean
		va_arg_returns_lvalue;
			/* If TRUE, the va_arg operator implemented when
			   passing stdarg references to generated code returns
			   an lvalue.  The C and C++ standards allow but
			   do not require that va_arg produce an lvalue. */

EXTERN a_boolean
		instantiate_extern_inline;
			/* TRUE if the instantiation mechanism should be used
			   to control the definition of extern inline
			   functions. */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE

EXTERN a_boolean
		sun_is_generated_code_target;
			/* TRUE if code is being generated for a Sun
			   compiler. */

#ifdef SUN_TARGET_VERSION_NUMBER
EXTERN unsigned long
		sun_target_version_number;
			/* The version number of the Sun compiler being
			   targeted (e.g., 0x530 for version 5.3). */
#endif /* ifdef SUN_TARGET_VERSION_NUMBER */

EXTERN a_boolean
		gcc_is_generated_code_target;
			/* TRUE if code is being generated for the GNU C or
			   C++ compiler. */

#if GCC_IS_GENERATED_CODE_TARGET || \
    (BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT)

EXTERN unsigned long
		gnu_target_version_number;
			/* The version number of the GNU compiler being
			   targeted (e.g., 30401 for GNU C/C++ 3.4.1). */

#endif /* GCC_IS_GENERATED_CODE_TARGET || ... */

EXTERN a_boolean
		gcc_builtin_varargs_in_generated_code;
			/* TRUE if the generated code should use vararg
			   primitives predefined by GNU compilers. */

EXTERN a_boolean
		msvc_is_generated_code_target;
			/* TRUE if code is being generated for the Microsoft
			   MSVC++ compiler. */

EXTERN int
		msvc_target_version_number;
			/* The version number (i.e., 1300 for 7.0) of the
			   Microsoft MSVC compiler being targeted. */

EXTERN int
		microsoft_dialect_is_generated_code_target;
			/* TRUE if code is being generated for a compiler
			   accepting Microsoft extensions. */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

EXTERN a_boolean
		always_fold_calls_to_builtin_constant_p;
			/* TRUE if calls to __builtin_constant_p should always
			   be folded in the front end. */

#if BACK_END_IS_CP_GEN_BE

EXTERN a_boolean
		cp_gen_be_target_matches_source_dialect;
			/* Flag that indicates that the C++-generating back
			   end should assume the target dialect is the same
			   as the source dialect. */

#endif /* BACK_END_IS_CP_GEN_BE */

EXTERN an_integer_kind
		plain_char_int_kind;
			/* Integer kind for a "plain" char, dependent on
			   the setting of targ_has_signed_chars. */

EXTERN a_boolean
		string_literals_shared;
			/* TRUE if string literals can be shared.  FALSE
			   if string literals are not shared because they
			   might be writable (as in pcc mode). */

#if !IA64_ABI
EXTERN an_integer_kind
		targ_runtime_elem_count_int_kind;
			/* Type used for number_of_elements arguments
			   in the cfront ABI. */
#endif /* !IA64_ABI */

EXTERN a_boolean
		warn_on_try_statement;
			/* When TRUE (and in Microsoft emulation mode),
			   a (one time) warning is issued when a try statement
			   is encountered. */

#if BACK_END_IS_C_GEN_BE
EXTERN a_boolean
		use_empty_struct_in_generated_c;
			/* When TRUE, the C-generating back end will use an
			   empty struct as the representation of an empty
			   class.  Otherwise, the generated struct will have
			   a one-byte padding field. */
#endif /* BACK_END_IS_C_GEN_BE */

#ifndef DO_NOT_UNDEF_TARGET_MACROS
/* Aside from occasional references in targ_def.h, the following values
   should be used *only* to initialize the variables declared in this file.
   To enforce this convention, they are undefined at this time.  (This is
   not foolproof, but it should catch most such misuses).  The macro
   DO_NOT_UNDEF_TARGET_MACROS is defined by target.c so that variables
   declared in this file may be initialized there. */
#undef TARG_LITTLE_ENDIAN
#undef TARG_CHAR_BIT
#undef TARG_HOST_STRING_CHAR_BIT
#undef TARG_HAS_SIGNED_CHARS
#undef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#undef TARG_WCHAR_T_INT_KIND
#undef TARG_WINT_T_INT_KIND
#undef TARG_CHAR16_T_INT_KIND
#undef TARG_CHAR32_T_INT_KIND
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
#if INT128_EXTENSIONS_ALLOWED
#undef TARG_SIZEOF_INT128
#undef TARG_ALIGNOF_INT128
#endif /* INT128_EXTENSIONS_ALLOWED */
#undef TARG_MAX_CLASS_OBJECT_SIZE
#undef TARG_MAX_BASE_CLASS_OFFSET
#undef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#undef TARG_BIT_FIELD_CONTAINER_SIZE
#undef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#undef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#undef TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED
#undef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#undef TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED
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
#undef TARG_PTRDIFF_T_INT_KIND
#undef TARG_SIZE_T_MAX
#undef TARG_SIZE_T_INT_KIND
#undef TARG_SSIZE_T_INT_KIND
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
#undef ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS
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
#undef MSVC_TARGET_VERSION_NUMBER
#if !IA64_ABI
#undef TARG_RUNTIME_ELEM_COUNT_INT_KIND
#endif /* !IA64_ABI */
#if BACK_END_IS_C_GEN_BE
#undef USE_EMPTY_STRUCT_IN_GENERATED_C
#endif /* BACK_END_IS_C_GEN_BE */
/* MAKE_TARG_NAMES_REFER_TO_VARIABLES cannot be set when this file is included
   by target.c.  If it was previously defined, undefine it and set it to the
   value required by target.c. */
#ifdef MAKE_TARG_NAMES_REFER_TO_VARIABLES
#undef  MAKE_TARG_NAMES_REFER_TO_VARIABLES
#define MAKE_TARG_NAMES_REFER_TO_VARIABLES 0
#endif /* ifdef MAKE_TARG_NAMES_REFER_TO_VARIABLES */
#endif /* ifndef DO_NOT_UNDEF_TARGET_MACROS */

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
#define TARG_WINT_T_INT_KIND targ_wint_t_int_kind
#define TARG_CHAR16_T_INT_KIND targ_char16_t_int_kind
#define TARG_CHAR32_T_INT_KIND targ_char32_t_int_kind
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
#if INT128_EXTENSIONS_ALLOWED
#define TARG_SIZEOF_INT128 targ_sizeof_int128
#define TARG_ALIGNOF_INT128 targ_alignof_128
#endif /* INT128_EXTENSIONS_ALLOWED */
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
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED                     \
                        targ_nonnegative_enum_bit_field_is_unsigned
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
#define TARG_PTRDIFF_T_INT_KIND targ_ptrdiff_t_int_kind
#define TARG_SIZE_T_MAX targ_size_t_max
#define TARG_SIZE_T_INT_KIND targ_size_t_int_kind
#define TARG_SSIZE_T_INT_KIND targ_ssize_t_int_kind
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
#define ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS             \
                        assume_this_cannot_be_null_in_conditional_operators
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
#define MSVC_TARGET_VERSION_NUMBER msvc_target_version_number
#if !IA64_ABI
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND targ_runtime_elem_count_int_kind
#endif /* !IA64_ABI */
#if BACK_END_IS_C_GEN_BE
#define USE_EMPTY_STRUCT_IN_GENERATED_C use_empty_struct_in_generated_c
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* MAKE_TARG_NAMES_REFER_TO_VARIABLES */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void init_microsoft_sized_int_types(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_targ_size_t size_of_pointer_to(a_type_ptr        type_pointed_to,
                                 a_targ_alignment  *alignment);

#if CHECKING
extern void check_target_configuration(void);
#endif /* CHECKING */

#if BACK_END_IS_CP_GEN_BE
extern void select_cp_gen_be_target_dialect(void);
#endif /* BACK_END_IS_CP_GEN_BE */

extern void target_init(void);

extern void target_early_init(void);

extern void target_one_time_init(void);

#endif /* ifndef TARGET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
