/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

il.h -- Declarations related to the intermediate language.

*/

/* Avoid including these declarations more than once. */
#ifndef IL_H
#define IL_H 1


/* il_def.h contains the definition of the IL tables.  It's in a separate
   file so that it can be included in back ends without dragging in all of the
   front end tables. */
#include "il_def.h"


/* Current memory region number for IL information. */
EXTERN a_memory_region_number
		curr_il_region_number;


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

extern a_param_type_ptr alloc_param_type(a_boolean at_file_scope);

extern a_class_type_supplement_ptr alloc_class_type_supplement(void);

extern a_type_ptr alloc_type(a_type_kind kind);

extern a_type_ptr fs_type(a_type_kind kind);

extern void add_to_types_list(a_type_ptr type_ptr,
                              a_boolean  at_file_scope,
                              a_boolean  in_old_style_param_decl_list);

extern a_type_ptr integer_type(an_integer_kind kind);

extern a_type_ptr signed_int_type(void);

extern a_type_ptr float_type(a_float_kind kind);

extern a_type_ptr string_type(a_targ_size_t num_chars);

extern a_type_ptr error_type(void);

extern a_type_ptr void_type(void);

extern a_type_ptr make_pointer_type(a_type_ptr type_pointed_to);

extern void switch_il_region(a_memory_region_number region_number);

extern void new_il_region(void);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

extern void add_to_constants_list(a_constant_ptr con_ptr);

extern void add_shareable_constants_to_constants_list(void);

extern void set_integer_constant(a_constant *cp,
                                 long       value);

extern char *alloc_text_of_string_literal(sizeof_t size);

extern a_variable_ptr alloc_variable(void);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_boolean      at_file_scope);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_field_ptr alloc_field(void);

extern a_routine_ptr alloc_routine(void);

extern void remove_from_routines_list(a_routine_ptr rout_ptr);

extern void add_to_routines_list(a_routine_ptr rout_ptr);

extern a_label_ptr alloc_label(void);

extern void add_to_labels_list(a_label_ptr label_ptr);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

extern void set_operator_node(an_expr_node_ptr      node,
                              an_expr_operator_kind kind,
	   	              a_type_ptr            type,
		              an_expr_node_ptr      operands);

extern an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	           a_type_ptr            type,
			   	           an_expr_node_ptr      operands);

extern an_expr_node_ptr error_node(void);

extern an_expr_node_ptr alloc_node_for_constant(a_constant *constant);

extern a_switch_clause_ptr alloc_switch_clause(void);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind);

extern a_scope_ptr alloc_scope(void);

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

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

/* Copy a constant entry. */
#define copy_constant(from, to) (*(to) = *(from))

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
extern void db_constant(a_constant *cp);

extern void db_field(a_field *fp);

extern void db_type(a_type *tp);

extern void db_variable(a_variable_ptr var_ptr);

extern void db_expression(an_expr_node_ptr node);

extern unsigned long show_il_space_used(void);
#endif /* DEBUG */

extern void il_init(void);

#endif /* ifndef IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
