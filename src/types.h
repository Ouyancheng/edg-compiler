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

types.h -- Declarations related to types.c (having to do with types).

*/

/* Avoid including these declarations more than once: */
#ifndef TYPES_H
#define TYPES_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/* There are copies of this macro in il_display.c and in c_gen_be.c;
   if you change this, you should probably change those definitions
   as well. */
#define skip_typerefs(tp)                                             \
  ((tp)->kind != (a_type_kind)tk_typeref ? (tp) : f_skip_typerefs(tp))

extern a_type_ptr f_skip_typerefs(a_type_ptr type_ptr);

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
extern a_boolean is_scalar_type(a_type_ptr tp);
extern a_boolean is_array_type(a_type_ptr tp);
extern a_boolean is_char_array_type(a_type_ptr tp);
extern a_boolean is_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_complete_class_struct_union_type(a_type_ptr tp);
extern a_boolean is_aggregate_or_union_type(a_type_ptr tp);

extern a_type_ptr array_element_type(a_type_ptr array_type);
extern a_type_ptr pointer_referenced_type(a_type_ptr pointer_type);

#define is_const_qualified_type(tp)                                   \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_const_qualified_type(tp))
#define is_volatile_qualified_type(tp)                                \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_volatile_qualified_type(tp))
#define is_qualified_type(tp)                                         \
  ((tp)->kind == (a_type_kind)tk_typeref && f_is_qualified_type(tp))

extern a_boolean f_is_const_qualified_type(a_type_ptr tp);
extern a_boolean f_is_volatile_qualified_type(a_type_ptr tp);
extern a_boolean f_is_qualified_type(a_type_ptr tp);
extern a_boolean int_kind_is_signed(an_integer_kind kind);

extern void check_fixup_list_for_array_types(void);
extern void add_if_necessary_to_array_fixup_list(a_type_ptr array_type);
extern void set_type_size(a_type_ptr type_ptr);
extern a_type_ptr type_after_integral_promotion(a_type_ptr type);
extern a_type_ptr default_argument_promotion(a_type_ptr old_type);
#define identical_types(t1, t2) \
	 ((t1) == (t2) || f_identical_types((t1), (t2)))
extern a_boolean f_identical_types(a_type_ptr type_1,
                                   a_type_ptr type_2);
extern a_boolean interchangeable_types(a_type_ptr type_1,
                                       a_type_ptr type_2);
#define types_are_compatible(t1, t2) \
	 ((t1) == (t2) || f_types_are_compatible((t1), (t2)))
extern a_boolean f_types_are_compatible(a_type_ptr type_1,
                                        a_type_ptr type_2);
extern a_boolean types_pointed_to_are_compatible(
                                      a_type_ptr type_1,
                                      a_type_ptr type_2,
                                      a_boolean  ptrs_to_void_compatible);
extern a_type_ptr composite_type(a_type_ptr type_1,
                                 a_type_ptr type_2);
extern a_type_ptr make_file_scope_type(a_type_ptr type);

/*
Return the type of the variable (lvalue) represented by node.  This mainly
involves removing the extra "pointer to" in the expression type for
an lvalue.
*/
#define lvalue_expr_type(node)                                        \
(is_error_type((node)->type) ? (node)->type :                         \
                               pointer_referenced_type((node)->type))

/*
Return TRUE if a routine type is the type of a nonstatic member function.
*/
#define routine_type_is_nonstatic_member_function(routine_type)       \
 (skip_typerefs(routine_type)->variant.routine.extra_info->           \
                                       implicit_this_param_type != NULL)

#endif /* ifndef TYPES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
