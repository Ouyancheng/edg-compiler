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

#include "il.h"
#include "symbol_tbl.h"

extern a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                        a_template_arg_ptr  *template_arg_list,
                                        a_source_position   *source_pos,
				 	a_boolean	    prototype_allowed);

extern void scan_a_template_parameter_declaration
                                          (a_symbol_locator *param_locator,
					   a_type_ptr       *param_type_ptr);

extern a_boolean matches_template_type
                                  (a_type_ptr          type,
                                   a_type_ptr          templ_type,
                                   a_template_arg_ptr  *templ_arg_list,
                                   a_boolean           allow_conversion,
                                   a_base_class_ptr   *base_class_conv_needed);

extern a_symbol_ptr find_template_function(a_symbol_ptr        templ_sym,
                                           a_template_arg_ptr  *templ_arg_list,
                                           a_source_position   *source_pos);

extern a_symbol_ptr matching_template_function
                                  (a_symbol_ptr        function_template_sym,
                                   a_type_ptr          curr_type,
                                   a_source_position   *source_pos);

extern void record_predeclared_template_function(a_symbol_ptr  templ_sym,
                                                 a_symbol_ptr  rout_sym);

extern void find_member_function_template(
                                    a_symbol_ptr  rout_sym,
                                    a_symbol_ptr  corresp_prototype_tag_sym);

extern void find_static_data_member_template(
                                    a_symbol_ptr  static_data_member_sym,
                                    a_symbol_ptr  corresp_prototype_tag_sym);

extern void f_check_for_uninstantiated_template_class(a_type_ptr  type);

extern void f_instantiate_template_class(a_type_ptr  type);

extern a_boolean equiv_template_arg_lists(a_template_arg_ptr list1,
                                          a_template_arg_ptr list2,
                                          a_boolean          is_func_template);

extern void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp);

extern void delayed_scan_for_function_template_default_args
			 (a_routine_ptr			   templ_rout,
			  a_routine_ptr			   rout_ptr,
			  a_template_symbol_supplement_ptr tssp);

extern a_symbol_ptr template_declaration(a_boolean  *defines_something);
extern void update_instantiation_required_flag(a_template_instance_ptr tip,
                                               a_boolean               value);
extern void instantiation_wrapup(void);
extern void templates_init(void);
#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern void create_or_remove_instantiation_information_file(void);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern void instantiation_pragma(a_pending_pragma_ptr	ppp);


/* If tp is a class in need of instantiation, instantiate it.  Otherwise,
   do nothing. */
#define check_for_uninstantiated_template_class(tp)                     \
{									\
  if (C_dialect == C_dialect_cplusplus) {				\
    if (is_incomplete_type(tp)) {					\
      f_check_for_uninstantiated_template_class(tp);		        \
    }  /* if */							        \
  }  /* if */							        \
}

/* tp is a class type.  If it is incomplete, see if it is a template class in
   need of instantiation and, if so, instantiate it. */
#define instantiate_template_class(tp)                                  \
{								        \
  if (is_incomplete_type(tp)) {						\
    f_instantiate_template_class(tp);					\
  }  /* if */							        \
}

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
