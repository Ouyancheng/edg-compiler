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

types.c -- Utility routines that check types.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#if !STANDALONE_UTILITY_PROGRAM
#include "class_decl.h"
#include "folding.h"
#include "symbol_ref.h"
#include "templates.h"
#include "func_def.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */
#include "trans_corresp.h"

/*
Macros defining some basic classes of types (see 3.1.2.5).  Defined as
macros to avoid many levels of function calls in evaluating these
predicates.
*/
/* The error type simply has type error. */
#define is_error(tp) ((tp)->kind == (a_type_kind)tk_error)

/* Function types are simply function types. */
#define is_function(tp) ((tp)->kind == (a_type_kind)tk_routine)

/* The void type is simply the void type. */
#define is_void(tp) ((tp)->kind == (a_type_kind)tk_void)

/* tk_integer types include integral types as well as enum types in C++. */
#define type_kind_is_integer(tp) ((tp)->kind == (a_type_kind)tk_integer)

/* Integral types comprise char, the signed and unsigned integer types,
   enumerated types (in C mode), and bool (in C++ mode). */
#define is_integral(tp) \
  (type_kind_is_integer(tp) && \
   (enum_type_is_integral || !(tp)->variant.integer.enum_type))

/* Enum types are integral types that are tagged as enums. */
#define is_enum(tp) \
  (type_kind_is_integer(tp) && (tp)->variant.integer.enum_type)

/* Sometimes useful in C++ since enum types are not integral; in C_mode this
   macro is interchangeable with is_integral (but is more efficient). */
#define is_integral_or_enum(tp) (type_kind_is_integer(tp))

/* The bool type is an integral type that is tagged as bool.  It only
   exists when bool_is_keyword is TRUE. */
#define is_bool(tp) \
  (type_kind_is_integer(tp) && (tp)->variant.integer.bool_type)

/* Character types are three particular integral types. */
#define is_character(tp) \
  (is_integral(tp) && \
   ((tp)->variant.integer.int_kind == (an_integer_kind)ik_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_unsigned_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_signed_char) && \
   !(tp)->variant.integer.wchar_t_type && \
   !(tp)->variant.integer.bool_type)

/* The floating types comprise all sizes of float. */
#define is_real_floating(tp) ((tp)->kind == (a_type_kind)tk_float)

#if C99_IL_EXTENSIONS_SUPPORTED
#define is_imaginary(tp) ((tp)->kind == (a_type_kind)tk_imaginary)
#define is_complex(tp) ((tp)->kind == (a_type_kind)tk_complex)
#define is_nonreal_floating(tp) (is_complex(tp) || is_imaginary(tp))
#define is_floating(tp) (is_real_floating(tp) || is_nonreal_floating(tp))
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define is_floating(tp) (is_real_floating(tp))
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
/* Arithmetic types are the integral types plus the floating types; in C++
   mode enum types are not integral. */
#define is_arithmetic_or_enum(tp) (is_integral_or_enum(tp) || is_floating(tp))

/* The pointer type is simply the pointer type. */
#define is_pointer(tp) ((tp)->kind == (a_type_kind)tk_pointer &&      \
                        !(tp)->variant.pointer.is_reference)

/* The reference type is simply the reference type. */
/* This is called is_reference_ptr because there is a field called
   is_reference in il_def.h and old preprocessors have problems with
   that. */
#define is_reference_ptr(tp) ((tp)->kind == (a_type_kind)tk_pointer &&\
                              (tp)->variant.pointer.is_reference)

/* Scalar types are the arithmetic and enum types and plus the pointer
   types. */
#define is_scalar(tp) (is_arithmetic_or_enum(tp) || is_pointer(tp))

/* Array types are simply array types. */
#define is_array(tp) ((tp)->kind == (a_type_kind)tk_array)

/* VLA (variable-length array) types (called only if is_array() is TRUE). */
#define array_is_vla(tp) ((tp)->variant.array.is_vla)

/* Union types are simply union types. */
#define is_union(tp) ((tp)->kind == (a_type_kind)tk_union)

#define is_class_struct_union(tp) (is_class_or_struct(tp) || is_union(tp))

/* Aggregate types are array and class/struct (but not union) types. */
#define is_aggregate(tp) (is_array(tp) || is_class_or_struct(tp))

/* Union or aggregate types are simply unions or aggregates. */
#define is_aggregate_or_union(tp) (is_aggregate(tp) || is_union(tp))

/* Pointer-to-member type. */
#define is_ptr_to_member(tp) ((tp)->kind == (a_type_kind)tk_ptr_to_member)

/* Object types are non-function and non-reference types that have sizes. */
#define is_object(tp) (!is_function(tp) && !is_reference_ptr(tp) && \
                       (tp)->size != 0)

/* Template parameter type. */
#define is_template_param(tp) ((tp)->kind == (a_type_kind)tk_template_param)

/* Incomplete types are types that have no size and are not functions.
   (In GNU C mode, there are zero-sized arrays and they are considered
   complete.) */
#if !GNU_EXTENSIONS_ALLOWED
#define is_incomplete(tp) ((tp)->size == 0 && !is_function(tp))
#else /* GNU_EXTENSIONS_ALLOWED */
#define is_incomplete(tp)                                               \
   ((tp)->size == 0 && !is_function(tp) &&                              \
    !(is_array(tp) && tp->variant.array.bound_is_zero))
#endif /* GNU_EXTENSIONS_ALLOWED */

/* Macro that is TRUE if two type kinds are the same, or are the same except
   that one is tk_class and the other is tk_struct. */
#define equiv_type_kinds(kind_1, kind_2)				\
  (kind_1 == kind_2 ||							\
  (kind_1 == (a_type_kind)tk_class && kind_2 == (a_type_kind)tk_struct) || \
  (kind_2 == (a_type_kind)tk_class && kind_1 == (a_type_kind)tk_struct))

#if GNU_EXTENSIONS_ALLOWED

/* Macro that is TRUE if the two types have the same type attributes.
   The types are already known not to be typerefs and to have the
   same type kind.  Incomplete types do not have their alignments set
   yet. */
#define same_type_attributes(type_1, type_2) \
  ((type_1)->alignment == (type_2)->alignment || \
   is_incomplete(type_1) || is_incomplete(type_2))

#endif /* GNU_EXTENSIONS_ALLOWED */

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
         !typeref_is_qualified(type_ptr)) {
    type_ptr = type_ptr->variant.typeref.type;
#if CHECKING
    if (type_ptr == NULL) {
      internal_error("skip_typedefs: NULL referenced type");
    }  /* if */
#endif /* CHECKING */
  }  /* while */
  return type_ptr;
}  /* skip_typedefs */


static a_type_ptr skip_typerefs_not_typedefs(a_type_ptr type_ptr)
/*
Strip any non-typedef typeref entries off the given type, and return a
pointer to the underlying type.  This removes cv-qualifiers but not
typedefs.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref &&
         !typeref_is_typedef(type_ptr)) {
    type_ptr = type_ptr->variant.typeref.type;
  }  /* while */
  return type_ptr;
}  /* skip_typerefs_not_typedefs */


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


a_boolean class_type_has_body(a_type_ptr tp)
/*
Return TRUE if the indicated type (a struct, union, or class type) has
a definition.
*/
{
  a_boolean                   has_body;
  a_class_type_supplement_ptr ctsp;

  check_assertion(is_immediate_class_type(tp));
  ctsp = tp->variant.class_struct_union.extra_info;
  has_body = (tp->variant.class_struct_union.field_list != NULL ||
              (ctsp != NULL && ctsp->assoc_scope != NULL));
  return has_body;
}  /* class_type_has_body */


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
Return TRUE if the given type is the void type (3.1.2.5) or a cv-qualified
version thereof.
*/
{
  tp = skip_typerefs(tp);
  return(is_void(tp));
}  /* is_void_type */


a_boolean is_void_star_type(a_type_ptr tp)
/*
Return TRUE if the given type is the void* type.  Note that this does not
allow "const void*" or any other qualified version.
*/
{
  a_boolean is_void_star = FALSE;

  tp = skip_typerefs(tp);
  if (is_pointer(tp)) {
    a_type_ptr ptr_type = type_pointed_to(tp);
    if (is_void_type(ptr_type) && !is_qualified_type(ptr_type)) {
      is_void_star = TRUE;
    }  /* if */
  }  /* if */
  return is_void_star;
}  /* is_void_star_type */


a_boolean is_integral_type(a_type_ptr tp)
/*
Return TRUE if the given type is an integral type (3.1.2.5).
*/
{
  tp = skip_typerefs(tp);
  return(is_integral(tp));
}  /* is_integral_type */


a_boolean is_signed_integral_type(a_type_ptr tp)
/*
Return TRUE if the type is a signed integral type or an enum type whose
underlying type is a signed integral type.
*/
{
  tp = skip_typerefs(tp);
  return (is_integral_or_enum(tp) &&
          int_kind_is_signed[(int)tp->variant.integer.int_kind]);
}  /* is_signed_integral_type */


a_boolean is_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an enum type.
*/
{
  tp = skip_typerefs(tp);
  return(is_enum(tp));
}  /* is_enum_type */


a_boolean is_integral_or_enum_type(a_type_ptr tp)
/*
Return TRUE if the type is an integral type or an enum type.  (In C++ an
enum type is not considered an integral type; in C it is.)
*/
{
  tp = skip_typerefs(tp);
  return(is_integral_or_enum(tp));
}  /* is_integral_or_enum_type */


a_boolean is_bool_type(a_type_ptr tp)
/*
Return TRUE if the given type is the bool type.  The bool type does not exist
when bool_is_keyword is FALSE (which, among other times, means when in C mode).
*/
{
  tp = skip_typerefs(tp);
  return is_bool(tp);
}  /* is_bool_type */


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
Return TRUE if the given type is a floating type.  In C99, that includes
complex and imaginary types.
*/
{
  tp = skip_typerefs(tp);
  return(is_floating(tp));
}  /* is_floating_type */

#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean is_real_floating_type(a_type_ptr tp)
/*
Return TRUE if the given type is a real floating type, i.e., not
complex or imaginary.
*/
{
  tp = skip_typerefs(tp);
  return is_real_floating(tp);
}  /* is_real_floating_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean is_nonreal_floating_type(a_type_ptr tp)
/*
Return TRUE if the given type is a nonreal (imaginary or complex) floating
type.
*/
{
  tp = skip_typerefs(tp);
  return is_nonreal_floating(tp);
}  /* is_nonreal_floating_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean is_imaginary_type(a_type_ptr tp)
/*
Return TRUE if the given type is an imaginary floating type.
*/
{
  tp = skip_typerefs(tp);
  return is_imaginary(tp);
}  /* is_imaginary_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if C99_IL_EXTENSIONS_SUPPORTED

a_boolean is_complex_type(a_type_ptr tp)
/*
Return TRUE if the given type is a complex floating type.
*/
{
  tp = skip_typerefs(tp);
  return is_complex(tp);
}  /* is_complex_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */

a_boolean is_arithmetic_or_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an arithmetic type (3.1.2.5) or an enum
type.  (An enum type *is* an arithmetic type in C but not in C++.)
*/
{
  tp = skip_typerefs(tp);
  return(is_arithmetic_or_enum(tp));
}  /* is_arithmetic_or_enum_type */


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


a_boolean is_vla_type(a_type_ptr  tp)
/*
Return TRUE if the type pointed to by tp is a variable length array type.
If tp is a multidimensional array, return TRUE if any of its dimensions
is nonconstant.
*/
{
  a_boolean is_vla = FALSE;

  if (is_array_type(tp)) {
    do {
      if (array_is_vla(skip_typerefs(tp))) {
        is_vla = TRUE;
        break;
      }  /* if */
      tp = array_element_type(tp);
    } while (tp != NULL && is_array_type(tp));
  }  /* if */
  return is_vla;
}  /* is_vla_type */


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


#if !STANDALONE_UTILITY_PROGRAM
a_boolean is_wchar_t_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of wchar_t.
*/
{
  a_boolean  is_wchar_t_array = FALSE;
  a_type_ptr elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    if (is_integral(elem_type)) {
      if (!wchar_t_is_keyword) {
        /* In C mode, or C++ mode when wchar_t is not a distinct type.
           See if the element type is the appropriate integer kind for
           wchar_t. */
        is_wchar_t_array = elem_type->variant.integer.int_kind ==
                                                        targ_wchar_t_int_kind;
      } else {
        /* In C++ mode when wchar_t is a distinct type.  Make sure this is
           a wchar_t type. */
        is_wchar_t_array = elem_type->variant.integer.wchar_t_type;
      }  /* if */
    }  /* if */
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


a_boolean is_ptrdiff_t_type(a_type_ptr tp)
/*
Return TRUE if the given type is ptrdiff_t, possibly cv-qualified.
*/
{
  a_boolean is_ptrdiff_t;

  tp = skip_typerefs(tp);
  is_ptrdiff_t = is_integral(tp) &&
                 tp->variant.integer.int_kind == targ_ptrdiff_t_int_kind;
  return is_ptrdiff_t;
}  /* is_ptrdiff_t_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_class_struct_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a class, struct, or union type.  Note that
this includes incomplete class, struct, or union types.
*/
{
  tp = skip_typerefs(tp);
  return is_class_struct_union(tp);
}  /* is_class_struct_union_type */


a_boolean is_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a union type.
*/
{
  tp = skip_typerefs(tp);
  return is_union(tp);
}  /* is_union_type */

      
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


a_boolean is_ptr_to_member_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer-to-member type (C++ only).
*/
{
  tp = skip_typerefs(tp);
  return is_ptr_to_member(tp);
}  /* is_ptr_to_member_type */


a_boolean is_template_param_type(a_type_ptr tp)
/*
Return TRUE if the given type is a template parameter type.
*/
{
  tp = skip_typerefs(tp);
  return is_template_param(tp);
}  /* is_template_param_type */


a_boolean is_template_class_type(a_type_ptr tp)
/*
Return TRUE if the given type is a template class type -- an instance
of a class template that has been created, or a nested class of a
class template.
*/
{
  register a_boolean                    result = FALSE;
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    result = tp->variant.class_struct_union.is_template_class;
  }  /* if */
  return result;
}  /* is_template_class_type */


a_boolean is_polymorphic_class_type(a_type_ptr tp)
/*
Return TRUE if the given type is a class type with virtual functions
(including any that may be in base classes).
*/
{
  register a_boolean                    result = FALSE;
  tp = skip_typerefs(tp);
  if (tp->kind == (a_type_kind)tk_class ||
      tp->kind == (a_type_kind)tk_struct ||
      tp->kind == (a_type_kind)tk_union) {
    result = tp->variant.class_struct_union.
                              any_virtual_functions_including_in_base_classes;
  }  /* if */
  return result;
}  /* is_polymorphic_class_type */


a_boolean is_empty_class_type(a_type_ptr type)
/*
Returns TRUE if the type passed as argument is a class type with no nonstatic
data members, no virtual functions or virtual bases, and no nonempty bases.
Otherwise, FALSE is returned.
*/
{
  a_boolean result = TRUE;

  type = skip_typerefs(type);
  if (!is_class_struct_union(type)) {
    result = FALSE;
  } else {
    result = type->variant.class_struct_union.is_empty_class;
  }  /* if */
  return result;
}  /* is_empty_class_type */


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


a_type_ptr underlying_array_element_type(a_type_ptr array_type)
/*
Return the underlying element type of the given array type.
*/
{
  a_type_ptr tp = array_type;
  do {
    tp = array_element_type(tp);
    /* Array-of-NULL is possible while the type is still being constructed. */
    if (tp == NULL) break;
  } while (is_array_type(tp));
  return tp;
}  /* underlying_array_element_type */


a_targ_size_t num_array_elements(a_type_ptr array_type)
/*
Compute and return the number of elements in an array.  For multi-dimensional
arrays, give the total number of elements.
*/
{
  a_targ_size_t num_elements = 1, elems_this_level;

  array_type = skip_typerefs(array_type);
  check_assertion_str(array_type->kind == (a_type_kind)tk_array,
                      "num_array_elements: type not array");
  for (;;) {
    check_assertion(!has_unknown_specified_bound(array_type));
    elems_this_level = array_type->variant.array.variant.number_of_elements;
    check_assertion(elems_this_level > 0);
    num_elements *= elems_this_level;
    array_type = array_type->variant.array.element_type;
    array_type = skip_typerefs(array_type);
    if (!is_array(array_type)) break;
  }  /* for */
  return num_elements;
}  /* num_array_elements */


a_type_ptr find_bottom_of_type(a_type_ptr type)
/*
Find the bottom type of a derived type.
*/
{
  for (;;) {
    a_type_ptr	new_bottom;
    new_bottom = underlying_type_of_derived_type(type);
    if (new_bottom == NULL) break;
    type = new_bottom;
  }  /* for */
  return type;
}  /* find_bottom_of_type */


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


a_type_ptr pm_member_type(a_type_ptr pm_type)
/*
pm_type is a pointer-to-member type.  Return the member type pointed to.
*/
{
  a_type_ptr tp = skip_typerefs(pm_type);

#if CHECKING
  if (tp->kind != (a_type_kind)tk_ptr_to_member) {
    internal_error("pm_member_type: not a pointer to member type");
  }  /* if */
#endif /* CHECKING */
  return tp->variant.ptr_to_member.type;
}  /* pm_member_type */


a_type_ptr pm_class_type(a_type_ptr pm_type)
/*
pm_type is a pointer-to-member type.  Return the class type pointed to.
*/
{
  a_type_ptr tp = skip_typerefs(pm_type);

#if CHECKING
  if (tp->kind != (a_type_kind)tk_ptr_to_member) {
    internal_error("pm_class_type: not a pointer to member type");
  }  /* if */
#endif /* CHECKING */
  return tp->variant.ptr_to_member.class_of_which_a_member;
}  /* pm_class_type */


a_type_ptr underlying_type_of_derived_type(a_type_ptr type)
/*
If type is a derived type, return the type from which it is derived.
Otherwise, return NULL.
*/
{
  switch (type->kind) {
    case tk_pointer:  /* Includes C++ reference too. */
      type = type_pointed_to(type);
      break;
    case tk_ptr_to_member:
      type = pm_member_type(type);
      break;
    case tk_array:
      type = array_element_type(type);
      break;
    case tk_routine:
      type = type->variant.routine.return_type;
      break;
    case tk_typeref:
      type = type->variant.typeref.type;
      break;
    default:
      type = NULL;
      break;
  }  /* switch */
  return type;
}  /* underlying_type_of_derived_type */

#if BACK_END_IS_CP_GEN_BE

a_type_ptr type_specifier_of_type(a_type_ptr type)
/*
Find the specifiers type at the bottom of a type, and return a pointer to
it.  For example, from "array[3] of pointer to const int" one gets back
"const int".
*/
{
  a_type_ptr return_type;

  for (;;) {
    return_type = type;
    /* Remove type qualifiers to see what is underneath.  If what is underneath
       is a derived type we keep going. */
    while (type->kind == (a_type_kind)tk_typeref &&
           !typeref_is_typedef(type)) {
      type = type->variant.typeref.type;
    }  /* while */
    switch (type->kind) {
      case tk_pointer:  /* Includes C++ reference too. */
      case tk_ptr_to_member:
      case tk_array:
      case tk_routine:
        /* Derived type -- keep looping. */
        type = underlying_type_of_derived_type(type);
        break;
      default:
        goto found_specifier_type;
    }  /* switch */
  }  /* for */
found_specifier_type:
  return return_type;
}  /* type_specifier_of_type */

#endif /* BACK_END_IS_CP_GEN_BE */

a_type_qualifier_set f_get_type_qualifiers(a_type_ptr  tp,
                                           a_boolean   top_level)
/*
Form a bit vector representing the type qualifiers on tp.  If top_level is
FALSE and tp is an array, this means checking for a qualifier on the element
type; top_level is usually TRUE in C mode (3.1.2.5).  As a general rule,
macros get_type_qualifiers and get_top_level_type_qualifiers should be
used instead of calling this routine directly.
*/
{
  a_type_qualifier_set  qualifiers = TQ_NONE;

  for (;;) {
    if (tp->kind == (a_type_kind)tk_typeref) {
      /* May be a typedef or a qualification. */
      qualifiers |= tp->variant.typeref.qualifiers;
      tp = tp->variant.typeref.type;
    } else if (!top_level && tp->kind == (a_type_kind)tk_array) {
      /* Check the array element type. */
      tp = tp->variant.array.element_type;
      if (tp == NULL) {
        /* Array-of-NULL is a possible temporary state during construction of
           a derived type. */
        break;
      }  /* if */
    } else {
      break;
    }  /* if */
  }  /* for */
  return qualifiers;
}  /* f_get_type_qualifiers */


a_boolean f_any_qualifier_missing(a_type_ptr  tp1,
                                  a_type_ptr  tp2)
/*
Return TRUE if tp1 does not have some type qualifier that tp2 has.  This
routine should be called via the any_qualifier_missing macro, which checks
that tp2 is a tk_typeref or tk_array.
*/
{
  a_boolean             any_missing;
  a_type_qualifier_set  tp1_qualifiers, tp2_qualifiers;

  tp2_qualifiers = f_get_type_qualifiers(tp2, /*top_level=*/FALSE);
  if (tp2_qualifiers == TQ_NONE) {
    /* tp2 has no qualifiers, so it can't have any that tp1 doesn't have. */
    any_missing = FALSE;
  } else {
    tp1_qualifiers = get_type_qualifiers(tp1);
    any_missing = any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers);
  }  /* if */
  return any_missing;
}  /* f_any_qualifier_missing */


a_boolean is_qualified_version_of_array_typedef(a_type_ptr type,
                                                a_type_ptr *unqual_array_type)
/*
Look at type (a tk_array) to see if it was generated by adding a type qualifier
to a typedef of an array type.  If so, return the original array typedef
in *unqual_array_type and return TRUE.
*/
{
  a_boolean                    is_qualified_array_typedef = FALSE;
  a_based_type_list_member_ptr btlmp;

  *unqual_array_type = NULL;
  for (btlmp = type->based_types; btlmp != NULL; btlmp = btlmp->next) {
    if (btlmp->kind == (a_based_type_kind)btk_unqualified_array_type) {
      a_type_ptr array_type = btlmp->based_type;
      if (array_type->kind == (a_type_kind)tk_typeref &&
          typeref_is_typedef(array_type)) {
        is_qualified_array_typedef = TRUE;
        *unqual_array_type = array_type;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return is_qualified_array_typedef;
}  /* is_qualified_version_of_array_typedef */

#if NEAR_AND_FAR_ALLOWED

a_boolean is_far_type(a_type_ptr tp)
/*
Return TRUE if and only if the indicated type is a "far" type (explicitly or
implicitly).  Used only when support for "near" and "far" is enabled (e.g.,
in 16-bit Microsoft mode).
*/
{
  a_boolean            is_far;
  a_type_qualifier_set qualifiers;

  /* For arrays in C mode, make sure we see the qualifiers on the
     underlying type. */
  qualifiers = f_get_type_qualifiers(tp, /*top_level=*/FALSE);
  check_assertion(near_and_far_enabled());
  if (qualifiers & TQ_NEAR) {
    /* near specified explicitly. */
    is_far = FALSE;
  } else if (qualifiers & TQ_FAR) {
    /* far specified explicitly. */
    is_far = TRUE;
  } else {
    /* near/far are not explicit in the type. */
    /* See if the type is a class with an explicit memory attribute (C++). */
    a_class_type_supplement_ptr ctsp;
    tp = skip_typerefs(tp);
    if (is_class_struct_union(tp) &&
        (ctsp = tp->variant.class_struct_union.extra_info) != NULL &&
        (qualifiers = ctsp->qualifiers) != TQ_NONE) {
      /* A C++ class with a memory attribute specified for all instances of
         the class. */
      is_far = (qualifiers & TQ_FAR) != TQ_NONE;
    } else {
      /* No memory attribute is specified explicitly, so it is implicit.
         Command-line options can specify different sizes for pointers to
         data and pointers to code. */
      if (il_header.far_code_pointers == il_header.far_data_pointers) {
        /* Speed optimization: code and data pointers are the same size so
           there's no need to determine which we have. */
        is_far = il_header.far_data_pointers;
      } else if (is_function(tp)) {
        /* Pointer to code with default size. */
        is_far = il_header.far_code_pointers;
      } else {
        /* Pointer to data with default size. */
        is_far = il_header.far_data_pointers;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_far;
}  /* is_far_type */


a_type_qualifier_set get_original_type_qualifiers(a_type_ptr type)
/*
Get and return the type qualifiers of the indicated type, including any
memory attributes that were explicit in the source but are implicit in
the type itself.  This routine is called only when support for near and far
is enabled (e.g., Microsoft 16-bit mode).
*/
{
  a_type_qualifier_set qualifiers = TQ_NONE;

  /* Loop through the typerefs and accumulate qualifiers. */
  for (;;) {
    if (type->kind == (a_type_kind)tk_typeref) {
      qualifiers |= type->variant.typeref.qualifiers;
      if (type->variant.typeref.explicit_memory_attribute_made_implicit) {
        /* A memory attribute was explicitly specified in the source but
           it's implied in the typeref.  Add it in. */
        qualifiers |= is_far_type(type->variant.typeref.type) ? TQ_FAR :
                                                                TQ_NEAR;
      }  /* if */
      type = type->variant.typeref.type;
    } else if (type->kind == (a_type_kind)tk_array) {
      /* If an array appears, the new qualifiers must be compatible with those
         on the element type.  This is true in both C and C++. */
      type = array_element_type(type);
    } else {
      break;
    }  /* if */
  }  /* for */
  return qualifiers;
}  /* get_original_type_qualifiers */
    
#endif /* NEAR_AND_FAR_ALLOWED */
#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_abstract_class_type(a_type_ptr  tp)
/*
Return TRUE if tp is a class/struct/union type for which the abstract
flag is set to TRUE.
*/
{
  a_boolean  is_abstract;

  tp = skip_typerefs(tp);
  if (is_class_struct_union(tp)) {
    is_abstract = tp->variant.class_struct_union.abstract;
  } else {
    is_abstract = FALSE;
  }  /* if */
  return is_abstract;
}  /* is_abstract_class_type */


a_boolean f_type_has_default_constructor(a_type_ptr  tp,
                                         a_boolean   user_declared_only,
                                         a_boolean   nontrivial_only)
/*
Return TRUE if the type pointed to by tp is a non-POD class type with a
default constructor (or an array thereof).  When user_declared_only is TRUE,
the function returns TRUE if class has a user-declared default constructor.
When nontrivial_only is TRUE, it returns TRUE if the class has a nontrivial
default constructor (user-declared or implicitly-generated).  If both flags
are FALSE, it also considers trivial_default_constructor pointer in the class
symbol supplement. This function is called in C++ mode only, and only through
one of the macros provided in types.h (type_has_default_constructor, etc.).
*/
{
  a_boolean                      has_default_ctor = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  if (is_array_type(tp)) {
    tp = underlying_array_element_type(tp);
  }  /* if */
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    /* It's a class type or an array of class type. */
    cssp = symbol_supplement_for_class(tp);
    if (cssp->has_user_declared_default_constructor) {
      /* Class has a user-declared default constructor. */
      has_default_ctor = TRUE;
    } else if (cssp->has_nontrivial_default_constructor) {
      /* Class has an implicitly declared nontrivial default constructor. */
      if (!user_declared_only) has_default_ctor = TRUE;
    } else if (cssp->trivial_default_constructor != NULL) {
      /* Class has an implicitly declared trivial default constructor. */
      if (!user_declared_only && !nontrivial_only) has_default_ctor = TRUE;
    }  /* if */
  }  /* if */
  return has_default_ctor;
}  /* f_type_has_default_constructor */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_base_class_ptr find_base_class_of_full(a_type_ptr derived_class,
                                         a_type_ptr base_class,
                                         a_boolean  instantiate_if_necessary)
/*
derived_class and base_class are both class types.  If base_class is a
(direct or indirect) base class of derived_class, return the appropriate
base class entry.  Otherwise, return NULL.  Either class is allowed to
be incomplete (in which case NULL is returned).  In C mode, NULL is always
returned.  In C++ mode, if instantiate_if_necessary is TRUE, the derived
class will be instantiated if necessary so that its base classes are known.
*/
{
  a_base_class_ptr bcp = NULL;

  /* Check for C++ mode.  This is important because the class type supplement
     is not allocated in C mode. */
  if (C_dialect == C_dialect_cplusplus) {
    derived_class = skip_typerefs(derived_class);
    base_class = skip_typerefs(base_class);
    if (instantiate_if_necessary) {
#if !STANDALONE_UTILITY_PROGRAM
      /* Force instantiation of the derived type if it is an uninstantiated
         template class.  This is necessary so that we can see what its base
         classes are.  Note that this can potentially force instantiation
         of the base class as well. */
      instantiate_template_class(derived_class);
#else /* STANDALONE_UTILITY_PROGRAM */
      unexpected_condition();
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */
    /* Check that both classes are complete, i.e., that their definitions have
       been seen. */
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
        if (same_entities(bcp->type, base_class)) break;
      }  /* for */
    }  /* if */
  }  /* if */
  return bcp;
}  /* find_base_class_of_full */

#if !STANDALONE_UTILITY_PROGRAM

a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                    a_type_ptr base_class)
/*
Like find_base_class_full, with instantiate_if_necessary set to TRUE.
*/
{
  a_base_class_ptr bcp =
                    find_base_class_of_full(derived_class, base_class,
                                            /*instantiate_if_necessary=*/TRUE);
  return bcp;
}  /* find_base_class_of */


a_base_class_ptr find_direct_base_class_of(a_type_ptr  derived_class,
                                           a_type_ptr  base_class_type)
/*
Return a pointer to the direct base class of derived_class with a type
identical to base_class_type.  Return NULL if none is found.
*/
{
  a_base_class_ptr  bcp;

  db_enter(4, "find_direct_base_class_of");
  bcp = base_classes_of(derived_class);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && same_entities(bcp->type, base_class_type)) break;
  }  /* for */
  db_exit();
  return bcp;
}  /* find_direct_base_class_of */


a_boolean is_on_any_derivation_of(a_base_class_ptr  bcp,
                                  a_base_class_ptr  ref_bcp)
/*
Return TRUE if ref_bcp appears as a step on any derivation of bcp.
*/
{
  a_boolean                    found;
  a_base_class_derivation_ptr  bcdp;
  a_derivation_step_ptr        step;
  a_base_class_ptr             start_bcp;

  if (ref_bcp == bcp) {
    /* They're the same base class -- return TRUE. */
    found = TRUE;
  } else {
    found = FALSE;
    /* Loop through all the derivations of bcp and search the paths for a
       match. */
    for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
      /* First examine each step in the current path segment. */
      for (step = bcdp->path; step != NULL; step = step->next) {
        if (step->base_class == ref_bcp) {
          found = TRUE;
          goto done;
        } else if (ref_bcp->direct || ref_bcp->is_virtual) {
          /* If ref_bcp is a direct or virtual base class, it would have to
             be the first entry in the list. */
          break;
        }  /* if */
      }  /* for */
      /* If the current derivation is not direct, it may start with a virtual
         base class whose own derivation(s) should be scanned as well. */
      if (!bcdp->direct) {
        /* Set start_bcp to the first base class in this path. */
        start_bcp = bcdp->path->base_class;
        if (start_bcp->is_virtual &&
            is_on_any_derivation_of(start_bcp, ref_bcp)) {
          found = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return found;
}  /* is_on_any_derivation_of */


a_base_class_ptr corresponding_base_class(a_base_class_ptr  base_class,
                                          a_type_ptr        new_class,
                                          a_base_class_ptr  disambiguator)
/*
Find the base class under new_class that is the same as the base class
indicated by base_class, and return a pointer to it.  The base class must
be found.  There may be more than one base class that matches; in that
case, disambiguator (if non-null) may used to decide which to use -- it is
also a base class of new_class, and its presence as the immediately preceding
step on the derivation list serves to confirm the match. 
*/
{
  a_base_class_ptr       new_base_class, bcp;
  a_derivation_step_ptr  step;

  db_enter(4, "corresponding_base_class");
#if CHECKING
  /* Be sure the disambiguator is a base class of new_class. */
  if (disambiguator != NULL &&
      !same_entities(disambiguator->derived_class, new_class)) {
    internal_error("corresponding_base_class: bad disambiguator");
  }  /* if */
#endif /* CHECKING */
  if (same_entities(base_class->derived_class, new_class)) {
    /* base_class is already a base class of new_class.  Just return it. */
    new_base_class = base_class;
    goto done;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fputs("looking in \"", f_debug);
    db_type_name(new_class);
    fputs("\" for a base class corresponding to:\n  ", f_debug);
    db_base_class(base_class, FALSE);
    if (disambiguator != NULL) {
      fputs("  disambiguator is ", f_debug);
      db_base_class(disambiguator, FALSE);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Look for a match among the base classes of new_class. */
  for (bcp = base_classes_of(new_class); bcp != NULL; bcp = bcp->next) {
    /* The first requirement of a match is that the type of the matching
       base class be the same as the type of the reference base_class. */
    if (same_entities(bcp->type, base_class->type)) {
      /* The types match. */
      if (base_class->is_virtual) {
        if (bcp->is_virtual) {
          /* Both are virtual, so they match. */
          new_base_class = bcp;
          goto done;
        }  /* if */
      } else if (bcp->is_virtual) {
        /* base_class is nonvirtual, so keep looking for a nonvirtual with
           the same type. */
      } else if (base_class->direct) {
        /* base_class is a direct base class, so it has a trivial derivation
           path.  See if the class of which bcp is a direct base class is the
           same that of which base_class is a direct base class. */
        if (bcp->direct) {
          /* bcp has a trivial derivation, too. */
          if (bcp->ambiguous) {
            if (disambiguator != NULL) {
              /* Keep looking. */
            } else {
#if 0
/* It's not clear what the right thing to do is when bcp is ambiguous and
   there's no disambiguator.  Should we arbitrarily choose one? */
#endif /* if 0 */
              new_base_class = bcp;
              goto done;
            }  /* if */
          } else {
            new_base_class = bcp;
            goto done;
          }  /* if */
        } else {
          /* Find the immediate predecessor in bcp's derivation path.  Note
             that bcp is nonvirtual, so we don't need to worry about multiple
             paths in looking for its immediate predecessor. */
          step = bcp->derivation->path;
          for (; step->next->base_class != bcp; step = step->next) {}
          if (same_entities(step->base_class->type,
                            base_class->derived_class)) {
            /* If bcp is ambiguous use the disambiguator to determine whether
               we have a match.  It that will be the immediate predecessor of
               bcp on bcp's derivation path. */
            if (bcp->ambiguous && disambiguator != NULL &&
                step->base_class != disambiguator) {
              /* Fails the disambiguation test. */
            } else {
              new_base_class = bcp;
              goto done;
            }  /* if */
          }  /* if */
        }  /* if */
      } else if (!bcp->ambiguous && !base_class->ambiguous) {
        /* Neither base class is ambiguous. */
        new_base_class = bcp;
        goto done;
      } else {
        /* One or both of the base classes is ambiguous.  That means there
           is more than one instance of the base class in the base classes
           list.  Check the derivations to resolve the ambiguity. */
        if (disambiguator != NULL) {
          if (bcp->direct) {
            /* A direct base class cannot have a disambiguator, since only
               the most derived class will be derived from it.  Move on to
               the next one. */
          } else {
            /* Find the immediate predecessor in bcp's derivation path.  Note
               that bcp is nonvirtual, so we don't need to worry about multiple
               paths in looking for its immediate predecessor. */
            step = bcp->derivation->path;
            for (; step->next->base_class != bcp; step = step->next) {}
            if (step->base_class == disambiguator) {
              new_base_class = bcp;
              goto done;
            }  /* if */
          }  /* if */
        } else {
          /* Neither bcp nor base_class is virtual, one or both is ambiguous,
             and there is no disambiguator.  The last avenue for confirming
             a match is to check for path congruence, including cases where
             the paths are congruent once we move far enough along bcp's
             derivation -- for instance, if the derivation of bcp is A==>B==>C
             and the derivation of base_class is B==>C. */
          step = bcp->derivation->path;
          for (; step != NULL; step = step->next) {
            if (congruent_paths(step, base_class->derivation->path)) {
              new_base_class = bcp;
              goto done;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
#if CHECKING
#if DEBUG
  if (debug_level > 0) {
    if (base_class != NULL) {
      fputs("cannot find base class", f_debug);
      db_base_class(base_class, /*show_offset=*/FALSE);
    }  /* if */
    fputs("new_class = ", f_debug);
    db_type_name(new_class);
    fputs(" with base classes:\n", f_debug);
    for (bcp = base_classes_of(new_class); bcp != NULL; bcp = bcp->next) {
      fputs("  ", f_debug);
      db_base_class(bcp, /*show_offset=*/FALSE);
    }  /* for */
    if (disambiguator != NULL) {
      fputs("disambiguator = ", f_debug);
      db_base_class(disambiguator, /*show_offset=*/FALSE);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  internal_error("corresponding_base_class: base class not found");
#else /* CHECKING */
  new_base_class = NULL;
#endif /* CHECKING */
done:
#if DEBUG
  if (debug_level >= 4 &&
      !same_entities(base_class->derived_class, new_class)) {
    fputs("found base class: ", f_debug);
    db_base_class(new_base_class, /*show_offset=*/FALSE);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return new_base_class;
}  /* corresponding_base_class */


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
  if (same_entities(class_1, class_2) ||
      find_base_class_of(class_1, class_2) != NULL) {
    is_same_or_base = TRUE;
  }  /* if */
  return is_same_or_base;
}  /* is_same_class_or_base_class_thereof */


a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                   a_type_ptr       type_2,
                                   a_boolean        *baseward_cast,
                                   a_base_class_ptr *bcp)
/*
type_1 and type_2 are pointer types.  Check to see if they are pointers to
related class types, and return TRUE if so.  If they are, set *baseward_cast
if type_1 --> type_2 is a baseward cast, and set *bcp to point to the base
class entry that shows the relationship.  Called from the macro
related_class_pointers.
*/
{
  a_boolean  related_classes = FALSE;
  a_type_ptr type_1_pointed_to, type_2_pointed_to;

  *baseward_cast = FALSE;
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
      *baseward_cast = TRUE;
    } else if ((*bcp = find_base_class_of(type_2_pointed_to,
                                          type_1_pointed_to)) != NULL) {
      related_classes = TRUE;
    }  /* if */
  }  /* if */
  return related_classes;
}  /* f_related_class_pointers */


a_boolean f_rel_member_pointers(a_type_ptr       type_1,
                                a_type_ptr       type_2,
                                a_boolean        *baseward_cast,
                                a_base_class_ptr *bcp)
/*
type_1 and type_2 are pointer to member types.  Check to see if they are
pointers to related class types, and return TRUE if so.  If they are,
set *baseward_cast if type_1 --> type_2 is a baseward cast, and set *bcp
to point to the base class entry that shows the relationship.  Note that
the member types are not compared.  Called from the macro
related_member_pointers.  Note that this DOES NOT test that the underlying
member types are the same.
*/
{
  a_boolean  related_pointers = FALSE;
  a_type_ptr class_1, class_2;

  *baseward_cast = FALSE;
  *bcp = NULL;
  /* See if the classes are related. */
  class_1 = pm_class_type(type_1);
  class_2 = pm_class_type(type_2);
  if ((*bcp = find_base_class_of(class_1, class_2)) != NULL) {
    related_pointers = TRUE;
    *baseward_cast = TRUE;
  } else if ((*bcp = find_base_class_of(class_2, class_1)) != NULL) {
    related_pointers = TRUE;
  }  /* if */
  return related_pointers;
}  /* f_rel_member_pointers */


a_boolean type_masks_handler_param_type(a_type_ptr  type_1,
                                        a_type_ptr  type_2)
/*
type_1 and type_2 are the types of handlers in a given try block, with
the handler for type_1 appearing before that of type_2.  Return TRUE
if type_1 masks type_2 -- i.e., if type_2's handler can never be invoked
because any exception it can handle would be caught by type_1's handler.
(See ARM 15.4.)
*/
{
  a_boolean         masked = FALSE;
  a_base_class_ptr  bcp;

  db_enter(5, "type_masks_handler_param_type");
  /* "Reference" on top of a type is ignored for handlers as are type
      qualifiers. */
  if (is_reference_type(type_1)) type_1 = type_pointed_to(type_1);
  type_1 = skip_typerefs(type_1);
  if (is_reference_type(type_2)) type_2 = type_pointed_to(type_2);
  type_2 = skip_typerefs(type_2);
  if (identical_types(type_1, type_2)) {
    /* A type is masked by another type that is the same. */
    masked = TRUE;
  } else {
    /* A handler for a derived class is masked by a handler for a base
       class, and a handler for a pointer-to-derived-class is masked by a
       handler for a pointer-to-base-class. */
    if (is_pointer_type(type_1) &&
        is_pointer_type(type_2)) {
      a_type_ptr  type_1_pointed_to = type_pointed_to(type_1);
      a_type_ptr  type_2_pointed_to = type_pointed_to(type_2);

      type_1_pointed_to = skip_typerefs(type_1_pointed_to);
      type_2_pointed_to = skip_typerefs(type_2_pointed_to);
      if (is_immediate_class_type(type_1_pointed_to) &&
          is_immediate_class_type(type_2_pointed_to)) {
        type_1 = type_1_pointed_to;
        type_2 = type_2_pointed_to;
      }  /* if */
    }  /* if */
    if (is_class_struct_union_type(type_1) &&
        is_class_struct_union_type(type_2)) {
      bcp = find_base_class_of(type_2, type_1);
      if (bcp != NULL) {
        /* A handler for a base class masks the handler for the derived class
           unless the base class is inaccessible or ambiguous. */
        if (!bcp->ambiguous && is_accessible_base_class(bcp)) masked = TRUE;
      }  /* if */
    } else if (is_pointer_type(type_1) && is_pointer_type(type_2)) {
      /* A pointer-type masks another pointer-type if the latter can be
         implicitly converted to the former.  (This is not explicit in
         the working paper or the ARM and may turn out to be an incorrect
         inference; see 15.4 para 1 and para 2.) */
      a_std_conv_descr std_conv;

      if (impl_pointer_conversion(type_2, /*source_is_constant=*/FALSE,
                                  /*source_is_string_literal=*/FALSE,
                                  (a_constant_ptr)NULL, type_1,
                                  /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                  /*suppress_extensions=*/TRUE,
                                  ec_no_error, &std_conv)) {
        masked = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return masked;
}  /* type_masks_handler_param_type */


a_boolean set_array_type_size(a_type_ptr	array_type,
			      a_boolean		suppress_error)
/*
Compute and set the size and alignment of the array type pointed to by
array_type.  Return TRUE if the array size is acceptable.  If the
array size is too large, the array type is converted to an error type
and a diagnostic is issued (unless suppress_error is TRUE).
*/
{
  a_type_ptr     underlying_elem_type;
  a_targ_size_t  temp, temp2;
  a_type_ptr     elem_type;
  a_boolean	 okay = TRUE;

  db_enter(5, "set_array_type_size");

  underlying_elem_type = underlying_array_element_type(array_type);
  underlying_elem_type = skip_typerefs(underlying_elem_type);
  if (is_incomplete(underlying_elem_type) &&
      (is_immediate_class_type(underlying_elem_type) ||
       is_immediate_enum_type(underlying_elem_type))) {
    /* This is an array whose element type (directly or indirectly) is an
       incomplete class or enum type.  The size cannot be determined now.
       The array type is put on a list so it can be fixed later if the
       element type is defined.  (Note that an array of incomplete struct is
       an extension in C, but it's standard in C++; array on incomplete enum
       is an extension in both languages.) */
    add_to_dependent_type_fixup_list(underlying_elem_type,
                                     (a_dependent_type_fixup_kind)
                                                dtfk_array_type_size,
                                     (char *)array_type,
                                     (a_byte_il_entry_kind)iek_type,
                                     &error_position);
  } else {
    /* Get the number of elements.  Note that this is zero for an incomplete
       type like int a[]. */
    if (!has_unknown_specified_bound(array_type)) {
      temp = array_type->variant.array.variant.number_of_elements;
    } else {
      /* We don't know the element count because it is not a constant value.
         Since a size of zero means "incomplete type", set the size as though
         the element count were 1. */
      temp = 1;
    }  /* if */
    /* Next get the size of an element.  If it is itself an array, its own
       size may need to be set. */
    elem_type = array_type->variant.array.element_type;
#if CHECKING
    if (elem_type == NULL) {
      internal_error("set_array_type_size: NULL element type");
    }  /* if */
#endif /* CHECKING */
    elem_type = skip_typerefs(elem_type);
    if (is_array_type(elem_type)) {
      set_type_size(elem_type);
    } else if (is_abstract_class_type(elem_type)) {
      /* error_position should already be set correctly. */
      report_abstract_class_error(ec_array_of_abstract_class, elem_type,
                                  &error_position);
    }  /* if */
    temp2 = elem_type->size;
#if CHECKING
    if (temp2 == 0 &&
        !(is_array_type(elem_type) &&
          elem_type->variant.array.bound_is_zero)) {
      internal_error("set_array_type_size: bad element type");
    }  /* if */
#endif /* CHECKING */
    /* Check whether or not the multiplication will overflow.  Note that we 
       avoid dividing by temp, since it may be zero for an incomplete type.
       temp2 can be zero too if the element type is a GNU C zero-length
       array. */
    if (temp2 != 0 && temp > targ_size_t_max / temp2) {
      if (!suppress_error) error(ec_array_size_too_large);
      set_type_kind(array_type, (a_type_kind)tk_error);
      set_type_size(array_type);
      okay = FALSE;
    } else {
      /* Now that we know the multiplication will not overflow, compute the
         array size. */
      array_type->size = temp*temp2;
      /* The alignment for the array is the same as the alignment for the
         elements. */
      array_type->alignment = elem_type->alignment;
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* set_array_type_size */


void set_type_size(a_type_ptr type_ptr)
/*
Compute and set the size of the type pointed to by type_ptr.  If it is already
set, leave it alone.  Also compute and set the alignment requirement.
*/
{
  a_targ_size_t    size;
  a_targ_alignment alignment;

  db_enter(5, "set_type_size");
  size = type_ptr->size;
  /* If the size is set already, leave it alone. */
  if (size == 0) {
    alignment = 1;  /* Default */
    switch(type_ptr->kind) {
      case tk_error:
      case tk_unknown:
      case tk_template_param:
        /* Use an arbitrary non-zero size for an error type.  This is
           important so that error types do not appear to be incomplete
           types.  The same holds for template parameter types (which are
           really placeholders) and unknown types. */
        size = 1;
        break;
      case tk_void:
      case tk_routine:
      case tk_typeref:
        /* These stay zero; they have no size directly. */
        break;
      case tk_integer:
        get_integer_size_and_alignment(type_ptr->variant.integer.int_kind,
                                       &size, &alignment);
        break;
      case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
      case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        switch (type_ptr->variant.float_kind) {
          case fk_float:
            size = targ_sizeof_float;
            alignment = targ_alignof_float;
            break;
          case fk_double:
            size = targ_sizeof_double;
            alignment = targ_alignof_double;
            break;
          case fk_long_double:
            size = targ_sizeof_long_double;
            alignment = targ_alignof_long_double;
            break;
#if CHECKING
          default:
            internal_error("set_type_size: bad float kind");
#endif /* CHECKING */
        }  /* switch */
#if C99_IL_EXTENSIONS_SUPPORTED
        if (type_ptr->kind == (a_type_kind)tk_complex) size *= 2;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        break;
      case tk_pointer:
        size = size_of_pointer_to(type_pointed_to(type_ptr), &alignment);
        break;
      case tk_array:
        (void)set_array_type_size(type_ptr, /*suppress_error=*/FALSE);
        goto size_already_set;
      case tk_ptr_to_member:
        if (is_function_type(pm_member_type(type_ptr))) {
          /* Pointer to nonstatic member function. */
          size = targ_sizeof_ptr_to_member_function;
          alignment = targ_alignof_ptr_to_member_function;
        } else {
          /* Pointer to nonstatic data member. */
          size = targ_sizeof_ptr_to_data_member;
          alignment = targ_alignof_ptr_to_data_member;
        }  /* if */
        break;
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
as the original type.  See also type_after_bit_field_integral_promotion for
integral promotions for bit field expressions.  Note that this routine
expects to receive an rvalue type.
*/
{
  a_type_ptr promoted_type = type;
  a_type_ptr unqual_type = skip_typerefs(type);

  db_enter(5, "type_after_integral_promotion");

  if (is_integral_or_enum(unqual_type)) {
    an_integer_kind ikind = unqual_type->variant.integer.int_kind;
    if (unqual_type->variant.integer.bool_type) {
      /* bool always promotes to int. */
      promoted_type = integer_type((an_integer_kind)ik_int);
    } else if (!C_mode() &&
                (unqual_type->variant.integer.enum_type ||
                 unqual_type->variant.integer.wchar_t_type) &&
                targ_sizeof_int == targ_sizeof_long &&
               (ikind == (an_integer_kind)ik_long ||
                ikind == (an_integer_kind)ik_unsigned_long)) {
      /* In C++, enums and wchar_t (when a keyword) go through the
         normal promotion processing for the underlying type.  If the type
         is unchanged, it will be converted to the corresponding plain
         integral type (see below).  However, there's one anomaly:
         if long and int are the same size, enums and wchar_t of
         size long promote to int or unsigned int. */
      if (ikind == (an_integer_kind)ik_long) {
        promoted_type = integer_type((an_integer_kind)ik_int);
      } else { /* ikind == (an_integer_kind)ik_unsigned_long */
        promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
      }  /* if */
    } else {
      switch (ikind) {
        case ik_char:
          if (targ_has_signed_chars) goto do_signed_char;
          goto do_unsigned_char;
        case ik_unsigned_char:
do_unsigned_char:
          if (C_dialect == C_dialect_pcc) {
            /* In pcc mode, unsigned char is promoted to unsigned int. */
            promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
          } else {
            /* In ANSI mode, unsigned char is promoted to int if all values
               of type unsigned char can be represented in an int; otherwise
               unsigned char is promoted to unsigned int. */
            if (targ_sizeof_int > 1) {
              /* All values of type unsigned char can fit in an int, so
                 unsigned char is promoted to int. */
              promoted_type = integer_type((an_integer_kind)ik_int);
            } else {
              /* int and char are the same size, so unsigned char is promoted
                 to unsigned int. */
              promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
            }  /* if */
          }  /* if */
          break;
        case ik_signed_char:
do_signed_char:;
        /*FALLTHROUGH*/
        case ik_short:
          /* Signed char and signed short are promoted to int. */
          promoted_type = integer_type((an_integer_kind)ik_int);
	  break;
        case ik_unsigned_short:
          if (C_dialect == C_dialect_pcc) {
            /* In pcc mode, unsigned short is promoted to unsigned int. */
            promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
          } else {
            /* In ANSI mode, unsigned short is promoted to int if all values
               of type unsigned short can be represented in an int; otherwise
               unsigned short is promoted to unsigned int. */
            if (targ_sizeof_int > targ_sizeof_short) {
              /* All values of type unsigned short can fit in an int, so
                 unsigned short is promoted to int. */
              promoted_type = integer_type((an_integer_kind)ik_int);
            } else {
              /* int and short are the same size, so unsigned short is promoted
                 to unsigned int. */
              promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
            }  /* if */
          }  /* if */
          break;
        case ik_int:
        case ik_unsigned_int:
        case ik_long:
        case ik_unsigned_long:
#if LONG_LONG_ALLOWED
        case ik_long_long:
        case ik_unsigned_long_long:
#endif /* LONG_LONG_ALLOWED */
          /* These are deliberately left as they are; they are not supposed
             to be promoted. */
          break;
#if CHECKING
        default:
          internal_error("type_after_integral_promotion: bad int kind");
#endif /* CHECKING */
      }  /* switch */
      if (C_dialect == C_dialect_cplusplus) {
        /* enums and wchar_t get promoted to the corresponding integral type
           (and lose their special properties) if they were not promoted
           above. */
        unqual_type = skip_typerefs(promoted_type);
        if (unqual_type->variant.integer.enum_type ||
            unqual_type->variant.integer.wchar_t_type) {
          /* Make a "plain" version of this type, i.e., the same underlying
             integral type but not tagged as an enum or wchar_t. */
          promoted_type = integer_type(unqual_type->variant.integer.int_kind);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */

  db_exit();
  return promoted_type;
}  /* type_after_integral_promotion */


a_type_ptr default_argument_promotion(a_type_ptr old_type)
/*
Determine what (old style) promotion should be done to this argument's
type.  Note that this routine does not actually change the type of the
node; it just returns the type that the node should be.  It is up to
the caller to do the cast.  See also arg_default_promote_operand; it depends
on the fact that the default argument promotions on an integral or enum type
are simply the integral promotions (to handle the bit-field integral
promotions case).  Note that this routine expects to receive an rvalue
type.
*/
{
  a_type_ptr new_type = old_type;
  a_type_ptr unqual_type = skip_typerefs(old_type);

  if (is_integral_or_enum(unqual_type)) {
    /* For integral types, do the integral promotions. */
    new_type = type_after_integral_promotion(old_type);
  } else if (is_real_floating(unqual_type)) {
    /* Promote float to double. */
    if (unqual_type->variant.float_kind == (a_float_kind)fk_float) {
      new_type = float_type((a_float_kind)fk_double);
    }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
  } else if (is_imaginary(unqual_type)) {
    /* Promote float imaginary to double imaginary. */
    if (unqual_type->variant.float_kind == (a_float_kind)fk_float) {
      new_type = imaginary_type((a_float_kind)fk_double);
    }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
  }  /* if */
  return new_type;
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
  a_type_ptr     complete_object_type = NULL;
  a_variable_ptr var;

  if (con_is_exact_addr_of_variable(constant, &var)) {
    /* Unmodified address of a variable.  The variable is the complete
       object and its type is the complete object type. */
    complete_object_type = var->type;
  }  /* if */
  return complete_object_type;
}  /* con_complete_object_type */


a_type_ptr node_complete_object_type(an_expr_node_ptr node,
                                     a_boolean        call_case)
/*
Return the type of the complete object that contains the location indicated
by node (an lvalue address), or NULL if no complete object can be determined.
call_case is TRUE if the answer will be used to optimize a virtual function
call.  NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.
*/
{
  a_type_ptr            complete_object_type = NULL;
  an_expr_operator_kind op;
  an_expr_node_ptr      first_operand;
  a_new_delete_supplement_ptr
                        ndsp;
  a_variable_ptr        var;

  switch (node->kind) {
    case enk_error:
    case enk_address_of_ellipsis:
    case enk_routine_address:
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
    case enk_result_of_overriding_function:
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
      /* Complete object type is not known. */
      break;
    case enk_variable:
      /* Complete object type is not known in general, but if the variable
         is the "this" parameter for a constructor or destructor, and we're
         optimizing a virtual call case, it is known. */
      var = node->variant.variable;
      if (call_case && var->source_corresp.name == NULL &&
          var->is_parameter &&
          depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        /* The variable is a parameter and we're inside a function. */
        a_scope_ptr scope =
                          scope_stack[depth_innermost_function_scope].il_scope;
        if (var == scope->variant.routine.this_param_variable) {
          /* The variable is the "this" parameter variable of the current
             function. */
          a_routine_ptr curr_routine = scope->variant.routine.ptr;
          if (curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
              curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor) {
            /* The current function is a constructor or destructor.  We know
               that the "this" parameter points to a complete object, at
               least for purposes of resolving virtual calls (ARM 12.7). */
            complete_object_type =
                          curr_routine->source_corresp.parent.class_type;
          }  /* if */
        } /* if */
      }  /* if */
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
        /* MSVC++ 5.0 doesn't do this optimization.  It allows one to
           do a placement new of a derived class type on a subobject
           and get the derived class behavior. */
        if (!microsoft_mode) {
          complete_object_type = first_operand->next->variant.field->type;
        }  /* if */
      } else if (op == (an_expr_operator_kind)eok_base_class_cast) {
        /* Cast to a base class.  Do a recursive call on the first operand
           to find the complete object. */
        complete_object_type = node_complete_object_type(first_operand,
                                                         call_case);
      } else if (op == (an_expr_operator_kind)eok_padd ||
                 op == (an_expr_operator_kind)eok_padd_subsc ||
                 op == (an_expr_operator_kind)eok_psubtract) {
        /* Pointer addition (subscripting) or subtraction.  Do a recursive
           call on the first operand to find the complete object. */
        complete_object_type = node_complete_object_type(first_operand,
                                                         call_case);
      }  /* if */
      break;
    case enk_temp_init:
      complete_object_type = node->type;
      if (node->variant.init.result_is_addr) {
        /* The result of the enk_temp_init is the address of the temporary,
           so drop the pointer-to to get the type of the temporary. */
        complete_object_type = type_pointed_to(complete_object_type);
      }  /* if */
      break;
    case enk_new_delete:
      ndsp = node->variant.new_delete;
      if (ndsp->is_new) {
        /* For new, the type is known. */
        complete_object_type = ndsp->type;
      } else {
        /* Not easy to tell the type for delete, and probably not worth it. */
      }  /* if */
      break;
    case enk_object_lifetime:
      complete_object_type =
                  node_complete_object_type(node->variant.object_lifetime.expr,
                                            call_case);
      break;
    case enk_typeid:
      /* For a typeid, the complete object type is the type of the struct
         indicated by the node type. */
      complete_object_type = type_pointed_to(node->type);
      break;
    case enk_runtime_sizeof:
      complete_object_type = node->type;
      break;
    case enk_throw:
    case enk_field:
    case enk_condition:
#if GNU_EXTENSIONS_ALLOWED
    case enk_statement:  /* Used only in C mode. */
#endif /* GNU_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str(
                             "node_complete_object_type: bad expression kind");
  }  /* switch */
  return complete_object_type;
}  /* node_complete_object_type */


a_type_ptr f_implicit_this_param_type_of(a_type_ptr  routine_type)
/*
Synthesize the type of "this" from the underlying class type and the
qualification of a member function type.
*/
{
  a_routine_type_supplement_ptr  rtsp =
                      skip_typerefs(routine_type)->variant.routine.extra_info;
  a_type_ptr                     result = rtsp->this_class;

  /* Note that the "restrict" qualifier goes on top of the pointer type (to
     denote the limited aliasing of the "this" pointer) whereas the other
     qualifiers apply to the underlying class type (to denote properties of
     the object pointed to). */
  if ((rtsp->qualifiers & ~TQ_RESTRICT) != TQ_NONE) {
    result = make_qualified_type(result, rtsp->qualifiers & ~TQ_RESTRICT);
  }  /* if */
  result = make_pointer_type(result);
  if (rtsp->qualifiers & TQ_RESTRICT) {
    result = make_qualified_type(result, TQ_RESTRICT);
  }  /* if */
  return result;
}  /* f_implicit_this_param_type_of */


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
               int_kind_is_signed[(int)type_1->variant.integer.int_kind] ==
                    int_kind_is_signed[(int)type_2->variant.integer.int_kind]);
  return same_repr;
}  /* same_repr_int_types */
#endif /* SAME_REPR_INTS_INTERCHANGEABLE_IN_IL */


static a_boolean identical_array_type_level(a_type_ptr  type_1,
                                            a_type_ptr  type_2)
/*
Return TRUE if the two array types have identical bounds.
*/
{
  a_boolean         identical = FALSE;

  check_assertion(is_array(type_1) && is_array(type_2));
  if (array_is_vla(type_1) || array_is_vla(type_2)) {
    /* One or both of the types is a variable length array. */
  } else if (type_1->variant.array.is_variable_size_array) {
    if (type_2->variant.array.is_variable_size_array) {
      /* Both arrays have variable bounds. */
      an_expr_node *node_1 = type_1->variant.array.variant.element_count_expr;
      an_expr_node *node_2 = type_2->variant.array.variant.element_count_expr;
      if (node_1->kind == (an_expr_node_kind)enk_constant &&
          node_2->kind == (an_expr_node_kind)enk_constant) {
        identical = eq_constants(node_1->variant.constant,
                                 node_2->variant.constant);
      }  /* if */
    } else {
      /* A variable-bound array and a fixed-bound array. */
    }  /* if */
  } else if (type_2->variant.array.is_variable_size_array) {
    /* A variable-bound array and a fixed-bound array. */
  } else if (type_1->variant.array.is_template_dependent_size_array) {
    if (type_2->variant.array.is_template_dependent_size_array) {
      /* Both arrays have unknown (but constant) bounds. */
      identical = eq_constants(
                        type_1->variant.array.variant.element_count_constant,
                        type_2->variant.array.variant.element_count_constant);
    } else {
      /* An unknown-bound array and a known-bound array. */
    }  /* if */
  } else if (type_2->variant.array.is_template_dependent_size_array) {
    /* A known-bound array and an unknown-bound array. */
  } else {
    /* Both arrays have fixed bounds.  Just compare the element counts. */
    identical = (type_1->variant.array.variant.number_of_elements ==
                 type_2->variant.array.variant.number_of_elements);
  }  /* if */
  return identical;
}  /* identical_array_type_level */


static a_boolean equiv_nonreal_templates(a_type_ptr	type_1,
				         a_symbol_ptr	sym_1,
					 a_type_ptr	type_2,
					 a_symbol_ptr	sym_2)
/*
Return TRUE if sym_1 and sym_2 are equivalent nonreal templates, such
as X in "T::X<int>" and "Y::X<int>".  type_1 and type_2 are nonreal
class types that are instances of the templates pointed to by sym_1 and sym_2.
*/
{
  a_boolean	result = FALSE;

  if (is_nonreal_template_symbol(sym_1) && is_nonreal_template_symbol(sym_2)) {
    /* They are both nonreal templates. */
    if (sym_1->header == sym_2->header) {
      /* They have the same names. */
      if (identical_types(type_1->source_corresp.parent.class_type,
                          type_2->source_corresp.parent.class_type)) {
        /* Their parent types are the same. */
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* equiv_nonreal_templates */


static a_boolean equiv_template_template_params(
				         a_symbol_ptr	sym_1,
					 a_symbol_ptr	sym_2)
/*
Return TRUE if sym_1 and sym_2 are both template template parameters for
equivalent templates, such as T in "T<int>" and "T<int>".
*/
{
  a_boolean	result = FALSE;

  if (is_template_template_param_symbol(sym_1) &&
      is_template_template_param_symbol(sym_2)) {
    /* They are both template template parameters.  Compare the
       underlying templates. */
    if (equiv_templates_given_supplement(sym_1->variant.template_info,
                                         sym_2->variant.template_info)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* equiv_template_template_params */


static a_boolean equiv_class_types(a_type_ptr type_1,
                                   a_type_ptr type_2,
                                   a_boolean  error_matches_anything)
/*
type_1 and type_2 are class/struct/union types.  Return TRUE if they are
equivalent types.  In general, classes, structs, and unions that aren't
the same type aren't equivalent.  The exception is with template classes
involving template parameters (i.e., nonreal template classes).  Two
nonreal template classes are identical if they are based on the same
class template and have identical template arguments.
If error_matches_anything is TRUE, consider an error type or constant in
a template argument to match anything (that's appropriate for compatibility
checking instead of equivalence checking).
*/
{
  a_boolean                     equiv = FALSE;
  a_class_symbol_supplement_ptr cssp_1, cssp_2;

  /* If the pointers are identical, the types are equivalent. */
  if (same_entities(type_1, type_2)) {
    equiv = TRUE;
  } else if (!in_front_end) {
    /* We are being called after fe_wrapup was called.  Proxy classes are
       not a consideration.  The field source_corresp.assoc_info points
       into freed memory. */
  } else {
    /* The pointers aren't the same, so the classes probably aren't
       equivalent, but do some special checking. */
    /* Go to the class symbol supplements for the types. */
    /* Watch out for types created by IL lowering, which do not have the
       assoc_info pointer. */
    if (type_1->source_corresp.assoc_info != NULL &&
        type_2->source_corresp.assoc_info != NULL) {
      cssp_1 = symbol_supplement_for_class(type_1);
      cssp_2 = symbol_supplement_for_class(type_2);
      if (cssp_1->template_param_for_proxy_class != NULL &&
          cssp_2->template_param_for_proxy_class != NULL) {
        /* Both types are proxy classes for template parameters.  See if the
           underlying parameters are the same. */
        if (identical_types(cssp_1->template_param_for_proxy_class,
                            cssp_2->template_param_for_proxy_class)) {
          equiv = TRUE;
        }  /* if */
      } else if (cssp_1->class_template != NULL &&
                 cssp_2->class_template != NULL) {
        if (cssp_1->class_template == cssp_2->class_template ||
            primary_template_of(cssp_1->class_template) ==
                                 primary_template_of(cssp_2->class_template) ||
            equiv_nonreal_templates(type_1, cssp_1->class_template,
                                    type_2, cssp_2->class_template) ||
            equiv_template_template_params(cssp_1->class_template,
                                           cssp_2->class_template)) {
          /* Both types are template classes, and they are based on the same
             class template, or equivalent nonreal templates.  Check further
             if (a) they are both nonreal template classes, or (b) error
             arguments are to be considered equivalent to anything. */
          if ((type_1->variant.class_struct_union.is_nonreal_class &&
               type_2->variant.class_struct_union.is_nonreal_class) ||
              error_matches_anything) {
            an_equiv_templ_arg_options_set    eta_options = ETA_NO_OPTIONS;
            if (error_matches_anything) {
              eta_options |= ETA_ERROR_MATCHES_ANYTHING;
            }  /* if */
            if (is_nonreal_template_symbol(cssp_1->class_template) ||
                is_nonreal_template_symbol(cssp_2->class_template)) {
              eta_options |= ETA_IS_NONREAL_MEMBER;
            }  /* if */
            if (equiv_template_arg_lists(
                             type_1->variant.class_struct_union.extra_info->
                                                            template_arg_list,
                             type_2->variant.class_struct_union.extra_info->
                                                            template_arg_list,
                             eta_options)) {
              equiv = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return equiv;
}  /* equiv_class_types */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean routine_linkages_are_compatible(a_name_linkage_kind  nlk1,
                                          a_name_linkage_kind  nlk2,
                                          a_boolean            is_impl_conv)
/*
Return TRUE if the indicated name linkages are compatible with respect to the
calling conventions they imply.  is_impl_conv is TRUE if the compatibility
check occurs in connection with an implicit type conversion.

Note: this routine (along with routine_linkages_are_identical) may have to
be customized if additional linkage kinds are added to a_name_linkage_kind
(defined in il_def.h).
*/
{
  a_boolean  compat;

  /* Normally we should have valid name linkage kinds, but in error situations
     we may have nlk_none. */
  check_assertion_str2((is_name_linkage_kind_for_rout_type(nlk1) ||
                        (total_errors > 0 &&
                         nlk1 == (a_name_linkage_kind)nlk_none)) &&
                       (is_name_linkage_kind_for_rout_type(nlk2) ||
                        (total_errors > 0 &&
                         nlk2 == (a_name_linkage_kind)nlk_none)),
                       "routine_linkages_are_compatible:",
                       "unexpected linkage for routine type");
#if !STANDALONE_UTILITY_PROGRAM
#ifdef is_custom_name_linkage_kind_for_rout_type
  if (is_custom_name_linkage_kind_for_rout_type(nlk1) ||
      is_custom_name_linkage_kind_for_rout_type(nlk2)) {
    /* Custom code may be added here to specify compatibilities involving
       implementation-defined name linkages.  By default, assume no
       compatibility if the linkages are not identical. */
    compat = (nlk1 == nlk2);
  } else
#endif /* ifdef is_custom_name_linkage_kind_for_rout_type */
  if (is_impl_conv &&
             impl_conv_between_c_and_cpp_function_ptrs_allowed) {
    /* Implicit conversion -- extern "C" and extern "C++" function types
       are treated as compatible. */
    compat = TRUE;
  } else {
    /* If c_and_cpp_function_types_are_distinct is TRUE, nlk_external and
       nlk_cplusplus_external are compatible. */
    if (c_and_cpp_function_types_are_distinct) {
      /* extern "C" and extern "C++" function pointers are incompatible, so
         be sure nlk1 and nlk2 are identical. */
      compat = (nlk1 == nlk2);
    } else {
      /* extern "C" and extern "C++" function pointers are compatible. */
      compat = TRUE;
    }  /* if */
  }  /* if */
#else /* STANDALONE_UTILITY_PROGRAM */
  /* In the context of a standalone program (where implicit conversion is not
     an issue), just assume extern "C" and extern "C++" linkages need to be
     treated as distinct. */
  compat = (nlk1 == nlk2);
#endif /* !STANDALONE_UTILITY_PROGRAM */
  return compat;
}  /* routine_linkages_are_compatible */

#if !STANDALONE_UTILITY_PROGRAM

static a_boolean routine_linkages_are_identical(a_name_linkage_kind nlk1,
                                                a_name_linkage_kind nlk2)
/*
Return TRUE if the indicated name linkages are identical with respect to the
calling conventions they imply.  In a default implementation, extern "C"
routine linkage is construed as different from extern "C++" linkage.

Note: this routine (along with routine_linkages_are_compatible) may have to
be customized if additional linkage kinds are added to a_name_linkage_kind
(defined in il_def.h).
*/
{
  check_assertion_str2(is_name_linkage_kind_for_rout_type(nlk1) &&
                       is_name_linkage_kind_for_rout_type(nlk2),
                       "routine_linkages_are_identical:",
                       "unexpected linkage for routine type");
  /* Unless c_and_cpp_function_types_are_distinct is TRUE, nlk_external and
     nlk_cplusplus_external are treated as identical. */
  return (c_and_cpp_function_types_are_distinct ? (nlk1 == nlk2) : TRUE);
}  /* routine_linkages_are_identical */


static a_boolean f_change_to_canonical_types(a_type_ptr  *type_1,
                                           a_type_ptr  *type_2,
                                           a_boolean   seek_corresp)
/*
If the given types have a canonical correspondence in another translation
unit change the pointers to point to those entries and return TRUE.
Otherwise, return FALSE.  If seek_corresp is TRUE, the two types are
expected to correspond, but the correspondence might not have been found
through the symbol table because the types are unnamed (e.g., when unnamed
class types are used in the declaration of entities with linkage in C).
In that case, type_1 will have its correspondence set to type_2.
*/
{
  a_boolean   changed = FALSE;
  a_type_ptr  new_type_1 = *type_1, new_type_2 = *type_2;
  a_boolean   is_class_1 = is_immediate_class_type(new_type_1),
              is_class_2 = is_immediate_class_type(new_type_2),
              is_enum_1 = is_immediate_enum_type(new_type_1),
              is_enum_2 = is_immediate_enum_type(new_type_2);

  /* For unnamed types (classes and enums), the correspondence matching is
     driving by type comparisons.  If we are comparing two unnamed types of
     the same kind, seek if perhaps the types do correspond to each other. */
  if (seek_corresp &&
      ((is_class_1 && is_class_2 &&
        (!has_name(new_type_1) ||
         new_type_1->variant.class_struct_union.originally_unnamed) &&
        (!has_name(new_type_2) ||
         new_type_2->variant.class_struct_union.originally_unnamed)) ||
       (is_enum_1 && is_enum_2 &&
        (!has_name(new_type_1) ||
         new_type_1->variant.integer.originally_unnamed) &&
        (!has_name(new_type_2) ||
         new_type_2->variant.integer.originally_unnamed)))) {
    (void)seek_type_corresp(new_type_1, new_type_2);
  }  /* if */
  /* Convert each type to its canonical entry if applicable. */
  if (is_immediate_class_type(new_type_1) ||
      is_immediate_enum_type(new_type_1) ||
      (new_type_1->kind == (a_type_kind)tk_typeref &&
       typeref_is_typedef(new_type_1))) {
    new_type_1 = canonical_type_entry_of(new_type_1);
    if (new_type_1 != *type_1) {
      *type_1 = new_type_1;
      changed = TRUE;
    }  /* if */
  }  /* if */
  if (is_immediate_class_type(new_type_2) ||
      is_immediate_enum_type(new_type_2) ||
      (new_type_2->kind == (a_type_kind)tk_typeref &&
       typeref_is_typedef(new_type_2))) {
    new_type_2 = canonical_type_entry_of(new_type_2);
    if (new_type_2 != *type_2) {
      *type_2 = new_type_2;
      changed = TRUE;
    }  /* if */
  }  /* if */
  return changed;
}  /* f_change_to_canonical_types */

#define change_to_canonical_types(type_1, type_2, seek_corresp)          \
  (in_front_end && secondary_translation_unit_seen() &&                  \
   f_change_to_canonical_types(type_1, type_2, seek_corresp))


a_boolean f_identical_types(a_type_ptr      type_1,
                            a_type_ptr      type_2,
                            an_itf_flag_set flags)
/*
Return TRUE if the two types are identical.  This includes separate copies
of identical types, as well as the case where the pointers point to the
same type.  flags is a set of options that control the way in which certain
type comparisons are done.  See the definition of the ITF flags in types.h
for more information.
*/
{
  register a_boolean            identical = FALSE;
  a_param_type_ptr              list1, list2;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_symbol_ptr                  sym_1, sym_2;

  db_enter(5, "f_identical_types");

  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls.  Do not use the same_entities
     macro: for this routine a slightly more thorough check is desirable
     (so it can be called from the correspondence checking code). */
  if (type_1 == type_2) {
    identical = TRUE;
  } else if (!type_qualifiers_match(type_1, type_2)) {
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
    } else if (!equiv_type_kinds(type_1->kind, type_2->kind)) {
      /* The top level kinds are different, so the types are different. */
      /* identical = FALSE;  -- Already set. */
    } else if (change_to_canonical_types(&type_1, &type_2,
                                         (flags & ITF_SEEK_CORRESP) != 0)) {
      /* The types might have come from different translation units: restart
         the comparison with the canonical entries instead. */
      identical = f_identical_types(type_1, type_2, flags);
    } else {
      /* The top level kinds are the same, check further. */
      a_boolean  il_identical = (flags & ITF_IL_IDENTICAL) != 0;
      a_boolean  unknown_this_class_type =
                                   (flags & ITF_UNKNOWN_THIS_CLASS_TYPE) != 0;
      /* Reset the unknown implicit this type flag so that it won't be passed
         to recursive calls of this routine. */
      flags &= ~ITF_UNKNOWN_THIS_CLASS_TYPE;
      switch (type_1->kind) {
        case tk_error:
        case tk_unknown:
        case tk_void:
          /* No further check needed.  The types are identical. */
          identical = TRUE;
          break;
        case tk_integer:
          /* Requiring equality for enum types forces an explicit
             type change between enumeration types and integers or
             other enumeration types.  It also makes explicit casts
             useful in suppressing warnings on type changes between
             integral types and enumerated types. */
          if (!type_1->variant.integer.enum_type &&
              !type_2->variant.integer.enum_type) {
            if (type_1->variant.integer.int_kind ==
                                            type_2->variant.integer.int_kind &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                type_1->variant.integer.microsoft_sized_int_type ==
                            type_2->variant.integer.microsoft_sized_int_type &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                type_1->variant.integer.wchar_t_type ==
                                        type_2->variant.integer.wchar_t_type &&
                type_1->variant.integer.bool_type ==
                                        type_2->variant.integer.bool_type) {
              identical = TRUE;
#if SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
            } else if (il_identical && same_repr_int_types(type_1, type_2)) {
              /* Integers with the same representation are considered to be
                 identical in the IL if the FE is configured that way. */
              identical = TRUE;
#endif /* SAME_REPR_INTS_INTERCHANGEABLE_IN_IL */
            }  /* if */
          } else if ((flags & ITF_SEEK_CORRESP) != 0 &&
                     type_1->variant.integer.enum_type &&
                     type_2->variant.integer.enum_type) {
            /* The types are expected to be identical, but because they are
               presumably defined in two different translation units, the
               correspondence of their inner structure must be checked. */
            identical = seek_type_corresp(type_1, type_2);
          }  /* if */
          break;
        case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_complex:
        case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          identical = (type_1->variant.float_kind ==
                       type_2->variant.float_kind);
          break;
        case tk_pointer:
          /* For pointers and references, they must point to identical types.
             To be IL identical, they need not be both pointers or both
             references. */
          if (il_identical ||
              type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference) {
            identical = f_identical_types(type_1->variant.pointer.type,
                                          type_2->variant.pointer.type,
                                          flags)
#ifdef pointer_types_have_same_repr
                        && pointer_types_have_same_repr(type_1, type_2)
#endif /* ifdef pointer_types_have_same_repr */
                                                                       ;
          }  /* if */
          break;
        case tk_array:
          /* For arrays, the sizes must be the same and the element types
             must be identical. */
          if (f_identical_types(type_1->variant.array.element_type,
                                type_2->variant.array.element_type,
                                flags) &&
              identical_array_type_level(type_1, type_2)) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_class:
        case tk_struct:
        case tk_union:
          /* In general, classes, structs, and unions that aren't the same
             type aren't identical.  There are some exceptions with template
             classes.  Check for those. */
          if (C_mode()) {
            if ((flags & ITF_SEEK_CORRESP) != 0) {
              /* The types are expected to be identical, but because they are
                 presumably defined in two different translation units, the
                 correspondence of their inner structure must be checked. */
              identical = seek_type_corresp(type_1, type_2);
            }  /* if */
          } else if (equiv_class_types(type_1, type_2,
                                       /*error_matches_anything=*/FALSE)) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_routine:
          {
            a_boolean	this_class_matches = FALSE;
            a_type_ptr	this1;
            a_type_ptr	this2;
            rtsp1 = type_1->variant.routine.extra_info;
            rtsp2 = type_2->variant.routine.extra_info;
            this1 = rtsp1->this_class;
            this2 = rtsp2->this_class;
            if (this1 == NULL && this2 == NULL) {
              /* Both this parameter types are NULL -- they match. */
              this_class_matches = TRUE;
            } else if (this1 == NULL || this2 == NULL) {
              /* One, but not both, of the this parameter types are NULL.
                 This is considered a match if the flag is set that 
                 indicates that we don't yet know whether the type has
                 an implicit this parameter type and if the non-NULL type
                 has no qualifiers. */
              this_class_matches = unknown_this_class_type &&
                                      rtsp1->qualifiers == TQ_NONE &&
                                      rtsp2->qualifiers == TQ_NONE;
            } else {
              /* Both types are non-null, see if they are identical. */
              this_class_matches =
                        rtsp1->qualifiers == rtsp2->qualifiers &&
                        identical_types(this1, this2);
            }  /* if */
            /* For functions, the return types must be identical, the
               parameter lists must be identical, and the implicit "this"
               parameter type (if any) must be identical. */
            if (this_class_matches &&
                f_identical_types(type_1->variant.routine.return_type,
                                  type_2->variant.routine.return_type,
                                  flags) &&
                rtsp1->prototyped == rtsp2->prototyped &&
                rtsp1->has_ellipsis == rtsp2->has_ellipsis &&
                routine_linkages_are_identical(
                          (a_name_linkage_kind)rtsp1->routine_name_linkage,
                          (a_name_linkage_kind)rtsp2->routine_name_linkage)) {
              /* So far they are identical, this flag will be reset if the
                 parameter types don't match. */
              identical = TRUE;
              /* Compare the types of the parameters on the two lists. */
              for (list1 = rtsp1->param_type_list,
                                                list2 = rtsp2->param_type_list;
                   list1 != NULL && list2 != NULL;
                   list1 = list1->next, list2 = list2->next) {
                if (!f_identical_types(list1->type, list2->type,
                                       flags)) {
                  /* The parameter types are not identical. */
                  identical = FALSE;
                  break;
                }  /* if */
              }  /* for */
              if (identical) {
                /* The parameter lists are identical if they both ended
                   together. */
                identical = (list1 == NULL && list2 == NULL);
              }  /* if */
            }  /* if */
          }
          break;
        case tk_ptr_to_member:
          /* Pointer-to-member types are identical if they refer to the same
             class type and to the same member type. */
          identical = (f_identical_types(pm_class_type(type_1),
                                         pm_class_type(type_2),
                                         flags) &&
                       f_identical_types(pm_member_type(type_1),
                                         pm_member_type(type_2),
                                         flags));
          break;
        case tk_template_param:
          if (type_1->variant.template_param.kind ==
                                    type_2->variant.template_param.kind) {
            a_template_param_type_supplement_ptr	tptsp_1;
            a_template_param_type_supplement_ptr	tptsp_2;
            switch (type_1->variant.template_param.kind) {
              case tptk_param:
                 /* Template parameter types are considered to be identical
                    if their positions in the template parameter list are
                    the same, and they are associated with template
                    declarations of the same nesting level. */
                tptsp_1 = type_1->variant.template_param.extra_info;
                tptsp_2 = type_2->variant.template_param.extra_info;
                identical = (tptsp_1->coordinates.position ==
                             tptsp_2->coordinates.position) &&
                          (equiv_nesting_depths(tptsp_1->coordinates.depth,
                                                tptsp_2->coordinates.depth) ||
                           (flags & ITF_IGNORE_NESTING_DEPTH) != 0);
                break;
              case tptk_member:
                /* Members types are the same if their names are the same
                   and if they are members of identical types. */
                check_assertion(in_front_end);
                sym_1 = (a_symbol_ptr)type_1->source_corresp.assoc_info;
                sym_2 = (a_symbol_ptr)type_2->source_corresp.assoc_info;
                check_assertion(sym_1 != NULL && sym_2 != NULL);
                if (sym_1->header == sym_2->header) {
                  /* The names are the same. */
                  identical = (identical_types(type_1->source_corresp.
                                                          parent.class_type,
                                               type_2->source_corresp.
                                                          parent.class_type));
                }  /* if */
                break;
              case tptk_unknown:
                /* Two unknown types.  This should only occur when comparing
                   the unknown types of two different translation units.
                   Consider them to be the same. */
                identical = TRUE;
                break;
              default:
                unexpected_condition_str
                             ("f_identical_types: bad templ param type kind");
            }  /* switch */
          }  /* if */
          break;
#if CHECKING
        default:
          internal_error("f_identical_types: bad type");
#endif /* CHECKING */
      }  /* switch */
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_mode && identical &&
          !same_type_attributes(type_1, type_2)) {
        /* The types have different attributes, so the types are different. */
        identical = FALSE;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "f_identical_types: %s\n", identical ? "TRUE" : "FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(identical);
}  /* f_identical_types */


a_boolean param_types_are_compatible(a_type_ptr              rout_type_1,
                                     a_type_ptr              rout_type_2,
                                     a_type_compat_flags_set flags)
/*
rout_type_1 and rout_type_2 point to routine type entries.  Return TRUE if the
parameter lists are compatible.  The "this" parameter types (if any) are
not compared.  flags is a set of bit flags that modify the comparison.
*/
{
  a_param_type_ptr              list1, list2;
  a_boolean                     compatible;
  a_boolean                     list1_prototyped, list2_prototyped;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_type_ptr                    param_1_type, param_2_type;

  rout_type_1 = skip_typerefs(rout_type_1);
  rout_type_2 = skip_typerefs(rout_type_2);
  rtsp1 = rout_type_1->variant.routine.extra_info;
  rtsp2 = rout_type_2->variant.routine.extra_info;
  list1 = rtsp1->param_type_list;
  list2 = rtsp2->param_type_list;
  list1_prototyped = rtsp1->prototyped;
  list2_prototyped = rtsp2->prototyped;
  if (rtsp1->has_ellipsis != rtsp2->has_ellipsis) {
    /* One has a variable length parameter list and the other does not, so
       they cannot be compatible. */
    compatible = FALSE;
  } else if (C_mode() && !list1_prototyped && !list2_prototyped) {
     /* Both parameter lists are old-style -- in C mode they are compatible. */
    compatible = TRUE;
  } else {
    /* If either function has a new-style parameter list, the individual
       parameter types must be compatible.  See the C standard, 3.5.4.3. */
    /* This doesn't ordinarily come up in C++ mode, but it can come up when
       comparing lowered types, e.g., in the il_to_str routines. */
    if (!list2_prototyped) {
      /* List2 is an old-style param list -- it must be that list1 is
         prototyped. */
      if (!rtsp2->old_style_params_scanned) {
        /* We are comparing a prototyped parameter list (list1) with an
           old-style list, but there is no parameter information as yet for
           the second one.  The prototyped parameter list from the first
           type is used, but each type will be compared with a promoted
           version of itself. */
        list2 = list1;
      }  /* if */
    } else if (!list1_prototyped) {
      /* It is list1 that is the old-style param list and list2 is
         prototyped. Reverse them, since the processing that follows
         assumes that the old-style list, if there is one, is the second. */
      list1 = list2;
      list1_prototyped = TRUE;
      if (!rtsp1->old_style_params_scanned) {
        /* Leave list2 unchanged (i.e., the same as what list1 now is), since
           the param type comparison will be of the unpromoted types on list1
           and the promoted versions of the same types, from list2. */
      } else {
        /* Ordinary case -- the two lists are swapped. */
        list2 = rtsp1->param_type_list;
      }  /* if */
      list2_prototyped = FALSE;
    }  /* if */
    /* Compare the types of the parameters on the two lists. */
    for (; list1 != NULL && list2 != NULL;
         list1 = list1->next, list2 = list2->next) {
      if (flags & TCF_DONT_IGNORE_PARAM_TYPE_QUALIFIERS) {
        /* Even though the top-level qualifiers have been removed from the
           parameter types, report an incompatibility if they differ. */
        /* This is an issue in Microsoft bugs mode: the type qualifiers are
           stripped from the parameter types as far as overloading is
           concerned, yet they still affect virtual function overriding. */
        if (list1->qualifiers != list2->qualifiers) {
          compatible = FALSE;
          goto done;
        }  /* if */
      }  /* if */
      /* Compare the parameter types, with the second parameter type
         type promoted appropriately if it is old-style. */
      param_1_type = list1->type;
      param_2_type = list2->type;
      if (C_mode()) {
         /* In C mode, the type qualifiers (if any) on the parameter types
            are ignored (ANSI C standard, 3.5.4.3).  Also when dealing with
            an old-style function, because it's like C mode, and -- especially
            -- because default_argument_promotion drops type qualifiers. */
        param_1_type = skip_typerefs(param_1_type);
        param_2_type = skip_typerefs(param_2_type);
        if (!list2_prototyped && !(flags & TCF_NO_DEFAULT_ARG_PROMOTIONS)) {
          param_2_type = default_argument_promotion(param_2_type);
        }  /* if */
      }  /* if */
      if (f_types_are_compatible(param_1_type, param_2_type, flags)) {
        /* The parameter types are compatible. */
#if PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED
      } else if (!strict_ansi_mode &&
                 is_integral_or_enum_type(param_1_type) &&
                 f_types_are_compatible(param_1_type, list2->type, flags)) {
        /* As an extension, allow a case like
             void f(char);
             void f(c) char c; {}
           if we know that in this implementation integer arguments
           to prototyped functions are passed like integer arguments
           to unprototyped functions, i.e., they are widened, perhaps
           because they are passed in a register. */
#endif /* PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED */
      } else {
        /* The parameter types are not compatible. */
        compatible = FALSE;
        goto done;
      }  /* if */
    }  /* for */
    /* The parameter lists are compatible if they both ended together. */
    compatible = (list1 == NULL && list2 == NULL);
done:;
  }  /* if */
  return compatible;  
}  /* param_types_are_compatible */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean calling_conventions_are_compatible(a_type_ptr type1,
                                             a_type_ptr type2)
/*
Return TRUE if the calling conventions of the two given function types
are compatible.  That means they are identical or one is cc_default and
the other matches default_calling_convention.
*/
{
  a_calling_convention          cc1, cc2;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_boolean                     compatible = FALSE;

  type1 = skip_typerefs(type1);
  rtsp1 = type1->variant.routine.extra_info;
  cc1 = rtsp1->calling_convention;
  type2 = skip_typerefs(type2);
  rtsp2 = type2->variant.routine.extra_info;
  cc2 = rtsp2->calling_convention;
  if (cc1 == cc2 ||
      (cc1 == (a_calling_convention)cc_default &&
       cc2 == default_calling_convention) ||
      (cc2 == (a_calling_convention)cc_default &&
       cc1 == default_calling_convention)) {
    compatible = TRUE;
  }  /* if */
  return compatible;
}  /* calling_conventions_are_compatible */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean f_types_are_compatible(a_type_ptr              type_1,
                                 a_type_ptr              type_2,
                                 a_type_compat_flags_set flags)
/*
Compare two types for compatibility.  In C, that means the types are the
same or almost the same; see section 3.1.2.6 in the ANSI C standard.
flags is a set of bits indicating options, e.g., is an error type
considered compatible with any other type.  This routine always checks
for compatibility of type-qualifiers.  This routine should generally not
be called directly; it's meant to be called by the macros
types_are_compatible, types_are_strictly_compatible, and
types_are_compatible_ignoring_qualifiers, which do an initial test
for exact pointer equality.
*/
{
  register a_boolean            compat = FALSE;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_boolean                     ignore_type_qualifiers = FALSE;
  a_boolean                     ignore_calling_conventions = FALSE;
  a_boolean                     error_matches_anything;
  a_boolean                     is_impl_conv;
  a_boolean                     top_level_for_redeclaration = FALSE;

  db_enter(5, "f_types_are_compatible");

  error_matches_anything = 
                 (flags & TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) != 0;
  ignore_type_qualifiers = flags & TCF_IGNORE_TYPE_QUALIFIERS;
  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls.  Do not use the same_entities
     macro: for this routine a slightly more thorough check is desirable
     (so it can be called from the correspondence checking code). */
  if (type_1 == type_2) {
    compat = TRUE;
  } else {
    /* Test for a qualifier mismatch. */
    a_boolean qualifier_mismatch = FALSE;
    if (!ignore_type_qualifiers &&
        !type_qualifiers_match(type_1, type_2)) {
      qualifier_mismatch = TRUE;
    }  /* if */
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (error_matches_anything && (is_error(type_1) || is_error(type_2))) {
      /* An error type is compatible with anything under the right setting
         of the input flags. */
      compat = TRUE;
    } else if (qualifier_mismatch) {
      /* The type qualifiers do not match, so the types are not compatible. */
      /* compat = FALSE;  -- Already set. */
    } else if (type_1 == type_2) {
      /* If the types are now the same, they are compatible. */
      compat = TRUE;
    } else if (!equiv_type_kinds(type_1->kind, type_2->kind)) {
      /* The top level kinds are different, so the types are different. */
      /* compat = FALSE;  -- Already set. */
    } else if (change_to_canonical_types(&type_1, &type_2,
                                         (flags & TCF_SEEK_CORRESP) != 0)) {
      /* The types might have come from different translation units: restart
         the comparison with the canonical entries instead. */
      compat = f_types_are_compatible(type_1, type_2, flags);
    } else {
      /* The top level kinds are the same, check further. */
      is_impl_conv = (flags & TCF_IMPLICIT_CONVERSION) != 0;
      /* The TCF_IGNORE_TYPE_QUALIFIERS flag does not get passed down in
         general, so if it's present remove it from the flags. */
      if (ignore_type_qualifiers) {
        flags &= ~TCF_IGNORE_TYPE_QUALIFIERS;
      }  /* if */
      /* Ditto for TCF_REDECLARATION -- it's only set for the top-level types
         of a redeclaration.  Save it off to the side. */
      if (flags & TCF_REDECLARATION) {
        top_level_for_redeclaration = TRUE;
        flags &= ~TCF_REDECLARATION;
      }  /* if */
      /* Ditto for TCF_IGNORE_CALLING_CONVENTIONS. */
      if (flags & TCF_IGNORE_CALLING_CONVENTIONS) {
        ignore_calling_conventions = TRUE;
        flags &= ~TCF_IGNORE_CALLING_CONVENTIONS;
      }  /* if */
      switch (type_1->kind) {
        case tk_error:
          /* Error types are not compatible by the test above, so they are not
             compatible here. */
          compat = FALSE;
          break;
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
                                           type_2->variant.integer.int_kind &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                type_1->variant.integer.microsoft_sized_int_type ==
                            type_2->variant.integer.microsoft_sized_int_type &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                type_1->variant.integer.wchar_t_type ==
                                        type_2->variant.integer.wchar_t_type &&
                type_1->variant.integer.bool_type ==
                                        type_2->variant.integer.bool_type) {
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
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_complex:
        case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          compat = (type_1->variant.float_kind == type_2->variant.float_kind);
          break;
        case tk_pointer:
          /* For pointers and references, they must be both pointers or both
             references and must point to compatible types. */
          if (type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference) {
            compat = f_types_are_compatible(type_1->variant.pointer.type,
                                            type_2->variant.pointer.type,
                                            flags)
#ifdef pointer_types_have_same_repr
                     && pointer_types_have_same_repr(type_1, type_2)
#endif /* ifdef pointer_types_have_same_repr */
                                                                    ;
          }  /* if */
          break;
        case tk_array:
          /* For arrays, if both have sizes the sizes must be the same.  The
             element types must be compatible. */
          { a_type_compat_flags_set sub_flags = flags;
            /* In C++ mode, if ignoring first-level qualifiers, we should
               ignore qualifiers on the element type, since such a qualifier
               counts as a first-level qualifier. */
            if (ignore_type_qualifiers && !C_mode()) {
              sub_flags |= TCF_IGNORE_TYPE_QUALIFIERS;
            }  /* if */
            /* Check that the element types are compatible. */
            if (f_types_are_compatible(type_1->variant.array.element_type,
                                       type_2->variant.array.element_type,
                                       sub_flags)) {
              /* Check that the bounds match. */
              if (identical_array_type_level(type_1, type_2)) {
                compat = TRUE;
              } else if (array_is_vla(type_1) || array_is_vla(type_2)) {
                /* One or the other is a VLA, which is compatible with any
                   array of the same element type. */
                compat = TRUE;
              } else if (C_mode() || top_level_for_redeclaration) {
                /* Check whether one of the arrays has unknown bounds.  Note
                   that in C++ this produces "compatibility" only for top-level
                   redeclarations: "extern int a[]" and "int a[3]" are
                   "compatible" (WP 3.5), but not "extern int (*p)[]" and
                   "int (*p)[3]" (WP 3.8). */
                if ((!has_unknown_specified_bound(type_1) &&
                     type_1->variant.array.variant.number_of_elements == 0) ||
                    (!has_unknown_specified_bound(type_2) &&
                     type_2->variant.array.variant.number_of_elements == 0)) {
                  /* One or the other is an unknown-bound array type, which is
                     compatible with any known-bound array of the same element
                     type. */
                  compat = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          }
          break;
        case tk_class:
        case tk_struct:
        case tk_union:
          /* In general, classes, structs, and unions that aren't the same
             type aren't compatible.  There are some exceptions with template
             classes.  Check for those. */
          if (!C_mode() &&
              equiv_class_types(type_1, type_2, error_matches_anything)) {
            compat = TRUE;
          }  /* if */
          break;
        case tk_routine:
          /* For functions, the return types must be compatible, the parameter
             types must be compatible, and the "this" parameter types (if any)
             must be compatible. */
          rtsp1 = type_1->variant.routine.extra_info;
          rtsp2 = type_2->variant.routine.extra_info;
          if (f_types_are_compatible(type_1->variant.routine.return_type,
                                     type_2->variant.routine.return_type,
                                     flags) &&
              param_types_are_compatible(type_1, type_2, flags) &&
              ((flags & TCF_IGNORE_THIS_CLASS_TYPE) ||
               (rtsp1->qualifiers == rtsp2->qualifiers &&
                ((rtsp1->this_class == NULL) ?
                    (rtsp2->this_class == NULL) :
                    (rtsp2->this_class != NULL &&
                     f_types_are_compatible(rtsp1->this_class,
                                            rtsp2->this_class, flags))))) &&
              (ignore_calling_conventions ||
               (routine_linkages_are_compatible(
                             (a_name_linkage_kind)rtsp1->routine_name_linkage,
                             (a_name_linkage_kind)rtsp2->routine_name_linkage,
                             is_impl_conv)
#if MICROSOFT_EXTENSIONS_ALLOWED
                && (!microsoft_mode ||
                    calling_conventions_are_compatible(type_1, type_2))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                       ))) {
            compat = TRUE;
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* Pointer-to-member types are compatible if they refer to the same
             class type and their member types are compatible. */
          if (flags & TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE) {
            flags |= TCF_IGNORE_THIS_CLASS_TYPE;
          }  /* if */
          if (f_types_are_compatible(pm_member_type(type_1),
                                     pm_member_type(type_2), flags)) {
            if ((flags & TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE) ||
                f_types_are_compatible(pm_class_type(type_1),
                                       pm_class_type(type_2), flags)) {
              compat = TRUE;
            }  /* if */
          }  /* if */
          break;
        case tk_template_param:
          /* Template parameter types are considered to be compatible if
             their positions in the template parameter list are the same. */
          compat = f_identical_types(type_1, type_2, ITF_NO_FLAGS);
          break;
#if CHECKING
        default:
          internal_error("f_types_are_compatible: bad type");
#endif /* CHECKING */
      }  /* switch */
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_mode && compat &&
          !same_type_attributes(type_1, type_2)) {
        /* The types have different attributes, so the types are different. */
        if (error_matches_anything &&
            (is_or_contains_error_type(type_1) ||
             is_or_contains_error_type(type_2))) {
          /* If an error type match is involved, ignore an attribute
             difference (specifically, an alignment difference). */
        } else {
          compat = FALSE;
        }  /* if */
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "f_types_are_compatible: %s\n", compat ? "TRUE":"FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(compat);
}  /* f_types_are_compatible */


static an_integer_kind canonical_integer_kind_of(a_type_ptr type)
/*
For the integral or enum type given return the canonical (signedness-free)
integer kind (e.g., "unsigned int" and "int" both yield ik_int).
*/
{
  an_integer_kind ikind;

  check_assertion(is_integral_or_enum(type));
  ikind = type->variant.integer.int_kind;
  if (ikind == (an_integer_kind)ik_signed_char ||
      ikind == (an_integer_kind)ik_unsigned_char) {
    ikind = (an_integer_kind)ik_char;
  } else if (ikind == (an_integer_kind)ik_unsigned_short) {
    ikind = (an_integer_kind)ik_short;
  } else if (ikind == (an_integer_kind)ik_unsigned_int) {
    ikind = (an_integer_kind)ik_int;
  } else if (ikind == (an_integer_kind)ik_unsigned_long) {
    ikind = (an_integer_kind)ik_long;
#if LONG_LONG_ALLOWED
  } else if (ikind == (an_integer_kind)ik_unsigned_long_long) {
    ikind = (an_integer_kind)ik_long_long;
#endif /* LONG_LONG_ALLOWED */
  }  /* if */
  return ikind;
}  /* canonical_integer_kind_of */


a_boolean integral_types_the_same_except_for_signedness(a_type_ptr type_1,
                                                        a_type_ptr type_2)
/*
Return TRUE if the integral types type_1 and type_2 are the same type
except for signedness.
*/
{
  return (canonical_integer_kind_of(type_1) ==
          canonical_integer_kind_of(type_2));
}  /* integral_types_the_same_except_for_signedness */


a_boolean interchangeable_types(a_type_ptr type_1,
                                a_type_ptr type_2)
/*
Return TRUE if the given types are "interchangeable".  This is a concept
implied by the footnote in 3.1.2.5 in the C standard: "The same representation
and alignment requirements are meant to imply interchangeability as
arguments to functions, return values, and members of unions".  
Interchangeability is a less strict matching than compatibility:
compatible types are interchangeable, but interchangeable types need
not be compatible.  This routine is called to check printf arguments,
arguments of old-style calls, and to allow some otherwise questionable
pointer compatibility (i.e., pointers to interchangeable types are
considered compatible, with a warning).  Note that the caller of this
routine must either (a) issue a warning if something is made legal by
that fact that the types are interchangeable, or (b) be checking something
that is not required to be checked by the ANSI C standard.
*/
{
  a_boolean       interch = FALSE;
  a_type_ptr      ptr_type_1, ptr_type_2;

  db_enter(5, "interchangeable_types");
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
            and int * should be interchangeable), and is allowed as an
            extension.
  */
  /* Drop type qualifiers and other typerefs.  This takes care of point (2)
     above. */
  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (types_are_compatible(type_1, type_2)) {
    /* Compatible types are interchangeable. */
    interch = TRUE;
  } else if (!equiv_type_kinds(type_1->kind, type_2->kind)) {
    /* The kinds are different, so the types are not interchangeable. */
    /* interch = FALSE;  -- already set. */
#if GNU_EXTENSIONS_ALLOWED
  } else if (type_1->alignment != type_2->alignment) {
    /* The types have different alignments (perhaps because of the gcc aligned
       attribute), so the types are not interchangeable. */
    /* interch = FALSE;  -- Already set. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (type_1->kind == (a_type_kind)tk_integer) {
    /* Look for two integral types that differ only in signedness, or
       two character types. */
    if (strict_ansi_mode) {
      /* In strict ANSI mode (C or C++), two integral types are interchangeable
         if they're the same type with signedness ignored. */
      if (integral_types_the_same_except_for_signedness(type_1, type_2)) {
        interch = TRUE;
      }  /* if */
    } else {
      /* When not in strict mode, consider any integral types that
         have the same size and alignment to be interchangeable.  Typically,
         this makes int and long interchangeable. */
      if (type_1->size == type_2->size &&
          type_1->alignment == type_2->alignment) interch = TRUE;
    }  /* if */
  } else if (type_1->kind == (a_type_kind)tk_pointer &&
             !type_1->variant.pointer.is_reference &&
             !type_2->variant.pointer.is_reference
#ifdef pointer_types_have_same_repr
             && pointer_types_have_same_repr(type_1, type_2)
#endif /* ifdef pointer_types_have_same_repr */
                                                            ) {
    /* Pointer types.  Get the underlying types. */
    ptr_type_1 = skip_typerefs(type_1->variant.pointer.type);
    ptr_type_2 = skip_typerefs(type_2->variant.pointer.type);
    if (same_entities(ptr_type_1, ptr_type_2) ||  /* This test for speed. */
        (strict_ansi_mode ? types_are_compatible(ptr_type_1, ptr_type_2) :
                            interchangeable_types(ptr_type_1, ptr_type_2))) {
      /* Pointers to compatible types are compatible.  As an extension,
         pointers to interchangeable types are interchangeable. */
      interch = TRUE;
    } else if ((is_void(ptr_type_1) && is_character(ptr_type_2)) ||
               (is_character(ptr_type_1) && is_void(ptr_type_2))) {
      /* void * and char * are interchangeable. */
      interch = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode && C_mode() &&
               !is_pointer(ptr_type_1) && !is_pointer(ptr_type_2)) {
      /* In Microsoft C mode, allow things like "int **" <--> "float **"; the
         pointer levels have to match up. */
      interch = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  db_exit();
  return interch;
}  /* interchangeable_types */


void clear_std_conv_descr(a_std_conv_descr_ptr std_conv)
/*
Clear a standard conversion description to default values.
*/
{
  std_conv->cast_base_class = NULL;
  std_conv->reversed_cast = FALSE;
  std_conv->type_qualifiers_added = FALSE;
  std_conv->pointer_normalization_needed = FALSE;
  std_conv->nontrivial_conversion = FALSE;
  std_conv->promotion = FALSE;
  std_conv->ptr_or_pm_to_bool = FALSE;
  std_conv->exception_spec_incompatibility = FALSE;
  std_conv->conv_of_string_literal_to_ptr_to_nonconst = FALSE;
  std_conv->warning_suggested = ec_no_error;
}  /* clear_std_conv_descr */


static a_boolean dest_of_ptr_cast_big_enough(a_type_ptr source_type,
                                             a_type_ptr dest_type)
/*
Return TRUE if a value of type "source_type" will fit in an entity of
type "dest_type".  This is used in testing whether or not non-portable
casts involving pointers should be allowed.
*/
{
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  return (dest_type->size >= source_type->size);
}  /* dest_of_ptr_cast_big_enough */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_address_of_string_constant(a_constant *constant)
/*
Return TRUE if the given constant is the address of a string constant
or wide string constant.
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

#if !STANDALONE_UTILITY_PROGRAM

a_boolean same_type_with_added_qualifiers(a_type_ptr source_type,
					  a_type_ptr dest_type,
					  a_boolean  ignore_qualifiers,
					  a_boolean  *p_qualifiers_added)
/*
Return TRUE if source_type and dest_type are compatible types except that
dest_type may have some additional type qualifiers at some level(s).
If ignore_qualifiers is TRUE, qualifiers are ignored at all levels,
which makes this routine something like a types_are_compatible that
ignores type qualifiers.

If any qualifiers are added, the flag pointed to by p_qualifiers_added
is set to TRUE.  Otherwise it is set to FALSE.  p_qualifiers_added
can be NULL if the caller does not need this flag returned.
*/
{
  a_boolean   same;
  a_boolean   qualifiers_added = FALSE;

  for (same = TRUE; same == TRUE;) {
    a_type_qualifier_set dest_type_qualifiers;
    a_type_qualifier_set source_type_qualifiers;
    dest_type_qualifiers = get_type_qualifiers(dest_type);
    source_type_qualifiers = get_type_qualifiers(source_type);
    if (!ignore_qualifiers &&
        any_qualifier_in_set_missing(dest_type_qualifiers,
                                     source_type_qualifiers)) {
      /* Some qualifier is missing. */
      same = FALSE;
    } else {
      /* See whether the destination type has additional qualifiers. */
      if (any_qualifier_in_set_missing(source_type_qualifiers,
                                       dest_type_qualifiers)) {
        qualifiers_added = TRUE;
      }  /* if */
      dest_type = skip_typerefs(dest_type);
      source_type = skip_typerefs(source_type);
      if (is_pointer_type(dest_type) && is_pointer_type(source_type)
#ifdef pointer_types_have_same_repr
          && pointer_types_have_same_repr(dest_type, source_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                                 ) {
        /* Continue at the next level for pointers. */
        dest_type = type_pointed_to(dest_type);
        source_type = type_pointed_to(source_type);
      } else if (is_ptr_to_member_type(dest_type) &&
                 is_ptr_to_member_type(source_type) &&
                 f_types_are_compatible(
                                 pm_class_type(dest_type),
                                 pm_class_type(source_type),
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING)) {
        /* Continue at the next level for pointers to members. */
        dest_type = pm_member_type(dest_type);
        source_type = pm_member_type(source_type);
      } else if (is_array_type(dest_type) && is_array_type(source_type) &&
                 !has_unknown_specified_bound(dest_type) &&
                 !has_unknown_specified_bound(source_type) &&
                 dest_type->variant.array.variant.number_of_elements ==
                     source_type->variant.array.variant.number_of_elements) {
        /* Continue at the next level for arrays. */
        dest_type = array_element_type(dest_type);
        source_type = array_element_type(source_type);
      } else {
        /* For other types, the underlying types must be the same. */
        same = types_are_compatible(dest_type, source_type);
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  /* If there were any qualifiers added, set the flag specified by the
     caller. */
  if (p_qualifiers_added != NULL) *p_qualifiers_added = qualifiers_added;
  return same;
}  /* same_type_with_added_qualifiers */


static a_boolean type_is_catchable_by_handler_for_other_type(
                                                      a_type_ptr  type,
                                                      a_type_ptr  other_type)
/*
Return TRUE if an object of type "type" is catchable by a handler whose
handler-parameter is of type "other_type".
*/
{
  a_boolean                    match;
  a_base_class_ptr             bcp;
  a_base_class_derivation_ptr  preferred_derivation;

  /* Return TRUE if the types are identical. */
  match = identical_types(type, other_type);
  if (!match) {
    /* if type is an unambiguous and public base class of other_type,
       a handler for other_type will catch type. */
    if (is_pointer_type(type) && is_pointer_type(other_type)) {
      /* The same goes if both are pointer types. */
      type = type_pointed_to(type);
      other_type = type_pointed_to(other_type);
    }  /* if */
    type = skip_typerefs(type);
    other_type = skip_typerefs(other_type);
    if (is_immediate_class_type(type) && is_immediate_class_type(other_type)) {
      /* bcp will come back non-NULL if type is a base class of other_type. */
      bcp = find_base_class_of(other_type, type);
      /* now be sure bcp is unambiguous and public. */
      if (bcp != NULL && !bcp->ambiguous) {
        /* Be sure bcp points to a public base class of other_type. */
        preferred_derivation = preferred_derivation_of(bcp);
        if (access_to_end_of_path((an_access_specifier)as_public,
                                  preferred_derivation->path,
                                  preferred_derivation) ==
                                             (an_access_specifier)as_public) {
          match = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return match;
}  /* type_is_catchable_by_handler_for_other_type */


a_boolean exception_spec_is_less_restrictive(a_type_ptr  type1,
                                             a_type_ptr  type2)
/*
Compare the exception specifications associated with function types type1
and type2.  Return TRUE if the exception specification on the former is less
restrictive than that on the latter.  The exception specification for one
function is considered "less restrictive" than that of another if at least
one type may be thrown from the former that would violate the exception
specification of the latter (i.e., that would not be caught by handlers for
the types specified for the latter).  For example, the following are in
order from most restrictive to least restrictive:

  void f1() throw();              // Nothing will be thrown
  void f2() throw(T);
  void f3() throw(T,U);
  void f4();                      // Anything might be thrown

Moreover:

  struct D : public B { ... };
  void g1() throw(D);             // Does not violate exception spec of g2
  void g2() throw(B);             // Violates exception spec of g1

If B is a public and unambiguous base class of D, g2 is less restrictive than
g1, because a handler for B can also catch a D, but a handler for D cannot
catch a B.
*/
{
  a_boolean                            is_less_restrictive = FALSE;
  an_exception_specification_ptr       esp1, esp2;
  an_exception_specification_type_ptr  estp1, estp2;

  if (exceptions_enabled) {
    type1 = skip_typerefs(type1);
    type2 = skip_typerefs(type2);
    if (is_error(type1) || is_error(type2)) {
      /* No action -- return FALSE. */
    } else {
      check_assertion(type1->kind == (a_type_kind)tk_routine &&
                      type2->kind == (a_type_kind)tk_routine);
      esp1 = type1->variant.routine.extra_info->exception_specification;
      esp2 = type2->variant.routine.extra_info->exception_specification;
      if (esp2 == NULL
#if MICROSOFT_EXTENSIONS_ALLOWED
          || esp2->throw_any
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                            ) {
        /* The function associated with type2 can throw any exception; type1
           cannot be less restrictive than that. */
        /* is_less_restrictive = FALSE; */
      } else if (esp1 == NULL
#if MICROSOFT_EXTENSIONS_ALLOWED
                 || esp1->throw_any
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                   ) {
        /* Type1's function can can throw any exception, and type2's function
           has at least some restriction, so the former is less restrictive. */
        is_less_restrictive = TRUE;
      } else {
        /* If any type on the exception specification list of type1's function
           does not match a type on the list of type2, the former is less
           restrictive. Corollary 1: if the list of the type1's function is
           empty (i.e., if its exception specification is maximally
           restrictive), there is no way it can be less restrictive; in this
           case, the outer loop stops before it even gets started.  Corollary
           2: if there is anything on the list for type1 and the list for
           type2 is empty, type1 has to be less restrictive; in this case it
           is the inner loop that doesn't run. */
        /* The outer loop traverses the types specified for type1. */
        estp1 = esp1->exception_specification_type_list;
        for (; estp1 != NULL; estp1 = estp1->next) {
          /* Ignore entries marked "redundant" -- the type has already been
             seen on the list. */
          if (estp1->redundant) continue;
          /* The inner loop traverses the types specified for type2, looking
             for an entry that matches the current entry from type1's list. */
          estp2 = esp2->exception_specification_type_list;
          for (; estp2 != NULL; estp2 = estp2->next) {
            /* Ignore entries marked "redundant" -- the type has already been
               seen on the list. */
            if (estp2->redundant) continue;
            /* The types "match" if a handler for estp1->type can catch
               estp2->type -- e.g., if the types are identical or estp1->type
               is a public and unambiguous base class of estp2->type. */
            if (type_is_catchable_by_handler_for_other_type(estp2->type,
                                                            estp1->type)) {
              /* Match. */
              goto continue_outer_loop;
            }  /* if */
          }  /* for */
          /* Falling through to here means a match was not found. */
          is_less_restrictive = TRUE;
          break;
continue_outer_loop:;
          /* A match was found.  Move on to the next type in type1's list. */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_less_restrictive;
}  /* exception_spec_is_less_restrictive */


a_boolean same_exception_spec(a_type_ptr type_1, a_type_ptr type_2)
/*
If the given types are function types, pointer or reference to function types,
or pointer-to-member function types, return whether their exception
specifications match.  For other types, just return TRUE.
*/
{
  a_boolean  result = TRUE;

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (is_or_contains_error_type(type_1) ||
      is_or_contains_error_type(type_2)) {
    /* Something went wrong during the processing of this function type
       already; to avoid unreliable diagnostics, ignore differences in
       exception specifications. */
  } else if (is_ptr_or_ref_type(type_1)) {
    type_1 = type_pointed_to(type_1);
    type_2 = type_pointed_to(type_2);
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (is_function_type(type_1) && is_function_type(type_2)) {
      result = !(exception_spec_is_less_restrictive(type_1, type_2) ||
                 exception_spec_is_less_restrictive(type_2, type_1));
    }  /* if */
  } else if (is_ptr_to_member_type(type_1)) {
    type_1 = pm_member_type(type_1);
    type_2 = pm_member_type(type_2);
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (is_function_type(type_1) && is_function_type(type_2)) {
      result = !(exception_spec_is_less_restrictive(type_1, type_2) ||
                 exception_spec_is_less_restrictive(type_2, type_1));
    }  /* if */
  } else if (is_function_type(type_1) && is_function_type(type_2)) {
    result = !(exception_spec_is_less_restrictive(type_1, type_2) ||
               exception_spec_is_less_restrictive(type_2, type_1));
  }  /* if */
  return result;
}  /* same_exception_spec */


static a_boolean same_exception_spec_on_return_and_param_type(
                                                           a_type_ptr  type_1,
                                                           a_type_ptr  type_2)
/*
Return TRUE if two function types have identical exception specifications on
their respective parameter types and on their return type.  (This is a
requirement when pointers to such functions are initialized or assigned.)
Otherwise, return FALSE.
*/
{
  a_boolean  result = TRUE;

  if (is_error_type(type_1) || is_error_type(type_2)) {
    /* Something went wrong earlier; ignore differences in exception
       specifications. */
  } else if (!same_exception_spec(type_1->variant.routine.return_type,
                                  type_2->variant.routine.return_type)) {
    result = FALSE;
  } else {
    a_routine_type_supplement_ptr  rtsp1 = type_1->variant.routine.extra_info;
    a_routine_type_supplement_ptr  rtsp2 = type_2->variant.routine.extra_info;
    a_param_type_ptr                pt_1 = rtsp1->param_type_list;
    a_param_type_ptr                pt_2 = rtsp2->param_type_list;

    for (; pt_1 != NULL && pt_2 != NULL;
           pt_1 = pt_1->next, pt_2 = pt_2->next) {
      if (!same_exception_spec(pt_1->type, pt_2->type)) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* same_exception_spec_on_return_and_param_type */


a_boolean exception_spec_conversion_possible(a_type_ptr source_type,
                                             a_type_ptr dest_type)
/*
Return TRUE if the exception specifications of source_type and dest_type
(two function types) are such that a pointer to source_type can be converted
to a pointer to dest_type.
*/
{
  a_boolean okay = TRUE;

  if (exceptions_enabled) {
    source_type = skip_typerefs(source_type);
    dest_type = skip_typerefs(dest_type);
    if (is_function(source_type) && is_function(dest_type) &&
        (exception_spec_is_less_restrictive(source_type, dest_type) ||
         !same_exception_spec_on_return_and_param_type(source_type,
                                                       dest_type))) {
      okay = FALSE;
    }  /* if */
  }  /* if */
  return okay;
}  /* exception_spec_conversion_possible */


a_boolean qualification_conversion_possible(a_type_ptr source_type,
					    a_type_ptr dest_type,
					    a_boolean  *p_qualifiers_added,
                                            a_boolean  ignore_underlying_type)
/*
Return TRUE if source_type and dest_type are compatible types except that
dest_type may have some additional type qualifiers at some level(s).
This is used to determine whether a qualification conversion (as
described in 4.4 [conv.qual] in the Working Paper) may be applied.
This conversion is used for conversions such as T** to T const * const *.
Note that the types passed in to this routine are the types under the
first level pointers, e.g., T* and T const * const in the example given.

The conversion specified in the WP permits the conversion of

    T cv1,n * ... cv1,1 * cv1,0
to
    T cv2,n * ... cv2,1 * cv2,0

provided that:

- cv2,x contains all of the qualifiers present in cv1,x.

- if cv2,x contains additional qualifiers, all previous qualifiers
  (cv2,1 through cv2,x-1) must contain a const qualifier.

If any qualifiers are added, the flag pointed to by p_qualifiers_added
is set to TRUE.  Otherwise it is set to FALSE.  p_qualifiers_added
can be NULL if the caller does not need this flag returned.

If ignore_underlying_type is TRUE, return TRUE once we've reached the
underlying type of either source_type or dest_type.
*/
{
  a_boolean   same;
  a_boolean   previous_qualifiers_include_const = TRUE;
  a_boolean   qualifiers_added = FALSE;

  for (same = TRUE; same == TRUE;) {
    a_type_qualifier_set dest_type_qualifiers;
    a_type_qualifier_set source_type_qualifiers;
    dest_type_qualifiers = get_type_qualifiers(dest_type);
    source_type_qualifiers = get_type_qualifiers(source_type);
    if (any_qualifier_in_set_missing(dest_type_qualifiers,
				     source_type_qualifiers)) {
      /* Some qualifier is missing. */
      same = FALSE;
    } else {
      /* In standard mode, if the destination has additional qualifiers
	 not found in the source, any previous qualifiers must have
	 included const. */
      if (any_qualifier_in_set_missing(source_type_qualifiers,
				       dest_type_qualifiers)) {
	qualifiers_added = TRUE;
	same = previous_qualifiers_include_const;
	if (!same) break;
      }  /* if */
      /* See if this qualifier includes const. */
      if ((dest_type_qualifiers & TQ_CONST) == 0) {
	previous_qualifiers_include_const = FALSE;
      }  /* if */
      dest_type = skip_typerefs(dest_type);
      source_type = skip_typerefs(source_type);
      if (is_pointer_type(dest_type) && is_pointer_type(source_type)) {
	/* Continue at the next level for pointers. */
        dest_type = type_pointed_to(dest_type);
	source_type = type_pointed_to(source_type);
      } else if (is_ptr_to_member_type(dest_type) &&
		 is_ptr_to_member_type(source_type)) {
        /* Continue at the next level for pointers to members. */
	dest_type = pm_member_type(dest_type);
	source_type = pm_member_type(source_type);
      } else {
        if (ignore_underlying_type) {
          /* We've reached the underlying type of one or the other of
             the types.  In this mode, that is counted as a match. */
          same = TRUE;
        } else {
          /* For other types, the underlying types must be the same. */
          same = types_are_compatible(dest_type, source_type);
        }  /* if */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  /* If there were any qualifiers added, set the flag specified by the
     caller. */
  if (p_qualifiers_added != NULL) *p_qualifiers_added = qualifiers_added;
  return same;
}  /* qualification_conversion_possible */


a_boolean cast_removes_qualifiers(a_type_ptr	source_type,
				  a_type_ptr	dest_type)
/*
Return TRUE if a cast from source_type to dest_type is a cast
that, by the rules in the WP [expr.const.cast], casts away const.

A cast removes constness if for the first MAX(N,M) levels of pointers,
there is an implicit conversion from the first type to the second type.
Note that the types beyond MAX(N,M) are completely ignored, including
the underlying type pointed to.

	T1 ... * cvN * cv3 * cv2 * cv1 * 
	T2 ... * cvM * cv3 * cv2 * cv1 *

If the conversion does not "cast away const" by this definition, return
FALSE.
*/
{
  a_boolean	qualifiers_added;
  a_boolean	result = FALSE;
  a_boolean	check_further = TRUE;

  if (is_pointer_type(dest_type) && is_pointer_type(source_type)) {
    dest_type = type_pointed_to(dest_type);
    source_type = type_pointed_to(source_type);
  } else if (is_ptr_to_member_type(dest_type) &&
             is_ptr_to_member_type(source_type)) {
    dest_type = pm_member_type(dest_type);
    source_type = pm_member_type(source_type);
  } else if (is_reference_type(dest_type) && is_reference_type(source_type)) {
    dest_type = type_pointed_to(dest_type);
    source_type = type_pointed_to(source_type);
  } else {
    check_further = FALSE;
  }  /* if */
  if (check_further) {
    /* There must be an implicit conversion from the source_type to the
       dest_type (according to the description above), otherwise we
       are casting away constness. */
    if (!qualification_conversion_possible(source_type, dest_type,
                                           &qualifiers_added,
                                           /*ignore_underlying_type=*/TRUE)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* cast_removes_qualifiers */
				  

a_boolean impl_pointer_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_string_literal,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_boolean            suppress_extensions,
                         an_error_code        default_warning_code,
                         a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type source_type
(any type) to something of type dest_type (a pointer type).
If source_is_constant is TRUE, the source is a constant, and source_constant
points to the constant value.  (That's needed to check for conversions of a
null pointer constant to a pointer type.)  If source_is_string_literal
is TRUE, the source is a simple string literal (that's needed for the
deprecated conversion from string literal to "char *"); the flag can
be TRUE even when source_is_constant is FALSE, for an extension.  If
allow_qualifier_or_eh_mismatch is TRUE, ignore cv-qualifier and exception
specification mismatches (the two types are probably the types of the
operands of an operation).  suppress_extensions is TRUE if conversions
that are extensions should not be allowed (what constitutes an
extension depends on C_dialect, of course).  If the conversion is
possible, *std_conv is filled out to describe the conversion.  In
particular, if the conversion is suspect and should be flagged with a
warning, the warning_suggested field is set to an appropriate error
code; normally, it is set to ec_no_error.  default_warning_code will
be copied into warning_suggested when no specific message applies.

Note that any type qualifiers on the types themselves (rather than the
types pointed to) are ignored.

See 4.6 (pointer conversions) and 5.17 (assignment operators) in the ARM,
and 3.3.6 (pointer - pointer), 3.3.8 (relational operators), 3.3.9 (equality
operators), 3.3.15 (?: operator), and 3.3.16.1 (simple assignment).
*/
{
  a_boolean        okay = FALSE, conversion_from_void_star_in_C;
  a_type_ptr       dest_type_pointed_to, source_type_pointed_to;
  a_type_ptr       unqual_dest_type_pointed_to, unqual_source_type_pointed_to;
  a_base_class_ptr bcp;
  a_boolean        qualifiers_added, qualifiers_checked;

  db_enter(5, "impl_pointer_conversion");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_pointer_conversion: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  clear_std_conv_descr(std_conv);
  /* Assume a nontrivial conversion; the flag will be cleared later if in
     fact there is nothing nontrivial. */
  std_conv->nontrivial_conversion = TRUE;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
#if CHECKING
  if (!is_pointer(dest_type)) {
    internal_error("impl_pointer_conversion: dest_type is not pointer");
  }  /* if */
#endif /* CHECKING */
  /* Get the type pointed to and drop type qualifiers and typedefs. */
  dest_type_pointed_to = type_pointed_to(dest_type);
  unqual_dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
  if (is_template_param_type(source_type)) {
    /* A template parameter type might be a pointer type.
       This has to be tested before the template null pointer case because
       we don't want the pointer_normalization_needed flag set. */
    okay = TRUE;
  } else if (source_is_constant &&
             is_or_might_be_null_pointer_constant(source_constant)) {
    /* A null pointer constant may be converted to a pointer to any type.
       ANSI C 3.3.9 (equality operators); ANSI C 3.3.15 (?: operator);
       ANSI C 3.3.16.1 (assignment); ARM 4.6 (pointer conversions).
       This test is done early because a null pointer constant may be
       an integer (0) or a pointer ((void *)0). */
    okay = TRUE;
    if (C_mode() && is_pointer(source_type) &&
        types_are_compatible(source_type, dest_type)) {
      /* In C mode, one can get the case of (void *)0 --> void *.
         That's fine, but it doesn't really require a pointer normalization,
         so don't set the flag. */
    } else {
      /* Normal case. */
      std_conv->pointer_normalization_needed = TRUE;
    }  /* if */
  } else if (is_pointer(source_type)) {
    /* Pointer --> pointer. */
    qualifiers_checked = FALSE;
    /* Get the type pointed to and drop type qualifiers and typedefs. */
    source_type_pointed_to = type_pointed_to(source_type);
    unqual_source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    /* The "_for_impl_conversion" version is used to get proper handling of
       pointers to arrays with qualified element types and (in C++) to deal
       appropriately with routine linkages on function types. */
    if (types_are_compatible_for_impl_conversion(
                                            unqual_source_type_pointed_to,
                                            unqual_dest_type_pointed_to)) {
      /* The types pointed to are compatible, ignoring the type qualifiers.
         ANSI C 3.3.6 (pointer - pointer: caller will check that types are
         object types); ANSI C 3.3.8 (relational operators: caller will check
         that types are both object or both incomplete); ANSI C 3.3.9
         (equality operators); ANSI C 3.3.15 (?: operator); ANSI C 3.3.16.1
         (assignment: preservation of qualifiers is tested below). */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
      if (!allow_qualifier_or_eh_mismatch &&
          is_function(unqual_dest_type_pointed_to) &&
          !exception_spec_conversion_possible(unqual_source_type_pointed_to,
                                              unqual_dest_type_pointed_to)) {
        /* In pointer-to-function assignment and initialization, any exception
           allowed by the source type must be allowed by the destination
           type; but that's not the case here, so return a flag. */
        std_conv->exception_spec_incompatibility = TRUE;
      }  /* if */
    } else if (is_error(unqual_dest_type_pointed_to) ||
               is_error(unqual_source_type_pointed_to)) {
      /* Pointer --> pointer-to-error and pointer-to-error --> pointer are
         always allowed. */
      okay = TRUE;
    } else if (!C_mode() && is_template_dependent_context() &&
               (is_template_dependent_type(unqual_dest_type_pointed_to) ||
                is_template_dependent_type(unqual_source_type_pointed_to))) {
      /* Conversions between template-dependent types are always allowed. */
      okay = TRUE;
    } else {
      /* The types pointed to are not compatible.  See if the pointers are
         compatible anyway because one or the other is a "void *". */
      if (is_void(unqual_dest_type_pointed_to)) {
        /* Destination type is "void *" or a pointer to a qualified version
           of void. */
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
          std_conv->pointer_normalization_needed = TRUE;
        } else if (is_function(unqual_source_type_pointed_to)) {
          /* Converting a pointer to function to a pointer to void. */
          if (C_dialect == C_dialect_cplusplus) {
            /* In ARM C++, a pointer to a function may be converted to
               "void *" if the pointer will fit in a "void *".
               ARM 4.6 (pointer conversions).  This is no longer
               allowed in standard C++, but we allow it as an extension. */
            if (microsoft_mode) {
              /* Allowed without a warning in Microsoft mode. */
              okay = TRUE;
              std_conv->pointer_normalization_needed = TRUE;
            } else if (!suppress_extensions &&
                       dest_of_ptr_cast_big_enough(source_type, dest_type)) {
              okay = TRUE;
              std_conv->pointer_normalization_needed = TRUE;
              std_conv->warning_suggested = default_warning_code;
            }  /* if */
          } else {
            /* In C, such a conversion is nonstandard, but allowed as
               an extension.  No diagnostic is issued. */
            if (!suppress_extensions) {
              okay = TRUE;
              std_conv->pointer_normalization_needed = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        conversion_from_void_star_in_C =
                       (C_dialect != C_dialect_cplusplus &&
                        is_void(unqual_source_type_pointed_to));
        if (conversion_from_void_star_in_C &&
            (is_object(unqual_dest_type_pointed_to) ||
             is_incomplete(unqual_dest_type_pointed_to))) {
          /* In C but not C++, a "void *" may be converted to a pointer to an
             object or incomplete type.  ANSI C 3.3.16.1 (assignment). */
          okay = TRUE;
        } else if (conversion_from_void_star_in_C && !suppress_extensions) {
          /* As an extension in C, we also allow a "void *" to be converted to
             a function pointer; a warning is issued. */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        } else if (!suppress_extensions && source_is_constant &&
                   is_address_of_string_constant(source_constant) &&
                   is_character_type(unqual_source_type_pointed_to) &&
                   is_character_type(unqual_dest_type_pointed_to)) {
          /* Allow a character string to be converted to a pointer to any kind
             of char.  This is an extension in both C and C++. */
          okay = TRUE;
          if (strict_ansi_mode) {
            std_conv->warning_suggested = default_warning_code;
          }  /* if */
        } else if (C_dialect == C_dialect_cplusplus &&
                   is_class_or_struct(unqual_source_type_pointed_to) &&
                   is_class_or_struct(unqual_dest_type_pointed_to) &&
                   (bcp = find_base_class_of(unqual_source_type_pointed_to,
                                             unqual_dest_type_pointed_to))
                                                                     != NULL) {
          /* In C++, a pointer to a class may be implicitly converted to a
             pointer to an accessible base class of that class provided the
             conversion is unambiguous (ARM 4.6).  We leave the ambiguity
             and accessibility check to be done when the cast is done. */
          okay = TRUE;
          std_conv->cast_base_class = bcp;
        } else if ((!suppress_extensions || !C_mode()) &&
                   qualification_conversion_possible
                                     (source_type_pointed_to,
				      dest_type_pointed_to,
				      &qualifiers_added,
                                      /*ignore_underlying_type=*/FALSE)) {
          /* Allow conversion between pointers where type qualifiers are
             being added at levels other than the first, e.g.,
             "int **" -> "const int * const *".  These are the const-safe
             cases.  This is an extension in C mode. */
          okay = TRUE;
          std_conv->nontrivial_conversion = FALSE;
          std_conv->type_qualifiers_added = qualifiers_added;
          qualifiers_checked = TRUE;
        } else if ((!suppress_extensions || any_cfront_mode()) &&
                    same_type_with_added_qualifiers(
                           source_type_pointed_to,
                           dest_type_pointed_to,
                           /*ignore_qualifiers=*/
                               allow_qualifier_or_eh_mismatch,
                           &qualifiers_added)) {
          /* Allow conversion between pointers where type qualifiers are
             being added at levels other than the first, e.g.,
             "int **" -> "const int **".  This is similar to the qualification
             conversion that is checked above, except that a few additional
             cases are accepted.  These are cases that are not const-safe,
             so a warning is issued in most modes.  This is an extension. */
          okay = TRUE;
          std_conv->nontrivial_conversion = FALSE;
          std_conv->type_qualifiers_added = qualifiers_added;
          qualifiers_checked = TRUE;
          if (!any_cfront_mode()) {
            std_conv->warning_suggested = default_warning_code;
          }  /* if */
        } else if (C_mode() && !suppress_extensions &&
                   interchangeable_types(unqual_dest_type_pointed_to,
                                         unqual_source_type_pointed_to)) {
          /* In C, allow conversion between pointers to interchangeable types,
             as an extension, with a warning.  This covers cases like
             "unsigned char *" --> "char *". */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        } else if (C_mode() && !suppress_extensions &&
                   is_function(unqual_dest_type_pointed_to) &&
                   is_function(unqual_source_type_pointed_to)) {
          /* In C, allow conversion between incompatible pointers to
             functions, as an extension, with a warning. */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        } else if (C_mode() && !suppress_extensions &&
                   (C_dialect == C_dialect_pcc || SVR4_C_mode || gcc_mode ||
                    microsoft_mode)) {
          /* In pcc mode, SVR4 C, gcc, and Microsoft C modes, allow conversion
             between incompatible pointer types, with a warning. */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        }  /* if */
      }  /* if */
    }  /* if */
    if (okay && !qualifiers_checked && !allow_qualifier_or_eh_mismatch) {
      /* The types pointed to must be such that the type pointed to by the
         left has all the qualifiers of the type pointed to by the right.
         It might have additional qualifiers.  ANSI C 3.3.16.1 (assignment);
         ARM 4.6 (pointer conversions: qualifiers cannot be dropped
         implicitly), 5.17 (assignment), 8.4 (initializers). */
      a_type_qualifier_set dest_type_qualifiers =
                                     get_type_qualifiers(dest_type_pointed_to);
      a_type_qualifier_set source_type_qualifiers =
                                   get_type_qualifiers(source_type_pointed_to);
      if (dest_type_qualifiers == source_type_qualifiers) {
        /* The qualifiers are the same. */
      } else if (!C_mode() && is_template_dependent_context() &&
                 (is_template_param_type(dest_type_pointed_to) ||
                  is_template_param_type(source_type_pointed_to))) {
        /* Because template parameters can include type qualifiers (e.g.,
           "T" could be "const int"), qualifier differences over them
           might be okay. */
      } else if (any_qualifier_in_set_missing(dest_type_qualifiers,
                                              source_type_qualifiers)) {
        /* Qualifiers are being dropped. */
        if (string_literals_are_const &&
            source_is_string_literal &&
            source_type_qualifiers == (dest_type_qualifiers | TQ_CONST) &&
            same_entities(unqual_dest_type_pointed_to,
                          unqual_source_type_pointed_to)) {
          /* A deprecated conversion in standard C++ allows conversion of
             a string literal or wide string literal to a pointer to
             non-const ([conv.array] paragraph 2). */
          std_conv->conv_of_string_literal_to_ptr_to_nonconst = TRUE;
        } else if (cfront_2_1_mode && 
                   is_void(unqual_dest_type_pointed_to) &&
                   is_void(unqual_source_type_pointed_to)) {
          /* cfront 2.1 allows conversion of a pointer to qualified void
             (e.g., "const void *") to "void *". */
        } else if ((microsoft_mode && C_mode()) || gcc_mode) {
          /* Microsoft C mode and gcc mode allow dropping qualifiers. */
          std_conv->warning_suggested = default_warning_code;
        } else {
          /* Qualifiers are being dropped. */
          okay = FALSE;
        }  /* if */
      } else {
        /* Qualifiers are being added. */
        std_conv->type_qualifiers_added = TRUE;
      }  /* if */
    }  /* if */
  } else if ((C_dialect == C_dialect_pcc || SVR4_C_mode || gcc_mode ||
             (C_mode() && microsoft_mode)) &&
	     is_integral_or_enum(source_type) && !suppress_extensions) {
    /* In pcc, SVR4, gcc, and Microsoft C compatibility modes, allow
       integer --> pointer with a warning.  The null pointer constant -->
       pointer case has been handled above and does not come here. */
    okay = TRUE;
    std_conv->warning_suggested = default_warning_code;
  } else if (is_error(source_type)) {
    /* Error --> pointer is always allowed. */
    okay = TRUE;
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_pointer_conversion: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_pointer_conversion */


a_boolean this_param_types_correspond(
                                   a_type_ptr rout_type_1,
                                   a_type_ptr rout_type_2,
                                   a_boolean  check_as_conversion,
                                   a_boolean  check_as_operands)
/*
Return TRUE if the "this" parameter types of the two function types given
match if one ignores any difference in the underlying class.  If neither
function type has a "this" parameter, they are also considered to match.
If check_as_conversion is TRUE, rout_type_1 and rout_type_2 are the
destination and source types of a conversion.  If check_as_operands is
TRUE, the two types are the types of the operands of an operation.
If neither is TRUE, the types are checked for an exact match.
*/
{
  a_boolean                      correspond = FALSE;
  a_routine_type_supplement_ptr  rtsp_1, rtsp_2;
  a_type_ptr                     this_class_1, this_class_2;

  rtsp_1 = skip_typerefs(rout_type_1)->variant.routine.extra_info;
  rtsp_2 = skip_typerefs(rout_type_2)->variant.routine.extra_info;
  this_class_1 = rtsp_1->this_class;
  this_class_2 = rtsp_2->this_class;
  if (this_class_1 == NULL) {
    /* type_1 does not have a "this" parameter type.  Match if type_2 also
       does not. */
    correspond = (this_class_2 == NULL);
  } else if (this_class_2 == NULL) {
    /* type_1 has a "this" parameter type, type_2 does not. */
    /* correspond = FALSE;  -- already set. */
  } else {
    a_type_qualifier_set  qualifiers_1 = rtsp_1->qualifiers,
                          qualifiers_2 = rtsp_2->qualifiers;
    if (!any_cfront_mode()) {
      if (qualifiers_1 != qualifiers_2) {
        /* The type qualifiers do not match. */
        /* correspond = FALSE;  -- already set. */
      } else {
        /* The underlying types are assumed to be appropriate class types. */
        correspond = TRUE;
      }  /* if */
    } else {
      /* In cfront compatibility mode, allow type qualifiers on the "this"
         parameter to be dropped.  For example:
           struct A { void f() const; };
           void (A::*fp)() = &A::f;  // allowed as an extension
         Note that this is backwards from what you would expect, but it
         makes sense: it's okay to put a pointer to const function into
         a pointer to non-const, because when you call the function it
         won't modify the object, which is okay, but it wouldn't be good
         to call a non-const function through a pointer to const function. */
      if (check_as_operands) {
        /* On operands, the type qualifiers are ignored. */
        correspond = TRUE;
      } else if (qualifiers_1 == qualifiers_2) {
        correspond = TRUE;
      } else if (check_as_conversion) {
        /* The type qualifiers do not match, but this is a conversion,
           so that may be okay. */
        if (any_qualifier_in_set_missing(qualifiers_2, qualifiers_1)) {
          /* Some type qualifiers are being added; that's never okay. */
          /* correspond = FALSE;  -- already set. */
        } else {
          /* Some type qualifiers are being dropped; that's okay as an
             extension. */
          correspond = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return correspond;
}  /* this_param_types_correspond */


static a_boolean function_types_correspond(
                                   a_type_ptr rout_type_1,
                                   a_type_ptr rout_type_2,
                                   a_boolean  allow_qualifier_or_eh_mismatch)
/*
Return TRUE if the two function types given are compatible if one ignores any
difference in the underlying class of their "this" parameter types.
If allow_qualifier_or_eh_mismatch is TRUE, ignore cv-qualifier and exception
specification mismatches (the two types are probably the types of the
operands of an operation).
*/
{
  a_boolean correspond;

  rout_type_1 = skip_typerefs(rout_type_1);
  rout_type_2 = skip_typerefs(rout_type_2);
  correspond = types_are_compatible(rout_type_1->variant.routine.return_type,
                                   rout_type_2->variant.routine.return_type) &&
               param_types_are_compatible(rout_type_1, rout_type_2,
                                    TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) &&
               this_param_types_correspond(rout_type_1, rout_type_2,
                                           !allow_qualifier_or_eh_mismatch,
                                           allow_qualifier_or_eh_mismatch);
  return correspond;
}  /* function_types_correspond */


static a_boolean member_types_correspond(
                                   a_type_ptr dest_type,
                                   a_type_ptr source_type,
                                   a_boolean  allow_qualifier_or_eh_mismatch,
				   a_boolean  *qualifiers_added)
/*
Return TRUE if the member types from two pointer-to-member types match
allowing for a possible difference due to the associated class type.
Specifically, this means that when comparing function types, the
difference in the underlying class of the "this" parameter type must
be ignored.  If allow_qualifier_or_eh_mismatch is TRUE, ignore
cv-qualifier and exception specification mismatches (the two types are
probably the types of the operands of an operation).
*/
{
  a_boolean correspond;

  *qualifiers_added = FALSE;
  if (!is_function_type(dest_type) || !is_function_type(source_type)) {
    /* This is not the special function case, so the normal check will work. */
    correspond = qualification_conversion_possible
                                  (source_type, dest_type, qualifiers_added,
                                   /*ignore_underlying_type=*/FALSE);
  } else {
    /* We have two function types from member pointers.  See if they
       match when we allow for the difference in the underlying type
       of the "this" parameter.  Note that this test must be done even
       when the class types are the same, because the routines may
       be from base classes. */
    correspond = function_types_correspond(dest_type, source_type,
                                           allow_qualifier_or_eh_mismatch);
  }  /* if */
  return correspond;
}  /* member_types_correspond */


a_boolean impl_ptr_to_member_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type source_type
(any type) to something of type dest_type (a pointer to member type).
If source_is_constant is TRUE, the source is a constant, and source_constant
points to the constant value.  (That's needed to check for conversions of a
null pointer constant to a pointer to member type.)  If
allow_qualifier_or_eh_mismatch is TRUE, ignore cv-qualifier and
exception specification mismatches (the two types are probably the
types of the operands of an operation).  If the conversion is
possible, *std_conv is filled out to describe the conversion.

Note that any type qualifiers on the types themselves (rather than the
types pointed to) are ignored.

See ARM 5.17 (assignment operators) and 4.8 (standard conversions for
pointers to members).
*/
{
  a_boolean  okay = FALSE;
 
  db_enter(5, "impl_ptr_to_member_conversion");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_ptr_to_member_conversion: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  clear_std_conv_descr(std_conv);
  /* Assume a nontrivial conversion; the flag will be cleared later if in
     fact there is nothing nontrivial. */
  std_conv->nontrivial_conversion = TRUE;
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  if (is_ptr_to_member(source_type)) {
    /* Pointer-to-member --> pointer-to-member.  Allowed if the types pointed
       to are the same (ignoring the difference in "this" parameter types)
       and the classes involved are the same or the destination class is an
       unambiguous derived (sic) class of the source class. */
    a_boolean        classes_okay = FALSE;
    a_type_ptr       source_class_type = pm_class_type(source_type);
    a_type_ptr       dest_class_type = pm_class_type(dest_type);
    a_base_class_ptr bcp;

    /* Check the classes. */
    if (identical_types(source_class_type, dest_class_type)) {
      /* Same class, okay. */
      classes_okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if ((bcp = find_base_class_of(dest_class_type,
                                         source_class_type)) != NULL) {
      /* Derived class, okay. */
      /* We leave the ambiguity and accessibility check to be done when
         the cast is done. */
      classes_okay = TRUE;
      std_conv->cast_base_class = bcp;
      std_conv->reversed_cast = TRUE;
    } else if (source_class_type->variant.class_struct_union.is_nonreal_class||
               dest_class_type->variant.class_struct_union.is_nonreal_class) {
      /* Template-dependent types.  Assume that they match. */
      classes_okay = TRUE;
    }  /* if */
    if (classes_okay) {
      a_type_ptr source_type_pointed_to = pm_member_type(source_type);
      a_type_ptr dest_type_pointed_to = pm_member_type(dest_type);
      a_boolean  qualifiers_added;

      /* Check the member types. */
      if (member_types_correspond(dest_type_pointed_to,
                                  source_type_pointed_to,
                                  allow_qualifier_or_eh_mismatch,
                                  &qualifiers_added)) {
        std_conv->type_qualifiers_added = qualifiers_added;
        okay = TRUE;
        if (!allow_qualifier_or_eh_mismatch &&
            is_function_type(dest_type_pointed_to) &&
            !exception_spec_conversion_possible(source_type_pointed_to,
                                                dest_type_pointed_to)) {
          /* In pointer-to-member-function assignment and initialization, any
             exception allowed by the source type must be allowed by the
             destination type; but that's not the case here, so return
             a flag. */
          std_conv->exception_spec_incompatibility = TRUE;
        }  /* if */
        if (!allow_qualifier_or_eh_mismatch) {
          /* The types pointed to must be such that the type pointed to by the
             left has all the qualifiers of the type pointed to by the right.
             It might have additional qualifiers. */
          a_type_qualifier_set dest_type_qualifiers =
                                     get_type_qualifiers(dest_type_pointed_to);
          a_type_qualifier_set source_type_qualifiers =
                                   get_type_qualifiers(source_type_pointed_to);
          if (dest_type_qualifiers == source_type_qualifiers) {
            /* The qualifiers are the same. */
          } else if (qualification_conversion_possible
                                (source_type_pointed_to, dest_type_pointed_to,
		                 &qualifiers_added,
                                 /*ignore_underlying_type=*/FALSE)) {
            /* This is an allowed qualification conversion. */
            std_conv->type_qualifiers_added = qualifiers_added;
          }  /* if */
        }  /* if */
      } else if (is_template_dependent_context() &&
                 (is_template_dependent_type(source_type_pointed_to) ||
                  is_template_dependent_type(dest_type_pointed_to))) {
        /* Conversion to or from a template-dependent type is allowed. */
        okay = TRUE;
      }  /* if */
    }  /* if */
  } else if (is_template_param_type(source_type)) {
    /* A template parameter type might be a pointer-to-member type.
       This has to be tested before the template null pointer case because
       we don't want the pointer_normalization_needed flag set. */
    okay = TRUE;
  } else if (source_is_constant &&
             is_or_might_be_null_pointer_constant(source_constant)) {
    /* 0 --> pointer-to-member. */
    okay = TRUE;
    std_conv->pointer_normalization_needed = TRUE;
  } else if (is_error(source_type)) {
    /* Error --> pointer to member is always allowed. */
    okay = TRUE;
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_ptr_to_member_conversion: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_ptr_to_member_conversion */


a_boolean impl_conversion_possible(
                          a_type_ptr           source_type,
                          a_boolean            source_is_constant,
                          a_boolean            source_is_string_literal,
                          a_constant           *source_constant,
                          a_type_ptr           dest_type,
                          a_boolean            allow_qualifier_or_eh_mismatch,
                          a_boolean            suppress_extensions,
                          an_error_code        default_warning_code,
                          a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it is okay to implicitly convert something of type source_type
to something of type dest_type.  If source_is_constant is TRUE, the source
is a constant, and source_constant points to the constant value.  (That's
needed to check for conversions of a null pointer constant to a pointer
type.)  If source_is_string_literal is TRUE, the source is a simple
string literal (that's needed for the deprecated conversion from
string literal to "char *"); the flag can be TRUE even when
source_is_constant is FALSE, for an extension.  suppress_extensions is
TRUE if conversions that are extensions should not be allowed (what
constitutes an extension depends on C_dialect, of course).  If the
conversion is possible, *std_conv is filled out to describe the
conversion.  In particular, if the conversion is suspect and should be
flagged with a warning, the warning_suggested field is set to an
appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into warning_suggested when no
specific message applies.

Note that any top-level type qualifiers on the types are ignored, and
when allow_qualifier_or_eh_mismatch is TRUE exception specifications
and lower level cv-qualifiers are also ignored.

See chapter 4 of the ARM (standard conversions).  Note that integral
promotions, default argument promotions, the usual arithmetic conversions,
array --> pointer to element, and function --> pointer to function are
handled in normal expression processing rather than here.  Reference
conversions have been turned into pointer conversions by the time they
get here.  User-defined conversions (constructors and conversion functions)
are not checked for here.

See also 3.3.16.1 in the ANSI C standard (simple assignment).

Note that the set of conversions handled here is not the full set allowed
in assignments (for example, struct --> same struct is not handled here).
See conversion_possible.
*/
{
  a_boolean  okay = FALSE;
  a_type_ptr dest_enum_type, source_enum_type;

  db_enter(5, "impl_conversion_possible");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_conversion_possible: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  clear_std_conv_descr(std_conv);
  /* Assume a nontrivial conversion; the flag will be cleared later if in
     fact there is nothing nontrivial. */
  std_conv->nontrivial_conversion = TRUE;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  if (is_incomplete(dest_type)) {
    /* Catch cases where an actual argument is being converted to an incomplete
       enum or struct/union type because a function parameter has that type.
       One is allowed to declare a parameter of that type, but the type must
       be completed if the function is defined or called. */
    /* okay = FALSE; -- already set. */
  } else if (is_bool(dest_type)) {
    /* Conversion to the bool type.  This is possible only in C++.
       Conversion is allowed from arithmetic, enumeration, pointer,
       and pointer to member. */
    if (is_bool(source_type)) {
      /* bool --> bool is no conversion. */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (is_arithmetic_or_enum(source_type)) {
      okay = TRUE;
    } else if (is_pointer(source_type) || is_ptr_to_member(source_type)) {
      okay = TRUE;
      /* This conversion is worse than others in overload resolution.
         Remember that. */
      std_conv->ptr_or_pm_to_bool = TRUE;
    }  /* if */
  } else if (is_arithmetic_or_enum(dest_type)) {
    /* Destination type is arithmetic or enum. */
    if (identical_types(source_type, dest_type)) {
      /* No type change. */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (!C_mode() && is_enum(dest_type)) {
      /* Conversion to an enum type in C++.  We already know this is not
         a conversion of an enum type to itself, so this is an error case:
         you can't convert other types to enum implicitly. */
      if (cfront_2_1_mode && is_integral_or_enum(source_type)) {
        /* cfront 2.1 allows conversion of integral or other enum types to
           an enum, with a warning.  (It also allows floating point types
           to be converted to an enum, but it doesn't seem necessary to
           duplicate that behavior.) */
        okay = TRUE;
        std_conv->warning_suggested = ec_mixed_enum_type;
      }  /* if */
    } else if (is_arithmetic_or_enum(source_type)) {
      /* Arithmetic or enum --> arithmetic (including enum in C). */
      okay = TRUE;
      if (C_mode()) {
        /* In C, check for conversion of one enumerated type to another,
           or conversion of an arithmetic non-enum type to an enum.
           C++ cases of converting to an enum were handled above. */
        dest_enum_type = NULL;
        if (is_integral_or_enum(dest_type)) {
          dest_enum_type = underlying_enum_type(dest_type);
        }  /* if */
        if (dest_enum_type != NULL) {
          /* Conversion is to an enum type. */
          source_enum_type = NULL;
          if (is_integral_or_enum(source_type)) {
            source_enum_type = underlying_enum_type(source_type);
          }  /* if */
          if (!same_entities(source_enum_type, dest_enum_type)) {
            /* Warn on mixing different enums, or non-enums and enums. */
            std_conv->warning_suggested = ec_mixed_enum_type;
          }  /* if */
        }  /* if */
      } else {
        /* C++ mode. */
        /* Note the cases that are promotions. */
        a_type_ptr prom_type = default_argument_promotion(source_type);
        if (types_are_compatible(prom_type, dest_type)) {
          std_conv->promotion = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_bugs &&
                   is_integral(source_type) &&
                   source_type->variant.integer.int_kind ==
                                                    (an_integer_kind)ik_long &&
                   is_integral(dest_type) &&
                   dest_type->variant.integer.int_kind ==
                                                    (an_integer_kind)ik_int) {
          /* MSVC++ considers long --> int to be better than a standard
             conversion (presumably because the representations are the
             same). */
          std_conv->promotion = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
      if (c99_mode) {
        /* Conversions between imaginary and real/integral drop information,
           so warn about those.  Don't warn if the source is a zero
           constant, because that may be intentional. */
        if ((is_imaginary(source_type) && !is_nonreal_floating(dest_type)) ||
            (is_imaginary(dest_type)   && !is_nonreal_floating(source_type) &&
             (!source_is_constant || !is_zero_constant(source_constant)))) {
          std_conv->warning_suggested = ec_real_imaginary_conversion;
        }  /* if */
      }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    } else if ((C_dialect == C_dialect_pcc || SVR4_C_mode || gcc_mode ||
                (C_mode() && microsoft_mode)) &&
               is_pointer(source_type) &&
               is_integral_or_enum(dest_type)) {
      /* In pcc, SVR4, gcc, or Microsoft C modes, allow pointer --> integer
         (even if the integer is not big enough).  Issue a warning. */
      okay = TRUE;
      std_conv->warning_suggested = default_warning_code;
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
                                   source_is_string_literal,
                                   source_constant, dest_type,
                                   allow_qualifier_or_eh_mismatch,
                                   suppress_extensions,
                                   default_warning_code,
                                   std_conv);
  } else if (is_ptr_to_member(dest_type)) {
    /* Conversion to a C++ pointer-to-member type. */
    okay = impl_ptr_to_member_conversion(source_type,
                                         source_is_constant, source_constant,
                                         dest_type,
                                         allow_qualifier_or_eh_mismatch,
                                         std_conv);
  } else if (is_error(dest_type)) {
    /* Anything can be converted to an error type. */
    okay = TRUE;
  } else if (is_template_param_type(dest_type)) {
    /* Anything can be converted to a template parameter type in a prototype
       instantiation. */
    okay = TRUE;
  }  /* if */
  /* If compatibility was not found any other way, check for the source
     having an error type or a template parameter type. */
  if (!okay) {
    if (is_error(source_type) || is_template_param_type(source_type)) {
      okay = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_conversion_possible */


a_boolean conversion_allowed_for_nontype_template_argument(
                                                  a_std_conv_descr *conversion)
/*
Return TRUE unless the indicated conversion contains something that
is not allowed in a conversion for a nontype template argument, e.g.,
a conversion of 0 to a pointer type.
*/
{
  a_boolean allowed = TRUE;

  if (conversion->pointer_normalization_needed && !microsoft_mode) {
    /* Conversion of 0 to a pointer type, or of a pointer to object type
       to void *, is not allowed on a nontype template argument. */
    allowed = FALSE;
  } else if (conversion->cast_base_class != NULL) {
    /* Derived-to-base pointer conversions and base-to-derived
       pointer-to-member conversions are not allowed on a nontype
       template argument. */
    allowed = FALSE;
  }  /* if */
  return allowed;
}  /* conversion_allowed_for_nontype_template_argument */


static a_boolean inverse_impl_conversion_possible(
                          a_type_ptr           source_type,
                          a_type_ptr           dest_type,
                          a_boolean            suppress_extensions,
                          a_boolean            allow_qualifier_or_eh_mismatch,
                          a_std_conv_descr_ptr std_conv)
/*
Return TRUE if the conversion source_type --> dest_type can be done as
a static_cast because the inverse dest_type --> source_type can be done as
an implicit conversion.  This is used for checking the part of static_cast
that allows the inverse of any standard conversion.  typerefs are already
removed from the types.  suppress_extensions is TRUE if conversions that
are extensions should not be allowed (what constitutes an extension depends
on C_dialect, of course).  If the conversion is possible, *std_conv is
filled out to describe the conversion.  In particular, if the conversion
is suspect and should be flagged with a warning, the warning_suggested
field is set to an appropriate error code; normally, it is set to
ec_no_error.  When allow_qualifier_or_eh_mismatch is TRUE, cv-qualifiers and
exception specifications are not checked.
*/
{
  a_boolean        okay = FALSE, baseward_cast, related_class_case = FALSE;
  a_base_class_ptr bcp;
  a_type_ptr       source_type_pointed_to, dest_type_pointed_to;
  a_boolean        qualifiers_added;

  clear_std_conv_descr(std_conv);
  if (related_class_pointers(source_type, dest_type, &baseward_cast, &bcp) &&
      !baseward_cast) {
    /* A pointer to a base class can be cast to a pointer to a derived
       class if it's not a virtual base and no qualifiers are dropped. */
    if (!bcp->is_virtual) {
      related_class_case = TRUE;
      source_type_pointed_to = type_pointed_to(source_type);
      dest_type_pointed_to = type_pointed_to(dest_type);
    }  /* if */
  } else if (related_member_pointers(source_type, dest_type, &baseward_cast,
                                     &bcp) && baseward_cast) {
    /* A pointer to member of a derived class can be cast to a pointer to
       member of a base class if no qualifiers are dropped. */
    source_type_pointed_to = pm_member_type(source_type);
    dest_type_pointed_to = pm_member_type(dest_type);
    if (member_types_correspond(dest_type_pointed_to,
                                source_type_pointed_to,
                                /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                &qualifiers_added)) {
      related_class_case = TRUE;
    }  /* if */
  }  /* if */
  if (related_class_case) {
    /* For pointer and pointer to member related class cases, check that
       no type qualifiers are being dropped. */
    a_type_qualifier_set source_type_qualifiers =
                                   get_type_qualifiers(source_type_pointed_to);
    a_type_qualifier_set dest_type_qualifiers =
                                     get_type_qualifiers(dest_type_pointed_to);
    okay = TRUE;
    if (dest_type_qualifiers == source_type_qualifiers) {
      /* The qualifiers are the same. */
    } else if (any_qualifier_in_set_missing(dest_type_qualifiers,
                                            source_type_qualifiers)) {
      /* Qualifiers are being dropped. */
      okay = FALSE;
    }  /* if */
  } else if ((!is_bool_type(source_type) &&
              impl_conversion_possible(dest_type,
                                       /*source_is_constant=*/FALSE,
                                       /*source_is_string_literal=*/FALSE,
                                       (a_constant *)NULL,
                                       source_type,
                                       allow_qualifier_or_eh_mismatch,
                                       suppress_extensions,
                                       ec_bad_cast,
                                       std_conv)) ||
             /* Test for conversion of "void *" to a pointer to object type.
                This does not fall out of the impl_conversion_possible
                test for cases like "void *" --> "const char *".  See
                5.2.9/10 in the C++ standard. */
             (is_pointer_type(source_type) &&
              is_pointer_type(dest_type) &&
              is_void_type(type_pointed_to(source_type)) &&
              is_object_type(type_pointed_to(dest_type)))) {
    /* The inverse implicit conversion can be done.  Note that the
       inverse of conversions to bool is not allowed (see [expr.static.cast]
       paragraph 9). */
    okay = TRUE;
    /* If the conversion is a pointer or pointer to member conversion, make
       sure qualifiers are not being removed. */
    if (!allow_qualifier_or_eh_mismatch &&
        ((is_pointer(source_type) && is_pointer(dest_type)) ||
        (is_ptr_to_member(source_type) && is_ptr_to_member(dest_type)))) {
      if (cast_removes_qualifiers(source_type, dest_type)) {
        okay = FALSE;
      }  /* if */
    }  /* if */
  } else if (is_enum_type(source_type) && is_enum_type(dest_type)) {
    /* Core Issue 128 makes enum --> enum a valid static_cast. */
    okay = TRUE;
  } else if (!C_mode() &&
             is_bool_type(source_type) && is_enum_type(dest_type)) {
    /* Allow bool --> enum.  [expr.static.cast] paragraphs 6 and 7
       strictly speaking seem to disallow it, but that's probably a mistake
       in the standard. */
    okay = TRUE;
  }  /* if */
  return okay;
}  /* inverse_impl_conversion_possible */


a_boolean static_cast_conversion_possible(
                                 a_type_ptr    source_type,
                                 a_boolean     source_is_constant,
                                 a_boolean     source_is_string_literal,
                                 a_constant    *source_constant,
                                 a_type_ptr    dest_type,
                                 a_boolean     allow_qualifier_or_eh_mismatch,
                                 an_error_code default_warning_code,
                                 an_error_code *warning_suggested)
/*
Return TRUE if it is okay to explicitly convert something of type source_type
to something of type dest_type in a static_cast.  If source_is_constant is
TRUE, the source is a constant, and source_constant points to the constant
value.  (That's needed to check for conversions of a null pointer constant
to a pointer type.)  If source_is_string_literal is TRUE, source is a
simple string literal (that's needed for the deprecated conversion
from string literal to "char *"); the flag can be TRUE even when
source_is_constant is FALSE, for an extension.  If the conversion is
suspect and should be flagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  Any type qualifiers on the types themselves
are ignored, and when allow_qualifier_or_eh_mismatch is TRUE exception
specifications and lower level cv-qualifiers are also ignored.
Note that this routine does not handle casts to reference types and it doesn't
handle user-defined conversions.  This routine is called in C mode as well as
C++ mode.  See [expr.static.cast].
*/
{
  a_boolean        okay = FALSE, suppress_extensions = FALSE;
  a_boolean        impl_okay = FALSE, inv_impl_okay = FALSE;
  a_std_conv_descr impl_std_conv, inv_impl_std_conv;

  db_enter(5, "static_cast_conversion_possible");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "static_cast_conversion_possible: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  check_assertion_str(!is_reference_ptr(dest_type),
                    "static_cast_conversion_possible: dest_type is reference");

  if (is_void(dest_type)) {
    /* Anything --> (possibly qualified) void is allowed. */
    okay = TRUE;
  } else if (is_incomplete(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else {
    impl_okay = impl_conversion_possible(source_type, source_is_constant,
                                         source_is_string_literal,
                                         source_constant, dest_type,
                                         allow_qualifier_or_eh_mismatch,
                                         suppress_extensions,
                                         default_warning_code,
                                         &impl_std_conv) != FALSE;
    if (impl_okay &&
        impl_std_conv.warning_suggested == ec_no_error) {
      /* There is an implicit conversion, and it's not questionable. */
      okay = TRUE;
    } else if (!C_mode()) {
      inv_impl_okay = inverse_impl_conversion_possible(
                                               source_type, dest_type,
                                               allow_qualifier_or_eh_mismatch,
                                               suppress_extensions,
                                               &inv_impl_std_conv);
      if (inv_impl_okay &&
          inv_impl_std_conv.warning_suggested == ec_no_error) {
        /* The inverse of any standard conversion is allowed in C++. */
        okay = TRUE;
      }  /* if */
    } else if (is_integral_or_enum(source_type) && is_enum(dest_type)) {
      /* In C, integral --> enum can be done as an implicit conversion
         but we check for it again here to avoid the warning. */
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay) {
    if (impl_okay) {
      /* There is a questionable implicit conversion that covers this case.
         Allow it, with a warning. */
      okay = TRUE;
      *warning_suggested = impl_std_conv.warning_suggested;
    } else if (inv_impl_okay) {
      /* There is a questionable inverse conversion that covers this
         case.  Allow it, with a warning. */
      okay = TRUE;
      *warning_suggested = inv_impl_std_conv.warning_suggested;
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "static_cast_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* static_cast_conversion_possible */


static a_boolean compound_conversion_possible(a_type_ptr source_type,
                                              a_type_ptr dest_type)
/*
Some conversions that are allowed in an explicit C++ cast are allowed
because they are a combination of other casts, e.g., a static_cast
plus a const_cast.  Test for such conversions between the given source
and destination types, and return TRUE if one is allowed.
*/
{
  a_boolean        okay = FALSE;
  a_boolean        baseward_cast;
  a_base_class_ptr bcp;

  if (related_class_pointers(source_type, dest_type, &baseward_cast, &bcp)) {
    /* A cast from const Derived * to Base * is allowed as a combination
       of a static_cast and a const_cast. */
    okay = TRUE;
  }  /* if */
  return okay;
}  /* compound_conversion_possible */


a_boolean reinterpret_cast_conversion_possible(
                                              a_type_ptr    source_type,
                                              a_type_ptr    dest_type,
                                              an_error_code *warning_suggested)
/*
Return TRUE if it is okay to explicitly convert something of type source_type
to something of type dest_type in a reinterpret_cast.  Any type qualifiers
on the types themselves are ignored.  If the conversion
is suspect and should be flagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
Note that this routine does not handle casts to reference types, it
doesn't reject conversions that cast away constness, and it doesn't
handle user-defined conversions.  This routine is called in C mode as
well as C++ mode.
*/
{
  a_boolean okay = FALSE, suppress_extensions = FALSE;

  db_enter(5, "reinterpret_cast_conversion_possible");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "reinterpret_cast_conversion_possible: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  check_assertion_str(!is_reference_ptr(dest_type),
               "reinterpret_cast_conversion_possible: dest_type is reference");

  if (is_incomplete(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else if (is_pointer(source_type) && is_integral(dest_type) &&
             (C_mode() || microsoft_mode ||
              dest_of_ptr_cast_big_enough(source_type, dest_type))) {
    /* Pointer --> integral is okay
         -- In C mode, always (size of destination is not an issue; see
            6.3.4 in the ISO C89 standard)
         -- In C++ mode, if (a) the integer is big enough or (b) it's
            not big enough but we're compiling in Microsoft mode. */
    okay = TRUE;
    if (!dest_of_ptr_cast_big_enough(source_type, dest_type)) {
      /* The destination is not large enough to hold all of the bits
         of the pointer.  Issue a warning. */
      *warning_suggested = ec_pointer_conversion_loses_bits;
    }  /* if */
  } else if (is_integral_or_enum(source_type) && is_pointer(dest_type)) {
    /* Integral or enum --> pointer. */
    okay = TRUE;
    if (!dest_of_ptr_cast_big_enough(source_type, dest_type)) {
      /* The destination is not large enough to hold all of the bits
         of the integer.  Issue a warning. */
      *warning_suggested = ec_conversion_to_pointer_loses_bits;
    }  /* if */
  } else if (is_pointer(source_type) && is_pointer(dest_type)) {
    /* Pointer --> pointer.  Get the types pointed to. */
    a_type_ptr source_type_pointed_to, dest_type_pointed_to;
    source_type_pointed_to = type_pointed_to(source_type);
    source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    dest_type_pointed_to = type_pointed_to(dest_type);
    dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
    if (is_function(source_type_pointed_to) ==
        is_function(dest_type_pointed_to)) {
      /* Pointer to function --> pointer to function, or pointer to
         object/incomplete --> pointer to object/incomplete.  Allowed in both
         C and C++. */
      okay = TRUE;
    } else {
      /* Pointer to function --> pointer to object/incomplete, or pointer
         to object/incomplete --> pointer to function.  Allowed as an
         extension in C and C++ if the destination is big enough. */
      if (!suppress_extensions &&
          dest_of_ptr_cast_big_enough(source_type, dest_type)) {
        okay = TRUE;
      }  /* if */
    }  /* if */
  } else if (is_ptr_to_member(source_type) &&
             is_ptr_to_member(dest_type)) {
    /* Pointer-to-member --> pointer-to-member.  Valid as long as both
       pointers are pointers to data members or both are pointers to member
       functions.  Note that this conversion cannot be folded as part of a
       constant-expression. */
    if (is_function_type(pm_member_type(source_type)) ==
        is_function_type(pm_member_type(dest_type))) {
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (!okay) {
    /* No normal conversion.  Look for error and template matches. */
    if (is_error(dest_type) || is_template_param_type(dest_type)) {
      if (is_error(source_type) || is_template_param_type(source_type) ||
          is_integral_or_enum(source_type) || is_pointer(source_type) ||
          is_ptr_to_member(source_type)) {
        okay = TRUE;
      }  /* if */
    } else if (is_error(source_type) || is_template_param_type(source_type)) {
      if (is_integral_or_enum(dest_type) || is_pointer(dest_type) ||
          is_ptr_to_member(dest_type)) {
        okay = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "reinterpret_cast_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* reinterpret_cast_conversion_possible */


a_boolean expl_conversion_possible(a_type_ptr    source_type,
                                   a_boolean     source_is_constant,
                                   a_boolean     source_is_string_literal,
                                   a_constant    *source_constant,
                                   a_type_ptr    dest_type,
                                   a_boolean     *reinterpret_cast_needed,
                                   an_error_code default_warning_code,
                                   an_error_code *warning_suggested)
/*
Return TRUE if it is okay to explicitly convert something of type
source_type to something of type dest_type in a C-style cast or a
functional-notation cast.  If source_is_constant is TRUE, the source is
a constant, and source_constant points to the constant value.
(That's needed to check for conversions of a null pointer constant to
a pointer type.)  If source_is_string_literal is TRUE, the source is a
simple string literal (that's needed for the deprecated conversion
from string literal to "char *"); the flag can be TRUE even when
source_is_constant is FALSE, for an extension.  Any type qualifiers on
the types themselves are ignored.  If the conversion is suspect and
should be flagged with a warning, *warning_suggested is set to an
appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.

Any implicit conversion is allowed (see impl_conversion_possible).  Also, the
explicit conversions allowed in casts (ARM 5.2.3 and 5.4; ANSI C 3.3.4)
are allowed.  Reference conversions have been turned into pointer conversions
by the time they get here.  Note that this routine does not handle user-defined
conversions (constructors and conversion functions).  If the conversion
requires a reinterpret_cast-like operation, *reinterpret_cast_needed will be
set to TRUE (otherwise it is set to FALSE).
*/
{
  a_boolean     okay = FALSE;
  a_boolean     static_cast_okay, reinterpret_cast_okay;
  an_error_code static_cast_warning_suggested;
  an_error_code reinterpret_cast_warning_suggested;

  db_enter(5, "expl_conversion_possible");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "expl_conversion_possible: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *reinterpret_cast_needed = FALSE;
  *warning_suggested = ec_no_error;
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);

  if (is_incomplete(dest_type) && !is_void(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else {
    static_cast_okay =
      static_cast_conversion_possible(source_type, source_is_constant,
                                      source_is_string_literal,
                                      source_constant, dest_type,
                                      /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                      default_warning_code,
                                      &static_cast_warning_suggested) != FALSE;
    if (static_cast_okay &&
        static_cast_warning_suggested == ec_no_error) {
      /* The conversion can be done as a static_cast, without a warning. */
      okay = TRUE;
    } else if (!C_mode() &&
               same_type_with_added_qualifiers(source_type, dest_type,
                                               /*ignore_qualifiers=*/TRUE,
                                               (a_boolean *)NULL)) {
      /* A const_cast can be done. */
      okay = TRUE;
    } else if (!C_mode() &&
               compound_conversion_possible(source_type, dest_type)) {
      /* Some cases can be done as the combination of a static_cast and
         a const_cast, or of two static_casts. */
      okay = TRUE;
    } else {
      reinterpret_cast_okay =
            reinterpret_cast_conversion_possible(
                              source_type, dest_type,
                              &reinterpret_cast_warning_suggested) != FALSE;
      if (reinterpret_cast_okay &&
          reinterpret_cast_warning_suggested == ec_no_error) {
        /* The conversion can be done as a reinterpret_cast, without a
           warning. */
        okay = TRUE;
        *reinterpret_cast_needed = TRUE;
      } else if (static_cast_okay) {
        /* static_cast is okay but with a warning. */
        okay = TRUE;
        *warning_suggested = static_cast_warning_suggested;
      } else if (reinterpret_cast_okay) {
        /* reinterpret_cast is okay but with a warning. */
        okay = TRUE;
        *warning_suggested = reinterpret_cast_warning_suggested;
        *reinterpret_cast_needed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "expl_conversion_possible: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* expl_conversion_possible */


a_type_ptr multilevel_composite_pointer_type(a_type_ptr type_1,
                                             a_type_ptr type_2)
/*
If the two given multilevel pointer types are identical except for the
cv-qualification on the multilevel pointer structure, this function returns
a similar type with the union of the cv-qualification signatures (except that
top-level qualifiers are dropped).  Otherwise, NULL is returned.
*/
{
  a_type_ptr  result;

  if (same_entities(type_1, type_2)) {
    /* Simple initial test for speed and to preserve typedefs if present. */
    result = type_1;
  } else {
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (identical_types(type_1, type_2)) {
      result = type_1;
    } else if (is_pointer_type(type_1) && is_pointer_type(type_2)) {
      a_type_ptr  type_pointed_to_1 = type_pointed_to(type_1);
      a_type_ptr  type_pointed_to_2 = type_pointed_to(type_2);

      result = multilevel_composite_pointer_type(type_pointed_to_1,
                                                 type_pointed_to_2);
      if (result != NULL) {
        result = make_qualified_type(result,
                                     get_type_qualifiers(type_pointed_to_1) |
                                       get_type_qualifiers(type_pointed_to_2));
        result = make_pointer_type(result);
      }  /* if */
    } else {
      /* The types at this level are not the same, so there is no composite
         type. */
      result = NULL;
    }  /* if */
  }  /* if */
  return result;
}  /* multilevel_composite_pointer_type */


static a_type_ptr composite_array_type(a_type_ptr array_type1,
                                       a_type_ptr array_type2)
/*
Determine the composite type of array types array_type1 and array_type2
and return a pointer to it.  Note: one of the types passed in may be returned
as the composite type; if either could be returned as the composite type,
preference is given to the first.
*/
{
  a_type_ptr    comp_type, comp_elem;
  a_targ_size_t num_elems;
  a_boolean     comp_has_nonconst_dimension = FALSE;

  /* When VLAs (variable length arrays) appear, "array[const]" is
     preferred over "array[expr]", and "array[expr]" or "array[*]"
     is preferred over "array[]".  Usually, the element type for the
     composite array type is the composite of the element types, but
     not for the VLA cases: the proposed words (WG14/N637) for 6.1.2.6
     of the C standard say "if one type is a variable length array
     type the composite type is that type."  This is good, because
     forming the composite element type and building a new VLA type
     on top would be problematical; it would require creation of a
     new a_vla_dimension entry. */
  /* The composite_type call for the element type is done in two
     different orders because if both types are equivalent to the
     composite type, the first operand is returned, and we'd like
     the element type to be the one from the array with the proper
     size. */
  if (!has_unknown_specified_bound(array_type1) &&
      array_type1->variant.array.variant.number_of_elements != 0) {
    /* array_type1 is "array[const]". */
    num_elems = array_type1->variant.array.variant.number_of_elements;
    comp_elem = composite_type(array_type1->variant.array.element_type,
                               array_type2->variant.array.element_type);
  } else if (!has_unknown_specified_bound(array_type2) &&
             array_type2->variant.array.variant.number_of_elements != 0) {
    /* array_type2 is "array[const]". */
    num_elems = array_type2->variant.array.variant.number_of_elements;
    comp_elem = composite_type(array_type2->variant.array.element_type,
                               array_type1->variant.array.element_type);
  } else if (array_type1->variant.array.has_assoc_vla_dimension) {
    /* array_type1 is "array[expr]". */
    comp_type = array_type1;
    comp_has_nonconst_dimension = TRUE;
  } else if (array_type2->variant.array.has_assoc_vla_dimension) {
    /* array_type2 is "array[expr]". */
    comp_type = array_type2;
    comp_has_nonconst_dimension = TRUE;
  } else if (array_type1->variant.array.is_vla) {
    /* array_type1 is "array[*]". */
    comp_type = array_type1;
    comp_has_nonconst_dimension = TRUE;
  } else if (array_type2->variant.array.is_vla) {
    /* array_type2 is "array[*]". */
    comp_type = array_type2;
    comp_has_nonconst_dimension = TRUE;
  } else if (array_type1->variant.array.is_template_dependent_size_array) {
    check_assertion(identical_array_type_level(array_type1, array_type2));
    /* Dimension expression must involve a template param. */
    comp_type = array_type1;
    comp_has_nonconst_dimension = TRUE;
  } else if (array_type2->variant.array.is_template_dependent_size_array) {
    check_assertion(identical_array_type_level(array_type1, array_type2));
    /* Dimension expression must involve a template param. */
    comp_type = array_type2;
    comp_has_nonconst_dimension = TRUE;
  } else {
    /* Both arrays have unknown unspecified bounds ("array[]"). */
    check_assertion(!has_unknown_specified_bound(array_type1) &&
                    array_type1->variant.array.variant.number_of_elements==0 &&
                    !has_unknown_specified_bound(array_type2) &&
                    array_type2->variant.array.variant.number_of_elements==0);
    num_elems = 0;
    comp_elem = composite_type(array_type1->variant.array.element_type,
                               array_type2->variant.array.element_type);
  }  /* if */
  /* For the VLA cases, comp_type is already set to one of the original
     types.  For other cases, see if one of the two types we already have
     matches the required composite type.  If that's not possible, build
     a new array type for the composite. */
  if (!comp_has_nonconst_dimension) {
    if (same_entities(comp_elem,
                      array_type1->variant.array.element_type) &&
        !array_type1->variant.array.is_variable_size_array &&
        num_elems == array_type1->variant.array.variant.number_of_elements) {
      comp_type = array_type1;
    } else if (same_entities(comp_elem,
                             array_type2->variant.array.element_type) &&
        !array_type2->variant.array.is_variable_size_array &&
        num_elems == array_type2->variant.array.variant.number_of_elements) {
      comp_type = array_type2;
    } else {
      comp_type = alloc_type((a_type_kind)tk_array);
      comp_type->variant.array.element_type = comp_elem;
      comp_type->variant.array.variant.number_of_elements = num_elems;
      set_type_size(comp_type);
    }  /* if */
  }  /* if */
  return comp_type;
}  /* composite_array_type */


static a_type_ptr composite_parameter_type(a_type_ptr  type1,
                                           a_type_ptr  type2)
/*
type1 and type2 are types pointed to by corresponding param_type entries of
routine types already known to be compatible with each other.  Determine the
composite of the two parameter types and return a pointer to it.  Note: one
of the types passed in may be returned as the composite type; if either
could be returned as the composite type, preference is given to the first.
*/
{
  a_type_ptr  tp;

  if (C_mode() && !type_qualifiers_match(type1, type2)) {
    /* One tricky case that comes up is
          int f(int);
          int f(const int);
       X3J11 has said that the composite of those parameter types is the
       composite of the unqualified types (interpretation 13).  We use that
       rule only when the qualifiers are different, so that the composite of
          int f(const int, int);
          int f(const int, const int);
       still has "const int" in the first parameter. */
    tp = composite_type(make_unqualified_type(type1),
                        make_unqualified_type(type2));
  } else {
    tp = composite_type(type1, type2);
  }  /* if */
  return tp;
}  /* composite_parameter_type */


static a_type_ptr composite_routine_type(a_type_ptr  rout_type1,
                                         a_type_ptr  rout_type2)
/*
Determine the composite type based on routine types rout_type1 and rout_type2
and return a pointer to it.  Note: one of the types passed in may be returned
as the composite type; if either could be returned as the composite type,
preference is given to the first.
*/
{
  a_type_ptr                     comp_type;
  a_type_ptr                     comp_return_type, tp;
  a_param_type_ptr               param_list1, param_list2, end_of_list;
  a_param_type_ptr               ptp1, ptp2, new_ptp;
  a_boolean                      comp_prototyped;
  a_routine_type_supplement_ptr  rtsp1, rtsp2, rtsp;
  a_boolean                      return_type1_as_comp_type = TRUE;
  a_boolean                      return_type2_as_comp_type = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_calling_convention           comp_calling_convention;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  rtsp1 = rout_type1->variant.routine.extra_info;
  param_list1 = rtsp1->param_type_list;
  rtsp2 = rout_type2->variant.routine.extra_info;
  param_list2 = rtsp2->param_type_list;
  /* Form the composite of the return types. */
  comp_return_type = composite_type(rout_type1->variant.routine.return_type,
                                    rout_type2->variant.routine.return_type);
  if (!same_entities(comp_return_type,
                     rout_type1->variant.routine.return_type)) {
    return_type1_as_comp_type = FALSE;
  }  /* if */
  if (!same_entities(comp_return_type,
                     rout_type2->variant.routine.return_type)) {
    return_type2_as_comp_type = FALSE;
  }  /* if */
  /* If both function types are not prototyped, the composite type is
     likewise not prototyped.  If one of the two types is prototyped, the
     other not, the composite type is the one that is prototyped.  If both
     types are prototyped, the composite type is prototyped, with each
     parameter type in its list being the composite of the corresponding
     parameter types in the two lists. */
  comp_prototyped = rtsp1->prototyped || rtsp2->prototyped;
  if ((a_boolean)(rtsp1->prototyped) != comp_prototyped) {
    return_type1_as_comp_type = FALSE;
  }  /* if */
  if ((a_boolean)(rtsp2->prototyped) != comp_prototyped) {
    return_type2_as_comp_type = FALSE;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  comp_calling_convention = rtsp1->calling_convention;
  if (comp_calling_convention == (a_calling_convention)cc_default) {
    comp_calling_convention = rtsp2->calling_convention;
  }  /* if */
  if (rtsp1->calling_convention != comp_calling_convention) {
    return_type1_as_comp_type = FALSE;
  }  /* if */
  if (rtsp2->calling_convention != comp_calling_convention) {
    return_type2_as_comp_type = FALSE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!return_type1_as_comp_type && !return_type2_as_comp_type) {
    goto make_new_comp_type;
  }  /* if */
  if (!comp_prototyped) {
    /* Both types have old-style (non-prototyped) interfaces, so there is no
       real parameter information in the composite. However, if one or the
       other has parameter information because it's a function with a
       definition, preserve that information in the composite. */
    if (param_list1 != NULL) {
      return_type2_as_comp_type = FALSE;
    }  /* if */
    if (param_list2 != NULL) {
      return_type1_as_comp_type = FALSE;
    }  /* if */
  } else if (!rtsp2->prototyped) {
    /* Type 2 has an old-style interface, so use the prototyped interface
       from type 1. */
    return_type2_as_comp_type = FALSE;
  } else if (!rtsp1->prototyped) {
    /* Type 1 has an old-style interface, so use the prototyped interface
       from type 2. */
    return_type1_as_comp_type = FALSE;
  } else {
    /* First go through both parameter lists and compare the types.  As
       long as one of the lists remains eligible to serve as the composite,
       keep going. */
    for (ptp1 = param_list1, ptp2 = param_list2;
         ptp1 != NULL;
         ptp1 = ptp1->next, ptp2 = ptp2->next) {
#if CHECKING
      if (ptp2 == NULL) {
        /* Since the types are compatible and old-style parameter lists
           have been ruled out, the two parameter lists should be the same
           length. */
        internal_error("composite_routine_type: unequal length param lists");
      }  /* if */
#endif /* CHECKING */
      if (!C_mode()) {
        /* Form the composite of the C++ default argument expressions;
           it's guaranteed that at most one of the parameter lists
           has a default argument expression. */
        if (ptp1->has_default_arg || ptp1->default_arg_expr != NULL) {
          return_type2_as_comp_type = FALSE;
          if (!return_type1_as_comp_type) goto make_new_comp_type;
        } else if (ptp2->has_default_arg ||
                   ptp2->default_arg_expr != NULL) {
          return_type1_as_comp_type = FALSE;
          if (!return_type2_as_comp_type) goto make_new_comp_type;
        }  /* if */
      }  /* if */
      /* Form the composite of the two types. */
      tp = composite_parameter_type(ptp1->type, ptp2->type);
      /* Compare the two parameter types against their composite type.  Stop
         if it is no longer true that one or the other of the original routine
         types can can serve as the composite type. */
      if (!same_entities(tp, ptp1->type)) {
        return_type1_as_comp_type = FALSE;
        if (!return_type2_as_comp_type) goto make_new_comp_type;
      }  /* if */
      if (!same_entities(tp, ptp2->type)) {
        return_type2_as_comp_type = FALSE;
        if (!return_type1_as_comp_type) goto make_new_comp_type;
      }  /* if */
    }  /* for */
  }  /* if */
  if (!C_mode()) {
    if (rtsp1->exception_specification == NULL) {
      if (rtsp2->exception_specification != NULL) {
        return_type1_as_comp_type = FALSE;
        if (!return_type2_as_comp_type) goto make_new_comp_type;
      }  /* if */
    } else {
      if (rtsp2->exception_specification == NULL) {
        return_type2_as_comp_type = FALSE;
        if (!return_type1_as_comp_type) goto make_new_comp_type;
      }  /* if */
    }  /* if */
    /* If the routine-name-linkage of one of the types has been set
       explicitly (e.g., with extern "C"), the other type cannot be used as
       the composite type. */
    if (!rtsp1->routine_name_linkage_is_explicit) {
      if (rtsp2->routine_name_linkage_is_explicit) {
        return_type1_as_comp_type = FALSE;
      }  /* if */
    } else {
      if (!rtsp2->routine_name_linkage_is_explicit) {
        return_type2_as_comp_type = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (return_type1_as_comp_type) {
    /* Nothing prevents returning rout_type1 as the composite type. */
    comp_type = rout_type1;
  } else if (return_type2_as_comp_type) {
    /* rout_type1 can't serve as composite type, but rout_type2 can. */
    comp_type = rout_type2;
  } else {
make_new_comp_type:
    /* Neither of the types passed in can be returned as the composite type,
       so allocate a new type entry and construct a new param-type list. */
    comp_type = alloc_type((a_type_kind)tk_routine);
    comp_type->variant.routine.return_type = comp_return_type;
    rtsp = comp_type->variant.routine.extra_info;
    if (!comp_prototyped) {
      /* Both types have old-style (non-prototyped) interfaces, so there is
         no real parameter information in the composite. However, if one or
         the other has parameter information because it's a function with a
         definition, preserve that information in the composite; but if both
         have parameter lists, the composite type should have a NULL param
         type list. */
      if (param_list1 == NULL) {
        rtsp->param_type_list = param_list2;
      } else if (param_list2 == NULL) {
        rtsp->param_type_list = param_list1;
      }  /* if */
    } else if (!rtsp1->prototyped) {
      /* Simply reuse the param-type list from type2, which is prototyped. */
      rtsp->param_type_list = param_list2;
    } else if (!rtsp2->prototyped) {
      /* Simply reuse the param-type list from type1, which is prototyped. */
      rtsp->param_type_list = param_list1;
    } else {
      ptp1 = param_list1;
      ptp2 = param_list2;
      check_assertion_str((ptp1 == NULL) == (ptp2 == NULL),
                          "composite_routine_type: param lists out of sync");
      end_of_list = NULL;
      while (ptp1 != NULL) {
        /* Pass a NULL source position to make_param_type to avoid
           inappropriate diagnostics on a type that doesn't correspond
           directly to a source construct. */
        new_ptp = make_param_type(composite_parameter_type(ptp1->type,
                                                           ptp2->type),
                                  &null_source_position);
        if (!C_mode()) {
          /* Form the composite of the C++ default argument expressions; it's
             guaranteed that at most one of the parameter lists has a default
             argument expression. */
          if (ptp1->has_default_arg) {
            new_ptp->has_default_arg = TRUE;
            new_ptp->has_unevaluated_template_default =
                                        ptp1->has_unevaluated_template_default;
            if (ptp1->default_arg_expr != NULL) {
              new_ptp->default_arg_expr =
                          duplicate_default_arg_expr(ptp1->default_arg_expr);
            }  /* if */
          } else if (ptp2->has_default_arg) {
            new_ptp->has_default_arg = TRUE;
            new_ptp->has_unevaluated_template_default =
                                        ptp2->has_unevaluated_template_default;
            if (ptp2->default_arg_expr != NULL) {
              new_ptp->default_arg_expr =
                          duplicate_default_arg_expr(ptp2->default_arg_expr);
            }  /* if */
          }  /* if */
          if (ptp1->type_involves_deduced_template_param) {
            check_assertion(ptp2->type_involves_deduced_template_param);
            new_ptp->type_involves_deduced_template_param = TRUE;
          }  /* if */
          if (ptp1->passed_via_copy_constructor) {
            check_assertion(ptp2->passed_via_copy_constructor);
            new_ptp->passed_via_copy_constructor = TRUE;
          }  /* if */
          if (remove_qualifiers_from_param_types) {
            /* Arbitrarily select the qualifiers from one of the types for
               the composite. */
            new_ptp->qualifiers = ptp1->qualifiers;
          }  /* if */
        }  /* if */
        /* Add the parameter type entry to the end of the list. */
        if (rtsp->param_type_list == NULL) {
          rtsp->param_type_list = new_ptp;
        } else {
          end_of_list->next = new_ptp;
        }  /* if */
        end_of_list = new_ptp;
        /* Advance to the next param-type enties.  Both lists will be of
           equal length. */
        ptp1 = ptp1->next;
        ptp2 = ptp2->next;
        check_assertion_str((ptp1 == NULL) == (ptp2 == NULL),
                            "composite_routine_type: param lists out of sync");
      }  /* while */
    }  /* if */
    rtsp->prototyped = comp_prototyped;
    rtsp->has_ellipsis = rtsp1->has_ellipsis;
#if MICROSOFT_EXTENSIONS_ALLOWED
    rtsp->calling_convention = comp_calling_convention;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!C_mode()) {
      /* Set the implicit-this-parameter type. */
      rtsp->this_class = rtsp1->this_class;
      rtsp->qualifiers = rtsp1->qualifiers;
      /* If the two exception specifications are not identical, it is
         because of an error that will already have been reported. */
      if (rtsp1->exception_specification != NULL) {
        rtsp->exception_specification = rtsp1->exception_specification;
      } else {
        rtsp->exception_specification = rtsp2->exception_specification;
      }  /* if */
      /* In C++ mode routine types will be created with a default routine
         name linkage of nlk_cplusplus_external.  If either rout_type1 or
         rout_type2 was explicitly set to a different value, use it
         (giving preference to the rout_type1). */
      if (rtsp1->routine_name_linkage_is_explicit) {
        rtsp->routine_name_linkage = rtsp1->routine_name_linkage;
        rtsp->routine_name_linkage_is_explicit = TRUE;
      } else if (rtsp2->routine_name_linkage_is_explicit) {
        rtsp->routine_name_linkage = rtsp2->routine_name_linkage;
        rtsp->routine_name_linkage_is_explicit = TRUE;
      }  /* if */
      /* Pass a NULL source position to set_routine_calling_method_flag to
         avoid inappropriate diagnostics on a type that doesn't correspond
         directly to a source construct. */
      set_routine_calling_method_flag(comp_type, &null_source_position);
    }  /* if */
  }  /* if */
  return comp_type;
}  /* composite_routine_type */


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
  a_type_ptr comp_type, comp_elem;
  a_type_ptr base_type_1, base_type_2;
  a_type_ptr member_type_1, member_type_2;

  db_enter(5, "composite_type");
  if (same_entities(type_1, type_2)) {
    /* If the types are identical (the most common case), the composite type
       is the same thing. */
    comp_type = type_1;
  } else {
    /* Remove extra typerefs and type qualifiers. */
    base_type_1 = skip_typerefs(type_1);
    base_type_2 = skip_typerefs(type_2);
    if (same_entities(base_type_1, base_type_2)) {
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
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_complex:
        case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
        case tk_template_param:
          /* Template parameter types.  If only one has a proxy class type,
             then it is the composite. */
            { a_template_param_type_supplement_ptr	tptsp_1;
              a_template_param_type_supplement_ptr	tptsp_2;
              tptsp_1 = base_type_1->variant.template_param.extra_info;
              tptsp_2 = base_type_2->variant.template_param.extra_info;
              /* Set the composite to type_1 until we determine otherwise. */
              comp_type = base_type_1;
              if (tptsp_1->class_type == NULL &&
                  tptsp_2->class_type != NULL) {
                /* Only the second type has a proxy class type. */
                comp_type = base_type_2;
              }  /* if */
            }
          break;
        case tk_pointer:
          /* Pointer and reference types.  The composite type is a pointer
	     or reference to the composite of the types pointed to. */
          comp_elem = composite_type(base_type_1->variant.pointer.type,
                                     base_type_2->variant.pointer.type);
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new pointer type. */
          if (same_entities(comp_elem, base_type_1->variant.pointer.type)) {
            comp_type = base_type_1;
          } else if (same_entities(comp_elem,
                                   base_type_2->variant.pointer.type)) {
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
          comp_type = composite_array_type(base_type_1, base_type_2);
          break;
        case tk_routine:
          /* Function types. */
          comp_type = composite_routine_type(base_type_1, base_type_2);
          break;
        case tk_ptr_to_member:
          /* The composite of two pointer-to-member types will point to the
             same class type and to a member type that is a composite of the
             two member types. */
          member_type_1 = pm_member_type(base_type_1);
          member_type_2 = pm_member_type(base_type_2);
          comp_elem = composite_type(member_type_1, member_type_2);
          if (same_entities(comp_elem, member_type_1)) {
            comp_type = base_type_1;
          } else if (same_entities(comp_elem, member_type_2)) {
            comp_type = base_type_2;
          } else {
            comp_type = ptr_to_member_type(comp_elem,
                                           pm_class_type(base_type_1));
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
    if (same_entities(comp_type, base_type_1)) {
      comp_type = type_1;
    } else if (same_entities(comp_type, base_type_2)) {
      comp_type = type_2;
    } else {
      /* Add any missing type qualifiers to the composite type.  Both
         original types must have the same type qualifiers, but the
         qualifiers are not necessarily indicated by the same sequence of
         typerefs. */
      comp_type = type_plus_qualifiers_from_second_type(comp_type, type_1);
    }  /* if */
  }  /* if */
  db_exit();
  return(comp_type);
}  /* composite_type */


a_boolean overload_distinguishable(a_symbol_ptr		old_sym_ptr,
                                   a_type_ptr		new_type,
				   a_template_param_ptr	templ_param_list,
                                   an_error_code	*err_code)
/*
Return TRUE if the new function type new_type is distinguishable under
overload resolution from all the types of the functions indicated by
old_sym_ptr (which might be a simple function or an sk_overloaded_function
symbol).  Otherwise, set *err_code to an appropriate error code
and return FALSE.  We assume that the caller has already determined that
the new type is not compatible with any of the existing types.
The new type may be for a function template (templ_param_list points to
the templates parameter list in that case), as may any of the types on
the old list.  Only callable in C++ mode.  See ARM 13.
*/
{
  a_boolean        distinguishable = TRUE;
  a_boolean        old_is_list, old_is_template;
  a_type_ptr       old_type;
  a_param_type_ptr old_param, new_param;
  a_routine_type_supplement_ptr
                   old_extra_info, new_extra_info;
  a_type_ptr       old_this_class, new_this_class;
  a_type_qualifier_set
                   old_this_qualifiers, new_this_qualifiers;
  a_boolean        old_this_qualified, new_this_qualified;
  a_boolean	   new_is_template = templ_param_list != NULL;

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
  new_this_class = new_extra_info->this_class;
  new_this_qualifiers = new_extra_info->qualifiers;
  do {
    /* Projection symbols are ignored. */
    if (old_sym_ptr->kind == (a_symbol_kind)sk_projection ||
        old_sym_ptr->kind == (a_symbol_kind)sk_namespace_projection) continue;
    /* See if old_sym_ptr and new_type are distinguishable. */
    old_is_template = (old_sym_ptr->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (new_is_template || old_is_template) {
      /* Template and nontemplate functions can always be distinguished.
         Furthermore, template arguments can presumably always be chosen
         to distinguish two template functions. */
      distinguishable = TRUE;
      goto distinguishable_determined;
    } else {
      distinguishable = FALSE;
    /* Get the old routine type. */
      old_type = routine_symbol_type(old_sym_ptr);
    }  /* if */
    old_extra_info = old_type->variant.routine.extra_info;
    /* See if the types are sufficiently different that they are
       distinguishable by overload resolution. */
    /* Note that the code here must match determine_arg_match_level
       and function_template_matches_operand_list. */
    /* See if the "this" parameter is distinguishable if it exists.  If one
       function has a "this" parameter and the other does not, they are not
       distinguishable on that basis alone (except in cfront compatibility
       mode, when a type qualifier on the "this" parameter type makes a
       nonstatic function distinguishable from a static function). */
    old_this_class = old_extra_info->this_class;
    old_this_qualifiers = old_extra_info->qualifiers;
    old_this_qualified = (old_this_qualifiers != TQ_NONE);
    new_this_qualified = (new_this_qualifiers != TQ_NONE);
    if ((old_this_qualified != new_this_qualified && any_cfront_mode()) ||
        (old_this_class != NULL && new_this_class != NULL &&
         (old_this_qualifiers != new_this_qualifiers ||
          !identical_types(old_this_class, new_this_class)))) {
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
    old_param = old_extra_info->param_type_list;
    new_param = new_extra_info->param_type_list;
    for (; old_param != NULL || new_param != NULL;
           old_param = old_param->next, new_param = new_param->next) {
      if (old_param == NULL || new_param == NULL) {
        /* The parameter lists do not end at the same point, so they
           are distinguishable. */
        distinguishable = TRUE;
        goto distinguishable_determined;
      } else {
        /* See if the types are distinguishable.  A parameter containing
           deducible template types is always distinguishable from one
           not containing such types. */
        if ((old_param->type_involves_deduced_template_param !=
                           new_param->type_involves_deduced_template_param) ||
            !f_types_are_compatible(old_param->type, new_param->type,
                                    TCF_NO_FLAGS)) {
          distinguishable = TRUE;
          goto distinguishable_determined;
        }  /* if */
      }  /* if */
    }  /* for */
    /* Falling through to here means the parameter types are all
       indistinguishable. */
    if ((old_this_class == NULL) != (new_this_class == NULL)) {
      /* Except in certain cases in cfront mode, it's an error to overload
         a static and nonstatic member function whose parameter types are
         the same. */
      *err_code = ec_static_nonstatic_with_same_param_types;
    } else {
      /* The parameter types are compatible, so the only incompatibility
         remaining must have to do with the return types. */
#if CHECKING
      if (types_are_strictly_compatible(
                                  old_type->variant.routine.return_type,
                                  new_type->variant.routine.return_type)) {
        /* The caller is supposed to have ensured that the case of
           completely compatible function types does not come here, since
           that's a case of redeclaration rather than overloading. */
        internal_error("overload_distinguishable: types compatible");
      }  /* if */
#endif /* CHECKING */
      /* User is trying to distinguish the function on the basis of the return
         type, which is not valid. */
      *err_code = ec_return_type_cannot_distinguish_functions;
    }  /* if */
distinguishable_determined:;
  } while (distinguishable &&
           old_is_list && (old_sym_ptr = old_sym_ptr->next) != NULL);
  db_exit();
  return distinguishable;
}  /* overload_distinguishable */


static a_boolean ttt_is_local_type(a_type_ptr  type_ptr,
                                   a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a local type
(i.e., a class or enumeration defined within a function or block scope,
including any class or enum defined within a local class).  Note that
typedefs are skipped, as they in name mangling; it is the underlying
type, not the typedef name (which can be declared anywhere) that we really
care about.
*/
{
  a_boolean  is_local = FALSE;

  if (type_ptr->source_corresp.is_local_to_function) {
    check_assertion(type_ptr->kind != (a_type_kind)tk_typeref);
    *force_end_of_traversal = is_local = TRUE;
  }  /* if */
  return is_local;
}  /* ttt_is_local_type */


static a_boolean ttt_is_unnamed_namespace_type(a_type_ptr  type_ptr,
                                   a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a type declared
in an unnamed namespace.
*/
{
  a_boolean  result = FALSE;

  /* Only check types that are namespace members.  When checking something
     like a nested class, traverse_type_tree will check its parents. */
  if (!type_ptr->source_corresp.is_class_member &&
      type_ptr->source_corresp.parent.namespace_ptr != NULL) {
    if (is_member_of_unnamed_namespace(&type_ptr->source_corresp)) {
      *force_end_of_traversal = result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* ttt_is_unnamed_namespace_type */


static a_boolean ttt_is_error_type(a_type_ptr  type_ptr,
                                   a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is an error type
*/
{
  a_boolean  result = FALSE;

  *force_end_of_traversal = result = is_error(type_ptr);
  return result;
}  /* ttt_is_error_type */


/* Static variables used to pass information back to the routine
   is_or_contains_unnamed_or_local_type. */
static a_boolean is_unnamed_type;
static a_boolean is_local_type;
static a_boolean ttt_is_unnamed_or_local_type(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is an unnamed or
local class, struct, union, or enum.  Typedefs will have been skipped, as
they are in name mangling; it is the underlying type, not the typedef name
(which can be declared anywhere) that we really care about.
*/
{
  a_boolean     result = FALSE;

  if (is_class_struct_union(type_ptr) || is_enum(type_ptr)) {
    /* Note two cases that are handled differently:
         typedef struct { ... } S, *P1; // named "S" for linkage purposes
         typedef struct { ... } *P2;    // has no name for linkage purposes
         template <class T> class X { ... };
         X<P1> a;                       // Okay
         X<P2> b;                       // Error
    */
    if (type_ptr->source_corresp.name == NULL) {
#if CHECKING
      a_symbol_ptr  sym  = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      check_assertion_str(sym != NULL && is_unnamed_tag_symbol(sym),
                          "ttt_is_unnamed_or_local_type: bad tag symbol");
#endif /* CHECKING */
      is_unnamed_type = *force_end_of_traversal = result = TRUE;
    }  /* if */
  }  /* if */
  if (type_ptr->source_corresp.is_local_to_function) {
    check_assertion(type_ptr->kind != (a_type_kind)tk_typeref);
    is_local_type = *force_end_of_traversal = result = TRUE;
  }  /* if */
  return result;
}  /* ttt_is_unnamed_or_local_type */


static a_boolean ttt_is_type_with_no_name_linkage(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a typedef to a
type composed from a struct/class/union or enum without a name. Such a
typedef-name has no linkage. Note that typerefs should not be skipped
during the traversal. '*force_end_of_traversal' can be set to true if the
result of the traversal is decided (in this case, when a typedef with no
name linkage is encountered).
*/
{
  a_boolean     result = FALSE;

  if (type_ptr->kind == (a_type_kind)tk_typeref &&
      typeref_is_typedef(type_ptr)) {
    /* First check if this typedef is ultimately built on top of an enum
       or class type. If so, and if that underlying user-defined type has
       no name, then the type name denoted by the typedef has no linkage.
       Note that in 'typedef struct {} X;' the typedef name 'X' is also
       imbued upon the underlying struct (for linkage purposes) and this
       code will not return TRUE (which is desired behavior). */
    a_type_ptr bottom_type = find_bottom_of_type(type_ptr);
    if ((is_class_struct_union(bottom_type) || is_enum(bottom_type)) &&
        bottom_type->source_corresp.name_linkage ==
                                               (a_name_linkage_kind)nlk_none) {
      *force_end_of_traversal = result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* ttt_is_type_with_no_name_linkage */


/* A pointer to the specific template parameter type to be found by
   ttt_is_or_contains_template_param. */
static a_type_ptr
		specific_template_param_type;

/* A pointer to the specific template parameter constant to be found by
   ttt_contains_template_param_constant. */
static a_constant_ptr
		specific_template_param_constant;

/* TRUE if only deduced contexts should be considered by
   ttt_contains_template_param_constant. */
static a_boolean
		deduced_contexts_only;

/* TRUE if nonreal classes should be found in addition to
   template parameters. */
static a_boolean
		find_all_dependent_types;


/* A pointer to the specific template template parameter to be found by
   ttt_contains_specific_template_template_param. */
static a_template_ptr
		specific_template_template_param;


static a_boolean constant_contains_template_param_constant(a_constant_ptr cp)
/*
Helper function for ttt_contains_template_param_constant to determine if a
given constant cp contains a template parameter.  Any parameter will cause
TRUE to be returned, unless specific_template_template_param is non-NULL in
which case that particular parameter must be present.
*/
{
  a_boolean found = FALSE;

  if (cp->kind == (a_constant_repr_kind)ck_template_param) {
    /* Only a ck_template_param constant can be or contain a template
       param constant, and it must. */
    if (specific_template_param_constant == NULL) {
      /* Any template param constant will do. */
      found = TRUE;
    } else if (cp->variant.template_param.kind ==
                            (a_template_param_constant_kind)tpck_expression) {
      /* Look for a particular template param constant in the expression tree.
         This is not done when only deduced contexts are considered because
         template parameters cannot be deduced from expressions. */
      if (!deduced_contexts_only &&
          expr_tree_contains_template_param_constant(
                                      cp->variant.template_param.variant.expr,
                                      specific_template_param_constant)) {
        found = TRUE;
      }  /* if */
    } else if (eq_constants(cp, specific_template_param_constant)) {
      /* Just compare the constant entries. */
      found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* constant_contains_template_param_constant */


static a_boolean ttt_contains_template_param_constant(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  If specific_template_param_constant is NULL, it
returns TRUE if type_ptr is based on a template parameter constant.  If
specific_template_param_type is non-NULL, it returns TRUE if type_ptr is
based on the specified template parameter constant.
*/
{
  an_expr_node_ptr    count;
  a_template_arg_ptr  tap;
  a_boolean           found = FALSE;

  if (is_array(type_ptr)) {
    if (type_ptr->variant.array.is_variable_size_array &&
        !array_is_vla(type_ptr)) {
      count = type_ptr->variant.array.variant.element_count_expr;
      if (expr_tree_contains_template_param_constant(
                                        count,
                                        specific_template_param_constant)) {
        found = TRUE;
      }  /* if */
    } else if (type_ptr->variant.array.is_template_dependent_size_array) {
      found = constant_contains_template_param_constant(
                      type_ptr->variant.array.variant.element_count_constant);
    }  /* if */
  } else if (is_class_struct_union(type_ptr)) {
    /* Examine each template argument, if any. */
    for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                         template_arg_list;
         tap != NULL;
         tap = tap->next) {
      if (is_nontype_templ_arg(tap)) {
        /* A non-type argument.  See if a template parameter constant is
           used -- e.g.,
             template <class T, int I> class A { B<I> *b; . . . };
           where the template argument for B<I> is template para constant I. */
        if (constant_contains_template_param_constant(tap->variant.constant)) {
          found = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* switch */
  if (found) *force_end_of_traversal = TRUE;
  return found;
}  /* ttt_contains_template_param_constant */


static a_boolean ttt_contains_template_template_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
Return TRUE if the type specified by type_ptr is a template class type
with a template argument that is a template template parameter.
*/
{
  a_boolean	found = FALSE;

  if (is_class_struct_union(type_ptr)) {
    /* Check for template template arguments of a template class. */
    a_template_arg_ptr  tap;
    for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                         template_arg_list;
         tap != NULL;
         tap = tap->next) {
      if (is_template_templ_arg(tap)) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = template_supplement_for_template(tap->variant.templ);
        /* Determine whether the template pointed to is a template template
           parameter. */
        if (tssp->variant.class_template.template_template_param) {
          *force_end_of_traversal = found = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (!found) {
      /* Check whether this is a class type that is based on a template
         template parameter. */
      a_symbol_ptr	template_sym;
      template_sym = class_template_for_type(type_ptr);
      if (template_sym != NULL) {
        if (template_sym->variant.template_info->
                              variant.class_template.template_template_param) {
          *force_end_of_traversal = found = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_contains_template_template_param */


static a_boolean ttt_is_or_contains_template_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  If specific_template_param_type is NULL, it
returns TRUE if type_ptr is a template parameter type or is based on a
template parameter constant.  If specific_template_param_type is non-NULL,
it returns TRUE if type_ptr is the specified template parameter type.
If find_all_dependent_types is TRUE, return TRUE for any dependent
types, i.e., also for nonreal classes.
*/
{
  a_boolean  found = FALSE;

  if (is_template_param(type_ptr)) {
    if (specific_template_param_type == NULL ||
        identical_types(type_ptr, specific_template_param_type)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  } else if (find_all_dependent_types &&
             is_immediate_class_type(type_ptr) &&
             type_ptr->variant.class_struct_union.is_nonreal_class) {
    /* A nonreal class is a dependent type. */
    *force_end_of_traversal = found = TRUE;
  } else {
    if (specific_template_param_type == NULL) {
      /* We are not looking for a specific template param type, so any
         template constant (e.g., appearing as an array bound) will also
         serve. */
      found = ttt_contains_template_param_constant(type_ptr,
                                                   force_end_of_traversal);
      if (!found) {
        /* Check for a template template parameter used as a template
           argument. */
        found = ttt_contains_template_template_param(type_ptr, 
                                                     force_end_of_traversal);
      }  /* if */
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_or_contains_template_param */


static a_boolean ttt_contains_specific_template_template_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if the type specified by
type_ptr is an instance of the template template parameter specified
by specific_template_template_param.
*/
{
  a_boolean	found = FALSE;
  a_symbol_ptr	template_sym;

  template_sym = class_template_for_type(type_ptr);
  if (template_sym != NULL) {
    a_template_ptr	templ_ptr;
    templ_ptr = template_sym->variant.template_info->il_template_entry;
    if (equiv_templates(templ_ptr, specific_template_template_param)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  if (!found) {
    if (is_class_struct_union(type_ptr)) {
      /* Check for template template arguments of a template class. */
      a_template_arg_ptr  tap;
      for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                         template_arg_list;
           tap != NULL;
           tap = tap->next) {
        if (is_template_templ_arg(tap)) {
          if (equiv_templates(tap->variant.templ,
                               specific_template_template_param)) {
            *force_end_of_traversal = found = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_contains_specific_template_template_param */


static a_boolean ttt_is_or_contains_deduced_template_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  Returns TRUE if the type specified by type_ptr
is a template parameter, or is based on a template parameter in a context
from which a template parameter value can be deduced.
*/
{
  a_boolean  found = FALSE;

  if (is_template_param(type_ptr)) {
    /* A type parameter -- make sure it is an actual template parameter
       and not something like tptk_member type. */
    if (type_ptr->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_param) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  } else {
    /* We are not looking for a specific template param type, so any
       template constant (e.g., appearing as an array bound) will also
       serve. */
    found = ttt_contains_template_param_constant(type_ptr,
                                                 force_end_of_traversal);
    if (!found) {
      /* Check for a template template parameter used as a template
         argument. */
      found = ttt_contains_template_template_param(type_ptr, 
                                                   force_end_of_traversal);
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_or_contains_deduced_template_param */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

static a_boolean ttt_is_uncompleted_class_type(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if the type specified by type_ptr
is a class type whose definition has been started but has not yet been
completed.
*/
{
  a_scope_ptr  sp;
  a_boolean    found = FALSE;

  if (is_immediate_class_type(type_ptr)) {
    sp = type_ptr->variant.class_struct_union.extra_info->assoc_scope;
    if (sp == NULL) {
      /* Type is undefined -- it is not considered "uncompleted" because its
         definition hasn't yet started. */
    } else if (sp->depth_in_scope_stack != NO_SCOPE_DEPTH) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_uncompleted_class_type */

/* A pointer to the specific class type to be found by
   ttt_is_or_is_member_of_specific_class_type. */
static a_type_ptr
		specific_class_type;

static a_boolean ttt_is_or_is_member_of_specific_class_type
                                        (a_type_ptr  type_ptr,
                                         a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if the type specified by type_ptr
is the same as the type pointed to by specified_class_type or if the latter
is a direct or indirect parent of a type on which type_ptr depends.
*/
{
  a_boolean  found = FALSE;

  if (type_ptr == specific_class_type) {
    *force_end_of_traversal = found = TRUE;
  } else if (type_ptr->source_corresp.is_class_member) {
    type_ptr = type_ptr->source_corresp.parent.class_type;
    if (type_ptr == specific_class_type ||
        type_involves_specific_class_type(type_ptr, specific_class_type,
                                          /*members_only=*/FALSE)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_or_is_member_of_specific_class_type */

    
static a_boolean ttt_is_member_of_specific_class_type
                                        (a_type_ptr  type_ptr,
                                         a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if the type specified by type_ptr
is a member of specified_class_type or if the latter is a direct or indirect
parent of a type on which type_ptr depends.
*/
{
  a_boolean  found = FALSE;

  if (type_ptr->source_corresp.is_class_member) {
    type_ptr = type_ptr->source_corresp.parent.class_type;
    if (type_ptr == specific_class_type ||
        type_involves_specific_class_type(type_ptr, specific_class_type,
                                          /*members_only=*/TRUE)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_member_of_specific_class_type */
    
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static a_boolean ttt_set_force_external_linkage_flag(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It always returns FALSE and always sets
*force_end_of_traversal to FALSE.  If type_ptr is a class or enum type with
internal linkage it sets the force_external_linkage flag in its symbol (or
in that of its top-level parent class).
*/
{
  a_symbol_ptr  sym;

  if (is_immediate_class_type(type_ptr) || is_immediate_enum_type(type_ptr)) {
    if (type_ptr->source_corresp.name_linkage ==
                                        (a_name_linkage_kind)nlk_internal) {
      while (type_ptr->source_corresp.is_class_member) {
        type_ptr = type_ptr->source_corresp.parent.class_type;
      }  /* while */
      sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      sym->force_external_linkage = TRUE;
    }  /* if */
  }  /* if */
  *force_end_of_traversal = FALSE;
  return FALSE;
}  /* ttt_set_force_external_linkage_flag */


/* Static variable initialized to FALSE by caller and used to return status
   information from ttt_is_ptr_or_ref_to_unknown_bound_array. */
static a_boolean type_is_ref_to_unknown_bound_array;

static a_boolean ttt_is_ptr_or_ref_to_unknown_bound_array(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
Return TRUE if type_ptr is a pointer or reference to an array of unknown
(and unspecified) bound.
*/
{
  a_boolean   found = FALSE;
  a_type_ptr  tp;

  if (is_ptr_or_ref_type(type_ptr)) {
    tp = type_pointed_to(type_ptr);
    tp = skip_typerefs(tp);
    if (is_array(tp)) {
      if (!has_unknown_specified_bound(tp) &&
          tp->variant.array.variant.number_of_elements == 0) {
        *force_end_of_traversal = found = TRUE;
        if (is_reference_type(type_ptr)) {
          type_is_ref_to_unknown_bound_array = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_ptr_or_ref_to_unknown_bound_array */


static a_boolean ttt_is_or_contains_vla_with_unspecified_bound(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
Return TRUE if type_ptr is a variable length array type with an unspecified
bound (i.e., one declared with "[*]").
*/
{
  a_boolean   found = FALSE;

  if (is_array(type_ptr)) {
    if (array_is_vla(type_ptr) &&
        !type_ptr->variant.array.has_assoc_vla_dimension) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_is_or_contains_vla_with_unspecified_bound */

#endif /* !STANDALONE_UTILITY_PROGRAM */

static a_boolean ttt_is_variably_modified_type(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
Return TRUE if type_ptr is a VLA or a typedef type that has been marked as
referring to a variably modified type.
*/
{
  a_boolean  found = FALSE;

  if ((is_array(type_ptr) && array_is_vla(type_ptr)) ||
      (type_ptr->kind == (a_type_kind)tk_typeref &&
       type_ptr->variant.typeref.has_variably_modified_type)) {
    *force_end_of_traversal = found = TRUE;
  }  /* if */
  return found;
}  /* ttt_is_variably_modified_type */    


a_boolean traverse_type_tree(a_type_ptr                     type_ptr,
                             a_type_predicate_function_ptr  func,
                             a_type_tree_traversal_flag_set flags)
/*
Traverse the type tree indicated type type_ptr and for each type in the
tree call func, which returns a boolean value.  Terminate the traversal as
soon as TRUE is returned by func, or as soon as func returns a flag forcing
the end of the traversal.  This function returns TRUE if func has returned
TRUE for any type in the tree.  The input parameter "flags" is a bit vector
containing directives about how thoroughly to traverse the tree (e.g., is a
routine type a leaf node, or should the return type be examined? what about
its parameters?).
*/
{
  a_boolean                      force_end_of_traversal = FALSE;
  a_type_ptr                     tp;
  a_template_arg_ptr             tap;
  a_boolean                      status;
  a_routine_type_supplement_ptr  rtsp;
#if STANDALONE_UTILITY_PROGRAM
  /* The global variable "nonstandard_qualified_deduction" doesn't exist
     in standalone programs.  This should only affect calls to this routine
     from is_or_contains_template_param, which is not used in standalone
     utility programs. */
  a_boolean			nonstandard_qualifier_deduction = FALSE;
#endif /* STANDALONE_UTILITY_PROGRAM */

  if (type_ptr == NULL) {
    /* If a NULL pointer was passed in, simply return FALSE. */
    status = FALSE;
    goto done;
  }  /* if */
  if (type_ptr->kind == (a_type_kind)tk_typeref) {
    if (flags & TTT_SKIP_TYPEREFS) {
      if (flags & TTT_STOP_AT_TYPEDEFS) {
        /* If we're asked to skip typerefs but stop at typedefs, skip over
           only non-typedefs here. */
        type_ptr = skip_typerefs_not_typedefs(type_ptr);
      } else {
        type_ptr = f_skip_typerefs(type_ptr);
      }  /* if */
    } else if (flags & TTT_SKIP_TYPEDEFS) {
      type_ptr = skip_typedefs(type_ptr);
    }  /* if */
    if ((flags & TTT_STOP_AT_TYPEDEFS) &&
        type_ptr->kind == (a_type_kind)tk_typeref &&
        typeref_is_typedef(type_ptr)) {
      force_end_of_traversal = TRUE;
    }  /* if */
  }  /* if */
  if (!force_end_of_traversal) {
    status = func(type_ptr, &force_end_of_traversal);
  } else {
    status = FALSE;
  }  /* if */
  if (force_end_of_traversal) {
    /* The function has determined that no further traversal is appropriate;
       return the current status to the caller. */
  } else {
    /* Traverse the tree. */
    switch (type_ptr->kind) {
      case tk_error:
      case tk_void:
      case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
      case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tk_unknown:
        /* Leaf nodes -- no further traversal required. */
        break;
      case tk_integer:
	/* Integer type -- if this is an enumeration we need to check
	   enclosing classes. */
        if (type_ptr->variant.integer.enum_type &&
	    type_ptr->source_corresp.is_class_member) {
	  goto check_enclosing_classes;
        }  /* if */
	break;
      case tk_pointer:
        tp = type_ptr->variant.pointer.type;
        status = traverse_type_tree(tp, func, flags);
        break;
      case tk_routine:
        /* Conditional traversal of contained types. */
        rtsp = type_ptr->variant.routine.extra_info;
        if (flags & TTT_RETURN_TYPE) {
          tp = type_ptr->variant.routine.return_type;
          if (traverse_type_tree(tp, func, flags)) {
            status = TRUE;
            break;
          }  /* if */
        }  /* if */
        if (flags & TTT_PARAM_TYPES) {
          a_param_type_ptr  ptp;
          for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
            tp = ptp->type;
            if (traverse_type_tree(tp, func, flags)) {
              status = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
        if (!C_mode()) {
          if (flags & TTT_THIS_PARAM_TYPE) {
            tp = rtsp->this_class;
            if (tp != NULL && traverse_type_tree(tp, func, flags)) {
              status = TRUE;
              break;
            }  /* if */
          }  /* if */
          if ((flags & TTT_EXCEPTION_SPECS) &&
              rtsp->exception_specification != NULL) {
            an_exception_specification_type_ptr  estp;
            for (estp = rtsp->exception_specification->
                                 exception_specification_type_list;
                 estp != NULL;
                 estp = estp->next) {
              tp = estp->type;
              if (traverse_type_tree(tp, func, flags)) {
                status = TRUE;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
        break;
      case tk_array:
        tp = type_ptr->variant.array.element_type;
        if (tp != NULL) status = traverse_type_tree(tp, func, flags);
        break;
      case tk_typeref:
        tp = type_ptr->variant.typeref.type;
        status = traverse_type_tree(tp, func, flags);
	break;
      case tk_template_param:
        /* "Member" template params (e.g., T::X) should have a pointer to a
           parent class. */
        check_assertion((a_boolean)type_ptr->source_corresp.is_class_member ==
                        (type_ptr->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_member));
        if ((!(flags & TTT_DEDUCED_CONTEXTS_ONLY) ||
             nonstandard_qualifier_deduction) &&
            type_ptr->source_corresp.is_class_member) {
          /* Check the template parameter associated with the proxy class that
             is the parent class.  This is only checked when considering
             nondeduced contexts, or when this is a deduced context when
             nonstandard deduction is enabled. */
          tp = type_ptr->source_corresp.parent.class_type;
          tp = symbol_supplement_for_class(tp)->template_param_for_proxy_class;
          if (tp != NULL) {
            if (traverse_type_tree(tp, func, flags)) {
              status = TRUE;
              break;
            }  /* if */
          }  /* if */
          /* For cases where the template parameter is a member of another
             class, check the enclosing classes too.  This is used for
             cases like "template <class T> void f(A<T>::N)". */
          goto check_enclosing_classes;
        }  /* if */
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Any code for the general class/struct/union case should go
	   here. */
        if (!C_mode()) {
          /* If this class is a proxy class, traverse its associated
             template parameter. */
          if (in_front_end) {
            a_symbol_ptr			class_sym;
            a_class_symbol_supplement_ptr	cssp;
            class_sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
            if (class_sym != NULL) {
              cssp = class_sym->variant.class_struct_union.extra_info;
              tp = cssp->template_param_for_proxy_class;
              if (tp != NULL) {
                if (traverse_type_tree(tp, func, flags)) {
                  status = TRUE;
                  break;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
          /* Conditional traversal of contained types. */
          if (flags & TTT_TEMPLATE_ARGS) {
            for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                          template_arg_list;
                 tap != NULL;
                 tap = tap->next) {
              if (is_type_templ_arg(tap)) {
                tp = tap->variant.type;
                if (traverse_type_tree(tp, func, flags)) {
                  status = TRUE;
                  break;
                }  /* if */
              } else if (is_template_templ_arg(tap)) {
                /* Template template arguments are not themselves processed,
                   but their parent type may be. */
                a_template_ptr	templ_ptr = tap->variant.templ;
                if ((!(flags & TTT_DEDUCED_CONTEXTS_ONLY) ||
                     nonstandard_qualifier_deduction) &&
                    !status && templ_ptr->source_corresp.is_class_member) {
                  /* Check the parent class.  This is only done when
                     considering nondeduced contexts, or when this is a
                     deduced context when nonstandard deduction is enabled. */
                  tp = templ_ptr->source_corresp.parent.class_type;
                  status = traverse_type_tree(tp, func, flags);
                }  /* if */      
              } else if (!tap->is_array_bound_of_unknown_type &&
                         tap->variant.constant != NULL) {
                /* Nontype template argument.  Check the type of the
                   constant. */
                if (!(flags & TTT_DEDUCED_CONTEXTS_ONLY)) {
                  tp = tap->variant.constant->type;
                  if (traverse_type_tree(tp, func, flags)) {
                    status = TRUE;
                    break;
                  }  /* if */
                }  /* if */
              }  /* if */
            }  /* for */
          }  /* if */
          if (!status && type_ptr->source_corresp.is_class_member) {
            /* If this class is a member of a proxy class, traverse the type
               of the template parameter with which the proxy class is
               associated. */
            tp = type_ptr->source_corresp.parent.class_type;
            check_assertion(in_front_end &&
                            tp->source_corresp.assoc_info != NULL);
            tp = symbol_supplement_for_class(tp)
                                             ->template_param_for_proxy_class;
            if (tp != NULL) {
              if (traverse_type_tree(tp, func, flags)) {
                status = TRUE;
              }  /* if */
              break;
            }  /* if */
          }  /* if */
check_enclosing_classes:
          if ((!(flags & TTT_DEDUCED_CONTEXTS_ONLY) ||
               nonstandard_qualifier_deduction) &&
              !status && type_ptr->source_corresp.is_class_member) {
            /* Check the parent class.  This is only done when considering
               nondeduced contexts, or when this is a deduced context when
               nonstandard deduction is enabled. */
            tp = type_ptr->source_corresp.parent.class_type;
            status = traverse_type_tree(tp, func, flags);
          }  /* if */      
        }  /* if */
        break;
      case tk_ptr_to_member:
        tp = type_ptr->variant.ptr_to_member.class_of_which_a_member;
        status = traverse_type_tree(tp, func, flags);
        if (!status) {
          tp = type_ptr->variant.ptr_to_member.type;
          status = traverse_type_tree(tp, func, flags);
        }  /* if */
        break;
#if CHECKING
      default:
        internal_error("traverse_type_tree: bad type kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
done:
  return status;
}  /* traverse_type_tree */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_or_contains_local_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a local class, struct,
union or enum type or is a type tree containing such a type.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_EXCEPTION_SPECS);

  return (traverse_type_tree(type_ptr, ttt_is_local_type, ttt_flags));
}  /* is_or_contains_local_type */


a_boolean is_or_contains_unnamed_namespace_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a type declared in
an unnamed namespace, or is a type tree containing such a type.
*/
{
  a_boolean	result;

  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_EXCEPTION_SPECS);

  result = (traverse_type_tree(type_ptr, ttt_is_unnamed_namespace_type,
                                ttt_flags));
  return result;
}  /* is_or_contains_unnamed_namespace_type */


a_boolean is_or_contains_unnamed_or_local_type(a_type_ptr  type_ptr,
					       a_boolean   *is_unnamed,
					       a_boolean   *is_local)
/*
Return TRUE if the type pointed to by type_ptr is itself a unnamed or
local class, struct, union or enum type or is a type tree containing such a
type.  If the result of the traverse_type_tree call is TRUE then the
static variables is_unnamed_type and is_local_type are set to reflect
which of the conditions is true.
*/
{
  a_boolean			  result;
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_EXCEPTION_SPECS);

  /* Clear the variables that are used to return status information
     from ttt_is_unnamed_or_local_type. */
  is_local_type = FALSE;
  is_unnamed_type = FALSE;
  result = traverse_type_tree(type_ptr, ttt_is_unnamed_or_local_type,
                              ttt_flags);
  *is_unnamed = is_unnamed_type;
  *is_local = is_local_type;
  return result;
}  /* is_or_contains_unnamed_or_local_type */


a_boolean contains_type_with_no_name_linkage(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr contains a class, struct,
union or enum type with no name linkage.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_EXCEPTION_SPECS);
  /* Note that TTT_SKIP_TYPEREFS is not set. */
  return (traverse_type_tree(type_ptr, ttt_is_type_with_no_name_linkage,
                             ttt_flags));
}  /* contains_type_with_no_name_linkage */


a_boolean is_or_contains_error_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a error type
or is a type tree containing such a type.
*/
{
  a_boolean			  result;
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_EXCEPTION_SPECS);

  result = traverse_type_tree(type_ptr, ttt_is_error_type, ttt_flags);
  return result;
}  /* is_or_contains_error_type */


a_boolean is_template_dependent_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is template-dependent, i.e.,
it is or contains a tk_template_param type entry or a nonreal class.
*/
{
  a_boolean result = FALSE;

  /* Template parameter types come up only in C++ mode. */
  if (!C_mode()) {
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                                 TTT_THIS_PARAM_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_TEMPLATE_ARGS);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = TRUE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
  }  /* if */
  return result;
}  /* is_template_dependent_type */


a_boolean is_or_contains_template_param(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a tk_template_param
type entry or is a type tree containing such a type or a type containing
a template parameter constant.
*/
{
  a_boolean result = FALSE;

  /* Template parameter types come up only in C++ mode. */
  if (!C_mode()) {
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_TEMPLATE_ARGS);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = FALSE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
  }  /* if */
  return result;
}  /* is_or_contains_template_param */


static a_boolean is_or_contains_deduced_template_param(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a tk_template_param
type entry or is a type tree containing such a type, or a type containing
a template parameter constant, in a context in which the template
parameter can be deduced.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
					       TTT_DEDUCED_CONTEXTS_ONLY |
                                               TTT_TEMPLATE_ARGS);

  check_assertion_str(!C_mode(),
              "is_or_contains_deduced_template_param: not callable in C mode");
  /* Setting these pointers to NULL indicates that any template param type
     or constant will do. */
  specific_template_param_type = NULL;
  specific_template_param_constant = NULL;
  deduced_contexts_only = TRUE;
  find_all_dependent_types = FALSE;
  return (traverse_type_tree(type_ptr,
                             ttt_is_or_contains_deduced_template_param,
                             ttt_flags));
}  /* is_or_contains_deduced_template_param */


void set_type_involves_deduced_template_param(a_type_ptr  rout_type)
/*
Go through the parameters for rout_type, which is assumed to be a
function type.  If any of the associated types involves a template
parameter in a context in which the parameter can be deduced, mark the
param type entry; this is useful for function arg matching.
Nondeduced contexts are the parent classes of a type (e.g., ignore the
T in A<T>::B) and nontype template parameters used in expression
contexts.
*/
{
  a_param_type_ptr  ptp;

  check_assertion(is_function_type(rout_type));
  ptp = skip_typerefs(rout_type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    ptp->type_involves_deduced_template_param = 
                              is_or_contains_deduced_template_param(ptp->type);
  }  /* for */
}  /* set_type_involves_deduced_template_param */


a_boolean is_or_contains_specific_template_param(a_type_ptr  type_ptr,
						 a_type_ptr  tparam_type)
/*
Return TRUE if the type pointed to by type_ptr is itself the specific
template parameter specified by tparam_type or is a type tree
containing such a reference to the type.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  /* This indicates that only a specific template parameter may be found. */
  specific_template_param_type = tparam_type;
  specific_template_param_constant = NULL;
  deduced_contexts_only = FALSE;
  find_all_dependent_types = FALSE;
  return (traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
          ttt_flags));
}  /* is_or_contains_specific_template_param */


a_boolean type_contains_specific_template_template_param(
					a_type_ptr	type_ptr,
					a_template_ptr	tparam_template)
/*
Return TRUE if the type tree pointed to by type_ptr contains a type
that is an instance of the template template parameter specified
by tparam_template.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  specific_template_template_param = tparam_template;
  return (traverse_type_tree(type_ptr,
          ttt_contains_specific_template_template_param,
          ttt_flags));
}  /* type_contains_specific_template_template_param */


a_boolean type_contains_specific_template_param_constant(a_type_ptr     tp,
                                                         a_constant_ptr cp)
/*
Return TRUE if the template parameter constant pointed to by cp is involved
in the type tree represented by tp.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  /* This indicates that only a specific template param constant may be
     found. */
  specific_template_param_constant = cp;
  specific_template_param_type = NULL;
  deduced_contexts_only = FALSE;
  find_all_dependent_types = FALSE;
  return (traverse_type_tree(tp, ttt_contains_template_param_constant,
                             ttt_flags));
}  /* type_contains_specific_template_param_constant */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS

a_boolean is_or_contains_member_of_uncompleted_class(a_type_ptr  tp)
/*
Return TRUE if the specified type is or is dependent on a class or a member
of a class whose definition has begun but has not yet been completed.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_SKIP_TYPEDEFS |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  ttt_flags |= TTT_RETURN_TYPE | TTT_THIS_PARAM_TYPE | TTT_EXCEPTION_SPECS;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  tp = skip_typerefs(tp);
  return (traverse_type_tree(tp, ttt_is_uncompleted_class_type, ttt_flags));
}  /* is_or_contains_member_of_uncompleted_class */


a_boolean template_args_involve_specific_class_type(
                                             a_template_arg_ptr  tap,
                                             a_type_ptr          class_type,
                                             a_boolean           members_only)
/*
When members_only is TRUE, return TRUE if any type in the template argument
list pointed to by tap depends on a member of class_type.  When members_only
is FALSE, return TRUE if any type in the template argument list depends on
class_type itself.
*/
{
  a_boolean                       result = FALSE;

  for (; tap != NULL; tap = tap->next) {
    if (is_type_templ_arg(tap)) {
      if (type_involves_specific_class_type(tap->variant.type, class_type,
                                            members_only)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* template_args_involve_specific_class_type */


a_boolean type_involves_specific_class_type(a_type_ptr  tp,
                                            a_type_ptr  class_type,
                                            a_boolean   members_only)
/*
When members_only is TRUE, return TRUE if a class that is a member of
class_type appears anywhere in the type tree specified by tp.  When
members_only is FALSE, return TRUE if class_type itself appears in tp.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_SKIP_TYPEDEFS |
                                               TTT_TEMPLATE_ARGS);
  a_type_predicate_function_ptr   func;

#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  ttt_flags |= (TTT_RETURN_TYPE | TTT_PARAM_TYPES |
                TTT_THIS_PARAM_TYPE | TTT_EXCEPTION_SPECS);
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  tp = skip_typerefs(tp);
  specific_class_type = class_type;
  func = members_only ? ttt_is_member_of_specific_class_type :
                        ttt_is_or_is_member_of_specific_class_type;
  return (traverse_type_tree(tp, func, ttt_flags));
}  /* type_involves_specific_class_type */

#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void set_force_external_linkage_flag(a_type_ptr  type_ptr)
/*
Set the force_external_linkage flag in the symbol associated with any
class or enum type contained in type_ptr.
*/
{
  /* This processing is needed only in cfront mode.  In other modes,
     the linkage of a class or enum is unaffected by how the class or
     enum is used. */
  if (any_cfront_mode()) {
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_SKIP_TYPEDEFS |
                                                 TTT_RETURN_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_THIS_PARAM_TYPE);

    (void)traverse_type_tree(type_ptr, ttt_set_force_external_linkage_flag,
                             ttt_flags);
  }  /* if */
}  /* set_force_external_linkage_flag */


void set_used_in_exception_or_rtti_flag(a_type_ptr  type_ptr)
/*
Set the used_in_exception_or_rtti flag in type_ptr to indicate that it
has been used in an exception handling or RTTI construct.
*/
{
  if (type_ptr->used_in_exception_or_rtti) {
    /* Already set.  No further action is required. */
  } else {
    a_type_ptr eff_type = type_ptr;
    type_ptr->used_in_exception_or_rtti = TRUE;
    /* Force generation of the typeinfo for any underlying class. */
    while (is_ptr_or_ref_type(eff_type)) {
      eff_type = type_pointed_to(eff_type);
    }  /* while */
    if (is_class_struct_union_type(eff_type)) {
      require_definitions_of_virtual_functions_in_class(eff_type);
    }  /* if */
    /* Add the type to the nontag_types_used_in_exception_or_rtti list,
       unless it will be on another list. */
    if (!has_name(type_ptr) &&
        !is_immediate_class_type(type_ptr) &&
        !is_immediate_enum_type(type_ptr) &&
        /* Do not put types from prototype instantiations on the list. */
        !is_template_dependent_type(type_ptr)) {
      an_il_header *p_il_header = &il_header;
      check_assertion(type_ptr->next == NULL);
      if (!is_primary_translation_unit && !in_secondary_trans_unit(type_ptr)) {
        /* Force a primary-IL type into the primary IL list. */
        p_il_header = &translation_units->il_header;
      } else if (is_primary_translation_unit &&
                 in_secondary_trans_unit(type_ptr)) {
        /* Force a secondary-IL type into an arbitrary secondary IL list. */
        p_il_header = &translation_units->next->il_header;
      }  /* if */
      type_ptr->next = p_il_header->nontag_types_used_in_exception_or_rtti;
      p_il_header->nontag_types_used_in_exception_or_rtti = type_ptr;
    }  /* if */
    set_force_external_linkage_flag(type_ptr);
  }  /* if */
}  /* set_used_in_exception_or_rtti_flag */


a_boolean is_or_contains_ptr_or_ref_to_unknown_bound_array(a_type_ptr tp,
                                                           a_boolean  *is_ref)
/*
Return TRUE if tp is or contains a pointer or reference to an array of
unknown bound.  If a reference to such an array is found, return *is_ref
TRUE; otherwise, return *is_ref FALSE.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_TEMPLATE_ARGS);
  a_boolean                       result = FALSE;

  /* Set the initial value for the static variable that may be set by
     ttt_is_ptr_or_ref_to_unknown_bound_array. */
  type_is_ref_to_unknown_bound_array = FALSE;
  result = traverse_type_tree(tp, ttt_is_ptr_or_ref_to_unknown_bound_array,
                              ttt_flags);
  *is_ref = type_is_ref_to_unknown_bound_array;
  return result;
}  /* is_or_contains_ptr_or_ref_to_unknown_bound_array */


a_boolean is_or_contains_vla_type_with_unspecified_bound(a_type_ptr  tp)
/*
Return TRUE is tp is or contains a variable length array type with an
unspecified bound (i.e., declared with [*]).
*/
{
  a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                              TTT_SKIP_TYPEREFS);
  a_boolean                       result = FALSE;

  if (vla_enabled) {
    result = traverse_type_tree(tp,
                                ttt_is_or_contains_vla_with_unspecified_bound,
                                tt_flags);
  }  /* if */
  return result;
}  /* is_or_contains_vla_type_with_unspecified_bound */


a_boolean is_variably_modified_type(a_type_ptr  tp)
/*
Return TRUE if tp is a "variably modified type", which includes VLA types,
pointers to VLA types, arrays whose element types are variably modified, and
typedefs referring to variably modified types.
*/
{
  a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE);
  a_boolean                       result = FALSE;

  if (vla_enabled) {
    result = traverse_type_tree(tp, ttt_is_variably_modified_type, tt_flags);
  }  /* if */
  return result;
}  /* is_variably_modified_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean could_be_dependent_class_type(a_type_ptr tp)
/*
Return TRUE if the given type is dependent and might be a class type when
instantiated (for a template parameter "T", this includes types such as "T",
"T::X" and "C<T>").
*/
{
  tp = skip_typerefs(tp);
  return is_template_param(tp) ||
         (is_class_struct_union(tp) &&
          tp->variant.class_struct_union.is_nonreal_class);
}  /* could_be_dependent_class_type */


a_boolean is_directly_variably_modified_type(a_type_ptr  tp)
/*
Return TRUE if tp is a "variably modified type" in which the variable
array bound appears directly (rather than hidden under a typedef).
*/
{
  a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                              TTT_STOP_AT_TYPEDEFS);
  a_boolean                       result = FALSE;
#if STANDALONE_UTILITY_PROGRAM
  /* The global variable "vla_enabled" doesn't exist in standalone
     programs. */
  a_boolean                       vla_enabled = VLA_ALLOWED;
#endif /* STANDALONE_UTILITY_PROGRAM */

  if (vla_enabled) {
    result = traverse_type_tree(tp, ttt_is_variably_modified_type, tt_flags);
  }  /* if */
  return result;
}  /* is_directly_variably_modified_type */

#if !STANDALONE_UTILITY_PROGRAM

/* Type of service function called by traverse_and_modify_type_tree to return
   TRUE if the type was modified or FALSE if it was not. */
typedef a_boolean a_type_modifier_function(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type);
typedef a_type_modifier_function *a_type_modifier_function_ptr;


/*ARGSUSED*/ /* flags is not required but is part of the general interface. */
static a_boolean tmtt_strip_local_and_nonreal_typedefs(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type)
/*
Strip local and nonreal typedef from type, returning TRUE if a modification
was done. The modified type (or the original type if no modification was done)
is returned in *new_type.
*/
{
  *new_type = strip_local_and_nonreal_typedefs(type);
  return !same_entities(type, *new_type);
}  /* tmtt_strip_local_and_nonreal_typedefs */


/*ARGSUSED*/ /* flags is not required but is part of the general interface. */
static a_boolean tmtt_remove_assoc_vla_dimensions(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type)
/*
Modify type so that it no longer is or contains array types with associated
VLA dimension entries.  The modified type (or the original type if no
modification was done) is returned in *new_type.
*/
{
  *new_type = remove_assoc_vla_dimensions(type);
  return !same_entities(type, *new_type);
}  /* tmtt_remove_assoc_vla_dimensions */


static a_type_ptr traverse_and_modify_type_tree(
                                         a_type_ptr                     type,
                                         a_type_modifier_function_ptr   func,
                                         a_type_tree_traversal_flag_set flags)
/*
Traverse the type tree represented by type and at each level call func to
perform optional modification of the subtree.  If the subtree is modified,
a new tree is built.
*/
{
  a_type_ptr              new_type = type;
  a_type_ptr              tp, tp2;
  a_param_type_ptr        ptp, new_ptp, prev_ptp;
  a_type_ptr              new_return_type, new_this_class;
  a_type_ptr              first_new_type_for_param_types_list;
  unsigned long           reusable_param_types;

  /* Traverse the tree. */
  switch (type->kind) {
    case tk_error:
    case tk_void:
    case tk_float:
    case tk_integer:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_unknown:
      /* Leaf nodes -- no further traversal required. */
      break;
    case tk_pointer:
      /* If the type pointed to is modified, then a new pointer or
         reference type must be created. */
      if (func(type->variant.pointer.type, flags, &tp)) {
        if (type->variant.pointer.is_reference) {
          new_type = make_reference_type(tp);
        } else {
          new_type = make_pointer_type(tp);
        }  /* if */
      }  /* if */
      break;
    case tk_routine:
      /* We can reuse "type" as long as we can reuse the return type and all
         its param types.  Otherwise we will need to allocate a new type
         entry. Go through "type" until we find that a new type was returned
         from the type-modification function. */
      reusable_param_types = 0;
      first_new_type_for_param_types_list = NULL;
      new_return_type = type->variant.routine.return_type;
      if (func(new_return_type, flags, &tp)) {
        new_return_type = tp;
      }  /* if */
      new_this_class = type->variant.routine.extra_info->this_class;
      if (new_this_class != NULL && func(new_this_class, flags, &tp)) {
        new_this_class = tp->variant.routine.extra_info->this_class;
        goto make_new_type;
      } else if (!same_entities(new_return_type,
                                type->variant.routine.return_type)) {
        goto make_new_type;
      }  /* if */
      /* Now examine each of the parameters. */
      for (ptp = type->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        if (func(ptp->type, flags, &tp)) {
          /* A modification was made, so a new routine type will be required.
             Remember tp so we can avoid calling the type modification
             function again for this param type entry. */
          first_new_type_for_param_types_list = tp;
          goto make_new_type;
        }  /* if */
        /* Keep track of the number of param type entries for which reuse of
           the existing type is okay. */
        ++reusable_param_types;
      }  /* for */
      /* Falling through to here means that no changes are required for this
         type.  Therefore it can simply be reused. */
      break;
make_new_type:
      /* Make a routine type based on "type".  Checking for reusable types
         has already been done for the return type and possibly for some of
         the parameter types. */
      new_type = alloc_type((a_type_kind)tk_routine);
      /* Fill in the return type.  It has already been determined. */
      new_type->variant.routine.return_type = new_return_type;
      /* Clone the routine type supplement, except for the pointers. */
      *(new_type->variant.routine.extra_info) =
                                       *(type->variant.routine.extra_info);
      new_type->variant.routine.extra_info->assoc_routine = NULL;
      new_type->variant.routine.extra_info->this_class = new_this_class;
      /* Make copies of the entries on type's param types list, making the
         appropriate modifications. */
      prev_ptp = NULL;
      for (ptp = type->variant.routine.extra_info->param_type_list;
           ptp != NULL;
           ptp = ptp->next) {
        if (reusable_param_types > 0) {
          /* We have already called the modification routine for this parameter
             and we know we can reuse the existing type. */
          tp = ptp->type;
          --reusable_param_types;
        } else if (first_new_type_for_param_types_list != NULL) {
          /* We have already called the modification routine for this param
             and the type returned contained a substitution; we can use that
             type. */
          tp = first_new_type_for_param_types_list;
          first_new_type_for_param_types_list = NULL;
        } else {
          tp = ptp->type;
          (void)func(ptp->type, flags, &tp);
        }  /* if */
        /* Allocate the param type entry and copy default arg info. */
        /* Pass a NULL source position to make_param_type to avoid
           inappropriate diagnostics on a type that doesn't correspond
           directly to a source construct. */
        new_ptp = make_param_type(tp, &null_source_position);
        if (ptp->has_default_arg) {
          new_ptp->has_default_arg = TRUE;
          new_ptp->has_unevaluated_template_default =
                                         ptp->has_unevaluated_template_default;
          if (ptp->default_arg_expr != NULL) {
            new_ptp->default_arg_expr =
                             duplicate_default_arg_expr(ptp->default_arg_expr);
          }  /* if */
        }  /* if */
        /* Recompute the value of the flag, if necessary. */
        new_ptp->type_involves_deduced_template_param =
                     (same_entities(ptp->type, tp) ?
                        ptp->type_involves_deduced_template_param :
                        is_or_contains_deduced_template_param(new_ptp->type));
        /* Add the new param type entry to the param types list. */
        if (prev_ptp == NULL) {
          new_type->variant.routine.extra_info->param_type_list = new_ptp;
        } else {
          prev_ptp->next = new_ptp;
        }  /* if */
        prev_ptp = new_ptp;
      }  /* if */
      /* Pass a NULL source position to set_routine_calling_method_flag to
         avoid inappropriate diagnostics on a type that doesn't correspond
         directly to a source construct. */
      set_routine_calling_method_flag(new_type, &null_source_position);
      break;
    case tk_array:
      /* Make an array type based on "type", making modifications as
         required in the element type.  Note that if the element type doesn't
         require modification, we don't create a new type entry. */
      if (func(type->variant.array.element_type, flags, &tp)) {
        /* Create a new array type. */
        new_type = alloc_type((a_type_kind)tk_array);
        copy_type(type, new_type);
        new_type->variant.array.element_type = tp;
      }  /* if */
      break;
    case tk_typeref:
      if (func(type->variant.typeref.type, flags, &tp)) {
        new_type = type_plus_qualifiers_from_second_type(tp, type);
      }  /* if */
      break;
    case tk_template_param:
      /* tptk_member template param types point to a parent type.  However,
         since class types are not themselves modified by this routine, do
         nothing for such cases. */
    case tk_class:
    case tk_struct:
    case tk_union:
      /* No action required. */
      break;
    case tk_ptr_to_member:
      (void)func(type->variant.ptr_to_member.type, flags, &tp);
      (void)func(type->variant.ptr_to_member.class_of_which_a_member, flags,
                 &tp2);
      if (!same_entities(tp, type->variant.ptr_to_member.type) ||
          !same_entities(
                   tp2, type->variant.ptr_to_member.class_of_which_a_member)) {
        /* Make a pointer-to-member type.  The current pointer-to-member type
           points to two types, so the new type is based on modified versions
           of one or both. */
        new_type = ptr_to_member_type(tp, tp2);
      }  /* if */
      break;
#if CHECKING
    default:
      internal_error("traverse_and_modify_type_tree: bad type kind");
#endif /* CHECKING */
  }  /* switch */
  return new_type;
}  /* traverse_and_modify_type_tree */


a_type_ptr strip_local_and_nonreal_typedefs(a_type_ptr  type)
/*
If type contains one or more typedefs that are local to a function, or that
were defined in a prototype instantiation (whether at the top level or
embedded somewhere within the tree) remove them and return the modified type
to the caller.  If no modification is done return the original type.
*/
{
  while (type->kind == (a_type_kind)tk_typeref) {
    a_boolean  is_nonreal = FALSE;
    a_boolean  is_local;
    /* See if the type is local to a function. */
    is_local = type->source_corresp.is_local_to_function;
    if (!is_local) {
      /* See if the type was defined in a prototype instantiation. */
      if (type->source_corresp.is_class_member) {
        a_symbol_ptr cowam_sym;
        cowam_sym = (a_symbol_ptr)type->source_corresp.parent.class_type->
                                                  source_corresp.assoc_info;
        check_assertion(in_front_end && cowam_sym != NULL);
        is_nonreal = !is_real_class_symbol(cowam_sym);
      }  /* if */
    }  /* if */
    /* Only continue processing this typedef if it is either a local typedef
       or defined in a prototype instantiation. */
    if (!is_local && !is_nonreal) break;
    /* The top level type is a local typedef. */
    check_assertion(!typeref_is_qualified(type));
    type = type->variant.typeref.type;
  }  /* while */
  return traverse_and_modify_type_tree(type,
				       tmtt_strip_local_and_nonreal_typedefs,
                                       TTT_NO_INPUT_FLAGS);
}  /* strip_local_and_nonreal_typedefs */


a_type_ptr remove_assoc_vla_dimensions(a_type_ptr  type)
/*
If type contains one or more arrays with associated VLA dimension entries,
return a new type without such arrays.  Traverse the type tree and turn VLAs
with associated VLA dimension entries into VLAs with unspecified bounds --
i.e., rewrite them as though they had been declared with [*].
*/
{
  a_type_ptr  new_type;

  if (is_array(type) && type->variant.array.has_assoc_vla_dimension) {
    /* This type has an associated VLA dimension entry.  Clone the array
       type but without the has_assoc_vla_dimension flag set.  Also apply
       this routine recursively to its element type. */
    new_type = alloc_type((a_type_kind)tk_array);
    new_type->variant.array.element_type =
            remove_assoc_vla_dimensions(type->variant.array.element_type);
    new_type->variant.array.is_variable_size_array = TRUE;
    new_type->variant.array.is_vla = TRUE;
    set_type_size(new_type);
  } else {
    /* The type is not an array type, so traverse the type tree looking
       for embedded array types with has_assoc_vla_dimension set to TRUE. */
    new_type = traverse_and_modify_type_tree(type,
                                             tmtt_remove_assoc_vla_dimensions,
                                             TTT_NO_INPUT_FLAGS);
  }  /* if */
  return new_type;
}  /* remove_assoc_vla_dimensions */

#if MICROSOFT_EXTENSIONS_ALLOWED
char *uuid_string_of_type(a_type_ptr  type)
/*
Return the uuid specification of a class or enum type.  If the given type is
not a class or enum type, return NULL.
*/
{
  char  *result;

  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    result = type->variant.class_struct_union.extra_info->uuid_string;
  } else if (is_immediate_enum_type(type)) {
    result = type->variant.integer.uuid_string;
  } else {
    result = NULL;
  }  /* if */
  return result;
}  /* uuid_string_of_type */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* !STANDALONE_UTILITY_PROGRAM */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
