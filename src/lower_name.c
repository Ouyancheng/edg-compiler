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

#include "basics.h"
#include "host_envir.h"

/* Only include this code if it is needed: */
#if NEED_NAME_MANGLING

#include "lower_name.h"
#include "lower_il.h"
#include "const_ints.h"
#include "float_pt.h"
#include "types.h"
#include "mem_manage.h"


static sizeof_t mangled_encoding_for_type(a_type_ptr type,
                                          char       *store_at);
static sizeof_t mangled_function_name(a_routine_ptr routine,
                                      a_boolean     suppress_param_encoding,
                                      char          *store_at);
static sizeof_t mangled_static_data_member_name(a_variable_ptr variable,
                                                char           *store_at);


#if AUTOMATIC_TEMPLATE_INSTANTIATION
/*
Dynamically allocated buffer used to contain transient mangled names,
those created to find out the mangled name of an entity without recording
the mangled name in the IL entry.
*/
static char	*mangled_name_buffer = NULL;
			/* Not allocated on a per-file basis. */
#define MANGLED_NAME_BUFFER_INITIAL_ALLOCATION 300
#define MANGLED_NAME_BUFFER_INCREMENTAL_ALLOCATION 300
			/* Initial and incremental allocation sizes for
			   mangled_name_buffer.  The initial allocation
			   should be such that almost all cases can be
			   accepted (so that the realloc is hardly ever
			   needed). */
/* See lower_il.h for size_mangled_name_buffer. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if AUTOMATIC_TEMPLATE_INSTANTIATION

static void expand_mangled_name_buffer(sizeof_t size_needed)
/*
Expand the mangled_name_buffer by reallocating it, so that its total size is at
least size_needed.  Called by ensure_mangled_name_buffer_space.
*/
{
  sizeof_t new_size;

  db_enter(4, "expand_mangled_name_buffer");
  new_size = size_mangled_name_buffer +
             MANGLED_NAME_BUFFER_INCREMENTAL_ALLOCATION;
  if (new_size < size_needed) new_size  = size_needed;
  mangled_name_buffer = realloc_general(mangled_name_buffer,
                                        size_mangled_name_buffer, new_size);
  size_mangled_name_buffer = new_size;
  db_exit();
}  /* expand_mangled_name_buffer */


/*
Ensure that mangled_name_buffer has at least size_needed bytes in it.
If not, expand mangled_name_buffer by reallocating it.
*/
#define ensure_mangled_name_buffer_space(size_needed)                 \
{ if (size_mangled_name_buffer < size_needed) {                       \
    expand_mangled_name_buffer((sizeof_t)(size_needed));              \
  }  /* if */                                                         \
}  /* ensure_mangled_name_buffer_space */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

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


static sizeof_t digits_to_represent_with_underscore(unsigned long value)
/*
Like digits_to_represent, returns the number of digits needed to represent
the value.  However, if the number of digits is greater than one, add
one to account for an underscore following the digits to separate them from
things following in the mangled name.
*/
{
  sizeof_t ndigits = digits_to_represent(value);

  if (ndigits > 1) ndigits++;
  return ndigits;
}  /* digits_to_represent_with_underscore */


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
                                                   char       *store_at)
/*
Determine the mangled encoding for the function type "type".  Place the
encoded form at *store_at if store_at != NULL, and (always) return the
length of the encoding.  See ARM 7.2.1c for name encoding.
*/
{
  /* The encoding for a function type is "F" followed by the encoding
     for the parameter types.  mangled_function_name takes care of putting
     out additional information preceding the "F" if the function is a
     member function. */
  /* Start with the "F" indicating a function type. */
  if (store_at != NULL) *store_at++ = 'F';
  return 1 + mangled_encoding_for_parameter_types(type, store_at);
}  /* mangled_encoding_for_function_type */


static sizeof_t mangled_encoding_for_function_qualifiers(a_type_ptr type,
                                                         char       *store_at)
/*
Determine the mangled encoding for the type qualifiers (if any) on the
member function type "type".  Place the encoded form at *store_at if
store_at != NULL, and (always) return the length of the encoding.
*/
{
  sizeof_t   mangled_name_length = 0;
  a_type_ptr this_param_type;

  type = skip_typerefs(type);
  this_param_type = type->variant.routine.extra_info->implicit_this_param_type;
  if (this_param_type != NULL) {
    /* The function is a nonstatic member function. */
    this_param_type = type_pointed_to(this_param_type);
    /* Add any qualifiers on the "this" parameter type (actually, the type
       pointed to by the "this" parameter). */
    if (is_top_level_const_qualified_type(this_param_type)) {
      mangled_name_length++;
      if (store_at != NULL) *store_at++ = 'C';
    }  /* if */
    if (is_top_level_volatile_qualified_type(this_param_type)) {
      mangled_name_length++;
      if (store_at != NULL) *store_at++ = 'V';
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
                                        char          *store_at)
/*
Store the decimal representation of value at *store_at.  If the representation
takes more than one digit, add an underscore after it.  digits indicates
the size of the output including the underscore.
*/
{
  (void)sprintf(store_at, "%lu%s", value, (digits > 1) ? "_" : "");
}  /* store_digits_and_underscore */


static sizeof_t literal_representation(a_constant_ptr con,
                                       char           *store_at)
/*
Place the literal form of the constant con at *store_at if store_at != NULL,
and (always) return the length of the literal representation.  This is
used to encode constants as part of the mangled names of template classes.
*/
{
  sizeof_t       literal_length, str_length, digits;
  char           *str;
  char           buffer[50];

  switch (con->kind) {
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
      digits = digits_to_represent_with_underscore((unsigned long)str_length);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        store_digits_and_underscore((unsigned long)str_length, digits,
                                    store_at);
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
      digits = digits_to_represent_with_underscore((unsigned long)str_length);
      literal_length = 1 + digits + str_length;
      if (store_at != NULL) {
        *store_at++ = 'L';
        store_digits_and_underscore((unsigned long)str_length, digits,
                                    store_at);
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
        a_type_ptr           class_type = NULL;
        a_routine_ptr        routine;
        an_address_base_kind abkind;

        check_assertion(con->variant.address.offset == 0);
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
          class_type = variable->source_corresp.class_of_which_a_member;
          if (class_type != NULL) {
            /* Static data member. */
            str_length = mangled_static_data_member_name(variable,
                                                         (char *)NULL);
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
            if (class_type != NULL) {
              /* Static data member. */
              (void)mangled_static_data_member_name(variable, store_at);
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
        digits= digits_to_represent_with_underscore((unsigned long)str_length);
        literal_length = 1 + digits + str_length;
        if (store_at != NULL) {
          *store_at++ = 'L';
          store_digits_and_underscore((unsigned long)str_length, digits,
                                      store_at);
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
        digits= digits_to_represent_with_underscore((unsigned long)str_length);
        literal_length += 2 + digits + str_length + 1;
        if (store_at != NULL) {
          *store_at++ = '_';
          *store_at++ = 'L';
          store_digits_and_underscore((unsigned long)str_length, digits,
                                      store_at);
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
#if CHECKING
    case ck_string:
      /* Strings should be converted to addresses. */
    default:
      internal_error("literal_representation: bad constant kind");
#endif /* CHECKING */
  }  /* switch */
  return literal_length;
}  /* literal_representation */


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
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_class_a_name */


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
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* give_unnamed_enum_a_name */


static sizeof_t mangled_template_arguments(a_type_ptr type,
                                           char       *store_at)
/*
Determine the mangled form of the actual parameters of the template class
"type".  Place the mangled name at *store_at if store_at != NULL, and (always)
return the length of the name.
*/
{
  sizeof_t           mangled_name_length, digits, arg_length, total_arg_length;
  sizeof_t           literal_length, type_length;
  a_template_arg_ptr template_arg_list =
                            type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
  a_template_arg_ptr tap;
  a_constant_ptr     con;
  int                pass;

  /* The mangled form of the parameters is something like
       3_ii
         ^^--- Two template arguments of type int.
       ^------ Total length of template argument list string,
	       including the underscore.
  */
  mangled_name_length = 0;
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
        /* Constant argument.  Representation is something like
             XCiL15   <-- integer constant 5
                  ^-- Literal constant representation.
                 ^--- Length of literal constant.
                ^---- L indicates literal constant; c indicates address
                      of variable, etc.
              ^^----- Type of template argument, with "const" added.
             ^------- X indicates beginning of constant argument.
        */
        con = tap->variant.constant;
        if (pass == 1) {
          arg_length = 2; /* "XC" */
          arg_length += mangled_encoding_for_type(con->type, (char *)NULL);
          literal_length = literal_representation(con, (char *)NULL);
          arg_length += literal_length;
        } else {
          mangled_name_length += 2;
          *store_at++ = 'X';
          *store_at++ = 'C';
          type_length = mangled_encoding_for_type(con->type, store_at);
          mangled_name_length += type_length;
          store_at += type_length;
          literal_length = literal_representation(con, store_at);
          mangled_name_length += literal_length;
          store_at += literal_length;
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


static sizeof_t mangled_basic_class_name(a_type_ptr type,
                                         char       *store_at)
/*
Determine the mangled form of the basic name of the class "type".  This is
not the version that contains a leading count of the number of characters
in the name; here, the name is usually just the original name, but is
different if the class is a template class or is unnamed.  Place the mangled
name at *store_at if store_at != NULL, and (always) return the length of
the name.
*/
{
  sizeof_t           mangled_name_length;
  char               *name;
  a_template_arg_ptr template_arg_list =
                            type->variant.class_struct_union.extra_info->
                                                             template_arg_list;

  /* Always start with the name of the class, which applies even in the
     template class case. */
  give_unnamed_class_a_name(type);
  name = type->source_corresp.name;
  mangled_name_length = strlen(name);
  if (store_at != NULL) {
    (void)memcpy(store_at, name, size_t_arg(mangled_name_length));
    store_at += mangled_name_length;
  }  /* if */
  if (!type->source_corresp.name_has_been_mangled) {
    if (template_arg_list != NULL) {
      /* A template class.  The mangled form of the name is something like
           abc__pt__3_ii
                      ^^--- Two template arguments of type int.
                    ^------ Total length of template argument list string,
                            including the underscore.
                ^^--------- Fixed string, indicates "parameterized type".
           ^^^------------- The name of the class template.
      */
#define PT_STR "__pt__"
      mangled_name_length += sizeof(PT_STR) - 1;
      if (store_at != NULL) {
        (void)strcpy(store_at, PT_STR);
        store_at += sizeof(PT_STR) - 1;
      }  /* if */
#undef PT_STR
      mangled_name_length += mangled_template_arguments(type, store_at);
    }  /* if */
    /* If the class is a local class, put out "__Lnn" using the declaration
       scope number for "nn".  This is not from the ARM.  cfront uses a
       similar form but it also includes the function mangling in the name
       and the number is probably different. */
    /* Don't do this for nested classes. */
    if (type->source_corresp.class_of_which_a_member == NULL) {
      a_symbol_ptr assoc_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
      if (assoc_sym->decl_scope != scope_stack[DEPTH_OF_FILE_SCOPE].number) {
        /* This is a local name. */
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
  }  /* if */
  return mangled_name_length;
}  /* mangled_basic_class_name */


static sizeof_t r_mangled_type_name(a_type_ptr    type,
                                    unsigned long nesting_level,
                                    char          *store_at)
/*
Determine the mangled form of the name of the type "type".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.  This routine should
not be called directly; mangled_type_name should be used instead.
A top-level call is made with nesting_level == 1; this routine then makes
recursive calls to itself with higher nesting levels to process the
initial parts of the qualified names.
*/
{
  sizeof_t   mangled_name_length, name_length;
  char       *name;
  sizeof_t   digits;
  a_type_ptr parent_class;

  /* The mangled form of a type name is the type name with a length
       preceding it:
         AB          --> 2AB
         ABCDEFGHIJK --> 11ABCDEFGHIJK
     The ARM (7.2.1c) also gives a syntax for encoding qualified class names,
     like "outer::inner", using a "Q" description:
       Q2_5outer5inner
          ^-----^-----mangled class names, outer to inner
        ^----count of levels of qualification
     Note that the ARM description does not include the underscore, which
     is necessary if you allow more than 9 levels of nesting.
  */
  mangled_name_length = 0;
  parent_class = type->source_corresp.class_of_which_a_member;
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  /* If this a nested type name promoted into the file scope in
     cfront 2.1 mode, do not use the nested form. */
  if (type->use_cfront_transitional_nested_type_name_mangling) {
  } else
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
  if (parent_class != NULL) {
    /* Nested type.  Do the containing class names. */
    name_length = r_mangled_type_name(parent_class, nesting_level+1, store_at);
    mangled_name_length += name_length;
    if (store_at != NULL) store_at += name_length;
  } else if (!is_immediate_class_type(type)) {
    /* The type is not a class type (it's a typedef or enum). */
  } else {
    /* Got to the topmost class. */
    if (nesting_level > 1) {
      /* More than one level of nesting, so put out the "Qn_". */
      digits = digits_to_represent(nesting_level);
      mangled_name_length += 2 + digits;
      if (store_at != NULL) {
        /* Actually store the "Qn_". */
        (void)sprintf(store_at, "Q%lu_", nesting_level);
        store_at += 2 + digits;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Put the innermost type name onto the name. */
  /* The name is preceded by a count of the number of characters in
     the name. */
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
  return mangled_name_length;
}  /* r_mangled_type_name */


static sizeof_t mangled_type_name(a_type_ptr type,
                                  char       *store_at)
/*
Determine the mangled form of the name of the type "type".  Place the
mangled name at *store_at if store_at != NULL, and (always) return the
length of the name.  See ARM 7.2.1c for name encoding.  This routine is
used for named types (classes, enums, and typedefs) and for unnamed classes.
*/
{
  return r_mangled_type_name(type, (unsigned long)1, store_at);
}  /* mangled_type_name */


sizeof_t mangled_class_name(a_type_ptr type,
                            char       *store_at)
/*
Determine the mangled form of the name of the class "type".  This is
the same as the basic class name if the class is not nested, and a
nested name encoding if the class is nested.  This is used for things
like the names of base class pointers.  Place the mangled name at
*store_at if store_at != NULL, and (always) return the length of the name.
*/
{
  sizeof_t mangled_name_length;

  if (type->source_corresp.class_of_which_a_member != NULL &&
      type->source_corresp.name != NULL &&
      !type->source_corresp.name_has_been_mangled
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
      /* If this a cfront 2.1 nested type, leave it in the unnested form. */
      && !type->use_cfront_transitional_nested_type_name_mangling
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
                                       ) {
    /* Use a nested type name. */
    mangled_name_length = mangled_type_name(type, store_at);
  } else {
    /* Use the basic class name. */
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
  a_type_ptr named_type, named_typedef, pm_base_type;
  sizeof_t   mangled_name_length, section_length;
  char       *s;
  a_boolean  is_const, is_volatile;

  mangled_name_length = 0;
  /* Walk through any typerefs above the type.  Remember type qualifiers,
     remember the bottommost named typedef, and skip down to the "real"
     underlying type. */
  named_typedef = NULL;
  is_const = is_volatile = FALSE;
  for (; type->kind == (a_type_kind)tk_typeref;
       type = type->variant.typeref.type) {
    /* Remember type qualifiers encountered. */
    if (type->variant.typeref.is_const)    is_const = TRUE;
    if (type->variant.typeref.is_volatile) is_volatile = TRUE;
    /* Remember the bottommost named typedef encountered. */
    if (type->source_corresp.name != NULL) named_typedef = type;
  }  /* for */
  /* Put out type qualifiers, if any. */
  if (is_const) {
    mangled_name_length += 1;
    if (store_at != NULL) *store_at++ = 'C';
  }  /* if */
  if (is_volatile) {
    mangled_name_length += 1;
    if (store_at != NULL) *store_at++ = 'V';
  }  /* if */
  /* See if the type is a named class or enum. */
  named_type = NULL;
  if (is_enum_type(type)) {
    if (type->source_corresp.name != NULL) {
      /* Named enum. */
      named_type = type;
    } else {
      /* Unnamed enum; if there is a named typedef above the enum, use its
         name.  Note that we use the typedef name even it it's the name of
         a qualified version of the enum; that's what cfront does. */
      named_type = named_typedef;
    }  /* if */
  } else if (is_immediate_class_type(type)) {
    /* Class type. */
    if (type->source_corresp.name != NULL) named_type = type;
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
      case tk_void:
        s = "v";
        break;
      case tk_integer:
        if (type->variant.integer.enum_type) {
          /* Unnamed enum.  mangled_type_name will make up a name. */
          mangled_name_length += mangled_type_name(type, store_at);
          goto have_whole_mangled_name;
        }  /* if */
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
          case ik_long_long:      s = "ll"; break;
          case ik_unsigned_long_long:
                                  s = "Ull";break;
#endif /* LONG_LONG_ALLOWED */
#if CHECKING
          default:
            internal_error("mangled_encoding_for_type: bad int kind");
#endif /* CHECKING */
        }  /* switch */
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
        section_length = mangled_encoding_for_function_type(type, store_at);
        mangled_name_length += section_length;
        if (store_at != NULL) store_at += section_length;
        /* Add the return type at the end, as "_" followed by the type. */
        mangled_name_length++;
        if (store_at != NULL) *store_at++ = '_';
        mangled_name_length +=
                  mangled_encoding_for_type(type->variant.routine.return_type,
                                            store_at);
        goto have_whole_mangled_name;
      case tk_class:
      case tk_struct:
      case tk_union:
        /* Unnamed classes.  mangled_type_name will make up a name. */
        mangled_name_length += mangled_type_name(type, store_at);
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
        check_assertion(!type->variant.array.is_variable_size_array);
        section_length =
           digits_to_represent((unsigned long)type->variant.array.
                                               variant.number_of_elements) + 1;
        mangled_name_length += section_length;
        if (store_at != NULL) {
          (void)sprintf(store_at, "%lu_",
                        (unsigned long)type->
                                     variant.array.variant.number_of_elements);
          store_at += section_length;
        }  /* if */
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
names.
*/
{
  char *name;

  switch (kind) {
    case onk_new:               /* "new" */
      name = "__nw";
      break;
    case onk_delete:            /* "delete" */
      name = "__dl";
      break;
    case onk_plus:              /* "+" */
      name = "__pl";
      break;
    case onk_minus:             /* "-" */
      name = "__mi";
      break;
    case onk_star:              /* "*" */
      name = "__ml";
      break;
    case onk_divide:            /* "/" */
      name = "__dv";
      break;
    case onk_remainder:         /* "%" */
      name = "__md";
      break;
    case onk_excl_or:           /* "^" */
      name = "__er";
      break;
    case onk_ampersand:         /* "&" */
      name = "__ad";
      break;
    case onk_or:                /* "|" */
      name = "__or";
      break;
    case onk_compl:             /* "~" */
      name = "__co";
      break;
    case onk_not:               /* "!" */
      name = "__nt";
      break;
    case onk_assign:            /* "=" */
      name = "__as";
      break;
    case onk_lt:                /* "<" */
      name = "__lt";
      break;
    case onk_gt:                /* ">" */
      name = "__gt";
      break;
    case onk_plus_assign:       /* "+=" */
      name = "__apl";
      break;
    case onk_minus_assign:      /* "-=" */
      name = "__ami";
      break;
    case onk_times_assign:      /* "*=" */
      name = "__amu";
      break;
    case onk_divide_assign:     /* "/=" */
      name = "__adv";
      break;
    case onk_remainder_assign:  /* "%=" */
      name = "__amd";
      break;
    case onk_excl_or_assign:    /* "^=" */
      name = "__aer";
      break;
    case onk_and_assign:        /* "&=" */
      name = "__aad";
      break;
    case onk_or_assign:         /* "|=" */
      name = "__aor";
      break;
    case onk_shift_left:        /* "<<" */
      name = "__ls";
      break;
    case onk_shift_right:       /* ">>" */
      name = "__rs";
      break;
    case onk_shift_right_assign:/* ">>=" */
      name = "__ars";
      break;
    case onk_shift_left_assign: /* "<<=" */
      name = "__als";
      break;
    case onk_eq:                /* "==" */
      name = "__eq";
      break;
    case onk_ne:                /* "!=" */
      name = "__ne";
      break;
    case onk_le:                /* "<=" */
      name = "__le";
      break;
    case onk_ge:                /* ">=" */
      name = "__ge";
      break;
    case onk_and_and:           /* "&&" */
      name = "__aa";
      break;
    case onk_or_or:             /* "||" */
      name = "__oo";
      break;
    case onk_plus_plus:         /* "++" */
      name = "__pp";
      break;
    case onk_minus_minus:       /* "--" */
      name = "__mm";
      break;
    case onk_comma:             /* "," */
      name = "__cm";
      break;
    case onk_arrow_star:        /* "->*" */
      name = "__rm";
      break;
    case onk_arrow:             /* "->" */
      name = "__rf";
      break;
    case onk_function_call:     /* "()" */
      name = "__cl";
      break;
    case onk_subscript:         /* "[]" */
      name = "__vc";
      break;
#if CHECKING
    default:
      internal_error("mangled_operator_name: bad kind");
#endif /* CHECKING */
  }  /* switch */
  return name;
}  /* mangled_operator_name */


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
  sizeof_t     mangled_name_length, section_length;
  char         *name;
  a_type_ptr   class_type, conversion_type, routine_type;

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
    switch (routine->special_kind) {
      case sfk_constructor:
        name = "__ct";
        break;
      case sfk_destructor:
        name = "__dt";
        break;
      case sfk_conversion:
        name = "__op";
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
  if (store_at != NULL) {
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
  /* See if the function is a member function. */
  class_type = routine->source_corresp.class_of_which_a_member;
  /* If we will be adding the class name or the parameter types, put out
     two underscores to separate the function name from the rest. */
  if (class_type != NULL || !suppress_param_encoding) {
    /* Add two underscores after the name. */
    mangled_name_length += 2;
    if (store_at != NULL) {
      *store_at++ = '_';
      *store_at++ = '_';
    }  /* if */
  }  /* if */
  if (class_type != NULL) {
    /* Put out the name of the class of which this function is a member. */
    section_length = mangled_type_name(class_type, store_at);
    mangled_name_length += section_length;
    if (store_at != NULL) store_at += section_length;
  }  /* if */
  if (!suppress_param_encoding) {
    if (class_type != NULL) {
      /* Member function.  Put out the qualifiers on the member function
         type. */
      section_length = mangled_encoding_for_function_qualifiers(routine_type,
                                                                store_at);
      mangled_name_length += section_length;
      if (store_at != NULL) store_at += section_length;
    }  /* if */
    /* Now output the function type, including the parameter types. */
    section_length = mangled_encoding_for_function_type(routine_type,
                                                        store_at);
    mangled_name_length += section_length;
  }  /* if */
  return mangled_name_length;
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
      routine->source_corresp.name = mangled_name;
      routine->source_corresp.name_has_been_mangled = TRUE;
    }  /* if */
  }  /* if */
}  /* mangle_function_name */

#if AUTOMATIC_TEMPLATE_INSTANTIATION

char *get_mangled_function_name(a_routine_ptr routine)
/*
Get the mangled name for the indicated routine, and return a pointer
to it.  If the routine name has not been mangled yet, create a copy
of the mangled name in mangled_name_buffer but do not change the
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
    /* Make sure we have enough space in mangled_name_buffer. */
    alloc_length = mangled_name_length + 1;
    ensure_mangled_name_buffer_space(alloc_length);
    mangled_name = mangled_name_buffer;
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
                                    char                    *store_at)
/*
Determine the mangled form of the name of the class member whose source
correspondence is given by scp.  Place the mangled name at *store_at if
store_at != NULL, and (always) return the length of the name.  See ARM
7.2.1c for name encoding.  This routine must be called only for static
data member variables and member constants.
*/
{
  sizeof_t mangled_name_length, section_length;
  char     *name;

  /* The mangled name of a static data member or member constant is the
     original name followed by two underscores followed by the mangled
     class name.  For example:
       AB::xy --> xy__2AB
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
  /* Add two underscores after the name. */
  mangled_name_length += 2;
  if (store_at != NULL) {
    *store_at++ = '_';
    *store_at++ = '_';
  }  /* if */
  /* Output the mangled class name. */
  section_length = mangled_type_name(scp->class_of_which_a_member, store_at);
  mangled_name_length += section_length;
  return mangled_name_length;
}  /* mangled_member_name */


static sizeof_t mangled_static_data_member_name(a_variable_ptr variable,
                                                char           *store_at)
/*
Determine the mangled form of the name of the static data member "variable".
Place the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.  See ARM 7.2.1c for name encoding.  This routine
must be called only for static data member variables.
*/
{
  return mangled_member_name(&variable->source_corresp, store_at);
}  /* mangled_static_data_member_name */


static void mangle_static_data_member_name(a_variable_ptr variable)
/*
Mangle the name of the indicated static data member.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  if (!variable->source_corresp.name_has_been_mangled) {
    error_position = variable->source_corresp.decl_position;
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_static_data_member_name(variable,
                                                          (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)mangled_static_data_member_name(variable, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    variable->source_corresp.name = mangled_name;
    variable->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_static_data_member_name */

#if AUTOMATIC_TEMPLATE_INSTANTIATION

char *get_mangled_static_data_member_name(a_variable_ptr variable)
/*
Get the mangled name for the indicated static data member, and return
a pointer to it.  If the variable name has not been mangled yet, create a
copy of the mangled name in mangled_name_buffer but do not change the
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
    mangled_name_length = mangled_static_data_member_name(variable,
                                                          (char *)NULL);
    /* Make sure we have enough space in mangled_name_buffer. */
    alloc_length = mangled_name_length + 1;
    ensure_mangled_name_buffer_space(alloc_length);
    mangled_name = mangled_name_buffer;
    /* Create the name. */
    (void)mangled_static_data_member_name(variable, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
  }  /* if */
  return mangled_name;
}  /* get_mangled_static_data_member_name */

#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

static void mangle_member_constant_name(a_constant_ptr con)
/*
Mangle the name of the indicated member constant, if necessary.  con
is either an enumerator constant or (as an extension) a declared member
constant.
*/
{
  sizeof_t mangled_name_length, alloc_length;
  char     *mangled_name;

  if (!con->source_corresp.name_has_been_mangled) {
    error_position = con->source_corresp.decl_position;
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_member_name(&con->source_corresp,
                                              (char *)NULL);
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)mangled_member_name(&con->source_corresp, mangled_name);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    con->source_corresp.name = mangled_name;
    con->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_member_constant_name */


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
    class_type->source_corresp.name = mangled_name;
    class_type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_class_name */


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
  if (type->source_corresp.class_of_which_a_member != NULL &&
      type->source_corresp.name != NULL &&
      !type->source_corresp.name_has_been_mangled
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
      /* If this a cfront 2.1 nested type, leave it in the unnested form. */
      && !type->use_cfront_transitional_nested_type_name_mangling
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
                                       ) {
    /* Nested type names must be mangled (because they exist in a scope
       that does not exist in the generated C code).  The mangled form
       is something like
         __Q2_1A1B
       The "Q2_1A1B" part is the normal representation for a mangled
       name, and the prefix makes it unique (i.e., makes it distinct
       from all user identifiers). */
    /* Determine how long the mangled name is. */
    mangled_name_length = mangled_type_name(type, (char *)NULL) +
                          2;  /* "__" */
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    mangled_name[0] = '_';
    mangled_name[1] = '_';
    (void)mangled_type_name(type, mangled_name + 2);
    mangled_name[mangled_name_length] = '\0';
    /* Note that the mangled name is not put into the type until after it has
       been completely built, because the old name is used in building the
       mangled form. */
    type->source_corresp.name = mangled_name;
    type->source_corresp.name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_nested_type_name */


static void do_scope_class_name_mangling(a_scope_ptr scope)
/*
Do name mangling for class names in scope and all its sub-scopes.  Note
that this does not include special processing for nested class names.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;

  /* Visit all types to find all class types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      mangle_class_name(type);
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) do_scope_class_name_mangling(class_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_class_name_mangling(block_scope);
  }  /* for */
}  /* do_scope_class_name_mangling */


static void do_scope_other_name_mangling(a_scope_ptr scope)
/*
Do name mangling for things other than classes (e.g., functions, static
data members) in scope and all its sub-scopes.
*/
{
  a_routine_ptr  routine;
  a_variable_ptr variable;
  a_type_ptr     type;
  a_constant_ptr con;
  a_scope_ptr    class_scope, block_scope;

  /* Visit all types to find all class types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      /* Make sure the type-as-subobject for a class gets the class name
         before it is changed, if it is a nested class name. */
      prelower_class_type(type);
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) do_scope_other_name_mangling(class_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_other_name_mangling(block_scope);
  }  /* for */
  /* Visit all routines. */
  for (routine = scope->routines; routine != NULL; routine = routine->next) {
    mangle_function_name(routine);
  }  /* for */
  /* If this is a class scope, visit the static data member variables,
     enum constants, and class constants. */
  if (scope->kind == (a_scope_kind)sck_class_struct_union) {
    /* Look for static data members and mangle their names. */
    for (variable = scope->variables;
         variable != NULL;
         variable = variable->next) {
      mangle_static_data_member_name(variable);
    }  /* for */
    /* Look for enum types and mangle the names of their constants. */
    for (type = scope->types; type != NULL; type = type->next) {
      if (is_immediate_enum_type(type)) {
        a_constant_ptr enum_con;
        for (enum_con = type->variant.integer.enum_info.constant_list;
             enum_con != NULL;
             enum_con = enum_con->next) {
          mangle_member_constant_name(enum_con);
        }  /* for */
      }  /* if */
    }  /* for */
    /* Look for member constants (an extension) and mangle their names. */
    for (con = scope->constants; con != NULL; con = con->next) {
      mangle_member_constant_name(con);
    }  /* for */
  }  /* if */
}  /* do_scope_other_name_mangling */


static void do_scope_nested_type_name_mangling(a_scope_ptr scope)
/*
Do name mangling for nested type names in scope and all its sub-scopes.
This must be done separately from and later than normal type name mangling
because the simple form of the name must remain available for use in
mangled names (e.g., virtual function table variable names).
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;

  /* Visit all types to find all named types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to change their names
     now. */
  for (type = scope->types; type != NULL; type = type->next) {
    if (is_immediate_class_type(type)) {
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) {
        do_scope_nested_type_name_mangling(class_scope);
      }  /* if */
    }  /* if */
    /* Note that the call here must be done after all subscopes have been
       visited; we don't want to change the name of a class until the
       classes nested within it have been processed. */
    mangle_nested_type_name(type);
  }  /* for */
  /* Visit all block scopes. */
  for (block_scope = scope->scopes;
       block_scope != NULL;
       block_scope = block_scope->next) {
    do_scope_nested_type_name_mangling(block_scope);
  }  /* for */
}  /* do_scope_nested_type_name_mangling */


void do_memory_region_name_mangling(a_scope_ptr scope)
/*
Do any required name mangling of members of the indicated scope and all
sub-scopes in the same memory region.
*/
{
  /* Mangle class names, not including special processing for nested
     class names. */
  do_scope_class_name_mangling(scope);
  /* Do function and static data member name mangling. */
  do_scope_other_name_mangling(scope);
  /* Mangle nested type names. */
  do_scope_nested_type_name_mangling(scope);
}  /* do_memory_region_name_mangling */


static sizeof_t mangled_derivation_name(a_derivation_step_ptr dsp,
                                        char                  *store_at)
/*
Determine the mangled form of the name of the indicated derivation.  Place
the mangled name at *store_at if store_at != NULL, and (always) return
the length of the name.
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
  name_length = mangled_class_name(class_type, store_at);
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
  section_length = mangled_type_name(class_type, store_at);
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
                                 a_routine_ptr           routine)
/*
scp points to the source correspondence field of an entity that is being
promoted out of the routine "routine" (or one of its block scopes) to
the file scope.  Give the entity a mangled name if necessary (e.g.,
if the function is a template function).  This routine is called only
once for each entity, and that is after normal name mangling has been done.
*/
{
  sizeof_t mangled_name_length, alloc_length, name_length, routine_name_length;
  char     *mangled_name, *store_at;

  if (routine->is_template_function && routine->source_corresp.name != NULL &&
      scp->name != NULL) {
    /* The routine is an instantiation of a template, so name mangling is
       needed.  Without it, two instances of the same function might promote
       two instances of the same entity to file scope.  Everything about them
       looks the same, so they would clash. */
    /* The encoding is the original name, two underscores, and the
       mangled name of the routine.  Note that the routine name has
       not been mangled yet, but the entity's name has been (if it needs
       mangling). */
    check_assertion(!routine->source_corresp.name_has_been_mangled);
    name_length = strlen(scp->name);
    routine_name_length =
                       mangled_function_name(routine,
                                             /*suppress_param_encoding=*/FALSE,
                                             (char *)NULL);
    mangled_name_length = name_length + 2 + routine_name_length;
    /* Allocate space for the mangled name and build it.  The old name is
       just thrown away. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    (void)strcpy(mangled_name, scp->name);
    store_at = mangled_name + name_length;
    *store_at++ = '_';
    *store_at++ = '_';
    (void)mangled_function_name(routine, /*suppress_param_encoding=*/FALSE,
                                store_at);
    /* Store the final null. */
    mangled_name[mangled_name_length] = '\0';
    scp->name = mangled_name;
    scp->name_has_been_mangled = TRUE;
  }  /* if */
}  /* mangle_promoted_entity_name */

#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */

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
  unnamed_enum_name_seed = 0;
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
