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

/* Header files used by files involved in declaration processing. */
#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#include "decl_hdrs.h"
#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED
#include "il_walk.h"
#include "layout.h"
#endif /* GNU_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
/*
The "alias" attribute can refer to entities that are declared later in a
translation unit.  Therefore, we record such attributes in a fixup list and
process the list at the end of the translation unit.  This is also used to
implement the "redefine_extname" pragma used by the Sun Solaris operating
system.
*/

typedef struct an_alias_fixup *an_alias_fixup_ptr;
typedef struct an_alias_fixup {
  an_alias_fixup_ptr
		next;	/* Pointer to the next fixup to process. */
  a_symbol_ptr	alias;
			/* The symbol that is an alias for another entity.
			   NULL if this is an entry created by a
			   redefine_extname pragma directive. */
  char*		alias_name;
			/* If this is an entry created by a redefine_extname
			   pragma directive, the name to substitute by the
			   name indicated by aliased_name.  NULL otherwise. */
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
                            char*               alias_name,
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
  entry->alias_name = alias_name;
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
  a_source_position   *pos;

  while (entries != NULL) {
    entry = entries;
    entries = entries->next;
    if (entry->alias == NULL) {
      /* This entry is the result of a redefine_extname pragma directive. */
      pos = &entry->alias_position;
    } else {
      pos = &entry->alias->decl_position;
      if (entry->alias->defined) {
        /* An entity cannot have a definition and simultaneously be an alias
           for another entity. */
        pos_error(ec_alias_cannot_have_definition, pos);
      }  /* if */
    }  /* if */
    clear_locator(&locator, pos);
    (void)find_symbol(entry->aliased_name,
                      (sizeof_t)strlen(entry->aliased_name), &locator);
    aliased_sym = normal_id_lookup(&locator, IDL_LINKAGE_LOOKUP);
    if (entry->alias == NULL) {
#if REDEFINE_EXTNAME_PRAGMA_ENABLED
      /* This entry corresponds to a redefine_extname pragma directive. */
      check_assertion(entry->alias_name != NULL);
      if (aliased_sym == NULL) {
        /* There is no declaration on which the pragma has an effect. */
      } else {
        a_source_correspondence_ptr  scp = NULL;
        switch (aliased_sym->kind) {
          case sk_routine:
            aliased_sym->variant.routine.ptr->asm_name = entry->alias_name;
            scp = &aliased_sym->variant.routine.ptr->source_corresp;
            break;
          case sk_variable:
            aliased_sym->variant.variable.ptr->asm_name_or_reg.name =
                                                          entry->alias_name;
            scp = &aliased_sym->variant.variable.ptr->source_corresp;
            break;
          case sk_overloaded_function:
            pos_sy_error(ec_bad_linkage_for_redefine_extname, pos,
                         aliased_sym);
            break;
          default:
            break;
        }  /* switch */
        if (scp != NULL &&
            scp->name_linkage != (a_name_linkage_kind)nlk_external) {
          pos_sy_error(ec_bad_linkage_for_redefine_extname, pos, aliased_sym);
        }  /* if */
      }  /* if */
#else /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
      unexpected_condition();
#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */
#if GNU_EXTENSIONS_ALLOWED
    } else if (aliased_sym == NULL) {
      /* The aliased entity was not declared in this translation unit.
         Just change the asm name of the alias (which is how GNU C behaves
         on Intel-based platforms) and issue a warning (because on some
         other platforms, GNU C considers this an error). */
      switch (entry->alias->kind) {
        case sk_routine:
          entry->alias->variant.routine.ptr->asm_name = entry->aliased_name;
          break;
        case sk_variable:
          entry->alias->variant.variable.ptr->asm_name_or_reg.name =
                                                          entry->aliased_name;
          break;
        default:
          unexpected_condition();
      }  /* switch */
      pos_st_warning(ec_aliased_name_undeclared,
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
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    free_alias_fixup(entry);
  }  /* while */
}  /* process_alias_fixup_list */

#if REDEFINE_EXTNAME_PRAGMA_ENABLED

void redefine_extname_pragma(a_pending_pragma_ptr  ppp)
/*
Process the Solaris redefine_extname pragma by recording an appropriate
alias fixup entry.  Such fixup entries are applied at a later time by
process_alias_fixup_list.
*/
{
  char       *src_name = NULL, *asm_name = NULL;
  sizeof_t   src_name_len, asm_name_len;
  a_boolean  err = FALSE;

  begin_rescan_of_pragma_tokens(ppp);
  if (curr_token == tok_identifier) {
    src_name = locator_for_curr_id.symbol_header->identifier;
    src_name_len = locator_for_curr_id.symbol_header->identifier_length;
    (void)get_token();
    if (curr_token == tok_identifier) {
      asm_name = locator_for_curr_id.symbol_header->identifier;
      asm_name_len = locator_for_curr_id.symbol_header->identifier_length;
      (void)get_token();
    } else {
      err = TRUE;
      error(ec_exp_identifier);
    }  /* if */
  } else {
    err = TRUE;
    error(ec_exp_identifier);
  }  /* if */
  wrapup_rescan_of_pragma_tokens(err);
  if (!err) {
    sizeof_t  prefix_len = sizeof("redefine_extname ")-1;
    sizeof_t  pragma_len = prefix_len+src_name_len+1+asm_name_len+1;
    add_alias_fixup((a_symbol_ptr)NULL, asm_name, src_name,
                    &ppp->pragma_position);
    /* Recreate the pragma string: "redefine_extname <src-name> <asm-name>". */
    ppp->pragma_text  = (char *)alloc_primary_file_scope_il(pragma_len);
    /*lint --e(668)*/(void)memcpy(ppp->pragma_text, "redefine_extname ",
                                  prefix_len);
    /*lint --e(668)*/(void)memcpy(ppp->pragma_text+prefix_len, src_name,
                                  src_name_len);
    ppp->pragma_text[prefix_len+src_name_len] = ' ';
    /*lint --e(668)*/(void)memcpy(ppp->pragma_text+prefix_len+src_name_len+1,
                                  asm_name, asm_name_len+1);
    /* Record the pragma in the IL. */
    create_il_entry_for_pragma(ppp, (a_symbol_ptr)NULL, (a_statement_ptr)NULL);
  }  /* if */
}  /* redefine_extname_pragma */

#endif /* REDEFINE_EXTNAME_PRAGMA_ENABLED */

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

#if GNU_EXTENSIONS_ALLOWED

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
    case ak_used:
    case ak_deprecated:
    case ak_constructor:
    case ak_destructor:
    case ak_noreturn:
    case ak_volatile:
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
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    case ak_visibility:
      ap->variant.ELF_visibility = (an_ELF_visibility_kind)evk_unspecified;
      break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    case ak_init_priority:
      ap->variant.init_priority = 0;
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
      case ak_used:
      case ak_deprecated:
      case ak_constructor:
      case ak_destructor:
      case ak_noreturn:
      case ak_volatile:
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
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      case ak_visibility:
        (*end)->variant.ELF_visibility = attributes->variant.ELF_visibility;
        break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      case ak_init_priority:
        (*end)->variant.init_priority = attributes->variant.init_priority;
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
          if (ovflo || param_number < 0 ||
              param_number > INT_MAX) { /*lint !e685*/
            goto error;
          }  /* if */
          /* Remember the value. */
          if (i == 0) {
            attribute->variant.format.fmt_arg = (int)param_number;
          } else {
            attribute->variant.format.first_subst_arg = (int)param_number;
          }  /* if */
        }  /* for */
        if (attribute->variant.format.kind ==
                                          (a_format_attribute_kind)fak_last) {
          /* An unrecognized format function type is not a fatal error. */
          pos_st_warning(ec_unrecognized_format_function_type, &pos, name);
        } else {
          /* All went well. */
          result = TRUE;
        }  /* if */
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
        if (ovflo || param_number < 0 ||
            param_number > INT_MAX) { /*lint !e685*/
          goto error;
        }  /* if */
        /* Remember the value. */
        attribute->variant.fmt_arg = param_number;
        /* All went well. */
        result = TRUE;
      }
      break;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    case ak_visibility:
      { char  *visibility_str;
        /* Look for a string-literal specifying the visibility. */
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
        visibility_str = const_for_curr_token.variant.string.value;
        if (strcmp(visibility_str, "hidden") == 0) {
          attribute->variant.ELF_visibility =
                                           (an_ELF_visibility_kind)evk_hidden;
        } else if (strcmp(visibility_str, "protected") == 0) {
          attribute->variant.ELF_visibility =
                                        (an_ELF_visibility_kind)evk_protected;
        } else if (strcmp(visibility_str, "internal") == 0) {
          attribute->variant.ELF_visibility =
                                         (an_ELF_visibility_kind)evk_internal;
        } else {
          result = FALSE;
          goto error;
        }  /* if */
        /* Consume the string literal. */
        (void)get_token();
      }
      break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    case ak_init_priority:
      { a_host_large_integer  priority;
        a_boolean             error_occurred;
        a_boolean             ovflo;
        /* If things go well, result will be reset to TRUE. */
        result = FALSE;
        /* Scan the priority. */
        priority = scan_integral_argument(&error_occurred, &ovflo);
        /* If there was no integer constant, a message has already
           been issued. */
        if (error_occurred) goto done;
        /* For overflow, issue the message now. */
        if (ovflo || priority < 101 || priority > 65535) {
          goto error;
        }  /* if */
        /* Remember the value. */
        attribute->variant.init_priority = priority;
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


static void clear_disabled_attributes(an_attribute_kind  *kind)
/*
Some attributes are only applicable in either GNU C or GNU C++ mode, but
not both.  Others are only recognized in some configurations.  If we are
in a mode or configuration for which *kind is not a recognized kind of
attribute, set *kind to ak_last.
*/
{
  switch (*kind) {
    case ak_nocommon:
    case ak_transparent_union:
      if (gpp_mode) {
        *kind = (an_attribute_kind)ak_last;
      }  /* if */
      break;
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    case ak_visibility:
      if (gnu_visibility_attribute_enabled) break;
      *kind = (an_attribute_kind)ak_last;
      break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    default:
      break;
  }  /* switch */
}  /* clear_disabled_attributes */


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
  used
  deprecated
  noreturn
  volatile
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
    if (curr_token != tok_identifier &&
        curr_token != tok_const && curr_token != tok_volatile) {
      error(ec_exp_attribute_name);
    } else {
      /* Remember the location of the attribute name.  This is the
         source position that we associate with the attribute. */
      copy_source_position(error_position, pos);
      if (curr_token == tok_const) {
        /* The const attribute is spelled the same as a keyword. */
        attribute_name = "const";
      } else if (curr_token == tok_volatile) {
        /* The volatile attribute is spelled the same as a keyword. */
        attribute_name = "volatile";
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
      clear_disabled_attributes(&attribute_kind);
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
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
          case ak_visibility:
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
          case ak_init_priority:
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
          case ak_used:
          case ak_deprecated:
          case ak_constructor:
          case ak_destructor:
          case ak_error:
          case ak_noreturn:
          case ak_volatile:
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
            syntax_error(ec_exp_lparen);
            attribute_kind = (an_attribute_kind)ak_error;
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


an_attribute_ptr f_scan_attributes(a_token_sequence_number  *last_token,
                                   a_source_position        *end_pos)
/*
Scan an (optional) series of attributes.  Each has the form:

  __attribute__ (( attribute-list [opt] ))

This function returns a list of all of the attributes in the order
that they appeared.  If last_token is non-NULL, *last_token is set
to the sequence number of the final right parenthesis (used for
template processing).  Similarly, if end_pos is non-NULL, *end_pos is
set to the position of the final right parenthesis.
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
    if (last_token != NULL) {
      *last_token = curr_token_sequence_number;
    }  /* if */
    if (end_pos != NULL) {
      *end_pos = pos_curr_token;
    }  /* if */
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
      case ak_volatile:
      case ak_const:
        /* GCC allows "noreturn" and "const" to apply to variables
           with pointer-to-function type.  GCC does not accept "pure"
           in this context, even though it is conceptually similar. */
        if (!is_pointer_type(type) ||
            !is_function_type(type_pointed_to(type))) {
          pos_ty_warning(ec_attr_requires_func_type, &ap->position, type);
        } else {
          /* Temporarily remove "ap" from the attributes list so that
             we can use copy_type_and_apply_attributes. */
          an_attribute_ptr next = ap->next;
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
        vp->has_gnu_unused_attribute = TRUE;
        break;
      case ak_used:
        vp->has_gnu_used_attribute = TRUE;
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
        mark_as_needed((char*)vp, (an_il_entry_kind)iek_variable);
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
        break;
      case ak_deprecated:
        vp->has_gnu_deprecated_attribute = TRUE;
        break;
      case ak_mode:
      case ak_noreturn:
      case ak_volatile:
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
                          (char*)NULL, ap->variant.alias, &ap->position);
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
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      case ak_visibility:
        vp->ELF_visibility = ap->variant.ELF_visibility;
        break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      case ak_init_priority:
        vp->init_priority = ap->variant.init_priority;
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


static void ensure_routine_type_is_modifiable(a_type_ptr  *tp)
/*
Before applying an attribute to the type field of a routine, we must make
sure that that type is not a typedef (which could be shared with other
routines).  This makes a private copy of the underlying type if that is
the case.
*/
{
  if ((*tp)->kind == (a_type_kind)tk_typeref && typeref_is_typedef(*tp)) {
    /* We cannot apply the attribute to the type underlying the
       typedef.  So make a copy of that type and apply the attribute
       to that. */
    *tp = copy_type_and_apply_attributes((an_attribute_ptr)NULL,
                                         (*tp)->variant.typeref.type,
                                         /*is_typedef=*/FALSE);
  }  /* if */
}  /* ensure_routine_type_is_modifiable */


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
        rp->has_gnu_unused_attribute = TRUE;
        break;
      case ak_used:
        rp->has_gnu_used_attribute = TRUE;
#if MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM
        mark_as_needed((char*)rp, (an_il_entry_kind)iek_routine);
#endif /* MAINTAIN_NEEDED_FLAGS && !STANDALONE_UTILITY_PROGRAM */
        break;
      case ak_deprecated:
        rp->has_gnu_deprecated_attribute = TRUE;
        break;
      case ak_pure:
        rp->is_pure = TRUE;
        break;
      case ak_noreturn:
      case ak_volatile:
      case ak_const:
        { a_routine_type_supplement_ptr  rtsp;
          ensure_routine_type_is_modifiable(&rp->type);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          if (ap->kind == (an_attribute_kind)ak_const) {
            rtsp->is_const = TRUE;
          } else {
            /* Note that "volatile" is a synonym for "noreturn". */
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
        if (rp->is_inline && rp->assoc_scope != NULL_region_number &&
            rp->suppress_inline_body) {
          /* GNU C accepts the alias attribute on a function that was
             previously defined as an extern inline function, and ignores
             that previous declaration. */
          a_symbol_ptr  sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
          pos_warning(ec_ignoring_inline_definition_because_of_alias,
                      &rp->source_corresp.decl_position);
          clear_function_body(il_header.region_scope_entry[rp->assoc_scope]);
          /* clear_function_body clears rp->defined, but not the defined flag
             on the associated symbol. */
          sym->defined = FALSE;
          rp->is_inline = FALSE;
        }  /* if */
        add_alias_fixup((a_symbol_ptr)rp->source_corresp.assoc_info,
                        (char*)NULL, ap->variant.alias, &ap->position);
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
          ensure_routine_type_is_modifiable(&rp->type);
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
              /* The EDG front end does not support strftime format
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
          ensure_routine_type_is_modifiable(&rp->type);
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
          ensure_routine_type_is_modifiable(&rp->type);
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
          ensure_routine_type_is_modifiable(&rp->type);
          rtsp = skip_typerefs(rp->type)->variant.routine.extra_info;
          rtsp->calling_convention = (a_calling_convention)cc_stdcall;
        }
        break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      case ak_visibility:
        rp->ELF_visibility = ap->variant.ELF_visibility;
        break;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
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


static void apply_one_attribute_to_type(an_attribute_ptr  ap,
                                        a_type_ptr        type,
                                        a_boolean         is_typedef)
/*
Apply the attribute ap to the type tp.  If this attribute is applied through
a typedef, is_typedef is TRUE.
*/
{
  a_type_ptr  mode_type, tp = skip_typerefs(type);

  switch (ap->kind) {
#if USER_CONTROL_OF_STRUCT_PACKING
    case ak_aligned:
      /* Set the alignment here.  When the actual class layout, or
         choice of integral type, is performed the value indicated
         here will be honored.  Note that this attribute applies to
         a typedef itself; not to its underlying type. */
      type->alignment = ap->variant.alignment;
      type->alignment_set_explicitly = TRUE;
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
      /* If this is a typedef, the attribute is attached to it rather than
         to the underlying type. */
      type->variables_are_implicitly_referenced = TRUE;
      break;
    case ak_deprecated:
      type->has_gnu_deprecated_attribute = TRUE;
      break;
    case ak_noreturn:
    case ak_volatile:
    case ak_const:
      /* GCC allows "noreturn" and "const" to apply to
         pointer-to-function types.  GCC does not accept "pure" in
         this context, even though it is conceptually similar. */
      if (!is_pointer_type(tp) || !is_function_type(type_pointed_to(tp))) {
        pos_ty_warning(ec_attr_requires_func_type, &ap->position, tp);
      } else {
        a_type_ptr rout_type = tp->variant.pointer.type;
        rout_type = copy_type_and_apply_attributes((an_attribute_ptr)NULL,
                                                   rout_type,
                                                   is_typedef);
        tp->variant.pointer.type = rout_type;
        rout_type = skip_typerefs(rout_type);
        if (ap->kind == (an_attribute_kind)ak_const) {
          rout_type->variant.routine.extra_info->is_const = TRUE;
        } else {
          /* Note that "volatile" is a synonym for "noreturn". */
          rout_type->variant.routine.extra_info->does_not_return = TRUE;
        }  /* if */
      }  /* if */
      break;
    case ak_transparent_union:
      {
        /* If tp is a typedef, the transparent_union attribute applies to
           the underlying type. */
        if (tp->kind != (a_type_kind)tk_union) {
          pos_ty_error(ec_transparent_type_is_not_union, &ap->position, type);
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
      }
      break;
#if GNU_X86_ATTRIBUTES_ALLOWED
    case ak_cdecl:
      { a_routine_type_supplement_ptr rtsp;
        if (is_pointer_type(tp)) {
          /* This attribute can be applied to both function types and
             pointer-to-function types. */
          tp = type_pointed_to(tp);
        }  /* if */
        if (!is_function_type(tp)) {
          pos_ty_warning(ec_attr_requires_func_type, &ap->position, tp);
        } else {
          ensure_routine_type_is_modifiable(&tp);
          rtsp = skip_typerefs(tp)->variant.routine.extra_info;
          if (rtsp->calling_convention == (a_calling_convention)cc_default) {
            /* The GNU C compiler appears to ignore the cdecl attribute if
               another calling convention is already specified. */
            rtsp->calling_convention = (a_calling_convention)cc_cdecl;
          }  /* if */
        }  /* if */
      }
      break;
    case ak_stdcall:
      { a_routine_type_supplement_ptr rtsp;
        if (is_pointer_type(tp)) {
          /* This attribute can be applied to both function types and
             pointer-to-function types. */
          tp = type_pointed_to(tp);
        }  /* if */
        if (!is_function_type(tp)) {
          pos_ty_warning(ec_attr_requires_func_type, &ap->position, tp);
        } else {
          ensure_routine_type_is_modifiable(&tp);
          rtsp = skip_typerefs(tp)->variant.routine.extra_info;
          rtsp->calling_convention = (a_calling_convention)cc_stdcall;
        }  /* if */
      }
      break;
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
    default:
      /* An invalid attribute. */
      pos_ty_error(ec_attribute_does_not_apply_to_type,
                   &ap->position, tp);
  }  /* switch */
}  /* apply_one_attribute_to_type */


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

  check_assertion(tp->kind != (a_type_kind)tk_typeref);
  for (ap = attributes; ap != NULL; ap = ap->next) {
    apply_one_attribute_to_type(ap, tp, is_typedef);
  }  /* for */
}  /* apply_attributes_to_type */


static a_type_ptr copy_type_and_apply_attributes(an_attribute_ptr attributes,
                                                 a_type_ptr       tp,
                                                 a_boolean        is_typedef)
/*
Make a copy of tp and apply the attributes to the copy.  Return the
newly created type.  If is_typedef is TRUE, tp is a new typedef.
The given type should not be a class or enum type.
*/
{
  a_type_qualifier_set qualifiers;
  a_type_ptr           copy;

  check_assertion(!is_class_struct_union_type(tp) && !is_enum_type(tp));
  /* Remember the type qualifiers so that we can create an identically
     qualified copy. */
  qualifiers = get_type_qualifiers(tp);
  /* Now that we have stored away the qualifiers, get the underlying
     type. */
  tp = skip_typerefs(tp);
  /* Make a copy of the type. */
  copy = alloc_type(tp->kind);
  copy_type(tp, copy);
  copy->source_corresp.has_associated_pragma = FALSE;
  copy->copy_with_additional_attributes = TRUE;
  /* Apply the attributes to the copy. */
  apply_attributes_to_type(attributes, copy, is_typedef);
  /* Create an appropriately qualified version of the copy. */
  copy = make_qualified_type(copy, qualifiers);

  return copy;
}  /* copy_type_and_apply_attributes */


void apply_attributes_to_typedef(an_attribute_ptr  attributes,
                                 a_type_ptr        tp,
                                 a_boolean         linkage_name)
/* 
Apply the attributes to the indicated type, which is a new typedef.
For certain underlying types (e.g., routine types) it is safe to make
a copy of that type and the attributes can be applied to that copy.
For unnamed class and enum types that acquire a name through the typedef,
linkage_name is TRUE and the attributes can be applied directly to the
underlying type.
*/
{
  a_type_ptr        dst, underlying_type = skip_typerefs(tp);
  an_attribute_ptr  ap;

  check_assertion(tp->kind == (a_type_kind)tk_typeref &&
                  typeref_is_typedef(tp));
  /* Determine which type the attributes should be applied to. */
  if (attributes == NULL) {
    /* Nothing will need to be done. */
  } else if (is_immediate_class_type(underlying_type)) {
    if (linkage_name &&
        underlying_type->variant.class_struct_union.originally_unnamed) {
      dst = underlying_type;
    } else {
      dst = tp;
    }  /* if */
  } else if (is_immediate_enum_type(underlying_type)) {
    if (linkage_name &&
        underlying_type->variant.integer.originally_unnamed) {
      dst = underlying_type;
    } else {
      dst = tp;
    }  /* if */
  } else {
    /* We can make a copy of the type and apply the attributes to that copy. */
    a_type_qualifier_set  qualifiers = get_type_qualifiers(tp);
    dst = alloc_type(underlying_type->kind);
    copy_type(underlying_type, dst);
    dst->source_corresp.has_associated_pragma = FALSE;
    dst->copy_with_additional_attributes = TRUE;
    tp->variant.typeref.type = make_qualified_type(dst, qualifiers);
  }  /* if */
  for (ap = attributes; ap != NULL; ap = ap->next) {
    apply_one_attribute_to_type(ap, dst, /*is_typedef=*/TRUE);
  }  /* for */
}  /* apply_attributes_to_typedef */


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
  a_type_ptr  result = dst;

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
          /* Update the result since a skip_typerefs was applied to dst. */
          result = dst;
        }
#endif /* GNU_X86_ATTRIBUTES_ALLOWED */
        break;
      default:
        /* No attributes to copy. */
        break;
    }  /* switch */
  }  /* if */
  return result;
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

#endif /* GNU_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

void attribute_init(void)
/*
Initialize static variables related to attribute processing that must
be initialized for each compilation.
*/
{
#if GNU_EXTENSIONS_ALLOWED
  avail_attributes = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */
  avail_alias_fixups = NULL;
  alias_fixup_list = NULL;
}  /* attribute_init */

#endif /* GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
