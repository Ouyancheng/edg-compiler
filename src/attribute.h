/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
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
   enabled because some functions take an_attribute_ptr as a parameter
   type, and the parameter lists for functions are always the same,
   independent of the configuration of the front end. */
typedef struct an_attribute *an_attribute_ptr;

#if REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void redefine_extname_pragma(a_pending_pragma_ptr  ppp);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void process_alias_fixup_list(void);
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED

/*
Enumeration of attributes that are accepted.
*/
enum an_attribute_kind_tag {
  ak_error,
  ak_first,
  ak_mode = ak_first,
#if USER_CONTROL_OF_STRUCT_PACKING
  ak_aligned,
  ak_packed,
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  ak_unused,
  ak_used,
  ak_deprecated,
  ak_constructor,
  ak_destructor,
  ak_noreturn,
  ak_volatile,
  ak_pure,
  ak_const,
  ak_weak,
  ak_section,
  ak_alias,
  ak_malloc,
  ak_nocommon,
  ak_transparent_union,
  ak_format,
  ak_format_arg,
#if GNU_NAKED_ATTRIBUTE_ALLOWED
  ak_naked,
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
  ak_no_instrument_function,
  ak_no_check_memory_usage,
#if GNU_X86_ATTRIBUTES_ALLOWED
  ak_cdecl,
  ak_stdcall,
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  ak_visibility,
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
  ak_init_priority,
  ak_last
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte an_attribute_kind;

/*
Names of attribute kinds.
*/
EXTERN char *attribute_kind_names[(int)ak_last + 1]
#if VAR_INITIALIZERS
= {
/* ak_error */                      "error",
/* ak_mode */                       "mode",
#if USER_CONTROL_OF_STRUCT_PACKING
/* ak_aligned */                    "aligned",
/* ak_packed */                     "packed",
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
/* ak_unused */                     "unused",
/* ak_used */                       "used",
/* ak_deprecated */                 "deprecated",
/* ak_constructor */                "constructor",
/* ak_destructor */                 "destructor",
/* ak_noreturn */                   "noreturn",
/* ak_volatile */                   "volatile",
/* ak_pure */                       "pure",
/* ak_const */                      "const",
/* ak_weak */                       "weak",
/* ak_section */                    "section",
/* ak_alias */                      "alias",
/* ak_malloc */                     "malloc",
/* ak_nocommon */                   "nocommon",
/* ak_transparent_union */          "transparent_union",
/* ak_format */                     "format",
/* ak_format_arg */                 "format_arg",
#if GNU_NAKED_ATTRIBUTE_ALLOWED
/* ak_naked */                      "naked",
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
/* ak_no_instrument_function */     "no_instrument_function",
/* ak_no_check_memory_usage */      "no_check_memory_usage",
#if GNU_X86_ATTRIBUTES_ALLOWED
/* ak_cdecl */                      "cdecl",
/* ak_stdcall */                    "stdcall",
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/* ak_visibility */                 "visibility",
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
/* ak_init_priority */              "init_priority",
/* ak_last */                       "last" /* used to check that
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
Entry containing information about an attribute.  
*/
typedef struct an_attribute {
  an_attribute_kind
  		kind;
			/* Which kind of attribute. */
  a_source_position
                position;
			/* Source location for the attribute. */
  union {
    /* When kind == ak_packed, ak_unused, ak_used, ak_deprecated,
       ak_constructor, or ak_destructor, no variant fields. */
#if USER_CONTROL_OF_STRUCT_PACKING
    /* When kind == ak_aligned. */
    a_targ_alignment
    		alignment;
			/* The alignment for the entity to which this
			   attribute applies. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    /* When kind == ak_mode. */
    a_type_mode_kind
		mode;
			/* The mode for the entity to which this
			   attribute applies. */
    /* When kind == ak_section. */
    char        *section;
			/* The section indicated for the entity to
			   which this attribute applies. */
    /* When kind == ak_alias. */
    char        *alias;
			/* The name of the entity for which this entity is an
			   alias. */
    /* When kind == ak_format. */
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
    /* When kind == ak_format_arg. */
    int         fmt_arg;
			/* The index (starting from 1) of the argument
			   that is a format string. */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    /* When kind == ak_visibility. */
    an_ELF_visibility_kind
		ELF_visibility;
			/* The visibility of an entity in an ELF object
			   file. */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    /* When kind == ak_init_priority. */
    a_gnu_init_priority
		init_priority;
			/* The initialization priority of a dynamically
			   initialized namespace-scope variable. */
  } variant;
  an_attribute_ptr
  		next;
			/* The next attribute in the list. */
} an_attribute;

extern an_attribute_ptr f_scan_attributes(a_token_sequence_number  *last_token,
                                          a_source_position        *end_pos);

#define scan_attributes()                                                  \
   f_scan_attributes((a_token_sequence_number*)NULL,                       \
                     (a_source_position*)NULL)

extern an_attribute_ptr copy_attribute_list(an_attribute_ptr attributes);

extern void free_attribute_list(an_attribute_ptr  attributes);

extern an_attribute_ptr *last_attribute_link(an_attribute_ptr *attributes);

extern a_type_ptr get_type_with_mode(a_type_ptr        type,
                                     a_type_mode_kind  mode,
                                     a_source_position *pos);

extern a_type_ptr apply_attributes_to_variable_type(
                                               an_attribute_ptr  attributes,
                                               a_type_ptr        type);

extern void apply_attributes_to_variable(an_attribute_ptr  attributes,
                                         a_variable_ptr    vp);

extern void apply_attributes_to_field(an_attribute_ptr  attributes,
                                      a_field_ptr       fp);

extern void apply_attributes_to_routine(an_attribute_ptr  attributes,
                                        a_routine_ptr     rp);

extern void apply_attributes_to_type(an_attribute_ptr attributes,
                                     a_type_ptr       tp,
                                     a_boolean        is_typedef);

extern void apply_attributes_to_typedef(an_attribute_ptr  attributes,
                                        a_type_ptr        tp,
                                        a_boolean         linkage_name);

extern void check_for_invalid_param_attributes(a_symbol_ptr     sym,
                                               an_attribute_ptr attributes);

extern void check_function_param_attributes(a_func_info_block_ptr func_info);

extern a_boolean check_transparent_union(a_type_ptr        tp,
                                         a_source_position *pos);

extern a_type_ptr copy_gnu_type_attributes(a_type_ptr  dst,
                                           a_type_ptr  src);

extern void attribute_one_time_init(void);

extern void attribute_init(void);

#endif /* GNU_EXTENSIONS_ALLOWED */

#endif /* ATTRIBUTE_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
