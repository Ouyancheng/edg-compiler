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
  ak_constructor,
  ak_destructor,
  ak_noreturn,
  ak_pure,
  ak_const,
  ak_weak,
  ak_section,
  ak_alias,
  ak_malloc,
  ak_nocommon,
  ak_transparent_union,
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
/* ak_error */          "error",
/* ak_mode */           "mode",
#if USER_CONTROL_OF_STRUCT_PACKING
/* ak_aligned */        "aligned",
/* ak_packed */         "packed",
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
/* ak_unused */	        "unused",
/* ak_constructor */    "constructor",
/* ak_destructor */     "destructor",
/* ak_noreturn */       "noreturn",
/* ak_pure */           "pure",
/* ak_const */          "const",
/* ak_weak */           "weak",
/* ak_section */        "section",
/* ak_alias */          "alias",
/* ak_malloc */         "malloc",
/* ak_nocommon */       "nocommon",
/* ak_transparent_union */
                        "transparent_union",
/* ak_last */           "last" /* used to check that initialization is
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
    /* When kind == ak_packed, ak_unused, ak_constructor, or
       ak_destructor, no variant fields. */
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
			/* The name of the entity for which this
			   entity is an alias. */
  } variant;
  an_attribute_ptr
  		next;	/* The next attribute in the list. */
} an_attribute;

extern an_attribute_ptr scan_attributes(void);

extern an_attribute_ptr copy_attribute_list(an_attribute_ptr attributes);

extern void free_attribute_list(an_attribute_ptr  attributes);

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

extern a_type_ptr apply_attributes_to_typedef(an_attribute_ptr  attributes,
                                              a_type_ptr        tp);

extern void copy_class_struct_or_union_definition(a_type_ptr to,
                                                  a_type_ptr from);

extern void check_for_invalid_param_attributes(a_symbol_ptr     sym,
                                               an_attribute_ptr attributes);

extern a_boolean check_transparent_union(a_type_ptr        tp,
                                         a_source_position *pos);

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
