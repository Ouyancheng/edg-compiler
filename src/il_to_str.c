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

il_to_str.c -- Produce an external string-form representation for various
               IL entries.

*/

#include "basics.h"
#include "host_envir.h"
#include "il_to_str.h"
#include "il.h"
#include "const_ints.h"
#include "float_pt.h"
#include "types.h"
#include "target.h"


void clear_il_to_str_output_control_block(
                                    an_il_to_str_output_control_block_ptr octl)
/*
Clear an output control block to default values.
*/
{
  octl->output_str                = NULL;
  octl->output_partial_token_str  = NULL;
  octl->output_name               = NULL;
  octl->output_default_arg        = NULL;
  octl->gen_compilable_code       = FALSE;
  octl->gen_pcc_code              = FALSE;
#if DEBUG
  octl->debug_output              = FALSE;
#endif /* DEBUG */
}  /* clear_il_to_str_output_control_block */


static void output_partial_token_str(
                                    char                                  *str,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Output a the null-terminated string str in the way indicated by octl.
The string may be only part of a token.
*/
{
  an_output_str_function_ptr rout;

  /* See if there's a special routine for partial token output.  If so, use
     it.  If not, use the output_str routine. */
  rout = octl->output_partial_token_str;
  if (rout == NULL) rout = octl->output_str;
  rout(str);
}  /* output_partial_token_str */


static void form_num(long                                  num,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a signed number as indicated by octl.
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%ld", num);
  octl->output_str(buffer);
}  /* form_num */


static void form_unsigned_num(unsigned long                         num,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output an unsigned number as indicated by octl.
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%lu", num);
  octl->output_str(buffer);
}  /* form_unsigned_num */


void form_template_args(a_template_arg_ptr                    tap,
                        an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated template arguments list (e.g., something like
<int, float>) in the way described by octl.  If tap is NULL, nothing
is put out.
*/
{
  if (tap != NULL) {
    octl->output_str("<");
    for (;;) {
      if (tap->is_type) {
        /* Type argument. */
        form_type(tap->variant.type, octl);
      } else {
        /* Nontype argument. */
        form_constant(tap->variant.constant, /*need_parens=*/FALSE, octl);
      }  /* if */
      tap = tap->next;
      /* Stop after the last argument. */
      if (tap == NULL) break;
      /* Put a comma between arguments. */
      octl->output_str(", ");
    }  /* for */
    octl->output_str(">");
  }  /* if */
}  /* form_template_args */


static void form_unqualified_name(
                              char                                  *entry,
                              an_il_entry_kind                      entry_kind,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output the (unqualified) name of the indicated IL entity of the indicated kind.
This includes template arguments on template classes.
*/
{
  a_source_correspondence *scp = (a_source_correspondence *)entry;
  char                    *name = scp->name;

  if (name == NULL) {
    /* For entities without names, use <unnamed>. */
    check_assertion(!octl->gen_compilable_code);
    octl->output_str("<unnamed>");
  } else {
    /* Output the base name. */
    octl->output_str(name);
  }  /* if */
  /* Check for template arguments on a class name. */
  if (il_header.source_language == sl_Cplusplus && entry_kind == iek_type) {
    a_type_ptr  type = (a_type_ptr)entry;
    if (is_immediate_class_type(type)) {
      a_template_arg_ptr tap =
                type->variant.class_struct_union.extra_info->template_arg_list;
      if (tap != NULL) {
        /* This is a template class name.  Put out the template argument
           list, e.g., "<int, float>". */
        form_template_args(tap, octl);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* form_unqualified_name */


void form_class_qualifier(a_type_ptr                            class_type,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output a class qualifier (e.g., "A::B::") that identifies the indicated
class type.  Do the output in the way described by octl.
*/
{
  a_type_ptr parent_class = class_type->source_corresp.class_of_which_a_member;

  /* Use recursion to handle multiple levels of nesting. */
  if (parent_class != NULL) {
    form_class_qualifier(parent_class, octl);
    /* Do the last level. */
    form_unqualified_name((char *)class_type, iek_type, octl);
  } else {
    form_name((char *)class_type, iek_type, octl);
  }  /* if */
  octl->output_str("::");
}  /* form_class_qualifier */


void form_name(char                                  *entry,
               an_il_entry_kind                      kind,
               an_il_to_str_output_control_block_ptr octl)
/*
Output the name of the indicated IL entity of the indicated kind.
If the entity is a class member, generate a qualified name.  Do the
output in the way described by octl.
*/
{
  /* See if there is a routine to do specialized name output. */
  if (octl->output_name != NULL) {
    /* Use the specialized routine. */
    octl->output_name(entry, kind);
  } else {
    /* Default handling. */
    /* This code isn't suitable for generating compilable output. */
    check_assertion_str(!octl->gen_compilable_code,
                        "form_name: doesn't handle compilable output");
    /* If the name is a member of a class in C++, output the class
       qualifier. */
    if (il_header.source_language == sl_Cplusplus) {
      a_type_ptr class_type = ((a_type_ptr)entry)->
                                        source_corresp.class_of_which_a_member;
      if (class_type != NULL) form_class_qualifier(class_type, octl);
    }  /* if */
    /* Output the base name. */
    form_unqualified_name(entry, kind, octl);
  }  /* if */
}  /* form_name */


static void form_tag_kind(a_type_kind                           kind,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output a string that describes the tag kind for the indicated type, i.e.,
"class" or "enum".  Do the output in the way described by octl.
*/
{
  char *str;

  switch (kind) {
    case tk_enum:   str = "enum";   break;
    case tk_class:  str = "class";  break;
    case tk_struct: str = "struct"; break;
    case tk_union:  str = "union";  break;
    default:
#if DEBUG
      if (octl->debug_output) {
        str = "**BAD-TAG-KIND**";
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_tag_kind: bad type kind");
  }  /* switch */
  octl->output_str(str);
}  /* form_tag_kind */


static void form_tag_reference(a_type_ptr                            type,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output a reference to a tag, doing output in the way described by octl.
*/
{
  /* See if there is a routine to do specialized name output. */
  if (octl->output_name != NULL) {
    /* Use the specialized routine. */
    octl->output_name((char *)type, iek_type);
  } else {
    /* Default handling. */
    if (il_header.source_language == sl_C || !has_name(type)) {
      /* In C, put "struct", "union", or "enum" on tags.  In C++, do it
         only for unnamed tags. */
      form_tag_kind(type->kind, octl);
      octl->output_str(" ");
    }  /* if */
    form_name((char *)type, iek_type, octl);
  }  /* if */
}  /* form_tag_reference */


char *int_kind_name(an_integer_kind kind)
/*
Return a string for the name of an integer kind.  Return a string beginning
with "**BAD" for a bad integer kind.
*/
{
  char *p;

  switch (kind) {
    case ik_char:               p = "char";               break;
    case ik_signed_char:        p = "signed char";        break;
    case ik_unsigned_char:      p = "unsigned char";      break;
    case ik_short:              p = "short";              break;
    case ik_unsigned_short:     p = "unsigned short";     break;
    case ik_int:                p = "int";                break;
    case ik_unsigned_int:       p = "unsigned int";       break;
    case ik_long:               p = "long";               break;
    case ik_unsigned_long:      p = "unsigned long";      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:          p = "long long";          break;
    case ik_unsigned_long_long: p = "unsigned long long"; break;
#endif /* LONG_LONG_ALLOWED */
    default:                    p = "**BAD-INT-KIND**";
  }  /* switch */
  return p;
}  /* int_kind_name */


static void form_int_kind_name(an_integer_kind                       kind,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the name of an integer kind, doing the output in the
way described by octl.
*/
{
  char *str;

  if (octl->gen_pcc_code) {
    if (kind == (an_integer_kind)ik_signed_char) {
      /* In pcc mode, "signed" doesn't exist, so this must be a plain
         char. */
      kind = (an_integer_kind)ik_char;
    } else if (kind == (an_integer_kind)ik_unsigned_char &&
               !il_header.plain_chars_are_signed) {
      /* In pcc mode, "char" is turned into signed char or unsigned char.
         If unsigned char is the default, we don't have to say "unsigned". */
      kind = (an_integer_kind)ik_char;
    }  /* if */
  }  /* if */
  str = int_kind_name(kind);
#if CHECKING
  if (*str == '*'
#if DEBUG
      && !octl->debug_output
#endif /* DEBUG */
                            ) {
    internal_error("form_int_kind_name: bad integer kind");
  }  /* if */
#endif /* CHECKING */
  octl->output_str(str);
}  /* form_int_kind_name */


char *float_kind_name(a_float_kind kind)
/*
Return a string for the name of a float kind.  Return a string beginning
with "**BAD" for a bad float kind.
*/
{
  char *p;

  switch (kind) {
    case fk_float:       p = "float";              break;
    case fk_double:      p = "double";             break;
    case fk_long_double: p = "long double";        break;
    default:             p = "**BAD-FLOAT-KIND**";
  }  /* switch */
  return p;
}  /* float_kind_name */


static void form_float_kind_name(a_float_kind                          kind,
                                 an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the name of a float kind, doing the output in the
way described by octl.
*/
{
  char *str;

  str = float_kind_name(kind);
#if CHECKING
  if (*str == '*'
#if DEBUG
      && !octl->debug_output
#endif /* DEBUG */
                            ) {
    internal_error("form_float_kind_name: bad float kind");
  }  /* if */
#endif /* CHECKING */
  octl->output_str(str);
}  /* form_float_kind_name */

#ifdef CFE

static void form_type_qualifier(a_type_ptr                            type,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the type qualifier for the top type of the given type
(i.e., just the first level).  The type must be a tk_typeref containing a
type qualifier.  Do the output in the way described by octl.
*/
{
  a_boolean previous_qualifier = FALSE;

  check_assertion_str(type->kind == (a_type_kind)tk_typeref,
                      "form_type_qualifier: bad type kind");
  if (type->variant.typeref.is_const) {
    octl->output_str("const");
    previous_qualifier = TRUE;
  }  /* if */
  if (type->variant.typeref.is_volatile) {
    if (previous_qualifier) octl->output_str(" ");
    octl->output_str("volatile");
  }  /* if */
}  /* form_type_qualifier */

#endif /* ifdef CFE */
#ifdef FFE

static void form_bound(a_bound_info_entry_ptr               biptr,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated dimension bound information entry in the way indicated
by octl.
*/
{
  char *str;

  switch (biptr->kind) {
    case bk_error:
      str = "<error>";
      break;
    case bk_constant:
      form_num((long)biptr->variant.constant_bound, octl);
      goto end_of_routine;
    case bk_adjustable:
      str = "<adjustable>";
      break;
    case bk_assumed:
      str = "*";
      break;
    case bk_unknown_adjustable:
      str = "<unknown-adjustable>";
      break;
    default:
#if DEBUG
      if (octl->debug_output) {
        str = "**BAD-BOUND-KIND**";
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_bound: bad bound kind");
  }  /* switch */
  octl->output_str(str);
end_of_routine:;
}  /* form_bound */

#endif /* ifdef FFE */

static void form_type_specifier(a_type_ptr                            type,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a type specifier.  Do the output in the way described
by octl.  Note that derived types should be handled above this level.
*/
{
  switch (type->kind) {
    case tk_error:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<error-type>");
      break;
    case tk_void:
      octl->output_str("void");
      break;
    case tk_integer:
#ifdef CFE
      if (type->variant.integer.enum_type) {
        /* Enum type, which is handled specially. */
        form_tag_reference(type, octl);
      } else
#endif /* ifdef CFE */
      {
        /* Normal integer type. */
#ifdef CFE
        if (type->variant.integer.explicitly_signed) {
          octl->output_str("signed ");
        }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
        if (type->variant.integer.logical_type) {
          octl->output_str("logical ");
        }  /* if */
#endif /* ifdef FFE */
        form_int_kind_name(type->variant.integer.int_kind, octl);
      }  /* if */
      break;
    case tk_float:
      form_float_kind_name(type->variant.float_kind, octl);
      break;
#ifdef CFE
    case tk_class:
    case tk_struct:
    case tk_union:
      form_tag_reference(type, octl);
      break;
    case tk_typeref:
      if (is_immediate_type_qualifier(type)) {
        /* The top type is a type qualifier.  Output it and move on to the
           underlying type. */
        form_type_qualifier(type, octl);
        octl->output_str(" ");
        form_type_specifier(type->variant.typeref.type, octl);
      } else if (!has_name(type)) {
        /* This is an internally generated typeref, so just output the
           underlying type. */
        form_type_specifier(type->variant.typeref.type, octl);
      } else {
        /* A typedef; output its name. */
        form_name((char *)type, iek_type, octl);
      }  /* if */
      break;
    case tk_template_param:
      form_name((char *)type, iek_type, octl);
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      octl->output_str("character*");
      if (type->variant.fcharacter.star_star) {
        octl->output_str("(*)");
      } else {
        form_unsigned_num((unsigned long)type->variant.fcharacter.length,
                          octl);
      }  /* if */
      break;
    case tk_hollerith:
      octl->output_str("hollerith*");
      form_unsigned_num((unsigned long)type->variant.hollerith_length, octl);
      break;
    case tk_farray:
      form_type_specifier(type->variant.farray.element_type, octl);
      octl->output_str(" array(");
      for (i = 0; i < type->variant.farray.number_of_dimensions; i++) {
        a_bound_info_entry_ptr bound_info = type->variant.farray.bound_info;
        if (i > 0) octl->output_str(", ");
        form_bound(&bound_info[i], octl);
        octl->output_str(":");
        form_bound(&bound_info[i+type->variant.farray.number_of_dimensions],
                   octl);
      }  /* for */
      octl->output_str(")");
      break;
    case tk_complex:
      form_float_kind_name(type->variant.float_kind, octl);
      octl->output_str(" complex");
      break;
    case tk_stmt_label:
      octl->output_str("<stmt-label>");
      break;
    case tk_format:
      octl->output_str("<format>");
      break;
    case tk_association:
      octl->output_str("association of size ");
      form_unsigned_num((unsigned long)type->size, octl);
      break;
    case tk_unspec_routine:
      octl->output_str("<unspec-routine>");
      break;
    case tk_blockdata:
      octl->output_str("<blockdata>");
      break;
#endif /* ifdef FFE */
    case tk_unknown:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<unknown-type>");
      break;
    default:
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**BAD-TYPE-KIND**");
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_type_specifier: bad type kind");
  }  /* switch */
}  /* form_type_specifier */

#ifdef CFE

static void form_pointer_type_qualifiers(
                               a_type_ptr                            qual_type,
                               a_type_ptr                            type,
                               a_boolean                             add_const,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output type qualifiers, if any, to follow a pointer "*", reference "&",
or pointer-to-member "name::*".  qual_type is the full pointer type,
and type is the unqualified version of that type (e.g., the tk_pointer
entry).  If add_const is TRUE, add an extra "const".  Do the output in
the way described by octl.
*/
{
  for (; qual_type != type; qual_type = qual_type->variant.typeref.type) {
    /* Put out a type qualifier. */
    form_type_qualifier(qual_type, octl);
    octl->output_str(" ");
  }  /* for */
  if (add_const) octl->output_str("const ");
}  /* form_pointer_type_qualifiers */

#endif /* ifdef CFE */

void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_boolean                             add_const,
                    an_il_to_str_output_control_block_ptr octl)
/*
For the indicated type, output the specifiers and the part of the declarator
that precedes the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
If need_trailing_space is TRUE, put a space at the end of the specifiers
part (needed if the declarator part is not empty, because it contains a
name or a derived type).  If add_const is TRUE, add an extra "const" on
top of the type.  Do the output in the way described by octl.
*/
{
  a_type_kind kind;
  a_type_ptr  qual_type;

  qual_type = type;
#ifdef CFE
  /* Remove type qualifiers but not typedefs. */
  while (is_immediate_type_qualifier(type)) type = type->variant.typeref.type;
#endif /* ifdef CFE */
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    form_type_first_part(type->variant.pointer.type,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         /*add_const=*/FALSE,
                         octl);
    /* Output "*" or "&" for pointer or reference. */
#ifdef CFE
    if (type->variant.pointer.is_reference) {
      octl->output_str("&");
    } else {
#endif /* ifdef CFE */
      octl->output_str("*");
#ifdef CFE
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    form_pointer_type_qualifiers(qual_type, type, add_const, octl);
#endif /* ifdef CFE */
#ifdef CFE
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    form_type_first_part(type->variant.ptr_to_member.type,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         /*add_const=*/FALSE,
                         octl);
    /* Output Classname::*. */
    form_name((char *)type->variant.ptr_to_member.class_of_which_a_member,
              iek_type, octl);
    octl->output_str("::*");
    /* Output the type qualifiers on the pointer, if any. */
    form_pointer_type_qualifiers(qual_type, type, add_const, octl);
#endif /* ifdef CFE */
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* A qualifier on a function type shouldn't be possible without a
       typedef. */
    check_assertion_str(qual_type == type,
                        "form_type_first_part: qualifier on function type");
    form_type_first_part(type->variant.routine.return_type,
                         /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/TRUE,
                         /*add_const=*/FALSE,
                         octl);
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) octl->output_str("(");
#ifdef CFE
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    /* A qualifier on an array type shouldn't be possible, period. */
    check_assertion_str(qual_type == type,
                        "form_type_first_part: qualifier on array type");
    form_type_first_part(type->variant.array.element_type,
                         /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/TRUE,
                         /*add_const=*/FALSE,
                         octl);
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) octl->output_str("(");
#endif /* ifdef CFE */
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    if (add_const) octl->output_str("const ");
    form_type_specifier(qual_type, octl);
    if (need_trailing_space) octl->output_str(" ");
  }  /* if */
}  /* form_type_first_part */


void form_function_declarator(a_type_ptr                            type,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output a function declarator for the indicated routine type.  Do the output
in the way described by octl.
*/
{
  a_routine_type_supplement_ptr rtsp = type->variant.routine.extra_info;
  a_param_type_ptr              param;

  octl->output_str("(");
  if ((!rtsp->prototyped || rtsp->old_style_params_scanned) &&
      (il_header.source_language != sl_Cplusplus ||
       octl->gen_compilable_code)) {
    /* Unprototyped function.  Put out nothing between the parentheses. */
    /* Note that in C++ the parameter types for old-style functions are
       listed when generating human-readable output. */
  } else {
    /* Prototyped list. */
    param = rtsp->param_type_list;
    if (param == NULL) {
      /* The first argument is NULL, so this is a "void" parameter list.
         Write it as void in C, as empty in C++. */
      if (il_header.source_language == sl_C) {
        octl->output_str("void");
      }  /* if */
    } else {
      /* List the parameter types. */
      for (;;) {
        form_type(param->type, octl);
        /* Put out a default argument expression if there is one. */
        if (param->default_arg_expr != NULL) {
          /* Use the callback routine supplied.  If there is no callback
             routine, do not put out the default argument. */
          if (octl->output_default_arg != NULL) {
            octl->output_default_arg(param);
          }  /* if */
        }  /* if */
        param = param->next;
        if (param == NULL) break;
        /* There are more parameters, so output a separator and keep
           looping. */
        octl->output_str(", ");
      }  /* for */
    }  /* if */
    if (rtsp->has_ellipsis) {
      /* There is an ellipsis. */
      /* Separate it from the parameters if there are any. */
      if (rtsp->param_type_list != NULL) octl->output_str(", ");
      octl->output_str("...");
    }  /* if */
  }  /* if */
  octl->output_str(")");
#ifdef CFE
  /* Output a cv-qualifier for a member function, if there is one. */
  if (rtsp->implicit_this_param_type != NULL) {
    a_type_ptr underlying_type =
                               type_pointed_to(rtsp->implicit_this_param_type);
    for (; is_immediate_type_qualifier(underlying_type);
         underlying_type = underlying_type->variant.typeref.type) {
      octl->output_str(" ");
      form_type_qualifier(underlying_type, octl);
    }  /* for */
  }  /* if */
#endif /* ifdef CFE */
}  /* form_function_declarator */


static void form_array_declarator(a_type_ptr                            type,
                                  an_il_to_str_output_control_block_ptr octl)
/*
Output an array declarator for the indicated array type.  Do the output in
the way described by octl.
*/
{
  octl->output_str("[");
  if (type->variant.array.is_variable_size_array) {
    check_assertion(!octl->gen_compilable_code);
    octl->output_str("<variable-sized>");
  } else if (type->variant.array.variant.number_of_elements == 0) {
    /* For unknown-bound arrays, put nothing between the []. */
  } else {
    form_unsigned_num((unsigned long)type->
                                     variant.array.variant.number_of_elements,
                      octl);
  }  /* if */
  octl->output_str("]");
}  /* form_array_declarator */


void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    an_il_to_str_output_control_block_ptr octl)
/*
Output the second part of a type reference, the part of the declarator
that follows the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
Do the output in the way described by octl.
*/
{
  a_type_kind kind;

#ifdef CFE
  /* Remove type qualifiers but not typedefs. */
  while (is_immediate_type_qualifier(type)) type = type->variant.typeref.type;
#endif /* ifdef CFE */
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    form_type_second_part(type->variant.pointer.type,
                          /*under_lhs_declarator=*/TRUE,
                          octl);
#ifdef CFE
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    form_type_second_part(type->variant.ptr_to_member.type,
                          /*under_lhs_declarator=*/TRUE,
                          octl);
#endif /* ifdef CFE */
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) octl->output_str(")");
    form_function_declarator(type, octl);
    form_type_second_part(type->variant.routine.return_type,
                          /*under_lhs_declarator=*/FALSE,
                          octl);
#ifdef CFE
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) octl->output_str(")");
    form_array_declarator(type, octl);
    form_type_second_part(type->variant.array.element_type,
                          /*under_lhs_declarator=*/FALSE,
                          octl);
#endif /* ifdef CFE */
  }  /* if */
}  /* form_type_second_part */


void form_type(a_type_ptr                            type,
               an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a type.  Do the output in the way described by octl.
*/
{
  if (type == NULL) {
    check_assertion(!octl->gen_compilable_code);
    octl->output_str("<null-type>");
  } else {
    /* Write the specifiers and the first part of the declarator. */
    form_type_first_part(type, /*under_lhs_declarator=*/FALSE,
                        /*need_trailing_space=*/FALSE,
                        /*add_const=*/FALSE, octl);
    /* Write the second part of the declarator. */
    form_type_second_part(type, /*under_lhs_declarator=*/FALSE, octl);
  }  /* if */
}  /* form_type */


static void form_cast(a_type_ptr                            type,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output a cast to the indicated type.  Do the output in the way described
by octl.
*/
{
  octl->output_str("(");
  form_type(type, octl);
  octl->output_str(")");
}  /* form_cast */


static void output_optional_open_paren(
                       a_boolean                             *need_parens,
                       a_boolean                             *need_close_paren,
                       an_il_to_str_output_control_block_ptr octl)
/*
Output an opening parenthesis and set *need_close_paren to indicate that
the close parenthesis is needed later.  However, if *need_parens is
FALSE, the parenthesis can be optimized away: don't generate it,
leave *need_close_paren set to FALSE, and set *need_parens to TRUE to
prevent doing the optimization more than once.
*/
{
  if (*need_parens) {
    octl->output_str("(");
    *need_close_paren = TRUE;
  } else {
    /* Suppress the parenthesis. */
    *need_parens = TRUE;
  }  /* if */
}  /* output_optional_open_paren */


static void output_optional_close_paren(
                        a_boolean                             need_close_paren,
                        an_il_to_str_output_control_block_ptr octl)
/*
Output the closing parenthesis that is part of an optional set of
parentheses begun by output_optional_open_paren.  The parenthesis is
output only if need_close_paren is TRUE.
*/
{
  if (need_close_paren) octl->output_str(")");
}  /* output_optional_close_paren */


static void form_integer_constant(
                           a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a string for an integer constant (i.e., a constant with a ck_integer
representation; this includes integers cast to pointer types).
If suppress_cast is TRUE, suppress any cast of the constant to another type.
If need_parens is TRUE, parentheses are placed around the constant
if there's any possibility of precedence confusion.  Do the output in
the way described by octl.
*/
{
  a_boolean       need_cast_close_paren = FALSE;
  a_boolean       need_negative_close_paren = FALSE;
  a_boolean       err, minus_1_trick = FALSE;
  a_constant_ptr  eff_constant = constant;
  a_constant      local_constant;
  a_type_ptr      con_type = skip_typerefs(constant->type);
  a_boolean       integer_type_constant =
                                   (con_type->kind == (a_type_kind)tk_integer);
  an_integer_kind ikind;
  a_boolean       signed_constant = FALSE;

  /* See if the constant is signed. */
  if (integer_type_constant) {
    ikind = con_type->variant.integer.int_kind;
    signed_constant = int_kind_is_signed[(int)ikind];
  } else {
    /* Treat null pointer constants as signed since it doesn't change the
       meaning and looks nicer. */
    if (cmplit_integer_constant(constant, 0L) == 0) signed_constant = TRUE;
  }  /* if */
  if (!suppress_cast &&
      /* If this is an integer value or enumerator constant cast to
         an enum type in C mode, or an integer value cast to an enum
         type in C++ mode (note that real enumerator constants don't
         get here), ... */
      (integer_type_constant &&
         (con_type->variant.integer.enum_type ||
      /* ... or, it's a constant that's shorter than int, ... */
          (int)ikind < (int)ik_int)) ||
      /* ... or, we're generating K&R C and it's an unsigned constant
         (pcc doesn't support unsigned integral constants), ... */
      (!signed_constant && octl->gen_pcc_code)) {
    /* ... then prefix the constant with an explicit cast. */
    output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
    form_cast(constant->type, octl);
  }  /* if */
  if (signed_constant && sign_of_integer_constant(constant) < 0) {
    /* Negative value.  Put in parentheses. */
    output_optional_open_paren(&need_parens, &need_negative_close_paren, octl);
    if (octl->gen_compilable_code) {
      /* Check for cases on two's complement machines where the constant
         cannot be represented as a positive constant preceded by a minus
         sign.  For those cases, use the -INT_MAX-1 trick.  One reason
         we do this is so that the type of the constant is right. */
      local_constant = *constant;
      negate_integer_value(&local_constant.variant.integer_value, &err);
      if (!err &&
          le_max_integer_value_of_kind(&local_constant.variant.integer_value,
                                       /*is_signed=*/TRUE, ikind)) {
        /* The negative of the constant is a legal constant. */
      } else {
        /* The negative of the constant is not legal.  Use the -INT_MAX-1
           trick. */
        minus_1_trick = TRUE;
        local_constant = *constant;
        eff_constant = &local_constant;
        incr_integer_value(&local_constant.variant.integer_value);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Write the literal form of the constant. */
  output_partial_token_str(str_for_integer_constant(eff_constant), octl);
  /* Put out a suffix if needed. */
  /* Unsigned suffix is only valid in ANSI C.  When generating K&R C,
     a prefix cast is used (see above). */
  if (!signed_constant && !octl->gen_pcc_code) {
    /* Unsigned constant. */
    output_partial_token_str("U", octl);
  }  /* if */
  if (integer_type_constant) {
    /* Add length suffixes if appropriate. */
    if (ikind == (an_integer_kind)ik_long           ||
        ikind == (an_integer_kind)ik_unsigned_long) {
      output_partial_token_str("L", octl);
#if LONG_LONG_ALLOWED
   } else if (ikind == (an_integer_kind)ik_long_long ||
              ikind == (an_integer_kind)ik_unsigned_long_long) {
      output_partial_token_str("LL", octl);
#endif /* LONG_LONG_ALLOWED */
    }  /* if */
  }  /* if */
  if (minus_1_trick) octl->output_str("-1");
  output_optional_close_paren(need_negative_close_paren, octl);
  output_optional_close_paren(need_cast_close_paren, octl);
}  /* form_integer_constant */


static void form_char(char                                  ch,
                      an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated character as part of a string literal or character
constant.  Handle unprintable characters and necessary escapes.  Do the
output in the way described by octl.
*/
{
  char buffer[10];
  char *bptr = buffer;

  if (isprint((unsigned char)ch)
#ifdef sun
    /* The Sun cc (4.1.2) in -O mode when outputting assembly language
       has a bug that transforms quote into accent grave.  Avoid it. */
      && ch != '\''
#endif /* ifdef sun */
                 ) {
    /* Escape some characters, e.g., quotes. */
    if (ch == '"' || ch == '\'' || ch == '\\') *bptr++ = '\\';
    *bptr++ = ch;
    *bptr = '\0';
  } else {
    char c = 0;
    /* Look for unprintable characters with specific escape codes. */
    switch (ch) {
                                  /* pcc does not recognize \a. */
      case TARG_ALERT_CHAR:       if (!octl->gen_pcc_code) c = 'a';
                                  break;
      case TARG_BACKSPACE_CHAR:   c = 'b'; break;
      case TARG_FORM_FEED_CHAR:   c = 'f'; break;
      case TARG_NEWLINE_CHAR:     c = 'n'; break;
      case TARG_CARR_RETURN_CHAR: c = 'r'; break;
      case TARG_HORIZ_TAB_CHAR:   c = 't'; break;
      case TARG_VERT_TAB_CHAR:    c = 'v'; break;
      /* Default case: no escape sequence is defined. */
      default:                    break;
    }  /* switch */
    if (c != 0) {
      /* Use a defined escape code. */
      buffer[0] = '\\';
      buffer[1] = c;
      buffer[2] = '\0';
    } else {
      /* Use the \nnn form for other unprintable characters. */
      (void)sprintf(buffer, "\\%03o",
                    (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
    }  /* if */
  }  /* if */
  /* Output the character. */
  output_partial_token_str(buffer, octl);
}  /* form_char */


static void form_pm_base_casts(a_derivation_step_ptr                path,
                              a_type_ptr                            pm_type,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output casts to cast a pointer-to-member constant to a pointer-to-member
of a base class (this requires an explicit cast).  path is the derivation
path to the base class.  pm_type is the pointer-to-member type we want
to end up with.  Do the output in the way described by octl.
*/
{
  a_type temp_type;

  check_assertion(pm_type->kind == (a_type_kind)tk_ptr_to_member);
  /* Generate a cast for each derivation step in case there are ambiguities
     etc.  They have to be put out in reverse order, since the last cast
     comes first in the source. */
  if (path->next == NULL) {
    /* Don't do the last step, since it is done at the top of form_constant
       because the implicit_cast flag is set. */
  } else {
    /* Use a recursive call to process all the steps after the first one. */
    form_pm_base_casts(path->next, pm_type, octl);
    /* Make a temporary pointer-to-member type with the right class type by
       modifying a copy of the pm_type. */
    temp_type = *pm_type;
    temp_type.variant.ptr_to_member.class_of_which_a_member =
                                                        path->base_class->type;
    /* Generate the cast for the first step. */
    form_cast(&temp_type, octl);
  }  /* for */
}  /* form_pm_base_casts */


static void form_pm_derived_casts(
                                 a_derivation_step_ptr                 path,
                                 a_type_ptr                            pm_type,
                                 an_il_to_str_output_control_block_ptr octl)
/*
Output casts to cast a pointer-to-member constant to a pointer-to-member
of a derived class.  path is the derivation to the base class.  pm_type
is the pointer-to-member type we want to end up with.  Do the output in
the way described by octl.
*/
{
  a_type temp_type;

  check_assertion(pm_type->kind == (a_type_kind)tk_ptr_to_member);
  /* Generate a cast for each derivation step in case there are ambiguities
     etc.  The derivation is in reverse order, but we want to put it
     out in reverse order because the last cast comes first in the source,
     so a simple loop works right. */
  /* Don't do the last step, since it is done at the top of form_constant
     because the implicit_cast flag is set. */
  for (; path->next != NULL; path = path->next) {
    /* Make a temporary pointer-to-member type with the right class type by
       modifying the pm_type. */
    /* Make a temporary pointer-to-member type with the right class type by
       modifying a copy of the pm_type. */
    temp_type = *pm_type;
    temp_type.variant.ptr_to_member.class_of_which_a_member =
                                                        path->base_class->type;
    /* Generate the cast for the first step. */
    form_cast(&temp_type, octl);
  }  /* for */
}  /* form_pm_derived_casts */


static void form_pm_constant(
                           a_constant_ptr                        constant,
                           a_boolean                             minimal_casts,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a pointer-to-member constant.  If minimal_casts is TRUE, suppress
any unnecessary casts in the generated form of the constant (casts that
serve just to disambiguate).  If need_parens is TRUE, parentheses are
placed around the constant if there's any possibility of precedence confusion.
Do the output in the way described by octl.
*/
{
  a_type_ptr              orig_type = constant->type;
  a_type_ptr              con_type = skip_typerefs(orig_type);
  char                    *entry = NULL;
  a_boolean               need_cast_close_paren = FALSE;
  an_il_entry_kind        entry_kind;
  a_base_class_ptr        bcp =
                            constant->variant.ptr_to_member.casting_base_class;

  /* See if this is a pointer to data member or pointer to member function. */
  if (constant->variant.ptr_to_member.is_function_ptr) {
    a_routine_ptr rout = constant->variant.ptr_to_member.variant.routine;
    if (rout != NULL) entry = (char *)rout;
    entry_kind = iek_routine;
  } else {
    a_field_ptr field = constant->variant.ptr_to_member.variant.field;
    if (field != NULL) entry = (char *)field;
    entry_kind = iek_field;
  }  /* if */
  /* If the constant is implicitly cast to another type, ... */
  if (constant->implicit_cast) {
    /* ... then prefix the constant with an explicit cast. */
    /* Do not put out the cast if it's not needed and minimal_casts is
       TRUE. */
    if (!minimal_casts || constant->variant.ptr_to_member.cast_to_base ||
        entry == NULL) {
      output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
      form_cast(orig_type, octl);
    }  /* if */
  }  /* if */
  if (entry == NULL) {
    /* A null pointer-to-member.  implicit_cast will be TRUE, so a cast
       to the right type has been put out above. */
    octl->output_str("0");
  } else {
    /* A non-null pointer-to-member. */
    a_boolean need_pm_close_paren = FALSE;
    output_optional_open_paren(&need_parens, &need_pm_close_paren, octl);
    if (!minimal_casts && bcp != NULL) {
      /* The pointer-to-member has been cast to another class.  Put in
         proper casts.  Note that implicit_cast will be set and therefore
         the final cast has already been issued above. */
      if (bcp->is_virtual) {
        /* For virtual base classes, the single cast generated above is
           enough.  In fact, we don't want to choose among the possible
           paths to the virtual base class if there are several. */
      } else {
        a_derivation_step_ptr path = bcp->derivation->path;
        /* Cast to the proper result type. */
        if (constant->variant.ptr_to_member.cast_to_base) {
          form_pm_base_casts(path, con_type, octl);
        } else {
          form_pm_derived_casts(path, con_type, octl);
        }  /* if */
      }  /* if */
    }  /* if */
    octl->output_str("&");
    form_name(entry, entry_kind, octl);
    output_optional_close_paren(need_pm_close_paren, octl);
  }  /* if */
  output_optional_close_paren(need_cast_close_paren, octl);
}  /* form_pm_constant */


static void form_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             do_indirection,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the value of a ck_address constant.  If do_indirection is TRUE,
do one level of indirection (i.e., remove the "&"); that's used for reference
initializations.  If need_parens is TRUE, parentheses are placed around the
constant if there's any possibility of precedence confusion.  Do the output
in the way described by octl.
*/
{
  a_boolean        need_second_ptr_close_paren = FALSE, need_scaling_cast;
  a_boolean        need_ptr_cast, need_ptr_cast_close_paren = FALSE;
  a_boolean        need_ampersand, need_offset_close_paren = FALSE;
  a_boolean        need_ampersand_close_paren = FALSE;
  a_type_ptr       orig_type = constant->type, underlying_object_type;
  a_type_ptr       con_type;
  a_targ_ptrdiff_t offset;

  con_type = skip_typerefs(orig_type);
  /* We need a cast to the result type if the constant is implicitly
     cast to another type (but we may be able to optimize it away). */
  need_ptr_cast = constant->implicit_cast;
  need_scaling_cast = FALSE;
  /* Extract the underlying type. */
  need_ampersand = TRUE;
  switch (constant->variant.address.kind) {
    case abk_routine:
      underlying_object_type = constant->variant.address.variant.routine->type;
      /* Exploit the implicit decay to pointer. */
      if (!do_indirection) need_ampersand = FALSE;
      break;
    case abk_variable:
      underlying_object_type= constant->variant.address.variant.variable->type;
      break;
    case abk_constant:
      underlying_object_type= constant->variant.address.variant.constant->type;
      break;
    default:
      unexpected_condition_str(
                              "form_address_constant: bad addr constant kind");
  }  /* switch */
  underlying_object_type = skip_typerefs(underlying_object_type);
  if (underlying_object_type->kind == (a_type_kind)tk_array &&
      !do_indirection) {
    /* For an array, we may be able to exploit the implicit decay to
       pointer to avoid the "&". */
    /* If the constant is the address of the array instead of a pointer
       to the first element, favor the "&x" notation.  Don't do that for
       pcc mode, though, because pcc gives warnings on that and uses
       the pointer-to-element type anyway. */
    if (!constant->implicit_cast && !octl->gen_pcc_code) {
      /* Keep the ampersand. */
    } else {
      /* Exploit the implicit decay to pointer.
         This is particularly helpful in cases where the underlying
         variable is something like
           struct _iobuf x[];
         for which the array has zero size but the element size is
         known. */
      need_ampersand = FALSE;
      underlying_object_type =
                            underlying_object_type->variant.array.element_type;
      /* If the constant type desired is exactly the type that results from
         the type decay, we don't need a cast.  Otherwise, we do. */
      need_ptr_cast = TRUE;
      if (orig_type->kind == (a_type_kind)tk_pointer) {
        if (orig_type->variant.pointer.type == underlying_object_type) {
          need_ptr_cast = FALSE;
        }  /* if */
      }  /* if */
      underlying_object_type = skip_typerefs(underlying_object_type);
    }  /* if */
  }  /* if */
  /* Look at the offset. */
  offset = constant->variant.address.offset;
  if (offset != 0) {
    a_targ_size_t underlying_object_size = underlying_object_type->size;
    /* Non-zero offset.  Deal with scaling issues. */
    /* See if the size of the underlying object is such that scaling
       can be done implicitly instead of playing tricks with casting
       to "char *" and back. */
    if (underlying_object_size != 0 &&
        (offset % (a_targ_ptrdiff_t)underlying_object_size) == 0) {
      /* The offset is divisible by the size of the object, so adjust
         the offset to the proper units. */
      offset /= (a_targ_ptrdiff_t)underlying_object_size;
    } else {
      /* The offset is not evenly divisible by the object size, so
         we need to cast to "char *" and back again. */
      need_scaling_cast = TRUE;
      need_ptr_cast = TRUE;  /* To get cast back. */
    }  /* if */
  }  /* if */
  if (need_ptr_cast) {
    /* Start with a cast to the desired result type. */
    output_optional_open_paren(&need_parens, &need_ptr_cast_close_paren, octl);
    form_cast(orig_type, octl);
    /* Look for cases where a pointer is implicitly cast to a strange type
       (e.g., "char").  The original code probably did this conversion
       as two casts, but the implicit_cast mechanism only retains
       information on the final type.  In such cases, go by way of a
       cast to unsigned long. */
    if (is_pointer_type(con_type) ||
        (is_integral_type(con_type) &&
         con_type->size >= targ_sizeof_pointer)) {
      /* Okay. */
    } else {
      need_second_ptr_close_paren = TRUE;
      octl->output_str("((unsigned long)");
    }  /* if */
  }  /* if */
  if (offset != 0) {
    output_optional_open_paren(&need_parens, &need_offset_close_paren, octl);
    if (need_scaling_cast) {
      /* Need a cast to "char *" to get the offset scaling right. */
      octl->output_str("(char *)");
    }  /* if */
  }  /* if */
  if (do_indirection) {
    /* Do one level of indirection, i.e., remove the "&". */
    need_ampersand = FALSE;
  }  /* if */
  /* If using an ampersand, surround the name with parentheses to avoid
     precedence problems. */
  if (need_ampersand) {
    output_optional_open_paren(&need_parens, &need_ampersand_close_paren,
                               octl);
    octl->output_str("&");
  }  /* if */
  switch (constant->variant.address.kind) {
    case abk_routine:
      form_name((char *)constant->variant.address.variant.routine,
                iek_routine, octl);
      break;
    case abk_variable:
      form_name((char *)constant->variant.address.variant.variable,
                iek_variable, octl);
      break;
    case abk_constant:
      /* Address of a constant, specifically a string. */
      check_assertion_str(constant->variant.address.variant.constant->kind
                                            == (a_constant_repr_kind)ck_string,
                          "form_address_constant: address of nonstring con");
      form_constant(constant->variant.address.variant.constant,
                    /*need_parens=*/FALSE, octl);
      break;
    default:
      unexpected_condition_str(
                              "form_address_constant: bad addr constant kind");
  }  /* switch */
  output_optional_close_paren(need_ampersand_close_paren, octl);
  if (offset != 0) {
    /* Add in the (signed) offset. */
    if (offset >= 0) {
      octl->output_str(" + ");
    } else {
      /* For negative numbers, the sign on the number will be the operator. */
      octl->output_str(" ");
    }  /* if */
    form_num((long)offset, octl);
    output_optional_close_paren(need_offset_close_paren, octl);
  }  /* if */
  output_optional_close_paren(need_second_ptr_close_paren, octl);
  output_optional_close_paren(need_ptr_cast_close_paren, octl);
}  /* form_address_constant */


static a_boolean is_wide_string_constant(a_constant_ptr constant)
/*
Return TRUE if the indicated string is a wide string constant (L"abc").
*/
{
  a_boolean  is_wide_string = FALSE;
  a_type_ptr con_type, elem_type;

  if (constant->kind == (a_constant_repr_kind)ck_string) {
    con_type = skip_typerefs(constant->type);
    elem_type = con_type->variant.array.element_type;
    elem_type = skip_typerefs(elem_type);
    /* Check for element type that is not some variety of char. */
    is_wide_string = !is_character_type(elem_type);
  }  /* if */
  return is_wide_string;
}  /* is_wide_string_constant */


void form_constant(a_constant_ptr                        constant,
                   a_boolean                             need_parens,
                   an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated constant.  If need_parens is TRUE, parentheses are
placed around the constant if there's any possibility of precedence
confusion.  Do the output in the way described by octl.
*/
{
  a_constant_repr_kind kind = constant->kind;
  a_float_kind         fkind;
  a_type_ptr           con_type = NULL, orig_type;
  a_boolean            need_cast_close_paren = FALSE;

  orig_type = constant->type;
  /* Watch out for constants (like aggregates) that have no type. */
  if (orig_type != NULL) {
    con_type = skip_typerefs(orig_type);
    /* See if we need a cast to the constant result type. */
    if (kind == (a_constant_repr_kind)ck_address ||
        kind == (a_constant_repr_kind)ck_ptr_to_member) {
      /* Don't do this here for address constants or pointer-to-member
         constants (they're handled in the subroutines). */
    } else {
      /* If the constant is implicitly cast to another type, ... */
      if (constant->implicit_cast) {
        /* ... then prefix the constant with an explicit cast. */
        output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
        form_cast(orig_type, octl);
      }  /* if */
    }  /* if */
  }  /* if */
  switch (kind) {
    case ck_error:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<error-constant>");
      break;
    case ck_integer:
      if (!octl->gen_pcc_code && is_enum_constant(constant)) {
        /* An enum constant. */
        form_name((char *)constant, iek_constant, octl);
      } else if (il_header.source_language == sl_Cplusplus &&
                 is_character_type(con_type)) {
        /* In C++, character constants have char type. */
        a_boolean       ovflo, need_char_cast_close_paren = FALSE;
        an_integer_kind ikind = con_type->variant.integer.int_kind;
        /* Use a cast if the constant is signed or unsigned, e.g.,
           (unsigned char)'a'. */
        if (ikind == (an_integer_kind)ik_signed_char ||
            ikind == (an_integer_kind)ik_unsigned_char) {
          output_optional_open_paren(&need_parens,
                                     &need_char_cast_close_paren,
                                     octl);
          form_cast(orig_type, octl);
        }  /* if */
        output_partial_token_str("'", octl);
        form_char((char)value_of_integer_constant(constant, &ovflo), octl);
        output_partial_token_str("'", octl);
        output_optional_close_paren(need_char_cast_close_paren, octl);
      } else {
        /* A normal integer constant. */
        form_integer_constant(constant, /*suppress_cast=*/FALSE, need_parens,
                              octl);
      }  /* if */
      break;
    case ck_string:
      /* String constant. */
      { a_targ_size_t a;
        char          ch;
        char          *str = constant->variant.string.value;
        a_targ_size_t len = constant->variant.string.length;

        if (is_wide_string_constant(constant)) {
          /* Wide string literal, e.g., L"abc". */
          /* The processing here must invert the processing done in
             conv_single_wide_char.  Do something that's right for the default
             (simple-minded) implementation, which maps one input character
             to one wide character. */
          output_partial_token_str("L\"", octl);
          for (a = 0; a < len; a += targ_sizeof_wchar_t) {
            /* When generating output for humans to read, abbreviate
               long strings. */
            if (!octl->gen_compilable_code && a > 20*targ_sizeof_wchar_t &&
                len > 25*targ_sizeof_wchar_t) {
              output_partial_token_str("...", octl);
              break;
            }  /* if */
            if (targ_little_endian) {
              ch = str[a];
            } else {
              ch = str[a + targ_sizeof_wchar_t - 1];
            }  /* if */
            /* Suppress the last character if it is a null. */
            if (a != (len - targ_sizeof_wchar_t) || ch != '\0') {
              form_char(ch, octl);
            }  /* if */
          }  /* for */
          output_partial_token_str("\"", octl);
        } else {
          /* Normal (non-wide) string. */
          output_partial_token_str("\"", octl);
          for (a = 0; a < len; a++) {
            /* When generating output for humans to read, abbreviate
               long strings. */
            if (!octl->gen_compilable_code && a > 20 && len > 25) {
              output_partial_token_str("...", octl);
              break;
            }  /* if */
            ch = str[a];
            /* Suppress the last character if it is a null. */
            if (a != (len - 1) || ch != '\0') {
              form_char(ch, octl);
            }  /* if */
          }  /* for */
          output_partial_token_str("\"", octl);
        }  /* if */
      }
      break;
    case ck_float:
      /* Floating-point constant. */
      /* Put parentheses around the constant in case it's negative. */
      octl->output_str("(");
      fkind = con_type->variant.float_kind;
      if (!octl->gen_pcc_code) {
        /* Output the floating-point constant. */
        output_partial_token_str(
                    fp_to_string(fkind, &constant->variant.float_value), octl);
        /* Add a suffix if necessary. */
        if (fkind == (a_float_kind)fk_float) {
          output_partial_token_str("F", octl);
        } else if (fkind == (a_float_kind)fk_long_double) {
          output_partial_token_str("L", octl);
        }  /* if */
      } else {
        /* Generating K&R C.  Suffixes are not allowed. */
        /* Cast to float if type is float (by default it would be double). */
        if (fkind == (a_float_kind)fk_float) {
          output_partial_token_str("(float)", octl);
        }  /* if */
        /* Output the floating-point constant. */
        output_partial_token_str(
                    fp_to_string(fkind, &constant->variant.float_value), octl);
      }  /* if */
      octl->output_str(")");
      break;
#ifdef FFE
    case ck_complex:
      /* Complex constant. */
      fkind = con_type->variant.float_kind;
      octl->output_str("(");
      octl->output_str(fp_to_string(fkind, &cp->variant.complex_value->real),
      octl->output_str(", ");
      octl->output_str(fp_to_string(fkind, &cp->variant.complex_value->imag),
      octl->output_str(")");
      break;
#endif /* ifdef FFE */
#ifdef CFE
    case ck_address:
      /* Address constant. */
      form_address_constant(constant, /*do_indirection=*/FALSE, need_parens,
                            octl);
      break;
    case ck_ptr_to_member:
      /* Pointer-to-member constant. */
      form_pm_constant(constant, /*minimal_casts=*/!octl->gen_compilable_code,
                       need_parens, octl);
      break;
    case ck_dynamic_init:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<dynamic-init-constant>");
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      octl->output_str("{");
      { a_constant_ptr sub_con = constant->variant.aggregate.first_constant;
        while (sub_con != NULL) {
          form_constant(sub_con, /*need_parens=*/FALSE, octl);
          sub_con = sub_con->next;
          if (sub_con != NULL) octl->output_str(", ");
        }  /* while */
      }
      octl->output_str("}");
      break;
    case ck_init_repeat:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<init-repeat-constant>");
      break;
#ifdef CFE
    case ck_template_param:
      check_assertion(!octl->gen_compilable_code);
      switch (constant->variant.template_param.kind) {
        case tpck_param:
        case tpck_member:
          form_name((char *)constant, iek_constant, octl);
          break;
        case tpck_expression:
          octl->output_str("<template-expr>");
          break;
        default:
          octl->output_str("**BAD-TEMPLATE-PARAM-CONSTANT-KIND**");
      }  /* switch */
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case ck_init_position:
      octl->output_str("<init-position-constant>");
      break;
    case ck_hex_octal:
      octl->output_str("<hex-octal-constant>");
      break;
#endif /* ifdef FFE */
    default:
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**BAD-CONSTANT-KIND**");
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_constant: bad constant kind");
  }  /* switch */
  if (need_cast_close_paren) octl->output_str(")");
}  /* form_constant */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
