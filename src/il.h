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

il.h -- Declarations related to the intermediate language.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_H
#define IL_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

/* il_def.h contains the definition of the IL tables.  It's in a separate
   file so that it can be included in back ends without dragging in all of the
   front end tables. */
#include "il_def.h"


/* Type of a scope nesting depth.  This is the depth within the scope_stack. */
/* Defined here (instead of the more obvious symbol_tbl.h) because it's
   used by add_to_types_list and we want to avoid including symbol_tbl.h
   from il.h (it would make life complicated for the standalone il_display
   and c_gen_be). */
typedef int	a_scope_depth;

#define NO_SCOPE_DEPTH (-1)
#define DEPTH_OF_FILE_SCOPE 0

/* Current memory region number for IL information. */
EXTERN a_memory_region_number
		curr_il_region_number;

EXTERN a_boolean
		curr_initial_il_walk_flag_setting;
			/* Value currently to be used as the initial value for
			   il_walk_flag when entries are created. */

/*
Macro that generates a unique unsigned long identifier from an IL pointer.
This is useful for generating names for unnamed symbols, for cross-reference
information, and for debug prints.  On most machines, the unique identifier
can simply be the pointer converted to unsigned long.  If that won't work,
a function can be substituted that does something else.
*/
#define unique_id_for_il_pointer(ptr) ((unsigned long)(ptr))


extern char *alloc_il(sizeof_t size);

extern char *alloc_cil(sizeof_t size);

extern void set_constant_kind(a_constant           *cp,
                              a_constant_repr_kind kind);

extern void clear_constant(a_constant           *cp,
                           a_constant_repr_kind kind);

extern void set_error_constant(a_constant *cp);

extern void set_arg_transfer_method_flag(a_param_type_ptr ptp);

extern a_param_type_ptr alloc_param_type(a_type_ptr type,
                                         a_boolean  at_file_scope);

extern an_access_adjustment_ptr alloc_access_adjustment(
                                              an_access_adjustment_kind  kind);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern an_overriding_virtual_function_ptr
                                       alloc_overriding_virtual_function(void);

extern a_base_class_ptr alloc_base_class(void);

extern void set_type_kind(a_type_ptr  pte,
                          a_type_kind kind);

extern void clear_type(a_type_ptr  pte,
                       a_type_kind kind);

extern a_type_ptr alloc_type(a_type_kind kind);

extern a_type_ptr fs_type(a_type_kind kind);

extern void add_to_types_list(a_type_ptr     type_ptr,
                              a_scope_depth  scope_level,
                              a_boolean      in_old_style_param_decl_list);

extern a_type_ptr integer_type(an_integer_kind kind);

extern a_type_ptr signed_int_type(void);

extern a_type_ptr float_type(a_float_kind kind);

extern a_type_ptr string_type(a_targ_size_t num_chars);

extern a_type_ptr wide_string_type(a_targ_size_t num_chars);

extern an_integer_kind char_int_kind_from_string_type(a_type_ptr str_type);

extern a_type_ptr error_type(void);

extern a_type_ptr unknown_type(void);

extern a_type_ptr void_type(void);

extern a_type_ptr ptr_to_member_type(a_type_ptr  member_type,
                                     a_type_ptr  class_type);

extern a_type_ptr get_based_type(a_type_ptr        base_type,
                                 a_based_type_kind kind);

extern void add_based_type_list_member(a_type_ptr        base_type,
                                       a_based_type_kind kind,
                                       a_type_ptr        based_type);

extern a_type_ptr make_pointer_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_qualified_type(a_type_ptr old_type,
                                      a_boolean  is_const,
                                      a_boolean  is_volatile);
extern a_type_ptr make_unqualified_type(a_type_ptr old_type);

extern void set_routine_calling_method_flag(a_type_ptr routine_type);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern void switch_il_region(a_memory_region_number region_number);

extern void switch_to_file_scope_region(
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_to_function_scope_region(
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_back_to_original_region(
                              a_memory_region_number region_to_switch_back_to);

extern a_scope_ptr new_il_region(a_scope_kind   kind,
                                 a_scope_number scope_number,
                                 a_routine_ptr  assoc_routine);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

extern void add_to_constants_list(a_constant_ptr con_ptr);

extern void add_shareable_constants_to_constants_list(void);

extern void set_integer_constant(a_constant      *cp,
                                 long            value,
                                 an_integer_kind kind);

extern void make_zero_of_proper_type(a_type_ptr desired_type,
                                     a_constant *zero_constant);

extern char *alloc_text_of_string_literal(sizeof_t size);

extern void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                                  a_dynamic_init_kind kind);

extern void clear_dynamic_init(a_dynamic_init_ptr  dip,
                               a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dtor_dynamic_init(a_dynamic_init_kind kind,
                                                  a_type_ptr          type);

extern void add_to_dynamic_inits_list(a_dynamic_init_ptr dip);

extern a_variable_ptr alloc_variable(void);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_scope_depth  scope_depth);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type);

extern a_field_ptr alloc_field(void);

extern a_routine_ptr alloc_routine(void);

extern void remove_from_routines_list(a_routine_ptr rout_ptr);

extern void add_to_routines_list(a_routine_ptr rout_ptr,
                                 a_boolean    at_file_scope);

extern a_label_ptr alloc_label(void);

extern void add_to_labels_list(a_label_ptr label_ptr);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

extern void set_node_operator(an_expr_node_ptr      node,
                              an_expr_operator_kind kind,
	   	              a_type_ptr            type,
		              an_expr_node_ptr      operands);

extern an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	           a_type_ptr            type,
			   	           an_expr_node_ptr      operands);

extern an_expr_node_ptr error_node(void);

extern an_expr_node_ptr alloc_node_for_constant(a_constant *constant);

extern an_expr_node_ptr node_for_integer_constant(long            value,
                                                  an_integer_kind kind);

extern an_expr_node_ptr copy_node(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_expr_tree(an_expr_node_ptr expr);

extern an_expr_node_ptr var_lvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_rvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr function_addr_expr(a_routine_ptr rout);

extern an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr this_param_value_expr(void);

extern an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern an_expr_node_ptr field_rvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern an_expr_node_ptr base_class_selection_expr(an_expr_node_ptr node,
                                                  a_base_class_ptr bcp);

extern a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                                 an_expr_node_ptr source);

extern a_statement_ptr make_call_assignment_statement(a_routine_ptr    rout,
                                                      an_expr_node_ptr dest,
                                                      an_expr_node_ptr source);

extern a_switch_clause_ptr alloc_switch_clause(void);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind);

extern a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind);

extern a_scope_ptr alloc_scope(a_scope_kind   kind,
                               a_scope_number number,
                               a_routine_ptr  assoc_routine);

extern void record_start_of_source_file(a_source_file_ptr parent_file,
	  		                a_seq_number      seq_number,
				        a_line_number     line_number,
			                char              *file_name,
			                char              *full_name,
			                a_source_file_ptr *new_file);
extern void record_end_of_source_file(a_source_file_ptr curr_file,
			              a_seq_number      seq_number);
extern void conv_seq_to_file_and_line(a_seq_number  seq_number,
			              char          **file_name,
				      char          **full_name,
				      a_line_number *line_number,
                                      a_boolean     *at_end_of_source);
extern a_boolean seq_is_in_include_file(a_seq_number seq_number);

extern void set_default_source_corresp(a_source_correspondence *sc);

/* Copy a constant entry. */
#define copy_constant(from, to) (*(to) = *(from))

/*
Return TRUE if a constant is an error constant.
*/
#define is_error_constant(cp) ((cp)->kind == (a_constant_repr_kind)ck_error)

/* Macro that returns TRUE if a variable's storage class has static storage
   duration.  See 3.1.2.4.  Note that storage classes have been 
   standardized during declaration processing. */
/* There is a copy of this macro, under the name static_storage_class,
   in c_gen_be.c.  If you change this, you should probably change that
   definition as well. */
#define has_static_storage_duration(storage_class)                    \
  ((storage_class) == (a_storage_class)sc_static ||                   \
   (storage_class) == (a_storage_class)sc_extern ||                   \
   (storage_class) == (a_storage_class)sc_unspecified)

#if DEBUG
extern void db_name(a_source_correspondence *sc);

extern void db_access_control(an_access_specifier as);

extern void db_constant(a_constant *cp);

extern void db_field(a_field *fp);

extern void db_type(a_type *tp);

extern void db_abbreviated_type(a_type *tp);

extern void db_variable(a_variable_ptr var_ptr);

extern void db_expression(an_expr_node_ptr node);

extern void db_dynamic_initializer(a_dynamic_init_ptr  dip,
                                   int                 level);

extern void db_initializer(a_variable_ptr  var_ptr,
                           int             level);

extern unsigned long show_il_space_used(void);
#endif /* DEBUG */

extern void il_init(void);

#endif /* ifndef IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
