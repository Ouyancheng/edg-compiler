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
extern a_boolean is_aggregate_or_union_type(a_type_ptr tp);
extern a_boolean is_ptr_to_member_type(a_type_ptr tp);
extern a_boolean is_illegal_abstract_class_type(a_type_ptr tp);

extern a_type_ptr array_element_type(a_type_ptr array_type);
extern a_type_ptr underlying_array_element_type(a_type_ptr array_type);
extern a_type_ptr type_pointed_to(a_type_ptr pointer_type);

/*
Return TRUE if a type is a direct class type (i.e., not a typeref on
top of a class type).
*/
#define is_immediate_class_type(type)                                 \
  ((type)->kind == (a_type_kind)tk_class  ||                          \
   (type)->kind == (a_type_kind)tk_struct ||                          \
   (type)->kind == (a_type_kind)tk_union)

#define is_const_qualified_type(tp)                                   \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_const_qualified_type(tp))
#define is_volatile_qualified_type(tp)                                \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_volatile_qualified_type(tp))
#define is_qualified_type(tp)                                         \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_qualified_type(tp))
#define type_or_element_type_is_const_qualified(tp)                   \
  (is_const_qualified_type(tp) ||                                     \
   (is_array_type(tp) &&                                              \
    is_const_qualified_type(underlying_array_element_type(tp))))
/* Return TRUE if the type qualifiers on two types match.  Typedefs and
   the underlying types are ignored. */
#define type_qualifiers_match(type_1, type_2)                         \
  (is_const_qualified_type(type_1) == is_const_qualified_type(type_2) && \
   is_volatile_qualified_type(type_1) == is_volatile_qualified_type(type_2))

extern a_boolean f_is_const_qualified_type(a_type_ptr tp);
extern a_boolean f_is_volatile_qualified_type(a_type_ptr tp);
extern a_boolean f_is_qualified_type(a_type_ptr tp);
extern a_boolean int_kind_is_signed(an_integer_kind kind);

extern a_base_class_ptr find_base_class_of(a_type_ptr derived_class,
                                           a_type_ptr base_class);
extern a_boolean is_same_class_or_base_class_thereof(a_type_ptr class_1,
                                                     a_type_ptr class_2);
extern a_boolean f_related_class_pointers(a_type_ptr       type_1,
                                          a_type_ptr       type_2,
                                          a_boolean        *downward_cast,
                                          a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related class pointers.  If they
are, set *downward_cast if type_1 --> type_2 is a downward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_class_pointers(type_1, type_2, downward_cast, bcp)    \
  (C_dialect == C_dialect_cplusplus &&                                \
   is_pointer_type(type_1) && is_pointer_type(type_2) &&              \
   f_related_class_pointers(type_1, type_2, downward_cast, bcp))

extern a_boolean f_rel_member_pointers(a_type_ptr       type_1,
                                       a_type_ptr       type_2,
                                       a_boolean        *downward_cast,
                                       a_base_class_ptr *bcp);
/*
Return TRUE if type_1 and type_2 are related pointers to members.  If they
are, set *downward_cast if type_1 --> type_2 is a downward cast, and
set *bcp to point to the base class entry that shows the relationship.
*/
#define related_member_pointers(type_1, type_2, downward_cast, bcp)   \
  (is_ptr_to_member_type(type_1) && is_ptr_to_member_type(type_2) &&  \
   f_rel_member_pointers(type_1, type_2, downward_cast, bcp))

extern void check_fixup_list_for_array_types(void);
extern void add_if_necessary_to_array_fixup_list(a_type_ptr array_type);
extern void set_type_size(a_type_ptr type_ptr);
extern a_type_ptr type_after_integral_promotion(a_type_ptr type);
extern a_type_ptr default_argument_promotion(a_type_ptr old_type);
extern a_type_ptr con_complete_object_type(a_constant_ptr constant);
extern a_type_ptr node_complete_object_type(an_expr_node_ptr node);
#define identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), /*il_identical=*/FALSE))
#define il_identical_types(t1, t2) \
  ((t1) == (t2) || f_identical_types((t1), (t2), /*il_identical=*/TRUE))
extern a_boolean f_identical_types(a_type_ptr type_1,
                                   a_type_ptr type_2,
                                   a_boolean  il_identical);
extern a_boolean interchangeable_types(a_type_ptr type_1,
                                       a_type_ptr type_2);
extern a_boolean arg_types_are_compatible(a_type_ptr  rout_type1,
                                          a_type_ptr  rout_type2);
#define types_are_compatible(t1, t2) \
	 ((t1) == (t2) || f_types_are_compatible((t1), (t2)))
extern a_boolean f_types_are_compatible(a_type_ptr type_1,
                                        a_type_ptr type_2);
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
                                          an_error_code *err_code);
extern a_type_ptr make_file_scope_type(a_type_ptr old_type);


/*
Return TRUE if type_1 does not have some top-level type qualifier that
type_2 has.  Note that this macro does not check that the underlying
types are compatible.
*/
#define fewer_qualifiers(type_1, type_2)                              \
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
The type must be known to be a routine type (not, for example, an error type).
*/
#define routine_type_is_nonstatic_member_function(routine_type)       \
 (routine_type->variant.routine.extra_info->implicit_this_param_type != NULL)

/*
Extract the class type from a nonstatic member function type.
*/
#define class_type_from_nonstatic_member_function_type(routine_type)  \
  (f_skip_typerefs(type_pointed_to(                                   \
    (routine_type)->variant.routine.extra_info->implicit_this_param_type)))

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
