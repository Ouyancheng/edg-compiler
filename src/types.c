/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
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
#include "lookup.h"
#if DO_IL_LOWERING
#include "lower_il.h"
#include "lower_c99.h"
#endif /* DO_IL_LOWERING */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#include "il_walk.h"
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

/* C++11 adds a distinction between scoped and unscoped enum types.  The
   former do not implicitly convert (promote) to integer types. */
#define is_integer_or_unscoped_enum(tp) \
  (type_kind_is_integer(tp) && !(tp)->variant.integer.is_scoped_enum)

/* The bool type is an integral type that is tagged as bool.  It only
   exists when bool_is_keyword is TRUE, or in C99 mode. */
#define is_bool(tp) \
  (type_kind_is_integer(tp) && (tp)->variant.integer.bool_type)

/* The nullptr type is the type of the nullptr keyword in C++ (i.e.,
   std::nullptr_t) and also includes the managed nullptr type in
   C++/CLI. */
#define is_nullptr(tp) ((tp)->kind == (a_type_kind)tk_nullptr)

/* Character types are three particular integral types. */
#define is_character(tp) \
  (is_integral(tp) && \
   ((tp)->variant.integer.int_kind == (an_integer_kind)ik_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_unsigned_char || \
    (tp)->variant.integer.int_kind == (an_integer_kind)ik_signed_char) && \
   !(tp)->variant.integer.wchar_t_type && \
   !(tp)->variant.integer.char16_t_type && \
   !(tp)->variant.integer.char32_t_type && \
   !(tp)->variant.integer.enum_type && \
   !(tp)->variant.integer.bool_type)

/* A general character is a char-type, a wchar_t, a char16_t, or a char32_t. */
#define is_general_character(tp) \
  (is_integral(tp) && \
   ((((tp)->variant.integer.int_kind == (an_integer_kind)ik_char || \
      (tp)->variant.integer.int_kind == (an_integer_kind)ik_unsigned_char || \
      (tp)->variant.integer.int_kind == (an_integer_kind)ik_signed_char) && \
     !(tp)->variant.integer.bool_type) || \
    (!wchar_t_is_keyword && \
     ((tp)->variant.integer.int_kind == targ_wchar_t_int_kind)) || \
    (uliterals_enabled && \
     ((!char16_t_and_char32_t_are_keywords && \
       ((tp)->variant.integer.int_kind == targ_char16_t_int_kind)) || \
      (!char16_t_and_char32_t_are_keywords && \
       ((tp)->variant.integer.int_kind == targ_char32_t_int_kind)))) || \
    (tp)->variant.integer.wchar_t_type || \
    (tp)->variant.integer.char16_t_type || \
    (tp)->variant.integer.char32_t_type))

#if FIXED_POINT_ALLOWED
#define is_fixed_point(tp) ((tp)->kind == (a_type_kind)tk_fixed_point)
#endif /* FIXED_POINT_ALLOWED */

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

#if FIXED_POINT_ALLOWED
#define or_is_fixed_point_type(tp)                                    \
  || (is_fixed_point(tp))
#else /* !FIXED_POINT_ALLOWED */
#define or_is_fixed_point_type(tp)  /* Nothing */
#endif /* FIXED_POINT_ALLOWED */

/* Arithmetic types are the integral types plus the floating types; in C++
   mode enum types are not integral.  Fixed-point types (an extension
   available in some configurations) are also arithmetic types. */
#define is_arithmetic_or_enum(tp)                                     \
  (is_integral_or_enum(tp) || is_floating(tp) or_is_fixed_point_type(tp))

#define is_arithmetic_or_unscoped_enum(tp)                            \
  (is_integer_or_unscoped_enum(tp) || is_floating(tp)                 \
   or_is_fixed_point_type(tp))

#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_generic_param(tp)                                      \
   ((tp)->kind == (a_type_kind)tk_template_param &&                   \
    (tp)->variant.template_param.is_generic_param)

#define is_pointer(tp) (is_pointer_or_handle(tp) &&                   \
                        !(tp)->variant.pointer.is_handle)
#define is_non_cli_pointer(tp) (is_pointer(tp) && \
                               !(tp)->variant.pointer.is_interior_ptr && \
                               !(tp)->variant.pointer.is_pin_ptr)
#define is_cli_pointer(tp) (is_pointer(tp) && \
                            ((tp)->variant.pointer.is_interior_ptr || \
                             (tp)->variant.pointer.is_pin_ptr))
#define same_cli_pointer_kinds(tp1, tp2) \
  ((tp1)->variant.pointer.is_interior_ptr == \
   (tp2)->variant.pointer.is_interior_ptr && \
   (tp1)->variant.pointer.is_pin_ptr == \
   (tp2)->variant.pointer.is_pin_ptr)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_pointer(tp) (is_pointer_or_handle(tp))
#define is_non_cli_pointer(tp) (is_pointer(tp))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* The reference type is a tk_pointer with the is_reference flag set.
   That includes both lvalue and rvalue references, and C++/CLI
   tracking references in some cases. */
#define is_any_reference(tp) ((tp)->kind == (a_type_kind)tk_pointer && \
                              (tp)->variant.pointer.is_reference)
#if MICROSOFT_EXTENSIONS_ALLOWED
/* This is called is_reference_ptr because there is a field called
   is_reference in il_def.h and old preprocessors have problems with
   that. */
#define is_reference_ptr(tp) (is_any_reference(tp) && \
                              !(tp)->variant.pointer.is_handle)
#define is_tracking_reference(tp) (is_any_reference(tp) && \
                                   (tp)->variant.pointer.is_handle)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_reference_ptr(tp) (is_any_reference(tp))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
/* Scalar types are the arithmetic and enum types plus the pointer (and
   pointer-to-member) types and the nullptr type.  C++/CLI handle types
   are also scalar (ECMA-372, 12). */
#define is_scalar(tp) (is_arithmetic_or_enum(tp) || \
                       is_pointer_or_handle(tp) ||  \
                       is_ptr_to_member(tp) ||      \
                       is_nullptr(tp))

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

/* Template parameter type. */
#define is_template_param(tp) ((tp)->kind == (a_type_kind)tk_template_param)

/* Incomplete types are types that have been declared but have not yet been
   (completely) defined.  (void types are also considered "incomplete".) */
#define is_incomplete(tp) ((tp)->incomplete)

/* Macro that is TRUE if two type kinds are the same, or are the same except
   that one is tk_class and the other is tk_struct. */
#define equiv_type_kinds(kind_1, kind_2)				\
  (kind_1 == kind_2 ||							\
  (kind_1 == (a_type_kind)tk_class && kind_2 == (a_type_kind)tk_struct) || \
  (kind_2 == (a_type_kind)tk_class && kind_1 == (a_type_kind)tk_struct))

#if GNU_EXTENSIONS_ALLOWED

/* Macro that is TRUE if the two types have the same alignment attributes.
   The types are already known not to be typerefs and to have the same type
   kind.  The alignment on an array always reflects an alignment attribute
   specified through a typedef on top of it, and is therefore ignored here.
   Incomplete types do not have their alignments set yet.  Nonreal dependent
   types do not have meaningful alignments either. */
#define same_alignment_attributes(type_1, type_2) \
  ((type_1)->alignment == (type_2)->alignment || \
   is_array(type_1) || \
   is_incomplete(type_1) || is_incomplete(type_2) || \
   (gpp_mode && (is_template_param_or_nonreal_class_type(type_1) || \
                 is_template_param_or_nonreal_class_type(type_2))))

#endif /* GNU_EXTENSIONS_ALLOWED */

#if FIXED_POINT_ALLOWED

/* Macro that is TRUE if the given fixed-point types are identical. */
#define same_fixed_point_type(tp1, tp2)                                      \
  ((tp1)->variant.fixed_point.precision ==                                   \
                                    (tp2)->variant.fixed_point.precision &&  \
   (tp1)->variant.fixed_point.is_unsigned ==                                 \
                                  (tp2)->variant.fixed_point.is_unsigned &&  \
   (tp1)->variant.fixed_point.is_fract_type ==                               \
                                (tp2)->variant.fixed_point.is_fract_type &&  \
   (tp1)->variant.fixed_point.saturating ==                                  \
                                     (tp2)->variant.fixed_point.saturating)

#endif /* FIXED_POINT_ALLOWED */

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
return a pointer to that.  cv-qualifiers (const, volatile) are not dropped,
but decltype entries are.  This function answers the request "give me the
underlying type, without changing the type represented."
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


a_type_ptr skip_typerefs_not_typedefs(a_type_ptr type_ptr)
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


a_type_ptr skip_typerefs_not_dependent_decltypes(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type, and return a pointer to
the underlying type, but keep dependent decltype or typeof types.
This is useful for cases where deduction will be done on the type:
a decltype has an underlying expression, which needs to be rescanned
during the deduction process and therefore must not be discarded.
Note that cv-qualifiers ARE stripped off.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref &&
         !type_ptr->variant.typeref.is_dependent_type_operator) {
    type_ptr = type_ptr->variant.typeref.type;
  }  /* while */
  return type_ptr;
}  /* skip_typerefs_not_dependent_decltypes */

#if !STANDALONE_UTILITY_PROGRAM

a_type_ptr skip_typerefs_not_parameterized_decltypes(a_type_ptr type_ptr)
/*
Strip any typeref entries off the given type, and return a pointer to the
underlying type, but keep decltype or typeof types that depend on a template
parameter.  This is distinct from skip_typerefs_not_dependent_decltypes in
that a decltype that is instantiation dependent but with an otherwise known
type is skipped by this routine.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref) {
    if (type_ptr->variant.typeref.is_dependent_type_operator &&
        is_template_dependent_type(type_ptr->variant.typeref.type)) {
      break;
    } else {
      type_ptr = type_ptr->variant.typeref.type;
    }  /* if */
  }  /* while */
  return type_ptr;
}  /* skip_typerefs_not_parameterized_decltypes */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_type_ptr skip_typedefs_not_dependent_decltypes(a_type_ptr type_ptr)
/*
Strip any typedef entries off the given type, and return a pointer to
the underlying type, but keep cv-qualifiers and dependent decltype or
typeof types.  This is useful for cases where deduction will be done
on the type: a decltype has an underlying expression, which needs to
be rescanned during the deduction process and therefore must not be
discarded.  This function answers the request "give me the underlying
type, without changing the type represented, for deduction purposes."
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref &&
         !typeref_is_qualified(type_ptr) &&
         !type_ptr->variant.typeref.is_dependent_type_operator) {
    type_ptr = type_ptr->variant.typeref.type;
  }  /* while */
  return type_ptr;
}  /* skip_typedefs_not_dependent_decltypes */

#if BACK_END_IS_CP_GEN_BE

a_type_ptr skip_typerefs_not_typedefs_or_type_operators(a_type_ptr type_ptr)
/*
Skip any typerefs that don't represent a typedef or a type operator.
*/
{
  while (type_ptr->kind == (a_type_kind)tk_typeref &&
         !(typeref_is_typedef(type_ptr) ||
           typeref_is_type_operator(type_ptr))) {
    type_ptr = type_ptr->variant.typeref.type;
  }  /* while */
  return type_ptr;
}  /* skip_typerefs_not_typedefs_or_type_operators */

#endif /* BACK_END_IS_CP_GEN_BE */

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


a_boolean is_incomplete_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an incomplete array type (i.e., one
declared without an array bound).  This differs from is_incomplete_type,
which returns TRUE if an array with a specified bound has an incomplete
element type.  If the type is not an array type, FALSE is returned.
*/
{
  a_boolean	result;
  tp = skip_typerefs(tp);
  result = is_array(tp) && !tp->variant.array.bound_is_zero &&
           !has_unknown_specified_bound(tp) &&
           tp->variant.array.variant.number_of_elements == 0;
  return result;
}  /* is_incomplete_array_type */


a_boolean is_flexible_array_type(a_type_ptr tp)
/*
Return TRUE if the given type could be the type of a flexible array member.
This is very similar to is_incomplete_array_type, except that in GNU C++ mode
a zero-length array is acceptable too.
*/
{
  a_boolean	result;
  tp = skip_typerefs(tp);
  result = is_array(tp) &&
           tp->variant.array.variant.number_of_elements == 0 &&
           (gpp_mode || !tp->variant.array.bound_is_zero) &&
           !has_unknown_specified_bound(tp);
  return result;
}  /* is_flexible_array_type */


a_boolean class_type_has_body(a_type_ptr tp)
/*
Return TRUE if the indicated type (a struct, union, or class type) has a
definition.  Note that this may return TRUE when the given type is still
incomplete (because the definition is not done yet).
*/
{
  a_boolean                   has_body;
  a_class_type_supplement_ptr ctsp;

  check_assertion(is_immediate_class_type(tp));
  ctsp = tp->variant.class_struct_union.extra_info;
  has_body = (tp->variant.class_struct_union.field_list != NULL ||
              tp->variant.class_struct_union.is_empty_class ||
              ctsp->assoc_scope != NULL);
  return has_body;
}  /* class_type_has_body */


a_boolean is_object_type(a_type_ptr tp)
/*
Return TRUE if the given type is an object type.  In C, that is always a
complete type.  In C++, it may be an incomplete type because object types
include incompletely-defined object types.  See is_complete_object_type
for an alternative.
*/
{
  a_boolean result;

  tp = skip_typerefs(tp);
  if (C_mode()) {
    /* In C mode, object types are complete types of objects.  See C99
       standard, 6.2.5p1: incomplete types are not object types. */
    result = !is_function(tp) && !is_incomplete(tp);
  } else {
    /* In C++ mode, an object type is one that is not a function type,
       not a reference type, and not a void type.  C++ object types do include
       incompletely-defined object types (incomplete class types, arrays of
       unknown size, etc.).  See C++ standard, [basic.types]. */
    result = !is_function(tp) && !is_any_reference(tp) &&
             !is_void(tp);
  }  /* if */
  return result;
}  /* is_object_type */


a_boolean is_complete_object_type(a_type_ptr tp)
/*
Return TRUE if the given type is a complete object type.  In C, that's the
same as an object type.  In C++, it's an object type that is not
incompletely-defined.
*/
{
  a_boolean result;

  tp = skip_typerefs(tp);
  result = is_object_type(tp);
  if (!C_mode() && is_incomplete(tp)) result = FALSE;
  return result;
}  /* is_complete_object_type */


a_boolean is_void_type(a_type_ptr tp)
/*
Return TRUE if the given type is the void type (3.1.2.5) or a cv-qualified
version thereof.
*/
{
  tp = skip_typerefs(tp);
  return(is_void(tp));
}  /* is_void_type */


a_boolean is_nullptr_type(a_type_ptr tp)
/*
Return TRUE if the given type is the type of the nullptr keyword in C++
(including both nullptr and __nullptr in C++/CLI), or a cv-qualified version
thereof.
*/
{
  tp = skip_typerefs(tp);
  return(is_nullptr(tp));
}  /* is_nullptr_type */


a_boolean is_managed_nullptr_type(a_type_ptr tp)
/*
In C++/CLI, the nullptr keyword has a type that is distinct from
std::nullptr_t (which is the type of the nullptr keyword in non-C++/CLI
modes and of the __nullptr keyword in Microsoft modes, both C++/CLI and
native).  This function returns TRUE for the type of the C++/CLI nullptr
keyword or a cv-qualified version thereof and FALSE for std::nullptr_t and
its cv-qualified variants.
*/
{
  tp = skip_typerefs(tp);
  return(is_nullptr(tp) && tp->incomplete);
}  /* is_managed_nullptr_type */


a_boolean is_standard_nullptr_type(a_type_ptr tp)
/*
Return TRUE if the given type is std::nullptr_t or a cv-qualified version
thereof.  (See is_managed_nullptr_keyword above for a discussion of the
distinction between the managed and standard nullptr types.)
*/
{
  tp = skip_typerefs(tp);
  return(is_nullptr(tp) && !tp->incomplete);
}  /* is_standard_nullptr_type */


a_boolean is_void_star_type(a_type_ptr tp)
/*
Return TRUE if the given type is the void* type.  Note that this does not
allow "const void*" or any other qualified version, nor does it allow
C++/CLI interior_ptr<void> or pin_ptr<void>.
*/
{
  a_boolean is_void_star = FALSE;

  tp = skip_typerefs(tp);
  if (is_non_cli_pointer(tp)) {
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


a_boolean is_scoped_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is a scoped enum type.
*/
{
  tp = skip_typerefs(tp);
  return type_kind_is_integer(tp) && integer_type_is_scoped_enum(tp);
}  /* is_scoped_enum_type */


a_boolean is_unscoped_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an unscoped enum type.
*/
{
  tp = skip_typerefs(tp);
  return is_enum(tp) && !integer_type_is_scoped_enum(tp);
}  /* is_unscoped_enum_type */


a_boolean is_integral_or_enum_type(a_type_ptr tp)
/*
Return TRUE if the type is an integral type or an enum type.  (In C++ an
enum type is not considered an integral type; in C it is.)
*/
{
  tp = skip_typerefs(tp);
  return(is_integral_or_enum(tp));
}  /* is_integral_or_enum_type */


a_boolean is_integral_or_unscoped_enum_type(a_type_ptr tp)
/*
Return TRUE if the type is an integral type or an unscoped enum type.  (In C++
an enum type is not considered an integral type; in C it is.)
*/
{
  tp = skip_typerefs(tp);
  return is_integer_or_unscoped_enum(tp);
}  /* is_integral_or_unscoped_enum_type */


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


a_boolean is_plain_char_type(a_type_ptr tp)
/*
Return TRUE if the type is a "plain" char type (not signed or unsigned).
*/
{
  tp = skip_typerefs(tp);
  return tp->kind == (a_type_kind)tk_integer &&
         tp->variant.integer.int_kind == (an_integer_kind)ik_char &&
         !tp->variant.integer.enum_type &&
         !tp->variant.integer.bool_type &&
         !tp->variant.integer.wchar_t_type &&
         !tp->variant.integer.char16_t_type &&
         !tp->variant.integer.char32_t_type;
}  /* is_plain_char_type */

#if MICROSOFT_EXTENSIONS_ALLOWED
#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_narrow_or_wide_character_type(a_type_ptr tp)
/*
Return TRUE if the given type is a narrow ("char") or wide ("wchar_t")
character type.
*/
{
  tp = skip_typerefs(tp);
  return is_integral(tp) &&
         !tp->variant.integer.bool_type &&
         (tp->variant.integer.int_kind == (an_integer_kind)ik_char ||
          tp->variant.integer.int_kind == (an_integer_kind)ik_unsigned_char ||
          tp->variant.integer.int_kind == (an_integer_kind)ik_signed_char ||
          (!wchar_t_is_keyword &&
           tp->variant.integer.int_kind == targ_wchar_t_int_kind) ||
          tp->variant.integer.wchar_t_type);
}  /* is_narrow_or_wide_character_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_cli_generic_definition_argument_type(a_type_ptr	type)
/*
Return TRUE if type is the type created to represent a generic argument
within the definition of a C++/CLI generic.  The type will be either a
value class or a handle to a ref or interface class.
*/
{
  a_boolean	result = FALSE;

  type = skip_typerefs(type);
  if (is_handle_ptr(type)) {
    type = type->variant.pointer.type;
    type = skip_typerefs(type);
  }  /* if */
  result = is_cli_generic_constraint(type);
  return result;
}  /* is_cli_generic_definition_argument_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if FIXED_POINT_ALLOWED

a_boolean is_fixed_point_type(a_type_ptr tp)
/*
Return TRUE if the given type is a fixed-point type.
*/
{
  tp = skip_typerefs(tp);
  return(is_fixed_point(tp));
}  /* is_fixed_point_type */

#endif /* FIXED_POINT_ALLOWED */

a_boolean is_floating_type(a_type_ptr tp)
/*
Return TRUE if the given type is a floating type.  In C99 and GNU modes, that
includes complex and imaginary types.
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


a_boolean is_nonreal_floating_type(a_type_ptr tp)
/*
Return TRUE if the given type is a nonreal (imaginary or complex) floating
type.
*/
{
  tp = skip_typerefs(tp);
  return is_nonreal_floating(tp);
}  /* is_nonreal_floating_type */


a_boolean is_imaginary_type(a_type_ptr tp)
/*
Return TRUE if the given type is an imaginary floating type.
*/
{
  tp = skip_typerefs(tp);
  return is_imaginary(tp);
}  /* is_imaginary_type */


a_boolean is_complex_type(a_type_ptr tp)
/*
Return TRUE if the given type is a complex floating type.
*/
{
  tp = skip_typerefs(tp);
  return is_complex(tp);
}  /* is_complex_type */

#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_VECTOR_TYPES_ALLOWED

a_boolean is_vector_type(a_type_ptr  tp)
/*
Return TRUE if the given type is a vector type (tk_vector).  For typerefs,
consider the underlying type.
*/
{
  return skip_typerefs(tp)->kind == (a_type_kind)tk_vector;
}  /* is_vector_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean vector_type_is_template_dependent(a_type_ptr  tp)
/*
The given type must be a tk_vector type.  Return TRUE if its size or its
element type is template-dependent.
*/
{
  a_constant_ptr  sc;
  check_assertion(tp->kind == (a_type_kind)tk_vector);
  sc = tp->variant.vector.size_constant;
  return (sc != NULL && sc->kind == (a_constant_repr_kind)ck_template_param) ||
         is_template_dependent_type(tp->variant.vector.element_type);
}  /* vector_type_is_template_dependent */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* GNU_VECTOR_TYPES_ALLOWED */

a_boolean is_arithmetic_or_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an arithmetic type or an enum type.  This
includes scoped enum types in C++11 and C++/CLI modes.  (An enum type *is* an
arithmetic type in C but not in C++.)
*/
{
  tp = skip_typerefs(tp);
  return(is_arithmetic_or_enum(tp));
}  /* is_arithmetic_or_enum_type */


a_boolean is_arithmetic_or_unscoped_enum_type(a_type_ptr tp)
/*
Return TRUE if the given type is an arithmetic type or an unscoped enum type.
*/
{
  tp = skip_typerefs(tp);
  return is_arithmetic_or_unscoped_enum(tp);
}  /* is_arithmetic_or_unscoped_enum_type */


a_boolean is_arithmetic_type(a_type_ptr tp)
/*
Return TRUE if the given type is an arithmetic type.
*/
{
  a_boolean is_arith = FALSE;

  tp = skip_typerefs(tp);
  if (is_arithmetic_or_enum(tp)) {
    is_arith =  TRUE;
    /* An enum type is arithmetic in C, but not in C++. */
    if (!C_mode() && is_enum(tp)) is_arith = FALSE;
  }  /* if */
  return is_arith;
}  /* is_arithmetic_type */


a_boolean is_pointer_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer type.  In C++/CLI mode, includes
interior_ptr and pin_ptr types.
*/
{
  tp = skip_typerefs(tp);
  return(is_pointer(tp));
}  /* is_pointer_type */


a_boolean is_plain_pointer_type(a_type_ptr tp)
/*
Return TRUE if the given type is a "plain" pointer type.  ("plain" in the sense
that C++/CLI-mode handles, interior_ptr, and pin_ptr types are not included.)
*/
{
  tp = skip_typerefs(tp);
  return tp->kind == (a_type_kind)tk_pointer &&
#if MICROSOFT_EXTENSIONS_ALLOWED
         !tp->variant.pointer.is_handle &&
         !tp->variant.pointer.is_interior_ptr &&
         !tp->variant.pointer.is_pin_ptr &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         !tp->variant.pointer.is_reference;
}  /* is_pointer_type */


a_boolean is_pointer_to_object_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer to an object type.  Note that
object types can be incomplete in some cases.
*/
{
  a_boolean result = FALSE;

  tp = skip_typerefs(tp);
  if (is_pointer(tp)) {
    result = is_object_type(tp->variant.pointer.type);
  }  /* if */
  return result;
}  /* is_pointer_to_object_type */


a_boolean is_pointer_or_handle_type(a_type_ptr tp)
/*
Return TRUE if the given type is a pointer type or a C++/CLI handle type.
Includes C++/CLI interior_ptr and pin_ptr types.
*/
{
  tp = skip_typerefs(tp);
  return is_pointer_or_handle(tp);
}  /* is_pointer_or_handle_type */


a_boolean types_are_both_pointers_or_both_handles(a_type_ptr  tp1, 
                                                  a_type_ptr  tp2)
/*
Return TRUE if tp1 and tp2 are both pointers or both C++/CLI handles.
*/
{
  a_boolean  result = FALSE;

  tp1 = skip_typerefs(tp1);
  tp2 = skip_typerefs(tp2);
  if (is_pointer(tp1) && is_pointer(tp2)) {
    result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_handle_ptr(tp1) && is_handle_ptr(tp2)) {
    result = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return result;
}  /* types_are_both_pointers_or_both_handles */


a_boolean is_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is a reference type.
*/
{
  tp = skip_typerefs(tp);
  return(is_reference_ptr(tp));
}  /* is_reference_type */


a_boolean is_lvalue_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is an ordinary "lvalue" reference type.
C++/CLI tracking references are excluded.
*/
{
  tp = skip_typerefs(tp);
  return is_reference_ptr(tp) && !tp->variant.pointer.is_rvalue_reference;
}  /* is_lvalue_reference_type */


a_boolean is_any_lvalue_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is an ordinary "lvalue" reference type,
including a C++/CLI tracking reference.
*/
{
  tp = skip_typerefs(tp);
  return is_any_reference(tp) && !tp->variant.pointer.is_rvalue_reference;
}  /* is_any_lvalue_reference_type */


a_boolean is_rvalue_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++11 rvalue reference type.
*/
{
  tp = skip_typerefs(tp);
  /* Note that C++/CLI tracking references are never rvalue references. */
  return is_reference_ptr(tp) && tp->variant.pointer.is_rvalue_reference;
}  /* is_rvalue_reference_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean rvalue_ref_can_be_bound_to_function_lvalue(void)
/*
Return TRUE if in the current dialect an rvalue reference can be bound
to a function lvalue.  (The C++11 standard allows that, but some earlier
implementations of rvalue references did not.)
*/
{
  a_boolean can_be_bound = TRUE;

  if (microsoft_mode && microsoft_version < 1800) can_be_bound = FALSE;
  return can_be_bound;
}  /* rvalue_ref_can_be_bound_to_function_lvalue */


a_boolean is_reference_that_can_bind_to_rvalue(a_type_ptr type)
/*
Return TRUE if type is a reference type that can bind to rvalues (including
xvalues), e.g., an lvalue reference to non-volatile const.
*/
{
  a_boolean can_bind = FALSE;

  if (is_lvalue_reference_type(type)) {
    a_type_ptr under_type = type_pointed_to(type);
    if (is_const_qualified_type(under_type)) {
      can_bind = TRUE;
      if (is_volatile_qualified_type(under_type)) {
        if (microsoft_bugs && (microsoft_version < 1600 ||
                               !is_class_struct_union_type(under_type))) {
          /* Before VC10, Microsoft allowed binding rvalues to references to
             const volatile types.  VC10 and later still allow it if the
             reference is to a non-class type. */
        } else if (any_cfront_mode()) {
          /* Cfront never considered volatile. */
        } else {
          can_bind = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_rvalue_reference_type(type)) {
    /* Rvalue references generally bind to rvalues, but rvalue references
       to functions bind to lvalues. */
    a_type_ptr under_type = type_pointed_to(type);
    can_bind = TRUE;
    if (is_function_type(under_type) &&
        rvalue_ref_can_be_bound_to_function_lvalue()) {
      can_bind = FALSE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && is_tracking_reference_type(type)) {
    /* A tracking reference binds to lvalues. */
    can_bind = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return can_bind;
}  /* is_reference_that_can_bind_to_rvalue */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean types_are_references_of_the_same_kind(a_type_ptr  tp1, 
                                                a_type_ptr  tp2)
/*
Return TRUE if tp1 and tp2 are both references or both C++/CLI tracking
references.
*/
{
  a_boolean  result = FALSE;

  tp1 = skip_typerefs(tp1);
  tp2 = skip_typerefs(tp2);
  if (is_reference_ptr(tp1) && is_reference_ptr(tp2)) {
    result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_tracking_reference(tp1) && is_tracking_reference(tp2)) {
    result = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return result;
}  /* types_are_references_of_the_same_kind */


a_boolean is_ptr_or_ref_type(a_type_ptr tp)
/*
Return TRUE if the given type is an IL pointer type (i.e., a pointer or
reference).  Excludes C++/CLI handles and tracking references, but includes
C++/CLI interior_ptr and pin_ptr types.
*/
{
  tp = skip_typerefs(tp);
  return (tp->kind == (a_type_kind)tk_pointer
#if MICROSOFT_EXTENSIONS_ALLOWED
          && !tp->variant.pointer.is_handle
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         );
}  /* is_ptr_or_ref_type */


a_boolean is_any_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is a reference or C++/CLI tracking reference.
*/
{
  tp = skip_typerefs(tp);
  return is_any_reference(tp);
}  /* is_any_reference_type */


a_boolean is_any_ptr_or_ref_type(a_type_ptr tp)
/*
Return TRUE if the given type is an IL pointer type (i.e., a pointer or
reference), including C++/CLI handles and tracking references.
*/
{
  tp = skip_typerefs(tp);
  return (tp->kind == (a_type_kind)tk_pointer);
}  /* is_any_ptr_or_ref_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_cli_generic_param_type(a_type_ptr  tp)
/*
Return TRUE if the given type entry represents a parameter of a C++/CLI
generic.
*/
{
  tp = skip_typerefs(tp);
  return is_cli_generic_param(tp);
}  /* is_cli_generic_param_type */


static a_boolean is_cli_generic_constraint_type(a_type_ptr tp)
/*
Return TRUE if the given type entry represents the class constraint
version of a C++/CLI generic parameter.
*/
{
  a_boolean is_constraint = FALSE;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) && is_cli_generic_constraint(tp)) {
    is_constraint = TRUE;
  }  /* if */
  return is_constraint;
}  /* is_cli_generic_constraint_type */


a_boolean is_handle_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI handle type.
*/
{
  tp = skip_typerefs(tp);
  return is_handle_ptr(tp);
}  /* is_handle_type */


a_boolean is_handle_type_not_value_generic(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI handle type, but not a
generic type that might in some cases be a value type.
*/
{
  a_boolean result = FALSE;

  if (is_handle_type(tp)) {
    result = TRUE;
    tp = type_pointed_to(tp);
    tp = skip_typerefs(tp);
    if (is_cli_generic_constraint(tp) &&
        tp->variant.class_struct_union.unconstrained) {
      /* This is a generic type that could be either a handle to a ref class
         or a value class type.  Note that it would seem sensible also to
         do the same for a generic constrained to derive from an interface,
         since that could also be a value class, but that's not how VC10
         does it. */
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_handle_type_not_value_generic */


a_boolean is_handle_type_not_generic_constraint(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI handle type, but not a
handle that was implicitly added to a generic constraint type and is
therefore not "real".
*/
{
  a_boolean result = FALSE;

  if (is_handle_type(tp)) {
    result = TRUE;
    tp = type_pointed_to(tp);
    tp = skip_typerefs(tp);
    if (is_cli_generic_constraint(tp)) {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_handle_type_not_generic_constraint */


a_boolean is_tracking_reference_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI tracking reference type.
*/
{
  tp = skip_typerefs(tp);
  return is_tracking_reference(tp);
}  /* is_tracking_reference_type */


a_boolean is_handle_or_tracking_ref_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI handle or tracking reference type.
*/
{
  tp = skip_typerefs(tp);
  return is_handle_ptr(tp) || is_tracking_reference(tp);
}  /* is_handle_or_tracking_ref_type */


a_boolean is_interior_ptr_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI interior_ptr type.
*/
{
  tp = skip_typerefs(tp);
  return (tp->kind == (a_type_kind)tk_pointer &&
          tp->variant.pointer.is_interior_ptr);
}  /* is_interior_ptr_type */


a_boolean is_pin_ptr_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI pin_ptr type.
*/
{
  tp = skip_typerefs(tp);
  return (tp->kind == (a_type_kind)tk_pointer &&
          tp->variant.pointer.is_pin_ptr);
}  /* is_pin_ptr_type */


a_boolean is_cli_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI array type.
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_class_type(tp) && class_type_supp(tp)->is_cli_array;
}  /* is_cli_array_type */


a_boolean is_handle_to_cli_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is a handle to a C++/CLI array type.
*/
{
  return is_handle_type(tp) &&
         is_cli_array_type(type_pointed_to(tp));
}  /* is_handle_to_cli_array_type */


a_boolean is_handle_to_nonconst_cppcx_plain_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is a handle to a non-const Platform::Array
instance (C++/CX mode).
*/
{
  a_boolean  result = FALSE;

  tp = skip_typerefs(tp);
  if (is_handle_ptr(tp)) {
    tp = type_pointed_to(tp);
    if (!is_const_qualified_type(tp)) {
      tp = skip_typerefs(tp);
      if (is_immediate_class_type(tp) &&
          class_type_supp(tp)->is_cli_array &&
          !class_type_supp(tp)->is_cppcx_write_only_array) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_handle_to_nonconst_cppcx_array_type */


a_type_ptr cli_array_element_type(a_type_ptr tp)
/*
tp is a C++/CLI array type.  Return its element type.
*/
{
  a_template_arg_ptr tap;
  a_type_ptr         elem_type;

  tp = skip_typerefs(tp);
  check_assertion(is_cli_array_type(tp));
  /* The element type is the first template argument type. */
  tap = class_type_supp(tp)->template_arg_list;
  check_assertion(tap != NULL && is_type_templ_arg(tap));
  elem_type = tap->variant.type;
  return elem_type;
}  /* cli_array_element_type */


a_constant_ptr cli_array_rank_constant(a_type_ptr  tp)
/*
Return a pointer to the constant representing the number of dimensions of the
given C++/CLI array type.
*/
{
  a_template_arg_ptr  tap;

  tp = skip_typerefs(tp);
  check_assertion(is_cli_array_type(tp));
  tap = class_type_supp(tp)->template_arg_list;
  /* The first argument is the element type. */
  check_assertion(tap != NULL && is_type_templ_arg(tap));
  /* The rank is the value of the second template argument. */
  tap = tap->next;
  check_assertion(tap != NULL && is_nontype_templ_arg(tap) &&
                  !tap->is_array_bound_of_unknown_type);
  return tap->variant.constant;
}  /* cli_array_rank_constant */


a_host_large_unsigned cli_array_rank(a_type_ptr tp,
                                     a_boolean  *unknown)
/*
tp is a C++/CLI array type.  Return its rank, i.e., the number of dimensions.
If the rank is unknown, e.g., because it's template-dependent, *unknown is
set to TRUE and 0 is returned.
*/
{
  a_host_large_unsigned rank = 0;
  a_constant_ptr        rank_con = cli_array_rank_constant(tp);

  if (rank_con->kind == (a_constant_repr_kind)ck_template_param ||
      rank_con->kind == (a_constant_repr_kind)ck_error) {
    /* A template-dependent value or error is unknown. */
    *unknown = TRUE;
  } else {
    a_boolean ovflo;
    check_assertion(rank_con->kind == (a_constant_repr_kind)ck_integer);
    rank = unsigned_value_of_integer_constant(rank_con, &ovflo);
    check_assertion(!ovflo);
    *unknown = FALSE;
  }  /* if */
  return rank;
}  /* cli_array_rank */
  

a_boolean is_ref_class_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is a C++/CLI ref class or ref struct.
*/
{
  a_boolean is_ref_class = FALSE;

  if (cli_or_cx_enabled) {
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp) && cli_class_type_kind_is(tp, cctk_ref)) {
      is_ref_class = TRUE;
    }  /* if */
  }  /* if */
  return is_ref_class;
}  /* is_ref_class_type */


a_boolean is_value_class_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is a C++/CLI value class or value struct.
*/
{
  a_boolean is_value_class = FALSE;

  if (cli_or_cx_enabled) {
    tp = skip_typerefs(tp);
    if (is_immediate_class_type(tp) &&
        cli_class_type_kind_is(tp, cctk_value)) {
      is_value_class = TRUE;
    }  /* if */
  }  /* if */
  return is_value_class;
}  /* is_value_class_type */


a_boolean is_simple_value_class_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is an ECMA-372 section 22.4 simple C++/CLI
value type.
*/
{
  a_boolean result = FALSE;

  tp = skip_typerefs(tp);
  if (is_value_class_type(tp)) {
    a_field_ptr curr;

    result = TRUE;  /* Assume */
    for (curr = tp->variant.class_struct_union.field_list;
         curr != NULL;
         curr = curr->next) {
      a_type_ptr curr_type = skip_typerefs(curr->type);
      if (!(system_type_from_fundamental_type(curr_type) != NULL ||
            fundamental_type_from_system_type(curr_type) != NULL ||
            is_enum(curr_type) ||
            is_pointer(curr_type) ||
            is_simple_value_class_type(curr_type))) {
        /* A simple value type can only have members that are either
           fundamental types, enums, pointers, or another simple value type. */
        check_assertion(!is_void(curr_type));
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* is_simple_value_class_type */


a_boolean is_standard_class_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is a native class type, i.e., a class
type that's not a C++/CLI class.
*/
{
  a_boolean is_standard_class = FALSE;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) &&
      cli_class_type_kind_is(tp, cctk_standard)) {
    is_standard_class = TRUE;
  }  /* if */
  return is_standard_class;
}  /* is_standard_class_type */


a_boolean is_managed_class_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI managed class type.
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_managed_class_type(tp);
}  /* is_managed_class_type */


a_boolean is_nonreal_template_template_param_instance(a_type_ptr tp)
/*
Return TRUE if the given type is a nonreal instance of a template template
parameter.
*/
{
  a_boolean  result = FALSE;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    a_template_ptr  templ = tp->variant.class_struct_union.extra_info
                              ->assoc_template;
    if (templ != NULL &&
        templ->kind == (a_template_kind)templk_template_template_param) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_nonreal_template_template_param_instance */


a_boolean is_cli_interface_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI interface class type.
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_cli_interface_type(tp);
}  /* is_cli_interface_type */


a_boolean is_cli_ref_or_interface_class_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is a C++/CLI ref class or interface class.
*/
{
  a_boolean result = FALSE;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) &&
      (cli_class_type_kind_is(tp, cctk_ref) ||
       cli_class_type_kind_is(tp, cctk_interface))) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_cli_ref_or_interface_class_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_cli_value_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI value type, which includes value
class types and also fundamental types (other than "void"), enums, and
pointers.  See ECMA standard 12.1.
*/
{
  a_boolean result = FALSE;

  if (cli_or_cx_enabled) {
    tp = skip_typerefs(tp);
    if (is_value_class_type(tp) ||
        is_enum(tp) ||
        is_pointer(tp) ||
        (system_type_from_fundamental_type(tp) != NULL && !is_void(tp))) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_cli_value_type */


a_boolean is_boxable_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI value type that can be boxed.
See ECMA-372 14.2.6 ("Boxing Conversions").
*/
{
  a_boolean result = FALSE;

  if (cli_or_cx_enabled) {
    if (is_cli_value_type(tp) &&
        !is_pointer_type(tp)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_boxable_type */


a_type_ptr boxed_type_for(a_type_ptr  unboxed_type)
/*
The given type must be a boxable type.  Return the corresponding boxed type
(which in the case of a value class type is the type itself).  (Usable for
both C++/CLI and C++/CX.)
*/
{
  a_type_ptr  result, orig_type = unboxed_type;

  unboxed_type = skip_typerefs(unboxed_type);
  if (is_immediate_class_type(unboxed_type)) {
    check_assertion(cli_class_type_kind_is(unboxed_type, cctk_value));
    result = orig_type;
    if (cppcx_enabled) {
      /* In C++/CX mode, the boxed version of the type is
         Platform::Box<T>. */
      result = make_cppcx_box_type(result);
    }  /* if */
  } else if (is_enum(unboxed_type)) {
    an_integer_type_supplement_ptr  itsp = integer_type_supp(unboxed_type);
    if (itsp->boxed_type == NULL) {
      make_boxed_enum_type(unboxed_type);
    }  /* if */
    result = itsp->boxed_type;
  } else {
    if (cppcx_enabled) {
      /* In C++/CX mode, the boxed version of the type is
         Platform::Box<T>. */
      result = make_cppcx_box_type(orig_type);
    } else {
      result = system_type_from_fundamental_type(unboxed_type);
    }  /* if */
  }  /* if */
  check_assertion(result != NULL);
  return result;
}  /* boxed_type_for */

a_boolean is_cli_nullable_type(a_type_ptr tp)
/*
Return TRUE if tp is an instance of the C++/CLI System::Nullable generic
value type.
*/
{
  a_boolean	result = FALSE;

  check_assertion_or_expect_error(!cppcx_enabled);
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) && is_cli_generic_instance_type(tp)) {
    a_class_symbol_supplement_ptr	cssp = symbol_supplement_for_class(tp);
    /* Note that there is both a generic value class named System::Nullable
       and a non-generic ref class.  The symbol returned by name lookup
       is always the generic version. */
    if (cssp->class_template == cli_symbol_from_kind(csk_system_nullable)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_cli_nullable_type */


static a_boolean is_cppcx_box_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CX Platform::Box<T> type.
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_class_type(tp) && class_type_supp(tp)->is_cppcx_box;
}  /* is_cppcx_box_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_delegate_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI delegate type.
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_delegate_type(tp);
}  /* is_delegate_type */


a_routine_ptr delegate_invocation_function(a_type_ptr delegate_type)
/*
Given a delegate class type, return its Invoke function.
*/
{
  a_routine_ptr  rp;

  delegate_type = skip_typerefs(delegate_type);
  check_assertion(is_immediate_delegate_type(delegate_type));
  rp = class_type_supp(delegate_type)->assoc_scope->routines;
#if CHECKING
  { a_const_char *name = unmangled_name_of(&rp->source_corresp);
    check_assertion(name != NULL && strcmp(name, "Invoke") == 0);
  }
#endif /* CHECKING */
  return rp;
}  /* delegate_invocation_function */


a_boolean is_delegate_invocation_function(a_routine_ptr rp)
/*
Return TRUE if the given function is the Invoke function of a C++/CLI
delegate.
*/
{
  a_boolean is_invocation_func = FALSE;

  if (cli_or_cx_enabled && rp->source_corresp.is_class_member) {
    a_type_ptr parent_class = parent_class_of(rp);
    if (is_immediate_delegate_type(parent_class)) {
      if (rp == delegate_invocation_function(parent_class)) {
        is_invocation_func = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_invocation_func;
}  /* is_delegate_invocation_function */


a_type_ptr delegate_invocation_type(a_type_ptr delegate_type)
/*
Given a delegate class type, return the associated function type.
*/
{
  delegate_type = skip_typerefs(delegate_type);
  check_assertion(is_immediate_delegate_type(delegate_type));
  return class_type_supp(delegate_type)->invocation_type;
}  /* delegate_invocation_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean f_is_cli_type_of_kind(a_type_ptr        tp,
                                a_cli_symbol_kind csk)
/*
Return TRUE if the given type is the C++/CLI type of the specified kind.
*/
{
  a_type_ptr system_type = cli_class_type_for(csk);

  return system_type != NULL && identical_types(tp, system_type);
}  /* f_is_cli_type_of_kind */


a_boolean is_cli_attribute_type(a_type_ptr tp)
/*
Return TRUE if the given type is a C++/CLI attribute type.
*/
{
  a_boolean result = FALSE;

  tp = skip_typerefs(tp);
  if (cli_or_cx_enabled && is_immediate_class_type(tp) &&
      cli_class_type_kind_is(tp, cctk_ref)) {
    complete_class_type_is_needed(tp);
    result = !is_incomplete_type(tp) && class_type_supp(tp)->is_cli_attribute;
  }  /* if */
  return result;
}  /* is_cli_attribute_type */


a_boolean is_valid_cli_attribute_parameter_type(a_type_ptr tp)
/*
Return TRUE if the given type is a valid C++/CLI attribute parameter type.
*/
{
  a_boolean  result = FALSE;

  tp = skip_typerefs(tp);
  if (is_handle_type(tp)) {
    a_boolean  rank_unknown = TRUE;
    tp = type_pointed_to(tp);
    tp = skip_typerefs(tp);
    if (is_cli_system_string_type(tp) ||
        is_cli_system_object_type(tp) ||
        is_cli_system_type_type(tp) ||
        (is_cli_array_type(tp) && cli_array_rank(tp, &rank_unknown) == 1 &&
         is_valid_cli_attribute_parameter_type(cli_array_element_type(tp)))) {
      result = TRUE;
    }  /* if */
  } else {
    tp = map_cli_system_type_to_fundamental_type(tp);
    if (is_integral(tp) ||
        (is_cli_enum_type(tp) &&
         integer_type_supp(tp)->assembly_visibility ==
                                       (an_assembly_visibility)av_public) ||
        (is_floating(tp) &&
         (tp->variant.float_kind == (a_float_kind)fk_float ||
          tp->variant.float_kind == (a_float_kind)fk_double))) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_valid_cli_attribute_parameter_type */


a_boolean class_is_instance_of_generic_from_metadata(a_type_ptr  class_type)
/*
The given class type is an instance of a C++/CLI class type generic or a
nested class thereof.  Return TRUE if the generic was imported from metadata.
*/
{
  a_boolean  result;

  check_assertion(is_immediate_managed_class_type(class_type));
  if (class_type->source_corresp.is_class_member) {
    a_type_ptr  parent_class = parent_class_of(class_type);
    result = class_is_from_metadata(parent_class);
  } else {
    result = f_class_template_for_type(class_type)->variant.template_info
                                                  ->from_metadata;
  }  /* if */
  return result;
}  /* class_is_instance_of_generic_from_metadata */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_cli_open_constructed_type(a_type_ptr	tp)
/*
Return TRUE if tp is a C++/CLI open constructed type (ECMA-372 31.2.1).
*/
{
  a_boolean	result = FALSE;

  tp = skip_typerefs(tp);
  /* If the type provided is a handle, strip off the handle. */
  if (is_handle_type(tp)) tp = type_pointed_to(tp);
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    if (is_cli_generic_constraint_type(tp)) {
      /* A generic parameter is an open constructed type. */
      result = TRUE;
    } else if (is_cli_generic_definition_type(tp)) {
      /* Something like A<T> referenced from within A. */
      result = TRUE;
    } else if (tp->variant.class_struct_union.is_open_constructed_type) {
      /* This is a generic instance with one or more generic arguments that
         are open constructed types.  Note that CLI arrays are open
         constructed types if their element is an open constructed type.
         Because CLI arrays are implemented as templates, this test for
         an open constructed generic instance will be TRUE for CLI arrays
         of open constructed types. */
      result = TRUE;
    }  /* if */
  } else if (is_cli_generic_param_type(tp)) {
    /* A generic parameter. */
    result = TRUE;
  }  /* if */
  return result;
}  /* is_cli_open_constructed_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean is_scalar_type(a_type_ptr tp)
/*
Return TRUE if the given type is a scalar type (which in C++ includes
pointer-to-member types).
*/
{
  tp = skip_typerefs(tp);
  return is_scalar(tp);
}  /* is_scalar_type */


a_boolean is_simple_scalar_type(a_type_ptr tp)
/*
Return TRUE if the given type is a scalar type but not a C++ pointer-to-member
type.
*/
{
  tp = skip_typerefs(tp);
  return is_scalar(tp) && !is_ptr_to_member(tp);
}  /* is_simple_scalar_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_trivially_copyable_type(a_type_ptr tp)
/*
Return TRUE if the given type is trivially copyable.
*/
{
  a_boolean  result;
  
  if (is_volatile_qualified_type(tp)) {
    result = FALSE;
  } else {
    tp = skip_array_types(tp);
    tp = skip_typerefs(tp);
    if (is_scalar(tp)) {
      result = TRUE;
    } else if (is_immediate_class_type(tp)) {
      /* A class type is trivially copyable if:
          - it has no nontrivial move/copy constructors, and
          - it has no nontrivial move/copy assignment operators, and
          - it has a trivial destructor.
      */
      a_class_symbol_supplement_ptr  cssp = class_symbol_supp(symbol_for(tp));
      if (!has_nontrivial_destructor(cssp) &&
          !cssp->has_user_provided_copy_constructor &&
          !cssp->has_user_provided_move_constructor &&
          !cssp->has_user_provided_move_assign_operator &&
          !tp->variant.class_struct_union.any_volatile_member) {
        a_symbol_ptr  sym;
        a_boolean     is_list;
        result = TRUE;
        /* Check for nontrivial copy/move constructors.  We already checked
           that none are user-provided, so we can just check the compiler-
           generated constructors. */
        sym = cssp->constructor;
        if (sym != NULL && symbol_is(sym, sk_overloaded_function)) {
          is_list = TRUE;
          sym = sym->variant.overloaded_function.symbols;
        } else {
          is_list = FALSE;
        }  /* if */
        for (; sym != NULL; sym = is_list ? sym->next : NULL) {
          a_routine_ptr	    rp;
          a_param_type_ptr  ptp;
          a_boolean         one_param;
          if (symbol_is(sym, sk_function_template)) continue;
          check_assertion(symbol_is(sym, sk_member_function));
          rp = sym->variant.routine.ptr;
          ptp = function_type_params(rp->type);
          one_param = ptp != NULL && ptp->next == NULL;
          /* A generated constructor with one parameter is always a copy
             constructor.  For deleted constructors a more expensive check
             is needed. */
          if ((((rp->compiler_generated || rp->is_defaulted) && one_param) ||
               (rp->is_deleted &&
                is_copy_constructor(rp, tp, (a_type_qualifier_set*)NULL,
                                    /*include_move_ctors=*/TRUE,
                                    /*is_declarative_context=*/TRUE))) &&
                !rp->is_trivial_copy_function) {
            result = FALSE;
            break;
          }  /* if */
        }  /* for */
        if (result) {
          /* Now check for assignment operators.  Unlike the copy/move
             constructor, there is currently no quick way to eliminate the
             case of a user-provided copy/move assignment operator. */
          sym = cssp->assignment_operator;
          if (sym != NULL && symbol_is(sym, sk_overloaded_function)) {
            is_list = TRUE;
            sym = sym->variant.overloaded_function.symbols;
          } else {
            is_list = FALSE;
          }  /* if */
          for (; sym != NULL; sym = is_list ? sym->next : NULL) {
            a_routine_ptr         rp;
            a_boolean             is_move;
            a_type_qualifier_set  tqs;
            if (symbol_is(sym, sk_function_template)) continue;
            check_assertion(symbol_is(sym, sk_member_function));
            rp = sym->variant.routine.ptr;
            if (rp->is_trivial_copy_function) {
              continue;
            } else if (rp->compiler_generated ||
                       routine_is_copy_or_move_assign_operator(
                                                        rp, &tqs, &is_move)) {
              result = FALSE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      } else {
        result = FALSE;
      }  /* if */
    } else {
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_trivially_copyable_type */


a_boolean is_pod_class(a_type_ptr  tp)
/*
Return TRUE if the given class type is a POD class type.  The definition of
POD changed between C++03 and C++11.
*/
{
  a_boolean  result;

  check_assertion(is_immediate_class_type(tp));
  if (cpp11_mode) {
    /* In C++11, a POD class is a standard-layout, trivial class type whose
       members do not have non-POD class types (or arrays thereof).  A trivial
       class type is a trivially copyable type that has a default constructor
       and no nontrivial default constructor. */
    a_class_symbol_supplement_ptr  cssp = class_symbol_supp(symbol_for(tp));
    result = cssp->standard_layout && has_trivial_default_constructor(cssp) &&
             is_trivially_copyable_type(tp);
    if (result) {
      /* Check that every field of class type (or array thereof) is of a POD
         class type. */
      a_field_ptr  fp = tp->variant.class_struct_union.field_list;
      for (; fp != NULL; fp = fp->next) {
        a_type_ptr ftp = skip_array_types(fp->type);
        ftp = skip_typerefs(ftp);
        if (is_immediate_class_type(ftp) && !is_pod_class(ftp)) {
          result = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  } else {
    result = class_symbol_supp(symbol_for(tp))->is_cpp03_POD;
  }  /* if */
  return result;
}  /* is_pod_class */


a_boolean is_literal_type(a_type_ptr tp)
/*
Return TRUE if the given type is a literal type.  If given type cannot be
incomplete class type (or an array thereof) in a valid program.
*/
{
  a_boolean  result;

  tp = skip_array_types(tp);
  tp = skip_typerefs(tp);
  if (is_scalar(tp) || is_any_reference(tp)) {
    result = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (gpp_mode && gnu_version >= 40800 && is_vector_type(tp)) {
    result = TRUE;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  } else if (is_immediate_class_type(tp)) {
    a_class_symbol_supplement_ptr
                 cssp = symbol_for(tp)->variant.class_struct_union.extra_info;
    if (tp->incomplete) {
      result = FALSE;
      expect_error();
    } else if (cssp->known_to_be_a_literal_type) {
      result = TRUE;
    } else if (cssp->known_not_to_be_a_literal_type) {
      result = FALSE;
    } else {
      set_literal_type_flag(tp);
      result = cssp->known_to_be_a_literal_type;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_literal_type */


a_boolean could_be_literal_type(a_type_ptr tp)
/*
Return TRUE if the given type is a literal type, a template parameter type, or
an error type (or an array thereof).
*/
{
  a_boolean  result;

  tp = skip_array_types(tp);
  tp = skip_typerefs(tp);
  if (is_template_param(tp) || is_error(tp) ||
      (is_immediate_class_type(tp) &&
       tp->variant.class_struct_union.is_nonreal_class)) {
    /* We cannot reliably tell whether the type is literal: Assume it might
       be. */
    result = TRUE;
  } else {
    result = is_literal_type(tp);
  }  /* if */
  return result;
}  /* could_be_literal_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

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


a_boolean is_char16_t_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of char16_t.
*/
{
  a_boolean  is_char16_t_array = FALSE;
  a_type_ptr elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    if (is_integral(elem_type)) {
      if (!char16_t_and_char32_t_are_keywords) {
        /* In C mode, or C++ mode when char16_t is not a distinct type.
           See if the element type is the appropriate integer kind for
           char16_t. */
        is_char16_t_array = elem_type->variant.integer.int_kind ==
                                                        targ_char16_t_int_kind;
      } else {
        /* In C++11 mode when char16_t is a distinct type.  Make sure this is
           a char16_t type. */
        is_char16_t_array = elem_type->variant.integer.char16_t_type;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_char16_t_array;
}  /* is_char16_t_array_type */


a_boolean is_char32_t_array_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of char32_t.
*/
{
  a_boolean  is_char32_t_array = FALSE;
  a_type_ptr elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    if (is_integral(elem_type)) {
      if (!char16_t_and_char32_t_are_keywords) {
        /* In C mode, or C++ mode when char32_t is not a distinct type.
           See if the element type is the appropriate integer kind for
           char32_t. */
        is_char32_t_array = elem_type->variant.integer.int_kind ==
                                                        targ_char32_t_int_kind;
      } else {
        /* In C++11 mode when char32_t is a distinct type.  Make sure this is
           a char32_t type. */
        is_char32_t_array = elem_type->variant.integer.char32_t_type;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_char32_t_array;
}  /* is_char32_t_array_type */


a_boolean is_string_type(a_type_ptr tp)
/*
Return TRUE if the given type is an array of character (any kind, including
wchar_t, char16_t, and char32_t).
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    result = is_general_character(elem_type);
  }  /* if */
  return result;
}  /* is_string_type */


a_boolean may_be_string_type(a_type_ptr  tp)
/*
Return TRUE if the given type is an array of character (any kind) or an array
of template parameter.
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  elem_type;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    elem_type = skip_typerefs(tp->variant.array.element_type);
    result = is_general_character(elem_type) || is_template_param(elem_type);
  }  /* if */
  return result;
}  /* may_be_string_type */


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


a_boolean is_class_struct_type(a_type_ptr tp)
/*
Return TRUE if the given type is a class or struct type.  Note that
this includes incomplete class or struct union types.
*/
{
  tp = skip_typerefs(tp);
  return is_class_or_struct(tp);
}  /* is_class_struct_type */


a_boolean is_real_class_type(a_type_ptr  tp)
/*
Return TRUE if the given type is a class, struct, or union type, but not a
nonreal class template instance.
*/
{
  tp = skip_typerefs(tp);
  return is_class_struct_union(tp) &&
         !tp->variant.class_struct_union.is_nonreal_class;
}  /* is_real_class_type */


a_boolean is_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a union type.
*/
{
  tp = skip_typerefs(tp);
  return is_union(tp);
}  /* is_union_type */

#if GNU_EXTENSIONS_ALLOWED

a_boolean is_transparent_union_type(a_type_ptr  tp)
/*
Return TRUE if the given type is a GNU C transparent union.
*/
{
  tp = skip_typerefs(tp);
  return is_union(tp) && tp->variant.class_struct_union.is_transparent;
}  /*is_transparent_union_type */

#if !STANDALONE_UTILITY_PROGRAM

static a_boolean transparent_union_has_field_type(a_type_ptr  union_type,
                                                  a_type_ptr  field_type)
/*
Return TRUE if the given transparent union type has a field of type
field_type.  Top-level qualifiers are ignored.
*/
{
  a_boolean    result = FALSE;
  a_field_ptr  field;

  union_type = skip_typerefs(union_type);
  check_assertion(is_union(union_type));
  field_type = skip_typerefs(field_type);
  field = union_type->variant.class_struct_union.field_list;
  for (; field != NULL; field = field->next) {
    a_type_ptr  tp = skip_typerefs(field->type);
    if (identical_types(tp, field_type)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* transparent_union_has_field_type */


static a_boolean transparent_union_match(a_type_ptr  tp1,
                                         a_type_ptr  tp2)
/*
Return TRUE if one of the types is a transparent union and the other type is
the type of a field in that union (ignoring top-level qualifiers).
*/
{
  a_boolean  result = FALSE;

  if (is_transparent_union_type(tp1)) {
    /* Check if tp1 has a field of type tp2. */
    result = transparent_union_has_field_type(tp1, tp2);
  } else if (is_transparent_union_type(tp2)) {
    /* The reverse case is entirely similar. */
    result = transparent_union_has_field_type(tp2, tp1);
  }  /* if */
  return result;
}  /* transparent_union_match */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* GNU_EXTENSIONS_ALLOWED */
      
a_boolean is_aggregate_or_union_type(a_type_ptr tp)
/*
Return TRUE if the given type is a union or aggregate type (array, struct,
or union; 3.1.2.5.  Also class, in C++).  Note that this includes incomplete
array, class, struct, and union types.  Also note that it doesn't match
the C++ definition of "aggregate" for class types; see is_aggregate_type
instead.
*/
{
  tp = skip_typerefs(tp);
  return(is_aggregate_or_union(tp));
}  /* is_aggregate_or_union_type */


a_boolean is_aggregate_type(a_type_ptr tp)
/*
Return TRUE if the given type is an aggregate type in the C++ sense
(C++ standard, [dcl.init.aggr]).  That means an array or a class with
no user-provided constructors, etc.
*/
{
  a_boolean is_aggr = FALSE;

  tp = skip_typerefs(tp);
  if (is_array(tp)) {
    is_aggr = TRUE;
  } else if (is_immediate_class_type(tp)) {
    if (class_symbol_supp(symbol_for(tp))->is_class_aggregate) {
      is_aggr = TRUE;
    }  /* if */
  }  /* if */
  return is_aggr;
}  /* is_aggregate_type */


a_boolean is_std_initializer_list_type(a_type_ptr tp)
/*
Return TRUE if the given type is an instance of std::initializer_list.  (Also
TRUE for the prototype instantiation.)
*/
{
  tp = skip_typerefs(tp);
  return is_immediate_class_type(tp) &&
         class_type_supp(tp)->is_initializer_list;
}  /* is_std_initializer_list_type */


a_boolean is_std_nothrow_type(a_type_ptr tp)
/*
Return TRUE if the given type is the std::nothrow_t type.
*/
{
  a_boolean  result = FALSE;
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp) && symbol_for_namespace_std != NULL) {
    a_symbol_ptr  sym = symbol_for(tp);
    if (!sym->is_class_member &&
        sym->parent.namespace_ptr == 
                        symbol_for_namespace_std->variant.namespace_info.ptr) {
      /* We know tp is a class type that is a member of namespace std.  Now
         check whether its name is "nothrow_t". */
      if (strcmp(sym->header->identifier, "nothrow_t") == 0) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_std_nothrow_type */


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

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_template_not_cli_generic_param_type(a_type_ptr tp)
/*
Return TRUE if the given type is a template parameter type, but not
a C++/CLI generic parameter type.
*/
{
  tp = skip_typerefs(tp);
  return is_template_param(tp) && !is_cli_generic_param(tp);
}  /* is_template_not_cli_generic_param_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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


a_boolean is_auto_type(a_type_ptr tp)
/*
Return TRUE if the indicated type is a special template parameter type used to
represent "auto" or "decltype(auto)".  No typerefs are stripped before
checking for that.
*/
{
  a_boolean result = FALSE;

  if (is_template_param(tp) &&
      tp->variant.template_param.kind ==
                                      (a_template_param_type_kind)tptk_param &&
      tp->variant.template_param.extra_info->coordinates.depth ==
                                                     AUTO_TYPE_NESTING_DEPTH) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_auto_type */


a_boolean is_or_has_volatile_qualified_type(a_type_ptr tp)
/*
Returns TRUE if the given type is volatile-qualified, is a class/struct/union
type with any volatile non-static members, or is an array whose underlying
element type satisfies one of those criteria.
*/
{
  a_boolean volatile_found = FALSE;
  if (is_array_type(tp)) {
    tp = underlying_array_element_type(tp);
  }  /* if */
  if (is_volatile_qualified_type(tp)) {
    volatile_found = TRUE;
  } else if (is_class_struct_union_type(tp)) {
    tp = skip_typerefs(tp);
    volatile_found = tp->variant.class_struct_union.any_volatile_member;
  }  /* if */
  return volatile_found;
}  /* is_or_has_volatile_qualified_type */


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


a_type_ptr skip_array_types(a_type_ptr  tp)
/*
If tp is an array type, return its underlying element type.  Otherwise, return
tp itself.
*/
{
  return is_array_type(tp) ? underlying_array_element_type(tp) : tp;
}  /* skip_array_types */


a_boolean has_any_unknown_specified_bound(a_type_ptr  array_type)
/*
Return TRUE if the given array type has a specified bound that is unknown
(a template parameter or a run-time quantity).  For multi-level arrays,
return TRUE if this is the case for any of the bounds.
*/
{
  a_boolean  result = FALSE;

  array_type = skip_typerefs(array_type);
  check_assertion(is_array(array_type));
  do {
    if (has_unknown_specified_bound(array_type)) {
      result = TRUE;
      break;
    } else {
      array_type = skip_typerefs(array_type->variant.array.element_type);
    }  /* if */
  } while (is_array(array_type));
  return result;
}  /* has_any_unknown_specified_bound */


a_boolean has_any_zero_bound(a_type_ptr  array_type)
/*
Return TRUE if the given array type has a zero bound.  For multi-level arrays,
return TRUE if this is the case for any of the bounds.
*/
{
  a_boolean  result = FALSE;

  array_type = skip_typerefs(array_type);
  check_assertion(is_array(array_type));
  do {
    if (array_type->variant.array.bound_is_zero) {
      result = TRUE;
      break;
    } else {
      array_type = skip_typerefs(array_type->variant.array.element_type);
    }  /* if */
  } while (is_array(array_type));
  return result;
}  /* has_any_zero_bound */


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
    check_assertion_str(!has_unknown_specified_bound(array_type),
                        "num_array_elements: array with unknown bound");
    elems_this_level = array_type->variant.array.variant.number_of_elements;
    check_assertion_str(elems_this_level > 0 ||
                        array_type->variant.array.bound_is_zero,
                        "num_array_elements: array with unspecified bound");
    num_elements *= elems_this_level;
    array_type = array_type->variant.array.element_type;
    array_type = skip_typerefs(array_type);
    if (!is_array(array_type)) break;
  }  /* for */
  return num_elements;
}  /* num_array_elements */


a_boolean constant_fully_initializes_type(a_constant_ptr con,
                                          a_type_ptr     type)
/*
Returns TRUE if the specified constant fully initializes the specified type.
Generally this verifies that the constant and the type have the same "shape",
that is, the relevant types have the same array dimensions (i.e., number of
dimensions as well as dimensions themselves).  The exception is that a
ck_string constant is considered to fully initialize a character array.
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  con_type;

  if (con->kind == (a_constant_repr_kind)ck_designator) {
    /* Skip any designators. */
    check_assertion(con->next != NULL);
    con = con->next;
  }  /* if */
  if (con->kind == (a_constant_repr_kind)ck_init_repeat) {
    /* Use the repeated constant for type comparisons. */
    con = con->variant.init_repeat.constant;
  }  /* if */
  con_type = skip_typerefs(con->type);
  type = skip_typerefs(type);
  if (con_type == type) {
    /* Types are the same, so they must have the same "dimension". */
    return TRUE;
  } else if (!is_array(type) && !is_array(con_type)) {
    /* Neither type is an array, so they have the same "dimension". */
    result = TRUE;
  } else if (is_array(type) && is_array(con_type)) {
    if (con->kind == (a_constant_repr_kind)ck_string &&
               is_character_type(skip_typerefs(
                                          type->variant.array.element_type))) {
      /* A ck_string fully initializes a character array. */
      result = TRUE;
    } else if (type->variant.array.variant.number_of_elements ==
                          con_type->variant.array.variant.number_of_elements) {
      /* Both are array types and have the same number of elements; check the
         element types. */
      check_assertion(con->kind == (a_constant_repr_kind)ck_aggregate);
      con = con->variant.aggregate.first_constant;
      result = constant_fully_initializes_type(con,
                                             type->variant.array.element_type);
    }  /* if */
  }  /* if */
  return result;
}  /* constant_fully_initializes_type */

#if GNU_VECTOR_TYPES_ALLOWED

a_targ_size_t num_vector_elements(a_type_ptr vector_type)
/*
Return the number of elements in a vector type.
*/
{
  a_targ_size_t num_elements;
  a_type_ptr    element_type;

  vector_type = skip_typerefs(vector_type);
  element_type = skip_typerefs(vector_type->variant.vector.element_type);
  check_assertion(vector_type->kind == (a_type_kind)tk_vector &&
                  element_type->size != 0);
  num_elements = vector_type->size / element_type->size;
  return num_elements;
}  /* num_vector_elements */

#endif /* GNU_VECTOR_TYPES_ALLOWED */

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
a pointer or a reference type, or a C++/CLI handle or tracking reference.
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


a_type_ptr f_underlying_type_of_derived_type(a_type_ptr  type,
                                             a_boolean   *p_is_derived_type)
/*
If type is a derived type, return the type from which it is derived.
Otherwise, return NULL.  If p_is_derived_type is non-NULL, *p_is_derived_type
is set to TRUE if the given type is a derived type.  This allows the caller to
distinguish non-derived types from partially constructed derived types, both
of which result in a NULL return value.  See the macro
underlying_type_of_derived_type, which provides a convenient way of
calling this function for the common case of the second argument
being NULL.
*/
{
  a_boolean  is_derived = TRUE;

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
      is_derived = FALSE;
      break;
  }  /* switch */
  if (p_is_derived_type != NULL) {
    *p_is_derived_type = is_derived;
  }  /* if */
  return type;
}  /* f_underlying_type_of_derived_type */


a_boolean is_possibly_qualified_typedef(a_type_ptr  tp)
/*
Return TRUE if the given type is a typedef type, possibly with other typeref
entries (e.g., qualifiers or decltype/typeof constructs) on top of it.
*/
{
  a_boolean  result = FALSE;

  while (tp->kind == (a_type_kind)tk_typeref) {
    if (typeref_is_typedef(tp)) {
      result = TRUE;
      break;
    }  /* if */
    tp = tp->variant.typeref.type;
  }  /* while */
  return result;
}  /* is_possibly_qualified_typedef */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean check_for_vla_in_pointer_to_member(a_type_ptr         type,
                                             a_source_position  *pos)
/*
Return TRUE and issue a diagnostic at the given position if the given type
or one of its underlying components is a pointer-to-member type to a variably
modified member type.
*/
{
  a_boolean  result = FALSE;

  if (il_header.vla_used) {
    while (type != NULL) {
      if (is_ptr_to_member(type)) {
        a_type_ptr  member_type = type->variant.ptr_to_member.type;
        if (is_variably_modified_type(member_type)) {
          pos_ty_error(ec_ptr_to_member_of_vla_type, pos, member_type);
          set_type_kind(type, (a_type_kind)tk_error);
          result = TRUE;
        }  /* if */
        break;
      }  /* if */
      type = underlying_type_of_derived_type(type);
    }  /* while */
  }  /* if */
  return result;
}  /* check_for_vla_in_pointer_to_member */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_type_ptr type_specifier_of_type(a_type_ptr type)
/*
Find the specifiers type at the bottom of a type, and return a pointer to
it.  For example, from "array[3] of pointer to const int" one gets back
"const int".
*/
{
  a_type_ptr return_type = type;

  while (type != NULL) {
    /* Remove type qualifiers to see what is underneath.  If what is underneath
       is a derived type we keep going. */
    type = skip_typerefs_not_typedefs(type);
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
    return_type = type;
  }  /* while */
found_specifier_type:
  return return_type;
}  /* type_specifier_of_type */

#if USER_CONTROL_OF_STRUCT_PACKING && \
    (GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED)

a_targ_alignment f_alignment_of_type(a_type_ptr  tp)
/*
Return the alignment of the given type.  Normally, this function should only
be called by using the macro alignment_of_type.
*/
{
  /* Skip any typerefs that do not affect the alignment. */
  while (!tp->alignment_set_explicitly &&
         tp->kind == (a_type_kind)tk_typeref) {
    tp = tp->variant.typeref.type;
  }  /* while */
  return tp->alignment;
}  /* f_alignment_of_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean type_explicitly_aligned(a_type_ptr  tp)
/*
Return TRUE if the given type is explicitly aligned, including possibly through
a typedef.
*/
{
  while (!tp->alignment_set_explicitly &&
         tp->kind == (a_type_kind)tk_typeref) {
    tp = tp->variant.typeref.type;
  }  /* while */
  return tp->alignment_set_explicitly;
}  /* type_explicitly_aligned */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* USER_CONTROL_OF_STRUCT_PACKING && ... */

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

#if NAMED_ADDRESS_SPACES_ALLOWED

a_boolean first_address_space_encloses_second(a_type_qualifier_set  q1,
                                              a_type_qualifier_set  q2)
/*
Return TRUE if and only if the address space embedded in q1 encloses the
address space embedded in q2.  (An address space is considered to enclose
itself.)
*/
{
  a_named_address_space_id
             nas_id_1 = named_address_space_from_qualifier_set(q1),
             nas_id_2 = named_address_space_from_qualifier_set(q2);
  a_boolean  result = FALSE;

  do {
    if (nas_id_2 == nas_id_1) {
      result = TRUE;
      break;
    }  /* if */
    nas_id_2 = named_address_spaces[nas_id_2].parent_id;
  } while (nas_id_2 != -1);
  return result;
}  /* first_address_space_encloses_second */

#if !STANDALONE_UTILITY_PROGRAM

a_type_ptr type_without_named_address_space_qualifiers(a_type_ptr  tp)
/*
Return a type like the given type except it is not qualified with a named
address space.
*/
{
  a_type_qualifier_set  qualifiers = get_type_qualifiers(tp);
  a_type_ptr            result = make_unqualified_type(tp);

  return make_qualified_type(result, simple_qualifiers(qualifiers));
}  /* type_without_named_address_space_qualifiers */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

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

#if UPC_EXTENSIONS_ALLOWED

a_upc_block_size f_get_upc_block_size(a_type_ptr  tp,
                                      a_boolean   top_level)
/*
Return the UPC block size for the type tp.  If top_level is FALSE and tp
is an array, this means checking the element type; top_level is usually TRUE
in C mode (3.1.2.5).  As a general rule, macros get_upc_block_size and
get_underlying_upc_block_size should be used instead of calling this routine
directly.
*/
{
  a_upc_block_size  result = UPC_BLOCK_SIZE_NONE;

  for (;;) {
    if (tp->kind == (a_type_kind)tk_typeref) {
      /* May be a typedef or a qualification. */
      if (typeref_is_shared_qualified(tp)) {
        result = tp->variant.typeref.extra_info->upc_block_size;
        break;
      } else {
        tp = tp->variant.typeref.type;
      }  /* if */
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
  /* Did not find anything earlier, so return 0 block size */
  return result;
}  /* f_get_upc_block_size */


a_boolean is_underlying_shared_qualified_type(a_type_ptr  tp)
/*
Return TRUE if this is fundamentally a shared type, i.e. if a pointer to
this object must be a pointer-to-upc-shared.
*/
{
  a_boolean  result;

  if (is_array_type(tp)) {
    /* This is an array: Check the underlying element type (if any). */
    tp = underlying_array_element_type(tp);
    result = (tp != NULL && is_shared_qualified_type(tp));
  } else {
    /* Not an array: Check the type itself. */
    result = is_shared_qualified_type(tp);
  }  /* if */
  return result;
} /* is_underlying_shared_qualified_type */


a_boolean is_shared_void_star_type(a_type_ptr tp)
/*
Returns TRUE if the specified type is a UPC shared void*.
*/
{
  a_boolean  result = FALSE;

  if (is_pointer_type(tp)) {
    tp = type_pointed_to(tp);
    if (is_underlying_shared_qualified_type(tp)) {
      tp = skip_typerefs(tp);
      if (is_void_type(tp)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_shared_void_star_type */

#if !STANDALONE_UTILITY_PROGRAM

void fixup_upc_block_size(a_type_ptr  tp)
/*
Fix up the UPC block size for a type for which pure block allocation was
specified.  This could be the given type tp, or a type on which tp is
based (through array and pointer constructs only).
*/
{
  a_targ_size_t     num_elements;
  a_type_ptr        elem_type;
  a_boolean         bad_block_size;
  a_upc_block_size  bsize;
  a_type_ptr        orig_type = skip_typerefs(tp);

  while (tp != NULL && (is_pointer_type(tp) ||
                        (is_array_type(tp) &&
                        !is_underlying_shared_qualified_type(tp)))) {
    /* Find shared type at the bottom (if any).  If the shared type is
       an array type (with no underlying shared element type), use the
       underlying element type. */
    while (tp != NULL && is_pointer_type(tp)) {
      tp = type_pointed_to(skip_typerefs(tp));
    }  /* while */
    if (tp != NULL && is_array_type(tp) &&
        !is_underlying_shared_qualified_type(tp)) {
      tp = underlying_array_element_type(tp);
    }  /* if */
  }  /* while */
  if (tp != NULL) {
    bsize = get_underlying_upc_block_size(tp);
    if (bsize == UPC_BLOCK_SIZE_BLOCK) {
      /* A shared [*] array type cannot be the basis for a pointer type or
         for an array of unspecified length. */
      if (is_pointer_type(orig_type)) {
        pos_error(ec_bad_upc_shared_pointer_layout_qualifier, &error_position);
      } else if (is_array_type(orig_type) && is_incomplete_type(orig_type)) {
        pos_error(ec_bad_upc_shared_array_layout_qualifier, &error_position);
      }  /* if */
      if (is_array_type(tp)) {
        /* Handle special case of indeterminate array size that is
           used as a pointer type.  */
        check_assertion(!tp->variant.array.is_variable_size_array);
        if (tp->variant.array.variant.number_of_elements == 0) {
          tp = array_element_type(tp);
        }  /* if */
      }  /* if */
      if (is_array_type(tp)) {
        /* The block size is the product of all the array dimensions, divided
           by THREADS.  The division only needs to be done if the number of
           threads is specified at compile time. */
        num_elements = num_array_elements(tp);
        elem_type = underlying_array_element_type(tp);
        if (!upc_dynamic_threads()) {
          /* Make sure the result is rounded up. */
          num_elements = (num_elements + upc_num_threads - 1)
                                              / (a_targ_size_t)upc_num_threads;
        }  /* if */
      } else {
        elem_type = tp;
        num_elements = 1;
      }  /* if */
      while (elem_type->kind == (a_type_kind)tk_typeref &&
             !typeref_is_shared_qualified(elem_type)) {
        elem_type = elem_type->variant.typeref.type;
      }  /* while */
      /* upc_block_size_too_large issues an error if the given block size
         is too large. */
      bad_block_size = upc_block_size_too_large(
                                         (a_host_large_unsigned)num_elements);
      elem_type->variant.typeref.extra_info->upc_block_size =
                                     (long)(bad_block_size ? 1 : num_elements);
    }  /* if */
  }  /* if */
}  /* fixup_upc_block_size */


a_boolean is_underlying_threads_dimensioned_array_type(a_type_ptr  tp)
/*
Return TRUE if this is an array type that either is dimensioned
to a THREADS multiple or has an element type that is a THREADS
dimensioned array type.
*/
{
  a_boolean  result = FALSE;

  if (tp != NULL && upc_dynamic_threads()) {
    /*lint --e{850} tp modified in loop */
    for (; tp != NULL && is_array_type(tp);
           tp = tp->variant.array.element_type) {
      tp = skip_typerefs(tp);
      if (tp->variant.array.is_threads_dimension) {
        result = TRUE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
} /* is_underlying_threads_dimensioned_array_type */


a_targ_size_t upc_local_type_size(a_type_ptr  tp)
/*
Calculates the locally allocated size for a shared array.  For
anything else, returns the regular size.
*/
{
  a_type_ptr     base_type = skip_typerefs(tp);
  a_targ_size_t  length = base_type->size;

  /* Make sure the allocated size of a shared array is enough to cover
     the maximum possible number of elements on a single thread.  If the
     number of local elements is a multiple of the block size, there is no
     problem; otherwise, we need to add some elements to cover the skew. */
  if (is_underlying_shared_qualified_type(base_type) &&
      is_array_type(base_type)) {
    a_type_ptr        elem_type = underlying_array_element_type(base_type);
    a_upc_block_size  block_size = get_upc_block_size(elem_type);
    a_targ_size_t     elem_size = skip_typerefs(elem_type)->size;
    a_targ_size_t     nelems = length / elem_size;
    a_targ_size_t     ref_thread_count;
    a_targ_size_t     blocks_per_thread;

    if (block_size == UPC_BLOCK_SIZE_INDEFINITE) {
      check_assertion_str2(
        !is_underlying_threads_dimensioned_array_type(base_type),
        "Cannot find local size of indefinite block size ",
        "threads-dimensioned array");
    } else {
      if (!upc_dynamic_threads()) {
        /* In this case, nelems is the total number of elements, because
           length is the total array length.  Determine how many elements
           do not fit into neat block_size*threads slices.  This will be at
           most one extra block per thread.  */
        ref_thread_count = upc_num_threads;
      } else {
        /* In this case, nelems is the per-thread number of elements, so
           use a thread count of 1 in the calculations. */
        ref_thread_count = 1;
      }  /* if */
      blocks_per_thread = nelems / (ref_thread_count * block_size);
      if (nelems - (blocks_per_thread * ref_thread_count * block_size) != 0) {
        ++blocks_per_thread;
      }  /* if */
      nelems = blocks_per_thread * block_size;
    }  /* if */
    length = nelems * elem_size;
  }  /* if */
  return length;
}  /* upc_local_type_size */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* UPC_EXTENSIONS_ALLOWED */

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
    a_boolean  is_class_type;
    tp = skip_typerefs(tp);
    is_class_type = is_class_struct_union(tp);
    if (is_class_type) {
      qualifiers = class_type_supp(tp)->qualifiers;
    }  /* if */
    if (is_class_type && qualifiers != TQ_NONE) {
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
                                         a_boolean   user_provided_only,
                                         a_boolean   nontrivial_only)
/*
Return TRUE if the type pointed to by tp is a non-POD class type with a
default constructor (or an array thereof).  When user_provided_only is TRUE,
the function returns TRUE if class has a user-provided default constructor.
When nontrivial_only is TRUE, it returns TRUE if the class has a nontrivial
default constructor (user-provided or not).  If both flags are FALSE, it also
considers the trivial_default_constructor pointer in the class symbol
supplement.  (Both flags should not be TRUE at the same time.)
This function is called in C++ mode only, and only through one of the macros
provided in types.h.
*/
{
  a_boolean                      has_default_ctor = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  check_assertion(!user_provided_only || !nontrivial_only);
  if (is_array_type(tp)) {
    tp = underlying_array_element_type(tp);
  }  /* if */
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    /* It's a class type or an array of class type. */
    complete_class_type_is_needed(tp);
    cssp = symbol_supplement_for_class(tp);
    if (user_provided_only) {
      has_default_ctor = cssp->has_user_provided_default_constructor;
    } else if (cssp->has_nontrivial_default_constructor) {
      /* Class has a nontrivial default constructor. */
      has_default_ctor = TRUE;
    } else if (cssp->trivial_default_constructor != NULL) {
      /* Class has an implicitly declared trivial default constructor. */
      if (!nontrivial_only) has_default_ctor = TRUE;
    }  /* if */
  }  /* if */
  return has_default_ctor;
}  /* f_type_has_default_constructor */


a_boolean type_has_nontrivial_destructor(a_type_ptr  tp)
/*
Return TRUE if the given type is a class type with a nontrivial destructor,
or an array thereof.
*/
{
  a_boolean                      has_nontrivial_dtor = FALSE;
  a_class_symbol_supplement_ptr  cssp;

  if (is_array_type(tp)) {
    tp = underlying_array_element_type(tp);
  }  /* if */
  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    /* It's a class type or an array of class type. */
    complete_class_type_is_needed(tp);
    cssp = symbol_supplement_for_class(tp);
    if (has_nontrivial_destructor(cssp)) has_nontrivial_dtor = TRUE;
  }  /* if */
  return has_nontrivial_dtor;
}  /* type_has_nontrivial_destructor */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean cli_type_has_public_default_constructor(a_type_ptr	tp)
/*
Return TRUE if tp has a public default constructor.  This routine should
only be called on C++/CLI reference type or interface (although it will
always return FALSE for interfaces).
*/
{
  a_boolean				result = FALSE;
  a_class_symbol_supplement_ptr		cssp;
  a_symbol_ptr				sym;
  a_boolean				is_list = FALSE;

  check_assertion(is_cli_ref_or_interface_class_type(tp));
  cssp = symbol_supplement_for_class(tp);
  sym = cssp->constructor;
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_list = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  }  /* if */
  /* If the constructor symbol is an overload set, loop through the members
     of the set. */
  for (; sym != NULL; sym = is_list ? sym->next : NULL) {
    a_routine_ptr	rp;
    a_param_type_ptr	ptp;
    check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
    rp = sym->variant.routine.ptr;
    ptp = rp->type->variant.routine.extra_info->param_type_list;
    /* If we find an entry with no parameter list, that is the default
       constructor.  This is sufficient for the purposes of this routine
       because CLI constructors cannot have default arguments. */
    if (ptp == NULL) {
      /* We found an entry -- make sure it is public. */
      if (access_for_symbol(sym) == (an_access_specifier)as_public) {
        result = TRUE;
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* cli_type_has_public_default_constructor */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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

  if (C_dialect == C_dialect_cplusplus) {
    derived_class = skip_typerefs(derived_class);
    base_class = skip_typerefs(base_class);
    if (instantiate_if_necessary &&
        !same_entities(derived_class, base_class)) {
#if !STANDALONE_UTILITY_PROGRAM
      /* Force instantiation of the derived type if it is an uninstantiated
         template class.  This is necessary so that we can see what its base
         classes are.  Note that this can potentially force instantiation
         of the base class as well. */
      complete_class_type_is_needed(derived_class);
#else /* STANDALONE_UTILITY_PROGRAM */
      unexpected_condition();
#endif /* !STANDALONE_UTILITY_PROGRAM */
    }  /* if */
    /* Check that both classes are complete, i.e., that their definitions have
       been seen. */
    if (class_type_supp(derived_class)->assoc_scope != NULL &&
        class_type_supp(base_class)->assoc_scope != NULL) {
      /* See if the base class appears on the base class list for the derived
         type.  The base class list contains all base classes, both direct
         and indirect. */
      for (bcp = base_classes_of(derived_class);
           bcp != NULL;
           bcp = bcp->next) {
        if (same_entities(bcp->type, base_class)) break;
      }  /* for */
      if (microsoft_bugs && bcp != NULL && bcp->ambiguous && !bcp->direct) {
        /* The Microsoft compiler allows a cast or conversion to an ambiguous
           base class in some cases.  If one of the instances of the
           ambiguous base is a direct class, it is used.  Make sure that
           in such a case we return the direct base class here so that it is
           possible to choose to use it later. */
        a_base_class_ptr bcp2;
        for (bcp2 = bcp->next; bcp2 != NULL; bcp2 = bcp2->next) {
          if (bcp2->direct && same_entities(bcp2->type, base_class)) {
            bcp = bcp2;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
  return bcp;
}  /* find_base_class_of_full */


a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                    a_type_ptr base_class)
/*
Like find_base_class_full, with instantiate_if_necessary set to TRUE if called
from the front end, and FALSE if called from a back end.
*/
{
  a_base_class_ptr  bcp = find_base_class_of_full(
                                   derived_class, base_class,
                                   /*instantiate_if_necessary=*/in_front_end);
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

  bcp = base_classes_of(derived_class);
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct && same_entities(bcp->type, base_class_type)) break;
  }  /* for */
  return bcp;
}  /* find_direct_base_class_of */

#if !STANDALONE_UTILITY_PROGRAM

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
    fputs("cannot find base class", f_debug);
    db_base_class(base_class, /*show_offset=*/FALSE);
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


a_base_class_ptr corresp_base_class(a_base_class_ptr base_class,
                                    a_base_class_ptr old_class_as_base_of_new)
/*
base_class is a base class of the type of old_class_as_base_of_new.
Find the corresponding base class under the derived class of
old_class_as_base_of_new and return a pointer to it.   The base class
must be found.

In other words:

  base_class:               A in B
  old_class_as_base_of_new:      B in C
  returned base class:      A      in C

*/
{
  a_base_class_ptr new_bcp;
  a_base_class_ptr disambiguator;

  check_assertion(base_class->derived_class == old_class_as_base_of_new->type);
  disambiguator = find_disambiguator(old_class_as_base_of_new, base_class);
  new_bcp = corresponding_base_class(base_class,
                                     old_class_as_base_of_new->derived_class,
                                     disambiguator);
  return new_bcp;
}  /* corresp_base_class */


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
  if (identical_types(class_1, class_2) ||
      find_base_class_of(class_1, class_2) != NULL) {
    is_same_or_base = TRUE;
  }  /* if */
  return is_same_or_base;
}  /* is_same_class_or_base_class_thereof */


a_boolean same_or_related_class_types(a_type_ptr type_1,
                                      a_type_ptr type_2)
/*
Return TRUE if the cv-unqualified versions of type_1 and type_2 are classes
and they are either the same type or one is a base class of the other.
*/
{
  a_boolean same_or_related = FALSE;

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (is_immediate_class_type(type_1) &&
      is_immediate_class_type(type_2) &&
      (identical_types(type_1, type_2) ||
       find_base_class_of(type_1, type_2) != NULL ||
       find_base_class_of(type_2, type_1) != NULL)) {
    same_or_related = TRUE;
  }  /* if */
  return same_or_related;
}  /* same_or_related_class_types */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean any_nonpublic_steps_in_derivation(a_base_class_ptr bcp)
/*
Return TRUE if any of the derivation steps between bcp->type and
bcp->derived_class is as_protected or as_private.
*/
{
  a_base_class_derivation_ptr derivation = preferred_derivation_of(bcp);
  a_boolean                   nonpublic_step_found = FALSE;
  a_derivation_step_ptr       step;

  for (step = derivation->path; step != NULL; step = step->next) {
    if (step->base_class->derivation->access !=
                                              (an_access_specifier)as_public) {
      nonpublic_step_found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return nonpublic_step_found;
}  /* any_nonpublic_steps_in_derivation */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                   a_type_ptr       type_2,
                                   a_boolean        *baseward_cast,
                                   a_base_class_ptr *bcp)
/*
type_1 and type_2 are pointer types.  Check to see if they are pointers to
related class types, and return TRUE if so.  If they are, set *baseward_cast
if type_1 --> type_2 is a baseward cast, and set *bcp to point to the base
class entry that shows the relationship.  Called from the macro
related_class_pointers.  Can also be called for C++/CLI handles from the
macro related_class_pointers_or_handles.
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
  a_std_conv_descr  std_conv;

  db_enter(5, "type_masks_handler_param_type");
  /* "Reference" on top of a type is ignored for handlers as are type
      qualifiers.  C++/CLI tracking references are not allowed. */
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
       handler for a pointer-to-base-class.  C++/CLI handles are allowed
       in place of pointers. */
    if (types_are_both_pointers_or_both_handles(type_1, type_2)) {
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
      if (impl_pointer_conversion(type_2, /*source_is_constant=*/FALSE,
                                  /*source_is_string_literal=*/FALSE,
                                  /*source_is_function=*/FALSE,
                                  (a_constant_ptr)NULL, type_1,
                                  /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                  /*suppress_extensions=*/TRUE,
                                  ec_no_error, &std_conv)) {
        masked = TRUE;
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled &&
               is_handle_type(type_1) && is_handle_type(type_2)) {
      /* A C++/CLI handle type masks another handle type if the latter can be
         implicitly converted to the former. */
      if (impl_handle_conversion(type_2, type_1,
                                 /*allow_qualifier_or_eh_mismatch=*/FALSE,
                                 (a_std_conv_descr *)NULL)) {
        masked = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
  a_type_ptr        underlying_elem_type;
  a_targ_size_t     temp, temp2;
  a_targ_alignment  underlying_elem_alignment;
  a_type_ptr        elem_type;
  a_boolean         okay = TRUE;

  db_enter(5, "set_array_type_size");

  underlying_elem_type = underlying_array_element_type(array_type);
  /* The alignment should be retrieved before applying a skip_typerefs,
     in case a GNU attribute was used to modify the alignment of a
     typedef type. */
  underlying_elem_alignment = alignment_of_type(underlying_elem_type);
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
    array_type->incomplete = TRUE;
    array_type->size = 0;
    array_type->alignment = 1;
  } else {
    /* Get the number of elements.  Note that this is zero for an incomplete
       type like int a[]. */
    if (!has_unknown_specified_bound(array_type)) {
      temp = array_type->variant.array.variant.number_of_elements;
    } else {
      /* We don't know the element count because it is not a constant value.
         Set the size as though the element count were 1. */
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
    }  /* if */
    /* Determine whether the array is now complete.  (Note that, in somewhat
       unusual cases, this may turn a previously complete array type back
       into an incomplete type.) */
    array_type->incomplete =
                    (is_incomplete(elem_type) ||
                     (temp == 0 && !array_type->variant.array.bound_is_zero));
    temp2 = elem_type->size;
    /* Normally, element types cannot have size zero.  In GNU modes, however,
       there are zero-length arrays, zero-sized classes, and x[][] parameters.
       */
    check_assertion_str(temp2 != 0 ||
                        (gnu_mode && (!is_incomplete_type(elem_type) ||
                                      is_array_type(elem_type))),
                        "set_array_type_size: bad element type");
    /* Check whether or not the multiplication will overflow.  Note that we 
       avoid dividing by temp, since it may be zero for an incomplete type.
       temp2 can be zero too if the element type is a GNU C zero-length
       array. */
    if (temp2 != 0 && temp > targ_size_t_max / temp2) {
      if (!suppress_error) pos_error(ec_array_size_too_large, &error_position);
      set_type_kind(array_type, (a_type_kind)tk_error);
      set_type_size(array_type);
      okay = FALSE;
#if UPC_EXTENSIONS_ALLOWED
    } else if (upc_mode &&
               array_type->variant.array.is_threads_dimension &&
               is_underlying_threads_dimensioned_array_type(elem_type)) {
      /* There cannot be more than one THREADS dimension in an array */
      if (!suppress_error) {
        pos_error(ec_duplicate_threads_dim, &error_position);
      }  /* if */
      set_type_kind(array_type, (a_type_kind)tk_error);
      set_type_size(array_type);
      okay = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
    } else {
      /* Now that we know the multiplication will not overflow, compute the
         array size. */
      array_type->size = temp*temp2;
      /* The alignment for the array is the same as the alignment for the
         elements. */
      array_type->alignment = underlying_elem_alignment;
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* set_array_type_size */


a_targ_alignment check_explicit_enum_alignment(a_type_ptr       type,
                                               a_targ_alignment base_alignment)
/*
Verify that any explicit alignment requirements on the specified enum type are
valid and return the resulting alignment of the type.  This is used in cases
where the alignment has already been explicitly specified (presumably by an
attribute) but the alignment of the underlying type had not been determined
yet.  base_alignment is the alignment of the underlying type for the enum.
*/
{
  a_targ_alignment result = base_alignment;

  check_assertion(is_enum_type(type));
#if USER_CONTROL_OF_STRUCT_PACKING
  if (type->alignment_set_explicitly && !(gnu_mode && !clang_mode)) {
    /* An explicit alignment can be set on enum types; verify that it
       is at least as large as the alignment for the underlying type.
       GCC (but not clang) appears not to perform this check. */
    if (type->alignment >= base_alignment) {
      result = type->alignment;
    } else {
      if (microsoft_mode) {
        /* Microsoft seems to ignore explicit alignments in this case. */
        result = type->alignment;
        pos_diagnostic(es_warning,
                       ec_invalid_alignment_reducing_attr,
                       &type->source_corresp.decl_position);
      } else {
        pos_diagnostic(es_discretionary_error,
                       ec_invalid_alignment_reducing_attr,
                       &type->source_corresp.decl_position);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  return result;
}  /* check_explicit_enum_alignment */


void set_type_size(a_type_ptr type_ptr)
/*
Compute and set the size of the type pointed to by type_ptr.  If it is already
set, leave it alone.  Also compute and set the alignment requirement.
*/
{
  a_targ_size_t    size = type_ptr->size;
  a_targ_alignment alignment;

  db_enter(5, "set_type_size");
  /* If the size is set already, leave it alone.  (Zero-length arrays still
     need their alignment set even though their size is already set.) */
  if (size == 0
#if GNU_EXTENSIONS_ALLOWED
      && !(gnu_mode && is_immediate_class_type(type_ptr))
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                         ) {
    alignment = 1;  /* Default */
    switch(type_ptr->kind) {
      case tk_error:
      case tk_unknown:
      case tk_template_param:
        /* Use an arbitrary non-zero size for an error type, a template
           parameter type (which is a placeholder), or an unknown type. */
        size = 1;
        break;
      case tk_routine:
      case tk_void:
      case tk_typeref:
        /* These stay zero; they have no size directly.  However, a function
           type is considered complete. */
        break;
      case tk_integer:
        get_integer_size_and_alignment(type_ptr->variant.integer.int_kind,
                                       &size, &alignment);
        if (type_ptr->variant.integer.enum_type) {
          /* Issue a diagnostic if an explicit alignment is too restrictive
             for the underlying type. */
          alignment = check_explicit_enum_alignment(type_ptr, alignment);
        }  /* if */
        break;
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
        size =
          targ_sizeof_fixed_point[type_ptr->variant.fixed_point.is_unsigned]
                                 [type_ptr->variant.fixed_point.precision]
                                 [type_ptr->variant.fixed_point.is_fract_type];
        alignment =
         targ_alignof_fixed_point[type_ptr->variant.fixed_point.is_unsigned]
                                 [type_ptr->variant.fixed_point.precision]
                                 [type_ptr->variant.fixed_point.is_fract_type];
        break;
#endif /* FIXED_POINT_ALLOWED */
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
          default:
            unexpected_condition_str("set_type_size: bad float kind");
        }  /* switch */
#if C99_IL_EXTENSIONS_SUPPORTED
        if (type_ptr->kind == (a_type_kind)tk_complex) size *= 2;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        break;
      case tk_pointer:
#if MICROSOFT_EXTENSIONS_ALLOWED
        /* Explicitly sized pointers have a size independent from the type
           pointed to.  Their size can vary even when
           TARG_ALL_POINTERS_SAME_SIZE is TRUE. */
        if ((type_ptr->variant.pointer.modifiers & PM_PTR32) != 0) {
          size = 4;
          alignment = 4;
        } else if ((type_ptr->variant.pointer.modifiers & PM_PTR64) != 0) {
          size = 8;
          alignment = 8;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          size = size_of_pointer_to(type_pointed_to(type_ptr), &alignment);
        }  /* if */
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
      case tk_nullptr:
        size = size_of_pointer_to(void_type(), &alignment);
        break;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Class, struct and union sizes should be set when they are declared.
           See do_class_layout. */
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
        /* Vector types get their size set when they are created. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      default:
        unexpected_condition_str("set_type_size: bad type kind");
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

  if (is_integer_or_unscoped_enum(unqual_type)) {
    an_integer_kind ikind = unqual_type->variant.integer.int_kind;
    if (unqual_type->variant.integer.bool_type) {
      /* bool always promotes to int. */
      promoted_type = integer_type((an_integer_kind)ik_int);
    } else if (!C_mode() &&
                (unqual_type->variant.integer.enum_type ||
                 unqual_type->variant.integer.wchar_t_type ||
                 unqual_type->variant.integer.char16_t_type ||
                 unqual_type->variant.integer.char32_t_type) &&
                targ_sizeof_int == targ_sizeof_long &&
               (ikind == (an_integer_kind)ik_long ||
                ikind == (an_integer_kind)ik_unsigned_long)) {
      /* In C++, enums, wchar_t, char16_t, and char32_t (when  keywords) go
         through the normal promotion processing for the underlying type.  If
         the type is unchanged, it will be converted to the corresponding plain
         integral type (see below).  However, there's one anomaly:
         if long and int are the same size, enums, wchar_t, char16_t, and
         char32_t of size long promote to int or unsigned int. */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (unqual_type->variant.integer.microsoft_sized_int_type) {
            /* __int32 promotes to int. */
            promoted_type = integer_type((an_integer_kind)ik_int);
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          break;
        case ik_unsigned_int:
        case ik_long:
        case ik_unsigned_long:
#if LONG_LONG_ALLOWED
        case ik_long_long:
        case ik_unsigned_long_long:
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
        case ik_int128:
        case ik_unsigned_int128:
#endif /* INT128_EXTENSIONS_ALLOWED */
          /* These are deliberately left as they are; they are not supposed
             to be promoted. */
          break;
        default:
          unexpected_condition_str(
                                "type_after_integral_promotion: bad int kind");
      }  /* switch */
      if (C_dialect == C_dialect_cplusplus) {
        /* enums, wchar_t, char16_t, and char32_t get promoted to the
           corresponding integral type (and lose their special properties) if
           they were not promoted above. */
        unqual_type = skip_typerefs(promoted_type);
        if (unqual_type->variant.integer.enum_type ||
            unqual_type->variant.integer.wchar_t_type ||
            unqual_type->variant.integer.char16_t_type ||
            unqual_type->variant.integer.char32_t_type) {
          /* Make a "plain" version of this type, i.e., the same underlying
             integral type but not tagged as an enum, wchar_t, char16_t, or
             char32_t. */
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


a_type_ptr type_after_array_to_pointer_transformation(a_type_ptr type)
/*
Do the array --> pointer type transformation and return the transformed type.
Note that the returned type is intended as an rvalue type, so qualifiers like
"restrict" on the array type are not added on top of the result type.
*/
{
  /* The array --> pointer transformation converts "array of X" to
     "pointer to X". */
  type = make_pointer_type(array_element_type(type));
  return type;
}  /* type_after_array_to_pointer_transformation */


a_boolean is_narrowing_conversion(a_type_ptr    source_type,
                                  a_constant    *source_constant,
                                  a_type_ptr    dest_type,
                                  an_error_code *err_code)
/*
Return TRUE if converting from source_type to dest_type is a narrowing
conversion as defined by [dcl.init.list] of the C++11 standard.
If source_constant is non-NULL, it gives the known constant value of
the source; if it's NULL, it's assumed the source is not constant.
If err_code is non-NULL, *err_code is set to an appropriate error
code if TRUE is returned (the error codes take two type fill-ins,
for source and destination type).
*/
{
  a_boolean   is_narrowing = FALSE;
  a_boolean   err, depends_on_fp_mode, dependent_constant = FALSE;
  a_boolean   con_check_done = FALSE, fp_precision_check_failed = FALSE;

  check_assertion(!C_mode());
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  if (source_constant != NULL &&
      source_constant->kind == (a_constant_repr_kind)ck_template_param) {
    dependent_constant = TRUE;
  }  /* if */
  if (is_floating_type(source_type)) {
    if (is_integral_type(dest_type)) {
      /* Floating-point to integer is always narrowing. */
      is_narrowing = TRUE;
#if C99_IL_EXTENSIONS_SUPPORTED
    } else if (source_type->kind != dest_type->kind &&
               (is_nonreal_floating_type(source_type) ||
                is_nonreal_floating_type(dest_type))) {
      /* Something like _Complex double --> float or double --> _Complex float.
         Not covered by the standard.  May or may not be valid as an implicit
         conversion, but leave that to the caller; don't call it a narrowing
         conversion. */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    } else if (is_floating_type(dest_type)) {
      /* We ruled out complex and imaginary cases above. */
      check_assertion(is_floating(source_type) &&
                      source_type->kind == dest_type->kind);
      if ((int)source_type->variant.float_kind >
                                          (int)dest_type->variant.float_kind) {
        /* Floating-point to smaller floating_point.  Okay if the value is
           constant and preserved, even if not with full precision. */
        is_narrowing = TRUE;
        if (source_constant != NULL &&
            source_constant->kind == (a_constant_repr_kind)ck_float) {
          an_internal_float_value fval;
          con_check_done = TRUE;
          check_assertion(is_floating_type(source_constant->type));
          fp_change_kind(&source_constant->variant.float_value,
                         skip_typerefs(source_constant->type)
                                                          ->variant.float_kind,
                         &fval,
                         dest_type->variant.float_kind,
                         &err,
                         &depends_on_fp_mode);
          if (!err) is_narrowing = FALSE;
        } else if (dependent_constant) {
          /* A dependent constant might have a value that can be converted
             without loss. */
          is_narrowing = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_integral_or_unscoped_enum_type(source_type)) {
    if (is_floating_type(dest_type)) {
      /* Integer or unscoped enum to floating.  Okay if the value is constant
         and is preserved. */
      is_narrowing = TRUE;
      if (source_constant != NULL &&
          source_constant->kind == (a_constant_repr_kind)ck_integer
#if C99_IL_EXTENSIONS_SUPPORTED
          && !is_imaginary_type(dest_type)
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
         ) {
        a_constant_ptr fp_constant = local_constant();
        a_boolean      complex_dest = FALSE;
#if C99_IL_EXTENSIONS_SUPPORTED
        complex_dest = is_complex_type(dest_type);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        con_check_done = TRUE;
        if (complex_dest) { 
#if C99_IL_EXTENSIONS_SUPPORTED
          clear_constant(fp_constant, (a_constant_repr_kind)ck_complex); 
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        } else { 
          clear_constant(fp_constant, (a_constant_repr_kind)ck_float); 
        } 
        fp_constant->type = dest_type; 
        if (complex_dest) { 
#if C99_IL_EXTENSIONS_SUPPORTED
          conv_integer_value_to_float(
                                     &source_constant->variant.integer_value, 
                                     int_constant_is_signed(source_constant), 
                                     &fp_constant->variant.complex_value->real,
                                     dest_type->variant.float_kind, &err); 
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        } else { 
          conv_integer_value_to_float(&source_constant->variant.integer_value, 
                                      int_constant_is_signed(source_constant), 
                                      &fp_constant->variant.float_value, 
                                      dest_type->variant.float_kind, 
                                      &err); 
        }  /* if */
        if (!err) {
          /* Convert back to the original integral type to see if we lost
             anything due to precision issues. */
          an_error_code     local_err_code;
          an_error_severity err_severity;
          a_constant_ptr    int_constant = local_constant();
          clear_constant(int_constant, (a_constant_repr_kind)ck_integer);
          int_constant->type = source_constant->type;
          conv_float_to_integer(fp_constant,
                                int_constant,
                                &local_err_code,
                                &err_severity,
                                &depends_on_fp_mode,
                                /*constant_context=*/FALSE);
          if (local_err_code == ec_no_error &&
              cmp_integer_constants(source_constant, int_constant) == 0) {
            is_narrowing = FALSE;
          } else {
            fp_precision_check_failed = TRUE;
          }  /* if */
          release_local_constant(&int_constant);
        }  /* if */
        release_local_constant(&fp_constant);
      } else if (dependent_constant) {
        /* A dependent constant might have a value that can be converted
           without loss. */
        is_narrowing = FALSE;
      }  /* if */
    } else if (is_integral_type(dest_type)) {
      check_assertion(source_type->kind == (a_type_kind)tk_integer &&
                      dest_type->kind   == (a_type_kind)tk_integer);
      if (source_type->size > dest_type->size ||
          (source_type->size == dest_type->size &&
           int_kind_is_signed[(int)source_type->variant.integer.int_kind] !=
           int_kind_is_signed[(int)  dest_type->variant.integer.int_kind])) {
        /* Integer or unscoped enum to integer to integer that cannot represent
           all the values of the source type.  Okay if the value is constant
           and is preserved. */
        is_narrowing = TRUE;
        if (source_constant != NULL &&
            source_constant->kind == (a_constant_repr_kind)ck_integer) {
          con_check_done = TRUE;
          if (in_range_for_integer_kind(source_constant, source_constant,
                                        dest_type->variant.integer.int_kind)) {
            is_narrowing = FALSE;
          }  /* if */
        } else if (dependent_constant) {
          /* A dependent constant might have a value that can be converted
             without loss. */
          is_narrowing = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err_code != NULL) {
    /* Return an appropriate error code. */
    an_error_code local_err_code = ec_no_error;
    if (is_narrowing) {
      if (fp_precision_check_failed) {
        local_err_code = ec_constant_narrowing_conversion_to_float;
      } else if (con_check_done) {
        local_err_code = ec_constant_narrowing_conversion;
      } else {
        local_err_code = ec_narrowing_conversion;
      }  /* if */
    }  /* if */
    *err_code = local_err_code;
  }  /* if */
  return is_narrowing;
}  /* is_narrowing_conversion */


a_type_ptr pointer_con_complete_object_type(a_constant_ptr constant)
/*
Return the type of the complete object that contains the location pointed to
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

  if (con_is_exact_addr_of_variable(constant, &var,
                                    /*array_decay_allowed=*/TRUE)) {
    /* Unmodified address of a variable.  The variable is the complete
       object and its type is the complete object type. */
    complete_object_type = var->type;
    if (is_array_type(complete_object_type) &&
        !is_array_type(type_pointed_to(constant->type))) {
      /* If the address is of an array variable decayed to pointer, use the
         element type. */
      complete_object_type =
                           underlying_array_element_type(complete_object_type);
    }  /* if */
  }  /* if */
  return complete_object_type;
}  /* pointer_con_complete_object_type */


static void examine_expr_for_complete_object_type(
                                    an_expr_node_ptr                    node,
                                    an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from the expression traversal routines to process an expression
as part of finding the complete object type.  The expression passed in
is an addressing expression, meaning either a glvalue that identifies an
object or a prvalue that is a pointer (or C++/CLI handle) to an object.
*/
{
  a_type_ptr complete_object_type = NULL;
  a_boolean  suppress_subtree_walk = FALSE;

  if (is_glvalue_node(node)) {
    /* The expression passed in is a glvalue for an object. */
    switch (node->kind) {
      case enk_error:
      case enk_routine:
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
      case enk_builtin_operation:
        /* Complete object type is not known. */
        suppress_subtree_walk = TRUE;
        break;
      case enk_variable:
        /* The variable is the complete object and its type is the
           complete object type. */
        complete_object_type = node_variable(node)->type;
        break;
      case enk_constant:
        /* The only constant lvalue is a string. */
        { a_constant_ptr con = node_constant(node);
          if (con->kind == (a_constant_repr_kind)ck_string) {
            complete_object_type = con->type;
          }  /* if */
        }
        break;
      case enk_operation:
        /* Operator. */
        { an_expr_operator_kind op = node->variant.operation.kind;
          an_expr_node_ptr      operand1 = node->variant.operation.operands;
          an_expr_node_ptr      operand2 = operand1->next;
          if (op == (an_expr_operator_kind)eok_dot_field ||
              op == (an_expr_operator_kind)eok_points_to_field) {
            /* Field selection (a.b or p->b).  The field itself is a complete
               object (recall that "complete" means "not a base class" rather
               than "not part of another object"). */
            /* MSVC++ doesn't do this optimization.  It allows one to
               do a placement new of a derived class type on a subobject
               and get the derived class behavior.  Confirmed in 5.0 through
               8.0. */
            if (!microsoft_mode) {
              complete_object_type = node_field(operand2)->type;
            }  /* if */
            suppress_subtree_walk = TRUE;
          } else if (op == (an_expr_operator_kind)eok_pm_field ||
                     op == (an_expr_operator_kind)eok_pm_points_to_field) {
            /* a.*pm, p->*pm.  We can't tell the complete object type because
               the member type of the pointer-to-member might be a base class
               of the actual member type. */
            suppress_subtree_walk = TRUE;
          } else if (op == (an_expr_operator_kind)eok_question
#if GNU_EXTENSIONS_ALLOWED
                     || is_gnu_min_max_operator(op)
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                   ) {
            /* Can't tell the complete type of a "?" operator or a GNU min/max
               operator because two operands would be considered. */
            suppress_subtree_walk = TRUE;
          }  /* if */
        }
        break;
      case enk_temp_init:
        /* The type of the temporary created is the complete object type. */
        complete_object_type = node->type;
        break;
      case enk_object_lifetime:
        /* Handled by the traversal routine. */
        break;
      case enk_typeid:
        /* Complete type could be const std::type_info or it could be an
           implementation-specific type derived from that, so we don't know
           the complete object type. */
        suppress_subtree_walk = TRUE;
        break;
      case enk_param_ref:
        /* The type of the parameter is the complete object type.  "this"
           cases won't get here because they are not lvalues. */
        complete_object_type = node->type;
        break;
      case enk_reuse_value:
      case enk_new_delete:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case enk_gcnew:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case enk_address_of_ellipsis:
      case enk_throw:
      case enk_field:
      case enk_condition:
      case enk_sizeof:
      case enk_alignof:
      case enk_sizeof_pack:
      case enk_type_operand:
#if GNU_EXTENSIONS_ALLOWED
      case enk_statement:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
      case enk_result_of_overriding_function:
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if VLA_DEALLOCATIONS_IN_IL
      case enk_vla_dealloc:
#endif /* VLA_DEALLOCATIONS_IN_IL */
      case enk_c11_generic:
      default:
        unexpected_condition_str(
                 "examine_expr_for_complete_object_type: bad expression kind");
    }  /* switch */
  } else {
#if DO_IL_LOWERING
    /* The expression passed in is a pointer or reference to an object,
       or a C++/CLI handle. */
    check_assertion((!is_glvalue_node(node) &&
                     (is_any_ptr_or_ref_type(node->type) ||
                      is_template_param_type(node->type) ||
                      is_error_type(node->type))) ||
                    is_error_node(node));
#else /* !DO_IL_LOWERING */
    /* The expression passed in is a pointer to an object (or a C++/CLI
       handle). */
    check_assertion((!is_glvalue_node(node) &&
                     (is_pointer_or_handle_type(node->type) ||
                      is_template_param_type(node->type) ||
                      is_error_type(node->type))) ||
                    is_error_node(node));
#endif /* DO_IL_LOWERING */
    switch (node->kind) {
      case enk_error:
      case enk_routine:
      case enk_temp_init:
      case enk_address_of_ellipsis:
#if GNU_EXTENSIONS_ALLOWED
      case enk_statement:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
      case enk_lowered_eh_construct:
#endif /* DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING */
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
      case enk_result_of_overriding_function:
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
      case enk_builtin_operation:
        /* Complete object type is not known. */
        suppress_subtree_walk = TRUE;
        break;
      case enk_variable:
        /* Value of a pointer variable. */
        /* Complete object type is not known in general, but if the variable
           is the "this" parameter for a constructor or destructor, and we're
           optimizing a virtual call case, it is known. */
        { a_variable_ptr var = node_variable(node);
          if (tblock->call_case && var->source_corresp.name == NULL &&
              var->is_parameter && innermost_function_scope != NULL) {
            /* The variable is a parameter and we're inside a function. */
            if (var == innermost_function_scope->variant.routine.
                                                         this_param_variable) {
              /* The variable is the "this" parameter variable of the current
                 function. */
              a_routine_ptr curr_routine =
                                 innermost_function_scope->variant.routine.ptr;
              if (curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_constructor ||
                  curr_routine->special_kind ==
                                    (a_special_function_kind)sfk_destructor) {
                /* The current function is a constructor or destructor.
                   We know that the "this" parameter points to a complete
                   object, at least for purposes of resolving virtual calls
                   (C++ standard [class.cdtor]). */
                complete_object_type = parent_class_of(curr_routine);
              }  /* if */
            }  /* if */
          }  /* if */
        }
        break;
      case enk_constant:
        /* Address constant. */
        complete_object_type =
                        pointer_con_complete_object_type(node_constant(node));
        break;
      case enk_operation:
        /* Operator. */
        { an_expr_operator_kind op = node->variant.operation.kind;
          an_expr_node_ptr      operand1 = node->variant.operation.operands;
          if (op == (an_expr_operator_kind)eok_array_to_pointer) {
            /* Array to pointer decay.  Strictly speaking, an array is a
               complete object, and its elements are complete objects.
               However, if we had a variable of pointer-to-array type, and
               by use of casts we stored into that variable a pointer to
               an array of same-sized elements (e.g., an array of a base
               class type), we wouldn't want the analysis here to assume
               we know the type of the array elements.  So use a recursive
               call on the first operand.  If the first operand is a
               prvalue array, give up. */
            if (is_glvalue_node(operand1)) {
              traverse_expr(operand1, tblock);
              complete_object_type = tblock->complete_object_type;
              if (complete_object_type != NULL &&
                  is_array_type(complete_object_type)) {
                complete_object_type =
                           underlying_array_element_type(complete_object_type);
                tblock->complete_object_type = complete_object_type;
              }  /* if */
            }  /* if */
            suppress_subtree_walk = TRUE;
          }  /* if */
        }
        break;
      case enk_new_delete:
        { a_new_delete_supplement_ptr ndsp = node->variant.new_delete;
          if (ndsp->is_new) {
            /* For new, the type is known. */
            complete_object_type = ndsp->type;
            if (is_array_type(complete_object_type)) {
              complete_object_type =
                           underlying_array_element_type(complete_object_type);
            }  /* if */
          } else {
            /* Not easy to tell the type for delete, and probably not
               worth it. */
          }  /* if */
        }
        break;
      case enk_lambda:
        /* The class value returned by the lambda node is a complete object. */
        complete_object_type = node->type;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case enk_gcnew:
      {
        a_gcnew_supplement_ptr gsp = node->variant.gcnew_info;
        complete_object_type = gsp->type;
      }
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case enk_object_lifetime:
        /* Handled by the traversal routine. */
        break;
      case enk_typeid:
        /* Complete type could be const std::type_info or it could be an
           implementation-specific type derived from that, so we don't know
           the complete object type. */
        suppress_subtree_walk = TRUE;
        break;
      case enk_param_ref:
        /* Complete object type is not known.  The parameter is a pointer,
           but we don't know the static type of the thing pointed to. */
        suppress_subtree_walk = TRUE;
        break;
      case enk_reuse_value:
        /* The details of the reused pointer value determine whether we know
           the complete object type. */
        { a_dynamic_init_ptr init = node->variant.reused_value_init;
          if (init->kind == (a_dynamic_init_kind)dik_constant) {
            complete_object_type =
                      pointer_con_complete_object_type(init->variant.constant);
          } else if (init->kind == (a_dynamic_init_kind)dik_expression ||
                     init->kind == 
                              (a_dynamic_init_kind)dik_class_result_via_ctor) {
            traverse_expr(init->variant.expression, tblock);
          }  /* if */
          suppress_subtree_walk = TRUE;
        }
        break;
      case enk_throw:
      case enk_field:
      case enk_condition:
      case enk_sizeof:
      case enk_alignof:
      case enk_sizeof_pack:
      case enk_type_operand:
#if VLA_DEALLOCATIONS_IN_IL
      case enk_vla_dealloc:
#endif /* VLA_DEALLOCATIONS_IN_IL */
      case enk_c11_generic:
      default:
        unexpected_condition_str(
                 "examine_expr_for_complete_object_type: bad expression kind");
    }  /* switch */
  } /* if */
  if (!tblock->terminate) {
    if (complete_object_type != NULL) {
      /* We've determined a complete object type at this level. */
      tblock->complete_object_type = complete_object_type;
      tblock->terminate = TRUE;
    } else if (suppress_subtree_walk) {
      tblock->suppress_subtree_walk = TRUE;
    }  /* if */
  }  /* if */
}  /* examine_expr_for_complete_object_type */


a_type_ptr expr_complete_object_type(an_expr_node_ptr expr,
                                     a_boolean        call_case)
/*
Return the type of the complete object that contains the object indicated
by expr (a glvalue or prvalue), or NULL if no complete object can be
determined.  call_case is TRUE if the answer will be used to optimize
a virtual function call.  NULL is always a safe answer; non-NULL
values may permit optimizations.  Note that "complete object" means an
object that is not a base class of another object, not necessarily a
top-level object.  This is used only in C++ mode; it is useful to know
what the complete object type is to optimize base class casts and
virtual function calls.
*/
{
  a_type_ptr complete_object_type = NULL;

  if (is_glvalue_node(expr)) {
    /* For a glvalue, look down the tree to find the underlying object. */
    an_expr_or_stmt_traversal_block tblock;

    clear_expr_or_stmt_traversal_block(&tblock);
    tblock.process_expr = examine_expr_for_complete_object_type;
    tblock.follow_addressing_path = TRUE;
    tblock.call_case = call_case;
    traverse_expr(expr, &tblock);
    complete_object_type = tblock.complete_object_type;
  } else {
    /* For a prvalue, the expression type is the complete object type. */
    complete_object_type = expr->type;
  }  /* if */
  return complete_object_type;
}  /* expr_complete_object_type */


a_type_ptr pointer_expr_complete_object_type(an_expr_node_ptr expr,
                                             a_boolean        call_case)
/*
Return the type of the complete object that contains the location pointed to
by expr (a pointer prvalue), or NULL if no complete object can be determined.
call_case is TRUE if the answer will be used to optimize a virtual function
call.  NULL is always a safe answer; non-NULL values may permit optimizations.
Note that "complete object" means an object that is not a base class of
another object, not necessarily a top-level object.  This is used only in
C++ mode; it is useful to know what the complete object type is to optimize
base class casts and virtual function calls.  In C++/CLI mode, the expression
can be a handle.
*/
{
  an_expr_or_stmt_traversal_block tblock;

  check_assertion((!is_glvalue_node(expr) &&
                   (is_pointer_or_handle_type(expr->type) ||
                    is_template_param_type(expr->type) ||
                    is_error_type(expr->type))) ||
                  is_error_node(expr));
  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_expr = examine_expr_for_complete_object_type;
  tblock.follow_addressing_path = TRUE;
  tblock.call_case = call_case;
  traverse_expr(expr, &tblock);
  return tblock.complete_object_type;
}  /* pointer_expr_complete_object_type */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- class_type is unused in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
a_type_ptr add_right_pointer_type_to_this(a_type_ptr type,
                                          a_type_ptr class_type)
/*
Add the right kind of "pointer to" to "type" so it can be used as a "this"
pointer for a member of the class class_type, and return the pointer type.
The kind of pointer is unusual (e.g., it can be a handle) when the class
is a C++/CLI or C++/CX class.  See type_of_address_of for a variant of
this function.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && is_value_class_type(class_type)) {
    /* "this" in a C++/CLI value class is an interior_ptr (ECMA 22.3.3). */
    type = make_interior_ptr_type(type);
  } else if (cli_or_cx_enabled && is_managed_class_type(class_type) &&
             !(cppcx_enabled && is_value_class_type(class_type))) {
    /* "this" in a C++/CLI managed class is a handle (ECMA 22.3.3).  That is
       also the case in C++/CX managed classes, except for C++/CX
       value classes. */
    type = make_handle_type(type);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    type = make_pointer_type(type);
  }  /* if */
  return type;
}  /* add_right_pointer_type_to_this */


a_type_ptr f_implicit_this_param_type_of(a_type_ptr  routine_type)
/*
Synthesize the type of "this" from the underlying class type and the
qualification of a member function type.  Note that the type
returned does not include a top-level "const", and that must sometimes
be added to get the actual "this" variable type.
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_type_ptr                    class_type;
  a_type_ptr                    result;

  routine_type = skip_typerefs(routine_type);
  check_assertion(is_function_type(routine_type));
  rtsp = routine_type->variant.routine.extra_info;
  class_type = rtsp->this_class;
  result = class_type;
  /* The standard function cv-qualifiers (recorded in rtsp->qualifiers) apply
     not to the "this" pointer, but to the type pointed to by the "this"
     pointer.  The nonstandard "restrict" qualifier, on the other hand, goes
     on top of the pointer type (to denote the limited aliasing of the "this"
     pointer); it (and any similar custom qualifiers) is recorded in
     rtsp->this_qualifiers. */
  if (rtsp->qualifiers != TQ_NONE) {
    result = make_qualified_type(result, rtsp->qualifiers);
  }  /* if */
  result = add_right_pointer_type_to_this(result, class_type);
  if (rtsp->this_qualifiers != TQ_NONE) {
    result = make_qualified_type(result, rtsp->this_qualifiers);
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
                   int_kind_is_signed[(int)type_2->variant.integer.int_kind] &&
               type_1->variant.integer.bool_type ==
                                            type_2->variant.integer.bool_type);
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
      node_1 = skip_parens(node_1);
      node_2 = skip_parens(node_2);
      if (node_1->kind == (an_expr_node_kind)enk_constant &&
          node_2->kind == (an_expr_node_kind)enk_constant) {
        identical = eq_constants(node_constant(node_1), node_constant(node_2));
      }  /* if */
    } else {
      /* A variable-bound array and a fixed-bound array. */
    }  /* if */
  } else if (type_2->variant.array.is_variable_size_array) {
    /* A variable-bound array and a fixed-bound array. */
  } else if (type_1->variant.array.is_template_dependent_size_array) {
    if (type_2->variant.array.is_template_dependent_size_array) {
      /* Both arrays have unknown (but constant) bounds. */
      a_constant_ptr b1 = type_1->variant.array.variant.element_count_constant;
      a_constant_ptr b2 = type_2->variant.array.variant.element_count_constant;
      if (b1 == NULL || b2 == NULL) {
        identical = (b1 == b2);
      } else {
        identical = eq_constants(b1, b2);
      }  /* if */
    } else {
      /* An unknown-bound array and a known-bound array. */
    }  /* if */
  } else if (type_2->variant.array.is_template_dependent_size_array) {
    /* A known-bound array and an unknown-bound array. */
  } else {
    /* Both arrays have fixed bounds.  Just compare the element counts. */
    identical = (type_1->variant.array.variant.number_of_elements ==
                 type_2->variant.array.variant.number_of_elements)
#if UPC_EXTENSIONS_ALLOWED
                && (type_1->variant.array.is_threads_dimension ==
                    type_2->variant.array.is_threads_dimension)
#endif /* UPC_EXTENSIONS_ALLOWED */
                                                               ;
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
      if (identical_types(parent_class_of(type_1),
                          parent_class_of(type_2))) {
        /* Their parent types are the same. */
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* equiv_nonreal_templates */


static a_boolean equiv_template_template_params(
				a_symbol_ptr	sym_1,
				a_symbol_ptr	sym_2,
				a_boolean	exact_templ_match_required)
/*
Return TRUE if sym_1 and sym_2 are both template template parameters for
equivalent templates, such as T in "T<int>" and "T<int>".
exact_templ_match_required is TRUE if the values of the template template
parameters must match exactly (i.e., point to the same template entry).
*/
{
  a_boolean	result = FALSE;

  if (is_template_template_param_symbol(sym_1) &&
      is_template_template_param_symbol(sym_2)) {
    /* They are both template template parameters.  Compare the
       underlying templates. */
    an_equiv_templates_options_set	et_options = ET_NO_OPTIONS;
    if (exact_templ_match_required) et_options |= ET_EXACT_MATCH_REQUIRED;
    if (equiv_templates_given_supplement(sym_1->variant.template_info,
                                         sym_2->variant.template_info,
                                         et_options, ETP_NO_OPTIONS)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* equiv_template_template_params */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- contextual_generic_parameters is unused in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
a_boolean equiv_class_types(a_type_ptr type_1,
                            a_type_ptr type_2,
                            a_boolean  error_matches_anything,
                            a_boolean  exact_templ_arg_match_required,
                            a_boolean  contextual_generic_parameters)
/*
type_1 and type_2 are class/struct/union types.  Return TRUE if they are
equivalent types.  In general, classes, structs, and unions that aren't
the same type aren't equivalent.  The exception is with template classes
involving template parameters (i.e., nonreal template classes).  Two
nonreal template classes are identical if they are based on the same
class template and have identical template arguments.
If error_matches_anything is TRUE, consider an error type or constant in
a template argument to match anything (that's appropriate for compatibility
checking instead of equivalence checking).  exact_templ_arg_match_required
is TRUE if the values of the templates arguments must match exactly (i.e.,
point to the same type or constant).  FALSE if only equivalence is required.
If contextual_generic_parameters parameters is TRUE, generic parameters are
compared not purely based on their "coordinates", but on the generic context
in which they are declared (see flags ITF_CONTEXTUAL_GENERIC_PARAMETERS and
TCF_CONTEXTUAL_GENERIC_PARAMETERS).
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled &&
             is_cli_generic_constraint(type_1) &&
             is_cli_generic_constraint(type_2)) {
    /* Check if these are constraint types that should be considered the
       same. */
    a_type_ptr	templ_param_type_1;
    a_type_ptr	templ_param_type_2;
    templ_param_type_1 = template_param_if_proxy_class(type_1);
    templ_param_type_2 = template_param_if_proxy_class(type_2);
    if (templ_param_type_1 == templ_param_type_2) {
      equiv = TRUE;
    } else if (!exact_templ_arg_match_required) {
      an_itf_flag_set  it_flags = ITF_NO_FLAGS;
      if (contextual_generic_parameters) {
        it_flags |= ITF_CONTEXTUAL_GENERIC_PARAMETERS;
      }  /* if */
      if (f_identical_types(templ_param_type_1, templ_param_type_2,
                            it_flags)) {
        equiv = TRUE;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if ((type_1->variant.class_struct_union.is_nonreal_class &&
              type_2->variant.class_struct_union.is_nonreal_class) ||
             (is_cli_open_constructed_instance(type_1) &&
              is_cli_open_constructed_instance(type_2)) ||
             error_matches_anything) {
    /* The pointers aren't the same, so the classes probably aren't
       equivalent, but do some special checking to see if they are equivalent
       nonreal classes based on the same template or equivalent template
       template parameters. */
    /* Go to the class symbol supplements for the types. */
    /* Watch out for types created by IL lowering, which do not have the
       assoc_info pointer. */
    a_symbol_ptr  type_sym_1 = symbol_for(type_1),
                  type_sym_2 = symbol_for(type_2);
    if (type_sym_1 != NULL && type_sym_2 != NULL) {
      cssp_1 = class_symbol_supp(type_sym_1);
      cssp_2 = class_symbol_supp(type_sym_2);
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
        if (identical_templates_given_symbol(cssp_1->class_template,
                                             cssp_2->class_template) ||
            identical_templates_given_symbol(
                                primary_template_of(cssp_1->class_template),
                                primary_template_of(cssp_2->class_template)) ||
            equiv_nonreal_templates(type_1, cssp_1->class_template,
                                    type_2, cssp_2->class_template) ||
            equiv_template_template_params(cssp_1->class_template,
                                           cssp_2->class_template,
                                           exact_templ_arg_match_required)) {
          /* Both types are template classes, and they are based on the same
             class template, or equivalent nonreal templates. */
          an_equiv_templ_arg_options_set	eta_options = ETA_NO_OPTIONS;
          a_symbol_ptr			templ_sym_1;
          a_symbol_ptr			templ_sym_2;
          a_template_symbol_supplement_ptr	tssp_1;
          a_template_symbol_supplement_ptr	tssp_2;
          templ_sym_1 = cssp_1->class_template;
          templ_sym_2 = cssp_2->class_template;
          templ_sym_1 = primary_template_of(templ_sym_1);
          templ_sym_2 = primary_template_of(templ_sym_2);
          tssp_1 = templ_sym_1->variant.template_info;
          tssp_2 = templ_sym_2->variant.template_info;
          /* If either template is variadic, pass the is_variadic flag. */
          if (tssp_1->is_variadic || tssp_2->is_variadic) {
            eta_options |= ETA_IS_VARIADIC;
          }  /* if */
          if (error_matches_anything) {
            eta_options |= ETA_ERROR_MATCHES_ANYTHING;
          }  /* if */
          if (is_nonreal_template_symbol(cssp_1->class_template) ||
              is_nonreal_template_symbol(cssp_2->class_template)) {
            eta_options |= ETA_IS_NONREAL_MEMBER;
          }  /* if */
          if (exact_templ_arg_match_required) {
            /* Template argument lists must match exactly, not just be
               equivalent. */
            eta_options |= ETA_EXACT_MATCH_REQUIRED;
          }  /* if */
          if (equiv_template_arg_lists(
                                 class_type_supp(type_1)->template_arg_list,
                                 class_type_supp(type_2)->template_arg_list,
                                 eta_options)) {
            equiv = TRUE;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (type_1->source_corresp.is_class_member &&
                 type_2->source_corresp.is_class_member &&
                 type_1->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class &&
                 type_2->variant.class_struct_union.
                                            is_ms_instantiated_nonreal_class) {
        /* In most cases nested classes of nonreal classes are represented a
           tptk_member template parameters, so they are not compared here.
           But Microsoft instantiated nonreal classes have actual nested
           classes as members.  They are considered the same if their names
           are the same and their parent classes are the same. */
        a_symbol_ptr	sym_1 = symbol_for(type_1);
        a_symbol_ptr	sym_2 = symbol_for(type_2);
        equiv = sym_1->header == sym_2->header &&
                identical_types(parent_class_of(type_1),
                                parent_class_of(type_2));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
  }  /* if */
  return equiv;
}  /* equiv_class_types */

#endif /* !STANDALONE_UTILITY_PROGRAM */
#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean equiv_pointer_modifiers(a_pointer_modifier_set  pms1,
                                         a_pointer_modifier_set  pms2)
/*
Return TRUE if the given pointer modifier sets are "equivalent".  On 64-bit
platforms, this means that the two sets are identical, except perhaps for the
__ptr64 modifier.  On non-64-bit platforms, "equivalent" means "identical".
*/
{
  if (is_64bit_target) {
    pms1 &= ~(a_pointer_modifier_set)PM_PTR64;
    pms2 &= ~(a_pointer_modifier_set)PM_PTR64;
  }  /* if */
  return pms1 == pms2;
}  /* equiv_pointer_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
    /* If c_and_cpp_function_types_are_distinct is FALSE, nlk_external and
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

#if !STANDALONE_UTILITY_PROGRAM

static a_boolean adjust_comparison_types_for_decltype(a_type_ptr *p_type_1,
                                                      a_type_ptr *p_type_2)
/*
The types pointed to by p_type_1 and p_type_2 are being compared by
f_identical_types or f_types_are_compatible as part of checking that
the result of template deduction and substitution has produced a valid
type.  If one or the other is a type produced by a decltype, adjust
the types to account for any differences introduced by the decltype
itself, e.g., an extra "reference to" on one of the types because the
operand of the decltype is an lvalue in one case and not in the other.
Return TRUE if such an adjustment was made.
*/
{
  a_boolean  adjustment_made = FALSE;
  a_type_ptr type_1 = *p_type_1;
  a_type_ptr type_2 = *p_type_2;

  /* Remove non-decltype typerefs from both types.  (This code is done
     after cv-qualifiers have been checked, so they are irrelevant at
     this point.) */
  while (type_1->kind == (a_type_kind)tk_typeref &&
         !typeref_is_type_operator(type_1)) {
    type_1 = type_1->variant.typeref.type;
  }  /* while */
  while (type_2->kind == (a_type_kind)tk_typeref &&
         !typeref_is_type_operator(type_2)) {
    type_2 = type_2->variant.typeref.type;
  }  /* while */
  /* If one is a decltype for a non-reference type and the other is
     a reference type, strip the reference. */
  if (type_1->kind == (a_type_kind)tk_typeref &&
      !is_any_reference_type(type_1) &&
      is_any_reference_type(type_2)) {
    type_2 = type_pointed_to(type_2);
    adjustment_made = TRUE;
  } else if (type_2->kind == (a_type_kind)tk_typeref &&
             !is_any_reference_type(type_2) &&
             is_any_reference_type(type_1)) {
    type_1 = type_pointed_to(type_1);
    adjustment_made = TRUE;
  }  /* if */
  *p_type_1 = type_1;
  *p_type_2 = type_2;
  return adjustment_made;
}  /* adjust_comparison_types_for_decltype */


static a_boolean f_change_to_canonical_types(a_type_ptr  *type_1,
                                             a_type_ptr  *type_2,
                                             a_boolean   seek_corresp)
/*
If the given types have a canonical correspondence in another translation
unit change the pointers to point to those entries and return TRUE.
Otherwise, return FALSE.  If seek_corresp is TRUE, the two types are
expected to correspond, but the correspondence might not have been found
through the symbol table:  Establish that correspondence now if appropriate.
*/
{
  a_boolean   changed = FALSE;
  a_type_ptr  new_type_1 = *type_1, new_type_2 = *type_2;
  a_boolean   is_class_1 = is_immediate_class_type(new_type_1),
              is_class_2 = is_immediate_class_type(new_type_2),
              is_enum_1 = is_immediate_enum_type(new_type_1),
              is_enum_2 = is_immediate_enum_type(new_type_2);

  /* In C mode, types correspondences are set as the result of checking for
     type compatibility across translation units.  In C++ mode, a similar
     mechanism is used for unnamed types (classes and enums). */
  if (seek_corresp &&
      ((is_class_1 && is_class_2 &&
        (C_mode() ||
         ((!has_name(new_type_1) ||
           new_type_1->variant.class_struct_union.originally_unnamed) &&
          (!has_name(new_type_2) ||
           new_type_2->variant.class_struct_union.originally_unnamed)))) ||
       (is_enum_1 && is_enum_2 &&
        (C_mode() ||
         ((!has_name(new_type_1) ||
           new_type_1->variant.integer.originally_unnamed) &&
          (!has_name(new_type_2) ||
           new_type_2->variant.integer.originally_unnamed)))))) {
    /* seek_type_corresp attempts to make the first type correspond to the
       second type: It does not attempt to make the second type correspond to
       the first.  So we may need to call it twice.  (E.g., if new_type_1 has
       a correspondence and new_type_2 does not, the first call will have no
       effect, but the second call may make new_type_2 correspond to
       new_type_1. */
    (void)(seek_type_corresp(new_type_1, new_type_2) ||
           seek_type_corresp(new_type_2, new_type_1));
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


static a_type_ptr skip_typerefs_for_distinct_decltype_check(
                                                        a_type_ptr type,
                                                        a_boolean  *check_expr)
/*
Remove typerefs from the given type in preparation for the check in
distinct_dependent_decltypes.  Typerefs are removed until there are no
more or until a dependent decltype or typeof based on an expression
is encountered.  If the type returned is one of those typerefs,
*check_expr is returned TRUE.
*/
{
  *check_expr = FALSE;
  while (type->kind == (a_type_kind)tk_typeref) {
    if (type->variant.typeref.is_dependent_type_operator &&
        /* Don't stop on __underlying_types, since they are not based on
           expressions. */
        !type->variant.typeref.is_underlying_type
#if GNU_EXTENSIONS_ALLOWED
        /* Don't stop on typeofs without expressions, since you can't compare
           expressions on those. */
        && !type->variant.typeref.is_typeof_with_type_operand
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                             ) {
      *check_expr = TRUE;
      break;
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  return type;
}  /* skip_typerefs_for_distinct_decltype_check */


static a_boolean distinct_dependent_decltypes(a_type_ptr      type_1,
                                              a_type_ptr      type_2,
                                              an_itf_flag_set itf_flags)
/*
If type_1 and/or type_2 are expressed using decltype (or typeof) constructs
with dependent arguments, return TRUE if those constructs can be considered
to be distinct.  itf_flags is a set of option flags that specify options for
type comparisons.  The C++ standard defines notions of "equivalent" and
"functionally equivalent" expressions.  When decltype is applied to
"equivalent expressions" (which implies identical syntax), the resulting types
are also equivalent (and this routine returns FALSE).  When decltype is
applied to "expressions that are not functionally equivalent", the resulting
types are distinct (and this routine returns TRUE).  In between there is a
gray area of expressions that are functionally equivalent but not equivalent:
This routine may return TRUE or FALSE for such cases.  For a return value
of TRUE, the caller immediately concludes that the types are not identical
or compatible.  For a result value of FALSE, the caller continues on to the
normal (underlying) type comparison.
*/
{ 
  a_boolean result = FALSE;
  a_boolean check_expr_1, check_expr_2;

  if (!C_mode() && in_front_end) {
top_of_loop:
    /* Peel off tk_typeref layers looking for template-dependent decltype or
       typeof nodes based on expressions. */
    type_1 = skip_typerefs_for_distinct_decltype_check(type_1,
                                                       &check_expr_1);
    type_2 = skip_typerefs_for_distinct_decltype_check(type_2,
                                                       &check_expr_2);
    if (check_expr_1 || check_expr_2) {
      /* Some dependent decltype/typeof type was encountered. */
      if (check_expr_1 != check_expr_2) {
        /* One is a decltype with an expression and the other isn't. */
        result = TRUE;
      } else if (type_1 == type_2) {
        /* We don't need to compare the types and expressions because they
           are identical. */
        result = FALSE;
      } else if (type_1->variant.typeref.is_decltype !=
                                      type_2->variant.typeref.is_decltype ||
#if GNU_EXTENSIONS_ALLOWED
                 type_1->variant.typeref.is_typeof !=
                                        type_2->variant.typeref.is_typeof ||
#endif /* GNU_EXTENSIONS_ALLOWED */
                 type_1->variant.typeref.decltype_expr_not_parenthesized !=
                     type_2->variant.typeref.decltype_expr_not_parenthesized) {
        /* The two types were obtained with different constructs and are
           therefore different. */
        result = TRUE;
      } else {
        /* Compare the expressions. */
        an_expr_node_ptr  expr1 = decltype_arg(type_1);
        an_expr_node_ptr  expr2 = decltype_arg(type_2);
        if (expr1 == NULL || expr2 == NULL) {
          /* No expression is available for cases constructed inside a
             function body that's no longer the current one.  In those cases,
             we assume they are distinct. */
          result = TRUE;
        } else {
          /* Compare the expression trees. */
          a_compare_constants_options_set cc_options =
                                         CC_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED;
          /* When this routine is called with the "exact template param" flag
             set, pass the corresponding flag to the constant comparison
             routine. */
          if (itf_flags & ITF_EXACT_TEMPLATE_PARAM_TYPE_REQUIRED) {
            cc_options |= CC_EXACT_TEMPLATE_PARAM_TYPE_REQUIRED;
          }  /* if */
          result = !compare_expressions(expr1, expr2, cc_options);
        }  /* if */
      }  /* if */
      if (!result) {
        /* The types are still not distinct.  Strip off the typerefs we
           just checked and go back and look again at the next level down. */
        if (check_expr_1) type_1 = type_1->variant.typeref.type;
        if (check_expr_2) type_2 = type_2->variant.typeref.type;
        goto top_of_loop;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* distinct_dependent_decltypes */


a_type_ptr param_type_restoring_orig_templ_array(a_param_type_ptr ptp)
/*
If the original declared parameter type for the indicated parameter is an
array type with a top-level template-dependent bound, return it.  Otherwise
return the normal parameter type.  In C mode, or when C++11 SFINAE is
not enabled, always return the normal parameter type.
*/
{
  a_type_ptr type = ptp->type;

  if (!C_mode() && cpp11_sfinae_enabled) {
    a_type_ptr decl_type = ptp->declared_type;
    if (decl_type != NULL && is_array_type(decl_type)) {
      a_type_ptr tp = skip_typerefs(decl_type);
      if (tp->variant.array.is_template_dependent_size_array) {
        type = decl_type;
      }  /* if */
    }  /* if */
  }  /* if */
  return type;
}  /* param_type_restoring_orig_templ_array */


static void set_up_array_param_type_comparison(a_param_type_ptr ptp1,
                                               a_param_type_ptr ptp2,
                                               a_type_ptr       *type1,
                                               a_type_ptr       *type2)
/*
If both of the parameter types indicated by ptp1 and ptp2 were originally
specified as array types with template-dependent bounds, set *type1 and
*type2 to those array types.  Otherwise, leave *type1 and *type2 unchanged.
*/
{
  if (!C_mode()) {
    a_type_ptr orig1 = param_type_restoring_orig_templ_array(ptp1);
    a_type_ptr orig2 = param_type_restoring_orig_templ_array(ptp2);

    if (is_array_type(orig1) && is_array_type(orig2)) {
      /* Both parameters have dependent array types, so compare those. */
      *type1 = orig1;
      *type2 = orig2;
    }  /* if */
  }  /* if */
}  /* set_up_array_param_type_comparison */


static a_boolean is_placeholder_deduction_match(a_type_ptr  type_1,
                                                a_type_ptr  type_2)
/*
Return TRUE if type_1 is a tk_typeref entry for a deduced "decltype(auto)" or
"auto" placeholder, and type_2 is a tk_template_param entry for the
corresponding placeholder.
*/
{
  a_boolean  result = FALSE;

  if (type_1->kind == (a_type_kind)tk_typeref && is_template_param(type_2) &&
      type_2->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_param &&
      type_2->variant.template_param.extra_info->coordinates.depth ==
                                                    AUTO_TYPE_NESTING_DEPTH) {
    if (type_2->variant.template_param.extra_info->coordinates.position
                                              == PLAIN_AUTO_TYPE_POS_NUMBER) {
      result = type_1->variant.typeref.is_deduced_auto;
    } else {
      result = type_1->variant.typeref.is_deduced_decltype_auto;
    }  /* if */
  }  /* if */
  return result;
}  /* is_placeholder_deduction_match */


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
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
  a_boolean                     ignore_ms_calling_convention = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */

  /* First, check if the types are the same.  This repeats the test in the
     identical_types macro, but it needs to be done here, too, since this
     function is called directly when the flags must be specified. */
  if (type_1 == type_2) {
    identical = TRUE;
    goto done;
  }  /* if */
  /* Now check for typeref equivalence: This includes type qualifiers and
     decltype/typeof constructs. */
check_typerefs:
  if (type_1->kind == (a_type_kind)tk_typeref ||
      type_2->kind == (a_type_kind)tk_typeref) {
    a_type_qualifier_set  tqs1 = TQ_NONE, tqs2 = TQ_NONE;
    a_boolean             type_op = FALSE;
    a_type_ptr            tp1 = type_1, tp2 = type_2;
    while (tp1->kind == (a_type_kind)tk_typeref) {
      if (!has_name(tp1)) {
        if (typeref_is_type_operator(tp1)) {
          type_op = TRUE;
        } else {
          tqs1 |= tp1->variant.typeref.qualifiers;
        }  /* if */
      }  /* if */
      tp1 = tp1->variant.typeref.type;
    } /* while */
    while (tp2->kind == (a_type_kind)tk_typeref) {
      if (!has_name(tp2)) {
        if (typeref_is_type_operator(tp2)) {
          type_op = TRUE;
        } else {
          tqs2 |= tp2->variant.typeref.qualifiers;
        }  /* if */
      }  /* if */
      tp2 = tp2->variant.typeref.type;
    } /* while */
    if (!(flags & ITF_IGNORE_TOP_LEVEL_QUALIFIERS) && tqs1 != tqs2) {
      /* The type qualifiers do not match, so the types are not identical. */
      /* identical = FALSE;  -- Already set. */
      goto done;
    }  /* if */
    if ((flags & ITF_CHECK_DEDUCED_PLACEHOLDER_MATCH) != 0 &&
        (is_placeholder_deduction_match(type_1, type_2) ||
         is_placeholder_deduction_match(type_2, type_1))) {
      /* If we are checking the top-level return type of a specialization
         and either type is a placeholder type ("auto" or "decltype(auto)"),
         then a corresponding tk_typeref entry is always considered a match.
         For example:
             template<typename T> auto f(T t) { return t; }
             extern template auto f(int);
         Here, the explicit instantiation will cause "auto" to be deduced to
         "int" producing a deduced function type "int (int)", and when
         checking the deduced type against "auto (int)", we want the
         comparison to succeed.  Note that the tk_typeref entry cannot appear
         under another tk_typeref entry in such cases. */
      identical = TRUE;
      goto done;
    } else if (type_op) {
      /* At least one of the types involves a type operator like decltype or
         typeof. */
      if ((flags & ITF_CHECKING_DEDUCTION_RESULT) &&
               adjust_comparison_types_for_decltype(&type_1, &type_2)) {
        /* When checking a deduction result, we have to allow some slight
           differences around a typeref for a decltype.  The decltype might
           have been applied to an lvalue in one case and an rvalue in the
           other, and therefore have an extra "reference to" on it.
           Go back and check the cv-qualifiers again after the adjustment. */
        goto check_typerefs;
      } else if ((flags & ITF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED) &&
                 distinct_dependent_decltypes(type_1, type_2, flags)) {
        /* type_1 and type_2 are built from decltype (or typeof) constructs
           with distinct template-dependent arguments, and we've been asked
           to check for decltype expression differences. */
        goto done;
      }  /* if */
    }  /* if */
    /* Now that type qualifiers are no longer an issue, strip them and other
       typerefs off the types. */
    type_1 = tp1;
    type_2 = tp2;
  }  /* if */
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
    if (unknown_this_class_type) {
      /* Reset the unknown implicit this type flag so that it won't be passed
         to recursive calls of this routine. */
      flags &= ~ITF_UNKNOWN_THIS_CLASS_TYPE;
    }  /* if */
    if (flags & ITF_IGNORE_TOP_LEVEL_QUALIFIERS) {
      /* ITF_IGNORE_TOP_LEVEL_QUALIFIERS should only be passed to recursive
         calls for arrays (and only in C++ mode). */
      if (type_1->kind != (a_type_kind)tk_array || C_mode()) {
        flags &= ~ITF_IGNORE_TOP_LEVEL_QUALIFIERS;
      }  /* if */
    }  /* if */
    /* ITF_IGNORE_CALLING_CONVENTION should not be passed down. */
    if (flags & ITF_IGNORE_MS_CALLING_CONVENTION) {
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      ignore_ms_calling_convention = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
      flags &= ~ITF_IGNORE_MS_CALLING_CONVENTION;
    }  /* if */
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
              type_1->variant.integer.char16_t_type ==
                                      type_2->variant.integer.char16_t_type &&
              type_1->variant.integer.char32_t_type ==
                                      type_2->variant.integer.char32_t_type &&
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
                   secondary_translation_unit_seen() &&
                   type_1->variant.integer.enum_type &&
                   type_2->variant.integer.enum_type) {
          /* The types are expected to be identical, but because they are
             presumably defined in two different translation units, the
             correspondence of their inner structure must be checked. */
          identical = seek_type_corresp(type_1, type_2);
        }  /* if */
        break;
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
        if (same_fixed_point_type(type_1, type_2)) {
          identical = TRUE;
        }  /* if */
        break;
#endif /* FIXED_POINT_ALLOWED */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_extensions &&
            (!equiv_pointer_modifiers(type_1->variant.pointer.modifiers,
                                      type_2->variant.pointer.modifiers) ||
             type_1->variant.pointer.base_variable !=
                                      type_2->variant.pointer.base_variable)) {
          /* If pointer modifiers were applied, they must be identical.
             Likewise if the pointer is based on a variable, it must be the
             same variable. */
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (il_identical ||
            (type_1->variant.pointer.is_reference ==
                                type_2->variant.pointer.is_reference &&
#if MICROSOFT_EXTENSIONS_ALLOWED
             type_1->variant.pointer.is_handle ==
                                type_2->variant.pointer.is_handle &&
             same_cli_pointer_kinds(type_1, type_2) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
             type_1->variant.pointer.is_rvalue_reference ==
                                type_2->variant.pointer.is_rvalue_reference)) {
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
          if ((flags & ITF_SEEK_CORRESP) != 0 &&
              secondary_translation_unit_seen()) {
            /* The types are expected to be identical, but because they are
               presumably defined in two different translation units, the
               correspondence of their inner structure must be checked. */
            identical = seek_type_corresp(type_1, type_2);
          }  /* if */
        } else {
          a_boolean  parametered;
          parametered = type_1->variant.class_struct_union.is_nonreal_class &&
                        type_2->variant.class_struct_union.is_nonreal_class;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (cli_or_cx_enabled) {
            if ((is_cli_generic_constraint(type_1) &&
                 is_cli_generic_constraint(type_2)) ||
                (is_cli_open_constructed_instance(type_1) &&
                 is_cli_open_constructed_instance(type_2))) {
              parametered = TRUE;
            }  /* if */
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          if (parametered &&  /* For speed. */
              equiv_class_types(
                        type_1, type_2, /*error_matches_anything=*/FALSE,
                        (flags & ITF_EXACT_TEMPLATE_PARAM_TYPE_REQUIRED) != 0,
                        (flags & ITF_CONTEXTUAL_GENERIC_PARAMETERS) != 0)) {
            identical = TRUE;
          }  /* if */
        }  /* if */
        break;
      case tk_routine:
        {
          a_boolean	   this_class_matches = FALSE;
          a_type_ptr	   this1;
          a_type_ptr	   this2;
          an_itf_flag_set  rt_flags = flags;
          rtsp1 = type_1->variant.routine.extra_info;
          rtsp2 = type_2->variant.routine.extra_info;
          this1 = rtsp1->this_class;
          this2 = rtsp2->this_class;
          if (this1 == NULL && this2 == NULL) {
            /* Both this parameter types are NULL.  We still need to
               check the qualifiers for cases like "typedef void F() const". */
            this_class_matches = rtsp1->qualifiers == rtsp2->qualifiers;
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
          if (rtsp1->ref_qualifiers != rtsp2->ref_qualifiers) {
            /* If ref-qualifiers don't match, the types don't match. */
            this_class_matches = FALSE;
          }  /* if */
          /* For functions, the return types must be identical, the
             parameter lists must be identical, and the implicit "this"
             parameter type (if any) must be identical. */
          if (deduced_return_types_enabled &&
              (flags & ITF_CHECKING_DEDUCTION_RESULT) != 0) {
            rt_flags |= ITF_CHECK_DEDUCED_PLACEHOLDER_MATCH;
          }  /* if */
          if (this_class_matches &&
              f_identical_types(type_1->variant.routine.return_type,
                                type_2->variant.routine.return_type,
                                rt_flags) &&
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
              a_type_ptr param_1_type = list1->type;
              a_type_ptr param_2_type = list2->type;
              if (list1->is_parameter_pack != list2->is_parameter_pack) {
                identical = FALSE;
                break;
              }  /* if */
              if (flags & ITF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED) {
                /* If both parameters were originally declared as arrays
                   with template-dependent bounds, switch to the array
                   types so we can compare the bounds expressions. */
                set_up_array_param_type_comparison(list1, list2,
                                                   &param_1_type,
                                                   &param_2_type);
              }  /* if */
              if (!f_identical_types(param_1_type, param_2_type, flags)) {
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
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
            if (identical && (ms_extensions || gnu_mode) &&
                !ignore_ms_calling_convention) {
              /* The types are identical so far.  Check the calling
                 conventions. */
              identical = calling_conventions_are_compatible(type_1, type_2);
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
          }  /* if */
          if (identical &&
              (flags & ITF_EXACT_DOES_NOT_RETURN_MATCH_REQUIRED) != 0 &&
              rtsp1->does_not_return != rtsp2->does_not_return) {
            /* Routines differ in setting of do_not_return flag. */
            identical = FALSE;
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
          if (gnu_mode && identical &&
              type_1->alignment != type_2->alignment) {
            /* The types have different alignment attributes, so the types are
               different. */
            identical = FALSE;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        }
        break;
      case tk_ptr_to_member:
        /* Pointer-to-member types are identical if they refer to the same
           class type and to the same member type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_extensions &&
            type_1->variant.ptr_to_member.modifiers !=
                                  type_2->variant.ptr_to_member.modifiers) {
          /* If pointer modifiers were applied, they must be identical. */
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          identical = (f_identical_types(pm_class_type(type_1),
                                         pm_class_type(type_2),
                                         flags) &&
                       f_identical_types(pm_member_type(type_1),
                                         pm_member_type(type_2),
                                         flags));
        }  /* if */
        break;
      case tk_template_param:
        /* Compare template parameters or generic parameters, as well as
           generalizations of such constructs (e.g., "T::X" for some template
           parameter T).  Generic parameters and ordinary template parameters
           are never identical. */
        if (type_1->variant.template_param.kind ==
                                  type_2->variant.template_param.kind &&
            type_1->variant.template_param.is_generic_param ==
                            type_2->variant.template_param.is_generic_param &&
            (flags & ITF_EXACT_TEMPLATE_PARAM_TYPE_REQUIRED) == 0) {
          a_template_param_type_supplement_ptr	tptsp_1, tptsp_2;
          tptsp_1 = type_1->variant.template_param.extra_info;
          tptsp_2 = type_2->variant.template_param.extra_info;
          switch (type_1->variant.template_param.kind) {
            case tptk_param:
               /* Template parameter types are considered to be identical
                  if their positions in the template parameter list are
                  the same, and they are associated with template
                  declarations of the same nesting level.  For generic
                  parameters, alternative rules are used if the flag
                  ITF_CONTEXTUAL_GENERIC_PARAMETERS is passed. */
#if MICROSOFT_EXTENSIONS_ALLOWED
              if ((flags & ITF_CONTEXTUAL_GENERIC_PARAMETERS) != 0 &&
                  type_1->variant.template_param.is_generic_param) {
                if (type_1->variant.template_param.is_generic_function_param !=
                    type_2->variant.template_param.is_generic_function_param) {
                  /* Generic function parameters are always distinct from
                     generic class parameters. */
                  identical = FALSE;
                } else if (type_1->variant.template_param
                                                  .is_generic_function_param) {
                  /* For generic functions, only consider the parameter
                     in its own parameter list (and ignore the generic
                     nesting depth). */
                  identical = (tptsp_1->coordinates.position ==
                                               tptsp_2->coordinates.position);
                } else {
                  /* For generic classes, compare the sequence number assigned
                     across all nested generics.  E.g., in
                       generic<class T> ref struct S {
                         generic<class U> interface struct I {};  // #1
                       };
                       generic<class T, class U> ref struct R {}; // #2
                     the U in #1 and #2 are the same types. */ 
                  check_assertion(tptsp_1->generic_param_seq_number != 0);
                  check_assertion(tptsp_2->generic_param_seq_number != 0);
                  identical = (tptsp_1->generic_param_seq_number ==
                                           tptsp_2->generic_param_seq_number);
                }  /* if */
              } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              /* Do not insert code here. */
              {
                identical =
                    (tptsp_1->coordinates.position ==
                                            tptsp_2->coordinates.position) &&
                      ((flags & ITF_EXACT_NESTING_DEPTHS_REQUIRED) != 0
                        ? tptsp_1->coordinates.depth ==
                                                   tptsp_2->coordinates.depth
                        : ((equiv_nesting_depths(tptsp_1->coordinates.depth,
                                                tptsp_2->coordinates.depth) ||
                            (flags & ITF_IGNORE_NESTING_DEPTH) != 0)));
              }  /* if */
              break;
            case tptk_member:
              /* Members types are the same if their names are the same
                 and if they are members of identical types. */
              sym_1 = (a_symbol_ptr)type_1->source_corresp.assoc_info;
              sym_2 = (a_symbol_ptr)type_2->source_corresp.assoc_info;
              if (in_front_end) {
                check_assertion(sym_1 != NULL && sym_2 != NULL);
                if (sym_1->header == sym_2->header) {
                  /* The names are the same. */
                  identical = (identical_types(parent_class_of(type_1),
                                               parent_class_of(type_2)));
                }  /* if */
              } else {
                check_assertion(prototype_instantiations_in_il);
                /* We only have a limited ability to compare these types in a
                   back end where we have no symbol information. */
                identical = (sym_1 == sym_2);
              }  /* if */
              if (identical &&
                  !identical_types(parent_class_of(type_1),
                                   parent_class_of(type_2))) {
                identical = FALSE;
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
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
        /* For vectors, the sizes must be the same and the element types
           must be identical. */
        if (f_identical_types(type_1->variant.vector.element_type,
                              type_2->variant.vector.element_type,
                              flags) &&
            type_1->size == type_2->size &&
            type_1->alignment == type_2->alignment) {
          identical = TRUE;
        }  /* if */
        break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      case tk_nullptr:
        /* The managed nullptr type and std::nullptr_t are distinguished by
           the former being an incomplete type while the latter is
           complete. */
        identical = (type_1->incomplete == type_2->incomplete);
        break;
      default:
        unexpected_condition_str("f_identical_types: bad type");
    }  /* switch */
  }  /* if */
done:
  return identical;
}  /* f_identical_types */


a_boolean cast_identical_types(a_type_ptr type_1,
                               a_type_ptr type_2)
/*
Similar to identical_types, but used to decide whether or not two types are
identical in the sense that an implicit cast from one to the other doesn't
require a cast in the IL.  This is intended to be configurable; if one
wanted, say, to preserve an implicit type change to a typedef with the
same underlying type, one could make this routine stricter about that
difference, and that would force the generation of IL casts for such
type changes.
*/
{
  a_boolean identical = identical_types(type_1, type_2);

  if (identical) {
    if (C_mode()) {
      type_1 = skip_typerefs(type_1);
      type_2 = skip_typerefs(type_2);
      if (type_1->kind == (a_type_kind)tk_integer &&
          !type_1->variant.integer.enum_type &&
          !type_2->variant.integer.enum_type &&
          type_1->variant.integer.enum_info.affiliated_type !=
                           type_2->variant.integer.enum_info.affiliated_type) {
        /* In C mode, a cast that strips the affiliated type from an enum
           constant should be rendered explicitly.  (Both types are int, but
           one is enum-int.) */
        identical = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return identical;
}  /* cast_identical_types */


a_boolean param_types_are_compatible_full(
                                     a_type_ptr                   rout_type_1,
                                     a_type_ptr                   rout_type_2,
                                     a_type_compat_flags_set      flags,
                                     a_type_difference_descr_ptr  diffs)
/*
rout_type_1 and rout_type_2 point to routine type entries.  Return TRUE if the
parameter lists are compatible.  The "this" parameter types (if any) are
not compared.  flags is a set of bit flags that modify the comparison.  diffs
records certain differences that do no otherwise affect the outcome of the
comparison (currently, only calling convention differences when flags includes
TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS).
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
  if (rtsp1->has_ellipsis != rtsp2->has_ellipsis &&
      !(gcc_mode && (rtsp1->has_ellipsis || !list1_prototyped) == 
                                 (rtsp2->has_ellipsis || !list2_prototyped))) {
    /* One has a variable length parameter list and the other does not, so
       they cannot be compatible.  (Except in GNU C mode, where an unprototyped
       definition can be provided for a routine that was previously declared
       with a prototype.) */
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
      if (list1->is_parameter_pack != list2->is_parameter_pack) {
        compatible = FALSE;
        goto done;
      }  /* if */
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
      if (flags & TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED) {
        /* If both parameters were originally declared as arrays with
           template-dependent bounds, switch to the array types so we
           can compare the bounds expressions. */
        set_up_array_param_type_comparison(list1, list2,
                                           &param_1_type, &param_2_type);
      }  /* if */
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
      if (f_types_are_compatible_full(param_1_type, param_2_type, flags,
                                      diffs)) {
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
#if GNU_EXTENSIONS_ALLOWED
      } else if (gcc_mode &&
                 transparent_union_match(param_1_type, param_2_type)) {
#endif /* GNU_EXTENSIONS_ALLOWED */
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
}  /* param_types_are_compatible_full */

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED

a_boolean calling_conventions_are_compatible(a_type_ptr type1,
                                             a_type_ptr type2)
/*
Return TRUE if the calling conventions of the two given function types
are compatible.  That means they are identical or one is cc_default and
the other matches default_calling_convention (or cc_thiscall in the case
of Microsoft-mode member functions).
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
  if (ms_extensions && targ_supports_x86_64) {
    /* Microsoft x86-64 conventions only distinguish __vectorcall and __clrcall
       from other conventions.  All other conventions (__cdecl, __fastcall,
       etc.) are accepted but have no effect. */
    if (cc1 != (a_calling_convention)cc_vectorcall &&
        cc1 != (a_calling_convention)cc_clrcall) {
      cc1 = (a_calling_convention)cc_default;
    }  /* if */
    if (cc2 != (a_calling_convention)cc_vectorcall &&
        cc2 != (a_calling_convention)cc_clrcall) {
      cc2 = (a_calling_convention)cc_default;
    }  /* if */
  }  /* if */
  if (cc1 == cc2) {
    compatible = TRUE;
  } else if (cc1 == (a_calling_convention)cc_default) {
    if (ms_extensions && rtsp1->this_class != NULL) {
      /* The default calling convention for member functions is cc_thiscall. */
      compatible = (cc2 == (a_calling_convention)cc_thiscall);
    } else {
      compatible = (cc2 ==  default_calling_convention);
    }  /* if */
  } else if (cc2 == (a_calling_convention)cc_default) {
    if (ms_extensions && rtsp2->this_class != NULL) {
      /* The default calling convention for member functions is cc_thiscall. */
      compatible = (cc1 == (a_calling_convention)cc_thiscall);
    } else {
      compatible = (cc1 ==  default_calling_convention);
    }  /* if */
  }  /* if */
  return compatible;
}  /* calling_conventions_are_compatible */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */

a_boolean f_types_are_compatible_full(a_type_ptr                   type_1,
                                      a_type_ptr                   type_2,
                                      a_type_compat_flags_set      flags,
                                      a_type_difference_descr_ptr  diffs)
/*
Compare two given types for compatibility (which means that the types are the
same or almost the same; C and C++ differ in some details in this respect).
flags is a set of bits indicating options (e.g., is an error type considered
compatible with any other type).  If diffs is non-NULL, certain differences
encountered during the comparison are recorded in *diffs: Currently, this is
limited to calling convention differences encountered while comparing types
with the TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS flag.
This routine should generally not be called directly; it's meant to be called
by the macros types_are_compatible, types_are_strictly_compatible, and
types_are_compatible_ignoring_qualifiers, which do an initial test for exact
pointer equality.
*/
{
  a_boolean                     compat = FALSE;
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_boolean                     ignore_type_qualifiers = FALSE;
  a_boolean                     ignore_calling_conventions = FALSE;
  a_boolean                     error_matches_anything;
  a_boolean                     is_impl_conv;
  a_boolean                     top_level_for_redeclaration = FALSE;
  a_boolean			allow_base_derived_this_match;

  db_enter(5, "f_types_are_compatible_full");

  error_matches_anything = 
                 (flags & TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) != 0;
  ignore_type_qualifiers = (flags & TCF_IGNORE_TYPE_QUALIFIERS) != 0;
  allow_base_derived_this_match =
                              (flags & TCF_ALLOW_BASE_DERIVED_THIS_MATCH) != 0;
  /* The TCF_ALLOW_BASE_DERIVED_THIS_MATCH is not passed along when
     this routine calls itself. */
  flags &= ~(a_type_compat_flags_set)TCF_ALLOW_BASE_DERIVED_THIS_MATCH;
  if (flags & TCF_REDECLARATION) {
    /* TCF_REDECLARATION implies exact decltype expression matching, and
       not just at the top level. */
    flags |= TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED;
  }  /* if */
  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls.  Do not use the same_entities
     macro: for this routine a slightly more thorough check is desirable
     (so it can be called from the correspondence checking code). */
  if (type_1 == type_2) {
    compat = TRUE;
  } else {
    /* Test for a qualifier mismatch. */
    a_boolean qualifier_mismatch;
    if (type_1->kind == (a_type_kind)tk_typeref ||
        type_2->kind == (a_type_kind)tk_typeref) {
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      if ((type_1->kind == (a_type_kind)tk_typeref &&
           (typeref_is_typedef(type_1) || typeref_is_type_operator(type_1))) ||
          (type_2->kind == (a_type_kind)tk_typeref &&
           (typeref_is_typedef(type_2) || typeref_is_type_operator(type_2)))) {
        /* Do not record calling convention differences under typedefs or
           type operators (like decltype). */
        flags &= ~TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
check_typerefs:
      qualifier_mismatch = FALSE;
      if (!ignore_type_qualifiers &&
          !type_qualifiers_match(type_1, type_2)) {
        qualifier_mismatch = TRUE;
      }  /* if */
      /* Except for potential qualifier mismatches, typeref entries can
         usually be skipped, but some care must be taken with decltype/typeof
         entries. */
      if ((flags & TCF_CHECK_DEDUCED_PLACEHOLDER_MATCH) != 0 &&
          (is_placeholder_deduction_match(type_1, type_2) ||
           is_placeholder_deduction_match(type_2, type_1))) {
        /* If we are checking the top-level return type of a redeclaration
           and either type is a placeholder type ("auto" or "decltype(auto)"),
           then a corresponding tk_typeref entry is always considered a match.
           For example:
               auto f() { return 1; }  // Deduces an "int" return type.
               auto f();  // "auto" is compatible with the deduced "int",
                          // which will have a special tk_typeref marker.
           Note that the tk_typeref entry cannot appear under another
           tk_typeref entry in such cases. */
        compat = TRUE;
        goto done;
      } else if ((flags & TCF_CHECKING_DEDUCTION_RESULT) &&
          adjust_comparison_types_for_decltype(&type_1, &type_2)) {
        /* When checking a deduction result, we have to allow some slight
           differences around a typeref for a decltype.  The decltype might
           have been applied to an lvalue in one case and an rvalue in the
           other, and therefore have an extra "reference to" on it.
           Go back and check the cv-qualifiers again after the adjustment. */
        goto check_typerefs;
      } else if ((flags & TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED)) {
        if (distinct_dependent_decltypes(type_1, type_2, ITF_NO_FLAGS)) {
          /* There's a difference due to a dependent decltype expression, and
             we've been asked to check those. */
          goto done;
        }  /* if */
      }  /* if */
      type_1 = skip_typerefs(type_1);
      type_2 = skip_typerefs(type_2);
    } else {
      qualifier_mismatch = FALSE;
    }  /* if */
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
      compat = f_types_are_compatible_full(type_1, type_2, flags, diffs);
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
      switch (type_1->kind) {
        case tk_error:
          /* Error types are not compatible by the test above, so they are not
             compatible here. */
          compat = FALSE;
          break;
        case tk_unknown:
        case tk_void:
        case tk_nullptr:
          /* No further check needed.  The types are compatible. */
          compat = TRUE;
          break;
        case tk_integer:
          if ((C_dialect == C_dialect_cplusplus ||
               (flags & TCF_SEEK_CORRESP) != 0) &&
              (type_1->variant.integer.enum_type ||
               type_2->variant.integer.enum_type)) {
            /* In C++, each enum type is a distinct type and is not compatible
               with any other type.  In C and C99, when looking for cross-
               translation compatibility, similar rules apply. */
          } else if (type_1->variant.integer.enum_type &&
                     type_2->variant.integer.enum_type &&
                     !(microsoft_mode || (gcc_mode && gnu_version < 30400)) &&
                     C_mode()) {
            /* In C modes, an enum type may be compatible with an integer type,
               but two different enum types are not compatible.  We do not
               apply the latter rule when emulating Microsoft C or early GNU C
               versions. */
          } else {
            if (type_1->variant.integer.int_kind ==
                                           type_2->variant.integer.int_kind &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                type_1->variant.integer.microsoft_sized_int_type ==
                            type_2->variant.integer.microsoft_sized_int_type &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                type_1->variant.integer.wchar_t_type ==
                                        type_2->variant.integer.wchar_t_type &&
                type_1->variant.integer.char16_t_type ==
                                       type_2->variant.integer.char16_t_type &&
                type_1->variant.integer.char32_t_type ==
                                       type_2->variant.integer.char32_t_type &&
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
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
          if (same_fixed_point_type(type_1, type_2)) {
            compat = TRUE;
          }  /* if */
          break;
#endif /* FIXED_POINT_ALLOWED */
        case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_complex:
        case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
          compat = (type_1->variant.float_kind == type_2->variant.float_kind);
          break;
        case tk_pointer:
          /* For pointers and references, they must be both pointers or both
             references and must point to compatible types.  Same for
             C++/CLI handles and tracking references. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (ms_extensions &&
              (!equiv_pointer_modifiers(type_1->variant.pointer.modifiers,
                                        type_2->variant.pointer.modifiers) ||
               type_1->variant.pointer.base_variable !=
                                      type_2->variant.pointer.base_variable)) {
            /* If pointer modifiers were applied, they must be identical.
               Likewise if the pointer is based on a variable, it must be the
               same variable. */
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          if (type_1->variant.pointer.is_reference ==
                                 type_2->variant.pointer.is_reference &&
#if MICROSOFT_EXTENSIONS_ALLOWED
              type_1->variant.pointer.is_handle ==
                                 type_2->variant.pointer.is_handle &&
              same_cli_pointer_kinds(type_1, type_2) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              type_1->variant.pointer.is_rvalue_reference ==
                                 type_2->variant.pointer.is_rvalue_reference) {
            compat = f_types_are_compatible_full(type_1->variant.pointer.type,
                                                 type_2->variant.pointer.type,
                                                 flags, diffs)
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
            if (f_types_are_compatible_full(type_1->variant.array.element_type,
                                            type_2->variant.array.element_type,
                                            sub_flags, diffs)) {
              /* Check that the bounds match. */
              if (identical_array_type_level(type_1, type_2)) {
                compat = TRUE;
              } else if (vla_enabled &&
                         (array_is_vla(type_1) || array_is_vla(type_2))) {
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
              equiv_class_types(
                          type_1, type_2, error_matches_anything,
                          /*exact_templ_arg_match_required=*/FALSE,
                          (flags & TCF_CONTEXTUAL_GENERIC_PARAMETERS) != 0)) {
            compat = TRUE;
          }  /* if */
          break;
        case tk_routine:
          /* For functions, the return types must be compatible, the parameter
             types must be compatible, and the "this" parameter types (if any)
             must be compatible. */
          { a_type_compat_flags_set  rt_flags;
            /* The flag indicating that calling conventions should be ignored
               does not apply to function types on which this function type
               is based. */
            if (flags & TCF_IGNORE_CALLING_CONVENTIONS) {
              ignore_calling_conventions = TRUE;
              flags &= ~TCF_IGNORE_CALLING_CONVENTIONS;
            }  /* if */
            rtsp1 = type_1->variant.routine.extra_info;
            rtsp2 = type_2->variant.routine.extra_info;
            if (flags & TCF_IGNORE_RETURN_TYPE_QUALIFIERS) {
              /* Don't propagate the flag to embedded function types. */
              flags &= ~TCF_IGNORE_RETURN_TYPE_QUALIFIERS;
              rt_flags = flags | TCF_IGNORE_TYPE_QUALIFIERS;
            } else {
              rt_flags = flags;
            }  /* if */
            if (deduced_return_types_enabled) {
              rt_flags |= TCF_CHECK_DEDUCED_PLACEHOLDER_MATCH;
            }  /* if */
            if (f_types_are_compatible_full(
                                          type_1->variant.routine.return_type,
                                          type_2->variant.routine.return_type,
                                          rt_flags, diffs) &&
                param_types_are_compatible_full(type_1, type_2, flags,
                                                diffs) &&
                rtsp1->ref_qualifiers == rtsp2->ref_qualifiers &&
                ((flags & TCF_IGNORE_THIS_CLASS_TYPE) ||
                 (rtsp1->qualifiers == rtsp2->qualifiers &&
                  ((rtsp1->this_class == NULL) ?
                      (rtsp2->this_class == NULL) :
                      (rtsp2->this_class != NULL &&
                       (f_types_are_compatible_full(rtsp1->this_class,
                                                    rtsp2->this_class, flags,
                                                    diffs) ||
                        /* In some cases, deduction can produce a routine
                           type in which the second type has a this_class that
                           is a base class of the first.  The special
                           TCF_ALLOW_BASE_DERIVED_THIS_MATCH is passed to
			   indicate this case. */
                        (inexact_ptr_to_member_deduction_enabled &&
                         allow_base_derived_this_match &&
                         find_base_class_of(rtsp1->this_class,
                                            rtsp2->this_class))))))) &&
                (ignore_calling_conventions ||
                 routine_linkages_are_compatible(
                             (a_name_linkage_kind)rtsp1->routine_name_linkage,
                             (a_name_linkage_kind)rtsp2->routine_name_linkage,
                             is_impl_conv))) {
              a_boolean  result = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
              if ((ms_extensions || gnu_mode) &&
                  (!ignore_calling_conventions ||
                   ((flags & TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS) &&
                    diffs != NULL))) {
                /* Check calling conventions, either because it affects
                   compatibility, or because the caller is interested in a
                   record of differences. */
                if (!calling_conventions_are_compatible(type_1, type_2)) {
                  if ((flags & TCF_RECORD_DIRECT_CALLING_CONVENTION_DIFFS) &&
                      diffs != NULL) {
                    /* Calling convention differences don't affect
                       compatibility, but the caller requested a record of
                       such differences. */
                    a_type_list_entry_ptr  tep1 = alloc_type_list_entry(),
                                           tep2 = alloc_type_list_entry();
                    tep1->type = type_1;
                    tep1->next = tep2;
                    tep2->type = type_2;
                    tep2->next = diffs->incompatible_calling_conventions;
                    diffs->incompatible_calling_conventions = tep1;
                  } else {
                    result = FALSE;
                  }  /* if */
                }  /* if */
              }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
              compat = result;
            }  /* if */
          }
          break;
        case tk_ptr_to_member:
          /* Pointer-to-member types are compatible if they refer to the same
             class type and their member types are compatible. */
          if (flags & TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE) {
            flags |= TCF_IGNORE_THIS_CLASS_TYPE;
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode &&
              type_1->variant.ptr_to_member.modifiers !=
                                    type_2->variant.ptr_to_member.modifiers) {
            /* A difference in __ptr32 or ptr64 modifiers makes pointer types
               incompatible. */
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          if (f_types_are_compatible_full(pm_member_type(type_1),
                                          pm_member_type(type_2), flags,
                                          diffs)) {
            if ((flags & TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE) ||
                f_types_are_compatible_full(pm_class_type(type_1),
                                            pm_class_type(type_2), flags,
                                            diffs)) {
              compat = TRUE;
            }  /* if */
          }  /* if */
          break;
        case tk_template_param:
          /* Template parameter types are considered to be compatible if
             their positions in the template parameter list are the same. */
          { an_itf_flag_set  it_flags = ITF_NO_FLAGS;
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (flags & TCF_CONTEXTUAL_GENERIC_PARAMETERS) {
              it_flags |= ITF_CONTEXTUAL_GENERIC_PARAMETERS;
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            if (flags & TCF_IGNORE_NESTING_DEPTH) {
              it_flags |= ITF_IGNORE_NESTING_DEPTH;
            }  /* if */
            compat = f_identical_types(type_1, type_2, it_flags);
          }
          break;
#if GNU_VECTOR_TYPES_ALLOWED
        case tk_vector:
          /* For vectors, the sizes must be the same and the element types
             must be identical. */
          if (f_identical_types(type_1->variant.vector.element_type,
                                type_2->variant.vector.element_type,
                                flags) &&
              type_1->size == type_2->size) {
            compat = TRUE;
          }  /* if */
          break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        default:
          unexpected_condition_str("f_types_are_compatible_full: bad type");
      }  /* switch */
#if GNU_EXTENSIONS_ALLOWED
      if (gnu_mode && compat && type_1->kind != (a_type_kind)tk_routine &&
          !same_alignment_attributes(type_1, type_2)) {
        /* The types have different alignments, so the types are
           incompatible. */
        if (error_matches_anything &&
            (is_or_contains_error_type(type_1) ||
             is_or_contains_error_type(type_2))) {
          /* If an error type match is involved, ignore an alignment
             difference. */
        } else {
          compat = FALSE;
        }  /* if */
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
done:
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "f_types_are_compatible_full: %s\n",
            compat ? "TRUE":"FALSE");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return compat;
}  /* f_types_are_compatible_full */


a_boolean f_types_are_compatible(a_type_ptr              type_1,
				 a_type_ptr              type_2,
				 a_type_compat_flags_set flags)
/*
Convenience function for f_types_are_compatible_full when specific type
differences need not be recorded.
*/
{
  return f_types_are_compatible_full(type_1, type_2, flags,
                                     (a_type_difference_descr_ptr)NULL);
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
  a_boolean interch = FALSE;

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
    /* Pointer or C++/CLI handle types.  Get the underlying types. */
    a_type_ptr und_type_1 = type_pointed_to(type_1);
    a_type_ptr und_type_2 = type_pointed_to(type_2);
    a_type_ptr ptr_type_1 = skip_typerefs(und_type_1);
    a_type_ptr ptr_type_2 = skip_typerefs(und_type_2);
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
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (named_address_spaces_enabled && interch) {
      /* Check that the named address spaces specified are not different. */
      a_type_qualifier_set     qualifiers;
      a_named_address_space_id nas_1, nas_2;
      qualifiers = get_type_qualifiers(und_type_1);
      nas_1 = named_address_space_from_qualifier_set(qualifiers);
      qualifiers = get_type_qualifiers(und_type_2);
      nas_2 = named_address_space_from_qualifier_set(qualifiers);
      if (nas_1 != nas_2) interch = FALSE;
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
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
  std_conv->secondary_type_qualifiers_added = FALSE;
  std_conv->pointer_normalization_needed = FALSE;
  std_conv->nontrivial_conversion = FALSE;
  std_conv->promotion = FALSE;
  std_conv->ptr_or_pm_to_bool = FALSE;
  std_conv->boxing_conversion = FALSE;
  std_conv->exception_spec_incompatibility = FALSE;
  std_conv->conv_of_string_literal_to_ptr_to_nonconst = FALSE;
  std_conv->warning_suggested = ec_no_error;
  std_conv->is_mild_warning = FALSE;
  std_conv->cli_array_covariance_conversion = FALSE;
  std_conv->gpp_conv_of_real_to_complex = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  std_conv->conv_of_string_literal_to_cli_string = FALSE;
  std_conv->param_array_conversion = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  std_conv->conv_to_std_initializer_list = FALSE;
}  /* clear_std_conv_descr */


static a_boolean dest_of_ptr_cast_big_enough(a_type_ptr source_type,
                                             a_type_ptr dest_type)
/*
Return TRUE if a value of type "source_type" will fit in an entity of
type "dest_type".  This is used in testing whether or not non-portable
casts involving pointers or nullptr types should be allowed.
*/
{
  source_type = skip_typerefs(source_type);
  check_assertion(is_pointer(source_type) || is_nullptr(source_type) ||
                  is_pointer(dest_type) || is_nullptr(dest_type));
  dest_type = skip_typerefs(dest_type);
  return (dest_type->size >= source_type->size);
}  /* dest_of_ptr_cast_big_enough */

#else /* STANDALONE_UTILITY_PROGRAM */

a_boolean f_standalone_identical_types(a_type_ptr type_1,
                                       a_type_ptr type_2)
/*
Return TRUE if the two types are identical, including separate copies of
identical types and pointers to identical types.  This is similar to the
processing in f_identical_types but limited so it does not use facilities
that are not present in standalone back ends and utilities.
*/
{
  a_boolean identical;

  if (type_1->kind == (a_type_kind)tk_typeref ||
      type_2->kind == (a_type_kind)tk_typeref) {
    if (!type_qualifiers_match(type_1, type_2)) {
      /* The type qualifiers are not identical, so the types are not. */
      identical = FALSE;
      goto done;
    }  /* if */
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
  }  /* if */
  if (same_entities(type_1, type_2)) {
    /* The types are the same. */
    identical = TRUE;
    goto done;
  }  /* if */
  if (type_1->kind != type_2->kind) {
    /* Different type kinds, so the types are different. */
    identical = FALSE;
    goto done;
  }  /* if */
  switch (type_1->kind) {
    case tk_error:
    case tk_unknown:
    case tk_void:
    case tk_nullptr:
      identical = TRUE;
      break;
    case tk_integer:
      identical = (!type_1->variant.integer.enum_type &&
                   !type_2->variant.integer.enum_type &&
                   type_1->variant.integer.int_kind ==
                                            type_2->variant.integer.int_kind &&
#if MICROSOFT_EXTENSIONS_ALLOWED
                   type_1->variant.integer.microsoft_sized_int_type ==
                            type_2->variant.integer.microsoft_sized_int_type &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                   type_1->variant.integer.wchar_t_type ==
                                        type_2->variant.integer.wchar_t_type &&
                   type_1->variant.integer.char16_t_type ==
                                       type_2->variant.integer.char16_t_type &&
                   type_1->variant.integer.char32_t_type ==
                                       type_2->variant.integer.char32_t_type &&
                   type_1->variant.integer.bool_type ==
                                            type_2->variant.integer.bool_type);
      break;
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      identical = same_fixed_point_type(type_1, type_2);
      break;
#endif /* FIXED_POINT_ALLOWED */
    case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      identical = (type_1->variant.float_kind == type_2->variant.float_kind);
      break;
    case tk_pointer:
      identical = (type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference &&
                   type_1->variant.pointer.is_rvalue_reference ==
                                 type_1->variant.pointer.is_rvalue_reference &&
#ifdef pointer_types_have_same_repr
                   pointer_types_have_same_repr(type_1, type_2) &&
#endif /* ifdef pointer_types_have_same_repr */
#if MICROSOFT_EXTENSIONS_ALLOWED
                   equiv_pointer_modifiers(
                                          type_1->variant.pointer.modifiers,
                                          type_2->variant.pointer.modifiers) &&
                   type_1->variant.pointer.base_variable ==
                                       type_2->variant.pointer.base_variable &&
                   type_1->variant.pointer.is_handle ==
                                       type_2->variant.pointer.is_handle &&
                   same_cli_pointer_kinds(type_1, type_2) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                   standalone_identical_types(type_1->variant.pointer.type,
                                              type_2->variant.pointer.type));
      break;
    case tk_array:
      identical = (!has_unknown_specified_bound(type_1) &&
                   !has_unknown_specified_bound(type_2) &&
                   type_1->variant.array.variant.number_of_elements ==
                            type_2->variant.array.variant.number_of_elements &&
                   standalone_identical_types(
                                          type_1->variant.array.element_type,
                                          type_2->variant.array.element_type));
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      /* Class/struct/union types that are not the same entity are not
         identical. */
      identical = FALSE;
      break;
    case tk_routine:
      {
        a_boolean                     this_class_matches;
        a_type_ptr                    this1;
        a_type_ptr                    this2;
        a_param_type_ptr              list1;
        a_param_type_ptr              list2;
        a_routine_type_supplement_ptr rtsp1 =
                                            type_1->variant.routine.extra_info;
        a_routine_type_supplement_ptr rtsp2 =
                                            type_2->variant.routine.extra_info;
        this1 = rtsp1->this_class;
        this2 = rtsp2->this_class;
        if (this1 == NULL && this2 == NULL) {
          /* Both this parameter types are NULL.  We still need to check
             the qualifiers for cases like "typedef void F() const". */
          this_class_matches = (rtsp1->qualifiers == rtsp2->qualifiers);
        } else if (this1 == NULL || this2 == NULL) {
          /* One, but not both, of the this parameter types are NULL, so
             the types do not match. */
          this_class_matches = FALSE;
        } else {
          /* Both types are non-null, see if they are identical. */
          this_class_matches = (rtsp1->qualifiers == rtsp2->qualifiers &&
                                standalone_identical_types(this1, this2));
        }  /* if */
        /* For functions, the return types must be identical, the parameter
           lists must be identical, and the implicit "this" parameter type
           (if any) must be identical. */
        identical = (this_class_matches &&
                     standalone_identical_types(type_1->
                                                variant.routine.return_type,
                                                type_2->
                                                variant.routine.return_type) &&
                     rtsp1->prototyped == rtsp2->prototyped &&
                     rtsp1->has_ellipsis == rtsp2->has_ellipsis &&
                     routine_linkages_are_identical(
                            (a_name_linkage_kind)rtsp1->routine_name_linkage,
                            (a_name_linkage_kind)rtsp2->routine_name_linkage));
        /* Compare the types of the parameters on the two lists. */
        for (list1 = rtsp1->param_type_list, list2 = rtsp2->param_type_list;
             identical && list1 != NULL && list2 != NULL;
             list1 = list1->next, list2 = list2->next) {
          identical = standalone_identical_types(list1->type, list2->type);
        }  /* for */
        if (identical) {
          /* The parameter lists are identical if they both ended
             together. */
          identical = (list1 == NULL && list2 == NULL);
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
        if (identical) {
          /* The types are identical so far.  Check the calling
             conventions.  (Note that this test simply checks that the
             conventions are the same; the corresponding test in
             f_identical_types uses calling_conventions_are_compatible,
             which is not available in a standalone utility because of its
             use of default_calling_convention, a front-end-only
             variable.) */
          identical = (rtsp1->calling_convention == rtsp2->calling_convention);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED */
      }
      break;
    case tk_ptr_to_member:
      identical = (
#if MICROSOFT_EXTENSIONS_ALLOWED
                   type_1->variant.ptr_to_member.modifiers ==
                                     type_2->variant.ptr_to_member.modifiers &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                   f_standalone_identical_types(pm_class_type(type_1),
                                                pm_class_type(type_2)) &&
                   f_standalone_identical_types(pm_member_type(type_1),
                                                pm_member_type(type_2)));
      break;
    case tk_template_param:
      /* Comparing template parameter types can only be done using
         front-end facilities. */
      identical = FALSE;
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      identical = (type_1->size == type_2->size &&
                   standalone_identical_types(type_1->
                                                 variant.vector.element_type,
                                              type_2->
                                                 variant.vector.element_type));
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_typeref:
    default:
      unexpected_condition_str("f_standalone_identical_types: bad type");
  }  /* switch */
#if GNU_EXTENSIONS_ALLOWED
  if (identical) {
    identical = same_alignment_attributes(type_1, type_2);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
done:
  return identical;
}  /* f_standalone_identical_types */
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

Note that a VLA array level is considered equivalent to any other
array type, by the (C99 standard-conforming) logic that for the right
value of the expression they are equivalent.
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
      if (types_are_both_pointers_or_both_handles(dest_type, source_type)) {
        if (dest_type->size != source_type->size
#ifdef pointer_types_have_same_repr
            || !pointer_types_have_same_repr(source_type, dest_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                                    ) {
          /* If the pointers have different representations, do not
             consider them the same. */
          same = FALSE;
        } else {
          /* Continue at the next level for pointers. */
          dest_type = type_pointed_to(dest_type);
          source_type = type_pointed_to(source_type);
        }  /* if */
      } else if (is_ptr_to_member(dest_type) &&
                 is_ptr_to_member(source_type)) {
        if (f_types_are_compatible(pm_class_type(dest_type),
                                   pm_class_type(source_type),
                                   TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING)) {
          /* Continue at the next level for pointers to members. */
          dest_type = pm_member_type(dest_type);
          source_type = pm_member_type(source_type);
        } else {
          same = FALSE;
        }  /* if */
      } else if (is_array(dest_type) && is_array(source_type)) {
        if (((!has_unknown_specified_bound(dest_type) &&
              !has_unknown_specified_bound(source_type) &&
              dest_type->variant.array.variant.number_of_elements ==
                      source_type->variant.array.variant.number_of_elements) ||
             (vla_enabled &&
              (array_is_vla(dest_type) || array_is_vla(source_type))))
#if UPC_EXTENSIONS_ALLOWED
             && (dest_type->variant.array.is_threads_dimension ==
                            source_type->variant.array.is_threads_dimension)
#endif /* UPC_EXTENSIONS_ALLOWED */
                                                                            ) {
          /* Continue at the next level for arrays. */
          dest_type = array_element_type(dest_type);
          source_type = array_element_type(source_type);
        } else {
          same = FALSE;
        }  /* if */
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
    /* If type is an unambiguous and public base class of other_type,
       a handler for other_type will catch type. */
    if (types_are_both_pointers_or_both_handles(type, other_type)) {
      /* The same goes if both are pointer or C++/CLI handle types. */
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


a_boolean is_nothrow_spec(an_exception_specification_ptr  esp)
/*
Return TRUE if the given exception specification is of the form "noexcept",
"noexcept(<true-constant>)", or "throw()".
*/
{
  a_boolean  result = FALSE;

  if (esp != NULL && !esp->arg_cached && !esp->indeterminate) {
    if (esp->throw_any) {
      /* This case eliminates "noexcept(<false-constant>)" and
         "noexcept(<template-dependent-constant>)". */
      result = FALSE;
    } else if (esp->is_noexcept) {
      result = TRUE;
    } else {
      result = esp->variant.exception_specification_type_list == NULL;
    }  /* if */
  }  /* if */
  return result;
}  /* is_nothrow_spec */


a_boolean is_nothrow_type(a_type_ptr  type)
/*
The given type is a routine type.  Return TRUE if it has an associated
"throw()" or "noexcept" specification or if exceptions are disabled (in which
case the function is not expected to throw an exception either).
The caller is responsible for ensuring that the type has no cached exception
specification and no indeterminate specification.
*/
{
  a_boolean  result;

  if (exceptions_enabled) {
    a_routine_type_supplement_ptr   rtsp = type->variant.routine.extra_info;
    an_exception_specification_ptr  esp = rtsp->exception_specification;
    if (esp != NULL && esp->indeterminate) {
      resolve_indeterminate_exception_specification(rtsp->assoc_routine);
      esp = rtsp->exception_specification;
    }  /* if */
    if (esp == NULL) {
      result = FALSE;
    } else {
      check_assertion_or_expect_error(!esp->arg_cached);
      result = is_nothrow_spec(esp);
    }  /* if */
  } else {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_nothrow_type */


a_boolean is_non_throwing_routine(a_routine_ptr rp)
/*
Return TRUE if the given routine cannot throw an exception.  See the
definition of "non-throwing exception specification" in [except.spec]
of the C++11 standard.
*/
{
  a_boolean result = (rp->is_trivial_default_constructor ||
                      rp->is_trivial_copy_function ||
                      rp->never_throws);

  if (!result) {
    if (rp->type->kind == (a_type_kind)tk_routine) {
      instantiate_exception_spec_if_needed(symbol_for(rp));
    }  /* if */
    result = is_nothrow_type(f_skip_typerefs(rp->type));
  }  /* if */
  return result;
}  /* is_non_throwing_routine */


a_boolean exception_spec_is_less_restrictive(
                                         an_exception_specification_ptr  esp1,
                                         an_exception_specification_ptr  esp2)
/*
Compare the given exception specifications.  Return TRUE if esp1 is less
restrictive than esp2.  An exception specification is considered "less
restrictive" than another if at least one is permitted by the first while
violating the second.  For example, the following are in order from most
restrictive to least restrictive:

  void f1() throw();              // Nothing will be thrown
  void f2() throw(T);
  void f3() throw(T, U);
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
  a_boolean  is_less_restrictive = FALSE;

  if (esp2 == NULL || esp2->throw_any) {
    /* The function associated with type2 can throw any exception; type1
       cannot be less restrictive than that. */
    /* is_less_restrictive = FALSE; */
  } else if (esp1 == NULL || esp1->throw_any) {
    /* type1's function can throw any exception, and type2's function has at
       least some restriction, so the former is less restrictive. */
    is_less_restrictive = TRUE;
  } else if (esp1->is_noexcept) {
    /* We already covered the "throw any" cases, so this must be a "never
       throws" case, which cannot be less restrictive than anything. */
    /* is_less_restrictive = FALSE; */
  } else if (esp2->is_noexcept) {
    /* We already covered the "throw any" cases, so this must be a "never
       throws" case.  esp1 is already known not to be a "noexcept" form, and
       therefore will be less restrictive, unless it represents "throw()". */
    is_less_restrictive =
                      esp1->variant.exception_specification_type_list != NULL;
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
    an_exception_specification_type_ptr  estp1, estp2;
    estp1 = esp1->variant.exception_specification_type_list;
    for (; estp1 != NULL; estp1 = estp1->next) {
      /* Ignore entries marked "redundant" -- the type has already been
         seen on the list. */
      if (estp1->redundant) continue;
      /* The inner loop traverses the types specified for type2, looking
         for an entry that matches the current entry from type1's list. */
      estp2 = esp2->variant.exception_specification_type_list;
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
  return is_less_restrictive;
}  /* exception_spec_is_less_restrictive */


a_boolean type_has_less_restrictive_exception_spec(a_type_ptr  type1,
                                                   a_type_ptr  type2)
/*
type1 and type2 are routine types (or typerefs with an underlying routine
type).  Return TRUE if the exception specification of type1 is less restrictive
than the exception specification of type2.  (For details regarding the "less
restrictive" relationship, see exception_spec_is_less_restrictive.)
*/
{
  a_boolean                            is_less_restrictive = FALSE;
  an_exception_specification_ptr       esp1, esp2;

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
      is_less_restrictive = exception_spec_is_less_restrictive(esp1, esp2);
    }  /* if */
  }  /* if */
  return is_less_restrictive;
}  /* type_has_less_restrictive_exception_spec */


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
  } else if (is_ptr_or_ref_type(type_1) &&
             is_ptr_or_ref_type(type_2)) {
    type_1 = type_pointed_to(type_1);
    type_2 = type_pointed_to(type_2);
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (is_function_type(type_1) && is_function_type(type_2)) {
      result = !(type_has_less_restrictive_exception_spec(type_1, type_2) ||
                 type_has_less_restrictive_exception_spec(type_2, type_1));
    }  /* if */
  } else if (is_ptr_to_member_type(type_1) &&
             is_ptr_to_member_type(type_2)) {
    type_1 = pm_member_type(type_1);
    type_2 = pm_member_type(type_2);
    type_1 = skip_typerefs(type_1);
    type_2 = skip_typerefs(type_2);
    if (is_function_type(type_1) && is_function_type(type_2)) {
      result = !(type_has_less_restrictive_exception_spec(type_1, type_2) ||
                 type_has_less_restrictive_exception_spec(type_2, type_1));
    }  /* if */
  } else if (is_function_type(type_1) && is_function_type(type_2)) {
    result = !(type_has_less_restrictive_exception_spec(type_1, type_2) ||
               type_has_less_restrictive_exception_spec(type_2, type_1));
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
        (type_has_less_restrictive_exception_spec(source_type, dest_type) ||
         !same_exception_spec_on_return_and_param_type(source_type,
                                                       dest_type))) {
      okay = FALSE;
    }  /* if */
  }  /* if */
  return okay;
}  /* exception_spec_conversion_possible */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean handle_microsoft_dropping_of_qualifiers(
                                  a_type_qualifier_set *source_type_qualifiers,
                                  a_type_qualifier_set *dest_type_qualifiers,
                                  a_type_ptr           dest_type,
                                  an_error_code        *warning_code)
/*
MSVC++ does some strange things with certain qualifiers in conversions.
Dropping __unaligned and __restrict is considered to be like adding those
qualifiers.  (This is from a source at Microsoft, and he didn't know why
that is done.)  *source_type_qualifiers and *dest_type_qualifiers give
the source and destination type qualifiers.  If they indicate the
strange cases we care about, adjust the qualifiers and set *warning_code
to indicate the particular weird case.  Otherwise, leave *warning_code
unchanged.  dest_type is the destination type (possibly with typerefs
not stripped), for use in a test.  Return TRUE if any adjustment
was made.
*/
{
  a_boolean adj_made = FALSE;

  check_assertion(microsoft_mode);
  if ((*source_type_qualifiers & TQ_UNALIGNED) &&
      !(*dest_type_qualifiers  & TQ_UNALIGNED)) {
    *source_type_qualifiers &=  ~TQ_UNALIGNED;
    *dest_type_qualifiers   |=   TQ_UNALIGNED;
    adj_made = TRUE;
    if (f_skip_typerefs(dest_type)->alignment != 1) {
      *warning_code = ec_unaligned_qualifier_dropped;
    }  /* if */
  }  /* if */
  if ((*source_type_qualifiers & TQ_RESTRICT) &&
      !(*dest_type_qualifiers  & TQ_RESTRICT)) {
    *source_type_qualifiers &=  ~TQ_RESTRICT;
    *dest_type_qualifiers   |=   TQ_RESTRICT;
    adj_made = TRUE;
    *warning_code = ec_restrict_qualifier_dropped;
  }  /* if */
  return adj_made;
}  /* handle_microsoft_dropping_of_qualifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean qualification_conversion_possible_full(
                                        a_type_ptr    source_type,
                                        a_type_ptr    dest_type,
                                        a_boolean     *p_qualifiers_added,
                                        a_boolean     ignore_underlying_type,
                                        an_error_code *warning_suggested,
                                        a_type_ptr    *underlying_source_type,
                                        a_type_ptr    *underlying_dest_type)
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
underlying type of either source_type or dest_type and return the types
that were reached in underlying_source_type and underlying_dest_type if
requested to do so by the caller by providing non-NULL values for those
parameters.  If warning_suggested is non-NULL, it will be set to any
warning suggested for the conversion, or to ec_no_error if no warning
is needed (this is useful for some weird Microsoft-mode handling of
the __unaligned and __restrict qualifiers).
*/
{
  a_boolean     same;
  a_boolean     previous_qualifiers_include_const = TRUE;
  a_boolean     qualifiers_added = FALSE;
  an_error_code warning_code = ec_no_error;

  if (warning_suggested != NULL) *warning_suggested = ec_no_error;
  for (same = TRUE; same;) {
    a_type_qualifier_set dest_type_qualifiers = get_type_qualifiers(dest_type);
    a_type_qualifier_set source_type_qualifiers =
                                              get_type_qualifiers(source_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    a_boolean            ms_qualifier_adj_made = FALSE;
    a_type_qualifier_set orig_dest_type_qualifiers = dest_type_qualifiers;
    a_type_qualifier_set orig_source_type_qualifiers = source_type_qualifiers;
    if (microsoft_mode) {
      /* MSVC++ allows some weird dropping of certain qualifiers. */
      ms_qualifier_adj_made = handle_microsoft_dropping_of_qualifiers(
                                              &source_type_qualifiers,
                                              &dest_type_qualifiers,
                                              dest_type,
                                              &warning_code);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_template_param_type(dest_type) ||
        is_template_param_type(source_type) ||
        (is_array_type(dest_type) &&
         is_template_param_type(array_element_type(dest_type))) ||
        (is_array_type(source_type) &&
         is_template_param_type(array_element_type(source_type)))) {
      /* With template parameter types, we can't tell.  const int converted
         to T might or might not be dropping cv-qualifiers, depending on
         the type of T. */
      break;
    } else if (any_qualifier_in_set_missing(dest_type_qualifiers,
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_qualifier_adj_made &&
            !any_qualifier_in_set_missing(orig_source_type_qualifiers,
                                          orig_dest_type_qualifiers)) {
          /* Some qualifiers were adjusted for the Microsoft case discussed
             above, but no qualifiers were being added originally.  Don't
             require that previous steps all have const. */
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          same = previous_qualifiers_include_const;
          if (!same) break;
        }  /* if */
      }  /* if */
      /* See if this qualifier includes const. */
      if ((dest_type_qualifiers & TQ_CONST) == 0) {
	previous_qualifiers_include_const = FALSE;
      }  /* if */
      dest_type = skip_typerefs(dest_type);
      source_type = skip_typerefs(source_type);
      if (types_are_both_pointers_or_both_handles(dest_type, source_type)) {
        if (dest_type->size != source_type->size
#ifdef pointer_types_have_same_repr
            || !pointer_types_have_same_repr(source_type, dest_type)
#endif /* ifdef pointer_types_have_same_repr */
                                                                    ) {
          /* If the pointer types have different representations we can't
             consider the overall conversion valid. */
          same = FALSE;
          break;
        }  /* if */
        /* Continue at the next level for pointers and handles. */
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
  if (ignore_underlying_type) {
    /* Return the underlying types to the caller. */
    if (underlying_source_type != NULL) {
      *underlying_source_type = source_type;
    }  /* if */
    if (underlying_dest_type != NULL) {
      *underlying_dest_type = dest_type;
    }  /* if */
  }  /* if */
  /* Return any warning code, e.g., for the Microsoft trick with __unaligned
     and __restrict described above. */
  if (warning_suggested != NULL && same) *warning_suggested = warning_code;
  return same;
}  /* qualification_conversion_possible_full */


a_boolean qualification_conversion_possible(
                                          a_type_ptr    source_type,
                                          a_type_ptr    dest_type,
                                          a_boolean     *p_qualifiers_added,
                                          an_error_code *warning_suggested,
                                          a_boolean     ignore_underlying_type)
/*
Interface to qualification_conversion_possible_full that supplies default
values for the underlying source and destination return values.
*/
{
  return qualification_conversion_possible_full(
                 source_type, dest_type, p_qualifiers_added,
                 ignore_underlying_type, warning_suggested,
                 (a_type_ptr*)NULL, (a_type_ptr*)NULL);
}  /* qualification_conversion_possible */


a_boolean cast_removes_qualifiers(a_type_ptr    source_type,
                                  a_type_ptr    dest_type,
                                  an_error_code *warning_suggested)
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

If warning_suggested is non-NULL, if the conversion is okay (i.e., it does not
cast away const, and this routine returns FALSE) but is suspect, return
*warning_suggested set to a warning to be issued; otherwise return
*warning suggested set to ec_no_error.
*/
{
  a_boolean	qualifiers_added;
  a_boolean	result = FALSE;
  a_boolean	check_further = TRUE;

  if (warning_suggested != NULL) *warning_suggested = ec_no_error;
  if (types_are_both_pointers_or_both_handles(dest_type, source_type)) {
    dest_type = type_pointed_to(dest_type);
    source_type = type_pointed_to(source_type);
  } else if (is_ptr_to_member_type(dest_type) &&
             is_ptr_to_member_type(source_type)) {
    dest_type = pm_member_type(dest_type);
    source_type = pm_member_type(source_type);
  } else if (types_are_references_of_the_same_kind(dest_type, source_type) &&
             is_rvalue_reference_type(dest_type) ==
                                       is_rvalue_reference_type(source_type)) {
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
                                           warning_suggested,
                                           /*ignore_underlying_type=*/TRUE)) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* cast_removes_qualifiers */

#if UPC_EXTENSIONS_ALLOWED

static a_boolean f_get_underlying_upc_block_size(a_type_ptr  type)
/*
Return the UPC block size of the given type if applicable, or 
UPC_BLOCK_SIZE_NONE otherwise.  For arguments without side effect,
the corresponding macro get_underlying_upc_block_size is usually
preferable.
*/
{
  return get_underlying_upc_block_size(type);
}  /* f_get_underlying_upc_block_size */

#define is_generic_shared_pointer_type(tp)                              \
  (is_shared_void_star_type(tp) &&                                      \
   f_get_underlying_upc_block_size(type_pointed_to(tp)) == 1)


static a_boolean check_implicit_upc_pointer_conversion(a_type_ptr  src,
                                                       a_type_ptr  dst)
/*
An implicit conversion is attempted from pointer type src to pointer type dst.
Return FALSE if the conversion is between a pointer to UPC "shared" and a
pointer to non-shared, or if the conversion is between two pointer to UPC
"shared" types with unequal associated block sizes.
*/
{
  a_boolean  result = TRUE;

  if (is_ptr_to_shared_type(dst)) {
    if (is_ptr_to_shared_type(src)) {
      a_type_ptr  src_pointed_to = type_pointed_to(src);
      a_type_ptr  dst_pointed_to = type_pointed_to(dst);
      if (get_upc_block_size(src_pointed_to) !=
                                         get_upc_block_size(dst_pointed_to) &&
          !is_generic_shared_pointer_type(dst) &&
          !is_generic_shared_pointer_type(src)) {
        /* Unequal block sizes and neither of the pointers is a generic
           shared pointer: No implicit conversion. */
        result = FALSE;
      }  /* if */
    } else {
      /* No implicit conversion of ptr-to-shared to ptr-to-non-shared. */
      result = FALSE;
    }  /* if */
  } else if (is_ptr_to_shared_type(dst)) {
    /* No implicit conversion of ptr-to-non-shared to ptr-to-shared. */
    result = FALSE;
  }  /* if */
  return result;
}  /* check_implicit_upc_pointer_conversion */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_prohibited_interior_ptr_conversion(a_type_ptr source_type,
                                                a_type_ptr dest_type)
/*
Return TRUE if conversion from source_type to dest_type is a prohibited
conversion in C++/CLI because it drops gc-ness of an interior_ptr.
*/
{
  a_boolean prohibited = FALSE;

  if (cli_or_cx_enabled &&
      is_interior_ptr_type(source_type) &&
      is_pointer_type(dest_type) &&
      !is_interior_ptr_type(dest_type) &&
      !is_pin_ptr_type(dest_type)) {
    prohibited = TRUE;
  }  /* if */
  return prohibited;
}  /* is_prohibited_interior_ptr_conversion */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- source_is_function is not used in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
a_boolean impl_pointer_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_string_literal,
                         a_boolean            source_is_function,
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
be TRUE even when source_is_constant is FALSE, for string literals
represented in expression form.  If source_is_function is TRUE, the
source is the address of a specific function, which matters for a
particular C++/CLI conversion.  If allow_qualifier_or_eh_mismatch is
TRUE, ignore cv-qualifier and exception specification mismatches (the
two types are probably the types of the operands of an operation);
mismatches in named address space qualifiers are not ignored, however.
suppress_extensions is TRUE if conversions that are extensions should
not be allowed (what constitutes an extension depends on C_dialect, of
course).  If the conversion is possible, *std_conv is filled out to
describe the conversion.  In particular, if the conversion is suspect
and should be flagged with a warning, the warning_suggested field is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into warning_suggested when no
specific message applies.

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
  an_error_code    warning_suggested;

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
    } else if (is_nullptr(source_type)) {
      /* Do not set the flag for the C++11 nullptr keyword, so that nullptr
         can be used as a template nontype argument. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (dest_type->variant.pointer.is_interior_ptr ||
               dest_type->variant.pointer.is_pin_ptr) {
      /* C++/CLI allows conversion of nullptr to interior_ptr or pin_ptr, but
         not conversion of 0. */
      okay = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Normal case. */
      std_conv->pointer_normalization_needed = TRUE;
    }  /* if */
  } else if (is_nullptr(source_type)) {
    /* Values of nullptr types can be converted to any pointer type.
       (Note: the nullptr/__nullptr keyword is handled in the preceding
       case; this case is for other expressions with a nullptr type, which
       are also "null pointer constants," although they need not be
       constant expressions.) */
    okay = TRUE;
  } else if (is_pointer(source_type)) {
    /* Pointer --> pointer. */
    qualifiers_checked = FALSE;
    /* Get the type pointed to and drop type qualifiers and typedefs. */
    source_type_pointed_to = type_pointed_to(source_type);
    unqual_source_type_pointed_to = skip_typerefs(source_type_pointed_to);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (is_prohibited_interior_ptr_conversion(source_type, dest_type)) {
      /* Conversion from an interior_ptr to a non-interior_ptr is not
         allowed, because it loses the gc-ness of the pointer. */
      okay = FALSE;
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (types_are_compatible_for_impl_conversion(
                                            unqual_source_type_pointed_to,
                                            unqual_dest_type_pointed_to)) {
      /* The "_for_impl_conversion" version is used to get proper handling of
         pointers to arrays with qualified element types and (in C++) to deal
         appropriately with routine linkages on function types. */
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
        /* Note that C++/CLI interior_ptr<void> and pin_ptr<void> work like
           void * in this regard. */
        if (is_object_type(unqual_source_type_pointed_to) ||
            (C_mode() && is_incomplete(unqual_source_type_pointed_to))) {
          /* In C, a pointer to an object or incomplete type
             may be converted to "void *".  In C++, a pointer to an
             object type may be converted to "void *".  In both cases,
             cv-qualifier differences are checked below.  Note that in
             C++ an object type may be incomplete in some cases. */
          okay = TRUE;
          std_conv->pointer_normalization_needed = TRUE;
        } else if (is_function(unqual_source_type_pointed_to)) {
          /* Converting a pointer to function to a pointer to void. */
          if (C_dialect == C_dialect_cplusplus) {
            /* In ARM C++, a pointer to a function may be converted to
               "void *" if the pointer will fit in a "void *".
               ARM 4.6 (pointer conversions).  This is no longer
               allowed in standard C++, but we allow it as an extension. */
            if (ms_extensions) {
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
            (is_object_type(unqual_dest_type_pointed_to) ||
             is_incomplete(unqual_dest_type_pointed_to))) {
          /* In C but not C++, a "void *" may be converted to a pointer to an
             object or incomplete type.  ANSI C 3.3.16.1 (assignment). */
          okay = TRUE;
        } else if (conversion_from_void_star_in_C && !suppress_extensions) {
          /* As an extension in C, we also allow a "void *" to be converted to
             a function pointer; a warning is issued. */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        } else if (!suppress_extensions &&
                   source_is_string_literal &&
                   is_character_type(unqual_source_type_pointed_to) &&
                   is_character_type(unqual_dest_type_pointed_to)) {
          /* Allow a character string to be converted to a pointer to any kind
             of char.  This is an extension in both C and C++. */
          /* C++/CLI interior_ptr<char> and pin_ptr<char> are treated as
             char * in this regard. */
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
                                      &warning_suggested,
                                      /*ignore_underlying_type=*/FALSE)) {
          /* Allow conversion between pointers where type qualifiers are
             being added at levels other than the first, e.g.,
             "int **" -> "const int * const *".  These are the const-safe
             cases.  This is an extension in C mode. */
          okay = TRUE;
          std_conv->nontrivial_conversion = FALSE;
          std_conv->type_qualifiers_added = qualifiers_added;
          std_conv->warning_suggested = warning_suggested;
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (cli_or_cx_enabled && source_is_function &&
                   is_function(unqual_dest_type_pointed_to) &&
                   is_function(unqual_source_type_pointed_to) &&
                   unqual_dest_type_pointed_to->variant.routine.extra_info
                    ->calling_convention == (a_calling_convention)cc_clrcall &&
                   f_types_are_compatible(unqual_dest_type_pointed_to,
                                          unqual_source_type_pointed_to,
                                          TCF_IGNORE_CALLING_CONVENTIONS)) {
          /* In C++/CLI, the address of a specific function (not an arbitrary
             function pointer) can be converted to a pointer to a __clrcall
             function because every function (even __cdecl or extern "C")
             has a secondary __clrcall entry point. */
          okay = TRUE;
          std_conv->nontrivial_conversion = FALSE;
          std_conv->type_qualifiers_added = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
                    ms_extensions)) {
          /* In pcc mode, SVR4 C, gcc, and Microsoft C modes, allow conversion
             between incompatible pointer types, with a warning. */
          okay = TRUE;
          std_conv->warning_suggested = default_warning_code;
        }  /* if */
      }  /* if */
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    if (upc_mode && okay && !qualifiers_checked) {
      okay = check_implicit_upc_pointer_conversion(source_type, dest_type);
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (named_address_spaces_enabled && okay && !qualifiers_checked) {
      /* Check that any named address space qualifiers are compatible.  Note
         that this is done even when allow_qualifier_or_eh_mismatch is TRUE. */
      a_type_qualifier_set dest_type_qualifiers =
                                     get_type_qualifiers(dest_type_pointed_to);
      a_type_qualifier_set source_type_qualifiers =
                                   get_type_qualifiers(source_type_pointed_to);
      if (!first_address_space_encloses_second(dest_type_qualifiers,
                                               source_type_qualifiers)) {
        /* The destination space is either more restrictive or altogether
           different: No conversion is possible. */
        okay = FALSE;
      } else if (named_address_space_from_qualifier_set(source_type_qualifiers)
             != named_address_space_from_qualifier_set(dest_type_qualifiers)) {
        /* A conversion is possible but the destination address space is
           less strict.  This is similar to adding qualifiers. */
        std_conv->type_qualifiers_added = TRUE;
      }  /* if */
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
      an_error_code ms_qualifier_warning = ec_no_error;
      if (microsoft_mode) {
        /* MSVC++ allows some weird dropping of certain qualifiers. */
        (void)handle_microsoft_dropping_of_qualifiers(
                                                &source_type_qualifiers,
                                                &dest_type_qualifiers,
                                                unqual_dest_type_pointed_to,
                                                &ms_qualifier_warning);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
        if (source_is_string_literal &&
            string_literals_are_const &&
            deprecated_string_literal_conv_allowed &&
            source_type_qualifiers == (dest_type_qualifiers | TQ_CONST) &&
            same_entities(unqual_dest_type_pointed_to,
                          unqual_source_type_pointed_to)) {
          /* A deprecated conversion in standard C++ allows conversion of
             a string literal or wide string literal to a pointer to
             non-const ([conv.array] paragraph 2). */
          std_conv->conv_of_string_literal_to_ptr_to_nonconst = TRUE;
          if (cpp11_mode || (gpp_mode && gnu_version >= 40200)) {
            std_conv->warning_suggested =
                               is_character_type(unqual_dest_type_pointed_to) ?
                                 ec_deprecated_string_conv :
                                 ec_deprecated_string_conv_gen;
          }  /* if */
        } else if ((microsoft_mode && !C_mode()) &&
                   source_is_string_literal &&
                   string_literals_are_const &&
                   is_void(unqual_dest_type_pointed_to)) {
          /* MSVC++ 7.1 allows conversion of a string literal (which is
             const) to void *.  8.0 also allows this.  Before 7.1, string
             literals were not const. */
          /* Note that it's deliberate that we do not test that only the
             "const" qualifier is dropped here.  That matches MSVC. */
          std_conv->pointer_normalization_needed = TRUE;
          std_conv->conv_of_string_literal_to_ptr_to_nonconst = TRUE;
        } else if (cfront_2_1_mode && 
                   is_void(unqual_dest_type_pointed_to) &&
                   is_void(unqual_source_type_pointed_to)) {
          /* cfront 2.1 allows conversion of a pointer to qualified void
             (e.g., "const void *") to "void *". */
        } else if (sun_mode &&
                   is_pointer(unqual_dest_type_pointed_to) &&
                   is_pointer(unqual_source_type_pointed_to)) {
          /* Sun C++ allows
               int * const *temp = 0;
               int **temp2 = temp; // Should be error
             This is still allowed in the Studio 11 version. */
          std_conv->warning_suggested = default_warning_code;
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
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ms_qualifier_warning != ec_no_error &&
          std_conv->warning_suggested == ec_no_error) {
        /* Trigger a diagnostic about the __unaligned or __restrict
           qualifier being implicitly dropped. */
        std_conv->warning_suggested = ms_qualifier_warning;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (std_conv->warning_suggested == ec_no_error &&
        source_type->size < dest_type->size &&
        (source_type->variant.pointer.modifiers & (PM_SPTR | PM_UPTR)) == 0 &&
        (dest_type->variant.pointer.modifiers & PM_UPTR) == 0) {
      /* A widening conversion from a pointer type with no explicit signedness
         (i.e., no __sptr/__uptr modifier) to an explicitly or implicitly
         signed pointer type.  Issue a remark (the default severity of this
         diagnostic is established in set_default_message_severities). */
      std_conv->warning_suggested = ec_microsoft_ptr_sign_extension;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean literal_type_convertible_to_cli_string(a_type_ptr type_ptr)
/*
Return TRUE if type_ptr (the type of a string literal operand) can be
implicitly converted to a handle to a C++/CLI System::String.
See 14.2.5 in the ECMA-372 standard.  This function also
returns TRUE for T* where T is a char or wchar_t, in addition to the T[]
cases, because array->pointer decay may have already been done.
*/
{
  a_boolean  result = FALSE;
  a_type_ptr underlying_type = NULL;

  type_ptr = skip_typerefs(type_ptr);
  if (is_array(type_ptr)) {
    underlying_type = array_element_type(type_ptr);
  } else if (is_pointer(type_ptr)) {
    underlying_type = type_pointed_to(type_ptr);
  }  /* if */
  if (underlying_type != NULL) {
    result = is_narrow_or_wide_character_type(underlying_type);
  }  /* if */
  return result;
}  /* literal_type_convertible_to_cli_string */


a_boolean cli_string_literal_conversion_possible(
                                              a_type_ptr           source_type,
                                              a_type_ptr           dest_type,
                                              a_std_conv_descr_ptr std_conv)
/*
Return TRUE if source_type (the type of a string literal operand) can be
implicitly converted to dest_type (if that is a handle to a C++/CLI
System::String, or a handle to some other type such that the literal
can be converted to a handle to System::String and then to dest_type).
See 14.2.5 in the ECMA-372 standard.  If the conversion is possible,
*std_conv is filled out to describe the conversion.  std_conv can be NULL
if that information is not needed.
*/
{
  a_boolean okay = FALSE;

  if (cli_or_cx_enabled) {
    if (std_conv != NULL) clear_std_conv_descr(std_conv);
    if (literal_type_convertible_to_cli_string(source_type)) {
      /* The source type is an appropriate string literal type.  See if
         the destination type is a handle. */
      if (is_handle_type(dest_type)) {
        /* The source type can be converted to a System::String^.  Next, test
           to see if a conversion from System::String^ to dest_type
           is possible. */
        if (impl_handle_conversion(make_handle_to_system_string(),
                                   dest_type,
                                   /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                   std_conv)) {
          /* The conversion from System::String^ --> dest_type is possible. */
          okay = TRUE;
          if (std_conv != NULL) {
            /* Mark this conversion as one that requires the
               source_type --> System::String^ conversion (possibly
               among others). */
            std_conv->nontrivial_conversion = TRUE;
            std_conv->conv_of_string_literal_to_cli_string = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return okay;
}  /* cli_string_literal_conversion_possible */


a_boolean cli_array_covariance_conversion_possible(
                                              a_type_ptr           source_type,
                                              a_type_ptr           dest_type,
                                              a_std_conv_descr_ptr std_conv)
/*
Return TRUE if source_type and dest_type are C++/CLI array types whose
element types are handle types and it's okay to implicitly convert a
handle to source_type to a handle to dest_type as an array covariance
conversion (see ECMA-372 14.2.1).  If the conversion is possible,
*std_conv is filled out to describe it.  std_conv can be NULL if that
information is not needed.
*/
{
  a_boolean okay = FALSE, source_unknown, dest_unknown;

  if (is_cli_array_type(source_type) &&
      is_cli_array_type(dest_type) &&
      cli_array_rank(source_type, &source_unknown) ==
                                    cli_array_rank(dest_type, &dest_unknown) &&
      source_unknown == dest_unknown) {
    /* Source and destination are arrays with the same rank; see if there
       is a handle conversion for the underlying elements. */
    a_std_conv_descr element_std_conv, *e_std_conv = NULL;
    a_type_ptr       source_element_type, dest_element_type;
    if (std_conv != NULL) e_std_conv = &element_std_conv;
    source_element_type = cli_array_element_type(source_type);
    dest_element_type = cli_array_element_type(dest_type);
    if (is_handle_type(source_element_type) &&
        is_handle_type(dest_element_type) &&
        impl_handle_conversion(source_element_type, dest_element_type,
                               /*allow_qualifier_or_eh_mismatch=*/FALSE,
                               e_std_conv)) {
      /* Array covariance conversion is applicable. */
      okay = TRUE;
      if (std_conv != NULL) {
        std_conv->cli_array_covariance_conversion = TRUE;
        std_conv->cast_base_class = element_std_conv.cast_base_class;
      }  /* if */
    }  /* if */
  }  /* if */
  return okay;
}  /* cli_array_covariance_conversion_possible */


static a_boolean cli_array_to_ienumerable_conversion_possible(
                                              a_type_ptr           source_type,
                                              a_type_ptr           dest_type)
/*
Return TRUE if source_type is a one-dimensional C++/CLI array type with
element type T and dest type is System::Collections::Generic::IEnumerable<T>.
That type is considered an effective base class of the array type.
*/
{
  a_boolean okay = FALSE, rank_unknown;

  if (is_cli_array_type(source_type) &&
      cli_array_rank(source_type, &rank_unknown) == 1) {
    a_type_ptr elem_type = cli_array_element_type(source_type);
    if (is_generic_cli_ienumerable_type(dest_type, elem_type)) {
      okay = TRUE;
    }  /* if */
  }  /* if */
  return okay;
}  /* cli_array_to_ienumerable_conversion_possible */


a_boolean impl_handle_conversion(
                         a_type_ptr           source_type,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type
source_type (any type) to something of type dest_type (a C++/CLI
handle type).  If allow_qualifier_or_eh_mismatch is TRUE, ignore
cv-qualifier mismatches (the two types are probably the types of the
operands of an operation).  If the conversion is possible, *std_conv
is filled out to describe the conversion.  std_conv can be NULL if
that information is not needed.  Doesn't cover boxing conversions
(value class --> handle to boxed value) or string literal conversions
(string-literal --> handle to System::String).
*/
{
  a_boolean        okay = FALSE;
  a_type_ptr       dest_type_pointed_to, source_type_pointed_to = NULL;
  a_type_ptr       unqual_dest_type_pointed_to, unqual_source_type_pointed_to;
  a_base_class_ptr bcp;
  a_boolean        qualifiers_checked = FALSE;

  db_enter(5, "impl_handle_conversion");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_handle_conversion: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  if (std_conv != NULL) {
    clear_std_conv_descr(std_conv);
    /* Assume a nontrivial conversion; the flag will be cleared later if in
       fact there is nothing nontrivial. */
    std_conv->nontrivial_conversion = TRUE;
  }  /* if */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
#if CHECKING
  if (!is_handle_ptr(dest_type)) {
    internal_error("impl_handle_conversion: dest_type is not handle");
  }  /* if */
#endif /* CHECKING */
  /* Get the type pointed to and drop type qualifiers and typedefs. */
  dest_type_pointed_to = type_pointed_to(dest_type);
  unqual_dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
  if (is_template_param_type(source_type)) {
    /* A template parameter type might be a handle type. */
    okay = TRUE;
    qualifiers_checked = TRUE;
  } else if (is_nullptr(source_type)) {
    /* Values of nullptr types can be converted to any handle type. */
    if (!is_handle_type_not_value_generic(dest_type)) {
      /* ... except a handle that is implicit on a generic type that might
         be a value type. */
    } else {
      okay = TRUE;
      qualifiers_checked = TRUE;
    }  /* if */
  } else if (is_handle_ptr(source_type)) {
    /* Handle --> handle. */
    /* Get the type pointed to and drop type qualifiers and typedefs. */
    source_type_pointed_to = type_pointed_to(source_type);
    unqual_source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    if (types_are_compatible(unqual_source_type_pointed_to,
                             unqual_dest_type_pointed_to)) {
      /* The types pointed to are compatible. */
      okay = TRUE;
      if (std_conv != NULL) std_conv->nontrivial_conversion = FALSE;
    } else if (is_class_or_struct(unqual_source_type_pointed_to) &&
               is_cli_system_object_type(unqual_dest_type_pointed_to)) {
      /* A handle to a managed class can be converted to System::Object^.
         For interfaces, this is possible even though the interface doesn't
         have Object as a base class.  For other cases, we test this
         first because we want pointer_normalization_needed set to indicate
         the handle equivalent of a pointer conversion to "void *".  The
         cast_base_class field is left NULL intentionally, even in cases
         where there is a relationship between the classes. */
      okay = TRUE;
      if (std_conv != NULL) std_conv->pointer_normalization_needed = TRUE;
    } else if (is_class_or_struct(unqual_source_type_pointed_to) &&
               is_class_or_struct(unqual_dest_type_pointed_to) &&
               (bcp = find_base_class_of(unqual_source_type_pointed_to,
                                         unqual_dest_type_pointed_to))
                                                                     != NULL) {
      /* Conversion from handle-to-derived to handle-to-base. */
      okay = TRUE;
      if (std_conv != NULL) std_conv->cast_base_class = bcp;
    } else if (is_template_dependent_context() &&
               (is_template_dependent_type(unqual_dest_type_pointed_to) ||
                is_template_dependent_type(unqual_source_type_pointed_to))) {
      /* Conversions between template-dependent types are always allowed. */
      okay = TRUE;
    } else if (cli_array_covariance_conversion_possible(
                                                 unqual_source_type_pointed_to,
                                                 unqual_dest_type_pointed_to,
                                                 std_conv)) {
      /* An array covariance conversion is possible. */
      okay = TRUE;
    } else if (cli_array_to_ienumerable_conversion_possible(
                                                unqual_source_type_pointed_to,
                                                unqual_dest_type_pointed_to)) {
      /* A conversion between a handle to a CLI array<T, 1> and a handle to
         IEnumerable<T> is allowed, simulating having that IEnumerable as
         a base class of the array type. */
      okay = TRUE;
    }  /* if */
  } else if (is_error(source_type)) {
    /* Error --> handle is always allowed. */
    okay = TRUE;
    qualifiers_checked = TRUE;
  }  /* if */
  if (okay && !qualifiers_checked && !allow_qualifier_or_eh_mismatch) {
    /* Check that no qualifiers are dropped in going from the source type to
       the destination type. */
    a_type_qualifier_set dest_type_qualifiers =
                                     get_type_qualifiers(dest_type_pointed_to);
    a_type_qualifier_set source_type_qualifiers =
                                   get_type_qualifiers(source_type_pointed_to);
    if (dest_type_qualifiers == source_type_qualifiers) {
      /* The qualifiers are the same. */
    } else if (any_qualifier_in_set_missing(dest_type_qualifiers,
                                            source_type_qualifiers)) {
      /* Qualifiers are being dropped. */
      okay = FALSE;
    } else {
      /* Qualifiers are being added. */
      if (std_conv != NULL) std_conv->type_qualifiers_added = TRUE;
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "impl_handle_conversion: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* impl_handle_conversion */


a_boolean boxing_conversion_possible(a_type_ptr           source_type,
                                     a_type_ptr           dest_type,
                                     a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type
source_type to something of type dest_type as a C++/CLI boxing conversion.
The cases accepted convert a value type to a handle to that value type.
If the conversion is possible, *std_conv is filled out to describe the
conversion.  std_conv can be NULL if that information is not needed.
*/
{
  a_boolean okay = FALSE;

  db_enter(5, "boxing_conversion_possible");
  if (cli_or_cx_enabled) {
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "boxing_conversion_possible: source_type = ");
      db_abbreviated_type(source_type);
      fprintf(f_debug, ", dest_type = ");
      db_abbreviated_type(dest_type);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (std_conv != NULL) clear_std_conv_descr(std_conv);
    /* The source type has to be a value type, but not a pointer type.
       The destination type has to be a handle type. */
    if (is_handle_type(dest_type) &&
        is_boxable_type(source_type)) {
      a_type_ptr qual_dest_type;
      /* cv-qualifiers on the source type are dropped, since the value gets
         copied into the box. */
      source_type = skip_typerefs(source_type);
      /* Convert a built-in type to the corresponding CLI type, e.g.,
         int to System::Int32.  Also box an enum. */
      source_type = boxed_type_for(source_type);
      qual_dest_type = type_pointed_to(dest_type);
      /* cv-qualifiers are ignored on the destination type, since it's okay
         to add cv-qualifiers. */
      dest_type = skip_typerefs(qual_dest_type);
      if (types_are_compatible(source_type, dest_type)) {
        /* A boxing conversion is possible. */
        okay = TRUE;
        if (std_conv != NULL &&
            is_qualified_type(qual_dest_type)) {
          std_conv->type_qualifiers_added = TRUE;
        }  /* if */
      } else if ((is_cppcx_box_type(source_type) ||
                  is_value_class_type(source_type)) &&
                 is_class_struct_union_type(dest_type) &&
                 impl_handle_conversion(make_handle_type(source_type),
                                        make_handle_type(qual_dest_type),
                                       /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                        std_conv)) {
        /* Boxing to a handle to a type followed by a handle conversion to
           a base class of that type.  If std_conv is non-NULL some of its
           fields will have been set (e.g., cast_base_class). */
        okay = TRUE;
      }  /* if */
      if (okay && std_conv != NULL) {
        std_conv->nontrivial_conversion = TRUE;
        std_conv->boxing_conversion = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* boxing_conversion_possible */


a_boolean unboxing_conversion_possible(a_type_ptr           source_type,
                                       a_type_ptr           dest_type,
                                       a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type
source_type to something of type dest_type as a C++/CLI unboxing conversion.
The cases accepted convert a handle to a value type.  If the conversion is
possible, *std_conv is filled out to describe the conversion.  std_conv
can be NULL if that information is not needed.
*/
{
  a_boolean okay = FALSE;

  db_enter(5, "unboxing_conversion_possible");
  if (cli_or_cx_enabled) {
#if DEBUG
    if (debug_level >= 5) {
      fprintf(f_debug, "unboxing_conversion_possible: source_type = ");
      db_abbreviated_type(source_type);
      fprintf(f_debug, ", dest_type = ");
      db_abbreviated_type(dest_type);
      fprintf(f_debug, "\n");
    }  /* if */
#endif /* DEBUG */
    if (std_conv != NULL) clear_std_conv_descr(std_conv);
    /* The source type has to be a handle type.  The destination type has
       to be a value type, but not a pointer type. */
    if (is_handle_type(source_type) &&
        is_boxable_type(dest_type)) {
      a_base_class_ptr bcp = NULL;
      source_type = type_pointed_to(source_type);
      /* cv-qualifiers on the source type are dropped. */
      source_type = skip_typerefs(source_type);
      /* cv-qualifiers are ignored on the destination type, since it's okay
         to add cv-qualifiers. */
      dest_type = skip_typerefs(dest_type);
      /* Convert a built-in type to the corresponding CLI type, e.g.,
         int to System::Int32.  Also box an enum. */
      dest_type = boxed_type_for(dest_type);
      if (types_are_compatible(source_type, dest_type)) {
        /* An unboxing conversion is possible:  cv1 V^ --> cv2 V. */
        okay = TRUE;
      } else if ((is_cppcx_box_type(dest_type) ||
                  is_value_class_type(dest_type)) &&
                 is_class_struct_union_type(source_type) &&
                 (bcp = find_base_class_of(dest_type, source_type)) != NULL) {
        /* System::ValueType^ --> value class type and
           System::Object^    --> value class type are also allowed,
           and conversions from interfaces that the value class type
           implements. */
        okay = TRUE;
      }  /* if */
      if (okay && std_conv != NULL) {
        std_conv->nontrivial_conversion = TRUE;
        std_conv->cast_base_class = bcp;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return okay;
}  /* unboxing_conversion_possible */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean this_param_types_correspond(a_type_ptr rout_type_1,
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
If neither is TRUE, the types are checked for an exact match.  These two
flags (check_as_conversion and check_as_operands) only have an effect in
Cfront mode.
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
    if (rtsp_1->ref_qualifiers != rtsp_2->ref_qualifiers) {
      /* C++11 ref-qualifiers do not match; so the types don't correspond. */
      /* correspond = FALSE;  -- already set. */
    } else if (!any_cfront_mode()) {
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


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* <-- source_is_function is not used in that case. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_boolean function_types_correspond(
                                   a_type_ptr dest_type,
                                   a_type_ptr source_type,
                                   a_boolean  source_is_function,
                                   a_boolean  allow_qualifier_or_eh_mismatch)
/*
Return TRUE if the two function types given are compatible if one ignores any
difference in the underlying class of their "this" parameter types.
If source_is_function is TRUE, the source is a pointer-to-member for a
specific function, which matters for a particular C++/CLI conversion.
If allow_qualifier_or_eh_mismatch is TRUE, ignore cv-qualifier and
exception specification mismatches (the two types are probably the
types of the operands of an operation).
*/
{
  a_boolean                correspond = FALSE;
  a_type_compat_flags_set  rt_flags;

  dest_type = skip_typerefs(dest_type);
  source_type = skip_typerefs(source_type);
  check_assertion(dest_type->kind == (a_type_kind)tk_routine &&
                  source_type->kind == (a_type_kind)tk_routine);
  rt_flags = TCF_IGNORE_THIS_CLASS_TYPE |
             TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING;
  if (this_param_types_correspond(dest_type, source_type,
                                  !allow_qualifier_or_eh_mismatch,
                                  allow_qualifier_or_eh_mismatch)) {
    if (f_types_are_compatible(dest_type, source_type, rt_flags)) {
      correspond = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled && source_is_function &&
               !allow_qualifier_or_eh_mismatch &&
               dest_type->variant.routine.extra_info
                    ->calling_convention == (a_calling_convention)cc_clrcall &&
               f_types_are_compatible(dest_type, source_type,
                                      (rt_flags |
                                       TCF_IGNORE_CALLING_CONVENTIONS))) {
      /* In C++/CLI, a pointer-to-member for a specific function can
         be converted to a pointer-to-member to a __clrcall function
         because every function (even __cdecl or extern "C") has a
         secondary __clrcall entry point. */
      correspond = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  return correspond;
}  /* function_types_correspond */


a_boolean member_types_correspond(a_type_ptr dest_type,
                                  a_type_ptr source_type,
                                  a_boolean  source_is_function,
                                  a_boolean  allow_qualifier_or_eh_mismatch,
                                  a_boolean  *qualifiers_added)
/*
Return TRUE if the member types from two pointer-to-member types match
allowing for a possible difference due to the associated class type.
Specifically, this means that when comparing function types, the
difference in the underlying class of the "this" parameter type must
be ignored.  If source_is_function is TRUE, the source is a
pointer-to-member for a specific function, which matters for a
particular C++/CLI conversion.  If allow_qualifier_or_eh_mismatch is
TRUE, ignore cv-qualifier and exception specification mismatches (the
two types are probably the types of the operands of an operation).
*/
{
  a_boolean correspond;

  *qualifiers_added = FALSE;
  if (!is_function_type(dest_type) || !is_function_type(source_type)) {
    /* This is not the special function case, so the normal check will work. */
    correspond = qualification_conversion_possible
                                  (source_type, dest_type, qualifiers_added,
                                   (an_error_code *)NULL,
                                   /*ignore_underlying_type=*/FALSE);
  } else {
    /* We have two function types from member pointers.  See if they
       match when we allow for the difference in the underlying type
       of the "this" parameter.  Note that this test must be done even
       when the class types are the same, because the routines may
       be from base classes. */
    correspond = function_types_correspond(dest_type, source_type,
                                           source_is_function,
                                           allow_qualifier_or_eh_mismatch);
  }  /* if */
  return correspond;
}  /* member_types_correspond */


a_boolean impl_ptr_to_member_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_function,
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
source_is_function is TRUE, the source is a pointer-to-member for a
specific function, which matters for a particular C++/CLI conversion.
If allow_qualifier_or_eh_mismatch is TRUE, ignore cv-qualifier and
exception specification mismatches (the two types are probably the
types of the operands of an operation).  If the conversion is
possible, *std_conv is filled out to describe the conversion.

Note that any type qualifiers on the types themselves (rather than the
types pointed to) are ignored.

See ARM 5.17 (assignment operators) and 4.8 (standard conversions for
pointers to members).
*/
{
  a_boolean     okay = FALSE;
  an_error_code warning_suggested;
 
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
                                  source_is_function,
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
                                 &warning_suggested,
                                 /*ignore_underlying_type=*/FALSE)) {
            /* This is an allowed qualification conversion. */
            std_conv->type_qualifiers_added = qualifiers_added;
            std_conv->warning_suggested = warning_suggested;
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
    if (!is_nullptr(source_type)) {
      /* The flag is only set for integral null pointer constants, so that
         the C++11 nullptr keyword can be used with a pointer-to-member
         non-type template parameter. */
      std_conv->pointer_normalization_needed = TRUE;
    }  /* if */
  } else if (is_nullptr(source_type)) {
    /* An expression with a nullptr type can be converted to all
       pointer-to-member types.  (The nullptr/__nullptr keyword is handled
       by the preceding case; this case is for other expressions with a
       nullptr type.) */
    okay = TRUE;
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

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean ilp64_will_narrow(a_type_ptr  source_type,
                                   a_type_ptr  dest_type)
/*
Return whether an ILP64-porting warning should be issued on a conversion from
source_type to dest_type (ILP64 is a class of ABIs where int, long, and
pointer types are 64 bits wide).  A warning should only be issued when
dest_type is currently 4 bytes wide and source_type is marked with the
Microsoft keyword __w64.
*/
{
  a_boolean  result = FALSE;

  if (source_type->has_microsoft_w64_specifier &&
      !dest_type->has_microsoft_w64_specifier &&
      skip_typerefs(dest_type)->size == 4 && is_integral_type(dest_type)) {
    result = TRUE;
  }  /* if */
  return result;
}  /* ilp64_will_narrow */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean is_conv_from_64_bit_integral_to_smaller(
                                                        a_type_ptr source_type,
                                                        a_type_ptr dest_type)
/*
Return TRUE if source_type is a 64-bit integral type and dest_type is
an integral type smaller than that.
*/
{
  a_boolean result;

  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  result = (is_integral(source_type) &&
            is_integral(dest_type) &&
            source_type->size*targ_char_bit == 64 &&
            dest_type->size*targ_char_bit < 64);
  return result;
}  /* is_conv_from_64_bit_integral_to_smaller */
      

static a_targ_size_t num_significant_bits(a_type_ptr type)
/*
Return the number of significant bits in the specified arithmetic type,
i.e., the size an integral type, the number of bits in the mantissa of a
floating point type, or the number of non-fractional bits in a fixed point
type.
*/
{
  a_targ_size_t num_bits = 0;

  if (is_floating_type(type)) {
    /* Includes complex types, if enabled; in that case, the result will be
       the number of mantissa bits in the real part. */
    switch (type->variant.float_kind) {
      case fk_float:
        num_bits = targ_flt_mant_dig;
        break;
      case fk_double:
        num_bits = targ_dbl_mant_dig;
        break;
      case fk_long_double:
        num_bits = targ_ldbl_mant_dig;
        break;
      default:
        unexpected_condition();
    }  /* switch */
#if FIXED_POINT_ALLOWED
  } else if (is_fixed_point_type(type)) {
    num_bits = non_fractional_bits_for_fixed_point(&type->variant.fixed_point);
#endif /* FIXED_POINT_ALLOWED */
  } else {
    check_assertion(is_integral_or_enum(type));
    num_bits = targ_char_bit * type->size;
  }  /* if */
  return num_bits;
}  /* num_significant_bits */


a_boolean impl_conversion_possible(
                          a_type_ptr           source_type,
                          a_boolean            source_is_constant,
                          a_boolean            source_is_string_literal,
                          a_boolean            source_is_function,
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
source_is_constant is FALSE, for an extension.  If source_is_function
is TRUE, the source is the address of a specific function, which
matters for a particular C++/CLI conversion.  suppress_extensions is
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
       Conversion is allowed from arithmetic, unscoped enumeration, pointer,
       and pointer to member.   C++/CLI does not allow conversion from a handle
       to bool (though it is allowed, effectively, in a boolean controlling
       expression). */
    if (is_bool(source_type)) {
      /* bool --> bool is no conversion. */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (is_arithmetic_or_unscoped_enum(source_type)) {
      okay = TRUE;
    } else if (is_pointer(source_type) || is_ptr_to_member(source_type) ||
               is_nullptr(source_type)) {
      okay = TRUE;
      /* This conversion is worse than others in overload resolution.
         Remember that. */
      std_conv->ptr_or_pm_to_bool = TRUE;
    }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (gnu_mode && is_vector_type(source_type)) {
    /* Different versions of GCC behave differently wrt. conversions between
       vector types.  Some versions by default allow conversions between
       vectors of the same size and "kind" (integer vs. float), regardless of
       the specific element type.  We emulate that behavior when
       permissive_gnu_vector_conversions_enabled is TRUE. */
    if (identical_types(source_type, dest_type)) {
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (permissive_gnu_vector_conversions_enabled) {
      if (is_vector_type(dest_type) &&
          skip_typerefs(source_type->variant.vector.element_type)->kind ==
                skip_typerefs(dest_type->variant.vector.element_type)->kind &&
          source_type->size == dest_type->size) {
        okay = TRUE;
        std_conv->warning_suggested = ec_incompatible_vectors_conversion;
      }  /* if */
    }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
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
      if (cfront_2_1_mode && is_integral_or_enum(source_type) &&
          !integer_type_is_scoped_enum(dest_type)) {
        /* cfront 2.1 allows conversion of integral or other enum types to
           an enum, with a warning.  (It also allows floating point types
           to be converted to an enum, but it doesn't seem necessary to
           duplicate that behavior.) */
        okay = TRUE;
        std_conv->warning_suggested = ec_mixed_enum_type;
      }  /* if */
    } else if (is_arithmetic_or_unscoped_enum(source_type)) {
      /* Arithmetic or unscoped enum --> arithmetic (including enum in C). */
      okay = TRUE;
      if (warning_on_lossy_conversion && !source_is_constant &&
          !identical_types(source_type, dest_type)) {
        if (num_significant_bits(dest_type) <
                                           num_significant_bits(source_type) ||
            (is_floating_type(source_type) && !is_floating_type(dest_type))) {
          /* Warn about possible loss of data. */
          std_conv->warning_suggested = ec_lossy_conversion;
        }  /* if */
      }  /* if */
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
            std_conv->is_mild_warning = TRUE;
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
#if C99_IL_EXTENSIONS_SUPPORTED
        } else if (gpp_mode &&
                   is_complex_type(source_type) !=is_complex_type(dest_type)) {
          /* GNU C++ does not allow _Complex double -> double and vice versa,
             for example.  C99 and GNU C do allow those conversions. */
          if (gnu_version >= 40300 && is_complex_type(dest_type)) {
            /* As of g++ 4.3, double -> _Complex double is allowed, but not
               the other way around. */
            std_conv->gpp_conv_of_real_to_complex = TRUE;
          } else {
            okay = FALSE;
          }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        }  /* if */
      }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
      if (c99_mode) {
        /* Conversions between imaginary and real/integral drop information,
           so warn about those.  Don't warn if the source is a zero constant,
           because that may be intentional. */
        if ((is_imaginary(source_type) && !is_nonreal_floating(dest_type)) ||
            (is_imaginary(dest_type)   && !is_nonreal_floating(source_type) &&
             (!source_is_constant || !is_zero_constant(source_constant)))) {
          std_conv->warning_suggested = ec_real_imaginary_conversion;
          std_conv->is_mild_warning = TRUE;
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
    /* Destination type is pointer. */
    okay = impl_pointer_conversion(source_type, source_is_constant,
                                   source_is_string_literal,
                                   source_is_function,
                                   source_constant, dest_type,
                                   allow_qualifier_or_eh_mismatch,
                                   suppress_extensions,
                                   default_warning_code,
                                   std_conv);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_handle_type(dest_type)) {
    /* Destination type is a C++/CLI handle. */
    if (boxing_conversion_possible(source_type, dest_type, std_conv)) {
      /* A boxing conversion is possible. */
      okay = TRUE;
    } else if (source_is_string_literal &&
               cli_string_literal_conversion_possible(source_type, dest_type,
                                                      std_conv)) {
      /* A string literal conversion is possible (string literal -->
         handle to System::String --> dest_type). */
      okay = TRUE;
    } else {
      okay = impl_handle_conversion(source_type, dest_type,
                                    allow_qualifier_or_eh_mismatch,
                                    std_conv);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (is_ptr_to_member(dest_type)) {
    /* Conversion to a C++ pointer-to-member type. */
    okay = impl_ptr_to_member_conversion(source_type,
                                         source_is_constant,
                                         source_is_function,
                                         source_constant,
                                         dest_type,
                                         allow_qualifier_or_eh_mismatch,
                                         std_conv);
  } else if (is_nullptr(dest_type)) {
    if (is_nullptr(source_type) ||
        (source_is_constant && !dest_type->incomplete &&
         is_or_might_be_null_pointer_constant(source_constant))) {
      /* An expression with a nullptr type can be converted to both
         std::nullptr_t and the managed nullptr type; however, a 0-valued
         integral constant expression cannot be converted to the managed
         nullptr type (identified by being an incomplete type). */
      okay = TRUE;
      std_conv->nontrivial_conversion = !is_nullptr(source_type);
      /* We only want to set pointer_normalization_needed for integral
         types: there should be an error for attempting to pass 0 to a
         non-type template parameter of type std::nullptr_t without a cast,
         but passing a template parameter should be allowed, since it might
         be instantiated as std::nullptr_t. */
      std_conv->pointer_normalization_needed = is_integral(source_type);
    }  /* if */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode && std_conv->warning_suggested == ec_no_error &&
             ilp64_will_narrow(source_type, dest_type)) {
    /* Check for potential problems when porting to an ILP64 environment. */
    std_conv->warning_suggested = ec_ilp64_will_narrow;
    std_conv->is_mild_warning = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (okay && !source_is_constant &&
             std_conv->warning_suggested == ec_no_error &&
             is_conv_from_64_bit_integral_to_smaller(source_type, dest_type)) {
    /* Conversion from a 64-bit integral type to a smaller integral type.
       Warn because this is a 64-bit porting issue.  This diagnostic is
       suppressed by default (see cmd_line.c). */
    std_conv->warning_suggested = ec_impl_narrowing_64_bit_int;
    std_conv->is_mild_warning = TRUE;
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


a_boolean impl_converted_constant_expr_conversion_possible(
                                           a_type_ptr       source_type,
                                           a_boolean        source_is_constant,
                                           a_constant       *source_constant,
                                           a_type_ptr       dest_type,
                                           an_error_code    *err_code)
/*
Return TRUE if a conversion from source_type to dest_type is allowed
as the implicit conversion on a converted constant expression (see
[expr.const] in the C++11 standard).  If source_is_constant is TRUE,
source_constant gives the constant value of the source; if
source_is_constant is FALSE, the source is assumed not to be a constant.
If err_code is non-NULL, *err_code is set to an appropriate error code
if a specific one is appropriate, otherwise to ec_no_error if
a generic conversion error code appropriate to the context should be used.
Does not handle user-defined conversions.
*/
{
  a_boolean     okay = FALSE;
  an_error_code local_err_code = ec_no_error;

  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  if (identical_types(source_type, dest_type)) {
    okay = TRUE;
  } else if (constexpr_enabled ?
                      (is_template_param_or_nonreal_class_type(source_type) ||
                       is_template_param_or_nonreal_class_type(dest_type))
                    : (is_template_param_type(source_type) ||
                       is_template_param_type(dest_type))) {
    /* A template parameter type might match another type. */
    okay = TRUE;
  } else if (is_error_type(source_type) ||
             is_error_type(dest_type)) {
    /* An error type might match another type. */
    okay = TRUE;
  } else if (is_integral_or_unscoped_enum_type(source_type) &&
             is_integral_type(dest_type)) {
    /* Integral or enum to integral is allowed as long as it's not a
       narrowing conversion. */
    if (gpp_mode || microsoft_mode) {
      /* g++ doesn't seem to do the narrowing check (as of 4.7).  Assume
         Microsoft mode shouldn't either (even though there is no MSVC that
         supports constexpr yet as of 2012). */
      okay = TRUE;
    } else if (!is_narrowing_conversion(source_type,
                                        source_is_constant ? source_constant :
                                                            (a_constant *)NULL,
                                        dest_type,
                                        &local_err_code)) {
      okay = TRUE;
    }  /* if */
  }  /* if */
  if (err_code != NULL) {
    *err_code = local_err_code;
  }  /* if */
  return okay;
}  /* impl_converted_constant_expr_conversion_possible */


a_boolean conversion_allowed_for_nontype_template_argument(
                                           a_std_conv_descr *conversion,
                                           a_type_ptr       source_type,
                                           a_boolean        source_is_constant,
                                           a_constant       *source_constant,
                                           a_type_ptr       dest_type,
                                           an_error_code    *err_code)
/*
Return TRUE unless the indicated conversion contains something that
is not allowed in a conversion for a nontype template argument, e.g.,
a conversion of 0 to a pointer type.  "conversion" is a previously
determined conversion description, which is often enough to resolve the
question.  Additionally, for C++11 constant expressions, source_type,
source_is_constant, source_constant, and dest_type may be specified.
If those are not available, dest_type is passed as NULL.
If err_code is non-NULL, *err_code is set to an appropriate error code
if there is an error, otherwise to ec_no_error.  Does not handle
user-defined conversions.
*/
{
  a_boolean     allowed = TRUE;
  an_error_code local_err_code = ec_no_error;

  if (conversion->pointer_normalization_needed) {
    /* Conversion of 0 to a pointer type, or of a pointer to object type
       to void *, is not allowed on a nontype template argument. */
    allowed = FALSE;
    /* But MSVC does allow it. */
    if (microsoft_mode) allowed = TRUE;
  } else if (conversion->cast_base_class != NULL) {
    /* Derived-to-base pointer conversions and base-to-derived
       pointer-to-member conversions are not allowed on a nontype
       template argument. */
    allowed = FALSE;
  } else if (constexpr_enabled) {
    /* C++11 checks.  More checks are needed in part because C++11
       constant expressions allow more things inside the expression. */
    if (dest_type != NULL && is_integral_or_unscoped_enum_type(dest_type)) {
      /* For integer and enum nontype template parameters, the conversions
         are those allowed for a converted constant expression. */
      if (!impl_converted_constant_expr_conversion_possible(source_type,
                                                            source_is_constant,
                                                            source_constant,
                                                            dest_type,
                                                            &local_err_code)) {
        allowed = FALSE;
      }  /* if */
    } else {
      /* For other cases, e.g., address constants, most nontrivial conversions
         are disallowed. */
      if (conversion->nontrivial_conversion) {
        allowed = FALSE;
        if (dest_type != NULL) {
          if (is_template_dependent_context() &&
              (is_template_dependent_type(source_type) ||
               is_template_dependent_type(dest_type))) {
            /* Conversions between template parameter types are allowed. */
            allowed = TRUE;
          } else if (is_error_type(source_type) ||
                     is_error_type(dest_type)) {
            /* Conversions between error types are allowed. */
            allowed = TRUE;
          } else if (source_is_constant &&
                     is_nullptr_type(source_constant->type) &&
                     (is_pointer_or_handle_type(dest_type) ||
                      is_ptr_to_member_type(dest_type))) {
            /* Conversions from nullptr to a pointer or pointer to member are
               allowed. */
            allowed = TRUE;
          }  /* if */
        }  /* if */
      } /* if */
    }  /* if */
  }  /* if */
  if (err_code != NULL) {
    /* Return an appropriate error code. */
    if (allowed) {
      local_err_code = ec_no_error;
    } else if (local_err_code == ec_no_error) {
      local_err_code = ec_bad_nontype_template_arg;
    }  /* if */
    *err_code = local_err_code;
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
that allows the inverse of any standard conversion.  This function also
returns TRUE for a few conversions allowed by static_cast but not quite the
inverse of an implicit conversion (e.g., enum --> enum).  typerefs are already
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
  a_type_ptr       source_type_pointed_to = NULL, dest_type_pointed_to = NULL;
  a_boolean        qualifiers_added;

  clear_std_conv_descr(std_conv);
  if (related_class_pointers_or_handles(source_type, dest_type,
                                        &baseward_cast, &bcp) &&
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
                                /*source_is_function=*/FALSE,
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
  } else if ((!is_bool_type(source_type) && !is_nullptr_type(dest_type) &&
              impl_conversion_possible(dest_type,
                                       /*source_is_constant=*/FALSE,
                                       /*source_is_string_literal=*/FALSE,
                                       /*source_is_function=*/FALSE,
                                       (a_constant *)NULL,
                                       source_type,
                                       allow_qualifier_or_eh_mismatch,
                                       suppress_extensions,
                                       ec_bad_cast,
                                       std_conv)
#if MICROSOFT_EXTENSIONS_ALLOWED
              /* Don't allow the inverse of boxing conversions.  If a
                 conversion like that is to be allowed, let it come in
                 openly via unboxing_conversion_possible (which covers
                 some additional cases). */
              && !boxing_conversion_possible(dest_type, source_type,
                                             (a_std_conv_descr *)NULL)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                      ) ||
             /* Test for conversion of "void *" to a pointer to object type.
                This does not fall out of the impl_conversion_possible
                test for cases like "void *" --> "const char *".  See
                5.2.9/10 in the C++ standard. */
             (is_pointer_type(source_type) &&
              is_pointer_type(dest_type) &&
              is_void_type(type_pointed_to(source_type)) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
              !is_interior_ptr_type(source_type) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              is_object_type(type_pointed_to(dest_type)))) {
    /* The inverse implicit conversion can be done.  Note that the
       inverse of conversions to bool is not allowed (see [expr.static.cast]
       paragraph 9).  The Standard does not currently disallow the inverse
       of the null pointer and null pointer-to-member conversions, but
       that appears to have been an oversight, so casts to std::nullptr_t
       are also not permitted. */
    okay = TRUE;
    /* If the conversion is a pointer or pointer to member conversion, make
       sure qualifiers are not being removed. */
    if (!allow_qualifier_or_eh_mismatch &&
        ((is_pointer_or_handle(source_type) &&
          is_pointer_or_handle(dest_type)) ||
        (is_ptr_to_member(source_type) && is_ptr_to_member(dest_type)))) {
      if (cast_removes_qualifiers(source_type, dest_type,
                                  (an_error_code *)NULL)) {
        okay = FALSE;
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (okay && cli_or_cx_enabled &&
        is_prohibited_interior_ptr_conversion(source_type, dest_type)) {
      /* Conversion from an interior_ptr to a non-interior_ptr is not
         allowed, because it loses the gc-ness of the pointer. */
      okay = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (is_enum_type(dest_type) &&
             (is_integral_or_enum(source_type) ||
              is_floating_type(source_type))) {
    /* Core Issue 128 makes enum --> enum a valid static_cast.  This also
       covers integer --> scoped enum (the unscoped case is a normal
       inverse of an implicit conversion), and bool --> enum (both scoped
       and unscoped). */
    /* Core Issue 1094 makes floating --> scoped enum valid. */
    okay = TRUE;
  } else if (is_enum_type(source_type) &&
             (is_integral_type(dest_type) ||
              is_floating_type(dest_type))) {
    /* Similarly, Core Issue 671 makes scoped enum --> integer valid. */
    /* And Core Issue 833 makes scoped enum --> floating valid. */
    okay = TRUE;
  }  /* if */
  return okay;
}  /* inverse_impl_conversion_possible */


static a_boolean static_cast_conversion_possible_full(
                                 a_type_ptr    source_type,
                                 a_boolean     source_is_constant,
                                 a_boolean     source_is_string_literal,
                                 a_boolean     source_is_function,
                                 a_constant    *source_constant,
                                 a_type_ptr    dest_type,
                                 a_boolean     allow_qualifier_or_eh_mismatch,
                                 an_error_code default_warning_code,
                                 an_error_code *warning_suggested,
                                 a_boolean     *is_mild_warning)
/*
Return TRUE if it is okay to explicitly convert something of type source_type
to something of type dest_type in a static_cast.  If source_is_constant is
TRUE, the source is a constant, and source_constant points to the constant
value.  (That's needed to check for conversions of a null pointer constant
to a pointer type.)  If source_is_string_literal is TRUE, source is a
simple string literal (that's needed for the deprecated conversion
from string literal to "char *"); the flag can be TRUE even when
source_is_constant is FALSE, for an extension.  If source_is_function
is TRUE, the source is the address of a specific function, which
matters for a particular C++/CLI conversion.  If the conversion is
suspect and should be flagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  *is_mild_warning is returned TRUE if the
warning in *warning_suggested is more of an observation rather than a
conformance issue.  Any type qualifiers on the types themselves
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

  db_enter(5, "static_cast_conversion_possible_full");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "static_cast_conversion_possible_full: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  *is_mild_warning = FALSE;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  check_assertion_str(!is_reference_ptr(dest_type),
               "static_cast_conversion_possible_full: dest_type is reference");

  if (is_void(dest_type)) {
    /* Anything --> (possibly qualified) void is allowed. */
    okay = TRUE;
  } else if (is_incomplete(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else {
    impl_okay = impl_conversion_possible(source_type, source_is_constant,
                                         source_is_string_literal,
                                         source_is_function,
                                         source_constant, dest_type,
                                         allow_qualifier_or_eh_mismatch,
                                         suppress_extensions,
                                         default_warning_code,
                                         &impl_std_conv) != FALSE;
    if (impl_okay &&
        (impl_std_conv.warning_suggested == ec_no_error ||
         impl_std_conv.is_mild_warning ||
         /* Special-case two warnings we'd like to go with without testing the
            inverse conversion. */
         impl_std_conv.warning_suggested == ec_restrict_qualifier_dropped ||
         impl_std_conv.warning_suggested == ec_unaligned_qualifier_dropped)) {
      /* There is an implicit conversion, and it's not questionable. */
      okay = TRUE;
      *warning_suggested = impl_std_conv.warning_suggested;
      *is_mild_warning = impl_std_conv.is_mild_warning;
      if (*is_mild_warning) {
        /* A mixed int/enum warning, possible in C mode, should get no
           warning when an explicit cast is used. */
        if (*warning_suggested == ec_mixed_enum_type) {
          *warning_suggested = ec_no_error;
          *is_mild_warning = FALSE;
        }  /* if */
      }  /* if */
    } else if (!C_mode()) {
      inv_impl_okay = inverse_impl_conversion_possible(
                                               source_type, dest_type,
                                               suppress_extensions,
                                               allow_qualifier_or_eh_mismatch,
                                               &inv_impl_std_conv);
      if (inv_impl_okay &&
          (inv_impl_std_conv.warning_suggested == ec_no_error ||
           inv_impl_std_conv.is_mild_warning)) {
        /* The inverse of any standard conversion is allowed in C++. */
        okay = TRUE;
        *warning_suggested = inv_impl_std_conv.warning_suggested;
        *is_mild_warning = inv_impl_std_conv.is_mild_warning;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cli_or_cx_enabled &&
                 unboxing_conversion_possible(source_type, dest_type,
                                              (a_std_conv_descr *)NULL)) {
        /* An unboxing conversion is allowed in C++/CLI. */
        okay = TRUE;
      } else if (cli_or_cx_enabled &&
                 is_handle_ptr(source_type) &&
                 is_interior_ptr_type(dest_type)) {
        a_type_ptr under_source = type_pointed_to(source_type);
        a_type_ptr under_dest = type_pointed_to(dest_type);
        if (is_value_class_type(under_source) &&
            types_are_compatible(under_source, under_dest)) {
          /* A conversion from a handle to X to an interior_ptr<X> is
             allowed for value classes. */
          okay = TRUE;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!okay) {
    if (impl_okay) {
      /* There is a questionable implicit conversion that covers this case.
         Allow it, with a warning. */
      okay = TRUE;
      *warning_suggested = impl_std_conv.warning_suggested;
      *is_mild_warning = impl_std_conv.is_mild_warning;
    } else if (inv_impl_okay) {
      /* There is a questionable inverse conversion that covers this
         case.  Allow it, with a warning. */
      okay = TRUE;
      *warning_suggested = inv_impl_std_conv.warning_suggested;
      *is_mild_warning = inv_impl_std_conv.is_mild_warning;
    }  /* if */
  }  /* if */

#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "static_cast_conversion_possible_full: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* static_cast_conversion_possible_full */


a_boolean static_cast_conversion_possible(
                                 a_type_ptr    source_type,
                                 a_boolean     source_is_constant,
                                 a_boolean     source_is_string_literal,
                                 a_boolean     source_is_function,
                                 a_constant    *source_constant,
                                 a_type_ptr    dest_type,
                                 a_boolean     allow_qualifier_or_eh_mismatch,
                                 an_error_code default_warning_code,
                                 an_error_code *warning_suggested)
/*
Interface to static_cast_conversion_possible for the simple case
where the is_mild_warning parameter is not needed.
*/
{
  a_boolean is_mild_warning;
  a_boolean okay = static_cast_conversion_possible_full(
                                                source_type,
                                                source_is_constant,
                                                source_is_string_literal,
                                                source_is_function,
                                                source_constant,
                                                dest_type,
                                                allow_qualifier_or_eh_mismatch,
                                                default_warning_code,
                                                warning_suggested,
                                                &is_mild_warning);
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

  if (related_class_pointers_or_handles(source_type, dest_type,
                                        &baseward_cast, &bcp)) {
    /* A cast from const Derived * to Base * is allowed as a combination
       of a static_cast and a const_cast. */
    okay = TRUE;
  }  /* if */
  return okay;
}  /* compound_conversion_possible */


static a_boolean reinterpret_cast_conversion_possible_full(
                                              a_type_ptr    source_type,
                                              a_type_ptr    dest_type,
                                              an_error_code *warning_suggested,
                                              a_boolean     *is_mild_warning)
/*
Return TRUE if it is okay to explicitly convert something of type source_type
to something of type dest_type in a reinterpret_cast.  Any type qualifiers
on the types themselves are ignored.  If the conversion
is suspect and should be flagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
*is_mild_warning is returned TRUE if the warning in *warning_suggested
is more of an observation rather than a conformance issue.
Note that this routine does not handle casts to reference types, it
doesn't reject conversions that cast away constness, and it doesn't
handle user-defined conversions.  This routine is called in C mode as
well as C++ mode.
*/
{
  a_boolean okay = FALSE, suppress_extensions = FALSE;

  db_enter(5, "reinterpret_cast_conversion_possible_full");
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug,
            "reinterpret_cast_conversion_possible_full: source_type = ");
    db_abbreviated_type(source_type);
    fprintf(f_debug, ", dest_type = ");
    db_abbreviated_type(dest_type);
    fprintf(f_debug, "\n");
  }  /* if */
#endif /* DEBUG */
  *warning_suggested = ec_no_error;
  *is_mild_warning = FALSE;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);
  check_assertion_str(!is_reference_ptr(dest_type),
          "reinterpret_cast_conversion_possible_full: dest_type is reference");

  if (is_incomplete(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else if (((is_pointer(source_type)
#if MICROSOFT_EXTENSIONS_ALLOWED
               && !source_type->variant.pointer.is_interior_ptr
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                               ) ||
              (is_nullptr(source_type) && !source_type->incomplete)) &&
             is_integral(dest_type) &&
             (C_mode() || ms_extensions || gpp_mode ||
              dest_of_ptr_cast_big_enough(source_type, dest_type))) {
    /* Pointer or std::nullptr_t (but not the managed nullptr type,
       identified by being incomplete) --> integral is okay
         -- In C mode, always (size of destination is not an issue; see
            6.3.4 in the ISO C89 standard)
         -- In C++ mode, if (a) the integer is big enough or (b) it's
            not big enough but we're compiling in Microsoft or GNU mode. */
    okay = TRUE;
    if (!dest_of_ptr_cast_big_enough(source_type, dest_type)) {
      /* The destination is not large enough to hold all of the bits
         of the pointer.  Issue a warning. */
      *warning_suggested = ec_pointer_conversion_loses_bits;
      *is_mild_warning = TRUE;
    } else if (source_type->size == dest_type->size) {
      /* The conversion is to a same-sized integral type.  Warn about
         this as a 64-bit porting issue (but the diagnostic is turned
         off by default). */
      *warning_suggested = ec_pointer_conversion_to_same_size_int;
      *is_mild_warning = TRUE;
    }  /* if */
  } else if (is_integral_or_enum(source_type) &&
             ((is_non_cli_pointer(dest_type)
#if UPC_EXTENSIONS_ALLOWED
              /* Casting an integer to a ptr-to-shared is not allowed. */
               && !(upc_mode && is_ptr_to_shared_type(dest_type))
#endif /* UPC_EXTENSIONS_ALLOWED */
                                                                 ) ||
              (cpp11_mode && identical_types(source_type, dest_type)))) {
    /* Integral or enum --> pointer or same integral or enum type. */
    okay = TRUE;
    if (is_pointer_type(dest_type) &&
        !dest_of_ptr_cast_big_enough(source_type, dest_type)) {
      /* The destination is not large enough to hold all of the bits
         of the integer.  Issue a warning. */
      *warning_suggested = ec_conversion_to_pointer_loses_bits;
    }  /* if */
  } else if (is_pointer(source_type) && is_pointer(dest_type)) {
    /* Pointer --> pointer.  Get the types pointed to. */
    a_type_ptr source_type_pointed_to, dest_type_pointed_to;
#if UPC_EXTENSIONS_ALLOWED
    a_boolean  source_is_shared = FALSE, dest_is_shared = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
    source_type_pointed_to = type_pointed_to(source_type);
#if UPC_EXTENSIONS_ALLOWED
    source_is_shared =
       upc_mode && is_underlying_shared_qualified_type(source_type_pointed_to);
#endif /* UPC_EXTENSIONS_ALLOWED */
    source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    dest_type_pointed_to = type_pointed_to(dest_type);
#if UPC_EXTENSIONS_ALLOWED
    dest_is_shared =
         upc_mode && is_underlying_shared_qualified_type(dest_type_pointed_to);
#endif /* UPC_EXTENSIONS_ALLOWED */
    dest_type_pointed_to = skip_typerefs(dest_type_pointed_to);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (is_cli_pointer(source_type) ||
        is_cli_pointer(dest_type)) {
      /* Some casts involving interior_ptr or pin_ptr in C++/CLI are not
         allowed. */
      if (is_interior_ptr_type(source_type) &&
          is_interior_ptr_type(dest_type)) {
        /* interior_ptr --> interior_ptr is okay. */
        okay = TRUE;
      } else if (is_pin_ptr_type(source_type)) {
        /* pin_ptr --> any pointer type is okay. */
        okay = TRUE;
      } else {
        okay = FALSE;
      }  /* if */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (is_template_param(source_type_pointed_to) ||
        is_template_param(dest_type_pointed_to)) {
      /* Cast involving template parameter types, in a prototype
         instantiation. */
      okay = TRUE;
    } else if (is_function(source_type_pointed_to) ==
               is_function(dest_type_pointed_to)) {
      /* Pointer to function --> pointer to function, or pointer to
         object/incomplete --> pointer to object/incomplete.  Allowed in both
         C and C++. */
      okay = TRUE;
#if UPC_EXTENSIONS_ALLOWED
      /* Do not allow a cast from non-shared to shared. */
      if (!source_is_shared && dest_is_shared) {
        okay = FALSE;
      }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
    } else {
      /* Pointer to function --> pointer to object/incomplete, or pointer
         to object/incomplete --> pointer to function.  If the destination
         is at least as large as the source, this is allowed in C++ as
         conditionally-supported behavior (see DR 195, adopted in April,
         2005) and in C as an extension. */
      if ((!C_mode() || !suppress_extensions) &&
          dest_of_ptr_cast_big_enough(source_type, dest_type)) {
        okay = TRUE;
        if (C_mode() && strict_ansi_mode) {
          *warning_suggested = ec_ptr_func_ptr_data_conv;
          if ((int)strict_ansi_error_severity < (int)es_error) {
            *is_mild_warning = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (is_handle_ptr(dest_type)) {
    /* Conversion to a C++/CLI handle type. */
    if (is_handle_ptr(source_type)) {
      okay = TRUE;
      if (!identical_types(source_type, dest_type)) {
        *warning_suggested = ec_reinterpret_cast_of_handle;
      }  /* if */
    } else if (cppcx_enabled && is_pointer(source_type)) {
      /* Conversion from a pointer to a handle type. */
      okay = TRUE;
    } else if (is_nullptr(source_type) &&
               !is_cli_generic_definition_argument_type(dest_type)) {
      okay = TRUE;
    }  /* if */
  } else if (is_handle_ptr(source_type)) {
    /* Conversion from a C++/CLI handle type (destination type is not a
       handle). */
    if (is_interior_ptr_type(dest_type)) {
      /* Handle --> interior_ptr is allowed. */
      okay = TRUE;
    } else if (cppcx_enabled && is_pointer(dest_type)) {
      /* Conversion from a handle to a pointer type. */
      okay = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (gnu_mode &&
             (is_vector_type(source_type) || is_vector_type(dest_type))) {
    /* Vector types are convertible to and from other vector types, integer
       types, and enum types, provided the two types have the same size. */
    if (source_type->size == dest_type->size) {
      a_boolean  src_is_vec = is_vector_type(source_type);
      a_boolean  dst_is_vec = is_vector_type(dest_type);
      okay = (src_is_vec && dst_is_vec) ||
             (src_is_vec && is_integral_or_enum(dest_type)) ||
             (dst_is_vec && is_integral_or_enum(source_type));
    }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  }  /* if */
  if (!okay) {
    /* No normal conversion.  Look for error and template matches. */
    if (is_error(dest_type) || is_template_param_type(dest_type)) {
      if (is_error(source_type) || is_template_param_type(source_type) ||
          is_integral_or_enum(source_type) ||
          is_pointer_or_handle(source_type) ||
          is_ptr_to_member(source_type)) {
        okay = TRUE;
      }  /* if */
    } else if (is_error(source_type) || is_template_param_type(source_type)) {
      if (is_integral_or_enum(dest_type) ||
          is_pointer_or_handle(dest_type) ||
          is_ptr_to_member(dest_type)) {
        okay = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode &&
      (*warning_suggested == ec_no_error ||
       *warning_suggested == ec_pointer_conversion_to_same_size_int) &&
      ilp64_will_narrow(source_type, dest_type)) {
    /* Check for potential problems when porting to an ILP64 environment. */
    *warning_suggested = ec_ilp64_will_narrow;
    *is_mild_warning = TRUE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DEBUG
  if (debug_level >= 5) {
    fprintf(f_debug, "reinterpret_cast_conversion_possible_full: %s\n",
                     okay ? "okay" : "not okay");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return okay;
}  /* reinterpret_cast_conversion_possible_full */


a_boolean reinterpret_cast_conversion_possible(
                                              a_type_ptr    source_type,
                                              a_type_ptr    dest_type,
                                              an_error_code *warning_suggested)
/*
Interface to reinterpret_cast_conversion_possible_full for the simple
case where the is_mild_warning parameter is not needed.
*/
{
  a_boolean is_mild_warning;
  a_boolean okay = reinterpret_cast_conversion_possible_full(source_type,
                                                             dest_type,
                                                             warning_suggested,
                                                             &is_mild_warning);
  return okay;
}  /* reinterpret_cast_conversion_possible */


a_boolean expl_conversion_possible(a_type_ptr    source_type,
                                   a_boolean     source_is_constant,
                                   a_boolean     source_is_string_literal,
                                   a_boolean     source_is_function,
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
source_is_constant is FALSE, for an extension.  If source_is_function
is TRUE, the source is the address of a specific function, which
matters for a particular C++/CLI conversion.  Any type qualifiers on
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
  a_boolean     static_cast_warning_is_mild;
  an_error_code reinterpret_cast_warning_suggested;
  a_boolean     reinterpret_cast_warning_is_mild;

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

  if (is_incomplete(dest_type) && !is_void(dest_type) &&
      !is_managed_nullptr_type(dest_type)) {
    /* Cannot cast to an incomplete type. */
    /* okay = FALSE; -- already set. */
  } else {
    static_cast_okay = static_cast_conversion_possible_full(
                                      source_type, source_is_constant,
                                      source_is_string_literal,
                                      source_is_function,
                                      source_constant, dest_type,
                                      /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                      default_warning_code,
                                      &static_cast_warning_suggested,
                                      &static_cast_warning_is_mild) != FALSE;

    if (static_cast_warning_suggested == ec_impl_narrowing_64_bit_int) {
      /* Change a message that refers to an implicit conversion to one
         referring to an explicit conversion. */
      static_cast_warning_suggested = ec_expl_narrowing_64_bit_int;
    }  /* if */
    if (static_cast_okay &&
        (static_cast_warning_suggested == ec_no_error ||
         static_cast_warning_is_mild)) {
      /* The conversion can be done as a static_cast, without a warning. */
      okay = TRUE;
      *warning_suggested = static_cast_warning_suggested;
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
            reinterpret_cast_conversion_possible_full(
                              source_type, dest_type,
                              &reinterpret_cast_warning_suggested,
                              &reinterpret_cast_warning_is_mild) != FALSE;
      if (reinterpret_cast_okay &&
          (reinterpret_cast_warning_suggested == ec_no_error ||
           reinterpret_cast_warning_is_mild)) {
        /* The conversion can be done as a reinterpret_cast, without a
           warning or with a mild warning. */
        okay = TRUE;
        *reinterpret_cast_needed = TRUE;
        *warning_suggested = reinterpret_cast_warning_suggested;
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

#if GENERATE_SOURCE_SEQUENCE_LISTS

void disentangle_default_args(a_type_ptr  rtp1,
                              a_type_ptr  rtp2)
/*
rtp1 and rtp2 are routine types that may be sharing default argument
expressions (e.g., as the result of calling composite_routine_type).  If the
types are distinct IL entries, they cannot coexist in the final IL tree:
Resolve the issue by duplicating the default argument expressions if needed.
*/
{
  rtp1 = skip_typerefs(rtp1);
  rtp2 = skip_typerefs(rtp2);
  if (rtp1 != rtp2) {
    /* Distinct routine type entries: Traverse their parameters. */
    a_param_type_ptr  ptp1, ptp2;
    ptp1 = rtp1->variant.routine.extra_info->param_type_list;
    ptp2 = rtp2->variant.routine.extra_info->param_type_list;
    while (ptp1 != NULL && ptp2 != NULL) {
      if (ptp1->default_arg_expr != NULL &&
          ptp1->default_arg_expr == ptp2->default_arg_expr) {
        /* A shared default argument expression: Duplicate it to avoid the
           sharing. */
        ptp1->default_arg_expr =
                          duplicate_default_arg_expr(ptp2->default_arg_expr);
      }  /* if */
      ptp1 = ptp1->next;
      ptp2 = ptp2->next;
    }  /* while */
  }  /* if */
}  /* disentangle_default_args */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static a_type_ptr make_cv_combined_type(a_type_ptr  tp1,
                                        a_type_ptr  tp2,
                                        a_type_ptr  common_sub_type)
/*
Return the cv-combined type of tp1 and tp2.  The caller has determined that
tp1 and tp2 are "similar" types.  This routine just recursively merges the
cv-qualifiers.  common_sub_type is a component of tp1 that has an equivalent
component in tp2 (so the recursion can end there).
*/
{
  a_type_ptr            result;
  a_type_qualifier_set  tqs1, tqs2;

  tqs1 = get_type_qualifiers(tp1);
  tqs2 = get_type_qualifiers(tp2);
  tp1 = skip_typerefs(tp1);
  tp2 = skip_typerefs(tp2);
  if (tp1 == common_sub_type) {
    result = common_sub_type;
  } else {
    result = alloc_type(tp1->kind);
    copy_type(tp1, result);
    switch (tp1->kind) {
      case tk_pointer:
        result->variant.pointer.type = make_cv_combined_type(
                                                   tp1->variant.pointer.type,
                                                   tp2->variant.pointer.type,
                                                   common_sub_type);
        break;
      case tk_ptr_to_member:
        result->variant.ptr_to_member.type = make_cv_combined_type(
                                              tp1->variant.ptr_to_member.type,
                                              tp2->variant.ptr_to_member.type,
                                              common_sub_type);
        break;
      case tk_array:
        result->variant.array.element_type = make_cv_combined_type(
                                              tp1->variant.array.element_type,
                                              tp2->variant.array.element_type,
                                              common_sub_type);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
  result = make_qualified_type(result, tqs1 | tqs2);
  return result;
}  /* make_cv_combined_type */


a_type_ptr make_cv_combined_type_if_possible(a_type_ptr  tp1,
                                             a_type_ptr  tp2)
/*
If tp1 and tp2 are similar types, single-level pointers to void or to related
class types, or single-level pointers-to-members of related class types, return
the corresponding cv-combined type (defined below).  Otherwise, if either type
is an error type, return an error type.  In all other cases return NULL.  Note
that for non-similar types the result can depend on the order of the tp1 and
tp2 arguments.

The cv-combined type of two types T1 and T2 is a type T3 similar to T1 whose
cv-qualification signature is determined as follows:
  - for every j > 0, cv3,j is the union of cv1,j and cv2,j
  - if the resulting cv3,j is different from cv1,j or cv2,j, then const is
    added to every cv3,k for 0 < k < j
(From N4431, paragraph 5/13.)
*/
{
  a_type_ptr  stp1 = tp1, stp2 = tp2, ustp1, ustp2;
  a_type_ptr  result = NULL;

  /* First handle the single-level pointer and pointer-to-member cases. */
  ustp1 = skip_typerefs(stp1);
  ustp2 = skip_typerefs(stp2);
  if (is_pointer_or_handle(ustp1) && is_pointer_or_handle(ustp2)) {
    stp1 = ustp1->variant.pointer.type;
    stp2 = ustp2->variant.pointer.type;
    ustp1 = skip_typerefs(stp1);
    ustp2 = skip_typerefs(stp2);
    if (identical_types(ustp1, ustp2)) {
      /* Combine the qualifiers. */
      result = make_pointer_type(
                        make_qualified_type(stp1, get_type_qualifiers(stp2)));
    } else if (ustp1->kind == (a_type_kind)tk_void) {
      /* Combine the qualifiers, keeping the non-void underlying type.  If the
         other underlying type is a function type, return NULL (this is not
         clear in the current C++ working paper -- N4527 -- but doing otherwise
         leads to unexpected results, such as "b ? (void*)p : (void(*)())q"
         being silently accepted). */
      if (ustp2->kind != (a_type_kind)tk_routine) {
        result = make_pointer_type(
                        make_qualified_type(stp2, get_type_qualifiers(stp1)));
      } else {
        goto done;
      }  /* if */
    } else if (ustp2->kind == (a_type_kind)tk_void) {
      /* Combine the qualifiers, keeping the non-void underlying type.  If the
         other underlying type is a function type, return NULL. */
      if (ustp1->kind != (a_type_kind)tk_routine) {
        result = make_pointer_type(
                        make_qualified_type(stp1, get_type_qualifiers(stp2)));
      } else {
        goto done;
      }  /* if */
    } else if (is_immediate_class_type(ustp1) &&
               is_immediate_class_type(ustp2)) {
      /* Check for related-class cases. */
      if (find_base_class_of(ustp1, ustp2) != NULL) {
        /* Return the pointer to the base class, adjusted with the qualifiers
           on the derived class. */
        result = make_pointer_type(
                        make_qualified_type(stp2, get_type_qualifiers(stp1)));
      } else if (find_base_class_of(ustp2, ustp1) != NULL) {
        /* Return the pointer to the base class, adjusted with the qualifiers
           on the derived class. */
        result = make_pointer_type(
                        make_qualified_type(stp1, get_type_qualifiers(stp2)));
      }  /* if */
    }  /* if */
  } else if (ustp1->kind == (a_type_kind)tk_ptr_to_member &&
             ustp2->kind == (a_type_kind)tk_ptr_to_member) {
    a_type_ptr  ctp1 = ustp1->variant.ptr_to_member.class_of_which_a_member;
    a_type_ptr  ctp2 = ustp2->variant.ptr_to_member.class_of_which_a_member;
    a_type_ptr  uctp1 = skip_typerefs(ctp1), uctp2 = skip_typerefs(ctp2);
    a_boolean   qualifiers_added;
    stp1 = ustp1->variant.ptr_to_member.type;
    stp2 = ustp2->variant.ptr_to_member.type;
    if (is_immediate_class_type(uctp1) && is_immediate_class_type(uctp2) &&
        member_types_correspond(skip_typerefs(stp1),
                                skip_typerefs(stp2),
                                /*source_is_function=*/FALSE,
                                /*allow_qualifier_or_eh_mismatch=*/TRUE,
                                &qualifiers_added)) {
      /* Check for related-class cases. */
      if (identical_types(ctp1, ctp2) ||
          find_base_class_of(ctp1, ctp2) != NULL) {
        stp1 = make_qualified_type(stp1, get_type_qualifiers(stp2));
        result = ptr_to_member_type(stp1, ctp1);
      } else if (find_base_class_of(ctp2, ctp1) != NULL) {
        stp2 = make_qualified_type(stp2, get_type_qualifiers(stp1));
        result = ptr_to_member_type(stp2, ctp2);
      }  /* if */
    }  /* if */
  } else if (ustp1->kind == (a_type_kind)tk_error ||
             ustp2->kind == (a_type_kind)tk_error) {
    /* If either type is an error type, return an error type. */
    result = error_type();
  } else {
    /* Not two pointers or not two pointers-to-members. */
    goto done;
  }  /* if */
  if (result == NULL) {
    /* Not one of the "special" single-level cases.  Handle normal, possibly
       multi-level, cases. */
    /* First check whether the types are similar, and record the outermost
       component that can be shared in the resulting cv-combined type. */
    a_type_ptr  common_sub_type = NULL;
    stp1 = tp1;
    stp2 = tp2;
    for (;;) {
      ustp1 = skip_typerefs(stp1);
      ustp2 = skip_typerefs(stp2);
      if (ustp1->kind != ustp2->kind) {
        /* The types are definitely not similar. */
        goto done;
      } else {
        /* Check if the remainder of the type is identical.  For routine types
           a difference in linkage is permissible in some modes. */
        if (ustp1->kind == (a_type_kind)tk_routine) {
          if (types_are_compatible_for_impl_conversion(ustp1, ustp2)) {
            common_sub_type = ustp1;
            break;
          }  /* if */
        } else {
          /* Not routines: Look for an exact match. */
          if (types_are_compatible(ustp1, ustp2)) {
            common_sub_type = ustp1;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      switch (ustp1->kind) {
        case tk_pointer:
          check_assertion(!ustp1->variant.pointer.is_reference &&
                          !ustp2->variant.pointer.is_reference);
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (!same_cli_pointer_kinds(ustp1, ustp2)) {
            goto done;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          stp1 = ustp1->variant.pointer.type;
          stp2 = ustp2->variant.pointer.type;
          break;
        case tk_ptr_to_member:
          if (!identical_types(
                      ustp1->variant.ptr_to_member.class_of_which_a_member,
                      ustp2->variant.ptr_to_member.class_of_which_a_member)) {
            goto done;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (microsoft_mode &&
                     ustp1->variant.ptr_to_member.modifiers !=
                                     ustp2->variant.ptr_to_member.modifiers) {
            /* A difference in __ptr32 or __ptr64 modifiers makes pointer types
               incompatible. */
            goto done;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          }  /* if */
          stp1 = ustp1->variant.ptr_to_member.type;
          stp2 = ustp2->variant.ptr_to_member.type;
          break;
        case tk_array:
          if (identical_array_type_level(ustp1, ustp2)) {
            /* Compatible array components. */
          } else if (vla_enabled &&
                     (array_is_vla(ustp1) || array_is_vla(ustp2))) {
            /* If a VLA is involved, we consider the array dimensions
               compatible.  (Clang and current versions of GCC are a bit
               stricter about this.) */
          } else {
            goto done;
          }  /* if */
          stp1 = ustp1->variant.array.element_type;
          stp2 = ustp2->variant.array.element_type;
          break;
        default:
          /* Since the identical_types test above failed, tp1 and tp2 are not
             similar. */
          goto done;
      }  /* switch */
    }  /* for */
    check_assertion(common_sub_type != NULL);
    /* Now build the combined type. */
    result = make_cv_combined_type(tp1, tp2, common_sub_type);
  }  /* if */
done:
  return result;
}  /* make_cv_combined_type_if_possible */


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
    } else if (is_pointer(type_1) && is_pointer(type_2)
#if MICROSOFT_EXTENSIONS_ALLOWED
               && same_cli_pointer_kinds(type_1, type_2)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              ) {
      a_type_ptr  type_pointed_to_1 = type_pointed_to(type_1);
      a_type_ptr  type_pointed_to_2 = type_pointed_to(type_2);

      result = multilevel_composite_pointer_type(type_pointed_to_1,
                                                 type_pointed_to_2);
      if (result != NULL) {
        result = make_qualified_type(result,
                                     get_type_qualifiers(type_pointed_to_1) |
                                       get_type_qualifiers(type_pointed_to_2));
        result = make_pointer_type_of_same_kind(result, type_1);
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
  a_type_ptr    comp_type = NULL, comp_elem = NULL;
  a_targ_size_t num_elems = 0;
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

#if GNU_EXTENSIONS_ALLOWED
  /* In GNU C mode, a parameter of transparent union type is compatible
     with a corresponding parameter that has a type of a field of that
     union. */
  tp = NULL;
  if (gcc_mode && skip_typerefs(type1)->kind != skip_typerefs(type2)->kind) {
    if ((is_transparent_union_type(type1) && !is_error_type(type2)) ||
        (is_transparent_union_type(type2) && !is_error_type(type1))) {
      /* Note that the first type is always used as the composite type, which
         results in behavior close to that of GCC's.  For example:
           union __attribute((transparent_union)) TU { int i; int *p; };
           void f(int i); // First declaration.
           void f(union TU tu);  // Redeclaration, type "int" retained.
           void g(int x) { f(&x); }  // Warning: int*->int conversion.
         However, our behavior is slightly different from GCC's in that we
         always retain the parameter type of a function definition (if a
         function definition is involved, type1 will be the parameter type
         from the definition).  Continuing the example above:
           void f(union TU tu) {}  // Definition, type "tu" retained.
           void h(int x) { f(&x); }  // No warning, no conversion.  */
      check_assertion(transparent_union_match(type1, type2));
      tp = type1;
    }  /* if */
  }  /* if */
  if (tp != NULL) {
    /* We're done. */
  } else
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
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
If a type distinct from those passed in is returned, it will share its default
argument expressions with the types passed in.  This is usually okay because
the original types are typically discarded in such cases, but if any of the
original types are retained, the default argument expressions should be made
unique to each type (e.g., by calling disentangle_default_args).
*/
{
  a_type_ptr                     comp_type;
  a_type_ptr                     comp_return_type, tp;
  a_param_type_ptr               param_list1, param_list2, end_of_list;
  a_param_type_ptr               ptp1, ptp2, new_ptp;
  a_boolean                      comp_prototyped, comp_trailing_return_type;
  a_boolean                      comp_does_not_return;
  a_routine_type_supplement_ptr  rtsp1, rtsp2, rtsp;
  a_boolean                      return_type1_as_comp_type = TRUE;
  a_boolean                      return_type2_as_comp_type = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_calling_convention           comp_calling_convention =
                                              (a_calling_convention)cc_default;
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
  /* The composite type will have the "trailing_return_type" set to TRUE if
     either of the original types has it set to TRUE. */
  comp_trailing_return_type = rtsp1->trailing_return_type ||
                              rtsp2->trailing_return_type;
  if (rtsp1->trailing_return_type != comp_trailing_return_type) {
    return_type1_as_comp_type = FALSE;
  }  /* if */
  if (rtsp2->trailing_return_type != comp_trailing_return_type) {
    return_type2_as_comp_type = FALSE;
  }  /* if */
  /* The composite type will have the "does_not_return" flag set to TRUE if
     either of the original types has it set to TRUE. */
  comp_does_not_return = rtsp1->does_not_return || rtsp2->does_not_return;
  if (rtsp1->does_not_return != comp_does_not_return) {
    return_type1_as_comp_type = FALSE;
  }  /* if */
  if (rtsp2->does_not_return != comp_does_not_return) {
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
  /* Compute the composite calling convention if we're in Microsoft mode.
     In GNU mode, this is done by copy_gnu_type_properties. */
  if (ms_extensions) {
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
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!C_mode()) {
    if (rtsp1->exception_specification == NULL) {
      if (rtsp2->exception_specification != NULL) {
        return_type1_as_comp_type = FALSE;
      }  /* if */
    } else {
      if (rtsp2->exception_specification == NULL) {
        return_type2_as_comp_type = FALSE;
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
#if GNU_EXTENSIONS_ALLOWED
  if (rtsp1->is_const != rtsp2->is_const) {
    /* One routine has the GNU "const" attribute and the other not.  Do not
       use the non-const type as a composite type since it would incorrectly
       be imbued with the attribute. */
    if (rtsp1->is_const) {
      return_type2_as_comp_type = FALSE;
    } else {
      return_type1_as_comp_type = FALSE;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
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
      /* If attributes are present on a parameter, those attributes must be
         preserved and therefore the other type cannot be returned as the
         composite type. */
      if (ptp1->attributes != NULL) return_type2_as_comp_type = FALSE;
      if (ptp2->attributes != NULL) return_type1_as_comp_type = FALSE;
      if (!C_mode()) {
        /* Form the composite of the C++ default argument expressions;
           it's guaranteed that at most one of the parameter lists
           has a default argument expression. */
        if (ptp1->has_default_arg || ptp1->default_arg_expr != NULL) {
          return_type2_as_comp_type = FALSE;
        } else if (ptp2->has_default_arg || ptp2->default_arg_expr != NULL) {
          return_type1_as_comp_type = FALSE;
        }  /* if */
      }  /* if */
      if (!return_type2_as_comp_type && !return_type1_as_comp_type) {
        /* Neither given type is the composite type of the given types.
           Avoid further checking a create a new type. */
        goto make_new_comp_type;
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
    rtsp->trailing_return_type = comp_trailing_return_type;
    rtsp->does_not_return = comp_does_not_return;
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
        if (ptp1->attributes != NULL || ptp2->attributes != NULL) {
          new_ptp->attributes =
                     composite_attributes(ptp1->attributes, ptp2->attributes);
        }  /* if */
        if (!C_mode()) {
          /* Form the composite of the C++ default argument expressions; it's
             guaranteed that at most one of the parameter lists has a default
             argument expression.  The composite type shares its default
             argument expressions with one of the original types; if the
             original types are retained in the IL, that sharing should be
             undone (e.g., by calling disentangle_default_args). */
          if (ptp1->has_default_arg) {
            new_ptp->has_default_arg = TRUE;
            new_ptp->default_arg_appeared_in_class_definition =
                                ptp1->default_arg_appeared_in_class_definition;
            new_ptp->has_unevaluated_template_default =
                                        ptp1->has_unevaluated_template_default;
            new_ptp->orig_param_type_for_unevaluated_default_arg_expr =
                        ptp1->orig_param_type_for_unevaluated_default_arg_expr;
            if (ptp1->default_arg_expr != NULL) {
              new_ptp->default_arg_expr = ptp1->default_arg_expr;
              new_ptp->entities_defined_in_default_arg =
                                         ptp1->entities_defined_in_default_arg;
            }  /* if */
          } else if (ptp2->has_default_arg) {
            new_ptp->has_default_arg = TRUE;
            new_ptp->default_arg_appeared_in_class_definition =
                                ptp2->default_arg_appeared_in_class_definition;
            new_ptp->has_unevaluated_template_default =
                                        ptp2->has_unevaluated_template_default;
            new_ptp->orig_param_type_for_unevaluated_default_arg_expr =
                        ptp2->orig_param_type_for_unevaluated_default_arg_expr;
            if (ptp2->default_arg_expr != NULL) {
              new_ptp->default_arg_expr = ptp2->default_arg_expr;
              new_ptp->entities_defined_in_default_arg =
                                         ptp2->entities_defined_in_default_arg;
            }  /* if */
          }  /* if */
          if (ptp1->type_involves_deduced_template_param) {
            check_assertion(ptp2->type_involves_deduced_template_param);
            new_ptp->type_involves_deduced_template_param = TRUE;
          }  /* if */
          if (ptp1->type_involves_template_param) {
            check_assertion(ptp2->type_involves_template_param);
            new_ptp->type_involves_template_param = TRUE;
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
          /* coverity[var_deref_op] */
          end_of_list->next = new_ptp;
        }  /* if */
        end_of_list = new_ptp;
        /* Advance to the next param-type entries.  Both lists will be of
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
    if (ms_extensions) {
      rtsp->calling_convention = comp_calling_convention;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!C_mode()) {
      /* Set the implicit-this-parameter type. */
      rtsp->this_class = rtsp1->this_class;
      rtsp->qualifiers = rtsp1->qualifiers;
      rtsp->this_qualifiers = rtsp1->this_qualifiers;
      rtsp->ref_qualifiers = rtsp1->ref_qualifiers;
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
If the types passed in are routine types with associated default argument
expressions, a new type may be returned that shares those default argument
expressions.  This is usually okay because the original types are typically
discarded in such cases, but if any of the original types are retained, the
default argument expressions should be made unique to each type (e.g., by
calling disentangle_default_args).
*/
{
  a_type_ptr comp_type = NULL, comp_elem;
  a_type_ptr base_type_1, base_type_2;
  a_type_ptr member_type_1, member_type_2;

  db_enter(5, "composite_type");
  if (same_entities(type_1, type_2)) {
    /* If the types are identical (the most common case), the composite type
       is the same thing. */
    comp_type = type_1;
  } else if (deduced_return_types_enabled &&
             is_placeholder_deduction_match(type_1, type_2)) {
    /* A placeholder type (type_2) and its deduced counterpart (type_1): The
       composite is the deduced version. */
    comp_type = type_1;
  } else if (deduced_return_types_enabled &&
             is_placeholder_deduction_match(type_2, type_1)) {
    /* Same as the previous situation, but with reversed types. */
    comp_type = type_2;
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
#if FIXED_POINT_ALLOWED
        case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
        case tk_complex:
        case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        case tk_float:
        case tk_class:
        case tk_struct:
        case tk_union:
#if GNU_VECTOR_TYPES_ALLOWED
        case tk_vector:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        case tk_nullptr:
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
          { a_pointer_modifier_set  modifiers = PM_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            modifiers = base_type_1->variant.pointer.modifiers;
            check_assertion(equiv_pointer_modifiers(
                                     modifiers,
                                     base_type_2->variant.pointer.modifiers));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
                comp_type = make_reference_type_of_same_kind(comp_elem,
                                                             base_type_1);
	      } else {
                comp_type = make_pointer_type_full(comp_elem, modifiers);
              }  /* if */
            }  /* if */
          }
          break;
        case tk_array:
          comp_type = composite_array_type(base_type_1, base_type_2);
          break;
        case tk_routine:
          /* Function types. */
          comp_type = composite_routine_type(base_type_1, base_type_2);
          break;
        case tk_ptr_to_member:
          { a_pointer_modifier_set  modifiers = PM_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            modifiers = base_type_1->variant.ptr_to_member.modifiers;
            check_assertion(equiv_pointer_modifiers(
                               base_type_1->variant.ptr_to_member.modifiers,
                               base_type_2->variant.ptr_to_member.modifiers));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
              comp_type = ptr_to_member_type_full(comp_elem,
                                                  pm_class_type(base_type_1),
                                                  modifiers);
            }  /* if */
          }  /* if */
          break;
        /* Typerefs were removed above, and therefore shouldn't occur. */
        case tk_typeref:
        default:
          unexpected_condition_str("composite_type: bad type kind");
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
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && skip_typerefs(type_1)->kind == skip_typerefs(type_2)->kind) {
    comp_type = copy_gnu_type_properties(comp_type, type_1);
    comp_type = copy_gnu_type_properties(comp_type, type_2);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  db_exit();
  return comp_type;
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
  a_ref_qualifier_kind
                   old_ref_qualifiers, new_ref_qualifiers;
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
  new_ref_qualifiers = new_extra_info->ref_qualifiers;
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
    /* Note that the code here must match determine_arg_match_level. */
    old_this_class = old_extra_info->this_class;
    old_ref_qualifiers = old_extra_info->ref_qualifiers;
    if (old_ref_qualifiers != new_ref_qualifiers) {
      if (old_ref_qualifiers != (a_ref_qualifier_kind)rqk_default &&
          new_ref_qualifiers != (a_ref_qualifier_kind)rqk_default) {
        /* "&" and "&&" ref-qualifiers are distinguishable, but other
           combinations are not.  In particular, if two declarations only
           differ in ref-qualifiers and one declaration has no explicit
           ref-qualifier, the declarations are not overload-distinguishable
           (and presumably an error will be issued since they aren't
           compatible either). */
        distinguishable = TRUE;
        goto distinguishable_determined;
      }  /* if */
    } else {
      /* See if the "this" parameter is distinguishable if it exists.  If one
         function has a "this" parameter and the other does not, they are not
         distinguishable on that basis alone (except in cfront compatibility
         mode, when a type qualifier on the "this" parameter type makes a
         nonstatic function distinguishable from a static function). */
      /* This is in the "else" branch of the ref-qualifiers test because if
         ref-qualifiers differ and one case has no ref-qualifiers at all, the
         cv-qualifiers are not a distinguishing factor (but parameter type
         differences test below are). */
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
            (old_param->type_involves_template_param !=
                           new_param->type_involves_template_param) ||
            (old_param->is_parameter_pack !=
                           new_param->is_parameter_pack) ||
            !f_types_are_compatible(old_param->type, new_param->type,
                                    TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED)) {
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
    } else if (old_ref_qualifiers != new_ref_qualifiers) {
      /* Two member functions with the same name and parameter types must
         either both have ref-qualifiers or both lack ref-qualifiers. */
      *err_code = ec_same_param_types_with_and_without_ref_qualifiers;
    } else {
      /* The parameter types are compatible, so the only incompatibility
         remaining must have to do with the return types. */
#if CHECKING
      if (f_types_are_compatible(old_type->variant.routine.return_type,
                                 new_type->variant.routine.return_type,
                                 TCF_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED)) {
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
typedefs are skipped, as they are in name mangling; it is the underlying
type, not the typedef name (which can be declared anywhere) that we really
care about.
*/
{
  a_boolean  is_local = FALSE;

  if (type_ptr->source_corresp.is_local_to_function) {
    check_assertion(type_ptr->kind != (a_type_kind)tk_typeref);
    *force_end_of_traversal = is_local = TRUE;
  } else if (vla_enabled && is_array(type_ptr) && array_is_vla(type_ptr)) {
    /* VLA types are considered to be local types. */
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
  if (is_namespace_member(type_ptr)) {
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
   is_invalid_template_arg_type. */
static a_boolean is_unnamed_type;
static a_boolean is_local_type;
static a_boolean treat_class_members_as_named;


static a_boolean ttt_is_type_with_no_name_linkage(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is a class type or
enumeration type with no name linkage (i.e., with no name and no name "for
linkage purposes" imbued by a typedef).  Nonreal class types are treated as
having name linkage.  '*force_end_of_traversal' can be set to true if the
result of the traversal is decided (in this case, when a type with no name
linkage is encountered).
*/
{
  a_boolean  result = FALSE;
  a_boolean  is_gpp_unnamed_case = FALSE;

  if (((is_class_struct_union(type_ptr) &&
        !type_ptr->variant.class_struct_union.is_nonreal_class) ||
       is_enum(type_ptr)) &&
      type_ptr->source_corresp.name_linkage == (a_name_linkage_kind)nlk_none) {
    /* treat_class_members_as_named is TRUE in certain cases in g++ mode
       where unnamed class members should be considered named.  For more
       details on when it is set see is_invalid_template_arg_type. */
    if (type_ptr->source_corresp.name == NULL &&
        treat_class_members_as_named &&
        type_ptr->source_corresp.is_class_member) {
      is_gpp_unnamed_case = TRUE;
    } else {
      *force_end_of_traversal = result = TRUE;
    }  /* if */
    if (type_ptr->source_corresp.is_local_to_function) {
      check_assertion(type_ptr->kind != (a_type_kind)tk_typeref);
      is_local_type = TRUE;
    }  /* if */
    /* If we decided to treat this as named above, don't set the unnamed
       flag here. */
    if (type_ptr->source_corresp.name == NULL && !is_gpp_unnamed_case) {
      is_unnamed_type = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* ttt_is_type_with_no_name_linkage */


static a_boolean ttt_is_trans_unit_specific_type(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It is a wrapper routine that passes type_ptr
and force_end_of_traversal to ttt_is_type_with_no_name_linkage and
ttt_is_unnamed_namespace_type.
*/
{
  a_boolean  result = FALSE;

  result = ttt_is_type_with_no_name_linkage(type_ptr, force_end_of_traversal);
  if (!result && !*force_end_of_traversal) {
    result = ttt_is_unnamed_namespace_type(type_ptr, force_end_of_traversal);
  }  /* if */
  return result;
}  /* ttt_is_trans_unit_specific_type */


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

/* TRUE if instantiation dependence is being tested. */
static a_boolean
		check_for_instantiation_dependence;


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

  /* For a cast, use the constant under the cast. */
  if (cp->kind == (a_constant_repr_kind)ck_template_param &&
      cp->variant.template_param.kind ==
                                   (a_template_param_constant_kind)tpck_cast) {
    cp = cp->variant.template_param.variant.constant;
  }  /* if */
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
                                           expr_node_from_tpck_expression(cp),
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
      if (type_ptr->variant.array.variant.element_count_constant != NULL) {
        found = constant_contains_template_param_constant(
                      type_ptr->variant.array.variant.element_count_constant);
      }  /* if */
    }  /* if */
  } else if (is_class_struct_union(type_ptr)) {
    /* Examine each template argument, if any. */
    begin_template_arg_list_traversal_simple(
            type_ptr->variant.class_struct_union.extra_info->template_arg_list,
            &tap);
    for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
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
    begin_template_arg_list_traversal_simple(
            type_ptr->variant.class_struct_union.extra_info->template_arg_list,
            &tap);
    for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
      if (is_template_templ_arg(tap)) {
        a_template_symbol_supplement_ptr	tssp;
        /* Note: the following line accesses template_info directly instead
           of calling template_supplement_for_template(), as would be
           normal practice.  The reason for this is that this code may be
           executed from a back end, and template_supplement_for_template()
           sometimes uses symbol table information, which is only available
           in the front end. */
        tssp = tap->variant.templ.ptr->template_info;
        /* Determine whether the template pointed to is a template template
           parameter. */
        if (tssp != NULL &&
            tssp->variant.class_template.template_template_param) {
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

  if (check_for_instantiation_dependence &&
      type_ptr->is_instantiation_dependent_cached) {
    /* This type was tested for instantiation dependence before: Use the
       cached outcome. */
    found = type_ptr->is_instantiation_dependent;
    *force_end_of_traversal = TRUE;
  } else {
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
    } else if (find_all_dependent_types &&
               is_immediate_enum_type(type_ptr) &&
               type_ptr->variant.integer.is_nonreal) {
      /* A nonreal enumeration type is a dependent type. */
      *force_end_of_traversal = found = TRUE;
    } else if (find_all_dependent_types &&
               type_ptr->kind == (a_type_kind)tk_typeref &&
               type_ptr->variant.typeref.is_dependent_type_operator) {
      /* A dependent decltype or typeof. */
      *force_end_of_traversal = found = TRUE;
    } else if (find_all_dependent_types &&
               type_ptr->kind == (a_type_kind)tk_typeref &&
               typeref_is_type_operator(type_ptr)) {
      /* A nondependent decltype or typeof. */
      check_assertion(!type_ptr->variant.typeref.is_dependent_type_operator);
      *force_end_of_traversal = TRUE;
      found = FALSE;
    } else if (find_all_dependent_types &&
               type_ptr->kind == (a_type_kind)tk_array &&
               type_ptr->variant.array.is_template_dependent_size_array) {
      /* A dependent array. */
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
    if (check_for_instantiation_dependence && found) {
      type_ptr->is_instantiation_dependent_cached = TRUE;
      type_ptr->is_instantiation_dependent = TRUE;
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
    if (equiv_templates(templ_ptr, specific_template_template_param,
                        ET_NO_OPTIONS)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  if (!found) {
    if (is_class_struct_union(type_ptr)) {
      /* Check for template template arguments of a template class. */
      a_template_arg_ptr  tap;
      begin_template_arg_list_traversal_simple(
            type_ptr->variant.class_struct_union.extra_info->template_arg_list,
            &tap);
      for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
        if (is_template_templ_arg(tap)) {
          if (equiv_templates(tap->variant.templ.ptr,
                              specific_template_template_param,
                              ET_NO_OPTIONS)) {
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
  } else if (type_ptr->kind == (a_type_kind)tk_typeref &&
             typeref_is_type_operator(type_ptr)) {
    /* The type under a decltype or typeof is not deduced. */
    *force_end_of_traversal = TRUE;
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
    type_ptr = parent_class_of(type_ptr);
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
    type_ptr = parent_class_of(type_ptr);
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
        type_ptr = parent_class_of(type_ptr);
      }  /* while */
      sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
      sym->force_external_linkage = TRUE;
    }  /* if */
  }  /* if */
  *force_end_of_traversal = FALSE;
  return FALSE;
}  /* ttt_set_force_external_linkage_flag */


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

#if !STANDALONE_UTILITY_PROGRAM

static a_boolean ttt_is_nonlocal_variably_modified_type(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
Return TRUE if type_ptr is a VLA not associated with the innermost function
scope (if there is no such scope, any VLA type is a "nonlocal variably
modified type").
*/
{
  a_boolean  found = FALSE;

  if (is_array(type_ptr) && array_is_vla(type_ptr) &&
      find_vla_dimension_in_current_function(type_ptr) == NULL) {
    *force_end_of_traversal = found = TRUE;
  }  /* if */
  return found;
}  /* ttt_is_nonlocal_variably_modified_type */    


static a_boolean ttt_type_has_side_effects(a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
Return TRUE if type_ptr is a VLA type with a dimension expression that has a
side effect.
*/
{
  a_boolean  found = FALSE;

  if (is_array(type_ptr) && type_ptr->variant.array.has_assoc_vla_dimension) {
    a_vla_dimension_ptr  vla_dim = find_vla_dimension(type_ptr);
    if (vla_dim->dimension_expr != NULL &&
        node_has_side_effects(vla_dim->dimension_expr, (a_boolean*)NULL)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  }  /* if */
  return found;
}  /* ttt_type_has_side_effects */    

#if DO_IL_LOWERING

/*ARGSUSED*/  /* force_end_of_traversal is not used (but part of the
                 interface). */
static a_boolean ttt_lower_vla_dimensions(a_type_ptr  type_ptr,
                                          a_boolean   *force_end_of_traversal)
/*
If type_ptr is a VLA type, lower its dimension expression.
*/
{
  a_boolean  found = FALSE;

  if (is_array(type_ptr) && type_ptr->variant.array.has_assoc_vla_dimension) {
    lower_vla_dimension_expression(find_vla_dimension(type_ptr));
  }  /* if */
  return found;
}  /* ttt_lower_vla_dimensions */ 

#endif /* DO_IL_LOWERING */

static a_boolean ttt_warn_about_use_of_deprecated_type(
                                          a_type_ptr  type_ptr,
                                          a_boolean   *force_end_of_traversal)
/*
Return TRUE if the given type was marked as deprecated (either using a GNU
attribute or using a Microsoft declspec specifier).  Also set
*force_end_of_traversal to TRUE in that case, and issue a warning.
*/
{
  a_boolean  found = FALSE;

  if (type_ptr->source_corresp.is_deprecated) {
    *force_end_of_traversal = found = TRUE;
    check_use_of_deprecated_entity(&type_ptr->source_corresp, &error_position);
  } else if ((type_ptr->kind == (a_type_kind)tk_typeref &&
              (typeref_is_typedef(type_ptr) ||
               typeref_is_type_operator(type_ptr))) ||
             is_template_param_or_nonreal_class_type(type_ptr)) {
    /* Stop at typedefs, since those are declared using a distinct
       declaration.  Also stop at nonreal types since their deprecation status
       isn't known.  Also stop at type operators: Any deprecation issues for
       those should be reported on the underlying expression instead. */
    *force_end_of_traversal = TRUE;
  }  /* if */
  return found;
}  /* ttt_warn_about_use_of_deprecated_type */


void warn_about_use_of_deprecated_type(a_type_ptr         type,
                                       a_source_position  *pos)
/*
Warn if the given type has a component that has been marked as deprecated.
Components under typedefs are not considered.  The warning is issued for
the given position.
*/
{
  a_source_position  saved_pos;

  saved_pos = error_position;
  error_position = *pos;
  (void)traverse_type_tree(type, ttt_warn_about_use_of_deprecated_type,
                           TTT_RETURN_TYPE | TTT_PARAM_TYPES |
                           TTT_EXCEPTION_SPECS | TTT_TEMPLATE_ARGS |
                           TTT_PARENT_CLASSES |
                           TTT_DECLTYPE_AND_TYPEOF_EXPRS);
  error_position = saved_pos;
}  /* warn_about_use_of_deprecated_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */


static void invoke_type_predicate_for_type(
                                 a_type_ptr                          type,
                                 an_expr_or_stmt_traversal_block_ptr tblock)
/*
Called from traverse_expr to invoke a type predicate function on the
specified type.
*/
{
  if (traverse_type_tree(type, tblock->type_predicate_function,
                         tblock->type_tree_traversal_flags)) {
    tblock->result = TRUE;
    tblock->terminate = TRUE;
  }  /* if */
}  /* invoke_type_predicate_for_type */


static a_boolean traverse_types_for_expr(
				an_expr_node_ptr		expr,
				a_type_predicate_function_ptr	func,
				a_type_tree_traversal_flag_set	flags)
/*
Invoke the traverse_type_tree predicate function func on the types used in
expr.  Pass the flag set flags to traverse_type_tree.  Return TRUE if
traverse_type_tree returns TRUE.
*/
{
  a_boolean				result;
  an_expr_or_stmt_traversal_block	tblock;

  clear_expr_or_stmt_traversal_block(&tblock);
  tblock.process_type = invoke_type_predicate_for_type;
  tblock.process_non_dynamic_constants = TRUE;
  tblock.process_expressions_for_constants = TRUE;
  tblock.process_template_parameter_constants_and_expressions = TRUE;
  tblock.type_predicate_function = func;
  tblock.type_tree_traversal_flags = flags;
  traverse_expr(expr, &tblock);
  result = tblock.result;
  return result;
}  /* traverse_types_for_expr */


static a_boolean traverse_template_args(
                             a_template_arg_ptr             template_args,
                             a_type_predicate_function_ptr  func,
                             a_type_tree_traversal_flag_set flags)
/*
This routine is called by traverse_type_tree to traverse the template argument
list specified by template_args.  See traverse_type_tree for func, flags,
and the meaning of the return value.
*/
{
  a_template_arg_ptr	tap;
  a_boolean		status = FALSE;
  a_type_ptr		tp;

  begin_template_arg_list_traversal_simple(template_args, &tap);
  for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
    if (is_type_templ_arg(tap)) {
      tp = tap->variant.type;
      if (traverse_type_tree(tp, func, flags)) {
        status = TRUE;
        break;
      }  /* if */
    } else if (is_template_templ_arg(tap)) {
      /* Template template arguments are not themselves processed, but their
         parent type may be. */
      a_template_ptr	templ_ptr = tap->variant.templ.ptr;
      if (!status && templ_ptr->source_corresp.is_class_member &&
          (flags & TTT_PARENT_CLASSES) != 0) {
        /* Check the parent class.  This is only done when considering
           nondeduced contexts, or when this is a deduced context when
           nonstandard deduction is enabled. */
        tp = parent_class_of(templ_ptr);
        status = traverse_type_tree(tp, func, flags);
      }  /* if */      
    } else if (!tap->is_array_bound_of_unknown_type &&
               tap->variant.constant != NULL) {
      /* Nontype template argument.  Check the type of the constant. */
      if (!(flags & TTT_DEDUCED_CONTEXTS_ONLY)) {
        tp = tap->variant.constant->type;
        if (traverse_type_tree(tp, func, flags)) {
          status = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  return status;
}  /* traverse_template_args */


a_boolean traverse_type_tree(a_type_ptr                     type_ptr,
                             a_type_predicate_function_ptr  func,
                             a_type_tree_traversal_flag_set flags)
/*
Traverse the type tree indicated by type_ptr and for each type in the
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
  a_boolean                      status = FALSE;
  a_routine_type_supplement_ptr  rtsp;
  a_typeref_type_supplement_ptr  ttsp;

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
#if FIXED_POINT_ALLOWED
      case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
      case tk_float:
#if C99_IL_EXTENSIONS_SUPPORTED
      case tk_complex:
      case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tk_nullptr:
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
          }  /* for */
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
              rtsp->exception_specification != NULL &&
              !rtsp->exception_specification->is_noexcept) {
            an_exception_specification_type_ptr  estp;
            for (estp = rtsp->exception_specification->
                                 variant.exception_specification_type_list;
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
        ttsp = type_ptr->variant.typeref.extra_info;
        status = traverse_type_tree(tp, func, flags);
        if (!status && flags & TTT_DECLTYPE_AND_TYPEOF_EXPRS &&
            ttsp->expr != NULL) {
          /* Traverse the expression under the decltype or typeof. */
          status = traverse_types_for_expr(ttsp->expr, func, flags);
        }  /* if */
        if (!status && flags & TTT_TEMPLATE_ARGS) {
          /* Traverse the template argument list, if present (for template
             aliases). */
          a_template_arg_ptr	tap;
          tap = ttsp->template_arg_list;
          if (tap != NULL) {
            status = traverse_template_args(tap, func, flags);
          }  /* if */
        }  /* if */
	break;
      case tk_template_param:
        /* "Member" template params (e.g., T::X) should have a pointer to a
           parent class. */
        check_assertion((a_boolean)type_ptr->source_corresp.is_class_member ==
                        (type_ptr->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_member));
        if (type_ptr->source_corresp.is_class_member &&
            (flags & TTT_PARENT_CLASSES) != 0) {
          /* Check the template parameter associated with the proxy class that
             is the parent class.  This is only checked when considering
             nondeduced contexts, or when this is a deduced context when
             nonstandard deduction is enabled. */
          tp = parent_class_of(type_ptr);
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
#if GNU_VECTOR_TYPES_ALLOWED
      case tk_vector:
        tp = type_ptr->variant.vector.element_type;
        if (tp != NULL) status = traverse_type_tree(tp, func, flags);
        break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Any code for the general class/struct/union case should go
	   here. */
        if (!C_mode()) {
          /* If this class is a proxy class, traverse its associated
             template parameter.  If this is a C++/CLI constraint type,
             the associated generic parameter is only traversed if requested
             by the caller. */
          if (in_front_end &&
              (!is_cli_generic_constraint(type_ptr) ||
               (flags & TTT_CLI_GENERIC_PARAMETERS) != 0)) {
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
            /* Traverse the template argument list, if present. */
            a_template_arg_ptr	tap;
            tap = type_ptr->
                      variant.class_struct_union.extra_info->template_arg_list;
            if (tap != NULL) {
              status = traverse_template_args(tap, func, flags);
            }  /* if */
          }  /* if */
          if (!status && type_ptr->source_corresp.is_class_member &&
              in_front_end) {
            /* If this class is a member of a proxy class, traverse the type
               of the template parameter with which the proxy class is
               associated. */
            tp = parent_class_of(type_ptr);
            check_assertion(tp->source_corresp.assoc_info != NULL);
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
          if (!status && type_ptr->source_corresp.is_class_member &&
              (flags & TTT_PARENT_CLASSES) != 0) {
            /* Check the parent class. */
            tp = parent_class_of(type_ptr);
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
      default:
        unexpected_condition_str("traverse_type_tree: bad type kind");
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
                                               TTT_TEMPLATE_ARGS |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_EXCEPTION_SPECS |
                                               TTT_PARENT_CLASSES);

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
                                               TTT_TEMPLATE_ARGS |
                                               TTT_SKIP_TYPEREFS |
                                               TTT_EXCEPTION_SPECS |
                                               TTT_PARENT_CLASSES);

  result = (traverse_type_tree(type_ptr, ttt_is_unnamed_namespace_type,
                                ttt_flags));
  return result;
}  /* is_or_contains_unnamed_namespace_type */


a_boolean is_or_contains_type_with_no_name_linkage(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr contains a class, struct,
union or enum type with no name linkage.
This function considers nonreal class and enum types to have linkage.
*/
{
  a_boolean			  result;
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_EXCEPTION_SPECS |
                                               TTT_SKIP_TYPEREFS);

  /* Clear the variables that are used to return status information
     from ttt_is_type_with_no_name_linkage. */
  is_local_type = FALSE;
  is_unnamed_type = FALSE;
  treat_class_members_as_named = FALSE;
  result = (traverse_type_tree(type_ptr, ttt_is_type_with_no_name_linkage,
                               ttt_flags));
  return result;
}  /* is_or_contains_type_with_no_name_linkage */


a_boolean is_or_contains_trans_unit_specific_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr contains a local type,
unnamed type, or type defined in an unnamed namespace.
*/
{
  a_boolean			  result;
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                                 TTT_THIS_PARAM_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_TEMPLATE_ARGS |
                                                 TTT_PARENT_CLASSES);

  /* Clear the variables that are used to return status information
     from ttt_is_type_with_no_name_linkage, which is called by
     ttt_is_trans_unit_specific_type. */
  is_local_type = FALSE;
  is_unnamed_type = FALSE;
  treat_class_members_as_named = FALSE;
  result = (traverse_type_tree(type_ptr, ttt_is_trans_unit_specific_type,
                               ttt_flags));
  return result;
}  /* is_or_contains_trans_unit_specific_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean ttt_is_or_contains_cli_generic_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  Return TRUE if type_ptr is a template parameter
type for a C++/CLI generic type parameter.
*/
{
  a_boolean  found = FALSE;

  if (is_cli_generic_param(type_ptr)) {
    *force_end_of_traversal = found = TRUE;
  }  /* if */
  return found;
}  /* ttt_is_or_contains_cli_generic_param */


a_boolean is_or_contains_cli_generic_param(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a tk_template_param
for a C++/CLI generic type parameter or is a type tree containing such a
type.
*/
{
  a_boolean result;
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_CLI_GENERIC_PARAMETERS |
                                               TTT_PARENT_CLASSES);

  result = traverse_type_tree(type_ptr, ttt_is_or_contains_cli_generic_param,
                              ttt_flags);
  return result;
}  /* is_or_contains_cli_generic_param */


static a_boolean ttt_has_clr_component(a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  Return TRUE if a function type containing the
type described by type_ptr in its signature requires the __clrcall calling
convention.
*/
{
  a_boolean  found = FALSE;

  /* Handles (but not tracking references), managed class types, and generic
     parameters appearing in function signatures cause the associated function
     type to have the __clrcall calling convention. */
  if (is_handle_ptr(type_ptr) ||
      is_immediate_managed_class_type(type_ptr) ||
      is_cli_generic_param(type_ptr)) {
    *force_end_of_traversal = found = TRUE;
  }  /* if */
  return found;
}  /* ttt_has_clr_component */


a_boolean function_type_has_clrcall_component(a_type_ptr  type_ptr)
/*
Return TRUE if the routine type pointed to by type_ptr has a component that
implies the __clrcall calling convention (e.g., a parameter that is a handle
type).
*/
{
  a_boolean result;
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_CLI_GENERIC_PARAMETERS);

  check_assertion(type_ptr->kind == (a_type_kind)tk_routine);
  result = traverse_type_tree(type_ptr, ttt_has_clr_component, ttt_flags);
  return result;
}  /* function_type_has_clrcall_component */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean is_invalid_template_arg_type(a_type_ptr  type_ptr,
                                       a_boolean   *is_unnamed,
                                       a_boolean   *is_local,
                                       a_boolean   *is_vla,
                                       a_boolean   *is_generic)
/*
Return TRUE if the type pointed to by type_ptr contains a class, struct,
union or enum type that cannot be part of a template argument type.  In
C++98/C++03 (but not C++11) this excludes class/enum types with no name
linkage.  The variable local_types_as_template_args_enabled is TRUE
in C++11 and some Microsoft modes to allow local and unnamed types
to be used as template arguments.  If the result is TRUE then
*is_unnamed or *is_local are set when the type traversal encountered a
component that is, respectively, unnamed or local (since the traversal
stops early, another component with a different property may also keep
the type from having linkage without it being reflected in the values
returned).  GNU C++ mode allows variable-length array types, but they
are not valid template argument types: If one is encountered, FALSE is
returned and *is_vla is set to TRUE.  In C++/CLI mode, *is_generic is
set to TRUE if the type contains a generic type parameter.  This routine also
sets the value of local_type_used_as_template_type_argument when needed.
*/
{
  a_boolean	result = FALSE;
  a_boolean	no_linkage = FALSE;
  a_boolean	local_type_check_needed;

  local_type_check_needed = !local_types_as_template_args_enabled;
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
  if (!local_type_check_needed) {
    /* When ENSURE_LOWERED_TYPE_LIST_ORDERING is TRUE, we need to record
       whether a local type was ever used as a nontype template argument.
       Once the first one is found, we don't need to check further when
       local types are allowed as template arguments. */
    local_type_check_needed = !local_type_used_as_template_type_argument;
  }  /* if */
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
  /* Clear the variables that are used to return status information from
     ttt_is_type_with_no_name_linkage. */
  *is_local = is_local_type = FALSE;
  *is_unnamed = is_unnamed_type = FALSE;
  *is_generic = FALSE;
  /* g++ treats unnamed class members as named starting with version 4.5. */
  treat_class_members_as_named = gpp_mode && gnu_version >= 40500;
  if (local_type_check_needed) {
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                                 TTT_THIS_PARAM_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_EXCEPTION_SPECS |
                                                 TTT_SKIP_TYPEREFS);

    no_linkage = traverse_type_tree(type_ptr, ttt_is_type_with_no_name_linkage,
                                    ttt_flags);
#if ENSURE_LOWERED_TYPE_LIST_ORDERING
    if (is_local_type) {
      local_type_used_as_template_type_argument = TRUE;
    }  /* if */
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */
  }  /* if */
  if (!local_types_as_template_args_enabled) {
    /* If local and unnamed types are not allowed as template arguments, return
       the flags set above to the caller. */
    result = no_linkage;
    *is_unnamed = is_unnamed_type;
    *is_local = is_local_type;
  }  /* if */
  /* If the type is otherwise valid, check for a VLA type. */
  if (!result && il_header.vla_used) {
    result = *is_vla = is_variably_modified_type(type_ptr);
  } else {
    *is_vla = FALSE;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* In C++/CLI mode, if the type is otherwise valid, check for a type
     containing a generic type parameter. */
  if (cli_or_cx_enabled && !result) {
    result = *is_generic = is_or_contains_cli_generic_param(type_ptr);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* is_invalid_template_arg_type */


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
                                               TTT_EXCEPTION_SPECS |
                                               TTT_PARENT_CLASSES |
                                               TTT_DECLTYPE_AND_TYPEOF_EXPRS);
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
                                                 TTT_TEMPLATE_ARGS |
                                                 TTT_SKIP_TYPEREFS |
                                                 TTT_PARENT_CLASSES);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = TRUE;
    check_for_instantiation_dependence = FALSE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
  }  /* if */
  return result;
}  /* is_template_dependent_type */


a_boolean is_template_dependent_type_or_cli_generic_param(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is template-dependent, i.e.,
it is or contains a tk_template_param type entry or a nonreal class.  Also
returns TRUE for types that contain C++/CLI generic parameters.
*/
{
  a_boolean result = FALSE;

  /* Template parameter types come up only in C++ mode. */
  if (!C_mode()) {
    a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                                 TTT_THIS_PARAM_TYPE |
                                                 TTT_PARAM_TYPES |
                                                 TTT_TEMPLATE_ARGS |
                                                 TTT_SKIP_TYPEREFS |
                                                 TTT_CLI_GENERIC_PARAMETERS |
                                                 TTT_PARENT_CLASSES);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = TRUE;
    check_for_instantiation_dependence = FALSE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
  }  /* if */
  return result;
}  /* is_template_dependent_type_or_cli_generic_param */


a_boolean is_instantiation_dependent_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is instantiation-dependent,
i.e., it contains a template parameter, even in a context that does not
render the type dependent.
*/
{
  a_boolean result;

  /* Template parameter types come up only in C++ mode. */
  if (C_mode()) {
    result = FALSE;
  } else if (type_ptr->is_instantiation_dependent_cached) {
    result = type_ptr->is_instantiation_dependent;
  } else {
    a_type_tree_traversal_flag_set  ttt_flags =
			 (TTT_RETURN_TYPE |
                          TTT_THIS_PARAM_TYPE |
                          TTT_PARAM_TYPES |
                          TTT_TEMPLATE_ARGS |
                          TTT_DECLTYPE_AND_TYPEOF_EXPRS |
                          TTT_PARENT_CLASSES);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = TRUE;
    check_for_instantiation_dependence = TRUE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
    type_ptr->is_instantiation_dependent = result;
    type_ptr->is_instantiation_dependent_cached = TRUE;
  }  /* if */
  return result;
}  /* is_instantiation_dependent_type */


a_boolean is_instantiation_dependent_type_or_cli_generic_param(
							a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is instantiation-dependent,
i.e., it contains a template parameter, even in a context that does not
render the type dependent.  Also returns TRUE for types that contain
C++/CLI generic parameters.
*/
{
  a_boolean result;

  /* Template parameter types come up only in C++ mode. */
  if (C_mode()) {
    result = FALSE;
  } else if (!cli_or_cx_enabled &&
             type_ptr->is_instantiation_dependent_cached) {
    result = type_ptr->is_instantiation_dependent;
  } else {
    a_type_tree_traversal_flag_set  ttt_flags =
			 (TTT_RETURN_TYPE |
                          TTT_THIS_PARAM_TYPE |
                          TTT_PARAM_TYPES |
                          TTT_TEMPLATE_ARGS |
                          TTT_DECLTYPE_AND_TYPEOF_EXPRS |
                          TTT_CLI_GENERIC_PARAMETERS |
                          TTT_PARENT_CLASSES);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = TRUE;
    check_for_instantiation_dependence = FALSE;
    result = traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
                                ttt_flags);
    if (!cli_or_cx_enabled) {
      type_ptr->is_instantiation_dependent = result;
      type_ptr->is_instantiation_dependent_cached = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_instantiation_dependent_type_or_cli_generic_param */


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
                                                 TTT_TEMPLATE_ARGS |
                                                 TTT_CLI_GENERIC_PARAMETERS |
                                                 TTT_PARENT_CLASSES);

    /* Setting these pointers to NULL indicates that any template param type
       or constant will do. */
    specific_template_param_type = NULL;
    specific_template_param_constant = NULL;
    deduced_contexts_only = FALSE;
    find_all_dependent_types = FALSE;
    check_for_instantiation_dependence = FALSE;
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
                                               TTT_SKIP_TYPEREFS |
					       TTT_DEDUCED_CONTEXTS_ONLY |
                                               TTT_CLI_GENERIC_PARAMETERS |
                                               TTT_TEMPLATE_ARGS);

  check_assertion_str(!C_mode(),
              "is_or_contains_deduced_template_param: not callable in C mode");
  /* Setting these pointers to NULL indicates that any template param type
     or constant will do. */
  specific_template_param_type = NULL;
  specific_template_param_constant = NULL;
  deduced_contexts_only = TRUE;
  find_all_dependent_types = FALSE;
  check_for_instantiation_dependence = FALSE;
  if (nonstandard_qualifier_deduction) {
    /* The template parameters of parent classes are normally not deduced, but
       in some modes a nonstandard deduction rule applies. */
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  return (traverse_type_tree(type_ptr,
                             ttt_is_or_contains_deduced_template_param,
                             ttt_flags));
}  /* is_or_contains_deduced_template_param */


void set_parameter_list_template_param_flags(a_type_ptr  rout_type)
/*
Go through the parameters for rout_type, which is assumed to be a
function type.  If any of its parameter types depends on a template
parameter, set a flag in the param type entry to indicate that.  In
addition, if any of the parameter types involves a template parameter
in a context in which the parameter can be deduced, set a second flag
to indicate that.  Nondeduced contexts are the parent classes of a
type (e.g., ignore the T in A<T>::B) and nontype template parameters
used in expression contexts.
*/
{
  a_param_type_ptr  ptp;

  check_assertion(is_function_type(rout_type));
  ptp = skip_typerefs(rout_type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    ptp->type_involves_template_param =
                    is_template_dependent_type_or_cli_generic_param(ptp->type);
    if (ptp->type_involves_template_param) {
      /* The type can only involve a deduced template parameter if it
         involves a template parameter in any context.  Parameter packs
         are nondeduced when they do not appear at the end of the parameter
         list. */
      ptp->type_involves_deduced_template_param =
                              !(ptp->is_parameter_pack && ptp->next != NULL) &&
                              is_or_contains_deduced_template_param(ptp->type);
    }  /* if */
  }  /* for */
}  /* set_parameter_list_template_param_flags */


a_boolean is_or_contains_specific_template_param(a_type_ptr  type_ptr,
						 a_type_ptr  tparam_type,
                                                 a_boolean   deduced_only)
/*
Return TRUE if the type pointed to by type_ptr is itself the specific
template parameter specified by tparam_type or is a type tree
containing such a reference to the type.  If deduced_only is TRUE, nondeduced
contexts are excluded from the check.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_CLI_GENERIC_PARAMETERS);
  
  /* When including non-deduced contexts, also include parent classes. */
  if (deduced_only) {
    ttt_flags |= TTT_DEDUCED_CONTEXTS_ONLY;
    ttt_flags |= TTT_SKIP_TYPEREFS;
  } else {
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  /* This indicates that only a specific template parameter may be found. */
  specific_template_param_type = tparam_type;
  specific_template_param_constant = NULL;
  deduced_contexts_only = deduced_only;
  find_all_dependent_types = FALSE;
  check_for_instantiation_dependence = FALSE;
  if (nonstandard_qualifier_deduction) {
    /* The template parameters of parent classes are normally not deduced, but
       in some modes a nonstandard deduction rule applies. */
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  return (traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
          ttt_flags));
}  /* is_or_contains_specific_template_param */


a_boolean type_contains_specific_template_template_param(
					a_type_ptr	type_ptr,
					a_template_ptr	tparam_template,
					a_boolean	deduced_only)
/*
Return TRUE if the type tree pointed to by type_ptr contains a type
that is an instance of the template template parameter specified
by tparam_template.  If deduced_only is TRUE, nondeduced contexts are
excluded from the check.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  /* When including non-deduced contexts, also include parent classes. */
  if (deduced_only) {
    ttt_flags |= TTT_DEDUCED_CONTEXTS_ONLY;
  } else {
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  specific_template_template_param = tparam_template;
  if (nonstandard_qualifier_deduction) {
    /* The template parameters of parent classes are normally not deduced, but
       in some modes a nonstandard deduction rule applies. */
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  return (traverse_type_tree(type_ptr,
          ttt_contains_specific_template_template_param,
          ttt_flags));
}  /* type_contains_specific_template_template_param */


a_boolean type_contains_specific_template_param_constant(
						a_type_ptr	tp,
						a_constant_ptr	cp,
						a_boolean	deduced_only)
/*
Return TRUE if the template parameter constant pointed to by cp is involved
in the type tree represented by tp.  If deduced_only is TRUE, nondeduced
contexts are excluded from the check.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  /* When including non-deduced contexts, also include parent classes. */
  if (deduced_only) {
    ttt_flags |= TTT_DEDUCED_CONTEXTS_ONLY;
  } else {
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
  /* This indicates that only a specific template param constant may be
     found. */
  specific_template_param_constant = cp;
  specific_template_param_type = NULL;
  deduced_contexts_only = deduced_only;
  find_all_dependent_types = FALSE;
  check_for_instantiation_dependence = FALSE;
  if (nonstandard_qualifier_deduction) {
    /* The template parameters of parent classes are normally not deduced, but
       in some modes a nonstandard deduction rule applies. */
    ttt_flags |= TTT_PARENT_CLASSES;
  }  /* if */
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
                                               TTT_TEMPLATE_ARGS |
                                               TTT_PARENT_CLASSES);

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

  begin_template_arg_list_traversal_simple(tap, &tap);
  for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
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
                                               TTT_TEMPLATE_ARGS |
                                               TTT_PARENT_CLASSES);
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
                                                 TTT_THIS_PARAM_TYPE |
                                                 TTT_PARENT_CLASSES);
    (void)traverse_type_tree(type_ptr, ttt_set_force_external_linkage_flag,
                             ttt_flags);
  }  /* if */
}  /* set_force_external_linkage_flag */

#if !DO_IL_LOWERING

static a_boolean may_have_decider_function(a_type_ptr class_type)
/*
Approximate whether the class might have a decider function.  This is used
when we don't have IL lowering, and therefore we can't know for sure
whether the class has a decider function or even whether that concept is
meaningful.
*/
{
  a_boolean has_decider = FALSE;
  a_boolean unknown;

  class_type = skip_typerefs(class_type);
  check_assertion(is_immediate_class_type(class_type));
  if (vtbl_decider_function_for_class(class_type, &unknown) != NULL ||
      unknown) {
    has_decider = TRUE;
  }  /* if */
  return has_decider;
}  /* may_have_decider_function */

#endif /* DO_IL_LOWERING */

void force_definition_of_typeinfo_for(a_type_ptr type)
/*
"type" is used in an exception or rtti context.  Do anything required to
make sure that the typeinfo for the type is defined (somewhere, not
necessarily in this translation unit), for example forcing the definition
of virtual functions if type is a class.
*/
{
  for (;;) {
    type = skip_typerefs(type);
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp = class_type_supp(type);
      a_base_class_ptr            bcp;
      a_boolean                   require_virtuals = FALSE;
      check_assertion(ctsp != NULL);
      /* Check if emitting the virtual table is determined by the decider
         function. */
      if (!type->variant.class_struct_union.is_template_class ||
          type->variant.class_struct_union.is_specialized) {
        /* If there is a decider function, it determines where the virtual
           function table and typeinfo structures are emitted. */
        a_routine_ptr decider = vtbl_decider_function_for_class(
                                                     type, (a_boolean *)NULL);
        if (decider != NULL && !routine_has_been_defined(decider)) {
          /* There is a decider function and it hasn't (yet) been defined.
             Don't emit the virtual function table or virtual functions right
             now. */
          break;
        }  /* if */
      }  /* if */
#if DO_IL_LOWERING
      /* If defining the typeinfo requires defining the vtable, force
         definition of the virtual functions to force the definition of the
         vtable. */
      { a_boolean unknown;
        if (typeinfo_goes_out_where_vtable_goes_out(type, &unknown) ||
            unknown) {
          require_virtuals = TRUE;
        }  /* if */
      }
#else /* !DO_IL_LOWERING */
      /* When we don't do IL lowering, just approximate whether virtual
         functions might have to be defined to get the right behavior. */
      if (!type->variant.class_struct_union.
                                        virtual_functions_marked_as_required &&
          may_have_decider_function(type)) {
        require_virtuals = TRUE;
      }  /* if */
#endif /* DO_IL_LOWERING */
      if (require_virtuals) {
        require_definitions_of_virtual_functions_in_class(type);
      }  /* if */
      /* Force typeinfos for the base classes. */
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        /* Handle only direct base classes, because the recursive call
           will handle that class's base classes. */
        if (bcp->direct) {
          force_definition_of_typeinfo_for(bcp->type);
        }  /* if */
      }  /* for */
      break;
    } else if (is_ptr_or_ref_type(type)) {
      type = type_pointed_to(type);
    } else if (is_ptr_to_member(type)) {
      force_definition_of_typeinfo_for(pm_class_type(type));
      type = pm_member_type(type);
    } else {
      /* Array types, function types, fundamental types, etc.  These don't
         need typeinfos for the underlying types, so stop here. */
      break;
    }  /* if */
  }  /* for */
}  /* force_definition_of_typeinfo_for */


void set_used_in_exception_or_rtti_flag(a_type_ptr  type_ptr)
/*
Set the used_in_exception_or_rtti flag in type_ptr to indicate that it
has been used in an exception handling or RTTI construct.
*/
{
  if (in_front_end &&
      (depth_scope_stack != NO_SCOPE_DEPTH &&
       is_template_dependent_context() &&
       !prototype_instantiations_in_il)) {
    /* We're currently inside a prototype instantiation, and we're
       not going to save it, so do not record the use. */
  } else if (type_ptr->used_in_exception_or_rtti) {
    /* Already set.  No further action is required. */
  } else {
    type_ptr->used_in_exception_or_rtti = TRUE;
    force_definition_of_typeinfo_for(type_ptr);
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


a_boolean is_or_contains_vla_type_with_unspecified_bound(a_type_ptr  tp)
/*
Return TRUE if tp is or contains a variable length array type with an
unspecified bound (i.e., declared with [*]).
*/
{
  a_boolean                       result = FALSE;

  if (il_header.vla_used) {
    a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                                TTT_SKIP_TYPEREFS);
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
typedefs referring to variably modified types.  Note that parameter types
are not considered to make a function type "variably modified" in C, and
that in C++ we do not allow VLAs in function signatures at all (and hence
neither parameter types nor exception specifications need to be searched
for VLAs).
*/
{
  a_boolean                       result = FALSE;

  if (il_header.vla_used) {
    a_type_tree_traversal_flag_set  tt_flags = TTT_RETURN_TYPE;
    result = traverse_type_tree(tp, ttt_is_variably_modified_type, tt_flags);
  }  /* if */
  return result;
}  /* is_variably_modified_type */


a_boolean is_nonlocal_variably_modified_type(a_type_ptr  tp)
/*
Return TRUE if tp is a "variably modified type" (which includes VLA types,
pointers to VLA types, arrays whose element types are variably modified, and
typedefs referring to variably modified types) with a VLA component that is
not associated with the current innermost function scope.  (This is only
relevant with local classes in C++ mode, which does not allow VLAs in parameter
types or exception specification types.)
*/
{
  a_boolean                       result = FALSE;

  if (il_header.vla_used && !C_mode()) {
    a_type_tree_traversal_flag_set  tt_flags = TTT_RETURN_TYPE;
    result = traverse_type_tree(tp, ttt_is_nonlocal_variably_modified_type,
                                tt_flags);
  }  /* if */
  return result;
}  /* is_nonlocal_variably_modified_type */


a_boolean type_has_side_effects(a_type_ptr  tp)
/*
Return TRUE if tp is a type which causes a side effect (e.g., when it appears
in a cast).  Currently, this is the case only for certain variably modified
types containing a VLA component with a dimension expression that has a side
effect.  Note that referring to a typedef whose underlying type has such side
effects does not itself create a side effect at the point of reference.
*/
{
  a_boolean                       result = FALSE;

  if (il_header.vla_used && innermost_function_scope != NULL) {
    a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                                TTT_STOP_AT_TYPEDEFS);
    if (C_mode()) {
      /* C++ modes do not allow VLAs in parameters, but C modes do. */
      tt_flags |= TTT_PARAM_TYPES;
    }  /* if */
    result = traverse_type_tree(tp, ttt_type_has_side_effects, tt_flags);
  }  /* if */
  return result;
}  /* type_has_side_effects */

#if DO_IL_LOWERING
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED

/*
A variable tracking the strictest ELF visibility during a type traversal with
ttt_ELF_visibility_of_type.
*/
static
	an_ELF_visibility_kind
		strictest_ELF_visibility_in_traversal;


static int ELF_visibility_strictness(an_ELF_visibility_kind evk)
/*
Map the given ELF visibility kind to a "strictness" (a higher value corresponds
to stricter/less visibility).
*/
{
  int result;

  switch (evk) {
    case evk_unspecified: result = 0; break;
    case evk_hidden:      result = 3; break;
    case evk_protected:   result = 2; break;
    case evk_internal:    result = 4; break;
    case evk_default:     result = 1; break;
    default:              unexpected_condition();
  }  /* switch */
  return result;
}  /* ELF_visibility_strictness */


/* ARGSUSED */  /* force_end_of_traversal is not used. */
static a_boolean ttt_check_ELF_visibility_of_type(
                                           a_type_ptr  type,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It always returns FALSE, but updates the file
scope variable strictest_ELF_visibility_in_traversal with the ELF visibility
of the given type if that type is a class type with a stricter ELF visibility
than recorded so far.
*/
{
  if (is_immediate_class_type(type)) {
    an_ELF_visibility_kind  curr_visibility =
                                        class_type_supp(type)->ELF_visibility;
    if (ELF_visibility_strictness(curr_visibility) >
           ELF_visibility_strictness(strictest_ELF_visibility_in_traversal)) {
      strictest_ELF_visibility_in_traversal = curr_visibility;
    }  /* if */
  }  /* if */
  return FALSE;
}  /* ttt_check_ELF_visibility_of_type */


an_ELF_visibility_kind ELF_visibility_of_type(a_type_ptr  type)
/*
Return the ELF visibility of the given type.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS |
                                               TTT_SKIP_TYPEREFS);
  strictest_ELF_visibility_in_traversal =
                                      (an_ELF_visibility_kind)evk_unspecified;
  check_assertion(!C_mode()); 
  (void)traverse_type_tree(type, ttt_check_ELF_visibility_of_type, ttt_flags);
  return strictest_ELF_visibility_in_traversal;
}  /* ELF_visibility_of_type */

#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

void lower_vla_dimensions_in_type(a_type_ptr  tp)
/*
Lower the dimension expressions of any VLA type component in tp.
*/
{
  if (il_header.vla_used && innermost_function_scope != NULL) {
    a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                                TTT_STOP_AT_TYPEDEFS);
    if (C_mode()) {
      /* C++ modes do not allow VLAs in parameters, but C modes do. */
      tt_flags |= TTT_PARAM_TYPES;
    }  /* if */
    (void)traverse_type_tree(tp, ttt_lower_vla_dimensions, tt_flags);
  }  /* if */
}  /* lower_vla_dimensions_in_type */ 

#endif /* DO_IL_LOWERING */

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
         (is_immediate_class_type(tp) &&
          tp->variant.class_struct_union.is_nonreal_class);
}  /* could_be_dependent_class_type */

#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_overloadable_type(a_type_ptr type)
/*
Return TRUE if the given type is one for which operator overloading
should be considered, i.e., a class or enum type or something that
could be such a type (an error type or a template parameter type).
*/
{
  a_boolean is_overloadable;

  type = skip_typerefs(type);
  is_overloadable = (is_error(type) ||
                     is_class_struct_union(type) ||
                     (operator_overloading_on_enums_enabled &&
                      is_enum(type)) ||
                     is_template_param(type));
  return is_overloadable;
}  /* is_overloadable_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_overloadable_handle_type(a_type_ptr type)
/*
C++/CLI treats a handle as the first operand of an operator as similar to
a class object with regard to overloading.  Return TRUE if the given type
(the type of a first operand) is a handle that can be so used.
*/
{
  a_boolean is_overloadable_handle = FALSE;

  if (is_handle_type(type) &&
      is_class_struct_union_type(type_pointed_to(type))) {
    is_overloadable_handle = TRUE;
  }  /* if */
  return is_overloadable_handle;
}  /* is_overloadable_handle_type */
  
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean is_potential_conv_function_source(a_type_ptr type)
/*
Return TRUE if the given type is one for which conversion functions
may be available to convert to another type.  Usually, that's just
class types, but in C++/CLI mode it also includes handle-to-class
types, since static conversion functions can convert from a handle
type.
*/
{
  a_boolean is_potential_source = FALSE;

  if (is_class_struct_union_type(type)) {
    is_potential_source = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && is_overloadable_handle_type(type)) {
    is_potential_source = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return is_potential_source;
}  /* is_potential_conv_function_source */


a_boolean is_overloadable_first_operand_type(a_type_ptr type)
/*
Return TRUE if the given type is one for which operator overloading
should be considered, i.e., a class or enum type or something that
could be such a type (an error type or a template parameter type).
The operand in question is the first operand of an operation (C++/CLI
has some special rules for overloading on such operands).
*/
{
  a_boolean is_overloadable = is_overloadable_type(type);
#if MICROSOFT_EXTENSIONS_ALLOWED
  /* C++/CLI treats a first operand that is a handle similarly to
     an operand of the class under the handle. */
  if (cli_or_cx_enabled && !is_overloadable &&
      is_overloadable_handle_type(type)) {
    is_overloadable = TRUE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return is_overloadable;
}  /* is_overloadable_first_operand_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_directly_variably_modified_type(a_type_ptr  tp)
/*
Return TRUE if tp is a "variably modified type" in which the variable
array bound appears directly (rather than hidden under a typedef).
*/
{
  a_boolean                       result = FALSE;

  if (il_header.vla_used) {
    a_type_tree_traversal_flag_set  tt_flags = (TTT_RETURN_TYPE |
                                                TTT_STOP_AT_TYPEDEFS);
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


/*ARGSUSED*/ /* flags is not required but is part of the general interface. */
static a_boolean tmtt_strip_routine_default_args(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type)
/*
Modify type so that any routine types that it contains no longer have
any default arguments.  The modified type (or the original type if no
modification was done) is returned in *new_type.
*/
{
  *new_type = strip_routine_default_args(type);
  return !same_entities(type, *new_type);
}  /* tmtt_strip_routine_default_args */


/*ARGSUSED*/ /* flags is not required but is part of the general interface. */
static a_boolean tmtt_strip_qualifiers_from_param_types(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type)
/*
Modify type so that any routine types that it contains no longer have
qualifiers on their parameter types.  The modified type (or the original
type if no modification was done) is returned in *new_type.
*/
{
  *new_type = strip_qualifiers_from_param_types(type);
  return !same_entities(type, *new_type);
}  /* tmtt_strip_qualifiers_from_param_types */


static a_type_ptr traverse_and_modify_type_tree(
                                         a_type_ptr                     type,
                                         a_type_modifier_function_ptr   func,
                                         a_type_tree_traversal_flag_set flags)
/*
Traverse the type tree represented by type and at each level call func to
perform optional modification of the subtree.  If the subtree is modified,
a new tree is built.  Unlike with traverse_type_tree, the "func" routine
in this case implements the tree walk; it must call back into
traverse_and_modify_type_tree in some way to do that (usually, by
calling back to the non-"tmtt_" routine that started the walk).
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
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_nullptr:
    case tk_unknown:
      /* Leaf nodes -- no further traversal required. */
      break;
    case tk_pointer:
      { a_pointer_modifier_set  modifiers = PM_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        modifiers = type->variant.pointer.modifiers;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* If the type pointed to is modified, then a new pointer or
           reference type must be created. */
        if (func(type->variant.pointer.type, flags, &tp)) {
          if (type->variant.pointer.is_reference) {
            if (type->variant.pointer.is_rvalue_reference) {
              new_type = make_rvalue_reference_type(tp);
            } else {
              new_type = make_reference_type(tp);
            }  /* if */
          } else {
            new_type = make_pointer_type_full(tp, modifiers);
          }  /* if */
        }  /* if */
      }
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
          new_ptp->default_arg_appeared_in_class_definition =
                                 ptp->default_arg_appeared_in_class_definition;
          new_ptp->has_unevaluated_template_default =
                                         ptp->has_unevaluated_template_default;
          new_ptp->orig_param_type_for_unevaluated_default_arg_expr =
                         ptp->orig_param_type_for_unevaluated_default_arg_expr;
          if (ptp->default_arg_expr != NULL) {
            new_ptp->default_arg_expr =
                             duplicate_default_arg_expr(ptp->default_arg_expr);
          }  /* if */
        }  /* if */
        /* Recompute the value of the flag, if necessary. */
        if (same_entities(ptp->type, tp)) {
          new_ptp->type_involves_deduced_template_param =
                                     ptp->type_involves_deduced_template_param;
          new_ptp->type_involves_template_param =
                                             ptp->type_involves_template_param;
        } else {
          new_ptp->type_involves_deduced_template_param =
                          is_or_contains_deduced_template_param(new_ptp->type);
          new_ptp->type_involves_template_param =
                                  is_or_contains_template_param(new_ptp->type);
        }  /* if */
        new_ptp->is_parameter_pack = ptp->is_parameter_pack;
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
      set_clrcall_convention_if_needed(new_type);
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
      { a_pointer_modifier_set  modifiers = PM_NONE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        modifiers = type->variant.ptr_to_member.modifiers;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        (void)func(type->variant.ptr_to_member.type, flags, &tp);
        (void)func(type->variant.ptr_to_member.class_of_which_a_member, flags,
                   &tp2);
        if (!same_entities(tp, type->variant.ptr_to_member.type) ||
            !same_entities(
                   tp2, type->variant.ptr_to_member.class_of_which_a_member)) {
          /* Make a pointer-to-member type.  The current pointer-to-member type
             points to two types, so the new type is based on modified versions
             of one or both. */
          new_type = ptr_to_member_type_full(tp, tp2, modifiers);
        }  /* if */
      }
      break;
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      /* The vector case is similar to the array case. */
      if (func(type->variant.vector.element_type, flags, &tp)) {
        /* Create a new vector type. */
        new_type = alloc_type((a_type_kind)tk_vector);
        copy_type(type, new_type);
        new_type->variant.vector.element_type = tp;
      }  /* if */
      break;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    default:
      unexpected_condition_str("traverse_and_modify_type_tree: bad type kind");
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
  a_boolean  type_operator_stripped = FALSE;
  a_boolean  force_strip_nonreal = FALSE;

  /* If the underlying type is not dependent, make sure we strip off any
     nonreal typerefs. */
  if (type->kind == (a_type_kind)tk_typeref &&
      prototype_instantiations_in_il) {
    if (is_template_dependent_context() && !is_template_dependent_type(type)) {
      force_strip_nonreal = TRUE;
    }  /* if */
  }  /* if */
  while (type->kind == (a_type_kind)tk_typeref) {
    a_boolean  is_nonreal = FALSE;
    a_boolean  is_local;
    /* Don't strip dependent decltype or typeof operators.  We need to
       check the underlying type because these operators could be
       instantiation-dependent without being type-dependent. */
    if (type->variant.typeref.is_dependent_type_operator &&
        is_template_dependent_type(type->variant.typeref.type)) {
      break;
    }  /* if */
    /* See if the type is local to a function. */
    is_local = type->source_corresp.is_local_to_function;
    if (!prototype_instantiations_in_il || force_strip_nonreal) {
      /* Check for a nonreal template alias. */
      is_nonreal = type->variant.typeref.is_nonreal;
    }  /* if */
    if (!is_local && !is_nonreal &&
        (!prototype_instantiations_in_il || force_strip_nonreal)) {
      /* See if the type was defined in a prototype instantiation. */
      if (type->source_corresp.is_class_member) {
        a_symbol_ptr cowam_sym = symbol_for(parent_class_of(type));
        check_assertion(in_front_end && cowam_sym != NULL);
        is_nonreal = !is_real_class_symbol(cowam_sym);
      }  /* if */
    }  /* if */
    if (!is_local && !is_nonreal && typeref_is_type_operator(type)) {
      if ((type->variant.typeref.extra_info->expr == NULL ||
           type_operator_stripped) &&
#if GNU_EXTENSIONS_ALLOWED
           !type->variant.typeref.is_typeof_with_type_operand &&
#endif /* GNU_EXTENSIONS_ALLOWED */
           !is_or_contains_local_type(type->variant.typeref.type)) {
        /* This is a decltype or typeof applied to a local expression (which is
           indicated by the expr field being NULL, meaning the expression is
           stored elsewhere for memory region reasons).   If a type operator
           has been stripped already (in a previous iteration of this loop),
           any other type operator argument is considered "local" (since it
           might contain an enk_param_ref node that has no meaning in the
           current context).  If the type under the typeref involves a
           local type, don't strip it because we know it can't be referred
           from other contexts and the type may not be able to be named
           without the decltype or typeof. */
        is_local = TRUE;
        type_operator_stripped = TRUE;
      }  /* if */
    }  /* if */
    /* Only continue processing this typedef if it is either a local typedef
       or defined in a prototype instantiation. */
    if (!is_local && !is_nonreal) break;
    /* The top level type is a local typedef. */
    check_assertion(!typeref_is_qualified(type));
    type = type->variant.typeref.type;
  }  /* while */
  if (type->kind == (a_type_kind)tk_array) {
    if (type->variant.array.constant_bound_expr_in_local_expr_node_ref) {
      /* For an array type that has an associated local expression that gives
         the backing expression for the constant bound, clear the flag and
         thereby discard the expression.  That's done by making a copy of
         the array type and clearing the flag in the copy. */
      a_type_ptr new_type = alloc_type((a_type_kind)tk_array);
      copy_type(type, new_type);
      type = new_type;
      type->variant.array.constant_bound_expr_in_local_expr_node_ref = FALSE;
    }  /* if */
  }  /* if */
  return traverse_and_modify_type_tree(type,
				       tmtt_strip_local_and_nonreal_typedefs,
                                       TTT_NO_INPUT_FLAGS);
}  /* strip_local_and_nonreal_typedefs */


a_type_ptr strip_routine_default_args(a_type_ptr  type)
/*
If type contains any routine types, remove the default arguments from the
type.
*/
{
  a_type_ptr new_type;

  if (type->kind == (a_type_kind)tk_routine) {
    /* Function type -- remove the default arguments, if any, by making a
       copy of the function type. */
    new_type = routine_type_without_default_args(type);
  } else {
    /* Not a function type -- walk the tree to remove default arguments at
       other levels. */
    a_type_tree_traversal_flag_set ttt_flags = TTT_SKIP_TYPEREFS;
    new_type = traverse_and_modify_type_tree(type,
                                             tmtt_strip_routine_default_args,
                                             ttt_flags);
  }  /* if */
  return new_type;
}  /* strip_routine_default_args */


a_type_ptr strip_qualifiers_from_param_types(a_type_ptr  type)
/*
If type contains any routine types, remove the qualifiers from the parameter
types.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_SKIP_TYPEREFS);
  if (type->kind == (a_type_kind)tk_routine) {
    type = routine_type_without_param_type_qualifiers(type);
  }  /* if */
  return traverse_and_modify_type_tree(type,
				       tmtt_strip_qualifiers_from_param_types,
                                       ttt_flags);
}  /* strip_qualifiers_from_param_types */


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

a_const_char *uuid_string_of_type(a_type_ptr  type)
/*
Return the uuid specification of a class or enum type.  If the given type is
not a class or enum type, return NULL.
*/
{
  a_const_char *result;

  type = skip_typerefs(type);
  if (is_immediate_class_type(type)) {
    result = class_type_supp(type)->uuid_string;
  } else if (is_immediate_enum_type(type)) {
    result = integer_type_supp(type)->uuid_string;
  } else {
    result = NULL;
  }  /* if */
  return result;
}  /* uuid_string_of_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if USER_CONTROL_OF_STRUCT_PACKING

void set_declspec_align(a_type_ptr         type,
                        a_targ_alignment   alignment,
                        a_source_position  *pos)
/*
Apply the given alignment to the given type: It was specified through a
Microsoft __declspec(align(x)) construct.  If the alignment was already
set explicitly, issue a warning for the given position.
*/
{
  if (type->alignment_set_explicitly) {
    pos_warning(ec_multiple_declspec_align, pos);
  }  /* if */
  type->alignment_set_explicitly = TRUE;
  type->alignment = alignment;
}  /* set_declspec_align */

#endif /* USER_CONTROL_OF_STRUCT_PACKING */

a_boolean in_definition_of_class(a_type_ptr  tp)
/*
Return TRUE if the given type is a class type and we are currently inside
that type's definition.
*/
{
  a_boolean  result = FALSE;

  if (num_classes_on_scope_stack != 0) {
    tp = skip_typerefs(tp);
    if (is_incomplete(tp) && is_immediate_class_type(tp) &&
        tp->variant.class_struct_union.extra_info->assoc_scope != NULL) {
      /* tp describes a class type that is being defined, but we may be in the
         midst of the instantiation of a template that's not inside the class.
         Walk the scope stack and parent types to ensure that we are in fact
         inside the definition of tp. */
      a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
      a_type_ptr               enclosing_type = NULL;
      while (ssep->kind != (a_scope_kind)sck_file &&
             ssep->kind != (a_scope_kind)sck_namespace) {
        if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
          /* We're in a class scope.  Break out of this loop and examine if
             it is the given type or a nested class thereof. */
          enclosing_type = ssep->assoc_type;
          break;
        } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* A template instantiation (presumably not of a class template,
             since we would have encountered its scope otherwise).  We must
             be lexically outside a class definition. */
          break;
        }  /* if */
        ssep = &scope_stack[ssep->previous_scope];
      }  /* while */
      /* If we found an enclosing class scope, examine if it or one of its
         parents corresponds to tp. */
      while (enclosing_type != NULL) {
        if (same_entities(enclosing_type, tp)) {
          result = TRUE;
          break;
        } else if (!enclosing_type->source_corresp.is_class_member) {
          break;
        } else {
          enclosing_type = parent_class_of(enclosing_type);
        }  /* if */
      }  /* while */
    }  /* if */
  }  /* if */
  return result;
}  /* in_definition_of_class */

#endif /* !STANDALONE_UTILITY_PROGRAM */


static a_boolean is_virtual_base_class_of(a_type_ptr  base_class_type,
                                          a_type_ptr  derived_type)
/*
Return TRUE if base_class_type is a virtual base class of derived_type.
*/
{
  a_base_class_ptr  bcp;

  /* Loop through the base classes. */
  /*lint --e{446} bcp modified in loop (LINTBUG) */
  for (bcp = base_classes_of(derived_type); bcp != NULL; bcp = bcp->next) {
    if (same_entities(bcp->type, base_class_type)) {
      /* Found it if it's virtual. */
      if (!bcp->is_virtual) bcp = NULL;
      break;
    }  /* if */
  }  /* for */
  /* Return TRUE if we found it. */
  return (bcp != NULL);
}  /* is_virtual_base_class_of */


a_boolean virtual_base_class_is_indirect(a_base_class_ptr  vbcp,
                                         a_type_ptr        class_type)
/*
vbcp points to a virtual direct base class of the current class (class_type).
Return TRUE if it is also an indirect base class of the current class -- i.e.,
if at least one direct base class of the current class is virtually derived
from the same class as the one with which vbcp is associated.
*/
{
  a_base_class_ptr  bcp;
  a_boolean         is_indirect = FALSE;

  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (is_virtual_base_class_of(vbcp->type, bcp->type)) {
      is_indirect = TRUE;
      break;
    }  /* if */
  }  /* for */
  return is_indirect;
}  /* virtual_base_class_is_indirect */


int32_t *min_template_arguments_for_type(a_type_ptr	tp)
/*
Return a pointer to the min_template_arguments field for tp, or NULL if tp
does not have such a field.
*/
{
  int32_t	*result = NULL;

  if (tp->kind == (a_type_kind)tk_typeref) {
    result = &tp->variant.typeref.extra_info->min_template_arguments;
  } else if (is_immediate_class_type(tp)) {
    result = &tp->variant.class_struct_union.extra_info->
                                                        min_template_arguments;
  }  /* if */
  return result;
}  /* min_template_arguments_for_type */


void types_early_init(void)
/*
One time initialization that must occur early in the execution of the
front end.  This must occur before the one-time initialization routines
of the front end are called.
*/
{
  enum_type_is_integral = FALSE ;
}  /* types_early_init */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_type_ptr system_type_from_fundamental_type(a_type_ptr tp)
/*
Returns the C++/CLI type corresponding to the fundamental type, specified by
tp, or NULL if there is no corresponding C++/CLI type.  Note that the caller is
expected to call skip_typerefs if necessary.  See
fundamental_type_from_system_type for the reverse mapping.
*/
{
  a_symbol_ptr symbol = NULL;
  a_type_ptr   sys_type = NULL;

  check_assertion(tp != NULL);
  switch (tp->kind) {
    case tk_integer:
      if (tp->variant.integer.enum_type) {
        symbol = NULL;
      } else if (tp->variant.integer.bool_type) {
        symbol = cli_symbol_from_kind(csk_system_boolean);
      } else if (tp->variant.integer.wchar_t_type) {
        symbol = cli_symbol_from_kind(csk_system_char);
      } else {
        symbol = cli_symbol_from_integer_kind(tp->variant.integer.int_kind);
      }  /* if */
      break;
    case tk_float:
      symbol = cli_symbol_from_float_kind(tp->variant.float_kind);
      break;
    case tk_void:
      symbol = cli_symbol_from_kind(csk_system_void);
      break;
    default:
      symbol = NULL;
      break;
  }  /* switch */
  if (symbol != NULL) {
    sys_type = type_symbol_type(symbol);
    check_assertion(is_value_class_type(sys_type));
  }  /* if */
  return sys_type;
}  /* system_type_from_fundamental_type */


a_type_ptr fundamental_type_from_system_type(a_type_ptr tp)
/*
Returns the fundamental type corresponding to the C++/CLI type, specified by
tp, or NULL if there is no corresponding basic type.  Note that the caller is
expected to call skip_typerefs if necessary.  See
system_type_from_fundamental_type for the reverse mapping.
*/
{
  a_type_ptr result;

  check_assertion(tp != NULL);
  if (is_immediate_class_type(tp)) {
    result = class_type_supp(tp)->corresponding_basic_type;
  } else {
    result = NULL;
  }  /* if */
  return result;
}  /* fundamental_type_from_system_type */

#if !STANDALONE_UTILITY_PROGRAM

a_type_ptr map_cli_system_type_to_fundamental_type(a_type_ptr tp)
/*
If tp is a C++/CLI value class type with a corresponding fundamental type,
return the fundamental type (cv-qualified the same as the original type).
If not, return the original type.
*/
{
  if (cli_or_cx_enabled) {
    a_type_ptr fund_type= fundamental_type_from_system_type(skip_typerefs(tp));
    if (fund_type != NULL) {
      tp = type_plus_qualifiers_from_second_type(fund_type, tp);
    }  /* if */
  }  /* if */
  return tp;
}  /* map_cli_system_type_to_fundamental_type */

#endif /* !STANDALONE_UTILITY_PROGRAM */

a_boolean is_value_class_or_fundamental_type(a_type_ptr tp)
/*
Returns TRUE if this is a C++/CLI value class or a fundamental type with a
corresponding value class type.
*/
{
  a_boolean  result = FALSE;

  tp = skip_typerefs(tp);
  if (is_value_class_type(tp)) {
    result = TRUE;
  } else if (!is_void(tp)) {
    tp = system_type_from_fundamental_type(tp);
    if (tp != NULL) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_value_class_or_fundamental_type */


a_boolean is_cli_enum_type(a_type_ptr tp)
/*
Returns TRUE if tp is a C++/CLI enum type.  Currently, there is no source or
IL distinction between C++11 scoped enums and C++/CLI enumerations.  However,
this function provides a layer of indirection in case this changes in the
future.
*/
{
  tp = skip_typerefs(tp);
  return type_kind_is_integer(tp) && integer_type_is_scoped_enum(tp) &&
         /* In C++/CX, scoped enum types without a declared assembly
            visibility are C++11 scoped enums. */
         !(cppcx_enabled &&
           integer_type_supp(tp)->declared_assembly_visibility ==
                                             (an_assembly_visibility)av_none);
}  /* is_cli_enum_type */


static a_param_type_ptr cli_param_array_from_routine_type(
                                                      a_type_ptr routine_type)
/*
Return a pointer to the C++/CLI parameter array in routine_type or NULL if
there is no parameter array.  The caller must ensure that routine_type is a
tk_routine entry.
*/
{
  a_param_type_ptr  result = NULL, ptp;

  check_assertion (routine_type->kind == (a_type_kind)tk_routine);
  ptp = function_type_params(routine_type);
  if (ptp != NULL) {
    /* Traverse to the end of the parameter list to check for a C++/CLI
       parameter array. */
    while (ptp->next != NULL) ptp = ptp->next;
    if (ptp->is_cli_param_array) result = ptp;
  }  /* if */
  return result;
}  /* cli_param_array_from_routine_type */


a_boolean is_cli_param_array_routine_type(a_type_ptr tp)
/*
Returns TRUE if tp is a type for a routine with a C++/CLI parameter array.
(tp can be a typeref.)
*/
{
  return cli_param_array_from_routine_type(skip_typerefs(tp)) != NULL;
}  /* is_cli_param_array_routine_type */


void error_if_cppcx_public_global_type(a_type_ptr            tp,
                                       a_source_position_ptr error_pos)
/*
Issue an error if the type is a public global C++/CX type, which is disallowed
in C++/CX.
*/
{
  if (cppcx_enabled &&
      !is_class_or_namespace_member(tp) &&
      ((is_immediate_managed_class_type(tp) &&
        class_type_supp(tp)->assembly_visibility ==
                                       (an_assembly_visibility)av_public) ||
       (is_cli_enum_type(tp) &&
        integer_type_supp(tp)->assembly_visibility ==
                                        (an_assembly_visibility)av_public))) {
    pos_error(ec_cppcx_public_global_type, error_pos);
  }  /* if */
}  /* error_if_cppcx_public_global_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_boolean compatible_ms_bit_field_container_types(a_type_ptr  tp1,
                                                  a_type_ptr  tp2)
/*
Return TRUE if tp1 and tp2 are compatible container types, according to the
conventions of Microsoft's bit-field allocation scheme.
*/
{
  a_boolean  compat;

  /* It is assumed that typerefs have already been stripped off. */
  check_assertion(tp1->kind != (a_type_kind)tk_typeref &&
                  tp2->kind != (a_type_kind)tk_typeref);
  /* Check that both are integral types, in case of errors. */
  if (!is_integral_or_enum_type(tp1) || !is_integral_or_enum_type(tp2)) {
    compat = FALSE;
  } else {
    /* Whether or not the integral types are identical, container-type
       compatibility only requires that the sizes be the same. */
    compat = (tp1->size == tp2->size);
  }  /* if */
  return compat;
}  /* compatible_ms_bit_field_container_types */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2015 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
