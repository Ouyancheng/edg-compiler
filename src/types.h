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

/* There are copies of this macro in il_display.c and in c_gen_be.c;
   if you change this, you should probably change those definitions
   as well. */
#define skip_typerefs(tp)                                             \
  ((tp)->kind != (a_type_kind)tk_typeref ? (tp) : f_skip_typerefs(tp))

#define m_is_error_type(tp)                                           \
  (skip_typerefs(tp)->kind == (a_type_kind)tk_error)

extern a_type_ptr f_skip_typerefs(a_type_ptr type_ptr);
extern a_type_ptr skip_typedefs(a_type_ptr type_ptr);

extern a_boolean is_error_type(a_type_ptr tp);
extern a_boolean is_function_type(a_type_ptr tp);
extern a_boolean is_incomplete_type(a_type_ptr tp);
extern a_boolean is_object_type(a_type_ptr tp);
extern a_boolean is_void_type(a_type_ptr tp);
extern a_boolean is_integral_type(a_type_ptr tp);
extern a_boolean is_signed_integral_type(a_type_ptr tp);
extern a_boolean is_enum_type(a_type_ptr tp);
extern a_boolean is_character_type(a_type_ptr tp);
extern a_boolean is_floating_type(a_type_ptr tp);
extern a_boolean is_arithmetic_type(a_type_ptr tp);
extern a_boolean is_pointer_type(a_type_ptr tp);
extern a_boolean is_reference_type(a_type_ptr tp);
extern a_boolean is_ptr_or_ref_type(a_type_ptr tp);
extern a_boolean is_scalar_type(a_type_ptr tp);
extern a_boolean is_array_type(a_type_ptr tp);
extern a_boolean is_char_array_type(a_type_ptr tp);
extern a_boolean is_string_type(a_type_ptr tp);
extern a_boolean is_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_complete_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_union_type(a_type_ptr tp);
extern a_boolean is_aggregate_or_union_type(a_type_ptr tp);
extern a_boolean is_ptr_to_member_type(a_type_ptr tp);
extern a_boolean is_abstract_class_type(a_type_ptr tp);
extern a_boolean is_template_param_type(a_type_ptr tp);
extern a_boolean is_template_class_type(a_type_ptr tp);

extern a_type_ptr array_element_type(a_type_ptr array_type);
extern a_type_ptr underlying_array_element_type(a_type_ptr array_type);
extern a_type_ptr type_pointed_to(a_type_ptr pointer_type);
extern a_type_ptr pm_member_type(a_type_ptr pm_type);
extern a_type_ptr pm_class_type(a_type_ptr pm_type);

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
Check for type qualifiers.  In C++ this includes looking for qualifiers
on the underlying element type of an array.
*/
#define is_qualified_type(tp)                                         \
  (((tp)->kind == (a_type_kind)tk_typeref ||                          \
    (tp)->kind == (a_type_kind)tk_array) &&                           \
   f_is_qualified_type((tp), /*top_level=*/C_mode()))
#define is_const_qualified_type(tp)                                   \
  (((tp)->kind == (a_type_kind)tk_typeref ||                          \
    (tp)->kind == (a_type_kind)tk_array) &&                           \
   f_is_const_qualified_type((tp), /*top_level=*/C_mode()))
#define is_volatile_qualified_type(tp)                                \
  (((tp)->kind == (a_type_kind)tk_typeref ||                          \
    (tp)->kind == (a_type_kind)tk_array) &&                           \
   f_is_volatile_qualified_type((tp), /*top_level=*/C_mode()))
/*
Check for "top-level" type qualifiers -- i.e., don't look at the element
type if tp is an array.
*/
#define is_top_level_qualified_type(tp)                               \
  ((tp)->kind == (a_type_kind)tk_typeref &&                           \
   f_is_qualified_type((tp), /*top_level=*/TRUE))
#define is_top_level_const_qualified_type(tp)                         \
  ((tp)->kind == (a_type_kind)tk_typeref &&                           \
   f_is_const_qualified_type((tp), /*top_level=*/TRUE))
#define is_top_level_volatile_qualified_type(tp)                      \
  ((tp)->kind == (a_type_kind)tk_typeref &&                           \
   f_is_volatile_qualified_type((tp), /*top_level=*/TRUE))
/*
Return TRUE if the type qualifiers on two types match.  Typedefs and
the underlying types are ignored.  On an array type it is the element
type that is checked for qualifiers.
*/
#define type_qualifiers_match(tp1, tp2)                               \
  (is_const_qualified_type(tp1) == is_const_qualified_type(tp2) &&    \
   is_volatile_qualified_type(tp1) == is_volatile_qualified_type(tp2))

extern a_boolean f_is_const_qualified_type(a_type_ptr tp,
                                           a_boolean  top_level);
extern a_boolean f_is_volatile_qualified_type(a_type_ptr tp,
                                              a_boolean  top_level);
extern a_boolean f_is_qualified_type(a_type_ptr tp,
                                     a_boolean  top_level);

extern a_boolean is_on_any_derivation_of(a_base_class_ptr  bcp,
                                         a_base_class_ptr  ref_bcp);
extern a_base_class_ptr corresponding_base_class(
                                            a_base_class_ptr  base_class,
                                            a_type_ptr        new_class,
                                            a_base_class_ptr  disambiguator);

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
extern void set_type_size(a_type_ptr type_ptr);
extern a_type_ptr type_after_integral_promotion(a_type_ptr type);
extern a_type_ptr default_argument_promotion(a_type_ptr old_type);
extern a_type_ptr con_complete_object_type(a_constant_ptr constant);
extern a_type_ptr node_complete_object_type(an_expr_node_ptr node,
                                            a_boolean        call_case);
#define identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), /*il_identical=*/FALSE))
#define il_identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), /*il_identical=*/TRUE))
/* Compare one level of two array types. */
extern a_boolean f_identical_types(a_type_ptr type_1,
                                   a_type_ptr type_2,
                                   a_boolean  il_identical);
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
#define TCF_NO_FLAGS 0x0
typedef int a_type_compat_flags_set;
extern a_boolean param_types_are_compatible(a_type_ptr              rout_type1,
                                            a_type_ptr              rout_type2,
                                            a_type_compat_flags_set flags);
extern a_boolean f_types_are_compatible(a_type_ptr              type_1,
                                        a_type_ptr              type_2,
                                        a_type_compat_flags_set flags);
/* Three macros to be used in calling f_types_are_compatible, since they short
   circuit some of the processing in common cases.  Use types_are_compatible
   when an error type should be treated as compatible with any type; use
   types_are_strictly_compatible when an error type is incompatible with any
   type, including an error type; use types_are_compatible_ignoring_qualifiers
   to check compatibility while ignoring first-level qualifiers. */
#define types_are_compatible(t1, t2) \
	 ((t1) == (t2) ||            \
          f_types_are_compatible((t1), (t2),                          \
                                 TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING))
#define types_are_strictly_compatible(t1, t2)                             \
         ((t1) == (t2) ? !is_error_type(t1) :                             \
            f_types_are_compatible((t1), (t2), TCF_NO_FLAGS))
#define types_are_compatible_ignoring_qualifiers(t1, t2)              \
  ((t1) == (t2) ||                                                    \
   f_types_are_compatible((t1), (t2),                                 \
                          TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING |   \
                          TCF_IGNORE_TYPE_QUALIFIERS))


extern a_boolean same_type_with_added_qualifiers(a_type_ptr dest_type,
                                                 a_type_ptr source_type,
                                                 a_boolean  ignore_qualifiers);

extern a_boolean impl_pointer_conversion(
                                a_type_ptr    source_type,
                                a_boolean     source_is_constant,
                                a_constant    *source_constant,
                                a_type_ptr    dest_type,
                                a_boolean     check_as_operands_not_conversion,
                                a_boolean     *pointer_normalization_needed,
                                a_boolean     suppress_extensions,
                                an_error_code default_warning_code,
                                an_error_code *warning_suggested);
extern a_boolean impl_ptr_to_member_conversion(
                                  a_type_ptr source_type,
                                  a_boolean  source_is_constant,
                                  a_constant *source_constant,
                                  a_type_ptr dest_type,
                                  a_boolean  check_as_operands_not_conversion);
extern a_boolean impl_conversion_possible(a_type_ptr    source_type,
                                          a_boolean     source_is_constant,
                                          a_constant    *source_constant,
                                          a_type_ptr    dest_type,
                                          a_boolean     suppress_extensions,
                                          an_error_code default_warning_code,
                                          an_error_code *warning_suggested);
extern a_boolean expl_conversion_possible(a_type_ptr    source_type,
                                          a_boolean     source_is_constant,
                                          a_constant    *source_constant,
                                          a_type_ptr    dest_type,
                                          an_error_code default_warning_code,
                                          an_error_code *warning_suggested);
extern a_type_ptr composite_type(a_type_ptr type_1,
                                 a_type_ptr type_2);
extern a_boolean overload_distinguishable(a_symbol_ptr  old_sym_ptr,
                                          a_type_ptr    new_type,
                                          a_boolean     new_is_template,
                                          an_error_code *err_code);
extern a_boolean is_or_contains_local_type(a_type_ptr  type_ptr);
extern a_boolean is_or_contains_unnamed_or_local_type(a_type_ptr  type_ptr,
						      a_boolean	  *is_unnamed,
						      a_boolean   *is_local);
extern a_boolean is_or_contains_template_param(a_type_ptr  type_ptr);
extern void set_type_involves_template_param_flags(a_type_ptr  rout_type);
extern a_boolean is_or_contains_specific_template_param
						(a_type_ptr  type_ptr,
						 a_type_ptr  tparam_type);
#if 0
/* The following is not needed until support for nontype template parameters
   on function templates is added. */
extern a_boolean type_contains_specific_template_param_constant(
                                                         a_type_ptr     tp,
                                                         a_constant_ptr cp);
#endif /* if 0 */
extern void set_force_external_linkage_flag(a_type_ptr  type_ptr);
extern void set_used_in_exception_flag(a_type_ptr  type_ptr);
extern a_type_ptr strip_local_typedefs(a_type_ptr  type);

/*
Return TRUE if type_1 does not have some top-level type qualifier that
type_2 has.  Note that this macro does not check that the underlying
types are compatible.
*/
#define any_qualifier_missing(type_1, type_2)                         \
  ((is_const_qualified_type(type_2) && !is_const_qualified_type(type_1)) || \
   (is_volatile_qualified_type(type_2) &&                             \
                                    !is_volatile_qualified_type(type_1)))

/*
Return the type of the variable (lvalue) represented by node.  This mainly
involves removing the extra "pointer to" in the expression type for
an lvalue.
*/
#define lvalue_expr_type(node)                                        \
(is_error_type((node)->type) ? (node)->type :                         \
                               type_pointed_to((node)->type))

/*
Return TRUE if a routine type is the type of a nonstatic member function.
The type must be known to be a routine type (not, for example, an error type),
but it may have typerefs on top of it.
*/
#define routine_type_is_nonstatic_member_function(routine_type)       \
 (f_skip_typerefs(routine_type)->                                     \
          variant.routine.extra_info->implicit_this_param_type != NULL)

/*
Extract the "this" parameter type from a nonstatic member function type.
*/
#define implicit_this_param_type_of(routine_type)                     \
 (f_skip_typerefs(routine_type)->                                     \
          variant.routine.extra_info->implicit_this_param_type)

/*
Extract a pointer to a base classes list for a class type.  This macro
may be called only for class, struct, and union types and only in C++ mode.
*/
#define base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->base_classes)

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
