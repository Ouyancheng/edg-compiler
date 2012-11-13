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

/* A dummy name for placeholders. */
EXTERN char     *routine_move_placeholder_name
#if VAR_INITIALIZERS
                         = "<routine move placeholder>"
#endif /* VAR_INITIALIZERS */
                                                       ;


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

/* The various type_info types. */
enum a_type_info_kind_tag {
  tik_user,             /* The user-visible std::type_info type.  This
			   type must be first. */
  /* tik_implementation is the type used by the runtime to implement
     type_info, which must start with the type_info fields, but may
     have additional information following that. */
#if IA64_ABI
  /* Additional derived classes of type_info defined by the IA-64 ABI: */
  tik_implementation = tik_user,
			/* An alias for the user type.	In some
			   places (like the exception_type_spec), the
			   front end creates pointers to the
			   "implementation" type_info type, and
			   providing this alias makes it unnecessary to
			   conditionalize that code. */
  tik_fundamental,	/* Void, integral, floating, decltype(nullptr)
			   types. */
  tik_enum,		/* Enumeration types. */
  tik_array,		/* Array types. */
  tik_function,		/* Function types. */
  tik_class,		/* Class types without inheritance. */
  tik_si_class,		/* Class types with single, public,
			   non-virtual inheritance. */
  tik_vmi_class,	/* Other class types. */
  tik_pbase,		/* Base class for pointers and
			   pointers-to-members. */
  tik_pointer,		/* Pointer types. */
  tik_ptr_to_member,	/* Pointer-to-member types. */
#else /* !IA64_ABI */
  tik_implementation,	/* Implementation type. */
#endif /* !IA64_ABI */
  tik_last
};
typedef enum a_type_info_kind_tag a_type_info_kind;

/* Names of type_info types. */
EXTERN char	*type_info_names[(int)tik_last+1]
#if VAR_INITIALIZERS
= { 
  "type_info",			/* tik_user */
#if IA64_ABI 
  "__fundamental_type_info",	/* tik_fundamental */
  "__enum_type_info",		/* tik_enum */
  "__array_type_info",		/* tik_array */
  "__function_type_info",	/* tik_function */
  "__class_type_info",		/* tik_class */
  "__si_class_type_info",	/* tik_si_class */
  "__vmi_class_type_info",	/* tik_vmi_class */
  "__pbase_type_info",		/* tik_pbase */
  "__pointer_type_info",	/* tik_pointer */
  "__pointer_to_member_type_info", /* tik_ptr_to_member */
#else /* !IA64_ABI */
  NULL,				/* tik_implementation */
#endif /* !IA64_ABI */
  NULL				/* tik_last */
}
#endif /* VAR_INITIALIZERS */
;

/* Pointer types for types defined in il_to_str.h. */
typedef struct an_il_to_str_output_control_block
                                        *an_il_to_str_output_control_block_ptr;

EXTERN a_type_ptr
		type_of_type_info;
			/* Points to the definition of the type_info type
			   returned by typeid.	This type is identified
			   by a #pragma define_type_info that immediately
			   precedes the class definition of type_info. */
EXTERN a_type_ptr
		types_of_type_info[(int)tik_last + 1];
			/* The user-visible type_info types.  The element
			   with index tik_user has the same value as
			   type_of_type_info. */

#if MICROSOFT_EXTENSIONS_ALLOWED
EXTERN a_type_ptr
		type_of_guid;
			/* Points to the definition of the _GUID struct. */

EXTERN a_boolean
		in_microsoft_implementation_key_mapping_region;
			/* Indicates whether the source being parsed is inside
			   a region of code delimited by "#pragma
			   start_map_region" and "#pragma stop_map_region". */
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

#if FIXED_POINT_ALLOWED

EXTERN a_stdc_pragma_value
		curr_fx_full_precision_state;
			/* Used to reflect the current setting of the
			   fx_full_precision state, which is set using the
			   STDC FX_FULL_PRECISION pragma. */

EXTERN a_stdc_pragma_value
		curr_fx_fract_overflow_state;
			/* Used to reflect the current setting of the
			   fx_fract_overflow state, which is set using the
			   STDC FX_FRACT_OVERFLOW pragma. */

EXTERN a_stdc_pragma_value
		curr_fx_accum_overflow_state;
			/* Used to reflect the current setting of the
			   fx_accum_overflow state, which is set using the
			   STDC FX_ACCUM_OVERFLOW pragma. */

#endif /* FIXED_POINT_ALLOWED */

#if UPC_EXTENSIONS_ALLOWED

EXTERN a_upc_access_method
		curr_upc_access_method;
			/* Used in UPC mode to reflect the last setting of
			   the UPC access mode through the UPC pragma. */

#endif /* UPC_EXTENSIONS_ALLOWED */

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

#if PARENS_IN_IL
extern an_expr_node_ptr f_skip_parens(an_expr_node_ptr expr);
#endif /* PARENS_IN_IL */

/*
Strip parentheses off an expression and return the underlying expression.
*/
#if PARENS_IN_IL
#define skip_parens(expr) f_skip_parens(expr)
#else /* !PARENS_IN_IL */
#define skip_parens(expr) (expr)
#endif /* PARENS_IN_IL */

/*
Macro to access the parent_scope field of an IL entry.
*/
#define parent_scope_of(ptr)                                                \
  ((ptr)->source_corresp.parent_scope)

/*
Macro that returns whether a parent scope was recorded for the given IL entry.
(This includes cases where the parent scope is recorded indirectly via an
entry of type a_local_scope_ref.)
*/
#define has_parent_scope(ptr)                                               \
  ((ptr)->source_corresp.parent_scope != NULL ||                            \
   (ptr)->source_corresp.parent_via_local_scope_ref)

extern a_scope_ptr f_get_parent_scope_of(a_source_correspondence_ptr  scp);

/*
Macro to get the parent scope of an IL entry.  This differs from the macro
"parent_scope_of" (see above) in that it works even for entities in file scope
memory whose parent scope is in function scope memory.
*/
#define get_parent_scope_of(ptr)                                            \
  ((ptr)->source_corresp.parent_via_local_scope_ref ?                       \
                            f_get_parent_scope_of(&(ptr)->source_corresp)   \
                          : parent_scope_of(ptr))

/*
Macro that returns TRUE if an IL entry represents a scoped enumerator.
*/
#define scp_is_enum_member(scp)                                             \
  ((scp)->parent_scope != NULL &&                                           \
   (scp)->parent_scope->kind == (a_scope_kind)sck_enum)

/*
Macros that return TRUE if an IL entry represents a namespace member.
*/
#define scp_is_namespace_member(scp)                                        \
  ((scp)->parent_scope != NULL &&                                           \
   (scp)->parent_scope->kind == (a_scope_kind)sck_namespace)

#define is_namespace_member(ptr)                                            \
  (scp_is_namespace_member(&(ptr)->source_corresp))

/*
Macros that return TRUE if an IL entry represents a class or namespace member.
*/
#define scp_is_class_or_namespace_member(scp)                               \
  ((scp)->is_class_member || scp_is_namespace_member((scp)))

#define is_class_or_namespace_member(ptr)                                   \
  (scp_is_class_or_namespace_member(&(ptr)->source_corresp))

/*
Macro that returns the parent type of a scoped enumerator. 
*/
#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
#define scp_parent_scoped_enum_type(scp)                                    \
  ((void)scp_is_enum_member(scp),                                           \
   (scp)->parent_scope->variant.assoc_type)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define scp_parent_scoped_enum_type(scp)                                    \
  (check_assertion(scp_is_enum_member(scp)),                                \
   (scp)->parent_scope->variant.assoc_type)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_scoped_enum_type(scp)                                    \
  ((scp)->parent_scope->variant.assoc_type)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */

/*
Macros that return the parent namespace of a namespace member. 
*/
#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
#define scp_parent_namespace(scp)                                           \
  ((void)scp_is_namespace_member(scp),                                      \
   (scp)->parent_scope->variant.assoc_namespace)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define scp_parent_namespace(scp)                                           \
  (check_assertion(scp_is_namespace_member(scp)),                           \
   (scp)->parent_scope->variant.assoc_namespace)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_namespace(scp)                                           \
  ((scp)->parent_scope->variant.assoc_namespace)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */

#define parent_namespace_of(ptr)                                            \
  (scp_parent_namespace(&(ptr)->source_corresp))

/*
Macros that return the parent namespace for a namespace member, and NULL for
entities that are neither namespace members nor class members.  (This macro
should not be used for class members.)
*/
#if EXPENSIVE_CHECKING && !defined(_lint)
#define scp_parent_namespace_or_null(scp)                                   \
  (check_assertion(!(scp)->is_class_member),                                \
   (scp_is_namespace_member((scp)) ? scp_parent_namespace((scp))            \
                                   : (a_namespace_ptr)NULL))
#else /* !(EXPENSIVE_CHECKING && !defined(_lint)) */
#define scp_parent_namespace_or_null(scp)                                   \
  (scp_is_namespace_member((scp)) ? scp_parent_namespace((scp))             \
                                  : (a_namespace_ptr)NULL)
#endif /* EXPENSIVE_CHECKING && !defined(_lint) */

#define parent_namespace_or_null(ptr)                                       \
  (scp_parent_namespace_or_null(&(ptr)->source_corresp))

/*
Macros that return the parent class of a class member. 
*/
#if defined(_lint)
/* When linting, duplicate the macro argument to catch side-effects that would
   be duplicated in the EXPENSIVE_CHECKING version, but don't call
   check_assertion since that results in spurious lint errors when the macro
   is used in a macro that itself duplicates its argument. */
#define scp_parent_class(scp)                                               \
  ((void)(scp)->is_class_member,                                            \
   (scp)->parent_scope->variant.assoc_type)
#else /* !defined(_lint) */
#if EXPENSIVE_CHECKING
#define scp_parent_class(scp)                                               \
  (check_assertion((scp)->is_class_member &&                                \
                   (scp)->parent_scope != NULL &&                           \
                   (scp)->parent_scope->kind ==                             \
                                    (a_scope_kind)sck_class_struct_union),  \
   (scp)->parent_scope->variant.assoc_type)
#else /* !EXPENSIVE_CHECKING */
#define scp_parent_class(scp)                                               \
  ((scp)->parent_scope->variant.assoc_type)
#endif /* EXPENSIVE_CHECKING */
#endif /* defined(_lint) */

#define parent_class_of(ptr)                                                \
  (scp_parent_class(&(ptr)->source_corresp))

/*
Macro that returns the parent class if the given pointer points to a class
member, and NULL otherwise.
*/
#define parent_class_or_null(ptr)                                           \
  ((ptr)->source_corresp.is_class_member ? parent_class_of(ptr)             \
                                         : (a_type_ptr)NULL)

/*
Return TRUE if cp is a ck_template_param/tpck_unknown_function constant.
*/
#define is_unknown_function_constant(cp) \
  ((cp)->kind == (a_constant_repr_kind)ck_template_param && \
   (cp)->variant.template_param.kind == \
        (a_template_param_constant_kind)tpck_unknown_function)

/*
Return TRUE if tp is a template type parameter pack.
*/
#define type_is_pack(tp)						\
  ((tp)->kind == (a_type_kind)tk_template_param &&			\
   (tp)->variant.template_param.is_pack)


/*
Return TRUE if cp is a template nontype parameter pack.
*/
#define constant_is_pack(cp)						\
  ((cp)->kind == (a_constant_repr_kind)ck_template_param &&		\
   (cp)->variant.template_param.is_pack)


extern a_routine_ptr lambda_body_for_closure(a_type_ptr	type);

extern a_lambda_ptr get_current_lambda(void);

extern a_namespace_ptr namespace_enclosing_class(a_type_ptr  tp);

/*
Macro to test a routine entry's special_kind field.
*/
#define special_kind_is(rp, sfk)                                            \
  ((rp)->special_kind == (a_special_function_kind)(sfk))

/*
Macro that returns TRUE if a routine has been defined.  The value is
TRUE from the beginning of scanning of the function body (not just
after the closing brace), and is also TRUE for functions with
compiler-generated bodies.  The value remains TRUE if the body of
the function is discarded, as for example with trivial default
constructors.  The test of routine_fixup is used so that a
function defined in a friend declaration in a template will be
considered defined even if the definition has not been fixed-up yet.
*/
#define routine_has_been_defined(rout) \
  ((rout)->defined || (rout)->assoc_scope != NULL_region_number ||	\
   (rout)->routine_fixup != NULL)


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

/* Test a routine to see whether it is inline.  When it is a template
   instance, this may require looking at the template because the
   is_inline flag is not recorded until the function is fully instantiated. */
#if STANDALONE_UTILITY_PROGRAM
#define rout_is_inline(rout)						\
  ((rout)->is_inline)
#else /* !STANDALONE_UTILITY_PROGRAM */
extern a_boolean intf_rout_is_inline_template_function(a_routine_ptr rout);
#define rout_is_inline(rout)						\
  ((rout)->is_inline ||							\
   ((rout)->is_template_function &&					\
    intf_rout_is_inline_template_function(rout)))
#endif /* STANDALONE_UTILITY_PROGRAM */

/* Helper macro for macros treat_as_static_inline and treat_as_extern_inline
   (see below). */
#if LOWER_EXTERN_INLINE && !IA64_ABI
#if MICROSOFT_EXTENSIONS_ALLOWED
#define and_not_dllexport(rout)  && !((rout)->decl_modifiers & DM_DLLEXPORT)
#else /* MICROSOFT_EXTENSIONS_ALLOWED */
#define and_not_dllexport(rout)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

/* Macro to determine whether a routine is to be treated as a static inline
   function.  This includes "extern inline" functions that are lowered to
   static functions. */
/* In IA-64 mode, the lowered functions are still external and they
   go out in COMDAT sections. */
/* Note that these macros work only in C++ mode. */
#if LOWER_EXTERN_INLINE && !IA64_ABI
/* When lowering "extern inline" all inline functions are treated as static.
   Those that really are static stay static (actually, they may get
   externalized if there are exported templates, or because of
   one-instantiation-per-object mode, then lowered to static again), and
   extern inline functions get lowered to static.  An exception
   is made for function definitions marked with dllexport: They must be
   spilled with extern linkage. */
#define treat_as_static_inline(rout)                                    \
  (rout_is_inline(rout)                                                 \
   and_not_dllexport(rout))
#else /* !(LOWER_EXTERN_INLINE && !IA64_ABI) */
/* When not lowering "extern inline" only those declared static are treated
   as static. */
#define treat_as_static_inline(rout)					\
  (rout_is_inline(rout) &&						\
   ((rout)->storage_class == (a_storage_class)sc_static))
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

/*
Return TRUE if the routine should be treated as an extern inline function.
*/
#if LOWER_EXTERN_INLINE && !IA64_ABI
#define treat_as_extern_inline(rout)                                    \
  ((rout)->is_inline &&                                                 \
   (rout)->storage_class == (a_storage_class)sc_unspecified             \
   and_not_dllexport(rout))
#else /* !LOWER_EXTERN_INLINE && !IA64_ABI */
#define treat_as_extern_inline(rout)                                    \
  ((rout)->is_inline &&                                                 \
   (rout)->storage_class == (a_storage_class)sc_unspecified)
#endif /* LOWER_EXTERN_INLINE && !IA64_ABI */

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

/* Macro to reset the needed flag. */
#if ONE_INSTANTIATION_PER_OBJECT
#define reset_needed_flag(scp) \
    ((scp)->needed = FALSE, \
     (scp)->per_instantiation_needed_flags = NULL)
#else /* !ONE_INSTANTIATION_PER_OBJECT */
#define reset_needed_flag(scp) ((scp)->needed = FALSE)
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


extern void set_inline_flag(a_routine_ptr  rp,
                            a_boolean      flag);

/*
A collection of source positions passed around during declaration processing.
*/
typedef struct a_decl_pos_block *a_decl_pos_block_ptr;
typedef struct a_decl_pos_block {
  a_source_position
		decl_pos;
			/* Source position of the identifier. */
  a_source_position
		storage_class_pos;
			/* Source position of storage-class, if any. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		identifier_range;
			/* Start and end positions of coalesced identifier. */
  a_source_range
		specifiers_range;
			/* Start and end positions of decl-specifiers. */
  a_source_range
		declarator_range;
			/* Start and end positions of declarator. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_range
		var_init_range;
			/* Start and end positions of initializer.  The end
			   position is only recorded when
			   EXTRA_SOURCE_POSITIONS_IN_IL is TRUE. */
} a_decl_pos_block;

extern void clear_decl_pos_block(a_decl_pos_block_ptr  decl_pos_block);

#if EXTRA_SOURCE_POSITIONS_IN_IL

extern a_decl_position_supplement_ptr make_decl_pos_supplement(
                                        a_boolean             at_file_scope,
                                        a_decl_pos_block_ptr  decl_pos_block);

extern void update_decl_pos_info(a_source_correspondence  *scp,
                                 a_decl_pos_block_ptr     decl_pos_block);

#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

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

extern void put_str_to_temp_text_buffer_octl(
                               char                                  *str,
                               an_il_to_str_output_control_block_ptr octl);

extern
void put_str_into_text_buffer(char                                  *str,
                              an_il_to_str_output_control_block_ptr octl);

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
extern a_boolean is_gnu_builtin_function(a_routine_ptr  rp);

extern void set_label_address_constant(a_label_ptr label,
                                       a_constant  *con);

/*
Macro to determine whether the argument constant is a label address.
*/
#define constant_is_address_of_label(cp)                                     \
  ((cp)->kind == (a_constant_repr_kind)ck_address &&                         \
   (cp)->variant.address.kind == (an_address_base_kind)abk_label)

/*
Macro to determine whether the argument variable was mapped on a specific
register using the GNU "asm(...)" extension.
*/
#define var_is_gnu_named_register(var)                                     \
  (!var_has_named_register_storage_class((var)) &&                         \
   !(var)->asm_name_is_valid)
#endif /* GNU_EXTENSIONS_ALLOWED */

extern void set_ptr_to_member_function_constant(a_routine_ptr routine,
                                                a_constant    *con);

extern void set_ptr_to_data_member_constant(a_field_ptr field,
                                            a_constant  *con);

extern void set_arg_transfer_method_flag(a_param_type_ptr   ptp,
                                         a_source_position  *err_pos);

extern a_param_type_ptr make_param_type(a_type_ptr         tp,
                                        a_source_position  *decl_pos);
extern a_type_ptr make_routine_type(a_type_ptr        return_type,
                                    a_type_ptr        param1_type,
                                    a_type_ptr        param2_type,
                                    a_type_ptr        param3_type,
                                    a_type_ptr        param4_type);

extern a_routine_ptr routine_from_function_expr(an_expr_node_ptr expr);

extern a_routine_ptr routine_and_node_from_function_expr(
                                                       an_expr_node_ptr expr,
                                                       an_expr_node_ptr *node);

extern a_type_ptr add_param_type(a_type_ptr  rout_type,
                                 a_type_ptr  param_type);

#if GNU_VECTOR_TYPES_ALLOWED
extern a_type_ptr make_vector_type(a_type_ptr     element_type,
                                   a_targ_size_t  n_elements);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#if NAMED_REGISTERS_ALLOWED
extern void record_named_register_storage_class(
                                             a_variable_ptr       var,
                                             a_named_register_id  register_id,
                                             a_boolean            is_redecl,
                                             a_source_position    *pos);

/*
Macro to determine if the given variable was declared with a named-register
storage class (an Embedded C extension).  The macro can be used in
configurations that don't allow named-register storage classes (in that case
the macro expands to FALSE).
*/
#define var_has_named_register_storage_class(var)                            \
  ((var)->has_named_register_storage_class)
#else /* !NAMED_REGISTERS_ALLOWED */
#define var_has_named_register_storage_class(var)  /*lint --e(506)*/FALSE
#endif /* NAMED_REGISTERS_ALLOWED */

extern a_boolean may_be_added_to_types_list(a_type_ptr     type_ptr,
                                            a_scope_depth  decl_level);

extern void set_parent_scope_for_type(a_type_ptr     type_ptr,
                                      a_scope_depth  scope_level);

extern void add_lambda_closure_to_types_list(a_type_ptr     type_ptr,
                                             a_scope_depth  scope_level);

extern void add_to_types_list(a_type_ptr     type_ptr,
                              a_scope_depth  scope_level);

extern void move_to_end_of_types_list(a_type_ptr     type_ptr,
                                      a_scope_depth  scope_level);

extern void do_based_type_fixup(void);

extern void record_fundamental_types_copied_from_secondary_IL(void);

extern a_type_ptr integer_type(an_integer_kind kind);

extern a_type_ptr signed_integer_type(an_integer_kind kind);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr microsoft_sized_integer_type(an_integer_kind kind);

extern a_type_ptr microsoft_sized_signed_integer_type(an_integer_kind kind);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_type_ptr other_signedness_integer_type(an_integer_kind ikind);

extern a_type_ptr wchar_t_type(void);

extern a_type_ptr char16_t_type(void);

extern a_type_ptr char32_t_type(void);

extern a_type_ptr eff_wchar_t_type(void);

extern a_type_ptr eff_char16_t_type(void);

extern a_type_ptr eff_char32_t_type(void);

extern a_type_ptr bool_type(void);

#if FIXED_POINT_ALLOWED
extern a_boolean fixed_point_type_used_in_primary_IL(
                                 a_fixed_point_type_descr descr);
extern a_type_ptr fixed_point_type(a_fixed_point_type_descr descr);
extern a_fixed_point_type_descr make_fixed_point_type_descr(
                                 a_fixed_point_precision  precision,
                                 a_boolean                is_unsigned,
                                 a_boolean                is_fract,
                                 a_boolean                saturating);
#endif /* FIXED_POINT_ALLOWED */

extern a_type_ptr float_type(a_float_kind kind);

#if C99_IL_EXTENSIONS_SUPPORTED
extern a_boolean complex_type_used_in_primary_IL(a_float_kind kind);

extern a_type_ptr complex_type(a_float_kind kind);

extern a_boolean imaginary_type_used_in_primary_IL(a_float_kind kind);

extern a_type_ptr imaginary_type(a_float_kind kind);

extern void set_complex_constant(a_float_kind float_kind,
                                 char         *real,
                                 char         *imag,
                                 a_constant   *con);
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

extern a_type_ptr string_literal_type(a_character_kind  kind,
                                      a_targ_size_t     num_chars);

extern a_type_ptr string_type(a_targ_size_t  num_chars);

#define wide_string_type(num_chars)                                        \
  string_literal_type((a_character_kind)chk_wchar_t, (num_chars))

extern a_type_ptr error_type(void);

extern a_type_ptr unknown_type(void);

extern a_type_ptr void_type(void);

extern a_type_ptr managed_nullptr_type(void);

extern a_type_ptr standard_nullptr_type(void);

extern void update_ptr_to_member_type(a_type_ptr  ptr_mem_type,
                                      a_type_ptr  member_type);

extern a_type_ptr make_partial_ptr_to_member_type(a_type_ptr  class_type);

extern a_type_ptr ptr_to_member_type_full(a_type_ptr              member_type,
                                          a_type_ptr              class_type,
                                          a_pointer_modifier_set  modifiers);

#define ptr_to_member_type(tp, cp)                                           \
  (ptr_to_member_type_full((tp), (cp), PM_NONE))

extern a_type_ptr related_member_type(a_type_ptr member_type,
                                      a_type_ptr class_type);

extern a_type_ptr related_ptr_to_member_type(a_type_ptr member_type,
                                             a_type_ptr class_type);

extern a_type_ptr make_pointer_type_full(
                                      a_type_ptr              pointed_to_type,
                                      a_pointer_modifier_set  modifiers);

#define make_pointer_type(tp)                                                \
  (make_pointer_type_full((tp), PM_NONE))

extern a_type_ptr make_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_rvalue_reference_type(a_type_ptr  pointed_to_type);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_type_ptr make_handle_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_handle_to_system_string(void);

extern a_type_ptr make_tracking_reference_type(a_type_ptr type_pointed_to);

extern a_type_ptr make_interior_ptr_type(a_type_ptr pointed_to_type);

extern a_type_ptr make_pin_ptr_type(a_type_ptr pointed_to_type);

extern a_routine_ptr get_idisposable_dispose_routine(void);

extern a_routine_ptr get_object_finalize_routine(void);

extern void f_set_clrcall_convention_if_needed(a_type_ptr  rtp);

#define set_clrcall_convention_if_needed(rtp)               \
  if (cppcli_enabled) f_set_clrcall_convention_if_needed(rtp)

#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
extern a_boolean f_is_member_of_namespace_cli(a_source_correspondence  *scp);

#define is_member_of_namespace_cli(ptr)                                      \
  (f_is_member_of_namespace_cli((a_source_correspondence*)ptr))
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define set_clrcall_convention_if_needed(rtp)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
extern
a_type_ptr make_pointer_type_of_same_kind(a_type_ptr base_type,
                                          a_type_ptr model_pointer_type);

a_type_ptr make_reference_type_of_same_kind(a_type_ptr base_type,
                                            a_type_ptr model_ref_type);

extern a_type_ptr make_reference_to_reference(
                                          a_type_ptr            base_ref_type,
                                          a_boolean             rvalue_ref,
                                          a_boolean             tracking_ref,
                                          a_type_qualifier_set  qualifiers,
                                          a_source_position     *qual_pos,
                                          a_boolean             *is_error);

extern a_type_ptr f_make_qualified_type(a_type_ptr            old_type,
                                        a_type_qualifier_set  qualifier,
                                        a_upc_block_size      upc_block_size);

#define make_qualified_type(old_type, qualifier)    \
  f_make_qualified_type(old_type, qualifier, UPC_BLOCK_SIZE_NONE)

/*
Make a version of type that has the same qualifiers as model_type, and return
a pointer to it.  The original qualifiers on type, if any, are ignored.
Note that type and model_type need not be the same (or even similar) types
under the qualifiers.
*/
#define make_identically_qualified_type(type, model_type)             \
  (make_qualified_type(f_skip_typerefs(type), get_type_qualifiers(model_type)))


extern a_type_ptr type_plus_qualifiers_from_second_type(a_type_ptr type,
                                                        a_type_ptr model_type);

extern a_type_ptr make_unqualified_type(a_type_ptr old_type);

extern a_type_ptr rvalue_type(a_type_ptr type);

extern a_type_ptr return_type_of(a_type_ptr routine_type);

extern a_type_ptr il_return_type_of(a_type_ptr routine_type);

extern a_vla_dimension_ptr find_vla_dimension_in_current_function(
                                                       a_type_ptr  array_type);

extern a_vla_dimension_ptr find_vla_dimension(a_type_ptr array_type);

extern void make_local_expr_node_ref(
                                 an_expr_node_ptr            expr,
                                 a_local_expr_node_ref_kind  kind,
                                 char                        *referrer,
                                 a_scope_ptr                 func_scope);

extern an_expr_node_ptr find_local_expr_node(char  *referrer,
                                             a_local_expr_node_ref_kind  kind);

extern void make_local_scope_ref(a_scope_ptr            scope,
                                 char                   *referrer,
                                 an_il_entry_kind       referrer_kind,
                                 a_scope_ptr            func_scope);

extern a_scope_ptr find_local_scope(char  *referrer);

#if PROTOTYPE_INSTANTIATIONS_IN_IL
extern an_expr_node_ptr generic_sizeof_arg_expr(a_constant_ptr  con);
#else /* !PROTOTYPE_INSTANTIATIONS_IN_IL */
#define generic_sizeof_arg_expr(con)                                        \
  ((con)->variant.template_param.variant.templ_sizeof.expr)
#endif /* PROTOTYPE_INSTANTIATIONS_IN_IL */

extern a_type_ptr make_field_selection_type(a_field_ptr           field,
                                            a_type_qualifier_set  qualifiers);

extern a_type_ptr make_pm_selection_type(a_type_ptr operand_1_type,
                                         a_type_ptr operand_2_type);

extern void skip_common_type_qualifiers(a_type_ptr  *type1,
                                        a_type_ptr  *type2);

/*
Macro that is TRUE if the given operator kind is a simple (i.e., not
compound) assignment.
*/
#define is_simple_assignment(op)                                            \
  ((op) == (an_expr_operator_kind)eok_assign)

/*
Macro that is TRUE for a dynamic initialization that initializes a
variable-length array (VLA).
*/
#define is_dynamic_init_for_vla(dip) \
  ((dip)->variable != NULL && (dip)->variable->is_vla)

extern a_boolean node_is_pointer_with_restrict_semantics(
                                                        an_expr_node_ptr node);

extern a_boolean is_rvalueable_node(an_expr_node_ptr node);

extern a_boolean node_includes_lvalue_to_rvalue_conv(an_expr_node_ptr node);

extern a_boolean dynamic_init_has_side_effects(
                                        a_dynamic_init_ptr dip,
                                        a_boolean          *suppress_warning);

extern a_boolean expr_list_has_side_effects(
                                           an_expr_node_ptr expr_list,
                                           a_boolean        *suppress_warning);

extern a_boolean node_has_side_effects(an_expr_node_ptr node,
                                       a_boolean        *suppress_warning);

extern a_boolean is_invariant_expr(an_expr_node_ptr expr,
                                   a_boolean        vars_can_change,
                                   a_boolean        treat_as_rvalue);

extern a_boolean expr_might_throw(an_expr_node_ptr expr);

extern a_boolean has_statement_expression(an_expr_node_ptr expr);

extern a_boolean expr_is_dep_static_member_of_current_instantiation(
                                                        an_expr_node_ptr expr);

extern a_boolean expr_is_instantiation_dependent(an_expr_node_ptr expr);

extern a_boolean constant_is_instantiation_dependent(a_constant_ptr con);

extern a_boolean expr_contains_error(an_expr_node_ptr expr);

extern a_boolean constant_contains_error(a_constant_ptr con);

extern void set_routine_calling_method_flag(a_type_ptr         routine_type,
                                            a_source_position  *err_pos);

extern void copy_type(a_type_ptr from,
                      a_type_ptr to);

extern a_type_ptr copy_routine_type_with_param_types(
                                               a_type_ptr  from_type,
                                               a_boolean   copy_default_args);

extern void copy_routine_type_default_args(a_type_ptr  from_type,
                                           a_type_ptr  to_type);

extern a_type_ptr routine_type_without_default_args(a_type_ptr orig_type);

extern a_type_ptr routine_type_without_this_class(a_type_ptr	orig_type);

extern
void ensure_underlying_function_type_is_modifiable(a_type_ptr  *p_type,
                                                   a_type_ptr  *func_type);

extern
a_type_ptr routine_type_without_param_type_qualifiers(a_type_ptr  orig_type);


#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
extern a_boolean class_type_can_be_named_in_namespace_scope(a_type_ptr  type);
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void begin_template_arg_list_traversal_simple(
                                             a_template_arg_ptr templ_arg_list,
                                             a_template_arg_ptr *tap);

extern void advance_to_next_template_arg_simple(a_template_arg_ptr *tap);

extern a_template_arg_ptr copy_template_arg_list(a_template_arg_ptr orig_list);

extern a_boolean is_default_constructor(a_routine_ptr  ctor_rout,
                                        a_boolean      is_declarative_context);

extern a_boolean is_copy_constructor_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             include_move_ctors,
                                 a_boolean             is_declarative_context);

extern a_boolean is_copy_constructor(
                                 a_routine_ptr         ctor_rout,
                                 a_type_ptr            class_of_which_a_member,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             include_move_ctors,
                                 a_boolean             is_declarative_context);

extern a_boolean routine_is_move_constructor(a_routine_ptr  rp);

extern a_boolean is_copy_assignment_operator_type(
                                 a_type_ptr            routine_type,
                                 a_type_ptr            class_type,
                                 a_boolean             move_assign_okay,
                                 a_boolean             *is_ref_arg,
                                 a_type_qualifier_set  *qualifiers,
                                 a_boolean             *is_base_class_match);

extern a_boolean routine_is_move_assignment_operator(a_routine_ptr  rp);

/* 
Macro that is TRUE if move constructors and move assign operators can be
defined with "= default;".
*/
#define move_operations_can_be_defaulted()                                  \
  (generate_move_operations || (gpp_mode && gnu_version >= 40500))

#if DO_IL_LOWERING
extern a_boolean special_member_is_user_provided(a_routine_ptr  rp);
#endif /* DO_IL_LOWERING */

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

void explode_string_initializer(a_constant_ptr con);

extern void combine_initializers(a_constant_ptr     first,
                                 a_dynamic_init_ptr first_dip,
                                 a_constant_ptr     second,
                                 a_dynamic_init_ptr second_dip);

extern void combine_initializer_constants(a_constant_ptr first,
                                          a_constant_ptr second);

extern a_constant_ptr alloc_unshared_constant(a_constant *cp);

extern a_constant_ptr alloc_unshared_constant_full(a_constant *cp,
                                                   a_boolean  source_in_il,
                                                   a_boolean  suppress_copy);

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
			   evaluated expressions (specifically, potentially
			   evaluated ones), not unevaluated ones. */
#define CE_COPIED_CONSTANTS_MAY_BE_SHARED 0x20
			/* TRUE if when constants are copied they may be
			   shared.  FALSE means such constants must be
			   unshared. */
#define CE_REPLACE_STRINGS_BY_VARIABLES 0x40
			/* TRUE if string literals with sequence_number != 0
			   should be replaced by variables as they are
			   copied.  More precisely, address constants that
			   point to ck_string constants with sequence_number
			   != 0 are rewritten as the addresses of the
			   generated variables. */
#define CE_COPY_NOT_EVALUATED 0x80
			/* TRUE if the copy is in an unevaluated context. */
#define CE_DEST_CONSTANT_IS_NOT_ALLOC_IN_IL 0x100
			/* TRUE if the destination address provided to
			   copy_constant_full is not an IL address (e.g.,
			   it's the address of a stack variable). */
#define CE_COPYING_EXPRESSION_FOR_CONSTANT 0x200
			/* TRUE if the constant being copied is a backing
			   expression attached to a constant. */
#define CE_COPYING_FROM_ONE_FUNC_TO_ANOTHER 0x400
			/* TRUE if the copy is from one function scope
			   memory region into another. */
#define CE_SRC_CONSTANT_IS_NOT_ALLOC_IN_IL 0x800
			/* TRUE if the source address provided to
			   copy_constant_full might not be an IL address (e.g.,
			   it's the address of a stack variable). */

a_constant_ptr copy_constant_full(a_constant_ptr           old_constant,
                                  a_constant_ptr           new_constant,
                                  an_expr_copy_options_set options);

/*
Flags used to specify options to compare_constants.
*/
typedef int a_compare_constants_options_set;

#define CC_NO_OPTIONS		0x0
#define CC_STRICTLY_IDENTICAL	0x1
			/* When this flag is not set the qualifiers are
			   stripped from the constant type before they
			   are compared; otherwise, a "const int 5" and
			   an "int 5" are treated as nonidentical. */
#define CC_EXACT_TEMPLATE_PARAM_TYPE_REQUIRED 0x2
			/* TRUE if, when comparing template parameters of
			   tpck_param kind, the constant pointers must
			   match, not just the coordinates. */
#define CC_EXACT_DECLTYPE_EXPR_MATCH_REQUIRED 0x4
			/* TRUE if, when checking that the types of expressions
			   match, one should check for an exact match of the
			   expressions under any dependent decltypes. */
#define CC_COORDINATE_MISMATCH_OKAY 0x8
			/* TRUE if, when comparing template parameters of
			   tpck_param kind, the coordinates are not
			   required to match. */

extern a_boolean compare_constants(a_constant_ptr                   cp1,
                                   a_constant_ptr                   cp2,
                                   a_compare_constants_options_set  options);

extern a_constant_ptr copy_unshared_constant(a_constant_ptr old_constant);

extern a_boolean eq_constants(a_constant *cp1,
                              a_constant *cp2);

extern a_boolean expr_tree_contains_template_param_constant(
                                             an_expr_node_ptr  node,
                                             a_constant_ptr    cp);

extern a_boolean nontype_templ_arg_constant_references_non_external_entity(
                                                      a_constant_ptr constant);

extern a_boolean has_non_file_scope_ref(a_constant *cp);

extern a_boolean constant_is_shareable(a_constant *cp);

extern a_constant_ptr alloc_shareable_constant(a_constant *cp);

extern void add_backing_expression_for_named_constant(a_constant *cp);

extern void add_scope_to_class_type(a_type_ptr  type);

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

/* Macro to retrieve the list of constants associated with an enum type.  The
   location of the list is different depending on whether it's a scoped enum
   or not. */
#define enum_constants(tp)                                                   \
  (!integer_type_supp(tp)->enumerator_list_seen ?                            \
      (a_constant_ptr)NULL :                                                 \
      (integer_type_is_scoped_enum((tp)) ?                                   \
          (tp)->variant.integer.enum_info.assoc_scope->constants :           \
          (tp)->variant.integer.enum_info.constant_list))
     
extern a_boolean is_enum_constant(a_constant_ptr con);

extern a_boolean is_ordinary_string_constant(a_constant_ptr constant);

extern a_boolean is_wide_string_constant(a_constant_ptr constant);

#define is_normal_character_kind(kind)  ((kind) == (a_character_kind)chk_char)

extern void make_zero_of_proper_type(a_type_ptr desired_type,
                                     a_constant *zero_constant);

extern void make_uuidof_constant(a_type_ptr     uuidof_type,
                                 a_constant_ptr uuidof_con);

extern void make_typeid_constant(a_type_ptr     typeid_type,
                                 a_constant_ptr typeid_con);

extern
a_constructor_init_ptr copy_ctor_init(a_constructor_init_ptr   ctor_init,
                                      an_expr_copy_options_set options);

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

extern void add_temporary_to_front_of_variables_list(a_variable_ptr temp,
                                                     a_scope_ptr    scope);

extern a_variable_ptr alloc_temporary_variable(a_type_ptr temp_type,
                                               a_boolean  force_static);

extern a_field_ptr next_initializable_field(a_field_ptr field);

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Macros to examine property and event members (and their accessor functions).
*/
#define field_is_property_or_event(fp)  ((fp)->property_or_event_descr != NULL)
#define property_or_event_kind_is(ep, pek)                                   \
   ((ep)->property_or_event_descr->kind == (a_property_or_event_kind)pek)
#define field_is_nontrivial_property_or_event(fp)                            \
  (field_is_property_or_event(fp) &&                                         \
   !(fp)->property_or_event_descr->is_trivial)
#define var_is_property_or_event(vp)  ((vp)->property_or_event_descr != NULL)
#define rout_is_cli_accessor(rp) \
  ((rp)->special_kind >= (int)sfk_first_accessor && \
   (rp)->special_kind <= (int)sfk_last_accessor)
#define rout_is_generic_definition(rp) ((rp)->is_generic_definition)
#define rout_is_generic_instance(rp) ((rp)->is_generic_instance)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define field_is_property_or_event(fp)  /*lint --e(506)*/FALSE
#define field_is_nontrivial_property_or_event(fp)  /*lint --e(506)*/FALSE
#define rout_is_cli_accessor(rp)  /*lint --e(506)*/FALSE
#define rout_is_generic_definition(rp)  /*lint --e(506)*/FALSE
#define rout_is_generic_instance(rp)  /*lint --e(506)*/FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean is_compound_assignment_operator(an_expr_operator_kind op);

extern a_type_ptr fixed_point_result_type(a_type_ptr  type_1,
                                          a_type_ptr  type_2);

extern a_type_ptr compound_assignment_operation_type(an_expr_node_ptr expr);

extern a_type_kind binary_operation_type_kind(an_expr_operator_kind  op,
                                              a_type_ptr             op1_type,
                                              a_type_ptr             op2_type);

extern
a_dynamic_init_ptr effective_dynamic_init_for_initializer_list_object(
                                         a_dynamic_init_ptr dip,
                                         a_type_ptr         *init_entity_type);

extern void perform_scheduled_routine_moves(void);

extern void schedule_move_to_current_end_of_routines_list(a_routine_ptr  rp);

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

#if GNU_EXTENSIONS_ALLOWED
extern void change_block_into_statement_expression(a_statement_ptr block);
#endif /* GNU_EXTENSIONS_ALLOWED */

extern void set_expr_result_not_used(an_expr_node_ptr node);

extern void set_node_operator(an_expr_node_ptr      node,
                              an_expr_operator_kind kind,
	   	              a_type_ptr            type,
                              a_boolean             is_lvalue,
		              an_expr_node_ptr      operands);

extern an_expr_node_ptr make_operator_node(an_expr_operator_kind kind,
			   	           a_type_ptr            type,
			   	           an_expr_node_ptr      operands);

extern an_expr_node_ptr make_lvalue_operator_node(
                                               an_expr_operator_kind kind,
                                               a_type_ptr            type,
                                               an_expr_node_ptr      operands);

extern an_expr_node_ptr make_comma_node(an_expr_node_ptr expr1,
                                        an_expr_node_ptr expr2);

extern an_expr_node_ptr make_comma_node_if_necessary(an_expr_node_ptr node1,
                                                     an_expr_node_ptr node2);

extern void overwrite_node(an_expr_node_ptr node,
                           an_expr_node_ptr source_node);

extern an_expr_node_ptr error_node(void);

extern an_expr_node_ptr fs_error_node(void);

extern an_expr_node_ptr alloc_node_for_constant(a_constant *constant);

extern an_expr_node_ptr alloc_node_for_allocated_constant(
                                                         a_constant *constant);

extern an_expr_node_ptr node_for_integer_constant(long            value,
                                                  an_integer_kind kind);

extern an_expr_node_ptr node_for_host_large_integer(
					     a_host_large_integer	value,
                                             an_integer_kind		kind);

extern a_boolean is_bad_type_for_template_arg_operand(a_type_ptr type);

extern a_boolean is_cast_operation_node(an_expr_node_ptr expr);

extern a_boolean is_generated_dynamic_init(a_dynamic_init_ptr dip);

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
#define CTWS_COPY_ARG_OPERAND_INFO	0x2
			/* TRUE if arg_operand information on nontype
			   template arguments should be copied over to the
			   substituted arguments for use in a rescan. */
#define CTWS_NON_CONSTANT_EXPR		0x4
			/* TRUE when copying a non-constant expression,
			   which can come up under a sizeof. */
#define CTWS_IS_PARTIAL_ORDER_CHECK	0x8
			/* TRUE when creating the substituted routine type
			   as part of the partial ordering process. */
#define CTWS_INSIDE_EXPR_RESCAN		0x10
			/* TRUE when we're inside a rescan of an expression.
			   Used to control pushing a template instantiation
			   scope when we first enter an expression rescan,
			   and not again in any nested processing. */
#define CTWS_IS_CALL_CONTEXT		0x20
			/* TRUE when the substitution routines are called
			   from rescan contexts and the entity being
			   substituted is the function name in a call (e.g.,
			   was followed by a "(" in the source). */
#define CTWS_PRESERVE_DEDUCED_PACKS	0x40
			/* TRUE if a deduced parameter pack should be
			   retained in the substituted type if there are no
			   template arguments associated with the pack.
			   This is used during the initial substitution of
			   explicitly supplied template arguments so that
			   the resulting type will still be usable to deduce
			   the remaining pack. */

/*
Structure used to represent a set of function parameters that resulted from
a pack expansion during the type substitution process.
*/
typedef struct a_variadic_param_info *a_variadic_param_info_ptr;
typedef struct a_variadic_param_info {
  a_variadic_param_info_ptr
		next;
			/* The next element on the list of entries, or NULL
			   for the last entry. */
  a_param_type_ptr
		param_type;
			/* The parameter type entry resulting from the
			   variadic expansion. */
  a_param_type_ptr
		orig_param_type;
			/* The parameter type entry for the parameter pack
			   representing the variadic expansion. */
   int		level;
			/* The value of routine_type_levels when this entry
			   was created. */
} a_variadic_param_info;


/*
Structure used to pass information between the routines that do template
argument substitution (primarily copy_type_with_substitution).
*/
typedef struct a_ctws_state *a_ctws_state_ptr;
typedef struct a_ctws_state {
  a_variadic_param_info_ptr
		variadic_param_info;
			/* A list of parameters created by variadic
			   pack expansions during this substitution. */
  a_variadic_param_info_ptr
		variadic_param_info_tail;
			/* The end of the list of parameters created by
			   variadic pack expansions during this
			   substitution. */
  int32_t	routine_type_levels;
			/* The level of nesting of routine types. */
} a_ctws_state;


extern an_expr_node_ptr copy_template_param_expr(
                                 an_expr_node_ptr         expr,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_type_ptr               guide_type,
                                 a_source_position        *source_pos,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error,
                                 a_ctws_state_ptr         ctws_state,
                                 a_constant_ptr           constant,
                                 a_constant_ptr           *alloc_con);

extern a_type_ptr type_of_decltype_expr_with_substitution(
                                 a_type_ptr               type,
                                 an_expr_node_ptr         expr,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error,
                                 a_ctws_state_ptr         ctws_state);


typedef struct a_symbol a_symbol_il_h_dummy_typedef;
extern struct a_symbol *
symbol_for_template_param_unknown_entity_con_after_substitution(
                                 a_constant_ptr           con,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_source_position        *source_pos,
                                 a_ctws_options_set       options);

extern a_constant_ptr copy_template_param_con_with_substitution(
                                 a_constant_ptr           con,
                                 a_template_arg_ptr       template_arg_list,
                                 struct a_template_param  *template_param_list,
                                 a_type_ptr               template_param_type,
                                 a_source_position        *source_pos,
                                 a_ctws_options_set       options,
                                 a_boolean                *copy_error,
                                 a_ctws_state_ptr         ctws_state);

extern void increment_template_dependent_enum_constant(a_constant_ptr  con);

extern a_boolean is_operator_returning_bool(an_expr_operator_kind op);

extern an_expr_node_ptr add_cast(an_expr_node_ptr node,
                                 a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_if_necessary(an_expr_node_ptr node,
                                              a_type_ptr       new_type);

extern an_expr_node_ptr add_cast_to_lvalue(an_expr_node_ptr node,
                                           a_type_ptr       type);

extern an_expr_node_ptr add_cast_to_lvalue_if_necessary(an_expr_node_ptr node,
                                                        a_type_ptr       type);

extern an_expr_node_ptr add_rvalue_class_adjust_node(an_expr_node_ptr node,
                                                     a_type_ptr       type);

extern an_expr_node_ptr copy_node(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_list_of_expr_trees(
                                            an_expr_node_ptr         expr_list,
                                            an_expr_copy_options_set options);

extern an_expr_node_ptr copy_expr_tree(an_expr_node_ptr         expr,
                                       an_expr_copy_options_set options);

extern a_dynamic_init_ptr copy_dynamic_init(a_dynamic_init_ptr       dip,
                                            an_expr_copy_options_set options);

extern an_expr_node_ptr copy_default_arg_expr(
                               a_routine_ptr    rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated,
                               a_boolean        evaluated);

extern an_expr_node_ptr duplicate_default_arg_expr(an_expr_node_ptr expr);

extern an_expr_node_ptr copy_default_arg_expr_list(
                               a_routine_ptr    rout_ptr,
                               a_param_type_ptr ptp,
                               a_boolean        inside_conditional_expression,
                               a_boolean        potentially_evaluated,
                               a_boolean        evaluated);

extern a_boolean is_gc_lvalue_expr(an_expr_node_ptr expr);

extern a_boolean cannot_be_null(an_expr_node_ptr expr);

extern an_expr_node_ptr var_lvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_rvalue_expr(a_variable_ptr var);

extern an_expr_node_ptr var_addr_expr(a_variable_ptr var);

extern an_expr_node_ptr function_lvalue_expr(a_routine_ptr rout);

extern an_expr_node_ptr function_rvalue_expr(a_routine_ptr rout);

extern an_expr_node_ptr function_addr_expr(a_routine_ptr rout);

extern an_expr_node_ptr rvalue_expr_for_lvalue(an_expr_node_ptr expr);

extern an_expr_node_ptr add_indirection_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr add_ref_indirection_to_node(an_expr_node_ptr node);

extern a_type_ptr type_of_address_of(an_expr_node_ptr node);

extern an_expr_node_ptr add_address_of_to_node(an_expr_node_ptr node);

extern an_expr_node_ptr add_reference_to_to_node(an_expr_node_ptr node);

extern void set_address_taken_for_variable_or_routine_expr(
                                                        an_expr_node_ptr node);

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
extern void adjust_nonstandard_anonymous_object_field_references(
                                                  an_expr_node_ptr node,
                                                  struct a_symbol  *field_sym,
                                                  a_boolean        std_also);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

extern an_expr_node_ptr fe_field_lvalue_selection_expr(an_expr_node_ptr node,
                                                       a_field_ptr      field);

extern a_boolean is_rvalue_reference_object_expr(an_expr_node_ptr expr);

extern an_expr_node_ptr base_class_selection_expr(an_expr_node_ptr node,
                                                  a_base_class_ptr bcp);

extern void mark_routine_referenced_full(a_routine_ptr routine,
                                         a_boolean     instantiate,
                                         a_boolean     elided_reference);

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

extern a_boolean has_nonreal_parent_type(a_source_correspondence	*scp);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void add_to_ms_attributes_list(an_ms_attribute_ptr	msap,
                                      a_scope_depth		scope_depth);

extern a_routine_ptr selectively_overridden_function(a_routine_ptr  rp);

extern an_assembly_visibility get_assembly_visibility_of(a_type_ptr  type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if GENERATE_MICROSOFT_IF_EXISTS_ENTRIES
extern void add_to_ms_if_exists_list(an_ms_if_exists_ptr	msiep,
                                     a_scope_depth		scope_depth);
#endif /* GENERATE_MICROSOFT_IF_EXISTS_ENTRIES */

#if RECORD_MACROS_IN_IL
extern void add_to_macros_list(a_macro_ptr  mp);
#endif /* RECORD_MACROS_IN_IL */

extern void add_to_pragma_list(a_pragma_ptr             pragma,
                               a_scope_depth            scope_depth,
                               a_source_correspondence  *scp);

extern a_pragma_ptr find_assoc_pragma(char          *il_entity,
                                      a_scope_ptr   scope,
                                      a_type_ptr    class_type,
                                      a_pragma_ptr  prev_assoc_pragma);

extern a_boolean operator_takes_lvalue_operand(an_expr_operator_kind op);

EXTERN an_object_lifetime_ptr
		curr_object_lifetime;
			/* The top of the currently active object lifetime
			   stack. */

extern void add_to_end_of_destructions_list(a_dynamic_init_ptr      dip,
                                            an_object_lifetime_ptr  olp);

extern void add_to_destructions_list_following(a_dynamic_init_ptr dip,
                                               a_dynamic_init_ptr new_dip);

extern void record_end_of_lifetime_destruction(
                                        a_dynamic_init_ptr  dip,
                                        a_boolean           static_lifetime,
                                        a_boolean           block_lifetime);

extern void record_partial_aggregate_cleanup_destruction(
                                                 a_dynamic_init_ptr dip,
                                                 a_boolean          evaluated);

extern void promote_lifetime_contents_to_curr_object_lifetime(
                                                      an_object_lifetime *olp);

extern void add_as_child_of_curr_object_lifetime(an_object_lifetime_ptr olp);

extern void free_object_lifetime(an_object_lifetime_ptr  olp);

extern an_object_lifetime_ptr init_expr_lifetime_of(a_dynamic_init_ptr dip);

extern void bind_object_lifetime(an_object_lifetime_ptr  olp,
                                 an_il_entry_kind        entity_kind,
                                 char                    *entity_ptr);

extern void unbind_object_lifetime(an_object_lifetime_ptr  olp);

extern
void push_or_repush_object_lifetime(an_il_entry_kind         entity_kind,
                                    char                     *entity_ptr,
                                    an_object_lifetime_ptr   olp,
                                    an_object_lifetime_kind  kind);

extern void push_object_lifetime(an_il_entry_kind         entity_kind,
                                 char                     *entity_ptr,
                                 an_object_lifetime_kind  kind);

extern a_boolean is_useless_object_lifetime(an_object_lifetime_ptr  olp);

extern void remove_from_destruction_list(a_dynamic_init_ptr  dip);

extern void unlink_expr_destructions(an_expr_node_ptr expr);

extern void mark_object_lifetime_as_useless(an_object_lifetime_ptr  olp);

extern a_boolean pop_object_lifetime_full(a_boolean unbound_okay);

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
				 a_boolean	   preinclude_macros_only,
				 a_boolean	   is_implicit_include,
				 a_boolean	   from_system_include_dir,
				 a_boolean	   is_assembly_file);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void record_inclusion_of_assembly_source_file(
                                     char              *file_name,
                                     char              *full_name,
                                     char              *name_as_written,
                                     a_source_file_ptr *new_file,
                                     a_boolean         is_system_include,
                                     a_boolean         is_preinclude,
                                     a_source_position *inserted_position);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void record_resumption_of_source_file(a_source_file_ptr	curr_file,
					     a_seq_number	seq_number,
					     a_line_number	line_number);

extern void record_end_of_source_file(a_source_file_ptr curr_file,
			              a_seq_number      seq_number);
extern a_source_file_ptr primary_source_file_for_seq(a_seq_number seq_number);
extern a_source_file_ptr source_file_for_seq(a_seq_number   seq_number,
                                             a_line_number  *line_number,
                                             a_boolean      *at_end_of_source,
                                             a_boolean      physical_line);
extern void conv_seq_to_file_and_line(a_seq_number  seq_number,
			              char          **file_name,
				      char          **full_name,
				      a_line_number *line_number,
                                      a_boolean     *at_end_of_source);

extern a_source_file_ptr eff_primary_source_file(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_cli_metadata_file_ptr map_assembly_index_to_cmfp(
                                             an_assembly_index assembly_index);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if !STANDALONE_UTILITY_PROGRAM

extern void conv_seq_to_physical_file_and_line(
                                          a_seq_number      seq_number,
                                          a_source_file_ptr *src_file,
                                          a_line_number     *physical_line,
                                          a_boolean         *at_end_of_source);

extern a_boolean seq_is_in_include_file(a_seq_number seq_number);

extern void break_instance_source_corresp(a_source_correspondence *sc);

extern void break_source_corresp(a_source_correspondence *sc);

extern void break_constant_source_corresp(a_constant_ptr cp);

#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_boolean seq_is_in_system_header(a_seq_number  seq_number);

extern a_source_correspondence *source_corresp_for_il_entry(
                                                 char              *entity_ptr,
                                                 an_il_entry_kind  kind);

extern a_boolean is_zero_constant(a_constant *constant);

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
Macro that returns TRUE if the type is unnamed.
*/
#define type_is_unnamed(type)                                           \
  (unmangled_name_of(&(type)->source_corresp) == NULL)

/*
Return TRUE if type is a lambda closure class.
*/
#define type_is_lambda_closure(type)					\
  ((type)->kind == (a_type_kind)tk_class &&				\
   class_type_supp(type)->is_lambda_closure_class)

/*
Return TRUE if rout_type is a routine type for a lambda.
*/
#define is_lambda_body_routine_type(rout_type)				\
  ((rout_type)->variant.routine.extra_info->assoc_routine != NULL &&	\
   (rout_type)->variant.routine.extra_info->assoc_routine->is_lambda_body)

/*
Return the unmangled name of an entity, given a pointer to its source
correspondence entry (for unnamed types that have been given a fabricated
name during mangling, returns NULL).
*/
#if NEED_NAME_MANGLING
#define unmangled_name_of(scp)                                          \
  ((scp)->unnamed_entity_given_fabricated_name ? (char *)NULL :         \
     (((scp)->name_has_been_mangled ?                                   \
       (scp)->unmangled_name_or_mangled_encoding : (scp)->name)))
#else /* !NEED_NAME_MANGLING */
#define unmangled_name_of(scp) ((scp)->name)
#endif /* NEED_NAME_MANGLING */

/*
Return the unmangled or fabricated (name given to unnamed entities during
mangling) name of an entity.
*/
#if NEED_NAME_MANGLING
#define unmangled_or_fabricated_name_of(scp)                            \
  ((scp)->name_has_been_mangled ?                                       \
   (scp)->unmangled_name_or_mangled_encoding : (scp)->name)
#else /* !NEED_NAME_MANGLING */
#define unmangled_or_fabricated_name_of(scp) (unmangled_name_of((scp)))
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

extern void clear_local_scope_ref_if_present(a_source_correspondence *scp);

extern void clear_parent(a_source_correspondence *scp);

extern void set_parent_scope(a_source_correspondence *scp,
                             an_il_entry_kind        entry_kind,
                             a_scope_ptr             parent_scope);

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
  ((arg)->kind == (a_templ_arg_kind)tak_type)
#define is_nontype_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_nontype)
#define is_template_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_template)
#define is_start_of_pack_expansion_templ_arg(arg) \
  ((arg)->kind == (a_templ_arg_kind)tak_start_of_pack_expansion)


extern a_boolean con_is_exact_addr_of_variable(
                                           a_constant_ptr con,
                                           a_variable_ptr *var,
                                           a_boolean      array_decay_allowed);

/*
Macro that returns TRUE if a constant entry is the exact address of
a routine.
*/
#define con_is_exact_addr_of_routine(con)                            \
  ((con)->kind == (a_constant_repr_kind)ck_address &&                 \
   (con)->variant.address.kind == (an_address_base_kind)abk_routine &&\
   (con)->variant.address.offset == 0 && !(con)->implicit_cast)

extern a_type_ptr make_auto_type(a_source_position *pos);

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

/* Return TRUE if the given class needs a virtual function table. */
#if !IA64_ABI
#define needs_virtual_function_table(type)                            \
  ((type)->variant.class_struct_union.any_virtual_functions)
#else /* IA64_ABI */
#define needs_virtual_function_table(type)                            \
  ((type)->variant.class_struct_union.any_virtual_functions ||        \
   (type)->variant.class_struct_union.any_virtual_base_classes)
#endif /* !IA64_ABI */

/* Return TRUE if two template nesting depths should be considered
   equivalent.  Depths are equivalent if they are the same, or if either
   of the depths is NO_NESTING_DEPTH. */
#define equiv_nesting_depths(depth1, depth2)				\
  ((depth1) == (depth2) ||						\
   (depth1) == NO_NESTING_DEPTH || (depth2 == NO_NESTING_DEPTH))

typedef struct a_translation_unit a_translation_unit_dummy_typedef;

#if !STANDALONE_UTILITY_PROGRAM
struct a_translation_unit *trans_unit_for_source_corresp(
                                                 a_source_correspondence *scp);
#endif /* !STANDALONE_UTILITY_PROGRAM */

extern a_routine_ptr enclosing_routine_for_local_type_or_null(a_type_ptr type);

extern a_routine_ptr enclosing_routine_for_local_type(a_type_ptr type);

extern a_scope_ptr function_scope_for_local_type(a_type_ptr type);

extern a_scope_ptr scope_for_routine(a_routine_ptr rout);

#if DEBUG
extern void db_template_arg_list(a_template_arg_ptr tap);

extern void db_template_name(a_template_ptr  tp);

extern void db_type_name(a_type_ptr  tp);

extern void db_name_full(a_source_correspondence *sc,
                         an_il_entry_kind        kind);

extern void db_name(a_source_correspondence *sc);

extern char *db_name_str_full(a_source_correspondence *scp,
                              an_il_entry_kind        kind,
                              a_boolean               include_func_params);

extern char *db_name_str(a_source_correspondence *sc,
                         an_il_entry_kind        kind);

extern void db_entity_info(char             *entry,
                           an_il_entry_kind kind);

extern void db_access_control(an_access_specifier as);

extern void db_class_list(a_class_list_entry_ptr list);

extern void db_constant(a_constant *cp);

extern void db_type(a_type *tp);

extern void db_function_param_list(a_type_ptr  tp);

extern char* db_qualifiers_str(a_type_qualifier_set  qualifiers);

extern void db_abbreviated_type(a_type *tp);

/* Abbreviated version of db_abbreviated type. */
/*lint -esym(755,db_abbr_type)*/
#define db_abbr_type(tp)                                              \
  (db_abbreviated_type(tp), (void)fputc('\n', f_debug))

extern void db_variable(a_variable_ptr var_ptr);

extern void db_expression(an_expr_node_ptr node);

extern void db_expr_range(an_expr_node_ptr node);

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

extern unsigned long db_show_il_c_fe_space_used(unsigned long grand_total);

extern unsigned long show_il_space_used(void);

extern void db_seq_number_lookup_table(void);

extern void db_source_file_for_seq_info(void);

extern a_line_number db_line_for_seq(a_seq_number seq_number);

extern void db_scheduled_routine_moves(void);

extern void put_str_to_f_debug(char                                  *str,
                               an_il_to_str_output_control_block_ptr octl);
#endif /* DEBUG */

#if ORPHAN_PROCESSING_NEEDED
/*
Record a file-scope entry as a potential orphan.  The macro here ensures
that once the entry is placed on an orphan list the subroutine is no
longer called (well, except if it's the last entry on the list).
The orphan is recorded in the current translation unit.
*/
#define add_orphaned_file_scope_il_entry(entry_ptr, entry_kind)       \
{ if (fs_orphan_pointer_of(entry_ptr) == NULL) {                      \
    f_add_orphaned_file_scope_il_entry((entry_ptr), (entry_kind),     \
                                       curr_translation_unit);        \
  }  /* if */                                                         \
}  /* add_orphaned_file_scope_il_entry */
/*
Like add_orphaned_file_scope_il_entry, but do not enter certain kinds
of entries that can never be orphans (e.g., class members).  The
orphan is recorded in the current translation unit.
*/
#define possibly_add_orphaned_file_scope_il_entry(entry_ptr, entry_kind) \
{ if (fs_orphan_pointer_of(entry_ptr) == NULL) { \
    f_possibly_add_orphaned_file_scope_il_entry((entry_ptr), (entry_kind), \
                                                curr_translation_unit); \
  }  /* if */ \
}  /* possibly_add_orphaned_file_scope_il_entry */
extern
void f_possibly_add_orphaned_file_scope_il_entry(
                                          char                      *entry_ptr,
                                          an_il_entry_kind          entry_kind,
                                          struct a_translation_unit *tup);
#endif /* ORPHAN_PROCESSING_NEEDED */
#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
#if !STANDALONE_UTILITY_PROGRAM
extern void add_scope_orphaned_il_lists(a_scope_ptr scope);
#endif /* !STANDALONE_UTILITY_PROGRAM */
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

void eliminate_pragmas_for_file_scope_entities(a_scope_ptr scope);

extern void clear_function_body(a_scope_ptr sp);

void detach_from_object_lifetime_tree(an_object_lifetime_ptr olp);

#if MAINTAIN_NEEDED_FLAGS
extern void eliminate_bodies_of_unneeded_functions(void);

#if SCOPE_ORPHANED_LIST_PROCESSING_NEEDED
extern void eliminate_unneeded_scope_orphaned_list_entries(void);
#endif /* SCOPE_ORPHANED_LIST_PROCESSING_NEEDED */

extern void eliminate_default_arg_object_lifetimes(a_type_ptr type);

extern void eliminate_routine_default_arg_object_lifetimes(a_routine_ptr rout);

extern void eliminate_variable_default_arg_object_lifetimes(
                                                           a_variable_ptr var);

extern void eliminate_unneeded_il_entries(a_scope_ptr scope);
#endif /* MAINTAIN_NEEDED_FLAGS */

extern void clear_variable_definition(a_variable_ptr variable);

extern a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type,
                                                     a_boolean  *unknown);

extern a_namespace_ptr f_skip_namespace_aliases(a_namespace_ptr nsp);

extern a_boolean is_member_of_unnamed_namespace(a_source_correspondence *scp);

#if DO_IL_LOWERING
extern a_boolean routine_should_be_externalized_for_exported_templates(
                                                           a_routine_ptr rout);
extern a_boolean variable_should_be_externalized_for_exported_templates(
                                                           a_variable_ptr var);
#endif /* DO_IL_LOWERING */

/*
Given a namespace pointer, return a pointer to the actual namespace,
skipping any namespace aliases that might be present.
*/
#define skip_namespace_aliases(nsp)					\
  ((nsp)->is_namespace_alias ? f_skip_namespace_aliases(nsp) : (nsp))

extern a_type_ptr init_predeclared_class(a_type_kind  kind,
                                         char         *name);

extern void enter_predeclared_class(a_type_ptr         predeclared_type,
                                    a_scope_depth      scope_depth,
                                    a_source_position  *pos);

extern a_targ_alignment alignment_of_variable(a_variable_ptr  vp);

extern an_attribute_ptr f_find_attribute(a_byte_attribute_kind  kind,
                                         an_attribute_ptr       attributes);

#define find_attribute(kind, attributes)                                     \
  (f_find_attribute((a_byte_attribute_kind)(kind), (attributes)))

#define routine_does_not_return(rp)                                          \
  (skip_typerefs(rp->type)->variant.routine.extra_info->does_not_return)

extern a_hash_value hash_constant(a_constant *cp);

extern a_hash_value hash_template_arg_list(a_template_arg_ptr	tap);

extern a_boolean compare_expressions(an_expr_node_ptr                node1,
                                     an_expr_node_ptr                node2,
                                     a_compare_constants_options_set options);

extern void rebuild_structures_on_il_read(void);

#if CHECKING
#if !(STANDALONE_UTILITY_PROGRAM && PROTOTYPE_INSTANTIATIONS_IN_IL)
extern a_boolean node_operands_have_correct_lvalueness(an_expr_node_ptr node);

extern a_boolean tree_has_correct_lvalueness(an_expr_node_ptr root);
#endif /* !(STANDALONE_UTILITY_PROGRAM && PROTOTYPE_INSTANTIATIONS_IN_IL) */

extern void check_operation_node_consistency(an_expr_node_ptr expr);
extern void check_result_not_used_flag(an_expr_node_ptr node);
#endif /* CHECKING */

#if ENSURE_LOWERED_TYPE_LIST_ORDERING
extern void fix_type_list_ordering_problems(void);
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING */

#if TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES
extern a_targ_alignment field_alignment_for(a_type_ptr  type);
#else /* !TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */
/*
The field alignment is equal to the intrinsic alignment of the type.
*/
#define field_alignment_for(tp) (alignment_of_type(tp))
#endif /* TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */

extern void il_reset(void);

extern void il_one_time_init(void);

extern void il_trans_unit_init(void);

extern void il_init(void);


#if UPC_EXTENSIONS_ALLOWED

#define upc_dynamic_threads() (upc_num_threads == 0)

extern a_boolean upc_block_size_too_large(a_host_large_unsigned  block_size);

EXTERN a_upc_block_size
		max_upc_block_size
#if VAR_INITIALIZERS
                         = MAX_UPC_BLOCK_SIZE
#endif /* VAR_INITIALIZERS */
                                             ;
			/* The maximum allowable UPC block size. */
#endif /* UPC_EXTENSIONS_ALLOWED */

#if MODULE_ID_NEEDED

extern void use_variable_or_routine_for_module_id_if_needed(
                                             a_source_correspondence_ptr scp,
                                             an_il_entry_kind            kind);

#endif /* MODULE_ID_NEEDED */

extern void destination_type_for_reference_cast(an_expr_node_ptr  expr,
                                                a_type            *ref_type,
                                                a_type            *quals_type);

extern a_boolean pm_constant_is_null(a_constant_ptr constant);

#if MAINTAIN_NEEDED_FLAGS
extern void clear_instantiation_required_on_unneeded_entities(
                                                            a_scope_ptr scope);
#endif /* MAINTAIN_NEEDED_FLAGS */

extern an_expr_node_ptr make_dummy_lvalue_expr(a_type_ptr type);

#endif /* ifndef IL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
