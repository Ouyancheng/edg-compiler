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

/*
Flags used to specify options to copy_type_with_substitution.
*/
typedef int a_ctws_options_set;

#define CTWS_NO_OPTIONS			0x0
#define CTWS_IS_PARENT			0x1
			/* TRUE if the type being processed is the parent
			   type of a class member.  This affects the way
			   in which names are looked up during the
			   substitution process. */
#define CTWS_PROTOTYPE_ALLOWED		0x2
			/* TRUE if, when copying a type like A<T>, that the
			   prototype instantiation may be used in preference
			   to the nonreal class of the same name. */

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

extern a_symbol_ptr primary_template_of(a_symbol_ptr sym);

extern a_boolean rout_is_inline_template_function(a_routine_ptr	rout);

extern a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                        a_template_arg_ptr  *template_arg_list,
				 	a_boolean	    prototype_allowed);

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

/*
Bit vector used to pass flags into matches_template_type.
*/
typedef unsigned int an_mtt_flag_set;

#define MTT_NO_FLAGS 0x00
#define MTT_ALLOW_CONVERSION 0x01
			/* TRUE when a conversion from Derived<T>
			   to Base<T> may be done if needed. */
#define MTT_UNKNOWN_IMPLICIT_THIS_TYPE 0x02
			/* TRUE if the implicit this parameter may not
			   be known yet.  When this flag is set, a
			   NULL implicit this parameter type is
			   ignored (i.e., no attempt is made to match
			   it with the template type). */

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
				a_boolean	    is_decl_context);

extern
a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
					 a_template_arg_ptr explicit_arg_list,
                                         a_boolean          is_decl_context);

extern
a_boolean has_matching_template_instance(
				a_symbol_ptr		sym,
                                a_type_ptr		type,
				a_template_arg_ptr	explicit_arg_list);

extern a_symbol_ptr find_matching_template_instance(
			a_symbol_ptr		sym,
			a_type_ptr		type,
			a_template_arg_ptr	templ_arg_list,
			a_boolean		explicit_arg_list_present);

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

extern a_type_ptr copy_type_with_substitution(
				a_type_ptr			type,
				a_template_arg_ptr		templ_arg_list,
				a_template_nesting_depth	depth,
				a_source_position		*source_pos,
				a_ctws_options_set		options,
				a_boolean			*copy_error);

extern a_boolean equiv_template_arg_lists(
				a_template_arg_ptr list1,
				a_template_arg_ptr list2,
				an_equiv_templ_arg_options_set	options);

extern a_boolean equiv_template_param_lists(
				a_template_param_ptr	old_list,
				a_template_param_ptr	new_list,
				a_boolean		issue_errors,
				a_source_position	*error_pos);

extern void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp);

extern
void delayed_scan_for_function_template_default_args(
		    a_routine_ptr		     templ_rout,
		    a_routine_ptr		     rout_ptr,
                    a_template_instance_ptr	     tip,
                    a_template_symbol_supplement_ptr tssp,
                    a_boolean                        push_instantiation_scope);

extern void template_directive_or_declaration(
			a_token_kind			*final_token,
			a_template_decl_options_set	options);

extern
void set_nested_template_class_symbol_info(a_symbol_ptr  sym,
                                           a_type_kind	 type_kind);

extern void update_instantiation_required_flag(
                                        a_template_instance_ptr tip,
                                        a_boolean               value,
				        a_boolean	        defer_linline);

extern void process_deferred_instantiation_requests(void);

extern void instantiation_wrapup(void);

extern void templates_one_time_init(void);

extern void templates_init(void);

#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern void wrapup_auto_instantiation_information(void);
extern void update_auto_instantiation_flags(void);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void instantiation_pragma(a_pending_pragma_ptr	ppp);

EXTERN a_type_ptr
		type_of_unknown_templ_param_constant /* = NULL */;
			/* A type used for template parameter constants whose
			   real type cannot be known. */

EXTERN unsigned long
		defer_inline_function_fixup_and_instantiations;
			/* Nonzero if the fixup of inline function bodies and
                           nonclass instantiations should be deferred.
			   This causes instantiations to be placed on the
			   deferred_instantiations list instead of being
			   processed immediately.  Instantiations are also
			   deferred when pending_class_definitions is
			   nonzero. */

/* tp is a class type.  If it is incomplete, see if it is a template class in
   need of instantiation and, if so, instantiate it. */
#define instantiate_template_class(tp)                                  \
{								        \
  if (is_incomplete_type(tp)) {						\
    f_instantiate_template_class(tp);					\
  }  /* if */							        \
}

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
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
