/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2009 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

attribute.h -- Declarations related to attribute.c (having to do with 
               attributes, a GCC C extension).

*/

/* Avoid including these declarations more than once: */
#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H 1

/* This type definition is declared even if GNU extensions are not
   enabled because some functions take a_gnu_attribute_ptr as a parameter
   type, and the parameter lists for functions are always the same,
   independent of the configuration of the front end. */
typedef struct a_gnu_attribute *a_gnu_attribute_ptr;

#if REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void redefine_extname_pragma(a_pending_pragma_ptr  ppp);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void process_alias_fixup_list(void);
extern unsigned long show_attribute_space_used(void);
extern void attribute_init(void);
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED

extern void record_asm_name_for_lookup(a_symbol_ptr  sym);

#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
extern void push_ELF_visibility(an_ELF_visibility_kind  evk,
                                a_boolean               namespace_attribute);

extern void pop_ELF_visibility(a_boolean  namespace_attribute);

extern an_ELF_visibility_kind ELF_visibility_from_string(
                                                       char  *visibility_str);

extern void update_for_default_ELF_visibility(
                                         an_ELF_visibility_kind  *visibility);
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */

/*
Enumeration of attributes that are accepted.
*/
enum a_gnu_attribute_kind_tag {
  gak_error,
  gak_first,
  gak_mode = gak_first,
#if USER_CONTROL_OF_STRUCT_PACKING
  gak_aligned,
  gak_packed,
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  gak_unused,
  gak_used,
  gak_deprecated,
  gak_constructor,
  gak_destructor,
  gak_noreturn,
  gak_volatile,
  gak_pure,
  gak_const,
  gak_weak,
  gak_weakref,
  gak_section,
  gak_alias,
  gak_malloc,
  gak_nocommon,
  gak_transparent_union,
  gak_format,
  gak_format_arg,
  gak_sentinel,
#if GNU_NAKED_ATTRIBUTE_ALLOWED
  gak_naked,
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
  gak_no_instrument_function,
  gak_no_check_memory_usage,
#if GNU_X86_ATTRIBUTES_ALLOWED
  gak_cdecl,
  gak_stdcall,
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  gak_visibility,
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
  gak_init_priority,
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
  gak_strong,
  gak_nonnull,
  gak_noinline,
  gak_always_inline,
  gak_cleanup,
  gak_nothrow,
  gak_warn_unused_result,
#if GNU_VECTOR_TYPES_ALLOWED
  gak_vector_size,
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  gak_last
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_gnu_attribute_kind;

/*
Names of attribute kinds.
*/
EXTERN char *attribute_kind_names[(int)gak_last + 1]
#if VAR_INITIALIZERS
= {
/* gak_error */                      "error",
/* gak_mode */                       "mode",
#if USER_CONTROL_OF_STRUCT_PACKING
/* gak_aligned */                    "aligned",
/* gak_packed */                     "packed",
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
/* gak_unused */                     "unused",
/* gak_used */                       "used",
/* gak_deprecated */                 "deprecated",
/* gak_constructor */                "constructor",
/* gak_destructor */                 "destructor",
/* gak_noreturn */                   "noreturn",
/* gak_volatile */                   "volatile",
/* gak_pure */                       "pure",
/* gak_const */                      "const",
/* gak_weak */                       "weak",
/* gak_weakref */                    "weakref",
/* gak_section */                    "section",
/* gak_alias */                      "alias",
/* gak_malloc */                     "malloc",
/* gak_nocommon */                   "nocommon",
/* gak_transparent_union */          "transparent_union",
/* gak_format */                     "format",
/* gak_format_arg */                 "format_arg",
/* gak_sentinel */                   "sentinel",
#if GNU_NAKED_ATTRIBUTE_ALLOWED
/* gak_naked */                      "naked",
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
/* gak_no_instrument_function */     "no_instrument_function",
/* gak_no_check_memory_usage */      "no_check_memory_usage",
#if GNU_X86_ATTRIBUTES_ALLOWED
/* gak_cdecl */                      "cdecl",
/* gak_stdcall */                    "stdcall",
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/* gak_visibility */                 "visibility",
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
/* gak_init_priority */              "init_priority",
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
/* gak_strong */			    "strong", 
/* gak_nonnull */		    "nonnull", 
/* gak_noinline */		    "noinline", 
/* gak_always_inline */		    "always_inline", 
/* gak_cleanup */		    "cleanup", 
/* gak_nothrow */		    "nothrow", 
/* gak_warn_unused_result */	    "warn_unused_result", 
#if GNU_VECTOR_TYPES_ALLOWED
/* gak_vector_size */                "vector_size", 
#endif /* GNU_VECTOR_TYPES_ALLOWED */
/* gak_last */                       "last" /* used to check that
                                              initialization is right. */
}
#endif /* VAR_INITIALIZERS */
;

/*
Enumeration of format attribute kinds.
*/
enum a_format_attribute_kind_tag {
  fak_none,
  fak_first,
  fak_printf = fak_first,
  fak_scanf,
  fak_strftime,
  fak_last
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_format_attribute_kind;

/* 
Names of format attribute kinds. 
*/
EXTERN char *format_attribute_kind_names[(int)fak_last + 1] 
#if VAR_INITIALIZERS
= {
/* fak_none */          "none",
/* fak_printf */        "printf",
/* fak_scanf */         "scanf",
/* fak_strftime */      "strftime",
/* fak_last */          "last" /* used to check that initialization is
                                  right. */
}
#endif /* VAR_INITIALIZERS */
;

/* 
Entry containing information about a GNU attribute.  
*/
typedef struct a_gnu_attribute {
  a_gnu_attribute_kind
  		kind;
			/* Which kind of attribute. */
  a_source_position
                position;
			/* Source location for the attribute. */
  a_byte_boolean
		is_declarator_attribute;
			/* TRUE if this attribute was scanned as part of
			   the declarator. */
  union {
    /* When kind == gak_packed, gak_unused, gak_used, gak_deprecated,
       gak_constructor, or gak_destructor, no variant fields. */
#if USER_CONTROL_OF_STRUCT_PACKING
    /* When kind == gak_aligned. */
    a_targ_alignment
    		alignment;
			/* The alignment for the entity to which this
			   attribute applies. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    /* When kind == gak_mode. */
    struct {
      a_type_mode_kind
		kind;
			/* The element mode for the entity to which this
			   attribute applies (does not include vector
			   length). */
      a_targ_size_t
		length;
			/* The vector length if this is a vector mode.
			   Otherwise, zero. */
    } mode;
    /* When kind == gak_section. */
    char        *section;
			/* The section indicated for the entity to
			   which this attribute applies. */
    /* When kind == gak_alias or gak_weakref. */
    char        *alias;
			/* The name of the entity for which this entity is an
			   alias. */
    /* When kind == gak_format. */
    struct {
      a_format_attribute_kind
		kind;
			/* The kind of format (printf, scanf, strftime)
			   specified. */
      int	fmt_arg;
			/* The index of the argument (starting from 1) that
			   contains the format string. */
      int	first_subst_arg;
			/* The index of the first argument (starting from 1)
			   that will be substituted into the format string. */
    } format;
    /* When kind == gak_format_arg. */
    int         fmt_arg;
			/* The index (starting from 1) of the argument
			   that is a format string. */
    /* When kind == gak_sentinel. */
    int		sentinel_pos;
			/* The sentinel position (counted backward from the
			   last argument position, which is number one). This
			   representation is "one off" compared to the source
			   form: I.e., "sentinel(0)" in the source is
			   represented with sentinel_pos == 1 to reserve
			   sentinel_pos == 0 as a representation for "no
			   sentinel". */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    /* When kind == gak_visibility. */
    an_ELF_visibility_kind
		ELF_visibility;
			/* The visibility of an entity in an ELF object
			   file. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    /* When kind == gak_init_priority. */
    a_gnu_init_priority
		init_priority;
			/* The initialization priority of a dynamically
			   initialized namespace-scope variable. */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    int		nonnull_param;
			/* The parameter number (starting at 1) that must be
			   non-NULL.  If zero, all pointer parameters must be
			   non-NULL. */
    a_routine_ptr
		cleanup_routine;
			/* The routine specified by the cleanup attribute. */
#if GNU_VECTOR_TYPES_ALLOWED
    a_constant_ptr
		vector_size;
			/* The size (in bytes) of the requested vector type. */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  } variant;
  a_gnu_attribute_ptr
  		next;
			/* The next attribute in the list. */
} a_gnu_attribute;

extern a_gnu_attribute_ptr f_scan_gnu_attributes(
                                         a_token_sequence_number  *last_token,
                                         a_source_position        *end_pos);

#define scan_gnu_attributes()                                              \
   f_scan_gnu_attributes((a_token_sequence_number*)NULL,                   \
                         (a_source_position*)NULL)

extern a_gnu_attribute_ptr copy_gnu_attribute_list(
                                              a_gnu_attribute_ptr attributes);

extern void free_gnu_attribute_list(a_gnu_attribute_ptr  attributes);

extern a_gnu_attribute_ptr *last_gnu_attribute_link(
                                             a_gnu_attribute_ptr *attributes);

extern a_type_ptr get_type_with_mode(a_type_ptr        type,
                                     a_type_mode_kind  mode,
                                     a_source_position *pos);

extern a_type_ptr apply_gnu_attributes_to_variable_type(
                                               a_gnu_attribute_ptr  attributes,
                                               a_type_ptr           type);

extern void apply_gnu_attributes_to_variable(
                                           a_gnu_attribute_ptr  attributes,
                                           a_variable_ptr       vp,
                                           a_boolean            is_definition);

extern void apply_gnu_attributes_to_field(a_gnu_attribute_ptr  attributes,
                                          a_field_ptr          fp);

extern void apply_gnu_attributes_to_routine(a_gnu_attribute_ptr  attributes,
                                            a_routine_ptr        rp);

extern void apply_gnu_attributes_to_type(a_gnu_attribute_ptr attributes,
                                         a_type_ptr          tp,
                                         a_boolean           is_typedef);

extern void apply_gnu_attributes_to_typedef(a_gnu_attribute_ptr  attributes,
                                            a_type_ptr           tp,
                                            a_boolean            linkage_name);

extern void apply_gnu_attributes_to_label(a_gnu_attribute_ptr  attributes,
                                          a_label_ptr          label);

extern
void apply_gnu_attributes_to_using_directive(a_gnu_attribute_ptr  attributes,
					     a_using_decl_ptr     udp,
					     a_namespace_ptr      nsp);

extern void apply_gnu_attributes_to_current_namespace(
                                             a_gnu_attribute_ptr  attributes);

extern a_type_ptr apply_type_transforming_attributes(a_type_ptr           tp,
                                                     a_gnu_attribute_ptr  *ap);

extern void check_for_invalid_param_attributes(a_symbol_ptr        sym,
                                               a_gnu_attribute_ptr attributes);

extern void check_function_param_attributes(a_func_info_block_ptr func_info);

extern a_boolean check_transparent_union(a_type_ptr        tp,
                                         a_source_position *pos);

extern a_type_ptr copy_gnu_type_attributes(a_type_ptr  dst,
                                           a_type_ptr  src);

extern void copy_class_attributes_to_variable(a_type_ptr      class_type,
                                              a_variable_ptr  var);

extern void copy_class_attributes_to_routine(a_type_ptr     class_type,
                                             a_routine_ptr  routine);

extern void attribute_one_time_init(void);

extern void attribute_trans_unit_init(void);

#endif /* GNU_EXTENSIONS_ALLOWED */

#endif /* ATTRIBUTE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2009 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
