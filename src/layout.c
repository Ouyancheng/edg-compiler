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

layout.c -- Managing the layout of class objects, based on user
            declarations and implementation conventions.

General comments on class layout

I.  Unions

  The offset of each nonstatic data member of a union is zero bytes from
  the starting address of the union object.  Its size is large enough to
  contain the largest member, and its alignment is that of the member with
  the greatest alignment requirement.  Unions cannot have base classes so
  no space is reserved for them (nor for pointers to data sections of
  virtual base classes).  Unions cannot have virtual functions, so no
  space is reserved for a "virtual function info" field.

II.  Classes and structs

A.  C vs. C++

  C structs are a special case of C++ structs.  They are classes without
  base classes and virtual functions, so space is required for fields
  only.  Fields are put out in strict declaration order and aligned based
  on data type.  The alignment requirement for the struct as a whole is that
  of the member with the greatest alignment requirement.

  C++ classes (in the generic sense) that are not unions have more
  complicated layouts.  The language definition provides few requirements
  on how a class is laid out, so there are potential incompatibilities
  between different language processors in (1) the order and locations of
  nonstatic data members, base class data sections, and pointers to
  virtual base classes, and (2) the representation of virtual function
  information.

B.  Layout options

  The EDG C++ front end provides several options in laying out a class,
  including compatibility with the layout of AT&T's cfront and a "normal"
  option that uses memory more efficiently than cfront's layout.  There is
  also an option that controls whether fields are put out in strict
  declaration order or grouped by accessibility.  The front end can be
  configured as required by setting CFRONT_OBJECT_CODE_COMPATIBILITY and
  TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE, both of which are
  defined in target.h.

  The normal layout is as follows:

    1. Nonvirtual direct base class data sections, in declaration order.
       (Such base class data sections are referred to as "incomplete
       subobjects" because the space reserved for them does not include
       space for their own virtual base classes.  All virtual base classes,
       direct and indirect, appear at the end of layout.)

    2. Nonstatic data members, in declaration order.

    3. The virtual function information "block"; in a typical
       implementation this will be space reserved for a pointer to a
       virtual function table. (An optimization allows the sharing of
       virtual function tables between a derived class and a nonvirtual
       base class, in which case this pointer will be omitted.)

    4. Pointers to direct and some indirect virtual base class subobjects.
       (A pointer-sharing optimization results in the omission of pointers
       to indirect virtual base classes in most cases.)

    5. The data sections for all direct and indirect virtual base classes.
       These appear as incomplete subobjects, in an order based on
       declaration order (namely, the order of their appearance on the base
       classes list, which is the same as the order of their construction:
       depth-first left-to-right traversal of the directed acyclic graph of
       the base classes).

  When TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE is FALSE, only item
  2 of the normal layout is modified.  The order of nonstatic data
  members becomes:  all public members, in declaration order; then, all
  protected members, in declaration order; and lastly, all private members,
  in declaration order.  (This option is provided based on a suggestion
  in the commentary section of ARM 11.1.)

  When CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE, items 1, 4, and 5 of the
  normal layout are affected.

    1. Nonvirtual direct base classes, in declaration order.  The first is
       an incomplete subobject; any others are complete -- i.e., space is
       reserved for their virtual base classes.  (This may result in wasted
       space, since a virtual base class may appear in association with
       more than one nonvirtual direct base class.)

    2. Nonstatic data members, in declaration order.

    3. Virtual function table pointer, unless omitted because the table is
       shared with the first nonvirtual direct base class.

    4. Pointers to direct and some indirect virtual base class subobjects.
       (The pointer-sharing optimization is the same as that for the
       normal layout.)  The order in which the pointers are put out cannot
       be summarized easily, since it appears to be an accidental artifact
       of the algorithm and internal data structures employed by cfront.

    5. All virtual base classes (generally direct but sometimes indirect)
       that are not embedded in some other subobject, in an order that
       again is an artifact of cfront's implementation.  These virtual
       base class data sections are put out as complete subobjects.

  In general, there are two advantages of the normal layout over the
  cfront-compatible layout.  First, and most importantly, it may result in
  smaller objects, since virtual base classes data sections are guaranteed
  to be put out only once.  Second, the order in which virtual base class
  pointers and virtual base class data sections appear in the layout is
  predictable, not an accident of the implementation.

  For detailed information about cfront layout compatibility issues please
  refer to the functions involved, including set_data_section_base_class
  in class_decl.c.
*/

#include "basics.h"
#include "layout.h"
#include "il.h"
#include "error.h"
#include "cmd_line.h"
#include "types.h"
#include "target.h"
#include "const_ints.h"


void clear_layout_block(a_layout_block_ptr  lob,
                        a_type_ptr          class_type)
/*
Clear the block used to contain information while working out class layout.
*/
{
  lob->class_type = class_type;
  lob->byte_offset = 0;
  lob->bit_offset = 0;
  lob->alignment = TARG_MINIMUM_STRUCT_ALIGNMENT;
  lob->any_overflow = FALSE;
}  /* clear_layout_block */


static void check_enum_type_for_bit_field(a_type_ptr    bit_field_type,
                                          unsigned long bit_field_size,
                                          a_boolean     *need_signed_type)
/*
Check to see that the values of the enumerated type bit_field_type will all
fit in a bit field of size bit_field_size.  If not, give a warning.  Return
*need_signed_type TRUE if the bit field type must be signed, FALSE if it
must be unsigned.
*/
{
  a_boolean      use_signed = FALSE, smallest_is_negative;
  a_constant     smallest, largest;
  unsigned long  bits_needed, bits_needed_largest;
  a_constant_ptr enum_con;

  enum_con = bit_field_type->variant.integer.enum_info.constant_list;
  if (enum_con == NULL) {
    /* There are no enumeration constants, so no bits are needed to
       represent all of them; by definition, they fit in the bit field. */
  } else {
    /* Check the constants on the list.  Start by finding the largest and
       smallest constants.  We are assuming most enum type lists
       won't be too long, and there won't be too many bit fields with enum
       type, so a linear search should be acceptable.  Furthermore,
       the usual case is that the bit field is big enough, so we're probably
       going to scan the whole constant list; therefore it's okay to always
       scan the whole list even though some errors could be detected during
       the scan. */
    smallest = *enum_con;
    largest = *enum_con;
    for (;;) {
      enum_con = enum_con->next;
      if (enum_con == NULL) break;
      if (cmp_integer_constants(enum_con, &smallest) < 0) smallest = *enum_con;
      if (cmp_integer_constants(enum_con, &largest)  > 0) largest  = *enum_con;
    }  /* for */
    /* Determine the number of bits needed to represent largest value. */
    bits_needed_largest =
                        bits_required_to_represent_integer_constant(&largest);
    /* See if the smallest value is negative. */
    smallest_is_negative = (sign_of_integer_constant(&smallest) < 0);
#if TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
    /* Enum bit fields are always unsigned (many ABIs require this). */
    use_signed = FALSE;
    bits_needed = bits_needed_largest;
#else /* !TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */
    /* Determine the proper signedness for the bit field.  One can't
       simply use the signedness of the enum type, since that was chosen
       for efficiency reasons: if the enum values just fit in the bit
       field size, an unsigned field might be necessary even though a 
       signed type was a good choice for the enum type. */
    if (smallest_is_negative) {
      /* Some enum values are negative, so a signed type is required.
         The enum type must already be signed. */
      use_signed = TRUE;
    } else if (bits_needed_largest >= bit_field_size) {
      /* The largest value is nonnegative (because the smallest is
         nonnegative), and it's big enough that it wouldn't fit in a
         signed field.  Therefore, an unsigned type is required. */
      use_signed = FALSE;
    } else {
      /* The signedness is not forced by the enum values, so use the 
         target preference.  Make a one-bit field always unsigned. */
      if (bit_field_size == 1) {
        use_signed = FALSE;
      } else {
        use_signed = !(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED);
      }  /* if */
    }  /* if */
    if (use_signed && sign_of_integer_constant(&largest) >= 0) {
      /* Using a signed bit field and the largest is nonnegative, so the
         largest really requires one more bit for a zero sign. */
      bits_needed_largest++;
    }  /* if */
    /* Determine the number of bits needed. */
    { a_boolean bits_needed_smallest =
                        bits_required_to_represent_integer_constant(&smallest);
      if (bits_needed_largest > bits_needed_smallest) {
        bits_needed = bits_needed_largest;
      } else {
        bits_needed = bits_needed_smallest;
      }  /* if */
    }
#endif /* TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */
    /* Check that the enum values will fit in the bit field. */
    if (bits_needed > bit_field_size
#if TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
        || smallest_is_negative
#endif /* TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */
                                    ) {
      warning(ec_enum_bit_field_too_small);
    }  /* if */
  }  /* if */
  *need_signed_type = use_signed;
}  /* check_enum_type_for_bit_field */


/* A value used by class declaration processing only to represent the
   size of a an unnamed bit field with a declared length of zero. */
#define UNNAMED_ZERO_LENGTH_BIT_FIELD_SIZE (TARG_MAX_BIT_FIELD_SIZE + 1)


void scan_bit_field_size(a_boolean         *unnamed_bit_field,
                         a_type_ptr        *p_base_type,
                         long              *p_bit_field_size,
                         a_boolean         *p_is_signed,
                         a_symbol_locator  *locator)
/*
Scan the size in a bit-field declaration:

    unsigned int j: 5 ;
                    ^---- this size.

The current token is the colon preceding the size.  If *unnamed_bit_field
is TRUE, the bit-field is unnamed.  *p_base_type gives the base type
of the declaration (unsigned int in the above example); it may be updated
on return.  *p_bit_field_size is set to the bit field size in bits.
*p_is_signed is set to indicate whether or not the bit field is signed.
*/
{
  unsigned long bit_field_size, max_size_allowed;
  a_type_ptr    base_type = *p_base_type;
  a_boolean     err = FALSE, is_signed = FALSE;
  a_constant    constant;
  a_type_ptr    bit_field_type;

  db_enter(3, "scan_bit_field_size");
  /* ANSI C says the type of a bit-field must be int, unsigned int,
     or signed int, but we also allow enums and integral types (see A.6.5.8
     in the Common Extensions appendix).  pcc and C++ (ARM 9.6) allow any
     integral or enum type. */
  bit_field_type = skip_typerefs(base_type);
  if (!is_integral_type(bit_field_type)) {
    /* Diagnostic has already been issued. */
    bit_field_type = integer_type((an_integer_kind)ik_int);
  }  /* if */
  /* Note that if the base type was not integral it has been replaced by
     "int" by this point. */
  /* Advance past the colon. */
  (void)get_token();
  /* Scan the integral size in bits of the bit-field. */
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
    /* Use small value to avoid more errors, but not 1 which is special. */
    bit_field_size = TARG_CHAR_BIT;
    err = TRUE;
  } else if (constant.kind == (a_constant_repr_kind)ck_template_param) {
    /* A template parameter during the prototype instantiation.  The value
       is not known.  Use a small value that is not 1. */
    bit_field_size = TARG_CHAR_BIT;
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("scan_bit_field_size: size not int");
    }  /* if */
#endif /* CHECKING */
    /* The size of the bit field must be non-negative and must not exceed
       the size of the underlying type (except for enums, whose type was
       picked by the front end) or the target maximum bit field size. */
    max_size_allowed = TARG_MAX_BIT_FIELD_SIZE;
    if (!bit_field_type->variant.integer.enum_type) {
      max_size_allowed = bit_field_type->size*TARG_CHAR_BIT;
    }  /* if */
    bit_field_size = unsigned_value_of_integer_constant(&constant, &err);
    /* Note that one reason for err to be TRUE is if the constant is
       less than zero. */
    if (err || bit_field_size > max_size_allowed) {
      error(ec_bad_bit_field_size);
      bit_field_size = max_size_allowed;
    } else if (bit_field_size == 0) {
      /* The bit-field size is zero, so the field must be unnamed. */
      if (*unnamed_bit_field || cfront_compatibility_mode) {
        /* Use a special value other than zero for the size of an unnamed
           zero length bit field.  This is required for distinguishing a
           field entry of type bit_field_type representing a zero length
           bit field from a field entry for an ordinary field of the same
           type that is unnamed (an extension). */
        bit_field_size = UNNAMED_ZERO_LENGTH_BIT_FIELD_SIZE;
        if (!(*unnamed_bit_field)) {
          /* Cfront compatibility -- permit named bit fields to have zero
             size, but change the value of *unnamed_bit_field so that they
             will not be entered into the symbol table.  Note that it would
             be possible for the name to be used again (though this would not
             be acceptable to cfront), but it also means the field will not
             be subject to initialization (cfront allows such fields to be
             initialized and may generate invalid C as a result) and it means
             the field cannot be referenced (again, cfront allows it and
             generates invalid C). */
          pos_warning(ec_zero_length_bit_field_must_be_unnamed,
                      &locator->source_position);
          *unnamed_bit_field = TRUE;
        }  /* if */
      } else {
        pos_error(ec_zero_length_bit_field_must_be_unnamed,
                  &locator->source_position);
        bit_field_size = 1;
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Determine the signedness of the bit field. */
  if (bit_field_type->variant.integer.enum_type) {
    /* The integral type is an enum type.  Give a warning if any of the
       enumeration's constants will not fit in the bit field, and determine
       whether the bit field should be signed or unsigned. */
    check_enum_type_for_bit_field(bit_field_type, bit_field_size, &is_signed);
  } else if (bit_field_type->variant.integer.explicitly_signed ||
             (C_dialect != C_dialect_pcc &&
              bit_field_type->variant.integer.int_kind ==
                                          (an_integer_kind)ik_signed_char)) {
    /* The integral type was explicitly signed in the source, e.g.,
       "signed int" instead of just "int".  (This information comes
       from the type entry itself.)  That forces the bit field to
       be signed.  The integral type already has the right kind and
       signedness.  Note that this won't happen in pcc mode because "signed"
       is not part of the pcc language. */
    is_signed = TRUE;
  } else if (!int_kind_is_signed[bit_field_type->variant.integer.int_kind]) {
    /* The integral type must have been explicitly declared "unsigned", or else
       it's a plain "char" that is treated as unsigned. */
    is_signed = FALSE;
  } else {
    /* The integral type is "plain" (i.e., plain "int", "char", "short",
       "long", or "long long") -- it's not explicitly signed or unsigned and
       it's not an enum type. */
    if (bit_field_size > 1 && !TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED) {
      /* Keep the default signedness of the plain integral type. */
      is_signed = TRUE;
    } else {
      /* The default for plain integral types in bit fields is unsigned -- or
         else this is a one-bit bit field, for which anything but unsigned
         may not make much sense.  Change the type to an unsigned version of
         the same integral type. */
      an_integer_kind  new_int_kind;

      is_signed = FALSE;
      switch (bit_field_type->variant.integer.int_kind) {
        case ik_signed_char:
          /* Possible in pcc mode only. */
        case ik_char:
          new_int_kind = (an_integer_kind)ik_unsigned_char;
          break;
        case ik_short:
          new_int_kind = (an_integer_kind)ik_unsigned_short;
          break;
        case ik_int:
          new_int_kind = (an_integer_kind)ik_unsigned_int;
          break;
        case ik_long:
          new_int_kind = (an_integer_kind)ik_unsigned_long;
          break;
#if LONG_LONG_ALLOWED
        case ik_long_long:
          new_int_kind = (an_integer_kind)ik_unsigned_long_long;
          break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
        default:
          internal_error("scan_bit_field_size: bad plain int kind");
#endif /* CHECKING */
      }  /* switch */
      bit_field_type = integer_type(new_int_kind);
    }  /* if */
  }  /* if */
  /* Give a warning for a signed one-bit field; ANSI C allows it, but it's
     strange. */
  if (!err && !*unnamed_bit_field && is_signed && bit_field_size == 1) {
    pos_warning(ec_signed_one_bit_field, &locator->source_position);
  }  /* if */
  /* Set base_type to bit_field_type with the proper type qualifiers. */
  if (bit_field_type == skip_typerefs(base_type)) {
    /* The original type, base_type, has turned out to be correct after all.
       Use it directly to avoid wasting the type qualifiers, if any. */
  } else {
    /* Build a type with the right qualifiers.  Note that bit_field_type
       should not have any qualifiers at this point; the qualifiers from the
       base type, if any, are added. */
    base_type = make_identically_qualified_type(bit_field_type, base_type);
  }  /* if */
  *p_base_type = base_type;
  *p_bit_field_size = bit_field_size;
  *p_is_signed = is_signed;

  db_exit();
}  /* scan_bit_field_size */


static a_boolean increment_field_offsets(a_targ_size_t *byte_offset,
                                         int           *bit_offset,
                                         a_targ_size_t byte_incr,
                                         int           bit_incr)
/*
Increment the byte and bit offsets by the indicated amount, checking for
overflow.  Return TRUE if the update is successful, FALSE if there was an
overflow error.
*/
{
  /* The ULTRIX C compiler has trouble with the type of folded compile-time
     unsigned expressions, so we use a variable for this value. */
  a_targ_size_t max_byte_offset = TARG_SIZE_T_MAX / TARG_CHAR_BIT;
  a_targ_size_t extra_byte_offset;
  a_boolean     overflow = FALSE;

  db_enter(4, "increment_field_offsets");
  /* The offset will eventually go into the field as a bit offset, and
     therefore the maximum byte offset is somewhat smaller than one might
     expect. */
  if (byte_incr >= max_byte_offset ||
      *byte_offset > (max_byte_offset - byte_incr)) {
    overflow = TRUE;
  } else {
    *byte_offset += byte_incr;
  }  /* if */
  if (bit_incr != 0) {
    if (*bit_offset > INT_MAX-bit_incr) {
      overflow = TRUE;
    } else {
      *bit_offset += bit_incr;
    }  /* if */
    /* If the bit offset has gone into the next byte, transfer some of the
       bit offset over to the byte offset. */
    if (*bit_offset >= TARG_CHAR_BIT) {
      extra_byte_offset = *bit_offset / TARG_CHAR_BIT;
      if (*byte_offset > max_byte_offset-extra_byte_offset) {
        overflow = TRUE;
      } else {
        *byte_offset += extra_byte_offset;
      }  /* if */
      *bit_offset = *bit_offset % TARG_CHAR_BIT;
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* increment_field_offsets */


a_boolean do_alignment(a_targ_size_t    *byte_offset,
                       int              *bit_offset,
                       a_targ_alignment alignment)
/*
Increment the byte and bit offsets to align them with the indicated 
byte-multiple boundary.  Return TRUE if the update is successful, FALSE if
there was an overflow error.
*/
{
  a_targ_size_t byte_mod;
  a_boolean	overflow = FALSE;

  if (*bit_offset != 0) {
    /* If the bit offset indicates a partial storage byte, round the offsets
       to the next byte. */
    overflow = !increment_field_offsets(byte_offset, bit_offset,
				       (a_targ_size_t)0,
                                       (int)(TARG_CHAR_BIT - *bit_offset));
  }  /* if */
  if (!overflow) {
    byte_mod = *byte_offset % alignment;
    if (byte_mod != 0) {
      /* Increment the byte offset to make it a multiple of the required
         alignment. */
      overflow = !increment_field_offsets(byte_offset, bit_offset,
                                         (a_targ_size_t)(alignment - byte_mod),
					 0);
    }  /* if */
  }  /* if */
  return !overflow;
}  /* do_alignment */


#if TARG_BIT_FIELD_CONTAINER_SIZE < 0           /* base_type is used. */
#else
#if  TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT < 0    /* base_type is used. */
#else                                           /* base_type is not used. */
/*ARGSUSED*/
#endif /* TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT < 0 */
#endif /* TARG_BIT_FIELD_CONTAINER_SIZE < 0 */
static a_boolean align_offsets_for_bit_field(int              bit_size,
                                             a_targ_size_t    *byte_offset,
                                             int              *bit_offset,
					     a_targ_alignment *p_alignment,
                                             a_type_ptr       base_type)
/*
As part of maintaining field offsets while processing fields of a struct
definition, update *byte_offset and *bit_offset to indicate the position
(after alignment if necessary) of a bit-field of size bit_size.  If
bit_size == 0, this forces some kind of bit-field alignment.  See 3.5.2.1.
base_type is the integral base type for the bit field (e.g., int, unsigned
int).  Return the effective alignment for the field, i.e., the alignment
for the container used, in *p_alignment.  If any overflow was detected in
computing the alignment, FALSE is returned; if there's no overflow TRUE is
returned.
*/
{
  a_targ_size_t    container_size;
  a_targ_alignment container_alignment;
  a_boolean	   overflow = FALSE;

  db_enter(4, "align_offsets_for_bit_field");

/*
Useful macro that determines whether a field of size bit_size at the
current offset will fit into a container of size container_size (in bytes)
aligned according to container_alignment.
*/
#define fits_in_container(container_size, container_alignment)        \
 (((*byte_offset % (container_alignment))*TARG_CHAR_BIT + *bit_offset) + \
                                    bit_size <= (container_size)*TARG_CHAR_BIT)

  if (bit_size == 0) {
    /* A zero-width bit field is declared for alignment only.  The container
       size is not significant. */
    container_size = 1;
    /* TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT is
         >  0 to indicate a particular alignment
         == 0 to indicate minimal alignment
         <  0 to indicate "use the alignment of the base type from the
              declaration as the container alignment".
    */
#if TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT > 0
    container_alignment = TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT;
#else
#if TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT == 0
    container_alignment = 1;
#else /* if TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT < 0 */
    /* Use the base type alignment. */
    base_type = skip_typerefs(base_type);
    container_alignment = base_type->alignment;
#endif /* TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT == 0 */
#endif /* TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT > 0 */
  } else {
    /* TARG_BIT_FIELD_CONTAINER_SIZE is
         >  0 to indicate a particular size for the bit-field container.
         == 0 to indicate "use the smallest integral type into which the
              bit-field will fit".
         < 0  to indicate "use the base type from the declaration as
              the container type".
    */
#if TARG_BIT_FIELD_CONTAINER_SIZE > 0
    /* Use a fixed size container.  TARG_BIT_FIELD_CONTAINER_SIZE indicates the
       size in bytes. */
    container_size = TARG_BIT_FIELD_CONTAINER_SIZE;
#if TARG_BIT_FIELD_CONTAINER_SIZE == 1
    container_alignment = 1;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_SHORT
    container_alignment = TARG_ALIGNOF_SHORT;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_INT
    container_alignment = TARG_ALIGNOF_INT;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_LONG
    container_alignment = TARG_ALIGNOF_LONG;
#else
#if LONG_LONG_ALLOWED
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_LONG_LONG
#define QQ_USE_LONG_LONG
#endif
#endif
#ifdef QQ_USE_LONG_LONG
    container_alignment = TARG_ALIGNOF_LONG_LONG;
#else
??=error -- TARG_BIT_FIELD_CONTAINER_SIZE in target.h is set wrong.
#endif
#endif
#endif
#endif
#endif

#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == 0
    /* Use the smallest integral type into which the field will fit as
       the container.  Try first to find such a type for the current
       position (where the field may start off a byte boundary, and
       may therefore require a larger container than it would if optimally
       aligned). */
    container_size = 0;  /* Meaning not set yet. */
    if (bit_size > 0) {
      if (fits_in_container(1, 1)) {
        /* Char. */
        container_size      = 1;
        container_alignment = 1;
      } else if (fits_in_container(TARG_SIZEOF_SHORT, TARG_ALIGNOF_SHORT)) {
        /* Short. */
        container_size      = TARG_SIZEOF_SHORT;
        container_alignment = TARG_ALIGNOF_SHORT;
      } else if (fits_in_container(TARG_SIZEOF_INT, TARG_ALIGNOF_INT)) {
        /* Int. */
        container_size      = TARG_SIZEOF_INT;
        container_alignment = TARG_ALIGNOF_INT;
      } else if (fits_in_container(TARG_SIZEOF_LONG, TARG_ALIGNOF_LONG)) {
        /* Long. */
        container_size      = TARG_SIZEOF_LONG;
        container_alignment = TARG_ALIGNOF_LONG;
      }  /* if */
    }  /* if */
    if (container_size == 0) {
      /* The field can't be made to fit at the current position, so alignment
         will have to be done.  A smaller container size might now apply,
         since the field will be optimally aligned. */
      container_size = (bit_size + (TARG_CHAR_BIT-1)) / TARG_CHAR_BIT;
      if (container_size <= 1) {
        /* Char. */
        container_size      = 1;
        container_alignment = 1;
      } else if (container_size <= TARG_SIZEOF_SHORT) {
        /* Short. */
        container_size      = TARG_SIZEOF_SHORT;
        container_alignment = TARG_ALIGNOF_SHORT;
      } else if (container_size <= TARG_SIZEOF_INT) {
        /* Int. */
        container_size      = TARG_SIZEOF_INT;
        container_alignment = TARG_ALIGNOF_INT;
      } else if (container_size <= TARG_SIZEOF_LONG) {
        /* Long. */
        container_size      = TARG_SIZEOF_LONG;
        container_alignment = TARG_ALIGNOF_LONG;
#if LONG_LONG_ALLOWED
      } else if (container_size <= TARG_SIZEOF_LONG_LONG) {
        /* Long long. */
        container_size      = TARG_SIZEOF_LONG_LONG;
        container_alignment = TARG_ALIGNOF_LONG_LONG;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
      } else {
        internal_error("align_offsets_for_bit_field: size is too big");
#endif /* CHECKING */
      }  /* if */
    }  /* if */
#else /* TARG_BIT_FIELD_CONTAINER_SIZE < 0 */
    /* Always use the base type size and alignment. */
    base_type = skip_typerefs(base_type);
    container_size      = base_type->size;
    container_alignment = base_type->alignment;
#endif /* TARG_BIT_FIELD_CONTAINER == 0 */
#endif /* TARG_BIT_FIELD_CONTAINER > 0 */
  }  /* if */

  /* We want to make sure that the bit field can be grabbed using one
     load of the size of the container aligned the way the container
     must be. */
  if (bit_size == 0 ||
      !fits_in_container(container_size, container_alignment)) {
    /* It can't be, so force alignment. */
    overflow = !do_alignment(byte_offset, bit_offset, container_alignment);
  }  /* if */
  *p_alignment = container_alignment;
  db_exit();
  return !overflow;
}  /* align_offsets_for_bit_field */
                              

/* Return TRUE if a given field is an unnamed field.  Test the name first
   to reduce the cost. */
#define is_unnamed_field(fp)                                          \
  ((fp)->source_corresp.name == NULL &&                               \
   (fp)->source_corresp.assoc_info == (char *)unnamed_field_symbol())


a_boolean set_field_size_and_offset(a_field_ptr      field,
                                    a_targ_size_t    *p_byte_offset,
                                    int              *p_bit_offset,
                                    a_targ_alignment *p_alignment)
/*
field points to a new field of a structure.  So far in the structure, the
byte/bit offsets are as given by *p_byte_offset and *p_bit_offset.  Set the
field's type size and alignment, and update *p_byte_offset and *p_bit_offset.
*p_alignment contains the maximum alignment required so far in the structure,
and is updated if the new field requires a larger alignment value.  If any
overflow was detected in computing the byte or bit offset, FALSE is returned;
if there's no overflow TRUE is returned.
*/
{
  a_type_ptr       field_type;
  a_targ_alignment field_alignment;
  a_boolean	   overflow;
  a_targ_size_t    save_byte_offset;
  int		   save_bit_offset;
  a_boolean        unnamed_zero_length_bit_field = FALSE;

  db_enter(4, "set_field_size_and_offset");
  /* Set the size and alignment for the field's type, if necessary. */
  field_type = skip_typerefs(field->type);
  set_type_size(field_type);
  if (is_error_type(field_type)) {
    overflow = FALSE;
  } else {
    /* Check for a bit-field. */
    if (field->bit_size != 0) {
      if (field->bit_size == UNNAMED_ZERO_LENGTH_BIT_FIELD_SIZE) {
        /* A special value was used to mark the field entry as representing
           an unnamed zero-length bit field.  Restore bit_size to zero. */
        field->bit_size = 0;
        unnamed_zero_length_bit_field = TRUE;
      }  /* if */
      /* Do any necessary alignment for a bit-field. */
      overflow = !align_offsets_for_bit_field((int)field->bit_size,
                                              p_byte_offset, p_bit_offset,
                                              &field_alignment, field_type);
    } else {
      /* Do any necessary alignment for a normal field. */
      field_alignment = field_type->alignment;
      overflow = !do_alignment(p_byte_offset, p_bit_offset, field_alignment);
    }  /* if */
    if (!overflow) {
      if (is_unnamed_field(field)) {
        /* This is an unnamed bit field (or unnamed non-bit field).  The
           alignment it forces should not affect the alignment of the struct
           as a whole. */
      } else {
        /* Remember the most stringent alignment requirement as the alignment
           requirement for the overall struct. */
        if (field_alignment > *p_alignment) {
          *p_alignment = field_alignment;
        }  /* if */
      }  /* if */
      /* Save the current byte_offset and bit_offset values.  The bit_offset
         value for the field is not updated until after increment_field_offsets
         is called because the latter performs overflow checking. */
      save_byte_offset = *p_byte_offset;
      save_bit_offset = *p_bit_offset;
      /* Increment the current offsets to account for the field. */
      if (field->bit_size != 0 || unnamed_zero_length_bit_field) {
        /* For a bit-field. */
        overflow = !increment_field_offsets(p_byte_offset, p_bit_offset,
                                            (a_targ_size_t)0,
                                            (int)field->bit_size);
      } else {
        /* For a normal field. */
        overflow = !increment_field_offsets(p_byte_offset, p_bit_offset,
                                           (a_targ_size_t)field_type->size, 0);
      }  /* if */
      if (!overflow) {
        /* Now compute the field's bit offset within the struct.  We know the
           sum will fit in the bit_offset field because increment_field_offsets
           did not report overflow. */
        field->bit_offset =
                        (save_byte_offset * TARG_CHAR_BIT) + save_bit_offset;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* set_field_size_and_offset */


static a_targ_size_t set_offset_and_alignment(a_layout_block_ptr  lob,
                                              a_targ_size_t       size,
                                              a_targ_alignment    alignment)
/*
Given a subobject of the specified size in bytes and requiring the specified
alignment, allocate space for it in the class whose current status is
given in the layout block pointed to by lob.  Return the byte offset at
which it is allocated.
*/
{
  a_targ_size_t  offset;

  /* Be sure the current byte_offset is consistent with the alignment required
     for the subobject. */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
    /* Issue an error only if one has not yet been put out. */
    if (!lob->any_overflow) {
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
  /* Save the offset at which space for the subobject is being reserved. */
  offset = lob->byte_offset;
  /* Adjust the overall alignment requirement for the current class, if
     necessary. */
  if (lob->alignment < alignment) lob->alignment = alignment;
  /* Advance the layout block's byte_offset value -- it will be class's
     size if no new subobjects are added or else the offset for the *next*
     subobject. */
  if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset, size, 0)) {
    /* Not enough space remains available in the class for this subobject. */
    if (!lob->any_overflow) {
      /* Issue an error only if one has not yet been put out. */
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
  /* Return the offset for the current subobject.  This value will typically
     be stored in the data struct representing the subobject. */
  return offset;
}  /* set_offset_and_alignment */


void set_offsets_for_nonvirtual_base_classes(a_layout_block_ptr  lob)
/*
Lay out the class_type object to store the nonvirtual direct base classes.
(They will precede the fields of the current class.)  Do this by going
through the base classes list for the current class and reserving enough
space for each base class that is directly and nonvirtually inherited;
storage is reserved in the order in which the base classes appear.  (Virtual
base classes, both direct and indirect, are dealt with after the fields of
the current class are allocated; indirect nonvirtual base classes are just
subobjects of the direct nonvirtual base classes.)  Lob points to the
layout block used to track the layout of the current class.
*/
{
  a_targ_size_t     size;
  a_targ_alignment  alignment;
  a_base_class_ptr  bcp;
  
  db_enter(4, "set_offsets_for_nonvirtual_base_classes");
  /* Traverse the list of base classes. */
  for (bcp = base_classes_of(lob->class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->is_virtual) {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      /* When cfront compatibility is required, space for a complete
         subobject (i.e., including space for it virtual base classes)
         is sometimes reserved, depending on how the complete_subobject
         flag has been set during prior processing. */
      if (bcp->complete_subobject) {
        alignment = bcp->type->alignment;
        size = bcp->type->size;
      } else {
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
        /* For a nonvirtual base classes reserve space for all the base
           class except what is required for its own virtual base classes.
           The latter will be added at the end of the storage. */
        alignment = bcp->type->variant.class_struct_union.extra_info->
                                      alignment_without_virtual_base_classes;
        size = bcp->type->variant.class_struct_union.extra_info->
                                      size_without_virtual_base_classes;
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
      bcp->offset = set_offset_and_alignment(lob, size, alignment);
#if DEBUG
      if (debug_level >= 4) {
        fputs("updated offset for ", f_debug);
        db_base_class(bcp, /*show_offset=*/TRUE);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_nonvirtual_base_classes */


#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
static void set_offsets_for_remaining_fields(a_layout_block_ptr  lob)
/*
Set the sizes and offsets of protected and private fields that were not
processed in decl_nonstatic_data_member.  This processing is required only
if fields are grouped by access before being allocated.  The public fields
will already have been done.
*/
{
  a_type_ptr           class_type = lob->class_type;
  a_field_ptr          field, prev_field, next_field;
  a_targ_size_t        local_byte_offset;
  int                  local_bit_offset, count;
  an_access_specifier  access;

  db_enter(4, "set_offsets_for_remaining_fields");
  /* Make two passes over the field list, one for protected fields and the
     other for private fields. */
  for (count = 1; count >= 0; count--) {
    access = count ? (an_access_specifier)as_protected :
                     (an_access_specifier)as_private;
    prev_field = NULL;
    /* Traverse the field list. */
    for (field = class_type->variant.class_struct_union.field_list;
         field != NULL;
         field = next_field) {
      /* Save a pointer to the next field, since field may be removed. */
      next_field = field->next;
      if (field->source_corresp.access == access) {
#if CHECKING
        if (field->bit_offset != 0) {
          internal_error(
                 "set_offsets_for_remaining_fields: field already allocated");
        }  /* if */
#endif
        /* This field has the access specification for which allocation is
           now being done.  Note that the code that follows is based on
           decl_nonstatic_data_member. */
        if (class_type->kind == (a_type_kind)tk_union) {
          /* All fields in a union have offset zero. */
          local_byte_offset = 0;
          local_bit_offset = 0;
        } else {
          local_byte_offset = lob->byte_offset;
          local_bit_offset = lob->bit_offset;
        }  /* if */
        if (!set_field_size_and_offset(field, &local_byte_offset,
                                       &local_bit_offset, &lob->alignment)) {
          /* Overflow error. */
          if (!lob->any_overflow) {
            error(ec_struct_too_large);
            lob->any_overflow = TRUE;
          }  /* if */
        }  /* if */
        /* Offset values were modified.  Save highest offset for unions, last
           offset for structs and classes, for use in establishing the size of
           the overall aggregate. */
        if (class_type->kind != (a_type_kind)tk_union ||
            local_byte_offset > lob->byte_offset ||
            (local_byte_offset == lob->byte_offset &&
             local_bit_offset > lob->bit_offset)) {
          lob->byte_offset = local_byte_offset;
          lob->bit_offset = local_bit_offset;
        }  /* if */
        if (field->source_corresp.assoc_info == NULL) {
          /* No symbol created for this field, so it must be unnamed.  Remove
             it from the field list. */
          if (prev_field == NULL) {
            class_type->variant.class_struct_union.field_list = next_field;
          } else {
            prev_field->next = next_field;
          }  /* if */
          field = NULL;
        }  /* if */
      }  /* if */
      /* If the current field was not removed from the field list, remember it
         for the next iteration of the loop. */
      if (field != NULL) prev_field = field;
    }  /* for */
  }  /* for */
  db_exit();
}  /* set_offsets_for_remaining_fields */
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */


static void set_offset_for_virtual_function_info(a_layout_block_ptr  lob)
/*
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions this routine allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by constants that can
be redefined for various implementation strategies.  Lob points to the
layout block used to track the layout of the current class.
*/
{
  a_class_type_supplement_ptr  ctsp, bcp_ctsp;
  a_targ_size_t                size;
  a_targ_alignment             alignment;
  a_base_class_ptr             bcp;

  db_enter(4, "set_offset_for_virtual_function_info");
  ctsp = lob->class_type->variant.class_struct_union.extra_info;
  if (lob->class_type->variant.class_struct_union.any_virtual_functions) {
    if (ctsp->virtual_function_info_base_class == NULL) {
      size = (a_targ_size_t)TARG_SIZEOF_VIRTUAL_FUNCTION_INFO;
      alignment = (a_targ_alignment)TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO;
      ctsp->virtual_function_info_offset =
                               set_offset_and_alignment (lob, size, alignment);
    } else {
      bcp = ctsp->virtual_function_info_base_class;
#if CHECKING
      if (bcp->offset != 0) {
        internal_error(
                  "set_offset_for_virtual_function_info: non-zero bcp offset");
      }  /* if */
#endif /* CHECKING */
      bcp_ctsp = bcp->type->variant.class_struct_union.extra_info;
      ctsp->virtual_function_info_offset = 
                          bcp->offset + bcp_ctsp->virtual_function_info_offset;
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_offset_for_virtual_function_info */


static void pointer_offset_for_virtual_base_class(a_layout_block_ptr  lob,
                                                  a_base_class_ptr    bcp)
/*
Allocate space in the current class for a pointer to virtual base class
bcp.
*/
{
  a_targ_size_t      size;
  a_targ_alignment   alignment;

  db_enter(4, "pointer_offset_for_virtual_base_class");
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#if CHECKING
  if (bcp->pointer_offset_is_set) {
    internal_error("pointer_offset_for_virtual_base_class: already set");
  }  /* if */
#endif /* CHECKING */
  bcp->pointer_offset_is_set = TRUE;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#if TARG_ALL_POINTERS_SAME_SIZE
  /* All pointers are the same size. */
  alignment = (a_targ_alignment)TARG_ALIGNOF_POINTER;
  size = (a_targ_size_t)TARG_SIZEOF_POINTER;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
??=error pointer_offset_for_virtual_base_class: different sized pointers
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  bcp->pointer_offset = set_offset_and_alignment(lob, size, alignment);
#if DEBUG
  if (debug_level >= 4) {
    fputs("updated pointer offset for ", f_debug);
    db_base_class(bcp, /*show_offset=*/TRUE);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* pointer_offset_for_virtual_base_class */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

/* This is a set of routines that allocate space for pointers to virtual
   base class data sections in cfront compatibility mode.  It is much more
   complicated that what is provided for normal mode because we have had
   to reverse-engineer cfront's algorithm for ordering the pointers. */

static a_boolean is_best_derivation(a_base_class_ptr  bcp,
                                    a_base_class_ptr  derived_bcp,
                                    a_type_ptr        class_type)
{
  a_boolean                 is_best_path;
  a_base_class_derivation_ptr  bcdp;
  a_derivation_step_ptr     step;

  if (derived_bcp == NULL) {
#if CHECKING
    if (!bcp->direct) {
      internal_error(
                 "is_best_derivation: no derived_bcp for indirect base class");
    }  /* if */
#endif /* CHECKING */
    is_best_path = TRUE;
  } else {
    bcp = corresponding_base_class(bcp, class_type, (a_base_class_ptr)NULL);
    if (first_derivation_is_direct(bcp)) {
      is_best_path = FALSE;
    } else {
      derived_bcp = corresponding_base_class(derived_bcp, class_type,
                                             (a_base_class_ptr)NULL);
      /* Return TRUE if a step pointing to derived_bcp is on the derivation
         for bcp. */
      bcdp = bcp->derivation;
      is_best_path = FALSE;
      while (!bcdp->direct) {
        for (step = bcdp->path; step != NULL; step = step->next) {
          if (step->base_class == derived_bcp) {
            is_best_path = TRUE;
            goto done;
          }  /* if */
        }  /* for */
        bcdp = bcdp->path->base_class->derivation;
      }  /* for */
#if 0
      /* Checking based on the derivation path is not really right.  If the
         need arises we'll have to beef this up. */
      if (bcp->is_virtual) {
        step = bcp->paths_to_virtual_base_class->derivation;
      } else {
        step = bcp->derivation;
      }  /* if */
      for(;;) {
        if (step->base_class == derived_bcp) {
          is_best_path = TRUE;
          break;
        } else if (step->next->base_class == bcp) {
          is_best_path = FALSE;
          break;
        }  /* if */
        step = step->next;
      }  /* if */
#endif /* if 0 */
    }  /* if */
  }  /* if */
done:
  return is_best_path;
}  /* is_best_derivation */


static void set_pointer_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
/*
base_class is a base class on the base classes list of derived_bcp.  Look
at it and its successors on the list.  For each that is a direct virtual
base class of derived_bcp and that has a NULL pointer base class allocate a
pointer to its data section.  For each base class on the list that is a
direct nonvirtual base class, call this routine recursively to pick up its
own virtual base classes, if any.  If use_decl_order is TRUE, we have a
simple loop, allocating pointers for base classes in the order they appear
on the base classes list; if it is FALSE, a recursive call assures that the
successors on the list, if any, are processed before the predecessor.
*/
{
  a_base_class_ptr  bcp;
  a_type_ptr        tp;

  db_enter(4, "set_pointer_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (first_derivation_is_direct(base_class)) {
      /* We are only interested in direct base classes. */
      if (!use_decl_order) {
        /* We should use reverse declaration order, so do the successors
           first, then return to the current base class. */
        set_pointer_offsets_for_corresponding_virtual_base_classes(
                                 lob, base_class->next, use_decl_order,
                                 derived_bcp);
      }  /* if */
      /* Virtual and nonvirtual direct base classes are handled differently. */
      if (base_class->is_virtual) {
        /* If the virtual base class needs and does not yet have a pointer
           to its data section, allocate it now and then allocate a pointer
           for any of its own virtual base classes that may need it. */
        bcp = corresponding_base_class(base_class, lob->class_type,
                                       (a_base_class_ptr)NULL);
        if (bcp->pointer_base_class == NULL && !bcp->pointer_offset_is_set &&
            is_best_derivation(bcp, derived_bcp, lob->class_type)) {
          /* Allocate the pointer. */
          pointer_offset_for_virtual_base_class(lob, bcp);
          /* Check its own virtual base classes, reversing the setting of
             use_decl_order as we go down another step in the derivation. */
          tp = bcp->type;
          if (tp->variant.class_struct_union.any_virtual_base_classes) {
            set_pointer_offsets_for_corresponding_virtual_base_classes(
                               lob, base_classes_of(tp), !use_decl_order, bcp);
          }  /* if */
        }  /* if */
      } else {
        /* If the base class has any virtual base classes of its own, allocate
           pointers to their data sections as needed.  Reverse the setting of
           use_decl_order as we go down another step in the derivation. */
        tp = base_class->type;
        if (tp->variant.class_struct_union.any_virtual_base_classes) {
          set_pointer_offsets_for_corresponding_virtual_base_classes(
                        lob, base_classes_of(tp), !use_decl_order, base_class);
        }  /* if */
      }  /* if */
      /* If we've already done the successors, break out of the loop now. */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_pointer_offsets_for_corresponding_virtual_base_classes */


static a_boolean has_virtual_base_class_with_null_pointer_base_class(
                                                  a_base_class_ptr  base_class,
                                                  a_type_ptr        class_type)
/*
Return TRUE if any of the base classes of base_class have a NULL pointer-
base-class field.  (It is the base class entry in the list associated with
class_type that we are interesting in examining.)
*/
{
  a_base_class_ptr  bcp, corresp_bcp;
  a_boolean         has_one = FALSE;

  /* Traverse the list of base classes of base_class. */
  for (bcp = base_classes_of(base_class->type); bcp != NULL; bcp = bcp->next) {
    /* If bcp is a virtual base class, look for the corresponding base class
       among the base classes of class_type. */
    if (bcp->is_virtual) {
      corresp_bcp = corresponding_base_class(bcp, class_type,
                                             (a_base_class_ptr)NULL);
      /* If it has a NULL pointer base class (meaning its pointer is not
         embedded within the body of some other base class) return TRUE.
         Otherwise keep looking. */
      if (corresp_bcp->pointer_base_class == NULL) {
        has_one = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return has_one;
}  /* has_virtual_base_class_with_null_pointer_base_class */


/* Forward declaration for set_pointer_offset_for_direct_virtual_base_class --
   mutual recursion with check_direct_virtual_base_classes_for_special_case */
static void set_pointer_offset_for_direct_virtual_base_class(
                                            a_layout_block_ptr lob,
                                            a_base_class_ptr   base_class,
                                            a_boolean          use_decl_order);

static void check_direct_virtual_base_classes_for_special_case(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   bcp,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
/*
bcp is a base class on the base classes list of derived_bcp.  Look at bcp
and its successors on the list.  For each that is a direct virtual base class
of derived_bcp that has a NULL pointer base class *and* has at least one of
its own virtual base classes with a NULL pointer base class (that's the
special case) allocate a pointer to its data section.  If use_decl_order is
TRUE, we have a simple loop, allocating pointers for base classes in the
order they appear on the base classes list; if it is FALSE, a recursive call
assures that the successors on the list, if any, are processed before the
predecessor.
*/
{
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && bcp->is_virtual && bcp->pointer_base_class == NULL &&
        has_virtual_base_class_with_null_pointer_base_class(bcp,
                                                            lob->class_type)) {
      if (!use_decl_order) {
        check_direct_virtual_base_classes_for_special_case(lob, bcp->next,
                                                           use_decl_order,
                                                           derived_bcp);
      }  /* if */
      /* Is there another derivation for this virtual base class via a more
         directly reachable intermediate base class?  If so, skip over it now
         -- it will be put out later. */
      if (is_best_derivation(bcp, derived_bcp, lob->class_type)) {
        set_pointer_offset_for_direct_virtual_base_class(lob, bcp,
                                                         use_decl_order);
      }  /* if */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
}  /* check_direct_virtual_base_classes_for_special_case */


static void set_pointer_offset_for_direct_virtual_base_class(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
base_class is a direct virtual base class of a base class of the current
class (identified by lob->class_type).  Allocate a pointer to its data
section in the current class, and then check its own direct virtual base
classes, for which pointers may also be needed.  Once all its own direct
virtual base classes have been accounted for, go through all its virtual
base classes, direct and indirect, and allocate pointers as needed for them.
*/
{
  a_base_class_ptr  bcp;
  a_type_ptr        tp;

  /* Find the base class entry on the base classes list of the current class
     that corresponds to base_class. */
  bcp = corresponding_base_class(base_class, lob->class_type,
                                 (a_base_class_ptr)NULL);
  if (!bcp->pointer_offset_is_set) {
    /* Allocate a pointer to its data section.  The offset of the pointer will
       be recored in *bcp. */
    pointer_offset_for_virtual_base_class(lob, bcp);
    tp = base_class->type; 
    if (tp->variant.class_struct_union.any_virtual_base_classes) {
      /* Next go through the base class's own direct virtual base classes,
         looking for any that require special handling. */
      bcp = base_classes_of(tp);
      check_direct_virtual_base_classes_for_special_case(lob, bcp,
                                                         !use_decl_order,
                                                         base_class);
      /* Finally, make another pass over the base classes list.  All base
         classes for which pointers must be allocated should be taken care
         of now. */
      set_pointer_offsets_for_corresponding_virtual_base_classes(
                                         lob, bcp, use_decl_order, base_class);
    }  /* if */
  }  /* if */
}  /* set_pointer_offsets_for_direct_virtual_base_class */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

static void set_virtual_base_class_pointer_offsets(a_layout_block_ptr lob)
/*
Set the pointer_offset fields in direct virtual base classes where the pointer
is not shared (i.e., where the pointer from a base class is not used).
*/
{
  a_base_class_ptr   bcp;
  
  db_enter(4, "set_virtual_base_class_pointer_offsets");

  if (lob->class_type->variant.class_struct_union.any_virtual_base_classes) {
    bcp = base_classes_of(lob->class_type);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* In cfront compatibility mode we go through the base classes list
       twice.  First we look at direct virtual base classes with a NULL pointer
       base class field and for which at least one of its own base classes
       has a NULL pointer base class field.  These get special treatment and
       are put out in declaration order. */

    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual && first_derivation_is_direct(bcp) &&
          bcp->pointer_base_class == NULL &&
          has_virtual_base_class_with_null_pointer_base_class(
                                                    bcp, lob->class_type)) {
        set_pointer_offset_for_direct_virtual_base_class(
                                          lob, bcp, /*use_decl_order=*/TRUE);
      }  /* if */
    }  /* for */
    /* Next we through the base classes list again to look for pointers that
       are still unaccounted for (i.e., have a NULL pointer base class and
       and still have a zero pointer offset).  Direct virtual base classes are
       put out in reverse declaration order, direct virtual base classes of
       direct virtual bases are put out in declaration order, etc.; in other
       words, if the number of levels of derivation is odd, they are put out in
       reverse order; if it is even, they are put out in the same order as they
       were declared.  Thus we use a recursive loop, and each time we go down
       a level we reverse the value of use_decl_order. */
    set_pointer_offsets_for_corresponding_virtual_base_classes(
                          lob, base_classes_of(lob->class_type),
                          /*use_decl_order=*/FALSE, (a_base_class_ptr)NULL);

#else /* i.e., #if !CFRONT_OBJECT_CODE_COMPATIBILITY */
    /* In normal layout mode we traverse the base classes list only once.
       The pointers are put out (when needed -- i.e., when the pointer base
       class is NULL) in base class order. */

    for (; bcp != NULL; bcp = bcp->next) {
      /* For virtual base classes we only reserve enough space for a pointer
         to the actual data section.  The latter is added at the end of the
         storage. */
      /* Only pointers to direct virtual base classes need space reserved --
         and only when the pointer is not shared, i.e., not already present in
         the data section of another base class, as indicated by the
         pointer_base_class field. */
      if (bcp->is_virtual && bcp->pointer_base_class == NULL) {
        pointer_offset_for_virtual_base_class(lob, bcp);
      }  /* if */
    }  /* for */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  db_exit();
}  /* set_virtual_base_class_pointer_offsets */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static void fixup_embedded_virtual_base_classes(a_base_class_ptr base_class,
                                                a_type_ptr       class_type)
/*
base_class is a virtual base class whose data section is being allocated;
base_class will be represented as a "complete subobject" of class_type, which
means space will be reserved for all its own virtual base classes. Therefore,
if it has any virtual base classes with data sections that have not already
been associated with some other base class, record the "official" location
of the latter as its position within base_class.  This is a recursive
algorithm.
*/
{
  a_base_class_ptr  bcp, embedded_base_class;

  db_enter(4, "fixup_embedded_virtual_base_classes");
  if (base_class->type->variant.class_struct_union.any_virtual_base_classes) {
    /* base_class has one or more virtual base classes of its own. */
    for (bcp = base_classes_of(base_class->type);
         bcp != NULL;
         bcp = bcp->next) {
      if (bcp->is_virtual) {
        /* bcp is one of the virtual base class of base_class.  Find the
           base class entry that corresponds to it in the base classes list
           for class_type. */
        embedded_base_class = corresponding_base_class(bcp, class_type,
                                                       (a_base_class_ptr)NULL);
        if (embedded_base_class->data_section_base_class != NULL) {
          /* The data section for this virtual base class has already been
             assigned a location. */
        } else {
          /* Proceed to specify how it should be embedded. */
          if (bcp->data_section_base_class == NULL) {
            /* In the context of base_class, it was not embedded (for instance,
               it may have been a direct virtual base class or a virtual
               base class that was inherited through a direct base class
               represented as an "incomplete subobject").  Therefore it will
               be embedded in the data section reserved for base_class in
               the layout of class_type. */
            embedded_base_class->data_section_base_class = base_class;
          } else {
            /* In the context of base class it was embedded (for instance,
               it may have been inherited through another virtual base class
               or through a "complete subobject" base class).  Use the same
               location in the layout of class_type. */
            embedded_base_class->data_section_base_class =
                  corresponding_base_class(bcp->data_section_base_class,
                                           class_type, (a_base_class_ptr)NULL);
          }  /* if */
          /* Apply the algorithm recursively. */
          fixup_embedded_virtual_base_classes(embedded_base_class, class_type);
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* for */
  db_exit();
}  /* fixup_embedded_virtual_base_classes */


static void set_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
*/
{
  a_base_class_ptr  bcp;

  db_enter(4, "set_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (base_class->is_virtual && base_class->direct &&
#if 0
        first_is_direct(base_class) &&
#endif /* if 0 */
        base_class->data_section_base_class == NULL) {
      if (!use_decl_order) {
        set_offsets_for_corresponding_virtual_base_classes(
                                        lob, base_class->next, use_decl_order);
      }  /* if */
      bcp = corresponding_base_class(base_class, lob->class_type,
                                     (a_base_class_ptr)NULL);
      if (bcp->data_section_base_class == NULL && bcp->offset == 0 &&
          !lob->any_overflow) {
#if CHECKING
        /* All virtual base classes should be marked as "complete
           subobjects". */
        if (!bcp->complete_subobject) {
          internal_error("set_offsets_for_corresp...: not complete subobj");
        }  /* if */
        if (lob->byte_offset == 0 && lob->bit_offset == 0) {
          internal_error("set_offsets_for_corresp...: zero offset");
        }  /* if */
#endif /* CHECKING */
        bcp->offset = set_offset_and_alignment(lob, bcp->type->size,
                                               bcp->type->alignment);
#if DEBUG
        if (debug_level >= 4) {
          fputs("updated offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
        fixup_embedded_virtual_base_classes(bcp, lob->class_type);
      }  /* if */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_corresponding_virtual_base_classes */


static void cfc_set_virtual_base_class_offsets(a_layout_block_ptr lob,
                                               a_base_class_ptr   base_class,
                                               a_boolean        use_decl_order)
/*
lob->class_type is the most-derived-type whose offsets are currently being
specified.  base_class may be NULL, in which case the direct base classes of
lob->class_type are processed; if base_class is non-NULL, the direct base
classes of base_class->type are process.  A non-NULL base_class is a direct
or indirect nonvirtual base class of lob->class_type with a complete_subobject
flag set to FALSE.  The processing involves going through the appropriate set
of direct base classes (of either lob->class_type of of base_class->type) and
locating virtual base classes that are not embedded anywhere else.  Space in
lob->class_type must be reserved for them.

base_class is a direct or indirect base class of lob->class_type for which
the complete_subobject flag is FALSE and whose virtual base classes, therefore,
may space reserved in the compete derived class.  Examine the direct
virtual base classes of base_class, find the corresponding indirect virtual
base class of class_type, and allocate space for the latter.
*/
{
  a_base_class_ptr  base_class_list, bcp;

  db_enter(4, "cfc_set_virtual_base_class_offsets");
  if (base_class == NULL) {
    base_class_list = base_classes_of(lob->class_type);
    for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
      if (bcp->data_section_base_class != NULL) {
        fixup_embedded_virtual_base_classes(bcp, lob->class_type);
      }  /* if */
    }  /* for */
    for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
#if 0
      if (bcp->direct && bcp->is_virtual &&
#if 0
          first_is_direct(bcp) &&
#endif /* if 0 */
          bcp->type->variant.class_struct_union.any_virtual_base_classes &&
          bcp->data_section_base_class == NULL) {
        /* Record the current offset in the data_section_offset of the
           virtual base class entry.  This allows for direct access of
           its fields (rather than through a pointer) as an optimization
           under certain circumstances. */
        bcp->offset = set_offset_and_alignment(lob, bcp->type->size,
                                               bcp->type->alignment);
#if DEBUG
        if (debug_level >= 4) {
          fputs("updated offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
        fixup_embedded_virtual_base_classes(bcp, lob->class_type);
      }  /* if */
#endif /* if 0 */
      if (bcp->direct) {
        if (!bcp->is_virtual) {
          if (bcp->type->variant.class_struct_union.any_virtual_base_classes) {
            break;
          }  /* if */
        } else if (first_derivation_is_direct(bcp) &&
                   bcp->type->
                        variant.class_struct_union.any_virtual_base_classes &&
                   bcp->data_section_base_class == NULL) {
          /* Record the current offset in the data_section_offset of the
             virtual base class entry.  This allows for direct access of
             its fields (rather than through a pointer) as an optimization
             under certain circumstances. */
          bcp->offset = set_offset_and_alignment(lob, bcp->type->size,
                                                 bcp->type->alignment);
#if DEBUG
          if (debug_level >= 4) {
            fputs("updated offset for ", f_debug);
            db_base_class(bcp, /*show_offset=*/TRUE);
          }  /* if */
#endif /* DEBUG */
          fixup_embedded_virtual_base_classes(bcp, lob->class_type);
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
    base_class_list = base_classes_of(base_class->type);
  }  /* if */
  if (use_decl_order) {
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->complete_subobject) {
      cfc_set_virtual_base_class_offsets(lob, bcp, !use_decl_order);
      break;
    }  /* if */
  }  /* for */
  if (!use_decl_order) {
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  db_exit();
}  /* cfc_set_virtual_base_class_offsets */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

static void set_virtual_base_class_offsets(a_layout_block_ptr  lob)
/*
Reserve space at the end of the class object for virtual base classes.
*/
{
  a_class_type_supplement_ptr	ctsp;
  int                           zero = 0;
  
  db_enter(4, "set_virtual_base_class_offsets");

  ctsp = lob->class_type->variant.class_struct_union.extra_info;
  /* Record the size and alignment of the class before space is added for
     virtual base classes. */
  if (lob->bit_offset > 0) {
    /* If the last data field was a bit field, bump the byte count by one
       before setting the size-without-virtual-base-classes value. */
    if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                 (a_targ_size_t)1, 0)) {
      if (!lob->any_overflow) {
        error(ec_struct_too_large);
        lob->any_overflow = TRUE;
      }  /* if */
    }  /* if */
    lob->bit_offset = 0;
  } else if (lob->byte_offset == 0) {
#if CHECKING
    /* The size should never be zero if there are any virtual base classes,
       since at the very least a virtual base class pointer will have been
       allocated. */
    if (lob->class_type->variant.class_struct_union.any_virtual_base_classes) {
      internal_error("set_virtual_base_class_offsets: offset is zero");
    }  /* if */
#endif /* CHECKING */
    /* An empty class must occupy at lease one byte of memory. */
    lob->byte_offset = 1;
  }  /* if */
  ctsp->size_without_virtual_base_classes = lob->byte_offset;
  ctsp->alignment_without_virtual_base_classes = lob->alignment;
  /* Note that the current size may not be consistent (according to the rules
     for C structs) with the current alignment.  Modify size-without-virtual-
     base-classes in such a case, but without changing lob->byte_offset (i.e.,
     without introducing unwanted padding in the current class before
     the data sections for the virtual base classes are put out.  This
     assures that size-without-virtual-base-classes will correspond to the
     the actual size of an incomplete subobject. */
  if (!do_alignment(&ctsp->size_without_virtual_base_classes, &zero,
                    ctsp->alignment_without_virtual_base_classes)) {
    if (!lob->any_overflow) {
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
  /* Now see if there are any virtual base class data sections that need to
     be added to the layout for the current class. */
  if (lob->class_type->variant.class_struct_union.any_virtual_base_classes) {
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    /* In cfront compatibility layout mode all virtual base classes that
       are not embedded in another class have space reserved for them.  The
       order in which cfront puts them out is emulated. */
    cfc_set_virtual_base_class_offsets(lob, (a_base_class_ptr)NULL,
                                       /*use_decl_order=*/FALSE);
#else
    /* In normal layout mode all virtual base classes have space reserved at
       this point in the layout.  The order in which they are put out is
       the order of their appearance in the base classes list, which
       corresponds to a depth-first left-to-right traversal of the the base
       classes represented in a directed acyclic graph (see ARM 12.6.2). */
    a_base_class_ptr   bcp = ctsp->base_classes;
    a_targ_size_t      size;
    a_targ_alignment   alignment;

    if (bcp != NULL) {
      /* Now add the virtual base classes to the storage.  This is done
         almost exactly as for nonvirtual base classes. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual) {
          /* Record the current offset in the data_section_offset of the
             virtual base class entry.  This allows for direct access of
             its fields (rather than through a pointer) as an optimization
             under certain circumstances. */
          size = bcp->type->variant.class_struct_union.extra_info->
                                             size_without_virtual_base_classes;
          alignment = bcp->type->variant.class_struct_union.extra_info->
                                        alignment_without_virtual_base_classes;
          bcp->offset = set_offset_and_alignment(lob, size, alignment);
#if DEBUG
          if (debug_level >= 4) {
            fputs("updated offset for ", f_debug);
            db_base_class(bcp, /*show_offset=*/TRUE);
          }  /* if */
#endif /* DEBUG */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  db_exit();
}  /* set_virtual_base_class_offsets */


static void set_base_class_offsets(a_base_class_ptr  proximate_derivation)
/*
The offset of base class proximate_derivation has been computed, but the
offsets of its own base classes have not.  The confusing part of this
processing is that two different base class entries are involved.  First,
the "most derived class" contains a list of all its base classes, direct
and indirect.  But each class from which it is derived has its own base
class list, too.  For example:

          A    A       class B points to base class A (path ==>A)
          |    |
          B    C       class C points to base class A (path ==>A)
           \  /
             D         class D points to base classes B (path ==>B)
                                                      A (path ==>B==>A)
                                                      C (path ==>C)
                                                      A (path ==>C==>A)

Thus, while the base classes for D, the "most derived class", are
represented by only 3 type entries (A, B, and C), there are 4 base class
entries involved, since 2 are associated with class A.  As for offsets, B's
"A" base class entry indicates the offset of the "A" data section within
the block occupied by class "B" entities, whereas the two "A" base classes
associated with class D should have offsets representing their locations
within a "D" object.  Computing the offset of an indirect base class (e.g.,
A) within a most derived class (e.g., D) requires adding the offset of the
base class immediately derived from it (e.g., B) to its own offset within
that class (e.g., A's offset within B); in other words, the D::A offset
equals the D::B offset plus the B::A offset.

The algorithm involves going through direct base classes of
proximate_derivation (in this example "B in D" is the proximate_derivation,
and it has only one direct base class of its own, namely, "A in B"),
finding the corresponding base class entry in the most derived class (e.g.,
finding the appropriate "A in D" -- the one whose path is ==>B==A), and
setting the offset field in the latter.
*/
{
  a_base_class_ptr  ref_bcp, bcp;

  db_enter(4, "set_base_class_offsets");
  /* Get the first "reference" base class of the root class, which is itself
     a base class of the most derived class.  It is called a reference base
     class because it contains an offset relative to the root base class.
     It is the offset value relative to the most derived class that we need
     to determine and record. */
  ref_bcp = base_classes_of(proximate_derivation->type);
#if DEBUG
  if (debug_level >= 4) {
    if (ref_bcp != NULL) {
      fputs("setting offsets for base classes of:\n  ", f_debug);
      db_base_class(proximate_derivation, /*show_offset=*/TRUE);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the reference base classes, the direct base classes of
     the proximate_derivation base class. */
  for (; ref_bcp != NULL; ref_bcp = ref_bcp->next) {
    if (ref_bcp->direct) {
      bcp = corresponding_base_class(ref_bcp,
                                     proximate_derivation->derived_class,
                                     proximate_derivation);
      if (!bcp->is_virtual) {
        /* Nonvirtual base class. */
        bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if DEBUG
        if (debug_level >= 4) {
          fputs("reference base class ", f_debug);
          db_base_class(ref_bcp, /*show_offset=*/TRUE);
          fputs("new offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      } else {
        /* Virtual base class. */
        if (bcp->data_section_base_class == proximate_derivation) {
          /* bcp is a virtual base class whose data section is embedded in
             the data section of another base class data section.  Update
             the offset. */
          bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if DEBUG
          if (debug_level >= 4) {
            fputs("reference base class ", f_debug);
            db_base_class(ref_bcp, /*show_offset=*/TRUE);
            fputs("new offset for ", f_debug);
            db_base_class(bcp, /*show_offset=*/TRUE);
          }  /* if */
#endif /* DEBUG */
        } else {
          continue;
        }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
      }  /* if */
      /* Make a recursive call to apply this processing to the next level of
         base classes. */
      set_base_class_offsets(bcp);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    } else if (ref_bcp->is_virtual) {
      /* Virtual indirect base class. */
      bcp = corresponding_base_class(ref_bcp,
                                     proximate_derivation->derived_class,
                                     (a_base_class_ptr)NULL);
      if (bcp->data_section_base_class == proximate_derivation) {
        /* bcp is a virtual base class whose data section is embedded in
           the data section of another base class data section.  Update
           the offset. */
        bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if DEBUG
        if (debug_level >= 4) {
          fputs("reference base class ", f_debug);
          db_base_class(ref_bcp, /*show_offset=*/TRUE);
          fputs("new offset for ", f_debug);
          db_base_class(bcp, /*show_offset=*/TRUE);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_base_class_offsets */


static void set_offsets_for_indirect_base_classes(a_type_ptr  class_type)
/*
Compute the offsets from the start of the object described by class_type
of each of its indirect base classes.  The direct base classes and the
data sections of all virtual base classes, direct or indirect, have already
been handled.  The processing of this routine and its subroutines is
addressed to indirect base classes.
*/
{
  a_base_class_ptr      bcp;

  db_enter(4, "set_offsets_for_indirect_base_classes");
  bcp = base_classes_of(class_type);
#if DEBUG
  if (debug_level >= 4) {
    if (bcp != NULL) {
      fputs("before setting offsets: ", f_debug);
      db_base_class_list(class_type);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the list of base classes, direct and indirect, that are
     defined for the class, but ignore all but the direct base classes.  The
     rest are handled by recursively scanning the base class tree. */
  for (; bcp != NULL; bcp = bcp->next) {
    if (first_derivation_is_direct(bcp)) {
      set_base_class_offsets(bcp);
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_indirect_base_classes */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static void set_embedded_virtual_base_class_offset(a_base_class_ptr base_class)
/*
base_class is a direct or indirect virtual base class of class_type.  If
it is allocated inside another base class, compute its offset within the layout
its class.  Then do the same check for its own direct virtual base
classes.
*/
{
  a_base_class_ptr  data_section_bcp, bcp, curr_class_bcp;

  db_enter(4, "set_embedded_virtual_base_class_offset");
  if (base_class->offset == 0) {
    /* Offset has not yet been set. */
#if DEBUG
    if (debug_level >= 4) {
      db_base_class(base_class, /*show_offset=*/TRUE);
    }  /* if */
#endif /* DEBUG */
    data_section_bcp = base_class->data_section_base_class;
    if (data_section_bcp != NULL) {
      if (data_section_bcp->is_virtual &&
          data_section_bcp->data_section_base_class != NULL &&
          data_section_bcp->offset == 0) {
        set_embedded_virtual_base_class_offset(data_section_bcp);
      }  /* if */
      /* Look for the corresponding virtual base class. */
      bcp = corresponding_base_class(base_class, data_section_bcp->type,
                                     (a_base_class_ptr)NULL);
      /* The pointer_offset value in the context of the derived class
         is the offset of the pointer base class plus the offset of the
         virtual base class pointer within the latter. */
      base_class->offset = bcp->offset + data_section_bcp->offset;
      /* Update the offsets of nonvirtual base classes from which base_class is
         derived. */
      set_base_class_offsets(base_class);
      /* Apply the check recursively to see if there are any indirect virtual
         base classes of class_type that have not been properly assigned an
         offset yet. */
      bcp = base_classes_of(base_class->type);
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual && bcp->direct) {
          curr_class_bcp = corresponding_base_class(bcp,
                                                    base_class->derived_class,
                                                    (a_base_class_ptr)NULL);
          if (curr_class_bcp->data_section_base_class == NULL) {
            /* curr_class_bcp is an indirect virtual base class of class type
               that is not yet marked as embedded. */
            curr_class_bcp->data_section_base_class = base_class;
          }  /* if */
          set_embedded_virtual_base_class_offset(curr_class_bcp);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_embedded_virtual_base_class_offset */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */

static void fixup_shared_virtual_base_class_offsets(a_type_ptr  class_type)
/*
Set the pointer_offset fields in direct virtual base classes where the
virtual base class pointer is shared with some other base class.
*/
{
  a_base_class_ptr             virtual_base_class;
  a_base_class_ptr             pointer_base_class;
  a_base_class_ptr             bcp;

  db_enter(4, "fixup_shared_virtual_base_class_offsets");
  /* Make a pass over all the base classes for the current derived class and
     check each virtual base class. */
  for (virtual_base_class = base_classes_of(class_type);
       virtual_base_class != NULL;
       virtual_base_class = virtual_base_class->next) {
    if (virtual_base_class->is_virtual) {
      /* If the pointer_base_class field is non-NULL, the virtual base class
         pointer for the derived class is the same as the pointer to the
         corresponding virtual base class for pointer_base_class. */
      pointer_base_class = virtual_base_class->pointer_base_class;
      if (pointer_base_class != NULL) {
        /* Look for the corresponding virtual base class. */
        bcp = corresponding_base_class(virtual_base_class,
                                       pointer_base_class->type,
                                       (a_base_class_ptr)NULL);
        /* The pointer_offset value in the context of the derived class
           is the offset of the pointer base class plus the offset of the
           virtual base class pointer within the latter. */
        virtual_base_class->pointer_offset = bcp->pointer_offset +
                                                  pointer_base_class->offset;
      }  /* if */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      set_embedded_virtual_base_class_offset(virtual_base_class);
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
  }  /* for */
  db_exit();
}  /* fixup_shared_virtual_base_class_offsets */


void finish_laying_out_class(a_layout_block_ptr  lob)
/*
Complete laying out the object specified by the layout block's class_type.
This includes allocating any remaining fields whose allocation may have been
delayed and making room for virtual base classes, which appear at the end of
the layout.
*/
{
  a_type_ptr        class_type = lob->class_type;

  db_enter(3, "finish_laying_out_class");
  /* Space for direct nonvirtual base classes was allocated right after the
     base class specifiers were scanned.  In addition, space for nonstatic
     data members was allocated as they were encountered (except in the
     case where they were segregated by accessibility -- see below). */
  if (C_dialect == C_dialect_cplusplus) {
#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
    /* Only public fields were given an offset in decl_nonstatic_data_member.
       Now that all the fields have been seen and added to the class's field
       list, traverse the field list again and allocate protected and private
       fields. */
    set_offsets_for_remaining_fields(lob);
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
    /* After the nonstatic data members allocate space for the virtual
       function info block (typically a pointer to the virtual function
       table. */
    set_offset_for_virtual_function_info(lob);
    /* Next allocate space for pointers to the virtual base class data
       sections. */
    set_virtual_base_class_pointer_offsets(lob);
    /* Finally, allocate space for the virtual base class data sections
       themselves. */
    set_virtual_base_class_offsets(lob);
  }  /* if */
  /* Adjust the total size of the class to be consistent with the
     overall alignment required for the class. */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, lob->alignment)) {
    if (!lob->any_overflow) error(ec_struct_too_large);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* Go through all the indirect base classes and compute their
       offsets within the current derived class. */
    set_offsets_for_indirect_base_classes(class_type);
    /* Similarly go through all the virtual base classes and do any required
       fixup on their pointer offsets and (in cfront compatibility mode)
       their data section offsets. */
    fixup_shared_virtual_base_class_offsets(class_type);
  }  /* if */
  /* Record the overall size and alignment in the class's type entry. */
  class_type->size = lob->byte_offset;
  class_type->alignment = lob->alignment;
  /* Avoid a zero-sized structure (as in "struct {int : 0;}" for C and in
     "class {}" for C++). */
  if (class_type->size == 0) class_type->size = 1;
#if DEBUG
  if (debug_level >= 3) {
    if (C_dialect == C_dialect_cplusplus) db_base_class_list(class_type);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* finish_laying_out_class */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
