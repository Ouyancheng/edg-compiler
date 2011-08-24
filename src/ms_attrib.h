/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

ms_attrib.h -- Declarations related to ms_attrib.c (Microsoft attribute
               processing).

*/

/* Avoid including these declarations more than once: */
#ifndef MS_ATTRIB_H
#define MS_ATTRIB_H 1

#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Value that identifies the kind of entity to which a given attribute kind
applies.
*/
#define MSAT_NONE		0x0
			/* No target has been specified. */

#define MSAT_STANDALONE		0x1
			/* A standalone attribute (not associated with an
			   entity). */
#define MSAT_CLASS		0x2
			/* Applies to a class. */
#define MSAT_STRUCT		0x4
			/* Applies to a struct. */
#define MSAT_UNION		0x8
			/* Applies to a union. */
#define MSAT_INTERFACE		0x10
			/* Applies to a Microsoft interface. */
#define MSAT_METHOD		0x20
			/* Applies to a member function. */
#define MSAT_PARAMETER		0x40
			/* Applies to a function parameter. */
#define MSAT_DATA_MEMBER	0x80
			/* Applies to a class data member. */
#define MSAT_ROUTINE		0x100
			/* Applies to nonmember functions. */
#define MSAT_TYPEDEF		0x200
			/* Applies to a typedef. */
#define MSAT_ENUM		0x400
			/* Applies to an enum. */
#define MSAT_ANY_TYPE	(MSAT_CLASS | MSAT_STRUCT | MSAT_UNION | \
                         MSAT_TYPEDEF | MSAT_ENUM)
			/* Applies to any type. */
#define MSAT_ANY		0x800
			/* Can be used with any target or as a standalone
			   attribute. */

/*
Storage size used to represent a target bit set.
*/
typedef unsigned short
		an_ms_attribute_target;

/*
Entry used to represent a parameter description for a Microsoft attribute.
*/
typedef struct an_ms_attribute_param *an_ms_attribute_param_ptr;
typedef struct an_ms_attribute_param {
  an_ms_attribute_param_ptr
		next;	/* Next entry on a list of parameter entries, or NULL
			   for the last entry. */
  char		*name;	/* Name of the parameter.  Present even for "unnamed"
			   parameters for descriptive purposes in
			   diagnostics. */
  char		**values;
			/* When "kind" is msapk_enumeration, this is an array
			   of the acceptable values.  The last list entry is
			   NULL. */
  an_ms_attribute_arg_kind
		kind;	/* The kind of parameter (string, integer, etc. ). */
  a_byte_boolean
		is_unnamed;
			/* TRUE if the parameter is unnamed. */
} an_ms_attribute_param;


/*
Entry used to represent the definition of a particular kind of Microsoft
attribute.
*/
typedef struct an_ms_attribute_kind_descr *an_ms_attribute_kind_descr_ptr;
typedef struct an_ms_attribute_kind_descr {
  an_ms_attribute_kind
		kind;
			/* The kind of attribute that this represents. */
  an_ms_attribute_target
		target;
			/* Identifies the kind of entity to which this
			   attribute may apply. */
  a_byte_boolean
		initialization_style_arg_allowed;
			/* TRUE if this attribute supports the "attr=1" form
			   of argument list. */
  int		num_params;
			/* The number of parameters the attribute has. */
  sizeof_t	name_length;
			/* The length of the name, not including the null
			   terminator. */
  char		*name;
			/* The name of the attribute.  Null terminated. */
  an_ms_attribute_kind_descr_ptr
		next;	/* Pointer to the next entry in a given hash table
			   bucket of attribute kind description entries. */
  an_ms_attribute_param_ptr
		parameters;
			/* A linked list of attribute parameter entries. */
  an_ms_attribute_param_ptr
		parameters_tail;
			/* Pointer to the last entry on the list of
			   parameters. */
} an_ms_attribute_kind_descr;


extern void skip_microsoft_attribute_tokens(void);

extern an_ms_attribute_ptr scan_microsoft_attributes(a_boolean	is_parameter);

extern
void apply_microsoft_attributes(an_ms_attribute_ptr	*attributes,
				char			*entity,
				an_il_entry_kind	kind,
				an_ms_attribute_target	target);

extern void verify_standalone_attributes(an_ms_attribute_ptr	*attributes);

extern void dispose_of_unapplied_attributes(an_ms_attribute_ptr	*attributes,
					    an_error_code	error_code);

extern
an_ms_attribute_ptr duplicate_ms_attributes(an_ms_attribute_ptr  orig,
                                            char                 *new_entity);

extern an_ms_attribute_ptr  find_ms_attribute_for_entity(
                                            an_ms_attribute_ptr          msap,
                                            a_source_correspondence_ptr  scp);

extern void ms_attrib_one_time_init(void);

extern void ms_attrib_init(void);

#if DEBUG
extern void db_microsoft_attribute(an_ms_attribute_ptr	msap);
extern unsigned long db_show_ms_attrib_space_used(unsigned long grand_total);
#endif /* DEBUG */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#endif /* ifndef MS_ATTRIB_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
