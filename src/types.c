/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
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
  ((tp)->size == 0 && !is_function(tp) && !is_reference_ptr(tp))

/* The void type is simply the void type. */
#define is_void(tp) ((tp)->kind == (a_type_kind)tk_void)

/* Integral types comprise char, the signed and unsigned integer types,
   and enumerated types. */
#define is_integral(tp) ((tp)->kind == (a_type_kind)tk_integer)

/* Enum types are integral types that are tagged as enums. */
#define is_enum(tp) (is_integral(tp) && (tp)->variant.integer.enum_type)

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
#define is_pointer(tp) ((tp)->kind == (a_type_kind)tk_pointer &&      \
                        !(tp)->variant.pointer.is_reference)

/* The reference type is simply the reference type. */
#define is_reference_ptr(tp) ((tp)->kind == (a_type_kind)tk_pointer &&\
                              (tp)->variant.pointer.is_reference)

/* Scalar types are the arithmetic types plus the pointer types. */
#define is_scalar(tp) (is_arithmetic(tp) || is_pointer(tp))

/* Array types are simply array types. */
#define is_array(tp) ((tp)->kind == (a_type_kind)tk_array)

/* Struct types are simply struct types (or, in C++, class/struct types). */
#define is_class_or_struct(tp)                                        \
  ((tp)->kind == (a_type_kind)tk_struct || (tp)->kind == (a_type_kind)tk_class)

/* Union types are simply union types. */
#define is_union(tp) ((tp)->kind == (a_type_kind)tk_union)

#define is_class_struct_union(tp) (is_class_or_struct(tp) || is_union(tp))

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
"skip_typerefs".  However, it does make sense to call this routine instead
of the macro to avoid multiple evaluations of the argument.
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
  return type_ptr;
}  /* f_skip_typerefs */


a_type_ptr skip_typedefs(a_type_ptr type_ptr)
/*
Strip any typedef entries off the given type to get to the real type, and
return a pointer to that.  Note that type qualifiers (const, volatile)
are not dropped here.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref &&
         !type_ptr->variant.typeref.is_const &&
         !type_ptr->variant.typeref.is_volatile) {
    type_ptr = type_ptr->variant.typeref.type;
#if CHECKING
    if (type_ptr == NULL) {
      internal_error("skip_typedefs: NULL referenced type");
    }  /* if */
#endif /* CHECKING */
  }  /* while */
  return type_ptr;
}  /* skip_typedefs */


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


a_boolean is_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an enum type.
*/
{
  tp = skip_typerefs(tp);
  return(is_enum(tp));
}  /* is_enum_type */


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
  return(is_reference_ptr(tp));
}  /* is_reference_type */


a_boolean is_ptr_or_ref_type(a_type_ptr tp)
/*
Return TRUE if the given type is an IL pointer type (i.e., a pointer or
reference).
*/
{
  tp = skip_typerefs(tp);
  return ((tp)->kind == (a_type_kind)tk_pointer);
}  /* is_ptr_or_ref_type */


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
  a_boolean  is_char_array = FALSE;
  a_type_ptr elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    is_char_array = is_character(elem_type);
  }  /* if */
  return is_char_array;
}  /* is_char_array_type */
  
  
static a_boolean is_wchar_t_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of wchar_t.
*/
{
  a_boolean  is_wchar_t_array = FALSE;
  a_type_ptr elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    is_wchar_t_array = is_integral_type(elem_type) &&
                       elem_type->variant.integer.int_kind ==
                                        (an_integer_kind)TARG_WCHAR_T_INT_KIND;
  }  /* if */
  return is_wchar_t_array;
}  /* is_wchar_t_array_type */


a_boolean is_string_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of character (any kind) or
an array of wchar_t.
*/
{
  a_boolean is_string;

  is_string = is_char_array_type(tp) || is_wchar_t_array_type(tp);
  return is_string;
}  /* is_string_type */


a_boolean is_class_struct_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a class, struct, or union type.  Note that
this includes incomplete class, struct, or union types.
*/
{
  tp = skip_typerefs(tp);
  return is_class_struct_union(tp);
}  /* is_class_struct_union_type */


a_boolean is_complete_class_struct_union_type(a_type_ptr tp)
/*
Return TRUE if the type is a complete class, struct, or union type.
*/
{
  tp = skip_typerefs(tp);
  return (!is_incomplete(tp) && is_class_struct_union(tp));
}  /* is_complete_class_struct_union_type */


a_boolean is_illegal_abstract_class_type(a_type_ptr  tp)
/*
There are certain restrictions on the used of an abstract class type.
Specifically, objects of such types may not be created, except insofar as
they are base class subobjects (ARM 10.3).  This is taken to mean that an
array of such objects is also illegal, as well as a pointer to an array
of such objects.  A pointer or reference to such an object is permitted,
however, since it may be a pointer to a subobject.  This function returns
TRUE if the type is that of an abstract class, struct, or union, or if it
is an array of abstract class objects, or if it is pointer or reference
to an array of abstract class objects.
*/
{
  a_boolean is_abstract = FALSE;
  a_boolean array_type_required = FALSE;

  for (;;) {
    tp = skip_typerefs(tp);
    switch (tp->kind) {
      case tk_pointer:
        tp = type_pointed_to(tp);
#if 0
        if (tp->variant.pointer.is_reference) {
          /* Check for NULL pointer in situation where type is being
             constructed but is not yet complete. */
          if (tp == NULL) goto done;
        }  /* if */
#endif
        array_type_required = TRUE;
        break;
      case tk_array:
        tp = tp->variant.array.element_type;
        array_type_required = FALSE;
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        if (tp->variant.class_struct_union.abstract) {
          is_abstract = !array_type_required;
        }  /* if */
      default:
        goto done;
    }  /* case */
  }  /* for */
done:
  return is_abstract;
}  /* is_illegal_abstract_class_type */


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


a_type_ptr type_pointed_to(a_type_ptr pointer_type)
/*
Return the type pointed to by the given tk_pointer type entry.  This can be
a pointer or a reference type.
*/
{
  a_type_ptr tp = skip_typerefs(pointer_type);
#if CHECKING
  if (tp->kind != (a_type_kind)tk_pointer) {
    internal_error("type_pointed_to: not a pointer type");
  }  /* if */
#endif /* CHECKING */
  return tp->variant.pointer.type;
}  /* type_pointed_to */


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


a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                    a_type_ptr base_class)
/*
derived_class and base_class are both class types.  If base_class is a
(direct or indirect) base class of derived_class, return the appropriate
base class entry.  Otherwise, return NULL.  Either class is allowed to
be incomplete (in which case NULL is returned).  In C mode, NULL is always
returned.
*/
{
  a_base_class_ptr bcp = NULL;

  /* Check for C++ mode.  This is important because the class type supplement
     is not allocated in C mode. */
  if (C_dialect == C_dialect_cplusplus) {
    /* Check that both classes are complete, i.e., that their definitions have
       been seen. */
    derived_class = skip_typerefs(derived_class);
    base_class = skip_typerefs(base_class);
    if (derived_class->variant.class_struct_union.extra_info->
                                                         assoc_scope != NULL &&
        base_class->variant.class_struct_union.extra_info->
                                                         assoc_scope != NULL) {
      /* See if the base class appears on the base class list for the derived
         type.  The base class list contains all base classes, both direct
         and indirect. */
      for (bcp = derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->type == base_class) break;
      }  /* for */
    }  /* if */
  }  /* if */
  return bcp;
}  /* find_base_class_of */


a_boolean is_same_class_or_base_class_thereof(a_type_ptr class_1,
                                              a_type_ptr class_2)
/*
Return TRUE if the two classes given are the same class or if class_2 is
a base class of class_1.  Only called in C++ mode.
*/
{
  a_boolean is_same_or_base = FALSE;

  /* Drop typedefs. */
  class_1 = skip_typerefs(class_1);
  class_2 = skip_typerefs(class_2);
  if (class_1 == class_2 || find_base_class_of(class_1, class_2) != NULL) {
    is_same_or_base = TRUE;
  }  /* if */
  return is_same_or_base;
}  /* is_same_class_or_base_class_thereof */


a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                   a_type_ptr       type_2,
                                   a_boolean        *downward_cast,
                                   a_base_class_ptr *bcp)
/*
type_1 and type_2 are pointer types.  Check to see if they are pointers to
related class types, and return TRUE if so.  If they are, set *downcard_cast
if type_1 --> type_2 is a downward cast, and set *bcp to point to the base
class entry that shows the relationship.  Called from the macro
related_class_pointers.
*/
{
  a_boolean  related_classes = FALSE;
  a_type_ptr type_1_pointed_to, type_2_pointed_to;

  *downward_cast = FALSE;
  *bcp = NULL;
  type_1_pointed_to = type_pointed_to(type_1);
  type_2_pointed_to = type_pointed_to(type_2);
  if (is_class_struct_union_type(type_1_pointed_to) &&
      is_class_struct_union_type(type_2_pointed_to)) {
    /* The source and destination types are both pointers to classes.
       See if the classes are related. */
    if ((*bcp = find_base_class_of(type_1_pointed_to,
                                   type_2_pointed_to)) != NULL) {
      related_classes = TRUE;
      *downward_cast = TRUE;
    } else if ((*bcp = find_base_class_of(type_2_pointed_to,
                                          type_1_pointed_to)) != NULL) {
      related_classes = TRUE;
    }  /* if */
  }  /* if */
  return related_classes;
}  /* f_related_class_pointers */


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
    if (is_incomplete(tp) && is_class_struct_union(tp)) {
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


a_type_ptr con_complete_object_type(a_constant_ptr constant)
/*
Return the type of the complete object that contains the location indicated
by constant, or NULL if no complete object can be determined or the constant
is not an address constant.  NULL is always a safe answer; non-NULL values
may permit optimizations.  Note that "complete object" means an object that
is not a base class of another object, not necessarily a top-level object.
This is used only in C++ mode; it is useful to know what the complete object
type is to optimize base class casts and virtual function calls.
*/
{
  a_type_ptr complete_object_type = NULL;

  if (constant->kind == (a_constant_repr_kind)ck_address &&
      constant->variant.address.kind == (an_address_base_kind)abk_variable &&
      constant->variant.address.offset == 0 &&
      !constant->implicit_cast) {
    /* Unmodified address of a variable.  The variable is the complete
       object and its type is the complete object type. */
    complete_object_type = constant->variant.address.variant.variable->type;
  }  /* if */
  return complete_object_type;
}  /* con_complete_object_type */


a_type_ptr node_complete_object_type(an_expr_node_ptr node)
/*
Return the type of the complete object that contains the location indicated
by node (an lvalue address), or NULL if no complete object can be determined.
NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.
*/
{
  a_type_ptr            complete_object_type = NULL;
  an_expr_operator_kind op;
  an_expr_node_ptr      first_operand;

  switch (node->kind) {
    case enk_error:
    case enk_variable:
      /* Complete object not known. */
      break;
    case enk_constant:
      complete_object_type = con_complete_object_type(node->variant.constant);
      break;
    case enk_variable_address:
      /* Address of a variable.  The variable is the complete object and its
         type is the complete object type. */
      complete_object_type = node->variant.variable->type;
      break;
    case enk_operation:
      /* Operator. */
      first_operand = node->variant.operation.operands;
      op = node->variant.operation.kind;
      if (op == (an_expr_operator_kind)eok_field ||
          op == (an_expr_operator_kind)eok_bit_field) {
        /* Field selection (normal or bit-field).  The field itself is a
           complete object (recall that "complete" means "not a base class"
           rather than "not part of another object"). */
        complete_object_type = first_operand->next->variant.field->type;
      } else if (op == (an_expr_operator_kind)eok_base_class_cast) {
        /* Cast to a base class.  Do a recursive call on the first operand
           to find the complete object. */
        complete_object_type = node_complete_object_type(first_operand);
      } else if (op == (an_expr_operator_kind)eok_padd ||
                 op == (an_expr_operator_kind)eok_padd_subsc ||
                 op == (an_expr_operator_kind)eok_psubtract) {
        /* Pointer addition (subscripting) or subtraction.  Do a recursive
           call on the first operand to find the complete object. */
        complete_object_type = node_complete_object_type(first_operand);
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("node_complete_object_type: bad expression kind");
#endif /* CHECKING */
  }  /* switch */
  return complete_object_type;
}  /* node_complete_object_type */


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


a_boolean f_identical_types(a_type_ptr type_1,
                            a_type_ptr type_2,
                            a_boolean  il_identical)
/*
Return TRUE if the two types are identical.  This includes separate copies
of identical types, as well as the case where the pointers point to the
same type.  If il_identical is TRUE, check only that the types are
identical from the point of view of the IL.  Basically, two types are
IL-identical if no cast is needed to assign a value of one type to an entity
of the other type.  This routine should never be called directly; it's
meant to be called only by the macros identical_types and il_identical_types,
which do the initial test for exact pointer equality.
*/
{
  register a_boolean            identical = FALSE;
  a_param_type_ptr              list1, list2;
  a_routine_type_supplement_ptr extra_info1, extra_info2;

  db_enter(4, "f_identical_types");

  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls. */
  if (type_1 == type_2) {
    identical = TRUE;
  } else if (is_const_qualified_type(type_1) !=
                                             is_const_qualified_type(type_2) ||
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
            } else if (il_identical && same_repr_int_types(type_1, type_2)) {
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
          /* For pointers and references, they both be pointers or both
             references and must point to identical types. */
          if (type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference) {
            identical = f_identical_types(type_1->variant.pointer.type,
                                          type_2->variant.pointer.type,
                                          il_identical);
          }  /* if */
          break;
        case tk_array:
          /* For arrays, the sizes must be the same and the element types
             must be identical. */
          if (f_identical_types(type_1->variant.array.element_type,
                                type_2->variant.array.element_type,
                                il_identical) &&
              type_1->variant.array.number_of_elements ==
              type_2->variant.array.number_of_elements) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Classes, structs, and unions that aren't the same type
             aren't identical. */
          break;
        case tk_routine:
          /* For functions, the return types must be identical, the
             parameter lists must be identical, and the implicit "this"
             parameter type (if any) must be identical. */
          extra_info1 = type_1->variant.routine.extra_info;
          extra_info2 = type_2->variant.routine.extra_info;
          if (f_identical_types(type_1->variant.routine.return_type,
                                type_2->variant.routine.return_type,
                                il_identical) &&
              extra_info1->prototyped == extra_info2->prototyped &&
              extra_info1->has_ellipsis == extra_info2->has_ellipsis &&
              ((extra_info1->implicit_this_param_type == NULL) ?
                  (extra_info2->implicit_this_param_type == NULL) :
                  (extra_info2->implicit_this_param_type != NULL &&
                   f_identical_types(extra_info1->implicit_this_param_type,
                                     extra_info2->implicit_this_param_type,
                                     il_identical)))) {
            /* Compare the types of the parameters on the two lists. */
            for (list1 = extra_info1->param_type_list,
                                          list2 = extra_info2->param_type_list;
                 list1 != NULL && list2 != NULL;
                 list1 = list1->next, list2 = list2->next) {
              if (!f_identical_types(list1->type, list2->type, il_identical)) {
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


a_boolean arg_types_are_compatible(a_type_ptr  rout_type1,
                                   a_type_ptr  rout_type2)
/*
rout_type1 and rout_type2 point to routine type entries with parameter
lists that are guaranteed to be prototyped.  Return TRUE if the
parameter lists are compatible.  The "this" parameter types (if any) are
not compared.
*/
{
  a_routine_type_supplement_ptr  extra_info1, extra_info2;
  a_param_type_ptr               list1, list2;
  a_boolean                      compatible;

  extra_info1 = rout_type1->variant.routine.extra_info;
  extra_info2 = rout_type2->variant.routine.extra_info;
  if (extra_info1->has_ellipsis != extra_info2->has_ellipsis) {
    /* One has a variable length parameter list and the other does not, so
       they cannot be compatible. */
    compatible = FALSE;
  } else {
    /* Compare the lists, parameter by parameter. */
    list1 = extra_info1->param_type_list;
    list2 = extra_info2->param_type_list;
    for (; list1 != NULL && list2 != NULL;
         list1 = list1->next, list2 = list2->next) {
      /* Compare the corresponding parameter types. */
      if (!types_are_compatible(list1->type, list2->type)) {
        compatible = FALSE;
        goto done;
      }  /* if */
    }  /* for */
    /* All the parameter types are compatible.  Be sure the lists ended at
       the same time. */
    compatible = (list1 == list2);
  }  /* if */
done:
  return compatible;  
}  /* arg_types_are_compatible */


a_boolean f_types_are_compatible(a_type_ptr type_1,
                                 a_type_ptr type_2)
/*
Compare two types for compatibility.  See section 3.1.2.6 in the standard.
An error type is considered compatible with any other type.  This routine
always checks for compatibility of type-qualifiers.  This routine should
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
          if (C_dialect == C_dialect_cplusplus &&
              (type_1->variant.integer.enum_type ||
               type_2->variant.integer.enum_type)) {
            /* In C++, each enum type is a distinct type and is not compatible
               with any other type. */
          } else {
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
          }  /* if */
          break;
        case tk_float:
          compat = (type_1->variant.float_kind == type_2->variant.float_kind);
          break;
        case tk_pointer:
          /* For pointers and references, they both be pointers or both
             references and must point to compatible types. */
          if (type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference) {
            compat = types_are_compatible(type_1->variant.pointer.type,
                                          type_2->variant.pointer.type);
          }  /* if */
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
          /* Classes, structs, and unions that aren't the same type
             aren't compatible. */
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
              ((extra_info1->implicit_this_param_type == NULL) ?
                  (extra_info2->implicit_this_param_type == NULL) :
                  (extra_info2->implicit_this_param_type != NULL &&
                   types_are_compatible(
                                 extra_info1->implicit_this_param_type,
                                 extra_info2->implicit_this_param_type)))) {
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
    ptr_type1 = skip_typerefs(type_1->variant.pointer.type);
    ptr_type2 = skip_typerefs(type_2->variant.pointer.type);
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
  return interch;
}  /* interchangeable_types */


static a_boolean dest_of_ptr_cast_big_enough(a_type_ptr source_type,
                                             a_type_ptr dest_type)
/*
Return TRUE if a pointer of type "source_type" will fit in an entity of
type "dest_type" (an integral or pointer type).  This is used in testing
whether or not non-portable casts involving pointers should be allowed.
*/
{
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  return (dest_type->size >= source_type->size);
}  /* dest_of_ptr_cast_big_enough */


static a_boolean is_address_of_string_constant(a_constant *constant)
/*
Return TRUE if the given constant is the address of a string constant.
*/
{
  a_boolean is_string;

  is_string  = (constant->kind == (a_constant_repr_kind)ck_address &&
                constant->variant.address.kind ==
                                  (an_address_base_kind)abk_constant &&
                constant->variant.address.variant.constant->kind ==
                                  (a_constant_repr_kind)ck_string);
  return is_string;
}  /* is_address_of_string_constant */


a_boolean impl_pointer_conversion(
                                a_type_ptr    source_type,
                                a_boolean     source_is_constant,
                                a_constant    *source_constant,
                                a_type_ptr    dest_type,
                                a_boolean     check_as_operands_not_conversion,
                                a_boolean     *pointer_normalization_needed,
                                a_boolean     suppress_extensions,
                                an_error_code default_warning_code,
                                an_error_code *warning_suggested)
/*
Return TRUE if it's okay to implicitly convert something of type source_type
(any type) to something of type dest_type (a pointer type).
If source_is_constant is TRUE, the source is a constant, and source_constant
points to the constant value.  (That's needed to check for conversions of a
null pointer constant to a pointer type.)  If check_as_operands_not_conversion
is TRUE, the two types are the types of the operands of an operation; only
do the checks required in that case, which are fewer than the checks required
for a conversion.  *pointer_normalization_needed is returned TRUE if the
conversion involves a pointer normalization (null pointer constant --> pointer
or pointer --> "void *").  suppress_extensions is TRUE if conversions
that are extensions should not be allowed (what constitutes an
extension depends on C_dialect, of course).  If the conversion is
suspect and should be tagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  In strict ANSI mode, if a conversion
flagged with *warning_suggested is done, the warning is required.

Note that any type qualifiers on the types themselves (rather than the
types pointed to) are ignored.

See 4.6 (pointer conversions) and 5.17 (assignment operators) in the ARM,
and 3.3.6 (pointer - pointer), 3.3.8 (relational operators), 3.3.9 (equality
operators), 3.3.15 (?: operator), and 3.3.16.1 (simple assignment).
*/
{
  a_boolean  okay = FALSE;
  a_type_ptr dest_type_pointed_to, source_type_pointed_to;
  a_type_ptr unqual_dest_type_pointed_to, unqual_source_type_pointed_to;

  db_enter(4, "impl_pointer_conversion");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "impl_pointer_conversion: source_type = ");
    db_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *pointer_normalization_needed = FALSE;
  *warning_suggested = ec_no_error;
#if CHECKING
  if (!is_pointer_type(dest_type)) {
    internal_error("impl_pointer_conversion: dest_type is not pointer");
  }  /* if */
#endif /* CHECKING */
  /* Get the type pointed to and drop type qualifiers and typedefs. */
  dest_type_pointed_to = type_pointed_to(dest_type);
  unqual_dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
  if (source_is_constant && is_null_pointer_constant(source_constant)) {
    /* A null pointer constant may be converted to a pointer to any type.
       ANSI C 3.3.9 (equality operators); ANSI C 3.3.15 (?: operator);
       ANSI C 3.3.16.1 (assignment); ARM 4.6 (pointer conversions).
       This test is done early because a null pointer constant may be
       an integer (0) or a pointer ((void *)0). */
    okay = TRUE;
    *pointer_normalization_needed = TRUE;
  } else if (is_pointer(source_type)) {
    /* Pointer --> pointer. */
    /* Get the type pointed to and drop type qualifiers and typedefs. */
    source_type_pointed_to = type_pointed_to(source_type);
    unqual_source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    if (types_are_compatible(unqual_source_type_pointed_to,
                             unqual_dest_type_pointed_to)) {
      /* The types pointed to are compatible, ignoring the type qualifiers.
         ANSI C 3.3.6 (pointer - pointer: caller will check that types are
         object types); ANSI C 3.3.8 (relational operators: caller will check
         that types are both object or both incomplete); ANSI C 3.3.9
         (equality operators); ANSI C 3.3.15 (?: operator); ANSI C 3.3.16.1
         (assignment: preservation of qualifiers is tested below). */
      okay = TRUE;
    } else if (is_error(unqual_dest_type_pointed_to) ||
               is_error(unqual_source_type_pointed_to)) {
      /* Pointer --> pointer-to-error and pointer-to-error --> pointer are
         always allowed. */
      okay = TRUE;
    } else {
      /* The types pointed to are not compatible.  See if the pointers are
         compatible anyway because one or the other is a "void *". */
      if (is_void(unqual_dest_type_pointed_to)) {
        /* Destination type is "void *". */
        if (is_object(unqual_source_type_pointed_to) ||
            is_incomplete(unqual_source_type_pointed_to)) {
          /* In ANSI C, a pointer to an object or incomplete type
             may be converted to a pointer to a qualified or unqualified
             version of void.  ANSI C 3.3.9 (equality operators);
             ANSI C 3.3.15 (?: operator); ANSI C 3.3.16.1 (assignment:
             preservation of qualifiers is tested below).  In C++, a pointer
             to any non-const and non-volatile object type may be converted
             to "void *".  ARM 4.6 (pointer conversions: preservation of
             qualifiers is tested below; "object type" includes incomplete
             types in the ARM definition). */
          okay = TRUE;
          *pointer_normalization_needed = TRUE;
        } else if (C_dialect == C_dialect_cplusplus &&
                   is_function(unqual_source_type_pointed_to)) {
          /* In C++, a pointer to a function may be converted to "void *" if
             the pointer will fit in a "void *".  ARM 4.6 (pointer
             conversions). */
          if (dest_of_ptr_cast_big_enough(source_type, dest_type)) {
            okay = TRUE;
            *pointer_normalization_needed = TRUE;
          }  /* if */
        }  /* if */
      } else if (C_dialect == C_dialect_cplusplus &&
                 is_class_or_struct(unqual_source_type_pointed_to) &&
                 is_class_or_struct(unqual_dest_type_pointed_to) &&
                 find_base_class_of(unqual_source_type_pointed_to,
                                    unqual_dest_type_pointed_to) != NULL) {
        /* In C++, a pointer to a class may be implicitly converted to a
           pointer to an accessible base class of that class provided the
           conversion is unambiguous (ARM 4.6).  We leave the ambiguity
           and accessibility check to be done when the cast is done.
           That's not quite what the ARM says, but it's what cfront does,
           and it makes sense. */
        okay = TRUE;
      } else if (C_dialect != C_dialect_cplusplus &&
                 !check_as_operands_not_conversion &&
                 is_void_type(unqual_source_type_pointed_to) &&
                 is_object(unqual_dest_type_pointed_to) ||
                 is_incomplete(unqual_dest_type_pointed_to)) {
        /* In C but not C++, a "void *" may be converted to a pointer to an
           object or incomplete type.  ANSI C 3.3.16.1 (assignment). */
        okay = TRUE;
      } else if (!suppress_extensions && source_is_constant &&
                 is_address_of_string_constant(source_constant) &&
                 is_character_type(unqual_source_type_pointed_to) &&
                 is_character_type(unqual_dest_type_pointed_to)) {
        /* Allow a character string to be converted to a pointer to any kind
           of char.  This is an extension in both C and C++. */
        okay = TRUE;
        if (strict_ansi_mode) *warning_suggested = default_warning_code;
      } else if (C_dialect == C_dialect_pcc) {
        /* In pcc mode, allow conversion between incompatible pointer types,
           with a warning. */
        okay = TRUE;
        *warning_suggested = default_warning_code;
      } else if (!suppress_extensions &&
                 interchangeable_types(unqual_dest_type_pointed_to,
                                       unqual_source_type_pointed_to)) {
        /* In ANSI C and C++ mode, allow conversion between pointers to
           interchangeable types, as an extension, with a warning.
           This covers cases like unsigned char * --> char *. */
        okay = TRUE;
        *warning_suggested = default_warning_code;
      }  /* if */
    }  /* if */
    if (okay && !check_as_operands_not_conversion) {
      /* The types pointed to must be such that the type pointed to by the
         left has all the qualifiers of the type pointed to by the right.
         It might have additional qualifiers.  ANSI C 3.3.16.1 (assignment);
         ARM 4.6 (pointer conversions: qualifiers cannot be dropped
         implicitly). */
      if ((is_const_qualified_type(source_type_pointed_to) &&
           !is_const_qualified_type(dest_type_pointed_to)) ||
          (is_volatile_qualified_type(source_type_pointed_to) &&
           !is_volatile_qualified_type(dest_type_pointed_to))) {
        okay = FALSE;
      }  /* if */
    }  /* if */
  } else if (C_dialect == C_dialect_pcc &&
             !check_as_operands_not_conversion &&
             is_integral(source_type)) {
    /* In pcc mode, allow integer --> pointer with a warning.  The null
       pointer constant --> pointer case has been handled above and does
       not come here. */
    okay = TRUE;
    *warning_suggested = default_warning_code;
  } else if (is_error(source_type)) {
    /* Error --> pointer is always allowed. */
    okay = TRUE;
  }  /* if */

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "impl_pointer_conversion: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_pointer_conversion */


a_boolean impl_conversion_possible(a_type_ptr    source_type,
                                   a_boolean     source_is_constant,
                                   a_constant    *source_constant,
                                   a_type_ptr    dest_type,
                                   a_boolean     suppress_extensions,
                                   an_error_code default_warning_code,
                                   an_error_code *warning_suggested)
/*
Return TRUE if it is okay to implicitly convert something of type source_type
to something of type dest_type.  If source_is_constant is TRUE, the source
is a constant, and source_constant points to the constant value.  (That's
needed to check for conversions of a null pointer constant to a pointer type.)
Any type qualifiers on the types themselves are ignored.  suppress_extensions
is TRUE if conversions that are extensions should not be allowed (what
constitutes an extension depends on C_dialect, of course).  If the conversion
is suspect and should be tagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  In strict ANSI mode, if a conversion flagged with
*warning_suggested is done, the warning is required.

See chapter 4 of the ARM (standard conversions).  Note that integral
promotions, default argument promotions, the usual arithmetic conversions,
array --> pointer to element, and function --> pointer to function are
handled in normal expression processing rather than here.

See also 3.3.16.1 in the ANSI C standard (simple assignment).
*/
{
  a_boolean      okay = FALSE;
  a_boolean      pointer_normalization_needed;
  a_constant_ptr dest_enum_list, source_enum_list;

  db_enter(4, "impl_conversion_possible");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "impl_conversion_possible: source_type = ");
    db_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  if (is_incomplete(dest_type)) {
    /* Catch cases where an actual argument is being converted to an incomplete
       enum or struct/union type because a function parameter has that type.
       One is allowed to declare a parameter of that type, but the type must
       be completed if the function is defined or called. */
    /* okay = FALSE; -- already set. */
  } else if (is_arithmetic(dest_type)) {
    /* Destination type is arithmetic. */
    if (is_arithmetic(source_type)) {
      /* Arithmetic --> arithmetic.  Okay. */
      okay = TRUE;
      /* Check for conversion of an arithmetic type to an enumerated type,
         which may be invalid or call for a warning. */
      dest_enum_list = NULL;
      if (is_integral(dest_type)) {
        dest_enum_list = dest_type->variant.integer.enum_constant_list;
      }  /* if */
      if (dest_enum_list != NULL) {
        /* Conversion is to an enum type. */
        source_enum_list = NULL;
        if (is_integral(source_type)) {
          source_enum_list = source_type->variant.integer.enum_constant_list;
        }  /* if */
        if (source_enum_list != dest_enum_list) {
          /* Conversion of one enum type to another, or conversion of an
             arithmetic non-enum type to an enum. */
          if (C_dialect == C_dialect_cplusplus) {
            /* In C++, the conversion is not allowed. */
            okay = FALSE;
          } else {
            /* In C, give a warning. */
            *warning_suggested = ec_mixed_enum_type;
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (C_dialect == C_dialect_pcc &&
               is_pointer(source_type) &&
               is_integral(dest_type) &&
               dest_of_ptr_cast_big_enough(source_type, dest_type)) {
      /* In pcc mode, allow pointer --> integer if the integer is big enough.
         Issue a warning. */
      okay = TRUE;
      *warning_suggested = default_warning_code;
    } else {
      /* Non-arithmetic --> arithmetic.  Error. */
      okay = FALSE;
    }  /* if */
  } else if (is_pointer(dest_type)) {
    /* Destination type is pointer.  See if the types are compatible.
       In C, the operands must be pointers to qualified or unqualified
       versions of compatible types (i.e., object, incomplete, or function
       types), and null pointer constants and "void *" pointers are specially
       handled (ANSI C 3.3.15).  Ditto in C++ (ARM 4.6, 5.16). */
    okay = impl_pointer_conversion(source_type, source_is_constant,
                                   source_constant, dest_type,
                                   /*check_as_operands_not_conversion=*/FALSE,
                                   &pointer_normalization_needed,
                                   suppress_extensions,
                                   default_warning_code,
                                   warning_suggested);
  } else if (is_class_struct_union(dest_type)) {
    /* A complete class, struct, or union can be converted to a compatible
       class, struct, or union type. */
    okay = types_are_compatible(dest_type, source_type);
  } else if (is_error(dest_type)) {
    /* Anything can be converted to an error type. */
    okay = TRUE;
  }  /* if */
  /* If compatibility was not found any other way, check for the source
     having an error type. */
  if (!okay && is_error(source_type)) okay = TRUE;

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "impl_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_conversion_possible */


a_boolean expl_conversion_possible(a_type_ptr    source_type,
                                   a_boolean     source_is_constant,
                                   a_constant    *source_constant,
                                   a_type_ptr    dest_type,
                                   an_error_code default_warning_code,
                                   an_error_code *warning_suggested)
/*
Return TRUE if it is okay to explicitly convert something of type source_type
to something of type dest_type.  If source_is_constant is TRUE, the source
is a constant, and source_constant points to the constant value.  (That's
needed to check for conversions of a null pointer constant to a pointer type.)
Any type qualifiers on the types themselves are ignored.  If the conversion
is suspect and should be tagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  In strict ANSI mode, if a conversion flagged with
*warning_suggested is done, the warning is required.

Any implicit conversion is allowed (see impl_conversion_possible).  Also, the
explicit conversions allowed in casts (ARM 5.2.3 and 5.4; ANSI C 3.3.4)
are allowed.
*/
{
  a_boolean     okay = FALSE, impl_okay;
  an_error_code impl_warning_suggested;

  db_enter(4, "expl_conversion_possible");
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "expl_conversion_possible: source_type = ");
    db_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);

  /* See if there is an implicit conversion between the types. */
  impl_okay = impl_conversion_possible(source_type,
                                       source_is_constant, source_constant,
                                       dest_type,
                                       /*suppress_extensions=*/FALSE,
                                       default_warning_code,
                                       &impl_warning_suggested);
  if (impl_okay && impl_warning_suggested == ec_no_error) {
    /* There is an implicit conversion, and it's not questionable. */
    okay = TRUE;
  } else if (is_incomplete(dest_type)) {
    /* Catch cases where an actual argument is being converted to an incomplete
       enum or struct/union type because a function parameter has that type.
       One is allowed to declare a parameter of that type, but the type must
       be completed if the function is defined or called. */
    /* okay = FALSE; -- already set. */
  } else if (is_pointer(source_type) && is_integral(dest_type) &&
             dest_of_ptr_cast_big_enough(source_type, dest_type)) {
    /* Pointer --> integral is okay if the integer is big enough. */
    okay = TRUE;
  } else if (is_integral(source_type) && is_pointer(dest_type)) {
    /* Integral --> pointer. */
    okay = TRUE;
  } else if (is_pointer(source_type) && is_pointer(dest_type)) {
    /* Pointer --> pointer.  Get the types pointed to. */
    a_type_ptr source_type_pointed_to, dest_type_pointed_to;
    source_type_pointed_to = type_pointed_to(source_type);
    source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    dest_type_pointed_to = type_pointed_to(dest_type);
    dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
    if (C_dialect == C_dialect_cplusplus &&
        is_class_or_struct(source_type_pointed_to) &&
        is_class_or_struct(dest_type_pointed_to)) {
      /* Pointer to class --> pointer to class. */
      /* In C++, a pointer to a class can be cast to a pointer to an
         unambiguously derived class if the base class is not
         a virtual base class.  Note that a cast in the other direction
         (derived --> base) would have been let by above as an implicit
         cast. */
      if (find_base_class_of(dest_type_pointed_to,
                             source_type_pointed_to) != NULL) {
        /* We leave the ambiguity and virtual-base check to be done when the
           cast is done. */
        okay = TRUE;
      } else {
        /* All other casts between pointers to classes are valid.  This
           includes cases where one or the other of the classes is not
           defined yet. */
        okay = TRUE;
      }  /* if */
    } else if (is_function(source_type_pointed_to) ==
               is_function(dest_type_pointed_to)) {
      /* Pointer to function --> pointer to function, or pointer to
         object/incomplete --> pointer to object/incomplete.  Allowed in both
         C and C++. */
      okay = TRUE;
    } else {
      /* Pointer to function --> pointer to object/incomplete, or pointer
         to object/incomplete --> pointer to function.  Allowed in C++ if
         the destination is big enough.  Allowed as an extension in C. */
      if (dest_of_ptr_cast_big_enough(source_type, dest_type)) {
        okay = TRUE;
        if (C_dialect != C_dialect_cplusplus) {
          *warning_suggested = ec_mixed_function_object_pointers;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!okay && impl_okay) {
    /* There is a questionable implicit conversion, and no explicit conversion
       that covers this case.  The conversion is allowed, but it's
       questionable.  It's likely that there are no questionable implicit
       conversions that aren't allowed as explicit conversions, but this code
       is here in case one is added. */
    okay = TRUE;
    *warning_suggested = impl_warning_suggested;
  }  /* if */

#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "expl_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* expl_conversion_possible */


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
  an_expr_node_ptr comp_default_arg_expr;

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
          /* Pointer and reference types.  The composite type is a pointer
	     or reference to the composite of the types pointed to. */
          comp_elem = composite_type(base_type_1->variant.pointer.type,
                                     base_type_2->variant.pointer.type);
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new pointer type. */
          if (comp_elem == base_type_1->variant.pointer.type) {
            comp_type = base_type_1;
          } else if (comp_elem == base_type_2->variant.pointer.type) {
            comp_type = base_type_2;
          } else {
	    if (base_type_1->variant.pointer.is_reference) {
              comp_type = make_reference_type(comp_elem);
	    } else {
              comp_type = make_pointer_type(comp_elem);
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
               there is no real parameter information in the composite.
               However, if one or the other has parameter information because
               it's a function with a definition, preserve that information
               in the composite. */
            if (list1 != NULL) {
              comp_param_list = list1;
            } else {
              comp_param_list = list2;
            }  /* if */
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
                 param1 != NULL;
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
                 type.  Stop if it is no longer true that one of the original
                 parameter lists can serve as the composite list. */
              comp_param_type = composite_type(param1->type, param2->type);
              /* Form the composite of the C++ default argument expressions;
                 it's guaranteed that at most one of the parameter lists
                 has a default argument expression. */
              comp_default_arg_expr = (param1->default_arg_expr != NULL) ?
                                                     param1->default_arg_expr :
                                                     param2->default_arg_expr;
              if (comp_param_type != param1->type ||
                  comp_default_arg_expr != param1->default_arg_expr) {
                comp_equals_list1 = FALSE;
              }  /* if */
              if (comp_param_type != param2->type ||
                  comp_default_arg_expr != param2->default_arg_expr) {
                comp_equals_list2 = FALSE;
              }  /* if */
              if (!comp_equals_list1 && !comp_equals_list2) break;
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
              a_param_type_ptr param1_on_which_first_loop_failed = param1;
              comp_param_list = end_comp_param_list = NULL;
              for (param1 = list1,  param2 = list2;
                   param1 != NULL;
                   param1 = param1->next, param2 = param2->next) {
                comp_param = alloc_param_type(/*at_file_scope=*/TRUE);
                if (param1 == param1_on_which_first_loop_failed) {
                  /* Little optimization: when we get to the parameters on
                     which the loop above failed, use the composite type
                     already formed.  This is nice when that type is something
                     distinct from the two parameter types.  Without this
                     trick, that type would be lost. */
                  comp_param->type = comp_param_type;
                } else {
                  /* For the other parameter pairs, we call composite_type.
                     For the parameters preceding the key pair, composite_type
                     will do what it did in the loop above and return one of
                     the original types; for parameters following that pair,
                     composite_type must be called because it has not been
                     called yet for those parameters. */
                  comp_param->type = composite_type(param1->type,
                                                    param2->type);
                }  /* if */
                /* Form the composite of the C++ default argument expressions;
                   it's guaranteed that at most one of the parameter lists
                   has a default argument expression. */
                comp_param->default_arg_expr =
                                           (param1->default_arg_expr != NULL) ?
                                                     param1->default_arg_expr :
                                                     param2->default_arg_expr;
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


static a_boolean types_distinguishable(a_type_ptr orig_type_1,
                                       a_type_ptr orig_type_2,
                                       a_boolean  *params_all_compatible)
/*
Return TRUE if the types type_1 and type_2 are distinguishable by overload
resolution.  If they are not distinguishable and they are not compatible,
set *params_all_compatible to FALSE (but do not change it otherwise; it's
cumulative over all the parameters).
*/
{
  a_boolean  reference_dropped = FALSE, distinguishable = FALSE;
  a_type_ptr type_1 = orig_type_1;
  a_type_ptr type_2 = orig_type_2;

  /* See if one of the types is a reference to the other type,
     e.g., T and T&. */
  if (is_reference_type(type_1)) {
    type_1 = type_pointed_to(type_1);
    reference_dropped = TRUE;
  }  /* if */
  if (is_reference_type(type_2)) {
    type_2 = type_pointed_to(type_2);
    reference_dropped = TRUE;
  }  /* if */
  /* If neither top-level type was a reference, drop the type qualifiers
     (it's impossible to distinguish between T, const T, and volatile T,
     but it's possible to distinguish between T&, const T&, and
     volatile T&). */
  if (!reference_dropped) {
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
  }  /* if */
  /* Now compare the types. */
  if (!types_are_compatible(type_1, type_2)) {
    /* The two types are distinguishable. */
    distinguishable = TRUE;
  } else {
    /* The types are indistinguishable.  See if the original types are
       compatible (meaning the same type, roughly).  This is useful to know
       in issuing the right error message. */
    if (*params_all_compatible &&
        !types_are_compatible(orig_type_1, orig_type_2)) {
      *params_all_compatible = FALSE;
    }  /* if */
  }  /* if */
  return distinguishable;
}  /* types_distinguishable */


a_boolean overload_distinguishable(a_symbol_ptr  old_sym_ptr,
                                   a_type_ptr    new_type,
                                   an_error_code *err_code)
/*
Return TRUE if the new function type new_type is distinguishable under
overload resolution from all the types of the functions indicated by
old_sym_ptr (which might be a simple function or an sk_overloaded_function
symbol).  Otherwise, set *err_code to an appropriate error code
and return FALSE.  We assume that the caller has already determined that
the new type is not compatible with any of the existing types.
Only callable in C++ mode.  See ARM 13.
*/
{
  a_boolean        distinguishable, params_all_compatible;
  a_boolean        old_is_list;
  a_type_ptr       old_type;
  a_param_type_ptr old_param, new_param;
  a_routine_type_supplement_ptr
                   old_extra_info, new_extra_info;
  a_type_ptr       old_this_param_type, new_this_param_type;
  a_boolean        old_this_qualified, new_this_qualified;

  db_enter(5, "overload_distinguishable");
  *err_code = ec_no_error;
  /* See if the old symbol is a list of overloaded functions. */
  if (old_sym_ptr->kind == (a_symbol_kind)sk_overloaded_function) {
    old_is_list = TRUE;
    old_sym_ptr = old_sym_ptr->variant.overloaded_function.symbols;
  } else {
    old_is_list = FALSE;
  }  /* if */
  new_type = skip_typerefs(new_type);
  new_extra_info = new_type->variant.routine.extra_info;
  new_this_param_type = new_extra_info->implicit_this_param_type;
  new_this_qualified = (new_this_param_type != NULL &&
                      is_qualified_type(type_pointed_to(new_this_param_type)));
  do {
    /* See if old_sym_ptr and new_type are distinguishable. */
    distinguishable = FALSE;
    params_all_compatible = TRUE;
    old_type = old_sym_ptr->variant.routine->type;
    old_type = skip_typerefs(old_type);
    old_extra_info = old_type->variant.routine.extra_info;
    /* See if the types are sufficiently different that they are
       distinguishable by overload resolution. */
    /* See if the "this" parameter is distinguishable if it exists.
       Note that if one function has a "this" parameter and the other
       does not, they cannot be distinguished on that basis.
       However, a type qualifier on the "this" parameter type
       makes a nonstatic function different from a static function.
       (The ARM doesn't say that, but cfront seems to do it that
       way; if you change this, see also member_function_redecl_sym,
       which does a similar check.) */
    old_this_param_type = old_extra_info->implicit_this_param_type;
    old_this_qualified = (old_this_param_type != NULL &&
                      is_qualified_type(type_pointed_to(old_this_param_type)));
    if (old_this_qualified != new_this_qualified ||
        (old_this_param_type != NULL && new_this_param_type != NULL &&
         types_distinguishable(old_this_param_type, new_this_param_type,
                               &params_all_compatible))) {
      /* "this" parameter types are distinguishable; this probably means
         one function is const or volatile and the other isn't. */
      distinguishable = TRUE;
      goto distinguishable_determined;
    }  /* if */
    /* If one type has an ellipsis and the other does not, the types are
       distinguishable. */
    if (old_extra_info->has_ellipsis != new_extra_info->has_ellipsis) {
      distinguishable = TRUE;
      goto distinguishable_determined;
    }  /* if */
    /* Compare the parameter types. */
    for (old_param = old_extra_info->param_type_list,
         new_param = new_extra_info->param_type_list;
         old_param != NULL || new_param != NULL;
         old_param = old_param->next, new_param = new_param->next) {
      if (old_param == NULL || new_param == NULL) {
        /* The parameter lists do not end at the same point, so they
           are distinguishable. */
        distinguishable = TRUE;
        goto distinguishable_determined;
      } else {
        /* See if the types are distinguishable. */
        if (types_distinguishable(old_param->type, new_param->type,
                                  &params_all_compatible)) {
          distinguishable = TRUE;
          goto distinguishable_determined;
        }  /* if */
      }  /* if */
    }  /* for */
    /* All the parameters are indistinguishable. */
    if (params_all_compatible) {
      /* The parameter types are not just indistinguishable, they are
         compatible.  This suggests that the user is trying to distinguish
         the function on the basis of the return type, which is not
         valid. */
#if CHECKING
      if (types_are_compatible(old_type->variant.routine.return_type,
                               new_type->variant.routine.return_type)) {
        /* The caller is supposed to have ensured that the case of
           completely compatible function types does not come here, since
           that's a case of redeclaration rather than overloading. */
        internal_error("overload_distinguishable: types compatible");
      }  /* if */
#endif /* CHECKING */
      *err_code = ec_return_type_cannot_distinguish_functions;
    } else {
      /* The parameter lists differ in some way. */
      *err_code = ec_overloaded_function_types_too_similar;
    }  /* if */
  } while (old_is_list && (old_sym_ptr = old_sym_ptr->next) != NULL);
distinguishable_determined:;
  db_exit();
  return distinguishable;
}  /* overload_distinguishable */


static a_param_type_ptr file_scope_param_list(a_param_type_ptr old_param)
/*
Make a file-scope copy of a parameter type list, and return a pointer
to it.
*/
{
  a_param_type_ptr new_param, new_param_list, end_new_param_list;

  new_param_list = end_new_param_list = NULL;
  for (; old_param != NULL; old_param = old_param->next) {
    new_param = alloc_param_type(/*at_file_scope=*/TRUE);
    *new_param = *old_param;
    if (new_param->type != NULL) {
      new_param->type = make_file_scope_type(old_param->type);
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


a_type_ptr make_file_scope_type(a_type_ptr old_type)
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
  a_type_ptr              new_type;
  a_routine_type_supplement_ptr
			  extra_info;
  a_type_kind             kind;
  a_memory_region_number  region_to_switch_back_to;
  a_based_type_kind       based_type_kind;
  a_boolean               is_const, is_volatile;

  /* See if the type entry is already at the file scope, and does not need
     to be copied. */
  if (in_file_scope((char *)old_type)) {
    new_type = old_type;
  } else {
    /* See if there is already a file-scope copy of the type. */
    new_type = get_based_type(old_type,
                              (a_based_type_kind)btk_file_scope_copy);
    if (new_type != NULL) {
      /* Yes.  Use it. */
    } else {
      /* The type entry will have to be copied. */
      kind = old_type->kind;
#if DEBUG
      if (debug_level >= 4) {
        fprintf(f_debug, "make_file_scope_type: copying type with kind = %d\n",
                         (int)kind);
      }  /* if */
#endif /* DEBUG */
      switch_to_file_scope_region(&region_to_switch_back_to);
      new_type = alloc_type(kind);
      switch_back_to_original_region(region_to_switch_back_to);
      /* Remember the location of the file scope copy in case it's ever again
         needed.  Do this early in case the type refers to itself internally,
         to avoid looping. */
      add_based_type_list_member(old_type,
                                 (a_based_type_kind)btk_file_scope_copy,
                                 new_type);
      /* Start with an extra copy of the old type, to ensure that all minor
         flags are copied. */
      copy_type(old_type, new_type);
      /* Clear the source correspondence (it refers to something not at file
         scope, if it refers to anything at all). */
      set_default_source_corresp(&new_type->source_corresp);
      /* Copy the substructure of the type, if any. */
      switch (kind) {
        case tk_error:
        case tk_unknown:
        case tk_void:
        case tk_float:
          /* No extra fields to copy. */
          break;
        case tk_integer:
          new_type->variant.integer.enum_constant_list = 
                                  old_type->variant.integer.enum_constant_list;
#if CHECKING
          if (old_type->variant.integer.enum_type) {
            internal_error(
                          "make_file_scope_copy: enum type not at file scope");
          }  /* if */
#endif /* if */
          break;
        case tk_pointer:
          if (new_type->variant.pointer.is_reference) {
            based_type_kind = (a_based_type_kind)btk_reference;
          } else {
            based_type_kind = (a_based_type_kind)btk_pointer;
          }  /* if */
          new_type->variant.pointer.type =
                          make_file_scope_type(old_type->variant.pointer.type);
          /* Build the proper based_types list entry for the pointer. */
          add_based_type_list_member(new_type->variant.pointer.type,
                                     based_type_kind, new_type);
          break;
        case tk_array:
          new_type->variant.array.element_type =
                    make_file_scope_type(old_type->variant.array.element_type);
          break;
        case tk_routine:
          new_type->variant.routine.return_type =
                   make_file_scope_type(old_type->variant.routine.return_type);
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
                        old_type->variant.routine.extra_info->param_type_list);
          break;
        case tk_typeref:
          new_type->variant.typeref.type =
                          make_file_scope_type(old_type->variant.typeref.type);
          /* Build the proper based_types list entry for the typeref. */
          is_const = new_type->variant.typeref.is_const;
          is_volatile = new_type->variant.typeref.is_volatile;
          if (is_const || is_volatile) {
            if (is_const) {
              if (is_volatile) {
                based_type_kind = (a_based_type_kind)btk_const_volatile;
              } else {
                based_type_kind = (a_based_type_kind)btk_const;
              }  /* if */
            } else {
              based_type_kind = (a_based_type_kind)btk_volatile;
            }  /* if */
            add_based_type_list_member(new_type->variant.typeref.type,
                                       based_type_kind, new_type);
          }  /* if */
          break;
#if CHECKING
        case tk_class:
        case tk_struct:
        case tk_union:
          /* Class/struct/union types should be allocated at the file scope
             and therefore should not need to be copied. */
        default:
          internal_error("make_file_scope_type: bad type kind");
#endif /* CHECKING */
      }  /* switch */
    }  /* if */
  }  /* if */
  return new_type;
}  /* make_file_scope_type */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1991 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
