/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

ms_attrib.c -- Microsoft attribute processing.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"


#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if MICROSOFT_EXTENSIONS_ALLOWED

#include "ms_attrib.h"


#if DEBUG
/*
Counts of tables allocated, to track total use of memory.
*/
static unsigned long
		num_ms_attribute_kind_descrs_allocated,
		num_ms_attribute_params_allocated;
#endif /* DEBUG */

static a_text_buffer_ptr
		ms_attr_buffer;
			/* A text buffer used to create temporary copies of
			   attribute argument values. */

static a_boolean
		scan_misc_attributes_as_unrecognized;
			/* TRUE if most recognized attributes should be scanned
			   as "unrecognized" attributes.  This means that the
			   only a string representation is recorded.  The
			   individual arguments are not scanned or checked.
			   No checking is done when the attributes are
			   applied.  This affects msak_misc attributes (so
			   some attributes, such as uuid, will still be
			   processed normally). */

static an_ms_attribute_kind_descr_ptr
		unrecognized_attribute;
			/* A special attribute kind entry that represents
			   an unrecognized attribute.  This is used even when
			   unrecognized attributes are not accepted, for
			   error recovery purposes. */

static an_ms_attribute_kind_descr_ptr
		curr_attribute_descr;
			/* Pointer to an attribute description that is in the
			   process of being defined.  This is set by
			   make_attribute_description and is used by
			   add_attribute_parameter. */

static a_token_cache
		attribute_cache;
			/* Token cache containing the tokens of the current
			   attribute block. */

#define ATTRIBUTE_LOOKUP_TABLE_SIZE 61
			/* The number of buckets in the template lookup table.
			   This number should be prime. */

static an_ms_attribute_kind_descr_ptr
		attribute_lookup_table[ATTRIBUTE_LOOKUP_TABLE_SIZE];
			/* Table used to determine the attribute kind for a
			   given attribute name.  Each element of the
			   array points to a list of entries for attributes
			   that hash to a given group. */

#define HASH_FACTOR ((unsigned int)73)
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from a name
                           string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */

static unsigned long hash_attribute_name(a_const_char	*name,
					 sizeof_t	length)
/*
Generate a hash value for the specified name.
*/
{
  unsigned long	hash_value = 0;
  unsigned char	*ptr;

  /* Hash the name.  This involves taking the name's first 3, last 3, and
     middle 3 characters.  Of course, if the identifier has 9 or fewer
     characters, take the entire identifier. */
  ptr = (unsigned char *)name;
  if (length > 9) {
    hash_value = *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + (length >> 1) - 1;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
    ptr = (unsigned char *)name + length - 3;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    hash_value = (hash_value * HASH_FACTOR) + *ptr;
  } else {
    unsigned int i;
    for (i = 0; i < length; i++) {
      hash_value = (hash_value * HASH_FACTOR) + *ptr++;
    }  /* for */
  }  /* if */
  return hash_value;
}  /* hash_attribute_name */


static void add_attribute_lookup_table_entry(
					an_ms_attribute_kind_descr_ptr	msakdp,
					a_const_char			*name)
/*
Add "msakdp" to the attribute lookup table.  "name" is the name of the
attribute.
*/
{
  int		bucket;
  sizeof_t	length = strlen(name);

  bucket = hash_attribute_name(name, length) % ATTRIBUTE_LOOKUP_TABLE_SIZE;
  msakdp->next = attribute_lookup_table[bucket];
  msakdp->name = copy_string_to_region(file_scope_region_number, name);
  msakdp->name_length = length;
  attribute_lookup_table[bucket] = msakdp;
}  /* add_attribute_lookup_table_entry  */


static an_ms_attribute_kind_descr_ptr find_attribute_kind(a_const_char	*name,
							  sizeof_t	length)
/*
Look up an attribute named "name" (with a length of "length") in the attribute
lookup table.  If found, return a pointer to the kind description entry,
otherwise return NULL.
*/
{
  int					bucket;
  an_ms_attribute_kind_descr_ptr	msakdp;

  bucket = hash_attribute_name(name, length) % ATTRIBUTE_LOOKUP_TABLE_SIZE;
  for (msakdp = attribute_lookup_table[bucket];
       msakdp != NULL; msakdp = msakdp->next) {
    if (msakdp->name_length == length &&
        strncmp(msakdp->name, name, size_t_arg(length)) == 0) {
      break;
    }  /* if */
  }  /* for */
  return msakdp;
}  /* find_attribute_kind */


static an_ms_attribute_kind_descr_ptr alloc_ms_attribute_kind_descr(void)
/*
Allocate an attribute kind description entry, initialize its fields, and
return a pointer to the entry.
*/
{
  an_ms_attribute_kind_descr_ptr msakdp;

  msakdp = alloc_fe_of_type(an_ms_attribute_kind_descr);
#if DEBUG
  num_ms_attribute_kind_descrs_allocated++;
#endif /* DEBUG */
  msakdp->kind = (an_ms_attribute_kind)msak_none;
  msakdp->target = MSAT_NONE;
  msakdp->name = NULL;
  msakdp->name_length = 0;
  msakdp->initialization_style_arg_allowed = FALSE;
  msakdp->next = NULL;
  msakdp->num_params = 0;
  msakdp->parameters = NULL;
  msakdp->parameters_tail = NULL;
  return msakdp;
}  /* alloc_ms_attribute_kind_descr */


static void make_attribute_description(an_ms_attribute_kind	kind,
				       a_const_char		*name,
				       an_ms_attribute_target	target)
/*
Create an attribute kind description entry for the specified attribute
kind, with a name of "name".  If the attribute has a name, enter it into
the attribute lookup table.  This routine sets a static variable to the
entry most recently added.  This is used by add_attribute_parameter, etc.
when specifying the parameters associated with an attribute.
*/
{
  an_ms_attribute_kind_descr_ptr	msakdp;

  msakdp = alloc_ms_attribute_kind_descr();
  if (scan_misc_attributes_as_unrecognized &&
      kind == (an_ms_attribute_kind)msak_misc) {
    /* When scanning miscellaneous attributes as unrecognized, override
       the specified kind and use the unrecognized kind instead. */
    msakdp->kind = (an_ms_attribute_kind)msak_unrecognized;
    msakdp->target = MSAT_ANY;
  } else {
    msakdp->kind = kind;
    msakdp->target = target;
  }  /* if */
  if (name != NULL) {
    /* If this is not an unnamed attribute, create a lookup table entry. */
    add_attribute_lookup_table_entry(msakdp, name);
  }  /* if */
  curr_attribute_descr = msakdp;
}  /* make_attribute_description */

#if RECOGNIZE_MICROSOFT_ATTRIBUTES || INCLUDE_EDG_TEST_ATTRIBUTES

static an_ms_attribute_param_ptr alloc_ms_attribute_param(void)
/*
Allocate an attribute parameter entry, initialize its fields, and
return a pointer to the entry.
*/
{
  an_ms_attribute_param_ptr msapp;

  msapp = alloc_fe_of_type(an_ms_attribute_param);
#if DEBUG
  num_ms_attribute_params_allocated++;
#endif /* DEBUG */
  msapp->next = NULL;
  msapp->name = NULL;
  msapp->values = NULL;
  msapp->kind = (an_ms_attribute_kind)msaak_none;
  msapp->is_unnamed = FALSE;
  return msapp;
}  /* alloc_ms_attribute_param */


static void add_attribute_parameter(an_ms_attribute_arg_kind	kind,
				    a_const_char		*name,
				    a_boolean			is_unnamed,
				    a_const_char		*values)
/*
Add the specified parameter to the list of parameter accepted by the
attribute most recently added by make_attribute_description.

"kind" indicates the type of parameter (string, integer, etc.), "name"
is the parameter name.  A name is specified even if "is_unnamed" is TRUE
so that a name is available, if needed, for diagnostic purposes.  For
msaak_enumeration parameters, "values" is a comma-separated list of the
acceptable values.  The list must not contain any null names.  The value
names must be in lower case, as the arguments will be converted to lower
case.
*/
{
  an_ms_attribute_param_ptr	msapp;

  msapp = alloc_ms_attribute_param();
  msapp->kind = kind;
  msapp->name = copy_string_to_region(file_scope_region_number, name);
  msapp->is_unnamed = is_unnamed;
  /* Link this parameter entry into the list of parameters. */
  if (curr_attribute_descr->parameters == NULL) {
    curr_attribute_descr->parameters = msapp;
  } else {
    curr_attribute_descr->parameters_tail->next = msapp;
  }  /* if */
  curr_attribute_descr->parameters_tail = msapp;
  if (kind == (an_ms_attribute_kind)msaak_enumeration && values != NULL) {
    /* Convert the specified list of values from a comma-separated list into
       an array of acceptable values. */
    int          num_elements = 1;
    int          element;
    a_const_char *ptr;
    char         **list;
    /* Find the number of elements. */
    for (ptr = values; *ptr != '\0'; ptr++) {
      if (*ptr == ',') num_elements++;
    }  /* for */
    /* Allocate an array for the list.  An extra element is allocated for a
       NULL terminating element. */
    list = (char**)alloc_fe((sizeof_t)(sizeof(char*) * (num_elements + 1)));
    /* Set the terminating element to NULL. */
    list[num_elements] = NULL;
    for (element = 0, ptr = values; element < num_elements; ++element) {
      /* Find the end of this element. */
      a_const_char *end = strchr(ptr, ',');
      sizeof_t     length;
      char         *entry;
      /* If this is the last element, find the end of the string. */
      if (end == NULL) end = &ptr[strlen(ptr)];
      length = end - ptr;
      check_assertion_str2(length > 0, "add_attribute_parameter:",
                           "empty enumeration names not allowed");
      entry = copy_string_of_length_to_region(FRONT_END_REGION_NUMBER,
                                              ptr, length);
      list[element] = entry;
      /* Advance to the next entry. */
      ptr = end + 1;
    }  /* for */
    msapp->values = list;
  }  /* if */
}  /* add_attribute_parameter */

#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES || INCLUDE_EDG_TEST_ATTRIBUTES */
#if RECOGNIZE_MICROSOFT_ATTRIBUTES

static void set_initialization_style_arg_allowed(void)
/*
Set the initialization-style argument allowed flag for the current attribute.
*/
{
  curr_attribute_descr->initialization_style_arg_allowed = TRUE;
}  /* set_initialization_style_arg_allowed */

#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES */

static void init_attribute_kinds(void)
/*
Create the data structures used to describe the kinds of attributes that
are accepted.
*/
{
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "<unrecognized>", MSAT_ANY);
  /* Save a pointer to the special "unrecognized" attribute kind. */
  unrecognized_attribute = curr_attribute_descr;
#if RECOGNIZE_MICROSOFT_ATTRIBUTES
  /* [aggregatable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "aggregatable", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "value", /*is_unnamed=*/FALSE,
                          "never,allowed,always");
  /* [aggregates]
     The Microsoft documentation describes a second parameter named
     "variable_name", but this does not actually seem to be accepted
     by the Microsoft compiler (7.1). */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "aggregates", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "clsid", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [appobject] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "appobject", MSAT_CLASS | MSAT_STRUCT);
  /* [as_expression] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "as_expression", MSAT_ANY);
  /* [as_string] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "as_string", MSAT_ANY);
  /* [async_uuid] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "async_uuid", MSAT_INTERFACE);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [attribute] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "attribute", MSAT_ANY);
  /* [bindable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "bindable", MSAT_METHOD);
  /* [call_as] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "call_as", MSAT_METHOD | MSAT_ROUTINE);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "function", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [case] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "case", MSAT_DATA_MEMBER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [coclass] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "coclass", MSAT_CLASS | MSAT_STRUCT);
  /* [com_interface_entry] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "com_interface_entry", MSAT_CLASS | MSAT_STRUCT);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "entry",
                          /*is_unnamed=*/TRUE,
                          (char*)NULL);
  /* [control] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "control", MSAT_CLASS | MSAT_STRUCT);
  /* [cpp_quote] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "cpp_quote", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "statement", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [custom] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "custom", MSAT_ANY);
  /* [db_accessor] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_accessor", MSAT_ANY);
  /* [db_column] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_column", MSAT_ANY);
  /* [db_command] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_command", MSAT_ANY);
  /* [db_param] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_param", MSAT_ANY);
  /* [db_source] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_source", MSAT_ANY);
  /* [db_table] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "db_table", MSAT_ANY);
  /* [default] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "default",
                             MSAT_CLASS | MSAT_STRUCT | MSAT_DATA_MEMBER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interface1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interface2", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [defaultbind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "defaultbind", MSAT_METHOD);
  /* [defaultcollelem] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "defaultcollelem", MSAT_METHOD);
  /* [default_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "default_value", MSAT_ANY);
  /* [defaultvalue] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "defaultvalue", MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [defaultvtable] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "defaultvtable", MSAT_ANY);
  /* [dispinterface] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "dispinterface", MSAT_INTERFACE);
  /* [displaybind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "displaybind", MSAT_METHOD);
  /* [dual] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "dual", MSAT_INTERFACE);
  /* [emitidl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "emitidl", MSAT_STANDALONE);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "mode",
                          /*is_unnamed=*/TRUE,
                          "false,true,restricted,forced");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "defaultimports",
                          /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [entry] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "entry", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [event_receiver] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "event_receiver", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/TRUE,
                          "native,com,managed");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "layout_dependent", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [event_source] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "event_source", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/TRUE,
                          "native,com,managed");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "optimize", /*is_unnamed=*/FALSE,
                          "speed,size");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "decorate", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [export] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "export",
                             MSAT_INTERFACE | MSAT_STRUCT | MSAT_UNION |
                             MSAT_ENUM | MSAT_TYPEDEF);
  /* [first_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "first_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpcontext] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "helpcontext",
                             MSAT_INTERFACE | MSAT_CLASS | MSAT_TYPEDEF |
                             MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpfile] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "helpfile",
                             MSAT_INTERFACE | MSAT_CLASS | MSAT_TYPEDEF |
                             MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "filename", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [help_string] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "help_string", MSAT_ANY);
  /* [helpstring] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "helpstring",
                             MSAT_INTERFACE | MSAT_CLASS | MSAT_TYPEDEF |
                             MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "string", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpstringcontext] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "helpstringcontext",
                             MSAT_INTERFACE | MSAT_CLASS | MSAT_TYPEDEF |
                             MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "contextID", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [helpstringdll] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "helpstringdll",
                             MSAT_INTERFACE | MSAT_CLASS | MSAT_TYPEDEF |
                             MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "string", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [hidden] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "hidden",
                              MSAT_METHOD | MSAT_INTERFACE |
                              MSAT_CLASS | MSAT_STRUCT);
  /* [hook] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "hook", MSAT_ANY);
  /* [id] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "id", MSAT_METHOD);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "id", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [idl_module] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "idl_module", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "dllname", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpstringcontext", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpcontext", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpfile", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "hidden", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "restricted", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [idl_quote] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "idl_quote", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "text", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [iid_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "iid_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [immediatebind] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "immediatebind", MSAT_METHOD);
  /* [implements] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "implements", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "interfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "dispinterfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [implements_category] */
  /* This is documented as taking a UUID, but this does not seem to be
     the case. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "implements_category", MSAT_CLASS | MSAT_STRUCT);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "implements_category", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [import] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "import", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "idl_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [importidl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "importidl", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "idl_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [importlib] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "importlib", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "tlb_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [in] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "in", MSAT_PARAMETER);
  /* [include] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "include", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "header_file", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [includelib] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "includelib", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name.idl", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [last_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "last_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [lcid] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "lcid", MSAT_PARAMETER);
  /* [length_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "length_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [library_block] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "library_block", MSAT_ANY);
  /* [licensed] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "licensed", MSAT_CLASS | MSAT_STRUCT);
  /* [local] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "local", MSAT_METHOD | MSAT_INTERFACE);
  /* [max_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "max_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [module] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "module", MSAT_STANDALONE | MSAT_CLASS);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "type", /*is_unnamed=*/FALSE, "dll,exe,service");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "version", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "lcid", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "control", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstringdll", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpfile", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpcontext", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "helpstringcontext", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "hidden", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                         "restricted", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                         "custom", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                         "resource_name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [ms_union] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "ms_union",
                             MSAT_CLASS | MSAT_STRUCT | MSAT_INTERFACE);
  /* [multi_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "multi_value", MSAT_ANY);
  /* [no_injected_text] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "no_injected_text", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [nonbrowsable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "nonbrowsable", MSAT_METHOD);
  /* [noncreatable] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "noncreatable", MSAT_CLASS | MSAT_STRUCT);
  /* [nonextensible] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "nonextensible", MSAT_INTERFACE);
  /* [notify_atlprov] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "notify_atlprov", MSAT_ANY);
  /* [odl] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "odl", MSAT_INTERFACE);
  /* [object] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "object", MSAT_INTERFACE);
  /* [oleautomation] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "oleautomation", MSAT_INTERFACE);
  /* [optional] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "optional", MSAT_ANY);
  /* [out] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "out", MSAT_PARAMETER);
  /* [perfmon] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "perfmon", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "register", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [perf_counter] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "perf_counter", MSAT_DATA_MEMBER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namestring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "name_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "help_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "countertype_string", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "defscale", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "default_counter", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "detail", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [perf_object] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "perf_object", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "name_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "help_res", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namestring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "helpstring", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "detail", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "no_instances", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "class", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "maxinstnamelen", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [pointer_default] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "pointer_default", MSAT_INTERFACE);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [pragma] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "pragma", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "pragma_statement", /*is_unnamed=*/TRUE,
                          (char*)NULL);
  /* [process_early] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "process_early", MSAT_ANY);
  /* [progid] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "progid", MSAT_CLASS | MSAT_STRUCT);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [ptr] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "ptr",
                              MSAT_PARAMETER | MSAT_METHOD |
                              MSAT_ROUTINE | MSAT_TYPEDEF);
  /* [propget] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "propget", MSAT_METHOD | MSAT_ROUTINE);
  /* [propput] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "propput", MSAT_METHOD | MSAT_ROUTINE);
  /* [propputref] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "propputref", MSAT_METHOD | MSAT_ROUTINE);
  /* [provider] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "provider", MSAT_STANDALONE);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [public] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "public", MSAT_TYPEDEF);
  /* [range] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "range", MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "low", /*is_unnamed=*/TRUE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "high", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [rdx] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "rdx", MSAT_DATA_MEMBER);
  /* [readonly] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "readonly", MSAT_METHOD);
  /* [ref] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "ref",
                              MSAT_PARAMETER | MSAT_METHOD |
                              MSAT_ROUTINE | MSAT_TYPEDEF);
  /* [registration_script] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "registration_script", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "script", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [repeatable] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "repeatable", MSAT_ANY);
  /* [requestedit] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "requestedit", MSAT_METHOD);
  /* [request_handler] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "request_handler", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "sdl", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [requires_category] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "requires_category", MSAT_CLASS | MSAT_STRUCT);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "requires_category", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [requires_value] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "requires_value", MSAT_ANY);
  /* [restricted] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "restricted",
                              MSAT_METHOD | MSAT_INTERFACE |
                              MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "interfaces", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [retval] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "retval", MSAT_PARAMETER);
  /* [satype] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "satype", MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "data_type", /*is_unnamed=*/FALSE, (char*)NULL);
  /* Source Annotation attributes. */
  /* [FormatString] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "FormatString", MSAT_ANY);
  }  /* if */
  /* [SA_FormatString] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_FormatString", MSAT_ANY);
  /* [InvalidCheck] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "InvalidCheck", MSAT_ANY);
  }  /* if */
  /* [SA_InvalidCheck] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_InvalidCheck", MSAT_ANY);
  /* [Post] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Post", MSAT_ANY);
  }  /* if */
  /* [SA_Post] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Post", MSAT_ANY);
  /* [Pre] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Pre", MSAT_ANY);
  }  /* if */
  /* [SA_Pre] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Pre", MSAT_ANY);
  /* [PostBound] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PostBound", MSAT_ANY);
  }  /* if */
  /* [SA_PostBound] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PostBound", MSAT_ANY);
  /* [PostRange] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PostRange", MSAT_ANY);
  }  /* if */
  /* [SA_PostRange] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PostRange", MSAT_ANY);
  /* [PreBound] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PreBound", MSAT_ANY);
  }  /* if */
  /* [SA_PreBound] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PreBound", MSAT_ANY);
  /* [SA_PreRange] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_PreRange", MSAT_ANY);
  /* [PreRange] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "PreRange", MSAT_ANY);
  }  /* if */
  /* [Success] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                               "Success", MSAT_ANY);
  }  /* if */
  /* [SA_Success] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "SA_Success", MSAT_ANY);
  /* [source_annotation_attribute] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "source_annotation_attribute", MSAT_ANY);
  /* [size_is] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "size_is",
                             MSAT_DATA_MEMBER | MSAT_METHOD | MSAT_PARAMETER);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "value", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [soap_handler] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "soap_handler", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "namespace", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "protocol", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "style", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "use", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_header] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "soap_header", MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "value", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "required", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "in", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "out", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_method] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "soap_method", MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [soap_namespace] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "soap_namespace", MSAT_ANY);
  /* [source] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "source",
                             MSAT_CLASS | MSAT_STRUCT | MSAT_INTERFACE);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "interfaces", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [string] */
  /* The Microsoft documentation does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "string", MSAT_ANY);
  /* [support_error_info] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "support_error_info", MSAT_CLASS);
  /* The documentation describes this parameter as a UUID, but it appears
     to actually be a string. */
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "error_interface", /*is_unnamed=*/FALSE,
                          (char*)NULL);
  /* [switch_type] */
  /* The Microsoft documentation does not describe the field argument and
     does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "switch_type", MSAT_UNION);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "field", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [switch_is] */
  /* The Microsoft documentation does not describe the type argument and
     does not have the correct target. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "switch_is", MSAT_UNION);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "type", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [synchronize] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "synchronize", MSAT_METHOD | MSAT_ROUTINE);
  /* [tag_name] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "tag_name", MSAT_METHOD);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "parse_func", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [threading] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "threading", MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "model", /*is_unnamed=*/FALSE,
                          "apartment,neutral,single,free,both");
  /* [transmit_as] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "transmit_as", MSAT_TYPEDEF);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "type", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [uidefault] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "uidefault", MSAT_METHOD);
  /* [unhook] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "unhook", MSAT_ANY);
  /* [unique] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "unique",
                              MSAT_PARAMETER | MSAT_METHOD |
                              MSAT_ROUTINE | MSAT_TYPEDEF);
  /* [usage] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "usage", MSAT_ANY);
  /* [usesgetlasterror] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "usesgetlasterror", MSAT_ANY);
  /* [uuid] */
  if (!C_mode()) {
    make_attribute_description((an_ms_attribute_kind)msak_uuid,
                               "uuid",
                               MSAT_CLASS | MSAT_STRUCT | MSAT_INTERFACE);
    set_initialization_style_arg_allowed();
    add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                            "uuid", /*is_unnamed=*/FALSE, (char*)NULL);
  }  /* if */
  /* [v1_alttype] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_alttype", MSAT_ANY);
  /* [v1_early] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_early", MSAT_ANY);
  /* [v1_enum] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "v1_enum", MSAT_ENUM);
  /* [v1_name] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             "v1_name", MSAT_ANY);
  /* [vararg] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "vararg", MSAT_METHOD);
  /* [version] */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "version",
                             MSAT_CLASS | MSAT_STRUCT);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "version", /*is_unnamed=*/TRUE, (char*)NULL);
  /* [vi_progid] */
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
			     "vi_progid", MSAT_CLASS | MSAT_STRUCT);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "name", /*is_unnamed=*/FALSE, (char*)NULL);
  /* [wire_marshal] */
  /* The Microsoft documentation does not describe the type argument. */
  make_attribute_description((an_ms_attribute_kind)msak_misc,
			     "wire_marshal", MSAT_TYPEDEF);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_other,
                          "type", /*is_unnamed=*/FALSE, (char*)NULL);
#endif /* RECOGNIZE_MICROSOFT_ATTRIBUTES */
#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* These are special attributes included for testing purposes. */
  /* [edg_test_1] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
			     "edg_test_1", MSAT_STANDALONE);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "arg1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "arg2", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "arg3", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "arg4", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "arg5", /*is_unnamed=*/FALSE, "a,b,c,aa,bb,cc");
  /* [edg_test_2] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
			     "edg_test_2", MSAT_STANDALONE);
  /* [edg_test_3] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
			     "edg_test_3", MSAT_ANY);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "arg1", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "arg2", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "arg3", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "arg4", /*is_unnamed=*/FALSE, (char*)NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "arg5", /*is_unnamed=*/FALSE, "a,b,c,aa,bb,cc");
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
}  /* init_attribute_kinds */


static void cache_attribute_block(void)
/*
We are at the opening "[" of a Microsoft attribute block.  The block contains
one or more attributes and is terminated by a closing "]".  Create a token
cache containing all of the tokens of the attribute block.
*/
{
  a_token_set_array	stop_tokens;
  a_boolean		save_expand_macros;
  a_boolean		save_do_string_literal_concatenation;
  a_boolean		save_fetch_pp_tokens;
  a_boolean		save_in_microsoft_attribute;
  a_boolean		save_suppress_keyword_recognition;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  save_suppress_keyword_recognition = suppress_keyword_recognition;
  save_in_microsoft_attribute = in_microsoft_attribute;
  /* Set the values required for attribute scanning. */
  expand_macros = TRUE;
  do_string_literal_concatenation = TRUE;
  fetch_pp_tokens = FALSE;
  suppress_keyword_recognition = TRUE;
  in_microsoft_attribute = TRUE;
  clear_token_cache(&attribute_cache, /*reusable=*/TRUE);
  /* Cache the current token and advance past it. */
  cache_curr_token(&attribute_cache);
  (void)get_token();
  /* Initialize a local stop token set. */
  clear_token_set_array(stop_tokens);
  /* Cache all of the tokens up to the closing "]".  Also stop on a right
     brace for error recovery purposes. */
  incr_token_set_array_element(stop_tokens, tok_rbracket);
  incr_token_set_array_element(stop_tokens, tok_rbrace);
  cache_token_stream(&attribute_cache, stop_tokens);
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(&attribute_cache);
  rescan_copy_of_cache(&attribute_cache);
  /* Restore the previous values. */
  expand_macros = save_expand_macros;
  do_string_literal_concatenation = save_do_string_literal_concatenation;
  fetch_pp_tokens = save_fetch_pp_tokens;
  suppress_keyword_recognition = save_suppress_keyword_recognition;
  in_microsoft_attribute = save_in_microsoft_attribute;
}  /* cache_attribute_block */


static an_ms_attribute_kind_descr_ptr look_up_attribute(void)
/*
Look up the identifier that names the attribute to be processed. 
*/
{
  an_ms_attribute_kind_descr_ptr	attr_descr = NULL;

  if (curr_token != tok_identifier) {
    /* An identifier that names the attribute was expected. */
    pos_error(ec_exp_attribute_name, &pos_curr_token);
    /* Discard the rest of this attribute. */
    flush_tokens();
  } else {
    a_symbol_header_ptr	sym_hdr = locator_for_curr_id.symbol_header;
    attr_descr = find_attribute_kind(sym_hdr->identifier,
                                     (sizeof_t)strlen(sym_hdr->identifier));
    if (attr_descr == NULL) {
      /* An unknown attribute -- issue a diagnostic. */
      pos_warning(ec_unrecognized_ms_attr, &pos_curr_token);
    }  /* if */
    /* Bypass the identifier. */
    (void)get_token();
  }  /* if */
  if (attr_descr == NULL) attr_descr = unrecognized_attribute;
  return attr_descr;
}  /* look_up_attribute */


static char *get_string_value_for_token(a_boolean	*err)
/*
If the current token is an identifier or string literal, this routine
copies the characters of the token static buffer and converts them to lower
case (for a string literal, the quotes are not part of the string).  If the
string contains a null character, any characters after the null are discarded.
A pointer to the static buffer is returned.  If the current token is something
other than an identifier or string literal, a NULL pointer is returned.  If
the token is a string literal, but the constant is an error constant, "err"
is set to TRUE.  Note that "err" is not TRUE for an unexpected token kind.
*/
{
  a_const_char	    *src = NULL;
  a_boolean	    valid_token = TRUE;
  char		    *result = NULL;
  a_character_kind  char_kind = (a_character_kind)chk_char;
  a_targ_size_t	    len, pos, char_size = 1;

  *err = FALSE;
  /* Copy the characters into a buffer, converting any upper case characters
     to lower case.  Allocate a buffer on the first call of this routine. */
  if (ms_attr_buffer == NULL) ms_attr_buffer = alloc_text_buffer(32);
  reset_text_buffer(ms_attr_buffer);
  /* The source of the characters to be copied depends on the kind of token
     provided. */
  if (curr_token == tok_identifier) {
    /* An identifier.  Use the characters of the identifier. */
    src = locator_for_curr_id.symbol_header->identifier;
    len = (a_targ_size_t)strlen(src);
  } else if (curr_token == tok_string_literal) {
    if (is_error_constant(&const_for_curr_token)) {
      /* We encountered a misformed string literal.  An error should
         have been issued already. */
      check_assertion(total_errors != 0);
      *err = TRUE;
    } else {
      src = const_for_curr_token.variant.string.value;
      /* Determine if the source constant is a wide string literal. */
      char_kind = const_for_curr_token.character_kind;
      char_size = character_size[char_kind];
      /* Subtract one character to ignore the null terminator. */
      len = const_for_curr_token.variant.string.length - char_size;
    }  /* if */
  } else if (is_keyword_token(curr_token)) {
    /* A token initially cached as a keyword that should be treated as an
       identifier. */
    src = token_names[(int)curr_token];
    len = (a_targ_size_t)strlen(src);
  } else {
    /* Some other token kind */
    valid_token = FALSE;
  }  /* if */
  /* Copy the token to the buffer. */
  if (src != NULL) {
    /* Copy the characters to the text buffer, converting them to lower
       case. */
    for (pos = 0; pos < len; src += char_size, pos += char_size) {
      char	ch;
      if (char_kind == (a_character_kind)chk_char) {
        ch = *src;
      } else {
        /* A wide literal.  Extract the character value.  Values out of
           range are truncated.  As this routine is used for strings with
           expected values, this should result in an error later. */
        ch = (char)extract_character_from_string(src,
                                                 (unsigned int)char_size);
      }  /* if */
      if (is_id_char[ch-CHAR_MIN]) ch = tolower((int)ch);
      add_char_to_text_buffer(ms_attr_buffer, ch);
    }  /* for */
    /* Add a null terminator. */
    add_char_to_text_buffer(ms_attr_buffer, '\0');
    result = ms_attr_buffer->buffer;
  }  /* if */
  /* For a valid token kind, bypass the token. */
  if (valid_token) (void)get_token();
  return result;
}  /* get_string_value_for_token */


static a_constant_ptr get_string_constant_for_token(a_boolean	*err)
/*
If the current token is an identifier or string literal, this routine
returns a constant that represents the string literal or the identifier
name converted to a string literal constant.  A string literal constant
is returned as scanned by the lexical routines, and so may contain
embedded null characters.  If the current token is something other than
an identifier or string literal, a NULL pointer is returned.  If the token
is a string literal, but the constant is an error constant, "err" is set to
TRUE.  Note that "err" is not TRUE for an unexpected token kind.
*/
{
  a_constant_ptr	result = NULL;
  a_boolean		valid_token = TRUE;
  a_constant		constant;

  *err = FALSE;
  if (curr_token == tok_string_literal) {
    if (is_error_constant(&const_for_curr_token)) {
      /* We encountered a misformed string literal.  An error should
         have been issued already. */
      check_assertion(total_errors != 0);
      *err = TRUE;
      set_error_constant(&constant);
      result = &constant;
    } else {
      result = &const_for_curr_token;
    }  /* if */
  } else if (curr_token == tok_identifier || is_keyword_token(curr_token)) {
    /* Convert the identifier into a string constant.  If a token was
       initially scanned as a keyword, treat is as an identifier with the
       name of the keyword. */
    sizeof_t     length;
    a_const_char *str;
    if (curr_token == tok_identifier) {
      str = locator_for_curr_id.symbol_header->identifier;
    } else {
      str = token_names[(int)curr_token];
    }  /* if */
    /* Add space for the null terminator. */
    length = strlen(str) + 1;
    clear_constant(&constant, (a_constant_repr_kind)ck_string);
    constant.type = string_type((a_targ_size_t)length);
    constant.variant.string.length = (a_targ_size_t)length;
    constant.variant.string.value =
                          copy_string_to_region(file_scope_region_number, str);
    result = &constant;
  } else {
    /* Some other token kind */
    valid_token = FALSE;
  }  /* if */
  /* For a valid token kind, bypass the token. */
  if (valid_token) (void)get_token();
  /* Get a shared version of the result constant. */
  if (result != NULL) result = alloc_shareable_constant(result);
  return result;
}  /* get_string_consant_for_token */


static long scan_ms_attribute_integer_arg(void)
/*
Scan an integer argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_host_large_integer	value = 0;
  a_constant		constant;

  /* The argument can be an expression, but must be constant. */
  scan_integral_constant_expression(&constant);
  if (!is_error_constant(&constant)) {
    a_boolean	err;
    value = value_of_integer_constant(&constant, &err);
    if (err || value > LONG_MAX || value < LONG_MIN) { /*lint !e685*/
      /* Attribute values should be small integers.  Issue an error on an
         attempt to use a very large integer. */
      error(ec_integer_too_large);
    }  /* if */
  }  /* if */
  return (long)value;
}  /* scan_ms_attribute_integer_arg */


static a_constant_ptr scan_ms_attribute_string_arg(void)
/*
Scan a string argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_boolean		err;
  a_constant_ptr	constant;

  constant = get_string_constant_for_token(&err);
  if (constant == NULL && !err) {
    /* The current token was not of an expected kind. */
    syntax_error(ec_exp_string_literal);
    constant = alloc_error_constant();
  }  /* if */
  return constant;
}  /* scan_ms_attribute_string_arg */


static char *scan_ms_attribute_other_arg(void)
/*
Scan an "other" argument of a Microsoft attribute.  Return the value
scanned.  The "other" argument kind is used for arguments that can
be arbitrary tokens.  The tokens until the argument terminator are
converted into a string.
*/
{
  char					*value = NULL;
  a_token_sequence_number		first_token;
  a_token_sequence_number		last_token;
  a_source_position			start_position;

  /* Save the token sequence number of the first token of the argument. */
  first_token = curr_token_sequence_number;
  start_position = pos_curr_token;
  flush_tokens_without_warning();
  /* Save the position of the token following the argument. */
  last_token = curr_token_sequence_number;
  /* Create the string version of the argument. */
  /* Don't allow wrapping keyword-like identifiers inside __identifier(...). */
  init_token_string(&start_position, /*keep_spacing=*/FALSE,
                    /*suppress_identifier_wrapping=*/TRUE);
  add_token_cache_segment_to_string(&attribute_cache, first_token,
                                    last_token);
  /* Copy the string to IL memory. */
  value = make_copy_of_token_string();
  return value;
}  /* scan_ms_attribute_other_arg */


static a_boolean scan_ms_attribute_boolean_arg(
					an_ms_attribute_param_ptr	param)
/*
Scan a boolean argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_boolean		result = FALSE;
  a_source_position	arg_pos;
  a_boolean		err;
  char			*string_for_token;

  arg_pos = pos_curr_token;
  string_for_token = get_string_value_for_token(&err);
  if (string_for_token == NULL && !err) {
    /* The current token was not of an expected kind. */
    str_error(ec_exp_ms_attr_bool_value, param->name);
    flush_tokens();
  }  /* if */
  /* The source pointer will be NULL in error cases. */
  if (string_for_token != NULL) {
    if (strcmp(string_for_token, "true") == 0) {
      result = TRUE;
    } else if (strcmp(string_for_token, "false") == 0) {
      result = FALSE;
    } else {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_exp_ms_attr_bool_value, &arg_pos, param->name);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_boolean_arg */


static int scan_ms_attribute_enum_arg(an_ms_attribute_param_ptr	param)
/*
Scan a enumeration argument of a Microsoft attribute.  Return the value
that identifies the element of the enumeration that was specified.
The first entry on the list is 1.  Zero is returned if no matching
entry was found.

The value specified may or may not be enclosed in quotes.  Case is not
significant.
*/
{
  int			result = 0;
  a_source_position	arg_pos;
  a_boolean		err;
  char			*string_for_token;

  arg_pos = pos_curr_token;
  string_for_token = get_string_value_for_token(&err);
  if (string_for_token == NULL && !err) {
    /* The current token was not of an expected kind. */
    str_error(ec_exp_ms_attr_enum_value, param->name);
    flush_tokens();
  }  /* if */
  /* The source pointer will be NULL in error cases. */
  if (string_for_token != NULL) {
    /* Look for the resulting value in the list of acceptable values. */
    char	**values;
    for (values = param->values; *values != NULL; values++) {
      if (strcmp(*values, string_for_token) == 0) break;
    }  /* for */
    /* If we found a match, compute the element number. */
    if (*values != NULL) {
      result = (int)(values - param->values);
    } else {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_invalid_ms_attr_enum_value, &arg_pos, param->name);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_enum_arg */


static a_const_char *scan_ms_attribute_uuid_arg(
                                               an_ms_attribute_param_ptr param)
/*
Scan an argument of UUID type.  Such arguments are either a UUID string
or a __uuidof operator.  The UUID string is returned.  A NULL pointer is
returned for invalid arguments.
*/
{
  a_const_char		*result = NULL;
  a_source_position	arg_pos;

  arg_pos = pos_curr_token;
  if (curr_token == tok_string_literal || curr_token == tok_uuid) {
    /* A string literal or an unquoted UUID string.  Scan it as a GUID
       string. */
    result = scan_GUID_string();
  } else {
    /* Something else.  The only other valid argument is a __uuidof operator.
       Keywords are not recognized within attributes, so check for an
       identifier named __uuidof. */
    a_boolean	is_uuidof = FALSE;
    if (curr_token == tok_identifier) {
      char	*identifier;
      a_boolean	err;
      identifier = get_string_value_for_token(&err);
      is_uuidof = identifier != NULL && strcmp(identifier, "__uuidof") == 0;
    }  /* if */
    if (!is_uuidof) {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_invalid_ms_attr_uuid_value, &arg_pos, param->name);
      flush_tokens();
    } else {
      /* Scan the __uuidof operator.  It is of the form __uuidof(operand). */
      result = scan_uuidof_operand();
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_uuid_arg */


static an_ms_attribute_arg_ptr scan_ms_attribute_arg(
					an_ms_attribute_param_ptr	param)
/*
Scan an argument to a Microsoft attribute reference.  Create an argument
entry that describes the argument, and return it to the caller.  "param"
is the parameter description for the parameter associated with the argument.
*/
{
  an_ms_attribute_arg_ptr	arg;

  /* Allocate an argument entry of the required kind. */
  arg = alloc_ms_attribute_arg(param->kind);
  arg->param_name = param->name;
  switch (param->kind) {
    case msaak_uuid:
      /* A GUID string. */
      arg->variant.uuid_string = scan_ms_attribute_uuid_arg(param);
      break;
    case msaak_integer:
      /* An integer constant. */
      arg->variant.integer_value = scan_ms_attribute_integer_arg();
      break;
    case msaak_boolean:
      /* A boolean constant. */
      arg->variant.bool_value = scan_ms_attribute_boolean_arg(param);
      break;
    case msaak_enumeration:
      /* A member of a list of specific values. */
      arg->variant.enum_value = scan_ms_attribute_enum_arg(param);
      break;
    case msaak_string:
      /* A string. */
      arg->variant.string_constant = scan_ms_attribute_string_arg();
      break;
    case msaak_other:
      /* An "other" argument.  Convert all the tokens until the terminating
         "," or ")" into a string. */
      arg->variant.other_string = scan_ms_attribute_other_arg();
      break;
    default:
      unexpected_condition_str("scan_ms_attribute_arg: bad argument kind");
  }  /* switch */
  return arg;
}  /* scan_ms_attribute_arg */


static an_ms_attribute_param_ptr get_named_parameter(
				an_ms_attribute_kind_descr_ptr	attr_descr,
				an_ms_attribute_arg_ptr		arg_list)
/*
The current token is expected to be an identifier that gives the name of
the parameter being specified.  Look up the name in the list of parameters
and return the parameter pointer.  If there is no matching parameter, issue
an error and return a NULL pointer.

attr_descr describes the attribute being scanned.  arg_list is the list of
arguments scanned so far, and is used to detect a duplicated argument.
*/
{
  char				*param_name;
  an_ms_attribute_param_ptr	msapp = NULL;
  a_boolean			err;
  a_source_position		name_pos;

  name_pos = pos_curr_token;
  /* Get the lower case string for the parameter name. */
  param_name = get_string_value_for_token(&err);
  if (param_name != NULL) {
    /* Look for the parameter name in the parameter list. */
    for (msapp = attr_descr->parameters; msapp != NULL; msapp = msapp->next) {
      /* See if the names match.  Ignore parameters that are to be considered
         unnamed. */
      if (!msapp->is_unnamed && strcmp(param_name, msapp->name) == 0) {
        break;
      }  /* if */
    }  /* for */
    if (msapp == NULL) {
      /* No match was found. */
      pos_st2_error(ec_invalid_ms_attr_name, &name_pos, attr_descr->name,
                    param_name);
    } else {
      /* Check for a repeated argument. */
      an_ms_attribute_arg_ptr	msaap;
      for (msaap = arg_list; msaap != NULL; msaap = msaap->next) {
        if (strcmp(msaap->param_name, param_name) == 0) {
          /* The argument has already been given a value. */
          pos_st_error(ec_duplicate_ms_attr_arg, &name_pos, param_name);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* The caller should have already verified that the next token is a "=". */
  check_assertion(curr_token == tok_assign);
  /* Bypass the "=". */
  (void)get_token();
  return msapp;
}  /* get_named_parameter */


static an_ms_attribute_arg_ptr scan_ms_attribute_arg_list(
				an_ms_attribute_kind_descr_ptr	attr_descr)
/*
Scan the arguments of a Microsoft attribute reference.  The current token is
usually either the "=" that precedes a single argument or the "(" the
precedes an argument list, but this routine is also called for attributes
that expect a parameter list even if such a list is missing (so that a
diagnostic can be issued here).  Return a pointer to the list of arguments,
or NULL if the argument list is invalid.
*/
{
  an_ms_attribute_param_ptr	param;
  an_ms_attribute_arg_ptr	arg_list = NULL;
  an_ms_attribute_arg_ptr	arg_tail = NULL;
  a_boolean			any_named_args = FALSE;
  a_boolean			any_errors = FALSE;

  param = attr_descr->parameters;
  if (curr_token == tok_assign) {
    /* Some attributes accept "attr=x" style references, which has the effect
       of providing a value for the initial argument. */
    if (!attr_descr->initialization_style_arg_allowed) {
      /* This style of argument is not permitted for this attribute. */
      str_error(ec_cannot_assign_to_ms_attr, attr_descr->name);
      flush_tokens();
      any_errors = TRUE;
    } else {
      /* Scan the argument associated with the initial parameter. */
      /* Bypass the "=" */
      (void)get_token();
      arg_list = scan_ms_attribute_arg(param);
      param = param->next;
    }  /* if */
  } else if (curr_token == tok_lparen) {
    /* Bypass the left parenthesis. */
    check_assertion(curr_token == tok_lparen);
    (void)get_token();
    add_stop_token(tok_rparen);
    /* Scan a comma-separated list of arguments. */
    do {
      an_ms_attribute_arg_ptr	arg;
      /* Arguments may be specified either positionally or with names.
         A named argument is of the form "name=value".  Check for a named
         argument. */
      if (curr_token == tok_identifier && next_token() == tok_assign) {
        param = get_named_parameter(attr_descr, arg_list);
        any_named_args = TRUE;
        if (param == NULL) {
          /* An invalid named parameter.  Flush to the next argument. */
          flush_tokens();
          continue;
        }   /* if */
      } else if (any_named_args) {
        /* A positional argument cannot follow a named one. */
        if (!any_errors) error(ec_positional_after_named);
        /* Flush to the next argument. */
        flush_tokens();
        any_errors = TRUE;
        continue;
      } else if (param == NULL) {
        if (!any_errors) error(ec_too_many_ms_attr_args);
        flush_tokens();
        any_errors = TRUE;
        continue;
      }   /* if */
      arg = scan_ms_attribute_arg(param);
      if (arg_list == NULL) {
        arg_list = arg;
      } else {
        arg_tail->next = arg;
      }  /* if */
      arg_tail = arg;
      /* Advance to the next parameter in the list. */
      if (!any_named_args) param = param->next;
    } while (loop_token(tok_comma));
    remove_stop_token(tok_rparen);
    /* Look for the closing right parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  if (arg_list == NULL && !any_errors &&
      attr_descr->kind  > (an_ms_attribute_kind)msak_misc &&
      param != NULL && !param->is_unnamed) {
    /* If there is a parameter list but no arguments were specified, issue
       an error.  We don't currently know which arguments are required,
       so we don't issue an error for too few arguments. */
    if (arg_list == NULL) {
      str_error(ec_exp_ms_attr_arg_list, attr_descr->name);
    }  /* if */
  }  /* if */
  return arg_list;
}  /* scan_ms_attribute_arg_list */


static void scan_unrecognized_ms_attribute_arg_list(void)
/*
Scan the arguments of an unrecognized Microsoft attribute reference.  The
current token is the "=" that precedes a single argument or the "(" that
precedes an argument list.  Because the attribute is unrecognized, we don't
know the form of the parameter list expected.  This routine just scans tokens
until the end of the attribute is found.
*/
{
  /* The stop tokens should be set appropriately so that this will flush
     to the "," that separates attributes, or to the closing "]" of
     the attribute block. */
  flush_tokens_without_warning();
}  /* scan_unrecognized_ms_attribute_arg_list */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* is_parameter is only used in some configurations. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
static an_ms_attribute_ptr scan_ms_attribute(a_boolean	is_parameter)
/*
Scan a single Microsoft attribute of an attribute block that may contain
multiple attributes.  Return a pointer to the attribute entry that represents
the attribute.

is_parameter is TRUE if the attribute is part of a function parameter
declaration.
*/
{
  an_ms_attribute_kind_descr_ptr	attr_descr = NULL;
  a_token_sequence_number		first_token;
  a_token_sequence_number		last_token;
  a_source_position			start_position;
  an_ms_attribute_ptr			attr = NULL;
  a_token_kind				next_tok;

  /* Save the token sequence number of the first token of this attribute. */
  first_token = curr_token_sequence_number;
  start_position = pos_curr_token;
  /* Look for a target specifier before the attribute name (for example,
     "returnvalue:SA_Post").  Microsoft added these when they restructured
     the attribute support for C++/CLI.  They are also used for source
     annotation attributes.  These are similar in purpose but have different
     values than the an_ms_attribute_target values in the front end.
     At this time, the values are ignored. */
  next_tok = next_token();
  if ((curr_token == tok_identifier || is_keyword_token(curr_token)) &&
      next_tok == tok_colon) {
    /* Skip past the target value and the colon. */
    (void)get_token();
    (void)get_token();
    next_tok = next_token();
  }  /* if */
  if (curr_token == tok_colon_colon ||
      (next_tok == tok_colon_colon || next_tok == tok_lt)) {
    /* The attribute name is a qualified identifier.  For now, just coalesce
       the name and treat it is an "unrecognized" attribute without a
       diagnostic. */
    (void)is_generalized_identifier_start(GID_NO_OPTIONS);
    /* Skip to the token after the identifier. */
    (void)get_token();
    attr_descr = unrecognized_attribute;
  }  /* if */
  /* Look up the attribute identifier.  If the identifier is unknown,
     the "unrecognized" attribute will be returned.  In error cases, such
     as a missing attribute name, a NULL attribute description is returned. */
  if (attr_descr == NULL) {
    attr_descr = look_up_attribute();
  }  /* if */
  if (attr_descr != NULL) {
    a_boolean	arg_list_present = curr_token == tok_assign ||
                                   curr_token == tok_lparen;
    /* Allocate an entry to represent this attribute. */
    attr = alloc_ms_attribute();
    attr->kind = attr_descr->kind;
    attr->name = attr_descr->name;
    attr->kind_descr = attr_descr;
    attr->position = start_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Create a source sequence entry for the attribute.  This is not done
       for parameter attributes as they appear within a declaration. */
    if (!is_parameter) {
      attr->source_sequence_entry = add_empty_source_sequence_entry();
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Look for an argument list.  We do this even for attributes without
       parameters, for error recovery purposes. */
    if (arg_list_present || attr_descr->parameters != NULL) {
      if (attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
        if (arg_list_present) {
          /* We are scanning an unrecognized attribute, so we don't know the
             form of the expected parameters. */
          scan_unrecognized_ms_attribute_arg_list();
        }  /* if */
      } else {
        /* The attribute is of a known kind.  The argument list can be
           scanned with knowledge of the associated parameters. */
        attr->arg_list = scan_ms_attribute_arg_list(attr_descr);
        if (attr->arg_list == NULL) {
          /* A NULL arg_list is returned if an error occurred while scanning
             the argument list.  Set attr to NULL to discard the attribute. */
          attr = NULL;
        }  /* if */
      }  /* if */
    } else if (curr_token != tok_comma && curr_token != tok_rbracket) {
      /* The attribute name was not followed by anything that looks like
         an argument, nor was it followed by anything that looks like an
         attribute separator or end of an attribute list. */
      if (attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
        /* There are forms of Microsoft attributes that we don't currently
           support.  Don't attempt to diagnose a missing argument list as
           we probably got lost earlier on. */
        flush_tokens();
      } else {
        /* No parameters were expected.  Complain of a missing "," or "]". */
        syntax_error(ec_exp_comma_or_rbracket);
        /* The attribute is ill-formed, so discard it. */
        attr = NULL;
      }  /* if */
    }  /* if */
    /* Save the position of the token following the attribute. */
    last_token = curr_token_sequence_number;
    /* Create the string version of the attribute. */
    /* Don't allow wrapping keyword-like identifiers inside
       __identifier(...). */
    init_token_string(&start_position, /*keep_spacing=*/FALSE,
                      /*suppress_identifier_wrapping=*/TRUE);
    add_token_cache_segment_to_string(&attribute_cache, first_token,
                                      last_token);
    /* Copy the string to IL memory. */
    if (attr != NULL) attr->string = make_copy_of_token_string();
#if DEBUG
    if (db_flag_is_set("msattr")) {
      if (attr != NULL) db_microsoft_attribute(attr);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  return attr;
}  /* scan_ms_attribute */


an_ms_attribute_ptr scan_microsoft_attributes(a_boolean	is_parameter)
/*
Scan a Microsoft attribute block.  The general syntax is:

  [ attr ]
  [ attr1, attr2 ]
  [ attr1(value, value), attr2 ]
  [ attr1(argname=value, argname=value) ]

Attributes can be standalone, in which case they are followed by a ";", or
can apply to the declaration that follows.

Each attribute block can contain multiple attributes.  A list of the attributes
is returned.  is_parameter is TRUE if the attribute is part of a function
parameter declaration.
*/
{
  an_ms_attribute_ptr	attr_list = NULL;
  an_ms_attribute_ptr	attr_tail = NULL;

  /* Start a new stop token state. */
  push_stop_token_stack();
  add_stop_token(tok_end_of_source);
  add_stop_token(tok_rbracket);
  add_stop_token(tok_comma);
  /* The current token is expected to be the opening "[". */
  check_assertion(curr_token == tok_lbracket);
  /* There can be multiple attribute blocks.  Scan all of them. */
  while (curr_token == tok_lbracket) {
    cache_attribute_block();
    /* Bypass the "[". */
    (void)get_token();
    /* Scan the list of attributes. */
    do {
      an_ms_attribute_ptr	attr;
      /* Scan the attribute. */
      attr = scan_ms_attribute(is_parameter);
      /* A NULL attribute may be returned in certain error cases. */
      if (attr == NULL) continue;
      /* Add it to the list of attributes for this block. */
      if (attr_list == NULL) {
        attr_list = attr;
      } else {
        /* The list is linked on both the next and next_in_block pointers. */
        attr_tail->next_in_block = attr;
        attr_tail->next = attr;
      }  /* if */
      attr_tail = attr;
    } while (loop_token(tok_comma));
    /* Look for the closing right bracket. */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    /* Discard the token cache used to create the attribute strings. */
    discard_token_cache(&attribute_cache);
  }  /* while */
  /* Restore the stop token set as at entry. */
  remove_stop_token(tok_end_of_source);
  remove_stop_token(tok_rbracket);
  remove_stop_token(tok_comma);
  pop_stop_token_stack();
  return attr_list;
}  /* scan_microsoft_attributes */


void skip_microsoft_attribute_tokens(void)
/*
A Microsoft attribute is next.  Skip over its tokens.
*/
{
  check_assertion(curr_token == tok_lbracket);
  while (curr_token == tok_lbracket) {
    flush_until_matching_token_full(/*limit_flush=*/FALSE);
    (void)get_token();
  }  /* if */
}  /* skip_microsoft_attribute_tokens */

#if GENERATE_SOURCE_SEQUENCE_LISTS

static void finalize_ms_attribute_source_sequence_entry(
						an_ms_attribute_ptr	msap,
						a_boolean		err)
/*
Complete the processing of the source sequence entry associated with the
Microsoft attribute "msap".  If "err" is TRUE, remove the empty source
sequence entry from the list.  Also remove it in template declaration contexts
when nonclass prototype instantiations are not recorded in the IL, since in
that case the corresponding entries for the template won't be recorded either.
Otherwise, complete the source sequence entry.
*/
{
  a_boolean  remove_sse = err;

  if (!remove_sse &&
      scope_is(&scope_stack_top(), sck_template_declaration) &&
      (!nonclass_prototype_instantiations ||
       !prototype_instantiations_in_il)) {
    remove_sse = TRUE;
  }  /* if */
  if (remove_sse) {
    /* This attribute is not being added to the IL, so we must remove the
       empty source sequence entry created for it. */
    if (msap->source_sequence_entry != NULL) {
      remove_from_src_seq_list(msap->source_sequence_entry);
      msap->source_sequence_entry = NULL;
    }  /* if */
  } else {
    update_source_sequence_list((char *)msap,
                                (an_il_entry_kind)iek_ms_attribute,
                                msap->source_sequence_entry);
  }  /* if */
}  /* finalize_ms_attribute_source_sequence_entry */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static void process_ms_attr_uuid(an_ms_attribute_ptr  msap)
/*
The given Microsoft attribute represents the "uuid" attribute applied to a
class type.  Record the associated uuid string in the class type.
*/
{
  check_assertion(
                msap->entity.kind == (a_byte_il_entry_kind)iek_type &&
                is_immediate_class_type((a_type_ptr)msap->entity.ptr) &&
                msap->arg_list != NULL &&
                msap->arg_list->kind == (an_ms_attribute_arg_kind)msaak_uuid);
  record_uuid_for_class((a_type_ptr)msap->entity.ptr,
                        msap->arg_list->variant.uuid_string,
                        &msap->position);
}  /* process_ms_attr_uuid */


static void process_microsoft_attribute(an_ms_attribute_ptr  msap)
/*
The given attribute requires special processing (e.g., updating the IL entry
it is bound to): Perform that processing.
*/
{
  switch (msap->kind) {
    case msak_uuid:
      process_ms_attr_uuid(msap);
      break;
#if INCLUDE_EDG_TEST_ATTRIBUTES
    case msak_edg_test:
      /* Nothing to be done. */
      break;
#endif /* INCLUDE_EDG_TEST_ATTRIBUTES */
    default:
      unexpected_condition();
  }  /* switch */
}  /* process_microsoft_attribute */


static a_boolean entity_is_prototype_instantiation(char              *entity,
                                                   an_il_entry_kind  kind)
/*
Return TRUE if the given entity is a prototype instantiation or a static
data member of a prototype instantiation.
*/
{
  a_boolean  result = FALSE;

  switch (kind) {
    case iek_type:
      { a_type_ptr  tp = (a_type_ptr)entity;
        if (is_immediate_class_type(tp) &&
            tp->variant.class_struct_union.is_prototype_instantiation) {
          result = TRUE;
        }  /* if */
      }
      break;
    case iek_routine:
      { a_routine_ptr  rp = (a_routine_ptr)entity;
        result = rp->is_prototype_instantiation;
      }
      break;
    case iek_variable:
      { a_variable_ptr  vp = (a_variable_ptr)entity;
        if (vp->source_corresp.is_class_member) {
          a_type_ptr  tp = parent_class_of(vp);
          if (tp->variant.class_struct_union.is_prototype_instantiation) {
            result = TRUE;
          }  /* if */
        }  /* if */
      }
      break;
    default:
      break;
  }  /* switch */
  return result;
}  /* entity_is_prototype_instantiation */


void apply_microsoft_attributes(an_ms_attribute_ptr	*attributes,
				char			*entity,
				an_il_entry_kind	kind,
				an_ms_attribute_target	target)
/*
This routine is used to indicate that the list of Microsoft attributes
specified by "attributes" should apply to the entity specified by "entity".
The attributes must apply to the entity kind specified by "target".  The
entity is updated to reflect the attributes that apply, and the attributes
are added to the appropriate IL list (either the scope list or a list
in the param_type entry).
*/
{
  an_ms_attribute_ptr		msap;
  an_ms_attribute_ptr		new_list = NULL;
  an_ms_attribute_ptr		new_tail = NULL;
  a_source_correspondence	*scp;
  an_ms_attribute_ptr		next_msap;
  a_boolean                     record_entity_in_attribute = TRUE;

  scp = source_corresp_for_il_entry(entity, kind);
  if (!prototype_instantiations_in_il) {
    /* Since prototype instantiations are not recorded in the IL, we cannot
       point the attribute to a prototype instantiation. */
    record_entity_in_attribute =
                             !entity_is_prototype_instantiation(entity, kind);
  }  /* if */
  /* Check whether the attributes have the appropriate target. */
  for (msap = *attributes; msap != NULL; msap = next_msap) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_boolean	is_error = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    next_msap = msap->next;
    if ((msap->kind_descr->target & target) == 0 &&
        msap->kind_descr->target != MSAT_ANY) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
       is_error = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
       if (msap->kind_descr->target == MSAT_STANDALONE) {
         pos_st_error(ec_invalid_use_of_standalone_ms_attr, &msap->position,
                      msap->name);
       } else {
         pos_st_error(ec_invalid_use_of_ms_attr, &msap->position, msap->name);
       }  /* if */
#if DEBUG
       if (db_flag_is_set("msattr")) {
         fprintf(f_debug, "Attribute target: allowed=%x, context=%x\n",
                 (int)msap->kind_descr->target, (int)target);
       }  /* if */
#endif /* DEBUG */
    } else {
      /* A valid attribute.  Add it to the new list. */
      if (new_list == NULL) {
        new_list = msap;
      } else {
        new_tail->next = msap;
      }  /* if */
      new_tail = msap;
      if (record_entity_in_attribute) {
        /* Update the entity pointer in the attribute. */
        msap->entity.kind = (a_byte_il_entry_kind)kind;
        msap->entity.ptr = entity;
      }  /* if */
      /* Except for parameter entries, add the attribute to the scope list. */
      if (kind != (an_il_entry_kind)iek_param_type) {
        add_to_ms_attributes_list(msap, decl_scope_level);
      }  /* if */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (kind != (an_il_entry_kind)iek_param_type) {
      /* Either complete the source sequence entry or discard it. */
      finalize_ms_attribute_source_sequence_entry(msap, is_error);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (msap->kind > (an_ms_attribute_kind)msak_misc) {
      /* An attribute for which special processing is neededed. */
      process_microsoft_attribute(msap);
    }  /* if */
  }  /* for */
  /* All entities except for param_type entries are expected to have
     source correspondences. */
  if (scp != NULL) {
    scp->has_associated_attribute = TRUE;
  } else {
    a_param_type_ptr	ptp;
    check_assertion(kind == (an_il_entry_kind)iek_param_type);
    /* Set the param type entry to point to the valid attributes. */
    ptp = (a_param_type_ptr)entity;
    ptp->ms_attributes = new_list;
  }  /* if */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* apply_microsoft_attributes */


void verify_standalone_attributes(an_ms_attribute_ptr	*attributes)
/*
This routine is used to verify that the list of Microsoft attributes
specified by "attributes" contains only standalone attributes.  Valid
attributes are added to the appropriate IL list.
*/
{
  an_ms_attribute_ptr	msap;
  an_ms_attribute_ptr	next_msap;

  for (msap = *attributes; msap != NULL; msap = next_msap) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_boolean	is_error = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    next_msap = msap->next;
    if ((msap->kind_descr->target & MSAT_STANDALONE) == 0 &&
        (msap->kind_descr->target & MSAT_ANY) == 0) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
       is_error = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
       pos_st_error(ec_invalid_use_of_ms_attr, &msap->position, msap->name);
    } else {
      /* Add the attribute to the IL. */
      add_to_ms_attributes_list(msap, decl_scope_level);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Either complete the source sequence entry or discard it. */
    finalize_ms_attribute_source_sequence_entry(msap, is_error);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* for */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* verify_standalone_attributes */


void dispose_of_unapplied_attributes(an_ms_attribute_ptr	*attributes,
				     an_error_code		error_code)
/*
"attributes" is a list of attributes that could not be applied to an entity.
Issue an error and do any cleanup needed to dispose of the attributes.
"error_code" identifies the message to be issued.  The error is suppressed
if it is ec_no_error.
*/
{
  check_assertion(*attributes != NULL);
  if (error_code != ec_no_error) {
    pos_error(error_code, &(*attributes)->position);
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Remove the empty source sequence entries previously created for these
     attributes. */
  { an_ms_attribute_ptr	msap;
    for (msap = *attributes; msap != NULL; msap = msap->next) {
      finalize_ms_attribute_source_sequence_entry(msap, /*is_error=*/TRUE);
    }  /* for */
  }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* dispose_of_unapplied_attributes */


static an_ms_attribute_arg_ptr duplicate_ms_attribute_args(
                                                an_ms_attribute_arg_ptr  orig)
/*
Return a duplicate of the given list of attribute arguments.
*/
{
  an_ms_attribute_arg_ptr  result = NULL, *p_msaap = &result;

  while (orig != NULL) {
    *p_msaap = alloc_ms_attribute_arg(orig->kind);
    **p_msaap = *orig;
    orig = orig->next;
    p_msaap = &((*p_msaap)->next);
  }  /* while */
  return result;
}  /* duplicate_ms_attribute_args */


an_ms_attribute_ptr duplicate_ms_attributes(an_ms_attribute_ptr  orig,
                                            char                 *new_entity)
/*
Return a duplicate of the given list of attributes.  The attribute arguments
(if any) are also duplicated.  However, other items these attributes and
arguments point to (like character strings) are shared.  Update the copy
of the attribute to refer to new_entity.
*/
{
  an_ms_attribute_ptr  result = NULL, *p_msap = &result, prev_msap = NULL;

  while (orig != NULL) {
    *p_msap = alloc_ms_attribute();
    **p_msap = *orig;
    (*p_msap)->entity.ptr = new_entity;
    if (prev_msap != NULL) {
      prev_msap->next = *p_msap;
    }  /* if */
    (*p_msap)->arg_list = duplicate_ms_attribute_args(orig->arg_list);
    prev_msap = *p_msap;
    orig = orig->next_in_block;
    p_msap = &((*p_msap)->next_in_block);
  }  /* while */
  return result;
}  /* duplicate_ms_attributes */


an_ms_attribute_ptr find_ms_attribute_for_entity(
                                            an_ms_attribute_ptr          msap,
                                            a_source_correspondence_ptr  scp)
/*
Find an attribute associated with the nonlocal (i.e., not defined inside a
function) entity whose source correspondence is scp.  If msap is NULL, the
search starts at the beginning of the list of attributes recorded for the
parent scope of scp; otherwise, the search starts with msap->next.  This
allows finding all attributes associated with scp using repeated calls to
find_ms_attribute_for_entity.  If no attribute associated with scp is found,
return NULL.
*/
{
  if (msap == NULL) {
    /* Determine the scope whose Microsoft attributes list should be
       searched. */
    if (C_mode()) {
      /* In C mode, there are no class or namespace scopes; so only the file
         scope is searched. */
      msap = il_header.primary_scope->ms_attributes;
    } else if (scp->is_class_member) {
      msap = class_type_supp(scp_parent_class(scp))->assoc_scope
                                                   ->ms_attributes;
    } else if (scp_is_namespace_member(scp)) {
      msap = scp_parent_namespace(scp)->variant.assoc_scope->ms_attributes;
    } else {
      /* File scope in C++ mode. */
      msap = il_header.primary_scope->ms_attributes;
    }  /* if */
  } else {
    /* Start off right after the given attribute (which is presumably an
       attribute found in a previous call). */
    msap = msap->next;
  }  /* if */
  while (msap != NULL) {
    if (msap->entity.ptr == (char*)scp) break;
    msap = msap->next;
  }  /* while */
  return msap;
}  /* find_ms_attribute_for_entity */

#if DEBUG

void db_microsoft_attribute(an_ms_attribute_ptr	msap)
/*
Display a Microsoft attribute entry, for debugging purposes.
*/
{
  an_ms_attribute_arg_ptr	arg;
  int				arg_number = 0;

  fprintf(f_debug, "Microsoft attribute '%s' at %p (%lu/%d):\n",
          msap->name == NULL ? "NULL" : msap->name, (void *)msap,
          (unsigned long)msap->position.seq, msap->position.column);
  fprintf(f_debug, "  attribute string: %s\n", msap->string);
  for (arg = msap->arg_list; arg != NULL; arg = arg->next) {
    fprintf(f_debug, "  argument %d (%s): ", arg_number++, arg->param_name);
    switch (arg->kind) {
      case msaak_integer:
        fprintf(f_debug, "%ld", (long)arg->variant.integer_value);
        break;
      case msaak_boolean:
        fprintf(f_debug, "%s", arg->variant.integer_value ? "true" : "false");
        break;
      case msaak_string:
        db_constant(arg->variant.string_constant);
        break;
      case msaak_other:
        fprintf(f_debug, "%s", arg->variant.other_string);
        break;
      case msaak_uuid:
        fprintf(f_debug, "%s", arg->variant.uuid_string);
        break;
      case msaak_enumeration:
        fprintf(f_debug, "%d", arg->variant.enum_value);
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    fprintf(f_debug, "\n");
  }  /* for */
}  /* db_microsoft_attribute */


unsigned long db_show_ms_attrib_space_used(unsigned long grand_total)
/*
Show space used by the Microsoft attribute routines.  This is called by
the symbol table space used routine.  The space used by these
routines is reported as part of the symbol table memory used.
*/
{
  unsigned long	num;
  unsigned long	size;
  unsigned long	total;

  db_space_used("ms attribute descrs",
                 num_ms_attribute_kind_descrs_allocated,
                 an_ms_attribute_kind_descr);
  db_space_used("ms attribute parameters",
                 num_ms_attribute_params_allocated,
                 an_ms_attribute_param);
  return grand_total;
}  /* db_show_ms_attrib_space_used */

#endif /* DEBUG */

void ms_attrib_one_time_init(void)
/*
One-time initialization for ms_attrib.c static variables.
*/
{
  ms_attr_buffer = NULL;
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_array_saved_var_array_elem(attribute_lookup_table),
      pch_saved_var_array_elem(unrecognized_attribute),
#if DEBUG
      pch_saved_var_array_elem(num_ms_attribute_kind_descrs_allocated),
      pch_saved_var_array_elem(num_ms_attribute_params_allocated),
#endif /* DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
}  /* ms_attrib_one_time_init */


void ms_attrib_init(void)
/*
The per-compilation unit initialization routine for variables related to
Microsoft attribute processing.
*/
{
  scan_misc_attributes_as_unrecognized =
                                       SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING;
  unrecognized_attribute = NULL;
#if DEBUG
  num_ms_attribute_kind_descrs_allocated = 0;
  num_ms_attribute_params_allocated = 0;
#endif /* DEBUG */
  memzero((char *)attribute_lookup_table, sizeof(attribute_lookup_table));
  curr_attribute_descr = NULL;
  /* Build the structure used to describe the various attributes. */
  if (microsoft_mode) init_attribute_kinds();
}  /* ms_attrib_init */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
