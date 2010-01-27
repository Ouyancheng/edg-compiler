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


#if REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void redefine_extname_pragma(a_pending_pragma_ptr  ppp);
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
extern void process_alias_fixup_list(void);
extern unsigned long show_attribute_space_used(void);
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

extern a_type_ptr get_type_with_mode(a_type_ptr        type,
                                     a_type_mode_kind  mode,
                                     a_source_position *pos);

extern a_boolean check_transparent_union(a_type_ptr        tp,
                                         a_source_position *pos);

#endif /* GNU_EXTENSIONS_ALLOWED */

/*
Return TRUE if the given attribute is unrecognized or an empty attribute.
Such attributes cannot be "applied" to any entities.
*/
#define is_unapplicable_attr(ap)                                             \
  ((ap)->kind == (a_byte_attribute_kind)ak_unrecognized ||                   \
   (ap)->kind == (a_byte_attribute_kind)ak_empty_attr)

/*
Return TRUE if the given attribute is unrecognized.
*/
#define is_unrecognized_attr(ap)                                             \
  ((ap)->kind == (a_byte_attribute_kind)ak_unrecognized)

/*
Reclassify the given attribute as "unrecognized".
*/
#define make_attr_unrecognized(ap)                                           \
  { (ap)->kind = (a_byte_attribute_kind)ak_unrecognized; }

extern an_attribute_ptr scan_attributes(an_attribute_location  loc);

extern an_attribute_ptr scan_gnu_attribute_groups(an_attribute_location  loc);

extern void unscan_attributes(an_attribute_ptr  attributes);

EXTERN a_token_sequence_number
		last_token_number_of_attributes;
			/* After scanning a group of attributes, this variable
			   holds the token sequence number of the last token of
			   that attribute group (until the next attribute
			   group is scanned). */

EXTERN a_source_position
		end_position_of_attributes;
			/* After scanning a group of attributes, this variable
			   holds the position of the last token of that
			   attribute group (until the next attribute group is
			   scanned). */

extern void skip_over_attributes(void);

extern void report_bad_attribute_target(an_error_severity  sev,
                                        an_attribute_ptr   ap);

extern void attach_attributes(an_attribute_ptr  attributes,
                              char              *entity,
                              an_il_entry_kind  entity_kind);

extern void transform_type_with_attributes(a_type_ptr        *p_type,
                                           an_attribute_ptr  attributes);

extern a_type_ptr make_typeref_with_attributes(a_type_ptr        tp,
                                               an_attribute_ptr  attributes);

extern void attach_type_attributes(a_type_ptr        *p_type,
                                   an_attribute_ptr  attributes);

extern an_attribute_ptr *f_last_attribute_link(an_attribute_ptr  *attributes);

/*
A macro to access the last link of an attributes list.  The argument ap must
be a pointer to an attribute pointer.  If ap is non-NULL and *ap points to a
list of one or more attributes, return the address of the last "next" pointer
of that list.  Otherwise, return ap itself.
*/
#define last_attribute_link(ap)                                              \
  (/*lint --e(506)*/ ((ap) == NULL || *(ap) == NULL) ?                       \
                                         (ap) : f_last_attribute_link(ap))

extern an_attribute_ptr copy_of_attributes_list(an_attribute_ptr  attributes);

extern an_attribute_ptr copy_of_attributes_with_substitution(
                                             an_attribute_ptr      attributes,
                                             a_template_param_ptr  t_params,
                                             a_template_arg_ptr    t_args,
                                             a_ctws_options_set    options,
                                             a_boolean             *err);


/*
Return TRUE if the given attribute may produce a new type entry when applied
to an existing type entry.
*/
#define is_type_transforming_attribute(ap)  ((ap)->transforms_type_specifier)

/*
Return TRUE if the given attribute was applied directly to a class type or
enum type.
*/
#define is_tag_attribute(ap)                                                 \
  ((ap)->syntactic_location == (a_byte_attribute_location)al_tag_name ||     \
   (ap)->syntactic_location ==                                               \
                        (a_byte_attribute_location)al_post_tag_definition)


extern void mark_primary_decl_attributes(an_attribute_ptr  attributes);

extern an_attribute_ptr composite_attributes(an_attribute_ptr  ap1,
                                             an_attribute_ptr  ap2);

extern an_attribute_ptr get_param_variable_attr_copies(a_param_type_ptr  ptp);

extern void attribute_one_time_init(void);

extern void attribute_trans_unit_init(void);

extern void attribute_init(void);


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
