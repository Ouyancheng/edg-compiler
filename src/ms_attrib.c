/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
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
		accept_unrecognized_attributes;
			/* TRUE if unrecognized attributes should be accepted.
			   If this is FALSE, they are still scanned as
			   unrecognized attributes, but a diagnostic is
			   issued. */

static an_ms_attribute_kind_descr_ptr
		unrecognized_attribute;
			/* A special attribute kind entry that represents
			   an unrecognized attribute.  This is used even when
			   unrecognized attribute are not accepted, for
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

#define ATTRIBUTE_LOOKUP_TABLE_SIZE 63
			/* The number of buckets in the template lookup table.
			   This number should be prime. */

static an_ms_attribute_kind_descr_ptr
		attribute_lookup_table[ATTRIBUTE_LOOKUP_TABLE_SIZE];
			/* Table used to determine the attribute kind for a
			   given attribute name.  Each element of the
			   array points to  a list of entries for attributes
			   that hash to a given group. */

#define HASH_FACTOR ((unsigned int)73)
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from a name
                           string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */

static unsigned long hash_attribute_name(char		*name,
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
					char				*name)
/*
Add "msakdp" to the attribute lookup table.  "name" is the name of the
attribute.
*/
{
  int		bucket;
  sizeof_t	length = strlen(name);

  bucket = hash_attribute_name(name, length) % ATTRIBUTE_LOOKUP_TABLE_SIZE;
  msakdp->next = attribute_lookup_table[bucket];
  msakdp->name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, name);
  msakdp->name_length = length;
  attribute_lookup_table[bucket] = msakdp;
}  /* add_attribute_lookup_table_entry  */


static an_ms_attribute_kind_descr_ptr find_attribute_kind(char		*name,
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
        strncmp(msakdp->name, name, length) == 0) {
      break;
    }  /* if */
  }  /* for */
  return msakdp;
}  /* an_ms_attribute_kind_descr_ptr */


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
  msakdp->target = msat_none;
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
				       char			*name,
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
  msakdp->kind = kind;
  msakdp->target = target;
  if (name != NULL) {
    /* If this is not an unnamed attribute, create a lookup table entry. */
    add_attribute_lookup_table_entry(msakdp, name);
  }  /* if */
  curr_attribute_descr = msakdp;
}  /* make_attribute_description */


static void set_initialization_style_arg_allowed(void)
/*
Set the initialization-style argument allowed flag for the current attribute.
*/
{
  curr_attribute_descr->initialization_style_arg_allowed = TRUE;
}  /* set_initiallization_style_arg_allowed */


static void add_attribute_parameter(an_ms_attribute_arg_kind	kind,
				    char			*name,
				    a_boolean			is_unnamed,
				    char			*values)
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
  msapp->name = copy_string_to_region(FILE_SCOPE_REGION_NUMBER, name);
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
    int		num_elements = 1;
    int		element;
    char	*ptr;
    char	**list;
    /* Find the number of elements. */
    for (ptr = values; *ptr != '\0'; ptr++) {
      if (*ptr == ',') num_elements++;
    }  /* for */
    /* Allocate an array for the list.  An extra element is allocated for a
       NULL terminating element. */
    list = (char**)alloc_fe(sizeof(char*) * num_elements + 1);
    /* Set the terminating element to NULL. */
    list[num_elements] = NULL;
    for (element = 0, ptr = values; element < num_elements; ++element) {
      /* Find the end of this element. */
      char	*end = strchr(ptr, ',');
      sizeof_t	length;
      char	*entry;
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


static void init_attribute_kinds(void)
/*
Create the data structures used to describe the kinds of attributes that
are accepted.
*/
{
  make_attribute_description((an_ms_attribute_kind)msak_unrecognized,
                             (char*)NULL, msat_none);
  /* Save a pointer to the special "unrecognized" attribute kind. */
  unrecognized_attribute = curr_attribute_descr;
  /* [aggregatable(value)] */
  make_attribute_description((an_ms_attribute_kind)msak_aggregatable,
			     "aggregatable", msat_class);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "value",
                          /*is_unnamed=*/FALSE,
                          "never,allowed,always");
  /* [coclass] */
  make_attribute_description((an_ms_attribute_kind)msak_coclass,
			     "coclass", msat_class);
  /* [com_interface_entry] */
  make_attribute_description((an_ms_attribute_kind)msak_com_interface_entry,
			     "com_interface_entry", msat_class);
  set_initialization_style_arg_allowed();
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "entry",
                          /*is_unnamed=*/TRUE,
                          NULL);
  /* [emitidl] */
  make_attribute_description((an_ms_attribute_kind)msak_emitidl,
			     "emitidl", msat_standalone);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "mode",
                          /*is_unnamed=*/TRUE,
                          "false,true,restricted,forced");
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "defaultimports",
                          /*is_unnamed=*/FALSE,
                          NULL);
  /* [soap_handler] */
  make_attribute_description((an_ms_attribute_kind)msak_soap_handler,
			     "soap_handler", msat_class);
#if INCLUDE_EDG_TEST_ATTRIBUTES
  /* These are special attributes included for testing purposes. */
  /* [edg_test_1] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
			     "edg_test_1", msat_standalone);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_integer,
                          "arg1", /*is_unnamed=*/FALSE, NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_boolean,
                          "arg2", /*is_unnamed=*/FALSE, NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_string,
                          "arg3", /*is_unnamed=*/FALSE, NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_uuid,
                          "arg4", /*is_unnamed=*/FALSE, NULL);
  add_attribute_parameter((an_ms_attribute_arg_kind)msaak_enumeration,
                          "arg5", /*is_unnamed=*/FALSE, "a,b,c,aa,bb,cc");
  /* [edg_test_2] */
  make_attribute_description((an_ms_attribute_kind)msak_edg_test,
			     "edg_test_2", msat_standalone);
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
  a_boolean		save_suppress_keyword_recognition;

  /* Save the current value of the lexical scanning mode flags. */
  save_expand_macros = expand_macros;
  save_do_string_literal_concatenation = do_string_literal_concatenation;
  save_fetch_pp_tokens = fetch_pp_tokens;
  save_suppress_keyword_recognition = suppress_keyword_recognition;
  /* Set the values required for attribute scanning. */
  expand_macros = FALSE;
  do_string_literal_concatenation = TRUE;
  fetch_pp_tokens = FALSE;
  suppress_keyword_recognition = TRUE;
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
                                     strlen(sym_hdr->identifier));
    if (attr_descr == NULL) {
      /* An unknown attribute -- issue a diagnostic.  This is only a warning
         if we accept unrecognized attributes. */
      pos_st_diagnostic(accept_unrecognized_attributes ? es_warning : es_error,
                        ec_unrecognized_attribute, &pos_curr_token,
                        sym_hdr->identifier);
    }  /* if */
    /* Bypass the identifier. */
    (void)get_token();
  }  /* if */
  if (attr_descr == NULL) attr_descr = unrecognized_attribute;
  return attr_descr;
}  /* look_up_attribute */


static char *get_string_for_token(a_boolean	*err)
/*
If the current token is an identifier or string literal, this routine
copies the characters of the token static buffer and converts them to lower
case.  A pointer to the static buffer is returned.  If the current token
is something else, a NULL pointer is returned.  If the token is a string
literal, but the constant is an error constant, "err" is set to TRUE.
Note that "err" is not TRUE for an unexpected token kind.
*/
{
  char		*src = NULL;
  a_boolean	valid_token = TRUE;
  char		*result = NULL;

  *err = FALSE;
  /* Copy the characters into a buffer, converting any upper case characters
     to lower case.  Allocate a buffer on the first call of this routine. */
  if (ms_attr_buffer == NULL) ms_attr_buffer = alloc_text_buffer(32);
  reset_text_buffer(ms_attr_buffer);
  /* The source of the characters to be copied depends on the kind of token
     provided. */
  if (curr_token == tok_identifier) {
    src = locator_for_curr_id.symbol_header->identifier;
  } else if (curr_token == tok_string_literal) {
    if (is_error_constant(&const_for_curr_token)) {
      /* We encountered a misformed string literal.  An error should
         have been issued already. */
      check_assertion(total_errors != 0);
      *err = TRUE;
    } else {
      src = const_for_curr_token.variant.string.value;
    }  /* if */
  } else {
    /* Some other token kind */
    valid_token = FALSE;
  }  /* if */
  /* Copy the token to the buffer. */
  if (src != NULL) {
    /* Copy the characters to the text buffer, converting them to lower
       case. */
    while (*src != '\0') {
      char ch = *src++;
      if (isalpha((unsigned char)ch)) ch = tolower(ch);
      add_char_to_text_buffer(ms_attr_buffer, ch);
    }  /* while */
    /* Add a null terminator. */
    add_char_to_text_buffer(ms_attr_buffer, '\0');
    result = ms_attr_buffer->buffer;
  }  /* if */
  /* For a valid token kind, bypass the token. */
  if (valid_token) (void)get_token();
  return result;
}  /* get_string_for_token */


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
  return value;
}  /* scan_ms_attribute_integer_arg */


static char *scan_ms_attribute_string_arg(void)
/*
Scan a string argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  char	*value = NULL;

  if (curr_token != tok_string_literal) {
    /* An string constant is required. */
    syntax_error(ec_exp_string_literal);
  } else {
    /* Make a copy of the string in IL memory. */
    char	*src;
    sizeof_t	length;
    src = const_for_curr_token.variant.string.value;
    /* Subtract one to exclude the null terminator. */
    length = (sizeof_t)const_for_curr_token.variant.string.length-1;
    value = copy_string_of_length_to_region(FILE_SCOPE_REGION_NUMBER,
                                            src, length);
    /* Bypass the string literal. */
    (void)get_token();
  }  /* if */
  return value;
}  /* scan_ms_attribute_string_arg */


static a_boolean scan_ms_attribute_boolean_arg(
					an_ms_attribute_param_ptr	param)
/*
Scan a boolean argument of a Microsoft attribute.  Return the value
scanned.
*/
{
  a_boolean		result = FALSE;
  a_source_position	arg_pos = pos_curr_token;
  a_boolean		err;
  char			*string_for_token;

  string_for_token = get_string_for_token(&err);
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
that identifiers the element of the enumeration that was specified.
The first entry on the list is 1.  Zero is returned if no matching
entry was found.

The value specified may or may not be enclosed in quotes.  Case is not
significant.
*/
{
  int			result = 0;
  a_source_position	arg_pos = pos_curr_token;
  a_boolean		err;
  char			*string_for_token;

  string_for_token = get_string_for_token(&err);
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
      result = values - param->values;
    } else {
      /* An invalid value.  Issue a diagnostic. */
      pos_st_error(ec_invalid_ms_attr_enum_value, &arg_pos, param->name);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_ms_attribute_enum_arg */


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
      arg->variant.uuid_string = scan_GUID_string();
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
      arg->variant.string = scan_ms_attribute_string_arg();
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
an error and return NULL pointer.

attr_descr describes the attribute being scanned.  arg_list is the list of
arguments scanned so far, and is used to detect a duplicated argument.
*/
{
  char				*param_name;
  an_ms_attribute_param_ptr	msapp = NULL;
  a_boolean			err;
  a_source_position		name_pos = pos_curr_token;

  /* Get the lower case string for the parameter name. */
  param_name = get_string_for_token(&err);
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
      pos_st2_error(ec_invalid_attr_name, &name_pos, attr_descr->name,
                    param_name);
    } else {
      /* Check for a repeated argument. */
      an_ms_attribute_arg_ptr	msaap;
      for (msaap = arg_list; msaap != NULL; msaap = msaap->next) {
        if (strcmp(msaap->param_name, param_name) == 0) {
          /* The argument has already been given a value. */
          pos_st_error(ec_duplicate_attr_arg, &name_pos, param_name);
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
the "=" that precedes a single argument or the "(" the precedes an argument
list.  Return a pointer to the list of arguments.
*/
{
  an_ms_attribute_param_ptr	param;
  an_ms_attribute_arg_ptr	arg_list = NULL;
  an_ms_attribute_arg_ptr	arg_tail = NULL;
  a_boolean			any_named_args = FALSE;

  param = attr_descr->parameters;
  if (curr_token == tok_assign) {
    /* Some attributes accept "attr=x" style references, which has the effect
       of providing a value for the initial argument. */
    if (!attr_descr->initialization_style_arg_allowed) {
      /* This style of argument is not permitted for this attribute. */
      str_error(ec_cannot_assign_to_attribute, attr_descr->name);
      flush_tokens();
    } else {
      /* Scan the argument associated with the initial parameter. */
      /* Bypass the "=" */
      (void)get_token();
      arg_list = scan_ms_attribute_arg(param);
    }  /* if */
  } else {
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
        /* A position argument cannot follow a named one. */
        error(ec_positional_after_named);
        /* Flush to the next argument. */
        flush_tokens();
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
  return arg_list;
}  /* scan_ms_attribute_arg_list */


static void scan_unrecognized_ms_attribute_arg_list(void)
/*
Scan the arguments of an unrecognized Microsoft attribute reference.  The
current token is the "=" that precedes a single argument or the "(" the
precedes an argument list.  Because the attribute is unrecognized, we don't
know the form of the parameter list expected.  This routine just scans tokens
until the end of the attribute is found.
*/
{
  /* The stop tokens should be set appropriately so that this will flush
     to the "," that separates attributes, or to the closing "]" of the
     attribute block. */
  flush_tokens();
}  /* scan_unrecognized_ms_attribute_arg_list */


static an_ms_attribute_ptr scan_ms_attribute(a_boolean	is_parameter)
/*
Scan a single Microsoft attribute of an attribute block that may contain
multiple attributes.  Return a pointer to the attribute entry that represents
the attribute.

is_parameter is TRUE if the attribute is part of a function parameter
declaration.
*/
{
  an_ms_attribute_kind_descr_ptr	attr_descr;
  a_token_sequence_number		first_token;
  a_token_sequence_number		last_token;
  a_source_position			start_position;
  an_ms_attribute_ptr			attr = NULL;

  /* Save the token sequence number of the first token of this attribute. */
  first_token = curr_token_sequence_number;
  start_position = pos_curr_token;
  /* Look up the attribute identifier.  If the identifier is unknown,
     the "unrecognized" attribute will be returned.  In error cases, such
     as a missing attribute name, a NULL attribute description is returned. */
  attr_descr = look_up_attribute();
  if (attr_descr != NULL) {
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
      add_to_source_sequence_list((char *)attr,
                                  (an_il_entry_kind)iek_ms_attribute);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Look for an argument list.  We do this even for attributes without
       parameters, for error recovery purposes. */
    if (curr_token == tok_assign || curr_token == tok_lparen) {
      if (attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
        /* We are scanning an unrecognized attribute, so we don't know the
           form of the expected parameters. */
        scan_unrecognized_ms_attribute_arg_list();
      } else {
        /* The attribute is of a known kind.  The argument list can be
           scanned with knowledge of the associated parameters. */
        attr->arg_list = scan_ms_attribute_arg_list(attr_descr);
      }  /* if */
    } else if (curr_token != tok_comma && curr_token != tok_rbracket) {
      /* The attribute name was not followed by anything that looks like
         an argument, nor was it followed by anything that looks like an
         attribute separator or end of an attribute list. */
      if (attr_descr->parameters != NULL ||
          attr->kind == (an_ms_attribute_kind)msak_unrecognized) {
        /* An attribute for which we expected a parameter list.  Complain
           of an expected argument list. */
        str_error(ec_exp_attr_arg_list, attr->name);
        flush_tokens();
      } else {
        /* No parameters were expected.  Complain of a missing "," or "]". */
        syntax_error(ec_exp_comma_or_rbracket);
      }  /* if */
    }  /* if */
    /* Save the position of the token following the attribute. */
    last_token = curr_token_sequence_number;
    /* Create the string version of the attribute. */
    init_token_string(&start_position);
    add_token_cache_segment_to_string(&attribute_cache, first_token,
                                      last_token);
    /* Copy the string to IL memory. */
    attr->string = make_copy_of_token_string();
    /* Add the attribute to the IL. */
    add_to_ms_attributes_list(attr, decl_scope_level);
#if DEBUG
    if (db_flag_is_set("msattr")) {
      fprintf(f_debug, "Attribute string: %s\n", attr->string);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
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
        attr_tail->next_in_block = attr;
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


void apply_microsoft_attributes(an_ms_attribute_ptr	*attributes,
				a_source_correspondence	*scp,
				an_ms_attribute_target	target)
/*
This routine is used to indicate that the list of Microsoft attributes
specified by "attributes" should apply to the entity specified by "scp".
The attributes must apply to the entity kind specified by "target".
*/
{
  an_ms_attribute_ptr	msap;

  if (scp != NULL) scp->has_associated_attribute = TRUE;
  /* Check whether the attributes have the appropriate target. */
  for (msap = *attributes; msap != NULL; msap = msap->next) {
    if (msap->kind_descr->target != target &&
        msap->kind_descr->target != msat_none) {
       if (msap->kind_descr->target == msat_standalone) {
         pos_st_error(ec_invalid_use_of_standalone_attr, &msap->position,
                      msap->name);
       } else {
         pos_st_error(ec_invalid_use_of_attr, &msap->position, msap->name);
       }  /* if */
    }  /* if */
  }  /* for */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* apply_microsoft_attributes */


void verify_standalone_attributes(an_ms_attribute_ptr	*attributes)
/*
This routine is used to verify that the list of Microsoft attributes
specified by "attributes" contains only standalone attributes.
*/
{
  an_ms_attribute_ptr	msap;

  for (msap = *attributes; msap != NULL; msap = msap->next) {
    if (msap->kind_descr->target != msat_standalone &&
        msap->kind_descr->target != msat_none) {
       pos_st_error(ec_invalid_use_of_attr, &msap->position, msap->name);
    }  /* if */
  }  /* for */
  /* Clear the attribute list pointer passed by the caller. */
  *attributes = NULL;
}  /* verify_standalone_attributes */


#if DEBUG

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
  accept_unrecognized_attributes = TRUE;
  unrecognized_attribute = NULL;
  ms_attr_buffer = NULL;
#if DEBUG
  num_ms_attribute_kind_descrs_allocated = 0;
  num_ms_attribute_params_allocated = 0;
#endif /* DEBUG */
  memzero((char *)attribute_lookup_table, sizeof(attribute_lookup_table));
  curr_attribute_descr = NULL;
  /* Build the structure used to describe the various attributes. */
  init_attribute_kinds();
}  /* ms_attrib_init */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2003 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
