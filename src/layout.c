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

*/

#include "basics.h"
#include "layout.h"
#include "class_decl.h"
#include "decls.h"
#include "il.h"
#include "error.h"
#include "cmd_line.h"
#include "types.h"
#include "target.h"

void clear_layout_block(a_layout_block_ptr  lob,
                        a_type_ptr          class_type)
{
  lob->class_type = class_type;
  lob->byte_offset = 0;
  lob->bit_offset = 0;
  lob->alignment = 1;
  lob->any_overflow = FALSE;
}  /* clear_layout_block */


static void check_enum_type_for_bit_field(a_type_ptr bit_field_type,
                                          long       bit_field_size,
                                          a_boolean *need_signed_type)
/*
Check to see that the values of the enumerated type bit_field_type will all
fit in a bit field of size bit_field_size.  If not, give a warning.  Return
*need_signed_type TRUE if the bit field type must be signed, FALSE if it
must be unsigned.
*/
{
  long           smallest, largest, smallest_possible, largest_possible,
                 enum_val;
  a_constant_ptr enum_con;
  a_boolean      use_signed;

  /* Check the constants on the list.  Start by finding the largest and
     smallest constants.  All constants are "int", so no consideration of
     unsigned constants is necessary.  We are assuming most enum type lists
     won't be too long, and there won't be too many bit fields with enum
     type, so a linear search should be acceptable.  Furthermore,
     the usual case is that the bit field is big enough, so we're probably
     going to scan the whole constant list; therefore it's okay to always
     scan the whole list even though some errors could be detected during
     the scan. */
  smallest = largest = 0;
  for (enum_con = bit_field_type->variant.integer.enum_constant_list;
       enum_con != NULL;
       enum_con = enum_con->next) {
    enum_val = enum_con->variant.integer_value;
    if (enum_val < smallest) smallest = enum_val;
    if (enum_val >  largest) largest  = enum_val;
  }  /* for */
  /* Determine the proper signedness for the bit field.  One can't
     simply use the signedness of the enum type, since that was chosen
     for efficiency reasons: if the enum values just fit in the bit
     field size, an unsigned field might be necessary even though a 
     signed type was a good choice for the enum type. */
  if (smallest < 0) {
    /* Some enum values are negative, so a signed type is required.
       The enum type must already be signed. */
    use_signed = TRUE;
  } else if ((((unsigned long)1 << (bit_field_size-1)) & largest) &&
             largest >= 0) {
    /* The largest value is positive, and it's so big it would require
       a "1" in the sign bit.  Therefore, an unsigned type is required. */
    use_signed = FALSE;
  } else {
    /* The signedness is not forced by the enum values, so use the 
       target preference.  Make a one-bit field always unsigned. */
    use_signed = (bit_field_size != 1 &&
                  !(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED));
  }  /* if */
  /* Make the largest and smallest values that will fit in the
     bit field, given the signedness selected. */
  /* Make the largest possible signed value, i.e., all 1's below the
     sign bit. */
  largest_possible = ((unsigned long)1 << (bit_field_size-1)) - 1;
  if (use_signed) {
    /* Signed bit field. */
    smallest_possible = ~largest_possible;
  } else {
    /* Unsigned bit field. */
    smallest_possible = 0;
    /* Turn "all 1's below the sign bit" into "all 1's including the sign 
       bit".  Watch out for integer overflow. */
    largest_possible = ((unsigned long)largest_possible << 1) | 1;
  }  /* if */
  /* Check that the enum values will fit in the bit field. */
  if (smallest < smallest_possible || largest > largest_possible) {
    warning(ec_enum_bit_field_too_small);
  }  /* if */
  *need_signed_type = use_signed;
}  /* check_enum_type_for_bit_field */


/* A value used by class declaration processing only to represent the
   size of a an unnamed bit field with a declared length of zero. */
#define UNNAMED_ZERO_LENGTH_BIT_FIELD_SIZE (TARG_MAX_BIT_FIELD_SIZE + 1)


void scan_bit_field_size(a_boolean  unnamed_bit_field,
                         a_type_ptr *p_base_type,
                         long       *p_bit_field_size)
/*
Scan the size in a bit-field declaration:

    unsigned int j: 5 ;
                    ^---- this size.

The current token is the colon preceding the size.  If unnamed_bit_field
is TRUE, the bit-field is unnamed.  *p_base_type gives the base type
of the declaration (unsigned int in the above example); it may be updated
on return.  *p_bit_field_size is set to the bit field size in bits.
*/
{
  long       bit_field_size;
  a_type_ptr base_type = *p_base_type;
  a_constant constant;
  a_type_ptr bit_field_type;

  /* Bit field.  ANSI says the type of a bit-field must be int, unsigned int,
     or signed int, but we also allow enums and integral types (see A.6.5.8
     in the Common Extensions appendix).  pcc allows those same things, so
     the ANSI and pcc behaviors are the same. */
  bit_field_type = skip_typerefs(base_type);
  if (!is_integral_type(bit_field_type)) {
    /* Error, not an integral type. */
    if (!is_error_type(bit_field_type)) error(ec_bad_bit_field_type);
    bit_field_type = integer_type((an_integer_kind)ik_int);
  } else {
    /* Integral base type.  In strict ANSI mode, give a warning about a
       nonstandard base type (anything other than int, unsigned int, and
       signed int).  In C++, however, any integer type is allowed (ARM 9.6). */
    if (C_dialect != C_dialect_cplusplus && strict_ansi_mode) {
      if (bit_field_type->variant.integer.enum_type ||
          (bit_field_type->variant.integer.int_kind !=
                                                     (an_integer_kind)ik_int &&
           bit_field_type->variant.integer.int_kind !=
                                           (an_integer_kind)ik_unsigned_int)) {
        warning(ec_nonstd_bit_field_type);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Advance past the colon. */
  (void)get_token();
  /* Scan the integral size in bits of the bit-field. */
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
    /* Use small value to avoid more errors, but not 1 which is special. */
    bit_field_size = TARG_CHAR_BIT;
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("scan_bit_field_size: size not int");
    }  /* if */
#endif /* CHECKING */
    /* The size of the bit field must be non-negative and must not exceed
       the size of the underlying type (except for enums, whose type was
       picked by the front end) or the target maximum bit field size.
       Note that this also catches very large unsigned values (they look
       negative). */
    bit_field_size = constant.variant.integer_value;
    if (bit_field_size < 0 ||
        (!bit_field_type->variant.integer.enum_type &&
         bit_field_size > (bit_field_type->size*TARG_CHAR_BIT))) {
      error(ec_bad_bit_field_size);
      bit_field_size = bit_field_type->size*TARG_CHAR_BIT;
    } else if (bit_field_size > TARG_MAX_BIT_FIELD_SIZE) {
      error(ec_bad_bit_field_size);
      bit_field_size = TARG_MAX_BIT_FIELD_SIZE;
    } else if (bit_field_size == 0) {
      /* The bit-field size is zero, so the field must be unnamed. */
      if (!unnamed_bit_field) {
        error(ec_zero_length_bit_field_must_be_unnamed);
        bit_field_size = 1;
      } else {
        /* Use a special value other than zero for the size of an unnamed
           zero length bit field.  This is required for distinguishing a
           field entry of type bit_field_type representing a zero length
           bit field from a field entry for an ordinary field of the same
           type that is unnamed (an extension). */
        bit_field_size = UNNAMED_ZERO_LENGTH_BIT_FIELD_SIZE;
      }  /* if */
    } else if (bit_field_type->variant.integer.enum_type) {
      /* The integral type is an enum type.  Give a warning if any of the
         enumeration's constants will not fit in the bit field, and determine
         whether the bit field should be signed or unsigned. */
      a_boolean need_signed_type;
      check_enum_type_for_bit_field(bit_field_type, bit_field_size,
                                    &need_signed_type);
      /* Change the base type for the bit field if necessary to get the
         right signedness. */
      if (need_signed_type !=
                int_kind_is_signed(bit_field_type->variant.integer.int_kind)) {
        /* Change to a signed or unsigned int type with the enum type
           indicated in it.  Note that this is a new and unshared type. */
        a_type_ptr new_enum_type = alloc_type((a_type_kind)tk_integer);
        new_enum_type->variant.integer.int_kind =
                           need_signed_type ? (an_integer_kind)ik_int :
                                              (an_integer_kind)ik_unsigned_int;
        new_enum_type->variant.integer.enum_type = TRUE;
        new_enum_type->variant.integer.enum_constant_list =
                            bit_field_type->variant.integer.enum_constant_list;
        set_type_size(new_enum_type);
        bit_field_type = new_enum_type;
      }  /* if */
    } else if (bit_field_type->variant.integer.explicitly_signed) {
      /* The integral type was explicitly signed in the source.  (This
         information comes from the type entry itself.)  The integral type
         stays what it is (specifically, "int" stays "int" and therefore
         signed).  Give a warning for an explicitly signed one-bit field;
         ANSI allows it, but it's strange. */
      if (bit_field_size == 1) warning(ec_signed_one_bit_field);
    } else if (bit_field_type->variant.integer.int_kind ==
                                                     (an_integer_kind)ik_int) {
      /* The integral type is a "plain" int, i.e., it's int, it's not
         explicitly signed, and it's not an enum type.  This is converted
         to the target preference with regard to signedness.  A one-bit field
         is probably not intended to be signed, so make it unsigned. */
      if (bit_field_size == 1 || TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED) {
        bit_field_type =integer_type((an_integer_kind)ik_unsigned_int);
      }  /* if */
    }  /* if */
  }  /* if */
  if (bit_field_type == skip_typerefs(base_type)) {
    /* The original type, base_type, has turned out to be correct after all.
       Use it directly to avoid wasting the type qualifiers, if any. */
  } else {
    /* Build a type with the right qualifiers. */
    base_type = make_qualified_type(bit_field_type,
                                    is_const_qualified_type(base_type),
                                    is_volatile_qualified_type(base_type));
  }  /* if */
  *p_base_type = base_type;
  *p_bit_field_size = bit_field_size;
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


#if TARG_BIT_FIELD_CONTAINER_SIZE >= 0
/*ARGSUSED*/ /* <-- base_type is not used. */
#endif /* TARG_BIT_FIELD_CONTAINER_SIZE >= 0 */
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
error -- TARG_BIT_FIELD_CONTAINER_SIZE in target.h is set wrong.
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
    /* Remember the most stringent alignment requirement as the alignment
       requirement for the overall struct. */
    if (field_alignment > *p_alignment) {
      *p_alignment = field_alignment;
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
      field->bit_offset = (save_byte_offset * TARG_CHAR_BIT) + save_bit_offset;
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* set_field_size_and_offset */


void set_offsets_for_nonvirtual_base_classes(a_layout_block_ptr  lob)
/*
Lay out the class_type object to store the nonvirtual direct base classes.
(They will precede the fields of the current class.)  Do this by going
through the base classes list for the current class and reserving enough
space for each base class that is directly and nonvirtually inherited;
storage is reserved in the order in which the base classes appear.
(Virtual base classes, both direct and indirect, are dealt with after the
fields of the current class are allocated; indirect nonvirtual base classes
are just subobjects of the direct nonvirtual base classes.)  If *any_overflow
is TRUE upon entry, nothing is done; if overflow is detected in computing
offsets and alignments, *any_overflow is set and returned to the caller.
*/
{
  a_targ_size_t     size;
  a_targ_alignment  alignment;
  a_base_class_ptr  bcp;
  
  db_enter(4, "set_offsets_for_nonvirtual_base_classes");
  /* Traverse the list of base classes. */
  bcp = lob->class_type->variant.class_struct_union.extra_info->base_classes;
  if (bcp != NULL) {
    for (; bcp != NULL; bcp = bcp->next) {
      if (!lob->any_overflow && bcp->direct && !bcp->is_virtual) {
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
        if (bcp->complete_subobject) {
          alignment = bcp->type->alignment;
          size = bcp->type->size;
        } else {
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
          /* For a nonvirtual base classes reserve space for all the base class
             fields, excluding space required for its own virtual base classes.
             The latter will be added at the end of the storage. */
          alignment = bcp->type->variant.class_struct_union.extra_info->
                                      alignment_without_virtual_base_classes;
          size = bcp->type->variant.class_struct_union.extra_info->
                                      size_without_virtual_base_classes;
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
        }  /* if */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
        /* Adjust the current offset to ensure that the base class is
           properly aligned. */
        if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
          error(ec_struct_too_large);
          lob->any_overflow = TRUE;
        } else {
          /* No error, so increment the offset to allow for the base class. */
          bcp->offset = lob->byte_offset;
          if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                       size, 0)) {
            error(ec_struct_too_large);
            lob->any_overflow = TRUE;
          }  /* if */
          /* Adjust the overall alignment requirement for the current class, if
             necessary. */
          if (lob->alignment < alignment) {
            lob->alignment = alignment;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* set_offsets_for_nonvirtual_base_classes */


static void set_offset_for_virtual_function_info(a_layout_block_ptr  lob)
/*
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions this routine allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by constants that can
be redefined for various implementation strategies.  If *any_overflow
is TRUE upon entry, nothing is done; if overflow is detected in computing
offsets and alignments, *any_overflow is set and returned to the caller.
*/
{
  a_class_type_supplement_ptr	ctsp;

  db_enter(4, "set_offset_for_virtual_function_info");
  ctsp = lob->class_type->variant.class_struct_union.extra_info;
  if (!lob->any_overflow && ctsp->virtual_function_count > 0) {
    if (!do_alignment(&lob->byte_offset, &lob->bit_offset,
                      TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO)) {
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    } else {
      ctsp->virtual_function_info_offset = lob->byte_offset;
      if (lob->alignment < TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) {
        lob->alignment = TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO;
      }  /* if */
      if (!increment_field_offsets(
                        &lob->byte_offset, &lob->bit_offset,
                        (a_targ_size_t)TARG_SIZEOF_VIRTUAL_FUNCTION_INFO, 0)) {
        error(ec_struct_too_large);
        lob->any_overflow = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_offset_for_virtual_function_info */




#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
static void pointer_offset_for_virtual_base_class(a_layout_block_ptr  lob,
                                                  a_base_class_ptr    bcp)
{
  a_targ_size_t      size;
  a_targ_alignment   alignment;

#if TARG_ALL_POINTERS_SAME_SIZE
  /* All pointers are the same size. */
  alignment = (a_targ_alignment)TARG_ALIGNOF_POINTER;
  size = (a_targ_size_t)TARG_SIZEOF_POINTER;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
??=error set_offsets_for_virtual_base_class_pointers: different sized pointers
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
  /* Adjust the current offset to ensure that the base class is properly
     aligned. */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
    error(ec_struct_too_large);
    lob->any_overflow = TRUE;
  } else {
    /* No error, so increment the offset. */
    bcp->pointer_offset = lob->byte_offset;
    if (lob->alignment < alignment) {
      lob->alignment = alignment;
    }  /* if */
    if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                 size, 0)) {
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
}  /* pointer_offset_for_virtual_base_class */


static a_boolean is_best_derivation(a_base_class_ptr  bcp,
                                    a_base_class_ptr  derived_bcp,
                                    a_type_ptr        class_type)
{
  a_boolean              is_best_path;
  a_derivation_step_ptr  step;

  if (derived_bcp == NULL) {
#if CHECKING
    if (!bcp->direct) {
      internal_error(
                 "is_best_derivation: no derived_bcp for indirect base class");
    }  /* if */
#endif /* CHECKING */
    is_best_path = TRUE;
  } else {
    bcp = corresponding_base_class(bcp, (a_type_ptr)NULL, class_type);
    if (bcp->direct) {
      is_best_path = FALSE;
    } else {
      derived_bcp = corresponding_base_class(derived_bcp, (a_type_ptr)NULL,
                                             class_type);
#if 0
      /* Checking based on the derivation path is not really right.  If the
         need arises we'll have to beef this up. */
#endif /* if 0 */
      step = bcp->derivation;
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
    }  /* if */
  }  /* if */
  return is_best_path;
}  /* is_best_derivation */


static void set_offsets_for_virtual_base_class_pointers(
                                            a_layout_block_ptr lob,
                                            a_base_class_ptr   base_class,
                                            a_boolean          use_decl_order);

static void set_pointer_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
/*
*/
{
  a_base_class_ptr  bcp;

  db_enter(4, "set_pointer_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (base_class->direct) {
      if (base_class->is_virtual) {
        if (!use_decl_order) {
          set_pointer_offsets_for_corresponding_virtual_base_classes(
                                 lob, base_class->next, use_decl_order,
                                 derived_bcp);
        }  /* if */
        bcp = corresponding_base_class(base_class,
                                       /*old_type=*/(a_type_ptr)NULL,
                                       lob->class_type);
        if (bcp->pointer_base_class == NULL && bcp->pointer_offset == 0 &&
            !lob->any_overflow &&
            is_best_derivation(bcp, derived_bcp, lob->class_type)) {
          pointer_offset_for_virtual_base_class(lob, bcp);
          set_offsets_for_virtual_base_class_pointers(lob, bcp,
                                                      !use_decl_order);
        }  /* if */
        if (!use_decl_order) break;
      } else {
        set_offsets_for_virtual_base_class_pointers(lob, base_class,
                                                    !use_decl_order);
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_pointer_offsets_for_corresponding_virtual_base_classes */


static a_boolean has_virtual_base_class_with_null_pointer_base_class(
                                                  a_base_class_ptr  base_class,
                                                  a_type_ptr        class_type)
{
  a_base_class_ptr  bcp, corresp_bcp;
  a_boolean         has_one = FALSE;

  for (bcp = base_class->type->
                          variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual) {
      corresp_bcp = corresponding_base_class(bcp, (a_type_ptr)NULL,
                                             class_type);
      if (corresp_bcp->pointer_base_class == NULL) {
        has_one = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return has_one;
}  /* has_virtual_base_class_with_null_pointer_base_class */


  
static void set_pointer_offset_for_direct_virtual_base_class(
                                            a_layout_block_ptr lob,
                                            a_base_class_ptr   base_class,
                                            a_boolean          use_decl_order);

static void check_direct_virtual_base_classes_for_special_case(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   bcp,
                                             a_boolean          use_decl_order,
                                             a_base_class_ptr   derived_bcp)
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
{
  a_base_class_ptr  bcp;

  bcp = corresponding_base_class(base_class, (a_type_ptr)NULL,
                                 lob->class_type);
  pointer_offset_for_virtual_base_class(lob, bcp);
  bcp = base_class->type->variant.class_struct_union.extra_info->base_classes;
  check_direct_virtual_base_classes_for_special_case(lob, bcp, !use_decl_order,
                                                     base_class);
  bcp = base_class->type->variant.class_struct_union.extra_info->base_classes;
  set_pointer_offsets_for_corresponding_virtual_base_classes(lob, bcp,
                                                             use_decl_order,
                                                             base_class);
}  /* set_pointer_offsets_for_direct_virtual_base_class */


static void set_offsets_for_virtual_base_class_pointers(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
*/
{
  a_base_class_ptr  base_class_list, bcp;

  db_enter(4, "set_offsets_for_virtual_base_class_pointers");
  if (base_class == NULL) {
    base_class_list =
          lob->class_type->variant.class_struct_union.extra_info->base_classes;
    use_decl_order = TRUE;
    for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct && bcp->is_virtual && bcp->pointer_base_class == NULL &&
          has_virtual_base_class_with_null_pointer_base_class(
                                                      bcp, lob->class_type)) {
        set_pointer_offset_for_direct_virtual_base_class(lob, bcp,
                                                         use_decl_order);
      }  /* if */
    }  /* for */
    set_pointer_offsets_for_corresponding_virtual_base_classes(
                                      lob, base_class_list,
                                      !use_decl_order, (a_base_class_ptr)NULL);
  } else {
    base_class_list =
         base_class->type->variant.class_struct_union.extra_info->base_classes;
    set_pointer_offsets_for_corresponding_virtual_base_classes(
                                                  lob, base_class_list,
                                                  !use_decl_order, base_class);
  }  /* if */
  db_exit();
}  /* set_offsets_for_virtual_base_class_pointers */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */


#if 0
static void set_offsets_for_virtual_base_class_pointers(
                                             a_base_class_ptr  base_class_list,
                                             a_targ_size_t     *p_byte_offset,
                                             int               *p_bit_offset,
                                             a_targ_alignment  *p_alignment,
                                             a_boolean         *any_overflow)
/*
Set the pointer_offset fields in direct virtual base classes where the pointer
is not shared (i.e., where the pointer from a base class is not used).
*/
{
  a_targ_size_t      size;
  a_targ_alignment   alignment;
  a_base_class_ptr   bcp;
  
  db_enter(4, "set_offsets_for_virtual_base_class_pointers");
  /* Traverse the list of base classes. */
  for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
    /* For virtual base classes we only reserve enough space for a pointer
       to the actual data section.  The latter is added at the end of the
       storage. */
    /* Only pointers to direct virtual base classes need space reserved --
       and only when the pointer is not shared, i.e., not already present in
       the data section of another base class, as indicated by the
       pointer_base_class field. */
    if (!*any_overflow && bcp->is_virtual && bcp->pointer_base_class == NULL) {
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
      /* Cfront puts out the pointers to virtual base class data sections in
         reverse declaration order.  This is emulated by a recursive call
         to allocate space for the next one before doing the current one. */
      if (bcp->next != NULL) {
        set_offsets_for_virtual_base_class_pointers(bcp->next, p_byte_offset,
                                                    p_bit_offset, p_alignment,
                                                    any_overflow);
      }  /* if */
      if (*any_overflow) break;
#else
      if (!bcp->direct) continue;
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
      pointer_offset_for_virtual_base_class(bcp, p_byte_offset, p_bit_offset,
                                            p_alignment, any_overflow);
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
      break;
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_virtual_base_class_pointers */
#endif /* if 0 */


#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
static void set_embedded_virtual_base_class_offset(a_base_class_ptr base_class,
                                                   a_type_ptr       class_type)
/*
base_class is a direct or indirect virtual base class of class_type.  If
it is allocated inside another base class, compute its offset within the layout
for class_type.  Then do the same check for its own direct virtual base
classes.
*/
{
  a_base_class_ptr  data_section_bcp, bcp;

  if (base_class->offset == 0) {
    /* Offset has not yet been set. */
    data_section_bcp = base_class->data_section_base_class;
    if (data_section_bcp != NULL) {
      if (data_section_bcp->is_virtual &&
          data_section_bcp->data_section_base_class != NULL &&
          data_section_bcp->offset == 0) {
        set_embedded_virtual_base_class_offset(data_section_bcp, class_type);
      }  /* if */
      /* Look for the corresponding virtual base class. */
      bcp = corresponding_base_class(base_class, class_type,
                                     data_section_bcp->type);
      /* The pointer_offset value in the context of the derived class
         is the offset of the pointer base class plus the offset of the
         virtual base class pointer within the latter. */
      base_class->offset = bcp->offset + data_section_bcp->offset;
    }  /* if */
  }  /* if */
  /* Apply the check recursively. */
  bcp = base_class->type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->is_virtual && bcp->direct) {
      set_embedded_virtual_base_class_offset(
                  corresponding_base_class(bcp, base_class->type, class_type),
                  class_type);
    }  /* if */
  }  /* for */
}  /* set_embedded_virtual_base_class_offset */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */


static void fixup_shared_virtual_base_class_offsets(a_type_ptr  class_type)
/*
Set the pointer_offset fields in direct virtual base classes where the
virtual base class pointer is shared with some other base class.
*/
{
  a_base_class_ptr             virtual_base_class;
  a_base_class_ptr             pointer_base_class;
  a_base_class_ptr             bcp;

  /* Make a pass over all the base classes for the current derived class and
     check each virtual base class. */
  for (virtual_base_class = class_type->variant.
                                  class_struct_union.extra_info->base_classes;
       virtual_base_class != NULL;
       virtual_base_class = virtual_base_class->next) {
    if (virtual_base_class->is_virtual) {
#if !CFRONT_CLASS_LAYOUT_COMPATIBILITY
      if (!virtual_base_class->direct) {
        /* Pointer base class is defined only for direct virtual base classes,
           but it may have been set tentatively before we knew it was not a
           direct base class. */
        virtual_base_class->pointer_base_class = NULL;
        continue;
      }  /* if */
#endif /* !CFRONT_CLASS_LAYOUT_COMPATIBILITY */
      /* If the pointer_base_class field is non-NULL, the virtual base class
         pointer for the derived class is the same as the pointer to the
         corresponding virtual base class for pointer_base_class. */
      pointer_base_class = virtual_base_class->pointer_base_class;
      if (pointer_base_class != NULL) {
        /* Look for the corresponding virtual base class. */
        bcp = corresponding_base_class(virtual_base_class, class_type,
                                       pointer_base_class->type);
        /* The pointer_offset value in the context of the derived class
           is the offset of the pointer base class plus the offset of the
           virtual base class pointer within the latter. */
        virtual_base_class->pointer_offset = bcp->pointer_offset +
                                                  pointer_base_class->offset;
      }  /* if */
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
      set_embedded_virtual_base_class_offset(virtual_base_class, class_type);
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
    }  /* if */
  }  /* for */
}  /* fixup_shared_virtual_base_class_offsets */


#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
static void set_offsets_for_corresponding_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
*/
{
  a_targ_size_t     size;
  a_targ_alignment  alignment;
  a_base_class_ptr  bcp;

  db_enter(4, "set_offsets_for_corresponding_virtual_base_classes");
  for (; base_class != NULL; base_class = base_class->next) {
    if (base_class->is_virtual && base_class->direct &&
        base_class->data_section_base_class == NULL) {
      if (!use_decl_order) {
        set_offsets_for_corresponding_virtual_base_classes(
                                        lob, base_class->next, use_decl_order);
      }  /* if */
      bcp = corresponding_base_class(base_class, /*old_type=*/(a_type_ptr)NULL,
                                     lob->class_type);
      if (bcp->data_section_base_class == NULL && bcp->offset == 0 &&
          !lob->any_overflow) {
#if CHECKING
        /* All virtual base classes should be marked as "complete
           subobjects". */
        if (!bcp->complete_subobject) {
          internal_error("set_offsets_for_corresp...: not complete subobj");
        }  /* if */
#endif /* CHECKING */
        alignment = bcp->type->alignment;
        size = bcp->type->size;
        if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
          error(ec_struct_too_large);
          lob->any_overflow = TRUE;
          break;
        } else {
          /* Record the current offset in the data_section_offset of the
             virtual base class entry.  This allows for direct access of
             its fields (rather than through a pointer) as an optimization
             under certain circumstances. */
#if CHECKING
          if (lob->byte_offset == 0) {
            internal_error("set_offsets_for_corresp...: zero offset");
          }  /* if */
#endif /* CHECKING */
          bcp->offset = lob->byte_offset;
          if (lob->alignment < alignment) lob->alignment = alignment;
          if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                       size, 0)) {
            error(ec_struct_too_large);
            lob->any_overflow = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!use_decl_order) break;
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_corresponding_virtual_base_classes */


static void set_offsets_for_indirect_virtual_base_classes(
                                             a_layout_block_ptr lob,
                                             a_base_class_ptr   base_class,
                                             a_boolean          use_decl_order)
/*
base_class is a direct or indirect base class of class_type for which
the complete_subobject flag is FALSE and whose virtual base classes, therefore,
may space reserved in the compete derived class.  Examine the direct
virtual base classes of base_class, find the corresponding indirect virtual
base class of class_type, and allocate space for the latter.
*/
{
  a_base_class_ptr  base_class_list, bcp;

  db_enter(4, "set_offsets_for_indirect_virtual_base_classes");
  if (base_class == NULL) {
    base_class_list = lob->class_type->
                           variant.class_struct_union.extra_info->base_classes;
  } else {
    base_class_list = base_class->type->
                           variant.class_struct_union.extra_info->base_classes;
  }  /* if */
  if (use_decl_order) {
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  for (bcp = base_class_list; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && !bcp->complete_subobject) {
      set_offsets_for_indirect_virtual_base_classes(lob, bcp, !use_decl_order);
      break;
    }  /* if */
  }  /* for */
  if (!use_decl_order) {
    set_offsets_for_corresponding_virtual_base_classes(lob, base_class_list,
                                                       use_decl_order);
  }  /* if */
  db_exit();
}  /* set_offsets_for_indirect_virtual_base_classes */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */


static void set_offsets_for_virtual_base_classes(a_layout_block_ptr  lob)
/*
Reserve space at the end of the class object for virtual base classes.
*p_byte_offset and *p_bit_offset represent the space already reserved; they
are updated to reflect the increase in the size of the object when the
virtual base classes are added.  *p_alignment is the overall alignment
required for the object -- i.e., the maximum alignment required for any of
its components; it too is updated if the alignment required for one of the
virtual subobjects exceeds the current maximum.  *any_overflow is TRUE if
the object has already been reported to be too large.
*/
{
  a_class_type_supplement_ptr	ctsp;
  
  db_enter(4, "set_offsets_for_virtual_base_classes");

  ctsp = lob->class_type->variant.class_struct_union.extra_info;
  /* Record the size and alignment of the class before space is added for
     virtual base classes. */
  /* The size will never be set to zero, even if the class's actual size
     is zero.  This is to assure that it will be given a unique location
     when incorporated as a subobject of some other class. */
  if (lob->byte_offset == 0) lob->byte_offset = 1;
  /* We add padding (if needed) to the part of the object preceding
     the virtual base classes based on the alignment requirements for
     the portion of the class processed thus far.  In odd cases this
     can result in suboptimal packing, but it permits treating the
     class-without-virtual-base-classes as a subobject whose size is
     consistent with its alignment. */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, lob->alignment)) {
    if (!lob->any_overflow) {
      error(ec_struct_too_large);
      lob->any_overflow = TRUE;
    }  /* if */
  }  /* if */
  ctsp->size_without_virtual_base_classes = lob->byte_offset;
  ctsp->alignment_without_virtual_base_classes = lob->alignment;
  if (lob->class_type->variant.class_struct_union.any_virtual_base_classes &&
      !lob->any_overflow) {
#if CFRONT_CLASS_LAYOUT_COMPATIBILITY
    set_offsets_for_indirect_virtual_base_classes(lob, (a_base_class_ptr)NULL,
                                                  /*use_decl_order=*/FALSE);
#else
    a_base_class_ptr   bcp = ctsp->base_classes;

    if (bcp != NULL) {
      /* Now add the virtual base classes to the storage.  This is done
         almost exactly as for nonvirtual base classes. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual) {
          a_targ_size_t     size;
          a_targ_alignment  alignment;

          alignment = bcp->type->variant.class_struct_union.extra_info->
                                        alignment_without_virtual_base_classes;
          size = bcp->type->variant.class_struct_union.extra_info->
                                        size_without_virtual_base_classes;
          if (!do_alignment(&lob->byte_offset, &lob->bit_offset, alignment)) {
            error(ec_struct_too_large);
            lob->any_overflow = TRUE;
            break;
          } else {
            /* Record the current offset in the data_section_offset of the
               virtual base class entry.  This allows for direct access of
               its fields (rather than through a pointer) as an optimization
               under certain circumstances. */
            bcp->offset = lob->byte_offset;
            if (lob->alignment < alignment) {
              lob->alignment = alignment;
            }  /* if */
            if (!increment_field_offsets(&lob->byte_offset, &lob->bit_offset,
                                         size, 0)) {
              error(ec_struct_too_large);
              lob->any_overflow = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* CFRONT_CLASS_LAYOUT_COMPATIBILITY */
  }  /* if */
  db_exit();
}  /* set_offsets_for_virtual_base_classes */


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


static void set_base_class_offsets(a_base_class_ptr       proximate_derivation,
                                   a_base_class_ptr       list_to_search,
                                   a_derivation_step_ptr  path,
                                   a_derivation_step_ptr  end_of_path)
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

Suppose that proximate_derivation is the D::B base class entry.
list_to_search includes all 4 base classes from the most derived class.
path will be simply ==>B, and end_of_path will be the same.  The algorithm
involves going through B's direct base classes (there's only one in this
case), finding the corresponding base class entry on list_to_search, and
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
  ref_bcp = proximate_derivation->type->
                variant.class_struct_union.extra_info->base_classes;
#if DEBUG
  if (debug_level >= 4) {
    if (ref_bcp != NULL) {
      fputs("root base class ", f_debug);
      db_name(&proximate_derivation->type->source_corresp);
      fprintf(f_debug, " at offset %ld\n", proximate_derivation->offset);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the reference base classes, the direct base classes of
     the proximate_derivation base class. */
  for (; ref_bcp != NULL; ref_bcp = ref_bcp->next) {
    if (ref_bcp->direct) {
#if DEBUG
      if (debug_level >= 4) {
        fputs("reference base class ", f_debug);
        db_name(&ref_bcp->type->source_corresp);
        fprintf(f_debug, " at offset %ld\n", ref_bcp->offset);
      }  /* if */
#endif /* DEBUG */
      /* Look for a match in the base classes list of the most derived
         class. */
      for (bcp = list_to_search; bcp != NULL; bcp = bcp->next) {
        /* Look for an indirect base class that is of the same type. */
        if (!bcp->direct && bcp->type == ref_bcp->type &&
            bcp->is_virtual == ref_bcp->is_virtual) {
          /* We seem to have found a match, but we have to check the path
             as well as the type (e.g., to distinguish B::A from C::A in the
             example above. */
          end_of_path->next = make_derivation_step(bcp,
                                                  (a_derivation_step_ptr)NULL);
          if (congruent_paths(path, bcp->derivation)) {
            /* It is a match.  If this is a virtual base class, it is the
               pointer_offset field that needs to be updated. */
            if (!bcp->is_virtual) {
              bcp->offset = proximate_derivation->offset + ref_bcp->offset;
#if !CFRONT_CLASS_LAYOUT_COMPATIBILITY
            } else {
              bcp->pointer_offset = proximate_derivation->offset +
                                    ref_bcp->pointer_offset;
#endif /* !CFRONT_CLASS_LAYOUT_COMPATIBILITY */
            }  /* if */
#if DEBUG
            if (debug_level >= 4) {
              fputs("base class ", f_debug);
              db_name(&bcp->type->source_corresp);
              fprintf(f_debug, ": setting %soffset to %ld\n",
                               bcp->is_virtual ? "pointer " : "",
                               bcp->is_virtual ? bcp->pointer_offset :
                                                 bcp->offset);
            }  /* if */
#endif /* DEBUG */
            /* Make a recursive call to apply this processing to the next
               level of base classes. */
            set_base_class_offsets(bcp, list_to_search,
                                   path, end_of_path->next);
          }  /* if */
          free_derivation_step(end_of_path->next);
          end_of_path->next = NULL;
          break;
        }  /* if */
      }  /* for */
#if CHECKING
      if (bcp == NULL && !ref_bcp->is_virtual) {
        /* Normal exit from loop means no match was found.  This is only
           possible when we're dealing with virtual base classes. */
        internal_error(
                 "set_base_class_offsets: no base class matches reference");
      }  /* if */
#endif /* CHECKING */
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
  a_base_class_ptr      bcp, bcp_list;
  a_derivation_step_ptr end_of_path;

  db_enter(4, "set_offsets_for_indirect_base_classes");
  bcp_list = class_type->variant.class_struct_union.extra_info->base_classes;
#if DEBUG
  if (debug_level >= 4) {
    if (bcp_list != NULL) {
      fputs("before setting offsets: ", f_debug);
      db_base_class_list(class_type);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the list of base classes, direct and indirect, that are
     defined for the class, but ignore all but the direct base classes.  The
     rest are handled by recursively scanning the base class tree. */
  for (bcp = bcp_list; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      end_of_path = bcp->derivation;
      /* We need to search for the end of the path (rather than assume, as
         is generally the case, that direct base classes have one-step
         derivations, to cover the case of virtual base classes that are
         more accessible along an indirect than the direct path. */
      while (end_of_path->next != NULL) end_of_path = end_of_path->next;
      set_base_class_offsets(bcp, bcp_list, bcp->derivation, end_of_path);
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_indirect_base_classes */


void finish_laying_out_class(a_layout_block_ptr  lob)
/*
Complete laying out the object specified by the layout block's class_type.
This includes allocating any remaining fields whose allocation may have been
delayed and making room for virtual base classes, which appear at the end of
the layout.
*/
{
  a_type_ptr        class_type = lob->class_type;
#if 0
  a_base_class_ptr  base_class_list;
#endif /* if 0 */

  db_enter(3, "finish_laying_out_class");
  if (C_dialect == C_dialect_cplusplus) {
#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
    /* Only public fields were given an offset in decl_nonstatic_data_member.
       Now that all the fields have been seen and added to the class's field
       list, traverse the field list again and allocate protected and private
       fields. */
    set_offsets_for_remaining_fields(lob);
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
    set_offsets_for_virtual_base_class_pointers(lob, (a_base_class_ptr)NULL,
                                                /*use_decl_order=*/FALSE);
#if 0
    base_class_list = class_type->
                          variant.class_struct_union.extra_info->base_classes;
    set_offsets_for_virtual_base_class_pointers(base_class_list, &byte_offset,
                                                &bit_offset, &alignment,
                                                &any_overflow);
#endif /* if 0 */
    set_offset_for_virtual_function_info(lob);
    set_offsets_for_virtual_base_classes(lob);
  }  /* if */
  if (!do_alignment(&lob->byte_offset, &lob->bit_offset, lob->alignment)) {
    if (!lob->any_overflow) error(ec_struct_too_large);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    set_offsets_for_indirect_base_classes(class_type);
    fixup_shared_virtual_base_class_offsets(class_type);
  }  /* if */
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
