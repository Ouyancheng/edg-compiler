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

#include "templates.h"

/*
Control block for mangling.
*/
typedef struct a_mangling_control_block *a_mangling_control_block_ptr;
typedef struct a_mangling_control_block {
  sizeof_t	length;
			/* Current length of the mangled name.  Note that
			   this differs from mangling_text_buffer->size in
			   that it does not count the blanks left as reserved
			   space for leading lengths, which will be removed
			   at the end of generating the name. */
  sizeof_t	num_leftover_spaces;
			/* Count of the extra leftover spaces described
			   above. */
  a_boolean	suppress_partial_spec_args;
			/* TRUE to suppress extra information on partial
			   specialization arguments. */
} a_mangling_control_block;


/*
Text buffer used for mangling.
*/
static a_text_buffer_ptr
		mangling_text_buffer;

/*
Second text buffer, needed when a recursive call to the mangling routines
is made, e.g., when generating a module id.
*/
static a_text_buffer_ptr
		second_mangling_text_buffer;


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl);
static void mangled_function_base_name(
                                      a_source_correspondence  *scp,
                                      a_special_function_kind  special_kind,
                                      an_opname_kind           opname_kind,
                                      a_type_ptr               conversion_type,
                                      a_mangling_control_block *mctl);
static void mangled_function_name(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              a_mangling_control_block *mctl);
static void mangled_member_variable_name(a_variable_ptr           variable,
                                         a_mangling_control_block *mctl);
static char *mangled_expr_operator_name(an_expr_operator_kind op);
static void mangled_encoding_for_expression(an_expr_node_ptr         expr,
                                            a_mangling_control_block *mctl);
static void mangled_member_name(a_source_correspondence  *scp,
                                a_boolean                is_specialization,
                                a_mangling_control_block *mctl);
static void mangled_encoding_for_constant(a_constant_ptr           con,
                                          a_boolean                old_form,
                                          a_mangling_control_block *mctl);
static char *compress_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl);
static void r_mangled_parent_qualifier(a_source_correspondence  *scp,
                                       unsigned long            nesting_level,
                                       a_mangling_control_block *mctl);
static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_mangling_control_block *mctl);
static a_boolean variable_name_mangling_needed(a_variable_ptr variable);

/*
Interface to r_mangled_parent_qualifier, to provide nesting_level == 1.
*/
#define mangled_parent_qualifier(parent, mctl)                        \
  r_mangled_parent_qualifier((parent), (unsigned long)1, (mctl))


static void clear_mangling_control_block(a_mangling_control_block_ptr mctl)
/*
Set the fields of the indicated mangling control block to default values.
*/
{
  mctl->length = 0;
  mctl->num_leftover_spaces = 0;
  mctl->suppress_partial_spec_args = FALSE;
}  /* clear_mangling_control_block */


static void start_mangling(a_mangling_control_block_ptr mctl)
/*
Do initialization for mangling one name.  This includes clearing
mangling_text_buffer and mctl.
*/
{
  clear_mangling_control_block(mctl);
  reset_text_buffer(mangling_text_buffer);
}  /* start_mangling */


static void add_to_mangled_name(char                         ch,
                                a_mangling_control_block_ptr mctl)
/*
Add the indicated character to the mangled name.
*/
{
  /* Count characters. */
  mctl->length++;
  add_char_to_text_buffer(mangling_text_buffer, ch);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_to_mangled_name */


static void add_str_to_mangled_name(char                         *str,
                                    a_mangling_control_block_ptr mctl)
/*
Add the indicated null-terminated string to the mangled name.
*/
{
  sizeof_t len = strlen(str);

  /* Count characters. */
  mctl->length += len;
  add_to_text_buffer(mangling_text_buffer, str, len);
  check_assertion(mctl->length + mctl->num_leftover_spaces ==
                                                   mangling_text_buffer->size);
}  /* add_str_to_mangled_name */


static char *end_mangling(a_source_correspondence      *scp,
                          a_boolean                    final,
                          a_mangling_control_block_ptr mctl)
/*
Do processing at the end of mangling a name, which is in the mangling
buffer.  At the least, this includes adding the final null character.
Return the address of the mangled name in the buffer.  If scp is
non-NULL, allocate a copy of the name in the IL memory region, and
update scp to point to it.  If final is TRUE, it's okay to do final
mangling, which may produce a name that can no longer be embedded in
other mangled names.
*/
{
  char *buffer;

  /* Add the final null. */
  add_to_mangled_name('\0', mctl);
  if (mctl->num_leftover_spaces) {
    /* This string contains some leftover spaces, the result of saving extra
       room for potentially large leading length indications.  Remove those
       spaces now. */
    char *src = mangling_text_buffer->buffer;
    char *dest = src;
    char ch;
    do {
      ch = *src++;
      if (ch != ' ') {
        *dest++ = ch;
      } else {
        /* Removing a space. */
        mangling_text_buffer->size--;
        mctl->num_leftover_spaces--;
      }  /* if */
    } while (ch != '\0');
    check_assertion_str(mctl->num_leftover_spaces == 0 &&
                        mangling_text_buffer->size == mctl->length,
                        "end_mangling: wrong nunber of leftover spaces");
  }  /* if */
  buffer = mangling_text_buffer->buffer;
  if (final) {
    /* Compress the mangled name to make it smaller. */
    buffer = compress_mangled_name((char *)NULL, scp, mctl);
    /* Truncate the mangled name if necessary. */
    buffer = truncate_mangled_name(buffer, scp, mctl);
  }  /* if */
  if (scp != NULL) {
    /* Allocate space for the mangled name and copy it. */
    char *mangled_name = alloc_lowered_name_string(mctl->length);
    (void)strcpy(mangled_name, buffer);
    /* Save the unmangled form of the name.  Do not save the unmangled
       name for a class that was originally unnamed and has been given a
       name. */
    if (!scp->name_has_been_mangled) {
      scp->unmangled_name = scp->name;
    }  /* if */
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
    scp->final_name_mangling_pending = !final;
  }  /* if */
  return buffer;
}  /* end_mangling */


static void add_number_to_mangled_name(unsigned long            value,
                                       a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
simple output -- just the digits of the value, with no additional
encoding.
*/
{
  char buffer[50];

  (void)sprintf(buffer, "%lu", value);
  add_str_to_mangled_name(buffer, mctl);
}  /* add_number_to_mangled_name */


/*
Information about a spot where space was reserved by
reserve_space_for_length for later use by fill_in_length to fill in
a leading length.
*/
typedef struct a_length_reservation {
  sizeof_t	start_position;
			/* The offset in the mangling_text_buffer of the
			   first character of the space reserved for insertion
			   of the length. */
  sizeof_t	start_length;
			/* The length of the mangled name (not counting the
			   leftover spaces, which will be removed later)
			   preceding the length, used to compute the length
			   of the text following. */
} a_length_reservation;


static void reserve_space_for_length(
                                  a_length_reservation     *length_reservation,
                                  a_mangling_control_block *mctl)
/*
Reserve some space in the mangled name so that we can insert a length
later.  Return information on the position of the reserved space in
*length_reservation.
*/
{
  int i;

  length_reservation->start_position = mangling_text_buffer->size;
  length_reservation->start_length = mctl->length;
  /* Leave room for lengths of up to 9,999,999. */
#define NUM_CHARS_RESERVED_FOR_LENGTH 7
  /* Fill the space with blanks, which cannot be part of a valid mangled
     name.  We'll overwrite some of those blanks with the actual length
     determined later.  The leftover blanks will be removed at the end of
     mangling. */
  for (i = 1; i <= NUM_CHARS_RESERVED_FOR_LENGTH; i++) {
    add_to_mangled_name(' ', mctl);
  }  /* for */
  mctl->length -= NUM_CHARS_RESERVED_FOR_LENGTH;
  mctl->num_leftover_spaces += NUM_CHARS_RESERVED_FOR_LENGTH;
}  /* reserve_space_for_length */


static void fill_in_length(a_length_reservation     *length_reservation,
                           a_mangling_control_block *mctl)
/*
Fill in the length of an item in the space previously reserved by a
call of reserve_space_for_length.  *length_reservation contains the
information returned from that call.
*/
{
  sizeof_t length, length_length;
  char     *length_pos;
  char     buffer[20];

  /* Determine the length, and the number of digits needed to
     represent the length. */
  length = mctl->length - length_reservation->start_length;
  (void)sprintf(buffer, "%lu", (unsigned long)length);
  length_length = strlen(buffer);
  if (length_length > NUM_CHARS_RESERVED_FOR_LENGTH) {
    catastrophe(ec_mangled_name_too_long);
  }  /* if */
  /* Determine the position of the start of the length in the buffer. */
  length_pos = mangling_text_buffer->buffer +
               length_reservation->start_position;
  /* Copy the length. */
  (void)memcpy(length_pos, buffer, size_t_arg(length_length));
  /* The characters overwritten are no longer leftover spaces. */
  mctl->length += length_length;
  mctl->num_leftover_spaces -= length_length;
}  /* fill_in_length */


static void mangled_encoding_for_type_qualifiers(
                                           a_type_qualifier_set     qualifiers,
                                           a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type qualifiers (if any)
in the set "qualifiers".
*/
{
  if (qualifiers & TQ_CONST) {
    add_to_mangled_name('C', mctl);
  }  /* if */
  if (qualifiers & TQ_VOLATILE) {
    add_to_mangled_name('V', mctl);
  }  /* if */
}  /* mangled_encoding_for_type_qualifiers */


static void mangled_encoding_for_parameter_types(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parameters of function
type "type".
*/
{
  a_routine_type_supplement_ptr rtsp;
  a_param_type_ptr              param, existing_param;
  unsigned long                 existing_param_num, num_matching_types;

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
  rtsp = type->variant.routine.extra_info;
  param = rtsp->param_type_list;
  if (param == NULL) {
    /* Void parameter list. */
    add_to_mangled_name('v', mctl);
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
            add_to_mangled_name('T', mctl);
          } else {
            /* More than one match, so use the "Nmn" form. */
            add_to_mangled_name('N', mctl);
            /* Output the "m" (repetition count). */
            add_number_to_mangled_name(num_matching_types, mctl);
          }  /* if */
          /* Output the "n" (existing parameter number). */
          add_number_to_mangled_name(existing_param_num, mctl);
          goto arg_done;
        }  /* if */
      }  /* for */
      /* The parameter type does not match any of the previous parameter
         types, so just put it out. */
      mangled_encoding_for_type(param->type, mctl);
arg_done:;
    }  /* for */
  }  /* if */
  /* Output the final "e" for an ellipsis. */
  if (rtsp->has_ellipsis) {
    add_to_mangled_name('e', mctl);
  }  /* if */
}  /* mangled_encoding_for_parameter_types */


static void mangled_encoding_for_function_type(
                                       a_type_ptr               type,
                                       a_boolean                do_return_type,
                                       a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the function type "type".
The return type of the function is encoded if do_return_type is TRUE.
*/
{
  check_assertion(type->kind == (a_type_kind)tk_routine);
  /* The encoding for a function type is "F" followed by the encoding
     for the parameter types.  mangled_function_name takes care of putting
     out additional information preceding the "F" if the function is a
     member function. */
  /* Start with the "F" indicating a function type. */
  add_to_mangled_name('F', mctl);
  if (c_and_cpp_function_types_are_distinct &&
      type->variant.routine.extra_info->routine_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
    /* The function type is marked as extern "C", and the distinction between
       extern "C" and extern "C++" is significant.  Put out a "K" to mark
       the function type as a C function. */
    add_to_mangled_name('K', mctl);
  }  /* if */
  /* Add the parameter types. */
  mangled_encoding_for_parameter_types(type, mctl);
  if (do_return_type) {
    /* Add the return type at the end, as "_" followed by the type. */
    add_to_mangled_name('_', mctl);
    mangled_encoding_for_type(type->variant.routine.return_type, mctl);
  }  /* if */
}  /* mangled_encoding_for_function_type */


static void mangled_encoding_for_function_qualifiers(
                                                a_type_ptr               type,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type qualifiers (if any)
on the member function type "type".
*/
{
  a_routine_type_supplement_ptr rtsp =
                              skip_typerefs(type)->variant.routine.extra_info;

  if (rtsp->this_class != NULL) {
    /* The function is a nonstatic member function. */
    /* Add any qualifiers on the "this" parameter type (actually, the type
       pointed to by the "this" parameter). */
    a_type_qualifier_set  qualifiers = rtsp->qualifiers;

    if (qualifiers != TQ_NONE) {
      mangled_encoding_for_type_qualifiers(qualifiers, mctl);
    }  /* if */
  } else {
    /* Static member function. */
    add_to_mangled_name('S', mctl);
  }  /* if */
}  /* mangled_encoding_for_function_qualifiers */


static void store_digits_and_underscore(unsigned long            value,
                                        a_boolean                old_form,
                                        a_mangling_control_block *mctl)
/*
Add the decimal representation of value to the mangled name.  This is
used for cases where the distinction between single-digit and multi-digit
cases needs to be indicated.  With old_form TRUE, the representation will
be simply "d" for single-digit cases, and "dd_" for multi-digit cases.
With old_form FALSE, the representation is "_dd_" regardless of the length.
*/
{
  if (old_form) {
    add_number_to_mangled_name(value, mctl);
    if (value > 9) add_to_mangled_name('_', mctl);
  } else {
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name(value, mctl);
    add_to_mangled_name('_', mctl);
  }  /* if */
}  /* store_digits_and_underscore */


static void mangled_encoding_for_template_parameter(
                                       a_template_param_coordinate *coordinate,
                                       a_template_arg_ptr          args,
                                       a_mangling_control_block    *mctl)
/*
Add to the mangled name the encoding for a template parameter with the
given coordinates.  args points to the template argument list (for a
template template parameter), if any.
*/
{
  check_assertion(distinct_template_signatures);
  /* The encoding is "ZnZ" for a first-level parameter, and "Zn_mZ" for
     a non-first-level parameter, with "n" the parameter number, and
     "m" the depth number.  The "Z" on the end is to avoid ambiguities
     when this construct is followed by something that begins with a
     number, e.g., when a template parameter in a function parameter
     list is followed by a class name. */
  add_to_mangled_name('Z', mctl);
  /* Put out the parameter position number. */
  add_number_to_mangled_name((unsigned long)coordinate->position, mctl);
  if (coordinate->depth != 1) {
    /* Put out "_depth". */
    add_to_mangled_name('_', mctl);
    add_number_to_mangled_name((unsigned long)coordinate->depth, mctl);
  }  /* if */
  if (args != NULL) {
    /* Put out template arguments of a template template parameter. */
    mangled_template_arguments(args,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               mctl);
  }  /* if */
  /* Put out the final "Z". */
  add_to_mangled_name('Z', mctl);
}  /* mangled_encoding_for_template_parameter */


static void mangled_encoding_for_constant_cast(a_type_ptr               type,
                                               a_constant_ptr           con,
                                               a_mangling_control_block *mctl)
/*
Add to the mangled name the mangled encoding for the constant "con" cast
to the type "type".
*/
{
  a_boolean cast_to_unknown =
             (type->kind == (a_type_kind)tk_template_param &&
              type->variant.template_param.kind ==
                    (a_template_param_type_kind)tptk_unknown);

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
  /* If the cast is to an unknown type, omit the cast and just put out the
     underlying constant. */
  if (!cast_to_unknown) {
    /* Put out the initial "O" followed by the operator name "cs". */
    add_str_to_mangled_name("Ocs", mctl);
    /* The operator name "cs" is followed by the encoding for the
       type cast to. */
    mangled_encoding_for_type(type, mctl);
    /* Put out the count of operands. */
    add_to_mangled_name('1', mctl);
  }  /* if */
  /* Put out the operand. */
  mangled_encoding_for_constant(con, /*old_form=*/FALSE, mctl);
  if (!cast_to_unknown) {
    /* Put out the final "O". */
    add_to_mangled_name('O', mctl);
  }  /* if */
}  /* mangled_encoding_for_constant_cast */


static void mangled_encoding_for_sizeof(a_type_ptr                     type,
                                        an_expr_node_ptr               expr,
                                        a_template_param_constant_kind kind,
                                        a_mangling_control_block       *mctl)
/*
Add to the mangled name the encoding of sizeof(type), __ALIGNOF__(type),
or __uuidof(type); kind indicates which.  If expr is non-NULL, the
original form used an expression, which expr points to.
*/
{
  /* Output has the form
       OszZ1Z0O <-- "sizeof(Z1)", Z1 indicating a template parameter.
              ^---- "O" to end the operation encoding.
             ^----- Count of operands, always 0 for sizeof.
          ^^^------ Encoding for type.
        ^^--------- Operation ("sz" for sizeof, "af" for __ALIGNOF__, or
                    "uu" for uuidof)
       ^----------- "O" for operation.
     mangled_encoding_for_expression generates a compatible structure, so
     if you change this be sure to change that as well.
     For an expression case, the type is replaced by "e"; there is still no
     expression.  (This is potentially a violation of the standard, since
     two templates that differ only in the expression under a sizeof could
     be mangled to the same name; however, doing mangling for full
     non-constant expressions -- think "delete[] x" -- would be quite
     a lot of additional work for very little gain.  We'll take this
     up with the standards committee.
  */
  /* Put out the initial "O". */
  add_to_mangled_name('O', mctl);
  /* Put out the operator name. */
  switch (kind) {
    case tpck_sizeof:
      add_str_to_mangled_name("sz", mctl);
      break;
    case tpck_alignof:
      add_str_to_mangled_name("af", mctl);
      break;
    case tpck_uuidof:
      add_str_to_mangled_name("uu", mctl);
      break;
    default:
      unexpected_condition();
  }  /* switch */
  /* The operator name is followed by the encoding for the type or the
     expression. */
  if (expr != NULL) {
    /* The expression form.  Put out "e" instead of the type. */
    add_to_mangled_name('e', mctl);
  } else {
    /* No expression, so put out the type. */
    mangled_encoding_for_type(type, mctl);
  }  /* if */
  /* Put out the count of operands (zero). */
  add_to_mangled_name('0', mctl);
  /* Put out the final "O". */
  add_to_mangled_name('O', mctl);
}  /* mangled_encoding_for_sizeof */


static void mangled_encoding_for_float_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_float constant con.
This is used to encode floating-point constants as part of the
mangled names of template classes.  If old_form is TRUE, use the old form
of length specification in the mangling for lengths of literals.
*/
{
  sizeof_t str_length;
  char     *str;

  /* Float: the encoding is like
       L4n1p5 <-- encoding for "-1.5"
          ^^^---- Literal value ("p" for decimal point).
         ^------- "n" indicates negative.
        ^-------- Length of the literal.
       ^--------- "L" indicates a number.
     cfront 3.0.1 does not implement this, so we made it up. */
  str = fp_to_string(skip_typerefs(con->type)->variant.float_kind,
                     &con->variant.float_value,
                     (a_boolean *)NULL, (a_boolean *)NULL, (a_boolean *)NULL);
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
      for (last_signif = ++p; isdigit((unsigned char)*p); p++) {
        if (*p != '0') last_signif = p;
      }  /* for */
      /* Change any insignificant zeroes to blanks. */
      while (last_signif < --p) {
        *p = ' ';
        str_length--;
      }  /* while */
    }  /* if */
  }
  add_to_mangled_name('L', mctl);
  store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
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
      add_to_mangled_name(c, mctl);
      str_length--;
    }  /* if */
  }  /* while */
}  /* mangled_encoding_for_float_constant */


static void mangled_encoding_for_address_constant(
                                                a_constant_ptr           con,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_address constant con.
This is used to encode address constants as part of the mangled names of
template classes.
*/
{
  char                 *str;
  an_address_base_kind abkind;
  a_length_reservation length_reservation;

  /* The offset can be non-zero in cases where a pointer to class was
     cast to a related class.  That's ignored in the output. */
  abkind = con->variant.address.kind;
  check_assertion_str(abkind != (an_address_base_kind)abk_constant,
                      "mangled_encoding_for_address_constant: abk_constant");
  /* Address of something other than a constant, i.e., a variable or
     routine.  The encoding is like
       4abcd <-- encoding for address of "abcd"
        ^^^^---- Name of entity.
       ^-------- Length of the name.
     This is compatible with cfront 3.0.1. */
  reserve_space_for_length(&length_reservation, mctl);
  if (abkind == (an_address_base_kind)abk_variable) {
    a_variable_ptr variable = con->variant.address.variant.variable;
    if (variable->source_corresp.is_class_member ||
        variable->source_corresp.parent.namespace_ptr != NULL) {
      /* Static data member or namespace member variable. */
      mangled_member_variable_name(variable, mctl);
    } else {
      /* Normal variable. */
      str = unmangled_name_of(&variable->source_corresp);
      check_assertion_str(str != NULL,
                     "mangled_encoding_for_address_constant: addr of unnamed");
      add_str_to_mangled_name(str, mctl);
    }  /* if */
  } else if (abkind == (an_address_base_kind)abk_routine) {
    a_routine_ptr routine = con->variant.address.variant.routine;
    mangled_function_name(routine, /*suppress_param_encoding=*/TRUE, mctl);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (abkind == (an_address_base_kind)abk_uuidof) {
    a_type_ptr uuid_type;
    char       *uuid_str;

    /* Microsoft __uuidof. */
    /* The uuid string attached to the associated type has the format
         hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
       (where "h" is a hexadecimal digit).  The mangled form is
       a length followed by "__UUID" followed by the string, with hyphens
       removed.  This is just made up; the Microsoft compiler uses a
       completely different mangling scheme, so compatibility is a moot
       point here. */
    add_str_to_mangled_name("__UUID", mctl);
    uuid_type = con->variant.address.variant.type;
    if (uuid_type == NULL) {
      /* Null GUID case. */
      uuid_str = "00000000-0000-0000-000000000000";
    } else {
      uuid_str = uuid_type->variant.class_struct_union.extra_info->uuid_string;
      if (uuid_str == NULL) {
        /* This can happen in error cases. */
        uuid_str = "00000000-0000-0000-000000000000";
      }  /* if */
    }  /* if */
    for (; *uuid_str != '\0'; uuid_str++) {
      if (*uuid_str != '-') add_to_mangled_name(*uuid_str, mctl);
    }  /* for */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    unexpected_condition_str(
                          "mangled_encoding_for_address_constant: bad abkind");
  }  /* if */
  fill_in_length(&length_reservation, mctl);
}  /* mangled_encoding_for_address_constant */


static void mangled_encoding_for_ptr_to_member_constant(
                                             a_constant_ptr           con,
                                             a_boolean                old_form,
                                             a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the ck_ptr_to_member constant con.
This is used to encode pointer-to-member constants as part of the mangled
names of template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.
*/
{
  sizeof_t str_length;
  char     *str;
  char     buffer[50];

  /* Pointer to member:
     For pointers to data members, the offset value encoded as an integer:
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
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_to_mangled_name('L', mctl);
    store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
    add_str_to_mangled_name(str, mctl);
  } else {
    /* Pointer to member function. */
    a_targ_ptrdiff_t delta, index, offset;
    a_routine_ptr    func;

    repr_for_ptr_to_member_function_constant(con, &delta, &index, &func,
                                             &offset);
    add_str_to_mangled_name("LM", mctl);
    /* Delta value. */
    (void)sprintf(buffer, "%ld", (long)delta);
    str = buffer;
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_str_to_mangled_name(str, mctl);
    /* Index value. */
    (void)sprintf(buffer, "%ld", (long)index);
    str = buffer;
    /* Use "n" to represent a minus sign. */
    if (str[0] == '-') str[0] = 'n';
    str_length = strlen(str);  /* Includes "-" sign if any. */
    add_str_to_mangled_name("_L", mctl);
    store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
    add_str_to_mangled_name(str, mctl);
    add_to_mangled_name('_', mctl);
    if (func != NULL) {
      a_length_reservation length_reservation;
      /* Name of function. */
      /* The newer version of this includes parent information, but that's
         not compatible with cfront. */
      a_boolean include_parent_info;
#if ABI_COMPATIBILITY_VERSION < 235
      include_parent_info = FALSE;
#else /* ABI_COMPATIBILITY_VERSION >= 235 */
      /* Making this conditional on the new-style mangling for templates
         is a little strange, but if you have the new-style mangling
         you're completely incompatible with cfront, so it's not
         a ridiculous idea. */
      include_parent_info = distinct_template_signatures;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
      reserve_space_for_length(&length_reservation, mctl);
      if (include_parent_info) {
        /* Include class and namespace information in the name. */
        mangled_function_name(func, /*suppress_param_encoding=*/TRUE, mctl);
      } else {
        /* Use a simple name (no class or namespace information). */
        str = unmangled_name_of(&func->source_corresp);
        check_assertion(str != NULL);
        /* Output the name.  Stop on two underscores. */
        for (str_length = 0;
             str[str_length] != '\0' &&
               (str[str_length] != '_' || str[str_length+1] != '_');
             str_length++) {
          add_to_mangled_name(str[str_length], mctl);
        }  /* for */
      }  /* if */
      fill_in_length(&length_reservation, mctl);
    } else {
      /* Offset, always coded as "0". */
      add_to_mangled_name('0', mctl);
    }  /* if */
  }  /* if */
}  /* mangled_encoding_for_ptr_to_member_constant */


static void mangled_encoding_for_unknown_function(
                                    a_constant_ptr           con,
                                    a_boolean                has_template_args,
                                    a_template_arg_ptr       template_arg_list,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con, which is
a ck_template_param/tpck_unknown_function constant.  This is used
to encode unknown functions that appear in template argument lists
of prototype instantiations.  If has_template_args is TRUE, the function
has an explicit template argument list, given by template_arg_list.
*/
{
  a_type_ptr              conversion_type =
                                         con->variant.template_param.variant.
                                              unknown_function.conversion_type;
  a_special_function_kind special_kind = (a_special_function_kind)sfk_none;

  /* This routine is a simplified version of mangled_function_name. */
  if (conversion_type != NULL) {
    special_kind = (a_special_function_kind)sfk_conversion;
  }  /* if */
  mangled_function_base_name(&con->source_corresp,
                             special_kind,
                             (an_opname_kind)onk_none,
                             conversion_type,
                             mctl);
  if (has_template_args) {
    /* Put out the template argument list. */
    mangled_template_arguments(template_arg_list,
                               /*partial_spec=*/FALSE,
                               /*old_form=*/FALSE,
                               mctl);
  }  /* if */
  if (con->source_corresp.is_class_member ||
      con->source_corresp.parent.namespace_ptr != NULL) {
    /* Add a parent qualifier for a member. */
    add_str_to_mangled_name("__", mctl);
    mangled_parent_qualifier(&con->source_corresp, mctl);
  }  /* if */
}  /* mangled_encoding_for_unknown_function */


static void literal_representation(a_constant_ptr           con,
                                   a_boolean                old_form,
                                   a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
This is used to encode constants as part of the mangled names of
template classes.  If old_form is TRUE, use the old form of length
specification in the mangling for lengths of literals.
*/
{
  sizeof_t            str_length;
  char                *str;
  a_boolean           has_template_args;
  a_template_arg_ptr  template_arg_list;
  a_constant_ptr      unk_func_con;

  switch (con->kind) {
    case ck_error:
      /* This might come up in mangling names for template instantiations. */
      add_to_mangled_name('?', mctl);
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
      /* Use "n" to represent a minus sign. */
      if (str[0] == '-') str[0] = 'n';
      str_length = strlen(str);  /* Includes "-" sign if any. */
      add_to_mangled_name('L', mctl);
      store_digits_and_underscore((unsigned long)str_length, old_form, mctl);
      add_str_to_mangled_name(str, mctl);
      break;
    case ck_float:
      /* Float constant. */
      mangled_encoding_for_float_constant(con, old_form, mctl);
      break;
    case ck_address:
      /* Address.  Put out the name of the entity whose address is involved. */
      mangled_encoding_for_address_constant(con, mctl);
      break;
    case ck_ptr_to_member:
      /* Pointer to member. */
      mangled_encoding_for_ptr_to_member_constant(con, old_form, mctl);
      break;
    case ck_template_param:
      /* This comes up when mangling the names for template entities using
         the modern mangling approach. */
      switch (con->variant.template_param.kind) {
        case tpck_param:
          /* A simple reference to a template parameter. */
          mangled_encoding_for_template_parameter(
                              &con->variant.template_param.variant.coordinates,
                              (a_template_arg *)NULL,
                              mctl);
          break;
        case tpck_expression:
          /* An expression involving template parameters. */
          mangled_encoding_for_expression(
                                      con->variant.template_param.variant.expr,
                                      mctl);
          break;
        case tpck_template_ref:
          /* An unknown function template with a list of explicit template
             arguments.  The template is given by an underlying
             tpck_unknown_function constant. */
          has_template_args = TRUE;
          template_arg_list = con->variant.template_param.variant.
                                                         template_ref.arg_list;
          unk_func_con = con->variant.template_param.variant.template_ref.con;
          check_assertion(unk_func_con->kind ==
                                     (a_constant_repr_kind)ck_template_param &&
                          unk_func_con->variant.template_param.kind ==
                        (a_template_param_constant_kind)tpck_unknown_function);
          goto do_unknown_function;
        case tpck_unknown_function:
          /* An unknown function, which may be a member of a class or
             namespace, and may be a conversion function (if conversion_type
             is non-NULL). */
          has_template_args = FALSE;
          template_arg_list = NULL;
          unk_func_con = con;
do_unknown_function:
          { a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
            mangled_encoding_for_unknown_function(unk_func_con,
                                                  has_template_args,
                                                  template_arg_list,
                                                  mctl);
            fill_in_length(&length_reservation, mctl);
          }
          break;
        case tpck_member:
          /* A member of a template parameter type, e.g., T::x. */
          { a_length_reservation length_reservation;
            reserve_space_for_length(&length_reservation, mctl);
            mangled_member_name(&con->source_corresp,
                                /*is_specialization=*/FALSE,
                                mctl);
            fill_in_length(&length_reservation, mctl);
          }
          break;
        case tpck_cast:
          mangled_encoding_for_constant_cast(
                                  con->type,
                                  con->variant.template_param.variant.constant,
                                  mctl);
          break;
        case tpck_address:
          /* For an address, just mangle the member name. */
          literal_representation(con->variant.template_param.variant.constant,
                                 old_form, mctl);
          break;
        case tpck_sizeof:
        case tpck_alignof:
        case tpck_uuidof:
          mangled_encoding_for_sizeof(
                         con->variant.template_param.variant.templ_sizeof.type,
                         con->variant.template_param.variant.templ_sizeof.expr,
                         con->variant.template_param.kind,
                         mctl);
          break;
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
}  /* literal_representation */


static void mangled_encoding_for_constant(a_constant_ptr           con,
                                          a_boolean                old_form,
                                          a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the constant con.
If old_form is TRUE, use the old form of length specification in the
mangling for lengths of literals.
*/
{
  /* Representation is something like
       CiL15   <-- integer constant 5
           ^-- Literal constant representation.
          ^--- Length of literal constant.
         ^---- L indicates literal constant; c indicates address
               of variable, etc.
       ^^----- Type of constant, with "const" added.
     If the constant is a template parameter constant, skip the "C" and
     the type.  Likewise for an address constant. */
  if (con->kind != (a_constant_repr_kind)ck_template_param &&
      con->kind != (a_constant_repr_kind)ck_address) {
    add_to_mangled_name('C', mctl);
    /* Put out the constant type. */
    mangled_encoding_for_type(con->type, mctl);
  }  /* if */
  /* Put out the literal representation for the constant. */
  literal_representation(con, old_form, mctl);
}  /* mangled_encoding_for_constant */


static void mangled_encoding_for_expression(an_expr_node_ptr         expr,
                                            a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the expression pointed to by expr.
These expressions come up in ck_template_param expressions as template
arguments, and as dimensions of arrays in template signatures.
*/
{
  char             *operation_name;
  an_expr_node_ptr operand;
  unsigned long    num_operands;

  /* Drop eok_lvalue and eok_rvalue. */
  while (is_operation_node(expr) &&
         (expr->variant.operation.kind == (an_expr_operator_kind)eok_lvalue ||
          expr->variant.operation.kind == (an_expr_operator_kind)eok_rvalue)) {
    expr = expr->variant.operation.operands;
  }  /* while */
  switch (expr->kind) {
    case enk_constant:
      mangled_encoding_for_constant(expr->variant.constant,
                                    /*old_form=*/FALSE,
                                    mctl);
      break;
    case enk_operation:
#if MICROSOFT_EXTENSIONS_ALLOWED
      check_assertion(expr->variant.operation.kind !=
                                            (an_expr_operator_kind)eok_assume);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
      add_to_mangled_name('O', mctl);
      /* Get the operator name and put it out. */
      operation_name= mangled_expr_operator_name(expr->variant.operation.kind);
      add_str_to_mangled_name(operation_name, mctl);
      /* For a cast, put out the type cast to. */
      if (operation_name[0] == 'c' && operation_name[1] == 's') {
        mangled_encoding_for_type(expr->type, mctl);
      }  /* if */
      /* Put out the count of operands. */
      for (num_operands = 0, operand = expr->variant.operation.operands;
           operand != NULL;
           num_operands++, operand = operand->next) {}
      check_assertion(num_operands <= 9);
      add_number_to_mangled_name(num_operands, mctl);
      /* Put out the operands. */
      for (operand = expr->variant.operation.operands;
           operand != NULL;
           operand = operand->next) {
        mangled_encoding_for_expression(operand, mctl);
      }  /* for */
      /* Put out the final "O". */
      add_to_mangled_name('O', mctl);
      break;
    default:
      unexpected_condition_str("mangled_encoding_for_expression: bad kind");
  }  /* switch */
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
  char     buffer[50];

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The class is unnamed, so make up a name. */
    /* The name is __Cnn, where nn is a unique number for the
       class.  This is not from the ARM.  cfront uses the __Cn form, but
       the number is different. */
    unnamed_class_name_seed++;
    (void)sprintf(buffer, "__C%lu", (unsigned long)unnamed_class_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    type->source_corresp.name = name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_class_a_name */


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
    char            *module_id;
    a_namespace_ptr parent_nsp;
    /* The name is __N followed by the module id. */
    check_assertion(!nsp->source_corresp.is_class_member);
    parent_nsp = nsp->source_corresp.parent.namespace_ptr;
    if (parent_nsp != NULL &&
        unmangled_name_of(&parent_nsp->source_corresp) == NULL) {
      /* A nested unnamed namespace within an unnamed namespace.
         Just use __N.  The name will be unique within the parent namespace. */
      module_id = "";
    } else {
      a_translation_unit_ptr tup;
      check_assertion(!nsp->is_namespace_alias);
      tup = trans_unit_for_scope[nsp->variant.assoc_scope->number];
      if (curr_translation_unit == tup) {
        /* Normal case -- the namespace is from the current translation
           unit. */
        /* Because generation of the module id can make a recursive call
           to the name mangling routines, save and restore the mangling
           buffer. */
        a_text_buffer_ptr saved_text_buffer = mangling_text_buffer;
        check_assertion(mangling_text_buffer != second_mangling_text_buffer);
        if (second_mangling_text_buffer == NULL) {
          second_mangling_text_buffer = alloc_text_buffer(2048);
        }  /* if */
        mangling_text_buffer = second_mangling_text_buffer;
        module_id = make_module_id();
        mangling_text_buffer = saved_text_buffer;
      } else {
        /* The namespace is from a translation unit other than the current
           one. */
        module_id = *(tup->module_id_ptr);
        /* The module id must have been generated already. */
        check_assertion(module_id != NULL);
      }  /* if */
    }  /* if */
    name_len = 3 + strlen(module_id) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, "__N");
    (void)strcpy(name+3, module_id);
    nsp->source_corresp.name = name;
    nsp->source_corresp.name_has_been_mangled = TRUE;
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
  char     buffer[50];

  /* Note that we may be changing a type that is not being lowered yet, but
     that's okay -- the name in the IL entry is not used by the front end. */
  if (type->source_corresp.name == NULL) {
    /* The enum is unnamed, so make up a name. */
    /* The name is __Enn, where nn is a unique number for the
       enum.  This is not from the ARM.  cfront uses the __En form, but
       the number is different. */
    unnamed_enum_name_seed++;
    (void)sprintf(buffer, "__E%lu", (unsigned long)unnamed_enum_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    type->source_corresp.name = name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_enum_a_name */


/*
Seed number for unnamed member variable names.
*/
static unsigned long
		unnamed_member_variable_name_seed;


static void give_unnamed_member_variable_a_name(a_variable_ptr var)
/*
If the indicated member variable is unnamed, give it a name.
*/
{
  char     *name;
  sizeof_t name_len;
  char     buffer[50];

  if (var->source_corresp.name == NULL) {
    /* The member variable is unnamed, so make up a name. */
    /* The name is __Vnn, where nn is a unique number for the
       member variable.  This is not from the ARM or cfront. */
    unnamed_member_variable_name_seed++;
    (void)sprintf(buffer, "__V%lu",
                  (unsigned long)unnamed_member_variable_name_seed);
    name_len = strlen(buffer) + 1;
    name = alloc_lowered_name_string(name_len);
    (void)strcpy(name, buffer);
    var->source_corresp.name = name;
    var->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_member_variable_a_name */


static void mangled_encoding_for_template_template_argument(
                                                a_template_arg_ptr       tap,
                                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template template argument
given by tap.
*/
{
  a_template_ptr                   temp = tap->variant.templ;

  if (temp->kind == (a_template_kind)templk_template_template_param) {
    /* The value of the argument is itself a template template parameter. */
    mangled_encoding_for_template_parameter(
                                     &temp->coordinates,
                                     (a_template_arg *)NULL,
                                     mctl);
  } else {
    /* The value of the argument is a template. */
    a_source_correspondence *scp = &temp->source_corresp;
    a_length_reservation    length_reservation;

    /* Name of template.  The encoding is like
         4abcd <-- encoding for template "abcd"
          ^^^^---- Name of entity.
         ^-------- Length of the name.
    */
    check_assertion(scp->name != NULL);
    reserve_space_for_length(&length_reservation, mctl);
    /* Put out the base part of the name. */
    add_str_to_mangled_name(scp->name, mctl);
    if (scp->is_class_member || scp->parent.namespace_ptr != NULL) {
      /* Add two underscores after the name. */
      add_str_to_mangled_name("__", mctl);
      /* Put out the name of the class or namespace of which this template
         is a member. */
      mangled_parent_qualifier(scp, mctl);
    }  /* if */
    fill_in_length(&length_reservation, mctl);
  }  /* if */
}  /* mangled_encoding_for_template_template_argument */


static void mangled_template_arguments(
                                    a_template_arg_ptr       template_arg_list,
                                    a_boolean                partial_spec,
                                    a_boolean                old_form,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the template arguments given
by template_arg_list.  If partial_spec is TRUE, this argument list is
the first one on a partial specialization.  If old_form is TRUE, use
the old form of length specification in the mangling for lengths of
literals.
*/
{
  char                 *str;
  a_template_arg_ptr   tap;
  a_length_reservation length_reservation;
  a_boolean            saved_suppress_partial_spec_args =
                                              mctl->suppress_partial_spec_args;

  /* The mangled form of template arguments is something like
       __tm__3_ii
               ^^--- Two template arguments of type int.
             ^------ Total length of template argument list string,
                     including the underscore.
         ^^--------- Fixed string, indicates "parameterized type".
     When distinct_template_signatures is FALSE, "__pt__" is used instead
     of "__tm__".  For the first argument list of a partial specialization,
     "__ps__" is used.
  */
  if (!distinct_template_signatures) {
    str = "__pt__";
  } else if (partial_spec) {
    str = "__ps__";
  } else {
    str = "__tm__";
  }  /* if */
  add_str_to_mangled_name(str, mctl);
#if ABI_COMPATIBILITY_VERSION > 245
  /* Suppress information on partial specializations in any parent types
     referenced in the template arguments. */
  mctl->suppress_partial_spec_args = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION > 245 */
  reserve_space_for_length(&length_reservation, mctl);
  add_to_mangled_name('_', mctl);
  /* Run through the template argument list, determining the representation
     for each argument. */
  for (tap = template_arg_list; tap != NULL; tap = tap->next) {
    if (is_type_templ_arg(tap)) {
      /* Type argument. */
      mangled_encoding_for_type(tap->variant.type, mctl);
    } else if (is_template_templ_arg(tap)) {
      /* A template template argument. */
      mangled_encoding_for_template_template_argument(tap, mctl);
    } else {
      check_assertion_str2(!tap->is_array_bound_of_unknown_type,
                           "mangled_template_arguments:",
                           "is_array_bound_of_unknown_type set");
      /* Constant argument.  The encoding for the constant begins with
         an "X". */
      add_to_mangled_name('X', mctl);
      mangled_encoding_for_constant(tap->variant.constant,
                                    old_form,
                                    mctl);
    }  /* if */
  }  /* for */
  /* Go back and fill in the length. */
  fill_in_length(&length_reservation, mctl);
  mctl->suppress_partial_spec_args = saved_suppress_partial_spec_args;
}  /* mangled_template_arguments */


static void mangled_specialization_indication(a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding qualifier that indicates specialization.
*/
{
  add_str_to_mangled_name("__S", mctl);
}  /* mangled_specialization_indication */


static void mangled_full_class_name(
                         a_type_ptr               type,
                         a_boolean                show_partial_spec_args,
                         a_boolean                show_template_specialization,
                         a_boolean                show_specialization,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is not the version that contains a leading count of the number
of characters in the name; here, the name is usually just the original
name, but is different if the class is a template class or is unnamed.
Also, this routine does not do anything special with nested types.
show_partial_spec_args is TRUE if template arguments for a partial
specialization should be put out.  show_template_specialization is TRUE
if the class is generated from a specialization of a template and an
indication of that fact should be put out.  show_specialization is TRUE
if the class is itself a specialization and an indication of that fact
should be put out.
*/
{
  char                        *name;
  a_class_type_supplement_ptr ctsp;
  a_template_arg_ptr          template_args;
  a_boolean                   use_previously_mangled_name = FALSE;

  check_assertion(is_immediate_class_type(type));
  ctsp = type->variant.class_struct_union.extra_info;
  check_assertion_str(ctsp != NULL,
                      "mangled_full_class_name: no class type supplement");
  if (type->source_corresp.name_has_been_mangled) {
    /* The name is already mangled, including any template parameters.
       We can use the mangled form unless we need to add specialization
       indicators, which are not present in the saved mangled form, or
       unless the name has been processed in some way that prevents its
       use as part of another mangled name. */
    if (!show_partial_spec_args &&
        !show_template_specialization &&
        !show_specialization &&
        !type->source_corresp.mangled_name_cannot_be_included_in_other_name) {
      use_previously_mangled_name = TRUE;
    }  /* if */
  }  /* if */
  if (use_previously_mangled_name) {
    /* Use the previously mangled version of the name. */
    add_str_to_mangled_name(type->source_corresp.name, mctl);
  } else {
    /* Develop the mangled name. */
    /* Always start with the name of the class, which applies even in the
       template class case. */
    name = unmangled_name_of(&type->source_corresp);
    if (name == NULL) {
      /* For an unnamed class, generate a name (or use the name previously
         generated). */
      give_unnamed_class_a_name(type);
      name = type->source_corresp.name;
    }  /* if */
    add_str_to_mangled_name(name, mctl);
    /* See if template arguments are needed.  For partial specializations,
       there are two argument lists. */
    template_args = ctsp->template_arg_list;
    if (mctl->suppress_partial_spec_args) show_partial_spec_args = FALSE;
#if ABI_COMPATIBILITY_VERSION < 241
    /* Before this change, all names included partial specialization
       arguments. */
    show_partial_spec_args = distinct_template_signatures;
#endif /* ABI_COMPATIBILITY_VERSION < 241 */
    if (show_partial_spec_args &&
        ctsp->partial_spec_template_arg_list != NULL) {
      /* A partial specialization.  The first list is the argument list
         from the prototype instantiation of the partial specialization.
           template <class T> struct A { ... };
           template <class T> struct A<T *> { ... };
                                       ^^^this argument list
      */
      a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(type);
      a_class_type_supplement_ptr   proto_ctsp;

      if (type->variant.class_struct_union.is_prototype_instantiation) {
        proto_ctsp = ctsp;
      } else {
        a_symbol_ptr proto_sym = cssp->corresp_prototype_sym;
        a_type_ptr   proto_type = proto_sym->variant.class_struct_union.type;
        proto_ctsp = proto_type->variant.class_struct_union.extra_info;
      }  /* if */
      mangled_template_arguments(proto_ctsp->template_arg_list,
                                 /*partial_spec=*/TRUE,
                                 /*old_form=*/FALSE,
                                 mctl);
      /* The second argument list is the deduced argument values for the
         template parameter list of the partial specialization. */
      template_args = ctsp->partial_spec_template_arg_list;
    }  /* if */
    if (show_template_specialization) {
      /* Put out an indication of the fact the template from which this
         class is generated is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
    if (template_args != NULL) {
      /* A template class.  Add information on template arguments. */
      /* old_form=TRUE forces use of the cfront-compatible mangling convention
         for lengths on literals, which though ambiguous is okay here because
         the class cannot be followed by an "_". */
      a_boolean old_form = !distinct_template_signatures;
#if ABI_COMPATIBILITY_VERSION < 235
      old_form = TRUE;
#endif /* ABI_COMPATIBILITY_VERSION < 235 */
      mangled_template_arguments(template_args,
                                 /*partial_spec=*/FALSE,
                                 old_form,
                                 mctl);
    }  /* if */
    if (show_specialization) {
      /* Put out an indication of the fact that this class is specialized. */
      mangled_specialization_indication(mctl);
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
      add_str_to_mangled_name("__L", mctl);
      add_number_to_mangled_name((unsigned long)assoc_sym->decl_scope, mctl);
    }  /* if */
  }  /* if */
}  /* mangled_full_class_name */


/*
Interface to mangled_full_class_name for the case where
show_partial_spec_args, show_template_specialization, and
show_specialization are FALSE (meaning no information about those things
should be put out).
*/
#define mangled_basic_class_name(type, mctl)                          \
  mangled_full_class_name((type), FALSE, FALSE, FALSE, (mctl))


static void mangled_name_with_length(char                     *name,
                                     a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for a name, with a prefix that
indicates the length, e.g., "3abc" for the name "abc".  name is
null-terminated.
*/
{
  add_number_to_mangled_name((unsigned long)strlen(name), mctl);
  add_str_to_mangled_name(name, mctl);
}  /* mangled_name_with_length */


static void mangled_class_encoding(
                         a_type_ptr               type,
                         a_boolean                show_partial_spec_args,
                         a_boolean                show_template_specialization,
                         a_boolean                show_specialization,
                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the version that contains a leading count of the number of
characters in the name, but not information on parents.  If the class
is a proxy class for a template parameter, the encoding for the template
parameter is put out (without a length).  If the class is a template
template parameter with a template argument list, put out an encoding
for that.  show_partial_spec_args is TRUE if template arguments for a
partial specialization should be put out.  show_template_specialization
is TRUE if the class is generated from a specialization of a template
and an indication of that fact should be put out.  show_specialization
is TRUE if the class is itself a specialization and an indication of
that fact should be put out.
*/
{
  a_type_ptr template_param = NULL;
  char       *name;

  check_assertion(is_immediate_class_type(type));
  if (type->source_corresp.assoc_info != NULL) {
    /* See if this class is a proxy class for a template parameter.  If so,
       we will use the template parameter encoding. */
    template_param =
             symbol_supplement_for_class(type)->template_param_for_proxy_class;
  }  /* if */
  if (template_param != NULL) {
    /* This class is the proxy for a template parameter.  Use the encoding
       for the template parameter as the name for the class. */
    check_assertion(template_param->kind == (a_type_kind)tk_template_param);
    switch (template_param->variant.template_param.kind) {
      case tptk_param:
        mangled_encoding_for_template_parameter(
               &template_param->variant.template_param.extra_info->coordinates,
               (a_template_arg *)NULL,
               mctl);
        break;
      case tptk_member:
        /* For something like T::x, where T is a template parameter, just
           put out "x" here. */
        name = unmangled_name_of(&type->source_corresp);
        check_assertion_str(name != NULL,
                            "mangled_class_encoding: tptk_member has no name");
        mangled_name_with_length(name, mctl);
        break;
      default:
        unexpected_condition_str(
                            "mangled_class_encoding: bad template param kind");
    }  /* switch */
  } else {
    /* Not a proxy for a template parameter. */
    /* See whether this is the proxy for a template template parameter. */
    a_boolean    is_template_template_param = FALSE;
    a_symbol_ptr template_sym = class_template_for_type(type);
    if (template_sym != NULL) {
      a_template_symbol_supplement_ptr tssp =
                                           template_sym->variant.template_info;
      if (tssp->variant.class_template.template_template_param) {
        /* Yes, this is a template template parameter. */
        is_template_template_param = TRUE;
        mangled_encoding_for_template_parameter(
                                     &tssp->il_template_entry->coordinates,
                                     type->variant.class_struct_union.
                                                 extra_info->template_arg_list,
                                     mctl);
      }  /* if */
    }  /* if */
    if (!is_template_template_param) {
      /* Not a template template parameter. */
      /* Put out the class name preceded by its length. */
      a_length_reservation length_reservation;
      reserve_space_for_length(&length_reservation, mctl);
      mangled_full_class_name(type,
                              show_partial_spec_args,
                              show_template_specialization,
                              show_specialization,
                              mctl);
      fill_in_length(&length_reservation, mctl);
    }  /* if */
  }  /* if */
}  /* mangled_class_encoding */


static void r_mangled_parent_qualifier(a_source_correspondence  *scp,
                                       unsigned long            nesting_level,
                                       a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the parent qualifier needed in
the mangled name for a member of a class or namespace whose source
correspondence is pointed to by scp.  nesting_level is used to track
recursive calls of this routine to deal with multiple levels of parents.
nesting_level == 1 refers to the innermost qualifier of a type,
nesting_level == 2 is the next level out, etc.  See the macro
mangled_parent_qualifier, which supplies the usual nesting_level == 1.
*/
{
  a_source_correspondence *parent_scp;
  a_boolean               more_levels;

  /* See if the present level is nested inside some other class or
     namespace. */
  if (scp->is_class_member) {
    a_type_ptr class_type = scp->parent.class_type;
#if CHECKING
    if (!class_type_has_body(class_type) &&
        !class_type->variant.class_struct_union.is_nonreal_class) {
#if DEBUG
      (void)fprintf(f_debug, "Parent class = ");
      db_abbr_type(class_type);
#endif /* DEBUG */
      unexpected_condition_str(
                       "r_mangled_parent_qualifier: parent class has no body");
    }  /* if */
#endif /* CHECKING */
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
    r_mangled_parent_qualifier(parent_scp, nesting_level + 1, mctl);
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
      add_to_mangled_name('Q', mctl);
      add_number_to_mangled_name(nesting_level, mctl);
      add_to_mangled_name('_', mctl);
    }  /* if */
  }  /* if */
  /* Put the class or namespace name at this level into the mangled name. */
  /* The name is preceded by a count of the number of characters in
     the name. */
  if (scp->is_class_member) {
    /* Class name. */
    a_type_ptr type = scp->parent.class_type;
    a_boolean  show_partial_spec_args = FALSE;
    a_boolean  is_specialization = FALSE;
    a_boolean  is_template_specialization = FALSE;
    if (distinct_template_signatures) {
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
      show_partial_spec_args = distinct_template_signatures;
    }  /* if */
    mangled_class_encoding(type,
                           show_partial_spec_args,
                           is_template_specialization,
                           is_specialization,
                           mctl);
  } else {
    /* Namespace name. */
    a_namespace_ptr nsp = scp->parent.namespace_ptr;
    char            *name = unmangled_name_of(&nsp->source_corresp);
    if (name == NULL) {
      /* For an unnamed namespace, generate a name (or use the name previously
         generated). */
      give_unnamed_namespace_a_name(nsp);
      name = nsp->source_corresp.name;
    }  /* if */
    /* Put out the namespace name preceded by the length of the name, e.g.,
       "NNN" --> "3NNN". */
    mangled_name_with_length(name, mctl);
  }  /* if */
}  /* r_mangled_parent_qualifier */


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


static void mangled_type_name(a_type_ptr               type,
                              a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the type "type".
This routine is used for named types (classes, enums, and typedefs)
and for unnamed classes and enums.  Nested types are encoded as such.
*/
{
  char *name;

  if (type_needs_parent_qualifier(type)) {
    /* The type is a member of a class or namespace, so put out a qualifier.
       Note that the count starts at 2 because the type name itself is level
       1. */
    r_mangled_parent_qualifier(&type->source_corresp,
                               (unsigned long)2,
                               mctl);
  }  /* if */
  /* Put out the type name itself. */
  /* The mangled form of a type name is the type name with a length
       preceding it:
         AB          --> 2AB
         ABCDEFGHIJK --> 11ABCDEFGHIJK
  */
  if (is_immediate_class_type(type)) {
    /* Class name. */
    mangled_class_encoding(type,
                           /*show_partial_spec_args=*/FALSE,
                           /*show_template_specialization*/FALSE,
                           /*show_specialization=*/FALSE,
                           mctl);
  } else {
    /* Not a class name (typedef or enum). */
    name = unmangled_name_of(&type->source_corresp);
    if (name == NULL) {
      /* For an unnamed enum, generate a name (or use the name previously
         generated). */
      check_assertion(is_enum_type(type));
      give_unnamed_enum_a_name(type);
      name = type->source_corresp.name;
    }  /* if */
    mangled_name_with_length(name, mctl);
  }  /* if */
}  /* mangled_type_name */


static void mangled_class_name_internal(a_type_ptr               type,
                                        a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type".
This is the encoding used for the name of the class as opposed to
the encoding for the class as a type (for example, it has no length
preceding a simple class name).  This routine has the name "_internal"
because it's intended to be called from inside a name mangling
operation; compare mangled_class_name (no "_internal").
*/
{
  if (type_needs_parent_qualifier(type)) {
    /* For a nested class, use the nested type encoding for the class. */
    mangled_type_name(type, mctl);
  } else {
    /* For a non-nested class, use the simple form of the name (with
       no preceding length). */
    mangled_basic_class_name(type, mctl);
  }  /* if */
}  /* mangled_class_name_internal */


static void mangled_encoding_for_type(a_type_ptr               type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the type "type".
*/
{
  a_type_ptr named_type, pm_base_type;
#if ABI_COMPATIBILITY_VERSION < 230
  a_type_ptr named_typedef = NULL;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
  char       *s;
  a_type_qualifier_set
             qualifiers;

  /* Walk through any typerefs above the type.  Remember type qualifiers
     and skip down to the "real" underlying type. */
  qualifiers = 0;
  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    /* Remember type qualifiers encountered. */
    qualifiers |= type->variant.typeref.qualifiers;
#if ABI_COMPATIBILITY_VERSION < 230
    /* Remember the bottommost named typedef encountered. */
    if (has_name(type)) named_typedef = type;
#endif /* ABI_COMPATIBILITY_VERSION < 230 */
#if DO_IL_LOWERING
    if (type->variant.typeref.orig_type != NULL) {
      /* A type like a pointer-to-member, which has been lowered.
         Switch to the original type. */
      type = type->variant.typeref.orig_type;
      break;
    }  /* if */
#endif /* DO_IL_LOWERING */
  }  /* for */
  /* Put out type qualifiers, if any. */
  if (qualifiers != 0) {
    mangled_encoding_for_type_qualifiers(qualifiers, mctl);
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
    mangled_type_name(named_type, mctl);
  } else {
    /* The type is not named, so develop a description string. */
    switch (type->kind) {
      case tk_error:
      case tk_unknown:
        /* This might come up in mangling names for template instantiation
           after errors have been detected. */
        check_assertion(total_errors != 0);
        s = "?";
        break;
      case tk_void:
        s = "v";
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          /* Unnamed enum.  mangled_type_name will make up a name. */
          mangled_type_name(type, mctl);
          goto have_whole_mangled_name;
        }  /* if */
        if (type->variant.integer.wchar_t_type) {
          s = "w";
        } else if (type->variant.integer.bool_type) {
          s = "b";
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (type->variant.integer.microsoft_sized_int_type) {
          /* Mangling of __intN types in certain Microsoft modes (Visual C++
             6.0 started treating these as new intrinsic types). */
          an_integer_kind  kind = type->variant.integer.int_kind;
          if (kind == targ_int8_int_kind) {
            s = "m1";
          } else if (kind == targ_unsigned_int8_int_kind) {
            s = "Um1";
          } else if (kind == targ_int16_int_kind) {
            s = "m2";
          } else if (kind == targ_unsigned_int16_int_kind) {
            s = "Um2";
          } else if (kind == targ_int32_int_kind) {
            s = "m4";
          } else if (kind == targ_unsigned_int32_int_kind) {
            s = "Um4";
          } else if (kind == targ_int64_int_kind) {
            s = "m8";
          } else if (kind == targ_unsigned_int64_int_kind) {
            s = "Um8";
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
        mangled_encoding_for_function_type(type,
                                           /*do_return_type=*/TRUE,
                                           mctl);
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name will make up a name. */
        mangled_type_name(type, mctl);
        goto have_whole_mangled_name;
      case tk_template_param:
        /* This comes up when mangling the names for template entities using
           the modern mangling approach. */
        switch (type->variant.template_param.kind) {
          case tptk_param:
            mangled_encoding_for_template_parameter(
                         &type->variant.template_param.extra_info->coordinates,
                         (a_template_arg *)NULL,
                         mctl);
            break;
          case tptk_member:
            /* Type selected from a template parameter type, e.g., T::x. */
            mangled_type_name(type, mctl);
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
    add_str_to_mangled_name(s, mctl);
    /* Do any processing needed after the description letter. */
    switch (type->kind) {
      case tk_pointer:
        /* Put out the type pointed to. */
        mangled_encoding_for_type(type->variant.pointer.type, mctl);
        break;
      case tk_ptr_to_member:
        /* Put out the mangled name of the class for which this is a member
           pointer. */
        mangled_encoding_for_type(type->variant.ptr_to_member.
                                                       class_of_which_a_member,
                                  mctl);
        pm_base_type = type->variant.ptr_to_member.type;
        if (is_function_type(pm_base_type)) {
          /* This is a pointer to member function.  Put out the type qualifiers
             (if any) on the member function type. */
          mangled_encoding_for_function_qualifiers(pm_base_type, mctl);
        }  /* if */
        /* Put out the type pointed to. */
        mangled_encoding_for_type(pm_base_type, mctl);
        break;
      case tk_array:
        /* Put out the array size, an underscore, and then the element type,
           i.e., int[10] is put out as A10_i. */
        check_assertion(!type->variant.array.is_variable_size_array);
        if (type->variant.array.is_template_dependent_size_array) {
          /* Template-dependent size arrays are possible when putting out
             function prototypes.  For that case the prefix is "A_". */
          check_assertion(distinct_template_signatures);
          add_to_mangled_name('_', mctl);
          /* Put out an encoding for the bound. */
          mangled_encoding_for_constant(
                            type->variant.array.variant.element_count_constant,
                            /*old_form=*/FALSE,
                            mctl);
        } else {
          /* Put out the (constant) number of elements. */
          add_number_to_mangled_name((unsigned long)type->variant.array.
                                                    variant.number_of_elements,
                                     mctl);
        }  /* if */
        add_to_mangled_name('_', mctl);
        /* Put out the element type. */
        mangled_encoding_for_type(type->variant.array.element_type, mctl);
        break;
      default:;
        /* Many cases don't require any handling. */
    }  /* switch */
  }  /* if */
have_whole_mangled_name:;
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
    case onk_question:          /* "?" */
      name = "qs";
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
    case eok_negate:
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_unary_plus:
      opkind = (an_opname_kind)onk_plus;
      break;
    case eok_not:
      opkind = (an_opname_kind)onk_not;
      break;
    case eok_cast:
    case eok_base_class_cast:
    case eok_derived_class_cast:
    case eok_pm_base_class_cast:
    case eok_pm_derived_class_cast:
    case eok_lvalue_cast:
    case eok_bool_cast:
      name = "cs";
      break;
    case eok_complement:
      opkind = (an_opname_kind)onk_compl;
      break;
    case eok_iadd:
    case eok_fadd:
    case eok_add:
      opkind = (an_opname_kind)onk_plus;
      break;
    case eok_isubtract:
    case eok_fsubtract:
    case eok_subtract:
      opkind = (an_opname_kind)onk_minus;
      break;
    case eok_imultiply:
    case eok_fmultiply:
    case eok_multiply:
      opkind = (an_opname_kind)onk_star;
      break;
    case eok_idivide:
    case eok_fdivide:
    case eok_divide:
      opkind = (an_opname_kind)onk_divide;
      break;
    case eok_ieq:
    case eok_feq:
    case eok_eq:
      opkind = (an_opname_kind)onk_eq;
      break;
    case eok_ine:
    case eok_fne:
    case eok_ne:
      opkind = (an_opname_kind)onk_ne;
      break;
    case eok_igt:
    case eok_fgt:
    case eok_gt:
      opkind = (an_opname_kind)onk_gt;
      break;
    case eok_ilt:
    case eok_flt:
    case eok_lt:
      opkind = (an_opname_kind)onk_lt;
      break;
    case eok_ige:
    case eok_fge:
    case eok_ge:
      opkind = (an_opname_kind)onk_ge;
      break;
    case eok_ile:
    case eok_fle:
    case eok_le:
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
    case eok_comma:
      opkind = (an_opname_kind)onk_comma;
      break;
    case eok_land:
      opkind = (an_opname_kind)onk_and_and;
      break;
    case eok_lor:
      opkind = (an_opname_kind)onk_or_or;
      break;
    case eok_question:
#if GNU_EXTENSIONS_ALLOWED
    case eok_binary_question:
#endif /* GNU_EXTENSIONS_ALLOWED */
      opkind = (an_opname_kind)onk_question;
      break;
    case eok_lvalue:                     /* Handled higher up */
    case eok_rvalue:                     /* Handled higher up */
#if MICROSOFT_EXTENSIONS_ALLOWED
    case eok_assume:                     /* Handled higher up */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      unexpected_condition_str("mangled_expr_operator_name: bad operator");
  }  /* switch */
  if (name == NULL) {
    /* Convert opkind to a name. */
    name = mangled_operator_name(opkind);
  }  /* if */
  return name;
}  /* mangled_expr_operator_name */


static void mangled_function_base_name(
                                      a_source_correspondence  *scp,
                                      a_special_function_kind  special_kind,
                                      an_opname_kind           opname_kind,
                                      a_type_ptr               conversion_type,
                                      a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the base name of the function
indicated by scp.  special_kind, opname_kind, and conversion_type give
additional information for special functions like constructors and
conversion functions.
*/
{
  char      *name;
  a_boolean add_leading_underscores = FALSE;

  if (special_kind == (a_special_function_kind)sfk_none) {
    /* Normal name. */
    name = unmangled_name_of(scp);
    check_assertion_str(name != NULL,
                        "mangled_function_base_name: unnamed routine");
  } else {
    /* Use a special name for the routine. */
    add_leading_underscores = TRUE;
    switch (special_kind) {
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
        name = mangled_operator_name(opname_kind);
        break;
      default:
        unexpected_condition_str(
                               "mangled_function_base_name: bad special kind");
    }  /* switch */
  }  /* if */
  if (add_leading_underscores) {
    add_str_to_mangled_name("__", mctl);
  }  /* if */
  /* Copy the name. */
  add_str_to_mangled_name(name, mctl);
  /* For a conversion function, add the type signature. */
  if (special_kind == (a_special_function_kind)sfk_conversion) {
    check_assertion(conversion_type != NULL);
    mangled_encoding_for_type(conversion_type, mctl);
  }  /* if */
}  /* mangled_function_base_name */


static void mangled_function_name(
                              a_routine_ptr            routine,
                              a_boolean                suppress_param_encoding,
                              a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the function "routine".
If suppress_param_encoding is TRUE, suppress the information on parameter
types; just put out the base encoded name.
*/
{
  a_type_ptr conversion_type, routine_type;
  a_boolean  is_member, mangle_as_template;
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
  mangle_as_template = (distinct_template_signatures &&
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
  /* Put out the base name of the function. */
  conversion_type = NULL;
  if (routine->special_kind == (a_special_function_kind)sfk_conversion) {
    conversion_type = routine_type->variant.routine.return_type;
  }  /* if */
  mangled_function_base_name(&routine->source_corresp, routine->special_kind,
                             routine->opname_or_builtin.opname_kind, 
			     conversion_type, mctl);
  if (mangle_as_template) {
    if (is_template_specialization) {
      /* Put out an indication of the fact the template from which this
         function is generated is specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
    if (routine->template_arg_list != NULL) {
      /* Put out the template arguments. */
      mangled_template_arguments(routine->template_arg_list,
                                 /*partial_spec=*/FALSE,
                                 /*old_form=*/FALSE,
                                 mctl);
    }  /* if */
    if (is_specialization) {
      /* Put out an indication of the fact that this function is
         specialized. */
      mangled_specialization_indication(mctl);
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
    add_str_to_mangled_name("__", mctl);
  }  /* if */
  if (is_member) {
    /* Put out the name of the class or namespace of which this function
       is a member. */
    mangled_parent_qualifier(&routine->source_corresp, mctl);
  }  /* if */
  if (!suppress_param_encoding) {
    a_boolean do_return_type;
    if (routine->source_corresp.is_class_member) {
      /* Class member function.  Put out the qualifiers on the member function
         type. */
      mangled_encoding_for_function_qualifiers(routine_type, mctl);
    }  /* if */
    /* Templates have their return types included. */
    do_return_type = mangle_as_template;
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
#if ABI_COMPATIBILITY_VERSION >= 243
        routine->special_kind == (a_special_function_kind)sfk_conversion ||
#endif /* ABI_COMPATIBILITY_VERSION >= 243 */
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* No return type on constructors, destructors, or conversion
         functions. */
      do_return_type = FALSE;
    }  /* if */
    /* Output the function type, including the parameter types. */
    mangled_encoding_for_function_type(routine_type, do_return_type, mctl);
  }  /* if */
}  /* mangled_function_name */


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
  if (!has_name(routine)) {
    /* Unnamed routines generally do not need mangled names. */
    /* Compiler-generated routines have no name, and they are left alone.
       But constructors for unnamed classes that got a name for linkage
       purposes should get mangled names. */
    if (routine->special_kind == (a_special_function_kind)sfk_constructor &&
        has_name(routine->source_corresp.parent.class_type)) {
      mangling_needed = TRUE;
    }  /* if */
  } else if (routine == il_header.main_routine) {
    /* Don't mangle "main" regardless of its linkage. */
  } else if (is_name_linkage_kind_subject_to_name_mangling(
                                       routine->source_corresp.name_linkage)) {
    /* Routines other than extern "C" routines need to be mangled. */
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

#if DO_IL_LOWERING

static void start_externalized_name(a_boolean                is_variable,
                                    a_mangling_control_block *mctl)
/*
Begin the output of the externalized mangled name for the entity with
the indicated source correspondence.  The entity is a variable if
is_variable is TRUE, a routine otherwise.
*/
{
  char *prefix = (is_variable ? (char *)"__STV__" : (char *)"__STF__");

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the prefix is put out here.
  */
  add_str_to_mangled_name(prefix, mctl);
}  /* start_externalized_name */

#endif /* DO_IL_LOWERING */
#if DO_IL_LOWERING

static void end_externalized_name(a_source_correspondence  *scp,
                                  a_mangling_control_block *mctl)
/*
End the output of the externalized mangled name for the entity with
the indicated source correspondence.
*/
{
  a_translation_unit_ptr tup;
  char                   *module_id;

  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
     Only the part after "name" is put out here.
  */
  /* Get the module id for the translation unit which this source
     correspondence is part of.  For a source correspondence with no
     associated symbol, use the current translation unit. */
  tup = (scp->assoc_info != NULL) ? trans_unit_for_source_corresp(scp) :
                                    curr_translation_unit;
  module_id = *tup->module_id_ptr;
  add_str_to_mangled_name("__", mctl);
  add_str_to_mangled_name(module_id, mctl);
}  /* end_externalized_name */


char *externalized_mangled_name(a_source_correspondence  *scp,
                                a_boolean                is_variable)
/*
Generate and return the externalized name for the entity with the
indicated source correspondence.  An externalized name is a name given
to a static entity when it is made external so that its name will remain
unique across the program.  The entity is a variable if is_variable
is TRUE, a routine otherwise.  The name returned is in a temporary
buffer, and must be copied elsewhere promptly.
*/
{
  a_mangling_control_block mctl;
  char                     *name = scp->name;
  char                     buffer[50];
  a_source_correspondence  *module_scp = scp;

#if CHECKING
  /* If the name needs to be mangled, the mangling should have been done
     already. */
  { a_boolean dummy;
    if (scp->name_has_been_mangled) {
      /* Okay, mangling already done. */
      /* Compression and truncation shouldn't have been done already,
         however. */
      check_assertion_str(!scp->mangled_name_cannot_be_included_in_other_name,
                      "externalized_mangled_name: mangled name already final");
    } else if (is_variable ?
                           variable_name_mangling_needed((a_variable_ptr)scp) :
                           function_name_mangling_needed((a_routine_ptr)scp,
                                                         &dummy)) {
#if DEBUG
      db_entity_info((char *)scp, is_variable ? iek_variable : iek_routine);
#endif /* DEBUG */
      internal_error("externalized_mangled_name: name not mangled");
    }  /* if */
  }
#endif /* CHECKING */
  start_mangling(&mctl);
  /* The generated name has the form
       __STV__name__module_id  (variable)
       __STF__name__module_id  (function)
  */
  start_externalized_name(is_variable, &mctl);
  if (name == NULL) {
    /* Entity has no name, e.g., a generated routine.  Generate one. */
    if (is_variable) {
      a_variable_ptr var = (a_variable_ptr)scp;
      if (var->is_anonymous_parent_object) {
        /* Give a name to an anonymous union variable based on its first
           member's name.  This is necessary so that the name will come out
           the same whether compiled in a primary translation unit or a
           secondary one. */
        a_type_ptr  union_type = var->type;
        a_field_ptr field;
        check_assertion(union_type->kind == (a_type_kind)tk_union);
        for (;;) {
          field = union_type->variant.class_struct_union.field_list;
          if (field == NULL) break;
          /* Use the name of the first member. */
          name = field->source_corresp.name;
          if (name != NULL) {
            module_scp = &field->source_corresp;
            break;
          }  /* if */
          /* Loop if the first member is itself an anonymous union. */
          if (!field->is_anonymous_parent_object) break;
          union_type = field->type;
        }  /* for */
      }  /* if */
    }  /* if */
    if (name == NULL) {
      /* Generate a name. */
      (void)sprintf(buffer, "%lu", unique_id_for_il_pointer(scp));
      name = buffer;
    }  /* if */
  }  /* if */
  add_str_to_mangled_name(name, &mctl);
  end_externalized_name(module_scp, &mctl);
  add_to_mangled_name('\0', &mctl);
  return mangling_text_buffer->buffer;
}  /* externalized_mangled_name */

#endif /* DO_IL_LOWERING */
#if TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED || MODULE_ID_NEEDED

char *get_mangled_function_name(a_routine_ptr routine)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in a temporary buffer but do not change the
name in the routine entry.
*/
{
  a_mangling_control_block mctl;
  a_boolean                suppress_param_encoding;
  char                     *mangled_name;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* Static entities are potentially referenced from exported templates
     and therefore get externalized, which gives them a different kind
     of mangled name. */
  needs_to_be_externalized =
                       (routine->storage_class == (a_storage_class)sc_static &&
                        any_exported_templates());
#endif /* DO_IL_LOWERING */
  if ((routine->source_corresp.name_has_been_mangled &&
       !routine->source_corresp.final_name_mangling_pending &&
       (!needs_to_be_externalized || routine->source_corresp.externalized)) ||
      !function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* The name has already been (completely) mangled, or it doesn't need
       to be mangled, so just return it. */
    mangled_name = routine->source_corresp.name;
    /* The routine should not be unnamed. */
    check_assertion(mangled_name != NULL);
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl);
    /* Create the name. */
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      start_externalized_name(/*is_variable=*/FALSE, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_function_name(routine, suppress_param_encoding, &mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      end_externalized_name(&routine->source_corresp, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_name = end_mangling((a_source_correspondence *)NULL,
                                /*final=*/TRUE, &mctl);
  }  /* if */
  return mangled_name;
}  /* get_mangled_function_name */

#endif /* TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED ||
          MODULE_ID_NEEDED */

static void mangled_member_name(a_source_correspondence  *scp,
                                a_boolean                is_specialization,
                                a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class or
namespace member whose source correspondence is given by scp.
This routine must be called only for static data member variables,
namespace member variables, and class and namespace member constants.
is_specialization is TRUE if the variable is a template static data
member specialization.
*/
{
  char *name;

  /* The mangled name of a static data member or member constant is the
     original name followed by two underscores followed by the mangled
     class name.  For example:
       AB::xy --> xy__2AB
     The same encoding is used for members of namespaces.
  */
  name = unmangled_name_of(scp);
  if (name == NULL) {
    /* For an unnamed member, use the generated name.  This can happen for
       an anonymous union in a namespace. */
    name = scp->name;
    check_assertion(name != NULL);
  }  /* if */
  /* Copy the name. */
  add_str_to_mangled_name(name, mctl);
  if (scp->member_of_unknown_base) {
    /* We're pretending that we found the member in a dependent
       base class.  That means the original form of reference
       was unqualified.  Don't put out the parent qualifier. */
  } else {
    if (distinct_template_signatures && is_specialization) {
      /* Put out an indication of the fact that a static data member is
         specialized. */
      mangled_specialization_indication(mctl);
    }  /* if */
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", mctl);
    /* Output the mangled parent name. */
    mangled_parent_qualifier(scp, mctl);
  }  /* if */
}  /* mangled_member_name */


static void mangled_member_variable_name(a_variable_ptr           variable,
                                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the member variable
"variable" (a static data member or namespace member variable).
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
  mangled_member_name(&variable->source_corresp, is_specialization, mctl);
}  /* mangled_member_variable_name */

#if TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED

char *get_mangled_member_variable_name(a_variable_ptr variable)
/*
Get the mangled name for the indicated variable or static data member, and
return a pointer to it.  If the variable name has not been mangled yet,
create a copy of the mangled name in a temporary buffer but do not change
the name in the variable entry.  The variable must be a namespace member
or a static data member (e.g., not a file scope variable).
*/
{
  a_mangling_control_block mctl;
  char                     *mangled_name;
  a_boolean                needs_to_be_externalized = FALSE;

#if DO_IL_LOWERING
  /* Static entities are potentially referenced from exported templates
     and therefore get externalized, which gives them a different kind
     of mangled name. */
  needs_to_be_externalized =
                      (variable->storage_class == (a_storage_class)sc_static &&
                       any_exported_templates());
#endif /* DO_IL_LOWERING */
  if (variable->source_corresp.name_has_been_mangled &&
      !variable->source_corresp.final_name_mangling_pending &&
      (!needs_to_be_externalized || variable->source_corresp.externalized)) {
    /* The name has already been completely mangled, so just return it. */
    mangled_name = variable->source_corresp.name;
    /* The variable should not be unnamed. */
    check_assertion(mangled_name != NULL);
  } else {
    /* Generate the mangled name in a buffer. */
    start_mangling(&mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      start_externalized_name(/*is_variable=*/TRUE, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_member_variable_name(variable, &mctl);
#if DO_IL_LOWERING
    if (needs_to_be_externalized) {
      end_externalized_name(&variable->source_corresp, &mctl);
    }  /* if */
#endif /* DO_IL_LOWERING */
    mangled_name = end_mangling((a_source_correspondence *)NULL,
                                /*final=*/TRUE, &mctl);
  }  /* if */
  return mangled_name;
}  /* get_mangled_member_variable_name */

#endif /* TEMPLATE_LOOKUP_NEEDED || MODULE_ID_NEEDED */

/* Declaration required because of forward reference: */
static void do_scope_other_name_mangling(a_scope_ptr scope);


static void mangle_class_name(a_type_ptr class_type)
/*
Mangle the name of the indicated class, if necessary.
*/
{
  a_mangling_control_block mctl;

  error_position = class_type->source_corresp.decl_position;
  /* Template class names must be mangled because otherwise all instances
     of the same class template have the same name. */
  if (class_type->variant.class_struct_union.extra_info->
                                                   template_arg_list != NULL &&
      !class_type->source_corresp.name_has_been_mangled) {
    start_mangling(&mctl);
    mangled_basic_class_name(class_type, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be reused more often.
       final_type_name_mangling will do the compression or truncation if
       necessary. */
    (void)end_mangling(&class_type->source_corresp, /*final=*/FALSE, &mctl);
  }  /* if */
}  /* mangle_class_name */


static void do_type_list_class_name_mangling(a_type_ptr type_list)
/*
Do class name mangling for the types on the indicated type list and subscopes
thereunder.  Note that this does not include final processing for type names.
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
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_class_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
    }  /* if */
  }  /* for */
}  /* do_type_list_class_name_mangling */


static void do_scope_class_name_mangling(a_scope_ptr scope)
/*
Do name mangling for class names in the indicated scope (the file scope
or a namespace scope) and all subscopes in the file-scope memory region.
Note that this does not include final processing for type names.
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


void do_class_name_mangling(void)
/*
Do name mangling for all class names.  Note that this does not include
final processing for type names.
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
  a_mangling_control_block mctl;

  error_position = con->source_corresp.decl_position;
  if (!con->source_corresp.name_has_been_mangled) {
    start_mangling(&mctl);
    /* Determine how long the mangled name is. */
    mangled_member_name(&con->source_corresp,
                        /*is_specialization=*/FALSE, &mctl);
    (void)end_mangling(&con->source_corresp, /*final=*/TRUE, &mctl);
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
#if DO_IL_LOWERING
      if (is_primary_translation_unit) {
        /* Make sure the type-as-subobject for a class gets the class name
           before it is changed, if it is a nested class name. */
        prelower_class_type(type);
      }  /* if */
#endif /* DO_IL_LOWERING */
      class_scope = ctsp->assoc_scope;
      if (class_scope != NULL) {
        do_scope_other_name_mangling(class_scope);
      }  /* if */
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_other_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
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
  a_boolean                suppress_param_encoding;
  a_mangling_control_block mctl;

  error_position = routine->source_corresp.decl_position;
  if (!routine->source_corresp.name_has_been_mangled &&
      function_name_mangling_needed(routine, &suppress_param_encoding)) {
    /* Mangle the function name. */
    start_mangling(&mctl);
    mangled_function_name(routine, suppress_param_encoding, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, so that the name can be used in building names of types
       and variables promoted out of the routine.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling(&routine->source_corresp, /*final=*/FALSE, &mctl);
  }  /* if */
}  /* mangle_function_name */


static a_boolean variable_name_mangling_needed(a_variable_ptr variable)
/*
Return TRUE if the name of the indicated variable needs to be mangled.
*/
{
  a_boolean mangling_needed = FALSE;

  if (!has_name(variable)) {
    /* Unnamed variables do not need mangled names. */
  } else if (variable->source_corresp.is_class_member ||
             variable->source_corresp.parent.namespace_ptr != NULL) {
    /* Static data members and members of namespaces need mangled names. */
    mangling_needed = TRUE;
    /* But do not mangle namespace members with extern "C" linkage. */
    if (!is_name_linkage_kind_subject_to_name_mangling(
                                      variable->source_corresp.name_linkage)) {
      mangling_needed = FALSE;
    }  /* if */
  }  /* if */
  return mangling_needed;
}  /* variable_name_mangling_needed */


static void mangle_member_variable_name(a_variable_ptr variable)
/*
Mangle the name of the indicated static data member or namespace member
variable.
*/
{
  a_mangling_control_block mctl;

  error_position = variable->source_corresp.decl_position;
  if (!variable->source_corresp.name_has_been_mangled &&
      variable_name_mangling_needed(variable)) {
    start_mangling(&mctl);
    mangled_member_variable_name(variable, &mctl);
    /* Note final=FALSE to prevent compression and truncation at this
       time, in case the name is externalized later.  do_final_name_mangling
       will do the compression or truncation if necessary. */
    (void)end_mangling(&variable->source_corresp, /*final=*/FALSE, &mctl);
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
      /* Make sure that an unnamed namespace in a secondary translation unit
         is given a mangled name in that translation unit, so it has the
         right module id.  If the namespace contains only types, the name
         wouldn't otherwise be mangled at this time. */
      give_unnamed_namespace_a_name(nsp);
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


void do_all_name_mangling(void)
/*
Do any required name mangling.  This is called at the beginning of lowering of
the file scope.  It processes everything in the file scope and also
function-local entities that require mangling (they are accessed through the
orphan lists).  Final name mangling is not done yet -- see
do_final_name_mangling.
*/
{
  /* Mangle class names, not including final mangling on type names. */
  do_class_name_mangling();
  /* Do function, namespace, and static data member name mangling, not
     including some final mangling. */
  do_scope_other_name_mangling(il_header.primary_scope);
}  /* do_all_name_mangling */


static void final_entity_name_mangling(a_source_correspondence *scp)
/*
Do any final name mangling processing required on the entity with
the indicated source correspondence.  This means checking for
compression and truncation.
*/
{
  if (scp->final_name_mangling_pending) {
    a_mangling_control_block mctl;
    char                     *name = scp->name;
    sizeof_t                 length = strlen(name)+1;

    error_position = scp->decl_position;
    check_assertion(name != NULL);
    /* One reason for calling start_mangling here is to zero
       mangling_text_buffer->size. */
    /* If neither compression nor truncation is done, the name pointer
       is passed through unchanged.  If compression is done, the compressed
       name is allocated in IL memory.  If truncation is done, the existing
       name is truncated in place. */
    start_mangling(&mctl);
    mctl.length = length;
    name = compress_mangled_name(name, scp, &mctl);
    name = truncate_mangled_name(name, scp, &mctl);
    scp->name = name;
    scp->final_name_mangling_pending = FALSE;
  }  /* if */
}  /* final_entity_name_mangling */


/*
The prefix put on the front of the type encoding for a nested type to get
the name placed in the nested type itself.
*/
#define PREFIX_ON_NESTED_TYPE_NAME "__"


static void final_type_name_mangling(a_type_ptr type)
/*
Do final mangling on a type name, mangling that would prevent the mangled
form of the name from being usable as part of another mangled name.
Such processing is delayed to the end to allow reuse of the mangled name
(and the attendant time savings) as many times as possible.
This does special processing for nested type names, compressed names,
and truncated names.
*/
{
  a_mangling_control_block mctl;

  error_position = type->source_corresp.decl_position;
  check_assertion_str2(!type->source_corresp.
                                 mangled_name_cannot_be_included_in_other_name,
                       "final_type_name_mangling:", 
                       "mangled_name_cannot_be_included_in_other_name is set");
  if (has_name(type)) {
    if (type_needs_parent_qualifier(type)) {
      /* Nested type names must be mangled (because they exist in a scope
         that does not exist in the generated C code).  The mangled form
         is something like
           __Q2_1A1B
         The "Q2_1A1B" part is the normal representation for a mangled
         name, and the prefix makes it unique (i.e., makes it distinct
         from all user identifiers).  Similar mangling is used for members
         of namespaces (a different kind of "nested" type). */
      start_mangling(&mctl);
      add_str_to_mangled_name(PREFIX_ON_NESTED_TYPE_NAME, &mctl);
      mangled_type_name(type, &mctl);
      /* The following does compression and truncation if necessary. */
      (void)end_mangling(&type->source_corresp, /*final=*/TRUE, &mctl);
      type->source_corresp.mangled_name_cannot_be_included_in_other_name= TRUE;
    } else {
      /* Not a nested type.  Check for compression and truncation. */
      final_entity_name_mangling(&type->source_corresp);
    }  /* if */
  }  /* if */
}  /* final_type_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope);


static void do_type_list_final_name_mangling(a_type_ptr type_list)
/*
Do final name mangling for the types on the indicated type list
and subscopes thereunder.  Functions and variables in the subscopes are
also processed.
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
        do_scope_final_name_mangling(class_scope);
      }  /* if */
#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
      /* If some local types of member functions were promoted into the
         class on their way to the file scope, mangle them now too. */
      do_type_list_final_name_mangling(ctsp->promoted_local_types);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#endif /* DO_IL_LOWERING */
    }  /* if */
    /* Do name mangling on the type. */
    /* Note that the call here must be done after all subscopes have been
       visited; we don't want to change the name of a class until the
       classes nested within it have been processed. */
    final_type_name_mangling(type);
  }  /* for */
}  /* do_type_list_final_name_mangling */


static void do_scope_final_name_mangling(a_scope_ptr scope)
/*
Do final name mangling for all type, function, and variable names in the
indicated scope (a file, namespace, or class scope) and all subscopes in the
file-scope memory region.
*/
{
  a_namespace_ptr nsp;
  a_routine_ptr   routine;
  a_variable_ptr  variable;

  /* Process the types in the scope. */
  do_type_list_final_name_mangling(scope->types);
  /* Process the namespaces in the scope. */
  for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
    if (!nsp->is_namespace_alias) {
      do_scope_final_name_mangling(nsp->variant.assoc_scope);
    }  /* if */
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    final_entity_name_mangling(&routine->source_corresp);
  }  /* for */
  /* Visit all variables. */
  for (variable = scope->variables;
       variable != NULL;
       variable = variable->next) {
    /* Skip variables that aren't members, because final mangling may have
       been done already for those (e.g., typeinfo variables). */
    if (variable->source_corresp.is_class_member ||
        variable->source_corresp.parent.namespace_ptr != NULL) {
      final_entity_name_mangling(&variable->source_corresp);
    }  /* if */
  }  /* for */
}  /* do_scope_final_name_mangling */


void do_final_name_mangling(void)
/*
Do final name mangling for all type, function, and variable names.  This
must be done separately from and later than normal name mangling because
the simple form of the name must remain available for use in mangled names
(e.g., virtual function table variable names).
*/
{
  a_scope_orphaned_list_header_ptr solhp;

  /* Process the file scope and all subscopes in the file-scope memory
     region. */
  do_scope_final_name_mangling(il_header.primary_scope);
  /* Process local types by visiting the types on orphan lists. */
  for (solhp = il_header.scope_orphaned_list_headers;
       solhp != NULL;
       solhp = solhp->next) {
    do_type_list_final_name_mangling(solhp->orphaned_types);
  }  /* for */
}  /* do_final_name_mangling */

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
      char *bcp_name      = unmangled_name_of(&bcp->type->source_corresp);
      char *orig_bcp_name = unmangled_name_of(&orig_bcp->type->source_corresp);
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

static void mangled_derivation_name(a_derivation_step_ptr    dsp,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the indicated
derivation.  This is used for the base class part of virtual function
table names.
*/
{
  a_type_ptr class_type;

  /* The name must be put out backwards, so use recursion to get to the
     bottom of the list. */
  if (dsp->next != NULL) {
    mangled_derivation_name(dsp->next, mctl);
    /* Add two underscores to separate names. */
    add_str_to_mangled_name("__", mctl);
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
    mangled_basic_class_name(class_type, mctl);
  } else
#endif /* ABI_COMPATIBILITY_VERSION >= 230  && ... */
  /* Do not insert code here -- this is the "else" of an "if". */
  {
    /* Note the use of mangled_class_name_internal instead of
       mangled_vtbl_class_name because we do not want two lengths on
       the front of nested class names. */
    mangled_class_name_internal(class_type, mctl);
  }
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
    if (bcp->is_virtual && same_entities(bcp->type, dir_bcp->type)) {
      same_name_exists = TRUE;
      break;
    }  /* if */
  }  /* for */
  return same_name_exists;
}  /* virtual_base_class_of_same_name_exists */


static long ambiguous_base_class_number(a_base_class_ptr bcp)
/*
Return an "ambiguous base class number" for the (ambiguous) base class bcp,
to be used in qualifying its name in a virtual function table mangled
name.  A return value of -1 indicates the base class that cfront discards,
which gets special treatment; otherwise, the value is non-negative.
*/
{
  long             num, count = -1;
  a_base_class_ptr test_bcp;
  a_type_ptr       class_type = bcp->derived_class;

  check_assertion(bcp->ambiguous);
  for (test_bcp = base_classes_of(class_type);
       ;
       test_bcp = test_bcp->next) {
    check_assertion(test_bcp != NULL);
    /* Count ambiguous base classes with the same name, in order. */
    if (test_bcp->ambiguous &&
        same_entities(test_bcp->type, bcp->type)) {
      if (bcp->direct && !bcp->is_virtual &&
          virtual_base_class_of_same_name_exists(bcp)) {
        /* This base class is a direct nonvirtual base class and there is
           a virtual base class with the same name.  cfront discards this
           base class (and therefore its virtual function table too), so
           this one is always qualified, even if it is the first one.
           That allows us to generate the same mangled name (an unqualified
           one) for the base class that cfront does keep (at least, if there
           is only one of those). */
        num = -1;
      } else {
        count++;
        num = count;
      }  /* if */
      /* Stop if we have found the base class we were looking for.  num is
         the number to use for it. */
      if (test_bcp == bcp) break;
    }  /* if */
  }  /* for */
  return num;
}  /* ambiguous_base_class_number */


static void mangled_vtbl_base_class_name(a_base_class_ptr         bcp,
                                         a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of a base class in
a virtual function table.  The name describes the base class given by bcp.
*/
{
  a_derivation_step_ptr dsp;
  a_length_reservation  length_reservation;

  /* The form of the name is like
       4abcd
     or
       8abcd__ef  (this for base class "abcd" in "ef")
     For virtual base classes, or nonvirtual base classes within virtual
     base classes, the first step is directly to the virtual base class.
  */
  dsp = cast_derivation_path_of(bcp);
  /* Put out the name length and the name. */
  reserve_space_for_length(&length_reservation, mctl);
  mangled_derivation_name(dsp, mctl);
  fill_in_length(&length_reservation, mctl);
  if (bcp->ambiguous) {
    /* Ambiguous base classes get a suffix to differentiate the different
       like-named base classes. */
    long num = ambiguous_base_class_number(bcp);
    if (num == 0) {
      /* The first ambiguous base class gets no suffix. */
    } else {
      add_str_to_mangled_name("__A", mctl);
      if (num < 0) {
        /* The base class that cfront discards (a direct nonvirtual base class
           with the same name as a virtual base class) gets the simple "__A"
           encoding.  This is for historical reasons: until version 3.0 of
           the EDG C++ Front End, this was the only ambiguity qualifier
           (we hadn't realized that there were other possibilities), so
           this one is kept the same to avoid an ABI change. */
      } else {
        /* For other base classes, use a __Ann encoding, where nn is the
           base class number. */
        add_number_to_mangled_name((unsigned long)num, mctl);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* mangled_vtbl_base_class_name */


static void mangled_vtbl_class_name(a_type_ptr               type,
                                    a_mangling_control_block *mctl)
/*
Add to the mangled name the encoding for the name of the class "type"
for use in a virtual function table name.
*/
{
#if ABI_COMPATIBILITY_VERSION >= 230 && CFRONT_OBJECT_CODE_COMPATIBILITY
  /* cfront mode. */
  if (type_needs_parent_qualifier(type)) {
    /* The type is a nested type.  Add a length in front of the mangled
       form (e.g., "7Q2_1A1B" instead of "Q2_1A1B"). */
    a_length_reservation length_reservation;
    reserve_space_for_length(&length_reservation, mctl);
    mangled_type_name(type, mctl);
    fill_in_length(&length_reservation, mctl);
  } else {
    /* Not a nested type name; just put out the type encoding. */
    mangled_type_name(type, mctl);
  }  /* if */
#else /* ABI_COMPATIBILITY_VERSION < 230 || ... */
  /* In non-cfront mode, or in old ABI versions, just pass through to
     mangled_type_name. */
  mangled_type_name(type, mctl);
#endif /* ABI_COMPATIBILITY_VERSION >= 230 && ... */
}  /* mangled_vtbl_class_name */


#if !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/*ARGSUSED*/ /* <-- ctor_bcp is not used in that case. */
#endif /* !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
char *mangled_vtbl_name(a_type_ptr               class_type,
                        a_base_class_ptr         bcp,
                        a_base_class_ptr         ctor_bcp)
/*
Return the mangled name for the virtual function table for base class
bcp of class class_type.  If bcp == NULL, the virtual function table is
for class_type itself.  If ctor_bcp is non-NULL, it is the base class
for class_type as a subobject of some larger class type that is the
actual complete object type (used in determining layout); class_type
in that case is the type considered to be the complete object type
for purposes of overriding (this is used during constructors and
destructors).  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Determine the mangled name.  It is
       __vtbl__<mangled-base-class-name>__<mangled-class-name> or
       __vtbl__<mangled-class-name>
     The mangled-base-class-name is really a sort of pathname for the
     base class, giving the base class names from base to derived.
     For example, __vtbl__5X__X1__1B for base class X inside X1 inside
     a whole object of type B.
  */
  add_str_to_mangled_name("__vtbl__", &mctl);
  if (bcp != NULL) {
    /* Add the base class name. */
    mangled_vtbl_base_class_name(bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
  }  /* if */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
  if (ctor_bcp != NULL) {
    /* There is a complete class type, so the name looks like
       __vtbl__<mangled-base-class-name>__<mangled-base-class-name>
                                        __<mangled-complete-class-name>
    */
    /* Add the second base class name. */
    mangled_vtbl_base_class_name(ctor_bcp, &mctl);
    /* Add two underscores after the name. */
    add_str_to_mangled_name("__", &mctl);
    class_type = ctor_bcp->derived_class;
  }  /* if */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
  /* Add the derived class name. */
  mangled_vtbl_class_name(class_type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_vtbl_name */


char *mangled_class_name(a_type_ptr type)
/*
Return the mangled name of the class "type".  This is the encoding used
for the name of the class as opposed to the encoding for the class as
a type (for example, it has no length preceding a simple class name).
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  mangled_class_name_internal(type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_class_name */


void mangle_subobject_class_name(a_type_ptr class_type,
                                 a_type_ptr subobject_type)
/*
Set the mangled name of a type generated by IL lowering as the
type-as-subobject of class_type.
*/
{
  a_mangling_control_block mctl;
  char                     *temp_name, *new_name_ptr;

  if (has_name(class_type)) {
    start_mangling(&mctl);
    add_str_to_mangled_name("__SO__", &mctl);
    mangled_basic_class_name(class_type, &mctl);
    /* Not "final" because this type will go through the final processing
       later.  We don't want to (e.g.) compress twice. */
    temp_name = end_mangling((a_source_correspondence *)NULL,
                             /*final=*/FALSE, &mctl);
    new_name_ptr = alloc_lowered_name_string((sizeof_t)strlen(temp_name)+1);
    (void)strcpy(new_name_ptr, temp_name);
    subobject_type->source_corresp.name = new_name_ptr;
    subobject_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_subobject_class_name */


static char *mangled_prefixed_type_encoding(char       *prefix,
                                            a_type_ptr type)
/*
Return a mangled name that is the indicated prefix followed by the encoding
for the indicated type.  The name returned is in a temporary buffer and must
be copied elsewhere.
*/
{
  a_mangling_control_block mctl;
  char                     *buffer;

  start_mangling(&mctl);
  /* Start with the prefix. */
  add_str_to_mangled_name(prefix, &mctl);
  /* Add the mangled name of the type. */
  mangled_encoding_for_type(type, &mctl);
  buffer = end_mangling((a_source_correspondence *)NULL,
                        /*final=*/TRUE, &mctl);
  return buffer;
}  /* mangled_prefixed_type_encoding */


char *mangled_typeinfo_name(a_type_ptr type)
/*
Return the mangled name for the typeinfo variable for type "type".
A typeinfo variable is used to describe runtime type information.
The name returned is in a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       __T_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__T_", type);
}  /* mangled_typeinfo_name */


char *mangled_id_object_name(a_type_ptr type)
/*
Return the mangled name for the id object variable for type "type".
The id object variable is pointed to by the typeinfo variable used
to provide runtime type information.  The name returned is in
a temporary buffer and must be copied elsewhere.
*/
{
  /* The mangled name looks like
       __TID_<mangled-type-name>
  */
  return mangled_prefixed_type_encoding("__TID_", type);
}  /* mangled_id_object_name */

#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE

static unsigned long search_scope_list(a_scope_ptr scope,
                                       a_scope_ptr scope_to_search,
                                       a_boolean   *found)
/*
Look for "scope" in "scope_to_search".  If it is found, set *found to TRUE
and return the position where it was found: 0 means scope and scope_to_search
are the same scope; scopes under scope_to_search are numbered in tree
traversal order starting from 1.  If the scope is not found, *found is not
changed (it is expected to be FALSE) and the count of scopes in the tree is
returned.
*/
{
  unsigned long scope_number;

  if (scope == scope_to_search) {
    scope_number = 0;
    *found = TRUE;
  } else {
    a_scope_ptr sp;
    scope_number = 1;
    for (sp = scope_to_search->scopes; sp != NULL; sp = sp->next) {
      scope_number += search_scope_list(scope, sp, found);
      if (*found) break;
    }  /* for */
  }  /* if */
  return scope_number;
}  /* search_scope_list */


void mangle_promoted_entity_name(a_source_correspondence *scp,
                                 a_boolean               final,
                                 a_routine_ptr           routine,
                                 a_scope_ptr             scope)
/*
scp points to the source correspondence field of an entity that is being
promoted out of the routine "routine" (or one of its block scopes) to
the file scope.  scope indicates the scope out of which the entity is
being promoted (a function or block scope).  Give the entity a mangled
name if necessary.  If final is TRUE, do the final name mangling,
which may produce a name that can no longer be embedded in other
mangled names.
*/
{
  a_mangling_control_block mctl;
  unsigned long            scope_number;

  /* Leave the name alone if the entity is unnamed. */
  if (scp->name != NULL) {
    start_mangling(&mctl);
    /* Name mangling is needed. */
    /* The encoding is the original name, followed by "__Lnn", where "nn"
       is the scope number within the function, followed by two underscores,
       followed by the mangled name of the routine.  Note that the routine
       name has not been mangled yet (except when it has been copied from
       a secondary translation unit), but the entity's name has been (if
       it needs mangling). */
    check_assertion(!routine->source_corresp.name_has_been_mangled ||
                    routine->source_corresp.copied_from_secondary_trans_unit);
    /* Develop a scope number for the scope in which the entity appears.
       This number must be relative to the function rather than to the
       whole compilation so that if a given function (e.g., an extern inline
       function) is compiled in more than one compilation unit the scope
       number -- and therefore the mangled name -- will be the same in each
       compilation. */
    { a_scope_ptr rout_scope =
                            il_header.region_scope_entry[routine->assoc_scope];
      a_boolean   found = FALSE;
      scope_number = search_scope_list(scope, rout_scope, &found);
      check_assertion_str(found,
                          "mangle_promoted_entity_name: scope not found");
    }
    add_str_to_mangled_name(scp->name, &mctl);
    add_str_to_mangled_name("__L", &mctl);
    add_number_to_mangled_name((unsigned long)scope_number, &mctl);
    add_str_to_mangled_name("__", &mctl);
    if (routine->source_corresp.name != NULL) {
      if (routine->source_corresp.name_has_been_mangled &&
          !routine->source_corresp.
                               mangled_name_cannot_be_included_in_other_name) {
        /* Using the mangled name as written is important if the routine
           is a static function that has been externalized. */
        add_str_to_mangled_name(routine->source_corresp.name, &mctl);
      } else {
        mangled_function_name(routine, /*suppress_param_encoding=*/FALSE,
                              &mctl);
      }  /* if */
    }  /* if */
    (void)end_mangling(scp, final, &mctl);
  }  /* if */
}  /* mangle_promoted_entity_name */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE */
#endif /* DO_IL_LOWERING */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN

void mangle_covariant_return_type_entry_name(a_routine_ptr entry_routine,
                                             a_routine_ptr prim_routine,
                                             a_type_ptr    overridden_class)
/*
entry_routine points to a routine that represents an entry point of
prim_routine (which is a virtual function with a covariant return type).
The entry routine is like the primary routine, but does a derived-to-base
cast on the returned pointer.  The entry routine is used when this routine
is called in a way that requires the return type of the overridden
function in overridden_class.  Put the appropriate mangled name into
entry_routine (it has no name on entry).
*/
{
  a_mangling_control_block mctl;

  start_mangling(&mctl);
  /* The mangled name has the form
       __VFE__<overridden_class>__<prim_routine>
     where <overridden_class> and <prim_routine> are the mangled names for
     those entities. */
  add_str_to_mangled_name("__VFE__", &mctl);
  /* Add the class name. */
  mangled_type_name(overridden_class, &mctl);
  /* Add two underscores after the class name. */
  add_str_to_mangled_name("__", &mctl);
  /* Add the routine name. */
  mangled_function_name(prim_routine,
                        /*suppress_param_encoding=*/FALSE,
                        &mctl);
  (void)end_mangling(&entry_routine->source_corresp, /*final=*/TRUE, &mctl);
}  /* mangle_covariant_return_type_entry_name */

#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */


static a_compressible_string_pos_ptr alloc_compressible_string_pos(void)
/*
Allocate compressible string position entry, set its fields to default
values, and return a pointer to it.
*/
{
  a_compressible_string_pos_ptr cspp;

  if (avail_compressible_string_pos != NULL) {
    /* Reuse a freed entry. */
    cspp = avail_compressible_string_pos;
    avail_compressible_string_pos = cspp->next;
  } else {
    /* Allocate a new entry. */
    cspp = (a_compressible_string_pos_ptr)alloc_fe(
                                            sizeof(a_compressible_string_pos));
#if DEBUG
    num_compressible_string_pos_allocated++;
#endif /* DEBUG */
  }  /* if */
  cspp->next = NULL;
  cspp->str_pos = 0;
  return cspp;
}  /* alloc_compressible_string_pos */


static void free_compressible_string_pos(a_compressible_string_pos_ptr cspp)
/*
Free the indicated compressible string position entry by returning it
to the available list for reuse.
*/
{
  cspp->next = avail_compressible_string_pos;
  avail_compressible_string_pos = cspp;
}  /* free_compressible_string_pos */


static char *compress_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
Compress the mangled name that's been built up (pointed to by mangled_name,
with length given by mctl->length, including a terminating null).
If mangled_name is NULL, the mangled name is in mangling_text_buffer, starting
at offset 0.  It is important to pass NULL, and not a pointer to
mangling_text_buffer, in that case.  Return a pointer to the name, either
the original one or a compressed version.  The compressed version is
in mangling_text_buffer if mangled_name is NULL, and allocated in IL memory if
mangled_name is non-NULL.  mangling_text_buffer->size must indicate the
first available position in mangling_text_buffer (e.g., after the terminating
null of the mangled name).  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  char *compr_name = NULL;

/* Macro to determine the input buffer address.  This is recomputed each
   time it is needed because the mangling_text_buffer might move. */
#define src_mangled_name \
  ((mangled_name == NULL) ? mangling_text_buffer->buffer : mangled_name)

  /* See whether the name should be examined to see if it is
     compressible.  mctl->length indicates the length of the name,
     including the terminating null.  Don't try compression if the name
     is already fairly small.  Note that one advantage of avoiding
     compression on relatively small names is allowing more compatibility
     with libraries compiled by cfront.  The largest name noted in the
     iostream package had 56 characters. */
  if (compress_mangled_names && mctl->length >= 60) {
    /* Build up the compressed name in the mangling_text_buffer, following
       anything already in there (e.g., after the null character at the
       end of the mangled name). */
    /* Note that positions in the mangling_text_buffer are kept as offsets
       rather than pointers because the mangling_text_buffer may get moved
       if it is resized. */
    sizeof_t start_of_compressed_name = mangling_text_buffer->size;
    sizeof_t src_pos = 0;
    a_compressible_string_pos_ptr
             cspp;
    sizeof_t size_of_mangled_name = mctl->length; /* Including final null. */
    sizeof_t size_of_compressed_name, prefix_length;
    sizeof_t i;
    char     buffer[20];
#define NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE 64
    a_compressible_string_pos_ptr
             hash_table[NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE];
    /* Clear the hash table used to keep track of the position of
       compressible strings in the original mangled name. */
    memzero((char *)hash_table, sizeof(hash_table));
    
    for (;;) {
      /* Copy characters from the original name to the mangled name, looking
         for a string of digits (which starts a compressible section). */
      char ch = src_mangled_name[src_pos];
      if (ch == '\0') break;
      if (!isdigit((unsigned char)ch)) {
        add_char_to_text_buffer(mangling_text_buffer, ch);
        /* If a "J" appears, copy it as "JJ" to avoid confusion with the
           "J" markers used to indicate compression. */
        if (ch == 'J') add_char_to_text_buffer(mangling_text_buffer, 'J');
        src_pos++;
      } else {
        /* A digit.  This may be the start of a compressible string. */
        sizeof_t      num_digits = 1;
        sizeof_t      digit, length, hash_value;
        unsigned long value = (ch - '0');
        a_boolean     ovflo = FALSE, valid, compressed = FALSE;
        /* Determine the number of digits in the digit string and accumulate
           its value. */
        for (;;) {
          ch = src_mangled_name[src_pos+num_digits];
          if (!isdigit((unsigned char)ch)) break;
          digit = ch - '0';
          num_digits++;
          if (value > ULONG_MAX / 10) ovflo = TRUE;
          value *= 10;
          if (value > ULONG_MAX-digit) ovflo = TRUE;
          value += digit;
        }  /* for */
        /* See whether the length is valid. */
        if (ovflo) {
          valid = FALSE;
        } else if (value < 4) {
          /* Don't compress very small strings like "3ABC", because
             the compressed form is probably not smaller.  This also
             discards cases where a user variable has a name like "f2",
             which are not worth examining. */
          valid = FALSE;
        } else if ((length = num_digits + value),
                   size_of_mangled_name-src_pos <= length) {
          /* The length is too long -- it runs off the end of the mangled
             name.  That means it can't be a real length. */
          valid = FALSE;
        } else {
          /* The length is okay. */
          valid = TRUE;
        }  /* if */
        if (valid) {
          /* See whether the string has appeared previously by comparing
             it against the strings in the hash table. */
          hash_value = value % NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE;
          for (cspp = hash_table[hash_value];
               cspp != NULL;
               cspp = cspp->next) {
            /* Compare the previous string to this new string. */
            if (strncmp(src_mangled_name+cspp->str_pos,
                        src_mangled_name+src_pos,
                        size_t_arg(length)) == 0) {
              /* Found an identical previous string, so we can compress it. */
              compressed = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (compressed) {
          /* Replace the string by "JnnnJ", where "nnn" is the position of
             the previous identical string. */
          (void)sprintf(buffer, "J%luJ", (unsigned long)cspp->str_pos);
          add_string_to_text_buffer(mangling_text_buffer, buffer);
          /* Continue scanning the original string after the full string
             that was compressed away. */
          src_pos += length;
        } else {
          /* The string has not appeared previously.  If it's a valid
             string, remember it for possible later reuse. */
          if (valid) {
            cspp = alloc_compressible_string_pos();
            cspp->str_pos = src_pos;
            cspp->next = hash_table[hash_value];
            hash_table[hash_value] = cspp;
          }  /* if */
          /* Put out the digit string. */
          for (i = 0; i < num_digits; i++) {
            add_char_to_text_buffer(mangling_text_buffer,
                                    src_mangled_name[src_pos]);
            src_pos++;
          }  /* for */
          /* Continue scanning the original string after the digit
             string. */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Add the final null. */
    add_char_to_text_buffer(mangling_text_buffer, '\0');
    /* Free the entries in the hash table. */
    for (i = 0; i < NUM_BUCKETS_IN_COMPRESSION_HASH_TABLE; i++) {
      a_compressible_string_pos_ptr cspp_next;
      for (cspp = hash_table[i]; cspp != NULL; cspp = cspp_next) {
        cspp_next = cspp->next;
        free_compressible_string_pos(cspp);
      } /* for */
    }  /* for */
    /* The prefix on the compressed form is "__CPR" followed by the size
       of the original (uncompressed) name, not counting the final null. */
    (void)sprintf(buffer, "__CPR%lu__", (unsigned long)size_of_mangled_name-1);
    prefix_length = strlen(buffer);
#if EXPENSIVE_CHECKING
    /* Make sure the name does not already have the compression prefix in
       it.  If it does, we've used a previously compressed name in building
       up this name, and that won't work. */
    check_assertion_str(strstr(mangling_text_buffer->buffer +
                                                      start_of_compressed_name,
                               "__CPR") == NULL,
                        "compress_mangled_name: double compression");
#endif /* EXPENSIVE_CHECKING */
    size_of_compressed_name = (mangling_text_buffer->size -
                               start_of_compressed_name) +
                              prefix_length;
    /* Note that both size_of_compressed_name and size_of_mangled_name
       include the terminating null. */
    if (size_of_compressed_name < size_of_mangled_name) {
      /* The compressed name is shorter, so use it.  (There are some
         pathological cases where the compressed version might be larger.) */
      if (mangled_name == NULL) {
        /* The original mangled name is in mangling_text_buffer, preceding
           the compressed form. */
        /* Put the prefix out in front of the compressed name, and return
           the position of the prefix in that position as the address of
           the full compressed name. */
        check_assertion(start_of_compressed_name >= prefix_length);
        compr_name = mangling_text_buffer->buffer+start_of_compressed_name -
                     prefix_length;
        (void)memcpy(compr_name, buffer, size_t_arg(prefix_length));
      } else {
        /* The mangled name is not in mangling_text_buffer.  Allocate new IL
           memory for the compressed name, including the prefix. */
        compr_name = alloc_lowered_name_string(size_of_compressed_name);
        (void)memcpy(compr_name, buffer, size_t_arg(prefix_length));
        (void)strcpy(compr_name+prefix_length,
                     mangling_text_buffer->buffer+start_of_compressed_name);
      }  /* if */
      mangled_name = compr_name;
      /* Update the length, including the null terminator. */
      mctl->length = size_of_compressed_name;
      if (scp != NULL) {
        /* A compressed name cannot be used as part of another mangled name. */
        scp->mangled_name_cannot_be_included_in_other_name = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* If the name was not compressed, return the source mangled name address. */
  if (compr_name == NULL) compr_name = src_mangled_name;
  return compr_name;
#undef get_char_from_mangled_name
}  /* compress_mangled_name */


static char *truncate_mangled_name(char                     *mangled_name,
                                   a_source_correspondence  *scp,
                                   a_mangling_control_block *mctl)
/*
If necessary, truncate the mangled name that has been built up.  The
name is pointed to by mangled_name.  Its length (with terminating
null) is given by mctl->length.  If the name is longer than
max_mangled_name_length (and the latter is greater than zero), truncate it
by computing a CRC checksum and putting the checksum, in hex, at the end
of as much of the name as will fit along with the checksum.  Such
a truncated name is short enough, and likely to be unique, but it
cannot be demangled.  scp, if non-NULL, points to the source
correspondence entry for the entity whose name this is.
*/
{
  /* The suffix is of the form "__abcdabcd", i.e., one needs 10 characters
     for it. */
  sizeof_t max_allowed_length = max_mangled_name_length - 10;

  if (max_mangled_name_length != 0 && mctl->length-1 > max_allowed_length) {
    /* The name must be truncated. */
    (void)sprintf(mangled_name+max_allowed_length, "__%08lx",
                  crc_32(mangled_name, (unsigned long)0));
    mctl->length = max_mangled_name_length+1;
    if (scp != NULL) {
      /* A truncated name cannot be used as part of another mangled name. */
      scp->mangled_name_cannot_be_included_in_other_name = TRUE;
    }  /* if */
  }  /* if */
  return mangled_name;
}  /* truncate_mangled_name */


void name_lower_one_time_init(void)
/*
Do one-time initialization of variables related to name mangling.
*/
{
  /* Allocate the text buffer used for mangling. */
  mangling_text_buffer = alloc_text_buffer(2048);
  second_mangling_text_buffer = NULL;
  /* Save variables from lower_name.c that are needed for precompiled
     headers */
  if (exceptions_enabled && precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(unnamed_class_name_seed),
      pch_saved_var_array_elem(unnamed_enum_name_seed),
      pch_saved_var_array_elem(unnamed_member_variable_name_seed),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
  register_trans_unit_variable(unnamed_class_name_seed);
  register_trans_unit_variable(unnamed_enum_name_seed);
  register_trans_unit_variable(unnamed_member_variable_name_seed);
}  /* name_lower_one_time_init */


void name_lower_init(void)
/*
Initialize static variables related to name mangling that must be
initialized for each compilation.
*/
{
  unnamed_class_name_seed = 0;
  unnamed_enum_name_seed = 0;
  unnamed_member_variable_name_seed = 0;
  avail_compressible_string_pos = NULL;
#if DEBUG
  num_compressible_string_pos_allocated = 0;
#endif /* DEBUG */
}  /* name_lower_init */

#endif /* NEED_NAME_MANGLING */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2001 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
