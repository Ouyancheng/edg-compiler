/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

templates.h -- Declarations relating to templates.c (template support)

*/

/* Avoid including these declarations more than once: */
#ifndef TEMPLATES_H
#define TEMPLATES_H 1

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */

/*
Flags used to specify options to the template declaration processing routines.
*/
typedef int a_template_decl_options_set;

#define TDO_NO_OPTIONS		0x0
#define TDO_EXTERN		0x1
			/* TRUE if the "extern" keyword was specified
			   before the "template" keyword. */

/*
Flags used to specify options to equiv_template_arg_lists.
*/
typedef int an_equiv_templ_arg_options_set;

#define ETA_NO_OPTIONS			0x0
#define ETA_ERROR_MATCHES_ANYTHING	0x1
			/* TRUE if an error type or constant will match
			   anything (this is used for compatibility checking
			   instead of equivalence checking). */
#define ETA_IS_NONREAL_MEMBER		0x2
			/* TRUE if the template is a member of a nonreal
			   class and has no template parameter list.  In such
			   cases, a NULL argument list, and argument lists of
			   different lengths are permitted.  */
#define ETA_IGNORE_UNKNOWN_ARG_VALUES	0x4
			/* TRUE if some of the arguments may have NULL type
			   or constant pointers and should be ignored
			   (i.e., be considered to match) for purposes
			   of this comparison.  This is used when
			   comparing an incomplete argument list specified
			   as an explicit function template argument list
			   with a complete list.  The unspecified arguments
			   will be represented in the list with NULL type
			   or constant pointers. */
#define ETA_MS_IGNORE_QUALIFIERS	0x8
			/* TRUE if, in Microsoft bugs mode, top level
			   qualifiers should be ignored when comparing two
			   argument lists. */
#define ETA_IS_PROTOTYPE		0x10
			/* TRUE if the first template argument list is
			   the argument list for a prototype instantiation of
			   a class template or a prototype instantiation of
			   a partial specialization. */

/*
Flags used to specify options to equiv_template_param_lists.
*/
typedef int an_equiv_templ_param_options_set;

#define ETP_NO_OPTIONS			0x0
#define ETP_BAD_PARAM_TYPE_OKAY		0x1
			/* TRUE if a nontype parameter initially declared with
			   one type may be redeclared later with a different
			   type.  This is used to emulate Microsoft and
			   g++ bugs. */

/*
Flags used to specify options to set_instance_requried and
update_instantiation_required_flag.
*/
typedef int a_set_instance_required_options_set;
#define SIR_NONE		0x0
#define SIR_DEFER_INLINE	0x1
			/* TRUE if the instantiation of an inline function
			   should be deferred and not done immediately upon
			   the call. */
#define SIR_CLEAR_VALUE		0x2
			/* Normally the value specified on a call is merged
			   with the earlier value.  This flag forces the
			   instance required flag to be cleared. */
#define SIR_GPP_FORCE_INLINE	0x4
			/* TRUE if, in g++ mode, an inline function should be
			   instantiated when it is put on the instantiations
			   required list rather than waiting until the end
			   of the translation unit. */

/*
Structure used to represent the information found an in export information
file.
*/
typedef struct an_export_info_file *an_export_info_file_ptr;
typedef struct an_export_info_file {
  a_directory_name_entry_ptr
		dir_name_entry;
			/* The directory name entry for the export template
			   search directory associated with this file. */
  char		*file_name;
			/* The name of the export information file. */
  a_directory_name_entry_ptr
		incl_search_path;
			/* The include search path to be used. */
  a_directory_name_entry_ptr
		end_incl_search_path;
			/* The end of the include search path. */
  a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The system include search path to be used (i.e.,
			   for includes of the form <...>). */
} an_export_info_file;

/*
Structure used to keep track of the class template partial specializations
or function templates that match a given instance.
*/
typedef struct a_partial_order_candidate *a_partial_order_candidate_ptr;
typedef struct a_partial_order_candidate {
  a_partial_order_candidate_ptr
		next;
			/* Next entry in the list. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol associated with a
			   given partial specialization or function
			   template. */
  a_template_arg_ptr
		template_arg_list;
			/* Template argument list to be used if this partial
			   specialization or function template is used to
			   generate the instance. */
} a_partial_order_candidate;


extern void add_to_partial_order_candidates_list(
			a_partial_order_candidate_ptr	*psc_list,
			a_symbol_ptr			new_sym,
			a_template_arg_ptr		templ_arg_list);

extern void select_best_partial_order_candidate(
			a_partial_order_candidate_ptr	psc_list,
			a_symbol_ptr			instance_sym,
			a_symbol_ptr			*best_sym,
			a_template_arg_ptr		*best_arg_list,
			a_boolean			*p_ambiguous);

extern
a_template_cache_ptr cache_for_template(a_template_symbol_supplement_ptr tssp);

extern a_template_arg_ptr templ_arg_list_for_class(a_type_ptr class_type);

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern a_src_seq_secondary_decl_ptr
                            secondary_src_seq_for_template(a_template_ptr  tp);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_symbol_ptr primary_template_of(a_symbol_ptr sym);

extern a_boolean rout_is_inline_template_function(a_routine_ptr rout,
                                                  a_boolean     in_class);

extern a_boolean template_arg_list_involves_template_param(
					a_template_arg_ptr	tap);

extern a_symbol_ptr find_template_class(
			     a_symbol_ptr        class_template_sym,
                             a_template_arg_ptr  *new_list,
			     a_boolean	         any_prototype_allowed,
			     a_symbol_ptr        specific_prototype_allowed);

extern a_namespace_ptr determine_referencing_namespace(void);

extern a_template_arg_ptr get_template_arg_by_list_pos(
                                    a_template_param_ptr      templ_param_list,
                                    a_template_arg_ptr        *templ_arg_list,
                                    a_template_param_list_pos pos);

extern a_type_ptr rescan_template_constant_parameter
                                     (a_symbol_ptr	   template_sym,
                                      a_symbol_ptr	   param_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list,
                                      a_boolean		   do_default_arg,
                                      a_constant_ptr       *constant);

extern a_type_ptr rescan_template_type_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list);

extern a_template_ptr rescan_template_template_default_arg
                                     (a_symbol_ptr	   template_sym,
			              a_template_param_ptr param_ptr,
				      a_template_arg_ptr   arg_list);

/*
Bit vector used to pass flags into matches_template_type.
*/
typedef unsigned int an_mtt_flag_set;

#define MTT_NO_FLAGS 0x00
#define MTT_ALLOW_BASE_CONVERSION 0x01
			/* TRUE when a conversion from Derived<T>
			   to Base<T> may be done if needed. */
#define MTT_UNKNOWN_THIS_CLASS_TYPE 0x02
			/* TRUE if the this class type may not
			   be known yet.  When this flag is set, a
			   NULL this class type is ignored (i.e., no attempt
                           is made to match it with the template type). */
#define MTT_IS_CONVERSION_TEMPLATE 0x04
			/* TRUE if argument deduction is being done in the
			   context of a conversion template return type. */

extern a_boolean matches_template_type_with_qualification_conversion(
				a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
				a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags);

extern
a_boolean matches_template_type(a_type_ptr           type,
                                a_type_ptr           templ_type,
                                a_template_arg_ptr   *templ_arg_list,
                                a_template_param_ptr templ_param_list,
				an_mtt_flag_set      flags);

extern
a_boolean tentatively_matches_template_type(
			       a_type_ptr           type,
		  	       a_type_ptr           templ_type,
                               a_template_param_ptr templ_param_list);

extern a_type_ptr substitute_template_arguments(
				a_symbol_ptr		templ_sym,
				a_template_arg_ptr	templ_arg_list,
				a_template_arg_ptr	*new_arg_list,
				a_template_param_ptr	templ_param_list);

extern a_type_ptr wrapup_function_template_argument_deduction(
				a_template_arg_ptr   templ_arg_list,
                                a_symbol_ptr         rout_templ_sym,
                                a_template_param_ptr templ_param_list);

extern a_symbol_ptr find_template_function(
			a_symbol_ptr		templ_sym,
                        a_template_arg_ptr	*new_list,
			a_boolean		explicit_arg_list_present,
                        a_source_position	*source_pos);

extern a_boolean is_match_for_function_template(
				a_symbol_ptr		templ_sym,
				a_type_ptr		curr_type,
				a_template_arg_ptr	*templ_arg_list,
				a_symbol_ptr		*instance_sym,
				a_template_param_ptr	templ_param_list,
				a_template_arg_ptr	explicit_arg_list,
				a_boolean		is_decl_context);

extern a_symbol_ptr matching_template_function(
				a_symbol_ptr        templ_sym,
                                a_type_ptr          curr_type,
				a_template_arg_ptr  explicit_arg_list,
				a_boolean	    explicit_arg_list_present,
				a_boolean	    is_decl_context,
				a_boolean	    in_class_specialization,
				a_boolean	    *is_new_template_instance);

extern
a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
					 a_template_arg_ptr explicit_arg_list,
                                         a_boolean          is_decl_context);

extern a_type_ptr explicit_arg_list_identifies_specialization(
				a_symbol_ptr		template_sym,
				a_template_arg_ptr	templ_arg_list,
				a_template_arg_ptr	*new_arg_list);

extern
a_boolean has_matching_template_instance(
				a_symbol_ptr		sym,
                                a_type_ptr		type,
				a_template_arg_ptr	explicit_arg_list);

extern a_symbol_ptr find_matching_template_instance(
			a_symbol_ptr		sym,
			a_type_ptr		type,
			a_template_arg_ptr	explicit_arg_list,
			a_boolean		explicit_arg_list_present,
			a_boolean		in_class_specialization,
			an_error_severity	severity_if_not_found);

extern int compare_function_templates(
				a_symbol_ptr 		templ_sym1,
				a_symbol_ptr		templ_sym2);

extern void record_predeclared_template_function(
                                        a_symbol_ptr         templ_sym,
                                        a_symbol_ptr         rout_sym,
                                        a_template_param_ptr templ_param_list);

extern void find_member_function_template(
                                    a_symbol_ptr  rout_sym,
                                    a_symbol_ptr  corresp_prototype_tag_sym);

extern void find_static_data_member_template(
                                    a_symbol_ptr  static_data_member_sym,
                                    a_symbol_ptr  corresp_prototype_tag_sym);

extern void check_for_uninstantiated_template_class(a_type_ptr  type);

extern void f_instantiate_template_class(a_type_ptr  type);

extern a_boolean is_template_param_from_list(
		        a_template_param_coordinate_ptr	coordinates,
			a_template_param_ptr		templ_param_list);

extern a_template_arg_ptr copy_template_arg_list_with_substitution(
			a_template_arg_ptr		arg_list_to_copy,
			a_template_param_ptr		param_list_for_copy,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error);

extern a_symbol_ptr copy_parent_type_with_substitution(
			a_symbol_ptr			sym,
			a_type_ptr			parent_type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_boolean			is_type,
			a_ctws_options_set		options,
			a_boolean			*copy_error);

extern a_type_ptr copy_type_with_substitution(
			a_type_ptr			type,
			a_template_arg_ptr		templ_arg_list,
			a_template_param_ptr		templ_param_list,
			a_source_position		*source_pos,
			a_ctws_options_set		options,
			a_boolean			*copy_error);

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern a_type_ptr instantiate_type_for_template_function(
                                                      a_type_ptr     type,
                                                      a_routine_ptr  routine);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_boolean equiv_template_arg_lists(
				a_template_arg_ptr list1,
				a_template_arg_ptr list2,
				an_equiv_templ_arg_options_set	options);

extern a_boolean equiv_template_param_lists(
			a_template_param_ptr			old_list,
			a_template_param_ptr			new_list,
			a_boolean				issue_errors,
			an_equiv_templ_param_options_set	options,
			a_source_position			*error_pos);

extern a_boolean identical_templates_given_symbol(a_symbol_ptr	sym1,
					          a_symbol_ptr	sym2);

extern a_boolean equiv_templates(a_template_ptr	templ1,
				 a_template_ptr	templ2);

extern a_boolean equiv_templates_given_supplement(
				a_template_symbol_supplement_ptr	tssp1,
				a_template_symbol_supplement_ptr	tssp2);

extern void prescan_function_template_default_arg_expr(
					a_param_type_ptr  ptp,
					a_scope_depth	  assoc_scope_depth);

extern void instantiate_default_argument(a_symbol_ptr		rout_sym,
					 a_param_type_ptr	param);

extern void default_arg_prototype_instantiation(
	a_symbol_ptr				template_sym,
	a_def_arg_expr_fixup_ptr		def_arg_list,
	a_symbol_ptr				prototype_scope_symbols,
        a_boolean                               update_declared_type);

extern a_template_arg_ptr create_prototype_arg_list(
			a_template_param_ptr	templ_param_list);

extern void function_prototype_instantiation(
			a_symbol_ptr		template_sym);

#if ONE_INSTANTIATION_PER_OBJECT
extern void set_routine_instantiation_needed_bit_number(a_routine_ptr routine);
extern void set_variable_instantiation_needed_bit_number(
                                                      a_variable_ptr variable);
#endif /* ONE_INSTANTIATION_PER_OBJECT */

extern void template_directive_or_declaration(
			a_token_kind			*final_token,
			a_template_decl_options_set	options);

extern
void set_nested_template_class_symbol_info(a_symbol_ptr  sym,
                                           a_type_kind	 type_kind);

extern
void update_nested_template_class_symbol_info(a_symbol_ptr       sym,
                                              a_type_kind	 type_kind);

extern
void set_instance_required(a_symbol_ptr				sym,
			   a_boolean				value,
			   a_set_instance_required_options_set	options);

extern void process_deferred_instantiation_requests(void);

extern void set_master_instance_for_new_canonical_routine(
					a_routine_ptr	primary_routine,
					a_routine_ptr	secondary_routine);

extern void set_master_instance_for_new_canonical_variable(
					a_variable_ptr	primary_variable,
					a_variable_ptr	secondary_variable);

extern void set_master_instance_information(void);

extern void template_and_inline_function_wrapup(void);

extern void record_cache_checksum(
	       a_template_symbol_supplement_ptr	tssp,
	       a_token_cache			*p_template_body_cache);

extern void add_to_inline_function_list(a_routine_ptr	rout_ptr);

extern
a_type_ptr type_if_unknown_conversion_function_symbol(a_symbol_ptr	sym);

extern void check_specialization_scope(a_symbol_ptr	     sym,
				       a_source_position     *pos);

extern void templates_one_time_init(void);

extern void templates_trans_unit_init(void);

extern void templates_init(void);

#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern void wrapup_auto_instantiation_information(void);
extern void update_auto_instantiation_flags(void);
extern void update_inline_function_flags(void);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void instantiation_pragma(a_pending_pragma_ptr	ppp);

EXTERN a_type_ptr
		type_of_unknown_templ_param_nontype /* = NULL */;
			/* A type used for template parameter nontype values
			   and expressions whose real type cannot be known. */

EXTERN unsigned long
		defer_inline_function_fixup_and_instantiations;
			/* Nonzero if the fixup of inline function bodies and
                           nonclass instantiations should be deferred.
			   This causes instantiations to be placed on the
			   deferred_instantiations list instead of being
			   processed immediately.  Instantiations are also
			   deferred when pending_class_definitions is
			   nonzero. */

EXTERN a_symbol_list_entry_ptr
		exported_templates_list;
			/* List of exported templates whose definitions
			   were provided in this compilation.  This list
			   includes only functions and static data members
			   (i.e., not classes). */

/* tp is a class type.  If it is incomplete, see if it is a template class in
   need of instantiation and, if so, instantiate it. */
#define instantiate_template_class(tp)                                  \
{								        \
  if (is_incomplete_type(tp) && is_class_struct_union_type(tp)) {	\
    f_instantiate_template_class(tp);					\
  }  /* if */							        \
}

/*
Return TRUE if there have been any exported templates defined in the current
translation unit.
*/
#define any_exported_templates()					\
  (exported_templates_list != NULL)


extern a_boolean is_nontemplate_routine_from_exported_trans_unit(
						a_routine_ptr rout_ptr);

#if DEBUG
extern unsigned long db_show_template_space_used(unsigned long grand_total);
#endif /* DEBUG */

#endif /* TEMPLATES_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
