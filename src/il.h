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

/*
If IL lowering is to be done, IL entry prefixes have a flag that indicates
whether or not IL lowering has visited them yet.  This is the initial
value for that flag when it is cleared.
*/
/* Not conditional because it's also used by trans_copy.c. */
EXTERN a_boolean
		initial_value_for_il_lowering_flag;

/*
The default "routine name linkage" is the value to which the
routine_name_linkage field of a routine type supplement is initialized.
When a value other than the language default is required, the caller of
alloc_type will make the correction.
*/
EXTERN a_name_linkage_kind
		default_routine_name_linkage;

EXTERN a_type_ptr
                type_of_type_info;
                        /* Points to the definition of the type_info type
			   returned by typeid.  This type is identified
			   by a #pragma define_type_info that immediately
			   precedes the class definition of type_info. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_type_ptr
		type_of_guid;
			/* Points to the definition of the _GUID struct. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

EXTERN a_boolean
		okay_to_eliminate_unneeded_il_entries;
			/* When TRUE unneeded entities may be pruned from the
			   IL tree; otherwise, pruning is suppressed even if
			   entities are determined to be unneeded. Always
			   FALSE when MAINTAIN_NEEDED_FLAGS is FALSE.
			   Otherwise, controlled by command line option
			   --[no_]remove_unneeded_entities; also FALSE if
			   templates appear in the source program and
			   template instantiation is not under the control
			   of the front end (e.g., when the C++-generating
			   back end is used).  */

EXTERN a_stdc_pragma_value
		curr_fp_contract_state;
			/* Used in C99 mode to reflect the current setting
			   of the fp_contract state, which is set using the
			   STDC FP_CONTRACT pragma. */

EXTERN a_stdc_pragma_value
		curr_fenv_access_state;
			/* Used in C99 mode to reflect the current setting
			   of the fenv_access state, which is set using the
			   STDC FENV_ACCESS pragma. */

EXTERN a_stdc_pragma_value
		curr_cx_limited_range_state;
			/* Used in C99 mode to reflect the current setting
			   of the cx_limited_range state, which is set using
			   the STDC CX_LIMITED_RANGE pragma. */

#if ONE_INSTANTIATION_PER_OBJECT

EXTERN unsigned long
		needed_flag_bit_number;
			/* If non-zero, indicates that instead of the normal
			   "needed" flag in the source correspondence entry,
			   the so-numbered bit in the
			   per_instantiation_needed_flags bit vector is to
			   be tested and set by the "needed" flag
			   processing. */

/* Structure used to hold state between calls of
next_set_instantiation_needed_flag. */
typedef struct an_instantiation_needed_flags_scan_state
                                 *an_instantiation_needed_flags_scan_state_ptr;
typedef struct an_instantiation_needed_flags_scan_state {
  a_per_instantiation_needed_flags_entry_ptr
		curr_segment;
			/* Current segment of the bit vector.  NULL after
			   falling off the end. */
  unsigned long	first_bit_this_segment;
			/* Number of the first bit in the current segment. */
  int		byte_number;
			/* Current byte number in the current segment. */
  int		bit_number;
			/* Current bit number in the current byte.  -1 if we
			   haven't started the current byte yet. */
} an_instantiation_needed_flags_scan_state;

extern void clear_instantiation_needed_flags_scan_state(
                          an_instantiation_needed_flags_scan_state_ptr infssp,
                          a_source_correspondence                      *scp);
extern unsigned long next_set_instantiation_needed_flag(
                          an_instantiation_needed_flags_scan_state_ptr infssp);

extern unsigned long assign_instantiation_needed_bit_number(void);

#endif /* ONE_INSTANTIATION_PER_OBJECT */

/*
Macro that returns TRUE if a routine has been defined.  The value is
TRUE from the beginning of scanning of the function body (not just
after the closing brace), and is also TRUE for functions with
compiler-generated bodies.  The value remains TRUE if the body of
the function is discarded, as for example with trivial default
constructors.
*/
#define routine_has_been_defined(rout) \
  ((rout)->defined || (rout)->assoc_scope != NULL_region_number)


/* Macro to fetch the value of the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define needed_flag_is_set(scp) \
  (needed_flag_bit_number == 0 ? (scp)->needed : \
                                 instantiation_needed_flag_is_set(scp,0))
extern a_boolean instantiation_needed_flag_is_set(
                                           a_source_correspondence *scp,
                                           int                     bit_offset);
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define needed_flag_is_set(scp) ((scp)->needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

/* Macro to determine whether a routine is to be treated as a static inline
   function.  This includes "extern inline" functions that are lowered to
   static functions. */
#if LOWER_EXTERN_INLINE
/* When lowering "extern inline" all inline functions are treated as static. */
#define treat_as_static_inline(rout)					\
  ((rout)->is_inline)
#else /* !LOWER_EXTERN_INLINE */
/* When not lowering "extern inline" only those declared static are treated
   as static. */
#define treat_as_static_inline(rout)					\
  ((rout)->is_inline && ((rout)->storage_class == (a_storage_class)sc_static))
#endif /* LOWER_EXTERN_INLINE */

#if !STANDALONE_UTILITY_PROGRAM

/* Macro to set the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_needed_flag(scp) \
  (needed_flag_bit_number == 0 ? ((scp)->needed = TRUE) : \
                                 (set_instantiation_needed_flag(scp,0,1), 0))
extern void set_instantiation_needed_flag(a_source_correspondence *scp,
                                          int                     bit_offset,
                                          int                     new_value);
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_needed_flag(scp) ((scp)->needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Macro to fetch the value of the class definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define class_definition_needed_flag_is_set(tp) \
  (needed_flag_bit_number == 0 ? \
                   (tp)->variant.class_struct_union.definition_needed : \
                   instantiation_needed_flag_is_set(&(tp)->source_corresp,1))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define class_definition_needed_flag_is_set(tp) \
                  ((tp)->variant.class_struct_union.definition_needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !STANDALONE_UTILITY_PROGRAM
/* Macro to set the class definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_class_definition_needed_flag(tp) \
  (needed_flag_bit_number == 0 ? \
                ((tp)->variant.class_struct_union.definition_needed = TRUE) : \
                (set_instantiation_needed_flag(&(tp)->source_corresp,1,1), 0))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_class_definition_needed_flag(tp) \
                ((tp)->variant.class_struct_union.definition_needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !STANDALONE_UTILITY_PROGRAM */

/* Macro to fetch the value of the routine definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define routine_definition_needed_flag_is_set(rp) \
  (needed_flag_bit_number == 0 ? \
                   (rp)->definition_needed : \
                   instantiation_needed_flag_is_set(&(rp)->source_corresp,1))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define routine_definition_needed_flag_is_set(rp) \
                  ((rp)->definition_needed)
#endif /* ONE_INSTANTIATION_PER_OBJECT */

#if !STANDALONE_UTILITY_PROGRAM
/* Macro to set the routine definition_needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define set_routine_definition_needed_flag(rp) \
  (needed_flag_bit_number == 0 ? \
                ((rp)->definition_needed = TRUE) : \
                (set_instantiation_needed_flag(&(rp)->source_corresp,1,1), 0))
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define set_routine_definition_needed_flag(rp) \
                ((rp)->definition_needed = TRUE)
#endif /* ONE_INSTANTIATION_PER_OBJECT */
#endif /* !STANDALONE_UTILITY_PROGRAM */


/*
Macro that generates a unique unsigned long identifier from an IL pointer.
This is useful for generating names for unnamed symbols, for cross-reference
information, and for debug prints.  On most machines, the unique identifier
can simply be the pointer converted to unsigned long.  If that won't work,
a function can be substituted that does something else.
*/
#define unique_id_for_il_pointer(ptr) ((unsigned long)(ptr))


extern int compare_source_positions(a_source_position  *pos1,
				    a_source_position  *pos2);

/*
Dynamically-allocated and expandable buffer used for short-lived text.
"short-lived" means text that is needed during a bit of processing during
which no parsing or lexical advance is done (no get_token calls, no
macro expansions, etc.).
*/
EXTERN char	*temp_text_buffer;
			/* The buffer itself.  Not allocated on a per-file
			   basis. */
EXTERN sizeof_t	size_temp_text_buffer;
			/* The size of temp_text_buffer, as currently
			   allocated. */
EXTERN sizeof_t	pos_in_temp_text_buffer;
			/* The number of characters actually in
			   temp_text_buffer currently. */
/* See il.c for TEMP_TEXT_BUFFER_INCREMENTAL_ALLOCATION. */

extern void expand_temp_text_buffer(sizeof_t size_needed);

/*
Ensure that temp_text_buffer has at least size_needed bytes in it.  If not,
expand temp_text_buffer by reallocating it.
*/
#define ensure_temp_text_buffer_space(size_needed)                     \
{ if (size_temp_text_buffer < (size_needed)) {                         \
    expand_temp_text_buffer((sizeof_t)(size_needed));                  \
  }  /* if */                                                          \
}  /* ensure_temp_text_buffer_space */

extern void put_str_to_temp_text_buffer(char *str);

extern void put_ch_to_temp_text_buffer(char ch);

extern void set_error_constant(a_constant *cp);

extern a_constant_ptr alloc_error_constant(void);

extern void set_routine_address_constant(a_routine_ptr routine,
                                         a_constant    *con,
                                         a_boolean     set_address_taken_flag);

extern void set_variable_address_taken(a_variable_ptr variable);

extern void set_variable_address_constant(
                                        a_variable_ptr variable,
                                        a_constant     *con,
                                        a_boolean      set_address_taken_flag);

extern void set_constant_address_constant(a_constant_ptr constant,
                                          a_constant     *con);

#if GNU_EXTENSIONS_ALLOWED
extern void set_label_address_constant(a_label_ptr label,
                                       a_constant  *con);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern void set_ptr_to_member_function_constant(a_routine_ptr routine,
                                                a_constant    *con);

extern void set_ptr_to_data_member_constant(a_field_ptr field,
                                            a_constant  *con);

extern void set_arg_transfer_method_flag(a_param_type_ptr   ptp,
                                         a_source_position  *err_pos);

extern a_param_type_ptr make_param_type(a_type_ptr         tp,
                                        a_source_position  *decl_pos);

extern void add_placeholder_for_class_instantiation(a_type_ptr  type_ptr);

extern a_boolean may_be_added_to_types_list(a_type_ptr     type_ptr,
                                            a_scope_depth  decl_level);

extern void add_to_types_list(a_type_ptr     type_ptr,
                              a_scope_depth  scope_level);

extern void move_to_end_of_types_list(a_type_ptr     type_ptr,
                                      a_scope_depth  scope_level,
                                      a_boolean      delete_placeholder);

extern void do_based_type_fixup(void);

extern an_integer_kind char_int_kind_from_string_type(a_type_ptr str_type);

extern a_type_ptr integer_type(an_integer_kind kind);

extern a_type_ptr signed_integer_type(an_integer_kind kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr microsoft_sized_integer_type(an_integer_kind kind);

extern a_type_ptr microsoft_sized_signed_integer_type(an_integer_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_type_ptr wchar_t_type(void);

#if C99_IL_EXTENSIONS_SUPPORTED
extern a_boolean bool_type_used_in_primary_IL(void);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

extern a_type_ptr bool_type(void);

extern a_type_ptr float_type(a_float_kind kind);

#if C99_IL_EXTENSIONS_SUPPORTED
extern a_boolean complex_type_used_in_primary_IL(a_float_kind kind);

extern a_type_ptr complex_type(a_float_kind kind);

extern a_boolean imaginary_type_used_in_primary_IL(a_float_kind kind);

extern a_type_ptr imaginary_type(a_float_kind kind);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

extern a_type_ptr string_type(a_targ_size_t num_chars);

extern a_type_ptr wide_string_type(a_targ_size_t num_chars);

extern a_type_ptr error_type(void);

extern a_type_ptr unknown_type(void);

extern a_type_ptr void_type(void);

extern a_type_ptr check_ptr_to_member_function_type(a_type_ptr  member_type,
                                                    a_type_ptr  class_type);

extern a_type_ptr ptr_to_member_type(a_type_ptr  member_type,
                                     a_type_ptr  class_type);

extern a_type_ptr related_member_type(a_type_ptr member_type,
                                      a_type_ptr class_type);

extern a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                             a_type_ptr class_type);

extern a_type_ptr make_pointer_type(a_type_ptr type_pointed_to);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr make_based_pointer_type(a_type_ptr     type_pointed_to,
	                                  a_variable_ptr variable);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_type_ptr make_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_qualified_type(a_type_ptr            old_type,
                                      a_type_qualifier_set  qualifier);

/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are ignored.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
#define make_identically_qualified_type(type, model_type)             \
  (make_qualified_type(skip_typerefs(type), get_type_qualifiers(model_type)))

/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are preserved,
which means that the result type has all the qualifiers of both types.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
#define type_plus_qualifiers_from_second_type(type, model_type)       \
  (make_qualified_type(type, get_type_qualifiers(model_type)))

extern a_type_ptr make_unqualified_type(a_type_ptr old_type);

extern a_type_ptr rvalue_type(a_type_ptr type);

extern a_type_ptr return_type_of(a_type_ptr routine_type);

extern a_type_ptr il_return_type_of(a_type_ptr routine_type);

extern a_vla_dimension_ptr find_vla_dimension(a_type_ptr array_type);

extern a_type_ptr make_field_selection_type(a_field_ptr           field,
                                            a_type_qualifier_set  qualifiers);

extern void skip_common_type_qualifiers(a_type_ptr  *type1,
                                        a_type_ptr  *type2);

extern void set_routine_calling_method_flag(a_type_ptr         routine_type,
                                            a_source_position  *err_pos);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern a_type_ptr copy_routine_type_with_param_types(
                                               a_type_ptr  from_type,
                                               a_boolean   copy_default_args);

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern void copy_routine_type_default_args(a_type_ptr  from_type,
                                           a_type_ptr  to_type);

extern a_type_ptr routine_type_without_default_args(a_type_ptr orig_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern a_template_arg_ptr copy_template_arg_list(a_template_arg_ptr orig_list);

extern a_boolean is_default_constructor(a_routine_ptr  ctor_rout,
                                        a_boolean      is_declarative_context);

extern a_boolean is_copy_constructor_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             is_declarative_context);

extern a_boolean is_copy_constructor(
                                a_routine_ptr         ctor_rout,
                                a_type_ptr            class_of_which_a_member,
                                a_type_qualifier_set  *qualifiers,
                                a_boolean             is_declarative_context);

extern void switch_il_region(a_memory_region_number region_number);

extern void switch_to_file_scope_region(
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_to_scope_region(
                             a_scope_depth          scope_depth,
                             a_memory_region_number *region_to_switch_back_to);

extern void switch_back_to_original_region(
                              a_memory_region_number region_to_switch_back_to);

extern a_scope_ptr new_il_region(a_scope_kind   kind,
                                 a_scope_number scope_number,
                                 a_routine_ptr  assoc_routine);

extern void copy_constant(a_constant *from,
                          a_constant *to);

extern void combine_initializers(a_constant_ptr     first,
                                 a_dynamic_init_ptr first_dip,
                                 a_constant_ptr     second,
                                 a_dynamic_init_ptr second_dip);

extern void combine_initializer_constants(a_constant_ptr first,
                                          a_constant_ptr second);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

/*
Options for copy_expr_tree et al.
*/
typedef int an_expr_copy_options_set;
#define CE_NO_OPTIONS 0
#define CE_DOING_INLINING_OF_FUNCTION_CALL 0x1
			/* TRUE if this copy operation is copying an expression
			   for inlining and extra operations like remapping
			   should be done. */
#define CE_TRANSFER_DESTR_ENTITY_DESCR 0x2
			/* TRUE if, when copying a dynamic initialization
			   entry, the pointer to the destructible entity
			   description should be transferred to the copy. */
#define CE_INSIDE_CONDITIONAL_EXPRESSION 0x4
			/* TRUE if the expression is being copied into a
			   context that is under a conditional operator. */
#define CE_UNLINK_SOURCE_DESTRUCTIONS 0x8
			/* TRUE if destructions in the source expression
			   should be unlinked from their object lifetimes. */
#define CE_COPYING_EVALUATED_DEFAULT_ARG_EXPR 0x10
			/* TRUE if this copy operation is copying a default
			   argument expression, i.e., making a real use from
			   the scanned expression.  This is set only for
			   evaluated expressions, not unevaluated ones. */
#define CE_COPIED_CONSTANTS_MAY_BE_SHARED 0x20
			/* TRUE if when constants are copied they may be
			   shared.  FALSE means such constants must be
			   unshared. */

a_constant_ptr copy_constant_full(a_constant_ptr           old_constant,
                                  a_constant_ptr           new_constant,
                                  an_expr_copy_options_set options);

extern a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant);

extern a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2);

extern a_boolean expr_tree_contains_template_param_constant(
                                             an_expr_node_ptr  node,
                                             a_constant_ptr    cp);

extern a_boolean constant_references_non_external_entity(
                                                      a_constant_ptr constant);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

/* Make sure "a_scope_stack_entry" is known as a struct tag before its use
   below.  Otherwise, the declaration would be in the prototype scope.  The
   "struct" form is used instead of the typedef name to avoid having to
   include symbol_tbl.h. */
typedef struct a_scope_stack_entry a_scope_stack_entry_dummy_typedef;
/* Likewise for a_template_param. */
typedef struct a_template_param a_template_param_dummy_typedef;
extern a_scope_ptr ensure_il_scope_exists(struct a_scope_stack_entry *ssep);

extern void add_to_namespaces_list(a_namespace_ptr  nsp);

extern void add_to_using_decls_list(a_using_decl_ptr  udp,
				    a_scope_depth     depth);

extern void add_to_scopes_list(a_scope_ptr                scope_ptr,
                               struct a_scope_stack_entry *ssep);

extern void add_to_constants_list(a_constant_ptr con_ptr,
                                  a_boolean      at_file_scope);

extern void empty_shareable_constants_table(void);

extern void empty_func_shareable_constants_table(void);

extern void set_integer_constant(a_constant		*cp,
                                 a_host_large_integer	value,
                                 an_integer_kind	kind);

extern void set_unsigned_integer_constant(a_constant		*cp,
                                          a_host_large_unsigned	value,
                                          an_integer_kind	kind);

extern a_boolean is_enum_constant(a_constant_ptr con);

extern a_boolean is_wide_string_constant(a_constant_ptr constant);

extern void make_zero_of_proper_type(a_type_ptr desired_type,
                                     a_constant *zero_constant);

extern void make_uuidof_constant(a_type_ptr     uuidof_type,
                                 a_constant_ptr uuidof_con);

extern void add_to_dynamic_inits_list(a_dynamic_init_ptr dip);

extern a_local_static_variable_init_ptr make_local_static_variable_init(
                                                  a_variable_ptr     var,
                                                  a_scope_ptr        var_scope,
                                                  an_init_kind       init_kind,
                                                  a_constant_ptr     con,
                                                  a_dynamic_init_ptr dip);

extern a_local_static_variable_init_ptr find_local_static_variable_init(
                                                      a_variable_ptr  var,
                                                      a_scope_ptr     scope);

extern a_vla_dimension_ptr make_vla_dimension(
                                       a_type_ptr        array_type,
                                       an_expr_node_ptr  expr_node,
                                       a_boolean         in_prototype_scope,
                                       a_source_position *position);

extern void get_variable_initializer(a_variable_ptr     variable,
                                     a_scope_ptr        var_scope,
                                     an_init_kind       *init_kind,
                                     an_initializer_ptr *initializer);

extern void remove_from_variables_list(a_variable_ptr var_ptr,
                                       a_scope_depth  scope_depth);

extern void add_to_variables_list(a_variable_ptr var_ptr,
                                  a_scope_depth  scope_depth);

extern void add_to_parameters_list(a_variable_ptr param_ptr);

extern a_variable_ptr make_variable(a_type_ptr      type_ptr,
                                    a_storage_class storage_class,
                                    a_scope_depth   scope_depth);

extern a_variable_ptr make_handler_parameter(a_type_ptr  type_ptr);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type);

extern a_field_ptr next_initializable_field(a_field_ptr field);

extern void remove_from_routines_list(a_routine_ptr rout_ptr,
                                      a_scope_depth scope_depth);

extern void add_to_routines_list(a_routine_ptr  rout_ptr,
                                 a_scope_depth  scope_level);

extern void add_to_asm_entries_list(an_asm_entry_ptr asm_entry_ptr);

extern void add_to_labels_list(a_label_ptr label_ptr);

extern void copy_statement(a_statement *from,
                           a_statement *to);

extern void change_statement_into_block(a_statement_ptr statement,
                                        a_statement_ptr *orig_statement);

extern void set_expr_result_not_used(an_expr_node_ptr node);

extern void set_node_operator(an_expr_node_ptr      node,
                              an_expr_operator_kind kind,
	   	              a_type_ptr            type,
		              an_expr_node_ptr      operands);

extern an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	           a_type_ptr            type,
			   	           an_expr_node_ptr      operands);

extern an_expr_node_ptr make_comma_node(an_expr_node_ptr expr1,
                                        an_expr_node_ptr expr2);

extern an_expr_node_ptr error_node(void);

extern an_expr_node_ptr alloc_node_for_constant(a_constant *constant);

extern an_expr_node_ptr alloc_node_for_allocated_constant(
                                                         a_constant *constant);

extern an_expr_node_ptr node_for_integer_constant(long            value,
                                                  an_integer_kind kind);

extern an_expr_node_ptr node_for_host_large_integer(
					     a_host_large_integer	value,
                                             an_integer_kind		kind);

extern a_boolean is_bad_type_for_template_arg_operand(a_type_ptr type);

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

extern a_constant_ptr copy_template_param_con_with_substitution(
                                 a_constant_ptr           con,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_type_ptr               template_param_type,
                                 a_source_position        *source_pos,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error);

extern a_boolean is_operator_returning_bool(an_expr_operator_kind op);

extern an_expr_node_ptr add_cast(an_expr_node_ptr node,
                                 a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                              a_type_ptr       new_type);

extern an_expr_node_ptr copy_node(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_list_of_expr_trees(
                                            an_expr_node_ptr         expr_list,
                                            an_expr_copy_options_set options);

extern an_expr_node_ptr copy_expr_tree(an_expr_node_ptr         expr,
                                       an_expr_copy_options_set options);

extern an_expr_node_ptr copy_default_arg_expr(
			       a_routine_ptr	rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated);

extern an_expr_node_ptr duplicate_default_arg_expr(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_default_arg_expr_list(
			       a_routine_ptr	rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated);

extern an_expr_node_ptr var_lvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_rvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr function_addr_expr(
                                         a_routine_ptr rout,
                                         a_boolean     set_address_taken_flag);

extern an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr add_object_lifetime_to_expr(
                                             an_expr_node_ptr       expr,
                                             an_object_lifetime_ptr lifetime);

extern an_expr_node_ptr this_param_value_expr(void);

extern an_expr_node_ptr field_lvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

extern an_expr_node_ptr field_rvalue_selection_expr(an_expr_node_ptr node,
                                                    a_field_ptr      field);

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING
extern void adjust_anonymous_union_field_selection(an_expr_node_ptr node,
                                                   a_field_ptr      au_field);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS || DO_IL_LOWERING */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
typedef struct a_symbol a_symbol_il_h_dummy_typedef;
extern void adjust_nonstandard_anonymous_object_field_references(
                                                  an_expr_node_ptr node,
                                                  struct a_symbol  *field_sym,
                                                  a_boolean        std_also);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

extern an_expr_node_ptr fe_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern an_expr_node_ptr fe_field_rvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern an_expr_node_ptr base_class_selection_expr(an_expr_node_ptr node,
                                                  a_base_class_ptr bcp);

extern void mark_routine_referenced_full(a_routine_ptr routine,
                                         a_boolean     instantiate);

extern void mark_routine_referenced(a_routine_ptr routine);

extern void set_routine_defined(a_routine_ptr rout);

extern a_statement_ptr make_assignment_statement(an_expr_node_ptr dest,
                                                 an_expr_node_ptr source);

extern a_statement_ptr make_array_assignment_statement(an_expr_node_ptr dest,
                                                      an_expr_node_ptr source);

extern void set_block_scope_handler(a_handler_ptr  handler);

extern a_statement_ptr alloc_expr_statement(an_expr_node_ptr node);

extern void add_to_templates_list(a_template_ptr  tp,
                                  a_scope_depth   scope_depth);

#if RECORD_MACROS_IN_IL
extern void add_to_macros_list(a_macro_ptr  mp);
#endif /* RECORD_MACROS_IN_IL */

extern void add_to_pragma_list(a_pragma_ptr             pragma,
                               a_scope_depth            scope_depth,
                               a_source_correspondence  *scp);

extern a_pragma_ptr find_assoc_pragma(char          *il_entity,
                                      a_scope_ptr   curr_func_or_block_scope,
                                      a_type_ptr    class_type,
                                      a_pragma_ptr  prev_assoc_pragma);

EXTERN an_object_lifetime_ptr
		curr_object_lifetime;
			/* The top of the currently active object lifetime
			   stack. */

extern void add_to_end_of_destructions_list(a_dynamic_init_ptr      dip,
                                            an_object_lifetime_ptr  olp);

extern void record_end_of_lifetime_destruction(
                                        a_dynamic_init_ptr  dip,
                                        a_boolean           static_lifetime,
                                        a_boolean           block_lifetime);

extern void move_destruction_to_curr_object_lifetime(a_dynamic_init_ptr  dip);

extern void free_object_lifetime(an_object_lifetime_ptr  olp);

extern an_object_lifetime_ptr init_expr_lifetime_of(a_dynamic_init_ptr dip);

extern void bind_object_lifetime(an_object_lifetime_ptr  olp,
                                 an_il_entry_kind        entity_kind,
                                 char                    *entity_ptr);

extern void unbind_object_lifetime(an_object_lifetime_ptr  olp);

extern void push_object_lifetime(an_il_entry_kind         entity_kind,
                                 char                     *entity_ptr,
                                 an_object_lifetime_kind  kind);

extern a_boolean is_useless_object_lifetime(an_object_lifetime_ptr  olp);

extern void remove_from_destruction_list(a_dynamic_init_ptr  dip);

extern void mark_object_lifetime_as_useless(an_object_lifetime_ptr  olp);

extern a_boolean pop_object_lifetime(void);

extern an_object_lifetime_ptr innermost_block_object_lifetime(
                                             an_object_lifetime_ptr  olp);

extern void record_start_of_source_file(
				 a_source_file_ptr parent_file,
			         a_seq_number      seq_number,
				 a_line_number     line_number,
			         char	           *file_name,
			         char	           *full_name,
                                 char              *name_as_written,
			         a_source_file_ptr *new_file,
                                 a_boolean	   is_include_file,
				 a_boolean	   is_system_include,
                                 a_boolean         is_preinclude,
				 a_boolean	   from_system_include_dir);

extern void record_end_of_source_file(a_source_file_ptr curr_file,
			              a_seq_number      seq_number);
extern a_source_file_ptr primary_source_file_for_seq(a_seq_number seq_number);
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

extern a_source_file_ptr eff_primary_source_file(void);

#if !STANDALONE_UTILITY_PROGRAM

extern void conv_seq_to_physical_file_and_line(
                                          a_seq_number      seq_number,
                                          a_source_file_ptr *src_file,
                                          a_line_number     *physical_line,
                                          a_boolean         *at_end_of_source);

extern a_boolean seq_is_in_include_file(a_seq_number seq_number);

extern void break_source_corresp(a_source_correspondence *sc);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern unsigned long write_file_name(char      *name,
                                     FILE      *f_output,
                                     a_boolean process_escapes);

extern a_source_correspondence *source_corresp_for_il_entry(
                                                 char              *entity_ptr,
                                                 an_il_entry_kind  kind);

/*
Macro that returns TRUE if an IL entry has a name.  (Applies only to
those containing source correspondence information.)
*/
#define has_name(entry) ((entry)->source_corresp.name != NULL)

/*
Macro that returns TRUE if a class/struct/union or enum tag type is unnamed
or is marked as being originally unnamed.
*/
#define is_unnamed_or_originally_unnamed_tag(tag_type)                   \
  ((tag_type)->source_corresp.name == NULL ||                            \
   (is_immediate_class_type(tag_type) &&                                 \
    (tag_type)->variant.class_struct_union.originally_unnamed))

/*
Return the original unmangled name of an entity, given a pointer to
its source correspondence entry.
*/
#if NEED_NAME_MANGLING
#define unmangled_name_of(scp) \
  ((scp)->name_has_been_mangled ? (scp)->unmangled_name : (scp)->name)
#else /* !NEED_NAME_MANGLING */
#define unmangled_name_of(scp) ((scp)->name)
#endif /* NEED_NAME_MANGLING */

/*
Macro that returns TRUE if an IL entry has a name before any name mangling
that was done.  This is useful when testing entities like classes and
namespaces that may have been given a generated name during name
mangling.  (Applies only to those entries containing source correspondence
information.)
*/
#define has_name_before_mangling(entry) \
  (unmangled_name_of(&(entry)->source_corresp) != NULL)

/*
Clear the parent information in the indicated entity to remove the entity
from any class or namespace of which it might be a member.
*/
#define clear_parent(entity) \
{ (entity)->source_corresp.is_class_member = FALSE; \
  (entity)->source_corresp.parent.namespace_ptr = NULL; \
}  /* clear_parent */


/*
Return TRUE if a constant is an error constant.
*/
#define is_error_constant(cp) ((cp)->kind == (a_constant_repr_kind)ck_error)

/* Macro that returns TRUE if a variable's storage class has static storage
   duration.  See 3.1.2.4.  Note that storage classes have been 
   standardized during declaration processing. */
#define has_static_storage_duration(storage_class)                    \
  ((storage_class) == (a_storage_class)sc_static ||                   \
   (storage_class) == (a_storage_class)sc_extern ||                   \
   (storage_class) == (a_storage_class)sc_unspecified)

/*
Macros used to determine the kind of a template argument.
*/
#define is_type_templ_arg(arg) \
  (arg->kind == (a_templ_arg_kind)tak_type)
#define is_nontype_templ_arg(arg) \
  (arg->kind == (a_templ_arg_kind)tak_nontype)
#define is_template_templ_arg(arg) \
  (arg->kind == (a_templ_arg_kind)tak_template)

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


/* Return TRUE if two template nesting depths should be considered
   equivalent.  Depths are equivalent if they are the same, or if either
   of the depths is NO_NESTING_DEPTH. */
#define equiv_nesting_depths(depth1, depth2)				\
  ((depth1) == (depth2) ||						\
   (depth1) == NO_NESTING_DEPTH || (depth2 == NO_NESTING_DEPTH))


#if DEBUG
extern void db_template_arg_list(a_template_arg_ptr tap);

extern void db_template_name(a_template_ptr  tp);

extern void db_type_name(a_type_ptr  tp);

extern void db_name(a_source_correspondence *sc);

extern char *db_name_str(a_source_correspondence *sc,
                         an_il_entry_kind        kind);

extern void db_entity_info(char             *entry,
                           an_il_entry_kind kind);

extern void db_access_control(an_access_specifier as);

extern void db_constant(a_constant *cp);

extern void db_type(a_type *tp);

extern void db_function_param_list(a_type_ptr  tp);

extern void db_abbreviated_type(a_type *tp);

/* Abbreviated version of db_abbreviated type. */
/*lint -esym(755,db_abbr_type)*/
#define db_abbr_type(tp)                                              \
  (db_abbreviated_type(tp), (void)fputc('\n', f_debug))

extern void db_variable(a_variable_ptr var_ptr);

extern void db_expression(an_expr_node_ptr node);

extern void db_expr_summary(an_expr_node_ptr  node);

extern void db_dynamic_initializer(a_dynamic_init_ptr  dip,
                                   int                 level);

extern void db_initializer(a_variable_ptr  var_ptr,
                           int             level);

extern void db_statement_kind(a_statement_kind  kind);

extern void db_statement(a_statement_ptr  sp);

extern void db_statement_list(a_statement_ptr  sp,
                              int              indent,
                              char             *str,
                              int              how_deep);

extern void db_scope(a_scope_ptr sp);

extern void db_scope_type_list(a_scope_ptr scope,
                               int         indent,
                               a_boolean   do_subscopes);

extern void db_type_lists(a_scope_ptr scope,
                          int         indent);

extern void db_destruction(a_dynamic_init_ptr  dip);

extern void db_object_lifetime_name(an_object_lifetime_ptr  olp);

extern void db_object_lifetime(an_object_lifetime_ptr  olp);

extern void db_object_lifetime_stack(void);

extern void db_pending_destructions(a_dynamic_init_ptr      dip,
                                    an_object_lifetime_ptr  stop_at);

extern void db_object_lifetime_tree(an_object_lifetime_ptr olp);

extern unsigned long db_show_based_type_fixups_used(unsigned long grand_total);

extern unsigned long show_il_space_used(void);
#endif /* DEBUG */

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
#if !STANDALONE_UTILITY_PROGRAM
extern void add_scope_orphaned_il_lists(a_scope_ptr scope);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void clear_function_body(a_scope_ptr sp);

void detach_from_object_lifetime_tree(an_object_lifetime_ptr olp);

#if MAINTAIN_NEEDED_FLAGS
extern void eliminate_bodies_of_unneeded_functions(void);

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
extern void eliminate_unneeded_scope_orphaned_list_entries(void);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void eliminate_default_arg_object_lifetimes(a_type_ptr  rout_type);

extern void eliminate_routine_default_arg_object_lifetimes(a_routine_ptr rout);

extern void eliminate_unneeded_il_entries(a_scope_ptr scope);
#endif /* MAINTAIN_NEEDED_FLAGS */

extern a_namespace_ptr f_skip_namespace_aliases(a_namespace_ptr nsp);

extern a_boolean is_member_of_unnamed_namespace(a_source_correspondence *scp);

/*
Given a namespace pointer, return a pointer to the actual namespace,
skipping any namespace aliases that might be present.
*/
#define skip_namespace_aliases(nsp)					\
  ((nsp)->is_namespace_alias ? f_skip_namespace_aliases(nsp) : (nsp))

/*
Macro that returns the trans_unit_corresp for an IL entry that has a source
correspondence.
*/
#define trans_unit_corresp_of(ptr)					\
  (((a_source_correspondence*)(ptr))->trans_unit_corresp)

/*
Macro that returns the canonical IL entry pointer for an IL entry that
has a source correspondence.  If the entry has no correspondence pointer,
a NULL pointer is returned.
*/
#define canonical_il_entry_of(ptr)					\
  (char*)(trans_unit_corresp_of(ptr) != NULL ? trans_unit_corresp_of(ptr) \
                                             : NULL)

/*
Compare to translation unit correspondence pointers.  They match if they
are equal and non-NULL.
*/
#define same_trans_unit_corresps(ptr1, ptr2)				\
  ((ptr1) == (ptr2) && (ptr1) != NULL)

/*
Return TRUE if two IL entries (that have source correspondence entries)
refer to the same IL entity.  If the pointers differ, check the
translation unit correspondence pointers.
*/
#define same_entities(ptr1, ptr2)					\
  ((ptr1) == (ptr2) ||							\
   same_trans_unit_corresps(trans_unit_corresp_of(ptr1),		\
                            trans_unit_corresp_of(ptr1)))

/*
Return TRUE if two base classes refer to the same IL entry.  If the
pointers differ, check the translation unit correspondence pointers.
*/
#define same_base_classes(ptr1, ptr2)					\
  ((ptr1) == (ptr2) ||							\
   same_trans_unit_corresps((ptr1)->trans_unit_corresp,			\
                            (ptr2)->trans_unit_corresp))


extern a_type_ptr init_predeclared_class(a_type_kind  kind,
                                         char         *name);

extern void il_reset(void);

extern void il_one_time_init(void);

extern void il_trans_unit_init(void);

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
