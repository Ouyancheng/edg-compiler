/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

expr.h -- Declarations related to expression parsing.

*/

/* Avoid including these declarations more than once: */
#ifndef EXPR_H
#define EXPR_H 1

#if !STANDALONE_UTILITY_PROGRAM
#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */
#endif /* !STANDALONE_UTILITY_PROGRAM */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/* Flag bits used to indicate scanning options that apply to one level
   of expression scanning.  These are localized options that indicate
   special handling for an expression because of the context. */
#define EOPT_DISALLOW_COMMA_OPERATOR 0x1
			/* The comma operator should not be allowed at the top
			   level.  Certain contexts suppress the comma because
			   it has another meaning there (e.g., argument
			   lists). */
#define EOPT_OPERAND_OF_CAST 0x2
			/* This expression is the immediate operand of a cast.
			   Floating constants are allowed in integral constant
			   expressions when they are the immediate operand
			   of a cast. */
#define EOPT_OPERAND_OF_ADDRESS_OF 0x4
			/* This expression is the operand of a unary "&"
			   operator. */
#define EOPT_TRAPPED_LEFT_PAREN 0x8
			/* The caller of scan_expr scanned over a left
			   parenthesis which it turned out should have begun
			   an expression.  scan_expr pretends that there is
			   a left parenthesis preceding the current token. */
#define EOPT_ALLOW_BOUND_FUNCTION 0x10
			/* A C++ bound function may be returned. */
#define EOPT_PRESERVE_PROPERTY_REF 0x20
			/* A reference to a Microsoft property member can
			   be left in that form, so that it has a chance
			   to be rewritten in the "put" form.  By default,
			   it will be rewritten in the "get" form. */
#define EOPT_MARKED_AS_GNU_EXTENSION 0x40
			/* The caller of scan_expr scanned over the GNU keyword
			   __extension__. */
#define EOPT_PTR_TO_MEMBER_CONTEXT 0x80
			/* The expression is the immediate operand of the
			   unary "&" operator where a pointer-to-member
			   constant would be valid (presumably without
			   intervening parentheses). */
#define EOPT_OPERAND_OF_OFFSETOF 0x100
			/* This expression is in the top-level chain of the
			   second operand of __builtin_offsetof. */
#define EOPT_DELEGATE_INITIALIZER 0x200
			/* This expression is a top-level expression in the
			   initializer for a C++/CLI gcnew of a delegate
			   type. */
#define EOPT_NO_OPTIONS 0

typedef int a_local_expr_options_set;
typedef struct an_operand *an_operand_ptr;

/*
Enumeration of the kinds of initializer values described by
an_init_component.
*/
enum an_init_component_kind_tag {
  ick_expression,	/* An expression. */
  ick_braced,		/* A brace-enclosed list. */
  ick_designator,	/* A designator (for C99-style or GNU-style
			   designated initializers). */
  ick_continued		/* A placeholder component indicating that more
			   elements of a brace-enclosed list should be
			   parsed. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_init_component_kind;


/*
An opaque type to point to state information needed to suspend/resume the
parsing of braced initializer lists.
*/
typedef struct a_braced_list_continuation *a_braced_list_continuation_ptr;


/*
Entry describing a value in an initializer, which is either an expression
or a brace-enclosed list.  In the C++11 standard, the corresponding syntax
term is "initializer-clause".  It does double duty: An expression-list (i.e.,
an argument list) is an initializer-list, which is a list of
initializer-clauses with possible variadic template expansions.
Accordingly, there are two names for this entry: an_init_component
and an_arg_list_elem.  Generally, the expression routines use
an_arg_list_elem and the initialization routines use an_init_component.
A true initializer list can contain designators (and an_init_component
is typically used), whereas an argument list cannot (and an_arg_list_elem
is typically used).
*/
typedef struct an_init_component *an_init_component_ptr;
typedef struct an_init_component {
  an_init_component_ptr
		next;	/* When this entity is on a list, a pointer to the
			   next component on the list.  NULL if this is the
			   last component or if the entry is not on a list. */
  an_init_component_kind
		kind;	/* The kind of initializer value (e.g., brace-enclosed
			   list). */
  a_bit_field	bundled:1;
			/* Set to TRUE if expressions within this component
			   have been "bundled," meaning some things like
			   object lifetimes have been detached from the
			   enclosing context and attached to this entry so
			   they can be pulled out and restored later when the
			   rest of the processing for the expression is
			   done. */
  a_bit_field	detached_ref_entries:1;
			/* Set to TRUE if any ref entries in this (expression)
			   component are not attached to the current
			   expression at present. */
  a_bit_field	contains_designator:1;
			/* Set to TRUE if this is an ick_braced component that
			   directly contains an ick_designator component. */
  a_bit_field	check_narrowing:1;
			/* TRUE if the narrowing conversion checks should
			   be forced when this component is processed.
			   This flag is set, for example, on the elements
			   of a braced-init-list when it is being used
			   as an argument list, since in that context even
			   the simple conversions of the arguments to the
			   parameter types are prohibited from involving
			   narrowing conversions.  The narrowing checks
			   generate errors, not warnings, in these cases. */
  a_bit_field	braced_init_in_parentheses:1;
			/* TRUE if this entity is a braced-init-list that
			   was scanned in a parenthesized initializer that
			   expects a single expression.  [dcl.init]p13 of
			   the C++11 standard disallows a parenthesized
			   initializer of the form ({x}) if the entity being
			   initialized is not a class. */
  a_bit_field	permanently_allocated:1;
			/* TRUE if this entry is permanently allocated and
			   an attempt to free it should be ignored. */
#if CHECKING
  a_bit_field	on_free_list:1;
			/* TRUE if this entry has been freed and is on the
			   available list. */
#endif /* CHECKING */
  a_bit_field	constant_expr_ruled_out:1;
			/* For an ick_expression component, TRUE if the
			   expression does not have the form required of a
			   constant expression in the current mode.  That
			   can be very slightly different from whether the
			   expression actually evaluates to a constant. */
  a_pack_expansion_descr_ptr
		pack_expansion_descr;
			/* If non-NULL, this entity is a pack expansion
			   (it is followed by "...", as in "T()..."), and this
			   points to the expansion description.  For an
			   ick_expression, the same pointer is also stored in
			   the operand pack_expansion_descr field. */
  union {
    /* When kind == ick_expression: */
    struct {
      struct an_arg_operand
		*arg_op;
			/* The expression.  The an_arg_operand struct is
			   opaque outside of the expression routines. */
      an_object_lifetime_ptr
		lifetime;
			/* When this entry is bundled, non-NULL to preserve
			   an associated lifetime until the point when the
			   expression is handled. */
    } expr;
    /* When kind == ick_braced: */
    struct {
      an_init_component_ptr
		list;
			/* A list of initializer values linked on the "next"
			   field.  NULL if the list is empty. */
      a_source_position
		start_pos,
		end_pos;
			/* The source positions of the opening and closing
			   brace tokens. */
    } braced;
    /* When kind == ick_designator: */
    struct {
      a_symbol_header_ptr
		field_name;
			/* Pointer to the symbol header for a field designator
			   or NULL if this is an array element designator. */
      a_targ_size_t
		element_index;
			/* The constant value specified in an array element
			   designator (the first one in the case of a GNU-style
			   array range designator), or zero if this is a field
			   designator. */
      a_targ_size_t
		last_element_index;
			/* If this component represents a GNU-style array range
			   designator, the second constant value specified in
			   the range.  Otherwise, zero for a field designator
			   and the same value as element_index for an array
			   designator. */
      a_source_position
		position;
			/* The source position of the designator. */
    } designator;
    /* When kind == ick_continued: */
    struct {
      a_braced_list_continuation_ptr
		state;
			/* An opaque pointer to state information that must be
			   restored to permit the continued parsing of a braced
			   initializer list. */
    } continuation;
  } variant;
} an_init_component;
typedef an_init_component an_arg_list_elem;
typedef an_arg_list_elem *an_arg_list_elem_ptr;

an_init_component_ptr get_continued_elem(an_init_component_ptr  icp);

void complete_braced_init_list_parsing(an_init_component_ptr  icp_tree);

/*
Macros to manage to the next initialization component.
*/
#define is_last_elem(icp)                                                    \
  ((icp)->next == NULL)

#define is_continuation_elem(icp)                                            \
  ((icp)->kind == (an_init_component_kind)ick_continued)

#define next_elem(icp)                                                       \
  (is_last_elem(icp)                 ? (an_init_component_ptr) NULL :        \
   is_continuation_elem((icp)->next) ? get_continued_elem(icp) :             \
                                       (icp)->next)

#define p_next_elem(icp)                                                     \
  (&(icp)->next)

#define is_single_elem(icp)                                                  \
  ((icp) != NULL && (icp)->next == NULL)

#define split_tail_elems(icp)                                                \
  ((icp)->next = NULL)

#define append_elem(icp, tail)                                               \
  ((icp)->next = tail)

/*
Macro to identify initialization components that are expressions.
*/
#define is_expression_component(icp)                                         \
  ((icp)->kind == (an_init_component_kind)ick_expression)

/*
Macro to identify braced initialization components.
*/
#define is_braced_init_component(icp)                                        \
  ((icp)->kind == (an_init_component_kind)ick_braced)

/*
Macro to identify designator components.
*/
#define is_designator_component(icp)                                        \
  ((icp)->kind == (an_init_component_kind)ick_designator)

/*
Return the operand address from an expression init component.
*/
#define operand_of_arg_list_elem(icp) (&(icp)->variant.expr.arg_op->operand)

/*
Entry used to pass information about the context for a rescan to redo
semantic analysis as part of template deduction.  Many of the fields here
are parameters to copy_template_param_expr that we want to pass from
that function into the expression routines and then back again without
having to list each one on every intervening call.
*/
typedef struct a_rescan_control_block {
  an_expr_node_ptr
		expr;
			/* The expression being rescanned.  This is used only
			   when calling the scan_xxx_operator routines, to
			   reduce the number of parameters by one. */
  a_token_kind	operator_token;
			/* The token kind associated with the operator of the
			   expression associated with expr, if there is one.
			   tok_error otherwise.  Also used only when calling
			   the scan_xxx_operator routines. */
  an_expr_node_ptr
		argument_list;
			/* When processing a call, this points to the
			   argument list. */
  a_template_arg_ptr
		template_arg_list;
			/* The template argument list being tried. */
  a_template_param_ptr
		template_param_list;
			/* The parameter list of the template being tried. */
  a_ctws_options_set
		options;
			/* Options for copy_template_param_expr. */
  a_ctws_state_ptr
		ctws_state;
			/* The template argument substitution state. */
  a_byte_boolean
		error_detected;
			/* TRUE if an error was detected in the rescan, which
			   makes the deduction fail. */
} a_rescan_control_block;


#if !STANDALONE_UTILITY_PROGRAM
extern void prescan_initializer_for_auto_type_deduction(
                                        a_decl_parse_state *dps,
                                        a_boolean          parenthesized_init);

extern void scan_and_discard_init_component(a_decl_parse_state  *dps);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern
an_expr_node_ptr make_lvalue_cast_node(an_expr_node_ptr source_expr,
                                       a_type_ptr       type_cast_to,
                                       a_boolean        compiler_generated);

extern void check_closing_paren_after_expr_list(void);

extern
void scan_ctor_arguments(a_symbol_ptr             constructor_sym,
                         a_source_position        *source_pos,
                         a_type_ptr               object_class_type,
                         a_type_ptr               dest_type,
                         a_boolean                fill_in_dtor,
                         a_boolean                elision_allowed,
                         a_rescan_control_block   *rcblock,
                         a_boolean                arg_list_supplied,
                         an_arg_list_elem_ptr     supplied_arg_list,
                         an_arg_list_elem_ptr     init_list_ctor_arg_list,
                         a_boolean                *trivial_ctor,
                         a_boolean                *elision_done,
                         a_boolean                *unboxing_conv,
                         a_boolean                *string_ctor_skip,
                         an_operand_ptr           simple_result,
                         a_dynamic_init_ptr       *p_dip,
                         an_expr_node_ptr         *p_temp_init_node,
                         a_source_position        *closing_paren_position);

extern void scan_dependent_parenthesized_initializer(
                                    a_rescan_control_block   *rcblock,
                                    a_boolean                arg_list_supplied,
                                    an_arg_list_elem_ptr     supplied_arg_list,
                                    an_operand_ptr           single_operand,
                                    a_dynamic_init_ptr       *dip);

extern a_type_ptr new_delete_base_type_from_operation_type(a_type_ptr type);

extern a_boolean new_or_delete_type_requires_array_handling(
                                                 a_type_ptr type,
                                                 a_boolean  check_constructor);

extern a_boolean is_expr_start_token(a_token_kind tok);

extern a_boolean token_is_function_name_string_literal(a_token_kind token);

extern void set_curr_token_to_function_name_string(a_boolean do_concat);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean set_curr_token_to_microsoft_lprefix_operator_string(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_const_char *spelling_for_function_name_token(a_token_kind token);

extern a_boolean operand_is_string_literal(an_operand_ptr operand);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean operand_is_cast_string_literal(an_operand_ptr operand,
                                                a_constant_ptr *string_con);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void record_start_of_lambda_header(a_lambda_ptr lambda);

extern void record_end_of_lambda_header(a_lambda_ptr lambda);

extern
a_type_ptr set_implicit_lambda_return_type(a_type_ptr        return_type,
                                           a_source_position *err_pos);

extern
void transfer_arg_operand_for_template_arg(a_template_arg_ptr tap,
                                           a_template_arg_ptr orig_tap);

extern a_template_arg_ptr
copy_template_arg_list_with_substitution_rebuilding_arg_operands(
			a_template_arg_ptr	arg_list_to_copy,
			a_template_param_ptr	param_list_for_copy,
			a_template_arg_ptr	templ_arg_list,
			a_template_param_ptr	templ_param_list,
			a_source_position	*source_pos,
			a_ctws_options_set	options,
			a_boolean		orig_is_nonreal_template,
			a_boolean		*copy_error,
			a_ctws_state_ptr	ctws_state);

extern an_expr_node_ptr scan_integer_expression(a_boolean is_switch_expr);

extern an_expr_node_ptr make_node_from_void_expression_operand(
                                          an_operand_ptr  operand,
                                          a_boolean       result_of_stmt_expr);

extern an_expr_node_ptr scan_void_expression(a_boolean repeated_in_loop,
                                             a_boolean marked_as_gnu_extension,
                                             a_boolean is_statement_expr);

extern
an_expr_node_ptr scan_typed_expression(a_type_ptr         required_type,
                                       a_type_ptr         alternate_type,
                                       an_error_code      err_code);

extern void scan_bool_constant_expression(a_constant *constant);

extern void check_range_based_for_statement(
                          a_statement_ptr            statement,
                          a_source_position          *expr_position,
                          a_token_sequence_number    tok_seq_number,
                          a_scope_pointers_block_ptr begin_end_pointers_block,
                          a_scope_pointers_block_ptr iterator_pointers_block);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern
void check_for_each_statement(a_statement_ptr            statement,
                              an_operand_ptr             prev_decl_iterator,
                              a_source_position          *expr_position,
                              a_token_sequence_number    tok_seq_number,
                              a_scope_pointers_block_ptr pointers_block);
extern void scan_for_each_expression(a_statement_ptr   statement,
                                     a_source_position *expr_position);
extern void scan_previously_decl_iterator_name(
                                      a_for_each_loop_ptr felp,
                                      an_operand_ptr      prev_decl_iterator);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void scan_range_based_for_expression(a_statement_ptr   statement,
                                            a_source_position *expr_position);

extern void scan_default_arg_expr(a_param_type_ptr ptp,
                                  a_boolean        is_member_or_friend);

extern an_expr_node_ptr convert_default_arg_expr(an_expr_node_ptr expr,
                                                 a_param_type_ptr ptp,
                                                 a_boolean        evaluated);
extern
a_boolean variable_eligible_for_copy_optimization(a_variable_ptr var,
                                                  a_boolean      return_case,
                                                  a_boolean      move_case);

extern an_expr_node_ptr scan_return_expression(
                                              a_type_ptr         required_type,
                                              an_error_code      err_code,
                                              a_dynamic_init_ptr *dip);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_fs_integral_constant_expression(a_type_ptr specific_type,
                                                 a_boolean  is_enum,
                                                 a_constant *constant);

extern void scan_constant_dimension_expression(a_constant *constant);

extern void scan_nonconstant_dimension_expression(
                                    a_boolean        is_new_or_delete_bound,
                                    a_boolean        is_top_level_vla_bound,
                                    a_boolean        is_evaluated_sizeof_arg,
                                    a_boolean        *is_constant,
                                    an_expr_node_ptr *expression,
                                    a_constant       *constant);

extern void extract_constant_from_operand_with_fs_fixup(
                                                     an_operand_ptr operand,
                                                     a_constant     *constant);

extern
void rescan_selector_of_call(a_rescan_control_block *rcblock,
                             an_operand_ptr         function_operand,
                             an_operand_ptr         bound_function_selector);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void insert_temporary_initialization(an_expr_node_ptr temp_init_expr,
                                            an_operand_ptr   result);

extern void process_microsoft_null_pointer_constant_bug(
                                                    an_operand_ptr operand,
                                                    a_type_ptr     dest_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
void process_simple_assignment(an_operand_ptr          operand_1,
                               an_operand_ptr          operand_2,
                               a_source_position       *operator_position,
                               a_token_sequence_number operator_tok_seq_number,
                               a_boolean               check_for_overloading,
                               an_operand_ptr          result);

#if GNU_EXTENSIONS_ALLOWED
an_expr_node_ptr scan_asm_operand_expression(a_boolean output,
                                             a_boolean input);
extern a_symbol_ptr gnu_builtin_func_by_name(a_const_char *name);

#endif /* GNU_EXTENSIONS_ALLOWED */

#if !STANDALONE_UTILITY_PROGRAM
extern a_dynamic_init_ptr scan_array_mem_initializer(a_constructor_init  *cip);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_expr_node_ptr prep_generated_arg_expr(an_expr_node_ptr  expr,
                                                a_param_type_ptr  param,
                                                a_source_position *err_pos);

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_class_initializer_expression(a_decl_parse_state  *dps);

extern a_boolean whole_aggr_class_init_possible(
                                            an_init_component_ptr  icp,
                                            a_type_ptr             dest_type);

extern a_boolean whole_array_init_possible(an_init_component_ptr  icp,
                                           a_type_ptr             dest_type,
                                           a_constant_ptr         *result);

#if GNU_VECTOR_TYPES_ALLOWED
a_boolean whole_vector_init_possible(an_init_component_ptr  icp,
                                     a_type_ptr             dest_type);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

extern
a_boolean is_overloadable_type_operand_full(an_operand_ptr operand,
                                            a_boolean      first_operand,
                                            a_boolean      CFOO_guard);

extern void value_init_variable_or_member(a_type_ptr         type,
                                          an_init_state      *is,
                                          a_source_position  *diag_pos);

extern void scan_class_parenthesized_initializer(
                                   a_type_ptr            class_type,
                                   a_type_ptr            object_class_type,
                                   a_source_position     *source_pos,
                                   a_boolean             fill_in_dtor,
                                   a_boolean             args_supplied,
                                   an_arg_list_elem_ptr  arg_list,
                                   an_init_state         *is);

extern a_dynamic_init_ptr forwarding_initializer_for_inheriting_constructor(
                                                          a_routine_ptr  ctor);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void scan_template_argument_constant_expression(a_type_ptr param_type,
                                                       a_constant *constant);

extern an_arg_operand_ptr scan_nontype_template_argument(
                                  a_decl_sequence_number initial_inst_seq_num);

extern a_boolean nontype_template_arg_is_compatible_with_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type);

extern void conv_nontype_template_arg_to_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type,
                                            a_constant         *constant);

#if !STANDALONE_UTILITY_PROGRAM
extern
an_init_component_ptr get_braced_init_list(a_boolean          is_full_expr,
                                           a_decl_parse_state *dps);

extern an_init_component_ptr scan_full_initializer_expr_as_component(
                                     a_decl_parse_state *dps,
                                     a_boolean          parenthesized,
                                     a_boolean          allow_empty_expansion);

extern
void convert_initializer(an_init_component_ptr icp,
                         a_type_ptr            dest_type,
                         a_boolean             is_var_init,
                         a_boolean             fill_in_dtor,
                         an_init_state         *is);

typedef struct an_arg_match_summary an_arg_match_summary_dummy_typedef;
extern void record_aggr_init_match(struct an_arg_match_summary *arg_match);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void aggr_init_cli_array_with_alloc(an_init_component_ptr  icp,
                                           a_type_ptr             hatype,
                                           an_init_state          *is,
                                           a_dynamic_init_ptr     *result);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean expr_is_rescannable(an_expr_node_ptr expr);

extern void rescan_expr_with_substitution_internal(
                            an_expr_node_ptr         expr,
                            a_rescan_control_block   *rcblock,
                            a_local_expr_options_set local_options,
                            an_operand_ptr           result,
                            an_operand_ptr           bound_function_selector);

extern an_expr_node_ptr rescan_expr_with_substitution(
                                             an_expr_node_ptr       expr,
                                             a_type_ptr             guide_type,
                                             a_rescan_control_block *rcblock,
                                             a_constant             *constant);

extern
void rescan_dynamic_init_with_substitution(a_dynamic_init_ptr     dip,
                                           a_rescan_control_block *rcblock,
                                           an_operand_ptr         result);

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_member_constant_initializer_expression(
                                                 a_decl_parse_state *dps,
                                                 a_constant         *constant);
extern
void scan_constant_initializer_expression(a_type_ptr         required_type,
                                          a_decl_parse_state *dps,
                                          a_constant         *constant);

extern void scan_dependent_type_parenthesized_initializer(an_init_state  *is);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_constant_ptr scan_case_label_constant(a_type_ptr switch_type);

extern an_expr_node_ptr scan_boolean_controlling_expression(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_const_char *scan_uuidof_operand(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
a_type_ptr scan_decltype_operator(a_rescan_control_block *rcblock,
                                  a_boolean              might_be_id_start);

extern a_type_ptr decltype_of_expr_with_substitution(
                                  a_type_ptr               type,
                                  an_expr_node_ptr         expr,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_param_ptr     template_param_list,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_ctws_state_ptr         ctws_state);

extern a_type_ptr scan_underlying_type_operator(void);

#if GNU_EXTENSIONS_ALLOWED 

extern a_type_ptr scan_bases_operator(void);

extern a_type_ptr scan_typeof_operator(a_rescan_control_block *rcblock,
                                       a_decl_pos_block       *decl_pos_block);

extern void typedef_initializer(a_symbol_ptr  symbol_ptr);

#endif /* GNU_EXTENSIONS_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED
extern an_expr_node_ptr scan_upc_forall_affinity(void);
#endif /* UPC_EXTENSIONS_ALLOWED */

extern an_expr_node_ptr make_condition_value_expression(
                                                a_variable_ptr var,
                                                a_boolean      is_switch_expr);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_variable_ptr based_variable(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean in_expression_context(void);

extern a_boolean arg_operand_is_instantiation_dependent(
                                               an_arg_operand_ptr arg_operand);

extern a_boolean arg_operand_involves_error_entity(
                                               an_arg_operand_ptr arg_operand);

extern
a_symbol_ptr find_default_constructor(a_type_ptr        class_type,
                                      a_boolean         include_templates,
                                      a_boolean         declarative_context,
                                      a_source_position *pos,
                                      a_boolean         *ambiguous,
                                      a_symbol_ptr      *inaccessible_match,
                                      a_boolean         *trivial);

extern
a_symbol_ptr find_copy_constructor(a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_symbol_ptr          *inaccessible_match,
                                   a_boolean             *class_bitwise_copy);

a_symbol_ptr find_copy_assignment_operator(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  source_cv_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_type_qualifier_set  dest_cv_qualifiers,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *bitwise_assign);

extern a_routine_ptr find_assignment_operator_for_memberwise_copy(
                                             a_type_ptr        class_type,
                                             an_expr_node_ptr  source_expr,
                                             an_expr_node_ptr  dest_expr,
                                             a_source_position *dest_decl_pos);

extern void process_unattached_template_argument_list(
                                         a_template_arg_ptr template_arg_list);

extern an_expr_node_ptr make_assignment_expr(
                                      an_expr_node_ptr       lvalue_expr,
                                      an_expr_operator_kind  op,
                                      an_expr_node_ptr       rvalue_expr);

extern a_boolean in_lambda_body(void);

extern a_scope_depth scope_depth_for_local_variable_capture(
                                                a_variable_ptr var,
                                                a_scope_depth  starting_depth,
                                                a_lambda_ptr   *lambda);

extern a_boolean check_var_for_lambda_capture(a_variable_ptr  var,
                                              a_boolean       implicit,
                                              an_error_code   *diag);

extern a_boolean current_mode_allows_field_selection_folding(void);

extern a_boolean compute_is_convertible(a_type_ptr  src_type,
                                        a_type_ptr  dst_type);

extern
a_boolean compute_is_constructible(a_builtin_operation_kind kind,
                                   a_type_ptr               dst_type,
                                   an_expr_node_ptr         args);

extern
a_boolean compute_is_destructible(a_builtin_operation_kind kind,
                                  a_type_ptr               type);

extern
a_boolean compute_is_assignable(a_builtin_operation_kind kind,
                                a_type_ptr               dst_type,
                                a_type_ptr               src_type);

/*
Macro that is TRUE if the node is an operation node.
*/
#define is_operation_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_operation)

/*
Macro that is TRUE if the node is a constant node.
*/
#define is_constant_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_constant)

/*
Macro that is TRUE if the node is a variable node.
*/
#define is_variable_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_variable)

/*
Macro that is TRUE if the node is a field node.
*/
#define is_field_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_field)

/*
Macro that is TRUE if the node is a routine node.
*/
#define is_routine_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_routine)

/*
Macro that is TRUE if the node is an error node.
*/
#define is_error_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_error)

/*
Return TRUE if the node is a glvalue, meaning an lvalue or an xvalue.
*/
#define is_glvalue_node(node)						\
  ((node)->is_lvalue || (node)->is_xvalue)

/*
Return TRUE if the operator in the given node (which must be an operation
node) is "op".
*/
#define node_operator_is(node, op)                                      \
  ((node)->variant.operation.kind == (an_expr_operator_kind)(op))

/*
Return TRUE if the given node (which must be an operation node) has its
type_kind field set to the given value.
*/
#define node_operator_type_kind_is(node, tkind)                    \
  ((node)->variant.operation.type_kind == (a_type_kind)(tkind))

/*
Return TRUE if "node" is a function call operation.
*/
#define is_call_node(node)                                              \
  (is_operation_node((node)) &&                                         \
   (node_operator_is((node), eok_call) ||                               \
    node_operator_is((node), eok_dot_member_call) ||                    \
    node_operator_is((node), eok_points_to_member_call) ||              \
    node_operator_is((node), eok_dot_pm_call) ||                        \
    node_operator_is((node), eok_points_to_pm_call)))


#if GNU_EXTENSIONS_ALLOWED

/*
Return TRUE if the given operator is a gnu min/max operator (>? or <?).
*/
#define is_gnu_min_max_operator(op) \
 ((op) == (an_expr_operator_kind)eok_gnu_min || \
  (op) == (an_expr_operator_kind)eok_gnu_max)
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
The operands for eok_subscript or eok_padd are a pointer and an
integral subscript which can appear in either order.  This macro returns
the pointer operand of these nodes.
*/
#define subscript_or_padd_pointer_operand(node)                        \
        ((node)->variant.operation.pointer_operand_is_second ?         \
         (node)->variant.operation.operands->next :                    \
         (node)->variant.operation.operands)

#if MICROSOFT_EXTENSIONS_ALLOWED

extern an_expr_node_ptr make_cli_array_length_nodes(
                                               a_host_large_unsigned  rank,
                                               a_host_large_integer   dims[]);

#define UNSPECIFIED_CLI_ARRAY_LENGTH ((a_host_large_integer)0xC0FFEE)
			/* Default value representing an unspecified length
			   for an array dimension.  This is dictated by the
			   ECMA-372 standard (24.6). */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
