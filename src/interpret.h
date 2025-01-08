/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

interpret.h -- Interface to IL interpreter for constexpr functions

*/
/* Avoid including these declarations more than once: */
#ifndef INTERPRET_H
#define INTERPRET_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

a_subobject_path_ptr* last_subobject_path_link(a_constant_ptr  con);

void decay_subobject_path(a_constant_ptr  con);

a_subobject_path_ptr get_trailing_subobject_path_entry(
                                               a_constant_ptr  con,
                                               a_boolean       is_offset,
                                               a_boolean       is_base_class);

a_boolean interpret_clang_enable_if_opnd(
                                       an_expr_node_ptr              expr,
                                       Dyn_array<a_constant*> const  &params,
                                       a_source_position             *pos,
                                       a_boolean                     *p_value);

a_boolean is_core_constant_expr(an_expr_node_ptr  expr,
                                a_diag_list_ptr   diag_list);

a_boolean interpret_expr(an_expr_node_ptr  expr,
                         a_boolean         is_constant_evaluated,
                         a_boolean         force_prvalue,
                         a_constant_ptr    result_con,
                         a_diag_list_ptr   diag_list);

a_routine_ptr get_constexpr_callee(an_expr_node_ptr  call_expr,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_constexpr_call(an_expr_node_ptr  call_expr,
                                   a_boolean         is_constant_evaluated,
                                   a_constant_ptr    result_con,
                                   a_diag_list_ptr   diag_list);

a_boolean interpret_dynamic_init_full(
                                  a_dynamic_init_ptr  dip,
                                  a_source_position   *pos,
                                  a_type_ptr          result_type,
                                  a_boolean           is_constant_evaluated,
                                  a_constant_ptr      result_con,
                                  a_diag_list_ptr     diag_list,
                                  a_boolean           allow_reinterpret_cast);

#define interpret_dynamic_init(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list)                        \
  (interpret_dynamic_init_full(dip, pos, result_type, is_constant_evaluated, \
                               result_con, diag_list, FALSE))

a_boolean interpret_constexpr_ctor(a_dynamic_init_ptr  dip,
                                   a_boolean           is_constant_evaluated,
                                   a_source_position   *pos,
                                   a_constant_ptr      result_con,
                                   a_diag_list_ptr     diag_list);

/*
The following macro (NS_scope_constexpr_intrinsics) describes functions in
namespace std and std::meta that the front end recognizes and attempts to
evaluate intrinsically.  The macro takes a macro M that should be replaced
by a macro of the form:

  #define MACRO(ns, name, signature)

Different parts of the front end invoke NS_scope_constexpr_intrinsics to
  1) define (here, in interpret.h) enumerator constants identifying the
     intrinsics by number
  2) define a table (in symbol_tbl.h) used to (a) mark associated symbol
     headers for efficient identification, and (b) build a Ptr_map to
     associate a "signature" to match function declarations with
  3) produce switch cases (in interpret.c) to handle evaluation dispatch

The "signature" is a string literal used as the third operand for the macro M.
This literal places some constraints on template arguments (optional), function
parameters (optional), and the return type (mandatory).  For example, if the
front end runs into a function std::construct_at, it will (a) recognize that
the symbol header for "construct_at" is marked as an intrinsic name, (b) look
up that header in the Ptr_map mentioned in (2) above (finding the string
literal "<T,>(*.,)*."), and (c) compare the declaration against the string as
follows:
  - "<T,>" indicates that a leading type argument is required (and the trailing
     comma indicates that additional unconstrained arguments are permitted)
  - "(*.,)" indicates that one or more parameter type are expected (again, the
     trailing comma indicates optional additional parameters); "*" represents
     "pointer to" and "." represents "any type"
  - The second "*." represents the return type ("pointer to any type")
The "type codes" currently recognized are:
  - ".": any type
  - "b": bool
  - "r": std::meta::info
  - "v": void
  - "C": a built-in character type
  - "I": an integral type
  - "Sv": std::string_view
  - "Sz": std::size_t
  - "Vr": std::meta::__infovec
  - "*": pointer to the type kind described in the following code

If a new intrinsic is added with name intrin in the namespace identified with
ns (currently ns must be std or std_meta), a function do_constexpr_ns_intrin
must be defined in interpret.c to implement its evaluation.
*/
#define NS_scope_constexpr_intrinsics(M) \
  M(std, is_constant_evaluated, "()b") \
  M(std, construct_at, "<T,>(*.,)*.") \
  M(std, __report_constexpr_value, "(I)v|(*C)v|(*C,I)v") \
  M(std_meta, make_constexpr_array, "<T>(*.,I)*.") \
  M(std_meta, identifier_of, "(r)Sv") \
  M(std_meta, name_of, "(r)Sv") \
  M(std_meta, members__impl, "(r)Vr") \
  M(std_meta, static_data_members__impl, "(r)Vr") \
  M(std_meta, nonstatic_data_members__impl, "(r)Vr") \
  M(std_meta, bases__impl, "(r)Vr") \
  M(std_meta, subobjects__impl, "(r)Vr") \
  M(std_meta, enumerators__impl, "(r)Vr") \
  M(std_meta, parameters__impl, "(r)Vr") \
  M(std_meta, current_parameters__impl, "()Vr") \
  M(std_meta, template_arguments__impl, "(r)Vr") \
  M(std_meta, annotations__impl, "(r,r)Vr") \
  M(std_meta, substitute__impl, "(r,Vr)r") \
  M(std_meta, reflect_result, "<T>(.)r") \
  M(std_meta, extract, "<T>(r).") \
  M(std_meta, value_of, "(r)r") \
  M(std_meta, is_token_sequence, "(r)b") \
  M(std_meta, is_empty_token_sequence, "(r)b") \
  M(std_meta, is_annotation, "(r)b") \
  M(std_meta, is_type, "(r)b") \
  M(std_meta, is_alias, "(r)b") \
  M(std_meta, is_incomplete_type, "(r)b") \
  M(std_meta, is_template, "(r)b") \
  M(std_meta, is_function_template, "(r)b") \
  M(std_meta, is_variable_template, "(r)b") \
  M(std_meta, is_class_template, "(r)b") \
  M(std_meta, is_alias_template, "(r)b") \
  M(std_meta, is_concept, "(r)b") \
  M(std_meta, is_constant, "(r)b") \
  M(std_meta, is_variable, "(r)b") \
  M(std_meta, is_function, "(r)b") \
  M(std_meta, is_function_parameter, "(r)b") \
  M(std_meta, is_explicit_object_parameter, "(r)b") \
  M(std_meta, is_namespace, "(r)b") \
  M(std_meta, is_nonstatic_data_member, "(r)b") \
  M(std_meta, is_base, "(r)b") \
  M(std_meta, is_constructor, "(r)b") \
  M(std_meta, is_destructor, "(r)b") \
  M(std_meta, is_special_member, "(r)b") \
  M(std_meta, is_public, "(r)b") \
  M(std_meta, is_protected, "(r)b") \
  M(std_meta, is_private, "(r)b") \
  M(std_meta, is_accessible, "(r)b") \
  M(std_meta, is_static_member, "(r)b") \
  M(std_meta, is_virtual, "(r)b") \
  M(std_meta, is_deleted, "(r)b") \
  M(std_meta, is_defaulted, "(r)b") \
  M(std_meta, is_explicit, "(r)b") \
  M(std_meta, is_override, "(r)b") \
  M(std_meta, is_pure_virtual, "(r)b") \
  M(std_meta, is_bit_field, "(r)b") \
  M(std_meta, has_static_storage_duration, "(r)b") \
  M(std_meta, has_internal_linkage, "(r)b") \
  M(std_meta, has_c_varargs, "(r)b") \
  M(std_meta, has_default_argument, "(r)b") \
  M(std_meta, has_consistent_name, "(r)b") \
  M(std_meta, has_template_arguments, "(r)b") \
  M(std_meta, has_identifier, "(r)b") \
  M(std_meta, dealias, "(r)r") \
  M(std_meta, template_of, "(r)r") \
  M(std_meta, type_of, "(r)r") \
  M(std_meta, return_type_of, "(r)r") \
  M(std_meta, parent_of, "(r)r") \
  M(std_meta, size_of, "(r)Sz") \
  M(std_meta, offset_of, "(r)Sz") \
  M(std_meta, bit_size_of, "(r)Sz") \
  M(std_meta, bit_offset_of, "(r)Sz") \
  M(std_meta, alignment_of, "(r)Sz") \
  M(std_meta, define_class__impl, "(r,I,*.)v") \
  M(std_meta, metacall__impl, "(r,Vr)r") \
  M(std_meta, __report_tokens, "(r)v") \
  M(std_meta, queue_injection, "(r,r)v") \
  M(std_meta, namespace_inject, "(r,r)v") \
  M(std_meta, nearest_token_queuing_context, "()r") \
  M(std_meta, nearest_class_or_namespace, "()r") \
  M(std_meta, nearest_namespace, "()r") \
  M(std_meta, type_tuple_size, "(r)I") \
  M(std_meta, type_tuple_element, "(I,r)r") \
  M(std_meta, type_is_void, "(r)I") \
  M(std_meta, type_is_null_pointer, "(r)I") \
  M(std_meta, type_is_integral, "(r)I") \
  M(std_meta, type_is_floating_point, "(r)I") \
  M(std_meta, type_is_array, "(r)I") \
  M(std_meta, type_is_pointer, "(r)I") \
  M(std_meta, type_is_lvalue_reference, "(r)I") \
  M(std_meta, type_is_rvalue_reference, "(r)I") \
  M(std_meta, type_is_member_object_pointer, "(r)I") \
  M(std_meta, type_is_member_function_pointer, "(r)I") \
  M(std_meta, type_is_enum, "(r)I") \
  M(std_meta, type_is_union, "(r)I") \
  M(std_meta, type_is_class, "(r)I") \
  M(std_meta, type_is_function, "(r)I") \
  M(std_meta, type_is_reflection, "(r)I") \
  M(std_meta, type_is_reference, "(r)b") \
  M(std_meta, type_is_arithmetic, "(r)b") \
  M(std_meta, type_is_fundamental, "(r)b") \
  M(std_meta, type_is_object, "(r)b") \
  M(std_meta, type_is_scalar, "(r)b") \
  M(std_meta, type_is_compound, "(r)b") \
  M(std_meta, type_is_member_pointer, "(r)b") \
  M(std_meta, type_is_const, "(r)b") \
  M(std_meta, type_is_volatile, "(r)b") \
  M(std_meta, type_is_trivial, "(r)b") \
  M(std_meta, type_is_trivially_copyable, "(r)b") \
  M(std_meta, type_is_standard_layout, "(r)b") \
  M(std_meta, type_is_empty, "(r)b") \
  M(std_meta, type_is_polymorphic, "(r)b") \
  M(std_meta, type_is_abstract, "(r)b") \
  M(std_meta, type_is_final, "(r)b") \
  M(std_meta, type_is_aggregate, "(r)b") \
  M(std_meta, type_is_signed, "(r)b") \
  M(std_meta, type_is_unsigned, "(r)b") \
  M(std_meta, type_is_bounded_array, "(r)b") \
  M(std_meta, type_is_unbounded_array, "(r)b") \
  M(std_meta, type_is_scoped_enum, "(r)b") \
  M(std_meta, type_remove_const, "(r)r") \
  M(std_meta, type_remove_volatile, "(r)r") \
  M(std_meta, type_remove_cv, "(r)r") \
  M(std_meta, type_add_const, "(r)r") \
  M(std_meta, type_add_volatile, "(r)r") \
  M(std_meta, type_add_cv, "(r)r") \
  M(std_meta, type_remove_reference, "(r)r") \
  M(std_meta, type_add_lvalue_reference, "(r)r") \
  M(std_meta, type_add_rvalue_reference, "(r)r") \
  M(std_meta, type_make_signed, "(r)r") \
  M(std_meta, type_make_unsigned, "(r)r") \
  M(std_meta, type_remove_extent, "(r)r") \
  M(std_meta, type_remove_all_extents, "(r)r") \
  M(std_meta, type_remove_pointer, "(r)r") \
  M(std_meta, type_add_pointer, "(r)r") \
  M(std_meta, type_remove_cvref, "(r)r") \
  M(std_meta, type_decay, "(r)r") \
  M(std_meta, type_underlying_type, "(r)r") \
  /* End of NS_scope_constexpr_intrinsics. */


enum a_constexpr_intrinsic {
  cit_error,
  cit_std_allocator_allocate,
  cit_std_allocator_deallocate,
#define CIT_name(ns, name, signature)  cit_##ns##_##name,
  NS_scope_constexpr_intrinsics(CIT_name)
#undef CIT_name
  cit_last
};

void register_constexpr_intrinsic(a_constexpr_intrinsic  tag,
                                  a_routine_ptr          rp);


#if DEBUG
uintptr_t db_hash_ptr(void  *ptr);

a_host_large_integer db_int_val(a_byte  *val_bytes);

void db_complete_object(a_byte  *addr);

void db_call_stack(void  *ips);

a_byte* db_stack_storage(void  *ptr,
                         void  *ips);

void db_data_map(void  *map_ptr);

void db_live_set(void  *interpreter_state);

unsigned long db_show_interpret_fe_space_used(unsigned long  grand_total);

#if TRACK_INTERPRETER_ALLOCATIONS
unsigned long db_object_alloc_num(a_byte  *ptr);
#endif /* TRACK_INTERPRETER_ALLOCATIONS */

#endif /* DEBUG */

void clean_up_interpreter(void);

void interpret_trans_unit_init(void);

void interpret_init(void);

void interpret_one_time_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef INTERPRET_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2015-2025 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
