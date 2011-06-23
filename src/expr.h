/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
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

extern void scan_and_discard_initializer_expression(a_decl_parse_state  *dps);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern
an_expr_node_ptr make_lvalue_cast_node(an_expr_node_ptr source_expr,
                                       a_type_ptr       type_cast_to,
                                       a_boolean        compiler_generated);

extern void check_closing_paren_after_expr_list(void);

extern a_boolean new_or_delete_type_requires_array_handling(
                                                 a_type_ptr type,
                                                 a_boolean  check_constructor);

extern a_boolean is_expr_start_token(a_token_kind tok);

extern a_boolean token_is_function_name_string_literal(a_token_kind token);

extern a_boolean do_expression_level_string_literal_concatenation(void);

extern void set_curr_token_to_function_name_string(a_boolean do_concat);

#if MICROSOFT_EXTENSIONS_ALLOWED
a_boolean set_curr_token_to_microsoft_lprefix_operator_string(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern char *spelling_for_function_name_token(a_token_kind token);

extern a_boolean operand_is_string_literal(an_operand_ptr operand);

extern void record_start_of_lambda_header(a_lambda_ptr lambda);

extern void record_end_of_lambda_header(a_lambda_ptr lambda);

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

extern an_expr_node_ptr scan_void_expression(a_boolean repeated_in_loop,
                                             a_boolean marked_as_gnu_extension,
                                             a_boolean is_statement_expr);

extern an_expr_node_ptr scan_typed_expression(a_type_ptr    required_type,
					      an_error_code err_code);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern an_expr_node_ptr scan_for_each_expression(
                                             a_variable_ptr      iterator,
                                             a_for_each_loop_ptr extra_info);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void scan_default_arg_expr(a_param_type_ptr ptp);

extern
a_boolean variable_eligible_for_copy_optimization(a_variable_ptr var,
                                                  a_boolean      return_case);

extern an_expr_node_ptr scan_return_expression(
                                              a_type_ptr         required_type,
                                              an_error_code      err_code,
                                              a_dynamic_init_ptr *dip);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_fs_integral_constant_expression(a_constant *constant);

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
extern void insert_temporary_initialization(
                            an_expr_node_ptr                    temp_init_expr,
                            a_rewritten_property_reference_kind rewritten_kind,
                            an_operand_ptr                      result);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void process_simple_assignment(an_operand_ptr    operand_1,
                                      an_operand_ptr    operand_2,
                                      a_source_position *operator_position,
                                      an_operand_ptr    result);

#if GNU_EXTENSIONS_ALLOWED
an_expr_node_ptr scan_asm_operand_expression(a_boolean output,
                                             a_boolean input);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_initializer_expression(
                                 a_type_ptr          required_type,
                                 a_decl_parse_state  *dps,
                                 a_boolean           static_lifetime,
                                 a_boolean           force_object_lifetime,
                                 a_boolean           suppress_object_lifetime,
                                 a_boolean           is_copy_initialization,
                                 a_boolean           *is_pack_expansion,
                                 a_boolean           *expr_not_present,
                                 a_boolean           *is_constant,
                                 an_expr_node_ptr    *expression,
                                 a_constant          *constant);

extern a_dynamic_init_ptr scan_array_mem_initializer(a_constructor_init  *cip);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_expr_node_ptr prep_generated_arg_expr(an_expr_node_ptr  expr,
                                                a_param_type_ptr  param,
                                                a_source_position *err_pos);

#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean scan_class_initializer_expression(a_decl_parse_state  *dps,
                                                   a_dynamic_init_ptr  *dip);

extern a_boolean scan_aggregate_initializer_expression(
                                   a_type_ptr         required_type,
                                   a_boolean          static_lifetime,
                                   a_boolean          suppress_object_lifetime,
                                   a_decl_parse_state *dps,
                                   a_boolean          *whole_string_init,
                                   a_boolean          *is_pack_expansion,
                                   a_boolean          *is_constant,
                                   a_dynamic_init_ptr *dip,
                                   a_constant         *constant);

void prescan_aggregate_initializer_expression(
                         a_decl_parse_state *dps,
                         a_boolean          static_lifetime,
                         a_boolean          suppress_object_lifetime,
                         a_boolean          *empty_expansion_at_closing_brace);

extern a_boolean token_ends_initializer(a_token_kind  token);

extern
a_boolean is_overloadable_type_operand_full(an_operand_ptr operand,
                                            a_boolean      first_operand,
                                            a_boolean      CFOO_guard);

extern void scan_class_parenthesized_initializer(
                                   a_type_ptr         class_type,
                                   a_type_ptr         object_class_type,
                                   a_decl_parse_state *dps,
                                   a_source_position  *source_pos,
                                   a_boolean          fill_in_dtor,
                                   a_dynamic_init_ptr *p_dip);
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

extern void scan_dependent_type_parenthesized_initializer(
                                                     a_decl_parse_state *dps,
                                                     a_dynamic_init_ptr *dip);
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MICROSOFT_EXTENSIONS_ALLOWED
void scan_microsoft_case_label_constant_expression(a_constant *constant);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern an_expr_node_ptr scan_boolean_controlling_expression(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern char *scan_uuidof_operand(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
a_type_ptr scan_decltype_operator(a_rescan_control_block *rcblock,
                                  a_decl_pos_block       *decl_pos_block);

extern a_type_ptr decltype_of_expr_with_substitution(
                                  a_type_ptr               type,
                                  an_expr_node_ptr         expr,
                                  a_template_arg_ptr       template_arg_list,
                                  a_template_param_ptr     template_param_list,
                                  a_ctws_options_set       options,
                                  a_boolean                *copy_error,
                                  a_ctws_state_ptr         ctws_state);

#if GNU_EXTENSIONS_ALLOWED 

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
a_symbol_ptr find_template_default_constructor(a_type_ptr        class_type,
                                               a_source_position *pos,
                                               a_boolean         *ambiguous);

extern a_symbol_ptr find_copy_constructor(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             source_is_rvalue,
                                   a_source_position     *pos,
                                   a_boolean             *ambiguous,
                                   a_boolean             *class_bitwise_copy);

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
                                        a_type_ptr  dst_type,
                                        a_boolean   src_is_rvalue);

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

extern a_boolean is_foldable_gnu_builtin_function(a_routine_ptr rp,
                                                  a_boolean     *pseudo_call);

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

#define INTERNAL_UNSPECIFIED_CLI_ARRAY_LENGTH ((a_host_large_integer)-1)
			/* Internal default value representing an unspecified
			   length for an array dimension.  The internal
			   representation is used within front end parsing
			   routines.  When generating the final IL for an
			   unspecified length, the internal representation is
			   converted to the external representation,
			   UNSPECIFIED_CLI_ARRAY_LENGTH. */
#define UNSPECIFIED_CLI_ARRAY_LENGTH ((a_host_large_integer)0xC0FFEE)
			/* Default value representing an unspecified length
			   for an array dimension.  This is dictated by the
			   ECMA-372 standard (24.6). */

extern an_expr_node_ptr create_cli_array_length_list(
                                                    a_type_ptr cli_array_type);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
