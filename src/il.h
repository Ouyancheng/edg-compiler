/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
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

#include "il_def.h"

/* Current memory region number for IL information. */
EXTERN a_memory_region_number
		curr_il_region_number;


#if ORPHAN_PROCESSING_NEEDED
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

extern void set_constant_kind(a_constant           *cp,
                              a_constant_repr_kind kind);

extern void clear_constant(a_constant           *cp,
                           a_constant_repr_kind kind);

extern void set_error_constant(a_constant *cp);

extern void set_routine_address_constant(a_routine_ptr routine,
                                         a_constant    *con,
                                         a_boolean     set_address_taken_flag);

extern void set_variable_address_constant(
                                        a_variable_ptr variable,
                                        a_constant     *con,
                                        a_boolean      set_address_taken_flag);

extern void set_constant_address_constant(a_constant_ptr constant,
                                          a_constant    *con);

extern void set_arg_transfer_method_flag(a_param_type_ptr   ptp,
                                         a_source_position  *err_pos);

extern a_param_type_ptr alloc_param_type(a_type_ptr type);

extern a_param_type_ptr make_param_type(a_type_ptr         tp,
                                        a_source_position  *decl_pos);

extern an_access_adjustment_ptr alloc_access_adjustment(an_il_entry_kind kind);

extern a_class_list_entry_ptr alloc_list_entry_for_class(void);

extern a_routine_list_entry_ptr alloc_list_entry_for_routine(void);

extern a_derivation_step_ptr alloc_derivation_step(void);

extern a_base_class_derivation_ptr alloc_base_class_derivation(void);

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

extern a_type_ptr signed_integer_type(an_integer_kind kind);

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

extern void set_routine_calling_method_flag(a_type_ptr         routine_type,
                                            a_source_position  *err_pos);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern void copy_routine_type_with_param_types(a_type_ptr from_type,
                                               a_type_ptr to_type);

extern a_boolean is_default_constructor(a_routine_ptr  ctor_rout);

extern a_boolean is_copy_constructor(a_routine_ptr  ctor_rout,
                                     a_type_ptr     class_of_which_a_member,
                                     a_boolean      *const_object_okay,
                                     a_boolean      *volatile_object_okay);

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

extern a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant);

extern a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2);

extern a_boolean expr_tree_contains_template_param_constant(
                                             an_expr_node_ptr  node,
                                             a_constant_ptr    cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

/* Make sure "a_scope_stack_entry" is known as a struct tag before its use
   below.  Otherwise, the declaration would be in the prototype scope.  The
   "struct" form is used instead of the typedef name to avoid having to
   include symbol_tbl.h. */
typedef struct a_scope_stack_entry a_scope_stack_entry_dummy_typedef;
extern a_scope_ptr ensure_il_scope_exists(struct a_scope_stack_entry *ssep);

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

extern a_boolean is_enum_constant(a_constant_ptr con);

extern a_boolean is_wide_string_constant(a_constant_ptr constant);

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

extern void remove_from_variables_list(a_variable_ptr var_ptr);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_boolean      at_file_scope);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type);

extern a_field_ptr alloc_field(void);

extern a_field_ptr next_initializable_field(a_field_ptr field);

extern an_exception_specification_ptr alloc_exception_specification(void);

extern an_exception_specification_type_ptr
                                  alloc_exception_specification_type(void);

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

extern void copy_statement(a_statement *from,
                           a_statement *to);

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

extern an_expr_node_ptr function_addr_expr(
                                         a_routine_ptr rout,
                                         a_boolean     set_address_taken_flag);

extern an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr this_param_value_expr(void);

extern an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern an_expr_node_ptr field_rvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING
extern void adjust_anonymous_union_field_selection(an_expr_node_ptr node,
                                                   a_field_ptr      au_field);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING */

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

extern an_accessible_base_class_ptr alloc_accessible_base_class(
                                                         a_base_class_ptr bcp);

extern a_handler_ptr alloc_handler(void);

extern void set_block_scope_handler(a_handler_ptr  handler);

extern void set_statement_kind(a_statement_ptr  sp,
                               a_statement_kind kind);

extern a_statement_ptr alloc_expr_statement(an_expr_node_ptr node);

extern a_statement_ptr alloc_statement(a_statement_kind stmt_kind);

extern a_constructor_init_ptr alloc_ctor_init(a_constructor_init_kind  kind);

#if RECORD_HIDDEN_NAMES_IN_IL
extern a_hidden_name_ptr alloc_hidden_name(void);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

#if RECORD_TEMPLATES_IN_IL
extern a_template_ptr alloc_template(void);

extern void add_to_templates_list(a_template_ptr  tp);
#endif /* RECORD_TEMPLATES_IN_IL */

#if RECORD_MACROS_IN_IL
extern a_macro_ptr alloc_macro(void);

extern void add_to_macros_list(a_macro_ptr  mp);
#endif /* RECORD_MACROS_IN_IL */

extern a_pragma_ptr alloc_pragma(a_pragma_kind  kind);

extern void add_to_pragma_list(a_pragma_ptr   pragma,
                               a_boolean      at_file_scope,
                               a_type_ptr     class_type);

extern a_pragma_ptr find_assoc_pragma(char          *il_entity,
                                      a_scope_ptr   curr_func_or_block_scope,
                                      a_type_ptr    class_type,
                                      a_pragma_ptr  prev_assoc_pragma);

extern a_scope_ptr alloc_scope(a_scope_kind   kind,
                               a_scope_number number,
                               a_routine_ptr  assoc_routine);

extern void record_start_of_source_file(a_source_file_ptr parent_file,
	  		                a_seq_number      seq_number,
				        a_line_number     line_number,
			                char              *file_name,
			                char              *full_name,
			                char              *name_as_written,
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

extern a_source_correspondence *source_corresp_for_il_entry(
                                                 char              *entity_ptr,
                                                 an_il_entry_kind  kind);

/*
Macro that returns TRUE if an IL entry has a name.  (Applies only to
those containing source correspondence information.)
*/
#define has_name(entry) ((entry)->source_corresp.name != NULL)

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

extern a_base_class_derivation_ptr preferred_virtual_derivation_of(
                                                      a_base_class_ptr  bcp);

extern a_base_class_derivation_ptr direct_virtual_derivation_of(
                                                     a_base_class_ptr  bcp);

/*
Macros that return information about base classes that may, for virtual base
classes, be contingent on the derivation selected.
*/
/* If bcp is a virtual base class, return a pointer to the virtual derivation
   entry associated with its preferred path; otherwise return a pointer to
   the derivation entry pointed to from bcp. */
#define preferred_derivation_of(bcp)                                 \
  ((bcp)->is_virtual ? preferred_virtual_derivation_of(bcp) :        \
                       (bcp)->derivation)

/* bcp is assumed to point to a base class for which direct is set to TRUE.
   If it is a virtual base class, return a pointer to the virtual derivation
   entry associated with its direct path; otherwise return a pointer to
   whatever derivation entry it points to. */
#define direct_derivation_of(bcp)                                    \
  ((bcp)->is_virtual ? direct_virtual_derivation_of(bcp) :           \
                       (bcp)->derivation)

/* Return TRUE if bcp is a direct nonvirtual base class or a virtual base
   class whose preferred derivation is direct. */
#define preferred_derivation_is_direct(bcp)                          \
  ((bcp)->direct &&                                                  \
   (!(bcp)->is_virtual || preferred_virtual_derivation_of(bcp)->direct))

/* Return TRUE if bcp is a direct nonvirtual base class or a virtual base
   class whose "first" derivation (i.e., first as found in a depth-first
   left-to-right search of the derivation graph) is direct. */
#define first_derivation_is_direct(bcp)                              \
  ((bcp)->derivation->direct)


extern a_derivation_step_ptr cast_virtual_derivation_path_of(
                                                         a_base_class_ptr bcp);

/* Return a derivation path to be used for a cast to the indicated base
   class.  For virtual base classes, this is a single step to the virtual
   base class. */
#define cast_derivation_path_of(bcp)                                  \
  ((bcp)->is_virtual ? cast_virtual_derivation_path_of(bcp) :         \
                       (bcp)->derivation->path)

/* Return TRUE if the given base class is virtual or if there is a virtual
   step in its derivation. */
#define any_virtual_steps_in_derivation(bcp)                          \
  ((bcp)->is_virtual || (bcp)->derivation->path->base_class->is_virtual)


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

#if GENERATE_SOURCE_SEQUENCE_LISTS

/*
Macro to extract the kind from a source sequence entry or secondary
declaration entry.
*/
#define ss_entry_kind(ssep) ((an_il_entry_kind)(ssep)->entity.kind)

/*
Macro to extract the pointer from a source sequence entry or secondary
declaration entry.  It is cast to the indicated pointer type.
*/
#define ss_entry_ptr(ssep, type) ((type)(ssep)->entity.ptr)

/*
Return TRUE if the indicated source sequence entry points to a source sequence
sublist header, i.e., it has kind iek_src_seq_sublist.
*/
#define is_sublist_parent(ssep) (ss_entry_kind(ssep) == iek_src_seq_sublist)

/*
ssep points to an iek_src_seq_sublist source sequence entry.  Such an entry
resides on the function-scope source sequence list but points to a header
for a sublist of file-scope source sequence entries.  Fetch and return a
pointer to the sublist header.
*/
#define assoc_sublist_of(ssep) ss_entry_ptr((ssep), a_src_seq_sublist_ptr)

#if DEBUG
extern void db_source_sequence_entry(a_source_sequence_entry_ptr  ssep);
extern void db_source_sequence_list(a_source_sequence_entry_ptr  ssep);
extern void db_ss_list_for_scope(a_scope_ptr  sp);
extern void dump_ss(a_scope_ptr  sp);
#endif /* DEBUG */

extern a_src_seq_secondary_decl_ptr alloc_src_seq_secondary_decl(void);

#if COMMENTS_IN_SOURCE_SEQUENCE_LISTS
extern a_comment_ptr alloc_comment(void);
#endif /* COMMENTS_IN_SOURCE_SEQUENCE_LISTS */

extern a_source_sequence_entry_ptr find_sublist_parent(
                                               a_src_seq_sublist_ptr sublist);

extern void add_to_source_sequence_list(a_source_sequence_entry  *new_ssep);

extern void f_update_source_sequence_list(char                    *entity_ptr,
                                          an_il_entry_kind        kind,
                                          a_source_sequence_entry *old_ssep);

#define update_source_sequence_list(entity_ptr, kind, old_ssep)          \
{ if (!source_sequence_entries_disallowed) {                             \
    f_update_source_sequence_list((entity_ptr), (kind), (old_ssep));     \
  }  /* if */                                                            \
}  /* update_source_sequence_list */

extern a_src_seq_sublist_ptr sublist_header_of(
                                            a_source_sequence_entry_ptr ssep);

extern a_source_sequence_entry_ptr add_empty_source_sequence_entry(void);

extern void add_end_of_construct_source_sequence_entry(
                                                char                   *ptr,
                                                a_byte_il_entry_kind   kind);

extern void remove_from_source_sequence_list(
                                      a_source_sequence_entry_ptr ssep_ptr,
                                      a_src_seq_sublist_ptr       *sublist);

extern void remove_sublist_header_and_parent(
                                      a_src_seq_sublist_ptr        sublist,
                                      a_source_sequence_entry_ptr  parent);

extern a_source_sequence_entry_ptr last_matching_source_sequence_entry(
                                                               char *entity);

extern void set_autonomous_tag_decl_flag(a_type_ptr  type,
                                         a_boolean   is_definition);

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if ORPHAN_PROCESSING_NEEDED
/*
Record a file-scope entry as a potential orphan.  The macro here ensures
that once the entry is placed on an orphan list the subroutine is no
longer called (well, except if it's the last entry on the list).
*/
#define add_orphaned_file_scope_il_entry(entry_ptr, entry_kind)       \
{ if (fs_orphan_pointer_of(entry_ptr) == NULL) {                      \
    f_add_orphaned_file_scope_il_entry((entry_ptr), (entry_kind));    \
  }  /* if */                                                         \
}  /* add_orphaned_file_scope_il_entry */
extern void f_add_orphaned_file_scope_il_entry(char             *entry_ptr,
                                               an_il_entry_kind entry_kind);
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
extern void add_scope_orphaned_il_lists(a_scope_ptr scope);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void il_reset(void);

extern void il_init(void);

#endif /* ifndef IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
