/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

il_to_str.c -- Produce an external string-form representation for various
               IL entries.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */


/* Macro that returns TRUE if the Microsoft form of output should
   be used for certain features. */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#define use_microsoft_form() (octl->gen_compilable_code ? \
                                      msvc_is_generated_code_target : \
                                      microsoft_mode)
#else /* !(BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE) */
#define use_microsoft_form() microsoft_mode
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */


void clear_il_to_str_output_control_block(
                                    an_il_to_str_output_control_block_ptr octl)
/*
Clear an output control block to default values.
*/
{
  octl->output_str                = NULL;
  octl->output_partial_token_str  = NULL;
  octl->output_name               = NULL;
  octl->output_template_name      = NULL;
  octl->output_class_qualifier    = NULL;
  octl->output_temp_name          = NULL;
  octl->output_func_declarator    = NULL;
  octl->output_expression         = NULL;
  octl->gen_compilable_code       = FALSE;
  octl->gen_pcc_code              = FALSE;
  octl->suppress_local_typedefs   = FALSE;
  octl->suppress_not_yet_defined_typedefs = FALSE;
  octl->render_c99_bool           = FALSE;
  octl->c_generating_back_end     = FALSE;
#if DEBUG
  octl->debug_output              = FALSE;
#endif /* DEBUG */
  octl->force_qualified_name      = FALSE;
  octl->gen_vla_array_as_asterisk_bound_array = FALSE;
  octl->gen_raw_tab_in_literals = FALSE;
}  /* clear_il_to_str_output_control_block */


static void output_partial_token_str(
                                    char                                  *str,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Output the null-terminated string str in the way indicated by octl.
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

#if BACK_END_IS_C_GEN_BE

static void output_temp_name(char                                  *entry,
                             an_il_to_str_output_control_block_ptr octl)
/*
Output a compiler-generated temporary name based on "entry".  Do this by
using a callback routine provided for this purpose.  Do the output as
indicated by octl.
*/
{
  an_output_temp_name_function_ptr rout;

  rout = octl->output_temp_name;
  check_assertion_str(rout != NULL, "output_temp_name: no routine provided");
  rout(entry);
}  /* output_temp_name */

#endif /* BACK_END_IS_C_GEN_BE */

static void form_num(a_host_large_integer                  num,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a signed number as indicated by octl.
*/
{
  char buffer[50];

  (void)sprintf(buffer, PRINTF_FORMAT_FOR_HOST_LARGE_INTEGER, num);
  octl->output_str(buffer);
}  /* form_num */


static void form_unsigned_num(a_host_large_unsigned                 num,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output an unsigned number as indicated by octl.
*/
{
  char buffer[50];

  (void)sprintf(buffer, PRINTF_FORMAT_FOR_HOST_LARGE_UNSIGNED, num);
  octl->output_str(buffer);
}  /* form_unsigned_num */

#if DEBUG

static void form_unsigned_hex(unsigned long                         num,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output an unsigned number in hexadecimal form, as indicated by octl.
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%lx", num);
  octl->output_str(buffer);
}  /* form_unsigned_hex */

#endif /* DEBUG */

static a_source_correspondence_ptr source_corresp_for_template_param(
                                        a_template_param_coordinate_ptr coord);

static void form_template(a_template_ptr	tp,
                          an_il_to_str_output_control_block_ptr octl)

/*
Output a string for a template name.  Do the output in the way described
by octl.
*/
{
  a_source_correspondence_ptr scp = &tp->source_corresp;
  an_il_entry_kind            kind = iek_template;

  /* See whether the template parameter name is remapped in the current
     context. */
  { a_source_correspondence_ptr new_scp;
    new_scp = source_corresp_for_template_param(&tp->coordinates);
    if (new_scp != NULL) {
      scp = new_scp;
      kind = iek_template_parameter;
    }  /* if */
  }
  /* If there is a special output routine for template names, use that.
     Otherwise go through the normal processing. */
  if (octl->output_template_name != NULL) {
    octl->output_template_name((char *)scp, kind);
  } else {
    form_name(scp, kind, octl);
  }  /* if */
}  /* form_template */


void form_a_template_arg(a_template_arg_ptr                    tap,
                         an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated template argument in the way described by octl.
*/
{
  switch (tap->kind) {
    case tak_type:
      /* Type argument. */
      form_type(tap->variant.type, octl);
      break;
    case tak_nontype:
      /* Nontype argument. */
      if (tap->is_array_bound_of_unknown_type) {
        /* The template argument is a deduced array bound whose type is not
           yet known (we know its value, but we don't yet know its type). */
        check_assertion(!octl->gen_compilable_code);
        octl->output_str("array-bound=");
        form_unsigned_num((a_host_large_unsigned)tap->variant.integer_value,
                          octl);
      } else {
        a_constant_ptr con = tap->variant.constant;
        if (tap->arg_operand != NULL && con == NULL) {
          /* The template argument is given by an expression operand (front
             end only). */
          check_assertion(!octl->gen_compilable_code);
          octl->output_str(" <expr> ");
        } else {
          check_assertion(con != NULL);
          if (is_reference_type(con->type)) {
            /* A reference parameter.  Display specially -- one level of
               indirection must be removed. */
            form_lvalue_address_constant(con, /*need_parens=*/FALSE, octl);
          } else {
            /* Normal (non-reference) case. */
            form_constant(con, /*need_parens=*/FALSE, octl);
          }  /* if */
        }  /* if */
      }
      break;
    case tak_template:
      /* A template template argument. */
      form_template(tap->variant.templ, octl);
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* form_a_template_arg */


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
    if (octl->gen_compilable_code) {
      /* When generating compilable code, put out a space after the
         opening "<" to avoid an accidental digraph if the first
         argument begins with a "::" global qualifier. */
      octl->output_str(" ");
    }  /* if */
    for (;;) {
      form_a_template_arg(tap, octl);
      tap = tap->next;
      /* Stop after the last argument. */
      if (tap == NULL) break;
      /* Put a comma between arguments. */
      octl->output_str(", ");
    }  /* for */
    octl->output_str(">");
    if (octl->gen_compilable_code) {
      /* When generating compilable code, put out a space after the
         final ">" avoid the possibility of getting ">>" with nested
         template references or with a nontype expression that ends
         with ">". */
      octl->output_str(" ");
    }  /* if */
  }  /* if */
}  /* form_template_args */


static void form_conversion_function_name(
                                    a_routine_ptr                         rout,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Generate the name of the indicated conversion function.  This is done
by generating "operator" followed by the result type.  This may
differ from the name as it appears in the source_corresp.name field
in that it includes typedef names as they appeared in the original
source.
*/
{
  a_type_ptr type = rout->type;

  octl->output_str("operator ");
  type = skip_typerefs(type);
  type = type->variant.routine.return_type;
  form_type(type, octl);
}  /* form_conversion_function_name */


void form_unqualified_name(a_source_correspondence               *scp,
                           an_il_entry_kind                      entry_kind,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output the (unqualified) name of the IL entity whose source correspondence
entry is pointed to by scp.  The IL entry is of the indicated kind.
The output includes template arguments on template classes.
*/
{
  char *name = unmangled_name_of(scp);

  if (name == NULL) {
    /* For entities without names, use <unnamed>. */
    check_assertion(!octl->gen_compilable_code);
    octl->output_str("<unnamed");
#if DEBUG
    if (octl->debug_output) {
      octl->output_str("@");
      form_unsigned_hex((unsigned long)scp, octl);
    }  /* if */
#endif /* DEBUG */
    octl->output_str(">");
  } else if (entry_kind == iek_routine &&
             ((a_routine_ptr)scp)->special_kind ==
                                     (a_special_function_kind)sfk_conversion) {
    /* For conversion functions, generate the routine name from the type
       name, to get original typedefs. */
    form_conversion_function_name((a_routine_ptr)scp, octl);
  } else {
    /* Output the base name. */
    octl->output_str(name);
  }  /* if */
  /* Check for template arguments on a class name. */
  if (il_header.source_language == sl_Cplusplus && entry_kind == iek_type) {
    a_type_ptr type = (a_type_ptr)scp;
    /* Ignore template parameters and classes whose bodies have been
       eliminated. */
    if (is_immediate_class_type(type) &&
        type->variant.class_struct_union.extra_info != NULL) {
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


static void form_namespace_qualifier(
                                    a_namespace_ptr                       nsp,
                                    an_il_to_str_output_control_block_ptr octl)
/*
Output a namespace qualifier (e.g., "N::") that identifies the indicated
namespace.  Do the output in the way described by octl.  Note that the
output_name routine in the control block (if there is one) will not be used
to output any part of the name.  Called only for C++.
*/
{
  a_source_correspondence  *scp = &nsp->source_corresp;

  if (!nsp->is_namespace_alias && scp->parent.namespace_ptr != NULL) {
    /* Use recursion to handle nested namespaces. */
    form_namespace_qualifier(scp->parent.namespace_ptr, octl);
  }  /* if */
  /* Do the last level. */
  form_unqualified_name(scp, iek_namespace, octl);
  octl->output_str("::");
}  /* form_namespace_qualifier */


static void form_class_qualifier(
                              a_type_ptr                            class_type,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output a class qualifier (e.g., "A::B::") that identifies the indicated
class type.  Do the output in the way described by octl.  Called only for C++.
*/
{
  /* Use the special routine if there is one. */
  if (octl->output_class_qualifier != NULL) {
    octl->output_class_qualifier(class_type);
  } else {
    /* Default processing. */
    a_source_correspondence     *scp = &class_type->source_corresp;
    a_class_type_supplement_ptr ctsp;
    a_boolean                   output_base_name = TRUE;

    /* Use recursion to handle multiple levels of nesting. */
    form_class_or_namespace_qualifier((a_boolean)scp->is_class_member,
                                      scp->parent, octl);
    /* Do the last level. */
    /* Ignore anonymous unions. */
    ctsp = class_type->variant.class_struct_union.extra_info;
#if CHECKING || DEBUG
    if (ctsp == NULL) {
      /* Avoid abort on error case where parent is incomplete class, so
         debug output will still come out okay. */
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("<incomplete parent>");
      } else
#endif /* DEBUG */
      {
        unexpected_condition_str("form_class_qualifier: parent has no body");
      }  /* if */
    } else
#endif /* CHECKING || DEBUG */
    /* Do not insert code here. */
    if (ctsp->anonymous_union_kind != (an_anonymous_union_kind)auk_none) {
      output_base_name = FALSE;
    }  /* if */
    if (output_base_name) {
      form_unqualified_name(scp, iek_type, octl);
      octl->output_str("::");
    }  /* if */
  }  /* if */
}  /* form_class_qualifier */


void form_class_or_namespace_qualifier(
                         a_boolean                             is_class_member,
                         a_parent_class_or_namespace           parent,
                         an_il_to_str_output_control_block_ptr octl)
/*
Output a class or namespace qualifier for an entity, if necessary.
is_class_member and parent give the class/namespace membership information
for the entity: if is_class_member is TRUE, the entity is a member of the
class indicated by parent.class_type.  If is_class_member is FALSE, and
parent.namespace_ptr is non-NULL, the entity is a member of a namespace,
and parent.namespace_ptr points to the namespace.  Note that the
output_name routine in the control block (if there is one) will not
be used to output all of the name.  Called only for C++.
*/
{
  if (is_class_member) {
    form_class_qualifier(parent.class_type, octl);
  } else if (parent.namespace_ptr != NULL) {
    form_namespace_qualifier(parent.namespace_ptr, octl);
  }  /* if */
}  /* form_class_or_namespace_qualifier */


void form_name(a_source_correspondence               *scp,
               an_il_entry_kind                      kind,
               an_il_to_str_output_control_block_ptr octl)
/*
Output the name of the IL entity whose source correspondence
entry is pointed to by scp.  The IL entry is of the indicated kind.
If the entity is a class member, generate a qualified name.  Do the
output in the way described by octl.
*/
{
  /* See if there is a routine to do specialized name output. */
  if (octl->output_name != NULL) {
    /* Use the specialized routine. */
    octl->output_name((char *)scp, kind);
  } else {
    /* Default handling. */
    /* This code isn't suitable for generating compilable output. */
    check_assertion_str(!octl->gen_compilable_code,
                        "form_name: doesn't handle compilable output");
    /* If the name is a member of a class or namespace in C++, output the
       qualifier. */
    if (il_header.source_language == sl_Cplusplus) {
      form_class_or_namespace_qualifier((a_boolean)scp->is_class_member,
                                        scp->parent, octl);
    }  /* if */
    /* Output the base name. */
    form_unqualified_name(scp, kind, octl);
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
    form_name(&type->source_corresp, iek_type, octl);
  }  /* if */
}  /* form_tag_reference */


/*ARGSUSED*/ /* <-- for_generated_code is not used in certain cases. */
char *int_kind_name_full(an_integer_kind kind,
                         a_boolean       for_generated_code)
/*
Return a string for the name of an integer kind.  Return a string beginning
with "**BAD" for a bad integer kind.  for_generated_code is TRUE if the
name is intended for use in code generated by the C-generating back end or
C++-generating back end.
*/
{
  char *p;

#if !STANDALONE_UTILITY_PROGRAM
  /* In some modes, plain char is equivalent to signed char.  In such
     modes, output just "char" for the equivalent type. */
  if (kind == plain_char_int_kind) kind = (an_integer_kind)ik_char;
#endif /* !STANDALONE_UTILITY_PROGRAM */
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
    case ik_long_long:          p = "long long";
                                goto common_long_long_processing;
    case ik_unsigned_long_long: p = "unsigned long long";
common_long_long_processing:
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
                                /* When generating code for MSVC++,
                                   use "__int64" for "long long". */
                                if (for_generated_code &&
                                    msvc_is_generated_code_target) {
                                  if (kind == (an_integer_kind)ik_long_long) {
                                    p = "__int64";
                                  } else {
                                    p = "unsigned __int64";
                                  }  /* if */
                                }  /* if */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
                                break;
#endif /* LONG_LONG_ALLOWED */
    default:                    p = "**BAD-INT-KIND**";
  }  /* switch */
  return p;
}  /* int_kind_name_full */


char *int_kind_name(an_integer_kind kind)
/*
Return a string for the name of an integer kind.  This is an interface
to int_kind_name_full for the normal case, i.e., when the name is not
intended for use in code generated by the C-generating back end or
C++-generating back end (for that, see int_kind_name_full).
*/
{
  char *p = int_kind_name_full(kind, /*for_generated_code=*/FALSE);
  return p;
}  /* int_kind_name */


static char *int_type_name_full(a_type_ptr type,
                                a_boolean  for_generated_code)
/*
Return a string for the name of the given integer type.  The standard cases
are delegated to int_kind_name_full, but for intrinsic Microsoft __intN types
(Visual C++ 6.0) the work is done here.  for_generated_code is TRUE if the
name is intended for use in code generated by the C-generating back end or
C++-generating back end.
*/
{
  char             *result;

  check_assertion(type->kind == (a_type_kind)tk_integer);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (type->variant.integer.microsoft_sized_int_type) {
    an_integer_kind  kind = type->variant.integer.int_kind;

    if (kind == targ_int8_int_kind) {
      result = "__int8";
    } else if (kind == targ_unsigned_int8_int_kind) {
      result = "unsigned __int8";
    } else if (kind == targ_int16_int_kind) {
      result = "__int16";
    } else if (kind == targ_unsigned_int16_int_kind) {
      result = "unsigned __int16";
    } else if (kind == targ_int32_int_kind) {
      result = "__int32";
    } else if (kind == targ_unsigned_int32_int_kind) {
      result = "unsigned __int32";
    } else if (kind == targ_int64_int_kind) {
      result = "__int64";
    } else if (kind == targ_unsigned_int64_int_kind) {
      result = "unsigned __int64";
    } else {
      result = "**BAD-SIZED-INT-KIND**";
    }  /* if */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    result = int_kind_name_full(type->variant.integer.int_kind,
                                for_generated_code);
  }  /* if */
  return result;
}  /* int_type_name_full */


char *int_type_name(a_type_ptr type)
/*
Return a string for the name of the given integer type.  This is an interface
to int_type_name_full for the normal case, i.e., when the name is not intended
for use in code generated by the C-generating back end or C++-generating
back end (for that, see int_type_name_full).
*/
{
  char *result = int_type_name_full(type, /*for_generated_code=*/FALSE);
  return result;
}  /* int_type_name */


static void form_int_type_name(a_type_ptr                            type,
                               an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the name of an integer kind, doing the output in the
way described by octl.
*/
{
  char             *str = NULL;
  an_integer_kind  kind = type->variant.integer.int_kind;

  if (octl->gen_pcc_code) {
    if (kind == (an_integer_kind)ik_signed_char) {
      /* In pcc mode, "signed" doesn't exist, so this must be a plain
         char. */
      str = "char";
    } else if (kind == (an_integer_kind)ik_unsigned_char &&
               !il_header.plain_chars_are_signed) {
      /* In pcc mode, "char" is turned into signed char or unsigned char.
         If unsigned char is the default, we don't have to say "unsigned". */
      str = "char";
    }  /* if */
  }  /* if */
  if (kind == (an_integer_kind)ik_unsigned_int && octl->gen_compilable_code
#if MICROSOFT_EXTENSIONS_ALLOWED
      && !type->variant.integer.microsoft_sized_int_type
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                        ) {
    /* When generating compilable code, use "unsigned" instead of
       "unsigned int".  This is necessary when doing vacuous destructors. */
    str = "unsigned";
  } else if (str == NULL) {
    str = int_type_name_full(type, (a_boolean)octl->gen_compilable_code);
  }  /* if */
#if CHECKING
  if (*str == '*'
#if DEBUG
      && !octl->debug_output
#endif /* DEBUG */
                            ) {
    internal_error("form_int_type_name: bad integer kind");
  }  /* if */
#endif /* CHECKING */
  octl->output_str(str);
}  /* form_int_type_name */


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

#if BACK_END_IS_C_GEN_BE
#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
  if (octl->c_generating_back_end) {
    if (kind == (a_float_kind)fk_long_double) {
      /* When generating K&R C from the C-generating back end, put out
         "double" for "long double" and issue a one-time-only warning. */
#if ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
      static a_boolean warning_issued = FALSE;
      if (!warning_issued) {
        pos_warning(ec_double_for_long_double, &null_source_position);
        warning_issued = TRUE;
      }  /* if */
#endif /* ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */
      kind = (a_float_kind)fk_double;
    }  /* if */
  }  /* if */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */
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

void form_type_qualifier(
                     a_type_qualifier_set                  qualifiers,
                     a_boolean                             need_trailing_space,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a string for the type qualifiers in the given qualifier set.
If the qualifier set is empty, put out nothing.  If need_trailing_space
is TRUE, put out a space after the type qualifier (if one is put out).
Do the output in the way described by octl.
*/
{
  a_boolean qualifier_put_out = FALSE;

/* Local macro that determines whether a given qualifier is present,
   and if so outputs the appropriate string. */
#define output_qualifier(flag, string)					\
{									\
  if ((qualifiers & flag) != 0) {					\
    if (qualifier_put_out) octl->output_str(" ");			\
    qualifier_put_out = TRUE;						\
    octl->output_str(string);						\
  }  /* if */								\
}  /* output_qualifier */

  if (octl->gen_pcc_code) {
    /* Qualifiers are suppressed when generating K&R C. */
  } else {
#if BACK_END_IS_C_GEN_BE && SUPPRESS_CONST_IN_GENERATED_C
    /* Suppress "const" in the output of the C-generating back end. */
    if (octl->c_generating_back_end) qualifiers &= ~TQ_CONST;
#endif /* BACK_END_IS_C_GEN_BE && SUPPRESS_CONST_IN_GENERATED_C */
    output_qualifier(TQ_CONST, "const"); /*lint !e774*/
    output_qualifier(TQ_VOLATILE, "volatile");
#if SUPPRESS_RESTRICT_IN_GENERATED_CODE
    /* Suppress "restrict" in generated compilable code. */
    if (octl->gen_compilable_code) qualifiers &= ~TQ_RESTRICT;
#endif /* SUPPRESS_RESTRICT_IN_GENERATED_CODE */
    output_qualifier(TQ_RESTRICT, "restrict");
#if MICROSOFT_EXTENSIONS_ALLOWED
#if SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
    if (octl->gen_compilable_code) {
      /* Suppress "__unaligned" in generated compilable code. */
      qualifiers &= ~TQ_UNALIGNED;
    }  /* if */
#endif /* SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */
    output_qualifier(TQ_UNALIGNED, "__unaligned");
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
#if SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE
    if (octl->gen_compilable_code) {
      /* Suppress "__near" and "__far" in generated compilable code. */
      qualifiers &= ~(TQ_NEAR | TQ_FAR);
    }  /* if */
#endif /* SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE */
    output_qualifier(TQ_NEAR,
                     (char *)(use_microsoft_form() ? "__near" : "near"));
    output_qualifier(TQ_FAR,
                     (char *)(use_microsoft_form() ? "__far" : "far"));
#endif /* NEAR_AND_FAR_ALLOWED */
    /* Put out a trailing space if required. */
    if (need_trailing_space && qualifier_put_out) octl->output_str(" ");
  }  /* if */
#undef output_qualifier
}  /* form_type_qualifier */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void form_calling_convention(
                     a_calling_convention                  calling_convention,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a Microsoft-specific calling convention.
Put out a space after the calling convention (if one is put out).
Do the output in the way described by octl.
*/
{
#if !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE
  /* Put out nothing for the default calling convention. */
  if (calling_convention != (a_calling_convention)cc_default) {
    octl->output_str(calling_convention_names[(int)calling_convention]);
    /* Put out a trailing space. */
    octl->output_str(" ");
  }  /* if */
#endif /* !SUPPRESS_MICROSOFT_KEYWORDS_IN_GENERATED_CODE */
}  /* form_calling_convention */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
      form_num((a_host_large_integer)biptr->variant.constant_bound, octl);
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

/*
Template parameters are primarily characterized by their coordinates: the
template nesting depth at which they are introduced and the position in the
associated template declaration clause.  Such parameters are represented by
a_constant or a_type entries whose source_correspondence entry may not always
contain the correct name, since the name can vary for a given coordinate set.
For example:
   template<int I> struct A {};
   template<int I> struct B { A<I> *a; };  // (1)
   template<int K> struct C { A<K> *a; };  // (2)
Only one type entry represents the type A<#1> in (1) and (2).  The source
correspondence entry for the first argument "#1" will name "I" because that
was the name used at the first point of instantiation of A<#1>.  However,
the C++ generating back (for example) needs to produce a name that is valid in
the context of use.  To achieve this, we establish a mapping from coordinates
to source correspondence entries, and we consult the map prior to emitting
a_type or a_constant entries that stand for template parameters.
*/

typedef struct a_template_param_map_level *a_template_param_map_level_ptr;
typedef struct a_template_param_map_level {
  /* A growable structure mapping (for a certain template nesting depth) the
     position of a template parameter to its current name. */
  a_template_param_list_pos
		max_position;
			/* The largest position ordinal for which a mapping is
			   stored at this depth. */
  a_source_correspondence_ptr
		*source_corresp;
			/* Points to an array of max_position possibly NULL
			   pointers to source correspondence entries. */
} a_template_param_map_level;


static a_template_param_map_level_ptr template_param_map = NULL;
			/* A pointer to the two-level lookup structure. */

static a_template_nesting_depth template_param_map_max_level = 0;
			/* The size of the first level (i.e., the maximum
			   template nesting depth for which a parameter
			   coordinate has been mapped). */

#if BACK_END_IS_CP_GEN_BE

void remap_template_param(a_template_param_coordinate_ptr  coord,
                          a_source_correspondence_ptr      scp)
/*
Associate the given template parameter coordinate with the given source
correspondence entry.
*/
{
  a_template_param_map_level_ptr  level;

  if (coord->depth == 0) {
    /* A parameter of a template template parameter; no remapping needed. */
  } else {
    /* First find/create the appropriate depth/level: */
    if (template_param_map == NULL) {
      template_param_map_max_level = (coord->depth > 5) ? 2*coord->depth : 10;
      template_param_map = (a_template_param_map_level_ptr)alloc_general(
              sizeof(a_template_param_map_level)*template_param_map_max_level);
      memzero(template_param_map,
              sizeof(a_template_param_map_level)*template_param_map_max_level);
    } else if (coord->depth > template_param_map_max_level) {
      a_template_nesting_depth new_max_level = 2*coord->depth;
      template_param_map =
          (a_template_param_map_level_ptr)realloc_general(
               (char*)template_param_map,
               sizeof(a_template_param_map_level)*template_param_map_max_level,
               sizeof(a_template_param_map_level)*new_max_level);
      memzero(&template_param_map[template_param_map_max_level],
              sizeof(a_template_param_map_level)*new_max_level -
              sizeof(a_template_param_map_level)*template_param_map_max_level);
      template_param_map_max_level = new_max_level;
    }  /* if */
    level = &template_param_map[coord->depth-1];
    /* Then add the new mapping at the right position: */
    if (level->max_position == 0) {
      level->max_position = (coord->position > 5) ? 2*coord->position : 10;
      level->source_corresp = (a_source_correspondence_ptr*)alloc_general(
                      sizeof(a_source_correspondence_ptr)*level->max_position);
      memzero(level->source_corresp,
              sizeof(a_source_correspondence_ptr)*level->max_position);
    } else if (coord->position > level->max_position) {
      a_template_param_list_pos new_max_pos = 2*coord->position;
      level->source_corresp = (a_source_correspondence_ptr*)realloc_general(
                       (char*)level->source_corresp,
                       sizeof(a_source_correspondence_ptr)*level->max_position,
                       sizeof(a_source_correspondence_ptr)*new_max_pos);
      memzero(&level->source_corresp[level->max_position],
              sizeof(a_source_correspondence_ptr)*new_max_pos -
                      sizeof(a_source_correspondence_ptr)*level->max_position);
    }  /* if */
    level->source_corresp[coord->position-1] = scp;
  }  /* if */
}  /* remap_template_param */


void unmap_template_param(a_template_param_coordinate_ptr  coord)
/*
Uninstall any mapping for the given template parameter coordinate.  Presumably
this will cause the source correspondence of the a_type or a_constant entry
for the template parameter to be used.
*/
{
  remap_template_param(coord, /*scp=*/NULL);
}  /* unmap_template_param */

#endif /* BACK_END_IS_CP_GEN_BE */

static a_source_correspondence_ptr source_corresp_for_template_param(
                                        a_template_param_coordinate_ptr coord)
/*
Look up the given template parameter coordinates in the template parameter map
to find a source correspondence entry that will produce a meaningful name in
the current context.  Return NULL if the template parameter is not remapped
in the current context.
*/
{
  a_source_correspondence_ptr     result;

  if (template_param_map == NULL ||
      coord->depth > template_param_map_max_level ||
      coord->depth == 0 || /* Template template parameter. */
      coord->position > template_param_map[coord->depth - 1].max_position) {
    result = NULL;
  } else {
    result = template_param_map[coord->depth - 1].
                                          source_corresp[coord->position - 1];
  }  /* if */
  return result;
}  /* source_corresp_for_template_param */


static void form_type_specifier(a_type_ptr                            type,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output a string for a type specifier.  Do the output in the way described
by octl.
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
      /* Enum types are often handled specially. */
      if (type->variant.integer.enum_type &&
          /* Don't generate enums when generating pcc code in the C-generating
             back end. */
          !(octl->c_generating_back_end && octl->gen_pcc_code) &&
          /* Empty enums (valid in C++ but not C) are put out as integers when
             generating ANSI C from the C-generating back end. */
          !(type->variant.integer.enum_info.constant_list == NULL &&
            octl->c_generating_back_end)) {
        /* Output a reference to the enum type. */
        form_tag_reference(type, octl);
      } else if (type->variant.integer.wchar_t_type &&
                 !octl->c_generating_back_end) {
        /* Output a wchar_t type as "wchar_t", except in the C generating
           back end, where it is output as its underlying type. */
        octl->output_str("wchar_t");
      } else if (type->variant.integer.bool_type &&
                 (!octl->c_generating_back_end || octl->render_c99_bool)) {
        /* Output a bool type as "bool", except in the C generating
           back end, where it is output as its underlying type. */
        octl->output_str((char *)(octl->render_c99_bool ? "_Bool" : "bool"));
      } else
#endif /* ifdef CFE */
      {
        /* Normal integer type. */
#ifdef CFE
        if (type->variant.integer.explicitly_signed &&
            /* "signed" is not allowed when generating pcc code. */
            !octl->gen_pcc_code) {
          octl->output_str("signed ");
        }  /* if */
#endif /* ifdef CFE */
#ifdef FFE
        if (type->variant.integer.logical_type) {
          octl->output_str("logical ");
        }  /* if */
#endif /* ifdef FFE */
        form_int_type_name(type, octl);
      }  /* if */
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
      form_float_kind_name(type->variant.float_kind, octl);
      octl->output_str((char *)(type->kind == (a_type_kind)tk_complex ?
                       " _Complex" : " _Imaginary"));
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
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
      /* A typeref here should be a typedef or a typeof operator. */
#if GNU_EXTENSIONS_ALLOWED
      if (type->variant.typeref.is_typeof) {
        octl->output_str("__typeof__(");
        form_type(type->variant.typeref.type, octl);
        octl->output_str(")");
      } else
#endif /* GNU_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        check_assertion_str(typeref_is_typedef(type),
                            "form_type_specifier: typeref is not typedef");
        form_name(&type->source_corresp, iek_type, octl);
      }  /* if */
      break;
    case tk_template_param:
      {
        a_source_correspondence_ptr scp = &type->source_corresp;
        an_il_entry_kind            scp_kind = iek_type;
        /* See whether the template parameter name is remapped in the current
           context. */
        if (type->variant.template_param.kind ==
                                     (a_template_param_type_kind)tptk_param) {
          a_source_correspondence_ptr new_scp;
          new_scp = source_corresp_for_template_param(
                       &type->variant.template_param.extra_info->coordinates);
          if (new_scp != NULL) {
            scp = new_scp;
            scp_kind = iek_template_parameter;
          }  /* if */
        }  /* if */
        form_name(scp, scp_kind, octl);
      }
      break;
#endif /* ifdef CFE */
#ifdef FFE
    case tk_fcharacter:
      octl->output_str("character*");
      if (type->variant.fcharacter.star_star) {
        octl->output_str("(*)");
      } else {
        form_unsigned_num((a_host_large_unsigned)
                                        type->variant.fcharacter.length, octl);
      }  /* if */
      break;
    case tk_hollerith:
      octl->output_str("hollerith*");
      form_unsigned_num((a_host_large_unsigned)
                                         type->variant.hollerith_length, octl);
      break;
    case tk_farray:
      { int i;
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
      }
      break;
#if !C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
      form_float_kind_name(type->variant.float_kind, octl);
      octl->output_str(" complex");
      break;
#endif /* !C99_IL_EXTENSIONS_SUPPORTED */
    case tk_stmt_label:
      octl->output_str("<stmt-label>");
      break;
    case tk_format:
      octl->output_str("<format>");
      break;
    case tk_association:
      octl->output_str("association of size ");
      form_unsigned_num((a_host_large_unsigned)type->size, octl);
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

/*
Return TRUE if the indicated typedef is "invisible" now because (a) it's
local to a function and we're suppressing local typedefs, or
(b) suppress_const is TRUE (we're suppressing top-level "const") and the
typedef contains a const qualifier, or (c) suppress_not_yet_defined_typedefs
is TRUE and the typedef definition has not yet been put out in the
C++-generating back end.
*/
#if BACK_END_IS_CP_GEN_BE
#define or_not_yet_defined_typedef(type) ||                           \
  ((octl)->suppress_not_yet_defined_typedefs &&                       \
   !(type)->typedef_definition_has_been_put_out)
#else /* !BACK_END_IS_CP_GEN_BE */
#define or_not_yet_defined_typedef(type) /* Nothing */
#endif /* BACK_END_IS_CP_GEN_BE */

#define typedef_is_invisible(type, suppress_const, octl)              \
 (((type)->source_corresp.is_local_to_function &&                     \
   (octl)->suppress_local_typedefs) ||                                \
  ((suppress_const) && is_const_qualified_type(type))                 \
  or_not_yet_defined_typedef(type))                                   \


static a_boolean can_use_qualified_array_typedef(
                          a_type_ptr                            *p_type,
                          a_type_qualifier_set                  *p_qualifiers,
                          a_boolean                             suppress_const,
                          an_il_to_str_output_control_block_ptr octl)
/*
If *p_type (an array type) was generated by adding a qualifier to a typedef
for an array type, and the qualified typedef can be used, return TRUE and
update *p_type and *p_qualifiers accordingly.  If suppress_const is TRUE,
we're supposed to suppress top-level "const".  octl is the output control
block, needed because it indicates whether local typedefs are invisible.
*/
{
  a_boolean            can_use_typedef = FALSE;
  a_type_ptr           type = *p_type, unqual_array_type;
  a_type_qualifier_set qualifiers;

  if (is_qualified_version_of_array_typedef(type, &unqual_array_type)) {
    /* This array type was generated by applying a type qualifier to
       a typedef of an array type. */
    if (typedef_is_invisible(unqual_array_type, suppress_const, octl)) {
      /* We can't use this typedef. */
    } else {
      /* We can use the array typedef. */
      /* The qualifiers to be added are those on the type we have here minus
         those that appeared on the element type of the array before
         qualification. */
      a_type_ptr           unqual_array_element_type =
                              underlying_array_element_type(unqual_array_type);
      a_type_qualifier_set unqual_array_qualifiers =
                      get_top_level_type_qualifiers(unqual_array_element_type);
      type = underlying_array_element_type(type);
      qualifiers = get_top_level_type_qualifiers(type);
      *p_qualifiers = qualifiers & ~unqual_array_qualifiers;
      *p_type = unqual_array_type;
      can_use_typedef = TRUE;
    }  /* if */
  }  /* if */
  return can_use_typedef;
}  /* can_use_qualified_array_typedef */

#endif /* ifdef CFE */

void form_type_first_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_boolean                             need_trailing_space,
                    a_type_qualifier_set                  added_qualifiers,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl)
/*
For the indicated type, output the specifiers and the part of the declarator
that precedes the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
If need_trailing_space is TRUE, put a space at the end of the specifiers
part (needed if the declarator part is not empty, because it contains a
name or a derived type).  added_qualifiers contains a set of type qualifiers
to be added on top of the type.  options contains options as bits in a set:
If FTO_SUPPRESS_CONST is TRUE, suppress generation of top-level "const";
if FTO_SUPPRESS_SPECIFIERS is TRUE, suppress generation of the type specifiers
(put out only the declarator).  Do the output in the way described by octl.
*/
{
  a_type_kind kind;
  a_boolean   suppress_const = (options & FTO_SUPPRESS_CONST) != 0;
#ifdef CFE
  a_type_qualifier_set
              qualifiers = TQ_NONE;
#if NEAR_AND_FAR_ALLOWED
  a_type_qualifier_set
              near_and_far_qualifiers;
  a_boolean   near_and_far_need_trailing_space;
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* ifdef CFE */

  if (type == NULL) {
    /* NULL type pointer. */
#if DEBUG
    if (octl->debug_output) {
      octl->output_str("**NULL-TYPE-POINTER**");
      goto end_of_routine;
    }  /* if */
#endif /* DEBUG */
    check_assertion_str(!octl->gen_compilable_code,
                        "form_type_first_part: NULL type");
    octl->output_str("<something>");
    goto end_of_routine;
  }  /* if */
#ifdef CFE
  options &= ~FTO_SUPPRESS_CONST;
  /* Remove type qualifiers but not typedefs.  Also drop typedefs
     that aren't visible here.  Accumulate the type qualifier set. */
  while (type->kind == (a_type_kind)tk_typeref) {
    if (typeref_is_typedef(type)) {
      /* Typedef.  Stop unless it's invisible. */
      if (!typedef_is_invisible(type, suppress_const, octl)) break;
#if GNU_EXTENSIONS_ALLOWED
    } else if (type->variant.typeref.is_typeof) {
      /* GNU C typeof operator: behaves much like a typedef. */
      break;
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
      /* Type qualifier typeref.  Accumulate the qualifiers. */
      qualifiers |= type->variant.typeref.qualifiers;
      /* If we're supposed to suppress "const" and this typeref has it, we
         can take care of the suppression now.  This has to be done inside
         the loop because the "typedef_is_invisible" test uses the
         suppress_const flag. */
      if (suppress_const && (qualifiers & TQ_CONST)) {
        qualifiers &= ~TQ_CONST;
        suppress_const = FALSE;
      }  /* if */
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
  /* Add top-level qualifiers if told to. */
  qualifiers |= added_qualifiers;
#if NEAR_AND_FAR_ALLOWED
  /* Look for any qualifiers (like "near") that are displayed specially. */
  near_and_far_qualifiers = qualifiers & (TQ_NEAR | TQ_FAR);
  if (near_and_far_qualifiers != TQ_NONE) {
    qualifiers -= near_and_far_qualifiers;
    near_and_far_need_trailing_space = need_trailing_space;
    need_trailing_space = TRUE;
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#endif /* ifdef CFE */
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    form_type_first_part(type->variant.pointer.type,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE, options, octl);
#ifdef CFE
    /* Output "*" or "&" for pointer or reference. */
    if (type->variant.pointer.is_reference && !octl->c_generating_back_end) {
      octl->output_str("&");
    } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (type->variant.pointer.base_variable != NULL) {
        /* This is a Microsoft based pointer -- add "__based(var-name) "
           before the asterisk. */
        octl->output_str("__based(");
        form_name(&type->variant.pointer.base_variable->source_corresp,
                  iek_variable, octl);
        octl->output_str(") ");
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifdef CFE */
      octl->output_str("*");
#ifdef CFE
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    if (qualifiers != TQ_NONE) {
      form_type_qualifier(qualifiers, need_trailing_space, octl);
    }  /* if */
#endif /* ifdef CFE */
#ifdef CFE
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    form_type_first_part(type->variant.ptr_to_member.type,
                         /*under_lhs_declarator=*/TRUE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE, options, octl);
    /* Output Classname::*. */
    form_class_qualifier(type->variant.ptr_to_member.class_of_which_a_member,
                         octl);
    /* form_class_qualifier put out "::".  Add the final "*" here.  That's
       okay; it's a separate token. */
    octl->output_str("*");
    /* Output the type qualifiers on the pointer, if any. */
    if (qualifiers != TQ_NONE) {
      form_type_qualifier(qualifiers, need_trailing_space, octl);
    }  /* if */
#endif /* ifdef CFE */
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* A qualifier on a function type shouldn't be possible without a
       typedef. */
    check_assertion_str(qualifiers == TQ_NONE,
                        "form_type_first_part: qualifier on function type");
    form_type_first_part(type->variant.routine.return_type,
                         /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE, options, octl);
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) octl->output_str("(");
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* A calling convention specifier is put out as a left-hand-side
       declarator. */
    form_calling_convention(type->variant.routine.extra_info->
                                                            calling_convention,
                            octl);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#ifdef CFE
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    /* A qualifier on an array type shouldn't be possible, period. */
    check_assertion_str(qualifiers == TQ_NONE,
                        "form_type_first_part: qualifier on array type");
    if (can_use_qualified_array_typedef(&type, &qualifiers, suppress_const,
                                        octl)) {
      /* This array type was generated by applying a type qualifier to
         a typedef of an array type.  Use the typedef. */
      goto handle_specifiers_type;
    }  /* if */
    if (suppress_const) options |= FTO_SUPPRESS_CONST;
    form_type_first_part(type->variant.array.element_type,
                         /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/TRUE,
                         TQ_NONE,
                         options,
                         octl);
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) octl->output_str("(");
#endif /* ifdef CFE */
  } else {
handle_specifiers_type:
    /* No declarator part to process.  Handle the specifier type. */
    if ((options & FTO_SUPPRESS_SPECIFIERS) == 0) {
      if (qualifiers != TQ_NONE) {
        form_type_qualifier(qualifiers, /*need_trailing_space=*/TRUE, octl);
      }  /* if */
      form_type_specifier(type, octl);
      /* Put out a trailing space if required. */
      if (need_trailing_space) octl->output_str(" ");
    }  /* if */
  }  /* if */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_qualifiers != TQ_NONE) {
    /* "near" or "far": display it next to the declarator name. */
    form_type_qualifier(near_and_far_qualifiers,
                        near_and_far_need_trailing_space, octl);
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
end_of_routine:;
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

  /* See if there's a special routine to output function declarators.
     If so, use it. */
  if (octl->output_func_declarator != NULL) {
    octl->output_func_declarator(type);
  } else {
    /* Default processing. */
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
        if (!rtsp->has_ellipsis) {
          /* The first argument is NULL, so this is a "void" parameter list.
             Write it as void in C, as empty in C++. */
          if (il_header.source_language == sl_C) {
            octl->output_str("void");
          }  /* if */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
#if !ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C
        } else if (octl->gen_compilable_code &&
                   (octl->c_generating_back_end ||
                    il_header.source_language == sl_C)) {
          /* For the C-generating back end (or the C++-generating back end
             when it is producing C code), we put out the ellipsis by itself
             only if it can be handled.  Otherwise, "(...)" is rendered by
             "()" in the generated C. */
#endif /* !ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
        } else {
          /* This is a parameter list consisting of only an ellipsis, which
             is standard in C++ and may be accepted as an extension in C
             mode. */
          octl->output_str("...");
        }  /* if */
      } else {
        /* List the parameter types. */
        for (;;) {
          form_type(param->type, octl);
          /* Default argument expressions are not put out. */
          param = param->next;
          if (param == NULL) break;
          /* There are more parameters, so output a separator and keep
             looping. */
          octl->output_str(", ");
        }  /* for */
        /* Put out the ellipsis if there is one. */
        if (rtsp->has_ellipsis) octl->output_str(", ...");
      }  /* if */
    }  /* if */
    octl->output_str(")");
    /* If the function type has a linkage that's not compatible with the
       default, add the linkage string after the closing parenthesis.  This
       is done only for non-compilable code. */
    if (!octl->gen_compilable_code) {
      a_name_linkage_kind linkage = rtsp->routine_name_linkage;
      if (linkage != (a_name_linkage_kind)nlk_internal &&
          linkage != (a_name_linkage_kind)nlk_none &&
          !routine_linkages_are_compatible(linkage,
                                           default_routine_name_linkage,
                                           /*is_impl_conv=*/FALSE)) {
        octl->output_str(" ");
        octl->output_str(name_linkage_kind_names[linkage]);
      }  /* if */
    } /* if */
#ifdef CFE
    /* Output a cv-qualifier for a member function, if there is one. */
    if (rtsp->this_class != NULL) {
      a_type_qualifier_set qualifiers = rtsp->qualifiers;
      if (qualifiers != TQ_NONE) {
        octl->output_str(" ");
        form_type_qualifier(qualifiers, /*need_trailing_space=*/FALSE, octl);
      }  /* if */
    }  /* if */
#endif /* ifdef CFE */
  }  /* if */
}  /* form_function_declarator */


static void form_array_declarator(a_type_ptr                            type,
                                  an_il_to_str_output_control_block_ptr octl)
/*
Output an array declarator for the indicated array type.  Do the output in
the way described by octl.
*/
{
  octl->output_str("[");
  form_type_qualifier(type->variant.array.qualifiers,
                      /*need_trailing_space=*/TRUE, octl);
#if !SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE
  if (type->variant.array.is_static) {
    /* C99 static. */
    octl->output_str("static ");
  }  /* if */
#endif /* !SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE */
  if (type->variant.array.is_vla) {
    /* Variable-length array. */
    if (!type->variant.array.has_assoc_vla_dimension ||
        octl->gen_vla_array_as_asterisk_bound_array) {
      /* Array[*] case. */
      octl->output_str("*");
    } else {
      /* Variable-length array with an associated expression. */
      if (octl->output_expression == NULL) {
        /* No routine to do the expression output.  Do default
           non-compilable output. */
        check_assertion(!octl->gen_compilable_code);
        octl->output_str("<variable>");
      } else {
        /* Output the expression using a special routine. */
        a_vla_dimension_ptr vlap = find_vla_dimension(type);
        octl->output_expression(vlap->dimension_expr);
      }  /* if */
    }  /* if */      
  } else if (type->variant.array.is_variable_size_array) {
    if (octl->output_expression == NULL) {
      /* No routine to do the expression output.  Do default
         non-compilable output. */
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<variable-sized>");
    } else {
      an_expr_node_ptr  count = type->variant.array.variant.element_count_expr;
      octl->output_expression(count);
    }  /* if */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  } else if (type->variant.array.bound_constant != NULL &&
             !octl->c_generating_back_end) {
    /* Use the recorded a_constant entry rather than a plain integer.  This
       allows the output to be closer to the original bound expression when
       the bound is more than just a literal (e.g., "2*2" instead of "4"). */
    form_constant(type->variant.array.bound_constant,
                  /*need_parens=*/FALSE, octl);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  } else if (type->variant.array.is_template_dependent_size_array) {
    form_constant(type->variant.array.variant.element_count_constant,
                  /*need_parens=*/FALSE, octl);
  } else if (type->variant.array.variant.number_of_elements == 0 &&
             !type->variant.array.put_out_unknown_bound_as_zero) {
    /* For unknown-bound arrays, put nothing between the []. */
  } else {
    form_unsigned_num((a_host_large_unsigned)type->
                                     variant.array.variant.number_of_elements,
                      octl);
  }  /* if */
  octl->output_str("]");
}  /* form_array_declarator */


void form_type_second_part(
                    a_type_ptr                            type,
                    a_boolean                             under_lhs_declarator,
                    a_form_type_options_set               options,
                    an_il_to_str_output_control_block_ptr octl)
/*
Output the second part of a type reference, the part of the declarator
that follows the name.  If under_lhs_declarator is TRUE, this type is
directly under a type that uses a left-side declarator, e.g., a pointer type.
(That's used to control use of parentheses around parts of the declarator.)
If options contains FTO_SUPPRESS_CONST, suppress generation of top-level
"const".  Do the output in the way described by octl.
*/
{
  a_type_kind kind;
  a_boolean   suppress_const = (options & FTO_SUPPRESS_CONST) != 0;
#ifdef CFE
  a_type_qualifier_set
              qualifiers = TQ_NONE;
#endif /* ifdef CFE */

  if (type == NULL) {
    /* NULL type pointer.  Handled in form_type_first_part. */
    goto end_of_routine;
  }  /* if */
#ifdef CFE
  options &= ~FTO_SUPPRESS_CONST;
  /* Remove type qualifiers but not typedefs.  Also drop typedefs
     that aren't visible here.  Accumulate the type qualifier set. */
  while (type->kind == (a_type_kind)tk_typeref) {
    if (typeref_is_typedef(type)) {
      /* Typedef.  Stop unless it's invisible. */
      if (!typedef_is_invisible(type, suppress_const, octl)) break;
    } else {
      /* Type qualifier typeref.  Accumulate the qualifiers. */
      qualifiers |= type->variant.typeref.qualifiers;
      /* If we're supposed to suppress "const" and this typeref has it, we
         can take care of the suppression now.  This has to be done inside
         the loop because the "typedef_is_invisible" test uses the
         suppress_const flag. */
      if (suppress_const && (qualifiers & TQ_CONST)) {
        qualifiers &= ~TQ_CONST;
        suppress_const = FALSE;
      }  /* if */
    }  /* if */
    type = type->variant.typeref.type;
  }  /* while */
#endif /* ifdef CFE */
  kind = type->kind;
  if (kind == (a_type_kind)tk_pointer) {
    /* Pointer or reference type. */
    form_type_second_part(type->variant.pointer.type,
                          /*under_lhs_declarator=*/TRUE,
                          options, octl);
#ifdef CFE
  } else if (kind == (a_type_kind)tk_ptr_to_member) {
    /* Pointer-to-member type. */
    form_type_second_part(type->variant.ptr_to_member.type,
                          /*under_lhs_declarator=*/TRUE,
                          options, octl);
#endif /* ifdef CFE */
  } else if (kind == (a_type_kind)tk_routine) {
    /* Function type. */
    /* This is a right-side declarator, so if it's under a left-side
       declarator parentheses are needed. */
    if (under_lhs_declarator) octl->output_str(")");
    form_function_declarator(type, octl);
    form_type_second_part(type->variant.routine.return_type,
                          /*under_lhs_declarator=*/FALSE,
                          options, octl);
#ifdef CFE
  } else if (kind == (a_type_kind)tk_array) {
    /* Array type. */
    if (can_use_qualified_array_typedef(&type, &qualifiers, suppress_const,
                                        octl)) {
      /* This array type was generated by applying a type qualifier to
         a typedef of an array type.  The type was handled as a specifiers
         type. */
    } else {
      /* This is a right-side declarator, so if it's under a left-side
         declarator parentheses are needed. */
      if (under_lhs_declarator) octl->output_str(")");
      form_array_declarator(type, octl);
      if (suppress_const) options |= FTO_SUPPRESS_CONST;
      form_type_second_part(type->variant.array.element_type,
                            /*under_lhs_declarator=*/FALSE,
                            options, octl);
    }  /* if */
#endif /* ifdef CFE */
  }  /* if */
end_of_routine:;
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
    form_type_first_part_simple(type, /*under_lhs_declarator=*/FALSE,
                                /*need_trailing_space=*/FALSE, octl);
    /* Write the second part of the declarator. */
    form_type_second_part_simple(type, /*under_lhs_declarator=*/FALSE, octl);
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


static void form_general_cast(
                     a_type_ptr                            type,
                     a_boolean                             is_reinterpret_cast,
                     an_il_to_str_output_control_block_ptr octl)
/*
Output a cast to the indicated type.  If is_reinterpret_cast is TRUE,
output a reinterpret_cast (note that a closing parenthesis will have to
be output later).  Do the output in the way described by octl.
*/
{
  if (is_reinterpret_cast) {
    octl->output_str("reinterpret_cast<");
    form_type(type, octl);
    octl->output_str(">(");
  } else {
    form_cast(type, octl);
  }  /* if */
}  /* form_general_cast */


static void output_optional_open_paren(
                       a_boolean                             *need_parens,
                       a_boolean                             *need_close_paren,
                       an_il_to_str_output_control_block_ptr octl)
/*
Output an opening parenthesis and set *need_close_paren to indicate that
the closing parenthesis is needed later.  However, if *need_parens is
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


void form_integer_constant(a_constant_ptr                        constant,
                           a_boolean                             suppress_cast,
                           a_boolean                             need_parens,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a string for an integer constant (i.e., a constant with a ck_integer
representation).  The constant is written in integer form even if it has
been cast to another type (e.g., a pointer type); the caller must handle
the implicit cast for that case if appropriate.  If suppress_cast is TRUE,
suppress any cast of the constant to another type.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
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
    if (cmplit_integer_constant(constant, (a_host_large_integer)0) == 0) {
      signed_constant = TRUE;
    }  /* if */
  }  /* if */
  if (!suppress_cast &&
      /* If this is an integer value or enumerator constant cast to
         an enum type in C mode, or an integer value cast to an enum
         type in C++ mode (note that real enumerator constants don't
         get here), ... */
      ((integer_type_constant && con_type->variant.integer.enum_type &&
      /* Don't do this in the C-generating back end when generating K&R C,
         because enum types don't appear. */
        !(octl->c_generating_back_end && octl->gen_pcc_code)) ||
      /* ... or, it's a constant that's shorter than int, ... */
       (integer_type_constant && (int)ikind < (int)ik_int) ||
      /* ... or, we're generating K&R C and it's an unsigned constant
         (pcc doesn't support unsigned integral constants), ... */
        (!signed_constant && octl->gen_pcc_code))) {
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
      if (use_microsoft_form()) {
        output_partial_token_str("i64", octl);
      } else {
        output_partial_token_str("LL", octl);
      }  /* if */
#endif /* LONG_LONG_ALLOWED */
    }  /* if */
  }  /* if */
  if (minus_1_trick) octl->output_str("-1");
  output_optional_close_paren(need_negative_close_paren, octl);
  output_optional_close_paren(need_cast_close_paren, octl);
}  /* form_integer_constant */


int form_char(char                                  ch,
              an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated character as part of a string literal or character
constant.  Handle unprintable characters and necessary escapes.  Do the
output in the way described by octl.  Return the number of characters
output.
*/
{
  char buffer[10];
  char *bptr = buffer;
  int  nchars = 1;

  if ((isprint((unsigned char)ch)
#ifdef sun
    /* The Sun cc (4.1.2) in -O mode when outputting assembly language
       has a bug that transforms quote into accent grave.  Avoid it. */
       && ch != '\''
#endif /* ifdef sun */
                    ) ||
       (ch == '\t' && octl->gen_raw_tab_in_literals)) {
    /* Escape some characters, e.g., quotes. */
    if (ch == '"' || ch == '\'' || ch == '\\' ||
        /* Avoid accidentally putting out trigraphs by escaping "?". */
        (ch == '?' && octl->gen_compilable_code && !octl->gen_pcc_code)) {
      *bptr++ = '\\';
      nchars++;
    }  /* if */
    *bptr++ = ch;
    *bptr = '\0';
  } else {
    char c = 0;
    /* Look for unprintable characters with specific escape codes. */
    switch (ch) {
                                  /* pcc does not recognize \a.  Also, some
                                     SVR4 compilers give a warning on it. */
      case TARG_ALERT_CHAR:       if (!octl->c_generating_back_end &&
                                      !octl->gen_pcc_code) c = 'a';
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
      nchars = 2;
    } else {
      /* Use the \nnn form for other unprintable characters. */
      (void)sprintf(buffer, "\\%03o",
                    (unsigned int)(ch&((1<<targ_host_string_char_bit)-1)));
      nchars = 4;
    }  /* if */
  }  /* if */
  /* Output the character. */
  output_partial_token_str(buffer, octl);
  return nchars;
}  /* form_char */


static int form_wide_char(unsigned long                         wc,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated wide character as part of a string literal or character
constant.  Handle unprintable characters and necessary escapes.  Do the
output in the way described by octl.  Return the number of characters
output.
*/
{
  char buffer[10];

  /* Use hex escapes always to avoid having to convert the wide character
     back to a multibyte character string. */
  (void)sprintf(buffer, "\\x%lx", wc);
  /* Output the character. */
  output_partial_token_str(buffer, octl);
  return strlen(buffer);
}  /* form_wide_char */


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
       modifying a copy of the pm_type. */
    temp_type = *pm_type;
    temp_type.variant.ptr_to_member.class_of_which_a_member =
                                                        path->base_class->type;
    /* Generate the cast for the first step. */
    form_cast(&temp_type, octl);
  }  /* for */
}  /* form_pm_derived_casts */


void form_pm_constant(a_constant_ptr                        constant,
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
  a_source_correspondence *scp = NULL;
  a_boolean               need_cast_close_paren = FALSE;
  an_il_entry_kind        entry_kind;
  a_base_class_ptr        bcp =
                            constant->variant.ptr_to_member.casting_base_class;

  /* See if this is a pointer to data member or pointer to member function. */
  if (constant->variant.ptr_to_member.is_function_ptr) {
    a_routine_ptr rout = constant->variant.ptr_to_member.variant.routine;
    if (rout != NULL) scp = &rout->source_corresp;
    entry_kind = iek_routine;
  } else {
    a_field_ptr field = constant->variant.ptr_to_member.variant.field;
    if (field != NULL) scp = &field->source_corresp;
    entry_kind = iek_field;
  }  /* if */
  /* If the constant is implicitly cast to another type, ... */
  if (constant->implicit_cast) {
    /* ... then prefix the constant with an explicit cast. */
    /* Do not put out the cast if it's not needed and minimal_casts is
       TRUE. */
    if (!minimal_casts || constant->variant.ptr_to_member.cast_to_base ||
        scp == NULL) {
      output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
      form_cast(orig_type, octl);
    }  /* if */
  }  /* if */
  if (scp == NULL) {
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
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && microsoft_version < 1100 &&
          octl->gen_compilable_code) {
        /* MSVC++ 4.2 doesn't like pointer-to-member casts that adjust both
           the base class and the member type, so add an extra cast to
           adjust the member type, if necessary. */
        a_type_ptr new_member_type = pm_member_type(con_type);
        a_type_ptr member_type = NULL, member_class;
        if (constant->variant.ptr_to_member.is_function_ptr) {
          a_routine_ptr rout = constant->variant.ptr_to_member.variant.routine;
          if (rout != NULL) {
            member_type = rout->type;
            member_class = rout->source_corresp.parent.class_type;
          }  /* if */
        } else {
          a_field_ptr field = constant->variant.ptr_to_member.variant.field;
          if (field != NULL) {
            member_type = field->type;
            member_class = field->source_corresp.parent.class_type;
          }  /* if */
        }  /* if */
        if (member_type != NULL) {
          /* The cast is not needed if the old and new types are the same.
             This test is done with a pointer comparison so as not to
             drag in front-end only routines, but that means it probably
             does nothing for pointers to functions. */
          if (same_entities(member_type, new_member_type)) member_type = NULL;
        }  /* if */
        /* No cast is needed for a null pointer-to-member constant. */
        if (member_type != NULL) {
          /* Make a local pointer to member type and cast to it. */
          a_type temp_type;
          /* Can't call clear_type in a standalone program, so copy and
             modify an existing type. */
          temp_type = *con_type;
          temp_type.variant.ptr_to_member.type = new_member_type;
          temp_type.variant.ptr_to_member.class_of_which_a_member=member_class;
          form_cast(&temp_type, octl);
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    octl->output_str("&");
    /* Output the name, forcing it to be a qualified name. */
    { a_boolean saved_force_qualified_name = octl->force_qualified_name;
      octl->force_qualified_name = TRUE;
      form_name(scp, entry_kind, octl);
      octl->force_qualified_name = saved_force_qualified_name;
    }
    output_optional_close_paren(need_pm_close_paren, octl);
  }  /* if */
  output_optional_close_paren(need_cast_close_paren, octl);
}  /* form_pm_constant */


static a_boolean types_match_ignoring_qualifiers(a_type_ptr type_1,
                                                 a_type_ptr type_2)
/*
Return TRUE if type_1 and type_2 are the same type ignoring type qualifiers.
This is used in deciding whether a particular lvalue formulation should
be used for an address constant; the differences allowed are ones that
can be bridged by a cast on an lvalue address.  This routine is similar to
same_type_with_added_qualifiers, but simpler, and needed here because
in il_to_str we can't use routines that aren't available to back ends
and standalone utility programs.
*/
{
  a_boolean types_match = FALSE;

  type_1 = skip_typerefs(type_1);
  type_2 = skip_typerefs(type_2);
  if (same_entities(type_1, type_2)) {
    types_match = TRUE;
  } else if (type_1->kind != type_2->kind) {
    /* Type kinds do not match, so types do not match. */
    /* types_match = FALSE; -- already set. */
  } else if (type_1->kind == (a_type_kind)tk_pointer
#ifdef pointer_types_have_same_repr
             && pointer_types_have_same_repr(type_1, type_2)
#endif /* ifdef pointer_types_have_same_repr */
                                                            ) {
    /* Continue at the next level for pointers. */
    types_match = types_match_ignoring_qualifiers(type_pointed_to(type_1),
                                                  type_pointed_to(type_2));
  } else if (type_1->kind == (a_type_kind)tk_ptr_to_member) {
    a_type_ptr  class_type_1 = pm_class_type(type_1);
    a_type_ptr  class_type_2 = pm_class_type(type_2);
    if (same_entities(class_type_1, class_type_2)) {
      /* Continue at the next level for pointers to members. */
      types_match = types_match_ignoring_qualifiers(pm_member_type(type_1),
                                                    pm_member_type(type_2));
    }  /* if */
  } else if (!C_mode() &&
             type_1->kind == (a_type_kind)tk_array &&
             !has_unknown_specified_bound(type_1) &&
             !has_unknown_specified_bound(type_2) &&
             type_1->variant.array.variant.number_of_elements ==
                            type_2->variant.array.variant.number_of_elements) {
    /* Continue at the next level for arrays (in C++ mode, the qualifiers on
       array element types count as qualifiers on the array). */
    types_match = types_match_ignoring_qualifiers(array_element_type(type_1),
                                                  array_element_type(type_2));
  }  /* if */
  return types_match;
}  /* types_match_ignoring_qualifiers */


static a_boolean type_matches_desired_type(a_type_ptr type,
                                           a_type_ptr desired_type,
                                           a_boolean  will_use_as_addr,
                                           a_boolean  *type_decay_used)
/*
Return TRUE if type is the same as desired_type, for the purpose of
forming an lvalue as part of generating an address constant.
If will_use_as_addr is TRUE, array-to-pointer decay can be considered
in the match-up.  If type decay is used in making the match, return
*type_decay_used TRUE.
*/
{
  a_boolean type_matches = FALSE;

  *type_decay_used = FALSE;
  /* Check for the same type, ignoring qualifier differences. */
  if (type == desired_type ||  /* For speed. */
      types_match_ignoring_qualifiers(type, desired_type)) {
    type_matches = TRUE;
  } else if (will_use_as_addr) {
    /* Check for the decay cases.  Note that this is checked only when the
       result will be used as an address, and therefore will decay from an
       lvalue to an rvalue.  Note that desired_type is an lvalue type, but
       when will_use_as_addr is TRUE what we're really aiming for is
       pointer-to that type, e.g., if type is "int[3]" and desired_type
       is "int" there's a match, because the type after decay ("int *")
       matches the type when you take the address of the lvalue.
       Note that under this formulation there's no need to check for
       the function decay, since it comes out the same as the first test
       above. */
    if (is_array_type(type)) {
      a_type_ptr element_type = array_element_type(type);
      if (types_match_ignoring_qualifiers(element_type, desired_type)) {
        type_matches = TRUE;
        *type_decay_used = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return type_matches;
}  /* type_matches_desired_type */


static a_field_ptr select_union_field_for_addr_constant(
                                                   a_type_ptr union_type,
                                                   a_type_ptr desired_type,
                                                   a_boolean  will_use_as_addr)
/*
union_type is a union type.  Try to find a field of that union that has type
desired_type, and return a pointer to it.  If no field matches, return NULL.
If will_use_as_addr is TRUE, array-to-pointer decay can be considered in
matching the type.
*/
{
  a_field_ptr field, selected_field = NULL;
  a_boolean   type_decay_used;

  /* Go through the fields, looking for one with the right type. */
  for (field = union_type->variant.class_struct_union.field_list;
       field != NULL;
       field = field->next) {
    if (type_matches_desired_type(field->type, desired_type,
                                  will_use_as_addr, &type_decay_used)) {
      /* If there are several fields with the same type, favor the one with
         the most access. */
      if (field->source_corresp.access == (an_access_specifier)as_public) {
        selected_field = field;
        break;
      }  /* if */
      if (selected_field == NULL ||
          is_more_accessible(field->source_corresp.access,
                             selected_field->source_corresp.access)) {
        /* This field is not public, but it's the most accessible field of
           the right type we've seen so far, so remember it and keep
           looking. */
        selected_field = field;
      }  /* if */
    }  /* if */
  }  /* for */
  return selected_field;
}  /* select_union_field_for_addr_constant */


static a_field_ptr select_arbitrary_field_of_union(a_type_ptr union_type)
/*
union_type points to a union.  Select an arbitrary field from that union
and return a pointer to it.  But try to pick a field that does not have
a class type, because putting a "&" in front of a class can run into
problems if the class overloads operator&.  Generates an internal error
if given an empty union.
*/
{
  a_field_ptr field;

  /* Go though the fields, looking for one with a non-class type. */
  for (field = union_type->variant.class_struct_union.field_list;
       field != NULL;
       field = field->next) {
    a_type_ptr ftype = skip_typerefs(field->type);
    if (ftype->kind != (a_type_kind)tk_class &&
        ftype->kind != (a_type_kind)tk_struct) break;
  }  /* for */
  if (field == NULL) {
    /* No field had a non-class type, so use the first field. */
    field = union_type->variant.class_struct_union.field_list;
    /* Unions can be empty, but we shouldn't be taking the address of
       something inside an empty one. */
    check_assertion(field != NULL);
  }  /* if */
  return field;
}  /* select_arbitrary_field_of_union */


void form_uuidof_reference(a_type_ptr                            uuid_type,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output a Microsoft __uuidof reference.  uuid_type is the type, or NULL
to indicate the "0" case.  Do the output in the way described by octl.
*/
{
  octl->output_str("__uuidof(");
  if (uuid_type != NULL) {
    form_type(uuid_type, octl);
  } else {
    /* Zero GUID. */
    octl->output_str("0");
  }  /* if */
  octl->output_str(")");
}  /* form_uuidof_reference */


static void form_lvalue_for_addressed_entity(
                   a_constant_ptr                        constant,
                   a_type_ptr                            desired_type,
                   a_boolean                             will_use_as_addr,
                   a_boolean                             base_entity_only,
                   a_boolean                             gen_output,
                   a_type_ptr                            *achieved_type,
                   a_boolean                             *type_decay_used,
                   a_targ_ptrdiff_t                      *offset,
                   a_boolean                             *formed_useful_lvalue,
                   an_il_to_str_output_control_block_ptr octl)
/*
Output code that is an lvalue for the entity addressed by the ck_address
constant "constant".  This is done as part of outputting an address constant.
The output can be as simple as "x" or something more complicated like
"x.a.b[5]".  desired_type is the lvalue type ultimately wanted, or NULL
if no preference is indicated; will_use_as_addr is TRUE if the lvalue
will be used as an address (rather than directly as an lvalue).  If
base_entity_only is TRUE, the base entity is put out but no attempt is
made to add addressing modifiers to it.  On return, *achieved_type is set
to the type of the lvalue.  If type decay was used in making the match,
*type_decay_used is returned TRUE, and *achieved_type is the type after
the type decay, minus the top-level pointer-to.  *offset is set to
whatever part of the ck_address constant offset couldn't be dealt with
in the lvalue.  If the type achieved is the desired type, ignoring
type qualifiers, and the offset was dealt with in some appropriate way,
*formed_useful_lvalue is returned TRUE.  If it's returned FALSE,
*achieved_type is set to the base entity type.  Do the output in the
way described by octl, but output nothing if gen_output == FALSE; that
is used for a first exploratory pass.  Note that the addressing
operators put out by this routine are never affected by overloading.
That is, this routine never puts out something like a "&" on a class
that might have operator& overloaded.  However, this routine still
generates code that is not compilable in some obscure C++ cases (e.g.,
taking the address of an inaccessible nonstatic data member of a class);
the C++-generating back end avoids that by arranging to have constant
addressing operations not folded to constants so that such cases won't
come here.  Parentheses are not put around the output; all the
operations generated bind very tightly to the identifier, so
parentheses are not needed.
*/
{
  a_type_ptr              type, orig_type;
  a_constant_ptr          con = NULL;
  a_source_correspondence *entity_scp = NULL;
  an_il_entry_kind        entity_kind;
  a_field_ptr             field;
  a_boolean               proper_type = FALSE;
  a_boolean               local_type_decay_used;
  a_targ_ptrdiff_t        orig_offset = constant->variant.address.offset;

  *formed_useful_lvalue = FALSE;
  *type_decay_used = FALSE;
  *offset = orig_offset;
  /* Get the type of the underlying entity. */
  switch (constant->variant.address.kind) {
    case abk_routine:
      /* A routine. */
      { a_routine_ptr rout = constant->variant.address.variant.routine;
        type = rout->type;
        entity_kind = iek_routine;
        entity_scp = &rout->source_corresp;
        *type_decay_used = TRUE;
      }
      break;
    case abk_variable:
      /* A variable. */
      { a_variable_ptr var = constant->variant.address.variant.variable;
        /* In C++, an anonymous union cannot be named directly, so we have to
           find a field within the union. */
        if (var->is_anonymous_parent_object && !octl->c_generating_back_end) {
          a_type_ptr  union_type = skip_typerefs(var->type);
          field = select_union_field_for_addr_constant(union_type,
                                                       desired_type,
                                                       will_use_as_addr);
          if (field == NULL) {
            /* If no field matches, choose one mostly arbitrarily. */
            /* Note that we're lucky that anonymous unions cannot have
               nonpublic members. */
            field = select_arbitrary_field_of_union(union_type);
          }  /* if */
          type = field->type;
          entity_kind = iek_field;
          entity_scp = &field->source_corresp;
        } else {
          /* Normal variable, not anonymous union. */
          type = var->type;
          entity_kind = iek_variable;
          entity_scp = &var->source_corresp;
        }  /* if */
      }
      break;
    case abk_constant:
      /* Address of a constant, specifically a string. */
      con = constant->variant.address.variant.constant;
      check_assertion_str(con->kind == (a_constant_repr_kind)ck_string,
                 "form_lvalue_for_addressed_entity: address of nonstring con");
      type = con->type;
      break;
    case abk_uuidof:
      /* Address of a structure that represents the uuid information for a
         given class type. */
      type = type_pointed_to(constant->type);
      break;
   case abk_label:
     /* Address of a label (GNU C extension). */
     { a_label_ptr label = constant->variant.address.variant.label;
       type = type_pointed_to(constant->type);
       entity_kind = iek_label;
       entity_scp = &label->source_corresp;
     }
     break;
    default:
      unexpected_condition_str(
                   "form_lvalue_for_addressed_entity: bad addr constant kind");
  }  /* switch */
  orig_type = type;
  /* If the desired type was not specified, use the entity type. */
  if (desired_type == NULL) desired_type = type;
  /* Put out the base entity name or constant. */
  if (gen_output) {
    if (entity_scp != NULL) {
      form_name(entity_scp, entity_kind, octl);
    } else if (con != NULL) {
      /* Constant case. */
      form_constant(con, /*need_parens=*/FALSE, octl);
    } else {
      /* Microsoft __uuidof. */
      check_assertion_str(constant->variant.address.kind ==
                                              (an_address_base_kind)abk_uuidof,
                          "form_lvalue_for_addressed_entity: bad kind");
      form_uuidof_reference(constant->variant.address.variant.type, octl);
    }  /* if */
  }  /* if */
  /* If the type is right and the offset is zero, we have what we need. */
  local_type_decay_used = FALSE;
  if (!constant->implicit_cast || /* For speed. */
      type_matches_desired_type(type, desired_type, will_use_as_addr,
                                &local_type_decay_used)) {
    proper_type = TRUE;
  }  /* if */
  if (proper_type && orig_offset == 0) {
    /* The base entity has the type and offset we need. */
    *formed_useful_lvalue = TRUE;
    /* Note we're careful not to clear *type_decay_used if it was set above
       for the function case. */
    if (local_type_decay_used) *type_decay_used = TRUE;
  } else if (base_entity_only) {
    /* We've been told not to look at addressing modifiers, so stop here. */
  } else {
    /* Loop, refining the lvalue each time around, until we get something with
       the right type and right address, or until we decide to give up. */
    for (;;) {
      a_type_ptr unqual_type = skip_typerefs(type);
      if (unqual_type->kind == (a_type_kind)tk_array) {
        /* Array. */
        /* When forming an address for an array element, it sometimes makes
           sense to leave part of the offset to be done by the caller.
             A arr[4];
             A *p = arr + 2;
           This form works better than "&arr[2]", which might be taking the
           address of a class object whose operator& is overloaded. */
        if (proper_type && will_use_as_addr) {
          *formed_useful_lvalue = TRUE;
          *type_decay_used = TRUE;
          break;
        } else {
          /* Add a subscripting operation. */
          a_type_ptr       element_type = array_element_type(unqual_type);
          a_targ_ptrdiff_t element_size = f_skip_typerefs(element_type)->size;
          a_targ_ptrdiff_t index = *offset / element_size;
          /* C division of negative numbers does not necessarily truncate
             towards zero.  If it doesn't, adjust to the result one would get
             if it did.  See comments in the routine divide_integers. */
          if (*offset < 0 && (*offset % element_size) > 0) index++;
          /* Put out the subscripting operation. */
          if (gen_output) {
            octl->output_str("[");
            form_num(index, octl);
            octl->output_str("]");
          }  /* if */
          type = element_type;
          *offset -= index * element_size;
        }  /* if */
      } else if (unqual_type->kind == (a_type_kind)tk_class ||
                 unqual_type->kind == (a_type_kind)tk_struct) {
        /* A class; try to find a field with the right offset, or at least
           get closer. */
        /* Give up if the offset is outside the class bounds. */
        if (*offset < 0 ||
            *offset >= (a_targ_ptrdiff_t)unqual_type->size) break;
        for (field = unqual_type->variant.class_struct_union.field_list;
             field != NULL;
             field = field->next) {
          /* Look for a field with the right offset.  We do a full check
             on the bounds of the field because there might be holes between
             fields (e.g., base classes). */
          if ((a_targ_ptrdiff_t)field->offset <= *offset &&
              *offset < (a_targ_ptrdiff_t)(field->offset +
                                      skip_typerefs(field->type)->size) &&
              /* Ignore bit fields. */
              field->bit_size == 0) break;
        }  /* for */
        /* Watch out for classes with no fields. */
        if (field == NULL) break;
        if (!has_name(field)) {
          /* An anonymous union field.  Usually, the field selection for such
             a field is just omitted.  In the C-generating back end, however,
             references to anonymous union fields can't be omitted, so give
             up. */
          if (octl->c_generating_back_end) break;
        } else {
          /* Normal field (not anonymous union field). */
          /* Put out the field selection. */
          if (gen_output) {
            octl->output_str(".");
            form_unqualified_name(&field->source_corresp, iek_field, octl);
          }  /* if */
        }  /* if */
        type = field->type;
        *offset -= (a_targ_ptrdiff_t)field->offset;
      } else if (unqual_type->kind == (a_type_kind)tk_union) {
        /* For a union, try to find a field with the right type.  If there's
           no match at the top level, don't try to find some sub-aggregate
           of those fields that will give the right offset; just give up. */
        if (*offset != 0) break;
        field = select_union_field_for_addr_constant(unqual_type,
                                                     desired_type,
                                                     will_use_as_addr);
        if (field == NULL) break;
        /* Found a field with the right type, so use it. */
        /* Put out the field selection. */
        if (gen_output) {
          octl->output_str(".");
          form_unqualified_name(&field->source_corresp, iek_field, octl);
        }  /* if */
        type = field->type;
      } else {
        /* Some other type (not an aggregate); we can't adjust the offset or
           type.  Give up. */
        break;
      }  /* if */
      /* If the offset is now zero, and the type is right, we have what we
         need. */
      if (type_matches_desired_type(type, desired_type, will_use_as_addr,
                                    &local_type_decay_used)) {
        proper_type = TRUE;
      }  /* if */
      if (proper_type && *offset == 0) {
        *formed_useful_lvalue = TRUE;
        *type_decay_used = local_type_decay_used;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (*formed_useful_lvalue) {
    if (*type_decay_used &&
        constant->variant.address.kind != (an_address_base_kind)abk_routine) {
      /* Adjust the type to reflect that fact that type decay was used.
         The type returned is the type under the resulting pointer type.
         Note that because of the convention used for the type, function
         to pointer decay does not change the type. */
      type = array_element_type(type);
    }  /* if */
  } else {
    /* If we didn't succeed in getting the right type and offset, roll the
       type and offset back to the original from the base entity. */
    type = orig_type;
    *offset = orig_offset;
  }  /* if */
  *achieved_type = type;
}  /* form_lvalue_for_addressed_entity */


static void form_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             form_lvalue,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the value of a ck_address constant.  If form_lvalue is TRUE,
put out an lvalue for the thing at that address.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
*/
{
  a_type_ptr       orig_type = constant->type;
  a_type_ptr       con_type, desired_type, achieved_type;
  a_type_ptr       direct_desired_type, direct_achieved_type;
  a_targ_ptrdiff_t offset, dummy_offset;
  a_boolean        cast_to_nonpointer = FALSE, type_decay_used;
  a_boolean        need_ampersand_paren = FALSE;
  a_boolean        final_cast_needed = FALSE;
  a_boolean        need_final_cast_close_paren = FALSE;
  a_boolean        need_offset_addition_close_paren = FALSE;
  a_boolean        need_char_star_cast_close_paren = FALSE;
  a_boolean        reinterpret_cast_needed = FALSE;
  a_boolean        need_reinterpret_cast_close_paren = FALSE;
  a_boolean        formed_useful_lvalue, need_char_star_cast = FALSE;

  con_type = skip_typerefs(orig_type);
  /* When the address constant is cast to some strange type (e.g., long),
     do that as a final cast and don't try to accommodate it in the normal
     processing. */
  /* Note that the test for pointer includes reference as well. */
  if (constant->implicit_cast && con_type->kind != (a_type_kind)tk_pointer) {
    check_assertion(!form_lvalue);
    cast_to_nonpointer = TRUE;
    final_cast_needed = TRUE;
    desired_type = NULL;
  } else {
    desired_type = con_type;
    desired_type = type_pointed_to(desired_type);
  }  /* if */
  if (constant->is_reinterpret_cast && !octl->c_generating_back_end) {
    reinterpret_cast_needed = TRUE;
  }  /* if */
  /* Examine the addressed entity (without generating any code) to
     determine how it will be put out as an lvalue.  This lets us decide
     on putting out a leading cast, etc. before the lvalue is put out. */
  form_lvalue_for_addressed_entity(constant, desired_type,
                                   /*will_use_as_addr=*/!form_lvalue,
                                   /*base_entity_only=*/FALSE,
                                   /*gen_output=*/FALSE,
                                   &achieved_type, &type_decay_used, &offset,
                                   &formed_useful_lvalue, octl);
  if (!type_decay_used && is_array_type(achieved_type) && !form_lvalue) {
    /* Array for which type decay is not being used.  This means, so far,
       that we plan to put a "&" in front of the array.  See if there's a
       reason not to. */
    if (octl->gen_pcc_code) {
      /* Don't do address-of-array in pcc mode, because pcc gives warnings
         on that and uses the pointer-to-element type anyway. */
      type_decay_used = TRUE;
    } else if (octl->gen_compilable_code &&
               constant->variant.address.kind ==
                                          (an_address_base_kind)abk_constant &&
               constant->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string) {
      /* Address of a string constant, e.g., &"abc".  Some ANSI/ISO C
         compilers have difficulty with that, perhaps because they don't
         believe a string is an lvalue.  Force type decay and a cast. */
      type_decay_used = TRUE;
    } else if (offset != 0) {
      /* Some compilers have difficulty with getting the size right when
         adding an offset to the address of an array. */
      type_decay_used = TRUE;
    }  /* if */
    if (type_decay_used) {
      /* If we've turned on array type decay, adjust the type.  Note that
         because of the convention used for the achieved type, the
         pointer-to part of the type is not needed. */
      achieved_type = array_element_type(achieved_type);
      final_cast_needed = TRUE;
    }  /* if */
  }  /* if */
  if (offset != 0) {
    /* The offset is nonzero.  The general way of dealing with this is to cast
       to "char *" and add in the offset.  However, in the right situation
       the addition can be done without going to "char *". */
    need_char_star_cast = TRUE;
    if (!form_lvalue) {
      a_targ_ptrdiff_t size = f_skip_typerefs(achieved_type)->size;
      if (size != 0 && (offset % size) == 0) {
        need_char_star_cast = FALSE;
        offset /= size;
      }  /* if */
    }  /* if */
    if (need_char_star_cast) final_cast_needed = TRUE;
  }  /* if */
  /* See if we need a final cast to the desired type. */
  direct_achieved_type = skip_typedefs(achieved_type);
  direct_desired_type = skip_typedefs(desired_type);
  if (desired_type == NULL ||
      !same_entities(direct_achieved_type, direct_desired_type)) {
    if (!constant->implicit_cast &&
        constant->variant.address.kind == (an_address_base_kind)abk_routine) {
      /* Function declarators don't get shared, so a pointer equality test
         doesn't work well.  We also can't use a routine like
         types_are_compatible to do a full test because those routines are
         not available in standalone utility programs.  It's okay to err on
         the side of putting out the cast, but in the most common case we
         can know that no cast is needed. */
    } else if (C_mode() && is_directly_variably_modified_type(desired_type)) {
      /* Eliminate an implicit cast to a variably-modified type.  We know
         the cast is implicit because explicit casts to directly
         variably-modified types are not folded into the constant. */
      final_cast_needed = FALSE;
      /* Cast to "(void *)" in case there were intervening casts on the
         original entity before the implicit cast to a variably-modified
         type. */
      output_optional_open_paren(&need_parens,
                                 &need_final_cast_close_paren, octl);
      octl->output_str("(void *)");
    } else {
      /* The proper type couldn't be achieved with address operators, so we
         need a final cast to adjust the type.  One important category of cases
         this handles is cases that require just qualification adjustments. */
      final_cast_needed = TRUE;
    }  /* if */
  }  /* if */
  if (final_cast_needed) {
    /* Generate a final cast to the constant type. */
    output_optional_open_paren(&need_parens,
                               &need_final_cast_close_paren, octl);
    if (!form_lvalue) {
      /* Forming an address, not an lvalue. */
      form_general_cast(con_type, reinterpret_cast_needed, octl);
      if (reinterpret_cast_needed) need_reinterpret_cast_close_paren = TRUE;
      if (cast_to_nonpointer) {
        a_targ_alignment alignment;
        /* This is a case where the final type is a nonpointer.  See if an
           extra cast to unsigned long is needed. */
        if (is_integral_or_enum_type(con_type) &&
            con_type->size >= size_of_pointer_to(achieved_type, &alignment)) {
          /* Cast to large-enough integral type.  No extra cast needed. */
        } else {
          /* Anything else (e.g., cast to float).  Go by way of unsigned long
             first. */
          octl->output_str("(unsigned long)");
        }  /* if */
      }  /* if */
    } else {
      a_type type_copy;
      /* When forming an lvalue (C++ only), generate a reference cast.
         Make a copy of the type so it can be changed to a reference type. */
      check_assertion(con_type->kind == (a_type_kind)tk_pointer);
      type_copy = *con_type;
      if (offset != 0 || il_header.source_language != sl_Cplusplus) {
        /* However, that's not possible when the offset is nonzero, or in
           C.  For those cases, use "*(type *)&x". */
        octl->output_str("*");
        type_copy.variant.pointer.is_reference = FALSE;
        form_cast(&type_copy, octl);
        form_lvalue = FALSE;
        type_decay_used = FALSE;
      } else {
        /* Offset is zero, so use reference cast. */
        type_copy.variant.pointer.is_reference = TRUE;
        form_general_cast(&type_copy, reinterpret_cast_needed, octl);
        if (reinterpret_cast_needed) need_reinterpret_cast_close_paren = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (offset != 0) {
    /* The offset couldn't be handled with addressing operators, so it
       will be added in later.  Put parentheses around the offset
       computation. */
    output_optional_open_paren(&need_parens,
                               &need_offset_addition_close_paren, octl);
  }  /* if */
  if (need_char_star_cast) {
    /* Cast to "char *" because the scaling on the offset addition is wrong
       otherwise. */
    output_optional_open_paren(&need_parens,
                               &need_char_star_cast_close_paren, octl);
    octl->output_str("(char *)");
  }  /* if */
  if (!form_lvalue) {
    /* Forming an address, not an lvalue. */
    /* Use array --> pointer or function --> pointer decay to get an address,
       if that's appropriate.  Otherwise a "&" must be put out. */
    if (type_decay_used) {
      /* Using type decay to get a pointer. */
    } else {
      output_optional_open_paren(&need_parens, &need_ampersand_paren, octl);
      if (constant->kind == (a_constant_repr_kind)ck_address &&
	  constant->variant.address.kind == (an_address_base_kind)abk_label) {
	/* The address of a label is taken with "&&", rather than the
	   ordinary "&". */
	octl->output_str("&&");
      } else {
	octl->output_str("&");
      }  /* if */
    }  /* if */
  }  /* if */
  /* Generate code for the lvalue for the entity. */
  form_lvalue_for_addressed_entity(constant, desired_type,
                                   /*will_use_as_addr=*/!form_lvalue,
                                   /*base_entity_only=*/!formed_useful_lvalue,
                                   /*gen_output=*/TRUE,
                                   &achieved_type, &type_decay_used,
                                   &dummy_offset,
                                   &formed_useful_lvalue, octl);
  if (need_ampersand_paren) {
    octl->output_str(")");
  }  /* if */
  output_optional_close_paren(need_char_star_cast_close_paren, octl);
  if (offset != 0) {
    /* Add in the (signed) offset. */
    if (offset >= 0) {
      octl->output_str(" + ");
    } else {
      /* For negative numbers, the sign on the number will be the operator. */
      octl->output_str(" ");
    }  /* if */
    form_num((a_host_large_integer)offset, octl);
    output_optional_close_paren(need_offset_addition_close_paren, octl);
  }  /* if */
  if (need_reinterpret_cast_close_paren) octl->output_str(")");
  output_optional_close_paren(need_final_cast_close_paren, octl);
}  /* form_address_constant */


static a_boolean is_enum_constant_equivalent(a_constant_ptr constant,
                                             a_constant_ptr *equiv_constant)
/*
Given a constant for which is_enum_constant is TRUE, see if it is
an equivalent of a named enum constant.  If it is, set *equiv_constant to
point to the enum constant and return TRUE.  An equivalent of an enum constant
is a copy of the enum constant made to be used in an initializer (because
an initializer requires an unshared copy of the constant).  It can be put
out as the original enum constant.
*/
{
  a_boolean      is_enum_equiv = FALSE;
  a_type_ptr     con_type = constant->type, enum_type;
  a_constant_ptr con;

  *equiv_constant = NULL;
  con_type = skip_typerefs(con_type);
  /* Get the enum type. */
  if (il_header.source_language == sl_Cplusplus) {
    enum_type = con_type;
  } else {
    /* In C, enum constants have type int but an affiliated type that is the
       enum type. */
    enum_type = con_type->variant.integer.enum_info.affiliated_type;
  }  /* if */
  check_assertion(enum_type->kind == (a_type_kind)tk_integer &&
                  enum_type->variant.integer.enum_type);
  /* Go through the list of enum constants and compare each one to the
     constant we want. */
  for (con = enum_type->variant.integer.enum_info.constant_list;
       con != NULL;
       con = con->next) {
    /* Compare the constant on the list to the one we want. */
    if (cmp_integer_constants(con, constant) == 0) {
      /* Equal, so we found the constant we want. */
      is_enum_equiv = TRUE;
      *equiv_constant = con;
      break;
    }  /* if */
    /* Keep looking.  Note that there is no guarantee that the constants are
       in ascending order, so we can't stop on a too-large constant. */
  }  /* for */
  return is_enum_equiv;
}  /* is_enum_constant_equivalent */


void form_unknown_function_constant(
                             a_constant_ptr                        constant,
                             an_il_to_str_output_control_block_ptr octl)
/*
Output the name indicated by a ck_template_param/tpck_unknown_function
or .../tpck_template_ref constant.  Note that while the constant represents
the address of the unknown function, this routine puts out just the name,
without a leading "&".  Do the output in the way described by octl.
*/
{
  a_boolean      is_template = FALSE;
  a_constant_ptr con = constant;

  check_assertion(constant->kind == (a_constant_repr_kind)ck_template_param);
  if (constant->variant.template_param.kind ==
                           (a_template_param_constant_kind)tpck_template_ref) {
    is_template = TRUE;
    con = constant->variant.template_param.variant.template_ref.con;
  }  /* if */
  check_assertion(con->variant.template_param.kind ==
                        (a_template_param_constant_kind)tpck_unknown_function);
  if (con->variant.template_param.variant.unknown_function.
                                                     conversion_type != NULL) {
    /* The associated function is a conversion function.  Generate
       its name from the type. */
    check_assertion(con->source_corresp.is_class_member);
    form_class_qualifier(con->source_corresp.parent.class_type, octl);
    octl->output_str("operator ");
    form_type(con->variant.template_param.variant.
                                              unknown_function.conversion_type,
              octl);
  } else {
    /* Normal case (not a conversion function). */
    a_boolean saved_force_qualified_name = octl->force_qualified_name;
    octl->force_qualified_name = con->variant.template_param.is_qualified_name;
    if (is_template && octl->output_template_name != NULL) {
      octl->output_template_name((char *)&con->source_corresp,
                                 iek_constant);
    } else {
      form_name(&con->source_corresp, iek_constant, octl);
    }  /* if */
    octl->force_qualified_name = saved_force_qualified_name;
  }  /* if */
  if (is_template) {
    /* Add the template arguments. */
    form_template_args(constant->variant.template_param.variant.
                                                         template_ref.arg_list,
                       octl);
  }  /* if */
}  /* form_unknown_function_constant */


static void form_float_constant(
                           an_internal_float_value               *float_value,
                           a_float_kind                          fkind,
                           an_il_to_str_output_control_block_ptr octl)
/*
Output the given floating-point value with the proper suffix (or cast in
K&R/pcc mode) determined by fkind.
*/
{
  char      *str, *suffix = "";
  char      buf[20];
  a_boolean pos_infinity, neg_infinity, not_a_number;

  if (!octl->gen_pcc_code) {
    /* Determine the suffix. */
    if (fkind == (a_float_kind)fk_float) {
      suffix = "F";
    } else if (fkind == (a_float_kind)fk_long_double) {
      suffix = "L";
    }  /* if */
  } else {
    /* Generating K&R C.  Suffixes are not allowed. */
    /* Cast to float if type is float (by default it would be double). */
    if (fkind == (a_float_kind)fk_float) {
      octl->output_str("(float)");
    }  /* if */
  }  /* if */
  str = fp_to_string(fkind, float_value,
                     &pos_infinity, &neg_infinity, &not_a_number);
  if (octl->gen_compilable_code &&
      (pos_infinity || neg_infinity || not_a_number)) {
    /* In compilable code, generate NaNs and Infinities as expressions.
       0.0/0.0 gives a NaN, 1.0/0.0 gives an infinity. */
    char *dividend;
    if (not_a_number) {
      dividend = "0.0";
    } else if (pos_infinity) {
      dividend = "1.0";
    } else {
      dividend = "-1.0";
    }  /* if */
#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
    if (msvc_is_generated_code_target) {
      /* MSVC++ gives an error on (x/0.0), so use a comma operator to
         fool it. */
      (void)sprintf(buf, "(%s%s/(0,0.0%s))", dividend, suffix, suffix);
    } else
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */
    {
      (void)sprintf(buf, "(%s%s/0.0%s)", dividend, suffix, suffix);
    }  /* if */
    str = buf;
    suffix = "";
  }  /* if */
  if (suffix[0] == '\0') {
    octl->output_str(str);
  } else {
    output_partial_token_str(str, octl);
    output_partial_token_str(suffix, octl);
  }  /* if */
}  /* form_float_constant */


static void form_expression(an_expr_node_ptr                      expr,
                            an_il_to_str_output_control_block_ptr octl);


static void form_dynamic_init(a_dynamic_init_ptr                    dip,
                              an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated dynamic initialization.  Do the output in the way
described by octl.  This is used only for non-compilable output (e.g.,
for debug output).
*/
{
  switch (dip->kind) {
    case dik_none:
      octl->output_str("<no-init>");
      break;
    case dik_zero:
      octl->output_str("<zero-init>");
      break;
    case dik_bitwise_copy:
      octl->output_str("<bitwise-copy>");
      break;
    case dik_constant:
    case dik_nonconstant_aggregate:
      form_constant(dip->variant.constant, /*need_parens=*/TRUE, octl);
      break;
    case dik_call_returning_class_via_cctor:
      octl->output_str("call returning class: ");
      /*FALLTHROUGH*/
    case dik_expression:
      form_expression(dip->variant.expression, octl);
      break;
    case dik_constructor:
      octl->output_str("<constructor-call>");
      break;
    default:
      unexpected_condition_str("form_dynamic_init: bad kind");
  }  /* switch */
}  /* form_dynamic_init */


static void form_expression(an_expr_node_ptr                      expr,
                            an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated expression.  Do the output in the way described by
octl.  This is used only for non-compilable output (e.g., for debug output),
and it doesn't have to provide detailed information on every expression.
*/
{
  switch (expr->kind) {
    case enk_error:
      octl->output_str("<error>");
      break;
    case enk_operation:
#if DEBUG
      { an_expr_node_ptr operand = expr->variant.operation.operands;
        char *op_str = db_operator_names[expr->variant.operation.kind];
        an_expr_operator_kind op = expr->variant.operation.kind;
        octl->output_str("(");
        if (op == (an_expr_operator_kind)eok_call ||
            op == (an_expr_operator_kind)eok_virtual_call ||
            op == (an_expr_operator_kind)eok_pm_call) {
          /* Calls. */
          form_expression(operand, octl);
          octl->output_str("(");
          while ((operand = operand->next) != NULL) {
            form_expression(operand, octl);
            if (operand->next != NULL) octl->output_str(", ");
          }  /* while */
          octl->output_str(")");
        } else if (op == (an_expr_operator_kind)eok_subscript) {
          /* Subscripting. */
          form_expression(operand, octl);
          octl->output_str("[");
          form_expression(operand->next, octl);
          octl->output_str("]");
        } else if (op == (an_expr_operator_kind)eok_cast ||
                   op == (an_expr_operator_kind)eok_bool_cast ||
                   op == (an_expr_operator_kind)eok_base_class_cast ||
                   op == (an_expr_operator_kind)eok_derived_class_cast ||
                   op == (an_expr_operator_kind)eok_pm_base_class_cast ||
                   op == (an_expr_operator_kind)eok_pm_derived_class_cast ||
                   op == (an_expr_operator_kind)eok_lvalue_cast) {
          /* Casts. */
          octl->output_str("(");
          form_type(expr->type, octl);
          octl->output_str(")");
          form_expression(operand, octl);
        } else if (operand->next == NULL) {
          /* Unary operators. */
          octl->output_str(op_str);
          octl->output_str(" ");
          form_expression(operand, octl);
        } else if (operand->next->next == NULL) {
          /* Binary operators. */
          form_expression(operand, octl);
          octl->output_str(" ");
          octl->output_str(op_str);
          octl->output_str(" ");
          form_expression(operand->next, octl);
        } else {
          /* Other operators, e.g., "?".  Use generic form. */
          octl->output_str(op_str);
          octl->output_str("(");
          while (operand != NULL) {
            form_expression(operand, octl);
            if (operand->next != NULL) octl->output_str(", ");
            operand = operand->next;
          }  /* while */
          octl->output_str(")");
        }  /* if */
        octl->output_str(")");
      }
#else /* !DEBUG */
      octl->output_str("<operation>");
#endif /* DEBUG */
      break;
    case enk_constant:
      form_constant(expr->variant.constant, /*need_parens=*/TRUE, octl);
      break;
    case enk_variable:
      form_name(&expr->variant.variable->source_corresp,
                (an_il_entry_kind)iek_variable, octl);
      break;
    case enk_variable_address:
      octl->output_str("(&");
      form_name(&expr->variant.variable->source_corresp,
                (an_il_entry_kind)iek_variable, octl);
      octl->output_str(")");
      break;
    case enk_routine_address:
      octl->output_str("(&");
      form_name(&expr->variant.routine->source_corresp,
                (an_il_entry_kind)iek_routine, octl);
      octl->output_str(")");
      break;
    case enk_field:
      form_name(&expr->variant.field->source_corresp,
                (an_il_entry_kind)iek_field, octl);
      break;
    case enk_temp_init:
      octl->output_str("temp-init(");
      form_dynamic_init(expr->variant.init.dynamic_init, octl);
      octl->output_str(")");
      break;
    default:
      octl->output_str("<expression>");
      break;
  }  /* switch */
}  /* form_expression */


static void form_dynamic_init_constant(
                                a_constant_ptr                        constant,
                                an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated ck_dynamic_init constant.  Do the output in the way
described by octl.  This is used only for non-compilable output (e.g.,
for debug output).
*/
{
  a_dynamic_init_ptr dip;

  check_assertion(!octl->gen_compilable_code &&
                  constant->kind == (a_constant_repr_kind)ck_dynamic_init);
  octl->output_str("dynamic-init: ");
  dip = constant->variant.dynamic_init;
  form_dynamic_init(dip, octl);
}  /* form_dynamic_init_constant */


void form_constant(a_constant_ptr                        constant,
                   a_boolean                             need_parens,
                   an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated constant.  If an expression node corresponding to the
operation that resulted in the constant is available, output the expression
instead of just the result of the operation.  If need_parens is TRUE,
parentheses are placed around the constant if there's any possibility of
precedence confusion.  Do the output in the way described by octl.
*/
{
  a_constant_repr_kind kind = constant->kind;
#if defined(FFE) && !C99_IL_EXTENSIONS_SUPPORTED
  a_float_kind         fkind;
#endif /* defined(FFE) && !C99_IL_EXTENSIONS_SUPPORTED */
  a_type_ptr           con_type = NULL, orig_type;
  a_boolean            need_cast_close_paren = FALSE, is_enum;
  a_boolean            need_reinterpret_cast = FALSE;
  a_constant_ptr       equiv_constant;
  a_boolean            suppress_cast_on_integer_constant = FALSE;

  orig_type = constant->type;
  /* Watch out for constants (like ck_init_repeat) that have no type. */
  if (orig_type == NULL) {
#if CHECKING
    if (kind != (a_constant_repr_kind)ck_init_repeat &&
	kind != (a_constant_repr_kind)ck_designator) {
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**NULL-CONSTANT-TYPE**");
      } else
#endif /* DEBUG */
      /* Do not insert code here.  This is the else of an "if". */
      {
        unexpected_condition_str("form_constant: constant with null type");
      }
    }  /* if */
#endif /* CHECKING */
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  } else if (constant_should_be_put_out_as_expr(constant) &&
             !octl->c_generating_back_end &&
             octl->output_expression != NULL) {
    /* An expression was recorded for this constant.  Output that expression
       rather than the folded constant. */
    octl->output_expression(constant->expr);
    goto done;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  } else {
    con_type = skip_typerefs(orig_type);
    /* See if we need a cast to the constant result type. */
    if (kind == (a_constant_repr_kind)ck_address ||
        kind == (a_constant_repr_kind)ck_ptr_to_member) {
      /* Don't do this here for address constants or pointer-to-member
         constants (they're handled in the subroutines). */
    } else {
      /* If the constant is implicitly cast to another type, prefix the
         constant with an explicit cast. */
      a_boolean need_cast = FALSE;
      if (constant->is_reinterpret_cast && !octl->c_generating_back_end) {
        /* The source form used reinterpret_cast, so a cast is needed. */
        need_cast = TRUE;
        need_reinterpret_cast = TRUE;
      } else if (constant->implicit_cast) {
        need_cast = TRUE;
        if (octl->gen_compilable_code && C_mode() &&
            is_directly_variably_modified_type(orig_type)) {
          /* Casts to directly variably-modified types must be suppressed.
             That's possible because they are folded into the constant only
             if they are implicit.  However, we must still deal with the
             fact that the constant may have been explicitly cast to some other
             pointer type before it was cast to the variably-modified type. */
          check_assertion(is_pointer_type(orig_type));
          need_cast = FALSE;
          /* For null pointer constants, the extra cast to "void *" is
             not necessary. */
          if (constant->kind != (a_constant_repr_kind)ck_integer ||
              cmplit_integer_constant(constant,
                                      (a_host_large_integer)0) != 0) {
            output_optional_open_paren(&need_parens, &need_cast_close_paren,
                                       octl);
            octl->output_str("(void *)");
            suppress_cast_on_integer_constant = TRUE;
          }  /* if */
        }  /* if */
      } else if (constant->kind == (a_constant_repr_kind)ck_template_param &&
                 constant->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_cast &&
                 constant->explicit_cast_applied) {
        need_cast = TRUE;
      }  /* if */
      if (need_cast) {
        /* Prefix the constant with an explicit cast. */
        output_optional_open_paren(&need_parens, &need_cast_close_paren, octl);
        form_general_cast(orig_type, need_reinterpret_cast, octl);
        suppress_cast_on_integer_constant = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  switch (kind) {
    case ck_error:
      check_assertion_str(!octl->gen_compilable_code,
                          "form_constant: error constant");
      octl->output_str("<error-constant>");
      break;
    case ck_integer:
      /* See if the constant is an enum constant, but don't emit enum
         constants when generating K&R C from the C-generating back end. */
      is_enum = !(octl->c_generating_back_end && octl->gen_pcc_code) &&
                is_enum_constant(constant);
      if (is_enum && has_name(constant)) {
        /* An enum constant. */
        form_name(&constant->source_corresp, iek_constant, octl);
      } else if (is_enum && il_header.source_language == sl_Cplusplus &&
#if DEBUG
                 !octl->debug_output &&
#endif /* DEBUG */
                 is_enum_constant_equivalent(constant, &equiv_constant)) {
        /* The equivalent of an enum constant (an enum constant used in
           an initializer; it's a nonshared constant with the same value as
           the named enumeration constant). */
        form_name(&equiv_constant->source_corresp, iek_constant, octl);
      } else if (!octl->c_generating_back_end &&
                 il_header.source_language == sl_Cplusplus &&
                 is_bool_type(con_type)) {
        /* A bool constant. */
        octl->output_str((char *)(
               cmplit_integer_constant(constant,
                            (a_host_large_integer)0) != 0 ? "true" : "false"));
      } else if (!octl->c_generating_back_end &&
                 il_header.source_language == sl_Cplusplus &&
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
        (void)form_char((char)value_of_integer_constant(constant, &ovflo),
                        octl);
        output_partial_token_str("'", octl);
        output_optional_close_paren(need_char_cast_close_paren, octl);
      } else if (!octl->c_generating_back_end &&
                 con_type->kind == (a_type_kind)tk_integer &&
                 con_type->variant.integer.wchar_t_type) {
        /* In C++, wide character constants have wchar_t type. */
        a_boolean ovflo;
        output_partial_token_str("L'", octl);
        (void)form_wide_char(
                    (unsigned long)unsigned_value_of_integer_constant(constant,
                                                                      &ovflo),
                    octl);
        output_partial_token_str("'", octl);
      } else {
        /* A normal integer constant. */
        form_integer_constant(constant, suppress_cast_on_integer_constant,
                              need_parens, octl);
      }  /* if */
      break;
    case ck_string:
      /* String constant. */
      { a_targ_size_t a;
        char          ch;
        unsigned long wc;
        char          *str = constant->variant.string.value;
        a_targ_size_t len = constant->variant.string.length;
        int           out_len = 0;

#if BACK_END_IS_C_GEN_BE
        if (octl->c_generating_back_end && constant->assoc_var_assigned) {
          /* The C-generating back end transforms wide string literals: it
             creates a variable initialized with the string value and then
             uses the variable instead of the string.  This ensures proper
             alignment for the string.  The temp name for the variable is
             derived from the address of the constant; there is no actual
             variable entry. */
          output_temp_name((char *)constant, octl);
        } else
#endif /* BACK_END_IS_C_GEN_BE */
        /* Do not insert code here.  This is the "else" of an "if". */
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
            if (out_len >= 128 && octl->gen_compilable_code &&
                !octl->gen_pcc_code) {
              /* Break long string constants by using concatenation.  This
                 allows the output routine to begin a new line. */
              output_partial_token_str("\"", octl);
              octl->output_str(" ");
              output_partial_token_str("L\"", octl);
              out_len = 0;
            }  /* if */
            wc = extract_wide_char_from_string(str+a);
            /* Suppress the last character if it is a null. */
            if (a != (len - targ_sizeof_wchar_t) || wc != '\0') {
              out_len += form_wide_char(wc, octl);
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
            if (out_len >= 128 && octl->gen_compilable_code &&
                !octl->gen_pcc_code) {
              /* Break long string constants by using concatenation.  This
                 allows the output routine to begin a new line. */
              output_partial_token_str("\"", octl);
              octl->output_str(" ");
              output_partial_token_str("\"", octl);
              out_len = 0;
            }  /* if */
            ch = str[a];
            /* Suppress the last character if it is a null. */
            if (a != (len - 1) || ch != '\0') {
              out_len += form_char(ch, octl);
            }  /* if */
          }  /* for */
          output_partial_token_str("\"", octl);
        }  /* if */
      }
      break;
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      /* Floating-point constant. */
      /* Put parentheses around the constant in case it's negative. */
      octl->output_str("(");
      form_float_constant(&constant->variant.float_value,
                          con_type->variant.float_kind,
                          octl);
#if C99_IL_EXTENSIONS_SUPPORTED
      if (kind == (a_constant_repr_kind)ck_imaginary) {
        /* Imaginary constants are constructed with the EDG-specific __I__. */
        octl->output_str("*__I__");
      }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
      octl->output_str(")");
      break;
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
      /* Complex constant. */
      /* Put parentheses around the constant and use the form
         ( A + B*__I__ ). */
      octl->output_str("(");
      form_float_constant(&constant->variant.complex_value->real,
                          con_type->variant.float_kind,
                          octl);
      octl->output_str(" + ");
      form_float_constant(&constant->variant.complex_value->imag,
                          con_type->variant.float_kind,
                          octl);
      octl->output_str("*__I__");
      octl->output_str(")");
      break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#ifdef CFE
    case ck_address:
      /* Address constant. */
      form_address_constant(constant, /*form_lvalue=*/FALSE, need_parens,
                            octl);
      break;
    case ck_ptr_to_member:
      /* Pointer-to-member constant. */
      form_pm_constant(constant, /*minimal_casts=*/!octl->gen_compilable_code,
                       need_parens, octl);
      break;
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      octl->output_str("<stack-offset-of: ");
      form_name(&constant->variant.stack_offset.variable->source_corresp,
                iek_variable, octl);
      if (constant->variant.stack_offset.offset != 0) {
        octl->output_str("+");
        form_unsigned_num(
                  (a_host_large_unsigned)constant->variant.stack_offset.offset,
                  octl);
      }  /* if */
      octl->output_str(">");
      break;
#endif /* DO_IL_LOWERING && ... */
    case ck_dynamic_init:
      form_dynamic_init_constant(constant, octl);
      break;
#endif /* ifdef CFE */
    case ck_aggregate:
      octl->output_str("{");
      { a_constant_ptr sub_con = constant->variant.aggregate.first_constant;
        for (; sub_con != NULL; sub_con = sub_con->next) {
          form_constant(sub_con, /*need_parens=*/FALSE, octl);
          if (sub_con->next != NULL &&
              sub_con->kind != (a_constant_repr_kind)ck_designator) {
            octl->output_str(", ");
          }  /* if */
        }  /* for */
      }
      octl->output_str("}");
      break;
    case ck_init_repeat:
      check_assertion(!octl->gen_compilable_code);
      octl->output_str("<");
      form_unsigned_num(
             (a_host_large_unsigned)constant->variant.init_repeat.count, octl);
      octl->output_str(" repetitions of ");
      form_constant(constant->variant.init_repeat.constant,
                    /*need_parens=*/FALSE, octl);
      octl->output_str(">");
      break;
#ifdef CFE
    case ck_template_param:
      check_assertion(!octl->gen_compilable_code ||
                      prototype_instantiations_in_il);
      switch (constant->variant.template_param.kind) {
        case tpck_unknown_function:
        case tpck_template_ref:
          /* Address of an unknown function, or of an unknown function template
             with an explicit template argument list. */
          if (need_parens) octl->output_str("(");
          octl->output_str("&");
          form_unknown_function_constant(constant, octl);
          if (need_parens) octl->output_str(")");
          break;
        case tpck_param:
          {
            a_source_correspondence_ptr scp = &constant->source_corresp;
            an_il_entry_kind            scp_kind = iek_constant;
            a_source_correspondence_ptr new_scp;
            /* See whether the template parameter name is remapped in the
               current context. */
            new_scp = source_corresp_for_template_param(
                       &constant->variant.template_param.variant.coordinates);
            if (new_scp != NULL) {
              scp = new_scp;
              scp_kind = iek_template_parameter;
            }  /* if */
            form_name(scp, scp_kind, octl);
          }
          break;
        case tpck_member:
          {
            a_source_correspondence_ptr scp = &constant->source_corresp;
            if (scp->member_of_unknown_base) {
              /* We're pretending that we found the member in a dependent
                 base class.  That means the original form of reference
                 was unqualified. */
              form_unqualified_name(scp, (an_il_entry_kind)iek_constant, octl);
            } else {
              form_name(scp, (an_il_entry_kind)iek_constant, octl);
            }  /* if */
          }
          break;
        case tpck_expression:
          if (octl->output_expression == NULL) {
            /* No routine to do the expression output.  Do default
               non-compilable output. */
            check_assertion(!octl->gen_compilable_code);
            octl->output_str("<template-expr>");
          } else {
            octl->output_expression(
                               constant->variant.template_param.variant.expr);
          }  /* if */
          break;
        case tpck_cast:
          form_constant(constant->variant.template_param.variant.constant,
                        /*need_parens=*/FALSE, octl);
          break;
        case tpck_address:
          if (need_parens) octl->output_str("(");
          octl->output_str("&");
          form_constant(constant->variant.template_param.variant.constant,
                        /*need_parens=*/FALSE, octl);
          if (need_parens) octl->output_str(")");
          break;
        case tpck_sizeof:
          octl->output_str("sizeof(");
          form_type(constant->variant.template_param.variant.type, octl);
          octl->output_str(")");
          break;
        case tpck_alignof:
          octl->output_str("__ALIGNOF__(");
          form_type(constant->variant.template_param.variant.type, octl);
          octl->output_str(")");
          break;
        case tpck_uuidof:
          /* The constant represents the address of the __uuidof, so add
             a "&". */
          if (need_parens) octl->output_str("(");
          octl->output_str("&");
          form_uuidof_reference(constant->variant.template_param.variant.type,
                                octl);
          if (need_parens) octl->output_str(")");
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
#ifdef CFE
    case ck_designator:
      if (constant->variant.designator.field != NULL) {
        a_field_ptr field = constant->variant.designator.field;
        octl->output_str(".");
        form_unqualified_name(&field->source_corresp, iek_field, octl);
        octl->output_str(" = ");
      } else {
        octl->output_str("[");
        form_unsigned_num(constant->variant.designator.array_element, octl);
        octl->output_str("] = ");
      } /* if */
      break;
#endif /* ifdef CFE */
    default:
#if DEBUG
      if (octl->debug_output) {
        octl->output_str("**BAD-CONSTANT-KIND**");
        break;
      }  /* if */
#endif /* DEBUG */
      unexpected_condition_str("form_constant: bad constant kind");
  }  /* switch */
  if (need_reinterpret_cast) octl->output_str(")");
  if (need_cast_close_paren) octl->output_str(")");
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
done:;
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
}  /* form_constant */


void form_lvalue_address_constant(
                          a_constant_ptr                        constant,
                          a_boolean                             need_parens,
                          an_il_to_str_output_control_block_ptr octl)
/*
Output the indicated constant.  It's the initial value of a reference,
so remove one level of indirection (basically, the constant is an address).
If need_parens is TRUE, parentheses are placed around the constant if
there's any possibility of precedence confusion.  Do the output in the
way described by octl.
*/
{
  a_type_ptr  object_type, element_type;
  if (constant->kind == (a_constant_repr_kind)ck_address &&
      /* Suppress this special processing on something like *"abcd", because
         gcc 2.95.2 issues a warning on storing into "abcd"[0] but not on
         storing into *"abcd".  The other form is correct; it's avoided
         only because it draws this warning in one form and not in the
         other.  (The issue is differences in test suite runs, not
         the warning per se.) */
      !(constant->implicit_cast &&
        constant->variant.address.kind == (an_address_base_kind)abk_constant &&
        constant->variant.address.variant.constant->kind ==
                                             (a_constant_repr_kind)ck_string &&
        constant->variant.address.offset == 0 &&
        is_pointer_type(constant->type) &&
        (object_type = type_pointed_to(constant->type),
         element_type = array_element_type(
                            constant->variant.address.variant.constant->type),
         same_entities(object_type, element_type)))) {
    /* An address constant (the usual case).  Drop one level of "&". */
    form_address_constant(constant, /*form_lvalue=*/TRUE, need_parens, octl);
  } else if (constant->kind == (a_constant_repr_kind)ck_template_param &&
             constant->variant.template_param.kind ==
                                  (a_template_param_constant_kind)tpck_param) {
    /* This is a template parameter list in a prototype instantiation.
       The parameter has a reference type, so no adjustment is needed. */
    form_name(&constant->source_corresp, iek_constant, octl);
  } else {
    /* For other cases, e.g.,
         int &r = *(int *)5;
       do an indirection in the code. */
    octl->output_str("(*");
    form_constant(constant, need_parens, octl);
    octl->output_str(")");
  }  /* if */
}  /* form_lvalue_address_constant */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
