/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
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

#if __ANSIC__
/* Include float.h to get the definition of things like FLT_MANT_DIG, etc. */
#include <float.h>
#endif /* __ANSIC__ */

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* !defined(HOST_ENVIR_H) */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* !defined(LANG_FEAT_H) */

/*
Flag used to retain ABI (Application Binary Interface, i.e., runtime layout
and calling sequence) compatibility with older versions.  The value is the
version number of the EDG C++ front end, e.g., 227 for version 2.27, for
which compatibility should be maintained.  ABI changes made after that
version will be suppressed.  Of course, that may suppress certain language
features that cannot be implemented without the corresponding ABI changes.
Note that even if the ABI version is set to newer version numbers,
if CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE certain language features
will be turned off.  Those features (e.g., RTTI) can be turned on
explicitly, and the front end will work, but you will have a version
with an ABI that is only cfront-like, not cfront-compatible.
*/
#ifndef ABI_COMPATIBILITY_VERSION
#define ABI_COMPATIBILITY_VERSION 9999 /* Latest version. */
#endif /* ifndef ABI_COMPATIBILITY_VERSION */

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
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY FALSE
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

/*
TRUE if code that exploits a cfront 2.1 bug that causes a global name to be
used by a member function when a base class has an entity with the same name.
The conditions under which this bug occurs are quite complicated.  The
full description can be found in lookup.c in the description of
check_for_cfront_name_lookup_bug.  The flag
CFRONT_2_1_OBJECT_CODE_COMPATIBILITY in targ_def.h must be TRUE when this
feature is used.  This is really a language feature and therefore would
be expected to be in lang_feat.h, but if it were there it couldn't
choose a default based on CFRONT_2_1_OBJECT_CODE_COMPATIBILITY.
*/
#ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG TRUE
#else /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG FALSE
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#endif /* ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/* The code that implements the cfront name lookup bug makes use of the
   information recorded for semivisible nested type handling.  Consequently,
   2.1 compatibility mode is required to use the name lookup bug. */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
 #error -- cfront name lookup bug support requires cfront 2.1 compatibility
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/*
Certain C99 features require IL constructs not otherwise present.
Because certain back ends may not support the new constructs, a mechanism
is provided to disable the C99 features that require back end support.
The C99_IL_EXTENSIONS_SUPPORTED flag should be TRUE if a back end
is prepared to accept all of the C99 IL extensions.
*/
#ifndef C99_IL_EXTENSIONS_SUPPORTED
#define C99_IL_EXTENSIONS_SUPPORTED TRUE
#endif /* ifndef C99_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE when C99 IL constructs should be lowered to constructs that
fit in the IL definition for C89.  This may result in calls to a C99 runtime
support library.
*/
#ifndef DO_C99_IL_LOWERING
#if DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED
#define DO_C99_IL_LOWERING TRUE
#else /* !(DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED) */
#define DO_C99_IL_LOWERING FALSE
#endif /* DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef DO_C99_IL_LOWERING */
#if DO_C99_IL_LOWERING && !DO_IL_LOWERING
 #error -- C99 IL lowering cannot be done if DO_IL_LOWERING is FALSE
#endif /* DO_C99_IL_LOWERING && !DO_IL_LOWERING */
#if DO_C99_IL_LOWERING && !C99_IL_EXTENSIONS_SUPPORTED
 #error -- C99 IL lowering cannot be done if C99 IL extensions not supported
#endif /* DO_C99_IL_LOWERING && !C99_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE if the "long long" data type and the associated language
features (e.g., suffixes for constants) are allowed.  "long long" is
standard in C99, and Microsoft mode needs the IL support for __int64.

This is really a language feature configuration macro, and as such should
be in lang_feat.h.  However, it affects the IL and requires support
from a back end, and the most sensible default takes into account
whether C99 IL extensions are supported, and that is only known here.
*/
#ifndef LONG_LONG_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED
#define LONG_LONG_ALLOWED TRUE
#else /* !(MICROSOFT_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED) */
#define LONG_LONG_ALLOWED FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef LONG_LONG_ALLOWED */

/*
Alignment required of pointers to malloc'd space (i.e., the maximum
alignment required by the host computer).  Use "1" if there are no
alignment requirements.  This must be defined as an actual constant
rather than as something like "sizeof(int)"; see mem_manage.h.
Note that space allocated by malloc must provide at least this
alignment, or the front end is powerless to provide the requested
alignment.

This is really a host configuration macro, and as such should be
in host_envir.h.  However, the most sensible default takes into
account whether LONG_LONG_ALLOWED is set, and that is only known
here.
*/
#ifndef HOST_ALIGNMENT_REQUIRED
#ifdef __alpha
#define HOST_ALIGNMENT_REQUIRED 8
#else /* !defined(__alpha) */
#if LONG_LONG_ALLOWED
#define HOST_ALIGNMENT_REQUIRED 8
#else /* !LONG_LONG_ALLOWED */
#define HOST_ALIGNMENT_REQUIRED 4
#endif /* LONG_LONG_ALLOWED */
#endif /* ifdef __alpha */
#endif /* ifndef HOST_ALIGNMENT_REQUIRED */

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
#define TARG_BACKSPACE_CHAR   '\b'
#define TARG_FORM_FEED_CHAR   '\f'
#define TARG_NEWLINE_CHAR     '\n'
#define TARG_CARR_RETURN_CHAR '\r'
#define TARG_HORIZ_TAB_CHAR   '\t'

#ifndef TARG_ALERT_CHAR
#define TARG_ALERT_CHAR       '\007'
#endif /* ifndef TARG_ALERT_CHAR */
#ifndef TARG_VERT_TAB_CHAR
#define TARG_VERT_TAB_CHAR    '\013'
#endif /* ifndef TARG_VERT_TAB_CHAR */
#ifndef TARG_ESC_CHAR
#define TARG_ESC_CHAR         '\033'
#endif /* ifndef TARG_ESC_CHAR */

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
If LONG_LONG_ALLOWED is TRUE, the host "long long" and "unsigned long Long"
are used by default.
*/
#ifndef TYPE_FOR_AN_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long long
#else /* !LONG_LONG_ALLOWED */
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef TYPE_FOR_AN_INTEGER_VALUE */
typedef TYPE_FOR_AN_INTEGER_VALUE an_integer_value;
#ifndef TYPE_FOR_A_SIGNED_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long long
#else /* !LONG_LONG_ALLOWED */
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef TYPE_FOR_A_SIGNED_INTEGER_VALUE */
typedef TYPE_FOR_A_SIGNED_INTEGER_VALUE a_signed_integer_value;

/* Minimum and maximum values that can be represented in an_integer_value. */
#ifndef MAX_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#ifdef LLONG_MAX
#define MAX_INTEGER_VALUE LLONG_MAX
#else /* !defined(LLONG_MAX) */
#define MAX_INTEGER_VALUE 9223372036854775807LL /* 64-bit */
#endif /* ifdef LLONG_MAX */
#else /* !LONG_LONG_ALLOWED */
#define MAX_INTEGER_VALUE LONG_MAX
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef MAX_INTEGER_VALUE */
#ifndef MIN_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#ifdef LLONG_MIN
#define MIN_INTEGER_VALUE LLONG_MIN
#else /* !defined(LLONG_MIN) */
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#endif /* ifdef LLONG_MIN */
#else /* !LONG_LONG_ALLOWED */
#define MIN_INTEGER_VALUE LONG_MIN
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef MIN_INTEGER_VALUE */
#ifndef MAX_UNSIGNED_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#ifdef ULLONG_MAX
#define MAX_UNSIGNED_INTEGER_VALUE ULLONG_MAX
#else /* !defined(ULLONG_MAX) */
#define MAX_UNSIGNED_INTEGER_VALUE 18446744073709551615ULL /* 64-bit */
#endif /* ifdef ULLONG_MAX */
#else /* !LONG_LONG_ALLOWED */
#define MAX_UNSIGNED_INTEGER_VALUE ULONG_MAX
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef MAX_UNSIGNED_INTEGER_VALUE */
#ifndef BITS_IN_AN_INTEGER_VALUE
#define BITS_IN_AN_INTEGER_VALUE (sizeof(an_integer_value) * CHAR_BIT)
#endif /* ifndef BITS_IN_AN_INTEGER_VALUE */
/* The printf formatting specifier to be used to print the integer type. */
#ifndef PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#define PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE   "%lld" /* long long */
#else /* !LONG_LONG_ALLOWED */
#define PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE   "%ld"  /* long */
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE */
#ifndef PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#define PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE "%llu" /* unsigned long long*/
#else /* !LONG_LONG_ALLOWED */
#define PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE "%lu"  /* unsigned long */
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE */
#ifndef PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE
#if LONG_LONG_ALLOWED
#define PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE      "%llx" /* hex long long */
#else /* !LONG_LONG_ALLOWED */
#define PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE      "%lx"  /* hex long */
#endif /* LONG_LONG_ALLOWED */
#endif /* ifndef PRINTF_FORMAT_FOR_HEX_INTEGER_VALUE */

/*
Host types used to manipulate integer values.
*/
typedef a_signed_integer_value a_host_large_integer;
typedef an_integer_value a_host_large_unsigned;

/*
The printf formatting specifier to be used to print a host large integer.
These default to the values used for integer values when an integer value
is a host integer.
*/
#ifndef PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER
#define PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER \
			PRINTF_FORMAT_FOR_SIGNED_INTEGER_VALUE
#endif /* ifndef PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER */
#ifndef PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED
#define PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED \
			PRINTF_FORMAT_FOR_UNSIGNED_INTEGER_VALUE
#endif /* ifndef PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED */

/*
Minimum and maximum values that can be represented in a_host_large_integer
and a_host_large_unsigned.
*/
#define MAX_HOST_LARGE_INTEGER MAX_INTEGER_VALUE
#define MIN_HOST_LARGE_INTEGER MIN_INTEGER_VALUE
#define MAX_HOST_LARGE_UNSIGNED MAX_UNSIGNED_INTEGER_VALUE

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
There is no host integer that is large enough, so use an array to represent
the target integers.
*/
/* Type of the elements of the array.  These must be at most half the
   size of a_host_large_integer (some large and efficient integer type on
   the host), and (for space reasons) preferably exactly half.
   Typically, this is a 16-bit value.  The bit size and
   maximum values indicate the range of values to be used, which may
   be smaller than the range actually available.  That is, one could
   use a type that is larger than half of a_host_large_integer but
   restrict the values to be used to just half of a_host_large_integer
   if no suitable smaller type is available. */
typedef unsigned short an_int_value_part;
#define MAX_UINT_VALUE_PART 0xffff
#define SIGN_BIT_INT_VALUE_PART 0x8000
#define SIZEOF_INT_VALUE_PART (sizeof(an_int_value_part)) /* Okay to change. */
#define BITS_IN_INT_VALUE_PART (SIZEOF_INT_VALUE_PART*CHAR_BIT)
/* Large and efficient host integer, at least twice the size of
   an_int_value_part, used in doing computations on integer values.
   The idea is that any operation involving two an_int_value_part
   values can be done in a_host_large_integer without special coding
   to deal with overflows.  These types are also used to manipulate integer
   values that are a subset of the values that can be represented by
   an_integer_value when AN_INTEGER_VALUE_REPR_IS_A_HOST_INTEGER is
   FALSE.  Many operations can be done using these types, because
   most constant values are small.  When the values are too large,
   alternate routines are used. */
typedef long a_host_large_integer;
typedef unsigned long a_host_large_unsigned;
#define MAX_HOST_LARGE_INTEGER LONG_MAX
#define MIN_HOST_LARGE_INTEGER LONG_MIN
#define MAX_HOST_LARGE_UNSIGNED ULONG_MAX

/* Define a macro that has the same value as TARG_CHAR_BIT.  This is done
   because TARG_CHAR_BIT cannot be used outside of targ_def.h (it gets
   undefined below).  The simulated integer routines do not support 
   implementations on which targ_char_bit can be changed. */
#ifndef INTERNAL_TARG_CHAR_BIT
#if TARG_CHAR_BIT == 8
#define INTERNAL_TARG_CHAR_BIT 8
#else /* TARG_CHAR_BIT != 8 */
#if TARG_CHAR_BIT == 16
#define INTERNAL_TARG_CHAR_BIT 16
#else /* TARG_CHAR_BIT != 16 */
#if TARG_CHAR_BIT == 24
#define INTERNAL_TARG_CHAR_BIT 24
#else /* TARG_CHAR_BIT != 24 */
#if TARG_CHAR_BIT == 32
#define INTERNAL_TARG_CHAR_BIT 32
#else /* TARG_CHAR_BIT != 32 */
 #error -- do not know how to set INTERNAL_TARG_CHAR_BIT
#endif /* TARG_CHAR_BIT == 32 */
#endif /* TARG_CHAR_BIT == 24 */
#endif /* TARG_CHAR_BIT == 16 */
#endif /* TARG_CHAR_BIT == 8 */
#endif /* ifndef INTERNAL_TARG_CHAR_BIT */

#define BITS_IN_HOST_LARGE_INTEGER (sizeof(a_host_large_integer)*CHAR_BIT)
/* The array is made up of elements of type an_int_value_part.
   Figure out how many. */
#define INT_VALUE_PARTS_PER_INTEGER_VALUE                             \
  ((TARG_SIZEOF_LARGEST_INTEGER*INTERNAL_TARG_CHAR_BIT)/	      \
   (SIZEOF_INT_VALUE_PART*CHAR_BIT))
/* This is an array inside a struct instead of just an array so that
   its address behaves in a predictable way. */
typedef struct an_integer_value {
  an_int_value_part part[INT_VALUE_PARTS_PER_INTEGER_VALUE];
} an_integer_value;
#define BITS_IN_AN_INTEGER_VALUE (BITS_IN_INT_VALUE_PART *	      \
				  INT_VALUE_PARTS_PER_INTEGER_VALUE)

/*
The printf formatting specifier to be used to print a host large integer.
*/
#ifndef PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER
#define PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER "%ld"  /* long */
#endif /* ifndef PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER */
#ifndef PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED
#define PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED "%lu"  /* unsigned long */
#endif /* ifndef PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED */

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
If this flag is TRUE, overflows on signed integer operations do
not cause errors (only warnings).  Usually this would be set to
match the target machine behavior on integer operations in C.
*/
#ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define TARG_NO_ERROR_ON_INTEGER_OVERFLOW TRUE
#endif /* ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW */

/* If this flag is TRUE, bit-field allocation follows the conventions of
   Microsoft C/C++.  Note that TARG_MICROSOFT_BIT_FIELD_ALLOCATION is set
   independently of MICROSOFT_EXTENSIONS_ALLOWED -- the former has more to
   do with ABI compatibility, the latter with language features that are
   accepted. */
/* When TARG_MICROSOFT_BIT_FIELD_ALLOCATION is TRUE, the setting of
   TARG_BIT_FIELD_CONTAINER_SIZE is required to be -1 and there is a
   two-stage allocation: first, a bit-field container based on the bit-field
   type is allocated (as though it were a field in its own right), and then
   bit fields are allocated within it. When the bit-field type changes or
   the container fills up, a new container is allocated. */
#ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION FALSE
			/* Default value, used to initialize global variable
			   targ_microsoft_bit_field_allocation. */
#endif /* ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION */

/* Container size to be used for bit-fields.  If > 0, indicates the
   size in bytes of one of the integral types.  0 means "use the smallest
   integral type into which the field will fit".  < 0 means "use the
   base type given in the declaration". */
/* Note that if the C-generating back end is being used, the setting of
   this switch must match ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
   so that the front end's layout code gets the same result that the
   underlying C compiler will get. */
#ifndef TARG_BIT_FIELD_CONTAINER_SIZE
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY && ABI_COMPATIBILITY_VERSION >= 232
/* In the C code it generates, cfront changes the underlying types of all
   bit-fields to int or unsigned int. */
#define TARG_BIT_FIELD_CONTAINER_SIZE TARG_SIZEOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY... */
#define TARG_BIT_FIELD_CONTAINER_SIZE 0
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY... */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
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

/* Signedness for enum bit fields (an extension in C): if TRUE, enum bit fields
   are always unsigned.  If FALSE, the rules are: (a) if the enum contains
   any negative values, the field is signed; otherwise (b) if the enum
   contains values large enough that they won't fit if one bit is allocated
   for a sign, the field is unsigned; otherwise (c) the signedness is as
   indicated by TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED. */
/* When the flag is TRUE, declaring a bit field with an enumeration type
   that includes negative enum constants will elicit a warning; moreover,
   the value extracted from the bit field will always be treated as an
   unsigned quantity (i.e., sign extension will not be done when extracting
   the value).  On the other hand, when the flag is FALSE, the value
   extracted from the bit field may be treated as a signed quantity (and
   sign extension may be done unexpectedly). */
#ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
/* The Microsoft compiler treats enum bit fields as signed or unsigned. */
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED FALSE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* Cfront treats enum bit fields as unsigned. */
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED FALSE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_enum_bit_fields_are_always_unsigned. */
#endif /* ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */

/* Alignment adjustment to be made when a zero-width (unnamed) bit field is
   declared.  If > 0 it is the alignment to be used (typically the alignment
   of one of the integral types).  A value of zero means "use the minimal
   alignment", which is single-byte alignment.  Any value less than zero
   means "use the alignment of the base type given in the declaration". */
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT (-1)
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* cfront changes all bit fields to int or unsigned int in the generated
   C code. */
/* This feature CAN be changed when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT TARG_ALIGNOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 0
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_zero_width_bit_field_alignment. */
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */

/* TRUE when a zero-width bit field, typically used to control the alignment
   of the next field, thereby also affects how the alignment of the struct
   as a whole is determined.  Should be TRUE for cfront and Microsoft ABI
   compatibility. */
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY && ABI_COMPATIBILITY_VERSION >= 232
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY... */
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY... */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			 targ_zero_width_bit_field_affects_struct_alignment. */
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT */

/* TRUE when an unnamed bit field, typically used to control the alignment
   of the next field, thereby also affects how the alignment of the struct
   as a whole is determined. */
#ifndef TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if ABI_COMPATIBILITY_VERSION <= 231
/* Setting it to FALSE corresponds to hard-coded behavior prior to 2.32. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#else /* !(ABI_COMPATIBILITY_VERSION <= 231) */
#if ABI_COMPATIBILITY_VERSION >= 235
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* This can be changed. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
/* This can be changed. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#else /* !(ABI_COMPATIBILITY_VERSION >= 235) */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#endif /* ABI_COMPATIBILITY_VERSION >= 235 */
#endif /* ABI_COMPATIBILITY_VERSION <= 231 */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_unnamed_bit_field_affects_struct_alignment. */
#endif /* ifndef TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT */

/*
Flag that is TRUE if "#pragma pack(n)" and the command-line option
"--pack_alignment=n", when supported, affect the container boundary/alignment
of bit fields.  FALSE indicates that TARG_BIT_FIELD_CONTAINER_SIZE controls
the container boundary/alignment at all times.
*/
#ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS  \
                                              USER_CONTROL_OF_STRUCT_PACKING
#endif /* ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS */


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
Integral kind to be used for the bool type in C++.
*/
#ifndef TARG_BOOL_INT_KIND
#define TARG_BOOL_INT_KIND ((an_integer_kind)ik_char)
			/* Default value, used to initialize global variable
			   targ_bool_int_kind. */
#endif /* !defined(TARG_BOOL_INT_KIND) */

/*
Pointer types:
*/
#if NEAR_AND_FAR_ALLOWED
/*
Sizes of near/far pointers (e.g., in 16-bit Microsoft mode; note that these
values are not used in 32-bit Microsoft mode).
*/
#ifndef TARG_SIZEOF_FAR_POINTER
#define TARG_SIZEOF_FAR_POINTER 4
#endif /* ifndef TARG_SIZEOF_FAR_POINTER */
#ifndef TARG_ALIGNOF_FAR_POINTER
#define TARG_ALIGNOF_FAR_POINTER 4
#endif /* ifndef TARG_ALIGNOF_FAR_POINTER */
#ifndef TARG_SIZEOF_NEAR_POINTER
#define TARG_SIZEOF_NEAR_POINTER 2
#endif /* ifndef TARG_SIZEOF_NEAR_POINTER */
#ifndef TARG_ALIGNOF_NEAR_POINTER
#define TARG_ALIGNOF_NEAR_POINTER 2
#endif /* ifndef TARG_ALIGNOF_NEAR_POINTER */
#endif /* NEAR_AND_FAR_ALLOWED */

/*
Are all pointers the same size?

If not, see also size_of_pointer_to, and you may want to define
pointer_types_have_same_repr if there's some aspect of pointer representation
that is not completely determined by the type pointed to (e.g., you have
both 32-bit and 64-bit pointers, for any underlying type, and you can choose
between them with some language extension).

Note that TARG_ALL_POINTERS_SAME_SIZE is not consulted in when near and far
pointers exist (e.g., in 16-bit Microsoft mode).  So this really means,
"ignoring near/far mode, are all pointers the same size?"
*/
#ifndef TARG_ALL_POINTERS_SAME_SIZE
#define TARG_ALL_POINTERS_SAME_SIZE TRUE
#endif /* !defined(TARG_ALL_POINTERS_SAME_SIZE) */

#if TARG_ALL_POINTERS_SAME_SIZE
/*
All pointers have the same size and alignment, so TARG_SIZEOF_POINTER and
TARG_ALIGNOF_POINTER should be defined.
*/
#ifndef TARG_SIZEOF_POINTER
#if NEAR_AND_FAR_ALLOWED
#define TARG_SIZEOF_POINTER TARG_SIZEOF_FAR_POINTER
#else /* !NEAR_AND_FAR_ALLOWED */
#define TARG_SIZEOF_POINTER 4
#endif /* NEAR_AND_FAR_ALLOWED */
			/* Default value, used to initialize global variable
			   targ_sizeof_pointer. */
#endif /* !defined(TARG_SIZEOF_POINTER) */
#ifndef TARG_ALIGNOF_POINTER
#if NEAR_AND_FAR_ALLOWED
#define TARG_ALIGNOF_POINTER TARG_ALIGNOF_FAR_POINTER
#else /* !NEAR_AND_FAR_ALLOWED */
#define TARG_ALIGNOF_POINTER 4
#endif /* NEAR_AND_FAR_ALLOWED */
			/* Default value, used to initialize global variable
			   targ_alignof_pointer. */
#endif /* !defined(TARG_ALIGNOF_POINTER) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/*
Pointers may have different sizes and alignments, so TARG_SIZEOF_POINTER and
TARG_ALIGNOF_POINTER are meaningless.  Consequently, all other definitions
that depend on TARG_SIZEOF_POINTER and TARG_ALIGNOF_POINTER need to be
configured in other terms, and it also means that global variables
targ_sizeof_pointer and targ_alignof_pointer will not be declared at all.
*/
#ifdef TARG_SIZEOF_POINTER
 #error -- do not use TARG_SIZEOF_POINTER if !TARG_ALL_POINTERS_SAME_SIZE
#endif /* defined(TARG_SIZEOF_POINTER) */
#ifdef TARG_ALIGNOF_POINTER
 #error -- do not use TARG_ALIGNOF_POINTER if !TARG_ALL_POINTERS_SAME_SIZE
#endif /* defined(TARG_ALIGNOF_POINTER) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

/* Indication of whether NULL pointer is like integer zero. */
#ifndef TARG_NULL_IS_ALL_BITS_ZERO
#define TARG_NULL_IS_ALL_BITS_ZERO TRUE
#endif /* ifndef TARG_NULL_IS_ALL_BITS_ZERO */

/* Integer type for the difference of two pointer types (ptrdiff_t).
   This type must be signed.  See 3.3.6 in the standard and the header
   file <stddef.h>. */
typedef a_host_large_integer a_targ_ptrdiff_t;  /* Must be
                                                   a_host_large_integer. */

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
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
			/* Default value, used to initialize global variable
			   targ_ptrdiff_t_int_kind. */
#endif /* ifndef TARG_PTRDIFF_T_INT_KIND */

/* size_t, used for size of arrays, offsets in fields, type of sizeof, etc.
   This type must be unsigned.  See 3.3.3.4 in the standard and the header
   file <stddef.h>. */
/* a_targ_size_t is the container used to hold size_t values on the host.
   It must be large enough to hold all the target size_t values, but can
   be larger. */
typedef a_host_large_unsigned a_targ_size_t;  /* Must be
						 a_host_large_unsigned. */
#ifndef TARG_SIZE_T_INT_KIND
/* Pick a typical representation for size_t: the smaller of unsigned int or
   unsigned long that can hold a pointer value. */
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)UINT_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of unsigned long is arbitrary. */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
			/* Default value, used to initialize global variable
			   targ_size_t_int_kind. */
#endif /* ifndef TARG_SIZE_T_INT_KIND */
/* TARG_SIZE_T_MAX defines the limit of the host representation
   of size_t constants; the range it defines can be equal to or smaller
   than the integer size implied by TARG_SIZE_T_INT_KIND. */
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
			/* Default value, used to initialize global variable
			   targ_size_t_max. */
#endif /* ifndef TARG_SIZE_T_MAX */

/* Specification of a target alignment requirement.  1 means no alignment
   requirement.  (TARG_MAXIMUM_PACK_ALIGNMENT must fit in this type.) */
#ifndef TYPE_FOR_TARG_ALIGNMENT
#define TYPE_FOR_TARG_ALIGNMENT a_byte
#endif /* ifndef TYPE_FOR_TARG_ALIGNMENT */

typedef TYPE_FOR_TARG_ALIGNMENT a_targ_alignment;

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
Type used to perform host floating point computations.  In general,
if long double is available, it should be used.  But if long double
and double are the same size, double may be used.
*/
#ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#if USING_ISO_C
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE TRUE
#else /* !USING_ISO_C */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE FALSE
#endif /* !USING_ISO_C */
#endif /* ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */

#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
typedef long double a_host_fp_value;
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
typedef double a_host_fp_value;
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */

/*
TRUE if the target supports IEEE floating point, i.e., it has NaNs
and Infinities.  Note that unless float_pt.c is rewritten this also
implies that the host supports IEEE floating point, because the
default float_pt.c support uses the host floating point.
*/
#ifndef TARG_HAS_IEEE_FLOATING_POINT
#if defined(sparc) || defined(__linux__) || EDG_WIN32
/* SPARC supports IEEE floating point. */
/* Linux (X86, Alpha, PowerPC, SPARC) supports IEEE floating point. */
/* Windows X86 supports IEEE floating point. */
#define TARG_HAS_IEEE_FLOATING_POINT TRUE
#else /* defined(sparc) || ... */
/* Include <math.h> to see if the C99 NAN macro is defined. */
#include <math.h>
#ifdef NAN
/* Systems with NAN defined support IEEE floating point. */
#define TARG_HAS_IEEE_FLOATING_POINT TRUE
#else /* !defined(NAN) */
#define TARG_HAS_IEEE_FLOATING_POINT FALSE
#endif /* ifdef NAN */
#endif /* defined(sparc) || ... */
#endif /* ifndef TARG_HAS_IEEE_FLOATING_POINT */

/*
Type used to represent float quantities internally:

(This should not be an array, so that it's always known whether one gets the
address or the value of the item.)
*/
typedef struct an_internal_float_value {
  /* The type here must match the code in float_pt.c.  The default
     declaration assumes that target floating constants are represented in
     a host double or long double, which are the types supported by the
     default version of float_pt.c. */
  a_byte bytes[sizeof(a_host_fp_value)];
  /*lint -esym(768,an_internal_float_value::bytes)*/
} an_internal_float_value;


/*
The floating-point manipulation routines require an unsigned 32-bit
type.  The macro TYPE_FOR_AN_FP_VALUE_PART can be used to specify the
type to be used.
*/
#ifndef TYPE_FOR_AN_FP_VALUE_PART
#define TYPE_FOR_AN_FP_VALUE_PART unsigned long
#endif /* ifndef TYPE_FOR_AN_FP_VALUE_PART */

typedef	TYPE_FOR_AN_FP_VALUE_PART an_fp_value_part;

/*
For each floating point data type, specify the number of bits used to
represent the mantissa, and the minimum and maximum exponent values.

In each case, use a previously defined TARG_ value if one is specified.
If none is specified, use the one defined (if any) by the float.h header.
Otherwise, supply a reasonable default value.
*/
#ifndef TARG_FLT_MANT_DIG
#ifdef FLT_MANT_DIG
#define TARG_FLT_MANT_DIG FLT_MANT_DIG
#else /* ifndef FLT_MANT_DIG */
#define TARG_FLT_MANT_DIG 24
#endif /* ifdef FLT_MANT_DIG */
#endif /* ifndef TARG_FLT_MANT_DIG */

#ifndef TARG_FLT_MIN_EXP
#ifdef FLT_MIN_EXP
#define TARG_FLT_MIN_EXP FLT_MIN_EXP
#else /* ifndef FLT_MIN_EXP */
#define TARG_FLT_MIN_EXP (-125)
#endif /* ifdef FLT_MIN_EXP */
#endif /* ifndef TARG_FLT_MIN_EXP */

#ifndef TARG_FLT_MAX_EXP
#ifdef FLT_MAX_EXP
#define TARG_FLT_MAX_EXP FLT_MAX_EXP
#else /* ifndef FLT_MAX_EXP */
#define TARG_FLT_MAX_EXP (128)
#endif /* ifdef FLT_MAX_EXP */
#endif /* ifndef TARG_FLT_MAX_EXP */

#ifndef TARG_DBL_MANT_DIG
#ifdef DBL_MANT_DIG
#define TARG_DBL_MANT_DIG DBL_MANT_DIG
#else /* ifndef DBL_MANT_DIG */
#define TARG_DBL_MANT_DIG 53
#endif /* ifdef DBL_MANT_DIG */
#endif /* ifndef TARG_DBL_MANT_DIG */

#ifndef TARG_DBL_MIN_EXP
#ifdef DBL_MIN_EXP
#define TARG_DBL_MIN_EXP DBL_MIN_EXP
#else /* ifndef DBL_MIN_EXP */
#define TARG_DBL_MIN_EXP (-1021)
#endif /* ifdef DBL_MIN_EXP */
#endif /* ifndef TARG_DBL_MIN_EXP */

#ifndef TARG_DBL_MAX_EXP
#ifdef DBL_MAX_EXP
#define TARG_DBL_MAX_EXP DBL_MAX_EXP
#else /* ifndef DBL_MAX_EXP */
#define TARG_DBL_MAX_EXP (1024)
#endif /* ifdef DBL_MAX_EXP */
#endif /* ifndef TARG_DBL_MAX_EXP */

#ifndef TARG_LDBL_MANT_DIG
#ifdef LDBL_MANT_DIG
#define TARG_LDBL_MANT_DIG LDBL_MANT_DIG
#else /* ifndef LDBL_MANT_DIG */
#define TARG_LDBL_MANT_DIG 64
#endif /* ifdef LDBL_MANT_DIG */
#endif /* ifndef TARG_LDBL_MANT_DIG */

#ifndef TARG_LDBL_MIN_EXP
#ifdef LDBL_MIN_EXP
#define TARG_LDBL_MIN_EXP LDBL_MIN_EXP
#else /* ifndef LDBL_MIN_EXP */
#define TARG_LDBL_MIN_EXP (-16381)
#endif /* ifdef LDBL_MIN_EXP */
#endif /* ifndef TARG_LDBL_MIN_EXP */

#ifndef TARG_LDBL_MAX_EXP
#ifdef LDBL_MAX_EXP
#define TARG_LDBL_MAX_EXP LDBL_MAX_EXP
#else /* ifndef LDBL_MAX_EXP */
#define TARG_LDBL_MAX_EXP (16384)
#endif /* ifdef LDBL_MAX_EXP */
#endif /* ifndef TARG_LDBL_MAX_EXP */


/*
C++ pointer-to-member type.
(The formulas here are for a typical implementation, but are not required.)
Note that cfront uses "int *" for pointers to data members; we use an
integer the same size as a pointer.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
#ifndef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_data_member. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_data_member. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
  ((((2*TARG_SIZEOF_SHORT+TARG_SIZEOF_POINTER-1)/TARG_ALIGNOF_POINTER)+1)* \
    TARG_ALIGNOF_POINTER)
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_member_function. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#ifndef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_member_function. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_data_member. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_data_member. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
           (2*TARG_SIZEOF_SHORT+TARG_SIZEOF_LONG)
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_member_function. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#ifndef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_member_function. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */


/* 
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions the front end allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by the following.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
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
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_virtual_function_info. */
#endif /* !defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
#ifndef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_virtual_function_info. */
#endif /* !defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

/*
Size and alignment of a pointer to virtual base class.  Despite the name, a
"pointer-to-virtual-base-class" member may or may not actually be a "pointer".
The default implementation (namely, IL lowering) does treat it as a pointer,
but implementations are free to do otherwise.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
#ifndef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#ifndef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#ifndef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

#if GNU_EXTENSIONS_ALLOWED

/* The default value used to initialize targ_word_mode. */
#ifndef TARG_WORD_MODE
#if TARG_SIZEOF_INT == 1
#define TARG_WORD_MODE tmk_QI
#else /* TARG_SIZEOF_INT != 1 */
#if TARG_SIZEOF_INT == 2
#define TARG_WORD_MODE tmk_HI
#else /* TARG_SIZEOF_INT != 2 */
#if TARG_SIZEOF_INT == 4
#define TARG_WORD_MODE tmk_SI
#else /* TARG_SIZEOF_INT != 4 */
#if TARG_SIZEOF_INT == 8
#define TARG_WORD_MODE tmk_DI
#else /* TARG_SIZEOF_INT != 8 */
#if TARG_SIZEOF_INT == 16
#define TARG_WORD_MODE tmk_TI
#else /* TARG_SIZEOF_INT != 16 */
 #error -- do not know how to set TARG_WORD_MODE
#endif /* TARG_SIZEOF_INT != 16 */
#endif /* TARG_SIZEOF_INT != 8 */
#endif /* TARG_SIZEOF_INT != 4 */
#endif /* TARG_SIZEOF_INT != 2 */
#endif /* TARG_SIZEOF_INT != 1 */
#endif /* defined(TARG_WORD_MODE) */

/* The default value used to initialize targ_pointer_mode. */
#if TARG_ALL_POINTERS_SAME_SIZE
#ifndef TARG_POINTER_MODE
#if TARG_SIZEOF_POINTER == 1
#define TARG_POINTER_MODE tmk_QI
#else /* TARG_SIZEOF_POINTER != 1 */
#if TARG_SIZEOF_POINTER == 2
#define TARG_POINTER_MODE tmk_HI
#else /* TARG_SIZEOF_POINTER != 2 */
#if TARG_SIZEOF_POINTER == 4
#define TARG_POINTER_MODE tmk_SI
#else /* TARG_SIZEOF_POINTER != 4 */
#if TARG_SIZEOF_POINTER == 8
#define TARG_POINTER_MODE tmk_DI
#else /* TARG_SIZEOF_POINTER != 8 */
#if TARG_SIZEOF_POINTER == 16
#define TARG_POINTER_MODE tmk_TI
#else /* TARG_SIZEOF_POINTER != 16 */
 #error -- do not know how to set TARG_POINTER_MODE
#endif /* TARG_SIZEOF_POINTER != 16 */
#endif /* TARG_SIZEOF_POINTER != 8 */
#endif /* TARG_SIZEOF_POINTER != 4 */
#endif /* TARG_SIZEOF_POINTER != 2 */
#endif /* TARG_SIZEOF_POINTER != 1 */
#endif /* defined(TARG_POINTER_MODE) */

#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* GCC does not support architectures where all pointers are not the
   same size.  It does not make sense to talk about a "pointer mode"
   on such an architecture. */
#endif /* !TARG_ALL_POINTERS_SAME_SIZE */

/*
Macro to indicate that asm expressions target a processor of an x86 family.
*/
#ifndef TARG_IS_X86
#define TARG_IS_X86 FALSE
#endif /* ifndef TARG_IS_X86 */

#endif /* GNU_EXTENSIONS_ALLOWED */

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
 #error -- NEW_CAN_BE_FOLDED_INTO_CTOR may not be FALSE if \
           ASSIGNMENT_TO_THIS_ALLOWED is TRUE
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */
#if ASSIGNMENT_TO_THIS_ALLOWED && !DELETE_CAN_BE_FOLDED_INTO_DTOR
 #error -- DELETE_CAN_BE_FOLDED_INTO_DTOR may not be FALSE if \
           ASSIGNMENT_TO_THIS_ALLOWED is TRUE
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
Enumerated types:  Default setting for targ_enum_types_can_be_smaller_than_int.
If TRUE, enumerated types can be allocated in integral types smaller than int.
*/
#ifndef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* This feature CAN be changed when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
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

/*
The maximum size a class object may have.  If TARG_MAX_CLASS_OBJECT_SIZE
is nonzero, then it is the value to which targ_max_class_object_size is set.
If it is zero, then targ_max_class_object_size is set to targ_size_t_max.
*/
#ifndef TARG_MAX_CLASS_OBJECT_SIZE
#define TARG_MAX_CLASS_OBJECT_SIZE 0
#endif /* ifndef TARG_MAX_CLASS_OBJECT_SIZE */

/*
The maximum offset a base class may have.  If TARG_MAX_BASE_CLASS_OFFSET
is nonzero, then it is the value to which targ_max_base_class_offset is
set -- except that, when DO_IL_LOWERING is TRUE, a smaller value will be
used, if necessary, to accommodate the maximum offset value (as implied by
TARG_DELTA_INT_KIND) that may be stored in a virtual function table.  If
TARG_MAX_BASE_CLASS_OFFSET is zero, then targ_max_base_class_offset is set
to targ_size_t_max.
*/
#ifndef TARG_MAX_BASE_CLASS_OFFSET
#define TARG_MAX_BASE_CLASS_OFFSET 0
#endif /* ifndef TARG_MAX_BASE_CLASS_OFFSET */

/*
A flag that is TRUE if the layout mechanism should attempt to allocate empty
base classes at the same offset as other subobjects.
*/
#ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 241 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT FALSE
                                                    /* Versions up to 2.41. */
#else /* ABI_COMPATIBILITY_VERSION > 241 && !CFRONT_... */
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT TRUE
                                                    /* Versions after 2.41. */
#endif /* ABI_COMPATIBILITY_VERSION <= 241 || CFRONT_... */
#endif /* ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */
#if TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#if ABI_COMPATIBILITY_VERSION <= 241
 #error -- TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 241
#endif /* ABI_COMPATIBILITY_VERSION <= 242 */
#endif /* TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */

/*
A flag that is TRUE if an empty base that does not share its offset with
another subobject (i.e., an "allocated base") should be padded according to
its alignment instead of allocating just one byte for it.  This flag should be
TRUE if the C-generating back end is used, because a C compiler will pad the
fields of struct type representing allocated empty base subobjects if
TARG_MINIMUM_STRUCT_ALIGNMENT is larger than one.
*/
#ifndef TARG_PAD_ALLOCATED_EMPTY_BASE
#define TARG_PAD_ALLOCATED_EMPTY_BASE BACK_END_IS_C_GEN_BE
#endif /* ifndef TARG_PAD_ALLOCATED_EMPTY_BASE */

/*
When a class with a copy constructor is passed to an ellipsis, does the
copy constructor get called?  If this is TRUE, what is passed as the argument
is the address of a temporary into which the class object has been copied.
This falls under undefined behavior.
*/
#ifndef USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS
#define USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS FALSE
#endif /* ifndef USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS */

/*
If this is TRUE, dead expressions under conditional operators "&&", "||",
and "?" are eliminated.  For example, "1 ? i : j" becomes simply "i".
*/
#ifndef ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS
#define ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS FALSE
#endif /* ifndef ELIMINATE_DEAD_CODE_UNDER_CONDITIONAL_OPERATORS */

/*
If this is TRUE, explicit casts that do nothing, for example
  int i = 0; int j = (int)i;
are preserved in the IL.  This may be desirable for certain source-analysis
applications.
*/
#ifndef PRESERVE_EFFECTLESS_EXPLICIT_CASTS_IN_IL
#define PRESERVE_EFFECTLESS_EXPLICIT_CASTS_IN_IL FALSE
#endif /* ifndef PRESERVE_EFFECTLESS_EXPLICIT_CASTS_IN_IL */

/*
This switch controls whether or not type qualifiers are removed from
parameter types (e.g., a "const int" parameter is seen simply as "int").
This may seem like a language feature, but it's an ABI issue, because the
parameter type ends up in the mangled name of the function.  This value is
the default for global variable remove_qualifiers_from_param_types.
*/
#ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES FALSE
#else /* ABI_COMPATIBILITY_VERSION > 228 && !CFRONT_... */
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES TRUE
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES */

/*
Flag that is TRUE if, by default, function types are considered distinct
when their only difference is that one has extern "C" routine linkage and
the other has extern "C++" routine linkage.  It is the initial value of
global variable c_and_cpp_function_types_are_distinct.  How to set this
flag is both a language issue (overloading, type conversions) and an ABI
issue (name mangling).  For example:
  typedef void (*PF)();             // Pointer to an extern "C++" function
  extern "C" typedef void (*PCF)(); // Pointer to an extern "C" function
  void f(PF);
  void f(PCF);
When the flag is TRUE, "void f(PCF)" introduces a new function, which is
consistent with the Working Paper; when it is FALSE, "void f(PCF)" is a
compatible redeclaration of "void f(PF)" -- cfront's behavior.  (Note: when
this flag is FALSE, a strictly conforming implementation is not possible;
when it is TRUE, running in cfront compatibility mode is compromised -- but
only rarely as long as if impl_conv_between_c_and_cpp_function_ptrs_allowed
is TRUE.)  This is also an ABI issue, because it affects the representation
of pointer-to-function types in a mangled name.  When the flag is TRUE, the
name-mangling of "void f(PCF)" is distinct from that of "void f(PF)"; if it
is FALSE, the two are mangled identically.
*/
#ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 233 || ...) */
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 233 || ... */
#endif /* ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT */

/*
Flag that is TRUE if the runtime library uses namespaces.  This
causes the runtime library to define the library classes (e.g., type_info)
in the "std" namespace.  It is also used by the standard header files
for the same purpose.
*/
#ifndef RUNTIME_USES_NAMESPACES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 230 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define RUNTIME_USES_NAMESPACES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 230 || CFRONT_...) */
#define RUNTIME_USES_NAMESPACES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 230 || CFRONT_... */
#endif /* ifndef RUNTIME_USES_NAMESPACES */

/*
Flag that is TRUE if the runtime library defines class type_info in the
"std" namespace.  Usually this should be set to the same value as
RUNTIME_USES_NAMESPACES.  This flag is used to initialize global variable
type_info_in_namespace_std.
*/
#ifndef DEFAULT_TYPE_INFO_IN_NAMESPACE_STD
#define DEFAULT_TYPE_INFO_IN_NAMESPACE_STD RUNTIME_USES_NAMESPACES
#endif /* ifndef DEFAULT_TYPE_INFO_IN_NAMESPACE_STD */


#if RUNTIME_USES_NAMESPACES
/*
The name of the macro to be defined when the runtime uses namespaces.
This is only used when RUNTIME_USES_NAMESPACES is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES
#define MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES \
  "__EDG_RUNTIME_USES_NAMESPACES"
#endif /* ifndef MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES */

/*
The name of the macro to be defined when the runtime should implicitly
do a "using namespace std".  This is only used when RUNTIME_USES_NAMESPACES
is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD
#define MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD "__EDG_IMPLICIT_USING_STD"
#endif /* ifndef MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD */
#endif /* RUNTIME_USES_NAMESPACES */

/*
Flag that is TRUE if the runtime library and/or system header files
use typename.
*/
#ifndef RUNTIME_USES_TYPENAME
#define RUNTIME_USES_TYPENAME FALSE
#endif /* ifndef RUNTIME_USES_TYPENAME */

#if BACK_END_IS_C_GEN_BE
/*
Switch that is TRUE if the C-generating back end should generate code for
gcc (the GNU C compiler).
*/

#ifndef GCC_IS_C_GEN_BE_TARGET
#ifdef __GNUC__
#define GCC_IS_C_GEN_BE_TARGET TRUE
#else /* !defined(__GNUC__) */
#define GCC_IS_C_GEN_BE_TARGET FALSE
#endif /* ifdef __GNUC__ */
#endif /* ifndef GCC_IS_C_GEN_BE_TARGET */
#endif /* BACK_END_IS_C_GEN_BE */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/*
Switch that is TRUE if the C-generating or C++-generating back end should
generate code for MSVC++ (the Microsoft C/C++ compiler).  This is the
initial value of the global variable msvc_is_generated_code_target.
*/

#ifndef MSVC_IS_GENERATED_CODE_TARGET
#if EDG_WIN32
#define MSVC_IS_GENERATED_CODE_TARGET TRUE
#else /* !EDG_WIN32 */
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#endif /* EDG_WIN32 */
#endif /* ifndef MSVC_IS_GENERATED_CODE_TARGET */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_C_GEN_BE
/*
Switch that is TRUE if the C-generating back end should generate ANSI C
instead of K&R C.
*/
#ifndef C_GEN_BE_GENERATES_ANSI_C
#if GCC_IS_C_GEN_BE_TARGET || USING_ISO_C
#define C_GEN_BE_GENERATES_ANSI_C TRUE
#else /* !(GCC_IS_C_GEN_BE_TARGET || USING_ISO_C) */
#define C_GEN_BE_GENERATES_ANSI_C FALSE
#endif /* GCC_IS_C_GEN_BE_TARGET || USING_ISO_C */
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
Control whether "long double" is put out as "long double" or as "double"
in generated C code.
*/
#ifndef LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if C_GEN_BE_GENERATES_ANSI_C
/* Generating ANSI C.  If "long double" is used as the host floating point
   representation, put out "long double" in the generated C. */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C FALSE
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C TRUE
#endif /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
/* Generating K&R C. */
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C TRUE
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifndef LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */

#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if TARG_SIZEOF_DOUBLE != TARG_SIZEOF_LONG_DOUBLE
 #error -- TARG_SIZEOF_DOUBLE must equal TARG_SIZEOF_LONG_DOUBLE when \
           LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C is TRUE
#endif /* TARG_SIZEOF_DOUBLE != TARG_SIZEOF_LONG_DOUBLE */
#if TARG_ALIGNOF_DOUBLE != TARG_ALIGNOF_LONG_DOUBLE
 #error -- TARG_ALIGNOF_DOUBLE must equal TARG_ALIGNOF_LONG_DOUBLE when \
           LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C is TRUE
#endif /* TARG_ALIGNOF_DOUBLE != TARG_ALIGNOF_LONG_DOUBLE */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */

/*
Control whether a warning is put out when "long double" is put out
as "double."
*/
#ifndef ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
#define ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE FALSE
#endif /* ifndef ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */

/*
If ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C is TRUE, "register" will be put
out for register variables whose address is taken.  This can occur when
compiling ANSI C code in SVR4 C compatibility mode.
*/
#ifndef ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C
#define ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C FALSE
#endif /* !defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C) */

/*
If the C-generating back end is being used, are bit fields in the
generated C allowed to have base types other than the standard
"int" and "unsigned int"?  Note that the setting of this switch must
match TARG_BIT_FIELD_CONTAINER_SIZE so that the front end's layout
code gets the same result that the underlying C compiler will get.
*/
#ifndef ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
#if CFRONT_OBJECT_CODE_COMPATIBILITY || ABI_COMPATIBILITY_VERSION < 235
/* In the C code it generates, cfront changes the underlying types of all
   bit-fields to int or unsigned int. */
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C FALSE
#else /* !(CFRONT_OBJECT_CODE_COMPATIBILITY || ...) */
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C TRUE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY || ... */
#endif /* ifndef ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */

/*
If the C-generating back end is being used, are "?" operators allowed
to have operands of void type?  pcc, for example, does not allow them.
If they are not allowed, they are rewritten as "(operand, 0)".
*/
#ifndef ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
#if C_GEN_BE_GENERATES_ANSI_C
#define ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C TRUE
#else /* !C_GEN_BE_GENERATES_ANSI_C */
#define ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C FALSE
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifndef ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */

/*
If the C-generating back end is being used, and the target environment
has .init sections (e.g., SVR4), this flag is TRUE to enable generation of
asm directives to get startup routines called (thus eliminating the need
for patch or munch).  Note that gcc has a better of way of doing this,
so it's not necessarily helpful to set this to TRUE when using gcc as the
target C compiler.
*/
#ifndef USE_INIT_SECTION_IN_GENERATED_C
#define USE_INIT_SECTION_IN_GENERATED_C FALSE
#endif /* ifndef USE_INIT_SECTION_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */

/*
If ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C is TRUE, "(...)" will be put out
as the parameter list for a routine with no parameters and has_ellipsis
set to TRUE.  The setting of this switch is irrelevant in C mode
if the construct is not allowed in the source (see
allow_ellipsis_only_param_in_C_mode).  However, the source construct
is always allowed in C++ mode, and this switch also controls the form of
the generated C for that case.
*/
#ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C FALSE
#endif /* ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C */

/*
When generating C or C++ code, add extra braces around "if" statements
without an "else" to avoid the "dangling else" problem.  This is necessary
only if customer code modifies the IL statement tree.
*/
#ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
#define ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C FALSE
#endif /* ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */

/*
TRUE if #pragma ident should be generated instead of #ident when the
C-generating back (c_gen_be) or C++-generating back end (cp_gen_be) is
used.
*/
#ifndef USE_PRAGMA_IDENT_IN_GENERATED_CODE
#define USE_PRAGMA_IDENT_IN_GENERATED_CODE FALSE
#endif /* ifndef USE_PRAGMA_IDENT_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the "restrict" keyword should
be suppressed in the output.
*/
#ifndef SUPPRESS_RESTRICT_IN_GENERATED_CODE
#define SUPPRESS_RESTRICT_IN_GENERATED_CODE TRUE
#endif /* SUPPRESS_RESTRICT_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the "static" keyword in
array declarators (a C99 feature) should be suppressed in the output.
*/
#ifndef SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE
#define SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE TRUE
#endif /* SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the Microsoft qualifiers
should be suppressed in the output.  This flag is only applicable if
MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
*/
#ifndef SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
#define SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE FALSE
#endif /* SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, near and far should be
suppressed in the output.  This flag is only applicable if
NEAR_AND_FAR_ALLOWED is TRUE.
*/
#ifndef SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE
#define SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE FALSE
#endif /* SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE */

/*
Flag that is TRUE if the C++/C-generating back end should issue class member
using-declarations instead of access declarations.
*/
#ifndef USING_DECLARATIONS_IN_GENERATED_CODE
#define USING_DECLARATIONS_IN_GENERATED_CODE TRUE
#endif /* USING_DECLARATIONS_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C++/C-generating back end (cp_gen_be)
is run, "specializations" for generated template instances should use
the old syntax instead of the modern "template <>" prefix form.  This is
the initial value of old_specializations_for_generated_instances.
*/
#ifndef DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES
#define DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES FALSE
#endif /* DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, references to the
<stdarg.h> macros should be scanned specially and output in the
original form.  This avoids problems with language extensions
used to implement those.  This is the initial value of
pass_stdarg_references_to_generated_code.

Note that GUARD_MACRO_FOR_VA_LIST can be defined to be a quoted string
that is the name of a macro to be defined when the built-in va_list
is defined.  If it is not set, no macro is defined.  A second macro
can be specified with GUARD_MACRO2_FOR_VA_LIST.
*/
#ifndef DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE
#if BACK_END_IS_C_GEN_BE
#if C_GEN_BE_GENERATES_ANSI_C
/* The C-generating back end is being used, and is generating ANSI/ISO C. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE TRUE
#else /* !C_GEN_BE_GENERATES_ANSI_C */
/* The C-generating back end is being used, and is generating old-style C. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE FALSE
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#else /* !BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
/* The C++-generating back end is being used. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
/* Neither the C-generating back end nor the C++-generating back end is being
   used. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* ifndef DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE */

/*
Name of a global type which, if defined when <stdarg.h> is included,
indicates the type to be used for the built-in va_list.
*/
#ifndef BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME
#define BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME "__edg_va_list"
#endif /* ifndef BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME */

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
#if BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL TRUE
#else /* !(BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C) */
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL FALSE
#endif /* BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C */
#endif /* !defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL) */

/*
Default value for distinct_template_signatures.  Controls whether the
signatures for template functions can match those for non-template
functions across separate compilation units.  In the modern C++
language, a normal function cannot be used to satisfy the need for a
template instance.  For example, a function "void f(int)" could not be
used to satisfy the need for an instantiation of a template "void
f(T)" with T set to int.  In older versions of the language, the name
mangling for templates was the same as for nontemplates, and a
nontemplate function could satisfy the need for a template function.
Distinct template signatures must be enabled in order to use function
template parameters that are not part of the signature of the function
template.
*/
#ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES

/* This configuration flag was renamed.  If there is no definition for
   the new name, but there is one for the old name, use the value specified
   for the old name. */
#ifdef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES \
        DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES
#else /* ifndef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES */

/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 232 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 232 || ...) */
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 232 || ... */

#endif /* ifdef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES */
#endif /* ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES */

#if NEED_NAME_MANGLING
/*
Default value for compress_mangled_names, which controls whether compression
is done on mangled names.
*/
#ifndef DEFAULT_COMPRESS_MANGLED_NAMES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 241 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_COMPRESS_MANGLED_NAMES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 241 || ... ) */
#define DEFAULT_COMPRESS_MANGLED_NAMES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 241 || ... */
#endif /* ifndef DEFAULT_COMPRESS_MANGLED_NAMES */
#endif /* NEED_NAME_MANGLING */

#if NEED_NAME_MANGLING
/*
Default value for max_mangled_name_length, which controls the maximum
length of mangled names.  Zero means no limit.  Names longer than
the limit are truncated by addition of a CRC code.  That makes them shorter
but no longer decodable.
*/
#ifndef DEFAULT_MAX_MANGLED_NAME_LENGTH
#define DEFAULT_MAX_MANGLED_NAME_LENGTH 0 /* No limit. */
#endif /* ifndef DEFAULT_MAX_MANGLED_NAME_LENGTH */
#endif /* NEED_NAME_MANGLING */

/*
This switch controls whether or not the ABI changes for runtime
type information (RTTI) are done.  This affects element 0 of virtual
function tables, the BCS_PUBLIC and BCS_AMBIGUOUS flags in base class
arrays, and the user type_info and name fields in the typeinfo
implementation structure.  Because the name field must be initialized
in all cases, typeinfo variables for non-class types are always
initialized in the new scheme (in the old scheme, a tentative
definition with default initialization to zero was enough).
If the switch is off, compatibility with versions up to 2.28
is preserved, but the RTTI language features are turned off.
The typeinfo generated in that case is adequate for exception
handling but not for RTTI.
*/
#ifndef ABI_CHANGES_FOR_RTTI
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_RTTI FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228  && !CFRONT_... */
#define ABI_CHANGES_FOR_RTTI TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_RTTI */
#if ABI_CHANGES_FOR_RTTI && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- ABI_CHANGES_FOR_RTTI TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* ABI_CHANGES_FOR_RTTI && (ABI_COMPATIBILITY_VERSION <= 228) */

#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
/*
This switch controls whether the typeinfo variables for RTTI are generated
when RTTI is turned off.  Setting this switch to TRUE will reduce memory
use in applications that never use RTTI, but it also makes it possible to
end up with configuration mismatches and link errors or runtime aborts,
e.g., by compiling part of the program in one mode and part in another.
See also the variable generate_rtti_typeinfo.
*/
#ifndef SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED
#define SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED FALSE
#endif /* ifndef SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */

/*
This switch controls whether or not the ABI changes for array
new and delete are done.  New runtime routines are added, and the
way array sizes are recorded by the runtime is different.
The changes are upward-compatible (you can use old object code
with new object code and the new library).  If the switch is off,
compatibility with versions up to 2.28 is preserved, but the
array new and delete language features are turned off.
*/
#ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228 && !CFRONT_... */
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE && ... */

/*
This switch controls whether or not the ABI changes for placement
delete are done.  New runtime routines/variables are added.
The changes are upward-compatible (you can use old object code
with new object code and the new library).  If the switch is off,
compatibility with versions up to 2.33 is preserved, but the
placement delete language feature is turned off.  Allocating an
array with placement new and then using the delete operator on it
is also considered part of "placement delete" and is controlled by
this switch.
*/
#ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_PLACEMENT_DELETE FALSE /* Versions up to 2.33. */
#else /* ABI_COMPATIBILITY_VERSION > 233 && !CFRONT_... */
#define ABI_CHANGES_FOR_PLACEMENT_DELETE TRUE  /* Versions after 2.33. */
#endif /* ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE && (ABI_COMPATIBILITY_VERSION <= 233)
 #error -- ABI_CHANGES_FOR_PLACEMENT_DELETE TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 233
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE && ... */

/*
This switch controls whether or not ABI changes are made to support
covariant return types on overriding virtual functions.  If the switch is
off, compatibility with versions up to 2.33 is preserved, but support for
covariant return types on overriding virtual functions is disabled (meaning
errors will be issued when compiling programs using the feature).
*/
#ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN FALSE
                                                    /* Versions up to 2.33. */
#else /* ABI_COMPATIBILITY_VERSION > 233 && !CFRONT_... */
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN TRUE
                                                    /* Versions after 2.33. */
#endif /* ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
#if ABI_COMPATIBILITY_VERSION <= 233
 #error -- ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 233
#endif /* ABI_COMPATIBILITY_VERSION <= 233 */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

/*
This switch controls whether or not ABI changes are made to fix problems
with virtual function tables during construction of classes that have
virtual base classes.  The changes include adding a parameter to some
constructors and destructors and increasing the size of a field in
the region table for the portable implementation of EH.
*/
#ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 238 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS FALSE
                                                    /* Versions up to 2.38. */
#else /* ABI_COMPATIBILITY_VERSION > 238 && !CFRONT_... */
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS TRUE
                                                    /* Versions after 2.38. */
#endif /* ABI_COMPATIBILITY_VERSION <= 238 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#if ABI_COMPATIBILITY_VERSION <= 238
 #error -- ABI_CHANGES_FOR_CONSTRUCTION_VTBLS TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 238
#endif /* ABI_COMPATIBILITY_VERSION <= 238 */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

/*
Flag that is TRUE if the definition of extern inline functions
should be controlled by the template instantiation mechanism.

When this flag is set, only one out-of-line copy of an extern inline
function is generated.  This is more standard conforming as it
ensures that the address of an inline function remains constant
across translation units.  The disadvantage is that it requires that
the template instantiation mechanism be employed for inline functions.
Extern inline functions cannot be lowered when this flag is set.

When this flag is FALSE, multiple copies of extern inline functions
are generated.  Note that if the function has local static variables,
multiple copies of the variables are *not* generated.
*/
#ifndef INSTANTIATE_EXTERN_INLINE
#define INSTANTIATE_EXTERN_INLINE FALSE
#endif /* ifndef INSTANTIATE_EXTERN_INLINE */

/*
This switch controls whether "extern inline" functions are rewritten as
normal inline functions.  The transformation involves promoting local static
variables to external, and rewriting references to the address of an
extern inline function to use a global variable containing the address
of the chosen copy.
*/
#ifndef LOWER_EXTERN_INLINE
#if INSTANTIATE_EXTERN_INLINE
#define LOWER_EXTERN_INLINE FALSE /* Do not change this. */
#else /* !INSTANTIATE_EXTERN_INLINE */
#define LOWER_EXTERN_INLINE TRUE /* You can change this. */
#endif /* INSTANTIATE_EXTERN_INLINE */
#endif /* ifndef LOWER_EXTERN_INLINE */

/*
This switch controls whether the unary plus operator is generated in the
IL.  When it is FALSE, +expr will be rendered simply as expr.  Note that
the eok_unary_plus operator is used in prototype instantiations even when
this switch is FALSE, but such code would not ordinarily be passed to
a code generator.
*/
#ifndef UNARY_PLUS_IN_IL
#if BACK_END_IS_CP_GEN_BE
#define UNARY_PLUS_IN_IL TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
#define UNARY_PLUS_IN_IL FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* ifndef UNARY_PLUS_IN_IL */

#if LOWER_EXTERN_INLINE && INSTANTIATE_EXTERN_INLINE
 #error -- extern inline functions cannot be instantiated when they are lowered
#endif /* !LOWER_EXTERN_INLINE && INSTANTIATE_EXTERN_INLINE */

/*
Flag that is TRUE if variable length arrays (VLAs) are allowed.  A VLA is
an array whose size is known only at execution time.  This is supported in
C mode only.  If VLA_ALLOWED is TRUE, support is enabled and disabled based
on command-line options --[no_]vla, which control global variable vla_enabled.
When VLAs are allowed, they are enabled by default in C99 mode.
*/
#ifndef VLA_ALLOWED
#if C99_IL_EXTENSIONS_SUPPORTED
#define VLA_ALLOWED TRUE
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define VLA_ALLOWED FALSE
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef VLA_ALLOWED */

/*
Flag that is TRUE if, when VLA support is enabled, the front end should
generate stmk_vla_dealloc statements to mark the points at which variable
length arrays go out of scope and may be deallocated.  It is used to set
global variable vla_dealloc_statements_in_il.
*/
#ifndef VLA_DEALLOC_STATEMENTS_IN_IL
#if VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE
#define VLA_DEALLOC_STATEMENTS_IN_IL TRUE
#else /* !(VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE) */
#define VLA_DEALLOC_STATEMENTS_IN_IL FALSE
#endif /* VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE */
#endif /* ifndef VLA_DEALLOC_STATEMENTS_IN_IL */
#if VLA_DEALLOC_STATEMENTS_IN_IL && !VLA_ALLOWED
  #error -- VLA_DEALLOC_STATEMENTS_IN_IL cannot be true unless \
            VLA_ALLOWED is true
#endif /* VLA_DEALLOC_STATEMENTS_IN_IL && !VLA_ALLOWED */
#if VLA_DEALLOC_STATEMENTS_IN_IL && BACK_END_IS_CP_GEN_BE
  #error -- VLA_DEALLOC_STATEMENTS_IN_IL cannot be true when \
            BACK_END_IS_CP_GEN_BE is true
#endif /* VLA_DEALLOC_STATEMENTS_IN_IL && BACK_END_IS_CP_GEN_BE */

/*
Flag that is used as the default setting for global variable vla_enabled.
The variable can also been controlled from the command line by --[no_]vla.
(Whatever the default, vla_enabled is always turned off in C++ mode, and
on in C99 mode, so the default here applies only in only other modes.)
*/
#ifndef DEFAULT_VLA_ENABLED
#define DEFAULT_VLA_ENABLED FALSE
#endif /* ifndef DEFAULT_VLA_ENABLED */
#if DEFAULT_VLA_ENABLED && !VLA_ALLOWED
  #error -- DEFAULT_VLA_ENABLED cannot be true unless VLA_ALLOWED is true
#endif /* DEFAULT_VLA_ENABLED && !VLA_ALLOWED */

/*
Flag that is TRUE if designators of the form 'x:' and '[expr ... expr]'
should be accepted in aggregate initializers.  This also makes the '='
following an array element designation optional.  It should not be TRUE
if DEFAULT_DESIGNATORS_ALLOWED is FALSE.  It is the initial value
of the global variable extended_designators_allowed.
*/
#ifndef DEFAULT_EXTENDED_DESIGNATORS_ALLOWED
#define DEFAULT_EXTENDED_DESIGNATORS_ALLOWED FALSE
#endif /* DEFAULT_EXTENDED_DESIGNATORS_ALLOWED */

/*
Flag that is TRUE if designators of the form '.x' and '[expr]' should be
accepted in aggregate initializers.  It is the initial value of the global
variable designators_allowed.
*/
#ifndef DEFAULT_DESIGNATORS_ALLOWED
#define DEFAULT_DESIGNATORS_ALLOWED FALSE
#endif /* DEFAULT_DESIGNATORS_ALLOWED */

/*
Flag that is TRUE if support for designated initializers and extended
designated initializers can be enabled.  Having this TRUE means the back
end is prepared to accept designated initializers, either in the
unlowered form or the lowered form (see LOWER_DESIGNATED_INITIALIZERS).
The C-generating and C++-generating back ends can handle designated
initializers (but that's useful only if the downstream compiler also
handles them).
*/
#ifndef DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
#if C99_IL_EXTENSIONS_SUPPORTED
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE TRUE
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define DESIGNATED_INITIALIZER_ENABLING_POSSIBLE FALSE
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#if !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE && DEFAULT_DESIGNATORS_ALLOWED
 #error -- designated initializer enabling not allowed
#endif /* !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE && ... */
/*
Flag that is TRUE if compound literals, which look vaguely like a cast
whose source expression is a brace-enclosed initializer (e.g.,
(int []){1, 2, 3}) should be accepted in expressions.  It is the
initial value of the global variable compound_literals_allowed.
*/
#ifndef DEFAULT_COMPOUND_LITERALS_ALLOWED
#define DEFAULT_COMPOUND_LITERALS_ALLOWED FALSE
#endif /* DEFAULT_COMPOUND_LITERALS_ALLOWED */

/*
This switch controls whether support for compound literals (a C99 feature)
can be enabled.  Having this TRUE means the back end is prepared to
accept compound literals, which are represented as enk_temp_init nodes.
The C-generating and C++-generating back ends can handle compound literals
(but that's useful only if the downstream compiler also handles them).
*/
#ifndef COMPOUND_LITERAL_ENABLING_POSSIBLE
#if C99_IL_EXTENSIONS_SUPPORTED
#define COMPOUND_LITERAL_ENABLING_POSSIBLE TRUE
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define COMPOUND_LITERAL_ENABLING_POSSIBLE FALSE
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef COMPOUND_LITERAL_ENABLING_POSSIBLE */
#if !COMPOUND_LITERAL_ENABLING_POSSIBLE && DEFAULT_COMPOUND_LITERALS_ALLOWED
 #error -- compound literal enabling not allowed
#endif /* !COMPOUND_LITERAL_ENABLING_POSSIBLE && ... */

#if DO_IL_LOWERING

/* Switches that control aspects of IL lowering: */

/*
This switch controls whether zeroing is added to variable definitions
to force them to be definitions in C.  This is generally a good thing,
but it may be wasteful for embedded system cross-compilers.
*/
#ifndef FORCE_VARIABLE_DEFINITION_VIA_ZEROING
#define FORCE_VARIABLE_DEFINITION_VIA_ZEROING TRUE
#endif /* ifndef FORCE_VARIABLE_DEFINITION_VIA_ZEROING */

/*
This switch controls whether or not types and static variables that
are local to function and block scopes are moved onto the file scope
lists.  When the switch is FALSE, no promotions are done.  Local
types and variables are allocated in the file scope memory region,
and they are linked on the local scope types or variables list.  That
accurately reflects the source form, which is desirable for
generating symbolic debug information.  That form probably works fine
when the IL is being fed into a true back end, but will not work when
the IL is being turned into C output (as with the C-generating back
end), because the local types and variables will not be visible from
member functions of local classes and in a file-scope termination
routine when it deals with calling a destructor for a local static
variable.  When the switch here is TRUE, the local types and
variables will be (selectively) promoted to the actual file scope.
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
#ifndef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
#if !GENERATE_EH_TABLES
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED TRUE
#else /* GENERATE_EH_TABLES */
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED FALSE
#endif /* !GENERATE_EH_TABLES */
#endif /* ifndef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#if !GENERATE_EH_TABLES
#if !KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
 #error -- KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED must be \
           TRUE when GENERATE_EH_TABLES is FALSE
#endif /* !KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#endif /* !GENERATE_EH_TABLES */

/*
When this switch is TRUE, instructions to set the cleanup state will
be emitted even at unreachable ends of blocks.  This may be desirable
if the back end is using the cleanup state instructions to build a
table, rather than leaving them as some kind of executable code.
*/
#ifndef INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE
#if DO_FULL_PORTABLE_EH_LOWERING
#define INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE FALSE
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#define INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE TRUE
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifndef INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE */

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
This switch controls generation of code at the end of fully-lowered
try blocks to fool C compilers into suppressing certain harmful optimizations.
Specifically, the code generated is an unreachable call of a runtime
routine, passing the addresses of all local variables modified within
the try block, to force the C compiler to store those immediately
when they are modified.
*/
#if DO_FULL_PORTABLE_EH_LOWERING
#ifndef FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
#define FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS BACK_END_IS_C_GEN_BE
#endif /* ifndef FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

/*
This switch can be set to enable the rewriting of the escape character
in universal character names (UCNs), i.e., "\", where it appears in
identifier names, to a single other character.  Note that this is not
a complete solution if the character to which the escape is changed
is a character that is valid in identifiers, because it would then
be possible (if unlikely) that a user might write an identifier that
would match an identifier with a rewritten UCN escape character (for
example, with a rewrite to "_", "x\u00d6" would be rewritten as
"x_u00d6").  A better choice is a character accepted by the linker
but not valid as an identifier character in C.  If there is no
such character, a character like "_" will provide a "good enough"
implementation.
*/
#ifndef REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
#define REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING TRUE
#ifndef UCN_ESCAPE_REWRITE_CHAR
#define UCN_ESCAPE_REWRITE_CHAR '_' /* Incomplete solution, see above. */
#endif /* ifndef UCN_ESCAPE_REWRITE_CHAR */
#endif /* ifndef REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */

/*
Integer kind to use for an offset into a class.  This is used for delta
fields in pointers to member functions, etc., but not for pointers to
data members.  If you change this, you will need to change
TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION, and possibly the related alignment
macro as well.
*/
#ifndef TARG_DELTA_INT_KIND
#define TARG_DELTA_INT_KIND ((an_integer_kind)ik_short)
#endif /* ifndef TARG_DELTA_INT_KIND */

/*
Integer kind to use for an index into a virtual function table.  Must be
no smaller than the size of a_virtual_function_number.  If you change
this, you will need to change TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION, and
possibly the related alignment macro as well.
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

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Microsoft mode allows a nonconstant aggregate initializer in C mode.
This switch controls whether such an aggregate is lowered to normal C.
That is done by invoking some subroutines from IL lowering, not the
whole process.
*/
#ifndef LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
#define LOWER_MICROSOFT_NONCONSTANT_AGGREGATE TRUE
#endif /* ifndef LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
This switch controls whether complex and imaginary types and operations
(a C99 feature) are lowered to C89 form.  The lowered form uses
calls to runtime routines to implement complex operations and conversions.
*/
#ifndef LOWER_COMPLEX
#if DO_C99_IL_LOWERING
#define LOWER_COMPLEX TRUE
#else /* !DO_C99_IL_LOWERING */
#define LOWER_COMPLEX FALSE
#endif /* DO_C99_IL_LOWERING */
#endif /* ifndef LOWER_COMPLEX */
#if LOWER_COMPLEX && !DO_C99_IL_LOWERING
 #error -- Complex cannot be lowered without doing C99 IL lowering
#endif /* LOWER_COMPLEX && !DO_C99_IL_LOWERING */

/*
This switch controls whether designated initializers (a C99 feature)
are lowered to standard C.  Well, almost standard C: a designated
initializer allows initialization of a member other than the first in
a union.  For that case, a ck_designator constant is left in the IL tree
(but only one, and only for members other than the first).  Also,
for extended designators of the form "[a ... b] = x", or when elements
of arrays are skipped in initialization, ck_init_repeat constants
are used to repeat an initializer constant the appropriate number
of times.
*/
#ifndef LOWER_DESIGNATED_INITIALIZERS
#define LOWER_DESIGNATED_INITIALIZERS TRUE
#endif /* ifndef LOWER_DESIGNATED_INITIALIZERS */

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

/*
Determine whether RTTI can be enabled.  It cannot be if we are doing IL
lowering and the ABI changes for RTTI aren't enabled.
*/
#ifndef RTTI_ENABLING_POSSIBLE
#if DO_IL_LOWERING
#if ABI_CHANGES_FOR_RTTI
#define RTTI_ENABLING_POSSIBLE TRUE
#else /* !ABI_CHANGES_FOR_RTTI */
#define RTTI_ENABLING_POSSIBLE FALSE
#endif /* ABI_CHANGES_FOR_RTTI */
#else /* !DO_IL_LOWERING */
#define RTTI_ENABLING_POSSIBLE TRUE
#endif /* DO_IL_LOWERING */
#else /* ifdef RTTI_ENABLING_POSSIBLE */
#if RTTI_ENABLING_POSSIBLE && DO_IL_LOWERING
#if !ABI_CHANGES_FOR_RTTI
  #error -- ABI_CHANGES_FOR_RTTI must be TRUE when RTTI_ENABLING_POSSIBLE \
            is TRUE
#endif /* !ABI_CHANGES_FOR_RTTI */
#endif /* RTTI_ENABLING_POSSIBLE && DO_IL_LOWERING */
#endif /* ifndef RTTI_ENABLING_POSSIBLE */

/*
Determine whether array new and delete can be enabled.  They cannot be if
we are doing IL lowering and the ABI changes for array new and delete
aren't enabled.
*/
#ifndef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
#if DO_IL_LOWERING
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE TRUE
#else /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE FALSE
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#else /* !DO_IL_LOWERING */
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE TRUE
#endif /* DO_IL_LOWERING */
#else /* ifdef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE && DO_IL_LOWERING
#if !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
  #error -- ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE must be TRUE when \
            ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE is TRUE
#endif /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE && DO_IL_LOWERING */
#endif /* ifndef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */

#endif /* !defined(TARG_DEF_H) */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
