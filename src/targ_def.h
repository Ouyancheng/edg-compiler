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

targ_def.h -- Definition of target machine characteristics.  See <limits.h>
              and standard, sec. 2.2.4.2, for related information.

Note that targ_def.h contains configuration values that are incorporated
when the compiler is built (#define values, typedefs), whereas target.h
declares configuration variables that, in principle, can be reset whenever
the compiler is invoked.

*/

/* Avoid including these declarations more than once: */
#ifndef TARG_DEF_H
#define TARG_DEF_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* !defined(HOST_ENVIR_H) */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* !defined(LANG_FEAT_H) */

/*
Flag that is TRUE if object code compatibility with USL's cfront is
required.  The main issue is class layout and specifically how the data
sections for virtual base classes are put out.  Other issues include
when virtual tables are generated.  The default behavior
(when this flag is FALSE) produces a more efficient use of space.
Some features of cfront changed from release 2.1 to release 3.0.  For example,
release 2.1 provided a special feature to ease the transition between
non-nested classes and nested classes.  This feature was removed for release
3.0.  Either the 2.1 or 3.0 flag should be set to designate the variety
of cfront compatibility that is desired.  When testing these flags for
behavior that did not change between 2.1 and 3.0 the general flag should
be used.
*/
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY TRUE
#endif /* !defined(CFRONT_2_1_OBJECT_CODE_COMPATIBILITY) */
#ifndef CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
#define CFRONT_3_0_OBJECT_CODE_COMPATIBILITY FALSE
#endif /* !defined(CFRONT_3_0_OBJECT_CODE_COMPATIBILITY) */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY||CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
#define CFRONT_OBJECT_CODE_COMPATIBILITY TRUE
#else /* !(CFRONT_2_1_...) */
#define CFRONT_OBJECT_CODE_COMPATIBILITY FALSE
#endif /* CFRONT_2_1_... */

#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
 #error -- Must select either 2.1 compatibility or 3.0 compatibility.
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY ... */

/* The code that implements the cfront name lookup bug makes use of the
   information recorded for semivisible nested type handling.  Consequently,
   2.1 compatibility mode is required to use the name lookup bug. */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
 #error -- cfront name lookup bug support requires cfront 2.1 compatibility
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/*
Target byte order.  Little-endian means the least-significant part of a
multi-byte integer is at the lowest memory address.
*/
#ifndef TARG_LITTLE_ENDIAN
#define TARG_LITTLE_ENDIAN FALSE
			/* Default value, used to initialize global variable
			   targ_little_endian. */
#endif /* !defined(TARG_LITTLE_ENDIAN) */

/*
Char types:
*/
#ifndef TARG_CHAR_BIT
#define TARG_CHAR_BIT 8
			/* Number of bits in a target char.  Default value,
			   used to initialize global variable targ_char_bit. */
#endif /* !defined(TARG_CHAR_BIT) */

/* TARG_HOST_STRING_CHAR_BIT is the number of data bits per character used
   when representing target characters as a string on the host.  One is
   allowed to make the target char larger than the host char, but individual
   characters in string literals will be limited by what is representable in
   a host char. */
#if TARG_CHAR_BIT > CHAR_BIT
#define TARG_HOST_STRING_CHAR_BIT CHAR_BIT
#else /* TARG_CHAR_BIT <= CHAR_BIT */
#define TARG_HOST_STRING_CHAR_BIT TARG_CHAR_BIT
#endif /* TARG_CHAR_BIT > CHAR_BIT */
			/* Default value, used to initialize global variable
			   targ_host_string_char_bit. */

/* Make the default for character signedness on the target the same as
   for the host.  That's not required; it's just the most common case,
   and doing it this way makes it less likely that this configuration
   will be done wrong. */
#ifndef TARG_HAS_SIGNED_CHARS
#if CHAR_MIN == 0
#define TARG_HAS_SIGNED_CHARS FALSE
#else /* CHAR_MIN != 0 */
#define TARG_HAS_SIGNED_CHARS TRUE
#endif /* CHAR_MIN == 0 */
			/* Default value, used to initialize global variable
			   targ_has_signed_chars. */
#endif /* !defined(TARG_HAS_SIGNED_CHARS) */

/*
Special characters:
*/
#define TARG_ALERT_CHAR       '\007'
#define TARG_BACKSPACE_CHAR   '\b'
#define TARG_FORM_FEED_CHAR   '\f'
#define TARG_NEWLINE_CHAR     '\n'
#define TARG_CARR_RETURN_CHAR '\r'
#define TARG_HORIZ_TAB_CHAR   '\t'
#define TARG_VERT_TAB_CHAR    '\013'

/*
Ordering of bytes in char constants:
*/
#ifndef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
			/* if 1, 'ab' == 0x6162. */
			/* if 0, 'ab' == 0x6261. */
			/* Default value, used to initialize global variable
			   targ_char_constant_first_char_most_significant. */
#endif /* !defined(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT) */

/*
Integer types:
*/
/* Remember that the size of a type must be a multiple of the alignment. */
#ifndef TARG_SIZEOF_SHORT
#define TARG_SIZEOF_SHORT 2
			/* Default value, used to initialize global variable
			   targ_sizeof_short. */
#endif /* !defined(TARG_SIZEOF_SHORT) */
#ifndef TARG_ALIGNOF_SHORT
#define TARG_ALIGNOF_SHORT 2
			/* Default value, used to initialize global variable
			   targ_alignof_short. */
#endif /* !defined(TARG_ALIGNOF_SHORT) */
#ifndef TARG_SIZEOF_INT
#define TARG_SIZEOF_INT 4
			/* Default value, used to initialize global variable
			   targ_sizeof_int. */
#endif /* !defined(TARG_SIZEOF_INT) */
#ifndef TARG_ALIGNOF_INT
#define TARG_ALIGNOF_INT 4
			/* Default value, used to initialize global variable
			   targ_alignof_int. */
#endif /* !defined(TARG_ALIGNOF_INT) */
#ifndef TARG_SIZEOF_LONG
#define TARG_SIZEOF_LONG 4
			/* Default value, used to initialize global variable
			   targ_sizeof_long. */
#endif /* !defined(TARG_SIZEOF_LONG) */
#ifndef TARG_ALIGNOF_LONG
#define TARG_ALIGNOF_LONG 4
			/* Default value, used to initialize global variable
			   targ_alignof_long. */
#endif /* !defined(TARG_ALIGNOF_LONG) */
#if LONG_LONG_ALLOWED
#ifndef TARG_SIZEOF_LONG_LONG
#define TARG_SIZEOF_LONG_LONG 8
			/* Default value, used to initialize global variable
			   targ_sizeof_long_long. */
#endif /* !defined(TARG_SIZEOF_LONG_LONG) */
#ifndef TARG_ALIGNOF_LONG_LONG
#define TARG_ALIGNOF_LONG_LONG 8
			/* Default value, used to initialize global variable
			   targ_alignof_long_long. */
#endif /* !defined(TARG_ALIGNOF_LONG_LONG) */
#endif /* LONG_LONG_ALLOWED */

/* Specify the size of the largest integer.  Note that this will constrain
   how targ_sizeof_long and targ_sizeof_long_long are configured at runtime,
   because TARG_SIZEOF_LARGEST_INTEGER is required to be a compile-time
   constant and so cannot be adjusted at run time the way some other target
   configuration values are.  Therefore, it should be set to the largest
   value a "long int" (or a "long long int") is allowed to have. */
#ifndef TARG_SIZEOF_LARGEST_INTEGER
/* By default, a minimum largest value is supplied, and it is expected to be
   one of 1, 4, 8, or 16.  This can be changed either here or in defines.h,
   if required.  However, it is necessary to use a literal instead of the
   more obvious named value, since TARG_SIZEOF_LARGEST_INTEGER is used later
   in defining other macros, but TARG_SIZEOF_LONG, etc., do not persist once
   the variables they correspond to have been defined. */
#if LONG_LONG_ALLOWED
/* A "long long int" is the largest integer. */
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG_LONG
#else /* !LONG_LONG_ALLOWED */
/* A "long int" is the largest integer. */
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG
#endif /* !LONG_LONG_ALLOWED */
#if TARG_SIZEOF_LARGEST_INTEGER == 4
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 4
#else /* TARG_SIZEOF_LARGEST_INTEGER != 4 */
#if TARG_SIZEOF_LARGEST_INTEGER == 8
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 8
#else /* TARG_SIZEOF_LARGEST_INTEGER != 8 */
#if TARG_SIZEOF_LARGEST_INTEGER == 16
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 16
#else /* TARG_SIZEOF_LARGEST_INTEGER != 16 */
#if TARG_SIZEOF_LARGEST_INTEGER == 1
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 1
#else /* TARG_SIZEOF_LARGEST_INTEGER != 1 */
 #error -- do not know how to set TARG_SIZEOF_LARGEST_INTEGER
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 1 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 16 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 8 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 4 */
#endif /* !defined(TARG_SIZEOF_LARGEST_INTEGER) */
/* Check the value.  It must be large enough to accommodate the largest
   integer we know about at this point.  An additional check is made after
   command line processing in case the variables corresponding to
   targ_sizeof_long and targ_sizeof_long_long get different values. */
#if LONG_LONG_ALLOWED
#if TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG_LONG
 #error -- TARG_SIZEOF_LARGEST_INTEGER too small for TARG_SIZEOF_LONG_LONG
#endif /* TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG_LONG */
#else /* !LONG_LONG_ALLOWED */
#if TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG
 #error -- TARG_SIZEOF_LARGEST_INTEGER too small for TARG_SIZEOF_LONG
#endif /* TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG */
#endif /* LONG_LONG_ALLOWED */

/*
Type used as the representation of an integer value.  More precisely,
this is the form used on the host to represent a target integer.
*/
/*
At the first level, one must choose between a representation using
a single value of some host integer type and one using an array of
smaller host integers.  The latter form is necessary when the front
end is used as part of a cross-compiler where the target has larger
integers than the host.
*/
#ifndef INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER TRUE
#endif /* !defined(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER) */

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/*
There is a host integer type that is large enough to hold all target integers,
so the integer representation is just some host integral type.
This type must be unsigned; a_signed_integer_value is the signed version.
Note that the types are allowed to be the unsigned and signed versions
of "long long" if the host allows them.  If "long long" is used, check the
setting of AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG.
*/
typedef unsigned long an_integer_value;
typedef long a_signed_integer_value;

/*
If this flag is TRUE an_integer_value is larger than a host long.
This is usually FALSE when an integer value is represented as a host
integer, but would be TRUE if an_integer_value is represented using a
host long long.
*/
#ifndef AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG
#define AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG	FALSE
#endif /* !defined(AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG) */

/* Minimum and maximum values that can be represented in an_integer_value. */
#define MAX_INTEGER_VALUE LONG_MAX
#define MIN_INTEGER_VALUE LONG_MIN
#define MAX_UNSIGNED_INTEGER_VALUE ULONG_MAX
#define BITS_IN_AN_INTEGER_VALUE (sizeof(an_integer_value) * CHAR_BIT)
/* The printf formatting specifier to be used to print the integer type. */
#define PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE   "%ld"  /* long */
#define PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE "%lu"  /* unsigned long */
#define PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE      "%lx"  /* hexadecimal */

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
There is no host integer that is large enough, so use an array to represent
the target integers.
*/
/* Type of the elements of the array.  These must be at least half the
   size of a_host_large_integer (some large and efficient integer type on
   the host), and (for space reasons) preferably exactly half.
   Typically, this is a 16-bit value.  The bit size and minimum and
   maximum values indicate the range of values to be used, which may
   be smaller than the range actually available. */
typedef unsigned short an_int_value_part;
#define MAX_UINT_VALUE_PART 0xffff
#define MAX_INT_VALUE_PART 0x7fff
#define MIN_INT_VALUE_PART (-0x8000)
#define SIGN_BIT_INT_VALUE_PART 0x8000
#define SIZEOF_INT_VALUE_PART (sizeof(an_int_value_part)) /* Okay to change. */
#define BITS_IN_INT_VALUE_PART (SIZEOF_INT_VALUE_PART*CHAR_BIT)
/* Large and efficient host integer, at least twice the size of
   an_int_value_part, used in doing computations on integer values.
   The idea is that any operation involving two an_int_value_part
   values in the range MIN_INT_VALUE_PART..MAX_INT_VALUE_PART can
   be done in a_host_large_integer without special coding to deal
   with overflows. */
typedef long a_host_large_integer;
typedef unsigned long a_host_large_unsigned;
#define MAX_HOST_LARGE_INTEGER LONG_MAX
#define MIN_HOST_LARGE_INTEGER LONG_MIN
#define MAX_HOST_LARGE_UNSIGNED LONG_UMAX
#define BITS_IN_HOST_LARGE_INTEGER (sizeof(a_host_large_integer)*CHAR_BIT)
/* The array is made up of elements of type an_int_value_part.
   Figure out how many. */
#define INT_VALUE_PARTS_PER_INTEGER_VALUE                             \
  (TARG_SIZEOF_LARGEST_INTEGER/SIZEOF_INT_VALUE_PART)
/* This is an array inside a struct instead of just an array so that
   its address behaves in a predictable way. */
typedef struct an_integer_value {
  an_int_value_part part[INT_VALUE_PARTS_PER_INTEGER_VALUE];
} an_integer_value;
#define BITS_IN_AN_INTEGER_VALUE (BITS_IN_INT_VALUE_PART *	      \
				  INT_VALUE_PARTS_PER_INTEGER_VALUE)

/*
If this flag is TRUE an_integer_value is larger than a host long.
This is true when simulated integers are being used.
*/
#ifndef AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG
#define AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG	TRUE
#endif /* !defined(AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG) */

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
If this flag is TRUE, overflows on signed integer operations do
not cause errors (only warnings).  Usually this would be set to
match the target machine behavior on integer operations in C.
*/
#ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define TARG_NO_ERROR_ON_INTEGER_OVERFLOW TRUE
#endif /* ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW */

/*
If this flag is TRUE, integer types with the same representation
(same size, alignment, and signedness) are considered to be
identical in the IL.  This requires back end support, i.e., the back
end must be comfortable with the fact that these types will be used
interchangeably without casts between them.  In pcc mode, such types
will be considered identical, which will typically make "int" and
"long" interchangeable, and likewise "unsigned int" and "unsigned
long".  ANSI mode IL is affected in that casts between such types
will not be generated.  However, the language accepted in ANSI mode
is not affected; such types are not considered to be identical, and
errors are still generated for type mismatches.
*/
#ifndef SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL FALSE
#else /* !BACK_END_IS_CP_GEN_BE */
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL TRUE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* !defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL) */

/* Maximum size of a bit-field.  Must not be larger than the size of a
   long (or a long long, if they are allowed). */
#ifndef TARG_MAX_BIT_FIELD_SIZE
#define TARG_MAX_BIT_FIELD_SIZE (TARG_SIZEOF_INT*TARG_CHAR_BIT)
			/* Default value, used to initialize global variable
			   targ_max_bit_field_size. */
#endif /* ifndef TARG_MAX_BIT_FIELD_SIZE */

/* Check the value: */
#if TARG_MAX_BIT_FIELD_SIZE > (TARG_SIZEOF_LARGEST_INTEGER*TARG_CHAR_BIT)
 #error -- TARG_MAX_BIT_FIELD_SIZE is too big
#endif /* TARG_MAX_BIT_FIELD_SIZE ... */
/* Bit field size is represented as a byte (see a_field in il_def.h). */
#if BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE
 #error -- TARG_MAX_BIT_FIELD_SIZE is too big.
#endif /* BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE */

/* Container size to be used for bit-fields.  If > 0, indicates the
   size in bytes of one of the integral types.  0 means "use the smallest
   integral type into which the field will fit".  < 0 means "use the
   base type given in the declaration". */
#ifndef TARG_BIT_FIELD_CONTAINER_SIZE
#define TARG_BIT_FIELD_CONTAINER_SIZE 0
			/* Default value, used to initialize global variable
			   targ_bit_field_container_size. */
#endif /* ifndef TARG_BIT_FIELD_CONTAINER_SIZE */

/* How plain "int" bit fields are to be treated (signed or unsigned).  Note
   that 1-bit fields are made unsigned regardless of this switch. This flag
   also controls how plain "short", "long", and "long long" are treated as
   bit field types. */
#ifndef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED (!TARG_HAS_SIGNED_CHARS)
			/* Default value, used to initialize global variable
			   targ_plain_int_bit_field_is_unsigned. */
#endif /* ifndef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED */

/* Signedness for enum bit fields (an extension): if TRUE, enum bit fields
   are always unsigned.  If FALSE, the rules are: (a) if the enum contains
   any negative values, the field is signed; (b) if the enum contains
   values large enough that they won't fit if one bit is allocated for a
   sign, the field is unsigned; otherwise (c) the signedness is as
   indicated by TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED. */
#ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED TRUE
			/* Default value, used to initialize global variable
			   targ_enum_bit_fields_are_always_unsigned. */
#endif /* ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */

/* Alignment adjustment to be made when a zero-width (unnamed) bit field is
   declared.  If > 0 it is the alignment to be used (typically the alignment
   of one of the integral types).  A value of zero means "use the minimal
   alignment", which is single-byte alignment.  Any value less than zero
   means "use the alignment of the base type given in the declaration". */
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT TARG_ALIGNOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 0
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
			/* Default value, used to initialize global variable
			   targ_zero_width_bit_field_alignment. */
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */

/*
Wide character constant type (wchar_t, see stddef.h and stdlib.h).
*/

#ifndef TARG_WCHAR_T_INT_KIND
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_short)
			/* Default value, used to initialize global variable
			   targ_wchar_t_int_kind. */
#endif /* !defined(TARG_WCHAR_T_INT_KIND) */
#ifndef TARG_SIZEOF_WCHAR_T
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_SHORT
			/* Default value, used to initialize global variable
			   targ_sizeof_wchar_t. */
#endif /* !defined(TARG_SIZEOF_WCHAR_T) */

/*
Pointer types:
*/
#define TARG_ALL_POINTERS_SAME_SIZE TRUE
			/* At the moment, this cannot be set FALSE.
			   See set_type_size in types.c. */
#ifndef TARG_SIZEOF_POINTER
#define TARG_SIZEOF_POINTER 4
			/* Default value, used to initialize global variable
			   targ_sizeof_pointer. */
#endif /* !defined(TARG_SIZEOF_POINTER) */
#ifndef TARG_ALIGNOF_POINTER
#define TARG_ALIGNOF_POINTER 4
			/* Default value, used to initialize global variable
			   targ_alignof_pointer. */
#endif /* !defined(TARG_ALIGNOF_POINTER) */

/* Indication of whether NULL pointer is like integer zero. */
#ifndef TARG_NULL_IS_ALL_BITS_ZERO
#define TARG_NULL_IS_ALL_BITS_ZERO TRUE
#endif /* ifndef TARG_NULL_IS_ALL_BITS_ZERO */

/* Integer type for the difference of two pointer types (ptrdiff_t).
   This type must be signed.  See 3.3.6 in the standard and the header
   file <stddef.h>. */
typedef long a_targ_ptrdiff_t;  /* Must be host "long". */

/* TARG_PTRDIFF_T_MAX and TARG_PTRDIFF_T_MIN define the limits of the host
   representation of ptrdiff_t constants; the range they define can be equal
   to or smaller than the integer size implied by TARG_PTRDIFF_T_INT_KIND.
   Except when the target ptrdiff_t is smaller than the host long, they
   should be LONG_MAX and LONG_MIN. */
#ifndef TARG_PTRDIFF_T_MAX
#define TARG_PTRDIFF_T_MAX ((a_targ_ptrdiff_t)LONG_MAX)
			/* Default value, used to initialize global variable
			   targ_ptrdiff_t_max. */
#endif /* !defined(TARG_PTRDIFF_T_MAX) */
#ifndef TARG_PTRDIFF_T_MIN
#define TARG_PTRDIFF_T_MIN ((a_targ_ptrdiff_t)LONG_MIN)
			/* Default value, used to initialize global variable
			   targ_ptrdiff_t_min. */
#endif /* !defined(TARG_PTRDIFF_T_MIN) */

/* Pick a typical representation for ptrdiff_t: the smaller of int or long
   that can hold a pointer value. */
#ifndef TARG_PTRDIFF_T_INT_KIND
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
			/* Default value, used to initialize global variable
			   targ_ptrdiff_t_int_kind. */
#endif /* ifndef TARG_PTRDIFF_T_INT_KIND */

/* size_t, used for size of arrays, offsets in fields, type of sizeof, etc.
   This type must be unsigned.  See 3.3.3.4 in the standard and the header
   file <stddef.h>. */
typedef unsigned long a_targ_size_t;  /* Must be host "unsigned long". */

/* TARG_SIZE_T_MAX defines the limit of the host representation
   of size_t constants; the range it defines can be equal to or smaller
   than the integer size implied by TARG_SIZE_T_INT_KIND.  Except when
   the target size_t is smaller than the host long, it should be ULONG_MAX. */
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
			/* Default value, used to initialize global variable
			   targ_size_t_max. */
#endif /* ifndef TARG_SIZE_T_MAX */

#ifndef TARG_SIZE_T_INT_KIND
/* Pick a typical representation for size_t: the smaller of unsigned int or
   unsigned long that can hold a pointer value. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
			/* Default value, used to initialize global variable
			   targ_size_t_int_kind. */
#endif /* ifndef TARG_SIZE_T_INT_KIND */

/* Specification of a target alignment requirement.  1 means no alignment
   requirement. */
typedef a_byte a_targ_alignment;

/*
Float types:
*/
/* Remember that the size of a type must be a multiple of the alignment. */
#ifndef TARG_SIZEOF_FLOAT
#define TARG_SIZEOF_FLOAT 4
			/* Default value, used to initialize global variable
			   targ_sizeof_float. */
#endif /* !defined(TARG_SIZEOF_FLOAT) */
#ifndef TARG_ALIGNOF_FLOAT
#define TARG_ALIGNOF_FLOAT 4
			/* Default value, used to initialize global variable
			   targ_alignof_float. */
#endif /* !defined(TARG_ALIGNOF_FLOAT) */
#ifndef TARG_SIZEOF_DOUBLE
#define TARG_SIZEOF_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_sizeof_double. */
#endif /* !defined(TARG_SIZEOF_DOUBLE) */
#ifndef TARG_ALIGNOF_DOUBLE
#define TARG_ALIGNOF_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_alignof_double. */
#endif /* !defined(TARG_ALIGNOF_DOUBLE) */
#ifndef TARG_SIZEOF_LONG_DOUBLE
#define TARG_SIZEOF_LONG_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_sizeof_long_double. */
#endif /* !defined(TARG_SIZEOF_LONG_DOUBLE) */
#ifndef TARG_ALIGNOF_LONG_DOUBLE
#define TARG_ALIGNOF_LONG_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_alignof_long_double. */
#endif /* !defined(TARG_ALIGNOF_LONG_DOUBLE) */

/*
Type used to represent float quantities internally:

(This should not be an array, so that it's always known whether one gets the
address or the value of the item.)
*/
typedef struct an_internal_float_value *an_internal_float_value_ptr;
typedef struct an_internal_float_value {
  /* The type here must match the code in float_pt.c.  The default
     declaration assumes that target floating constants are represented in
     a host double, which is what the default float_pt.c does. */
  a_byte bytes[sizeof(double)];
} an_internal_float_value;

/*
C++ pointer-to-member type.
(The formulas here are for a typical implementation, but are not required.)
*/
#ifndef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_SHORT
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_data_member. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_SHORT
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_data_member. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
           (2*TARG_SIZEOF_SHORT+TARG_SIZEOF_POINTER)
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_member_function. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#ifndef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_member_function. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */

/* 
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions the front end allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by the following.
*/
#ifndef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_virtual_function_info. */
#endif /* !defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
#ifndef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_virtual_function_info. */
#endif /* !defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */

/* 
Numbering for virtual functions.  Each virtual member function in a given
class is assigned a unique number which can (for instance) be used to
define a virtual function table index value.
*/
typedef unsigned short a_virtual_function_number;
#ifndef MAX_VIRTUAL_FUNCTIONS_PER_CLASS
#define MAX_VIRTUAL_FUNCTIONS_PER_CLASS USHRT_MAX
#endif /* ifndef MAX_VIRTUAL_FUNCTIONS_PER_CLASS */

/*
Control over whether or not C++ "new" and "delete" operations are allowed
to be folded into the constructor or destructor if possible.
*/
/* IL lowering requires that the delete be folded into the destructor.
   Otherwise the size is not available for the two-argument delete case.
   There is a consistency check in lower_il.c */
#ifndef NEW_CAN_BE_FOLDED_INTO_CTOR
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define NEW_CAN_BE_FOLDED_INTO_CTOR TRUE  /* cfront compatibility setting. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define NEW_CAN_BE_FOLDED_INTO_CTOR TRUE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(NEW_CAN_BE_FOLDED_INTO_CTOR) */
#ifndef DELETE_CAN_BE_FOLDED_INTO_DTOR
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define DELETE_CAN_BE_FOLDED_INTO_DTOR TRUE /* cfront compatibility setting. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define DELETE_CAN_BE_FOLDED_INTO_DTOR TRUE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(DELETE_CAN_BE_FOLDED_INTO_DTOR) */
/* If assignment to "this" is allowed, the folding must be done. */
#if ASSIGNMENT_TO_THIS_ALLOWED && !NEW_CAN_BE_FOLDED_INTO_CTOR
 #error -- NEW_CAN_BE_FOLDED_INTO_CTOR set wrong.
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */
#if ASSIGNMENT_TO_THIS_ALLOWED && !DELETE_CAN_BE_FOLDED_INTO_DTOR
 #error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */

/*
Control over whether or not C++ "new" and "delete" operations for an array
whose elements are classes with a constructor or destructor can be folded
into the runtime routine to process those.
*/
#ifndef NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE \
  TRUE  /* cfront compatibility setting. */
#else /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#define NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE \
  TRUE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_...) */
/* This must be TRUE for IL lowering.  There's a consistency check there. */

/*
Enumerated types:  Default setting for enum_types_can_be_smaller_than_int.
If TRUE, enumerated types can be allocated in integral types smaller than int.
*/
#ifndef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE /* cfront compat. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
			/* Default value, used to initialize global variable
			   targ_enum_types_can_be_smaller_than_int. */
#endif /* !defined(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT) */

/*
Definition of shift operations:
*/
#ifndef TARG_RIGHT_SHIFT_IS_ARITHMETIC
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
			/* Right shift on a signed quantity does sign
			   extension.  Default value, used to initialize
			   global variable targ_right_shift_is_arithmetic. */
#endif /* !defined(TARG_RIGHT_SHIFT_IS_ARITHMETIC) */

/*
Number of significant characters in an external name (names will be truncated
to this length if necessary).  If there is no limit, this value should be set
to zero.  It should in any case be set to a reasonably small value (not, for
instance, to the maximum integer size) since an array of this many
characters may be allocated (see find_external_symbol in symbol_tbl.c).
*/
#ifndef TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME
#define TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME 0
#endif /* ifndef TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME */

/*
Flag that is TRUE if external names are case sensitive (in which case, e.g.,
the object language would distinguish routines XXX and xxx).
*/
#ifndef TARG_CASE_SENSITIVE_EXTERNAL_NAMES
#define TARG_CASE_SENSITIVE_EXTERNAL_NAMES TRUE
#endif /* ifndef TARG_CASE_SENSITIVE_EXTERNAL_NAMES */

/*
Flag that is TRUE if external names begin with an added underscore.
This is used by some utility programs (e.g., edg_munch) that deal with
names.  The front end doesn't add the underscore.
*/
#ifndef TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED TRUE
#endif /* !defined(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED) */

/*
Flag that is TRUE if class and struct fields are allocated in the same order
as they are declared, regardless of access specification.  When it is FALSE,
fields are grouped by access (private first, followed by protected and then
public) and offsets are assigned within each group in declaration order.
(Constraints on allocation are discussed in ARM 9.2 and 11.1.  In particular,
both approaches described in the embedded annotation in section 11.1 are
supported.)
*/
#ifndef TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE TRUE
#endif /* ifndef TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */

/*
The minimum alignment required for class/struct/union objects in the target
environment.  If C code is being generated, this may be dictated by the
characteristics of the C compiler that will be used for subsequent
processing.
*/
#ifndef TARG_MINIMUM_STRUCT_ALIGNMENT
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
			/* Default value, used to initialize global variable
			   targ_minimum_struct_alignment. */
#endif /* !defined(TARG_MINIMUM_STRUCT_ALIGNMENT) */

#if USER_CONTROL_OF_STRUCT_PACKING
/*
Set the minimum and maximum values which a "pack alignment" value may have.
This is an alignment that is the maximum alignment for a nonstatic data
member of a class; it can force a member to be aligned at a lesser alignment
than its type type would normally require.
*/
#ifndef TARG_MINIMUM_PACK_ALIGNMENT
#define TARG_MINIMUM_PACK_ALIGNMENT 1
			/* Default value, used to initialize global variable
			   targ_minimum_pack_alignment. */
#endif /* !defined(TARG_MINIMUM_PACK_ALIGNMENT) */
#ifndef TARG_MAXIMUM_PACK_ALIGNMENT
#if MICROSOFT_EXTENSIONS_ALLOWED
#define TARG_MAXIMUM_PACK_ALIGNMENT 16
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define TARG_MAXIMUM_PACK_ALIGNMENT 8
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
			/* Default value, used to initialize global variable
			   targ_maximum_pack_alignment. */
#endif /* !defined(TARG_MAXIMUM_PACK_ALIGNMENT) */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

#if BACK_END_IS_C_GEN_BE
/*
Switch that is TRUE if the C-generating back end should generate code for
gcc (the GNU C compiler).
*/

#ifndef GCC_IS_C_GEN_BE_TARGET
#define GCC_IS_C_GEN_BE_TARGET FALSE
#endif /* !defined(GCC_IS_C_GEN_BE_TARGET) */
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE
/*
Switch that is TRUE if the C-generating back end should generate ANSI C
instead of K&R C.
*/
#ifndef C_GEN_BE_GENERATES_ANSI_C
#if GCC_IS_C_GEN_BE_TARGET
#define C_GEN_BE_GENERATES_ANSI_C TRUE
#else /* !GCC_IS_C_GEN_BE_TARGET */
#define C_GEN_BE_GENERATES_ANSI_C FALSE
#endif /* GCC_IS_C_GEN_BE_TARGET */
#endif /* !defined(C_GEN_BE_GENERATES_ANSI_C) */
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE
/*
If SUPPRESS_CONST_IN_GENERATED_C is TRUE, "const" will not be put out when
the C-generating back end generates ANSI C.  (const is never put out when
generating K&R C.)
*/
#ifndef SUPPRESS_CONST_IN_GENERATED_C
#define SUPPRESS_CONST_IN_GENERATED_C FALSE
#endif /* !defined(SUPPRESS_CONST_IN_GENERATED_C) */

/*
If ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C is TRUE, "register" will be put
out for register variables whose address is taken.  This can occur when
compiling ANSI C code in SVR4 C compatibility mode.
*/
#ifndef ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C
#define ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C FALSE
#endif /* !defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C) */
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE
/*
If the C-generating back end is being used, and the target environment
has .init sections (e.g., SVR4), this flag is TRUE to enable generation of
asm directives to get startup routines called (thus eliminating the need
for patch or munch).  The form of the generated lines is right for Solaris.
*/
#ifndef USE_INIT_SECTION_IN_GENERATED_C
#define USE_INIT_SECTION_IN_GENERATED_C FALSE
#endif /* ifndef USE_INIT_SECTION_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/*
When generating C or C++ code, add extra braces around "if" statements
without an "else" to avoid the "dangling else" problem.  This is necessary
only if customer code modifies the IL statement tree.
*/
#ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
#define ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C FALSE
#endif /* ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the "restrict" keyword should
be suppressed in the output.  This flag is only applicable if
RESTRICT_ALLOWED is TRUE.
*/
#ifndef SUPPRESS_RESTRICT_IN_GENERATED_CODE
#define SUPPRESS_RESTRICT_IN_GENERATED_CODE FALSE
#endif /* SUPPRESS_RESTRICT_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the Microsoft qualifiers
should be suppressed in the output.  This flag is only applicable if
MICROSOFT_KEYWORDS_ALLOWED is TRUE.
*/
#ifndef SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
#define SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE FALSE
#endif /* SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */

#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#if DO_IL_LOWERING

/* Switches that control aspects of IL lowering: */

/*
This switch controls whether or not types and static variables that are local
to function and block scopes are moved onto the file scope lists.  Such
entities are allocated in the file scope memory region, but they are
normally linked on the local scope types or variables list.  That accurately
reflects the source form, which is desirable for generating symbolic
debug information.  That form probably works fine when the IL is being
fed into a true back end, but will not work when the IL is being turned
into C output (as with the C-generating back end), because the local
types and variables will not be visible from member functions of local
classes and in a file-scope termination routine when it deals with
calling a destructor for a local static variable.  When the switch here
is TRUE, the local types and variables will be (selectively) promoted
to the actual file scope.
*/
#ifndef PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
#define PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE BACK_END_IS_C_GEN_BE
#endif /* ifndef PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */

/*
This switch controls whether or not all functions and function calls will
be turned into old-style unprototyped form.  This is what cfront effectively
does, in generating old-style C that is compiled by a C compiler.
This change is important if one wants to be able to call libraries that
were compiled by cfront.  (cfront's +a1 option requests generation of ANSI C
code; if one wants compatibility with cfront in that mode, this option
should be set to FALSE.)  This is the initial value of the variable
make_all_functions_unprototyped.  There is no command-line option to
change that variable, but having a variable makes it possible to have one.
*/
#ifndef MAKE_ALL_FUNCTIONS_UNPROTOTYPED
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED CFRONT_OBJECT_CODE_COMPATIBILITY
#endif /* ifndef MAKE_ALL_FUNCTIONS_UNPROTOTYPED */

/*
When this switch is TRUE, exception handling features will be completely
lowered to C form, in a portable way.  All exception-handling statements and
expressions are completely lowered, code is generated to maintain an
exception handling stack, and cleanup tables and typeinfo entries are
generated.  Note that the C-generating back end requires this mode.
*/
#ifndef DO_FULL_PORTABLE_EH_LOWERING
#define DO_FULL_PORTABLE_EH_LOWERING TRUE
#endif /* ifndef DO_FULL_PORTABLE_EH_LOWERING */

/*
When this switch is TRUE, cleanup tables and typeinfo entries will be generated
for exception-handling constructs.
*/
#if DO_FULL_PORTABLE_EH_LOWERING
#undef GENERATE_EH_TABLES
#define GENERATE_EH_TABLES TRUE  /* Do not change this. */
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#ifndef GENERATE_EH_TABLES
#define GENERATE_EH_TABLES FALSE
#endif /* ifndef GENERATE_EH_TABLES */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

/*
When this switch is TRUE, the object lifetime information in the IL
will be left around by IL lowering, but only when exceptions are
enabled.
*/
#if !GENERATE_EH_TABLES
#undef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
                                          /* Do not change this VVVV */
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED TRUE
#else /* GENERATE_EH_TABLES */
#ifndef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED FALSE
#endif /* KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#endif /* !GENERATE_EH_TABLES */

/*
This switch, which affects the portable implementation of exception
handling, controls generation of extra code and extra conditional
flags to ensure that all appropriate expression temporaries are destroyed
on the occurrence of an exception in an expression that includes unordered
construction of temporaries.  "Unordered" temporaries are constructed
in parts of an expression that are, by C/C++ language rules, unordered
with respect to one another.  For example, in "A(1) + A(2)" the temporary
for A(1) could be constructed before or after the temporary for A(2).
The problem for a portable implementation of exception handling is that
is it necessary when indicating the cleanup to be done at the point of
construction of A(1) or A(2) to indicate that the other temporary might
or might not already have been constructed.  This is done by adding a
conditional flag for each temporary that indicates whether or not the
construction has been done, and having the cleanup position in the
cleanup region table include destructions for both temporaries.  The
runtime then tests the conditional flags to indicate whether the
destructions should actually be done.  This method, of course, involves
a time and space overhead, so the present flag is provided as a way to
switch it off.  It can be switched off if the back end will do the
constructions in the canonical order indicated by the IL, or if the
next_in_destruction_list linkage of the initializations is changed before
IL lowering to match the order that will actually be used by the back end,
or if the back end will use some different technique to accomplish the
same result.  Note that when using the C-generating back end, the
order in which temporaries are constructed depends on the C compiler
that will be used, so if one has detailed information about the order
in which it will evaluate expressions, one may be able to switch off
this processing.
*/
#ifndef DO_UNORDERED_EH_PROCESSING
#if GENERATE_EH_TABLES
#define DO_UNORDERED_EH_PROCESSING TRUE
#else /* !GENERATE_EH_TABLES */
#define DO_UNORDERED_EH_PROCESSING FALSE
#endif /* GENERATE_EH_TABLES */
#endif /* ifndef DO_UNORDERED_EH_PROCESSING */

/*
Integer kind to use for an offset into a class.  Its size must match
TARG_SIZEOF_PTR_TO_DATA_MEMBER.
*/
#ifndef TARG_DELTA_INT_KIND
#define TARG_DELTA_INT_KIND ((an_integer_kind)ik_short)
#endif /* ifndef TARG_DELTA_INT_KIND */

/*
Integer kind to use for an index into a virtual function table.  Must be
no smaller than the size of a_virtual_function_number.
*/
#ifndef TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND ((an_integer_kind)ik_short)
#endif /* ifndef TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND */

/*
This switch controls whether or not operations with
returns_lvalue_instead_of_usual_rvalue TRUE are rewritten by IL lowering.
These are operations (specifically, assignments, prefix ++/--, and
the "?" and "," operators) that return rvalues in C but can return lvalues
in C++.  When this switch is TRUE, the non-C cases are transformed into
valid C, which usually requires some duplication of parts of the expression
tree.
*/
#ifndef LOWER_LVALUE_RETURNING_OPERATIONS
#define LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !defined(LOWER_LVALUE_RETURNING_OPERATIONS) */

/*
This switch controls whether or not "guard" code is placed around
initializations of static data members of templates.  Such guard code is
necessary if template instantiation resolution is done by instantiating
everything and then having the (specially-modified) linker discard
duplicate copies of instantiated routines.  Static data members are
a particular problem: because the initialization code is generated
in startup routines, and is undifferentiated from other code in
those routines, a flag is needed to indicate that initialization
has already been done.  After any one instance of the code does
initialization, all other instances will do nothing.
*/
#ifndef TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
#define TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE FALSE
#endif /* !defined(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE) */

#if DO_FULL_PORTABLE_EH_LOWERING
/*
jmp_buf is a type defined by <setjmp.h> for use in setjmp/longjmp.
IL lowering uses setjmp/longjmp for the portable implementation of
exception try/throw statements.  jmp_buf is defined to be an array type;
we further assume it is an array of some kind of integral or floating
type, which is not guaranteed by the standard but is usually a safe
assumption.  The definitions here specify the number of elements in
the array type and the integral or floating kind for the array element
type.
*/
#ifndef TARG_JMP_BUF_NUM_ELEMENTS
#define TARG_JMP_BUF_NUM_ELEMENTS 9  /* For SPARC, SunOS 4.1.2. */
#endif /* !defined(TARG_JMP_BUF_NUM_ELEMENTS) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_num_elements. */
#ifndef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT FALSE
#endif /* ifndef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT */
			/* Choose between integer and float. */
#ifndef TARG_JMP_BUF_ELEMENT_INT_KIND
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)
#endif /* !defined(TARG_JMP_BUF_ELEMENT_INT_KIND) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_element_int_kind. */
#ifndef TARG_JMP_BUF_ELEMENT_FLOAT_KIND
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND ((a_float_kind)fk_long_double)
#endif /* !defined(TARG_JMP_BUF_ELEMENT_FLOAT_KIND) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_element_float_kind. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if GENERATE_EH_TABLES
/*
The integral kind to be used for a cleanup region number with exception
processing.
*/
#ifndef TARG_REGION_NUMBER_INT_KIND
#define TARG_REGION_NUMBER_INT_KIND ((an_integer_kind)ik_unsigned_short)
#endif /* ifndef TARG_REGION_NUMBER_INT_KIND */

/*
The integral kind to be used for a local variable identifier in exception
processing.  In the portable scheme, this is an index into the object
address table.  In the partial-lowering scheme, it is an offset in the
stack.
*/
#ifndef TARG_VAR_HANDLE_INT_KIND
#if DO_FULL_PORTABLE_EH_LOWERING
#define TARG_VAR_HANDLE_INT_KIND ((an_integer_kind)ik_unsigned_short)
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#define TARG_VAR_HANDLE_INT_KIND TARG_SIZE_T_INT_KIND
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifndef TARG_VAR_HANDLE_INT_KIND */
#endif /* GENERATE_EH_TABLES */

#endif /* DO_IL_LOWERING */

#endif /* !defined(TARG_DEF_H) */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
