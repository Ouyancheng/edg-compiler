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

extern
a_template_cache_ptr cache_for_template(a_template_symbol_supplement_ptr tssp);

extern a_template_arg_ptr templ_arg_list_for_class(a_type_ptr class_type);

extern a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                        a_template_arg_ptr  *template_arg_list,
				 	a_boolean	    prototype_allowed);

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
				an_mtt_flag_set      flags,
                                a_base_class_ptr     *base_class_conv_needed);

extern
a_boolean tentatively_matches_template_type(
			       a_type_ptr           type,
		  	       a_type_ptr           templ_type,
                               a_template_param_ptr templ_param_list);

extern a_boolean verify_function_template_nontype_args(
                                        a_template_arg_ptr   templ_arg_list,
                                        a_symbol_ptr         rout_templ_sym,
                                        a_template_param_ptr templ_param_list);


extern a_symbol_ptr find_template_function(a_symbol_ptr        templ_sym,
                                           a_template_arg_ptr  *templ_arg_list,
                                           a_source_position   *source_pos);

extern a_boolean is_match_for_function_template(
                                       a_symbol_ptr         templ_sym,
                                       a_type_ptr           curr_type,
                                       a_template_arg_ptr   *templ_arg_list,
                                       a_symbol_ptr         *instance_sym,
                                       a_template_param_ptr templ_param_list,
                                       a_boolean	    is_decl_context);

extern a_symbol_ptr matching_template_function
                                  (a_symbol_ptr        function_template_sym,
                                   a_type_ptr          curr_type,
				   a_boolean	       is_decl_context);

extern
a_boolean has_matching_template_function(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
                                         a_boolean          is_decl_context);

extern
a_boolean has_matching_template_instance(a_symbol_ptr      sym,
                                         a_type_ptr        type);

extern
a_symbol_ptr find_matching_template_instance(a_symbol_ptr      sym,
                                             a_type_ptr        type);

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

extern a_boolean equiv_template_arg_lists(
                                    a_template_arg_ptr list1,
                                    a_template_arg_ptr list2,
                                    a_boolean          error_matches_anything,
                                    a_boolean          is_nonreal_member);

extern void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp);

extern
void delayed_scan_for_function_template_default_args(
		    a_routine_ptr		     templ_rout,
		    a_routine_ptr		     rout_ptr,
                    a_template_instance_ptr	     tip,
                    a_template_symbol_supplement_ptr tssp,
                    a_boolean                        push_instantiation_scope);

extern void template_directive_or_declaration(a_token_kind  *final_token);

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
extern void create_or_remove_instantiation_information_file(void);
extern void update_auto_instantiation_flags(void);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void instantiation_pragma(a_pending_pragma_ptr	ppp);

EXTERN a_type_ptr
		type_of_unknown_templ_param_constant /* = NULL */;
			/* A type used for template parameter constants whose
			   real type cannot be known. */

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
