/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

decl_spec.c -- Scanning of declaration specifiers.

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

/* Additional header files. */
#include "folding.h"

#if NEAR_AND_FAR_ALLOWED

static void scan_near_or_far(a_type_qualifier_set  *qualifiers)
/*
Scan a "near" or "far" memory attribute, setting a bit in *qualifiers if
there is no error.
*/
{
  a_type_qualifier_set new_qualifier;

  if (curr_token == tok_near) {
    new_qualifier = TQ_NEAR;
  } else {
    check_assertion(curr_token == tok_far);
    new_qualifier = TQ_FAR;
  }  /* if */
  /* Check for incompatibilities. */
  if (*qualifiers & new_qualifier) {
    /* The bit is already set -- this is a duplicate. */
    warning(ec_dupl_mem_attrib);
  } else if ((*qualifiers & (TQ_NEAR | TQ_FAR)) != TQ_NONE) {
    /* The other bit has already been set -- error. */
    error(ec_mem_attrib_incompatible);
    new_qualifier = TQ_NONE;
  }  /* if */
  *qualifiers |= new_qualifier;
  (void)get_token();        
}  /* scan_near_or_far */

#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

static char *check_GUID_hex_digits(char      *str,
                                   int       ndigits,
                                   a_boolean *err)
/*
As part of checking a GUID string, check for ndigits hexadecimal digits
beginning at str.  Set *err to TRUE if the digits do not appear.  Return str,
advanced past the digits that do appear.
*/
{
  for (; ndigits > 0; ndigits--, str++) {
    if (!isxdigit((unsigned char)*str)) {
      *err = TRUE;
      break;
    }  /* if */
  }  /* for */
  return str;
}  /* check_GUID_hex_digits */


static char *check_GUID_hyphen(char      *str,
                               a_boolean *err)
/*
As part of checking a GUID string, check that *str is a hyphen character.
If not, set *err to TRUE.  Return str, advanced past the hyphen if one is
present.
*/
{
  if (*str != '-') {
    *err = TRUE;
  } else {
    str++;
  }  /* if */
  return str;
}  /* check_GUID_hyphen */


static a_boolean is_valid_GUID_string(char          *str,
                                      a_targ_size_t length)
/*
Check the indicated string to see if it is a valid Microsoft GUID string.
Such a string must have the form

  hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh

where "h" is a hex digit.  length is the length of the string (it is not
necessarily null-terminated).
*/
{
  char      *orig_str = str;
  a_boolean err = FALSE;

  str = check_GUID_hex_digits(str, 8, &err);
  str = check_GUID_hyphen(str, &err);
  str = check_GUID_hex_digits(str, 4, &err);
  str = check_GUID_hyphen(str, &err);
  str = check_GUID_hex_digits(str, 4, &err);
  str = check_GUID_hyphen(str, &err);
  str = check_GUID_hex_digits(str, 4, &err);
  str = check_GUID_hyphen(str, &err);
  str = check_GUID_hex_digits(str, 12, &err);
  /* Check that the string ends at the right place. */
  if (str != orig_str+length) err = TRUE;
  return !err;
}  /* is_valid_GUID_string */


static void scan_declspec_property(a_decl_modifiers_block_ptr  decl_modifiers)
/*
Scan the Microsoft C++ mode extension

  __declspec(property(get=gname,put=pname))

The current token is the "property" keyword.  Add the information on
the property specification to *decl_modifiers.  Return with the closing
parenthesis of the property list as the current token.
*/
{
  if (next_token() != tok_lparen) {
    /* Parenthesized list is missing. */
    error(ec_bad_declspec_property);
  } else {
    /* Advance past "property" and the left parenthesis. */
    (void)get_token();
    (void)get_token();
    add_stop_token(tok_rparen);
    do {
      a_boolean         is_get = FALSE, is_put = FALSE;
      a_source_position getput_position;
      char              *name;

      if (curr_token == tok_identifier) {
        char *getput = locator_for_curr_id.symbol_header->identifier;
        if (strcmp(getput, "get") == 0) {
          is_get = TRUE;
        } else if (strcmp(getput, "put") == 0) {
          is_put = TRUE;
        }  /* if */
      }  /* if */
      if (!is_get && !is_put) {
        /* Expected "get" or "put". */
        syntax_error(ec_bad_declspec_property);
        break;
      }  /* if */
      getput_position = pos_curr_token;
      /* Advance past "get" or "put". */
      (void)get_token();
      /* Check for "=". */
      if (curr_token != tok_assign) {
        syntax_error(ec_exp_assign);
        break;
      }  /* if */
      (void)get_token();
      if (curr_token != tok_identifier) {
        /* Expected a name following "get=" or "put=". */
        syntax_error(ec_bad_declspec_property);
        break;
      }  /* if */
      /* Allocate a copy of the name specified. */
      name = alloc_il(len_of_curr_token+1);
      (void)memcpy(name, start_of_curr_token, size_t_arg(len_of_curr_token));
      name[len_of_curr_token] = '\0';
      if (is_get) {
        if (decl_modifiers->get_property_name != NULL) {
          /* "get" specified more than once. */
          pos_error(ec_dupl_get_or_put, &getput_position);
        } else {
          decl_modifiers->get_property_name = name;
        }  /* if */
      } else {
        if (decl_modifiers->put_property_name != NULL) {
          /* "put" specified more than once. */
          pos_error(ec_dupl_get_or_put, &getput_position);
        } else {
          decl_modifiers->put_property_name = name;
        }  /* if */
      }  /* if */
      /* Advance past the routine name. */
      (void)get_token();
      /* Loop if a comma is next. */
    } while (loop_token(tok_comma));
    /* Check for closing parenthesis. */
    (void)required_token_no_advance(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  }  /* if */
}  /* scan_declspec_property */


static a_boolean scan_inheritance_kind(an_inheritance_kind  *inheritance_kind,
                                       a_source_position    *pos)
/*
Unless *inheritance_kind is already set, scan for an inheritance kind keyword
-- i.e.,
  __single_inheritance
  __multiple_inheritance
  __virtual_inheritance
(each of which can also be spelled with a single leading underscore) -- and
return the result in *inheritance_kind.  The source position of the specified
inheritance kind is returned in *pos.  Return TRUE if the scan is successful.
*/
{
  a_boolean  found = FALSE;
  char       *name;

  if (*inheritance_kind == (an_inheritance_kind)ihk_none) {
    check_assertion(curr_token == tok_identifier);
    name = locator_for_curr_id.symbol_header->identifier;
    if (*(name++) == '_') {
      if (*name == '_') name++;
      /* Check the name without its leading single or double underscore. */
      if (strcmp(name, "single_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_single;
        found = TRUE;
      } else if (strcmp(name, "multiple_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_multiple;
        found = TRUE;
      } else if (strcmp(name, "virtual_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_virtual;
        found = TRUE;
      }  /* if */
      if (found) {
        /* Remember the source position, in case a diagnostic is required
           later. */
        *pos = pos_curr_token;
        /* Advance past the inheritance-kind keyword. */
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  return found;
}  /* scan_inheritance_kind */


static void scan_declspec_attributes(
                                a_decl_modifiers_block_ptr  decl_modifiers,
                                a_boolean                   is_class_decl,
                                a_boolean                   is_member_decl,
                                a_boolean                   *err)
/*
Scan the Microsoft __declspec specifier, which has the form

	__declspec ( extended-decl-modifier-seq   )
	                                       opt
	extended-decl-modifier-seq:
		extended-decl_modifier
		                      opt
		extended-decl-modifier-seq extended-decl-modifier

	extended-decl_modifier:
		thread
		naked
		dllimport
		dllexport
                selectany
                nothrow
                novtable
                noreturn
                uuid ( "hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh" )
                property ( get = xxx, put = yyy )
                allocate ( data-segment-name )

Return the modifiers that were found by updating the decl_modifiers block.
Issue a warning for an unrecognized modifier.  If an error occurs (e.g., a
syntax error), set err to TRUE.  err is unchanged if there are no errors.
is_class_decl is TRUE if the modifiers apply to a class declaration (e.g.,
"class __declspec(dllexport) A ...") rather than to a declarator.
is_member_decl is TRUE if the modifiers are being scanned as part of the
declaration of a class member.
*/
{
  check_assertion(curr_token == tok_declspec);
  /* Bypass the __declspec token. */
  (void)get_token();
  if (required_token(tok_lparen, ec_exp_lparen)) {
    add_stop_token(tok_rparen);
    while (curr_token == tok_identifier) {
      char *modifier;
      modifier = locator_for_curr_id.symbol_header->identifier;
      if (strcmp(modifier, "dllexport") == 0) {
        if (decl_modifiers->flags & DM_DLLIMPORT) {
          /* The dllimport and dllexport attributes are mutually
             exclusive. */
          warning(ec_bad_combination_of_dll_attributes);
        } else {
          decl_modifiers->flags |= DM_DLLEXPORT;
        }  /* if */
      } else if (strcmp(modifier, "dllimport") == 0) {
        if (decl_modifiers->flags & DM_DLLEXPORT) {
          /* The dllimport and dllexport attributes are mutually
             exclusive. */
          warning(ec_bad_combination_of_dll_attributes);
        } else {
          decl_modifiers->flags |= DM_DLLIMPORT;
        }  /* if */
      } else if (strcmp(modifier, "thread") == 0) {
        if (is_class_decl) {
          /* "thread" is not allowed on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_THREAD;
        }  /* if */
      } else if (strcmp(modifier, "naked") == 0) {
        if (is_class_decl) {
          /* "naked" is not allowed on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_NAKED;
        }  /* if */
      } else if (strcmp(modifier, "selectany") == 0) {
        if (is_class_decl) {
          /* "selectany" is not allowed on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_SELECTANY;
        }  /* if */
      } else if (!C_mode() && strcmp(modifier, "nothrow") == 0) {
        if (is_class_decl) {
          /* "nothrow" is not allowed on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_NOTHROW;
        }  /* if */
      } else if (!C_mode() && strcmp(modifier, "novtable") == 0) {
        if (!is_class_decl) {
          /* "novtable" is allowed only on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_NOVTABLE;
        }  /* if */
      } else if (strcmp(modifier, "noreturn") == 0) {
        if (is_class_decl) {
          /* "noreturn" is not allowed on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
        } else {
          decl_modifiers->flags |= DM_NORETURN;
        }  /* if */
      } else if (!C_mode() && strcmp(modifier, "uuid") == 0) {
        if (!is_class_decl) {
          /* "uuid" is allowed only on a class declaration. */
          pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                         &pos_curr_token, modifier);
          if (next_token() == tok_lparen) {
            /* Advance past "uuid" to the left paren. */
            (void)get_token();
            /* Flush all tokens till the matching right paren is
               found. */
            flush_until_matching_token();
          }  /* if */
        } else {
          /* The syntax is
               uuid ( string-literal )
             where the string-literal optionally begins and ends with
             braces and is of the form
               hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
             where "h" is any hex digit and the hyphens are required. */
          /* Advance past "uuid". */
          (void)get_token();
          if (required_token(tok_lparen, ec_exp_lparen)) {
            if (curr_token != tok_string_literal) {
              /* Error. */
              syntax_error(ec_bad_uuid_string);
            } else if (is_error_constant(&const_for_curr_token)) {
              /* We encountered a misformed string literal.  An error should
                 have been issued already. */
              check_assertion(total_errors != 0);
            } else {
              char          *str =
                               const_for_curr_token.variant.string.value;
              a_targ_size_t length = /* Without null. */
                            const_for_curr_token.variant.string.length-1;
              if (*str == '{') {
                /* Has surrounding braces. */
                /* Check for matching closing brace. */
                if (str[length-1] != '}') {
                  error(ec_bad_uuid_string);
                  goto end_of_uuid_string;
                }  /* if */
                str++;
                length -= 2;
              }  /* if */
              /* Do error checking on the string. */
              if (is_valid_GUID_string(str, length)) {
                decl_modifiers->uuid_string=alloc_il((sizeof_t)length+1);
                /* Copy the string, lower-casing hex letters so that
                   strcmp can be used to compare strings. */
                { char		*src = str;
                  char		*dst = decl_modifiers->uuid_string;
                  a_targ_size_t	count = length;
                  for (; count != 0; count--) {
                    char ch = *src++;
                    if (isalpha((unsigned char)ch)) ch = tolower(ch);
                    *dst++ = ch;
                  }  /* for */
                  *dst = '\0';
                }
              } else {
                error(ec_bad_uuid_string);
                *err = TRUE;
              }  /* if */
end_of_uuid_string:
              (void)get_token();
            }  /* if */
            (void)required_token_no_advance(tok_rparen, ec_exp_rparen);
          } else {
            break;
          }  /* if */
        }  /* if */
      } else if (!C_mode() && strcmp(modifier, "property") == 0) {
        if (is_class_decl || !is_member_decl) {
          /* "property" is not allowed on a class declaration, and
             not on a non-member declaration. */
          pos_diagnostic(es_discretionary_error,
                         ec_declspec_property_not_allowed,
                         &pos_curr_token);
          if (next_token() == tok_lparen) {
            /* Advance past "property" to the left paren. */
            (void)get_token();
            /* Flush all tokens till the matching right paren is
               found. */
            flush_until_matching_token();
          }  /* if */
        } else {
          /* __declspec(property(get=..., put=...)) */
          scan_declspec_property(decl_modifiers);
        }  /* if */
      } else if (strcmp(modifier, "allocate") == 0) {
        if (is_class_decl) {
          /* "allocate" is not allowed on a class declaration. */
          pos_error(ec_declspec_allocate_not_allowed, &pos_curr_token);
          *err = TRUE;
          if (next_token() == tok_lparen) {
            /* Advance past "allocate" to the left paren. */
            (void)get_token();
            /* Flush all tokens till the matching right paren is
               found. */
            flush_until_matching_token();
          }  /* if */
        } else {
          /* The syntax is
               allocate ( string-literal )
             where string-literal specifies the name of a data segment
             in which a data item will be allocated. */
          /* Advance past "allocate". */
          (void)get_token();
          if (required_token(tok_lparen, ec_exp_lparen)) {
            if (curr_token != tok_string_literal) {
              /* Error. */
              syntax_error(ec_bad_allocate_segname);
              *err = TRUE;
            } else {
              /* The current token is a string literal.  No checking
                 is done to assure that it is a valid data segment name
                 (though such a check could be added if the appropriate
                 #pragma support were also added). */
              char           *str;
              a_targ_size_t  len;  /* Length includes terminal null. */

              str = const_for_curr_token.variant.string.value;
              len = const_for_curr_token.variant.string.length;
              /* Copy the token string into IL memory and save the
                 address. */
              decl_modifiers->allocate_segname = alloc_il((sizeof_t)len);
              (void)memcpy(decl_modifiers->allocate_segname, str,
                           size_t_arg(len));
              check_assertion(decl_modifiers->
                                       allocate_segname[len-1] == '\0');
              /* Advance past the string literal. */
              (void)get_token();
            }  /* if */
            /* Advance past the right paren. */
            (void)required_token_no_advance(tok_rparen, ec_exp_rparen);
          } else {
            break;
          }  /* if */
        }  /* if */
      } else {
        /* Issue a warning on an unrecognized __declspec attribute. */
        pos_st_warning(ec_bad_declspec_modifier, &error_position,
                       modifier);
        /* An unrecognized construct could be of two forms:
             __declspec(xxx)        // Like "dllimport" or "nothrow"
             __declspec(xxx(yyy))   // Like "allocate" or "uuid"
           If the next token is a left paren, skip to the matching
           right paren. */
        if (next_token() == tok_lparen) {
          /* Advance to the left paren. */
          (void)get_token();
          /* Flush all tokens till the matching right paren is found. */
          flush_until_matching_token();
        }  /* if */
      }  /* if */
      (void)get_token();
    }  /* while */
    remove_stop_token(tok_rparen);
    /* Check for the closing right paren. */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
}  /* scan_declspec_attributes */


static a_boolean any_multiple_inheritance(a_type_ptr  class_type)
/*
Return TRUE if the specified class type or any of its base classes was
declared with more than one base class.
*/
{
  a_boolean                    multiple = FALSE;
  a_base_class_ptr             bcp;

  bcp = base_classes_of(class_type);
  if (bcp != NULL) {
    /* Find the first direct base class. */
    while (!bcp->direct) bcp = bcp->next;
    if (bcp->next != NULL || any_multiple_inheritance(bcp->type)) {
      /* If there's a next pointer, there must be another direct base
         class.  Otherwise, it depends on the inheritance of the
         associated class type. */
      multiple = TRUE;
    }  /* if */
  }  /* if */
  return multiple;
}  /* any_multiple_inheritance */


void check_inheritance_kind(a_type_ptr           class_type,
                            an_inheritance_kind  inheritance_kind,
                            a_source_position    *err_pos)
/*
Issue an error if the specified inheritance kind (which was either
explicitly specified for the specified class or assigned to it by default)
is insufficient for the actual characteristics of the class.  *err_pos
indicates the source position at which the error should be put out.
*/
{
  a_boolean  err;

  if (inheritance_kind != (an_inheritance_kind)ihk_none) {
    err = FALSE;
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      err = inheritance_kind < (an_inheritance_kind)ihk_virtual;
    } else if (any_multiple_inheritance(class_type)) {
      err = inheritance_kind < (an_inheritance_kind)ihk_multiple;
    }  /* if */
    if (err) {
      pos_stsy_error(ec_invalid_inheritance_kind_for_class, err_pos,
                     inheritance_kind_names[(int)inheritance_kind],
                     (a_symbol_ptr)class_type->source_corresp.assoc_info);
    }  /* if */
  }  /* if */
}  /* check_inheritance_kind */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE || NEAR_AND_FAR_ALLOWED
  
#if !DECL_MODIFIERS_IN_USE
/*ARGSUSED*/ /* err_pos is used only if DECL_MODIFIERS_IN_USE is set. */
#endif /* !DECL_MODIFIERS_IN_USE */
void update_extended_decl_info_for_class(
                            a_type_ptr                  class_type,
                            an_extended_decl_info_block *extended_decl_info,
                            a_source_position           *err_pos)
/*
Update the specified class type with information based on a previous scan of
extended declaration modifiers, as specified by *extended_decl_info.  err_pos
is a pointer to a source position used for diagnostics.
*/
{
  a_class_type_supplement_ptr ctsp;

  ctsp = class_type->variant.class_struct_union.extra_info;
  /* If there were any class-wide modifiers or memory attributes
     specified, record them in the class type supplement. */
#if NEAR_AND_FAR_ALLOWED
  ctsp->qualifiers = extended_decl_info->qualifiers;
#endif /* NEAR_AND_FAR_ALLOWED */
#if DECL_MODIFIERS_IN_USE
  if (extended_decl_info->decl_modifiers.flags != DM_NONE) {
    /* The following processing is more complicated that it needs to be so as
       to allow for the easy addition of decl-modifiers. */
    a_boolean        any_invalid_redecl = FALSE;
    a_boolean        invalid_modifier, invalid_redecl;
    int              bit_number;
    a_decl_modifier  modifier_value;

    for (bit_number = 0; bit_number < (int)dmt_last; ++bit_number) {
      modifier_value = (1 << bit_number);
      if ((extended_decl_info->decl_modifiers.flags & modifier_value) != 0) {
        /* This bit is set. */
        invalid_modifier = FALSE;
        invalid_redecl = FALSE;
        switch (bit_number) {
#if MICROSOFT_EXTENSIONS_ALLOWED
          case dmt_dllimport:
            if (ctsp->decl_modifiers & DM_DLLEXPORT) {
              if (is_incomplete_type(class_type)) {
                /* No definition has been seen yet: replace dllexport by
                   dllimport. */
                ctsp->decl_modifiers &= ~DM_DLLEXPORT;
              } else {
                /* A definition was already seen and that froze the dllimport/
                   dllexport setting.  Just ignore this one. */
                extended_decl_info->decl_modifiers.flags &= ~modifier_value;
              }  /* if */
            }  /* if */
            break;
          case dmt_dllexport:
            if (ctsp->decl_modifiers & DM_DLLIMPORT) {
              if (is_incomplete_type(class_type)) {
                /* No definition has been seen yet: replace dllimport by
                   dllexport. */
                ctsp->decl_modifiers &= ~DM_DLLIMPORT;
              } else {
                /* A definition was already seen and that froze the dllimport/
                   dllexport setting.  Just ignore this one. */
                extended_decl_info->decl_modifiers.flags &= ~modifier_value;
              }  /* if */
            }  /* if */
            break;
          case dmt_novtable:
            break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          default:
            invalid_modifier = TRUE;
            break;
        }  /* switch */
        /* If this modifier is invalid, reset the bit in the new modifiers. */
        if (invalid_modifier || invalid_redecl) {
          extended_decl_info->decl_modifiers.flags &= (~modifier_value);
        }  /* if */
        if (invalid_modifier) {
          pos_st_diagnostic(es_discretionary_error,
                            ec_decl_modifiers_invalid_for_this_decl,
                            err_pos, decl_modifier_names[bit_number]);
        }  /* if */
        any_invalid_redecl |= invalid_redecl;
      }  /* if */
    }  /* for */
    if (any_invalid_redecl) {
      pos_diagnostic(es_discretionary_error,
                     ec_decl_modifiers_incompatible_with_previous_decl,
                     err_pos);
    }  /* if */
    /* Update the routine entry with any valid modifiers that were found. */
    ctsp->decl_modifiers |= extended_decl_info->decl_modifiers.flags;
  }  /* if */
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (extended_decl_info->inheritance_kind != (an_inheritance_kind)ihk_none) {
    /* Set the specified inheritance kind, unless a different inheritance
       kind has already been locked in -- either explicitly through a prior
       declaration or implicitly, based on the setting of global variable
       default_inheritance_kind, if a pointer-to-member declaration has
       been seen. */
    if (ctsp->inheritance_kind == (an_inheritance_kind)ihk_none) {
      ctsp->inheritance_kind = extended_decl_info->inheritance_kind;
    } else if (ctsp->inheritance_kind !=
                                  extended_decl_info->inheritance_kind) {
      /* Inheritance kind has already been set for this class. */
      pos_stsy_error(ec_inheritance_kind_already_set,
                     &extended_decl_info->inheritance_kind_pos,
                     inheritance_kind_names[(int)ctsp->inheritance_kind],
                     (a_symbol_ptr)class_type->source_corresp.assoc_info);
    }  /* if */
    if (ctsp->inheritance_kind == extended_decl_info->inheritance_kind) {
      ctsp->inheritance_kind_is_explicit = TRUE;
    }  /* if */
  }  /* if */
  if (extended_decl_info->decl_modifiers.uuid_string != NULL) {
    if (ctsp->uuid_string != NULL) {
      /* Issue an error if __declspec(uuid(...)) strings are present and
         they aren't identical. */
      if (strcmp(ctsp->uuid_string,
                 extended_decl_info->decl_modifiers.uuid_string) != 0) {
        pos_diagnostic(es_discretionary_error,
                       ec_decl_modifiers_incompatible_with_previous_decl,
                       err_pos);
      }  /* if */
    } else {
      ctsp->uuid_string = extended_decl_info->decl_modifiers.uuid_string;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* update_extended_decl_info_for_class */

#endif /* DECL_MODIFIERS_IN_USE || NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* is_member_decl and err are used only if
                MICROSOFT_EXTENSIONS_ALLOWED is set. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
void scan_extended_decl_modifiers(
                             a_boolean                    is_class_decl,
                             a_boolean                    is_member_decl,
                             an_extended_decl_info_block  *extended_decl_info,
                             a_boolean                    *err)
/*
Scan extended declaration modifiers (e.g., Microsoft extensions) and
record them in the specified extended-decl-info block.  is_class_decl is
TRUE if the current declaration is of a class; is_member_decl is TRUE
if it's a declaration of a class member.  *err is returned TRUE for certain
kinds of errors.
*/
{
  for (;;) {
#if NEAR_AND_FAR_ALLOWED
    if (is_class_decl && is_near_or_far()) {
      /* Memory attribute like "near". */
      scan_near_or_far(&extended_decl_info->qualifiers);
      continue;
    }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (curr_token == tok_declspec) {
      /* __declspec(...) */
      scan_declspec_attributes(&extended_decl_info->decl_modifiers,
                               is_class_decl, is_member_decl, err);
      continue;
    }  /* if */
    if (is_class_decl && curr_token == tok_identifier) {
      /* This is a class declaration, so if the next token is an identifier
         it is probably the class name.  But it might also be the "inheritance
         kind". */
      if (scan_inheritance_kind(&extended_decl_info->inheritance_kind,
                                &extended_decl_info->inheritance_kind_pos)) {
        continue;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    break;
  }  /* for */
}  /* scan_extended_decl_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

static a_boolean tag_currently_being_defined(a_type_ptr tag_type)
/*
Returns TRUE if the type pointed to by tag_type is in the process
of being defined.  This is determined by examining any
class/struct/union scopes on the scope stack.  This is only used in
C mode.
*/
{
  a_scope_depth	depth;
  a_boolean	result = FALSE;

  for (depth = depth_scope_stack ;depth != DEPTH_OF_FILE_SCOPE; depth--) {
    if (scope_stack[depth].kind == (a_scope_kind)sck_class_struct_union) {
      if (scope_stack[depth].assoc_type == tag_type) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* tag_currently_being_defined */


static void check_qualified_tag_access(a_boolean	is_tag_definition)
/*
Do access and ambiguity checking on a qualified name being processed
by scan_tag_name.  is_tag_definition is TRUE if the tag is being defined.
*/
{
  /* The Microsoft compiler does check the access of qualified tag
     references. */
  if (microsoft_bugs) {
    check_for_ambiguity(&locator_for_curr_id);
  } else {
    check_ambiguity_and_verify_access(&locator_for_curr_id);
  }  /* if */
  if (is_tag_definition && any_deferred_access_checks()) {
    /* When defining a class member outside of its class definition
       using a qualified name, any access errors that may have been
       detected when scanning the qualified name should be suppressed.
       This context is not really a declarator, but the concept is the
       same as suppressing access errors when scanning the declarator
       of a member function or static data member. */
    discard_declarator_access_errors();
  }  /* if */
}  /* check_qualified_tag_access */


static a_boolean is_namespace_for_type_info_definition(void)
/*
Returns TRUE if the current namespace is the one in which
type_info may be defined.
*/
{
  a_boolean	result = FALSE;

  if (type_info_in_namespace_std && !ignore_std_namespace) {
    /* When type_info is required to be defined in the std namespace,
       make sure we are in that namespace now. */
    a_namespace_ptr	nsp;
    nsp = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
    if (nsp == symbol_for_namespace_std->variant.namespace_info.ptr) {
      result = TRUE;
    }  /* if */
  } else {
    /* When type_info is not in std, it must be in the global namespace.
       This is also the case when using the g++ compatibility feature
       where the std namespace is an alias for the global namespace. */
    result = depth_scope_stack == DEPTH_OF_FILE_SCOPE;
  }  /* if */
  return result;
}  /* is_namespace_for_type_info_definition */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_symbol_ptr scan_tag_name(a_symbol_kind     tag_kind,
                                  a_symbol_locator  *locator,
                                  a_boolean         *is_friend_decl,
                                  a_boolean         *check_for_vacuous_decl,
                                  a_boolean         is_ref_within_new_expr,
                                  a_scope_depth     *effective_decl_level,
                                  a_boolean         *tag_resolution,
                                  a_boolean         *is_predeclared_type_decl,
                                  a_decl_pos_block  *decl_pos_block)
/*
Scan a tag identifier for a class, struct, union, or enum declaration.
If a tag symbol already exists for the identifier, return a pointer to
that symbol; otherwise return NULL.  If there is no identifier or if there
is an error, return NULL.

*is_friend_decl is TRUE when the declaration appears to be of the form
"friend class X;"; if it turns out that no semicolon follows the identifier,
however, the flag will be reset to FALSE and a normal lookup will be done.
*check_for_vacuous_decl is TRUE when the context permits a declaration like
"struct x;".  is_ref_within_new_expr is TRUE when the declaration appears
inside a new expression.  *effective_decl_level will have been initialized
to decl_scope_level by the caller; it may be changed in C++ for a forward
reference to a tag within a function prototype or a class definition -- the
tag is entered into the innermost non-class/non-prototype scope, which is
returned as its effective declaration level.  *tag_resolution is returned
TRUE if this is the definition of a previously declared incomplete class or
enum.  *is_predeclared_type_decl is returned TRUE if this is the explicit
declaration of a predeclared type like type_info in C++ or _GUID in Microsoft
mode.

This routine may look more complicated than is necessary -- it isn't.
This routine can either be matching up a definition with a previous
declaration or may be entering a definition in a new scope.  The lookups
have to be done very carefully to create new entries only when required
and to find existing entries only when appropriate.  Exercise great
caution when modifying this routine.
*/
{
  a_symbol_ptr               tag_sym = NULL, templ_sym = NULL;
  a_token_kind               next_tok;
  a_boolean	             err = FALSE;
  a_boolean	             tag_err = FALSE;
  a_boolean	             is_tag_definition = FALSE;
  an_identifier_options_set  options;
  a_scope_depth              computed_decl_level;

  db_enter(3, "scan_tag_name");
  *tag_resolution = FALSE;
  /* Coalesce the identifier that follows the class, struct, union, or
     enum keyword. */
  options = GID_TEMPLATE_ARGS_OPTIONAL | GID_IMPLICIT_TYPE_CONTEXT;
  if (is_ref_within_new_expr) options |= GID_IS_NEW_TYPE_NAME;
  if (is_generalized_identifier_start(options)) {
    /* Determine whether this is a definition or something else (a
       declaration or an elaborated type specifier). */
    next_tok = next_token();
    if (next_tok == tok_lbrace ||
        (next_tok == tok_colon && C_dialect == C_dialect_cplusplus &&
         tag_kind != (a_symbol_kind)sk_enum_tag && !is_ref_within_new_expr)) {
      /* The token following the tag marks the start of a class or enum
         definition. Determine whether it is the resolution of a previous
         incomplete declaration. */
      /* Note that we had to check the is_ref_within_new_expr flag because a
         colon has a different meaning in an expression context than in a
         declaration context (namely, it may belong to a ?: operator). */
      is_tag_definition = TRUE;
    }  /* if */
  } else {
    /* Identifier is missing. */
    error(ec_exp_identifier);
    tag_err = TRUE;
  }  /* if */
  if (!C_mode() || microsoft_mode) {
    /* The effective scope depth for the current declaration may need to be
       reset.  Compute the depth to which it should be reset now, since it's
       used for processing declarations of predeclared types.  The actual
       resetting, if required, will be done later. */
    /* In C++ mode or Microsoft C, don't enter tags in prototype scopes. */
    a_boolean     done = FALSE;
    a_symbol_ptr  instance_sym;

    computed_decl_level = *effective_decl_level;
    do {
      switch (scope_stack[computed_decl_level].kind) {
        case sck_template_instantiation:
          /* We hit a template instantiation scope.  If the instantiation
             scope is for a real instantiation then effective_decl_level
             will be set to file scope.  If it is a prototype or nonreal
             instantiation then it will be left pointing at the instantiation
             scope.  The problem is that a class declared in a prototype
             instantiation may not be a real type, but we don't know yet.
             We want to avoid contaminating the name space, etc., so it gets
             declared in the instantiation scope. */
          instance_sym = scope_stack[computed_decl_level].instance_sym;
          if (instance_sym == NULL ||
              !is_nonreal_instance_class_symbol(instance_sym)) {
            computed_decl_level = depth_innermost_namespace_scope;
          }  /* if */
          /*FALLTHROUGH*/
        case sck_file:
        case sck_namespace:
        case sck_namespace_extension:
        case sck_function:
        case sck_block:
          done = TRUE;
          break;
        default:
          computed_decl_level--;
      }  /* if */
    } while (!done);
  }  /* if */
  if (!C_mode() && !tag_err) {
    /* Check for the presence of a qualified name.  If we have a qualified
       name, do the lookup in a manner that will only find tag names. */
    if (coalesce_and_lookup_qualified_name(options, ilm_tag, &err) ||
        (curr_token == tok_identifier &&
         locator_for_curr_id.is_template_id)) {
      if (err) {
        /* An error occurred while scanning or looking up the qualified
           name. */
        tag_err = TRUE;
      } else {
        /* Do access and ambiguity checking on the name. */
        check_qualified_tag_access(is_tag_definition);
        tag_sym = locator_for_curr_id.specific_symbol;
        if (tag_sym != NULL) {
          reduce_projection_symbol_to_fundamental_symbol(tag_sym);
          if (tag_sym->kind != tag_kind) {
            /* A qualified name is being used with a different tag kind than
               that of its declaration.  Issue an error. */
            if (is_type_template_param_symbol(tag_sym)) {
                /* This is a template parameter during a prototype
                   instantiation. Don't issue an error.  This will be checked
                   during real instantiations. */
            } else if (tag_sym->kind == (a_symbol_kind)sk_class_template) {
              templ_sym = tag_sym;
              tag_sym = NULL;
            } else if (is_template_class_symbol(tag_sym) &&
                       tag_kind != (a_symbol_kind)sk_enum_tag &&
                       tag_sym->kind != (a_symbol_kind)sk_enum_tag) {
              /* Caller will issue the diagnostic. */
            } else {
              pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                             &locator_for_curr_id.source_position,
                             name_of_symbol_kind(tag_kind), tag_sym);
              tag_sym = NULL;
              tag_err = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (curr_token == tok_identifier &&
               decl_scope_level == depth_innermost_namespace_scope &&
               tag_kind != (a_symbol_kind)sk_enum_tag) {
      /* Look up what may be a class template symbol.  If the name is
         the start of a qualified name (e.g., A::B) or has a template
         argument list (e.g., A<T>) it will have been coalesced by the
         call to coalesce_and_lookup_qualified_name.  If it just a simple
         identifier (e.g., "A") we need look it up and coalesce it here. */
      templ_sym = normal_id_lookup(&locator_for_curr_id, IDL_LINKAGE_LOOKUP);
    }  /* if */
    if (templ_sym != NULL) {
      /* Check for an identifier that is a class template name.  A class
         template name at file scope must have an argument list.  A use of a
         class template name in another scope is actually a declaration of a
         new class that has nothing to do with the template. */
      /* There are two situations that need to be handled: this could be the
         first time we are scanning this template reference -- in which case
         we to scan the arguments (using coalesce_template_class_reference).
         Alternately, the arguments may have already been coalesced.  If the
         symbol is a class template then we need to scan the arguments.  If
         the symbol is a template class symbol then the arguments have already
         been scanned and we should simply use the symbol returned by
         normal_id_lookup. */
      if (templ_sym->kind == (a_symbol_kind)sk_class_template) {
        tag_sym = coalesce_template_class_reference(
                                   templ_sym,
                                   microsoft_bugs ? GID_TEMPLATE_ARGS_OPTIONAL
                                                  : GID_NO_OPTIONS,
                                   &err);
        if (tag_sym->kind == (a_symbol_kind)sk_class_template) {
          if (microsoft_bugs) {
            /* In Microsoft bugs mode, the following is accepted:
                  template<class T> struct S;
                  struct S; // ignored
            */
            if (next_token() == tok_semicolon) {
              tag_sym = tag_sym->variant.template_info
                             ->variant.class_template.prototype_instantiation;
              warning(ec_not_a_class_or_struct_name);
            } else {
              error(ec_not_a_class_or_struct_name);
              err = TRUE;
            }  /* if */
          } else {
            check_assertion(err);
          }  /* if */
        }  /* if */
        /* If an error occurred while scanning the template arguments, set
           tag_sym to NULL.  The caller is not prepared for it to point to
           an error symbol. */
        if (err) {
          tag_sym = NULL;
          tag_err = TRUE;
        }  /* if */
      } else if (is_template_class_symbol(templ_sym)) {
        /* Use the symbol pointer from the locator (returned by
           normal_id_lookup earlier).  The template class reference has
           already been coalesced. */
        tag_sym = templ_sym;
      } else {
        /* Don't prejudice subsequent lookups. */
        clear_specific_symbol(locator_for_curr_id);
      }  /* if */
    }  /* if */
    if (tag_sym == NULL && !tag_err &&
        !locator_for_curr_id.is_qualified_name &&
        computed_decl_level == depth_innermost_namespace_scope &&
        tag_kind != (a_symbol_kind)sk_enum_tag) {
      /* See if this is an explicit declaration of class type_info, which was
         already "predeclared".  If it is, reuse the original symbol. */
      a_type_ptr       predeclared_type = NULL;
      a_symbol_ptr     type_info_sym;
      a_namespace_ptr  nsp = NULL;

      check_assertion(type_of_type_info != NULL);
      type_info_sym = (a_symbol_ptr)type_of_type_info->
                                           source_corresp.assoc_info;
      /* Note that we need a match not only on the name but also on the
         namespace.  This depends on whether the implicitly declared type_info
         is expected to be in namespace "std" or in the global namespace. */
      if (locator_for_curr_id.symbol_header == type_info_sym->header) {
        a_pending_pragma_ptr  ppp;

        if (computed_decl_level == (DEPTH_OF_FILE_SCOPE + 1)) {
          nsp = scope_stack[computed_decl_level].il_scope->
                                                  variant.assoc_namespace;
        }  /* if */
        if (is_namespace_for_type_info_definition()) {
          /* The identifier is indeed "type_info".  Check for the pragma that
             specifically identifies it as the type_info that is returned by
             typeid (typically, the type_info defined in typeinfo.h). */
          ppp = extract_specific_pragmas((a_pragma_kind)pk_define_type_info,
                                         type_info_sym, (a_statement_ptr)NULL,
                                         /*curr_scope_only=*/TRUE);
          if (ppp != NULL) {
            /* This is the one. */
            tag_sym = type_info_sym;
            /* RTTI is outside the "Embedded C++" subset. */
            feature_is_not_part_of_embedded_cplusplus_subset(
                                                &pos_curr_token,
                                                ec_rtti_in_embedded_cplusplus);
            free_pending_pragma_list(ppp);
          } else {
#if !PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED
            /* The pragma is not required (e.g., when the C++ generating back
               end is in use). */
            tag_sym = type_info_sym;
#else /* PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED */
#if ABI_CHANGES_FOR_RTTI
            if (type_info_sym->decl_scope == NO_SCOPE_DEPTH) {
              /* Not yet explicitly redeclared. */
              /* Run-time support for RTTI declares type_info, so consider the
                 name to be reserved. */
              pos_st_error(ec_conflicts_with_predeclared_type_info,
                           &locator_for_curr_id.source_position,
                           (char *)(type_info_in_namespace_std
                                            ? "std::type_info" : "type_info"));
            }  /* if */
            tag_sym = type_info_sym;
#endif /* ABI_CHANGES_FOR_RTTI */
#endif /* !PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED */
          }  /* if */
        }  /* if */
        if (tag_sym == type_info_sym &&
            tag_sym->decl_scope == NO_SCOPE_NUMBER) {
          predeclared_type = type_of_type_info;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_mode && !C_mode() &&
                 computed_decl_level == DEPTH_OF_FILE_SCOPE) {
        /* Similarly, check for predeclared "struct _GUID". */
        a_symbol_ptr  guid_sym;
        check_assertion(type_of_guid != NULL);
        guid_sym = (a_symbol_ptr)type_of_guid->source_corresp.assoc_info;
        if (locator_for_curr_id.symbol_header == guid_sym->header) {
          tag_sym = guid_sym;
          if (tag_sym->decl_scope == NO_SCOPE_NUMBER) {
            predeclared_type = type_of_guid;
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (predeclared_type != NULL) {
        /* If the type_info or _GUID symbol has no scope number, it hasn't
           been added to the symbol table yet.  Use the current source
           position. */
        tag_sym->decl_position = locator_for_curr_id.source_position;
        reenter_symbol(tag_sym, computed_decl_level, /*suppress_error=*/FALSE);
        /* Call set_source_corresp again to get everything in sync. */
        set_source_corresp(&(predeclared_type->source_corresp), tag_sym);
        set_namespace_membership(tag_sym,
                                 &(predeclared_type->source_corresp), nsp);
        /* The referenced flag may have been reset by set_source_corresp. */
        predeclared_type->source_corresp.referenced = tag_sym->referenced;
        add_to_types_list(predeclared_type, computed_decl_level);
        *is_predeclared_type_decl = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!tag_err) {
    if (locator_for_curr_id.is_qualified_name) {
      /* A "vacuous declaration" may not involve a qualified name: "struct x;"
         is okay, but "struct A::x;" is not. */
      *check_for_vacuous_decl = FALSE;
    }  /* if */
    if (locator_for_curr_id.is_operator_name ||
        locator_for_curr_id.is_conversion_name) {
      /* Issue an error for something like "class operator+" or
         "class operator int". */
      pos_error(ec_operator_name_not_allowed,
                &locator_for_curr_id.source_position);
      tag_err = TRUE;
      tag_sym = NULL;
    }  /* if */
  }  /* if */
  if (tag_sym != NULL) {
    /* Tag symbol is a qualified name or a template class reference. */
    /* Return a copy of the locator to the caller. */
    *locator = locator_for_curr_id;
  } else if (tag_err) {
    /* An error occurred while handling a qualified name or a template
       reference earlier. */
  } else if (is_error_locator(locator_for_curr_id)) {
    /* There was some other error on the lookup -- e.g., maybe this was
       an ambiguous namespace projection. */
    tag_err = TRUE;
  } else {
    a_boolean  is_vacuous_declaration = FALSE;

    /* Save the symbol locator for this identifier. */
    *locator = locator_for_curr_id;
    if (next_tok == tok_semicolon) {
      if (*check_for_vacuous_decl && C_dialect != C_dialect_pcc) {
        /* This may be a "vacuous declaration" (e.g. "struct S;" or "enum E;").
           The effect of a vacuous declaration (unless we are in pcc mode) is
           to establish the name in the current scope, even if the tag name
           exists in a containing scope or is inherited from a base class. */
        is_vacuous_declaration = TRUE;
      }  /* if */
    } else if (*is_friend_decl) {
      /* In a friend class declaration a semicolon will always follow the
         identifier.  It doesn't here -- maybe it's something like:
           friend class X *f();
         i.e., the "friend" specifier doesn't apply to the class.  Also allow
         for the case where the friend class declaration is a definition
         (which is an error reported elsewhere). */
      if (next_tok != tok_colon && next_tok != tok_lbrace) {
        *is_friend_decl = FALSE;
      }  /* if */
    }  /* if */
    if (is_tag_definition || is_vacuous_declaration) {
      /* Look for a tag symbol in the current scope.  If the tag kind does
         not match the tag being processed, issue an error. */
      /* Note: we only call curr_scope_id_lookup for declarations, not for
         references within the declaration of something else, because of
         cases like this:
           class A;
           namespace { class A; }
           class A *p;                // Error -- ambiguous reference
           class A { };               // Okay -- defines ::A
         curr_scope_id_lookup will return ::A only, whereas normal_id_lookup
         will return a projection symbol that informs of the ambiguity. */
      tag_sym = curr_scope_id_lookup(locator, IDL_MUST_BE_TAG);
      if (tag_sym != NULL && is_injected_class_symbol(tag_sym)) {
        /* Ignore an injected class symbol, which would be found for this sort
           of case:
             struct A { struct A { ... }; };
        */
        tag_sym = NULL;
      }  /* if */
    }  /* if */
    if (is_tag_definition) {
      if (tag_sym != NULL) {
        /* The tag has already appeared in the current scope. */
        if (!tag_sym->defined &&
            (!C_mode() ||
             !tag_currently_being_defined(type_symbol_type(tag_sym)))) {
          /* Resolution of a previous incomplete declaration.  In C mode, make
             sure that an incomplete type is not in the process of being
             defined. */
          *tag_resolution = TRUE;
        } else {
          /* Redeclaration of a tag that has already been defined.  Set
             tag_sym to NULL and let enter_symbol issue an error. */
          tag_sym = NULL;
        }  /* if */
      }  /* if */
    } else if (tag_sym == NULL) {
      /* This is the first appearance of the tag in the current scope.  This
         is not its definition, so it is either a reference to an existing
         tag or a declaration of a new (incomplete) tag. */
      /* Check for a cfront bug (violation of ARM 7.1.3, which says a typedef
         name may not appear in an elaborated type specifier) which allows
         a typedef name as long as it refers to a class/struct/union type. */
      if (any_cfront_mode() && tag_kind != (a_symbol_kind)sk_enum_tag) {
        /* Look up the name again in the current scope, but this time don't
           restrict the search to tag names. */
        a_symbol_ptr  sym;

        check_assertion(locator->specific_symbol == NULL);
        sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS);
        if (sym != NULL) {
          /* Found a symbol of the same name that was declared in the current
             scope. */
          if (sym->kind == (a_symbol_kind)sk_type) {
            /* Name is already declared in the current scope as a typedef. */
            a_type_ptr  tp = skip_typerefs(sym->variant.type.ptr);
            if (is_immediate_class_type(tp) &&
                ((tag_kind == (a_symbol_kind)sk_union_tag) ==
                 (tp->kind == (a_type_kind)tk_union))) {
              /* This is the special case.  Return an sk_type symbol instead
                 of the normally expected sk_class_or_struct_tag. */
              tag_sym = sym;
              goto done;
            }  /* if */
          }  /* if */
          /* Reset the specific_symbol pointer to avoid prejudicing any
             subsequent lookup. */
          clear_specific_symbol(*locator);
        }  /* if */
      }  /* if */
      if (is_vacuous_declaration) {
        /* This is a vacuous declaration.  Leave tag_sym set to NULL to force
           creation of a new symbol in the current scope. */
      } else {
        /* This may be a reference to an existing tag, either from the
           current scope or from a containing scope or a base class. */
        tag_sym = curr_tag_symbol(locator, tag_kind, *is_friend_decl);
        if (tag_sym == NULL) {
          /* We will need to enter an incomplete tag that may be resolved
             later.  Just leave tag_sym NULL.  In C it will be entered at
             the scope level indicated by decl_scope_level.  In C++ we need
             to pop out to the innermost non-class/non-prototype scope.
             (For example, to introduce class name B in a parameter
             declaration of a member function within the definition of class
             A does not introduce the name of nested class A::B; rather, B
             is entered in the same scope as A.) */
          /* Note: in Microsoft C mode, tags are not entered in function
             prototype scopes. */
          if (C_dialect == C_dialect_cplusplus || microsoft_mode) {
            *effective_decl_level = computed_decl_level;
          }  /* if */
        } else if (is_injected_class_symbol(tag_sym)) {
          /* A symbol representing an injected class name.  Use the tag symbol
             associated with the class in its place. */
          tag_sym = (a_symbol_ptr)tag_sym->variant.type.ptr->
                                                 source_corresp.assoc_info;
        } else if (!C_mode() && tag_sym->kind == (a_symbol_kind)sk_type) {
          /* The tag is a template parameter type.  A diagnostic will have
             been issued in curr_tag_symbol.  This usage is still supported
             in the front end although the feature is no longer permitted.
             An error will have been issued in strict mode. */
          goto done;
        }  /* if */
      }  /* if */
      if (tag_sym == NULL && tag_kind == (a_symbol_kind)sk_enum_tag &&
          !is_error_locator(*locator)) {
        /* Since tag_sym was not found, this is either a vacuous declaration
           or a reference to an incomplete (because not yet declared) type.
           In either case this is non-standard for enums.  It is allowed as
           an extension by analogy with classes. */
        if (strict_ansi_mode) {
          /* Incomplete enum declarations are nonstandard in C and C++. */
          pos_diagnostic(strict_ansi_error_severity,
                         ec_nonstd_forward_decl_enum,
                         &locator->source_position);
        }  /* if */
      }  
    }  /* if */
    if (!tag_err && tag_sym != NULL && tag_sym->kind != tag_kind) {
      an_error_severity  severity;
      if (any_cfront_mode() && tag_kind != (a_symbol_kind)sk_enum_tag &&
          tag_sym->kind != (a_symbol_kind)sk_enum_tag) {
        /* Allow mixing of struct/class and union in cfront mode. */
        severity = (an_error_severity)es_warning;
      } else {
        severity = (an_error_severity)es_error;
        tag_err = TRUE;
      }  /* if */
      pos_stsy_diagnostic(severity,
                          ec_tag_kind_incompatible_with_declaration,
                          &locator->source_position,
                          name_of_symbol_kind(tag_kind), tag_sym);
      if (tag_err) tag_sym = NULL;
    }  /* if */
  }  /* if */
done:
  if (tag_err) {
    /* If an error occurred while scanning the tag, make the locator that
       is returned to the caller an error locator. */
    set_to_error_locator(*locator);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* The end position of the current token is the end of the identifier
       and (as far as we know so far) the end of the specifier to which the
       class or enum declaration may belong. */
    decl_pos_block->identifier_range.end = end_pos_curr_token;
    decl_pos_block->specifiers_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Now that we have completed the lookup on the tag identifier we can
     advance past it. */
  (void)get_token();
  db_exit();
  return tag_sym;
}  /* scan_tag_name */


static void set_name_linkage_for_type(a_type_ptr  tp)
/*
Set the name_linkage field of the class or enum type pointed to by tp.
*/
{
  a_source_correspondence  *scp = &tp->source_corresp;

  check_assertion(is_immediate_class_type(tp) || is_immediate_enum_type(tp));
  if (scp->is_class_member) {
    /* A nested class or enum has the same linkage as the class of which it
       is a member. */
    scp->name_linkage = scp->parent.class_type->source_corresp.name_linkage;
  } else if (any_cfront_mode() &&
             depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
    /* In cfront mode -- unless this is a class or enum declared within a
       namespace -- give it internal linkage by default.  It may be promoted
       later, based on how it's used, etc. */
    scp->name_linkage = (a_name_linkage_kind)nlk_internal;
  } else {
    /* Ordinary default for classes and enums is C++ external linkage. */
    scp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
  }  /* if */
}  /* set_name_linkage_for_type */


static a_boolean namespace_scope_should_be_pushed(a_symbol_ptr       tag_sym,
                                                  a_symbol_locator   *loc,
                                                  a_source_position  *pos,
                                                  a_boolean          *err)
/*
The class or enum indicated by tag_sym is being defined, having originally
been declared a namespace member.  Determine whether it's legal in this
context (if not, issue a diagnostic and return *err set to TRUE), and if it
is legal, determine whether a scope stack entry needs to be pushed (in which
case return TRUE).
*/
{
  a_boolean    should_be_pushed = FALSE;
  a_scope_ptr  scope = scope_stack[decl_scope_level].il_scope;

  if (!namespace_is_enclosed_by_curr_scope(tag_sym)) {
    /* This declaration appears within a namespace scope in which the name
       cannot be defined -- it is a member (directly or indirectly) of a
       namespace that is not enclosed by the current namespace scope (see
       WP 7.3.1.4). */
    pos_sy_error(ec_bad_scope_for_definition, pos, tag_sym);
    *err = TRUE;
  } else if (scope->kind != (a_scope_kind)sck_namespace ||
             tag_sym->parent.namespace_ptr !=
                                 scope->variant.assoc_namespace) {
    /* Push a namespace extension scope. */
    should_be_pushed = TRUE;
  } else if (loc->is_qualified_name) {
    /* A namespace-qualified name that refers to the current namespace is
       not allowed in a definition. */
    check_assertion_str2(scope->kind == (a_scope_kind)sck_namespace &&
                         tag_sym->parent.namespace_ptr ==
                                         scope->variant.assoc_namespace,
                         "namespace_scope_should_be_pushed:",
                         "expected curr-namespace qualified name");
    pos_error(ec_qualifier_in_namespace_member_decl, pos);
    *err = TRUE;
  }  /* if */
  return should_be_pushed;
}  /* namespace_scope_should_be_pushed */


static void check_nested_class_redeclaration(
                                 a_symbol_ptr            tag_sym,
                                 a_source_position       *tag_position,
                                 a_boolean               is_class_definition,
                                 a_boolean               is_friend_decl,
                                 a_boolean               is_qualified_name,
                                 a_boolean               *declares_something)
/*
Helper for class_specifier (below) that verifies whether the use of an 
elaborated type-specifier is really a redeclaration, and if so performs
various checks related to access and the use of a qualified name.
This function is called if a class-specifier is seen in the scope of another
class type, and the tag of that specifier was already declared in that scope.
In: tag_sym is a pointer to the symbol associated with the elaborated name;
tag_position is the position of the elaborated name (tag) that was just
scanned; is_class_definition and is_friend_decl are set when the tag is used
to define the nested type or introduce a friend declaration. If the tag-name
was qualified, is_qualified_name is set as well.
Out: *declares_something is set to false if the elaborated type specifier
does not introduce a definition and is not followed by a semicolon; otherwise
it is left unchanged.
*/
{
  a_type_ptr  type = tag_sym->variant.class_struct_union.type;

  if (is_class_definition && is_qualified_name) {
    /* Issue a warning on a case like this:
         class A {
           class N;
           class A::N { ... };    // Qualified name is not allowed
         };
       -- a warning rather than an error for consistency with other
       member declarations (see simplify_curr_class_qualified_name). */
    pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                   ec_qualifier_in_member_declaration, tag_position);
  }  /* if */
  if (!is_class_definition && curr_token != tok_semicolon) {
    /* For example:
          struct S { struct N {}; private: struct N* f(); };
       is fine---there is no redeclaration of struct N here. */
    *declares_something = FALSE;
  } else if (!is_friend_decl) {
    /* Be sure the access is consistent on the redeclaration. */
    a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
    if (ssep->current_access != type->source_corresp.access) {
      /* The access specified for the previous declaration does not
         correspond to the access for current declaration. */
      an_error_code      error_code;
      an_error_severity  severity;

      /* If this is a definition, use the current access instead of
         that specified on the original declaration. */
      if (is_class_definition) {
        type->source_corresp.access = ssep->current_access;
        error_code = ec_redecl_changes_access;
      } else {
        error_code = ec_cannot_change_access;
      }  /* if */
      severity = strict_ansi_mode ?
                   strict_ansi_discretionary_severity : es_warning;
      pos_sy_diagnostic(severity, error_code, tag_position, tag_sym);
    }  /* if */
    if ((tag_sym->defined || !is_class_definition) &&
        /* Exclude ordinary (nonprototype) template instantiations: */
        !(type->variant.class_struct_union.is_template_class &&
          !type->variant.class_struct_union.is_prototype_instantiation &&
          !type->variant.class_struct_union.is_specialized)) {
      /* The only standard conforming nested class redeclaration is a
         definition following a nondefining declaration. */
      pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_invalid_nested_class_redecl, tag_position);

    }  /* if */
  }  /* if */
} /* check_nested_class_redeclaration */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_boolean class_specifier(a_boolean         vacuous_decl_allowed,
                                 a_boolean         is_friend_decl,
                                 a_boolean         is_typedef,
                                 a_boolean         is_ref_within_new_expr,
				 a_boolean         is_explicit_instantiation,
                                 a_boolean         is_template_specialization,
                                 a_type_ptr        *type_ptr,
                                 a_boolean         *declares_something,
                                 a_boolean         *defines_something,
                                 a_decl_pos_block  *decl_pos_block)
/*
Scan a class-specifier (3.5.2.1), which declares a struct or
union type.  The syntax is

9
        class-specifier:
                class-head { member-list    }
                                        opt

        class-head:
                class-key identifier    base-spec
                                    opt          opt
                class-key class-name base-spec
                                              opt

        class-key
                class
                struct
                union

9.2
        member-list
                member-declaration member-list
                                              opt
                access-specifier : member-list

        member-declaration:
                decl-specifiers    member-declarator-list    ;
                               opt                       opt
                function-definition ;
                                     opt
                qualified-name ;

        member-declarator-list:
                member-declarator
                member-declarator-list , member-declarator

        member-declarator
                declarator pure-specifier
                                         opt
                identifier    : constant-expression
                          opt

        pure-specifier
                = 0

The type is returned in *type_ptr. *declares_something is set to indicate
whether or not this specifier declares something, and *defines_something
to indicate whether the class/struct/union is actually defined.
is_explicit_instantiation is TRUE if the declaration being scanned
is part of an explicit instantiation.  This causes a class specifier
of the form "class A<int>" to not be considered a specific declaration of
the template.  is_typedef is TRUE if the class specifier is being typedefed.
*/
{
  a_symbol_kind           tag_kind;
  a_type_kind             type_kind;
  a_symbol_locator        locator;
  a_symbol_ptr            tag_sym, error_tag_sym = NULL;
  a_symbol_ptr            parent_sym;
  a_boolean               tag_id_present;
  a_type_ptr              class_type;
  a_boolean               is_local_class = FALSE;
  a_boolean               is_template_class_instantiation = FALSE;
  a_boolean               tag_resolution = FALSE;
  a_boolean               err = FALSE;
  a_scope_depth           effective_decl_level = decl_scope_level;
  a_scope_depth           orig_decl_level = decl_scope_level;
  a_boolean               is_class_definition;
  a_source_position       decl_start_pos;
  a_scope_stack_entry_ptr ssep;
  a_source_position       tag_position;
  a_symbol_reference_kind srk_flags;
  a_boolean               delayed_nested_class_def = FALSE;
  a_boolean               namespace_extension_pushed = FALSE;
  a_boolean               is_redeclaration = FALSE;
  a_boolean               is_template_specific_decl = FALSE;
  a_boolean               is_predeclared_type_decl = FALSE;
  a_decl_pos_block        local_decl_pos_block;
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  an_extended_decl_info_block
                          extended_decl_info;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

  db_enter(3, "class_specifier");
  *declares_something = FALSE;
  *defines_something = FALSE;
  decl_start_pos = pos_curr_token;
  clear_decl_pos_block(&local_decl_pos_block);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  local_decl_pos_block.specifiers_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  clear_extended_decl_info_block(extended_decl_info);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  /* Determine whether this is a template class instantiation or a local
     class (one being declared within a function scope). */
  ssep = &scope_stack[depth_scope_stack];
  if (depth_innermost_function_scope != NO_SCOPE_NUMBER ||
      inside_local_class) {
    /* This declaration appears within a function or block scope, or else it
       is a nested class declaration within a local class.  In either case,
       it is a local class. */
    is_local_class = TRUE;
  }  /* if */
  if (curr_token == tok_class ||
      curr_token == tok_struct ||
      curr_token == tok_union) {
    /* Skip over "class", "struct", or "union", remembering which appears. */
    if (curr_token == tok_union) {
      tag_kind = (a_symbol_kind)sk_union_tag;
      type_kind = (a_type_kind)tk_union;
    } else {
      tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
      type_kind = (a_type_kind)(curr_token == tok_struct ?
                                                    tk_struct : tk_class);
    }  /* if */
    (void)get_token();
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (!C_mode() && (microsoft_mode or_near_and_far_enabled())) {
      a_boolean  local_err;

      /* Scan the decl-modifiers that apply to an entire class.  They will be
         passed on to scan_function_definition and applied to each member
         declaration, where appropriate. */
      scan_extended_decl_modifiers(/*is_class_decl=*/TRUE,
                                   /*is_member_decl=*/FALSE,
                                   &extended_decl_info, &local_err);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    /* If there is an identifier next, it is a tag.  It can be the declaration
       of a new tag or a reference to an existing tag.  Although it is an
       error, also be on the lookout for a qualified name. */
    tag_id_present = curr_token == tok_identifier ||
                     (curr_token == tok_colon_colon &&
                      next_token() == tok_identifier) ||
                     curr_token == tok_operator;        /* Error case. */
  } else {
    /* class_specifier is called with is_friend_decl TRUE only when the name
       has not yet been declared; this happens in cfront compatibility mode
       only.  Default kind is "class" when a class is introduced by a friend
       declaration.   (In fact, there is a slight incompatibility here, since
       in cfront 2.1 this can also be turned into a union declaration.) */
    check_assertion(is_friend_decl &&
                    curr_token == tok_identifier &&
                    locator_for_curr_id.has_been_coalesced);
    tag_id_present = TRUE;
    tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
    type_kind = (a_type_kind)tk_class;
  }  /* if */
  if (tag_id_present) {
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    tag_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    local_decl_pos_block.identifier_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    *declares_something = TRUE;
    check_assertion(!vacuous_decl_allowed || !is_friend_decl);
    tag_sym = scan_tag_name(tag_kind, &locator, &is_friend_decl,
                            &vacuous_decl_allowed, is_ref_within_new_expr,
                            &effective_decl_level, &tag_resolution,
                            &is_predeclared_type_decl, &local_decl_pos_block);
  }  /* if */
  if (tag_id_present) {
    if (tag_sym != NULL) {
      if (is_friend_decl) {
        /* A friend declaration: if the identifier was a qualified name
           (either namespace qualified or globally qualified), be sure the
           lookup did not find a namespace-projection symbol.  If it did,
           issue an error. */
        if (locator.is_qualified_name &&
            locator.specific_symbol->kind ==
                                  (a_symbol_kind)sk_namespace_projection) {
          a_namespace_ptr  nsp = qualifier_namespace_ptr(locator);
          if (nsp == NULL) {
            /* Must be something like this:
                 namespace N { class X; }
                 using N::X;
                 class Y {
                   friend class ::Y;      // Error
                 };
            */
            check_assertion(locator.is_file_scope_qualified_name);
            pos_st_error(ec_name_not_tag_in_file_scope,
                         &locator.source_position,
                         locator.symbol_header->identifier);
          } else {
            /* Must be something like this:
                 namespace N { class X; }
                 namespace M { using N::X; }
                 class Y {
                   friend class M::Y;     // Error
                 };
            */
            pos_stsy_error(ec_not_an_actual_member, &locator.source_position,
                           locator.symbol_header->identifier,
                           (a_symbol_ptr)nsp->source_corresp.assoc_info);
          }  /* if */
          set_to_named_error_locator(locator);
          tag_sym = NULL;
        }  /* if */
      } else if ((is_explicit_instantiation || is_template_specialization) &&
                 !is_declarator_start()) {
        /* This is an explicit instantiation directive or a specialization
           of a class template.  Check for member templates from a base class
           that are specified using the derived class name as qualifier. */
        if (locator.is_class_member && locator.is_qualified_name &&
            locator.specific_symbol != NULL &&
            is_template_instance_class_symbol(locator.specific_symbol) &&
            locator.specific_symbol->parent.class_type !=
                                           locator.parent.class_type) {
          /* Specifying an inherited name in an explicit instantiation
             directive or in a template specialization declaration is
             disallowed. */
          pos_error(ec_inherited_member_not_allowed, &locator.source_position);
        }  /* if */
      }  /* if */
    }  /* if */
    if (tag_sym != NULL) {
      /* Check for tag mismatch.  This can only happen when an instance of a
         class template is being referenced in an elaborated type specifier. */
      if (tag_sym->kind == (a_symbol_kind)sk_type) {
        if (tag_sym->variant.type.ptr->kind ==
                                             (a_type_kind)tk_template_param) {
          /* Template param used in with a class-key -- for instance:
               template <class T> class A {
                 class T x;
               };
             During prototype instantiation we have to assume that T can be a
             valid class name.  Therefore "class T x" is treated as synonymous
             with "T x".  In addition, "friend class T" is also supported. */
          if (is_friend_decl || tag_sym->is_class_member) {
            /* If the form is similar to "struct T::X", we must preserve the
               elaborator (for disambiguation purposes).  So use a proxy
               class instead of the raw template parameter entity.  For a
               case like "friend class T;" we must also ensure that the
               returned symbol is a class type. */
            a_type_ptr  proxy_type = proxy_class_for_template_param(
                                                   tag_sym->variant.type.ptr);
            proxy_type->kind = type_kind;
            tag_sym = (a_symbol_ptr)proxy_type->source_corresp.assoc_info;
            tag_sym->kind = tag_kind;
          }  /* if */
#if CHECKING
        } else if (any_cfront_mode()) {
          /* Cfront bug that allows this:
               typedef class A B;
               class B;
               class B *pa;
             The current declaration must not be a definition and the
             typedef name must refer to a class type. */
          check_assertion(is_class_struct_union_type(tag_sym->
                                                       variant.type.ptr));
        } else {
          internal_error("class_specifier: invalid sk_type tag_sym");
#endif /* CHECKING */
        }  /* if */
      } else if (tag_sym->kind != tag_kind) {
        if (is_nonreal_instance_class_symbol(tag_sym)) {
          /* Ignore a union/nonunion mismatch on nonreal classes. */
        } else if (is_template_class_symbol(tag_sym)) {
          /* Error -- tag-kind mismatch in a specialization. */
          pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos,
                       tag_sym->variant.class_struct_union.extra_info->
                                                            class_template);
          set_to_named_error_locator(locator);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
        } else if (type_symbol_type(tag_sym) == type_of_type_info) {
          /* Error -- tag-kind mismatch in type_info. */
          pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos, tag_sym);
          set_to_named_error_locator(locator);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
#if CHECKING
        } else {
          /* Mixing union and nonunion declarations is allowed in cfront
             mode.  A diagnostic will already have been issued. */
          check_assertion(any_cfront_mode());
#endif /* CHECKING */
        }  /* if */
      }  /* if */
    }  /* if */
    if (is_error_locator(locator)) err = TRUE;
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    /* Don't leave tag_position undefined. */
    tag_position = decl_start_pos;
    set_to_error_locator(locator);
    if (is_ref_within_new_expr) {
      /* We are within a new expression and no class name is given following
         the keyword -- e.g., "class A *pa = new class;" -- report the missing
         identifier as a syntax error. */
      syntax_error(ec_exp_identifier);
      err = TRUE;
    } else if (curr_token == tok_lbrace ||
               (C_dialect == C_dialect_cplusplus && curr_token == tok_colon)) {
      /* This is a tagless class definition. */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      if (scope_stack[depth_scope_stack].kind ==
                        (a_scope_kind)sck_template_declaration) {
        /* Inside a template-declaration scope -- don't issue a diagnostic
           that suggests a definition would be appropriate. */
        syntax_error(ec_exp_identifier);
      } else {
        add_stop_token(tok_lbrace);
        if (C_dialect == C_dialect_cplusplus) add_stop_token(tok_colon);
        syntax_error(ec_exp_definition_of_tag);
        if (C_dialect == C_dialect_cplusplus) remove_stop_token(tok_colon);
        remove_stop_token(tok_lbrace);
      }  /* if */
      err = TRUE;
    }  /* if */
  }  /* if */
  /* If the next token is a "{" or, in C++, a ":" (introducing a list of
     base classes) we should expect to scan a class definition.  The exception
     to this is when an elaborated class name (e.g., "struct S" instead of
     simply "S") appears within the context of a new expression.  The
     issue is the colon: since a colon could be part of the expression
     context (e.g., "struct S *ps = flag ? new struct S : 0;") it should
     not be interpreted as introducing a base classes list. */
  is_class_definition = curr_token == tok_lbrace ||
                        (C_dialect == C_dialect_cplusplus &&
                         curr_token == tok_colon && !is_ref_within_new_expr);
  if (is_class_definition && is_friend_decl) {
    /* This is an error.  Defer the diagnostic until we have a tag_sym
       to use for the fill-in.  If tag_sym is already non-NULL, we'll create
       another one. */
    set_to_named_error_locator(locator);
    tag_sym = NULL;
  }  /* if */
  if (tag_sym != NULL && C_dialect == C_dialect_cplusplus) {
    a_class_symbol_supplement_ptr	cssp;

    cssp = (tag_sym->kind == (a_symbol_kind)sk_type) ?
                        NULL : tag_sym->variant.class_struct_union.extra_info;
    class_type = type_symbol_type(tag_sym);
    if (tag_sym->kind == (a_symbol_kind)sk_type) {
      if (is_class_definition) {
        /* Attempting to redefine a template parameter name.  Let enter_symbol
           issue an error. */
        tag_sym = NULL;
      }  /* if */
    } else if (is_class_definition && tag_sym->defined) {
      /* This class has already been defined.  If this is a template
         specialization declaration (and the entity has not already
         been defined as a specialization), indicate that the entity being
         specialized has already been referenced. */
      if (is_template_specialization &&
          !class_type->variant.class_struct_union.is_specialized) {
        pos_sy_error(ec_specialization_of_referenced_entity,
                     &tag_position, tag_sym);
      } else {
        pos_sy_error(ec_already_defined, &tag_position, tag_sym);
      }  /* if */
      error_tag_sym = tag_sym;
      tag_sym = NULL;
      set_to_named_error_locator(locator);
      err = TRUE;
    } else {
      a_boolean		class_type_is_complete;
      class_type_is_complete = !is_incomplete_type(class_type);
      if (class_type->variant.class_struct_union.is_template_class) {
        /* A template class or a nested class within a template class. */
        if (is_template_specialization) {
          /* A specialization using the template<> syntax. */
          if (is_class_definition || curr_token == tok_semicolon) {
            is_template_specific_decl = TRUE;
            if (class_type->variant.class_struct_union.is_specialized) {
              /* Redeclaration. */
              *declares_something = FALSE;
            } else {
              if (tag_sym->decl_scope != ssep->number &&
                  ((!tag_sym->is_class_member &&
                    tag_sym->parent.namespace_ptr == NULL) ||
                   !namespace_is_enclosed_by_curr_scope(tag_sym))) {
                pos_sy_error(ec_bad_scope_for_specialization,
                             &tag_position, tag_sym);
                tag_sym = NULL;
                set_to_named_error_locator(locator);
                err = TRUE;
              }  /* if */
              if (class_type->variant.class_struct_union.is_nonreal_class) {
                /* A specialization of a nonreal class.  This is usually the
                   result of a specialization in an invalid scope, in which
                   case an error will have already been issued.  This can
                   also occur in Microsoft mode where specializations are
                   allowed in class scopes. */
                if (microsoft_mode) {
                  class_type->variant.class_struct_union.is_specialized = TRUE;
                  /* The specialization that is in the prototype instantiation
		     of the enclosing class should itself be treated as a
		     prototype instantiation. */
                } else {
                  tag_sym = NULL;
                  set_to_named_error_locator(locator);
                  err = TRUE;
                }  /* if */
              } else if (class_type_is_complete && !err) {
                /* The class has already been instantiated and can't now
                   be specialized. */
                pos_sy_error(ec_specialization_of_referenced_entity,
                             &tag_position, tag_sym);
              } else {
                class_type->variant.class_struct_union.is_specialized = TRUE;
                /* Set the referencing namespace to the namespace containing
                   the class.  This is needed in Microsoft mode when an
                   instantiation scope is pushed for specialized classes. */
                if (tag_sym != NULL) {
                  cssp->referencing_namespace =
                                          parent_namespace_for_symbol(tag_sym);
                }  /* if */
                if (instantiation_mode == tim_local) {
                  /* In tim_local mode generated instances have internal
                     linkage.  For specialized classes, the name linkage must
                     be reset. */
                  set_name_linkage_for_type(class_type);
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (is_class_definition && tag_sym->is_class_member &&
                   !is_explicit_instantiation &&
                   cssp->class_template != NULL) {
          /* This is a definition of a member template instance -- apparently
             an attempt at old-style specialization, but only the "template<>"
             syntax is allowed for member template specializations. */
          if (depth_innermost_namespace_scope == depth_scope_stack) {
            /* A valid scope in which "template<>" can appear. */
            pos_sy_error(ec_old_specialization_not_allowed, &tag_position,
                         tag_sym);
          } else {
            /* Also an invalid scope. */
            pos_sy_error(ec_bad_scope_for_specialization, &tag_position,
                         tag_sym);
          }  /* if */            
          tag_sym = NULL;
          set_to_named_error_locator(locator);
          err = TRUE;
        } else if (is_class_definition ||
            (curr_token == tok_semicolon &&
             !is_friend_decl && !is_explicit_instantiation &&
             (!microsoft_mode || microsoft_version < 1100))) {
          /* We have a specific declaration of a template class.  Note that
             starting with version 11.0 (Visual C++ 5.0) the Microsoft
             compiler no longer considers a declaration such as
             "class A<int>;" to declare an incomplete specialization. */
          if (tag_sym->decl_scope != ssep->number &&
              ((!tag_sym->is_class_member &&
                tag_sym->parent.namespace_ptr == NULL) ||
               !namespace_is_enclosed_by_curr_scope(tag_sym))) {
            /* Explicit specializations of class templates must appear in the
               file or namespace scope in which the template was originally
               declared or in a scope enclosing the original scope. */
            pos_sy_error(ec_bad_scope_for_specialization,
                         &tag_position, tag_sym);
            tag_sym = NULL;
            set_to_named_error_locator(locator);
            err = TRUE;
          } else if (tag_sym->is_class_member &&
                     tag_sym->decl_scope == ssep->number) {
            /* This is a vacuous declaration of a nested class of a
               class template such as:
	         template <class T> struct A {
		   struct B;
		   struct B;
                 };
               Don't consider this to be a specialization. */
          } else if (class_type_is_complete) {
            /* The class has already been instantiated -- don't consider this
               to be a specialization. */
          } else {
            check_old_specialization_allowed(tag_sym, &tag_position);
            class_type->variant.class_struct_union.is_specialized = TRUE;
            class_type->variant.class_struct_union.
                                      specialized_with_old_syntax = TRUE;
            is_template_specific_decl = TRUE;
            /* Set the referencing namespace to the namespace containing
               the class.  This is needed in Microsoft mode when an
               instantiation scope is pushed for specialized classes. */
            cssp->referencing_namespace = parent_namespace_for_symbol(tag_sym);
            if (instantiation_mode == tim_local) {
              /* In tim_local mode generated instances have internal
                 linkage.  For specialized classes, the name linkage must
                 be reset. */
              set_name_linkage_for_type(class_type);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (tag_sym != NULL) {
      if (!tag_sym->is_class_member) {
        if (is_class_definition) {
          /* This is a definition and a namespace-qualified name. */
          if (tag_sym->parent.namespace_ptr == NULL) {
            if (tag_sym->decl_scope != ssep->number) {
              /* Unless a class is a namespace member or nested in another
                 class, it cannot be defined other than it the scope to which
                 it belongs. */
              pos_sy_error(ec_bad_scope_for_definition, &tag_position,
                           tag_sym);
              tag_sym = NULL;
              set_to_error_locator(locator);
            }  /* if */
          } else {
            /* The class being defined was originally declared a namespace
               member.  Determine (1) whether it's legal in this context and
               if so, (2) whether a scope stack entry needs to be pushed. */
            a_boolean  scope_err = FALSE;
            if (namespace_scope_should_be_pushed(tag_sym, &locator,
                                                 &tag_position, &scope_err)) {
              /* Push a namespace extension scope. */
              push_namespace_extension_scope(tag_sym->parent.namespace_ptr);
              namespace_extension_pushed = TRUE;
              effective_decl_level = depth_scope_stack;
            } else if (scope_err) {
              /* An error was issued by the subroutine. */
              tag_sym = NULL;
              set_to_error_locator(locator);
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Nested class. */
        if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
            tag_sym->parent.class_type == ssep->assoc_type) {
          /* Possible redeclaration of nested class name inside the body of
             the class of which it is a member. Note that if the elaborated
             type-specifier does not introduce a definition and is not
             followed by a semicolon, then it is not a redeclaration. */
          check_nested_class_redeclaration(
             tag_sym, &tag_position, is_class_definition, is_friend_decl,
             (a_boolean)locator.is_qualified_name, declares_something);
        } else if (is_class_definition) {
          /* A definition of a nested class that appears in the scope other
             than that of its parent class. */
          parent_sym = (a_symbol_ptr)tag_sym->parent.class_type->
                                                  source_corresp.assoc_info;
          /* Find the outermost enclosing class. */
          while (parent_sym->is_class_member) {
            parent_sym = (a_symbol_ptr)parent_sym->parent.class_type->
                                                    source_corresp.assoc_info;
          }  /* while */
          if (parent_sym->decl_scope == ssep->number) {
            /* Okay to define the nested class in this scope -- it is the
               scope in which the parent was defined. */
            delayed_nested_class_def = TRUE;
          } else if (parent_sym->parent.namespace_ptr != NULL &&
                     namespace_is_enclosed_by_curr_scope(parent_sym)) {
            /* Also okay to define the nested class in this scope -- it is a
               a scope (namespace- or file-scope) enclosing the namespace
               scope in which the parent class was defined.  Handle this like
               the case where the parent class itself is defined in such an
               enclosing scope, so that we get the innermost namespace scope
               and the effective declaration level right.  Here's an example:
                 namespace NS1 {
                   namespace NS2 {
                     class A;
                     class B { class N; };
                   }
                 }
                 class NS1::NS2::A { class N; };   // Okay (handled above)
                 class NS1::NS2::A::N { };         // Okay (handled here)
                 class NS1::NS2::B::N { };         // Okay (handled here)
               Specifically, push the namespace extension scope. */
            push_namespace_extension_scope(parent_sym->parent.namespace_ptr);
            namespace_extension_pushed = TRUE;
            effective_decl_level = depth_scope_stack;
            delayed_nested_class_def = TRUE;
          } else {
            pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
            tag_sym = NULL;
            set_to_error_locator(locator);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new class, struct, or union type.  All such types are
       allocated in the file scope memory region, though local types will be
       added to the function scope's types list. */
    class_type = alloc_type(type_kind);
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      /* A type is actually declared in a function prototype scope only in
         C mode.  In C++ the type is injected into a containing scope. */
      check_assertion(err || C_dialect != C_dialect_cplusplus ||
                      is_class_definition);
      class_type->declared_in_function_prototype = TRUE;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && error_tag_sym != NULL) {
      class_type->variant.class_struct_union.extra_info->template_arg_list =
             error_tag_sym->variant.class_struct_union.type->
                      variant.class_struct_union.extra_info->template_arg_list;
    }  /* if */
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "struct {int a; int b;}"). */
    if (tag_id_present) {
      tag_sym = enter_local_symbol(tag_kind, &locator, effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      if (!friend_injection_enabled && is_friend_decl) {
        /* The name of a class first declared in a friend declaration is
           entered into the innermost non-class scope, but it's not visible
           to lookup. */
        tag_sym->is_invisible = TRUE;
      }  /* if */
    } else {
      /* Tagless class, struct, or union.  Create a symbol to represent it;
         though not entered in the symbol table, it is needed to carry
         around some information about classes that is of interest to the
         front end only. */
      tag_sym = make_unnamed_tag_symbol(tag_kind, &pos_curr_token);
      /* Although the symbol header has a name of sorts, it should not appear
         in the type, so NULL it out after the call to set_source_corresp. */
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      class_type->source_corresp.name = NULL;
      class_type->variant.class_struct_union.originally_unnamed = TRUE;
    }  /* if */
    tag_sym->variant.class_struct_union.type = class_type;
    if (C_dialect == C_dialect_cplusplus) {
      if (is_class_definition && is_friend_decl) {
        /* Issuing the diagnostic was deferred till now. */
        pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
        err = TRUE;
      }  /* if */
      /* Set parent class or namespace pointers, if appropriate. */
      switch (scope_stack[effective_decl_level].kind) {
        case sck_class_struct_union:
          /* A new class name is being declared within a class scope. */
          if (is_class_definition ||
              (vacuous_decl_allowed && curr_token == tok_semicolon)) {
            /* Either a definition or a vacuous declaration -- the latter
               introduces a name into the current scope. */
            set_class_membership(tag_sym, &class_type->source_corresp,
                                 scope_stack[decl_scope_level].assoc_type);
            class_type->source_corresp.access = ssep->current_access;
          }  /* if */
          break;
        case sck_namespace:
        case sck_namespace_extension:
          /* A class is being declared within a namespace.  This includes
             friend declarations injected into a namespace from a class
             scope. */
          set_namespace_membership(tag_sym, &class_type->source_corresp,
                                   scope_stack[effective_decl_level].
                                       il_scope->variant.assoc_namespace);
          break;
        default:;
      }  /* switch */
      /* In C classes have no linkage, as do local classes in C++; otherwise
         classes have "C++-external" name linkage.  (Note: in cfront mode
         classes may also have internal linkage -- see ARM 3.3.)  Note that
         even nameless classes may be marked as having linkage; this is
         useful for dealing with member functions.) */
      if (!is_local_class) {
        /* Nonlocal class. */
        set_name_linkage_for_type(class_type);
      }  /* if */
    }  /* if */
    if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
        tag_sym->is_class_member && !tag_sym->is_error) {
      /* Determine whether this is a referenced to a nested class within
         a class template.  If so, set the correspondence with the
         corresponding prototype class. */
      set_nested_template_class_symbol_info(tag_sym, type_kind);
    }  /* if */
    srk_flags = SRK_DECLARATION;
    if (is_class_definition) srk_flags |= SRK_DEFINITION;
    if (is_friend_decl) srk_flags |= SRK_FRIEND;
    record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                              (a_source_sequence_entry_ptr)NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Set the first_declaration flag in the associated source-sequence
       secondary declaration entry.  The corresponding field in the class
       symbol supplement will already have been set for definitions, if
       appropriate. */
    if (!is_class_definition) {
      (void)set_src_seq_secondary_decl_fields((char *)class_type,
                                              (a_type_ptr)NULL,
                                              SSSD_FIRST_DECLARATION);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    if (tag_sym->variant.type.ptr->kind == (a_type_kind)tk_template_param) {
      /* Use of template parameter name as a proxy tag name during a
         prototype instantiation. */
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  } else {
    a_class_type_supplement_ptr  ctsp;
    /* Using an existing type.  Fetch the type pointer from it. */
    class_type = tag_sym->variant.class_struct_union.type;
    ctsp = class_type->variant.class_struct_union.extra_info;
    if (!is_template_specific_decl || !(*declares_something)) {
      is_redeclaration = TRUE;
    }  /* if */
    if (!friend_injection_enabled && !is_friend_decl) {
      /* In case the previous declaration was a friend declaration, ensure
         that the symbol is henceforth visible for lookup. */
      tag_sym->is_invisible = FALSE;
    }  /* if */
    if (is_class_definition) {
      /* Allow for alternating between class and struct, but stay with the
         one associated with the definition.  The difference only affects
         default member access. */
      class_type->kind = type_kind;
      /* If this is a nested class of a class template, update the type kind
         associated with the template. */
      if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
          !is_template_specialization && !tag_sym->is_error) {
        update_nested_template_class_symbol_info(tag_sym, type_kind);
      }  /* if */
    }  /* if */
    /* Record cross-reference information. */
    if (!is_friend_decl && !locator.is_template_id &&
        is_file_or_namespace_scope(&scope_stack[depth_scope_stack]) &&
        class_type->variant.class_struct_union.is_prototype_instantiation &&
        ctsp->template_arg_list != NULL &&
        !(ctsp->assoc_scope != NULL &&
          ctsp->assoc_scope->depth_in_scope_stack != NO_SCOPE_DEPTH)) {
      /* A prototype instantiation of a class template (as opposed of that of
         a class nested in a class template).
         This can only happen in the emulation of a peculiar Microsoft bug
         that causes the following to be accepted:
           template<class T> struct S;
           struct S; // ignored (scan_tag_name returns the prototype
                     //          instantiation)
         Such "redeclarations" are ignored (i.e., not recorded).
      */
      check_assertion(microsoft_bugs);
    } else if (is_class_definition || is_predeclared_type_decl ||
               (curr_token == tok_semicolon &&
                (vacuous_decl_allowed ||
                 is_friend_decl || is_template_specific_decl))) {
      /* Vacuous declarations are not typically permitted to use qualified
         names.  Exceptions are made for friend declarations and for
         template specialization declarations. */
      srk_flags = SRK_DECLARATION;
      if (is_friend_decl) srk_flags |= SRK_FRIEND;
      if (is_class_definition) {
        if (!is_template_class_instantiation) {
          srk_flags |= SRK_DEFINITION;
        }  /* if */
      } else {
        /* A declaration of the form "class A;", when A has already been
           declared, is treated as a redeclaration (not a reference). */
      }  /* if */
      record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                                (a_source_sequence_entry_ptr)NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (is_predeclared_type_decl && !is_class_definition) {
        /* This is the first explicit declaration of a predeclared type --
           set the first_declaration flag in the associated source-sequence
           secondary declaration entry. */
        (void)set_src_seq_secondary_decl_fields((char *)class_type,
                                                (a_type_ptr)NULL,
                                                SSSD_FIRST_DECLARATION);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Not a definition, not a vacuous declaration, so presumably a
         reference. */
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  if (tag_sym->kind != (a_symbol_kind)sk_type &&
      !is_redeclaration && !is_template_specific_decl &&
      may_be_added_to_types_list(class_type, effective_decl_level)) {
    /* This is the initial declaration of this class type. */
    add_to_types_list(class_type, effective_decl_level);
  }  /* if */
  if (err ||
      (depth_template_declaration_scope != NO_SCOPE_DEPTH &&
       scope_stack[depth_scope_stack].kind ==
                                      (a_scope_kind)sck_class_struct_union)) {
    /* Pragma processing may run into invalid scopes.  So discard them. */
    discard_curr_construct_pragmas();
  } else if (is_class_definition || curr_token == tok_semicolon) {
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (!C_mode() && (microsoft_mode or_near_and_far_enabled()) &&
      tag_sym->kind != (a_symbol_kind)sk_type) {
    update_extended_decl_info_for_class(class_type, &extended_decl_info,
                                        &locator.source_position);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  if (is_class_definition) {
    if (scan_class_definition(class_type, effective_decl_level,
                              orig_decl_level, is_local_class,
                              delayed_nested_class_def,
                              /*is_template_instantiation=*/FALSE,
                              (a_template_ptr)NULL,
                              &local_decl_pos_block)) {
      *defines_something = TRUE;
    } else {
      err = TRUE;
    }  /* if */
    /* If necessary, pop the namespace extension scope. */
    if (namespace_extension_pushed) pop_namespace_extension_scope();
    /* If there are no longer any classes in the process of being defined
       do any class fixups and template instantiations that have been
       deferred.  (Note that this has to be done after the namespace
       extension scope is popped to handle source-sequence insertion for
       templates correctly.)  At this point we should not be in a template
       declaration scope, unless an earlier syntax error caused us to confuse
       the intended construct; in that case a diagnostic has been or will be
       issued elsewhere. */
    if (depth_template_declaration_scope == NO_SCOPE_DEPTH &&
        !(microsoft_bugs && is_typedef)) {
      /* In Microsoft bugs mode, the typedef is processed before member
         function bodies etc. are rescanned.  This makes e.g. the following
         legal:
            typedef struct {
              enum { e };
              void f() { S::e; }
            } S;
         Hence, in that mode the following call will be made after the call
         to decl_typedef.
      */
      process_deferred_class_fixups_and_instantiations();
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    check_for_and_remove_redundant_secondary_decl_ss_entry(class_type);
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && !C_mode() && tag_sym->kind != (a_symbol_kind)sk_type) {
    /* If the class has been defined, issue an error if the inheritance kind
       (if any) for the class is too restrictive. */
    a_class_type_supplement_ptr ctsp = class_type->variant.
                                           class_struct_union.extra_info;
    if (extended_decl_info.inheritance_kind != (an_inheritance_kind)ihk_none) {
      if (extended_decl_info.inheritance_kind != ctsp->inheritance_kind) {
        /* An error will already have been issued -- no need for another. */
      } else if (is_incomplete_type(class_type)) {
        /* The class hasn't been defined yet, so there's no way to check. */
      } else {
        /* Be sure the inheritance kind specified on the current declaration
           is not "too restrictive" for the actual characteristics of the
           class. */
        check_inheritance_kind(class_type, extended_decl_info.inheritance_kind,
                               &extended_decl_info.inheritance_kind_pos);
      }  /* if */
    } else if (is_class_definition) {
      /* There was no explicit specification of an inheritance kind on the
         class, but there may have been on a previous declaration, or the
         default inheritance kind may have been assigned.  In either case,
         now that we have a definition, determine whether the preestablished
         inheritance kind is appropriate. */
      if (ctsp->inheritance_kind != (an_inheritance_kind)ihk_none) {
        check_inheritance_kind(class_type, ctsp->inheritance_kind,
                               &locator.source_position);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Copy the end specifiers end position into decl_pos_block.  There are
       potentially two declarations here -- e.g.,
         const struct S { ... } *ps;
       where both "S" and "ps" are declared (and where *decl_pos_block
       belongs to the declaration of "ps" and local_decl_pos_block belongs
       to the declaration of "S").  Note that the specifiers ranges start at 
       different positions but end at the same position. */
    decl_pos_block->specifiers_range.end =
                             local_decl_pos_block.specifiers_range.end;
  }  /* if */
  if (is_class_definition ||
      (!is_redeclaration && tag_sym->kind != (a_symbol_kind)sk_type)) {
    /* If this is the initial or defining declaration of the class, update
       the extra source information for the class type. */
    a_decl_position_supplement_ptr  dpsp = class_type->
                                               source_corresp.decl_pos_info;
    if (dpsp != NULL) {
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
#if DEBUG
      if (delayed_nested_class_def) {
        if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
          fprintf(f_debug, "decl-pos info for delayed nested class def\n");
          db_decl_pos_info(tag_sym);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_class_definition && *declares_something &&
      tag_sym->kind != (a_symbol_kind)sk_type) {
    /* Update source range information in the secondary-decl entry. */
    a_source_sequence_entry_ptr     class_ssep;
    a_src_seq_secondary_decl_ptr    sssdp;
    a_decl_position_supplement_ptr  dpsp;

    class_ssep = last_matching_source_sequence_entry((char *)class_type);
    if (class_ssep != NULL &&
        ss_entry_kind(class_ssep) == iek_src_seq_secondary_decl) {
      sssdp = (a_src_seq_secondary_decl_ptr)class_ssep->entity.ptr;
      if (sssdp->decl_pos_info == NULL) {
        dpsp = alloc_decl_position_supplement(in_file_scope(sssdp));
        dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
        if (is_friend_decl) {
          /* If this is a friend declaration, adjust the specifiers range to
             include "friend". */
          dpsp->specifiers_range.start =
                                   decl_pos_block->specifiers_range.start;
        }  /* if */
        if (tag_id_present) {
          dpsp->identifier_range = local_decl_pos_block.identifier_range;
        }  /* if */
        sssdp->decl_pos_info = dpsp;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (err) {
    *type_ptr = error_type();
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    *type_ptr = tag_sym->variant.type.ptr;
  } else {
    *type_ptr = class_type;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(tag_sym, "tag_sym: ", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return !err;
}  /* class_specifier */


static an_integer_kind
		largest_enum_int_kind;
			/* The largest integer kind that enum type can
			   have.  Ordinarily ik_int in C mode, but in C++
			   mode the integer kind corresponding to the
			   largest integer type supported. */

#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static void enum_specifier(a_boolean         vacuous_decl_allowed,
                           a_type_ptr        *type_ptr,
                           a_boolean         *declares_something,
                           a_boolean         *defines_something,
                           a_decl_pos_block  *decl_pos_block)
/*
Scan an enumeration specifier (3.5.2.2).  The syntax is

3.5.2.2
       enum-specifier:
		enum identifier    { enumerator-list }
                               opt
		enum identifier

       enumerator-list:
		enumerator
		enumerator-list , enumerator

       enumerator:
		enumeration-constant
		enumeration-constant = constant-expression

An enumeration-constant is an identifier.

The type is returned in *type_ptr.  *declares_something is set to indicate
whether or not this specifier declares something, and *defines_something
to indicate whether an enumeration is actually defined.
*/
{
  a_symbol_locator         locator;
  a_symbol_ptr             tag_sym;
  a_boolean                tag_id_present;
  a_type_ptr               enum_type;
  a_type_ptr               enum_con_type;
  a_symbol_ptr             enum_sym;
  a_constant               constant;
  a_boolean                err, did_not_fold, template_param;
  a_constant_ptr           enum_con;
  a_constant_ptr           end_of_enum_con_list;
  a_constant               max_value, min_value;
  a_boolean                done, min_max_set;
  a_source_position        pos_comma;
  a_memory_region_number   region_to_switch_back_to;
  a_type_ptr               class_of_which_a_member;
  an_access_specifier      access;
  a_scope_depth            effective_decl_level = decl_scope_level;
  a_boolean                inside_class_definition;
  a_boolean                is_redeclaration;
  a_boolean                namespace_extension_pushed = FALSE;
  a_source_position        tag_position;
  a_decl_pos_block         local_decl_pos_block;
  a_boolean                is_predeclared_type_decl = FALSE;

  db_enter(3, "enum_specifier");

  *declares_something = FALSE;
  *defines_something = FALSE;
  if (scope_stack[decl_scope_level].kind ==
                                     (a_scope_kind)sck_class_struct_union) {
    class_of_which_a_member = scope_stack[decl_scope_level].assoc_type;
    access = scope_stack[decl_scope_level].current_access;
    inside_class_definition = TRUE;
  } else {
    class_of_which_a_member = NULL;
    access = (an_access_specifier)as_public;
    inside_class_definition = FALSE;
  }  /* if */
  clear_decl_pos_block(&local_decl_pos_block);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  local_decl_pos_block.specifiers_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Skip over "enum". */
  check_assertion(curr_token == tok_enum);
  (void)get_token();
  /* If there is an identifier next, it is a tag.  It can be the declaration
     of a new tag or a reference to an existing tag. */
  tag_id_present = is_expr_qualified_name_start();
  if (tag_id_present) {
    a_boolean   tag_resolution;
    a_boolean   is_friend_decl = FALSE;
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    *declares_something = TRUE;
    tag_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    local_decl_pos_block.identifier_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    tag_sym = scan_tag_name((a_symbol_kind)sk_enum_tag, &locator,
                            &is_friend_decl, &vacuous_decl_allowed,
                            /*is_ref_within_new_expr=*/FALSE,
                            &effective_decl_level, &tag_resolution,
                            &is_predeclared_type_decl, &local_decl_pos_block);
    if (tag_resolution) {                            
      /* Resolution of a previous incomplete declaration. */
      if (effective_decl_level != decl_scope_level) {
        class_of_which_a_member = NULL;
        access = (an_access_specifier)as_public;
      }  /* if */
    } else if (tag_sym != NULL && curr_token == tok_lbrace) {
      /* This is a definition of an enumeration that has previously been
         declared. */
      if (tag_sym->is_class_member) {
        if (tag_sym->parent.class_type != class_of_which_a_member) {
          /* This is an attempt to define a member enum outside the class of
             which it is a member. */
          pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
          tag_sym = NULL;
          set_to_error_locator(locator);
        }  /* if */
      } else if (tag_sym->parent.namespace_ptr != NULL) {
        err = FALSE;
        if (namespace_scope_should_be_pushed(tag_sym, &locator, &tag_position,
                                             &err)) {
          /* Push a namespace extension scope. */
          push_namespace_extension_scope(tag_sym->parent.namespace_ptr);
          namespace_extension_pushed = TRUE;
          effective_decl_level = depth_scope_stack;
        } else if (err) {
          /* An error was issued by the subroutine. */
          tag_sym = NULL;
          set_to_error_locator(locator);
        }  /* if */
      }  /* if */
    } else if (is_error_locator(locator) && curr_token != tok_lbrace) {
      /* There was an error is looking up the tag, and this is not a
         definition.  For error recovery, return an error type. */
      *type_ptr = error_type();
      goto return_point;
    } else if (tag_sym == NULL && class_of_which_a_member &&
               effective_decl_level != decl_scope_level) {
      /* This is a (non-standard) forward declaration of a nonclass that
         appears inside a class definition. */
      class_of_which_a_member = NULL;
      access = (an_access_specifier)as_public;
    }  /* if */
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    set_to_error_locator(locator);
    tag_position = pos_curr_token;
    if (curr_token == tok_lbrace) {
      /* This is a tagless class definition. */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      add_stop_token(tok_lbrace);
      syntax_error(ec_exp_definition_of_tag);
      remove_stop_token(tok_lbrace);
      /* This statement might have declared something, but since we're
         scanning past the relevant tokens we'll never know.  Set the flag
         to TRUE anyway, to avoid other errors down the line. */
      *declares_something = TRUE;
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new enumerated type.  All enumeration type entries are
       allocated in the file scope memory region. */
    enum_type = alloc_type((a_type_kind)tk_integer);
    is_redeclaration = FALSE;
    /* set_type_size is called later, once the final type is known. */
    /* Set a default representation of "int", which may be adjusted later. */
    enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    enum_type->variant.integer.enum_type = TRUE;
    enum_type->variant.integer.enum_info.constant_list = NULL;
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      enum_type->declared_in_function_prototype = TRUE;
    }  /* if */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "enum {a, b, c}"). */
    if (tag_id_present) {
      tag_sym = enter_local_symbol((a_symbol_kind)sk_enum_tag, &locator,
                                   effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      *declares_something = TRUE;
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      tag_sym->variant.enumeration.type = enum_type;
    } else {
      /* Unnamed enum.  Create a symbol to represent it. */
      tag_sym = make_unnamed_tag_symbol((a_symbol_kind)sk_enum_tag,
                                        &pos_curr_token);
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      enum_type->source_corresp.name = NULL;
      tag_sym->variant.enumeration.type = enum_type;
      /* set_source_corresp and mark_defined are not called, so clear the
         reference flag and copy in the decl position manually. */
      enum_type->source_corresp.referenced = FALSE;
      enum_type->source_corresp.decl_position = locator.source_position;
    }  /* if */
    if (!C_mode()) {
      if (class_of_which_a_member != NULL) {
        /* Add a pointer to the parent class in the symbol and the type. */
        set_class_membership(tag_sym, &enum_type->source_corresp,
                             class_of_which_a_member);
      } else if (scope_stack[effective_decl_level].kind ==
                                     (a_scope_kind)sck_namespace ||
                 scope_stack[effective_decl_level].kind ==
                                     (a_scope_kind)sck_namespace_extension) {
        /* Set the parent namespace. */
        set_namespace_membership(tag_sym, &enum_type->source_corresp,
                                 scope_stack[effective_decl_level].
                                         il_scope->variant.assoc_namespace);
      }  /* if */
      if (depth_innermost_function_scope == NO_SCOPE_NUMBER &&
          !inside_local_class) {
        /* Enum declaration is not local to a function. */
        set_name_linkage_for_type(enum_type);
      }  /* if */
    }  /* if */
    /* When an enumeration is defined within a class definition, its access
       should be set based on the access recorded in the current scope stack
       entry. */
    enum_type->source_corresp.access = access;
    if (tag_id_present) {
      /* Note that mark_defined and mark_referenced are called after the
         namespace/class membership has been specified. */
      if (curr_token == tok_lbrace) {
        mark_defined(tag_sym, &locator.source_position);
      } else {
        mark_declared(tag_sym, &locator.source_position);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Set the first_declaration flag in the associated source-sequence
           secondary declaration entry. */
        (void)set_src_seq_secondary_decl_fields((char *)enum_type,
                                                (a_type_ptr)NULL,
                                                SSSD_FIRST_DECLARATION);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (curr_token == tok_lbrace) {
        /* An unnamed enum type.  mark_defined can't be called to put out a
           source sequence entry for it, but we need one anyway, so call
           the subroutine directly. */
        if (!source_sequence_entries_disallowed) {
          f_update_source_sequence_list((char *)enum_type,
                                        (an_il_entry_kind)iek_type,
                                        (a_source_sequence_entry_ptr)NULL);
        }  /* if */
      }  /* if */
#endif  /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      /* In Microsoft compatibility mode enum types can be declared without
         being defined and can also be used.  The use requires that the size
         be set. */
      check_assertion(!enum_types_can_be_smaller_than_int);
      set_type_size(enum_type);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
  } else {
    /* Using an existing type.  Fetch the enumerated type pointer from it. */
    enum_type = type_symbol_type(tag_sym);
    is_redeclaration = TRUE;
    /* Record cross-reference information. */
    if (curr_token == tok_lbrace) {
      mark_defined(tag_sym, &locator.source_position);
      if (!C_mode() && inside_class_definition) {
        /* enum_type is a class member and is being defined having been
           forward-declared. */
        check_assertion(tag_sym->is_class_member == TRUE);
        if (enum_type->source_corresp.access != access) {
          /* The access specified for the previous declaration does not
             correspond to the access for current declaration. */
          pos_sy_diagnostic(strict_ansi_mode ?
                              strict_ansi_discretionary_severity :
                              es_warning,
                            ec_redecl_changes_access,
                            &locator.source_position, tag_sym);
           /* Since this is a definition, use the current access instead of
             that specified on the original declaration. */
          enum_type->source_corresp.access = access;
        }  /* if */
      }  /* if */
    } else if (curr_token == tok_semicolon && !strict_ansi_mode) {
      /* A useless redeclaration of an enum tag. */
      mark_declared(tag_sym, &locator.source_position);
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* We associate a curr-construct pragma with this enum type only if this
       is a definition.  Otherwise this is assumed to be part of a declaration
       of something else -- to which the pragma should be bound. */
    if (tag_sym != NULL) {
      /* Do processing required for any pragmas that are bound to the current
         declaration. */
      process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
    } else {
      /* Issue diagnostics on pragmas that are trying to bind to an unnamed
         enum. */
      cannot_bind_to_curr_construct();
    }  /* if */
    /* Scan the enumeration itself.  Since the enumeration type entry is
       allocated in the file scope memory region, all its components should
       also be.  Switch to the file scope memory region here at the start of
       the definition and switch back when we reach the right brace. */
    *defines_something = TRUE;
    (void)get_token();
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ the type of an enumerator is the same as that of its
         enumeration, but that won't actually be known until the definition
         is complete.  Set the types in the enum constants later. */
      enum_con_type = NULL;
    } else {
      /* In C the type of the constants is always "int", regardless of
         the type of the enumerated type (see 3.5.2.2).  However, it is
         tagged with the enumerated type, so that enum compatibility checking
         can be done later. */
      check_assertion(!enum_types_can_be_larger_than_int);
      enum_con_type = alloc_type((a_type_kind)tk_integer);
      enum_con_type->variant.integer.int_kind = (an_integer_kind)ik_int;
      enum_con_type->variant.integer.enum_type = FALSE;
      enum_con_type->variant.integer.enum_info.affiliated_type = enum_type;
      set_type_size(enum_con_type);
    }  /* if */
    min_max_set = FALSE;
    if (curr_token == tok_rbrace &&
        (C_dialect == C_dialect_cplusplus
#if MICROSOFT_EXTENSIONS_ALLOWED
                        || microsoft_mode
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                         )) {
      /* An enumerator constant list is optional in C++ and Microsoft C. */
    } else {
      add_stop_token(tok_rbrace);
      end_of_enum_con_list = NULL;
      /* Scan the list of enumerated constants. */
      do {
        a_source_sequence_entry_ptr  enum_con_ssep = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        a_source_range               enum_id_range, enum_value_range;

        enum_id_range = null_source_range;
        enum_value_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        add_stop_token(tok_comma);
        add_stop_token(tok_assign);
        if (curr_token != tok_identifier) {
          (void)required_token(tok_identifier, ec_exp_identifier);
          set_to_error_locator(locator);
        } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
          if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
            fprintf(f_debug, "enum_specifier: empty ss entry for \"%s\":\n",
                    locator_for_curr_id.symbol_header->identifier);
          }  /* if */
#endif /* DEBUG */
          enum_con_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          locator = locator_for_curr_id;
#if EXTRA_SOURCE_POSITIONS_IN_IL
          enum_id_range.start = pos_curr_token;
          enum_id_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          /* Advance past the identifier. */
          (void)get_token();
          /* Set the error position to the identifier position. */
          copy_source_position(locator.source_position, error_position);
        }  /* if */
        /* Note that the enumerator symbol is entered at little later, after
           the constant expression (if any) has been scanned.  (C standard,
           3.1.2.1 and 3.5.2.2) */
        remove_stop_token(tok_assign);
        err = FALSE;
        template_param = FALSE;
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
        /* Forget about expressions scanned in previous constants. */
        constant.expr = NULL;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
        /* See if "= constant-expression" follows. */
        if (curr_token == tok_assign) {
          (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
          enum_value_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          /* Scan the constant expression. */
          scan_fs_integral_constant_expression(&constant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
          enum_value_range.end = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          if (is_error_constant(&constant)) {
            err = TRUE;
          } else if (constant.kind ==
                                (a_constant_repr_kind)ck_template_param) {
            /* We are doing a prototype instantiation and we have a case like
               this:
                 template <int N> class A { enum e { e1 = N }; };
            */
            template_param = TRUE;
          } else if (enum_types_can_be_larger_than_int) {
            /* No need to check, since the largest integer kind will be
               used if needed. */
          } else {
            check_assertion(constant.kind == (a_constant_repr_kind)ck_integer);
            /* Check the value to see if it is out of range.  (3.5.2.2,
               constraints) */
            if (!in_range_for_integer_kind(&constant, &constant,
                                           largest_enum_int_kind)) {
              a_boolean		conversion_allowed = TRUE;
              if (strict_ansi_mode) {
                conversion_allowed = strict_ansi_error_severity != es_error;
              }  /* if */
              if (conversion_allowed &&
                  f_skip_typerefs(constant.type)->size <= targ_sizeof_int) {
                /* In non-strict mode, allow unsigned constants that can be
                   coerced into an int. */
                type_change_constant(&constant,
                                     integer_type((an_integer_kind)ik_int),
                                     /*is_implicit_cast=*/TRUE,
                                     /*constant_context=*/TRUE,
                                     /*evaluated_context=*/TRUE,
                                     /*fold_constant_addr_exprs=*/TRUE,
                                     /*is_reinterpret_cast=*/FALSE,
                                     /*maintain_expression=*/TRUE,
                                     &did_not_fold,
                                     &error_position);
                if (strict_ansi_mode) {
                  warning(ec_enum_value_out_of_int_range);
                }  /* if */
              } else {
                error(ec_enum_value_out_of_int_range);
                set_error_constant(&constant);
              }  /* if */
            }  /* if */
          }  /* if */
        } else {
          /* No explicit value. */
          if (end_of_enum_con_list == NULL) {
            /* This is the first enumerator.  Start with zero. */
            set_integer_constant(&constant, (a_host_large_integer)0,
                                 (an_integer_kind)ik_int);
          } else if (is_error_constant(&constant)) {
            /* There was a previous error. */
            err = TRUE;
          } else {
            /* Use a value one larger than the previous value. */
            /* Check the value to see if it is out of range.  (3.5.2.2,
               constraints) */
            if (is_max_value_for_integer_kind(&constant,
                                              largest_enum_int_kind)) {
              error(ec_enum_value_out_of_int_range);
              err = TRUE;
            } else {
              /* If incrementing the current value requires a larger
                 integer, just use the integer corresponding to
                 largest_enum_int_kind.  It's not specified by the standard
                 what larger integer to use, and it doesn't seem to make
                 much difference. */
              if (is_max_value_for_integer_kind(
                                  &constant,
                                  constant.type->variant.integer.int_kind)) {
                constant.type = integer_type(largest_enum_int_kind);
              }  /* if */
              incr_integer_value(&constant.variant.integer_value);
            }  /* if */
          }  /* if */
        }  /* if */
        /* Enter the enumeration constant identifier. */
        enum_sym = enter_local_symbol((a_symbol_kind)sk_constant, &locator,
                                      decl_scope_level,
                                      /*suppress_redecl_error=*/FALSE);
        *declares_something = TRUE;
        /* Track the highest and lowest values in the enumeration.  These are
           used to determine the appropriate representation type. */
        if (err) {
          /* There was some kind of error in the value for the enumerator. */
          set_error_constant(&constant);
        } else if (template_param) {
          /* The expression has a template parameter value, so it has
             no effect on the size of the enumeration. */
        } else if (!min_max_set) {
          max_value = constant;
          min_value = constant;
          min_max_set = TRUE;
        } else if (cmp_integer_constants(&constant, &max_value) > 0) {
          max_value = constant;
        } else if (cmp_integer_constants(&constant, &min_value) < 0) {
          min_value = constant;
        }  /* if */
        /* Assign the value to the enumeration constant. */
        switch_to_file_scope_region(&region_to_switch_back_to);
        enum_con = alloc_unshared_constant(&constant);
        /* Switch back from the file scope memory region to whatever region
           was current upon entry. */
        switch_back_to_original_region(region_to_switch_back_to);
        set_source_corresp(&(enum_con->source_corresp), enum_sym);
        enum_sym->variant.constant = enum_con;
        if (C_mode()) {
          enum_con->type = enum_con_type;
        } else {
          /* In C++ mode leave the type of the constant unchanged for now.
             The enumerator constants will get the type of the enumeration,
             but not until after all the constants have been scanned.  (This
             affects cases in which an enum constant expression involves a
             previously declared enum constant from the same enumeration.) */
          /* In C++ specify membership and access. */
          if (class_of_which_a_member != NULL) {
            /* Set the parent class. */
            set_class_membership(enum_sym, &enum_con->source_corresp,
                                 class_of_which_a_member);
          } else if (tag_sym->parent.namespace_ptr != NULL) {
            /* Set the parent namespace. */
            set_namespace_membership(enum_sym, &enum_con->source_corresp,
                                     tag_sym->parent.namespace_ptr);
          }  /* if */
          enum_con->source_corresp.access = access;
        }  /* if */
        record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, enum_sym,
                                  &locator.source_position, enum_con_ssep);
        /* Add the enumeration constant to the list under the enumerated
           type. */
        if (end_of_enum_con_list == NULL) {
          enum_type->variant.integer.enum_info.constant_list = enum_con;
        } else {
          end_of_enum_con_list->next = enum_con;
        }  /* if */
        end_of_enum_con_list = enum_con;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        { a_decl_position_supplement_ptr  dpsp;
          dpsp = enum_con->source_corresp.decl_pos_info;
          if (dpsp != NULL) {
            dpsp->identifier_range = enum_id_range;
            dpsp->variant.enum_value_range = enum_value_range;
          }  /* if */
        }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* Keep looping while there are more enumeration constant
           identifiers. */
        copy_source_position(pos_curr_token, pos_comma);
        done = !loop_token(tok_comma);
        if (!done && curr_token == tok_rbrace) {
           /* In K&R and C99 C modes an extra comma is allowed at the end of
              the list.  In other C and C++ modes, we allow it as an
              extension, with a strict ANSI diagnostic (the gcc compiler
              source includes cases like this, and that source is part of
              the SPEC benchmark suite). */
          done = TRUE;
          if (C_dialect != C_dialect_pcc && !c99_mode) {
            an_error_severity    severity;
            severity = strict_ansi_mode ? strict_ansi_discretionary_severity :
                                          es_remark;
            pos_diagnostic(severity, ec_nonstd_extra_comma, &pos_comma);
          }  /* if */
        }  /* if */
        remove_stop_token(tok_comma);
      } while (!done);
      remove_stop_token(tok_rbrace);
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    local_decl_pos_block.specifiers_range.end = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for and pass over the closing "}". */
    (void)required_token(tok_rbrace, ec_exp_rbrace);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the enum definition. */
    add_end_of_construct_source_sequence_entry((char *)enum_type,
                                               (a_byte_il_entry_kind)iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Determine the representation type for the enumeration.  When
       enum_types_can_be_smaller_than_int is FALSE (e.g., in pcc mode and
       Microsoft mode), it's always "int", and that's already set.
       Otherwise, pick the first of "char", "signed char", "unsigned char",
       "short", "unsigned short", and "int" into which the enumeration
       values will fit.  Only if enum_types_can_be_larger_than_int (e.g.,
       in strict C++ mode), is there any point in trying "unsigned int" and
       larger integer types. */
#if CHECKING
    if (C_dialect == C_dialect_pcc) {
      check_assertion(!enum_types_can_be_smaller_than_int &&
                      !enum_types_can_be_larger_than_int);
    }  /* if */
#endif /* CHECKING */
    if (enum_types_can_be_smaller_than_int) {
      if (!min_max_set || in_range_for_integer_kind(&min_value, &max_value,
                                                    plain_char_int_kind)) {
        /* "Plain" char. */
        enum_type->variant.integer.int_kind = plain_char_int_kind;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_signed_char)) {
        /* Signed char. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_signed_char;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                          (an_integer_kind)ik_unsigned_char)) {
        /* Unsigned char. */
        enum_type->variant.integer.int_kind =
                                             (an_integer_kind)ik_unsigned_char;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_short)) {
        /* Short. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_short;
      } else if ((targ_sizeof_short < targ_sizeof_int) &&
                  in_range_for_integer_kind(&min_value, &max_value,
                                         (an_integer_kind)ik_unsigned_short)) {
        /* Unsigned short.  Note that we can only get here if
           sizeof(short) < sizeof(int) on the target, for otherwise the
           previous test (for "short") is testing the same range as "int"
           (into which all enumeration values must fall), so "short" would
           have been selected.  This is important, as we would not want
           to pick "unsigned short" if the integral promotions would
           promote it to "unsigned int" rather than "int". */
        enum_type->variant.integer.int_kind =
                                            (an_integer_kind)ik_unsigned_short;
      } else {
        /* Use the default representation type, which is already set. */
      }  /* if */
    }  /* if */
    /* If the underlying integer type of enums can be larger than "int" (as
       is standard in C++) and if the type has not already been adjusted to
       be smaller than int, keep checking. */
    if (min_max_set && enum_types_can_be_larger_than_int &&
        enum_type->variant.integer.int_kind == (an_integer_kind)ik_int) {
      if (in_range_for_integer_kind(&min_value, &max_value,
                                    (an_integer_kind)ik_int)) {
        /* Int. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                          (an_integer_kind)ik_unsigned_int)) {
        /* Unsigned int. */
        enum_type->variant.integer.int_kind =
                                             (an_integer_kind)ik_unsigned_int;

      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_long)) {
        /* Long. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_long;
#if LONG_LONG_ALLOWED
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                          (an_integer_kind)ik_unsigned_long)) {
        /* Unsigned long. */
        enum_type->variant.integer.int_kind =
                                          (an_integer_kind)ik_unsigned_long;
      } else if (in_range_for_integer_kind(&min_value, &max_value,
                                           (an_integer_kind)ik_long_long)) {
        /* Long long. */
        enum_type->variant.integer.int_kind = (an_integer_kind)ik_long_long;
#endif /* LONG_LONG_ALLOWED */
      } else {
        /* Representation should be largest_enum_int_kind. */
        enum_type->variant.integer.int_kind = largest_enum_int_kind;
      }  /* if */
    }  /* if */
    /* Set the type size (based on the integral type it is mapped onto). */
    set_type_size(enum_type);
    if (!C_mode()) {
      /* In C++ now that we know the type of the enumeration, we can update
         each constant to share the same type. */
      for (enum_con = enum_type->variant.integer.enum_info.constant_list;
           enum_con != NULL;
           enum_con = enum_con->next) {
        enum_con->type = enum_type;
      }  /* for */
    }  /* if */
    /* If entities dependent on this enum type were declared before it was
       defined, they will have been recorded on a fixup list.  Go through
       the fixup list and complete the declarations. */
    check_dependent_type_fixup_list(tag_sym);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Copy the end specifiers end position into decl_pos_block. */
    decl_pos_block->specifiers_range.end =
                             local_decl_pos_block.specifiers_range.end;
  }  /* if */
  if (*defines_something || !is_redeclaration) {
    /* This is either the definition of the enumeration or its initial
       declaration.  Update the extra source position information in the
       type entry. */
    a_decl_position_supplement_ptr  dpsp = enum_type->
                                               source_corresp.decl_pos_info;
    if (dpsp != NULL) {
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (*declares_something && !(*defines_something)) {
    /* Update source range information in the secondary-decl entry. */
    a_source_sequence_entry_ptr     ssep;
    a_src_seq_secondary_decl_ptr    sssdp;
    a_decl_position_supplement_ptr  dpsp;

    /* Ordinarily secondary declarations of enum types (e.g., forward
       declarations) are not allowed, but they are sometimes allowed as an
       extension. */
    check_assertion(!strict_ansi_mode || !is_redeclaration);
    /* Look for the secondary-decl entry. */
    ssep = last_matching_source_sequence_entry((char *)enum_type);
    if (ssep != NULL &&
        ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
      check_assertion(sssdp->decl_pos_info == NULL);
      /* Allocate the supplement, set its fields, and link it to the
         secondary-decl entry that was just located for enum_type. */
      dpsp = alloc_decl_position_supplement(in_file_scope(sssdp));
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
      sssdp->decl_pos_info = dpsp;
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Add the type to the types list for the current scope.  This is done
     after the closing brace, if any, to get the IL types list in the right
     order.  Incomplete enums are added to the types list even though the
     actual definition has not yet appeared; however, it will be reentered
     on the list if and when the definition appears. */
  if (may_be_added_to_types_list(enum_type, effective_decl_level)) {
    if (!is_redeclaration) {
      /* This is the initial declaration of this enum type. */
      add_to_types_list(enum_type, effective_decl_level);
    } else if (*defines_something) {
      /* This is a redeclaration and also a definition.  Remove the enum type
         from the types list and reenter it at the end. */
      move_to_end_of_types_list(enum_type, effective_decl_level);
    }  /* if */
  }  /* if */
  /* If necessary, pop the namespace extension scope. */
  if (namespace_extension_pushed) pop_namespace_extension_scope();
  *type_ptr = enum_type;
return_point:;
  db_exit();
}  /* enum_specifier */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
void typename_specifier(a_type_ptr            *type_ptr,
                        a_boolean             within_using_decl,
                        a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a typename specifier.  Typename is an elaborated type specifier.
The syntax is

	typename ::    nested-name-specifier identifier
                   opt

The identifier that follows the typename keyword must be a type name,
otherwise a diagnostic is issued.  The type is returned in *type_ptr.
On return, the current token is the one following the final identifier
above.  within_using_decl is TRUE in a class member using declaration that
starts with "using typename".  decl_pos_block is a possibly NULL pointer to
a block of source position information when the context is a declaration.
*/
{
  a_type_ptr	tp = NULL;

  /* Skip over "typename". */
  check_assertion(curr_token == tok_typename);
  /* The typename keyword may only be used within a template, including the
     template parameter list. */
  if (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
      depth_template_declaration_scope == NO_SCOPE_DEPTH) {
    diagnostic(es_discretionary_error, ec_typename_not_in_template);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Assume the current token is the last decl-specifier. */
    decl_pos_block->specifiers_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)get_token();
  if (!is_generalized_identifier_start(GID_IS_TYPENAME)) {
    syntax_error(ec_exp_identifier);
  } else {
    a_boolean	               err = FALSE;
    an_identifier_lookup_mode  ilm;

    ilm = within_using_decl ? ilm_using_typename : ilm_typename;
    if (!coalesce_and_lookup_qualified_name(GID_NO_OPTIONS, ilm, &err) ||
        !locator_for_curr_id.is_qualified_name || 
        locator_for_curr_id.is_file_scope_qualified_name || err) {
      /* The identifier scanned is not a class-qualified name,
         namespace-qualified name (file-scope qualified names such as ::x are
         disallowed by the syntax), or is a qualified name that refers to a
         nonexistent member. */
      if (!err) {
        error(ec_qualified_name_required);
      }  /* if */
    } else {
      a_symbol_ptr	sym = locator_for_curr_id.specific_symbol;
      a_symbol_ptr	fund_sym;
      check_assertion(sym != NULL);
      check_ambiguity_and_verify_access(&locator_for_curr_id);
      fund_sym = fundamental_symbol_of(sym);
      if (!is_type_symbol(fund_sym)) {
        /* The symbol is not a type name. */
        sym_error(ec_sym_not_a_type_name, sym);
      } else {
        mark_referenced(fund_sym, &locator_for_curr_id.source_position);
        tp = type_symbol_type(fund_sym);
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      /* Assume the current token is the last decl-specifier. */
      decl_pos_block->specifiers_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Bypass the identifier token. */
    if (!within_using_decl) (void)get_token();
  }  /* if */
  /* If no type was created, an error must have occurred above.  Return
     an error type. */
  if (tp == NULL) tp = error_type();
  *type_ptr = tp;
}  /* typename_specifier */


a_boolean is_constructor_decl(a_type_ptr    class_type)
/*
class_type is a pointer to the class that is currently being defined.  Return
TRUE and modify locator_for_curr_id appropriately if the current declaration
is a that of a constructor.
*/
{
  a_boolean          is_constructor = FALSE;
  a_symbol_ptr       tag_sym, sym;
  a_symbol_ptr       ctor_type_sym;
  a_token_cache      cache;
  a_boolean          cache_in_use = FALSE;
  a_source_position  pos;
  a_boolean          name_match = FALSE;
  a_boolean          type_mismatch = FALSE;

  db_enter(4, "is_constructor_decl");
  if (microsoft_mode &&
      (((curr_token == tok_struct || curr_token == tok_class) &&
        (class_type->kind == (a_type_kind)tk_struct ||
         class_type->kind == (a_type_kind)tk_class)) ||
       (curr_token == tok_union &&
        class_type->kind == (a_type_kind)tk_union))) {
    /* In Microsoft mode it is possible to use an elaborated type name to
       declare a constructor.  E.g. "struct S { struct S(); };". */
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_in_use = TRUE;
    /* Put the current token in the cache. */
    cache_curr_token(&cache);
    (void)get_token();
  }  /* if */
  /* See whether the name of the current identifier token is the same as
     that of a class being defined.  If so, this declaration is treated
     as a constructor declaration if the next two tokens are a left paren
     and declaration start token.  Use token caching in the look-ahead,
     since the tokens will have to be rescanned no matter what. */
  tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  ctor_type_sym = locator_for_curr_id.specific_symbol;
  if (locator_for_curr_id.symbol_header == tag_sym->header) {
    name_match = TRUE;
    if (ctor_type_sym != NULL) {
      if (ctor_type_sym == tag_sym) {
        /* The type specified matches the class type symbol. */
      } else if (ctor_type_sym->kind == (a_symbol_kind)sk_type &&
                 ctor_type_sym->variant.type.is_injected_class_name &&
                 ctor_type_sym->variant.type.ptr == class_type) {
        /* The type specified is the injected class symbol.  This is okay. */
      } else {
        /* The names match, but the types don't.  This happens in templates
           when the class name is "A" but the constructor was specified as
           A<T>, and A<T> does not refer to the prototype instantiation.
           This can also occur during a real instantiation if the constructor
           was specified as A<int> and we are instantiating A<char>.
           If there is a mismatch, make a note of it now, but don't issue a
           diagnostic until the balance of the "is constructor" tests have
           been done. */
        type_mismatch = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (name_match || microsoft_mode) {
    /* Change "A::A" into "A" if we are processing inside the definition of
       class "A".  This is necessary for curr_token_type_symbol to handle
       this case correctly. */
    (void)simplify_curr_class_qualified_name();
    if (!locator_for_curr_id.is_qualified_name &&
        !locator_for_curr_id.is_conversion_name &&
        !locator_for_curr_id.is_operator_name) {
      if (!cache_in_use) {
        clear_token_cache(&cache, /*reusable=*/FALSE);
        cache_in_use = TRUE;
      }  /* if */
      /* Put the current token in the cache. */
      cache_curr_token(&cache);
      /* Skip right parentheses that may enclose the declarator---e.g.,
         "struct S { (((S)))(); };"---and advance to what may be a left
         parenthesis: */
      while (get_token() == tok_rparen) { cache_curr_token(&cache); }
      /* A left parenthesis presumably starts a parameter declaration list: */
      if (curr_token == tok_lparen) {
        /* Cache the left parenthesis. */
        cache_curr_token(&cache);
        /* Advance past it.  If the next token is a right paren or
           the start of a parameter declaration, this must be a
           constructor. */
        (void)get_token();
        if (curr_token == tok_rparen || curr_token == tok_ellipsis ||
            is_decl_start(/*expr_context=*/FALSE,
                          /*real_declarator_allowed=*/TRUE)) {
          /* Constructor. */
          is_constructor = TRUE;
        }  /* if */
      }  /* if */
      /* Note that rescan_cached_tokens caches the current token as well as
         resetting the current token state to what it was before token caching
         was started.  So the current token should again be the name of the
         class being defined. */
      rescan_cached_tokens(&cache);
      cache_in_use = FALSE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (!name_match && is_constructor) {
      /* We must be in Microsoft mode.  MSVC++ allows a typedef name that
         refers to the current class to replace the class name in a
         constructor declaration.  The syntax looks like a constructor
         declaration, so do a lookup to see if it's a typedef name for the
         current class. */
      sym = normal_id_lookup(&locator_for_curr_id,
                             IDL_TENTATIVE_TYPE_LOOKUP |
                             IDL_DO_NOT_ADD_TO_NONREAL_CLASS |
                             IDL_DO_NOT_CREATE_PROJ_SYM);
      if (sym != NULL && sym->kind == (a_symbol_kind)sk_type &&
          skip_typerefs(sym->variant.type.ptr) == class_type &&
          !sym->ambiguous) {
        /* Note that qualifiers on the typedef name are ignored -- this
           corresponds to MSVC++ behavior. */
        /* name_match = TRUE; */
      } else {
        is_constructor = FALSE;
      }  /* if */
      clear_specific_symbol(locator_for_curr_id);
    }  /* if */
#endif /* if MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_constructor) {
      /* Turn the current locator from a "specific symbol" locator into a
         constructor locator. */
      clear_specific_symbol(locator_for_curr_id);
      locator_for_curr_id.specific_symbol = NULL;
      (void)class_qualified_id_lookup(&locator_for_curr_id, class_type,
                                      IDL_DIRECT_CLASS_MEMBERS_ONLY);
      sym = locator_for_curr_id.specific_symbol;
      if (sym != tag_sym) {
        /* The symbol one gets by looking up the class name is not the same as
           the class symbol.  This might be okay, but it has to be checked
           carefully. */
        if (sym != NULL) {
          if (is_constructor_symbol(sym)) {
            /* Okay. */
          } else if (sym->kind == (a_symbol_kind)sk_type &&
                     f_skip_typerefs(sym->variant.type.ptr) == class_type) {
            /* There is a typedef for the class type with the same name as
               the class.  It was found instead of the class on the lookup.
               That's okay. */
          } else if (sym->kind != (a_symbol_kind)sk_projection ||
                     sym->variant.projection.is_using_decl) {
            /* This can only mean that another member has been
               declared with the class name.  Issue an error. */
            str_error(ec_id_already_declared,
                        locator_for_curr_id.symbol_header->identifier);
          }  /* if */
        }  /* if */
        /* Use the class symbol instead of whatever the lookup returned. */
        locator_for_curr_id.specific_symbol = tag_sym;
      }  /* if */
      pos = locator_for_curr_id.source_position;
      change_class_locator_into_constructor_locator(&locator_for_curr_id,
                                                    &pos);
    }  /* if */
  }  /* if */
  if (cache_in_use) {
    /* If we haven't done so yet, reset the token stream. */
    rescan_cached_tokens(&cache);
  }  /* if */
  if (is_constructor && type_mismatch) {
    /* The type used to declare the constructor does not match the type
       of the current class. */
    pos_ty_error(ec_constructor_type_mismatch,
                 &locator_for_curr_id.source_position, class_type);
  }  /* if */
  db_exit();
  return is_constructor;
}  /* is_constructor_decl */


/* Enumerations used by decl_specifiers for its internal processing and
   for calls to its subroutines. */
/* The basic type (without qualifiers or other specifiers). */
typedef enum {
  bt_none,
  bt_void,
  bt_char,
  bt_wchar_t,
  bt_bool,
  bt_int,
  bt_float,
  bt_double,
  bt_typedef,
  bt_struct_union,
  bt_enum,
  bt_typename,
  bt_no_type,
  bt_error
} a_basic_type;
/* The sign specifier. */
typedef enum {
  sign_none,
  sign_signed,
  sign_unsigned
} a_type_sign;
/* The size specifier. */
typedef enum {
  size_none,
  size_short,
  size_long
#if LONG_LONG_ALLOWED
  , size_long_long
#endif /* LONG_LONG_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  , size_int8,
  size_int16,
  size_int32,
  size_int64
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_type_size;
/* C99 floating-point modifiers. */
typedef enum {
  cxa_none,
  cxa_complex,
  cxa_imaginary
} a_complex_attribute;


static a_boolean combine_type_specifiers(a_type_ptr           *type_ptr,
                                         a_basic_type         basic_type,
                                         a_type_sign          sign,
                                         a_type_size          size,
                                         a_complex_attribute  complex_attr)
/*
Given a basic type, a sign specifier, and a size specifier, return a
pointer to a type entry in *type_ptr.  This routine is only called from
decl_specifiers.
*/
{
  an_integer_kind  ikind;
  a_float_kind     fkind;
  a_boolean        bad_combination = FALSE;

  if (C_dialect == C_dialect_pcc && basic_type == bt_typedef &&
      (sign != sign_none || size != size_none)) {
    /* pcc allows use of unsigned, long, and short as adjectives modifying
       a typedef type.  Turn the typedef into a matching basic type,
       for the cases for which it makes sense.  For the others, an error
       will be detected below. */
    a_type_ptr  temp_type = skip_typerefs(*type_ptr);
    if (temp_type->kind == (a_type_kind)tk_integer) {
      if (temp_type->variant.integer.enum_type) {
        /* Don't allow adjectives on enum integers. */
      } else {
        /* Adjectives (size and sign) are only allowed where they
           fill in empty holes -- unspecified attributes -- in the
           following table:

                                      sign      size      base type
             ik_signed_char                    -fixed-     char
             ik_unsigned_char       see note   -fixed-     char
             ik_short                          short       int
             ik_unsigned_short      unsigned   short       int
             ik_int                                        int
             ik_unsigned_int        unsigned               int
             ik_long                           long        int
             ik_unsigned_long       unsigned   long        int
             ik_long_long                      long long   int
             ik_unsigned_long_long  unsigned   long long   int

           In pcc mode, the "signed" keyword does not exist, so something
           that is signed really has unspecified sign.  Note that ik_char
           is not used in pcc mode, so ik_unsigned_char has to be viewed
           as not specifying a sign if it is plain_char_int_kind.
           ik_signed_char always implies an unspecified sign.  A size may
           not be specified for those ("short char" and "long char" don't
           make sense). */
        ikind = temp_type->variant.integer.int_kind;
        switch (ikind) {
          case ik_unsigned_char:
            if (plain_char_int_kind != ikind && sign != sign_none) break;
            /* Fall into signed char case. */
          case ik_signed_char:
            if (size != size_none
#if MICROSOFT_EXTENSIONS_ALLOWED
                && size != size_int8
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                    ) break;
            basic_type = bt_char;
            break;
          case ik_short:
            if (size != size_none) break;
            basic_type = bt_int;
            size = size_short;
            break;
          case ik_unsigned_short:
            /* No holes to fill in. */
            break;
          case ik_unsigned_int:
            if (sign != sign_none) break;
            sign = sign_unsigned;
            /* Fall into signed int case. */
          case ik_int:
            basic_type = bt_int;
            break;
          case ik_long:
            if (size != size_none) break;
            basic_type = bt_int;
            size = size_long;
            break;
          case ik_unsigned_long:
            /* No holes to fill in. */
            break;
#if LONG_LONG_ALLOWED
          case ik_long_long:
            if (size == size_none) {
              basic_type = bt_int;
              size = size_long_long;
            }  /* if */
            break;
          case ik_unsigned_long_long:
            /* No holes to fill in. */
            break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
          default:
            internal_error("combine_type_specifiers: bad typedef int kind");
#endif /* CHECKING */
        }  /* switch */
      }  /* if */
    } else if (temp_type->kind == (a_type_kind)tk_float) {
      fkind = temp_type->variant.float_kind;
      if (fkind == (a_float_kind)fk_float) {
        basic_type = bt_float;
      } else if (fkind == (a_float_kind)fk_double) {
        basic_type = bt_double;
      }  /* if */
    }  /* if */
    if (basic_type != bt_typedef) *type_ptr = NULL;
  }  /* if */
  /* Now check for the various legal combinations of specifiers.  See 3.5.2
     for list. */
  switch (basic_type) {
    case bt_void:
      if (sign == sign_none && size == size_none) {
        /* void type. */
        *type_ptr = void_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_char:
      if (size != size_none
#if MICROSOFT_EXTENSIONS_ALLOWED
          && size != size_int8
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                              ) {
        bad_combination = TRUE;
      } else {
        switch (sign) {
          case sign_none:
            /* "plain" char. */
            ikind = plain_char_int_kind;
            break;
          case sign_signed:
            /* signed char. */
            ikind = (an_integer_kind)ik_signed_char;
            break;
          case sign_unsigned:
            /* unsigned char. */
            ikind = (an_integer_kind)ik_unsigned_char;
            break;
#if CHECKING
          default:
            internal_error(
                      "combine_type_specifiers: bad value for a_type_sign");
#endif /* CHECKING */
        }
        /* In Microsoft Visual C++ 6.0 __int8 is a distinct type (not just a
           synonym for a char type). */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && microsoft_version >= 1200 &&
            size == size_int8) {
          if (ikind == (an_integer_kind)ik_signed_char) {
            /* signed __int8 is the same as __int8. */
            ikind = (an_integer_kind)ik_char;
          }  /* if */
          *type_ptr = microsoft_sized_integer_type((an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          *type_ptr = integer_type((an_integer_kind)ikind);
        }  /* if */
      }  /* if */
      break;
    case bt_wchar_t:
      if (sign == sign_none && size == size_none) {
        *type_ptr = wchar_t_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_bool:
      if (sign == sign_none && size == size_none) {
        *type_ptr = bool_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_none:
      /* If there was no explicit basic type, assume an integer type. */
    case bt_int:
      switch (size) {
        case size_short:
          if (sign != sign_unsigned) {
            /* short, signed short, short int, signed short int. */
            ikind = (an_integer_kind)ik_short;
          } else {
            /* unsigned short, unsigned short int. */
            ikind = (an_integer_kind)ik_unsigned_short;
          }  /* if */
          break;
        case size_none:
          if (sign != sign_unsigned) {
            /* int, signed, signed int, or no type specifiers. */
            ikind = (an_integer_kind)ik_int;
          } else {
            /* unsigned, unsigned int. */
            ikind = (an_integer_kind)ik_unsigned_int;
          }  /* if */
          break;
        case size_long:
          if (sign != sign_unsigned) {
            /* long, signed long, long int, signed long int. */
            ikind = (an_integer_kind)ik_long;
          } else {
            /* unsigned long, unsigned long int. */
            ikind = (an_integer_kind)ik_unsigned_long;
          }  /* if */
          break;
#if LONG_LONG_ALLOWED
        case size_long_long:
          if (sign != sign_unsigned) {
            /* long long, signed long long, long long int,
               signed long long int. */
            ikind = (an_integer_kind)ik_long_long;
          } else {
            /* unsigned long long, unsigned long long int. */
            ikind = (an_integer_kind)ik_unsigned_long_long;
          }  /* if */
          break;
#endif /* LONG_LONG_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
        case size_int16:
          if (sign != sign_unsigned) {
            /* __int16, signed __int16. */
            ikind = targ_int16_int_kind;
          } else {
            /* unsigned __int16. */
            ikind = targ_unsigned_int16_int_kind;
          }  /* if */
          break;
        case size_int32:
          if (sign != sign_unsigned) {
            /* __int32, signed __int32. */
            ikind = targ_int32_int_kind;
          } else {
            /* unsigned __int32. */
            ikind = targ_unsigned_int32_int_kind;
          }  /* if */
          break;
        case size_int64:
          if (sign != sign_unsigned) {
            /* __int64, signed __int64. */
            ikind = targ_int64_int_kind;
          } else {
            /* unsigned __int64. */
            ikind = targ_unsigned_int64_int_kind;
          }  /* if */
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if CHECKING
        default:
          internal_error("combine_type_specifiers: bad size for int");
#endif /* CHECKING */
      }  /* switch */
        /* In Microsoft Visual C++ 6.0 __intN is a distinct type (not just a
           synonym for another integral type). */
      if (sign == sign_signed) {
        /* For an explicitly "signed" int, use a different type entry.
           Plain "int" and "signed int" have to be kept separate because
           they may mean different things as bit-field types.  The same
           applies to explicitly signed short, long, and long long. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && microsoft_version >= 1200 &&
            (int)size >= (int)size_int8 && (int)size <= (int)size_int64) {
          *type_ptr = microsoft_sized_signed_integer_type(
                                                      (an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          *type_ptr = signed_integer_type((an_integer_kind)ikind);
        }  /* if */
      } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && microsoft_version >= 1200 &&
            (int)size >= (int)size_int8 && (int)size <= (int)size_int64) {
          *type_ptr = microsoft_sized_integer_type((an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          *type_ptr = integer_type((an_integer_kind)ikind);
        }  /* if */
      }  /* if */
      break;
    case bt_float:
    case bt_double:
      if (sign != sign_none || (size != size_none && size != size_long)) {
        bad_combination = TRUE;
      } else {
        if (size == size_none) {
          if (basic_type == bt_float) {
            /* float. */
            fkind = (a_float_kind)fk_float;
          } else {
            fkind = (a_float_kind)fk_double;
          }  /* if */
        } else {
          if (basic_type == bt_float) {
            /* long float, which is double in pcc.  Allowed as an extension
               in ANSI mode. */
            fkind = (a_float_kind)fk_double;
            if (strict_ansi_mode) {
              diagnostic(strict_ansi_error_severity,
                         ec_bad_combination_of_type_specifiers);
            }  /* if */
          } else {
            /* long double. */
            fkind = (a_float_kind)fk_long_double;
          }  /* if */
        }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
        if (complex_attr == cxa_complex) {
          *type_ptr = complex_type((a_float_kind)fkind);
        } else if (complex_attr == cxa_imaginary) {
          *type_ptr = imaginary_type((a_float_kind)fkind);
        } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        /* Do not insert code here. */
        {
          *type_ptr = float_type((a_float_kind)fkind);
        }  /* if */
      }  /* if */
      break;
    case bt_struct_union:
    case bt_enum:
    case bt_typename:
    case bt_typedef:
      if (sign != sign_none || size != size_none) bad_combination = TRUE;
      check_assertion_str2(*type_ptr != NULL,
                           "combine_type_specifiers: null type ptr for",
                           "class, struct, union, enum, or typedef");
      break;
    case bt_no_type:
      /* No specifiers type declared (constructor, destructor, or conversion
         operator). */
      *type_ptr = unknown_type();
      break;
    case bt_error:
      /* Error, already diagnosed. */
      *type_ptr = error_type();
      break;
#if CHECKING
    default:
      internal_error("combine_type_specifiers: bad basic type");
#endif /* CHECKING */
  }  /* switch */
  if (bad_combination) {
    /* Bad combination of type specifiers.  Issue a diagnostic and set the
       type to an error type. */
    error(ec_bad_combination_of_type_specifiers);
    *type_ptr = error_type();
  }  /* if */
  /* Return TRUE if no problems were encountered in combining type
     specifiers. */
  return !bad_combination;
}  /* combine_type_specifiers */


static a_boolean add_type_qualifiers(a_type_ptr            *type_ptr,
                                     a_type_qualifier_set  *qualifiers,
                                     a_source_position     *qualifier_pos,
                                     a_source_position     *restrict_pos)
/*
Add the type qualifiers specified by *qualifiers to the type specified by
*type_ptr.  *qualifier_pos is the source position of the first of the type
qualifiers (if any), not counting restrict.  *restrict_pos is the source
position of the restrict keyword (if it's there).  This function is called
from decl_specifiers only.
*/
{
  a_boolean          err = FALSE;
  an_error_severity  severity;

  if (*qualifiers != TQ_NONE) {
    if ((*type_ptr)->kind == (a_type_kind)tk_typeref) {
      if (C_dialect == C_dialect_cplusplus) {
        /* In C++ adding a qualifier to a typedef name that is already
           identically qualified is okay, so don't even bother checking for
           an error.  Note that make_qualified_type will not actually add
           superfluous qualifiers. */
        /* However, adding a qualifier to a typedef for a reference type
           is not allowed.  More precisely, the qualifier is ignored.
           Issue a diagnostic. */
        if (is_reference_type(*type_ptr)) {
          /* "restrict" may be applied to reference types, but the other
              qualifiers may not. */
          if ((*qualifiers & ~TQ_RESTRICT) != TQ_NONE) {
            *qualifiers &= TQ_RESTRICT;
            pos_warning(ec_useless_type_qualifiers, qualifier_pos);
          }  /* if */
        }  /* if */
      } else {
        /* In C we check for duplicate qualifiers on a declaration, even
           if, in the case of an array type, one is a top-level qualifier
           and the other qualifies an element type.  That's why top_level
           is set to FALSE here -- that's normally not the case in C mode. */
        /* According to 3.5.3: "If the specification of an array type
           includes any type qualifiers, the element type is so-qualified,
           not the array type.", and this is interpreted recursively
           for arrays of arrays.  The type qualifiers therefore apply
           to the ultimate element type.  This can only happen with typedefs,
           as in "typedef int A[2][3]; const A a;", which makes "a" an
           array of array of const int. */
        if ((*qualifiers &
             f_get_type_qualifiers(*type_ptr, /*top_level=*/FALSE)) != 0) {
          /* Duplication of type qualifier (probably because of a typedef
             that is already qualified).  In strict ANSI mode issue an
             error or warning; otherwise, just issue a remark. */
          if (strict_ansi_mode) {
            severity = strict_ansi_error_severity;
            if (severity == es_error) err = TRUE;
          } else {
            severity = es_remark;
          }  /* if */
          diagnostic(severity, ec_dupl_type_qualifier);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!C_mode() && *qualifiers != TQ_NONE) {
      /* Type qualifiers are not allowed on function types. */
      if (is_function_type(*type_ptr) ||
          (is_array_type(*type_ptr) &&
           is_function_type(underlying_array_element_type(*type_ptr)))) {
        /* Put out a remark instead of an error in Microsoft and cfront
           compatibility modes -- but don't add the qualifiers. */
        if (microsoft_mode || any_cfront_mode()) {
          severity = es_remark;
        } else {
          severity = es_discretionary_error;
          /* Note that err is not set for this discretionary error.  That's
             because it might actually end up as a warning.  Besides, since
             the qualifiers are ignored, there are no side-effects in the
             IL, etc. */
        }  /* if */
        pos_diagnostic(severity, ec_cv_qualified_function_type, qualifier_pos);
        *qualifiers = TQ_NONE;
      }  /* if */
    }  /* if */
    /* The restrict qualifier may only be applied to pointer and reference
       types (but not pointer-to-function-type), pointer-to-member types,
       and (in parameter declarations only) array types. */
    if ((*qualifiers & TQ_RESTRICT) &&
        !restrict_qualifier_is_allowed(*type_ptr, restrict_pos)) {
      /* Diagnostic has already been issued.  Just remove TQ_RESTRICT
         from the qualifier set. */
      *qualifiers &= ~TQ_RESTRICT;
      err = TRUE;
    }  /* if */
    if (*qualifiers != TQ_NONE) {
      if (is_unknown_type(*type_ptr)) {
        *type_ptr = integer_type((an_integer_kind)ik_int);
      }  /* if */
      /* Add the qualifiers if necessary.  make_qualified_type understands
         the strange array case too. */
      *type_ptr = make_qualified_type(*type_ptr, *qualifiers);
    }  /* if */
  }  /* if */
  return !err;
}  /* add_type_qualifiers */


static a_boolean implicit_int_member_with_name_of_type(void)
/*
Helper called from decl_specifiers to determine if the current identifier
might have meant to be a declarator in Cfront or Microsoft mode.  Both those
modes accept:
   struct X; struct Y { X(); };
and take Y::X to be an ordinary member function returning int.
Note that Microsoft will not accept such function declarations if they take
any parameters.  Cfront will, but that behavior is not imitated here.
*/
{
  a_boolean    result;
  a_symbol_ptr sym;
  a_token_kind token_after_next;

  check_assertion(curr_token == tok_identifier);
  sym = locator_for_curr_id.symbol_header->symbol;
  if (sym != NULL && is_type_symbol(sym)) {
    (void)next_two_tokens(tok_lparen, &token_after_next);
    result = (token_after_next == tok_rparen);
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* implicit_int_member_with_name_of_type */


/* Define a bit vector to be used within decl_specifiers to track which
   specifiers have been encountered. */
typedef long a_decl_specifiers_set;
#define DS_NONE (a_decl_specifiers_set)(0x0)
			/* No decl-specifiers have been scanned. */
#define DS_STORAGE_CLASS (a_decl_specifiers_set)(0x1)
			/* A storage class has been scanned. */
#define DS_TYPE_QUALIFIER (a_decl_specifiers_set)(0x2)
			/* A type qualifier (including "restrict" and the
			   Microsoft type qualifiers "near" and "far") has
			   been scanned. */
#define DS_TYPE (a_decl_specifiers_set)(0x4)
			/* A basic type or a size or "signed" or "unsigned"
			   has been scanned. */
#define DS_FRIEND (a_decl_specifiers_set)(0x8)
			/* "friend" has been scanned (C++ only). */
#define DS_VIRTUAL (a_decl_specifiers_set)(0x10)
			/* "virtual" has been scanned (C++ only). */
#define DS_EXPLICIT (a_decl_specifiers_set)(0x20)
			/* "explicit" has been scanned (C++ only). */
#define DS_INLINE (a_decl_specifiers_set)(0x40)
			/* "inline" has been scanned (C++ only). */
#define DS_MUTABLE  (a_decl_specifiers_set)(0x80)
			/* "mutable" has been scanned (C++ only). */
#define DS_LINKAGE_SPEC (a_decl_specifiers_set)(0x100)
			/* A linkage specification (e.g., extern "C", has
			   been scanned.  This can occur only in Microsoft
			   C++ mode. */
#define DS_DECLSPEC (a_decl_specifiers_set)(0x200)
			/* "__declspec(...)" has been scanned (Microsoft mode
			   only). */
#define DS_MICROSOFT_INLINE (a_decl_specifiers_set)(0x400)
			/* "__inline" has been scanned (Microsoft mode
			   only). */
#define DS_FORCEINLINE (a_decl_specifiers_set)(0x800)
			/* "__forceinline" has been scanned (Microsoft mode
			   only). */
#define DS_OVERLOAD (a_decl_specifiers_set)(0x1000)
			/* "overload" has been scanned (a C++ anachronism). */
#define DS_VOID (a_decl_specifiers_set)(0x2000)
			/* "void" was scanned as the very first specifier. */


static void report_bad_type_name(a_decl_flag_set  input_flags)
/*
locator_for_curr_id describes a source name that was expected to name a valid
type, but it does not.  Report different errors depending on whether the name
can be found at all (in which case it presumably does not name a type).
The parameter input_flags is the same value that was passed to decl_specifiers
(from where this routine is called).
*/
{
  if (!is_error_locator(locator_for_curr_id)) {
    a_boolean                  lookup_error;
    a_symbol_ptr               sym = NULL;
    an_identifier_options_set  options;

    options = (input_flags & DSI_IS_NEW_TYPE_NAME) ?
                                        GID_IS_NEW_TYPE_NAME : GID_NO_OPTIONS;
    check_assertion(is_generalized_identifier_start(options));
    sym = coalesce_and_lookup_generalized_identifier(
                                          options, ilm_normal, &lookup_error);
    if (!lookup_error) {
      /* No error message has been issued yet. */
      if (sym != NULL) {
        /* The name refers to something, but not a type. */
        sym_error(ec_sym_not_a_type_name, sym);
      } else {
        str_error(ec_undefined_identifier,
                  locator_for_curr_id.symbol_header->identifier);
      }  /* if */
    }  /* if */
    reference_to_invalid_name(&locator_for_curr_id);
    clear_specific_symbol(locator_for_curr_id);
  }  /* if */
}  /* report_bad_type_name */


static a_type_ptr  enclosing_class_type(a_decl_flag_set  input_flags)
/*
Called from decl_specifiers to determine the type of the class for which a
member is being scanned.  See decl_specifier for the meaning of input_flags.
Returns NULL in case of error.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_type_ptr               result = NULL;

  if (ssep->kind == (a_scope_kind)sck_template_instantiation ||
      (input_flags & DSI_IS_TEMPLATE_DECLARATION)) {
    /* Either this is a member template declaration or a rescan of a member
       template declaration to instantiate it. */
    --ssep;
  }  /* if */
  if (ssep->kind != (a_scope_kind)sck_class_struct_union &&
      ssep->kind != (a_scope_kind)sck_class_reactivation) {
    /* Error case of some sort. */
  } else {
    result = ssep->assoc_type;
    check_assertion(result != NULL && is_class_struct_union_type(result));
  }  /* if */
  return result;
}  /* enclosing_class_type */


a_boolean decl_specifiers(a_decl_flag_set            input_flags,
                          a_decl_flag_set            *output_flags,
                          a_storage_class            *storage_class,
                          a_type_ptr                 *type_ptr,
                          a_type_qualifier_set       *qualifiers,
                          a_decl_modifiers_block_ptr decl_modifiers,
                          a_decl_pos_block_ptr       decl_pos_block)
/*
Scan a list of declaration specifiers.  Specifically, scan a
declaration-specifiers (3.5), a specifier_qualifier_list (3.5.2.1), or
a type_qualifier_list (3.5.4).  These are all made up of storage class
specifiers (3.5.1; allowed only if the input_flags bit
DSI_STORAGE_CLASS_SPECIFIER_ALLOWED is set), type specifiers (3.5.2;
allowed only if DSI_TYPE_SPECIFIER_ALLOWED is set), and type
qualifiers (3.5.3; always allowed).  The list must always include at
least one specifier.  The ANSI C syntax is as follows:

3.5    declaration-specifiers:
		storage-class-specifier declaration-specifiers
                                                              opt
		type-specifier declaration-specifiers
                                                     opt
		type-qualifier declaration-specifiers
                                                     opt
3.5.2.1
       specifier-qualifier-list:
		type-specifier specifier-qualifier-list
                                                       opt
		type-qualifier specifier-qualifier-list
						       opt
3.5.4  type-qualifier-list:
		type-qualifier
		type-qualifier-list type-qualifier
3.5.1  storage-class-specifier:
		typedef
		extern
		static
		auto
		register
3.5.2  type-specifier:
		void
		char
		short
		int
		long
		float
		double
		signed
		unsigned
		struct-or-union-specifier
		enum-specifier
		typedef-name
3.5.3  type-qualifier:
		const
		volatile

When Microsoft keywords are recognized, the syntax is amended as follows
(see comments below regarding recognition of the modified syntax):

        storage-class-specifier:
		__declspec ( extended-decl-modifier-seq )
		__inline
                __forceinline

	extended-decl-modifier-seq:
		extended-decl_modifier
		                      opt
		extended-decl-modifier-seq extended-decl-modifier

	extended-decl_modifier:
		thread
		naked
		dllimport
		dllexport

The DSI_IS_PARAMETER bit of input_flags is set if these specifiers are
part of the declaration of a parameter, and, for C++, the
DSI_VIRTUAL_OR_FRIEND_ALLOWED bit is set when a declaration appears within
a class declaration, to permit recognition of "virtual" and "friend"
keywords.

The syntax for the Microsoft extensions does not exactly match the syntax
described in the Microsoft documentation.  It does, however, match the
observed behavior of the Microsoft compiler.  The additional type
qualifiers are only recognized when MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
The additional storage class specifiers are recognized anywhere that
storage classes are normally allowed.

Returns *storage_class set to the storage class scanned (or
sc_unspecified if none was scanned), *type_ptr pointing to the type
scanned (including qualifiers, if any), and any of various flags in
*output_flags: DSO_HAS_EXPLICIT_TYPE_SPECIFIER is set if there was at
least one type specifier; DSO_DECLARES_SOMETHING is set if the
specifiers declare something (a tag or enumeration members);
DSO_JUST_VOID is set if the specifiers are simply the one keyword
"void"; and DSO_CONST_QUALIFIED and DSO_VOLATILE_QUALIFIED are set
if the associated qualifiers appear directly in the qualifiers list
(these flags are useful when this routine is called to scan only type
qualifiers, since in that case no type is built).  For C++ specifically,
DSO_VIRTUAL, DSO_INLINE, and DSO_FRIEND are set to report that a
"virtual", "inline", or "friend" keyword was scanned.  It also returns
a name linkage specifier to signal when, for instance, ``extern "C"''
was encountered (C++ only).  If DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER is
true, then DSO_DANGLING_TYPE_SPECIFIER may be set for cases that are
recognized as an omitted semi-colon or comma after a class or enum
definition (e.g., "typedef int T; struct A { ... } T x;").

Returns TRUE if there is an error in the specifiers.
*/
{
  a_symbol_ptr               curr_token_type_symbol;
  a_boolean                  err = FALSE;
  a_boolean                  bad_combination_of_type_specifiers = FALSE;
  a_source_position          start_pos;
  a_source_position          non_restrict_qualifier_pos;
  a_boolean                  is_parameter = (input_flags & DSI_IS_PARAMETER);
  a_boolean                  is_member_decl =
                                    (input_flags & DSI_IS_MEMBER_DECLARATION);
  a_boolean                  vacuous_decl_allowed;
  a_boolean                  declares_something = FALSE;
  a_boolean                  defines_something = FALSE;
  a_boolean                  type_specifier_allowed;
  a_boolean                  dangling_type_specifier = FALSE;
  a_boolean                  is_elaborated_type_specifier = FALSE;
  an_error_severity          es;
  a_basic_type               basic_type = bt_none;
  a_type_sign                sign = sign_none;
  a_type_size                size = size_none;
  a_complex_attribute        complex_attr = cxa_none;
  a_source_position          restrict_pos;
  a_source_position          storage_class_pos;
  a_boolean                  bad_type_name_error;
  a_decl_specifiers_set      decl_specifiers_seen;
  a_boolean                  any_decl_specifiers_seen = FALSE;

  db_enter(3, "decl_specifiers");
  *output_flags = DSO_NO_OUTPUT_FLAGS;
  *storage_class = (a_storage_class)sc_unspecified;
  *type_ptr = NULL;
  *qualifiers = TQ_NONE;
  clear_decl_modifiers_block(decl_modifiers);
  decl_specifiers_seen = DS_NONE;
  type_specifier_allowed = (input_flags & DSI_TYPE_SPECIFIER_ALLOWED);
  vacuous_decl_allowed = (input_flags & DSI_VACUOUS_TAG_DECL_ALLOWED) != 0;
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, start_pos);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Assume the current source position is the starting position of the
       decl-specifiers.   If it turns out there are no decl-specifiers, the
       field will be reset to null_source_position. */
    decl_pos_block->specifiers_range.start = start_pos;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Loop for each declaration specifier. */
  for (;;) {
    switch (curr_token) {
      case tok_extern:
        if (C_dialect == C_dialect_cplusplus &&
            next_token() == tok_string_literal) {
          /* This is a C++ linkage specification, which is usually recognized
             and ignored in this context -- except for the error that's put
             out. */
          if (microsoft_mode && (decl_specifiers_seen == DS_DECLSPEC) &&
              !(input_flags & DSI_IS_LINKAGE_SPEC_DECL) &&
              (is_member_decl ||
               depth_scope_stack == depth_innermost_namespace_scope)) {
            /* In Microsoft C++ compatibility mode, accept __declspec(...)
               before a linkage specification.  No other decl-modifiers are
               allowed to precede a linkage specification. */
            a_name_linkage_kind  kind;

            /* The Microsoft compiler appears simply to ignore the
               decl-modifiers that precede the linkage specifier:
                 __declspec(dllexport) extern "C" void f();
                 extern "C" void f();      // MSVC++ issues no error
               Therefore, we throw away any decl-modifiers that were
               accumulated to this point. */
            clear_decl_modifiers_block(decl_modifiers);
            warning(ec_decl_modifiers_ignored);
            /* Advance to the string token. */
            (void)get_token();
            if (scan_name_linkage_string(&kind)) { 
              push_name_linkage(kind);
              *output_flags |= DSO_LINKAGE_SPEC_DECL;
            }  /* if */
          } else {
            error(ec_linkage_specifier_not_allowed);
            err = TRUE;
            /* Consume "extern".  We don't bother validating the string
               since an error has already been issued. */
            (void)get_token();
          }  /* if */
          decl_specifiers_seen |= DS_LINKAGE_SPEC;
          break;
        }  /* if */
        /* Otherwise drop through for normal storage class processing. */
      case tok_typedef:
      case tok_static:
      case tok_auto:
      case tok_register:
      case tok_mutable:
        /* A storage class specifier (3.5.1). */
        if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED)) {
          error((!C_mode() && curr_token == tok_typedef) ?
                    ec_typedef_not_allowed : ec_storage_class_not_allowed);
          err = TRUE;
#if ASM_FUNCTION_ALLOWED
        } else if (*storage_class == (a_storage_class)sc_asm) {
          error(ec_storage_class_not_allowed);
          err = TRUE;
#endif /* ASM_FUNCTION_ALLOWED */
        } else if (decl_specifiers_seen & (DS_MUTABLE | DS_STORAGE_CLASS)) {
          /* More than  one storage class may not be specified. */
          error(ec_mult_storage_classes);
          err = TRUE;
        } else if (is_parameter && curr_token != tok_register &&
                   (C_dialect != C_dialect_cplusplus ||
                    curr_token != tok_auto)) {
          /* For parameters, the only allowed storage class specifiers are
             "register" and (in C++ only) "auto". */
          if (curr_token == tok_typedef) {
            if (input_flags & DSI_IS_OLD_STYLE_PARAM_DECL) {
              /* Error will be handled by caller. */
              *storage_class = (a_storage_class)sc_typedef;
              decl_specifiers_seen |= DS_STORAGE_CLASS;
            } else {
              error(ec_typedef_not_allowed);
              err = TRUE;
            }  /* if */
          } else {
            /* "static" and "extern" aren't allowed on parameter declarations,
               but Microsoft compilers ignore them with a warning. */
            diagnostic(microsoft_mode ? es_warning : es_error,
                       ec_bad_param_storage_class);
            err = !microsoft_mode;
          }  /* if */
        } else if (!C_mode() && (decl_specifiers_seen & DS_INLINE) &&
                   curr_token != tok_static &&
                   (!extern_inline_allowed || curr_token != tok_extern)) {
          /* In C++, "inline static" is allowed; if extern_inline_allowed
             is TRUE, so is "inline extern"; otherwise, we issue an error.
             (In C99 mode "inline" can appear with both "static" and
             "extern".) */
          error(ec_bad_storage_class_with_inline);
          err = TRUE;
        } else if (curr_token == tok_mutable) {
          if (!is_member_decl || (decl_specifiers_seen & DS_FRIEND)) {
            error(ec_mutable_not_allowed);
            err = TRUE;
          } else {
            /* "mutable" is outside the "Embedded C++" subset. */
            feature_is_not_part_of_embedded_cplusplus_subset(
                                             &pos_curr_token,
                                             ec_mutable_in_embedded_cplusplus);
            /* Aside from interactions with storage classes, errors cannot
               be issued on mutable until the declarator has been scanned.
               Just return a flag to the caller. */
            decl_specifiers_seen |= DS_MUTABLE;
            *output_flags |= DSO_MUTABLE;
            storage_class_pos = pos_curr_token;
          }  /* if */
        } else if ((decl_specifiers_seen & DS_FRIEND) && !microsoft_mode) {
          /* Note: in Microsoft-compatibility mode a friend function can
             be declared "static" or "extern".  The check is done later. */
          error(ec_storage_class_in_friend_decl);
          err = TRUE;
        } else if ((input_flags & DSI_IS_SPECIALIZATION) &&
                   curr_token != tok_static) {
          error(curr_token == tok_typedef ?
                    ec_typedef_not_allowed : ec_storage_class_not_allowed);
          err = TRUE;
        } else if (is_member_decl && !microsoft_mode &&
                   !(decl_specifiers_seen & DS_FRIEND) &&
                   curr_token != tok_static && curr_token != tok_typedef) {
          error(ec_bad_member_storage_class);
          err = TRUE;
        } else if (input_flags & DSI_IS_LINKAGE_SPEC_DECL &&
                   (curr_token != tok_typedef && !microsoft_mode)) {
          /* Except in Microsoft mode, we disallow
               extern "C" static void f();
             but in order to support association between a name linkage and a
             function type we do allow
               extern "C" typedef void FT();
          */
          error(ec_storage_class_not_allowed);
          err = TRUE;
        } else if (input_flags & DSI_IS_TEMPLATE_DECLARATION &&
                   curr_token != tok_extern && curr_token != tok_static) {
          error(curr_token == tok_typedef ?
                   ec_typedef_not_allowed :
                   ec_bad_storage_class_on_template_decl);
          err = TRUE;
        } else if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
                   (curr_token == tok_auto || curr_token == tok_register)) {
          error(ec_bad_file_scope_storage_class);
          err = TRUE;
        } else if (input_flags & DSI_IS_CONDITION_DECL) {
          /* Issue a diagnostic for the specification of a storage class on
             a condition declaration.  Ignore auto and register except in
             strict mode.  Only set err if an error is issued.  Do not
             set *storage_class. */
          if (curr_token == tok_auto || curr_token == tok_register) {
            es = strict_ansi_mode ? strict_ansi_error_severity : es_none;
          } else {
            es = es_error;
          }  /* if */
          /* Put out the diagnostic. */
          if ((int)es != (int)es_none) {
            diagnostic(es, curr_token == tok_typedef ? ec_typedef_not_allowed :
                                                 ec_storage_class_not_allowed);
          }  /* if */
          /* If an error was issued, set the flag; otherwise, set the bit in
             decl_specifiers_seen, so that the multiple-storage-class
             diagnostic will be put out if another storage class is
             specified. */
          if ((int)es > (int)es_warning) {
            err = TRUE;
          } else {
            decl_specifiers_seen |= DS_STORAGE_CLASS;
          }  /* if */
        } else {
          if (C_dialect != C_dialect_pcc && !err) {
            if (decl_specifiers_seen & ~(DS_INLINE | DS_FRIEND)) {
              /* Issue a diagnostic if the storage class is not the first
                 specifier (except for "inline" or "friend"). */
              diagnostic(strict_ansi_mode ? es_warning : es_remark,
                         ec_storage_class_not_first);
            }  /* if */
          }  /* if */
          switch (curr_token) {
            case tok_typedef:
              *storage_class = (a_storage_class)sc_typedef;  break;
            case tok_extern:
              *storage_class = (a_storage_class)sc_extern;   break;
            case tok_static:
              *storage_class = (a_storage_class)sc_static;   break;
            case tok_auto:
              *storage_class = (a_storage_class)sc_auto;     break;
            case tok_register:
              *storage_class = (a_storage_class)sc_register; break;
#if CHECKING
            default:
              internal_error("decl_specifiers: bad storage class");
#endif /* CHECKING */
          }  /* switch */
          decl_specifiers_seen |= DS_STORAGE_CLASS;
          storage_class_pos = pos_curr_token;
          if (decl_pos_block != NULL) {
            /* Set the source position of the storage class for use by the
               caller in issuing diagnostics. */
            decl_pos_block->storage_class_pos = pos_curr_token;
          }  /* if */
        }  /* if */
        break;
#if ASM_FUNCTION_ALLOWED
      case tok_asm:
        /* Specifier indicating an asm function declaration.  It is
           inconsistent with an explicit storage class declaration or
           "inline". */
        if (*storage_class == (a_storage_class)sc_asm) {
          error(ec_dupl_decl_specifier);
          err = TRUE;
        } else if (!(input_flags & DSI_ASM_ALLOWED) ||
                   (decl_specifiers_seen & (DS_INLINE | DS_STORAGE_CLASS))) {
          /* asm is not allowed if we've already seen a storage class or
             inline. */
          error(ec_asm_not_allowed);
          err = TRUE;
        } else {
          /* The asm specifier is represented as a storage class (even though
             strictly speaking it's more like "inline" as an attribute of a
             routine declaration). */
          *storage_class = (a_storage_class)sc_asm;
          decl_specifiers_seen |= DS_STORAGE_CLASS;
        }  /* if */
        break;
#endif /* ASM_FUNCTION_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_microsoft_inline:
      case tok_forceinline:
      case tok_declspec:
        /* A Microsoft specific storage class.  Note that Microsoft
           allows these in some nonstandard places such as on
           linkage declarations (e.g., extern "C" declarations). */
        {
          an_extended_decl_info_block  extended_decl_info;
          a_source_position            specifier_start_pos;
          a_boolean                    is_declspec = FALSE;
          a_boolean                    local_is_member_decl;

          local_is_member_decl =
                        ((input_flags & DSI_IS_MEMBER_DECLARATION) != 0);
          clear_extended_decl_info_block(extended_decl_info);
          specifier_start_pos = pos_curr_token;
          /* A Microsoft storage class modifier.  If this is a __declspec,
             scan the list of declaration modifiers. */
          switch (curr_token) {
            case tok_declspec:
              scan_extended_decl_modifiers(/*is_class_decl=*/FALSE,
                                           local_is_member_decl,
                                           &extended_decl_info, &err);
              decl_specifiers_seen |= DS_DECLSPEC;
              is_declspec = TRUE;
              break;
            case tok_microsoft_inline:
	      extended_decl_info.decl_modifiers.flags = DM_MICROSOFT_INLINE;
              decl_specifiers_seen |= DS_MICROSOFT_INLINE;
              break;
            case tok_forceinline:
	      extended_decl_info.decl_modifiers.flags = DM_FORCEINLINE;
              decl_specifiers_seen |= DS_FORCEINLINE;
              break;
            default:
              unexpected_condition();
          }  /* switch */
          if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED) &&
              !(input_flags & (DSI_IS_EXPLICIT_INSTANTIATION |
                               DSI_IS_SPECIALIZATION))) {
            /* Unless the declaration is an explicit instantiation or an
               explicit specialization, a diagnostic is issued when
               DSI_STORAGE_CLASS_SPECIFIER_ALLOWED is not set. */
            pos_error(ec_storage_class_not_allowed, &specifier_start_pos);
            err = TRUE;
          } else if (input_flags & DSI_IS_CONDITION_DECL) {
            pos_error(ec_storage_class_not_allowed, &specifier_start_pos);
            err = TRUE;
          } else {
            /* There were no errors; update decl_modifiers to reflect
               this specifier. */
            a_decl_modifiers_block  new_modifiers;

            new_modifiers = extended_decl_info.decl_modifiers;
            decl_modifiers->flags |= new_modifiers.flags;
            /* Check __declspec(property(...)) specifications. */
            if (new_modifiers.get_property_name != NULL) {
              if (decl_modifiers->get_property_name != NULL) {
                /* "get" specified more than once. */
                pos_error(ec_dupl_get_or_put, &specifier_start_pos);
              } else {
                decl_modifiers->get_property_name =
                                               new_modifiers.get_property_name;
              }  /* if */
            }  /* if */
            if (new_modifiers.put_property_name != NULL) {
              if (decl_modifiers->put_property_name != NULL) {
                /* "put" specified more than once. */
                pos_error(ec_dupl_get_or_put, &specifier_start_pos);
              } else {
                decl_modifiers->put_property_name =
                                               new_modifiers.put_property_name;
              }  /* if */
            }  /* if */
            if (new_modifiers.allocate_segname != NULL) {
              if (decl_modifiers->allocate_segname != NULL) {
                pos_error(ec_dupl_allocate_segname, &specifier_start_pos);
              } else {
                decl_modifiers->allocate_segname =
                                            new_modifiers.allocate_segname;
              }  /* if */
            }  /* if */
            if (is_parameter) {
              /* For parameters, warn if a storage class modifier is used. */
              pos_warning(ec_bad_param_storage_class, &specifier_start_pos);
            }  /* if */
          }  /* if */
          if (is_declspec) {
            /* Closing rparen of "__declspec(...)" has already been taken. */
            goto no_get_token;
          }  /* if */
        }
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_const:
        /* const type qualifier (3.5.3). */
        if (*qualifiers & TQ_CONST) {
          /* const may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* In Microsoft mode, duplicate qualifiers result in a warning. */
          if (microsoft_mode) es = es_warning;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_CONST;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_volatile:
        /* volatile type qualifier (3.5.3). */
        if (*qualifiers & TQ_VOLATILE) {
          /* volatile may not appear more than once. */
          es = (C_dialect == C_dialect_cplusplus) ?
                 (strict_ansi_mode ? strict_ansi_error_severity : es_warning) :
                 es_error;
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* In Microsoft mode, duplicate qualifiers result in a warning. */
          if (microsoft_mode) es = es_warning;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_VOLATILE;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_restrict:
        /* restrict type qualifier. */
        if (*qualifiers & TQ_RESTRICT) {
          /* Issue a diagnostic if restrict appears more than once. */
          es = (C_dialect == C_dialect_cplusplus) ? es_warning : es_error;
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* In Microsoft mode, duplicate qualifiers result in a warning. */
          if (microsoft_mode) es = es_warning;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          *qualifiers |= TQ_RESTRICT;
          restrict_pos = pos_curr_token;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_unaligned:
        /* Microsoft __unaligned type qualifier. */
        if (*qualifiers & TQ_UNALIGNED) {
          /* __unaligned may not appear more than once. */
          warning(ec_dupl_type_qualifier);
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_UNALIGNED;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
      case tok_near:
        /* "near" memory attribute, usually treated as a type qualifier. */
        /* This qualifier applies only on pointer declarators and not in
           normal type specifiers. */
        if ((input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) == 0) {
          goto something_unexpected;
        }  /* if */
        if (*qualifiers & TQ_NEAR) {
          /* near may not appear more than once. */
          warning(ec_dupl_mem_attrib);
        } else if (*qualifiers & TQ_FAR) {
          /* near and far are incompatible. */
          error(ec_mem_attrib_incompatible);
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_NEAR;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_far:
        /* "far" memory attribute, usually treated as a type qualifier. */
        /* This qualifier applies only on pointer declarators and not in
           normal type specifiers. */
        if ((input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) == 0) {
          goto something_unexpected;
        }  /* if */
        if (*qualifiers & TQ_FAR) {
          /* far may not appear more than once. */
          warning(ec_dupl_mem_attrib);
        } else if (*qualifiers & TQ_NEAR) {
          /* near and far are incompatible. */
          error(ec_mem_attrib_incompatible);
        } else {
          non_restrict_qualifier_pos = pos_curr_token;
          *qualifiers |= TQ_FAR;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#endif /* NEAR_AND_FAR_ALLOWED */
      case tok_friend:
	/* "friend" specifier is allowed only in a C++ class declaration.
	   This also excludes its appearing in a function parameter
	   specification. */
	if (is_parameter) {
	  /* "friend" may not appear in a function parameter specification. */
	  error(ec_bad_param_specifier);
	  err = TRUE;
	} else if (!is_member_decl) {
	  /* In fact, it may only appear in a C++ class (or struct or union)
	     declaration. */
	  error(ec_bad_specifier_outside_class_decl);
	  err = TRUE;
	} else if (decl_specifiers_seen & DS_FRIEND) {
	  /* Only one "friend" specifier at at time. */
	  error(ec_dupl_decl_specifier);
	  err = TRUE;
	} else {
          decl_specifiers_seen |= DS_FRIEND;
	  *output_flags |= DSO_FRIEND;
          if (decl_specifiers_seen != DS_FRIEND) {
            if ((decl_specifiers_seen & DS_STORAGE_CLASS) && !microsoft_mode) {
              error(ec_storage_class_in_friend_decl);
              err = TRUE;
              *storage_class = (a_storage_class)sc_unspecified;
              decl_specifiers_seen &= ~(DS_STORAGE_CLASS);
            } else if (decl_specifiers_seen & DS_MUTABLE) {
              pos_error(ec_mutable_not_allowed, &storage_class_pos);
              err = TRUE;
              decl_specifiers_seen &= ~(DS_MUTABLE);
              *output_flags &= ~DSO_MUTABLE;
            }  /* if */
          } else {
            /* Check for a special case -- a friend declaration of the form
               "friend T;" which is taken to mean the same as "friend class T;"
               by cfront (even if T has not yet been defined).  Although there
               is no support for this syntax in the ARM, we accept it (except
               in strict ANSI mode) since it is widely used in older C++
               code.  */
            /* Advance to the token following "friend". */
            (void)get_token();
            if (!is_decl_qualified_name_start()) {
              /* Can't be the start of an identifier -- back up to continue
                 processing. */
              unget_token();
              curr_token = tok_friend;
            } else {
              /* Advance over the identifier, which may actually be a
                 qualified name or even a template class. */
              a_boolean          lookup_err;
              a_symbol_ptr       tag_sym;
              a_source_position  ident_pos;

              ident_pos = pos_curr_token;
              tag_sym = coalesce_and_lookup_generalized_identifier(
                              GID_NO_OPTIONS, ilm_tentative_type, &lookup_err);
              /* Even if the lookup was successful, if the next token is not
                 a ";" this is not of the form "friend T;". */
              if (next_token() != tok_semicolon) {
                /* No semicolon -- back up. */
                clear_specific_symbol(locator_for_curr_id);
                unget_token();
                curr_token = tok_friend;
              } else if (any_cfront_mode() && tag_sym == NULL) {
                /* This friend declaration introduces a new type -- which is
                   okay in cfront compatibility mode.  Still, issue a remark
                   on use of a nonstandard feature. */
                vacuous_decl_allowed = FALSE;
                pos_st_remark(ec_nonstd_friend_decl, &ident_pos, "class");
                goto process_class_specifier;
              } else {
                a_type_ptr  tp = NULL;
                a_boolean   is_typedef = FALSE;

                if (tag_sym != NULL && is_class_symbol(tag_sym)) {
                  tp = type_symbol_type(tag_sym);
                  if (tp->kind == (a_type_kind)tk_typeref) {
                    is_typedef = TRUE;
                    if (is_qualified_type(tp)) {
                      /* This should be an error:
                           typedef const struct A TA;
                           class B { friend TA; };
                      */
                      tp = NULL;
                    }  /* if */
                  }  /* if */
                }  /* if */
                if (tp == NULL) {
                  /* Lookup failed to find an unqualified class symbol.  Issue
                     an error. */
                  error(ec_bad_friend_decl);
                  err = TRUE;
                  basic_type = bt_error;
                } else {
                  /* This declaration is of the form "friend T;" and T is
                     a previously declared class name.  Issue a diagnostic
                     for using a nonstandard feature. */
                  char               *class_key_string;

                  switch (skip_typerefs(tp)->kind) {
                    case tk_class:   class_key_string = "class";   break;
                    case tk_struct:  class_key_string = "struct";  break;
                    case tk_union:   class_key_string = "union";   break;
#if CHECKING
                    default: internal_error("decl_specifiers: bad type kind");
#endif /* CHECKING */
                  }  /* switch */
                  /* Strict ANSI diagnostic in strict ANSI mode, remark
                     otherwise. */
                  pos_st_diagnostic(strict_ansi_mode ?
                                      strict_ansi_error_severity : es_remark,
                                    ec_nonstd_friend_decl, &ident_pos,
                                    class_key_string);
                  /* Note that scan_class_specifier is not called for this
                     case.  Therefore, mark the symbol declared. */
                  record_symbol_declaration(SRK_DECLARATION | SRK_FRIEND,
                                            tag_sym, &ident_pos,
                                            (a_source_sequence_entry_ptr)NULL);
                  if (!is_typedef) declares_something = TRUE;
                  *type_ptr = tp;
                  basic_type = bt_struct_union;
                  is_elaborated_type_specifier = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
	}  /* if */
	break;
      case tok_virtual:
        if (is_parameter) {
          /* "virtual" may not appear in a function parameter specification. */
          error(ec_bad_param_specifier);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_FRIEND) {
          error(ec_virtual_not_allowed);
          err = TRUE;
        } else if (!is_member_decl) {
          /* In fact, it may only appear in a C++ class (or struct or union)
             declaration. */
          error(ec_bad_specifier_outside_class_decl);
          err = TRUE;
        } else if (input_flags & DSI_IS_TEMPLATE_DECLARATION) {
          /* Must be a member function template -- virtual is not allowed. */
          error(ec_virtual_function_template);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_VIRTUAL) {
          /* Only one "virtual" specifier at a time. */
          diagnostic(microsoft_mode ? es_warning : es_error,
                     ec_dupl_decl_specifier);
          if (!microsoft_mode) {
            err = TRUE;
          }  /* if */
        } else {
          decl_specifiers_seen |= DS_VIRTUAL;
          *output_flags |= DSO_VIRTUAL;
        }  /* if */
        break;
      case tok_inline:
        if (is_parameter) {
          /* "inline" may not appear in a function parameter specification. */
          error(ec_bad_param_specifier);
          err = TRUE;
        } else if (!(input_flags & DSI_INLINE_ALLOWED) ||
                   (C_dialect == C_dialect_cplusplus &&
                    !extern_inline_allowed &&
                    *storage_class == (a_storage_class)sc_extern)) {
          /* "inline" allowed on certain function declarations only. */
          error(ec_inline_not_allowed);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_INLINE) {
          /* Only one "inline" specifier at a time.  C99 and Microsoft C++
             allow multiple "inline" specifiers, but that is unlikely the
             intent of a programmer. */
          diagnostic((c99_mode || microsoft_mode) ? es_warning : es_error,
                     ec_dupl_decl_specifier);
          if (!(c99_mode || microsoft_mode)) {
            err = TRUE;
          }  /* if */
        } else if (input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) {
          /* The keyword "inline" was seen as a qualifier.  This is only
             possible in Microsoft mode and that qualifier is ignored. */
          check_assertion(microsoft_mode);
          warning(ec_inline_qualifier_ignored);
        } else {
          decl_specifiers_seen |= DS_INLINE;
          *output_flags |= DSO_INLINE;
        }  /* if */
        break;
      case tok_explicit:
        if (is_parameter) {
          /* "explicit" may not appear in a function parameter
              specification. */
          error(ec_bad_param_specifier);
          err = TRUE;
        } else if (!is_member_decl) {
          /* It's only allowed inside a class definition. */
          error(ec_explicit_not_allowed);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_EXPLICIT) {
          /* Disallow duplicates. */
          error(ec_dupl_decl_specifier);
          err = TRUE;
        } else {
          decl_specifiers_seen |= DS_EXPLICIT;
          *output_flags |= DSO_EXPLICIT;
        }  /* if */
        break;
      case tok_void:
        if (C_dialect == C_dialect_pcc) {
          /* To allow "typedef <something> void;" to be ignored in pcc mode,
             stop scanning on "void" when a basic type has already been scanned
             in a typedef. */
          if (basic_type != bt_none &&
              *storage_class == (a_storage_class)sc_typedef) {
            goto exit_loop;
          }  /* if */
        }  /* if */
        /* Fall-through to next case. */
      case tok_char:
      case tok_wchar_t:
      case tok_c99_bool:
      case tok_bool:
      case tok_int:
      case tok_float:
      case tok_double:
        /* A type specifier (3.5.2) that indicates a basic type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (basic_type != bt_none) {
          /* Basic type has already been specified in some way. */
          bad_combination_of_type_specifiers = TRUE;
          error(ec_bad_combination_of_type_specifiers);
        } else {
          switch (curr_token) {
            case tok_void:     basic_type = bt_void;    break;
            case tok_char:     basic_type = bt_char;    break;
            case tok_wchar_t:  basic_type = bt_wchar_t; break;
            case tok_c99_bool:
            case tok_bool:     basic_type = bt_bool;    break;
            case tok_int:      basic_type = bt_int;     break;
            case tok_float:    basic_type = bt_float;   break;
            case tok_double:   basic_type = bt_double;  break;
#if CHECKING
            default:
              internal_error("decl_specifiers: bad type specifier");
#endif /* CHECKING */
          }  /* switch */
          if (curr_token == tok_void && !any_decl_specifiers_seen) {
            decl_specifiers_seen = DS_VOID;
          } else {
            decl_specifiers_seen |= DS_TYPE;
          }  /* if */
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_int8:
      case tok_int16:
      case tok_int32:
      case tok_int64:
        /* The Microsoft keywords __int8, __int16, __int32, and __int64
           represent a basic type and a size in combination.  In other words,
           an explicit size may not be specified in conjunction with either. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (basic_type != bt_none || size != size_none) {
          /* Basic type or size has already been specified in some way. */
          bad_combination_of_type_specifiers = TRUE;
          error(ec_bad_combination_of_type_specifiers);
        } else {
          if (curr_token == tok_int8) {
            /* __int8 is treated as a plain char. */
            basic_type = bt_char;
            size = size_int8;
          } else {
            /* Set both basic type and size. */
            basic_type = bt_int;
            switch (curr_token) {
              case tok_int16:  size = size_int16; break;
              case tok_int32:  size = size_int32; break;
              case tok_int64:  size = size_int64; break;
              default:;
            }  /* switch */
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_short:
      case tok_long:
        /* A type specifier (3.5.2) that modifies the length of a basic
           type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (size != size_none) {
          /* Size has already been specified in some way. */
          if (size == size_long && curr_token == tok_long) {
            /* long long.  This is an extension. */
#if LONG_LONG_ALLOWED
            size = size_long_long;
            if (strict_ansi_mode && !long_long_is_standard) {
              diagnostic(strict_ansi_discretionary_severity,
                         ec_nonstd_long_long);
            }  /* if */
#else /* !LONG_LONG_ALLOWED */
            if (any_cfront_mode()) {
              /* Cfront warns about "long long" and treats it as "long". */
              warning(ec_dupl_decl_specifier);
            } else {
              error(ec_nonstd_long_long);
            }  /* if */
#endif /* LONG_LONG_ALLOWED */
          } else if (size == size_short && curr_token == tok_short) {
            /* "short short".  Issue an error, except in cfront mode,
               which is silent about "short short". */
            diagnostic((any_cfront_mode() ? es_warning : es_error),
                       ec_dupl_decl_specifier);
          } else {
            /* Some other bad combination. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
          }  /* if */
        } else {
          /* First specification of size. */
          if (curr_token == tok_short) {
            size = size_short;
          } else {
            size = size_long;
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case tok_c99_complex:
        if (!c99_mode) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (complex_attr == cxa_complex) {
          /* E.g. "_Complex float _Complex". */
          error(ec_dupl_decl_specifier);
        } else if (complex_attr == cxa_imaginary) {
          /* E.g. "_Imaginary float _Complex". */
          error(ec_bad_combination_of_type_specifiers);
          bad_combination_of_type_specifiers = TRUE;
        } else {
          complex_attr = cxa_complex;
        }  /* if */
        break;
      case tok_c99_imaginary:
        if (!c99_mode) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (complex_attr == cxa_imaginary) {
          /* E.g. "_Imaginary float _Imaginary". */
          error(ec_dupl_decl_specifier);
        } else if (complex_attr == cxa_complex) {
          /* E.g. "_Complex float _Imaginary". */
          error(ec_bad_combination_of_type_specifiers);
          bad_combination_of_type_specifiers = TRUE;
        } else {
          complex_attr = cxa_imaginary;
        }  /* if */
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      case tok_signed:
      case tok_unsigned:
        /* A type specifier (3.5.2) that modifies the signedness of a
           basic type. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else if (sign != sign_none) {
          /* Sign has already been specified in some way. */
          if ((sign == sign_signed) == (curr_token == tok_signed)) {
            /* Either "signed signed" or "unsigned unsigned".  Issue an error,
               except in cfront mode. */
            diagnostic((any_cfront_mode() ? es_warning : es_error),
                       ec_dupl_decl_specifier);
          } else {
            /* Mixing signs. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
          }  /* if */
        } else {
          /* First specification of sign. */
          sign = (curr_token == tok_signed) ? sign_signed : sign_unsigned;
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
      case tok_class:
      case tok_struct:
      case tok_union:
process_class_specifier:
        /* A struct or union specifier (3.5.2.1). */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            if (any_decl_specifiers_seen) vacuous_decl_allowed = FALSE;
            if (microsoft_mode && is_member_decl && !err &&
                is_constructor_decl(enclosing_class_type(input_flags))) {
              /* In Microsoft mode, "struct S { struct S(); }; is accepted. */
              basic_type = bt_no_type;
              *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
              /* Skip "class" or "struct". */
              (void)get_token();
              goto exit_loop;
            } else {
              if (!class_specifier(
                          vacuous_decl_allowed,
                          (decl_specifiers_seen & DS_FRIEND) != 0,
                          *storage_class == (a_storage_class)sc_typedef,
                          (input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                          (input_flags & DSI_IS_EXPLICIT_INSTANTIATION) != 0,
                          (input_flags & DSI_IS_SPECIALIZATION) != 0,
                          type_ptr, &declares_something,
                          &defines_something, decl_pos_block)) {
                err = TRUE;
              }  /* if */
              basic_type = bt_struct_union;
              is_elaborated_type_specifier = TRUE;
            }  /* if */
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
            /* Scan the specifier anyway, but throw it away. */
            (void)class_specifier(
                          /*vacuous_decl_allowed=*/FALSE,
                          /*is_friend_decl=*/FALSE,
                          /*is_typedef=*/FALSE,
                          (input_flags & DSI_IS_EXPLICIT_INSTANTIATION) != 0,
                          (input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                          (input_flags & DSI_IS_SPECIALIZATION) != 0,
                          &dummy_type, &dummy_flag, &dummy_flag,
                          decl_pos_block);
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
      case tok_enum:
        /* An enumeration specifier (3.5.2.2). */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            if (any_decl_specifiers_seen || strict_ansi_mode) {
              vacuous_decl_allowed = FALSE;
            }  /* if */
            enum_specifier(vacuous_decl_allowed, type_ptr,
                           &declares_something, &defines_something,
                           decl_pos_block);
            if (is_error_type(*type_ptr)) {
              /* An error was detected in enum_specifier -- typically, an
                 ill-formed tag name. */
              err = TRUE;
              basic_type = bt_error;
            } else {
              basic_type = bt_enum;
              is_elaborated_type_specifier = TRUE;
            }  /* if */
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
            /* Scan the specifier anyway, but throw it away. */
            enum_specifier(/*vacuous_decl_allowed=*/FALSE,
                           &dummy_type, &dummy_flag, &dummy_flag,
                           decl_pos_block);
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
      case tok_typename:
        /* A typename specifier.  The typename keyword is used to
	   specify that the qualified name that follows the keyword is
	   a type.  This is used to parse template definitions (as
           opposed to parsing a template instantiation when the values of
           the template parameters are known. */
        if (!type_specifier_allowed) {
          error(ec_type_specifier_not_allowed);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            typename_specifier(type_ptr, /*within_using_decl=*/FALSE,
                               decl_pos_block);
            basic_type = bt_typename;
            is_elaborated_type_specifier = TRUE;
          } else {
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            error(ec_bad_combination_of_type_specifiers);
            /* Scan the specifier anyway, but throw it away. */
            typename_specifier(&dummy_type, /*within_using_decl=*/FALSE,
                               decl_pos_block);
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
      case tok_overload:
        /* Special case -- the "overload" keyword (which shows up in
           cfront compatibility mode only).  Ignore it and advance to the
           next token. */
        diagnostic(anachronism_error_severity, ec_overload_anachronism);
        decl_specifiers_seen |= DS_OVERLOAD;
        break;
      case QUALIFIED_NAME_START_CASE:  /* Identifier or "::". */
        /* Identifier. */
        if (C_dialect == C_dialect_cplusplus) {
          an_identifier_options_set  options = GID_NO_OPTIONS;

          /* In case the identifier has not yet been coalesced, do it now. */
          if (input_flags & DSI_IS_NEW_TYPE_NAME) {
            options |= GID_IS_NEW_TYPE_NAME;
          }  /* if */
          if (!is_generalized_identifier_start(options)) {
            /* This could result from "::" followed by something strange. */
            goto something_unexpected;
          }  /* if */
          /* Check for a constructor declaration.  The following conditions
             must be satisfied:  (1) we are inside a class definition;
             (2) the current token is the name of the class being defined
             (note that typedef names are not allowed); (3) the declaration
             has no other specifiers besides "inline" and "explicit" (which
             are legal) and "virtual" or "static" (which are not); (4) the
             next token is a left parenthesis; (5) the token following the
             left paren is a right paren or the start of a formal parameter
             declaration. */
          if (is_member_decl &&
              !(decl_specifiers_seen & ~(DS_VIRTUAL | DS_STORAGE_CLASS |
                                         DS_EXPLICIT | DS_INLINE |
                                         DS_DECLSPEC | DS_MICROSOFT_INLINE |
                                         DS_FORCEINLINE)) &&
              (*storage_class == (a_storage_class)sc_unspecified ||
               *storage_class == (a_storage_class)sc_static)) {
            a_type_ptr  class_type = enclosing_class_type(input_flags);

            if (class_type != NULL) {
              /* The following test will not succeed if the constructor
                 declaration is parenthesized; in that case, the test is
                 repeated in scan_real_declarator_id. */
              if (is_constructor_decl(class_type)) {
                basic_type = bt_no_type;
                *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
                /* Note that with a branch to exit_loop the get_token call
                   is bypassed.  This means curr_token will still represent
                   the constructor name (= class name) upon return to the
                   caller. */
                goto exit_loop;
              } else if ((microsoft_bugs || any_cfront_mode()) &&
                         implicit_int_member_with_name_of_type()) {
                /* Microsoft and Cfront will accept:
                     struct X; struct Y { X(); }; */
                *output_flags |= DSO_NO_DECL_SPECIFIERS;
                goto exit_loop;
              }  /* if */              
            }  /* if */              
          }  /* if */
        }  /* if */
        /* The appearance of an identifier may mean that the specifiers
           are complete (the identifier is a declarator) or it may be another
           specifier.  First we look for conditions that will cause us to
           exit the loop -- generally because the identifier is clearly not
           a specifier. */
        /* To be more specific: in ANSI C, an identifier that appears to be
           a typedef name is not recognized as such if the specifiers list
           already includes a basic type or sign.  This is so that the
           following (from the standard, 3.5.6) will work:
               typedef signed int t;
               main () {
                 long t;    <-- This declares a new identifier t.
               }
           K&R (first edition, Appendix A, section 11.1) also includes the
           following:
               typedef float distance;
               {
                 auto int distance;
               }
           but pcc (at least on a Sun 3) doesn't accept this.  It always
           seems to scan a typedef name as a typedef name.  To conform to
           K&R, we consider an identifier to be a typedef when there is
           just a sign or size (since these are "adjectives" to pcc), but
           not when there is a type specifier. */
        if (basic_type != bt_none) {
          /* There's already a basic type, so the identifier should be
             processed as a declarator.  If it happens to be a type name,
             it is better to have a invalid-redeclaration error later than
             a bad-combination-of-types error here.  In addition, the
             following is permitted in C++:
                 struct S {...};
                 int S;
             since tag names are not in the same name space with other
             objects. */
          goto exit_loop;
        }  /* if */
        if (sign != sign_none || size != size_none) {
          /* There is an indication of sign and/or size (but no indication
             of a basic type).  In ANSI C and C++, assume we're dealing with
             a declarator.  In pcc mode, adjectival modification of a typedef
             is allowed in certain circumstances, so keep going till we know
             if the identifier is a typedef. */
          if (C_dialect != C_dialect_pcc) goto exit_loop;
        }  /* if */
        /* Look up the identifier as a type symbol, if it has not already been
           looked up. */
        curr_token_type_symbol =
                    curr_type_symbol((input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                     /*in_prescan=*/FALSE);
        if (curr_token_type_symbol != NULL) {
          if (locator_for_curr_id.is_class_member &&
              curr_token_type_symbol->kind == (a_symbol_kind)sk_type &&
              curr_token_type_symbol->variant.type.is_injected_class_name &&
              (!(decl_specifiers_seen &
                 ~(DS_FRIEND | DS_INLINE | DS_DECLSPEC |
                   DS_MICROSOFT_INLINE | DS_FORCEINLINE)))) {
            /* This identifier appears specify a constructor. */
            a_type_ptr    tp = type_symbol_type(curr_token_type_symbol);
            a_symbol_ptr  sym = symbol_supplement_for_class(tp)->constructor;

            if (sym != NULL &&
                skip_typerefs(locator_for_curr_id.parent.class_type) == tp) {
              *output_flags |= DSO_CONSTRUCTOR;
              if (!any_decl_specifiers_seen) {
                *output_flags |= DSO_NO_DECL_SPECIFIERS;
              }  /* if */
              basic_type = bt_no_type;
              locator_for_curr_id.specific_symbol = sym;
              locator_for_curr_id.symbol_header = sym->header;
              goto exit_loop;
            }  /* if */
          }  /* if */
          if (sign != sign_none || size != size_none) {
            /* We are in pcc mode, in which adjectival modification of a
               typedef is allowed -- but with restrictions.  For integral
               types, any size/sign is allowed. For floating types, only
               "long" is allowed. */
            a_type_ptr  tp = type_symbol_type(curr_token_type_symbol);
            if (is_integral_or_enum_type(tp) ||
                (is_floating_type(tp) &&
                 sign == sign_none && size == size_long)) {
              /* Adjectives okay. */
            } else {
              goto exit_loop;
            }  /* if */
          }  /* if */
          /* Do ambiguity and access control checking. */
          check_ambiguity_and_verify_access(&locator_for_curr_id);
          /* The identifier is a type name and should be treated as a
             type specifier. */
          if (is_error_locator(locator_for_curr_id)) {
            /* Ambiguity error was issued. */
            err = TRUE;
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
            *type_ptr = error_type();
          } else {
            if (locator_for_curr_id.is_semivisible_nested_type) {
              /* The symbol in the locator is a nested class that is not
                 visible according to the ARM lookup rules but is returned
                 in support of the nested class anachronism (ARM 18.3.5).
                 Issue an anachronism diagnostic. */
              sym_diagnostic(anachronism_error_severity, 
                             ec_nested_class_anachronism,
                             locator_for_curr_id.specific_symbol);
            }  /* if */
            /* If the symbol is a projection symbol, get the fundamental
               symbol. */
            reduce_projection_symbol_to_fundamental_symbol(
                                                      curr_token_type_symbol);
            mark_referenced(curr_token_type_symbol,
                            &locator_for_curr_id.source_position);
            if (!type_specifier_allowed) {
              error(ec_type_specifier_not_allowed);
              err = TRUE;
            } else {
              /* Save the type. */
              basic_type = bt_typedef;
              *type_ptr = type_symbol_type(curr_token_type_symbol);
              decl_specifiers_seen |= DS_TYPE;
            }  /* if */
          }  /* if */
          break;
        }  /* if */
        /* Getting to this point means the identifier is not a type name.
           However, the lookup may still have found something -- see
           locator_for_curr_id.specific_symbol. */
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          /* This is an identifier in a "new" expression so it was
             probably intended to be a type name.  Issue an error and
             pretend that's what it is. */
          error(ec_exp_type_specifier);
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          decl_specifiers_seen |= DS_TYPE;
          break;
        }  /* if */
        if (locator_for_curr_id.is_operator_name ||
            locator_for_curr_id.is_conversion_name) {
          /* This identifier represents something like "A::operator+" or
             "A::operator int"." */
          goto operator_or_conversion_name;
        }  /* if */
        if (locator_for_curr_id.is_destructor_name &&
            !(decl_specifiers_seen & DS_FRIEND) &&
	    (simplify_curr_class_qualified_name() ||
	     !locator_for_curr_id.is_qualified_name)) {
          /* This identifier represents something like "A::~A".  This
             case is handled one way if we are currently processing the
             definition of class A and another way if we are not.  If we
	     are processing the definition of class A, "A::~A" will have
	     already been coalesced and the qualifier information
             will have been discarded by simplify_curr_class_qualified_name.
             This test identifies this case and transfers control to the code
             that would have been executed if the program simply said "~A"
             instead of "A::~A".  If we are not processing the definition of
             class A, we simply fall through this test. */
          goto destructor_name;
        }  /* if */
        bad_type_name_error = FALSE;
        if (is_error_locator(locator_for_curr_id) &&
            locator_for_curr_id.is_template_id) {
          /* An error was detected in scanning a class template id.  Since
             a template id can only be a type, treat it as an error type. */
          bad_type_name_error = TRUE;
        } else if (!any_decl_specifiers_seen &&
                   !(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
          /* If this is the first specifier, and this identifier is undefined,
             assume that we are dealing with a name that was supposed to be
             declared as a typedef.  Note that we do not get here on
             declarations, so this bit of error recovery tweaking applies
             only to things like prototyped parameter declarations and
             members of structs/unions. */
          bad_type_name_error = TRUE;
        } else if (!any_decl_specifiers_seen &&
                   is_error_locator(locator_for_curr_id) &&
                   locator_for_curr_id.is_global_qualified_name) {
          /* An error was detected in scanning a qualified name that started
             with "::".  Since declarators may not start with "::" we assume
             this to be like an unidentified type name. */
          bad_type_name_error = TRUE;
        } else if (type_specifier_allowed && basic_type == bt_none &&
                   sign == sign_none && size == size_none) {
          /* This is an error recovery optimization.  The current identifier
             is not a type name, but if it is undefined and the next token
             is the start of a declarator, we may plausibly have something
             like "extern x y" or static x *z", where x can be interpreted
             as a type name. */
          a_token_cache  cache;

          clear_token_cache(&cache, /*reusable=*/FALSE);
          /* Put the current token in the cache. */
          cache_curr_token(&cache);
          /* Advance to next token. */
          (void)get_token();
          /* Check next token for start of a declarator.  "(" may be part
             of a function declaration, so it doesn't count. */
          if (is_declarator_start() && curr_token != tok_lparen) {
            /* Assume that the undefined identifier that is apparently
               followed by a declarator was intended to be a type name. */
            bad_type_name_error = TRUE;
          }  /* if */
          /* Restore the token state. */
          rescan_cached_tokens(&cache);
        }  /* if */
        if (bad_type_name_error) {
          report_bad_type_name(input_flags);
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          decl_specifiers_seen |= DS_TYPE;
          break;
        }  /* if */
        if (!(decl_specifiers_seen &
              ~(DS_FRIEND | DS_INLINE | DS_DECLSPEC |
                DS_MICROSOFT_INLINE | DS_FORCEINLINE))) {
          /* A function declaration without declaration specifiers is
             permitted. */
          if (!any_decl_specifiers_seen) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          /* Set the type appropriately if this the name of a constructor
             member function. */
          if (locator_for_curr_id.is_class_member) {
            a_symbol_ptr  sym  = class_qualified_id_lookup(
                                        &locator_for_curr_id,
                                        locator_for_curr_id.parent.class_type,
                                        IDL_DIRECT_CLASS_MEMBERS_ONLY);
            if (sym != NULL) {
              if (is_constructor_symbol(sym)) {
                *output_flags |= DSO_CONSTRUCTOR;
                basic_type = bt_no_type;
              } else if (is_destructor_symbol(sym)) {
                *output_flags |= DSO_DESTRUCTOR;
                basic_type = bt_no_type;
              } else {
                clear_specific_symbol(locator_for_curr_id);
              }  /* if */
            }  /* if */
          }  /* if */
          goto exit_loop;
        } else if (decl_specifiers_seen & DS_FRIEND) {
          /* Clear the specific symbol pointer in the locator so that
             subsequent lookups will be done correctly. */
          clear_specific_symbol(locator_for_curr_id);
          goto exit_loop;
        }  /* if */
        /* For non-typedef identifiers, branch to the default case. */
        goto something_unexpected;
      case tok_operator:
        /* Coalesce the operator name. */
        (void)is_generalized_identifier_start(GID_NO_OPTIONS);
operator_or_conversion_name:
        if (locator_for_curr_id.is_conversion_name) {
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* A type has already been specified, but the error is issued
               later in order to unify error processing for both of the
               following (only the first of which is detected here):
                 class A {
                   char operator char();       // Error detectable here
                   char* operator char*();     // Error not detectable here
                 };
               Errors for both are issued in declarator. */
          }  /* if */
        } else if (locator_for_curr_id.is_operator_name &&
                   is_member_decl && !(decl_specifiers_seen & DS_FRIEND)) {
          if (is_new_operator(locator_for_curr_id.variant.opname) ||
              is_delete_operator(locator_for_curr_id.variant.opname)) {
            /* We are inside a class definition, so an operator new or
               operator delete function is automatically treated as a
               static member function, even if "static" is not explicitly
               specified. */
            if (*storage_class == (a_storage_class)sc_unspecified) {
              *storage_class = (a_storage_class)sc_static;
            }  /* if */
          }  /* if */
        }  /* if */
        goto exit_loop;
      case tok_template:
        /* "template" cannot appear in decl-specifiers. */
        set_to_error_locator(locator_for_curr_id);
        locator_for_curr_id.source_position = pos_curr_token;
        pos_error(ec_template_not_allowed, &pos_curr_token);
        if (next_token() == tok_lt) {
          flush_tokens();
        } else {
          (void)get_token();
        }  /* if */
        err = TRUE;
        if (basic_type == bt_typedef) {
          *type_ptr = error_type();
        } else {
          basic_type = bt_error;
        }  /* if */
        goto no_get_token;
      case tok_compl:
destructor_name:
        if (is_member_decl) {
          if (!any_decl_specifiers_seen) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          *output_flags |= DSO_DESTRUCTOR;
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* It is an error to specify the type on a destructor, but it
               will be reported later. */
          }  /* if */
          goto exit_loop;
        }  /* if */
        /* If destructors aren't expected, fall through into the default
           case. */
      default:
        /* Something unexpected.  After the first time, we can just exit
           the loop (we've taken all we're supposed to).  The first time,
           this is an error. */
something_unexpected:
        if (!any_decl_specifiers_seen) {
          if (!(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
            syntax_error(ec_exp_type_specifier);
            err = TRUE;
            basic_type = bt_error;
          } else {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
        }  /* if */
        goto exit_loop;
    }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      /* Each time through the loop assume the current token is the last. */
      decl_pos_block->specifiers_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
no_get_token:
    any_decl_specifiers_seen = TRUE;
    /* Check for special conditions that will cause this loop to terminate. */
    if (input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) {
      /* We are only interested in scanning type qualifiers in a
         pointer declarator. */
      if (is_type_qualifier() or_is_near_or_far() ||
          (microsoft_mode && curr_token == tok_inline)) {
        /* Keep looping. */
      } else {
        /* Did we see tok_inline used as a qualifier? */
        goto exit_loop;
      }  /* if */
    } else if (defines_something &&
               input_flags & DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER) {
      /* The basic type is a class, struct, union, or enum that actually
         defines a type.  We are especially interested in cases like this:
           class A {...}          <== Note the missing semicolon.
           class B {...};
         where we'd rather report a missing semicolon than a conflict of
         types.  This is referred to as a "dangling type specifier".  Look
         for a type-specifier keyword or (if this is not a typedef declaration)
         a type name.  E.g.,
           class A {...} int...                 <== Dangling type specifier
         Note that this logic works for both C++ and standard C. */
      if (is_type_specifier()) {
        /* The current token is a type keyword; treat it as the start
           of a new declaration.  The error on missing punctuation will be
           handled by the caller. */
        dangling_type_specifier = TRUE;
        goto exit_loop;
      }  /* if */
    }  /* if */
  }  /* for */

exit_loop:
  if (microsoft_mode && (decl_specifiers_seen & DS_STORAGE_CLASS)) {
    /* Certain Microsoft-mode diagnostics involving storage class specifiers
       are put off until all the specifiers have been collected. */
    if (decl_specifiers_seen & DS_FRIEND) {
      /* "extern" and "static" are permitted on a friend declaration in
         Microsoft compatibility mode -- other storage classes are ignored,
         with a warning. */
      if (*storage_class != (a_storage_class)sc_extern &&
          *storage_class != (a_storage_class)sc_static) {
        pos_warning(ec_storage_class_in_friend_decl, &storage_class_pos);
        *storage_class = (a_storage_class)sc_unspecified;
        decl_specifiers_seen &= ~DS_STORAGE_CLASS;
      }  /* if */
    } else if (is_member_decl) {
      /* The diagnostics on invalid storage class were deferred, in case
         this turned out to be a friend declaration instead of a member
         declaration. */
      if (*storage_class != (a_storage_class)sc_typedef &&
          *storage_class != (a_storage_class)sc_static) {
        pos_error(ec_bad_member_storage_class, &storage_class_pos);
        *storage_class = (a_storage_class)sc_unspecified;
        decl_specifiers_seen &= ~DS_STORAGE_CLASS;
      }  /* if */
    }  /* if */
  }  /* if */
  if (decl_specifiers_seen == DS_VOID) {
    /* Set the output_flags bit to indicate that the sequence of specifiers
       had just one specifier, and it was "void". */
    *output_flags |= DSO_JUST_VOID;
  } else if (is_elaborated_type_specifier) {
    if (!err && !defines_something &&
        !(decl_specifiers_seen & (DS_STORAGE_CLASS | DS_INLINE |
                                  DS_VIRTUAL | DS_TYPE_QUALIFIER))) {
      *output_flags |= DSO_ELABORATED_TYPE_SPECIFIER;
      if (basic_type == bt_typename) { *output_flags |= DSO_TYPENAME; }
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL && !any_decl_specifiers_seen) {
    /* No decl-specifiers were seen, so clear the starting position. */
    decl_pos_block->specifiers_range.start = null_source_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  /* Return the position of the first specifier as the position of the
     overall list of specifiers for error purposes. */
  copy_source_position(start_pos, error_position);

  if (type_specifier_allowed) {
    if ((basic_type != bt_none && basic_type != bt_no_type) ||
        sign != sign_none || size != size_none) {
      /* Set the flag indicating an explicit type specifier.  Adjectival
         type modifiers (size, sign) are okay, since this is for detecting
         implicit void function return types in pre-ANSI C. */
      *output_flags |= DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
      /* Note that is certain cases a diagnostic is issued to warn the
         user about a missing type specifier.  This is handled by the
         caller. */
    }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
    if (complex_attr != cxa_none &&
        basic_type != bt_float && basic_type != bt_double) {
      /* "_Imaginary" and "_Complex" must be combined with a floating-point
         type.  For error recovery purposes we assume "double" was actually
         specified. */
      str_error(ec_only_applies_to_float_types,
                (complex_attr == cxa_complex) ? "_Complex" : "_Imaginary");
      basic_type = bt_double;
      *output_flags |= DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
    }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    if (dangling_type_specifier) {
      /* Set the bit to mark a malformed type specification, typically
         caused by a missing semicolon following an class, struct, union,
	 or enum declaration.  Error reporting is left to the caller in
         such cases. */
      *output_flags |= DSO_DANGLING_TYPE_SPECIFIER;
    }  /* if */
    if (bad_combination_of_type_specifiers) {
      /* Error has already been diagnosed. */
      *type_ptr = error_type();
      err = TRUE;
    } else {
      /* Combine the type specifiers (except for the type qualifiers) into a
         type.  *type_ptr is updated, based on the basic type, sign, and size
         specified. */
      if (!combine_type_specifiers(type_ptr, basic_type, sign, size,
                                   complex_attr)) {
        err = TRUE;
      } else {
        /* Add any type qualifiers (const or volatile) to the type. */
        if (!add_type_qualifiers(type_ptr, qualifiers,
                                 &non_restrict_qualifier_pos,
                                 &restrict_pos)) {
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* If there was an error, assume something was declared.  Who knows what the
     correct code should have done. */
  if (err || declares_something) *output_flags |= DSO_DECLARES_SOMETHING;
  if (defines_something) *output_flags |= DSO_DEFINES_SOMETHING;
#if DEBUG
  if (debug_level >= 3) {
    fputs("type_ptr: ", f_debug);
    if (*type_ptr == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_type(*type_ptr);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return(err);
}  /* decl_specifiers */


void decl_spec_one_time_init(void)
/*
Do one-time initialization of variables related to the processing of
decl-specifiers.
*/
{
  if (!enum_types_can_be_larger_than_int) {
    largest_enum_int_kind = (an_integer_kind)ik_int;
  } else {
#if LONG_LONG_ALLOWED
    largest_enum_int_kind = (an_integer_kind)ik_unsigned_long_long;
#else /* !LONG_LONG_ALLOWED */
    largest_enum_int_kind = (an_integer_kind)ik_unsigned_long;
#endif /* LONG_LONG_ALLOWED */
  }  /* if */

  /* Save variables from decl_spec.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(largest_enum_int_kind),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* decl_spec_one_time_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
