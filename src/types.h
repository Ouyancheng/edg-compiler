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

types.h -- Declarations related to types.c (having to do with types).

*/

/* Avoid including these declarations more than once: */
#ifndef TYPES_H
#define TYPES_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef EXPR_H
#include "expr.h"
#endif /* ifndef EXPR_H */

EXTERN a_boolean
		enum_type_is_integral /* = FALSE */;
			/* TRUE if an enum type is considered an integral
			   type.  Typically TRUE in C mode and FALSE in C++
			   mode. */

/* Strip typerefs off a type. */
#define skip_typerefs(tp)                                             \
  ((tp)->kind != (a_type_kind)tk_typeref ? (tp) : f_skip_typerefs(tp))

/* Fast macro version of is_error_type. */
#define m_is_error_type(tp)                                           \
  (skip_typerefs(tp)->kind == (a_type_kind)tk_error)

extern a_type_ptr f_skip_typerefs(a_type_ptr type_ptr);
extern a_type_ptr skip_typedefs(a_type_ptr type_ptr);

extern a_boolean is_error_type(a_type_ptr tp);
extern a_boolean is_function_type(a_type_ptr tp);
extern a_boolean is_incomplete_type(a_type_ptr tp);
extern a_boolean is_object_type(a_type_ptr tp);
extern a_boolean is_void_type(a_type_ptr tp);
extern a_boolean is_void_star_type(a_type_ptr tp);
extern a_boolean is_integral_type(a_type_ptr tp);
extern a_boolean is_signed_integral_type(a_type_ptr tp);
extern a_boolean is_enum_type(a_type_ptr tp);
extern a_boolean is_integral_or_enum_type(a_type_ptr tp);
extern a_boolean is_bool_type(a_type_ptr tp);
extern a_boolean is_character_type(a_type_ptr tp);
extern a_boolean is_floating_type(a_type_ptr tp);
#if C99_IL_EXTENSIONS_SUPPORTED
extern a_boolean is_nonreal_floating_type(a_type_ptr tp);
extern a_boolean is_imaginary_type(a_type_ptr tp);
extern a_boolean is_complex_type(a_type_ptr tp);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
extern a_boolean is_arithmetic_or_enum_type(a_type_ptr tp);
extern a_boolean is_pointer_type(a_type_ptr tp);
extern a_boolean is_reference_type(a_type_ptr tp);
extern a_boolean is_ptr_or_ref_type(a_type_ptr tp);
extern a_boolean is_scalar_type(a_type_ptr tp);
extern a_boolean is_array_type(a_type_ptr tp);
extern a_boolean is_vla_type(a_type_ptr tp);
extern a_boolean is_char_array_type(a_type_ptr tp);
#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean is_wchar_t_array_type(a_type_ptr tp);
extern a_boolean is_string_type(a_type_ptr tp);
#endif /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean is_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_union_type(a_type_ptr tp);
extern a_boolean is_aggregate_or_union_type(a_type_ptr tp);
extern a_boolean is_ptr_to_member_type(a_type_ptr tp);
extern a_boolean is_abstract_class_type(a_type_ptr tp);
extern a_boolean is_template_param_type(a_type_ptr tp);
extern a_boolean is_template_class_type(a_type_ptr tp);
extern a_boolean is_polymorphic_class_type(a_type_ptr tp);
extern a_boolean is_empty_class_type(a_type_ptr tp);

extern a_type_ptr array_element_type(a_type_ptr array_type);
extern a_type_ptr underlying_array_element_type(a_type_ptr array_type);
extern a_targ_size_t num_array_elements(a_type_ptr array_type);
extern a_type_ptr find_bottom_of_type(a_type_ptr type);
extern a_type_ptr type_pointed_to(a_type_ptr pointer_type);
extern a_type_ptr pm_member_type(a_type_ptr pm_type);
extern a_type_ptr pm_class_type(a_type_ptr pm_type);
extern a_type_ptr underlying_type_of_derived_type(a_type_ptr type);
#if BACK_END_IS_CP_GEN_BE
extern a_type_ptr type_specifier_of_type(a_type_ptr type);
#endif /* BACK_END_IS_CP_GEN_BE */

/*
Return TRUE if a type is a direct class type (i.e., not a typeref on
top of a class type).
*/
#define is_immediate_class_type(type)                                 \
  ((type)->kind == (a_type_kind)tk_class  ||                          \
   (type)->kind == (a_type_kind)tk_struct ||                          \
   (type)->kind == (a_type_kind)tk_union)

/*
Return TRUE if a type is a direct error type (i.e., not a typeref on
top of such a type).
*/
#define is_immediate_error_type(type) ((type)->kind == (a_type_kind)tk_error)

/*
Return TRUE if a type is a direct enum type (i.e., not a typeref on top of
an enum type).
*/
#define is_immediate_enum_type(type)                                  \
  ((type)->kind == (a_type_kind)tk_integer &&                         \
   (type)->variant.integer.enum_type)

#define is_unknown_type(tp) ((tp)->kind == (a_type_kind)tk_unknown)

/*
Return TRUE or FALSE about the qualifiers of a tk_typeref type.
*/
#define typeref_is_qualified(tp)                                      \
 ((tp)->variant.typeref.qualifiers != TQ_NONE)
#define typeref_is_const_qualified(tp)                                \
 (((tp)->variant.typeref.qualifiers & TQ_CONST) != 0)
#define typeref_is_volatile_qualified(tp)                             \
 (((tp)->variant.typeref.qualifiers & TQ_VOLATILE) != 0)
#define typeref_is_restrict_qualified(tp)                             \
 (((tp)->variant.typeref.qualifiers & TQ_RESTRICT) != 0)

/*
Macro that takes an array type and returns TRUE if its bound is specified but
unknown (e.g., a template parameter or a run-time quantity).
*/
#define has_unknown_specified_bound(array_type)                       \
  ((array_type)->variant.array.is_variable_size_array ||              \
   (array_type)->variant.array.is_template_dependent_size_array)

/*
Macro that returns TRUE if a type is a template class type that has
not been specialized.
*/
#define is_unspecialized_template_class(tp)				\
  (is_immediate_class_type(tp) &&					\
   (!(tp)->variant.class_struct_union.is_template_class ||		\
    ((tp)->variant.class_struct_union.is_specialized)))

/*
Return TRUE if a tk_typeref type represents a typedef name.  Note that this
is not identical to !typeref_is_qualified -- though typeref_is_qualified
and typeref_is_typedef can never be true at the same time -- since there
are cases in which typerefs are produced that are empty, with neither name
nor qualifier.
*/
#define typeref_is_typedef(tp)                                        \
 ((tp)->source_corresp.name != NULL)

/*
Return TRUE if ph points to a tk_typeref type that is a placeholder-for-
namespace-type that points to tp.
*/
#define is_assoc_namespace_type_placeholder(ph, tp)                   \
  ((ph)->kind == (a_type_kind)tk_typeref &&                           \
   (ph)->variant.typeref.is_placeholder_for_namespace_type &&         \
   (ph)->variant.typeref.type == (tp))

extern a_type_qualifier_set f_get_type_qualifiers(a_type_ptr  tp,
                                                  a_boolean   top_level);

#define get_type_qualifiers(tp)                                       \
  (((tp)->kind == (a_type_kind)tk_typeref ||                          \
    (tp)->kind == (a_type_kind)tk_array) ?                            \
      (f_get_type_qualifiers((tp), /*top_level=*/C_mode())) :         \
      (a_type_qualifier_set)TQ_NONE)

#define get_top_level_type_qualifiers(tp)                             \
  ((tp)->kind == (a_type_kind)tk_typeref ?                            \
      (f_get_type_qualifiers((tp), /*top_level=*/TRUE)) :             \
      (a_type_qualifier_set)TQ_NONE)

/*
Check for type qualifiers.  In C++ this includes looking for qualifiers
on the underlying element type of an array.
*/
#define is_qualified_type(tp)                                         \
  (get_type_qualifiers(tp) != TQ_NONE)
#define is_const_qualified_type(tp)                                   \
  ((get_type_qualifiers(tp) & TQ_CONST) != 0)
#define is_volatile_qualified_type(tp)                                \
  ((get_type_qualifiers(tp) & TQ_VOLATILE) != 0)

/*
Check for "top-level" type qualifiers -- i.e., don't look at the element
type if tp is an array.
*/
#define is_top_level_qualified_type(tp)                               \
  (get_top_level_type_qualifiers(tp) != TQ_NONE)

/*
Return TRUE if the type qualifiers on two types match.  Typedefs and
the underlying types are ignored.  On an array type it is the element
type that is checked for qualifiers.
*/
#define type_qualifiers_match(tp1, tp2)                               \
  (get_type_qualifiers(tp1) == get_type_qualifiers(tp2))

/*
Return TRUE if tp1_qualifiers does not have some type qualifier that
tp2_qualifiers has.
*/
#if NEAR_AND_FAR_ALLOWED
/* The "near" qualifier is backwards in that it's okay to remove it
   (producing "far") but not okay to add it, so flip it in the test. */
#define any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers)  \
  ((~((tp1_qualifiers) ^ TQ_NEAR) & ((tp2_qualifiers) ^ TQ_NEAR)) != 0)
#else /* !NEAR_AND_FAR_ALLOWED */
#define any_qualifier_in_set_missing(tp1_qualifiers, tp2_qualifiers)  \
  ((~(tp1_qualifiers) & (tp2_qualifiers)) != 0)
#endif /* NEAR_AND_FAR_ALLOWED */

/*
Return TRUE if tp1 does not have some type qualifiers that tp2 has.  Note
that this macro does not check that the underlying types are compatible.
*/
#define any_qualifier_missing(tp1, tp2)                               \
  (((tp2)->kind == (a_type_kind)tk_typeref ||                         \
    (tp2)->kind == (a_type_kind)tk_array) ?                           \
          f_any_qualifier_missing(tp1, tp2) : FALSE)

extern a_boolean f_any_qualifier_missing(a_type_ptr  tp1,
                                         a_type_ptr  tp2);
extern a_boolean is_qualified_version_of_array_typedef(
                                                a_type_ptr type,
                                                a_type_ptr *unqual_array_type);
#if NEAR_AND_FAR_ALLOWED
extern a_boolean is_far_type(a_type_ptr tp);
extern a_type_qualifier_set get_original_type_qualifiers(a_type_ptr type);
#endif /* NEAR_AND_FAR_ALLOWED */


extern a_boolean f_type_has_default_constructor(a_type_ptr  tp,
                                                a_boolean   user_declared_only,
                                                a_boolean   nontrivial_only);

/*
Return TRUE if tp is a class type (or array thereof) with a user-declared
default constructor.
*/
#define type_has_user_declared_default_constructor(tp)               \
  f_type_has_default_constructor(tp, /*user_declared_only=*/TRUE,    \
                                 /*nontrivial_only=*/FALSE)
/*
Return TRUE if tp is a class type (or array thereof) with a user-declared
default constructor or a nontrivial implicitly declared default constructor.
*/
#define type_has_nontrivial_default_constructor(tp)                  \
  f_type_has_default_constructor(tp, /*user_declared_only=*/FALSE,   \
                                 /*nontrivial_only=*/TRUE)


extern a_boolean is_on_any_derivation_of(a_base_class_ptr  bcp,
                                         a_base_class_ptr  ref_bcp);
extern a_base_class_ptr corresponding_base_class(
                                            a_base_class_ptr  base_class,
                                            a_type_ptr        new_class,
                                            a_base_class_ptr  disambiguator);

extern a_base_class_ptr find_base_class_of_full(
                                         a_type_ptr derived_class,
                                         a_type_ptr base_class,
                                         a_boolean  instantiate_if_necessary);
extern a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                           a_type_ptr base_class);
extern a_base_class_ptr find_direct_base_class_of(a_type_ptr  derived_class,
                                                  a_type_ptr  base_class_type);
extern a_boolean is_same_class_or_base_class_thereof(a_type_ptr class_1,
                                                     a_type_ptr class_2);
extern a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                          a_type_ptr       type_2,
                                          a_boolean        *baseward_cast,
                                          a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related class pointers.  If they
are, set *baseward_cast if type_1 --> type_2 is a baseward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_class_pointers(type_1, type_2, baseward_cast, bcp)    \
  (C_dialect == C_dialect_cplusplus &&                                \
   is_pointer_type(type_1) && is_pointer_type(type_2) &&              \
   f_related_class_pointers(type_1, type_2, baseward_cast, bcp))

extern a_boolean f_rel_member_pointers(a_type_ptr       type_1,
                                       a_type_ptr       type_2,
                                       a_boolean        *baseward_cast,
                                       a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related pointers to members.  If they
are, set *baseward_cast if type_1 --> type_2 is a baseward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_member_pointers(type_1, type_2, baseward_cast, bcp)   \
  (is_ptr_to_member_type(type_1) && is_ptr_to_member_type(type_2) &&  \
   f_rel_member_pointers(type_1, type_2, baseward_cast, bcp))

extern a_boolean type_masks_handler_param_type(a_type_ptr  type_1,
                                               a_type_ptr  type_2);
extern a_boolean set_array_type_size(a_type_ptr	array_type,
				     a_boolean		suppress_error);
extern void set_type_size(a_type_ptr type_ptr);
extern a_type_ptr type_after_integral_promotion(a_type_ptr type);
extern a_type_ptr default_argument_promotion(a_type_ptr old_type);
extern a_type_ptr con_complete_object_type(a_constant_ptr constant);
extern a_type_ptr node_complete_object_type(an_expr_node_ptr node,
                                            a_boolean        call_case);

/*
Bit vector used to pass flags into f_identical_types.
*/
typedef unsigned int an_itf_flag_set;

#define ITF_NO_FLAGS 0x0

#define ITF_IL_IDENTICAL 0x01
			/* If il_identical is TRUE, check only that the
			   types are identical from the point of view
			   of the IL.  Basically, two types are
			   IL-identical if no cast is needed to assign
			   a value of one type to an entity of the
			   other type. */

#define ITF_UNKNOWN_THIS_CLASS_TYPE 0x02
			/* TRUE if the this class type may not
			   be known yet.  When this flag is set, a
			   NULL "this" class type is ignored. */

#define identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), ITF_NO_FLAGS))
#define il_identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), ITF_IL_IDENTICAL))
#define unknown_this_class_identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), \
                                     ITF_UNKNOWN_THIS_CLASS_TYPE))

/* Compare one level of two array types. */
extern a_boolean f_identical_types(a_type_ptr      type_1,
                                   a_type_ptr      type_2,
                                   an_itf_flag_set flags);

extern a_boolean integral_types_the_same_except_for_signedness(
                                                            a_type_ptr type_1,
                                                            a_type_ptr type_2);
extern a_boolean interchangeable_types(a_type_ptr type_1,
                                       a_type_ptr type_2);
extern a_boolean this_param_types_correspond(a_type_ptr rout_type_1,
                                             a_type_ptr rout_type_2,
                                             a_boolean  check_as_conversion,
                                             a_boolean  check_as_operands);
/*
Bit flags for calls of f_types_are_compatible et al.
*/
#define TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING 0x1
			/* An error type is considered compatible with
			   anything. */
#define TCF_IGNORE_TYPE_QUALIFIERS 0x2
			/* Ignore type qualifiers at the first level.  In C++,
			   this includes qualifiers on array element types. */
#define TCF_REDECLARATION 0x4
			/* This is a top-level compatibility check for a
			   redeclaration.  It's important in C++ because it's
			   the only context in which known- and unknown-bound
			   array types are "compatible" (WP 3.5). */
#define TCF_IGNORE_CALLING_CONVENTIONS 0x8
			/* Ignore the calling conventions implied by name
			   linkage specified on top-level function types. */
#if MICROSOFT_EXTENSIONS_ALLOWED
			/* Also ignore Microsoft style calling convention
			   specifications. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define TCF_IMPLICIT_CONVERSION 0x10
			/* The conversion appears in the context of an
			   implicit conversion, which (in C++) may affect how
			   how routine linkage compatibility is determined. */
#define TCF_DONT_IGNORE_PARAM_TYPE_QUALIFIERS 0x20
			/* Parameter types should be regarded as incompatible
			   when their top-level type qualifiers differ, even
			   when remove_qualifiers_from_param_types is TRUE.
			   This flag is used in Microsoft-bugs mode only,
			   to deal with a bug in checking for overriding
			   virtual functions. */
#define TCF_IGNORE_PTR_TO_MEMBER_CLASS_TYPE 0x40
			/* Two pointer-to-member types are deemed compatible
			   as long as the member-types match -- no check
			   should be done for the class-types.  This flag is
			   used in Microsoft-bugs mode only, to deal with a
			   bug in redeclaration of static data members. */
#define TCF_IGNORE_THIS_CLASS_TYPE 0x80
			/* Two function types are deemed compatible even if
			   the this class types do not match.
			   This flag is used in Microsoft-bugs mode only, to
			   deal with a bug in redeclaration of static data
			   members. */

#define TCF_NO_FLAGS 0x0
typedef int a_type_compat_flags_set;

extern a_boolean param_types_are_compatible(a_type_ptr              rout_type1,
                                            a_type_ptr              rout_type2,
                                            a_type_compat_flags_set flags);
#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean calling_conventions_are_compatible(a_type_ptr type1,
                                                    a_type_ptr type2);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern a_boolean f_types_are_compatible(a_type_ptr              type_1,
                                        a_type_ptr              type_2,
                                        a_type_compat_flags_set flags);
/*
Several macros to be used in calling f_types_are_compatible, since they short
circuit some of the processing in common cases.
*/
/* Use types_are_compatible when an error type should be treated as compatible
   with any type. */
#define types_are_compatible(t1, t2)                                  \
	 ((t1) == (t2) ||                                             \
          f_types_are_compatible((t1), (t2),                          \
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING))
/* Use types_are_redecl_compatible for special handling in C++ of known- and
   unknown-bound arrays; otherwise it's the same as types_are_compatible. */
#define types_are_redecl_compatible(t1, t2)                           \
	 ((t1) == (t2) ||                                             \
          f_types_are_compatible((t1), (t2),                          \
                                 TCF_REDECLARATION |                  \
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING))
/* Use types_are_strictly_compatible when an error type is incompatible with
   any type, including an error type. */
#define types_are_strictly_compatible(t1, t2)                         \
         ((t1) == (t2) ? !is_error_type(t1) :                         \
            f_types_are_compatible((t1), (t2), TCF_NO_FLAGS))
/* Use types_are_compatible_ignoring_qualifiers to check compatibility while
   ignoring first-level qualifiers. */
#define types_are_compatible_ignoring_qualifiers(t1, t2)              \
  ((t1) == (t2) ||                                                    \
   f_types_are_compatible((t1), (t2),                                 \
                          TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |   \
                          TCF_IGNORE_TYPE_QUALIFIERS))
/* Use routine_types_are_compatible to check types of routines, ignoring
   top-level calling convention modifiers. */
#define routine_types_are_compatible(t1, t2, extra_flags)             \
         ((t1) == (t2) ||                                             \
          f_types_are_compatible((t1), (t2),                          \
                       TCF_IGNORE_CALLING_CONVENTIONS | (extra_flags)))

#define types_are_compatible_for_impl_conversion(t1, t2)              \
  ((t1) == (t2) ||                                                    \
   f_types_are_compatible((t1), (t2),                                 \
                          TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |   \
                          TCF_IGNORE_TYPE_QUALIFIERS |                \
                          TCF_IMPLICIT_CONVERSION))

extern a_boolean is_address_of_string_constant(a_constant *constant);

extern a_boolean same_type_with_added_qualifiers
                                    (a_type_ptr source_type,
				     a_type_ptr dest_type,
				     a_boolean  ignore_qualifiers,
				     a_boolean  *p_qualifiers_added);

extern
a_boolean qualification_conversion_possible(a_type_ptr source_type,
					    a_type_ptr dest_type,
					    a_boolean  *p_qualifiers_added,
                                            a_boolean  ignore_underlying_type);
extern
a_boolean cast_removes_qualifiers(a_type_ptr	source_type,
				  a_type_ptr	dest_type);

/*
Description of a standard conversion (implicit or explicit), or at least
of information relating to such a conversion that's non-trivial to compute.
*/
typedef struct a_std_conv_descr *a_std_conv_descr_ptr;
typedef struct a_std_conv_descr {
  a_base_class_ptr
		cast_base_class;
			/* If the standard conversion is a related-class cast,
			   this is the base class entry for it.  Otherwise,
			   NULL. */
  a_byte_boolean
		reversed_cast;
			/* If TRUE, cast_base_class describes the
			   reverse of the cast performed.  Used for
			   conversions of pointers to members to
			   pointers to members of derived classes. */
  a_byte_boolean
		type_qualifiers_added;
			/* TRUE if type qualifiers were added under a pointer
			   or reference.  Serves as a tie-breaker in
			   overload resolution. */
  a_byte_boolean
		pointer_normalization_needed;
			/* TRUE if the conversion involves converting a
			   null pointer constant to a pointer type or a
			   pointer type to "void *". */
  a_byte_boolean
		nontrivial_conversion;
			/* TRUE if the conversion is, in the terms of
			   overload resolution (ARM 13.2), more than just
			   a sequence of trivial conversions. */
  a_byte_boolean
		promotion;
			/* TRUE if the conversion is a promotion, e.g.,
			   short --> int.  Only set in C++ mode. */
  a_byte_boolean
		ptr_or_pm_to_bool;
			/* TRUE if this conversion is from a pointer or
			   pointer to member to bool. */
  a_byte_boolean
		conv_failed_because_of_exception_specifications;
			/* TRUE if the conversion could not be done because
			   of an incompatibility of exception
			   specifications. */
  a_byte_boolean
		conv_of_string_literal_to_ptr_to_nonconst;
			/* TRUE if the conversion is the deprecated conversion
			   of a string literal to "char *", or a wide string
			   literal to "wchar_t *". */
  an_error_code	warning_suggested;
			/* If not ec_no_error, the code for a warning to be
			   issued if this conversion is done. */
} a_std_conv_descr;


extern void clear_std_conv_descr(a_std_conv_descr_ptr std_conv);
extern a_boolean exception_spec_is_less_restrictive(a_type_ptr  type1,
                                                    a_type_ptr  type2);
extern a_boolean impl_pointer_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_boolean            source_is_string_literal,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_boolean            suppress_extensions,
                         an_error_code        default_warning_code,
                         a_std_conv_descr_ptr std_conv);
extern a_boolean impl_ptr_to_member_conversion(
                         a_type_ptr           source_type,
                         a_boolean            source_is_constant,
                         a_constant           *source_constant,
                         a_type_ptr           dest_type,
                         a_boolean            allow_qualifier_or_eh_mismatch,
                         a_std_conv_descr_ptr std_conv);
extern a_boolean impl_conversion_possible(
                          a_type_ptr           source_type,
                          a_boolean            source_is_constant,
                          a_boolean            source_is_string_literal,
                          a_constant           *source_constant,
                          a_type_ptr           dest_type,
                          a_boolean            allow_qualifier_or_eh_mismatch,
                          a_boolean            suppress_extensions,
                          an_error_code        default_warning_code,
                          a_std_conv_descr_ptr std_conv);
extern a_boolean conversion_allowed_for_nontype_template_argument(
                                                 a_std_conv_descr *conversion);
extern a_boolean static_cast_conversion_possible(
                                 a_type_ptr    source_type,
                                 a_boolean     source_is_constant,
                                 a_boolean     source_is_string_literal,
                                 a_constant    *source_constant,
                                 a_type_ptr    dest_type,
                                 a_boolean     allow_qualifier_or_eh_mismatch,
                                 an_error_code default_warning_code,
                                 an_error_code *warning_suggested);
extern a_boolean reinterpret_cast_conversion_possible(
                                             a_type_ptr    source_type,
                                             a_type_ptr    dest_type,
                                             an_error_code *warning_suggested);
extern a_boolean expl_conversion_possible(
                                        a_type_ptr    source_type,
                                        a_boolean     source_is_constant,
                                        a_boolean     source_is_string_literal,
                                        a_constant    *source_constant,
                                        a_type_ptr    dest_type,
                                        a_boolean     *reinterpret_cast_needed,
                                        an_error_code default_warning_code,
                                        an_error_code *warning_suggested);
extern a_type_ptr multilevel_composite_pointer_type(a_type_ptr type_1,
                                                    a_type_ptr type_2);
extern a_type_ptr composite_type(a_type_ptr type_1,
                                 a_type_ptr type_2);
extern
a_boolean overload_distinguishable(a_symbol_ptr		old_sym_ptr,
                                   a_type_ptr		new_type,
				   a_template_param_ptr	templ_param_list,
                                   an_error_code	*err_code);
extern a_boolean is_or_contains_error_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_local_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_unnamed_or_local_type(a_type_ptr  type_ptr,
						      a_boolean	  *is_unnamed,
						      a_boolean   *is_local);
extern a_boolean contains_type_with_no_name_linkage(a_type_ptr type_ptr);
extern a_boolean is_template_dependent_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_template_param(a_type_ptr  type_ptr);
extern void set_type_involves_deduced_template_param(a_type_ptr  rout_type);
extern a_boolean is_or_contains_specific_template_param
						(a_type_ptr  type_ptr,
						 a_type_ptr  tparam_type);
extern a_boolean type_contains_specific_template_template_param(
					a_type_ptr	type_ptr,
					a_template_ptr	tparam_template);
extern a_boolean type_contains_specific_template_param_constant(
                                                         a_type_ptr     tp,
                                                         a_constant_ptr cp);
extern a_boolean could_be_dependent_class_type(a_type_ptr tp);

/*
Alias for could_be_dependent_class_type, representing another view of the
same test: template parameter type or nonreal class type.
*/
#define is_template_param_or_nonreal_class_type(tp) \
  could_be_dependent_class_type(tp)

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
extern a_boolean is_or_contains_member_of_uncompleted_class(a_type_ptr  tp);
extern a_boolean template_args_involve_specific_class_type(
                                             a_template_arg_ptr  tap,
                                             a_type_ptr          class_type,
                                             a_boolean           members_only);
extern a_boolean type_involves_specific_class_type(a_type_ptr  tp,
                                                   a_type_ptr  class_type,
                                                   a_boolean   members_only);
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void set_force_external_linkage_flag(a_type_ptr  type_ptr);
extern void set_used_in_exception_or_rtti_flag(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_ptr_or_ref_to_unknown_bound_array(
                                                           a_type_ptr tp,
                                                           a_boolean  *is_ref);
extern a_boolean is_or_contains_vla_type_with_unspecified_bound(a_type_ptr tp);
extern a_boolean is_variably_modified_type(a_type_ptr  tp);
extern a_boolean is_directly_variably_modified_type(a_type_ptr  tp);
extern a_type_ptr strip_local_and_nonreal_typedefs(a_type_ptr  type);
extern a_type_ptr remove_assoc_vla_dimensions(a_type_ptr  type);

extern a_boolean routine_linkages_are_compatible(
                                           a_name_linkage_kind  nlk1,
                                           a_name_linkage_kind  nlk2,
                                           a_boolean            is_impl_conv);

/*
Return TRUE if a routine type is the type of a nonstatic member function.
The type must be known to be a routine type (not, for example, an error type),
but it may have typerefs on top of it.
*/
#define routine_type_is_nonstatic_member_function(routine_type)       \
 (f_skip_typerefs(routine_type)->                                     \
          variant.routine.extra_info->this_class != NULL)

extern a_type_ptr f_implicit_this_param_type_of(a_type_ptr  routine_type);

/*
Synthesize the type of the implicit "this" parameter of a routine if the
underlying class exists (nonstatic members and pointer-to-members); otherwise
NULL.
*/
#define implicit_this_param_type_of(rout_type)                         \
  (routine_type_is_nonstatic_member_function(rout_type) ?              \
                              f_implicit_this_param_type_of(rout_type) : NULL)

/*
Extract a pointer to a base classes list for a class type.  This macro
may be called only for class, struct, and union types and only in C++ mode.
*/
#define base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->base_classes)

#if PROTOTYPE_INSTANTIATIONS_IN_IL
/*
Extract the pointer to the template that generated a class type.
*/
#define assoc_template_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->assoc_template)
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */

/*
Macro that extracts the underlying enum type from an integral type,
or NULL if there is no underlying enum type.  Used for enum
compatibility checking.  The type must be an integral type, and
not even a typeref on top of an integral type.
*/
#define underlying_enum_type(tp)                                      \
  ((tp)->variant.integer.enum_type ?                                  \
          (tp) :                                                      \
          (tp)->variant.integer.enum_info.affiliated_type)


/* Bit vector used to pass flags into traverse_type_tree.  Each bit
   represents a flag. */
typedef int a_type_tree_traversal_flag_set;
/* Constants defining bits in the input bit vector used in calls to
   traverse_type_tree. */
#define TTT_NO_INPUT_FLAGS 0x0
#define TTT_RETURN_TYPE 0x1
			/* When the type being traversed is a function type,
			   apply the predicate check to the return type. */
#define TTT_PARAM_TYPES 0x2
			/* When the type being traversed is a function type,
			   apply the predicate check to the parameter types. */
#define TTT_THIS_PARAM_TYPE 0x4
			/* When the type being traversed is a function type,
			   apply the predicate check to the implicit "this"
			   parameter type. */
#define TTT_TEMPLATE_ARGS 0x8
			/* When the type being traversed is a class type,
			   apply the predicate check to its template args
                           (if it is a template class). */
#define TTT_SKIP_TYPEREFS 0x10
			/* Skip over typerefs before applying the predicate
			   check to a given type. */
#define TTT_SKIP_TYPEDEFS 0x20
			/* Skip over typedefs before applying the predicate
			   check to a given type. */
#define TTT_EXCEPTION_SPECS 0x40
			/* When the type being traversed is a function type,
			   apply the predicate check to the exception
			   specification list. */
#define TTT_STOP_AT_TYPEDEFS 0x80
			/* When the type encountered is a typedef, stop
			   the traversal. */
#define TTT_DEDUCED_CONTEXTS_ONLY 0x100
			/* When the type is traversed, only consider contexts
			   in which a template argument value can be deduced.
			   This ignores template parameters used in
			   the parent classes of a type (e.g., ignore
			   the T in A<T>::B) and nontype template
			   parameters used in expression contexts. */

/* Type of service function called by traverse_type_tree to return TRUE or
   FALSE status regarding a given type in a type tree. */
typedef a_boolean a_type_predicate_function(a_type_ptr tp, a_boolean *flag);
typedef a_type_predicate_function *a_type_predicate_function_ptr;

a_boolean traverse_type_tree(a_type_ptr                     type_ptr,
                             a_type_predicate_function_ptr  func,
                             a_type_tree_traversal_flag_set flags);

#endif /* ifndef TYPES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
