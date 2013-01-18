/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

class_decl.h -- Declarations related to class_decl.c (having to do with
	        scanning declarations of C++ classes).

*/

/* Avoid including these declarations more than once: */
#ifndef CLASS_DECL_H
#define CLASS_DECL_H 1

#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* ifndef LANG_FEAT_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef DECLS_H
#include "decls.h"
#endif /* ifndef DECLS_H */

/*
Forward declarations needed:
(A dummy typedef is used to avoid spurious warnings in CodeCenter.)
*/
typedef struct a_class_def_state class_def_state_class_decl_dummy_typedef;

extern a_symbol_ptr find_corresp_prototype_tag_sym(a_symbol_ptr  curr_sym);

extern a_boolean conflicts_with_previous_function_decl(
                                                a_symbol_ptr       fund_sym,
                                                a_symbol_ptr       sym,
                                                a_source_position  *pos);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_routine_ptr find_finalize_routine(a_type_ptr class_type,
                                           a_boolean  *p_is_object_finalize);

extern a_boolean check_for_cli_delegate_definition(void);

extern void scan_cli_delegate_definition(a_decl_parse_state  *dps,
                                         a_symbol_locator    *loc,
                                         a_func_info_block   *func_info);

extern void create_cli_delegate_class_definition(
                                              a_type_ptr          class_type,
                                              a_scope_depth       decl_level,
                                              a_decl_parse_state  *dps,
                                              a_func_info_block   *func_info);

extern void scan_and_record_cli_delegate_definition(a_decl_parse_state  *dps);

extern void scan_cli_delegate_definition_from_assembly_import(void);

extern void complete_generic_constraint_type(a_type_ptr  proxy_class);

extern
void create_generic_constraint_types(a_template_decl_info_ptr  decl_info);

extern void make_boxed_enum_type(a_type_ptr  tp);

extern a_boolean in_cli_property_or_event_definition(void);

extern a_boolean in_static_cli_property_or_event_definition(void);

extern void check_initonly_members(a_type_ptr  class_type,
                                   a_boolean   static_ctor_def_seen);

extern void ensure_inclass_static_member_constant_initializer_is_scanned(
                                                         a_variable_ptr  var);

extern void merge_dll_flags_from_parent_class(a_type_ptr          class_type,
                                              a_decl_parse_state  *dps);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void form_exception_specification_for_generated_function(
                                                           a_routine_ptr  rp);

extern void remove_routine_typedef_if_needed(a_symbol_locator    *loc,
                                             a_decl_parse_state  *dps,
                                             a_boolean           no_cv_quals);

extern void add_noexcept_specification(a_routine_type_supplement_ptr  rtsp);

extern void check_for_conflicts_with_using_decls(
                                             a_symbol_ptr       overload_sym,
                                             a_source_position  *pos);

void prescan_member_function_default_arg_expr(a_param_type_ptr  ptp,
					      a_boolean		is_friend,
					      unsigned long	param_number);

extern a_symbol_ptr class_member_template_declaration(
                                     a_type_ptr            class_type,
                                     a_template_param_ptr  templ_param_list,
                                     a_template_ptr        il_template_entry,
                                     a_decl_pos_block_ptr  decl_pos_block_ptr);

extern a_type_ptr rescan_member_template_declaration(
                                           a_type_ptr               class_type,
                                           a_template_instance_ptr  instance);

extern void check_for_file_with_unterminated_type_definition(
                                                  a_source_position  *end_pos);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void set_target_of_conversion_function_flag_if_needed(
                                                      a_type_ptr  class_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean scan_class_definition(
                                   a_type_ptr       class_type,
                                   a_scope_depth    effective_decl_level,
                                   a_boolean        is_local_class,
                                   a_boolean        delayed_nested_class_def,
                                   a_boolean        is_template_instantiation,
				   a_boolean	    is_template_specialization,
                                   a_template_ptr   il_template_entry,
                                   a_decl_pos_block *decl_pos_block);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean is_lambda(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_lambda_ptr scan_lambda(void);

extern void default_argument_fixup_for_class(a_type_ptr  class_type,
                                             a_boolean   is_template_based,
                                             a_boolean   template_second_pass);

extern void process_deferred_class_fixups_and_instantiations(
						a_boolean for_instantiation);

extern void set_mixed_static_nonstatic_flag(a_symbol_ptr  overload_sym);

extern
void add_routine_fixup_for_specialization(a_type_ptr		class_type,
					  a_symbol_ptr		symbol,
					  a_func_info_block	*func_info,
					  a_token_cache_ptr	body_cache);

extern void add_routine_fixup_for_template_decl(
		a_symbol_ptr			symbol,
		a_symbol_ptr			prototype_scope_symbols,
		a_type_ptr			class_type,
		a_boolean			is_definition,
		a_def_arg_expr_fixup_ptr	default_args);

extern void check_member_decl_is_copy_constructor(
				a_routine_ptr		rout_ptr,
				a_type_ptr		class_type,
				a_boolean		compiler_generated);

extern void check_defaulted_or_deleted_function(a_decl_parse_state  *dps,
                                                a_func_info_block   *func_info,
                                                a_source_position   *diag_pos);

extern
a_lambda_capture_ptr lambda_capture_for_variable(a_variable_ptr		vp,
						 a_source_position_ptr	pos);

#if MICROSOFT_EXTENSIONS_ALLOWED
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
extern a_boolean microsoft_routine_def_is_unmovable(a_boolean
                                                          explicit_overrider);
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_two_argument_delete(a_routine_ptr delete_routine);

extern void abstract_class_diagnostic(an_error_severity  severity,
                                      an_error_code      error_code,
                                      a_type_ptr         class_type,
                                      a_source_position  *error_pos);

extern void check_anonymous_union_symbols(
                                   a_symbol_ptr              assoc_object_sym,
                                   struct a_class_def_state  *cdsp,
                                   a_boolean                 is_nonstd);

#if NEW_CAN_BE_FOLDED_INTO_CTOR
extern void set_class_assoc_operator_new_routine(a_type_ptr     class_type);
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

extern a_symbol_ptr find_class_assoc_operator_delete_routine(
                                                      a_type_ptr class_type,
                                                      a_boolean  *ambiguous);

#if DELETE_CAN_BE_FOLDED_INTO_DTOR
extern void set_class_assoc_operator_delete_routine(a_type_ptr class_type);
#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

extern
a_boolean is_implicitly_callable_conversion_function(a_type_ptr rout_type);

extern a_boolean is_assignment_operator_for_copy(
                                   a_symbol_ptr          sym,
                                   a_boolean             move_assign_okay,
                                   a_boolean             *is_ref_arg,
                                   a_type_qualifier_set  *qualifiers,
                                   a_boolean             *is_base_class_match);

extern
void update_friend_function_info(a_routine_ptr rout_ptr,
                                 a_type_ptr    class_type);

extern void check_for_invalid_friend_declaration(
					a_type_ptr		parent_type,
					a_symbol_ptr		sym,
					a_symbol_locator	*locator);

extern void decl_friend_class(a_type_ptr  class_type,
			      a_type_ptr  friend_class_type,
                              a_boolean   for_friend_template);

extern a_symbol_ptr member_function_redecl_sym(
                                       a_symbol_ptr          sym,
                                       a_decl_parse_state    *dps,
                                       a_template_param_ptr  templ_param_list,
                                       a_symbol_ptr          *other_match);

extern void update_routine_type_exception_specification_if_needed(
                                                        a_routine_ptr  rp,
                                                        a_type_ptr     *p_tp);

extern a_base_class_ptr find_disambiguator(a_base_class_ptr  bcp1,
                                           a_base_class_ptr  bcp2);

extern a_derivation_step_ptr make_derivation_step(
                                            a_base_class       *base_class,
                                            a_derivation_step  *existing_step);

extern void free_derivation_step(a_derivation_step_ptr  step);

extern a_boolean congruent_paths(a_derivation_step_ptr  dsp1,
                                 a_derivation_step_ptr  dsp2);

#if IA64_ABI

extern a_base_class_ptr nominal_primary_base(a_base_class_ptr  bcp);

#endif /* IA64_ABI */

extern void check_class_linkage(void);

extern void class_decl_one_time_init(void);

extern void class_decl_trans_unit_init(void);

extern void class_decl_init(void);

extern void define_type_info_pragma(a_pending_pragma_ptr    ppp,
				    a_symbol_ptr            sym,
				    a_statement_ptr         stmt);

extern void add_to_deferred_friend_function_fixup_list(
						a_routine_fixup_ptr	rfp);

extern void process_deferred_friend_fixup_list(void);

/*
Macro to consume and ignore certain right parentheses in declarations.  This
is used to emulate a strange bug in some versions of the Microsoft compiler.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
extern void f_consume_any_stray_microsoft_rparen(void);

#define consume_any_stray_microsoft_rparen()                             \
  if (curr_token == tok_rparen && microsoft_bugs) {                      \
    f_consume_any_stray_microsoft_rparen();                              \
  }  /* if */
#else /* MICROSOFT_EXTENSIONS_ALLOWED */
#define consume_any_stray_microsoft_rparen()  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if USE_X86_64
extern a_type_ptr make_va_list_tag_type(void);
#endif /* USE_X86_64 */

#if DEBUG
extern unsigned long db_show_routine_fixups_used(unsigned long grand_total);

extern unsigned long db_show_class_fixups_used(unsigned long grand_total);

extern unsigned long db_show_override_registry_entries_used(
                                                   unsigned long grand_total);

extern unsigned long db_show_initializer_fixups_used(
                                                   unsigned long grand_total);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern unsigned long db_show_quasi_override_descrs_used(
                                                   unsigned long grand_total);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern unsigned long db_show_override_exception_check_entries_used(
                                                   unsigned long grand_total);

#if IA64_ABI
extern unsigned long db_show_covariant_overrides_used(
                                                   unsigned long grand_total);
#endif /* IA64_ABI */

extern void db_path(a_derivation_step_ptr dsp,
                    a_boolean             show_offset);

extern void db_abbreviated_base_class(a_base_class_ptr  bcp);

extern void db_base_class(a_base_class_ptr  bcp,
                          a_boolean         show_offset);

extern void db_base_class_list(a_type_ptr tp);

extern void db_all_virtual_function_override_lists(a_type_ptr  class_type);
#endif /* DEBUG */

#endif /* CLASS_DECL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
