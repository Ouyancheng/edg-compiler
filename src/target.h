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

target.h -- Definition of target machine characteristics.
            See <limits.h> and standard, sec. 2.2.4.2, for related information.

*/

/* Avoid including these declarations more than once: */
#ifndef TARGET_H
#define TARGET_H 1

/*
Target byte order.  Little-endian means the least-significant part of a
multi-byte integer is at the lowest memory address.
*/
#define TARG_LITTLE_ENDIAN FALSE

/*
Char types:
*/
#define TARG_CHAR_BIT 8
#define TARG_SCHAR_MIN (-128)
#define TARG_SCHAR_MAX 127
#define TARG_UCHAR_MAX ((unsigned)255)
/* Make the default for character signedness on the target the same as
   for the host.  That's not required; it's just the most common case,
   and doing it this way makes it less likely that this configuration
   will be done wrong. */
#if CHAR_MIN == 0
#define DEFAULT_TARG_HAS_SIGNED_CHARS FALSE
#else /* CHAR_MIN != 0 */
#define DEFAULT_TARG_HAS_SIGNED_CHARS TRUE
#endif /* CHAR_MIN == 0 */
			/* Default setting for targ_has_signed_chars. */

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
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
			/* if 1, 'ab' == 0x6162. */
			/* if 0, 'ab' == 0x6261. */

/*
Wide character constant type (wchar_t, see stddef.h and stdlib.h):
(These are allowed to be nonconstant, e.g., plain_char_int_kind for
TARG_WCHAR_T_INT_KIND.)
*/
#define TARG_WCHAR_T_INT_KIND ik_unsigned_short
#define TARG_SIZEOF_WCHAR_T TARG_SIZEOF_SHORT

/*
Integer types:
*/
#define TARG_SHRT_MAX ((short)0x7fff)
#define TARG_SHRT_MIN ((short)0x8000)
#define TARG_USHRT_MAX ((unsigned short)0xffff)
#define TARG_INT_MAX ((long)0x7fffffffL)
#define TARG_INT_MIN ((long)0x80000000L)
#define TARG_UINT_MAX ((unsigned long)0xffffffffL)
#define TARG_LONG_MAX ((long)0x7fffffffL)
#define TARG_LONG_MIN ((long)0x80000000L)
#define TARG_ULONG_MAX ((unsigned long)0xffffffffL)

/* Remember that the size of a type must be a multiple of the alignment. */
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4

/*
If this flag is TRUE, overflows on signed integer operations do
not cause errors (only warnings).  Usually this would be set to
match the target machine behavior on integer operations in C.
*/
#define TARG_NO_ERROR_ON_INTEGER_OVERFLOW TRUE

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
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL TRUE

/* Maximum size of a bit-field.  Must not be larger than the size of a
   long. */
#define TARG_MAX_BIT_FIELD_SIZE (TARG_SIZEOF_INT*TARG_CHAR_BIT)
#if TARG_MAX_BIT_FIELD_SIZE > (TARG_SIZEOF_LONG*TARG_CHAR_BIT)
error -- TARG_MAX_BIT_FIELD_SIZE may not be bigger than the size of a long
#endif /* TARG_MAX_BIT_FIELD_SIZE ... */
/* Second check required for definition of a_field (see il_def.h).  We add
   1 to TARG_MAX_BIT_FIELD_SIZE for a front end use (see class_decl.c). */
#if BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE+1
error -- TARG_MAX_BIT_FIELD_SIZE is too big.
#endif /* BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE */

/* Container size to be used for bit-fields.  If > 0, indicates the
   size in bytes of one of the integral types.  0 means "use the smallest
   integral type into which the field will fit".  < 0 means "use the
   base type given in the declaration". */
#define TARG_BIT_FIELD_CONTAINER_SIZE 0
/* How plain "int" bit fields are to be treated (signed or unsigned).
   Note that 1-bit fields are made unsigned regardless of this switch. */
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED (!DEFAULT_TARG_HAS_SIGNED_CHARS)

/*
Pointer types:
*/
#define TARG_ALL_POINTERS_SAME_SIZE TRUE
			/* At the moment, this cannot be set FALSE.
			   See set_type_size in types.c. */
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
/* Integer type for the difference of two pointer types (ptrdiff_t).
   This type must be signed.  See 3.3.6 in the standard and the header
   file <stddef.h>. */
typedef long a_targ_ptrdiff_t;  /* Must be host "long". */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_PTRDIFF_T_MAX TARG_INT_MAX
#define TARG_PTRDIFF_T_MIN TARG_INT_MIN
#define TARG_PTRDIFF_T_INT_KIND ik_int
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_PTRDIFF_T_MAX TARG_LONG_MAX
#define TARG_PTRDIFF_T_MIN TARG_LONG_MIN
#define TARG_PTRDIFF_T_INT_KIND ik_long
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */

/* size_t, used for size of arrays, offsets in fields, type of sizeof, etc.
   This type must be unsigned.  See 3.3.3.4 in the standard and the header
   file <stddef.h>. */
typedef unsigned long a_targ_size_t;  /* Must be host "unsigned long". */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_SIZE_T_MAX TARG_UINT_MAX
#define TARG_SIZE_T_INT_KIND ik_unsigned_int
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_SIZE_T_MAX TARG_ULONG_MAX
#define TARG_SIZE_T_INT_KIND ik_unsigned_long
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */

/* Specification of a target alignment requirement.  1 means no alignment
   requirement. */
typedef a_byte a_targ_alignment;

/*
Float types:
*/
/* Remember that the size of a type must be a multiple of the alignment. */
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 8
#define TARG_ALIGNOF_LONG_DOUBLE 8

/*
Type used to represent float quantities internally:

(This should not be an array, so that it's always known whether one gets the
address or the value of the item.)
*/
typedef struct an_internal_float_value *an_internal_float_value_ptr;
typedef struct an_internal_float_value {
  a_byte bytes[TARG_SIZEOF_LONG_DOUBLE];
} an_internal_float_value;

/*
C++ pointer-to-member type.
(The formulas here are for a typical implementation, but are not required.)
*/
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_SHORT
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_SHORT
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
  (2*TARG_SIZEOF_SHORT+TARG_SIZEOF_POINTER)
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_POINTER

/* 
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions the front end allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by the following.
*/
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO TARG_SIZEOF_POINTER
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO TARG_ALIGNOF_POINTER

/*
Enumerated types:
*/
#define DEFAULT_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT TRUE
			/* Default setting for
			   enum_types_can_be_smaller_than_int.  If TRUE,
			   enumerated types can be allocated in integral
			   types smaller than int. */

/*
Definition of shift operations:
*/
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
			/* Right shift on a signed quantity does sign
			   extension. */

/*
Number of significant characters in an external name (names will be truncated
to this length if necessary).  If there is no limit, this value should be set
to zero.  It should in any case be set to a reasonably small value (not, for
instance, to the maximum integer size) since an array of this many
characters may be allocated (see find_external_symbol in symbol_tbl.c).
*/
#define TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME 0

/*
Flag that is TRUE if external names are case sensitive (in which case, e.g.,
the object language would distinguish routines XXX and xxx).
*/
#define TARG_CASE_SENSITIVE_EXTERNAL_NAMES TRUE

/*
Flag that is TRUE if class and struct fields are allocated in the same order
as they are declared, regardless of access specification.  When it is FALSE,
fields are grouped by access (private first, followed by protected and then
private) and offsets are assigned within each group in declaration order.
(Constraints on allocation are discussed in ARM 9.2 and 11.1.  In particular,
both approaches described in the embedded annotation in section 11.1 are
supported.)
*/
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE TRUE

/*
Flag that is TRUE if assignment to "this" (a C++ anachronism) should
be allowed.  This affects the source language accepted.  If assignment
to "this" is allowed, the interface to and wrapper code within constructors
and destructors may have to be changed.
*/
#define ASSIGNMENT_TO_THIS_ALLOWED TRUE

/*
Flag that is TRUE if the class layout scheme used by AT&T's cfront should
be duplicated.  The main issue is how the data sections for virtual base
classes are put out.  The default behavior (when this flag is FALSE)
produces a more efficient use of space.
*/
#define CFRONT_CLASS_LAYOUT_COMPATIBILITY TRUE

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
