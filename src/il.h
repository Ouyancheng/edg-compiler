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


/* Type of a scope nesting depth.  This is the depth within the scope_stack. */
/* Defined here (instead of the more obvious symbol_tbl.h) because it's
   used by add_to_types_list and we want to avoid including symbol_tbl.h
   from il.h (it would make life complicated for the standalone il_display
   and c_gen_be). */
typedef int	a_scope_depth;

/* il_def.h contains the definition of the IL tables.  It's in a separate
   file so that it can be included in back ends without dragging in all of the
   front end tables. */
#include "il_def.h"

#define NO_SCOPE_DEPTH (-1)
#define DEPTH_OF_FILE_SCOPE 0

/* Current memory region number for IL information. */
EXTERN a_memory_region_number
		curr_il_region_number;


#if ORPHAN_PROCESSING_NEEDED
/*
List of all IL entry kinds:
*/
/* If you change this, also change sizeof_il_entry in il_file.h. */
typedef enum /*an_il_entry_kind*/ {
  iek_none,		/* Skip zero value; it's used as a marker. */
  iek_source_file,	/* a_source_file */
  iek_constant,		/* a_constant */
  iek_param_type,	/* a_param_type */
  iek_routine_type_supplement,
			/* a_routine_type_supplement */
  iek_based_type_list_member,
			/* a_based_type_list_member */
  iek_type,		/* a_type */
  iek_variable,		/* a_variable */
#ifdef CIL
  iek_field,		/* a_field */
  iek_throw_specification,
			/* a_throw_specification */
  iek_throw_spec_type,	/* a_throw_spec_type */
#endif /* ifdef CIL */
  iek_routine,		/* a_routine */
  iek_label,		/* a_label */
  iek_expr_node,	/* an_expr_node */
#ifdef CIL
  iek_for_loop,         /* a_for_loop */
  iek_switch_clause,	/* a_switch_clause */
  iek_handler,          /* a_handler */
#endif /* ifdef CIL */
  iek_block,		/* a_block */
  iek_statement,	/* a_statement */
  iek_scope,		/* a_scope */
  iek_id_name,          /* String giving the name of an identifier. */
  iek_string_text,	/* Text of a string literal. */
  iek_other_text,	/* Text of a file name or similar information. */
#ifdef FIL
  iek_internal_complex_value,
			/* an_internal_complex_value */
  iek_bound_info_entry,	/* a_bound_info_entry */
  iek_do_loop,		/* a_do_loop */
  iek_label_list_entry,	/* a_label_list_entry */
  iek_io_specifier,	/* an_io_specifier */
  iek_io_list_item,	/* an_io_list_item */
  iek_namelist_group_member,
			/* a_namelist_group_member */
  iek_namelist_group,	/* a_namelist_group */
  iek_input_output_description,
			/* an_input_output_description */
  iek_entry_param,	/* an_entry_param */
  iek_entry_description,/* an_entry_description */
#endif /* ifdef FIL */
#ifdef CIL
  iek_dynamic_init,	/* a_dynamic_init */
  iek_access_adjustment,/* an_access_adjustment */
  iek_overriding_virtual_function,
			/* an_overriding_virtual_function */
  iek_derivation_step,  /* a_derivation_step */
  iek_base_class,	/* a_base_class */
  iek_class_list_entry, /* a_class_list_entry */
  iek_routine_list_entry,
                        /* a_routine_list_entry */
  iek_class_type_supplement,
			/* a_class_type_supplement */
  iek_constructor_init, /* a_constructor_init */
  iek_asm_entry,        /* an_asm_entry */
  iek_template_arg,     /* a_template_arg */
  iek_new_delete_supplement,
			/* a_new_delete_supplement */
#endif /* ifdef CIL */
  iek_orphaned_il_list, /* an_orphaned_il_list */
  iek_last		/* Marks the end of the list. */
} an_il_entry_kind;

/* Macro to test whether or not an entry kind is a string kind. */
#define is_string_entry_kind(entry_kind) \
  ((entry_kind) == iek_id_name || (entry_kind) == iek_string_text || \
   (entry_kind) == iek_other_text)

#if NEED_IL_DISPLAY || DEBUG
EXTERN char *il_entry_kind_names[(int)iek_last + 1]
#if VAR_INITIALIZERS
= {
/* iek_none */				"none",
/* iek_source_file */			"source-file",
/* iek_constant */			"constant",
/* iek_param_type */			"param-type",
/* iek_routine_type_supplement */	"routine-type-supplement",
/* iek_based_type_list_member */	"based-type-list-member",
/* iek_type */				"type",
/* iek_variable */			"variable",
#ifdef CIL
/* iek_field */				"field",
/* iek_throw_specification */		"throw-specification",
/* iek_throw_spec_type */		"throw-spec-type",
#endif /* ifdef CIL */
/* iek_routine */			"routine",
/* iek_label */				"label",
/* iek_expr_node */			"expr-node",
#ifdef CIL
/* iek_for_loop */			"for-loop",
/* iek_switch_clause */			"switch-clause",
/* iek_handler */			"handler",
#endif /* ifdef CIL */
/* iek_block */				"block",
/* iek_statement */			"statement",
/* iek_scope */				"scope",
/* iek_id_name */			"id-name",
/* iek_string_text */			"string-text",
/* iek_other_text */			"other-text",
#ifdef FIL
/* iek_internal_complex_value */	"internal-complex-value",
/* iek_bound_info_entry */		"bound-info-entry",
/* iek_do_loop */			"do-loop",
/* iek_label_list_entry */		"label-list-entry",
/* iek_io_specifier */			"io-specifier",
/* iek_io_list_item */			"io-list-item",
/* iek_namelist_group_member */		"namelist-group-member",
/* iek_namelist_group */		"namelist-group",
/* iek_input_output_description */	"input-output-description",
/* iek_entry_param */			"entry-param",
/* iek_entry_description */		"entry-description",
#endif /* ifdef FIL */
#ifdef CIL
/* iek_dynamic_init */			"dynamic-init",
/* iek_access_adjustment */		"access-adjustment",
/* iek_overriding_virtual_function */ 	"overriding-virtual-function",
/* iek_derivation_step */		"derivation-step",
/* iek_base_class */			"base-class",
/* iek_class_list_entry */		"class-list-entry",
/* iek_routine_list_entry */		"routine-list-entry",
/* iek_class_type_supplement */		"class-type-supplement",
/* iek_constructor_init */		"constructor-init",
/* iek_asm_entry */			"asm-entry",
/* iek_template_arg */			"template-arg",
/* iek_new_delete_supplement */		"new-delete-supplement",
#endif /* ifdef CIL */
/* iek_orphaned_il_list */		"orphaned-il-list",
/* iek_last */				"last"
} /* il_entry_kind_names */
#endif /* VAR_INITIALIZERS */
;
#endif /* NEED_IL_DISPLAY || DEBUG */

/*
It is necessary to maintain a list of IL entries that are allocated in
the file scope memory region but accessed from the function scope
region.  These lists are walked during IL file writing and reading
and when displaying the IL to ensure that all IL entries are
visited.  Note, the first_entry and last_entry point to the first
byte of the IL entry.  The address of the next entry in the linked list
precedes the IL entry.
*/
typedef struct an_orphaned_il_entry_list {
  char *first_entry;	/* Pointer to the first IL entry of a specific
			   kind in a linked list. */
  char *last_entry;	/* Pointer to the last IL entry of a specific
			   kind in a linked list. */
} an_orphaned_il_entry_list;

EXTERN an_orphaned_il_entry_list
		orphaned_file_scope_il_entries[(int)iek_last];
			/* Array of orphaned IL entry lists containing
			   individual orphaned file scope IL entries. */
#endif /* ORPHAN_PROCESSING_NEEDED */

#if DO_IL_LOWERING
/*
If IL lowering is to be done, IL entry prefixes have a flag that indicates
whether or not IL lowering has visited them yet.  This is the initial
value for that flag when it is cleared.
*/
EXTERN a_boolean
		initial_value_for_il_lowering_flag;
#endif /* DO_IL_LOWERING */

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

extern void set_routine_address_constant(a_routine_ptr routine,
                                         a_constant    *con);

extern void set_variable_address_constant(a_variable_ptr variable,
                                          a_constant    *con);

extern void set_arg_transfer_method_flag(a_param_type_ptr ptp);

extern a_param_type_ptr alloc_param_type(a_type_ptr type);

extern an_access_adjustment_ptr alloc_access_adjustment(
                                              an_access_adjustment_kind  kind);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_routine_list_entry_ptr alloc_list_entry_for_routine(void);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern an_overriding_virtual_function_ptr
                                       alloc_overriding_virtual_function(void);

extern a_template_arg_ptr alloc_template_arg(a_boolean is_type_arg);

extern void free_template_arg_list(a_template_arg_ptr  tap);

extern a_template_param_type_descr_ptr alloc_template_param_type_descr(void);

extern a_base_class_ptr alloc_base_class(void);

extern void set_type_kind(a_type_ptr  pte,
                          a_type_kind kind);

extern a_type_ptr alloc_type(a_type_kind kind);

extern void add_to_types_list(a_type_ptr     type_ptr,
                              a_scope_depth  scope_level);

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

extern a_type_ptr related_member_type(a_type_ptr member_type,
                                      a_type_ptr class_type);

extern a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                             a_type_ptr class_type);

extern void add_based_type_list_member(a_type_ptr        base_type,
                                       a_based_type_kind kind,
                                       a_type_ptr        based_type);

extern a_type_ptr make_pointer_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_qualified_type(a_type_ptr old_type,
                                      a_boolean  is_const,
                                      a_boolean  is_volatile);

extern a_type_ptr make_identically_qualified_type(a_type_ptr type,
                                                  a_type_ptr model_type);

extern a_type_ptr type_plus_qualifiers_from_second_type(a_type_ptr type,
                                                        a_type_ptr model_type);

extern a_type_ptr make_unqualified_type(a_type_ptr old_type);

extern void skip_common_type_qualifiers(a_type_ptr  *type1,
                                        a_type_ptr  *type2);

extern void set_routine_calling_method_flag(a_type_ptr routine_type);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern void copy_routine_type_with_param_types(a_type_ptr from_type,
                                               a_type_ptr to_type);

extern void switch_il_region(a_memory_region_number region_number);

extern void switch_to_file_scope_region(
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_back_to_original_region(
                              a_memory_region_number region_to_switch_back_to);

extern a_scope_ptr new_il_region(a_scope_kind   kind,
                                 a_scope_number scope_number,
                                 a_routine_ptr  assoc_routine);

extern a_constant_ptr alloc_constant(a_constant_repr_kind kind);

extern a_constant_ptr fs_constant(a_constant_repr_kind kind);

extern void copy_constant(a_constant *from,
                          a_constant *to);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

extern a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2);

extern a_boolean expr_tree_contains_template_param_constant(
                                             an_expr_node_ptr  node,
                                             a_constant_ptr    cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

extern void add_to_constants_list(a_constant_ptr con_ptr,
                                  a_boolean      at_file_scope);

extern void empty_shareable_constants_table(void);

extern void empty_func_shareable_constants_table(void);

extern void set_integer_constant(a_constant      *cp,
                                 long            value,
                                 an_integer_kind kind);

extern void set_unsigned_integer_constant(a_constant      *cp,
                                          unsigned long   value,
                                          an_integer_kind kind);

extern void make_zero_of_proper_type(a_type_ptr desired_type,
                                     a_constant *zero_constant);

extern char *alloc_text_of_string_literal(sizeof_t size);

extern void set_dynamic_init_kind(a_dynamic_init_ptr  dip,
                                  a_dynamic_init_kind kind);

extern void clear_dynamic_init(a_dynamic_init_ptr  dip,
                               a_dynamic_init_kind kind);

extern a_dynamic_init_ptr alloc_dynamic_init(a_dynamic_init_kind kind);

extern void add_to_dynamic_inits_list(a_dynamic_init_ptr dip);

extern a_variable_ptr alloc_variable(a_storage_class  storage_class);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_boolean      at_file_scope);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type);

extern a_field_ptr alloc_field(void);

extern a_throw_specification_ptr alloc_throw_specification(void);

extern a_throw_spec_type_ptr alloc_throw_spec_type(void);

extern a_routine_ptr alloc_routine(void);

extern void remove_from_routines_list(a_routine_ptr rout_ptr);

extern void add_to_routines_list(a_routine_ptr rout_ptr,
                                 a_boolean    at_file_scope);

extern an_asm_entry_ptr alloc_asm_entry(void);

extern void add_to_asm_entries_list(an_asm_entry_ptr asm_entry_ptr);

extern a_label_ptr alloc_label(void);

extern void add_to_labels_list(a_label_ptr label_ptr);

extern void set_expr_node_kind(an_expr_node_ptr  node,
                               an_expr_node_kind kind);

extern void clear_expr_node(an_expr_node_ptr  node,
                            an_expr_node_kind kind);

extern an_expr_node_ptr alloc_expr_node(an_expr_node_kind node_kind);

extern void set_expr_result_not_used(an_expr_node_ptr node);

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

extern an_expr_node_ptr copy_list_of_expr_trees(an_expr_node_ptr expr_list);

extern an_expr_node_ptr copy_expr_tree(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_default_arg_expr_list(a_param_type_ptr ptp);

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

extern a_dynamic_init_ptr alloc_dtor_dynamic_init(
                             a_dynamic_init_kind kind,
                             a_type_ptr          type,
                             a_boolean           evaluated,
                             a_boolean           in_return_by_cctor_expression,
                             a_source_position   *position);

extern an_expr_node_ptr alloc_temp_init_node(a_type_ptr temp_type,
                                             a_boolean  result_is_addr);

extern an_expr_node_ptr create_expr_temporary(
                               a_type_ptr        temp_type,
                               a_boolean         result_is_addr,
                               a_boolean         evaluated,
                               a_boolean         in_return_by_cctor_expression,
                               a_source_position *position);

extern an_expr_node_ptr func_call_expr(
                               an_expr_node_ptr  function_node,
                               a_type_ptr        function_type,
                               a_boolean         is_virtual,
                               a_boolean         evaluated,
                               a_boolean         in_return_by_cctor_expression,
                               a_source_position *err_pos);

extern void mark_routine_referenced(a_routine_ptr  routine);

extern a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                                 an_expr_node_ptr source);

extern a_statement_ptr make_array_assignment_statement(an_expr_node_ptr dest,
                                                      an_expr_node_ptr source);

extern a_statement_ptr make_call_assignment_statement(
                                                   a_routine_ptr     rout,
                                                   an_expr_node_ptr  dest,
                                                   an_expr_node_ptr  source,
                                                   a_source_position *err_pos);

extern a_switch_clause_ptr alloc_switch_clause(void);

extern a_handler_ptr alloc_handler(void);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_expr_statement(an_expr_node_ptr node);

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
			                a_source_file_ptr *new_file,
					a_boolean         is_system_include);

extern void record_end_of_source_file(a_source_file_ptr curr_file,
			              a_seq_number      seq_number);
extern a_source_file_ptr source_file_for_seq(a_seq_number   seq_number,
                                             a_line_number  *line_number,
                                             a_boolean      *at_end_of_source,
                                             unsigned long  *nesting_depth,
                                             a_boolean      physical_line);
extern void conv_seq_to_file_and_line(a_seq_number  seq_number,
			              char          **file_name,
				      char          **full_name,
				      a_line_number *line_number,
                                      a_boolean     *at_end_of_source);
#if !STANDALONE_UTILITY_PROGRAM

extern void conv_seq_to_physical_file_and_line(
                                          a_seq_number      seq_number,
                                          a_source_file_ptr *src_file,
                                          a_line_number     *physical_line,
                                          a_boolean         *at_end_of_source);

extern a_boolean seq_is_in_include_file(a_seq_number seq_number);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern void break_source_corresp(a_source_correspondence *sc);

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

/*
Macro that returns TRUE if a constant entry is the exact address of
a variable.
*/
#define con_is_exact_addr_of_variable(con)                            \
  ((con)->kind == (a_constant_repr_kind)ck_address &&                 \
   (con)->variant.address.kind == (an_address_base_kind)abk_variable &&\
   (con)->variant.address.offset == 0 && !(con)->implicit_cast)

/*
Macro that returns TRUE if a constant entry is the exact address of
a routine.
*/
#define con_is_exact_addr_of_routine(con)                            \
  ((con)->kind == (a_constant_repr_kind)ck_address &&                 \
   (con)->variant.address.kind == (an_address_base_kind)abk_routine &&\
   (con)->variant.address.offset == 0 && !(con)->implicit_cast)


#if DEBUG
extern void db_type_name(a_type_ptr  tp);

extern void db_name(a_source_correspondence *sc);

extern void db_access_control(an_access_specifier as);

extern void db_constant(a_constant *cp);

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

#if ORPHAN_PROCESSING_NEEDED
extern void add_orphaned_file_scope_il_entry(char             *entry_ptr,
                                             an_il_entry_kind entry_kind);

extern void add_orphaned_file_scope_il_list(a_type_ptr     types,
                                            a_variable_ptr variables);
#endif /* ORPHAN_PROCESSING_NEEDED */

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
