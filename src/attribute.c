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

attribute.c -- Processing of attributes, a GCC extension.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if GNU_EXTENSIONS_ALLOWED

/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"
#include "layout.h"


/*
The "alias" attribute can refer to entities that are declared later in a
translation unit.  Therefore, we record such attributes in a fixup list and
process the list at the end of the translation unit.
*/

typedef struct an_alias_fixup *an_alias_fixup_ptr;
typedef struct an_alias_fixup {
  an_alias_fixup_ptr
		next;	/* Pointer to the next fixup to process. */
  a_symbol_ptr	alias;
			/* The symbol that is an alias for another entity. */
  char*		aliased_name;
			/* The name of the entity being aliased. */
  a_source_position
		alias_position;
			/* The position of the alias attribute (used for error
			   reporting purposes). */
} an_alias_fixup;

/* Pointer to the head of the list of alias fixups. */
static an_alias_fixup_ptr
	alias_fixup_list;

/* Pointer to a list of available (freed) alias fixups. */
static an_alias_fixup_ptr
	avail_alias_fixups;


static void add_alias_fixup(a_symbol_ptr        alias,
                            char*               aliased_name,
                            a_source_position*  alias_position)
/*
Allocate a fixup entry for a new alias described by the given parameters.
*/
{
  an_alias_fixup_ptr  entry;

  if (avail_alias_fixups != NULL) {
    entry = avail_alias_fixups;
    avail_alias_fixups = avail_alias_fixups->next;
  } else {
    entry = (an_alias_fixup_ptr)alloc_fe(sizeof(an_alias_fixup));
  }  /* if */
  entry->next = alias_fixup_list;
  alias_fixup_list = entry;
  entry->alias = alias;
  entry->aliased_name = aliased_name;
  entry->alias_position = *alias_position;
}  /* add_alias_fixup */


static void free_alias_fixup(an_alias_fixup_ptr  entry)
/*
Return the given entry to the list of available entries.
*/
{
  entry->next = avail_alias_fixups;
  avail_alias_fixups = entry->next;
}  /* free_alias_fixup */


void process_alias_fixup_list(void)
/*
Traverse the list of alias fixups and set the alias fields as needed.
*/
{
  an_alias_fixup_ptr  entries = alias_fixup_list, entry;
  a_symbol_ptr        aliased_sym;
  a_symbol_locator    locator;

  while (entries != NULL) {
    entry = entries;
    entries = entries->next;
    (void)find_symbol(entry->aliased_name,
                      (sizeof_t)strlen(entry->aliased_name), &locator);
    aliased_sym = locator.symbol_header->inactive_symbols;
    for (; aliased_sym != NULL; aliased_sym = aliased_sym->next) {
      if (aliased_sym->decl_scope == FILE_SCOPE_NUMBER) {
        break;
      }  /* if */
    }  /* for */
    if (entry->alias->defined) {
      /* An entity cannot have a definition and simultaneously be an alias for
         another entity. */
      pos_error(ec_alias_cannot_have_definition, &entry->alias->decl_position);
    }  /* if */
    if (aliased_sym == NULL) {
      pos_st_error(ec_aliased_name_undeclared,
                   &entry->alias_position, entry->aliased_name);
    } else if (aliased_sym->kind != entry->alias->kind) {
      pos_sy_error(ec_aliased_name_bad_kind,
                   &entry->alias->decl_position, aliased_sym);
    } else {
      switch (entry->alias->kind) {
        case sk_routine:
          entry->alias->variant.routine.ptr->aliased_routine =
                                              aliased_sym->variant.routine.ptr;
          break;
        case sk_variable:
          entry->alias->variant.variable.ptr->aliased_variable =
                                             aliased_sym->variant.variable.ptr;
          break;
        default:
          unexpected_condition();
      }  /* switch */
      /* The aliased entity is referenced in the alias specification; only
	 now, however, do we know the symbol to mark it as referenced. */
      mark_referenced(aliased_sym, &entry->alias->decl_position);
    }  /* if */
    free_alias_fixup(entry);
  }  /* while */
}  /* process_alias_fixup_list */


/* Needed because of forward references: */
static a_type_ptr copy_type_and_apply_attributes(an_attribute_ptr attributes,
                                                 a_type_ptr       tp,
                                                 a_boolean        is_typedef);

/* Previously allocated attributes available for reuse. */
static an_attribute_ptr avail_attributes;


static an_attribute_ptr alloc_attribute(an_attribute_kind  kind,
                                        a_source_position  *pos)
/*
Allocate an attribute of the indicated kind, initialize its fields,
and return a pointer to it.  "pos" gives the source position to
associate with the attribute.  It is copied here, so the memory
pointed to be "pos" can be freed when this routine returns.
*/
{
  an_attribute_ptr ap;

  if (avail_attributes != NULL) {
    /* Reuse a previously allocated attribute. */
    ap = avail_attributes;
    avail_attributes = avail_attributes->next;
  } else {
    /* Allocate memory for a new attribute. */
    ap = (an_attribute_ptr)alloc_fe(sizeof(an_attribute));
  }  /* if */
  ap->kind = kind;
  ap->next = NULL;
  copy_source_position(*pos, ap->position);
  switch (kind) {
    case ak_mode:
      ap->variant.mode = (an_attribute_kind)tmk_error;
      break;
#if USER_CONTROL_OF_STRUCT_PACKING
    case ak_aligned:
      ap->variant.alignment = 0;
      break;
    case ak_packed:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    case ak_unused:
    case ak_constructor:
    case ak_destructor:
    case ak_noreturn:
    case ak_pure:
    case ak_const:
    case ak_weak:
    case ak_malloc:
    case ak_nocommon:
    case ak_transparent_union:
#if GNU_NAKED_ATTRIBUTE_ALLOWED
    case ak_naked:
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
    case ak_no_instrument_function:
    case ak_no_check_memory_usage:
#if GNU_X86_ATTRIBUTES_ALLOWED
    case ak_stdcall:
    case ak_cdecl:
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
      break;
    case ak_section:
      ap->variant.section = NULL;
      break;
    case ak_alias:
      ap->variant.alias = NULL;
      break;
    case ak_format:
      ap->variant.format.kind = (a_format_attribute_kind)fak_none;
      ap->variant.format.fmt_arg = 0;
      ap->variant.format.first_subst_arg = 0;
      break;
    case ak_format_arg:
      ap->variant.fmt_arg = 0;
      break;
    default:
      unexpected_condition_str("alloc_attribute: bad kind");
  }  /* switch */

  return ap;
}  /* alloc_attribute */


an_attribute_ptr copy_attribute_list(an_attribute_ptr attributes)
/* 
Return a copy of the complete attribute list.
*/
{
  an_attribute_ptr copy = NULL;
  an_attribute_ptr *end = &copy;

  while (attributes != NULL) {
    *end = alloc_attribute(attributes->kind, &attributes->position);
    switch (attributes->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_packed:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_unused:
      case ak_constructor:
      case ak_destructor:
      case ak_noreturn:
      case ak_pure:
      case ak_const:
      case ak_weak:
      case ak_malloc:
      case ak_nocommon:
      case ak_transparent_union:
#if GNU_NAKED_ATTRIBUTE_ALLOWED
      case ak_naked:
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
      case ak_no_instrument_function:
      case ak_no_check_memory_usage:
#if GNU_X86_ATTRIBUTES_ALLOWED
      case ak_stdcall:
      case ak_cdecl:
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
        /* No variant fields. */
        break;
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
        (*end)->variant.alignment = attributes->variant.alignment;
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_mode:
        (*end)->variant.mode = attributes->variant.mode;
        break;
      case ak_section:
        (*end)->variant.section = attributes->variant.section;
        break;
      case ak_alias:
        (*end)->variant.alias = attributes->variant.alias;
        break;
      case ak_format:
        (*end)->variant.format = attributes->variant.format;
        break;
      case ak_format_arg:
        (*end)->variant.fmt_arg = attributes->variant.fmt_arg;
        break;
      default:
        unexpected_condition_str("copy_attribute_list: bad kind");
        break;
    }  /* switch */
    attributes = attributes->next;
    end = &(*end)->next;
  }  /* while */

  return copy;
}  /* copy_attribute_list */


void free_attribute_list(an_attribute_ptr  ap)
/*
Free the storage associated with the entire list of attributes given by
ap.  ap may be NULL.
*/
{
  an_attribute_ptr  last;

  if (ap != NULL) {
    /* Find the last attribute in the list. */
    for (last = ap; last->next != NULL; last = last->next) {}
    /* Add the entire list of attributes to the front of the free list. */
    last->next = avail_attributes;
    avail_attributes = ap;
  }  /* if */
}  /* free_attribute_list */


an_attribute_ptr *last_attribute_link(an_attribute_ptr *attributes)
/*
Return the address of the last "next" pointer in the list given by
"*attributes". ( If "*attributes" is NULL, return "attributes".)
If "attributes" itself is NULL, then return NULL.
*/
{
  if (attributes != NULL) {
    while (*attributes != NULL) {
      attributes = &(*attributes)->next;
    }  /* while */
  }  /* if */

  return attributes;
}  /* last_attribute_link */


static a_host_large_integer scan_integral_argument(a_boolean *err,
                                                   a_boolean *ovflo)
/*
Scan an integral attribute argument and return the value.  If an error
occurs during the scan, *err is set to TRUE, and an error message is
issued.  If the conversion to a host integer overflows, *ovflo is set
to TRUE, but no error message is issued.
*/
{
  a_constant            constant;
  a_host_large_integer  value = 0;

  *err = *ovflo = FALSE;
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
    *err = TRUE;
  } else {
    value = value_of_integer_constant(&constant, ovflo);
  }  /* if */

  return value;
}  /* scan_integral_argument */


static a_boolean scan_attribute_arguments(an_attribute_ptr  attribute)
/*
Scan the arguments to an attribute, and store them in the attribute
provided.  Returns FALSE if the arguments are so erroneous that the
entire attribute should be discarded.  Called only for attributes
that do take arguments.
*/
{
  char               *name;
  int                i;
  a_source_position  pos;
  a_boolean          result = TRUE;

  pos = error_position;
  /* Different kinds of attributes take different kinds of 
     arguments.  */
  switch (attribute->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
    case ak_aligned:
      { a_host_large_integer  alignment;
        a_boolean             ovflo;
        a_boolean             error_occurred;
        /* The "__aligned__" attribute takes one argument, which is a
           constant expression indicating the desired alignment.  Scan the
           expression. */
        alignment = scan_integral_argument(&error_occurred, &ovflo);
        if (!error_occurred) {
          /* If the value isn't reasonable, issue an error message. */
          if (ovflo || 
              !check_pack_alignment_value(alignment,
                                          &attribute->variant.alignment)) {
            pos_error(ec_bad_attribute_alignment, &pos);
            error_occurred = TRUE;
          }  /* if */
        }  /* if */
        /* If something went wrong, use the maximum valid alignment
           value so that redundant error messages are not issued. */
        if (error_occurred) {
          attribute->variant.alignment = targ_maximum_pack_alignment;
        }  /* if */
      }
      break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    case ak_mode:
      /* Look for an identifier corresponding to the mode. */
      if (curr_token != tok_identifier) {
        goto error;
      }  /* if */
      /* Get the name of the mode. */
      name = locator_for_curr_id.symbol_header->identifier;
      /* Consume the name. */
      (void)get_token();
      /* Look it up. */
      for (i = (int)tmk_first; i < (int)tmk_last; ++i) {
        if (same_string_ignoring_underscores(type_mode_kind_names[i], 
                                             name)) {
          break;
        }  /* if */
      }  /* for */
      /* If it wasn't in the table, it might be one of the special
         "byte", "word", or "pointer" values. */
      if (i == (int)tmk_last) {
        if (same_string_ignoring_underscores("byte", name)) {
          i = (int)tmk_QI;
        } else if (same_string_ignoring_underscores("word", name)) {
          i = (int)targ_word_mode;
#if TARG_ALL_POINTERS_SAME_SIZE
        } else if (same_string_ignoring_underscores("pointer", name)) {
          i = (int)targ_pointer_mode;
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
        }  /* if */
      }  /* if */
      /* If the mode was not valid, issue an error message. */
      if (i == (int)tmk_last) {
        goto error;
      }  /* if */
      attribute->variant.mode = (a_type_mode_kind)i;
      break;
    case ak_section:
    case ak_alias:
      /* Look for a string-literal giving the section or alias
         name. */
      if (curr_token != tok_string_literal) {
        result = FALSE;
        goto error;
      }  /* if */
      /* If there was an error in parsing the string, we do not need
         to issue another error here. */
      if (is_error_constant(&const_for_curr_token)) {
        result = FALSE;
        break;
      }  /* if */
      /* For both the section and alias attributes, GCC accepts string
         literals with embedded NULs (like "ab\0c") but ignores
         everything after the "\0".  So, storing the attribute
         argument as a character pointer, without a length, gives 
         compatibility with GCC. */
      if (attribute->kind == (an_attribute_kind)ak_section) {
        attribute->variant.section = const_for_curr_token.variant.string.value;
      } else {
        check_assertion(attribute->kind == (an_attribute_kind)ak_alias);
        attribute->variant.alias = const_for_curr_token.variant.string.value;
      }  /* if */
      /* Consume the string literal. */
      (void)get_token();
      break;
    case ak_format:
      { a_host_large_integer  param_number;
        a_boolean             error_occurred;
        a_boolean             ovflo;
        /* If things go well, result will be reset to TRUE. */
        result = FALSE;
        /* Scan the identifier indicating the format kind. */
        if (curr_token != tok_identifier) goto error;
        name = locator_for_curr_id.symbol_header->identifier;
        /* Bypass the identifier. */
        (void)get_token();
        /* Look up the format name. */
        for (i = (int)fak_first; i < (int)fak_last; i++) {
          if (same_string_ignoring_underscores(format_attribute_kind_names[i],
                                               name)) {
            break;
          }  /* if */
        }  /* for */
        if (i == (int)fak_last) goto error;
        attribute->variant.format.kind = (a_format_attribute_kind)i;
        /* Read the arguments that indicate the format string argument
           and the start of the variable arguments. */
        for (i = 0; i < 2; i++) {
          /* Look for the "," that separates the next argument from
             this one. */
          if (!required_token(tok_comma, ec_exp_comma)) {
            /* An error message will already have been issued. */
            goto done;
          }  /* if */
          /* Scan the argument number. */
          param_number = scan_integral_argument(&error_occurred, &ovflo);
          /* If there was no integer constant, a message has already
             been issued. */
          if (error_occurred) goto done;
          /* For overflow, issue the message now. */
          if (ovflo || param_number < 0 || param_number > INT_MAX) {
            goto error;
          }  /* if */
          /* Remember the value. */
          if (i == 0) {
            attribute->variant.format.fmt_arg = (int)param_number;
          } else {
            attribute->variant.format.first_subst_arg = (int)param_number;
          }  /* if */
        }  /* for */
        /* All went well. */
        result = TRUE;
      }
      break;
    case ak_format_arg:
      { a_host_large_integer  param_number;
        a_boolean             error_occurred;
        a_boolean             ovflo;
        /* If things go well, result will be reset to TRUE. */
        result = FALSE;
        /* Scan the argument number. */
        param_number = scan_integral_argument(&error_occurred, &ovflo);
        /* If there was no integer constant, a message has already
           been issued. */
        if (error_occurred) goto done;
        /* For overflow, issue the message now. */
        if (ovflo || param_number < 0 || param_number > INT_MAX) {
          goto error;
        }  /* if */
        /* Remember the value. */
        attribute->variant.fmt_arg = param_number;
        /* All went well. */
        result = TRUE;
      }
      break;
    default:
      unexpected_condition();
  }  /* switch */
  goto done;

error:
  pos_st_error(ec_invalid_argument_to_attribute,
               &pos, attribute_kind_names[(int)attribute->kind]);
  flush_tokens();

done:
  return result;
}  /* scan_attribute_arguments */


static an_attribute_ptr *scan_attribute_list(an_attribute_ptr *next)
/*
Scan an (optional) list of attributes.  The syntax varies with the
particular attribute.  In general, there are two forms:
 
  identifier 
  identifier ( arguments )

Specifically, these attributes take no arguments:

  packed
  constructor
  destructor
  unused
  noreturn
  pure
  const
  weak
  malloc
  nocommon
  transparent_union
  naked
  no_instrument_function
  no_check_memory_usage
  cdecl
  stdcall

These attributes take arguments:

  mode ( machine-mode )
  aligned ( constant-expression )
  section ( string-literal )
  alias ( string-literal )
  format ( identifier, constant-expression, constant-expression )
  format_arg ( constant-expression )

The attributes are appended at the location pointed to by next.  This
function returns the address of the last attribute.
*/
{
  an_attribute_ptr      attribute;
  char                  *attribute_name;
  an_attribute_kind     attribute_kind;
  int                   i;
  a_source_position     pos;

  /* Keep going until there are no more attributes. */
  do {
    /* The next token should be the name of an attribute. */
    if (curr_token != tok_identifier && curr_token != tok_const) {
      error(ec_exp_attribute_name);
    } else {
      /* Remember the location of the attribute name.  This is the
         source position that we associate with the attribute. */
      copy_source_position(error_position, pos);
      if (curr_token == tok_const) {
        /* The const attribute is spelled the same as a keyword. */
        attribute_name = "const";
      } else {
        /* Get the name of the attribute. */
        attribute_name = locator_for_curr_id.symbol_header->identifier;
      }  /* if */
      /* Look up the attribute name. */
      for (i = (int)ak_first; i < (int) ak_last; i++) {
        if (same_string_ignoring_underscores(attribute_kind_names[i],
                                             attribute_name)) {
          break;
        }  /* if */
      }  /* for */
      attribute_kind = (an_attribute_kind)i;
      if (attribute_kind == (an_attribute_kind)ak_last) {
        /* If the attribute name was not recognized issue a warning. */
        str_warning(ec_unrecognized_attribute, attribute_name);
        attribute_kind = (an_attribute_kind)ak_error;
        attribute = NULL;
      } else {
        /* Create a new attribute. */
        attribute = alloc_attribute(attribute_kind, &pos);
      }  /* if */
      /* Bypass the attribute name and check if it is followed by arguments. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        /* There are arguments to the attribute. */
        /* Bypass the lparen. */
        (void)get_token();
        add_stop_token(tok_rparen);
        switch (attribute_kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
          case ak_aligned:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
          case ak_mode:
          case ak_section:
          case ak_alias:
          case ak_format:
          case ak_format_arg:
            if (!scan_attribute_arguments(attribute)) {
              /* If the arguments were erroneous, it sometimes makes
                 sense to ignore the attribute completely so that we
                 do not issue spurious errors later. */
              attribute_kind = (an_attribute_kind)ak_error;
              free_attribute_list(attribute);
            }  /* if */
            break;
          case ak_error:
            /* Skip over the arguments. */
            flush_tokens();
            break;
          default:
            /* There should not have been an argument. */
            str_error(ec_arguments_provided_for_attribute,
                      attribute_name);
            /* Skip over the arguments. */
            flush_tokens();
            break;
        }  /* switch */
        /* Look for the closing rparen. */
        (void)required_token(tok_rparen, ec_exp_rparen);
        remove_stop_token(tok_rparen);
      } else {
        /* No arguments are provided for this attribute. */
        switch (attribute_kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
          case ak_packed:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
          case ak_unused:
          case ak_constructor:
          case ak_destructor:
          case ak_error:
          case ak_noreturn:
          case ak_pure:
          case ak_const:
          case ak_weak:
          case ak_malloc:
          case ak_nocommon:
          case ak_transparent_union:
#if GNU_NAKED_ATTRIBUTE_ALLOWED
          case ak_naked:
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
          case ak_no_instrument_function:
          case ak_no_check_memory_usage:
#if GNU_X86_ATTRIBUTES_ALLOWED
          case ak_stdcall:
          case ak_cdecl:
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
            /* These attributes do not take arguments. */
            break;
#if USER_CONTROL_OF_STRUCT_PACKING
          case ak_aligned:
            /* If there is no argument to the "aligned" attribute, then
               the maximum alignment useful on the target is implied. */
            attribute->variant.alignment = targ_maximum_intrinsic_alignment;
            break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
          default:
            str_error(ec_arguments_required_for_attribute,
                      attribute_name);
            break;
        }  /* switch */
      } /* if */
      if (attribute_kind != (an_attribute_kind)ak_error) { 
        /* Add the attribute to the list. */
        *next = attribute;
        /* The new attribute is now the last entry in the list. */
        next = &attribute->next;
      }  /* if */
    }  /* if */
    if (curr_token != tok_comma && curr_token != tok_rparen) {
      /* If an error occurs, skip tokens until we reach the start of
         the next attribute, or the end of the attribute list. */
      add_stop_token(tok_comma);
      syntax_error(ec_exp_comma);
      remove_stop_token(tok_comma);
    }  /* if */
  } while (loop_token(tok_comma));

  return next;
}  /* scan_attribute_list */


an_attribute_ptr scan_attributes(void)
/*
Scan an (optional) series of attributes.  Each has the form:

  __attribute__ (( attribute-list [opt] ))

This function returns a list of all of the attributes in the order
that they appeared.  
*/
{
  an_attribute_ptr  attributes = NULL;
  an_attribute_ptr  *next_attribute;

  /* The next_attribute will be the first one in the list. */
  next_attribute = &attributes;
  /* Keep going until there are no more attributes. */
  while (curr_token == tok_attribute) {
    /* Bypass "attribute". */
    (void)get_token();
    /* There should now be two left parens. */
    (void)required_token(tok_lparen, ec_exp_lparen);
    (void)required_token(tok_lparen, ec_exp_lparen);
    add_stop_token(tok_rparen);
    if (curr_token == tok_rparen) {
      /* Presumably an empty attribute. */
    } else {
      /* Scan the attribute-list and attach it to the list we already have. */
      next_attribute = scan_attribute_list(next_attribute);
    }  /* if */
    /* There should now be two right parens. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  }  /* while */
  return attributes;
}  /* scan_attributes */


a_type_ptr get_type_with_mode(a_type_ptr        type,
                              a_type_mode_kind  mode,
                              a_source_position *pos)
/*
Return a type, similar to the type provided, but with the indicated
machine mode.  The source position at which any errors should be
emitted is given by pos.
*/
{
  an_integer_kind  ikind;
  a_float_kind     fkind;
  a_type_kind      type_kind;
  a_targ_size_t    size;

  switch (mode) {
    case tmk_QI:
      type_kind = (a_type_kind)tk_integer;
      size = 1;
      break;
    case tmk_HI:
      type_kind = (a_type_kind)tk_integer;
      size = 2;
      break;
    case tmk_SI:
      type_kind = (a_type_kind)tk_integer;
      size = 4;
      break;
    case tmk_DI:
      type_kind = (a_type_kind)tk_integer;
      size = 8;
      break;
    case tmk_TI:
      type_kind = (a_type_kind)tk_integer;
      size = 16;
      break;
    case tmk_SF:
      type_kind = (a_type_kind)tk_float;
      size = 4;
      break;
    case tmk_DF:
      type_kind = (a_type_kind)tk_float;
      size = 8;
      break;
    case tmk_XF:
      type_kind = (a_type_kind)tk_float;
      size = 12;
      break;
    case tmk_TF:
      type_kind = (a_type_kind)tk_float;
      size = 16;
      break;
    case tmk_error:
      type_kind = (a_type_kind)tk_error;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* If the mode was erroneous, we can just return the original type.
     Otherwise, find a type with the appropriate mode. */
  if (type_kind != (a_type_kind)tk_error) {
    a_type_qualifier_set qualifiers;
    /* Remember the type qualifiers so that we can create an identically
       qualified copy. */
    qualifiers = get_type_qualifiers(type);
    /* Check to see that the type implied by the mode matches the type
     of the variable. */
    type = skip_typerefs(type);
    if (type->kind != type_kind) {
      pos_ty_error(ec_mode_incompatible_with_type, pos, type);
    } else if (type->kind == (a_type_kind)tk_integer) {
      ikind = int_kind_for_bit_size((unsigned int)(size * targ_char_bit),
                                    is_signed_integral_type(type));
      if (ikind == (an_integer_kind)ik_none) {
        pos_error(ec_no_type_of_specified_width, pos);
      } else {
        type = integer_type(ikind);
      }  /* if */
    } else {
      for (fkind = (a_float_kind)0;
           fkind < (a_float_kind)fk_last;
           fkind = (a_float_kind)((int)fkind + 1)) {
        if (float_type(fkind)->size == size) {
          break; 
        }  /* if */
      }  /* for */
      if (fkind == (a_float_kind)fk_last) {
        pos_error(ec_no_type_of_specified_width, pos);
      } else {
        type = float_type(fkind);
      }  /* if */
    }  /* if */
    /* The new type should have the same qualifiers as the original. */
    type = make_qualified_type(type, qualifiers);
  }  /* if */

  return type;
}  /* get_type_with_mode */


a_type_ptr apply_attributes_to_variable_type(an_attribute_ptr  attributes,
                                             a_type_ptr        type)
/*
A variable or field is being declared with the indicated type.  The
attributes apply to the variable.  Return the type, appropriately
adjusted for the attributes.  Diagnostics are not issued for invalid
attributes.  */
{
  an_attribute_ptr ap;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
        /* The aligned attribute is handled by setting the alignment
           field in the variable directly, not by modifying the type of
           the variable. */
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_mode:
        /* The __mode__ attribute is used to specify the width of an
           integer, pointer, or floating-point type independent of the
           type-specifier used.  For example,

             char i __attribute__((__mode__(SImode)));

           is precisely the same as:

             int i;

           on a machine where sizeof(int) == 4. */
        type = get_type_with_mode(type, ap->variant.mode, &ap->position);
        break;
      case ak_noreturn:
      case ak_const:
        /* GCC allows "noreturn" and "const" to apply to variables
           with pointer-to-function type.  GCC does not accept "pure"
           in this context, even though it is conceptually similar. */
        {
          an_attribute_ptr next;
          /* Temporarily remove "ap" from the attributes list so that
             we can use copy_type_and_apply_attributes. */
          next = ap->next;
          ap->next = (an_attribute_ptr)NULL;
          type = copy_type_and_apply_attributes(ap, type, 
                                                /*is_typedef=*/FALSE);
          /* Restore the attribute list. */
          ap->next = next;
        }
        break;
      default:
        /* No action. */
        break;
    }  /* switch */
  }  /* for */
  return type;
}  /* apply_attributes_to_variable_type */


static a_boolean check_variable_not_local(a_variable_ptr   variable,
                                          an_attribute_ptr attribute,
                                          a_boolean        allow_local_static)
/*
The attribute only applies to variables that are not local to a function.
If variable is not such a variable, issue an error and return FALSE.
Otherwise, return TRUE.  Local static variables are treated as non-local
when allow_local_static is TRUE.
*/
{
  a_boolean is_not_local = TRUE;

  if (variable->source_corresp.is_local_to_function &&
      /* Don't consider block extern declarations local. */
      variable->storage_class != (a_storage_class)sc_extern &&
      (!allow_local_static ||
       variable->storage_class != (a_storage_class)sc_static)) {
    pos_st_error(ec_attribute_does_not_apply_to_local_variable,
                 &attribute->position, 
                 attribute_kind_names[(int)attribute->kind]);
    is_not_local = FALSE;
  }  /* if */
  return is_not_local;
}  /* check_variable_not_local */


a_boolean check_transparent_union(a_type_ptr        tp,
                                  a_source_position *pos)
/*
"tp" is known to be an (immediate) union type.  Verify that it can be
transparent.  If not, issue a diagnostic and return FALSE.
*/
{
  a_field_ptr f;

  check_assertion(tp->kind == (a_type_kind)tk_union);
  /* Check to see that all members of the union have the same size
     as the union itself.  Otherwise, GCC does not permit the union
     to be transparent.  It seems that GCC looks at the type of the
     field, not the actual size -- for example, the size of
     bit fields is ignored. */
  for (f = tp->variant.class_struct_union.field_list;
       f != NULL;
       f = f->next) {
    if (skip_typerefs(f->type)->size != tp->size) {
      a_symbol_ptr sym = (a_symbol_ptr)f->source_corresp.assoc_info;
      if (sym != NULL && has_name(f)) {
        pos_syty_warning(ec_union_cannot_be_transparent_sym,
                         pos, sym, tp);
      } else {
        pos_ty2_warning(ec_union_cannot_be_transparent, pos,
                        tp, f->type);
      }  /* if */
      break;
    }  /* if */
  }  /* for */
  return f == NULL;
}  /* check_transparent_union */


void apply_attributes_to_variable(an_attribute_ptr  attributes,
                                  a_variable_ptr    vp)
/*
Apply the attributes to the indicated variable.  Issue diagnostics for
invalid attributes.
*/
{
  an_attribute_ptr  ap;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
        vp->alignment = ap->variant.alignment;
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_unused:
        /* Mark the variable as referenced in order to suppress warnings
           about it if it is unused. */
        vp->unused = TRUE;
        break;
      case ak_mode:
      case ak_noreturn:
      case ak_const:
        /* These attributes were handled in
           apply_attributes_to_variable_type. */
        break;
      case ak_weak:
        if (check_variable_not_local(vp, ap, /*allow_local_static=*/FALSE)) {
          vp->is_weak = TRUE;
        }  /* if */
        break;
      case ak_section:
        if (check_variable_not_local(vp, ap, /*allow_local_static=*/TRUE)) {
          vp->section = ap->variant.section;
        }  /* if */
        break;
      case ak_alias:
        if (check_variable_not_local(vp, ap, /*allow_local_static=*/FALSE)) {
          add_alias_fixup((a_symbol_ptr)vp->source_corresp.assoc_info,
                            ap->variant.alias, &ap->position);
        }  /* if */
        break;
      case ak_nocommon:
        if (check_variable_not_local(vp, ap, /*allow_local_static=*/TRUE)) {
          vp->is_not_common = TRUE;
        }  /* if */
        break;
      case ak_transparent_union:
        if (!vp->is_parameter) {
          pos_error(ec_transparent_variable, &ap->position); 
        } else if (!is_union_type(vp->type)) {
          pos_ty_error(ec_transparent_type_is_not_union,
                       &ap->position, vp->type);
        } else if (is_incomplete_type(vp->type)) {
          /* Parameters must already have complete types, and
             there is no point in complaining twice. */
        } else if (check_transparent_union(skip_typerefs(vp->type),
                                           &ap->position)) {
          /* Record the attribute. */
          check_assertion(vp->assoc_param_type != NULL);
          vp->assoc_param_type->is_transparent = TRUE;
        }  /* if */
        break;
      default:
        /* This attribute is not applicable to variables. */
        pos_sy_warning(ec_attribute_does_not_apply,
                       &ap->position,
                       (a_symbol_ptr)vp->source_corresp.assoc_info);
        break;
    }  /* switch */
  }  /* for */
}  /* apply_attributes_to_variable */


void apply_attributes_to_field(an_attribute_ptr attributes,
                               a_field_ptr      fp)
/* 
Apply the attributes to the indicated field.  Issue diagnostic
messages about any invalid attributes.
*/
{
  an_attribute_ptr  ap;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
      case ak_mode:
        /* This attribute was already handled in
           apply_attributes_to_variable_type. */
        break;
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
        /* Apply the specified alignment (which may be an increase or a
           decrease). */
        fp->alignment = ap->variant.alignment;
        break;
      case ak_packed:
        /* If a field is declared to be "packed", then it is aligned on
           a character boundary. */
        fp->alignment = 1;
        fp->is_packed = TRUE;
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      default:
        sym_warning(ec_attribute_does_not_apply,
                    (a_symbol_ptr)fp->source_corresp.assoc_info);
        break;
    }  /* switch */
  }  /* for */
}  /* apply_attributes_to_field */


static void ensure_routine_has_modifiable_type(a_routine_ptr  rp)
/*
Before applying an attribute to the type field of a routine, we must make
sure that that type is not a typedef (which could be shared with other
routines).  This makes a private copy of the underlying type is that is
the case.
*/
{
  if (rp->type->kind == (a_type_kind)tk_typeref &&
      typeref_is_typedef(rp->type)) {
    /* We cannot apply the attribute to the type underlying the
       typedef.  So make a copy of that type and apply the attribute
       to that. */
    rp->type = copy_type_and_apply_attributes((an_attribute_ptr)NULL,
                                              rp->type->variant.typeref.type,
                                              /*is_typedef=*/FALSE);
  }  /* if */
}  /* ensure_routine_has_modifiable_type */


void apply_attributes_to_routine(an_attribute_ptr  attributes,
                                 a_routine_ptr     rp)
/*
Apply the attributes to the indicated routine.  Issue diagnostic
messages about any invalid attributes.
*/
{
  an_attribute_ptr  ap;
  a_boolean         referenced = FALSE;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
      case ak_constructor:
        rp->is_initialization_routine = TRUE;
        /* Since the routine will be called at program start up, treat
           it as referenced. */
        referenced = TRUE;
        break;
      case ak_destructor:
        rp->is_finalization_routine = TRUE;
        /* Since the routine will be called at program shut down, treat
           it as referenced. */
        referenced = TRUE;
        break;
      case ak_unused:
        rp->unused = TRUE;
        break;
      case ak_pure:
        rp->is_pure = TRUE;
        break;
      case ak_noreturn:
      case ak_const:
        { a_routine_type_supplement_ptr  rtsp;
          ensure_routine_has_modifiable_type(rp);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          if (ap->kind == (an_attribute_kind)ak_const) {
            rtsp->is_const = TRUE;
          } else {
            rtsp->does_not_return = TRUE;
          }  /* if */
        }
        break;
      case ak_weak:
        rp->is_weak = TRUE;
        break;
      case ak_section:
        rp->section = ap->variant.section;
        break;
      case ak_alias:
        add_alias_fixup((a_symbol_ptr)rp->source_corresp.assoc_info,
                        ap->variant.alias, &ap->position);
        break;
      case ak_malloc:
        /* GCC does not issue any diagnostics if the routine does not
           return a pointer type. */
        rp->allocates_memory = TRUE;
        break;
      case ak_format:
        { a_routine_type_supplement_ptr rtsp;
          a_param_type_ptr              ptp;
          a_boolean                     error_occurred = FALSE;
          int                           count;
          ensure_routine_has_modifiable_type(rp);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          if (!rtsp->prototyped) {
            /* For an unprototyped function, no checks are
               required. */
          } else if (!rtsp->has_ellipsis) {
            if (ap->variant.format.first_subst_arg != 0) {
              /* A function without an ellipsis cannot have the "format"
                 attribute (unless the substitution argument was specified
                 as zero). */
              pos_sy_error(ec_format_rout_not_varargs, &ap->position,
                           (a_symbol_ptr)rp->source_corresp.assoc_info);
              error_occurred = TRUE;
            }  /* if */
          } else {
            /* Check to see that the format argument has string type
               and that the substitution argument is the first
               variable argument. */
            for (count = 0, ptp = rtsp->param_type_list; ptp != NULL; 
                 ptp = ptp->next) {
              count++;
              if (count == ap->variant.format.fmt_arg &&
                  !(is_pointer_type(ptp->type) &&
                    is_character_type(type_pointed_to(ptp->type)))) {
                pos_error(ec_fmt_arg_is_not_string, &ap->position);
                error_occurred = TRUE;
              }  /* if */
            }  /* for */
            /* If the format argument index is out of range, issue an
               error message. */
            if (count < ap->variant.format.fmt_arg) {
              pos_error(ec_fmt_arg_does_not_exist, &ap->position);
              error_occurred = TRUE;
            }  /* if */
            if (ap->variant.format.first_subst_arg != count + 1) {
              pos_error(ec_subst_arg_is_not_variable, &ap->position);
              error_occurred = TRUE;
            }  /* if */
          }  /* if */
          if (!error_occurred) {
            switch (ap->variant.format.kind) {
            case fak_printf:
              rtsp->arg_pragma = (a_pragma_kind)pk_printf_args;
              rtsp->fmt_arg = ap->variant.format.fmt_arg;
              break;
            case fak_scanf:
              rtsp->arg_pragma = (a_pragma_kind)pk_scanf_args;
              rtsp->fmt_arg = ap->variant.format.fmt_arg;
              break;
            case fak_strftime:
              /* The EDG support does not support strftime format
                 checking, so this form of the attribute is silently
                 ignored. */
              break;
            default:
              unexpected_condition();
            }  /* switch */
          }  /* if */
        }
        break;
      case ak_format_arg:
        { a_routine_type_supplement_ptr rtsp;
          a_param_type_ptr              ptp;
          int                           count;
          a_boolean                     error_occurred = FALSE;
          ensure_routine_has_modifiable_type(rp);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          if (!rtsp->prototyped) {
            /* For an unprototyped function, no checks are
               required. */
          } else {
            /* Check to see that the format argument has string type
               and that the substitution argument is variable. */
            for (count = 0, ptp = rtsp->param_type_list; ptp != NULL; 
                 ptp = ptp->next) {
              count++;
              if (count == ap->variant.fmt_arg &&
                  !(is_pointer_type(ptp->type) &&
                    is_character_type(type_pointed_to(ptp->type)))) {
                pos_error(ec_fmt_arg_is_not_string, &ap->position);
                error_occurred = TRUE;
              }  /* if */
            }  /* for */
            /* If the format argument index is out of range, issue an
               error message. */
            if (count < ap->variant.fmt_arg) {
              pos_error(ec_fmt_arg_does_not_exist, &ap->position);
              error_occurred = TRUE;
            }  /* if */
          }  /* if */
          if (!error_occurred) {
            rtsp->fmt_arg = ap->variant.fmt_arg;
          }  /* if */
        }
        break;
#if GNU_NAKED_ATTRIBUTE_ALLOWED
      case ak_naked:
        rp->is_naked = TRUE;
        break;
#endif /* GNU_NAKED_ATTRIBUTE_ALLOWED */
      case ak_no_instrument_function:
        rp->no_instrument_function = TRUE;
        break;
      case ak_no_check_memory_usage:
        rp->no_check_memory_usage = TRUE;
        break;
#if GNU_X86_ATTRIBUTES_ALLOWED
      case ak_cdecl:
        { a_routine_type_supplement_ptr rtsp;
          ensure_routine_has_modifiable_type(rp);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          if (rtsp->calling_convention == (a_calling_convention)cc_default) {
            /* The GNU C compiler appears to ignore the cdecl attribute if
               another calling convention is already specified. */
            rtsp->calling_convention = (a_calling_convention)cc_cdecl;
          }  /* if */
        }
        break;
      case ak_stdcall:
        { a_routine_type_supplement_ptr rtsp;
          ensure_routine_has_modifiable_type(rp);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          rtsp->calling_convention = (a_calling_convention)cc_stdcall;
        }
        break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
      default:
        /* An invalid attribute. */
        pos_sy_warning(ec_attribute_does_not_apply,
                       &ap->position,
                       (a_symbol_ptr)rp->source_corresp.assoc_info);
        break;
    }  /* switch */
  }  /* for */

  if (referenced) {
    /* Mark the routine as referenced in order to suppress warnings
       about it if it is unused. */
    mark_referenced((a_symbol_ptr)rp->source_corresp.assoc_info,
                    &pos_curr_token);
  }  /* if */
}  /* apply_attributes_to_routine */


void apply_attributes_to_type(an_attribute_ptr attributes,
                              a_type_ptr       tp,
                              a_boolean        is_typedef)
/*
Apply the attributes to the indicated type, which must not be a
typeref.  Issue diagnostic messages about any invalid attributes.  If
is_typedef is TRUE, then tp is a new type being created as part of a
typedef declaration.  This routine modifies tp in place; the caller
must make a copy if tp may already be shared.
*/
{
  an_attribute_ptr  ap;
  a_type_ptr        mode_type;

  check_assertion(tp->kind != (a_type_kind)tk_typeref);
  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
        /* Set the alignment here.  When the actual class layout, or
           choice of integral type, is performed the value indicated
           here will be honored. */
        tp->alignment = ap->variant.alignment;
        tp->alignment_set_explicitly = TRUE;
        break;
      case ak_packed:
        if (is_typedef) {
          pos_warning(ec_packed_attribute_ignored_in_typedef, &ap->position);
        } else if (is_enum_type(tp)) {
          /* A packed enumerated type can be smaller than an "int". */
          tp->variant.integer.packed = TRUE;
        } else if (is_immediate_class_type(tp)) {
          /* A packed class is one where all of the members are aligned on
             a 1-byte boundary.   In addition, bit fields may straddle
             container boundaries. */
          tp->variant.class_struct_union.is_packed = TRUE;
          tp->variant.class_struct_union.max_member_alignment = 1;
        } else {
          pos_ty_error(ec_attribute_does_not_apply_to_type, 
                       &ap->position, tp);
        }  /* if */
        break;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_mode:
        mode_type = get_type_with_mode(tp, ap->variant.mode, &ap->position);
        if (tp->kind != (a_type_kind)tk_integer &&
            tp->kind != (a_type_kind)tk_float) {
          /* If tp had neither integer nor floating type, it is 
             an error to use the mode attribute.  An error will have
             been issued by get_type_with_mode. */
        } else {
          if (tp->kind == (a_type_kind)tk_integer) {
            tp->variant.integer.int_kind = mode_type->variant.integer.int_kind;
          } else if (tp->kind == (a_type_kind)tk_float) {
            tp->variant.float_kind = mode_type->variant.float_kind;
          }  /* if */
          tp->size = mode_type->size;
          if (!tp->alignment_set_explicitly) {
            tp->alignment = mode_type->alignment;
          }  /* if */
        }  /* if */
        break;
      case ak_unused:
        tp->variables_are_implicitly_referenced = TRUE;
        break;
      case ak_noreturn:
      case ak_const:
        /* GCC allows "noreturn" and "const" to apply to
           pointer-to-function types.  GCC does not accept "pure" in
           this context, even though it is conceptually similar. */
        /* Recall that tp is not a typeref here. */
        if (!is_pointer_type(tp) || !is_function_type(type_pointed_to(tp))) {
          pos_ty_warning(ec_attr_requires_func_type,
                         &ap->position, tp);
        } else {
          a_type_ptr rout_type = tp->variant.pointer.type;
          rout_type = copy_type_and_apply_attributes((an_attribute_ptr)NULL,
                                                     rout_type,
                                                     is_typedef);
          tp->variant.pointer.type = rout_type;
          rout_type = skip_typerefs(rout_type);
          if (ap->kind == (an_attribute_kind)ak_noreturn) {
            rout_type->variant.routine.extra_info->does_not_return = TRUE;
          } else {
            rout_type->variant.routine.extra_info->is_const = TRUE;
          }  /* if */
        }  /* if */
        break;
      case ak_transparent_union:
        if (tp->kind != (a_type_kind)tk_union) {
          pos_ty_error(ec_transparent_type_is_not_union,
                       &ap->position, tp);
        } else if (is_typedef && is_incomplete_type(tp)) {
          pos_warning(ec_transparent_attribute_ignored, &ap->position);
        } else if (!is_typedef) {
          /* We cannot do any checking in the non-typedef case because
             the type has not yet been laid out.  When do_class_layout
             processes the type, it will call check_transparent_union 
             to make sure that the attribute is legal. */
          tp->variant.class_struct_union.is_transparent = TRUE;
        } else if (check_transparent_union(tp, &ap->position)) {
          /* In the typedef case, the type has already been laid out
             so we can do the check now. */
          tp->variant.class_struct_union.is_transparent = TRUE;
        }  /* if */
        break;
      default:
        /* An invalid attribute. */
        pos_ty_error(ec_attribute_does_not_apply_to_type,
                     &ap->position, tp);
    }  /* switch */
  }  /* for */
}  /* apply_attributes_to_type */


static a_type_ptr copy_type_and_apply_attributes(an_attribute_ptr attributes,
                                                 a_type_ptr       tp,
                                                 a_boolean        is_typedef)
/*
Make a copy of tp and apply the attributes to the copy.  Return the
newly created type.  If is_typedef is TRUE, tp is a new typedef.
*/
{
  a_type_qualifier_set qualifiers;
  a_type_ptr           copy;

  /* Remember the type qualifiers so that we can create an identically
     qualified copy. */
  qualifiers = get_type_qualifiers(tp);
  /* Now that we have stored away the qualifiers, get the underlying
     type. */
  tp = skip_typerefs(tp);
  /* Make a copy of the type.  The actions required depend on the kind
     of type we are copying. */
  copy = alloc_type(tp->kind);
  copy_type(tp, copy);
  copy->source_corresp.has_associated_pragma = FALSE;
  copy->copy_with_additional_attributes = TRUE;
  /* We must make a deep copy of class types. */
  if (is_immediate_class_type(copy)) {
    if (is_incomplete_type(tp)) {
      /* Add the incomplete type to the list of types that will need
         fixups when tp is defined. */
      add_to_dependent_type_fixup_list
        (tp, 
         (a_dependent_type_fixup_kind)dtfk_copy_definition, 
         (char *)copy, (a_byte_il_entry_kind)iek_type,
         &error_position);
    } else {
      copy_class_struct_or_union_definition(copy, tp);
    }  /* if */
  }  /* if */
  if (is_immediate_class_type(copy) || is_immediate_enum_type(copy)) {
    add_to_types_list(copy, NO_SCOPE_DEPTH);
  }  /* if */
  /* Apply the attributes to the copy. */
  apply_attributes_to_type(attributes, copy, is_typedef);
  /* Create an appropriately qualified version of the copy. */
  copy = make_qualified_type(copy, qualifiers);

  return copy;
}  /* copy_type_and_apply_attributes */


a_type_ptr apply_attributes_to_typedef(an_attribute_ptr attributes,
                                       a_type_ptr       tp)
/* 
Apply the attributes to the indicated type, which is a new typedef.
*/
{
  a_type_ptr copy, underlying_type = skip_typerefs(tp);
  a_type_ptr union_type;

  if ((attributes->kind == (an_attribute_kind)ak_transparent_union &&
       attributes->next == NULL) ||
      (is_immediate_class_type(underlying_type) &&
       underlying_type->variant.class_struct_union.originally_unnamed) ||
      (is_immediate_enum_type(underlying_type) &&
       underlying_type->variant.class_struct_union.originally_unnamed)) {
    /* The transparent_union attribute always applies to the underlying type.
       Hence, if it is the only attribute, we can apply it directly to that
       underlying type.  Similarly, if this type is acquiring the typedef
       name for linkage purposes (class and enum types only), the attributes
       can directly be applied to the unnamed underlying type. */
    apply_attributes_to_type(attributes, underlying_type, /*is_typedef=*/TRUE);
    copy = underlying_type;
  } else {
    copy = copy_type_and_apply_attributes(attributes, tp, /*is_typedef=*/TRUE);
    /* The transparent union attribute applies to the original type as
       well as the typedef. */
    if (is_union_type(copy)) {
      union_type = skip_typerefs(copy);
      if (union_type->variant.class_struct_union.is_transparent) {
        check_assertion(is_union_type(tp));
        skip_typerefs(tp)->variant.class_struct_union.is_transparent = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return copy;
}  /* apply_attributes_to_typedef */


void copy_class_struct_or_union_definition(a_type_ptr to,
                                           a_type_ptr from)
/*
"to" is a copy of "from".  Copy the definition of "from" to "to".
Both "from" and "to" are class, struct, or union types.
*/
{
  a_field_ptr  *fp;
  a_field_ptr  f;

  check_assertion(is_immediate_class_type(to));
  check_assertion(is_immediate_class_type(from));
  /* Copy the size and alignment. */
  to->size = from->size;
  to->alignment = from->alignment;
  /* Copy the fields of a complete type. */
  to->variant.class_struct_union.field_list = 
    from->variant.class_struct_union.field_list;
  for (fp = &to->variant.class_struct_union.field_list;
       *fp != NULL;
       fp = &(*fp)->next) {
    /* Create a new field. */
    f = alloc_field();
    /* Copy the data from the old field. */
    *f = **fp;
    /* There are no #pragmas associated with the new field. */
    f->source_corresp.has_associated_pragma = FALSE;
    /* Chain the copy onto the list, in place of the original. */
    *fp = f;
  }  /* for */
}  /* copy_class_struct_or_union_definition */


void check_for_invalid_param_attributes(a_symbol_ptr     sym,
                                        an_attribute_ptr attributes)
/*
sym is the symbol for a parameter that was present in a function
that was declared, but not defined.  It will be NULL if the
parameter has no name.  The attributes apply to that parameter.
Issue error messages about any attributes that are not valid
for a parameter.
*/
{
  an_attribute_ptr ap;

  for (ap = attributes; ap != NULL; ap = ap->next) {
    switch (ap->kind) {
      case ak_mode:
        /* These attributes apply to the type of the parameter, so
           they are OK. */
        break;
#if USER_CONTROL_OF_STRUCT_PACKING
      case ak_aligned:
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      case ak_unused:
        /* These attributes apply to the variable itself and so are
           not permitted here. */
        pos_st_error(ec_attribute_only_in_func_def, &ap->position, 
                     attribute_kind_names[(int)ap->kind]);
        break;
      default:
        /* These attributes do not apply to parameters. */
        if (sym != NULL) {
          pos_sy_warning(ec_attribute_does_not_apply, &ap->position, sym);
        } else {
          pos_warning(ec_attribute_does_not_apply_to_param, &ap->position);
        }  /* if */
        break;
    }  /* switch */
  }  /* for */
}  /* check_for_invalid_param_attributes */


void check_function_param_attributes(a_func_info_block_ptr func_info)
/*
The function with which func_info is associated has been declared but not
defined.  Check for invalid attributes in that context.  (The attributes
should not be associated with the parameter, but with its type.)
*/
{
  a_param_id_ptr  pid = func_info->param_id_list;

  for (; pid != NULL; pid = pid->next) {
    check_for_invalid_param_attributes(pid->symbol, pid->attributes);
  }  /* if */
}  /* check_function_param_attributes */


a_type_ptr copy_gnu_type_attributes(a_type_ptr  dst,
                                    a_type_ptr  src)
/*
Copy any GNU type attributes in type dst to type src.
*/
{
  src = skip_typerefs(src);
  dst = skip_typerefs(dst);
  if (dst == src) {
    /* Nothing to be done. */
  } else {
    switch (src->kind) {
      case tk_routine:
#if GNU_X86_ATTRIBUTES_ALLOWED
        { a_routine_type_supplement_ptr src_rtsp, dst_rtsp;
          src_rtsp = src->variant.routine.extra_info;
          dst_rtsp = dst->variant.routine.extra_info;
          if (src_rtsp->calling_convention !=
                                           (a_calling_convention)cc_default &&
              dst_rtsp->calling_convention !=
                                           (a_calling_convention)cc_stdcall) {
            dst_rtsp->calling_convention = src_rtsp->calling_convention;
          }  /* if */
          dst_rtsp->does_not_return = src_rtsp->does_not_return;
          dst_rtsp->is_const = src_rtsp->is_const;
        }
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
        break;
      default:
        /* No attributes to copy. */
        break;
    }  /* switch */
  }  /* if */
  return dst;
}  /* copy_gnu_type_attributes */


void attribute_one_time_init(void)
/*
Do one-time initialization of variables related to the processing of
attributes.
*/
{
#if CHECKING
  /* Check that the table of mode names is correctly initialized. */
  if (type_mode_kind_names[(int)tmk_last] == NULL ||
      strcmp(type_mode_kind_names[(int)tmk_last], "last") != 0) {
    internal_error(
     "attribute_one_time_init: initialization of type_mode_kind_names is bad");
  }  /* if */
  /* Check that the table of format attribute names is correctly
     initialized. */
  if (format_attribute_kind_names[(int)fak_last] == NULL ||
      strcmp(format_attribute_kind_names[(int)fak_last], "last") != 0) {
    internal_error(
     "attribute_one_time_init: init of format_attribute_kind_names is bad");
  }  /* if */
  /* Check that the table of attribute names is correctly
     initialized. */
  if (attribute_kind_names[(int)ak_last] == NULL ||
      strcmp(attribute_kind_names[(int)ak_last], "last") != 0) {
    internal_error(
     "attribute_one_time_init: initialization of attribute_kind_names is bad");
  }  /* if */
#endif /* CHECKING */
  /* Save variables from attribute.h and attribute.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_attributes),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* attribute_one_time_init */


void attribute_init(void)
/*
Initialize static variables related to attribute processing that must
be initialized for each compilation.
*/
{
  avail_attributes = NULL;
  avail_alias_fixups = NULL;
  alias_fixup_list = NULL;
}  /* attribute_init */

#endif /* GNU_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
