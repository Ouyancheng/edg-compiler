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

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */

/*
Flag that is TRUE if object code compatibility with AT&T's cfront is
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
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY TRUE
#define CFRONT_3_0_OBJECT_CODE_COMPATIBILITY FALSE
#define CFRONT_OBJECT_CODE_COMPATIBILITY \
                           (CFRONT_2_1_OBJECT_CODE_COMPATIBILITY ||   \
                            CFRONT_3_0_OBJECT_CODE_COMPATIBILITY)

#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
??=error -- must select either 2.1 compatibility or 3.0 compatibility
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY ... */

/* The code that implements the cfront name lookup bug makes use of the
   information recorded for semivisible nested type handling.  Consequently,
   2.1 compatibility mode is required to use the name lookup bug. */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
??=error -- cfront name lookup bug support requires cfront 2.1 compatibility
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/*
Target byte order.  Little-endian means the least-significant part of a
multi-byte integer is at the lowest memory address.
*/
#define TARG_LITTLE_ENDIAN FALSE

/*
Char types:
*/
#define TARG_CHAR_BIT 8
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
/* Remember that the size of a type must be a multiple of the alignment. */
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#if LONG_LONG_ALLOWED
#define TARG_SIZEOF_LONG_LONG 8
#define TARG_ALIGNOF_LONG_LONG 8
#endif /* LONG_LONG_ALLOWED */

#if LONG_LONG_ALLOWED
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG_LONG
#else /* !LONG_LONG_ALLOWED */
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG
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
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER FALSE
#endif /*INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

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
#endif /* AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG */

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
#endif /* AN_INTEGER_VALUE_IS_LARGER_THAN_HOST_LONG */

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

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
   long (or a long long, if they are allowed). */
#define TARG_MAX_BIT_FIELD_SIZE (TARG_SIZEOF_INT*TARG_CHAR_BIT)
/* Check the value: */
#if TARG_MAX_BIT_FIELD_SIZE > (TARG_SIZEOF_LARGEST_INTEGER*TARG_CHAR_BIT)
??=error -- TARG_MAX_BIT_FIELD_SIZE is too big
#endif /* TARG_MAX_BIT_FIELD_SIZE ... */
/* Second check required for definition of a_field (see il_def.h).  We add
   1 to TARG_MAX_BIT_FIELD_SIZE for a front end use (see layout.c). */
#if BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE+1
??=error -- TARG_MAX_BIT_FIELD_SIZE is too big.
#endif /* BYTE_MAX < TARG_MAX_BIT_FIELD_SIZE */

/* Container size to be used for bit-fields.  If > 0, indicates the
   size in bytes of one of the integral types.  0 means "use the smallest
   integral type into which the field will fit".  < 0 means "use the
   base type given in the declaration". */
#define TARG_BIT_FIELD_CONTAINER_SIZE 0
/* How plain "int" bit fields are to be treated (signed or unsigned).
   Note that 1-bit fields are made unsigned regardless of this switch. */
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED (!DEFAULT_TARG_HAS_SIGNED_CHARS)
/* Signedness for enum bit fields (an extension): if TRUE, enum bit fields
   are always unsigned.  If FALSE, the rules are: (a) if the enum contains
   any negative values, the field is signed; (b) if the enum contains
   values large enough that they won't fit if one bit is allocated for a
   sign, the field is unsigned; otherwise (c) the signedness is as
   indicated by TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED. */
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED TRUE
/* Alignment adjustment to be made when a zero-width (unnamed) bit field is
   declared.  If > 0 it is the alignment to be used (typically the alignment
   of one of the integral types).  A value of zero means "use the minimal
   alignment", which is single-byte alignment.  Any value less than zero
   means "use the alignment of the base type given in the declaration". */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT TARG_ALIGNOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 0
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

/*
Pointer types:
*/
#define TARG_ALL_POINTERS_SAME_SIZE TRUE
			/* At the moment, this cannot be set FALSE.
			   See set_type_size in types.c. */
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
/* Indication of whether NULL pointer is like integer zero. */
#define TARG_NULL_IS_ALL_BITS_ZERO TRUE
/* Integer type for the difference of two pointer types (ptrdiff_t).
   This type must be signed.  See 3.3.6 in the standard and the header
   file <stddef.h>. */
typedef long a_targ_ptrdiff_t;  /* Must be host "long". */
/* TARG_PTRDIFF_T_MAX and TARG_PTRDIFF_T_MIN define the limits of the host
   representation of ptrdiff_t constants; the range they define can be equal
   to or smaller than the integer size implied by TARG_PTRDIFF_T_INT_KIND.
   Except when the target ptrdiff_t is smaller than the host long, they
   should be LONG_MAX and LONG_MIN. */
#define TARG_PTRDIFF_T_MAX ((a_targ_ptrdiff_t)LONG_MAX)
#define TARG_PTRDIFF_T_MIN ((a_targ_ptrdiff_t)LONG_MIN)
/* Pick a typical representation for ptrdiff_t: the smaller of int or long
   that can hold a pointer value. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_PTRDIFF_T_INT_KIND ik_int
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_PTRDIFF_T_INT_KIND ik_long
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */

/* size_t, used for size of arrays, offsets in fields, type of sizeof, etc.
   This type must be unsigned.  See 3.3.3.4 in the standard and the header
   file <stddef.h>. */
typedef unsigned long a_targ_size_t;  /* Must be host "unsigned long". */
/* TARG_SIZE_T_MAX defines the limit of the host representation
   of size_t constants; the range it defines can be equal to or smaller
   than the integer size implied by TARG_SIZE_T_INT_KIND.  Except when
   the target size_t is smaller than the host long, it should be ULONG_MAX. */
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
/* Pick a typical representation for size_t: the smaller of unsigned int or
   unsigned long that can hold a pointer value. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_SIZE_T_INT_KIND ik_unsigned_int
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
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
  /* The type here must match the code in float_pt.c.  The default
     declaration assumes that target floating constants are represented in
     a host double, which is what the default float_pt.c does. */
  a_byte bytes[sizeof(double)];
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
Numbering for virtual functions.  Each virtual member function in a given
class is assigned a unique number which can (for instance) be used to
define a virtual function table index value.
*/
typedef unsigned short a_virtual_function_number;
#define MAX_VIRTUAL_FUNCTIONS_PER_CLASS USHRT_MAX

/*
Control over whether or not C++ "new" and "delete" operations are allowed
to be folded into the constructor or destructor if possible.
*/
/* IL lowering requires that the delete be folded into the destructor.
   Otherwise the size is not available for the two-argument delete case.
   There is a consistency check in lower_il.c */
#define NEW_CAN_BE_FOLDED_INTO_CTOR TRUE
#define DELETE_CAN_BE_FOLDED_INTO_DTOR TRUE
/* If assignment to "this" is allowed, the folding must be done. */
#if ASSIGNMENT_TO_THIS_ALLOWED && !NEW_CAN_BE_FOLDED_INTO_CTOR
??=error -- NEW_CAN_BE_FOLDED_INTO_CTOR set wrong.
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */
#if ASSIGNMENT_TO_THIS_ALLOWED && !DELETE_CAN_BE_FOLDED_INTO_DTOR
??=error -- DELETE_CAN_BE_FOLDED_INTO_DTOR set wrong.
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */

/*
Enumerated types:
*/
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE /* Do not change */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define DEFAULT_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
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
The minimum alignment required for class/struct/union objects in the target
environment.  If C code is being generated, this may be dictated by the
characteristics of the C compiler that will be used for subsequent
processing.
*/
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1


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
classes and in the file-scope termination routine when it deals with
calling destructors for local static variables.  When the switch here
is TRUE, the local types and variables will be (selectively) promoted
to the actual file scope.
*/
#define PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE BACK_END_IS_C_GEN_BE

/*
This switch controls whether or not all functions and function calls will
be turned into old-style unprototyped form.  This is what cfront effectively
does, in generating old-style C that is compiled by a C compiler.
This change is important if one wants to be able to call libraries that
were compiled by cfront.  (cfront's +a1 option requests generation of ANSI C
code; if one wants compatibility with cfront in that mode, this option
should be set to FALSE.)
*/
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED CFRONT_OBJECT_CODE_COMPATIBILITY

/*
Integer kind to use for an offset into a class.  Its size must match
TARG_SIZEOF_PTR_TO_DATA_MEMBER.
*/
#define TARG_DELTA_INT_KIND ((an_integer_kind)ik_short)

/*
Integer kind to use for an index into a virtual function table.  Must be
no smaller than the size of a_virtual_function_number.
*/
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND ((an_integer_kind)ik_short)

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
#endif /* ifndef LOWER_LVALUE_RETURNING_OPERATIONS */

/*
This switch controls whether or not "guard" code is placed around
initializations of static data members of templates.  Such guard code is
necessary if template instantiation resolution is done by instantiating
everything and then having the (specially-modified) linker discard
duplicate copies of instantiated routines.  Static data members are
a particular problem: because the initialization/destruction code is
generated in startup/termination routines, and is undifferentiated
from other code in those routines, a flag is needed to indicate that
initialization or destruction has already been done.  After any one
instance of the code does initialization or destruction, all other
instances will do nothing.
*/
#ifndef TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
#define TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE FALSE
#endif /* TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE */

#endif /* DO_IL_LOWERING */


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
