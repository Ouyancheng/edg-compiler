/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

types.c -- Utility routines that check types.

*/

#include "basics.h"
#include "il.h"
#include "error.h"
#include "target.h"
#include "types.h"
#include "symbol_tbl.h"
#include "cmd_line.h"
#include "mem_manage.h"


/*
Macros defining some basic classes of types (see 3.1.2.5).  Defined as
macros to avoid many levels of function calls in evaluating these
predicates.
*/
/* The error type simply has type error. */
#define is_error(tp) ((tp)->kind == (a_type_kind)tk_error)

/* Function types are simply function types. */
#define is_function(tp) ((tp)->kind == (a_type_kind)tk_routine)

/* Object types are non-function types that have sizes. */
#define is_object(tp) (!is_function(tp) && (tp)->size != 0)

/* Incomplete types are types that have no size and are neither functions
   nor references. */
#define is_incomplete(tp) \
  (!is_function(tp) && !is_reference(tp) && (tp)->size == 0)

/* The void type is simply the void type. */
#define is_void(tp) ((tp)->kind == (a_type_kind)tk_void)

/* Integral types comprise char, the signed and unsigned integer types,
   and enumerated types. */
#define is_integral(tp) ((tp)->kind == (a_type_kind)tk_integer)

/* Character types are three particular integral types. */
#define is_character(tp) \
  (is_integral(tp) && \
   ((tp)->variant.integer.int_kind == (an_integer_kind)ik_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_unsigned_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_signed_char))

/* The floating types comprise all sizes of float. */
#define is_floating(tp) ((tp)->kind == (a_type_kind)tk_float)

/* Arithmetic types are the integral types plus the floating types. */
#define is_arithmetic(tp) (is_integral(tp) || is_floating(tp))

/* The pointer type is simply the pointer type. */
#define is_pointer(tp) ((tp)->kind == (a_type_kind)tk_pointer)

/* The reference type is simply the reference type. */
#define is_reference(tp) ((tp)->kind == (a_type_kind)tk_reference)

/* Scalar types are the arithmetic types plus the pointer types. */
#define is_scalar(tp) (is_arithmetic(tp) || is_pointer(tp))

/* Array types are simply array types. */
#define is_array(tp) ((tp)->kind == (a_type_kind)tk_array)

/* Struct types are simply struct types (or, in C++, class/struct types). */
#define is_class_or_struct(tp)                                        \
  ((tp)->kind == (a_type_kind)tk_struct || (tp)->kind == (a_type_kind)tk_class)

/* Union types are simply union types. */
#define is_union(tp) ((tp)->kind == (a_type_kind)tk_union)

/* Aggregate types are array and class/struct (but not union) types. */
#define is_aggregate(tp) (is_array(tp) || is_class_or_struct(tp))

/* Union or aggregate types are simply unions or aggregates. */
#define is_aggregate_or_union(tp) (is_aggregate(tp) || is_union(tp))


a_type_ptr f_skip_typerefs(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type to get to the real type, and
return a pointer to that.  Note that the typeref may have some type
qualifiers (const, volatile), and they will be dropped here.  Therefore,
this routine should not be used when checking type qualifiers.  Note
that ordinarily this routine should not be called directly; use the macro
"skip_typerefs".
*/
/*
There are copies of this routine, under the name local_skip_typerefs,
in il_display.c and c_gen_be.c.  If you change this routine, you
should probably change those routines too.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref) {
    type_ptr = type_ptr->variant.typeref.type;
#if CHECKING
    if (type_ptr == NULL) {
      internal_error("f_skip_typerefs: NULL referenced type");
    }  /* if */
#endif /* CHECKING */
  }  /* while */
  return(type_ptr);
}  /* f_skip_typerefs */


a_type_ptr make_qualified_type(a_type_ptr old_type,
                               a_boolean  is_const,
                               a_boolean  is_volatile)
/*
Make a version of the type old_type with the additional type qualifiers
indicated by is_const and is_volatile.  The qualifiers are added only if
they are not already present.
*/
{
  a_type_ptr new_type;

  /* Add only qualifiers not present already in the type. */
  is_const = is_const && !is_const_qualified_type(old_type);
  is_volatile = is_volatile && !is_volatile_qualified_type(old_type);
  if (is_const || is_volatile) {
    /* Type qualifiers are added by adding a typeref entry which includes
       the type qualifiers.  The original type is not modified. */
    if (in_file_scope((char *)old_type)) {
      new_type = fs_type((a_type_kind)tk_typeref);
    } else {
      new_type = alloc_type((a_type_kind)tk_typeref);
    }  /* if */
    new_type->variant.typeref.type        = old_type;
    new_type->variant.typeref.is_const    = is_const;
    new_type->variant.typeref.is_volatile = is_volatile;
  } else {
    /* No qualifiers to add, so return original type. */
    new_type = old_type;
  }  /* if */
  
  return(new_type);
}  /* make_qualified_type */


a_type_ptr make_unqualified_type(a_type_ptr type)
/*
Return a type that is the unqualified version of the type given by type.
*/
{
  /* Remove the minimum number of typerefs that will produce an unqualified
     type, in order to save typedefs if possible. */
  while (is_qualified_type(type)) {
    type = type->variant.typeref.type;
  }  /* while */

  return(type);
}  /* make_unqualified_type */


a_boolean is_error_type(a_type_ptr tp)
/*
Return TRUE if the given type is an error type.
*/
{
  tp = skip_typerefs(tp);
  return(is_error(tp));
}  /* is_error_type */


a_boolean is_function_type(a_type_ptr tp)
/*
Return TRUE if the given type is a function type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_function(tp));
}  /* is_function_type */


a_boolean is_incomplete_type(a_type_ptr tp)
/*
Return TRUE if the given type is an incomplete type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_incomplete(tp));
}  /* is_incomplete_type */


a_boolean is_object_type(a_type_ptr tp)
/*
Return TRUE if the given type is an object type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_object(tp));
}  /* is_object_type */


a_boolean is_void_type(a_type_ptr tp)
/*
Return TRUE if the given type is the void type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_void(tp));
}  /* is_void_type */


a_boolean is_integral_type(a_type_ptr tp)
/*
Return TRUE if the given type is an integral type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_integral(tp));
}  /* is_integral_type */


a_boolean int_kind_is_signed(an_integer_kind kind)
/*
Return TRUE if the given integer kind is signed.
*/
/*
There are copies of this routine, under the name is_signed_int_kind, in
il_display.c and c_gen_be.c.  If you change this routine, you should
probably change those routines too.
*/
{
  return((kind == (an_integer_kind)ik_char && targ_has_signed_chars) ||
         kind == (an_integer_kind)ik_signed_char                     ||
         kind == (an_integer_kind)ik_short                           ||
         kind == (an_integer_kind)ik_int                             ||
         kind == (an_integer_kind)ik_long);
}  /* int_kind_is_signed */


a_boolean is_signed_integral_type(a_type_ptr tp)
/*
Return TRUE if the type is a signed integral type.
*/
{
  tp = skip_typerefs(tp);
  return(is_integral(tp) && int_kind_is_signed(tp->variant.integer.int_kind));
}  /* is_signed_integral_type */


a_boolean is_character_type(a_type_ptr tp)
/*
Return TRUE if the type is a character type (signed, unsigned, or "plain").
*/
{
  tp = skip_typerefs(tp);
  return (is_character(tp));
}  /* is_character_type */


a_boolean is_floating_type(a_type_ptr tp)
/*
Return TRUE if the given type is a floating type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_floating(tp));
}  /* is_floating_type */


a_boolean is_arithmetic_type(a_type_ptr tp)
/*
Return TRUE if the given type is an arithmetic type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_arithmetic(tp));
}  /* is_arithmetic_type */


a_boolean is_pointer_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_pointer(tp));
}  /* is_pointer_type */


a_boolean is_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is a reference type.
*/
{
  tp = skip_typerefs(tp);
  return(is_reference(tp));
}  /* is_reference_type */


a_boolean is_scalar_type(a_type_ptr tp)
/*
Return TRUE if the given type is a scalar type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_scalar(tp));
}  /* is_scalar_type */


a_boolean is_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array type (3.1.2.5).  Note that
"array type" includes incomplete array types.
*/
{
  tp = skip_typerefs(tp);
  return(is_array(tp));
}  /* is_array_type */


a_boolean is_char_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of (any type of) character.
This includes incomplete arrays of character types.  See 3.1.2.5 for
the definition of "character types" -- "plain" char, signed char, and
unsigned char are all included.
*/
{
  a_boolean       is_char_array = FALSE;
  a_type_ptr      elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    is_char_array = is_character(elem_type);
  }  /* if */
  return(is_char_array);
}  /* is_char_array_type */


a_boolean is_class_struct_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a class, struct, or union type.  Note that
this includes incomplete class, struct, or union types.
*/
{
  tp = skip_typerefs(tp);
  return(is_class_or_struct(tp) || is_union(tp));
}  /* is_class_struct_union_type */


a_boolean is_complete_class_struct_union_type(a_type_ptr tp)
/*
Return TRUE if the type is a complete class, struct, or union type.
*/
{
  tp = skip_typerefs(tp);
  return(!is_incomplete(tp) && (is_class_or_struct(tp) || is_union(tp)));
}  /* is_complete_class_struct_union_type */


a_boolean is_aggregate_or_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a union or aggregate type (array, struct,
or union; 3.1.2.5.  Also class, in C++).  Note that this includes incomplete
array, class, struct, and union types.
*/
{
  tp = skip_typerefs(tp);
  return(is_aggregate_or_union(tp));
}  /* is_aggregate_or_union_type */


a_type_ptr array_element_type(a_type_ptr array_type)
/*
Return the element type of the given array type.
*/
{
  a_type_ptr tp = skip_typerefs(array_type);
#if CHECKING
  if (tp->kind != (a_type_kind)tk_array) {
    internal_error("array_element_type: non-array type");
  }  /* if */
#endif /* CHECKING */
  return(tp->variant.array.element_type);
}  /* array_element_type */


a_type_ptr pointer_referenced_type(a_type_ptr pointer_type)
/*
Return the referenced type of the given pointer type (i.e., the type pointed
to).
*/
{
  a_type_ptr tp = skip_typerefs(pointer_type);
#if CHECKING
  if (tp->kind != (a_type_kind)tk_pointer) {
    internal_error("pointer_referenced_type: non-pointer type");
  }  /* if */
#endif /* CHECKING */
  return(tp->variant.pointer_type_pointed_to);
}  /* pointer_referenced_type */


a_boolean f_is_const_qualified_type(a_type_ptr tp)
/*
Return TRUE if the given type is a const-qualified type (3.1.2.5).
The macro is_const_qualified_type should be called instead of calling this
routine directly.
*/
{
  a_boolean is_const = FALSE;

  for (; tp->kind == (a_type_kind)tk_typeref; tp = tp->variant.typeref.type) {
    if (tp->variant.typeref.is_const) {
      is_const = TRUE;
      break;
    }  /* if */
  }  /* while */

  return(is_const);
}  /* f_is_const_qualified_type */


a_boolean f_is_volatile_qualified_type(a_type_ptr tp)
/*
Return TRUE if the given type is a volatile-qualified type (3.1.2.5).
The macro is_volatile_qualified_type should be called instead of calling this
routine directly.
*/
{
  a_boolean is_volatile = FALSE;

  for (; tp->kind == (a_type_kind)tk_typeref; tp = tp->variant.typeref.type) {
    if (tp->variant.typeref.is_volatile) {
      is_volatile = TRUE;
      break;
    }  /* if */
  }  /* while */

  return(is_volatile);
}  /* f_is_volatile_qualified_type */


a_boolean f_is_qualified_type(a_type_ptr tp)
/*
Return TRUE if the given type is a qualified type (3.1.2.5).
The macro is_qualified_type should be called instead of calling this
routine directly.
*/
{
  a_boolean is_qualified = FALSE;

  for (; tp->kind == (a_type_kind)tk_typeref; tp = tp->variant.typeref.type) {
    if (tp->variant.typeref.is_const || tp->variant.typeref.is_volatile) {
      is_qualified = TRUE;
      break;
    }  /* if */
  }  /* while */

  return(is_qualified);
}  /* f_is_qualified_type */


void check_fixup_list_for_array_types(void)
/*
Check the list of array types to be fixed up, to see if any of their element
types now have a size.  This is used only rarely, when a file-scope variable
is declared with an array type whose elements are an incomplete struct or
union type (this is an extension).  The array type size cannot be determined
when it is declared, so it it put on a list of types to be fixed up.
This routine is called when a struct or union type is completed; it checks
to see if any of the types on the list can now be given sizes.
*/
{
  a_boolean               something_changed;
  an_array_type_fixup_ptr atfp, prev_atfp;

  db_enter(5, "check_fixup_list_for_array_types");
  do {
    something_changed = FALSE;
    for (prev_atfp = NULL,
                    atfp = scope_stack[decl_scope_level].array_type_fixup_list;
         atfp != NULL;
         prev_atfp = atfp, atfp = atfp->next) {
      /* See if the element type for the type to be fixed up by this
         entry is now complete. */
      if (!is_incomplete_type(array_element_type(atfp->array_type))) {
        set_type_size(atfp->array_type);
        something_changed = TRUE;
        /* Take the entry off the list.  The storage for the entry is
           just lost, but there should be very, very few of these. */
        if (prev_atfp == NULL) {
          scope_stack[decl_scope_level].array_type_fixup_list = atfp->next;
        } else {
          prev_atfp->next = atfp->next;
        }  /* if */
      }  /* if */
    }  /* for */
  } while (something_changed);
  db_exit();
}  /* check_fixup_list_for_array_types */


static void add_to_array_fixup_list(a_type_ptr array_type)
/*
array_type points to an array type that cannot be sized now because it
depends on an incomplete struct or union type.  Put it on the list to be
fixed up later.
*/
{
  an_array_type_fixup_ptr atfp;

  db_enter(5, "add_to_array_fixup_list");
  atfp = (an_array_type_fixup_ptr)alloc_fe(sizeof(an_array_type_fixup));
  atfp->next = scope_stack[decl_scope_level].array_type_fixup_list;
  atfp->array_type = array_type;
  scope_stack[decl_scope_level].array_type_fixup_list = atfp;
  db_exit();
}  /* add_to_array_fixup_list */


static a_boolean is_arr_of_incomp_struct_or_union(a_type_ptr tp)
/*
Return TRUE if the given type is an array based of an incomplete struct or
union type.  Such an array is allowed only as an extension, and its size
cannot be determined until the struct or union is completed.
*/
{
  a_boolean  is_arr_of_incomp = FALSE;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    /* Drop any number of array types. */
    do {
      tp = skip_typerefs(tp->variant.array.element_type);
    } while (is_array(tp));
    /* Check for an incomplete struct or union type. */
    if (is_incomplete(tp) && (is_class_or_struct(tp) || is_union(tp))) {
      is_arr_of_incomp = TRUE;
    }  /* if */
  }  /* if */
  return is_arr_of_incomp;
}  /* is_arr_of_incomp_struct_or_union */


void add_if_necessary_to_array_fixup_list(a_type_ptr array_type)
/*
If the given type is an array type that must be fixed up later (its
element type is, directly or indirectly, an incomplete struct or union
type), add it to the fixup list.
*/
{
  if (is_arr_of_incomp_struct_or_union(array_type)) {
    add_to_array_fixup_list(array_type);
  }  /* if */
}  /* add_if_necessary_to_array_fixup_list */


static void set_array_type_size(a_type_ptr array_type)
/*
Compute and set the size and alignment of the array type pointed to by
array_type.
*/
{
  register a_targ_size_t temp, temp2;
  register a_type_ptr    elem_type;

  db_enter(4, "set_array_type_size");
  if (is_arr_of_incomp_struct_or_union(array_type)) {
    /* This is an array whose element type (directly or indirectly) is
       an incomplete struct or union.  The size cannot be determined now.
       Put the type on a list so it can be fixed later if the struct
       or union type is defined. */
    add_to_array_fixup_list(array_type);
  } else {
    /* Get the number of elements.  Note that this is zero for an incomplete
       type like int a[]. */
    temp = array_type->variant.array.number_of_elements;
    elem_type = array_type->variant.array.element_type;
#if CHECKING
    if (elem_type == NULL) {
      internal_error("set_array_type_size: NULL element type");
    }  /* if */
#endif /* CHECKING */
    elem_type = skip_typerefs(elem_type);
#if CHECKING
    if (elem_type->size == 0) {
      internal_error("set_array_type_size: bad element type");
    }  /* if */
#endif /* CHECKING */
    temp2 = elem_type->size;
    /* Check whether or not the multiplication will overflow.  Note that we 
       avoid dividing by temp, since it may be zero for an incomplete type. */
    if (temp > TARG_SIZE_T_MAX/temp2) {
      error(ec_array_size_too_large);
      array_type->variant.array.number_of_elements = 0;
    } else {
      /* Now that we know the multiplication will not overflow, compute the
         array size. */
      array_type->size = temp*temp2;
    }  /* if */
    /* The alignment for the array is the same as the alignment for the
       elements. */
    array_type->alignment = elem_type->alignment;
  }  /* if */
  db_exit();
}  /* set_array_type_size */


void set_type_size(a_type_ptr type_ptr)
/*
Compute and set the size of the type pointed to by type_ptr.  If it is already
set, leave it alone.  Also compute and set the alignment requirement.
*/
{
  a_targ_size_t    size;
  a_targ_alignment alignment;

  db_enter(4, "set_type_size");
  size = type_ptr->size;
  /* If the size is set already, leave it alone. */
  if (size == 0) {
    alignment = 1;  /* Default */
    switch(type_ptr->kind) {
      case tk_error:
        /* Use an arbitrary non-zero size for an error type.  This is
           important so that error types do not appear to be incomplete
           types. */
        size = 1;
        break;
      case tk_unknown:
      case tk_void:
      case tk_routine:
      case tk_typeref:
      case tk_reference:
        /* These stay zero; they have no size directly. */
        break;
      case tk_integer:
        switch (type_ptr->variant.integer.int_kind) {
          case ik_char:
          case ik_signed_char:
          case ik_unsigned_char:
            size = 1;
            break;
          case ik_short:
          case ik_unsigned_short:
            size = TARG_SIZEOF_SHORT;
            alignment = TARG_ALIGNOF_SHORT;
            break;
          case ik_int:
          case ik_unsigned_int:
            size = TARG_SIZEOF_INT;
            alignment = TARG_ALIGNOF_INT;
            break;
          case ik_long:
          case ik_unsigned_long:
            size = TARG_SIZEOF_LONG;
            alignment = TARG_ALIGNOF_LONG;
            break;
#if CHECKING
          default:
            internal_error("set_type_size: bad integer kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_float:
        switch (type_ptr->variant.float_kind) {
          case fk_float:
            size = TARG_SIZEOF_FLOAT;
            alignment = TARG_ALIGNOF_FLOAT;
            break;
          case fk_double:
            size = TARG_SIZEOF_DOUBLE;
            alignment = TARG_ALIGNOF_DOUBLE;
            break;
          case fk_long_double:
            size = TARG_SIZEOF_LONG_DOUBLE;
            alignment = TARG_ALIGNOF_LONG_DOUBLE;
            break;
#if CHECKING
          default:
            internal_error("set_type_size: bad float kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_pointer:
#if TARG_ALL_POINTERS_SAME_SIZE
        /* All pointers are the same size. */
        size = TARG_SIZEOF_POINTER;
        alignment = TARG_ALIGNOF_POINTER;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
        error -- set_type_size: different-sized pointers not implemented.
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
        break;
      case tk_array:
        set_array_type_size(type_ptr);
        goto size_already_set;
#if CHECKING
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Class, struct and union sizes should be set when they are declared.
           See set_field_size_and_offset. */
      default:
        internal_error("set_type_size: bad type kind");
#endif /* CHECKING */
    }  /* switch */
    type_ptr->size      = size;
    type_ptr->alignment = alignment;
size_already_set:;
  }  /* if */
  db_exit();
}  /* set_type_size */


a_type_ptr type_after_integral_promotion(a_type_ptr type)
/*
Determine the type that would result from applying the integral promotions
(3.2.1.1) to type.  Return the promoted type, which may be the same
as the original type.  Type qualifiers (if any) are dropped.
See also node_type_after_integral_promotion for integral promotions for
bit fields.
*/
{
  register a_type_ptr promoted_type;

  db_enter(4, "type_after_integral_promotion");

  promoted_type = skip_typerefs(type);
  if (is_integral(promoted_type)) {
    switch (promoted_type->variant.integer.int_kind) {
      case ik_char:
        if (targ_has_signed_chars) goto do_signed_char;
        goto do_unsigned_char;
      case ik_unsigned_char:
do_unsigned_char:
	if (C_dialect == C_dialect_pcc) {
	  /* In pcc mode, unsigned char becomes unsigned int. */
	  promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
	  break;
	}  /* if */
	/* Fall through. */
      case ik_signed_char:
do_signed_char:;
      case ik_short:
	/* Promote the expression to int. */
	promoted_type = integer_type((an_integer_kind)ik_int);
	break;
      case ik_unsigned_short:
	if (C_dialect == C_dialect_pcc) {
	  /* In pcc mode, unsigned short becomes unsigned int. */
	  promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
	} else {
	  /* Promote to int if int can contain all values, otherwise promote to
	     unsigned int. */
#if TARG_SIZEOF_INT > TARG_SIZEOF_SHORT
	  /* "int" can contain all values of "short"; use "int". */
	  promoted_type = integer_type((an_integer_kind)ik_int);
#else
	  /* All values of "unsigned short" cannot be represented by "int"; use
	     "unsigned int". */
	  promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
#endif /* TARG_SIZEOF_INT > TARG_SIZEOF_SHORT */
	}  /* if */
	break;
      case ik_int:
      case ik_unsigned_int:
      case ik_long:
      case ik_unsigned_long:
        /* These are deliberately left as they are; they are not supposed
           to be promoted. */
        break;
#if CHECKING
      default:
        internal_error("type_after_integral_promotion: bad int kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */

  db_exit();
  return(promoted_type);
}  /* type_after_integral_promotion */


a_type_ptr default_argument_promotion(a_type_ptr old_type)
/*
Determine what (old style) promotion should be done to this argument's
type.  Note that this routine does not actually change the type of the
node; it just returns the type that the node should be.  It is up to
the caller to do the cast.  See also scan_function_call; it depends
on the fact that the default argument promotions on an integral type
are simply the integral promotions (to handle the bit-field integral
promotions case).
*/
{
  a_type_ptr new_type;

  new_type = skip_typerefs(old_type);
  if (is_integral(new_type)) {
    /* For integral types, do the integral promotions. */
    new_type = type_after_integral_promotion(new_type);
  } else if (is_floating(new_type)) {
    /* Promote float to double. */
    if (new_type->variant.float_kind == (a_float_kind)fk_float) {
      new_type = float_type((a_float_kind)fk_double);
    }  /* if */
  }  /* if */

  return(new_type);
}  /* default_argument_promotion */


#if SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
static a_boolean same_repr_int_types(a_type_ptr type_1,
                                     a_type_ptr type_2)
/*
Return TRUE if the two given integer types have the same representation
(same size, signedness, and alignment).
*/
{
  a_boolean same_repr;

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  same_repr = (type_1->size == type_2->size &&
               type_1->alignment == type_2->alignment &&
               int_kind_is_signed(type_1->variant.integer.int_kind) ==
                         int_kind_is_signed(type_2->variant.integer.int_kind));
  return same_repr;
}  /* same_repr_int_types */
#endif /* SAME_REPR_INTS_INTERCHANGEABLE_IN_IL */


/*
Return TRUE if the two class/struct/union types given are copies of one
another, i.e., one of them is a file-scope copy of the other, made
by make_file_scope_type.  type_1 and type_2 are evaluated more than once.
*/
#if 0
/* This needs to be changed to compare classes. */
#endif
#define copies_of_same_struct_union_type(type_1, type_2)                \
  ((type_1)->variant.class_struct_union.field_list != NULL &&           \
   (type_2)->variant.class_struct_union.field_list != NULL &&           \
   (type_1)->variant.class_struct_union.field_list->                    \
					source_corresp.assoc_info ==    \
   (type_2)->variant.class_struct_union.field_list->source_corresp.assoc_info)


a_boolean f_identical_types(a_type_ptr type_1,
                            a_type_ptr type_2)
/*
Return TRUE if the two types are identical.  This includes separate copies
of identical types, as well as the case where the pointers point to the
same type.  This is not a C concept; it's more of an IL concept.  Basically,
two types are identical if no cast is needed to assign a value of one
type to an entity of the other type.  This routine should never be called
directly; it's meant to be called only by the macro identical_types, which
does the initial test for exact pointer equality. 
*/
{
  register a_boolean            identical = FALSE;
  a_param_type_ptr              list1, list2;
  a_routine_type_supplement_ptr extra_info1, extra_info2;

  db_enter(4, "f_identical_types");

  if (is_const_qualified_type(type_1) != is_const_qualified_type(type_2) ||
      is_volatile_qualified_type(type_1) !=
                                        is_volatile_qualified_type(type_2)) {
    /* The type qualifiers do not match, so the types are not identical. */
    /* identical = FALSE;  -- Already set. */
  } else {
    /* Now that type qualifiers are no longer an issue, strip them and other
       typerefs off the types. */
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (type_1 == type_2) {
      /* If the types are now the same, they are identical. */
      identical = TRUE;
    } else if (type_1->kind == type_2->kind) {
      /* The top level kinds are the same, check further. */
      switch (type_1->kind) {
        case tk_error:
        case tk_unknown:
        case tk_void:
           /* No further check needed.  The types are identical. */
          identical = TRUE;
          break;
        case tk_integer:
          /* Requiring equality for enum_constant_list forces an explicit
             type change between enumeration types and integers or
             other enumeration types.  It also makes explicit casts
             useful in suppressing warnings on type changes between
             integral types and enumerated types. */
          if (type_1->variant.integer.enum_constant_list ==
                                  type_2->variant.integer.enum_constant_list) {
            if (type_1->variant.integer.int_kind ==
                                            type_2->variant.integer.int_kind) {
              identical = TRUE;
#if SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
            } else if (same_repr_int_types(type_1, type_2)) {
              /* Integers with the same representation are considered to be
                 identical in the IL if the FE is configured that way. */
              identical = TRUE;
#endif /* SAME_REPR_INTS_INTERCHANGEABLE_IN_IL */
            }  /* if */
          }  /* if */
          break;
        case tk_float:
          identical = (type_1->variant.float_kind ==
                       type_2->variant.float_kind);
          break;
        case tk_pointer:
        case tk_reference:
          /* For pointers and references, they must point to identical
	     types. */
          identical = identical_types(
                           type_1->variant.pointer_type_pointed_to,
                           type_2->variant.pointer_type_pointed_to);
          break;
        case tk_array:
          /* For arrays, the sizes must be the same and the element types
             must be identical. */
          if (identical_types(type_1->variant.array.element_type,
                              type_2->variant.array.element_type) &&
              type_1->variant.array.number_of_elements ==
              type_2->variant.array.number_of_elements) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Generally, if classes, structs, or unions are not exactly the
             same they are not compatible.  However, we may be comparing
             a class, struct, or union with a copy of that type at the file
             scope, made by make_file_scope_type.  If we didn't consider
             those types identical, a cast to a class, struct, or union type
             would be generated. */
          if (copies_of_same_struct_union_type(type_1, type_2)) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_routine:
          /* For functions, the return types must be identical, the
             parameter lists must be identical, and the implicit "this"
             parameter type (if any) must be identical. */
          extra_info1 = type_1->variant.routine.extra_info;
          extra_info2 = type_2->variant.routine.extra_info;
          if (identical_types(type_1->variant.routine.return_type,
                              type_2->variant.routine.return_type) &&
              extra_info1->prototyped == extra_info2->prototyped &&
              extra_info1->has_ellipsis == extra_info2->has_ellipsis &&
              identical_types(extra_info1->implicit_this_param_type,
                              extra_info2->implicit_this_param_type)) {
            /* Compare the types of the parameters on the two lists. */
            for (list1 = extra_info1->param_type_list,
                                          list2 = extra_info2->param_type_list;
                 list1 != NULL && list2 != NULL;
                 list1 = list1->next, list2 = list2->next) {
              if (!identical_types(list1->type, list2->type)) {
                /* The parameter types are not identical. */
                goto funcs_not_identical;
              }  /* if */
            }  /* for */
            /* The parameter lists are identical if they both ended
               together. */
            identical = (list1 == NULL && list2 == NULL);
funcs_not_identical:;
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("f_identical_types: bad type");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "f_identical_types: %s\n", identical ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(identical);
}  /* f_identical_types */


a_boolean f_types_are_compatible(a_type_ptr type_1,
                                 a_type_ptr type_2)
/*
Compare two types for compatibility.  See section 3.1.2.6 in the standard.
An error type is considered compatible with any other type.
This routine always checks for compatibility of type-qualifiers; see
types_pointed_to_are_compatible for an alternative.  This routine should
never be called directly; it's meant to be called only by the macro
types_are_compatible, which does the initial test for exact pointer equality.
*/
{
  register a_boolean            compat = FALSE;
  a_param_type_ptr              list1, list2;
  a_boolean                     list1_prototyped, list2_prototyped;
  a_routine_type_supplement_ptr extra_info1, extra_info2, local_extra_info2;
  a_type_ptr                    param_2_type;

  db_enter(4, "f_types_are_compatible");

  if (is_const_qualified_type(type_1) != is_const_qualified_type(type_2) ||
      is_volatile_qualified_type(type_1) !=
                                         is_volatile_qualified_type(type_2)) {
    /* The type qualifiers do not match, so the types are not compatible. */
    /* compat = FALSE;  -- Already set. */
  } else {
    /* Now that type qualifiers are no longer an issue, strip them and other
       typerefs off the types. */
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (type_1 == type_2) {
      /* If the types are now the same, they are compatible. */
      compat = TRUE;
    } else if (type_1->kind == type_2->kind) {
      /* The top level kinds are the same, check further. */
      switch (type_1->kind) {
        case tk_error:
        case tk_unknown:
        case tk_void:
          /* No further check needed.  The types are compatible. */
          compat = TRUE;
          break;
        case tk_integer:
          /* The enum_constant_list fields need not be the same. */
          if (type_1->variant.integer.int_kind ==
                                            type_2->variant.integer.int_kind) {
            compat = TRUE;
#if SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
          } else if (C_dialect == C_dialect_pcc &&
                     same_repr_int_types(type_1, type_2)) {
            /* In pcc mode, integers with the same representation are
               considered to be identical if the FE is configured that way.
               This typically makes int and long compatible. */
            compat = TRUE;
#endif /* SAME_REPR_INTS_INTERCHANGEABLE_IN_IL */
          }  /* if */
          break;
        case tk_float:
          compat = (type_1->variant.float_kind == type_2->variant.float_kind);
          break;
        case tk_pointer:
        case tk_reference:
          /* For pointers and references, they must point to compatible
             types. */
          compat = types_are_compatible(
                           type_1->variant.pointer_type_pointed_to,
                           type_2->variant.pointer_type_pointed_to);
          break;
        case tk_array:
          /* For arrays, if both have sizes the sizes must be the same.  The
             element types must be compatible. */
          if (types_are_compatible(type_1->variant.array.element_type,
                                   type_2->variant.array.element_type)) {
            if (type_1->variant.array.number_of_elements == 0 ||
                type_2->variant.array.number_of_elements == 0 ||
                type_1->variant.array.number_of_elements ==
                type_2->variant.array.number_of_elements) {
              compat = TRUE;
            }  /* if */
          }  /* if */
          break;
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Generally, if classes, structs, or unions are not exactly the
             same they are not compatible.  However, we may be comparing
             a class, struct, or union with a copy of that type at the file
             scope, made by make_file_scope_type.  If we didn't consider
             those types identical, a cast to a class, struct, or union type
             would be generated. */
          if (copies_of_same_struct_union_type(type_1, type_2)) {
            compat = TRUE;
          }  /* if */
          break;
        case tk_routine:
          /* For functions, the return types must be compatible.  If
             either function has a new-style parameter list, the
             individual parameter types must be compatible.
             See 3.5.4.3. */
          /* Both types must have an ellipsis or neither must. */
          /* The implicit "this" parameter types (if any) must be compatible
             too. */
          extra_info1 = type_1->variant.routine.extra_info;
          extra_info2 = type_2->variant.routine.extra_info;
          if (types_are_compatible(type_1->variant.routine.return_type,
                                   type_2->variant.routine.return_type) &&
              extra_info1->has_ellipsis == extra_info2->has_ellipsis &&
              types_are_compatible(extra_info1->implicit_this_param_type,
                                   extra_info2->implicit_this_param_type)) {
            list1_prototyped = extra_info1->prototyped;
            list2_prototyped = extra_info2->prototyped;
            if (!list1_prototyped && !list2_prototyped) {
              /* Both parameter lists are old-style, so they are compatible. */
              compat = TRUE;
            } else {
              /* At least one of the function types has a prototyped
                 parameter list. */
              list1 = extra_info1->param_type_list;
              list2 = extra_info2->param_type_list;
              local_extra_info2 = extra_info2;
              if (!list1_prototyped) {
                /* Switch the two parameter lists, so that if there is
                   an old-style parameter list involved, it is list2. */
                list1 = list2;
                list2 = extra_info1->param_type_list;
                list1_prototyped = TRUE;
                list2_prototyped = FALSE;
                local_extra_info2 = extra_info1;
              }  /* if */
              if (!list2_prototyped) {
                /* The second parameter list is old-style.  */
                if (local_extra_info2->assoc_routine == NULL) {
                  /* The old-style type is the type for a routine without a
                     body, so there is no parameter information.  The
                     prototyped parameter list from the first type is used,
                     and each type on the list will be promoted before
                     comparison. */
                  list2 = list1;
                }  /* if */
              }  /* if */
              /* Compare the types of the parameters on the two lists. */
              for (; list1 != NULL && list2 != NULL;
                   list1 = list1->next, list2 = list2->next) {
                /* Compare the parameter types, with the second parameter
                   type promoted appropriately if it is old-style. */
                param_2_type = list2->type;
                if (!list2_prototyped) {
                  param_2_type = default_argument_promotion(param_2_type);
                }  /* if */
                if (!types_are_compatible(list1->type, param_2_type)) {
                  /* The parameter types are not compatible. */
                  goto funcs_not_compatible;
                }  /* if */
              }  /* for */
              /* The parameter lists are compatible if they both ended
                 together. */
              compat = (list1 == NULL && list2 == NULL);
funcs_not_compatible:;
            }  /* if */
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("f_types_are_compatible: bad type");
#endif /* CHECKING */
      }  /* switch */
    } else if (type_1->kind == (a_type_kind)tk_error ||
               type_2->kind == (a_type_kind)tk_error) {
      /* An error type is compatible with any other type. */
      compat = TRUE;
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "f_types_are_compatible: %s\n", compat ? "TRUE":"FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(compat);
}  /* f_types_are_compatible */


a_boolean interchangeable_types(a_type_ptr type_1,
                                a_type_ptr type_2)
/*
Return TRUE if the given types are "interchangeable".  This is a concept
implied by the footnote in 3.1.2.5 in the C standard: "The same representation
and alignment requirements are meant to imply interchangeability as
arguments to functions, return values, and members of unions".  
Interchangeability is a less strict matching than compatibility:
compatible types are interchangeable, but interchangeable types need
not be compatible.  This routine is called to check printf arguments
and arguments of old-style calls.
*/
{
  a_boolean       interch = FALSE;
  an_integer_kind ikind1, ikind2;
  a_type_ptr      ptr_type1, ptr_type2;

  db_enter (4, "interchangeable_types");
  /* The footnote in 3.1.2.5 applies to four cases:
       (1)  Corresponding signed and unsigned integral types.
       (2)  Qualified or unqualified versions of interchangeable types.
       (3)  void * and char *.
       (4)  Pointers to qualified or unqualified versions of compatible
            types.
     In addition,
       (5)  Compatible types are interchangeable.
       (6)  Pointers to interchangeable types are considered 
            interchangeable.  The standard doesn't say this, but it
            seems sensible given the other clauses (e.g., unsigned int *
            and int * should be interchangeable).
  */
  /* Drop type qualifiers and other typerefs.  This takes care of point (2)
     above. */
  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (types_are_compatible(type_1, type_2)) {
    /* Compatible types are interchangeable. */
    interch = TRUE;
  } else if (type_1->kind != type_2->kind) {
    /* The kinds are different, so the types are not interchangeable. */
    /* interch = FALSE;  -- already set. */
  } else if (type_1->kind == (a_type_kind)tk_integer) {
    /* Look for two integral types that differ only in signedness, or
       two character types. */
    /* Since types_are_compatible was called above, we only have to look
       for cases where the kinds don't match, and we don't have to consider
       the PCC_SAME_REPR_INTS_INTERCHANGEABLE_IN_IL cases. */
    ikind1 = type_1->variant.integer.int_kind;
    ikind2 = type_2->variant.integer.int_kind;
    if (ikind1 == (an_integer_kind)ik_signed_char ||
        ikind1 == (an_integer_kind)ik_unsigned_char ||
        ikind1 == (an_integer_kind)ik_char) {
      interch = (ikind2 == (an_integer_kind)ik_signed_char ||
                 ikind2 == (an_integer_kind)ik_unsigned_char ||
                 ikind2 == (an_integer_kind)ik_char);
    } else if ((ikind1 == (an_integer_kind)ik_short &&
                ikind2 == (an_integer_kind)ik_unsigned_short) ||
               (ikind1 == (an_integer_kind)ik_unsigned_short &&
                ikind2 == (an_integer_kind)ik_short) ||
               (ikind1 == (an_integer_kind)ik_int &&
                ikind2 == (an_integer_kind)ik_unsigned_int) ||
               (ikind1 == (an_integer_kind)ik_unsigned_int &&
                ikind2 == (an_integer_kind)ik_int) ||
               (ikind1 == (an_integer_kind)ik_long &&
                ikind2 == (an_integer_kind)ik_unsigned_long) ||
               (ikind1 == (an_integer_kind)ik_unsigned_long &&
                ikind2 == (an_integer_kind)ik_long)) {
      interch = TRUE;
    }  /* if */
  } else if (type_1->kind == (a_type_kind)tk_pointer) {
    /* Pointer types.  Get the underlying types. */
    ptr_type1 = skip_typerefs(type_1->variant.pointer_type_pointed_to);
    ptr_type2 = skip_typerefs(type_2->variant.pointer_type_pointed_to);
    if (ptr_type1 == ptr_type2 ||  /* This test for speed. */
        interchangeable_types(ptr_type1, ptr_type2)) {
      /* Pointers to interchangeable types are interchangeable. */
      interch = TRUE;
    } else if ((is_void(ptr_type1) && is_character(ptr_type2)) ||
               (is_character(ptr_type1) && is_void(ptr_type2))) {
      /* void * and char * are interchangeable. */
      interch = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
  return (interch);
}  /* interchangeable_types */


a_boolean types_pointed_to_are_compatible(a_type_ptr type_1,
                                          a_type_ptr type_2,
                                          a_boolean  ptrs_to_void_compatible)
/*
type_1 and type_2 are pointer types.  Return TRUE if the types they point
to are compatible, ignoring any top-level type qualifiers on those
types.  Treat a pointer to void as being compatible with any other
pointer (to object or incomplete) type if ptrs_to_void_compatible is TRUE.
*/
{
  a_boolean compat = FALSE;
  register a_type_ptr ptr_type1;
  register a_type_ptr ptr_type2;

  db_enter (4, "types_pointed_to_are_compatible");

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);

#if CHECKING
  if (type_1->kind != (a_type_kind)tk_pointer ||
      type_2->kind != (a_type_kind)tk_pointer) {
    internal_error("types_pointed_to_are_compatible: non-pointer types");
  }  /* if */
#endif /* CHECKING */
  if (type_1 == type_2) {
    /* For speed: if the types are identical, they are compatible. */
    compat = TRUE;
  } else {
    /* Remove any type qualifiers, check for compatibility of the types pointed
       to.  skip_typerefs is used instead of make_unqualified_type because
       it's needed for the is_void test (it removes typedefs too), and it's
       faster. */
    ptr_type1 = skip_typerefs(type_1->variant.pointer_type_pointed_to);
    ptr_type2 = skip_typerefs(type_2->variant.pointer_type_pointed_to);
    if (types_are_compatible(ptr_type1, ptr_type2) ||
        (ptrs_to_void_compatible &&
         ((is_void(ptr_type1)      && !is_function(ptr_type2)) ||
          (!is_function(ptr_type1) && is_void(ptr_type2))))) {
      compat = TRUE;
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "types_pointed_to_are_compatible: %s\n",
                     compat ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(compat);
}  /* types_pointed_to_are_compatible */


a_type_ptr composite_type(a_type_ptr type_1,
                          a_type_ptr type_2)
/*
Return the type that is the composite type of type_1 and type_2.  See 3.1.2.6.
type_1 and type_2 must be compatible (see types_are_compatible).  The type
returned might be equal to type_1, type_2, both, or neither.  Where it's
possible, this routine tries to return type_1.  Note that if a new type
is allocated, it is allocated in the file scope.
*/
{
  a_type_ptr       comp_type;
  a_type_ptr       base_type_1, base_type_2;
  a_boolean        add_const, add_volatile;
  a_type_ptr       comp_elem, comp_param_type;
  a_targ_size_t    num_elems;
  a_param_type_ptr list1, list2, param1, param2;
  a_boolean        list1_prototyped, list2_prototyped;
  a_boolean        comp_equals_list1, comp_equals_list2;
  a_param_type_ptr comp_param, comp_param_list, end_comp_param_list;
  a_boolean        comp_prototyped;

  db_enter(4, "composite_type");

#if CHECKING
  if (!types_are_compatible(type_1, type_2)) {
    internal_error("composite_type: types are not compatible");
  }  /* if */
#endif /* CHECKING */
  if (type_1 == type_2) {
    /* If the types are identical (the most common case), the composite type
       is the same thing. */
    comp_type = type_1;
  } else {
    /* Remove extra typerefs and type qualifiers. */
    base_type_1 = skip_typerefs(type_1);
    base_type_2 = skip_typerefs(type_2);
    if (base_type_1 == base_type_2) {
      /* If the types are now identical, they are also the composite type. */
      comp_type = base_type_1;
    } else if (base_type_1->kind != base_type_2->kind) {
      /* Since the types are compatible, the kinds should be the same.
         They aren't, so it must be that one of the types is an error
         type. */
#if CHECKING
      if (base_type_1->kind != (a_type_kind)tk_error &&
          base_type_2->kind != (a_type_kind)tk_error) {
        internal_error("composite_type: kinds not equal");
      }  /* if */
#endif /* CHECKING */
      /* The composite type is an error type. */
      comp_type = error_type();
    } else {
      switch (base_type_1->kind) {
        case tk_error:
        case tk_unknown:
        case tk_void:
        case tk_integer:
        case tk_float:
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Simple types.  The composite type is either of the types. */
          /* The class/struct/union cases are here because a
             class/struct/union can be compatible with a file-scope
             copy of itself. */
          comp_type = base_type_1;
          break;
        case tk_pointer:
        case tk_reference:
          /* Pointer and reference types.  The composite type is a pointer
	     or reference to the composite of the types pointed to. */
          comp_elem = composite_type(
                        base_type_1->variant.pointer_type_pointed_to,
                        base_type_2->variant.pointer_type_pointed_to);
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new pointer type. */
          if (comp_elem == base_type_1->variant.pointer_type_pointed_to) {
            comp_type = base_type_1;
          } else if (comp_elem ==
                     base_type_2->variant.pointer_type_pointed_to) {
            comp_type = base_type_2;
          } else {
	    if (base_type_1->kind == tk_pointer) {
              comp_type = make_pointer_type(comp_elem);
	    } else {
              comp_type = make_reference_type(comp_elem);
	    }  /* if */
          }  /* if */
          break;
        case tk_array:
          /* Array types.  The composite type is an array with elements
             of the composite type of the two array element types.  The size
             is the size of the non-incomplete array type, if there is
             one.  The composite_type call is done in two different orders
             because if both types are equivalent to the composite type,
             the first operand is returned, and we'd like the element type
             to be the one from the non-incomplete array. */
          if (base_type_1->variant.array.number_of_elements != 0) {
            num_elems = base_type_1->variant.array.number_of_elements;
            comp_elem = composite_type(base_type_1->variant.array.element_type,
                                      base_type_2->variant.array.element_type);
          } else {
            num_elems = base_type_2->variant.array.number_of_elements;
            comp_elem = composite_type(base_type_2->variant.array.element_type,
                                      base_type_1->variant.array.element_type);
          }  /* if */
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new array type. */
          if (comp_elem == base_type_1->variant.array.element_type &&
              num_elems == base_type_1->variant.array.number_of_elements) {
            comp_type = base_type_1;
          } else if (comp_elem == base_type_2->variant.array.element_type &&
              num_elems == base_type_2->variant.array.number_of_elements) {
            comp_type = base_type_2;
          } else {
            comp_type = fs_type((a_type_kind)tk_array);
            comp_type->variant.array.element_type       = comp_elem;
            comp_type->variant.array.number_of_elements = num_elems;
            set_type_size(comp_type);
          }  /* if */
          break;
        case tk_routine:
          /* Function types. */
          /* Form the composite of the return types. */
          comp_elem = composite_type(base_type_1->variant.routine.return_type,
                                     base_type_2->variant.routine.return_type);
          /* If both function types are not prototyped, the composite type
             is likewise not prototyped.  If one of the two types is
             prototyped, the other not, the composite type is the one
             that is prototyped.  If both types are prototyped, the
             composite type is prototyped, with each parameter type in
             its list being the composite of the corresponding parameter
             types in the two lists. */
          list1 = base_type_1->variant.routine.extra_info->param_type_list;
          list2 = base_type_2->variant.routine.extra_info->param_type_list;
          list1_prototyped = base_type_1->variant.routine.extra_info->
                                                                    prototyped;
          list2_prototyped = base_type_2->variant.routine.extra_info->
                                                                    prototyped;
          comp_prototyped = list1_prototyped || list2_prototyped;
          if (!comp_prototyped) {
            /* Both types have old-style (non-prototyped) interfaces, so
               there is no parameter information in the composite. */
            comp_param_list = NULL;
          } else if (!list2_prototyped) {
            /* Type 2 has an old-style interface, so use the prototyped
               interface from type 1. */
            comp_param_list = list1;
          } else if (!list1_prototyped) {
            /* Type 1 has an old-style interface, so use the prototyped
               interface from type 2. */
            comp_param_list = list2;
          } else {
            /* Both types are prototyped, so the element-wise composite of
               the two parameter lists must be constructed.  Take a first
               pass through to see if the composite is equal to either of
               the parameter lists. */
            for (comp_equals_list1 = comp_equals_list2 = TRUE,
                                                param1 = list1, param2 = list2;
                 param1 != NULL && (comp_equals_list1 || comp_equals_list2);
                 param1 = param1->next, param2 = param2->next) {
#if CHECKING
              if (param2 == NULL) {
                /* Since the types are compatible and old-style parameter lists
                   have been ruled out, the two parameter lists should have the
                   same length. */
                internal_error("composite_type: unequal length param lists");
              }  /* if */
#endif /* CHECKING */
              /* Compare the two parameter types against their composite
                 type.  Note that if a new type is allocated here for the
                 composite, it will probably just be wasted. */
              comp_param_type = composite_type(param1->type, param2->type);
              if (comp_param_type != param1->type) comp_equals_list1 = FALSE;
              if (comp_param_type != param2->type) comp_equals_list2 = FALSE;
            }  /* for */
            if (comp_equals_list1) {
              comp_param_list = list1;
            } else if(comp_equals_list2) {
              comp_param_list = list2;
            } else {
              /* Neither parameter list matches the composite, so construct
                 a new parameter list.  This happens for something like

                   int f(int (*)(      ), double (*)[3]);
                   int f(int (*)(char *), double (*)[ ]);

                 where the composite type is

                   int f(int (*)(char *), double (*)[3]);

              */
              comp_param_list = end_comp_param_list = NULL;
              for (param1 = list1,  param2 = list2;
                   param1 != NULL;
                   param1 = param1->next, param2 = param2->next) {
                comp_param = alloc_param_type(/*at_file_scope=*/TRUE);
                comp_param->type = composite_type(param1->type, param2->type);
                /* Add the parameter type entry to the end of the list. */
                if (comp_param_list == NULL) {
                  comp_param_list = comp_param;
                } else {
                  end_comp_param_list->next = comp_param;
                }  /* if */
                end_comp_param_list = comp_param;
              }  /* for */
            }  /* if */
          }  /* if */
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new function type.  The return type,
             parameter list, and prototyped flag must match.  Note that the
             has_ellipsis flag is not checked because both types must have the
             same value, and likewise the implicit "this" parameter type. */
          if (base_type_1->variant.routine.return_type == comp_elem &&
              base_type_1->variant.routine.extra_info->param_type_list ==
                                                             comp_param_list &&
              base_type_1->variant.routine.extra_info->prototyped ==
                                                             comp_prototyped) {
            comp_type = base_type_1;
          } else if (base_type_2->variant.routine.return_type == comp_elem &&
              base_type_2->variant.routine.extra_info->param_type_list ==
                                                             comp_param_list &&
              base_type_2->variant.routine.extra_info->prototyped ==
                                                             comp_prototyped) {
            comp_type = base_type_2;
          } else {
            /* Build a new function type. */
            a_routine_type_supplement_ptr extra_info;
            comp_type = fs_type((a_type_kind)tk_routine);
            comp_type->variant.routine.return_type = comp_elem;
            extra_info = comp_type->variant.routine.extra_info;
            extra_info->param_type_list = comp_param_list;
            extra_info->prototyped = comp_prototyped;
            extra_info->has_ellipsis =
                         base_type_1->variant.routine.extra_info->has_ellipsis;
            extra_info->implicit_this_param_type =
             base_type_1->variant.routine.extra_info->implicit_this_param_type;
          }  /* if */
          break;
#if CHECKING
        /* Typerefs were removed above, and therefore shouldn't occur. */
        case tk_typeref:
        default:
          internal_error("composite_type: bad type kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
    /* If the composite type is different from both original types, some
       type qualifiers may have to be added. */
    if (comp_type == base_type_1) {
      comp_type = type_1;
    } else if (comp_type == base_type_2) {
      comp_type = type_2;
    } else {
      /* Add any missing type qualifiers to the composite type.  Both
         original types must have the same type qualifiers, but the
         qualifiers are not necessarily indicated by the same sequence of
         typerefs. */
      add_const = is_const_qualified_type(type_1) &&
                  !is_const_qualified_type(comp_type);
      add_volatile = is_volatile_qualified_type(type_1) &&
                     !is_volatile_qualified_type(comp_type);
      if (add_const || add_volatile) {
        comp_type = make_qualified_type(comp_type, add_const, add_volatile);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return(comp_type);
}  /* composite_type */


/*
Memory region to switch back to after file_scope_type copy, if non-zero.
*/
static a_memory_region_number
		region_to_switch_back_to;

/*
Entry used to keep track of the list of type entries already visited by
file_scope_type during its travels to find things to copy.
*/
typedef struct a_type_copy_history *a_type_copy_history_ptr;
typedef struct a_type_copy_history {
  a_type_copy_history_ptr
		prev;
			/* Pointer to the entry in the stack of the recursive
			   parent of the current level. */
#define HISTORY_ARRAY_SIZE 30
			/* Size of history array.  See comment in
			   file_scope_type. */
  int		num_entries;
			/* The number of entries in old_type/new_type.
			   At least 1. */
  a_type_ptr	old_type[HISTORY_ARRAY_SIZE],
		new_type[HISTORY_ARRAY_SIZE];
			/* The "before" and "after" versions of types
			   being processed.  old_type[0] will always be
			   the type being converted at the level represented
			   by this entry, and new_type[0] the file-scope
			   version of that type. */
} a_type_copy_history;


/* Forward declaration required because of mutual recursion: */
static a_type_ptr file_scope_type(a_type_ptr              old_type,
                                  a_type_copy_history_ptr prev_history);


static a_param_type_ptr file_scope_param_list(
                                        a_param_type_ptr        old_param,
                                        a_type_copy_history_ptr prev_history)
/*
Make a file-scope copy of a parameter type list, and return a pointer
to it.  prev_history gives the history of copies up to this point.
*/
{
  a_param_type_ptr new_param, new_param_list, end_new_param_list;

  new_param_list = end_new_param_list = NULL;
  for (; old_param != NULL; old_param = old_param->next) {
    new_param = alloc_param_type(/*at_file_scope=*/TRUE);
    *new_param = *old_param;
    if (new_param->type != NULL) {
      new_param->type = file_scope_type(old_param->type, prev_history);
    }  /* if */
    new_param->next = NULL;
    if (new_param_list == NULL) {
      new_param_list = new_param;
    } else {
      end_new_param_list->next = new_param;
    }  /* if */
    end_new_param_list = new_param;
  }  /* for */

  return(new_param_list);
}  /* file_scope_param_list */


static a_type_ptr file_scope_type(a_type_ptr              old_type,
                                  a_type_copy_history_ptr prev_history)
/*
Routine that does the work for make_file_scope_type.  Returns a version
of old_type that is entirely accessible in the file scope.  prev_history points
to a list of entries that indicates all the type entries visited/copied
so far, to avoid repeating work or getting into infinite loops.
*/
{
  a_type_ptr              new_type;
  a_type_copy_history     history;
  a_type_copy_history_ptr h_ptr;
  int			  h_index, prev_h_index;
  a_constant_ptr          old_ec, new_ec, new_ec_list, end_new_ec_list;
  a_field_ptr             old_field, new_field, new_field_list,
                          end_new_field_list;
  a_routine_type_supplement_ptr
			  extra_info;
  a_type_kind             kind;

  /* See if the type entry is already at the file scope, and does not need
     to be copied. */
  if (in_file_scope((char *)old_type)) {
    new_type = old_type;
  } else {
    /* See if the type has already been copied at some point in the
       traversal of this type tree.  The old_type[0] entry is critical
       in that it prevents infinite loops when, for example, structs
       contain pointers to themselves.  The other entries are only for
       optimization: they prevent duplicate copying of types already
       encountered in other parts of the tree that are not above the
       current location.  Such duplicate copying would be inelegant, but
       not incorrect.  The more entries there are (HISTORY_ARRAY_SIZE),
       the more complicated a tree will be copied without duplicate
       copies.  However, it is unlikely that a tree will have a large
       number of different structs, and therefore a moderate value
       of HISTORY_ARRAY_SIZE is from a practical standpoint almost
       equivalent to a large value. */
    for (h_ptr = prev_history; h_ptr != NULL; h_ptr = h_ptr->prev) {
      for (h_index = 0; h_index < h_ptr->num_entries; h_index++) {
        if (h_ptr->old_type[h_index] == old_type) {
          /* Found the type in the history list, so do not copy it again;
             use the corresponding new_type. */
          new_type = h_ptr->new_type[h_index];
          goto end_of_routine;
        }  /* if */
      }  /* for */
    }  /* for */
    /* The type entry was not found in the history list, so it will have
       to be copied.  Switch to the file scope memory region if not
       already there. */
    if (curr_il_region_number != FILE_SCOPE_REGION_NUMBER) {
      region_to_switch_back_to = curr_il_region_number;
      switch_il_region(FILE_SCOPE_REGION_NUMBER);
    }  /* if */
    kind = old_type->kind;
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "file_scope_type: copying type with kind = %d\n",
                       (int)kind);
    }  /* if */
#endif /* DEBUG */
    /* Set the history entry for the current level. */
    history.prev = prev_history;
    history.num_entries = 1;
    history.old_type[0] = old_type;
    history.new_type[0] = new_type = alloc_type(kind);
    /* Start with an extra copy of the old type, to ensure that all minor
       flags are copied. */
    copy_type(old_type, new_type);
    /* Clear the source correspondence (it refers to something not at file
       scope, if it refers to anything at all). */
    set_default_source_corresp(&new_type->source_corresp);
    /* assoc_pointer_type can be retained if the associated type is
       at the file scope.  Otherwise, clear the field. */
    if (!in_file_scope((char *)old_type->assoc_pointer_type)) {
      new_type->assoc_pointer_type = NULL;
    }  /* if */
    /* Copy the substructure of the type, if any. */
    switch(kind) {
      case tk_error:
      case tk_unknown:
      case tk_void:
      case tk_float:
        /* No extra fields to copy. */
        break;
      case tk_integer:
        /* Copy an enumerated constant list if there is one. */
        new_ec_list = end_new_ec_list = NULL;
        if (old_type->variant.integer.enum_type) {
          old_ec = old_type->variant.integer.enum_constant_list;
          add_to_types_list(new_type, /*at_file_scope=*/TRUE,
                                      /*in_old_style_param_decl_list=*/FALSE);
          for (; old_ec != NULL; old_ec = old_ec->next) {
            new_ec = alloc_unshared_constant(old_ec);
            new_ec->type = file_scope_type(old_ec->type, &history);
            if (new_ec_list == NULL) {
              new_ec_list = new_ec;
            } else {
              end_new_ec_list->next = new_ec;
            }  /* if */
            end_new_ec_list = new_ec;
          }  /* for */
        }  /* if */
        new_type->variant.integer.enum_constant_list = new_ec_list;
        break;
      case tk_pointer:
      case tk_reference:
        new_type->variant.pointer_type_pointed_to =
          file_scope_type(old_type->variant.pointer_type_pointed_to, &history);
        break;
      case tk_array:
        new_type->variant.array.element_type =
               file_scope_type(old_type->variant.array.element_type, &history);
        new_type->variant.array.number_of_elements =
                                    old_type->variant.array.number_of_elements;
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Copy the field list. */
        new_field_list = end_new_field_list = NULL;
        for (old_field = old_type->variant.class_struct_union.field_list;
             old_field != NULL;
             old_field = old_field->next) {
          new_field = alloc_field();
          *new_field = *old_field;
          /* The source correspondence is NOT cleared.  The name is needed
             for IL output.  The copy still corresponds to the source
             construct. */
          new_field->type = file_scope_type(old_field->type, &history);
          new_field->assoc_class_struct_union_type = new_type;
          new_field->next = NULL;
          if (new_field_list == NULL) {
            new_field_list = new_field;
          } else {
            end_new_field_list->next = new_field;
          }  /* if */
          end_new_field_list = new_field;
        }  /* for */
        new_type->variant.class_struct_union.field_list = new_field_list;
        if (old_type->variant.class_struct_union.extra_info != NULL) {
          /* Copy the supplement. */
#if 0
          internal_error(
                    "file_scope_type: copy of class supplement unimplemented");
#endif
        }  /* if */
        /* Add the type to the file scope types list.  This is done after
           the fields are processed to get the file-scope types in the
           right order. */
        add_to_types_list(new_type, /*at_file_scope=*/TRUE,
                          /*in_old_style_param_decl_list=*/FALSE);
        break;
      case tk_routine:
        new_type->variant.routine.return_type =
              file_scope_type(old_type->variant.routine.return_type, &history);
        extra_info = new_type->variant.routine.extra_info;
        /* The prototype scope contains only named types, and does not need
           to be copied explicitly (any types from it that are used will
           be copied on use). */
        extra_info->prototype_scope = NULL;
        /* This copy of the type is not associated with the original routine
           if any. */
        extra_info->assoc_routine = NULL;
        /* Copy the lists of parameter type information. */
        extra_info->param_type_list = file_scope_param_list(
              old_type->variant.routine.extra_info->param_type_list,
              &history);
        break;
      case tk_typeref:
        new_type->variant.typeref.type =
                     file_scope_type(old_type->variant.typeref.type, &history);
        break;
#if CHECKING
      default:
        internal_error("file_scope_type: bad type kind");
#endif /* CHECKING */
    }  /* switch */
    /* Copy a record of the types processed back into the parent's 
       history array. */
    if (prev_history != NULL) {
      for (h_index = 0; h_index < history.num_entries; h_index++) {
        prev_h_index = prev_history->num_entries;
        if (prev_h_index == HISTORY_ARRAY_SIZE) {
          /* The parent's history array is full, so copy no more. */
          break;
        } else {
          prev_history->old_type[prev_h_index] = history.old_type[h_index];
          prev_history->new_type[prev_h_index] = history.new_type[h_index];
          prev_history->num_entries++;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
end_of_routine:
  return(new_type);
}  /* file_scope_type */


a_type_ptr make_file_scope_type(a_type_ptr type)
/*
Make a version of the indicated type that is in the file scope (top-level)
memory region, and thus is accessible from anywhere.  This is used when
a type appears inside a function, and is needed globally for type checking
(like for compatibility of externals).  This routine only copies the parts
of the type that are not already in the file scope.  In the limiting case,
if the type is already completely in the file scope, the original pointer
is returned.
*/
{
  a_type_ptr new_type;

  region_to_switch_back_to = NULL_region_number;
  new_type = file_scope_type(type, (a_type_copy_history_ptr)NULL);
  /* Switch back to the original memory region if we switched to the file
     scope region during the copy. */
  if (region_to_switch_back_to != NULL_region_number) {
    switch_il_region(region_to_switch_back_to);
  }  /* if */
  return(new_type);
}  /* make_file_scope_type */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
