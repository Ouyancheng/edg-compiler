/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_name.c -- Do name mangling for IL lowering.

*/

#include "basic_hdrs.h"
#if NEED_NAME_MANGLING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* NEED_NAME_MANGLING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if NEED_NAME_MANGLING


static sizeof_t mangled_encoding_for_type(a_type_ptr type,
                                          char       *store_at);
static sizeof_t mangled_function_name(a_routine_ptr routine,
                                      a_boolean     suppress_param_encoding,
                                      char          *store_at);
static sizeof_t mangled_member_variable_name(a_variable_ptr variable,
                                             char           *store_at);
static char *mangled_expr_operator_name(an_expr_operator_kind op);
static sizeof_t mangled_encoding_for_expression(an_expr_node_ptr expr,
                                                char             *store_at);
static sizeof_t mangled_member_name(a_source_correspondence *scp,
                                    a_boolean               is_specialization,
                                    char                    *store_at);
static sizeof_t mangled_encoding_for_constant(a_constant_ptr con,
                                              a_boolean      old_form,
                                              char           *store_at);


static sizeof_t digits_to_represent(unsigned long value)
/*
Return the number of digits needed for the decimal representation of value,
e.g., 1297 --> 4.
*/
{
  sizeof_t ndigits = 1;

  while (value > 9) {
    value /= 10;
    ndigits++;
  }  /* while */
  return ndigits;
}  /* digits_to_represent */


static sizeof_t digits_to_represent_with_underscore(unsigned long value,
                                                    a_boolean     old_form)
/*
Like digits_to_represent, returns the number of digits needed to represent
the value.  However, this is used for cases where the distinction between
single-digit and multi-digit cases needs to be indicated.  With old_form
TRUE, the representation will be simply "d" for single-digit cases, and
"dd_" for multi-digit cases (which has some ambiguity problems in contexts
where an underscore could be next).  With old_form FALSE, the representation
is "_dd_" regardless of the length.
*/
{
  sizeof_t ndigits = digits_to_represent(value);

  if (old_form) {
    /* "d" or "dd_". */
    if (ndigits > 1) ndigits++;
  } else {
    /* "_dd_". */
    ndigits += 2;
  }  /* if */
  return ndigits;
}  /* digits_to_represent_with_underscore */


static sizeof_t mangled_encoding_for_type_qualifiers(
                                               a_type_qualifier_set qualifiers,
                                               char                 *store_at)
/*
Determine the mangled encoding for the type qualifiers (if any) in the
set "qualifiers".  Place the encoded form at *store_at if store_at != NULL,
and (always) return the length of the encoding.
*/
{
  sizeof_t mangled_name_length = 0;

  if (qualifiers & TQ_CONST) {
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'C';
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'V';
  }  /* if */
  return mangled_name_length;
}  /* mangled_encoding_for_type_qualifiers */


static sizeof_t mangled_encoding_for_parameter_types(a_type_ptr type,
                                                     char       *store_at)
/*
Determine the mangled encoding for the parameters of function type "type".
Place the encoded form at *store_at if store_at != NULL, and (always) return
the length of the encoding.  See ARM 7.2.1c for name encoding.
*/
{
  sizeof_t                      mangled_name_length, section_length;
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param, existing_param;
  unsigned long                 existing_param_num, num_matching_types;
  sizeof_t                      digits;

  /* The encoding for parameter types is as follows:
       (1)  For each parameter, the encoding for the type.  If a parameter
            has a type that has appeared already in the parameter list,
            "Tn" is used to repeat the type of parameter "n" ("n" can be
            a multi-digit number; the first parameter is numbered 1).
            If several consecutive parameters have the same type as a previous
            parameter, "Nmn" is used to indicate "m" repetitions of the
            type of parameter "n" ("n" is as for "Tn"; "m" is a one-digit
            number, so a maximum of 9 repetitions is possible).
            If the parameter list is empty, "v" for "void".
       (2)  If the parameter list ends with an ellipsis, "e".
  */
  mangled_name_length = 0;
  rtsp = type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  if (param == NULL) {
    /* Void parameter list. */
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'v';
  } else {
    /* Output the parameter types. */
    for (; param != NULL; param = param->next) {
      /* See if the parameter type is the same as any existing parameter
         type. */
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
      /* Only check the first 9 parameters to avoid multi-digit
         numbers which would cause ambiguous mangling. */
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
      for (existing_param = rtsp->param_type_list, existing_param_num = 1;
           existing_param != param
#if !CFRONT_OBJECT_CODE_COMPATIBILITY
                                   && existing_param_num < 10
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
                                                             ;
           existing_param = existing_param->next, existing_param_num++) {
        if (types_are_compatible(existing_param->type, param->type)) {
          /* Found a type that is being reused.  See if there are more
             instances following this one, in which case we can use the "Nmn"
             encoding.  Stop when 9 matches are found, since that's the most
             that can be encoded in a single "Nmn" sequence. */
          for (num_matching_types = 1;
               num_matching_types < 9 && param->next != NULL &&
                 types_are_compatible(existing_param->type, param->next->type);
               num_matching_types++, param = param->next) {}
          if (num_matching_types == 1) {
            /* Only one match, so use the "Tn" form. */
            mangled_name_length++;
            if (store_at != NULL) *store_at++ = 'T';
          } else {
            /* More than one match, so use the "Nmn" form. */
            mangled_name_length++;
            if (store_at != NULL) *store_at++ = 'N';
            /* Output the "m" (repetition count). */
            digits = 1;  /* digits_to_represent(num_matching_types) */
            mangled_name_length += digits;
            if (store_at != NULL) {
              (void)sprintf(store_at, "%lu", num_matching_types);
              store_at += digits;
            }  /* if */
          }  /* if */
          /* Output the "n" (existing parameter number). */
          digits = digits_to_represent(existing_param_num);
          mangled_name_length += digits;
          if (store_at != NULL) {
            (void)sprintf(store_at, "%lu", existing_param_num);
            store_at += digits;
          }  /* if */
          goto arg_done;
        }  /* if */
      }  /* for */
      /* The parameter type does not match any of the previous parameter
         types, so just put it out. */
      section_length = mangled_encoding_for_type(param->type, store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
arg_done:;
    }  /* for */
  }  /* if */
  /* Output the final "e" for an ellipsis. */
  if (rtsp->has_ellipsis) {
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'e';
  }  /* if */
  return mangled_name_length;
}  /* mangled_encoding_for_parameter_types */


static sizeof_t mangled_encoding_for_function_type(a_type_ptr type,
                                                   a_boolean  do_return_type,
                                                   char       *store_at)
/*
Determine the mangled encoding for the function type "type".  Place the
encoded form at *store_at if store_at != NULL, and (always) return the
length of the encoding.  See ARM 7.2.1c for name encoding.  The return type
of the function is encoded if do_return_type is TRUE.
*/
{
  sizeof_t mangled_name_length, section_length;

  check_assertion(type->kind == (a_type_kind)tk_routine);
  /* The encoding for a function type is "F" followed by the encoding
     for the parameter types.  mangled_function_name takes care of putting
     out additional information preceding the "F" if the function is a
     member function. */
  /* Start with the "F" indicating a function type. */
  mangled_name_length = 1;
  if (store_at != NULL) *store_at++ = 'F';
  /* Add the parameter types. */
  section_length = mangled_encoding_for_parameter_types(type, store_at);
  mangled_name_length += section_length;
  if (store_at != NULL) store_at += section_length;
  if (do_return_type) {
    /* Add the return type at the end, as "_" followed by the type. */
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = '_';
    mangled_name_length +=
                  mangled_encoding_for_type(type->variant.routine.return_type,
                                            store_at);
  }  /* if */
  return mangled_name_length;
}  /* mangled_encoding_for_function_type */


static sizeof_t mangled_encoding_for_function_qualifiers(a_type_ptr type,
                                                         char       *store_at)
/*
Determine the mangled encoding for the type qualifiers (if any) on the
member function type "type".  Place the encoded form at *store_at if
store_at != NULL, and (always) return the length of the encoding.
*/
{
  sizeof_t              mangled_name_length = 0, section_length;
  a_type_ptr            this_param_type;
  a_type_qualifier_set  qualifiers;

  type = skip_typerefs(type);
  this_param_type = type->variant.routine.extra_info->implicit_this_param_type;
  if (this_param_type != NULL) {
    /* The function is a nonstatic member function. */
    this_param_type = type_pointed_to(this_param_type);
    /* Add any qualifiers on the "this" parameter type (actually, the type
       pointed to by the "this" parameter). */
    qualifiers = get_top_level_type_qualifiers(this_param_type);
    if (qualifiers != 0) {
      section_length = mangled_encoding_for_type_qualifiers(qualifiers,
                                                            store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
  } else {
    /* Static member function. */
    mangled_name_length++;
    if (store_at != NULL) *store_at++ = 'S';
  }  /* if */
  return mangled_name_length;
}  /* mangled_encoding_for_function_qualifiers */


static void store_digits_and_underscore(unsigned long value,
                                        sizeof_t      digits,
                                        a_boolean     old_form,
                                        char          *store_at)
/*
Store the decimal representation of value at *store_at.  This is used for
cases where the distinction between single-digit and multi-digit cases needs
to be indicated.  With old_form TRUE, the representation will be simply "d"
for single-digit cases, and "dd_" for multi-digit cases.  With old_form
FALSE, the representation is "_dd_" regardless of the length.
digits indicates the size of the output including any underscores, as
determined by digits_to_represent_with_underscore.
*/
{
  if (old_form) {
    (void)sprintf(store_at, "%lu%s", value, (digits > 1) ? "_" : "");
  } else {
    (void)sprintf(store_at, "_%lu_", value);
  }  /* if */
}  /* store_digits_and_underscore */


static size_t mangled_encoding_for_template_parameter(
                                       a_template_param_coordinate *coordinate,
                                       char                        *store_at)
/*
Place the encoding for a template parameter with the given coordinates at
*store_at if store_at != NULL, and (always) return the length of the
encoding.
*/
{
  sizeof_t mangled_name_length, digits;

  check_assertion(distinct_mangling_for_templates);
  /* The encoding is "ZnZ" for a first-level parameter, and "Zn_mZ" for
     a non-first-level parameter, with "n" the parameter number, and
     "m" the depth number.  The "Z" on the end is to avoid ambiguities
     when this construct is followed by something that begins with a
     number, e.g., when a template parameter in a function parameter
     list is followed by a class name. */
  mangled_name_length = 1;
  if (store_at != NULL) *store_at++ = 'Z';
  /* Put out the parameter position number. */
  digits = digits_to_represent((unsigned long)coordinate->position);
  mangled_name_length += digits;
  if (store_at != NULL) {
    (void)sprintf(store_at, "%lu", (unsigned long)coordinate->position);
    store_at += digits;
  }  /* if */
  if (coordinate->depth != 1) {
    /* Put out "_depth". */
    digits = digits_to_represent((unsigned long)coordinate->depth);
    mangled_name_length += digits + 1;
    if (store_at != NULL) {
      (void)sprintf(store_at, "_%lu", (unsigned long)coordinate->depth);
      store_at += digits + 1;
    }  /* if */
  }  /* if */
  /* Put out the final "Z". */
  mangled_name_length++;
  if (store_at != NULL) *store_at++ = 'Z';
  return mangled_name_length;
}  /* mangled_encoding_for_template_parameter */


static sizeof_t mangled_encoding_for_constant_cast(a_type_ptr     type,
                                                   a_constant_ptr con,
                                                   char           *store_at)
/*
Place a mangled representation of the constant "con" cast to the type "type"
at *store_at if store_at != NULL, and (always) return the length of the
mangled form.
*/
{
  sizeof_t mangled_expr_length, section_length;

  /* Output has the form
       Ocsi1Z1ZO <-- "(int)Z1", Z1 indicating a nontype template parameter.
               ^---- "O" to end the operation encoding.
            ^^^----- Operand.
           ^-------- Count of operands, always 1 for cast.
          ^--------- Encoding for type to cast to.
        ^^---------- Operation, always "cs" for cast.
       ^------------ "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
  */
  /* Put out the initial "O". */
  mangled_expr_length = 1;
  if (store_at != NULL) *store_at++ = 'O';
  /* Put out the operator name "cs". */
  mangled_expr_length += 2;
  if (store_at != NULL) {
    (void)strcpy(store_at, "cs");
    store_at += 2;
  }  /* if */
  /* The operator name "cs" is followed by the encoding for the
     type cast to. */
  section_length = mangled_encoding_for_type(type, store_at);
  mangled_expr_length += section_length;
  if (store_at != NULL) store_at += section_length;
  /* Put out the count of operands. */
  mangled_expr_length++;
  if (store_at != NULL) *store_at++ = '1';
  /* Put out the operand. */
  section_length = mangled_encoding_for_constant(con, /*old_form=*/FALSE,
                                                 store_at);
  mangled_expr_length += section_length;
  if (store_at != NULL) store_at += section_length;
  /* Put out the final "O". */
  mangled_expr_length++;
  if (store_at != NULL) *store_at++ = 'O';
  return mangled_expr_length;
}  /* mangled_encoding_for_constant_cast */


static sizeof_t literal_representation(a_constant_ptr con,
                                       a_boolean      old_form,
                                       char           *store_at)
/*
Place the literal form of the constant con at *store_at if store_at != NULL,
and (always) return the length of the literal representation.  This is
used to encode constants as part of the mangled names of template classes.
If old_form is TRUE, use the old form of length specification in the
mangling for lengths of literals.
*/
{
  sizeof_t       literal_length, str_length, digits;
  char           *str;
  char           buffer[50];

  switch (con->kind) {
    case ck_error:
      /* This might come up in mangling names for template instantiations. */
      literal_length = 1;
      if (store_at != NULL) *store_at++ = '?';
      break;
    case ck_integer:
      /* Integer: the encoding is like
           L3n12  <-- encoding for "-12"
              ^^----- Literal value.
             ^------- "n" indicates negative.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         This is compatible with cfront 3.0.1. */
      str = str_for_integer_constant(con);
      str_length = strlen(str);  /* Includes "-" sign if any. */
      digits = digits_to_represent_with_underscore((unsigned long)str_length,
                                                   old_form);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        store_digits_and_underscore((unsigned long)str_length, digits,
                                    old_form, store_at);
        store_at += digits;
        (void)memcpy(store_at, str, size_t_arg(str_length));
        /* Use "n" to represent a minus sign. */
        if (*store_at == '-') *store_at = 'n';
        store_at += str_length;
      }  /* if */
      break;
    case ck_float:
      /* Float: the encoding is like
           L4n1p5 <-- encoding for "-1.5"
              ^^^---- Literal value ("p" for decimal point).
             ^------- "n" indicates negative.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         cfront 3.0.1 does not implement this, so we made it up. */
      str = fp_to_string(skip_typerefs(con->type)->variant.float_kind,
                         &con->variant.float_value);
      str_length = strlen(str);  /* Includes "-" sign if any. */
      /* Remove unnecessary trailing zeroes, e.g., change
         "1.50000e+10" to "1.5    e+10".  The blanks are then dropped
         in the copy below. */
      { char *p = strchr(str, '.'), *last_signif;
        if (p != NULL) {
          /* There is a decimal point.  Find the last significant digit
             following the decimal point. */
          /* The first digit after the decimal is considered significant even
             if it is a zero. */
          for (last_signif = ++p; isdigit(*p); p++) {
            if (*p != '0') last_signif = p;
          }  /* for */
          /* Change any insignificant zeroes to blanks. */
          while (last_signif < --p) {
            *p = ' ';
            str_length--;
          }  /* while */
        }  /* if */
      }
      digits = digits_to_represent_with_underscore((unsigned long)str_length,
                                                   old_form);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        store_digits_and_underscore((unsigned long)str_length, digits,
                                    old_form, store_at);
        store_at += digits;
        while (str_length > 0) {
          /* Move the string and recode non-alphanumeric characters. */
          char c = *str++;
          if (c == ' ') {
            /* A blank is an insignificant digit removed above. */
          } else {
            if (c == '-') {
              /* Use "n" to represent a minus sign. */
              c = 'n';
            } else if (c == '.') {
              /* Use "d" to represent a decimal point. */
              c = 'd';
            } else if (c == '+') {
              /* Use "p" to represent a plus sign. */
              c = 'p';
            }  /* if */
            *store_at++ = c;
            str_length--;
          }  /* if */
        }  /* while */
      }  /* if */
      break;
    case ck_address:
      /* Address.  Put out the name of the entity whose address is involved. */
      { a_variable_ptr       variable;
        a_boolean            is_member = FALSE;
        a_routine_ptr        routine;
        an_address_base_kind abkind;

        /* The offset can be non-zero is cases where a pointer to class was
           cast to a related class.  That's ignored in the output. */
        abkind = con->variant.address.kind;
#if CHECKING
        if (abkind == (an_address_base_kind)abk_constant) {
          internal_error("literal_representation: addr of const");
        }  /* if */
#endif /* CHECKING */
        /* Address of something other than a constant, i.e., a variable or
           routine.  The encoding is like
             4abcd <-- encoding for address of "abcd"
              ^^^^---- Name of entity.
             ^-------- Length of the name.
           This is compatible with cfront 3.0.1. */
        if (abkind == (an_address_base_kind)abk_variable) {
          variable = con->variant.address.variant.variable;
          if (variable->source_corresp.is_class_member ||
              variable->source_corresp.parent.namespace_ptr != NULL) {
            /* Static data member or namespace member variable. */
            is_member = TRUE;
            str_length = mangled_member_variable_name(variable, (char *)NULL);
          } else {
            /* Normal variable. */
            str = variable->source_corresp.name;
#if CHECKING
            if (str == NULL) {
              internal_error("literal_representation: addr of unnamed");
            }  /* if */
#endif /* CHECKING */
            str_length = strlen(str);
          }  /* if */
        } else {
#if CHECKING
          if (abkind != (an_address_base_kind)abk_routine) {
            internal_error("literal_representation: bad abkind");
          }  /* if */
#endif /* CHECKING */
          routine = con->variant.address.variant.routine;
          str_length = mangled_function_name(routine,
                                             /*suppress_param_encoding=*/TRUE,
                                             (char *)NULL);
        }  /* if */
        digits = digits_to_represent((unsigned long)str_length);
        literal_length = digits + str_length;
        if (store_at != NULL) {
          (void)sprintf(store_at, "%lu", (unsigned long)str_length);
          store_at += digits;
          if (abkind == (an_address_base_kind)abk_variable) {
            if (is_member) {
              /* Static data member or namespace member variable. */
              (void)mangled_member_variable_name(variable, store_at);
            } else {
              /* Normal variable. */
              (void)memcpy(store_at, str, size_t_arg(str_length));
            }  /* if */
          } else {
            (void)mangled_function_name(routine,
                                        /*suppress_param_encoding=*/TRUE,
                                        store_at);
          }  /* if */
          store_at += str_length;
        }  /* if */
      }
      break;
    case ck_ptr_to_member:
      /* Pointer to member:
         For pointers to data members, the offset value encoded as
         an integer:
           L212  <--- encoding for an offset of "12"
             ^^------ Literal value.
            ^-------- Length of the literal.
           ^--------- "L" indicates a number.
         For pointers to member functions, the __mptr triplet of
         values (delta, index, function or offset), encoded as follows:
           LM0_L2n1_1j
                    ^^- Function name, or alternatively "0" if the pointer
                        to member uses an offset (e.g., LM0_L11_0).
               ^^^^---- Index value, encoded as an integer.
             ^--------- Delta value.
           ^^---------- "LM" indicates a pointer to member function.
         This is compatible with cfront 3.0.1.  Note that "0" is always
         used for the offset, not the actual offset value.  This follows
         cfront.  The idea seems to be that "0" is really a way of saying
         "there is no function;" the offset value itself would not be
         of interest to a name demangler. */
      if (!con->variant.ptr_to_member.is_function_ptr) {
        /* Pointer to data member. */
        a_targ_ptrdiff_t delta;
        repr_for_ptr_to_data_member_constant(con, &delta);
        (void)sprintf(buffer, "%ld", (long)delta);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        digits = digits_to_represent_with_underscore((unsigned long)str_length,
                                                     old_form);
        literal_length = 1 + digits + str_length;
        if (store_at != NULL) {
          *store_at++ = 'L';
          store_digits_and_underscore((unsigned long)str_length, digits,
                                      old_form, store_at);
          store_at += digits;
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
        }  /* if */
      } else {
        /* Pointer to member function. */
        a_targ_ptrdiff_t delta, index, offset;
        a_routine_ptr    func;
        repr_for_ptr_to_member_function_constant(con, &delta, &index, &func,
                                                 &offset);
        literal_length = 2;  /* "LM" */
        if (store_at != NULL) {
          *store_at++ = 'L';
          *store_at++ = 'M';
        }  /* if */
        /* Delta value. */
        (void)sprintf(buffer, "%ld", (long)delta);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        literal_length += str_length;
        if (store_at != NULL) {
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
        }  /* if */
        /* Index value. */
        (void)sprintf(buffer, "%ld", (long)index);
        str = buffer;
        str_length = strlen(str);  /* Includes "-" sign if any. */
        digits = digits_to_represent_with_underscore((unsigned long)str_length,
                                                     old_form);
        literal_length += 2 + digits + str_length + 1;
        if (store_at != NULL) {
          *store_at++ = '_';
          *store_at++ = 'L';
          store_digits_and_underscore((unsigned long)str_length, digits,
                                      old_form, store_at);
          store_at += digits;
          (void)memcpy(store_at, str, size_t_arg(str_length));
          /* Use "n" to represent a minus sign. */
          if (*store_at == '-') *store_at = 'n';
          store_at += str_length;
          *store_at++ = '_';
        }  /* if */
        if (func != NULL) {
          /* Name of function.  Note that this is the unmangled name. */
          str = func->source_corresp.name;
          /* Determine the size of the name.  Stop on two underscores. */
          for (str_length = 0;
               str[str_length] != '\0' &&
                 (str[str_length] != '_' || str[str_length+1] != '_');
               str_length++) {}
          digits = digits_to_represent((unsigned long)str_length);
          literal_length += digits + str_length;
          if (store_at != NULL) {
            (void)sprintf(store_at, "%lu", (unsigned long)str_length);
            store_at += digits;
            (void)memcpy(store_at, str, size_t_arg(str_length));
            store_at += str_length;
          }  /* if */
        } else {
          /* Offset, always coded as "0". */
          literal_length++;
          if (store_at != NULL) *store_at++ = '0';
        }  /* if */
      }  /* if */
      break;
    case ck_template_param:
      /* This comes up when mangling the names for template entities using
         the modern mangling approach. */
      switch (con->variant.template_param.kind) {
        case tpck_param:
          /* A simple reference to a template parameter. */
          literal_length = mangled_encoding_for_template_parameter(
                              &con->variant.template_param.variant.coordinates,
                              store_at);
          if (store_at != NULL) store_at += literal_length;
          break;
        case tpck_expression:
          /* An expression involving template parameters. */
          literal_length = mangled_encoding_for_expression(
                                      con->variant.template_param.variant.expr,
                                      store_at);
          if (store_at != NULL) store_at += literal_length;
          break;
        case tpck_member:
          /* A member of a template parameter type, e.g., T::x. */
          literal_length = mangled_member_name(&con->source_corresp,
                                               /*is_specialization=*/FALSE,
                                               store_at);
          if (store_at != NULL) store_at += literal_length;
          break;
        case tpck_cast:
          literal_length = mangled_encoding_for_constant_cast(
                                  con->type,
                                  con->variant.template_param.variant.constant,
                                  store_at);
          if (store_at != NULL) store_at += literal_length;
          break;
        case tpck_sizeof:
        case tpck_alignof:
        default:
          unexpected_condition_str(
                            "literal_representation: bad template param kind");
      }  /* switch */
      break;
#if CHECKING
    case ck_string:
      /* Strings should be converted to addresses. */
    default:
      internal_error("literal_representation: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  return literal_length;
}  /* literal_representation */


static sizeof_t mangled_encoding_for_constant(a_constant_ptr con,
                                              a_boolean      old_form,
                                              char           *store_at)
/*
Put out the mangled encoding for a constant at *store_at if store_at != NULL,
and (always) return the length of the mangled form.   If old_form is TRUE,
use the old form of length specification in the mangling for lengths of
literals.
*/
{
  sizeof_t mangled_form_length = 0, section_length;

  /* Representation is something like
       CiL15   <-- integer constant 5
           ^-- Literal constant representation.
          ^--- Length of literal constant.
         ^---- L indicates literal constant; c indicates address
               of variable, etc.
       ^^----- Type of constant, with "const" added.
     If the constant is a template parameter constant, skip the "C" and
     the type. */
  if (con->kind != (a_constant_repr_kind)ck_template_param ||
      con->variant.template_param.kind ==
                                   (a_template_param_constant_kind)tpck_cast) {
    mangled_form_length++;
    if (store_at != NULL) *store_at++ = 'C';
    /* Put out the constant type. */
    section_length = mangled_encoding_for_type(con->type, store_at);
    mangled_form_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  /* Put out the literal representation for the constant. */
  section_length = literal_representation(con, old_form, store_at);
  mangled_form_length += section_length;
  if (store_at != NULL) store_at += section_length;
  return mangled_form_length;
}  /* mangled_encoding_for_constant */


static sizeof_t mangled_encoding_for_expression(an_expr_node_ptr expr,
                                                char             *store_at)
/*
Place a mangled representation of the expression pointed to by expr at
*store_at if store_at != NULL, and (always) return the length of the mangled
form.  These expressions come up in ck_template_param expressions as
template arguments, and as dimensions of arrays in template signatures.
*/
{
  sizeof_t         mangled_expr_length, section_length;
  char             *operation_name;
  an_expr_node_ptr operand;
  unsigned long    num_operands;

  switch (expr->kind) {
    case enk_constant:
      mangled_expr_length = mangled_encoding_for_constant(
                                                        expr->variant.constant,
                                                        /*old_form=*/FALSE,
                                                        store_at);
      break;
    case enk_operation:
      /* Operation.  Output has the form
           Opl2Z1ZZ2ZO <-- "Z1 + Z2", Z1/Z2 indicating nontype template
                           parameters.
                     ^---- "O" to end the operation encoding.
                  ^^^----- Second operand.
               ^^^-------- First operand.
              ^----------- Count of operands.
            ^^------------ Operation, using same encoding as for operator
                           function names.
           ^-------------- "O" for operation.
         mangled_encoding_for_constant_cast generates a compatible structure,
         so if you change this be sure to change that as well.
      */
      /* Put out the initial "O". */
      mangled_expr_length = 1;
      if (store_at != NULL) *store_at++ = 'O';
      /* Get the operator name and put it out. */
      operation_name= mangled_expr_operator_name(expr->variant.operation.kind);
      section_length = strlen(operation_name);
      mangled_expr_length += section_length;
      if (store_at != NULL) {
        (void)strcpy(store_at, operation_name);
        store_at += section_length;
      }  /* if */
      /* For a cast, put out the type cast to. */
      if (operation_name[0] == 'c' && operation_name[1] == 's') {
        section_length = mangled_encoding_for_type(expr->type, store_at);
        mangled_expr_length += section_length;
        if (store_at != NULL) store_at += section_length;
      }  /* if */
      /* Put out the count of operands. */
      for (num_operands = 0, operand = expr->variant.operation.operands;
           operand != NULL;
           num_operands++, operand = operand->next) {}
      section_length = digits_to_represent(num_operands);
      mangled_expr_length += section_length;
      if (store_at != NULL) {
        (void)sprintf(store_at, "%lu", (unsigned long)num_operands);
        store_at += section_length;
      }  /* if */
      /* Put out the operands. */
      for (operand = expr->variant.operation.operands;
           operand != NULL;
           operand = operand->next) {
        section_length = mangled_encoding_for_expression(operand, store_at);
        mangled_expr_length += section_length;
        if (store_at != NULL) store_at += section_length;
      }  /* for */
      /* Put out the final "O". */
      mangled_expr_length++;
      if (store_at != NULL) *store_at++ = 'O';
      break;
    default:
      unexpected_condition_str("mangled_encoding_for_expression: bad kind");
  }  /* switch */
  return mangled_expr_length;
}  /* mangled_encoding_for_expression */


/*
Seed number for unnamed class names.
*/
static unsigned long
		unnamed_class_name_seed;


static void give_unnamed_class_a_name(a_type_ptr type)
/*
If the indicated class type is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The class is unnamed, so make up a name. */
    /* The name is __Cnn, where nn is a unique number for the
       class.  This is not from the ARM.  cfront uses the __Cn form, but
       the number is different. */
    unnamed_class_name_seed++;
    name_len = digits_to_represent(unnamed_class_name_seed) + 4; /*"__C"+null*/
    name = alloc_lowered_name_string(name_len);
    (void)sprintf(name, "__C%lu", (unsigned long)unnamed_class_name_seed);
    type->source_corresp.name = name;
  }  /* if */
}  /* give_unnamed_class_a_name */


/*
Seed number for unnamed namespace names.
*/
static unsigned long
		unnamed_namespace_name_seed;


static void give_unnamed_namespace_a_name(a_namespace_ptr nsp)
/*
If the indicated namespace is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  /* Note that we may be changing a namespace that is not being lowered yet,
     but that's okay -- the name in the IL entry is not used by the front
     end. */
  if (nsp->source_corresp.name == NULL) {
    /* The namespace is unnamed, so make up a name. */
    /* The name is __Nnn, where nn is a unique number for the
       namespace.  This is not from the ARM or cfront. */
    unnamed_namespace_name_seed++;
    name_len = digits_to_represent(unnamed_namespace_name_seed) + 4;
                                                                 /*"__N"+null*/
    name = alloc_lowered_name_string(name_len);
    (void)sprintf(name, "__N%lu", (unsigned long)unnamed_namespace_name_seed);
    nsp->source_corresp.name = name;
  }  /* if */
}  /* give_unnamed_namespace_a_name */


/*
Seed number for unnamed enum names.
*/
static unsigned long
		unnamed_enum_name_seed;


static void give_unnamed_enum_a_name(a_type_ptr type)
/*
If the indicated enum type is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The enum is unnamed, so make up a name. */
    /* The name is __Enn, where nn is a unique number for the
       enum.  This is not from the ARM.  cfront uses the __En form, but
       the number is different. */
    unnamed_enum_name_seed++;
    name_len = digits_to_represent(unnamed_enum_name_seed) + 4; /*"__E"+null*/
    name = alloc_lowered_name_string(name_len);
    (void)sprintf(name, "__E%lu", (unsigned long)unnamed_enum_name_seed);
    type->source_corresp.name = name;
  }  /* if */
}  /* give_unnamed_enum_a_name */


/*
Seed number for unnamed member variable names.
*/
static unsigned long
		unnamed_member_variable_name_seed;


static void give_unnamed_member_variable_a_name(a_variable_ptr nsp)
/*
If the indicated member variable is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;

  if (nsp->source_corresp.name == NULL) {
    /* The member variable is unnamed, so make up a name. */
    /* The name is __Vnn, where nn is a unique number for the
       member variable.  This is not from the ARM or cfront. */
    unnamed_member_variable_name_seed++;
    name_len = digits_to_represent(unnamed_member_variable_name_seed) + 4;
                                                                 /*"__V"+null*/
    name = alloc_lowered_name_string(name_len);
    (void)sprintf(name, "__V%lu",
                  (unsigned long)unnamed_member_variable_name_seed);
    nsp->source_corresp.name = name;
  }  /* if */
}  /* give_unnamed_member_variable_a_name */


static sizeof_t mangled_template_arguments(
                                          a_template_arg_ptr template_arg_list,
                                          a_boolean          old_form,
                                          char               *store_at)
/*
Determine the mangled form of the template arguments given by
template_arg_list.  Place the output at *store_at if store_at != NULL,
and (always) return the length of the output.  If old_form is TRUE, use
the old form of length specification in the mangling for lengths of literals.
*/
{
  sizeof_t           mangled_name_length, digits, arg_length, total_arg_length;
  sizeof_t           con_length, type_length;
  a_template_arg_ptr tap;
  int                pass;

  /* The mangled form of template arguments is something like
       __pt__3_ii
               ^^--- Two template arguments of type int.
             ^------ Total length of template argument list string,
                     including the underscore.
         ^^--------- Fixed string, indicates "parameterized type".
  */
#define PT_STR "__pt__"
  mangled_name_length = sizeof(PT_STR) - 1;
  if (store_at != NULL) {
    (void)strcpy(store_at, PT_STR);
    store_at += sizeof(PT_STR) - 1;
  }  /* if */
#undef PT_STR
  /* Run through the template argument list, determining the representation
     for each argument.  The first time through, determine the size;
     the second, put out the string. */
  for (pass = 1; ; pass++) {
    total_arg_length = 0;
    for (tap = template_arg_list; tap != NULL; tap = tap->next) {
      if (tap->is_type) {
        /* Type argument. */
        if (pass == 1) {
          arg_length = mangled_encoding_for_type(tap->variant.type,
                                                 (char *)NULL);
        } else {
          type_length = mangled_encoding_for_type(tap->variant.type,
                                                  store_at);
          mangled_name_length += type_length;
          store_at += type_length;
        }  /* if */
      } else {
        /* Constant argument.  The encoding for the constant begins with
           an "X". */
        if (pass == 1) {
          arg_length = mangled_encoding_for_constant(tap->variant.constant,
                                                     old_form,
                                                     (char *)NULL) + 1;
        } else {
          mangled_name_length++;
          if (store_at != NULL) *store_at++ = 'X';
          con_length = mangled_encoding_for_constant(tap->variant.constant,
                                                     old_form,
                                                     store_at);
          mangled_name_length += con_length;
          store_at += con_length;
        }  /* if */
      }  /* if */
      if (pass == 1) total_arg_length += arg_length;
    }  /* for */
    /* After the second pass, quit the loop. */
    if (pass == 2) break;
    /* First pass: */
    /* Put out the length of the entire argument section, and the "_". */
    total_arg_length++;  /* "_" */
    digits = digits_to_represent((unsigned long)total_arg_length);
    mangled_name_length += 1 + digits;
    if (store_at != NULL) {
      (void)sprintf(store_at, "%lu_", (unsigned long)total_arg_length);
      store_at += digits + 1;
    }  /* if */
    if (store_at == NULL) {
      /* If we are not storing, we do not need to do the second pass. */
      mangled_name_length += total_arg_length - 1;
      break;
    }  /* if */
  }  /* for */
  return mangled_name_length;
}  /* mangled_template_arguments */


static sizeof_t mangled_specialization_indication(char *store_at)
/*
Add a qualifier that indicates specialization.  Place it at *store_at if
store_at != NULL, and (always) return its length.
*/
{
#define SPEC_INDIC "__S"
  if (store_at != NULL) (void)strcpy(store_at, SPEC_INDIC);
  return sizeof(SPEC_INDIC) - 1;
#undef SPEC_INDIC
}  /* mangled_specialization_indication */


static sizeof_t mangled_full_class_name(
                                       a_type_ptr type,
                                       a_boolean  show_template_specialization,
                                       a_boolean  show_specialization,
                                       char       *store_at)
/*
Determine the mangled form of the name of the class "type".  This is
not the version that contains a leading count of the number of characters
in the name; here, the name is usually just the original name, but is
different if the class is a template class or is unnamed.  Also, this
routine does not do anything special with nested types.  Place the mangled
name at *store_at if store_at != NULL, and (always) return the length of
the name.  show_template_specialization is TRUE if the class is generated
from a specialization of a template and an indication of that fact should be
put out.  show_specialization is TRUE if the class is itself a specialization
and an indication of that fact should be put out.
*/
{
  sizeof_t           mangled_name_length, section_length;
  char               *name;
  a_class_type_supplement_ptr
                     ctsp = type->variant.class_struct_union.extra_info;
  a_boolean          previously_mangled_version_used = FALSE;

  /* This routine shouldn't be called after the nested type mangling has been
     done. */
  check_assertion_str(!type->source_corresp.nested_type_mangling_has_been_done,
                 "mangled_full_class_name: nested type mangling done already");
  /* Always start with the name of the class, which applies even in the
     template class case. */
  name = type->source_corresp.name;
  if (type->source_corresp.assoc_info != NULL) {
    /* See if this class is a proxy class for a template parameter.  If so,
       Use the template parameter name. */
    a_type_ptr template_param =
             symbol_supplement_for_class(type)->template_param_for_proxy_class;
    if (template_param != NULL) {
      name = template_param->source_corresp.name;
    }  /* if */
  }  /* if */
  if (name == NULL) {
    give_unnamed_class_a_name(type);
    name = type->source_corresp.name;
  }  /* if */
  if (type->source_corresp.name_has_been_mangled) {
    /* The name is already mangled, including any template parameters.
       We can use the mangled form unless we need to add specialization
       indicators, which are not present in the saved mangled form. */
    if (!show_template_specialization && !show_specialization) {
      previously_mangled_version_used = TRUE;
    } else {
      name = type->source_corresp.unmangled_name;
    }  /* if */
  }  /* if */
  mangled_name_length = strlen(name);
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(mangled_name_length));
    store_at += mangled_name_length;
  }  /* if */
  if (!previously_mangled_version_used) {
    if (show_template_specialization) {
      /* Put out an indication of the fact the template from which this
         class is generated is specialized. */
      section_length = mangled_specialization_indication(store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    if (ctsp->template_arg_list != NULL) {
      /* A template class.  Add information on template arguments. */
      /* old_form=TRUE forces use of the cfront-compatible mangling convention
         for lengths on literals, which though ambiguous is okay here because
         the class cannot be followed by an "_". */
      section_length = mangled_template_arguments(ctsp->template_arg_list,
                                                  /*old_form=*/TRUE,
                                                  store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    if (show_specialization) {
      /* Put out an indication of the fact that this class is specialized. */
      section_length = mangled_specialization_indication(store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    /* If the class is a local class, put out "__Lnn" using the declaration
       scope number for "nn".  This is not from the ARM.  cfront uses a
       similar form but it also includes the function mangling in the name
       and the number is probably different. */
    /* Don't do this for nested classes. */
    if (type->source_corresp.is_local_to_function &&
        !type->source_corresp.is_class_member) {
      /* This is a local name. */
      a_symbol_ptr assoc_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      sizeof_t digits =
                     digits_to_represent((unsigned long)assoc_sym->decl_scope);
      mangled_name_length += digits + 3;  /* "__L" */
      if (store_at != NULL) {
        (void)sprintf(store_at, "__L%lu",
                      (unsigned long)assoc_sym->decl_scope);
        store_at += digits + 3;
      }  /* if */
    }  /* if */
  }  /* if */
  return mangled_name_length;
}  /* mangled_full_class_name */


/*
Interface to mangled_full_class_name for the case where
show_template_specialization and show_specialization are FALSE (meaning no
information about those things should be put out).
*/
#define mangled_basic_class_name(type, store_at)                      \
  mangled_full_class_name((type), FALSE, FALSE, (store_at))


static sizeof_t r_mangled_parent_qualifier(
                                         a_source_correspondence *scp,
                                         unsigned long           nesting_level,
                                         char                    *store_at)
/*
Determine the parent qualifier needed in the mangled name for a member of
a class or namespace whose source correspondence is pointed to by scp.
Place it at *store_at if store_at != NULL, and (always) return the length
of the parent qualifier.  nesting_level is used to track recursive calls
of this routine to deal with multiple levels of parents.  nesting_level == 1
refers to the innermost qualifier of a type, nesting_level == 2 is the
next level out, etc.  See the macro mangled_parent_qualifier, which supplies
the usual nesting_level == 1.
*/
{
  a_source_correspondence *parent_scp;
  sizeof_t                mangled_name_length = 0, name_length, section_length;
  sizeof_t                digits;
  a_boolean               more_levels;

  /* See if the present level is nested inside some other class or
     namespace. */
  if (scp->is_class_member) {
    parent_scp = &scp->parent.class_type->source_corresp;
  } else {
    check_assertion(scp->parent.namespace_ptr != NULL);
    parent_scp = &scp->parent.namespace_ptr->source_corresp;
  }  /* if */
  more_levels = (parent_scp->is_class_member ||
                 parent_scp->parent.namespace_ptr != NULL);
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  /* If this a nested type name promoted into the file scope in
     cfront 2.1 mode, do not use the nested form. */
  if (scp->is_class_member &&
      scp->parent.class_type->
                           use_cfront_transitional_nested_type_name_mangling) {
    more_levels = FALSE;
  }  /* if */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  if (more_levels) {
    /* This level is nested inside something else.  Do a recursive call to
       deal with all of the parents. */
    section_length = r_mangled_parent_qualifier(parent_scp, nesting_level + 1,
                                                store_at);
    mangled_name_length = section_length;
    if (store_at != NULL) store_at += section_length;
  } else {
    /* This is the topmost qualifier. */
    if (nesting_level > 1) {
      /* More than one level of nesting, so use the ARM (7.2.1c) encoding
         for nested class names, like "outer::inner", using a "Q" description:
           Q2_5outer5inner
              ^-----^-----mangled class names, outer to inner
            ^----count of levels of qualification
         Note that the ARM description does not include the underscore, which
         is necessary if you allow more than 9 levels of nesting.
         The same scheme is used for namespace names. */
      digits = digits_to_represent(nesting_level);
      mangled_name_length = 2 + digits;
      if (store_at != NULL) {
        /* Actually store the "Qn_". */
        (void)sprintf(store_at, "Q%lu_", nesting_level);
        store_at += 2 + digits;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Put the class or namespace name at this level into the mangled name. */
  /* The name is preceded by a count of the number of characters in
     the name. */
  if (scp->is_class_member) {
    /* Class name. */
    a_type_ptr type = scp->parent.class_type;
    a_boolean  is_specialization = FALSE;
    a_boolean  is_template_specialization = FALSE;
    if (distinct_mangling_for_templates) {
      /* When templates get distinct mangling from normal functions,
         information is included for specialization in parent classes. */
      /* See if the class comes from a template and that template is
         specialized. */
      a_symbol_ptr template_sym =
                             symbol_supplement_for_class(type)->class_template;
      if (template_sym != NULL) {
        /* This class is an instance of a template. */
        if (template_sym->variant.template_info->is_specific_definition) {
          /* The template is specialized. */
          is_template_specialization = TRUE;
        }  /* if */
      }  /* if */
      /* See if the class itself is specialized (but not with the old
         syntax). */
      if (type->variant.class_struct_union.is_specialized &&
          !type->variant.class_struct_union.specialized_with_old_syntax) {
        is_specialization = TRUE;
      }  /* if */
    }  /* if */
    name_length = mangled_full_class_name(type,
                                          is_template_specialization,
                                          is_specialization,
                                          (char *)NULL);
    digits = digits_to_represent((unsigned long)name_length);
    mangled_name_length += name_length + digits;
    if (store_at != NULL) {
      /* Actually store the name. */
      (void)sprintf(store_at, "%lu", (unsigned long)name_length);
      store_at += digits;
      store_at += mangled_full_class_name(type,
                                          is_template_specialization,
                                          is_specialization,
                                          store_at);
    }  /* if */
  } else {
    /* Namespace name. */
    a_namespace_ptr nsp = scp->parent.namespace_ptr;
    char            *name = nsp->source_corresp.name;
    if (name == NULL) {
      /* Unnamed namespace. */
      give_unnamed_namespace_a_name(nsp);
      name = nsp->source_corresp.name;
    }  /* if */
    /* Put out the namespace name preceded by the length of the name, e.g.,
       "NNN" --> "3NNN". */
    name_length = strlen(name);
    digits = digits_to_represent((unsigned long)name_length);
    mangled_name_length += name_length + digits;
    if (store_at != NULL) {
      /* Actually store the name. */
      (void)sprintf(store_at, "%lu", (unsigned long)name_length);
      store_at += digits;
      (void)memcpy(store_at, name, size_t_arg(name_length));
      store_at += name_length;
    }  /* if */
  }  /* if */
  return mangled_name_length;
}  /* r_mangled_parent_qualifier */


/*
Interface to r_mangled_parent_qualifier, to provide nesting_level == 1.
*/
#define mangled_parent_qualifier(parent, store_at)                    \
  r_mangled_parent_qualifier((parent), (unsigned long)1, (store_at))


/* Return TRUE if the indicated type needs a parent (class or namespace)
   qualifier. */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define type_needs_parent_qualifier(type)                             \
  ((type)->source_corresp.is_class_member ||                          \
   (type)->source_corresp.parent.namespace_ptr != NULL)
#else /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#define type_needs_parent_qualifier(type)                             \
  (((type)->source_corresp.is_class_member ||                         \
    (type)->source_corresp.parent.namespace_ptr != NULL) &&           \
   !type->use_cfront_transitional_nested_type_name_mangling)
#endif /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */


/*
The prefix put on the front of the type encoding for a nested type to get
the name placed in the nested type itself.
*/
#define PREFIX_ON_NESTED_TYPE_NAME "__"


static sizeof_t mangled_type_name(a_type_ptr type,
                                  char       *store_at)
/*
Determine the mangled form of the name of the type "type".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.  This routine is
used for named types (classes, enums, and typedefs) and for unnamed
classes and enums.  Nested types are encoded as such.
*/
{
  sizeof_t   mangled_name_length = 0, name_length;
  char       *name;
  sizeof_t   digits;

  if (type->source_corresp.nested_type_mangling_has_been_done) {
    /* The parent information has already been mangled into the type name,
       i.e., the name is already fully mangled.  Return the name after
       the prefix. */
    char *mangled_name = type->source_corresp.name +
                         sizeof(PREFIX_ON_NESTED_TYPE_NAME) - 1;
    mangled_name_length = strlen(mangled_name);
    if (store_at != NULL) {
      (void)strcpy(store_at, mangled_name);
      store_at += mangled_name_length;
    }  /* if */
  } else {
    if (type_needs_parent_qualifier(type)) {
      /* The type is a member of a class or namespace, so put out a qualifier.
         Note that the count starts at 2 because the type name itself is level
         1. */
      mangled_name_length = r_mangled_parent_qualifier(&type->source_corresp,
                                                       (unsigned long)2,
                                                       store_at);
      if (store_at != NULL) store_at += mangled_name_length;
    }  /* if */
    /* Put out the type name itself. */
    /* The mangled form of a type name is the type name with a length
         preceding it:
           AB          --> 2AB
           ABCDEFGHIJK --> 11ABCDEFGHIJK
    */
    if (is_immediate_class_type(type)) {
      /* Class name. */
      name_length = mangled_basic_class_name(type, (char *)NULL);
      digits = digits_to_represent((unsigned long)name_length);
      mangled_name_length += name_length + digits;
      if (store_at != NULL) {
        /* Actually store the name. */
        (void)sprintf(store_at, "%lu", (unsigned long)name_length);
        store_at += digits;
        store_at += mangled_basic_class_name(type, store_at);
      }  /* if */
    } else {
      /* Not a class name (typedef or enum). */
      name = type->source_corresp.name;
      if (name == NULL) {
        /* Unnamed entity. */
        check_assertion(is_enum_type(type));
        give_unnamed_enum_a_name(type);
        name = type->source_corresp.name;
      }  /* if */
      name_length = strlen(name);
      digits = digits_to_represent((unsigned long)name_length);
      mangled_name_length += name_length + digits;
      if (store_at != NULL) {
        /* Actually store the name. */
        (void)sprintf(store_at, "%lu", (unsigned long)name_length);
        store_at += digits;
        (void)memcpy(store_at, name, size_t_arg(name_length));
        store_at += name_length;
      }  /* if */
    }  /* if */
  }  /* if */
  return mangled_name_length;
}  /* mangled_type_name */


sizeof_t mangled_class_name(a_type_ptr type,
                            char       *store_at)
/*
Determine the mangled form of the name of the class "type".  This is
the encoding used for the name of the class as opposed to the encoding
for the class as a type (for example, it has no length preceding a
simple class name).  Place the mangled name at *store_at if
store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length;

  if (type_needs_parent_qualifier(type)) {
    /* For a nested class, use the nested type encoding for the class. */
    mangled_name_length = mangled_type_name(type, store_at);
  } else {
    /* For a non-nested class, use the simple form of the name (with
       no preceding length). */
    mangled_name_length = mangled_basic_class_name(type, store_at);
  }  /* if */
  return mangled_name_length;
}  /* mangled_class_name */


static sizeof_t mangled_encoding_for_type(a_type_ptr type,
                                          char       *store_at)
/*
Determine the mangled encoding for the type "type".  Place the encoding at
*store_at if store_at != NULL, and (always) return the length of the name.
See ARM 7.2.1c for name encoding.
*/
{
  a_type_ptr named_type, pm_base_type;
#if ABI_COMPATIBILITY_VERSION < 230
  a_type_ptr named_typedef = NULL;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  sizeof_t   mangled_name_length, section_length;
  char       *s;
  a_type_qualifier_set
             qualifiers;

  mangled_name_length = 0;
  /* Walk through any typerefs above the type.  Remember type qualifiers
     and skip down to the "real" underlying type. */
  qualifiers = 0;
  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    /* Remember type qualifiers encountered. */
    qualifiers |= type->variant.typeref.qualifiers;
#if ABI_COMPATIBILITY_VERSION < 230
    /* Remember the bottommost named typedef encountered. */
    if (type->source_corresp.name != NULL) named_typedef = type;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  }  /* for */
  /* Put out type qualifiers, if any. */
  if (qualifiers != 0) {
    section_length= mangled_encoding_for_type_qualifiers(qualifiers, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  /* See if the type is a named class or enum. */
  named_type = NULL;
  if (has_name(type) &&
      (is_immediate_class_type(type) || is_immediate_enum_type(type))) {
    /* Named class or enum type. */
    named_type = type;
#if ABI_COMPATIBILITY_VERSION < 230
  } else if (named_typedef != NULL && is_immediate_enum_type(type)) {
    /* Unnamed enum with a typedef above it.  Use the typedef name for the
       enum even though it's the name of a qualified version of the enum.
       In ABI versions >= 2.30, the processing for this was moved
       to decls.c for greater compatibility with cfront when
       CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE, and eliminated otherwise. */
    named_type = named_typedef;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  }  /* if */
  /* If the type is named, use the name. */
  if (named_type != NULL) {
    /* Put out the mangled form of the name, e.g., "2AB" for "AB". */
    section_length = mangled_type_name(named_type, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  } else {
    /* The type is not named, so develop a description string. */
    switch (type->kind) {
      case tk_error:
        /* This might come up in mangling names for template instantiation. */
        s = "?";
        break;
      case tk_void:
        s = "v";
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          /* Unnamed enum.  mangled_type_name will make up a name. */
          mangled_name_length += mangled_type_name(type, store_at);
          goto have_whole_mangled_name;
        }  /* if */
        if (type->variant.integer.wchar_t_type) {
          s = "w";
        } else if (type->variant.integer.bool_type) {
          s = "b";
        } else {
          switch (type->variant.integer.int_kind) {
            case ik_char:           s = "c";  break;
            case ik_signed_char:    s = "Sc"; break;
            case ik_unsigned_char:  s = "Uc"; break;
            case ik_short:          s = "s";  break;
            case ik_unsigned_short: s = "Us"; break;
            case ik_int:            s = "i";  break;
            case ik_unsigned_int:   s = "Ui"; break;
            case ik_long:           s = "l";  break;
            case ik_unsigned_long:  s = "Ul"; break;
#if LONG_LONG_ALLOWED
            case ik_long_long:      s = "L";  break;
            case ik_unsigned_long_long:
                                    s = "UL"; break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
            default:
              internal_error("mangled_encoding_for_type: bad int kind");
#endif /* CHECKING */
          }  /* switch */
        }  /* if */
        break;
      case tk_float:
        switch (type->variant.float_kind) {
          case fk_float:          s = "f";  break;
          case fk_double:         s = "d";  break;
          case fk_long_double:    s = "r";  break;
#if CHECKING
          default:
            internal_error("mangled_encoding_for_type: bad float kind");
#endif /* CHECKING */
        }  /* switch */
        break;
      case tk_pointer:
        if (type->variant.pointer.is_reference) {
          s = "R";
        } else {
          s = "P";
        }  /* if */
        /* More of this below -- the "P" or "R" is followed by the
           type pointed to/referenced. */
        break;
      case tk_ptr_to_member:
        /* Pointer to member.  int S::* is put out as M1Si. */
        /* Put out "M". */
        s = "M";
        /* More of this below -- the "M" is followed by the class name and
           the type pointed to. */
        break;
      case tk_array:
        s = "A";
        /* More of this below -- int[10] is put out as A10_i. */
        break;
      case tk_routine:
        /* Function.  Put out "F" and the argument types. */
        section_length = mangled_encoding_for_function_type(type,
                                                       /*do_return_type=*/TRUE,
                                                            store_at);
        mangled_name_length += section_length;
        if (store_at != NULL) store_at += section_length;
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name will make up a name. */
        mangled_name_length += mangled_type_name(type, store_at);
        goto have_whole_mangled_name;
      case tk_template_param:
        /* This comes up when mangling the names for template entities using
           the modern mangling approach. */
        switch (type->variant.template_param.kind) {
          case tptk_param:
            mangled_name_length += mangled_encoding_for_template_parameter(
                         &type->variant.template_param.extra_info->coordinates,
                         store_at);
            break;
          case tptk_member:
            /* Type selected from a template parameter type, e.g., T::x. */
            section_length = mangled_type_name(type, store_at);
            mangled_name_length += section_length;
            if (store_at != NULL) store_at += section_length;
            break;
          default:
            unexpected_condition_str(
                      "mangled_encoding_for_type: bad tk_template_param kind");
        }  /* if */
        goto have_whole_mangled_name;
#if CHECKING
      default:
        internal_error("mangled_encoding_for_type: bad type kind");
#endif /* CHECKING */
    }  /* switch */
    /* s is now set to a type description string to be output. */
    section_length = strlen(s);
    mangled_name_length += section_length;
    if (store_at != NULL) {
      (void)memcpy(store_at, s, size_t_arg(section_length));
      store_at += section_length;
    }  /* if */
    /* Do any processing needed after the description letter. */
    switch (type->kind) {
      case tk_pointer:
        /* Put out the type pointed to. */
        mangled_name_length +=
                          mangled_encoding_for_type(type->variant.pointer.type,
                                                    store_at);
        break;
      case tk_ptr_to_member:
        /* Put out the mangled name of the class for which this is a member
           pointer. */
        section_length = mangled_encoding_for_type(type->variant.ptr_to_member.
                                                       class_of_which_a_member,
                                                   store_at);
        mangled_name_length += section_length;
        if (store_at != NULL) store_at += section_length;
        pm_base_type = type->variant.ptr_to_member.type;
        if (is_function_type(pm_base_type)) {
          /* This is a pointer to member function.  Put out the type qualifiers
             (if any) on the member function type. */
          section_length = mangled_encoding_for_function_qualifiers(
                                                                  pm_base_type,
                                                                  store_at);
          mangled_name_length += section_length;
          if (store_at != NULL) store_at += section_length;
        }  /* if */
        /* Put out the type pointed to. */
        mangled_name_length += mangled_encoding_for_type(pm_base_type,
                                                         store_at);
        break;
      case tk_array:
        /* Put out the array size, an underscore, and then the element type,
           i.e., int[10] is put out as A10_i. */
        if (type->variant.array.is_variable_size_array) {
          /* Variable size arrays are possible when putting out function
             prototypes.  For that case the prefix is "A_". */
          check_assertion(distinct_mangling_for_templates);
          mangled_name_length++;
          if (store_at != NULL) *store_at++ = '_';
          /* Put out an encoding for the expression. */
          section_length = mangled_encoding_for_expression(
                                type->variant.array.variant.element_count_expr,
                                store_at);
          mangled_name_length += section_length;
          if (store_at != NULL) store_at += section_length;
        } else {
          /* Put out the (constant) number of elements. */
          section_length =
             digits_to_represent((unsigned long)type->variant.array.
                                                   variant.number_of_elements);
          mangled_name_length += section_length;
          if (store_at != NULL) {
            (void)sprintf(store_at, "%lu",
                          (unsigned long)type->
                                     variant.array.variant.number_of_elements);
            store_at += section_length;
          }  /* if */
        }  /* if */
        mangled_name_length++;
        if (store_at != NULL) *store_at++ = '_';
        /* Put out the element type. */
        mangled_name_length +=
            mangled_encoding_for_type(type->variant.array.element_type,
                                      store_at);
        break;
      default:;
        /* Many cases don't require any handling. */
    }  /* switch */
  }  /* if */
have_whole_mangled_name:      
  return mangled_name_length;
}  /* mangled_encoding_for_type */


static char *mangled_operator_name(an_opname_kind kind)
/*
Return the string used to indicate the indicated operator name in mangled
names.  The string does not have the leading "__" used in some cases.
*/
{
  char *name;

  switch (kind) {
    case onk_new:               /* "new" */
      name = "nw";
      break;
    case onk_delete:            /* "delete" */
      name = "dl";
      break;
    case onk_array_new:         /* "new[]" */
      name = "nwa";
      break;
    case onk_array_delete:      /* "delete[]" */
      name = "dla";
      break;
    case onk_plus:              /* "+" */
      name = "pl";
      break;
    case onk_minus:             /* "-" */
      name = "mi";
      break;
    case onk_star:              /* "*" */
      name = "ml";
      break;
    case onk_divide:            /* "/" */
      name = "dv";
      break;
    case onk_remainder:         /* "%" */
      name = "md";
      break;
    case onk_excl_or:           /* "^" */
      name = "er";
      break;
    case onk_ampersand:         /* "&" */
      name = "ad";
      break;
    case onk_or:                /* "|" */
      name = "or";
      break;
    case onk_compl:             /* "~" */
      name = "co";
      break;
    case onk_not:               /* "!" */
      name = "nt";
      break;
    case onk_assign:            /* "=" */
      name = "as";
      break;
    case onk_lt:                /* "<" */
      name = "lt";
      break;
    case onk_gt:                /* ">" */
      name = "gt";
      break;
    case onk_plus_assign:       /* "+=" */
      name = "apl";
      break;
    case onk_minus_assign:      /* "-=" */
      name = "ami";
      break;
    case onk_times_assign:      /* "*=" */
      name = "amu";
      break;
    case onk_divide_assign:     /* "/=" */
      name = "adv";
      break;
    case onk_remainder_assign:  /* "%=" */
      name = "amd";
      break;
    case onk_excl_or_assign:    /* "^=" */
      name = "aer";
      break;
    case onk_and_assign:        /* "&=" */
      name = "aad";
      break;
    case onk_or_assign:         /* "|=" */
      name = "aor";
      break;
    case onk_shift_left:        /* "<<" */
      name = "ls";
      break;
    case onk_shift_right:       /* ">>" */
      name = "rs";
      break;
    case onk_shift_right_assign:/* ">>=" */
      name = "ars";
      break;
    case onk_shift_left_assign: /* "<<=" */
      name = "als";
      break;
    case onk_eq:                /* "==" */
      name = "eq";
      break;
    case onk_ne:                /* "!=" */
      name = "ne";
      break;
    case onk_le:                /* "<=" */
      name = "le";
      break;
    case onk_ge:                /* ">=" */
      name = "ge";
      break;
    case onk_and_and:           /* "&&" */
      name = "aa";
      break;
    case onk_or_or:             /* "||" */
      name = "oo";
      break;
    case onk_plus_plus:         /* "++" */
      name = "pp";
      break;
    case onk_minus_minus:       /* "--" */
      name = "mm";
      break;
    case onk_comma:             /* "," */
      name = "cm";
      break;
    case onk_arrow_star:        /* "->*" */
      name = "rm";
      break;
    case onk_arrow:             /* "->" */
      name = "rf";
      break;
    case onk_function_call:     /* "()" */
      name = "cl";
      break;
    case onk_subscript:         /* "[]" */
      name = "vc";
      break;
#if CHECKING
    default:
      internal_error("mangled_operator_name: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return name;
}  /* mangled_operator_name */


static char *mangled_expr_operator_name(an_expr_operator_kind op)
/*
Return the string used to mangle the indicated expression operator.
This routine only needs to handle the operators that can be used in
expressions on nontype template parameters in function signatures.
*/
{
  char           *name = NULL;
  an_opname_kind opkind;

  switch (op) {
    case eok_inegate:
    case eok_fnegate:
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_not:
      opkind = (an_opname_kind)onk_not;
      break;
    case eok_cast:
    case eok_base_class_cast:
    case eok_derived_class_cast:
    case eok_pm_base_class_cast:
    case eok_pm_derived_class_cast:
    case eok_bool_cast:
      name = "cs";
      break;
    case eok_complement:
      opkind = (an_opname_kind)onk_compl;
      break;
    case eok_iadd:
    case eok_fadd:
      opkind = (an_opname_kind)onk_plus;
      break;
    case eok_isubtract:
    case eok_fsubtract:
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_imultiply:
    case eok_fmultiply:
      opkind = (an_opname_kind)onk_star;
      break;
    case eok_idivide:
    case eok_fdivide:
      opkind = (an_opname_kind)onk_divide;
      break;
    case eok_ieq:
    case eok_feq:
      opkind = (an_opname_kind)onk_eq;
      break;
    case eok_ine:
    case eok_fne:
      opkind = (an_opname_kind)onk_ne;
      break;
    case eok_igt:
    case eok_fgt:
      opkind = (an_opname_kind)onk_gt;
      break;
    case eok_ilt:
    case eok_flt:
      opkind = (an_opname_kind)onk_lt;
      break;
    case eok_ige:
    case eok_fge:
      opkind = (an_opname_kind)onk_ge;
      break;
    case eok_ile:
    case eok_fle:
      opkind = (an_opname_kind)onk_le;
      break;
    case eok_remainder:
      opkind = (an_opname_kind)onk_remainder;
      break;
    case eok_shiftl:
      opkind = (an_opname_kind)onk_shift_left;
      break;
    case eok_shiftr:
      opkind = (an_opname_kind)onk_shift_right;
      break;
    case eok_and:
      opkind = (an_opname_kind)onk_ampersand;
      break;
    case eok_or:
      opkind = (an_opname_kind)onk_or;
      break;
    case eok_xor:
      opkind = (an_opname_kind)onk_excl_or;
      break;
    case eok_land:
      opkind = (an_opname_kind)onk_and_and;
      break;
    case eok_lor:
      opkind = (an_opname_kind)onk_or_or;
      break;
    case eok_question:
      opkind = (an_opname_kind)onk_question;
      break;
    default:
      unexpected_condition_str("mangled_expr_operator_name: bad operator");
  }  /* switch */
  if (name == NULL) {
    /* Convert opkind to a name. */
    name = mangled_operator_name(opkind);
  }  /* if */
  return name;
}  /* mangled_expr_operator_name */


static sizeof_t mangled_function_name(a_routine_ptr routine,
                                      a_boolean     suppress_param_encoding,
                                      char          *store_at)
/*
Determine the mangled form of the name of the function "routine".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.
*/
{
  sizeof_t   mangled_name_length, section_length;
  char       *name;
  a_type_ptr conversion_type, routine_type;
  a_boolean  is_member, mangle_as_template, add_leading_underscores = FALSE;
  a_boolean  is_specialization = FALSE, is_template_specialization = FALSE;

  /* Most of the processing is done in mangled_encoding_for_function_type,
     but this routine handles:
       (1)  The output of the name of the function, followed by "__".
            For special member functions, a special name is used, e.g.,
            "__ct" for constructors.
       (2)  If the function is a member function, the name of the
            class pointed to, followed by
              (a) if the function is nonstatic, "C", "V", or "CV" if there
                  are type qualifiers on the "this" parameter type, or
              (b) if the function is static, "S".
     mangled_encoding_for_function_type is then called to do the rest of the
     processing.
  */
  routine_type = skip_typerefs(routine->type);
  /* See if the function should be mangled as a template.  In the modern C++
     language, template functions are mangled using the template arguments
     and the prototype for the function.  This allows overloading of function
     templates (the instances have the same function parameter types, but
     one can be chosen over the other based on whether it is more
     specialized). */
  mangle_as_template = (distinct_mangling_for_templates &&
                        routine->is_template_function);
  if (mangle_as_template) {
    /* See if the function comes from a template and that template is
       specialized. */
    a_symbol_ptr sym = (a_symbol_ptr)(routine->source_corresp.assoc_info);
    if (sym->variant.routine.instance_ptr != NULL) {
      /* This function is an instance of a template. */
      a_symbol_ptr template_sym =
                               sym->variant.routine.instance_ptr->template_sym;
      a_template_symbol_supplement_ptr tssp =
                                  template_supplement_for_symbol(template_sym);
      if (tssp->is_specific_definition) {
        /* The template is specialized. */
        is_template_specialization = TRUE;
      }  /* if */
      /* Use the type of the prototype routine from the template as the
         routine type for the rest of the mangling. */
      /* Note that "routine" is not updated. */
      routine_type = tssp->variant.function.routine->type;
      routine_type = skip_typerefs(routine_type);
    }  /* if */
    /* See if the function itself is specialized (but not with the old
       syntax). */
    if (routine->is_specialized && !routine->specialized_with_old_syntax) {
      is_specialization = TRUE;
    }  /* if */
  }  /* if */
  /* Put out the name of the function. */
  mangled_name_length = 0;
  if (routine->special_kind == (a_special_function_kind)sfk_none) {
    /* Normal name. */
    name = routine->source_corresp.name;
#if CHECKING
    if (name == NULL) {
      internal_error("mangled_function_name: unnamed routine");
    }  /* if */
#endif /* CHECKING */
  } else {
    /* Use a special name for the routine. */
    add_leading_underscores = TRUE;
    switch (routine->special_kind) {
      case sfk_constructor:
        name = "ct";
        break;
      case sfk_destructor:
        name = "dt";
        break;
      case sfk_conversion:
        name = "op";
        /* Type signature is put out below. */
        break;
      case sfk_operator:
        name = mangled_operator_name(routine->opname_kind);
        break;
#if CHECKING
      default:
        internal_error("mangled_function_name: bad special kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* if */
  /* Copy the name. */
  section_length = strlen(name);
  mangled_name_length += section_length;
  if (add_leading_underscores) mangled_name_length += 2;  
  if (store_at != NULL) {
    if (add_leading_underscores) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
    (void)memcpy(store_at, name, size_t_arg(section_length));
    store_at += section_length;
  }  /* if */
  /* For a conversion function, add the type signature. */
  if (routine->special_kind == (a_special_function_kind)sfk_conversion) {
    conversion_type = routine_type->variant.routine.return_type;
    section_length = mangled_encoding_for_type(conversion_type, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  if (mangle_as_template) {
    if (is_template_specialization) {
      /* Put out an indication of the fact the template from which this
         function is generated is specialized. */
      section_length = mangled_specialization_indication(store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    if (routine->template_arg_list != NULL) {
      /* Put out the template arguments. */
      section_length = mangled_template_arguments(routine->template_arg_list,
                                                  /*old_form=*/FALSE,
                                                  store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    if (is_specialization) {
      /* Put out an indication of the fact that this function is
         specialized. */
      section_length = mangled_specialization_indication(store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
  }  /* if */
  /* See if the function is a class member function or a member of a
     namespace. */
  is_member = (routine->source_corresp.is_class_member ||
               routine->source_corresp.parent.namespace_ptr != NULL);
  /* If we will be adding the class or namespace name or the parameter types,
     put out two underscores to separate the function name from the rest. */
  if (is_member || !suppress_param_encoding) {
    /* Add two underscores after the name. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  if (is_member) {
    /* Put out the name of the class or namespace of which this function
       is a member. */
    section_length = mangled_parent_qualifier(&routine->source_corresp,
                                              store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  if (!suppress_param_encoding) {
    a_boolean do_return_type;
    if (routine->source_corresp.is_class_member) {
      /* Class member function.  Put out the qualifiers on the member function
         type. */
      section_length = mangled_encoding_for_function_qualifiers(routine_type,
                                                                store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    /* Templates have their return types included. */
    do_return_type = mangle_as_template;
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* No return type on constructors or destructors. */
      do_return_type = FALSE;
    }  /* if */
    /* Output the function type, including the parameter types. */
    section_length = mangled_encoding_for_function_type(routine_type,
                                                        do_return_type,
                                                        store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  return mangled_name_length;
}  /* mangled_function_name */

#if AUTOMATIC_TEMPLATE_INSTANTIATION || DO_IL_LOWERING

static a_boolean function_name_mangling_needed(
                                        a_routine_ptr routine,
                                        a_boolean     *suppress_param_encoding)
/*
Return TRUE if the name of the indicated routine needs to be mangled.
If so, also return *suppress_param_encoding TRUE if the name should be
mangled without parameter encoding.
*/
{
  a_boolean mangling_needed = FALSE;

  *suppress_param_encoding = FALSE;
  /* All names except C external names must be mangled, because they might
     be overloaded.  All member function names must be mangled because
     they exist in a scope that does not exist in the C version of the
     program (of course, none of them have C external linkage, so no
     separate test is needed). */
  if (routine->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_external) {
    mangling_needed = TRUE;
  } else if (routine->special_kind != (a_special_function_kind)sfk_none) {
    /* Operator function names must be somewhat mangled even if they are
       not C++ external, because their names are not normal C names --
       they contain special characters, etc. */
    mangling_needed = TRUE;
    *suppress_param_encoding = TRUE;
  }  /* if */
  return mangling_needed;
}  /* function_name_mangling_needed */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION || DO_IL_LOWERING */
#if AUTOMATIC_TEMPLATE_INSTANTIATION

char *get_mangled_function_name(a_routine_ptr routine)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in temp_text_buffer but do not change the
name in the routine entry.
*/
{
  a_boolean suppress_param_encoding;
  sizeof_t  mangled_name_length, alloc_length;
  char      *mangled_name;

  /* The routine should not be unnamed. */
  mangled_name = routine->source_corresp.name;
  check_assertion(mangled_name != NULL);
  if (routine->source_corresp.name_has_been_mangled ||
      !function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* The name has already been mangled, or it doesn't need to be
       mangled, so just return it. */
  } else {
    /* Generate the mangled name in a buffer. */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_function_name(routine,
                                                suppress_param_encoding,
                                                (char *)NULL);
    /* Make sure we have enough space in temp_text_buffer. */
    alloc_length = mangled_name_length + 1;
    ensure_temp_text_buffer_space(alloc_length);
    mangled_name = temp_text_buffer;
    /* Create the name. */
    (void)mangled_function_name(routine, suppress_param_encoding,
                                mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
  }  /* if */
  return mangled_name;
}  /* get_mangled_function_name */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

static sizeof_t mangled_member_name(a_source_correspondence *scp,
                                    a_boolean               is_specialization,
                                    char                    *store_at)
/*
Determine the mangled form of the name of the class or namespace member
whose source correspondence is given by scp.  Place the mangled name
at *store_at if store_at != NULL, and (always) return the length of the
name.  See ARM 7.2.1c for name encoding.  This routine must be called
only for static data member variables, namespace member variables, and
class and namespace member constants.  is_specialization is TRUE if the
variable is a template static data member specialization.
*/
{
  sizeof_t mangled_name_length, section_length;
  char     *name;

  /* The mangled name of a static data member or member constant is the
     original name followed by two underscores followed by the mangled
     class name.  For example:
       AB::xy --> xy__2AB
     The same encoding is used for members of namespaces.
  */
  mangled_name_length = 0;
  name = scp->name;
#if CHECKING
  if (name == NULL) internal_error("mangled_member_name: unnamed member");
#endif /* CHECKING */
  /* Copy the name. */
  section_length = strlen(name);
  mangled_name_length += section_length;
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(section_length));
    store_at += section_length;
  }  /* if */
  if (distinct_mangling_for_templates && is_specialization) {
    /* Put out an indication of the fact that a static data member is
       specialized. */
    section_length = mangled_specialization_indication(store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  /* Add two underscores after the name. */
  mangled_name_length += 2;
  if (store_at != NULL) {
    *store_at++ = '_';
    *store_at++ = '_';
  }  /* if */
  /* Output the mangled parent name. */
  section_length = mangled_parent_qualifier(scp, store_at);
  mangled_name_length += section_length;
  if (store_at != NULL) store_at += section_length;
  return mangled_name_length;
}  /* mangled_member_name */


static sizeof_t mangled_member_variable_name(a_variable_ptr variable,
                                             char           *store_at)
/*
Determine the mangled form of the name of the member variable "variable"
(a static data member or namespace member variable).  Place the mangled name
at *store_at if store_at != NULL, and (always) return the length of the name.
See ARM 7.2.1c for name encoding.
*/
{
  a_boolean is_specialization;

  if (!has_name(variable)) {
    /* An anonymous union can cause an unnamed member of a namespace:
         namespace {
           static union {float bf;};
         }
    */
    check_assertion_str(!variable->source_corresp.is_class_member,
                        "mangled_member_variable_name: unnamed class member");
    give_unnamed_member_variable_a_name(variable);
  }  /* if */
  is_specialization = (variable->is_specialized &&
                       !variable->specialized_with_old_syntax);
  return mangled_member_name(&variable->source_corresp, is_specialization,
                             store_at);
}  /* mangled_member_variable_name */

#if AUTOMATIC_TEMPLATE_INSTANTIATION

char *get_mangled_static_data_member_name(a_variable_ptr variable)
/*
Get the mangled name for the indicated static data member, and return
a pointer to it.  If the variable name has not been mangled yet, create a
copy of the mangled name in temp_text_buffer but do not change the
name in the variable entry.
*/
{
  sizeof_t   mangled_name_length, alloc_length;
  char       *mangled_name;

  /* The variable should not be unnamed. */
  mangled_name = variable->source_corresp.name;
  check_assertion(mangled_name != NULL);
  if (variable->source_corresp.name_has_been_mangled) {
    /* The name has already been mangled, so just return it. */
  } else {
    /* Generate the mangled name in a buffer. */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_member_variable_name(variable,
                                                       (char *)NULL);
    /* Make sure we have enough space in temp_text_buffer. */
    alloc_length = mangled_name_length + 1;
    ensure_temp_text_buffer_space(alloc_length);
    mangled_name = temp_text_buffer;
    /* Create the name. */
    (void)mangled_member_variable_name(variable, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
  }  /* if */
  return mangled_name;
}  /* get_mangled_static_data_member_name */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

/* Exclude routines that are needed for IL lowering but not for name
   mangling in the absence of IL lowering. */
#if DO_IL_LOWERING
/* Declaration required because of forward reference: */
static void do_scope_other_name_mangling(a_scope_ptr scope);


static void mangle_class_name(a_type_ptr class_type)
/*
Mangle the name of the indicated class, if necessary.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  error_position = class_type->source_corresp.decl_position;
  if (class_type->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL &&
      !class_type->source_corresp.name_has_been_mangled) {
    /* Template class names must be mangled because otherwise all instances
       of the same class template have the same name. */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_basic_class_name(class_type, (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)mangled_basic_class_name(class_type, mangled_name);
    mangled_name[mangled_name_length] = '\0';
    /* Note that the mangled name is not put into the type until after it has
       been completely built, because the old name is used in building the
       mangled form. */
    class_type->source_corresp.unmangled_name= class_type->source_corresp.name;
    class_type->source_corresp.name = mangled_name;
    class_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_class_name */


static void do_type_list_class_name_mangling(a_type_ptr type_list)
/*
Do class name mangling for the types on the indicated type list and subscopes
thereunder.  Note that this does not include special processing for
nested class names.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, process it and its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      mangle_class_name(type);
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_type_list_class_name_mangling(class_scope->types);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_class_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
  }  /* for */
}  /* do_type_list_class_name_mangling */


static void do_scope_class_name_mangling(a_scope_ptr scope)
/*
Do name mangling for class names in the indicated scope (the file scope
or a namespace scope) and all subscopes in the file-scope memory region.
*/
{
  a_namespace_ptr nsp;

  /* Process the types in the scope. */
  do_type_list_class_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_class_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* do_scope_class_name_mangling */


static void do_class_name_mangling(void)
/*
Do name mangling for all class names.  Note that this does not include
special processing for nested class names.
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_class_name_mangling(il_header.primary_scope);
  /* Process local types by visiting the types on orphan lists. */
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    do_type_list_class_name_mangling(solhp->orphaned_types);
  }  /* for */
}  /* do_class_name_mangling */


static void mangle_member_constant_name(a_constant_ptr con)
/*
Mangle the name of the indicated member constant, if necessary.  con
is either an enumerator constant, a namespace member constant, or (as an
extension) a declared class member constant.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  if (!con->source_corresp.name_has_been_mangled) {
    error_position = con->source_corresp.decl_position;
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_member_name(&con->source_corresp,
                                              /*is_specialization=*/FALSE,
                                              (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)mangled_member_name(&con->source_corresp,
                              /*is_specialization=*/FALSE, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    con->source_corresp.unmangled_name = con->source_corresp.name;
    con->source_corresp.name = mangled_name;
    con->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_member_constant_name */


static void do_type_list_other_name_mangling(a_type_ptr type_list)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) for the types on the indicated type list and subscopes
thereunder.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      /* Make sure the type-as-subobject for a class gets the class name
         before it is changed, if it is a nested class name. */
      prelower_class_type(type);
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_scope_other_name_mangling(class_scope);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_other_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    } else if (is_immediate_enum_type(type) &&
               (type->source_corresp.is_class_member ||
                type->source_corresp.parent.namespace_ptr != NULL)) {
      /* Mangle the names of member enum constants. */
      a_constant_ptr enum_con;
      for (enum_con = type->variant.integer.enum_info.constant_list;
           enum_con != NULL;
           enum_con = enum_con->next) {
        mangle_member_constant_name(enum_con);
      }  /* for */
    }  /* if */
  }  /* for */
}  /* do_type_list_other_name_mangling */


static void mangle_function_name(a_routine_ptr routine)
/*
Mangle the name of the indicated function, if necessary.
*/
{
  a_boolean suppress_param_encoding;
  sizeof_t  mangled_name_length, alloc_length;
  char      *mangled_name;

  error_position = routine->source_corresp.decl_position;
  /* Compiler-generated routines have no name, and they are left alone. */
  if (routine->source_corresp.name != NULL &&
      !routine->source_corresp.name_has_been_mangled) {
    if (function_name_mangling_needed(routine, &suppress_param_encoding)) {
      /* Mangle the function name. */
      /* Determine how long the mangled name is. */
      mangled_name_length = mangled_function_name(routine,
                                                  suppress_param_encoding,
                                                  (char *)NULL);
      /* Allocate space for the mangled name and build it.  The old name is
         just thrown away. */
      alloc_length = mangled_name_length + 1;
      mangled_name = alloc_lowered_name_string(alloc_length);
      (void)mangled_function_name(routine, suppress_param_encoding,
                                  mangled_name);
      /* Store the final null. */
      mangled_name[mangled_name_length] = '\0';
      routine->source_corresp.unmangled_name = routine->source_corresp.name;
      routine->source_corresp.name = mangled_name;
      routine->source_corresp.name_has_been_mangled = TRUE;
    }  /* if */
  }  /* if */
}  /* mangle_function_name */


static void mangle_member_variable_name(a_variable_ptr variable)
/*
Mangle the name of the indicated static data member or namespace member
variable.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  if (!variable->source_corresp.name_has_been_mangled &&
      /* Do not mangle namespace members with extern "C" linkage. */
      variable->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_external) {
    error_position = variable->source_corresp.decl_position;
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_member_variable_name(variable, (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)mangled_member_variable_name(variable, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    variable->source_corresp.unmangled_name = variable->source_corresp.name;
    variable->source_corresp.name = mangled_name;
    variable->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_member_variable_name */


static void do_scope_other_name_mangling(a_scope_ptr scope)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) in the indicated scope and its subscopes.  The scope is
the file scope or a class scope.  If the scope is the file scope,
the orphan lists for function-local entities are also processed.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;
  a_constant_ptr  con;

  /* Visit all types. */
  do_type_list_other_name_mangling(scope->types);
  if (scope == il_header.primary_scope) {
    /* When processing the file scope, also process function-local types
       by processing the orphan lists. */
    a_scope_orphaned_list_header_ptr solhp;
    for (solhp = il_header.scope_orphaned_list_headers;
         solhp != NULL;
         solhp = solhp->next) {
      do_type_list_other_name_mangling(solhp->orphaned_types);
    }  /* for */
    /* Visit all constants.  This is generally useless, but there might be
       constants that were promoted out of a local class into the file
       scope. */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      if (con->source_corresp.is_class_member) {
        mangle_member_constant_name(con);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Visit all namespaces. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_other_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    mangle_function_name(routine);
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_class_struct_union ||
      scope->kind == (a_scope_kind)sck_namespace) {
    /* For a class or namespace scope, visit the member variables
       and constants. */
    /* Look for static data members/namespace member variables and mangle
       their names. */
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      mangle_member_variable_name(variable);
    }  /* for */
    /* Look for member constants (an extension in classes) and mangle their
       names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      mangle_member_constant_name(con);
    }  /* for */
  }  /* if */
}  /* do_scope_other_name_mangling */


static void mangle_nested_type_name(a_type_ptr type)
/*
Mangle the name of the indicated type, if it is nested.  This does special
processing for nested type names that must be delayed until all of the
other name mangling that might use the name is done.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  error_position = type->source_corresp.decl_position;
  if (type_needs_parent_qualifier(type) && has_name(type) &&
      !type->source_corresp.nested_type_mangling_has_been_done) {
    /* Nested type names must be mangled (because they exist in a scope
       that does not exist in the generated C code).  The mangled form
       is something like
         __Q2_1A1B
       The "Q2_1A1B" part is the normal representation for a mangled
       name, and the prefix makes it unique (i.e., makes it distinct
       from all user identifiers).  Similar mangling is used for members
       of namespaces (a different kind of "nested" type). */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_type_name(type, (char *)NULL) +
                          sizeof(PREFIX_ON_NESTED_TYPE_NAME) - 1;
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)strcpy(mangled_name, PREFIX_ON_NESTED_TYPE_NAME);
    (void)mangled_type_name(type,
                            mangled_name+sizeof(PREFIX_ON_NESTED_TYPE_NAME)-1);
    mangled_name[mangled_name_length] = '\0';
    /* Note that the mangled name is not put into the type until after it has
       been completely built, because the old name is used in building the
       mangled form. */
    if (!type->source_corresp.name_has_been_mangled) {
      type->source_corresp.unmangled_name = type->source_corresp.name;
    }  /* if */
    type->source_corresp.name = mangled_name;
    type->source_corresp.name_has_been_mangled = TRUE;
    type->source_corresp.nested_type_mangling_has_been_done = TRUE;
  }  /* if */
}  /* mangle_nested_type_name */


static void do_type_list_nested_type_name_mangling(a_type_ptr type_list)
/*
Do nested type name mangling for the types on the indicated type list
and subscopes thereunder.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope;

  /* Visit all types on the list. */
  for (type = type_list; type != NULL; type = type->next) {
    /* If the type is a class, do its scope. */
    if (is_immediate_class_type(type)) {
      a_class_type_supplement_ptr ctsp =
                                   type->variant.class_struct_union.extra_info;
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_type_list_nested_type_name_mangling(class_scope->types);
      }  /* if */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_nested_type_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
    }  /* if */
    /* Do name mangling on the type. */
    /* Note that the call here must be done after all subscopes have been
       visited; we don't want to change the name of a class until the
       classes nested within it have been processed. */
    mangle_nested_type_name(type);
  }  /* for */
}  /* do_type_list_nested_type_name_mangling */


static void do_scope_nested_type_name_mangling(a_scope_ptr scope)
/*
Do name mangling for all nested type names in the indicated scope (the
file scope or a namespace scope) and all subscopes in the file-scope
memory region.
*/
{
  a_namespace_ptr nsp;

  /* Process the types in the scope. */
  do_type_list_nested_type_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_nested_type_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
}  /* do_scope_nested_type_name_mangling */


static void do_nested_type_name_mangling(void)
/*
Do name mangling for all nested type names.  This must be done separately
from and later than normal type name mangling because the simple form
of the name must remain available for use in mangled names (e.g.,
virtual function table variable names).
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_nested_type_name_mangling(il_header.primary_scope);
  /* Process local types by visiting the types on orphan lists. */
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    do_type_list_nested_type_name_mangling(solhp->orphaned_types);
  }  /* for */
}  /* do_nested_type_name_mangling */


void do_all_name_mangling(void)
/*
Do any required name mangling.  This is called at the beginning of lowering of
the file scope.  It processes everything in the file scope and also
function-local entities that require mangling (they are accessed through the
orphan lists).
*/
{
  /* Mangle class names, not including special processing for nested
     class names. */
  do_class_name_mangling();
  /* Do function, namespace, and static data member name mangling. */
  do_scope_other_name_mangling(il_header.primary_scope);
  /* Mangle nested type names. */
  do_nested_type_name_mangling();
}  /* do_all_name_mangling */

#endif /* DO_IL_LOWERING */

#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY

static a_boolean base_class_of_same_name_exists(a_base_class_ptr orig_bcp)
/*
Return TRUE if in the base class list of which orig_bcp is a part there is
another base class with the same name.  Note that this is "same name," not
necessarily "same type."
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = orig_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp != orig_bcp) {
      char *bcp_name = bcp->type->source_corresp.name;
      char *orig_bcp_name = orig_bcp->type->source_corresp.name;
      if (bcp_name != NULL && orig_bcp_name != NULL &&
          strcmp(bcp_name, orig_bcp_name) == 0) {
        /* Found another base class with the same name. */
        same_name_exists = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* base_class_of_same_name_exists */

#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */

static sizeof_t mangled_derivation_name(a_derivation_step_ptr dsp,
                                        char                  *store_at)
/*
Determine the mangled form of the name of the indicated derivation.
This is used for the base class part of virtual function table names.
Place the mangled name at *store_at if store_at != NULL, and (always)
return the length of the name.
*/
{
  sizeof_t   mangled_name_length, name_length;
  a_type_ptr class_type;

  mangled_name_length = 0;
  /* The name must be put out backwards, so use recursion to get to the
     bottom of the list. */
  if (dsp->next != NULL) {
    mangled_name_length = mangled_derivation_name(dsp->next, store_at);
    if (store_at != NULL) store_at += mangled_name_length;
    /* Add two underscores to separate names. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  /* Put out the name on the first derivation step. */
  class_type = dsp->base_class->type;
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
  /* cfront doesn't encode nested class information in base class names.
     This doesn't work in general, because it is possible to have base
     classes with the same basic name and different qualified names (e.g.,
     "A" and "A::B").  cfront doesn't seem to work right on all the cases
     with repeated basic names, so it gets away with it.  We use the
     cfront-compatible mangling only if there is no other base class
     with the same name. */
  if (!base_class_of_same_name_exists(dsp->base_class)) {
    name_length = mangled_basic_class_name(class_type, store_at);
  } else
#endif /* ABI_COMPATIBILITY_VERSION >= 230  && ... */
  /* Do not insert code here -- this is the "else" of an "if". */
  {
    /* Note the use of mangled_class_name instead of mangled_vtbl_class_name
       because we do not want two lengths on the front of nested class
       names. */
    name_length = mangled_class_name(class_type, store_at);
  }
  mangled_name_length += name_length;
  if (store_at != NULL) store_at += name_length;
  return mangled_name_length;
}  /* mangled_derivation_name */


static a_boolean virtual_base_class_of_same_name_exists(
                                                      a_base_class_ptr dir_bcp)
/*
Return TRUE if in the base class list of which dir_bcp (a direct, nonvirtual
base class) is a part there is also a virtual base class of the same name.
*/
{
  a_boolean        same_name_exists = FALSE;
  a_base_class_ptr bcp;

  for (bcp = dir_bcp->derived_class->variant.class_struct_union.extra_info->
                                                                  base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->is_virtual && bcp->type == dir_bcp->type) {
      same_name_exists = TRUE;
      break;
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* virtual_base_class_of_same_name_exists */


static sizeof_t mangled_vtbl_base_class_name(a_base_class_ptr bcp,
                                             char             *store_at)
/*
Determine the mangled form of the name of a base class in a virtual
function table.  The name describes the base class given by bcp.  Place
the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.
*/
{
  sizeof_t              mangled_name_length, name_length, digits;
  a_derivation_step_ptr dsp;
  a_boolean             ambiguous_direct_base_class = FALSE;

  /* The form of the name is like
       4abcd
     or
       8abcd__ef  (this for base class "abcd" in "ef")
     For virtual base classes, or nonvirtual base classes within virtual
     base classes, the first step is directly to the virtual base class.
  */
  dsp = cast_derivation_path_of(bcp);
  /* Determine the length. */
  name_length = mangled_derivation_name(dsp, (char *)NULL);
  digits = digits_to_represent((unsigned long)name_length);
  mangled_name_length = digits + name_length;
  if (bcp->ambiguous && bcp->direct && !bcp->is_virtual &&
      virtual_base_class_of_same_name_exists(bcp)) {
    /* This base class is a direct nonvirtual base class and there is
       a virtual base class with the same name, so put a suffix on the name
       to distinguish it from the virtual base class.  We change the name of
       the direct nonvirtual base class rather than the other one because
       cfront eliminates the direct base class (and therefore its virtual
       function table instance too). */
    ambiguous_direct_base_class = TRUE;
#define AMB_SUFFIX "__A"
    mangled_name_length += sizeof(AMB_SUFFIX)-1;
  }  /* if */
  if (store_at != NULL) {
    /* Put out the name length and the name. */
    (void)sprintf(store_at, "%lu", (unsigned long)name_length);
    store_at += digits;
    (void)mangled_derivation_name(dsp, store_at);
    store_at += name_length;
    if (ambiguous_direct_base_class) {
      (void)strcpy(store_at, AMB_SUFFIX);
      store_at += sizeof(AMB_SUFFIX)-1;
    }  /* if */
#undef AMB_SUFFIX
  }  /* if */
  return mangled_name_length;
}  /* mangled_vtbl_base_class_name */


static sizeof_t mangled_vtbl_class_name(a_type_ptr type,
                                        char       *store_at)
/*
Determine the mangled form of the name of the class "type" for use in
a virtual function table name.  Place the mangled name at *store_at if
store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length;
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
  sizeof_t name_length, digits;

  /* cfront mode. */
  if (type_needs_parent_qualifier(type)) {
    /* The type is a nested type.  Add a length in front of the mangled
       form (e.g., "7Q2_1A1B" instead of "Q2_1A1B"). */
    name_length = mangled_type_name(type, (char *)NULL);
    digits = digits_to_represent((unsigned long)name_length);
    mangled_name_length = name_length + digits;
    if (store_at != NULL) {
      /* Actually store the name. */
      (void)sprintf(store_at, "%lu", (unsigned long)name_length);
      store_at += digits;
      store_at += mangled_type_name(type, store_at);
    }  /* if */
  } else {
    /* Not a nested type name; just put out the type encoding. */
    mangled_name_length = mangled_type_name(type, store_at);
  }  /* if */
#else /* ABI_COMPATIBILITY_VERSION < 230 || ... */
  /* In non-cfront mode, or in old ABI versions, just pass through to
     mangled_type_name. */
  mangled_name_length = mangled_type_name(type, store_at);
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
  return mangled_name_length;
}  /* mangled_vtbl_class_name */


sizeof_t mangled_vtbl_name(a_type_ptr       class_type,
                           a_base_class_ptr bcp,
                           char             *store_at)
/*
Determine the mangled form of the name of the virtual function table for
base class bcp of class class_type.  If bcp == NULL, the virtual
function table is for class_type itself.  Place the mangled name at
*store_at if store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length, section_length;

  /* Determine the mangled name.  It is
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
     The mangled-base-class-name is really a sort of pathname for the
     base class, giving the base class names from base to derived.
     For example, __vtbl__5X__X1__1B for base class X inside X1 inside
     a whole object of type B.
  */
#define VTBL_STR "__vtbl__"
  mangled_name_length = sizeof(VTBL_STR) - 1;
  if (store_at != NULL) {
    (void)memcpy(store_at, VTBL_STR, size_t_arg(mangled_name_length));
    store_at += mangled_name_length;
  }  /* if */
  if (bcp != NULL) {
    /* Add the base class name. */
    section_length = mangled_vtbl_base_class_name(bcp, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
    /* Add two underscores after the name. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  /* Add the derived class name. */
  section_length = mangled_vtbl_class_name(class_type, store_at);
  mangled_name_length += section_length;
  if (store_at != NULL) store_at += section_length;
  return mangled_name_length;
#undef VTBL_STR
}  /* mangled_vtbl_name */


static sizeof_t mangled_prefixed_type_encoding(char       *prefix,
                                               a_type_ptr type,
                                               char       *store_at)
/*
Make a mangled name consisting of the indicated prefix followed by
the mangled encoding for the indicated type.  Place the mangled name
at *store_at if store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length, section_length;

  /* Determine the length of the mangled name. */
  mangled_name_length = strlen(prefix);
  if (store_at != NULL) {
    (void)strcpy(store_at, prefix);
    store_at += mangled_name_length;
  }  /* if */
  /* Add the mangled name of the type. */
  section_length = mangled_encoding_for_type(type, store_at);
  mangled_name_length += section_length;
  if (store_at != NULL) store_at += section_length;
  return mangled_name_length;
}  /* mangled_prefixed_type_encoding */


sizeof_t mangled_typeinfo_name(a_type_ptr type,
                               char       *store_at)
/*
Determine the mangled form of the name of the typeinfo variable for
type "type".  Place the mangled name at *store_at if store_at != NULL,
and (always) return the length of the name.  A typeinfo variable is
used to describe runtime type information.
*/
{
  /* The mangled name looks like
       __T_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__T_", type, store_at);
}  /* mangled_typeinfo_name */


sizeof_t mangled_id_object_name(a_type_ptr type,
                                char       *store_at)
/*
Determine the mangled form of the name of the id object variable for
type "type".  Place the mangled name at *store_at if store_at != NULL,
and (always) return the length of the name.  The id object variable
is pointed to by the typeinfo variable used to provide runtime type
information.
*/
{
  /* The mangled name looks like
       __TID_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__TID_", type, store_at);
}  /* mangled_id_object_name */

#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE

void mangle_promoted_entity_name(a_source_correspondence *scp,
                                 a_routine_ptr           routine,
                                 a_scope_ptr             scope)
/*
scp points to the source correspondence field of an entity that is being
promoted out of the routine "routine" (or one of its block scopes) to
the file scope.  scope indicates the scope out of which the entity is
being promoted (a function or block scope).  Give the entity a mangled
name if necessary.  This routine is called only once for each entity,
and that is after normal name mangling has been done.
*/
{
  sizeof_t mangled_name_length, alloc_length, name_length, routine_name_length;
  sizeof_t scope_num_length;
  char     *mangled_name, *store_at;

  /* Leave the name alone if the entity is unnamed. */
  if (scp->name != NULL) {
    /* Name mangling is needed. */
    /* The encoding is the original name, two underscores, the mangled
       name of the routine, and "__Lnn", where "nn" is the scope number.
       Note that the routine name has not been mangled yet, but the
       entity's name has been (if it needs mangling). */
    check_assertion(!routine->source_corresp.name_has_been_mangled);
    name_length = strlen(scp->name);
    if (routine->source_corresp.name != NULL) {
      routine_name_length =
                       mangled_function_name(routine,
                                             /*suppress_param_encoding=*/FALSE,
                                             (char *)NULL);
    } else {
      /* Unnamed routine. */
      routine_name_length = 0;
    }  /* if */
    /* Determine the length of "__Lnn". */
    scope_num_length = digits_to_represent((unsigned long)scope->number) + 3;
    mangled_name_length = name_length + 2 + routine_name_length +
                          scope_num_length;
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)strcpy(mangled_name, scp->name);
    store_at = mangled_name + name_length;
    *store_at++ = '_';
    *store_at++ = '_';
    if (routine_name_length > 0) {
      (void)mangled_function_name(routine, /*suppress_param_encoding=*/FALSE,
                                  store_at);
      store_at += routine_name_length;
    }  /* if */
    (void)sprintf(store_at, "__L%lu", (unsigned long)scope->number);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    scp->unmangled_name = scp->name;
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_promoted_entity_name */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */


void name_lower_one_time_init(void)
/*
Do one-time initialization of variables related to name mangling.  (Variables
that need to be reinitialized with each new translation unit are handled in
name_lower_init.)
*/
{
  /* Save variables from lower_name.c that are needed for precompiled
     headers */
  if (exceptions_enabled && precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(unnamed_class_name_seed),
      pch_saved_var_array_elem(unnamed_namespace_name_seed),
      pch_saved_var_array_elem(unnamed_enum_name_seed),
      pch_saved_var_array_elem(unnamed_member_variable_name_seed),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* eh_lower_one_time_init */


void name_lower_init(void)
/*
Initialize static variables related to name mangling.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
  /* Static variable in lower_name.c: */
  unnamed_class_name_seed = 0;
  unnamed_namespace_name_seed = 0;
  unnamed_enum_name_seed = 0;
  unnamed_member_variable_name_seed = 0;
}  /* name_lower_init */

#endif /* NEED_NAME_MANGLING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
