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
#include "templates.h"
#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
Macros defining some basic classes of types (see 3.1.2.5).  Defined as
macros to avoid many levels of function calls in evaluating these
predicates.
*/
/* The error type simply has type error. */
#define is_error(tp) ((tp)->kind == (a_type_kind)tk_error)

/* Function types are simply function types. */
#define is_function(tp) ((tp)->kind == (a_type_kind)tk_routine)

/* Incomplete types are types that have no size and are not functions. */
#define is_incomplete(tp) ((tp)->size == 0 && !is_function(tp))

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

/* Pointer-to-member type. */
#define is_ptr_to_member(tp) ((tp)->kind == (a_type_kind)tk_ptr_to_member)

/* Object types are non-function and non-reference types that have sizes. */
#define is_object(tp) (!is_function(tp) && !is_reference_ptr(tp) && \
                       (tp)->size != 0)

/* Template parameter type. */
#define is_template_param(tp) ((tp)->kind == (a_type_kind)tk_template_param)


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


a_boolean is_signed_integral_type(a_type_ptr tp)
/*
Return TRUE if the type is a signed integral type.
*/
{
  tp = skip_typerefs(tp);
  return (is_integral(tp) &&
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
    if (is_integral_type(elem_type) &&
        (elem_type->variant.integer.int_kind == targ_wchar_t_int_kind)) {
      is_wchar_t_array = TRUE;
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
of a class template that has been created.  This is determined by first
making sure that the type represents a class, struct, or union, then
making sure that it has a class type supplement, and finally looking for a
non-null template argument list.
*/
{
  register a_boolean                    result = FALSE;
  register a_class_type_supplement_ptr  ctsp;
  tp = skip_typerefs(tp);
  if (tp->kind == (a_type_kind)tk_class ||
      tp->kind == (a_type_kind)tk_struct ||
      tp->kind == (a_type_kind)tk_union) {
    if ((ctsp = tp->variant.class_struct_union.extra_info) != NULL) {
      if (ctsp->template_arg_list != NULL) result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* is_template_class_type */


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
    any_missing = ((tp1_qualifiers & tp2_qualifiers) != tp2_qualifiers);
  }  /* if */
  return any_missing;
}  /* f_any_qualifier_missing */
    
#if !STANDALONE_UTILITY_PROGRAM

a_boolean is_abstract_class_type(a_type_ptr  tp)
/*
Return TRUE if tp is a class/struct/union type for which the abstract
flag is set to TRUE.  Also return TRUE if tp is an uninstantiated template
class for which the flag will be set when it is instantiated.
*/
{
  a_boolean  is_abstract = FALSE;

  if (!C_mode()) {
    tp = skip_typerefs(tp);
    if (is_class_struct_union(tp)) {
      if (tp->variant.class_struct_union.abstract) {
        is_abstract = TRUE;
      } else if (is_incomplete(tp) &&
                 tp->variant.class_struct_union.extra_info->
                                                template_arg_list != NULL) {
        /* This is an uninstantiated template class.  If the template is
           abstract, then so will this instance of it be. */
        a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(tp);
        a_symbol_ptr                   prototype_sym;

        if (!cssp->is_specific_template_def) {
          /* This is not a specific definition, so this instance will be based
             on the template.  To get from here to the type created for the
             prototype instantiation indirect through the template symbol to
             its supplement to the symbol representing the prototype
             instantiation to the type. */
          prototype_sym = cssp->class_template->variant.template_info->
                               variant.class_template.prototype_instantiation;
          if (prototype_sym == NULL) {
            /* Class template has not yet been defined. */
          } else if (prototype_sym->variant.class_struct_union.type->
                                        variant.class_struct_union.abstract) {
            is_abstract = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return is_abstract;
}  /* is_abstract_class_type */


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
    /* Force instantiation of the derived type if it is an uninstantiated
       template class.  This is necessary so that we can see what its base
       classes are.  Note that this can potentially force instantiation
       of the base class as well. */
    instantiate_template_class(derived_class);
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
    if (bcp->direct && bcp->type == base_class_type) break;
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
      disambiguator->derived_class != new_class) {
    internal_error("corresponding_base_class: bad disambiguator");
  }  /* if */
#endif /* CHECKING */
  if (base_class->derived_class == new_class) {
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
    if (bcp->type == base_class->type) {
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
          for (; step->next->base_class != bcp; step = step->next);
          if (step->base_class->type == base_class->derived_class) {
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
            for (; step->next->base_class != bcp; step = step->next);
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
  if (debug_level >= 4 && base_class->derived_class != new_class) {
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
  if (class_1 == class_2 || find_base_class_of(class_1, class_2) != NULL) {
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
related class types, and return TRUE if so.  If they are, set *downcard_cast
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
set *downcard_cast if type_1 --> type_2 is a baseward cast, and set *bcp
to point to the base class entry that shows the relationship.  Note that
the member types are not compared.  Called from the macro
related_member_pointers.
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
        is_class_struct_union_type(type_pointed_to(type_1)) &&
        is_pointer_type(type_2) &&
        is_class_struct_union_type(type_pointed_to(type_2))) {
      type_1 = skip_typerefs(type_pointed_to(type_1));
      type_2 = skip_typerefs(type_pointed_to(type_2));
    }  /* if */
    if (is_class_struct_union_type(type_1) &&
        is_class_struct_union_type(type_2)) {
      bcp = find_base_class_of(type_2, type_1);
#if 0
      /* Strict interpretation of 15.4 para 2. */
      masked = (bcp != NULL);
#else /* 0 */
      /* 15.4 para 1 is clear that a handler for a base class will not
         handle the derived class if the base class is not accessible.
         Does this apply to masking that can be diagnosed at compile time?
         Or is it a restriction on runtime behavior?  For now, we will
         ignore a strict literal interpretation of 15.4 para 2, even
         though it is very explicit about when a masking error is required and
         does not provide a loophole when the base class is inaccessible. */
      masked = (bcp != NULL && is_accessible_base_class(bcp));
#endif /* if 0 */
    } else if (is_pointer_type(type_1) && is_pointer_type(type_2)) {
      /* A pointer-type masks another pointer-type if the latter can be
         implicitly converted to the former.  (This is not explicit in
         the working paper or the ARM and may turn out to be an incorrect
         inference; see 15.4 para 1 and para 2.) */
      a_std_conv_descr std_conv;

      if (impl_pointer_conversion(type_2, /*source_is_constant=*/FALSE,
                                  (a_constant_ptr)NULL, type_1,
                                  /*check_as_operands_not_conversion=*/FALSE,
                                  /*suppress_extensions=*/TRUE,
                                  ec_no_error, &std_conv)) {
        masked = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return masked;
}  /* type_masks_handler_param_type */


static void set_array_type_size(a_type_ptr array_type)
/*
Compute and set the size and alignment of the array type pointed to by
array_type.
*/
{
  a_type_ptr     underlying_elem_type;
  a_targ_size_t  temp, temp2;
  a_type_ptr     elem_type;

  db_enter(5, "set_array_type_size");

  underlying_elem_type =
                  skip_typerefs(underlying_array_element_type(array_type));
  if (is_incomplete(underlying_elem_type) &&
      is_immediate_class_type(underlying_elem_type)) {
    /* This is an array whose element type (directly or indirectly) is an
       incomplete class type.  The size cannot be determined now. The array
       type is put on a list so it can be fixed later if the element type is
       defined.  (Note that an array of incomplete struct is an extension in
       C, but it's standard in C++.) */
    add_to_dependent_type_fixup_list(underlying_elem_type,
                                     (a_dependent_type_fixup_kind)
                                                dtfk_array_type_size,
                                     (char *)array_type,
                                     (a_byte_il_entry_kind)iek_type,
                                     &error_position);
  } else {
    /* Get the number of elements.  Note that this is zero for an incomplete
       type like int a[]. */
    if (!array_type->variant.array.is_variable_size_array) {
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
      error(ec_array_of_abstract_class);
    }  /* if */
#if CHECKING
    if (elem_type->size == 0) {
      internal_error("set_array_type_size: bad element type");
    }  /* if */
#endif /* CHECKING */
    temp2 = elem_type->size;
    /* Check whether or not the multiplication will overflow.  Note that we 
       avoid dividing by temp, since it may be zero for an incomplete type. */
    if (temp > targ_size_t_max / temp2) {
      error(ec_array_size_too_large);
      set_type_kind(array_type, (a_type_kind)tk_error);
      set_type_size(array_type);
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
        break;
      case tk_pointer:
#if TARG_ALL_POINTERS_SAME_SIZE
        /* All pointers are the same size. */
        size = targ_sizeof_pointer;
        alignment = targ_alignof_pointer;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
        error -- set_type_size: different-sized pointers not implemented.
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
        break;
      case tk_array:
        set_array_type_size(type_ptr);
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
as the original type.  Type qualifiers (if any) are dropped.
See also node_type_after_integral_promotion for integral promotions for
bit fields.
*/
{
  register a_type_ptr promoted_type;

  db_enter(5, "type_after_integral_promotion");

  promoted_type = skip_typerefs(type);
  if (is_integral(promoted_type)) {
    switch (promoted_type->variant.integer.int_kind) {
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
            /* All values of type unsigned char can fit in an int, so unsigned
               char is promoted to int. */
            promoted_type = integer_type((an_integer_kind)ik_int);
          } else {
            /* int and char are the same size, so unsigned char is promoted to
               unsigned int. */
            promoted_type = integer_type((an_integer_kind)ik_unsigned_int);
          }  /* if */
        }  /* if */
        break;
      case ik_signed_char:
do_signed_char:;
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
    /* In C++, enumeration types lose their enumeration identity when they
       get promoted. */
    if (C_dialect == C_dialect_cplusplus &&
        promoted_type->variant.integer.enum_type) {
      /* Make a "plain" version of this enum type, i.e., the same underlying
         integral type but not tagged as an enum. */
      promoted_type = integer_type(promoted_type->variant.integer.int_kind);
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

  if (con_is_exact_addr_of_variable(constant)) {
    /* Unmodified address of a variable.  The variable is the complete
       object and its type is the complete object type. */
    complete_object_type = constant->variant.address.variant.variable->type;
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
                          curr_routine->source_corresp.class_of_which_a_member;
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
        complete_object_type = first_operand->next->variant.field->type;
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
  an_expr_node_ptr  node_1, node_2;

  check_assertion(is_array(type_1) && is_array(type_2));
  if (type_1->variant.array.is_variable_size_array) {
    if (type_2->variant.array.is_variable_size_array) {
      /* Both arrays have variable bounds. */
      node_1 = type_1->variant.array.variant.element_count_expr;
      node_2 = type_2->variant.array.variant.element_count_expr;
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
  } else {
    /* Both arrays have fixed bounds.  Just compare the element counts. */
    identical = (type_1->variant.array.variant.number_of_elements ==
                 type_2->variant.array.variant.number_of_elements);
  }  /* if */
  return identical;
}  /* identical_array_type_level */


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
  if (type_1 == type_2) {
    equiv = TRUE;
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
                 cssp_2->class_template == cssp_1->class_template) {
        /* Both types are template classes, and they are based on the same
           class template.  Check further if (a) they are both nonreal
           template classes, or (b) error arguments are to be considered
           equivalent to anything. */
        if ((cssp_1->is_nonreal_class && cssp_2->is_nonreal_class) ||
            error_matches_anything) {
          if (equiv_template_arg_lists(
                             type_1->variant.class_struct_union.extra_info->
                                                            template_arg_list,
                             type_2->variant.class_struct_union.extra_info->
                                                            template_arg_list,
                             /*is_func_template=*/FALSE,
                             error_matches_anything)) {
            equiv = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return equiv;
}  /* equiv_class_types */


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
  a_routine_type_supplement_ptr rtsp1, rtsp2;
  a_symbol_ptr                  sym_1, sym_2;

  db_enter(5, "f_identical_types");

  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls. */
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
          /* Requiring equality for enum types forces an explicit
             type change between enumeration types and integers or
             other enumeration types.  It also makes explicit casts
             useful in suppressing warnings on type changes between
             integral types and enumerated types. */
          if (!type_1->variant.integer.enum_type &&
              !type_2->variant.integer.enum_type) {
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
          /* For pointers and references, they must point to identical types.
             To be IL identical, they need not be both pointers or both
             references. */
          if (il_identical ||
              type_1->variant.pointer.is_reference ==
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
          if (!C_mode() &&
              equiv_class_types(type_1, type_2,
                                /*error_matches_anything=*/FALSE)) {
            identical = TRUE;
          }  /* if */
          break;
        case tk_routine:
          /* For functions, the return types must be identical, the
             parameter lists must be identical, and the implicit "this"
             parameter type (if any) must be identical. */
          rtsp1 = type_1->variant.routine.extra_info;
          rtsp2 = type_2->variant.routine.extra_info;
          if (f_identical_types(type_1->variant.routine.return_type,
                                type_2->variant.routine.return_type,
                                il_identical) &&
              rtsp1->prototyped == rtsp2->prototyped &&
              rtsp1->has_ellipsis == rtsp2->has_ellipsis &&
              ((rtsp1->implicit_this_param_type == NULL) ?
                  (rtsp2->implicit_this_param_type == NULL) :
                  (rtsp2->implicit_this_param_type != NULL &&
                   f_identical_types(rtsp1->implicit_this_param_type,
                                     rtsp2->implicit_this_param_type,
                                     il_identical)))) {
            /* Compare the types of the parameters on the two lists. */
            for (list1 = rtsp1->param_type_list,
                                                list2 = rtsp2->param_type_list;
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
        case tk_ptr_to_member:
          /* Pointer-to-member types are identical if they refer to the same
             class type and to the same member type. */
          identical = (pm_class_type(type_1) == pm_class_type(type_2) &&
                       f_identical_types(pm_member_type(type_1),
                                         pm_member_type(type_2),
                                         il_identical));
          break;
        case tk_template_param:
          if (type_1->variant.template_param.kind ==
                                    type_2->variant.template_param.kind) {
            a_template_param_type_descr_ptr	tptdp_1;
            a_template_param_type_descr_ptr	tptdp_2;
            /* The tag kinds (if any) associated with the template parameters
               must match. */
            tptdp_1 = type_1->variant.template_param.descr;
            tptdp_2 = type_2->variant.template_param.descr;
            if (matching_template_tag_kinds(tptdp_1, tptdp_2)) {
              switch (type_1->variant.template_param.kind) {
                case tptk_param:
                   /* Template parameter types are considered to be identical
                      if their positions in the template parameter list are
                      the same. */
                  identical = (type_1->variant.template_param.list_position ==
                               type_2->variant.template_param.list_position);
                  break;
                case tptk_member:
                  /* Members types are the same if their names are the same
                     and if they are members of identical types. */
                  sym_1 = (a_symbol_ptr)type_1->source_corresp.assoc_info;
                  sym_2 = (a_symbol_ptr)type_2->source_corresp.assoc_info;
                  check_assertion(sym_1 != NULL && sym_2 != NULL);
                  if (sym_1->header == sym_2->header) {
                    /* The names are the same. */
                    identical = (identical_types(type_1->source_corresp.
                                                    class_of_which_a_member,
                                                 type_2->source_corresp.
                                                    class_of_which_a_member));
                  }  /* if */
                  break;
                case tptk_type_of_member_constant:
                  /* Should never happen. */
                  break;
#if CHECKING
                default:
                  internal_error
                             ("f_identical_types: bad templ param type kind");
#endif /* CHECKING */
              }  /* switch */
            }  /* if */
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
  } else if (C_dialect == C_dialect_cplusplus &&
             ((!list1_prototyped && !rtsp1->old_style_params_scanned) ||
              (!list2_prototyped && !rtsp2->old_style_params_scanned))) {
    /* In C++ we can be in the situation of having no information about the
       parameters of a function only in an error case (e.g., using what must
       be a function without its having been declared -- which is legal in
       C mode).  Such a function is not compatible with any other. */
    compatible = FALSE;
  } else if (C_dialect != C_dialect_cplusplus &&
             !list1_prototyped && !list2_prototyped) {
     /* Both parameter lists are old-style -- in C mode they are compatible. */
    compatible = TRUE;
  } else {
    /* If either function has a new-style parameter list, the individual
       parameter types must be compatible.  See the C standard, 3.5.4.3. */
    if (!list2_prototyped) {
      /* List2 is an old-style param list. */
      if (C_dialect != C_dialect_cplusplus) {
        /* In C mode it must be that list1 is prototyped. */
        if (!rtsp2->old_style_params_scanned) {
          /* We are comparing a prototyped parameter list (list1) with an
             old-style list, but there is no parameter information as yet for
             the second one.  The prototyped parameter list from the first
             type is used, but each type will be compared with a promoted
             version of itself. */
          list2 = list1;
        }  /* if */
      } else {
        /* In C++ mode it might be that both parameter lists are old-style --
           in which case they are treated as if prototyped, except that any
           qualifiers are stripped off the param types before comparison. */
      }  /* if */
    } else if (!list1_prototyped) {
      /* It is list1 that is the old-style param list and list2 is prototyped.
         Reverse them, since the processing that follows assumes that the
         old-style list, if there is one, is the second. */
      list1 = list2;
      list1_prototyped = TRUE;
      if (C_dialect != C_dialect_cplusplus &&
          !rtsp1->old_style_params_scanned) {
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
      /* Compare the parameter types, with the second parameter type
         type promoted appropriately if it is old-style. */
      param_1_type = list1->type;
      param_2_type = list2->type;
      if (C_dialect != C_dialect_cplusplus || !list2_prototyped) {
         /* In C mode, the type qualifiers (if any) on the parameter
            types are ignored (ANSI C standard, 3.5.4.3).
            Also when dealing with an old-style function, because it's
            like C mode, and -- especially -- because
            default_argument_promotion drops type qualifiers. */
        param_1_type = skip_typerefs(param_1_type);
        param_2_type = skip_typerefs(param_2_type);
        if (!list2_prototyped) {
          if (C_dialect == C_dialect_cplusplus) {
            /* Do not do default promotion of the argument in C++ mode.
               This is a matter not of conformity to the language definition,
               since old-style param declarations are not supported, but
               of compatibility with cfront, which overloads f in the
               following example:
                 void f(int);            // prototyped
                 void f(x) char x { }    // old-style -- char is not
                                         //   promoted to int           */
          } else {
            param_2_type = default_argument_promotion(param_2_type);
          }  /* if */
        }  /* if */
      }  /* if */
      if (f_types_are_compatible(param_1_type, param_2_type, flags)) {
        /* The parameter types are compatible. */
#if PROTOTYPED_INT_ARGS_PASSED_LIKE_UNPROTOTYPED
      } else if (!strict_ansi_mode && is_integral_type(param_1_type) &&
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

#if MICROSOFT_KEYWORDS_ALLOWED

static a_boolean calling_conventions_are_compatible(a_calling_convention cc1,
                                                    a_calling_convention cc2)
/*
Return TRUE if the two given calling conventions are compatible.  That
means they are identical or one is cc_default and the other matches
default_calling_convention.
*/
{
  a_boolean compatible = (cc1 == cc2 ||
                          (cc1 == (a_calling_convention)cc_default &&
                           cc2 == default_calling_convention) ||
                          (cc2 == (a_calling_convention)cc_default &&
                           cc1 == default_calling_convention));
  return compatible;
}  /* calling_conventions_are_compatible */

#endif /* MICROSOFT_KEYWORDS_ALLOWED */

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
#if MICROSOFT_KEYWORDS_ALLOWED
  a_boolean                     ignore_calling_conventions = FALSE;
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
  a_boolean                     error_matches_anything = 
                        (flags & TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) != 0;

  db_enter(5, "f_types_are_compatible");

  /* The TCF_IGNORE_TYPE_QUALIFIERS flag does not get passed down in general,
     so if it's present remove it from the flags set and keep it off to
     the side. */
  if (flags & TCF_IGNORE_TYPE_QUALIFIERS) {
    ignore_type_qualifiers = TRUE;
    flags &= ~TCF_IGNORE_TYPE_QUALIFIERS;
  }  /* if */
#if MICROSOFT_KEYWORDS_ALLOWED
  /* Ditto for TCF_IGNORE_CALLING_CONVENTIONS. */
  if (flags & TCF_IGNORE_CALLING_CONVENTIONS) {
    ignore_calling_conventions = TRUE;
    flags &= ~TCF_IGNORE_CALLING_CONVENTIONS;
  }  /* if */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
  /* Although the macros do the type_1 == type_2 test, repeat it here
     so it's present for the recursive calls. */
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
    } else if (type_1->kind == type_2->kind) {
      /* The top level kinds are the same, check further. */
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
          /* For pointers and references, they must be both pointers or both
             references and must point to compatible types. */
          if (type_1->variant.pointer.is_reference ==
                                        type_2->variant.pointer.is_reference) {
            compat = f_types_are_compatible(type_1->variant.pointer.type,
                                            type_2->variant.pointer.type,
                                            flags);
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
            if (f_types_are_compatible(type_1->variant.array.element_type,
                                       type_2->variant.array.element_type,
                                       sub_flags)) {
              if ((!type_1->variant.array.is_variable_size_array &&
                   type_1->variant.array.variant.number_of_elements == 0) ||
                  (!type_2->variant.array.is_variable_size_array &&
                   type_2->variant.array.variant.number_of_elements == 0)) {
                compat = TRUE;
              } else {
                compat = identical_array_type_level(type_1, type_2);
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
              ((rtsp1->implicit_this_param_type == NULL) ?
                  (rtsp2->implicit_this_param_type == NULL) :
                  (rtsp2->implicit_this_param_type != NULL &&
                   f_types_are_compatible(rtsp1->implicit_this_param_type,
                                          rtsp2->implicit_this_param_type,
                                          flags)))
#if MICROSOFT_KEYWORDS_ALLOWED
              && (ignore_calling_conventions ||
                  calling_conventions_are_compatible(rtsp1->calling_convention,
                                                    rtsp2->calling_convention))
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
                                                  ) {
            compat = TRUE;
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* Pointer-to-member types are compatible if they refer to the same
             class type and their member types are compatible. */
          compat = (f_types_are_compatible(pm_class_type(type_1),
                                           pm_class_type(type_2), flags) &&
                    f_types_are_compatible(pm_member_type(type_1),
                                           pm_member_type(type_2), flags));
          break;
        case tk_template_param:
          /* Template parameter types are considered to be compatible if
             their positions in the template parameter list are the same. */
          compat = f_identical_types(type_1, type_2, /*il_identical=*/FALSE);
          break;
#if CHECKING
        default:
          internal_error("f_types_are_compatible: bad type");
#endif /* CHECKING */
      }  /* switch */
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
For the integral type given return the canonical (signedness-free)
integer kind (e.g., "unsigned int" and "int" both yield ik_int).
*/
{
  an_integer_kind ikind;

  check_assertion(is_integral(type));
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
  } else if (type_1->kind != type_2->kind) {
    /* The kinds are different, so the types are not interchangeable. */
    /* interch = FALSE;  -- already set. */
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
             !type_2->variant.pointer.is_reference) {
    /* Pointer types.  Get the underlying types. */
    ptr_type_1 = skip_typerefs(type_1->variant.pointer.type);
    ptr_type_2 = skip_typerefs(type_2->variant.pointer.type);
    if (ptr_type_1 == ptr_type_2 ||  /* This test for speed. */
        (strict_ansi_mode ? types_are_compatible(ptr_type_1, ptr_type_2) :
                            interchangeable_types(ptr_type_1, ptr_type_2))) {
      /* Pointers to compatible types are compatible.  As an extension,
         pointers to interchangeable types are interchangeable. */
      interch = TRUE;
    } else if ((is_void(ptr_type_1) && is_character(ptr_type_2)) ||
               (is_character(ptr_type_1) && is_void(ptr_type_2))) {
      /* void * and char * are interchangeable. */
      interch = TRUE;
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
  std_conv->warning_suggested = ec_no_error;
}  /* clear_std_conv_descr */


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


a_boolean same_type_with_added_qualifiers(a_type_ptr dest_type,
                                          a_type_ptr source_type,
                                          a_boolean  ignore_qualifiers)
/*
Return TRUE if source_type and dest_type are compatible types except that
dest_type may have some additional type qualifiers at some level(s).
If ignore_qualifiers is TRUE, qualifiers are ignored at all levels,
which makes this routine something like a types_are_compatible that
ignores type qualifiers.  This routine is used to deal with pointer
conversions that add a type qualifier somewhere other than the top
level, e.g., int ** --> const int **.  Right now, this is a cfront
compatibility feature.  In the future, the version of this feature
specified in the Working Paper will be implemented and this routine
will probably handle it.
*/
{
  a_boolean same = FALSE;

  if (!ignore_qualifiers && any_qualifier_missing(dest_type, source_type)) {
    /* Some qualifier is missing. */
    same = FALSE;
  } else {
    dest_type = skip_typerefs(dest_type);
    source_type = skip_typerefs(source_type);
    if (is_pointer_type(dest_type) && is_pointer_type(source_type)) {
      /* Continue at the next level for pointers. */
      same = same_type_with_added_qualifiers(type_pointed_to(dest_type),
                                             type_pointed_to(source_type),
                                             ignore_qualifiers);
    } else if (is_array_type(dest_type) && is_array_type(source_type) &&
               !dest_type->variant.array.is_variable_size_array &&
               !source_type->variant.array.is_variable_size_array &&
               dest_type->variant.array.variant.number_of_elements ==
                  source_type->variant.array.variant.number_of_elements) {
      /* Continue at the next level for arrays. */
      same = same_type_with_added_qualifiers(array_element_type(dest_type),
                                             array_element_type(source_type),
                                             ignore_qualifiers);
    } else {
      /* For other types, the underlying types must be the same. */
      same = types_are_compatible(dest_type, source_type);
    }  /* if */
  }  /* if */
  return same;
}  /* same_type_with_added_qualifiers */


a_boolean impl_pointer_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            check_as_operands_not_conversion,
                         a_boolean            suppress_extensions,
                         an_error_code        default_warning_code,
                         a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type source_type
(any type) to something of type dest_type (a pointer type).
If source_is_constant is TRUE, the source is a constant, and source_constant
points to the constant value.  (That's needed to check for conversions of a
null pointer constant to a pointer type.)  If check_as_operands_not_conversion
is TRUE, the two types are the types of the operands of an operation; only
do the checks required in that case, which are fewer than the checks required
for a conversion.  suppress_extensions is TRUE if conversions that are
extensions should not be allowed (what constitutes an extension depends on
C_dialect, of course).  If the conversion is possible, *std_conv is filled
out to describe the conversion.  In particular, if the conversion is
suspect and should be flagged with a warning, the warning_suggested field is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into warning_suggested when no
specific message applies.  In strict mode, if a conversion flagged
with warning_suggested is done, the warning is required.

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
  if (source_is_constant && is_null_pointer_constant(source_constant)) {
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
    /* Get the type pointed to and drop type qualifiers and typedefs. */
    source_type_pointed_to = type_pointed_to(source_type);
    unqual_source_type_pointed_to = skip_typerefs(source_type_pointed_to);
    /* The "_ignoring_qualifiers" version is used to get proper handling of
       pointers to arrays with qualified element types. */
    if (types_are_compatible_ignoring_qualifiers(unqual_source_type_pointed_to,
                                                unqual_dest_type_pointed_to)) {
      /* The types pointed to are compatible, ignoring the type qualifiers.
         ANSI C 3.3.6 (pointer - pointer: caller will check that types are
         object types); ANSI C 3.3.8 (relational operators: caller will check
         that types are both object or both incomplete); ANSI C 3.3.9
         (equality operators); ANSI C 3.3.15 (?: operator); ANSI C 3.3.16.1
         (assignment: preservation of qualifiers is tested below). */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (is_error(unqual_dest_type_pointed_to) ||
               is_error(unqual_source_type_pointed_to)) {
      /* Pointer --> pointer-to-error and pointer-to-error --> pointer are
         always allowed. */
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
            /* In C++, a pointer to a function may be converted to "void *" if
               the pointer will fit in a "void *".  ARM 4.6 (pointer
               conversions). */
            if (dest_of_ptr_cast_big_enough(source_type, dest_type)) {
              okay = TRUE;
              std_conv->pointer_normalization_needed = TRUE;
            }  /* if */
          } else {
            /* In C, such a conversion is nonstandard, but allowed as
               an extension, with a warning */
            if (!suppress_extensions) {
              okay = TRUE;
              std_conv->pointer_normalization_needed = TRUE;
              std_conv->warning_suggested = default_warning_code;
            }  /* if */
          }  /* if */
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
           and accessibility check to be done when the cast is done.
           That's not quite what the ARM says, but it's what cfront does,
           and it makes sense. */
        okay = TRUE;
        std_conv->cast_base_class = bcp;
      } else if ((conversion_from_void_star_in_C =
                   (C_dialect != C_dialect_cplusplus &&
                    !check_as_operands_not_conversion &&
                    is_void_type(unqual_source_type_pointed_to))) &&
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
      } else if (C_dialect == C_dialect_pcc) {
        /* In pcc mode, allow conversion between incompatible pointer types,
           with a warning. */
        okay = TRUE;
        std_conv->warning_suggested = default_warning_code;
      } else if ((!suppress_extensions || any_cfront_mode()) &&
                 same_type_with_added_qualifiers(dest_type_pointed_to,
                                                 source_type_pointed_to,
                                                 /*ignore_qualifiers=*/
                                           check_as_operands_not_conversion)) {
        /* Allow conversion between pointers where type qualifiers are
           being added at levels other than the first, e.g.,
           "int **" -> "const int **".  This is an extension, and a
           warning is issued.  cfront allows this, so issue no warning in
           that mode. */
        okay = TRUE;
        std_conv->nontrivial_conversion = FALSE;
        if (!any_cfront_mode()) {
          std_conv->warning_suggested = default_warning_code;
        } /* if */
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
      }  /* if */
    }  /* if */
    if (okay && !check_as_operands_not_conversion) {
      /* The types pointed to must be such that the type pointed to by the
         left has all the qualifiers of the type pointed to by the right.
         It might have additional qualifiers.  ANSI C 3.3.16.1 (assignment);
         ARM 4.6 (pointer conversions: qualifiers cannot be dropped
         implicitly), 5.17 (assignment), 8.4 (initializers). */
      if (type_qualifiers_match(dest_type_pointed_to,
                                source_type_pointed_to)) {
        /* The qualifiers are the same. */
      } else if (any_qualifier_missing(dest_type_pointed_to,
                                       source_type_pointed_to)) {
        /* Qualifiers are being dropped. */
        if (cfront_2_1_mode && 
            is_void(unqual_dest_type_pointed_to) &&
            is_void(unqual_source_type_pointed_to)) {
          /* cfront 2.1 allows conversion of a pointer to qualified void
             (e.g., "const void *") to "void *". */
        } else {
          /* Qualifiers are being dropped. */
          okay = FALSE;
        }  /* if */
      } else {
        /* Qualifiers are being added. */
        std_conv->type_qualifiers_added = TRUE;
      }  /* if */
    }  /* if */
  } else if (C_dialect == C_dialect_pcc && is_integral(source_type)) {
    /* In pcc mode, allow integer --> pointer with a warning.  The null
       pointer constant --> pointer case has been handled above and does
       not come here. */
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
  a_boolean  correspond = FALSE;
  a_type_ptr this_type_1, this_type_2;

  rout_type_1 = skip_typerefs(rout_type_1);
  rout_type_2 = skip_typerefs(rout_type_2);
  this_type_1 = rout_type_1->variant.routine.extra_info->
                                                      implicit_this_param_type;
  this_type_2 = rout_type_2->variant.routine.extra_info->
                                                      implicit_this_param_type;
  if (this_type_1 == NULL) {
    /* type_1 does not have a "this" parameter type.  Match if type_2 also
       does not. */
    correspond = (this_type_2 == NULL);
  } else if (this_type_2 == NULL) {
    /* type_1 has a "this" parameter type, type_2 does not. */
    /* correspond = FALSE;  -- already set. */
  } else if (!type_qualifiers_match(this_type_1, this_type_2)) {
    /* The type qualifiers do not match. */
    /* correspond = FALSE;  -- already set. */
  } else {
    this_type_1 = type_pointed_to(this_type_1);
    this_type_2 = type_pointed_to(this_type_2);
    if (!any_cfront_mode()) {
      if (!type_qualifiers_match(this_type_1, this_type_2)) {
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
      } else if (type_qualifiers_match(this_type_1, this_type_2)) {
        correspond = TRUE;
      } else if (check_as_conversion) {
        /* The type qualifiers do not match, but this is a conversion,
           so that may be okay. */
        if (any_qualifier_missing(this_type_2, this_type_1)) {
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
                                   a_boolean  check_as_operands_not_conversion)
/*
Return TRUE if the two function types given are compatible if one ignores any
difference in the underlying class of their "this" parameter types.
If check_as_operands_not_conversion is TRUE, the two types are the types
of the operands of an operation; if FALSE, rout_type_1 and rout_type_2
are the destination and source types of a conversion.
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
                                           !check_as_operands_not_conversion,
                                           check_as_operands_not_conversion);
  return correspond;
}  /* function_types_correspond */


static a_boolean member_types_correspond(
                                   a_type_ptr member_type_1,
                                   a_type_ptr member_type_2,
                                   a_boolean  check_as_operands_not_conversion)
/*
Return TRUE if the member types from two pointer-to-member types match
allowing for a possible difference due to the associated class type.
Specifically, this means that when comparing function types, the
difference in the underlying class of the "this" parameter type must
be ignored.  If check_as_operands_not_conversion is TRUE, the two types
are the types of the operands of an operation; if FALSE, member_type_1
and member_type_2 are the destination and source types of a conversion.
*/
{
  a_boolean correspond;

  if (!is_function_type(member_type_1) || !is_function_type(member_type_2)) {
    /* This is not the special function case, so the normal
       types_are_compatible check will work. */
    correspond = types_are_compatible(member_type_1, member_type_2);
  } else {
    /* We have two function types from member pointers.  See if they
       match when we allow for the difference in the underlying type
       of the "this" parameter.  Note that this test must be done even
       when the class types are the same, because the routines may
       be from base classes. */
    correspond = function_types_correspond(member_type_1, member_type_2,
                                           check_as_operands_not_conversion);
  }  /* if */
  return correspond;
}  /* member_types_correspond */


a_boolean impl_ptr_to_member_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            check_as_operands_not_conversion,
                         a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it's okay to implicitly convert something of type source_type
(any type) to something of type dest_type (a pointer to member type).
If source_is_constant is TRUE, the source is a constant, and source_constant
points to the constant value.  (That's needed to check for conversions of a
null pointer constant to a pointer to member type.)
If check_as_operands_not_conversion is TRUE, the two types are the types
of the operands of an operation; only do the checks required in that case,
which are fewer than the checks required for a conversion.  If the conversion
is possible, *std_conv is filled out to describe the conversion.

Note that any type qualifiers on the types themselves (rather than the
types pointed to) are ignored.

See ARM 5.17 (assignment operators) and 4.8 (standard conversions for
pointers to members).
*/
{
  a_boolean  okay = FALSE;
  a_type_ptr dest_type_pointed_to, source_type_pointed_to;

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
       unambiguous derived (sic) class of the source class.  See ARM 4.8. */
    source_type_pointed_to = pm_member_type(source_type);
    dest_type_pointed_to = pm_member_type(dest_type);
    if (member_types_correspond(skip_typerefs(dest_type_pointed_to),
                                skip_typerefs(source_type_pointed_to),
                                check_as_operands_not_conversion)) {
      a_type_ptr       source_class_type = pm_class_type(source_type);
      a_type_ptr       dest_class_type = pm_class_type(dest_type);
      a_base_class_ptr bcp;
      /* The types pointed to are the same.  Check the classes. */
      if (source_class_type == dest_class_type) {
        /* Same class, okay. */
        okay = TRUE;
        std_conv->nontrivial_conversion = FALSE;
      } else if ((bcp = find_base_class_of(dest_class_type,
                                           source_class_type)) != NULL) {
        /* Derived class, okay. */
        /* We leave the ambiguity and accessibility check to be done when
           the cast is done. */
        okay = TRUE;
        std_conv->cast_base_class = bcp;
        std_conv->reversed_cast = TRUE;
      }  /* if */
    }  /* if */
    if (okay && !check_as_operands_not_conversion) {
      /* The types pointed to must be such that the type pointed to by the
         left has all the qualifiers of the type pointed to by the right.
         It might have additional qualifiers.  This is not mentioned in
         the ARM, but it makes sense by analogy with pointer types
         (ARM 4.6, 5.17, 8.4). */
      if (type_qualifiers_match(dest_type_pointed_to,
                                source_type_pointed_to)) {
        /* The qualifiers are the same. */
      } else if (any_qualifier_missing(dest_type_pointed_to,
                                       source_type_pointed_to)) {
        /* Qualifiers are being dropped. */
        okay = FALSE;
      } else {
        /* Qualifiers are being added. */
        std_conv->type_qualifiers_added = TRUE;
      }  /* if */
    }  /* if */
  } else if (source_is_constant &&
             is_null_pointer_constant(source_constant)) {
    /* 0 --> pointer-to-member.  See ARM 4.8. */
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


a_boolean impl_conversion_possible(a_type_ptr           source_type,
                                   a_boolean            source_is_constant,
                                   a_constant           *source_constant,
                                   a_type_ptr           dest_type,
                                   a_boolean            suppress_extensions,
                                   an_error_code        default_warning_code,
                                   a_std_conv_descr_ptr std_conv)
/*
Return TRUE if it is okay to implicitly convert something of type source_type
to something of type dest_type.  If source_is_constant is TRUE, the source
is a constant, and source_constant points to the constant value.  (That's
needed to check for conversions of a null pointer constant to a pointer
type.)  suppress_extensions is TRUE if conversions that are extensions
should not be allowed (what constitutes an extension depends on
C_dialect, of course).  If the conversion is possible, *std_conv is
filled out to describe the conversion.  In particular, if the conversion
is suspect and should be flagged with a warning, the warning_suggested
field is set to an appropriate error code; normally, it is set to
ec_no_error.  default_warning_code will be copied into warning_suggested
when no specific message applies.  In strict mode, if a conversion
flagged with warning_suggested is done, the warning is required.

Note that any top-level type qualifiers on the types are ignored.

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
  a_boolean  source_is_integral;
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
  } else if (is_arithmetic(dest_type)) {
    /* Destination type is arithmetic. */
    if (identical_types(source_type, dest_type)) {
      /* No type change. */
      okay = TRUE;
      std_conv->nontrivial_conversion = FALSE;
    } else if (is_arithmetic(source_type)) {
      /* Arithmetic --> arithmetic.  Okay. */
      okay = TRUE;
      /* Check for conversion of an arithmetic type to an enumerated type,
         which may be invalid or call for a warning. */
      dest_enum_type = NULL;
      if (is_integral(dest_type)) {
        dest_enum_type = underlying_enum_type(dest_type);
      }  /* if */
      if (dest_enum_type != NULL) {
        /* Conversion is to an enum type. */
        source_enum_type = NULL;
        source_is_integral = is_integral(source_type);
        if (source_is_integral) {
          source_enum_type = underlying_enum_type(source_type);
        }  /* if */
        if (source_enum_type != dest_enum_type) {
          /* Conversion of one enum type to another, or conversion of an
             arithmetic non-enum type to an enum. */
          if (C_dialect != C_dialect_cplusplus) {
            /* Mixed integral/enum types allowed in C with a warning. */
            std_conv->warning_suggested = ec_mixed_enum_type;
          } else if (cfront_2_1_mode && source_is_integral) {
            /* Integral --> enum allowed in cfront 2.1 mode, with a
               warning.  cfront 2.1 also allows floats to be converted to
               enums, but it doesn't seem necessary to duplicate that
               behavior. */
            std_conv->warning_suggested = ec_mixed_enum_type;
          } else {
            /* Other C++ modes: no mixing allowed. */
            okay = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (C_dialect == C_dialect_pcc &&
               is_pointer(source_type) &&
               is_integral(dest_type)) {
      /* In pcc mode, allow pointer --> integer (even if the integer is not
         big enough).  Issue a warning. */
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
                                   source_constant, dest_type,
                                   /*check_as_operands_not_conversion=*/FALSE,
                                   suppress_extensions,
                                   default_warning_code,
                                   std_conv);
  } else if (is_ptr_to_member(dest_type)) {
    /* Conversion to a C++ pointer-to-member type. */
    okay = impl_ptr_to_member_conversion(source_type,
                                         source_is_constant, source_constant,
                                         dest_type,
                                    /*check_as_operands_not_conversion=*/FALSE,
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
is suspect and should be flagged with a warning, *warning_suggested is
set to an appropriate error code; normally, it is set to ec_no_error.
default_warning_code will be copied into *warning_suggested when no
specific message applies.  In strict mode, if a conversion flagged with
*warning_suggested is done, the warning is required.

Any implicit conversion is allowed (see impl_conversion_possible).  Also, the
explicit conversions allowed in casts (ARM 5.2.3 and 5.4; ANSI C 3.3.4)
are allowed.  Reference conversions have been turned into pointer conversions
by the time they get here.  Note that this routine does not handle user-defined
conversions (constructors and conversion functions).
*/
{
  a_boolean        okay = FALSE, impl_okay, suppress_extensions = FALSE;
  a_std_conv_descr impl_std_conv;

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
  *warning_suggested = ec_no_error;
  /* If in strict mode and nonstandard constructs should be reported as
     errors, disable extensions. */
  if (strict_ansi_mode && strict_ansi_error_severity == es_error) {
    suppress_extensions = TRUE;
  }  /* if */
  /* Drop any type qualifiers and typedefs on the two types. */
  source_type = skip_typerefs(source_type);
  dest_type = skip_typerefs(dest_type);

  /* See if there is an implicit conversion between the types. */
  impl_okay = impl_conversion_possible(source_type,
                                       source_is_constant, source_constant,
                                       dest_type,
                                       suppress_extensions,
                                       default_warning_code,
                                       &impl_std_conv);
  if (impl_okay && impl_std_conv.warning_suggested == ec_no_error) {
    /* There is an implicit conversion, and it's not questionable. */
    okay = TRUE;
  } else if (is_incomplete(dest_type)) {
    /* This catches incomplete enums for completeness.  The caller probably
       ruled out incomplete types anyway. */
    /* okay = FALSE; -- already set. */
  } else if (is_integral(source_type) && is_enum(dest_type)) {
    /* In C++, integral --> enum can only be done by explicit conversion.
       In C, it's allowed as an implicit conversion but we check for it
       again here to avoid the warning. */
    okay = TRUE;
  } else if (is_pointer(source_type) && is_integral(dest_type) &&
             (C_mode() ||
              dest_of_ptr_cast_big_enough(source_type, dest_type))) {
    /* Pointer --> integral is okay if (a) the integer is big enough or
       (b) it's not big enough but we're compiling C. */
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
        if (C_dialect != C_dialect_cplusplus) {
          /* C mode. */
          if (!suppress_extensions) {
            okay = TRUE;
            *warning_suggested = ec_mixed_function_object_pointers;
          }  /* if */
        } else {
          /* C++ mode. */
          okay = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_ptr_to_member(source_type) &&
             is_ptr_to_member(dest_type)) {
    /* Pointer-to-member --> pointer-to-member.  Valid if the classes involved
       are the same or related (ARM 5.4).  Note that the type of thing pointed
       to is not important here, which is different than the implicit
       base::* --> derived::* case, so the base::* --> derived::* case must be
       checked here as well as in impl_conversion_allowed.  We leave the
       ambiguity check to be done when the cast is done. */
    a_type_ptr source_class, dest_class;
    source_class = pm_class_type(source_type);
    dest_class = pm_class_type(dest_type);
    if (source_class == dest_class ||
        find_base_class_of(source_class, dest_class) != NULL ||
        find_base_class_of(dest_class, source_class) != NULL) {
      /* The ARM doesn't say this, but pointers-to-data-members and
         pointers-to-member-functions should not be compatible.  This
         follows cfront. */
      if (is_function_type(pm_member_type(source_type)) ==
          is_function_type(pm_member_type(dest_type))) {
        okay = TRUE;
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
    *warning_suggested = impl_std_conv.warning_suggested;
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
  a_type_ptr       member_type_1, member_type_2;
  a_type_ptr       comp_elem, comp_param_type, param_type;
  a_targ_size_t    num_elems;
  a_param_type_ptr list1, list2, param1, param2;
  a_boolean        list1_prototyped, list2_prototyped;
  a_boolean        comp_equals_list1, comp_equals_list2;
  a_param_type_ptr comp_param, comp_param_list, end_comp_param_list;
  a_boolean        comp_prototyped;
  an_expr_node_ptr comp_default_arg_expr;
  a_boolean        comp_has_default_arg;
  a_type_ptr       param_1_type, param_2_type;
  a_routine_type_supplement_ptr
                   rtsp1, rtsp2;
#if MICROSOFT_KEYWORDS_ALLOWED
  a_calling_convention
                   comp_calling_convention;
#endif /* MICROSOFT_KEYWORDS_ALLOWED */

  db_enter(5, "composite_type");

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
        case tk_template_param:
          /* Template parameter types.  If only one of the types points to
             a param_type_descr then it is the composite.  If both point to
             a type descriptor, but only one includes a tag kind, then it is
             the composite. */
            { a_template_param_type_descr_ptr	tptdp_1;
              a_template_param_type_descr_ptr	tptdp_2;
              /* The tag kinds (if any) associated with the template parameters
                 must match. */
              tptdp_1 = base_type_1->variant.template_param.descr;
              tptdp_2 = base_type_2->variant.template_param.descr;
              /* Set the composite to type_1 until we determine otherwise. */
              comp_type = base_type_1;
              if (tptdp_1 == NULL && tptdp_2 != NULL) {
                /* Only the second type as a type descr. */
                comp_type = base_type_2;
              } else if (tptdp_2 != NULL &&
                         (tptdp_1->tag_kind == (a_type_kind)tk_unknown &&
                          tptdp_2->tag_kind != (a_type_kind)tk_unknown)) {
                /* Only the second type has a tag kind. */
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
          check_assertion(!base_type_1->variant.array.is_variable_size_array);
          check_assertion(!base_type_2->variant.array.is_variable_size_array);
          if (base_type_1->variant.array.variant.number_of_elements != 0) {
            num_elems = base_type_1->variant.array.variant.number_of_elements;
            comp_elem = composite_type(base_type_1->variant.array.element_type,
                                      base_type_2->variant.array.element_type);
          } else {
            num_elems = base_type_2->variant.array.variant.number_of_elements;
            comp_elem = composite_type(base_type_2->variant.array.element_type,
                                      base_type_1->variant.array.element_type);
          }  /* if */
          /* Try to use one of the two types we already have.  If that's
             not possible, build a new array type. */
          if (comp_elem == base_type_1->variant.array.element_type &&
              num_elems == base_type_1->
                                  variant.array.variant.number_of_elements) {
            comp_type = base_type_1;
          } else if (comp_elem == base_type_2->variant.array.element_type &&
              num_elems == base_type_2->
                                  variant.array.variant.number_of_elements) {
            comp_type = base_type_2;
          } else {
            comp_type = alloc_type((a_type_kind)tk_array);
            comp_type->variant.array.element_type = comp_elem;
            comp_type->variant.array.variant.number_of_elements = num_elems;
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
          rtsp1 = base_type_1->variant.routine.extra_info;
          rtsp2 = base_type_2->variant.routine.extra_info;
          list1 = rtsp1->param_type_list;
          list2 = rtsp2->param_type_list;
          list1_prototyped = rtsp1->prototyped;
          list2_prototyped = rtsp2->prototyped;
          comp_prototyped = list1_prototyped || list2_prototyped;
#if MICROSOFT_KEYWORDS_ALLOWED
          comp_calling_convention = rtsp1->calling_convention;
          if (comp_calling_convention == (a_calling_convention)cc_default) {
            comp_calling_convention = rtsp2->calling_convention;
          }  /* if */
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
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
              /* Form the composite of the two types.  One difficult case
                 that comes up is
                   int f(int);
                   int f(const int);
                 X3J11 has said that the composite of those parameter types
                 is the composite of the unqualified types (interpretation
                 13).  We use that rule only when the qualifiers are
                 different, so that the composite of
                   int f(const int, int);
                   int f(const int, const int);
                 still has "const int" in the first parameter. */
              param_1_type = param1->type;
              param_2_type = param2->type;
              if (!type_qualifiers_match(param_1_type, param_2_type)) {
                param_1_type = make_unqualified_type(param_1_type);
                param_2_type = make_unqualified_type(param_2_type);
              }  /* if */
              comp_param_type = composite_type(param_1_type, param_2_type);
              /* Form the composite of the C++ default argument expressions;
                 it's guaranteed that at most one of the parameter lists
                 has a default argument expression. */
              comp_default_arg_expr = (param1->default_arg_expr != NULL) ?
                                                     param1->default_arg_expr :
                                                     param2->default_arg_expr;
	      comp_has_default_arg = param1->has_default_arg ||
				     param2->has_default_arg;
              /* Compare the two parameter types against their composite
                 type.  Stop if it is no longer true that one of the original
                 parameter lists can serve as the composite list. */
              if (comp_param_type != param1->type ||
		  comp_has_default_arg != (a_boolean)param1->has_default_arg ||
                  comp_default_arg_expr != param1->default_arg_expr) {
                comp_equals_list1 = FALSE;
              }  /* if */
              if (comp_param_type != param2->type ||
		  comp_has_default_arg != (a_boolean)param2->has_default_arg ||
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
                if (param1 == param1_on_which_first_loop_failed) {
                  /* Little optimization: when we get to the parameters on
                     which the loop above failed, use the composite type
                     already formed.  This is nice when that type is something
                     distinct from the two parameter types.  Without this
                     trick, that type would be lost. */
                  param_type = comp_param_type;
                } else {
                  /* For the other parameter pairs, we call composite_type.
                     For the parameters preceding the key pair, composite_type
                     will do what it did in the loop above and return one of
                     the original types; for parameters following that pair,
                     composite_type must be called because it has not been
                     called yet for those parameters. */
                  param_type = composite_type(param1->type, param2->type);
                }  /* if */
                /* Pass a NULL source position to make_param_type to avoid
                   inappropriate diagnostics on a type that doesn't correspond
                   directly to a source construct. */
                comp_param = make_param_type(param_type,
                                             &null_source_position);
                /* Form the composite of the C++ default argument expressions;
                   it's guaranteed that at most one of the parameter lists
                   has a default argument expression. */
                if (param1->has_default_arg) {
                  comp_param->has_default_arg = TRUE;
                  comp_param->default_arg_expr = param1->default_arg_expr;
                } else if (param2->has_default_arg) {
                  comp_param->has_default_arg = TRUE;
                  comp_param->default_arg_expr = param2->default_arg_expr;
                }  /* if */
                if (param1->type_involves_template_param ||
                    param2->type_involves_template_param) {
                  comp_param->type_involves_template_param = TRUE;
                }  /* if */
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
              rtsp1->param_type_list == comp_param_list &&
              list1_prototyped == comp_prototyped
#if MICROSOFT_KEYWORDS_ALLOWED
              && rtsp1->calling_convention == comp_calling_convention
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
                                                 ) {
            comp_type = base_type_1;
            if (rtsp1->exception_specification == NULL) {
              rtsp1->exception_specification = rtsp2->exception_specification;
            }  /* if */
          } else if (base_type_2->variant.routine.return_type == comp_elem &&
              rtsp2->param_type_list == comp_param_list &&
              (a_boolean)rtsp2->prototyped == comp_prototyped
#if MICROSOFT_KEYWORDS_ALLOWED
              && rtsp2->calling_convention == comp_calling_convention
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
                                                             ) {
            comp_type = base_type_2;
            if (rtsp2->exception_specification == NULL) {
              rtsp2->exception_specification = rtsp1->exception_specification;
            }  /* if */
          } else {
            /* Build a new function type. */
            a_routine_type_supplement_ptr rtsp;
            comp_type = alloc_type((a_type_kind)tk_routine);
            comp_type->variant.routine.return_type = comp_elem;
            rtsp = comp_type->variant.routine.extra_info;
            rtsp->param_type_list = comp_param_list;
            rtsp->prototyped = comp_prototyped;
#if MICROSOFT_KEYWORDS_ALLOWED
            rtsp->calling_convention = comp_calling_convention;
#endif /* MICROSOFT_KEYWORDS_ALLOWED */
            rtsp->has_ellipsis = rtsp1->has_ellipsis;
            rtsp->implicit_this_param_type = rtsp1->implicit_this_param_type;
            if (rtsp1->exception_specification != NULL) {
              rtsp->exception_specification = rtsp1->exception_specification;
            } else {
              rtsp->exception_specification = rtsp2->exception_specification;
            }  /* if */
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* The composite of two pointer-to-member types will point to the
             same class type and to a member type that is a composite of the
             two member types. */
          member_type_1 = pm_member_type(base_type_1);
          member_type_2 = pm_member_type(base_type_2);
          comp_elem = composite_type(member_type_1, member_type_2);
          if (comp_elem == member_type_1) {
            comp_type = base_type_1;
          } else if (comp_elem == member_type_2) {
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
    if (comp_type == base_type_1) {
      comp_type = type_1;
    } else if (comp_type == base_type_2) {
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
  /* Now compare the types.  If any error types appear in the type tree, that
     will be enough to distinguish the types. */
  if (!f_types_are_compatible(type_1, type_2, TCF_NO_FLAGS)) {
    /* The two types are distinguishable. */
    distinguishable = TRUE;
  } else {
    /* The types are indistinguishable.  See if the original types are
       compatible (meaning the same type, roughly).  This is useful to know
       in issuing the right error message. */
    if (*params_all_compatible &&
        !types_are_strictly_compatible(orig_type_1, orig_type_2)) {
      *params_all_compatible = FALSE;
    }  /* if */
  }  /* if */
  return distinguishable;
}  /* types_distinguishable */


a_boolean overload_distinguishable(a_symbol_ptr  old_sym_ptr,
                                   a_type_ptr    new_type,
                                   a_boolean     new_is_template,
                                   an_error_code *err_code)
/*
Return TRUE if the new function type new_type is distinguishable under
overload resolution from all the types of the functions indicated by
old_sym_ptr (which might be a simple function or an sk_overloaded_function
symbol).  Otherwise, set *err_code to an appropriate error code
and return FALSE.  We assume that the caller has already determined that
the new type is not compatible with any of the existing types.
The new type may be for a function template (new_is_template is TRUE
in that case), as may any of the types on the old list.  Only callable
in C++ mode.  See ARM 13.
*/
{
  a_boolean        distinguishable, params_all_compatible;
  a_boolean        old_is_list, old_is_template;
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
                        is_qualified_type(
                                   type_pointed_to(new_this_param_type)));
  do {
    /* See if old_sym_ptr and new_type are distinguishable. */
    old_is_template = (old_sym_ptr->kind ==
                                          (a_symbol_kind)sk_function_template);
    if (old_is_template != new_is_template) {
      /* Function templates are always distinguishable from non-template
         functions. */
      distinguishable = TRUE;
      goto distinguishable_determined;
    }  /* if */
    distinguishable = FALSE;
    params_all_compatible = TRUE;
    /* Get the old routine type. */
    if (old_is_template) {
      old_type = old_sym_ptr->variant.template_info->
                                                variant.function.routine->type;
      old_type = skip_typerefs(old_type);
    } else {
      old_type = routine_symbol_type(old_sym_ptr);
    }  /* if */
    old_extra_info = old_type->variant.routine.extra_info;
    /* See if the types are sufficiently different that they are
       distinguishable by overload resolution. */
    /* Note that the code here must match determine_arg_match_level
       and function_template_matches_operand_list. */
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
                          is_qualified_type(
                                    type_pointed_to(old_this_param_type)));
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
    old_param = old_extra_info->param_type_list;
    new_param = new_extra_info->param_type_list;
    if ((!old_extra_info->prototyped && old_param == NULL) ||
        (!new_extra_info->prototyped && new_param == NULL)) {
      /* In C++ an old-style declaration can have an empty param list only
         as the result of an error.  Assume an error is distinguishable from
         anything else. */
      distinguishable = TRUE;
      goto distinguishable_determined;
    }  /* if */
    for (; old_param != NULL || new_param != NULL;
           old_param = old_param->next, new_param = new_param->next) {
      if (old_param == NULL || new_param == NULL) {
        /* The parameter lists do not end at the same point, so they
           are distinguishable. */
        distinguishable = TRUE;
        goto distinguishable_determined;
      } else {
        /* See if the types are distinguishable.  A parameter containing
           template types is always distinguishable from one not containing
           such types. */
        /* Note that we do NOT do default argument promotions on old-style
           (unprototyped) function parameter types, because in overload
           resolution the unprototyped type is used. */
        if (old_param->type_involves_template_param !=
            new_param->type_involves_template_param ||
            types_distinguishable(old_param->type, new_param->type,
                                  &params_all_compatible)) {
          distinguishable = TRUE;
          goto distinguishable_determined;
        }  /* if */
      }  /* if */
    }  /* for */
    /* All the parameters are indistinguishable. */
    /* If one function is an old-style unprototyped function and the
       other isn't, go with the "normal" message. */
    if (params_all_compatible &&
        old_extra_info->prototyped == new_extra_info->prototyped) {
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
distinguishable_determined:;
  } while (distinguishable &&
           old_is_list && (old_sym_ptr = old_sym_ptr->next) != NULL);
  db_exit();
  return distinguishable;
}  /* overload_distinguishable */


/* Bit vector used to pass flags into traverse_type_tree.  Each bit
   represents a flag. */
typedef int a_type_tree_traversal_flag_set;
/* Constants defining bits in the input bit vector used in calls to
   declarator. */
#define TTT_NO_INPUT_FLAGS 0x0
#define TTT_RETURN_TYPE 0x1
			/* When the type being traversed is a function type,
			   apply the predicate check to the return type. */
#define TTT_PARAM_TYPES 0x2
			/* When the type being traversed is a function type,
			   apply the predicate check to the parameter types. */
#define TTT_THIS_PARAM_TYPE 0x4
			/* When the type being traversed is a function type,
			   apply the predicate check to the implicit this
			   param type. */
#define TTT_MEMBER_TYPES 0x8
			/* When the type being traversed is a class type,
			   apply the predicate check to nested classes,
			   enums, and typedef names. */
#define TTT_TYPES_OF_MEMBER_FUNCTIONS 0x10
			/* When the type being traversed is a class type,
			   apply the predicate check to types of member
			   functions. */
#define TTT_TYPES_OF_DATA_MEMBERS 0x20
			/* When the type being traversed is a class type,
			   apply the predicate check to the types of data
			   members. */
#define TTT_BASE_CLASSES 0x40
			/* When the type being traversed is a class type,
			   apply the predicate check to its base classes. */
#define TTT_TEMPLATE_ARGS 0x80
			/* When the type being traversed is a class type,
			   apply the predicate check to its template args
                           (if it is a template class). */
#define TTT_SKIP_TYPEREFS 0x100
			/* Skip over typerefs before applying the predicate
			   check to a given type. */
#define TTT_SKIP_TYPEDEFS 0x200
			/* Skip over typedefs before applying the predicate
			   check to a given type. */

/* Type of service function called by traverse_type_tree to return TRUE or
   FALSE status regarding a given type in a type tree. */
typedef a_boolean a_type_predicate_function(a_type_ptr tp, a_boolean *flag);
typedef a_type_predicate_function *a_type_predicate_function_ptr;

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


/* Static variables used to pass information back to the routine
   is_or_contains_local_or_unnamed_type. */
static a_boolean is_unnamed_type;
static a_boolean is_local_type;
static a_boolean ttt_is_unnamed_or_local_type(
                                           a_type_ptr  type_ptr,
                                           a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  It returns TRUE if type_ptr is an unnamed or
local class, struct, union, or enum.  Typedefs will have been skipped, as
they in name mangling; it is the underlying type, not the typedef name
(which can be declared anywhere) that we really care about.
*/
{
  a_symbol_ptr  sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
  a_boolean result = FALSE;

  if (is_class_struct_union(type_ptr)) {
    if (is_unnamed_class_symbol(sym)) {
      is_unnamed_type = *force_end_of_traversal = result = TRUE;
    }  /* if */
  } else if (is_enum_type(type_ptr)) {
    if (sym == NULL) {
      is_unnamed_type = *force_end_of_traversal = result = TRUE;
    }  /* if */
  }  /* if */
  if (type_ptr->source_corresp.is_local_to_function) {
    check_assertion(type_ptr->kind != (a_type_kind)tk_typeref);
    is_local_type = *force_end_of_traversal = result = TRUE;
  }  /* if */
  return result;
}  /* ttt_is_unnamed_or_local_type */


/* A pointer to the specific template parameter type to be found by
   ttt_is_or_contains_template_param. */
static a_type_ptr specific_template_param_type;
/* A pointer to the specific template parameter constant to be found by
   ttt_contains_template_param_constant. */
static a_constant_ptr specific_template_param_constant;


static a_boolean ttt_contains_template_param_constant(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
{
  an_expr_node_ptr    count;
  a_template_arg_ptr  tap;
  a_boolean           found = FALSE;
  a_constant_ptr      cp;

  if (is_array(type_ptr)) {
    if (type_ptr->variant.array.is_variable_size_array) {
      count = type_ptr->variant.array.variant.element_count_expr;
      if (expr_tree_contains_template_param_constant(
                                        count,
                                        specific_template_param_constant)) {
        found = TRUE;
      }  /* if */
    }  /* if */
  } else if (is_class_struct_union(type_ptr)) {
    /* For nested classes only the outermost class can have template
       arguments.  Find the outermost class before doing the check. */
    while (type_ptr->source_corresp.class_of_which_a_member != NULL) {
      type_ptr = type_ptr->source_corresp.class_of_which_a_member;
    }  /* while */
    /* Examing each template argument, if any. */
    for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                         template_arg_list;
         tap != NULL;
         tap = tap->next) {
      if (!tap->is_type) {
        /* A non-type argument.  See if a template parameter constant is
           used -- e.g.,
             template <class T, int I> class A { B<I> *b; . . . };
           where the template argument for B<I> is template para constant I. */
        cp = tap->variant.constant;
        if (cp->kind == (a_constant_repr_kind)ck_template_param) {
          /* Only a ck_template_param constant can be or contain a template
             param constant, and it must. */
          if (specific_template_param_constant == NULL) {
            /* Any template param constant will do. */
            found = TRUE;
          } else if (cp->variant.template_param.kind ==
                        (a_template_param_constant_kind)tpck_expression) {
            /* Look for a particular template param constant in the expression
               tree. */
            if (expr_tree_contains_template_param_constant(
                                      cp->variant.template_param.variant.expr,
                                      specific_template_param_constant)) {
              found = TRUE;
            }  /* if */
          } else if (eq_constants(cp, specific_template_param_constant)) {
            /* Just compare the constant entries. */
            found = TRUE;
          }  /* if */
          /* Exit the loop once a match is found. */
          if (found) break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* switch */
  if (found) *force_end_of_traversal = TRUE;
  return found;
}  /* ttt_contains_template_param_constant */


static a_boolean ttt_is_or_contains_template_param(
                                       a_type_ptr  type_ptr,
                                       a_boolean   *force_end_of_traversal)
/*
This is a service function designed to be called from traverse_type_tree
(whence the ttt_ prefix).  If specific_template_param_type is NULL, it
returns TRUE if type_ptr is a template parameter type or is based on a
template parameter constant.  If specific_template_param_type is non-NULL,
it returns TRUE if type_ptr is the specified template parameter type.
*/
{
  a_boolean  found = FALSE;

  if (is_template_param(type_ptr)) {
    if (specific_template_param_type == NULL ||
        identical_types(type_ptr, specific_template_param_type)) {
      *force_end_of_traversal = found = TRUE;
    }  /* if */
  } else if (specific_template_param_type == NULL) {
    /* We are not looking for a specific template param type, so any
       template constant (e.g., appearing as an array bound) will also
       serve. */
    found = ttt_contains_template_param_constant(type_ptr,
                                                 force_end_of_traversal);
  }  /* if */
  return found;
}  /* ttt_is_or_contains_template_param */


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
      while (type_ptr->source_corresp.class_of_which_a_member != NULL) {
        type_ptr = type_ptr->source_corresp.class_of_which_a_member;
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
bound.
*/
{
  a_boolean   found = FALSE;
  a_type_ptr  tp;

  if (is_ptr_or_ref_type(type_ptr)) {
    tp = skip_typerefs(type_pointed_to(type_ptr));
    if (is_array(tp)) {
      if (!tp->variant.array.is_variable_size_array &&
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


static a_boolean traverse_type_tree(a_type_ptr                     type_ptr,
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
  a_boolean           force_end_of_traversal = FALSE;
  a_type_ptr          tp;
  a_template_arg_ptr  tap;
  a_boolean           status;

  if (type_ptr->kind == (a_type_kind)tk_typeref) {
    if (flags & TTT_SKIP_TYPEREFS) {
      type_ptr = f_skip_typerefs(type_ptr);
    } else if (flags & TTT_SKIP_TYPEDEFS) {
      type_ptr = skip_typedefs(type_ptr);
    }  /* if */
  }  /* if */
  status = func(type_ptr, &force_end_of_traversal);
  if (force_end_of_traversal) {
    /* The function has determined that no further traversal is appropriate;
       return the current status to the caller. */
  } else {
    /* Traverse the tree. */
    switch (type_ptr->kind) {
      case tk_error:
      case tk_void:
      case tk_float:
      case tk_unknown:
        /* Leaf nodes -- no further traversal required. */
        break;
      case tk_integer:
	/* Integer type -- if this is an enumeration we need to check
	   enclosing classes. */
        if (type_ptr->variant.integer.enum_type &&
	    type_ptr->source_corresp.class_of_which_a_member != NULL) {
	  goto check_enclosing_classes;
        }  /* if */
	break;
      case tk_pointer:
        tp = type_ptr->variant.pointer.type;
        status = traverse_type_tree(tp, func, flags);
        break;
      case tk_routine:
        /* Conditional traversal of contained types. */
        if (flags & TTT_RETURN_TYPE) {
          tp = type_ptr->variant.routine.return_type;
          if (traverse_type_tree(tp, func, flags)) {
            status = TRUE;
            break;
          }  /* if */
        }  /* if */
        if (flags & TTT_THIS_PARAM_TYPE) {
          tp = type_ptr->variant.routine.extra_info->implicit_this_param_type;
          if (tp != NULL && traverse_type_tree(tp, func, flags)) {
            status = TRUE;
            break;
          }  /* if */
        }  /* if */
        if (flags & TTT_PARAM_TYPES) {
          a_param_type_ptr  ptp;
          for (ptp = type_ptr->variant.routine.extra_info->param_type_list;
               ptp != NULL;
               ptp = ptp->next) {
            tp = ptp->type;
            if (traverse_type_tree(tp, func, flags)) {
              status = TRUE;
              break;
            }  /* if */
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
      case tk_class:
      case tk_struct:
      case tk_union:
#if 0
        /* Any code for the general class/struct/union case should go
	   here. */
#endif /* 0 */
check_enclosing_classes:
        /* Conditional traversal of contained types. */
        if (flags & TTT_TEMPLATE_ARGS) {
	  /* For nested classes only the outermost class can have
	     template arguments.  Find the outermost class before doing
	     the check. */
          while (type_ptr->source_corresp.class_of_which_a_member != NULL) {
            type_ptr = type_ptr->source_corresp.class_of_which_a_member;
          }  /* while */
          for (tap = type_ptr->variant.class_struct_union.extra_info->
                                                          template_arg_list;
               tap != NULL;
               tap = tap->next) {
            if (tap->is_type) {
              tp = tap->variant.type;
              if (traverse_type_tree(tp, func, flags)) {
                status = TRUE;
                break;
              }  /* if */
            }  /* if */
          }  /* for */
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
      case tk_template_param:
        /* "Member" template params (e.g., T::X) should have a pointer to a
           parent class. */
        tp = type_ptr->source_corresp.class_of_which_a_member;
        check_assertion((tp != NULL) ==
                        (type_ptr->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_member));
        if (tp != NULL) {
          tp = symbol_supplement_for_class(tp)->template_param_for_proxy_class;
          if (tp != NULL) {
            status = traverse_type_tree(tp, func, flags);
          }  /* if */
        }  /* if */
        break;
#if CHECKING
      default:
        internal_error("traverse_type_tree: bad type kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  return status;
}  /* traverse_type_tree */


a_boolean is_or_contains_local_type(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a local class, struct,
union or enum type or is a type tree containing such a type.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_THIS_PARAM_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_SKIP_TYPEREFS);

  return (traverse_type_tree(type_ptr, ttt_is_local_type, ttt_flags));
}  /* is_or_contains_local_type */


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
                                               TTT_SKIP_TYPEREFS);

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


a_boolean is_or_contains_template_param(a_type_ptr  type_ptr)
/*
Return TRUE if the type pointed to by type_ptr is itself a tk_template_param
type entry or is a type tree containing such a type or a type containing
a template parameter constant.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_TEMPLATE_ARGS);

  /* Setting these pointers to NULL indicates that any template param type
     or constant will do. */
  specific_template_param_type = NULL;
  specific_template_param_constant = NULL;
  return (traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
          ttt_flags));
}  /* is_or_contains_template_param */


void set_type_involves_template_param_flags(a_type_ptr  rout_type)
/*
Go through the parameters for rout_type, which is assumed to be a function
type.  If any of the associated types involves a template parameter, mark
the param type entry; this is useful for function arg matching.
*/
{
  a_param_type_ptr  ptp;

  check_assertion(is_function_type(rout_type));
  ptp = skip_typerefs(rout_type)->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (is_or_contains_template_param(ptp->type)) {
      ptp->type_involves_template_param = TRUE;
    }  /* if */
  }  /* for */
}  /* set_type_involves_template_param_flags */


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
  return (traverse_type_tree(type_ptr, ttt_is_or_contains_template_param,
          ttt_flags));
}  /* is_or_contains_specific_template_param */


#if 0
/* The following is not needed until support for nontype template parameters
   on function templates is added. */
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
  return (traverse_type_tree(tp, ttt_contains_template_param_constant,
                             ttt_flags));
}  /* type_contains_specific_template_param_constant */
#endif /* if 0 */


void set_force_external_linkage_flag(a_type_ptr  type_ptr)
/*
Set the force_external_linkage flag in the symbol associated with any
class or enum type contained in type_ptr.
*/
{
  a_type_tree_traversal_flag_set  ttt_flags = (TTT_SKIP_TYPEDEFS |
                                               TTT_RETURN_TYPE |
                                               TTT_PARAM_TYPES |
                                               TTT_THIS_PARAM_TYPE);

  (void)traverse_type_tree(type_ptr, ttt_set_force_external_linkage_flag,
                           ttt_flags);
}  /* set_force_external_linkage_flag */


void set_used_in_exception_flag(a_type_ptr  type_ptr)
/*
Set the used_in_exception flag in type_ptr and traverse its type tree to
set the force_external_linkage flag for each class and enum type in the tree.
*/
{
  if (type_ptr->used_in_exception) {
    /* Already set.  No further action is required. */
  } else {
    type_ptr->used_in_exception = TRUE;
    set_force_external_linkage_flag(type_ptr);
  }  /* if */
}  /* set_used_in_exception_flag */


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


/* Type of service function called by traverse_and_modify_type_tree to return
   TRUE if the type was modified or FALSE if it was not. */
typedef a_boolean a_type_modifier_function(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type);
typedef a_type_modifier_function *a_type_modifier_function_ptr;


/*ARGSUSED*/ /* flags is not required but is part of the general interface. */
static a_boolean tmtt_strip_local_typedef(
                                    a_type_ptr                      type,
                                    a_type_tree_traversal_flag_set  flags,
                                    a_type_ptr                      *new_type)
/*
Strip local typedef from type, returning TRUE if a modification was done.
The modified type (or the original type if no modification was done) is
returned in *new_type.
*/
{
  *new_type = strip_local_typedefs(type);
  return (type != *new_type);
}  /* tmtt_strip_local_typedef */


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
  a_type_ptr              new_return_type, new_this_param_type;
  a_type_ptr              first_new_type_for_param_types_list;
  unsigned long           reusable_param_types;
  a_source_position       dummy_decl_pos;

  /* Traverse the tree. */
  switch (type->kind) {
    case tk_error:
    case tk_void:
    case tk_float:
    case tk_integer:
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
      new_this_param_type =
                 type->variant.routine.extra_info->implicit_this_param_type;
      if (new_this_param_type != NULL &&
          func(new_this_param_type, flags, &tp)) {
        new_this_param_type = tp;
        goto make_new_type;
      } else if (new_return_type != type->variant.routine.return_type) {
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
      new_type->variant.routine.extra_info->implicit_this_param_type =
                                                     new_this_param_type;
      /* Pass a NULL source position to make_param_type and to
         set_routine_calling_method to avoid inappropriate diagnostics on
         a type that doesn't correspond directly to a source construct. */
      dummy_decl_pos.seq = 0;
      dummy_decl_pos.column = SP_COL_UNKNOWN;
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
        new_ptp = make_param_type(tp, &dummy_decl_pos);
        if (ptp->has_default_arg) {
          new_ptp->has_default_arg = TRUE;
          if (!ptp->type_involves_template_param) {
            new_ptp->default_arg_expr =
                             duplicate_default_arg_expr(ptp->default_arg_expr);
          }  /* if */
        }  /* if */
        /* Recompute the value of the flag, if necessary. */
        new_ptp->type_involves_template_param =
              (ptp->type == tp) ? ptp->type_involves_template_param :
                                  is_or_contains_template_param(new_ptp->type);
        /* Add the new param type entry to the param types list. */
        if (prev_ptp == NULL) {
          new_type->variant.routine.extra_info->param_type_list = new_ptp;
        } else {
          prev_ptp->next = new_ptp;
        }  /* if */
        prev_ptp = new_ptp;
      }  /* if */
      set_routine_calling_method_flag(new_type, &dummy_decl_pos);
      break;
    case tk_array:
      /* Make an array type based on "type", making modifications as
         required in the element type.  Note that if the element type doesn't
         require modification, we don't create a new type entry. */
      if (func(type->variant.array.element_type, flags, &tp)) {
        /* Create a new array type. */
        new_type = alloc_type((a_type_kind)tk_array);
        *new_type = *type;
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
      if (tp != type->variant.ptr_to_member.type ||
          tp2 != type->variant.ptr_to_member.class_of_which_a_member) {
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


a_type_ptr strip_local_typedefs(a_type_ptr  type)
/*
If type contains one or more typedefs that are local to a function (whether
at the top level or embedded somewhere within the tree) remove them and
return the modified type to the caller.  If no modification is done return
the original type.
*/
{
  while (type->kind == (a_type_kind)tk_typeref &&
         type->source_corresp.is_local_to_function) {
    /* The top level type is a local typedef. */
    check_assertion(!typeref_is_qualified(type));
    type = type->variant.typeref.type;
  }  /* while */
  return traverse_and_modify_type_tree(type, tmtt_strip_local_typedef,
                                       TTT_NO_INPUT_FLAGS);
}  /* strip_local_typedefs */

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
