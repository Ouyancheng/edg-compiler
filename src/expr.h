/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
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
			/* A reference of a field defined with the Microsoft
			   extension __declspec(property(...)) can be returned
			   in that form, so that it has a chance to be
			   rewritten in the "put" form.  By default, it will
			   be rewritten to the "get" form. */
#define EOPT_MARKED_AS_GNU_EXTENSION 0x40
			/* The caller of scan_expr scanned over the GNU keyword
			   __extension__. */
#define EOPT_PTR_TO_MEMBER_CONTEXT 0x80
			/* The expression is the immediate operand of the
			   unary "&" operator where a pointer-to-member
			   constant would be valid (presumably without
			   intervening parentheses). */
#define EOPT_FIELD_FOR_OFFSETOF 0x100
			/* The dot or arrow operator being scanned must
			   resolve to a nonstatic data member (or "field").
			   This is used in the implementation of
			   "__builtin_offsetof". */
#define EOPT_NO_OPTIONS 0

typedef int a_local_expr_options_set;


#if !STANDALONE_UTILITY_PROGRAM
extern void prescan_initializer_for_auto_type_deduction(
                                                    a_decl_parse_state  *dps);

extern void scan_and_discard_initializer_expression(a_decl_parse_state  *dps);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_expr_node_ptr make_lvalue_cast_node(an_expr_node_ptr source_expr,
                                              a_type_ptr       type_cast_to);

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

#if MICROSOFT_EXTENSIONS_ALLOWED || BACK_END_IS_CP_GEN_BE
extern char *spelling_for_function_name_token(a_token_kind token);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || BACK_END_IS_CP_GEN_BE */

extern an_expr_node_ptr scan_integer_expression(a_boolean is_switch_expr);

extern an_expr_node_ptr scan_void_expression(a_boolean repeated_in_loop,
                                             a_boolean marked_as_gnu_extension,
                                             a_boolean is_statement_expr);

extern an_expr_node_ptr scan_typed_expression(a_type_ptr    required_type,
					      an_error_code err_code);

extern void scan_default_arg_expr(a_param_type_ptr ptp);

extern an_expr_node_ptr scan_return_expression(
                                              a_type_ptr         required_type,
                                              an_error_code      err_code,
                                              a_dynamic_init_ptr *dip);

extern void scan_pp_expression(a_constant *constant);

extern void scan_integral_constant_expression(a_constant *constant);

extern void scan_fs_integral_constant_expression(a_constant *constant);

extern void scan_nonconstant_dimension_expression(
                                           a_boolean        is_vla_decl,
                                           a_boolean        *is_constant,
                                           an_expr_node_ptr *expression,
                                           a_constant       *constant);

#if GNU_EXTENSIONS_ALLOWED
an_expr_node_ptr scan_asm_operand_expression(a_boolean output);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_initializer_expression(
                                 a_type_ptr          required_type,
                                 a_decl_parse_state  *dps,
                                 a_boolean           static_lifetime,
                                 a_boolean           force_object_lifetime,
                                 a_boolean           suppress_object_lifetime,
                                 a_boolean           is_copy_initialization,
                                 a_boolean           *is_constant,
                                 an_expr_node_ptr    *expression,
                                 a_constant          *constant);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern an_expr_node_ptr prep_rvalue_arg_expr(an_expr_node_ptr  expr,
                                             a_param_type_ptr  param,
                                             a_source_position *err_pos);

#if !STANDALONE_UTILITY_PROGRAM
extern a_boolean scan_class_initializer_expression(a_decl_parse_state  *dps,
                                                   a_dynamic_init_ptr  *dip);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean scan_aggregate_initializer_expression(
                                   a_type_ptr         required_type,
                                   a_boolean          static_lifetime,
                                   a_boolean          suppress_object_lifetime,
                                   unsigned long      *levels_down,
                                   a_boolean          *is_constant,
                                   a_dynamic_init_ptr *dip,
                                   a_constant         *constant);

#if !STANDALONE_UTILITY_PROGRAM
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

extern an_arg_operand_ptr scan_nontype_template_argument(void);

extern a_boolean nontype_template_arg_is_compatible_with_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type);

extern void conv_nontype_template_arg_to_param_type(
                                            an_arg_operand_ptr arg_operand,
                                            a_type_ptr         param_type,
                                            a_constant         *constant);

#if !STANDALONE_UTILITY_PROGRAM
extern void scan_member_constant_initializer_expression(
                                                 a_decl_parse_state *dps,
                                                 a_constant         *constant);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void scan_constant_initializer_expression(a_type_ptr required_type,
                                                 a_constant *constant);

#if !STANDALONE_UTILITY_PROGRAM
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

extern a_type_ptr scan_decltype_operator(a_decl_pos_block  *decl_pos_block);

#if GNU_EXTENSIONS_ALLOWED 

extern a_type_ptr scan_typeof_operator(a_decl_pos_block  *decl_pos_block);

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

extern a_boolean arg_operand_contains_template_param(
                                               an_arg_operand_ptr arg_operand);

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
                                             a_boolean         *pass_by_value,
                                             a_source_position *dest_decl_pos);

extern void process_unattached_template_argument_list(
                                         a_template_arg_ptr template_arg_list);

extern an_expr_node_ptr make_assignment_expr(
                                      an_expr_node_ptr       lvalue_expr,
                                      an_expr_operator_kind  op,
                                      an_expr_node_ptr       rvalue_expr);

extern a_boolean compute_is_convertible(a_type_ptr  src_type,
                                        a_type_ptr  dst_type,
                                        a_boolean   src_is_rvalue);

#if EXPR_RANGE_MODIFIERS_IN_IL
extern void move_expr_range_modifiers(an_expr_node_ptr from_node,
                                      an_expr_node_ptr to_node);
#endif /* EXPR_RANGE_MODIFIERS_IN_IL */

/*
Flag controlling whether a (fairly expensive) check should be made for "lost"
expr range modifiers (i.e., modifiers that were attached to expr nodes that
were discarded as a result of some transformation of expression operands).
*/
#ifndef CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS
#if EXPR_RANGE_MODIFIERS_IN_IL && DEBUG && EXPENSIVE_CHECKING && \
    IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT
#define CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS TRUE
#else /* !(EXPR_RANGE_MODIFIERS_IN_IL && DEBUG && ...) */
#define CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS FALSE
#endif /* EXPR_RANGE_MDOIFIERS_IN_IL && DEBUG && ... */
#endif /* CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS */

#if CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS
#if !EXPR_RANGE_MODIFIERS_IN_IL
 #error CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS requires that \
        EXPR_RANGE_MODIFIERS_IN_IL be TRUE.
#endif /* !EXPR_RANGE_MODIFIERS_IN_IL */
#if !(IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT)
/* Checking for loss of range modifiers is done during the IL walk done
   while writing the IL in the alternate file format. */
 #error CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS requires that both \
        IL_SHOULD_BE_WRITTEN_TO_FILE and ALTERNATE_IL_FILE_FORMAT be TRUE
#endif /* !(IL_SHOULD_BE_WRITTEN_TO_FILE && ALTERNATE_IL_FILE_FORMAT) */

extern void remove_expr_range_modifier(an_expr_range_modifier_ptr ermp);
extern void forget_expr_range_modifiers_in_tree(an_expr_node_ptr top_expr,
                                                an_expr_node_ptr end_expr);
extern void forget_expr_range_modifiers_in_constant(a_constant_ptr con);
typedef struct an_operand *an_operand_ptr;
extern void forget_expr_range_modifiers_in_operand(an_operand_ptr operand);
extern void display_lost_expr_range_modifiers(void);
#endif /* CHECK_FOR_LOSS_OF_EXPR_RANGE_MODIFIERS */

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
Macro that is TRUE if the node is a variable address node.
*/
#define is_variable_address_node(node)					\
	((node)->kind == (an_expr_node_kind)enk_variable_address)

/*
Macro that is TRUE if the node is a routine address node.
*/
#define is_routine_address_node(node)					\
	((node)->kind == (an_expr_node_kind)enk_routine_address)

/*
Macro that is TRUE if the node is an error node.
*/
#define is_error_node(node)						\
	((node)->kind == (an_expr_node_kind)enk_error)

/*
Return TRUE if the operator in node "node" (which must be an operation
node) is "op".
*/
#define node_operator_is(node, op)                                      \
  ((node)->variant.operation.kind == (an_expr_operator_kind)(op))

/*
Return TRUE if "node" is a function call operation.
*/
#define is_call_node(node)                                              \
  (is_operation_node((node)) &&                                         \
   (node_operator_is((node), eok_call) ||                               \
    node_operator_is((node), eok_virtual_call) ||                       \
    node_operator_is((node), eok_pm_call)))

#endif /* ifndef EXPR_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2007 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
